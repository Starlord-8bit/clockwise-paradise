#pragma once

#include <WiFi.h>
#include <functional>
#include <CWPreferences.h>
#include <CWOTA.h>
#include "StatusController.h"
#include "esp_log.h"

// New multi-page UI
#include "CWWebUI.h"
#include "pages/HomePage.h"
#include "pages/ClockPage.h"
#include "pages/SyncPage.h"
#include "pages/HardwarePage.h"
#include "pages/UpdatePage.h"

// Legacy single-page UI (kept temporarily)
#include "SettingsWebPage.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"

#ifndef CLOCKFACE_NAME
  #define CLOCKFACE_NAME "UNKNOWN"
#endif

WiFiServer server(80);

struct ClockwiseWebServer
{
  String httpBuffer;
  bool force_restart;
  uint8_t pending_clockface_index = 255; // 255 = no pending switch

  // Callback set by main.cpp for live clockface switching without reboot.
  // Signature: void switchClockface(uint8_t index)
  // If null, falls back to save+reboot.
  std::function<void(uint8_t)> onClockfaceSwitch = nullptr;
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
      key   = qs.substring(0, qs.indexOf('='));
      value = qs.substring(qs.indexOf('=') + 1);
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
      client.setTimeout(100);
      unsigned long hdr_deadline = millis() + 500;
      while (client.connected() && millis() < hdr_deadline) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) break;
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
    } else if (method == "GET" && path == "/backup") {
      exportConfig(client);
    } else if (method == "POST" && path == "/restore") {
      importConfig(client, key, value);
    } else if (method == "POST" && path == "/restart") {
      client.println("HTTP/1.0 204 No Content");
      force_restart = true;
    } else if (method == "POST" && path == "/set") {
      ClockwiseParams::getInstance()->load();
      // URL-decode value (browser sends e.g. Europe%2FStockholm)
      value.replace("%2F", "/");
      value.replace("%3A", ":");
      value.replace("%20", " ");
      value.replace("%40", "@");
      value.replace("%2B", "+");
      value.replace("%2C", ",");
      //a baby seal has died due this ifs
      if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT) {
        ClockwiseParams::getInstance()->displayBright = value.toInt();
        if (onBrightnessChange) onBrightnessChange((uint8_t)value.toInt());
      } else if (key == ClockwiseParams::getInstance()->PREF_WIFI_SSID) {
        ClockwiseParams::getInstance()->wifiSsid = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_WIFI_PASSWORD) {
        ClockwiseParams::getInstance()->wifiPwd = value;
      } else if (key == "autoBright") {   //autoBright=0010,0800
        ClockwiseParams::getInstance()->autoBrightMin = value.substring(0,4).toInt();
        ClockwiseParams::getInstance()->autoBrightMax = value.substring(5,9).toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_SWAP_BLUE_GREEN) {
        ClockwiseParams::getInstance()->swapBlueGreen = (value == "1");
        // Legacy: map old swapBlueGreen to new ledColorOrder
        if (value == "1") ClockwiseParams::getInstance()->ledColorOrder = ClockwiseParams::LED_ORDER_RBG;
        else if (!ClockwiseParams::getInstance()->swapBlueRed) ClockwiseParams::getInstance()->ledColorOrder = ClockwiseParams::LED_ORDER_RGB;
      } else if (key == ClockwiseParams::getInstance()->PREF_SWAP_BLUE_RED) {
        ClockwiseParams::getInstance()->swapBlueRed = (value == "1");
        // Legacy: map old swapBlueRed to new ledColorOrder
        if (value == "1") ClockwiseParams::getInstance()->ledColorOrder = ClockwiseParams::LED_ORDER_GBR;
        else if (!ClockwiseParams::getInstance()->swapBlueGreen) ClockwiseParams::getInstance()->ledColorOrder = ClockwiseParams::LED_ORDER_RGB;
      } else if (key == ClockwiseParams::getInstance()->PREF_LED_COLOR_ORDER) {
        ClockwiseParams::getInstance()->ledColorOrder = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_REVERSE_PHASE) {
        ClockwiseParams::getInstance()->reversePhase = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_AUTO_CHANGE) {
        ClockwiseParams::getInstance()->autoChange = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_USE_24H_FORMAT) {
        ClockwiseParams::getInstance()->use24hFormat = (value == "1");
        if (on24hFormatChange) on24hFormatChange(value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_LDR_PIN) {
        ClockwiseParams::getInstance()->ldrPin = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_TIME_ZONE) {
        ClockwiseParams::getInstance()->timeZone = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_NTP_SERVER) {
        ClockwiseParams::getInstance()->ntpServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_CANVAS_FILE) {
        ClockwiseParams::getInstance()->canvasFile = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_CANVAS_SERVER) {
        ClockwiseParams::getInstance()->canvasServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_MANUAL_POSIX) {
        ClockwiseParams::getInstance()->manualPosix = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_ROTATION) {
        ClockwiseParams::getInstance()->displayRotation = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_DRIVER) {
        ClockwiseParams::getInstance()->driver = value.toInt();
      }  else if (key == ClockwiseParams::getInstance()->PREF_I2CSPEED) {
        ClockwiseParams::getInstance()->i2cSpeed = value.toInt();
      }  else if (key == ClockwiseParams::getInstance()->PREF_E_PIN) {
        ClockwiseParams::getInstance()->E_pin = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_AUTO_CHANGE) {
        ClockwiseParams::getInstance()->autoChange = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_MQTT_ENABLED) {
        ClockwiseParams::getInstance()->mqttEnabled = (value == "1");
      } else if (key == ClockwiseParams::getInstance()->PREF_MQTT_BROKER) {
        ClockwiseParams::getInstance()->mqttBroker = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_MQTT_PORT) {
        ClockwiseParams::getInstance()->mqttPort = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_MQTT_USER) {
        ClockwiseParams::getInstance()->mqttUser = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_MQTT_PASS) {
        ClockwiseParams::getInstance()->mqttPass = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_MQTT_PREFIX) {
        ClockwiseParams::getInstance()->mqttPrefix = value;
      } else if (key == "clockFaceIndex") {
        uint8_t idx = (uint8_t)value.toInt();
        ClockwiseParams::getInstance()->clockFaceIndex = idx;
        if (onClockfaceSwitch) {
          // Live runtime switch — no reboot needed
          onClockfaceSwitch(idx);
          // Respond before the callback runs (it may block briefly)
        } else {
          // Fallback: save + reboot (dispatcher not wired up yet)
          force_restart = true;
        }
      } else if (key == ClockwiseParams::getInstance()->PREF_BRIGHT_METHOD) {
        ClockwiseParams::getInstance()->brightMethod = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_START_H) {
        ClockwiseParams::getInstance()->nightStartH = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_START_M) {
        ClockwiseParams::getInstance()->nightStartM = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_END_H) {
        ClockwiseParams::getInstance()->nightEndH = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_END_M) {
        ClockwiseParams::getInstance()->nightEndM = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_BRIGHT) {
        ClockwiseParams::getInstance()->nightBright = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_MODE) {
        ClockwiseParams::getInstance()->nightMode = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_NIGHT_LEVEL) {
        ClockwiseParams::getInstance()->nightLevel = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_SUPER_COLOR) {
        ClockwiseParams::getInstance()->superColor = value.toInt();
      } else if (key == ClockwiseParams::getInstance()->PREF_BIGCLOCK_SERVER) {
        ClockwiseParams::getInstance()->bigclockServer = value;
      } else if (key == ClockwiseParams::getInstance()->PREF_BIGCLOCK_FILE) {
        ClockwiseParams::getInstance()->bigclockFile = value;
      }
      ClockwiseParams::getInstance()->save();
      client.println("HTTP/1.0 204 No Content");
    }
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
    json += "\"superColor\":"    + String(p->superColor)              + ",";
    json += "\"mqttEnabled\":"   + String(p->mqttEnabled ? 1 : 0)    + ",";
    json += "\"mqttBroker\":\""  + p->mqttBroker                      + "\",";
    json += "\"mqttPort\":"      + String(p->mqttPort)                + ",";
    json += "\"mqttUser\":\""    + p->mqttUser                        + "\",";
    json += "\"mqttPrefix\":\""  + p->mqttPrefix                      + "\"";
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
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_SUPER_COLOR, ClockwiseParams::getInstance()->superColor);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_BIGCLOCK_SERVER, ClockwiseParams::getInstance()->bigclockServer.c_str());
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_BIGCLOCK_FILE, ClockwiseParams::getInstance()->bigclockFile.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_TOTAL_DAYS, ClockwiseParams::getInstance()->totalDays);
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_MQTT_ENABLED, ClockwiseParams::getInstance()->mqttEnabled);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MQTT_BROKER, ClockwiseParams::getInstance()->mqttBroker.c_str());
    client.printf(HEADER_TEMPLATE_D, ClockwiseParams::getInstance()->PREF_MQTT_PORT, ClockwiseParams::getInstance()->mqttPort);
    client.printf(HEADER_TEMPLATE_S, ClockwiseParams::getInstance()->PREF_MQTT_PREFIX, ClockwiseParams::getInstance()->mqttPrefix.c_str());

    client.printf(HEADER_TEMPLATE_D, "clockfaceIndex", ClockwiseParams::getInstance()->clockFaceIndex);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_VERSION", CW_FW_VERSION);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_NAME", CW_FW_NAME);
    client.printf(HEADER_TEMPLATE_S, "CLOCKFACE_NAME", CLOCKFACE_NAME);
    // Device IP address — useful for status display
    client.printf(HEADER_TEMPLATE_S, "wifiIP", WiFi.localIP().toString().c_str());
    client.println();
  }

};
