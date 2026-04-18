#pragma once

#include <stdint.h>

namespace cw {
namespace logic {

constexpr uint8_t kMinBrightDisplayOn = 4;
constexpr uint8_t kMinBrightDisplayOff = 0;
constexpr uint8_t kBrightnessMethodLdr = 0;
constexpr uint8_t kBrightnessMethodTime = 1;
constexpr uint8_t kBrightnessMethodFixed = 2;

enum class NightModeTransition {
  kStayDay,
  kEnterNight,
  kStayNight,
  kExitNight,
};

struct BrightnessTarget {
  bool hasValue;
  uint8_t brightness;
  int slot;
};

inline bool isNightWindow(
  int nowHour,
  int nowMinute,
  int startHour,
  int startMinute,
  int endHour,
  int endMinute
) {
  const int now = nowHour * 60 + nowMinute;
  const int start = startHour * 60 + startMinute;
  const int end = endHour * 60 + endMinute;

  if (start < end) {
    return now >= start && now < end;
  }

  return now >= start || now < end;
}

inline NightModeTransition resolveNightModeTransition(bool wasNightModeActive, bool inNight) {
  if (inNight) {
    return wasNightModeActive ? NightModeTransition::kStayNight : NightModeTransition::kEnterNight;
  }

  return wasNightModeActive ? NightModeTransition::kExitNight : NightModeTransition::kStayDay;
}

inline uint8_t resolveNightModeBrightness(uint8_t nightAction, uint8_t nightMinBrightness) {
  return nightAction == 0 ? kMinBrightDisplayOff : nightMinBrightness;
}

inline int mapRangeClamped(int value, int inMin, int inMax, int outMin, int outMax) {
  if (inMax <= inMin) {
    return outMax;
  }

  if (value < inMin) value = inMin;
  if (value > inMax) value = inMax;

  const long scaled = static_cast<long>(value - inMin) * (outMax - outMin);
  return static_cast<int>(scaled / (inMax - inMin) + outMin);
}

inline BrightnessTarget resolveLdrBrightnessTarget(
  int currentLdrValue,
  uint16_t ldrMin,
  uint16_t ldrMax,
  uint8_t displayBrightness
) {
  const uint8_t minBrightness =
    currentLdrValue < static_cast<int>(ldrMin) ? kMinBrightDisplayOff : kMinBrightDisplayOn;
  const int mappedSlot = mapRangeClamped(currentLdrValue, ldrMin, ldrMax, 1, 10);
  const int mappedBrightness = mapRangeClamped(mappedSlot, 1, 10, minBrightness, displayBrightness);

  return {true, static_cast<uint8_t>(mappedBrightness), mappedSlot};
}

inline BrightnessTarget resolveNormalBrightnessTarget(
  uint8_t brightnessMethod,
  bool wifiConnectedOnce,
  bool scheduleNightActive,
  uint8_t displayBrightness,
  uint8_t scheduledNightBrightness,
  int currentLdrValue,
  uint16_t ldrMin,
  uint16_t ldrMax
) {
  if (brightnessMethod == kBrightnessMethodLdr) {
    if (ldrMax == 0) {
      return {false, 0, -1};
    }
    return resolveLdrBrightnessTarget(currentLdrValue, ldrMin, ldrMax, displayBrightness);
  }

  if (brightnessMethod == kBrightnessMethodTime) {
    if (!wifiConnectedOnce) {
      return {false, 0, -1};
    }

    const uint8_t brightness = scheduleNightActive ? scheduledNightBrightness : displayBrightness;
    return {true, brightness, brightness};
  }

  return {true, displayBrightness, displayBrightness};
}

inline bool shouldApplyAutomaticBrightness(
  uint8_t brightnessMethod,
  int currentBrightnessSlot,
  const BrightnessTarget& target
) {
  if (!target.hasValue) {
    return false;
  }

  if (brightnessMethod == kBrightnessMethodLdr) {
    int delta = currentBrightnessSlot - target.slot;
    if (delta < 0) delta = -delta;
    return delta >= 2 || target.brightness == 0;
  }

  return currentBrightnessSlot != target.slot;
}

}  // namespace logic
}  // namespace cw