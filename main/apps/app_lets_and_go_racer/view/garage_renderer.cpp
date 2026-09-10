#include "garage_renderer.h"
#include "track_projection.h"
#include "garage_car_transform.h"
#include "home_theme.h"
#include "../controller/inspection_presentation.h"

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
             GarageSurfaceCache& surface,int centerX,int centerY,float scale,float yaw,
             float wheelPhase,PencilDetail,float pitch=.5713375f,bool pitTheme=false,
             int percent=100,bool inspectionPrepared=false,int displayPercent=100) {
#ifdef ESP_PLATFORM
    const uint64_t startedUs=esp_timer_get_time();
#endif
    auto& raster=surface.raster;
    TrackCamera camera{};
    camera.principalX=centerX;camera.principalY=centerY;camera.focalLength=scale*5.8f;
    const int displayWidth=(352*displayPercent+50)/100,displayHeight=(288*displayPercent+50)/100;
    const int originX=centerX-displayWidth/2,originY=centerY-(162*displayHeight+144)/288;
    const int rasterWidth=(352*percent+50)/100,rasterHeight=(288*percent+50)/100;
    if(percent==100)raster.begin(originX,originY);
    else raster.begin(0,0,rasterWidth,rasterHeight);
#ifdef ESP_PLATFORM
    const uint64_t clearedUs=esp_timer_get_time();
#endif
    const GarageCarTransform transform(spec,yaw,pitch,wheelPhase);
    camera.focalLength*=float(displayHeight)/288;
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

    camera.focalLength=scale*5.8f;
    float horizontalCorrection=1.f;
#ifdef ESP_PLATFORM
    const uint64_t shadowUs=esp_timer_get_time();
#endif
    if(percent!=100) {
        const float ratioX=float(rasterWidth)/352,ratioY=float(rasterHeight)/288;
        camera.principalX=176*ratioX;camera.principalY=162*ratioY;
        camera.focalLength*=ratioY;horizontalCorrection=ratioX/ratioY;
    }
    const auto rasterTransform=[&](CarPoint point,uint8_t wheel) {
        auto value=transform(point,wheel);value.x*=horizontalCorrection;return value;
    };
    if(inspectionPrepared)surface.inspection.project(camera,horizontalCorrection);
#ifdef ESP_PLATFORM
    const uint64_t projectedUs=esp_timer_get_time();
#endif
    for(std::size_t i=0;i<mesh.count;++i) {
        if(inspectionPrepared) {
            PreparedCarPanel panel;
            surface.inspection.panel(panel,camera,mesh.panels[i],i,rasterTransform);
            raster.preparedPanel(camera,panel);
        } else raster.panel(camera,mesh.panels[i],rasterTransform);
    }
#ifdef ESP_PLATFORM
    const uint64_t rasterUs=esp_timer_get_time();
#endif
    if(percent==100)raster.blit(canvas);
    else raster.blitScaled(canvas,originX,originY,displayWidth,displayHeight);
#ifdef ESP_PLATFORM
    const uint64_t endUs=esp_timer_get_time();
    static uint64_t lastLogUs=0;
    if(endUs-lastLogUs>=2000000u) {
        lastLogUs=endUs;
        mclog::tagInfo("GarageStage","panels={} vertices={} scale_pct={} clear_us={} shadow_us={} project_us={} raster_us={} blit_us={}",
            mesh.count,inspectionPrepared ? surface.inspection.count : 0,percent,
            uint32_t(clearedUs-startedUs),uint32_t(shadowUs-clearedUs),uint32_t(projectedUs-shadowUs),
            uint32_t(rasterUs-projectedUs),uint32_t(endUs-rasterUs));
    }
#endif
}

