#include "app_gundam_museum.h"
#include "view/museum_layout.h"
#include "../common/performance/display_frame_scope.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uint32_t kPerformanceWindowMs=2000;
}

AppGundamMuseum::AppGundamMuseum(){setAppInfo().name="Gundam Museum";setAppInfo().icon=(void*)&icon_gundam_museum;}
void AppGundamMuseum::onOpen(){
    GetHAL().stopLvglUpdate();GetHAL().lvglLock();GetHAL().lvglUnlock();
    _controller.reset();_presented=false;
    _direct=GetHAL().hasDisplayFrameBuffer();
    const bool ready=_renderer.open();
    mclog::tagInfo("Museum","open ready={} working_bytes={}",ready,_renderer.workingBytes());
    _input.open();_input.setScreen(lets_and_go::GameScreen::MuseumInspect);
    draw(GetHAL().millis());_input.presentScreen(lets_and_go::GameScreen::MuseumInspect);
    // Exclude model/index construction and the first complete framebuffer
    // from the steady-state interaction window.
    resetPerformanceWindow(GetHAL().millis());
}
void AppGundamMuseum::onRunning(){
    auto input=_input.sample(GetHAL().millis());
    const uint32_t now=GetHAL().millis();
    const bool dirty=_controller.update(input,now);
    if(_controller.exitRequested()){close();return;}
    if(dirty)draw(now);
    GetHAL().delay(5);
}
void AppGundamMuseum::draw(uint32_t now){
    const uint64_t start=esp_timer_get_time();
    const bool partial=_presented;
    uint64_t rendered=0;
    if(_direct){
        auto& display=GetHAL().getDisplay();
        // Room grids rotate into the top/bottom gutters as well as the model
        // tile. Present their full dirty extent in both rendering paths.
        const app_performance::DisplayRegion region{0,0,display.width(),display.height()};
        app_performance::DisplayFrameScope frame(display,region);
        _renderer.render(display,_controller.view(),_controller.percent(now),true,false,false,partial);
        rendered=esp_timer_get_time();frame.finish();
    }else{
        _renderer.render(GetHAL().getCanvas(),_controller.view(),_controller.percent(now),true,false,false,partial);
        rendered=esp_timer_get_time();
        GetHAL().updateCanvas();
    }
    _presented=true;
    const uint64_t finished=esp_timer_get_time();
    const uint32_t drawUs=uint32_t(rendered-start),presentUs=uint32_t(finished-rendered);
    const uint32_t frameUs=uint32_t(finished-start);
    const int percent=_controller.percent(now);
    const auto stats=_renderer.stats();
    ++_perfFrames;
    if(percent==65)++_perfFrames65;
    if(percent==100)++_perfFrames100;
    _perfDrawUs+=drawUs;_perfPresentUs+=presentUs;
    _perfClearUs+=stats.clearUs;_perfCullUs+=stats.prepareUs;
    _perfRasterUs+=stats.rasterUs;_perfBlitUs+=stats.blitUs;
    _perfPeakUs=std::max(_perfPeakUs,frameUs);
    const uint32_t finishedMs=GetHAL().millis();
    if(finishedMs-_perfStarted>=kPerformanceWindowMs){
        const uint32_t elapsed=finishedMs-_perfStarted;
        const uint32_t frames=std::max<uint32_t>(1,_perfFrames);
        mclog::tagInfo("MuseumPerf","model={} frames={} fps_x10={} scale65={} scale100={} draw_us={} present_us={} peak_us={}",
            int(_controller.view().model),_perfFrames,_perfFrames*10000u/elapsed,
            _perfFrames65,_perfFrames100,uint32_t(_perfDrawUs/frames),
            uint32_t(_perfPresentUs/frames),_perfPeakUs);
        mclog::tagInfo("MuseumStageAvg","clear_us={} cull_us={} project_raster_us={} blit_us={} stack={}",
            uint32_t(_perfClearUs/frames),uint32_t(_perfCullUs/frames),
            uint32_t(_perfRasterUs/frames),uint32_t(_perfBlitUs/frames),
            uint32_t(uxTaskGetStackHighWaterMark(nullptr)));
        mclog::tagInfo("MuseumMemory","internal_free={} internal_min={} internal_largest={} psram_free={} psram_min={} psram_largest={}",
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        resetPerformanceWindow(finishedMs);
    }
}
void AppGundamMuseum::resetPerformanceWindow(uint32_t now){
    _perfStarted=now;_perfFrames=0;_perfFrames65=0;_perfFrames100=0;_perfPeakUs=0;
    _perfDrawUs=0;_perfPresentUs=0;_perfClearUs=0;_perfCullUs=0;_perfRasterUs=0;_perfBlitUs=0;
}
void AppGundamMuseum::onClose(){
    _input.close();_renderer.close();_controller.reset();_direct=false;_presented=false;
    GetHAL().startLvglUpdate();
}
