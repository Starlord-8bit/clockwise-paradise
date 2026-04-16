#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "esp_ota_ops.h"
#include <core/CWPreferences.h>
#include <core/CWLogic.h>
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

static void markRunningFirmwareValidAfterSuccessfulBoot()
{
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (running == nullptr) {
    ESP_LOGW("OTA", "Could not resolve running partition; skipping rollback-window close");
    return;
  }

  esp_ota_img_states_t runningState = ESP_OTA_IMG_UNDEFINED;
  if (esp_ota_get_state_partition(running, &runningState) != ESP_OK) {
    ESP_LOGW("OTA", "Could not read OTA state; skipping rollback-window close");
    return;
  }

  if (!cw::ota::shouldMarkValid(static_cast<uint32_t>(runningState))) {
    return;
  }

  const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    ESP_LOGI("OTA", "Firmware marked valid after successful WiFi boot");
  } else {
    ESP_LOGW("OTA", "Failed to close rollback window: %s", esp_err_to_name(err));
  }
}

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

  StatusController::getInstance()->wifiConnecting();
  if (!connectWifiAndTime(appState, p)) {
    ESP_LOGE("Boot", "Startup failed: WiFi/time setup did not complete");
    StatusController::getInstance()->forceRestart("WiFi setup timeout or invalid credentials");
  }

  markRunningFirmwareValidAfterSuccessfulBoot();

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
