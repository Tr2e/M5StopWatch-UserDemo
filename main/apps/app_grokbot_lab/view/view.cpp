/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include <assets/assets.h>
#include <algorithm>
#include <cmath>

using namespace view;
using namespace uitk;
using namespace uitk::lvgl_cpp;

namespace {
constexpr int kPanelSize = 466;
constexpr int kStageSize = 466;
constexpr int kCenter = kStageSize / 2;
constexpr int kFaceSize = kStageSize;
constexpr int kEffectFaceSize = 270;
constexpr float kPi = 3.14159265358979323846f;

void drawRounded(lv_layer_t* layer, int x, int y, int width, int height, int radius, uint32_t color, lv_opa_t opa = LV_OPA_COVER)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = opa;
    dsc.radius = radius;
    dsc.border_width = 0;
    const lv_area_t area = {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y),
                            static_cast<lv_coord_t>(x + width - 1), static_cast<lv_coord_t>(y + height - 1)};
    lv_draw_rect(layer, &dsc, &area);
}

void drawRing(lv_layer_t* layer, int cx, int cy, int radius, uint32_t color, int width, lv_opa_t opa)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.width = width;
    dsc.opa = opa;
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = radius;
    dsc.start_angle = 0;
    dsc.end_angle = 359;
    dsc.rounded = 1;
    lv_draw_arc(layer, &dsc);
}

void drawTriangle(lv_layer_t* layer, lv_point_t a, lv_point_t b, lv_point_t c, uint32_t color, lv_opa_t opa = LV_OPA_COVER)
{
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.p[0] = {a.x, a.y};
    dsc.p[1] = {b.x, b.y};
    dsc.p[2] = {c.x, c.y};
    dsc.color = lv_color_hex(color);
    dsc.opa = opa;
    lv_draw_triangle(layer, &dsc);
}

void drawRibbonSegment(lv_layer_t* layer, lv_point_t from, lv_point_t to, uint32_t color, int width, lv_opa_t opa)
{
    const float dx = static_cast<float>(to.x - from.x);
    const float dy = static_cast<float>(to.y - from.y);
    const float length = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    const float half_width = static_cast<float>(width) * 0.5f;
    const float nx = -dy / length * half_width;
    const float ny = dx / length * half_width;
    const lv_point_t a = {static_cast<lv_coord_t>(std::lround(from.x + nx)), static_cast<lv_coord_t>(std::lround(from.y + ny))};
    const lv_point_t b = {static_cast<lv_coord_t>(std::lround(from.x - nx)), static_cast<lv_coord_t>(std::lround(from.y - ny))};
    const lv_point_t c = {static_cast<lv_coord_t>(std::lround(to.x + nx)), static_cast<lv_coord_t>(std::lround(to.y + ny))};
    const lv_point_t d = {static_cast<lv_coord_t>(std::lround(to.x - nx)), static_cast<lv_coord_t>(std::lround(to.y - ny))};
    drawTriangle(layer, a, b, c, color, opa);
    drawTriangle(layer, b, d, c, color, opa);
}

void drawEffectContour(lv_layer_t* layer, int cx, int cy, int radius, uint32_t tint)
{
    // Compact black faces need just enough separation from the AMOLED black
    // canvas to read as a character, not as two floating eyes.
    drawRing(layer, cx, cy, radius + 4, tint, 5, 22);
    drawRing(layer, cx, cy, radius + 1, tint, 2, 116);
}

void drawCelebrateBurst(lv_layer_t* layer, int cx, int cy, float seconds, lv_opa_t opacity = LV_OPA_COVER)
{
    // A single, wide fountain from behind the crown. Every shred has a small
    // launch delay and follows the same ballistic curve, which reads much more
    // like physical confetti than a synchronized radial expansion.
    static constexpr uint32_t colors[] = {0xA9D6FF, 0xFFCC66, 0xFF8292, 0xA7E577, 0xC599FF, 0x58D4CC};
    for (int i = 0; i < 28; ++i) {
        const float delay = static_cast<float>((i * 37) % 230) / 1000.0f;
        const float age = seconds - delay;
        if (age <= 0.0f) continue;
        const float life = 1.92f;
        const float fade = 1.0f - std::clamp((age - 1.42f) / (life - 1.42f), 0.0f, 1.0f);
        const float angle = -2.82f + static_cast<float>(i) * (2.46f / 27.0f);
        const float speed = 148.0f + static_cast<float>((i * 29) % 74);
        const float startX = static_cast<float>(cx) + static_cast<float>((i % 5) - 2) * 4.0f;
        const float startY = static_cast<float>(cy) + static_cast<float>((i % 3) - 1) * 3.0f;
        const float endX = startX + std::cos(angle) * speed * age;
        const float endY = startY + std::sin(angle) * speed * age + 118.0f * age * age;
        const uint32_t color = colors[i % (sizeof(colors) / sizeof(colors[0]))];
        const lv_opa_t opa = static_cast<lv_opa_t>(205.0f * fade * opacity / static_cast<float>(LV_OPA_COVER));
        if (i % 4 == 0) {
            const float previous = std::max(0.0f, age - 0.11f);
            const lv_point_t from = {static_cast<lv_coord_t>(std::lround(startX + std::cos(angle) * speed * previous)),
                                     static_cast<lv_coord_t>(std::lround(startY + std::sin(angle) * speed * previous + 118.0f * previous * previous))};
            const lv_point_t to = {static_cast<lv_coord_t>(std::lround(endX)), static_cast<lv_coord_t>(std::lround(endY))};
            drawRibbonSegment(layer, from, to, color, 5 + (i % 2) * 2, opa);
        } else {
            const int size = 5 + (i % 3) * 2;
            drawRounded(layer, static_cast<int>(std::lround(endX)) - size / 2, static_cast<int>(std::lround(endY)) - size / 2,
                        size, size, 2, color, opa);
        }
    }
}

