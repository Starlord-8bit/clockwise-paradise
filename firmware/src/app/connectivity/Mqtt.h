#pragma once

#include "../core/AppState.h"

void bindMqttCallbacks(AppState& state);

void mqttLoop();
