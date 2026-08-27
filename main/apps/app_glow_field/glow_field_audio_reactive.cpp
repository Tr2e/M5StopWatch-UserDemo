#include "glow_field_audio_reactive.h"

#include <algorithm>
#include <cmath>

namespace glow_field {
namespace {

constexpr std::array<float, 5> kIntraBandWeights = {1.16f, 1.08f, 1.00f, 0.90f, 0.80f};
constexpr float kIntraBandWeightSum = 1.16f + 1.08f + 1.00f + 0.90f + 0.80f;

constexpr std::array<float, kAudioBandCount> kFluxWeights = {
    1.25f, 1.20f, 1.15f, 1.10f, 1.05f, 1.00f, 0.95f, 0.90f, 0.85f, 0.80f,
    0.55f, 0.50f, 0.45f, 0.40f, 0.35f, 0.22f, 0.18f, 0.14f, 0.10f, 0.06f,
};

float envelopeAlpha(float dtMs, float tauMs)
{
    if (tauMs <= 0.01f) return 1.0f;
    return 1.0f - std::exp(-dtMs / tauMs);
}

float smoothToward(float current, float target, float dtMs, float attackMs, float releaseMs)
{
    const float tauMs = target > current ? attackMs : releaseMs;
    return current + (target - current) * envelopeAlpha(dtMs, tauMs);
}

float weightedGroup(const std::array<float, kAudioBandCount>& bands, std::size_t group)
{
    float sum = 0.0f;
    const std::size_t start = group * 5;
    for (std::size_t i = 0; i < 5; ++i) {
        sum += bands[start + i] * kIntraBandWeights[i];
    }
    return sum / kIntraBandWeightSum;
}

float rangeFlux(const std::array<float, kAudioBandCount>& current,
                const std::array<float, kAudioBandCount>& previous,
                std::size_t begin, std::size_t end)
{
    float sum = 0.0f;
    float weightSum = 0.0f;
    for (std::size_t i = begin; i < end; ++i) {
        const float rise = std::max(0.0f, current[i] - previous[i]);
        sum += rise * kFluxWeights[i];
        weightSum += kFluxWeights[i];
    }
    return weightSum > 0.0f ? sum / weightSum : 0.0f;
}

float rangeEnergy(const std::array<float, kAudioBandCount>& bands,
                  std::size_t begin, std::size_t end)
{
    float sum = 0.0f;
    for (std::size_t i = begin; i < end; ++i) sum += bands[i];
    return end > begin ? sum / static_cast<float>(end - begin) : 0.0f;
}

float spectralCentroid(const std::array<float, kAudioBandCount>& bands)
{
    float weighted = 0.0f;
    float total = 0.0f;
    for (std::size_t i = 0; i < kAudioBandCount; ++i) {
        weighted += bands[i] * static_cast<float>(i);
        total += bands[i];
    }
    if (total <= 1.0e-5f) return 0.0f;
    return weighted / total;
}

}  // namespace

void AudioReactiveController::reset(uint32_t nowMs)
{
    _previousBands = {};
    _previousTransientBands = {};
    _groups = {};
    _transientDetectors = {};
    _onsetTimes = {};
    _overallEnergy = 0.0f;
    _grooveEnergy = 0.0f;
    _slowEnergy = 0.0f;
    _groovePulse = 0.0f;
    _lastUpdateMs = nowMs;
    _onsetSuppressUntilMs = nowMs + AudioReactiveParams::resumeSuppressMs;
    _sequence = 0;
    _onsetTimeIndex = 0;
    _hasPrevious = false;
    _previousSignalActive = false;
    _visualPaused = false;
}

void AudioReactiveController::setVisualPaused(bool paused, uint32_t nowMs)
{
    if (_visualPaused == paused) return;
    _visualPaused = paused;
    if (!paused) {
        resetTransientDetectors();
        _onsetSuppressUntilMs = nowMs + AudioReactiveParams::resumeSuppressMs;
    }
}

void AudioReactiveController::resetTransientDetectors()
{
    _transientDetectors = {};
}

void AudioReactiveController::pushFlux(TransientDetector& detector, float flux, uint32_t nowMs)
{
    detector.history[detector.index] = {nowMs, flux};
    detector.index = (detector.index + 1) % kFluxHistorySize;
    if (detector.count < kFluxHistorySize) ++detector.count;
}

void AudioReactiveController::fluxStats(const TransientDetector& detector, uint32_t nowMs,
                                        float& mean, float& deviation) const
{
    float sum = 0.0f;
    float sumSquares = 0.0f;
    int count = 0;
    for (std::size_t i = 0; i < detector.count; ++i) {
        const FluxSample& sample = detector.history[i];
        if (nowMs - sample.timeMs > AudioReactiveParams::fluxWindowMs) continue;
        sum += sample.value;
        sumSquares += sample.value * sample.value;
        ++count;
    }
    if (count <= 0) {
        mean = 0.0f;
        deviation = 0.0f;
        return;
    }
    mean = sum / static_cast<float>(count);
    const float variance = std::max(0.0f, sumSquares / static_cast<float>(count) - mean * mean);
    deviation = std::sqrt(variance);
}

bool AudioReactiveController::onsetRateAvailable(uint32_t nowMs) const
{
    uint32_t recent = 0;
    for (uint32_t timeMs : _onsetTimes) {
        if (timeMs != 0 && nowMs - timeMs < AudioReactiveParams::onsetRateWindowMs) ++recent;
    }
    return recent < AudioReactiveParams::maxOnsetsPerSecond;
}

void AudioReactiveController::recordOnset(uint32_t nowMs)
{
    _onsetTimes[_onsetTimeIndex] = nowMs;
    _onsetTimeIndex = (_onsetTimeIndex + 1) % kOnsetRateSize;
}

AudioReactiveFrame AudioReactiveController::update(const std::array<float, kAudioBandCount>& bands,
                                                   const std::array<float, kAudioBandCount>& transientBands,
                                                   float /*peakFrequencyHz*/, bool signalActive,
                                                   float signalConfidence, float inputRmsDbfs,
                                                   float signalToNoiseDb, uint32_t nowMs)
{
    AudioReactiveFrame frame;
    frame.bands = bands;
    frame.spectralCentroid = spectralCentroid(bands);
    frame.signalActive = signalActive;
    frame.signalConfidence = signalConfidence;
    frame.inputRmsDbfs = inputRmsDbfs;
    frame.signalToNoiseDb = signalToNoiseDb;

    uint32_t dtMs = 16;
    if (_lastUpdateMs != 0 && nowMs >= _lastUpdateMs) {
        dtMs = std::clamp<uint32_t>(nowMs - _lastUpdateMs, 1, 80);
    }
    _lastUpdateMs = nowMs;

    const std::array<float, kAudioGroupCount> rawGroups = {
        weightedGroup(bands, 0),
        weightedGroup(bands, 1),
        weightedGroup(bands, 2),
        weightedGroup(bands, 3),
    };

    frame.transientGroups = {
        rangeEnergy(transientBands, 0, 5),
        rangeEnergy(transientBands, 5, 14),
        rangeEnergy(transientBands, 14, 20),
    };

    if (signalActive && !_previousSignalActive) {
        resetTransientDetectors();
        _onsetSuppressUntilMs = nowMs + AudioReactiveParams::resumeSuppressMs;
        _previousTransientBands = transientBands;
    }
    _previousSignalActive = signalActive;

    if (!_hasPrevious) {
        _previousBands = bands;
        _previousTransientBands = transientBands;
        _groups = rawGroups;
        _overallEnergy = AudioReactiveParams::overallBassWeight * _groups[0] +
                         AudioReactiveParams::overallLowMidWeight * _groups[1] +
                         AudioReactiveParams::overallMidWeight * _groups[2] +
                         AudioReactiveParams::overallTrebleWeight * _groups[3];
        _grooveEnergy = _overallEnergy;
        _slowEnergy = _overallEnergy;
        _hasPrevious = true;
        frame.groups = _groups;
        frame.overallEnergy = _overallEnergy;
        frame.grooveEnergy = _grooveEnergy;
        frame.slowEnergy = _slowEnergy;
        ++_sequence;
        frame.sequence = _sequence;
        return frame;
    }

    _groups[0] = smoothToward(_groups[0], rawGroups[0], static_cast<float>(dtMs),
                              AudioReactiveParams::bassAttackMs, AudioReactiveParams::bassReleaseMs);
    _groups[1] = smoothToward(_groups[1], rawGroups[1], static_cast<float>(dtMs),
                              AudioReactiveParams::lowMidAttackMs,
                              AudioReactiveParams::lowMidReleaseMs);
    _groups[2] = smoothToward(_groups[2], rawGroups[2], static_cast<float>(dtMs),
                              AudioReactiveParams::midAttackMs, AudioReactiveParams::midReleaseMs);
    _groups[3] = smoothToward(_groups[3], rawGroups[3], static_cast<float>(dtMs),
                              AudioReactiveParams::trebleAttackMs,
                              AudioReactiveParams::trebleReleaseMs);

    const float rawOverall = AudioReactiveParams::overallBassWeight * _groups[0] +
                             AudioReactiveParams::overallLowMidWeight * _groups[1] +
                             AudioReactiveParams::overallMidWeight * _groups[2] +
                             AudioReactiveParams::overallTrebleWeight * _groups[3];
    _overallEnergy = smoothToward(_overallEnergy, rawOverall, static_cast<float>(dtMs),
                                  AudioReactiveParams::overallAttackMs,
                                  AudioReactiveParams::overallReleaseMs);

    const float gatedOverall = signalActive ? rawOverall : 0.0f;
    _grooveEnergy = smoothToward(_grooveEnergy, gatedOverall, static_cast<float>(dtMs),
                                 65.0f, 360.0f);
    _slowEnergy = smoothToward(_slowEnergy, gatedOverall, static_cast<float>(dtMs),
                               280.0f, 1400.0f);
    _groovePulse *= std::exp(-static_cast<float>(dtMs) / 280.0f);

    const std::array<float, 3> flux = {
        rangeFlux(transientBands, _previousTransientBands, 0, 5),
        rangeFlux(transientBands, _previousTransientBands, 5, 14),
        rangeFlux(transientBands, _previousTransientBands, 14, 20),
    };
    _previousBands = bands;
    _previousTransientBands = transientBands;

    std::array<float, 3> threshold = {};
    constexpr std::array<float, 3> minimumThreshold = {
        AudioReactiveParams::onsetMinThreshold,
        AudioReactiveParams::midTransientMinThreshold,
        AudioReactiveParams::trebleTransientMinThreshold,
    };
    constexpr std::array<float, 3> sensitivity = {
        AudioReactiveParams::onsetSensitivity,
        AudioReactiveParams::midTransientSensitivity,
        AudioReactiveParams::trebleTransientSensitivity,
    };
    for (std::size_t i = 0; i < _transientDetectors.size(); ++i) {
        float mean = 0.0f;
        float deviation = 0.0f;
        fluxStats(_transientDetectors[i], nowMs, mean, deviation);
        threshold[i] = std::max(minimumThreshold[i], mean + sensitivity[i] * deviation);
        pushFlux(_transientDetectors[i], flux[i], nowMs);
    }

    const bool suppressed = _visualPaused || !signalActive || signalConfidence < 0.25f ||
                            nowMs < _onsetSuppressUntilMs;
    TransientDetector& bassDetector = _transientDetectors[0];
    const bool bassCooldownReady = bassDetector.lastEventMs == 0 ||
                                   nowMs - bassDetector.lastEventMs >= AudioReactiveParams::onsetCooldownMs;
    const bool bassHit = !suppressed && bassCooldownReady && onsetRateAvailable(nowMs) &&
                         frame.transientGroups[0] >= AudioReactiveParams::onsetMinLowEnergy &&
                         flux[0] > threshold[0];
    if (bassHit) {
        bassDetector.lastEventMs = nowMs;
        recordOnset(nowMs);
        frame.onset = true;
        frame.bassOnsetStrength = std::clamp((flux[0] - threshold[0]) /
                                                std::max(0.03f, threshold[0]) +
                                                frame.transientGroups[0] * 0.35f,
                                            0.0f, 1.0f);
        frame.onsetStrength = frame.bassOnsetStrength;
        _groovePulse = std::max(_groovePulse, 0.55f + frame.bassOnsetStrength * 0.45f);
    }

    TransientDetector& midDetector = _transientDetectors[1];
    const bool midReady = midDetector.lastEventMs == 0 ||
                          nowMs - midDetector.lastEventMs >= AudioReactiveParams::midTransientCooldownMs;
    if (!suppressed && midReady && frame.transientGroups[1] > 0.10f && flux[1] > threshold[1]) {
        midDetector.lastEventMs = nowMs;
        frame.midTransient = std::clamp((flux[1] - threshold[1]) /
                                            std::max(0.025f, threshold[1]) +
                                            frame.transientGroups[1] * 0.25f,
                                        0.0f, 1.0f);
    }

    TransientDetector& trebleDetector = _transientDetectors[2];
    const bool trebleReady = trebleDetector.lastEventMs == 0 ||
                             nowMs - trebleDetector.lastEventMs >= AudioReactiveParams::trebleTransientCooldownMs;
    if (!suppressed && trebleReady && frame.transientGroups[2] > 0.08f && flux[2] > threshold[2]) {
        trebleDetector.lastEventMs = nowMs;
        frame.trebleTransient = std::clamp((flux[2] - threshold[2]) /
                                               std::max(0.02f, threshold[2]) +
                                               frame.transientGroups[2] * 0.30f,
                                           0.0f, 1.0f);
    }

    frame.groups = _groups;
    frame.overallEnergy = _overallEnergy;
    frame.grooveEnergy = _grooveEnergy;
    frame.slowEnergy = _slowEnergy;
    frame.groovePulse = _groovePulse;
    frame.onsetThreshold = threshold[0];
    ++_sequence;
    frame.sequence = _sequence;
    return frame;
}

}  // namespace glow_field
