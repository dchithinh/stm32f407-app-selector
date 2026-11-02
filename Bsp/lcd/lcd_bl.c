#include "lcd_bl.h"
#include "lcd_hw.h"
#include "ili9341_regs.h"
#include "font8x16.h"
#include "main.h"
#include <string.h>

SPI_HandleTypeDef hspi2;

static SPI_TypeDef *const lcd_spi = LCD_SPI;

#define CS_L()  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define CS_H()  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define DC_CMD() HAL_GPIO_WritePin(LCD_DCX_PORT, LCD_DCX_PIN, GPIO_PIN_RESET)
#define DC_DAT() HAL_GPIO_WritePin(LCD_DCX_PORT, LCD_DCX_PIN, GPIO_PIN_SET)

static void spi_wait_tx(void)
{
	while (!(lcd_spi->SR & SPI_SR_TXE)) {
	}
	while (lcd_spi->SR & SPI_SR_BSY) {
	}
}

static void spi_write8(uint8_t b)
{
	while (!(lcd_spi->SR & SPI_SR_TXE)) {
	}
	lcd_spi->DR = b;
	spi_wait_tx();
}

static void lcd_cmd(uint8_t cmd)
{
	CS_L();
	DC_CMD();
	spi_write8(cmd);
	DC_DAT();
	CS_H();
}

static void lcd_data(const uint8_t *buf, uint32_t len)
{
	CS_L();
	DC_DAT();
	for (uint32_t i = 0; i < len; i++) {
		spi_write8(buf[i]);
	}
	CS_H();
}

static void lcd_pin_init(void)
{
	GPIO_InitTypeDef gpio = {0};

	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	CS_H();
	HAL_GPIO_WritePin(LCD_RESX_PORT, LCD_RESX_PIN, GPIO_PIN_SET);
	DC_DAT();

	gpio.Pin = LCD_CS_PIN;
	gpio.Mode = GPIO_MODE_OUTPUT_PP;
	gpio.Pull = GPIO_NOPULL;
	gpio.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(LCD_CS_PORT, &gpio);

	gpio.Pin = LCD_RESX_PIN | LCD_DCX_PIN;
	HAL_GPIO_Init(LCD_RESX_PORT, &gpio);

	gpio.Pin = LCD_SCLK_PIN | LCD_MOSI_PIN;
	gpio.Mode = GPIO_MODE_AF_PP;
	gpio.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(LCD_SCLK_PORT, &gpio);
	HAL_GPIO_Init(LCD_MOSI_PORT, &gpio);

	gpio.Pin = LCD_MISO_PIN;
	gpio.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(LCD_MISO_PORT, &gpio);
}

static void lcd_spi_init(void)
{
	hspi2.Instance = LCD_SPI;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;

	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		Error_Handler();
	}
	__HAL_SPI_ENABLE(&hspi2);
}

static void lcd_reset(void)
{
	HAL_GPIO_WritePin(LCD_RESX_PORT, LCD_RESX_PIN, GPIO_PIN_RESET);
	HAL_Delay(50);
	HAL_GPIO_WritePin(LCD_RESX_PORT, LCD_RESX_PIN, GPIO_PIN_SET);
	HAL_Delay(50);
}

static void lcd_panel_config(void)
{
	uint8_t p[15];

	lcd_cmd(ILI9341_SWRESET);

	lcd_cmd(ILI9341_POWERB);
	p[0] = 0x00; p[1] = 0xD9; p[2] = 0x30;
	lcd_data(p, 3);

	lcd_cmd(ILI9341_POWER_SEQ);
	p[0] = 0x64; p[1] = 0x03; p[2] = 0x12; p[3] = 0x81;
	lcd_data(p, 4);

	lcd_cmd(ILI9341_DTCA);
	p[0] = 0x85; p[1] = 0x10; p[2] = 0x7A;
	lcd_data(p, 3);

	lcd_cmd(ILI9341_POWERA);
	p[0] = 0x39; p[1] = 0x2C; p[2] = 0x00; p[3] = 0x34; p[4] = 0x02;
	lcd_data(p, 5);

	lcd_cmd(ILI9341_PRC);
	p[0] = 0x20;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_DTCB);
	p[0] = 0x00; p[1] = 0x00;
	lcd_data(p, 2);

	lcd_cmd(ILI9341_POWER1);
	p[0] = 0x1B;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_POWER2);
	p[0] = 0x12;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_VCOM1);
	p[0] = 0x08; p[1] = 0x26;
	lcd_data(p, 2);

	lcd_cmd(ILI9341_VCOM2);
	p[0] = 0xB7;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_PIXEL_FORMAT);
	p[0] = 0x55;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_FRMCTR1);
	p[0] = 0x00; p[1] = 0x1B;
	lcd_data(p, 2);

	lcd_cmd(ILI9341_DFC);
	p[0] = 0x0A; p[1] = 0xA2;
	lcd_data(p, 2);

	lcd_cmd(ILI9341_3GAMMA_EN);
	p[0] = 0x02;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_GAMMA);
	p[0] = 0x01;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_PGAMMA);
	p[0] = 0x0F; p[1] = 0x1D; p[2] = 0x1A; p[3] = 0x0A; p[4] = 0x0D;
	p[5] = 0x07; p[6] = 0x49; p[7] = 0x66; p[8] = 0x3B; p[9] = 0x07;
	p[10] = 0x11; p[11] = 0x01; p[12] = 0x09; p[13] = 0x05; p[14] = 0x04;
	lcd_data(p, 15);

	lcd_cmd(ILI9341_NGAMMA);
	p[0] = 0x00; p[1] = 0x18; p[2] = 0x1D; p[3] = 0x02; p[4] = 0x0F;
	p[5] = 0x04; p[6] = 0x36; p[7] = 0x13; p[8] = 0x4C; p[9] = 0x07;
	p[10] = 0x13; p[11] = 0x0F; p[12] = 0x2E; p[13] = 0x2F; p[14] = 0x05;
	lcd_data(p, 15);

	lcd_cmd(ILI9341_MAC);
	/* Portrait: MY|MX|RGB — same as stm32f407xx_pomodoro lcd.c */
	p[0] = 0xC0U;
	lcd_data(p, 1);

	lcd_cmd(ILI9341_SLEEP_OUT);
	HAL_Delay(50);
	lcd_cmd(ILI9341_DISPLAY_ON);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint8_t p[4];

	p[0] = (uint8_t)(x0 >> 8); p[1] = (uint8_t)(x0 & 0xFF);
	p[2] = (uint8_t)(x1 >> 8); p[3] = (uint8_t)(x1 & 0xFF);
	lcd_cmd(ILI9341_CASET);
	lcd_data(p, 4);

	p[0] = (uint8_t)(y0 >> 8); p[1] = (uint8_t)(y0 & 0xFF);
	p[2] = (uint8_t)(y1 >> 8); p[3] = (uint8_t)(y1 & 0xFF);
	lcd_cmd(ILI9341_RASET);
	lcd_data(p, 4);

	lcd_cmd(ILI9341_GRAM);
}

