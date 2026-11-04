#include "bl_sm.h"
#include "bl_sm_guards.h"
#include "bl_sm_actions.h"
#include "bl_sm_ui.h"
#include "debug_uart.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

typedef struct {
	bl_state_t from;
	bl_event_t event;
	bool (*guard)(const bl_sm_ctx_t *ctx);
	void (*action)(bl_sm_ctx_t *ctx);
	bl_state_t to;
} bl_sm_transition_t;

static bl_sm_ctx_t s_ctx;

static void on_entry(bl_state_t st, bl_sm_ctx_t *ctx);
static void on_exit(bl_state_t st, bl_sm_ctx_t *ctx);

static const bl_sm_transition_t s_transitions[] = {
	{ BL_STATE_NONE, BL_EVENT_POWER_ON, bl_guard_true, bl_action_start_assess,
	  BL_STATE_ASSESS_IMAGE },

	{ BL_STATE_ASSESS_IMAGE, BL_EVENT_ASSESS_DONE, bl_guard_app_image_valid_and_autoboot,
	  bl_action_init_grace_ui, BL_STATE_AUTOBOOT_GRACE },
	{ BL_STATE_ASSESS_IMAGE, BL_EVENT_ASSESS_DONE, bl_guard_enter_loader,
	  bl_action_init_loader_shell, BL_STATE_AWAIT_CATALOG },

	{ BL_STATE_AUTOBOOT_GRACE, BL_EVENT_GRACE_ELAPSED, bl_guard_true,
	  bl_action_handoff_to_app, BL_STATE_RUNNING_APP },
	{ BL_STATE_AUTOBOOT_GRACE, BL_EVENT_STAY_IN_LOADER, bl_guard_true,
	  bl_action_init_loader_shell, BL_STATE_AWAIT_CATALOG },

	{ BL_STATE_AWAIT_CATALOG, BL_EVENT_SCAN_DONE, bl_guard_catalog_ready,
	  bl_action_show_list, BL_STATE_BROWSE },
	{ BL_STATE_AWAIT_CATALOG, BL_EVENT_SCAN_DONE, bl_guard_catalog_unavailable,
	  bl_action_show_error, BL_STATE_INFORM },

	{ BL_STATE_BROWSE, BL_EVENT_SELECT_APP, bl_guard_true, bl_action_update_selection,
	  BL_STATE_BROWSE },
	{ BL_STATE_BROWSE, BL_EVENT_REFRESH_CATALOG, bl_guard_true, bl_action_rescan_sd,
	  BL_STATE_BROWSE },
	{ BL_STATE_BROWSE, BL_EVENT_CONFIRM_INSTALL, bl_guard_app_selected,
	  bl_action_begin_program, BL_STATE_PROGRAM },
	{ BL_STATE_BROWSE, BL_EVENT_LAUNCH_FLASHED_APP, bl_guard_app_image_valid,
	  bl_action_handoff_to_app, BL_STATE_RUNNING_APP },
	{ BL_STATE_BROWSE, BL_EVENT_REBOOT, bl_guard_app_image_valid,
	  bl_action_handoff_to_app, BL_STATE_RUNNING_APP },
	{ BL_STATE_BROWSE, BL_EVENT_REBOOT, bl_guard_not_app_image_valid,
	  bl_action_nvic_reset, BL_STATE_COLD_RESET },
	{ BL_STATE_BROWSE, BL_EVENT_COLD_RESET, bl_guard_true, bl_action_nvic_reset,
	  BL_STATE_COLD_RESET },

	{ BL_STATE_PROGRAM, BL_EVENT_PROGRAM_DONE, bl_guard_program_success,
	  bl_action_nvic_reset, BL_STATE_COLD_RESET },
	{ BL_STATE_PROGRAM, BL_EVENT_PROGRAM_DONE, bl_guard_program_fail,
	  bl_action_show_install_failed, BL_STATE_INFORM },

	{ BL_STATE_INFORM, BL_EVENT_DISMISS, bl_guard_catalog_ready, bl_action_show_list,
	  BL_STATE_BROWSE },
	{ BL_STATE_INFORM, BL_EVENT_DISMISS, bl_guard_catalog_unavailable, bl_action_keep_message,
	  BL_STATE_INFORM },
};

const bl_sm_ctx_t *bl_sm_context(void)
{
	return &s_ctx;
}

void bl_sm_init(void)
{
	s_ctx.state = BL_STATE_NONE;
	s_ctx.catalog_ready = false;
	s_ctx.catalog_unavailable = false;
	s_ctx.program_success = false;
	s_ctx.assess_dispatched = false;
	s_ctx.scan_done_posted = false;
	s_ctx.program_done_posted = false;
	s_ctx.selected_index = -1;
	s_ctx.grace_sec_remaining = 0U;
	s_ctx.grace_second_deadline_ms = 0U;
	s_ctx.grace_counting = false;
}

static void on_exit(bl_state_t st, bl_sm_ctx_t *ctx)
{
	switch (st) {
	case BL_STATE_ASSESS_IMAGE:
		bl_action_latch_boot_decision(ctx);
		break;
	case BL_STATE_AUTOBOOT_GRACE:
		bl_action_hide_countdown(ctx);
		break;
	case BL_STATE_AWAIT_CATALOG:
		bl_action_classify_catalog(ctx);
		break;
	default:
		break;
	}
}

