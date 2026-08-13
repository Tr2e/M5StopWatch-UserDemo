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

const char* stateName(GrokBotLabView::State state)
{
    static constexpr const char* names[] = {
        "IDLE", "CURIOUS", "LISTENING", "THINKING", "WORKING",
        "HAPPY", "PLAYFUL", "SURPRISED", "SLEEPING", "ALERT", "CELEBRATE", "PROGRESS",
    };
    return names[static_cast<uint8_t>(state)];
}
}  // namespace

void GrokBotLabView::init(lv_obj_t* parent)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(kPanelSize, kPanelSize);
    _panel->setBgColor(lv_color_hex(0x050505));
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
    _state_label->setTextColor(lv_color_hex(0xF2F2F2));
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
    }

    // Inverted icon palette: near-black body with warm-white eyes.
    // Separate dark values retain the blob silhouette on the black AMOLED stage.
    const int bodyRadius = std::min(bodyW, bodyH) / 2;
    drawRounded(layer, bodyX, bodyY, bodyW, bodyH, bodyRadius, 0x242424);
    drawRounded(layer, bodyX + 16, bodyY + 16, bodyW - 32, bodyH / 2, std::max(12, bodyRadius - 16), 0x343434, 191);
    drawRounded(layer, bodyX + 14, bodyY + bodyH / 2, bodyW - 28, bodyH / 2 - 14, std::max(12, bodyRadius - 22), 0x171717, 214);
    if (view->_state == State::Curious) {
        offsetX += 8;
        offsetY -= 3;
    }
    drawRounded(layer, bodyX + bodyW / 2 - eyeGap / 2 - eyeW / 2 + offsetX, eyeY + offsetY, eyeW, std::max(7, eyeH), eyeW / 2, 0xF4F1EB);
    drawRounded(layer, bodyX + bodyW / 2 + eyeGap / 2 - eyeW / 2 + offsetX, eyeY + offsetY, eyeW, std::max(7, eyeH), eyeW / 2, 0xF4F1EB);

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
