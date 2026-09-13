/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_lucky_wheel.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>
#include <smooth_lvgl.hpp>

using namespace mooncake;

AppLuckyWheel::AppLuckyWheel()
{
    // Configure App name
    setAppInfo().name = "LuckyWheel";
    // Configure App icon
    setAppInfo().icon = (void*)&icon_lucky_wheel;
}

// Called when the App is installed
void AppLuckyWheel::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppLuckyWheel::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    _external_power = GetHAL().setGrove5VPower(true);
    if (_external_power) {
        GetHAL().delay(20);
        _external_input = std::make_unique<lucky_wheel::ExternalInput>();
        _external_input->open();
    } else {
        mclog::tagError(getAppInfo().name, "external controller power unavailable");
    }

    LvglLockGuard lock;

    _selection_view = std::make_unique<view::SelectionView>();
    _selection_view->init(lv_screen_active());
}

void AppLuckyWheel::onRunning()
{
    input::KeyEvent key_event = input::KeyEvent::None;
    if (_key_manager) {
        key_event = _key_manager->update();
    }
    const auto external = _external_input
        ? _external_input->consume() : lucky_wheel::ExternalInputEvents{};

    if (key_event == input::KeyEvent::GoHome || external.exit) {
        close();
        return;
    }

    if (_selection_view) {
        {
            LvglLockGuard lock;
            const char* controllerStatus = "Joystick: power unavailable";
            if (_external_input) {
                using vector_canyon_fighter::InputReadiness;
                switch (_external_input->readiness()) {
                    case InputReadiness::Ready:
                        controllerStatus = "Joystick: ready";
                        break;
                    case InputReadiness::Calibrating:
                        controllerStatus = "Joystick: calibrating";
                        break;
                    case InputReadiness::Fault:
                        controllerStatus = "Joystick: fault";
                        break;
                    default:
                        controllerStatus = "Joystick: disconnected";
                        break;
                }
            }
            _selection_view->setControllerStatus(controllerStatus);
            _selection_view->moveSelection(external.selectionSteps);
            if (external.confirm) _selection_view->confirm();
        }
        if (_selection_view->isConfirmed()) {
            int optionCount = _selection_view->confirmedOptionCount();

            if (_external_input) _external_input->changeToWheel();
            LvglLockGuard lock;
            _selection_view.reset();
            _wheel_view = std::make_unique<view::WheelView>();
            _wheel_view->init(lv_screen_active(), optionCount);
        }

        return;
    }

    if (_wheel_view) {
        LvglLockGuard lock;

        if (external.confirm) {
            _wheel_view->startSpin(view::SpinDirection::Random);
        } else if (key_event == input::KeyEvent::GoPrevious) {
            _wheel_view->startSpin(view::SpinDirection::Counterclockwise);
        } else if (key_event == input::KeyEvent::GoNext) {
            _wheel_view->startSpin(view::SpinDirection::Clockwise);
        }

        _wheel_view->update();
    }
}

void AppLuckyWheel::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();
    if (_external_input) {
        _external_input->close();
        _external_input.reset();
    }
    if (_external_power) {
        GetHAL().setGrove5VPower(false);
        _external_power = false;
    }

    LvglLockGuard lock;

    _selection_view.reset();
    _wheel_view.reset();
}
