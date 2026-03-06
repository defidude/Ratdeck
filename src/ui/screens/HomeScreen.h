#pragma once

#include "ui/UIManager.h"

class ReticulumManager;
class SX1262;
class UserConfig;

class HomeScreen : public Screen {
public:
    void update() override;

    void setReticulumManager(ReticulumManager* rns) { _rns = rns; }
    void setRadio(SX1262* radio) { _radio = radio; }
    void setUserConfig(UserConfig* cfg) { _cfg = cfg; }

    const char* title() const override { return "Home"; }
    void draw(LGFX_TDeck& gfx) override;

private:
    ReticulumManager* _rns = nullptr;
    SX1262* _radio = nullptr;
    UserConfig* _cfg = nullptr;
    unsigned long _lastUptime = 0;
    uint32_t _lastHeap = 0;
};
