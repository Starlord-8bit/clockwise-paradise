# Load per-developer config (device IP, serial port, etc.)
# Copy .env.example to .env and fill in your values — .env is gitignored.
-include .env
export DEVICE_IP PORT BAUD

IDF_VERSION ?= v4.4.7
PORT        ?= /dev/ttyUSB0
BAUD        ?= 115200

TOOLS_VENV ?= .venv-tools
PIO_BIN    ?= $(CURDIR)/$(TOOLS_VENV)/bin/pio
VCMAKE_BIN ?= $(CURDIR)/$(TOOLS_VENV)/bin/cmake
NATIVE_TEST_BUILD_DIR ?= $(CURDIR)/build/test-native-cmake

CMAKE_BIN := $(shell command -v cmake 2>/dev/null)
ifeq ($(strip $(CMAKE_BIN)),)
CMAKE_BIN := $(VCMAKE_BIN)
endif

# Artifact tag should follow firmware version source-of-truth (version.txt),
# not git-describe commit distance, so hardware test logs stay unambiguous.
VERSION   := $(shell V=$$(cat version.txt 2>/dev/null | tr -d '[:space:]'); if [ -n "$$V" ]; then echo "$$V" | sed 's/^[vV]\{0,1\}/v/'; else git describe --tags --always --dirty; fi)
BUILD_DIR := build/compile
DIST_DIR  := build/$(VERSION)

APP_BIN  := $(BUILD_DIR)/clockwise-paradise.bin
BOOT_BIN := $(BUILD_DIR)/bootloader/bootloader.bin
PART_BIN := $(BUILD_DIR)/partition_table/partition-table.bin

DIST_APP  := $(DIST_DIR)/clockwise-paradise.$(VERSION).bin
DIST_BOOT := $(DIST_DIR)/bootloader.bin
DIST_PART := $(DIST_DIR)/partition-table.bin

# For flash-ota: explicit IP= on command line takes priority, falls back to DEVICE_IP from .env
_FLASH_IP := $(or $(IP),$(DEVICE_IP))

.PHONY: tools-setup tools-check pio-test test build check flash flash-ota test-hw clean help

## Install pinned local developer tools in .venv-tools (platformio + cmake)
tools-setup:
	python3 -m venv $(TOOLS_VENV)
	$(TOOLS_VENV)/bin/pip install --upgrade pip
	$(TOOLS_VENV)/bin/pip install "platformio>=6.1,<7" "cmake>=3.28,<4"

## Show which tools are currently available
tools-check:
	@echo "System cmake: $$(command -v cmake || echo missing)"
	@echo "System pio:   $$(command -v pio || echo missing)"
	@echo "Venv cmake:   $(VCMAKE_BIN)"
	@echo "Venv pio:     $(PIO_BIN)"
	@$(CMAKE_BIN) --version >/dev/null 2>&1 && echo "CMake OK via $(CMAKE_BIN)" || echo "CMake missing; run 'make tools-setup'"
	@$(PIO_BIN) --version >/dev/null 2>&1 && echo "PlatformIO OK via $(PIO_BIN)" || echo "PlatformIO venv missing; run 'make tools-setup'"

## Run PlatformIO native tests using pinned local venv toolchain
pio-test:
	@test -x $(PIO_BIN) || (echo "PlatformIO missing in $(TOOLS_VENV); run 'make tools-setup'"; exit 1)
	@echo "PlatformIO native test discovery is not wired to this CMake-native suite; delegating to 'make test'."
	@$(MAKE) test

