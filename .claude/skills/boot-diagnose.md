---
description: Systematic boot-loop and sporadic-boot triage for Clockwise Paradise. Enumerates reset causes, log signatures, the bootloader rollback window, and the order of checks to find root cause instead of patching symptoms. Use /boot-diagnose when the device boots inconsistently, crashes during setup, or rolls back unexpectedly.
---

# Boot Diagnose — Clockwise Paradise Edition

Boot sequence (authoritative, from [firmware/src/main.cpp](../../firmware/src/main.cpp)):

1. `Serial.begin(115200)` + log-level mutes
2. Built-in LED pinMode + initial blink
3. `ClockwiseParams::getInstance()->load()` — NVS read
4. LDR pin configured
5. `preconnectWifiBeforeDisplay(p)` — kicks WiFi before the display buffers eat RAM
6. `displaySetup(...)` — constructs `MatrixPanel_I2S_DMA`, calls `begin()`
7. `Locator::provide(appState.display)`
8. `displayBootSanityTest(...)` — R/G/B stripes for ~900 ms
9. `configureWidgetManager(appState, p)` + `bindWebUiCallbacks(appState)`
10. Brightness init (method-dependent)
11. Status controller logo + 1 s delay
12. WiFi connect + NTP time — blocks here until success or forced restart
13. `markRunningFirmwareValidAfterSuccessfulBoot()` — closes the OTA rollback window
14. `activateStartupWidget(...)`
15. HA + MQTT start
16. Boot banner printed

A crash or hard reset **before step 13** leaves the rollback window open. Three such crashes cause the bootloader to swap OTA partitions and try the previous firmware.

## Reset reasons (from `rst:0x...` in serial)

| Code | Meaning |
|---|---|
| `0x1` | Power-on reset — cold start |
| `0x3` | External reset (RESET button) |
| `0x5` | Deep-sleep wake |
| `0xc` | Software reset (OTA finish, `ESP.restart`) |
| `0xe` | Brownout — check PSU amperage, HUB75 draws a lot |
| `0xf` | Task watchdog timeout — see CONSTRAINTS RT-1 |
| `0x10` | RTC watchdog |

## Common crash signatures

### "Guru Meditation Error" with backtrace
- CPU exception (null deref, unaligned, stack corruption). Decode the backtrace with `addr2line` against `build/compile/clockwise-paradise.elf`. The topmost frame inside project code is the culprit 90 % of the time.

### "Task watchdog got triggered. (CPU X)"
- The task named in the log blocked for > 5 s. On CPU 1 this is almost always the Arduino loop task — find the blocking call ([CONSTRAINTS RT-1](../../CONSTRAINTS.md)).

### "E (N) esp_image: ..." during early boot
- Bootloader failed to verify the app image. Often a partial flash. Re-flash from USB.

### "Brownout detector was triggered"
- Power supply cannot sustain HUB75 current spikes. Symptom: crash under white/bright content. Fix: bigger PSU (≥ 5 V @ 4 A for full-white on a 64×64). Not a firmware bug.

### "assert failed: ... nvs_api.cpp"
- NVS partition corruption or mismatched partition table. Check `partitions.csv` hasn't changed. If it did, user may need `idf.py erase_flash`.

### "CORRUPT HEAP"
- Double free, write past a buffer, or allocation from an ISR. Look for recent changes that added `String` work in a tick or allocation in an ISR. See CONSTRAINTS RT-2.

### Boots clean but web server dead after N minutes
- `webServerWatchdog` design makes HTTP serve recover every 5 min. If the watchdog itself blocks or loops back quickly, something is starving `handleHttpRequest`. Walk back through `loop()` and find a call that sometimes exceeds 100 ms.

### Device "flaps" online/offline in HA
- LWT firing. WiFi is reconnecting, or MQTT `loop()` is not getting pumped. Check whether anything in `loop()` blocks for > keepalive.

## Rollback window

- If the device does not reach `markRunningFirmwareValidAfterSuccessfulBoot()` within 3 boots, the bootloader swaps to the previous partition.
- Symptom: "I flashed new firmware but the old version came back." This is the rollback working correctly.
- Check `GET /ota/status` → `running_version`, `other_version`, `running_state`. `valid` = mark-valid called; `new` = still in rollback window.
- Do not try to fix "mysterious rollback" by disabling `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` — fix the crash that triggered it.

## Order of triage

1. Capture serial log from cold boot — the first error is usually the root cause.
2. Check reset reason. `0xf` → task watchdog. `0xe` → power. `0xc` → software (OTA or explicit restart). `0x1` → cold start (no info on its own).
3. Check partition state via `/ota/status` if reachable. Rolled back? — you have a post-mark-valid crash on the candidate image.
4. Correlate serial timestamps with boot phases (see the list at top). Which phase did not complete?
5. Bisect: was the last known-good build flashed recently? `git log` the touched area since then.
6. Rule out hardware: brownout under bright content, loose HUB75 ribbon, cold solder on E pin.
7. Reach for the specialist: RT-1/RT-2 issue → `firmware-rtos`; DMA symptom → `display-render`; MQTT/WiFi flake → `connectivity`.

## What not to do

- Do not disable the task watchdog to "stop the reset." You are hiding the starvation.
- Do not extend the rollback window to "give the board more time." You are defeating a safety net.
- Do not increase `CONFIG_ESP_MAIN_TASK_STACK_SIZE` without understanding why. Stack overflows usually mean a recursive function, not a legitimately deep stack.
- Do not flash an older firmware to "get it working" without filing what regressed. Bisect.

## Related

- `/diagnose` — runtime device state verification
- `/test-hw` — build + OTA + smoke
- [CONSTRAINTS.md](../../CONSTRAINTS.md) — the rules whose violation causes most boot problems
