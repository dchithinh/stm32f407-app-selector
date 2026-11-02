#include "boot_ui.h"
#include "boot_app.h"
#include "boot_loader.h"
#include "boot_policy.h"
#include "debug_uart.h"
#include "lcd_bl.h"
#include "lcd_hw.h"
#include "sd_apps.h"
#include "sd_spi.h"
#include "touch_bl.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/*
 * Dark navy theme: #181d43 page, #206DE0 accent bar / buttons.
 */
#define COL_SCR_BG      LCD_COLOR(24, 29, 67)    /* #181d43 */
#define COL_HEADER      LCD_COLOR(14, 17, 42)    /* #0e112a — darker header strip */
#define COL_RULE        LCD_COLOR(32, 109, 224)  /* #206DE0 */
#define COL_ROW         LCD_COLOR(36, 44, 92)    /* list card */
#define COL_ROW_SEL     LCD_COLOR(32, 109, 224)  /* #206DE0 selected row */
#define COL_ACCENT      LCD_COLOR(32, 109, 224)  /* primary button */
#define COL_BAR_BG      LCD_COLOR(28, 34, 78)
#define COL_BAR_FILL    LCD_COLOR(32, 109, 224)
#define COL_TEXT        LCD_COLOR(232, 235, 245) /* on dark surfaces */
#define COL_TEXT_ON_DARK LCD_COLOR(255, 255, 255)
#define COL_TEXT_DIM    LCD_COLOR(120, 130, 168)
#define COL_SUBTEXT     LCD_COLOR(168, 176, 210)
#define COL_WARN        LCD_COLOR(255, 140, 64)
/* Pressed: same blue family, slightly darker (not white flash). */
#define COL_BTN_PRESS_BG LCD_COLOR(22, 78, 176)
#define COL_BTN_PRESS_FG COL_TEXT_ON_DARK

#define UI_HEADER_RULE  2

#define UI_FONT_H           16
#define UI_CHAR_ADV         9
#define UI_TITLE_H          38
#define UI_ROW_H            44
#define UI_ROW_Y0           40
#define UI_VISIBLE_ROWS     5U
#define UI_BTN_X            0
#define UI_BTN_Y            268
#define UI_BTN_W            LCD_WIDTH
#define UI_BTN_H            40
#define UI_BODY_Y           80
#define UI_BAR_X            16
#define UI_BAR_Y            124
#define UI_BAR_W            (LCD_WIDTH - 32)
#define UI_BAR_H            20
#define UI_PCT_Y            (UI_BAR_Y - UI_FONT_H - 6)

#define UI_AUTO_CARD_X          0
#define UI_AUTO_CARD_Y          UI_TITLE_H
#define UI_AUTO_CARD_W          LCD_WIDTH
#define UI_AUTO_CARD_H          (UI_BTN_Y - UI_AUTO_CARD_Y)
#define UI_AUTO_SUB_Y           (UI_AUTO_CARD_Y + 24)
#define UI_AUTO_COUNT_SCALE     3U
#define UI_AUTO_COUNT_TOP       (UI_AUTO_SUB_Y + UI_FONT_H + 8)
#define UI_AUTO_COUNT_Y_BIAS    (-14)

#define UI_SCROLL_X         168

static bool s_display_ready;

typedef enum {
	UI_SCREEN_LIST,
	UI_SCREEN_MSG,
	UI_SCREEN_INSTALL,
} ui_screen_t;

static ui_screen_t g_screen;
static sd_app_list_t g_apps;
static int g_selected;   /* -1 until user taps a row */
static int g_scroll;
static char g_msg[64];
static char g_title[32];
static char g_install_name[36];
static bool g_touch_down;
static int16_t g_last_x;
static int16_t g_last_y;
static uint32_t g_ignore_touch_until;
static bool g_action_btn_pressed;

static int ui_text_y_center(int top, int h)
{
	return top + (h - UI_FONT_H) / 2;
}

