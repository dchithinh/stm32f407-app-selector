# Bootloader FSM (design-first)

**Flat FSM** — no composite / outer boxes (`SYSTEM`, `LOADER_SHELL`, nested `COUNTING`). Choice vertices are UML **diamonds** in [`bootloader_design.puml`](bootloader_design.puml). Firmware (`Bsp/sm/bl_sm_engine.c`) is the same machine with diamonds **flattened** to two table rows (same event, exclusive guards). There is no `STATE_ROUTE_TARGET` / `STATE_REBOOT_TARGET` enum.

**Naming:**

| Element | Convention | Example |
|---------|------------|---------|
| **State** | `ALL_CAPS` | `BROWSE`, `AUTOBOOT_GRACE` |
| **Event** | `ALL_CAPS` | `CONFIRM_INSTALL`, `REBOOT` |
| **Guard** | `ALL_CAPS` in `[brackets]` | `[APP_SELECTED]` |
| **Action (effect)** | `snake_case` after `/` | `/ begin_program` |
| **Transition label** | `EVENT [guard] / action` | `CONFIRM_INSTALL [APP_SELECTED] / begin_program` |

Render:

```bash
java -jar plantuml.jar Doc/bootloader_design.puml
```

## Leaf states (`bl_state_t`)

| State | Design entry / do / exit | Code (`bl_sm_engine.c`) |
|-------|--------------------------|-------------------------|
| **ASSESS_IMAGE** | entry: `validate_vector_table` · exit: `latch_boot_decision` | entry: `boot_app_log_status` + `boot_policy_log` · exit: latch (no-op) |
| **AUTOBOOT_GRACE** | entry: `show_countdown` · do: `poll_inputs`; `tick_one_second` · exit: `hide_countdown` | entry: `bl_sm_ui_grace_enter` via `init_grace_ui` · do: poll stay + tick · exit: `hide_countdown` |
| **AWAIT_CATALOG** | entry: `mount_and_scan_apps` · exit: `classify_catalog` | entry: `bl_sm_ui_catalog_scan` · exit: classify (no-op; flags set in scan) |
| **BROWSE** | entry: `render_app_list` · do: `poll_list_inputs` | entry: **no second draw** (`render_app_list` is empty — list is drawn on `show_list` / `rescan`) · do: `bl_sm_ui_poll_browse` |
| **PROGRAM** | entry: `erase_app_slot` · do: flash · exit: verify | **work is `begin_program`** (`bl_sm_ui_run_program`) on the incoming transition; entry only clears `program_done_posted`; then `PROGRAM_DONE` |
| **INFORM** | entry: `render_message` · do: `poll_dismiss` | draw on `show_error` / `show_install_failed`; entry `render_message` is empty · do: `bl_sm_ui_poll_inform` |
| **RUNNING_APP** | entry: `deinit_bl_jump_app` (terminate) | `handoff_to_app` → `boot_jump_to_app()` (does not return) |
| **COLD_RESET** | entry: `nvic_system_reset` (terminate) | `nvic_reset` → `boot_mcu_reboot()` |

`init_loader_shell` is the **transition action** into `AWAIT_CATALOG` (`bl_sm_ui_loader_shell_enter`: reset selection/scroll). Display/SPI/touch init is `bl_sm_ui_display_init()` once in `bl_sm_run()`.

## Choice diamonds (diagram) vs C

| Diamond | Incoming (diagram) | Outgoing | C (`s_transitions[]`) |
|---------|--------------------|----------|------------------------|
| **ROUTE_TARGET** | `ASSESS_DONE [ ] / route_boot_target` | `[APP_IMAGE_VALID_AND_AUTOBOOT]` → `AUTOBOOT_GRACE` · `[ENTER_LOADER]` → `AWAIT_CATALOG` | From **`ASSESS_IMAGE`**, event `ASSESS_DONE`, two rows |
| **CATALOG_RESULT** | `SCAN_DONE [ ] / classify_catalog` | `[CATALOG_READY]` → `BROWSE` · `[CATALOG_UNAVAILABLE]` → `INFORM` | From **`AWAIT_CATALOG`**, event `SCAN_DONE`, two rows |
| **REBOOT_TARGET** | `REBOOT [ ] / classify_reboot` | `[APP_IMAGE_VALID]` → `RUNNING_APP` · `[NOT_APP_IMAGE_VALID]` → `COLD_RESET` | From **`BROWSE`**, event `REBOOT`, two rows |

`LAUNCH_FLASHED_APP` is **not** a choice: one row `[APP_IMAGE_VALID]`; if false, stay in `BROWSE`.

## Events and transitions

