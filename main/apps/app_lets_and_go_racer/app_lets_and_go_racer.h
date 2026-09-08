#pragma once

#include "controller/game_flow.h"

#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppLetsAndGoRacer : public mooncake::AppAbility {
public:
    AppLetsAndGoRacer();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    void renderShell(uint32_t nowMs);

    std::unique_ptr<input::KeyManager> _keys;
    lets_and_go::GameFlow _flow;
    uint32_t _lastFrameMs = 0;
};
