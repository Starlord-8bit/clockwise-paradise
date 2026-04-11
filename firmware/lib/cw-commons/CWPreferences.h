#pragma once

#include <Preferences.h>

#ifndef CW_PREF_DB_NAME
    #define CW_PREF_DB_NAME "clockwise"
#endif


struct ClockwiseParams
{
    Preferences preferences;

    const char* const PREF_SWAP_BLUE_GREEN = "swapBlueGreen";
    const char* const PREF_SWAP_BLUE_RED   = "swapBlueRed";
    const char* const PREF_LED_COLOR_ORDER = "ledColorOrder";  // 0=RGB, 1=RBG, 2=GBR
    const char* const PREF_REVERSE_PHASE   = "reversePhase";
    const char* const PREF_USE_24H_FORMAT  = "use24hFormat";
    const char* const PREF_DISPLAY_BRIGHT  = "displayBright";
    const char* const PREF_DISPLAY_ABC_MIN = "autoBrightMin";
    const char* const PREF_DISPLAY_ABC_MAX = "autoBrightMax";
    const char* const PREF_LDR_PIN         = "ldrPin";
    const char* const PREF_TIME_ZONE       = "timeZone";
    const char* const PREF_WIFI_SSID       = "wifiSsid";
    const char* const PREF_WIFI_PASSWORD   = "wifiPwd";
    const char* const PREF_NTP_SERVER      = "ntpServer";
    const char* const PREF_CANVAS_FILE     = "canvasFile";
    const char* const PREF_CANVAS_SERVER   = "canvasServer";
    const char* const PREF_MANUAL_POSIX    = "manualPosix";
    const char* const PREF_DISPLAY_ROTATION = "displayRotation";
    const char* const PREF_DRIVER          = "driver";
    const char* const PREF_I2CSPEED        = "i2cSpeed";
    const char* const PREF_E_PIN           = "E_pin";
    // Auto-change clockface
    const char* const PREF_AUTO_CHANGE     = "autoChange";     // 0=off, 1=sequence, 2=random
    // Brightness method
    const char* const PREF_BRIGHT_METHOD   = "brightMethod";   // 0=auto-LDR, 1=time-based, 2=fixed
    // Night schedule (shared by brightness-method and night-mode)
    const char* const PREF_NIGHT_START_H   = "nightStartH";
    const char* const PREF_NIGHT_START_M   = "nightStartM";
    const char* const PREF_NIGHT_END_H     = "nightEndH";
    const char* const PREF_NIGHT_END_M     = "nightEndM";
    const char* const PREF_NIGHT_BRIGHT    = "nightBright";    // dim brightness for time-based mode
    // Night mode strategy
    const char* const PREF_NIGHT_MODE      = "nightMode";      // 0=nothing, 1=off, 2=big clock
    const char* const PREF_NIGHT_LEVEL     = "nightLevel";     // 1-5 brightness for big clock
    const char* const PREF_SUPER_COLOR     = "superColor";     // RGB565 digit color for big clock
    const char* const PREF_BIGCLOCK_SERVER = "bigclockSrv";
    const char* const PREF_BIGCLOCK_FILE   = "bigclockFile";
    // Uptime counter
    const char* const PREF_TOTAL_DAYS      = "totalDays";
    const char* const PREF_CLOCKFACE_INDEX  = "clockfaceIdx"; // persisted clockface index (0-based)
    // OTA
    const char* const PREF_OTA_ENABLED    = "otaEnabled";
    const char* const PREF_OTA_OWNER      = "otaOwner";
    const char* const PREF_OTA_REPO       = "otaRepo";
    const char* const PREF_OTA_ASSET      = "otaAsset";
    // MQTT
    const char* const PREF_MQTT_ENABLED    = "mqttEnabled";
    const char* const PREF_MQTT_BROKER     = "mqttBroker";
    const char* const PREF_MQTT_PORT       = "mqttPort";
    const char* const PREF_MQTT_USER       = "mqttUser";
    const char* const PREF_MQTT_PASS       = "mqttPass";
    const char* const PREF_MQTT_PREFIX     = "mqttPrefix";

    // LED colour order constants
    static const uint8_t LED_ORDER_RGB = 0;
    static const uint8_t LED_ORDER_RBG = 1;
    static const uint8_t LED_ORDER_GBR = 2;

    // Auto-change constants
    static const uint8_t AUTO_CHANGE_OFF      = 0;
    static const uint8_t AUTO_CHANGE_SEQUENCE = 1;
    static const uint8_t AUTO_CHANGE_RANDOM   = 2;

