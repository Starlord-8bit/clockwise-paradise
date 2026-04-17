# Firmware Constraints — Non-negotiable

This is the authoritative list of rules that produce bootloops, flaky connections, memory corruption, or silent data loss when violated. The `reviewer-firmware` agent enforces this file as hard rejections. These rules are not stylistic preferences.

Every rule has a **Why** — the specific failure mode. If an agent can prove the Why does not apply (rare, and must be justified in the Handoff Contract), the rule is still in force until the PM and user sign off on an ADR.

---

## RT-1 — No blocking calls in `loop()` or HTTP handlers

**Rule.** Any call in `loop()`, in an HTTP handler, or in an MQTT callback must return in < 100 ms. `delay()` > 5 ms is forbidden in `loop()` (`delay(1)` at end of loop is fine — it yields to IDLE). Synchronous network calls (blocking HTTP clients, DNS lookups without timeout, TLS handshakes) are forbidden in handlers.

**Why.** This project runs on the Arduino loop task only. Blocking in `loop()` starves `handleHttpRequest()`, `mqttLoop()`, and `webServerWatchdog()`. Symptoms: web UI freezes, MQTT disconnects with LWT, device reboots after 5 min when the watchdog triggers.

**Instead.** Use short polls, state machines, or schedule via timestamp comparisons. For one-shot network calls use bounded timeouts. If a call legitimately needs >100 ms, open an ADR.

---

## RT-2 — No heap allocation in the render path or ISR

**Rule.** Within widget `tick()` / render code / any function reachable from an I2S DMA ISR: no `malloc`, no `new`, no `std::vector::push_back` that may grow, no `String` concatenation in a loop body, no `std::stringstream`. Pre-allocate at setup time or use stack-local fixed buffers.

**Why.** Heap fragmentation on ESP32 kills long-running devices. A 16-byte allocation in the render path, 60 fps, for 24 hours, fragments the heap into uselessness. ISR-context allocation is worse — it calls into a non-ISR-safe allocator and crashes.

**Instead.** Fixed-size `char` buffers, static arrays, or pre-allocated pools in setup.

---

## RT-3 — No new FreeRTOS tasks without an ADR

**Rule.** Do not call `xTaskCreate` / `xTaskCreatePinnedToCore` in new code. If you believe a task is necessary, stop and write an ADR proposing: task name, core pinning, priority, stack size (bytes), what it owns, what it must not touch, how it coordinates with `loop()`.

**Why.** The project has zero custom tasks today. Priority inversion, starving the display DMA, or racing the HTTP server are easy to cause and hard to diagnose. Bootloops after `v3.0.0` refactors were traced to misbehaving tasks.

**Instead.** Pump work from `loop()` with timestamp gating, or use `Ticker` / `ezt::events` if they already fit.

---

## RT-4 — NVS keys are ≤ 15 characters

**Rule.** Every key passed to `nvs.getXxx(...)` / `nvs.putXxx(...)` has a length ≤ 15 characters (not counting null terminator). Count manually. Match the key in both `loadPreferences()` and `savePreferences()`.

**Why.** ESP-IDF NVS truncates silently at 15 bytes. A 16-char key written and a 15-char key read resolve to *different* NVS entries. Symptom: setting "doesn't save" — actually, it saves to a different key than the one being read back.

**Instead.** `snake_case` with aggressive abbreviations. `night_start_h` is fine, `night_schedule_start_hour` is not.

---

## RT-5 — No `Serial.print*` in production code

**Rule.** New code uses `ESP_LOGI/LOGW/LOGE/LOGD` with a module `TAG`. The only permitted `Serial.print*` is the explicit boot banner in `main.cpp` and Improv WiFi handshake (Arduino lib requirement).

**Why.** `ESP_LOG*` respects log levels (compiled-out in release), adds timestamps, is structured. `Serial.print` is always-on and muddies the serial log the user relies on for diagnostics.

**Instead.**
```cpp
static const char* TAG = "MyModule";
ESP_LOGI(TAG, "Value: %d", value);
```

---

## RT-6 — No undisclosed file changes

**Rule.** The Handoff Contract's `files changed` list must match exactly what `git diff` shows. No "drive-by cleanups" in unrelated files. No touching `components/`, `firmware/clockfaces/`, or any git submodule.

**Why.** Undisclosed changes bypass the reviewer's spec cross-check and frequently introduce regressions (the spaghetti problem). Submodule changes break the build for everyone.

**Instead.** If cleanup is needed, open a separate task.

