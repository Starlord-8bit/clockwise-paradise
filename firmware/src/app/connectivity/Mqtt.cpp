#include "Mqtt.h"

#include <connectivity/CWMqtt.h>
#include <core/CWPreferences.h>

void bindMqttCallbacks(AppState& state) {
  CWMqtt::getInstance()->onClockfaceSwitch = [&state](uint8_t idx) {
    if (state.widgetManager.activateClockWidget(idx)) {
      ClockwiseParams::getInstance()->clockFaceIndex = idx;
      ClockwiseParams::getInstance()->saveClockfaceIndex();
    }
  };

  CWMqtt::getInstance()->onWidgetSwitch = [&state](const String& widgetName) {
    auto* prefs = ClockwiseParams::getInstance();
    return state.widgetManager.activateWidgetByName(widgetName, prefs->clockFaceIndex);
  };

  CWMqtt::getInstance()->onBrightnessChange = [&state](uint8_t bright) {
    if (ClockwiseParams::getInstance()->brightMethod == 2) {
      state.display->setBrightness8(bright);
    }
  };
}

void mqttLoop() {
  CWMqtt::getInstance()->loop();
}
