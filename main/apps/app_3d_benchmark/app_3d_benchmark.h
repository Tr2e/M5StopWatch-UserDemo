#pragma once

#include "../common/key_manager/key_manager.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mooncake.h>

struct BenchmarkSurface;
namespace gundam_museum { class MuseumRenderer; }

class App3DBenchmark : public mooncake::AppAbility {
public:
    App3DBenchmark();
    ~App3DBenchmark() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    static constexpr std::size_t kStageCount=4;
    struct StageResult {
        uint64_t intervalSumUs=0;
        uint64_t renderedTrianglesSum=0;
        uint64_t renderedQuadsSum=0;
#if STOPWATCH_BENCHMARK_AUTORUN
        uint64_t clearSumUs=0,beginSumUs=0,renderSumUs=0,blitSumUs=0,presentSumUs=0;
        uint64_t panelPrepareSumUs=0,mainRasterSumUs=0,workerRasterSumUs=0;
        uint32_t internalDepthFrames=0;
        uint32_t fastPathAll=0xffffffffu,fastPathAny=0;
#endif
        uint32_t frames=0,intervals=0,totalTriangles=0,totalQuads=0;
        uint32_t averageUs=0,averageRenderedTriangles=0,averageRenderedQuads=0;
    };

    void resetRun();
    void enterStage(uint8_t stage);
    void finishStage();
    void drawFrame(uint32_t now);
    void drawProcedural(lgfx::LGFXBase& display,uint32_t now);
    void drawRx78(lgfx::LGFXBase& display,uint32_t now);
    void drawResults(lgfx::LGFXBase& display);
    void recordFrame(uint64_t completedUs,uint32_t now);

    std::unique_ptr<BenchmarkSurface> _surface;
    std::unique_ptr<gundam_museum::MuseumRenderer> _museum;
    input::KeyManager _keys;
    std::array<StageResult,kStageCount> _results{};
    bool _direct=false,_resumeWifi=false,_resultsScreen=false,_resultsDrawn=false;
    bool _stageEndpointDrawn=false;
    uint8_t _renderPercent=100;
    uint8_t _stage=0;
    uint8_t _auditPass=0;
    uint32_t _stageStarted=0;
    uint64_t _lastCompletedUs=0;
#if STOPWATCH_BENCHMARK_AUTORUN
    uint32_t _lastClearUs=0,_lastBeginUs=0,_lastRenderUs=0,_lastBlitUs=0,_lastPresentUs=0;
    uint32_t _lastPanelPrepareUs=0,_lastMainRasterUs=0,_lastWorkerRasterUs=0;
    bool _lastInternalDepth=false;
    uint32_t _lastFastPathFlags=0;
#endif
    uint32_t _lastTotalTriangles=0,_lastRenderedTriangles=0;
    uint32_t _lastTotalQuads=0,_lastRenderedQuads=0;
};
