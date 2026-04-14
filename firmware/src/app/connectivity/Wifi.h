#pragma once

#include <core/CWPreferences.h>

#include "../core/AppState.h"

void preconnectWifiBeforeDisplay(const ClockwiseParams* prefs);

bool connectWifiAndTime(AppState& state, ClockwiseParams* prefs);
