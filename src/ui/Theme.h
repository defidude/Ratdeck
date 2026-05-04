#pragma once

#include <cstdint>

// =============================================================================
// Ratdeck design constants
// Dark field-console palette tuned for the 320x240 LVGL surface.
// =============================================================================

namespace Theme {

// --- Backgrounds ---
constexpr uint32_t BG             = 0x05080A;  // Screen base
constexpr uint32_t BG_ELEVATED    = 0x0C1418;  // Buttons, inputs, list items
constexpr uint32_t BG_SURFACE     = 0x121D23;  // Modals, dropdowns, shell bars
constexpr uint32_t BG_HOVER       = 0x183033;  // Focus/hover background

// --- Text hierarchy ---
constexpr uint32_t TEXT_PRIMARY   = 0xE4F3F0;  // Primary copy
constexpr uint32_t TEXT_SECONDARY = 0x91A8A5;  // Metadata and labels
constexpr uint32_t TEXT_MUTED     = 0x526866;  // Disabled, placeholders

// --- Brand / interactive ---
constexpr uint32_t PRIMARY        = 0x00E06D;  // Signal green
constexpr uint32_t PRIMARY_MUTED  = 0x00A853;  // Pressed/active state
constexpr uint32_t PRIMARY_SUBTLE = 0x06251A;  // Selection backgrounds
constexpr uint32_t ACCENT         = 0x4FD7FF;  // Console cyan accent

// --- Status ---
constexpr uint32_t SUCCESS        = 0x31E981;  // Online, delivered, connected
constexpr uint32_t WARNING_CLR    = 0xF0C04F;  // Queued, pending, caution
constexpr uint32_t ERROR_CLR      = 0xFF5C6C;  // Failed, error, critical

// --- Structural ---
constexpr uint32_t BORDER         = 0x21383B;  // Subtle structural borders
constexpr uint32_t BORDER_ACTIVE  = 0x00E06D;  // Focus borders
constexpr uint32_t DIVIDER        = 0x102329;  // Hairline separators

// --- Messages ---
constexpr uint32_t MSG_OUT_BG     = 0x082719;  // Outgoing bubble
constexpr uint32_t MSG_IN_BG      = 0x0B151C;  // Incoming bubble

// --- Shell ---
constexpr uint32_t STATUS_BG      = 0x071014;
constexpr uint32_t STATUS_FLASH   = 0x12351F;
constexpr uint32_t TAB_BG         = 0x081115;
constexpr uint32_t TAB_ACTIVE_BG  = 0x0B241B;
constexpr uint32_t TAB_ACTIVE     = 0x00E06D;
constexpr uint32_t TAB_INACTIVE   = 0x91A8A5;
constexpr uint32_t BADGE_BG       = 0xFF5C6C;
constexpr uint32_t TOAST_BG       = 0x00E06D;


// --- Layout Metrics ---
constexpr int SCREEN_W       = 320;
constexpr int SCREEN_H       = 240;
constexpr int STATUS_BAR_H   = 20;
constexpr int TAB_BAR_H      = 26;
constexpr int CONTENT_Y      = STATUS_BAR_H;
constexpr int CONTENT_H      = SCREEN_H - STATUS_BAR_H - TAB_BAR_H;
constexpr int CONTENT_W      = SCREEN_W;

// --- Tab Bar ---
constexpr int TAB_COUNT      = 5;
constexpr int TAB_W          = SCREEN_W / TAB_COUNT;
constexpr int TAB_BADGE_W    = 18;
constexpr int TAB_BADGE_H    = 10;

// --- Spacing (base unit: 4px) ---
constexpr int SP_1 = 4;
constexpr int SP_2 = 8;
constexpr int SP_3 = 12;
constexpr int SP_4 = 16;
constexpr int SP_6 = 24;

}  // namespace Theme

// LVGL color helper - available when LVGL is included
#ifdef LV_CONF_H
#include <lvgl.h>
inline lv_color_t lvColor(uint32_t rgb888) { return lv_color_hex(rgb888); }
#endif
