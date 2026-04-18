#pragma once

#include <WiFi.h>
#include <CWPreferences.h>
#include <CWWidgetManager.h>
#include <CWClockfaceDriver.h>
#include "esp_log.h"
#include "esp_timer.h"

struct CWWebServerDevice {
  static void sendClockfaceList(WiFiClient client) {
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

  static void sendWidgetList(WiFiClient client) {
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

  static void sendWidgetState(WiFiClient client, std::function<String()> onWidgetStateJson) {
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

  static void readPin(WiFiClient client, String key, uint16_t pin) {
    ClockwiseParams::getInstance()->load();
    client.println("HTTP/1.0 204 No Content");
    client.printf("X-%s: %d\r\n", key.c_str(), analogRead(pin));
    client.println();
  }

  static uint64_t getUptimeSec() {
    return (uint64_t)(esp_timer_get_time() / 1000000ULL);
  }
};
