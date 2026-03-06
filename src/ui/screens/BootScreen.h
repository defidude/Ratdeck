#pragma once

#include "ui/UIManager.h"

class BootScreen : public Screen {
public:
    void setProgress(float progress, const char* status);

    const char* title() const override { return "Boot"; }
    void draw(LGFX_TDeck& gfx) override;

private:
    float _progress = 0;
    char _status[64] = "Starting...";
};
