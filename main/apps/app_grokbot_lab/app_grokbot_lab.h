/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <mooncake.h>
#include <memory>

class AppGrokBotLab : public mooncake::AppAbility {
public:
    AppGrokBotLab();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<view::GrokBotLabView> _view;
    std::unique_ptr<input::KeyManager> _key_manager;
    uint32_t _combo_started_at = 0;
    bool _combo_consumed = false;
};
