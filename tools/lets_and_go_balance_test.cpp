#include "../main/apps/app_lets_and_go_racer/controller/race_controller.h"
#include <algorithm>
#include <iostream>

namespace {
using namespace lets_and_go;

struct Sweep { int wins = 0; int positions = 0; double seconds = 0; };

bool run(CarId car, uint32_t seed, int strategy, Sweep& sweep,TrackId track)
{
    RaceSetup setup;
    setup.playerCar = car;
    setup.track = track;
    for(std::size_t offset=1;offset<=kMaximumRivals;++offset)
        setup.rivalMask |= carMask(static_cast<CarId>((std::size_t(car)+offset)%kCarCount));
    RaceController race;
    race.prepare(setup, seed);
    if (race.snapshot().player().position != kMaximumRaceCars) return false;
    RacerInput input;
    input.valid = true;
    input.boostHeld = strategy != 0;
    for (int tick = 0; tick < 60 * 60 && !race.snapshot().playerFinished; ++tick) {
        if (strategy == 2) {
            // A reproducible clean passing line inside the outer guard rail,
            // using exactly the same bounded joystick input as a human.
            const auto& motion = race.snapshot().player().motion;
            input.steer = std::clamp((1.22f - motion.lateralOffset) * 1.7f -
                                     motion.lateralVelocity * 0.22f, -1.0f, 1.0f);
        }
        race.stepFixed(input);
    }
    if (!race.snapshot().playerFinished) return false;
    const auto& player = race.snapshot().player();
    sweep.wins += player.position == 1u;
    sweep.positions += player.position;
    sweep.seconds += player.finishSeconds;
    return player.completedLaps == 3u;
}
} // namespace

int main()
{
    for(auto track:{TrackId::SkyLoop,TrackId::TriCross,TrackId::GrandSpiral}) for (std::size_t car = 0; car < kCarCount; ++car) {
        Sweep idle, boost, passing;
        for (uint32_t seed = 1u; seed <= 32u; ++seed) {
            if (!run(static_cast<CarId>(car), seed, 0, idle,track) ||
                !run(static_cast<CarId>(car), seed, 1, boost,track) ||
                !run(static_cast<CarId>(car), seed, 2, passing,track)) {
                std::cerr << "Race lost last-grid/three-lap contract\n";
                return 1;
            }
        }
        // With fewer rivals, a faster run can finish in the same position.
        // In that case require a real aggregate time gain (over 1ms/run),
        // while still requiring the clean passing line to improve race rank.
        const bool boostImproved=boost.positions<idle.positions ||
            (boost.positions==idle.positions && boost.seconds+.032<idle.seconds);
        if (passing.wins < 8 || passing.wins <= idle.wins ||
            passing.positions >= boost.positions || !boostImproved) {
            std::cerr << "Car " << car << " has no meaningful boost/passing advantage: wins="
                      << idle.wins << ',' << boost.wins << ',' << passing.wins
                      << " positions=" << idle.positions << ',' << boost.positions << ',' << passing.positions
                      << " seconds=" << idle.seconds << ',' << boost.seconds << ',' << passing.seconds << '\n';
            return 1;
        }
        std::cout << overpassTrackName(track) << ' ' << carSpec(static_cast<CarId>(car)).shortName << " wins / 32: idle="
                  << idle.wins << " boost=" << boost.wins << " passing=" << passing.wins << '\n';
    }
}
