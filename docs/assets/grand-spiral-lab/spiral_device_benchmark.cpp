// Temporary device experiment; removed before delivering the normal app.
#include "garage_renderer.h"
#include "race_renderer.h"
#include <hal/hal.h>
#include <esp_timer.h>
#include <esp_rom_crc.h>
#include <esp_heap_caps.h>
#include <mooncake_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
void spiralDeviceBenchmark() {
    using namespace lets_and_go;
    GetHAL().stopLvglUpdate();auto& canvas=GetHAL().getCanvas();
    auto garage=std::make_unique<GarageRenderer>();auto renderer=std::make_unique<RaceRenderer>();
    garage->open(canvas.width(),canvas.height());renderer->open(canvas.width(),canvas.height(),true,true,true,false);
    renderer->setEdgeUpscale(true);
    auto race=std::make_unique<RaceController>();ResultsSelection results;GarageSelection selection;
    unsigned mismatches=0;
    mclog::tagInfo("SpiralAB","BEGIN edge={} internal={} psram={}",renderer->edgeUpscaleActive(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    for(unsigned car=0;car<kCarCount;++car)for(auto track:{TrackId::SkyLoop,TrackId::TriCross,TrackId::GrandSpiral}) {
        GameFlow flow;flow.useDeviceControls();flow.selectPlayerCar(static_cast<CarId>(car));flow.confirmPlayerCar();flow.completeCarShowcase();
        flow.toggleRival(static_cast<CarId>((car+1)%kCarCount));flow.toggleRival(static_cast<CarId>((car+2)%kCarCount));
        flow.confirmRivals();flow.selectTrack(track);
        if(car==0)for(unsigned frame=0;frame<13;++frame) for(int pass=0;pass<2;++pass) {
            const int mode=(frame+pass)%2;garage->setTrackPreviewDecorations(mode==0);
            const auto start=esp_timer_get_time();garage->render(flow,selection,frame*2454,PencilDetail::High,{}, {},true);
            const auto drawn=esp_timer_get_time();GetHAL().updateCanvas();const auto end=esp_timer_get_time();
            if(frame)mclog::tagInfo("SpiralPreview","track={} frame={} mode={} draw={} present={}",int(track),frame,mode,uint32_t(drawn-start),uint32_t(end-drawn));
            GetHAL().feedTheDog();GetHAL().delay(1);
        }
        flow.confirmTrack();flow.completeGridIntro();flow.completeCountdown();race->prepare(flow.setup(),42);
        for(int frame=0;frame<13;++frame) {
            auto& state=const_cast<RaceSnapshot&>(race->snapshot());unsigned rival=0;
            for(unsigned i=0;i<state.carCount;++i) {
                auto& c=state.cars[i];c.motion.distance=frame*race->track().length()/12;c.motion.speed=12.f;
                if(!c.player) {c.motion.distance+=(rival==0 ? .5f : frame%4==3 ? -2.2f : -.25f);c.motion.lateralOffset=(rival++==0 ? .55f : -.55f);}
            }
            uint32_t checks[2]{},draw[2]{},present[2]{};RaceRenderStages stages[2]{};
            for(int pass=0;pass<2;++pass) {
                const int mode=(frame+pass)%2;renderer->setTrackCulling(mode==1);
                const auto start=esp_timer_get_time();renderer->render(flow,*race,results,frame*33,false,PencilDetail::Low,true);
                const auto drawn=esp_timer_get_time();GetHAL().updateCanvas();const auto end=esp_timer_get_time();
                draw[mode]=drawn-start;present[mode]=end-drawn;stages[mode]=renderer->stages();
                checks[mode]=esp_rom_crc32_le(0,static_cast<const uint8_t*>(canvas.getBuffer()),canvas.width()*canvas.height()*2);
                GetHAL().feedTheDog();GetHAL().delay(1);
            }
            mismatches+=checks[0]!=checks[1];
            if(frame) for(int mode=0;mode<2;++mode) {
                const auto& s=stages[mode];
                mclog::tagInfo("SpiralAB","car={} track={} frame={} mode={} draw={} present={} road={} faces={} upscale={} player={} opponents={}",
                    car,int(track),frame,mode,draw[mode],present[mode],s.trackUs,s.roadSurfaces,s.upscaleUs,s.playerUs,s.opponentsUs);
            }
        }
        mclog::tagInfo("SpiralAB","GROUP car={} track={} mismatches={} stack_free={}",car,int(track),mismatches,uxTaskGetStackHighWaterMark(nullptr));
    }
    mclog::tagInfo("SpiralAB","END mismatches={} internal={} psram={} stack_free={}",mismatches,heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM),uxTaskGetStackHighWaterMark(nullptr));
    renderer->close();garage->close();GetHAL().startLvglUpdate();
}
