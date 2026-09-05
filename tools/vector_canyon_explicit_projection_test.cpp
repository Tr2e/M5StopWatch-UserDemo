#include "../main/apps/app_vector_canyon_fighter/explicit_canyon_projection.h"
#include "../main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h"
#include "../main/apps/app_vector_canyon_fighter/vector_canyon_config.h"
#include "../main/apps/app_vector_canyon_fighter/vector_canyon_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

float vectorLength(CanyonCameraVector value)
{
    return std::sqrt(canyonDot(value, value));
}

bool validateCameraMatrix(const ExplicitCanyonStream& stream)
{
    bool valid = true;
    const CanyonRouteFrame route = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera camera = makeExplicitCanyonCamera(route, 0.5f, 0.0f, 468, 466);
    valid &= check(std::abs(vectorLength(camera.right) - 1.0f) < 0.0001f &&
                       std::abs(vectorLength(camera.up) - 1.0f) < 0.0001f &&
                       std::abs(vectorLength(camera.forward) - 1.0f) < 0.0001f,
                   "M3 camera axes are not normalized");
    valid &= check(std::abs(canyonDot(camera.right, camera.up)) < 0.0001f &&
                       std::abs(canyonDot(camera.right, camera.forward)) < 0.0001f &&
                       std::abs(canyonDot(camera.up, camera.forward)) < 0.0001f,
                   "M3 camera axes are not orthogonal");
    valid &= check(std::abs(camera.position.x - route.centerX) < 0.0001f &&
                       std::abs(camera.position.z - route.centerZ) < 0.0001f,
                   "M3 camera must remain on the route center without lateral double motion");
    valid &= check(std::abs(camera.position.y - 0.95f) < 0.0001f,
                   "M3 neutral camera height no longer matches the A1-A4 baseline");
    valid &= check(std::abs(camera.principalX - 234.0f) < 0.0001f &&
                       std::abs(camera.principalY - 161.702f) < 0.001f &&
                       std::abs(camera.focalLength - 387.036f) < 0.001f,
                   "M3 normalized 450px camera parameters were not mapped to StopWatch dimensions");

    const std::size_t slice = 8;
    const CanyonCameraPoint left = explicitCanyonToCamera(
        camera, stream.worldPoint(slice, static_cast<std::size_t>(CanyonProfilePoint::LeftFloorEdge)));
    const CanyonCameraPoint right = explicitCanyonToCamera(
        camera, stream.worldPoint(slice, static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge)));
    CanyonScreenPoint leftScreen{};
    CanyonScreenPoint rightScreen{};
    valid &= check(projectExplicitCanyonPoint(camera, left, leftScreen) &&
                       projectExplicitCanyonPoint(camera, right, rightScreen),
                   "M3 visible floor edges failed projection");
    valid &= check(leftScreen.x < rightScreen.x,
                   "M3 positive canyon lateral direction does not map to screen right");
    return valid;
}

bool validateNearClipping()
{
    bool valid = true;
    CanyonCameraPoint from{-0.4f, -0.2f, 0.05f};
    CanyonCameraPoint to{0.8f, 0.4f, 1.05f};
    valid &= check(clipExplicitCanyonSegmentToNear(from, to), "M3 crossing segment was rejected");
    valid &= check(std::abs(from.z - kExplicitCanyonNearPlane) < 0.000001f &&
                       to.z > kExplicitCanyonNearPlane,
                   "M3 crossing segment was not clipped at the near plane");

    CanyonCamera camera{};
    camera.principalX = 234.0f;
    camera.principalY = 161.702f;
    camera.focalLength = 387.036f;
    CanyonScreenPoint fromScreen{};
    CanyonScreenPoint toScreen{};
    valid &= check(projectExplicitCanyonPoint(camera, from, fromScreen) &&
                       projectExplicitCanyonPoint(camera, to, toScreen) &&
                       std::isfinite(fromScreen.x) && std::isfinite(fromScreen.y),
                   "M3 clipped segment produced invalid projection coordinates");

    CanyonCameraPoint behindA{0.0f, 0.0f, 0.02f};
    CanyonCameraPoint behindB{1.0f, 1.0f, 0.19f};
    valid &= check(!clipExplicitCanyonSegmentToNear(behindA, behindB),
                   "M3 segment fully behind the near plane was not rejected");
    CanyonScreenPoint rejected{};
    valid &= check(!projectExplicitCanyonPoint(camera, behindB, rejected),
                   "M3 point behind the near plane was projected directly");
    return valid;
}

