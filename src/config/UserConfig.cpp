#include "UserConfig.h"
#include "config/BoardConfig.h"

bool UserConfig::parseJson(const String& json) {
    Serial.printf("[CONFIG] Parsing config (%d bytes)\n", json.length());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("[CONFIG] Parse error: %s\n", err.c_str());
        return false;
    }

    _settings.radioRegion   = constrain((int)(doc["radio_region"] | 0), 0, REGION_COUNT - 1);
    _settings.loraFrequency = doc["lora_freq"] | (long)LORA_DEFAULT_FREQ;
    _settings.loraSF        = doc["lora_sf"]   | (int)LORA_DEFAULT_SF;
    _settings.loraBW        = doc["lora_bw"]   | (long)LORA_DEFAULT_BW;
    _settings.loraCR        = doc["lora_cr"]   | (int)LORA_DEFAULT_CR;
    _settings.loraTxPower   = doc["lora_txp"]  | (int)LORA_DEFAULT_TX_POWER;
    _settings.loraPreamble  = doc["lora_pre"]  | (long)LORA_DEFAULT_PREAMBLE;

    // WiFi mode — migrate from legacy wifi_enabled bool
    int mode = doc["wifi_mode"] | -1;
    if (mode >= 0) {
        _settings.wifiMode = (RatWiFiMode)constrain(mode, 0, 2);
    } else {
        _settings.wifiMode = (doc["wifi_enabled"] | true) ? RAT_WIFI_AP : RAT_WIFI_OFF;
    }
    _settings.wifiAPSSID     = doc["wifi_ap_ssid"]     | "";
    _settings.wifiAPPassword = doc["wifi_ap_pass"]     | WIFI_AP_PASSWORD;
    _settings.wifiSTASelected = constrain((int)(doc["wifi_sta_selected"] | 0), 0, (int)WIFI_STA_MAX_NETWORKS - 1);

    // Migrate legacy single-network config into the multi-network list.
    _settings.wifiSTANetworks.clear();
    JsonArray staArr = doc["wifi_sta_networks"];
    if (staArr) {
        for (JsonObject obj : staArr) {
            if (_settings.wifiSTANetworks.size() >= WIFI_STA_MAX_NETWORKS) break;
            WiFiNetwork n;
            n.ssid = obj["ssid"] | "";
            n.password = obj["pass"] | "";
            _settings.wifiSTANetworks.push_back(n);
        }
    } else {
        WiFiNetwork legacy;
        legacy.ssid = doc["wifi_sta_ssid"] | "";
        legacy.password = doc["wifi_sta_pass"] | "";
        if (!legacy.ssid.isEmpty()) _settings.wifiSTANetworks.push_back(legacy);
    }

    // AutoInterface (LAN auto-discovery)
    _settings.autoIfaceEnabled  = doc["autoiface_en"]    | false;
    _settings.autoIfaceGroupId  = doc["autoiface_group"] | "reticulum";
    _settings.autoIfaceMaxPeers = doc["autoiface_max"]   | 8;

    // TCP outbound connections
    _settings.tcpConnections.clear();
    JsonArray tcpArr = doc["tcp_connections"];
    if (tcpArr) {
        for (JsonObject obj : tcpArr) {
            if (_settings.tcpConnections.size() >= MAX_TCP_CONNECTIONS) break;
            TCPEndpoint ep;
            ep.host = obj["host"] | "";
            ep.port = obj["port"] | TCP_DEFAULT_PORT;
            ep.autoConnect = obj["auto"] | true;
            if (!ep.host.isEmpty()) _settings.tcpConnections.push_back(ep);
        }
    }

    _settings.screenDimTimeout = doc["screen_dim"] | 30;
    _settings.screenOffTimeout = doc["screen_off"] | 60;
    // Brightness: stored as 1-100%. Migrate old 0-255 values.
    int rawBri = doc["brightness"] | 100;
    if (rawBri > 100) rawBri = rawBri * 100 / 255;  // Migrate from PWM to percentage
    _settings.brightness = constrain(rawBri, 1, 100);
    _settings.denseFontMode    = doc["dense_font"] | false;
    _settings.keyboardBrightness = constrain(doc["kb_brightness"] | 100, 0, 100);
    _settings.keyboardAutoOn     = doc["kb_auto_on"] | false;
    _settings.keyboardAutoOff    = doc["kb_auto_off"] | false;
    _settings.trackballSpeed   = doc["trackball_speed"] | 3;
    _settings.touchSensitivity = doc["touch_sens"] | 3;
    _settings.bleEnabled       = false;

    _settings.gpsTimeEnabled     = doc["gps_time"]     | true;
    _settings.gpsLocationEnabled = doc["gps_location"] | false;
    _settings.timezoneIdx        = doc["tz_idx"]       | 6;
    _settings.timezoneSet        = doc["tz_set"]       | false;
    _settings.use24HourTime      = doc["time_24h"]     | false;

    _settings.audioEnabled = doc["audio_on"]  | true;
    _settings.audioVolume  = doc["audio_vol"] | 80;

    _settings.displayName = doc["display_name"] | "";
    _settings.sdStorageEnabled = doc["sd_storage"] | _settings.sdStorageEnabled;
    _settings.announceInterval = doc["announce_int"] | 30;
    if (_settings.announceInterval < 30) _settings.announceInterval = 30;
    if (_settings.announceInterval > 360) _settings.announceInterval = 360;
    _settings.devMode     = doc["dev_mode"]     | false;

    Serial.println("[CONFIG] Settings loaded");
    return true;
}

