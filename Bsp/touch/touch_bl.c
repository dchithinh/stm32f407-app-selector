#include "touch_bl.h"
#include "boot_layout.h"
#include "hat_board.h"
#include "lcd_hw.h"
#include "sd_spi.h"
#include "main.h"

/* Same calibration as stm32f407xx_pomodoro (lv_drv_conf.h). */
#define XPT2046_X_MIN       494
#define XPT2046_Y_MIN       327
#define XPT2046_X_MAX       3503
#define XPT2046_Y_MAX       3729
#define XPT2046_X_INV       1
#define XPT2046_Y_INV       1

#define CMD_X_READ  0x90U
#define CMD_Y_READ  0xD0U

static int16_t map_coord(int16_t raw, int16_t min_r, int16_t max_r, int16_t res, int inv)
{
	int32_t v;

	if (raw > min_r) {
		v = raw - min_r;
	} else {
		v = 0;
	}
	v = v * res / (max_r - min_r);
	if (inv) {
		v = res - v;
	}
	if (v < 0) {
		v = 0;
	}
	if (v >= res) {
		v = res - 1;
	}
	return (int16_t)v;
}

void touch_bl_init(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOE_CLK_ENABLE();
	gpio.Pin = BOOT_TOUCH_IRQ_PIN;
	gpio.Mode = GPIO_MODE_INPUT;
	gpio.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(BOOT_TOUCH_IRQ_PORT, &gpio);
}

void touch_bl_probe(void)
{
	sd_spi_set_touch();
	sd_spi_cs_release_all();
	sd_spi_cs_assert(TOUCH_CS_PIN);
	(void)sd_spi_xchg(0x80U);
	(void)sd_spi_xchg(0U);
	(void)sd_spi_xchg(0U);
	sd_spi_cs_release_all();
	(void)sd_spi_xchg(0xFF);
	sd_spi_set_slow();
}

bool touch_bl_read(int16_t *x, int16_t *y, bool *pressed)
{
	uint8_t hi;
	uint8_t lo;
	int16_t rx;
	int16_t ry;

	if (HAL_GPIO_ReadPin(BOOT_TOUCH_IRQ_PORT, BOOT_TOUCH_IRQ_PIN) != GPIO_PIN_RESET) {
		*pressed = false;
		return false;
	}

	sd_spi_set_touch();
	sd_spi_cs_release_all();
	sd_spi_cs_assert(TOUCH_CS_PIN);

	(void)sd_spi_xchg(CMD_X_READ);
	hi = sd_spi_xchg(0U);
	lo = sd_spi_xchg(0U);
	rx = (int16_t)(((int16_t)hi << 8) | lo);
	rx = (int16_t)(rx >> 3);

	(void)sd_spi_xchg(CMD_Y_READ);
	hi = sd_spi_xchg(0U);
	lo = sd_spi_xchg(0U);
	ry = (int16_t)(((int16_t)hi << 8) | lo);
	ry = (int16_t)(ry >> 3);

	sd_spi_cs_release_all();
	(void)sd_spi_xchg(0xFF);
	sd_spi_set_slow();

	*x = map_coord(rx, XPT2046_X_MIN, XPT2046_X_MAX, (int16_t)LCD_WIDTH, XPT2046_X_INV);
	*y = map_coord(ry, XPT2046_Y_MIN, XPT2046_Y_MAX, (int16_t)LCD_HEIGHT, XPT2046_Y_INV);
	*pressed = true;
	return true;
}
