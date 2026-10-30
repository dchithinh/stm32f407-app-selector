#ifndef BOOT_POLICY_H
#define BOOT_POLICY_H

#include <stdbool.h>
#include "main.h"
#include "boot_layout.h"

/* PA0 USER (B1): board pulldown, pressed = high. BOOT_B1_ACTIVE_LOW=0, GPIO_NOPULL in policy. */
static inline bool boot_b1_is_pressed(void)
{
	GPIO_PinState s = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

#if BOOT_B1_ACTIVE_LOW
	return s == GPIO_PIN_RESET;
#else
	return s == GPIO_PIN_SET;
#endif
}

void boot_policy_init_b1_gpio(void);
void boot_policy_poll_b1_latch(void);
void boot_policy_ensure_b1_gpio(void);

/* Capture B1 hold window — call right after MX_GPIO_Init(), before slow UART. */
void boot_policy_sample_at_reset(void);

/* true = stay in bootloader (user chose update / no valid app). */
bool boot_should_enter_loader(void);
void boot_policy_log(void);

/* false = user pressed B1 during grace window — stay in bootloader. */
bool boot_policy_confirm_autoboot(void);

#endif
