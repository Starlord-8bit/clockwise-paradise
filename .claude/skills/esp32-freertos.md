---
description: Reference for ESP32 / FreeRTOS rules as they apply to Clockwise Paradise. Covers the single-loop model, why new tasks are forbidden without an ADR, task priorities / core pinning if one is eventually approved, stack sizing, watchdog discipline, and blocking patterns that cause bootloops.
---

# ESP32 / FreeRTOS — Clockwise Paradise Edition

Authoritative ADR: [ADR/0002-single-loop-concurrency.md](../../ADR/0002-single-loop-concurrency.md).
Authoritative rules: [CONSTRAINTS.md](../../CONSTRAINTS.md) RT-1, RT-2, RT-3.

## The rule in one line

**Everything runs from the Arduino `loop()` task.** No custom FreeRTOS tasks in new code without an ADR.

## Why

- The HUB75 I2S DMA is interrupt-driven and sensitive to what else runs on core 1.
- Past tasking experiments produced priority inversion, DMA starvation, and bootloops that took days to diagnose.
- Single-thread reasoning: no locks, no races, no "who owns this buffer." Cheap to reason about.

## Loop cadence

Typical `loop()` round on a healthy boot is sub-10 ms. The whole pipeline is:
```
loop() {
  improvWiFi handle
  if WiFi up: HTTP drain, ezt events, webServerWatchdog, mqttLoop
  widgetManager.update()
  if WiFi once: uptimeCheck, autoChangeCheck
  automaticBrightControl
  nightModeCheck
  delay(1)   // yields IDLE1 — required, do not remove
}
```
Any step that takes > 100 ms starves the others and eventually trips the task watchdog or the `webServerWatchdog` (reboots after 5 min of unresponsive HTTP).

## Things that look fine and aren't

| Pattern | Why it hurts |
|---|---|
| `delay(100)` somewhere "just to settle" | Blocks the whole loop; watchdog fires |
| `while (!client.available()) {}` | Same |
| `String s; for (int i=0; i<N; i++) s += x[i];` in a tick | Heap fragmentation (RT-2) |
| `HTTPClient.GET()` with no timeout | Default timeout is seconds; handler hangs |
| `WiFi.reconnect()` inside a handler | Kicks WiFi stack from the wrong context |
| "Just a small 1 ms poll loop" | Not small after 100 iterations |

## If you believe you need a task

Open an ADR. The ADR must answer:

- Name. `TAG` for logs.
- Core: 0 or 1. Rationale. Default is do not pin.
- Priority. Must be ≤ `tskIDLE_PRIORITY + 2`. Higher priorities have starved the I2S DMA in practice. Specify why the priority chosen is correct.
- Stack size in bytes. Start at 4096; justify if larger. Too small crashes the task; too large eats RAM.
- What the task owns. What it never touches (display, WiFi stack internals, NVS concurrent writes).
- How it receives work from `loop()`. Preferred: queue with bounded length + non-blocking send from `loop()`.
- How it yields. `vTaskDelay(pdMS_TO_TICKS(...))` in idle path; never `delay()`.

## Watchdog

- Task watchdog timeout: default. The loop task is subscribed to the TWDT.
- If you add a task that does long work, it must call `esp_task_wdt_reset()` inside the long loop (and be subscribed with `esp_task_wdt_add`).
- If you see `Task watchdog got triggered` on CPU 1 → the loop task blocked. Find the blocker. Do not increase the timeout.

## Stack sizing rough guide

- Arduino loop task: default (8KB). Do not grow arbitrary stack locals beyond ~2 KB per function.
- Any user task: start 4096 bytes. Measure with `uxTaskGetStackHighWaterMark` before trimming.
- ISR: stack locals only, small, no recursion, no printf.

## Memory budget

- Start of day: ~300 KB free after WiFi+BT + HUB75 DMA buffers.
- After MQTT+HA Discovery: ~200 KB free is a healthy steady state.
- Dropping below 80 KB free is a problem — look for string concatenation in hot paths, unreleased `HTTPClient` sessions, or buffered widget state that never resets.

## Heap anti-patterns to reject on sight

- `String` concatenation inside a per-tick function.
- `std::vector` that is `.push_back`'d in a hot path.
- Allocating a buffer inside an ISR or anywhere reachable from one.
- Copying JSON into and out of an Arduino `String` multiple times per request.

## The one legitimate `delay(1)`

`loop()` ends with `delay(1)`. This yields the IDLE1 task so the watchdog gets fed and the power-saving state runs correctly. On ESP-IDF v4.4 `vTaskDelay(0)` does not yield reliably — that is why this pattern uses `delay(1)`. Do not remove it.

## When in doubt

Do the cooperative-scheduling version first. Measure. If it actually starves something, open an ADR.
