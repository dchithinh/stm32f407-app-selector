#include "boot_app.h"
#include "boot_layout.h"
#include "ff.h"
#include "lcd_bl.h"
#include "sd_apps.h"
#include "sd_spi.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

#define APP_FLASH_FIRST_SECTOR    4U
#define APP_FLASH_SECTOR_COUNT    8U

/* SD/FatFs: larger reads amortize SPI overhead; keep multiple of 512. */
#define INSTALL_IO_CHUNK          4096U
#define INSTALL_PROGRESS_STEP     INSTALL_IO_CHUNK

static uint8_t s_install_io_buf[INSTALL_IO_CHUNK] __attribute__((aligned(8)));

/* FIL is ~600 bytes; keep off the 2 KiB bootloader stack. */
static FIL s_install_fp;

#define INSTALL_PCT_ERASE         5U
#define INSTALL_PCT_PROGRAM       95U

static boot_app_progress_fn s_progress_fn;
static uint32_t s_install_prog_total;
static uint32_t s_install_prog_done;
static uint8_t s_progress_pct_last = 0xFFU;

void boot_app_set_progress_handler(boot_app_progress_fn fn)
{
	s_progress_fn = fn;
}

static void install_progress_report(uint8_t percent)
{
	if (percent > 100U) {
		percent = 100U;
	}
	if (percent != 100U && percent == s_progress_pct_last) {
		return;
	}
	s_progress_pct_last = percent;
	if (s_progress_fn != NULL) {
		s_progress_fn(percent);
	}
}

static void install_progress_after_program_chunk(void)
{
	uint8_t pct;

	if (s_install_prog_total == 0U) {
		pct = INSTALL_PCT_ERASE;
	} else {
		pct = (uint8_t)(INSTALL_PCT_ERASE +
		                (s_install_prog_done * INSTALL_PCT_PROGRAM) / s_install_prog_total);
	}
	install_progress_report(pct);
}

static bool thumb_reset_in_app_region(uint32_t addr)
{
	if ((addr & 1U) == 0U) {
		return false;
	}
	addr &= ~1U;
	return (addr >= APP_FLASH_BASE) &&
	       (addr < (APP_FLASH_BASE + APP_FLASH_SIZE));
}

static bool reset_vector_in_app_flash(uint32_t reset)
{
	return thumb_reset_in_app_region(reset);
}

static bool stack_pointer_in_sram(uint32_t sp)
{
	return (sp >= 0x20000000U) && (sp <= 0x20020000U);
}

bool boot_app_is_valid(void)
{
	const uint32_t *vt = (const uint32_t *)APP_VECTOR_TABLE;
	uint32_t sp = vt[0];
	uint32_t reset = vt[1];

	if (sp == 0xFFFFFFFFU || reset == 0xFFFFFFFFU) {
		return false;
	}
	if (!stack_pointer_in_sram(sp)) {
		return false;
	}
	if (!reset_vector_in_app_flash(reset)) {
		return false;
	}
	return true;
}

void boot_app_log_status(void)
{
	const uint32_t *vt = (const uint32_t *)APP_VECTOR_TABLE;
	uint32_t sp = vt[0];
	uint32_t reset = vt[1];
	uint32_t entry = reset & ~1U;

	printf("App slot @ 0x%08lX\r\n", (unsigned long)APP_FLASH_BASE);
	printf("  VTOR[0] initial SP : 0x%08lX %s\r\n",
	       (unsigned long)sp,
	       stack_pointer_in_sram(sp) ? "OK" : "BAD (expect 0x2000xxxx..0x20020000)");
	printf("  VTOR[1] reset vector: 0x%08lX (entry 0x%08lX, thumb=%lu) %s\r\n",
	       (unsigned long)reset,
	       (unsigned long)entry,
	       (unsigned long)(reset & 1U),
	       reset_vector_in_app_flash(reset) ? "OK" :
		       "BAD (thumb reset must point into app flash 0x08010000..0x080FFFFF)");
	if (entry < APP_FLASH_BASE) {
		printf("  hint: entry below app base — app may be linked with\r\n");
		printf("        STM32F407VGTX_BL.ld (0x08000000) not APP.ld (0x%08lX)\r\n",
		       (unsigned long)APP_FLASH_BASE);
	} else {
		printf("  first word @ entry: 0x%08lX\r\n",
		       (unsigned long)(*(volatile uint32_t *)entry));
	}
	if (sp == 0xFFFFFFFFU && reset == 0xFFFFFFFFU) {
		printf("  hint: flash looks erased — program stm32f407xx_pomodoro.elf\r\n");
	}
	printf("  boot_app_is_valid(): %s\r\n", boot_app_is_valid() ? "YES" : "NO");
}

