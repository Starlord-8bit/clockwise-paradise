#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <widgets/clockface/CWClockfaceDriver.h>
#include <core/CWDateTime.h>
#include <widgets/clockface/CWWidgetManager.h>
#include <connectivity/WiFiController.h>

struct AppState {
  MatrixPanel_I2S_DMA* display = nullptr;
  const CWClockfaceDriver* currentFace = nullptr;

  CWWidgetManager widgetManager;
  WiFiController wifi;
  CWDateTime dateTime;

  unsigned long autoBrightMillis = 0;
  int currentBrightSlot = -1;

  int lastAutoChangeDay = -1;
  bool nightModeActive = false;
  int lastUptimeDay = -1;

  unsigned long lastWebServerMillis = 0;
};
