#include "../runtime/racer_audio.h"
#include <hal/hal.h>
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <thread>

int main()
{
    using namespace lets_and_go;
    auto& hal=GetHAL();
    hal.reject=true;
    {RacerAudio failed(44100);assert(!failed.open());}
    assert(hal.stops==0);
    hal.reject=false;
    std::atomic<bool> running{true};
    std::thread audio([&] {while(running){hal.pump();std::this_thread::yield();}});
    for(int cycle=0;cycle<100;++cycle) {
        auto owner=std::make_unique<RacerAudio>(44100);
        assert(owner->open());assert(owner->open());
        RacerAudio competitor(44100);assert(!competitor.open());
        for(int frame=0;frame<50;++frame) {
            owner->setScene(frame%5==0 ? MusicScene::Paused : MusicScene::Race);
            owner->trigger(SoundCue::Boost);
        }
        owner->close();owner->close();owner.reset();
    }
    running=false;audio.join();
    assert(hal.stops==100 && !hal.active);
    const auto before=hal.callbacks;hal.pump();assert(hal.callbacks==before);
    std::cout << "RacerAudio: unavailable/competing stream, idempotent open/close, 100 concurrent lifecycles passed\n";
}
