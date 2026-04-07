---
description: Diagnose and verify the device after a flash. Default path is web-based (Playwright against 192.168.1.212). Serial path tails the rackpeek tmux session (/dev/ttyUSB0 @ 115200) for ESP_LOGE / panic / watchdog output. Use /diagnose after /flash, or standalone when investigating a fault.
---

Diagnose the Clockwise Paradise device at 192.168.1.212.

## Device Info

- **Fixed IP**: `192.168.1.212`
- **Web UI**: `http://192.168.1.212/`
- **Serial port**: `/dev/ttyUSB0` at 115200 baud
- **tmux session**: `rackpeek` (serial monitor window — do not destroy it)

---

## Path A: Web Verification (default — run first)

Uses Playwright to check the device is serving the web UI and that API endpoints are healthy.

### Step 1 — Basic reachability
```bash
curl -s --max-time 5 http://192.168.1.212/ -o /dev/null -w "HTTP %{http_code}\n"
```
If not 200: skip to Path B immediately.

### Step 2 — Web UI smoke test (Playwright)
```bash
playwright screenshot http://192.168.1.212/ --full-page /tmp/cw-ui.png && echo "Screenshot saved to /tmp/cw-ui.png"
```

Read the screenshot to visually confirm the UI loaded correctly (no blank page, no error state).

### Step 3 — API endpoint checks
Query the settings API to verify preferences loaded from NVS:
```bash
curl -s http://192.168.1.212/api/prefs | python3 -m json.tool
```
```bash
curl -s http://192.168.1.212/api/status | python3 -m json.tool
```

Compare returned values against expected values from the task context (e.g., if a setting was just changed, verify the new value is present). Report any discrepancy as a failure.

If the API returns 404, the endpoint may not exist yet — note this and fall back to the web UI screenshot.

### Step 4 — Report web result
- **PASS**: UI loaded + API returned valid JSON with expected values.
- **PARTIAL**: UI loaded but API missing or values unexpected — investigate in serial path.
- **FAIL**: UI unreachable — proceed to Path B.

---

## Path B: Serial Diagnosis (fault tracing)

Use when: device is unreachable, boot looping, crashing before web server starts, or when validating low-level behavior (DMA, FreeRTOS, NVS writes).

### Step 1 — Check if rackpeek session has a picocom window

```bash
tmux list-windows -t rackpeek
```

If a picocom window already exists, capture its buffer non-destructively:
```bash
tmux capture-pane -t rackpeek -p -S -500
```
Do NOT send keystrokes or interrupt the session.

### Step 2 — If no serial session running, start one in a new window
```bash
tmux new-window -t rackpeek -n serial "picocom -b 115200 /dev/ttyUSB0"
```

Then capture output after ~10 seconds:
```bash
sleep 10 && tmux capture-pane -t rackpeek:serial -p -S -200
```

### Step 3 — Parse serial output for known fault patterns

| Pattern | Meaning |
|---------|---------|
| `[OTA-Upload] Starting, expecting N bytes` | OTA upload received by device |
| `[OTA-Upload] Success — rebooting` | OTA write completed cleanly |
| `Guru Meditation Error` | CPU exception / crash — read the backtrace |
| `Task watchdog got triggered` | Main task blocking — look for blocking call or long computation |
| `ESP_ERROR_CHECK failed` | Fatal API error — read the expression and error code |
| `nvs: NVS key too long` | NVS key >15 chars — check CWPreferences.h |
| `E (...)` | Any ESP_LOGE output — report tag and message |
| `W (...)` | ESP_LOGW warnings — report if frequent |
| `rst:0x...` | Reset reason — `0x1` = power-on, `0xc` = SW reset (OTA), `0xf` = watchdog |

### Step 4 — Report serial result

For each fault found: report the pattern, the exact log line, and the probable cause with a reference to the source file if known.

---

## Combined Verdict

| Web | Serial | Verdict |
|-----|--------|---------|
| PASS | PASS or N/A | **OK** — deployment healthy |
| PASS | FAIL | **WARN** — errors present but device serving; may be non-fatal |
| FAIL | PASS (clean boot) | **FAIL** — device up but web server broken |
| FAIL | FAIL (crash/panic) | **FAIL** — device not booting; report backtrace |
| FAIL | FAIL (no output) | **FAIL** — device not responding at all; check power and cable |

Always end with: "Run `/flash` to redeploy or share the serial output for further diagnosis."
