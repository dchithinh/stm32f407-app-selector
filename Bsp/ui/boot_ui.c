#include "boot_ui.h"
#include "boot_app.h"
#include "boot_loader.h"
#include "debug_uart.h"
#include "lcd_bl.h"
#include "lcd_hw.h"
#include "sd_apps.h"
#include "sd_spi.h"
#include "touch_bl.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define COL_BG          LCD_COLOR(16, 24, 48)
#define COL_TITLE       LCD_COLOR(32, 48, 80)
#define COL_ROW         LCD_COLOR(24, 36, 64)
#define COL_ROW_SEL     LCD_COLOR(48, 72, 120)
#define COL_BTN         LCD_COLOR(0, 120, 40)
#define COL_TEXT        LCD_COLOR(255, 255, 255)
#define COL_WARN        LCD_COLOR(255, 200, 0)

#define UI_TITLE_H      32
#define UI_ROW_H        40
#define UI_ROW_Y0       36
#define UI_VISIBLE_ROWS 6U
#define UI_BTN_Y        280
#define UI_BTN_H        36

typedef enum {
	UI_SCREEN_LIST,
	UI_SCREEN_MSG,
} ui_screen_t;

static ui_screen_t g_screen;
static sd_app_list_t g_apps;
static int g_selected;   /* -1 until user taps a row */
static int g_scroll;
static char g_msg[64];
static char g_title[32];
static bool g_touch_down;
static int16_t g_last_x;
static int16_t g_last_y;
static uint32_t g_ignore_touch_until;

static void ui_draw_list_row_slot(uint8_t slot)
{
	int idx = g_scroll + (int)slot;
	int y = UI_ROW_Y0 + (int)slot * UI_ROW_H;
	uint16_t bg;
	char line[40];

	if (idx >= (int)g_apps.count) {
		lcd_bl_fill_rect(4, y, LCD_WIDTH - 8, UI_ROW_H - 4, COL_BG);
		return;
	}

	bg = (idx == g_selected) ? COL_ROW_SEL : COL_ROW;
	lcd_bl_fill_rect(4, y, LCD_WIDTH - 8, UI_ROW_H - 4, bg);
	snprintf(line, sizeof(line), "%c %s",
	         idx == g_selected ? '>' : ' ',
	         g_apps.items[idx].name);
	lcd_bl_text(12, y + 14, line, COL_TEXT, bg);
}

static void ui_redraw_app_row(int app_idx)
{
	int slot;

	if (app_idx < g_scroll) {
		return;
	}
	slot = app_idx - g_scroll;
	if (slot >= (int)UI_VISIBLE_ROWS) {
		return;
	}
	ui_draw_list_row_slot((uint8_t)slot);
}

static void ui_set_selected(int app_idx)
{
	int prev = g_selected;

	if (prev == app_idx) {
		return;
	}
	g_selected = app_idx;
	if (prev >= 0) {
		ui_redraw_app_row(prev);
	}
	ui_redraw_app_row(app_idx);
}

static void ui_draw_list(void)
{
	uint8_t i;

	lcd_bl_fill_screen(COL_BG);
	lcd_bl_fill_rect(0, 0, LCD_WIDTH, UI_TITLE_H, COL_TITLE);
	lcd_bl_text(8, 12, "Bootloader - SD apps", COL_TEXT, COL_TITLE);

	for (i = 0; i < UI_VISIBLE_ROWS; i++) {
		if (g_scroll + (int)i >= (int)g_apps.count) {
			break;
		}
		ui_draw_list_row_slot(i);
	}

	if (g_apps.count > UI_VISIBLE_ROWS) {
		lcd_bl_text(196, UI_ROW_Y0 + 8, "scroll", COL_WARN, COL_BG);
	}

	lcd_bl_fill_rect(8, UI_BTN_Y, LCD_WIDTH - 16, UI_BTN_H, COL_BTN);
	lcd_bl_text(48, UI_BTN_Y + 14, "Install & Run", COL_TEXT, COL_BTN);
}

static void ui_draw_message(const char *title, const char *body)
{
	lcd_bl_fill_screen(COL_BG);
	lcd_bl_fill_rect(0, 0, LCD_WIDTH, UI_TITLE_H, COL_TITLE);
	lcd_bl_text(8, 12, title, COL_TEXT, COL_TITLE);
	lcd_bl_text(8, 80, body, COL_TEXT, COL_BG);
	lcd_bl_fill_rect(8, UI_BTN_Y, LCD_WIDTH - 16, UI_BTN_H, COL_BTN);
	lcd_bl_text(72, UI_BTN_Y + 14, "Back", COL_TEXT, COL_BTN);
}

