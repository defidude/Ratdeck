#pragma once

#include <string>

class LGFX_TDeck;

class StatusBar {
public:
    void setGfx(LGFX_TDeck* gfx) { _gfx = gfx; }
    void draw(LGFX_TDeck& gfx);
    void update();

    void setLoRaOnline(bool online) { _loraOnline = online; markDirty(); }
    void setBLEActive(bool active) { _bleActive = active; markDirty(); }
    void setWiFiActive(bool active) { _wifiActive = active; markDirty(); }
    void setBatteryPercent(int pct) { if (_battPct != pct) { _battPct = pct; markDirty(); } }
    void setTransportMode(const char* mode) { _transportMode = mode; markDirty(); }
    void flashAnnounce();
    void showToast(const char* msg, uint32_t durationMs = 1500);

    bool isDirty() const { return _dirty; }
    void markDirty() { _dirty = true; }
    void clearDirty() { _dirty = false; }

    friend class UIManager;

private:
    LGFX_TDeck* _gfx = nullptr;
    bool _dirty = true;

    bool _loraOnline = false;
    bool _bleActive = false;
    bool _wifiActive = false;
    int _battPct = -1;
    std::string _transportMode;
    unsigned long _announceFlashEnd = 0;

    // Toast notification
    std::string _toastMsg;
    unsigned long _toastEnd = 0;
};