void drawTrackPreview(LGFX_Sprite& canvas, const PencilTrack& preview,
                      uint32_t screenElapsedMs, PencilDetail detail,PencilOcclusion& surfaces,
                      TrackId track,bool decorations)
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
    if(track==TrackId::GrandSpiral) {
        TrackVec3 low{1e6f,1e6f,1e6f},high{-1e6f,-1e6f,-1e6f};
        for(std::size_t i=0;i<=preview.count;++i)for(auto p:{preview.left[i],preview.right[i]}) {
            low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);low.z=std::min(low.z,p.z);
            high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);high.z=std::max(high.z,p.z);
        }
        const auto target=trackScale(trackAdd(low,high),.5f);
        const float radius=trackLength(trackSubtract(high,low))*.5f+1.f;
        const float range=radius*1.8f;
        const auto position=trackAdd(target,{std::sin(orbit)*range,range*.85f,-std::cos(orbit)*range});
        camera=makeTrackLookAtCamera(position,target,canvas.width(),canvas.height(),.81f);
        camera.principalY=canvas.height()*.43f;
    }
    track_paint::backdrop(canvas,detail);
    drawPencilTrackGround(canvas, camera, preview, detail);
    drawPencilTrack(canvas, camera, preview, detail,&surfaces,decorations);
}

}  // namespace

void GarageRenderer::open(int width, int height)
{
    _width = width;
    _height = height;
    _meshCached = false;
    _inspectionPercent = 100;
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
        _surface->inspection.indexed=false;
    }
    return _surface->mesh;
}

