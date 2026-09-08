#pragma once

#include "controller/game_flow.h"
#include "controller/garage_selection.h"
#include "view/garage_renderer.h"

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
    void handleKey(input::KeyEvent event, uint32_t nowMs);

    std::unique_ptr<input::KeyManager> _keys;
    lets_and_go::GameFlow _flow;
    lets_and_go::GarageSelection _selection;
    lets_and_go::GarageRenderer _renderer;
    uint32_t _lastFrameMs = 0;
    uint32_t _screenStartedMs = 0;
};