void drawCelebrateFrontFall(lv_layer_t* layer, int cx, int cy, float seconds, lv_opa_t opacity = LV_OPA_COVER)
{
    // Only five late, slow shreds cross the face; fixed irregular lanes and
    // launch times keep this as a chance drift rather than a numbered cascade.
    static constexpr uint32_t colors[] = {0xFFCC66, 0xA9D6FF, 0xFF8292, 0xA7E577, 0xC599FF};
    static constexpr float lanes[] = {-94.0f, 42.0f, -16.0f, 96.0f, -57.0f};
    static constexpr float delays[] = {0.31f, 0.03f, 0.43f, 0.16f, 0.27f};
    static constexpr float angles[] = {-0.88f, 0.42f, -0.18f, 1.07f, 0.73f};
    for (int i = 0; i < 5; ++i) {
        const float age = seconds - 0.48f - delays[i];
        if (age <= 0.0f) continue;
        const float startX = static_cast<float>(cx) + lanes[i];
        const float x = startX + std::sin(age * (3.1f + i * 0.31f) + i * 1.73f) * (8.0f + i * 1.7f);
        const float y = static_cast<float>(cy) - 102.0f - age * (17.0f + (i % 3) * 8.0f) + (104.0f + i * 9.0f) * age * age;
        const float angle = angles[i] + std::sin(age * (4.2f + i * 0.37f) + i) * 0.35f;
        if (i % 3 == 0) {
            const float half = 13.0f + i;
            const lv_point_t from = {static_cast<lv_coord_t>(std::lround(x - std::cos(angle) * half)),
                                     static_cast<lv_coord_t>(std::lround(y - std::sin(angle) * half))};
            const lv_point_t to = {static_cast<lv_coord_t>(std::lround(x + std::cos(angle) * half)),
                                   static_cast<lv_coord_t>(std::lround(y + std::sin(angle) * half))};
            drawRibbonSegment(layer, from, to, colors[i], 5 + i % 2, opacity);
        } else if (i % 3 == 1) {
            const int size = 9 + i;
            drawRounded(layer, static_cast<int>(std::lround(x)) - size / 2, static_cast<int>(std::lround(y)) - size / 2,
                        size, size, 2, colors[i], opacity);
        } else {
            const int width = 13;
            const int height = 7;
            drawRounded(layer, static_cast<int>(std::lround(x)) - width / 2, static_cast<int>(std::lround(y)) - height / 2,
                        width, height, 3, colors[i], opacity);
        }
    }
}

void drawRoundedStar(lv_layer_t* layer, int cx, int cy, int radius, uint32_t color, lv_opa_t opacity = LV_OPA_COVER)
{
    constexpr int kPoints = 5;
    // Keep the tips softened without turning the star into five visible blobs.
    constexpr int kTipCorner = 10;
    const float inner_radius = radius * 0.48f;
    lv_point_t inner[kPoints] = {};
    lv_point_t tip[kPoints] = {};
    for (int i = 0; i < kPoints; ++i) {
        const float outer_angle = -kPi / 2.0f + i * 2.0f * kPi / kPoints;
        const float inner_angle = outer_angle + kPi / kPoints;
        inner[i] = {static_cast<lv_coord_t>(cx + std::cos(inner_angle) * inner_radius),
                    static_cast<lv_coord_t>(cy + std::sin(inner_angle) * inner_radius)};
        tip[i] = {static_cast<lv_coord_t>(cx + std::cos(outer_angle) * (radius - kTipCorner)),
                  static_cast<lv_coord_t>(cy + std::sin(outer_angle) * (radius - kTipCorner))};
    }
    const lv_point_t center = {static_cast<lv_coord_t>(cx), static_cast<lv_coord_t>(cy)};
    for (int i = 0; i < kPoints; ++i) {
        drawTriangle(layer, center, inner[i], inner[(i + 1) % kPoints], color, opacity);
        drawTriangle(layer, tip[i], inner[i], inner[(i + kPoints - 1) % kPoints], color, opacity);
        drawRounded(layer, tip[i].x - kTipCorner, tip[i].y - kTipCorner,
                    kTipCorner * 2, kTipCorner * 2, kTipCorner, color, opacity);
    }
    // Triangles are rendered independently by LVGL; their anti-aliased shared
    // edges can show as hairline seams. A solid core intentionally overlaps all
    // joins, leaving one clean, continuous star surface.
    const int core = static_cast<int>(inner_radius) + 5;
    drawRounded(layer, cx - core, cy - core, core * 2, core * 2, core, color, opacity);
}

void drawExclaimSymbol(lv_layer_t* layer, int cx, int cy, float scale, lv_opa_t opacity)
{
    const int stemW = std::max(4, static_cast<int>(38 * scale));
    const int stemH = std::max(8, static_cast<int>(146 * scale));
    const int dotSize = std::max(5, static_cast<int>(44 * scale));
    drawRounded(layer, cx - stemW / 2, cy - static_cast<int>(104 * scale), stemW, stemH, stemW / 2, 0xF4F1EB, opacity);
    drawRounded(layer, cx - dotSize / 2, cy + static_cast<int>(82 * scale), dotSize, dotSize, dotSize / 2, 0xF4F1EB, opacity);
}

struct RibbonSpec {
    uint32_t color;
    float radius_x;
    float radius_y;
    float roll;
    float speed;
    float phase;
    float trail_seconds;
    float offset_x;
    float offset_y;
    int width;
};

static constexpr RibbonSpec kOrbitRibbons[] = {
        {0x6377DC, 184, 76, -0.78f, 1.04f, 0.10f, 1.42f, -4, 8, 8},
        {0xD362C5, 166, 106, -0.41f, 0.91f, 1.68f, 1.18f, 12, -9, 7},
        {0x72D2C5, 151, 68, 0.18f, 1.28f, 2.64f, 1.04f, -15, 7, 7},
        {0x80B9E7, 159, 91, -0.15f, 1.12f, 4.20f, 1.30f, 8, 13, 7},
        {0xF0A778, 137, 63, 0.52f, 1.42f, 5.04f, 0.88f, 4, -4, 6},
};

void drawRibbons(lv_layer_t* layer, int cx, int cy, float seconds, bool front, lv_opa_t opacity = LV_OPA_COVER)
{
    constexpr int count = sizeof(kOrbitRibbons) / sizeof(kOrbitRibbons[0]);
    constexpr int kSegments = 34;
    for (int ribbon = 0; ribbon < count; ++ribbon) {
        const RibbonSpec& spec = kOrbitRibbons[ribbon];
        const auto pointAt = [&](float trail_age, float& depth) {
            const float angle = (seconds - trail_age) * spec.speed + spec.phase;
            const float plane_x = std::cos(angle) * spec.radius_x;
            const float plane_y = std::sin(angle) * spec.radius_y;
            depth = std::sin(angle);
            return lv_point_t{static_cast<lv_coord_t>(cx + spec.offset_x + plane_x * std::cos(spec.roll) - plane_y * std::sin(spec.roll)),
                              static_cast<lv_coord_t>(cy + spec.offset_y + plane_x * std::sin(spec.roll) + plane_y * std::cos(spec.roll))};
        };
        for (int segment = 0; segment < kSegments; ++segment) {
            const float age0 = spec.trail_seconds * static_cast<float>(segment) / kSegments;
            const float age1 = spec.trail_seconds * static_cast<float>(segment + 1) / kSegments;
            float depth0 = 0.0f;
            float depth1 = 0.0f;
            const lv_point_t p0 = pointAt(age0, depth0);
            const lv_point_t p1 = pointAt(age1, depth1);
            if (((depth0 + depth1) * 0.5f >= 0.0f) != front) continue;
            const lv_opa_t segment_opa = front ? opacity : static_cast<lv_opa_t>(opacity * static_cast<int>(LV_OPA_70) / static_cast<int>(LV_OPA_COVER));
            drawRibbonSegment(layer, p0, p1, spec.color, spec.width, segment_opa);
        }
    }
}

