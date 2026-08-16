/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "arc_top_clock.h"
#include <assets/assets.h>
#include <mooncake_log.h>
#include <hal/hal.h>
#include <wifi_manager.h>
#include <string>
#include <ctime>
#include <cmath>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace view;

// All top indicators share the same circular geometry, based on the centre
// of the 466 x 466 AMOLED. Angles start at 12 o'clock and increase clockwise.
static constexpr float _screen_center       = 233.0f;
static constexpr float _clock_radius        = 200.0f;
// Balance the actual widths of MM/DD, HH:MM and the Wi-Fi glyph as one
// header group. The time is intentionally offset a little right so the
// complete three-part composition is centred at 12 o'clock.
static constexpr float _date_start_angle    = -34.0f;
static constexpr float _clock_start_angle   = -3.0f;
static constexpr float _clock_angle_step    = 5.0f;
static constexpr float _wifi_angle          = 31.5f;
// The connection point is 10 px inward from the canvas pivot. Keeping the
// pivot at 210 therefore puts that bottom point on the time's 200 px arc.
static constexpr float _wifi_radius         = 210.0f;
static constexpr int _wifi_icon_width       = 42;
static constexpr int _wifi_icon_height      = 38;

static float degrees_to_radians(float degrees)
{
    return degrees * static_cast<float>(M_PI) / 180.0f;
}

void ArcTopClock::init()
{
    // This is only a lifecycle owner. The clock glyphs themselves live on
    // the top layer so they can be positioned directly on the circular edge.
    setSize(1, 1);
    setBorderWidth(0);
    setOutlineWidth(0);
    setPaddingAll(0);
    setBgOpa(LV_OPA_TRANSP);
    removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    auto init_labels = [this](auto& group) {
        for (auto& label : group) {
            label = std::make_unique<Label>(lv_layer_top());
            label->setText(".");
            label->setTextFont(&lv_font_maple_mono_medium_28);
            label->setTextColor(lv_color_hex(color));
            label->setBgOpa(LV_OPA_TRANSP);
            label->setBorderWidth(0);
        }
    };
    init_labels(date_labels);
    init_labels(labels);

    // Draw the complete indicator in one canvas. This avoids the unreliable
    // layout of tiny nested LVGL objects on this display.
    wifi_icon = std::make_unique<Canvas>(lv_layer_top());
    wifi_icon->setSize(_wifi_icon_width, _wifi_icon_height);
    wifi_icon->setBgOpa(LV_OPA_TRANSP);
    wifi_icon->setBorderWidth(0);
    wifi_icon->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    wifi_icon->addFlag(LV_OBJ_FLAG_FLOATING);
    wifi_icon->createBuffer(_wifi_icon_width, _wifi_icon_height, LV_COLOR_FORMAT_ARGB8888);

    const float wifi_radians = degrees_to_radians(_wifi_angle);
    const int wifi_center_x  = std::lround(_screen_center + _wifi_radius * std::sin(wifi_radians));
    const int wifi_center_y  = std::lround(_screen_center - _wifi_radius * std::cos(wifi_radians));
    wifi_icon->setPos(wifi_center_x - _wifi_icon_width / 2, wifi_center_y - _wifi_icon_height / 2);
    wifi_icon->setTransformPivot(_wifi_icon_width / 2, _wifi_icon_height / 2);
    wifi_icon->setRotation(std::lround(_wifi_angle * 10.0f));

    update(true);
}

void ArcTopClock::update(bool force)
{
    if (force || GetHAL().millis() - update_time_count > updateInterval) {
        std::time_t now    = std::time(nullptr);
        std::tm* localTime = std::localtime(&now);
        set_date_to(fmt::format("{:02d}/{:02d}", localTime->tm_mon + 1, localTime->tm_mday));
        set_clock_to(fmt::format("{:02d}:{:02d}", localTime->tm_hour, localTime->tm_min));
        // Text and font changes invalidate LVGL's content-sized labels. Flush
        // that deferred layout before reading their dimensions, otherwise the
        // first frame after returning to Launcher uses the old placeholder
        // size and visibly jumps on the next one-second update.
        lv_obj_update_layout(lv_layer_top());
        layout_date();
        layout_clock();
        update_wifi_indicator();
        // set_clock_to("00:00");
        update_time_count = GetHAL().millis();
    }
}

