#include "../main/apps/app_lets_and_go_racer/controller/race_controller.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
using namespace lets_and_go;

bool finiteSnapshot(const RaceSnapshot& snapshot, const OverpassTrack& track)
{
    if (!std::isfinite(snapshot.elapsedSeconds) || snapshot.carCount == 0u ||
        snapshot.carCount > kMaximumRaceCars || snapshot.playerIndex >= snapshot.carCount) {
        return false;
    }
    for (std::size_t index = 0; index < snapshot.carCount; ++index) {
        const auto& car = snapshot.cars[index];
        const TrackFrame frame = track.sample(car.motion.distance);
        if (!std::isfinite(car.motion.distance) || !std::isfinite(car.motion.speed) ||
            !std::isfinite(car.motion.lateralOffset) || car.motion.speed < 0.0f ||
            std::abs(car.motion.lateralOffset) > frame.halfWidth + 0.001f ||
            car.position == 0u || car.position > snapshot.carCount ||
            car.completedLaps > kRaceLapCount) {
            return false;
        }
    }
    return true;
}

RaceSetup makeSetup(uint32_t scenario)
{
    RaceSetup setup;
    setup.playerCar = static_cast<CarId>(scenario % kCarCount);
    setup.track = static_cast<TrackId>((scenario/64u)%kTrackCount);
    const uint8_t wanted = static_cast<uint8_t>((scenario / kCarCount) % 4u);
    for (std::size_t index = 0; index < kCarCount && setup.rivalCount() < wanted; ++index) {
        const CarId car = static_cast<CarId>(index);
        if (car != setup.playerCar) setup.rivalMask |= carMask(car);
    }
    return setup;
}

bool runScenario(uint32_t scenario)
{
    RaceController race;
    race.prepare(makeSetup(scenario), 0x9e3779b9u ^ scenario * 0x45d9f3bu);
    if(race.track().id()!=makeSetup(scenario).track)return false;
    RacerInput input;
    for (int step = 0; step < 60 * 180 && !race.snapshot().playerFinished; ++step) {
        const auto& player = race.snapshot().player();
        const TrackFrame frame = race.track().sample(player.motion.distance);
        input.valid = (step % 901) >= 15;
        input.steer = std::clamp(-frame.curvature * 1.8f -
                                     player.motion.lateralOffset * 0.82f,
                                 -1.0f, 1.0f);
        input.boostHeld = (step % 420) < 120 && std::abs(frame.curvature) < 0.03f;
        input.brakeHeld = (step % 997) < 3;
        race.stepFixed(input);
        if ((step % 503) == 0) {
            const float before = race.snapshot().elapsedSeconds;
            race.advance(input, std::numeric_limits<float>::quiet_NaN());
            if (race.snapshot().elapsedSeconds != before) return false;
        }
        if (!finiteSnapshot(race.snapshot(), race.track())) return false;
    }
    return race.snapshot().playerFinished &&
           race.snapshot().player().completedLaps == kRaceLapCount &&
           race.snapshot().player().bestLapSeconds > 0.0f;
}
}  // namespace

int main()
{
    for (uint32_t scenario = 0; scenario < 64u*kTrackCount; ++scenario) {
        if (!runScenario(scenario)) {
            std::cerr << "FAIL: soak scenario " << scenario << '\n';
            return 1;
        }
    }
    std::cout << "128 deterministic 180-second-bound scenarios on two courses passed\n";
    return 0;
}