bool validateHudReferenceFrame(const ExplicitCanyonStream& stream)
{
    const CanyonRouteFrame route = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera neutral =
        makeExplicitCanyonChaseCamera(route, 0.0f, 0.5f, 0.0f, 468, 466);
    CanyonHudReferenceFrame neutralHud{};
    bool valid = check(makeCanyonHudReferenceFrame(neutral, neutralHud),
                       "HUD world reference could not be projected");
    valid &= check(std::abs(neutralHud.horizonY - 140.74f) < 0.15f &&
                       std::abs(neutralHud.horizonSlope) < 0.0001f,
                   "HUD neutral horizon no longer overlays the world horizon");

    const CanyonCamera higher =
        makeExplicitCanyonChaseCamera(route, 0.0f, 1.2f, 0.0f, 468, 466);
    CanyonHudReferenceFrame higherHud{};
    valid &= check(makeCanyonHudReferenceFrame(higher, higherHud) &&
                       std::abs(higherHud.horizonY - neutralHud.horizonY) < 0.0001f,
                   "HUD horizon changed when only camera altitude changed");

    const CanyonCamera climb =
        makeExplicitCanyonChaseCamera(route, 0.0f, 0.5f, 18.0f, 468, 466);
    const CanyonCamera dive =
        makeExplicitCanyonChaseCamera(route, 0.0f, 0.5f, -18.0f, 468, 466);
    CanyonHudReferenceFrame climbHud{};
    CanyonHudReferenceFrame diveHud{};
    valid &= check(makeCanyonHudReferenceFrame(climb, climbHud) &&
                       makeCanyonHudReferenceFrame(dive, diveHud) &&
                       climbHud.horizonY > neutralHud.horizonY + 21.0f &&
                       diveHud.horizonY < neutralHud.horizonY - 21.0f,
                   "HUD horizon does not move down/up with chase-camera pitch");

    CanyonScreenPoint positiveFive{};
    CanyonScreenPoint negativeFive{};
    valid &= check(projectCanyonHudElevation(
                       neutral, neutralHud, 5.0f, positiveFive) &&
                       projectCanyonHudElevation(
                           neutral, neutralHud, -5.0f, negativeFive) &&
                       positiveFive.y < neutralHud.horizonY &&
                       negativeFive.y > neutralHud.horizonY,
                   "HUD pitch rungs have incorrect earth-relative ordering");
    return valid;
}

bool validateFarFieldConvergence(const ExplicitCanyonStream& stream)
{
    const CanyonRouteFrame route = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera camera = makeExplicitCanyonChaseCamera(
        route, 0.0f, 0.5f, 0.0f, 468, 466);
    CanyonHudReferenceFrame hud{};
    CanyonScreenPoint farFloor{};
    const bool projected = makeCanyonHudReferenceFrame(camera, hud) &&
        projectExplicitCanyonPoint(
            camera,
            explicitCanyonToCamera(
                camera,
                stream.farWorldPoint(
                    ExplicitCanyonStream::kFarSliceCount - 1,
                    static_cast<std::size_t>(CanyonProfilePoint::FloorCenter))),
            farFloor);
    const float farDepth = stream.farSlices().back().worldS -
                           stream.playerWorldS();
    bool valid = check(projected && farDepth > 90.0f,
                       "sparse far field does not extend beyond 90 world units");
    valid &= check(std::abs(farFloor.y - hud.horizonY) < 5.5f,
                   "far floor still fails to converge near the true HUD horizon");
    std::cout << "far_field_depth=" << farDepth << '\n';
    std::cout << "far_floor_horizon_gap_px="
              << std::abs(farFloor.y - hud.horizonY) << '\n';
    return valid;
}

