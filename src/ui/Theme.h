#pragma once

#include <cstdint>

// =============================================================================
// RatDeck — Design System Constants
// Brand orange palette with warm dark neutrals
// =============================================================================

namespace Theme {

// --- Backgrounds (true black base) ---
constexpr uint32_t BG             = 0x000000;  // Pure black (screen bg)
constexpr uint32_t BG_ELEVATED    = 0x0E140E;  // Cards, inputs, list items
constexpr uint32_t BG_SURFACE     = 0x162016;  // Modals, dropdowns, tooltips
constexpr uint32_t BG_HOVER       = 0x1E2E1E;  // Focus/hover background

// --- Text Hierarchy (cool green-tinted for cohesion with brand) ---
constexpr uint32_t TEXT_PRIMARY   = 0xCCE8CC;  // Primary text (green-tinted white)
constexpr uint32_t TEXT_SECONDARY = 0x7A9A7A;  // Timestamps, metadata (green-tinted gray)
constexpr uint32_t TEXT_MUTED     = 0x4A5E4A;  // Disabled, placeholder (green-tinted dark)

// --- Brand / Interactive ---
constexpr uint32_t PRIMARY        = 0x00CC66;  // Emerald green (interactive, focus)
constexpr uint32_t PRIMARY_MUTED  = 0x00AA55;  // Pressed/active state
constexpr uint32_t PRIMARY_SUBTLE = 0x0A1F12;  // Selection backgrounds, subtle highlight
constexpr uint32_t ACCENT         = 0x40E880;  // Headers, brand text (bright emerald)

// --- Status ---
constexpr uint32_t SUCCESS        = 0x00CC66;  // Online, delivered, connected (= PRIMARY)
constexpr uint32_t WARNING_CLR    = 0xE0B83E;  // Queued, pending, caution (golden amber)
constexpr uint32_t ERROR_CLR      = 0xF87171;  // Failed, error, critical (soft red)

// --- Structural ---
constexpr uint32_t BORDER         = 0x1E2E1E;  // Subtle structural borders (= BG_HOVER)
constexpr uint32_t BORDER_ACTIVE  = 0x00CC66;  // Focus borders (= PRIMARY)
constexpr uint32_t DIVIDER        = 0x162016;  // Hairline separators (= BG_SURFACE)

// --- Messages ---
constexpr uint32_t MSG_OUT_BG     = 0x0A1F12;  // Outgoing bubble (= PRIMARY_SUBTLE)
constexpr uint32_t MSG_IN_BG      = 0x0A120A;  // Incoming bubble

// --- Tab Bar ---
constexpr uint32_t TAB_ACTIVE     = 0x00CC66;  // Active tab (= PRIMARY)
constexpr uint32_t TAB_INACTIVE   = 0x9A908A;  // Inactive tabs (= TEXT_SECONDARY)
constexpr uint32_t BADGE_BG       = 0xF87171;  // Unread badge (= ERROR_CLR)


// --- Layout Metrics ---
constexpr int SCREEN_W       = 320;
constexpr int SCREEN_H       = 240;
constexpr int STATUS_BAR_H   = 20;
constexpr int TAB_BAR_H      = 24;
constexpr int CONTENT_Y      = STATUS_BAR_H;
constexpr int CONTENT_H      = SCREEN_H - STATUS_BAR_H - TAB_BAR_H;
constexpr int CONTENT_W      = SCREEN_W;

// --- Tab Bar ---
constexpr int TAB_COUNT      = 5;
constexpr int TAB_W          = SCREEN_W / TAB_COUNT;

// --- Spacing (base unit: 4px) ---
constexpr int SP_1 = 4;
constexpr int SP_2 = 8;
constexpr int SP_3 = 12;
constexpr int SP_4 = 16;
constexpr int SP_6 = 24;

}  // namespace Theme

// LVGL color helper — available when LVGL is included
#ifdef LV_CONF_H
#include <lvgl.h>
inline lv_color_t lvColor(uint32_t rgb888) { return lv_color_hex(rgb888); }
#endif
