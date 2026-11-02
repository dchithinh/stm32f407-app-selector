#ifndef BOOT_LOADER_H
#define BOOT_LOADER_H

void boot_loader_run(void) __attribute__((noreturn));
void boot_mcu_reboot(void) __attribute__((noreturn));

#endif
