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
constexpr int kStageSize = 420;
constexpr int kCenter = kStageSize / 2;
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
    _stage->align(LV_ALIGN_CENTER, 0, 18);
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
    _state_label->align(LV_ALIGN_TOP_MID, 0, 18);
    _state_label->setTextFont(&lv_font_maple_mono_medium_24);
    _state_label->setTextColor(lv_color_hex(0xF2F2F2));

    _hint_label = std::make_unique<Label>(_panel->get());
    _hint_label->align(LV_ALIGN_BOTTOM_MID, 0, -14);
    _hint_label->setTextFont(&lv_font_montserrat_16);
    _hint_label->setTextColor(lv_color_hex(0x7E7E7E));
    _hint_label->setText("A/B switch  ·  hold for FX");

    _state_started_at = lv_tick_get();
    updateLabels();
}

void GrokBotLabView::update(uint32_t now)
{
    if (_auto_return_to_idle && !_touching && now - _state_started_at > 2400) {
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
    _state = state;
    _state_started_at = lv_tick_get();
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
    int bodyW = 208;
    int bodyH = 192;
    int bodyX = area.x1 + kCenter - bodyW / 2;
    int bodyY = area.y1 + kCenter - bodyH / 2 + static_cast<int>(breath * 5.0f);
    int eyeY = bodyY + 103;
    int eyeGap = 58;
    int eyeW = 23;
    int eyeH = static_cast<int>(42 * blink);
    int offsetX = view->_gaze_x;
    int offsetY = view->_gaze_y;

    if (view->_state == State::Listening) {
        bodyY -= 3;
        offsetY -= 8;
        eyeGap -= 8;
    } else if (view->_state == State::Working) {
        bodyW -= 8;
        bodyH += 6;
        eyeY += 5;
        eyeGap -= 9;
        offsetY += 4;
    } else if (view->_state == State::Sleeping) {
        bodyY += static_cast<int>(breath * 7.0f) + 5;
        eyeY += 8;
        eyeH = 7;
        eyeW = 28;
    } else if (view->_state == State::Surprised) {
        bodyY -= 7;
        bodyW += 8;
        eyeY -= 9;
        eyeW += 5;
        eyeH += 14;
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 125 + static_cast<int>(std::sin(seconds * 7.0f) * 5.0f),
                 0xF4F1EB, 2, 115);
    } else if (view->_state == State::Celebrate) {
        bodyY += static_cast<int>(std::fabs(std::sin(seconds * 5.0f)) * 15.0f) - 5;
        eyeY -= 4;
        eyeGap += 8;
    } else if (view->_state == State::Progress) {
        bodyW -= 14;
        bodyH -= 8;
        bodyX += 7;
        bodyY += 5;
        eyeY += 2;
        eyeGap -= 10;
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
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 132 + static_cast<int>(std::sin(seconds * 5.0f) * 8.0f),
                 0xFF4E5C, 4, LV_OPA_70);
    } else if (view->_state == State::Thinking) {
        for (int i = 0; i < 3; ++i) {
            const float phase = seconds * 2.8f + i * 0.72f;
            const int dotX = area.x1 + kCenter + 112 + static_cast<int>(std::cos(phase) * 14.0f);
            const int dotY = area.y1 + kCenter - 52 + i * 26;
            drawRounded(layer, dotX - 5, dotY - 5, 10, 10, 5, 0xA9D6FF,
                        static_cast<lv_opa_t>(90 + 150 * (0.5f + 0.5f * std::sin(phase))));
        }
    } else if (view->_state == State::Working) {
        for (int i = 0; i < 4; ++i) {
            const float phase = seconds * 4.0f + i * 1.22f;
            const int x = area.x1 + kCenter - 54 + i * 36;
            const int y = area.y1 + kCenter + 128 + static_cast<int>(std::sin(phase) * 6.0f);
            drawRounded(layer, x - 4, y - 4, 8, 8, 4, 0xF4F1EB,
                        static_cast<lv_opa_t>(75 + 115 * (0.5f + 0.5f * std::sin(phase))));
        }
    } else if (view->_state == State::Sleeping) {
        for (int i = 0; i < 2; ++i) {
            const float phase = seconds * 1.2f + i * 1.6f;
            const int x = area.x1 + kCenter + 105 + i * 22;
            const int y = area.y1 + kCenter - 55 - i * 25 + static_cast<int>(std::sin(phase) * 5.0f);
            drawRounded(layer, x - 5, y - 5, 10 + i * 3, 10 + i * 3, 5, 0xA9D6FF, 166);
        }
    } else if (view->_state == State::Celebrate) {
        static constexpr uint32_t colors[] = {0xF4F1EB, 0xA9D6FF, 0xFFCC66, 0xFF8292};
        for (int i = 0; i < 18; ++i) {
            const float phase = seconds * (1.7f + (i % 3) * 0.22f) + i * 0.81f;
            const int radius = 130 + (i % 5) * 13;
            const int x = area.x1 + kCenter + static_cast<int>(std::cos(phase) * radius);
            const int y = area.y1 + kCenter + static_cast<int>(std::sin(phase * 1.31f) * (78 + (i % 4) * 12));
            const int size = 5 + (i % 3) * 2;
            drawRounded(layer, x - size / 2, y - size / 2, size, size, size / 2, colors[i % 4], 210);
        }
    } else if (view->_state == State::Progress) {
        const int progress = static_cast<int>(std::fmod(seconds * 0.18f, 1.0f) * 360.0f);
        drawRing(layer, area.x1 + kCenter, area.y1 + kCenter, 138, 0x2F2F2F, 7, LV_OPA_COVER);
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color = lv_color_hex(0xF4F1EB);
        arc.width = 7;
        arc.opa = LV_OPA_COVER;
        arc.center.x = area.x1 + kCenter;
        arc.center.y = area.y1 + kCenter;
        arc.radius = 138;
        arc.start_angle = 270;
        arc.end_angle = 270 + progress;
        arc.rounded = 1;
        lv_draw_arc(layer, &arc);
    }

    // Inverted icon palette: near-black body with warm-white eyes.
    // Separate dark values retain the blob silhouette on the black AMOLED stage.
    drawRounded(layer, bodyX, bodyY, bodyW, bodyH, 94, 0x242424);
    drawRounded(layer, bodyX + 12, bodyY + 12, bodyW - 24, bodyH / 2, 82, 0x343434, 191);
    drawRounded(layer, bodyX + 10, bodyY + bodyH / 2, bodyW - 20, bodyH / 2 - 10, 76, 0x171717, 214);
    if (view->_state == State::Curious) {
        offsetX += 8;
        offsetY -= 3;
    }
    drawRounded(layer, bodyX + bodyW / 2 - eyeGap / 2 - eyeW / 2 + offsetX, eyeY + offsetY, eyeW, std::max(7, eyeH), eyeW / 2, 0xF4F1EB);
    drawRounded(layer, bodyX + bodyW / 2 + eyeGap / 2 - eyeW / 2 + offsetX, eyeY + offsetY, eyeW, std::max(7, eyeH), eyeW / 2, 0xF4F1EB);

    if (view->_state == State::Happy || view->_state == State::Playful) {
        drawRounded(layer, bodyX + bodyW / 2 - 24, bodyY + 145, 48, 9, 5, 0xF4F1EB, 199);
    }
}
