/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"

#include <algorithm>
#include <assets/assets.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kCanvas = 466;
constexpr int kCenter = kCanvas / 2;
constexpr int kContentLeft = 48;
constexpr int kScoreLeft = kContentLeft - 10;
constexpr int kMetaY = 122;
constexpr int kScoreY = 164;
constexpr int kDetailY = 294;
constexpr int kDetailWidth = 248;
constexpr int kUnitGap = 6;
constexpr uint32_t kBg = 0x050505;
constexpr uint32_t kText = 0xF5F5F5;
constexpr uint32_t kMuted = 0x8E8E93;
constexpr uint32_t kFaint = 0x5C5C5C;
constexpr uint32_t kDot = 0xC8C8C8;
constexpr uint32_t kRim = 0x2A2A2A;
constexpr uint32_t kRimFill = 0xE8E8E8;
constexpr uint32_t kActive = 0xFF5C2A;
constexpr uint32_t kActiveText = 0xFF6A33;

const char* shortStateLabel(ruview::SentinelState state)
{
    switch (state) {
        case ruview::SentinelState::WaitingForWifi: return "NO WIFI";
        case ruview::SentinelState::WaitingForSignal: return "NO SIGNAL";
        case ruview::SentinelState::Calibrating: return "CALIBRATING";
        case ruview::SentinelState::AdaptingBaseline: return "ADAPTING";
        case ruview::SentinelState::Still: return "STILL";
        case ruview::SentinelState::Activity: return "ACTIVITY";
        case ruview::SentinelState::DeviceMoving: return "MOVING";
        case ruview::SentinelState::Error: return "ERROR";
    }
    return "RUVIEW";
}

bool isNumericScoreState(ruview::SentinelState state)
{
    return state == ruview::SentinelState::Calibrating ||
           state == ruview::SentinelState::AdaptingBaseline ||
           state == ruview::SentinelState::Still ||
           state == ruview::SentinelState::Activity;
}

bool isCalibratingState(ruview::SentinelState state)
{
    return state == ruview::SentinelState::Calibrating ||
           state == ruview::SentinelState::AdaptingBaseline;
}

void styleLabel(lv_obj_t* label, const lv_font_t* font, uint32_t color, lv_text_align_t align)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_outline_width(label, 0, 0);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_SCROLLABLE);
}

