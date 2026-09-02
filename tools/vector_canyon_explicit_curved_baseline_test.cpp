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

float length2(float x, float z)
{
    return std::sqrt(x * x + z * z);
}

bool validateCurvedFrames(ExplicitCanyonStream& stream)
{
    bool valid = true;
    const auto beforeUpdate = stream.slices();
    float minimumCenterX = stream.slices().front().centerX;
    float maximumCenterX = minimumCenterX;
    float minimumStep = 1000.0f;
    float maximumStep = 0.0f;
    float minimumForwardAlignment = 1.0f;
    float minimumTangentContinuity = 1.0f;

    valid &= check(stream.playerWorldS() == 0.0f && stream.firstSegment() == 0,
                   "M5 curved preview did not reset at the route origin");
    valid &= check(stream.eventWindow().count == 0 && !stream.eventWindow().overflow,
                   "M5 curved preview unexpectedly enabled shoulder events");

    for (std::size_t index = 0; index < stream.slices().size(); ++index) {
        const ExplicitCanyonSlice& slice = stream.slices()[index];
        const float tangentLength = length2(slice.tangentX, slice.tangentZ);
        valid &= check(std::abs(tangentLength - 1.0f) < 0.0002f,
                       "M5 route tangent is not normalized");
        valid &= check(slice.leftWidth == kExplicitCanyonFloorHalfWidth &&
                           slice.rightWidth == kExplicitCanyonFloorHalfWidth,
                       "M5 curved preview contains a cliff intrusion");
        const CanyonBoundary boundary = stream.boundaryAt(slice.worldS);
        valid &= check(boundary.leftWidth == kExplicitCanyonFloorHalfWidth &&
                           boundary.rightWidth == kExplicitCanyonFloorHalfWidth,
                       "M5 curved preview boundary query is not constant");
        minimumCenterX = std::min(minimumCenterX, slice.centerX);
        maximumCenterX = std::max(maximumCenterX, slice.centerX);

        const CanyonWorldPoint left =
            stream.worldPoint(index, static_cast<std::size_t>(CanyonProfilePoint::LeftFloorEdge));
        const CanyonWorldPoint right =
            stream.worldPoint(index, static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge));
        const float rowX = right.x - left.x;
        const float rowZ = right.z - left.z;
        const float normalX = slice.tangentZ;
        const float normalZ = -slice.tangentX;
        valid &= check(std::abs(rowX * slice.tangentX + rowZ * slice.tangentZ) < 0.0002f,
                       "M5 cross-row is not perpendicular to its local route tangent");
        valid &= check(rowX * normalX + rowZ * normalZ > 0.0f,
                       "M5 local frame mirrored the left and right canyon sides");

        if (index == 0) continue;
        const ExplicitCanyonSlice& previous = stream.slices()[index - 1];
        const float deltaX = slice.centerX - previous.centerX;
        const float deltaZ = slice.centerZ - previous.centerZ;
        const float step = length2(deltaX, deltaZ);
        minimumStep = std::min(minimumStep, step);
        maximumStep = std::max(maximumStep, step);
        const float forwardAlignment =
            (deltaX * previous.tangentX + deltaZ * previous.tangentZ) / std::max(step, 0.00001f);
        minimumForwardAlignment = std::min(minimumForwardAlignment, forwardAlignment);
        const float tangentContinuity =
            slice.tangentX * previous.tangentX + slice.tangentZ * previous.tangentZ;
        minimumTangentContinuity = std::min(minimumTangentContinuity, tangentContinuity);
        valid &= check(forwardAlignment > 0.97f,
                       "M5 arc-length slice moved backward relative to the route frame");
        valid &= check(tangentContinuity > 0.94f,
                       "M5 neighboring route frames flipped or turned discontinuously");
    }

    valid &= check(maximumCenterX - minimumCenterX > 2.2f,
                   "M5 route is too straight to exercise the curved-frame path");
    valid &= check(std::abs(minimumStep - ExplicitCanyonStream::kSliceSpacing) < 0.025f &&
                       std::abs(maximumStep - ExplicitCanyonStream::kSliceSpacing) < 0.025f,
                   "M5 slice centers are not approximately equal arc-length samples");

    stream.update(100000.0f);
    valid &= check(stream.playerWorldS() == 0.0f && stream.firstSegment() == 0,
                   "M5 static curved preview responded to flight movement");
    for (std::size_t index = 0; index < stream.slices().size(); ++index) {
        valid &= check(stream.slices()[index].centerX == beforeUpdate[index].centerX &&
                           stream.slices()[index].centerZ == beforeUpdate[index].centerZ,
                       "M5 curved preview cache changed after update");
    }

    std::cout << "center_x_span=" << maximumCenterX - minimumCenterX << '\n';
    std::cout << "center_step_range=" << minimumStep << ".." << maximumStep << '\n';
    std::cout << "minimum_forward_alignment=" << minimumForwardAlignment << '\n';
    std::cout << "minimum_tangent_continuity=" << minimumTangentContinuity << '\n';
    return valid;
}

