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
import queue
import subprocess
import sys
import threading
import time
import urllib.request
import urllib.error
import urllib.parse
import uuid
from dataclasses import dataclass
from pathlib import Path


# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------

SERIAL_BAUD     = 115200
BOOT_TIMEOUT_S  = 90
POLL_INTERVAL_S = 4
MQTT_TIMEOUT_S  = 45

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


def http_post(url: str, data: bytes | None = None,
              content_type: str | None = None, timeout: int = 10):
    """POST request helper. Returns (status, headers_dict_lowercase, body_dict_or_None)."""
    req = urllib.request.Request(url, data=data if data is not None else b"", method="POST")
    if content_type:
        req.add_header("Content-Type", content_type)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
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


def http_set_key(ip: str, key: str, value: str | int, sensitive: bool = False) -> None:
    """Apply a single setting using the same /set contract as the Web UI."""
    if sensitive:
        key_q = urllib.parse.quote(key, safe="")
        body = urllib.parse.urlencode({"value": str(value)}).encode("utf-8")
        status, _, _ = http_post(
            f"http://{ip}/set?{key_q}=",
            data=body,
            content_type="application/x-www-form-urlencoded;charset=UTF-8",
        )
    else:
        encoded = urllib.parse.quote(str(value), safe="")
        status, _, _ = http_post(f"http://{ip}/set?{key}={encoded}")
    if status != 204:
        raise ConnectionError(f"/set rejected for {key!r}: HTTP {status}")


def http_restart(ip: str) -> None:
    status, _, _ = http_post(f"http://{ip}/restart")
    if status != 204:
        raise ConnectionError(f"/restart rejected: HTTP {status}")


def choose_brightness_probe(current: int) -> int:
    if current < 255:
        return current + 1
    if current > 0:
        return current - 1
    return 1


@dataclass(frozen=True)
class MqttMessage:
    topic: str
    payload: str
    retain: bool
    timestamp: float


class MqttTap:
    def __init__(self, host: str, port: int, username: str | None,
                 password: str | None, subscriptions: list[tuple[str, int]]):
        try:
            import paho.mqtt.client as mqtt
        except ImportError as e:
            raise RuntimeError(
                "paho-mqtt is required for --mqtt-smoke; install with 'python3 -m pip install paho-mqtt'"
            ) from e

        self._mqtt = mqtt
        self._client = mqtt.Client(client_id=f"cw-hw-{uuid.uuid4().hex[:10]}")
        if username:
            self._client.username_pw_set(username, password or "")
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message
        self._host = host
        self._port = port
        self._subscriptions = subscriptions
        self._connected = threading.Event()
        self._messages: list[MqttMessage] = []
        self._queue: queue.Queue[MqttMessage] = queue.Queue()
        self._lock = threading.Lock()

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        if rc != 0:
            return
        for topic, qos in self._subscriptions:
            client.subscribe(topic, qos=qos)
        self._connected.set()

    def _on_message(self, client, userdata, msg):
        message = MqttMessage(
            topic=msg.topic,
            payload=msg.payload.decode("utf-8", errors="replace"),
            retain=bool(getattr(msg, "retain", False)),
            timestamp=time.time(),
        )
        with self._lock:
            self._messages.append(message)
        self._queue.put(message)

    def start(self, timeout: int) -> None:
        self._client.connect(self._host, self._port, keepalive=30)
        self._client.loop_start()
        if not self._connected.wait(timeout=timeout):
            self.stop()
            raise RuntimeError(f"MQTT tap could not connect to {self._host}:{self._port}")

    def stop(self) -> None:
        try:
            self._client.loop_stop()
        finally:
            try:
                self._client.disconnect()
            except Exception:
                pass

    def publish(self, topic: str, payload: str, qos: int = 1, retain: bool = False) -> None:
        info = self._client.publish(topic, payload, qos=qos, retain=retain)
        info.wait_for_publish(timeout=10)
        if info.rc != self._mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"publish failed for {topic!r}: rc={info.rc}")

    def wait_for_topic(self, topic: str, timeout: int, description: str) -> MqttMessage:
        return self.wait_for(lambda message: message.topic == topic, timeout=timeout, description=description)

    def wait_for(self, predicate, timeout: int, description: str) -> MqttMessage:
        deadline = time.time() + timeout
        while True:
            with self._lock:
                for message in self._messages:
                    if predicate(message):
                        return message
            remaining = deadline - time.time()
            if remaining <= 0:
                raise TimeoutError(f"Timed out waiting for {description}")
            try:
                message = self._queue.get(timeout=min(0.5, remaining))
            except queue.Empty:
                continue
            if predicate(message):
                return message