void ArcTopClock::update_wifi_indicator()
{
    auto& wifi = WifiManager::GetInstance();

    int level  = 0;
    if (wifi.IsConnected()) {
        const int rssi = wifi.GetRssi();
        level          = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
    }

    draw_wifi_indicator(level);
}

void ArcTopClock::draw_wifi_indicator(int level)
{
    // Standard Wi-Fi glyph: a connection point and three nested arches.
    // RSSI controls the number of white arches; inactive arches remain dim.
    wifi_icon->fillBg(lv_color_black(), LV_OPA_TRANSP);
    wifi_icon->startDrawing();

    constexpr int center_x = _wifi_icon_width / 2;
    constexpr int center_y = 29;
    // Keep a clear gap between the connection dot and the inner arch while
    // matching the thin stroke weight of the clock text.
    constexpr int radii[]  = {10, 15, 20};
    constexpr int widths[] = {3, 3, 3};

    for (int i = 0; i < 3; ++i) {
        const bool active      = level >= (i + 2);
        const auto arc_color   = lv_color_hex(active ? 0xFFFFFF : 0x5D6875);
        const auto arc_opacity = active ? LV_OPA_COVER : LV_OPA_50;
        // 225° to 315° traces a top-facing arch in LVGL's clockwise system.
        wifi_icon->drawArc(center_x, center_y, radii[i], 225, 315, arc_color, arc_opacity, widths[i], true);
    }

    const auto dot_color = lv_color_hex(level > 0 ? 0xFFFFFF : 0x5D6875);
    wifi_icon->drawCircle(center_x, center_y, 3, dot_color, level > 0 ? LV_OPA_COVER : LV_OPA_50);

    wifi_icon->finishDrawing();
    lv_obj_invalidate(wifi_icon->get());
}

void ArcTopClock::set_clock_to(const std::string_view text)
{
    const int count = std::min((int)text.size(), (int)labels.size());
    for (int i = 0; i < count; i++) {
        if (text[i] == '0') {
            labels[i]->setText("O");
        } else {
            char buf[2] = {text[i], '\0'};
            labels[i]->setText(buf);
        }
    }
}

void ArcTopClock::set_date_to(const std::string_view text)
{
    const int count = std::min((int)text.size(), (int)date_labels.size());
    for (int i = 0; i < count; i++) {
        if (text[i] == '0') {
            date_labels[i]->setText("O");
        } else {
            char buf[2] = {text[i], '\0'};
            date_labels[i]->setText(buf);
        }
    }
}

void ArcTopClock::layout_date()
{
    for (int i = 0; i < date_labels.size(); ++i) {
        const float angle    = _date_start_angle + _clock_angle_step * i;
        const float radians  = degrees_to_radians(angle);
        const int center_x   = std::lround(_screen_center + _clock_radius * std::sin(radians));
        const int center_y   = std::lround(_screen_center - _clock_radius * std::cos(radians));
        const int label_w    = lv_obj_get_width(date_labels[i]->get());
        const int label_h    = lv_obj_get_height(date_labels[i]->get());

        date_labels[i]->setTransformPivot(label_w / 2, label_h / 2);
        date_labels[i]->setRotation(std::lround(angle * 10.0f));
        date_labels[i]->setPos(center_x - label_w / 2, center_y - label_h / 2);
    }
}

void ArcTopClock::layout_clock()
{
    for (int i = 0; i < labels.size(); ++i) {
        const float angle    = _clock_start_angle + _clock_angle_step * i;
        const float radians  = degrees_to_radians(angle);
        const int center_x   = std::lround(_screen_center + _clock_radius * std::sin(radians));
        const int center_y   = std::lround(_screen_center - _clock_radius * std::cos(radians));
        const int label_w    = lv_obj_get_width(labels[i]->get());
        const int label_h    = lv_obj_get_height(labels[i]->get());

        labels[i]->setTransformPivot(label_w / 2, label_h / 2);
        labels[i]->setRotation(std::lround(angle * 10.0f));
        labels[i]->setPos(center_x - label_w / 2, center_y - label_h / 2);
    }
}
