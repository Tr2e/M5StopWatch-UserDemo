#include "../main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h"
#include "../main/apps/app_vector_canyon_fighter/vector_canyon_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

constexpr uint32_t kSeed = 0xC4A71001u;
constexpr float kEpsilon = 0.001f;

struct StreamMetrics {
    uint64_t hash = 1469598103934665603ull;
    uint64_t routeHash = 1469598103934665603ull;
    uint32_t recycleCount = 0;
    uint32_t maximumWindowEventCount = 0;
    uint32_t windowOverflowCount = 0;
    float minimumTotalWidth = 1000.0f;
    float maximumCachedError = 0.0f;
    float maximumAdjacentBoundaryDelta = 0.0f;
    float minimumReachabilityMargin = 1000.0f;
    float maximumRouteSpacingError = 0.0f;
    float maximumRouteFrameError = 0.0f;
    float maximumRouteCurvatureWidthProduct = 0.0f;
    float minimumOuterRailForwardDot = 1000.0f;
};

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

float pointDistance(const ExplicitCanyonSlice& left, const ExplicitCanyonSlice& right)
{
    const float x = right.centerX - left.centerX;
    const float z = right.centerZ - left.centerZ;
    return std::sqrt(x * x + z * z);
}

float localLateral(const ExplicitCanyonSlice& slice, const CanyonWorldPoint& point)
{
    const float normalX = slice.tangentZ;
    const float normalZ = -slice.tangentX;
    return (point.x - slice.centerX) * normalX + (point.z - slice.centerZ) * normalZ;
}

bool validateProfile()
{
    bool valid = true;
    for (std::size_t point = 7; point <= 17; ++point) {
        valid &= check(std::abs(kExplicitCanyonProfile[point].height) < kEpsilon, "A1 floor must be flat");
    }
    for (std::size_t point = 0; point <= 3; ++point) {
        valid &= check(std::abs(kExplicitCanyonProfile[point].height - kExplicitCanyonWallHeight) < kEpsilon,
                       "A1 left plateau must be flat");
    }
    for (std::size_t point = 21; point < kExplicitCanyonProfile.size(); ++point) {
        valid &= check(std::abs(kExplicitCanyonProfile[point].height - kExplicitCanyonWallHeight) < kEpsilon,
                       "A1 right plateau must be flat");
    }
    for (std::size_t point = 0; point < kExplicitCanyonProfile.size(); ++point) {
        const auto& left = kExplicitCanyonProfile[point];
        const auto& right = kExplicitCanyonProfile[kExplicitCanyonProfile.size() - 1 - point];
        valid &= check(std::abs(left.lateral + right.lateral) < kEpsilon &&
                           std::abs(left.height - right.height) < kEpsilon,
                       "A1 profile must be mirror symmetric before events");
    }
    const auto& faceHigh = kExplicitCanyonProfile[4];
    const auto& faceLow = kExplicitCanyonProfile[5];
    const float faceDegrees = std::atan2(faceHigh.height - faceLow.height,
                                         std::abs(faceHigh.lateral - faceLow.lateral)) * 57.2957795f;
    valid &= check(faceDegrees >= 78.0f && faceDegrees <= 85.0f, "A1 main cliff face angle left reviewed range");
    return valid;
}