@dataclass(frozen=True)
class MqttSmokeConfig:
    host: str
    port: int
    username: str
    password: str | None
    prefix: str
    boot_timeout: int
    timeout: int


def build_mqtt_smoke_config(args) -> MqttSmokeConfig | None:
    if not args.mqtt_smoke:
        return None

    host = args.mqtt_host or os.environ.get("MQTT_HOST")
    if not host:
        raise ValueError("--mqtt-smoke requires --mqtt-host or MQTT_HOST")

    port = args.mqtt_port
    if port is None:
        port = int(os.environ.get("MQTT_PORT", "1883"))

    username = args.mqtt_user if args.mqtt_user is not None else os.environ.get("MQTT_USER", "")
    password = args.mqtt_pass if args.mqtt_pass is not None else os.environ.get("MQTT_PASS")
    prefix = args.mqtt_prefix or f"cw-hw-{int(time.time())}-{uuid.uuid4().hex[:6]}"

    return MqttSmokeConfig(
        host=host,
        port=port,
        username=username,
        password=password,
        prefix=prefix,
        boot_timeout=args.timeout,
        timeout=args.mqtt_timeout,
    )


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


def stage_mqtt_smoke(ip: str, version: str, config: MqttSmokeConfig, result: Result) -> None:
    print(f"\n[mqtt]   Broker-backed smoke against {config.host}:{config.port}...")
    print(f"         Using unique topic prefix: {config.prefix}")

    tap = MqttTap(
        host=config.host,
        port=config.port,
        username=config.username or None,
        password=config.password,
        subscriptions=[("homeassistant/#", 1), (f"{config.prefix}/#", 1)],
    )

    try:
        tap.start(timeout=config.timeout)
        result.ok("MQTT tap connected", f"{config.host}:{config.port}")

        http_set_key(ip, "mqttEnabled", 1)
        http_set_key(ip, "mqttBroker", config.host)
        http_set_key(ip, "mqttPort", config.port)
        http_set_key(ip, "mqttUser", config.username)
        if config.password is not None:
            http_set_key(ip, "mqttPass", config.password, sensitive=True)
        http_set_key(ip, "mqttPrefix", config.prefix)
        result.ok("MQTT settings applied", config.prefix)

        http_restart(ip)
        result.ok("Restart requested", "MQTT config saved")

        if stage_boot(ip, version, config.boot_timeout, result):
            result.ok("Device rebooted for MQTT smoke", version)
        else:
            return

        avail_message = tap.wait_for(
            lambda m: m.topic.startswith(f"{config.prefix}/") and m.topic.endswith("/availability") and m.payload == "online",
            timeout=config.timeout,
            description="MQTT availability=online",
        )
        base_topic = avail_message.topic.rsplit("/", 1)[0]
        state_topic = f"{base_topic}/state"
        result.ok("MQTT availability published", avail_message.topic)
        _assert_retained_delivery(
            config,
            avail_message.topic,
            config.timeout,
            lambda m: m.payload == "online",
            "retained availability",
        )
        result.ok("MQTT availability retained", avail_message.topic)

        brightness_discovery = tap.wait_for(
            lambda m: m.topic.startswith("homeassistant/") and _matches_brightness_discovery(m.payload, base_topic),
            timeout=config.timeout,
            description="brightness discovery payload for the fresh prefix",
        )
        discovery_payload = json.loads(brightness_discovery.payload)
        result.ok("HA discovery published", brightness_discovery.topic)
        _assert_retained_delivery(
            config,
            brightness_discovery.topic,
            config.timeout,
            lambda m: _matches_brightness_discovery(m.payload, base_topic),
            "retained discovery payload",
        )
        result.ok("HA discovery retained", brightness_discovery.topic)

        initial_state = _wait_for_state(tap, state_topic, config.timeout)
        current_brightness = int(initial_state.get("brightness", 0))
        result.ok("MQTT state published", f"brightness={current_brightness}")

        probe_brightness = choose_brightness_probe(current_brightness)
        command_topic = discovery_payload["command_topic"]
        tap.publish(command_topic, str(probe_brightness), qos=1, retain=False)
        updated_state = _wait_for_state(
            tap,
            state_topic,
            config.timeout,
            predicate=lambda payload: int(payload.get("brightness", -1)) == probe_brightness,
        )
        result.ok(
            "MQTT command round-trip",
            f"{command_topic} -> brightness={updated_state.get('brightness')}",
        )

        if probe_brightness != current_brightness:
            tap.publish(command_topic, str(current_brightness), qos=1, retain=False)
            _wait_for_state(
                tap,
                state_topic,
                config.timeout,
                predicate=lambda payload: int(payload.get("brightness", -1)) == current_brightness,
            )
            result.ok("MQTT brightness restored", str(current_brightness))
    except Exception as e:
        result.fail("MQTT smoke", str(e))
    finally:
        tap.stop()


