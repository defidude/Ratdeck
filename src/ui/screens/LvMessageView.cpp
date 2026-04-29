#include "LvMessageView.h"
#include "ui/Theme.h"
#include "ui/LvTheme.h"
#include "ui/LvTabBar.h"
#include "reticulum/LXMFManager.h"
#include "reticulum/AnnounceManager.h"
#include <Arduino.h>
#include <time.h>
#include "fonts/fonts.h"

std::string LvMessageView::getPeerName() {
    if (_am) {
        std::string name = _am->lookupName(_peerHex);
        if (!name.empty()) return name;
    }
    return _peerHex.substr(0, 12);
}

void LvMessageView::createUI(lv_obj_t* parent) {
    _screen = parent;
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    // Use flex column layout: header, messages (grows), input
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 0, 0);

    const lv_font_t* font = &lv_font_ratdeck_12;
    int headerH = 22;
    int inputH = 28;

    // Header bar (top)
    _header = lv_obj_create(parent);
    lv_obj_set_size(_header, lv_pct(100), headerH);
    lv_obj_set_style_bg_color(_header, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_header, lv_color_hex(Theme::BORDER), 0);
    lv_obj_set_style_border_width(_header, 1, 0);
    lv_obj_set_style_border_side(_header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(_header, 0, 0);
    lv_obj_set_style_radius(_header, 0, 0);
    lv_obj_clear_flag(_header, LV_OBJ_FLAG_SCROLLABLE);

    _lblHeader = lv_label_create(_header);
    lv_obj_set_style_text_font(_lblHeader, &lv_font_ratdeck_14, 0);
    lv_obj_set_style_text_color(_lblHeader, lv_color_hex(Theme::ACCENT), 0);
    lv_obj_align(_lblHeader, LV_ALIGN_LEFT_MID, 4, 0);

    // Make header tappable for back navigation
    lv_obj_add_flag(_header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_header, [](lv_event_t* e) {
        auto* self = (LvMessageView*)lv_event_get_user_data(e);
        if (self->_onBack) self->_onBack();
    }, LV_EVENT_CLICKED, this);

    // Message scroll area (middle, grows to fill)
    _msgScroll = lv_obj_create(parent);
    lv_obj_set_width(_msgScroll, lv_pct(100));
    lv_obj_set_flex_grow(_msgScroll, 1);
    lv_obj_set_style_bg_color(_msgScroll, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_msgScroll, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_msgScroll, 0, 0);
    lv_obj_set_style_pad_all(_msgScroll, 4, 0);
    lv_obj_set_style_pad_row(_msgScroll, 6, 0);
    lv_obj_set_style_radius(_msgScroll, 0, 0);
    lv_obj_set_layout(_msgScroll, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_msgScroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_style(_msgScroll, LvTheme::styleScrollbar(), LV_PART_SCROLLBAR);

    // Input row (bottom, just above tab bar)
    _inputRow = lv_obj_create(parent);
    lv_obj_set_size(_inputRow, lv_pct(100), inputH);
    lv_obj_set_style_bg_color(_inputRow, lv_color_hex(Theme::BG), 0);
    lv_obj_set_style_bg_opa(_inputRow, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_inputRow, lv_color_hex(Theme::BORDER), 0);
    lv_obj_set_style_border_width(_inputRow, 1, 0);
    lv_obj_set_style_border_side(_inputRow, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(_inputRow, 3, 0);
    lv_obj_set_style_radius(_inputRow, 0, 0);
    lv_obj_clear_flag(_inputRow, LV_OBJ_FLAG_SCROLLABLE);

    _textarea = lv_textarea_create(_inputRow);
    lv_obj_set_size(_textarea, Theme::CONTENT_W - 50, 22);
    lv_obj_align(_textarea, LV_ALIGN_LEFT_MID, 0, 0);
    lv_textarea_set_one_line(_textarea, true);
    lv_textarea_set_placeholder_text(_textarea, "Type message...");
    lv_obj_add_style(_textarea, LvTheme::styleTextarea(), 0);
    lv_obj_set_style_border_width(_textarea, 0, 0);
    lv_obj_set_style_text_font(_textarea, font, 0);
    lv_obj_set_style_pad_all(_textarea, 2, 0);

    _btnSend = lv_btn_create(_inputRow);
    lv_obj_set_size(_btnSend, 40, 22);
    lv_obj_align(_btnSend, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_style(_btnSend, LvTheme::styleBtn(), 0);
    lv_obj_set_style_pad_all(_btnSend, 0, 0);
    lv_obj_t* sendLbl = lv_label_create(_btnSend);
    lv_obj_set_style_text_font(sendLbl, &lv_font_ratdeck_10, 0);
    lv_label_set_text(sendLbl, "Send");
    lv_obj_center(sendLbl);
    lv_obj_add_event_cb(_btnSend, [](lv_event_t* e) {
        auto* self = (LvMessageView*)lv_event_get_user_data(e);
        self->sendCurrentMessage();
    }, LV_EVENT_CLICKED, this);
}

void LvMessageView::destroyUI() {
    _header = nullptr;
    _lblHeader = nullptr;
    _msgScroll = nullptr;
    _inputRow = nullptr;
    _textarea = nullptr;
    _btnSend = nullptr;
    LvScreen::destroyUI();
}

void LvMessageView::onEnter() {
    if (_lxmf) {
        _lxmf->markRead(_peerHex);
        // Update unread badge on Messages tab
        if (_ui) _ui->lvTabBar().setUnreadCount(LvTabBar::TAB_MSGS, _lxmf->unreadCount());
        // Register status callback — partial update without full rebuild
        std::string peer = _peerHex;
        _lxmf->setStatusCallback([this, peer](const std::string& peerHex, double ts, LXMFStatus newStatus) {
            if (peerHex != peer) return;
            for (int i = _cachedMsgs.size() - 1; i >= 0; i--) {
                if (!_cachedMsgs[i].incoming && fabs(_cachedMsgs[i].timestamp - ts) < 1.0) {
                    _cachedMsgs[i].status = newStatus;
                    updateMessageStatus(i, newStatus);
                    return;
                }
            }
        });
    }
    _lastMsgCount = -1;
    _lastRefreshMs = 0;
    _inputText.clear();

    if (_lblHeader) {
        char header[48];
        snprintf(header, sizeof(header), "< %s", getPeerName().c_str());
        lv_label_set_text(_lblHeader, header);
    }
    if (_textarea) {
        lv_textarea_set_text(_textarea, "");
    }
    _cachedMsgs.clear();  // Force fresh load
    rebuildMessages();
}

void LvMessageView::onExit() {
    if (_lxmf) _lxmf->setStatusCallback(nullptr);
    _inputText.clear();
    _cachedMsgs.clear();
}

void LvMessageView::refreshUI() {
    if (!_lxmf) return;
    unsigned long now = millis();
    if (now - _lastRefreshMs < REFRESH_INTERVAL_MS) return;
    _lastRefreshMs = now;

    // Only reload from disk when message count changes (new messages arrive)
    auto* summary = _lxmf->getConversationSummary(_peerHex);
    if (summary && summary->totalCount == (int)_cachedMsgs.size()) return;

    auto newMsgs = _lxmf->getMessages(_peerHex);
    if (newMsgs.size() != _cachedMsgs.size()) {
        if (newMsgs.size() > _cachedMsgs.size()) {
            // Incremental append — only create widgets for new messages
            size_t oldCount = _cachedMsgs.size();
            _cachedMsgs = std::move(newMsgs);
            _lastMsgCount = (int)_cachedMsgs.size();
            _lastRefreshMs = millis();
            for (size_t i = oldCount; i < _cachedMsgs.size(); i++) {
                appendMessage(_cachedMsgs[i]);
            }
            lv_obj_scroll_to_y(_msgScroll, LV_COORD_MAX, LV_ANIM_OFF);
        } else {
            // Count decreased (deletion?) — full rebuild
            _cachedMsgs = std::move(newMsgs);
            _lastMsgCount = (int)_cachedMsgs.size();
            rebuildMessages();
        }
        // Mark as read since user is actively viewing this conversation
        _lxmf->markRead(_peerHex);
        if (_ui) _ui->lvTabBar().setUnreadCount(LvTabBar::TAB_MSGS, _lxmf->unreadCount());
    }
}

void LvMessageView::appendMessage(const LXMFMessage& msg) {
    if (!_msgScroll) return;

    const lv_font_t* font = &lv_font_ratdeck_12;
    int maxBubbleW = Theme::CONTENT_W * 3 / 4;

    // Bubble container. Reserve pad_bottom for the timestamp row so the
    // bubble's allocated height in the flex scroll includes it — without
    // this, the time renders into the row gap and looks like it's hanging
    // outside the bubble.
    lv_obj_t* bubble = lv_obj_create(_msgScroll);
    lv_obj_set_width(bubble, Theme::CONTENT_W - 12);
    lv_obj_set_style_pad_all(bubble, 0, 0);
    lv_obj_set_style_pad_bottom(bubble, 14, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);

    // Message text in a rounded box
    lv_obj_t* box = lv_obj_create(bubble);
    lv_obj_set_style_radius(box, 4, 0);
    lv_obj_set_style_pad_all(box, 5, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_width(box, LV_SIZE_CONTENT);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    if (msg.incoming) {
        lv_obj_set_style_bg_color(box, lv_color_hex(Theme::MSG_IN_BG), 0);
        lv_obj_align(box, LV_ALIGN_TOP_LEFT, 0, 0);
    } else {
        lv_obj_set_style_bg_color(box, lv_color_hex(Theme::MSG_OUT_BG), 0);
        lv_obj_align(box, LV_ALIGN_TOP_RIGHT, 0, 0);
    }
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);

    // Message text color — incoming is plain text, outgoing reflects delivery status
    uint32_t textColor = Theme::TEXT_PRIMARY; // incoming default
    if (!msg.incoming) {
        switch (msg.status) {
            case LXMFStatus::QUEUED:
            case LXMFStatus::SENDING:
                textColor = Theme::TEXT_SECONDARY; break;
            case LXMFStatus::SENT:
            case LXMFStatus::DELIVERED:
                textColor = Theme::TEXT_PRIMARY; break;
            case LXMFStatus::FAILED:
                textColor = Theme::ERROR_CLR; break;
            default:
                textColor = Theme::TEXT_PRIMARY; break;
        }
    }
    lv_obj_t* lbl = lv_label_create(box);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(textColor), 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, maxBubbleW - 14);
    lv_label_set_text(lbl, msg.content.c_str());

    // Status indicator for outgoing (tracked for partial updates).
    // Glyph + colour come from applyStatusGlyph so single-check vs
    // double-check rendering stays in one place.
    if (!msg.incoming) {
        lv_obj_t* statusLbl = lv_label_create(box);
        lv_obj_set_style_text_font(statusLbl, &lv_font_montserrat_12, 0);
        lv_obj_align(statusLbl, LV_ALIGN_BOTTOM_RIGHT, 0, 1);
        applyStatusGlyph(statusLbl, msg.status);
        _statusLabels.push_back(statusLbl);
        _textLabels.push_back(lbl);
    } else {
        _statusLabels.push_back(nullptr);
        _textLabels.push_back(nullptr);
    }

    // Timestamp below bubble
    if (msg.timestamp > 1700000000) {
        time_t t = (time_t)msg.timestamp;
        struct tm* tm = localtime(&t);
        if (tm) {
            char timeBuf[8];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tm->tm_hour, tm->tm_min);
            lv_obj_t* timeLbl = lv_label_create(bubble);
            lv_obj_set_style_text_font(timeLbl, &lv_font_ratdeck_10, 0);
            lv_obj_set_style_text_color(timeLbl, lv_color_hex(Theme::TEXT_MUTED), 0);
            lv_label_set_text(timeLbl, timeBuf);
            if (msg.incoming) {
                lv_obj_align_to(timeLbl, box, LV_ALIGN_OUT_BOTTOM_LEFT, 2, 1);
            } else {
                lv_obj_align_to(timeLbl, box, LV_ALIGN_OUT_BOTTOM_RIGHT, -2, 1);
            }
        }
    }
}

void LvMessageView::rebuildMessages() {
    if (!_lxmf || !_msgScroll) return;

    // Only load from disk if _cachedMsgs is empty (first call or after send)
    if (_cachedMsgs.empty()) {
        _cachedMsgs = _lxmf->getMessages(_peerHex);
    }
    _lastMsgCount = (int)_cachedMsgs.size();
    _lastRefreshMs = millis();
    lv_obj_clean(_msgScroll);
    _statusLabels.clear();
    _textLabels.clear();

    for (const auto& msg : _cachedMsgs) {
        appendMessage(msg);
    }

    // Auto-scroll to bottom
    lv_obj_scroll_to_y(_msgScroll, LV_COORD_MAX, LV_ANIM_OFF);
}

void LvMessageView::updateMessageStatus(int msgIdx, LXMFStatus status) {
    if (msgIdx < 0 || msgIdx >= (int)_statusLabels.size()) return;
    lv_obj_t* statusLbl = _statusLabels[msgIdx];
    lv_obj_t* textLbl = _textLabels[msgIdx];
    if (!statusLbl) return;  // Incoming message, no status label

    applyStatusGlyph(statusLbl, status);

    // Update text color to match status
    if (textLbl) {
        uint32_t textColor = Theme::TEXT_PRIMARY;
        if (status == LXMFStatus::QUEUED || status == LXMFStatus::SENDING) {
            textColor = Theme::TEXT_SECONDARY;
        } else if (status == LXMFStatus::FAILED) {
            textColor = Theme::ERROR_CLR;
        }
        lv_obj_set_style_text_color(textLbl, lv_color_hex(textColor), 0);
    }
}

void LvMessageView::applyStatusGlyph(lv_obj_t* lbl, LXMFStatus status) {
    if (!lbl) return;
    const char* glyph;
    uint32_t color;
    switch (status) {
        case LXMFStatus::DELIVERED:
            glyph = LV_SYMBOL_OK LV_SYMBOL_OK;  // Double check
            color = Theme::SUCCESS;
            break;
        case LXMFStatus::SENT:
            glyph = LV_SYMBOL_OK;               // Single check
            color = Theme::TEXT_MUTED;
            break;
        case LXMFStatus::FAILED:
            glyph = LV_SYMBOL_WARNING;
            color = Theme::ERROR_CLR;
            break;
        case LXMFStatus::QUEUED:
        case LXMFStatus::SENDING:
        default:
            glyph = LV_SYMBOL_REFRESH;          // In-flight
            color = Theme::TEXT_MUTED;
            break;
    }
    lv_label_set_text(lbl, glyph);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
}

void LvMessageView::sendCurrentMessage() {
    if (!_lxmf || _peerHex.empty() || _inputText.empty()) return;

    RNS::Bytes destHash;
    destHash.assignHex(_peerHex.c_str());
    _lxmf->sendMessage(destHash, _inputText.c_str());

    _inputText.clear();
    if (_textarea) lv_textarea_set_text(_textarea, "");
    _cachedMsgs.clear();  // Force fresh load in rebuildMessages
    rebuildMessages();
}

bool LvMessageView::handleKey(const KeyEvent& event) {
    if (event.character == 0x1B) {
        if (_onBack) _onBack();
        return true;
    }

    if (event.del || event.character == 0x08) {
        if (!_inputText.empty()) {
            _inputText.pop_back();
            if (_textarea) lv_textarea_set_text(_textarea, _inputText.c_str());
        } else {
            if (_onBack) _onBack();
        }
        return true;
    }

    if (event.enter || event.character == '\n' || event.character == '\r') {
        sendCurrentMessage();
        return true;
    }

    // Scroll
    if (event.up) {
        if (_msgScroll) lv_obj_scroll_to_y(_msgScroll,
            lv_obj_get_scroll_y(_msgScroll) - 30, LV_ANIM_OFF);
        return true;
    }
    if (event.down) {
        if (_msgScroll) lv_obj_scroll_to_y(_msgScroll,
            lv_obj_get_scroll_y(_msgScroll) + 30, LV_ANIM_OFF);
        return true;
    }

    if (event.character >= 0x20 && event.character < 0x7F) {
        _inputText += (char)event.character;
        if (_textarea) lv_textarea_set_text(_textarea, _inputText.c_str());
        return true;
    }

    return false;
}
