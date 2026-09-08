#include "garage_renderer.h"
#include "track_projection.h"
#include "garage_car_transform.h"

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
             int centerX,int centerY,float scale,float yaw,float wheelPhase,PencilDetail,
             float pitch=.5713375f)
{
    TrackCamera camera{};
    camera.principalX=centerX;camera.principalY=centerY;camera.focalLength=scale*5.8f;
    raster.begin(centerX-176,centerY-162);
    const GarageCarTransform transform(spec,yaw,pitch,wheelPhase);
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

void drawTrackPreview(LGFX_Sprite& canvas, const PencilTrack& preview,
                      uint32_t screenElapsedMs, PencilDetail detail,PencilOcclusion& surfaces,
                      TrackId track)
{
    // A bounded three-quarter orbit keeps the bridge readable and the whole
    // course inside the round display, even when the selection page is idle.
    const float orbit = .55f + std::sin(static_cast<float>(screenElapsedMs)*.00016f)*.24f;
    const bool complex = track == TrackId::TriCross;
    const float distance = complex ? 36.f : 29.f;
    const TrackVec3 cameraPosition{std::sin(orbit) * distance, complex ? 28.f : 22.f,
                                   -std::cos(orbit) * distance};
    TrackCamera camera = makeTrackLookAtCamera(cameraPosition,
                                                     {0.0f, complex ? 2.0f : 1.4f, 0.0f},
                                                     canvas.width(), canvas.height(), 0.81f);
    camera.principalY-=18;
    track_paint::backdrop(canvas,detail);
    drawPencilTrackGround(canvas, camera, preview, detail);
    drawPencilTrack(canvas, camera, preview, detail,&surfaces);
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
                            const RacerInputStatus& inputStatus,const GarageViewState& view)
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
        canvas.setTextColor(kPencilFaint,kPaper);
        canvas.drawString(garageViewLabel(view.preset),_width/2,97);
        drawCar(canvas, spec, mesh, _surface->raster, _width / 2+std::lround(view.carSlide),
                std::lround(view.centerY),view.scale*view.carZoom,
                view.yaw,view.wheelPhase,detail,view.pitch);
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
        canvas.drawString("L/R: CAR   U/D: VIEW", _width / 2, 418);
        canvas.drawString("BLUE: SELECT", _width / 2, 438);
        return;
    }

    if (screen == GameScreen::CarShowcase) {
        drawHeader(canvas, "MACHINE READY");
        const float entrance = std::min(1.0f, static_cast<float>(screenElapsedMs) / 480.0f);
        const float scale = view.scale*(1.f+.06f*entrance);
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
        drawCar(canvas, spec, mesh, _surface->raster, _width / 2 + static_cast<int>(vibration),
                std::lround(view.centerY+12.f*entrance),scale,view.yaw,view.wheelPhase,detail,view.pitch);
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
                    0.f, detail);
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
        if (_track.id() != flow.setup().track) {
            _track.select(flow.setup().track);
            _trackPreview.open(_track);
        }
        drawTrackPreview(canvas, _trackPreview, screenElapsedMs, detail,_surface->trackSurfaces,_track.id());
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextSize(1);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.drawString("LET'S & GO!!",_width/2,45);
        canvas.setTextColor(0x9d36u,track_paint::night);
        canvas.drawString("SELECT COURSE",_width/2,68);
        for(int index=0;index<3;++index)
            canvas.fillRect(215+index*13,87,10,3,index==0 ? track_paint::coral :
                            index==1 ? track_paint::chalk : track_paint::blue);
        canvas.setTextSize(2);
        canvas.setTextColor(track_paint::chalk,track_paint::floor);
        canvas.drawString(overpassTrackName(_track.id()), _width / 2, 377);
        canvas.setTextSize(1);
        canvas.setTextColor(0x9d36u,track_paint::floor);
        canvas.drawString(_track.id()==TrackId::TriCross ? "2/2  3 CROSSINGS  3 LAPS" : "1/2  1 CROSSING  3 LAPS", _width / 2, 405);
        canvas.drawString("L/R: COURSE  BLUE: START", _width / 2, 430);
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