---

## RT-7 — One source of truth for display init

**Rule.** `displaySetup()` in `firmware/src/app/core/DisplayControl.cpp` is the only place that constructs `MatrixPanel_I2S_DMA`. No other module may call `new MatrixPanel_I2S_DMA(...)` or `display->begin()`.

**Why.** Double-initialisation of the I2S DMA corrupts the framebuffer and may crash the CPU. Any module that needs display access takes a pointer from `AppState` or `Locator`.

---

## RT-8 — OTA rollback window

**Rule.** Do not remove or delay `markRunningFirmwareValidAfterSuccessfulBoot()` in `main.cpp`. Do not mark the image valid *before* WiFi and time have come up. Do not mark it valid from inside a web handler.

**Why.** The bootloader auto-rolls back if the image is not marked valid within three consecutive boots. Marking valid too early defeats the safety net (a crash after boot but before service becomes persistent). Marking it inside a handler races with the watchdog reboot path.

---

## RT-9 — Web UI raw-string delimiter

**Rule.** The `SETTINGS_PAGE` constant uses `R""""(...)""""` — four quotes. Never use `"""` (three quotes) anywhere inside the HTML/JS/CSS. Use `&quot;` or single quotes inside attributes.

**Why.** Three quotes match the closing delimiter and truncate the embedded page silently. Symptom: device serves a broken UI; the browser may show a blank page or syntax error.

---

## RT-10 — No `innerHTML` from device/API values

**Rule.** In `SettingsWebPage.h` / `CWWebUI.h`: never assign `innerHTML = <value-from-API-or-user-input>`. Use `textContent` or build DOM nodes. Never bind event handlers via string (`setAttribute('onclick', ...)`, `eval`, `new Function`, string-based `setTimeout`).

**Why.** The settings UI is reachable by anything on the LAN. XSS here is a device-takeover primitive.

---

## RT-11 — No new CDN dependencies or `<style>` blocks in the UI

**Rule.** Only W3.CSS and FontAwesome. No new `<style>` blocks — use W3.CSS utility classes.

**Why.** The UI must work offline. Each CDN is a failure point and a privacy leak. `<style>` blocks fragment the design system and make future refactors painful.

---

## RT-12 — UI card schema is fixed

**Rule.** Every card in `createCards()` has exactly the six fields `title`, `description`, `formInput`, `icon`, `save`, `property`. The `id` in `formInput` matches the identifier in `save`. The `property` matches the JSON key from `CWWebServer.h` verbatim.

**Why.** Any drift and the setting silently fails to save, or the card fails to render.

---

## RT-13 — No skunk work

**Rule.** Committed code contains no commented-out code blocks, no dead variables, no unused `#include`s, no `TODO` comments. If you want to leave a marker for future work, open an issue.

**Why.** Spaghetti starts with "I'll clean it up later."

---

## RT-14 — Every unit-testable change ships with a test

**Rule.** Any change to logic inside `firmware/lib/cw-logic/` or pure-C++ helpers must add or update a Unity test in `firmware/test/test_native/`. Tests that trivially pass (`TEST_ASSERT_TRUE(true)`, empty body, asserting that a variable was assigned) are a hard rejection.

**Why.** The spaghetti problem is usually the absence of a test that would have caught the drift. Native tests run in < 1 s — there is no excuse.

---

## RT-15 — Version discipline

**Rule.** `version.txt` and the `CW_FW_VERSION` define in `firmware/platformio.ini` match. Do not bump versions by hand in a feature PR — release-please owns version bumps.

**Why.** HA discovery uses firmware version to gate entities; a mismatched version causes the integration to hide or duplicate entities.

---

## RT-16 — No credentials in logs, topics, or GET responses

**Rule.** WiFi password, MQTT broker password, OTA token, and any other secret stored in NVS must never appear in: `ESP_LOG*` output, any MQTT topic payload, any HA Discovery payload, or any HTTP GET response body. Secrets are write-only at runtime.

**Why.** The settings UI and MQTT broker are reachable by anything on the LAN. Publishing a credential on a retained topic or in a discovery payload exposes it permanently to any subscriber, including future devices.

**Instead.** GET endpoints return the value as `"****"` or omit the field entirely. Log the presence of a credential (`"broker password set"`) but never its value.

---

## Rule additions

Reviewers may propose new rules in NOK verdicts (`## Reviewer Note`). The PM turns those into a proposed amendment to this file and asks the user before merging. Reviewer patterns without a documented rule here are advisory, not hard rejections.
