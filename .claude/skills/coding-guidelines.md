---
description: Reference for Clockwise Paradise C++ firmware coding conventions. Use /coding-guidelines when writing or reviewing ESP-IDF/Arduino firmware code for this project.
---

# Clockwise Paradise — Firmware Coding Guidelines

## Language & Standard

- **C++17** throughout. Use modern features where they improve clarity but not at the
  cost of RAM or flash (this is an embedded target with ~300 KB usable RAM).
- ESP-IDF v4.4.x APIs are authoritative. Arduino convenience wrappers are acceptable in
  non-ISR code but prefer ESP-IDF for anything timing-critical.

---

## File Organization

- **Header-only libraries** go in `firmware/lib/`. Each library is a single `.h` file
  or a `.h`/`.cpp` pair. No subdirectories inside `firmware/lib/` except existing ones.
- **Source files** go in `firmware/src/`.
- **Component submodules** go in `components/` (ESP-IDF) or `firmware/clockfaces/` (clockfaces).
  Never modify files inside `components/` or `firmware/clockfaces/` — they are third-party.

---

## Naming

- Classes: `PascalCase` (e.g., `CWWebServer`, `StatusController`)
- Methods and functions: `camelCase` (e.g., `handleOtaUpload`, `loadPreferences`)
- Member variables: `camelCase`, no prefix (e.g., `brightness`, `nightMode`)
- Constants and `#define`: `ALL_CAPS_SNAKE` (e.g., `MAX_BRIGHTNESS`, `NVS_NAMESPACE`)
- NVS keys: `snake_case`, **≤ 15 characters** — count before committing

---

## Logging

Always use ESP-IDF logging macros. Never use `Serial.print` in production code.

```cpp
static const char* TAG = "MyModule";

ESP_LOGI(TAG, "Value: %d", value);      // informational
ESP_LOGW(TAG, "Unexpected: %s", msg);   // warning — non-fatal
ESP_LOGE(TAG, "Failed: %d", err);       // error
```

Log levels in production builds: INFO and above. DEBUG logs are stripped in release.

---

## Memory Rules

| Context | Allowed | Forbidden |
|---------|---------|-----------|
| Display loop / ISR | Stack locals, static arrays | `malloc`, `new`, `std::vector` growth |
| Setup / one-time init | Dynamic allocation OK | Unbounded allocation |
| FreeRTOS tasks | Stack locals up to stack size | Stack arrays >512 bytes without justification |
| HTTP handlers | `String` is acceptable | Large heap allocations without free |

When adding a FreeRTOS task, always specify stack size explicitly and add a comment
explaining the sizing rationale.

---

## Blocking Calls

The main FreeRTOS task runs `loop()`. **No blocking calls allowed here.**

A call is blocking if it may take > 100 ms without returning (RT-1). Examples:
- `delay(> 5)` — forbidden in `loop()`. `delay(1)` at end of loop is fine (yields to IDLE).
- Synchronous HTTP requests without a timeout — must set `http.setTimeout(5000)` or equivalent
- `while (!x)` polling loops — restructure as a state machine with timestamp gating
- NVS read inside a handler — cache at startup, read from the cached struct in loop

---

## Settings Pattern (NVS)

Every new device setting requires **all four** of these (see also [CONSTRAINTS.md RT-4](../../CONSTRAINTS.md) for the NVS key length rule):

```cpp
// 1. Struct field in CWPreferences.h
struct Preferences {
    int myNewSetting = 42;  // default value here
};

// 2. Load in loadPreferences()
prefs.myNewSetting = nvs.getInt("my_new_key", 42);  // key ≤ 15 chars — count manually

// 3. Save in savePreferences()
nvs.putInt("my_new_key", prefs.myNewSetting);

// 4. HTTP endpoint in CWWebServer.h
// GET  /api/myNewSetting  → returns current value
// POST /api/myNewSetting  → accepts new value, saves, returns {"status":"ok"}
```

For a complete end-to-end template use [.claude/prompts/add-setting.md](../prompts/add-setting.md).

---

## HTTP Endpoint Pattern

Follow the existing pattern in `CWWebServer.h`:

```cpp
// GET handler
if (method == "GET" && path == "/api/myEndpoint") {
    String json = "{\"value\":" + String(prefs.myValue) + "}";
    sendJson(client, json);
    return;
}

// POST handler
if (method == "POST" && path == "/api/myEndpoint") {
    String body = readBody(client);
    prefs.myValue = body.toInt();
    savePreferences();
    sendJson(client, "{\"status\":\"ok\"}");
    return;
}
```

---

## What Never Goes In a Commit

- Commented-out code blocks
- Dead variables (declared, never read)
- `TODO` comments
- `Serial.print` / `Serial.println`
- `#include` directives for headers not used in the file
- Magic numbers without a named constant or explanatory comment
- NVS keys longer than 15 characters
