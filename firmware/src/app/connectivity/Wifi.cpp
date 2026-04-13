#include "Wifi.h"

#include <StatusController.h>
#include <WiFi.h>
#include <esp_log.h>

void preconnectWifiBeforeDisplay(const ClockwiseParams* prefs) {
  if (prefs->wifiSsid.isEmpty()) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(prefs->wifiSsid.c_str(), prefs->wifiPwd.c_str());

  for (uint8_t i = 0; i < 80 && WiFi.status() != WL_CONNECTED; i++) {
    delay(100);
  }
  ESP_LOGI("WiFi", "Pre-DMA connect status: %d (3=OK)", WiFi.status());
}

bool connectWifiAndTime(AppState& state, ClockwiseParams* prefs) {
  if (!state.wifi.begin()) {
    return false;
  }

  StatusController::getInstance()->ntpConnecting();
  state.dateTime.begin(prefs->timeZone.c_str(), prefs->use24hFormat,
                       prefs->ntpServer.c_str(), prefs->manualPosix.c_str());
  return true;
}
