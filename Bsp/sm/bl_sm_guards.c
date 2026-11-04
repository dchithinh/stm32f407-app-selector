#include "bl_sm_guards.h"
#include "boot_app.h"
#include "boot_layout.h"
#include "boot_policy.h"

bool bl_guard_true(const bl_sm_ctx_t *ctx)
{
	(void)ctx;
	return true;
}

bool bl_guard_app_image_valid_and_autoboot(const bl_sm_ctx_t *ctx)
{
	(void)ctx;
#if BOOT_FORCE_LOADER
	return false;
#else
	return boot_app_is_valid() && !boot_should_enter_loader();
#endif
}

bool bl_guard_enter_loader(const bl_sm_ctx_t *ctx)
{
	(void)ctx;
#if BOOT_FORCE_LOADER
	return true;
#else
	return !boot_app_is_valid() || boot_should_enter_loader();
#endif
}

bool bl_guard_catalog_ready(const bl_sm_ctx_t *ctx)
{
	return ctx != NULL && ctx->catalog_ready;
}

bool bl_guard_catalog_unavailable(const bl_sm_ctx_t *ctx)
{
	return ctx != NULL && ctx->catalog_unavailable;
}

bool bl_guard_app_selected(const bl_sm_ctx_t *ctx)
{
	return ctx != NULL && ctx->selected_index >= 0;
}

bool bl_guard_app_image_valid(const bl_sm_ctx_t *ctx)
{
	(void)ctx;
	return boot_app_is_valid();
}

bool bl_guard_not_app_image_valid(const bl_sm_ctx_t *ctx)
{
	return !bl_guard_app_image_valid(ctx);
}

bool bl_guard_program_success(const bl_sm_ctx_t *ctx)
{
	return ctx != NULL && ctx->program_success;
}

bool bl_guard_program_fail(const bl_sm_ctx_t *ctx)
{
	return ctx != NULL && !ctx->program_success;
}

bool bl_guard_grace_remaining_zero(const bl_sm_ctx_t *ctx)
{
	return ctx != NULL && ctx->grace_sec_remaining == 0U;
}
