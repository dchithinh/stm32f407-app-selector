#include "bl_sm_actions.h"
#include "bl_sm_ui.h"
#include "boot_app.h"
#include "boot_loader.h"
#include "boot_policy.h"
#include "debug_uart.h"
#include <stdio.h>

void bl_action_noop(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_start_assess(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	printf("SM: POWER_ON / start_assess\r\n");
}

void bl_action_validate_vector_table(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	boot_app_log_status();
	boot_policy_log();
}

void bl_action_latch_boot_decision(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_route_boot_target(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_init_grace_ui(bl_sm_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	bl_sm_ui_grace_enter(ctx);
}

void bl_action_init_loader_shell(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	bl_sm_ui_loader_shell_enter();
}

void bl_action_show_countdown(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_hide_countdown(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	bl_sm_ui_grace_hide();
}

void bl_action_tick_one_second(bl_sm_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	bl_sm_ui_grace_tick(ctx);
}

void bl_action_notify_grace_elapsed(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	printf("SM: GRACE_ELAPSED\r\n");
}

void bl_action_handoff_to_app(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	printf("SM: handoff_to_app\r\n");
	boot_jump_to_app();
}

void bl_action_mount_and_scan_apps(bl_sm_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	bl_sm_ui_catalog_scan(ctx);
}

void bl_action_classify_catalog(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_show_list(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	bl_sm_ui_show_browse();
}

void bl_action_show_error(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	bl_sm_ui_show_inform(false);
}

void bl_action_render_app_list(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_update_selection(bl_sm_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	bl_sm_ui_apply_selection(ctx->selected_index);
}

void bl_action_rescan_sd(bl_sm_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	bl_sm_ui_rescan(ctx);
}

void bl_action_begin_program(bl_sm_ctx_t *ctx)
{
	if (ctx == NULL) {
		return;
	}
	ctx->program_success = bl_sm_ui_run_program();
}

void bl_action_show_install_failed(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	bl_sm_ui_show_inform(true);
}

void bl_action_render_message(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_keep_message(bl_sm_ctx_t *ctx)
{
	(void)ctx;
}

void bl_action_nvic_reset(bl_sm_ctx_t *ctx)
{
	(void)ctx;
	boot_mcu_reboot();
}
