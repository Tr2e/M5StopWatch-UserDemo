// Temporary on-device A/B harness; removed from the normal firmware after use.
#include "garage_renderer.h"
#include "phase2_reference.h"
#include "race_renderer.h"
#include "../controller/inspection_presentation.h"
#include <hal/hal.h>
#include <esp_timer.h>
#include <esp_rom_crc.h>
#include <esp_heap_caps.h>
#include <mooncake_log.h>

void phase2DeviceBenchmark() {
    using namespace lets_and_go;
    GetHAL().stopLvglUpdate();
    auto& canvas=GetHAL().getCanvas();
    const int width=canvas.width(),height=canvas.height();
    auto current=std::make_unique<GarageRenderer>();
    auto reference=std::make_unique<ReferenceGarageRenderer>();
    auto race=std::make_unique<RaceRenderer>();
    current->open(width,height);
    race->open(width,height,true,true,true,false);
    reference->open(width,height);
    GameFlow flow;GarageSelection selection;
    flow.useDeviceControls();flow.inspectCar();
    unsigned mismatches=0;
    mclog::tagInfo("InspectionAB","BEGIN width={} height={} free_internal={}",width,height,
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    for(unsigned car=0;car<kCarCount;++car)for(int percent:{80,100}) {
        selection.reset(static_cast<CarId>(car));
        std::array<uint32_t,12> before{},after{};
        uint64_t transferUs=0,partialUs=0;
        for(int frame=0;frame<13;++frame) {
            GarageViewState pose;pose.yaw+=frame*.12f;pose.pitch+=frame*.015f;
            uint32_t hashes[2]{};
            for(int pass=0;pass<2;++pass) {
                const int variant=(pass+frame)%2;
                const auto start=esp_timer_get_time();
                if(variant)current->render(flow,selection,frame*33,PencilDetail::High,{},pose,true,percent);
                else reference->render(flow,selection,frame*33,PencilDetail::High,{},pose,true,percent);
                const auto elapsed=esp_timer_get_time()-start;
                if(frame)(variant ? after : before)[frame-1]=uint32_t(elapsed);
                hashes[variant]=esp_rom_crc32_le(0,static_cast<const uint8_t*>(canvas.getBuffer()),width*height*2);
                GetHAL().feedTheDog();GetHAL().delay(1);
            }
            if(hashes[0]!=hashes[1])++mismatches;
            for(int pass=0;pass<2;++pass) {
                const auto start=esp_timer_get_time();
                if((pass+frame)%2) {
                    const auto region=inspectionRefreshRegion(width);
                    GetHAL().updateCanvasRegion(region.x,region.y,region.width,region.height);
                    if(frame)partialUs+=esp_timer_get_time()-start;
                } else {
                    GetHAL().updateCanvas();
                    if(frame)transferUs+=esp_timer_get_time()-start;
                }
            }
        }
        uint64_t beforeUs=0,afterUs=0;
        for(auto v:before)beforeUs+=v;
        for(auto v:after)afterUs+=v;
        std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
        mclog::tagInfo("InspectionAB","car={} pct={} frames=12 before_us={} after_us={} before_max={} after_max={} transfer_us={} partial_us={} mismatches={}",
            car,percent,beforeUs/12,afterUs/12,before.back(),after.back(),transferUs/12,partialUs/12,mismatches);
    }
    reference->close();current->close();race->close();
    GetHAL().startLvglUpdate();
    mclog::tagInfo("InspectionAB","END mismatches={}",mismatches);
}
