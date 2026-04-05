#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include <CWOTA.h>
#include "StatusController.h"
#include "SettingsWebPage.h"

#ifndef CLOCKFACE_NAME
  #define CLOCKFACE_NAME "UNKNOWN"
#endif

WiFiServer server(80);

struct ClockwiseWebServer
{
  String httpBuffer;
  bool force_restart;
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
    if (client)
    {
      StatusController::getInstance()->blink_led(100, 1);

      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          httpBuffer.concat(c);

          if (c == '\n')
          {
            uint8_t method_pos = httpBuffer.indexOf(' ');
            uint8_t path_pos = httpBuffer.indexOf(' ', method_pos + 1);

            String method = httpBuffer.substring(0, method_pos);
            String path = httpBuffer.substring(method_pos + 1, path_pos);
            String key = "";
            String value = "";

            if (path.indexOf('?') > 0)
            {
              key = path.substring(path.indexOf('?') + 1, path.indexOf('='));
              value = path.substring(path.indexOf('=') + 1);
              path = path.substring(0, path.indexOf('?'));
            }

            processRequest(client, method, path, key, value);
            httpBuffer = "";
            break;
          }
        }
      }
      delay(1);
      client.stop();
    }
  }

  void processRequest(WiFiClient client, String method, String path, String key, String value)
  {
    if (method == "GET" && path == "/") {
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
      //a baby seal has died due this ifs
      if (key == ClockwiseParams::getInstance()->PREF_DISPLAY_BRIGHT) {
        ClockwiseParams::getInstance()->displayBright = value.toInt();
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
        // dispatcher index (0-based) maps to clockface
        ClockwiseParams::getInstance()->autoChange = ClockwiseParams::getInstance()->autoChange; // no-op stub
        // full dispatcher switching handled at runtime
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

    client.printf(HEADER_TEMPLATE_S, "CW_FW_VERSION", CW_FW_VERSION);
    client.printf(HEADER_TEMPLATE_S, "CW_FW_NAME", CW_FW_NAME);
    client.printf(HEADER_TEMPLATE_S, "CLOCKFACE_NAME", CLOCKFACE_NAME);
    client.println();
  }
  
};
