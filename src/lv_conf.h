#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Color depth: 16-bit RGB565
#define LV_COLOR_DEPTH 16

// Memory: use stdlib malloc (PSRAM-aware on ESP32-S3)
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc

// Tick: custom (provided by main loop)
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

// Display
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 240
#define LV_DPI_DEF 130

// Logging
#define LV_USE_LOG 0

// Fonts - built-in
#define LV_FONT_MONTSERRAT_8  1
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_UNSCII_8      1
#define LV_FONT_DEFAULT        &lv_font_montserrat_12

// Widgets
#define LV_USE_LABEL      1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_TEXTAREA   1
#define LV_USE_LIST       1
#define LV_USE_BAR        1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_DROPDOWN   1
#define LV_USE_ROLLER     1
#define LV_USE_TABLE      1
#define LV_USE_TABVIEW    1
#define LV_USE_IMG        1
#define LV_USE_LINE       1
#define LV_USE_ARC        1
#define LV_USE_SPINNER    1
#define LV_USE_MSGBOX     1
#define LV_USE_KEYBOARD   1

// Scroll
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

// OS
#define LV_USE_OS LV_OS_NONE

#endif // LV_CONF_H
