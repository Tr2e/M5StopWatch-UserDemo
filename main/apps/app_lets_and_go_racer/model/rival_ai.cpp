#include "rival_ai.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {

float randomUnit(uint32_t& state)
{
    state ^= state << 13u;
    state ^= state >> 17u;
    state ^= state << 5u;
    return static_cast<float>(state & 0xffffu) / 65535.0f;
}

}  // namespace

void resetRivalAi(RivalAiState& state, uint32_t seed, float motorEfficiency)
{
    state = {};
    state.randomState = seed == 0u ? 1u : seed;
    state.motorEfficiency = std::clamp(motorEfficiency, 0.94f, 1.04f);
    state.targetLateral = (randomUnit(state.randomState) - 0.5f) * 0.7f;
}

RacerInput updateRivalAi(RivalAiState& state, const RaceSnapshot& race,
                         std::size_t racerIndex, const TrackFrame& track,
                         float deltaSeconds)
{
    RacerInput input;
    input.valid = true;
    if (racerIndex >= race.carCount) return input;
    const RaceCarSnapshot& self = race.cars[racerIndex];
    state.decisionCooldown -= deltaSeconds;
    if (state.decisionCooldown <= 0.0f) {
        state.decisionCooldown = 0.42f + randomUnit(state.randomState) * 0.48f;
        // Outside line on entry, then opportunistically choose the free side
        // if another car occupies the next 1.8 m of course.
        state.targetLateral = std::clamp(-track.curvature * 2.2f, -0.75f, 0.75f);
        for (std::size_t index = 0; index < race.carCount; ++index) {
            if (index == racerIndex || !race.cars[index].active) continue;
            const float gap = race.cars[index].motion.distance - self.motion.distance;
            if (gap > 0.0f && gap < 1.8f &&
                std::abs(race.cars[index].motion.lateralOffset -
                         self.motion.lateralOffset) < 0.55f) {
                state.targetLateral = self.motion.lateralOffset <= 0.0f ? 0.78f : -0.78f;
                break;
            }
        }
    }
    const float error = state.targetLateral - self.motion.lateralOffset;
    input.steer = std::clamp(error * 1.7f - self.motion.lateralVelocity * 0.22f,
                             -1.0f, 1.0f);
    input.boostHeld = std::abs(track.curvature) < 0.035f &&
                      self.motion.boostCharge > 0.45f;
    return input;
}

}  // namespace lets_and_go
