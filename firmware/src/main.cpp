#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "esp_log.h"
#include "esp_ota_ops.h"

// v3 clockface driver registry (replaces v2 dispatcher + class hierarchy)
#include <CWClockfaceDriver.h>
static const CWClockfaceDriver* currentFace = nullptr;

// Commons
#include <WiFiController.h>
#include <CWDateTime.h>
#include <CWPreferences.h>
#include <CWWebServer.h>
#include <CWWidgetManager.h>
#include <StatusController.h>
#include <Locator.h>
#include <CWMqtt.h>

#define MIN_BRIGHT_DISPLAY_ON 4
#define MIN_BRIGHT_DISPLAY_OFF 0

#define ESP32_LED_BUILTIN 2

MatrixPanel_I2S_DMA *dma_display = nullptr;
// clockface pointer declared above
CWWidgetManager widgetManager;
WiFiController wifi;
CWDateTime cwDateTime;

bool autoBrightEnabled;
long autoBrightMillis = 0;
uint8_t currentBrightSlot = -1;

// Auto-change clockface: track last day for midnight rollover
int lastAutoChangeDay = -1;

// Night mode state
bool nightModeActive = false;

// Uptime counter
int lastUptimeDay = -1;

// Web server watchdog
long lastWebServerMillis = 0;
#define WEB_SERVER_WATCHDOG_MS 300000  // 5 minutes

bool isValidI2SSpeed(uint32_t speed) {
  return speed == 8000000 || speed == 16000000 || speed == 20000000;
}

bool isValidDriver(uint32_t drv) {
  return drv >= 0 && drv <= 5;
}

/**
 * Returns true if current time falls inside the configured night window.
 * Handles midnight-wrapping windows (e.g. 22:00-07:00).
 */
bool isNightTime() {
  auto* p = ClockwiseParams::getInstance();
  int nowMins   = cwDateTime.getHour() * 60 + cwDateTime.getMinute();
  int startMins = p->nightStartH * 60 + p->nightStartM;
  int endMins   = p->nightEndH   * 60 + p->nightEndM;
  if (startMins < endMins) return nowMins >= startMins && nowMins < endMins;
  return nowMins >= startMins || nowMins < endMins;
}

void displaySetup(uint8_t ledColorOrder, bool reversePhase, uint8_t displayBright,
                  uint8_t displayRotation, uint8_t driver, uint32_t i2cSpeed, uint8_t E_pin)
{
  HUB75_I2S_CFG mxconfig(64, 64, 1);

  if (ledColorOrder == ClockwiseParams::LED_ORDER_RBG) {
    mxconfig.gpio.b1 = 26; mxconfig.gpio.b2 = 12;
    mxconfig.gpio.g1 = 27; mxconfig.gpio.g2 = 13;
  } else if (ledColorOrder == ClockwiseParams::LED_ORDER_GBR) {
    mxconfig.gpio.g1 = 25; mxconfig.gpio.g2 = 14;
    mxconfig.gpio.r1 = 26; mxconfig.gpio.r2 = 12;
  }

  mxconfig.gpio.e   = E_pin;
  mxconfig.clkphase = reversePhase;

  if (isValidDriver(driver))
    mxconfig.driver = static_cast<HUB75_I2S_CFG::shift_driver>(driver);
  if (isValidI2SSpeed(i2cSpeed))
    mxconfig.i2sspeed = static_cast<HUB75_I2S_CFG::clk_speed>(i2cSpeed);

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    // DMA init failed — blink indefinitely rather than crashing into null deref
    // on setBrightness8/clearScreen. Typically caused by hardware fault or matrix
    // connected during boot on SPI-interfering hardware configs.
    while (true) {
      digitalWrite(ESP32_LED_BUILTIN, HIGH); delay(100);
      digitalWrite(ESP32_LED_BUILTIN, LOW);  delay(100);
    }
  }
  dma_display->setBrightness8(displayBright);
  dma_display->clearScreen();
  dma_display->setRotation(displayRotation);
}

