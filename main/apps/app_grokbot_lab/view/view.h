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
        Surprised,
        Sleeping,
        Alerting,
        Celebrate,
        Progress,
        Exclaim,
        Hex,
        Party,
        Orbit,
        Spawn,
        Count,
    };

    void init(lv_obj_t* parent);
    void update(uint32_t now);
    void nextState();
    void previousState();
    void celebrate();
    void showProgress();
    void setButtonFeedback(bool left_pressed, bool right_pressed, uint32_t now);
    void toggleDemo();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _stage;
    std::unique_ptr<uitk::lvgl_cpp::Label> _state_label;
    State _state = State::Idle;
    uint32_t _state_started_at = 0;
    uint32_t _last_redraw_at = 0;
    uint32_t _state_label_shown_at = 0;
    uint32_t _left_button_released_at = 0;
    uint32_t _right_button_released_at = 0;
    uint32_t _demo_step_started_at = 0;
    uint8_t _demo_index = 0;
    int _gaze_x = 0;
    int _gaze_y = 0;
    bool _touching = false;
    bool _auto_return_to_idle = false;
    bool _left_button_pressed = false;
    bool _right_button_pressed = false;
    bool _demo_mode = false;

    void setState(State state);
    void updateLabels();
    void updateGaze(lv_event_t* event, bool active);
    static void onStageEvent(lv_event_t* event);
    static void onStageDraw(lv_event_t* event);
};

}  // namespace view
