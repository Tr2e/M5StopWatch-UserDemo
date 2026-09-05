#include "../main/apps/app_vector_canyon_fighter/hud_layout.h"

#include <cmath>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool validateCueConstraint()
{
    constexpr HudLayoutPoint origin{234.0f, 145.0f};
    const HudBoundedCue neutral = constrainHudCueToRect(
        {260.0f, 170.0f}, origin, 88.0f, 380.0f, 92.0f, 216.0f);
    bool valid = check(!neutral.constrained &&
                           std::abs(neutral.point.x - 260.0f) < 0.001f &&
                           std::abs(neutral.point.y - 170.0f) < 0.001f,
                       "in-bounds route cue was moved");

    constexpr HudLayoutPoint raw{520.0f, 400.0f};
    const HudBoundedCue bounded = constrainHudCueToRect(
        raw, origin, 88.0f, 380.0f, 92.0f, 216.0f);
    const float rawX = raw.x - origin.x;
    const float rawY = raw.y - origin.y;
    const float boundedX = bounded.point.x - origin.x;
    const float boundedY = bounded.point.y - origin.y;
    valid &= check(bounded.constrained &&
                       bounded.point.x >= 88.0f && bounded.point.x <= 380.0f &&
                       bounded.point.y >= 92.0f && bounded.point.y <= 216.0f,
                   "route cue escaped its central navigation bounds");
    valid &= check(std::abs(rawX * boundedY - rawY * boundedX) < 0.01f,
                   "route cue constraint changed the guidance direction");

    const HudBoundedCue upperLeft = constrainHudCueToRect(
        {-200.0f, -300.0f}, origin, 88.0f, 380.0f, 92.0f, 376.0f);
    valid &= check(upperLeft.constrained &&
                       upperLeft.point.x >= 88.0f && upperLeft.point.y >= 92.0f,
                   "first-person route cue escaped at the upper-left boundary");
    return valid;
}

bool validatePitchLabelZones()
{
    bool valid = check(hudPitchLabelVisible(120.0f, 180.0f, true, 468, 466),
                       "valid third-person pitch label was rejected");
    valid &= check(!hudPitchLabelVisible(120.0f, 230.0f, true, 468, 466),
                   "third-person pitch label entered the aircraft/course zone");
    valid &= check(hudPitchLabelVisible(120.0f, 300.0f, false, 468, 466),
                   "valid first-person lower pitch label was rejected");
    valid &= check(!hudPitchLabelVisible(120.0f, 390.0f, false, 468, 466) &&
                       !hudPitchLabelVisible(60.0f, 180.0f, false, 468, 466),
                   "pitch label entered a peripheral HUD or circular clip zone");
    return valid;
}

bool validateFirstPersonCueSeparation()
{
    constexpr HudLayoutPoint origin{234.0f, 233.0f};
    HudBoundedCue centered{};
    centered.point = origin;
    bool valid = check(!hudRouteCueVisible(centered, origin, false),
                       "centered first-person route cue obscures the datum");
    centered.point.x += 15.0f;
    valid &= check(hudRouteCueVisible(centered, origin, false),
                   "first-person route cue stayed hidden after a readable deviation");
    centered.point = origin;
    centered.constrained = true;
    valid &= check(hudRouteCueVisible(centered, origin, false),
                   "constrained first-person guidance cue was hidden");
    valid &= check(hudRouteCueVisible(centered, origin, true),
                   "third-person route cue must remain visible");
    return valid;
}

bool validatePeripheralScales()
{
    bool valid = check(normalizeHudDegrees(-5) == 355 &&
                           normalizeHudDegrees(360) == 0 &&
                           normalizeHudDegrees(725) == 5,
                       "heading labels do not wrap through north");
    constexpr float centerX = 234.0f;
    const float northX = hudHeadingTickX(360.0f, 359.0f, centerX);
    const float previousX = hudHeadingTickX(355.0f, 359.0f, centerX);
    valid &= check(northX > centerX && northX - centerX < 5.0f &&
                       previousX < centerX,
                   "heading tape is discontinuous around 359/000 degrees");
    const float subDegreeBefore = hudHeadingTickX(10.0f, 9.10f, centerX);
    const float subDegreeAfter = hudHeadingTickX(10.0f, 9.35f, centerX);
    valid &= check(subDegreeAfter < subDegreeBefore &&
                       subDegreeBefore - subDegreeAfter > 1.0f &&
                       subDegreeBefore - subDegreeAfter < 1.2f,
                   "heading tape discarded sub-degree motion");

    constexpr float datumY = 212.0f;
    const float highSpeedBefore = hudTapeTickY(80.0f, 70.0f, 5.0f, 10.0f, datumY);
    const float highSpeedAfter = hudTapeTickY(80.0f, 75.0f, 5.0f, 10.0f, datumY);
    valid &= check(highSpeedBefore < datumY && highSpeedAfter > highSpeedBefore,
                   "speed tape did not scroll downward as speed increased");
    const float aglNegative = hudTapeTickY(-2.0f, -1.0f, 1.0f, 10.0f, datumY);
    valid &= check(std::isfinite(aglNegative) && aglNegative > datumY,
                   "AGL tape cannot represent negative clearance");

    valid &= check(hudRectInsideCircularArea(35.0f, 204.0f, 40.0f, 17.0f,
                                             468, 466, 31.0f) &&
                       hudRectInsideCircularArea(393.0f, 204.0f, 40.0f, 17.0f,
                                                 468, 466, 31.0f),
                   "side tape readout escaped the core circular safe area");
    return valid;
}

bool validateProximityArrowScale()
{
    const int farLength = hudProximityArrowLength(0.0f);
    const int middleLength = hudProximityArrowLength(0.5f);
    const int nearLength = hudProximityArrowLength(1.0f);
    bool valid = check(farLength == 7 && nearLength == 18 &&
                           farLength < middleLength && middleLength < nearLength &&
                           hudProximityArrowLength(-1.0f) == farLength &&
                           hudProximityArrowLength(2.0f) == nearLength,
                       "proximity arrow is not monotonic or exceeded 7..18 px");
    valid &= check(hudRectInsideCircularArea(45.0f, 271.0f, 18.0f, 19.0f,
                                             468, 466, 31.0f) &&
                       hudRectInsideCircularArea(405.0f, 271.0f, 18.0f, 19.0f,
                                                 468, 466, 31.0f) &&
                       hudRectInsideCircularArea(205.0f, 374.0f, 58.0f, 16.0f,
                                                 468, 466, 31.0f),
                   "integrated wall or independent floor cue escaped the safe area");
    return valid;
}

}  // namespace

int main()
{
    bool valid = validateCueConstraint();
    valid &= validatePitchLabelZones();
    valid &= validateFirstPersonCueSeparation();
    valid &= validatePeripheralScales();
    valid &= validateProximityArrowScale();
    return valid ? 0 : 1;
}
