#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "esp_ota_ops.h"
#include <core/CWPreferences.h>
#include <web/CWWebServer.h>
#include <Locator.h>
#include <display/StatusController.h>
#include "esp_log.h"

#include "app/core/AppState.h"
#include "app/core/DisplayControl.h"
#include "app/connectivity/Wifi.h"
#include "app/connectivity/HomeAssistant.h"
#include "app/connectivity/Mqtt.h"
#include "app/widgets/ClockCore.h"
#include "app/widgets/ClockCoreChecks.h"
#include "app/web/WebUi.h"

#define WEB_SERVER_WATCHDOG_MS 300000  // 5 minutes
#define ESP32_LED_BUILTIN 2

WiFiServer server(80);
ImprovWiFi improvSerial(&Serial);

static AppState appState;

void setup()
{
  Serial.begin(115200);
  esp_log_level_set("wifi", ESP_LOG_WARN);
  esp_log_level_set("wifi_init", ESP_LOG_WARN);
  esp_log_level_set("gpio", ESP_LOG_ERROR);

  pinMode(ESP32_LED_BUILTIN, OUTPUT);
  StatusController::getInstance()->blink_led(5, 100);

  ClockwiseParams::getInstance()->load();
  auto* p = ClockwiseParams::getInstance();

  pinMode(p->ldrPin, INPUT);

  displaySetup(appState, p->ledColorOrder, p->reversePhase, p->displayBright,
               p->displayRotation, p->driver, p->i2cSpeed, p->E_pin);

  Locator::provide(appState.display);

  configureWidgetManager(appState, p);
  bindWebUiCallbacks(appState);

  if (p->brightMethod == 2) {
    appState.display->setBrightness8(p->displayBright);
  }

  StatusController::getInstance()->clockwiseLogo();
  delay(1000);

  // Mark OTA image valid before wifi.begin(); avoids rollback on AP timeout restarts.
  esp_ota_mark_app_valid_cancel_rollback();
  ESP_LOGI("OTA", "Firmware marked valid — rollback window closed");

  StatusController::getInstance()->wifiConnecting();
  if (!connectWifiAndTime(appState, p)) {
    ESP_LOGE("Boot", "Startup failed: WiFi/time setup did not complete");
    StatusController::getInstance()->forceRestart("WiFi setup timeout or invalid credentials");
  }

  activateStartupWidget(appState, p);

  startHomeAssistantIntegration();
  bindMqttCallbacks(appState);

  String bootTs = appState.dateTime.getFormattedTime("Y-m-d H:i:s");
  Serial.printf("Successful boot at %s\n", bootTs.c_str());

}

void loop()
{
  appState.wifi.handleImprovWiFi();

  if (appState.wifi.isConnected()) {
    ClockwiseWebServer::getInstance()->handleHttpRequest();
    ezt::events();
    webServerWatchdog(appState, WEB_SERVER_WATCHDOG_MS);
    mqttLoop();
  }

  appState.widgetManager.update();

  if (appState.wifi.connectionSucessfulOnce) {
    uptimeCheck(appState);
    autoChangeCheck(appState);
  }

  automaticBrightControl(appState);
  nightModeCheck(appState);

  // Keep a 1 ms yield for IDLE1; prevents loopTask watchdog panics.
  // vTaskDelay(0) does not yield on ESP-IDF v4.4.
  delay(1);
}
