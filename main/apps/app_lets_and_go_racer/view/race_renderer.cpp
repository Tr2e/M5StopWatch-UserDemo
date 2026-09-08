#include "race_renderer.h"

#include "track_projection.h"

#include <hal/hal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <new>

namespace lets_and_go {
namespace {

constexpr uint16_t kPaper = 0xef3au;
constexpr uint16_t kPencil = 0x4269u;
constexpr uint16_t kFaint = 0x9cd3u;
constexpr uint16_t kWarning = 0xd945u;

void panel(LGFX_Sprite& canvas,int x,int y,int width,int height,int radius,uint16_t color)
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
                 const PencilOcclusion& occlusion,PencilDetail)
{
    const auto frame=track.sample(car.motion.distance);
    const auto pose=makeCarPose(frame,car);
    // Derive screen bounds from the complete rotated car, not from its centre.
    // Very close/lapped cars may span several tiles; they must not get square-cut.
    float left=float(canvas.width()),right=0,top=float(canvas.height()),bottom=0;
    bool inFront=false,nearClipped=false;
    for(float x : {-.64f,.64f}) for(float y : {0.f,.64f}) for(float z : {-1.f,1.f}) {
        const auto point=trackToCamera(camera,carPointToWorld({x,y,z},pose));
        if(point.z<kTrackNearPlane) {nearClipped=true;continue;}
        TrackScreenPoint p{};
        if(!projectTrackPoint(camera,point,p))continue;
        inFront=true;left=std::min(left,p.x);right=std::max(right,p.x);
        top=std::min(top,p.y);bottom=std::max(bottom,p.y);
    }
    if(!inFront)return;
    if(nearClipped) {left=0;top=0;right=canvas.width()-1;bottom=canvas.height()-1;}
    if(right<0 || bottom<0 || left>=canvas.width() || top>=canvas.height())return;
    const int x0=int(std::max(0.f,std::floor(left))),y0=int(std::max(0.f,std::floor(top)));
    const int x1=int(std::min(float(canvas.width()-1),std::ceil(right)));
    const int y1=int(std::min(float(canvas.height()-1),std::ceil(bottom)));
    const float phase=car.motion.distance/(.34f*kModelWheelRadius);
    const float cosine=std::cos(phase),sine=std::sin(phase);
    const auto transform=[&](CarPoint p,uint8_t wheel) {
        return trackToCamera(camera,carPointToWorld(animateCarPanelPoint(p,wheel,cosine,sine),pose));
    };
    for(int y=y0;y<=y1;y+=112) for(int x=x0;x<=x1;x+=112) {
        raster.begin(x,y);
        for(int i=0;i<12;++i) {
            const float a=i*6.2831853f/12,b=(i+1)*6.2831853f/12;
            CarPanel shadow;
            shadow.point={{{0,.001f,0},{.52f*std::cos(a),.001f,.82f*std::sin(a)},
                           {.52f*std::cos(b),.001f,.82f*std::sin(b)},{0,.001f,0}}};
            shadow.color=0x9cd3;
            raster.panel(camera,shadow,transform);
        }
        for(std::size_t i=0;i<mesh.count;++i)raster.panel(camera,mesh.panels[i],transform);
        raster.blit(canvas,&occlusion);
    }
}

void drawSpeedLines(LGFX_Sprite& canvas, const RaceCarSnapshot& player,
                    uint32_t elapsedMs, PencilDetail detail)
{
    if (player.motion.speed < 8.0f) return;
    int count = player.motion.speed > 18.0f ? 10 : player.motion.speed > 12.0f ? 6 : 2;
    if (detail == PencilDetail::Medium) count = std::max(2, count - 2);
    if (detail == PencilDetail::Low) count = std::max(1, count / 2);
    for (int index = 0; index < count; ++index) {
        const float travel = std::fmod(elapsedMs * 0.00065f + index * 0.173f, 1.0f);
        const int side = (index & 1) == 0 ? -1 : 1;
        const int x = 233 + side * static_cast<int>(80 + 140 * travel);
        const int y = 175 + static_cast<int>(200 * travel);
        const int length = 10 + static_cast<int>(player.motion.speed * 0.7f);
        canvas.drawLine(x, y, x + side * length, y + 8 + static_cast<int>(10 * travel), kFaint);
    }
}

void drawPaperAndMountains(LGFX_Sprite& canvas, PencilDetail detail)
{
    uint32_t hash = 0xa341316cu;
    const int textureCount = detail == PencilDetail::High ? 26
                             : detail == PencilDetail::Medium ? 16 : 8;
    for (int index = 0; index < textureCount; ++index) {
        hash = hash * 1664525u + 1013904223u;
        const int x = 40 + static_cast<int>((hash >> 8u) % 386u);
        hash = hash * 1664525u + 1013904223u;
        const int y = 38 + static_cast<int>((hash >> 8u) % 388u);
        canvas.drawLine(x, y, x + 3, y, kFaint);
    }
    constexpr int kHorizon = 148;
    const int mountainStep = detail == PencilDetail::Low ? 76 : 38;
    for (int x = 38; x < 430; x += mountainStep) {
        const int peak = kHorizon - 9 - ((x * 13) % 24);
        canvas.drawLine(x - 38, kHorizon, x, peak, kFaint);
        canvas.drawLine(x, peak, x + 39, kHorizon, kFaint);
    }
}

void drawHud(LGFX_Sprite& canvas, const RaceSnapshot& race,
             const OverpassTrack& track,
             const TrackMiniMap& miniMap)
{
    // Paper instrument cards keep labels readable below the dark bridge and
    // on the newly coloured road, without text-sized cream cut-outs.
    const auto card=[&](int x,int y,int width,int height,int radius) {
        panel(canvas,x,y,width,height,radius,kPaper);
    };
    card(106,35,253,39,12);
    card(116,383,241,44,11);
    canvas.drawLine(232,46,232,64,kFaint);
    canvas.drawLine(241,394,241,416,kFaint);
    const RaceCarSnapshot& player = race.player();
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(kPencil, kPaper);
    canvas.setTextSize(2);
    char value[32] = {};
    std::snprintf(value, sizeof(value), "LAP %u/3", static_cast<unsigned>(
        std::min<uint8_t>(kRaceLapCount, static_cast<uint8_t>(player.completedLaps + 1u))));
    canvas.drawString(value, 157, 55);
    std::snprintf(value, sizeof(value), "P%u/%u", static_cast<unsigned>(player.position),
                  static_cast<unsigned>(race.carCount));
    canvas.drawString(value, 312, 55);
    canvas.setTextSize(1);
    std::snprintf(value, sizeof(value), "%03d km/h",
                  static_cast<int>(std::lround(player.motion.speed * 3.6f)));
    canvas.drawString(value, 159, 411);
    canvas.drawRect(263, 408, 86, 10, kPencil);
    canvas.fillRect(265, 410, static_cast<int>(82.0f * player.motion.boostCharge),
                    6, carSpec(player.car).accentColor);
    canvas.drawString("BOOST", 306, 394);

    miniMap.draw(canvas);
    // Rivals first, player last: even four cars at the same pixel cannot hide
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
                 const ResultsSelection& selection)
{
    canvas.fillScreen(kPaper);
    drawPaperAndMountains(canvas, PencilDetail::High);
    const RaceCarSnapshot& player = race.player();
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(carSpec(player.car).accentColor, kPaper);
    canvas.setTextSize(4);
    char position[20] = {};
    std::snprintf(position, sizeof(position), "PLACE %u/%u",
                  static_cast<unsigned>(player.position),
                  static_cast<unsigned>(race.carCount));
    canvas.drawString(position, canvas.width() / 2, 92);
    char time[20] = {};
    formatRaceTime(player.finished ? player.finishSeconds : race.elapsedSeconds, time, sizeof(time));
    canvas.setTextSize(2);
    canvas.setTextColor(kPencil, kPaper);
    canvas.drawString(time, canvas.width() / 2, 145);
    char best[32] = "BEST --:--.---";
    if (player.bestLapSeconds > 0.0f) {
        char lap[20] = {};
        formatRaceTime(player.bestLapSeconds, lap, sizeof(lap));
        std::snprintf(best, sizeof(best), "BEST %s", lap);
    }
    canvas.setTextSize(1);
    canvas.setTextColor(kFaint, kPaper);
    canvas.drawString(best, canvas.width() / 2, 177);
    for (int index = 0; index < static_cast<int>(ResultAction::Count); ++index) {
        const ResultAction action = static_cast<ResultAction>(index);
        const bool selected = action == selection.cursor();
        const int y = 245 + index * 50;
        if (selected) canvas.drawRoundRect(116, y - 17, 234, 36, 8,
                                           carSpec(player.car).accentColor);
        canvas.setTextSize(selected ? 2 : 1);
        canvas.setTextColor(selected ? kPencil : kFaint, kPaper);
        canvas.drawString(resultActionLabel(action), canvas.width() / 2, y);
    }
}

}  // namespace

