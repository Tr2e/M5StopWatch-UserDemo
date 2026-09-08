#include "garage_renderer.h"
#include "track_projection.h"

#include <hal/hal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace lets_and_go {
namespace {

constexpr uint16_t kPaper = 0xef3au;
constexpr uint16_t kPencil = 0x4269u;
constexpr uint16_t kPencilFaint = 0x9cd3u;
constexpr uint16_t kPencilLight = 0xc638u;
constexpr uint16_t kCourseEdge = 0x31e8u;


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

void drawCar(LGFX_Sprite& canvas,const CarSpec& spec,const CarDisplayMesh& mesh,
             CarSurfaceRaster<352,288>& raster,
             int centerX,int centerY,float scale,float yaw,float wheelPhase,PencilDetail)
{
    TrackCamera camera{};
    camera.principalX=centerX;camera.principalY=centerY;camera.focalLength=scale*5.8f;
    raster.begin(centerX-176,centerY-162);
    const float cosine=std::cos(yaw),sine=std::sin(yaw);
    const float wheelCosine=std::cos(wheelPhase),wheelSine=std::sin(wheelPhase);
    const float widthScale=(float(spec.dimensions.width)/spec.dimensions.length)/(97.f/155.f);
    const auto transform=[&](CarPoint p,uint8_t wheel) {
        p=animateCarPanelPoint(p,wheel,wheelCosine,wheelSine);
        p.x*=widthScale;
        const float x=p.x*cosine+p.z*sine,z=p.z*cosine-p.x*sine;
        return TrackCameraPoint{x,p.y*.84f-z*.54f,5.8f-(z*.84f+p.y*.54f)};
    };
    for(int ring=0;ring<2;++ring) for(int i=0;i<24;++i) {
        const float a=i*6.2831853f/24,b=(i+1)*6.2831853f/24;
        const float radius=ring==0 ? 1.f : .82f;
        TrackScreenPoint center{},p{},q{};
        projectTrackPoint(camera,transform({.025f,-.02f,0},0),center);
        projectTrackPoint(camera,transform({.025f+.56f*radius*std::cos(a),-.02f,.86f*radius*std::sin(a)},0),p);
        projectTrackPoint(camera,transform({.025f+.56f*radius*std::cos(b),-.02f,.86f*radius*std::sin(b)},0),q);
        canvas.fillTriangle(std::lround(center.x),std::lround(center.y),std::lround(p.x),
                            std::lround(p.y),std::lround(q.x),std::lround(q.y),ring==0 ? 0xdeb8 : 0xceb7);
    }
    for(std::size_t i=0;i<mesh.count;++i)raster.panel(camera,mesh.panels[i],transform);
    raster.blit(canvas);
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
    // A bounded three-quarter orbit keeps the bridge readable and the whole
    // course inside the round display, even when the selection page is idle.
    const float orbit = .55f + std::sin(static_cast<float>(screenElapsedMs)*.00016f)*.24f;
    const TrackVec3 cameraPosition{std::sin(orbit) * 29.0f, 22.0f,
                                   -std::cos(orbit) * 29.0f};
    const TrackCamera camera = makeTrackLookAtCamera(cameraPosition,
                                                     {0.0f, 1.4f, 0.0f},
                                                     canvas.width(), canvas.height(), 0.72f);
    drawMountains(canvas);
    drawPencilTrackGround(canvas, camera, preview, detail);
    drawPencilTrack(canvas, camera, preview, detail);
}

}  // namespace

void GarageRenderer::open(int width, int height)
{
    _width = width;
    _height = height;
    _meshCached = false;
    _surface.reset(new(std::nothrow) GarageSurfaceCache);
    _trackPreview.open(_track);
}

void GarageRenderer::close()
{
    _width = 0;
    _height = 0;
    _meshCached = false;
    _surface.reset();
}

const CarDisplayMesh& GarageRenderer::showcaseMesh(CarId car, PencilDetail detail)
{
    if (!_meshCached || car != _cachedCar || detail != _cachedDetail) {
        buildCarDisplayMesh(car, _surface->mesh,
            detail==PencilDetail::High ? CarSurfaceDetail::High :
            detail==PencilDetail::Medium ? CarSurfaceDetail::Medium : CarSurfaceDetail::Low);
        _cachedCar = car;
        _cachedDetail = detail;
        _meshCached = true;
    }
    return _surface->mesh;
}

void GarageRenderer::render(const GameFlow& flow, const GarageSelection& selection,
                            uint32_t screenElapsedMs, PencilDetail detail,
                            const RacerInputStatus& inputStatus)
{
    if (_width <= 0 || _height <= 0) return;
    auto& canvas = GetHAL().getCanvas();
    canvas.fillScreen(kPaper);
    drawPaperTexture(canvas, detail);
    if(!_surface) {
        drawHeader(canvas,"RENDER MEMORY LOW");
        canvas.drawString("HOLD BOTH BUTTONS TO EXIT",_width/2,233);
        return;
    }

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
        drawCar(canvas, spec, mesh, _surface->raster, _width / 2, 268, 146.0f,
                -0.65f + std::sin(seconds * 0.7f) * 0.06f, seconds * 7.0f, detail);
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
        const float scale = 146.0f + 9.0f * entrance;
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
        drawCar(canvas, spec, mesh, _surface->raster, _width / 2 + static_cast<int>(vibration), 282,
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
            drawCar(canvas, spec, mesh, _surface->raster, _width / 2, 270, 137.0f, -0.65f,
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
