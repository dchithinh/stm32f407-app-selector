#include "boot_app.h"
#include "boot_layout.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

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
		       "BAD (expect odd addr in 0x08020000..0x080FFFFF)");
	if (entry < APP_FLASH_BASE) {
		printf("  hint: entry below app base — Pomodoro may be linked with\r\n");
		printf("        STM32F407VGTX_FLASH.ld (0x08000000) not APP.ld (0x08020000)\r\n");
	}
	if (sp == 0xFFFFFFFFU && reset == 0xFFFFFFFFU) {
		printf("  hint: flash looks erased — program stm32f407xx_pomodoro.elf\r\n");
	}
	printf("  boot_app_is_valid(): %s\r\n", boot_app_is_valid() ? "YES" : "NO");
}

static void boot_teardown(void)
{
	__disable_irq();

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	for (int i = 0; i < 8; i++) {
		NVIC->ICER[i] = 0xFFFFFFFFU;
		NVIC->ICPR[i] = 0xFFFFFFFFU;
	}

	/* Release debug UART so the app can own USART2 cleanly. */
	__HAL_RCC_USART2_FORCE_RESET();
	__HAL_RCC_USART2_RELEASE_RESET();
}

static void boot_jump_with_msp(uint32_t sp, uint32_t entry)
{
	__set_MSP(sp);
	SCB->VTOR = APP_VECTOR_TABLE;
	__set_CONTROL(0U);

#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
	SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
#endif

	__DSB();
	__ISB();

	/* bx to Thumb reset vector (entry LSB must be 1). */
	__asm volatile("bx %0" : : "r"(entry) : "memory");
	__builtin_unreachable();
}

void boot_jump_to_app(void)
{
	uint32_t app_sp = *(volatile uint32_t *)APP_VECTOR_TABLE;
	uint32_t app_entry = *(volatile uint32_t *)(APP_VECTOR_TABLE + 4U);

	if ((app_entry & 1U) == 0U) {
		app_entry |= 1U;
	}

	printf("Jumping to app: SP=0x%08lX PC=0x%08lX VTOR=0x%08lX\r\n",
	       (unsigned long)app_sp,
	       (unsigned long)(app_entry & ~1U),
	       (unsigned long)APP_VECTOR_TABLE);
	HAL_Delay(20);

	boot_teardown();

	/* HSI default — matches app startup before SystemClock_Config(). */
	HAL_RCC_DeInit();

	boot_jump_with_msp(app_sp, app_entry);

	for (;;) {
	}
}

bool boot_app_install_selected(void)
{
	return false;
}
