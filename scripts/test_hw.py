#!/usr/bin/env python3
"""
test_hw.py — Hardware integration test for clockwise-paradise.

Workflow:
  1. OTA-flash the built firmware to the device
  2. Wait for the device to reboot and come back online
  3. Confirm the new firmware version is running and OTA state is valid
  4. Run HTTP sanity checks (version header, web UI, OTA status)
  5. Auto-discover and run any test modules in tests/hardware/

Optional serial boot monitoring: pip install pyserial

Usage:
    # Normal: flash + verify
    python3 scripts/test_hw.py

    # Skip flash — run checks only (device must already be on target version)
    python3 scripts/test_hw.py --skip-flash

    # With serial boot monitoring
    python3 scripts/test_hw.py --port /dev/ttyUSB0

    # Custom device
    python3 scripts/test_hw.py --ip 192.168.1.99 --version 2.7.0
"""

import argparse
import importlib.util
import json
import os
import subprocess
import sys
import time
import urllib.request
import urllib.error
from pathlib import Path


# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------

SERIAL_BAUD     = 115200
BOOT_TIMEOUT_S  = 90
POLL_INTERVAL_S = 4

# The serial string that confirms new firmware has completed setup() and
# called esp_ota_mark_app_valid_cancel_rollback() — see main.cpp.
BOOT_MARKER = "[OTA] Firmware marked valid"

REPO_ROOT = Path(__file__).resolve().parent.parent


def _load_dotenv() -> None:
    """Load REPO_ROOT/.env into os.environ (simple parser, no deps required)."""
    env_file = REPO_ROOT / ".env"
    if not env_file.exists():
        return
    for line in env_file.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip()
        if key and key not in os.environ:  # don't override shell env
            os.environ[key] = value

_load_dotenv()


# ---------------------------------------------------------------------------
# Result tracker
# ---------------------------------------------------------------------------

class Result:
    def __init__(self):
        self._passed: list[tuple[str, str]] = []
        self._failed: list[tuple[str, str]] = []

    def ok(self, name: str, detail: str = "") -> None:
        self._passed.append((name, detail))
        suffix = f"  ({detail})" if detail else ""
        print(f"  PASS  {name}{suffix}")

    def fail(self, name: str, detail: str = "") -> None:
        self._failed.append((name, detail))
        suffix = f"  ({detail})" if detail else ""
        print(f"  FAIL  {name}{suffix}")

    @property
    def ok_count(self) -> int:
        return len(self._passed)

    @property
    def fail_count(self) -> int:
        return len(self._failed)

    @property
    def passed(self) -> bool:
        return self.fail_count == 0


# ---------------------------------------------------------------------------
# HTTP helpers (stdlib only)
# ---------------------------------------------------------------------------

def http_get(url: str, timeout: int = 10):
    """Returns (status, headers_dict_lowercase, body_dict_or_None)."""
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            raw = resp.read()
            headers = {k.lower(): v for k, v in resp.headers.items()}
            try:
                body = json.loads(raw) if raw else None
            except (json.JSONDecodeError, ValueError):
                body = None
            return resp.status, headers, body
    except urllib.error.HTTPError as e:
        headers = {k.lower(): v for k, v in e.headers.items()}
        return e.code, headers, None
    except Exception as e:
        raise ConnectionError(str(e)) from e


def http_post_binary(url: str, data: bytes, timeout: int = 120):
    """POST raw bytes. Returns (status, body_dict_or_None)."""
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/octet-stream")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read()
            body = json.loads(raw) if raw else None
            return resp.status, body
    except urllib.error.HTTPError as e:
        return e.code, None
    except Exception as e:
        raise ConnectionError(str(e)) from e


# ---------------------------------------------------------------------------
# Version helpers
# ---------------------------------------------------------------------------

def get_build_version() -> str:
    """
    Returns the version baked into the firmware at build time.
    Reads version.txt (single source of truth since main/CMakeLists.txt was updated).
    Falls back to git describe --abbrev=0 for repos without version.txt.
    """
    version_file = REPO_ROOT / "version.txt"
    if version_file.exists():
        return version_file.read_text().strip()
    try:
        tag = subprocess.check_output(
            ["git", "describe", "--tags", "--abbrev=0", "--match", "v*"],
            cwd=REPO_ROOT, text=True, stderr=subprocess.DEVNULL
        ).strip()
        return tag.lstrip("v")
    except subprocess.CalledProcessError:
        return "dev"