static void on_entry(bl_state_t st, bl_sm_ctx_t *ctx)
{
	switch (st) {
	case BL_STATE_ASSESS_IMAGE:
		bl_action_validate_vector_table(ctx);
		break;
	case BL_STATE_AUTOBOOT_GRACE:
		bl_action_show_countdown(ctx);
		break;
	case BL_STATE_AWAIT_CATALOG:
		s_ctx.scan_done_posted = false;
		bl_action_mount_and_scan_apps(ctx);
		break;
	case BL_STATE_BROWSE:
		bl_action_render_app_list(ctx);
		break;
	case BL_STATE_INFORM:
		bl_action_render_message(ctx);
		break;
	case BL_STATE_PROGRAM:
		s_ctx.program_done_posted = false;
		break;
	default:
		break;
	}
}

bool bl_sm_dispatch(bl_event_t event)
{
	size_t i;
	const bl_sm_transition_t *hit = NULL;

	for (i = 0U; i < (sizeof(s_transitions) / sizeof(s_transitions[0])); i++) {
		const bl_sm_transition_t *t = &s_transitions[i];

		if (t->from != s_ctx.state || t->event != event) {
			continue;
		}
		if (t->guard != NULL && !t->guard(&s_ctx)) {
			continue;
		}
		hit = t;
		break;
	}

	if (hit == NULL) {
		return false;
	}

	printf("SM: state %d event %d -> %d\r\n", (int)s_ctx.state, (int)event,
	       (int)hit->to);

	if (hit->from != hit->to) {
		on_exit(s_ctx.state, &s_ctx);
	}
	if (hit->action != NULL) {
		hit->action(&s_ctx);
	}
	s_ctx.state = hit->to;
	if (hit->from != hit->to) {
		on_entry(s_ctx.state, &s_ctx);
	}

	return true;
}

static void bl_sm_run_do(void)
{
	switch (s_ctx.state) {
	case BL_STATE_ASSESS_IMAGE:
		if (!s_ctx.assess_dispatched) {
			s_ctx.assess_dispatched = true;
			(void)bl_sm_dispatch(BL_EVENT_ASSESS_DONE);
		}
		break;
	case BL_STATE_AUTOBOOT_GRACE:
		if (bl_sm_ui_grace_poll_stay()) {
			(void)bl_sm_dispatch(BL_EVENT_STAY_IN_LOADER);
			break;
		}
		if (s_ctx.grace_counting &&
		    (int32_t)(HAL_GetTick() - s_ctx.grace_second_deadline_ms) >= 0) {
			if (s_ctx.grace_sec_remaining > 0U) {
				s_ctx.grace_sec_remaining--;
				bl_action_tick_one_second(&s_ctx);
				s_ctx.grace_second_deadline_ms = HAL_GetTick() + 1000U;
			}
			if (s_ctx.grace_sec_remaining == 0U) {
				bl_action_notify_grace_elapsed(&s_ctx);
				(void)bl_sm_dispatch(BL_EVENT_GRACE_ELAPSED);
			}
		}
		break;
	case BL_STATE_AWAIT_CATALOG:
		if (!s_ctx.scan_done_posted) {
			s_ctx.scan_done_posted = true;
			(void)bl_sm_dispatch(BL_EVENT_SCAN_DONE);
		}
		break;
	case BL_STATE_BROWSE: {
		bl_sm_ui_poll_t ev;
		int sel = s_ctx.selected_index;

		ev = bl_sm_ui_poll_browse(&sel);
		if (ev == BL_UI_POLL_SELECT_APP) {
			s_ctx.selected_index = sel;
		} else if (sel != s_ctx.selected_index) {
			s_ctx.selected_index = sel;
		}
		if (ev == BL_UI_POLL_SELECT_APP) {
			(void)bl_sm_dispatch(BL_EVENT_SELECT_APP);
		} else if (ev == BL_UI_POLL_REFRESH_CATALOG) {
			(void)bl_sm_dispatch(BL_EVENT_REFRESH_CATALOG);
		} else if (ev == BL_UI_POLL_CONFIRM_INSTALL) {
			(void)bl_sm_dispatch(BL_EVENT_CONFIRM_INSTALL);
		} else if (ev == BL_UI_POLL_LAUNCH_APP) {
			(void)bl_sm_dispatch(BL_EVENT_LAUNCH_FLASHED_APP);
		} else if (ev == BL_UI_POLL_COLD_RESET) {
			(void)bl_sm_dispatch(BL_EVENT_COLD_RESET);
		} else if (ev == BL_UI_POLL_REBOOT_BTN) {
			(void)bl_sm_dispatch(BL_EVENT_REBOOT);
		}
		break;
	}
	case BL_STATE_PROGRAM:
		if (!s_ctx.program_done_posted) {
			s_ctx.program_done_posted = true;
			(void)bl_sm_dispatch(BL_EVENT_PROGRAM_DONE);
		}
		break;
	case BL_STATE_INFORM: {
		bl_sm_ui_poll_t ev = bl_sm_ui_poll_inform();

		if (ev == BL_UI_POLL_DISMISS) {
			(void)bl_sm_dispatch(BL_EVENT_DISMISS);
		}
		break;
	}
	default:
		break;
	}
}

void bl_sm_run(void)
{
	bl_sm_init();
	bl_sm_ui_display_init();
	(void)bl_sm_dispatch(BL_EVENT_POWER_ON);

	for (;;) {
		bl_sm_run_do();
		HAL_Delay(10);
	}
}
