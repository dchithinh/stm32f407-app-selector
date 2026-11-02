#ifndef BOOT_APP_H
#define BOOT_APP_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*boot_app_progress_fn)(uint8_t percent);

void boot_app_set_progress_handler(boot_app_progress_fn fn);

bool boot_app_is_valid(void);
void boot_app_log_status(void);
void boot_jump_to_app(void) __attribute__((noreturn));

void boot_app_reboot_after_install(void) __attribute__((noreturn));

/* Program app slot from SD — returns true when flash is ready to run. */
bool boot_app_install_from_file(const char *fat_path);

bool boot_app_install_selected(void);

#endif
