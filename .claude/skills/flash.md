---
description: Flash the firmware to the device. Primary method is OTA upload to 192.168.1.212 (preferred — no cable needed). Fallback is direct serial flash via esptool on /dev/ttyUSB0. Use /flash after /build to deploy autonomously.
---

Flash Clockwise Paradise firmware to the device at 192.168.1.212.

## Device Info

- **Fixed IP**: `192.168.1.212`
- **OTA endpoint**: `POST /ota/upload` (Content-Type: application/octet-stream)
- **Serial port**: `/dev/ttyUSB0` at 115200 baud
- **App binary** (OTA): `build/clockwise-paradise.bin`
- **Response on success**: `{"status":"ok","message":"Upload complete, rebooting"}`

---

## Method 1: OTA (preferred)

### Step 1 — Confirm binary exists
```bash
ls -lh build/clockwise-paradise.bin
```
If missing, tell the user to run `/build` first and stop.

### Step 2 — Check device is reachable
```bash
curl -s --max-time 5 http://192.168.1.212/ -o /dev/null -w "%{http_code}"
```
If not reachable (non-200 or timeout): warn the user and offer to fall back to serial flash.

### Step 3 — Upload via OTA
```bash
curl -X POST http://192.168.1.212/ota/upload \
     -H 'Content-Type: application/octet-stream' \
     --data-binary @build/clockwise-paradise.bin \
     --max-time 120 \
     -w "\nHTTP %{http_code}\n"
```
- Expect `{"status":"ok",...}` and HTTP 200.
- On error response or non-200: report the error message from JSON and stop — do NOT fall back to serial automatically (flashing wrong partition could brick the device).

### Step 4 — Wait for reboot and confirm recovery
After a successful OTA response the device reboots automatically. Wait for it to come back:
```bash
for i in $(seq 1 12); do
  sleep 5
  code=$(curl -s --max-time 3 http://192.168.1.212/ -o /dev/null -w "%{http_code}" 2>/dev/null)
  echo "Attempt $i: HTTP $code"
  [ "$code" = "200" ] && break
done
```
- If device responds with 200 within 60s: report success and suggest running `/diagnose`.
- If device does not respond after 60s: warn that the device may be in a boot loop — recommend checking serial via `/diagnose --serial`.

---

## Method 2: Serial Flash (fallback — requires device in download mode)

Only use this method when:
- The device is unreachable over Wi-Fi (first-time flash, OTA partition corrupt, Wi-Fi credentials lost)
- The user explicitly requests it

**Prerequisite**: Device must be held in download mode (GPIO0 pulled low at boot / BOOT button held while pressing RESET).

### Flash via esptool in Docker
```bash
docker run --rm \
  --device /dev/ttyUSB0:/dev/ttyUSB0 \
  -v "$(pwd)/build":/firmware \
  espressif/idf:v4.4.7 \
  esptool.py \
    --chip esp32 \
    --port /dev/ttyUSB0 \
    --baud 460800 \
    write_flash \
      0x1000  /firmware/bootloader/bootloader.bin \
      0x8000  /firmware/partition_table/partition-table.bin \
      0x10000 /firmware/clockwise-paradise.bin
```

After flashing: reset the device (release GPIO0, press RESET) and wait 15s, then confirm with:
```bash
curl -s --max-time 10 http://192.168.1.212/ -o /dev/null -w "HTTP %{http_code}\n"
```

---

## Notes

- Never run both OTA and serial flash simultaneously.
- If `/dev/ttyUSB0` permission is denied, run: `sudo usermod -aG dialout $USER` then re-login.
- The Docker serial flash mounts `/dev/ttyUSB0` into the container — this requires the host user to have dialout group access.
- After a successful flash, run `/diagnose` to verify the deployment.
