# ADR 0002 — Single Arduino loop task, no custom FreeRTOS tasks

- Status: accepted
- Date: 2026-04-17

## Context

ESP32 supports FreeRTOS tasks, two cores, queues, and notifications. Previous experiments pinned work to core 0 (display) and core 1 (network) and introduced dedicated tasks per subsystem. Result: priority inversion, display DMA starvation, MQTT reconnect storms, and hard-to-reproduce bootloops. The display I2S DMA is interrupt-driven and is very sensitive to what else is running on core 1.

## Decision

Drive everything from the Arduino `loop()` task only. No `xTaskCreate` / `xTaskCreatePinnedToCore` calls in new code. Subsystems are cooperatively scheduled from `loop()` with timestamp-based gating (the existing `autoBrightMillis`, `webServerWatchdog`, `lastAutoChangeDay` patterns).

Exceptions require an ADR that specifies: task name, core pinning, priority, stack size in bytes, what it owns, what it never touches, and how it coordinates with `loop()`.

## Consequences

- All code runs on one thread. No locks needed for shared state. Reasoning about ordering is straightforward.
- No custom code can cause priority inversion against the display DMA ISR.
- Blocking anywhere in `loop()` starves the whole system — hence [CONSTRAINTS.md](../CONSTRAINTS.md) RT-1.
- Heavy background work (file downloads, crypto) must be chunked or deferred. Today no such work exists; OTA is handled synchronously during its own HTTP request.
- If a future feature genuinely needs background work (e.g., streaming audio), we pay the ADR cost.

## Alternatives considered

- **Dedicated task per subsystem.** Tried, caused bootloops. Rejected.
- **AsyncWebServer + callbacks.** Different web stack. Large rewrite for a non-problem. Rejected.
- **Core pinning: display core 0, rest core 1.** Requires tasks to begin with. Rejected as default.
