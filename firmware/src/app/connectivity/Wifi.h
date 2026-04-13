#pragma once

#include <CWPreferences.h>

#include "../core/AppState.h"

void preconnectWifiBeforeDisplay(const ClockwiseParams* prefs);

bool connectWifiAndTime(AppState& state, ClockwiseParams* prefs);
