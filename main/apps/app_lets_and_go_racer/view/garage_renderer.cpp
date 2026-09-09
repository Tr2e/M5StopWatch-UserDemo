#include "garage_renderer.h"
#include "track_projection.h"
#include "garage_car_transform.h"
#include "home_theme.h"

#include <hal/hal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#include <mooncake_log.h>
#endif

namespace lets_and_go {
namespace {

void drawCar(LGFX_Sprite& canvas,const CarSpec& spec,const CarDisplayMesh& mesh,
             CarSurfaceRaster<352,288>& raster,
             int centerX,int centerY,float scale,float yaw,float wheelPhase,PencilDetail,
             float pitch=.5713375f,bool pitTheme=false)
{
    TrackCamera camera{};
    camera.principalX=centerX;camera.principalY=centerY;camera.focalLength=scale*5.8f;
    raster.begin(centerX-176,centerY-162);
    const GarageCarTransform transform(spec,yaw,pitch,wheelPhase);
#ifdef ESP_PLATFORM
    const uint64_t startUs=esp_timer_get_time();
#endif
    for(int ring=0;ring<2;++ring) for(int i=0;i<24;++i) {
        const float a=i*6.2831853f/24,b=(i+1)*6.2831853f/24;
        const float radius=ring==0 ? 1.f : .82f;
        TrackScreenPoint center{},p{},q{};
        projectTrackPoint(camera,transform({.025f,-.02f,0},0),center);
        projectTrackPoint(camera,transform({.025f+.56f*radius*std::cos(a),-.02f,.86f*radius*std::sin(a)},0),p);
        projectTrackPoint(camera,transform({.025f+.56f*radius*std::cos(b),-.02f,.86f*radius*std::sin(b)},0),q);
        canvas.fillTriangle(std::lround(center.x),std::lround(center.y),std::lround(p.x),
                            std::lround(p.y),std::lround(q.x),std::lround(q.y),pitTheme ? (ring==0 ? home_theme::panel : uint16_t(0x0842)) :
                            (ring==0 ? uint16_t(0xdeb8) : uint16_t(0xceb7)));
    }
#ifdef ESP_PLATFORM
    const uint64_t shadowUs=esp_timer_get_time();
#endif
    for(std::size_t i=0;i<mesh.count;++i)raster.panel(camera,mesh.panels[i],transform);
#ifdef ESP_PLATFORM
    const uint64_t meshUs=esp_timer_get_time();
#endif
    raster.blit(canvas);
#ifdef ESP_PLATFORM
    const uint64_t endUs=esp_timer_get_time();
    static uint64_t lastLogUs=0;
    if(endUs-lastLogUs>=2000000u) {
        lastLogUs=endUs;
        mclog::tagInfo("GarageStage","panels={} shadow_us={} raster_us={} blit_us={}",
                       mesh.count,uint32_t(shadowUs-startUs),uint32_t(meshUs-shadowUs),uint32_t(endUs-meshUs));
    }
#endif
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
    if(_surface)_surface->trackSurfaces.preferInternalMemory();
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
                            const RacerInputStatus& inputStatus,const GarageViewState& view, bool deviceControls)
{
    if (_width <= 0 || _height <= 0) return;
    auto& canvas = GetHAL().getCanvas();
    const GameScreen screen = flow.screen();
    if(screen==GameScreen::InputCheck || screen==GameScreen::InputCalibration) {
        home_theme::entry(canvas,screen==GameScreen::InputCalibration,inputStatus);
        return;
    }
    home_theme::backdrop(canvas);
    if(!_surface) {
        home_theme::label(canvas,"GARAGE UNAVAILABLE",_width/2,210,2);
        home_theme::label(canvas,"HOLD A+B TO EXIT",_width/2,250,1,home_theme::muted);
        return;
    }

    CarId visibleCar = flow.setup().playerCar;
    if (screen == GameScreen::CarSelect) visibleCar = selection.playerCursor();
    if (screen == GameScreen::RivalSelect && !selection.rivalCursorIsDone()) {
        visibleCar = selection.rivalCursorCar();
    }
    const CarSpec& spec = carSpec(visibleCar);
    const auto mesh = [&]() -> const CarDisplayMesh& { return showcaseMesh(visibleCar,detail); };

    if (screen == GameScreen::CarSelect) {
        using namespace home_theme;
        label(canvas,"LET'S & GO!!",_width/2,43,2);
        char title[32];
        std::snprintf(title,sizeof(title),"MACHINE %02u / %02u",unsigned(visibleCar)+1,unsigned(kCarCount));
        label(canvas,title,_width/2,70,2);
        canvas.drawLine(123,63,116,70,muted);canvas.drawLine(116,70,123,77,muted);
        char viewHint[40];
        std::snprintf(viewHint,sizeof(viewHint),deviceControls ? "%s  / TAP VIEW" : "%s  / U-D VIEW",
                      garageViewLabel(view.preset));
        const auto viewButton=home_layout::viewAction;
        canvas.fillRect(viewButton.x,viewButton.y,viewButton.width,viewButton.height,panel);
        label(canvas,viewHint,_width/2,96,1,muted,panel);
        drawCar(canvas, spec, mesh(), _surface->raster, _width / 2+std::lround(view.carSlide),
                std::lround(view.centerY),view.scale*view.carZoom,
                view.yaw,view.wheelPhase,detail,view.pitch,true);
        label(canvas,spec.shortName,_width/2,366,3);
        arrows(canvas,366);
        char stats[48];
        std::snprintf(stats,sizeof(stats),"SPD %02d  TURN %02d  GRIP %02d",
            int(spec.performance.topSpeed*99.f),int(spec.performance.steering*99.f),
            int(spec.performance.stability*99.f));
        label(canvas,stats,_width/2,391,1,muted);
        action(canvas,home_layout::carSelect,deviceControls ? "SELECT" : "BLUE: SELECT");
        label(canvas,deviceControls ? "A NEXT / B SELECT" : "L/R: MACHINE",_width/2,446,1,muted);
        return;
    }

    if (screen == GameScreen::CarShowcase) {
        using namespace home_theme;
        setupHeader(canvas,"MACHINE CONFIRMED",false);
        const float entrance = std::min(1.0f, static_cast<float>(screenElapsedMs) / 480.0f);
        const float scale = view.scale*(1.f+.06f*entrance);
        drawCar(canvas, spec, mesh(), _surface->raster, _width / 2,
                std::lround(view.centerY+12.f*entrance),scale,view.yaw,view.wheelPhase,detail,view.pitch,true);
        label(canvas,spec.shortName,_width/2,385,2);
        label(canvas,"NEXT: CHOOSE RIVALS",_width/2,416,1,muted);
        return;
    }

    if (screen == GameScreen::RivalSelect) {
        using namespace home_theme;
        setupHeader(canvas,"02 / 03  RIVALS");
        const unsigned count=flow.setup().rivalCount();
        char status[32];
        std::snprintf(status,sizeof(status),"%u / %u RIVALS SELECTED",count,unsigned(kMaximumRivals));
        label(canvas,status,_width/2,95,1,muted);
        unsigned slot=0;
        for(unsigned car=0;car<kCarCount && slot<kMaximumRivals;++car) {
            if(!flow.setup().hasRival(CarId(car)))continue;
            const int x=100+140*slot++;
            canvas.fillRect(x,111,128,22,panel);
            canvas.fillRect(x,111,3,22,blue);
            label(canvas,carSpec(CarId(car)).shortName,x+64,122,1,white,panel);
        }
        for(;slot<kMaximumRivals;++slot) {
            const int x=100+140*slot;
            canvas.fillRect(x,111,128,22,panel);
            label(canvas,"EMPTY",x+64,122,1,muted,panel);
        }
        if (selection.rivalCursorIsDone()) {
            label(canvas,count ? "GRID READY" : "SOLO RUN",_width/2,224,3);
            std::snprintf(status,sizeof(status),"%u MACHINE%s / 3 LAPS",count+1,count ? "S" : "");
            label(canvas,status,_width/2,265,1,muted);
            label(canvas,"CHOOSE YOUR COURSE",_width/2,313,1,muted);
            arrows(canvas,331);
            label(canvas,deviceControls ? "B: NEXT" : "BLUE: NEXT",_width/2,364,2,blue);
        } else {
            drawCar(canvas,spec,mesh(),_surface->raster,_width/2,238,112.f,-.65f,0.f,detail,.5713375f,true);
            label(canvas,spec.shortName,_width/2,331,2);
            arrows(canvas,331);
            const bool selected=flow.setup().hasRival(visibleCar);
            const bool full=count>=kMaximumRivals;
            action(canvas,home_layout::rivalToggle,selected ? "REMOVE" : full ? "GRID FULL" : "ADD RIVAL",
                   nullptr,selected ? blue : full ? line : red);
        }
        label(canvas,selection.rivalCursorIsDone() ? "CHANGE RIVALS OR CONTINUE" :
                     count>=kMaximumRivals ? "REMOVE A RIVAL TO REPLACE" :
                     count ? "ADD ANOTHER OR CONTINUE" : "NO RIVALS = SOLO RUN",_width/2,386,1,muted);
        action(canvas,home_layout::setupNext,deviceControls ? "NEXT: TRACK" :
               selection.rivalCursorIsDone() ? "BLUE: NEXT" : "READY >",nullptr,
               deviceControls || selection.rivalCursorIsDone() ? red : panel);
        label(canvas,deviceControls ? (selection.rivalCursorIsDone() ? "A BROWSE / B NEXT" : "A NEXT / B TOGGLE") :
              "L/R / BLUE SELECT",_width/2,445,1,muted);
        return;
    }

    if (screen == GameScreen::TrackSelect) {
        if (_track.id() != flow.setup().track) {
            _track.select(flow.setup().track);
            _trackPreview.open(_track);
        }
        drawTrackPreview(canvas, _trackPreview, screenElapsedMs, detail,_surface->trackSurfaces,_track.id());
        using namespace home_theme;
        setupHeader(canvas,"03 / 03  COURSE",true,track_paint::night);
        label(canvas,overpassTrackName(_track.id()),_width/2,358,2,white,track_paint::floor);
        arrows(canvas,358);
        label(canvas,_track.id()==TrackId::TriCross ? "2/2  3 CROSSINGS  3 LAPS" : "1/2  1 CROSSING  3 LAPS",
              _width/2,384,1,muted,track_paint::floor);
        action(canvas,home_layout::setupNext,"START RACE");
        label(canvas,deviceControls ? "A COURSE / B START" : "L/R COURSE / BLUE GO",
              _width/2,445,1,muted,track_paint::floor);
        return;
    }

    home_theme::setupHeader(canvas,gameScreenLabel(screen),false);
}

}  // namespace lets_and_go
