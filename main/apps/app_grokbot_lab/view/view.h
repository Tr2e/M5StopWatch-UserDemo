/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>

namespace view {

class GrokBotLabView {
public:
    enum class State : uint8_t {
        Idle,
        Curious,
        Listening,
        Thinking,
        Working,
        Happy,
        Playful,
        Sleeping,
        Exclaim,
        Progress,
        Surprised,
        Alerting,
        Celebrate,
        Orbit,
        Dance,
        Star,
        Count,
    };

    void init(lv_obj_t* parent);
    void update(uint32_t now);
    void nextState();
    void previousState();
    void celebrate();
    void showProgress();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _stage;
    State _state = State::Idle;
    State _previous_state = State::Idle;
    uint32_t _state_started_at = 0;
    uint32_t _last_redraw_at = 0;
    int _gaze_x = 0;
    int _gaze_y = 0;
    int _expression_gaze_x = 0;
    int _expression_gaze_y = 0;
    uint32_t _gaze_seed = 0xA341316Cu;
    bool _touching = false;
    bool _auto_return_to_idle = false;
    bool _motion_initialized = false;
    uint32_t _motion_updated_at = 0;
    float _motion_body_x = 0.0f;
    float _motion_body_y = 0.0f;
    float _motion_body_w = 0.0f;
    float _motion_body_h = 0.0f;
    float _motion_eye_y = 0.0f;
    float _motion_eye_gap = 0.0f;
    float _motion_eye_w = 0.0f;
    float _motion_eye_h = 0.0f;
    float _motion_gaze_x = 0.0f;
    float _motion_gaze_y = 0.0f;
    uint32_t _gaze_updated_at = 0;
    float _transition_body_x = 0.0f;
    float _transition_body_y = 0.0f;
    float _transition_body_w = 0.0f;
    float _transition_body_h = 0.0f;
    float _transition_eye_y = 0.0f;
    float _transition_eye_gap = 0.0f;
    float _transition_eye_w = 0.0f;
    float _transition_eye_h = 0.0f;
    float _transition_gaze_x = 0.0f;
    float _transition_gaze_y = 0.0f;

    void setState(State state);
    void smoothPose(uint32_t now, int& body_x, int& body_y, int& body_w, int& body_h, int& eye_y, int& eye_gap, int& eye_w, int& eye_h);
    void smoothGaze(uint32_t now, float target_x, float target_y, int& gaze_x, int& gaze_y);
    void updateGaze(lv_event_t* event, bool active);
    static void onStageEvent(lv_event_t* event);
    static void onStageDraw(lv_event_t* event);
};

}  // namespace view