## Run native unit tests (no Docker required)
test:
	@test -x $(CMAKE_BIN) || (echo "CMake missing; run 'make tools-setup'"; exit 1)
	mkdir -p $(NATIVE_TEST_BUILD_DIR)
	cd firmware/test/test_native && \
	  $(CMAKE_BIN) -B $(NATIVE_TEST_BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug && \
	  $(CMAKE_BIN) --build $(NATIVE_TEST_BUILD_DIR) && \
	  $(NATIVE_TEST_BUILD_DIR)/test_native

## Build firmware via Docker — mirrors CI exactly
## Compile artifacts → build/compile/  |  Firmware binaries → build/$(VERSION)/
## Override IDF version: make build IDF_VERSION=v4.4.7
build:
	docker run --rm \
	  -v "$(CURDIR)":/project \
	  -w /project \
	  espressif/idf:$(IDF_VERSION) \
	  idf.py -B $(BUILD_DIR) build
	mkdir -p $(DIST_DIR)
	cp $(APP_BIN)  $(DIST_APP)
	cp $(BOOT_BIN) $(DIST_BOOT)
	cp $(PART_BIN) $(DIST_PART)
	@echo ""
	@echo "Firmware ready in $(DIST_DIR)/"
	@ls -lh $(DIST_DIR)/

## Run tests then build — mirrors the CI gate; run this before pushing a release
check: test build

## Flash over USB using auto-reset sequencing (requires: pip install esptool pyserial)
## Override port: make flash PORT=/dev/ttyACM0
## For Wi-Fi OTA (no USB cable), use: make flash-ota
flash:
	@test -f $(DIST_APP) || (echo "No firmware found — run 'make build' first"; exit 1)
	@test -n "$(PORT)" || (echo "No serial port — set PORT in .env or pass PORT=/dev/ttyUSB0"; echo "For Wi-Fi OTA, use: make flash-ota"; exit 1)
	python3 scripts/flash.py \
	  --port   "$(PORT)" \
	  --baud   "$(or $(BAUD),460800)" \
	  --bootloader      $(DIST_BOOT) \
	  --partition-table $(DIST_PART) \
	  --app    $(DIST_APP)

## Flash over Wi-Fi OTA — no USB cable required (requires: curl)
## Usage: make flash-ota [IP=192.168.x.x]   (or set DEVICE_IP in .env)
flash-ota:
	@test -f $(DIST_APP)   || (echo "No firmware found — run 'make build' first"; exit 1)
	@test -n "$(_FLASH_IP)" || (echo "No device IP — set DEVICE_IP in .env or pass IP=<addr>"; exit 1)
	curl -X POST "http://$(_FLASH_IP)/ota/upload" \
	  -H "Content-Type: application/octet-stream" \
	  --data-binary @$(DIST_APP) \
	  --progress-bar

## Build + OTA flash + hardware verification (requires: pip install pyserial)
## Set DEVICE_IP in .env or override: make test-hw DEVICE_IP=192.168.x.x
## Optional serial monitoring:         make test-hw PORT=/dev/ttyUSB0
test-hw: build
	@test -n "$(DEVICE_IP)" || (echo "No device IP — set DEVICE_IP in .env or pass DEVICE_IP=<addr>"; exit 1)
	python3 scripts/test_hw.py \
	  --ip      "$(DEVICE_IP)" \
	  --bin     "$(DIST_APP)" \
	  $(if $(PORT),--port "$(PORT)",)

## Remove compile artifacts and the current version's dist folder
## (build/compile is Docker root-owned — uses sudo if plain rm fails)
clean:
	rm -rf $(DIST_DIR)
	rm -rf $(BUILD_DIR) 2>/dev/null || sudo rm -rf $(BUILD_DIR)

help:
	@echo ""
	@echo "Usage: make <target> [VAR=value]"
	@echo ""
	@echo "Targets:"
	@echo "  tools-setup Install local dev tools into $(TOOLS_VENV)"
	@echo "  tools-check Show system and venv tool availability"
	@echo "  pio-test    Validate PlatformIO install, then run native tests via make test"
	@echo "  test        Run native unit tests (no Docker)"
	@echo "  build       Build firmware (IDF_VERSION=$(IDF_VERSION)) -> $(DIST_DIR)/"
	@echo "  check       Run test + build — pre-release gate"
	@echo "  flash       Flash over USB   (PORT=$(PORT)  BAUD=$(BAUD))"
	@echo "  flash-ota   Flash over Wi-Fi (DEVICE_IP or IP=<addr>)"
	@echo "  test-hw     Build + OTA flash + hardware verification (DEVICE_IP in .env)"
	@echo "  clean       Remove build/compile and build/$(VERSION)"
	@echo ""
	@echo "Local config: copy .env.example -> .env and set DEVICE_IP"
	@echo ""
