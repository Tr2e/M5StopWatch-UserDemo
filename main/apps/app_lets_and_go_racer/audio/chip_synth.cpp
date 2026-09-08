#include "chip_synth.h"
#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {
// Eight bars of eighth notes, A minor. Separate garage and chase melodies.
constexpr std::array<int8_t,64> chase{{
    76,0,76,79,81,79,76,74, 72,76,79,76,74,72,71,74,
    72,76,79,84,83,79,76,79, 74,79,83,86,83,79,74,71,
    76,81,84,83,81,79,76,74, 72,77,81,84,81,77,76,72,
    74,77,81,86,84,81,77,74, 71,76,80,83,86,83,80,76
}};
constexpr std::array<int8_t,64> garage{{
    69,0,72,0,76,74,72,0, 69,0,72,76,77,0,76,72,
    67,0,72,0,76,0,79,76, 71,0,74,0,79,77,74,0,
    72,0,76,79,81,0,79,76, 69,0,72,0,77,76,72,0,
    69,0,74,77,81,0,77,74, 71,0,76,80,83,0,80,76
}};
constexpr std::array<int8_t,8> roots{{45,41,48,43,45,41,50,40}};
constexpr std::array<uint16_t,14> durations{{45,140,120,100,110,420,260,110,300,650,1250,140,160,100}};
constexpr std::array<uint8_t,14> priorities{{1,2,2,2,4,5,2,1,3,4,6,5,5,2}};
static_assert(durations.size()==std::size_t(SoundCue::Count) && priorities.size()==durations.size());
int envelope(uint32_t position,uint32_t length,uint32_t attack,uint32_t release)
{
    if(position>=length)return 0;
    return int(std::min({uint32_t(256),position*256/std::max<uint32_t>(1,attack),
                        (length-position)*256/std::max<uint32_t>(1,release)}));
}
}

ChipSynth::ChipSynth(uint32_t sampleRate):_rate(std::clamp<uint32_t>(sampleRate,8000,48000)) {}

uint32_t ChipSynth::increment(int midi) const
{
    if(!midi)return 0;
    return uint32_t(440.0*std::pow(2.0,(midi-69)/12.0)*4294967296.0/_rate);
}

void ChipSynth::setScene(MusicScene scene)
{
    if(scene==_scene)return;
    _scene=scene;
    if(scene==MusicScene::Paused) { _effectRemaining=0;return; }
    if(scene==MusicScene::Off) {
        _score=scene;_step=0;_stepSample=0;_effectRemaining=0;return;
    }
    if(scene!=_score) {
        // Final lap raises tempo without restarting the musical phrase.
        if(!(_score==MusicScene::Race && scene==MusicScene::FinalLap))_step=0;
        _score=scene;_stepSample=0;_phase.fill(0);startStep();
    }
}

void ChipSynth::startStep()
{
    const unsigned bpm=_score==MusicScene::Garage ? 120 : _score==MusicScene::Results ? 112 :
                       _score==MusicScene::FinalLap ? 176 : 152;
    _stepLength=_rate*30/bpm;
    const bool racing=_score==MusicScene::Race || _score==MusicScene::FinalLap;
    const auto root=roots[(_step/8)%8];
    _increment[0]=increment((racing ? chase : garage)[_step%64]);
    _increment[1]=increment(root+(_step%2 ? 12 : 0));
    _increment[2]=increment(root+24+(_step%3==0 ? 0 : _step%3==1 ? 7 : 12));
}

void ChipSynth::trigger(SoundCue cue)
{
    const auto index=std::size_t(cue);
    if(index>=durations.size() || (_effectRemaining && priorities[index]<_priority))return;
    _effect=cue;_priority=priorities[index];_effectPhase=0;
    _effectRemaining=_effectLength=_rate*durations[index]/1000;
}