static bool point_in_rect(int16_t x, int16_t y, int rx, int ry, int rw, int rh)
{
	return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

static void ui_handle_message_tap(int16_t x, int16_t y)
{
	if (point_in_rect(x, y, 8, UI_BTN_Y, LCD_WIDTH - 16, UI_BTN_H)) {
		g_screen = UI_SCREEN_LIST;
		if (g_apps.count > 0U) {
			ui_draw_list();
		} else {
			ui_draw_message(g_title, g_msg);
		}
	}
}

static bool ui_hit_install_btn(int16_t x, int16_t y)
{
	return point_in_rect(x, y, 8, UI_BTN_Y, LCD_WIDTH - 16, UI_BTN_H);
}

static void ui_handle_list_tap(int16_t x, int16_t y)
{
	int i;

	if (point_in_rect(x, y, 196, UI_ROW_Y0 + 10, 40, 40)) {
		if (g_scroll > 0) {
			g_scroll--;
			ui_draw_list();
		}
		return;
	}
	if (point_in_rect(x, y, 196, UI_ROW_Y0 + 60, 40, 40)) {
		if (g_scroll + (int)UI_VISIBLE_ROWS < (int)g_apps.count) {
			g_scroll++;
			ui_draw_list();
		}
		return;
	}

	for (i = 0; i < (int)UI_VISIBLE_ROWS; i++) {
		int idx = g_scroll + i;
		int ry = UI_ROW_Y0 + i * UI_ROW_H;

		if (idx >= (int)g_apps.count) {
			break;
		}
		if (point_in_rect(x, y, 4, ry, LCD_WIDTH - 8, UI_ROW_H - 4)) {
			ui_set_selected(idx);
			printf("UI: selected %s (%d,%d)\r\n",
			       g_apps.items[g_selected].name, (int)x, (int)y);
			return;
		}
	}

	if (ui_hit_install_btn(x, y)) {
		if (g_selected < 0 || g_apps.count == 0U) {
			printf("UI: tap install but no selection (%d,%d)\r\n", (int)x, (int)y);
			return;
		}
		ui_draw_message("Working", "Installing...");
		printf("UI: install %s\r\n", g_apps.items[g_selected].path);
		if (boot_app_install_from_file(g_apps.items[g_selected].path)) {
			boot_app_reboot_after_install();
		}
		snprintf(g_msg, sizeof(g_msg), "Install failed");
		strncpy(g_title, "Error", sizeof(g_title));
		g_title[sizeof(g_title) - 1U] = '\0';
		g_screen = UI_SCREEN_MSG;
		ui_draw_message(g_title, g_msg);
		return;
	}

	printf("UI: tap (%d,%d) no hit\r\n", (int)x, (int)y);
}

static void ui_poll_touch(void)
{
	int16_t x;
	int16_t y;
	bool pressed;

	if (!touch_bl_read(&x, &y, &pressed)) {
		if (g_touch_down) {
			if (HAL_GetTick() >= g_ignore_touch_until) {
				if (g_screen == UI_SCREEN_LIST) {
					ui_handle_list_tap(g_last_x, g_last_y);
				} else {
					ui_handle_message_tap(g_last_x, g_last_y);
				}
			}
		}
		g_touch_down = false;
		return;
	}

	g_touch_down = true;
	g_last_x = x;
	g_last_y = y;
}

static void ui_poll_uart(void)
{
	uint8_t c;

	if (!debug_uart_getchar_nonblock(&c)) {
		return;
	}
	if (c == 'a' || c == 'A') {
		if (boot_app_is_valid()) {
			boot_jump_to_app();
		}
	} else if (c == 'r' || c == 'R') {
		(void)sd_apps_scan(&g_apps);
		g_selected = -1;
		g_scroll = 0;
		g_screen = UI_SCREEN_LIST;
		if (g_apps.count > 0U) {
			ui_draw_list();
			g_ignore_touch_until = HAL_GetTick() + 250U;
		}
	} else if (c == 'b' || c == 'B') {
		boot_mcu_reboot();
	}
}

void boot_ui_run(void)
{
	lcd_bl_init();
	sd_spi_init();
	touch_bl_init();
	touch_bl_probe();

	g_screen = UI_SCREEN_LIST;
	g_selected = -1;
	g_scroll = 0;
	g_ignore_touch_until = HAL_GetTick() + 250U;

	if (!sd_apps_mount()) {
		g_screen = UI_SCREEN_MSG;
		strncpy(g_title, "SD error", sizeof(g_title));
		strncpy(g_msg, "Mount failed", sizeof(g_msg));
		g_title[sizeof(g_title) - 1U] = '\0';
		g_msg[sizeof(g_msg) - 1U] = '\0';
		ui_draw_message(g_title, g_msg);
	} else if (!sd_apps_scan(&g_apps)) {
		g_screen = UI_SCREEN_MSG;
		strncpy(g_title, "SD apps", sizeof(g_title));
		strncpy(g_msg, g_apps.error, sizeof(g_msg));
		g_title[sizeof(g_title) - 1U] = '\0';
		g_msg[sizeof(g_msg) - 1U] = '\0';
		ui_draw_message(g_title, g_msg);
	} else {
		printf("UI: %u app(s) on SD\r\n", (unsigned)g_apps.count);
		ui_draw_list();
		g_ignore_touch_until = HAL_GetTick() + 250U;
	}

	for (;;) {
		ui_poll_uart();
		ui_poll_touch();
		HAL_Delay(10);
	}
}
