#pragma once

#include <Arduino.h>

#include "AppState.h"

void displaySetup(AppState& state, uint8_t ledColorOrder, bool reversePhase,
                  uint8_t displayBright, uint8_t displayRotation,
                  uint8_t driver, uint32_t i2cSpeed, uint8_t ePin);

void automaticBrightControl(AppState& state);

void nightModeCheck(AppState& state);
