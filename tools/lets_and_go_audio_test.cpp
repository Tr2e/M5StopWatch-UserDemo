#include "../main/apps/app_lets_and_go_racer/audio/chip_synth.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

using namespace lets_and_go;
constexpr unsigned rate=44100;
std::vector<int16_t> samples(ChipSynth& synth,std::size_t count)
{
    std::vector<int16_t> out(count);synth.render(out.data(),out.size());return out;
}
void verify(const std::vector<int16_t>& pcm,bool audible)
{
    long long sum=0,energy=0;int peak=0;
    for(auto s:pcm) { assert(s%256==0);peak=std::max(peak,std::abs(int(s)));sum+=s;energy+=int(s)*int(s); }
    assert(peak<=16000);
    if(audible) {assert(peak>256);assert(energy>0);}
    else assert(peak==0);
    if(pcm.size()>rate*4)assert(std::abs(double(sum)/pcm.size())<100);
}
void writeWav(const char* path,const std::vector<int16_t>& pcm)
{
    std::ofstream file(path,std::ios::binary);
    const auto le=[&](uint32_t value,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i)file.put(char(value>>(8*i)));
    };
    file.write("RIFF",4);le(36+pcm.size()*2,4);file.write("WAVEfmt ",8);
    le(16,4);le(1,2);le(1,2);le(rate,4);le(rate*2,4);le(2,2);le(16,2);
    file.write("data",4);le(pcm.size()*2,4);
    for(auto s:pcm)le(uint16_t(s),2);
    assert(file.good());
}
int main(int argc,char** argv)
{
    for(auto scene:{MusicScene::Garage,MusicScene::Race,MusicScene::FinalLap,MusicScene::Results}) {
        ChipSynth whole,blocks;whole.setScene(scene);blocks.setScene(scene);
        const auto expected=samples(whole,rate*18);
        std::vector<int16_t> actual(expected.size());
        for(std::size_t i=0;i<actual.size();i+=512)
            blocks.render(actual.data()+i,std::min(std::size_t(512),actual.size()-i));
        assert(actual==expected);verify(actual,true);
    }
    for(unsigned cue=0;cue<unsigned(SoundCue::Count);++cue) {
        ChipSynth synth;synth.trigger(SoundCue(cue));verify(samples(synth,rate*2),true);
        assert(!synth.effectActive());verify(samples(synth,512),false);
    }
    ChipSynth paused,reference;paused.setScene(MusicScene::Race);reference.setScene(MusicScene::Race);
    assert(samples(paused,rate)==samples(reference,rate));
    paused.setScene(MusicScene::Paused);verify(samples(paused,rate*3),false);
    paused.trigger(SoundCue::Pause);verify(samples(paused,rate),true);
    paused.setScene(MusicScene::Race);assert(samples(paused,rate)==samples(reference,rate));
    paused.setScene(MusicScene::Off);verify(samples(paused,rate),false);
    paused.trigger(SoundCue::Count);verify(samples(paused,100),false);
    ChipSynth critical,uninterrupted;critical.trigger(SoundCue::Finish);uninterrupted.trigger(SoundCue::Finish);
    critical.trigger(SoundCue::Wall);critical.trigger(SoundCue::Navigate);
    assert(samples(critical,rate*2)==samples(uninterrupted,rate*2));
    for(uint32_t hz:{8000u,22050u,44100u,48000u}) {
        ChipSynth mixed(hz);mixed.setScene(MusicScene::FinalLap);
        for(unsigned cue=0;cue<unsigned(SoundCue::Count);++cue) {
            mixed.trigger(SoundCue(cue));verify(samples(mixed,hz),true);
        }
    }
    // Preview comes from precisely the same synth as the firmware callback.
    if(argc>1) {
        std::vector<int16_t> preview;
        ChipSynth synth;
        const auto append=[&](unsigned milliseconds) {
            const auto pcm=samples(synth,rate*milliseconds/1000);
            preview.insert(preview.end(),pcm.begin(),pcm.end());
        };
        synth.setScene(MusicScene::Garage);append(2000);
        synth.trigger(SoundCue::Navigate);append(500);synth.trigger(SoundCue::Confirm);append(1500);
        synth.setScene(MusicScene::Off);
        for(int i=0;i<3;++i){synth.trigger(SoundCue::Countdown);append(1000);}
        synth.setScene(MusicScene::Race);synth.trigger(SoundCue::Go);append(4000);
        synth.trigger(SoundCue::Boost);append(2000);synth.trigger(SoundCue::Brake);append(1000);
        synth.trigger(SoundCue::Wall);append(1000);
        synth.setScene(MusicScene::FinalLap);synth.trigger(SoundCue::FinalLap);append(3000);
        synth.setScene(MusicScene::Results);synth.trigger(SoundCue::Finish);append(3000);
        synth.setScene(MusicScene::Off);append(100);
        writeWav(argv[1],preview);
    }
    std::cout << "Chip audio: 4 scene arrangements, 14 cues, 4 sample rates, chunk invariance, pause/resume, priority, 8-bit headroom passed; synth="
              << sizeof(ChipSynth) << " bytes\n";
}
