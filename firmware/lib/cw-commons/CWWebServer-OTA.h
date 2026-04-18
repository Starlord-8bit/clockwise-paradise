#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include <CWOTA.h>
#include "StatusController.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

struct CWWebServerOTA {
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
  static void handleOtaUpload(WiFiClient& client, int contentLength) {
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
  static String getOtaStatus() {
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
  static void handleOtaRollback(WiFiClient& client) {
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
};
