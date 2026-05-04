#pragma once

#include <lvgl.h>
#include <string>

class LvStatusBar {
public:
    void create(lv_obj_t* parent);
    void update();

    // Status setters
    void setLoRaOnline(bool online);
    void setBLEActive(bool active);
    void setBLEEnabled(bool enabled);
    void setWiFiActive(bool active);
    void setWiFiEnabled(bool enabled);
    void setTCPConnected(bool connected);
    // -1 = disabled/muted; 0 = yellow (idle); >0 = green (peers)
    void setAutoIfacePeers(int n);
    void setGPSFix(bool hasFix);
    void setBatteryPercent(int pct);
    void flashAnnounce();
    void showToast(const char* msg, uint32_t durationMs = 1500);

    // Time display
    void setUse24Hour(bool use24h);
    void updateTime();   // Call at 1 Hz to refresh clock

    lv_obj_t* obj() { return _bar; }

private:
    void refreshIndicators();
    void refreshBattery();
    void refreshTimeColor();

    lv_obj_t* _bar = nullptr;
    lv_obj_t* _lblTime = nullptr;       // Left: current time
    lv_obj_t* _lblLinks = nullptr;      // Center: brand/status text
    lv_obj_t* _lblBatt = nullptr;       // Right: battery %
    lv_obj_t* _toast = nullptr;
    lv_obj_t* _lblToast = nullptr;


    bool _loraOnline = false;
    bool _bleActive = false;
    bool _bleEnabled = false;
    bool _wifiActive = false;
    bool _wifiEnabled = false;
    bool _tcpConnected = false;
    int _autoIfacePeers = -1;  // -1 hidden, 0 yellow, >0 green
    bool _gpsFix = false;
    bool _use24h = false;
    int _battPct = -1;
    int _lastHour = -1;
    int _lastMinute = -1;
    unsigned long _announceFlashEnd = 0;
    unsigned long _toastEnd = 0;
};