static int ui_text_width(const char *s)
{
	return (int)(strlen(s) * (size_t)UI_CHAR_ADV);
}

static void ui_text(int x, int y, const char *s, uint16_t fg, uint16_t bg)
{
	lcd_bl_text_8x16(x, y, s, fg, bg);
}

static void ui_text_centered(int y, const char *s, uint16_t fg, uint16_t bg)
{
	int tw = ui_text_width(s);
	int tx = ((int)LCD_WIDTH - tw) / 2;

	lcd_bl_text_8x16(tx, y, s, fg, bg);
}

static void ui_text_in_bar(int x, const char *s, uint16_t fg, uint16_t bg)
{
	lcd_bl_text_8x16(x, ui_text_y_center(0, UI_TITLE_H), s, fg, bg);
}

static void ui_text_in_bar_centered(const char *s, uint16_t fg, uint16_t bg)
{
	ui_text_centered(ui_text_y_center(0, UI_TITLE_H), s, fg, bg);
}

static bool ui_hit_install_btn(int16_t x, int16_t y);

static void ui_button_label_ex(const char *label, uint16_t fg, uint16_t bg)
{
	int tw = ui_text_width(label);
	int tx = UI_BTN_X + (((int)UI_BTN_W - tw) / 2);
	int ty = ui_text_y_center(UI_BTN_Y, UI_BTN_H);

	lcd_bl_text_8x16(tx, ty, label, fg, bg);
}

static void ui_draw_action_button(const char *label, bool pressed)
{
	uint16_t bg = pressed ? COL_BTN_PRESS_BG : COL_ACCENT;
	uint16_t fg = pressed ? COL_BTN_PRESS_FG : COL_TEXT_ON_DARK;

	lcd_bl_fill_rect(UI_BTN_X, UI_BTN_Y, UI_BTN_W, UI_BTN_H, bg);
	ui_button_label_ex(label, fg, bg);
}

static void ui_button_label(const char *label, uint16_t fg, uint16_t bg)
{
	ui_button_label_ex(label, fg, bg);
}

static const char *ui_screen_button_label(void)
{
	if (g_screen == UI_SCREEN_LIST) {
		return "Install & Run";
	}
	if (g_screen == UI_SCREEN_MSG) {
		return "Back";
	}
	return NULL;
}

static void ui_action_button_press_visual(int16_t x, int16_t y, bool finger_down)
{
	const char *label = ui_screen_button_label();
	bool on_btn;
	bool pressed;

	if (label == NULL) {
		return;
	}

	on_btn = ui_hit_install_btn(x, y);
	pressed = finger_down && on_btn;
	if (pressed == g_action_btn_pressed) {
		return;
	}

	g_action_btn_pressed = pressed;
	ui_draw_action_button(label, pressed);
}

static int ui_autoboot_count_h(void)
{
	return (int)(UI_FONT_H * UI_AUTO_COUNT_SCALE);
}

static int ui_autoboot_count_y(void)
{
	int region_h = UI_BTN_Y - UI_AUTO_COUNT_TOP;

	return UI_AUTO_COUNT_TOP + (region_h - ui_autoboot_count_h()) / 2
	       + UI_AUTO_COUNT_Y_BIAS;
}

static int ui_autoboot_count_text_width(const char *s)
{
	return (int)(strlen(s) * 9U * UI_AUTO_COUNT_SCALE);
}

static void ui_format_app_line(char *line, size_t len, int idx)
{
	const char *name = g_apps.items[idx].name;
	int max_name = ((int)LCD_WIDTH - 24) / UI_CHAR_ADV - 2;

	if (max_name < 4) {
		max_name = 4;
	}
	snprintf(line, len, "%c %.*s",
	         idx == g_selected ? '>' : ' ',
	         max_name, name);
}

