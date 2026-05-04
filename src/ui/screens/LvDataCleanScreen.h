#pragma once

#include "ui/UIManager.h"
#include <functional>

class LvDataCleanScreen : public LvScreen {
public:
    void createUI(lv_obj_t* parent) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Setup"; }

    // Callback: true = user chose Yes (wipe), false = user chose No (skip)
    void setDoneCallback(std::function<void(bool)> cb) { _doneCb = cb; }

    // Replace buttons with a status message (for showing progress)
    void showStatus(const char* msg);

private:
    lv_obj_t* _yesBox = nullptr;
    lv_obj_t* _noBox = nullptr;
    lv_obj_t* _yesLabel = nullptr;
    lv_obj_t* _noLabel = nullptr;
    lv_obj_t* _hintLabel = nullptr;
    lv_obj_t* _confirmLabel = nullptr;
    lv_obj_t* _statusLabel = nullptr;
    bool _selectedYes = false;
    bool _confirmWipe = false;
    std::function<void(bool)> _doneCb;
    void updateSelection();
};
