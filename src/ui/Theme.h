#pragma once

#include <cstdint>

// =============================================================================
// Ratdeck — Cyberpunk Theme Constants (LovyanGFX direct drawing)
// =============================================================================

namespace Theme {

// --- Colors (RGB888 for LovyanGFX) ---
constexpr uint32_t BG           = 0x000000;  // Pure black
constexpr uint32_t PRIMARY      = 0x00FF41;  // Matrix green
constexpr uint32_t SECONDARY    = 0x00CC33;  // Dimmed green
constexpr uint32_t ACCENT       = 0x00FFFF;  // Cyan
constexpr uint32_t MUTED        = 0x336633;  // Dark green
constexpr uint32_t ERROR_CLR    = 0xFF3333;  // Red
constexpr uint32_t WARNING_CLR  = 0xFFFF00;  // Yellow
constexpr uint32_t BORDER       = 0x004400;  // Subtle green
constexpr uint32_t SELECTION_BG = 0x003300;  // Highlight
constexpr uint32_t MSG_OUT_BG   = 0x002200;  // Outgoing bubble
constexpr uint32_t MSG_IN_BG    = 0x1A1A2E;  // Incoming bubble
constexpr uint32_t TAB_ACTIVE   = 0x00FF41;
constexpr uint32_t TAB_INACTIVE = 0x336633;
constexpr uint32_t BADGE_BG     = 0xFF3333;

// --- Layout Metrics ---
constexpr int SCREEN_W       = 320;
constexpr int SCREEN_H       = 240;
constexpr int STATUS_BAR_H   = 14;
constexpr int TAB_BAR_H      = 14;
constexpr int CONTENT_Y      = STATUS_BAR_H;
constexpr int CONTENT_H      = SCREEN_H - STATUS_BAR_H - TAB_BAR_H;
constexpr int CONTENT_W      = SCREEN_W;

// --- Tab Bar ---
constexpr int TAB_COUNT      = 5;
constexpr int TAB_W          = SCREEN_W / TAB_COUNT;

}  // namespace Theme
