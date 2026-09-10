// Temporary factorial experiment; removed from normal application builds.
#include "garage_renderer.h"
#include "race_renderer.h"
#include <hal/hal.h>
#include <esp_timer.h>
#include <esp_rom_crc.h>
#include <esp_heap_caps.h>
#include <mooncake_log.h>
void raceDeviceBenchmark() {
    using namespace lets_and_go;
    GetHAL().stopLvglUpdate();
    auto garage=std::make_unique<GarageRenderer>();auto renderer=std::make_unique<RaceRenderer>();
    auto& canvas=GetHAL().getCanvas();constexpr unsigned pixels=468u*466u;
    if(canvas.width()!=468 || canvas.height()!=466) {
        GetHAL().startLvglUpdate();mclog::tagInfo("RaceAB","ABORT unsupported canvas");return;
    }
    garage->open(canvas.width(),canvas.height());
    renderer->open(canvas.width(),canvas.height(),true,true,true,false);
    auto before=std::make_unique<uint16_t[]>(pixels),uv=std::make_unique<uint16_t[]>(pixels);
    auto race=std::make_unique<RaceController>();ResultsSelection results;
    unsigned copyMismatches=0,uvChangedFrames=0;uint64_t uvChangedPixels=0;
    mclog::tagInfo("RaceAB","BEGIN half={} internal={}",renderer->halfResolutionActive(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    for(unsigned car=0;car<kCarCount;++car)for(auto track:{TrackId::SkyLoop,TrackId::TriCross}) {
        GameFlow flow;flow.useDeviceControls();flow.selectPlayerCar(static_cast<CarId>(car));
        flow.confirmPlayerCar();flow.completeCarShowcase();
        flow.toggleRival(static_cast<CarId>((car+1)%kCarCount));flow.toggleRival(static_cast<CarId>((car+2)%kCarCount));
        flow.confirmRivals();flow.selectTrack(track);flow.confirmTrack();flow.completeGridIntro();flow.completeCountdown();
        race->prepare(flow.setup(),42);
        std::array<std::array<uint32_t,24>,4> totals{};
        uint64_t draw[4]{},present[4]{},road[4]{},player[4]{},opponents[4]{},scale[4]{},hud[4]{};
        for(int frame=0;frame<25;++frame) {
            auto& state=const_cast<RaceSnapshot&>(race->snapshot());
            const float distance=frame*race->track().length()/24;
            unsigned rival=0;
            for(unsigned i=0;i<state.carCount;++i) {
                auto& c=state.cars[i];c.motion.distance=distance;c.motion.speed=12.f;
                if(!c.player) {c.motion.distance+=(rival==0 ? .5f : -.25f);c.motion.lateralOffset=(rival++==0 ? .55f : -.55f);}
            }
            uint32_t hashes[4]{};
            for(int pass=0;pass<4;++pass) {
                const int variant=(frame+pass)%4;
                renderer->setBulkSceneCopy(variant&1);renderer->setSharedUvReciprocal(variant&2);
                const auto start=esp_timer_get_time();
                renderer->render(flow,*race,results,frame*33,false,PencilDetail::Low,true);
                const auto drawn=esp_timer_get_time();GetHAL().updateCanvas();const auto end=esp_timer_get_time();
                if(frame) {
                    totals[variant][frame-1]=uint32_t(end-start);draw[variant]+=drawn-start;present[variant]+=end-drawn;
                    const auto stages=renderer->stages();road[variant]+=stages.trackUs;player[variant]+=stages.playerUs;
                    opponents[variant]+=stages.opponentsUs;scale[variant]+=stages.upscaleUs;hud[variant]+=stages.hudUs;
                }
                const auto* image=static_cast<const uint16_t*>(canvas.getBuffer());
                hashes[variant]=esp_rom_crc32_le(0,reinterpret_cast<const uint8_t*>(image),pixels*2);
                std::memcpy(variant&2 ? uv.get() : before.get(),image,pixels*2);
                GetHAL().feedTheDog();GetHAL().delay(1);
            }
            if(hashes[0]!=hashes[1] || hashes[2]!=hashes[3])++copyMismatches;
            if(hashes[0]!=hashes[2]) {
                ++uvChangedFrames;for(unsigned i=0;i<pixels;++i)uvChangedPixels+=before[i]!=uv[i];
            }
        }
        for(int v=0;v<4;++v) {
            auto& values=totals[v];std::sort(values.begin(),values.end());
            mclog::tagInfo("RaceAB","car={} track={} variant={} n=24 draw_us={} present_us={} p95_us={} max_us={} road_us={} player_us={} opponents_us={} upscale_us={} hud_us={}",
                car,int(track),v,draw[v]/24,present[v]/24,values[22],values[23],road[v]/24,player[v]/24,opponents[v]/24,scale[v]/24,hud[v]/24);
        }
        mclog::tagInfo("RaceAB","pixels car={} track={} copy_mismatches={} uv_changed_frames={} uv_changed_pixels={}",car,int(track),copyMismatches,uvChangedFrames,uvChangedPixels);
    }
    renderer->close();garage->close();GetHAL().startLvglUpdate();
    mclog::tagInfo("RaceAB","END copy_mismatches={} uv_changed_frames={} uv_changed_pixels={}",copyMismatches,uvChangedFrames,uvChangedPixels);
}
