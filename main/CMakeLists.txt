set(CLOCKFACES cw-cf-0x01 cw-cf-0x02 cw-cf-0x03 cw-cf-0x04 cw-cf-0x05 cw-cf-0x06)

idf_component_register(
			SRC_DIRS "."
			INCLUDE_DIRS ${PROJECT_DIR}/firmware/src
			PRIV_REQUIRES ESP32-HUB75-MatrixPanel-I2S-DMA cw-commons cw-gfx-engine WiFiManager Improv-WiFi-Library arduino Adafruit-GFX-Library esp_https_ota mqtt app_update ${CLOCKFACES}
                       )

# Read version from version.txt — single source of truth across all build systems.
# Release-please (simple type) updates this file automatically on each release.
file(READ "${CMAKE_SOURCE_DIR}/version.txt" CW_VERSION)
string(STRIP "${CW_VERSION}" CW_VERSION)

message(STATUS "Clockwise Paradise firmware version: ${CW_VERSION}")

target_compile_options(${COMPONENT_TARGET} PUBLIC
    -DCW_FW_VERSION="${CW_VERSION}"
    -DCW_FW_NAME="Clockwise Paradise"
)
