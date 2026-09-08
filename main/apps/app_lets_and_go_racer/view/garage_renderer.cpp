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

struct ScreenPoint { int16_t x, y; };

struct CarCamera {
    float cosine, sine, scale, widthScale;
    int x, y;
    CarPoint rotate(CarPoint p) const {
        p.x *= widthScale;
        return {p.x*cosine+p.z*sine, p.y, p.z*cosine-p.x*sine};
    }
    float depth(CarPoint p) const {
        p = rotate(p);
        return p.z*.84f+p.y*.54f;
    }
    ScreenPoint project(CarPoint p) const {
        p = rotate(p);
        const float perspective = 4.8f/(4.8f-(p.z*.84f+p.y*.54f)*.35f);
        return {static_cast<int16_t>(std::lround(x+p.x*scale*perspective)),
            static_cast<int16_t>(std::lround(y+(p.z*.54f-p.y*.84f)*scale*perspective))};
    }
};

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

void drawCar(LGFX_Sprite& canvas, const CarSpec& spec, const CarDisplayMesh& mesh,
             std::array<CarPanelOrder, CarDisplayMesh::kMaximumPanels>& order,
             int centerX, int centerY, float scale, float yaw, float wheelPhase,
             PencilDetail detail)
{
    const CarCamera camera{std::cos(yaw),std::sin(yaw),scale,
        (float(spec.dimensions.width)/spec.dimensions.length)/(97.0f/155.0f),
        centerX,centerY};
    const float cosine=std::cos(wheelPhase), sine=std::sin(wheelPhase);
    // Paint individual faces far-to-near. Outlines belong to the face, so the
    // near cowl hides the far tire, chassis and wing support instead of X-rays.
    for (std::size_t i=0;i<mesh.count;++i) {
        float depth=0;
        const auto& face=mesh.panels[i].parent<mesh.count
                             ? mesh.panels[mesh.panels[i].parent] : mesh.panels[i];
        for (auto p : face.point)
            depth += camera.depth(animateCarPanelPoint(p,face.wheel,cosine,sine));
        order[i]={depth*.25f,static_cast<uint16_t>(i)};
    }
    std::sort(order.begin(),order.begin()+mesh.count,
        [](const CarPanelOrder& a,const CarPanelOrder& b) {
            return a.depth==b.depth ? a.index<b.index : a.depth<b.depth;
        });
    for (std::size_t i=0;i<mesh.count;++i) {
        const auto& panel=mesh.panels[order[i].index];
        std::array<ScreenPoint,4> p;
        for (std::size_t v=0;v<4;++v)
            p[v]=camera.project(animateCarPanelPoint(panel.point[v],panel.wheel,cosine,sine));
        canvas.fillTriangle(p[0].x,p[0].y,p[1].x,p[1].y,p[2].x,p[2].y,panel.color);
        canvas.fillTriangle(p[0].x,p[0].y,p[2].x,p[2].y,p[3].x,p[3].y,panel.color);
        for (std::size_t edge=0;edge<4;++edge) {
            if (!(panel.edges & (1u<<edge))) continue;
            const auto a=p[edge], b=p[(edge+1)%4];
            const int minimumLength=detail==PencilDetail::Low ? 10 :
                                    detail==PencilDetail::Medium ? 4 : 0;
            if(std::abs(a.x-b.x)+std::abs(a.y-b.y)<minimumLength) continue;
            canvas.drawLine(a.x,a.y,b.x,b.y,kPencil);
        }
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

const CarDisplayMesh& GarageRenderer::showcaseMesh(CarId car, PencilDetail detail)
{
    if (!_meshCached || car != _cachedCar || detail != _cachedDetail) {
        buildCarDisplayMesh(car, _showcaseMesh,
            detail==PencilDetail::High ? CarSurfaceDetail::High :
            detail==PencilDetail::Medium ? CarSurfaceDetail::Medium : CarSurfaceDetail::Low);
        _cachedCar = car;
        _cachedDetail = detail;
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
    const auto& mesh = showcaseMesh(visibleCar,detail);
    const float seconds = static_cast<float>(screenElapsedMs) * 0.001f;

    if (screen == GameScreen::CarSelect) {
        drawHeader(canvas, "SELECT MACHINE");
        drawCar(canvas, spec, mesh, _panelOrder, _width / 2, 273, 151.0f,
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
        const float scale = 151.0f + 16.0f * entrance;
        const float vibration = screenElapsedMs > 450u
                                    ? std::sin(seconds * 80.0f) * 1.4f
                                    : 0.0f;
        if (screenElapsedMs > 520u) {
            for (int line = 0; line < 5; ++line) {
                const int y = 320 + line * 12;
                const int travel = static_cast<int>((screenElapsedMs / 5u + line * 37u) % 120u);
                canvas.drawLine(34 + travel, y, 82 + travel, y - 3, kPencilFaint);
            }
        }
        drawCar(canvas, spec, mesh, _panelOrder, _width / 2 + static_cast<int>(vibration), 282,
                scale, -0.82f + entrance * 0.20f, seconds * 13.0f, detail);
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
            drawCar(canvas, spec, mesh, _panelOrder, _width / 2, 270, 140.0f, -0.65f,
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
