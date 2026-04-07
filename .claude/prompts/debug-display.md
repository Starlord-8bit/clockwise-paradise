---
description: Template for diagnosing display glitches, DMA collisions, panel wiring issues, and rendering artifacts on the 64x64 HUB75 panel. Use this prompt when the display shows corruption, flickering, wrong colors, or ghost pixels.
---

# Task: Debug Display / DMA Issue

## Symptom Classification

Identify the symptom before diagnosing:

| Symptom | Likely cause |
|---------|-------------|
| Full screen flicker at regular intervals | DMA buffer underrun or task starvation |
| Ghosting (previous frame visible) | Incomplete DMA flush before next write |
| Wrong color order (e.g., red shows as blue) | `colorOrder` setting wrong (RGB/RBG/GBR) |
| Half panel showing garbage | I2S DMA memory fragmentation |
| Vertical bands / tearing | `reversePhase` setting |
| Panel goes blank during OTA | DMA collision with HTTP task |
| Corruption only in bottom half | Panel chain addressing (A/B/C/D/E pins) |
| Corruption after long uptime | Heap fragmentation (check `esp_get_free_heap_size()`) |

---

## Diagnostic Steps

### Step 1 — Capture serial output

Use the `rackpeek` tmux session or run `/diagnose` to capture boot log and runtime output.

Look for:
- `[DMA]` tagged messages
- `E (...)` error lines during display operations
- Watchdog triggers (`Task watchdog got triggered`)
- Memory warnings (`heap`)

### Step 2 — Check DMA configuration in main.cpp

Read `firmware/src/main.cpp` and verify:

```cpp
// Key parameters to check:
MatrixPanel_I2S_DMA *dma_display = nullptr;

HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // should be 64
    PANEL_RES_Y,   // should be 64
    PANEL_CHAIN    // number of panels chained
);
mxconfig.gpio.e = 18;          // E pin for 64-row panels — must be set
mxconfig.clkphase = false;     // try toggling if phase issues
mxconfig.driver = HUB75_I2S_CFG::FM6126A;  // check driver matches panel IC
```

For 64×64 panels: the E pin **must** be configured. Without it, only 32 rows address correctly.

### Step 3 — Isolate the conflicting task

The display DMA runs on a dedicated I2S interrupt. Conflicts happen when:

1. A FreeRTOS task writes to the framebuffer without yielding (`vTaskDelay(1)` missing)
2. OTA write and DMA are active simultaneously (known collision — see `CWWebServer.h:141`)
3. HTTP server task runs too long without yield

Check `firmware/src/main.cpp` loop() for blocking patterns:
```bash
grep -n "delay\|vTaskDelay\|while\|for(" firmware/src/main.cpp
```

### Step 4 — Color order diagnosis

If colors are wrong, the `colorOrder` NVS setting may be mismatched with the panel's LED order.

Check current setting:
```bash
curl -s http://192.168.1.212/api/prefs | python3 -m json.tool | grep color
```

Try each value in order: RGB (0) → RBG (1) → GBR (2) via the web UI.
The correct value is panel-specific and must be set per-device.

### Step 5 — Heap / memory check

Add temporarily to `loop()` for diagnosis only (remove before committing):
```cpp
ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
```

If heap drops below ~50KB: investigate what is allocating memory in the loop.

---

## Known Collision: OTA + DMA

`CWWebServer.h:141` shows `StatusController::getInstance()->printCenter("Uploading...", 32)`
is called during OTA — this writes to the display buffer while DMA is active.

If corruption only happens during OTA uploads: this is expected behavior.
The device reboots after OTA completes, so display state during OTA is not persistent.

---

## What NOT to Do

- Do not increase DMA buffer size without checking available PSRAM — this target has no PSRAM.
- Do not modify files under `components/ESP32-HUB75-MatrixPanel-I2S-DMA/` — report upstream.
- Do not add `delay()` calls in `loop()` to "fix" flickering — this will trigger the watchdog.
- Do not call `dma_display->clearScreen()` from an ISR context.

---

## After Diagnosis

If a code change is needed: dispatch to the coder agent with:
- The exact symptom
- The root cause identified above
- The specific file and line range to modify
- The constraint that must be preserved (no blocking, no DMA writes from wrong context)
