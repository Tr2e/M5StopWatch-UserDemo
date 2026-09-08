#include "race_controller.h"

#include "../model/car_catalog.h"
#include "../lets_and_go_config.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {

uint32_t nextRandom(uint32_t& state)
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return state;
}

float randomSigned(uint32_t& state)
{
    return static_cast<float>(nextRandom(state) & 0xffffu) / 32767.5f - 1.0f;
}

}  // namespace

void RaceController::prepare(const RaceSetup& setup, uint32_t seed)
{
    _snapshot = {};
    _accumulator = 0.0f;
    _paused = false;
    _track.select(setup.track);
    uint32_t random = seed == 0u ? 0x4c657473u : seed;

    const CarId playerCar = isValidCar(setup.playerCar)
                                ? setup.playerCar : CarId::CycloneMagnum;
    std::size_t index = 0;
    for (std::size_t carIndex = 0; carIndex < kCarCount && index < kMaximumRivals; ++carIndex) {
        const CarId car = static_cast<CarId>(carIndex);
        if (car == playerCar || !setup.hasRival(car)) continue;
        RaceCarSnapshot& entry = _snapshot.cars[index];
        entry.car = car;
        entry.active = true;
        // All grid slots are behind the SAME start/finish line (s = 0).
        entry.startDistance = -static_cast<float>(index + 1u) * 1.35f;
        const float initialSpeed = 7.6f + randomSigned(random) * 0.22f;
        _models[index].reset(entry.startDistance,
                             (index & 1u) == 0u ? -0.42f : 0.42f,
                             initialSpeed);
        const float efficiency = tuning::kRivalMotorEfficiency +
                                 randomSigned(random) * tuning::kRivalMotorVariation;
        resetRivalAi(_ai[index], nextRandom(random), efficiency);
        ++index;
    }

    _snapshot.playerIndex = index;
    RaceCarSnapshot& player = _snapshot.cars[index];
    player.car = playerCar;
    player.active = true;
    player.player = true;
    player.startDistance = -static_cast<float>(index + 1u) * 1.35f;
    _models[index].reset(player.startDistance, 0.0f, 6.9f);
    ++index;
    _snapshot.carCount = index;
    for (std::size_t carIndex = 0; carIndex < _snapshot.carCount; ++carIndex) {
        _lastLapCrossing[carIndex] = -1.0f;
        _snapshot.cars[carIndex].motion = _models[carIndex].state();
    }
    updatePositions();
    _prepared = true;
}

void RaceController::advance(const RacerInput& playerInput, float elapsedSeconds)
{
    if (!_prepared || _paused || !std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0f) {
        return;
    }
    _accumulator += std::min(elapsedSeconds, 0.25f);
    int steps = 0;
    while (_accumulator >= kFixedStepSeconds && steps < kMaximumCatchUpSteps) {
        stepFixed(playerInput);
        _accumulator -= kFixedStepSeconds;
        ++steps;
    }
    if (_accumulator >= kFixedStepSeconds) {
        _accumulator = std::fmod(_accumulator, kFixedStepSeconds);
        if (_snapshot.simulationClampCount < UINT16_MAX) {
            ++_snapshot.simulationClampCount;
        }
    }
}

void RaceController::stepFixed(const RacerInput& playerInput)
{
    if (!_prepared || _paused || _snapshot.playerFinished) return;
    _snapshot.elapsedSeconds += kFixedStepSeconds;
    const RaceSnapshot decisionSnapshot = _snapshot;
    for (std::size_t index = 0; index < _snapshot.carCount; ++index) {
        RaceCarSnapshot& entry = _snapshot.cars[index];
        if (!entry.active || entry.finished) continue;
        const TrackFrame frame = _track.sample(_models[index].state().distance);
        RacerInput input = playerInput;
        float efficiency = _snapshot.elapsedSeconds < 2.5f ? 0.90f : 1.0f;
        if (!entry.player) {
            input = updateRivalAi(_ai[index], decisionSnapshot, index, frame,
                                  kFixedStepSeconds, _track.length());
            efficiency = _ai[index].motorEfficiency;
            const float finalHalfLap = _track.length() * 2.5f;
            if (entry.raceProgress < finalHalfLap) {
                const float positionAssist =
                    static_cast<float>(entry.position > 1u ? entry.position - 1u : 0u) * 0.006f;
                efficiency += std::min(positionAssist, 0.018f);
                if (entry.position == 1u) efficiency -= 0.004f;
            }
        }
        const float previousDistance = entry.motion.distance;
        _models[index].step(input, carSpec(entry.car), frame,
                            kFixedStepSeconds, efficiency);
        entry.motion = _models[index].state();
        entry.raceProgress = std::max(0.0f, entry.motion.distance);
        updateLapAndFinish(index, previousDistance);
    }
    resolveCarContacts();
    updatePositions();
}

