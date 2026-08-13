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
constexpr int kFaceSize = 430;
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

void drawArc(lv_layer_t* layer, int cx, int cy, int radius, int start_angle, int end_angle, uint32_t color, int width, lv_opa_t opa)
{
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.width = width;
    dsc.opa = opa;
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = radius;
    dsc.start_angle = start_angle;
    dsc.end_angle = end_angle;
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

void drawHexagon(lv_layer_t* layer, int cx, int cy, int radius, uint32_t color)
{
    lv_point_t points[6] = {};
    for (int i = 0; i < 6; ++i) {
        const float angle = (30.0f + i * 60.0f) * kPi / 180.0f;
        points[i] = {static_cast<lv_coord_t>(cx + std::cos(angle) * radius), static_cast<lv_coord_t>(cy + std::sin(angle) * radius)};
    }
    const lv_point_t center = {static_cast<lv_coord_t>(cx), static_cast<lv_coord_t>(cy)};
    for (int i = 0; i < 6; ++i) drawTriangle(layer, center, points[i], points[(i + 1) % 6], color);
}

const char* stateName(GrokBotLabView::State state)
{
    static constexpr const char* names[] = {
        "IDLE", "CURIOUS", "LISTENING", "THINKING", "WORKING",
        "HAPPY", "PLAYFUL", "SURPRISED", "SLEEPING", "ALERT", "CELEBRATE", "PROGRESS",
        "EXCLAIM", "HEX", "PARTY", "ORBIT", "SPAWN",
    };
    return names[static_cast<uint8_t>(state)];
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smooth(float value)
{
    value = clamp01(value);
    return value * value * (3.0f - 2.0f * value);
}

void drawDemoFace(lv_layer_t* layer, int cx, int cy, int size, float eye_turn, lv_opa_t opa = LV_OPA_COVER)
{
    const int x = cx - size / 2;
    const int y = cy - size / 2;
    drawRounded(layer, x, y, size, size, size / 2, 0x101114, opa);
    drawRounded(layer, x + size / 13, y + size / 13, size - size * 2 / 13, size / 2, size / 2 - size / 13, 0x1C1D21,
                static_cast<lv_opa_t>(opa * 0.62f));
    const int eyeW = std::max(8, size / 9);
    const int eyeH = std::max(14, size / 5);
    const int gap = size / 4;
    const int eyeY = cy - eyeH / 4 + static_cast<int>(eye_turn * 8.0f);
    const int eyeShift = static_cast<int>(eye_turn * 13.0f);
    drawRounded(layer, cx - gap - eyeW / 2 + eyeShift, eyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB, opa);
    drawRounded(layer, cx + gap - eyeW / 2 + eyeShift, eyeY, eyeW, eyeH, eyeW / 2, 0xF4F1EB, opa);
}

void drawDemoTimeline(lv_layer_t* layer, const lv_area_t& area, float elapsed)
{
    constexpr float kDuration = 18.0f;
    const float t = std::fmod(elapsed, kDuration);
    const int cx = area.x1 + kCenter;
    const int cy = area.y1 + kCenter;

    // 0–2.4: familiar face settles in and looks upward.
    if (t < 2.4f) {
        const float p = smooth(t / 2.4f);
        drawDemoFace(layer, cx, cy, static_cast<int>(325 + p * 85), -0.25f + p * 0.8f);
        return;
    }

    // 2.4–4.4: circle and hexagon cross-morph through a shared centre/scale.
    if (t < 4.4f) {
        const float p = smooth((t - 2.4f) / 2.0f);
        const int radius = static_cast<int>(204 - p * 36);
        drawDemoFace(layer, cx, cy, radius * 2, 0.35f, static_cast<lv_opa_t>((1.0f - p) * 255));
        drawHexagon(layer, cx, cy, radius, 0x101114);
        const int eyeW = 34;
        const int eyeH = 65;
        drawRounded(layer, cx - 64, cy - 12, eyeW, eyeH, eyeW / 2, 0xF4F1EB);
        drawRounded(layer, cx + 30, cy - 12, eyeW, eyeH, eyeW / 2, 0xF4F1EB);
        return;
    }

    // 4.4–6.4: the shape contracts into a travelling seed with a colour trail.
    if (t < 6.4f) {
        const float p = smooth((t - 4.4f) / 2.0f);
        const int size = std::max(18, static_cast<int>(336 * (1.0f - p)));
        const int x = cx - static_cast<int>(p * 128.0f);
        const int y = cy + static_cast<int>(p * 72.0f);
        drawDemoFace(layer, x, y, size, 0.2f, static_cast<lv_opa_t>(255 - p * 45));
        static constexpr uint32_t colors[] = {0x54C8B8, 0xB64CD9, 0xF0A640};
        for (int i = 0; i < 5; ++i) {
            const float tail = p - i * 0.12f;
            if (tail < 0.0f) continue;
            const int tx = x - 25 - i * 30;
            const int ty = y + 18 - i * 15;
            drawRounded(layer, tx - 8, ty - 8, 16, 16, 8, colors[i % 3], static_cast<lv_opa_t>(170 - i * 24));
        }
        return;
    }

    // 6.4–9.2: the seed blooms back into a face while orbital paths emerge.
    if (t < 9.2f) {
        const float p = smooth((t - 6.4f) / 2.8f);
        drawDemoFace(layer, cx, cy + 18, static_cast<int>(42 + p * 210), -0.1f);
        static constexpr uint32_t colors[] = {0x5B68D9, 0xB64CD9, 0xE45B67, 0xF0A640};
        for (int i = 0; i < 4; ++i) {
            const int start = static_cast<int>(std::fmod(elapsed * 95.0f + i * 91.0f, 360.0f));
            drawArc(layer, cx, cy + 18, 132 + i * 14, start, start + static_cast<int>(58 + p * 62), colors[i], 6, 220);
        }
        return;
    }

    // 9.2–12.2: orbital energy condenses into a party hat and ribbons.
    if (t < 12.2f) {
        const float p = smooth((t - 9.2f) / 3.0f);
        const int size = 235;
        const int faceY = cy + 36;
        drawDemoFace(layer, cx, faceY, size, 0.0f);
        const int hatY = faceY - size / 2 - static_cast<int>(p * 116);
        drawTriangle(layer, {static_cast<lv_coord_t>(cx - 70), static_cast<lv_coord_t>(faceY - 85)},
                     {static_cast<lv_coord_t>(cx + 70), static_cast<lv_coord_t>(faceY - 85)}, {static_cast<lv_coord_t>(cx), static_cast<lv_coord_t>(hatY)},
                     0x101114);
        static constexpr uint32_t colors[] = {0xF0A640, 0xB64CD9, 0x5B68D9, 0xE45B67, 0x54C8B8};
        for (int i = 0; i < 5; ++i) {
            const int start = 210 + i * 23 + static_cast<int>(std::sin(elapsed * 2.0f + i) * 9.0f);
            drawArc(layer, cx, cy + 30, 134 + i * 12, start, start + static_cast<int>(30 + p * 45), colors[i], 6, 220);
        }
        return;
    }

    // 12.2–14.4: party shape drops away; a crisp exclamation owns the frame.
    if (t < 14.4f) {
        const float p = smooth((t - 12.2f) / 2.2f);
        drawDemoFace(layer, cx, cy + 70, static_cast<int>(235 * (1.0f - p)), 0.0f, static_cast<lv_opa_t>((1.0f - p) * 255));
        const int stemW = static_cast<int>(28 + p * 16);
        const int stemH = static_cast<int>(100 + p * 62);
        drawRounded(layer, cx - stemW / 2, cy - 112, stemW, stemH, stemW / 2, 0x101114);
        drawRounded(layer, cx - 23, cy + 87, 46, 46, 23, 0x101114);
        return;
    }

    // 14.4–18: the punctuation softens into the next round Idle face.
    const float p = smooth((t - 14.4f) / 3.6f);
    drawRounded(layer, cx - 20, cy - 112, 40, 150, 20, 0x101114, static_cast<lv_opa_t>((1.0f - p) * 255));
    drawRounded(layer, cx - 23, cy + 87, 46, 46, 23, 0x101114, static_cast<lv_opa_t>((1.0f - p) * 255));
    drawDemoFace(layer, cx, cy, static_cast<int>(32 + p * 398), -0.15f + p * 0.15f, static_cast<lv_opa_t>(p * 255));
}
}  // namespace

void GrokBotLabView::init(lv_obj_t* parent)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(kPanelSize, kPanelSize);
    _panel->setBgColor(lv_color_hex(0xF7F6F4));
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _stage = std::make_unique<Container>(_panel->get());
    _stage->align(LV_ALIGN_CENTER, 0, 0);
    _stage->setSize(kStageSize, kStageSize);
    _stage->setBgOpa(LV_OPA_TRANSP);
    _stage->setBorderWidth(0);
    _stage->setPaddingAll(0);
    _stage->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _stage->addEventCb(onStageDraw, LV_EVENT_DRAW_MAIN_BEGIN, this);
    _stage->addEventCb(onStageEvent, LV_EVENT_PRESSED, this);
    _stage->addEventCb(onStageEvent, LV_EVENT_PRESSING, this);
    _stage->addEventCb(onStageEvent, LV_EVENT_RELEASED, this);

    _state_label = std::make_unique<Label>(_panel->get());
    _state_label->align(LV_ALIGN_TOP_MID, 0, 32);
    _state_label->setTextFont(&lv_font_maple_mono_medium_24);
    _state_label->setTextColor(lv_color_hex(0x252525));
    _state_label->setOpa(0);

    _state_started_at = lv_tick_get();
    _state_label_shown_at = _state_started_at;
    updateLabels();
}

