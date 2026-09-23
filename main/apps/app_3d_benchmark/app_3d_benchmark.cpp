#include "app_3d_benchmark.h"

#include "../app_gundam_museum/view/museum_renderer.h"
#include "../common/network/wifi_service.h"
#include "../common/performance/display_frame_scope.h"
#include "../common/soft3d/asset/builtin_samples.h"
#include "../common/soft3d/raster/surface_raster.h"
#include "../common/soft3d/runtime/scratch.h"
#include <assets/assets.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <wifi_manager.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <new>

namespace {
constexpr int kSampleSide=424;
constexpr uint32_t kWarmupMs=300;
constexpr float kPi=3.14159265359f;
constexpr uint32_t kRxDurationMs=10000;
#if STOPWATCH_BENCHMARK_AUTORUN
constexpr uint8_t kAuditPassCount=3;
#endif
constexpr std::array<uint32_t,4> kStageDurationMs{{3000,3000,3000,kRxDurationMs}};
constexpr std::array<const char*,4> kStageNames{{
    "01 CUBE PAIR","02 RIGID ASSEMBLY","03 MIXED LOAD","04 RX-78 FINAL"
}};
constexpr std::array<const char*,4> kStageShortNames{{"CUBES","CHAINS","MIXED","RX-78"}};
constexpr std::array<uint16_t,6> kCubePaletteA{{
    0x2e9f,0xfd20,0xf34d,0x435f,0xef7d,0xffff
}};
constexpr std::array<uint16_t,6> kCubePaletteB{{
    0xfd20,0x435f,0xef7d,0xf34d,0x2e9f,0xbdf7
}};
constexpr std::array<std::array<uint16_t,8>,4> kChainPalettes{{
    {{0x2e9f,0x435f,0xef7d,0xffff,0x2e9f,0x435f,0xef7d,0xffff}},
    {{0xfd20,0xf34d,0xef7d,0xbdf7,0xfd20,0xf34d,0xef7d,0xbdf7}},
    {{0xf34d,0x2e9f,0xfd20,0xffff,0xf34d,0x2e9f,0xfd20,0xffff}},
    {{0x435f,0xef7d,0x2e9f,0xfd20,0x435f,0xef7d,0x2e9f,0xfd20}}
}};
constexpr std::array<std::array<float,2>,12> kPitchRoll{{
    {{.30f,.20f}},{{-.24f,.42f}},
    {{.18f,.22f}},{{-.28f,-.18f}},{{.34f,-.30f}},{{-.16f,.28f}},
    {{.26f,.18f}},{{-.26f,-.20f}},{{-.20f,.34f}},{{.16f,.24f}},
    {{.32f,-.28f}},{{-.34f,.22f}}
}};

struct FixedOrientation {float cp=1,sp=0,cr=1,sr=0;};

auto makeColorCubeAsset(const std::array<uint16_t,6>& palette) {
    using Storage=soft3d::ModelAssetStorage<8,6,6>;
    struct Result {Storage storage;soft3d::AssetError error=soft3d::AssetError::None;};
    Result result{};
    constexpr float l=-.5f,r=.5f,b=-.5f,t=.5f,k=-.5f,f=.5f;
    const std::array<soft3d::samples::SamplePanel,6> panels{{
        {{{{l,b,f},{r,b,f},{r,t,f},{l,t,f}}},palette[0]},
        {{{{r,b,k},{l,b,k},{l,t,k},{r,t,k}}},palette[1]},
        {{{{l,b,k},{l,b,f},{l,t,f},{l,t,k}}},palette[2]},
        {{{{r,b,f},{r,b,k},{r,t,k},{r,t,f}}},palette[3]},
        {{{{l,t,f},{r,t,f},{r,t,k},{l,t,k}}},palette[4]},
        {{{{l,b,k},{r,b,k},{r,b,f},{l,b,f}}},palette[5]}
    }};
    result.error=soft3d::buildPanelAsset(result.storage,panels.size(),[&](std::size_t i) {
        return soft3d::samples::SamplePanelView{&panels[i]};
    },{"benchmark_color_cube",soft3d::AssetDeterministicOrder|soft3d::AssetAllSolid,
       soft3d::AssetProfile::SolidStatic,-.5f});
    return result;
}

auto makeColorChainAsset(const std::array<uint16_t,8>& palette) {
    using Storage=soft3d::ModelAssetStorage<64,48,8,1,8>;
    struct Result {Storage storage;soft3d::AssetError error=soft3d::AssetError::None;};
    Result result{};auto panels=std::unique_ptr<std::array<soft3d::samples::SamplePanel,48>>(
        new(std::nothrow) std::array<soft3d::samples::SamplePanel,48>{});
    if(!panels){result.error=soft3d::AssetError::MissingData;return result;}
    std::size_t count=0;
    for(uint16_t bone=0;bone<8;++bone) {
        const float x=float(bone)*.32f-1.12f,l=x-.14f,r=x+.14f,b=.15f,t=.75f,k=-.14f,f=.14f;
        const std::array<std::array<soft3d::Vec3,4>,6> faces{{
            {{{l,b,f},{r,b,f},{r,t,f},{l,t,f}}},{{{r,b,k},{l,b,k},{l,t,k},{r,t,k}}},
            {{{l,b,k},{l,b,f},{l,t,f},{l,t,k}}},{{{r,b,f},{r,b,k},{r,t,k},{r,t,f}}},
            {{{l,t,f},{r,t,f},{r,t,k},{l,t,k}}},{{{l,b,k},{r,b,k},{r,b,f},{l,b,f}}}
        }};
        for(const auto& face:faces)(*panels)[count++]={face,palette[bone],bone,false};
    }
    result.error=soft3d::buildPanelAsset(result.storage,count,[&](std::size_t i) {
        return soft3d::samples::SamplePanelView{&(*panels)[i]};
    },{"benchmark_rigid_chain",soft3d::AssetDeterministicOrder|soft3d::AssetAllSolid|
       soft3d::AssetRigidSkeleton,soft3d::AssetProfile::SolidRigid,.15f});
    result.storage.boneCount=8;
    for(unsigned i=0;i<8;++i)
        result.storage.bones[i]={int16_t(i?i-1:-1),uint16_t(0x100u+i),{float(i)*.32f-1.12f,.45f,0}};
    result.storage.seal("benchmark_rigid_chain",result.storage.asset.bounds,
        soft3d::AssetDeterministicOrder|soft3d::AssetAllSolid|soft3d::AssetRigidSkeleton,
        soft3d::AssetProfile::SolidRigid);
    result.error=soft3d::validate(result.storage.asset);
    return result;
}

}

