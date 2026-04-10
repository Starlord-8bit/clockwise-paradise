# Load per-developer config (device IP, serial port, etc.)
# Copy .env.example to .env and fill in your values — .env is gitignored.
-include .env
export DEVICE_IP PORT BAUD

IDF_VERSION ?= v4.4.7
PORT        ?= /dev/ttyUSB0
BAUD        ?= 115200

VERSION   := $(shell git describe --tags --always --dirty)
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

.PHONY: test build check flash flash-ota test-hw clean help

## Run native unit tests (no Docker required)
test:
	cd firmware/test/test_native && \
	  cmake -B build -DCMAKE_BUILD_TYPE=Debug && \
	  cmake --build build && \
	  ./build/test_native

## Build firmware via Docker — mirrors CI exactly
## Compile artifacts → build/compile/  |  Firmware binaries → build/$(VERSION)/
## Override IDF version: make build IDF_VERSION=v4.4.7
## Note: if the build fails with HOME/permission errors, remove --user from the docker run line
build:
	docker run --rm \
	  -v "$(CURDIR)":/project \
	  -w /project \
	  --user "$$(id -u):$$(id -g)" \
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
flash:
	@test -f $(DIST_APP) || (echo "No firmware found — run 'make build' first"; exit 1)
	python3 scripts/flash.py \
	  --port   $(PORT) \
	  --baud   $(BAUD) \
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
clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)

help:
	@echo ""
	@echo "Usage: make <target> [VAR=value]"
	@echo ""
	@echo "Targets:"
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