void drawOrbitSatellites(lv_layer_t* layer, int cx, int cy, float seconds, bool front, lv_opa_t opacity = LV_OPA_COVER)
{
    constexpr uint32_t color = 0xF4F1EB;
    constexpr float speed = 1.18f;
    for (int i = 0; i < 2; ++i) {
        const float angle = seconds * speed + i * kPi;
        const float depth = std::sin(angle);
        if ((depth >= 0.0f) != front) continue;
        const int x = cx + static_cast<int>(std::cos(angle) * 178.0f);
        const int y = cy + static_cast<int>(std::sin(angle) * 84.0f);
        constexpr int size = 14;
        const lv_opa_t satellite_opa = front ? opacity : static_cast<lv_opa_t>(opacity * static_cast<int>(LV_OPA_60) / static_cast<int>(LV_OPA_COVER));
        drawRounded(layer, x - size / 2, y - size / 2, size, size, size / 2, color, satellite_opa);
    }
}

uint32_t danceColor(uint32_t beat, uint32_t salt)
{
    static constexpr uint32_t palette[] = {0x6377DC, 0xD362C5, 0x72D2C5, 0xF0A778, 0xC599FF, 0x80B9E7};
    uint32_t value = beat * 1664525u + 1013904223u + salt * 97u;
    value ^= value >> 16;
    return palette[value % (sizeof(palette) / sizeof(palette[0]))];
}

float danceUnit(uint32_t beat, uint32_t salt)
{
    uint32_t value = beat * 22695477u + 1u + salt * 747796405u;
    value ^= value >> 13;
    return static_cast<float>(value & 0xffu) / 255.0f;
}

void drawDanceLights(lv_layer_t* layer, int cx, int cy, float seconds, lv_opa_t opacity = LV_OPA_COVER)
{
    // Keep the former random stage composition, but render exactly one large
    // alpha triangle per light. Geometry is interpolated; colors rotate on the
    // beat without duplicating each full-screen fill for a cross-fade.
    const uint32_t beat = static_cast<uint32_t>(seconds / 0.42f);
    const float rawBlend = std::fmod(seconds, 0.42f) / 0.42f;
    const float blend = rawBlend * rawBlend * (3.0f - 2.0f * rawBlend);
    const lv_opa_t beamOpa = static_cast<lv_opa_t>(150 * opacity / static_cast<int>(LV_OPA_COVER));
    for (int i = 0; i < 3; ++i) {
        const auto interpolateUnit = [beat, blend](uint32_t salt) {
            return danceUnit(beat, salt) + (danceUnit(beat + 1u, salt) - danceUnit(beat, salt)) * blend;
        };
        const float sourceOffset = -158.0f + i * 158.0f + (interpolateUnit(i + 11) - 0.5f) * 42.0f;
        const float targetOffset = (interpolateUnit(i + 23) - 0.5f) * 218.0f;
        const float sway = std::sin(seconds * (2.1f + i * 0.29f) + i * 1.6f) * 25.0f;
        const int sourceX = cx + static_cast<int>(sourceOffset);
        const int targetX = cx + static_cast<int>(targetOffset + sway);
        const int targetY = cy + 144 + static_cast<int>(interpolateUnit(i + 37) * 28.0f);
        const int aperture = 22 + static_cast<int>(interpolateUnit(i + 51) * 15.0f);
        const lv_point_t lamp = {static_cast<lv_coord_t>(sourceX), 0};
        const lv_point_t left = {static_cast<lv_coord_t>(targetX - aperture), static_cast<lv_coord_t>(targetY)};
        const lv_point_t right = {static_cast<lv_coord_t>(targetX + aperture), static_cast<lv_coord_t>(targetY)};
        drawTriangle(layer, lamp, left, right, danceColor(beat, i + 3), beamOpa);
    }
}

void drawDanceGlowSticks(lv_layer_t* layer, int cx, int cy, float seconds, float poseTurn, float depthTurn,
                         bool foreground, lv_opa_t opacity = LV_OPA_COVER)
{
    // Restore the intentional front/back dance staging. Z-order is stable for
    // the expression, so no stick can rapidly pop through the face on a gaze
    // crossing; the two-layer stick preserves its neon look.
    static constexpr uint32_t colors[] = {0x72D2C5, 0xD362C5};
    const bool nearLeft = depthTurn > 0.0f;
    for (int index = 0; index < 2; ++index) {
        const int side = index == 0 ? -1 : 1;
        const bool near = (side < 0) == nearLeft;
        if (near != foreground) continue;
        const float swing = std::sin(seconds * (5.35f + index * 0.41f) + index * 2.1f) * 0.42f;
        const float angle = (side < 0 ? -2.08f : -1.06f) + swing;
        const float length = near ? 112.0f : 88.0f;
        const float baseX = static_cast<float>(cx + side * 86) + poseTurn * (near ? 10.0f : 3.0f);
        const float baseY = static_cast<float>(cy + 57);
        const lv_point_t from = {static_cast<lv_coord_t>(std::lround(baseX)), static_cast<lv_coord_t>(std::lround(baseY))};
        const lv_point_t to = {static_cast<lv_coord_t>(std::lround(baseX + std::cos(angle) * length)),
                               static_cast<lv_coord_t>(std::lround(baseY + std::sin(angle) * length))};
        const lv_opa_t glowOpa = static_cast<lv_opa_t>((near ? 78 : 34) * opacity / static_cast<int>(LV_OPA_COVER));
        const lv_opa_t coreOpa = static_cast<lv_opa_t>((near ? 255 : 145) * opacity / static_cast<int>(LV_OPA_COVER));
        drawRibbonSegment(layer, from, to, colors[index], near ? 14 : 9, glowOpa);
        drawRibbonSegment(layer, from, to, colors[index], near ? 7 : 5, coreOpa);
    }
}

}  // namespace

void GrokBotLabView::init(lv_obj_t* parent)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(kPanelSize, kPanelSize);
    _panel->setBgColor(lv_color_hex(0x000000));
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _stage = std::make_unique<Container>(_panel->get());
    _stage->align(LV_ALIGN_CENTER, 0, 0);
    _stage->setSize(kStageSize, kStageSize);
    // The panel supplies the black canvas. Keep this draw surface transparent:
    // LVGL paints its own background after DRAW_MAIN_BEGIN and would otherwise
    // cover the bot rendered by onStageDraw.
    _stage->setBgOpa(LV_OPA_TRANSP);
    _stage->setBorderWidth(0);
    _stage->setPaddingAll(0);
    _stage->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _stage->addEventCb(onStageDraw, LV_EVENT_DRAW_MAIN_BEGIN, this);
    _stage->addEventCb(onStageEvent, LV_EVENT_PRESSED, this);
    _stage->addEventCb(onStageEvent, LV_EVENT_PRESSING, this);
    _stage->addEventCb(onStageEvent, LV_EVENT_RELEASED, this);

    _state_started_at = lv_tick_get();
}

