#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>

namespace vector_canyon_fighter {

struct AircraftCollisionStation {
    float forwardLead = 0.0f;
    float halfWidth = 0.0f;
};

// The aircraft is a screen-space third-person model, but these stations define
// the world-space footprint it represents. The wing station is deliberately
// placed where a 0.24-unit half-span projects to the rendered ~47 px wing span.
inline constexpr std::array<AircraftCollisionStation, 3> kAircraftCollisionStations = {{
    {1.10f, 0.10f},  // tail / engine cluster
    {1.85f, 0.24f},  // main wing
    {2.45f, 0.07f},  // nose
}};

inline constexpr std::size_t kAircraftWingStationIndex = 1;
inline constexpr float kAircraftMaximumHalfWidth = 0.24f;
inline constexpr float kAircraftFloorClearance = 0.15f;
inline constexpr float kAircraftNominalScreenHalfSpan = 53.0f;
inline constexpr int kAircraftScreenCenterY = 304;
inline constexpr int kAircraftCourseCueY = 230;
inline constexpr float kAircraftNominalScreenBottomY = 346.5f;
inline constexpr float kChaseCameraPitchFollow = 0.12f;

inline constexpr int aircraftMachRingCount(float boostAmount)
{
    return boostAmount >= 0.35f ? 4 : 2;
}

inline constexpr float aircraftPlumeLength(float boostAmount)
{
    return 2.25f + 2.05f * std::clamp(boostAmount, 0.0f, 1.0f);
}

inline constexpr float kAircraftPlumeApexExtension = 0.42f;

// Stable nozzle-to-tip taper used by the exhaust cross-section rings. The
// visible middle rings shrink toward the convergence apex. Geometry remains
// static; only a coordinated luminance wave travels across the rings.
inline constexpr float aircraftExhaustRingFraction(int ringIndex, int ringCount)
{
    // The conceptual largest ring at the nozzle is intentionally omitted.
    // The visible sequence begins with the middle rings and restores the small
    // terminal ring, leaving a clean nozzle gap before converging to the tip.
    return (static_cast<float>(ringIndex) + 1.55f) /
           (static_cast<float>(ringCount) + 1.0f);
}

inline constexpr float aircraftExhaustRingRadiusScale(int ringIndex, int ringCount)
{
    return 0.92f - 0.60f * aircraftExhaustRingFraction(ringIndex, ringCount);
}

inline float aircraftExhaustRingHighlight(float cyclePhase, int ringIndex, int ringCount)
{
    constexpr float kTau = 6.28318530718f;
    const float ringPhase = static_cast<float>(ringIndex) /
                            static_cast<float>(ringCount);
    const float wave = 0.5f + 0.5f * std::cos(kTau * (cyclePhase - ringPhase));
    return 0.18f + 0.82f * wave * wave;
}

struct AircraftScreenOffset {
    float x = 0.0f;
    float y = 0.0f;
};

// Precompute pose trigonometry once per frame. The richer image-derived mesh
// projects hundreds of endpoints, so repeating sin/cos per endpoint would be
// a measurable regression on the watch.
class AircraftPoseProjector {
public:
    AircraftPoseProjector(float pitchDegrees, float rollDegrees)
    {
        constexpr float kDegreesToRadians = 0.01745329252f;
        const float pitch = pitchDegrees * kDegreesToRadians;
        const float roll = rollDegrees * kDegreesToRadians;
        _pitchCosine = std::cos(pitch);
        _pitchSine = std::sin(pitch);
        _rollCosine = std::cos(roll);
        _rollSine = std::sin(roll);

        // Match the canyon chase camera. The scenery follows 12% of aircraft
        // pitch, so the airframe and floor keep the same optical relationship
        // while the aircraft still visibly changes attitude.
        const float cameraPitch = (-3.1f + pitchDegrees * kChaseCameraPitchFollow) *
                                  kDegreesToRadians;
        _cameraCosine = std::cos(cameraPitch);
        _cameraSine = std::sin(cameraPitch);
    }

