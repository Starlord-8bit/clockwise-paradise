# Clockwise Paradise — Test Surfaces

This repo has two live device-facing test paths:

- Playwright end-to-end tests for the Web UI in `tests/e2e/`
- Hardware smoke checks in `scripts/test_hw.py` for OTA, HTTP, and optional MQTT broker coverage

## Requirements

- Node.js ≥ 18
- Device reachable on the network

## Hardware smoke test

The hardware runner lives at `scripts/test_hw.py` and is the narrow hardware validation path for OTA, HTTP, and MQTT broker smoke coverage.

MQTT smoke is optional because it needs a real broker and live device. The first pass is intentionally narrow: it proves broker connect, retained availability delivery, retained HA discovery delivery, state publication, and one safe command round-trip.

### Extra requirements for MQTT smoke

- Python package `paho-mqtt`
- A broker reachable from both the test host and the device
- A device you can safely repoint at that broker for the duration of the test

Install the optional MQTT dependency with:

```bash
python3 -m pip install paho-mqtt
```

Run the broker-backed smoke path with:

```bash
python3 scripts/test_hw.py \
	--skip-flash \
	--ip 192.168.1.50 \
	--mqtt-smoke \
	--mqtt-host 192.168.1.10
```

With broker auth:

```bash
python3 scripts/test_hw.py \
	--skip-flash \
	--ip 192.168.1.50 \
	--mqtt-smoke \
	--mqtt-host 192.168.1.10 \
	--mqtt-user clockwise \
	--mqtt-pass topsecret
```

Notes:

- The runner applies MQTT settings through the device HTTP API, restarts the device, and waits for the broker-side events.
- To avoid retained-topic false positives, the runner uses a fresh per-run topic prefix unless `--mqtt-prefix` is passed explicitly.
- The safe round-trip uses the brightness command topic, then restores the original brightness value.
- The runner does not restore prior MQTT broker settings. Use a dedicated hardware test device or rerun with your preferred MQTT config afterward.

## Setup

```bash
cd tests
npm install
npx playwright install chromium
```

## Run against a live device

```bash
# Default target: http://192.168.1.212
npm test

# Override target
CW_DEVICE_URL=http://192.168.1.50 npm test
```

## What's tested

| Suite | File |
|---|---|
| `GET /get` — settings API headers | `e2e/webui.spec.ts` |
| Home page — version, IP, nav | `e2e/webui.spec.ts` |
| Clock page — brightness, clockface | `e2e/webui.spec.ts` |
| Sync page — timezone decode, HA stub | `e2e/webui.spec.ts` |
| Hardware page — driver selector | `e2e/webui.spec.ts` |
| Update page — version badge, upload | `e2e/webui.spec.ts` |
| OTA check API — JSON response | `e2e/webui.spec.ts` |
| Legacy UI — fallback page exists | `e2e/webui.spec.ts` |

## CI

Tests run automatically via GitHub Actions on pushes and PRs.
See `.github/workflows/e2e.yml`.
