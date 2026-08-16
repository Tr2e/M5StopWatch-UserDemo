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

    lv_obj_t* _panel = nullptr;
    lv_obj_t* _pulse = nullptr;
    lv_obj_t* _eyebrow = nullptr;
    lv_obj_t* _state = nullptr;
    lv_obj_t* _detail = nullptr;
    lv_obj_t* _meter = nullptr;
    lv_obj_t* _score = nullptr;
    lv_obj_t* _stats = nullptr;
    lv_obj_t* _hint = nullptr;
    lv_obj_t* _core = nullptr;
    lv_obj_t* _band_bubbles[8] = {};
    lv_timer_t* _animation_timer = nullptr;
    ruview::SentinelSnapshot _visual_snapshot = {};
    int16_t _bubble_x[8] = {};
    int16_t _bubble_y[8] = {};
    uint8_t _bubble_size[8] = {};
    uint8_t _bubble_float_amplitude[8] = {};
    uint8_t _bubble_shake_strength[8] = {};
    float _bubble_phase[8] = {};
    float _bubble_speed[8] = {};
    uint32_t _visual_tick = 0;
    float _pulse_progress = 0.0f;
    float _shock_progress = 0.0f;
    bool _pulse_active = false;
    bool _shock_active = false;
    bool _has_visual_snapshot = false;
    ruview::SentinelState _last_state = ruview::SentinelState::Error;
};

}  // namespace view
