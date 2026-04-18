#pragma once

#include "ImprovWiFiLibrary.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "CWWebServer.h"
#include "StatusController.h"
#include <WiFiManager.h>
#include "esp_log.h"
// Forward-declare brownout API (avoids fragile SDK include path).
extern "C" { void esp_brownout_init(void); void esp_brownout_disable(void); }

extern ImprovWiFi improvSerial;

struct WiFiController
{
  long elapsedTimeOffline = 0;
  bool connectionSucessfulOnce = false;

  // Callbacks set by main.cpp to pause/resume I2S DMA around the AP portal.
  // This mitigates I2S/WiFi RF interference (ESP32-HUB75-MatrixPanel-DMA#258):
  // the DMA clock runs at 8–20 MHz which can corrupt association-phase packets.
  inline static void (*onBeforePortal)() = nullptr;
  inline static void (*onAfterPortal)()  = nullptr;

  // -------------------------------------------------------------------------
  // Diagnostic helpers
  // -------------------------------------------------------------------------
  static const char* wifiStatusName(wl_status_t status)
  {
    switch (status) {
      case WL_IDLE_STATUS:     return "idle";
      case WL_NO_SSID_AVAIL:   return "no_ssid";
      case WL_SCAN_COMPLETED:  return "scan_completed";
      case WL_CONNECTED:       return "connected";
      case WL_CONNECT_FAILED:  return "connect_failed";
      case WL_CONNECTION_LOST: return "connection_lost";
      case WL_DISCONNECTED:    return "disconnected";
      default:                 return "unknown";
    }
  }

  static const char* wifiDisconnectReasonName(uint8_t reason)
  {
    switch (static_cast<wifi_err_reason_t>(reason)) {
      case WIFI_REASON_AUTH_EXPIRE:            return "AUTH_EXPIRE";
      case WIFI_REASON_AUTH_FAIL:              return "AUTH_FAIL";
      case WIFI_REASON_NO_AP_FOUND:            return "NO_AP_FOUND";
      case WIFI_REASON_ASSOC_EXPIRE:           return "ASSOC_EXPIRE";
      case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
      case WIFI_REASON_BEACON_TIMEOUT:         return "BEACON_TIMEOUT";
      case WIFI_REASON_HANDSHAKE_TIMEOUT:      return "HANDSHAKE_TIMEOUT";
      case WIFI_REASON_CONNECTION_FAIL:        return "CONNECTION_FAIL";
      default:                                 return "OTHER";
    }
  }

  // -------------------------------------------------------------------------
  // Improv-WiFi callbacks
  // -------------------------------------------------------------------------
  static void onImprovWiFiErrorCb(ImprovTypes::Error err)
  {
    ClockwiseWebServer::getInstance()->stopWebServer();
    StatusController::getInstance()->blink_led(2000, 3);
  }

  static void onImprovWiFiConnectedCb(const char *ssid, const char *password)
  {
    ClockwiseParams::getInstance()->load();
    ClockwiseParams::getInstance()->wifiSsid = String(ssid);
    ClockwiseParams::getInstance()->wifiPwd  = String(password);
    ClockwiseParams::getInstance()->save();

    ClockwiseWebServer::getInstance()->startWebServer();

    if (MDNS.begin("clockwise"))
    {
      MDNS.addService("http", "tcp", 80);
    }
  }

  // -------------------------------------------------------------------------
  // Runtime helpers
  // -------------------------------------------------------------------------
  bool isConnected()
  {
    if (improvSerial.isConnected()) {
      elapsedTimeOffline = 0;
      return true;
    } else {
      if (elapsedTimeOffline == 0 && !connectionSucessfulOnce)
        elapsedTimeOffline = millis();

      if ((millis() - elapsedTimeOffline) > 1000 * 60 * 5)
        StatusController::getInstance()->forceRestart();

      return false;
    }
  }

  static void handleImprovWiFi()
  {
    improvSerial.handleSerial();
  }

