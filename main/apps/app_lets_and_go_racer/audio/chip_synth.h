#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lets_and_go {

enum class MusicScene : uint8_t { Off, Garage, Race, FinalLap, Results, Paused };
enum class SoundCue : uint8_t {
    Navigate, Confirm, Back, Reject, Countdown, Go, Boost, Wall,
    Lap, FinalLap, Finish, Pause, Resume, Brake, Count
};

// Original tracker score + procedural effects, not sampled copyrighted music.
// Fixed storage, integer phase oscillators, no allocations in render().
class ChipSynth {
public:
    explicit ChipSynth(uint32_t sampleRate=44100);
    void setScene(MusicScene scene);
    void trigger(SoundCue cue);
    void render(int16_t* output,std::size_t count);
    MusicScene scene() const { return _scene; }
    uint32_t musicStep() const { return _step; }
    bool effectActive() const { return _effectRemaining!=0; }
private:
    void startStep();
    int musicSample();
    int effectSample();
    uint32_t increment(int midi) const;
    uint32_t _rate;
    MusicScene _scene=MusicScene::Off,_score=MusicScene::Off;
    std::array<uint32_t,3> _phase{},_increment{};
    uint32_t _step=0,_stepSample=0,_stepLength=1;
    uint32_t _noise=0x13579bdu;
    uint32_t _effectNoise=0x2468aceu,_effectIncrement=0;
    SoundCue _effect=SoundCue::Navigate;
    uint32_t _effectPhase=0,_effectRemaining=0,_effectLength=0;
    uint8_t _priority=0;
};

} // namespace lets_and_go
