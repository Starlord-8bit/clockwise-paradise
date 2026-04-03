#pragma once

#include <Preferences.h>

#ifndef CW_PREF_DB_NAME
    #define CW_PREF_DB_NAME "clockwise"
#endif


struct ClockwiseParams
{
    Preferences preferences;

    const char* const PREF_SWAP_BLUE_GREEN = "swapBlueGreen";
    const char* const PREF_SWAP_BLUE_RED = "swapBlueRed";
    const char* const PREF_LED_COLOR_ORDER = "ledColorOrder";
    const char* const PREF_REVERSE_PHASE = "reversePhase";
    const char* const PREF_USE_24H_FORMAT = "use24hFormat";
    const char* const PREF_DISPLAY_BRIGHT = "displayBright";
    const char* const PREF_DISPLAY_ABC_MIN = "autoBrightMin";
    const char* const PREF_DISPLAY_ABC_MAX = "autoBrightMax";
    const char* const PREF_LDR_PIN = "ldrPin";
    const char* const PREF_TIME_ZONE = "timeZone";
    const char* const PREF_WIFI_SSID = "wifiSsid";
    const char* const PREF_WIFI_PASSWORD = "wifiPwd";
    const char* const PREF_NTP_SERVER = "ntpServer";
    const char* const PREF_CANVAS_FILE = "canvasFile";
    const char* const PREF_CANVAS_SERVER = "canvasServer";
    const char* const PREF_MANUAL_POSIX = "manualPosix";
    const char* const PREF_DISPLAY_ROTATION = "displayRotation";
    const char* const PREF_DRIVER = "driver";
    const char* const PREF_I2CSPEED = "i2cSpeed";
    const char* const PREF_E_PIN = "E_pin";
    const char* const PREF_AUTO_CHANGE = "autoChange";

    // LED colour order values
    static const uint8_t LED_ORDER_RGB = 0;
    static const uint8_t LED_ORDER_RBG = 1;
    static const uint8_t LED_ORDER_GBR = 2;

    // Auto-change clockface values
    static const uint8_t AUTO_CHANGE_OFF = 0;
    static const uint8_t AUTO_CHANGE_SEQUENCE = 1;
    static const uint8_t AUTO_CHANGE_RANDOM = 2;

    // Kept for backwards compatibility with existing NVS data
    bool swapBlueGreen;
    bool swapBlueRed;

    uint8_t ledColorOrder;  // 0=RGB, 1=RBG, 2=GBR
    bool reversePhase;
    bool use24hFormat;
    uint8_t displayBright;
    uint16_t autoBrightMin;
    uint16_t autoBrightMax;
    uint8_t ldrPin;
    String timeZone;
    String wifiSsid;
    String wifiPwd;
    String ntpServer;
    String canvasFile;
    String canvasServer;
    String manualPosix;
    uint8_t displayRotation;
    uint8_t driver;
    uint32_t i2cSpeed;
    uint8_t E_pin;
    uint8_t autoChange;  // 0=off, 1=sequence, 2=random

    ClockwiseParams() {
        preferences.begin("clockwise", false); 
        //preferences.clear();
    }

    static ClockwiseParams* getInstance() {
        static ClockwiseParams base;
        return &base;
    }

   
    void save()
    {
        preferences.putBool(PREF_SWAP_BLUE_GREEN, swapBlueGreen);
        preferences.putBool(PREF_SWAP_BLUE_RED, swapBlueRed);
        preferences.putUInt(PREF_LED_COLOR_ORDER, ledColorOrder);
        preferences.putBool(PREF_REVERSE_PHASE, reversePhase);
        preferences.putBool(PREF_USE_24H_FORMAT, use24hFormat);
        preferences.putUInt(PREF_DISPLAY_BRIGHT, displayBright);
        preferences.putUInt(PREF_DISPLAY_ABC_MIN, autoBrightMin);
        preferences.putUInt(PREF_DISPLAY_ABC_MAX, autoBrightMax);
        preferences.putUInt(PREF_LDR_PIN, ldrPin);        
        preferences.putString(PREF_TIME_ZONE, timeZone);
        preferences.putString(PREF_WIFI_SSID, wifiSsid);
        preferences.putString(PREF_WIFI_PASSWORD, wifiPwd);
        preferences.putString(PREF_NTP_SERVER, ntpServer);
        preferences.putString(PREF_CANVAS_FILE, canvasFile);
        preferences.putString(PREF_CANVAS_SERVER, canvasServer);
        preferences.putString(PREF_MANUAL_POSIX, manualPosix);
        preferences.putUInt(PREF_DISPLAY_ROTATION, displayRotation);
        preferences.putUInt(PREF_DRIVER, driver);
        preferences.putUInt(PREF_I2CSPEED, i2cSpeed);
        preferences.putUInt(PREF_E_PIN, E_pin);
        preferences.putUInt(PREF_AUTO_CHANGE, autoChange);
    }

    void load()
    {
        // Load legacy booleans for backwards compatibility
        swapBlueGreen = preferences.getBool(PREF_SWAP_BLUE_GREEN, false);
        swapBlueRed = preferences.getBool(PREF_SWAP_BLUE_RED, false);

        // Derive ledColorOrder from legacy values if not explicitly set
        // Existing devices that had swapBlueGreen=true get RBG, swapBlueRed=true gets GBR
        uint8_t defaultOrder = LED_ORDER_RGB;
        if (swapBlueGreen) defaultOrder = LED_ORDER_RBG;
        if (swapBlueRed)   defaultOrder = LED_ORDER_GBR;
        ledColorOrder = preferences.getUInt(PREF_LED_COLOR_ORDER, defaultOrder);

        reversePhase = preferences.getBool(PREF_REVERSE_PHASE, false);
        use24hFormat = preferences.getBool(PREF_USE_24H_FORMAT, true);
        displayBright = preferences.getUInt(PREF_DISPLAY_BRIGHT, 32);
        autoBrightMin = preferences.getUInt(PREF_DISPLAY_ABC_MIN, 0);
        autoBrightMax = preferences.getUInt(PREF_DISPLAY_ABC_MAX, 0);
        ldrPin = preferences.getUInt(PREF_LDR_PIN, 35);        
        timeZone = preferences.getString(PREF_TIME_ZONE, "America/Sao_Paulo");
        wifiSsid = preferences.getString(PREF_WIFI_SSID, "");
        wifiPwd = preferences.getString(PREF_WIFI_PASSWORD, "");
        ntpServer = preferences.getString(PREF_NTP_SERVER, "time.google.com");
        canvasFile = preferences.getString(PREF_CANVAS_FILE, "");
        canvasServer = preferences.getString(PREF_CANVAS_SERVER, "raw.githubusercontent.com");
        manualPosix = preferences.getString(PREF_MANUAL_POSIX, "");
        displayRotation = preferences.getUInt(PREF_DISPLAY_ROTATION, 0);
        driver = preferences.getUInt(PREF_DRIVER, 0);
        i2cSpeed = preferences.getUInt(PREF_I2CSPEED, (uint32_t)8000000);
        E_pin = preferences.getUInt(PREF_E_PIN, 18);
        autoChange = preferences.getUInt(PREF_AUTO_CHANGE, AUTO_CHANGE_OFF);
    }

};
