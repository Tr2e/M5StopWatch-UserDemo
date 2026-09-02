#include "../main/apps/app_vector_canyon_fighter/explicit_canyon_projection.h"
#include "../main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h"
#include "../main/apps/app_vector_canyon_fighter/vector_canyon_config.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool validateStraightBaseline(ExplicitCanyonStream& stream)
{
    bool valid = true;
    const auto beforeUpdate = stream.slices();
    valid &= check(stream.playerWorldS() == 0.0f, "M4 static baseline player position moved at reset");
    valid &= check(stream.eventWindow().count == 0 && !stream.eventWindow().overflow,
                   "M4 static baseline unexpectedly contains gameplay events");

    for (std::size_t sliceIndex = 0; sliceIndex < stream.slices().size(); ++sliceIndex) {
        const auto& slice = stream.slices()[sliceIndex];
        const float expectedWorldS = static_cast<float>(sliceIndex) * ExplicitCanyonStream::kSliceSpacing;
        valid &= check(slice.segmentId == sliceIndex && std::abs(slice.worldS - expectedWorldS) < 0.0001f,
                       "M4 baseline segment identity or spacing changed");
        valid &= check(slice.centerX == 0.0f && std::abs(slice.centerZ - expectedWorldS) < 0.0001f &&
                           slice.tangentX == 0.0f && slice.tangentZ == 1.0f,
                       "M4 baseline route is not perfectly straight");
        valid &= check(slice.leftWidth == kExplicitCanyonFloorHalfWidth &&
                           slice.rightWidth == kExplicitCanyonFloorHalfWidth,
                       "M4 baseline contains a shoulder intrusion");
        const CanyonBoundary boundary = stream.boundaryAt(slice.worldS);
        valid &= check(boundary.leftWidth == kExplicitCanyonFloorHalfWidth &&
                           boundary.rightWidth == kExplicitCanyonFloorHalfWidth,
                       "M4 baseline boundary query is not constant");
        for (std::size_t profile = 0; profile < kExplicitCanyonProfileCount; ++profile) {
            const CanyonWorldPoint point = stream.worldPoint(sliceIndex, profile);
            valid &= check(std::abs(point.x - kExplicitCanyonProfile[profile].lateral) < 0.0001f &&
                               std::abs(point.y - kExplicitCanyonProfile[profile].height) < 0.0001f &&
                               std::abs(point.z - expectedWorldS) < 0.0001f,
                           "M4 baseline world point departed from the reviewed A1 profile");
        }
    }

    stream.update(100000.0f);
    valid &= check(stream.playerWorldS() == 0.0f && stream.firstSegment() == 0,
                   "M4 static baseline responded to flight movement");
    for (std::size_t slice = 0; slice < stream.slices().size(); ++slice) {
        valid &= check(stream.slices()[slice].segmentId == beforeUpdate[slice].segmentId &&
                           stream.slices()[slice].worldS == beforeUpdate[slice].worldS &&
                           stream.slices()[slice].centerZ == beforeUpdate[slice].centerZ,
                       "M4 static baseline cache changed after update");
    }
    return valid;
}

bool validateStaticComposition(const ExplicitCanyonStream& stream)
{
    bool valid = true;
    const CanyonCamera camera =
        makeExplicitCanyonCamera(stream.routeFrameAt(0.0f), 0.5f, 0.0f, 468, 466);
    std::size_t visiblePoints = 0;
    float minimumX = 100000.0f;
    float maximumX = -100000.0f;
    float minimumY = 100000.0f;
    float maximumY = -100000.0f;
    for (std::size_t slice = 0; slice < ExplicitCanyonStream::kSliceCount; ++slice) {
        for (std::size_t profile = 0; profile < ExplicitCanyonStream::kProfileCount; ++profile) {
            CanyonScreenPoint screen{};
            if (!projectExplicitCanyonPoint(
                    camera, explicitCanyonToCamera(camera, stream.worldPoint(slice, profile)), screen)) {
                continue;
            }
            ++visiblePoints;
            minimumX = std::min(minimumX, screen.x);
            maximumX = std::max(maximumX, screen.x);
            minimumY = std::min(minimumY, screen.y);
            maximumY = std::max(maximumY, screen.y);
        }
    }

    constexpr std::size_t kReviewSlice = 10;
    CanyonScreenPoint leftCap{};
    CanyonScreenPoint leftFloor{};
    CanyonScreenPoint rightFloor{};
    CanyonScreenPoint rightCap{};
    valid &= check(projectExplicitCanyonPoint(
                       camera,
                       explicitCanyonToCamera(camera, stream.worldPoint(
                           kReviewSlice, static_cast<std::size_t>(CanyonProfilePoint::LeftCap))),
                       leftCap) &&
                       projectExplicitCanyonPoint(
                           camera,
                           explicitCanyonToCamera(camera, stream.worldPoint(
                               kReviewSlice, static_cast<std::size_t>(CanyonProfilePoint::LeftFloorEdge))),
                           leftFloor) &&
                       projectExplicitCanyonPoint(
                           camera,
                           explicitCanyonToCamera(camera, stream.worldPoint(
                               kReviewSlice, static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge))),
                           rightFloor) &&
                       projectExplicitCanyonPoint(
                           camera,
                           explicitCanyonToCamera(camera, stream.worldPoint(
                               kReviewSlice, static_cast<std::size_t>(CanyonProfilePoint::RightCap))),
                           rightCap),
                   "M4 review cross-section failed projection");
    valid &= check(leftCap.x < leftFloor.x && leftFloor.x < rightFloor.x && rightFloor.x < rightCap.x,
                   "M4 cliff and floor ordering is not readable on screen");
    valid &= check(leftCap.y < leftFloor.y && rightCap.y < rightFloor.y,
                   "M4 cliff caps are not visibly above the flat floor");
    valid &= check(std::abs(leftFloor.y - rightFloor.y) < 0.001f,
                   "M4 projected valley floor is not flat");
    valid &= check(std::abs(leftCap.y - rightCap.y) < 0.001f,
                   "M4 projected plateau top is not flat");
    valid &= check(visiblePoints == 825, "M4 baseline visible workset changed unexpectedly");
    valid &= check(minimumX < 0.0f && maximumX > 468.0f && minimumY < 0.0f && maximumY > 300.0f,
                   "M4 geometry no longer fills and crops naturally within the round display");

    std::cout << "visible_points=" << visiblePoints << '\n';
    std::cout << "projected_bounds=" << minimumX << ',' << minimumY << ".." << maximumX << ',' << maximumY << '\n';
    std::cout << "review_floor_y=" << leftFloor.y << '\n';
    std::cout << "review_cap_y=" << leftCap.y << '\n';
    return valid;
}

}  // namespace

int main()
{
    ExplicitCanyonStream stream;
    stream.resetStraightBaseline();
    bool valid = validateStraightBaseline(stream);
    valid &= validateStaticComposition(stream);
    return valid ? 0 : 1;
}
