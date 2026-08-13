/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "icon_bot_lab.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int kSize = 200;

struct Rgb {
    int r;
    int g;
    int b;
};

float roundedBoxDistance(float x, float y, float cx, float cy, float half_w, float half_h, float radius)
{
    const float dx = std::fabs(x - cx) - half_w + radius;
    const float dy = std::fabs(y - cy) - half_h + radius;
    return std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) + std::max(dy, 0.0f) * std::max(dy, 0.0f)) +
           std::min(std::max(dx, dy), 0.0f) - radius;
}

float coverage(float distance)
{
    return std::clamp(0.5f - distance, 0.0f, 1.0f);
}

Rgb blend(Rgb under, Rgb over, float alpha)
{
    return {
        static_cast<int>(under.r + (over.r - under.r) * alpha),
        static_cast<int>(under.g + (over.g - under.g) * alpha),
        static_cast<int>(under.b + (over.b - under.b) * alpha),
    };
}

uint16_t toRgb565(Rgb color)
{
    return static_cast<uint16_t>(((color.r & 0xf8) << 8) | ((color.g & 0xfc) << 3) | (color.b >> 3));
}

Rgb paintRoundedBox(Rgb color, float x, float y, float cx, float cy, float half_w, float half_h, float radius, Rgb fill,
                    float opacity = 1.0f)
{
    return blend(color, fill, coverage(roundedBoxDistance(x, y, cx, cy, half_w, half_h, radius)) * opacity);
}

std::array<uint16_t, kSize * kSize> makeIcon()
{
    std::array<uint16_t, kSize * kSize> pixels = {};
    for (int py = 0; py < kSize; ++py) {
        for (int px = 0; px < kSize; ++px) {
            const float x = px + 0.5f;
            const float y = py + 0.5f;
            Rgb color = {10, 11, 14};

            // A quiet, rounded tile matches the dark launcher icon family.
            color = paintRoundedBox(color, x, y, 100, 100, 82, 82, 42, {27, 29, 35});
            color = paintRoundedBox(color, x, y, 100, 92, 72, 60, 34, {42, 44, 52}, 0.48f);

            // Shadow and near-black character silhouette: an original compact bot mark.
            color = paintRoundedBox(color, x, y, 101, 116, 58, 46, 35, {7, 8, 10}, 0.68f);
            color = paintRoundedBox(color, x, y, 98, 103, 56, 47, 34, {17, 18, 22});
            color = paintRoundedBox(color, x, y, 98, 94, 48, 25, 27, {33, 35, 42}, 0.56f);
            color = paintRoundedBox(color, x, y, 98, 111, 50, 30, 29, {10, 11, 14}, 0.84f);

            // Two short, slightly offset warm-white eyes preserve legibility at launcher scale.
            const float left_eye_x = x - 82;
            const float right_eye_x = x - 112;
            const float eye_y = y - 108;
            const float left_tilt_y = eye_y + left_eye_x * 0.20f;
            const float right_tilt_y = eye_y + right_eye_x * 0.20f;
            color = paintRoundedBox(color, left_eye_x, left_tilt_y, 0, 0, 7, 17, 7, {244, 241, 235});
            color = paintRoundedBox(color, right_eye_x, right_tilt_y, 0, 0, 7, 17, 7, {244, 241, 235});

            pixels[py * kSize + px] = toRgb565(color);
        }
    }
    return pixels;
}
}  // namespace

const lv_image_dsc_t* getBotLabIcon()
{
    static const std::array<uint16_t, kSize * kSize> pixels = makeIcon();
    static const lv_image_dsc_t image = {
        .header = {.magic = LV_IMAGE_HEADER_MAGIC,
                   .cf = LV_COLOR_FORMAT_RGB565,
                   .flags = 0,
                   .w = kSize,
                   .h = kSize,
                   .stride = kSize * 2,
                   .reserved_2 = 0},
        .data_size = kSize * kSize * sizeof(uint16_t),
        .data = reinterpret_cast<const uint8_t*>(pixels.data()),
        .reserved = nullptr,
        .reserved_2 = nullptr,
    };
    return &image;
}
