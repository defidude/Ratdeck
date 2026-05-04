#pragma once

#include "ui/UIManager.h"

class LvBootScreen : public LvScreen {
public:
    void createUI(lv_obj_t* parent) override;
    const char* title() const override { return "Boot"; }

    void setProgress(float progress, const char* status);

private:
    lv_obj_t* _lblTitle = nullptr;
    lv_obj_t* _bar = nullptr;
};
