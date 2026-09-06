#include "fruit_snake_sfx.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace fruit_snake {
namespace {

struct ChipNote {
    int startHz;
    int endHz;
    uint16_t durationMs;
    uint16_t gapMs;
    uint8_t dutyPercent;
    uint8_t volumePercent;
};

struct Pattern {
    std::array<ChipNote, 5> notes;
    std::size_t count;
};

Pattern patternFor(Sound sound)
{
    switch (sound) {
        case Sound::FruitAdded:
            return {{{{659, 740, 42, 10, 38, 45},
                       {880, 988, 62, 0, 38, 48},
                       {}, {}, {}}}, 2};
        case Sound::FruitEaten:
            return {{{{659, 659, 35, 7, 50, 44},
                       {831, 831, 38, 7, 50, 46},
                       {1047, 1175, 78, 0, 38, 50},
                       {}, {}}}, 3};
        case Sound::SnakeShortened:
            return {{{{784, 698, 40, 6, 32, 40},
                       {587, 523, 62, 0, 32, 37},
                       {}, {}, {}}}, 2};
        case Sound::Tickled:
            return {{{{1047, 1175, 25, 9, 25, 33},
                       {0, 0, 18, 0, 50, 0},
                       {1175, 1397, 26, 7, 25, 35},
                       {1568, 1760, 42, 0, 25, 38},
                       {}}}, 4};
        case Sound::EdgeBounce:
            return {{{{520, 255, 76, 5, 50, 35},
                       {392, 659, 48, 0, 25, 31},
                       {}, {}, {}}}, 2};
    }
    return {};
}

int16_t quantizedSquare(float phase, uint8_t dutyPercent, float envelope,
                        uint8_t volumePercent)
{
    constexpr float kBaseAmplitude = 4300.0f;
    constexpr int kQuantizationStep = 256;
    const float duty = std::clamp(static_cast<float>(dutyPercent) * 0.01f, 0.1f, 0.9f);
    const float raw = (phase < duty ? 1.0f : -1.0f) * kBaseAmplitude * envelope *
                      static_cast<float>(volumePercent) * 0.01f;
    return static_cast<int16_t>(static_cast<int>(raw / kQuantizationStep) *
                                kQuantizationStep);
}

}  // namespace

std::vector<int16_t> synthesize8BitSfx(Sound sound, int sampleRate)
{
    if (sampleRate < 8000) return {};
    const Pattern pattern = patternFor(sound);
    std::size_t totalSamples = 0;
    for (std::size_t i = 0; i < pattern.count; ++i) {
        totalSamples += static_cast<std::size_t>(sampleRate) *
                        (pattern.notes[i].durationMs + pattern.notes[i].gapMs) / 1000u;
    }

    std::vector<int16_t> pcm;
    pcm.reserve(totalSamples);
    float phase = 0.0f;
    for (std::size_t noteIndex = 0; noteIndex < pattern.count; ++noteIndex) {
        const ChipNote& note = pattern.notes[noteIndex];
        const int noteSamples = sampleRate * note.durationMs / 1000;
        const int attackSamples = std::max(1, sampleRate * 2 / 1000);
        const int releaseSamples = std::max(1, std::min(noteSamples / 2, sampleRate * 9 / 1000));
        for (int sample = 0; sample < noteSamples; ++sample) {
            if (note.startHz <= 0 || note.volumePercent == 0) {
                pcm.push_back(0);
                continue;
            }
            const float progress = noteSamples > 1
                                       ? static_cast<float>(sample) /
                                             static_cast<float>(noteSamples - 1)
                                       : 0.0f;
            const float frequency = note.startHz + (note.endHz - note.startHz) * progress;
            float envelope = 1.0f;
            if (sample < attackSamples) {
                envelope = static_cast<float>(sample) / attackSamples;
            }
            if (sample >= noteSamples - releaseSamples) {
                envelope *= static_cast<float>(noteSamples - sample) / releaseSamples;
            }
            phase += frequency / static_cast<float>(sampleRate);
            phase -= std::floor(phase);
            pcm.push_back(quantizedSquare(phase, note.dutyPercent, envelope,
                                          note.volumePercent));
        }
        pcm.insert(pcm.end(), sampleRate * note.gapMs / 1000, 0);
    }
    return pcm;
}

}  // namespace fruit_snake