void automaticBrightControl()
{
  auto* p = ClockwiseParams::getInstance();
  uint8_t method = p->brightMethod;

  if (method == 2) return;  // fixed — nothing to do

  if (millis() - autoBrightMillis < 3000) return;
  autoBrightMillis = millis();

  if (method == 0 && p->autoBrightMax > 0) {
    // Auto-LDR
    int16_t currentValue = analogRead(p->ldrPin);
    uint16_t ldrMin = p->autoBrightMin;
    uint16_t ldrMax = p->autoBrightMax;
    const uint8_t minBright = (currentValue < ldrMin ? MIN_BRIGHT_DISPLAY_OFF : MIN_BRIGHT_DISPLAY_ON);
    uint8_t maxBright = p->displayBright;
    uint8_t slots    = 10;
    uint8_t mapLDR   = map(currentValue > ldrMax ? ldrMax : currentValue, ldrMin, ldrMax, 1, slots);
    uint8_t mapBright = map(mapLDR, 1, slots, minBright, maxBright);
    if (abs(currentBrightSlot - mapLDR) >= 2 || mapBright == 0) {
      dma_display->setBrightness8(mapBright);
      currentBrightSlot = mapLDR;
    }
  } else if (method == 1 && wifi.connectionSucessfulOnce) {
    // Time-based — requires cwDateTime to be initialised (only after NTP sync)
    uint8_t targetBright = isNightTime() ? p->nightBright : p->displayBright;
    if (currentBrightSlot != targetBright) {
      dma_display->setBrightness8(targetBright);
      currentBrightSlot = targetBright;
    }
  }
}

void nightModeCheck()
{
  auto* p = ClockwiseParams::getInstance();
  if (p->nightMode == 0) return;
  bool inNight = false;
  if (p->nightTrigger == 1) {
    inNight = analogRead(p->ldrPin) <= p->nightLdrThres;
  } else {
    if (!wifi.connectionSucessfulOnce) return;  // cwDateTime not initialised yet
    inNight = isNightTime();
  }

  if (inNight && !nightModeActive) {
    nightModeActive = true;
    if (p->nightAction == 0) {
      dma_display->setBrightness8(0);
    } else {
      dma_display->setBrightness8(p->nightMinBr);
    }
  } else if (!inNight && nightModeActive) {
    nightModeActive = false;
    dma_display->setBrightness8(p->displayBright);
  }
}

void autoChangeCheck()
{
  auto* p = ClockwiseParams::getInstance();
  if (p->autoChange == ClockwiseParams::AUTO_CHANGE_OFF) return;
  if (!wifi.connectionSucessfulOnce) return;

  int today = cwDateTime.getDay();
  if (lastAutoChangeDay == -1) { lastAutoChangeDay = today; return; }
  if (today != lastAutoChangeDay) {
    lastAutoChangeDay = today;
    uint8_t next;
    if (p->autoChange == ClockwiseParams::AUTO_CHANGE_SEQUENCE) {
      next = (p->clockFaceIndex + 1) % CWDriverRegistry::COUNT;
    } else {
      next = random(CWDriverRegistry::COUNT);
      if (next == p->clockFaceIndex) next = (next + 1) % CWDriverRegistry::COUNT;
    }
    if (widgetManager.activateClockWidget(next)) {
      p->clockFaceIndex = next;
      p->activeWidget = CWWidgetManager::WIDGET_CLOCK;
      p->save();
      ESP_LOGI("AUTO", "Day changed — switched to clockface %d", next);
    }
  }
}

void uptimeCheck()
{
  int today = cwDateTime.getDay();
  if (lastUptimeDay == -1) { lastUptimeDay = today; return; }
  if (today != lastUptimeDay) {
    lastUptimeDay = today;
    ClockwiseParams::getInstance()->totalDays++;
    ClockwiseParams::getInstance()->save();
  }
}

