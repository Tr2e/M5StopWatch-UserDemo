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
                      carMask(CarId::NeoTridaggerZmc);
    return setup;
}

bool runDeterministicRace(uint32_t seed, RaceSnapshot& result)
{
    RaceController race;
    race.prepare(fullGrid(), seed);
    bool valid = check(race.snapshot().carCount == kMaximumRaceCars &&
                           race.snapshot().playerIndex == kMaximumRivals &&
                           race.snapshot().player().position == kMaximumRaceCars &&
                           race.snapshot().player().motion.speed <
                               race.snapshot().cars[0].motion.speed,
                       "player was not placed last on full grid");
    RacerInput input;
    input.valid = true;
    for (int step = 0; step < 60 * 120 && !race.snapshot().playerFinished; ++step) {
        const auto previous = race.snapshot();
        const TrackFrame frame = race.track().sample(race.snapshot().player().motion.distance);
        input.steer = std::clamp(-frame.curvature * 1.8f -
                                     race.snapshot().player().motion.lateralOffset * 0.8f,
                                 -1.0f, 1.0f);
        input.boostHeld = std::abs(frame.curvature) < 0.025f;
        race.stepFixed(input);
        unsigned positions = 0u;
        for (std::size_t i = 0; i < race.snapshot().carCount; ++i) {
            const auto& car = race.snapshot().cars[i];
            const unsigned expectedLap = static_cast<unsigned>(std::clamp(
                car.motion.distance / race.track().length(), 0.0f, 3.0f));
            valid &= check(car.completedLaps == expectedLap,
                           "laps were not counted at the shared finish line");
            valid &= check((positions & (1u << car.position)) == 0u,
                           "race assigned duplicate positions");
            positions |= 1u << car.position;
            if (car.finished && !previous.cars[i].finished) {
                const float fraction = (3.0f * race.track().length() -
                    previous.cars[i].motion.distance) /
                    (car.motion.distance - previous.cars[i].motion.distance);
                const float expectedTime = race.snapshot().elapsedSeconds -
                    RaceController::kFixedStepSeconds +
                    fraction * RaceController::kFixedStepSeconds;
                valid &= check(std::abs(car.finishSeconds - expectedTime) < 0.0001f,
                               "finish timing did not interpolate the crossing");
            }
        }
    }
    result = race.snapshot();
    valid &= check(result.playerFinished && result.player().completedLaps == 3u,
                   "player did not finish three laps within bound");
    valid &= check(result.player().bestLapSeconds > 1.0f &&
                       result.player().position >= 1u && result.player().position <= kMaximumRaceCars,
                   "lap timing or final position invalid");
    return valid;
}

bool validateDeterminismAndModes()
{
    RaceCarSnapshot earlier{};
    RaceCarSnapshot later{};
    earlier.finished = later.finished = true;
    earlier.finishSeconds = 10.001f;
    later.finishSeconds = 10.009f;
    bool rankingValid = check(raceCarAhead(earlier, 3u, later, 0u) &&
                                  !raceCarAhead(later, 0u, earlier, 3u),
                              "same-tick finish favored array order over crossing time");
    earlier.finishSeconds = later.finishSeconds;
    rankingValid &= check(raceCarAhead(later, 0u, earlier, 3u) &&
                              !raceCarAhead(earlier, 3u, later, 0u),
                          "exact finish tie was not stable");
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
    setup.playerCar = static_cast<CarId>(255u);
    setup.rivalMask = 255u;
    solo.prepare(setup, 5u);
    valid &= check(solo.snapshot().carCount == kMaximumRaceCars &&
                       solo.snapshot().player().car == CarId::CycloneMagnum,
                   "malformed setup was not sanitized");
    for (std::size_t i = 0; i < solo.snapshot().carCount; ++i) {
        valid &= check(solo.snapshot().cars[i].startDistance < 0.0f,
                       "grid slot started ahead of the finish line");
    }
    solo.setPaused(true);
    const auto paused = solo.snapshot();
    solo.advance(input, 0.2f);
    valid &= check(solo.snapshot().elapsedSeconds == paused.elapsedSeconds,
                   "paused simulation advanced");
    return valid && rankingValid;
}
}  // namespace

int main()
{
    // Every legal roster, including bit 7, still fits the bounded race arrays.
    for(std::size_t player=0;player<kCarCount;++player)for(unsigned mask=0;mask<256;++mask) {
        RaceSetup setup;setup.playerCar=static_cast<CarId>(player);setup.rivalMask=uint8_t(mask);
        if(setup.hasRival(setup.playerCar) || setup.rivalCount()>kMaximumRivals)continue;
        RaceController race;race.prepare(setup,123);
        if(race.snapshot().carCount!=setup.rivalCount()+1 || race.snapshot().player().car!=setup.playerCar)return 1;
        RacerInput input;input.valid=true;race.stepFixed(input);
        for(std::size_t i=0;i<race.snapshot().carCount;++i)
            if(!race.snapshot().cars[i].player && !setup.hasRival(race.snapshot().cars[i].car))return 1;
    }
    return validateDeterminismAndModes() ? 0 : 1;
}
