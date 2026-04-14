#include "ClockCore.h"

#include <widgets/clockface/CWClockfaceDriver.h>

void configureWidgetManager(AppState& state, ClockwiseParams* prefs) {
  CWDriverRegistry::get(prefs->clockFaceIndex);
  state.widgetManager.begin(state.display, &state.dateTime, &state.currentFace);

  state.widgetManager.onWidgetChanged = [](const String& widgetName) {
    auto* params = ClockwiseParams::getInstance();
    String normalized = widgetName;
    normalized.toLowerCase();

    if (params->activeWidget != normalized) {
      params->activeWidget = normalized;
      params->saveActiveWidget();
    }
  };
}

void activateStartupWidget(AppState& state, ClockwiseParams* prefs) {
  if (state.widgetManager.activateWidgetByName(prefs->activeWidget, prefs->clockFaceIndex)) {
    return;
  }

  state.widgetManager.activateClockWidget(prefs->clockFaceIndex);
  prefs->activeWidget = CWWidgetManager::WIDGET_CLOCK;
  prefs->saveActiveWidget();
}