int ChipSynth::musicSample()
{
    if(_scene==MusicScene::Off || _scene==MusicScene::Paused)return 0;
    _noise^=_noise<<13;_noise^=_noise>>17;_noise^=_noise<<5;
    if(_stepSample>=_stepLength) { _step=(_step+1)%64;_stepSample=0;startStep(); }
    for(unsigned voice=0;voice<3;++voice)_phase[voice]+=_increment[voice];
    const int env=envelope(_stepSample,_stepLength*9/10,_rate/250,_rate/50);
    const int lead=_increment[0] ? ((_phase[0]&0x80000000u) ? 1550 : -1550)*env/256 : 0;
    const int ramp=int(_phase[1]>>20); // -1024..1024 triangle, zero DC.
    const int bass=(ramp<2048 ? ramp-1024 : 3072-ramp)*env/256;
    const int arp=((_phase[2]>>30)==0 ? 600 : -200)*env/256;
    const uint32_t drumLength=_rate/14;
    int drum=0;
    if(_stepSample<drumLength) {
        const int decay=int((drumLength-_stepSample)*256/drumLength);
        if(_step%4==0)drum=((_stepSample/(_rate/180))%2 ? 1300 : -1300)*decay/256;
        else drum=((_noise&1) ? 1 : -1)*(_step%4==2 ? 950 : 260)*decay/256;
    }
    ++_stepSample;
    return lead+bass+arp+drum;
}

int ChipSynth::effectSample()
{
    if(!_effectRemaining)return 0;
    const uint32_t age=_effectLength-_effectRemaining;
    const unsigned part=age*4/_effectLength;
    // Pitch slides and short arpeggios use frequencies rather than per-sample pow().
    // Refresh slides every 32 samples, not with a 64-bit division at each DAC sample.
    if(age%32==0) {
      unsigned hz=880;
      switch(_effect) {
        case SoundCue::Navigate: hz=1100;break;
        case SoundCue::Confirm: hz=part<2 ? 880 : 1320;break;
        case SoundCue::Back: hz=part<2 ? 660 : 440;break;
        case SoundCue::Reject: hz=165;break;
        case SoundCue::Countdown: hz=880;break;
        case SoundCue::Go: hz=part<2 ? 1320 : 1760;break;
        case SoundCue::Boost: hz=330+age*1500/_effectLength;break;
        case SoundCue::Wall: hz=90;break;
        case SoundCue::Lap: hz=part<2 ? 1047 : 1568;break;
        case SoundCue::FinalLap: {constexpr unsigned notes[]={880,1047,1319,1760};hz=notes[part];break;}
        case SoundCue::Finish: {constexpr unsigned notes[]={1047,1319,1568,2093};hz=notes[part];break;}
        case SoundCue::Pause: hz=660-age*330/_effectLength;break;
        case SoundCue::Resume: hz=660+age*660/_effectLength;break;
        case SoundCue::Brake: hz=440-age*300/_effectLength;break;
        default: break;
      }
      _effectIncrement=uint32_t((uint64_t(hz)<<32)/_rate);
    }
    _effectPhase+=_effectIncrement;
    const int env=envelope(age,_effectLength,_rate/500,_rate/50);
    int value=(_effectPhase&0x80000000u) ? 3800 : -3800;
    if(_effect==SoundCue::Wall) {
        _effectNoise^=_effectNoise<<13;_effectNoise^=_effectNoise>>17;_effectNoise^=_effectNoise<<5;
        value=(_effectNoise&1) ? 2400 : -2400;
    }
    --_effectRemaining;
    return value*env/256;
}

void ChipSynth::render(int16_t* output,std::size_t count)
{
    for(std::size_t i=0;i<count;++i) {
        const int music=musicSample()/(_effectRemaining ? 3 : 1);
        const int sample=std::clamp(music+effectSample(),-16000,16000);
        output[i]=int16_t((sample/256)*256); // Signed 8-bit DAC character in 16-bit PCM.
    }
}

} // namespace lets_and_go
