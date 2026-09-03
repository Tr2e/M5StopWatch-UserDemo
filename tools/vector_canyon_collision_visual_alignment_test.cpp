#include "../main/apps/app_vector_canyon_fighter/explicit_canyon_projection.h"
#include "../main/apps/app_vector_canyon_fighter/model/collision_model.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

CanyonWorldPoint routePoint(const ExplicitCanyonStream& stream, float worldS,
                            float lateral, float height)
{
    const CanyonRouteFrame route = stream.routeFrameAt(worldS);
    return {
        route.centerX + route.tangentZ * lateral,
        height,
        route.centerZ - route.tangentX * lateral,
    };
}

bool projectWorld(const CanyonCamera& camera, CanyonWorldPoint world, CanyonScreenPoint& screen)
{
    return projectExplicitCanyonPoint(camera, explicitCanyonToCamera(camera, world), screen);
}

bool validateSideContact(CanyonSide side, uint32_t eventIndex)
{
    ExplicitCanyonStream stream;
    stream.reset(0xC4A71001u);
    const CanyonShoulderEvent event = stream.eventAtIndex(eventIndex);
    const AircraftCollisionStation wing = kAircraftCollisionStations[kAircraftWingStationIndex];
    const float playerS = event.centerWorldS - wing.forwardLead;
    stream.update(playerS / ExplicitCanyonStream::kForwardDistanceScale);

    FlightState flight{};
    flight.altitude = 0.5f;
    const CanyonBoundary boundary = stream.boundaryAt(event.centerWorldS);
    const float signedSide = side == CanyonSide::Left ? -1.0f : 1.0f;
    const float wallWidth = (side == CanyonSide::Left ? boundary.leftWidth : boundary.rightWidth) +
                            explicitCanyonWallOutsetAtHeight(flight.altitude);
    flight.lateralOffset = signedSide * (wallWidth - wing.halfWidth);

    const CanyonRouteFrame playerRoute = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera camera = makeExplicitCanyonChaseCamera(
        playerRoute, flight.lateralOffset, flight.altitude, 0.0f, 468, 466);
    CanyonScreenPoint wingTip{};
    CanyonScreenPoint wallSurface{};
    CanyonScreenPoint aircraftCenter{};
    bool valid = check(
        projectWorld(camera, routePoint(stream, event.centerWorldS,
                                        flight.lateralOffset + signedSide * wing.halfWidth,
                                        flight.altitude), wingTip) &&
            projectWorld(camera, routePoint(stream, event.centerWorldS,
                                            signedSide * wallWidth, flight.altitude), wallSurface) &&
            projectWorld(camera, routePoint(stream, event.centerWorldS,
                                            flight.lateralOffset, flight.altitude), aircraftCenter),
        "M8 wall-contact points were not projectable");
    const float visualGap = std::hypot(wingTip.x - wallSurface.x, wingTip.y - wallSurface.y);
    const float projectedHalfSpan = std::abs(wingTip.x - aircraftCenter.x);
    valid &= check(visualGap < 0.25f,
                   "M8 collision contact does not coincide with the rendered wall surface");
    valid &= check(std::abs(projectedHalfSpan - kAircraftNominalScreenHalfSpan) < 8.0f,
                   "M8 world collision footprint no longer matches the aircraft screen span");

    flight.lateralOffset = signedSide * (wallWidth - wing.halfWidth - 0.015f);
    CollisionStatus status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(!status.collided,
                   "M8 aircraft collided before its projected wing reached the wall");
    flight.lateralOffset = signedSide * (wallWidth - wing.halfWidth + 0.015f);
    status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(status.collided &&
                       status.impactHazard == (side == CanyonSide::Left
                                                   ? CollisionHazard::LeftWall
                                                   : CollisionHazard::RightWall),
                   "M8 aircraft did not collide after its projected wing crossed the wall");

    std::cout << (side == CanyonSide::Left ? "left" : "right")
              << "_contact_gap_px=" << visualGap
              << " half_span_px=" << projectedHalfSpan << '\n';
    return valid;
}

float projectedFloorY(const ExplicitCanyonStream& stream, float altitude, float pitch)
{
    const CanyonRouteFrame playerRoute = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera camera = makeExplicitCanyonChaseCamera(
        playerRoute, 0.0f, altitude, pitch, 468, 466);
    CanyonScreenPoint floor{};
    projectWorld(camera,
                 routePoint(stream, stream.playerWorldS() +
                                        kAircraftCollisionStations[kAircraftWingStationIndex].forwardLead,
                            0.0f, 0.0f),
                 floor);
    return floor.y;
}

