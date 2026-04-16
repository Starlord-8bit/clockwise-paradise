#pragma once

#include "ImprovWiFiLibrary.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "web/CWWebServer.h"
#include "display/StatusController.h"
#include <WiFiManager.h>
#include "esp_log.h"
extern "C" { void esp_brownout_init(void); void esp_brownout_disable(void); }

extern ImprovWiFi improvSerial;

struct WiFiController
{
  long elapsedTimeOffline = 0;
  bool connectionSucessfulOnce = false;
  inline static volatile bool s_wifiDiagnosticsAttached = false;
  inline static volatile bool s_suppressStaDisconnectLog = false;

  static const char* wifiStatusName(wl_status_t status)
  {
    switch (status) {
      case WL_IDLE_STATUS:    return "idle";
      case WL_NO_SSID_AVAIL:  return "no_ssid";
      case WL_SCAN_COMPLETED: return "scan_completed";
      case WL_CONNECTED:      return "connected";
      case WL_CONNECT_FAILED: return "connect_failed";
      case WL_CONNECTION_LOST:return "connection_lost";
      case WL_DISCONNECTED:   return "disconnected";
      default:                return "unknown";
    }
  }

  static const char* wifiDisconnectReasonName(uint8_t reason)
  {
    switch (static_cast<wifi_err_reason_t>(reason)) {
      case WIFI_REASON_AUTH_EXPIRE:            return "AUTH_EXPIRE";
      case WIFI_REASON_ASSOC_EXPIRE:           return "ASSOC_EXPIRE";
      case WIFI_REASON_ASSOC_TOOMANY:          return "ASSOC_TOOMANY";
      case WIFI_REASON_NOT_AUTHED:             return "NOT_AUTHED";
      case WIFI_REASON_NOT_ASSOCED:            return "NOT_ASSOCED";
      case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
      case WIFI_REASON_BEACON_TIMEOUT:         return "BEACON_TIMEOUT";
      case WIFI_REASON_NO_AP_FOUND:            return "NO_AP_FOUND";
      case WIFI_REASON_AUTH_FAIL:              return "AUTH_FAIL";
      case WIFI_REASON_ASSOC_FAIL:             return "ASSOC_FAIL";
      case WIFI_REASON_HANDSHAKE_TIMEOUT:      return "HANDSHAKE_TIMEOUT";
      case WIFI_REASON_CONNECTION_FAIL:        return "CONNECTION_FAIL";
      default:                                 return "OTHER";
    }
  }

  static void attachWiFiDiagnostics()
  {
    if (s_wifiDiagnosticsAttached) {
      return;
    }

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED || s_suppressStaDisconnectLog) {
        return;
      }

      const uint8_t reason = info.wifi_sta_disconnected.reason;
      ESP_LOGW("WiFi", "STA disconnected: status=%s(%d) reason=%s(%u)",
               wifiStatusName(WiFi.status()), WiFi.status(),
               wifiDisconnectReasonName(reason), reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
      String ip = WiFi.softAPIP().toString();
      ESP_LOGI("WiFi", "Config AP started: ssid=Clockwise-Wifi channel=%d ip=%s",
               WiFi.channel(), ip.c_str());
    }, ARDUINO_EVENT_WIFI_AP_START);

    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t) {
      ESP_LOGW("WiFi", "Config AP stopped");
    }, ARDUINO_EVENT_WIFI_AP_STOP);

    s_wifiDiagnosticsAttached = true;
  }

  static bool waitForStationConnection(uint32_t timeoutMs)
  {
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
      delay(250);
    }
    return WiFi.status() == WL_CONNECTED;
  }

  static void startConnectedServices()
  {
    ClockwiseWebServer::getInstance()->startWebServer();
    if (MDNS.begin("clockwise")) {
      MDNS.addService("http", "tcp", 80);
    }
  }

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

    startConnectedServices();
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
        StatusController::getInstance()->forceRestart("WiFi offline for more than 5 minutes");

      return false;
    }
  }

  static void handleImprovWiFi()
  {
    improvSerial.handleSerial();
  }

  static void prepareForConfigPortal()
  {
    s_suppressStaDisconnectLog = true;
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.disconnect();
    delay(150);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    delay(250);
  }

  bool alternativeSetupMethod()
  {
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(600); // 10 min window to configure WiFi via AP

    // Reset WiFi into a lower-power AP+STA state before launching the portal.
    // This avoids carrying a noisy/failed STA session into the AP startup path.
    prepareForConfigPortal();

    ESP_LOGW("WiFi", "Starting WiFiManager AP portal: Clockwise-Wifi");

    bool success = wifiManager.startConfigPortal("Clockwise-Wifi");
    s_suppressStaDisconnectLog = false;

    if (success)
    {
      const String savedSsid = wifiManager.getWiFiSSID(false);
      const String savedPass = wifiManager.getWiFiPass(false);
      onImprovWiFiConnectedCb(savedSsid.c_str(), savedPass.c_str());
      ESP_LOGI("WiFi", "Connected via WiFiManager to %s, IP address %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      connectionSucessfulOnce = success;
    }
    else
    {
      ESP_LOGE("WiFi", "WiFiManager AP portal failed/timed out");
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
      startConnectedServices();
      ESP_LOGI("WiFi", "Pre-connected to %s, IP address %s",
               WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      return true;
    }

    attachWiFiDiagnostics();

    // Suppress brownout detector during WiFi mode change; DMA is already
    // running so the combined current draw can exceed the detector threshold.
    esp_brownout_disable();
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.disconnect();
    esp_brownout_init();  // re-arm
    s_suppressStaDisconnectLog = false;

    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, CW_FW_NAME, CW_FW_VERSION, "Clockwise");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);

    ClockwiseParams::getInstance()->load();

    if (!ClockwiseParams::getInstance()->wifiSsid.isEmpty())
    {
      ESP_LOGI("WiFi", "Connecting to saved SSID %s", ClockwiseParams::getInstance()->wifiSsid.c_str());

      if (waitForStationConnection(4000)) {
        connectionSucessfulOnce = true;
        startConnectedServices();
        ESP_LOGI("WiFi", "Connected to %s, IP address %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
      }

      WiFi.disconnect();
      delay(200);
      WiFi.begin(ClockwiseParams::getInstance()->wifiSsid.c_str(), ClockwiseParams::getInstance()->wifiPwd.c_str());

      if (waitForStationConnection(12000)) {
        connectionSucessfulOnce = true;
        startConnectedServices();
        ESP_LOGI("WiFi", "Connected to %s, IP address %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
      }

  ESP_LOGW("WiFi", "Saved SSID %s did not connect, status=%s(%d)",
       ClockwiseParams::getInstance()->wifiSsid.c_str(),
       wifiStatusName(WiFi.status()), WiFi.status());
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
