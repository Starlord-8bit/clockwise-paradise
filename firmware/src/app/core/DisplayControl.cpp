#include "DisplayControl.h"

#include <core/CWPreferences.h>
#include <esp_log.h>

static constexpr uint8_t MIN_BRIGHT_DISPLAY_ON = 4;
static constexpr uint8_t MIN_BRIGHT_DISPLAY_OFF = 0;
static constexpr uint8_t ONBOARD_LED_PIN = 2;

static bool isValidI2SSpeed(uint32_t speed) {
  return speed == 8000000 || speed == 16000000 || speed == 20000000;
}

static bool isValidDriver(uint32_t driver) {
  return driver <= 5;
}

static bool isNightTime(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();
  int nowMins = state.dateTime.getHour() * 60 + state.dateTime.getMinute();
  int startMins = prefs->nightStartH * 60 + prefs->nightStartM;
  int endMins = prefs->nightEndH * 60 + prefs->nightEndM;

  if (startMins < endMins) {
    return nowMins >= startMins && nowMins < endMins;
  }
  return nowMins >= startMins || nowMins < endMins;
}

void displaySetup(AppState& state, uint8_t ledColorOrder, bool reversePhase,
                  uint8_t displayBright, uint8_t displayRotation,
                  uint8_t driver, uint32_t i2cSpeed, uint8_t ePin) {
  HUB75_I2S_CFG mxconfig(64, 64, 1);

  if (ledColorOrder == ClockwiseParams::LED_ORDER_RBG) {
    mxconfig.gpio.b1 = 26;
    mxconfig.gpio.b2 = 12;
    mxconfig.gpio.g1 = 27;
    mxconfig.gpio.g2 = 13;
  } else if (ledColorOrder == ClockwiseParams::LED_ORDER_GBR) {
    mxconfig.gpio.g1 = 25;
    mxconfig.gpio.g2 = 14;
    mxconfig.gpio.r1 = 26;
    mxconfig.gpio.r2 = 12;
  }

  mxconfig.gpio.e = ePin;
  mxconfig.clkphase = reversePhase;

  if (isValidDriver(driver)) {
    mxconfig.driver = static_cast<HUB75_I2S_CFG::shift_driver>(driver);
  }
  if (isValidI2SSpeed(i2cSpeed)) {
    mxconfig.i2sspeed = static_cast<HUB75_I2S_CFG::clk_speed>(i2cSpeed);
  }

  state.display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!state.display->begin()) {
    while (true) {
      digitalWrite(ONBOARD_LED_PIN, HIGH);
      delay(100);
      digitalWrite(ONBOARD_LED_PIN, LOW);
      delay(100);
    }
  }

  state.display->setBrightness8(displayBright);
  state.display->clearScreen();
  state.display->setRotation(displayRotation);
}

void automaticBrightControl(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();

  if (prefs->brightMethod == 2) {
    return;
  }

  if (millis() - state.autoBrightMillis < 3000) {
    return;
  }
  state.autoBrightMillis = millis();

  if (prefs->brightMethod == 0 && prefs->autoBrightMax > 0) {
    int16_t currentValue = analogRead(prefs->ldrPin);
    uint16_t ldrMin = prefs->autoBrightMin;
    uint16_t ldrMax = prefs->autoBrightMax;

    const uint8_t minBright =
        (currentValue < ldrMin ? MIN_BRIGHT_DISPLAY_OFF : MIN_BRIGHT_DISPLAY_ON);
    uint8_t maxBright = prefs->displayBright;
    uint8_t slots = 10;
    uint8_t mappedLdr =
        map(currentValue > ldrMax ? ldrMax : currentValue, ldrMin, ldrMax, 1, slots);
    uint8_t mappedBright = map(mappedLdr, 1, slots, minBright, maxBright);

    if (abs(state.currentBrightSlot - mappedLdr) >= 2 || mappedBright == 0) {
      state.display->setBrightness8(mappedBright);
      state.currentBrightSlot = mappedLdr;
    }
  } else if (prefs->brightMethod == 1 && state.wifi.connectionSucessfulOnce) {
    uint8_t targetBright = isNightTime(state) ? prefs->nightBright : prefs->displayBright;
    if (state.currentBrightSlot != targetBright) {
      state.display->setBrightness8(targetBright);
      state.currentBrightSlot = targetBright;
    }
  }
}

void nightModeCheck(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();
  if (prefs->nightMode == 0) {
    return;
  }

  bool inNight = false;
  if (prefs->nightTrigger == 1) {
    inNight = analogRead(prefs->ldrPin) <= prefs->nightLdrThres;
  } else {
    if (!state.wifi.connectionSucessfulOnce) {
      return;
    }
    inNight = isNightTime(state);
  }

  if (inNight && !state.nightModeActive) {
    state.nightModeActive = true;
    if (prefs->nightAction == 0) {
      state.display->setBrightness8(0);
    } else {
      state.display->setBrightness8(prefs->nightMinBr);
    }
  } else if (!inNight && state.nightModeActive) {
    state.nightModeActive = false;
    state.display->setBrightness8(prefs->displayBright);
  }
}
