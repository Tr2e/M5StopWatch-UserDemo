/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "../model/csi_sentinel.h"
#include <lvgl.h>

namespace view {

class RuViewView {
public:
    ~RuViewView();
    void init(lv_obj_t* parent);
    void update(const ruview::SentinelSnapshot& snapshot, const char* error_message = nullptr);

private:
    static void animationTimerCallback(lv_timer_t* timer);
    void updateAmbientVisuals();
    void setScore(const char* text, bool large_numeric, bool show_unit = false);

    lv_obj_t* _panel = nullptr;
    lv_obj_t* _meter = nullptr;
    lv_obj_t* _meta = nullptr;
    lv_obj_t* _score = nullptr;
    lv_obj_t* _unit = nullptr;
    lv_obj_t* _alert = nullptr;
    lv_obj_t* _detail = nullptr;
    lv_obj_t* _stats = nullptr;
    lv_obj_t* _hint = nullptr;
    lv_obj_t* _band_dots[8] = {};
    lv_timer_t* _animation_timer = nullptr;
    ruview::SentinelSnapshot _visual_snapshot = {};
    int16_t _dot_x[8] = {};
    int16_t _dot_y[8] = {};
    uint32_t _visual_tick = 0;
    bool _has_visual_snapshot = false;
    int _last_rssi_dbm = 0;
    bool _have_rssi = false;
};

}  // namespace view