using BenchmarkOccupancy=std::array<uint8_t,(kSampleSide*kSampleSide+7)/8>;
struct BenchmarkHotState {
    std::array<soft3d::BoneTransform,8> chainPose{};
    std::array<FixedOrientation,kPitchRoll.size()> fixedOrientation{};
    std::array<soft3d::ModelInstance,16> instances{};
    soft3d::RigidRenderScratch<64,8> renderScratch{};
};

struct BenchmarkSurface {
    decltype(makeColorCubeAsset(kCubePaletteA)) cubeA=makeColorCubeAsset(kCubePaletteA);
    decltype(makeColorCubeAsset(kCubePaletteA)) cubeB=makeColorCubeAsset(kCubePaletteB);
    std::array<decltype(makeColorChainAsset(kChainPalettes[0])),4> chains{{
        makeColorChainAsset(kChainPalettes[0]),makeColorChainAsset(kChainPalettes[1]),
        makeColorChainAsset(kChainPalettes[2]),makeColorChainAsset(kChainPalettes[3])
    }};
    soft3d::SurfaceRaster<kSampleSide,kSampleSide> raster;
    BenchmarkOccupancy occupiedDepth{};
    soft3d::Scratch<BenchmarkOccupancy> fastOccupiedDepth;
    BenchmarkHotState hotStateFallback{};
    soft3d::Scratch<BenchmarkHotState> fastHotState;
    BenchmarkHotState& hotState(){return fastHotState.get()?*fastHotState.get():hotStateFallback;}
    BenchmarkSurface() {
        // Occupancy participates in per-pixel clear/composite work, so it gets
        // first claim on scarce internal RAM. Instance/cache state is second.
        const bool internalOccupancy=fastOccupiedDepth.allocate();
        const bool internalHotState=fastHotState.allocate();
        raster.setSolidFastPath(true);raster.setSolidSpanFastPath(true);
        raster.setTrustedSolidDepthFastPath(true);raster.setSolidQuadFastPath(true);
        raster.setDirectSpanFastPath(true);raster.setNativeFrameBufferFastPath(true);
        raster.setSparseCompositeFastPath(true);
        auto* occupied=fastOccupiedDepth.get();
        raster.setSparseDepthStorage(occupied?occupied->data():occupiedDepth.data(),occupiedDepth.size());
        raster.setSparseDepthClearFastPath(true);raster.setSparseDepthSpanClearFastPath(true);
        auto& hot=hotState();
        for(std::size_t i=0;i<hot.fixedOrientation.size();++i) {
            const float pitch=kPitchRoll[i][0],roll=kPitchRoll[i][1];
            hot.fixedOrientation[i]={std::cos(pitch),std::sin(pitch),
                                     std::cos(roll),std::sin(roll)};
        }
        for(std::size_t i=0;i<hot.chainPose.size();++i) {
            hot.chainPose[i].value={{1,0,0,0,0,1,0,(i&1u)?.10f:0.f,0,0,1,0}};
        }
        bool invalid=cubeA.error!=soft3d::AssetError::None || cubeB.error!=soft3d::AssetError::None;
        for(const auto& chain:chains)invalid=invalid || chain.error!=soft3d::AssetError::None;
        if(invalid)mclog::tagError("3DBenchAssets","immutable asset construction failed");
        mclog::tagInfo("3DBenchMemory","occupancy_internal={} hot_state_internal={}",
                       internalOccupancy,internalHotState);
    }
};

