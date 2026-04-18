#include "DisplayControl.h"

#include <NightModeLogic.h>

#include <CWPreferences.h>
#include <esp_log.h>

static constexpr uint8_t ONBOARD_LED_PIN = 2;

static bool isValidI2SSpeed(uint32_t speed) {
  return speed == 8000000 || speed == 16000000 || speed == 20000000;
}

static bool isValidDriver(uint32_t driver) {
  return driver <= 5;
}

static bool isNightTime(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();
  return cw::logic::isNightWindow(
      state.dateTime.getHour(),
      state.dateTime.getMinute(),
      prefs->nightStartH,
      prefs->nightStartM,
      prefs->nightEndH,
      prefs->nightEndM);
}

static cw::logic::BrightnessTarget resolveCurrentNormalBrightnessTarget(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();
  int currentLdrValue = 0;
  if (prefs->brightMethod == cw::logic::kBrightnessMethodLdr) {
    currentLdrValue = analogRead(prefs->ldrPin);
  }

  return cw::logic::resolveNormalBrightnessTarget(
      prefs->brightMethod,
      state.wifi.connectionSucessfulOnce,
      isNightTime(state),
      prefs->displayBright,
      prefs->nightBright,
      currentLdrValue,
      prefs->autoBrightMin,
      prefs->autoBrightMax);
}

static void applyBrightnessTarget(AppState& state,
                                  const cw::logic::BrightnessTarget& target,
                                  bool force = false) {
  if (!target.hasValue) {
    return;
  }

  if (!force && state.currentBrightness == target.brightness &&
      state.currentBrightSlot == target.slot) {
    return;
  }

  state.display->setBrightness8(target.brightness);
  state.currentBrightness = target.brightness;
  state.currentBrightSlot = target.slot;
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
  state.currentBrightness = displayBright;
  state.display->clearScreen();
  state.display->setRotation(displayRotation);
}

void automaticBrightControl(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();

  if (state.nightModeActive || prefs->brightMethod == cw::logic::kBrightnessMethodFixed) {
    return;
  }

  if (millis() - state.autoBrightMillis < 3000) {
    return;
  }
  state.autoBrightMillis = millis();

  const auto target = resolveCurrentNormalBrightnessTarget(state);
  if (cw::logic::shouldApplyAutomaticBrightness(
          prefs->brightMethod, state.currentBrightSlot, target)) {
    applyBrightnessTarget(state, target);
  }
}

void nightModeCheck(AppState& state) {
  auto* prefs = ClockwiseParams::getInstance();
  if (prefs->nightMode == 0) {
    if (state.nightModeActive) {
      state.nightModeActive = false;
      applyBrightnessTarget(state, resolveCurrentNormalBrightnessTarget(state), true);
    }
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

  const auto transition =
      cw::logic::resolveNightModeTransition(state.nightModeActive, inNight);
  const cw::logic::BrightnessTarget nightTarget = {
      true,
      cw::logic::resolveNightModeBrightness(prefs->nightAction, prefs->nightMinBr),
      -1};

  if (transition == cw::logic::NightModeTransition::kEnterNight) {
    state.nightModeActive = true;
    applyBrightnessTarget(state, nightTarget, true);
  } else if (transition == cw::logic::NightModeTransition::kStayNight) {
    applyBrightnessTarget(state, nightTarget);
  } else if (transition == cw::logic::NightModeTransition::kExitNight) {
    state.nightModeActive = false;
    applyBrightnessTarget(state, resolveCurrentNormalBrightnessTarget(state), true);
  }
}
