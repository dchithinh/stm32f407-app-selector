#ifndef BOOT_LAYOUT_H
#define BOOT_LAYOUT_H

#include <stdint.h>

#define BOOT_FLASH_BASE           0x08000000U
/* Sectors 0–3 (16 KiB × 4). Linker FLASH length; ~50 KiB used today. */
#define BOOT_FLASH_SIZE           (64U * 1024U)

#define APP_FLASH_BASE            0x08010000U
#define APP_FLASH_SIZE            (960U * 1024U)

#define APP_VECTOR_TABLE          APP_FLASH_BASE

/* PA0 USER: R39 pulldown → idle low; pressed = VDD (active-HIGH). Use GPIO_NOPULL in boot_policy. */
#ifndef BOOT_USR_BTN_ACTIVE_LOW
#define BOOT_USR_BTN_ACTIVE_LOW     0
#endif

/* Set 1 to always stay in bootloader (no autoboot). */
#ifndef BOOT_FORCE_LOADER
#define BOOT_FORCE_LOADER         0
#endif

/* XPT2046 PENIRQ on HAT (active low while touch). */
#define BOOT_TOUCH_IRQ_PORT       GPIOE
#define BOOT_TOUCH_IRQ_PIN        GPIO_PIN_5

#endif
