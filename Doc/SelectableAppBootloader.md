# Selectable application bootloader (STM32F407)

Firmware in this repo is a **custom bootloader** for the STM32F407 Discovery plus the 2.4" TFT HAT. It lets you keep many application images on a microSD card, pick one from a touch UI, program internal flash when needed, and run the selected app—without using ST-Link for every app change.

**Target board:** [stm32f407_tft_2.4inch_hat](https://github.com/dchithinh/pcb-lab/tree/main/stm32f407_tft_2.4inch_hat) (schematic, pin table, and photos in pcb-lab).

---

## Goals

- Mount a FAT volume on the **microSD** socket on the LCD module (SPI3, dedicated chip select).
- Show a **menu on the ILI9341** (SPI2) and accept **touch** input (XPT2046 on SPI3).
- List installable apps from the SD card; on user selection, **verify** and **program** one application region in internal flash, then **jump** to that app.
- After the bootloader is programmed once over SWD, routine app swaps use **SD card only**.

## Non-goals

- Running application code **directly from SD** (STM32F407 has no execute-in-place from SD on this hardware).
- Storing many full-sized apps **simultaneously** in the 1 MB internal flash (flash holds **one active app** at a time; the catalog lives on SD).

---

## How “many apps” works

| Location | Role |
|----------|------|
| **microSD** | Catalog of app images (`.bin` plus optional metadata). Limited by card capacity. |
| **Internal flash** | Bootloader (fixed, low addresses) + **one app slot** (remaining flash). |

Selecting an app means: copy the chosen image from SD into the app slot (if missing or outdated), validate, then start execution. Images stay on SD as the source of truth for updates and recovery.

---

## Boot flow

1. Reset → bootloader at `0x08000000`.
2. Initialize display, touch, and SD (FatFs).
3. If a **valid app** is already in the app slot and policy allows (e.g. user did not hold “enter menu”, or auto-boot timeout), **jump to app**.
4. Otherwise show the **selection UI** (list apps from SD).
5. On selection: check size/CRC (and optional version rules) → erase/program app region → verify → jump.
6. On failure: stay in bootloader, show error, keep SD images intact.

Handoff to the app: set `VTOR` to the app vector table base, reset MSP from the app vector, tear down SysTick/NVIC (and reset USART2), `HAL_RCC_DeInit()`, jump to the app reset handler (Thumb address). The app must call `__enable_irq()` early in `main` (bootloader leaves IRQs off) and run `SystemClock_Config()` again. Bootloader and Pomodoro both use **84 MHz SYSCLK** (APB1 42 MHz for SPI2 / USART2).

### Enter bootloader

1. **Autoboot grace (5 s):** press **user button** (PA0) or send **`j`** on USART2 (115200) → stay in bootloader.
2. **At reset:** hold **user button** through the early sample window → enter bootloader (same as grace intent).
3. **Invalid app slot** → always bootloader.
4. **Dev:** `BOOT_FORCE_LOADER` **`1`** in `boot_layout.h` — never autoboot.

From the bootloader menu, send **`a`** to run the app. **`j`** is only for canceling autoboot during grace.

Blue **user button / PA0**: active-high (R39 pulldown on board); firmware uses **`GPIO_NOPULL`**, not internal pull-up.

### Clock and app handoff (why 84 MHz matched)

The app does **not** require the same SYSCLK as the bootloader forever — it always runs its own `SystemClock_Config()` after jump. What broke earlier was a mix of issues, not “168 vs 84” alone:

| Issue | Effect |
|--------|--------|
| Jump with **even** reset vector (Thumb bit stripped) | App never really started |
| **`__disable_irq()`** left set after jump | DMA LCD flush never finished → white screen |
| **UART before** `SystemClock_Config()` | One readable line then garbage baud |
| **Different PLL** (168 BL → 84 app) + `HAL_RCC_DeInit` | Extra reconfig; works if order/IRQ/UART are correct |

Using **84 MHz in both** avoids an extra PLL change at handoff and matches the Pomodoro project tuning, so bring-up is simpler. With the fixes above, mismatched clocks can work if the app re-clocks cleanly and enables IRQs before DMA/UI.

---

## Proposed flash layout (STM32F407VG, 1 MB)

Values are initial planning numbers; adjust after sizing the bootloader binary.

| Region | Start | Size (typical) | Notes |
|--------|--------|----------------|--------|
| Bootloader | `0x08000000` | **64 KB** (sectors 0–3) | `STM32F407VGTX_BL.ld` |
| App slot | `0x08010000` | **960 KB** (sectors 4–11) | Apps use `STM32F407VGTX_APP.ld`; `VECT_TAB_OFFSET = 0x00010000` |

App projects must use a linker script and vector table offset matching the chosen app base. Bootloader and apps should agree on clock and basic HAL expectations at handoff.

---

## SD card layout (convention)

Example (TBD in implementation):

```
/apps/
  pomodoro.bin          # raw image, programmed from 0x08010000
  stm32f407xx_pomodoro.elf   # ARM ELF32; only FLASH PT_LOAD is programmed
```

`.bin` size must be ≤ app slot. `.elf` may be larger (debug); the bootloader writes loadable flash segments only and still requires `APP.ld` (`0x08010000`). Do not copy an ELF and flash it as if it were a raw `.bin`.

---

## Hardware interfaces (HAT)

| Function | Bus / pins (see pcb-lab README) |
|----------|----------------------------------|
| LCD ILI9341 | SPI2 — `LCD_SCK`, `LCD_MOSI`, `LCD_MISO`, `LCD_CS`, `LCD_DC`, `LCD_RESET` |
| Touch XPT2046 | SPI3 — shared CLK/MOSI/MISO with SD; CS `LCD_T_CS` (PE3) |
| microSD | SPI3 — CS `LCD_SD_CS` (PE4) |
| Debug console | USART2 — PA2 / PA3 |

Only one SPI3 peripheral (touch or SD) is active at a time; drivers must serialize access and drive the correct CS.

---

## Safety and recovery

- **Power loss during programming:** Do not mark the new image active until verify completes; keep golden copies on SD.
- **Bad image:** Refuse to erase if checks fail; bootloader remains runnable via reset.
- **First-time setup:** Bootloader still requires one SWD/ST-Link flash; afterward updates are SD-driven.
- Optional: store last-selected app id in backup registers or a small config sector for auto-boot.

---

## Implementation notes for this repo

- Reuse board support where possible (e.g. LCD/touch from `stm32f407xx_pomodoro` in the same MCU workspace).
- Add SPI3 SD + FatFs (`diskio` glue); enable HAL SPI and FatFs in Cube/HAL config as needed.
- Split linker script: bootloader must not use the full 1024 KB at `0x08000000` once the app region is defined.
- Companion **application** projects are separate images built for the app flash base, delivered as `.bin` on SD.

---

## Related documentation

- HAT pinout and connectors: [pcb-lab — stm32f407_tft_2.4inch_hat](https://github.com/dchithinh/pcb-lab/tree/main/stm32f407_tft_2.4inch_hat)
