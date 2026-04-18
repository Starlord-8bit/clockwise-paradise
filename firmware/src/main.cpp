#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "esp_ota_ops.h"
#include <CWPreferences.h>
#include <CWWebServer.h>
#include <Locator.h>
#include <StatusController.h>
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
  pinMode(ESP32_LED_BUILTIN, OUTPUT);
  StatusController::getInstance()->blink_led(5, 100);

  ClockwiseParams::getInstance()->load();
  auto* p = ClockwiseParams::getInstance();

  pinMode(p->ldrPin, INPUT);

  preconnectWifiBeforeDisplay(p);

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
    ESP_LOGW("WiFi", "AP config portal timed out without configuration - restarting");
    delay(3000);
    ESP.restart();
  }

  activateStartupWidget(appState, p);

  startHomeAssistantIntegration();
  bindMqttCallbacks(appState);

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

  if (appState.wifi.connectionSucessfulOnce) {
    appState.widgetManager.update();
    uptimeCheck(appState);
    autoChangeCheck(appState);
  }

  nightModeCheck(appState);
  automaticBrightControl(appState);

  // Keep a 1 ms yield for IDLE1; prevents loopTask watchdog panics.
  // vTaskDelay(0) does not yield on ESP-IDF v4.4.
  delay(1);
}