bool validateAttitudeAndHeightCue()
{
    bool valid = true;
    valid &= check(kAircraftScreenCenterY == 320 &&
                       (466 - kAircraftScreenCenterY) >= 140 &&
                       (466 - kAircraftScreenCenterY) <= 156,
                   "M8 aircraft is no longer positioned near the lower screen third");
    valid &= check(kAircraftScreenCenterY - kAircraftCourseCueY >= 40,
                   "M8 course cue overlaps the raised aircraft datum");
    const AircraftScreenOffset noseUp = projectAircraftPose(0.0f, 0.02f, 3.70f, 12.0f, 0.0f);
    const AircraftScreenOffset tailUp = projectAircraftPose(0.0f, -0.22f, -2.34f, 12.0f, 0.0f);
    const AircraftScreenOffset noseDown = projectAircraftPose(0.0f, 0.02f, 3.70f, -12.0f, 0.0f);
    const AircraftScreenOffset tailDown = projectAircraftPose(0.0f, -0.22f, -2.34f, -12.0f, 0.0f);
    const float upSlope = noseUp.y - tailUp.y;
    const float downSlope = noseDown.y - tailDown.y;
    valid &= check(upSlope < downSlope - 18.0f,
                   "M8 aircraft silhouette does not visibly change with pitch");

    const AircraftScreenOffset leftWing = projectAircraftPose(-3.06f, 0.62f, -1.62f, 0.0f, 18.0f);
    const AircraftScreenOffset rightWing = projectAircraftPose(3.06f, 0.62f, -1.62f, 0.0f, 18.0f);
    valid &= check(std::abs(leftWing.y - rightWing.y) > 24.0f,
                   "M8 aircraft silhouette does not visibly bank with roll");

    ExplicitCanyonStream stream;
    stream.resetStraightBaseline();
    const float lowFloorY = projectedFloorY(stream, 0.25f, 0.0f);
    const float highFloorY = projectedFloorY(stream, 1.10f, 0.0f);
    valid &= check(std::abs(lowFloorY - highFloorY) > 120.0f,
                   "M8 terrain projection does not preserve a strong height separation cue");
    const float pitchLow = projectedFloorY(stream, 0.20f, -12.0f);
    const float pitchHigh = projectedFloorY(stream, 0.20f, 12.0f);
    valid &= check(std::abs(pitchLow - pitchHigh) < 45.0f,
                   "M8 chase camera pitch follow displaced floor contact excessively");
    const CanyonRouteFrame thresholdRoute = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera thresholdCamera = makeExplicitCanyonChaseCamera(
        thresholdRoute, 0.0f, kAircraftFloorClearance, 0.0f, 468, 466);
    CanyonScreenPoint thresholdTail{};
    projectWorld(thresholdCamera,
                 routePoint(stream, stream.playerWorldS() +
                                        kAircraftCollisionStations.front().forwardLead,
                            0.0f, 0.0f),
                 thresholdTail);
    valid &= check(std::abs(thresholdTail.y - kAircraftNominalScreenBottomY) < 8.0f,
                   "M8 floor collision threshold is not visually adjacent to the aircraft belly");

    const AircraftGroundShadow lowShadow = makeAircraftGroundShadow(0.0f);
    const AircraftGroundShadow highShadow = makeAircraftGroundShadow(1.20f);
    valid &= check(lowShadow.radiusX <= 16 && lowShadow.radiusY <= 4 &&
                       highShadow.radiusX <= lowShadow.radiusX &&
                       highShadow.radiusY <= lowShadow.radiusY,
                   "M8 ground shadow exceeded its compact footprint or grew with altitude");
    valid &= check(lowShadow.centerY - kAircraftNominalScreenBottomY <= 9.0f &&
                       highShadow.centerY <= 394,
                   "M8 ground shadow is not bounded between aircraft and lower HUD");
    valid &= check(lowShadow.brightness > highShadow.brightness,
                   "M8 ground shadow does not fade with altitude");

    std::cout << "attitude_slope_up_down=" << upSlope << ',' << downSlope << '\n';
    std::cout << "floor_y_low_high=" << lowFloorY << ',' << highFloorY
              << " threshold_pitch_range=" << std::abs(pitchLow - pitchHigh)
              << " threshold_tail_y=" << thresholdTail.y
              << " shadow_low_high_y=" << lowShadow.centerY << ',' << highShadow.centerY
              << " shadow_low_high_rx=" << lowShadow.radiusX << ',' << highShadow.radiusX << '\n';
    return valid;
}

bool validateWallProfile()
{
    bool valid = true;
    float previous = 0.0f;
    for (int step = 0; step <= 38; ++step) {
        const float height = static_cast<float>(step) * 0.10f;
        const float outset = explicitCanyonWallOutsetAtHeight(height);
        valid &= check(outset + 0.0001f >= previous,
                       "M8 wall surface inset decreased with height");
        previous = outset;
    }
    valid &= check(std::abs(explicitCanyonWallOutsetAtHeight(0.50f) - 0.2512f) < 0.01f,
                   "M8 low-altitude wall surface no longer follows the rendered toe");
    valid &= check(std::abs(explicitCanyonWallOutsetAtHeight(1.35f) - 0.527f) < 0.03f,
                   "M8 high-altitude wall surface no longer follows the rendered face");
    return valid;
}

}  // namespace

int main()
{
    bool valid = validateSideContact(CanyonSide::Left, 0);
    valid &= validateSideContact(CanyonSide::Right, 1);
    valid &= validateAttitudeAndHeightCue();
    valid &= validateWallProfile();
    return valid ? 0 : 1;
}
