#pragma once

/**
 * CWMqtt.h — MQTT client for Clockwise Paradise
 *
 * Implements Home Assistant MQTT Discovery protocol so the clock
 * auto-appears in HA as a proper device — no manual config needed.
 *
 * Discovery topics:  homeassistant/<type>/<device_id>/<object_id>/config
 * State topic:       clockwise-paradise/<mac>/state  (JSON)
 * Command topics:    clockwise-paradise/<mac>/set/<property>
 * Availability:      clockwise-paradise/<mac>/availability  (online/offline LWT)
 *
 * Entities published:
 *   select   — Clockface (1-7)
 *   number   — Brightness (0-255)
 *   select   — Night Mode (nothing/off/big clock)
 *   number   — Night Brightness Level (1-5)
 *   select   — Auto-change Clockface (off/sequence/random)
 *   sensor   — Uptime (days)
 *   binary_sensor — Night Active
 *   button   — Restart
 */

#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include <CWPreferences.h>
#include <StatusController.h>
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "mqtt_client.h"
#ifdef __cplusplus
}
#endif

#define MQTT_STATE_INTERVAL_MS  30000
#define MQTT_DISCOVERY_PREFIX   "homeassistant"

class CWMqtt {
public:
    // Runtime callbacks — set by main.cpp for live control without reboot
    std::function<void(uint8_t)> onClockfaceSwitch = nullptr;
    std::function<void(uint8_t)> onBrightnessChange = nullptr;

    static CWMqtt* getInstance() {
        static CWMqtt instance;
        return &instance;
    }

    void begin() {
        auto* p = ClockwiseParams::getInstance();
        if (!p->mqttEnabled || p->mqttBroker.isEmpty()) return;

        // Use last 6 hex digits of MAC as unique device ID
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char mac_str[13];
        snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        _device_id = String("clockwise_paradise_") + mac_str;
        _base_topic = p->mqttPrefix + "/" + mac_str;

        String broker_uri = "mqtt://" + p->mqttBroker + ":" + String(p->mqttPort);
        String avail_topic = _base_topic + "/availability";

        // IDF v4.4.x flat config struct (v5.x uses nested broker/credentials/session)
        esp_mqtt_client_config_t cfg = {};
        cfg.uri          = broker_uri.c_str();
        cfg.username     = p->mqttUser.isEmpty() ? nullptr : p->mqttUser.c_str();
        cfg.password     = p->mqttPass.isEmpty() ? nullptr : p->mqttPass.c_str();
        cfg.lwt_topic    = avail_topic.c_str();
        cfg.lwt_msg      = "offline";
        cfg.lwt_qos      = 1;
        cfg.lwt_retain   = 1;

        _client = esp_mqtt_client_init(&cfg);
        esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, _event_handler, this);
        esp_mqtt_client_start(_client);

        ESP_LOGI("MQTT", "Connecting to %s (device_id: %s)",
                      broker_uri.c_str(), _device_id.c_str());
        _enabled = true;
    }

    void loop() {
        if (!_enabled || !_connected) return;
        if (millis() - _lastPublish > MQTT_STATE_INTERVAL_MS) {
            publishState();
            _lastPublish = millis();
        }
    }

    void publishState() {
        if (!_enabled || !_connected) return;
        auto* p = ClockwiseParams::getInstance();

        String payload = "{\"brightness\":" + String(p->displayBright)
            + ",\"nightMode\":" + String(p->nightMode)
            + ",\"nightLevel\":" + String(p->nightLevel)
            + ",\"autoChange\":" + String(p->autoChange)
            + ",\"clockface\":" + String(p->clockFaceIndex)
            + ",\"nightActive\":false"
            + ",\"totalDays\":" + String(p->totalDays)
            + ",\"version\":\"" + String(CW_FW_VERSION) + "\"}";
        String topic = _base_topic + "/state";
        esp_mqtt_client_publish(_client, topic.c_str(), payload.c_str(), 0, 1, 0);
    }

