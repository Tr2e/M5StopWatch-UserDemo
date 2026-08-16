/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ruview.h"

#include <assets/assets.h>
#include <cmath>
#include <hal/hal.h>
#include <mooncake_log.h>

AppRuView::AppRuView()
{
    setAppInfo().name = "RuView";
    setAppInfo().icon = (void*)&icon_ruview;
}

void AppRuView::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppRuView::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _key_manager = std::make_unique<input::KeyManager>();
    _last_sample_ms = 0;
    _second_activity_pulse_at = 0;
    _have_previous_accel = false;
    _filtered_motion = 0.0f;
    _last_state = ruview::SentinelState::Error;
    _sentinel.start();

    LvglLockGuard lock;
    _view = std::make_unique<view::RuViewView>();
    _view->init(lv_screen_active());
}

float AppRuView::updateDeviceMotion()
{
    GetHAL().updateImuData();
    const auto& imu = GetHAL().getImuData();
    if (!_have_previous_accel) {
        _previous_accel_x = imu.accelX;
        _previous_accel_y = imu.accelY;
        _previous_accel_z = imu.accelZ;
        _have_previous_accel = true;
        return 0.0f;
    }

    const float dx = imu.accelX - _previous_accel_x;
    const float dy = imu.accelY - _previous_accel_y;
    const float dz = imu.accelZ - _previous_accel_z;
    const float accel_delta = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float gyro = std::sqrt(imu.gyroX * imu.gyroX + imu.gyroY * imu.gyroY + imu.gyroZ * imu.gyroZ) / 180.0f;
    const float instantaneous = accel_delta + gyro;
    _filtered_motion += 0.28f * (instantaneous - _filtered_motion);

    _previous_accel_x = imu.accelX;
    _previous_accel_y = imu.accelY;
    _previous_accel_z = imu.accelZ;
    return _filtered_motion;
}

void AppRuView::onRunning()
{
    GetHAL().updateButtonStates();
    const auto key_event = _key_manager ? _key_manager->update(false) : input::KeyEvent::None;
    if (key_event == input::KeyEvent::GoHome) {
        close();
        return;
    }
    if (key_event == input::KeyEvent::GoPrevious) {
        _sentinel.resetCalibration();
        GetHAL().vibrate(45, 55);
    } else if (key_event == input::KeyEvent::GoNext) {
        _sentinel.cycleSensitivity();
        GetHAL().vibrate(30, 45);
    }

    const uint32_t now = GetHAL().millis();
    if (_second_activity_pulse_at != 0 && now >= _second_activity_pulse_at) {
        GetHAL().vibrate(65, 78);
        _second_activity_pulse_at = 0;
    }
    if (_last_sample_ms != 0 && now - _last_sample_ms < 200) {
        return;
    }
    _last_sample_ms = now;

    const float motion = updateDeviceMotion();
    const auto snapshot = _sentinel.update(now, motion);
    if (snapshot.state == ruview::SentinelState::Activity && _last_state != ruview::SentinelState::Activity) {
        GetHAL().vibrate(70, 90);
        _second_activity_pulse_at = now + 135;
    }
    _last_state = snapshot.state;

    LvglLockGuard lock;
    if (_view) {
        _view->update(snapshot, snapshot.state == ruview::SentinelState::Error ? _sentinel.errorMessage() : nullptr);
    }
}

void AppRuView::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _sentinel.stop();
    GetHAL().stopVibrate();
    _key_manager.reset();
    LvglLockGuard lock;
    _view.reset();
}
