/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

uint32_t stateColor(ruview::SentinelState state)
{
    switch (state) {
        case ruview::SentinelState::Still: return 0x67D9B4;
        case ruview::SentinelState::Activity: return 0xFF665E;
        case ruview::SentinelState::DeviceMoving: return 0xFFC857;
        case ruview::SentinelState::Calibrating: return 0x72A7FF;
        case ruview::SentinelState::AdaptingBaseline: return 0xA78BFA;
        case ruview::SentinelState::WaitingForSignal: return 0xA78BFA;
        case ruview::SentinelState::WaitingForWifi: return 0x8E9AAF;
        case ruview::SentinelState::Error: return 0xFF665E;
    }
    return 0xFFFFFF;
}

void styleLabel(lv_obj_t* label, const lv_font_t* font, uint32_t color)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

}  // namespace

namespace view {

void RuViewView::updateAmbientVisuals(const ruview::SentinelSnapshot& snapshot, uint32_t color)
{
    ++_visual_tick;
    const bool calibrating = snapshot.state == ruview::SentinelState::Calibrating ||
                             snapshot.state == ruview::SentinelState::AdaptingBaseline;
    const float breath = (std::sin(static_cast<float>(_visual_tick) * 0.48f) + 1.0f) * 0.5f;

    int core_size = 116 + static_cast<int>(breath * 10.0f);
    if (snapshot.state == ruview::SentinelState::Activity) {
        core_size = 124 + static_cast<int>(std::clamp(snapshot.activity_score, 0.0f, 100.0f) * 0.08f);
        lv_obj_set_style_bg_color(_core, lv_color_hex(0x2A0908), 0);
    } else if (calibrating) {
        core_size = 118 + static_cast<int>(breath * 8.0f);
        lv_obj_set_style_bg_color(_core, lv_color_hex(0x08162A), 0);
    } else {
        lv_obj_set_style_bg_color(_core, lv_color_hex(0x071713), 0);
    }
    lv_obj_set_size(_core, core_size, core_size);
    lv_obj_align(_core, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_border_color(_core, lv_color_hex(color), 0);
    lv_obj_set_style_border_opa(_core, 80 + static_cast<int>(breath * 90.0f), 0);

    constexpr float kPi = 3.14159265358979323846f;
    constexpr int kCenterX = 233;
    constexpr int kCenterY = 237;
    constexpr float kDotRadius = 136.0f;
    const uint8_t scan_dot = static_cast<uint8_t>((_visual_tick / 2) % 8);
    for (uint8_t index = 0; index < 8; ++index) {
        float level = snapshot.band_activity[index];
        if (calibrating) {
            level = index == scan_dot ? 100.0f : 10.0f;
        } else if (snapshot.state == ruview::SentinelState::WaitingForWifi ||
                   snapshot.state == ruview::SentinelState::WaitingForSignal) {
            level = index == scan_dot ? 60.0f : 5.0f;
        }
        const int dot_size = 5 + static_cast<int>(std::clamp(level, 0.0f, 100.0f) * 0.07f);
        const float angle = (-90.0f + index * 45.0f) * kPi / 180.0f;
        const int x = kCenterX + static_cast<int>(std::cos(angle) * kDotRadius) - dot_size / 2;
        const int y = kCenterY + static_cast<int>(std::sin(angle) * kDotRadius) - dot_size / 2;
        lv_obj_set_size(_band_dots[index], dot_size, dot_size);
        lv_obj_set_pos(_band_dots[index], x, y);
        lv_obj_set_style_bg_color(_band_dots[index], lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(_band_dots[index], 75 + static_cast<int>(level * 1.8f), 0);
    }

    if (snapshot.state == ruview::SentinelState::Activity &&
        (_last_state != ruview::SentinelState::Activity ||
         (_pulse_step == 0 && _visual_tick % 10 == 0))) {
        _pulse_step = 1;
        lv_obj_remove_flag(_pulse, LV_OBJ_FLAG_HIDDEN);
    }
    if (_pulse_step > 0) {
        const int pulse_size = 268 + _pulse_step * 22;
        const int pulse_opa = std::max(0, 190 - _pulse_step * 34);
        lv_obj_set_size(_pulse, pulse_size, pulse_size);
        lv_obj_align(_pulse, LV_ALIGN_CENTER, 0, 4);
        lv_obj_set_style_border_color(_pulse, lv_color_hex(color), 0);
        lv_obj_set_style_border_opa(_pulse, pulse_opa, 0);
        if (++_pulse_step >= 6) {
            _pulse_step = 0;
            lv_obj_add_flag(_pulse, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const int base_arc_opa = snapshot.state == ruview::SentinelState::Still
                                 ? 70 + static_cast<int>(breath * 45.0f)
                                 : 90;
    lv_obj_set_style_arc_opa(_meter, base_arc_opa, LV_PART_MAIN);
}

RuViewView::~RuViewView()
{
    if (_panel) {
        lv_obj_delete(_panel);
        _panel = nullptr;
    }
}

void RuViewView::init(lv_obj_t* parent)
{
    _panel = lv_obj_create(parent);
    lv_obj_set_size(_panel, 466, 466);
    lv_obj_center(_panel);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_panel, 0, 0);
    lv_obj_set_style_pad_all(_panel, 0, 0);
    lv_obj_remove_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);

    _pulse = lv_obj_create(_panel);
    lv_obj_set_size(_pulse, 268, 268);
    lv_obj_align(_pulse, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_bg_opa(_pulse, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_pulse, 3, 0);
    lv_obj_set_style_border_opa(_pulse, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(_pulse, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(_pulse, 0, 0);
    lv_obj_remove_flag(_pulse, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_pulse, LV_OBJ_FLAG_HIDDEN);

    _eyebrow = lv_label_create(_panel);
    lv_label_set_text(_eyebrow, "RUVIEW  /  SENTINEL");
    styleLabel(_eyebrow, &lv_font_montserrat_16, 0x7F8DA8);
    lv_obj_align(_eyebrow, LV_ALIGN_TOP_MID, 0, 48);

    _meter = lv_arc_create(_panel);
    lv_obj_set_size(_meter, 310, 310);
    lv_obj_align(_meter, LV_ALIGN_CENTER, 0, 4);
    lv_arc_set_rotation(_meter, 135);
    lv_arc_set_bg_angles(_meter, 0, 270);
    lv_arc_set_range(_meter, 0, 100);
    lv_arc_set_value(_meter, 0);
    lv_obj_remove_style(_meter, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_width(_meter, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(0x1B2230), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_meter, 12, LV_PART_INDICATOR);

    _core = lv_obj_create(_panel);
    lv_obj_set_size(_core, 120, 120);
    lv_obj_align(_core, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(_core, lv_color_hex(0x071713), 0);
    lv_obj_set_style_bg_opa(_core, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_core, 2, 0);
    lv_obj_set_style_border_color(_core, lv_color_hex(0x67D9B4), 0);
    lv_obj_set_style_radius(_core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(_core, 0, 0);
    lv_obj_remove_flag(_core, LV_OBJ_FLAG_SCROLLABLE);

    for (auto& dot : _band_dots) {
        dot = lv_obj_create(_panel);
        lv_obj_set_size(dot, 5, 5);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x67D9B4), 0);
        lv_obj_set_style_bg_opa(dot, 90, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }

    _score = lv_label_create(_panel);
    lv_label_set_text(_score, "--");
    styleLabel(_score, &lv_font_montserrat_36, 0xFFFFFF);
    lv_obj_align(_score, LV_ALIGN_CENTER, 0, -40);

    _state = lv_label_create(_panel);
    lv_label_set_text(_state, "STARTING");
    styleLabel(_state, &lv_font_montserrat_22, 0xFFFFFF);
    lv_obj_set_width(_state, 330);
    lv_obj_align(_state, LV_ALIGN_CENTER, 0, 12);

    _detail = lv_label_create(_panel);
    lv_label_set_text(_detail, "Preparing Wi-Fi CSI");
    styleLabel(_detail, &lv_font_montserrat_14, 0x9AA6BB);
    lv_obj_set_width(_detail, 252);
    lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
    lv_obj_align(_detail, LV_ALIGN_CENTER, 0, 55);

    _stats = lv_label_create(_panel);
    lv_label_set_text(_stats, "0 fps   RSSI --   0 frames");
    styleLabel(_stats, &lv_font_montserrat_14, 0x69758A);
    lv_obj_set_width(_stats, 370);
    lv_obj_align(_stats, LV_ALIGN_BOTTOM_MID, 0, -64);

    _hint = lv_label_create(_panel);
    lv_label_set_text(_hint, "A RECAL   B SENS   A+B EXIT");
    styleLabel(_hint, &lv_font_montserrat_14, 0x7F8DA8);
    lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -34);
}

void RuViewView::update(const ruview::SentinelSnapshot& snapshot, const char* error_message)
{
    if (!_panel) return;

    const uint32_t color = stateColor(snapshot.state);
    const bool calibrating = snapshot.state == ruview::SentinelState::Calibrating ||
                             snapshot.state == ruview::SentinelState::AdaptingBaseline;
    const int meter_value = calibrating
                                ? static_cast<int>(snapshot.calibration_progress * 100.0f)
                                : static_cast<int>(std::clamp(snapshot.activity_score, 0.0f, 100.0f));
    lv_arc_set_value(_meter, meter_value);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(_state, lv_color_hex(color), 0);
    updateAmbientVisuals(snapshot, color);

    char score[16];
    if (snapshot.state == ruview::SentinelState::WaitingForWifi ||
        snapshot.state == ruview::SentinelState::WaitingForSignal ||
        snapshot.state == ruview::SentinelState::Error) {
        std::snprintf(score, sizeof(score), "--");
    } else if (calibrating) {
        std::snprintf(score, sizeof(score), "%d%%", meter_value);
    } else if (snapshot.state == ruview::SentinelState::DeviceMoving) {
        std::snprintf(score, sizeof(score), "HOLD");
    } else {
        std::snprintf(score, sizeof(score), "%d", meter_value);
    }
    lv_label_set_text(_score, score);
    lv_label_set_text(_state, ruview::sentinelStateTitle(snapshot.state));
    char detail[96];
    if (error_message) {
        std::snprintf(detail, sizeof(detail), "%s", error_message);
    } else if (snapshot.activity_event_active) {
        std::snprintf(detail, sizeof(detail), "Peak %.0f   %.1fs   #%lu", snapshot.activity_peak_score,
                      snapshot.activity_duration_ms / 1000.0f,
                      static_cast<unsigned long>(snapshot.activity_event_count));
    } else if (snapshot.state == ruview::SentinelState::Still && snapshot.has_activity_event) {
        std::snprintf(detail, sizeof(detail), "Last %lus ago   peak %.0f   #%lu",
                      static_cast<unsigned long>(snapshot.last_activity_age_ms / 1000),
                      snapshot.activity_peak_score,
                      static_cast<unsigned long>(snapshot.activity_event_count));
    } else {
        std::snprintf(detail, sizeof(detail), "%s", ruview::sentinelStateDetail(snapshot.state));
    }
    lv_label_set_text(_detail, detail);

    char stats[96];
    std::snprintf(stats, sizeof(stats), "%.0f fps   RSSI %d   %s   T%.0f", snapshot.frame_rate_hz,
                  snapshot.rssi_dbm, ruview::sensitivityName(snapshot.sensitivity),
                  snapshot.trigger_score);
    lv_label_set_text(_stats, stats);

    if (_last_state != snapshot.state && snapshot.state == ruview::SentinelState::Activity) {
        lv_obj_set_style_bg_color(_panel, lv_color_hex(0x150807), 0);
    } else if (snapshot.state != ruview::SentinelState::Activity) {
        lv_obj_set_style_bg_color(_panel, lv_color_hex(0x05070B), 0);
    }
    _last_state = snapshot.state;
}

}  // namespace view