def _matches_brightness_discovery(payload: str, base_topic: str) -> bool:
    try:
        body = json.loads(payload)
    except json.JSONDecodeError:
        return False
    return (
        body.get("command_topic") == f"{base_topic}/set/brightness"
        and body.get("state_topic") == f"{base_topic}/state"
        and body.get("availability_topic") == f"{base_topic}/availability"
    )


def _wait_for_state(tap: MqttTap, state_topic: str, timeout: int, predicate=None) -> dict:
    def matches(message: MqttMessage) -> bool:
        if message.topic != state_topic:
            return False
        try:
            payload = json.loads(message.payload)
        except json.JSONDecodeError:
            return False
        if predicate and not predicate(payload):
            return False
        return True

    message = tap.wait_for(matches, timeout=timeout, description=f"state on {state_topic}")
    return json.loads(message.payload)


def _assert_retained_delivery(config: MqttSmokeConfig, topic: str, timeout: int, predicate, description: str) -> None:
    retained_tap = MqttTap(
        host=config.host,
        port=config.port,
        username=config.username or None,
        password=config.password,
        subscriptions=[(topic, 1)],
    )
    try:
        retained_tap.start(timeout=timeout)
        retained_message = retained_tap.wait_for_topic(topic, timeout, description)
        if not retained_message.retain:
            raise AssertionError(f"Expected retained delivery for {topic}")
        if not predicate(retained_message):
            raise AssertionError(f"Unexpected payload for {topic}")
    finally:
        retained_tap.stop()


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
    parser.add_argument("--mqtt-smoke", action="store_true",
                        help="Run broker-backed MQTT + HA discovery smoke checks after HTTP checks")
    parser.add_argument("--mqtt-host",  default=None,
                        help="Broker host/IP reachable from both the device and this test runner")
    parser.add_argument("--mqtt-port",  default=None, type=int,
                        help="Broker port (default: 1883 or MQTT_PORT env var)")
    parser.add_argument("--mqtt-user",  default=None,
                        help="Broker username for both the device and the local MQTT tap (optional)")
    parser.add_argument("--mqtt-pass",  default=None,
                        help="Broker password for both the device and the local MQTT tap (optional)")
    parser.add_argument("--mqtt-prefix", default=None,
                        help="MQTT topic prefix to apply on the device (defaults to a fresh per-run prefix)")
    parser.add_argument("--mqtt-timeout", default=MQTT_TIMEOUT_S, type=int,
                        help=f"MQTT broker/event wait timeout in seconds (default: {MQTT_TIMEOUT_S})")
    args = parser.parse_args()

    result  = Result()
    version = args.version or get_build_version()
    try:
        mqtt_config = build_mqtt_smoke_config(args)
    except ValueError as e:
        print(f"[error] {e}")
        return 1

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
    if mqtt_config:
        stage_mqtt_smoke(ip, version, mqtt_config, result)
    stage_plugins(ip, version, result)

    _print_summary(result)
    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main())
