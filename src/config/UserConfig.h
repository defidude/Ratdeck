#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "config/Config.h"
#include "config/BoardConfig.h"

enum RatWiFiMode : uint8_t { RAT_WIFI_OFF = 0, RAT_WIFI_AP = 1, RAT_WIFI_STA = 2 };

struct WiFiNetwork {
    String ssid;
    String password;
};

constexpr size_t WIFI_STA_MAX_NETWORKS = 3;

struct TCPEndpoint {
    String host;
    uint16_t port = TCP_DEFAULT_PORT;
    bool autoConnect = true;
};

struct UserSettings {
    // Radio
    uint8_t radioRegion = REGION_AMERICAS;
    uint32_t loraFrequency = LORA_DEFAULT_FREQ;
    uint8_t loraSF = LORA_DEFAULT_SF;
    uint32_t loraBW = LORA_DEFAULT_BW;
    uint8_t loraCR = LORA_DEFAULT_CR;
    int8_t loraTxPower = LORA_DEFAULT_TX_POWER;
    long loraPreamble = LORA_DEFAULT_PREAMBLE;
    bool loraEnabled = true;

    // WiFi
    RatWiFiMode wifiMode = RAT_WIFI_STA;
    RatWiFiMode wifiRestoreMode = RAT_WIFI_STA;
    String wifiAPSSID;
    String wifiAPPassword = WIFI_AP_PASSWORD;
    std::vector<WiFiNetwork> wifiSTANetworks;
    uint8_t wifiSTASelected = 0;

    // AutoInterface (Reticulum LAN auto-discovery via IPv6 multicast).
    // Active only in STA mode; opt-in until proven stable on real APs.
    bool   autoIfaceEnabled  = false;
    String autoIfaceGroupId  = "reticulum";
    uint8_t autoIfaceMaxPeers = 8;

    // TCP outbound connections (STA mode only)
    std::vector<TCPEndpoint> tcpConnections;

    // Display
    uint16_t screenDimTimeout = 30;   // seconds
    uint16_t screenOffTimeout = 60;   // seconds
    uint8_t brightness = 100;  // Percentage 1-100
    bool denseFontMode = false;       // T-Deck Plus: adaptive font toggle

    // Keyboard
    uint8_t keyboardBrightness = 100; // Percentage 0-100 (0 = off)
    bool keyboardAutoOn = false;      // Backlight ON when switching to ACTIVE power state
    bool keyboardAutoOff = false;     // Backlight OFF when switching from ACTIVE power state

    // Trackball
    uint8_t trackballSpeed = 3;       // 1-5 sensitivity

    // Touch
    uint8_t touchSensitivity = 3;     // 1-5

    // BLE
    bool bleEnabled = false;

    // GPS & Time
    bool gpsTimeEnabled = true;      // GPS time sync (default ON)
    bool gpsLocationEnabled = false; // GPS position tracking (default OFF, user must opt in)
    uint8_t timezoneIdx = 6;         // Index into TIMEZONE_TABLE (default: New York EST/EDT)
    bool timezoneSet = false;        // false = show timezone picker at boot
    bool use24HourTime = false;      // false = 12h (no AM/PM), true = 24h

    // Audio
    bool audioEnabled = true;
    uint8_t audioVolume = 80;  // 0-100

    // Identity
    String displayName;

    // Storage
    bool sdStorageEnabled = false;   // Removable SD stores plaintext unless explicitly enabled

    // Announce
    uint16_t announceInterval = 30; // minutes, 30-360

    // Developer mode — unlocks custom radio parameters
    bool devMode = false;
};

class UserConfig {
public:
    // Flash-only (original API, kept for compatibility)
    bool load(FlashStore& flash);
    bool save(FlashStore& flash);

    // Dual-backend: SD primary, flash fallback
    bool load(SDStore& sd, FlashStore& flash);
    bool save(SDStore& sd, FlashStore& flash);

    UserSettings& settings() { return _settings; }
    const UserSettings& settings() const { return _settings; }

private:
    bool parseJson(const String& json);
    String serializeToJson() const;

    UserSettings _settings;
};
