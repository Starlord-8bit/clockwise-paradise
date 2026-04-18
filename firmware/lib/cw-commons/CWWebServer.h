#pragma once

#include <WiFi.h>
#include <functional>
#include <CWPreferences.h>
#include <CWOTA.h>
#include <CWWidgetManager.h>
#include "../cw-logic/core/CWLogic.h"
#include "StatusController.h"
#include "esp_log.h"

// New multi-page UI
#include "CWWebUI.h"
#include "pages/HomePage.h"
#include "pages/ClockPage.h"
#include "pages/WidgetsPage.h"
#include "pages/SyncPage.h"
#include "pages/HardwarePage.h"
#include "pages/UpdatePage.h"

// Legacy single-page UI (kept temporarily)
#include "SettingsWebPage.h"

#include "CWClockfaceDriver.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"

#ifndef CLOCKFACE_NAME
  #define CLOCKFACE_NAME "UNKNOWN"
#endif

extern WiFiServer server;

struct ClockwiseWebServer
{
  static constexpr unsigned long SET_REQUEST_BODY_RECEIVE_WINDOW_MS = cw::logic::kSetRequestBodyReceiveWindowMs;

  String httpBuffer;
  bool force_restart;
  uint8_t pending_clockface_index = 255; // 255 = no pending switch

  // Callback set by main.cpp for live clockface switching without reboot.
  // Signature: void switchClockface(uint8_t index)
  // If null, falls back to save+reboot.
  std::function<void(uint8_t)> onClockfaceSwitch = nullptr;
  // Generic widget switch callback. Returns true if the widget was activated.
  std::function<bool(const String&)> onWidgetSwitch = nullptr;
  // Returns widget runtime state JSON.
  std::function<String()> onWidgetStateJson = nullptr;
  // Callback for live brightness apply (fixed-mode only).
  std::function<void(uint8_t)> onBrightnessChange = nullptr;
  // Callback for live 24h format toggle.
  std::function<void(bool)> on24hFormatChange = nullptr;
  const char* HEADER_TEMPLATE_D = "X-%s: %d\r\n";
  const char* HEADER_TEMPLATE_S = "X-%s: %s\r\n";

  static ClockwiseWebServer *getInstance()
  {
    static ClockwiseWebServer base;
    return &base;
  }

  void startWebServer()
  {
    server.begin();
    StatusController::getInstance()->blink_led(100, 3);
  }

  void stopWebServer()
  {
    server.stop();
  }

  void handleHttpRequest()
  {
    if (force_restart)
      StatusController::getInstance()->forceRestart();

    WiFiClient client = server.available();
    if (!client) return;

    StatusController::getInstance()->blink_led(100, 1);

    // --- Read first line only (method + path) ---
    // Original fast char-by-char approach: minimal blocking.
    String firstLine = "";
    unsigned long deadline = millis() + 500;
    while (client.connected() && millis() < deadline) {
      if (!client.available()) { delay(1); continue; }
      char c = client.read();
      if (c == '\n') break;
      firstLine += c;
    }
    firstLine.trim();
    if (firstLine.isEmpty()) { client.stop(); return; }

    // Parse method + path from first line
    int s1 = firstLine.indexOf(' ');
    int s2 = firstLine.indexOf(' ', s1 + 1);
    String method = firstLine.substring(0, s1);
    String path   = firstLine.substring(s1 + 1, s2);
    String key = "", value = "";
    if (path.indexOf('?') > 0) {
      String qs = path.substring(path.indexOf('?') + 1);
      parseEncodedAssignment(qs, key, value);
      path  = path.substring(0, path.indexOf('?'));
    }

    // --- Binary upload: read remaining headers for Content-Length ---
    // Only done for /ota/upload — all other routes skip the body.
    if (method == "POST" && path == "/ota/upload") {
      int contentLength = 0;
      bool expectContinue = false;

      // Drain remaining headers, extract Content-Length and handle Expect: 100-continue
      client.setTimeout(200);
      unsigned long hdr_deadline = millis() + 2000;
      while (client.connected() && millis() < hdr_deadline) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) break;  // blank line = end of headers
        String lower = line; lower.toLowerCase();
        if (lower.startsWith("content-length:")) {
          contentLength = line.substring(15).toInt();
        } else if (lower.startsWith("expect:")) {
          // curl sends this automatically for large uploads; reply 100 Continue so it actually sends the body.
          if (lower.indexOf("100-continue") >= 0) expectContinue = true;
        }
      }

      if (expectContinue) {
        client.print("HTTP/1.1 100 Continue\r\n\r\n");
        client.flush();
      }

