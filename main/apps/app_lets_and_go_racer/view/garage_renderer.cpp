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
        case WireStroke::Mechanical: return spec.wheelColor;
        // The paper is the white body fill. A graphite outline stays readable for
        // all four liveries while the official identity color remains an accent.
        case WireStroke::Body: return spec.bodyColor == 0x2145u ? spec.bodyColor : kPencil;
    }
    return kPencil;
}

void drawPaperTexture(LGFX_Sprite& canvas, PencilDetail detail)
{
    uint32_t state = 0x6d2b79f5u;
    const int count = detail == PencilDetail::High ? 34
                      : detail == PencilDetail::Medium ? 22 : 12;
    for (int index = 0; index < count; ++index) {
        state = state * 1664525u + 1013904223u;
        const int x = 38 + static_cast<int>((state >> 8u) % 390u);
        state = state * 1664525u + 1013904223u;
        const int y = 34 + static_cast<int>((state >> 8u) % 398u);
        canvas.drawLine(x, y, x + 2 + static_cast<int>(state & 3u), y, kPencilLight);
    }
}

void drawCar(LGFX_Sprite& canvas, const CarSpec& spec, const CarWireframe& mesh,
             int centerX, int centerY, float scale, float yaw, float wheelPhase,
             PencilDetail detail)
{
    const float yawCosine = std::cos(yaw);
    const float yawSine = std::sin(yaw);
    const float wheelCosine = std::cos(wheelPhase);
    const float wheelSine = std::sin(wheelPhase);
    const auto triangle = [&](CarPoint a, CarPoint b, CarPoint c, uint16_t color) {
        const auto p = project(a, yawCosine, yawSine, scale, centerX, centerY);
        const auto q = project(b, yawCosine, yawSine, scale, centerX, centerY);
        const auto r = project(c, yawCosine, yawSine, scale, centerX, centerY);
        canvas.fillTriangle(p.x, p.y, q.x, q.y, r.x, r.y, color);
    };
    // Flat pigment washes under the wireframe retain the official white/blue,
    // white/red and dark/red liveries, rather than making every body paper-white.
    for (float axle : {spec.rearAxleZ, spec.frontAxleZ}) {
        for (float side : {-0.59f, 0.59f}) {
            for (int segment = 0; segment < 12; ++segment) {
                const float a = segment * 6.2831853f / 12.0f;
                const float b = (segment + 1) * 6.2831853f / 12.0f;
                for (int ring = 0; ring < 2; ++ring) {
                    const float radius = spec.wheelRadius * (ring == 0 ? 1.0f : 0.58f);
                    triangle({side, spec.wheelRadius, axle},
                        {side, spec.wheelRadius + std::sin(a) * radius, axle + std::cos(a) * radius},
                        {side, spec.wheelRadius + std::sin(b) * radius, axle + std::cos(b) * radius},
                        ring == 0 ? kPencil : spec.wheelColor);
                }
            }
        }
    }
    const uint16_t wash = spec.bodyColor == 0x2145u ? 0x5aebu : spec.bodyColor;
    for (std::size_t i = 1; i < spec.profile.size(); ++i) {
        const auto& a = spec.profile[i - 1u];
        const auto& b = spec.profile[i];
        const auto station = [](const CarProfileStation& p) {
            return std::array<CarPoint, 5>{{{-p.halfWidth, p.sillY, p.z},
                {-p.halfWidth * 0.72f, p.deckY, p.z}, {0, p.centerY, p.z},
                {p.halfWidth * 0.72f, p.deckY, p.z}, {p.halfWidth, p.sillY, p.z}}};
        };
        const auto from = station(a), to = station(b);
        for (std::size_t face = 0; face < 4u; ++face) {
            triangle(from[face], from[face + 1u], to[face + 1u], wash);
            triangle(from[face], to[face + 1u], to[face], wash);
        }
        // Central canopy and paired body streaks, anchored to the car geometry.
        for (float side : {-1.0f, 1.0f}) {
            const auto stripe = [side](const CarProfileStation& p, float t) {
                return CarPoint{side * p.halfWidth * 0.72f * t,
                    p.centerY + (p.deckY - p.centerY) * t, p.z};
            };
            triangle(stripe(a, 0.50f), stripe(a, 0.73f), stripe(b, 0.73f), spec.accentColor);
            triangle(stripe(a, 0.50f), stripe(b, 0.73f), stripe(b, 0.50f), spec.accentColor);
        }
    }
    for (std::size_t index = 0; index < mesh.lineCount; ++index) {
        const WireLine& line = mesh.lines[index];
        if (detail == PencilDetail::Low && line.stroke == WireStroke::Mechanical &&
            (index & 1u) != 0u) continue;
        const CarPoint from = animateWheelPoint(line.from, spec, line.stroke,
                                                wheelCosine, wheelSine);
        const CarPoint to = animateWheelPoint(line.to, spec, line.stroke,
                                              wheelCosine, wheelSine);
        const ScreenPoint a = project(from, yawCosine, yawSine, scale, centerX, centerY);
        const ScreenPoint b = project(to, yawCosine, yawSine, scale, centerX, centerY);
        const bool wheel = line.stroke == WireStroke::Mechanical &&
                           std::abs(std::abs(line.from.x) - 0.59f) < 0.02f;
        const uint16_t color = line.stroke == WireStroke::Mechanical && !wheel
                                   ? kPencil : strokeColor(spec, line.stroke);
        canvas.drawLine(a.x, a.y, b.x, b.y, color);
    }
}

