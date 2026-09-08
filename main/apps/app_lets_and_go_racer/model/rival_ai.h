#pragma once

#include "race_state.h"

namespace lets_and_go {

struct RivalAiState {
    float targetLateral = 0.0f;
    float decisionCooldown = 0.0f;
    float motorEfficiency = 1.0f;
    uint32_t randomState = 1u;
};

void resetRivalAi(RivalAiState& state, uint32_t seed, float motorEfficiency);
RacerInput updateRivalAi(RivalAiState& state, const RaceSnapshot& race,
                         std::size_t racerIndex, const TrackFrame& track,
                         float deltaSeconds);

}  // namespace lets_and_go