static void boot_teardown(void)
{
	uint32_t i;

	/* SD/FatFs and HAL timeouts need SysTick. Do this before killing the tick. */
	sd_apps_unmount();
	sd_spi_cs_release_all();

	/* App SystemClock_Config expects reset-like clocks (HSI), not the BL PLL. */
	(void)HAL_RCC_DeInit();

	__disable_irq();

	SysTick->CTRL = 0U;
	SysTick->LOAD = 0U;
	SysTick->VAL = 0U;
	SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;

	for (i = 0U; i < 8U; i++) {
		NVIC->ICER[i] = 0xFFFFFFFFU;
		NVIC->ICPR[i] = 0xFFFFFFFFU;
	}

	/* Release bus/peripherals the bootloader used so the app HAL can own them. */
	__HAL_RCC_SPI2_FORCE_RESET();
	__HAL_RCC_SPI2_RELEASE_RESET();
	__HAL_RCC_SPI3_FORCE_RESET();
	__HAL_RCC_SPI3_RELEASE_RESET();
	__HAL_RCC_USART2_FORCE_RESET();
	__HAL_RCC_USART2_RELEASE_RESET();
	__HAL_RCC_DMA1_FORCE_RESET();
	__HAL_RCC_DMA1_RELEASE_RESET();

	/* Do not call HAL_DeInit(): it resets all AHB buses including the flash
	 * interface, which can hang the core before the app's SystemInit runs. */
}

static void boot_jump_with_msp(uint32_t sp, uint32_t entry)
{
	__disable_irq();
	__set_CONTROL(0U);

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
	SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
#endif

	SCB->VTOR = APP_VECTOR_TABLE;
	__DSB();
	__ISB();

	/* Do not set MSP in C then keep executing: locals can live on the old
	 * stack. Switch SP and branch in one asm sequence. */
	__asm volatile(
		"msr msp, %0\n"
		"bx  %1\n"
		:
		: "r"(sp), "r"(entry)
		: "memory");

	__builtin_unreachable();
}

void boot_jump_to_app(void)
{
	uint32_t app_sp;
	uint32_t app_entry;

	app_sp = *(volatile uint32_t *)APP_VECTOR_TABLE;
	app_entry = *(volatile uint32_t *)(APP_VECTOR_TABLE + 4U);

	if ((app_entry & 1U) == 0U) {
		app_entry |= 1U;
	}

	printf("Jumping to app: SP=0x%08lX PC=0x%08lX VTOR=0x%08lX\r\n",
	       (unsigned long)app_sp,
	       (unsigned long)(app_entry & ~1U),
	       (unsigned long)APP_VECTOR_TABLE);
	printf("  reset vector thumb=%lu\r\n", (unsigned long)(app_entry & 1U));

	/* Let UART drain before we tear down USART2. */
	HAL_Delay(50);

	boot_teardown();

	/* Re-read vectors after teardown (flash / no stale cache). */
	app_sp = *(volatile uint32_t *)APP_VECTOR_TABLE;
	app_entry = *(volatile uint32_t *)(APP_VECTOR_TABLE + 4U);
	if ((app_entry & 1U) == 0U) {
		app_entry |= 1U;
	}

	boot_jump_with_msp(app_sp, app_entry);

	for (;;) {
	}
}

static bool flash_erase_app_slot(void)
{
	FLASH_EraseInitTypeDef erase = {0};
	uint32_t sector_error = 0U;
	HAL_StatusTypeDef st;

	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
	                       FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	erase.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	erase.Sector = APP_FLASH_FIRST_SECTOR;
	erase.NbSectors = APP_FLASH_SECTOR_COUNT;

	st = HAL_FLASHEx_Erase(&erase, &sector_error);

	if (st != HAL_OK) {
		printf("flash erase failed: st=%d sector_err=%lu hal_err=0x%08lX\r\n",
		       (int)st, (unsigned long)sector_error,
		       (unsigned long)HAL_FLASH_GetError());
		return false;
	}
	return true;
}

