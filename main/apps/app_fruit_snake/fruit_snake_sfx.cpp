#include "fruit_snake_sfx.h"

#include <hal/hal.h>

namespace fruit_snake {
namespace {

constexpr uint32_t kMinimumSoundSpacingMs = 45;

}  // namespace

void SfxPlayer::reset()
{
    _lastPlayedMs = 0;
}

void SfxPlayer::play(Sound sound, uint32_t nowMs)
{
    if (_lastPlayedMs != 0 && nowMs - _lastPlayedMs < kMinimumSoundSpacingMs) return;
    if (!GetHAL().getButtonConfig().sfxEnabled || GetHAL().getSpeakerVolume() <= 0) return;

    std::vector<int16_t> pcm = synthesize8BitSfx(sound, GetHAL().getAudioSampleRate());
    if (pcm.empty()) return;
    _lastPlayedMs = nowMs;
    GetHAL().audioPlay(pcm, true);
}

}  // namespace fruit_snake