bool validateCockpitHudReferenceFrame(const ExplicitCanyonStream& stream)
{
    const CanyonRouteFrame route = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera neutral = makeExplicitCanyonCockpitCamera(
        route, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 468, 466);
    CanyonHudReferenceFrame neutralHud{};
    bool valid = check(makeCanyonHudReferenceFrame(neutral, neutralHud),
                       "cockpit HUD world reference could not be projected");
    valid &= check(std::abs(neutralHud.horizonY - 233.0f) < 0.15f &&
                       std::abs(neutralHud.horizonSlope) < 0.0001f,
                   "cockpit neutral horizon is not centered and conformal");

    const CanyonCamera climb = makeExplicitCanyonCockpitCamera(
        route, 0.0f, 0.5f, 18.0f, 0.0f, 0.0f, 468, 466);
    const CanyonCamera dive = makeExplicitCanyonCockpitCamera(
        route, 0.0f, 0.5f, -18.0f, 0.0f, 0.0f, 468, 466);
    CanyonHudReferenceFrame climbHud{};
    CanyonHudReferenceFrame diveHud{};
    valid &= check(makeCanyonHudReferenceFrame(climb, climbHud) &&
                       makeCanyonHudReferenceFrame(dive, diveHud) &&
                       climbHud.horizonY > neutralHud.horizonY + 115.0f &&
                       diveHud.horizonY < neutralHud.horizonY - 115.0f,
                   "cockpit horizon does not follow full aircraft pitch");

    const CanyonCamera banked = makeExplicitCanyonCockpitCamera(
        route, 0.0f, 0.5f, 0.0f, 30.0f, 0.0f, 468, 466);
    CanyonHudReferenceFrame bankedHud{};
    valid &= check(makeCanyonHudReferenceFrame(banked, bankedHud) &&
                       std::abs(std::abs(bankedHud.horizonSlope) - 0.57735f) < 0.01f,
                   "cockpit horizon does not share the camera bank angle");
    valid &= check(std::abs(vectorLength(banked.right) - 1.0f) < 0.0001f &&
                       std::abs(vectorLength(banked.up) - 1.0f) < 0.0001f &&
                       std::abs(canyonDot(banked.right, banked.up)) < 0.0001f,
                   "cockpit roll broke the camera orthonormal basis");

    const CanyonCamera yawed = makeExplicitCanyonCockpitCamera(
        route, 0.0f, 0.5f, 0.0f, 0.0f, 8.0f, 468, 466);
    valid &= check(canyonDot(neutral.forward, yawed.forward) < 0.995f,
                   "cockpit camera ignored aircraft turn yaw");
    return valid;
}

bool validateLodContract()
{
    bool valid = true;
    std::size_t structuralCount = 0;
    std::size_t midCount = 0;
    for (std::size_t profile = 0; profile < kExplicitCanyonProfileCount; ++profile) {
        if (isExplicitCanyonStructuralRail(profile)) ++structuralCount;
        if (isExplicitCanyonMidRail(profile)) ++midCount;
    }
    valid &= check(structuralCount == 7, "M3 structural LOD must contain exactly seven semantic rails");
    valid &= check(midCount == 4, "M3 mid LOD must contain exactly four semantic rails");
    valid &= check(explicitCanyonRailLodWeight(0, 1000.0f) == 1.0f,
                   "M3 outer plateau structural rail disappeared in the far field");
    valid &= check(explicitCanyonRailLodWeight(5, 20.0f) == 1.0f &&
                       explicitCanyonRailLodWeight(5, 25.0f) == 0.0f,
                   "M3 mid rail fade range changed");
    valid &= check(explicitCanyonRailLodWeight(1, 9.0f) == 1.0f &&
                       explicitCanyonRailLodWeight(1, 14.0f) == 0.0f,
                   "M3 fine rail fade range changed");
    for (std::size_t profile = 0; profile < kExplicitCanyonProfileCount; ++profile) {
        float previous = 1.0f;
        for (float depth = 0.0f; depth <= 40.0f; depth += 0.05f) {
            const float weight = explicitCanyonRailLodWeight(profile, depth);
            valid &= check(weight <= previous + 0.00001f && weight >= 0.0f && weight <= 1.0f,
                           "M3 LOD weight is discontinuous or outside [0,1]");
            previous = weight;
        }
    }
    return valid;
}

