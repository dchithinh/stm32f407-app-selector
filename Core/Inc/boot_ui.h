#ifndef BSP_BOOT_UI_H
#define BSP_BOOT_UI_H

#include <stdbool.h>

void boot_ui_display_init(void);
bool boot_ui_autoboot_grace(void);

void boot_ui_run(void) __attribute__((noreturn));

#endif
