/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "model/csi_sentinel.h"
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppRuView : public mooncake::AppAbility {
public:
    AppRuView();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    float updateDeviceMotion();

    std::unique_ptr<view::RuViewView> _view;
    std::unique_ptr<input::KeyManager> _key_manager;
    ruview::CsiSentinel _sentinel;
    ruview::SentinelState _last_state = ruview::SentinelState::Error;
    uint32_t _last_sample_ms = 0;
    bool _have_previous_accel = false;
    float _previous_accel_x = 0.0f;
    float _previous_accel_y = 0.0f;
    float _previous_accel_z = 0.0f;
    float _filtered_motion = 0.0f;
};