bool validateProjectedRows(const ExplicitCanyonStream& stream)
{
    bool valid = true;
    const CanyonCamera camera =
        makeExplicitCanyonCamera(stream.routeFrameAt(0.0f), 0.5f, 0.0f, 468, 466);
    std::size_t reviewedRows = 0;
    float minimumScreenWidth = 100000.0f;
    float maximumRowAngle = -100000.0f;
    float minimumRowAngle = 100000.0f;

    for (std::size_t index = 2; index < stream.slices().size(); ++index) {
        CanyonScreenPoint left{};
        CanyonScreenPoint right{};
        if (!projectExplicitCanyonPoint(
                camera,
                explicitCanyonToCamera(camera, stream.worldPoint(
                    index, static_cast<std::size_t>(CanyonProfilePoint::LeftFloorEdge))),
                left) ||
            !projectExplicitCanyonPoint(
                camera,
                explicitCanyonToCamera(camera, stream.worldPoint(
                    index, static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge))),
                right)) {
            continue;
        }
        ++reviewedRows;
        valid &= check(left.x < right.x,
                       "M5 projected cross-row mirrored its left and right floor edges");
        const float screenWidth = right.x - left.x;
        minimumScreenWidth = std::min(minimumScreenWidth, screenWidth);
        const float angle = std::atan2(right.y - left.y, screenWidth);
        minimumRowAngle = std::min(minimumRowAngle, angle);
        maximumRowAngle = std::max(maximumRowAngle, angle);
    }

    valid &= check(reviewedRows >= 28, "M5 too few curved cross-rows remain visible for review");
    valid &= check(minimumScreenWidth > 0.0f,
                   "M5 one or more projected cross-rows collapsed or reversed");
    valid &= check(maximumRowAngle - minimumRowAngle > 0.01f,
                   "M5 cross-rows do not visibly rotate with the curved route");

    std::cout << "reviewed_projected_rows=" << reviewedRows << '\n';
    std::cout << "minimum_projected_row_width=" << minimumScreenWidth << '\n';
    std::cout << "projected_row_angle_span=" << maximumRowAngle - minimumRowAngle << '\n';
    return valid;
}

bool validateTopDebugCamera(const ExplicitCanyonStream& stream)
{
    bool valid = true;
    const CanyonCamera camera =
        makeExplicitCanyonTopDebugCamera(stream.routeFrameAt(0.0f), 468, 466);
    valid &= check(std::abs(length2(camera.right.x, camera.right.z) - 1.0f) < 0.0002f,
                   "M5 top debug camera right axis is not horizontal and normalized");
    valid &= check(std::abs(canyonDot(camera.right, camera.up)) < 0.0002f &&
                       std::abs(canyonDot(camera.right, camera.forward)) < 0.0002f &&
                       std::abs(canyonDot(camera.up, camera.forward)) < 0.0002f,
                   "M5 top debug camera axes are not orthogonal");
    std::size_t visibleCenters = 0;
    for (std::size_t index = 0; index < stream.slices().size(); ++index) {
        CanyonScreenPoint center{};
        const CanyonWorldPoint point{
            stream.slices()[index].centerX,
            0.0f,
            stream.slices()[index].centerZ,
        };
        if (projectExplicitCanyonPoint(camera, explicitCanyonToCamera(camera, point), center)) {
            ++visibleCenters;
        }
    }
    valid &= check(visibleCenters >= 30,
                   "M5 top debug camera cannot expose enough of the centerline for frame diagnosis");
    std::cout << "top_debug_visible_centers=" << visibleCenters << '\n';
    return valid;
}

}  // namespace

int main()
{
    static_assert(VECTOR_CANYON_EXPLICIT_TERRAIN == 1,
                  "M5 must enable the explicit terrain path");
    static_assert(VECTOR_CANYON_EXPLICIT_PREVIEW == 1,
                  "M5 must keep the isolated preview path enabled");
    static_assert(VECTOR_CANYON_EXPLICIT_STATIC_BASELINE == 0,
                  "M5 must disable the straight baseline");

    ExplicitCanyonStream stream;
    stream.resetCurvedBaseline(0xC4A71001u);
    bool valid = validateCurvedFrames(stream);
    valid &= validateProjectedRows(stream);
    valid &= validateTopDebugCamera(stream);
    return valid ? 0 : 1;
}