bool validateRouteAndWorldPoints(ExplicitCanyonStream& stream)
{
    bool valid = true;
    const auto& slices = stream.slices();
    float minimumX = slices.front().centerX;
    float maximumX = slices.front().centerX;
    float maximumSpacingError = 0.0f;
    float maximumFrameError = 0.0f;
    float maximumCurvatureWidthProduct = 0.0f;

    for (std::size_t sliceIndex = 0; sliceIndex < slices.size(); ++sliceIndex) {
        const auto& slice = slices[sliceIndex];
        minimumX = std::min(minimumX, slice.centerX);
        maximumX = std::max(maximumX, slice.centerX);
        const float tangentLength = std::sqrt(slice.tangentX * slice.tangentX + slice.tangentZ * slice.tangentZ);
        maximumFrameError = std::max(maximumFrameError, std::abs(tangentLength - 1.0f));
        const CanyonRouteFrame queriedFrame = stream.routeFrameAt(slice.worldS);
        valid &= check(std::abs(queriedFrame.centerX - slice.centerX) < 0.002f &&
                           std::abs(queriedFrame.centerZ - slice.centerZ) < 0.002f,
                       "A2 cached route frame lookup selected the wrong slice");

        const CanyonWorldPoint leftFloor =
            stream.worldPoint(sliceIndex, static_cast<std::size_t>(CanyonProfilePoint::LeftFloorEdge));
        const CanyonWorldPoint rightFloor =
            stream.worldPoint(sliceIndex, static_cast<std::size_t>(CanyonProfilePoint::RightFloorEdge));
        const CanyonWorldPoint floorCenter =
            stream.worldPoint(sliceIndex, static_cast<std::size_t>(CanyonProfilePoint::FloorCenter));
        const CanyonWorldPoint leftPlateau =
            stream.worldPoint(sliceIndex, static_cast<std::size_t>(CanyonProfilePoint::LeftPlateauMid));
        valid &= check(std::abs(localLateral(slice, leftFloor) + slice.leftWidth) < 0.002f,
                       "A3 left floor edge must use the cached left boundary");
        valid &= check(std::abs(localLateral(slice, rightFloor) - slice.rightWidth) < 0.002f,
                       "A3 right floor edge must use the cached right boundary");
        valid &= check(std::abs(floorCenter.y) < kEpsilon, "A1 floor center height changed");
        valid &= check(std::abs(leftPlateau.y - kExplicitCanyonWallHeight) < kEpsilon,
                       "A1 plateau height changed");

        if (sliceIndex == 0) continue;
        const auto& previous = slices[sliceIndex - 1];
        const float spacing = pointDistance(previous, slice);
        maximumSpacingError = std::max(maximumSpacingError, std::abs(spacing - ExplicitCanyonStream::kSliceSpacing));
        const float tangentDot = std::clamp(previous.tangentX * slice.tangentX +
                                                previous.tangentZ * slice.tangentZ,
                                            -1.0f, 1.0f);
        const float curvature = std::acos(tangentDot) / std::max(spacing, 0.0001f);
        maximumCurvatureWidthProduct = std::max(maximumCurvatureWidthProduct, curvature * 7.0f);

        const CanyonWorldPoint previousOuter =
            stream.worldPoint(sliceIndex - 1, static_cast<std::size_t>(CanyonProfilePoint::RightPlateauOuter));
        const CanyonWorldPoint outer =
            stream.worldPoint(sliceIndex, static_cast<std::size_t>(CanyonProfilePoint::RightPlateauOuter));
        const float averageTangentX = previous.tangentX + slice.tangentX;
        const float averageTangentZ = previous.tangentZ + slice.tangentZ;
        valid &= check((outer.x - previousOuter.x) * averageTangentX +
                           (outer.z - previousOuter.z) * averageTangentZ > 0.0f,
                       "A2 outer rail folded against route direction");
    }

    valid &= check(maximumFrameError < 0.001f, "A2 tangent frame is not normalized");
    valid &= check(maximumSpacingError < 0.025f, "A2 arc-length slice spacing drifted");
    valid &= check(maximumCurvatureWidthProduct < 0.95f, "A2 route curvature can fold the seven-unit plateau");
    valid &= check(maximumX - minimumX >= 2.5f, "A2 route does not produce a visible curved centerline");

    std::cout << "maximum_spacing_error=" << maximumSpacingError << '\n';
    std::cout << "maximum_frame_error=" << maximumFrameError << '\n';
    std::cout << "maximum_curvature_width_product=" << maximumCurvatureWidthProduct << '\n';
    std::cout << "initial_route_x_span=" << maximumX - minimumX << '\n';
    return valid;
}

