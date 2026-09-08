#include "../main/apps/app_lets_and_go_racer/controller/race_controller.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

RaceSetup fullGrid()
{
    RaceSetup setup;
    setup.playerCar = CarId::CycloneMagnum;
    setup.rivalMask = carMask(CarId::HurricaneSonic) |
                      carMask(CarId::NeoTridaggerZmc) |
                      carMask(CarId::BrockenGigant);
    return setup;
}

bool runDeterministicRace(uint32_t seed, RaceSnapshot& result)
{
    RaceController race;
    race.prepare(fullGrid(), seed);
    bool valid = check(race.snapshot().carCount == 4u &&
                           race.snapshot().playerIndex == 3u &&
                           race.snapshot().player().position == 4u &&
                           race.snapshot().player().motion.speed <
                               race.snapshot().cars[0].motion.speed,
                       "player was not placed last on full grid");
    RacerInput input;
    input.valid = true;
    for (int step = 0; step < 60 * 120 && !race.snapshot().playerFinished; ++step) {
        const TrackFrame frame = race.track().sample(race.snapshot().player().motion.distance);
        input.steer = std::clamp(-frame.curvature * 1.8f -
                                     race.snapshot().player().motion.lateralOffset * 0.8f,
                                 -1.0f, 1.0f);
        input.boostHeld = std::abs(frame.curvature) < 0.025f;
        race.stepFixed(input);
    }
    result = race.snapshot();
    valid &= check(result.playerFinished && result.player().completedLaps == 3u,
                   "player did not finish three laps within bound");
    valid &= check(result.player().bestLapSeconds > 1.0f &&
                       result.player().position >= 1u && result.player().position <= 4u,
                   "lap timing or final position invalid");
    return valid;
}

bool validateDeterminismAndModes()
{
    RaceSnapshot first{};
    RaceSnapshot second{};
    bool valid = runDeterministicRace(0x12345678u, first) &&
                 runDeterministicRace(0x12345678u, second);
    valid &= check(first.elapsedSeconds == second.elapsedSeconds &&
                       first.player().position == second.player().position &&
                       first.player().bestLapSeconds == second.player().bestLapSeconds,
                   "same seed did not reproduce race result");

    RaceController solo;
    RaceSetup setup;
    setup.playerCar = CarId::NeoTridaggerZmc;
    solo.prepare(setup, 5u);
    valid &= check(solo.snapshot().carCount == 1u &&
                       solo.snapshot().playerIndex == 0u &&
                       solo.snapshot().player().position == 1u,
                   "solo grid setup failed");
    RacerInput input;
    input.valid = true;
    solo.advance(input, 1.0f);
    valid &= check(solo.snapshot().simulationClampCount == 1u,
                   "slow-frame catch-up clamp was not reported");
    return valid;
}
}  // namespace

int main()
{
    return validateDeterminismAndModes() ? 0 : 1;
}