    // Legacy swap booleans (kept for NVS backwards-compat)
    bool swapBlueGreen;
    bool swapBlueRed;

    uint8_t  ledColorOrder;   // 0=RGB, 1=RBG, 2=GBR
    bool     reversePhase;
    bool     use24hFormat;
    uint8_t  displayBright;
    uint16_t autoBrightMin;
    uint16_t autoBrightMax;
    uint8_t  ldrPin;
    String   timeZone;
    String   wifiSsid;
    String   wifiPwd;
    String   ntpServer;
    String   canvasFile;
    String   canvasServer;
    String   manualPosix;
    uint8_t  displayRotation;
    uint8_t  driver;
    uint32_t i2cSpeed;
    uint8_t  E_pin;
    uint8_t  autoChange;      // 0=off, 1=sequence, 2=random
    // Brightness
    uint8_t  brightMethod;    // 0=auto-LDR, 1=time-based, 2=fixed
    uint8_t  nightStartH;
    uint8_t  nightStartM;
    uint8_t  nightEndH;
    uint8_t  nightEndM;
    uint8_t  nightBright;     // brightness during time-based night window
    // Night mode
    uint8_t  nightMode;       // 0=nothing, 1=off, 2=big clock
    uint8_t  nightLevel;      // 1-5 brightness for big clock
    uint16_t superColor;      // RGB565 digit color
    String   bigclockServer;
    String   bigclockFile;
    // Uptime
    uint32_t totalDays;
    uint8_t  clockFaceIndex; // 0-based persisted clockface selection
    // OTA
    bool     otaEnabled;
    String   otaOwner;
    String   otaRepo;
    String   otaAssetName;
    // MQTT
    bool     mqttEnabled;
    String   mqttBroker;
    uint16_t mqttPort;
    String   mqttUser;
    String   mqttPass;
    String   mqttPrefix;

    ClockwiseParams() {
        preferences.begin("clockwise", false);
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
        preferences.putUInt(PREF_BRIGHT_METHOD, brightMethod);
        preferences.putUInt(PREF_NIGHT_START_H, nightStartH);
        preferences.putUInt(PREF_NIGHT_START_M, nightStartM);
        preferences.putUInt(PREF_NIGHT_END_H, nightEndH);
        preferences.putUInt(PREF_NIGHT_END_M, nightEndM);
        preferences.putUInt(PREF_NIGHT_BRIGHT, nightBright);
        preferences.putUInt(PREF_NIGHT_MODE, nightMode);
        preferences.putUInt(PREF_NIGHT_LEVEL, nightLevel);
        preferences.putUInt(PREF_SUPER_COLOR, superColor);
        preferences.putString(PREF_BIGCLOCK_SERVER, bigclockServer);
        preferences.putString(PREF_BIGCLOCK_FILE, bigclockFile);
        preferences.putUInt(PREF_TOTAL_DAYS, totalDays);
        preferences.putUChar(PREF_CLOCKFACE_INDEX, clockFaceIndex);
        preferences.putBool(PREF_OTA_ENABLED, otaEnabled);
        preferences.putString(PREF_OTA_OWNER, otaOwner);
        preferences.putString(PREF_OTA_REPO, otaRepo);
        preferences.putString(PREF_OTA_ASSET, otaAssetName);
        preferences.putBool(PREF_MQTT_ENABLED, mqttEnabled);
        preferences.putString(PREF_MQTT_BROKER, mqttBroker);
        preferences.putUInt(PREF_MQTT_PORT, mqttPort);
        preferences.putString(PREF_MQTT_USER, mqttUser);
        preferences.putString(PREF_MQTT_PASS, mqttPass);
        preferences.putString(PREF_MQTT_PREFIX, mqttPrefix);
    }

    void saveClockfaceIndex()
    {
        preferences.putUChar(PREF_CLOCKFACE_INDEX, clockFaceIndex);
    }

    void saveTotalDays()
    {
        preferences.putUInt(PREF_TOTAL_DAYS, totalDays);
    }