bool validateIndependentShoulders(ExplicitCanyonStream& stream)
{
    bool valid = true;
    const CanyonShoulderEvent left = stream.eventAtIndex(0);
    const CanyonShoulderEvent right = stream.eventAtIndex(1);
    const CanyonBoundary leftPeak = stream.boundaryAt(left.centerWorldS);
    const CanyonBoundary rightPeak = stream.boundaryAt(right.centerWorldS);
    valid &= check(left.side == CanyonSide::Left && right.side == CanyonSide::Right,
                   "A4 S-gate event order changed");
    valid &= check(std::abs(leftPeak.leftWidth - (kExplicitCanyonFloorHalfWidth - left.amplitude)) < kEpsilon &&
                       std::abs(leftPeak.rightWidth - kExplicitCanyonFloorHalfWidth) < kEpsilon,
                   "A3 left event modified the wrong boundary");
    valid &= check(std::abs(rightPeak.rightWidth - (kExplicitCanyonFloorHalfWidth - right.amplitude)) < kEpsilon &&
                       std::abs(rightPeak.leftWidth - kExplicitCanyonFloorHalfWidth) < kEpsilon,
                   "A3 right event modified the wrong boundary");
    const float recovery = (right.centerWorldS - right.halfLength) - (left.centerWorldS + left.halfLength);
    valid &= check(recovery > 0.0f, "A4 S-gate lost its recovery interval");
    return valid;
}

void hashBoundary(uint64_t& hash, const CanyonBoundary& boundary)
{
    const std::array<uint32_t, 2> values = {
        static_cast<uint32_t>(std::lround(boundary.leftWidth * 1000000.0f)),
        static_cast<uint32_t>(std::lround(boundary.rightWidth * 1000000.0f)),
    };
    for (const uint32_t value : values) {
        hash ^= value;
        hash *= 1099511628211ull;
    }
}

void hashRouteFrame(uint64_t& hash, const ExplicitCanyonSlice& slice)
{
    const std::array<int32_t, 4> values = {
        static_cast<int32_t>(std::lround(slice.centerX * 100000.0f)),
        static_cast<int32_t>(std::lround(slice.centerZ * 100000.0f)),
        static_cast<int32_t>(std::lround(slice.tangentX * 100000.0f)),
        static_cast<int32_t>(std::lround(slice.tangentZ * 100000.0f)),
    };
    for (const int32_t value : values) {
        hash ^= static_cast<uint32_t>(value);
        hash *= 1099511628211ull;
    }
}