Design (PlantUML) lists diamonds as vertices. **Firmware table** (authoritative for C):

| From | Event [guard] / action | To | Source |
|------|------------------------|-----|--------|
| `NONE` | `POWER_ON [ ] / start_assess` | `ASSESS_IMAGE` | `bl_sm_run()` |
| `ASSESS_IMAGE` | `ASSESS_DONE [APP_IMAGE_VALID_AND_AUTOBOOT] / init_grace_ui` | `AUTOBOOT_GRACE` | do-loop posts `ASSESS_DONE` once |
| `ASSESS_IMAGE` | `ASSESS_DONE [ENTER_LOADER] / init_loader_shell` | `AWAIT_CATALOG` | same |
| `AUTOBOOT_GRACE` | `GRACE_ELAPSED [ ] / handoff_to_app` | `RUNNING_APP` | countdown remaining = 0 |
| `AUTOBOOT_GRACE` | `STAY_IN_LOADER [ ] / init_loader_shell` | `AWAIT_CATALOG` | user btn, UART `j`, touch BOOT MENU |
| `AWAIT_CATALOG` | `SCAN_DONE [CATALOG_READY] / show_list` | `BROWSE` | do-loop after scan |
| `AWAIT_CATALOG` | `SCAN_DONE [CATALOG_UNAVAILABLE] / show_error` | `INFORM` | mount/scan fail |
| `BROWSE` | `SELECT_APP [ ] / update_selection` | `BROWSE` | row tap (self-transition: no re-entry) |
| `BROWSE` | `REFRESH_CATALOG [ ] / rescan_sd` | `BROWSE` | UART `r` |
| `BROWSE` | `CONFIRM_INSTALL [APP_SELECTED] / begin_program` | `PROGRAM` | Install button |
| `BROWSE` | `LAUNCH_FLASHED_APP [APP_IMAGE_VALID] / handoff_to_app` | `RUNNING_APP` | UART `a` |
| `BROWSE` | `REBOOT [APP_IMAGE_VALID] / handoff_to_app` | `RUNNING_APP` | touch **Reboot** |
| `BROWSE` | `REBOOT [NOT_APP_IMAGE_VALID] / nvic_reset` | `COLD_RESET` | touch **Reboot** |
| `BROWSE` | `COLD_RESET [ ] / nvic_reset` | `COLD_RESET` | UART `b` |
| `PROGRAM` | `PROGRAM_DONE [SUCCESS] / nvic_reset` | `COLD_RESET` | install ok |
| `PROGRAM` | `PROGRAM_DONE [FAIL] / show_install_failed` | `INFORM` | install fail |
| `INFORM` | `DISMISS [CATALOG_READY] / show_list` | `BROWSE` | Back if catalog ok |
| `INFORM` | `DISMISS [CATALOG_UNAVAILABLE] / keep_message` | `INFORM` | Back if still no catalog |

Self-transitions (`from == to`) skip `on_exit` / `on_entry` (avoids list flicker on `SELECT_APP`).

`BL_EVENT_TICK` exists in `bl_sm_types.h` but is unused; grace uses `do` + `GRACE_ELAPSED`.

## Guards (`bl_sm_guards.c`)

| Guard | True when |
|-------|-----------|
| `APP_IMAGE_VALID_AND_AUTOBOOT` | `boot_app_is_valid()` **and** not `BOOT_FORCE_LOADER` **and** not `boot_should_enter_loader()` |
| `ENTER_LOADER` | `BOOT_FORCE_LOADER` **or** invalid app **or** `boot_should_enter_loader()` |
| `APP_IMAGE_VALID` | `boot_app_is_valid()` |
| `NOT_APP_IMAGE_VALID` | `!boot_app_is_valid()` |
| `APP_SELECTED` | `ctx->selected_index >= 0` |
| `CATALOG_READY` | SD mount + scan succeeded |
| `CATALOG_UNAVAILABLE` | Mount or scan failed |
| `SUCCESS` / `FAIL` | `ctx->program_success` after `begin_program` |

## Files

| Role | Path |
|------|------|
| Dispatch / table | `Bsp/sm/bl_sm_engine.c` |
| Types | `Bsp/sm/bl_sm_types.h` |
| Guards | `Bsp/sm/bl_sm_guards.c` |
| Actions | `Bsp/sm/bl_sm_actions.c` |
| UI adapter + original screens | `Bsp/ui/boot_ui.c` (`bl_sm_ui_*`) |
| Policy / app flash | `boot_policy.c`, `boot_app.c` |
| Catalog | `sd_apps.c` |
| Entry | `main.c` → `bl_sm_run()` |

