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
 *
 * Implementation split:
 *   CWMqttDiscovery.h — HA discovery entity table + publishDiscovery()
 *   CWMqttCommands.h  — handleCommand() dispatch + publishState()
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ctype.h>
#include <functional>
#include <core/CWPreferences.h>
#include <core/CWLogic.h>
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
        if (WiFi.status() != WL_CONNECTED) {
            ESP_LOGW("MQTT", "WiFi not connected — skipping MQTT start");
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
        _nextReconfigureAllowedMs = millis() + 2000;
        stopClient();
        begin();
    }

    String sanitizeMqttNodeId(const String& raw) {
        return String(cw::sanitizeMqttNodeId(raw.c_str()).c_str());
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

    // ── HA MQTT Discovery (see CWMqttDiscovery.h) ───────────────────────────
    #include "CWMqttDiscovery.h"

    // ── Command handler + state publisher (see CWMqttCommands.h) ────────────
    #include "CWMqttCommands.h"

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