bool validateProjectionWorksetAndBudget(const ExplicitCanyonStream& stream)
{
    bool valid = true;
    const CanyonCamera camera =
        makeExplicitCanyonCamera(stream.routeFrameAt(stream.playerWorldS()), 0.5f, 0.0f, 468, 466);
    std::size_t visiblePoints = 0;
    for (std::size_t slice = 0; slice < ExplicitCanyonStream::kSliceCount; ++slice) {
        for (std::size_t profile = 0; profile < ExplicitCanyonStream::kProfileCount; ++profile) {
            CanyonScreenPoint screen{};
            if (projectExplicitCanyonPoint(
                    camera, explicitCanyonToCamera(camera, stream.worldPoint(slice, profile)), screen)) {
                ++visiblePoints;
                valid &= check(std::isfinite(screen.x) && std::isfinite(screen.y),
                               "M3 production workset contains a non-finite projected point");
            }
        }
    }

    constexpr std::size_t ribSegments =
        ExplicitCanyonStream::kSliceCount * (ExplicitCanyonStream::kProfileCount - 1);
    std::size_t railSegments = 0;
    for (std::size_t profile = 0; profile < ExplicitCanyonStream::kProfileCount; ++profile) {
        for (std::size_t slice = 1; slice < ExplicitCanyonStream::kSliceCount; ++slice) {
            const float worldS = (stream.slices()[slice - 1].worldS + stream.slices()[slice].worldS) * 0.5f;
            if (explicitCanyonRailLodWeight(profile, worldS - stream.playerWorldS()) > 0.01f) ++railSegments;
        }
    }
    constexpr std::size_t maximumDiagonalSegments =
        ((ExplicitCanyonStream::kSliceCount + 1) / 2) * 4;
    constexpr std::size_t farRibSegments =
        ExplicitCanyonStream::kFarSliceCount *
        (ExplicitCanyonStream::kProfileCount - 1);
    constexpr std::size_t farRailSegments =
        ExplicitCanyonStream::kFarSliceCount * 7;
    const std::size_t maximumFrameSegments = ribSegments + railSegments +
        maximumDiagonalSegments + farRibSegments + farRailSegments;
    valid &= check(visiblePoints > 700 && visiblePoints <= 850,
                   "M3 default camera projected an unexpected portion of the 850-point workset");
    valid &= check(maximumFrameSegments < 1700,
                   "M3 explicit line budget exceeded the reviewed thousand-level range");

    std::cout << "visible_points=" << visiblePoints << '\n';
    std::cout << "host_renderer_size=" << sizeof(Renderer) << '\n';
    std::cout << "rib_segments=" << ribSegments << '\n';
    std::cout << "lod_rail_segments=" << railSegments << '\n';
    std::cout << "maximum_frame_segments=" << maximumFrameSegments << '\n';
    return valid;
}

}  // namespace

int main()
{
    ExplicitCanyonStream stream;
    stream.reset(0xC4A71001u);
    bool valid = validateCameraMatrix(stream);
    valid &= validateNearClipping();
    valid &= validateHudReferenceFrame(stream);
    valid &= validateFarFieldConvergence(stream);
    valid &= validateCockpitHudReferenceFrame(stream);
    valid &= validateLodContract();
    valid &= validateProjectionWorksetAndBudget(stream);
    return valid ? 0 : 1;
}
