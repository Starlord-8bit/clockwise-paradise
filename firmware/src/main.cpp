#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// v3 clockface driver registry (replaces v2 dispatcher + class hierarchy)
#include <CWClockfaceDriver.h>
static const CWClockfaceDriver* currentFace = nullptr;

// Commons
#include <WiFiController.h>
#include <CWDateTime.h>
#include <CWPreferences.h>
#include <CWWebServer.h>
#include <StatusController.h>
#include <CWMqtt.h>

#define MIN_BRIGHT_DISPLAY_ON 4
#define MIN_BRIGHT_DISPLAY_OFF 0

#define ESP32_LED_BUILTIN 2

MatrixPanel_I2S_DMA *dma_display = nullptr;
// clockface pointer declared above
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
  dma_display->begin();
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
  } else if (method == 1) {
    // Time-based
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

  bool inNight = isNightTime();

  if (inNight && !nightModeActive) {
    nightModeActive = true;
    if (p->nightMode == 1) {
      dma_display->setBrightness8(0);
    } else if (p->nightMode == 2) {
      uint8_t bright = map(p->nightLevel, 1, 5, 8, 64);
      dma_display->setBrightness8(bright);
      p->canvasServer = p->bigclockServer;
      p->canvasFile   = p->bigclockFile;
      CWDriverRegistry::switchTo(&currentFace, p->clockFaceIndex, dma_display, &cwDateTime);
    }
  } else if (!inNight && nightModeActive) {
    nightModeActive = false;
    p->load();  // restore original canvas settings
    dma_display->setBrightness8(p->displayBright);
    CWDriverRegistry::switchTo(&currentFace, p->clockFaceIndex, dma_display, &cwDateTime);
  }
}

void autoChangeCheck()
{
  auto* p = ClockwiseParams::getInstance();
  if (p->autoChange == ClockwiseParams::AUTO_CHANGE_OFF) return;
  // NOTE: full auto-change requires the multi-clockface dispatcher (feature/clockface-dispatcher).
  // This stub saves the day counter for when dispatcher is merged.
  int today = cwDateTime.getDay();
  if (lastAutoChangeDay == -1) { lastAutoChangeDay = today; return; }
  if (today != lastAutoChangeDay) lastAutoChangeDay = today;
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

  displaySetup(p->ledColorOrder, p->reversePhase, p->displayBright,
               p->displayRotation, p->driver, p->i2cSpeed, p->E_pin);

  // v3: initialise from saved clockface index
  CWDriverRegistry::switchTo(&currentFace, p->clockFaceIndex, dma_display, &cwDateTime);

  // Wire live-switch callback — instant, no reboot
  ClockwiseWebServer::getInstance()->onClockfaceSwitch = [](uint8_t idx) {
    if (CWDriverRegistry::switchTo(&currentFace, idx, dma_display, &cwDateTime)) {
      ClockwiseParams::getInstance()->clockFaceIndex = idx;
      ClockwiseParams::getInstance()->save();
    }
  };

  // Fixed brightness: apply immediately
  if (p->brightMethod == 2) dma_display->setBrightness8(p->displayBright);

  autoBrightEnabled = (p->autoBrightMax > 0);

  StatusController::getInstance()->clockwiseLogo();
  delay(1000);

  StatusController::getInstance()->wifiConnecting();
  if (wifi.begin()) {
    StatusController::getInstance()->ntpConnecting();
    cwDateTime.begin(p->timeZone.c_str(), p->use24hFormat,
                     p->ntpServer.c_str(), p->manualPosix.c_str());
    // Re-run setup now that time is available
    if (currentFace) currentFace->setup(dma_display, &cwDateTime);
    CWMqtt::getInstance()->begin();  // start MQTT after WiFi + time sync
  }
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
    if (currentFace) currentFace->update();
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
