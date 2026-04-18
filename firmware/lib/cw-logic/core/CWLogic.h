#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <utility>

namespace cw {
namespace logic {

constexpr unsigned long kSetRequestBodyReceiveWindowMs = 75;

enum class SetRequestResolutionStatus {
  kUseQuery,
  kResolvedFromBody,
  kRejectIncompleteBody,
  kRejectInvalidBody,
};

enum class SetPersistenceKey {
  kUnknown,
  kDisplayBright,
  kUse24hFormat,
  kClockFaceIndex,
  kActiveWidget,
  kAutoBright,
  kSwapBlueGreen,
  kSwapBlueRed,
  kLedColorOrder,
  kAutoChange,
  kLdrPin,
  kDisplayRotation,
  kDriver,
  kEPin,
  kBrightMethod,
  kNightStartH,
  kNightStartM,
  kNightEndH,
  kNightEndM,
  kNightBright,
  kNightMode,
  kNightLevel,
  kNightTrig,
  kNightAction,
  kNightMinBr,
  kSuperColor,
  kMqttPort,
  kNightLdrThr,
  kI2cSpeed,
  kReversePhase,
  kMqttEnabled,
  kWifiSsid,
  kWifiPwd,
  kTimeZone,
  kNtpServer,
  kCanvasFile,
  kCanvasServer,
  kManualPosix,
  kMqttBroker,
  kMqttUser,
  kMqttPass,
  kMqttPrefix,
  kBigclockSrv,
  kBigclockFile,
};

enum class ClockfaceSetApplyDecision {
  kReject,
  kPersistMutation,
};

struct SetRequestResolution {
  std::string key;
  std::string value;
  SetRequestResolutionStatus status;
};

struct RequestBodyReadResult {
  std::string body;
  bool complete;
};

template <typename AvailableFn, typename ReadFn, typename WaitFn, typename MillisFn>
inline RequestBodyReadResult readRequestBodyWithinWindow(
  size_t contentLength,
  unsigned long receiveWindowMs,
  AvailableFn&& availableBytes,
  ReadFn&& readBytes,
  WaitFn&& waitForMoreData,
  MillisFn&& currentMillis
) {
  RequestBodyReadResult result = {"", contentLength == 0};
  if (contentLength == 0) return result;

  result.body.reserve(contentLength);
  const unsigned long startMs = currentMillis();

  while (result.body.size() < contentLength) {
    const size_t remaining = contentLength - result.body.size();
    const int available = availableBytes();

    if (available > 0) {
      const size_t appended = readBytes(result.body, remaining);
      if (appended > remaining) {
        result.complete = false;
        return result;
      }

      if (appended == 0) {
        if (currentMillis() - startMs >= receiveWindowMs) break;
        waitForMoreData();
        continue;
      }

      if (result.body.size() == contentLength) {
        result.complete = true;
        return result;
      }

      continue;
    }

    if (currentMillis() - startMs >= receiveWindowMs) break;
    waitForMoreData();
  }

  result.complete = result.body.size() == contentLength;
  return result;
}

inline bool parseSetEncodedAssignment(const std::string& encoded, std::string& key, std::string& value) {
  if (encoded.empty()) return false;

  size_t separator = encoded.find('=');
  if (separator == std::string::npos) {
    key.assign(encoded);
    value.clear();
    return !key.empty();
  }

  key.assign(encoded.substr(0, separator));
  value.assign(encoded.substr(separator + 1));
  return !key.empty();
}

inline void replaceAll(std::string& input, const char* from, const char* to) {
  if (from == NULL || to == NULL || *from == '\0') return;

  const size_t fromLen = std::strlen(from);
  const size_t toLen = std::strlen(to);

  size_t pos = 0;
  while ((pos = input.find(from, pos)) != std::string::npos) {
    input.replace(pos, fromLen, to);
    pos += toLen;
  }
}

inline std::string urlDecodeCopy(std::string value) {
  replaceAll(value, "%2F", "/");
  replaceAll(value, "%2f", "/");
  replaceAll(value, "%3A", ":");
  replaceAll(value, "%3a", ":");
  replaceAll(value, "%20", " ");
  replaceAll(value, "%40", "@");
  replaceAll(value, "%2B", "+");
  replaceAll(value, "%2b", "+");
  replaceAll(value, "%2C", ",");
  replaceAll(value, "%2c", ",");
  return value;
}

inline std::string formUrlDecodeCopy(std::string value) {
  replaceAll(value, "+", " ");
  return urlDecodeCopy(std::move(value));
}

inline bool isSensitiveSetKey(const std::string& key) {
  return key == "wifiPwd" || key == "mqttPass";
}

inline SetPersistenceKey resolveSetPersistenceKey(const std::string& key) {
  if (key == "displayBright") return SetPersistenceKey::kDisplayBright;
  if (key == "use24hFormat") return SetPersistenceKey::kUse24hFormat;
  if (key == "clockFaceIndex") return SetPersistenceKey::kClockFaceIndex;
  if (key == "activeWidget") return SetPersistenceKey::kActiveWidget;
  if (key == "autoBright") return SetPersistenceKey::kAutoBright;
  if (key == "swapBlueGreen") return SetPersistenceKey::kSwapBlueGreen;
  if (key == "swapBlueRed") return SetPersistenceKey::kSwapBlueRed;
  if (key == "ledColorOrder") return SetPersistenceKey::kLedColorOrder;
  if (key == "autoChange") return SetPersistenceKey::kAutoChange;
  if (key == "ldrPin") return SetPersistenceKey::kLdrPin;
  if (key == "displayRotation") return SetPersistenceKey::kDisplayRotation;
  if (key == "driver") return SetPersistenceKey::kDriver;
  if (key == "E_pin") return SetPersistenceKey::kEPin;
  if (key == "brightMethod") return SetPersistenceKey::kBrightMethod;
  if (key == "nightStartH") return SetPersistenceKey::kNightStartH;
  if (key == "nightStartM") return SetPersistenceKey::kNightStartM;
  if (key == "nightEndH") return SetPersistenceKey::kNightEndH;
  if (key == "nightEndM") return SetPersistenceKey::kNightEndM;
  if (key == "nightBright") return SetPersistenceKey::kNightBright;
  if (key == "nightMode") return SetPersistenceKey::kNightMode;
  if (key == "nightLevel") return SetPersistenceKey::kNightLevel;
  if (key == "nightTrig") return SetPersistenceKey::kNightTrig;
  if (key == "nightAction") return SetPersistenceKey::kNightAction;
  if (key == "nightMinBr") return SetPersistenceKey::kNightMinBr;
  if (key == "superColor") return SetPersistenceKey::kSuperColor;
  if (key == "mqttPort") return SetPersistenceKey::kMqttPort;
  if (key == "nightLdrThr") return SetPersistenceKey::kNightLdrThr;
  if (key == "i2cSpeed") return SetPersistenceKey::kI2cSpeed;
  if (key == "reversePhase") return SetPersistenceKey::kReversePhase;
  if (key == "mqttEnabled") return SetPersistenceKey::kMqttEnabled;
  if (key == "wifiSsid") return SetPersistenceKey::kWifiSsid;
  if (key == "wifiPwd") return SetPersistenceKey::kWifiPwd;
  if (key == "timeZone") return SetPersistenceKey::kTimeZone;
  if (key == "ntpServer") return SetPersistenceKey::kNtpServer;
  if (key == "canvasFile") return SetPersistenceKey::kCanvasFile;
  if (key == "canvasServer") return SetPersistenceKey::kCanvasServer;
  if (key == "manualPosix") return SetPersistenceKey::kManualPosix;
  if (key == "mqttBroker") return SetPersistenceKey::kMqttBroker;
  if (key == "mqttUser") return SetPersistenceKey::kMqttUser;
  if (key == "mqttPass") return SetPersistenceKey::kMqttPass;
  if (key == "mqttPrefix") return SetPersistenceKey::kMqttPrefix;
  if (key == "bigclockSrv") return SetPersistenceKey::kBigclockSrv;
  if (key == "bigclockFile") return SetPersistenceKey::kBigclockFile;
  return SetPersistenceKey::kUnknown;
}

inline ClockfaceSetApplyDecision resolveClockfaceSetApplyDecision(
  bool hasRuntimeSwitchCallback,
  bool runtimeSwitchSucceeded
) {
  if (hasRuntimeSwitchCallback && !runtimeSwitchSucceeded) {
    return ClockfaceSetApplyDecision::kReject;
  }

  return ClockfaceSetApplyDecision::kPersistMutation;
}

inline bool looksLikeNamedSetFormBody(const std::string& body) {
  return body.find('&') != std::string::npos ||
         body.find("key=") == 0 ||
         body.find("value=") == 0 ||
         body == "key" ||
         body == "value";
}

inline bool getSetFormField(const std::string& body, const char* fieldName, std::string& value) {
  size_t start = 0;
  while (start <= body.size()) {
    size_t end = body.find('&', start);
    if (end == std::string::npos) end = body.size();
    std::string token = body.substr(start, end - start);

    if (token.find('=') == std::string::npos) {
      if (end >= body.size()) break;
      start = end + 1;
      continue;
    }

    std::string fieldKey;
    std::string fieldValue;
    if (parseSetEncodedAssignment(token, fieldKey, fieldValue)) {
      fieldKey = formUrlDecodeCopy(std::move(fieldKey));
      fieldValue = formUrlDecodeCopy(std::move(fieldValue));
      if (fieldKey == fieldName) {
        value = std::move(fieldValue);
        return true;
      }
    }

    if (end >= body.size()) break;
    start = end + 1;
  }

  return false;
}

inline std::string trimCopy(const std::string& value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

inline SetRequestResolution resolveSetRequest(std::string key, std::string value, std::string body, bool bodyComplete) {
  if (!bodyComplete) {
    return {std::move(key), std::move(value), SetRequestResolutionStatus::kRejectIncompleteBody};
  }

  body = trimCopy(body);
  if (body.empty()) {
    return {std::move(key), std::move(value), SetRequestResolutionStatus::kUseQuery};
  }

  if (isSensitiveSetKey(key)) {
    std::string bodyValue;
    if (getSetFormField(body, "value", bodyValue)) {
      return {std::move(key), std::move(bodyValue), SetRequestResolutionStatus::kResolvedFromBody};
    }

    if (looksLikeNamedSetFormBody(body)) {
      return {std::move(key), std::move(value), SetRequestResolutionStatus::kRejectInvalidBody};
    }

    std::string encodedKey;
    std::string encodedValue;
    if (body.find('=') != std::string::npos && parseSetEncodedAssignment(body, encodedKey, encodedValue)) {
      encodedKey = formUrlDecodeCopy(std::move(encodedKey));
      encodedValue = formUrlDecodeCopy(std::move(encodedValue));
      if (encodedKey == key) {
        return {std::move(key), std::move(encodedValue), SetRequestResolutionStatus::kResolvedFromBody};
      }
      return {std::move(key), std::move(value), SetRequestResolutionStatus::kRejectInvalidBody};
    }

    return {std::move(key), formUrlDecodeCopy(std::move(body)), SetRequestResolutionStatus::kResolvedFromBody};
  }

  std::string bodyKey;
  std::string bodyValue;
  bool hasBodyKey = getSetFormField(body, "key", bodyKey);
  bool hasBodyValue = getSetFormField(body, "value", bodyValue);
  if (hasBodyKey || hasBodyValue || looksLikeNamedSetFormBody(body)) {
    if (hasBodyKey && hasBodyValue && isSensitiveSetKey(bodyKey)) {
      return {std::move(bodyKey), std::move(bodyValue), SetRequestResolutionStatus::kResolvedFromBody};
    }
    return {std::move(key), std::move(value), SetRequestResolutionStatus::kRejectInvalidBody};
  }

  if (body.find('=') != std::string::npos && parseSetEncodedAssignment(body, bodyKey, bodyValue)) {
    bodyKey = formUrlDecodeCopy(std::move(bodyKey));
    bodyValue = formUrlDecodeCopy(std::move(bodyValue));
    if (isSensitiveSetKey(bodyKey)) {
      return {std::move(bodyKey), std::move(bodyValue), SetRequestResolutionStatus::kResolvedFromBody};
    }
  }

  return {std::move(key), std::move(value), SetRequestResolutionStatus::kUseQuery};
}

}  // namespace logic
}  // namespace cw