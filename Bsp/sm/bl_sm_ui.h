#ifndef BL_SM_UI_H
#define BL_SM_UI_H

#include "bl_sm_types.h"
#include <stdbool.h>

void bl_sm_ui_display_init(void);
void bl_sm_ui_grace_enter(bl_sm_ctx_t *ctx);
void bl_sm_ui_grace_tick(bl_sm_ctx_t *ctx);
void bl_sm_ui_grace_hide(void);
bool bl_sm_ui_grace_poll_stay(void);

void bl_sm_ui_loader_shell_enter(void);
void bl_sm_ui_catalog_scan(bl_sm_ctx_t *ctx);
void bl_sm_ui_show_browse(void);
void bl_sm_ui_apply_selection(int selected_index);
void bl_sm_ui_redraw_browse(void);
void bl_sm_ui_rescan(bl_sm_ctx_t *ctx);
void bl_sm_ui_show_inform(bool install_failed);

bool bl_sm_ui_run_program(void);

typedef enum {
	BL_UI_POLL_NONE = 0,
	BL_UI_POLL_STAY_IN_LOADER,
	BL_UI_POLL_SELECT_APP,
	BL_UI_POLL_REFRESH_CATALOG,
	BL_UI_POLL_CONFIRM_INSTALL,
	BL_UI_POLL_LAUNCH_APP,
	BL_UI_POLL_REBOOT_BTN,
	BL_UI_POLL_COLD_RESET,
	BL_UI_POLL_DISMISS,
} bl_sm_ui_poll_t;

bl_sm_ui_poll_t bl_sm_ui_poll_browse(int *selected_out);
bl_sm_ui_poll_t bl_sm_ui_poll_inform(void);

const char *bl_sm_ui_selected_path(void);

#endif
