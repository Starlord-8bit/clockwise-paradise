// CWMqttDiscovery.h — HA MQTT Discovery payloads for CWMqtt
//
// Included directly inside the CWMqtt class body. Not a standalone header.
// Accesses: _client, _device_id, _base_topic (CWMqtt private members).

    void publishDiscovery() {
        // Device info block (shared across all entities)
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
