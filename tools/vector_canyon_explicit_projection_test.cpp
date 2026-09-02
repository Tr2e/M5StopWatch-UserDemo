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
    const std::size_t maximumFrameSegments = ribSegments + railSegments + maximumDiagonalSegments;
    valid &= check(visiblePoints > 700 && visiblePoints <= 850,
                   "M3 default camera projected an unexpected portion of the 850-point workset");
    valid &= check(maximumFrameSegments < 1600,
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
    valid &= validateLodContract();
    valid &= validateProjectionWorksetAndBudget(stream);
    return valid ? 0 : 1;
}
