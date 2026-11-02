#ifndef BOOT_APP_H
#define BOOT_APP_H

#include <stdbool.h>

bool boot_app_is_valid(void);
void boot_app_log_status(void);
void boot_jump_to_app(void) __attribute__((noreturn));

/* Program app slot from SD — returns true when flash is ready to run. */
bool boot_app_install_from_file(const char *fat_path);

bool boot_app_install_selected(void);

#endif
