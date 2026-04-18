#pragma once

#include <WiFi.h>
#include <functional>
#include <CWPreferences.h>
#include <CWOTA.h>
#include "../cw-logic/core/CWLogic.h"
#include "StatusController.h"
#include "CWWebUI.h"
#include "pages/HomePage.h"
#include "pages/ClockPage.h"
#include "pages/WidgetsPage.h"
#include "pages/SyncPage.h"
#include "pages/HardwarePage.h"
#include "pages/UpdatePage.h"
#include "SettingsWebPage.h"
#include "CWClockfaceDriver.h"
#include "esp_log.h"

#include "CWWebServer-OTA.h"
#include "CWWebServer-Settings.h"
#include "CWWebServer-Device.h"
#include "CWWebServer-Config.h"

// Route dispatcher: maps HTTP method+path to handler
struct CWWebServerRouter {

  static void processRequest(
    WiFiClient client,
    const String& method,
    const String& path,
    const String& key,
    const String& value,
    bool& force_restart,
    std::function<void(uint8_t)> onBrightnessChange,
    std::function<void(bool)> on24hFormatChange,
    std::function<bool(uint8_t)> onClockfaceSwitch,
    std::function<bool(const String&)> onWidgetSwitch,
    std::function<String()> onWidgetStateJson
  ) {
    if (method == "GET" && path == "/") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendHomePage(client);
    } else if (method == "GET" && path == "/clock") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendClockPage(client);
    } else if (method == "GET" && path == "/widgets") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendWidgetsPage(client);
    } else if (method == "GET" && path == "/sync") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendSyncPage(client);
    } else if (method == "GET" && path == "/hardware") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendHardwarePage(client);
    } else if (method == "GET" && path == "/update") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendUpdatePage(client);
    } else if (method == "GET" && path == "/legacy") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println(SETTINGS_PAGE);
    } else if (method == "GET" && path == "/get") {
      getCurrentSettings(client);
    } else if (method == "GET" && path == "/read") {
      if (key == "pin") {
        CWWebServerDevice::readPin(client, key, value.toInt());
      }
    } else if (method == "GET" && path == "/ota/check") {
      String result = CWOTA::getInstance()->checkOnly();
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print(result);
    } else if (method == "GET" && path == "/ota/status") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print(CWWebServerOTA::getOtaStatus());
    } else if (method == "POST" && path == "/ota/rollback") {
      CWWebServerOTA::handleOtaRollback(client);
      return;
    } else if (method == "POST" && path == "/ota/update") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"updating\",\"message\":\"OTA started - device will reboot\"}");
      client.flush();
      client.stop();
      CWOTA::getInstance()->checkAndUpdate();
      return;
    } else if (method == "GET" && path == "/api/clockfaces") {
      CWWebServerDevice::sendClockfaceList(client);
    } else if (method == "GET" && path == "/api/widgets") {
      CWWebServerDevice::sendWidgetList(client);
    } else if (method == "GET" && path == "/api/widget-state") {
      CWWebServerDevice::sendWidgetState(client, onWidgetStateJson);
    } else if (method == "POST" && path == "/api/widget/show") {
      handleWidgetShow(client, key, value, onWidgetSwitch);
      return;
    } else if (method == "GET" && path == "/backup") {
      CWWebServerConfig::exportConfig(client);
    } else if (method == "POST" && path == "/restore") {
      processRequest(client, "POST", "/set", key, value, force_restart, onBrightnessChange, on24hFormatChange, onClockfaceSwitch, onWidgetSwitch, onWidgetStateJson);
      return;
    } else if (method == "POST" && path == "/restart") {
      client.println("HTTP/1.0 204 No Content");
      force_restart = true;
    } else if (method == "POST" && path == "/set") {
      ClockwiseParams::getInstance()->load();
      String decodedValue = value;
      CWWebServerSettings::urlDecode(decodedValue);
      CWWebServerSettings::SetApplyResult result = CWWebServerSettings::handleSet(
        key,
        decodedValue,
        onBrightnessChange,
        on24hFormatChange,
        onClockfaceSwitch,
        onWidgetSwitch,
        force_restart
      );
      if (result == CWWebServerSettings::SetApplyResult::kPersistMutation) {
        ClockwiseParams::getInstance()->saveSetMutation(key);
      }
      client.println("HTTP/1.0 204 No Content");
    }
  }

