#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Forward-declare custom ratdeck fonts for LV_FONT_DEFAULT
typedef struct _lv_font_t lv_font_t;
extern const lv_font_t lv_font_ratdeck_14;

// Color depth: 16-bit RGB565
#define LV_COLOR_DEPTH 16

// CRITICAL: Byte-swap for LovyanGFX + ST7789V
// Without this, colors appear pixelated/glitchy
#define LV_COLOR_16_SWAP 1

// Memory: route all LVGL allocations to PSRAM via heap_caps
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <esp_heap_caps.h>
#define LV_MEM_CUSTOM_ALLOC(size)       heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#define LV_MEM_CUSTOM_FREE              heap_caps_free
#define LV_MEM_CUSTOM_REALLOC(p, size)  heap_caps_realloc(p, size, MALLOC_CAP_SPIRAM)

// Tick: custom (provided by main loop)
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

// Display
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240
#define LV_DPI_DEF 130

// Logging (enabled temporarily for debug)
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

// Theme — disabled; we use our own LvTheme system which handles all styling.
// The default theme's blue focus outline conflicts with our green design.
#define LV_USE_THEME_DEFAULT 0
#define LV_THEME_DEFAULT_DARK 0

// Fonts - built-in (only 16 still used for titles; 10/12/14 replaced by custom ratdeck fonts)
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 1  // Delivery-status check glyphs in LvMessageView
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_UNSCII_8      0
#define LV_FONT_DEFAULT        &lv_font_ratdeck_14

// Widgets — only enable what we actually use
#define LV_USE_LABEL      1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1  // Required by LVGL calendar/msgbox internals
#define LV_USE_TEXTAREA   1
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_BAR        1
#define LV_USE_SLIDER     0
#define LV_USE_SWITCH     0
#define LV_USE_DROPDOWN   1  // Required by LVGL calendar internals
#define LV_USE_ROLLER     1
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_IMG        1   // required by LV_USE_CANVAS / LV_USE_QRCODE
#define LV_USE_LINE       0
#define LV_USE_ARC        0
#define LV_USE_SPINNER    0
#define LV_USE_MSGBOX     0
#define LV_USE_KEYBOARD   0
#define LV_USE_CHECKBOX   0
#define LV_USE_CANVAS     1   // required by LV_USE_QRCODE
#define LV_USE_QRCODE     1

// Layouts
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

// Animations
#define LV_USE_ANIMIMG 0

// OS
#define LV_USE_OS LV_OS_NONE

#endif // LV_CONF_H
