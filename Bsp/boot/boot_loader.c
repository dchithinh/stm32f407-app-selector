#include "boot_loader.h"
#include "boot_app.h"
#include "boot_layout.h"
#include "boot_policy.h"
#include "debug_uart.h"
#include "main.h"
#include <stdio.h>

#define BOOT_BLINK_MS         400U

static void status_blink(void)
{
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	HAL_Delay(BOOT_BLINK_MS);
}

static void boot_loader_print_help(void)
{
	printf("Bootloader commands (UART):\r\n");
	printf("  a = run app (if valid)\r\n");
	printf("  ? = this help\r\n");
}

static void boot_loader_try_jump_app(void)
{
	if (!boot_app_is_valid()) {
		printf("No valid app at 0x%08lX.\r\n", (unsigned long)APP_FLASH_BASE);
		return;
	}
	printf("Jumping to app...\r\n");
	HAL_Delay(50);
	boot_jump_to_app();
}

void boot_loader_run(void)
{
	printf("Bootloader: waiting for app selection (SD/UI later).\r\n");
	boot_loader_print_help();

	for (;;) {
		uint8_t c = 0;

		status_blink();

		if (debug_uart_getchar_nonblock(&c)) {
			if (c == 'a' || c == 'A') {
				boot_loader_try_jump_app();
			} else if (c == '?') {
				boot_loader_print_help();
			} else {
				printf("Unknown key 0x%02X — send ? for help\r\n", (unsigned)c);
			}
		}
	}
}
