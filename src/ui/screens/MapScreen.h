#pragma once

#include "ui/UIManager.h"

class MapScreen : public Screen {
public:
    const char* title() const override { return "Map"; }
    void draw(LGFX_TDeck& gfx) override;
};
