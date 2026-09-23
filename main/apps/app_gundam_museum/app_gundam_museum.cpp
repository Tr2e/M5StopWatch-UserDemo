#include "app_gundam_museum.h"
#include "view/museum_layout.h"
#include "../common/performance/display_frame_scope.h"
#include "../common/network/wifi_service.h"
#include "../common/soft3d/asset/builtin_samples.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <wifi_manager.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr uint32_t kPerformanceWindowMs=2000;
constexpr uint32_t kBenchmarkFrames=96;
constexpr int kSampleSide=192;
const char* gradeName(soft3d::FrameRateGrade grade) {
    switch(grade) {
    case soft3d::FrameRateGrade::Fps30:return "30fps";
    case soft3d::FrameRateGrade::Fps24:return "24fps";
    case soft3d::FrameRateGrade::Fps20:return "20fps";
    case soft3d::FrameRateGrade::Fps15:return "15fps";
    default:return "below15";
    }
}
uint32_t p95(const std::array<uint32_t,kBenchmarkFrames>& values) {
    static std::array<uint32_t,kBenchmarkFrames> scratch;
    scratch=values;std::sort(scratch.begin(),scratch.end());return scratch[91];
}
}

struct Soft3DSampleBenchmark {
    decltype(soft3d::samples::makeCrate()) crate=soft3d::samples::makeCrate();
    decltype(soft3d::samples::makeRigidChain()) chain=soft3d::samples::makeRigidChain();
    soft3d::MeasuredSurfaceRaster<kSampleSide,kSampleSide> raster;
};

