#include "garage_renderer.h"
#include "track_projection.h"

#include <hal/hal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lets_and_go {
namespace {

constexpr uint16_t kPaper = 0xef3au;
constexpr uint16_t kPencil = 0x4269u;
constexpr uint16_t kPencilFaint = 0x9cd3u;
constexpr uint16_t kPencilLight = 0xc638u;
constexpr uint16_t kCourseEdge = 0x31e8u;
constexpr uint16_t kCourseFaint = 0x8490u;

CarPoint animateWheelPoint(CarPoint point, const CarSpec& spec, WireStroke stroke,
                           float wheelCosine, float wheelSine)
{
    if (stroke != WireStroke::Mechanical ||
        std::abs(std::abs(point.x) - 0.59f) > 0.02f) {
        return point;
    }
    const float axleZ = std::abs(point.z - spec.frontAxleZ) <
                                std::abs(point.z - spec.rearAxleZ)
                            ? spec.frontAxleZ
                            : spec.rearAxleZ;
    const float y = point.y - spec.wheelRadius;
    const float z = point.z - axleZ;
    point.y = spec.wheelRadius + y * wheelCosine - z * wheelSine;
    point.z = axleZ + y * wheelSine + z * wheelCosine;
    return point;
}

struct ScreenPoint {
    int16_t x = 0;
    int16_t y = 0;
};

ScreenPoint project(CarPoint point, float yawCosine, float yawSine, float scale,
                    int centerX, int centerY)
{
    const float rotatedX = point.x * yawCosine + point.z * yawSine;
    const float rotatedZ = point.z * yawCosine - point.x * yawSine;
    const float depthScale = 3.3f / std::max(2.0f, 3.3f - rotatedZ * 0.38f);
    return {
        static_cast<int16_t>(std::lround(centerX + rotatedX * scale * depthScale)),
        static_cast<int16_t>(std::lround(centerY - point.y * scale * depthScale +
                                         rotatedZ * scale * 0.32f * depthScale)),
    };
}

uint16_t strokeColor(const CarSpec& spec, WireStroke stroke)
{
    switch (stroke) {
        case WireStroke::Accent: return spec.accentColor;
        case WireStroke::Mechanical: return kPencil;
        // The paper is the white body fill. A graphite outline stays readable for
        // all four liveries while the official identity color remains an accent.
        case WireStroke::Body: return kPencil;
    }
    return kPencil;
}

void drawPaperTexture(LGFX_Device& canvas)
{
    uint32_t state = 0x6d2b79f5u;
    for (int index = 0; index < 34; ++index) {
        state = state * 1664525u + 1013904223u;
        const int x = 38 + static_cast<int>((state >> 8u) % 390u);
        state = state * 1664525u + 1013904223u;
        const int y = 34 + static_cast<int>((state >> 8u) % 398u);
        canvas.drawLine(x, y, x + 2 + static_cast<int>(state & 3u), y, kPencilLight);
    }
}

void drawCar(LGFX_Device& canvas, const CarSpec& spec, const CarWireframe& mesh,
             int centerX, int centerY, float scale, float yaw, float wheelPhase)
{
    const float yawCosine = std::cos(yaw);
    const float yawSine = std::sin(yaw);
    const float wheelCosine = std::cos(wheelPhase);
    const float wheelSine = std::sin(wheelPhase);
    for (std::size_t index = 0; index < mesh.lineCount; ++index) {
        const WireLine& line = mesh.lines[index];
        const CarPoint from = animateWheelPoint(line.from, spec, line.stroke,
                                                wheelCosine, wheelSine);
        const CarPoint to = animateWheelPoint(line.to, spec, line.stroke,
                                              wheelCosine, wheelSine);
        const ScreenPoint a = project(from, yawCosine, yawSine, scale, centerX, centerY);
        const ScreenPoint b = project(to, yawCosine, yawSine, scale, centerX, centerY);
        canvas.drawLine(a.x, a.y, b.x, b.y, strokeColor(spec, line.stroke));
    }
}

void drawHeader(LGFX_Device& canvas, const char* title)
{
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextSize(1);
    canvas.setTextColor(kPencil, kPaper);
    canvas.drawString("LET'S & GO!!", canvas.width() / 2, 45);
    canvas.setTextColor(kPencilFaint, kPaper);
    canvas.drawString(title, canvas.width() / 2, 68);
}

bool drawWorldLine(LGFX_Device& canvas, const TrackCamera& camera,
                   TrackVec3 fromWorld, TrackVec3 toWorld, uint16_t color)
{
    TrackCameraPoint from = trackToCamera(camera, fromWorld);
    TrackCameraPoint to = trackToCamera(camera, toWorld);
    if (!clipTrackSegmentToNear(from, to)) return false;
    TrackScreenPoint a{};
    TrackScreenPoint b{};
    if (!projectTrackPoint(camera, from, a) || !projectTrackPoint(camera, to, b)) {
        return false;
    }
    constexpr float kGuard = 24.0f;
    if (!clipTrackSegmentToViewport(a, b, static_cast<float>(canvas.width()),
                                    static_cast<float>(canvas.height()), kGuard)) return false;
    canvas.drawLine(static_cast<int>(std::lround(a.x)), static_cast<int>(std::lround(a.y)),
                    static_cast<int>(std::lround(b.x)), static_cast<int>(std::lround(b.y)),
                    color);
    return true;
}

void drawMountains(LGFX_Device& canvas)
{
    constexpr int kHorizonY = 151;
    canvas.drawLine(36, kHorizonY, 430, kHorizonY, kPencilLight);
    for (int x = 38; x < 430; x += 34) {
        const int peak = kHorizonY - 10 - ((x * 17) % 27);
        canvas.drawLine(x - 34, kHorizonY, x, peak, kPencilLight);
        canvas.drawLine(x, peak, x + 35, kHorizonY, kPencilLight);
    }
}

void drawTrackLayer(LGFX_Device& canvas, const TrackPreviewGeometry& preview,
                    const TrackCamera& camera, TrackLayer requestedLayer)
{
    for (std::size_t index = 0; index < TrackPreviewGeometry::kSegments; ++index) {
        if (preview.layer[index] != requestedLayer) continue;
        const TrackVec3 left = preview.left[index];
        const TrackVec3 right = preview.right[index];
        const TrackVec3 nextLeft = preview.left[index + 1u];
        const TrackVec3 nextRight = preview.right[index + 1u];
        drawWorldLine(canvas, camera, left, nextLeft, kCourseEdge);
        drawWorldLine(canvas, camera, right, nextRight, kCourseEdge);
        if ((index & 1u) == 0u) {
            drawWorldLine(canvas, camera, left, right, kCourseFaint);
        }
        const TrackVec3 railLift{0.0f, 0.48f, 0.0f};
        drawWorldLine(canvas, camera, trackAdd(left, railLift),
                      trackAdd(nextLeft, railLift), kCourseEdge);
        drawWorldLine(canvas, camera, trackAdd(right, railLift),
                      trackAdd(nextRight, railLift), kCourseEdge);
        if ((index % 4u) == 0u) {
            drawWorldLine(canvas, camera, left, trackAdd(left, railLift), kCourseFaint);
            drawWorldLine(canvas, camera, right, trackAdd(right, railLift), kCourseFaint);
        }
    }
}

void drawTrackPreview(LGFX_Device& canvas, const TrackPreviewGeometry& preview,
                      uint32_t screenElapsedMs)
{
    const float orbit = static_cast<float>(screenElapsedMs) * 0.00016f;
    const TrackVec3 cameraPosition{std::sin(orbit) * 21.0f, 13.0f,
                                   -std::cos(orbit) * 21.0f};
    const TrackCamera camera = makeTrackLookAtCamera(cameraPosition,
                                                     {0.0f, 1.4f, 0.0f},
                                                     canvas.width(), canvas.height(), 0.69f);
    drawMountains(canvas);
    drawTrackLayer(canvas, preview, camera, TrackLayer::Lower);
    drawTrackLayer(canvas, preview, camera, TrackLayer::Transition);

    // The upper deck is deliberately last at the crossing: there is no center
    // divider, only the two external guard rails.
    drawTrackLayer(canvas, preview, camera, TrackLayer::Upper);
}

}  // namespace

