#pragma once

#include <cctype>
#include <string>
#include <string_view>
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

inline bool parseSetEncodedAssignment(std::string_view encoded, std::string& key, std::string& value) {
  if (encoded.empty()) return false;

  size_t separator = encoded.find('=');
  if (separator == std::string_view::npos) {
    key.assign(encoded);
    value.clear();
    return !key.empty();
  }

  key.assign(encoded.substr(0, separator));
  value.assign(encoded.substr(separator + 1));
  return !key.empty();
}

inline void replaceAll(std::string& input, std::string_view from, std::string_view to) {
  if (from.empty()) return;

  size_t pos = 0;
  while ((pos = input.find(from.data(), pos, from.size())) != std::string::npos) {
    input.replace(pos, from.size(), to.data(), to.size());
    pos += to.size();
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

inline bool isSensitiveSetKey(std::string_view key) {
  return key == "wifiPwd" || key == "mqttPass";
}

inline bool looksLikeNamedSetFormBody(std::string_view body) {
  return body.find('&') != std::string_view::npos ||
         body.rfind("key=", 0) == 0 ||
         body.rfind("value=", 0) == 0 ||
         body == "key" ||
         body == "value";
}

inline bool getSetFormField(std::string_view body, std::string_view fieldName, std::string& value) {
  size_t start = 0;
  while (start <= body.size()) {
    size_t end = body.find('&', start);
    if (end == std::string_view::npos) end = body.size();
    std::string_view token = body.substr(start, end - start);

    if (token.find('=') == std::string_view::npos) {
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

inline std::string trimCopy(std::string_view value) {
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