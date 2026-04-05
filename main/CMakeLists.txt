set(CLOCKFACES cw-cf-0x01 cw-cf-0x02 cw-cf-0x03 cw-cf-0x04 cw-cf-0x05 cw-cf-0x06)

idf_component_register(
			SRC_DIRS "."
			INCLUDE_DIRS ${PROJECT_DIR}/firmware/src
			PRIV_REQUIRES ESP32-HUB75-MatrixPanel-I2S-DMA cw-commons cw-gfx-engine WiFiManager Improv-WiFi-Library arduino Adafruit-GFX-Library esp_https_ota mqtt app_update ${CLOCKFACES}
                       )

# Derive version from git tag at build time
# Falls back to "dev" if no tag is present (e.g. untagged branch)
execute_process(
    COMMAND git describe --tags --abbrev=0 --match "v*"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_TAG)
    set(GIT_TAG "dev")
endif()

# Strip leading 'v' so version is "2.0.0" not "v2.0.0"
string(REGEX REPLACE "^v" "" CW_VERSION "${GIT_TAG}")

message(STATUS "Clockwise Paradise firmware version: ${CW_VERSION}")

target_compile_options(${COMPONENT_TARGET} PUBLIC
    -DCW_FW_VERSION="${CW_VERSION}"
    -DCW_FW_NAME="Clockwise Paradise"
)
