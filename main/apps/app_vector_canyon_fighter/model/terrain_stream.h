#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vector_canyon_fighter {

inline constexpr std::size_t kTerrainColumnCount = 37;

struct TerrainSlice {
    float z = 0.0f;
    float worldZ = 0.0f;
    float center = 0.0f;
    float halfWidth = 2.0f;
    float floor = -1.0f;
    float floorTilt = 0.0f;
    float floorCrown = 0.0f;
    float leftWall = 2.0f;
    float rightWall = 2.0f;
    float leftRidgeOutset = 0.5f;
    float rightRidgeOutset = 0.5f;
    std::array<float, kTerrainColumnCount> surfaceHeights = {};
};

class TerrainStream {
public:
    static constexpr size_t kSliceCount = 26;
    static constexpr size_t kColumnCount = kTerrainColumnCount;
    static constexpr float kTerrainMinX = -5.6f;
    static constexpr float kTerrainMaxX = 5.6f;

    static constexpr float columnX(size_t column)
    {
        return kTerrainMinX + (kTerrainMaxX - kTerrainMinX) * static_cast<float>(column) /
                                  static_cast<float>(kColumnCount - 1);
    }

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