def find_dist_bin(version: str) -> Path | None:
    """Look for the firmware binary produced by 'make build' in build/{tag}/."""
    for prefix in ("v", ""):
        tag = f"{prefix}{version}"
        candidate = REPO_ROOT / "build" / tag / f"clockwise-paradise.{tag}.bin"
        if candidate.exists():
            return candidate
    return None


# ---------------------------------------------------------------------------
# Stage 1: OTA flash
# ---------------------------------------------------------------------------

def stage_flash(ip: str, bin_path: Path, result: Result) -> bool:
    url = f"http://{ip}/ota/upload"
    size_kb = bin_path.stat().st_size // 1024
    print(f"\n[flash]  {bin_path.name}  ({size_kb} KB)  ->  {url}")
    try:
        status, body = http_post_binary(url, bin_path.read_bytes())
    except Exception as e:
        result.fail("OTA upload", str(e))
        return False

    if status == 200:
        msg = (body or {}).get("message", "accepted")
        result.ok("OTA upload accepted", msg)
        return True
    else:
        result.fail("OTA upload", f"HTTP {status}")
        return False


# ---------------------------------------------------------------------------
# Stage 2: Boot confirmation
# ---------------------------------------------------------------------------

def stage_boot(ip: str, expected_version: str, timeout: int,
               result: Result, port: str | None = None) -> bool:
    """Poll /ota/status until the device is back up, running the right version."""
    print(f"\n[boot]   Waiting for reboot (up to {timeout}s)...")

    if port:
        # Serial monitoring runs in the same thread — best effort
        if _check_serial_boot(port, timeout):
            result.ok("Serial boot marker", BOOT_MARKER)
        else:
            result.fail("Serial boot marker", "not seen within timeout")

    deadline = time.time() + timeout
    last_err = None

    while time.time() < deadline:
        time.sleep(POLL_INTERVAL_S)
        try:
            _, _, body = http_get(f"http://{ip}/ota/status")
            if body:
                state   = body.get("running_state", "")
                version = body.get("running_version", "")
                print(f"         state={state!r}  version={version!r}")

                if state in ("invalid", "aborted"):
                    result.fail("OTA state after boot", f"firmware rejected — state={state!r}")
                    return False

                if state == "valid":
                    if version == expected_version:
                        result.ok("New firmware running", f"version={version}  state=valid")
                        return True
                    else:
                        result.fail("Version mismatch", f"expected={expected_version!r}  got={version!r}")
                        return False

        except ConnectionError as e:
            last_err = str(e)  # device still rebooting — keep polling

    result.fail("Device did not come back online", last_err or f"timeout after {timeout}s")
    return False


def _check_serial_boot(port: str, timeout: int) -> bool:
    try:
        import serial as pyserial
    except ImportError:
        print("         [serial] pyserial not installed — skipping serial monitoring")
        return False
    try:
        deadline = time.time() + timeout
        with pyserial.Serial(port, SERIAL_BAUD, timeout=1.0) as ser:
            while time.time() < deadline:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line:
                    print(f"         [serial] {line}")
                if BOOT_MARKER in line:
                    return True
        return False
    except Exception as e:
        print(f"         [serial] Could not open {port}: {e}")
        return False


# ---------------------------------------------------------------------------
# Stage 3: HTTP assertion checks
# ---------------------------------------------------------------------------

def stage_checks(ip: str, version: str, result: Result) -> None:
    print(f"\n[checks] HTTP assertions against {ip}...")

    # /ota/status — running version + state
    try:
        _, _, body = http_get(f"http://{ip}/ota/status")
        if body:
            state   = body.get("running_state", "")
            running = body.get("running_version", "")
            if state == "valid":
                result.ok("/ota/status: state", "valid")
            else:
                result.fail("/ota/status: state", f"expected=valid  got={state!r}")
            if running == version:
                result.ok("/ota/status: version", running)
            else:
                result.fail("/ota/status: version", f"expected={version!r}  got={running!r}")
        else:
            result.fail("/ota/status", "empty or unparseable response")
    except Exception as e:
        result.fail("/ota/status", str(e))

    # GET /get — X-CW_FW_VERSION response header
    try:
        _, headers, _ = http_get(f"http://{ip}/get")
        fw_ver = headers.get("x-cw_fw_version", "")
        if fw_ver == version:
            result.ok("X-CW_FW_VERSION header", fw_ver)
        else:
            result.fail("X-CW_FW_VERSION header", f"expected={version!r}  got={fw_ver!r}")
    except Exception as e:
        result.fail("GET /get", str(e))

    # GET / — web UI is reachable
    try:
        status, _, _ = http_get(f"http://{ip}/")
        if status == 200:
            result.ok("Web UI reachable", "HTTP 200")
        else:
            result.fail("Web UI reachable", f"HTTP {status}")
    except Exception as e:
        result.fail("Web UI reachable", str(e))