static void ui_draw_list_row_slot(uint8_t slot)
{
	int idx = g_scroll + (int)slot;
	int y = UI_ROW_Y0 + (int)slot * UI_ROW_H;
	uint16_t bg;
	uint16_t fg;
	char line[40];

	if (idx >= (int)g_apps.count) {
		lcd_bl_fill_rect(4, y, LCD_WIDTH - 8, UI_ROW_H - 4, COL_SCR_BG);
		return;
	}

	bg = (idx == g_selected) ? COL_ROW_SEL : COL_ROW;
	fg = (idx == g_selected) ? COL_TEXT_ON_DARK : COL_TEXT;
	lcd_bl_fill_rect(4, y, LCD_WIDTH - 8, UI_ROW_H - 4, bg);
	ui_format_app_line(line, sizeof(line), idx);
	lcd_bl_text_8x16(12, ui_text_y_center(y, UI_ROW_H - 4), line, fg, bg);
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

	lcd_bl_fill_screen(COL_SCR_BG);
	lcd_bl_fill_rect(0, 0, LCD_WIDTH, UI_TITLE_H, COL_HEADER);
	lcd_bl_fill_rect(0, UI_TITLE_H - UI_HEADER_RULE, LCD_WIDTH, UI_HEADER_RULE, COL_RULE);
	ui_text_in_bar(8, "Application Lists", COL_TEXT_ON_DARK, COL_HEADER);

	for (i = 0; i < UI_VISIBLE_ROWS; i++) {
		if (g_scroll + (int)i >= (int)g_apps.count) {
			break;
		}
		ui_draw_list_row_slot(i);
	}

	if (g_apps.count > UI_VISIBLE_ROWS) {
		ui_text(UI_SCROLL_X, UI_ROW_Y0 + 4, "^", COL_TEXT_DIM, COL_SCR_BG);
		ui_text(UI_SCROLL_X, UI_ROW_Y0 + UI_ROW_H * 2 + 4, "v",
		        COL_TEXT_DIM, COL_SCR_BG);
	}

	g_action_btn_pressed = false;
	ui_draw_action_button("Install & Run", false);
}

static void ui_draw_message(const char *title, const char *body)
{
	lcd_bl_fill_screen(COL_SCR_BG);
	lcd_bl_fill_rect(0, 0, LCD_WIDTH, UI_TITLE_H, COL_HEADER);
	lcd_bl_fill_rect(0, UI_TITLE_H - UI_HEADER_RULE, LCD_WIDTH, UI_HEADER_RULE, COL_RULE);
	ui_text_in_bar(8, title, COL_TEXT_ON_DARK, COL_HEADER);
	ui_text(8, UI_BODY_Y, body, COL_TEXT, COL_SCR_BG);
	g_action_btn_pressed = false;
	ui_draw_action_button("Back", false);
}

static void ui_update_install_bar(uint8_t percent);

static void ui_draw_install_screen(void)
{
	lcd_bl_fill_screen(COL_SCR_BG);
	lcd_bl_fill_rect(0, 0, LCD_WIDTH, UI_TITLE_H, COL_HEADER);
	lcd_bl_fill_rect(0, UI_TITLE_H - UI_HEADER_RULE, LCD_WIDTH, UI_HEADER_RULE, COL_RULE);
	ui_text_in_bar(8, "Installing...", COL_TEXT_ON_DARK, COL_HEADER);
	ui_text(8, UI_BODY_Y, g_install_name, COL_TEXT, COL_SCR_BG);
	ui_update_install_bar(0U);
}

