#pragma once

#include <Arduino.h>

#include "../core/AppState.h"

void autoChangeCheck(AppState& state);

void uptimeCheck(AppState& state);

void webServerWatchdog(AppState& state, unsigned long watchdogMs);
