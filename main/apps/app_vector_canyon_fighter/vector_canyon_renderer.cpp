#include "vector_canyon_renderer.h"

#include "explicit_canyon_projection.h"
#include "hud_layout.h"
#include "hud_semantics.h"
#include "vector_canyon_config.h"

#include <hal/hal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace vector_canyon_fighter {
namespace {

// The projection datum places the reviewed silhouette around y=257..345, with
// its belly adjacent to the collision-aligned y=346.5 floor threshold.
constexpr int kShipCenterY = kAircraftScreenCenterY;

const char* axisSourceLabel(FlightAxisSource source)
{
    switch (source) {
        case FlightAxisSource::Imu: return "IMU";
        case FlightAxisSource::Joystick2: return "JOY2";
        default: return "NONE";
    }
}

struct Vec3 {
    float x;
    float y;
    float z;
};

struct FuselageStation {
    float z;
    float halfWidth;
    float topY;
    float sideY;
    float bottomY;
};

// Original variable-geometry space-fighter silhouette. Width changes are
// staged around a cockpit shoulder rather than forming one uninterrupted
// needle, so the fuselage reads as a vehicle instead of a center decoration.
constexpr std::array<FuselageStation, 10> kFuselageStations = {{
    { 6.50f, 0.04f,0.06f, 0.00f,-0.05f},
    { 5.60f,0.15f,0.14f, 0.01f,-0.10f},
    { 4.65f,0.30f,0.25f, 0.02f,-0.16f},
    { 3.65f,0.48f,0.42f, 0.02f,-0.23f},
    { 2.65f,0.64f,0.73f, 0.00f,-0.31f},
    { 1.55f,0.74f,0.90f,-0.03f,-0.39f},
    { 0.45f,0.80f,0.70f,-0.06f,-0.44f},
    {-0.65f,0.74f,0.53f,-0.08f,-0.43f},
    {-1.75f,0.60f,0.37f,-0.09f,-0.37f},
    {kAircraftFuselageTailZ,0.38f,0.22f,-0.08f,-0.25f},
}};

struct EngineSpec {
    float x;
    float y;
    float radius;
};

constexpr std::array<EngineSpec, 4> kEngineSpecs = {{
    {-2.42f,-0.27f,kAircraftMinimumEngineRadius},
    {-1.12f,-0.50f,0.32f},
    { 1.12f,-0.50f,0.32f},
    { 2.42f,-0.27f,kAircraftMinimumEngineRadius},
}};

constexpr std::array<std::array<float, 2>, 6> kEngineRadials = {{
    {{ 0.00f,  1.00f}}, {{ 0.87f,  0.50f}}, {{ 0.87f, -0.50f}},
    {{ 0.00f, -1.00f}}, {{-0.87f, -0.50f}}, {{-0.87f,  0.50f}},
}};

Vec3 fuselagePoint(const FuselageStation& station, int lane)
{
    switch (lane & 3) {
        case 0: return {0.0f, station.topY, station.z};
        case 1: return {station.halfWidth, station.sideY, station.z};
        case 2: return {0.0f, station.bottomY, station.z};
        default: return {-station.halfWidth, station.sideY, station.z};
    }
}

Vec3 wingPoint(float side, const AircraftWingStation& station, float chord)
{
    return {
        side * station.span,
        station.upperY - chord * 0.10f,
        station.leadingZ + (station.trailingZ - station.leadingZ) * chord,
    };
}

Vec3 enginePoint(const EngineSpec& engine, std::size_t ring, std::size_t radial)
{
    return {
        engine.x + kEngineRadials[radial][0] * engine.radius,
        engine.y + kEngineRadials[radial][1] * engine.radius,
        kAircraftEngineRingZ[ring],
    };
}

uint16_t scaleRgb565(uint16_t color, float weight)
{
    const uint16_t safeWeight = static_cast<uint16_t>(std::lround(std::clamp(weight, 0.0f, 1.0f) * 256.0f));
    const uint16_t red = static_cast<uint16_t>(((color >> 11) & 0x1fu) * safeWeight >> 8);
    const uint16_t green = static_cast<uint16_t>(((color >> 5) & 0x3fu) * safeWeight >> 8);
    const uint16_t blue = static_cast<uint16_t>((color & 0x1fu) * safeWeight >> 8);
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

uint16_t blendRgb565(uint16_t from, uint16_t to, float amount)
{
    const float safeAmount = std::clamp(amount, 0.0f, 1.0f);
    const float inverse = 1.0f - safeAmount;
    const uint16_t red = static_cast<uint16_t>(std::lround(
        static_cast<float>((from >> 11) & 0x1fu) * inverse +
        static_cast<float>((to >> 11) & 0x1fu) * safeAmount));
    const uint16_t green = static_cast<uint16_t>(std::lround(
        static_cast<float>((from >> 5) & 0x3fu) * inverse +
        static_cast<float>((to >> 5) & 0x3fu) * safeAmount));
    const uint16_t blue = static_cast<uint16_t>(std::lround(
        static_cast<float>(from & 0x1fu) * inverse +
        static_cast<float>(to & 0x1fu) * safeAmount));
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

const char* warningLabel(CollisionHazard hazard)
{
    switch (hazard) {
        case CollisionHazard::LeftWall: return "PROX LEFT";
        case CollisionHazard::RightWall: return "PROX RIGHT";
        case CollisionHazard::Floor: return "PULL UP";
        case CollisionHazard::None: break;
    }
    return "PROX";
}

const char* impactLabel(CollisionHazard hazard)
{
    switch (hazard) {
        case CollisionHazard::LeftWall: return "IMPACT LEFT";
        case CollisionHazard::RightWall: return "IMPACT RIGHT";
        case CollisionHazard::Floor: return "IMPACT FLOOR";
        case CollisionHazard::None: break;
    }
    return "IMPACT";
}

}  // namespace

void Renderer::open(int width, int height)
{
    _width = width;
    _height = height;
}

void Renderer::close()
{
    _width = 0;
    _height = 0;
}

bool Renderer::drawExplicitTerrain(const CanyonCamera& camera, const ExplicitCanyonStream& terrain,
                                   uint16_t terrainPrimary, uint16_t terrainMid, uint16_t terrainSecondary)
{
    constexpr int16_t kInvisible = std::numeric_limits<int16_t>::min();
    constexpr int kMinimumCoordinate = std::numeric_limits<int16_t>::min() + 1;
    constexpr int kMaximumCoordinate = std::numeric_limits<int16_t>::max();
    auto& canvas = GetHAL().getDisplay();
    const auto& slices = terrain.slices();
    const auto cacheIndex = [](std::size_t slice, std::size_t profilePoint) {
        return slice * ExplicitCanyonStream::kProfileCount + profilePoint;
    };
    const auto toPackedPoint = [&](CanyonScreenPoint screen) {
        const int x = std::clamp(static_cast<int>(std::lround(screen.x)), kMinimumCoordinate, kMaximumCoordinate);
        const int y = std::clamp(static_cast<int>(std::lround(screen.y)), kMinimumCoordinate, kMaximumCoordinate);
        return ProjectedCanyonPoint{static_cast<int16_t>(x), static_cast<int16_t>(y)};
    };
    const auto segmentOutsideViewport = [&](const ProjectedCanyonPoint& from,
                                            const ProjectedCanyonPoint& to) {
        return (from.x < 0 && to.x < 0) ||
               (from.x >= _width && to.x >= _width) ||
               (from.y < 0 && to.y < 0) ||
               (from.y >= _height && to.y >= _height);
    };

    for (std::size_t slice = 0; slice < ExplicitCanyonStream::kSliceCount; ++slice) {
        for (std::size_t profilePoint = 0; profilePoint < ExplicitCanyonStream::kProfileCount; ++profilePoint) {
            const CanyonCameraPoint cameraPoint =
                explicitCanyonToCamera(camera, terrain.worldPoint(slice, profilePoint));
            CanyonScreenPoint screen{};
            ProjectedCanyonPoint& projected = _explicitTerrainPoints[cacheIndex(slice, profilePoint)];
            if (projectExplicitCanyonPoint(camera, cameraPoint, screen)) {
                projected = toPackedPoint(screen);
            } else {
                projected = {kInvisible, kInvisible};
            }
        }
    }

    bool drewAny = false;
    const auto drawSegment = [&](std::size_t fromSlice, std::size_t fromProfile,
                                 std::size_t toSlice, std::size_t toProfile, uint16_t color) {
        const ProjectedCanyonPoint& fromProjected = _explicitTerrainPoints[cacheIndex(fromSlice, fromProfile)];
        const ProjectedCanyonPoint& toProjected = _explicitTerrainPoints[cacheIndex(toSlice, toProfile)];
        const bool fromVisible = fromProjected.x != kInvisible;
        const bool toVisible = toProjected.x != kInvisible;
        if (fromVisible && toVisible) {
            if (segmentOutsideViewport(fromProjected, toProjected)) return;
            canvas.drawLine(fromProjected.x, fromProjected.y, toProjected.x, toProjected.y, color);
            drewAny = true;
            return;
        }
        if (!fromVisible && !toVisible) return;

        CanyonCameraPoint fromCamera =
            explicitCanyonToCamera(camera, terrain.worldPoint(fromSlice, fromProfile));
        CanyonCameraPoint toCamera =
            explicitCanyonToCamera(camera, terrain.worldPoint(toSlice, toProfile));
        if (!clipExplicitCanyonSegmentToNear(fromCamera, toCamera)) return;
        CanyonScreenPoint fromScreen{};
        CanyonScreenPoint toScreen{};
        if (!projectExplicitCanyonPoint(camera, fromCamera, fromScreen) ||
            !projectExplicitCanyonPoint(camera, toCamera, toScreen)) {
            return;
        }
        const ProjectedCanyonPoint clippedFrom = toPackedPoint(fromScreen);
        const ProjectedCanyonPoint clippedTo = toPackedPoint(toScreen);
        if (segmentOutsideViewport(clippedFrom, clippedTo)) return;
        canvas.drawLine(clippedFrom.x, clippedFrom.y, clippedTo.x, clippedTo.y, color);
        drewAny = true;
    };
    const auto depthColor = [&](float relativeDepth) {
        if (relativeDepth < 9.0f) return terrainPrimary;
        if (relativeDepth < 20.0f) return terrainMid;
        return terrainSecondary;
    };

    // Complete local N/Up ribs establish the flat floor, steep cliff faces,
    // and flat plateaus. Draw far to near so the near geometry remains legible.
    for (std::size_t reverse = ExplicitCanyonStream::kSliceCount; reverse > 0; --reverse) {
        const std::size_t slice = reverse - 1;
        const float relativeDepth = std::max(0.0f, slices[slice].worldS - terrain.playerWorldS());
        const uint16_t color = depthColor(relativeDepth);
        for (std::size_t profilePoint = 1; profilePoint < ExplicitCanyonStream::kProfileCount; ++profilePoint) {
            drawSegment(slice, profilePoint - 1, slice, profilePoint, color);
        }
    }

    // Semantic profile indices form longitudinal rails. Structural rails never
    // disappear; mid and fine rails fade continuously with world depth.
    for (std::size_t profilePoint = 0; profilePoint < ExplicitCanyonStream::kProfileCount; ++profilePoint) {
        for (std::size_t slice = 1; slice < ExplicitCanyonStream::kSliceCount; ++slice) {
            const float midpointWorldS = (slices[slice - 1].worldS + slices[slice].worldS) * 0.5f;
            const float relativeDepth = std::max(0.0f, midpointWorldS - terrain.playerWorldS());
            const float weight = explicitCanyonRailLodWeight(profilePoint, relativeDepth);
            if (weight <= 0.01f) continue;
            const uint16_t baseColor = isExplicitCanyonStructuralRail(profilePoint)
                                           ? terrainPrimary
                                           : depthColor(relativeDepth);
            drawSegment(slice - 1, profilePoint, slice, profilePoint, scaleRgb565(baseColor, weight));
        }
    }

    // Sparse deterministic diagonals make the two explicit cliff faces read as
    // low-poly surfaces. They share the fine LOD fade and never replace rails.
    constexpr std::array<std::size_t, 4> kFaceStarts = {3, 4, 18, 19};
    for (std::size_t slice = 0; slice + 1 < ExplicitCanyonStream::kSliceCount; slice += 2) {
        const float relativeDepth = std::max(0.0f, slices[slice].worldS - terrain.playerWorldS());
        const float weight = 1.0f - explicitCanyonSmoothUnit((relativeDepth - 9.0f) / 5.0f);
        if (weight <= 0.01f) continue;
        for (const std::size_t faceStart : kFaceStarts) {
            drawSegment(slice, faceStart, slice + 1, faceStart + 1,
                        scaleRgb565(depthColor(relativeDepth), weight));
        }
    }
    return drewAny;
}

void Renderer::renderExplicitPreview(const FlightState& flight, const ExplicitCanyonStream& terrain)
{
    if (_width <= 0 || _height <= 0) return;

    auto& display = GetHAL().getDisplay();
    const uint16_t terrainPrimary = display.color565(36, 127, 145);
    const uint16_t terrainMid = display.color565(21, 76, 91);
    const uint16_t terrainSecondary = display.color565(9, 40, 50);
    const CanyonRouteFrame route = terrain.routeFrameAt(terrain.playerWorldS());
#if VECTOR_CANYON_EXPLICIT_TOP_DEBUG
    const CanyonCamera camera = makeExplicitCanyonTopDebugCamera(route, _width, _height);
#else
    const CanyonCamera camera =
        makeExplicitCanyonCamera(route, flight.altitude, flight.pitch, _width, _height);
#endif
    display.startWrite();
    display.fillScreen(TFT_BLACK);
    drawExplicitTerrain(camera, terrain, terrainPrimary, terrainMid, terrainSecondary);
    display.endWrite();
}

void Renderer::render(const FlightState& flight, const ExplicitCanyonStream& terrain,
                      const CollisionStatus& collision, float calibrationProgress,
                      const InputStatus& inputStatus, bool aircraftVisible)
{
    renderGame(flight, terrain, collision, calibrationProgress, inputStatus,
               aircraftVisible);
}

void Renderer::renderGame(const FlightState& flight, const ExplicitCanyonStream& terrain,
                          const CollisionStatus& collision, float calibrationProgress,
                          const InputStatus& inputStatus, bool aircraftVisible)
{
    if (_width <= 0 || _height <= 0) return;

    auto& display = GetHAL().getDisplay();
    auto& canvas = display;
    const int centerX = _width / 2;
    // M9 palette: the canyon is a low-luminance blue-cyan world layer;
    // phosphor green is reserved for flight symbology, and warm hues only
    // appear when the vehicle state requires attention.
    const uint16_t terrainPrimary = display.color565(36, 127, 145);
    const uint16_t terrainMid = display.color565(21, 76, 91);
    const uint16_t terrainSecondary = display.color565(9, 40, 50);
    const uint16_t shipColor = display.color565(68, 124, 139);
    const uint16_t shipDim = display.color565(20, 49, 58);
    const uint16_t structureColor = display.color565(43, 86, 99);
    const uint16_t engineColor = display.color565(48, 92, 104);
    const uint16_t canopyColor = display.color565(42, 105, 121);
    const uint16_t exhaust = display.color565(126, 58, 18);
    const uint16_t engineCoreHalo = display.color565(126, 76, 18);
    const uint16_t engineCoreGlow = display.color565(242, 174, 44);
    const uint16_t engineCore = display.color565(255, 244, 176);
    const uint16_t strobeGlow = display.color565(132, 210, 255);
    const uint16_t strobeCore = display.color565(232, 250, 255);
    const uint16_t hudColor = display.color565(114, 230, 162);
    const uint16_t hudMid = display.color565(67, 153, 108);
    const uint16_t hudDim = display.color565(39, 94, 69);
    const uint16_t hudAccent = display.color565(182, 255, 208);
    const uint16_t caution = display.color565(255, 179, 71);
    const uint16_t impact = display.color565(255, 88, 72);

    // The StopWatch display already owns a PSRAM framebuffer. Drawing into a
    // second full-screen sprite and pushing it duplicated the entire 468x466
    // memory copy before every panel transfer. Hold one outer transaction so
    // nested drawing calls only update the framebuffer; endWrite submits the
    // completed frame once.
    display.startWrite();

    // Calibration overlay: shown before game starts / after restart
    if (calibrationProgress >= 0.0f && calibrationProgress < 1.0f) {
        canvas.fillScreen(TFT_BLACK);
        const int cx = _width / 2;
        const int cy = _height / 2;
        const bool inputOffline =
            inputStatus.readiness == InputReadiness::Disconnected ||
            inputStatus.readiness == InputReadiness::Fault;
        const auto drawCentered = [&](const char* text, int y) {
            canvas.setCursor(cx - canvas.textWidth(text) / 2, y);
            canvas.print(text);
        };

        // A compact centered instrument stack. Text widths come from the
        // active display font instead of hand-tuned character offsets.
        canvas.setTextSize(1);
        canvas.setTextColor(hudDim, TFT_BLACK);
        char sourceTitle[28]{};
        std::snprintf(sourceTitle, sizeof(sourceTitle), "FLIGHT CONTROL / %s",
                      axisSourceLabel(inputStatus.axisSource));
        drawCentered(sourceTitle, cy - 86);

        canvas.setTextSize(2);
        canvas.setTextColor(inputOffline ? caution : hudAccent, TFT_BLACK);
        drawCentered(inputOffline ? "INPUT OFFLINE" : "CALIBRATING", cy - 61);
        canvas.drawLine(cx - 102, cy - 43, cx - 76, cy - 43, hudDim);
        canvas.drawLine(cx + 76, cy - 43, cx + 102, cy - 43, hudDim);

        canvas.setTextSize(1);
        canvas.setTextColor(inputOffline ? caution : hudColor, TFT_BLACK);
        if (inputOffline && inputStatus.axisSource == FlightAxisSource::Joystick2) {
            drawCentered("CHECK PORT.A / ADDR 0x63", cy - 25);
        } else {
            drawCentered(inputOffline ? "SENSOR NOT READY" : "HOLD DEVICE LEVEL",
                         cy - 25);
        }

        constexpr int kBarW = 160;
        constexpr int kBarH = 7;
        const int barX = cx - kBarW / 2;
        const int barY = cy + 2;
        canvas.drawRect(barX - 1, barY - 1, kBarW + 2, kBarH + 2, hudDim);
        const int filled = inputOffline
                               ? 0
                               : std::clamp(
                                     static_cast<int>(calibrationProgress * kBarW),
                                     0, kBarW);
        canvas.fillRect(barX, barY, filled, kBarH, hudColor);
        for (int division = 1; division < 4; ++division) {
            const int tickX = barX + division * kBarW / 4;
            canvas.drawLine(tickX, barY - 4, tickX, barY - 2, hudDim);
            canvas.drawLine(tickX, barY + kBarH + 2, tickX, barY + kBarH + 4, hudDim);
        }

        canvas.setTextColor(hudDim, TFT_BLACK);
        if (inputOffline) {
            drawCentered("WAITING FOR VALID SIGNAL", cy + 24);
        } else {
            const float remaining = (1.0f - calibrationProgress) * 2.5f;
            char countdown[16]{};
            std::snprintf(countdown, sizeof(countdown), "T- %.1f SEC", remaining);
            drawCentered(countdown, cy + 24);
        }
        canvas.setTextColor(hudColor, TFT_BLACK);
        drawCentered(inputOffline ? "K1+K2 / EXIT" : "KEEP STILL", cy + 50);
        display.endWrite();
        return;
    }

    canvas.fillScreen(TFT_BLACK);

    const CanyonRouteFrame route = terrain.routeFrameAt(terrain.playerWorldS());
    const CanyonCamera camera = aircraftVisible
        ? makeExplicitCanyonChaseCamera(
              route, flight.lateralOffset, flight.altitude, flight.pitch,
              _width, _height)
        : makeExplicitCanyonCockpitCamera(
              route, flight.lateralOffset, flight.altitude, flight.pitch,
              flight.roll, flight.turnYaw, _width, _height);
    CanyonScreenPoint routeCue{
        static_cast<float>(centerX), camera.principalY,
    };
    const bool terrainVisible = drawExplicitTerrain(
        camera, terrain, terrainPrimary, terrainMid, terrainSecondary);
    CanyonScreenPoint farCenter{};
    if (projectExplicitCanyonPoint(
            camera,
            explicitCanyonToCamera(camera, terrain.worldPoint(
                ExplicitCanyonStream::kSliceCount - 1,
                static_cast<std::size_t>(CanyonProfilePoint::FloorCenter))),
            farCenter)) {
        routeCue = farCenter;
    }
    if (!terrainVisible) {
        display.endWrite();
        return;
    }

    // The chase camera follows the aircraft laterally, so the aircraft stays at
    // the screen datum while the canyon moves around it. This is also the same
    // camera used by the collision-aligned ground shadow below.
    const int shipX = centerX;
    const int shipY = kShipCenterY;
    const AircraftPoseProjector poseProjector(
        flight.pitch, flight.roll, flight.turnYaw);
    const auto projectShip = [&](const Vec3& point) {
        const AircraftScreenOffset offset = poseProjector.project(point.x, point.y, point.z);
        return std::array<int, 2>{
            shipX + static_cast<int>(offset.x),
            shipY + static_cast<int>(offset.y),
        };
    };
    const auto drawShipLine = [&](const Vec3& fromPoint, const Vec3& toPoint,
                                  uint16_t color) {
        if (!aircraftVisible) return;
        const auto from = projectShip(fromPoint);
        const auto to = projectShip(toPoint);
        canvas.drawLine(from[0], from[1], to[0], to[1], color);
    };

    // Keep the ground cue deliberately small. A collision envelope spans the
    // camera depth and becomes a giant trapezoid under perspective; it is not
    // an appropriate shadow shape. This flattened footprint only communicates
    // aircraft position and AGL, leaving the canyon grid exposed.
    const AircraftGroundShadow shadow = makeAircraftGroundShadow(collision.floorClearance);
    const uint16_t shadowFill = scaleRgb565(terrainPrimary, shadow.brightness);
    const uint16_t shadowEdge = scaleRgb565(terrainPrimary, shadow.brightness + 0.10f);
    if (aircraftVisible) {
        canvas.fillEllipse(shipX, shadow.centerY, shadow.radiusX, shadow.radiusY, shadowFill);
        canvas.drawEllipse(shipX, shadow.centerY, shadow.radiusX, shadow.radiusY, shadowEdge);
    }

    // Exhaust has two distinct states: cruise uses a short amber axis and one
    // steady mid-plume ring, while boost expands into three animated rings. A
    // permanent steady core is drawn inside every nozzle later, after the
    // nacelle outlines, so engine activity never depends on ring animation.
    const uint32_t frameMillis = GetHAL().millis();
    const int machRingCount = aircraftMachRingCount(flight.boostAmount);
    const float plumeLength = aircraftPlumeLength(flight.boostAmount);
    const float exhaustApexZ = kAircraftEngineRingZ.back() - plumeLength -
                               kAircraftPlumeApexExtension;
    constexpr uint32_t highlightPeriodMs = 520u;
    const float highlightPhase =
        static_cast<float>(frameMillis % highlightPeriodMs) /
        static_cast<float>(highlightPeriodMs);
    const uint16_t machBase = display.color565(196, 128, 24);
    const uint16_t machPeak = display.color565(255, 255, 176);
    const uint16_t cruiseMach = display.color565(224, 166, 52);
    const bool boostRings = flight.boostAmount >= 0.35f;
    for (const EngineSpec& engine : kEngineSpecs) {
        const Vec3 plumeTip{engine.x, engine.y, exhaustApexZ};
        drawShipLine({engine.x, engine.y, kAircraftEngineRingZ.back()}, plumeTip, exhaust);

        for (int ring = 0; ring < machRingCount; ++ring) {
            const float fraction = boostRings
                ? aircraftExhaustRingFraction(ring, machRingCount)
                : kAircraftCruiseRingFraction;
            const float ringZ = kAircraftEngineRingZ.back() - fraction * plumeLength;
            const float radius = engine.radius * (boostRings
                ? aircraftExhaustRingRadiusScale(ring, machRingCount)
                : kAircraftCruiseRingRadiusScale);
            const uint16_t machColor = boostRings
                ? blendRgb565(
                      machBase, machPeak,
                      aircraftExhaustRingHighlight(
                          highlightPhase, ring, machRingCount))
                : cruiseMach;
            std::array<Vec3, 8> ringPoints{};
            for (std::size_t point = 0; point < ringPoints.size(); ++point) {
                constexpr float kTau = 6.28318530718f;
                const float angle = kTau * static_cast<float>(point) /
                                    static_cast<float>(ringPoints.size());
                ringPoints[point] = {
                    engine.x + radius * std::cos(angle),
                    engine.y + radius * std::sin(angle),
                    ringZ,
                };
            }
            for (std::size_t edge = 0; edge < ringPoints.size(); ++edge) {
                drawShipLine(ringPoints[edge],
                             ringPoints[(edge + 1) % ringPoints.size()], machColor);
            }
        }
    }

    // Draw only the upper transverse facets. Hidden lower ribs previously
    // collapsed onto the same watch pixels and obscured the fuselage volume.
    for (std::size_t index = 0; index < kFuselageStations.size(); ++index) {
        if (index != 0 && index + 1 != kFuselageStations.size() && index % 2 == 0) continue;
        const FuselageStation& station = kFuselageStations[index];
        drawShipLine(fuselagePoint(station, 3), fuselagePoint(station, 0), shipDim);
        drawShipLine(fuselagePoint(station, 0), fuselagePoint(station, 1), shipDim);
    }
    for (std::size_t station = 0; station + 1 < kFuselageStations.size(); ++station) {
        for (int lane = 0; lane < 4; ++lane) {
            const bool silhouette = lane != 2;
            drawShipLine(fuselagePoint(kFuselageStations[station], lane),
                         fuselagePoint(kFuselageStations[station + 1], lane),
                         silhouette ? shipColor : shipDim);
        }
    }

    // Broad swept wings use a real chord/span grid rather than decorative
    // diagonals. This is the defining construction visible in the approved icon.
    constexpr std::array<float, 3> kWingChords = {{0.0f, 0.50f, 1.0f}};
    for (float side : {-1.0f, 1.0f}) {
        for (std::size_t station = 0; station < kAircraftWingStations.size(); ++station) {
            for (std::size_t chord = 0; chord + 1 < kWingChords.size(); ++chord) {
                const bool outline = station == 0 ||
                                     station + 1 == kAircraftWingStations.size();
                drawShipLine(wingPoint(side, kAircraftWingStations[station], kWingChords[chord]),
                             wingPoint(side, kAircraftWingStations[station], kWingChords[chord + 1]),
                             outline ? shipColor : shipDim);
            }
        }
        for (std::size_t chord = 0; chord < kWingChords.size(); ++chord) {
            for (std::size_t station = 0;
                 station + 1 < kAircraftWingStations.size(); ++station) {
                const bool outline = chord == 0 || chord + 1 == kWingChords.size();
                drawShipLine(wingPoint(side, kAircraftWingStations[station], kWingChords[chord]),
                             wingPoint(side, kAircraftWingStations[station + 1], kWingChords[chord]),
                             outline ? shipColor : shipDim);
            }
        }

    }

    // Shoulder intake boxes bridge fuselage, wing and inner nacelles. Their
    // front-mouth diagonals give the craft a mechanical torso instead of a
    // featureless flat delta.
    for (float side : {-1.0f, 1.0f}) {
        const std::array<Vec3, 4> intakeTop = {{
            {side * 0.52f,0.40f,2.62f},
            {side * 1.14f,0.30f,2.18f},
            {side * 1.20f,0.20f,0.34f},
            {side * 0.66f,0.38f,0.52f},
        }};
        // The camera trails the aircraft, so the forward-facing mouth and its
        // lower rim are occluded. Only the top shell, outer wall and rear lip
        // are legal visible edges from this viewpoint.
        drawShipLine(intakeTop[1], intakeTop[2], structureColor);
        drawShipLine(intakeTop[2], intakeTop[3], structureColor);
        drawShipLine(intakeTop[3], intakeTop[0], structureColor);
    }

    // Twin canted tail fins add a second silhouette tier behind the cockpit
    // and provide the vertical-volume cue missing from the rejected flat mesh.
    for (float side : {-1.0f, 1.0f}) {
        const std::array<Vec3, 4> fin = {{
            {side * 0.72f,0.46f,-0.92f},
            {side * 0.88f,0.30f,-2.70f},
            {side * 1.38f,1.05f,-2.18f},
            {side * 1.20f,1.24f,-1.24f},
        }};
        for (std::size_t edge = 0; edge < fin.size(); ++edge) {
            drawShipLine(fin[edge], fin[(edge + 1) % fin.size()], shipColor);
        }
        drawShipLine(fin[0], fin[2], shipDim);

        // Compact wingtip sensor/weapon booms echo the four-pod interceptor
        // language without crossing the HUD safe area.
        drawShipLine({side * 3.64f,0.16f,-0.72f},
                     {side * 3.64f,0.12f,0.82f}, structureColor);
        drawShipLine({side * 3.54f,0.12f,0.64f},
                     {side * 3.74f,0.12f,0.64f}, structureColor);
    }

    // Visibility-filtered nacelles: the rear lip is complete, but forward rings
    // and rails only show their camera-facing upper half. This is enough to read
    // as a cylinder without creating four dense luminous columns.
    for (const EngineSpec& engine : kEngineSpecs) {
        for (std::size_t ring = 0; ring < kAircraftEngineRingZ.size(); ++ring) {
            const bool rearRing = ring + 1 == kAircraftEngineRingZ.size();
            for (std::size_t radial = 0; radial < kEngineRadials.size(); ++radial) {
                if (!rearRing && radial >= 2 && radial <= 3) continue;
                drawShipLine(enginePoint(engine, ring, radial),
                             enginePoint(engine, ring, (radial + 1) % kEngineRadials.size()),
                             rearRing ? engineColor : shipDim);
            }
        }
        for (std::size_t ring = 0; ring + 1 < kAircraftEngineRingZ.size(); ++ring) {
            constexpr std::array<std::size_t, 3> kVisibleRails = {{0, 1, 5}};
            for (const std::size_t radial : kVisibleRails) {
                drawShipLine(enginePoint(engine, ring, radial),
                             enginePoint(engine, ring + 1, radial),
                             engineColor);
            }
        }
        const float pylonY = engine.x < -1.7f || engine.x > 1.7f ? 0.16f : 0.20f;
        drawShipLine({engine.x, engine.y + engine.radius, kAircraftEngineRingZ.front()},
                     {engine.x - engine.radius * 0.48f, pylonY, 0.18f}, structureColor);
        drawShipLine({engine.x, engine.y + engine.radius, kAircraftEngineRingZ.front()},
                     {engine.x + engine.radius * 0.48f, pylonY, 0.18f}, structureColor);
    }

    // One restrained, steady core per nozzle. A dim five-pixel halo surrounds
    // the crisp three-pixel center, increasing presence without becoming a
    // solid disc. Both remain inside the smallest projected exhaust port and
    // stay identical in cruise and boost; only downstream rings encode boost.
    if (aircraftVisible) {
        for (const EngineSpec& engine : kEngineSpecs) {
            const auto core = projectShip({engine.x, engine.y, kAircraftEngineRingZ.back()});
            canvas.drawCircle(core[0], core[1], kAircraftEngineCoreGlowRadiusPx,
                              engineCoreHalo);
            canvas.fillCircle(core[0], core[1], kAircraftEngineCoreRadiusPx,
                              engineCoreGlow);
            canvas.drawPixel(core[0], core[1], engineCore);
        }
    }

    // A cyan canopy cage breaks up the white fuselage and reads as a cockpit,
    // while remaining transparent wireframe geometry.
    constexpr std::array<Vec3, 6> kCanopy = {{
        { 0.00f,0.52f,3.40f}, {-0.28f,0.38f,3.15f}, {0.28f,0.38f,3.15f},
        { 0.00f,0.82f,0.55f}, {-0.44f,0.48f,0.38f}, {0.44f,0.48f,0.38f},
    }};
    drawShipLine(kCanopy[0], kCanopy[1], canopyColor);
    drawShipLine(kCanopy[0], kCanopy[2], canopyColor);
    drawShipLine(kCanopy[1], kCanopy[2], canopyColor);
    drawShipLine(kCanopy[3], kCanopy[4], canopyColor);
    drawShipLine(kCanopy[3], kCanopy[5], canopyColor);
    drawShipLine(kCanopy[4], kCanopy[5], canopyColor);
    drawShipLine(kCanopy[0], kCanopy[3], canopyColor);
    drawShipLine(kCanopy[1], kCanopy[4], canopyColor);
    drawShipLine(kCanopy[2], kCanopy[5], canopyColor);

    // Synchronized double-flash anti-collision strobes sit on the outer wing
    // panels. A two-pixel cool-white beacon is enough to read on the watch;
    // no decorative housing or persistent marker competes with the wireframe.
    if (aircraftVisible && aircraftWingStrobeOn(frameMillis)) {
        for (float side : {-1.0f, 1.0f}) {
            const auto beacon = projectShip(
                wingPoint(side, kAircraftWingStations.back(), 0.72f));
            canvas.fillCircle(beacon[0], beacon[1], kAircraftWingStrobeRadiusPx,
                              strobeGlow);
            canvas.drawPixel(beacon[0], beacon[1], strobeCore);
        }
    }

    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setTextSize(1);

    const int heading = canyonHudHeadingDegrees(route, flight.turnYaw);
    canvas.setCursor(92, 50);
    canvas.print("VR-01");
    canvas.setCursor(_width - 120, 50);
    canvas.print(canyonHudViewModeLabel(aircraftVisible));
    canvas.setCursor(centerX - 25, 42);
    canvas.print("HDG");
    canvas.setCursor(centerX + 1, 42);
    canvas.printf("%03d", heading);
    canvas.drawLine(centerX - 55, 63, centerX + 55, 63, hudDim);
    for (int tick = -2; tick <= 2; ++tick) {
        const int x = centerX + tick * 22;
        const int length = tick == 0 ? 8 : 4;
        canvas.drawLine(x, 63, x, 63 + length, tick == 0 ? hudAccent : hudColor);
    }
    canvas.drawLine(centerX - 4, 75, centerX, 69, hudAccent);
    canvas.drawLine(centerX, 69, centerX + 4, 75, hudAccent);

    // Bank scale: fixed ticks with a moving cyan pointer.
    constexpr int kBankCenterY = 112;
    constexpr int kBankRadius = 30;
    for (int angle = -45; angle <= 45; angle += 15) {
        const float bankRadians = static_cast<float>(angle - 90) * 0.0174532925f;
        const int x1 = centerX + static_cast<int>(std::cos(bankRadians) * kBankRadius);
        const int y1 = kBankCenterY + static_cast<int>(std::sin(bankRadians) * kBankRadius);
        const int tickLength = angle == 0 ? 7 : (angle % 30 == 0 ? 5 : 3);
        const int x2 = centerX + static_cast<int>(std::cos(bankRadians) * (kBankRadius - tickLength));
        const int y2 = kBankCenterY + static_cast<int>(std::sin(bankRadians) * (kBankRadius - tickLength));
        canvas.drawLine(x1, y1, x2, y2, angle == 0 ? hudColor : hudDim);
    }
    const float bankPointerRadians = (-flight.roll - 90.0f) * 0.0174532925f;
    const int bankPointerX = centerX + static_cast<int>(std::cos(bankPointerRadians) * (kBankRadius - 10));
    const int bankPointerY = kBankCenterY + static_cast<int>(std::sin(bankPointerRadians) * (kBankRadius - 10));
    canvas.drawCircle(bankPointerX, bankPointerY, 2, hudAccent);

    // Earth-referenced HUD layer. It is projected by the same chase camera as
    // the canyon, so the zero rung overlays the rendered world horizon. The
    // chase camera is world-up stabilized; aircraft roll belongs to the ship
    // and bank pointer, not to this line.
    CanyonHudReferenceFrame hudReference{};
    const bool hudReferenceValid =
        makeCanyonHudReferenceFrame(camera, hudReference);
    const float horizonDirectionLength = std::sqrt(
        1.0f + hudReference.horizonSlope * hudReference.horizonSlope);
    const float horizonDirectionX = 1.0f / horizonDirectionLength;
    const float horizonDirectionY =
        hudReference.horizonSlope / horizonDirectionLength;
    const auto attitudePoint = [&](CanyonScreenPoint center, float along,
                                   float normal = 0.0f) {
        return CanyonScreenPoint{
            center.x + along * horizonDirectionX - normal * horizonDirectionY,
            center.y + along * horizonDirectionY + normal * horizonDirectionX,
        };
    };
    const auto drawAttitudeSegment = [&](CanyonScreenPoint rungCenter,
                                         float from, float to,
                                         uint16_t color) {
        const CanyonScreenPoint start = attitudePoint(rungCenter, from);
        const CanyonScreenPoint end = attitudePoint(rungCenter, to);
        canvas.drawLine(static_cast<int>(std::lround(start.x)),
                        static_cast<int>(std::lround(start.y)),
                        static_cast<int>(std::lround(end.x)),
                        static_cast<int>(std::lround(end.y)), color);
    };
    const auto drawAttitudeDashes = [&](CanyonScreenPoint rungCenter,
                                        float from, float to,
                                        uint16_t color) {
        constexpr float kDashLength = 5.0f;
        constexpr float kDashStride = 9.0f;
        for (float position = from; position < to; position += kDashStride) {
            drawAttitudeSegment(rungCenter, position,
                                std::min(position + kDashLength, to), color);
        }
    };
    for (int level = -2; level <= 2; ++level) {
        CanyonScreenPoint rungCenter{};
        if (!hudReferenceValid ||
            !projectCanyonHudElevation(camera, hudReference,
                                       static_cast<float>(level * 5),
                                       rungCenter)) {
            continue;
        }
        const int halfWidth = level == 0 ? 78 : (std::abs(level) == 1 ? 35 : 27);
        const int gap = level == 0 ? (aircraftVisible ? 14 : 30) : 11;
        const uint16_t color = level == 0
            ? hudColor
            : (std::abs(level) == 1 ? hudMid : hudDim);
        if (level < 0) {
            drawAttitudeDashes(rungCenter, -halfWidth, -gap, color);
            drawAttitudeDashes(rungCenter, gap, halfWidth, color);
        } else {
            drawAttitudeSegment(rungCenter, -halfWidth, -gap, color);
            drawAttitudeSegment(rungCenter, gap, halfWidth, color);
        }
        if (level != 0) {
            const float tickNormal = level > 0 ? 5.0f : -5.0f;
            const CanyonScreenPoint leftStart = attitudePoint(rungCenter, -halfWidth);
            const CanyonScreenPoint leftEnd =
                attitudePoint(rungCenter, -halfWidth, tickNormal);
            const CanyonScreenPoint rightStart = attitudePoint(rungCenter, halfWidth);
            const CanyonScreenPoint rightEnd =
                attitudePoint(rungCenter, halfWidth, tickNormal);
            canvas.drawLine(static_cast<int>(std::lround(leftStart.x)),
                            static_cast<int>(std::lround(leftStart.y)),
                            static_cast<int>(std::lround(leftEnd.x)),
                            static_cast<int>(std::lround(leftEnd.y)), color);
            canvas.drawLine(static_cast<int>(std::lround(rightStart.x)),
                            static_cast<int>(std::lround(rightStart.y)),
                            static_cast<int>(std::lround(rightEnd.x)),
                            static_cast<int>(std::lround(rightEnd.y)), color);
            canvas.setTextColor(color, TFT_BLACK);
            const CanyonScreenPoint leftLabel =
                attitudePoint(rungCenter, -halfWidth - 20.0f);
            const CanyonScreenPoint rightLabel =
                attitudePoint(rungCenter, halfWidth + 6.0f);
            if (hudPitchLabelVisible(leftLabel.x, leftLabel.y, aircraftVisible,
                                     _width, _height)) {
                canvas.setCursor(static_cast<int>(std::lround(leftLabel.x)),
                                 static_cast<int>(std::lround(leftLabel.y)) - 3);
                canvas.printf("%+d", level * 5);
            }
            if (hudPitchLabelVisible(rightLabel.x, rightLabel.y, aircraftVisible,
                                     _width, _height)) {
                canvas.setCursor(static_cast<int>(std::lround(rightLabel.x)),
                                 static_cast<int>(std::lround(rightLabel.y)) - 3);
                canvas.printf("%+d", level * 5);
            }
        }
    }
    canvas.setTextColor(hudColor, TFT_BLACK);

    // Route cue: a diamond follows the projected far canyon center in both
    // axes. Avoid the circle-with-wings silhouette reserved for a real flight
    // path marker until FlightState exposes lateral/vertical velocity.
    constexpr float kRouteCueMinX = 88.0f;
    constexpr float kRouteCueMaxInsetX = 88.0f;
    constexpr float kRouteCueMinY = 92.0f;
    constexpr float kThirdPersonCueMaxY = 216.0f;
    constexpr float kFirstPersonCueBottomInset = 90.0f;
    const HudBoundedCue boundedRouteCue = constrainHudCueToRect(
        {routeCue.x, routeCue.y}, {camera.principalX, camera.principalY},
        kRouteCueMinX, static_cast<float>(_width) - kRouteCueMaxInsetX,
        kRouteCueMinY, aircraftVisible
            ? kThirdPersonCueMaxY
            : static_cast<float>(_height) - kFirstPersonCueBottomInset);
    const int routeCueX = static_cast<int>(std::lround(boundedRouteCue.point.x));
    const int routeCueY = static_cast<int>(std::lround(boundedRouteCue.point.y));
    constexpr int kRouteCueRadius = 6;
    const HudLayoutPoint cameraDatum{camera.principalX, camera.principalY};
    if (hudRouteCueVisible(boundedRouteCue, cameraDatum, aircraftVisible)) {
        canvas.drawLine(routeCueX, routeCueY - kRouteCueRadius,
                        routeCueX + kRouteCueRadius, routeCueY, hudAccent);
        canvas.drawLine(routeCueX + kRouteCueRadius, routeCueY,
                        routeCueX, routeCueY + kRouteCueRadius, hudAccent);
        canvas.drawLine(routeCueX, routeCueY + kRouteCueRadius,
                        routeCueX - kRouteCueRadius, routeCueY, hudAccent);
        canvas.drawLine(routeCueX - kRouteCueRadius, routeCueY,
                        routeCueX, routeCueY - kRouteCueRadius, hudAccent);
        if (boundedRouteCue.constrained) {
            canvas.drawLine(
                routeCueX + static_cast<int>(std::lround(boundedRouteCue.directionX * 9.0f)),
                routeCueY + static_cast<int>(std::lround(boundedRouteCue.directionY * 9.0f)),
                routeCueX + static_cast<int>(std::lround(boundedRouteCue.directionX * 14.0f)),
                routeCueY + static_cast<int>(std::lround(boundedRouteCue.directionY * 14.0f)),
                hudAccent);
        }
    }
    if (!aircraftVisible) {
        const int datumY = static_cast<int>(std::lround(camera.principalY));
        canvas.drawLine(centerX - 24, datumY, centerX - 8, datumY, hudColor);
        canvas.drawLine(centerX - 8, datumY, centerX, datumY + 7, hudColor);
        canvas.drawLine(centerX, datumY + 7, centerX + 8, datumY, hudColor);
        canvas.drawLine(centerX + 8, datumY, centerX + 24, datumY, hudColor);
    }

    // Speed tape with primary/secondary ticks and a boxed active readout.
    canvas.setCursor(39, 127);
    canvas.print("SPD");
    const float forwardSpeed = effectiveFlightForwardSpeed(flight);
    canvas.drawRect(34, 139, 35, 15, hudDim);
    canvas.setCursor(40, 143);
    canvas.printf("%03d", static_cast<int>(forwardSpeed + 0.5f));
    canvas.drawLine(45, 160, 45, 264, hudDim);
    const int speedMarkerY = 255 - std::clamp(
        static_cast<int>((forwardSpeed - 42.0f) * 1.05f), 0, 95);
    for (int tick = 0; tick <= 10; ++tick) {
        const int y = 160 + tick * 10;
        const bool major = tick % 2 == 0;
        canvas.drawLine(45, y, major ? 59 : 52, y, major ? hudColor : hudDim);
    }
    canvas.drawLine(40, speedMarkerY, 61, speedMarkerY, hudAccent);
    canvas.drawLine(61, speedMarkerY, 65, speedMarkerY - 3, hudAccent);
    canvas.drawLine(61, speedMarkerY, 65, speedMarkerY + 3, hudAccent);
    // AGL tape mirrors speed and reports the same floor clearance used by the
    // collision model.
    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setCursor(_width - 72, 127);
    canvas.print("AGL");
    canvas.drawRect(_width - 81, 139, 42, 15, hudDim);
    canvas.setCursor(_width - 77, 143);
    canvas.printf("%+03d", static_cast<int>(collision.floorClearance * 10.0f));
    canvas.drawLine(_width - 45, 160, _width - 45, 264, hudDim);
    const int altitudeMarkerY = 208 -
        std::clamp(static_cast<int>(collision.floorClearance * 32.0f), -42, 42);
    for (int tick = 0; tick <= 10; ++tick) {
        const int y = 160 + tick * 10;
        const bool major = tick % 2 == 0;
        canvas.drawLine(_width - 45, y, _width - (major ? 59 : 52), y, major ? hudColor : hudDim);
    }
    canvas.drawLine(_width - 61, altitudeMarkerY, _width - 40, altitudeMarkerY, hudAccent);
    canvas.drawLine(_width - 61, altitudeMarkerY, _width - 65, altitudeMarkerY - 3, hudAccent);
    canvas.drawLine(_width - 61, altitudeMarkerY, _width - 65, altitudeMarkerY + 3, hudAccent);
    // Compact course-deviation scale sits above the fighter, not over terrain.
    // Keep the course cue above the raised aircraft, between the two side tapes.
    const int kCourseY = aircraftVisible ? kAircraftCourseCueY : 330;
    canvas.drawLine(centerX - 52, kCourseY, centerX + 52, kCourseY, hudDim);
    for (int tick = -2; tick <= 2; ++tick) {
        const int x = centerX + tick * 21;
        canvas.drawLine(x, kCourseY - 3, x, kCourseY + 3, tick == 0 ? hudColor : hudDim);
    }
    const int deviationX = centerX + std::clamp(static_cast<int>(flight.lateralOffset * 19.0f), -42, 42);
    canvas.drawLine(deviationX - 4, kCourseY - 9, deviationX, kCourseY - 4, hudAccent);
    canvas.drawLine(deviationX, kCourseY - 4, deviationX + 4, kCourseY - 9, hudAccent);

    canvas.setCursor(92, _height - 48);
    const HudInputAlert inputAlert = canyonHudInputAlert(inputStatus);
    if (inputAlert == HudInputAlert::AxisLost) {
        canvas.setTextColor(caution, TFT_BLACK);
        canvas.print("AXIS LOST");
    } else if (inputAlert == HudInputAlert::ActionLost) {
        canvas.setTextColor(caution, TFT_BLACK);
        canvas.print("ACT LOST");
    } else {
        canvas.print("IN ");
        canvas.print(axisSourceLabel(inputStatus.axisSource));
    }
    canvas.setCursor(_width - 143, _height - 48);
    canvas.setTextColor(flight.boostAmount > 0.05f ? caution : hudColor, TFT_BLACK);
    canvas.print(flight.boostAmount > 0.05f ? "PWR BST" : "PWR CRZ");
    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setCursor(centerX - 34, _height - 48);
    canvas.printf("R%+03d P%+03d", static_cast<int>(flight.roll), static_cast<int>(flight.pitch));
    if (flight.collided) {
        canvas.setTextColor(impact, TFT_BLACK);
        canvas.drawLine(centerX - 52, 190, centerX + 52, 190, impact);
        canvas.setCursor(centerX - 34, 198);
        canvas.print(impactLabel(collision.impactHazard));
        canvas.setCursor(centerX - 37, 210);
        canvas.print("K1 HOLD RESET");
    } else if (flight.paused) {
        canvas.setTextColor(hudColor, TFT_BLACK);
        canvas.setCursor(centerX - 20, 198);
        canvas.print("PAUSED");
    } else if (collision.warning) {
        canvas.setTextColor(caution, TFT_BLACK);
        canvas.drawLine(centerX - 34, 190, centerX + 34, 190, caution);
        canvas.setCursor(centerX - 28, 198);
        canvas.print(warningLabel(collision.warningHazard));
    }

    display.endWrite();
}

}  // namespace vector_canyon_fighter
