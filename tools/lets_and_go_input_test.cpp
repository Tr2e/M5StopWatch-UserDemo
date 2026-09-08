#include "../main/apps/app_lets_and_go_racer/input/racer_input_logic.h"
#include "../main/apps/app_vector_canyon_fighter/input/external_input_logic.h"
#include "../main/apps/app_vector_canyon_fighter/input/flight_input.h"

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
    raw.viewAxis = 1.0f;
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
                           !mapped.confirmPressed && !mapped.cancelPressed &&
                           !mapped.brakeHeld && !mapped.boostHeld &&
                           !mapped.pausePressed && mapped.exitPressed &&
                           mapped.sequence == 7u && mapped.menuBlocked && mapped.viewAxis==0.f,
                       "valid input mapping failed");
    raw.axesValid = false;
    const RacerInput stale = mapRacerInput(raw, 8u);
    valid &= check(!stale.valid && stale.steer == 0.0f && stale.viewAxis==0.f && !stale.brakeHeld &&
                       !stale.boostHeld && stale.exitPressed,
                   "invalid input did not fail neutral");
    raw.actionsValid = false;
    valid &= check(!mapRacerInput(raw, 8u).exitPressed,
                   "invalid buttons triggered exit");
    RawRacerInput buttonOnly;
    buttonOnly.actionsValid = true;
    buttonOnly.redClicked = true;
    const RacerInput buttonsWithoutAxes = mapRacerInput(buttonOnly, 8u);
    valid &= check(!buttonsWithoutAxes.valid &&
                       buttonsWithoutAxes.cancelPressed &&
                       !buttonsWithoutAxes.confirmPressed,
                   "valid Dual Button edge was coupled to invalid axes");
    raw.actionsValid = true;
    raw.axesValid = true;
    raw.redHeld = false;
    raw.redHoldStarted = false;
    raw.redClicked = false;
    raw.chordStarted = false;
    raw.viewAxis=-2.f;
    valid &= check(mapRacerInput(raw, 8u).confirmPressed &&
                       mapRacerInput(raw, 8u).boostHeld,
                   "single blue button mapping failed");
    valid &= check(mapRacerInput(raw,8u).viewAxis==-1.f,"Y axis not mapped/clamped");
    raw.viewAxis=std::numeric_limits<float>::quiet_NaN();
    valid &= check(mapRacerInput(raw,8u).viewAxis==0.f && mapRacerInput(raw,8u).valid,
                   "bad preview Y must not invalidate racing steering");
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

bool validateSlowFrameInput()
{
    using namespace vector_canyon_fighter;
    DebouncedActiveLowButton red, blue;
    TwoButtonFlightActionMapper actions;
    LongChordDetector chord;
    RacerInputMailbox mailbox;
    const auto poll = [&](bool redDown, bool blueDown, uint32_t now) {
        const auto r = red.update(redDown, now);
        const auto b = blue.update(blueDown, now);
        const auto a = actions.update(r.pressed, b.pressed, r.clicked,
                                      b.clicked, r.holdStarted, b.holding);
        RawRacerInput raw;
        raw.axesValid = raw.actionsValid = true;
        raw.steer = 0.8f;
        raw.viewAxis = -0.9f;
        raw.redClicked = a.wasPressed(FlightAction::ThrottleDown);
        raw.blueClicked = a.wasPressed(FlightAction::ThrottleUp);
        raw.redHeld = a.isHeld(FlightAction::ThrottleDown);
        raw.blueHeld = a.isHeld(FlightAction::ThrottleUp);
        raw.redHoldStarted = a.wasPressed(FlightAction::ToggleImmersive);
        raw.chordStarted = chord.update(raw.redHeld, raw.blueHeld, now);
        mailbox.publish(mapRacerInput(raw, now));
    };
    // One 100 ms click is entirely inside a 400 ms render stall.
    for (uint32_t t = 0; t <= 400; t += 10)
        poll(false, t >= 50 && t < 150, t);
    auto input = mailbox.consume();
    bool valid = check(input.confirmPressed && !input.boostHeld && input.valid &&
                           input.steer == 0.8f && input.viewAxis == -0.9f,
                       "slow render lost short blue click or latest axes");
    valid &= check(!mailbox.consume().confirmPressed,
                   "buffered click replayed on a second frame");
    for (uint32_t t = 410; t <= 800; t += 10)
        poll(t >= 450 && t < 550, false, t);
    valid &= check(mailbox.consume().cancelPressed,
                   "slow render lost short red click");
    // Hold both through an entire long frame and release before consumption.
    for (uint32_t t = 810; t <= 2100; t += 10)
        poll(t >= 850 && t < 1850, t >= 850 && t < 1850, t);
    input = mailbox.consume();
    valid &= check(input.exitPressed && !input.confirmPressed &&
                       !input.cancelPressed && !input.pausePressed,
                   "buffered exit chord leaked pause or release clicks");
    valid &= check(!mailbox.consume().exitPressed, "exit chord replayed");
    RacerInput stale;
    stale.confirmPressed = true;
    mailbox.publish(stale);
    mailbox.reset();
    valid &= check(!mailbox.consume().confirmPressed,
                   "close/recalibration kept a queued click");
    return valid;
}
}  // namespace

int main()
{
    return validateMapping() && validateMenuRepeater() && validateLongChord() &&
           validateSlowFrameInput() ? 0 : 1;
}