  // Synchronously wait up to timeoutMs for WL_CONNECTED.
  static bool waitForStationConnection(uint32_t timeoutMs)
  {
    const unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
      delay(200);
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

  // -------------------------------------------------------------------------
  // AP config portal
  // -------------------------------------------------------------------------
  bool alternativeSetupMethod()
  {
    // Per ESP32-HUB75-MatrixPanel-DMA#258: I2S DMA at 8–20 MHz interferes with
    // WiFi association. Pause the display engine before starting the portal so
    // the RF environment is clean during credential exchange.
    if (onBeforePortal) { onBeforePortal(); }

    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(600); // 10 min window

    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.disconnect();
    delay(150);
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    delay(250);

    ESP_LOGW("WiFi", "Starting WiFiManager AP portal: Clockwise-Wifi");
    bool success = wifiManager.startConfigPortal("Clockwise-Wifi");

    if (success)
    {
      // Use WiFiManager getters — WiFi.psk() can be stale/empty after portal.
      const String savedSsid = wifiManager.getWiFiSSID(false);
      const String savedPass = wifiManager.getWiFiPass(false);
      ESP_LOGI("WiFi", "Portal: saved SSID='%s' (pass len=%u)",
               savedSsid.c_str(), savedPass.length());
      onImprovWiFiConnectedCb(savedSsid.c_str(), savedPass.c_str());
      ESP_LOGI("WiFi", "Connected via portal to %s, IP %s",
               WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      connectionSucessfulOnce = true;
    }
    else
    {
      ESP_LOGE("WiFi", "WiFiManager portal timed out");
    }

    if (onAfterPortal) { onAfterPortal(); }
    return success;
  }

  // -------------------------------------------------------------------------
  // begin() — called after DMA is already running
  // -------------------------------------------------------------------------
  bool begin()
  {
    // Fast path: preconnectWifiBeforeDisplay() associated before I2S DMA started.
    // Do NOT call WiFi.disconnect() here — that would throw away the clean
    // pre-DMA association and force a reconnect in a noisy RF environment.
    if (WiFi.status() == WL_CONNECTED) {
      connectionSucessfulOnce = true;
      startConnectedServices();
      ESP_LOGI("WiFi", "Pre-connected to %s, IP %s",
               WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      return true;
    }

    // Attach disconnect-reason logging before touching radio state.
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
      const uint8_t reason = info.wifi_sta_disconnected.reason;
      ESP_LOGW("WiFi", "STA disconnected: status=%s(%d) reason=%s(%u)",
               wifiStatusName(WiFi.status()), static_cast<int>(WiFi.status()),
               wifiDisconnectReasonName(reason), reason);
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Brownout guard: DMA is already running; combined RF+DMA current can trip
    // the brownout detector during WiFi.mode() radio reconfiguration.
    esp_brownout_disable();
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.disconnect();
    esp_brownout_init();  // re-arm

    improvSerial.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, CW_FW_NAME, CW_FW_VERSION, "Clockwise");
    improvSerial.onImprovError(onImprovWiFiErrorCb);
    improvSerial.onImprovConnected(onImprovWiFiConnectedCb);

    ClockwiseParams::getInstance()->load();

    if (!ClockwiseParams::getInstance()->wifiSsid.isEmpty())
    {
      ESP_LOGI("WiFi", "Connecting to saved SSID '%s'",
               ClockwiseParams::getInstance()->wifiSsid.c_str());

      // Stage 1: check whether the pre-DMA association is still alive (4 s).
      if (waitForStationConnection(4000)) {
        connectionSucessfulOnce = true;
        startConnectedServices();
        ESP_LOGI("WiFi", "Connected to %s, IP %s",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
      }

      // Stage 2: explicit reconnect with saved credentials (12 s).
      WiFi.disconnect();
      delay(200);
      WiFi.begin(ClockwiseParams::getInstance()->wifiSsid.c_str(),
                 ClockwiseParams::getInstance()->wifiPwd.c_str());

      if (waitForStationConnection(12000)) {
        connectionSucessfulOnce = true;
        startConnectedServices();
        ESP_LOGI("WiFi", "Connected to %s, IP %s",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
      }

      ESP_LOGW("WiFi", "Saved SSID '%s' did not connect, status=%s(%d)",
               ClockwiseParams::getInstance()->wifiSsid.c_str(),
               wifiStatusName(WiFi.status()), static_cast<int>(WiFi.status()));
    }

    StatusController::getInstance()->wifiConnectionFailed("AP: Clockwise-Wifi");
    ESP_LOGW("WiFi", "Starting config AP portal");
    if (alternativeSetupMethod()) {
      return true;
    }

    StatusController::getInstance()->wifiConnectionFailed("WiFi setup timeout");
    return false;
  }
};

