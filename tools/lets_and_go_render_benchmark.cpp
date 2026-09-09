#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_renderer.h"
#include <hal/hal.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace lets_and_go;
template<class F> void measure(const char* name, F render) {
    std::vector<double> samples;
    for (int i = 0; i < 25; ++i) {
        const auto start = std::chrono::steady_clock::now();
        render(i);
        const auto end = std::chrono::steady_clock::now();
        if (i) samples.push_back(std::chrono::duration<double,std::milli>(end-start).count());
    }
    std::sort(samples.begin(), samples.end());
    double sum = 0;
    for (double value : samples) sum += value;
    std::printf("%s mean_ms=%.3f median_ms=%.3f p95_ms=%.3f\n", name,
                sum / samples.size(), samples[samples.size()/2], samples[22]);
}
int main() {
    GarageRenderer garage;
    garage.open(466,466);
    GameFlow flow;
    GarageSelection selection;
    flow.useDeviceControls();
    for (auto detail : {PencilDetail::High, PencilDetail::Low}) {
        measure(detail == PencilDetail::High ? "garage_high" : "garage_low", [&](int frame) {
            GarageViewState view;
            view.yaw += frame * .002f;
            garage.render(flow,selection,frame*33,detail,{},view);
        });
    }
    flow.selectPlayerCar(CarId::CycloneMagnum);
    flow.confirmPlayerCar(); flow.completeCarShowcase();
    flow.toggleRival(CarId::HurricaneSonic);
    flow.confirmRivals(); flow.confirmTrack();
    flow.completeGridIntro(); flow.completeCountdown();
    RaceController race;
    race.prepare(flow.setup(), 0x1234u);
    RaceRenderer renderer;
    renderer.open(466,466);
    ResultsSelection results;
    for (auto detail : {PencilDetail::High, PencilDetail::Low}) {
        race.prepare(flow.setup(),0x1234u);
        measure(detail == PencilDetail::High ? "race_high" : "race_low", [&](int frame) {
            RacerInput input;
            input.valid = true;
            race.advance(input,.033f);
            renderer.render(flow,race,results,frame*33,false,detail);
        });
    }
    // Deterministic nearby opponent over one loop, including bridge clipping.
    // Only the benchmark injects snapshots; production physics is unchanged.
    for (auto detail : {PencilDetail::High, PencilDetail::Low}) {
        race.prepare(flow.setup(),0x1234u);
        measure(detail == PencilDetail::High ? "race_close_high" : "race_close_low", [&](int frame) {
            auto& state=const_cast<RaceSnapshot&>(race.snapshot());
            auto& player=state.cars[state.playerIndex];
            player.motion.distance=frame* race.track().length()/24;
            auto& rival=state.cars[1-state.playerIndex];
            rival.motion.distance=player.motion.distance-.15f;
            rival.motion.lateralOffset=.55f;
            renderer.render(flow,race,results,frame*33,false,detail);
        });
    }
    race.prepare(flow.setup(),0x1234u);
    measure("race_nearclip_low", [&](int frame) {
        auto& state=const_cast<RaceSnapshot&>(race.snapshot());
        auto& player=state.cars[state.playerIndex];
        player.motion.distance=frame*race.track().length()/24;
        auto& rival=state.cars[1-state.playerIndex];
        rival.motion.distance=player.motion.distance-2.2f;
        rival.motion.lateralOffset=.3f;
        renderer.render(flow,race,results,frame*33,false,PencilDetail::Low);
    });
    // Isolate road style at identical cameras and actual half-scene dimensions.
    // This host benchmark compares CPU work only, not ESP32 FPS or SPI transfer.
    LGFX_Sprite scene;scene.createSprite(234,233);
    auto occlusion=std::make_unique<PencilOcclusion>();
    for(auto trackId : {TrackId::SkyLoop,TrackId::TriCross}) {
        OverpassTrack course(trackId);PencilTrack track;track.open(course);
        for(bool wire : {false,true}) {
            const std::string name=std::string("track_")+std::to_string(int(trackId))+
                (wire ? "_wire" : "_solid");
            measure(name.c_str(),[&](int frame) {
                const auto camera=makeRacerChaseCamera(course.sample(course.length()*frame/24),0,234,233);
                track_paint::backdrop(scene,PencilDetail::Low,false);
                drawPencilTrack(scene,camera,track,PencilDetail::Low,occlusion.get(),false,wire);
            });
        }
    }
}
