#include "WebUi.h"

#include <widgets/clockface/CWClockfaceDriver.h>
#include <connectivity/CWMqtt.h>
#include <web/CWWebServer.h>

void bindWebUiCallbacks(AppState& state) {
  ClockwiseWebServer::getInstance()->onClockfaceSwitch = [&state](uint8_t idx) {
    if (state.widgetManager.activateClockWidget(idx)) {
      ClockwiseParams::getInstance()->clockFaceIndex = idx;
      ClockwiseParams::getInstance()->saveClockfaceIndex();
    }
  };

  ClockwiseWebServer::getInstance()->onWidgetSwitch = [&state](const String& widgetName) {
    const auto* prefs = ClockwiseParams::getInstance();
    return state.widgetManager.activateWidgetByName(widgetName, prefs->clockFaceIndex);
  };

  ClockwiseWebServer::getInstance()->onWidgetStateJson = [&state]() {
    const auto* prefs = ClockwiseParams::getInstance();
    String json = "{";
    json += "\"activeWidget\":\"" + String(state.widgetManager.activeWidgetName()) + "\"";
    json += ",\"timerRemainingSec\":" + String(state.widgetManager.timerRemainingSeconds());
    json += ",\"clockfaceName\":\"" + String(CWDriverRegistry::name(prefs->clockFaceIndex)) + "\"";
    json += ",\"canReturnToClock\":" +
            String(state.widgetManager.canReturnToClock() ? "true" : "false");
    json += "}";
    return json;
  };

  ClockwiseWebServer::getInstance()->onBrightnessChange = [&state](uint8_t bright) {
    if (ClockwiseParams::getInstance()->brightMethod == 2) {
      state.display->setBrightness8(bright);
    }
  };

  ClockwiseWebServer::getInstance()->on24hFormatChange = [&state](bool use24) {
    state.dateTime.set24hFormat(use24);
  };

  ClockwiseWebServer::getInstance()->onTimeSyncSettingsChange = [&state]() {
    auto* prefs = ClockwiseParams::getInstance();
    state.dateTime.begin(prefs->timeZone.c_str(), prefs->use24hFormat,
                         prefs->ntpServer.c_str(), prefs->manualPosix.c_str());
  };

  ClockwiseWebServer::getInstance()->onMqttSettingsChange = []() {
    CWMqtt::getInstance()->reconfigureFromPreferences();
  };
}