static void ui_update_install_bar(uint8_t percent)
{
	char pct_line[8];
	uint16_t fill_w;
	int tw;
	int tx;

	if (percent > 100U) {
		percent = 100U;
	}

	snprintf(pct_line, sizeof(pct_line), "%u%%", (unsigned)percent);
	tw = ui_text_width(pct_line);
	tx = (int)UI_BAR_X + ((int)UI_BAR_W - tw) / 2;

	lcd_bl_fill_rect((int)UI_BAR_X, UI_PCT_Y - 2, (int)UI_BAR_W,
	                 UI_FONT_H + 4, COL_SCR_BG);
	lcd_bl_fill_rect((int)UI_BAR_X, (int)UI_BAR_Y, (int)UI_BAR_W,
	                 (int)UI_BAR_H, COL_BAR_BG);
	fill_w = (uint16_t)((UI_BAR_W * (uint32_t)percent) / 100U);
	if (fill_w > 0U) {
		lcd_bl_fill_rect((int)UI_BAR_X, (int)UI_BAR_Y, (int)fill_w,
		                 (int)UI_BAR_H, COL_BAR_FILL);
	}

	ui_text(tx, UI_PCT_Y, pct_line, COL_TEXT, COL_SCR_BG);
}

static void ui_on_install_progress(uint8_t percent)
{
	if (g_screen != UI_SCREEN_INSTALL) {
		return;
	}
	ui_update_install_bar(percent);
}

