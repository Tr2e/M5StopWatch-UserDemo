#include "../main/apps/app_launcher/launcher_external_input_logic.h"

#include <iostream>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

}  // namespace

int main()
{
    using launcher_input::AxisNavigationRepeater;
    using launcher_input::NavigationStep;
    AxisNavigationRepeater input;
    bool valid = check(input.update(0.20f, true, 0) == NavigationStep::None,
                       "dead zone generated navigation");
    valid &= check(input.update(0.70f, true, 10) == NavigationStep::Next,
                   "right deflection did not advance");
    valid &= check(input.update(0.90f, true, 400) == NavigationStep::None,
                   "held axis repeated before initial delay");
    valid &= check(input.update(0.90f, true, 490) == NavigationStep::Next,
                   "held axis did not repeat after initial delay");
    valid &= check(input.update(0.0f, true, 500) == NavigationStep::None,
                   "release generated navigation");
    valid &= check(input.update(-0.70f, true, 510) == NavigationStep::Previous,
                   "left deflection did not go back");
    valid &= check(input.update(-0.90f, false, 1000) == NavigationStep::None,
                   "invalid controller generated navigation");
    valid &= check(input.update(-0.90f, true, 1010) == NavigationStep::Previous,
                   "reconnected controller remained latched");
    return valid ? 0 : 1;
}
