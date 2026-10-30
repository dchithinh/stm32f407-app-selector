#ifndef BOOT_LAYOUT_H
#define BOOT_LAYOUT_H

#include <stdint.h>

#define BOOT_FLASH_BASE           0x08000000U
#define BOOT_FLASH_SIZE           (128U * 1024U)

#define APP_FLASH_BASE            0x08020000U
#define APP_FLASH_SIZE            (896U * 1024U)

#define APP_VECTOR_TABLE          APP_FLASH_BASE

/* PA0 USER: R39 pulldown → idle low; pressed = VDD (active-HIGH). Use GPIO_NOPULL in boot_policy. */
#ifndef BOOT_B1_ACTIVE_LOW
#define BOOT_B1_ACTIVE_LOW        0
#endif

/* Set 1 to always stay in bootloader (no autoboot). */
#ifndef BOOT_FORCE_LOADER
#define BOOT_FORCE_LOADER         0
#endif

/* XPT2046 PENIRQ on HAT (active low while touch). */
#define BOOT_TOUCH_IRQ_PORT       GPIOE
#define BOOT_TOUCH_IRQ_PIN        GPIO_PIN_5

#endif
