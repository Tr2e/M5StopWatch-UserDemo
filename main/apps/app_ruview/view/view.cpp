/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <algorithm>
#include <cstdio>

namespace {

uint32_t stateColor(ruview::SentinelState state)
{
    switch (state) {
        case ruview::SentinelState::Still: return 0x67D9B4;
        case ruview::SentinelState::Activity: return 0xFF665E;
        case ruview::SentinelState::DeviceMoving: return 0xFFC857;
        case ruview::SentinelState::Calibrating: return 0x72A7FF;
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
    const bool calibrating = snapshot.state == ruview::SentinelState::Calibrating;
    const int meter_value = calibrating
                                ? static_cast<int>(snapshot.calibration_progress * 100.0f)
                                : static_cast<int>(std::clamp(snapshot.activity_score, 0.0f, 100.0f));
    lv_arc_set_value(_meter, meter_value);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(_state, lv_color_hex(color), 0);

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
