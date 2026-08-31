#include "terrain_stream.h"

#include <cmath>

namespace vector_canyon_fighter {
namespace {

constexpr float kSliceSpacing = 2.15f;

}  // namespace

void TerrainStream::reset(uint32_t seed)
{
    _seed = seed;
    _firstSegment = 0;
    _lastForwardDistance = 0.0f;
    for (size_t i = 0; i < _slices.size(); ++i) {
        _slices[i] = makeSlice(static_cast<uint32_t>(i));
        _slices[i].z = 1.15f + static_cast<float>(i) * kSliceSpacing;
    }
}

void TerrainStream::update(float forwardDistance)
{
    const float traveled = forwardDistance - _lastForwardDistance;
    _lastForwardDistance = forwardDistance;
    for (auto& slice : _slices) slice.z -= traveled * 0.035f;

    while (_slices.front().z < 0.8f) {
        for (size_t i = 1; i < _slices.size(); ++i) _slices[i - 1] = _slices[i];
        ++_firstSegment;
        _slices.back() = makeSlice(_firstSegment + static_cast<uint32_t>(_slices.size() - 1));
        _slices.back().z = _slices[_slices.size() - 2].z + kSliceSpacing;
    }
}

TerrainSlice TerrainStream::makeSlice(uint32_t segment) const
{
    const float phase = static_cast<float>((segment + _seed) % 97u) * 0.19f;
    TerrainSlice slice;
    slice.center = std::sin(phase) * 0.72f + std::sin(phase * 0.37f) * 0.28f;
    slice.halfWidth = 2.15f + std::sin(phase * 0.73f) * 0.42f;
    slice.floor = -1.05f + std::sin(phase * 1.27f) * 0.46f;
    slice.leftWall = 2.05f + std::sin(phase * 0.51f) * 0.35f;
    slice.rightWall = 2.15f + std::cos(phase * 0.57f) * 0.38f;
    return slice;
}

}  // namespace vector_canyon_fighter
