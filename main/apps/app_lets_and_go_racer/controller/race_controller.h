#pragma once

#include "game_flow.h"
#include "../model/overpass_track.h"
#include "../model/race_state.h"
#include "../model/rival_ai.h"

#include <array>
#include <cstdint>

namespace lets_and_go {

class RaceController {
public:
    static constexpr float kFixedStepSeconds = 1.0f / 60.0f;
    static constexpr int kMaximumCatchUpSteps = 5;

    void prepare(const RaceSetup& setup, uint32_t seed);
    void advance(const RacerInput& playerInput, float elapsedSeconds);
    void stepFixed(const RacerInput& playerInput);
    void setPaused(bool paused) { _paused = paused; }
    bool paused() const { return _paused; }
    const RaceSnapshot& snapshot() const { return _snapshot; }
    const OverpassTrack& track() const { return _track; }
    bool prepared() const { return _prepared; }

private:
    void updateLapAndFinish(std::size_t index);
    void updatePositions();
    void resolveCarContacts();

    OverpassTrack _track{};
    std::array<RacerModel, kMaximumRaceCars> _models{};
    std::array<RivalAiState, kMaximumRaceCars> _ai{};
    std::array<float, kMaximumRaceCars> _lastLapCrossing{};
    RaceSnapshot _snapshot{};
    float _accumulator = 0.0f;
    uint8_t _nextFinishOrder = 1u;
    bool _paused = false;
    bool _prepared = false;
};

static_assert(sizeof(RaceController) <= 2048u,
              "race controller exceeded its reviewed resident-state budget");

}  // namespace lets_and_go