private:
    esp_mqtt_client_handle_t _client  = nullptr;
    bool    _enabled   = false;
    bool    _connected = false;
    long    _lastPublish = 0;
    String  _device_id;
    String  _base_topic;

    // ── HA MQTT Discovery ────────────────────────────────────────────────────

    void publishDiscovery() {
        auto* p = ClockwiseParams::getInstance();

        // Device info block (shared across all entities)
        // We build it once as a JSON fragment
        String dev_str = "{\"identifiers\":[\"" + _device_id + "\"]"
            ",\"name\":\"Clockwise Paradise\""
            ",\"model\":\"Clockwise Paradise\""
            ",\"manufacturer\":\"Starlord-8bit\""
            ",\"sw_version\":\"" + String(CW_FW_VERSION) + "\"}";

        String avail  = _base_topic + "/availability";
        String state  = _base_topic + "/state";
        String set    = _base_topic + "/set/";

        struct Entity {
            const char* type;
            const char* object_id;
            const char* name;
            const char* value_template;
            const char* command_topic_suffix;  // nullptr = read-only
            const char* extra_json;            // appended to discovery payload
        };

        Entity entities[] = {
            {
                "number", "brightness", "Brightness",
                "{{ value_json.brightness }}",
                "brightness",
                R"(,"min":0,"max":255,"step":1,"icon":"mdi:brightness-5")"
            },
            {
                "select", "night_mode", "Night Mode",
                "{{ value_json.nightMode }}",
                "nightMode",
                R"(,"options":["0","1","2"],"icon":"mdi:weather-night")"
            },
            {
                "number", "night_level", "Night Brightness Level",
                "{{ value_json.nightLevel }}",
                "nightLevel",
                R"(,"min":1,"max":5,"step":1,"icon":"mdi:moon-waning-crescent")"
            },
            {
                "select", "auto_change", "Auto-change Clockface",
                "{{ value_json.autoChange }}",
                "autoChange",
                R"(,"options":["0","1","2"],"icon":"mdi:shuffle-variant")"
            },
            {
                "sensor", "uptime", "Uptime (days)",
                "{{ value_json.totalDays }}",
                nullptr,
                R"(,"icon":"mdi:timer-outline","state_class":"total_increasing")"
            },
            {
                "binary_sensor", "night_active", "Night Active",
                "{{ 'ON' if value_json.nightActive else 'OFF' }}",
                nullptr,
                R"(,"device_class":"running","icon":"mdi:weather-night")"
            },
        };

        for (auto& e : entities) {
            String disc_topic = String(MQTT_DISCOVERY_PREFIX) + "/" + e.type + "/" +
                                _device_id + "/" + e.object_id + "/config";

            // Build payload manually to keep memory low on ESP32
            String payload = "{";
            payload += "\"name\":\"" + String(e.name) + "\",";
            payload += "\"unique_id\":\"" + _device_id + "_" + e.object_id + "\",";
            payload += "\"availability_topic\":\"" + avail + "\",";
            payload += "\"state_topic\":\"" + state + "\",";
            payload += "\"value_template\":\"" + String(e.value_template) + "\"";
            if (e.command_topic_suffix) {
                payload += ",\"command_topic\":\"" + set + e.command_topic_suffix + "\"";
            }
            payload += ",\"device\":" + dev_str;
            payload += e.extra_json;
            payload += "}";

            esp_mqtt_client_publish(_client, disc_topic.c_str(),
                                    payload.c_str(), 0, 1, 1);  // retain=1
        }

        // Restart button
        String btn_topic = String(MQTT_DISCOVERY_PREFIX) + "/button/" +
                           _device_id + "/restart/config";
        String btn_payload = "{\"name\":\"Restart\","
            "\"unique_id\":\"" + _device_id + "_restart\","
            "\"command_topic\":\"" + set + "restart\","
            "\"availability_topic\":\"" + avail + "\","
            "\"device\":" + dev_str + ","
            "\"icon\":\"mdi:restart\"}";
        esp_mqtt_client_publish(_client, btn_topic.c_str(),
                                btn_payload.c_str(), 0, 1, 1);

        ESP_LOGI("MQTT", "Discovery payloads published");
    }

    // ── Command handler ──────────────────────────────────────────────────────

    void handleCommand(const String& topic, const String& payload) {
        auto* p = ClockwiseParams::getInstance();
        String set_prefix = _base_topic + "/set/";

        if (!topic.startsWith(set_prefix)) return;
        String prop = topic.substring(set_prefix.length());

        if (prop == "brightness") {
            int val = payload.toInt();
            if (val >= 0 && val <= 255) {
              p->displayBright = val;
              p->save();
              if (onBrightnessChange) onBrightnessChange((uint8_t)val);
            }
        } else if (prop == "nightMode") {
            int val = payload.toInt();
            if (val >= 0 && val <= 2) { p->nightMode = val; p->save(); }
        } else if (prop == "nightLevel") {
            int val = payload.toInt();
            if (val >= 1 && val <= 5) { p->nightLevel = val; p->save(); }
        } else if (prop == "autoChange") {
            int val = payload.toInt();
            if (val >= 0 && val <= 2) { p->autoChange = val; p->save(); }
        } else if (prop == "clockface") {
            uint8_t idx = (uint8_t)payload.toInt();
            if (onClockfaceSwitch) {
              onClockfaceSwitch(idx);
            } else {
              ESP_LOGW("MQTT", "clockface switch requested but callback not wired");
            }
        } else if (prop == "restart") {
            StatusController::getInstance()->forceRestart();
        }

        publishState();
    }

    // ── ESP-IDF event handler ────────────────────────────────────────────────

    static void _event_handler(void* args, esp_event_base_t base,
                                int32_t event_id, void* event_data)
    {
        CWMqtt* self = static_cast<CWMqtt*>(args);
        auto*   ev   = static_cast<esp_mqtt_event_handle_t>(event_data);

        switch ((esp_mqtt_event_id_t)event_id) {
            case MQTT_EVENT_CONNECTED:
                self->_connected = true;
                ESP_LOGI("MQTT", "Connected");
                {
                    // Publish availability + subscribe to commands
                    String avail = self->_base_topic + "/availability";
                    String set   = self->_base_topic + "/set/#";
                    esp_mqtt_client_publish(self->_client, avail.c_str(), "online", 0, 1, 1);
                    esp_mqtt_client_subscribe(self->_client, set.c_str(), 1);
                    self->publishDiscovery();
                    self->publishState();
                }
                break;

            case MQTT_EVENT_DISCONNECTED:
                self->_connected = false;
                ESP_LOGW("MQTT", "Disconnected — will retry");
                break;

            case MQTT_EVENT_DATA: {
                String topic(ev->topic, ev->topic_len);
                String data(ev->data, ev->data_len);
                self->handleCommand(topic, data);
                break;
            }
            default: break;
        }
    }
};