static bool flash_program_buffer(uint32_t addr, const uint8_t *data, uint32_t len)
{
	uint32_t i;

	for (i = 0U; i < len; i += 4U) {
		uint32_t word = 0xFFFFFFFFU;

		memcpy(&word, data + i, (len - i >= 4U) ? 4U : (len - i));
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
			printf("flash program fail @ 0x%08lX hal_err=0x%08lX\r\n",
			       (unsigned long)(addr + i),
			       (unsigned long)HAL_FLASH_GetError());
			return false;
		}
	}
	return true;
}

static uint32_t rd_le32(const uint8_t *p)
{
	uint32_t v;

	memcpy(&v, p, 4U);
	return v;
}

static uint16_t rd_le16(const uint8_t *p)
{
	uint16_t v;

	memcpy(&v, p, 2U);
	return v;
}

static bool path_ends_with_ci(const char *path, const char *suffix)
{
	size_t n;
	size_t s;
	size_t i;

	if (path == NULL || suffix == NULL) {
		return false;
	}
	n = strlen(path);
	s = strlen(suffix);
	if (n < s) {
		return false;
	}
	for (i = 0U; i < s; i++) {
		unsigned char a = (unsigned char)path[n - s + i];
		unsigned char b = (unsigned char)suffix[i];

		if (a >= (unsigned char)'A' && a <= (unsigned char)'Z') {
			a = (unsigned char)(a + 32U);
		}
		if (b >= (unsigned char)'A' && b <= (unsigned char)'Z') {
			b = (unsigned char)(b + 32U);
		}
		if (a != b) {
			return false;
		}
	}
	return true;
}

/* FatFs uses direct disk_read when btr is a multiple of 512 at a sector boundary;
 * this SD stack is flaky for that path on the first read. Stay under one sector. */
#define INSTALL_SD_READ_SLICE  128U

static bool install_read_bytes(FIL *fp, uint8_t *buf, UINT want, UINT *out_got)
{
	UINT got = 0U;
	FRESULT fr = FR_OK;

	while (got < want) {
		UINT br = 0U;
		UINT chunk = want - got;

		if (chunk > INSTALL_SD_READ_SLICE) {
			chunk = INSTALL_SD_READ_SLICE;
		}
		fr = f_read(fp, buf + got, chunk, &br);
		if (fr != FR_OK || br == 0U) {
			break;
		}
		got += br;
	}
	if (out_got != NULL) {
		*out_got = got;
	}
	return (got == want && fr == FR_OK);
}

static bool install_read_prefix(FIL *fp, uint32_t file_size, UINT *prefix_len_out)
{
	UINT want;
	UINT got;

	if (file_size == 0U) {
		return false;
	}
	want = 512U;
	if (file_size < want) {
		want = (UINT)file_size;
	}
	if (!install_read_bytes(fp, s_install_io_buf, want, &got)) {
		return false;
	}
	*prefix_len_out = got;
	return true;
}

