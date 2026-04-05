# 🌴 Clockwise Paradise

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![GitHub Release](https://img.shields.io/github/v/release/Starlord-8bit/clockwise-paradise)](https://github.com/Starlord-8bit/clockwise-paradise/releases)
[![HA Integration](https://img.shields.io/badge/Home%20Assistant-Integration-blue)](https://github.com/Starlord-8bit/ha-clockwise)

**Clockwise Paradise** is a feature-rich ESP32 LED matrix wall clock firmware — a significantly enhanced fork of the open-source [Clockwise](https://github.com/jnthas/clockwise) project.

It builds on the solid foundation of the original, adds the best features from the Chinese ClockWise Plus firmware (v3.11), and goes beyond both with a clean, maintainable, privacy-respecting architecture.

---

## ✨ Features

### Everything from Clockwise OG
- 64×64 HUB75 LED matrix support
- Web-based configuration UI
- NTP time sync, timezone support, POSIX strings
- Canvas clockface (runtime JSON themes — infinite customisation)
- 6 built-in clockfaces: Mario, Pac-Man, World Map, Castlevania, Pokedex, Canvas

### New in Clockwise Paradise

| Feature | Description |
|---|---|
| **LED Colour Order** | 3-way selector: RGB / RBG / GBR — fixes colour issues on all panel types |
| **Reverse Phase** | Toggle clock phase — fixes ghosting/misalignment on some panels |
| **Brightness Method** | Auto-LDR / Time-based / Fixed — choose how brightness is controlled |
| **Night Schedule** | Configurable night window (e.g. 22:00–07:00) with midnight-wrap |
| **Night Strategy** | Nothing / Turn off display / Big clock — what happens at night |
| **Big Clock** | Large HH:MM display at night via Canvas, with configurable color |
| **Auto-change Clockface** | Rotate faces at midnight — sequential or random |
| **Uptime Counter** | Days running shown in the settings footer |
| **Web Server Watchdog** | Auto-restart HTTP server every 5 minutes — no more freezes |
| **OTA Updates** | Update firmware over Wi-Fi — via web UI or HTTP API (no USB required after initial flash) |
| **Canvas Clockfaces** | 11 ready-to-use JSON faces in `clockfaces/` — local server supported |

### No callhome, no cloud, no surprises
- Zero telemetry or device registration
- All features work fully offline
- Canvas faces served from your own homelab — no external dependencies required

---

## 🏠 Home Assistant Integration

A native HACS integration is available at **[Starlord-8bit/ha-clockwise](https://github.com/Starlord-8bit/ha-clockwise)**.

Gives you proper HA entities: clockface selector, brightness slider, night mode controls, auto-change, NTP server, Canvas file/server, restart button, uptime sensor — all auto-detected based on firmware version.

---

## 🖼️ Canvas Clockfaces

The `clockfaces/` directory contains ready-to-use Canvas JSON themes:

| File | Description |
|---|---|
| `bigclock.json` | Night mode big clock (black bg, large digits) |
| `kirby.json` | Kirby animated |
| `hello-kitty.json` | Hello Kitty |
| `picachu.json` | Pikachu |
| `labubu.json` | Labubu animated |
| `girl.json` | Girl animated |
| `sharpeidog.json` | Shar Pei Dog |
| `snoopy3.json` | Snoopy |
| `retrocomputer.json` | Retro Computer |
| `pepsi-final.json` | Pepsi |
| `clock-club.json` | Clock Club community face |

Serve these from any web server on your LAN, point `Canvas Server` at it — no internet needed.

More community faces at [jnthas/clock-club](https://github.com/jnthas/clock-club).

---

## 🛠️ Building

### Requirements
- ESP-IDF v4.4.x or PlatformIO
- ESP32 dev board
- 64×64 HUB75 LED matrix

### Quick start
```bash
git clone --recurse-submodules https://github.com/Starlord-8bit/clockwise-paradise
cd clockwise-paradise
idf.py build
idf.py flash
```

Or use the web flasher at [clockwise.page](https://clockwise.page) with a release `.bin`.

---

## 🔌 Flashing

### Initial flash (USB — one time only)

The first time you flash a new ESP32 you need to write three files: the bootloader, the partition table, and the app firmware. Download all three from the [latest release](https://github.com/Starlord-8bit/clockwise-paradise/releases/latest):

```
bootloader.bin         → flash at 0x1000
partition-table.bin    → flash at 0x8000
clockwise-paradise.bin → flash at 0x10000
```

Using `esptool`:

```bash
esptool --chip esp32 --port /dev/ttyUSB0 --baud 460800 write-flash \
  0x1000  bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 clockwise-paradise.bin
```

> **Tip:** hold the BOOT button on the ESP32, tap RESET, then release BOOT before running the command. This forces the chip into download mode if auto-reset doesn't work.

### OTA updates (Wi-Fi — all subsequent updates)

Once the initial firmware is flashed, you never need USB again.

**Via the web UI:**
1. Open `http://<device-ip>/` in a browser
2. Click **Upload .bin** in the toolbar
3. Select `clockwise-paradise.bin` from the latest release
4. Wait ~15 seconds for the device to reboot

**Via HTTP API (for scripts / AI automation):**
```bash
curl -X POST http://<device-ip>/ota/upload \
     -H 'Content-Type: application/octet-stream' \
     --data-binary @clockwise-paradise.bin
```
Returns `{"status":"ok"}` and reboots, or `{"status":"error","message":"..."}` on failure.

**Via GitHub auto-update (checks latest release):**
```bash
curl -X POST http://<device-ip>/ota/update
```
The device fetches and flashes the latest `clockwise-paradise.bin` from GitHub Releases automatically.

> **Note:** OTA can only update the app firmware (`clockwise-paradise.bin`). Bootloader and partition table changes still require USB.

---

## 📋 Roadmap

- [ ] Multi-clockface runtime dispatcher (switch faces without reflashing)
- [ ] Multi-Canvas slots (up to 5 Canvas faces configured from web UI)
- [ ] CI/CD pipeline (compile test on every push)
- [ ] HACS default store submission
- [ ] Canvas clockface editor integration

---

## 🙏 Credits

- **[jnthas/clockwise](https://github.com/jnthas/clockwise)** — the original open-source Clockwise project (MIT). Clockwise Paradise is built on this foundation.
- **[topyuan.top/clock](https://topyuan.top/clock)** — ClockWise Plus firmware, inspiration for feature set.
- **[jnthas/clock-club](https://github.com/jnthas/clock-club)** — community Canvas clockface themes.

---

## 📄 License

MIT — see [LICENSE](LICENSE).
