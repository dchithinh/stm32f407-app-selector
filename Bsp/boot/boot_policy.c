#include "boot_policy.h"
#include "boot_app.h"
#include "boot_layout.h"
#include "debug_uart.h"
#include "main.h"
#include <stdio.h>

#define BOOT_HOLD_MS           400U
#define BOOT_B1_SAMPLE_MS      5U
#define BOOT_AUTOBOOT_GRACE_MS 5000U

static bool s_b1_held_at_reset;
static bool s_b1_early_latch;
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
	return boot_b1_is_pressed() || boot_uart_got_stay_in_bl();
}

static void boot_b1_configure_input(void)
{
	__HAL_RCC_GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef g = {0};
	g.Pin = B1_Pin;
	g.Mode = GPIO_MODE_INPUT;
	/* Board R39 pulls PA0 low; pressed ties VDD — do not enable MCU pull-up. */
	g.Pull = GPIO_NOPULL;
	g.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(B1_GPIO_Port, &g);
}

void boot_policy_init_b1_gpio(void)
{
	boot_b1_configure_input();
	if (boot_b1_is_pressed()) {
		s_b1_early_latch = true;
	}
}

void boot_policy_ensure_b1_gpio(void)
{
	/* Cube MX_GPIO_Init sets PA0 to EXTI/event mode — restore plain input. */
	boot_b1_configure_input();
	boot_policy_poll_b1_latch();
}

void boot_policy_poll_b1_latch(void)
{
	if (boot_b1_is_pressed()) {
		s_b1_early_latch = true;
	}
}

static bool user_button_held_at_boot(void)
{
	uint32_t t0 = HAL_GetTick();

	if (s_b1_early_latch) {
		return true;
	}

	while ((HAL_GetTick() - t0) < BOOT_HOLD_MS) {
		if (boot_b1_is_pressed()) {
			return true;
		}
		HAL_Delay(BOOT_B1_SAMPLE_MS);
	}
	return false;
}

void boot_policy_sample_at_reset(void)
{
	if (s_sampled) {
		return;
	}
	s_b1_held_at_reset = s_b1_early_latch || user_button_held_at_boot();
	s_enter_loader = s_b1_held_at_reset || !boot_app_is_valid();
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
	printf("Boot policy: B1 at reset=%s\r\n",
	       s_b1_held_at_reset ? "yes" : "no");
	if (s_b1_held_at_reset) {
		printf("  -> enter bootloader (B1 at reset)\r\n");
	} else if (!boot_app_is_valid()) {
		printf("  -> enter bootloader (app slot invalid)\r\n");
	} else {
		printf("  -> autoboot to app (stay in BL: send j or press B1 in grace)\r\n");
	}
}

bool boot_policy_confirm_autoboot(void)
{
	uint32_t until = HAL_GetTick() + BOOT_AUTOBOOT_GRACE_MS;
	uint32_t last_sec_shown = 0xFFFFFFFFU;

	while (HAL_GetTick() < until) {
		uint32_t remain_ms = until - HAL_GetTick();
		uint32_t sec = (remain_ms + 999U) / 1000U;

		if (sec != last_sec_shown) {
			last_sec_shown = sec;
			printf("\rPress j or B1 to stop autoboot: %2lu    ",
			       (unsigned long)sec);
			fflush(stdout);
		}

		if (boot_user_wants_loader_now()) {
			printf("\r\nStay in bootloader (B1 or j).\r\n");
			return false;
		}
		HAL_Delay(5);
	}

	printf("\r\n");
	return true;
}