      handleOtaUpload(client, contentLength);
      return;
    }

    // --- All other routes: drain remaining headers quickly then process ---
    {
      int contentLength = 0;
      client.setTimeout(100);
      unsigned long hdr_deadline = millis() + 500;
      while (client.connected() && millis() < hdr_deadline) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) break;
        String lower = line; lower.toLowerCase();
        if (lower.startsWith("content-length:")) {
          contentLength = line.substring(15).toInt();
        }
      }

      if (method == "POST" && path == "/set" && contentLength > 0) {
        cw::logic::RequestBodyReadResult body = readSetRequestBody(client, contentLength);
        cw::logic::SetRequestResolution resolved = cw::logic::resolveSetRequest(
          std::string(key.c_str()),
          std::string(value.c_str()),
          std::string(body.body.c_str()),
          body.complete
        );

        if (resolved.status == cw::logic::SetRequestResolutionStatus::kRejectIncompleteBody) {
          client.println("HTTP/1.0 400 Bad Request");
          client.println("Content-Type: application/json");
          client.println();
          client.print("{\"status\":\"error\",\"message\":\"Incomplete request body\"}");
          client.stop();
          return;
        }

        if (resolved.status == cw::logic::SetRequestResolutionStatus::kRejectInvalidBody) {
          client.println("HTTP/1.0 400 Bad Request");
          client.println("Content-Type: application/json");
          client.println();
          client.print("{\"status\":\"error\",\"message\":\"Invalid request body\"}");
          client.stop();
          return;
        }

        key = resolved.key.c_str();
        value = resolved.value.c_str();
      }
    }

    processRequest(client, method, path, key, value);
    delay(1);
    client.stop();
  }

  /**
   * POST /ota/upload — stream a raw .bin directly into the OTA partition.
   *
   * Accepts a raw binary body (Content-Type: application/octet-stream).
   * Streams bytes directly to the OTA write API — the full binary is never
   * held in RAM. Compatible with both merged (0x0000) and app-only binaries.
   *
   * From the web UI: <input type="file"> + fetch() with body=file.
   * From curl / AI automation:
   *   curl -X POST http://<ip>/ota/upload \
   *        -H 'Content-Type: application/octet-stream' \
   *        --data-binary @clockwise-paradise.bin
   *
   * Returns JSON: {"status":"ok"} on success (then reboots),
   *               {"status":"error","message":"..."}  on failure.
   */
  void handleOtaUpload(WiFiClient& client, int contentLength) {
    if (contentLength <= 0) {
      client.println("HTTP/1.0 400 Bad Request");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Missing Content-Length\"}");
      client.flush(); client.stop();
      return;
    }

    ESP_LOGI("OTA", "Upload starting, expecting %d bytes", contentLength);
    StatusController::getInstance()->printCenter("Uploading...", 32);

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition) {
      client.println("HTTP/1.0 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"No OTA partition found\"}");
      client.flush(); client.stop();
      return;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, contentLength, &ota_handle);
    if (err != ESP_OK) {
      client.println("HTTP/1.0 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println();
      client.printf("{\"status\":\"error\",\"message\":\"esp_ota_begin: %s\"}", esp_err_to_name(err));
      client.flush(); client.stop();
      return;
    }

    // Stream body into OTA write — 4KB chunks
    static uint8_t buf[4096];
    int remaining = contentLength;
    bool write_error = false;

    while (remaining > 0 && client.connected()) {
      int to_read = min(remaining, (int)sizeof(buf));
      int got = 0;
      unsigned long chunk_deadline = millis() + 10000;
      while (got < to_read && millis() < chunk_deadline) {
        int avail = client.available();
        if (avail > 0) {
          int r = client.read(buf + got, min(avail, to_read - got));
          if (r > 0) { got += r; }
        } else {
          delay(1);
        }
      }
      if (got == 0) { write_error = true; break; }  // timeout

      err = esp_ota_write(ota_handle, buf, got);
      if (err != ESP_OK) { write_error = true; break; }
      remaining -= got;
      ESP_LOGD("OTA", "Upload progress: %d/%d bytes", contentLength - remaining, contentLength);
    }

    if (write_error || remaining != 0) {
      esp_ota_abort(ota_handle);
      client.println("HTTP/1.0 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println();
      client.printf("{\"status\":\"error\",\"message\":\"%s\"}",
                    write_error ? esp_err_to_name(err) : "Upload incomplete");
      client.flush(); client.stop();
      return;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
      client.println("HTTP/1.0 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println();
      client.printf("{\"status\":\"error\",\"message\":\"esp_ota_end: %s\"}", esp_err_to_name(err));
      client.flush(); client.stop();
      return;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
      client.println("HTTP/1.0 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println();
      client.printf("{\"status\":\"error\",\"message\":\"esp_ota_set_boot: %s\"}", esp_err_to_name(err));
      client.flush(); client.stop();
      return;
    }

    ESP_LOGI("OTA", "Upload complete — rebooting");
    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: application/json");
    client.println();
    client.print("{\"status\":\"ok\",\"message\":\"Upload complete, rebooting\"}");
    client.flush();
    client.stop();
    delay(500);
    esp_restart();
  }

  /**
   * GET /ota/status — returns JSON describing the active OTA partition and its state.
   *
   * Fields:
   *   running_partition  — "app0" or "app1"
   *   running_state      — "valid" | "pending" | "new" | "invalid" | "aborted" | "undefined"
   *   running_version    — version string embedded in the running binary
   *   other_partition    — the inactive OTA slot label
   *   other_valid        — true if the other slot contains a flashable image
   *   other_version      — version string in the other slot (only when other_valid is true)
   *
   * The "pending" state means this firmware was OTA'd and has not yet called
   * esp_ota_mark_app_valid_cancel_rollback() — it will be rolled back on the next
   * unclean reset. Under normal operation this state clears at end of setup().
   */
  String getOtaStatus() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* other   = esp_ota_get_next_update_partition(nullptr);

    esp_app_desc_t running_desc = {};
    esp_ota_get_partition_description(running, &running_desc);

    esp_ota_img_states_t running_state = ESP_OTA_IMG_UNDEFINED;
    esp_ota_get_state_partition(running, &running_state);

    const char* state_str = "undefined";
    switch (running_state) {
      case ESP_OTA_IMG_NEW:            state_str = "new";     break;
      case ESP_OTA_IMG_PENDING_VERIFY: state_str = "pending"; break;
      case ESP_OTA_IMG_VALID:          state_str = "valid";   break;
      case ESP_OTA_IMG_INVALID:        state_str = "invalid"; break;
      case ESP_OTA_IMG_ABORTED:        state_str = "aborted"; break;
      default:                         state_str = "undefined"; break;
    }

    esp_app_desc_t other_desc = {};
    bool other_valid = false;
    if (other && esp_ota_get_partition_description(other, &other_desc) == ESP_OK) {
      esp_ota_img_states_t other_state = ESP_OTA_IMG_UNDEFINED;
      esp_ota_get_state_partition(other, &other_state);
      // INVALID and ABORTED mean the bootloader has already rejected this slot — don't offer it.
      // UNDEFINED is fine (factory/never-OTA'd slot with a valid image header).
      other_valid = (other_state != ESP_OTA_IMG_INVALID && other_state != ESP_OTA_IMG_ABORTED);
    }

    String json = "{";
    json += "\"running_partition\":\"" + String(running ? running->label : "unknown") + "\"";
    json += ",\"running_state\":\""    + String(state_str)                            + "\"";
    json += ",\"running_version\":\""  + String(running_desc.version)                 + "\"";
    json += ",\"other_partition\":\""  + String(other ? other->label : "none")        + "\"";
    json += ",\"other_valid\":"        + String(other_valid ? "true" : "false");
    if (other_valid) {
      json += ",\"other_version\":\"" + String(other_desc.version) + "\"";
    }
    json += "}";
    return json;
  }

  /**
   * POST /ota/rollback — boot the other OTA partition on next restart.
   *
   * Checks that the other partition contains a valid image before committing.
   * Responds with JSON, then reboots. Returns 409 if no valid other image exists.
   *
   * Use this when a new OTA firmware is functionally broken but hasn't crashed
   * (so the automatic rollback never triggered). The user can force-rollback
   * via the web UI without needing physical flash access.
   */
  void handleOtaRollback(WiFiClient& client) {
    const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);

    esp_app_desc_t other_desc = {};
    bool other_valid = false;
    if (other && esp_ota_get_partition_description(other, &other_desc) == ESP_OK) {
      esp_ota_img_states_t other_state = ESP_OTA_IMG_UNDEFINED;
      esp_ota_get_state_partition(other, &other_state);
      other_valid = (other_state != ESP_OTA_IMG_INVALID && other_state != ESP_OTA_IMG_ABORTED);
    }

    if (!other_valid) {
      client.println("HTTP/1.0 409 Conflict");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"No valid firmware in other partition — rollback unavailable\"}");
      client.flush(); client.stop();
      return;
    }

    esp_err_t err = esp_ota_set_boot_partition(other);
    if (err != ESP_OK) {
      client.println("HTTP/1.0 500 Internal Server Error");
      client.println("Content-Type: application/json");
      client.println();
      client.printf("{\"status\":\"error\",\"message\":\"%s\"}", esp_err_to_name(err));
      client.flush(); client.stop();
      return;
    }

    ESP_LOGI("OTA", "Manual rollback to %s (%s) — rebooting", other->label, other_desc.version);

    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: application/json");
    client.println();
    client.printf("{\"status\":\"ok\",\"message\":\"Rebooting to %s\",\"version\":\"%s\"}",
                  other->label, other_desc.version);
    client.flush(); client.stop();
    delay(500);
    esp_restart();
  }

  // ── Setting descriptors (pointer-to-member, type-safe) ──────────────────
  struct SettU8  { const char* k; uint8_t  ClockwiseParams::*f; };
  struct SettU16 { const char* k; uint16_t ClockwiseParams::*f; };
  struct SettU32 { const char* k; uint32_t ClockwiseParams::*f; };
  struct SettB   { const char* k; bool     ClockwiseParams::*f; };
  struct SettS   { const char* k; String   ClockwiseParams::*f; };

  static bool parseEncodedAssignment(const String& encoded, String& key, String& value) {
    std::string decodedKey;
    std::string decodedValue;
    bool parsed = cw::logic::parseSetEncodedAssignment(encoded.c_str(), decodedKey, decodedValue);
    key = decodedKey.c_str();
    value = decodedValue.c_str();
    return parsed;
  }

  static void urlDecode(String& v) {
    v = cw::logic::urlDecodeCopy(v.c_str()).c_str();
  }

  static cw::logic::RequestBodyReadResult readSetRequestBody(WiFiClient& client, int contentLength) {
    if (contentLength <= 0) {
      return {"", true};
    }

    return cw::logic::readRequestBodyWithinWindow(
      static_cast<size_t>(contentLength),
      SET_REQUEST_BODY_RECEIVE_WINDOW_MS,
      [&client]() {
        return client.available();
      },
      [&client](std::string& body, size_t remaining) {
        size_t appended = 0;
        while (appended < remaining && client.available() > 0) {
          const int next = client.read();
          if (next < 0) break;
          body.push_back(static_cast<char>(next));
          ++appended;
        }
        return appended;
      },
      []() {
        delay(1);
      },
      []() {
        return millis();
      }
    );
  }

  /**
   * Apply a single URL-decoded key=value pair to ClockwiseParams.
   * Special keys (with runtime callbacks) are handled explicitly;
   * everything else falls through to compact lookup tables.
   */
  bool handleSet(const String& key, const String& value) {
    auto* p = ClockwiseParams::getInstance();

    // ── Special keys with side-effects ──
    if (key == "displayBright") {
      p->displayBright = (uint8_t)value.toInt();
      if (onBrightnessChange) onBrightnessChange(p->displayBright);
      return true;
    }
    if (key == "use24hFormat") {
      p->use24hFormat = (value == "1");
      if (on24hFormatChange) on24hFormatChange(p->use24hFormat);
      return true;
    }
    if (key == "clockFaceIndex") {
      uint8_t idx = (uint8_t)value.toInt();
      p->clockFaceIndex = idx;
      p->activeWidget = "clock";
      if (onClockfaceSwitch) onClockfaceSwitch(idx);
      else force_restart = true;
      return true;
    }
    if (key == "activeWidget") {
      String normalized = value;
      normalized.toLowerCase();
      if (onWidgetSwitch) {
        if (!onWidgetSwitch(normalized)) {
          ESP_LOGW("Web", "Widget '%s' not activated", normalized.c_str());
          return false;
        }
      } else {
        p->activeWidget = normalized;
        force_restart = true;
      }
      return true;
    }
    if (key == "autoBright") {
      p->autoBrightMin = value.substring(0, 4).toInt();
      p->autoBrightMax = value.substring(5, 9).toInt();
      return true;
    }
    // Legacy colour swap — also updates ledColorOrder
    if (key == "swapBlueGreen") {
      p->swapBlueGreen = (value == "1");
      p->ledColorOrder = (value == "1") ? ClockwiseParams::LED_ORDER_RBG
                       : (p->swapBlueRed ? ClockwiseParams::LED_ORDER_GBR
                                         : ClockwiseParams::LED_ORDER_RGB);
      return true;
    }
    if (key == "swapBlueRed") {
      p->swapBlueRed = (value == "1");
      p->ledColorOrder = (value == "1") ? ClockwiseParams::LED_ORDER_GBR
                       : (p->swapBlueGreen ? ClockwiseParams::LED_ORDER_RBG
                                           : ClockwiseParams::LED_ORDER_RGB);
      return true;
    }

    // ── Table-driven simple settings ──
    static const SettU8 U8S[] = {
      { "ledColorOrder",   &ClockwiseParams::ledColorOrder },
      { "autoChange",      &ClockwiseParams::autoChange },
      { "ldrPin",          &ClockwiseParams::ldrPin },
      { "displayRotation", &ClockwiseParams::displayRotation },
      { "driver",          &ClockwiseParams::driver },
      { "E_pin",           &ClockwiseParams::E_pin },
      { "brightMethod",    &ClockwiseParams::brightMethod },
      { "nightStartH",     &ClockwiseParams::nightStartH },
      { "nightStartM",     &ClockwiseParams::nightStartM },
      { "nightEndH",       &ClockwiseParams::nightEndH },
      { "nightEndM",       &ClockwiseParams::nightEndM },
      { "nightBright",     &ClockwiseParams::nightBright },
      { "nightMode",       &ClockwiseParams::nightMode },
      { "nightLevel",      &ClockwiseParams::nightLevel },
      { "nightTrig",       &ClockwiseParams::nightTrigger },
      { "nightAction",     &ClockwiseParams::nightAction },
      { "nightMinBr",      &ClockwiseParams::nightMinBr },
    };
    for (const auto& s : U8S) if (key == s.k) { p->*(s.f) = (uint8_t)value.toInt(); return true; }

    static const SettU16 U16S[] = {
      { "superColor", &ClockwiseParams::superColor },
      { "mqttPort",   &ClockwiseParams::mqttPort },
      { "nightLdrThr", &ClockwiseParams::nightLdrThres },
    };
    for (const auto& s : U16S) if (key == s.k) { p->*(s.f) = (uint16_t)value.toInt(); return true; }

    static const SettU32 U32S[] = {
      { "i2cSpeed", &ClockwiseParams::i2cSpeed },
    };
    for (const auto& s : U32S) if (key == s.k) { p->*(s.f) = (uint32_t)value.toInt(); return true; }

    static const SettB BS[] = {
      { "reversePhase", &ClockwiseParams::reversePhase },
      { "mqttEnabled",  &ClockwiseParams::mqttEnabled },
    };
    for (const auto& s : BS) if (key == s.k) { p->*(s.f) = (value == "1"); return true; }

    static const SettS SS[] = {
      { "wifiSsid",     &ClockwiseParams::wifiSsid },
      { "wifiPwd",      &ClockwiseParams::wifiPwd },
      { "timeZone",     &ClockwiseParams::timeZone },
      { "ntpServer",    &ClockwiseParams::ntpServer },
      { "canvasFile",   &ClockwiseParams::canvasFile },
      { "canvasServer", &ClockwiseParams::canvasServer },
      { "manualPosix",  &ClockwiseParams::manualPosix },
      { "mqttBroker",   &ClockwiseParams::mqttBroker },
      { "mqttUser",     &ClockwiseParams::mqttUser },
      { "mqttPass",     &ClockwiseParams::mqttPass },
      { "mqttPrefix",   &ClockwiseParams::mqttPrefix },
      { "activeWidget", &ClockwiseParams::activeWidget },
      { "bigclockSrv",  &ClockwiseParams::bigclockServer },
      { "bigclockFile", &ClockwiseParams::bigclockFile },
    };
    for (const auto& s : SS) if (key == s.k) { p->*(s.f) = value; return true; }

    return false;
  }

  void processRequest(WiFiClient client, String method, String path, String key, String value)
  {
    if (method == "GET" && path == "/") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendHomePage(client);
    } else if (method == "GET" && path == "/clock") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendClockPage(client);
    } else if (method == "GET" && path == "/widgets") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendWidgetsPage(client);
    } else if (method == "GET" && path == "/sync") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendSyncPage(client);
    } else if (method == "GET" && path == "/hardware") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendHardwarePage(client);
    } else if (method == "GET" && path == "/update") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      cw_sendUpdatePage(client);
    } else if (method == "GET" && path == "/legacy") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println(SETTINGS_PAGE);
    } else if (method == "GET" && path == "/get") {
      getCurrentSettings(client);
    } else if (method == "GET" && path == "/read") {
      if (key == "pin") {
        readPin(client, key, value.toInt());
      }
    } else if (method == "GET" && path == "/ota/check") {
      String result = CWOTA::getInstance()->checkOnly();
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print(result);
    } else if (method == "GET" && path == "/ota/status") {
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print(getOtaStatus());
    } else if (method == "POST" && path == "/ota/rollback") {
      handleOtaRollback(client);
      return;
    } else if (method == "POST" && path == "/ota/update") {
      // Respond first, then start OTA (device will reboot on success)
      client.println("HTTP/1.0 200 OK");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"updating\",\"message\":\"OTA started - device will reboot\"}");
      client.flush();
      client.stop();
      CWOTA::getInstance()->checkAndUpdate();
      return;
    } else if (method == "GET" && path == "/api/clockfaces") {
      sendClockfaceList(client);
    } else if (method == "GET" && path == "/api/widgets") {
      sendWidgetList(client);
    } else if (method == "GET" && path == "/api/widget-state") {
      sendWidgetState(client);
    } else if (method == "POST" && path == "/api/widget/show") {
      handleWidgetShow(client, key, value);
      return;
    } else if (method == "GET" && path == "/backup") {
      exportConfig(client);
    } else if (method == "POST" && path == "/restore") {
      importConfig(client, key, value);
    } else if (method == "POST" && path == "/restart") {
      client.println("HTTP/1.0 204 No Content");
      force_restart = true;
    } else if (method == "POST" && path == "/set") {
      ClockwiseParams::getInstance()->load();
      urlDecode(value);
      handleSet(key, value);
      ClockwiseParams::getInstance()->save();
      client.println("HTTP/1.0 204 No Content");
    }
  }



  void sendClockfaceList(WiFiClient client) {
    auto* p = ClockwiseParams::getInstance();
    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: application/json");
    client.println();
    client.print("[");
    for (uint8_t i = 0; i < CWDriverRegistry::COUNT; i++) {
      const CWClockfaceDriver* d = CWDriverRegistry::get(i);
      if (!d) continue;
      if (i > 0) client.print(",");
      client.printf("{\"index\":%d,\"name\":\"%s\",\"active\":%s}",
                    d->index, d->name,
                    (p->clockFaceIndex == d->index) ? "true" : "false");
    }
    client.print("]");
  }

  void sendWidgetList(WiFiClient client) {
    auto* p = ClockwiseParams::getInstance();
    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: application/json");
    client.println();
    client.print("[");
    client.printf("{\"name\":\"%s\",\"implemented\":true,\"active\":%s}",
                  CWWidgetManager::WIDGET_CLOCK,
                  (p->activeWidget == CWWidgetManager::WIDGET_CLOCK) ? "true" : "false");
    client.printf(",{\"name\":\"%s\",\"implemented\":false,\"active\":%s}",
                  CWWidgetManager::WIDGET_WEATHER,
                  (p->activeWidget == CWWidgetManager::WIDGET_WEATHER) ? "true" : "false");
    client.printf(",{\"name\":\"%s\",\"implemented\":false,\"active\":%s}",
                  CWWidgetManager::WIDGET_NOTIFICATION,
                  (p->activeWidget == CWWidgetManager::WIDGET_NOTIFICATION) ? "true" : "false");
    client.printf(",{\"name\":\"%s\",\"implemented\":false,\"active\":%s}",
                  CWWidgetManager::WIDGET_STOCKS,
                  (p->activeWidget == CWWidgetManager::WIDGET_STOCKS) ? "true" : "false");
    client.printf(",{\"name\":\"%s\",\"implemented\":true,\"active\":%s}",
                  CWWidgetManager::WIDGET_TIMER,
                  (p->activeWidget == CWWidgetManager::WIDGET_TIMER) ? "true" : "false");
    client.print("]");
  }

  void sendWidgetState(WiFiClient client) {
    auto* p = ClockwiseParams::getInstance();
    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: application/json");
    client.println();
    if (onWidgetStateJson) {
      client.print(onWidgetStateJson());
      return;
    }
    client.print("{");
    client.print("\"activeWidget\":\"" + p->activeWidget + "\"");
    client.print(",\"timerRemainingSec\":0");
    client.print(",\"clockfaceName\":\"");
    client.print(CWDriverRegistry::name(p->clockFaceIndex));
    client.print("\"");
    client.print(",\"canReturnToClock\":");
    client.print(p->activeWidget == CWWidgetManager::WIDGET_CLOCK ? "false" : "true");
    client.print("}");
  }

  void handleWidgetShow(WiFiClient client, const String& key, String value) {
    if (key != "spec") {
      client.println("HTTP/1.0 400 Bad Request");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Missing spec query parameter\"}");
      return;
    }

    urlDecode(value);
    value.trim();

    if (!onWidgetSwitch) {
      client.println("HTTP/1.0 503 Service Unavailable");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Widget runtime unavailable\"}");
      return;
    }

    if (!onWidgetSwitch(value)) {
      client.println("HTTP/1.0 422 Unprocessable Entity");
      client.println("Content-Type: application/json");
      client.println();
      client.print("{\"status\":\"error\",\"message\":\"Widget command rejected\"}");
      return;
    }

    client.println("HTTP/1.0 204 No Content");
  }

  void readPin(WiFiClient client, String key, uint16_t pin) {
    ClockwiseParams::getInstance()->load();

    client.println("HTTP/1.0 204 No Content");
    client.printf(HEADER_TEMPLATE_D, key, analogRead(pin));

    client.println();
  }


  /**
   * GET /backup — returns all settings as a JSON file download.
   * The user can save this and restore it later via POST /restore.
   */
  void exportConfig(WiFiClient client) {
    auto* p = ClockwiseParams::getInstance();
    p->load();

    String json = "{";
    json += "\"use24hFormat\":"  + String(p->use24hFormat ? 1 : 0)  + ",";
    json += "\"displayBright\":" + String(p->displayBright)           + ",";
    json += "\"autoBrightMin\":" + String(p->autoBrightMin)           + ",";
    json += "\"autoBrightMax\":" + String(p->autoBrightMax)           + ",";
    json += "\"ldrPin\":"        + String(p->ldrPin)                  + ",";
    json += "\"ledColorOrder\":" + String(p->ledColorOrder)           + ",";
    json += "\"reversePhase\":"  + String(p->reversePhase ? 1 : 0)   + ",";
    json += "\"displayRotation\":" + String(p->displayRotation)       + ",";
    json += "\"timeZone\":\""    + p->timeZone                        + "\",";
    json += "\"ntpServer\":\""   + p->ntpServer                       + "\",";
    json += "\"manualPosix\":\"" + p->manualPosix                     + "\",";
    json += "\"canvasFile\":\""  + p->canvasFile                      + "\",";
    json += "\"canvasServer\":\"" + p->canvasServer                   + "\",";
    json += "\"driver\":"        + String(p->driver)                  + ",";
    json += "\"i2cSpeed\":"      + String(p->i2cSpeed)                + ",";
    json += "\"E_pin\":"         + String(p->E_pin)                   + ",";
    json += "\"autoChange\":"    + String(p->autoChange)              + ",";
    json += "\"brightMethod\":"  + String(p->brightMethod)            + ",";
    json += "\"nightStartH\":"   + String(p->nightStartH)             + ",";
    json += "\"nightStartM\":"   + String(p->nightStartM)             + ",";
    json += "\"nightEndH\":"     + String(p->nightEndH)               + ",";
    json += "\"nightEndM\":"     + String(p->nightEndM)               + ",";
    json += "\"nightBright\":"   + String(p->nightBright)             + ",";
    json += "\"nightMode\":"     + String(p->nightMode)               + ",";
    json += "\"nightLevel\":"    + String(p->nightLevel)              + ",";
    json += "\"nightTrig\":"     + String(p->nightTrigger)            + ",";
    json += "\"nightLdrThr\":"   + String(p->nightLdrThres)           + ",";
    json += "\"nightAction\":"   + String(p->nightAction)             + ",";
    json += "\"nightMinBr\":"    + String(p->nightMinBr)              + ",";
    json += "\"superColor\":"    + String(p->superColor)              + ",";
    json += "\"mqttEnabled\":"   + String(p->mqttEnabled ? 1 : 0)    + ",";
    json += "\"mqttBroker\":\""  + p->mqttBroker                      + "\",";
    json += "\"mqttPort\":"      + String(p->mqttPort)                + ",";
    json += "\"mqttUser\":\""    + p->mqttUser                        + "\",";
    json += "\"mqttPrefix\":\""  + p->mqttPrefix                      + "\",";
    json += "\"activeWidget\":\"" + p->activeWidget                   + "\"";
    // Note: mqttPass intentionally omitted from export for security
    json += "}";

    client.println("HTTP/1.0 200 OK");
    client.println("Content-Type: application/json");
    client.println("Content-Disposition: attachment; filename=\"clockwise-paradise-config.json\"");
    client.println("Content-Length: " + String(json.length()));
    client.println();
    client.print(json);
  }

  /**
   * POST /restore?key=value — applies a single key=value pair from the config JSON.
   * The web UI sends each key individually (same pattern as /set).
   * This endpoint handles the "restore" action from the settings page.
   */
  void importConfig(WiFiClient client, String key, String value) {
    // Reuse the /set handler logic — restore is just bulk /set calls
    processRequest(client, "POST", "/set", key, value);
  }

  void getCurrentSettings(WiFiClient client) {
    ClockwiseParams::getInstance()->load();

    client.println("HTTP/1.0 204 No Content");

    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT, ClockwiseParams::getInstance()->displayBright);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MIN, ClockwiseParams::getInstance()->autoBrightMin);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ABC_MAX, ClockwiseParams::getInstance()->autoBrightMax);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN, ClockwiseParams::getInstance()->swapBlueGreen);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED, ClockwiseParams::getInstance()->swapBlueRed);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_LED_COLOR_ORDER, ClockwiseParams::getInstance()->ledColorOrder);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_REVERSE_PHASE, ClockwiseParams::getInstance()->reversePhase);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_AUTO_CHANGE, ClockwiseParams::getInstance()->autoChange);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT, ClockwiseParams::getInstance()->use24hFormat);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_LDR_PIN, ClockwiseParams::getInstance()->ldrPin);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_TIME_ZONE, ClockwiseParams::getInstance()->timeZone.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_WIFI_SSID, ClockwiseParams::getInstance()->wifiSsid.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_NTP_SERVER, ClockwiseParams::getInstance()->ntpServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_CANVAS_FILE, ClockwiseParams::getInstance()->canvasFile.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_CANVAS_SERVER, ClockwiseParams::getInstance()->canvasServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MANUAL_POSIX, ClockwiseParams::getInstance()->manualPosix.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION, ClockwiseParams::getInstance()->displayRotation);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_DRIVER, ClockwiseParams::getInstance()->driver);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_I2CSPEED, ClockwiseParams::getInstance()->i2cSpeed);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_E_PIN, ClockwiseParams::getInstance()->E_pin);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_AUTO_CHANGE, ClockwiseParams::getInstance()->autoChange);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_BRIGHT_METHOD, ClockwiseParams::getInstance()->brightMethod);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_START_H, ClockwiseParams::getInstance()->nightStartH);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_START_M, ClockwiseParams::getInstance()->nightStartM);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_END_H, ClockwiseParams::getInstance()->nightEndH);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_END_M, ClockwiseParams::getInstance()->nightEndM);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_BRIGHT, ClockwiseParams::getInstance()->nightBright);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_MODE, ClockwiseParams::getInstance()->nightMode);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_LEVEL, ClockwiseParams::getInstance()->nightLevel);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_TRIGGER, ClockwiseParams::getInstance()->nightTrigger);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_LDR_THRES, ClockwiseParams::getInstance()->nightLdrThres);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_ACTION, ClockwiseParams::getInstance()->nightAction);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_NIGHT_MIN_BRT, ClockwiseParams::getInstance()->nightMinBr);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SUPER_COLOR, ClockwiseParams::getInstance()->superColor);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_BIGCLOCK_SERVER, ClockwiseParams::getInstance()->bigclockServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_BIGCLOCK_FILE, ClockwiseParams::getInstance()->bigclockFile.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_TOTAL_DAYS, ClockwiseParams::getInstance()->totalDays);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_MQTT_ENABLED, ClockwiseParams::getInstance()->mqttEnabled);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MQTT_BROKER, ClockwiseParams::getInstance()->mqttBroker.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_MQTT_PORT, ClockwiseParams::getInstance()->mqttPort);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MQTT_PREFIX, ClockwiseParams::getInstance()->mqttPrefix.c_str());

    client.printf(HEADER_TEMPLATE_D, "clockfaceIndex", ClockwiseParams::getInstance()->clockFaceIndex);
    client.printf(HEADER_TEMPLATE_S, "clockfaceName", CWDriverRegistry::name(ClockwiseParams::getInstance()->clockFaceIndex));
    client.printf(HEADER_TEMPLATE_S, "activeWidget", ClockwiseParams::getInstance()->activeWidget.c_str());
    client.printf(HEADER_TEMPLATE_S, "CW_FW_VERSION", CW_FW_VERSION);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_NAME", CW_FW_NAME);
    client.printf(HEADER_TEMPLATE_S, "CLOCKFACE_NAME", CLOCKFACE_NAME);
    // Device IP address — useful for status display
    client.printf(HEADER_TEMPLATE_S, "wifiIP", WiFi.localIP().toString().c_str());
    uint64_t uptimeSec = (uint64_t)(esp_timer_get_time() / 1000000ULL);
    client.printf(HEADER_TEMPLATE_D, "uptimeSec", (uint32_t)uptimeSec);
    client.println();
  }

};
