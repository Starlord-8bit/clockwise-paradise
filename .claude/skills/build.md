---
description: Build the firmware using Docker (espressif/idf:v4.4.7 — same image as CI). Runs idf.py build inside the container, surfaces errors with file:line references. Use /build to compile and check for errors autonomously without a local ESP-IDF toolchain.
---

Build the Clockwise Paradise firmware via Docker (matches CI environment exactly).

## Steps

1. Confirm Docker is available:
```bash
docker --version
```
If Docker is not found, report: "Docker is required for /build. Install Docker Desktop or Docker Engine." and stop.

2. Pull the image if not already cached (silent if present):
```bash
docker image inspect espressif/idf:latest >/dev/null 2>&1 || echo "Image not cached — first pull will take a few minutes."
```

3. Run the build inside the container. The repo root is mounted read-write so build artifacts land in `build/` on the host:
```bash
docker run --rm \
  -v "$(pwd)":/project \
  -w /project \
  espressif/idf:latest \
  idf.py build 2>&1 | tail -100
```

4. Parse the output:
   - If build **succeeded** (line contains `Project build complete`):
     - Report binary size from the `build/` summary line
     - Report artifact locations: `build/clockwise-paradise.bin` (@ 0x10000), `build/bootloader/bootloader.bin` (@ 0x1000), `build/partition_table/partition-table.bin` (@ 0x8000)
   - If build **failed**: extract each error/warning line and present as `[file:line](file:line) — error message`, grouped by file.

5. If there are errors, identify the most likely root cause (usually the first error in the chain):
   > **Root cause**: `firmware/lib/cw-commons/CWPreferences.h:87` — ...

6. If errors are in submodule paths (`components/` or `firmware/clockfaces/`), note that these are third-party — do not modify them directly; check if a version pin or patch is needed.

## Common Error Patterns

- `'ESP_LOGI' was not declared` → missing `#include "esp_log.h"`
- `undefined reference to 'nvs_open'` → missing `nvs_flash` in CMakeLists `PRIV_REQUIRES`
- `undefined reference to 'esp_ota_ops'` → missing `app_update` in CMakeLists `PRIV_REQUIRES`
- `Unexpected token` / JS parse error → check `SettingsWebPage.h` for missing closing braces in inline JS
- `stack overflow` → reduce local variable usage or increase `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
- `NVS key too long` → NVS keys must be ≤ 15 characters; check `CWPreferences.h`

## Notes

- First run pulls `espressif/idf:latest` (~1.5 GB). Subsequent runs use the local cache and are fast.
- The container runs as root; build artifacts in `build/` may be owned by root on the host. Run `sudo chown -R $USER build/` if needed.
- Submodules must be checked out: `git submodule update --init --recursive` before building.

After reporting errors, ask: "Would you like me to fix these?" before making any changes.
