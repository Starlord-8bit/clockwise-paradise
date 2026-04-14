#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <functional>

#include "widgets/clockface/CWClockfaceDriver.h"

class CWWidgetManager {
public:
    static constexpr const char* WIDGET_CLOCK        = "clock";
    static constexpr const char* WIDGET_WEATHER      = "weather";
    static constexpr const char* WIDGET_NOTIFICATION = "notification";
    static constexpr const char* WIDGET_STOCKS       = "stocks";
    static constexpr const char* WIDGET_TIMER        = "timer";

    // Optional callback fired when active widget changes.
    std::function<void(const String&)> onWidgetChanged = nullptr;

    void begin(Adafruit_GFX* display, CWDateTime* dateTime,
               const CWClockfaceDriver** currentClockface) {
        _display = display;
        _dateTime = dateTime;
        _currentClockface = currentClockface;
        _activeWidget = WIDGET_CLOCK;
        _timerLastDrawSec = UINT32_MAX;
    }

    bool activateClockWidget(uint8_t clockfaceIndex) {
        if (!_display || !_dateTime || !_currentClockface) {
            ESP_LOGW("Widget", "Widget manager is not initialised");
            return false;
        }

        if (!CWDriverRegistry::switchTo(_currentClockface, clockfaceIndex, _display, _dateTime)) {
            return false;
        }

        _activeWidget = WIDGET_CLOCK;
        _activeClockfaceIndex = clockfaceIndex;
        _timerEndMs = 0;
        _timerLastDrawSec = UINT32_MAX;
        notifyWidgetChanged();
        return true;
    }

    bool activateWidgetByName(const String& name, uint8_t clockfaceIndex) {
        String normalizedName;
        uint32_t timerSecs = 0;
        parseWidgetSpec(name, &normalizedName, &timerSecs);

        if (normalizedName == WIDGET_CLOCK) {
            return activateClockWidget(clockfaceIndex);
        }

        if (normalizedName == WIDGET_TIMER) {
            if (timerSecs == 0) {
                ESP_LOGW("Widget", "Timer widget requires duration payload 'timer:<seconds>'");
                return false;
            }
            return activateTimerWidget(timerSecs, clockfaceIndex);
        }

        if (isPlaceholderWidget(normalizedName)) {
            ESP_LOGW("Widget", "Widget '%s' is a placeholder and not implemented yet", normalizedName.c_str());
            return false;
        }

        ESP_LOGW("Widget", "Unknown widget '%s'", normalizedName.c_str());
        return false;
    }

    void update() {
        if (_activeWidget == WIDGET_CLOCK && _currentClockface && *_currentClockface) {
            (*_currentClockface)->update();
            return;
        }

        if (_activeWidget == WIDGET_TIMER) {
            updateTimerWidget();
        }
    }

    const char* activeWidgetName() const {
        return _activeWidget.c_str();
    }

    uint8_t activeClockfaceIndex() const {
        return _activeClockfaceIndex;
    }

    uint32_t timerRemainingSeconds() const {
        if (_activeWidget != WIDGET_TIMER) return 0;
        return remainingTimerSeconds();
    }

    bool canReturnToClock() const {
        return _activeWidget != WIDGET_CLOCK;
    }

    static bool isKnownWidget(const String& name) {
        String normalized = name;
        normalized.toLowerCase();
        return normalized == WIDGET_CLOCK || normalized == WIDGET_WEATHER ||
               normalized == WIDGET_NOTIFICATION || normalized == WIDGET_STOCKS ||
               normalized == WIDGET_TIMER;
    }

private:
    bool activateTimerWidget(uint32_t durationSecs, uint8_t clockfaceIndex) {
        if (!_display || !_currentClockface) {
            ESP_LOGW("Widget", "Timer widget activation failed: manager not initialised");
            return false;
        }

        if (_currentClockface && *_currentClockface && (*_currentClockface)->teardown) {
            (*_currentClockface)->teardown();
        }

        _activeWidget = WIDGET_TIMER;
        _activeClockfaceIndex = clockfaceIndex;
        _timerEndMs = millis() + (durationSecs * 1000UL);
        _timerLastDrawSec = UINT32_MAX;
        drawTimer(remainingTimerSeconds());
        notifyWidgetChanged();
        ESP_LOGI("Widget", "Timer widget activated for %lu seconds", (unsigned long)durationSecs);
        return true;
    }

    void updateTimerWidget() {
        uint32_t nowRemaining = remainingTimerSeconds();
        if (nowRemaining != _timerLastDrawSec) {
            drawTimer(nowRemaining);
            _timerLastDrawSec = nowRemaining;
        }

        if (millis() >= _timerEndMs) {
            ESP_LOGI("Widget", "Timer finished — returning to clock widget");
            activateClockWidget(_activeClockfaceIndex);
        }
    }

    uint32_t remainingTimerSeconds() const {
        if (_timerEndMs == 0) return 0;
        unsigned long now = millis();
        if (now >= _timerEndMs) return 0;
        unsigned long remainMs = _timerEndMs - now;
        return (remainMs + 999UL) / 1000UL;
    }

    void drawTimer(uint32_t remainingSecs) {
        if (!_display) return;
        uint32_t mins = remainingSecs / 60;
        uint32_t secs = remainingSecs % 60;
        char mmss[16];
        snprintf(mmss, sizeof(mmss), "%lu:%02lu", (unsigned long)mins, (unsigned long)secs);

        _display->fillScreen(0);
        _display->setTextWrap(false);
        _display->setTextColor(0xFFFF);
        _display->setTextSize(1);
        _display->setCursor(8, 18);
        _display->print("TIMER");
        _display->setTextSize(2);
        _display->setCursor(6, 34);
        _display->print(mmss);
    }

    static void parseWidgetSpec(const String& input, String* nameOut, uint32_t* timerSecsOut) {
        String normalized = input;
        normalized.toLowerCase();

        int sep = normalized.indexOf(':');
        if (sep < 0) {
            *nameOut = normalized;
            *timerSecsOut = 0;
            return;
        }

        *nameOut = normalized.substring(0, sep);
        String value = normalized.substring(sep + 1);
        *timerSecsOut = (uint32_t)value.toInt();
    }

    void notifyWidgetChanged() {
        if (onWidgetChanged) {
            onWidgetChanged(_activeWidget);
        }
    }

    static bool isPlaceholderWidget(const String& normalizedName) {
        return normalizedName == WIDGET_WEATHER ||
               normalizedName == WIDGET_NOTIFICATION ||
               normalizedName == WIDGET_STOCKS;
    }

    Adafruit_GFX* _display = nullptr;
    CWDateTime* _dateTime = nullptr;
    const CWClockfaceDriver** _currentClockface = nullptr;

    String _activeWidget = WIDGET_CLOCK;
    uint8_t _activeClockfaceIndex = 0;
    unsigned long _timerEndMs = 0;
    uint32_t _timerLastDrawSec = UINT32_MAX;
};
