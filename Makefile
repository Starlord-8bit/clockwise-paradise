IDF_VERSION ?= v4.4.7
PORT        ?= /dev/ttyUSB0
BAUD        ?= 460800

APP_BIN  := build/clockwise-paradise.bin
BOOT_BIN := build/bootloader/bootloader.bin
PART_BIN := build/partition_table/partition-table.bin

.PHONY: test build check flash flash-ota clean help

## Run native unit tests (no Docker required)
test:
	cd firmware/test/test_native && \
	  cmake -B build -DCMAKE_BUILD_TYPE=Debug && \
	  cmake --build build && \
	  ./build/test_native

## Build firmware via Docker — mirrors CI exactly
## Override IDF version: make build IDF_VERSION=v4.4.7
build:
	docker run --rm \
	  -v "$(CURDIR)":/project \
	  -w /project \
	  espressif/idf:$(IDF_VERSION) \
	  idf.py build

## Run tests then build — mirrors the CI gate; run this before pushing a release
check: test build

## Flash over USB using auto-reset sequencing (requires: pip install esptool pyserial)
## Override port: make flash PORT=/dev/ttyACM0
flash:
	@test -f $(APP_BIN) || (echo "No build output found — run 'make build' first"; exit 1)
	python3 scripts/flash.py \
	  --port   $(PORT) \
	  --baud   $(BAUD) \
	  --bootloader      $(BOOT_BIN) \
	  --partition-table $(PART_BIN) \
	  --app    $(APP_BIN)

## Flash over Wi-Fi OTA — no USB cable required (requires: curl)
## Usage: make flash-ota IP=192.168.1.x
flash-ota:
	@test -f $(APP_BIN) || (echo "No build output found — run 'make build' first"; exit 1)
	@test -n "$(IP)"    || (echo "Usage: make flash-ota IP=<device-ip>"; exit 1)
	curl -X POST "http://$(IP)/ota/upload" \
	  -H "Content-Type: application/octet-stream" \
	  --data-binary @$(APP_BIN) \
	  --progress-bar

## Remove build artifacts
clean:
	rm -rf build/

help:
	@echo ""
	@echo "Usage: make <target> [VAR=value]"
	@echo ""
	@echo "Targets:"
	@echo "  test        Run native unit tests (cmake + Unity, no Docker)"
	@echo "  build       Build firmware in Docker (IDF_VERSION=$(IDF_VERSION))"
	@echo "  check       Run test + build — mirrors the CI gate"
	@echo "  flash       Flash over USB   (PORT=$(PORT)  BAUD=$(BAUD))"
	@echo "  flash-ota   Flash over Wi-Fi (IP=<device-ip>)"
	@echo "  clean       Remove build/ directory"
	@echo ""
