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
    void benchmarkFrame();
    void resetPerformanceWindow(uint32_t now);
    gundam_museum::MuseumRenderer _renderer;
    gundam_museum::MuseumController _controller;
    lets_and_go::DeviceControlSource _input;
    bool _direct=false,_presented=false,_resumeWifi=false;
    uint32_t _perfStarted=0,_perfFrames=0,_perfFrames65=0,_perfFrames100=0;
    uint32_t _perfPeakUs=0;
    uint64_t _perfDrawUs=0,_perfPresentUs=0;
    uint64_t _perfClearUs=0,_perfCullUs=0,_perfRasterUs=0,_perfBlitUs=0,_perfOverlayUs=0;
    uint64_t _perfBackgroundUs=0,_perfSpaceUs=0,_perfDepthClearUs=0;
    uint64_t _perfPanelPrepareUs=0,_perfSpaceWaitUs=0,_perfMainRasterUs=0,_perfWorkerRasterUs=0;
    uint32_t _benchmarkFrame=0,_benchmarkBatch=0,_benchmarkHashA=2166136261u,_benchmarkHashB=0;
    uint64_t _benchmarkDrawUs=0,_benchmarkPresentUs=0,_benchmarkCullUs=0,_benchmarkPanelUs=0;
    uint64_t _benchmarkRasterUs=0,_benchmarkMainUs=0,_benchmarkWorkerUs=0,_benchmarkBlitUs=0,_benchmarkBackgroundUs=0;
    uint64_t _benchmarkAsyncPresentUs=0;
};
