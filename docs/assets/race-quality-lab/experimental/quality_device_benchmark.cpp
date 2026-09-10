// Temporary player quality/material experiment; removed after measurement.
#include "garage_renderer.h"
#include "race_renderer.h"
#include <hal/hal.h>
#include <esp_timer.h>
#include <esp_rom_crc.h>
#include <esp_heap_caps.h>
#include <mooncake_log.h>
void qualityDeviceBenchmark() {
    using namespace lets_and_go;GetHAL().stopLvglUpdate();auto& canvas=GetHAL().getCanvas();
    auto garage=std::make_unique<GarageRenderer>();auto renderer=std::make_unique<RaceRenderer>();
    garage->open(canvas.width(),canvas.height());renderer->open(canvas.width(),canvas.height(),true,true,true,false);
    auto race=std::make_unique<RaceController>();ResultsSelection results;
    constexpr int percents[]={100,90,85,100,90,85};unsigned restoreMismatches=0;
    mclog::tagInfo("QualityAB","BEGIN half={} internal={} psram={}",renderer->halfResolutionActive(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    for(unsigned car=0;car<kCarCount;++car)for(auto track:{TrackId::SkyLoop,TrackId::TriCross}) {
        GameFlow flow;flow.useDeviceControls();flow.selectPlayerCar(static_cast<CarId>(car));flow.confirmPlayerCar();flow.completeCarShowcase();
        flow.toggleRival(static_cast<CarId>((car+1)%kCarCount));flow.toggleRival(static_cast<CarId>((car+2)%kCarCount));
        flow.confirmRivals();flow.selectTrack(track);flow.confirmTrack();flow.completeGridIntro();flow.completeCountdown();race->prepare(flow.setup(),42);
        std::array<std::array<uint32_t,12>,6> totals{};
        uint64_t draw[6]{},present[6]{},player[6]{},opponents[6]{},road[6]{};uint32_t coldUs=0;
        for(int frame=0;frame<13;++frame) {
            auto& state=const_cast<RaceSnapshot&>(race->snapshot());unsigned rival=0;
            for(unsigned i=0;i<state.carCount;++i) {
                auto& c=state.cars[i];c.motion.distance=frame*race->track().length()/12;c.motion.speed=12.f;
                if(!c.player) {c.motion.distance+=(rival==0 ? .5f : frame%4==3 ? -2.2f : -.25f);c.motion.lateralOffset=(rival++==0 ? .55f : -.55f);}
            }
            uint32_t reference=0;
            for(int pass=0;pass<6;++pass) {
                const int v=(frame+pass)%6;renderer->setPlayerRenderMode(percents[v],v>=3);
                const auto start=esp_timer_get_time();renderer->render(flow,*race,results,frame*33,false,PencilDetail::Low,true);
                const auto drawn=esp_timer_get_time();GetHAL().updateCanvas();const auto end=esp_timer_get_time();
                coldUs=std::max(coldUs,renderer->playerPaintBuildUs());
                if(frame) {totals[v][frame-1]=uint32_t(end-start);draw[v]+=drawn-start;present[v]+=end-drawn;
                    const auto s=renderer->stages();player[v]+=s.playerUs;opponents[v]+=s.opponentsUs;road[v]+=s.trackUs;}
                if(v==0)reference=esp_rom_crc32_le(0,static_cast<const uint8_t*>(canvas.getBuffer()),canvas.width()*canvas.height()*2);
                GetHAL().feedTheDog();GetHAL().delay(1);
            }
            renderer->setPlayerRenderMode(100,false);renderer->render(flow,*race,results,frame*33,false,PencilDetail::Low,true);
            restoreMismatches+=reference!=esp_rom_crc32_le(0,static_cast<const uint8_t*>(canvas.getBuffer()),canvas.width()*canvas.height()*2);
            GetHAL().feedTheDog();GetHAL().delay(1);
        }
        for(int v=0;v<6;++v) {auto& times=totals[v];std::sort(times.begin(),times.end());
            mclog::tagInfo("QualityAB","car={} track={} variant={} raster_pct={} atlas={} n=12 draw_us={} present_us={} p95_us={} max_us={} player_us={} opponents_us={} road_us={}",
                car,int(track),v,percents[v],v>=3,draw[v]/12,present[v]/12,times[11],times[11],player[v]/12,opponents[v]/12,road[v]/12);}
        mclog::tagInfo("QualityAB","cache car={} track={} entries={} fallback_panels={} cold_us={} restore_mismatches={}",car,int(track),renderer->playerPaintCount(),renderer->playerPaintMisses(),coldUs,restoreMismatches);
    }
    mclog::tagInfo("QualityAB","END restore_mismatches={} internal={} psram={}",restoreMismatches,heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    renderer->close();garage->close();GetHAL().startLvglUpdate();
}