void pinLeftColumn(lv_obj_t* label, int y)
{
    lv_obj_set_pos(label, kContentLeft, y);
    lv_obj_set_width(label, kDetailWidth);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

void setLabelIfChanged(lv_obj_t* label, const char* text)
{
    const char* current = lv_label_get_text(label);
    if (current && std::strcmp(current, text) == 0) return;
    lv_label_set_text(label, text);
}

lv_obj_t* makeDot(lv_obj_t* parent)
{
    lv_obj_t* dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_bg_color(dot, lv_color_hex(kDot), 0);
    lv_obj_set_style_bg_opa(dot, 40, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    return dot;
}

}  // namespace

namespace view {

void RuViewView::animationTimerCallback(lv_timer_t* timer)
{
    auto* view = static_cast<RuViewView*>(lv_timer_get_user_data(timer));
    if (view) view->updateAmbientVisuals();
}

void RuViewView::setScore(const char* text, bool large_numeric, bool show_unit)
{
    const lv_font_t* font = large_numeric ? &CommissionerMedium108 : &lv_font_maple_mono_medium_48;
    const lv_font_t* unit_font = &lv_font_maple_mono_medium_48;
    const int digit_w = lv_font_get_glyph_width(font, '0', 0);
    const int slot_w = digit_w * 2;
    const int score_x = kScoreLeft;
    const int unit_x = score_x + slot_w + kUnitGap;
    const char* current = lv_label_get_text(_score);
    const bool same_text = current && std::strcmp(current, text) == 0;

    lv_obj_set_style_text_font(_score, font, 0);
    lv_obj_set_style_text_align(_score, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_size(_score, slot_w, font->line_height);
    lv_obj_set_pos(_score, score_x, kScoreY);
    if (!same_text) {
        lv_label_set_text(_score, text);
    }

    if (show_unit) {
        lv_obj_set_style_text_font(_unit, unit_font, 0);
        const int score_baseline = kScoreY + font->line_height - font->base_line;
        const int unit_y = score_baseline - (unit_font->line_height - unit_font->base_line);
        lv_obj_set_pos(_unit, unit_x, unit_y);
        lv_obj_remove_flag(_unit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(_alert, _unit, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    } else {
        lv_obj_add_flag(_unit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(_alert, _score, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    }
}

void RuViewView::updateAmbientVisuals()
{
    if (!_panel || !_has_visual_snapshot) return;

    const auto& snapshot = _visual_snapshot;
    ++_visual_tick;
    const float breath = (std::sin(static_cast<float>(_visual_tick) * 0.035f) + 1.0f) * 0.5f;
    const bool calibrating = isCalibratingState(snapshot.state);
    const bool active = snapshot.state == ruview::SentinelState::Activity;

    lv_obj_set_style_bg_color(_panel, lv_color_hex(kBg), 0);
    lv_obj_set_style_arc_width(_meter, active ? 4 : 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(active ? kActive : kRimFill), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(_meter, 40 + static_cast<int>(breath * 14.0f), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(_meter, active ? 200 : (calibrating ? 120 : 88 + static_cast<int>(breath * 16.0f)),
                             LV_PART_INDICATOR);

    if (!lv_obj_has_flag(_alert, LV_OBJ_FLAG_HIDDEN)) {
        const int alert_opa = active ? 150 + static_cast<int>(breath * 90.0f) : 180;
        lv_obj_set_style_bg_opa(_alert, alert_opa, 0);
        lv_obj_set_style_bg_color(_alert, lv_color_hex(active ? kActive : kText), 0);
    }

    const float chase_head = std::fmod(static_cast<float>(_visual_tick) * 0.20f, 8.0f);
    for (uint8_t index = 0; index < 8; ++index) {
        int size = 9;
        int opa = 32;
        uint32_t color = kDot;

        if (active) {
            float trail = chase_head - static_cast<float>(index);
            if (trail < 0.0f) trail += 8.0f;
            const float glow = std::max(0.0f, 1.0f - trail / 2.6f);
            const float head = glow * glow;
            size = 8 + static_cast<int>(head * 13.0f);
            opa = 36 + static_cast<int>(head * 210.0f);
            color = kActive;
        } else {
            float band = snapshot.band_activity[index];
            if (calibrating) {
                const float wave = (std::sin(_visual_tick * 0.045f + index * 0.7f) + 1.0f) * 0.5f;
                band = 10.0f + wave * 22.0f;
            } else if (!snapshot.calibrated) {
                band = 0.0f;
            }
            const float clamped = std::clamp(band, 0.0f, 100.0f);
            size = 9 + static_cast<int>(clamped / 22.0f);
            opa = 32 + static_cast<int>(clamped * 1.5f);
        }

        lv_obj_set_size(_band_dots[index], size, size);
        lv_obj_set_pos(_band_dots[index], _dot_x[index] - size / 2, _dot_y[index] - size / 2);
        lv_obj_set_style_bg_color(_band_dots[index], lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(_band_dots[index], std::min(opa, 240), 0);
    }
}

RuViewView::~RuViewView()
{
    if (_animation_timer) {
        lv_timer_delete(_animation_timer);
        _animation_timer = nullptr;
    }
    if (_panel) {
        lv_obj_delete(_panel);
        _panel = nullptr;
    }
}

void RuViewView::init(lv_obj_t* parent)
{
    _panel = lv_obj_create(parent);
    lv_obj_set_size(_panel, kCanvas, kCanvas);
    lv_obj_center(_panel);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_panel, 0, 0);
    lv_obj_set_style_radius(_panel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(_panel, 0, 0);
    lv_obj_remove_flag(_panel, LV_OBJ_FLAG_SCROLLABLE);

    _meter = lv_arc_create(_panel);
    lv_obj_set_size(_meter, 450, 450);
    lv_obj_center(_meter);
    lv_arc_set_rotation(_meter, 270);
    lv_arc_set_bg_angles(_meter, 0, 360);
    lv_arc_set_range(_meter, 0, 100);
    lv_arc_set_value(_meter, 0);
    lv_obj_remove_style(_meter, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(_meter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(_meter, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_meter, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(kRim), LV_PART_MAIN);
    lv_obj_set_style_arc_color(_meter, lv_color_hex(kRimFill), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(_meter, 56, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(_meter, 96, LV_PART_INDICATOR);

    constexpr float kDotRadius = 188.0f;
    constexpr float kStartDeg = -58.0f;
    constexpr float kEndDeg = 58.0f;
    constexpr float kDeg2Rad = 3.14159265f / 180.0f;
    for (uint8_t index = 0; index < 8; ++index) {
        const float t = index / 7.0f;
        const float rad = (kStartDeg + t * (kEndDeg - kStartDeg)) * kDeg2Rad;
        _dot_x[index] = static_cast<int16_t>(kCenter + kDotRadius * std::cos(rad));
        _dot_y[index] = static_cast<int16_t>(kCenter + kDotRadius * std::sin(rad));
        _band_dots[index] = makeDot(_panel);
        lv_obj_set_pos(_band_dots[index], _dot_x[index] - 5, _dot_y[index] - 5);
    }

    _meta = lv_label_create(_panel);
    lv_label_set_text(_meta, "RUVIEW");
    styleLabel(_meta, &lv_font_montserrat_16, kMuted, LV_TEXT_ALIGN_LEFT);
    pinLeftColumn(_meta, kMetaY);

    _score = lv_label_create(_panel);
    styleLabel(_score, &CommissionerMedium108, kText, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_size(_score, lv_font_get_glyph_width(&CommissionerMedium108, '0', 0) * 2,
                    CommissionerMedium108.line_height);
    lv_label_set_long_mode(_score, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(_score, kScoreLeft, kScoreY);
    lv_label_set_text(_score, "0");

    _unit = lv_label_create(_panel);
    lv_label_set_text(_unit, "%");
    styleLabel(_unit, &lv_font_maple_mono_medium_48, kMuted, LV_TEXT_ALIGN_LEFT);
    lv_obj_add_flag(_unit, LV_OBJ_FLAG_HIDDEN);

    _alert = lv_obj_create(_panel);
    lv_obj_set_size(_alert, 16, 16);
    lv_obj_set_style_bg_color(_alert, lv_color_hex(kText), 0);
    lv_obj_set_style_bg_opa(_alert, 200, 0);
    lv_obj_set_style_border_width(_alert, 0, 0);
    lv_obj_set_style_radius(_alert, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(_alert, 0, 0);
    lv_obj_remove_flag(_alert, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_alert, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(_alert, _score, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    _detail = lv_label_create(_panel);
    lv_label_set_text(_detail, "Preparing Wi-Fi CSI.");
    styleLabel(_detail, &lv_font_montserrat_16, kMuted, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(_detail, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(_detail, 6, 0);
    pinLeftColumn(_detail, kDetailY);

    _stats = lv_label_create(_panel);
    lv_label_set_text(_stats, "");
    styleLabel(_stats, &lv_font_montserrat_14, kFaint, LV_TEXT_ALIGN_LEFT);
    pinLeftColumn(_stats, kDetailY + 52);

    _hint = lv_label_create(_panel);
    lv_label_set_text(_hint, "A RECAL     MED     B SENS");
    styleLabel(_hint, &lv_font_montserrat_14, kFaint, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_width(_hint, kCanvas);
    lv_obj_align(_hint, LV_ALIGN_BOTTOM_MID, 0, -36);

    setScore("--", false);
    _animation_timer = lv_timer_create(animationTimerCallback, 40, this);
}

void RuViewView::update(const ruview::SentinelSnapshot& snapshot, const char* error_message)
{
    if (!_panel) return;

    const bool calibrating = isCalibratingState(snapshot.state);
    const bool active = snapshot.state == ruview::SentinelState::Activity;
    const int meter_value = calibrating
                                ? static_cast<int>(snapshot.calibration_progress * 100.0f)
                                : static_cast<int>(std::clamp(snapshot.activity_score, 0.0f, 100.0f));
    const int display_value = std::clamp(meter_value, 0, 99);
    lv_arc_set_value(_meter, meter_value);
    _visual_snapshot = snapshot;
    _has_visual_snapshot = true;

    setLabelIfChanged(_meta, shortStateLabel(snapshot.state));
    lv_obj_set_style_text_font(_meta, active ? &lv_font_montserrat_22 : &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_meta, lv_color_hex(active ? kActiveText : kMuted), 0);
    pinLeftColumn(_meta, kMetaY);

    char score[8];
    if (isNumericScoreState(snapshot.state)) {
        std::snprintf(score, sizeof(score), "%d", display_value);
        setScore(score, true, calibrating);
        lv_obj_set_style_text_color(_score, lv_color_hex(active ? kActiveText : (calibrating ? kText : 0xC8C8C8)), 0);
        lv_obj_set_style_text_color(_unit, lv_color_hex(active ? kActiveText : kMuted), 0);
    } else {
        setScore("--", false);
        lv_obj_set_style_text_color(_score, lv_color_hex(kMuted), 0);
    }

    const bool show_alert = active || snapshot.state == ruview::SentinelState::Error ||
                            snapshot.state == ruview::SentinelState::DeviceMoving;
    if (show_alert) {
        lv_obj_remove_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_alert, LV_OBJ_FLAG_HIDDEN);
    }

    char detail[96];
    if (error_message) {
        std::snprintf(detail, sizeof(detail), "%s", error_message);
    } else if (snapshot.state == ruview::SentinelState::Activity) {
        std::snprintf(detail, sizeof(detail), "RF environment changed.");
    } else if (snapshot.state == ruview::SentinelState::Still && snapshot.has_activity_event) {
        std::snprintf(detail, sizeof(detail), "The room is still again.");
    } else {
        std::snprintf(detail, sizeof(detail), "%s.", ruview::sentinelStateDetail(snapshot.state));
    }
    setLabelIfChanged(_detail, detail);
    lv_obj_set_style_text_color(_detail, lv_color_hex(active ? kActiveText : kMuted), 0);
    pinLeftColumn(_detail, kDetailY);

    char stats[32];
    if (snapshot.state == ruview::SentinelState::Still || active ||
        snapshot.state == ruview::SentinelState::DeviceMoving) {
        int rssi = snapshot.rssi_dbm;
        if (_have_rssi && rssi - _last_rssi_dbm < 3 && _last_rssi_dbm - rssi < 3) {
            rssi = _last_rssi_dbm;
        } else {
            _last_rssi_dbm = rssi;
            _have_rssi = true;
        }
        std::snprintf(stats, sizeof(stats), "%d dBm", rssi);
    } else {
        _have_rssi = false;
        stats[0] = '\0';
    }
    setLabelIfChanged(_stats, stats);

    char hint[40];
    std::snprintf(hint, sizeof(hint), "A RECAL     %s     B SENS",
                  ruview::sensitivityName(snapshot.sensitivity));
    setLabelIfChanged(_hint, hint);
}

}  // namespace view
