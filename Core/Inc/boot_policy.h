#ifndef BOOT_POLICY_H
#define BOOT_POLICY_H

#include <stdbool.h>
#include "main.h"
#include "boot_layout.h"

#define BOOT_AUTOBOOT_GRACE_MS    5000U

static inline bool boot_usr_btn_pressed(void)
{
	GPIO_PinState s = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

#if BOOT_USR_BTN_ACTIVE_LOW
	return s == GPIO_PIN_RESET;
#else
	return s == GPIO_PIN_SET;
#endif
}

void boot_policy_usr_btn_init(void);
void boot_policy_usr_btn_poll(void);

void boot_policy_sample_at_reset(void);

bool boot_should_enter_loader(void);
void boot_policy_log(void);
bool boot_policy_user_requests_bootloader(void);
bool boot_policy_confirm_autoboot(void);

#endif
