#pragma once

#include "ImprovWiFiLibrary.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "CWWebServer.h"
#include "StatusController.h"
#include <WiFiManager.h>
#include "esp_log.h"

ImprovWiFi improvSerial(&Serial);

struct WiFiController
{
  long elapsedTimeOffline = 0;
  bool connectionSucessfulOnce;

  static void onImprovWiFiErrorCb(ImprovTypes::Error err)
  {
    ClockwiseWebServer::getInstance()->stopWebServer();
    StatusController::getInstance()->blink_led(2000, 3);
  }

  static void onImprovWiFiConnectedCb(const char *ssid, const char *password)
  {
    ClockwiseParams::getInstance()->load();
    ClockwiseParams::getInstance()->wifiSsid = String(ssid);
    ClockwiseParams::getInstance()->wifiPwd = String(password);
    ClockwiseParams::getInstance()->save();

    ClockwiseWebServer::getInstance()->startWebServer();

    if (MDNS.begin("clockwise"))
    {
      MDNS.addService("http", "tcp", 80);
    }
  }

  bool isConnected()
  {
    if (improvSerial.isConnected()) {
      elapsedTimeOffline = 0;
      return true;
    } else {
      if (elapsedTimeOffline == 0 && !connectionSucessfulOnce)
        elapsedTimeOffline = millis();

      if ((millis() - elapsedTimeOffline) > 1000 * 60 * 5)  // restart if clockface is not showed and is 5min offline
        StatusController::getInstance()->forceRestart();

      return false;
    }
  }

  static void handleImprovWiFi()
  {
    improvSerial.handleSerial();
  }

  bool alternativeSetupMethod()
  {
    WiFiManager wifiManager;
     wifiManager.setConfigPortalTimeout(600); // 10 min window to configure WiFi via AP

    bool success = wifiManager.startConfigPortal("Clockwise-Wifi");

    if (success)
    {
      onImprovWiFiConnectedCb(WiFi.SSID().c_str(), WiFi.psk().c_str());
      ESP_LOGI("WiFi", "Connected via WiFiManager to %s, IP address %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      connectionSucessfulOnce = success;
    }

    return success;
  }

  bool begin()
  {
    // Fast path: WiFi was pre-connected in main.cpp setup() before the I2S/DMA driver
    // started. Avoid the disconnect() + reconnect cycle — that would drop the
    // association established in the clean RF environment before DMA was active.
    if (WiFi.status() == WL_CONNECTED) {
      connectionSucessfulOnce = true;
      ClockwiseWebServer::getInstance()->startWebServer();
      if (MDNS.begin("clockwise")) {
        MDNS.addService("http", "tcp", 80);
      }
      ESP_LOGI("WiFi", "Pre-connected to %s, IP address %s",
               WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);  // max TX power for DMA-noisy environment
    WiFi.disconnect();

    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, CW_FW_NAME, CW_FW_VERSION, "Clockwise");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);

    ClockwiseParams::getInstance()->load();

    if (!ClockwiseParams::getInstance()->wifiSsid.isEmpty())
    {
      if (improvSerial.tryConnectToWifi(ClockwiseParams::getInstance()->wifiSsid.c_str(), ClockwiseParams::getInstance()->wifiPwd.c_str()))
      {
        connectionSucessfulOnce = true;
        ClockwiseWebServer::getInstance()->startWebServer();
        ESP_LOGI("WiFi", "Connected to %s, IP address %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
      }
    }

    // "AP: Clockwise-Wifi" tells the user exactly which AP to connect to for configuration
    StatusController::getInstance()->wifiConnectionFailed("AP: Clockwise-Wifi");
    ESP_LOGW("WiFi", "Saved WiFi credentials failed or absent — starting config AP");
    if (alternativeSetupMethod()) {
      // User configured WiFi via AP portal — connectionSucessfulOnce already set true
      return true;
    }

    // AP portal timed out without configuration — caller must restart
    StatusController::getInstance()->wifiConnectionFailed("WiFi setup timeout");
    return false;
  }
};