void GarageRenderer::render(const GameFlow& flow, const GarageSelection& selection,
                            uint32_t screenElapsedMs, PencilDetail detail,
                            const RacerInputStatus& inputStatus,const GarageViewState& view, bool deviceControls,
                            int inspectionPercent,bool reuseInspectionBackground,int inspectionDisplayPercent)
{
    if (_width <= 0 || _height <= 0) return;
    auto& canvas = GetHAL().getCanvas();
    const GameScreen screen = flow.screen();
    // Inspection is quality-first: no adaptive LOD or low-resolution paint atlas.
    if(screen==GameScreen::CarInspect)detail=PencilDetail::High;
    if(screen==GameScreen::InputCheck || screen==GameScreen::InputCalibration) {
        home_theme::entry(canvas,screen==GameScreen::InputCalibration,inputStatus);
        return;
    }
    // The caller may reuse only a previously presented inspector for the same
    // car. This rectangle contains all moving pixels and the model-name label,
    // while leaving the header, reset button and footer in the existing canvas.
    const bool reuse=screen==GameScreen::CarInspect && reuseInspectionBackground &&
        _surface && _meshCached && _cachedCar==selection.playerCursor() && _cachedDetail==PencilDetail::High;
    const bool reuseSelection=screen==GameScreen::CarSelect && _selectionOptimizations &&
        reuseInspectionBackground && _surface && _meshCached && _cachedCar==selection.playerCursor();
    if(reuseSelection) {
        const auto region=selectionRefreshRegion(_width);
        canvas.fillRect(region.x,region.y,region.width,region.height,home_theme::background);
    } else if(reuse)canvas.fillRect(_width/2-176,102,352,284,home_theme::background);
    else home_theme::backdrop(canvas);
    if(!_surface) {
        home_theme::label(canvas,"GARAGE UNAVAILABLE",_width/2,210,2);
        home_theme::label(canvas,"HOLD A+B TO EXIT",_width/2,250,1,home_theme::muted);
        return;
    }

    CarId visibleCar = flow.setup().playerCar;
    if (screen == GameScreen::CarSelect || screen == GameScreen::CarInspect) visibleCar = selection.playerCursor();
    if (screen == GameScreen::RivalSelect && !selection.rivalCursorIsDone()) {
        visibleCar = selection.rivalCursorCar();
    }
    const CarSpec& spec = carSpec(visibleCar);
    const auto mesh = [&]() -> const CarDisplayMesh& { return showcaseMesh(visibleCar,detail); };

    if (screen == GameScreen::CarInspect) {
        using namespace home_theme;
        if(!reuse) {
            setupHeader(canvas,"VIEW MACHINE");
            label(canvas,"HIGH DETAIL / DRAG OR STICK",_width/2,96,1,muted);
        }
        const auto& carMesh=mesh();
#ifdef ESP_PLATFORM
        const uint64_t fitStartedUs=esp_timer_get_time();
#endif
        const float scale=_surface->inspection.fit(spec,carMesh,view.yaw,view.pitch);
        _inspectionPercent=inspectionPercent==65 ? 65 : inspectionPercent==82 ? 82 : inspectionPercent==77 ? 77 : inspectionPercent==80 ? 80 : inspectionPercent==85 ? 85 :
            inspectionPercent==50 ? 50 : inspectionPercent==75 ? 75 : 100;
#ifdef ESP_PLATFORM
        const uint64_t fitFinishedUs=esp_timer_get_time();
#endif
        drawCar(canvas,spec,carMesh,*_surface,_width/2,245,scale,
                view.yaw,0,detail,view.pitch,true,_inspectionPercent,true,
                _inspectionPercent==100 ? 100 : std::clamp(inspectionDisplayPercent,85,100));
#ifdef ESP_PLATFORM
        const uint64_t modelFinishedUs=esp_timer_get_time();
        static uint64_t lastInspectionLogUs=0;
        if(modelFinishedUs-lastInspectionLogUs>=2000000u) {
            lastInspectionLogUs=modelFinishedUs;
            mclog::tagInfo("InspectionStage","car={} scale_pct={} fit_us={} model_us={}",
                spec.shortName,_inspectionPercent,uint32_t(fitFinishedUs-fitStartedUs),
                uint32_t(modelFinishedUs-fitFinishedUs));
        }
#endif
        label(canvas,spec.shortName,_width/2,376,2);
        if(!reuse) {
            action(canvas,home_layout::inspectReset,"RESET VIEW");
            label(canvas,deviceControls ? "A ROTATE / B RESET" : "RED BACK / BLUE RESET",_width/2,444,1,muted);
        }
        return;
    }

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
        label(canvas,viewHint,viewButton.x+viewButton.width/2,viewButton.y+viewButton.height/2,1,muted,panel);
        const auto inspectButton=home_layout::inspectAction;
        canvas.fillRect(inspectButton.x,inspectButton.y,inspectButton.width,inspectButton.height,panel);
        label(canvas,"VIEW CAR",inspectButton.x+inspectButton.width/2,inspectButton.y+inspectButton.height/2,1,white,panel);
        const auto& carMesh=mesh();
        if(_selectionOptimizations)
            _surface->inspection.prepare(spec,carMesh,view.yaw,view.pitch,view.wheelPhase);
        const int percent=_selectionOptimizations ?
            (inspectionPercent==65 ? 65 : inspectionPercent==85 ? 85 : 100) : 100;
        drawCar(canvas, spec, carMesh, *_surface, _width / 2+std::lround(view.carSlide),
                std::lround(view.centerY),view.scale*view.carZoom,
                view.yaw,view.wheelPhase,detail,view.pitch,true,percent,_selectionOptimizations);
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
        drawCar(canvas, spec, mesh(), *_surface, _width / 2,
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
            drawCar(canvas,spec,mesh(),*_surface,_width/2,238,112.f,-.65f,0.f,detail,.5713375f,true);
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
        drawTrackPreview(canvas, _trackPreview, screenElapsedMs, detail,_surface->trackSurfaces,_track.id(),
            _track.id()!=TrackId::GrandSpiral || _trackPreviewDecorations);
        using namespace home_theme;
        setupHeader(canvas,"03 / 03  COURSE",true,track_paint::night);
        label(canvas,overpassTrackName(_track.id()),_width/2,358,2,white,track_paint::floor);
        arrows(canvas,358);
        label(canvas,_track.id()==TrackId::GrandSpiral ? "3/3  SPIRAL + BRIDGES" : _track.id()==TrackId::TriCross ? "2/3  3 CROSSINGS  3 LAPS" : "1/3  1 CROSSING  3 LAPS",
              _width/2,384,1,muted,track_paint::floor);
        action(canvas,home_layout::setupNext,"START RACE");
        label(canvas,deviceControls ? "A COURSE / B START" : "L/R COURSE / BLUE GO",
              _width/2,445,1,muted,track_paint::floor);
        return;
    }

    home_theme::setupHeader(canvas,gameScreenLabel(screen),false);
}

}  // namespace lets_and_go