void GrokBotLabView::update(uint32_t now)
{
    const uint32_t auto_return_after = _state == State::Progress ? 4300 : 2400;
    if (_auto_return_to_idle && !_touching && now - _state_started_at > auto_return_after) {
        _auto_return_to_idle = false;
        setState(State::Idle);
    }
    if (now - _last_redraw_at < 33) {
        return;
    }
    _last_redraw_at = now;
    lv_obj_invalidate(_stage->get());
}

void GrokBotLabView::nextState()
{
    setState(static_cast<State>((static_cast<uint8_t>(_state) + 1) % static_cast<uint8_t>(State::Count)));
}

void GrokBotLabView::previousState()
{
    const auto count = static_cast<uint8_t>(State::Count);
    setState(static_cast<State>((static_cast<uint8_t>(_state) + count - 1) % count));
}

void GrokBotLabView::celebrate()
{
    setState(State::Celebrate);
    _auto_return_to_idle = true;
}

void GrokBotLabView::showProgress()
{
    setState(State::Progress);
    _auto_return_to_idle = true;
}

void GrokBotLabView::setState(State state)
{
    _previous_state = _state;
    if (_motion_initialized) {
        _transition_body_x = _motion_body_x;
        _transition_body_y = _motion_body_y;
        _transition_body_w = _motion_body_w;
        _transition_body_h = _motion_body_h;
        _transition_eye_y = _motion_eye_y;
        _transition_eye_gap = _motion_eye_gap;
        _transition_eye_w = _motion_eye_w;
        _transition_eye_h = _motion_eye_h;
        _transition_gaze_x = _motion_gaze_x;
        _transition_gaze_y = _motion_gaze_y;
    }
    _state = state;
    _state_started_at = lv_tick_get();
    if (state == State::Progress && _previous_state == State::Exclaim) {
        // The Exclaim→Progress bridge already lands on this compact pose. Seed
        // the motion filter with that landing geometry so the next regular
        // frame cannot briefly resurrect the older Sleeping pose.
        _motion_initialized = true;
        _motion_updated_at = _state_started_at;
        _motion_body_x = kCenter - 150;
        _motion_body_y = kCenter - 150;
        _motion_body_w = 300;
        _motion_body_h = 300;
        _motion_eye_y = kCenter - 38;
        _motion_eye_gap = 80;
        _motion_eye_w = 32;
        _motion_eye_h = 62;
    }
    // Choose a stable gaze for this expression. Positions are deliberately
    // discrete so left/up/right/down reads immediately on the small display.
    static constexpr int kGazeX[] = {0, -36, 36, 0, 0, -29, 29, -29, 29};
    static constexpr int kGazeY[] = {0, 0, 0, -18, 18, -15, -15, 15, 15};
    _gaze_seed = _gaze_seed * 1664525u + 1013904223u + static_cast<uint8_t>(state) * 97u + _state_started_at;
    const uint32_t gazeIndex = (_gaze_seed >> 16) % 9u;
    _expression_gaze_x = kGazeX[gazeIndex];
    _expression_gaze_y = kGazeY[gazeIndex];
    if (state == State::Sleeping) {
        _expression_gaze_x = 0;
        _expression_gaze_y = 12;
    }
    _auto_return_to_idle = false;
    _panel->setBgColor(lv_color_hex(0x000000));
    lv_obj_invalidate(_stage->get());
}

void GrokBotLabView::smoothPose(uint32_t now, int& body_x, int& body_y, int& body_w, int& body_h, int& eye_y, int& eye_gap, int& eye_w, int& eye_h)
{
    if (!_motion_initialized) {
        _motion_initialized = true;
        _motion_updated_at = now;
        _motion_body_x = body_x;
        _motion_body_y = body_y;
        _motion_body_w = body_w;
        _motion_body_h = body_h;
        _motion_eye_y = eye_y;
        _motion_eye_gap = eye_gap;
        _motion_eye_w = eye_w;
        _motion_eye_h = eye_h;
        return;
    }

    const float dt = std::min(0.050f, static_cast<float>(now - _motion_updated_at) / 1000.0f);
    _motion_updated_at = now;
    // Exponential damping keeps the visual response independent of redraw
    // cadence, while the 50 ms clamp prevents a wake-from-sleep jump.
    const float response = 1.0f - std::exp(-11.0f * dt);
    const auto approach = [response](float& current, float target) { current += (target - current) * response; };
    approach(_motion_body_x, body_x);
    approach(_motion_body_y, body_y);
    approach(_motion_body_w, body_w);
    approach(_motion_body_h, body_h);
    approach(_motion_eye_y, eye_y);
    approach(_motion_eye_gap, eye_gap);
    approach(_motion_eye_w, eye_w);
    approach(_motion_eye_h, eye_h);

    body_x = static_cast<int>(std::lround(_motion_body_x));
    body_y = static_cast<int>(std::lround(_motion_body_y));
    body_w = static_cast<int>(std::lround(_motion_body_w));
    body_h = static_cast<int>(std::lround(_motion_body_h));
    eye_y = static_cast<int>(std::lround(_motion_eye_y));
    eye_gap = static_cast<int>(std::lround(_motion_eye_gap));
    eye_w = static_cast<int>(std::lround(_motion_eye_w));
    eye_h = static_cast<int>(std::lround(_motion_eye_h));
}

void GrokBotLabView::smoothGaze(uint32_t now, float target_x, float target_y, int& gaze_x, int& gaze_y)
{
    if (_gaze_updated_at == 0) {
        _gaze_updated_at = now;
        _motion_gaze_x = target_x;
        _motion_gaze_y = target_y;
    }
    const float dt = std::min(0.050f, static_cast<float>(now - _gaze_updated_at) / 1000.0f);
    _gaze_updated_at = now;
    // Eyes lead the body slightly. This short, frame-rate independent filter
    // gives a deliberate saccade followed by a soft settle without a timer.
    const float response = 1.0f - std::exp(-16.0f * dt);
    _motion_gaze_x += (target_x - _motion_gaze_x) * response;
    _motion_gaze_y += (target_y - _motion_gaze_y) * response;
    gaze_x = static_cast<int>(std::lround(_motion_gaze_x));
    gaze_y = static_cast<int>(std::lround(_motion_gaze_y));
}

void GrokBotLabView::updateGaze(lv_event_t* event, bool active)
{
    _touching = active;
    if (!active) {
        _gaze_x = 0;
        _gaze_y = 0;
        return;
    }
    lv_indev_t* indev = lv_indev_get_act();
    lv_point_t point = {};
    if (indev != nullptr) {
        lv_indev_get_point(indev, &point);
        lv_area_t area;
        lv_obj_get_coords(lv_event_get_target_obj(event), &area);
        _gaze_x = std::clamp(static_cast<int>((point.x - (area.x1 + kStageSize / 2)) / 6), -40, 40);
        _gaze_y = std::clamp(static_cast<int>((point.y - (area.y1 + kStageSize / 2)) / 8), -22, 22);
    }
}

