#include "terrain_stream.h"

#include <algorithm>
#include <cmath>

namespace vector_canyon_fighter {
namespace {

constexpr float kSliceSpacing = 0.78f;
constexpr float kRecycleZ = -0.56f;
constexpr float kControlSpacing = 3.4f;
constexpr int kSkeletonCount = 7;

uint32_t mixBits(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

float hashSigned(int x, int z, uint32_t seed)
{
    const uint32_t value = mixBits(static_cast<uint32_t>(x) * 0x9e3779b9u ^
                                   static_cast<uint32_t>(z) * 0x85ebca6bu ^ seed);
    return static_cast<float>(value & 0xffffu) / 32767.5f - 1.0f;
}

float smoothStep(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float valueNoise(float x, float z, uint32_t seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float tx = smoothStep(x - static_cast<float>(x0));
    const float tz = smoothStep(z - static_cast<float>(z0));
    const float a = hashSigned(x0, z0, seed);
    const float b = hashSigned(x0 + 1, z0, seed);
    const float c = hashSigned(x0, z0 + 1, seed);
    const float d = hashSigned(x0 + 1, z0 + 1, seed);
    const float top = a + (b - a) * tx;
    const float bottom = c + (d - c) * tx;
    return top + (bottom - top) * tz;
}

float controlNoise(int index, uint32_t seed)
{
    return hashSigned(index - 1, 0, seed) * 0.18f + hashSigned(index, 0, seed) * 0.64f +
           hashSigned(index + 1, 0, seed) * 0.18f;
}

float centerControl(int index, uint32_t seed)
{
    const float i = static_cast<float>(index);
    const float seedPhase = static_cast<float>(seed & 255u) * 0.013f;
    return std::sin(i * 0.43f + seedPhase) * 0.78f + std::sin(i * 0.17f - seedPhase * 0.7f) * 0.34f +
           controlNoise(index, seed ^ 0xa511e9b3u) * 0.22f;
}

float catmullRom(float p0, float p1, float p2, float p3, float t)
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float skeletonControlX(int skeleton, int index, uint32_t seed)
{
    const float center = centerControl(index, seed);
    if (skeleton == 0) return center;

    const bool left = skeleton <= 3;
    const int layer = left ? skeleton : skeleton - 3;
    constexpr float kOffsets[] = {0.0f, 2.35f, 3.70f, 4.82f};
    const float side = left ? -1.0f : 1.0f;
    constexpr float kCenterCoupling[] = {0.0f, 0.84f, 0.48f, 0.22f};
    constexpr float kDriftScale[] = {0.0f, 0.34f, 0.49f, 0.62f};
    const float independentDrift = controlNoise(index, seed ^ (0x6d2b79f5u * static_cast<uint32_t>(skeleton))) *
                                   kDriftScale[layer];
    const float slowFold = std::sin(static_cast<float>(index) * (0.31f + layer * 0.037f) + skeleton * 0.83f) *
                           (0.14f + layer * 0.07f);
    return center * kCenterCoupling[layer] + side * (kOffsets[layer] + independentDrift + slowFold);
}

float skeletonCurveX(int skeleton, float controlPosition, uint32_t seed)
{
    const int index = static_cast<int>(std::floor(controlPosition));
    const float t = controlPosition - static_cast<float>(index);
    return catmullRom(skeletonControlX(skeleton, index - 1, seed), skeletonControlX(skeleton, index, seed),
                      skeletonControlX(skeleton, index + 1, seed), skeletonControlX(skeleton, index + 2, seed), t);
}

float amplitudeControl(int skeleton, int index, uint32_t seed)
{
    if (skeleton == 0) return -1.55f;
    const int layer = skeleton <= 3 ? skeleton : skeleton - 3;
    constexpr float kBaseAmplitude[] = {0.0f, 2.58f, 2.22f, 2.84f};
    const float pulse = 0.62f + 0.38f * std::sin(static_cast<float>(index) * (0.69f + layer * 0.08f) +
                                                 static_cast<float>(skeleton) * 1.17f);
    const float variation = controlNoise(index, seed ^ (0x27d4eb2du * static_cast<uint32_t>(skeleton))) * 0.28f;
    return std::max(0.32f, kBaseAmplitude[layer] * pulse + variation);
}

float skeletonAmplitude(int skeleton, float controlPosition, uint32_t seed)
{
    if (skeleton == 0) return -1.55f;
    const int index = static_cast<int>(std::floor(controlPosition));
    const float t = controlPosition - static_cast<float>(index);
    return std::max(0.32f, catmullRom(amplitudeControl(skeleton, index - 1, seed),
                                      amplitudeControl(skeleton, index, seed),
                                      amplitudeControl(skeleton, index + 1, seed),
                                      amplitudeControl(skeleton, index + 2, seed), t));
}

struct SliceSkeletons {
    std::array<float, kSkeletonCount> x;
    std::array<float, kSkeletonCount> amplitude;
};

SliceSkeletons sampleSkeletons(float worldZ, uint32_t seed)
{
    SliceSkeletons samples{};
    const float controlPosition = worldZ / kControlSpacing;
    for (int skeleton = 0; skeleton < kSkeletonCount; ++skeleton) {
        samples.x[skeleton] = skeletonCurveX(skeleton, controlPosition, seed);
        samples.amplitude[skeleton] = skeletonAmplitude(skeleton, controlPosition, seed);
    }
    return samples;
}

float terrainHeight(float x, float worldZ, uint32_t seed, const SliceSkeletons& skeletons)
{
    float height = valueNoise(x * 0.43f, worldZ * 0.22f, seed ^ 0x68bc21ebu) * 0.17f +
                   valueNoise(x * 0.91f, worldZ * 0.47f, seed ^ 0x02e5be93u) * 0.07f;

    // Broad, continuous relief belongs to the mountain masses, not the flight
    // channel. Masking it away from the valley keeps the widened groove readable
    // while making successive ridge sections rise and fall in depth.
    const float sideDistance = std::abs(x - skeletons.x[0]);
    const float mountainMask = smoothStep((sideDistance - 1.30f) / 1.90f);
    height += mountainMask *
              (valueNoise(x * 0.24f, worldZ * 0.16f, seed ^ 0xb5297a4du) * 0.72f +
               valueNoise(x * 0.51f, worldZ * 0.34f, seed ^ 0x1b56c4e9u) * 0.28f);

    for (int skeleton = 0; skeleton < kSkeletonCount; ++skeleton) {
        const int layer = skeleton == 0 ? 0 : (skeleton <= 3 ? skeleton : skeleton - 3);
        constexpr float kRadius[] = {1.08f, 1.38f, 1.24f, 1.46f};
        constexpr float kSharpness[] = {1.82f, 1.38f, 1.54f, 1.42f};
        const float distance = std::abs(x - skeletons.x[skeleton]);
        const float effectiveDistance = skeleton == 0 ? std::max(0.0f, distance - 1.15f) : distance;
        const float envelope = std::max(0.0f, 1.0f - effectiveDistance / kRadius[layer]);
        if (envelope > 0.0f) height += skeletons.amplitude[skeleton] * std::pow(envelope, kSharpness[layer]);
    }
    return height;
}

}  // namespace

void TerrainStream::reset(uint32_t seed)
{
    _seed = seed;
    _firstSegment = 0;
    _lastForwardDistance = 0.0f;
    for (size_t i = 0; i < _slices.size(); ++i) {
        _slices[i] = makeSlice(static_cast<uint32_t>(i));
        _slices[i].z = 1.18f + static_cast<float>(i) * kSliceSpacing;
    }
}

void TerrainStream::update(float forwardDistance)
{
    const float traveled = forwardDistance - _lastForwardDistance;
    _lastForwardDistance = forwardDistance;
    for (auto& slice : _slices) slice.z -= traveled * 0.018f;

    // Keep one slice behind the camera. The renderer interpolates its crossing
    // with the next slice onto a fixed near plane, so recycling cannot make the
    // foreground jump forward by one full slice spacing.
    while (_slices.front().z < kRecycleZ) {
        for (size_t i = 1; i < _slices.size(); ++i) _slices[i - 1] = _slices[i];
        ++_firstSegment;
        _slices.back() = makeSlice(_firstSegment + static_cast<uint32_t>(_slices.size() - 1));
        _slices.back().z = _slices[_slices.size() - 2].z + kSliceSpacing;
    }
}

TerrainSlice TerrainStream::makeSlice(uint32_t segment) const
{
    TerrainSlice slice;
    slice.worldZ = static_cast<float>(segment) * kSliceSpacing;
    const SliceSkeletons skeletons = sampleSkeletons(slice.worldZ, _seed);
    slice.center = skeletons.x[0];
    slice.halfWidth = 1.42f + valueNoise(slice.worldZ * 0.19f, 0.0f, _seed ^ 0x9e3779b9u) * 0.18f;
    slice.floor = terrainHeight(slice.center, slice.worldZ, _seed, skeletons);
    slice.floorTilt = 0.0f;
    slice.floorCrown = 0.0f;

    float leftPeak = slice.floor;
    float rightPeak = slice.floor;
    for (size_t column = 0; column < slice.surfaceHeights.size(); ++column) {
        const float x = TerrainStream::columnX(column);
        const float height = terrainHeight(x, slice.worldZ, _seed, skeletons);
        slice.surfaceHeights[column] = height;
        if (x < slice.center) leftPeak = std::max(leftPeak, height);
        if (x > slice.center) rightPeak = std::max(rightPeak, height);
    }
    slice.leftWall = leftPeak;
    slice.rightWall = rightPeak;
    return slice;
}

}  // namespace vector_canyon_fighter