void lcd_bl_init(void)
{
	lcd_pin_init();
	lcd_spi_init();
	lcd_reset();
	lcd_panel_config();
}

static void spi_write_rgb565(uint16_t color)
{
	/* ILI9341 expects high byte first on the wire (see Pomodoro color bars). */
	spi_write8((uint8_t)(color >> 8));
	spi_write8((uint8_t)(color & 0xFFU));
}

static void lcd_bl_write_pixels(const uint16_t *pixels, uint32_t count)
{
	/* With MADCTL MX|MY, GRAM writes run opposite to our logical LTR order. */
	CS_L();
	DC_DAT();
	for (uint32_t i = count; i > 0U; i--) {
		spi_write_rgb565(pixels[i - 1U]);
	}
	CS_H();
}

void lcd_bl_fill_screen(uint16_t color)
{
	static uint16_t line[LCD_WIDTH];
	uint32_t x;

	for (x = 0; x < LCD_WIDTH; x++) {
		line[x] = color;
	}
	for (uint32_t y = 0; y < LCD_HEIGHT; y++) {
		lcd_set_window(0, (uint16_t)y, LCD_WIDTH - 1U, (uint16_t)y);
		lcd_bl_write_pixels(line, LCD_WIDTH);
	}
}

void lcd_bl_fill_rect(int x, int y, int w, int h, uint16_t color)
{
	static uint16_t line[LCD_WIDTH];
	int row;
	int col;
	uint32_t run;

	if (w <= 0 || h <= 0) {
		return;
	}
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > (int)LCD_WIDTH) {
		w = (int)LCD_WIDTH - x;
	}
	if (y + h > (int)LCD_HEIGHT) {
		h = (int)LCD_HEIGHT - y;
	}
	if (w <= 0 || h <= 0) {
		return;
	}

	for (col = 0; col < w; col++) {
		line[(uint32_t)col] = color;
	}

	for (row = y; row < y + h; row++) {
		lcd_set_window((uint16_t)x, (uint16_t)row,
		               (uint16_t)(x + w - 1), (uint16_t)row);
		run = (uint32_t)w;
		lcd_bl_write_pixels(line, run);
	}
}

void lcd_bl_text_8x16(int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
	uint16_t rowbuf[8];

	while (*s) {
		unsigned char c = (unsigned char)*s++;
		const uint8_t *glyph;
		int row;
		int col;

		if (c < 32U || c > 126U) {
			c = '?';
		}
		glyph = font8x16_basic[c - 32U];

		for (row = 0; row < 16; row++) {
			uint8_t bits = glyph[row];
			int py = y + row;

			if (py < 0 || py >= (int)LCD_HEIGHT) {
				continue;
			}
			for (col = 0; col < 8; col++) {
				rowbuf[7U - (uint32_t)col] =
					(bits & (0x80U >> (uint32_t)col)) ? fg : bg;
			}
			if (x >= 0 && x + 8 <= (int)LCD_WIDTH) {
				lcd_set_window((uint16_t)x, (uint16_t)py,
				               (uint16_t)(x + 7), (uint16_t)py);
				lcd_bl_write_pixels(rowbuf, 8U);
			}
		}
		x += 9;
	}
}

void lcd_bl_text_8x16_scale(int x, int y, const char *s, uint16_t fg, uint16_t bg,
                            uint8_t scale)
{
	int cx = x;

	if (scale == 0U) {
		scale = 1U;
	}

	while (*s) {
		unsigned char c = (unsigned char)*s++;
		const uint8_t *glyph;
		int row;
		int col;

		if (c < 32U || c > 126U) {
			c = '?';
		}
		glyph = font8x16_basic[c - 32U];

		for (row = 0; row < 16; row++) {
			uint8_t bits = glyph[row];

			for (col = 0; col < 8; col++) {
				uint16_t color = (bits & (0x80U >> (uint32_t)col)) ? fg : bg;
				int px = cx + col * (int)scale;

				if (color == bg) {
					continue;
				}
				lcd_bl_fill_rect(px, y + row * (int)scale,
				                 (int)scale, (int)scale, color);
			}
		}
		cx += (int)(9U * scale);
	}
}
