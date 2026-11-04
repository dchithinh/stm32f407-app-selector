#ifndef BL_SM_GUARDS_H
#define BL_SM_GUARDS_H

#include "bl_sm_types.h"

bool bl_guard_true(const bl_sm_ctx_t *ctx);
bool bl_guard_app_image_valid_and_autoboot(const bl_sm_ctx_t *ctx);
bool bl_guard_enter_loader(const bl_sm_ctx_t *ctx);
bool bl_guard_catalog_ready(const bl_sm_ctx_t *ctx);
bool bl_guard_catalog_unavailable(const bl_sm_ctx_t *ctx);
bool bl_guard_app_selected(const bl_sm_ctx_t *ctx);
bool bl_guard_app_image_valid(const bl_sm_ctx_t *ctx);
bool bl_guard_not_app_image_valid(const bl_sm_ctx_t *ctx);
bool bl_guard_program_success(const bl_sm_ctx_t *ctx);
bool bl_guard_program_fail(const bl_sm_ctx_t *ctx);
bool bl_guard_grace_remaining_zero(const bl_sm_ctx_t *ctx);

#endif
