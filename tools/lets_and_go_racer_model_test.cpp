#include "../main/apps/app_lets_and_go_racer/model/racer_model.h"
#include "../main/apps/app_lets_and_go_racer/model/overpass_track.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

TrackFrame straightTrack()
{
    TrackFrame frame;
    frame.tangent = {0.0f, 0.0f, 1.0f};
    frame.lateral = {1.0f, 0.0f, 0.0f};
    frame.halfWidth = OverpassTrack::kHalfWidth;
    return frame;
}

bool validateDriving()
{
    RacerModel model;
    RacerInput input;
    input.valid = true;
    const CarSpec& car = carSpec(CarId::CycloneMagnum);
    TrackFrame track = straightTrack();
    for (int step = 0; step < 240; ++step) model.step(input, car, track, 1.0f / 60.0f);
    const float cruiseSpeed = model.state().speed;
    bool valid = check(cruiseSpeed > 15.0f && model.state().distance > 30.0f,
                       "automatic throttle failed");
    input.boostHeld = true;
    for (int step = 0; step < 120; ++step) model.step(input, car, track, 1.0f / 60.0f);
    valid &= check(model.state().boostCharge < 0.7f && model.state().speed > cruiseSpeed,
                   "boost did not trade charge for speed");
    input.boostHeld = false;
    input.brakeHeld = true;
    for (int step = 0; step < 120; ++step) model.step(input, car, track, 1.0f / 60.0f);
    valid &= check(model.state().speed < 2.0f, "held brake did not decelerate car");
    return valid;
}

bool validateCollisionAndFailure()
{
    RacerModel model;
    const CarSpec& car = carSpec(CarId::HurricaneSonic);
    TrackFrame track = straightTrack();
    model.reset(0.0f, track.halfWidth - 0.30f, 18.0f);
    RacerInput input;
    input.valid = true;
    input.steer = 1.0f;
    for (int step = 0; step < 20 && model.state().wallImpact == 0.0f; ++step) {
        model.step(input, car, track, 1.0f / 60.0f);
    }
    bool valid = check(model.state().wallImpact > 0.0f &&
                           model.state().speed < 18.0f &&
                           std::abs(model.state().lateralOffset) <= track.halfWidth,
                       "guard-rail collision response failed");
    RacerInput disconnected;
    const float before = model.state().speed;
    for (int step = 0; step < 30; ++step) {
        model.step(disconnected, car, track, 1.0f / 60.0f);
    }
    valid &= check(model.state().speed < before &&
                       std::abs(model.state().headingOffset) < 0.1f,
                   "disconnected input did not brake and neutralize steering");
    const RacerState snapshot = model.state();
    model.step(input, car, track, std::numeric_limits<float>::quiet_NaN());
    valid &= check(model.state().distance == snapshot.distance,
                   "non-finite delta time mutated state");
    return valid;
}
}  // namespace

int main()
{
    return validateDriving() && validateCollisionAndFailure() ? 0 : 1;
}