void RaceController::updateLapAndFinish(std::size_t index, float previousDistance)
{
    RaceCarSnapshot& entry = _snapshot.cars[index];
    const auto crossingTime = [&](float line) {
        const float fraction = (line - previousDistance) /
                               (entry.motion.distance - previousDistance);
        return _snapshot.elapsedSeconds - kFixedStepSeconds +
               std::clamp(fraction, 0.0f, 1.0f) * kFixedStepSeconds;
    };
    if (previousDistance < 0.0f && entry.motion.distance >= 0.0f) {
        _lastLapCrossing[index] = crossingTime(0.0f);
    }
    const uint8_t laps = static_cast<uint8_t>(std::min(
        static_cast<int>(kRaceLapCount),
        static_cast<int>(entry.raceProgress / _track.length())));
    if (laps > entry.completedLaps) {
        const float crossedAt = crossingTime(static_cast<float>(laps) * _track.length());
        const float lapSeconds = crossedAt - _lastLapCrossing[index];
        if (entry.bestLapSeconds <= 0.0f || lapSeconds < entry.bestLapSeconds) {
            entry.bestLapSeconds = lapSeconds;
        }
        _lastLapCrossing[index] = crossedAt;
        entry.completedLaps = laps;
    }
    if (!entry.finished && entry.completedLaps >= kRaceLapCount) {
        entry.finished = true;
        entry.finishSeconds = _lastLapCrossing[index];
        if (entry.player) _snapshot.playerFinished = true;
    }
}

void RaceController::resolveCarContacts()
{
    for (std::size_t left = 0; left < _snapshot.carCount; ++left) {
        for (std::size_t right = left + 1u; right < _snapshot.carCount; ++right) {
            if (!_snapshot.cars[left].active || !_snapshot.cars[right].active ||
                _snapshot.cars[left].finished || _snapshot.cars[right].finished) continue;
            RacerState& a = _models[left].mutableState();
            RacerState& b = _models[right].mutableState();
            const float gap = std::remainder(b.distance - a.distance, _track.length());
            if (std::abs(gap) >= 0.62f ||
                std::abs(a.lateralOffset - b.lateralOffset) >= 0.42f) continue;
            if (gap > 0.0f && a.speed > b.speed) {
                a.speed = b.speed * 0.96f;
                b.speed *= 1.01f;
            } else if (gap < 0.0f && b.speed > a.speed) {
                b.speed = a.speed * 0.96f;
                a.speed *= 1.01f;
            }
            _snapshot.cars[left].motion = a;
            _snapshot.cars[right].motion = b;
        }
    }
}

void RaceController::updatePositions()
{
    for (std::size_t index = 0; index < _snapshot.carCount; ++index) {
        uint8_t position = 1u;
        for (std::size_t other = 0; other < _snapshot.carCount; ++other) {
            if (index == other) continue;
            const RaceCarSnapshot& a = _snapshot.cars[index];
            const RaceCarSnapshot& b = _snapshot.cars[other];
            const bool otherAhead = raceCarAhead(b, other, a, index);
            if (otherAhead) ++position;
        }
        _snapshot.cars[index].position = position;
        if (_snapshot.cars[index].finished) _snapshot.cars[index].finishOrder = position;
    }
}

}  // namespace lets_and_go
