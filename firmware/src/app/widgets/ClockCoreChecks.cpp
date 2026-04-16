#include "ClockCoreChecks.h"

#include <widgets/clockface/CWClockfaceDriver.h>
#include <core/CWPreferences.h>
#include <core/CWLogic.h>
#include <web/CWWebServer.h>
#include <esp_log.h>

void autoChangeCheck(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();
  if (prefs->autoChange == ClockwiseParams::AUTO_CHANGE_OFF ||
      !state.wifi.connectionSucessfulOnce) {
    return;
  }

  int today = state.dateTime.getDay();
  if (state.lastAutoChangeDay == -1) {
    state.lastAutoChangeDay = today;
    return;
  }

  if (today != state.lastAutoChangeDay) {
    state.lastAutoChangeDay = today;
    uint8_t next = cw::nextAutoChangeIndex(
        prefs->clockFaceIndex, CWDriverRegistry::COUNT,
        prefs->autoChange, (uint8_t)random(CWDriverRegistry::COUNT));

    if (state.widgetManager.activateClockWidget(next)) {
      prefs->clockFaceIndex = next;
      prefs->activeWidget = CWWidgetManager::WIDGET_CLOCK;
      prefs->save();
      ESP_LOGI("AUTO", "Day changed - switched to clockface %d", next);
    }
  }
}

void uptimeCheck(AppState& state) {
  int today = state.dateTime.getDay();
  if (state.lastUptimeDay == -1) {
    state.lastUptimeDay = today;
    return;
  }

  if (today != state.lastUptimeDay) {
    state.lastUptimeDay = today;
    ClockwiseParams::getInstance()->totalDays++;
    ClockwiseParams::getInstance()->save();
  }
}

void webServerWatchdog(AppState& state, unsigned long watchdogMs) {
  if (millis() - state.lastWebServerMillis <= watchdogMs) {
    return;
  }

  state.lastWebServerMillis = millis();
  ClockwiseWebServer::getInstance()->stopWebServer();
  ClockwiseWebServer::getInstance()->startWebServer();
}
