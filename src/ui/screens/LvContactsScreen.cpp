#include "LvContactsScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvInput.h"
#include "ui/UIManager.h"
#include "reticulum/AnnounceManager.h"
#include <Arduino.h>
#include "fonts/fonts.h"

void LvContactsScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    _lblEmpty = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblEmpty, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_lblEmpty, lv_color_hex(Theme::TEXT_MUTED), 0);
    lv_label_set_text(_lblEmpty, "No saved contacts");
    lv_obj_center(_lblEmpty);

    _list = lv_obj_create(parent);
    lv_obj_set_size(_list, lv_pct(100), lv_pct(100));
    lv_obj_add_style(_list, LvTheme::styleList(), 0);
    lv_obj_set_layout(_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);

    _lastContactCount = -1;
    rebuildList();
}

void LvContactsScreen::onEnter() {
    _lastContactCount = -1;
    _focusActive = false;
    rebuildList();
}

void LvContactsScreen::refreshUI() {
    if (!_am) return;
    int contacts = 0;
    for (const auto& n : _am->nodes()) { if (n.saved) contacts++; }
    if (contacts != _lastContactCount) {
        rebuildList();
    }
}

void LvContactsScreen::rebuildList() {
    if (!_am || !_list) return;
    _contactIndices.clear();

    lv_obj_clean(_list);

    const auto& nodes = _am->nodes();
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (nodes[i].saved) _contactIndices.push_back(i);
    }
    int count = (int)_contactIndices.size();
    _lastContactCount = count;

    if (count == 0) {
        lv_obj_clear_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_list, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < count; i++) {
        int nodeIdx = _contactIndices[i];
        const auto& node = nodes[nodeIdx];

        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, Theme::CONTENT_W, 32);
        lv_obj_add_style(row, LvTheme::styleListBtn(), 0);
        lv_obj_add_style(row, LvTheme::styleListBtnFocused(), LV_STATE_FOCUSED);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(Theme::BORDER), 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);

        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            auto* self = (LvContactsScreen*)lv_event_get_user_data(e);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            if (idx < (int)self->_contactIndices.size() && self->_onSelect) {
                int nodeIdx = self->_contactIndices[idx];
                self->_onSelect(self->_am->nodes()[nodeIdx].hash.toHex());
            }
        }, LV_EVENT_CLICKED, this);

        lv_group_add_obj(LvInput::group(), row);
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_ON);
        }, LV_EVENT_FOCUSED, nullptr);

        // Contact name
        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, &lv_font_ratdeck_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(Theme::PRIMARY), 0);
        lv_label_set_text(lbl, node.name.c_str());
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

        // Hash suffix (right side, muted)
        lv_obj_t* lblHash = lv_label_create(row);
        lv_obj_set_style_text_font(lblHash, &lv_font_ratdeck_10, 0);
        lv_obj_set_style_text_color(lblHash, lv_color_hex(Theme::TEXT_MUTED), 0);
        std::string shortHash = node.hash.toHex().substr(0, 8);
        lv_label_set_text(lblHash, shortHash.c_str());
        lv_obj_align(lblHash, LV_ALIGN_RIGHT_MID, -8, 0);
    }

    // Clear auto-focus if user hasn't started navigating yet
    if (!_focusActive) {
        lv_obj_t* focused = lv_group_get_focused(LvInput::group());
        if (focused) lv_obj_clear_state(focused, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
    }
}

bool LvContactsScreen::handleLongPress() {
    if (!_am || _contactIndices.empty()) return false;
    // Only consume long-press once the user has actively navigated into
    // the list. Otherwise let main.cpp blank the screen.
    if (!_focusActive) return false;
    lv_obj_t* focused = lv_group_get_focused(LvInput::group());
    if (!focused) return false;
    _deleteIdx = (int)(intptr_t)lv_obj_get_user_data(focused);
    if (_deleteIdx < 0 || _deleteIdx >= (int)_contactIndices.size()) return false;
    _confirmDelete = true;
    if (_ui) _ui->lvStatusBar().showToast("Delete contact? Enter=Yes Esc=No", 5000);
    return true;
}

bool LvContactsScreen::handleKey(const KeyEvent& event) {
    if (!_am || _contactIndices.empty()) return false;

    if (!_focusActive && (event.up || event.down || event.enter)) {
        _focusActive = true;
        lv_obj_t* focused = lv_group_get_focused(LvInput::group());
        if (focused) lv_obj_add_state(focused, LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
        return true;
    }

    if (_confirmDelete) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            if (_deleteIdx >= 0 && _deleteIdx < (int)_contactIndices.size()) {
                int nodeIdx = _contactIndices[_deleteIdx];
                if (nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
                    _am->deleteContact(nodeIdx);
                    if (_ui) _ui->lvStatusBar().showToast("Contact deleted", 1200);
                    rebuildList();
                }
            }
            _confirmDelete = false;
            return true;
        }
        _confirmDelete = false;
        if (_ui) _ui->lvStatusBar().showToast("Cancelled", 800);
        return true;
    }

    return false;
}
