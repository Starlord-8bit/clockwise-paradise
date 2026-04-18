#include "WebUi.h"

#include <CWClockfaceDriver.h>
#include <CWWebServer.h>

void bindWebUiCallbacks(AppState& state) {
  ClockwiseWebServer::getInstance()->onClockfaceSwitch = [&state](uint8_t idx) {
    return state.widgetManager.activateClockWidget(idx);
  };

  ClockwiseWebServer::getInstance()->onWidgetSwitch = [&state](const String& widgetName) {
    auto* prefs = ClockwiseParams::getInstance();
    return state.widgetManager.activateWidgetByName(widgetName, prefs->clockFaceIndex);
  };

  ClockwiseWebServer::getInstance()->onWidgetStateJson = [&state]() {
    auto* prefs = ClockwiseParams::getInstance();
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
}
