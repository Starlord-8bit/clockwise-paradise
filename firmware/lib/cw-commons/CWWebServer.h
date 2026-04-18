#pragma once

#include <WiFi.h>
#include <functional>
#include <CWPreferences.h>

#ifndef CLOCKFACE_NAME
  #define CLOCKFACE_NAME "UNKNOWN"
#endif

extern WiFiServer server;

// Include all modular components
#include "CWWebServer-Core.h"

struct ClockwiseWebServer
{
  uint8_t pending_clockface_index = 255; // 255 = no pending switch

  // Callback set by main.cpp for live clockface switching without reboot.
  // Signature: void switchClockface(uint8_t index)
  // If null, falls back to save+reboot.
  std::function<void(uint8_t)> onClockfaceSwitch = nullptr;
  // Generic widget switch callback. Returns true if the widget was activated.
  std::function<bool(const String&)> onWidgetSwitch = nullptr;
  // Returns widget runtime state JSON.
  std::function<String()> onWidgetStateJson = nullptr;
  // Callback for live brightness apply (fixed-mode only).
  std::function<void(uint8_t)> onBrightnessChange = nullptr;
  // Callback for live 24h format toggle.
  std::function<void(bool)> on24hFormatChange = nullptr;

  // Internal core implementation
  CWWebServerCore core;

  static ClockwiseWebServer *getInstance()
  {
    static ClockwiseWebServer base;
    return &base;
  }

  void startWebServer()
  {
    core.startWebServer();
  }

  void stopWebServer()
  {
    core.stopWebServer();
  }

  void handleHttpRequest()
  {
    core.handleHttpRequest(
      core.force_restart,
      onBrightnessChange,
      on24hFormatChange,
      onClockfaceSwitch,
      onWidgetSwitch,
      onWidgetStateJson
    );
    if (core.force_restart) {
      StatusController::getInstance()->forceRestart();
    }
  }
};