App3DBenchmark::App3DBenchmark() {
    setAppInfo().name="3D Benchmark";setAppInfo().icon=(void*)&icon_vector_run;
}
App3DBenchmark::~App3DBenchmark()=default;

void App3DBenchmark::onOpen() {
    GetHAL().stopLvglUpdate();GetHAL().lvglLock();GetHAL().lvglUnlock();
    auto& wifi=WifiManager::GetInstance();
    _resumeWifi=network::WifiService::GetInstance().getStatus().has_saved_network && !wifi.IsConfigMode();
    if(_resumeWifi)wifi.StopStation();
    _direct=GetHAL().hasDisplayFrameBuffer();
    if(_direct && !GetHAL().setDisplayFrameBufferAsync(true))
        mclog::tagWarn("3DBench","async framebuffer unavailable; using synchronous present");
#if STOPWATCH_BENCHMARK_AUTORUN
    constexpr uint32_t iterations=20000;
    volatile int64_t timerSink=0;
    const auto timerStarted=esp_timer_get_time();
    for(uint32_t i=0;i<iterations;++i)timerSink+=esp_timer_get_time();
    const auto timerElapsed=esp_timer_get_time()-timerStarted;
    mclog::tagInfo("3DBenchAudit","timer_calls={} elapsed_us={} ns_per_call={} sink={}",
        iterations,timerElapsed,uint32_t(timerElapsed*1000/iterations),timerSink!=0);
#endif
    resetRun();
}

void App3DBenchmark::resetRun() {
    _results={};_resultsScreen=false;_resultsDrawn=false;
    enterStage(0);
}