void GrokBotLabView::update(uint32_t now)
{
    if (_auto_return_to_idle && !_touching && now - _state_started_at > 2400) {
        _auto_return_to_idle = false;
        setState(State::Idle);
    }
    constexpr uint32_t kLabelHoldMs = 900;
    constexpr uint32_t kLabelFadeMs = 350;
    const uint32_t labelAge = now - _state_label_shown_at;
    if (labelAge < kLabelHoldMs) {
        _state_label->setOpa(LV_OPA_COVER);
    } else if (labelAge < kLabelHoldMs + kLabelFadeMs) {
        const auto fade = static_cast<lv_opa_t>(LV_OPA_COVER - (labelAge - kLabelHoldMs) * LV_OPA_COVER / kLabelFadeMs);
        _state_label->setOpa(fade);
    } else {
        _state_label->setOpa(LV_OPA_TRANSP);
    }
    if (now - _last_redraw_at < 33) {
        return;
    }
    _last_redraw_at = now;
    lv_obj_invalidate(_stage->get());
}

void GrokBotLabView::nextState()
{
    _demo_mode = false;
    setState(static_cast<State>((static_cast<uint8_t>(_state) + 1) % static_cast<uint8_t>(State::Count)));
}

void GrokBotLabView::previousState()
{
    _demo_mode = false;
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

void GrokBotLabView::toggleDemo()
{
    _demo_mode = !_demo_mode;
    _demo_index = 0;
    _demo_step_started_at = lv_tick_get();
    setState(State::Idle);
    if (_demo_mode) _state_label->setOpa(LV_OPA_TRANSP);
}

void GrokBotLabView::setButtonFeedback(bool left_pressed, bool right_pressed, uint32_t now)
{
    if (_left_button_pressed && !left_pressed) {
        _left_button_released_at = now;
    }
    if (_right_button_pressed && !right_pressed) {
        _right_button_released_at = now;
    }
    _left_button_pressed = left_pressed;
    _right_button_pressed = right_pressed;
}

void GrokBotLabView::setState(State state)
{
    _state = state;
    _state_started_at = lv_tick_get();
    _state_label_shown_at = _state_started_at;
    _auto_return_to_idle = false;
    updateLabels();
    lv_obj_invalidate(_stage->get());
}

void GrokBotLabView::updateLabels()
{
    _state_label->setText(stateName(_state));
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
        _gaze_x = std::clamp(static_cast<int>((point.x - (area.x1 + kStageSize / 2)) / 9), -14, 14);
        _gaze_y = std::clamp(static_cast<int>((point.y - (area.y1 + kStageSize / 2)) / 11), -10, 10);
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
    if (view->_demo_mode) {
        drawDemoTimeline(layer, area, seconds);
        return;
    }
    const float breath = std::sin(seconds * 2.0f * kPi / 3.4f);
    const float blinkCycle = std::fmod(seconds, 4.1f);
    const float blink = blinkCycle > 3.82f ? std::max(0.18f, std::fabs(blinkCycle - 3.96f) * 18.0f) : 1.0f;
    int bodyW = kFaceSize;
    int bodyH = kFaceSize;
    int bodyX = area.x1 + kCenter - bodyW / 2;
    int bodyY = area.y1 + kCenter - bodyH / 2 + static_cast<int>(breath * 5.0f);
    int eyeY = bodyY + 220;
    int eyeGap = 116;
    int eyeW = 44;
    int eyeH = static_cast<int>(80 * blink);
    int offsetX = view->_gaze_x;
    int offsetY = view->_gaze_y;
    bool drawBody = true;
    bool drawEyes = true;

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
        eyeH = 14;
        eyeW = 54;
    } else if (view->_state == State::Surprised) {
        bodyY -= 7;
        bodyW = 350;
        bodyH = 350;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2 - 5;
        eyeY = bodyY + 175;
        eyeY -= 9;
        eyeW += 8;
        eyeH += 26;
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 202 + static_cast<int>(std::sin(seconds * 7.0f) * 5.0f),
                 0xF4F1EB, 3, 115);
    } else if (view->_state == State::Celebrate) {
        bodyW = kEffectFaceSize;
        bodyH = kEffectFaceSize;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 138;
        eyeGap = 76;
        eyeW = 31;
        eyeH = static_cast<int>(58 * blink);
        bodyY += static_cast<int>(std::fabs(std::sin(seconds * 5.0f)) * 15.0f) - 5;
        eyeY -= 4;
        eyeGap += 8;
    } else if (view->_state == State::Progress) {
        bodyW = 300;
        bodyH = 300;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 154;
        eyeGap = 80;
        eyeW = 32;
        eyeH = static_cast<int>(62 * blink);
    } else if (view->_state == State::Exclaim) {
        drawBody = false;
        drawEyes = false;
    } else if (view->_state == State::Hex) {
        bodyW = 330;
        bodyH = 330;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 160;
        eyeGap = 88;
        eyeW = 36;
        eyeH = static_cast<int>(68 * blink);
        drawBody = false;
    } else if (view->_state == State::Party || view->_state == State::Orbit || view->_state == State::Spawn) {
        bodyW = 235;
        bodyH = 235;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2 + 28;
        eyeY = bodyY + 118;
        eyeGap = 64;
        eyeW = 27;
        eyeH = static_cast<int>(50 * blink);
    }

    if (view->_state == State::Happy) {
        bodyY -= 5;
        eyeY -= 4;
        offsetY -= 4;
    } else if (view->_state == State::Playful) {
        bodyY += static_cast<int>(std::fabs(std::sin(seconds * 2.6f)) * 16.0f) - 8;
        bodyW += static_cast<int>(std::sin(seconds * 5.2f) * 9.0f);
        eyeGap += 5;
    } else if (view->_state == State::Alerting) {
        bodyX += static_cast<int>(std::sin(seconds * 25.0f) * 6.0f);
        bodyW = 350;
        bodyH = 350;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2;
        eyeY = bodyY + 180;
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 205 + static_cast<int>(std::sin(seconds * 5.0f) * 8.0f),
                 0xFF4E5C, 4, LV_OPA_70);
    } else if (view->_state == State::Thinking) {
        for (int i = 0; i < 3; ++i) {
            const float phase = seconds * 2.8f + i * 0.72f;
            const int dotX = area.x1 + kCenter + 170 + static_cast<int>(std::cos(phase) * 14.0f);
            const int dotY = area.y1 + kCenter - 80 + i * 40;
            drawRounded(layer, dotX - 8, dotY - 8, 16, 16, 8, 0xA9D6FF,
                        static_cast<lv_opa_t>(90 + 150 * (0.5f + 0.5f * std::sin(phase))));
        }
    } else if (view->_state == State::Working) {
        for (int i = 0; i < 4; ++i) {
            const float phase = seconds * 4.0f + i * 1.22f;
            const int x = area.x1 + kCenter - 95 + i * 64;
            const int y = area.y1 + kCenter + 180 + static_cast<int>(std::sin(phase) * 8.0f);
            drawRounded(layer, x - 6, y - 6, 12, 12, 6, 0xF4F1EB,
                        static_cast<lv_opa_t>(75 + 115 * (0.5f + 0.5f * std::sin(phase))));
        }
    } else if (view->_state == State::Sleeping) {
        for (int i = 0; i < 2; ++i) {
            const float phase = seconds * 1.2f + i * 1.6f;
            const int x = area.x1 + kCenter + 160 + i * 28;
            const int y = area.y1 + kCenter - 76 - i * 34 + static_cast<int>(std::sin(phase) * 5.0f);
            drawRounded(layer, x - 7, y - 7, 14 + i * 4, 14 + i * 4, 7, 0xA9D6FF, 166);
        }
    } else if (view->_state == State::Celebrate) {
        static constexpr uint32_t colors[] = {0xF4F1EB, 0xA9D6FF, 0xFFCC66, 0xFF8292};
        for (int i = 0; i < 18; ++i) {
            const float phase = seconds * (1.7f + (i % 3) * 0.22f) + i * 0.81f;
            const int radius = 156 + (i % 5) * 10;
            const int x = area.x1 + kCenter + static_cast<int>(std::cos(phase) * radius);
            const int y = area.y1 + kCenter + static_cast<int>(std::sin(phase * 1.31f) * (78 + (i % 4) * 12));
            const int size = 5 + (i % 3) * 2;
            drawRounded(layer, x - size / 2, y - size / 2, size, size, size / 2, colors[i % 4], 210);
        }
    } else if (view->_state == State::Progress) {
        const int progress = static_cast<int>(std::fmod(seconds * 0.18f, 1.0f) * 360.0f);
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
        drawRounded(layer, area.x1 + kCenter - stemW / 2, area.y1 + kCenter - 104, stemW, stemH, stemW / 2, 0x101114);
        drawRounded(layer, area.x1 + kCenter - 22, area.y1 + kCenter + 82, 44, 44, 22, 0x101114);
    } else if (view->_state == State::Party) {
        const int hatTop = bodyY - 128;
        drawTriangle(layer, {static_cast<lv_coord_t>(bodyX + 46), static_cast<lv_coord_t>(bodyY + 28)},
                     {static_cast<lv_coord_t>(bodyX + bodyW - 44), static_cast<lv_coord_t>(bodyY + 28)},
                     {static_cast<lv_coord_t>(bodyX + bodyW / 2), static_cast<lv_coord_t>(hatTop)}, 0x101114);
        static constexpr uint32_t colors[] = {0xF0A640, 0xB64CD9, 0x5B68D9, 0xE45B67, 0x54C8B8};
        for (int i = 0; i < 5; ++i) {
            const int radius = 132 + i * 12;
            const int start = 205 + i * 23 + static_cast<int>(std::sin(seconds * 2.0f + i) * 9.0f);
            drawArc(layer, area.x1 + kCenter, area.y1 + kCenter + 20, radius, start, start + 72, colors[i], 6, 220);
        }
    } else if (view->_state == State::Orbit) {
        static constexpr uint32_t colors[] = {0x5B68D9, 0xB64CD9, 0xE45B67, 0xF0A640};
        for (int i = 0; i < 4; ++i) {
            const int start = static_cast<int>(std::fmod(seconds * 95.0f + i * 91.0f, 360.0f));
            drawArc(layer, area.x1 + kCenter, area.y1 + kCenter + 18, 148 + i * 13, start, start + 116, colors[i], 6, 220);
        }
    } else if (view->_state == State::Spawn) {
        const float cycle = std::fmod(seconds, 2.4f) / 2.4f;
        const float scale = cycle < 0.5f ? cycle * 2.0f : 2.0f - cycle * 2.0f;
        bodyW = std::max(18, static_cast<int>(235 * scale));
        bodyH = bodyW;
        bodyX = area.x1 + kCenter - bodyW / 2;
        bodyY = area.y1 + kCenter - bodyH / 2 + 28;
        eyeY = bodyY + bodyH / 2;
        eyeGap = std::max(8, bodyW / 4);
        eyeW = std::max(5, bodyW / 9);
        eyeH = std::max(8, bodyW / 5);
        for (int i = 0; i < 4; ++i) {
            const int x = area.x1 + kCenter - 110 + i * 34 + static_cast<int>(cycle * 100.0f);
            const int y = area.y1 + kCenter + 102 - i * 17;
            drawRounded(layer, x - 6, y - 6, 12, 12, 6, i % 2 ? 0xB64CD9 : 0x54C8B8,
                        static_cast<lv_opa_t>(220 * (1.0f - cycle)));
        }
    }

    // Black body with warm-white eyes on the official-style light canvas.
    const int bodyRadius = std::min(bodyW, bodyH) / 2;
    if (drawBody) {
        drawRounded(layer, bodyX, bodyY, bodyW, bodyH, bodyRadius, 0x101114);
        drawRounded(layer, bodyX + 16, bodyY + 16, bodyW - 32, bodyH / 2, std::max(12, bodyRadius - 16), 0x1C1D21, 184);
        drawRounded(layer, bodyX + 14, bodyY + bodyH / 2, bodyW - 28, bodyH / 2 - 14, std::max(12, bodyRadius - 22), 0x08090B, 220);
    }
    if (view->_state == State::Hex) drawHexagon(layer, bodyX + bodyW / 2, bodyY + bodyH / 2, 174, 0x101114);
    if (view->_state == State::Curious) {
        offsetX += 8;
        offsetY -= 3;
    }
    if (drawEyes) {
        drawRounded(layer, bodyX + bodyW / 2 - eyeGap / 2 - eyeW / 2 + offsetX, eyeY + offsetY, eyeW, std::max(7, eyeH), eyeW / 2, 0xF4F1EB);
        drawRounded(layer, bodyX + bodyW / 2 + eyeGap / 2 - eyeW / 2 + offsetX, eyeY + offsetY, eyeW, std::max(7, eyeH), eyeW / 2, 0xF4F1EB);
    }

    if (view->_state == State::Happy || view->_state == State::Playful) {
        drawRounded(layer, bodyX + bodyW / 2 - 46, bodyY + bodyH * 3 / 4, 92, 15, 8, 0xF4F1EB, 199);
    }

    // Side-key feedback sits above the face at the physical key's matching
    // screen edge: yellow at 10:30, blue at 1:30.
    constexpr uint32_t kEdgeReleaseMs = 220;
    const auto drawButtonPulse = [&](bool pressed, uint32_t released_at, bool left, uint32_t color) {
        float amount = 0.0f;
        if (pressed) {
            amount = 1.0f;
        } else if (released_at != 0 && now - released_at < kEdgeReleaseMs) {
            const float t = static_cast<float>(now - released_at) / kEdgeReleaseMs;
            amount = (1.0f - t) * (1.0f - t);
        }
        if (amount <= 0.01f) return;

        const int start = left ? 192 : 282;
        const int end = left ? 258 : 348;
        const int inset = static_cast<int>((1.0f - amount) * 18.0f);
        drawArc(layer, area.x1 + kCenter, area.y1 + kCenter, 226 - inset, start, end, color, 12,
                static_cast<lv_opa_t>(24 + amount * 72));
        drawArc(layer, area.x1 + kCenter, area.y1 + kCenter, 221 - inset, start + 5, end - 5, color, 5,
                static_cast<lv_opa_t>(72 + amount * 165));
        drawArc(layer, area.x1 + kCenter, area.y1 + kCenter, 214 - inset, start + 15, end - 15, 0xF4F1EB, 2,
                static_cast<lv_opa_t>(amount * 135));
    };
    drawButtonPulse(view->_left_button_pressed, view->_left_button_released_at, true, 0xFFD166);
    drawButtonPulse(view->_right_button_pressed, view->_right_button_released_at, false, 0x8CCBFF);
}
