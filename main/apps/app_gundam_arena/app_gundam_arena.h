#pragma once
#include "controller/arena_controller.h"
#include "../app_lets_and_go_racer/input/device_control_source.h"
#include "../app_lets_and_go_racer/input/hardware_racer_input_provider.h"
#include "../common/key_manager/key_manager.h"
#include <memory>
#include <mooncake.h>

class AppGundamArena : public mooncake::AppAbility {
public:
    AppGundamArena();
    void onOpen() override;
    void onRunning() override;
    void onClose() override;
private:
    void draw(uint32_t now);
    gundam_arena::ArenaRenderer _renderer;
    gundam_arena::ArenaController _controller;
    lets_and_go::DeviceControlSource _input;
    std::unique_ptr<lets_and_go::HardwareRacerInputProvider> _pad;
    std::unique_ptr<input::KeyManager> _keys;
    bool _direct=false,_externalPower=false;
    gundam_arena::Mode _padNavMode=gundam_arena::Mode::Play;
    uint32_t _lastFrame=0,_lastLog=0;
};
