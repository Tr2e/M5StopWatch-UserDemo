#include "../main/apps/app_vector_canyon_fighter/explicit_canyon_preview_schedule.h"
#include "../main/apps/app_vector_canyon_fighter/model/explicit_canyon_stream.h"
#include "../main/apps/app_vector_canyon_fighter/vector_canyon_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

struct Metrics {
    uint64_t hash = 1469598103934665603ull;
    uint32_t recycledSlices = 0;
    uint32_t boostFrames = 0;
    uint32_t leftIntrusionFrames = 0;
    uint32_t rightIntrusionFrames = 0;
    uint32_t maximumEvents = 0;
    uint32_t maximumRecyclePerFrame = 0;
    float finalPlayerWorldS = 0.0f;
    bool valid = true;
};

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

void hashValue(uint64_t& hash, int32_t value)
{
    hash ^= static_cast<uint32_t>(value);
    hash *= 1099511628211ull;
}

void hashSlice(uint64_t& hash, const ExplicitCanyonSlice& slice)
{
    hashValue(hash, static_cast<int32_t>(slice.segmentId));
    hashValue(hash, static_cast<int32_t>(std::lround(slice.centerX * 100000.0f)));
    hashValue(hash, static_cast<int32_t>(std::lround(slice.centerZ * 100000.0f)));
    hashValue(hash, static_cast<int32_t>(std::lround(slice.leftWidth * 100000.0f)));
    hashValue(hash, static_cast<int32_t>(std::lround(slice.rightWidth * 100000.0f)));
}

Metrics simulate(uint32_t seed)
{
    constexpr uint32_t kFrameCount = 1800;
    constexpr float kFrameSeconds = 1.0f / 30.0f;
    ExplicitCanyonStream stream;
    stream.reset(seed);
    Metrics metrics{};
    float forwardDistance = 0.0f;
    auto previousSlices = stream.slices();
    uint32_t previousFirst = stream.firstSegment();

    for (uint32_t frame = 0; frame < kFrameCount; ++frame) {
        const uint32_t elapsedMs = frame * 1000u / 30u;
        const bool boosted = isExplicitCanyonPreviewBoosted(elapsedMs);
        if (boosted) ++metrics.boostFrames;
        forwardDistance += explicitCanyonPreviewSpeed(elapsedMs) * kFrameSeconds;
        stream.update(forwardDistance);

        const uint32_t recycled = stream.firstSegment() - previousFirst;
        metrics.recycledSlices += recycled;
        metrics.maximumRecyclePerFrame = std::max(metrics.maximumRecyclePerFrame, recycled);
        metrics.valid &= check(recycled <= 1, "M6 preview recycled more than one slice in a frame");
        if (recycled == 1) {
            const auto& front = stream.slices().front();
            const auto& retained = previousSlices[1];
            metrics.valid &= check(front.segmentId == retained.segmentId &&
                                       front.centerX == retained.centerX && front.centerZ == retained.centerZ &&
                                       front.leftWidth == retained.leftWidth &&
                                       front.rightWidth == retained.rightWidth,
                                   "M6 recycled cache introduced a geometry seam");
        }

        bool hasLeftIntrusion = false;
        bool hasRightIntrusion = false;
        const auto& slices = stream.slices();
        for (std::size_t index = 0; index < slices.size(); ++index) {
            const auto& slice = slices[index];
            metrics.valid &= check(slice.segmentId == stream.firstSegment() + index,
                                   "M6 slice IDs are no longer contiguous");
            metrics.valid &= check(std::abs(slice.worldS - static_cast<float>(slice.segmentId) *
                                               ExplicitCanyonStream::kSliceSpacing) < 0.0002f,
                                   "M6 recycled slice lost its arc-length coordinate");
            hasLeftIntrusion |= slice.leftWidth < kExplicitCanyonFloorHalfWidth - 0.05f;
            hasRightIntrusion |= slice.rightWidth < kExplicitCanyonFloorHalfWidth - 0.05f;
            if (index == 0) continue;
            const auto& previous = slices[index - 1];
            const CanyonWorldPoint previousOuter = stream.worldPoint(
                index - 1, static_cast<std::size_t>(CanyonProfilePoint::RightPlateauOuter));
            const CanyonWorldPoint outer = stream.worldPoint(
                index, static_cast<std::size_t>(CanyonProfilePoint::RightPlateauOuter));
            metrics.valid &= check((outer.x - previousOuter.x) * (previous.tangentX + slice.tangentX) +
                                       (outer.z - previousOuter.z) * (previous.tangentZ + slice.tangentZ) > 0.0f,
                                   "M6 outer structural rail folded backward");
        }
        if (hasLeftIntrusion) ++metrics.leftIntrusionFrames;
        if (hasRightIntrusion) ++metrics.rightIntrusionFrames;
        metrics.maximumEvents =
            std::max(metrics.maximumEvents, static_cast<uint32_t>(stream.eventWindow().count));
        metrics.valid &= check(!stream.eventWindow().overflow,
                               "M6 six-event window overflowed during the 60-second run");

        hashSlice(metrics.hash, slices.front());
        hashSlice(metrics.hash, slices.back());
        previousSlices = slices;
        previousFirst = stream.firstSegment();
    }

    metrics.finalPlayerWorldS = stream.playerWorldS();
    metrics.valid &= check(metrics.boostFrames == 450,
                           "M6 60-second schedule no longer contains three five-second boost windows");
    metrics.valid &= check(metrics.recycledSlices >= 115 && metrics.recycledSlices <= 120,
                           "M6 60-second preview covered an unexpected route distance");
    metrics.valid &= check(metrics.leftIntrusionFrames > 1500 && metrics.rightIntrusionFrames > 1500,
                           "M6 preview did not repeatedly expose both S-gate sides");
    metrics.valid &= check(metrics.maximumEvents > 0 && metrics.maximumEvents <= kExplicitCanyonEventCapacity,
                           "M6 event window count is outside its fixed capacity");
    return metrics;
}

}  // namespace

int main()
{
    const Metrics first = simulate(0xC4A71001u);
    const Metrics repeat = simulate(0xC4A71001u);
    const Metrics different = simulate(0xC4A71002u);
    bool valid = first.valid && repeat.valid && different.valid;
    valid &= check(first.hash == repeat.hash,
                   "M6 fixed seed did not reproduce the same 60-second stream");
    valid &= check(first.hash != different.hash,
                   "M6 different seeds produced the same 60-second stream");

    std::cout << "stream_hash=" << first.hash << '\n';
    std::cout << "repeat_stream_hash=" << repeat.hash << '\n';
    std::cout << "different_seed_stream_hash=" << different.hash << '\n';
    std::cout << "recycled_slices=" << first.recycledSlices << '\n';
    std::cout << "boost_frames=" << first.boostFrames << '\n';
    std::cout << "left_intrusion_frames=" << first.leftIntrusionFrames << '\n';
    std::cout << "right_intrusion_frames=" << first.rightIntrusionFrames << '\n';
    std::cout << "maximum_event_count=" << first.maximumEvents << '\n';
    std::cout << "maximum_recycle_per_frame=" << first.maximumRecyclePerFrame << '\n';
    std::cout << "final_player_world_s=" << first.finalPlayerWorldS << '\n';
    return valid ? 0 : 1;
}