void webServerWatchdog()
{
  if (millis() - lastWebServerMillis > WEB_SERVER_WATCHDOG_MS) {
    lastWebServerMillis = millis();
    ClockwiseWebServer::getInstance()->stopWebServer();
    ClockwiseWebServer::getInstance()->startWebServer();
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(ESP32_LED_BUILTIN, OUTPUT);
  StatusController::getInstance()->blink_led(5, 100);

  ClockwiseParams::getInstance()->load();
  auto* p = ClockwiseParams::getInstance();

  pinMode(p->ldrPin, INPUT);

  // Connect WiFi BEFORE starting the HUB75 I2S/DMA display driver.
  // DMA generates I2S-band RF emissions that prevent new WiFi associations.
  // We establish the connection while the RF environment is clean, then hold
  // it through DMA start. WiFiController::begin() fast-paths on WL_CONNECTED,
  // skipping the disconnect+reconnect that would drop the early association.
  if (!p->wifiSsid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.begin(p->wifiSsid.c_str(), p->wifiPwd.c_str());
    // Wait for association (up to 8s) before starting DMA.
    // On a reachable AP this completes in ~1s — no noticeable delay.
    // On a missing/wrong-password AP we give up and fall through to the
    // AP-portal fallback path in wifi.begin() after displaySetup().
    for (uint8_t i = 0; i < 80 && WiFi.status() != WL_CONNECTED; i++) {
      delay(100);
    }
    ESP_LOGI("WiFi", "Pre-DMA connect status: %d (3=OK)", WiFi.status());
  }

  displaySetup(p->ledColorOrder, p->reversePhase, p->displayBright,
               p->displayRotation, p->driver, p->i2cSpeed, p->E_pin);

  // Register display with Locator so StatusController (and any other lib) can reach it.
  // Must happen before clockwiseLogo() — Locator::_display starts null; any call before
  // this line crashes with LoadProhibited (EXCVADDR=0x00000000).
  Locator::provide(dma_display);

  // v3: note target clockface — setup() called after cwDateTime is ready
  CWDriverRegistry::get(p->clockFaceIndex); // validate index early
  widgetManager.begin(dma_display, &cwDateTime, &currentFace);
  widgetManager.onWidgetChanged = [](const String& widgetName) {
    auto* prefs = ClockwiseParams::getInstance();
    String normalized = widgetName;
    normalized.toLowerCase();
    if (prefs->activeWidget != normalized) {
      prefs->activeWidget = normalized;
      prefs->saveActiveWidget();
    }
  };

  // Wire live-switch callback — instant, no reboot
  ClockwiseWebServer::getInstance()->onClockfaceSwitch = [](uint8_t idx) {
    if (widgetManager.activateClockWidget(idx)) {
      ClockwiseParams::getInstance()->clockFaceIndex = idx;
      ClockwiseParams::getInstance()->saveClockfaceIndex();
    }
  };
  ClockwiseWebServer::getInstance()->onWidgetSwitch = [](const String& widgetName) {
    auto* prefs = ClockwiseParams::getInstance();
    if (widgetManager.activateWidgetByName(widgetName, prefs->clockFaceIndex)) {
      return true;
    }
    return false;
  };
  ClockwiseWebServer::getInstance()->onWidgetStateJson = []() {
    auto* prefs = ClockwiseParams::getInstance();
    String json = "{";
    json += "\"activeWidget\":\"" + String(widgetManager.activeWidgetName()) + "\"";
    json += ",\"timerRemainingSec\":" + String(widgetManager.timerRemainingSeconds());
    json += ",\"clockfaceName\":\"" + String(CWDriverRegistry::name(prefs->clockFaceIndex)) + "\"";
    json += ",\"canReturnToClock\":" + String(widgetManager.canReturnToClock() ? "true" : "false");
    json += "}";
    return json;
  };

  // Live brightness apply (fixed mode only — auto modes manage their own brightness)
  ClockwiseWebServer::getInstance()->onBrightnessChange = [](uint8_t bright) {
    if (ClockwiseParams::getInstance()->brightMethod == 2)
      dma_display->setBrightness8(bright);
  };

  // Live 24h format toggle — update cwDateTime immediately so clockfaces see the change
  ClockwiseWebServer::getInstance()->on24hFormatChange = [](bool use24) {
    cwDateTime.set24hFormat(use24);
  };

  // Fixed brightness: apply immediately
  if (p->brightMethod == 2) dma_display->setBrightness8(p->displayBright);

  autoBrightEnabled = (p->autoBrightMax > 0);

  StatusController::getInstance()->clockwiseLogo();
  delay(1000);

    // OTA rollback guard — placed before wifi.begin() so that a restart triggered by
    // AP portal timeout does not leave the OTA partition in PENDING_VERIFY state and
    // cause the bootloader to roll back to the previous firmware. Display init and NVS
    // load passing is sufficient evidence of firmware health at this stage.
    // Safe to call unconditionally: no-op when partition is already VALID (e.g. non-OTA boots).
    esp_ota_mark_app_valid_cancel_rollback();
    ESP_LOGI("OTA", "Firmware marked valid — rollback window closed");

  StatusController::getInstance()->wifiConnecting();
  if (wifi.begin()) {
    StatusController::getInstance()->ntpConnecting();
    cwDateTime.begin(p->timeZone.c_str(), p->use24hFormat,
                     p->ntpServer.c_str(), p->manualPosix.c_str());
    // Now safe to setup the clockface — cwDateTime is ready
    if (!widgetManager.activateWidgetByName(p->activeWidget, p->clockFaceIndex)) {
      widgetManager.activateClockWidget(p->clockFaceIndex);
      p->activeWidget = CWWidgetManager::WIDGET_CLOCK;
      p->saveActiveWidget();
    }
    CWMqtt::getInstance()->begin();  // start MQTT after WiFi + time sync
    } else {
      // AP config portal timed out without WiFi credentials being configured.
      // Restart immediately so the portal becomes available again on next boot.
      // This avoids the dead zone where no AP and no web server are reachable.
      ESP_LOGW("WiFi", "AP config portal timed out without configuration — restarting");
      delay(3000);
      ESP.restart();
    }

  // Wire MQTT callbacks — same runtime behaviour as web UI
  CWMqtt::getInstance()->onClockfaceSwitch = [](uint8_t idx) {
    if (widgetManager.activateClockWidget(idx)) {
      ClockwiseParams::getInstance()->clockFaceIndex = idx;
      ClockwiseParams::getInstance()->saveClockfaceIndex();
    }
  };
  CWMqtt::getInstance()->onWidgetSwitch = [](const String& widgetName) {
    auto* prefs = ClockwiseParams::getInstance();
    if (widgetManager.activateWidgetByName(widgetName, prefs->clockFaceIndex)) {
      return true;
    }
    return false;
  };
  CWMqtt::getInstance()->onBrightnessChange = [](uint8_t bright) {
    if (ClockwiseParams::getInstance()->brightMethod == 2)
      dma_display->setBrightness8(bright);
  };

}

void loop()
{
  wifi.handleImprovWiFi();

  if (wifi.isConnected()) {
    ClockwiseWebServer::getInstance()->handleHttpRequest();
    ezt::events();
    webServerWatchdog();
    CWMqtt::getInstance()->loop();
  }

  if (wifi.connectionSucessfulOnce) {
    widgetManager.update();
    uptimeCheck();
    autoChangeCheck();
  }

  automaticBrightControl();
  nightModeCheck();

  // Yield to the FreeRTOS scheduler so IDLE1 can reset the task watchdog.
  // Without this, loopTask (Arduino loop on CPU 1) spins continuously and
  // starves the idle task, triggering "Task watchdog got triggered" panics
  // every ~5 s. A 1 ms delay is enough to keep IDLE1 alive without any
  // visible impact on display refresh rate (DMA handles the panel async).
  //
  // NOTE for other agents: do not remove this delay or replace with a
  // vTaskDelay(0) — zero ticks does NOT yield on ESP-IDF v4.4 FreeRTOS;
  // it must be at least 1. If you add a blocking operation inside loop()
  // that already yields (e.g. a FreeRTOS queue wait), audit whether this
  // delay is still needed to avoid double-sleeping.
  delay(1);
}
