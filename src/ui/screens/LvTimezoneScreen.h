#pragma once

#include "ui/UIManager.h"
#include "config/Config.h"
#include <functional>

struct TimezoneEntry {
    const char* label;      // Display name (e.g., "New York (EST/EDT)")
    const char* posixTZ;    // POSIX TZ string for setenv("TZ", ...)
    int8_t baseOffset;      // Base UTC offset for display (e.g., -5 for EST)
    uint8_t radioRegion;    // Mapped RadioRegion for ISM band defaults
};

// Major world timezones with DST rules where applicable
static const TimezoneEntry TIMEZONE_TABLE[] = {
    {"Honolulu",              "HST10",                                    -10, REGION_AMERICAS},
    {"Anchorage",             "AKST9AKDT,M3.2.0,M11.1.0",               -9,  REGION_AMERICAS},
    {"Los Angeles",           "PST8PDT,M3.2.0,M11.1.0",                 -8,  REGION_AMERICAS},
    {"Denver",                "MST7MDT,M3.2.0,M11.1.0",                 -7,  REGION_AMERICAS},
    {"Phoenix",               "MST7",                                    -7,  REGION_AMERICAS},
    {"Chicago",               "CST6CDT,M3.2.0,M11.1.0",                 -6,  REGION_AMERICAS},
    {"New York",              "EST5EDT,M3.2.0,M11.1.0",                  -5,  REGION_AMERICAS},
    {"San Juan",              "AST4",                                     -4,  REGION_AMERICAS},
    {"Sao Paulo",             "<-03>3",                                   -3,  REGION_AMERICAS},
    {"London",                "GMT0BST,M3.5.0/1,M10.5.0",                0,   REGION_EUROPE},
    {"Paris",                 "CET-1CEST,M3.5.0,M10.5.0/3",              1,   REGION_EUROPE},
    {"Cairo",                 "EET-2",                                    2,   REGION_EUROPE},
    {"Moscow",                "MSK-3",                                    3,   REGION_EUROPE},
    {"Dubai",                 "<+04>-4",                                  4,   REGION_EUROPE},
    {"Karachi",               "PKT-5",                                    5,   REGION_EUROPE},
    {"Kolkata",               "IST-5:30",                                 5,   REGION_EUROPE},
    {"Bangkok",               "<+07>-7",                                  7,   REGION_ASIA},
    {"Singapore",             "<+08>-8",                                  8,   REGION_ASIA},
    {"Tokyo",                 "JST-9",                                    9,   REGION_ASIA},
    {"Sydney",                "AEST-10AEDT,M10.1.0,M4.1.0/3",           10,   REGION_AUSTRALIA},
    {"Auckland",              "NZST-12NZDT,M9.5.0,M4.1.0/3",            12,   REGION_AUSTRALIA},
};

static constexpr int TIMEZONE_COUNT = sizeof(TIMEZONE_TABLE) / sizeof(TIMEZONE_TABLE[0]);

class LvTimezoneScreen : public LvScreen {
public:
    void createUI(lv_obj_t* parent) override;
    bool handleKey(const KeyEvent& event) override;
    void onEnter() override { _enterTime = millis(); }
    const char* title() const override { return "Timezone"; }

    void setDoneCallback(std::function<void(int tzIndex)> cb) { _doneCb = cb; }

    // Pre-select an index (e.g., from saved config)
    void setSelectedIndex(int idx) { _selectedIdx = idx; }

private:
    std::function<void(int)> _doneCb;
    int _selectedIdx = 6;  // Default: New York (EST/EDT)
    unsigned long _enterTime = 0;
    static constexpr unsigned long ENTER_GUARD_MS = 600;

    lv_obj_t* _roller = nullptr;
    lv_obj_t* _upButton = nullptr;
    lv_obj_t* _downButton = nullptr;
    lv_obj_t* _doneButton = nullptr;

    void stepSelection(int delta);
    void submit(bool enforceEnterGuard);
};
