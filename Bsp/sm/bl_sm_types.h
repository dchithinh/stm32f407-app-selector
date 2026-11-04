#ifndef BL_SM_TYPES_H
#define BL_SM_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	BL_STATE_NONE = 0,
	BL_STATE_ASSESS_IMAGE,
	BL_STATE_AUTOBOOT_GRACE,
	BL_STATE_AWAIT_CATALOG,
	BL_STATE_BROWSE,
	BL_STATE_PROGRAM,
	BL_STATE_INFORM,
	BL_STATE_RUNNING_APP,
	BL_STATE_COLD_RESET,
} bl_state_t;

typedef enum {
	BL_EVENT_POWER_ON = 0,
	BL_EVENT_ASSESS_DONE,
	BL_EVENT_GRACE_ELAPSED,
	BL_EVENT_STAY_IN_LOADER,
	BL_EVENT_SCAN_DONE,
	BL_EVENT_SELECT_APP,
	BL_EVENT_REFRESH_CATALOG,
	BL_EVENT_CONFIRM_INSTALL,
	BL_EVENT_LAUNCH_FLASHED_APP,
	BL_EVENT_REBOOT,
	BL_EVENT_COLD_RESET,
	BL_EVENT_PROGRAM_DONE,
	BL_EVENT_DISMISS,
	BL_EVENT_TICK,
} bl_event_t;

typedef struct {
	bl_state_t state;
	bool catalog_ready;
	bool catalog_unavailable;
	bool program_success;
	bool assess_dispatched;
	bool scan_done_posted;
	bool program_done_posted;
	int selected_index;
	uint32_t grace_sec_remaining;
	uint32_t grace_second_deadline_ms;
	bool grace_counting;
} bl_sm_ctx_t;

#endif
