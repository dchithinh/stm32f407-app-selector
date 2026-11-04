# App Selector Bootloader

Selectable-application **bootloader** for **STM32F407** on the [2.4" TFT HAT](https://github.com/dchithinh/pcb-lab/tree/main/stm32f407_tft_2.4inch_hat) (ILI9341 + XPT2046 touch + microSD). Flash the bootloader once over SWD; swap applications from the SD card using the touch menu—no ST-Link for every app update.

## Features

- FAT microSD catalog under `/apps/` (`.bin` or ARM **ELF32**)
- Touch UI to pick an image, program internal flash, then run the app
- **Autoboot** into a valid flashed app after a 10 s grace period
- Debug console on **USART2** (PA2/PA3, **115200** 8N1)

## Flash layout (STM32F407VG, 1 MB)

| Region     | Start        | Size   | Linker script              |
|------------|--------------|--------|----------------------------|
| Bootloader | `0x08000000` | 64 KB  | `STM32F407VGTX_BL.ld`      |
| App slot   | `0x08010000` | 960 KB | `STM32F407VGTX_APP.ld` (template in this repo) |

Constants are in `Core/Inc/boot_layout.h`.

## Build and flash (bootloader)

1. Open the project in **STM32CubeIDE**.
2. Build **Debug** or **Release** (linker script is `STM32F407VGTX_BL.ld`).
3. Program the MCU at **`0x08000000`** via ST-Link (full chip or bootloader region only).

## SD card

Format the card **FAT32**. Create:

```
/apps/
  my_app.bin
  my_app1.elf
```

| File type | Behavior |
|-----------|----------|
| **`.bin`** | Raw image written starting at `0x08010000`. Size must be ≤ 960 KB. |
| **`.elf`** | Only **PT_LOAD** segments that map to internal flash are programmed; RAM/CCM loads are skipped. The linked flash base must still be **`0x08010000`**. |

The menu lists at most **100** installable files (`.bin`/`.elf`) from `/apps/` (`SD_APPS_MAX` in `Bsp/sdcard/sd_apps.h`). Additional matching files are **ignored** during scan (no error on UART); remove or rename extras if you need a different set visible.

After a successful install from the UI, the bootloader **resets** and then autoboots or shows the menu according to policy.

**ST-Link note:** A `.bin` has no load address in the file. When programming an app directly (bypassing this bootloader), use start address **`0x08010000`**, not `0x08000000`.

## Application projects

Each app is a **separate** firmware image, not part of this repo.

1. Copy **`STM32F407VGTX_APP.ld`** into the app project.
2. Set vector table offset to match the app base, e.g. in `system_stm32f4xx.c`:  
   `VECT_TAB_OFFSET = 0x00010000U` (with `VECT_TAB_SRAM` unset for flash).
3. Build **Release** and copy the `.bin` (or `.elf`) to `/apps/` on the SD card.

Example companion app in the same workspace: [stm32f407xx_pomodoro](https://github.com/dchithinh/stm32f407xx_pomodoro)

## Control flow (state machine)

Behavior matches [`Doc/bootloader_design.puml`](Doc/bootloader_design.puml) and [`Doc/bootloader_state_machine.md`](Doc/bootloader_state_machine.md).

| Module | Role |
|--------|------|
| `Bsp/sm/bl_sm_engine.c` | Transition table, dispatch, entry/exit, main loop |
| `Bsp/sm/bl_sm_guards.c` | Pure guards |
| `Bsp/sm/bl_sm_actions.c` | Effects (flash, reset, handoff) |
| `Bsp/sm/bl_sm_ui.c` | LCD/touch/SD UI; polls user input into events |

`main()` initializes HAL and boot policy, then calls `bl_sm_run()` (no separate policy branch / `boot_ui_run()` loop).

## Entering the bootloader

| Condition | Result |
|-----------|--------|
| Valid app + autoboot allowed | 10 s grace (`BOOT_AUTOBOOT_GRACE_MS`), then jump to app |
| During grace: **user button** (PA0) or UART **`j`** | Stay in bootloader |
| **User button** held ~400 ms at reset | Bootloader menu |
| Invalid or empty app slot | Bootloader menu |
| `BOOT_FORCE_LOADER` **`1`** in `boot_layout.h` | Never autoboot (development) |

User button is **active high** (board pulldown on PA0).

## UART commands (in bootloader menu)

Printed at startup (`?` repeats help):

| Key | Action |
|-----|--------|
| Touch **Reboot** | `REBOOT`: jump to flashed app if valid, else MCU reset |
| **`a`** | `LAUNCH_FLASHED_APP` — run flashed app (if valid) |
| **`r`** | `REFRESH_CATALOG` |
| **`b`** | `COLD_RESET` — MCU reset |
| **`?`** | Help |

During autoboot grace only: **`j`** cancels autoboot (same intent as pressing the user button).

## License / upstream

STM32 HAL, CMSIS, and FatFs follow their respective licenses in `Drivers/` and `Middlewares/`. Application-specific code is under `Core/` and `Bsp/`.
