#include "boot_policy.h"
#include "boot_app.h"
#include "boot_layout.h"
#include "debug_uart.h"
#include "main.h"
#include <stdio.h>

#define BOOT_HOLD_MS              400U
#define BOOT_USR_BTN_SAMPLE_MS    5U

static bool s_usr_btn_at_reset;
static bool s_usr_btn_latch;
static bool s_enter_loader;
static bool s_sampled;

static bool boot_uart_got_stay_in_bl(void)
{
	uint8_t c;

	if (!debug_uart_getchar_nonblock(&c)) {
		return false;
	}
	return c == 'j' || c == 'J';
}

static bool boot_user_wants_loader_now(void)
{
	return boot_usr_btn_pressed() || boot_uart_got_stay_in_bl();
}

bool boot_policy_user_requests_bootloader(void)
{
	return boot_user_wants_loader_now();
}

static void usr_btn_gpio_init(void)
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef g = {0};

	g.Pin = B1_Pin;
	g.Mode = GPIO_MODE_INPUT;
	g.Pull = GPIO_NOPULL;
	g.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(B1_GPIO_Port, &g);
}

void boot_policy_usr_btn_init(void)
{
	usr_btn_gpio_init();
	boot_policy_usr_btn_poll();
}

void boot_policy_usr_btn_poll(void)
{
	if (boot_usr_btn_pressed()) {
		s_usr_btn_latch = true;
	}
}

static bool usr_btn_held_at_boot(void)
{
	uint32_t t0 = HAL_GetTick();

	if (s_usr_btn_latch) {
		return true;
	}

	while ((HAL_GetTick() - t0) < BOOT_HOLD_MS) {
		if (boot_usr_btn_pressed()) {
			return true;
		}
		HAL_Delay(BOOT_USR_BTN_SAMPLE_MS);
	}
	return false;
}

void boot_policy_sample_at_reset(void)
{
	if (s_sampled) {
		return;
	}
	s_usr_btn_at_reset = s_usr_btn_latch || usr_btn_held_at_boot();
	s_enter_loader = s_usr_btn_at_reset || !boot_app_is_valid();
	s_sampled = true;
}

bool boot_should_enter_loader(void)
{
	if (!s_sampled) {
		boot_policy_sample_at_reset();
	}
	return s_enter_loader;
}

void boot_policy_log(void)
{
	printf("Boot policy: user btn at reset=%s\r\n",
	       s_usr_btn_at_reset ? "yes" : "no");
	if (s_usr_btn_at_reset) {
		printf("  -> enter bootloader (user btn at reset)\r\n");
	} else if (!boot_app_is_valid()) {
		printf("  -> enter bootloader (app slot invalid)\r\n");
	} else {
		printf("  -> autoboot to app (stay in BL: send j or press user btn in grace)\r\n");
	}
}
