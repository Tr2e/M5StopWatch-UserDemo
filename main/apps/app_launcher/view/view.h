/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <apps/common/arc_top_clock/arc_top_clock.h>
#include <apps/common/key_manager/key_manager.h>
#include <apps/app_launcher/launcher_external_input_logic.h>
#include <apps/app_vector_canyon_fighter/input/dual_button_action_source.h>
#include <apps/app_vector_canyon_fighter/input/joystick2_axis_source.h>
#include <mooncake.h>
#include <smooth_ui_toolkit.hpp>
#include <uitk/short_namespace.hpp>
#include <smooth_lvgl.hpp>
#include <functional>
#include <vector>
#include <memory>

namespace view {

/**
 * @brief
 *
 */
class LauncherView {
public:
    ~LauncherView();

    enum State_t {
        STATE_STARTUP,
        STATE_NORMAL,
    };

    std::function<void(int appID)> onAppClicked;

    void init(std::vector<mooncake::AppProps_t> appPorps);
    void update();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _icon_panels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Image>> _icon_images;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _lr_indicator_panels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Image>> _lr_indicators_images;
    std::unique_ptr<view::ArcTopClock> _clock;
    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<vector_canyon_fighter::Joystick2AxisSource> _external_joystick;
    std::unique_ptr<vector_canyon_fighter::DualButtonActionSource> _external_buttons;
    launcher_input::AxisNavigationRepeater _external_navigation;
    std::vector<int> _icon_app_ids;
    bool _external_power_enabled = false;

    int _clicked_app_id = -1;
    State_t _state      = STATE_STARTUP;

    void scroll_to_nearby_icon(int direction);
    void update_external_controller();
    void open_centered_app();
    void handle_state_startup();
    void handle_state_normal();
    void handle_scroll_in_loop();
};

/**
 * @brief
 *
 */
class GuidePage {
public:
    GuidePage();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Image> _img;
};

}  // namespace view
