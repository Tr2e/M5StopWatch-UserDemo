#pragma once

#include "controller/game_flow.h"
#include "controller/garage_selection.h"
#include "input/hardware_racer_input_provider.h"
#include "input/racer_input_logic.h"
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
    void handleRacerInput(const lets_and_go::RacerInput& input, uint32_t nowMs);

    std::unique_ptr<input::KeyManager> _keys;
    std::unique_ptr<lets_and_go::RacerInputProvider> _racerInput;
    lets_and_go::MenuAxisRepeater _menuAxis;
    lets_and_go::GameFlow _flow;
    lets_and_go::GarageSelection _selection;
    lets_and_go::GarageRenderer _renderer;
    uint32_t _lastFrameMs = 0;
    uint32_t _screenStartedMs = 0;
};