void GrokBotLabView::onStageEvent(lv_event_t* event)
{
    auto* view = static_cast<GrokBotLabView*>(lv_event_get_user_data(event));
    if (view == nullptr) return;
    const auto code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        view->setState(State::Curious);
        view->_auto_return_to_idle = true;
        view->updateGaze(event, true);
    } else if (code == LV_EVENT_PRESSING) {
        view->updateGaze(event, true);
    } else if (code == LV_EVENT_RELEASED) {
        view->updateGaze(event, false);
    }
}

void GrokBotLabView::onStageDraw(lv_event_t* event)
{
    auto* view = static_cast<GrokBotLabView*>(lv_event_get_user_data(event));
    if (view == nullptr) return;
    lv_layer_t* layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    const uint32_t now = lv_tick_get();
    const float seconds = static_cast<float>(now - view->_state_started_at) / 1000.0f;
    const int centerX = area.x1 + kCenter;
    const int centerY = area.y1 + kCenter;
    const auto ease = [](float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    };
    constexpr float kOpaCover = static_cast<float>(LV_OPA_COVER);
    // Special figures are not separate scenes: each is grown from or folded
    // back into the same ball. This bridge runs before normal state rendering.
    if (view->_state == State::Progress && view->_previous_state == State::Exclaim && seconds < 0.50f) {
        const float p = ease(seconds / 0.50f);
        drawExclaimSymbol(layer, centerX, centerY, 1.0f - p * 0.78f, static_cast<lv_opa_t>((1.0f - p) * kOpaCover));
        const int size = std::max(12, static_cast<int>(300 * p));
        const int eyeW = std::max(3, static_cast<int>(32 * p));
        const int eyeH = std::max(4, static_cast<int>(62 * p));
        const int eyeGap = static_cast<int>(80 * p);
        const int eyeY = centerY - 7 - eyeH / 2;
        drawRounded(layer, centerX - size / 2, centerY - size / 2, size, size, size / 2, 0x000000);
        drawRing(layer, centerX, centerY, size / 2 + 3, 0xC7EEFF, 4, static_cast<lv_opa_t>(96 * p));
        drawRounded(layer, centerX - eyeGap / 2 - eyeW / 2, eyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB,
                    static_cast<lv_opa_t>(p * kOpaCover));
        drawRounded(layer, centerX + eyeGap / 2 - eyeW / 2, eyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB,
                    static_cast<lv_opa_t>(p * kOpaCover));
        return;
    }
    if (view->_state == State::Exclaim && seconds < 0.48f) {
        const float p = ease(seconds / 0.48f);
        const int sourceX = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_x)) : area.x1;
        const int sourceY = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_y)) : area.y1;
        const int sourceW = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_w)) : kFaceSize;
        const int sourceH = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_h)) : kFaceSize;
        const int sourceCx = sourceX + sourceW / 2;
        const int sourceCy = sourceY + sourceH / 2;
        const int sourceSize = std::max(22, static_cast<int>(std::min(sourceW, sourceH) * (1.0f - p * 0.82f)));
        const lv_opa_t sourceOpa = static_cast<lv_opa_t>((1.0f - p * 0.45f) * kOpaCover);
        drawRounded(layer, sourceCx - sourceSize / 2, sourceCy - sourceSize / 2, sourceSize, sourceSize, sourceSize / 2, 0x000000, sourceOpa);
        const int eyeW = std::max(3, static_cast<int>(view->_transition_eye_w * (1.0f - p)));
        const int eyeH = std::max(3, static_cast<int>(view->_transition_eye_h * (1.0f - p)));
        const int eyeGap = static_cast<int>(view->_transition_eye_gap * (1.0f - p));
        const int sourceEyeCenterY = static_cast<int>(std::lround(view->_transition_eye_y + view->_transition_eye_h * 0.5f));
        const int eyeY = sourceEyeCenterY - eyeH / 2;
        const int eyeShiftX = static_cast<int>(std::lround(view->_transition_gaze_x * (1.0f - p)));
        const lv_opa_t eyeOpa = static_cast<lv_opa_t>((1.0f - p) * kOpaCover);
        drawRounded(layer, sourceCx - eyeGap / 2 - eyeW / 2 + eyeShiftX, eyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB, eyeOpa);
        drawRounded(layer, sourceCx + eyeGap / 2 - eyeW / 2 + eyeShiftX, eyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB, eyeOpa);
        drawExclaimSymbol(layer, centerX, centerY, p, static_cast<lv_opa_t>(p * kOpaCover));
        return;
    }
    if (view->_state == State::Star && view->_previous_state == State::Dance && seconds < 0.52f) {
        const float p = ease(seconds / 0.52f);
        const int sourceX = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_x)) : centerX - 118;
        const int sourceY = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_y)) : centerY - 90;
        const int sourceW = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_w)) : 235;
        const int sourceH = view->_motion_initialized ? static_cast<int>(std::lround(view->_transition_body_h)) : 235;
        const int sourceCx = sourceX + sourceW / 2;
        const int sourceCy = sourceY + sourceH / 2;
        const int sourceSize = std::max(16, static_cast<int>(std::min(sourceW, sourceH) * (1.0f - p * 0.84f)));
        drawDanceLights(layer, centerX, centerY, seconds, static_cast<lv_opa_t>((1.0f - p) * kOpaCover));
        drawRounded(layer, sourceCx - sourceSize / 2, sourceCy - sourceSize / 2, sourceSize, sourceSize, sourceSize / 2, 0x000000);
        drawRing(layer, sourceCx, sourceCy, sourceSize / 2 + 3, 0xBEEBFF, 4, static_cast<lv_opa_t>((1.0f - p) * 150));
        const int radius = std::max(8, static_cast<int>(174 * p));
        drawRoundedStar(layer, centerX, centerY, radius, 0xF4F1EB, static_cast<lv_opa_t>(p * kOpaCover));
        const int eyeW = std::max(3, static_cast<int>(36 * p));
        const int eyeH = std::max(4, static_cast<int>(68 * p));
        const int eyeGap = static_cast<int>(88 * p);
        const int eyeY = centerY - 45 + (68 - eyeH) / 2;
        drawRounded(layer, centerX - eyeGap / 2 - eyeW / 2, eyeY, eyeW, eyeH, eyeW / 2, 0x151515,
                    static_cast<lv_opa_t>(p * kOpaCover));
        drawRounded(layer, centerX + eyeGap / 2 - eyeW / 2, eyeY, eyeW, eyeH, eyeW / 2, 0x151515,
                    static_cast<lv_opa_t>(p * kOpaCover));
        return;
    }
    if (view->_state == State::Star && view->_previous_state == State::Exclaim && seconds < 0.52f) {
        const float p = ease(seconds / 0.52f);
        drawExclaimSymbol(layer, centerX, centerY, 1.0f - p * 0.78f, static_cast<lv_opa_t>((1.0f - p) * kOpaCover));
        const int radius = std::max(8, static_cast<int>(174 * p));
        drawRoundedStar(layer, centerX, centerY, radius, 0xF4F1EB, static_cast<lv_opa_t>(p * kOpaCover));
        const int eyeW = std::max(3, static_cast<int>(36 * p));
        const int eyeH = std::max(4, static_cast<int>(68 * p));
        const int eyeGap = static_cast<int>(88 * p);
        const int starEyeY = centerY - 45 + (68 - eyeH) / 2;
        drawRounded(layer, centerX - eyeGap / 2 - eyeW / 2, starEyeY, eyeW, eyeH, eyeW / 2, 0x151515,
                    static_cast<lv_opa_t>(p * kOpaCover));
        drawRounded(layer, centerX + eyeGap / 2 - eyeW / 2, starEyeY, eyeW, eyeH, eyeW / 2, 0x151515,
                    static_cast<lv_opa_t>(p * kOpaCover));
        return;
    }
    if (view->_state == State::Idle && view->_previous_state == State::Star && seconds < 0.52f) {
        const float p = ease(seconds / 0.52f);
        const int radius = std::max(5, static_cast<int>(174 * (1.0f - p)));
        drawRoundedStar(layer, centerX, centerY, radius, 0xF4F1EB, static_cast<lv_opa_t>((1.0f - p) * kOpaCover));
        const int eyeW = std::max(5, static_cast<int>(44 * p));
        const int eyeH = std::max(6, static_cast<int>(80 * p));
        const int eyeGap = static_cast<int>(136 * p);
        const int idleEyeY = area.y1 + 178 + (80 - eyeH) / 2;
        drawRounded(layer, centerX - eyeGap / 2 - eyeW / 2, idleEyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB,
                    static_cast<lv_opa_t>(p * kOpaCover));
        drawRounded(layer, centerX + eyeGap / 2 - eyeW / 2, idleEyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB,
                    static_cast<lv_opa_t>(p * kOpaCover));
        return;
    }
    const float breath = std::sin(seconds * 2.0f * kPi / 3.4f);
    const float blinkCycle = std::fmod(seconds, 4.1f);
    float blink = 1.0f;
    if (blinkCycle > 3.82f) {
        const float blinkPhase = (blinkCycle - 3.82f) / (4.1f - 3.82f);
        blink = std::max(0.18f, std::fabs(blinkPhase - 0.5f) * 2.0f);
    }
    // A short transition blink hides the most abrupt part of any new pose.
    if (seconds > 0.11f && seconds < 0.25f) {
        const float p = (seconds - 0.11f) / 0.14f;
        blink = std::min(blink, std::max(0.15f, std::fabs(p - 0.5f) * 2.0f));
    }
    int bodyW = kFaceSize;
    int bodyH = kFaceSize;
    int bodyX = area.x1 + kCenter - bodyW / 2;
    int bodyY = area.y1 + kCenter - bodyH / 2;
    // On a full-screen face the eyes belong slightly above the geometric
    // centre; this keeps the entire dial reading as one head rather than a chin.
    int eyeY = bodyY + 178;
    // The dial itself is the full face, so give the resting pair a generous
    // silhouette. Compact effect states below deliberately override this.
    int eyeGap = 136;
    int eyeW = 44;
    int openEyeH = 80;
    int eyeH = static_cast<int>(openEyeH * blink);
    float intentX = static_cast<float>(view->_expression_gaze_x);
    float intentY = static_cast<float>(view->_expression_gaze_y);
    // The star is a graphic interlude rather than a head-turn pose: keep its
    // two eyes centered and level so the symbol reads cleanly at a glance.
    if (view->_state == State::Star) {
        intentX = 0.0f;
        intentY = 0.0f;
    }
    if (!view->_touching) {
        // Low-amplitude scanning prevents an inactive face from looking frozen.
        const float statePhase = static_cast<float>(static_cast<uint8_t>(view->_state));
        intentX += std::sin(seconds * 0.73f + statePhase * 0.91f) * 1.6f;
        intentY += std::sin(seconds * 0.47f + statePhase * 0.57f) * 1.1f;
    } else {
        intentX = static_cast<float>(view->_gaze_x);
        intentY = static_cast<float>(view->_gaze_y);
    }
    if (!view->_touching) {
        // Keep a new expression's gaze in the same coordinate frame as the
        // outgoing face before it settles on its new look target. This removes
        // the one-frame lateral snap that previously read as eye shearing.
        const float gazeBlend = ease(std::clamp((seconds - 0.04f) / 0.38f, 0.0f, 1.0f));
        intentX = view->_transition_gaze_x + (intentX - view->_transition_gaze_x) * gazeBlend;
        intentY = view->_transition_gaze_y + (intentY - view->_transition_gaze_y) * gazeBlend;
    }
    const float arrival = std::exp(-seconds * 6.5f) * std::sin(seconds * 23.0f);
    intentX += arrival * 5.0f;
    intentY -= arrival * 2.5f;
    int offsetX = 0;
    int offsetY = 0;
    view->smoothGaze(now, intentX, intentY, offsetX, offsetY);
    bool drawBody = true;
    bool drawEyes = true;
    uint32_t eyeColor = 0xF4F1EB;
    const float stateEntry = std::min(1.0f, seconds / 0.32f);
    const lv_opa_t entryOpa = static_cast<lv_opa_t>(stateEntry * static_cast<float>(LV_OPA_COVER));

    if (view->_state == State::Listening) {
        bodyY -= 3;
        offsetY -= 8;
        eyeGap -= 8;
    } else if (view->_state == State::Working) {
        bodyW -= 12;
        bodyH += 4;
        eyeY += 8;
        eyeGap -= 16;
        offsetY += 4;
    } else if (view->_state == State::Sleeping) {
        bodyY += static_cast<int>(breath * 7.0f) + 5;
        eyeY += 8;
        openEyeH = 14;
        eyeH = openEyeH;
        eyeW = 54;
    } else if (view->_state == State::Surprised) {
        bodyY -= 7;
        bodyW = 350;
        bodyH = 350;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2 - 5;
        eyeY = bodyY + 138;
        eyeY -= 9;
        eyeW += 8;
        openEyeH = 106;
        eyeH = static_cast<int>(openEyeH * blink);
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 202 + static_cast<int>(std::sin(seconds * 7.0f) * 5.0f),
                 0xF4F1EB, 3, static_cast<lv_opa_t>(115 * entryOpa / static_cast<int>(LV_OPA_COVER)));
    } else if (view->_state == State::Celebrate) {
        bodyW = kEffectFaceSize;
        bodyH = kEffectFaceSize;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeGap = 76;
        eyeW = 31;
        openEyeH = 58;
        eyeH = static_cast<int>(openEyeH * blink);
        bodyY += static_cast<int>(std::fabs(std::sin(seconds * 5.0f)) * 15.0f) - 5;
        eyeY = bodyY + 100;
        eyeGap += 8;
    } else if (view->_state == State::Progress) {
        bodyW = 300;
        bodyH = 300;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 112;
        eyeGap = 80;
        eyeW = 32;
        openEyeH = 62;
        eyeH = static_cast<int>(openEyeH * blink);
    } else if (view->_state == State::Exclaim) {
        drawBody = false;
        drawEyes = false;
    } else if (view->_state == State::Star) {
        bodyW = 330;
        bodyH = 330;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 120;
        eyeGap = 88;
        eyeW = 36;
        openEyeH = 68;
        eyeH = static_cast<int>(openEyeH * blink);
        drawBody = false;
        eyeColor = 0x151515;
    } else if (view->_state == State::Orbit) {
        bodyW = 235;
        bodyH = 235;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2 + 28;
        eyeY = bodyY + 76;
        eyeGap = 64;
        eyeW = 27;
        openEyeH = 50;
        eyeH = static_cast<int>(openEyeH * blink);
    } else if (view->_state == State::Dance) {
        // An uneven beat makes the small ball feel like it is choosing its own
        // moves rather than following a repeating UI easing curve.
        const int beat = static_cast<int>(seconds / 0.46f);
        const float beatProgress = std::fmod(seconds, 0.46f) / 0.46f;
        uint32_t shuffle = static_cast<uint32_t>(beat) * 1103515245u + 12345u;
        const float jumpHeight = 48.0f + static_cast<float>((shuffle >> 16) % 48u);
        const float jump = std::sin(beatProgress * kPi) * jumpHeight;
        const float lean = static_cast<float>(static_cast<int>((shuffle >> 8) % 31u) - 15) * 0.9f;
        bodyW = 235;
        bodyH = 235;
        bodyX = area.x1 + kCenter - bodyW / 2 + static_cast<int>(lean * std::sin(beatProgress * kPi));
        bodyY = area.y1 + kCenter - bodyH / 2 + 28 - static_cast<int>(jump);
        eyeY = bodyY + 76;
        eyeGap = 64;
        eyeW = 27;
        openEyeH = 50;
        eyeH = static_cast<int>(openEyeH * blink);
    }

    if (view->_state == State::Happy) {
        bodyY -= 5;
        eyeY -= 4;
        offsetY -= 4;
        eyeW = 48;
        openEyeH = 56;
        eyeH = static_cast<int>(openEyeH * blink);
    } else if (view->_state == State::Playful) {
        bodyY += static_cast<int>(std::fabs(std::sin(seconds * 2.6f)) * 16.0f) - 8;
        bodyW += static_cast<int>(std::sin(seconds * 5.2f) * 9.0f);
        eyeGap += 5;
    } else if (view->_state == State::Alerting) {
        bodyW = 350;
        bodyH = 350;
        bodyX = area.x1 + kCenter - bodyW / 2 + static_cast<int>(std::sin(seconds * 25.0f) * 6.0f);
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 142;
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 205 + static_cast<int>(std::sin(seconds * 5.0f) * 8.0f),
                 0xFF4E5C, 4, static_cast<lv_opa_t>(static_cast<int>(LV_OPA_70) * entryOpa / static_cast<int>(LV_OPA_COVER)));
    }

    // A restrained settle is shared by every drawable face, so changing state
    // feels like the same character changing its mind rather than a new frame.
    if (drawEyes) {
        const float settle = std::exp(-seconds * 8.5f) * std::sin(seconds * 27.0f);
        eyeGap += static_cast<int>(settle * 7.0f);
        eyeW = std::max(12, eyeW + static_cast<int>(settle * 3.0f));
        eyeH = std::max(7, eyeH - static_cast<int>(settle * 5.0f));
    }
    if (drawEyes) eyeY += (openEyeH - eyeH) / 2;

    view->smoothPose(now, bodyX, bodyY, bodyW, bodyH, eyeY, eyeGap, eyeW, eyeH);

    const bool hasOrbitScene = view->_state == State::Orbit;
    const bool isDanceScene = view->_state == State::Dance;
    const int faceCenterX = bodyX + bodyW / 2;
    const int faceCenterY = bodyY + bodyH / 2;
    const float danceTurn = std::clamp(static_cast<float>(offsetX) / 30.0f, -1.0f, 1.0f);
    // Keep prop z-order stable for the entire Dance expression. The eye motion
    // still steers the sticks' placement, but a gaze crossing centre cannot
    // make a whole glow stick abruptly pop through the ball.
    const float danceDepth = view->_touching ? danceTurn : (view->_expression_gaze_x >= 0 ? 0.35f : -0.35f);
    if (hasOrbitScene) {
        drawRibbons(layer, faceCenterX, faceCenterY, seconds, false, entryOpa);
        drawOrbitSatellites(layer, faceCenterX, faceCenterY, seconds, false, entryOpa);
    } else if (isDanceScene) {
        const float lightIn = ease(std::clamp(seconds / 0.56f, 0.0f, 1.0f));
        const lv_opa_t lightOpa = static_cast<lv_opa_t>(entryOpa * lightIn);
        drawDanceLights(layer, area.x1 + kCenter, area.y1 + kCenter, seconds, lightOpa);
        // Orbit's trails dissolve only at Dance entry; they never remain as a
        // permanent decorative layer behind the stage lights.
        if (view->_previous_state == State::Orbit && lightIn < 1.0f) {
            const lv_opa_t trailOpa = static_cast<lv_opa_t>(entryOpa * (1.0f - lightIn));
            drawRibbons(layer, faceCenterX, faceCenterY, seconds * 1.22f, false, trailOpa);
        }
        drawDanceGlowSticks(layer, faceCenterX, faceCenterY, seconds, danceTurn, danceDepth, false, entryOpa);
    }
    if (view->_state == State::Celebrate) {
        drawCelebrateBurst(layer, faceCenterX, faceCenterY - bodyH / 3, seconds, entryOpa);
    }

    // A single pure-black body blends into the round AMOLED bezel.
    const int bodyRadius = std::min(bodyW, bodyH) / 2;
    if (drawBody) {
        drawRounded(layer, bodyX, bodyY, bodyW, bodyH, bodyRadius, 0x000000);
    }
    if (view->_state == State::Star) drawRoundedStar(layer, bodyX + bodyW / 2, bodyY + bodyH / 2, 174, 0xF4F1EB);
    if (drawEyes) {
        // Treat gaze direction as the face turning away from that side: when
        // looking screen-left, the screen-right eye is visually nearer (and
        // vice versa). The near eye grows and travels farther, giving the dial
        // a readable turn without an extra 3D render pass.
        const float turn = danceTurn;
        const float turnAmount = std::fabs(turn);
        // Position carries the glance; size is only a subtle depth cue.
        const float leftScale = turn > 0.0f ? 1.0f + turnAmount * 0.065f : 1.0f - turnAmount * 0.065f;
        const float rightScale = turn < 0.0f ? 1.0f + turnAmount * 0.065f : 1.0f - turnAmount * 0.065f;
        const int leftW = std::max(8, static_cast<int>(eyeW * leftScale));
        const int rightW = std::max(8, static_cast<int>(eyeW * rightScale));
        const int leftH = std::max(7, static_cast<int>(eyeH * leftScale));
        const int rightH = std::max(7, static_cast<int>(eyeH * rightScale));
        const int leftShift = static_cast<int>(offsetX * (turn > 0.0f ? 1.04f : 0.90f));
        const int rightShift = static_cast<int>(offsetX * (turn < 0.0f ? 1.04f : 0.90f));
        const int leftY = eyeY + offsetY + (turn > 0.0f ? 1 : -1);
        const int rightY = eyeY + offsetY + (turn < 0.0f ? 1 : -1);
        drawRounded(layer, bodyX + bodyW / 2 - eyeGap / 2 - leftW / 2 + leftShift, leftY, leftW, leftH, leftW / 2, eyeColor);
        drawRounded(layer, bodyX + bodyW / 2 + eyeGap / 2 - rightW / 2 + rightShift, rightY, rightW, rightH, rightW / 2, eyeColor);
    }

    if (view->_state == State::Happy || view->_state == State::Playful) {
        // Eyes now sit higher on the full dial; keep the smile in the same
        // facial zone rather than stranded near the chin.
        drawRounded(layer, bodyX + bodyW / 2 - 46, bodyY + bodyH * 68 / 100, 92, 15, 8, 0xF4F1EB, 199);
    }
    if (view->_state == State::Celebrate) {
        drawCelebrateFrontFall(layer, faceCenterX, faceCenterY, seconds, entryOpa);
    }

    if (view->_state == State::Celebrate) {
        drawEffectContour(layer, faceCenterX, faceCenterY, bodyW / 2, 0xFFE7BA);
    } else if (view->_state == State::Progress) {
        drawEffectContour(layer, faceCenterX, faceCenterY, bodyW / 2, 0xC7EEFF);
    } else if (view->_state == State::Orbit) {
        drawEffectContour(layer, faceCenterX, faceCenterY, bodyW / 2, 0xBEEBFF);
    } else if (view->_state == State::Dance) {
        drawRing(layer, faceCenterX, faceCenterY, bodyW / 2 + 4, 0xBEEBFF, 7, 84);
        drawRing(layer, faceCenterX, faceCenterY, bodyW / 2 + 1, 0xBEEBFF, 2, 190);
    }

    if (hasOrbitScene) {
        drawRibbons(layer, faceCenterX, faceCenterY, seconds, true, entryOpa);
        drawOrbitSatellites(layer, faceCenterX, faceCenterY, seconds, true, entryOpa);
    } else if (isDanceScene) {
        const float lightIn = ease(std::clamp(seconds / 0.56f, 0.0f, 1.0f));
        if (view->_previous_state == State::Orbit && lightIn < 1.0f) {
            drawRibbons(layer, faceCenterX, faceCenterY, seconds * 1.22f, true,
                        static_cast<lv_opa_t>(entryOpa * (1.0f - lightIn)));
        }
        drawDanceGlowSticks(layer, faceCenterX, faceCenterY, seconds, danceTurn, danceDepth, true, entryOpa);
    }

    // Foreground symbols are drawn after the face so they remain legible on
    // the full-screen black sphere as well as the compact effect states.
    if (view->_state == State::Thinking) {
        for (int i = 0; i < 3; ++i) {
            const float phase = seconds * 2.8f + i * 0.72f;
            const int dotX = area.x1 + kCenter + 170 + static_cast<int>(std::cos(phase) * 14.0f);
            const int dotY = area.y1 + kCenter - 80 + i * 40;
            drawRounded(layer, dotX - 8, dotY - 8, 16, 16, 8, 0xA9D6FF,
                        static_cast<lv_opa_t>((90 + 150 * (0.5f + 0.5f * std::sin(phase))) * entryOpa / static_cast<float>(LV_OPA_COVER)));
        }
    } else if (view->_state == State::Working) {
        for (int i = 0; i < 4; ++i) {
            const float phase = seconds * 4.0f + i * 1.22f;
            const int x = area.x1 + kCenter - 95 + i * 64;
            const int y = area.y1 + kCenter + 180 + static_cast<int>(std::sin(phase) * 8.0f);
            drawRounded(layer, x - 6, y - 6, 12, 12, 6, 0xF4F1EB,
                        static_cast<lv_opa_t>((75 + 115 * (0.5f + 0.5f * std::sin(phase))) * entryOpa / static_cast<float>(LV_OPA_COVER)));
        }
    } else if (view->_state == State::Sleeping) {
        for (int i = 0; i < 2; ++i) {
            const float phase = seconds * 1.2f + i * 1.6f;
            const int x = area.x1 + kCenter + 160 + i * 28;
            const int y = area.y1 + kCenter - 76 - i * 34 + static_cast<int>(std::sin(phase) * 5.0f);
            drawRounded(layer, x - 7, y - 7, 14 + i * 4, 14 + i * 4, 7, 0xA9D6FF,
                        static_cast<lv_opa_t>(166 * entryOpa / static_cast<int>(LV_OPA_COVER)));
        }
    } else if (view->_state == State::Progress) {
        const int progress = static_cast<int>(std::min(1.0f, seconds / 3.40f) * 360.0f);
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 211, 0x2F2F2F, 8, LV_OPA_COVER);
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color = lv_color_hex(0xF4F1EB);
        arc.width = 7;
        arc.opa = LV_OPA_COVER;
        arc.center.x = area.x1 + kCenter;
        arc.center.y = area.y1 + kCenter;
        arc.radius = 211;
        arc.start_angle = 270;
        arc.end_angle = 270 + progress;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
    } else if (view->_state == State::Exclaim) {
        const float pulse = 0.92f + 0.08f * std::sin(seconds * 7.0f);
        const int stemW = static_cast<int>(38 * pulse);
        const int stemH = static_cast<int>(146 * pulse);
        const int dotSize = static_cast<int>(44 * pulse);
        const int dotY = area.y1 + kCenter + 82 + static_cast<int>(std::sin(seconds * 7.0f) * 7.0f);
        drawRounded(layer, area.x1 + kCenter - stemW / 2, area.y1 + kCenter - 104, stemW, stemH, stemW / 2, 0xF4F1EB);
        drawRounded(layer, area.x1 + kCenter - dotSize / 2, dotY, dotSize, dotSize, dotSize / 2, 0xF4F1EB);
    }

}
