#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include "../cw-logic/core/CWLogic.h"
#include "esp_log.h"

struct CWWebServerSettings {
  static constexpr unsigned long SET_REQUEST_BODY_RECEIVE_WINDOW_MS = cw::logic::kSetRequestBodyReceiveWindowMs;

  // ── Setting descriptors (pointer-to-member, type-safe) ──────────────────
  struct SettU8  { const char* k; uint8_t  ClockwiseParams::*f; };
  struct SettU16 { const char* k; uint16_t ClockwiseParams::*f; };
  struct SettU32 { const char* k; uint32_t ClockwiseParams::*f; };
  struct SettB   { const char* k; bool     ClockwiseParams::*f; };
  struct SettS   { const char* k; String   ClockwiseParams::*f; };

  static bool parseEncodedAssignment(const String& encoded, String& key, String& value) {
    std::string decodedKey;
    std::string decodedValue;
    bool parsed = cw::logic::parseSetEncodedAssignment(encoded.c_str(), decodedKey, decodedValue);
    key = decodedKey.c_str();
    value = decodedValue.c_str();
    return parsed;
  }

  static void urlDecode(String& v) {
    v = cw::logic::urlDecodeCopy(v.c_str()).c_str();
  }

  static cw::logic::RequestBodyReadResult readSetRequestBody(WiFiClient& client, int contentLength) {
    if (contentLength <= 0) {
      return {"", true};
    }

    return cw::logic::readRequestBodyWithinWindow(
      static_cast<size_t>(contentLength),
      SET_REQUEST_BODY_RECEIVE_WINDOW_MS,
      [&client]() {
        return client.available();
      },
      [&client](std::string& body, size_t remaining) {
        size_t appended = 0;
        while (appended < remaining && client.available() > 0) {
          const int next = client.read();
          if (next < 0) break;
          body.push_back(static_cast<char>(next));
          ++appended;
        }
        return appended;
      },
      []() {
        delay(1);
      },
      []() {
        return millis();
      }
    );
  }