void App3DBenchmark::enterStage(uint8_t stage) {
    _stage=stage;_lastTotalTriangles=0;_lastRenderedTriangles=0;
    _lastTotalQuads=0;_lastRenderedQuads=0;_stageEndpointDrawn=false;_lastCompletedUs=0;
    if(stage<3) {
        _museum.reset();
        if(!_surface)_surface=std::unique_ptr<BenchmarkSurface>(new(std::nothrow) BenchmarkSurface{});
    } else {
        _surface.reset();
        _museum=std::unique_ptr<gundam_museum::MuseumRenderer>(new(std::nothrow) gundam_museum::MuseumRenderer{});
        if(_museum) {
            if(!_museum->open())_museum.reset();
            else {
                _museum->setSpaceEnabled(false);
                _museum->setBackgroundColor(0x0000);
            }
        }
    }
    // Resource construction is not part of the stage duration or warm-up.
    // Starting here prevents a slow allocation from consuming the 300 ms
    // warm-up and accidentally admitting cold first frames into the result.
    _stageStarted=GetHAL().millis();
    mclog::tagInfo("3DBench","stage={} name={} internal_free={} psram_free={}",stage,kStageNames[stage],
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void App3DBenchmark::finishStage() {
    auto& result=_results[_stage];
    result.averageUs=result.intervals?uint32_t(result.intervalSumUs/result.intervals):0;
    result.averageRenderedTriangles=result.frames
        ?uint32_t((result.renderedTrianglesSum+result.frames/2)/result.frames):0;
    result.averageRenderedQuads=result.frames
        ?uint32_t((result.renderedQuadsSum+result.frames/2)/result.frames):0;
    mclog::tagInfo("3DBenchResult","pass={} render_percent={} topology_stats={} stage={} name={} frames={} intervals={} avg_interval_us={} fps_x10={} total_tri={} rendered_tri={} total_quad={} rendered_quad={}",
        _auditPass,_renderPercent,STOPWATCH_COLLECT_TOPOLOGY_STATS,_stage,kStageNames[_stage],
        result.frames,result.intervals,result.averageUs,
        result.averageUs?10000000u/result.averageUs:0,result.totalTriangles,result.averageRenderedTriangles,
        result.totalQuads,result.averageRenderedQuads);
}

void App3DBenchmark::onRunning() {
    GetHAL().updateButtonStates();const auto event=_keys.update(false);
    if(event==input::KeyEvent::GoHome){close();return;}
    const uint32_t now=GetHAL().millis();
    if(event==input::KeyEvent::GoPrevious){resetRun();return;}
    if(event==input::KeyEvent::GoNext) {
        _renderPercent=_renderPercent==100?60:100;
        resetRun();return;
    }
    if(_resultsScreen) {
        if(!_resultsDrawn){drawFrame(now);_resultsDrawn=true;}
        GetHAL().delay(20);return;
    }
    if(now-_stageStarted>=kStageDurationMs[_stage]) {
        if(_stage==3 && !_stageEndpointDrawn) {
            // Render the exact 2π endpoint once; otherwise the pre-frame
            // duration check would always stop one frame short of a full turn.
            drawFrame(_stageStarted+kStageDurationMs[_stage]);
            _stageEndpointDrawn=true;GetHAL().feedTheDog();GetHAL().delay(1);return;
        }
        finishStage();
        if(_stage+1<kStageCount){enterStage(_stage+1);return;}
#if STOPWATCH_BENCHMARK_AUTORUN
        mclog::tagInfo("3DBenchAudit","run_complete pass={} render_percent={} topology_stats={}",
                       _auditPass,_renderPercent,STOPWATCH_COLLECT_TOPOLOGY_STATS);
        if(_renderPercent==100) {
            _renderPercent=60;resetRun();return;
        }
        if(++_auditPass<kAuditPassCount) {
            _renderPercent=100;resetRun();return;
        }
        mclog::tagInfo("3DBenchAudit","all_complete passes={} topology_stats={}",
                       kAuditPassCount,STOPWATCH_COLLECT_TOPOLOGY_STATS);
#endif
        _museum.reset();_resultsScreen=true;_resultsDrawn=false;
    }
    drawFrame(now);GetHAL().feedTheDog();GetHAL().delay(1);
    if(_resultsScreen)_resultsDrawn=true;
}

void App3DBenchmark::drawFrame(uint32_t now) {
    auto& display=_direct?static_cast<lgfx::LGFXBase&>(GetHAL().getDisplay()):
                          static_cast<lgfx::LGFXBase&>(GetHAL().getCanvas());
    if(_direct) {
        auto& panel=GetHAL().getDisplay();
        app_performance::DisplayFrameScope frame(panel,{0,0,panel.width(),panel.height()});
        if(_resultsScreen)drawResults(panel);
        else if(_stage==3)drawRx78(panel,now);
        else drawProcedural(panel,now);
        frame.finish();
    } else {
        if(_resultsScreen)drawResults(display);
        else if(_stage==3)drawRx78(display,now);
        else drawProcedural(display,now);
        GetHAL().updateCanvas();
    }
    const uint64_t completedUs=esp_timer_get_time();
    if(!_resultsScreen)recordFrame(completedUs,now);
}

void App3DBenchmark::drawProcedural(lgfx::LGFXBase& display,uint32_t now) {
    if(!_surface){display.setTextColor(0xf800,0x0000);display.drawString("RASTER MEMORY ERROR",118,220);return;}
    uint8_t* nativeFrameBuffer=nullptr;std::size_t nativeStride=0;
    int32_t clipX=0,clipY=0,clipW=0,clipH=0;
    display.getClipRect(&clipX,&clipY,&clipW,&clipH);
    const bool nativeLayout=_direct && &display==&GetHAL().getDisplay() &&
        display.getColorDepth()==16 && display.getRotation()==0 &&
        clipX==0 && clipY==0 && clipW==display.width() && clipH==display.height();
    if(nativeLayout) {
        auto* first=GetHAL().getDisplayFrameBufferLine(0);
        auto* second=GetHAL().getDisplayFrameBufferLine(1);
        if(first && second && second>first) {
            nativeFrameBuffer=first;nativeStride=std::size_t(second-first);
        }
    }
    if(nativeFrameBuffer && !(display.width()&1) && !(nativeStride&3) &&
       !(reinterpret_cast<std::uintptr_t>(nativeFrameBuffer)&3) &&
       nativeStride>=std::size_t(display.width())*2) {
        for(int y=0;y<display.height();++y)
            std::fill_n(reinterpret_cast<uint32_t*>(nativeFrameBuffer+std::size_t(y)*nativeStride),
                        display.width()/2,0u);
        GetHAL().markDisplayFrameBufferModified(0,0,display.width(),display.height());
    } else display.fillScreen(0x0000);
    auto& raster=_surface->raster;
    const int outputX=(display.width()-kSampleSide)/2;
    const int outputY=(display.height()-kSampleSide)/2;
    const int internalSide=(kSampleSide*int(_renderPercent)+50)/100;
    const int internalX=(display.width()-internalSide)/2;
    const int internalY=(display.height()-internalSide)/2;
    raster.setDeferredSparseDepthRecord(internalSide==kSampleSide);
    raster.begin(internalX,internalY,internalSide,internalSide);
    soft3d::Camera camera{};camera.principalX=display.width()*.5f;
    camera.principalY=display.height()*.5f;
    camera.focalLength=200.f*float(_renderPercent)/100.f;

    const float seconds=float(now-_stageStarted)*.001f;
    auto& hot=_surface->hotState();auto& instances=hot.instances;std::size_t count=0;
    const auto add=[&](const soft3d::ModelAsset& asset,float x,float y,float z,
                       float yaw,std::size_t fixedIndex,float scale=1.f,bool posed=false) {
        auto& instance=instances[count++];
        float cy=0,sy=0;
#ifdef ESP_PLATFORM
        __builtin_sincosf(yaw,&sy,&cy);
#else
        cy=std::cos(yaw);sy=std::sin(yaw);
#endif
        const auto& orientation=hot.fixedOrientation[fixedIndex];
        const float cp=orientation.cp,sp=orientation.sp,cr=orientation.cr,sr=orientation.sr;
        instance={};instance.asset=&asset;
        instance.world.value={{
            scale*(cr*cy-sr*sp*sy),scale*(-sr*cp),scale*(cr*sy+sr*sp*cy),x,
            scale*(sr*cy+cr*sp*sy),scale*(cr*cp),scale*(sr*sy-cr*sp*cy),y,
            scale*(-cp*sy),scale*sp,scale*(cp*cy),z
        }};
        if(posed)instance.pose={hot.chainPose.data(),hot.chainPose.size()};
    };
    if(_stage==0) {
        // Two cubes with different palette, size, depth, axis, and perspective.
        add(_surface->cubeA.storage.asset,-1.95f,.42f,5.0f, seconds*.76f,0,2.00f);
        add(_surface->cubeB.storage.asset, 2.20f,-.32f,7.1f,-seconds*.51f,1,1.64f);
    } else if(_stage==1) {
        // Four rigid chains with independent palettes, scale, depth, and pose.
        add(_surface->chains[0].storage.asset,-2.10f,1.30f,5.4f,seconds*.55f,2,1.52f,true);
        add(_surface->chains[1].storage.asset,1.95f,1.25f,7.0f,-seconds*.43f,3,1.42f,true);
        add(_surface->chains[2].storage.asset,-1.75f,-1.40f,8.1f,seconds*.36f,4,1.28f,true);
        add(_surface->chains[3].storage.asset,2.15f,-1.25f,6.0f,-seconds*.62f,5,1.46f,true);
    } else {
        // Four cube loads from stage 1 plus two rigid-chain loads from stage 2.
        add(_surface->cubeA.storage.asset,-3.25f,1.55f,5.4f, seconds*.72f,6,1.26f);
        add(_surface->chains[1].storage.asset,.0f,1.55f,6.8f,-seconds*.42f,7,1.02f,true);
        add(_surface->cubeB.storage.asset,3.25f,1.35f,7.6f,-seconds*.48f,8,1.18f);
        add(_surface->chains[0].storage.asset,-2.75f,-1.55f,5.9f,seconds*.50f,9,1.06f,true);
        add(_surface->cubeB.storage.asset,.10f,-1.55f,6.3f,seconds*.60f,10,1.24f);
        add(_surface->cubeA.storage.asset,3.20f,-1.45f,8.0f,-seconds*.40f,11,1.10f);
    }
    const auto work=soft3d::renderSceneCachedIndexed(
        raster,camera,{{instances.data(),count}},hot.renderScratch);
    raster.blitScaled(display,outputX,outputY,kSampleSide,kSampleSide,nativeFrameBuffer,nativeStride);
    if(nativeFrameBuffer)GetHAL().markDisplayFrameBufferModified(outputX,outputY,kSampleSide,kSampleSide);
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
    _lastTotalTriangles=work.totalTriangles;
    _lastRenderedTriangles=work.submittedTriangles;
    _lastTotalQuads=work.totalQuads;
    _lastRenderedQuads=work.submittedQuads;
#endif
}

void App3DBenchmark::drawRx78(lgfx::LGFXBase& display,uint32_t now) {
    if(!_museum){display.fillScreen(0x0842);display.setTextColor(0xf800,0x0842);display.drawString("RX-78 MEMORY ERROR",128,220);return;}
    gundam_museum::View view{};view.equipment=true;
    const float phase=std::min(1.f,float(now-_stageStarted)/float(kStageDurationMs[3]));
    view.yaw=-.75f+phase*2.f*kPi;view.pitch=.1f;
    _museum->render(display,view,_renderPercent,true,false,false,true);
    const auto& stats=_museum->stats();
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
    _lastTotalTriangles=uint32_t(stats.totalTriangles);
    _lastRenderedTriangles=uint32_t(stats.submittedTriangles);
    _lastTotalQuads=uint32_t(stats.totalQuads);
    _lastRenderedQuads=uint32_t(stats.submittedQuads);
#endif
}

void App3DBenchmark::recordFrame(uint64_t completedUs,uint32_t now) {
    if(now-_stageStarted<kWarmupMs)return;
    auto& result=_results[_stage];
    if(_lastCompletedUs) {
        result.intervalSumUs+=completedUs-_lastCompletedUs;
        ++result.intervals;
    }
    _lastCompletedUs=completedUs;++result.frames;
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
    result.totalTriangles=_lastTotalTriangles;
    result.renderedTrianglesSum+=_lastRenderedTriangles;
    result.totalQuads=_lastTotalQuads;
    result.renderedQuadsSum+=_lastRenderedQuads;
#endif
}

void App3DBenchmark::drawResults(lgfx::LGFXBase& display) {
    constexpr uint16_t background=0x0000,rule=0x18c3,muted=0x7bef;
    constexpr uint16_t cyan=0x2e9f,orange=0xfd20,white=0xffff;
    constexpr int rowHeight=72;
    const int centerX=display.width()/2;
    const int totalX=centerX-84,drawX=centerX,fpsX=centerX+84;
    display.fillScreen(background);

    // Keep every label inside the 202 px round-screen safe radius. The first
    // row is deliberately narrow because the circle is tightest near the top.
    char text[80];std::snprintf(text,sizeof(text),"RENDER %u%%",unsigned(_renderPercent));
    display.setTextDatum(textdatum_t::middle_center);display.setTextSize(1);
    display.setTextColor(muted,background);display.drawString(text,centerX,45);
    display.drawFastHLine(centerX-80,58,160,rule);

    for(std::size_t i=0;i<kStageCount;++i) {
        const auto& r=_results[i];const uint32_t fps10=r.averageUs?10000000u/r.averageUs:0;
        const int stageY=73+int(i)*rowHeight;
        const int valueY=stageY+20,labelY=stageY+38,quadY=stageY+51;

        std::snprintf(text,sizeof(text),"%02u  %s",unsigned(i+1),kStageShortNames[i]);
        display.setTextDatum(textdatum_t::middle_center);display.setTextSize(1);
        display.setTextColor(i==3?orange:white,background);display.drawString(text,centerX,stageY);

        std::snprintf(text,sizeof(text),"%lu",(unsigned long)r.totalTriangles);
        display.setTextSize(2);display.setTextColor(cyan,background);display.drawString(text,totalX,valueY);
        std::snprintf(text,sizeof(text),"%lu",(unsigned long)r.averageRenderedTriangles);
        display.setTextColor(orange,background);display.drawString(text,drawX,valueY);
        std::snprintf(text,sizeof(text),"%lu.%lu",(unsigned long)(fps10/10),(unsigned long)(fps10%10));
        display.setTextColor(white,background);display.drawString(text,fpsX,valueY);

        display.setTextSize(1);display.setTextColor(cyan,background);
        display.drawString("TOTAL TRI",totalX,labelY);
        display.setTextColor(orange,background);display.drawString("DRAW TRI",drawX,labelY);
        display.setTextColor(muted,background);display.drawString("FPS",fpsX,labelY);
        std::snprintf(text,sizeof(text),"QUAD %lu",(unsigned long)r.totalQuads);
        display.setTextColor(cyan,background);display.drawString(text,totalX,quadY);
        std::snprintf(text,sizeof(text),"QUAD %lu",(unsigned long)r.averageRenderedQuads);
        display.setTextColor(orange,background);display.drawString(text,drawX,quadY);
        display.drawFastHLine(centerX-140,stageY+64,280,rule);
    }
    display.setTextDatum(textdatum_t::middle_center);display.setTextSize(1);
    display.setTextColor(muted,background);
    display.drawString("A  RESTART       B  100% / 60%",centerX,390);
}

void App3DBenchmark::onClose() {
    _surface.reset();if(_museum){_museum->close();_museum.reset();}
    if(_direct)GetHAL().setDisplayFrameBufferAsync(false);
    if(_resumeWifi)WifiManager::GetInstance().StartStation();
    _resumeWifi=false;_direct=false;GetHAL().startLvglUpdate();
}
