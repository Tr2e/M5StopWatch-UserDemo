#include "race_renderer.h"

#include "track_projection.h"
#include "home_theme.h"
#include "../controller/race_ui_layout.h"

#include <hal/hal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <new>
#ifdef ESP_PLATFORM
#include <esp_timer.h>
#include <mooncake_log.h>
#endif

namespace lets_and_go {
namespace {

constexpr uint16_t kWarning = 0xd945u;
// Panel_CO5300 exposes 468 x 466. Keep capacity independent of the older
// square host captures, and share it between the guard and the copy buffer.
constexpr int kUpscaleRowPixels = 480;
constexpr uint16_t kMapTransparent=0xf81fu;

void drawPanel(LGFX_Sprite& canvas,int x,int y,int width,int height,int radius,uint16_t color)
{
    canvas.fillRect(x+radius,y,width-2*radius,height,color);
    canvas.fillRect(x,y+radius,width,height-2*radius,color);
    for(int cx : {x+radius,x+width-radius-1})
        for(int cy : {y+radius,y+height-radius-1})canvas.fillCircle(cx,cy,radius,color);
}

struct CarPose {
    TrackVec3 base;
    TrackVec3 lateral;
    TrackVec3 forward;
    TrackVec3 up;
};

CarPose makeCarPose(const TrackFrame& frame, const RaceCarSnapshot& car)
{
    const float cosine = std::cos(car.motion.headingOffset);
    const float sine = std::sin(car.motion.headingOffset);
    CarPose pose;
    pose.base = trackAdd(frame.center,
                         trackScale(frame.lateral, car.motion.lateralOffset));
    pose.base.y += std::sin(frame.bankRadians) * car.motion.lateralOffset;
    const TrackVec3 right = trackNormalize(trackAdd(frame.lateral,
        {0.0f, std::sin(frame.bankRadians), 0.0f}));
    const TrackVec3 forward = trackNormalize(trackSubtract(frame.tangent,
        trackScale(right, trackDot(frame.tangent, right))));
    pose.up = trackNormalize(trackCross(forward, right));
    pose.lateral = trackAdd(trackScale(right, cosine), trackScale(forward, -sine));
    pose.forward = trackAdd(trackScale(forward, cosine), trackScale(right, sine));
    return pose;
}

TrackVec3 carPointToWorld(CarPoint point, const CarPose& pose)
{
    point=carPointInTrackBasis(point);
    constexpr float kCarWorldScale = 0.34f;
    TrackVec3 result = pose.base;
    result = trackAdd(result, trackScale(pose.lateral, point.x * kCarWorldScale));
    result = trackAdd(result, trackScale(pose.forward, point.z * kCarWorldScale));
    result = trackAdd(result, trackScale(pose.up, 0.015f + point.y * kCarWorldScale));
    return result;
}

void drawRaceCar(LGFX_Sprite& canvas,const TrackCamera& camera,
                 const OverpassTrack& track,const RaceCarSnapshot& car,
                 const RaceSurfaceMesh& mesh,CarSurfaceRaster<112,112>& raster,
                 const PencilOcclusion& occlusion,
                 std::array<PreparedCarPanel,RaceSurfaceMesh::kMaximumPanels+12u>& prepared,
                 std::array<RaceProjectedVertex,RaceSurfaceMesh::kMaximumVertices>& vertices,PencilDetail,
                 float occlusionScale=1.f)
{
#ifdef ESP_PLATFORM
    const uint64_t startUs=esp_timer_get_time();
    uint64_t rasterUs=0,blitUs=0;
    unsigned tiles=0;
#endif
    const auto frame=track.sample(car.motion.distance);
    const auto pose=makeCarPose(frame,car);
    const float phase=car.motion.distance/(.34f*kModelWheelRadius);
    const float cosine=std::cos(phase),sine=std::sin(phase);
    const auto transform=[&](CarPoint p,uint8_t wheel) {
        return trackToCamera(camera,carPointToWorld(animateCarPanelPoint(p,wheel,cosine,sine),pose));
    };
    // Adjacent panels share geometry, even where their paint/UVs differ.
    for(std::size_t i=0;i<mesh.vertexCount;++i) {
        const auto key=mesh.vertexCorner[i];const auto& face=mesh.panels[key/4];
        auto& vertex=vertices[i];
        vertex.camera=transform(face.point[key%4],face.wheel);
        vertex.inverse=vertex.camera.z>=kTrackNearPlane ? 1.f/vertex.camera.z : 0;
        if(vertex.inverse>0) {
            vertex.screen={camera.principalX+camera.focalLength*vertex.camera.x*vertex.inverse,
                           camera.principalY-camera.focalLength*vertex.camera.y*vertex.inverse};
        }
    }
    std::size_t count=0;
    float left=float(canvas.width()),right=0,top=float(canvas.height()),bottom=0;
    const auto append=[&](const PreparedCarPanel& panel) {
        if(!panel.visibility)return;
        left=std::min(left,panel.left);right=std::max(right,panel.right);
        top=std::min(top,panel.top);bottom=std::max(bottom,panel.bottom);
        ++count;
    };
    for(int i=0;i<12;++i) {
        const float a=i*6.2831853f/12,b=(i+1)*6.2831853f/12;
        CarPanel shadow;
        shadow.point={{{0,.001f,0},{.52f*std::cos(a),.001f,.82f*std::sin(a)},
                       {.52f*std::cos(b),.001f,.82f*std::sin(b)},{0,.001f,0}}};
        shadow.color=0x9cd3;
        prepareCarPanel(prepared[count],camera,shadow,transform);
        append(prepared[count]);
    }
    for(std::size_t i=0;i<mesh.count;++i) {
        auto& panel=prepared[count];const auto& face=mesh.panels[i];
        unsigned front=0;
        for(unsigned c=0;c<4;++c)if(vertices[mesh.cornerIndex[i*4+c]].inverse>0)++front;
        if(front==0)continue;
        if(front!=4) {
            unsigned corner=0;
            prepareCarPanel(panel,camera,face,[&](CarPoint,uint8_t) {
                return vertices[mesh.cornerIndex[i*4+corner++]].camera;
            });
        } else {
            panel.visibility=1;panel.color=face.color;panel.paint=face.paint;panel.light=face.light;
            panel.left=panel.top=1e20f;panel.right=panel.bottom=-1e20f;
            new(&panel.screen) decltype(panel.screen);
            for(unsigned c=0;c<4;++c) {
                const auto& v=vertices[mesh.cornerIndex[i*4+c]];
                panel.screen[c]={v.screen.x,v.screen.y,v.inverse,
                    ((c==0 || c==3 ? face.u0 : face.u1)/255.f)*v.inverse,
                    ((c<2 ? face.v0 : face.v1)/255.f)*v.inverse};
                panel.left=std::min(panel.left,v.screen.x);panel.right=std::max(panel.right,v.screen.x);
                panel.top=std::min(panel.top,v.screen.y);panel.bottom=std::max(panel.bottom,v.screen.y);
            }
        }
        append(panel);
    }
    if(!count || right<0 || bottom<0 || left>=canvas.width() || top>=canvas.height())return;
    const int x0=int(std::max(0.f,std::floor(left))),y0=int(std::max(0.f,std::floor(top)));
    const int x1=int(std::min(float(canvas.width()-1),std::ceil(right)));
    const int y1=int(std::min(float(canvas.height()-1),std::ceil(bottom)));
#ifdef ESP_PLATFORM
    const uint64_t prepareUs=esp_timer_get_time();
#endif
    for(int y=y0;y<=y1;y+=112) for(int x=x0;x<=x1;x+=112) {
#ifdef ESP_PLATFORM
        const uint64_t tileStartUs=esp_timer_get_time();
#endif
        raster.begin(x,y);
        for(std::size_t i=0;i<count;++i)raster.preparedPanel(camera,prepared[i]);
#ifdef ESP_PLATFORM
        const uint64_t tileDrawUs=esp_timer_get_time();
        rasterUs+=tileDrawUs-tileStartUs;
        ++tiles;
#endif
        raster.blit(canvas,&occlusion,occlusionScale);
#ifdef ESP_PLATFORM
        blitUs+=esp_timer_get_time()-tileDrawUs;
#endif
    }
#ifdef ESP_PLATFORM
    const uint64_t endUs=esp_timer_get_time();
    static uint64_t lastLogUs=0;
    if(endUs-lastLogUs>=2000000u) {
        lastLogUs=endUs;
        mclog::tagInfo("CarStage","car={} panels={} vertices={} tiles={} prepare_us={} raster_us={} blit_us={}",
            int(car.car),count,mesh.vertexCount,tiles,uint32_t(prepareUs-startUs),uint32_t(rasterUs),uint32_t(blitUs));
    }
#endif
}

void drawHud(LGFX_Sprite& canvas, const RaceSnapshot& race,
             const OverpassTrack& track,
             const TrackMiniMap& miniMap,LGFX_Sprite* mapImage,bool deviceControls)
{
#ifdef ESP_PLATFORM
    const uint64_t hudStartedUs=esp_timer_get_time();
#endif
    using namespace home_theme;
    drawPanel(canvas,106,35,253,39,12,background);
    const auto instruments=race_ui_layout::instruments;
    drawPanel(canvas,instruments.x,instruments.y,instruments.width,instruments.height,8,background);
    const RaceCarSnapshot& player = race.player();
    char value[32] = {};
    std::snprintf(value,sizeof(value),"LAP %u/3",unsigned(std::min<unsigned>(kRaceLapCount,unsigned(player.completedLaps)+1)));
    label(canvas,value,165,55,2);
    std::snprintf(value,sizeof(value),"P%u/%u",unsigned(player.position),unsigned(race.carCount));
    label(canvas,value,305,55,2);
    if(deviceControls) {
        canvas.fillRect(230,49,3,12,muted);canvas.fillRect(237,49,3,12,muted);
    } else canvas.drawLine(235,45,235,65,line);
    const float speed=std::isfinite(player.motion.speed) ? std::clamp(player.motion.speed*3.6f,0.f,999.f) : 0.f;
    std::snprintf(value,sizeof(value),"%03d km/h",int(std::lround(speed)));
    label(canvas,value,160,98,1);
    label(canvas,"BOOST",250,88,1,muted);
    canvas.fillRect(220,100,60,6,line);
    const float charge=std::isfinite(player.motion.boostCharge) ? std::clamp(player.motion.boostCharge,0.f,1.f) : 0.f;
    canvas.fillRect(220,100,int(60*charge),6,blue);

#ifdef ESP_PLATFORM
    const uint64_t mapStartedUs=esp_timer_get_time();
#endif
    if(mapImage) {
#ifdef ESP_PLATFORM
        canvas.pushImage(TrackMiniMap::centerX-TrackMiniMap::radius,
            TrackMiniMap::centerY-TrackMiniMap::radius,mapImage->width(),mapImage->height(),
            static_cast<const lgfx::swap565_t*>(mapImage->getBuffer()),lgfx::rgb565_t(kMapTransparent));
#else
        canvas.pushImage(TrackMiniMap::centerX-TrackMiniMap::radius,
            TrackMiniMap::centerY-TrackMiniMap::radius,mapImage->width(),mapImage->height(),
            static_cast<const uint16_t*>(mapImage->getBuffer()),kMapTransparent);
#endif
    } else miniMap.draw(canvas);
    // Rivals first, player last: even a full grid at the same pixel cannot hide
    // the player's contrasting locator. Projection is shared with the road.
    for(int pass=0;pass<2;++pass) for (std::size_t index = 0; index < race.carCount; ++index) {
        const RaceCarSnapshot& car = race.cars[index];
        if(!car.active || car.player!=(pass==1))continue;
        const TrackFrame marker = track.sample(car.motion.distance);
        const auto point=miniMap.project(marker.center);
        const int x=point.x,y=point.y;
        const uint16_t color = carSpec(car.car).accentColor;
        if (car.player) {
            canvas.fillCircle(x,y,4,track_paint::night);
            canvas.fillCircle(x, y, 3, color);
            canvas.drawCircle(x,y,3,track_paint::chalk);
            canvas.fillRect(x,y,1,1,track_paint::chalk);
        } else {
            canvas.fillCircle(x,y,2,track_paint::night);
            canvas.fillCircle(x,y,1,color);
        }
    }
#ifdef ESP_PLATFORM
    const uint64_t hudFinishedUs=esp_timer_get_time();
    static uint64_t lastHudLogUs=0;
    if(hudFinishedUs-lastHudLogUs>=2000000u) {
        lastHudLogUs=hudFinishedUs;
        mclog::tagInfo("HudStage","map_us={} total_us={} map_cached={}",
            uint32_t(hudFinishedUs-mapStartedUs),uint32_t(hudFinishedUs-hudStartedUs),bool(mapImage));
    }
#endif
}

void formatRaceTime(float seconds, char* output, std::size_t capacity)
{
    const uint32_t milliseconds = seconds > 0.0f
        ? static_cast<uint32_t>(seconds * 1000.0f + 0.5f) : 0u;
    const uint32_t minutes = milliseconds / 60000u;
    const uint32_t remainder = milliseconds % 60000u;
    std::snprintf(output, capacity, "%02u:%02u.%03u",
                  static_cast<unsigned>(minutes),
                  static_cast<unsigned>(remainder / 1000u),
                  static_cast<unsigned>(remainder % 1000u));
}

void drawResults(LGFX_Sprite& canvas, const RaceSnapshot& race,
                 const ResultsSelection& selection,bool deviceControls)
{
    using namespace home_theme;
    backdrop(canvas);const int cx=canvas.width()/2;
    const auto& player=race.player();
    label(canvas,"RACE COMPLETE",cx,46,1,muted);
    char value[32];
    std::snprintf(value,sizeof(value),"%u / %u",unsigned(player.position),unsigned(race.carCount));
    label(canvas,value,cx,100,4);
    label(canvas,race.carCount==1 ? "SOLO FINISH" : "FINISH POSITION",cx,137,1,muted);
    label(canvas,carSpec(player.car).shortName,cx,163,2);
    canvas.fillRect(106,187,254,46,home_theme::panel);
    label(canvas,"RACE TIME",cx,196,1,muted,home_theme::panel);
    formatRaceTime(player.finished ? player.finishSeconds : race.elapsedSeconds,value,sizeof(value));
    label(canvas,value,cx,216,2,white,home_theme::panel);
    canvas.fillRect(106,241,254,42,home_theme::panel);
    label(canvas,"BEST LAP",cx,250,1,muted,home_theme::panel);
    if(player.bestLapSeconds>0)formatRaceTime(player.bestLapSeconds,value,sizeof(value));
    else std::snprintf(value,sizeof(value),"--:--.---");
    label(canvas,value,cx,269,2,white,home_theme::panel);
    for(int index=0;index<int(ResultAction::Count);++index) {
        const auto actionId=static_cast<ResultAction>(index);
        action(canvas,race_ui_layout::resultRow(index),resultActionLabel(actionId),nullptr,
               actionId==selection.cursor() ? red : home_theme::panel);
    }
    label(canvas,deviceControls ? "A NEXT / B SELECT" : "L/R / BLUE SELECT",cx,445,1,muted);
}

// Fixed stroke geometry avoids font side bearings/baselines: the visible digit
// bounds are centred on the physical display, including the narrow digit 1.
void drawCountdown(LGFX_Sprite& canvas,uint32_t elapsed)
{
    using namespace home_theme;
    const int cx=canvas.width()/2,cy=canvas.height()/2;
    const unsigned count=3-std::min<uint32_t>(2u,elapsed/1000u);
    canvas.fillCircle(cx,cy,62,background);
    canvas.drawCircle(cx,cy,64,line);
    canvas.drawCircle(cx,cy,63,line);
    for(int i=0;i<3;++i) {
        canvas.fillCircle(cx+(i-1)*26,cy-87,7,i<int(4-count) ? red : background);
        canvas.drawCircle(cx+(i-1)*26,cy-87,8,line);
    }
    const auto horizontal=[&](int y) {
        canvas.fillRect(cx-20,y-4,41,9,white);
        canvas.fillCircle(cx-20,y,4,white);canvas.fillCircle(cx+20,y,4,white);
    };
    const auto vertical=[&](int x,int y) {
        canvas.fillRect(x-4,y-12,9,25,white);
        canvas.fillCircle(x,y-12,4,white);canvas.fillCircle(x,y+12,4,white);
    };
    if(count==1) {
        canvas.fillRect(cx-4,cy-32,9,65,white);
        canvas.fillCircle(cx,cy-32,4,white);canvas.fillCircle(cx,cy+32,4,white);
    } else {
        horizontal(cy-32);horizontal(cy);horizontal(cy+32);
        vertical(cx+20,cy-16);
        vertical(count==2 ? cx-20 : cx+20,cy+16);
    }
    drawPanel(canvas,cx-72,cy+73,144,30,8,background);
    label(canvas,"GET READY",cx,cy+88,2);
}

void controlHelp(LGFX_Sprite& canvas,bool deviceControls,int y)
{
    using namespace home_theme;
    drawPanel(canvas,103,y-17,260,38,8,background);
    label(canvas,deviceControls ? "A BRAKE / B BOOST" : "RED BRAKE / BLUE BOOST",canvas.width()/2,y-5,1);
    label(canvas,deviceControls ? "DRAG TO STEER / TOP TO PAUSE" : "JOYSTICK STEER / HOLD RED PAUSE",
          canvas.width()/2,y+10,1,muted);
}

}  // namespace

void RaceRenderer::open(int width, int height, bool halfResolution, bool raceCaches,bool playerQuality,
                        bool wireframeTrack)
{
    _playerQuality=playerQuality;
    _wireframeTrack=wireframeTrack;
    _width = width;
    _height = height;
    _scene.reset();
    _paintAtlas.reset();_miniMapImage.reset();
    if(raceCaches) {
        _paintAtlas.reset(new(std::nothrow) RacePaintAtlas);
        _miniMapImage.reset(new(std::nothrow) LGFX_Sprite);
        if(_miniMapImage) {
            _miniMapImage->setPsram(true);_miniMapImage->setColorDepth(16);
            constexpr int side=TrackMiniMap::radius*2+1;
            if(!_miniMapImage->createSprite(side,side))_miniMapImage.reset();
        }
    }
    // The fixed row buffer supports the device's native width. On allocation
    // failure keep the original renderer; HUD and controls always stay native.
    if (halfResolution && width > 0 && width <= kUpscaleRowPixels && height > 0 &&
        width % 2 == 0 && height % 2 == 0) {
        _scene.reset(new(std::nothrow) LGFX_Sprite);
        if (_scene) {
            _scene->setPsram(true);
            _scene->setColorDepth(16);
            if (!_scene->createSprite(width / 2, height / 2)) _scene.reset();
        }
    }
#ifdef ESP_PLATFORM
    mclog::tagInfo("RacerMemory", "scene={}x{} requested_half={}",
        _scene ? _scene->width() : width, _scene ? _scene->height() : height, halfResolution);
#endif
    _surface.reset(new(std::nothrow) RaceSurfaceCache);
    if(_surface) {
        const bool rows=_surface->occlusion.preferInternalMemory();
        const unsigned planes=_surface->raster.preferInternalMemory();
#ifdef ESP_PLATFORM
        mclog::tagInfo("RacerMemory","track_rows_internal={} car_planes_internal={} free_internal={} largest_internal={}",
            rows,planes,heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
#else
        (void)rows;(void)planes;
#endif
    }
    _cachedTrack = TrackId::Count;
}

void RaceRenderer::close()
{
    _width = 0;
    _height = 0;
    _surface.reset();
    _scene.reset();
    _paintAtlas.reset();_miniMapImage.reset();
}

void RaceRenderer::render(const GameFlow& flow, const RaceController& race,
                          const ResultsSelection& results,
                          uint32_t screenElapsedMs, bool pausedForInputLoss,
                          PencilDetail detail, bool deviceControls)
{
    if (_width <= 0 || _height <= 0 || !race.prepared()) return;
    auto& canvas = GetHAL().getCanvas();
    if(!_surface) {
        home_theme::backdrop(canvas);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextSize(1);canvas.setTextColor(kWarning,home_theme::background);
        canvas.drawString("RENDER MEMORY LOW",_width/2,220);
        canvas.drawString("HOLD BOTH BUTTONS TO EXIT",_width/2,244);
        return;
    }
    const auto surfaceDetail=detail==PencilDetail::Low ? CarSurfaceDetail::Low : CarSurfaceDetail::Medium;
    bool materialsChanged=false;
    {
        for(std::size_t i=0;i<race.snapshot().carCount;++i) {
            const auto id=race.snapshot().cars[i].car;
            auto& mesh=_surface->meshes[i];
            const auto meshDetail=_playerQuality ? (race.snapshot().cars[i].player ?
                CarSurfaceDetail::Medium : CarSurfaceDetail::Minimal) : surfaceDetail;
            if(mesh.detail==meshDetail && _surface->cars[i]==id)continue;
            materialsChanged=true;
            const auto built=buildCarSurfaceInto(id,mesh.panels.data(),
                                                 mesh.panels.size(),meshDetail);
            if(built.overflowed) {_surface.reset();return;}
            mesh.count=built.count;
            mesh.detail=meshDetail;
            mesh.indexVertices(_surface->vertexSlots);
            _surface->cars[i]=id;
        }
        // Inactive slots may retain another quality tier from an earlier race.
        // Invalidate them before a later solo -> full roster expansion.
        for(std::size_t i=race.snapshot().carCount;i<kMaximumRaceCars;++i) {
            materialsChanged|=_surface->cars[i]!=CarId::Count;
            _surface->cars[i]=CarId::Count;
        }
        _surface->detail=surfaceDetail;
    }
    if(_paintAtlas && materialsChanged) {
        _paintAtlas->clear();
        unsigned missed=0;
        for(std::size_t i=0;i<race.snapshot().carCount;++i) {
            if(_playerQuality && race.snapshot().cars[i].player)continue;
            const auto& mesh=_surface->meshes[i];
            for(std::size_t p=0;p<mesh.count;++p) {
                const auto& face=mesh.panels[p];
                if(!_paintAtlas->add(face.paint,face.color,face.light))++missed;
            }
        }
#ifdef ESP_PLATFORM
        mclog::tagInfo("RacerMaterial","texture_side={} entries={} fallback_panels={} map_cached={}",
            RacePaintAtlas::kSide,_paintAtlas->count(),missed,bool(_miniMapImage));
#else
        (void)missed;
#endif
    }
    _surface->raster.setPaintAtlas(_paintAtlas.get());
    if (flow.screen() == GameScreen::Results) {
        drawResults(canvas,race.snapshot(),results,deviceControls);
        return;
    }
    if (_cachedTrack != race.track().id()) {
        _trackGeometry.open(race.track());
        _miniMap.open(_trackGeometry);
        if(_miniMapImage) {
            _miniMapImage->fillScreen(kMapTransparent);
            _miniMap.draw(*_miniMapImage,TrackMiniMap::centerX-TrackMiniMap::radius,
                TrackMiniMap::centerY-TrackMiniMap::radius);
        }
        _cachedTrack = race.track().id();
    }
    auto& scene = _scene ? *_scene : canvas;
    track_paint::backdrop(scene,detail,false);
    const RaceSnapshot& snapshot = race.snapshot();
    const RaceCarSnapshot& player = snapshot.player();
    const TrackFrame playerFrame = race.track().sample(player.motion.distance);
    const TrackCamera camera=makeRacerChaseCamera(playerFrame,player.motion.lateralOffset,scene.width(),scene.height());
#ifdef ESP_PLATFORM
    const uint64_t trackStartedUs=esp_timer_get_time();
#endif
    drawPencilTrack(scene, camera, _trackGeometry, detail, &_surface->occlusion,!_playerQuality,
                    _wireframeTrack);
#ifdef ESP_PLATFORM
    const uint64_t trackFinishedUs=esp_timer_get_time();
#endif

    std::array<std::size_t, kMaximumRaceCars> order{};
    std::array<float, kMaximumRaceCars> depth{};
    for (std::size_t i = 0; i < snapshot.carCount; ++i) {
        order[i] = i;
        depth[i] = trackToCamera(camera, race.track().sample(snapshot.cars[i].motion.distance).center).z;
    }
    std::sort(order.begin(), order.begin() + snapshot.carCount,
              [&](std::size_t a, std::size_t b) { return depth[a] > depth[b]; });
    unsigned submitted=0;
#ifdef ESP_PLATFORM
    uint32_t playerUs=0,opponentsUs=0;
#endif
    const auto submit=[&](std::size_t index,LGFX_Sprite& target,const TrackCamera& view,float scale) {
        const auto& car=snapshot.cars[index];
        if(depth[index]<kTrackNearPlane || !car.active || (car.finished && !car.player))return;
        _surface->raster.setPaintAtlas(_playerQuality && car.player ? nullptr : _paintAtlas.get());
#ifdef ESP_PLATFORM
        const uint64_t beginUs=esp_timer_get_time();
#endif
        drawRaceCar(target,view,race.track(),car,_surface->meshes[index],_surface->raster,
            _surface->occlusion,_surface->preparedPanels,_surface->projectedVertices,detail,scale);
        ++submitted;
#ifdef ESP_PLATFORM
        const uint32_t elapsed=esp_timer_get_time()-beginUs;
        if(car.player)playerUs+=elapsed;else opponentsUs+=elapsed;
#endif
    };
    // Preserve the existing far-to-near car ordering across both resolutions.
    // Opponents behind the player in draw order stay in the small scene; after
    // upscaling, the player and any closer opponents use the native canvas.
    std::size_t nativeBegin=snapshot.carCount;
    for(std::size_t slot=0;slot<snapshot.carCount;++slot) {
        if(_playerQuality && _scene && snapshot.cars[order[slot]].player) {
            nativeBegin=slot;break;
        }
        submit(order[slot],scene,camera,1.f);
    }
#ifdef ESP_PLATFORM
    const uint64_t carsFinishedUs=esp_timer_get_time();
#endif
    if (_scene) {
        // Sprite storage is byte-swapped RGB565 on M5GFX. Preserve that type
        // so pushImage can copy complete rows without per-pixel conversion.
#ifdef ESP_PLATFORM
        using ScenePixel = lgfx::swap565_t;
#else
        using ScenePixel = uint16_t;
#endif
        const auto* source = static_cast<const ScenePixel*>(scene.getBuffer());
        std::array<ScenePixel,kUpscaleRowPixels> row;
        for (int y = 0; y < scene.height(); ++y) {
            for (int x = 0; x < scene.width(); ++x)
                row[x*2] = row[x*2+1] = source[y*scene.width()+x];
            canvas.pushImage(0,y*2,_width,1,row.data());
            canvas.pushImage(0,y*2+1,_width,1,row.data());
        }
    }
#ifdef ESP_PLATFORM
    const uint64_t upscaleFinishedUs=esp_timer_get_time();
#endif
    if(nativeBegin<snapshot.carCount) {
        const auto nativeCamera=makeRacerChaseCamera(playerFrame,player.motion.lateralOffset,_width,_height);
        for(std::size_t slot=nativeBegin;slot<snapshot.carCount;++slot)
            submit(order[slot],canvas,nativeCamera,float(scene.width())/_width);
    }
#ifdef ESP_PLATFORM
    const uint64_t nativeFinishedUs=esp_timer_get_time();
    static uint64_t lastLogUs=0;
    if(carsFinishedUs-lastLogUs>=2000000u) {
        lastLogUs=carsFinishedUs;
        mclog::tagInfo("RaceStage","track_us={} cars_us={} upscale_us={} scene_width={} surfaces={} cars={} track_id={} player_car={} submitted={} player_us={} opponents_us={} player_native={}",
            uint32_t(trackFinishedUs-trackStartedUs),uint32_t(carsFinishedUs-trackFinishedUs+nativeFinishedUs-upscaleFinishedUs),
            uint32_t(upscaleFinishedUs-carsFinishedUs),scene.width(),
            _surface->occlusion.count,snapshot.carCount,int(race.track().id()),int(snapshot.player().car),submitted,playerUs,opponentsUs,_playerQuality);
    }
#else
    (void)submitted;
#endif

    canvas.setTextDatum(textdatum_t::middle_center);
    if (player.motion.wallImpact > 0.05f) {
        canvas.drawCircle(_width / 2, _height / 2, 194, kWarning);
        canvas.drawCircle(_width / 2 + 2, _height / 2 - 1, 188, kWarning);
    }
    drawHud(canvas,snapshot,race.track(),_miniMap,_miniMapImage.get(),deviceControls);
    const GameScreen screen = flow.screen();
    if (screen == GameScreen::GridIntro) {
        drawPanel(canvas,91,192,284,52,12,track_paint::night);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(3);
        canvas.drawString(snapshot.carCount == 1u ? "SOLO RUN" : "CHASE THE PACK", _width / 2, 218);
        controlHelp(canvas,deviceControls,382);
    } else if (screen == GameScreen::Countdown) {
        drawCountdown(canvas,screenElapsedMs);
        controlHelp(canvas,deviceControls,382);
    } else if (screen == GameScreen::Paused) {
        drawPanel(canvas,100,185,266,81,12,track_paint::night);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(3);
        canvas.drawString(pausedForInputLoss ? "INPUT LOST" : "PAUSED", _width / 2, 213);
        canvas.setTextSize(1);
        canvas.drawString(deviceControls ? "B / TAP: CONTINUE" : "HOLD RED WHEN INPUT READY", _width / 2, 246);
        controlHelp(canvas,deviceControls,382);
    } else if (screen == GameScreen::Finish) {
        drawPanel(canvas,98,183,270,74,12,track_paint::night);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(5);
        canvas.drawString("FINISH!", _width / 2, 220);
    }

}

}  // namespace lets_and_go
