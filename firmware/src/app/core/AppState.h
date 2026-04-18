#pragma once

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <CWClockfaceDriver.h>
#include <CWDateTime.h>
#include <CWWidgetManager.h>
#include <WiFiController.h>

struct AppState {
  MatrixPanel_I2S_DMA* display = nullptr;
  const CWClockfaceDriver* currentFace = nullptr;

  CWWidgetManager widgetManager;
  WiFiController wifi;
  CWDateTime dateTime;

  unsigned long autoBrightMillis = 0;
  int currentBrightness = -1;
  int currentBrightSlot = -1;

  int lastAutoChangeDay = -1;
  bool nightModeActive = false;
  int lastUptimeDay = -1;

  unsigned long lastWebServerMillis = 0;
};
