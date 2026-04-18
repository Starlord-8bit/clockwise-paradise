#include "Mqtt.h"

#include <CWMqtt.h>
#include <CWPreferences.h>

void bindMqttCallbacks(AppState& state) {
  CWMqtt::getInstance()->onClockfaceSwitch = [&state](uint8_t idx) {
    return state.widgetManager.activateClockWidget(idx);
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
