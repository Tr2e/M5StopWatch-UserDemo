#pragma once
#include "chip_synth.h"
#include <mutex>

namespace lets_and_go {
class RacerAudio {
public:
    explicit RacerAudio(uint32_t rate):_synth(rate) {}
    ~RacerAudio();
    bool open();
    void close();
    void setScene(MusicScene scene);
    void trigger(SoundCue cue);
private:
    static void fill(void* owner,int16_t* output,std::size_t count);
    std::mutex _mutex;
    ChipSynth _synth;
    bool _open=false;
};
}
