#ifndef BL_SM_H
#define BL_SM_H

#include "bl_sm_types.h"

void bl_sm_init(void);
bool bl_sm_dispatch(bl_event_t event);
void bl_sm_run(void) __attribute__((noreturn));

const bl_sm_ctx_t *bl_sm_context(void);

#endif
