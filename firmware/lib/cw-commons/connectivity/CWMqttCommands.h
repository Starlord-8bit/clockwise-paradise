// CWMqttCommands.h — MQTT command dispatch and state publishing for CWMqtt
//
// Included directly inside the CWMqtt class body. Not a standalone header.
// Accesses: _client, _base_topic, onClockfaceSwitch, onWidgetSwitch,
//           onBrightnessChange (CWMqtt members).

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
