#include "vector_canyon_renderer.h"

#include "explicit_canyon_projection.h"
#include "vector_canyon_config.h"

#include <hal/hal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace vector_canyon_fighter {
namespace {

constexpr int kHorizonY = 150;
// 146 px above the 466 px panel bottom: visually lower-middle, with enough
// canyon visible below the aircraft to read its compact ground shadow.
constexpr int kShipCenterY = kAircraftScreenCenterY;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct ShipEdge {
    uint8_t from;
    uint8_t to;
    uint8_t style;
};

// v0-v13: fuselage  v14-v25: wings (upper L/R, lower L/R)
// v26-v33: left-outer engine (front: top/bot/inner/outer, rear: top/bot/inner/outer)
// v34-v41: left-inner engine   v42-v49: right-inner engine   v50-v57: right-outer engine
// v58-v61: canopy struts   v62-v65: wing-tip weapon pods
constexpr std::array<Vec3, 66> kShipVertices = {{
    // Fuselage
    {0.00f, 0.02f, 3.70f},   {0.00f, 0.24f, 2.28f},   {-0.40f, 0.02f, 1.38f},
    {0.40f, 0.02f, 1.38f},   {0.00f, 0.72f, 0.88f},   {0.00f, 0.78f, -0.18f},
    {-0.44f, 0.34f, -0.08f}, {0.44f, 0.34f, -0.08f},  {-0.56f, -0.04f, 0.02f},
    {0.56f, -0.04f, 0.02f},  {0.00f, -0.28f, 0.92f},  {0.00f, -0.34f, -1.54f},
    {0.00f, 0.14f, -2.18f},  {0.00f, -0.22f, -2.34f},
    // Wings: upper left, upper right, lower left, lower right
    {-0.62f, 0.16f, 0.55f},  {-1.65f, 0.38f, -0.05f}, {-3.00f, 0.62f, -0.72f},
    {0.62f, 0.16f, 0.55f},   {1.65f, 0.38f, -0.05f},  {3.00f, 0.62f, -0.72f},
    {-0.62f, -0.14f, 0.10f}, {-1.55f, -0.42f, -0.55f},{-2.75f, -0.68f, -1.28f},
    {0.62f, -0.14f, 0.10f},  {1.55f, -0.42f, -0.55f}, {2.75f, -0.68f, -1.28f},
    // Left outer engine nacelle – front ring (top, bot, inner x+, outer x-)
    {-1.65f, 0.48f, -0.28f}, {-1.65f, 0.04f, -0.28f}, {-1.43f, 0.26f, -0.28f}, {-1.87f, 0.26f, -0.28f},
    // Left outer engine – rear ring
    {-1.65f, 0.36f, -1.55f}, {-1.65f, -0.08f, -1.55f},{-1.43f, 0.14f, -1.55f}, {-1.87f, 0.14f, -1.55f},
    // Left inner engine nacelle – front ring
    {-0.70f, 0.00f, -0.38f}, {-0.70f, -0.44f, -0.38f},{-0.48f, -0.22f, -0.38f},{-0.92f, -0.22f, -0.38f},
    // Left inner engine – rear ring
    {-0.70f, -0.10f, -1.72f},{-0.70f, -0.54f, -1.72f},{-0.48f, -0.32f, -1.72f},{-0.92f, -0.32f, -1.72f},
    // Right inner engine nacelle – front ring
    {0.70f, 0.00f, -0.38f},  {0.70f, -0.44f, -0.38f}, {0.92f, -0.22f, -0.38f}, {0.48f, -0.22f, -0.38f},
    // Right inner engine – rear ring
    {0.70f, -0.10f, -1.72f}, {0.70f, -0.54f, -1.72f}, {0.92f, -0.32f, -1.72f}, {0.48f, -0.32f, -1.72f},
    // Right outer engine nacelle – front ring
    {1.65f, 0.48f, -0.28f},  {1.65f, 0.04f, -0.28f},  {1.87f, 0.26f, -0.28f},  {1.43f, 0.26f, -0.28f},
    // Right outer engine – rear ring
    {1.65f, 0.36f, -1.55f},  {1.65f, -0.08f, -1.55f}, {1.87f, 0.14f, -1.55f},  {1.43f, 0.14f, -1.55f},
    // Canopy struts
    {-0.52f, 0.20f, -1.50f}, {-0.64f, 1.02f, -1.88f}, {0.52f, 0.20f, -1.50f},  {0.64f, 1.02f, -1.88f},
    // Wing tip weapon pods
    {-3.06f, 0.62f, -1.62f}, {3.06f, 0.62f, -1.62f},  {-2.80f, -0.68f, -2.04f},{2.80f, -0.68f, -2.04f},
}};

constexpr std::array<ShipEdge, 114> kShipEdges = {{
    // Fuselage
    {0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {1, 2, 0}, {1, 3, 0}, {1, 4, 2},
    {4, 5, 2}, {4, 6, 2}, {4, 7, 2}, {5, 6, 2}, {5, 7, 2}, {6, 7, 2},
    {2, 8, 1}, {3, 9, 1}, {2, 10, 0}, {3, 10, 0}, {8, 9, 0}, {8, 11, 1},
    {9, 11, 1}, {10, 11, 0}, {8, 12, 1}, {9, 12, 1}, {11, 13, 0}, {12, 13, 1},
    // Upper wings
    {2, 14, 1}, {14, 15, 1}, {15, 16, 1}, {16, 8, 1}, {14, 16, 0}, {15, 8, 0},
    {3, 17, 1}, {17, 18, 1}, {18, 19, 1}, {19, 9, 1}, {17, 19, 0}, {18, 9, 0},
    // Lower wings
    {8, 20, 1}, {20, 21, 1}, {21, 22, 1}, {22, 11, 1}, {20, 22, 0}, {21, 11, 0},
    {9, 23, 1}, {23, 24, 1}, {24, 25, 1}, {25, 11, 1}, {23, 25, 0}, {24, 11, 0},
    // Wing S-foil cross-sections (upper mid/tip ↔ lower mid/tip for thickness)
    {15, 21, 0}, {16, 22, 0}, {18, 24, 0}, {19, 25, 0},
    // Left outer engine: front ring (top→inner→bot→outer→top)
    {26, 28, 0}, {28, 27, 0}, {27, 29, 0}, {29, 26, 0},
    // rear ring + longitudinals + wing attachment
    {30, 32, 0}, {32, 31, 0}, {31, 33, 0}, {33, 30, 0},
    {26, 30, 1}, {27, 31, 1}, {28, 32, 0}, {29, 33, 0}, {15, 26, 0},
    // Left inner engine: front ring
    {34, 36, 0}, {36, 35, 0}, {35, 37, 0}, {37, 34, 0},
    // rear ring + longitudinals + wing attachment
    {38, 40, 0}, {40, 39, 0}, {39, 41, 0}, {41, 38, 0},
    {34, 38, 1}, {35, 39, 1}, {36, 40, 0}, {37, 41, 0}, {20, 34, 0},
    // Right inner engine: front ring
    {42, 44, 0}, {44, 43, 0}, {43, 45, 0}, {45, 42, 0},
    // rear ring + longitudinals + wing attachment
    {46, 48, 0}, {48, 47, 0}, {47, 49, 0}, {49, 46, 0},
    {42, 46, 1}, {43, 47, 1}, {44, 48, 0}, {45, 49, 0}, {23, 42, 0},
    // Right outer engine: front ring
    {50, 52, 0}, {52, 51, 0}, {51, 53, 0}, {53, 50, 0},
    // rear ring + longitudinals + wing attachment
    {54, 56, 0}, {56, 55, 0}, {55, 57, 0}, {57, 54, 0},
    {50, 54, 1}, {51, 55, 1}, {52, 56, 0}, {53, 57, 0}, {18, 50, 0},
    // Canopy struts
    {8, 58, 1}, {58, 59, 1}, {59, 12, 1}, {9, 60, 1}, {60, 61, 1}, {61, 12, 1},
    // Wing tip weapon pods
    {16, 62, 1}, {19, 63, 1}, {22, 64, 1}, {25, 65, 1},
}};

uint16_t scaleRgb565(uint16_t color, float weight)
{
    const uint16_t safeWeight = static_cast<uint16_t>(std::lround(std::clamp(weight, 0.0f, 1.0f) * 256.0f));
    const uint16_t red = static_cast<uint16_t>(((color >> 11) & 0x1fu) * safeWeight >> 8);
    const uint16_t green = static_cast<uint16_t>(((color >> 5) & 0x3fu) * safeWeight >> 8);
    const uint16_t blue = static_cast<uint16_t>((color & 0x1fu) * safeWeight >> 8);
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
                      const CollisionStatus& collision, float calibrationProgress)
{
    renderGame(flight, terrain, collision, calibrationProgress);
}

void Renderer::renderGame(const FlightState& flight, const ExplicitCanyonStream& terrain,
                          const CollisionStatus& collision, float calibrationProgress)
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
    const uint16_t shipColor = display.color565(228, 238, 235);
    const uint16_t shipDim = display.color565(84, 107, 106);
    const uint16_t canopyColor = display.color565(101, 183, 200);
    const uint16_t exhaust = display.color565(155, 225, 236);
    const uint16_t hudColor = display.color565(114, 230, 162);
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
        const auto drawCentered = [&](const char* text, int y) {
            canvas.setCursor(cx - canvas.textWidth(text) / 2, y);
            canvas.print(text);
        };

        // A compact centered instrument stack. Text widths come from the
        // active display font instead of hand-tuned character offsets.
        canvas.setTextSize(1);
        canvas.setTextColor(hudDim, TFT_BLACK);
        drawCentered("FLIGHT CONTROL / IMU", cy - 86);

        canvas.setTextSize(2);
        canvas.setTextColor(hudAccent, TFT_BLACK);
        drawCentered("CALIBRATING", cy - 61);
        canvas.drawLine(cx - 102, cy - 43, cx - 76, cy - 43, hudDim);
        canvas.drawLine(cx + 76, cy - 43, cx + 102, cy - 43, hudDim);

        canvas.setTextSize(1);
        canvas.setTextColor(hudColor, TFT_BLACK);
        drawCentered("HOLD DEVICE LEVEL", cy - 25);

        constexpr int kBarW = 160;
        constexpr int kBarH = 7;
        const int barX = cx - kBarW / 2;
        const int barY = cy + 2;
        canvas.drawRect(barX - 1, barY - 1, kBarW + 2, kBarH + 2, hudDim);
        const int filled = std::clamp(static_cast<int>(calibrationProgress * kBarW), 0, kBarW);
        canvas.fillRect(barX, barY, filled, kBarH, hudColor);
        for (int division = 1; division < 4; ++division) {
            const int tickX = barX + division * kBarW / 4;
            canvas.drawLine(tickX, barY - 4, tickX, barY - 2, hudDim);
            canvas.drawLine(tickX, barY + kBarH + 2, tickX, barY + kBarH + 4, hudDim);
        }

        const float remaining = (1.0f - calibrationProgress) * 2.5f;
        char countdown[16]{};
        std::snprintf(countdown, sizeof(countdown), "T- %.1f SEC", remaining);
        canvas.setTextColor(hudDim, TFT_BLACK);
        drawCentered(countdown, cy + 24);
        canvas.setTextColor(hudColor, TFT_BLACK);
        drawCentered("KEEP STILL", cy + 50);
        display.endWrite();
        return;
    }