String UserConfig::serializeToJson() const {
    JsonDocument doc;

    doc["radio_region"] = _settings.radioRegion;
    doc["lora_freq"] = _settings.loraFrequency;
    doc["lora_sf"]   = _settings.loraSF;
    doc["lora_bw"]   = _settings.loraBW;
    doc["lora_cr"]   = _settings.loraCR;
    doc["lora_txp"]  = _settings.loraTxPower;
    doc["lora_pre"]  = _settings.loraPreamble;

    doc["wifi_mode"] = (int)_settings.wifiMode;
    doc["wifi_ap_ssid"] = _settings.wifiAPSSID;
    doc["wifi_ap_pass"] = _settings.wifiAPPassword;
    doc["wifi_sta_selected"] = (int)constrain((int)_settings.wifiSTASelected, 0, (int)WIFI_STA_MAX_NETWORKS - 1);
    JsonArray staArr = doc["wifi_sta_networks"].to<JsonArray>();
    for (size_t slot = 0; slot < WIFI_STA_MAX_NETWORKS; slot++) {
        JsonObject obj = staArr.add<JsonObject>();
        if (slot < _settings.wifiSTANetworks.size()) {
            obj["ssid"] = _settings.wifiSTANetworks[slot].ssid;
            obj["pass"] = _settings.wifiSTANetworks[slot].password;
        } else {
            obj["ssid"] = "";
            obj["pass"] = "";
        }
    }

    doc["autoiface_en"]    = _settings.autoIfaceEnabled;
    doc["autoiface_group"] = _settings.autoIfaceGroupId;
    doc["autoiface_max"]   = _settings.autoIfaceMaxPeers;

    JsonArray tcpArr = doc["tcp_connections"].to<JsonArray>();
    for (auto& ep : _settings.tcpConnections) {
        JsonObject obj = tcpArr.add<JsonObject>();
        obj["host"] = ep.host;
        obj["port"] = ep.port;
        obj["auto"] = ep.autoConnect;
    }

    doc["screen_dim"] = _settings.screenDimTimeout;
    doc["screen_off"] = _settings.screenOffTimeout;
    doc["brightness"] = _settings.brightness;
    doc["dense_font"] = _settings.denseFontMode;
    doc["kb_brightness"] = _settings.keyboardBrightness;
    doc["kb_auto_on"] = _settings.keyboardAutoOn;
    doc["kb_auto_off"] = _settings.keyboardAutoOff;
    doc["trackball_speed"] = _settings.trackballSpeed;
    doc["touch_sens"] = _settings.touchSensitivity;
    doc["ble_enabled"] = false;

    doc["gps_time"]     = _settings.gpsTimeEnabled;
    doc["gps_location"] = _settings.gpsLocationEnabled;
    doc["tz_idx"]       = _settings.timezoneIdx;
    doc["tz_set"]       = _settings.timezoneSet;
    doc["time_24h"]     = _settings.use24HourTime;

    doc["audio_on"]  = _settings.audioEnabled;
    doc["audio_vol"] = _settings.audioVolume;

    doc["display_name"] = _settings.displayName;
    doc["sd_storage"] = _settings.sdStorageEnabled;
    doc["announce_int"] = _settings.announceInterval;
    doc["dev_mode"]     = _settings.devMode;

    String json;
    serializeJson(doc, json);
    return json;
}

bool UserConfig::load(FlashStore& flash) {
    String json = flash.readString(PATH_USER_CONFIG);
    if (json.isEmpty()) {
        Serial.println("[CONFIG] No saved config, using defaults");
        return false;
    }
    return parseJson(json);
}

bool UserConfig::save(FlashStore& flash) {
    String json = serializeToJson();
    bool ok = flash.writeString(PATH_USER_CONFIG, json);
    if (ok) Serial.println("[CONFIG] Settings saved to flash");
    return ok;
}

bool UserConfig::load(SDStore& sd, FlashStore& flash) {
    String json = flash.readString(PATH_USER_CONFIG);
    bool flashOk = false;
    if (!json.isEmpty()) {
        flashOk = parseJson(json);
    } else {
        Serial.println("[CONFIG] No flash config found");
    }

    if (flashOk && _settings.sdStorageEnabled && sd.isReady()) {
        String sdJson = sd.readString(SD_PATH_USER_CONFIG);
        if (!sdJson.isEmpty()) {
            Serial.println("[CONFIG] Loading opt-in SD config");
            bool sdOk = parseJson(sdJson);
            if (sdOk) {
                _settings.sdStorageEnabled = true;
                return true;
            }
            Serial.println("[CONFIG] SD config invalid, keeping flash config");
            return true;
        }
    }

    if (!flashOk && sd.isReady()) {
        String sdJson = sd.readString(SD_PATH_USER_CONFIG);
        if (!sdJson.isEmpty()) {
            Serial.println("[CONFIG] Flash config unavailable, trying SD config");
            bool sdOk = parseJson(sdJson);
            if (sdOk) {
                _settings.sdStorageEnabled = true;
                flash.writeString(PATH_USER_CONFIG, serializeToJson());
                return true;
            }
            Serial.println("[CONFIG] SD config invalid");
        }
    }

    if (!flashOk) Serial.println("[CONFIG] No saved config, using defaults");
    return flashOk;
}

bool UserConfig::save(SDStore& sd, FlashStore& flash) {
    String json = serializeToJson();
    bool ok = false;

    if (_settings.sdStorageEnabled && sd.isReady()) {
        sd.ensureDir("/ratdeck");
        sd.ensureDir("/ratdeck/config");
        if (sd.writeString(SD_PATH_USER_CONFIG, json)) {
            Serial.println("[CONFIG] Saved to SD");
            ok = true;
        } else {
            Serial.println("[CONFIG] SD write failed");
        }
    }

    // Write to flash (backup)
    if (flash.writeString(PATH_USER_CONFIG, json)) {
        Serial.println("[CONFIG] Saved to flash");
        ok = true;
    }

    return ok;
}
