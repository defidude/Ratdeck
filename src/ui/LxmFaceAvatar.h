#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <cstddef>
#include <cstdint>

namespace LxmFaceAvatar {

struct View {
    lv_obj_t* box = nullptr;
    lv_obj_t* canvas = nullptr;
};

size_t bufferSize(lv_coord_t size);
View create(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
            void* buffer, uint32_t bg, uint32_t border);
bool render(lv_obj_t* canvas, const String& seed);

}  // namespace LxmFaceAvatar
