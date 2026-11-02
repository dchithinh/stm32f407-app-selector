#include "boot_loader.h"
#include "boot_ui.h"
#include "debug_uart.h"
#include <stdio.h>

static void boot_loader_print_help(void)
{
	printf("Bootloader (LCD menu active):\r\n");
	printf("  touch = select app / Install & Run\r\n");
	printf("  a = run flashed app (if valid)\r\n");
	printf("  r = rescan SD apps\r\n");
	printf("  s = SD mount test (UART)\r\n");
	printf("  ? = help\r\n");
}

void boot_loader_run(void)
{
	printf("Bootloader: starting LCD UI...\r\n");
	boot_loader_print_help();
	boot_ui_run();
}
