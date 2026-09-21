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
    void resetPerformanceWindow(uint32_t now);
    gundam_museum::MuseumRenderer _renderer;
    gundam_museum::MuseumController _controller;
    lets_and_go::DeviceControlSource _input;
    bool _direct=false,_presented=false;
    uint32_t _perfStarted=0,_perfFrames=0,_perfFrames65=0,_perfFrames100=0;
    uint32_t _perfPeakUs=0;
    uint64_t _perfDrawUs=0,_perfPresentUs=0;
    uint64_t _perfClearUs=0,_perfCullUs=0,_perfRasterUs=0,_perfBlitUs=0;
};
