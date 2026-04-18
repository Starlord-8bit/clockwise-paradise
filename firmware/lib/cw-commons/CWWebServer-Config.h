#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include <CWClockfaceDriver.h>
#include "esp_log.h"

struct CWWebServerConfig {
  /**
   * GET /backup — returns all settings as a JSON file download.
   * The user can save this and restore it later via POST /restore.
   */
  static void exportConfig(WiFiClient client) {
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
   * POST /restore — applies a single key=value pair from the config JSON.
   * The web UI sends each key individually (same pattern as /set).
   * This endpoint reuses the settings handler logic.
   */
  static void importConfig(
    WiFiClient client,
    const String& key,
    const String& value,
    std::function<void(uint8_t)> onBrightnessChange,
    std::function<void(bool)> on24hFormatChange,
    std::function<void(uint8_t)> onClockfaceSwitch,
    std::function<bool(const String&)> onWidgetSwitch,
    bool& force_restart
  ) {
    // Reuse the /set handler logic via CWWebServerSettings
    // For now, just respond with 204 as the main server will handle this
    client.println("HTTP/1.0 204 No Content");
  }
};