static bool flash_program_file_range(FIL *fp, uint32_t file_off, uint32_t flash_addr, uint32_t nbytes)
{
	uint32_t done = 0U;
	uint32_t next_progress = INSTALL_PROGRESS_STEP;

	if ((flash_addr < APP_FLASH_BASE) || (nbytes > APP_FLASH_SIZE) ||
	    ((flash_addr - APP_FLASH_BASE) > (APP_FLASH_SIZE - nbytes))) {
		printf("install: dest 0x%08lX+%lu outside app slot\r\n",
		       (unsigned long)flash_addr, (unsigned long)nbytes);
		return false;
	}

	if (f_lseek(fp, file_off) != FR_OK) {
		printf("install: seek %lu failed\r\n", (unsigned long)file_off);
		return false;
	}

	printf("install: programming %lu bytes\r\n", (unsigned long)nbytes);

	while (done < nbytes) {
		UINT to_read = (UINT)INSTALL_IO_CHUNK;
		UINT br = 0U;
		FRESULT fr;

		if ((nbytes - done) < to_read) {
			to_read = (UINT)(nbytes - done);
		}

		fr = f_read(fp, s_install_io_buf, to_read, &br);
		if (fr != FR_OK || br == 0U) {
			printf("install: read fail @ file+%lu (fr=%d br=%u)\r\n",
			       (unsigned long)(file_off + done), (int)fr, (unsigned)br);
			return false;
		}

		if ((br & 3U) != 0U) {
			while ((br & 3U) != 0U && br < INSTALL_IO_CHUNK) {
				s_install_io_buf[br++] = 0xFFU;
			}
		}

		if (!flash_program_buffer(flash_addr + done, s_install_io_buf, br)) {
			return false;
		}

		done += br;
		s_install_prog_done += br;
		if (done >= next_progress || done == nbytes) {
			printf("install: %lu / %lu @ 0x%08lX\r\n",
			       (unsigned long)done, (unsigned long)nbytes,
			       (unsigned long)flash_addr);
			install_progress_after_program_chunk();
			while (next_progress <= done) {
				next_progress += INSTALL_PROGRESS_STEP;
			}
		}
	}
	return true;
}

static bool install_raw_bin(FIL *fp, uint32_t file_size)
{
	if (file_size == 0U || file_size > APP_FLASH_SIZE) {
		printf("install: bad .bin size %lu\r\n", (unsigned long)file_size);
		return false;
	}
	printf("install: raw .bin %lu bytes -> 0x%08lX\r\n",
	       (unsigned long)file_size, (unsigned long)APP_FLASH_BASE);
	return flash_program_file_range(fp, 0U, APP_FLASH_BASE, file_size);
}

#define ELF_SEG_MAX  8U

typedef struct {
	uint32_t file_off;
	uint32_t dest;
	uint32_t len;
} elf_seg_t;

static elf_seg_t s_elf_segs[ELF_SEG_MAX];
static uint8_t s_elf_nsegs;

/* Prefer flash LMA (p_paddr). GNU ld .data is VMA=SRAM, LMA=flash. */
static uint32_t elf_ph_load_addr(const uint8_t *ph)
{
	uint32_t p_vaddr = rd_le32(ph + 8);
	uint32_t p_paddr = rd_le32(ph + 12);

	if ((p_paddr >= APP_FLASH_BASE) &&
	    (p_paddr < (APP_FLASH_BASE + APP_FLASH_SIZE))) {
		return p_paddr;
	}
	if ((p_vaddr >= APP_FLASH_BASE) &&
	    (p_vaddr < (APP_FLASH_BASE + APP_FLASH_SIZE))) {
		return p_vaddr;
	}
	if (p_paddr != 0U) {
		return p_paddr;
	}
	return p_vaddr;
}

static bool elf_addr_in_ram_or_ccm(uint32_t addr)
{
	if (addr >= 0x20000000U && addr < 0x20020000U) {
		return true;
	}
	if (addr >= 0x10000000U && addr < 0x10010000U) {
		return true;
	}
	return false;
}

/* True if this PT_LOAD belongs in the app flash slot (LMA/VMA in flash map). */
static bool elf_ph_flash_dest(const uint8_t *ph, uint32_t *load_addr_out,
                              uint32_t *filesz_out, bool log_skips)
{
	uint32_t p_type = rd_le32(ph + 0);
	uint32_t p_filesz = rd_le32(ph + 16);
	uint32_t load_addr;

	if (p_type != 1U || p_filesz == 0U) {
		return false;
	}

	load_addr = elf_ph_load_addr(ph);
	if (elf_addr_in_ram_or_ccm(load_addr)) {
		if (log_skips) {
			printf("install: skip RAM/CCM segment @ 0x%08lX (%lu B)\r\n",
			       (unsigned long)load_addr, (unsigned long)p_filesz);
		}
		return false;
	}
	if (p_filesz > APP_FLASH_SIZE) {
		if (log_skips) {
			printf("install: skip segment (%lu B) larger than app slot\r\n",
			       (unsigned long)p_filesz);
		}
		return false;
	}
	if (load_addr < APP_FLASH_BASE ||
	    (load_addr - APP_FLASH_BASE) > (APP_FLASH_SIZE - p_filesz)) {
		if (log_skips) {
			printf("install: skip segment @ 0x%08lX (%lu B) outside app slot\r\n",
			       (unsigned long)load_addr, (unsigned long)p_filesz);
		}
		return false;
	}

	*load_addr_out = load_addr;
	*filesz_out = p_filesz;
	return true;
}