    canvas.fillScreen(TFT_BLACK);

    const CanyonRouteFrame route = terrain.routeFrameAt(terrain.playerWorldS());
    const CanyonCamera camera = makeExplicitCanyonChaseCamera(
        route, flight.lateralOffset, flight.altitude, flight.pitch, _width, _height);
    int vanishingX = centerX;
    const bool terrainVisible = drawExplicitTerrain(
        camera, terrain, terrainPrimary, terrainMid, terrainSecondary);
    CanyonScreenPoint farCenter{};
    if (projectExplicitCanyonPoint(
            camera,
            explicitCanyonToCamera(camera, terrain.worldPoint(
                ExplicitCanyonStream::kSliceCount - 1,
                static_cast<std::size_t>(CanyonProfilePoint::FloorCenter))),
            farCenter)) {
        vanishingX = static_cast<int>(std::lround(farCenter.x));
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
    const auto projectShip = [&](const Vec3& point) {
        // Rotate the model itself around its lateral axis. The former renderer
        // only translated the whole sprite vertically, so its silhouette never
        // communicated pitch.
        const AircraftScreenOffset offset =
            projectAircraftPose(point.x, point.y, point.z, flight.pitch, flight.roll);
        return std::array<int, 2>{
            shipX + static_cast<int>(offset.x),
            shipY + static_cast<int>(offset.y),
        };
    };

    // Keep the ground cue deliberately small. A collision envelope spans the
    // camera depth and becomes a giant trapezoid under perspective; it is not
    // an appropriate shadow shape. This flattened footprint only communicates
    // aircraft position and AGL, leaving the canyon grid exposed.
    const AircraftGroundShadow shadow = makeAircraftGroundShadow(collision.floorClearance);
    const uint16_t shadowFill = scaleRgb565(terrainPrimary, shadow.brightness);
    const uint16_t shadowEdge = scaleRgb565(terrainPrimary, shadow.brightness + 0.10f);
    canvas.fillEllipse(shipX, shadow.centerY, shadow.radiusX, shadow.radiusY, shadowFill);
    canvas.drawEllipse(shipX, shadow.centerY, shadow.radiusX, shadow.radiusY, shadowEdge);

    for (const auto& edge : kShipEdges) {
        const auto from = projectShip(kShipVertices[edge.from]);
        const auto to = projectShip(kShipVertices[edge.to]);
        const uint16_t color = edge.style == 2 ? canopyColor : (edge.style == 1 ? shipColor : shipDim);
        if (edge.style == 1) canvas.drawLine(from[0], from[1] + 1, to[0], to[1] + 1, shipDim);
        canvas.drawLine(from[0], from[1], to[0], to[1], color);
    }

    const float exhaustTailZ = -2.30f - 0.86f * flight.boostAmount;
    const std::array<Vec3, 8> exhaustPoints = {{
        {-1.65f, 0.14f, -1.55f}, {-1.65f, 0.14f, exhaustTailZ},
        {-0.70f, -0.32f, -1.72f}, {-0.70f, -0.32f, exhaustTailZ},
        {0.70f, -0.32f, -1.72f}, {0.70f, -0.32f, exhaustTailZ},
        {1.65f, 0.14f, -1.55f}, {1.65f, 0.14f, exhaustTailZ},
    }};
    for (int engine = 0; engine < 4; ++engine) {
        const auto from = projectShip(exhaustPoints[engine * 2]);
        const auto to = projectShip(exhaustPoints[engine * 2 + 1]);
        canvas.drawLine(from[0], from[1], to[0], to[1], exhaust);
    }

    // Mach ring (shock diamond) effect – animated contracting diamond rings along each plume
    if (flight.boostAmount > 0.08f) {
        const uint16_t machColor = display.color565(92, 210, 230);
        const float machPhase = static_cast<float>(GetHAL().millis() % 400u) / 400.0f;
        const std::array<float, 12> kNozzles = {{
            -1.65f,  0.14f, -1.55f,
            -0.70f, -0.32f, -1.72f,
             0.70f, -0.32f, -1.72f,
             1.65f,  0.14f, -1.55f,
        }};
        for (int eng = 0; eng < 4; ++eng) {
            const float nx = kNozzles[static_cast<size_t>(eng) * 3];
            const float ny = kNozzles[static_cast<size_t>(eng) * 3 + 1];
            const float nz = kNozzles[static_cast<size_t>(eng) * 3 + 2];
            for (int ring = 0; ring < 2; ++ring) {
                const float frac = std::fmod(machPhase + ring * 0.5f, 1.0f);
                const float ringZ = nz - 0.12f - frac * 0.80f;
                const float r = 0.21f * (0.45f + 0.42f * std::sin(frac * 3.14159f));
                const std::array<Vec3, 4> pts = {{
                    {nx,       ny + r, ringZ},
                    {nx + r,   ny,     ringZ},
                    {nx,       ny - r, ringZ},
                    {nx - r,   ny,     ringZ},
                }};
                for (int s = 0; s < 4; ++s) {
                    const auto p1 = projectShip(pts[static_cast<size_t>(s)]);
                    const auto p2 = projectShip(pts[static_cast<size_t>((s + 1) % 4)]);
                    canvas.drawLine(p1[0], p1[1], p2[0], p2[1], machColor);
                }
            }
        }
    }

    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setTextSize(1);

    const int heading = static_cast<int>(flight.heading + 0.5f) % 360;
    canvas.setCursor(92, 50);
    canvas.print("VR-01");
    canvas.setCursor(_width - 120, 50);
    canvas.print("NAV");
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

    // Pitch ladder rotates with roll while labels remain upright for legibility.
    const int attitudeY = kHorizonY + static_cast<int>(flight.pitch * 1.25f);
    const float attitudeRadians = -flight.roll * 0.0174532925f;
    const float attitudeCosine = std::cos(attitudeRadians);
    const float attitudeSine = std::sin(attitudeRadians);
    const auto drawAttitudeLine = [&](int x1, int y1, int x2, int y2, uint16_t color) {
        const auto rotateX = [&](int x, int y) {
            return vanishingX + static_cast<int>(x * attitudeCosine - y * attitudeSine);
        };
        const auto rotateY = [&](int x, int y) {
            return attitudeY + static_cast<int>(x * attitudeSine + y * attitudeCosine);
        };
        canvas.drawLine(rotateX(x1, y1), rotateY(x1, y1), rotateX(x2, y2), rotateY(x2, y2), color);
    };
    for (int level = -2; level <= 2; ++level) {
        const int localY = -level * 18;
        const int halfWidth = level == 0 ? 78 : (std::abs(level) == 1 ? 35 : 27);
        const int gap = level == 0 ? 14 : 11;
        const uint16_t color = level == 0 ? hudColor : hudDim;
        drawAttitudeLine(-halfWidth, localY, -gap, localY, color);
        drawAttitudeLine(gap, localY, halfWidth, localY, color);
        if (level != 0) {
            drawAttitudeLine(-halfWidth, localY, -halfWidth, localY + (level > 0 ? 5 : -5), color);
            drawAttitudeLine(halfWidth, localY, halfWidth, localY + (level > 0 ? 5 : -5), color);
            canvas.setTextColor(hudDim, TFT_BLACK);
            canvas.setCursor(vanishingX - halfWidth - 17, attitudeY + localY - 3);
            canvas.printf("%d", std::abs(level) * 10);
            canvas.setCursor(vanishingX + halfWidth + 6, attitudeY + localY - 3);
            canvas.printf("%d", std::abs(level) * 10);
        }
    }
    canvas.setTextColor(hudColor, TFT_BLACK);

    // Flight-path marker follows the canyon vanishing point. The W marker is
    // the fixed aircraft datum, so their separation communicates flight path.
    canvas.drawCircle(vanishingX, kHorizonY, 5, hudAccent);
    canvas.drawLine(vanishingX - 14, kHorizonY, vanishingX - 5, kHorizonY, hudAccent);
    canvas.drawLine(vanishingX + 5, kHorizonY, vanishingX + 14, kHorizonY, hudAccent);
    canvas.drawLine(vanishingX, kHorizonY - 10, vanishingX, kHorizonY - 5, hudAccent);
    constexpr int kDatumY = 188;
    canvas.drawLine(centerX - 24, kDatumY, centerX - 8, kDatumY, hudColor);
    canvas.drawLine(centerX - 8, kDatumY, centerX, kDatumY + 7, hudColor);
    canvas.drawLine(centerX, kDatumY + 7, centerX + 8, kDatumY, hudColor);
    canvas.drawLine(centerX + 8, kDatumY, centerX + 24, kDatumY, hudColor);

    // Speed tape with primary/secondary ticks and a boxed active readout.
    canvas.setCursor(39, 127);
    canvas.print("SPD");
    canvas.drawRect(34, 139, 35, 15, hudDim);
    canvas.setCursor(40, 143);
    canvas.printf("%03d", static_cast<int>(flight.speed + 0.5f));
    canvas.drawLine(45, 160, 45, 264, hudDim);
    const int speedMarkerY = 255 - std::clamp(static_cast<int>((flight.speed - 42.0f) * 1.05f), 0, 95);
    for (int tick = 0; tick <= 10; ++tick) {
        const int y = 160 + tick * 10;
        const bool major = tick % 2 == 0;
        canvas.drawLine(45, y, major ? 59 : 52, y, major ? hudColor : hudDim);
    }
    canvas.drawLine(40, speedMarkerY, 61, speedMarkerY, hudAccent);
    canvas.drawLine(61, speedMarkerY, 65, speedMarkerY - 3, hudAccent);
    canvas.drawLine(61, speedMarkerY, 65, speedMarkerY + 3, hudAccent);
    canvas.setTextColor(hudDim, TFT_BLACK);
    canvas.setCursor(35, 270);
    canvas.printf("T%02d", std::clamp(static_cast<int>((flight.speed - 42.0f) * 0.67f), 0, 60));

    // AGL tape mirrors speed; it reports the same floor clearance used by the
    // collision model, while the lower cue remains the aircraft pitch trend.
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
    canvas.setTextColor(hudDim, TFT_BLACK);
    canvas.setCursor(_width - 75, 270);
    canvas.printf("P%+03d", static_cast<int>(flight.pitch));

    // Compact course-deviation scale sits above the fighter, not over terrain.
    // Keep the course cue above the raised aircraft, between the two side tapes.
    constexpr int kCourseY = kAircraftCourseCueY;
    canvas.drawLine(centerX - 52, kCourseY, centerX + 52, kCourseY, hudDim);
    for (int tick = -2; tick <= 2; ++tick) {
        const int x = centerX + tick * 21;
        canvas.drawLine(x, kCourseY - 3, x, kCourseY + 3, tick == 0 ? hudColor : hudDim);
    }
    const int deviationX = centerX + std::clamp(static_cast<int>(flight.lateralOffset * 19.0f), -42, 42);
    canvas.drawLine(deviationX - 4, kCourseY - 9, deviationX, kCourseY - 4, hudAccent);
    canvas.drawLine(deviationX, kCourseY - 4, deviationX + 4, kCourseY - 9, hudAccent);

    canvas.setCursor(92, _height - 48);
    canvas.print("IN IMU");
    canvas.setCursor(_width - 143, _height - 48);
    canvas.setTextColor(flight.boostAmount > 0.05f ? caution : hudColor, TFT_BLACK);
    canvas.print(flight.boostAmount > 0.05f ? "THR BOOST" : "THR CRZ");
    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setCursor(centerX - 34, _height - 48);
    canvas.printf("R%+03d P%+03d", static_cast<int>(flight.roll), static_cast<int>(flight.pitch));
    if (flight.collided) {
        canvas.setTextColor(impact, TFT_BLACK);
        canvas.drawLine(centerX - 52, 190, centerX + 52, 190, impact);
        canvas.setCursor(centerX - 34, 198);
        canvas.print(impactLabel(collision.impactHazard));
        canvas.setCursor(centerX - 31, 210);
        canvas.print("A HOLD RESET");
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
