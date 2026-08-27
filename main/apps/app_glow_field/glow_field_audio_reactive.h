#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace glow_field {

inline constexpr std::size_t kAudioBandCount = 20;
inline constexpr std::size_t kAudioGroupCount = 4;

enum class AudioGroup : uint8_t {
    Bass,
    LowMid,
    Mid,
    Treble,
};

struct AudioReactiveParams {
    static constexpr float bassAttackMs = 28.0f;
    static constexpr float bassReleaseMs = 210.0f;
    static constexpr float lowMidAttackMs = 35.0f;
    static constexpr float lowMidReleaseMs = 180.0f;
    static constexpr float midAttackMs = 24.0f;
    static constexpr float midReleaseMs = 145.0f;
    static constexpr float trebleAttackMs = 14.0f;
    static constexpr float trebleReleaseMs = 95.0f;
    static constexpr float overallAttackMs = 40.0f;
    static constexpr float overallReleaseMs = 240.0f;

    static constexpr float overallBassWeight = 0.38f;
    static constexpr float overallLowMidWeight = 0.28f;
    static constexpr float overallMidWeight = 0.22f;
    static constexpr float overallTrebleWeight = 0.12f;

    static constexpr uint32_t fluxWindowMs = 550;
    static constexpr uint32_t onsetCooldownMs = 145;
    static constexpr uint32_t midTransientCooldownMs = 105;
    static constexpr uint32_t trebleTransientCooldownMs = 65;
    static constexpr uint32_t onsetRateWindowMs = 1000;
    static constexpr uint32_t maxOnsetsPerSecond = 4;
    static constexpr uint32_t resumeSuppressMs = 200;
    static constexpr float onsetSensitivity = 1.20f;
    static constexpr float midTransientSensitivity = 1.35f;
    static constexpr float trebleTransientSensitivity = 1.40f;
    static constexpr float onsetMinLowEnergy = 0.12f;
    static constexpr float onsetMinThreshold = 0.045f;
    static constexpr float midTransientMinThreshold = 0.035f;
    static constexpr float trebleTransientMinThreshold = 0.025f;

    static constexpr uint32_t fadeInMs = 200;
    static constexpr uint32_t fadeOutMs = 220;
    static constexpr uint32_t pauseFadeMs = 220;

    static constexpr uint32_t sparkMinIntervalMs = 72;
    static constexpr uint32_t sparkMinDurationMs = 80;
    static constexpr uint32_t sparkMaxDurationMs = 180;
    static constexpr uint32_t symbolMutationMinMs = 90;
    static constexpr uint32_t symbolMutationMaxMs = 140;

    static constexpr uint16_t audioRippleMinDurationMs = 900;
    static constexpr uint16_t audioRippleMaxDurationMs = 1250;
    static constexpr int audioRippleOriginRadius = 48;
    static constexpr std::size_t maxAudioRipples = 3;
    static constexpr std::size_t maxSparksPerEvent = 3;
};

struct AudioReactiveFrame {
    std::array<float, kAudioBandCount> bands = {};
    std::array<float, kAudioGroupCount> groups = {};
    std::array<float, 3> transientGroups = {};
    float overallEnergy = 0.0f;
    float grooveEnergy = 0.0f;
    float slowEnergy = 0.0f;
    float groovePulse = 0.0f;
    float spectralCentroid = 0.0f;
    float onsetStrength = 0.0f;
    float bassOnsetStrength = 0.0f;
    float midTransient = 0.0f;
    float trebleTransient = 0.0f;
    float onsetThreshold = 0.0f;
    float inputRmsDbfs = -120.0f;
    float signalToNoiseDb = 0.0f;
    float signalConfidence = 0.0f;
    bool signalActive = false;
    bool onset = false;
    uint32_t sequence = 0;
};

class AudioReactiveController {
public:
    void reset(uint32_t nowMs);
    AudioReactiveFrame update(const std::array<float, kAudioBandCount>& bands,
                              const std::array<float, kAudioBandCount>& transientBands,
                              float peakFrequencyHz, bool signalActive, float signalConfidence,
                              float inputRmsDbfs, float signalToNoiseDb, uint32_t nowMs);
    void setVisualPaused(bool paused, uint32_t nowMs);

    bool visualPaused() const { return _visualPaused; }

private:
    static constexpr std::size_t kFluxHistorySize = 96;
    static constexpr std::size_t kOnsetRateSize = AudioReactiveParams::maxOnsetsPerSecond;

    struct FluxSample {
        uint32_t timeMs = 0;
        float value = 0.0f;
    };

    struct TransientDetector {
        std::array<FluxSample, kFluxHistorySize> history = {};
        std::size_t index = 0;
        std::size_t count = 0;
        uint32_t lastEventMs = 0;
    };

    void resetTransientDetectors();
    void pushFlux(TransientDetector& detector, float flux, uint32_t nowMs);
    void fluxStats(const TransientDetector& detector, uint32_t nowMs,
                   float& mean, float& deviation) const;
    bool onsetRateAvailable(uint32_t nowMs) const;
    void recordOnset(uint32_t nowMs);

    std::array<float, kAudioBandCount> _previousBands = {};
    std::array<float, kAudioBandCount> _previousTransientBands = {};
    std::array<float, kAudioGroupCount> _groups = {};
    std::array<TransientDetector, 3> _transientDetectors = {};
    std::array<uint32_t, kOnsetRateSize> _onsetTimes = {};
    float _overallEnergy = 0.0f;
    float _grooveEnergy = 0.0f;
    float _slowEnergy = 0.0f;
    float _groovePulse = 0.0f;
    uint32_t _lastUpdateMs = 0;
    uint32_t _onsetSuppressUntilMs = 0;
    uint32_t _sequence = 0;
    std::size_t _onsetTimeIndex = 0;
    bool _hasPrevious = false;
    bool _previousSignalActive = false;
    bool _visualPaused = false;
};

}  // namespace glow_field
