#pragma once

#include <core/CWPreferences.h>

#include "../core/AppState.h"

void configureWidgetManager(AppState& state, const ClockwiseParams* prefs);

void activateStartupWidget(AppState& state, ClockwiseParams* prefs);
