#pragma once

#include <WiFi.h>
#include <functional>
#include "StatusController.h"

#include "CWWebServer-HTTP.h"
#include "CWWebServer-Router.h"
#include "CWWebServer-OTA.h"
#include "CWWebServer-Settings.h"

extern WiFiServer server;

struct CWWebServerCore {
  bool force_restart;

  CWWebServerCore() : force_restart(false) {}

  void startWebServer() {
    server.begin();
    StatusController::getInstance()->blink_led(100, 3);
  }

  void stopWebServer() {
    server.stop();
  }

  void handleHttpRequest(
    bool& force_restart,
    std::function<void(uint8_t)> onBrightnessChange,
    std::function<void(bool)> on24hFormatChange,
    std::function<bool(uint8_t)> onClockfaceSwitch,
    std::function<bool(const String&)> onWidgetSwitch,
    std::function<String()> onWidgetStateJson
  ) {
    if (force_restart)
      StatusController::getInstance()->forceRestart();

    WiFiClient client = server.available();
    if (!client) return;

    StatusController::getInstance()->blink_led(100, 1);

    // --- Read first line only (method + path) ---
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

    // Parse method + path from first line using HTTP parser
    String method, path, key, value;
    CWWebServerHTTP::parseFirstLine(firstLine, method, path, key, value);

    // --- OTA upload: special handling for Content-Length and Expect: 100-continue ---
    if (method == "POST" && path == "/ota/upload") {
      int contentLength = 0;
      bool expectContinue = false;
      CWWebServerHTTP::drainHeaders(client, 200, contentLength, expectContinue);

      if (expectContinue) {
        client.print("HTTP/1.1 100 Continue\r\n\r\n");
        client.flush();
      }

      CWWebServerOTA::handleOtaUpload(client, contentLength);
      return;
    }

    // --- All other routes: drain headers for Content-Length and /set body ---
    int contentLength = 0;
    bool expectContinue = false;
    CWWebServerHTTP::drainHeaders(client, 100, contentLength, expectContinue);

    if (method == "POST" && path == "/set" && contentLength > 0) {
      cw::logic::RequestBodyReadResult body = CWWebServerSettings::readSetRequestBody(client, contentLength);
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

    // Dispatch to router
    CWWebServerRouter::processRequest(client, method, path, key, value, force_restart, onBrightnessChange, on24hFormatChange, onClockfaceSwitch, onWidgetSwitch, onWidgetStateJson);
    delay(1);
    client.stop();
  }
};
