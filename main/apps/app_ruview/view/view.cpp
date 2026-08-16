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

    int core_width = 184 + static_cast<int>(breath * 12.0f);
    int core_height = 132 + static_cast<int>(breath * 8.0f);
    const int core_x = static_cast<int>(std::sin(_visual_tick * 0.21f) * 5.0f);
    const int core_y = -20 + static_cast<int>(std::sin(_visual_tick * 0.27f + 1.2f) * 4.0f);
    if (snapshot.state == ruview::SentinelState::Activity) {
        const float bounce = std::fabs(std::sin(_visual_tick * 0.92f));
        core_width = 198 + static_cast<int>(bounce * 18.0f);
        core_height = 140 - static_cast<int>(bounce * 8.0f);
        lv_obj_set_style_bg_color(_core, lv_color_hex(0x351329), 0);
    } else if (calibrating) {
        core_width = 188 + static_cast<int>(breath * 10.0f);
        core_height = 134 + static_cast<int>(breath * 6.0f);
        lv_obj_set_style_bg_color(_core, lv_color_hex(0x151B38), 0);
    } else {
        lv_obj_set_style_bg_color(_core, lv_color_hex(0x122B2A), 0);
    }
    lv_obj_set_size(_core, core_width, core_height);
    lv_obj_align(_core, LV_ALIGN_CENTER, core_x, core_y);
    lv_obj_set_style_bg_opa(_core, 172 + static_cast<int>(breath * 34.0f), 0);
    const int text_drift_y = core_y + 20;
    lv_obj_align(_score, LV_ALIGN_CENTER, core_x, -40 + text_drift_y);
    lv_obj_align(_state, LV_ALIGN_CENTER, core_x, 12 + text_drift_y);

    static constexpr uint32_t kBubbleColors[8] = {
        0x6377DC, 0xD362C5, 0x72D2C5, 0xF0A778,
        0xC599FF, 0x80B9E7, 0xFF8292, 0xA7E577,
    };
    static constexpr int kBubbleX[8] = {80, 145, 302, 376, 92, 365, 154, 308};
    static constexpr int kBubbleY[8] = {145, 91, 94, 151, 294, 292, 349, 346};
    static constexpr int kBubbleBase[8] = {27, 18, 25, 20, 23, 29, 18, 24};
    static constexpr float kBubblePhase[8] = {0.1f, 1.7f, 3.0f, 4.4f, 5.8f, 2.3f, 3.8f, 0.9f};
    const uint8_t scan_bubble = static_cast<uint8_t>((_visual_tick / 2) % 8);
    for (uint8_t index = 0; index < 8; ++index) {
        float level = snapshot.band_activity[index];
        if (calibrating) {
            level = index == scan_bubble ? 100.0f : 14.0f;
        } else if (snapshot.state == ruview::SentinelState::WaitingForWifi ||
                   snapshot.state == ruview::SentinelState::WaitingForSignal) {
            level = index == scan_bubble ? 62.0f : 8.0f;
        }
        const float phase = kBubblePhase[index];
        const float drift_x = std::sin(_visual_tick * (0.16f + index * 0.007f) + phase) *
                              (7.0f + index % 3 * 2.0f);
        const float drift_y = std::cos(_visual_tick * (0.13f + index * 0.009f) + phase * 1.4f) *
                              (6.0f + (index + 1) % 3 * 2.0f);
        const float bounce = snapshot.state == ruview::SentinelState::Activity
                                 ? std::fabs(std::sin(_visual_tick * 0.78f + phase)) * 12.0f
                                 : 0.0f;
        const int bubble_width = kBubbleBase[index] +
                                 static_cast<int>(std::clamp(level, 0.0f, 100.0f) * 0.15f + bounce);
        const float aspect = 0.76f + (index % 3) * 0.14f;
        const int bubble_height = std::max(12, static_cast<int>(bubble_width * aspect));
        const float push = snapshot.state == ruview::SentinelState::Activity ? 1.12f : 1.0f;
        const int x = 233 + static_cast<int>((kBubbleX[index] - 233) * push + drift_x) - bubble_width / 2;
        const int y = 237 + static_cast<int>((kBubbleY[index] - 237) * push + drift_y) - bubble_height / 2;
        lv_obj_set_size(_band_bubbles[index], bubble_width, bubble_height);
        lv_obj_set_pos(_band_bubbles[index], x, y);
        lv_obj_set_style_bg_color(_band_bubbles[index], lv_color_hex(kBubbleColors[index]), 0);
        lv_obj_set_style_bg_opa(_band_bubbles[index],
                                68 + static_cast<int>(std::clamp(level, 0.0f, 100.0f) * 1.55f), 0);
    }

    if (snapshot.state == ruview::SentinelState::Activity &&
        (_last_state != ruview::SentinelState::Activity ||
         (_pulse_step == 0 && _visual_tick % 10 == 0))) {
        _pulse_step = 1;
        lv_obj_remove_flag(_pulse, LV_OBJ_FLAG_HIDDEN);
    }
    if (_pulse_step > 0) {
        const int pulse_size = 118 + _pulse_step * 28;
        const int pulse_opa = std::max(0, 145 - _pulse_step * 25);
        lv_obj_set_size(_pulse, pulse_size, pulse_size);
        lv_obj_align(_pulse, LV_ALIGN_CENTER, core_x, core_y);
        lv_obj_set_style_bg_color(_pulse, lv_color_hex(0xD362C5), 0);
        lv_obj_set_style_bg_opa(_pulse, pulse_opa, 0);
        if (++_pulse_step >= 6) {
            _pulse_step = 0;
            lv_obj_add_flag(_pulse, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const int base_arc_opa = snapshot.state == ruview::SentinelState::Still
                                 ? 25 + static_cast<int>(breath * 18.0f)
                                 : 34;
    lv_obj_set_style_arc_opa(_meter, base_arc_opa, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(_meter, snapshot.state == ruview::SentinelState::Activity ? 205 : 145,
                             LV_PART_INDICATOR);
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
    lv_obj_set_size(_pulse, 118, 118);
    lv_obj_align(_pulse, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_opa(_pulse, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_pulse, 0, 0);
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
    lv_obj_set_style_arc_width(_meter, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(0x343153), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_meter, 6, LV_PART_INDICATOR);

    _core = lv_obj_create(_panel);
    lv_obj_set_size(_core, 190, 136);
    lv_obj_align(_core, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(_core, lv_color_hex(0x122B2A), 0);
    lv_obj_set_style_bg_opa(_core, 185, 0);
    lv_obj_set_style_border_width(_core, 0, 0);
    lv_obj_set_style_radius(_core, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(_core, 0, 0);
    lv_obj_remove_flag(_core, LV_OBJ_FLAG_SCROLLABLE);

    for (auto& bubble : _band_bubbles) {
        bubble = lv_obj_create(_panel);
        lv_obj_set_size(bubble, 20, 16);
        lv_obj_set_style_bg_color(bubble, lv_color_hex(0x72D2C5), 0);
        lv_obj_set_style_bg_opa(bubble, 90, 0);
        lv_obj_set_style_border_width(bubble, 0, 0);
        lv_obj_set_style_radius(bubble, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(bubble, 0, 0);
        lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);
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