void RaceRenderer::open(int width, int height)
{
    _width = width;
    _height = height;
    _surface.reset(new(std::nothrow) RaceSurfaceCache);
    _cachedTrack = TrackId::Count;
}

void RaceRenderer::close()
{
    _width = 0;
    _height = 0;
    _surface.reset();
}

void RaceRenderer::render(const GameFlow& flow, const RaceController& race,
                          const ResultsSelection& results,
                          uint32_t screenElapsedMs, bool pausedForInputLoss,
                          PencilDetail detail)
{
    if (_width <= 0 || _height <= 0 || !race.prepared()) return;
    auto& canvas = GetHAL().getCanvas();
    if(!_surface) {
        canvas.fillScreen(kPaper);
        canvas.setTextDatum(textdatum_t::middle_center);
        canvas.setTextSize(1);canvas.setTextColor(kWarning,kPaper);
        canvas.drawString("RENDER MEMORY LOW",_width/2,220);
        canvas.drawString("HOLD BOTH BUTTONS TO EXIT",_width/2,244);
        return;
    }
    const auto surfaceDetail=detail==PencilDetail::Low ? CarSurfaceDetail::Low : CarSurfaceDetail::Medium;
    {
        for(std::size_t i=0;i<race.snapshot().carCount;++i) {
            const auto id=race.snapshot().cars[i].car;
            if(_surface->detail==surfaceDetail && _surface->cars[i]==id)continue;
            auto& mesh=_surface->meshes[i];
            const auto built=buildCarSurfaceInto(id,mesh.panels.data(),
                                                 mesh.panels.size(),surfaceDetail);
            if(built.overflowed) {_surface.reset();return;}
            mesh.count=built.count;
            _surface->cars[i]=id;
        }
        // Inactive slots may retain another quality tier from an earlier race.
        // Invalidate them before a later solo -> four-car roster expansion.
        for(std::size_t i=race.snapshot().carCount;i<kMaximumRaceCars;++i)
            _surface->cars[i]=CarId::Count;
        _surface->detail=surfaceDetail;
    }
    if (flow.screen() == GameScreen::Results) {
        drawResults(canvas, race.snapshot(), results);
        return;
    }
    if (_cachedTrack != race.track().id()) {
        _trackGeometry.open(race.track());
        _miniMap.open(_trackGeometry);
        _cachedTrack = race.track().id();
    }
    track_paint::backdrop(canvas,detail);
    const RaceSnapshot& snapshot = race.snapshot();
    const RaceCarSnapshot& player = snapshot.player();
    const TrackFrame playerFrame = race.track().sample(player.motion.distance);
    const TrackCamera camera=makeRacerChaseCamera(playerFrame,player.motion.lateralOffset,_width,_height);
    drawPencilTrack(canvas, camera, _trackGeometry, detail, &_surface->occlusion);

    std::array<std::size_t, kMaximumRaceCars> order{};
    std::array<float, kMaximumRaceCars> depth{};
    for (std::size_t i = 0; i < snapshot.carCount; ++i) {
        order[i] = i;
        depth[i] = trackToCamera(camera, race.track().sample(snapshot.cars[i].motion.distance).center).z;
    }
    std::sort(order.begin(), order.begin() + snapshot.carCount,
              [&](std::size_t a, std::size_t b) { return depth[a] > depth[b]; });
    for (std::size_t slot = 0; slot < snapshot.carCount; ++slot) {
        const RaceCarSnapshot& car = snapshot.cars[order[slot]];
        if (depth[order[slot]] < kTrackNearPlane || !car.active ||
            (car.finished && !car.player)) continue;
        drawRaceCar(canvas, camera, race.track(), car,
                    _surface->meshes[order[slot]], _surface->raster, _surface->occlusion, detail);
    }
    if (flow.screen() == GameScreen::Racing)
        drawSpeedLines(canvas, player, screenElapsedMs, detail);

    canvas.setTextDatum(textdatum_t::middle_center);
    if (player.motion.wallImpact > 0.05f) {
        canvas.drawCircle(_width / 2, _height / 2, 194, kWarning);
        canvas.drawCircle(_width / 2 + 2, _height / 2 - 1, 188, kWarning);
    }
    drawHud(canvas, snapshot, race.track(), _miniMap);
    const GameScreen screen = flow.screen();
    if (screen == GameScreen::GridIntro) {
        panel(canvas,91,192,284,52,12,track_paint::night);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(3);
        canvas.drawString(snapshot.carCount == 1u ? "SOLO RUN" : "CHASE THE PACK", _width / 2, 218);
    } else if (screen == GameScreen::Countdown) {
        const int count = std::max(1, 3 - static_cast<int>(screenElapsedMs / 1000u));
        char number[4] = {};
        std::snprintf(number, sizeof(number), "%d", count);
        canvas.fillCircle(_width/2,222,43,track_paint::night);
        canvas.drawCircle(_width/2,222,43,track_paint::ridge);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(7);
        canvas.drawString(number, _width / 2, 222);
    } else if (screen == GameScreen::Paused) {
        panel(canvas,100,185,266,81,12,track_paint::night);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(3);
        canvas.drawString(pausedForInputLoss ? "INPUT LOST" : "PAUSED", _width / 2, 213);
        canvas.setTextSize(1);
        canvas.drawString("HOLD RED TO CONTINUE", _width / 2, 246);
    } else if (screen == GameScreen::Finish) {
        panel(canvas,98,183,270,74,12,track_paint::night);
        canvas.setTextColor(track_paint::chalk,track_paint::night);
        canvas.setTextSize(5);
        canvas.drawString("FINISH!", _width / 2, 220);
    }
}

}  // namespace lets_and_go
