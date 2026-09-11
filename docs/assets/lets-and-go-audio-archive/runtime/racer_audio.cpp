#include "racer_audio.h"
#include <hal/hal.h>

namespace lets_and_go {
RacerAudio::~RacerAudio() { close(); }
bool RacerAudio::open()
{
    if(!_open)_open=GetHAL().audioStartStream(this,fill);
    return _open;
}
void RacerAudio::close()
{
    // Never hold _mutex while calling HAL: callback lock order is HAL -> synth.
    if(_open)GetHAL().audioStopStream(this);
    _open=false;
}
void RacerAudio::setScene(MusicScene scene)
{
    std::lock_guard<std::mutex> lock(_mutex);_synth.setScene(scene);
}
void RacerAudio::trigger(SoundCue cue)
{
    std::lock_guard<std::mutex> lock(_mutex);_synth.trigger(cue);
}
void RacerAudio::fill(void* owner,int16_t* output,std::size_t count)
{
    auto& self=*static_cast<RacerAudio*>(owner);
    std::lock_guard<std::mutex> lock(self._mutex);self._synth.render(output,count);
}
}
