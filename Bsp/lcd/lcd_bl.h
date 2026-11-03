#ifndef BSP_LCD_BL_H
#define BSP_LCD_BL_H

#include <stdint.h>

void lcd_bl_init(void);
void lcd_bl_dma_irq_handler(void);
void lcd_bl_spi_irq_handler(void);

void lcd_bl_fill_screen(uint16_t color);
void lcd_bl_fill_rect(int x, int y, int w, int h, uint16_t color);
void lcd_bl_text_8x16(int x, int y, const char *s, uint16_t fg, uint16_t bg);
void lcd_bl_text_8x16_scale(int x, int y, const char *s, uint16_t fg, uint16_t bg,
                            uint8_t scale);

#define LCD_COLOR(r, g, b) \
	((uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | (((b) & 0xF8U) >> 3)))

#endif