static bool point_in_rect(int16_t x, int16_t y, int rx, int ry, int rw, int rh)
{
	return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

static bool ui_autoboot_stay_btn_hit(int16_t x, int16_t y)
{
	return point_in_rect(x, y, UI_BTN_X, UI_BTN_Y, (int)UI_BTN_W, UI_BTN_H);
}

static void ui_draw_autoboot_stay_button(bool pressed)
{
	uint16_t bg = pressed ? COL_BTN_PRESS_BG : COL_ACCENT;
	uint16_t fg = pressed ? COL_BTN_PRESS_FG : COL_TEXT_ON_DARK;

	lcd_bl_fill_rect(UI_BTN_X, UI_BTN_Y, UI_BTN_W, UI_BTN_H, bg);
	ui_button_label_ex("BOOT MENU", fg, bg);
}

static void ui_draw_autoboot_frame(void)
{
	lcd_bl_fill_screen(COL_SCR_BG);
	lcd_bl_fill_rect(0, 0, LCD_WIDTH, UI_TITLE_H, COL_HEADER);
	lcd_bl_fill_rect(0, UI_TITLE_H - UI_HEADER_RULE, LCD_WIDTH, UI_HEADER_RULE, COL_RULE);
	ui_text_in_bar_centered("Autoboot", COL_TEXT_ON_DARK, COL_HEADER);

	lcd_bl_fill_rect(UI_AUTO_CARD_X, UI_AUTO_CARD_Y, UI_AUTO_CARD_W,
	                 UI_AUTO_CARD_H, COL_ROW);
	ui_text_centered(UI_AUTO_SUB_Y, "Autoboot to app",
	                 COL_SUBTEXT, COL_ROW);

	ui_draw_autoboot_stay_button(false);
}

static void ui_update_autoboot_line(uint32_t sec)
{
	char line[16];
	int tw;
	int tx;
	int cy;

	snprintf(line, sizeof(line), "%lu", (unsigned long)sec);
	tw = ui_autoboot_count_text_width(line);
	tx = ((int)LCD_WIDTH - tw) / 2;
	cy = ui_autoboot_count_y();

	lcd_bl_fill_rect(0, UI_AUTO_COUNT_TOP, (int)LCD_WIDTH,
	                 UI_BTN_Y - UI_AUTO_COUNT_TOP, COL_ROW);
	lcd_bl_text_8x16_scale(tx, cy, line, COL_TEXT, COL_ROW, UI_AUTO_COUNT_SCALE);
}

void boot_ui_display_init(void)
{
	if (s_display_ready) {
		return;
	}
	lcd_bl_init();
	sd_spi_init();
	touch_bl_init();
	touch_bl_probe();
	s_display_ready = true;
}

static bool ui_autoboot_poll_cancel(bool *touch_down, int16_t *last_x, int16_t *last_y,
                                    bool *btn_pressed)
{
	int16_t x;
	int16_t y;
	bool pressed;

	if (boot_policy_user_requests_bootloader()) {
		printf("\r\nStay in bootloader (user btn or j).\r\n");
		return true;
	}

	if (touch_bl_read(&x, &y, &pressed)) {
		bool on_btn = ui_autoboot_stay_btn_hit(x, y);
		bool show_pressed = on_btn;

		*touch_down = true;
		*last_x = x;
		*last_y = y;
		if (show_pressed != *btn_pressed) {
			*btn_pressed = show_pressed;
			ui_draw_autoboot_stay_button(show_pressed);
		}
	} else if (*touch_down) {
		bool fire = ui_autoboot_stay_btn_hit(*last_x, *last_y);

		*touch_down = false;
		if (*btn_pressed) {
			*btn_pressed = false;
			ui_draw_autoboot_stay_button(false);
		}
		if (fire) {
			printf("\r\nStay in bootloader (touch).\r\n");
			return true;
		}
	}

	return false;
}

bool boot_ui_autoboot_grace(void)
{
	uint32_t total_sec = BOOT_AUTOBOOT_GRACE_MS / 1000U;
	bool touch_down = false;
	int16_t last_x = 0;
	int16_t last_y = 0;
	bool btn_pressed = false;
	uint32_t sec;

	ui_draw_autoboot_frame();

	for (sec = total_sec; sec >= 1U; sec--) {
		uint32_t hold_until;

		ui_update_autoboot_line(sec);
		printf("\rAutoboot to app: %lu    ", (unsigned long)sec);
		fflush(stdout);

		/* One full wall-clock second per displayed digit (after draw). */
		hold_until = HAL_GetTick() + 1000U;
		while (HAL_GetTick() < hold_until) {
			if (ui_autoboot_poll_cancel(&touch_down, &last_x, &last_y,
			                            &btn_pressed)) {
				return false;
			}
			HAL_Delay(1);
		}
	}

	printf("\r\n");
	return true;
}

static void ui_handle_message_tap(int16_t x, int16_t y)
{
	if (point_in_rect(x, y, UI_BTN_X, UI_BTN_Y, (int)UI_BTN_W, UI_BTN_H)) {
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
	return point_in_rect(x, y, UI_BTN_X, UI_BTN_Y, (int)UI_BTN_W, UI_BTN_H);
}

static void ui_handle_list_tap(int16_t x, int16_t y)
{
	int i;

	if (point_in_rect(x, y, UI_SCROLL_X, UI_ROW_Y0 + 4, 64, UI_ROW_H * 2)) {
		if (g_scroll > 0) {
			g_scroll--;
			ui_draw_list();
		}
		return;
	}
	if (point_in_rect(x, y, UI_SCROLL_X, UI_ROW_Y0 + UI_ROW_H * 2, 64, UI_ROW_H * 2)) {
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
		strncpy(g_install_name, g_apps.items[g_selected].name, sizeof(g_install_name) - 1U);
		g_install_name[sizeof(g_install_name) - 1U] = '\0';
		g_screen = UI_SCREEN_INSTALL;
		boot_app_set_progress_handler(ui_on_install_progress);
		ui_draw_install_screen();
		printf("UI: install %s\r\n", g_apps.items[g_selected].path);
		if (boot_app_install_from_file(g_apps.items[g_selected].path)) {
			boot_app_set_progress_handler(NULL);
			boot_app_reboot_after_install();
		}
		boot_app_set_progress_handler(NULL);
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
			if (g_action_btn_pressed) {
				g_action_btn_pressed = false;
				if (ui_screen_button_label() != NULL) {
					ui_draw_action_button(ui_screen_button_label(), false);
				}
			}
			if (HAL_GetTick() >= g_ignore_touch_until) {
				if (g_screen == UI_SCREEN_LIST) {
					ui_handle_list_tap(g_last_x, g_last_y);
				} else if (g_screen == UI_SCREEN_MSG) {
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
	ui_action_button_press_visual(x, y, true);
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
	boot_ui_display_init();

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
		strncpy(g_title, "Application Lists", sizeof(g_title));
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
