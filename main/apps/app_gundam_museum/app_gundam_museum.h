#pragma once
#include "controller/museum_controller.h"
#include "../app_lets_and_go_racer/input/device_control_source.h"
#include <mooncake.h>

class AppGundamMuseum : public mooncake::AppAbility {
public:
    AppGundamMuseum();
    void onOpen() override;
    void onRunning() override;
    void onClose() override;
private:
    void draw(uint32_t now);
    gundam_museum::MuseumRenderer _renderer;
    gundam_museum::MuseumController _controller;
    lets_and_go::DeviceControlSource _input;
    bool _direct=false,_presented=false,_lastAuto=false;
    uint32_t _lastLog=0;
};
