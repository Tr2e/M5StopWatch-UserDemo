/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "icon_bot_lab.h"

#include <array>
#include <utility>

namespace {

constexpr int kIconSize = 200;
constexpr int kPixelCount = kIconSize * kIconSize;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint16_t>(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
}

constexpr int square(int value)
{
    return value * value;
}

constexpr bool inCircle(int x, int y, int cx, int cy, int radius)
{
    return square(x - cx) + square(y - cy) < square(radius);
}

constexpr bool inCapsule(int x, int y, int ax, int ay, int bx, int by, int radius)
{
    const int dx = bx - ax;
    const int dy = by - ay;
    const int px = x - ax;
    const int py = y - ay;
    const int projection = px * dx + py * dy;
    const int lengthSquared = dx * dx + dy * dy;
    if (projection <= 0) return px * px + py * py < radius * radius;
    if (projection >= lengthSquared) return square(x - bx) + square(y - by) < radius * radius;
    const int cross = px * dy - py * dx;
    return cross * cross < radius * radius * lengthSquared;
}

constexpr bool inLeftEye(int x, int y)
{
    return inCapsule(x, y, 77, 92, 77, 112, 8);
}

constexpr bool inRightEye(int x, int y)
{
    return inCapsule(x, y, 127, 92, 127, 112, 8);
}

constexpr uint16_t iconPixel(int x, int y)
{
    constexpr uint16_t kCanvas = rgb565(8, 8, 8);
    constexpr uint16_t kEdge = rgb565(222, 222, 218);
    constexpr uint16_t kBall = rgb565(248, 248, 245);
    constexpr uint16_t kEye = rgb565(20, 20, 20);
    if (inCircle(x, y, 100, 100, 70)) {
        return (inLeftEye(x, y) || inRightEye(x, y)) ? kEye : kBall;
    }
    return inCircle(x, y, 100, 100, 72) ? kEdge : kCanvas;
}

template <size_t... Index>
constexpr std::array<uint16_t, kPixelCount> makeIcon(std::index_sequence<Index...>)
{
    return {iconPixel(static_cast<int>(Index % kIconSize), static_cast<int>(Index / kIconSize))...};
}

// Keep the 200×200 RGB565 asset in flash. This is a pure white sphere with
// black eyes on the launcher canvas—no rounded card, shadow, or backing tile.
constexpr auto kBotLabIconMap = makeIcon(std::make_index_sequence<kPixelCount>{});

}  // namespace

const lv_image_dsc_t bot_lab_icon = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC,
               .cf = LV_COLOR_FORMAT_RGB565,
               .flags = 0,
               .w = kIconSize,
               .h = kIconSize,
               .stride = kIconSize * static_cast<int>(sizeof(uint16_t)),
               .reserved_2 = 0},
    .data_size = kPixelCount * sizeof(uint16_t),
    .data = reinterpret_cast<const uint8_t*>(kBotLabIconMap.data()),
    .reserved = nullptr,
    .reserved_2 = nullptr,
};