StreamMetrics simulateStream(uint32_t seed)
{
    constexpr uint32_t kFrameCount = 1800;
    constexpr float kFramesPerSecond = 30.0f;
    constexpr float kWorstFlightSpeed = 176.0f;
    constexpr float kWorstTerrainSpeed = kWorstFlightSpeed * ExplicitCanyonStream::kForwardDistanceScale;
    constexpr float kMaxLateralVelocity = 1.8f;

    ExplicitCanyonStream stream;
    stream.reset(seed);
    StreamMetrics metrics{};
    uint32_t previousFirstSegment = stream.firstSegment();
    for (uint32_t frame = 0; frame < kFrameCount; ++frame) {
        stream.update(static_cast<float>(frame) * kWorstFlightSpeed / kFramesPerSecond);
        metrics.recycleCount += stream.firstSegment() - previousFirstSegment;
        previousFirstSegment = stream.firstSegment();
        metrics.maximumWindowEventCount =
            std::max(metrics.maximumWindowEventCount, static_cast<uint32_t>(stream.eventWindow().count));
        if (stream.eventWindow().overflow) ++metrics.windowOverflowCount;

        const auto& slices = stream.slices();
        for (std::size_t sliceIndex = 0; sliceIndex < slices.size(); ++sliceIndex) {
            const auto& slice = slices[sliceIndex];
            const CanyonBoundary recomputed = stream.boundaryAt(slice.worldS);
            metrics.maximumCachedError =
                std::max(metrics.maximumCachedError,
                         std::max(std::abs(recomputed.leftWidth - slice.leftWidth),
                                  std::abs(recomputed.rightWidth - slice.rightWidth)));
            metrics.minimumTotalWidth = std::min(metrics.minimumTotalWidth, slice.leftWidth + slice.rightWidth);
            hashBoundary(metrics.hash, {slice.leftWidth, slice.rightWidth});
            hashRouteFrame(metrics.routeHash, slice);
            const float tangentLength = std::sqrt(slice.tangentX * slice.tangentX + slice.tangentZ * slice.tangentZ);
            metrics.maximumRouteFrameError =
                std::max(metrics.maximumRouteFrameError, std::abs(tangentLength - 1.0f));
            if (sliceIndex > 0) {
                const auto& previous = slices[sliceIndex - 1];
                metrics.maximumAdjacentBoundaryDelta =
                    std::max(metrics.maximumAdjacentBoundaryDelta,
                             std::max(std::abs(slice.leftWidth - previous.leftWidth),
                                      std::abs(slice.rightWidth - previous.rightWidth)));
                const float spacing = pointDistance(previous, slice);
                metrics.maximumRouteSpacingError =
                    std::max(metrics.maximumRouteSpacingError,
                             std::abs(spacing - ExplicitCanyonStream::kSliceSpacing));
                const float tangentDot = std::clamp(previous.tangentX * slice.tangentX +
                                                        previous.tangentZ * slice.tangentZ,
                                                    -1.0f, 1.0f);
                const float curvature = std::acos(tangentDot) / std::max(spacing, 0.0001f);
                metrics.maximumRouteCurvatureWidthProduct =
                    std::max(metrics.maximumRouteCurvatureWidthProduct, curvature * 7.0f);
                const CanyonWorldPoint previousOuter =
                    stream.worldPoint(sliceIndex - 1,
                                      static_cast<std::size_t>(CanyonProfilePoint::RightPlateauOuter));
                const CanyonWorldPoint outer =
                    stream.worldPoint(sliceIndex,
                                      static_cast<std::size_t>(CanyonProfilePoint::RightPlateauOuter));
                const float forwardDot = (outer.x - previousOuter.x) * (previous.tangentX + slice.tangentX) +
                                         (outer.z - previousOuter.z) * (previous.tangentZ + slice.tangentZ);
                metrics.minimumOuterRailForwardDot = std::min(metrics.minimumOuterRailForwardDot, forwardDot);
            }
        }
    }

    for (uint32_t cycle = 0; cycle < 6; ++cycle) {
        const CanyonShoulderEvent left = stream.eventAtIndex(cycle * 2u);
        const CanyonShoulderEvent right = stream.eventAtIndex(cycle * 2u + 1u);
        const float recovery = (right.centerWorldS - right.halfLength) -
                               (left.centerWorldS + left.halfLength);
        const float availableTravel = kMaxLateralVelocity * recovery / kWorstTerrainSpeed;
        const float requiredShift = (left.amplitude + right.amplitude) * 0.5f;
        metrics.minimumReachabilityMargin =
            std::min(metrics.minimumReachabilityMargin, availableTravel - requiredShift);
    }
    return metrics;
}

