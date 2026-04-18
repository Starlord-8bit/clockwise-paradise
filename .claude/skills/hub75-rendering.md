---
description: Reference for the HUB75 64x64 panel and its I2S DMA driver on this project. Covers init ordering, the single-owner rule for the panel object, per-frame constraints, color-order / phase / driver / i2sspeed knobs, and rendering primitives that survive long-run operation.
---

# HUB75 / I2S DMA — Clockwise Paradise Edition

Library: [ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA) v3.0.12 (pinned in `firmware/platformio.ini`). Do not change the pin without an ADR; driver revisions change DMA semantics.

Authoritative files:
- [firmware/src/app/core/DisplayControl.cpp](../../firmware/src/app/core/DisplayControl.cpp) — the only place the panel is constructed.
- [firmware/src/app/core/DisplayControl.h](../../firmware/src/app/core/DisplayControl.h)
- `AppState::display` — `MatrixPanel_I2S_DMA*`, shared pointer all renderers use.

## The one-owner rule (RT-7)

`displaySetup()` is the only function that may call `new MatrixPanel_I2S_DMA(...)` and `display->begin()`. Every other renderer takes `state.display` by pointer and never recreates the panel. Violating this corrupts DMA buffers and crashes.

## Init knobs

Set in `mxconfig` **before** `begin()`:

| Knob | Controls | Default | Notes |
|---|---|---|---|
| `ledColorOrder` (RGB/RBG/GBR) | Which GPIO pairs are R/G/B | RGB | Per-panel; user-configurable |
| `clkphase` | Falling vs rising edge sampling | false | Fixes some ghosting / bands |
| `driver` | Shift driver IC (FM6126A, ICN2038, …) | default | Wrong value → garbage or blank |
| `i2sspeed` | 8 / 16 / 20 MHz DMA clock | default | Higher = brighter but more EMI |
| `gpio.e` | Row-select E pin for 64-row panels | required | Without this only 32 rows address correctly |

After `begin()`, only brightness and framebuffer writes are safe. Changing a GPIO mapping post-init without a full reinit is unsupported.

## Per-frame constraints

- **No heap allocation** in any widget `tick()` / render path (RT-2). Pre-allocate stack or member buffers.
- **Non-blocking only.** Render is on the `loop()` thread; see [esp32-freertos](./esp32-freertos.md).
- **Use the `state.display` pointer** via `AppState` or `Locator`. Do not cache it in another singleton.
- **Brightness:** change only via `display->setBrightness8(n)`. Guard with a change detector to avoid writing every frame.

## Symptom → cause cheat sheet

| Symptom | Likely cause | Fix path |
|---|---|---|
| Full-screen flicker | DMA underrun / task starvation | Find blocking call in `loop()`; see [esp32-freertos](./esp32-freertos.md) |
| Ghosting (prev frame visible) | Incomplete flush before next write | Do not toggle phase at runtime; check clkphase init |
| Red shows as blue / swapped channels | `ledColorOrder` wrong | User setting; test RGB → RBG → GBR |
| Garbage bottom half | Missing/miswired E pin | Check `gpio.e` matches hardware |
| Vertical tearing | `reversePhase` mismatch vs panel | Toggle at init only |
| Blank during OTA | DMA collision with OTA write to display buffer | Known; transient; device reboots after OTA |
| Corruption after hours/days | Heap fragmentation | Find per-frame allocation; see CONSTRAINTS RT-2 |
| Wrong rows after power-cycle | Driver IC mismatch | Check `driver` config vs panel IC silkscreen |

## Rendering primitives already available

- `display->fillScreen(color)` — clear with 565 color
- `display->fillRect(x, y, w, h, color)`
- `display->drawRect(...)`, `drawPixel(...)`, `drawLine(...)`
- `display->color565(r, g, b)` — 8-bit RGB → 565
- `cw-gfx-engine` provides `Sprite`, `Tile`, `Object`, `EventBus` for widget composition. Use these before writing a new primitive.

## Night mode

`nightModeCheck` in `DisplayControl.cpp` owns the brightness transition between day/night based on NVS settings (`nightMode`, `nightTrigger`, `nightAction`, `nightBright`, `nightLdrThres`). Custom night behavior: extend this function, do not write a parallel implementation.

## Boot sanity test

`displayBootSanityTest` draws a 21/22/21 R/G/B stripe pattern at fixed brightness for ~900 ms at startup to distinguish "config makes it dark" from "panel is dead." Preserve it for diagnostic purposes — a user who sees blackness on boot can tell us whether even the sanity test rendered.

## Things not to do

- `clearScreen()` from an ISR — races DMA.
- Reconstruct `MatrixPanel_I2S_DMA` at runtime.
- Modify the third-party HUB75 library under `components/` (RT-6).
- Increase DMA buffer size on the assumption of PSRAM. This board has none.
- Add `delay()` in `loop()` to mask flicker — you are hiding RT-1, not fixing it.

## Debugging checklist

1. Check the boot sanity test appears. If it doesn't, it's almost always a GPIO / E-pin issue.
2. Check `/api/prefs` for `ledColorOrder`, `reversePhase`, `driver`, `i2cSpeed`.
3. Check `esp_get_free_heap_size()` trend over hours if symptom is time-dependent.
4. Suspect the last change to any function reachable from `tick()` — look for new heap usage.
5. Capture serial: `Task watchdog got triggered`, `E (...) I2S`, `rst:0xf` (watchdog reset) all point to different root causes.
