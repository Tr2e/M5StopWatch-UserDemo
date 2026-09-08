#include "../main/apps/app_lets_and_go_racer/input/racer_input_logic.h"

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

bool validateMapping()
{
    RawRacerInput raw;
    raw.steer = 2.0f;
    raw.axesValid = true;
    raw.actionsValid = true;
    raw.redClicked = true;
    raw.blueClicked = true;
    raw.redHeld = true;
    raw.blueHeld = true;
    raw.redHoldStarted = true;
    raw.chordStarted = true;
    const RacerInput mapped = mapRacerInput(raw, 7u);
    bool valid = check(mapped.valid && mapped.steer == 1.0f &&
                           mapped.confirmPressed && mapped.cancelPressed &&
                           mapped.brakeHeld && mapped.boostHeld &&
                           mapped.pausePressed && mapped.exitPressed &&
                           mapped.sequence == 7u,
                       "valid input mapping failed");
    raw.axesValid = false;
    const RacerInput stale = mapRacerInput(raw, 8u);
    valid &= check(!stale.valid && stale.steer == 0.0f && !stale.brakeHeld &&
                       !stale.boostHeld && !stale.exitPressed,
                   "invalid input did not fail neutral");
    raw.axesValid = true;
    raw.steer = std::numeric_limits<float>::quiet_NaN();
    valid &= check(!mapRacerInput(raw, 9u).valid, "NaN steering was accepted");
    return valid;
}

bool validateMenuRepeater()
{
    MenuAxisRepeater repeater;
    bool valid = check(repeater.update(0.7f, 100u) == 1,
                       "initial positive navigation edge missing");
    valid &= check(repeater.update(0.8f, 300u) == 0 &&
                       repeater.update(0.8f, 460u) == 1,
                   "held navigation repeat timing failed");
    valid &= check(repeater.update(0.1f, 470u) == 0 &&
                       repeater.update(-0.8f, 480u) == -1,
                   "release/reverse hysteresis failed");
    repeater.reset();
    valid &= check(repeater.update(-0.9f, UINT32_MAX - 100u) == -1 &&
                       repeater.update(-0.9f, 250u) == 0 &&
                       repeater.update(-0.9f, 260u) == -1,
                   "navigation repeat failed across millis rollover");
    return valid;
}

bool validateLongChord()
{
    LongChordDetector chord;
    bool valid = check(!chord.update(true, true, 100u) &&
                           !chord.update(true, true, 899u) &&
                           chord.update(true, true, 900u) &&
                           !chord.update(true, true, 1200u),
                       "long chord timing or one-shot latch failed");
    valid &= check(!chord.update(false, true, 1210u) &&
                       !chord.update(true, true, UINT32_MAX - 300u) &&
                       !chord.update(true, true, 498u) &&
                       chord.update(true, true, 500u),
                   "long chord reset or millis rollover failed");
    return valid;
}
}  // namespace

int main()
{
    return validateMapping() && validateMenuRepeater() && validateLongChord() ? 0 : 1;
}
