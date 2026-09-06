#include "../main/apps/app_fruit_snake/fruit_snake_sfx.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

}  // namespace

int main()
{
    using namespace fruit_snake;
    constexpr int sampleRate = 44100;
    const std::array<Sound, 5> sounds = {Sound::FruitAdded, Sound::FruitEaten,
                                          Sound::SnakeShortened, Sound::Tickled,
                                          Sound::EdgeBounce};
    bool valid = true;
    std::size_t previousSize = 0;
    for (Sound sound : sounds) {
        const std::vector<int16_t> pcm = synthesize8BitSfx(sound, sampleRate);
        valid &= check(!pcm.empty(), "sound effect synthesized no samples");
        valid &= check(pcm.size() < static_cast<std::size_t>(sampleRate / 3),
                       "sound effect is too long for toddler feedback");
        const auto peak = std::max_element(pcm.begin(), pcm.end(),
                                           [](int16_t a, int16_t b) {
                                               return std::abs(static_cast<int>(a)) <
                                                      std::abs(static_cast<int>(b));
                                           });
        valid &= check(peak != pcm.end() && std::abs(static_cast<int>(*peak)) <= 2304,
                       "sound effect exceeded the gentle amplitude budget");
        valid &= check(std::any_of(pcm.begin(), pcm.end(), [](int16_t sample) {
                           return sample != 0 && sample % 256 == 0;
                       }), "sound effect lost its 8-bit quantized character");
        if (previousSize != 0) {
            valid &= check(pcm.size() != previousSize,
                           "two interaction sounds accidentally share one pattern");
        }
        previousSize = pcm.size();
    }
    valid &= check(synthesize8BitSfx(Sound::FruitAdded, 4000).empty(),
                   "unsupported sample rate was not rejected");
    return valid ? 0 : 1;
}
