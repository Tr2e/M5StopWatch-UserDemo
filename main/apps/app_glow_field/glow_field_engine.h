#pragma once

#include "glow_field_audio_reactive.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace glow_field {

enum class SymbolGlyph : uint8_t {
    Triangle,
    Circle,
    Cross,
    Square,
};

enum class InteractionMode : uint8_t {
    Ripple,
    Paint,
    AudioReactive,
};

inline constexpr std::size_t kSymbolGlyphCount = 4;

struct Dot {
    int16_t x = 0;
    int16_t y = 0;
    uint8_t energy = 0;
    uint8_t colorIndex = 5;
    uint8_t rippleEnergy = 0;
    uint8_t rippleColorIndex = 5;
    uint8_t symbolIndex = 0;
    uint8_t symbolColorIndex = 0;
    uint8_t energySymbolIndex = 0;
    uint8_t energySymbolColorIndex = 0;
    uint8_t rippleSymbolIndex = 0;
    uint8_t rippleSymbolColorIndex = 0;
    uint8_t audioEnergy = 0;
    uint8_t audioColorIndex = 0;
    uint8_t audioSymbolIndex = 0;
    uint8_t audioBandIndex = 0;
    bool energyUsesSymbolPalette = false;
    bool energyMutatesSymbols = false;
    bool rippleUsesSymbolPalette = false;
    bool audioUsesSymbolPalette = false;
    bool rippleFromAudio = false;
    bool visible = false;
};

struct AudioApplyResult {
    bool onsetTriggered = false;
    uint8_t sparksSpawned = 0;
    uint8_t bloomsSpawned = 0;
};

class Engine {
public:
    static constexpr std::size_t kMaxDots = 512;
    static constexpr std::size_t kMaxRipples = 6;

    void reset(int width, int height);
    void clear();
    void update(uint32_t nowMs);
    void triggerRipple(int x, int y, uint32_t nowMs, uint8_t colorIndex,
                       bool symbolMix = false, bool mutateSymbols = false);
    void triggerPaintPoint(int x, int y, uint32_t nowMs, uint8_t colorIndex,
                           bool symbolMix = false, bool mutateSymbols = false);
    void beginTouch(int x, int y, uint32_t nowMs, uint8_t colorIndex,
                    bool symbolMix = false);
    void moveTouch(int x, int y, uint32_t nowMs);
    void endTouch();
    AudioApplyResult applyAudioFrame(const AudioReactiveFrame& frame, uint32_t nowMs,
                                     bool symbolMix, uint8_t baseColorIndex);
    void triggerAudioRipple(int x, int y, uint32_t nowMs, uint8_t strength, bool symbolMix);
    void setAudioReactionEnabled(bool enabled, uint32_t nowMs);
    void setAudioVisualPaused(bool paused, uint32_t nowMs);
    void clearAudioReaction(bool immediate);

    const std::array<Dot, kMaxDots>& dots() const { return _dots; }
    std::size_t dotCount() const { return _dotCount; }
    bool touching() const { return _touching; }
    bool audioOutputActive() const;
    float audioActiveDotRatio() const { return _audioActiveDotRatio; }

private:
    struct Ripple {
        int16_t x = 0;
        int16_t y = 0;
        int16_t reflectedX = 0;
        int16_t reflectedY = 0;
        uint32_t startedMs = 0;
        uint32_t seed = 0;
        uint16_t durationMs = 0;
        uint16_t maxRadius = 0;
        uint16_t travelMs = 0;
        uint16_t afterglowMs = 0;
        uint8_t colorIndex = 5;
        uint8_t paletteOffset = 0;
        uint8_t strength = 255;
        bool hasReflection = false;
        bool symbolMix = false;
        bool mutateSymbols = false;
        bool fromAudio = false;
        bool active = false;
    };

    struct AudioSpark {
        uint16_t index = 0;
        uint32_t startedMs = 0;
        uint16_t durationMs = 0;
        uint8_t peakEnergy = 0;
        uint8_t colorIndex = 0;
        uint8_t symbolIndex = 0;
        bool active = false;
    };

    void startRipple(int x, int y, uint32_t nowMs, uint8_t colorIndex, uint16_t durationMs,
                     uint16_t maxRadius, bool symbolMix, bool mutateSymbols,
                     bool fromAudio = false, uint8_t strength = 255, uint16_t travelMs = 0,
                     uint16_t afterglowMs = 0);
    void injectPoint(int x, int y, uint8_t colorIndex, int radius, uint32_t nowMs,
                     bool symbolMix, bool mutateSymbols);
    void injectImpact(int x, int y, uint8_t colorIndex);
    void stepSimulation();
    void updateRipples(uint32_t nowMs);
    void updateAudioGain(uint32_t nowMs);
    std::size_t acquireRippleSlot(bool fromAudio);
    uint8_t spawnAudioSparks(const AudioReactiveFrame& frame, uint32_t nowMs, bool symbolMix);
    uint8_t spawnAudioBloom(const AudioReactiveFrame& frame, uint32_t nowMs, bool symbolMix);
    void overlayAudioSparks(uint32_t nowMs);

    std::array<Dot, kMaxDots> _dots = {};
    std::array<Ripple, kMaxRipples> _ripples = {};
    std::size_t _dotCount = 0;
    std::size_t _nextRipple = 0;
    uint32_t _lastUpdateMs = 0;
    uint32_t _simulationAccumulatorMs = 0;
    uint32_t _decayRemainder = 0;
    int _maxRippleRadius = 0;
    int _centerX = 0;
    int _centerY = 0;
    int _fieldRadius = 0;
    int _lastTouchX = 0;
    int _lastTouchY = 0;
    uint8_t _touchColorIndex = 5;
    bool _touchSymbolMix = false;
    bool _touching = false;
    static constexpr std::size_t kMaxAudioSparks = 20;
    std::array<AudioSpark, kMaxAudioSparks> _audioSparks = {};
    uint32_t _lastAppliedAudioSequence = 0;
    uint32_t _audioGainStartedMs = 0;
    uint32_t _audioGainDurationMs = 0;
    uint32_t _lastSparkMs = 0;
    uint32_t _lastBloomMs = 0;
    uint32_t _lastAudioFieldMapMs = 0;
    uint32_t _audioMutationUntilMs = 0;
    int16_t _lastAudioRippleX = 0;
    int16_t _lastAudioRippleY = 0;
    uint8_t _audioGain = 0;
    uint8_t _audioGainFrom = 0;
    uint8_t _audioGainTarget = 0;
    uint8_t _lastAudioRippleColor = 1;
    uint8_t _audioBaseColorIndex = 5;
    float _audioActiveDotRatio = 0.0f;
    bool _audioEnabled = false;
    bool _audioVisualPaused = false;
    bool _audioNeedsClear = false;
};

}  // namespace glow_field
