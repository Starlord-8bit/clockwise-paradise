---
description: Build firmware, OTA-flash to the test device, confirm boot, verify version, run hardware checks. Use /test-hw before committing any firmware change to ensure the device survives the update.
---

Hardware integration test for clockwise-paradise. Compiles the firmware, flashes it to the test device via OTA, confirms the device boots cleanly, and verifies the correct version is running.

**Goal:** catch regressions before they reach git. Never push code that bricks the test device.

---

## Pre-flight

1. Confirm `.env` exists with `DEVICE_IP` set:
```bash
cat .env
```
If missing, tell the user: "Copy `.env.example` to `.env` and set `DEVICE_IP` to your device's IP address."

2. Confirm the device is reachable:
```bash
curl -s --max-time 5 "http://$DEVICE_IP/" -o /dev/null -w "HTTP %{http_code}\n"
```
If not reachable (non-200): stop and tell the user — the device must be online before proceeding.

---

## Step 1: Build

```bash
make build 2>&1 | tail -60
```

- If build fails: extract the first error with file:line reference and report it. Do NOT proceed.
- If build succeeds: note the binary location and version tag.

---

## Step 2: Run hardware tests

```bash
make test-hw 2>&1
```

This runs `scripts/test_hw.py` which:
- OTA-uploads the firmware to `DEVICE_IP`
- Waits for the device to reboot (up to 90s)
- Polls `GET /ota/status` until `running_state=valid` and the new version is confirmed
- Checks `X-CW_FW_VERSION` header via `GET /get`
- Checks web UI is reachable (`GET /`)
- Runs any test modules in `tests/hardware/*.py`

If serial monitoring is available, run instead:
```bash
make test-hw PORT=/dev/ttyUSB0 2>&1
```
This additionally confirms `[OTA] Firmware marked valid` on serial output.

---

## Step 3: Interpret results

**If PASSED:**
- Report: "Hardware tests passed — N/N checks OK. Safe to commit."
- The device is running the new firmware and self-reported as valid.

**If FAILED — version mismatch:**
- The device booted but is running a different version than expected.
- The OTA may have auto-rolled back (ESP32 rollback window). Check:
```bash
curl -s "http://$DEVICE_IP/ota/status" | python3 -m json.tool
```
- Report the `running_version` and `other_version` fields so the user understands the partition state.
- Do NOT commit.

**If FAILED — device did not come back:**
- The device may be in a boot loop (the bootloader will auto-rollback after 3 failed boots).
- Wait 3–4 minutes and retry the checks only:
```bash
python3 scripts/test_hw.py --skip-flash 2>&1
```
- If still unreachable, escalate: USB serial flash is needed. Run `/flash` for recovery.
- Do NOT commit.

**If FAILED — OTA upload rejected:**
- Check the device is not mid-update or in a bad state.
- Restart the device via `POST /restart` and retry once:
```bash
curl -s -X POST "http://$DEVICE_IP/restart"
sleep 15
make test-hw 2>&1
```
- Do NOT commit on persistent failure.

---

## Step 4: Hardware plugin tests

If `tests/hardware/*.py` files exist, `make test-hw` runs them automatically. Each file exports:
```python
def run_tests(ip: str, version: str, result) -> None:
    # result.ok("description", "detail")
    # result.fail("description", "detail")
```

To add a new feature test:
- Create `tests/hardware/test_<feature>.py`
- Use `GET /get` headers, `POST /set` params, or any device HTTP endpoint
- Example — verify brightness setting round-trips:
  ```python
  def run_tests(ip, version, result):
      import urllib.request, json
      # Set brightness to 42
      urllib.request.urlopen(f"http://{ip}/set?key=displayBright&value=42", data=b"")
      # Read it back
      with urllib.request.urlopen(f"http://{ip}/get") as r:
          val = dict(r.headers).get("x-displaybright", "")
      if val == "42":
          result.ok("brightness round-trip", "42")
      else:
          result.fail("brightness round-trip", f"got {val!r}")
  ```

---

## Rollback reference

| Situation | Action |
|-----------|--------|
| Device came back, wrong version | `GET /ota/status` to inspect partitions |
| Device in boot loop (3 attempts) | Wait — ESP32 will auto-rollback to previous partition |
| Manual rollback needed | `POST /ota/rollback` then confirm boot |
| Device completely unresponsive | USB flash via `/flash` (serial method) |

---

## Notes

- `make test-hw` runs `make build` first — it always flashes the freshly-compiled binary, not a stale artifact.
- The test device is your own hardware; it is acceptable if it gets bricked during testing. Other devices are never touched.
- Version baked into firmware = nearest git tag (strip `v`), set by `main/CMakeLists.txt` at build time.
- OTA state `"valid"` means `esp_ota_mark_app_valid_cancel_rollback()` was called — the device completed `setup()` successfully.
