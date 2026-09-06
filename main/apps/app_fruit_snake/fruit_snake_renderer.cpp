#include "fruit_snake_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <hal/hal.h>

namespace fruit_snake {
namespace {

constexpr uint32_t kFrameIntervalMs = 33;
constexpr float kPi = 3.14159265358979323846f;

struct Palette {
    uint16_t sky;
    uint16_t field;
    uint16_t fieldShade;
    uint16_t ink;
    uint16_t mint;
    uint16_t mintLight;
    uint16_t mintDark;
    uint16_t blush;
    uint16_t cream;
    uint16_t white;
    uint16_t red;
    uint16_t redLight;
    uint16_t orange;
    uint16_t yellow;
    uint16_t leaf;
    uint16_t leafLight;
    uint16_t purple;
    uint16_t kiwi;
    uint16_t watermelon;
};

Palette makePalette(LGFX_Device& display)
{
    return {
        display.color565(255, 240, 218), display.color565(244, 255, 220),
        display.color565(222, 245, 192), display.color565(54, 78, 62),
        display.color565(101, 207, 142), display.color565(165, 239, 175),
        display.color565(54, 156, 101), display.color565(255, 151, 165),
        display.color565(255, 247, 205), display.color565(255, 255, 246),
        display.color565(244, 76, 91), display.color565(255, 124, 130),
        display.color565(255, 151, 63), display.color565(255, 210, 74),
        display.color565(64, 158, 82), display.color565(117, 198, 92),
        display.color565(139, 91, 202), display.color565(160, 112, 62),
        display.color565(255, 92, 111),
    };
}

void drawHeart(LGFX_Device& canvas, int x, int y, int size, uint16_t color)
{
    const int lobe = std::max(2, size / 3);
    canvas.fillCircle(x - lobe, y - lobe / 2, lobe, color);
    canvas.fillCircle(x + lobe, y - lobe / 2, lobe, color);
    canvas.fillTriangle(x - size, y, x + size, y, x, y + size + 2, color);
}

void drawSparkle(LGFX_Device& canvas, int x, int y, int size, uint16_t color)
{
    canvas.fillTriangle(x, y - size, x - 2, y, x + 2, y, color);
    canvas.fillTriangle(x, y + size, x - 2, y, x + 2, y, color);
    canvas.fillTriangle(x - size, y, x, y - 2, x, y + 2, color);
    canvas.fillTriangle(x + size, y, x, y - 2, x, y + 2, color);
}

void drawLeaf(LGFX_Device& canvas, int x, int y, int direction, int size,
              const Palette& colors)
{
    const int sx = direction >= 0 ? 1 : -1;
    canvas.fillTriangle(x, y, x + sx * size, y - size / 2,
                        x + sx * size, y + size / 2, colors.leaf);
    canvas.drawLine(x + sx * 2, y, x + sx * (size - 2), y, colors.leafLight);
}

void drawTinyFace(LGFX_Device& canvas, int x, int y, int scale, const Palette& colors)
{
    canvas.fillCircle(x - scale, y, std::max(1, scale / 3), colors.ink);
    canvas.fillCircle(x + scale, y, std::max(1, scale / 3), colors.ink);
    canvas.drawLine(x - scale / 2, y + scale, x, y + scale + 1, colors.ink);
    canvas.drawLine(x, y + scale + 1, x + scale / 2, y + scale, colors.ink);
}

void drawApple(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    canvas.fillCircle(x - r / 3, y + 2, r * 3 / 4, colors.red);
    canvas.fillCircle(x + r / 3, y + 2, r * 3 / 4, colors.red);
    canvas.fillRect(x - r * 2 / 3, y, r * 4 / 3, r, colors.red);
    canvas.drawLine(x, y - r + 2, x + 2, y - r - 8, colors.ink);
    drawLeaf(canvas, x + 1, y - r + 1, 1, r / 2, colors);
    canvas.fillCircle(x - r / 3, y - r / 3, std::max(2, r / 6), colors.redLight);
    drawTinyFace(canvas, x, y + 2, std::max(4, r / 4), colors);
}

void drawStrawberry(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    canvas.fillCircle(x, y - r / 4, r * 3 / 4, colors.redLight);
    canvas.fillTriangle(x - r * 3 / 4, y - r / 5, x + r * 3 / 4, y - r / 5,
                        x, y + r, colors.redLight);
    for (int sx : {-1, 0, 1}) {
        canvas.fillTriangle(x, y - r + 2, x + sx * r / 2 - r / 3, y - r / 2,
                            x + sx * r / 2 + r / 3, y - r / 2, colors.leaf);
    }
    const uint16_t seed = colors.cream;
    canvas.fillCircle(x - r / 3, y, 1, seed);
    canvas.fillCircle(x + r / 3, y + r / 4, 1, seed);
    canvas.fillCircle(x, y + r / 2, 1, seed);
    drawTinyFace(canvas, x, y - r / 7, std::max(4, r / 4), colors);
}

void drawOrange(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    canvas.fillCircle(x, y, r, colors.orange);
    canvas.fillCircle(x - r / 3, y - r / 3, std::max(2, r / 6), colors.yellow);
    drawLeaf(canvas, x, y - r + 2, 1, r / 2, colors);
    drawTinyFace(canvas, x, y + 1, std::max(4, r / 4), colors);
}

void drawWatermelon(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    canvas.fillCircle(x, y, r, colors.leaf);
    canvas.fillCircle(x, y, r - 4, colors.cream);
    canvas.fillCircle(x, y, r - 7, colors.watermelon);
    canvas.fillCircle(x - r / 3, y - r / 3, 2, colors.ink);
    canvas.fillCircle(x + r / 3, y - r / 4, 2, colors.ink);
    drawTinyFace(canvas, x, y + r / 4, std::max(4, r / 4), colors);
}

void drawBanana(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    for (int i = -4; i <= 4; ++i) {
        const int px = x + i * r / 5;
        const int py = y + (i * i * r) / 80;
        canvas.fillCircle(px, py, r / 2 + 2, colors.kiwi);
    }
    for (int i = -4; i <= 4; ++i) {
        const int px = x + i * r / 5;
        const int py = y + (i * i * r) / 80 - 1;
        canvas.fillCircle(px, py, r / 2, colors.yellow);
    }
    drawTinyFace(canvas, x, y - 1, std::max(4, r / 4), colors);
}

void drawGrapes(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    const std::array<std::array<int, 2>, 7> offsets = {{{-7, -6}, {7, -6}, {0, 3},
                                                         {-10, 7}, {10, 7}, {-5, 15}, {5, 15}}};
    const int scale = std::max(1, r / 20);
    for (const auto& offset : offsets) {
        canvas.fillCircle(x + offset[0] * scale, y + offset[1] * scale,
                          std::max(5, r / 3), colors.purple);
    }
    drawLeaf(canvas, x, y - r + 3, 1, r / 2, colors);
    drawTinyFace(canvas, x, y + r / 5, std::max(4, r / 4), colors);
}

void drawCherries(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    canvas.drawLine(x, y - r, x - r / 2, y + 2, colors.leaf);
    canvas.drawLine(x, y - r, x + r / 2, y + 3, colors.leaf);
    canvas.fillCircle(x - r / 2, y + r / 3, r * 3 / 5, colors.red);
    canvas.fillCircle(x + r / 2, y + r / 3, r * 3 / 5, colors.redLight);
    drawLeaf(canvas, x, y - r, 1, r / 2, colors);
    canvas.fillCircle(x - r * 2 / 3, y + r / 6, 2, colors.white);
    canvas.fillCircle(x + r / 3, y + r / 6, 2, colors.white);
}

void drawKiwi(LGFX_Device& canvas, int x, int y, int r, const Palette& colors)
{
    canvas.fillCircle(x, y, r, colors.kiwi);
    canvas.fillCircle(x, y, r - 4, colors.leafLight);
    canvas.fillCircle(x, y, r / 3, colors.cream);
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * kPi / 4.0f;
        canvas.fillCircle(x + static_cast<int>(std::cos(angle) * r * 0.55f),
                          y + static_cast<int>(std::sin(angle) * r * 0.55f), 1, colors.ink);
    }
    drawTinyFace(canvas, x, y, std::max(4, r / 4), colors);
}

void drawFruit(LGFX_Device& canvas, const Fruit& fruit, uint32_t nowMs, const Palette& colors)
{
    const float phase = static_cast<float>((nowMs + fruit.bornMs) % 1800u) / 1800.0f *
                        kPi * 2.0f;
    const int x = static_cast<int>(std::lround(fruit.position.x));
    const int y = static_cast<int>(std::lround(fruit.position.y + std::sin(phase) * 3.0f));
    constexpr int r = 19;
    canvas.fillCircle(x + 3, y + 5, r, colors.fieldShade);
    switch (fruit.kind) {
        case FruitKind::Apple: drawApple(canvas, x, y, r, colors); break;
        case FruitKind::Strawberry: drawStrawberry(canvas, x, y, r, colors); break;
        case FruitKind::Orange: drawOrange(canvas, x, y, r, colors); break;
        case FruitKind::Watermelon: drawWatermelon(canvas, x, y, r, colors); break;
        case FruitKind::Banana: drawBanana(canvas, x, y, r, colors); break;
        case FruitKind::Grapes: drawGrapes(canvas, x, y, r, colors); break;
        case FruitKind::Cherries: drawCherries(canvas, x, y, r, colors); break;
        case FruitKind::Kiwi: drawKiwi(canvas, x, y, r, colors); break;
    }
}

void drawGarden(LGFX_Device& canvas, int width, int height, float playRadius,
                const Palette& colors)
{
    canvas.fillScreen(colors.sky);
    const int cx = width / 2;
    const int cy = height / 2;
    canvas.fillCircle(cx, cy + 3, static_cast<int>(playRadius + 7), colors.fieldShade);
    canvas.fillCircle(cx, cy, static_cast<int>(playRadius + 3), colors.field);

    const std::array<std::array<int, 2>, 12> dots = {{{68, 127}, {112, 66}, {174, 93},
                                                       {290, 72}, {365, 112}, {399, 205},
                                                       {374, 324}, {304, 390}, {205, 403},
                                                       {112, 365}, {67, 285}, {211, 62}}};
    for (std::size_t i = 0; i < dots.size(); ++i) {
        const int x = dots[i][0] * width / 466;
        const int y = dots[i][1] * height / 466;
        canvas.fillCircle(x, y, 3 + static_cast<int>(i % 2), colors.fieldShade);
    }

    const std::array<std::array<int, 2>, 5> flowers = {{{80, 185}, {135, 397}, {337, 383},
                                                         {390, 265}, {257, 74}}};
    for (const auto& flower : flowers) {
        const int x = flower[0] * width / 466;
        const int y = flower[1] * height / 466;
        canvas.fillCircle(x - 5, y, 5, colors.white);
        canvas.fillCircle(x + 5, y, 5, colors.white);
        canvas.fillCircle(x, y - 5, 5, colors.white);
        canvas.fillCircle(x, y + 5, 5, colors.white);
        canvas.fillCircle(x, y, 4, colors.yellow);
    }
}

void drawSnake(LGFX_Device& canvas, const Engine& engine, uint32_t nowMs,
               const Palette& colors)
{
    const auto& segments = engine.segments();
    for (std::size_t reverse = engine.segmentCount(); reverse > 1; --reverse) {
        const std::size_t i = reverse - 1;
        const Point from = segments[i];
        const Point to = segments[i - 1];
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        const int steps = std::max(1, static_cast<int>(distance / 6.0f));
        const uint16_t body = (i % 2 == 0) ? colors.mint : colors.mintLight;
        for (int step = 0; step <= steps; ++step) {
            const float amount = static_cast<float>(step) / steps;
            canvas.fillCircle(static_cast<int>(from.x + dx * amount),
                              static_cast<int>(from.y + dy * amount), 15, colors.mintDark);
        }
        for (int step = 0; step <= steps; ++step) {
            const float amount = static_cast<float>(step) / steps;
            canvas.fillCircle(static_cast<int>(from.x + dx * amount),
                              static_cast<int>(from.y + dy * amount), 13, body);
        }
        canvas.fillCircle(static_cast<int>(from.x - 3), static_cast<int>(from.y - 4),
                          3, colors.mintDark);
    }

    const Point head = segments[0];
    const float heading = engine.heading();
    const float hx = std::cos(heading);
    const float hy = std::sin(heading);
    const float px = -hy;
    const float py = hx;
    const int headX = static_cast<int>(std::lround(head.x));
    const int headY = static_cast<int>(std::lround(head.y));
    canvas.fillCircle(headX + 3, headY + 4, 25, colors.fieldShade);
    canvas.fillCircle(headX, headY, 25, colors.mintDark);
    canvas.fillCircle(headX, headY, 22, colors.mintLight);
    canvas.fillCircle(headX - static_cast<int>(px * 8) - 5,
                      headY - static_cast<int>(py * 8) - 6, 5, colors.white);

    for (int side : {-1, 1}) {
        const int eyeX = headX + static_cast<int>(hx * 8.0f + px * side * 8.0f);
        const int eyeY = headY + static_cast<int>(hy * 8.0f + py * side * 8.0f);
        canvas.fillCircle(eyeX, eyeY, 5, colors.ink);
        canvas.fillCircle(eyeX - 1, eyeY - 2, 2, colors.white);
        const int cheekX = headX + static_cast<int>(hx * 11.0f + px * side * 15.0f);
        const int cheekY = headY + static_cast<int>(hy * 11.0f + py * side * 15.0f);
        canvas.fillCircle(cheekX, cheekY, 4, colors.blush);
    }
    const int mouthX = headX + static_cast<int>(hx * 17.0f);
    const int mouthY = headY + static_cast<int>(hy * 17.0f);
    canvas.fillCircle(mouthX, mouthY, 3, colors.ink);
    if (engine.feedback().kind == FeedbackKind::FruitEaten &&
        nowMs - engine.feedback().startedMs < 380u) {
        canvas.fillCircle(mouthX + static_cast<int>(hx * 4.0f),
                          mouthY + static_cast<int>(hy * 4.0f), 2, colors.blush);
    }
}

void drawFeedback(LGFX_Device& canvas, const Feedback& feedback, uint32_t nowMs,
                  const Palette& colors)
{
    if (feedback.kind == FeedbackKind::None || nowMs < feedback.startedMs) return;
    const uint32_t elapsed = nowMs - feedback.startedMs;
    if (elapsed > 950u) return;
    const int x = static_cast<int>(feedback.position.x);
    const int y = static_cast<int>(feedback.position.y);
    const int rise = static_cast<int>(elapsed / 28u);
    const int pulse = 8 + static_cast<int>(elapsed / 35u);
    switch (feedback.kind) {
        case FeedbackKind::FruitAdded:
            canvas.drawCircle(x, y, pulse, colors.yellow);
            drawSparkle(canvas, x - pulse, y - pulse / 2, 6, colors.orange);
            break;
        case FeedbackKind::FruitEaten:
            drawHeart(canvas, x - 15, y - rise, 7, colors.blush);
            drawHeart(canvas, x + 12, y - rise / 2, 5, colors.redLight);
            drawSparkle(canvas, x, y - 20 - rise / 2, 7, colors.yellow);
            break;
        case FeedbackKind::SnakeShortened:
            drawSparkle(canvas, x - 8, y - rise, 7, colors.yellow);
            drawHeart(canvas, x + 8, y - rise / 2, 5, colors.blush);
            break;
        case FeedbackKind::Tickled:
            drawHeart(canvas, x - 25, y - 22 - rise / 2, 6, colors.blush);
            drawHeart(canvas, x + 20, y - 28 - rise / 3, 5, colors.redLight);
            break;
        case FeedbackKind::EdgeBounce:
            drawSparkle(canvas, x - 14, y - 10, 7, colors.yellow);
            drawSparkle(canvas, x + 15, y + 5, 5, colors.orange);
            break;
        case FeedbackKind::None: break;
    }
}

void drawWelcomeHint(LGFX_Device& canvas, const Engine& engine, uint32_t elapsed,
                     const Palette& colors)
{
    if (elapsed > 5200u) return;
    const int cx = engine.width() / 2;
    const int bottom = engine.height() - 47;
    const int bob = static_cast<int>(std::sin(elapsed * 0.007f) * 3.0f);
    canvas.fillCircle(cx, bottom + bob, 18, colors.white);
    canvas.fillCircle(cx, bottom + bob, 14, colors.cream);
    canvas.drawLine(cx, bottom - 7 + bob, cx, bottom + 8 + bob, colors.ink);
    canvas.fillTriangle(cx, bottom + 11 + bob, cx - 6, bottom + 3 + bob,
                        cx + 6, bottom + 3 + bob, colors.ink);
    drawSparkle(canvas, cx - 28, bottom - 5 + bob, 5, colors.orange);
    drawSparkle(canvas, cx + 28, bottom - 11 + bob, 4, colors.blush);
}

}  // namespace

void Renderer::open(int width, int height, uint32_t nowMs)
{
    _width = width;
    _height = height;
    _openedMs = nowMs;
    _lastFrameMs = 0;
}

void Renderer::render(const Engine& engine, uint32_t nowMs)
{
    if (_width <= 0 || _height <= 0) return;
    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;

    auto& display = GetHAL().getDisplay();
    const Palette colors = makePalette(display);
    display.startWrite();
    drawGarden(display, _width, _height, engine.playRadius(), colors);
    for (const Fruit& fruit : engine.fruits()) {
        if (fruit.active) drawFruit(display, fruit, nowMs, colors);
    }
    drawSnake(display, engine, nowMs, colors);
    drawFeedback(display, engine.feedback(), nowMs, colors);
    drawWelcomeHint(display, engine, nowMs - _openedMs, colors);
    display.endWrite();
}

void Renderer::close()
{
    auto& display = GetHAL().getDisplay();
    display.startWrite();
    display.fillScreen(TFT_BLACK);
    display.endWrite();
    _width = 0;
    _height = 0;
    _lastFrameMs = 0;
}

}  // namespace fruit_snake
