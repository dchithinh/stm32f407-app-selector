#ifndef BL_SM_ACTIONS_H
#define BL_SM_ACTIONS_H

#include "bl_sm_types.h"

void bl_action_start_assess(bl_sm_ctx_t *ctx);
void bl_action_validate_vector_table(bl_sm_ctx_t *ctx);
void bl_action_latch_boot_decision(bl_sm_ctx_t *ctx);
void bl_action_route_boot_target(bl_sm_ctx_t *ctx);
void bl_action_init_grace_ui(bl_sm_ctx_t *ctx);
void bl_action_init_loader_shell(bl_sm_ctx_t *ctx);
void bl_action_show_countdown(bl_sm_ctx_t *ctx);
void bl_action_hide_countdown(bl_sm_ctx_t *ctx);
void bl_action_tick_one_second(bl_sm_ctx_t *ctx);
void bl_action_notify_grace_elapsed(bl_sm_ctx_t *ctx);
void bl_action_handoff_to_app(bl_sm_ctx_t *ctx);
void bl_action_mount_and_scan_apps(bl_sm_ctx_t *ctx);
void bl_action_classify_catalog(bl_sm_ctx_t *ctx);
void bl_action_show_list(bl_sm_ctx_t *ctx);
void bl_action_show_error(bl_sm_ctx_t *ctx);
void bl_action_render_app_list(bl_sm_ctx_t *ctx);
void bl_action_update_selection(bl_sm_ctx_t *ctx);
void bl_action_rescan_sd(bl_sm_ctx_t *ctx);
void bl_action_begin_program(bl_sm_ctx_t *ctx);
void bl_action_show_install_failed(bl_sm_ctx_t *ctx);
void bl_action_render_message(bl_sm_ctx_t *ctx);
void bl_action_keep_message(bl_sm_ctx_t *ctx);
void bl_action_nvic_reset(bl_sm_ctx_t *ctx);
void bl_action_noop(bl_sm_ctx_t *ctx);

#endif