AppGundamMuseum::AppGundamMuseum(){setAppInfo().name="Gundam Museum";setAppInfo().icon=(void*)&icon_gundam_museum;}
AppGundamMuseum::~AppGundamMuseum()=default;
void AppGundamMuseum::onOpen(){
    GetHAL().stopLvglUpdate();GetHAL().lvglLock();GetHAL().lvglUnlock();
    _controller.reset();_presented=false;
    _benchmarkScene=0;_benchmarkLifecycleChecked=false;_benchmarkFrame=_benchmarkBatch=0;
    _benchmarkHashA=2166136261u;_benchmarkHashB=0;_benchmarkCoveredPixels=0;
    _benchmarkTestedPixels=_benchmarkWrittenPixels=_benchmarkDepthRejectedPixels=0;
    _benchmarkDrawUs=_benchmarkPresentUs=_benchmarkAsyncPresentUs=_benchmarkCullUs=_benchmarkPanelUs=0;
    _benchmarkRasterUs=_benchmarkMainUs=_benchmarkWorkerUs=_benchmarkBlitUs=_benchmarkBackgroundUs=0;
    auto& wifi=WifiManager::GetInstance();
    _resumeWifi=network::WifiService::GetInstance().getStatus().has_saved_network && !wifi.IsConfigMode();
    if(_resumeWifi)wifi.StopStation();
    _direct=GetHAL().hasDisplayFrameBuffer();
    if(_direct && !GetHAL().setDisplayFrameBufferAsync(true))
        mclog::tagWarn("Museum","async framebuffer unavailable; using synchronous present");
    const bool ready=_renderer.open();
    mclog::tagInfo("Museum","open ready={} working_bytes={}",ready,_renderer.workingBytes());
    _sampleBenchmark=std::unique_ptr<Soft3DSampleBenchmark>(new(std::nothrow) Soft3DSampleBenchmark{});
    if(!_sampleBenchmark)mclog::tagWarn("Soft3DBench","sample workspace unavailable; continuing with RX-78");
    else {
        _sampleBenchmark->raster.setSolidFastPath(true);
        _sampleBenchmark->raster.setSolidSpanFastPath(true);
        _sampleBenchmark->raster.setTrustedSolidDepthFastPath(true);
        _sampleBenchmark->raster.setSolidQuadFastPath(true);
    }
    _input.open();_input.setScreen(lets_and_go::GameScreen::MuseumInspect);
    draw(GetHAL().millis());_input.presentScreen(lets_and_go::GameScreen::MuseumInspect);
    // Exclude model/index construction and the first complete framebuffer
    // from the steady-state interaction window.
    resetPerformanceWindow(GetHAL().millis());
}
void AppGundamMuseum::onRunning(){
    benchmarkFrame();return;
    auto input=_input.sample(GetHAL().millis());
    const uint32_t now=GetHAL().millis();
    const bool dirty=_controller.update(input,now);
    if(_controller.exitRequested()){close();return;}
    if(dirty)draw(now);
    GetHAL().delay(5);
}
void AppGundamMuseum::benchmarkFrame(){
    constexpr uint32_t frames=kBenchmarkFrames;
    if(_benchmarkScene<2 && !_sampleBenchmark)_benchmarkScene=2;
    auto& display=GetHAL().getDisplay();
    if(_benchmarkScene<2) {
        const bool rigid=_benchmarkScene==1;const char* scene=rigid?"rigid_chain":"crate";
        const int originX=(display.width()-kSampleSide)/2,originY=(display.height()-kSampleSide)/2;
        const app_performance::DisplayRegion region{0,0,display.width(),display.height()};
        const uint64_t started=esp_timer_get_time();app_performance::DisplayFrameScope frame(display,region);
        display.fillScreen(0x0841);auto& raster=_sampleBenchmark->raster;raster.resetMetrics();raster.begin(originX,originY);
        soft3d::Camera camera{};camera.principalX=display.width()*.5f;camera.principalY=display.height()*.5f+24.f;camera.focalLength=132.f;
        const float angle=float(_benchmarkFrame)*6.28318530718f/float(frames),cs=std::cos(angle),sn=std::sin(angle);
        soft3d::ModelInstance instance{};
        instance.asset=rigid?&_sampleBenchmark->chain.storage.asset:&_sampleBenchmark->crate.storage.asset;
        _benchmarkWork=soft3d::renderModelAsset(raster,camera,instance,[&](soft3d::Vec3 p,uint16_t bone) {
            const float x=cs*p.x+sn*p.z,z=-sn*p.x+cs*p.z;
            const float lift=rigid && (bone&1u)?.10f:0.f;
            return soft3d::CameraPoint{x,p.y-.45f+lift,z+3.4f};
        });
        const auto rasterMetrics=raster.metrics();_benchmarkWork.testedPixels=rasterMetrics.testedPixels;
        _benchmarkWork.writtenPixels=rasterMetrics.writtenPixels;_benchmarkWork.depthRejectedPixels=rasterMetrics.depthRejectedPixels;
        _benchmarkTestedPixels+=rasterMetrics.testedPixels;_benchmarkWrittenPixels+=rasterMetrics.writtenPixels;
        _benchmarkDepthRejectedPixels+=rasterMetrics.depthRejectedPixels;
        raster.blit(display);const uint64_t rendered=esp_timer_get_time();
        uint32_t covered=0;for(int y=0;y<kSampleSide;++y)for(int x=0;x<kSampleSide;++x)covered+=raster.depthAt(x,y)!=0;
        _benchmarkCoveredPixels+=covered;
        uint32_t frameHash=2166136261u;for(int y=0;y<display.height();++y){const auto* row=reinterpret_cast<const uint16_t*>(GetHAL().getDisplayFrameBufferLine(y));for(int x=0;x<display.width();++x)frameHash=(frameHash^row[x])*16777619u;}_benchmarkHashA=(_benchmarkHashA^frameHash)*16777619u;_benchmarkHashB+=frameHash;
        const uint64_t beforePresent=esp_timer_get_time();frame.finish();const uint64_t finished=esp_timer_get_time();
        const auto drawUs=uint32_t(rendered-started),presentUs=uint32_t(finished-beforePresent),cycleUs=drawUs+presentUs;
        _benchmarkDrawSamples[_benchmarkFrame]=drawUs;_benchmarkPresentSamples[_benchmarkFrame]=presentUs;_benchmarkCycleSamples[_benchmarkFrame]=cycleUs;
        _benchmarkDrawUs+=drawUs;_benchmarkPresentUs+=presentUs;_benchmarkAsyncPresentUs+=GetHAL().getDisplayFrameBufferPresentUs();
        if(++_benchmarkFrame==frames) {
            const auto p95Cycle=p95(_benchmarkCycleSamples);const auto maxCycle=*std::max_element(_benchmarkCycleSamples.begin(),_benchmarkCycleSamples.end());
            const auto core=soft3d::selectCoreMode(soft3d::game30Profile(),{},_benchmarkWork);
            mclog::tagInfo("Soft3DBatch","scene={} frames={} vertices={} primitives={} solid={} tested={} written={} rejected={} final_pixels={} route={} hash={}:{}",
                scene,frames,_benchmarkWork.uniqueVertices,_benchmarkWork.visiblePrimitives,_benchmarkWork.solidPrimitives,
                uint32_t(_benchmarkTestedPixels/frames),uint32_t(_benchmarkWrittenPixels/frames),uint32_t(_benchmarkDepthRejectedPixels/frames),uint32_t(_benchmarkCoveredPixels/frames),
                core==soft3d::CoreMode::DualBands?"dual":"single",_benchmarkHashA,_benchmarkHashB);
            mclog::tagInfo("Soft3DTiming","scene={} draw_avg={} draw_p95={} present_avg={} present_p95={} cycle_avg={} cycle_p95={} cycle_max={} grade={}",
                scene,uint32_t(_benchmarkDrawUs/frames),p95(_benchmarkDrawSamples),uint32_t(_benchmarkPresentUs/frames),p95(_benchmarkPresentSamples),
                uint32_t((_benchmarkDrawUs+_benchmarkPresentUs)/frames),p95Cycle,maxCycle,gradeName(soft3d::classifyFrameRate(p95Cycle)));
            mclog::tagInfo("Soft3DMemory","scene={} internal_free={} internal_min={} internal_largest={} psram_free={} psram_min={} psram_largest={} main_stack_free={}",scene,
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),uint32_t(uxTaskGetStackHighWaterMark(nullptr)));
            ++_benchmarkScene;_benchmarkFrame=0;_benchmarkHashA=2166136261u;_benchmarkHashB=0;_benchmarkDrawUs=_benchmarkPresentUs=_benchmarkAsyncPresentUs=0;_benchmarkCoveredPixels=0;_benchmarkTestedPixels=_benchmarkWrittenPixels=_benchmarkDepthRejectedPixels=0;
            if(_benchmarkScene==2){_sampleBenchmark.reset();mclog::tagInfo("Soft3DBench","sample workspace released psram_free={}",heap_caps_get_free_size(MALLOC_CAP_SPIRAM));}
        }
        GetHAL().feedTheDog();GetHAL().delay(1);return;
    }
    benchmarkMuseumFrame();
}
void AppGundamMuseum::benchmarkMuseumFrame(){
    constexpr uint32_t frames=kBenchmarkFrames;auto& display=GetHAL().getDisplay();
    gundam_museum::View view;view.model=gundam_museum::ModelId::Rx78;view.equipment=true;view.pitch=.10f;view.yaw=float(_benchmarkFrame)*6.28318530718f/float(frames);
    const app_performance::DisplayRegion region{0,0,display.width(),display.height()};const uint64_t started=esp_timer_get_time();app_performance::DisplayFrameScope frame(display,region);_renderer.render(display,view,65,true,false,false,true);const uint64_t rendered=esp_timer_get_time();
    uint32_t frameHash=2166136261u;for(int y=0;y<display.height();++y){const auto* row=reinterpret_cast<const uint16_t*>(GetHAL().getDisplayFrameBufferLine(y));for(int x=0;x<display.width();++x)frameHash=(frameHash^row[x])*16777619u;}_benchmarkHashA=(_benchmarkHashA^frameHash)*16777619u;_benchmarkHashB+=frameHash;
    const uint64_t beforePresent=esp_timer_get_time();frame.finish();const uint64_t finished=esp_timer_get_time();const auto& stats=_renderer.stats();
    const auto drawUs=uint32_t(rendered-started),presentUs=uint32_t(finished-beforePresent),cycleUs=drawUs+presentUs;
    _benchmarkDrawSamples[_benchmarkFrame]=drawUs;_benchmarkPresentSamples[_benchmarkFrame]=presentUs;_benchmarkCycleSamples[_benchmarkFrame]=cycleUs;
    _benchmarkDrawUs+=drawUs;_benchmarkPresentUs+=presentUs;_benchmarkAsyncPresentUs+=GetHAL().getDisplayFrameBufferPresentUs();_benchmarkCullUs+=stats.prepareUs;_benchmarkPanelUs+=stats.panelPrepareUs;_benchmarkRasterUs+=stats.rasterUs;_benchmarkMainUs+=stats.mainRasterUs;_benchmarkWorkerUs+=stats.workerRasterUs;_benchmarkBlitUs+=stats.blitUs;_benchmarkBackgroundUs+=stats.backgroundUs;
    if(++_benchmarkFrame==frames){
        const auto p95Cycle=p95(_benchmarkCycleSamples),maxCycle=*std::max_element(_benchmarkCycleSamples.begin(),_benchmarkCycleSamples.end());
        mclog::tagInfo("MuseumBatch","batch={} draw={} draw_p95={} present={} present_p95={} cycle={} cycle_p95={} cycle_max={} grade={} hash={}:{}",_benchmarkBatch++,uint32_t(_benchmarkDrawUs/frames),p95(_benchmarkDrawSamples),uint32_t(_benchmarkPresentUs/frames),p95(_benchmarkPresentSamples),uint32_t((_benchmarkDrawUs+_benchmarkPresentUs)/frames),p95Cycle,maxCycle,gradeName(soft3d::classifyFrameRate(p95Cycle)),_benchmarkHashA,_benchmarkHashB);
        mclog::tagInfo("MuseumBatchStages","panel_tx={} background={} cull={} panel={} raster={} main={} worker={} blit={}",uint32_t(_benchmarkAsyncPresentUs/frames),uint32_t(_benchmarkBackgroundUs/frames),uint32_t(_benchmarkCullUs/frames),uint32_t(_benchmarkPanelUs/frames),uint32_t(_benchmarkRasterUs/frames),uint32_t(_benchmarkMainUs/frames),uint32_t(_benchmarkWorkerUs/frames),uint32_t(_benchmarkBlitUs/frames));
        mclog::tagInfo("MuseumBatchMemory","internal_free={} internal_min={} internal_largest={} psram_free={} psram_min={} psram_largest={} main_stack_free={} worker_stack_free={}",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),uint32_t(uxTaskGetStackHighWaterMark(nullptr)),stats.workerStackFree);
        if(!_benchmarkLifecycleChecked) {
            const auto before=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);_renderer.close();
            const auto released=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);const bool reopened=_renderer.open();
            const auto restored=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            mclog::tagInfo("Soft3DLifecycle","before_close={} after_close={} released={} reopened={} after_reopen={} retained_delta={}",before,released,released-before,reopened,restored,int32_t(before)-int32_t(restored));
            _benchmarkLifecycleChecked=true;
        }
        _benchmarkFrame=0;_benchmarkHashA=2166136261u;_benchmarkHashB=0;_benchmarkDrawUs=_benchmarkPresentUs=_benchmarkAsyncPresentUs=_benchmarkCullUs=_benchmarkPanelUs=_benchmarkRasterUs=_benchmarkMainUs=_benchmarkWorkerUs=_benchmarkBlitUs=_benchmarkBackgroundUs=0;
    }
    GetHAL().feedTheDog();GetHAL().delay(1);
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
    _perfRasterUs+=stats.rasterUs;_perfBlitUs+=stats.blitUs;_perfOverlayUs+=stats.overlayUs;
    _perfBackgroundUs+=stats.backgroundUs;_perfSpaceUs+=stats.spaceUs;
    _perfDepthClearUs+=stats.depthClearUs;
    _perfPanelPrepareUs+=stats.panelPrepareUs;_perfSpaceWaitUs+=stats.spaceWaitUs;
    _perfMainRasterUs+=stats.mainRasterUs;_perfWorkerRasterUs+=stats.workerRasterUs;
    _perfPeakUs=std::max(_perfPeakUs,frameUs);
    const uint32_t finishedMs=GetHAL().millis();
    if(finishedMs-_perfStarted>=kPerformanceWindowMs){
        const uint32_t elapsed=finishedMs-_perfStarted;
        const uint32_t frames=std::max<uint32_t>(1,_perfFrames);
        mclog::tagInfo("MuseumPerf","model={} frames={} fps_x10={} scale65={} scale100={} draw_us={} present_us={} peak_us={}",
            int(_controller.view().model),_perfFrames,_perfFrames*10000u/elapsed,
            _perfFrames65,_perfFrames100,uint32_t(_perfDrawUs/frames),
            uint32_t(_perfPresentUs/frames),_perfPeakUs);
        mclog::tagInfo("MuseumStageAvg","clear_us={} cull_us={} project_raster_us={} blit_us={} overlay_us={} stack={}",
            uint32_t(_perfClearUs/frames),uint32_t(_perfCullUs/frames),
            uint32_t(_perfRasterUs/frames),uint32_t(_perfBlitUs/frames),
            uint32_t(_perfOverlayUs/frames),
            uint32_t(uxTaskGetStackHighWaterMark(nullptr)));
        mclog::tagInfo("MuseumStageDetail","background_us={} space_work_us={} depth_clear_us={}",
            uint32_t(_perfBackgroundUs/frames),uint32_t(_perfSpaceUs/frames),
            uint32_t(_perfDepthClearUs/frames));
        mclog::tagInfo("MuseumStageParallel","panel_prepare_us={} space_wait_us={} main_raster_us={} worker_raster_us={}",
            uint32_t(_perfPanelPrepareUs/frames),uint32_t(_perfSpaceWaitUs/frames),
            uint32_t(_perfMainRasterUs/frames),uint32_t(_perfWorkerRasterUs/frames));
        mclog::tagInfo("MuseumMemory","internal_free={} internal_min={} internal_largest={} psram_free={} psram_min={} psram_largest={} projected_internal_bytes={} depth_internal_bytes={} command_internal_bytes={} worker_stack_free={}",
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
            heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),stats.internalProjectedBytes,stats.internalDepthBytes,
            stats.internalCommandBytes,
            stats.workerStackFree);
        resetPerformanceWindow(finishedMs);
    }
}
void AppGundamMuseum::resetPerformanceWindow(uint32_t now){
    _perfStarted=now;_perfFrames=0;_perfFrames65=0;_perfFrames100=0;_perfPeakUs=0;
    _perfDrawUs=0;_perfPresentUs=0;_perfClearUs=0;_perfCullUs=0;_perfRasterUs=0;_perfBlitUs=0;_perfOverlayUs=0;
    _perfBackgroundUs=0;_perfSpaceUs=0;_perfDepthClearUs=0;
    _perfPanelPrepareUs=0;_perfSpaceWaitUs=0;_perfMainRasterUs=0;_perfWorkerRasterUs=0;
}
void AppGundamMuseum::onClose(){
    _sampleBenchmark.reset();
    if(_direct)GetHAL().setDisplayFrameBufferAsync(false);
    _input.close();_renderer.close();_controller.reset();_direct=false;_presented=false;
    if(_resumeWifi)WifiManager::GetInstance().StartStation();
    _resumeWifi=false;
    GetHAL().startLvglUpdate();
}