# ---------------------------------------------------------------------------
# Stage 4: Plugin tests  (tests/hardware/*.py)
#
# Each file may export:
#   run_tests(ip: str, version: str, result: Result) -> None
#
# Example — tests/hardware/test_brightness.py:
#   def run_tests(ip, version, result):
#       # POST /set?key=displayBright&value=200, then GET /get and assert header
#       ...
# ---------------------------------------------------------------------------

def stage_plugins(ip: str, version: str, result: Result) -> None:
    hw_dir = REPO_ROOT / "tests" / "hardware"
    if not hw_dir.exists():
        return
    plugins = sorted(hw_dir.glob("*.py"))
    if not plugins:
        return

    print(f"\n[plugins] {len(plugins)} hardware test module(s) found...")
    for path in plugins:
        spec = importlib.util.spec_from_file_location(path.stem, path)
        mod  = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(mod)
            if callable(getattr(mod, "run_tests", None)):
                print(f"  -> {path.name}")
                mod.run_tests(ip, version, result)
            else:
                print(f"  [skip] {path.name} — no run_tests() function")
        except Exception as e:
            result.fail(f"plugin:{path.stem}", str(e))


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def _print_summary(result: Result) -> None:
    total = result.ok_count + result.fail_count
    print(f"\n{'='*50}")
    if result.passed:
        print(f"PASSED  {result.ok_count}/{total} checks")
    else:
        print(f"FAILED  {result.fail_count}/{total} checks")
        for name, detail in result._failed:
            print(f"  x  {name}" + (f": {detail}" if detail else ""))
    print("="*50)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Flash + boot-verify + HTTP checks for clockwise-paradise."
    )
    parser.add_argument("--ip",         default=None,
                        help="Device IP address (required; or set DEVICE_IP env var)")
    parser.add_argument("--version",    default=None,
                        help="Expected version without 'v' prefix (auto-detected from git tag if omitted)")
    parser.add_argument("--bin",        default=None,
                        help="Path to firmware .bin (auto-detected from build/{{tag}}/ if omitted)")
    parser.add_argument("--port",       default=None,
                        help="Serial port for boot monitoring (optional, e.g. /dev/ttyUSB0)")
    parser.add_argument("--timeout",    default=BOOT_TIMEOUT_S, type=int,
                        help=f"Boot wait timeout in seconds (default: {BOOT_TIMEOUT_S})")
    parser.add_argument("--skip-flash", action="store_true",
                        help="Skip OTA flash — run checks only")
    args = parser.parse_args()

    result  = Result()
    version = args.version or get_build_version()

    ip = args.ip or os.environ.get("DEVICE_IP")
    if not ip:
        print("[error] No device IP — pass --ip <addr> or set DEVICE_IP in your .env")
        return 1

    print(f"\n=== clockwise-paradise hardware test ===")
    print(f"Device:   {ip}")
    print(f"Version:  {version}")

    if not args.skip_flash:
        bin_path = Path(args.bin) if args.bin else find_dist_bin(version)
        if not bin_path or not bin_path.exists():
            print(f"[error] Firmware binary not found for version {version!r}")
            print(f"        Expected: build/v{version}/clockwise-paradise.v{version}.bin")
            print(f"        Run 'make build' first, or pass --bin <path>")
            return 1

        print(f"Binary:   {bin_path}")
        if args.port:
            print(f"Serial:   {args.port}")
        print()

        if not stage_flash(ip, bin_path, result):
            _print_summary(result)
            return 1

        if not stage_boot(ip, version, args.timeout, result, args.port):
            _print_summary(result)
            return 1
    else:
        print(f"(skip-flash — checks only)\n")

    stage_checks(ip, version, result)
    stage_plugins(ip, version, result)

    _print_summary(result)
    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())
