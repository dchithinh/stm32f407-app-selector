#include "boot_app.h"
#include "boot_layout.h"
#include "ff.h"
#include "sd_apps.h"
#include "sd_spi.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

#define APP_FLASH_FIRST_SECTOR    4U
#define APP_FLASH_SECTOR_COUNT    8U

/* SD/FatFs: larger reads amortize SPI overhead; keep multiple of 512. */
#define INSTALL_IO_CHUNK          4096U
#define INSTALL_PROGRESS_STEP     (32U * 1024U)

static uint8_t s_install_io_buf[INSTALL_IO_CHUNK] __attribute__((aligned(8)));

static bool thumb_reset_in_app_region(uint32_t addr)
{
	if ((addr & 1U) == 0U) {
		return false;
	}
	addr &= ~1U;
	return (addr >= APP_FLASH_BASE) &&
	       (addr < (APP_FLASH_BASE + APP_FLASH_SIZE));
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
	if (!thumb_reset_in_app_region(reset)) {
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
	       thumb_reset_in_app_region(reset) ? "OK" :
		       "BAD (expect odd addr in app slot range)");
	if (entry < APP_FLASH_BASE) {
		printf("  hint: entry below app base — app may be linked with\r\n");
		printf("        STM32F407VGTX_BL.ld (0x08000000) not APP.ld (0x%08lX)\r\n",
		       (unsigned long)APP_FLASH_BASE);
	}
	if (sp == 0xFFFFFFFFU && reset == 0xFFFFFFFFU) {
		printf("  hint: flash looks erased — program stm32f407xx_pomodoro.elf\r\n");
	}
	printf("  boot_app_is_valid(): %s\r\n", boot_app_is_valid() ? "YES" : "NO");
}

static void boot_teardown(void)
{
	uint32_t i;

	__disable_irq();

	SysTick->CTRL = 0U;
	SysTick->LOAD = 0U;
	SysTick->VAL = 0U;

	for (i = 0U; i < 8U; i++) {
		NVIC->ICER[i] = 0xFFFFFFFFU;
		NVIC->ICPR[i] = 0xFFFFFFFFU;
	}

	SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;

	sd_apps_unmount();
	sd_spi_cs_release_all();

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

typedef void (*app_entry_fn)(void);

static void boot_jump_with_msp(uint32_t sp, uint32_t entry)
{
	app_entry_fn go;

	__set_MSP(sp);
	SCB->VTOR = APP_VECTOR_TABLE;
	__set_CONTROL(0U);

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
	SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
#endif

	__DSB();
	__ISB();

	/* App startup (Reset_Handler) expects to run with interrupts unmasked. */
	__enable_irq();

	go = (app_entry_fn)entry;
	go();
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

bool boot_app_install_from_file(const char *fat_path)
{
	FIL fp;
	FRESULT fr;
	UINT br;
	uint32_t addr = APP_FLASH_BASE;
	uint32_t total = 0U;
	uint32_t file_size;
	uint32_t next_progress = INSTALL_PROGRESS_STEP;

	if (fat_path == NULL || fat_path[0] == '\0') {
		return false;
	}

	fr = f_open(&fp, fat_path, FA_READ);
	if (fr != FR_OK) {
		printf("install: open %s failed (%d)\r\n", fat_path, (int)fr);
		return false;
	}

	file_size = (uint32_t)f_size(&fp);
	if (file_size == 0U || file_size > APP_FLASH_SIZE) {
		printf("install: bad size %lu\r\n", (unsigned long)file_size);
		(void)f_close(&fp);
		return false;
	}

	printf("install: erasing app slot...\r\n");
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
	                       FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	if (!flash_erase_app_slot()) {
		HAL_FLASH_Lock();
		(void)f_close(&fp);
		return false;
	}

	while (total < file_size) {
		UINT to_read = (UINT)INSTALL_IO_CHUNK;

		if ((file_size - total) < to_read) {
			to_read = (UINT)(file_size - total);
		}

		fr = f_read(&fp, s_install_io_buf, to_read, &br);
		if (fr != FR_OK || br == 0U) {
			printf("install: read fail @ %lu\r\n", (unsigned long)total);
			HAL_FLASH_Lock();
			(void)f_close(&fp);
			return false;
		}

		if ((br & 3U) != 0U) {
			while ((br & 3U) != 0U && br < INSTALL_IO_CHUNK) {
				s_install_io_buf[br++] = 0xFFU;
			}
		}

		if (!flash_program_buffer(addr, s_install_io_buf, br)) {
			HAL_FLASH_Lock();
			(void)f_close(&fp);
			return false;
		}

		addr += br;
		total += br;

		if (total >= next_progress || total == file_size) {
			printf("install: %lu / %lu\r\n", (unsigned long)total, (unsigned long)file_size);
			while (next_progress <= total) {
				next_progress += INSTALL_PROGRESS_STEP;
			}
		}
	}

	HAL_FLASH_Lock();
	(void)f_close(&fp);

	if (!boot_app_is_valid()) {
		printf("install: image invalid after flash\r\n");
		return false;
	}

	__DSB();
	__ISB();

	printf("install: OK\r\n");
	return true;
}

bool boot_app_install_selected(void)
{
	return false;
}