  /**
   * Apply a single URL-decoded key=value pair to ClockwiseParams.
   * Special keys (with runtime callbacks) are handled via callback references;
   * everything else falls through to compact lookup tables.
   *
   * Callbacks are captured as function pointers passed from the main server instance.
   */
  static bool handleSet(
    const String& key,
    const String& value,
    std::function<void(uint8_t)> onBrightnessChange,
    std::function<void(bool)> on24hFormatChange,
    std::function<void(uint8_t)> onClockfaceSwitch,
    std::function<bool(const String&)> onWidgetSwitch,
    bool& force_restart
  ) {
    auto* p = ClockwiseParams::getInstance();

    // ── Special keys with side-effects ──
    if (key == "displayBright") {
      p->displayBright = (uint8_t)value.toInt();
      if (onBrightnessChange) onBrightnessChange(p->displayBright);
      return true;
    }
    if (key == "use24hFormat") {
      p->use24hFormat = (value == "1");
      if (on24hFormatChange) on24hFormatChange(p->use24hFormat);
      return true;
    }
    if (key == "clockFaceIndex") {
      uint8_t idx = (uint8_t)value.toInt();
      p->clockFaceIndex = idx;
      p->activeWidget = "clock";
      if (onClockfaceSwitch) onClockfaceSwitch(idx);
      else force_restart = true;
      return true;
    }
    if (key == "activeWidget") {
      String normalized = value;
      normalized.toLowerCase();
      if (onWidgetSwitch) {
        if (!onWidgetSwitch(normalized)) {
          ESP_LOGW("Web", "Widget '%s' not activated", normalized.c_str());
          return false;
        }
      } else {
        p->activeWidget = normalized;
        force_restart = true;
      }
      return true;
    }
    if (key == "autoBright") {
      p->autoBrightMin = value.substring(0, 4).toInt();
      p->autoBrightMax = value.substring(5, 9).toInt();
      return true;
    }
    // Legacy colour swap — also updates ledColorOrder
    if (key == "swapBlueGreen") {
      p->swapBlueGreen = (value == "1");
      p->ledColorOrder = (value == "1") ? ClockwiseParams::LED_ORDER_RBG
                       : (p->swapBlueRed ? ClockwiseParams::LED_ORDER_GBR
                                         : ClockwiseParams::LED_ORDER_RGB);
      return true;
    }
    if (key == "swapBlueRed") {
      p->swapBlueRed = (value == "1");
      p->ledColorOrder = (value == "1") ? ClockwiseParams::LED_ORDER_GBR
                       : (p->swapBlueGreen ? ClockwiseParams::LED_ORDER_RBG
                                           : ClockwiseParams::LED_ORDER_RGB);
      return true;
    }

    // ── Table-driven simple settings ──
    static const SettU8 U8S[] = {
      { "ledColorOrder",   &ClockwiseParams::ledColorOrder },
      { "autoChange",      &ClockwiseParams::autoChange },
      { "ldrPin",          &ClockwiseParams::ldrPin },
      { "displayRotation", &ClockwiseParams::displayRotation },
      { "driver",          &ClockwiseParams::driver },
      { "E_pin",           &ClockwiseParams::E_pin },
      { "brightMethod",    &ClockwiseParams::brightMethod },
      { "nightStartH",     &ClockwiseParams::nightStartH },
      { "nightStartM",     &ClockwiseParams::nightStartM },
      { "nightEndH",       &ClockwiseParams::nightEndH },
      { "nightEndM",       &ClockwiseParams::nightEndM },
      { "nightBright",     &ClockwiseParams::nightBright },
      { "nightMode",       &ClockwiseParams::nightMode },
      { "nightLevel",      &ClockwiseParams::nightLevel },
      { "nightTrig",       &ClockwiseParams::nightTrigger },
      { "nightAction",     &ClockwiseParams::nightAction },
      { "nightMinBr",      &ClockwiseParams::nightMinBr },
    };
    for (const auto& s : U8S) if (key == s.k) { p->*(s.f) = (uint8_t)value.toInt(); return true; }

    static const SettU16 U16S[] = {
      { "superColor", &ClockwiseParams::superColor },
      { "mqttPort",   &ClockwiseParams::mqttPort },
      { "nightLdrThr", &ClockwiseParams::nightLdrThres },
    };
    for (const auto& s : U16S) if (key == s.k) { p->*(s.f) = (uint16_t)value.toInt(); return true; }

    static const SettU32 U32S[] = {
      { "i2cSpeed", &ClockwiseParams::i2cSpeed },
    };
    for (const auto& s : U32S) if (key == s.k) { p->*(s.f) = (uint32_t)value.toInt(); return true; }

    static const SettB BS[] = {
      { "reversePhase", &ClockwiseParams::reversePhase },
      { "mqttEnabled",  &ClockwiseParams::mqttEnabled },
    };
    for (const auto& s : BS) if (key == s.k) { p->*(s.f) = (value == "1"); return true; }

    static const SettS SS[] = {
      { "wifiSsid",     &ClockwiseParams::wifiSsid },
      { "wifiPwd",      &ClockwiseParams::wifiPwd },
      { "timeZone",     &ClockwiseParams::timeZone },
      { "ntpServer",    &ClockwiseParams::ntpServer },
      { "canvasFile",   &ClockwiseParams::canvasFile },
      { "canvasServer", &ClockwiseParams::canvasServer },
      { "manualPosix",  &ClockwiseParams::manualPosix },
      { "mqttBroker",   &ClockwiseParams::mqttBroker },
      { "mqttUser",     &ClockwiseParams::mqttUser },
      { "mqttPass",     &ClockwiseParams::mqttPass },
      { "mqttPrefix",   &ClockwiseParams::mqttPrefix },
      { "activeWidget", &ClockwiseParams::activeWidget },
      { "bigclockSrv",  &ClockwiseParams::bigclockServer },
      { "bigclockFile", &ClockwiseParams::bigclockFile },
    };
    for (const auto& s : SS) if (key == s.k) { p->*(s.f) = value; return true; }

    return false;
  }
};