bool validateStreamMetrics(const StreamMetrics& metrics, const StreamMetrics& repeat,
                           const StreamMetrics& differentSeed)
{
    bool valid = true;
    valid &= check(metrics.hash == repeat.hash, "A4 same-seed stream hash is not deterministic");
    valid &= check(metrics.hash != differentSeed.hash, "A4 different seed did not change the stream");
    valid &= check(metrics.routeHash == repeat.routeHash, "A2 same-seed route hash is not deterministic");
    valid &= check(metrics.routeHash != differentSeed.routeHash, "A2 different seed did not change the route");
    valid &= check(metrics.hash == 17445385684075537385ull, "A4 production stream no longer matches reviewed event output");
    valid &= check(metrics.recycleCount > 150u, "A4 did not exercise enough slice recycling");
    valid &= check(metrics.maximumWindowEventCount <= kExplicitCanyonEventCapacity &&
                       metrics.windowOverflowCount == 0u,
                   "A4 fixed event window overflowed");
    valid &= check(metrics.minimumTotalWidth > 1.44f, "A4 generated an impassable canyon width");
    valid &= check(metrics.maximumCachedError < 0.000001f, "A4 cached boundary changed when recomputed");
    valid &= check(metrics.maximumAdjacentBoundaryDelta < 0.40f, "A4 adjacent boundary change exceeded limit");
    valid &= check(metrics.minimumReachabilityMargin > 0.0f, "A4 S-gate failed reachability margin");
    valid &= check(metrics.maximumRouteSpacingError < 0.025f, "A2 streamed arc-length spacing drifted");
    valid &= check(metrics.maximumRouteFrameError < 0.001f, "A2 streamed tangent frame lost unit length");
    valid &= check(metrics.maximumRouteCurvatureWidthProduct < 0.95f,
                   "A2 streamed route curvature can fold the plateau");
    valid &= check(metrics.minimumOuterRailForwardDot > 0.0f, "A2/A3 streamed outer rail folded backward");
    return valid;
}

void printMetrics(const StreamMetrics& metrics, const StreamMetrics& repeat,
                  const StreamMetrics& differentSeed)
{
    std::cout << "explicit_slice_size=" << sizeof(ExplicitCanyonSlice) << '\n';
    std::cout << "explicit_stream_size=" << sizeof(ExplicitCanyonStream) << '\n';
    std::cout << "event_window_size=" << sizeof(CanyonEventWindow) << '\n';
    std::cout << "recycle_count=" << metrics.recycleCount << '\n';
    std::cout << "maximum_window_event_count=" << metrics.maximumWindowEventCount << '\n';
    std::cout << "window_overflow_count=" << metrics.windowOverflowCount << '\n';
    std::cout << "minimum_total_width=" << metrics.minimumTotalWidth << '\n';
    std::cout << "maximum_cached_error=" << metrics.maximumCachedError << '\n';
    std::cout << "maximum_adjacent_boundary_delta=" << metrics.maximumAdjacentBoundaryDelta << '\n';
    std::cout << "minimum_reachability_margin=" << metrics.minimumReachabilityMargin << '\n';
    std::cout << "maximum_streamed_route_spacing_error=" << metrics.maximumRouteSpacingError << '\n';
    std::cout << "maximum_streamed_route_frame_error=" << metrics.maximumRouteFrameError << '\n';
    std::cout << "maximum_streamed_curvature_width_product=" << metrics.maximumRouteCurvatureWidthProduct << '\n';
    std::cout << "minimum_outer_rail_forward_dot=" << metrics.minimumOuterRailForwardDot << '\n';
    std::cout << "stream_hash=" << metrics.hash << '\n';
    std::cout << "repeat_stream_hash=" << repeat.hash << '\n';
    std::cout << "different_seed_stream_hash=" << differentSeed.hash << '\n';
    std::cout << "route_hash=" << metrics.routeHash << '\n';
    std::cout << "repeat_route_hash=" << repeat.routeHash << '\n';
    std::cout << "different_seed_route_hash=" << differentSeed.routeHash << '\n';
}

}  // namespace

int main()
{
    static_assert(VECTOR_CANYON_EXPLICIT_TERRAIN == 0,
                  "M2 must keep the production renderer on the legacy terrain path");

    bool valid = validateProfile();
    ExplicitCanyonStream stream;
    stream.reset(kSeed);
    valid &= validateRouteAndWorldPoints(stream);
    valid &= validateIndependentShoulders(stream);

    const StreamMetrics metrics = simulateStream(kSeed);
    const StreamMetrics repeat = simulateStream(kSeed);
    const StreamMetrics differentSeed = simulateStream(kSeed ^ 0x9e3779b9u);
    valid &= validateStreamMetrics(metrics, repeat, differentSeed);
    printMetrics(metrics, repeat, differentSeed);
    return valid ? 0 : 1;
}
