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
#include <ctype.h>
#include <functional>
#include <core/CWPreferences.h>
#include <display/StatusController.h>
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
    std::function<bool(const String&)> onWidgetSwitch = nullptr;
    std::function<void(uint8_t)> onBrightnessChange = nullptr;

    static CWMqtt* getInstance() {
        static CWMqtt instance;
        return &instance;
    }

    void begin() {
        auto* p = ClockwiseParams::getInstance();
        if (_authRetryBlocked) {
            ESP_LOGW("MQTT", "Auth retries blocked until reboot or settings update");
            return;
        }
        if (!p->mqttEnabled || p->mqttBroker.isEmpty()) {
            _enabled = false;
            return;
        }

        if (_client) {
            stopClient();
        }

        // Use last 6 hex digits of MAC as unique device ID
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        char mac_str[13];
        snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        String mqtt_node_id = sanitizeMqttNodeId(p->mqttDeviceId);
        if (mqtt_node_id.isEmpty()) {
            mqtt_node_id = String(mac_str);
        }

        _device_id = String("clockwise_paradise_") + mqtt_node_id;
        _base_topic = p->mqttPrefix + "/" + mqtt_node_id;

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
        if (!_client) {
            ESP_LOGE("MQTT", "Failed to initialize MQTT client");
            _enabled = false;
            return;
        }
        esp_mqtt_client_register_event(_client, MQTT_EVENT_ANY, _event_handler, this);
        esp_err_t start_err = esp_mqtt_client_start(_client);
        if (start_err != ESP_OK) {
            ESP_LOGE("MQTT", "Failed to start client: %s", esp_err_to_name(start_err));
            esp_mqtt_client_destroy(_client);
            _client = nullptr;
            _enabled = false;
            return;
        }
        _started = true;

        ESP_LOGI("MQTT", "Connecting to %s (device_id: %s, topic_node: %s)",
                      broker_uri.c_str(), _device_id.c_str(), mqtt_node_id.c_str());
        _enabled = true;
    }

    void reconfigureFromPreferences() {
        const unsigned long now = millis();
        if ((long)(now - _nextReconfigureAllowedMs) < 0) {
            _reconfigurePending = true;
            _pendingReconfigureAtMs = _nextReconfigureAllowedMs;
            return;
        }
        applyReconfigureNow();
    }

    void loop() {
        if (_reconfigurePending && (long)(millis() - _pendingReconfigureAtMs) >= 0) {
            _reconfigurePending = false;
            applyReconfigureNow();
        }
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
            + ",\"activeWidget\":\"" + p->activeWidget + "\""
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
    uint8_t _authFailureCount = 0;
    bool    _authRetryBlocked = false;
    bool    _started = false;
    bool    _reconfigurePending = false;
    unsigned long _pendingReconfigureAtMs = 0;
    unsigned long _nextReconfigureAllowedMs = 0;

    void applyReconfigureNow() {
        ESP_LOGI("MQTT", "Applying MQTT settings without reboot");
        _authRetryBlocked = false;
        _authFailureCount = 0;
        _nextReconfigureAllowedMs = millis() + 800;
        stopClient();
        begin();
    }

    String sanitizeMqttNodeId(const String& raw) {
        String src = raw;
        src.trim();
        String out;
        out.reserve(src.length());
        for (size_t i = 0; i < src.length(); ++i) {
            const char c = src[i];
            if (isalnum((unsigned char)c) || c == '_' || c == '-') {
                out += (char)tolower((unsigned char)c);
            } else if (c == ' ') {
                out += '_';
            }
        }
        return out;
    }

    void stopClient() {
        if (_client) {
            if (_started) {
                esp_mqtt_client_stop(_client);
            }
            esp_mqtt_client_destroy(_client);
            _client = nullptr;
        }
        _started = false;
        _connected = false;
        _enabled = false;
    }

    bool isAuthFailure(const esp_mqtt_event_handle_t ev) {
        if (!ev || !ev->error_handle) return false;
        auto* err = ev->error_handle;
        if (err->error_type != MQTT_ERROR_TYPE_CONNECTION_REFUSED) return false;
        const int rc = err->connect_return_code;
        // MQTT CONNACK 0x04 (bad username/password), 0x05 (not authorized)
        return rc == 4 || rc == 5;
    }

    void handleAuthFailure() {
        if (_authFailureCount < 255) {
            _authFailureCount++;
        }
        ESP_LOGW("MQTT", "Broker authentication failed (%u/3)", _authFailureCount);
        if (_authFailureCount >= 3) {
            _authRetryBlocked = true;
            ESP_LOGE("MQTT", "MQTT login failed 3 times; stopping retries until reboot or setting change");
            stopClient();
        }
    }

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

        auto logChangeU8 = [&](const char* key, uint8_t oldVal, uint8_t newVal) {
            if (oldVal == newVal) return;
            ESP_LOGI("DeviceCfg", "Setting changed via MQTT: %s: %u -> %u",
                     key, oldVal, newVal);
        };

        if (prop == "brightness") {
            int val = payload.toInt();
            if (val >= 0 && val <= 255) {
              uint8_t oldVal = p->displayBright;
              p->displayBright = val;
              p->save();
              logChangeU8("displayBright", oldVal, p->displayBright);
              if (onBrightnessChange) onBrightnessChange((uint8_t)val);
            }
        } else if (prop == "nightMode") {
            int val = payload.toInt();
            if (val >= 0 && val <= 2) {
                uint8_t oldVal = p->nightMode;
                p->nightMode = val;
                p->save();
                logChangeU8("nightMode", oldVal, p->nightMode);
            }
        } else if (prop == "nightLevel") {
            int val = payload.toInt();
            if (val >= 1 && val <= 5) {
                uint8_t oldVal = p->nightLevel;
                p->nightLevel = val;
                p->save();
                logChangeU8("nightLevel", oldVal, p->nightLevel);
            }
        } else if (prop == "autoChange") {
            int val = payload.toInt();
            if (val >= 0 && val <= 2) {
                uint8_t oldVal = p->autoChange;
                p->autoChange = val;
                p->save();
                logChangeU8("autoChange", oldVal, p->autoChange);
            }
        } else if (prop == "clockface") {
            uint8_t idx = (uint8_t)payload.toInt();
            if (onClockfaceSwitch) {
                onClockfaceSwitch(idx);
            } else {
                ESP_LOGW("MQTT", "clockface switch requested but callback not wired");
            }
        } else if (prop == "widget") {
            String normalized = payload;
            normalized.toLowerCase();
            if (onWidgetSwitch) {
                if (!onWidgetSwitch(normalized)) {
                    ESP_LOGW("MQTT", "Widget '%s' was rejected", normalized.c_str());
                }
            } else {
                ESP_LOGW("MQTT", "widget switch requested but callback not wired");
            }
        } else if (prop == "restart") {
            StatusController::getInstance()->forceRestart("MQTT restart command received");
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
                self->_authFailureCount = 0;
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
                if (!self->_authRetryBlocked) {
                    ESP_LOGW("MQTT", "Disconnected — will retry");
                }
                break;

            case MQTT_EVENT_ERROR:
                if (self->isAuthFailure(ev)) {
                    self->handleAuthFailure();
                }
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
