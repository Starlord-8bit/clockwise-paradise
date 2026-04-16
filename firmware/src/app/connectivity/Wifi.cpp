#include "Wifi.h"

#include <display/StatusController.h>
#include <WiFi.h>
#include <esp_log.h>
extern "C" { void esp_brownout_init(void); void esp_brownout_disable(void); }

void preconnectWifiBeforeDisplay(const ClockwiseParams* prefs) {
  if (prefs->wifiSsid.isEmpty()) {
    return;
  }

  // Suppress the brownout detector during WiFi radio bring-up.
  // phy_init (RF calibration) draws a brief current spike that can trip the
  // detector on a marginal supply. The window is ~100 ms max.
  esp_brownout_disable();
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.begin(prefs->wifiSsid.c_str(), prefs->wifiPwd.c_str());
  esp_brownout_init();  // re-arm

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