    AircraftScreenOffset project(float x, float y, float z) const
    {
        // The visual aircraft sits just ahead of and below the same near-level
        // chase camera used by the canyon. Uniform model scale is essential:
        // foreshortening now comes from the scene camera, not from an unrelated
        // top-down presentation view.
        constexpr float kModelScale = 0.060f;
        constexpr float kFocalLength = 387.0f;
        constexpr float kAnchorForward = 1.60f;
        constexpr float kAnchorVertical = -0.685f;

        // Bank is a real rotation around the longitudinal axis rather than a
        // 2D rotation of an already projected drawing.
        const float rolledX = x * _rollCosine - y * _rollSine;
        const float rolledY = x * _rollSine + y * _rollCosine;
        const float pitchedY = rolledY * _pitchCosine + z * _pitchSine;
        const float pitchedZ = z * _pitchCosine - rolledY * _pitchSine;

        const float worldY = kAnchorVertical + pitchedY * kModelScale;
        const float worldZ = kAnchorForward + pitchedZ * kModelScale;
        const float cameraY = worldY * _cameraCosine - worldZ * _cameraSine;
        const float cameraZ = worldY * _cameraSine + worldZ * _cameraCosine;
        const float anchorCameraY = kAnchorVertical * _cameraCosine -
                                    kAnchorForward * _cameraSine;
        const float anchorCameraZ = kAnchorVertical * _cameraSine +
                                    kAnchorForward * _cameraCosine;

        return {
            kFocalLength * rolledX * kModelScale / cameraZ,
            -kFocalLength * cameraY / cameraZ +
                kFocalLength * anchorCameraY / anchorCameraZ,
        };
    }

private:
    float _pitchCosine = 1.0f;
    float _pitchSine = 0.0f;
    float _rollCosine = 1.0f;
    float _rollSine = 0.0f;
    float _cameraCosine = 1.0f;
    float _cameraSine = 0.0f;
};

// Compatibility helper retained for geometry tests and one-off projections.
inline AircraftScreenOffset projectAircraftPose(float x, float y, float z,
                                                 float pitchDegrees, float rollDegrees)
{
    return AircraftPoseProjector(pitchDegrees, rollDegrees).project(x, y, z);
}

struct AircraftGroundShadow {
    int centerY = 0;
    int radiusX = 0;
    int radiusY = 0;
    float brightness = 0.0f;
};

// A compact visual altitude cue, deliberately independent of the much longer
// collision envelope. The footprint stays immediately below the aircraft and
// never expands into a screen-spanning ground polygon.
inline AircraftGroundShadow makeAircraftGroundShadow(float floorClearance)
{
    const float normalized = std::clamp(floorClearance / 1.20f, 0.0f, 1.0f);
    return {
        static_cast<int>(std::lround(kAircraftNominalScreenBottomY + 8.0f + normalized * 24.0f)),
        static_cast<int>(std::lround(16.0f - normalized * 6.0f)),
        static_cast<int>(std::lround(4.0f - normalized * 2.0f)),
        0.16f + (1.0f - normalized) * 0.12f,
    };
}

static_assert(kAircraftCollisionStations[kAircraftWingStationIndex].halfWidth ==
                  kAircraftMaximumHalfWidth,
              "The wing station must own the maximum collision span");
static_assert(aircraftMachRingCount(0.0f) == 2,
              "Cruise exhaust must retain one middle and one terminal ring");
static_assert(aircraftMachRingCount(1.0f) == 4,
              "Boost exhaust must retain three middle and one terminal ring");
static_assert(aircraftExhaustRingFraction(0, 4) > 0.0f &&
                  aircraftExhaustRingFraction(3, 4) < 1.0f,
              "Visible exhaust rings must omit only the nozzle endpoint ring");
static_assert(aircraftExhaustRingRadiusScale(0, 4) >
                  aircraftExhaustRingRadiusScale(3, 4),
              "Exhaust section rings must shrink away from the nozzle");
static_assert(aircraftPlumeLength(0.0f) >= 2.20f,
              "Cruise exhaust plume became too short to read");
static_assert(aircraftPlumeLength(1.0f) >= 4.20f,
              "Boost exhaust plume must visibly extend beyond cruise");
static_assert(kAircraftPlumeApexExtension >= 0.40f,
              "Exhaust axis needs a readable exposed segment after the terminal ring");

}  // namespace vector_canyon_fighter
