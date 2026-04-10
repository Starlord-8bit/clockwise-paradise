#!/usr/bin/env python3
"""
flash.py — Flash clockwise-paradise firmware to an ESP32 over USB.

Handles download-mode entry automatically via DTR/RTS toggling so you don't
need to hold the BOOT button. Falls back gracefully if the device is already
in download mode.

Requirements:
    pip install esptool pyserial

Usage:
    python3 scripts/flash.py \
        --port /dev/ttyUSB0 --baud 460800 \
        --bootloader      build/bootloader/bootloader.bin \
        --partition-table build/partition_table/partition-table.bin \
        --app             build/clockwise-paradise.bin
"""

import argparse
import sys
import time
import serial
import esptool


FLASH_OFFSET_BOOT = "0x1000"
FLASH_OFFSET_PART = "0x8000"
FLASH_OFFSET_APP  = "0x10000"


def trigger_download_mode(port: str, max_attempts: int = 5) -> bool:
    """Toggle DTR/RTS to drive the ESP32 into download mode via auto-reset circuit."""
    print(f"[flash] Entering download mode on {port}...")

    with serial.Serial(port, baudrate=115200, timeout=0.1) as ser:
        for attempt in range(1, max_attempts + 1):
            # Clear state: IO0 high, EN high
            ser.setDTR(False)
            ser.setRTS(False)
            time.sleep(0.1)

            # Pull GPIO0 low before releasing reset
            ser.setDTR(True)
            time.sleep(0.05)

            # Pulse EN (reset)
            ser.setRTS(True)
            time.sleep(0.2)
            ser.setRTS(False)

            # Listen for ROM bootloader confirmation
            deadline = time.time() + 1.2
            while time.time() < deadline:
                if ser.in_waiting:
                    raw = ser.readline()
                    try:
                        line = raw.decode("utf-8", errors="ignore").strip()
                        if "waiting for download" in line.lower() or "boot:0x3" in line.lower():
                            ser.setDTR(False)
                            print("[flash] Device confirmed in download mode.")
                            return True
                    except Exception:
                        pass

            print(f"[flash] Attempt {attempt}/{max_attempts}: no confirmation yet, retrying...")
            time.sleep(0.2)

    print("[flash] Could not confirm download mode. Check cable/connections.")
    return False


def run_flash(port: str, baud: int, bootloader: str, partition_table: str, app: str) -> None:
    """Write all three binaries using esptool."""
    print(f"[flash] Writing firmware to {port} at {baud} baud...")
    esptool.main([
        "--chip",   "esp32",
        "--port",   port,
        "--baud",   str(baud),
        "--before", "no_reset",   # download mode already triggered above
        "--after",  "hard_reset",
        "write_flash",
        FLASH_OFFSET_BOOT, bootloader,
        FLASH_OFFSET_PART, partition_table,
        FLASH_OFFSET_APP,  app,
    ])


def main() -> None:
    parser = argparse.ArgumentParser(description="Flash clockwise-paradise firmware to ESP32.")
    parser.add_argument("--port",            default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0)")
    parser.add_argument("--baud",            default=460800, type=int, help="Flash baud rate (default: 460800)")
    parser.add_argument("--bootloader",      required=True, help="Path to bootloader.bin")
    parser.add_argument("--partition-table", required=True, dest="partition_table", help="Path to partition-table.bin")
    parser.add_argument("--app",             required=True, help="Path to application .bin")
    args = parser.parse_args()

    if not trigger_download_mode(args.port):
        sys.exit(1)

    run_flash(args.port, args.baud, args.bootloader, args.partition_table, args.app)


if __name__ == "__main__":
    main()
