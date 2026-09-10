// Temporary device comparison; excluded from normal firmware.
#include "garage_renderer.h"
#include "race_renderer.h"
#include "../controller/inspection_presentation.h"
#include <hal/hal.h>
#include <esp_timer.h>
#include <mooncake_log.h>
void compactDeviceBenchmark() {
    using namespace lets_and_go;
    GetHAL().stopLvglUpdate();
    auto current=std::make_unique<GarageRenderer>();
    auto race=std::make_unique<RaceRenderer>();
    auto& canvas=GetHAL().getCanvas();
    current->open(canvas.width(),canvas.height());
    race->open(canvas.width(),canvas.height(),true,true,true,false);
    GameFlow flow;flow.useDeviceControls();flow.inspectCar();GarageSelection selection;
    constexpr int percents[]={77,65,82,100},displays[]={100,85,93,100};
    mclog::tagInfo("CompactAB","BEGIN");
    for(unsigned car=0;car<kCarCount;++car) {
        selection.reset(static_cast<CarId>(car));
        uint64_t draws[4]{},presents[4]{};
        std::array<std::array<uint32_t,12>,4> totals{};
        for(int frame=0;frame<13;++frame)for(int pass=0;pass<4;++pass) {
            const int variant=(pass+frame)%4;
            GarageViewState pose;pose.yaw+=frame*.24f;pose.pitch+=frame*.03f;
            const auto start=esp_timer_get_time();
            current->render(flow,selection,frame*33,PencilDetail::High,{},pose,true,
                percents[variant],frame>0,displays[variant]);
            const auto drawn=esp_timer_get_time();
            const auto region=inspectionRefreshRegion(canvas.width());
            GetHAL().updateCanvasRegion(region.x,region.y,region.width,region.height);
            const auto end=esp_timer_get_time();
            if(frame) {
                draws[variant]+=drawn-start;presents[variant]+=end-drawn;
                totals[variant][frame-1]=uint32_t(end-start);
            }
            GetHAL().feedTheDog();GetHAL().delay(1);
        }
        for(int variant=0;variant<4;++variant) {
            auto& times=totals[variant];std::sort(times.begin(),times.end());
            mclog::tagInfo("CompactAB","car={} raster_pct={} display_pct={} n=12 draw_us={} present_us={} p95_us={} max_us={}",
                car,percents[variant],displays[variant],draws[variant]/12,presents[variant]/12,times[11],times[11]);
        }
    }
    current->close();race->close();GetHAL().startLvglUpdate();
    mclog::tagInfo("CompactAB","END");
}