private:
  static void handleWidgetShow(WiFiClient client, const String& key, String value, std::function<bool(const String&)> onWidgetSwitch) {
    if (key != "spec") {
      client.println("HTTP/1.0 400 Bad Request");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Missing spec query parameter\"}");
      return;
    }

    CWWebServerSettings::urlDecode(value);
    value.trim();

    if (!onWidgetSwitch) {
      client.println("HTTP/1.0 503 Service Unavailable");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Widget runtime unavailable\"}");
      return;
    }

    if (!onWidgetSwitch(value)) {
      client.println("HTTP/1.0 422 Unprocessable Entity");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Widget command rejected\"}");
      return;
    }

    client.println("HTTP/1.0 204 No Content");
  }

  static void getCurrentSettings(WiFiClient client) {
    ClockwiseParams::getInstance()->load();

    client.println("HTTP/1.0 204 No Content");

    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT, ClockwiseParams::getInstance()->displayBright);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MIN, ClockwiseParams::getInstance()->autoBrightMin);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MAX, ClockwiseParams::getInstance()->autoBrightMax);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN, ClockwiseParams::getInstance()->swapBlueGreen);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED, ClockwiseParams::getInstance()->swapBlueRed);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_LED_COLOR_ORDER, ClockwiseParams::getInstance()->ledColorOrder);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_REVERSE_PHASE, ClockwiseParams::getInstance()->reversePhase);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_AUTO_CHANGE, ClockwiseParams::getInstance()->autoChange);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT, ClockwiseParams::getInstance()->use24hFormat);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_LDR_PIN, ClockwiseParams::getInstance()->ldrPin);
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_TIME_ZONE, ClockwiseParams::getInstance()->timeZone.c_str());
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_WIFI_SSID, ClockwiseParams::getInstance()->wifiSsid.c_str());
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_NTP_SERVER, ClockwiseParams::getInstance()->ntpServer.c_str());
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_CANVAS_FILE, ClockwiseParams::getInstance()->canvasFile.c_str());
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_CANVAS_SERVER, ClockwiseParams::getInstance()->canvasServer.c_str());
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_MANUAL_POSIX, ClockwiseParams::getInstance()->manualPosix.c_str());
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION, ClockwiseParams::getInstance()->displayRotation);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_DRIVER, ClockwiseParams::getInstance()->driver);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_I2CSPEED, ClockwiseParams::getInstance()->i2cSpeed);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_E_PIN, ClockwiseParams::getInstance()->E_pin);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_AUTO_CHANGE, ClockwiseParams::getInstance()->autoChange);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_BRIGHT_METHOD, ClockwiseParams::getInstance()->brightMethod);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_START_H, ClockwiseParams::getInstance()->nightStartH);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_START_M, ClockwiseParams::getInstance()->nightStartM);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_END_H, ClockwiseParams::getInstance()->nightEndH);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_END_M, ClockwiseParams::getInstance()->nightEndM);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_BRIGHT, ClockwiseParams::getInstance()->nightBright);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_MODE, ClockwiseParams::getInstance()->nightMode);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_LEVEL, ClockwiseParams::getInstance()->nightLevel);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_TRIGGER, ClockwiseParams::getInstance()->nightTrigger);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_LDR_THRES, ClockwiseParams::getInstance()->nightLdrThres);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_ACTION, ClockwiseParams::getInstance()->nightAction);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_NIGHT_MIN_BRT, ClockwiseParams::getInstance()->nightMinBr);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_SUPER_COLOR, ClockwiseParams::getInstance()->superColor);
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_BIGCLOCK_SERVER, ClockwiseParams::getInstance()->bigclockServer.c_str());
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_BIGCLOCK_FILE, ClockwiseParams::getInstance()->bigclockFile.c_str());
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_TOTAL_DAYS, ClockwiseParams::getInstance()->totalDays);
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_MQTT_ENABLED, ClockwiseParams::getInstance()->mqttEnabled);
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_MQTT_BROKER, ClockwiseParams::getInstance()->mqttBroker.c_str());
    client.printf("X-%s: %d\r\n", ClockwiseParams::getInstance()->PREF_MQTT_PORT, ClockwiseParams::getInstance()->mqttPort);
    client.printf("X-%s: %s\r\n", ClockwiseParams::getInstance()->PREF_MQTT_PREFIX, ClockwiseParams::getInstance()->mqttPrefix.c_str());

    client.printf("X-%s: %d\r\n", "clockfaceIndex", ClockwiseParams::getInstance()->clockFaceIndex);
    client.printf("X-%s: %s\r\n", "clockfaceName", CWDriverRegistry::name(ClockwiseParams::getInstance()->clockFaceIndex));
    client.printf("X-%s: %s\r\n", "activeWidget", ClockwiseParams::getInstance()->activeWidget.c_str());
    client.printf("X-%s: %s\r\n", "CW_FW_VERSION", CW_FW_VERSION);
    client.printf("X-%s: %s\r\n", "CW_FW_NAME", CW_FW_NAME);
    client.printf("X-%s: %s\r\n", "CLOCKFACE_NAME", CLOCKFACE_NAME);
    client.printf("X-%s: %s\r\n", "wifiIP", WiFi.localIP().toString().c_str());
    uint64_t uptimeSec = CWWebServerDevice::getUptimeSec();
    client.printf("X-%s: %d\r\n", "uptimeSec", (uint32_t)uptimeSec);
    client.println();
  }
};
