/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_grokbot_lab.h"
#include "icon_bot_lab.h"
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;

AppGrokBotLab::AppGrokBotLab()
{
    setAppInfo().name = "Bot Lab";
    setAppInfo().icon = (void*)getBotLabIcon();
}

void AppGrokBotLab::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppGrokBotLab::onOpen()
{
    _key_manager = std::make_unique<input::KeyManager>();
    LvglLockGuard lock;
    _view = std::make_unique<view::GrokBotLabView>();
    _view->init(lv_screen_active());
}

void AppGrokBotLab::onRunning()
{
    GetHAL().updateButtonStates();
    if (_key_manager && _key_manager->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

    LvglLockGuard lock;
    if (_view) {
        const uint32_t now = GetHAL().millis();
        const bool leftPressed = GetHAL().btnA.isPressed();
        const bool rightPressed = GetHAL().btnB.isPressed();
        _view->setButtonFeedback(leftPressed, rightPressed, now);

        // Hold both physical keys to start/stop the hands-free expression reel.
        if (leftPressed && rightPressed) {
            if (_combo_started_at == 0) _combo_started_at = now;
            if (!_combo_consumed && now - _combo_started_at >= 650) {
                _view->toggleDemo();
                _combo_consumed = true;
            }
        } else {
            _combo_started_at = 0;
        }
        if (_combo_consumed) {
            if (!leftPressed && !rightPressed) _combo_consumed = false;
            _view->update(now);
            return;
        }
        if (GetHAL().btnA.wasReleased()) {
            if (GetHAL().btnA.wasReleasedAfterHold()) {
                _view->celebrate();
            } else {
                _view->previousState();
            }
        }
        if (GetHAL().btnB.wasReleased()) {
            if (GetHAL().btnB.wasReleasedAfterHold()) {
                _view->showProgress();
            } else {
                _view->nextState();
            }
        }
        _view->update(now);
    }
}

void AppGrokBotLab::onClose()
{
    _key_manager.reset();
    LvglLockGuard lock;
    _view.reset();
}