static bool prefix_is_elf32_arm_exec(const uint8_t *eh, UINT n)
{
	uint16_t e_type;

	if (n < 52U) {
		return false;
	}
	if (eh[0] != 0x7FU || eh[1] != 'E' || eh[2] != 'L' || eh[3] != 'F') {
		return false;
	}
	if (eh[4] != 1U || eh[5] != 1U) {
		printf("install: need ELF32 little-endian\r\n");
		return false;
	}
	e_type = rd_le16(eh + 16);
	if (e_type != 2U && e_type != 3U) {
		printf("install: ELF not executable (e_type=%u)\r\n", (unsigned)e_type);
		return false;
	}
	if (rd_le16(eh + 18) != 40U) {
		printf("install: ELF not EM_ARM\r\n");
		return false;
	}
	return true;
}

static bool elf_add_flash_seg(const uint8_t *ph)
{
	uint32_t dest;
	uint32_t filesz;
	uint32_t poff;

	if (!elf_ph_flash_dest(ph, &dest, &filesz, true)) {
		return true;
	}
	if (s_elf_nsegs >= ELF_SEG_MAX) {
		printf("install: too many FLASH PT_LOAD\r\n");
		return false;
	}
	poff = rd_le32(ph + 4);
	s_elf_segs[s_elf_nsegs].file_off = poff;
	s_elf_segs[s_elf_nsegs].dest = dest;
	s_elf_segs[s_elf_nsegs].len = filesz;
	s_elf_nsegs++;
	s_install_prog_total += filesz;
	printf("install: FLASH PT_LOAD file+0x%lX -> 0x%08lX (%lu B)\r\n",
	       (unsigned long)poff, (unsigned long)dest, (unsigned long)filesz);
	return true;
}

static bool elf_collect_from_phdr_table(const uint8_t *tbl, uint16_t phnum,
                                        uint16_t phentsize)
{
	uint16_t i;

	s_elf_nsegs = 0U;
	s_install_prog_total = 0U;

	for (i = 0U; i < phnum; i++) {
		if (!elf_add_flash_seg(tbl + ((uint32_t)i * phentsize))) {
			return false;
		}
	}
	return true;
}

static bool elf_collect_flash_segs(FIL *fp, const uint8_t *prefix, UINT prefix_len)
{
	uint32_t phoff = rd_le32(prefix + 28);
	uint16_t phentsize = rd_le16(prefix + 42);
	uint16_t phnum = rd_le16(prefix + 44);
	uint32_t phbytes;
	UINT br;

	printf("install: ELF32 ARM phoff=%lu phentsize=%u phnum=%u\r\n",
	       (unsigned long)phoff, (unsigned)phentsize, (unsigned)phnum);

	if (phentsize < 32U || phnum == 0U || phnum > 32U) {
		printf("install: bad ELF phdrs (%u x %u)\r\n",
		       (unsigned)phnum, (unsigned)phentsize);
		return false;
	}

	phbytes = (uint32_t)phnum * (uint32_t)phentsize;
	if (phoff >= 52U && (phoff + phbytes) <= (uint32_t)prefix_len) {
		return elf_collect_from_phdr_table(prefix + phoff, phnum, phentsize);
	}

	if (phbytes > INSTALL_IO_CHUNK) {
		printf("install: PHDR table too large (%lu)\r\n", (unsigned long)phbytes);
		return false;
	}

	printf("install: reading PHDRs from file+%lu (%lu B)\r\n",
	       (unsigned long)phoff, (unsigned long)phbytes);
	if (f_lseek(fp, phoff) != FR_OK ||
	    f_read(fp, s_install_io_buf, (UINT)phbytes, &br) != FR_OK ||
	    br != (UINT)phbytes) {
		printf("install: PHDR table read fail\r\n");
		return false;
	}
	return elf_collect_from_phdr_table(s_install_io_buf, phnum, phentsize);
}

