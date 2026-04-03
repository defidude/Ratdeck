#include "LvContactsScreen.h"
#include "ui/Theme.h"
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
    lv_obj_set_style_text_color(_lblEmpty, lv_color_hex(Theme::MUTED), 0);
    lv_label_set_text(_lblEmpty, "No saved contacts");
    lv_obj_center(_lblEmpty);

    _list = lv_obj_create(parent);
    lv_obj_set_size(_list, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(_list, 0, 0);
    lv_obj_set_flex_grow(_list, 1);
    lv_obj_set_style_bg_color(_list, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_list, 0, 0);
    lv_obj_set_style_pad_all(_list, 0, 0);
    lv_obj_set_style_pad_row(_list, 0, 0);
    lv_obj_set_style_radius(_list, 0, 0);
    lv_obj_set_layout(_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);

    // Pre-allocate row pool
    const lv_font_t* font = &lv_font_ratdeck_14;
    for (int i = 0; i < ROW_POOL_SIZE; i++) {
        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, Theme::CONTENT_W, 28);
        lv_obj_set_style_bg_color(row, lv_color_hex(Theme::BG), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(Theme::BORDER), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            auto* self = (LvContactsScreen*)lv_event_get_user_data(e);
            int poolIdx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            int dataIdx = self->_viewportStart + poolIdx;
            if (dataIdx < (int)self->_contactIndices.size() && self->_onSelect) {
                self->_selectedIdx = dataIdx;
                self->syncVisibleRows();
                int nodeIdx = self->_contactIndices[dataIdx];
                self->_onSelect(self->_am->nodes()[nodeIdx].hash.toHex());
            }
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, font, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(Theme::ACCENT), 0);
        lv_label_set_text(lbl, "");
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

        _poolRows[i] = row;
        _poolNameLabels[i] = lbl;
    }

    _lastContactCount = -1;
    rebuildList();
}

void LvContactsScreen::onEnter() {
    _lastContactCount = -1;
    _selectedIdx = 0;
    _viewportStart = 0;
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

void LvContactsScreen::updateSelection(int oldIdx, int newIdx) {
    syncVisibleRows();
}

void LvContactsScreen::rebuildList() {
    if (!_am || !_list) return;
    _rows.clear();
    _contactIndices.clear();

    const auto& nodes = _am->nodes();
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (nodes[i].saved) _contactIndices.push_back(i);
    }
    int count = (int)_contactIndices.size();
    _lastContactCount = count;

    if (count == 0) {
        lv_obj_clear_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < ROW_POOL_SIZE; i++) lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_list, LV_OBJ_FLAG_HIDDEN);

    if (_selectedIdx >= count) _selectedIdx = count - 1;
    if (_selectedIdx < 0) _selectedIdx = 0;

    syncVisibleRows();
}

void LvContactsScreen::syncVisibleRows() {
    if (!_am || !_list) return;
    int count = (int)_contactIndices.size();
    const auto& nodes = _am->nodes();

    if (count == 0) {
        for (int i = 0; i < ROW_POOL_SIZE; i++) lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Compute viewport centered on selection
    int halfPool = ROW_POOL_SIZE / 2;
    _viewportStart = _selectedIdx - halfPool;
    if (_viewportStart < 0) _viewportStart = 0;
    if (_viewportStart + ROW_POOL_SIZE > count) {
        _viewportStart = count - ROW_POOL_SIZE;
        if (_viewportStart < 0) _viewportStart = 0;
    }

    for (int i = 0; i < ROW_POOL_SIZE; i++) {
        int contactIdx = _viewportStart + i;
        if (contactIdx >= count) {
            lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        int nodeIdx = _contactIndices[contactIdx];
        if (nodeIdx < 0 || nodeIdx >= (int)nodes.size()) {
            lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto& node = nodes[nodeIdx];
        bool isSelected = (contactIdx == _selectedIdx);

        lv_obj_set_style_bg_color(_poolRows[i], lv_color_hex(
            isSelected ? Theme::SELECTION_BG : Theme::BG), 0);
        lv_label_set_text(_poolNameLabels[i], node.name.c_str());
    }
}

bool LvContactsScreen::handleLongPress() {
    if (!_am || _contactIndices.empty()) return false;
    if (_selectedIdx < 0 || _selectedIdx >= (int)_contactIndices.size()) return false;
    _confirmDelete = true;
    if (_ui) _ui->lvStatusBar().showToast("Delete contact? Enter=Yes Esc=No", 5000);
    return true;
}

bool LvContactsScreen::handleKey(const KeyEvent& event) {
    if (!_am || _contactIndices.empty()) return false;

    // Confirm delete mode
    if (_confirmDelete) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            if (_selectedIdx >= 0 && _selectedIdx < (int)_contactIndices.size()) {
                int nodeIdx = _contactIndices[_selectedIdx];
                if (nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
                    auto& nodes = const_cast<std::vector<DiscoveredNode>&>(_am->nodes());
                    nodes.erase(nodes.begin() + nodeIdx);
                    _am->saveContacts();
                    if (_ui) _ui->lvStatusBar().showToast("Contact deleted", 1200);
                    _selectedIdx = 0;
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

    int count = (int)_contactIndices.size();

    if (event.up) {
        if (_selectedIdx > 0) {
            _selectedIdx--;
            syncVisibleRows();
        }
        return true;
    }
    if (event.down) {
        if (_selectedIdx < count - 1) {
            _selectedIdx++;
            syncVisibleRows();
        }
        return true;
    }
    if (event.enter || event.character == '\n' || event.character == '\r') {
        if (_selectedIdx >= 0 && _selectedIdx < count && _onSelect) {
            int nodeIdx = _contactIndices[_selectedIdx];
            _onSelect(_am->nodes()[nodeIdx].hash.toHex());
        }
        return true;
    }
    return false;
}
