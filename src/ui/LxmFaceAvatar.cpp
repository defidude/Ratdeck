#include "ui/LxmFaceAvatar.h"

#include <math.h>

namespace {

constexpr int kGrid = 8;

struct LxmColor {
    uint16_t h;
    double s;
    double l;
};

struct LxmFaceData {
    LxmColor color;
    LxmColor bgcolor;
    LxmColor spotcolor;
    uint8_t grid[kGrid][kGrid] = {};
};

class LxmPrng {
public:
    explicit LxmPrng(const String& seed) {
        for (size_t i = 0; i < seed.length(); i++) {
            int idx = i % 4;
            uint32_t v = (uint32_t)_s[idx];
            _s[idx] = (int32_t)((v << 5) - v + (uint8_t)seed.charAt(i));
        }
    }

    double next() {
        int32_t t = _s[0] ^ (int32_t)((uint32_t)_s[0] << 11);
        _s[0] = _s[1];
        _s[1] = _s[2];
        _s[2] = _s[3];
        uint32_t result = (uint32_t)(_s[3] ^ (_s[3] >> 19) ^ t ^ (t >> 8));
        _s[3] = (int32_t)result;
        return (double)result / 2147483648.0;
    }

private:
    int32_t _s[4] = {};
};

LxmColor createColor(LxmPrng& rng) {
    LxmColor c;
    c.h = (uint16_t)floor(rng.next() * 360.0);
    c.s = rng.next() * 60.0 + 40.0;
    c.l = (rng.next() + rng.next() + rng.next() + rng.next()) * 25.0;
    return c;
}

LxmFaceData createFace(const String& seed) {
    LxmPrng rng(seed);
    LxmFaceData face;
    face.color = createColor(rng);
    face.bgcolor = createColor(rng);
    face.spotcolor = createColor(rng);

    for (int y = 0; y < kGrid; y++) {
        for (int x = 0; x < kGrid / 2; x++) {
            face.grid[y][x] = (uint8_t)floor(rng.next() * 2.3);
        }
        for (int x = 0; x < kGrid / 2; x++) {
            face.grid[y][kGrid / 2 + x] = face.grid[y][kGrid / 2 - 1 - x];
        }
    }
    return face;
}

double hueToRgb(double p, double q, double t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}

lv_color_t toLvColor(const LxmColor& color) {
    double h = fmod((double)color.h, 360.0) / 360.0;
    double s = constrain(color.s, 0.0, 100.0) / 100.0;
    double l = constrain(color.l, 0.0, 100.0) / 100.0;

    double r = l;
    double g = l;
    double b = l;
    if (s > 0.0) {
        double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        double p = 2.0 * l - q;
        r = hueToRgb(p, q, h + 1.0 / 3.0);
        g = hueToRgb(p, q, h);
        b = hueToRgb(p, q, h - 1.0 / 3.0);
    }

    uint32_t rgb =
        ((uint32_t)constrain((int)round(r * 255.0), 0, 255) << 16) |
        ((uint32_t)constrain((int)round(g * 255.0), 0, 255) << 8) |
        (uint32_t)constrain((int)round(b * 255.0), 0, 255);
    return lv_color_hex(rgb);
}

bool inCircle(int x, int y, int size) {
    double center = (double)size / 2.0;
    double radius = (double)size / 2.0;
    double dx = ((double)x + 0.5) - center;
    double dy = ((double)y + 0.5) - center;
    return (dx * dx + dy * dy) <= (radius * radius);
}

}  // namespace

namespace LxmFaceAvatar {

size_t bufferSize(lv_coord_t size) {
    if (size <= 0) return 0;
    return lv_img_buf_get_img_size(size, size, LV_IMG_CF_TRUE_COLOR_ALPHA);
}

View create(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
            void* buffer, uint32_t bg, uint32_t border) {
    View view;
    view.box = lv_obj_create(parent);
    lv_obj_set_pos(view.box, x, y);
    lv_obj_set_size(view.box, size, size);
    lv_obj_set_style_bg_color(view.box, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(view.box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(view.box, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(view.box, 1, 0);
    lv_obj_set_style_radius(view.box, size / 2, 0);
    lv_obj_set_style_clip_corner(view.box, false, 0);
    lv_obj_set_style_pad_all(view.box, 0, 0);
    lv_obj_set_style_shadow_width(view.box, 0, 0);
    lv_obj_clear_flag(view.box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    view.canvas = lv_canvas_create(view.box);
    lv_canvas_set_buffer(view.canvas, buffer, size, size, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_set_size(view.canvas, size, size);
    lv_obj_align(view.canvas, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_antialias(view.canvas, false);
    lv_obj_set_style_bg_opa(view.canvas, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view.canvas, 0, 0);
    lv_obj_set_style_pad_all(view.canvas, 0, 0);
    lv_obj_clear_flag(view.canvas, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    return view;
}

bool render(lv_obj_t* canvas, const String& seed) {
    if (!canvas || seed.isEmpty()) return false;

    lv_img_dsc_t* dsc = lv_canvas_get_img(canvas);
    if (!dsc || !dsc->data || dsc->header.w <= 0 || dsc->header.h <= 0) return false;
    int size = dsc->header.w < dsc->header.h ? dsc->header.w : dsc->header.h;
    if (size <= 0) return false;

    LxmFaceData face = createFace(seed);
    lv_color_t colors[3] = {
        toLvColor(face.bgcolor),
        toLvColor(face.color),
        toLvColor(face.spotcolor),
    };

    for (int y = 0; y < size; y++) {
        int gy = y * kGrid / size;
        if (gy >= kGrid) gy = kGrid - 1;
        for (int x = 0; x < size; x++) {
            int gx = x * kGrid / size;
            if (gx >= kGrid) gx = kGrid - 1;
            uint8_t idx = face.grid[gy][gx];
            if (idx > 2) idx = 0;
            lv_img_buf_set_px_color(dsc, x, y, colors[idx]);
            lv_img_buf_set_px_alpha(dsc, x, y, inCircle(x, y, size) ? LV_OPA_COVER : LV_OPA_TRANSP);
        }
    }
    lv_obj_invalidate(canvas);
    return true;
}

}  // namespace LxmFaceAvatar