static bool install_elf_cached(FIL *fp)
{
	uint8_t i;

	for (i = 0U; i < s_elf_nsegs; i++) {
		if (!flash_program_file_range(fp, s_elf_segs[i].file_off,
		                              s_elf_segs[i].dest, s_elf_segs[i].len)) {
			return false;
		}
	}
	return s_elf_nsegs > 0U;
}

bool boot_app_install_from_file(const char *fat_path)
{
	FIL *fp = &s_install_fp;
	FRESULT fr;
	uint32_t file_size;
	UINT prefix_len;
	bool ok;
	bool elf;

	if (fat_path == NULL || fat_path[0] == '\0') {
		return false;
	}

	fr = f_open(fp, fat_path, FA_READ);
	if (fr != FR_OK) {
		printf("install: open %s failed (%d)\r\n", fat_path, (int)fr);
		return false;
	}

	file_size = (uint32_t)f_size(fp);
	if (file_size == 0U) {
		printf("install: empty file\r\n");
		(void)f_close(fp);
		return false;
	}

	/* Only .elf needs a header parse. .bin goes straight to sequential
	 * programming (the path that already worked on this SD stack). */
	if (!path_ends_with_ci(fat_path, ".elf")) {
		elf = false;
		prefix_len = 0U;
	} else if (!install_read_prefix(fp, file_size, &prefix_len)) {
		printf("install: prefix read fail (size=%lu)\r\n",
		       (unsigned long)file_size);
		(void)f_close(fp);
		return false;
	} else {
		elf = (prefix_len >= 4U &&
		       s_install_io_buf[0] == 0x7FU &&
		       s_install_io_buf[1] == 'E' &&
		       s_install_io_buf[2] == 'L' &&
		       s_install_io_buf[3] == 'F');
		if (!elf) {
			printf("install: %s named .elf but not ELF magic — treating as BIN\r\n",
			       fat_path);
			(void)f_lseek(fp, 0);
		}
	}

	printf("install: %s (%lu bytes, %s)\r\n",
	       fat_path, (unsigned long)file_size, elf ? "ELF" : "BIN");

	lcd_bl_force_cpu_pixels(true);
	s_install_prog_done = 0U;
	s_progress_pct_last = 0xFFU;
	s_elf_nsegs = 0U;
	if (elf) {
		if (!prefix_is_elf32_arm_exec(s_install_io_buf, prefix_len) ||
		    !elf_collect_flash_segs(fp, s_install_io_buf, prefix_len) ||
		    s_elf_nsegs == 0U) {
			printf("install: no flash PT_LOAD for app slot (check APP.ld @ 0x%08lX)\r\n",
			       (unsigned long)APP_FLASH_BASE);
			lcd_bl_force_cpu_pixels(false);
			(void)f_close(fp);
			return false;
		}
		printf("install: flash payload %lu bytes in %u segment(s)\r\n",
		       (unsigned long)s_install_prog_total, (unsigned)s_elf_nsegs);
	} else {
		s_install_prog_total = file_size;
	}

	printf("install: erasing app slot...\r\n");
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
	                       FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	if (!flash_erase_app_slot()) {
		HAL_FLASH_Lock();
		lcd_bl_force_cpu_pixels(false);
		(void)f_close(fp);
		return false;
	}
	/* Flash erase can take seconds; dummy clocks so the SD SPI slave is ready. */
	sd_spi_bus_idle_clocks(16U);
	install_progress_report(INSTALL_PCT_ERASE);

	ok = elf ? install_elf_cached(fp) : install_raw_bin(fp, file_size);

	HAL_FLASH_Lock();
	(void)f_close(fp);
	lcd_bl_force_cpu_pixels(false);

	if (!ok) {
		return false;
	}

	if (!boot_app_is_valid()) {
		printf("install: image invalid after flash (wrong link addr? need APP @ 0x%08lX)\r\n",
		       (unsigned long)APP_FLASH_BASE);
		return false;
	}

	__DSB();
	__ISB();

	install_progress_report(100U);
	printf("install: OK\r\n");
	return true;
}

void boot_app_reboot_after_install(void)
{
	printf("Installed: rebooting...\r\n");
	HAL_Delay(30);
	__disable_irq();
	HAL_NVIC_SystemReset();
	for (;;) {
	}
}

bool boot_app_install_selected(void)
{
	return false;
}
