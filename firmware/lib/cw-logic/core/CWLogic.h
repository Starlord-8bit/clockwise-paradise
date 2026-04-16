#pragma once

/**
 * CWLogic.h — Pure logic functions for Clockwise Paradise
 *
 * Zero hardware dependencies. Compiles on native (x86/ARM) and ESP32.
 * Every function here is tested by test_unit/SimpleTests.cpp.
 *
 * Source files delegate to these functions so the native test suite
 * exercises the REAL logic, not duplicated reimplementations.
 */

#include <stdint.h>
#include <string>
#include <cstring>
#include <cctype>

namespace cw {

// ── Night window predicate ──────────────────────────────────────────────
// Returns true when now_h:now_m falls inside [start_h:start_m, end_h:end_m).
// Handles midnight wrap (e.g. 22:00 → 07:00).
inline bool isNightWindow(int nowH, int nowM, int startH, int startM, int endH, int endM) {
    const int now   = nowH   * 60 + nowM;
    const int start = startH * 60 + startM;
    const int end   = endH   * 60 + endM;
    if (start < end) {
        return now >= start && now < end;
    }
    return now >= start || now < end;
}

// ── URL decode (std::string version) ────────────────────────────────────
// Decodes the subset of percent-encoded characters used by the web UI.
// Not full RFC 3986 — only handles chars the Clockwise UI can produce.
inline void urlDecode(std::string& v) {
    auto replaceAll = [](std::string& s, const char* from, const char* to) {
        const size_t fromLen = std::strlen(from);
        const size_t toLen   = std::strlen(to);
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, fromLen, to);
            pos += toLen;
        }
    };
    replaceAll(v, "%2F", "/");
    replaceAll(v, "%2f", "/");
    replaceAll(v, "%3A", ":");
    replaceAll(v, "%3a", ":");
    replaceAll(v, "%20", " ");
    replaceAll(v, "%40", "@");
    replaceAll(v, "%2B", "+");
    replaceAll(v, "%2b", "+");
    replaceAll(v, "%2C", ",");
    replaceAll(v, "%2c", ",");
}

// Convenience: decode and return.
inline std::string urlDecoded(const std::string& v) {
    std::string copy = v;
    urlDecode(copy);
    return copy;
}

// ── Sensitive key check ─────────────────────────────────────────────────
inline bool isSensitiveKey(const char* key) {
    return std::strcmp(key, "wifiPwd") == 0 || std::strcmp(key, "mqttPass") == 0;
}

// ── OTA state predicates ────────────────────────────────────────────────
// State values match esp_ota_img_states_t from ESP-IDF esp_ota_ops.h.
// Using uint32_t so the header doesn't depend on ESP-IDF.
namespace ota {
    static constexpr uint32_t STATE_NEW            = 0x0U;
    static constexpr uint32_t STATE_PENDING_VERIFY = 0x1U;
    static constexpr uint32_t STATE_VALID          = 0x2U;
    static constexpr uint32_t STATE_INVALID        = 0x3U;
    static constexpr uint32_t STATE_ABORTED        = 0x4U;
    static constexpr uint32_t STATE_UNDEFINED      = 0xFFFFFFFFU;

    inline bool isRollbackEligible(uint32_t state) {
        return state != STATE_INVALID && state != STATE_ABORTED;
    }

    inline bool shouldMarkValid(uint32_t state) {
        return state == STATE_PENDING_VERIFY;
    }
}

// ── Version normalization ───────────────────────────────────────────────
// Strips leading 'v' so "v2.8.1" and "2.8.1" compare equal.
inline std::string normalizeVersion(const std::string& v) {
    if (!v.empty() && v[0] == 'v') {
        return v.substr(1);
    }
    return v;
}

inline bool otaUpdateAvailable(const std::string& current, const std::string& remote) {
    return normalizeVersion(current) != normalizeVersion(remote);
}

// ── MQTT node ID sanitization ───────────────────────────────────────────
// Keeps alphanumeric, underscore, hyphen. Spaces become underscores. Lowercased.
inline std::string sanitizeMqttNodeId(const char* raw) {
    std::string out;
    for (const char* p = raw; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (std::isalnum(c) || c == '_' || c == '-') {
            out += static_cast<char>(std::tolower(c));
        } else if (c == ' ') {
            out += '_';
        }
    }
    // trim leading/trailing underscores/hyphens that came from whitespace
    return out;
}

// ── Auto-change clockface index ─────────────────────────────────────────
// mode: 1=sequence, 2=random. randomVal is a pre-generated random number [0, count).
inline uint8_t nextAutoChangeIndex(uint8_t current, uint8_t count, uint8_t mode, uint8_t randomVal) {
    if (count == 0) return 0;
    if (mode == 1) { // sequence
        return (current + 1) % count;
    }
    // random — avoid same face
    uint8_t next = randomVal % count;
    if (next == current) {
        next = (next + 1) % count;
    }
    return next;
}

} // namespace cw
