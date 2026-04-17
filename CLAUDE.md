# Clockwise Paradise — Agent Ground Truth

This file is the architectural contract for every agent working in this repository.
If something here contradicts what an agent "remembers" or guesses, this file wins.

Companion docs:
- [CONSTRAINTS.md](CONSTRAINTS.md) — hard rules the firmware reviewer enforces
- [ADR/](ADR/) — architecture decision records (why we chose X)
- [.claude/agents/](./.claude/agents/) — specialist agents and their scopes
- [.claude/skills/](./.claude/skills/) — callable slash commands

---

## What this project is

An ESP32 firmware driving a 64×64 HUB75 LED matrix. Shows clock widgets, timer, (planned) weather/stocks/messages. Configured via embedded web UI. Integrates with Home Assistant over MQTT with Discovery. OTA updates over Wi-Fi.

Upstream: fork of [jnthas/clockwise](https://github.com/jnthas/clockwise). Feature surface is closer to Tidbyt than to upstream Clockwise.

## Hardware target

- **MCU:** ESP32 (esp32doit-devkit-v1), 4MB flash, no PSRAM assumed
- **Display:** 64×64 HUB75 panel, I2S DMA driven via [ESP32-HUB75-MatrixPanel-I2S-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA) v3.0.12
- **Dev device:** fixed IP `192.168.1.212` on user's LAN, serial on `/dev/ttyUSB0` @ 115200

## Framework stack (do not drift)

- **Build system:** ESP-IDF v4.4.7 via Docker (`espressif/idf:v4.4.7`) — authoritative
- **Arduino-as-component:** `CONFIG_AUTOSTART_ARDUINO=y` — Arduino APIs (`setup()`, `loop()`, `Serial`) are available, but **ESP-IDF APIs are preferred** for new code
- **C++ standard:** C++17
- **FreeRTOS tick:** 1000 Hz (`CONFIG_FREERTOS_HZ=1000`)
- **Partition table:** `partitions.csv` (custom, OTA-capable, rollback enabled)
- **PlatformIO:** used for native tests (`test_native`) and Arduino convenience, **not for device builds** (CI and `make build` both use `idf.py`)
- **Pinned libs:** see `firmware/platformio.ini` — do not reach for a new HTTP/MQTT/WiFi library on a whim; extend existing wrappers

## Source layout

```
firmware/
  src/
    main.cpp                         ← setup()/loop() entry only; minimal logic
    app/
      core/         AppState, DisplayControl (HUB75 init, auto-bright, night)
      connectivity/ Wifi, Mqtt, HomeAssistant (Arduino/WiFi/MQTT integration)
      widgets/      ClockCore, ClockCoreChecks (widget lifecycle helpers)
      web/          WebUi (HTTP handler bindings)
  lib/
    cw-commons/     preferences, dateTime, web server/ui, MQTT, OTA,
                    WiFi controller, HA discovery, widget manager
    cw-gfx-engine/  Sprite, Tile, Object, EventBus — shared drawing primitives
    cw-logic/       pure-C++ logic extracted for native unit testing
  clockfaces/       submodules: cw-cf-0x01 (mario), cw-cf-0x02, etc.
  test/
    test_native/    CMake-driven Unity tests — run on host (no device)
    test_embedded/  device-only tests (not run by CI)
    test_unit/      PlatformIO Unity tests
components/         ESP-IDF components / third-party — DO NOT MODIFY
main/               ESP-IDF app entry (thin)
scripts/            flash.py, test_hw.py
```

Rule of thumb: **new logic that can be unit-tested goes in `cw-logic` as pure C++**. Side-effecting code stays in `src/app/` behind a thin interface.

## Concurrency model

This project runs on the **Arduino loop task only** (single-threaded `loop()` on core 1 by default under Arduino-as-component). There are no custom FreeRTOS tasks today.

**Implications for agents:**
- The display DMA is driven by a dedicated I2S interrupt — don't touch that path.
- `loop()` must stay non-blocking. A >100 ms blocking call starves the watchdog and the web server watchdog.
- All connectivity (Wi-Fi polls, MQTT loop, HTTP server, NTP events) is cooperative — each must be fast.
- Network calls from handlers must be bounded (timeouts set, retries capped).
- **Do not spawn new FreeRTOS tasks** without an ADR justifying it. If you must, specify core pinning, priority, and stack size explicitly, and document why.

See also: [.claude/skills/esp32-freertos.md](./.claude/skills/esp32-freertos.md).

## Memory discipline

- No PSRAM on this board. ~300 KB usable SRAM after Wi-Fi/BT stacks.
- **No heap allocation in the render path or any ISR.** Framebuffer writes happen at loop cadence.
- Large buffers at setup time only, and only when sized against `esp_get_free_heap_size()` expectations.
- Long-running strings must be `const` / `PROGMEM` (see `SettingsWebPage.h`).
- Watch for `String` concatenation in hot paths — prefer fixed char buffers.

## Settings / NVS

- NVS keys are **≤ 15 characters** — count before committing.
- A new setting requires all four of: struct field + default, `loadPreferences()` line, `savePreferences()` line, HTTP endpoint in `CWWebServer.h`. See `.claude/prompts/add-setting.md`.
- New UI card lives in `firmware/lib/cw-commons/web/SettingsWebPage.h` — six required fields (`title`, `description`, `formInput`, `icon`, `save`, `property`). See `.claude/skills/frontend-design.md`.

## Connectivity surface

- **HTTP server:** `ClockwiseWebServer` singleton, drained in `loop()` via `handleHttpRequest()`. See [firmware/lib/cw-commons/web/CWWebServer.h](firmware/lib/cw-commons/web/CWWebServer.h).
- **MQTT:** `CWMqtt` wrapper, `mqttLoop()` pumped from `loop()`. Prefix = `clockwise/<device-mac>` by default, availability topic with LWT.
- **HA Discovery:** entities published via `CWMqttDiscovery` — discovery prefix `homeassistant`.
- **OTA:** `CWOTA` — `POST /ota/upload` (raw `.bin`) or `POST /ota/update` (pulls from GitHub). Rollback is **bootloader-driven** (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`); `markRunningFirmwareValidAfterSuccessfulBoot()` in `main.cpp` closes the rollback window after WiFi + time come up.
- **WiFi controller:** `WiFiController` handles Improv onboarding + WiFiManager captive portal.

See: [.claude/skills/mqtt-ha-discovery.md](./.claude/skills/mqtt-ha-discovery.md).

## Widget runtime

- `CWWidgetManager` owns the active widget; `appState.widgetManager.update()` ticks from `loop()`.
- Widgets are simple state machines with `onEnter()` / `tick()` / `onExit()`-style lifecycle.
- Clock is a widget; timer is a widget (`timer:<seconds>`); weather/stocks/notification are placeholders today.
- Activation: HTTP `POST /api/widget/show?spec=<id[:arg]>`, or MQTT topic `<prefix>/<mac>/set/widget`.
- Auto-return: timer falls back to the current clockface on expiry.

See: [.claude/skills/widget-contract.md](./.claude/skills/widget-contract.md).

## Build, test, flash — authoritative commands

| Task | Command | Notes |
|------|---------|-------|
| Native tests | `make test` | Host-only, no device needed |
| Firmware build | `make build` | Dockerized ESP-IDF, matches CI |
| CI gate (local) | `make check` | cppcheck-ci + test + build |
| USB flash | `make flash PORT=/dev/ttyUSB0` | First flash only |
| OTA flash | `make flash-ota` | Uses `DEVICE_IP` from `.env` |
| HW integration | `make test-hw` | Build + OTA + smoke |
| Cppcheck (full) | `make cppcheck` | Informational report |

Agent-callable equivalents: `/build`, `/test`, `/flash`, `/test-hw`, `/diagnose`.

## Agentic workflow (who does what)

```
user ──▶ pm (intake, triage, task spec) ──▶ github-specialist (branch setup)
              │
              ├──▶ coder              (generic firmware changes)
              ├──▶ firmware-rtos      (boot, tasks, memory, ISR, watchdog)
              ├──▶ display-render     (HUB75, DMA, framebuffer, brightness)
              ├──▶ widget-author      (widget lifecycle, widget manager)
              ├──▶ connectivity       (WiFi, MQTT, HA discovery, OTA)
              └──▶ frontend           (HTML/JS/CSS inside SettingsWebPage.h)
                        │
                        ▼
              reviewer ──▶ reviewer-firmware OR reviewer-frontend
                        │
                        ▼ (OK)
              github-specialist (commit, push, PR)
```

- **`pm` is the only agent the user should need to talk to.** It refuses to write code; it clarifies, triages, writes the task contract, and dispatches.
- Specialists (`firmware-rtos`, `display-render`, `widget-author`, `connectivity`) are opt-in when the PM decides the domain-specific checklist matters. For small, uncontroversial firmware changes the PM may dispatch `coder` directly.
- Reviewer routing is by `type:` on the Handoff Contract (`firmware` or `frontend`).
- Max 3 iterations per specialist → reviewer loop. On the 3rd NOK, specialist escalates to PM, PM escalates to user.

## When an agent should ask the user (via PM)

- Hardware anomalies reported anecdotally — get a repro first.
- Any new FreeRTOS task.
- Any new top-level dependency (new lib, new CDN, new submodule).
- Any change to `components/` or submodules under `firmware/clockfaces/`.
- Any change that touches flash partitions, NVS key renames, or OTA layout.
- Anything that would reduce test coverage.

## Release process

Automated via release-please. Conventional Commit titles on PRs drive version bumps. Merge the release PR → tag is created → `release.yml` builds and publishes. See [.claude/skills/git-go-for-release.md](./.claude/skills/git-go-for-release.md). Never pre-create tags; never hand-edit `CHANGELOG.md`.

## Where knowledge lives

| Question | Where to look |
|---|---|
| "Is this pattern allowed?" | [CONSTRAINTS.md](CONSTRAINTS.md) |
| "Why did we choose X?" | [ADR/](ADR/) |
| "How do I add a setting?" | [.claude/prompts/add-setting.md](./.claude/prompts/add-setting.md) |
| "Display is glitching" | [.claude/prompts/debug-display.md](./.claude/prompts/debug-display.md) |
| "How does the widget runtime work?" | [.claude/skills/widget-contract.md](./.claude/skills/widget-contract.md) |
| "HUB75 DMA footguns" | [.claude/skills/hub75-rendering.md](./.claude/skills/hub75-rendering.md) |
| "MQTT entity conventions" | [.claude/skills/mqtt-ha-discovery.md](./.claude/skills/mqtt-ha-discovery.md) |
| "Boot loop triage" | [.claude/skills/boot-diagnose.md](./.claude/skills/boot-diagnose.md) |
| "Where are the tests?" | `firmware/test/test_native/` |
| "Build and flash" | Makefile + [.claude/skills/build.md](./.claude/skills/build.md) + [.claude/skills/flash.md](./.claude/skills/flash.md) |
