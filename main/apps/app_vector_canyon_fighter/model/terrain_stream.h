#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vector_canyon_fighter {

struct TerrainSlice {
    float z = 0.0f;
    float center = 0.0f;
    float halfWidth = 2.0f;
    float floor = -1.0f;
    float leftWall = 2.0f;
    float rightWall = 2.0f;
};

class TerrainStream {
public:
    static constexpr size_t kSliceCount = 8;

    void reset(uint32_t seed);
    void update(float forwardDistance);
    const std::array<TerrainSlice, kSliceCount>& slices() const { return _slices; }

private:
    TerrainSlice makeSlice(uint32_t segment) const;

    std::array<TerrainSlice, kSliceCount> _slices = {};
    uint32_t _seed = 0;
    uint32_t _firstSegment = 0;
    float _lastForwardDistance = 0.0f;
};

}  // namespace vector_canyon_fighter