void GarageRenderer::open(int width, int height)
{
    _width = width;
    _height = height;
    _meshCached = false;
    const float step = _track.length() /
                       static_cast<float>(TrackPreviewGeometry::kSegments);
    for (std::size_t index = 0; index <= TrackPreviewGeometry::kSegments; ++index) {
        const float distance = step * static_cast<float>(index);
        _trackPreview.left[index] = _track.edge(distance, -1.0f);
        _trackPreview.right[index] = _track.edge(distance, 1.0f);
        if (index < TrackPreviewGeometry::kSegments) {
            _trackPreview.layer[index] = _track.layer(distance + step * 0.5f);
        }
    }
}

void GarageRenderer::close()
{
    _width = 0;
    _height = 0;
    _meshCached = false;
}

const CarWireframe& GarageRenderer::showcaseMesh(CarId car)
{
    if (!_meshCached || car != _cachedCar) {
        _showcaseMesh = buildCarWireframe(car, CarLod::Showcase);
        _cachedCar = car;
        _meshCached = true;
    }
    return _showcaseMesh;
}

void GarageRenderer::render(const GameFlow& flow, const GarageSelection& selection,
                            uint32_t screenElapsedMs)
{
    if (_width <= 0 || _height <= 0) return;
    auto& canvas = GetHAL().getDisplay();
    canvas.fillScreen(kPaper);
    drawPaperTexture(canvas);

    const GameScreen screen = flow.screen();
    CarId visibleCar = flow.setup().playerCar;
    if (screen == GameScreen::CarSelect) visibleCar = selection.playerCursor();
    if (screen == GameScreen::RivalSelect && !selection.rivalCursorIsDone()) {
        visibleCar = selection.rivalCursorCar();
    }
    const CarSpec& spec = carSpec(visibleCar);
    const auto& mesh = showcaseMesh(visibleCar);
    const float seconds = static_cast<float>(screenElapsedMs) * 0.001f;

    if (screen == GameScreen::CarSelect) {
        drawHeader(canvas, "SELECT MACHINE");
        drawCar(canvas, spec, mesh, _width / 2, 267, 112.0f,
                -0.52f + std::sin(seconds * 0.7f) * 0.08f, seconds * 7.0f);
        canvas.setTextColor(kPencil, kPaper);
        canvas.setTextSize(2);
        canvas.drawString(spec.officialName, _width / 2, 365);
        char stats[48] = {};
        std::snprintf(stats, sizeof(stats), "SPD %02d  TURN %02d  GRIP %02d",
                      static_cast<int>(spec.performance.topSpeed * 99.0f),
                      static_cast<int>(spec.performance.steering * 99.0f),
                      static_cast<int>(spec.performance.stability * 99.0f));
        canvas.setTextSize(1);
        canvas.setTextColor(kPencilFaint, kPaper);
        canvas.drawString(stats, _width / 2, 398);
        canvas.drawString("A: CHANGE   B: SELECT", _width / 2, 425);
        return;
    }

    if (screen == GameScreen::CarShowcase) {
        drawHeader(canvas, "MACHINE READY");
        const float entrance = std::min(1.0f, static_cast<float>(screenElapsedMs) / 480.0f);
        const float scale = 108.0f + 30.0f * entrance;
        const float vibration = screenElapsedMs > 450u
                                    ? std::sin(seconds * 80.0f) * 1.4f
                                    : 0.0f;
        drawCar(canvas, spec, mesh, _width / 2 + static_cast<int>(vibration), 282,
                scale, -0.82f + entrance * 0.62f, seconds * 13.0f);
        if (screenElapsedMs > 520u) {
            for (int line = 0; line < 5; ++line) {
                const int y = 320 + line * 12;
                const int travel = static_cast<int>((screenElapsedMs / 5u + line * 37u) % 120u);
                canvas.drawLine(34 + travel, y, 82 + travel, y - 3, kPencilFaint);
            }
        }
        canvas.setTextSize(2);
        canvas.setTextColor(spec.accentColor, kPaper);
        canvas.drawString(spec.officialName, _width / 2, 397);
        return;
    }

    if (screen == GameScreen::RivalSelect) {
        drawHeader(canvas, "SELECT RIVALS  0-3");
        if (selection.rivalCursorIsDone()) {
            canvas.setTextColor(kPencil, kPaper);
            canvas.setTextSize(3);
            canvas.drawString("RACE READY", _width / 2, 230);
        } else {
            drawCar(canvas, spec, mesh, _width / 2, 260, 96.0f, -0.55f,
                    seconds * 5.0f);
            canvas.setTextColor(spec.accentColor, kPaper);
            canvas.setTextSize(2);
            canvas.drawString(spec.shortName, _width / 2, 355);
            canvas.setTextSize(1);
            canvas.setTextColor(kPencilFaint, kPaper);
            canvas.drawString(flow.setup().hasRival(visibleCar) ? "SELECTED" : "B: TOGGLE",
                              _width / 2, 382);
        }
        char count[24] = {};
        std::snprintf(count, sizeof(count), "RIVALS %u / 3",
                      static_cast<unsigned>(flow.setup().rivalCount()));
        canvas.setTextSize(1);
        canvas.setTextColor(kPencil, kPaper);
        canvas.drawString(count, _width / 2, 420);
        return;
    }

    if (screen == GameScreen::TrackSelect) {
        drawHeader(canvas, "SELECT COURSE");
        drawTrackPreview(canvas, _trackPreview, screenElapsedMs);
        canvas.setTextSize(2);
        canvas.setTextColor(kCourseEdge, kPaper);
        canvas.drawString(overpassTrackName(), _width / 2, 377);
        canvas.setTextSize(1);
        canvas.setTextColor(kPencilFaint, kPaper);
        canvas.drawString("3 LAPS  /  OPEN LANE", _width / 2, 405);
        canvas.drawString("B: START", _width / 2, 430);
        return;
    }

    drawHeader(canvas, gameScreenLabel(screen));
    canvas.setTextColor(kPencilFaint, kPaper);
    canvas.drawString("NEXT STAGE IN DEVELOPMENT", _width / 2, _height / 2);
}

}  // namespace lets_and_go
