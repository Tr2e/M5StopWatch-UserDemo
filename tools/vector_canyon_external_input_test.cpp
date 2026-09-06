#include "../main/apps/app_vector_canyon_fighter/input/external_input_logic.h"

#include <cmath>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool validateJoystickProtocolAndCurve()
{
    const uint8_t positive[] = {0x34, 0x12};
    const uint8_t negative[] = {0x00, 0xf8};
    bool valid = check(joystick2::decodeSignedLittleEndian(positive) == 0x1234,
                       "Joystick2 signed LE positive decode failed");
    valid &= check(joystick2::decodeSignedLittleEndian(negative) == -2048,
                   "Joystick2 signed LE negative decode failed");
    valid &= check(joystick2::normalizeOffset(80, 0) == 0.0f,
                   "Joystick2 dead zone leaked small input");
    const float half = joystick2::normalizeOffset(1010, 0);
    valid &= check(half > 0.30f && half < 0.55f,
                   "Joystick2 response curve lost progressive center control");
    valid &= check(std::abs(joystick2::normalizeOffset(2048, 0) - 1.0f) < 0.001f,
                   "Joystick2 positive travel did not clamp to one");
    valid &= check(std::abs(joystick2::normalizeOffset(-2048, 0) + 1.0f) < 0.001f,
                   "Joystick2 negative travel did not clamp to minus one");
    return valid;
}

bool validateJoystickRgbFeedback()
{
    using joystick2::LedFeedbackState;
    bool valid = check(
        joystick2::feedbackColor(0.0f, 0.0f, LedFeedbackState::Ready, 0)
                .packed() == 0x0000060eu,
        "Joystick2 neutral feedback is not dim blue");
    const auto left = joystick2::feedbackColor(
        -1.0f, 0.0f, LedFeedbackState::Ready, 0);
    const auto right = joystick2::feedbackColor(
        1.0f, 0.0f, LedFeedbackState::Ready, 0);
    const auto forward = joystick2::feedbackColor(
        0.0f, -1.0f, LedFeedbackState::Ready, 0);
    const auto back = joystick2::feedbackColor(
        0.0f, 1.0f, LedFeedbackState::Ready, 0);
    valid &= check(left.red > 0 && left.blue > left.red && left.green == 0,
                   "Joystick2 left feedback is not violet");
    valid &= check(right.red == 0 && right.green > 0 &&
                       right.blue > right.green,
                   "Joystick2 right feedback is not cyan");
    valid &= check(forward.green > forward.red &&
                       forward.green > forward.blue,
                   "Joystick2 forward feedback is not green");
    valid &= check(back.red > back.green && back.blue == 0,
                   "Joystick2 back feedback is not orange");
    valid &= check(
        joystick2::feedbackColor(0.0f, 0.0f, LedFeedbackState::Fault, 0)
                .packed() == 0x00400000u,
        "Joystick2 fault feedback is not red");
    const auto pulseLow = joystick2::feedbackColor(
        0.0f, 0.0f, LedFeedbackState::Calibrating, 0);
    const auto pulseHigh = joystick2::feedbackColor(
        0.0f, 0.0f, LedFeedbackState::Calibrating, 400);
    valid &= check(pulseHigh.red > pulseLow.red &&
                       pulseHigh.green > pulseLow.green,
                   "Joystick2 calibration feedback does not breathe");
    return valid;
}

bool validateButtonDebounceAndHold()
{
    DebouncedActiveLowButton button;
    bool valid = check(!button.update(false, 0).clicked,
                       "idle button generated an event");
    valid &= check(!button.update(true, 5).clicked &&
                       !button.update(false, 12).clicked,
                   "contact bounce generated a click");
    button.update(true, 30);
    valid &= check(!button.update(true, 51).clicked,
                   "press edge generated a click before release");
    button.update(false, 100);
    const ButtonTransition click = button.update(false, 121);
    valid &= check(click.clicked && !click.holding,
                   "short press did not generate exactly one click");
    valid &= check(!button.update(false, 140).clicked,
                   "short press click repeated");

    button.update(true, 200);
    button.update(true, 221);
    const ButtonTransition hold = button.update(true, 721);
    valid &= check(hold.holdStarted && hold.holding,
                   "long press did not enter held state");
    const ButtonTransition held = button.update(true, 760);
    valid &= check(!held.holdStarted && held.holding,
                   "held edge repeated or held state was lost");
    button.update(false, 800);
    const ButtonTransition release = button.update(false, 821);
    valid &= check(!release.clicked && !release.holding,
                   "long release incorrectly generated a short click");
    return valid;
}

}  // namespace

int main()
{
    return validateJoystickProtocolAndCurve() && validateJoystickRgbFeedback() &&
                   validateButtonDebounceAndHold()
               ? 0
               : 1;
}