void drawHeader(LGFX_Sprite& canvas, const char* title)
{
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextSize(1);
    canvas.setTextColor(kPencil, kPaper);
    canvas.drawString("LET'S & GO!!", canvas.width() / 2, 45);
    canvas.setTextColor(kPencilFaint, kPaper);
    canvas.drawString(title, canvas.width() / 2, 68);
}

void drawMountains(LGFX_Sprite& canvas)
{
    constexpr int kHorizonY = 151;
    canvas.drawLine(36, kHorizonY, 430, kHorizonY, kPencilLight);
    for (int x = 38; x < 430; x += 34) {
        const int peak = kHorizonY - 10 - ((x * 17) % 27);
        canvas.drawLine(x - 34, kHorizonY, x, peak, kPencilLight);
        canvas.drawLine(x, peak, x + 35, kHorizonY, kPencilLight);
    }
}

void drawTrackPreview(LGFX_Sprite& canvas, const PencilTrack& preview,
                      uint32_t screenElapsedMs, PencilDetail detail)
{
    const float orbit = static_cast<float>(screenElapsedMs) * 0.00016f;
    const TrackVec3 cameraPosition{std::sin(orbit) * 29.0f, 19.0f,
                                   -std::cos(orbit) * 29.0f};
    const TrackCamera camera = makeTrackLookAtCamera(cameraPosition,
                                                     {0.0f, 1.4f, 0.0f},
                                                     canvas.width(), canvas.height(), 0.69f);
    drawMountains(canvas);
    drawPencilTrack(canvas, camera, preview, detail);
}

}  // namespace

void GarageRenderer::open(int width, int height)
{
    _width = width;
    _height = height;
    _meshCached = false;
    _trackPreview.open(_track);
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
        const auto result = buildCarWireframeInto(car, CarLod::Showcase,
            _showcaseMesh.lines.data(), _showcaseMesh.lines.size());
        _showcaseMesh.lineCount = result.lineCount;
        _showcaseMesh.overflowed = result.overflowed;
        _cachedCar = car;
        _meshCached = true;
    }
    return _showcaseMesh;
}

void GarageRenderer::render(const GameFlow& flow, const GarageSelection& selection,
                            uint32_t screenElapsedMs, PencilDetail detail,
                            const RacerInputStatus& inputStatus)
{
    if (_width <= 0 || _height <= 0) return;
    auto& canvas = GetHAL().getCanvas();
    canvas.fillScreen(kPaper);
    drawPaperTexture(canvas, detail);

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
                -0.52f + std::sin(seconds * 0.7f) * 0.08f, seconds * 7.0f, detail);
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
        canvas.drawString("STICK: CHANGE  BLUE: SELECT", _width / 2, 425);
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
                scale, -0.82f + entrance * 0.62f, seconds * 13.0f, detail);
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
                    seconds * 5.0f, detail);
            canvas.setTextColor(spec.accentColor, kPaper);
            canvas.setTextSize(2);
            canvas.drawString(spec.shortName, _width / 2, 355);
            canvas.setTextSize(1);
            canvas.setTextColor(kPencilFaint, kPaper);
            canvas.drawString(flow.setup().hasRival(visibleCar) ? "SELECTED" : "BLUE: TOGGLE",
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
        drawTrackPreview(canvas, _trackPreview, screenElapsedMs, detail);
        canvas.setTextSize(2);
        canvas.setTextColor(kCourseEdge, kPaper);
        canvas.drawString(overpassTrackName(), _width / 2, 377);
        canvas.setTextSize(1);
        canvas.setTextColor(kPencilFaint, kPaper);
        canvas.drawString("3 LAPS  /  OPEN LANE", _width / 2, 405);
        canvas.drawString("BLUE: START", _width / 2, 430);
        return;
    }

    drawHeader(canvas, gameScreenLabel(screen));
    canvas.setTextColor(kPencilFaint, kPaper);
    canvas.setTextSize(2);
    canvas.setTextColor(kPencil, kPaper);
    canvas.drawString(screen == GameScreen::InputCalibration ? "CENTER THE STICK" : "CONNECT CONTROLS",
                      _width / 2, 184);
    canvas.setTextSize(1);
    canvas.drawString(inputStatus.axesConnected ? "JOYSTICK2  CONNECTED" : "JOYSTICK2  WAITING",
                      _width / 2, 238);
    canvas.drawString(inputStatus.actionsConfigured ? "DUAL BUTTON  CONFIGURED" : "DUAL BUTTON  CHECK SETTINGS",
                      _width / 2, 266);
    if (screen == GameScreen::InputCalibration) {
        const float progress = std::isfinite(inputStatus.calibrationProgress)
                                   ? std::clamp(inputStatus.calibrationProgress, 0.0f, 1.0f) : 0.0f;
        canvas.drawRect(143, 298, 180, 12, kPencilFaint);
        canvas.fillRect(145, 300, static_cast<int>(176 * progress), 8, kCourseEdge);
    }
    canvas.setTextColor(kPencilFaint, kPaper);
    canvas.drawString(inputStatus.readiness == RacerInputReadiness::Fault
                          ? "INPUT FAULT - CHECK CABLE" : "CONTINUES WHEN READY", _width / 2, 345);
    canvas.drawString("HOLD BOTH BUTTONS TO EXIT", _width / 2, 405);
}

}  // namespace lets_and_go
