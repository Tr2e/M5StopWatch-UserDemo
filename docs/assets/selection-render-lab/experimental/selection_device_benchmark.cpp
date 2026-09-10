// Temporary paired selection rendering experiment; remove before delivery.
#include "garage_renderer.h"
#include "race_renderer.h"
#include "../controller/inspection_presentation.h"
#include <hal/hal.h>
#include <esp_timer.h>
#include <esp_rom_crc.h>
#include <mooncake_log.h>
void selectionDeviceBenchmark() {
    using namespace lets_and_go;GetHAL().stopLvglUpdate();auto& canvas=GetHAL().getCanvas();
    auto renderer=std::make_unique<GarageRenderer>();auto race=std::make_unique<RaceRenderer>();
    renderer->open(canvas.width(),canvas.height());race->open(canvas.width(),canvas.height(),true,true,true,false);
    GameFlow flow;flow.useDeviceControls();GarageSelection selection;GarageViewController motion;
    const auto region=selectionRefreshRegion(canvas.width());unsigned nativeChecks=0,mismatches=0;
    mclog::tagInfo("SelectionAB","BEGIN");
    for(unsigned car=0;car<kCarCount;++car) {
        selection.reset(static_cast<CarId>(car));motion.reset(static_cast<CarId>(car),0);
        for(int frame=0;frame<20;++frame) {
            const uint32_t now=1000+frame*100;
            if(frame>=1 && frame<=5)motion.drag({1,frame*24,frame%2 ? 32 : -12,true,true},now);
            if(frame==6)motion.endDrag(now);
            if(frame==11)motion.changeView(1,now);
            const auto view=motion.state(now);const int percent=motion.renderPercent(now);
            uint32_t crc[3]{},draw[3]{},present[3]{};
            for(int pass=0;pass<3;++pass) {
                const int mode=(frame+pass)%3;renderer->setSelectionOptimizations(mode!=0);
                const bool partial=frame>0 && mode!=0;
                const auto start=esp_timer_get_time();
                renderer->render(flow,selection,now,PencilDetail::High,{},view,true,mode==2 ? percent : 100,partial);
                const auto drawn=esp_timer_get_time();
                if(partial)GetHAL().updateCanvasRegion(region.x,region.y,region.width,region.height);else GetHAL().updateCanvas();
                const auto end=esp_timer_get_time();draw[mode]=drawn-start;present[mode]=end-drawn;
                crc[mode]=esp_rom_crc32_le(0,static_cast<const uint8_t*>(canvas.getBuffer()),canvas.width()*canvas.height()*2);
                GetHAL().feedTheDog();GetHAL().delay(1);
            }
            ++nativeChecks;mismatches+=crc[0]!=crc[1];
            if(percent==100){++nativeChecks;mismatches+=crc[0]!=crc[2];}
            if(frame)for(int mode=0;mode<3;++mode)
                mclog::tagInfo("SelectionAB","car={} frame={} mode={} percent={} draw={} present={}",car,frame,mode,mode==2 ? percent : 100,draw[mode],present[mode]);
            motion.presented(now);
        }
        mclog::tagInfo("SelectionAB","GROUP car={} native_checks={} mismatches={}",car,nativeChecks,mismatches);
    }
    mclog::tagInfo("SelectionAB","END native_checks={} mismatches={}",nativeChecks,mismatches);
    renderer->close();race->close();GetHAL().startLvglUpdate();
}
