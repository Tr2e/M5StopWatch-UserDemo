#pragma once

#include <cstdint>
#include <vector>

namespace fruit_snake {

enum class Sound : uint8_t {
    FruitAdded,
    FruitEaten,
    SnakeShortened,
    Tickled,
    EdgeBounce,
};

// Public so the tiny synthesizer can be regression-tested without hardware.
std::vector<int16_t> synthesize8BitSfx(Sound sound, int sampleRate);

class SfxPlayer {
public:
    void reset();
    void play(Sound sound, uint32_t nowMs);

private:
    uint32_t _lastPlayedMs = 0;
};

}  // namespace fruit_snake
