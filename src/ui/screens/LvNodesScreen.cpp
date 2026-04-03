#include "LvNodesScreen.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/UIManager.h"
#include "reticulum/AnnounceManager.h"
#include "config/UserConfig.h"
#include <Arduino.h>
#include <algorithm>
#include "fonts/fonts.h"

void LvNodesScreen::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    // Empty state label
    _lblEmpty = lv_label_create(parent);
    lv_obj_set_style_text_font(_lblEmpty, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_lblEmpty, lv_color_hex(Theme::MUTED), 0);
    lv_label_set_text(_lblEmpty, "No nodes discovered");
    lv_obj_center(_lblEmpty);

    // Scrollable list container
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

    // Pre-allocate ROW_POOL_SIZE row widgets
    const lv_font_t* font = &lv_font_ratdeck_14;
    const lv_font_t* smallFont = &lv_font_ratdeck_10;

    for (int i = 0; i < ROW_POOL_SIZE; i++) {
        lv_obj_t* row = lv_obj_create(_list);
        lv_obj_set_size(row, Theme::CONTENT_W, 24);
        lv_obj_set_style_bg_color(row, lv_color_hex(Theme::BG), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_user_data(row, (void*)(intptr_t)i);
        lv_obj_add_event_cb(row, [](lv_event_t* e) {
            auto* self = (LvNodesScreen*)lv_event_get_user_data(e);
            int poolIdx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            int entryIdx = self->_viewportStart + poolIdx;
            if (entryIdx < self->_totalEntries) {
                self->_selectedIdx = entryIdx;
                self->syncVisibleRows();
                int nodeIdx = self->getNodeIdxForEntry(entryIdx);
                if (nodeIdx >= 0 && nodeIdx < (int)self->_am->nodes().size()) {
                    self->showActionMenu(nodeIdx);
                }
            }
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* nameLbl = lv_label_create(row);
        lv_obj_set_style_text_font(nameLbl, font, 0);
        lv_obj_set_style_text_color(nameLbl, lv_color_hex(Theme::PRIMARY), 0);
        lv_label_set_text(nameLbl, "");
        lv_obj_align(nameLbl, LV_ALIGN_LEFT_MID, 8, 0);

        lv_obj_t* infoLbl = lv_label_create(row);
        lv_obj_set_style_text_font(infoLbl, smallFont, 0);
        lv_obj_set_style_text_color(infoLbl, lv_color_hex(Theme::SECONDARY), 0);
        lv_label_set_text(infoLbl, "");
        lv_obj_align(infoLbl, LV_ALIGN_RIGHT_MID, -4, 0);

        _poolRows[i] = row;
        _poolNameLabels[i] = nameLbl;
        _poolInfoLabels[i] = infoLbl;
    }

    _lastNodeCount = -1;
    _lastContactCount = -1;
    updateSortOrder();
    syncVisibleRows();

    // --- Action modal overlay (hidden initially, on top layer so it floats above scroll) ---
    _overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_overlay, 180, 100);
    // Center on screen (accounting for status bar offset)
    lv_obj_set_pos(_overlay, (320 - 180) / 2, 20 + (Theme::CONTENT_H - 100) / 2);
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x001100), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_overlay, 1, 0);
    lv_obj_set_style_border_color(_overlay, lv_color_hex(Theme::PRIMARY), 0);
    lv_obj_set_style_radius(_overlay, 6, 0);
    lv_obj_set_style_pad_all(_overlay, 6, 0);
    lv_obj_set_style_pad_row(_overlay, 2, 0);
    lv_obj_set_layout(_overlay, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);

    const char* menuText[] = {"Add Contact", "Message", "Back"};
    for (int i = 0; i < 3; i++) {
        // Use a container for each menu item so it's tappable with a larger hit area
        lv_obj_t* btn = lv_obj_create(_overlay);
        lv_obj_set_size(btn, 166, 26);
        lv_obj_set_style_bg_color(btn, lv_color_hex(Theme::BG), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_radius(btn, 3, 0);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            auto* self = (LvNodesScreen*)lv_event_get_user_data(e);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            self->_menuIdx = idx;
            // Simulate enter key to execute the menu action
            KeyEvent tap = {};
            tap.enter = true;
            self->handleKey(tap);
        }, LV_EVENT_CLICKED, this);

        _menuLabels[i] = lv_label_create(btn);
        lv_obj_set_style_text_font(_menuLabels[i], &lv_font_ratdeck_14, 0);
        lv_obj_set_style_text_color(_menuLabels[i], lv_color_hex(Theme::PRIMARY), 0);
        lv_label_set_text(_menuLabels[i], menuText[i]);
        lv_obj_center(_menuLabels[i]);

        _menuBtns[i] = btn;
    }

    // Nickname input widgets (hidden in menu mode)
    _nicknameBox = lv_obj_create(_overlay);
    lv_obj_set_size(_nicknameBox, 166, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(_nicknameBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_nicknameBox, 0, 0);
    lv_obj_set_style_pad_all(_nicknameBox, 0, 0);
    lv_obj_set_style_pad_row(_nicknameBox, 2, 0);
    lv_obj_set_layout(_nicknameBox, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_nicknameBox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_nicknameBox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_nicknameBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_nicknameBox, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* nickTitle = lv_label_create(_nicknameBox);
    lv_obj_set_style_text_font(nickTitle, &lv_font_ratdeck_12, 0);
    lv_obj_set_style_text_color(nickTitle, lv_color_hex(Theme::ACCENT), 0);
    lv_label_set_text(nickTitle, "Enter nickname:");

    _nicknameLbl = lv_label_create(_nicknameBox);
    lv_obj_set_style_text_font(_nicknameLbl, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_nicknameLbl, lv_color_hex(Theme::PRIMARY), 0);
    lv_label_set_text(_nicknameLbl, "_");

    _nicknameHint = lv_label_create(_nicknameBox);
    lv_obj_set_style_text_font(_nicknameHint, &lv_font_ratdeck_10, 0);
    lv_obj_set_style_text_color(_nicknameHint, lv_color_hex(Theme::MUTED), 0);
    lv_label_set_text(_nicknameHint, "Enter=Save  Esc=Cancel");
}

void LvNodesScreen::onEnter() {
    _lastNodeCount = -1;
    _lastContactCount = -1;
    _selectedIdx = 0;
    _viewportStart = 0;
    updateSortOrder();
    syncVisibleRows();
}

void LvNodesScreen::refreshUI() {
    if (!_am) return;
    unsigned long now = millis();
    if (now - _lastRebuild < REBUILD_INTERVAL_MS) return;
    int contacts = 0;
    for (const auto& n : _am->nodes()) { if (n.saved) contacts++; }
    int countDelta = abs(_am->nodeCount() - _lastNodeCount);
    int contactDelta = abs(contacts - _lastContactCount);
    if (countDelta > 0 || contactDelta > 0) {
        _lastRebuild = now;
        if (countDelta > 3 || contactDelta > 0) {
            updateSortOrder();
            syncVisibleRows();
        }
    }
}

void LvNodesScreen::updateSortOrder() {
    if (!_am) return;
    const auto& nodes = _am->nodes();
    int count = (int)nodes.size();
    _lastNodeCount = count;

    _sortedContactIndices.clear();
    _sortedOnlineIndices.clear();

    for (int i = 0; i < count; i++) {
        if (nodes[i].saved) _sortedContactIndices.push_back(i);
        else _sortedOnlineIndices.push_back(i);
    }
    _lastContactCount = (int)_sortedContactIndices.size();

    // Sort online nodes: most recently seen first
    std::sort(_sortedOnlineIndices.begin(), _sortedOnlineIndices.end(), [&nodes](int a, int b) {
        return nodes[a].lastSeen > nodes[b].lastSeen;
    });

    // Total entries: contacts header (if any) + contacts + online header + online nodes
    _totalEntries = 0;
    if (!_sortedContactIndices.empty()) _totalEntries += 1 + (int)_sortedContactIndices.size();
    if (count > 0) _totalEntries += 1 + (int)_sortedOnlineIndices.size();

    // Clamp selection
    if (_selectedIdx >= _totalEntries) _selectedIdx = _totalEntries - 1;
    if (_selectedIdx < 0) _selectedIdx = 0;
    // Skip headers
    while (_selectedIdx < _totalEntries && getNodeIdxForEntry(_selectedIdx) == -1) _selectedIdx++;
    if (_selectedIdx >= _totalEntries) _selectedIdx = _totalEntries - 1;

    _dataChanged = true;
}

// Map a logical entry index to a node index, or -1 for section headers
static int mapEntryToNodeIdx(int entry, const std::vector<int>& contacts, const std::vector<int>& online) {
    int pos = 0;
    if (!contacts.empty()) {
        if (entry == pos) return -1; // Contacts header
        pos++;
        if (entry < pos + (int)contacts.size()) return contacts[entry - pos];
        pos += (int)contacts.size();
    }
    // Online header
    if (entry == pos) return -1;
    pos++;
    if (entry < pos + (int)online.size()) return online[entry - pos];
    return -1;
}

// Determine if entry is a header, and which section
// Returns: 0 = contacts header, 1 = online header, -1 = not a header
static int headerType(int entry, const std::vector<int>& contacts, const std::vector<int>& online) {
    int pos = 0;
    if (!contacts.empty()) {
        if (entry == pos) return 0;
        pos += 1 + (int)contacts.size();
    }
    if (entry == pos) return 1;
    return -1;
}

void LvNodesScreen::syncVisibleRows() {
    if (!_am || !_list) return;

    if (_totalEntries == 0) {
        lv_obj_clear_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < ROW_POOL_SIZE; i++) lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(_lblEmpty, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_list, LV_OBJ_FLAG_HIDDEN);

    const auto& nodes = _am->nodes();

    // Compute viewport: center selected item with buffer
    int halfPool = ROW_POOL_SIZE / 2;
    _viewportStart = _selectedIdx - halfPool;
    if (_viewportStart < 0) _viewportStart = 0;
    if (_viewportStart + ROW_POOL_SIZE > _totalEntries) {
        _viewportStart = _totalEntries - ROW_POOL_SIZE;
        if (_viewportStart < 0) _viewportStart = 0;
    }

    for (int i = 0; i < ROW_POOL_SIZE; i++) {
        int entryIdx = _viewportStart + i;
        if (entryIdx >= _totalEntries) {
            lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        lv_obj_clear_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
        bool isSelected = (entryIdx == _selectedIdx);

        int hdr = headerType(entryIdx, _sortedContactIndices, _sortedOnlineIndices);
        if (hdr >= 0) {
            // Section header row
            char buf[32];
            if (hdr == 0) {
                snprintf(buf, sizeof(buf), "Contacts (%d)", (int)_sortedContactIndices.size());
            } else {
                snprintf(buf, sizeof(buf), "Online (%d)", (int)_sortedOnlineIndices.size());
            }
            lv_obj_set_size(_poolRows[i], Theme::CONTENT_W, 22);
            lv_obj_set_style_bg_color(_poolRows[i], lv_color_hex(Theme::BG), 0);
            lv_obj_set_style_border_width(_poolRows[i], 1, 0);
            lv_obj_set_style_border_color(_poolRows[i], lv_color_hex(Theme::BORDER), 0);
            lv_obj_set_style_border_side(_poolRows[i], LV_BORDER_SIDE_BOTTOM, 0);
            lv_obj_set_style_text_font(_poolNameLabels[i], &lv_font_ratdeck_12, 0);
            lv_obj_set_style_text_color(_poolNameLabels[i], lv_color_hex(Theme::ACCENT), 0);
            lv_label_set_text(_poolNameLabels[i], buf);
            lv_obj_align(_poolNameLabels[i], LV_ALIGN_LEFT_MID, 4, 0);
            lv_label_set_text(_poolInfoLabels[i], "");
        } else {
            // Node row
            int nodeIdx = mapEntryToNodeIdx(entryIdx, _sortedContactIndices, _sortedOnlineIndices);
            if (nodeIdx < 0 || nodeIdx >= (int)nodes.size()) {
                lv_obj_add_flag(_poolRows[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            const auto& node = nodes[nodeIdx];

            lv_obj_set_size(_poolRows[i], Theme::CONTENT_W, 24);
            lv_obj_set_style_bg_color(_poolRows[i], lv_color_hex(
                isSelected ? Theme::SELECTION_BG : Theme::BG), 0);
            lv_obj_set_style_border_width(_poolRows[i], 0, 0);
            lv_obj_set_style_border_side(_poolRows[i], LV_BORDER_SIDE_NONE, 0);

            // Name + hash (use smaller font to reduce truncation)
            bool devMode = _cfg && _cfg->settings().devMode;
            std::string truncName = node.name.substr(0, devMode ? 12 : 15);
            std::string displayHash = node.hash.toHex().substr(0, 12);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s [%s]", truncName.c_str(), displayHash.c_str());
            lv_obj_set_style_text_font(_poolNameLabels[i], &lv_font_ratdeck_12, 0);
            lv_obj_set_style_text_color(_poolNameLabels[i], lv_color_hex(
                node.saved ? Theme::ACCENT : Theme::PRIMARY), 0);
            lv_label_set_text(_poolNameLabels[i], buf);
            lv_obj_align(_poolNameLabels[i], LV_ALIGN_LEFT_MID, 8, 0);

            // Hops + age + optional RSSI (dev mode)
            unsigned long ageSec = (millis() - node.lastSeen) / 1000;
            char infoBuf[32];
            if (node.hops > 0 && node.hops < 128) {
                if (ageSec < 60) snprintf(infoBuf, sizeof(infoBuf), "%dhop %lus", node.hops, ageSec);
                else snprintf(infoBuf, sizeof(infoBuf), "%dhop %lum", node.hops, ageSec / 60);
            } else {
                if (ageSec < 60) snprintf(infoBuf, sizeof(infoBuf), "%lus", ageSec);
                else snprintf(infoBuf, sizeof(infoBuf), "%lum", ageSec / 60);
            }
            if (devMode && node.rssi != 0) {
                char rssiBuf[16];
                snprintf(rssiBuf, sizeof(rssiBuf), " %ddB", node.rssi);
                strncat(infoBuf, rssiBuf, sizeof(infoBuf) - strlen(infoBuf) - 1);
            }
            lv_label_set_text(_poolInfoLabels[i], infoBuf);
            lv_obj_align(_poolInfoLabels[i], LV_ALIGN_RIGHT_MID, -4, 0);
        }
    }

    _dataChanged = false;
    scrollToSelected();
}

// Helper: get node index for a given logical entry, -1 for headers
int LvNodesScreen::getNodeIdxForEntry(int entry) const {
    return mapEntryToNodeIdx(entry, _sortedContactIndices, _sortedOnlineIndices);
}

void LvNodesScreen::scrollToSelected() {
    // With widget pool, scrolling is handled by viewport recomputation in syncVisibleRows
    // Find which pool row corresponds to selected entry and scroll to it
    int poolIdx = _selectedIdx - _viewportStart;
    if (poolIdx >= 0 && poolIdx < ROW_POOL_SIZE && _poolRows[poolIdx]) {
        lv_obj_scroll_to_view(_poolRows[poolIdx], LV_ANIM_OFF);
    }
}

// --- Action modal helpers ---

void LvNodesScreen::showActionMenu(int nodeIdx) {
    _actionNodeIdx = nodeIdx;
    _menuIdx = 0;
    _actionState = NodeAction::ACTION_MENU;
    _nicknameText = "";
    if (_overlay) {
        for (int i = 0; i < 3; i++) lv_obj_clear_flag(_menuBtns[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_nicknameBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
        updateMenuSelection();
    }
}

void LvNodesScreen::hideOverlay() {
    _actionState = NodeAction::BROWSE;
    _actionNodeIdx = -1;
    _nicknameText = "";
    if (_overlay) lv_obj_add_flag(_overlay, LV_OBJ_FLAG_HIDDEN);
}

void LvNodesScreen::showNicknameInput() {
    _actionState = NodeAction::NICKNAME_INPUT;
    // Pre-fill with announce name
    if (_am && _actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size()) {
        _nicknameText = String(_am->nodes()[_actionNodeIdx].name.c_str());
    }
    for (int i = 0; i < 3; i++) lv_obj_add_flag(_menuBtns[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_nicknameBox, LV_OBJ_FLAG_HIDDEN);
    updateNicknameDisplay();
}

void LvNodesScreen::updateMenuSelection() {
    for (int i = 0; i < 3; i++) {
        bool sel = (i == _menuIdx);
        lv_obj_set_style_text_color(_menuLabels[i], lv_color_hex(
            sel ? Theme::BG : Theme::PRIMARY), 0);
        lv_obj_set_style_bg_color(_menuBtns[i], lv_color_hex(
            sel ? Theme::PRIMARY : 0x001100), 0);
        lv_obj_set_style_bg_opa(_menuBtns[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_bottom(_menuLabels[i], 2, 0);
        lv_obj_set_style_radius(_menuLabels[i], 3, 0);
    }
}

void LvNodesScreen::updateNicknameDisplay() {
    if (_nicknameLbl) {
        String display = _nicknameText + "_";
        lv_label_set_text(_nicknameLbl, display.c_str());
    }
}

bool LvNodesScreen::handleLongPress() {
    if (!_am || _totalEntries == 0) return false;
    if (_selectedIdx < 0 || _selectedIdx >= _totalEntries) return false;
    int nodeIdx = getNodeIdxForEntry(_selectedIdx);
    if (nodeIdx < 0 || nodeIdx >= (int)_am->nodes().size()) return false;
    const auto& node = _am->nodes()[nodeIdx];
    if (node.saved) {
        _confirmDelete = true;
        if (_ui) _ui->lvStatusBar().showToast("Remove friend? Enter=Yes Esc=No", 5000);
    } else {
        showActionMenu(nodeIdx);
    }
    return true;
}

bool LvNodesScreen::handleKey(const KeyEvent& event) {
    if (!_am || _totalEntries == 0) return false;

    // --- Nickname input mode ---
    if (_actionState == NodeAction::NICKNAME_INPUT) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            // Save contact with nickname
            if (_actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size()) {
                auto& node = const_cast<DiscoveredNode&>(_am->nodes()[_actionNodeIdx]);
                String finalName = _nicknameText;
                finalName.trim();
                if (finalName.isEmpty()) {
                    // Fallback: announce name → hex hash
                    if (!node.name.empty()) finalName = String(node.name.c_str());
                    else finalName = String(node.hash.toHex().substr(0, 12).c_str());
                }
                node.name = finalName.c_str();
                node.saved = true;
                _am->saveContacts();
                if (_ui) _ui->lvStatusBar().showToast("Contact saved!", 1200);
                hideOverlay();
                updateSortOrder();
                syncVisibleRows();
            } else {
                hideOverlay();
            }
            return true;
        }
        if (event.character == 0x1B) { hideOverlay(); return true; }  // Esc
        if (event.character == '\b' || event.character == 0x7F) {
            if (_nicknameText.length() > 0) _nicknameText.remove(_nicknameText.length() - 1);
            updateNicknameDisplay();
            return true;
        }
        if (event.character >= 0x20 && event.character <= 0x7E && _nicknameText.length() < 16) {
            _nicknameText += (char)event.character;
            updateNicknameDisplay();
            return true;
        }
        return true;  // Consume all keys in input mode
    }

    // --- Action menu mode ---
    if (_actionState == NodeAction::ACTION_MENU) {
        if (event.up) {
            if (_menuIdx > 0) { _menuIdx--; updateMenuSelection(); }
            return true;
        }
        if (event.down) {
            if (_menuIdx < 2) { _menuIdx++; updateMenuSelection(); }
            return true;
        }
        if (event.enter || event.character == '\n' || event.character == '\r') {
            switch (_menuIdx) {
                case 0:  // Add Contact
                    showNicknameInput();
                    break;
                case 1:  // Message
                    if (_actionNodeIdx >= 0 && _actionNodeIdx < (int)_am->nodes().size() && _onSelect) {
                        std::string hex = _am->nodes()[_actionNodeIdx].hash.toHex();
                        hideOverlay();
                        _onSelect(hex);
                    } else {
                        hideOverlay();
                    }
                    break;
                case 2:  // Back
                    hideOverlay();
                    break;
            }
            return true;
        }
        if (event.character == 0x1B) { hideOverlay(); return true; }  // Esc
        return true;  // Consume all keys in menu mode
    }

    // --- Browse mode (normal) ---

    // Confirm delete mode
    if (_confirmDelete) {
        if (event.enter || event.character == '\n' || event.character == '\r') {
            int nodeIdx = getNodeIdxForEntry(_selectedIdx);
            if (nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
                auto& nodes = const_cast<std::vector<DiscoveredNode>&>(_am->nodes());
                nodes.erase(nodes.begin() + nodeIdx);
                _am->saveContacts();
                if (_ui) _ui->lvStatusBar().showToast("Contact deleted", 1200);
                _selectedIdx = 0;
                updateSortOrder();
                syncVisibleRows();
            }
            _confirmDelete = false;
            return true;
        }
        _confirmDelete = false;
        if (_ui) _ui->lvStatusBar().showToast("Cancelled", 800);
        return true;
    }

    if (event.up) {
        int prev = _selectedIdx;
        _selectedIdx--;
        while (_selectedIdx >= 0 && getNodeIdxForEntry(_selectedIdx) == -1) _selectedIdx--;
        if (_selectedIdx < 0) _selectedIdx = prev;
        if (_selectedIdx != prev) syncVisibleRows();
        return true;
    }
    if (event.down) {
        int prev = _selectedIdx;
        _selectedIdx++;
        while (_selectedIdx < _totalEntries && getNodeIdxForEntry(_selectedIdx) == -1) _selectedIdx++;
        if (_selectedIdx >= _totalEntries) _selectedIdx = prev;
        if (_selectedIdx != prev) syncVisibleRows();
        return true;
    }
    if (event.enter || event.character == '\n' || event.character == '\r') {
        int nodeIdx = getNodeIdxForEntry(_selectedIdx);
        if (nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
            showActionMenu(nodeIdx);
        }
        return true;
    }
    // 's' or 'S' to save/unsave contact
    if (event.character == 's' || event.character == 'S') {
        int nodeIdx = getNodeIdxForEntry(_selectedIdx);
        if (nodeIdx >= 0 && nodeIdx < (int)_am->nodes().size()) {
            auto& node = const_cast<DiscoveredNode&>(_am->nodes()[nodeIdx]);
            node.saved = !node.saved;
            if (node.saved) {
                _am->saveContacts();
            }
            updateSortOrder();
            syncVisibleRows();
        }
        return true;
    }
    return false;
}