    void load()
    {
        swapBlueGreen = preferences.getBool(PREF_SWAP_BLUE_GREEN, false);
        swapBlueRed   = preferences.getBool(PREF_SWAP_BLUE_RED, false);
        // Derive ledColorOrder from legacy values on first load
        uint8_t defaultOrder = LED_ORDER_RGB;
        if (swapBlueGreen) defaultOrder = LED_ORDER_RBG;
        if (swapBlueRed)   defaultOrder = LED_ORDER_GBR;
        ledColorOrder = preferences.getUInt(PREF_LED_COLOR_ORDER, defaultOrder);

        reversePhase  = preferences.getBool(PREF_REVERSE_PHASE, false);
        use24hFormat  = preferences.getBool(PREF_USE_24H_FORMAT, true);
        displayBright = preferences.getUInt(PREF_DISPLAY_BRIGHT, 32);
        autoBrightMin = preferences.getUInt(PREF_DISPLAY_ABC_MIN, 0);
        autoBrightMax = preferences.getUInt(PREF_DISPLAY_ABC_MAX, 0);
        ldrPin        = preferences.getUInt(PREF_LDR_PIN, 35);
        timeZone      = preferences.getString(PREF_TIME_ZONE, "America/Sao_Paulo");
        wifiSsid      = preferences.getString(PREF_WIFI_SSID, "");
        wifiPwd       = preferences.getString(PREF_WIFI_PASSWORD, "");
        ntpServer     = preferences.getString(PREF_NTP_SERVER, "time.google.com");
        canvasFile    = preferences.getString(PREF_CANVAS_FILE, "");
        canvasServer  = preferences.getString(PREF_CANVAS_SERVER, "raw.githubusercontent.com");
        manualPosix   = preferences.getString(PREF_MANUAL_POSIX, "");
        displayRotation = preferences.getUInt(PREF_DISPLAY_ROTATION, 0);
        driver        = preferences.getUInt(PREF_DRIVER, 0);
        i2cSpeed      = preferences.getUInt(PREF_I2CSPEED, (uint32_t)8000000);
        E_pin         = preferences.getUInt(PREF_E_PIN, 18);
        autoChange    = preferences.getUInt(PREF_AUTO_CHANGE, AUTO_CHANGE_OFF);
        brightMethod  = preferences.getUInt(PREF_BRIGHT_METHOD, 0);
        nightStartH   = preferences.getUInt(PREF_NIGHT_START_H, 22);
        nightStartM   = preferences.getUInt(PREF_NIGHT_START_M, 0);
        nightEndH     = preferences.getUInt(PREF_NIGHT_END_H, 7);
        nightEndM     = preferences.getUInt(PREF_NIGHT_END_M, 0);
        nightBright   = preferences.getUInt(PREF_NIGHT_BRIGHT, 8);
        nightMode     = preferences.getUInt(PREF_NIGHT_MODE, 0);
        nightLevel    = preferences.getUInt(PREF_NIGHT_LEVEL, 1);
        superColor    = preferences.getUInt(PREF_SUPER_COLOR, 16936);
        bigclockServer = preferences.getString(PREF_BIGCLOCK_SERVER, "raw.githubusercontent.com");
        bigclockFile   = preferences.getString(PREF_BIGCLOCK_FILE, "clockwise-paradise/main/clockfaces/bigclock");
        totalDays     = preferences.getUInt(PREF_TOTAL_DAYS, 0);
        clockFaceIndex = preferences.getUChar(PREF_CLOCKFACE_INDEX, 0);
        // Decode any legacy URL-encoded string values (e.g. Europe%2FStockholm -> Europe/Stockholm)
        // This handles devices that had the bug before v2.4.0
        if (timeZone.indexOf('%') >= 0) {
          timeZone.replace("%2F", "/"); timeZone.replace("%2f", "/");
          timeZone.replace("%20", " "); timeZone.replace("%3A", ":");
          preferences.putString(PREF_TIME_ZONE, timeZone);
        }
        if (ntpServer.indexOf('%') >= 0) {
          ntpServer.replace("%2F", "/"); ntpServer.replace("%20", " ");
          preferences.putString(PREF_NTP_SERVER, ntpServer);
        }
        otaEnabled    = preferences.getBool(PREF_OTA_ENABLED, true);
        otaOwner      = preferences.getString(PREF_OTA_OWNER, "Starlord-8bit");
        otaRepo       = preferences.getString(PREF_OTA_REPO, "clockwise-paradise");
        otaAssetName  = preferences.getString(PREF_OTA_ASSET, "clockwise-paradise.bin");
        mqttEnabled   = preferences.getBool(PREF_MQTT_ENABLED, false);
        mqttBroker    = preferences.getString(PREF_MQTT_BROKER, "");
        mqttPort      = preferences.getUInt(PREF_MQTT_PORT, 1883);
        mqttUser      = preferences.getString(PREF_MQTT_USER, "");
        mqttPass      = preferences.getString(PREF_MQTT_PASS, "");
        mqttPrefix    = preferences.getString(PREF_MQTT_PREFIX, "clockwise");
    }
};
