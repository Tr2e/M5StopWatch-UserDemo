#include "arena_renderer.h"
#include "../../app_gundam_museum/view/museum_layout.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace gundam_arena {
namespace {
void rasterizeBall(lets_and_go::CarSurfaceRaster<424,424>& raster,
                   const lets_and_go::TrackCamera& camera,
                   lets_and_go::TrackCameraPoint v,float radius){
    constexpr float near=lets_and_go::kTrackNearPlane;
    if(!(v.z>near) || !std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))return;
    constexpr int n=16;
    const lets_and_go::CarSurfaceVertex center{v.x,v.y,v.z,0,0};
    lets_and_go::CarSurfaceVertex rim[n];
    const auto fan=[&](float scale,uint16_t color){
        for(int i=0;i<n;++i){
            const float a=float(i)*(2.f*kPi)/n;
            rim[i]={v.x+radius*scale*std::cos(a),v.y+radius*scale*std::sin(a),v.z,0,0};
        }
        for(int i=0;i<n;++i)
            raster.cameraTriangle(camera,center,rim[i],rim[(i+1)%n],color,lets_and_go::CarPaint::Solid,255);
    };
    fan(1.12f,space::ballRim);
    fan(1.f,space::ball);
}
}

bool ArenaRenderer::open(){
    close();
    _surface.reset(new(std::nothrow) Surface{});
    if(!_surface)return false;
    buildRx78Rigged(_surface->mesh,_bind);
    ++_meshBuilds;
    _surface->projection.index(_surface->mesh);
    ++_indexBuilds;
    _skeleton=_bind;
    return true;
}
void ArenaRenderer::close(){_surface.reset();_stats={};}
std::size_t ArenaRenderer::workingBytes(){return sizeof(Surface);}

void ArenaRenderer::render(lgfx::LGFXBase& canvas,const CharacterModel& character,const ArenaView& view){
    using gundam_museum::layout::side;
    using gundam_museum::layout::top;
    canvas.fillScreen(space::background);
    const ArenaCamera cam(view);
    const auto spaceCamera=space::canvasCamera(canvas);
    space::draw(canvas,cam,spaceCamera);
    if(!_surface)return;
    evaluateSkeleton(_skeleton,character.pose);
    const auto eye=cam.eye();
    const int p=std::clamp(view.percent,50,100),w=(424*p+50)/100,h=(424*p+50)/100;
    auto& raster=_surface->raster;
    raster.setSolidFastPath(true);raster.begin(0,0,w,h);
    lets_and_go::TrackCamera camera{};
    camera.principalX=w*.5f;camera.principalY=h*.5f;
    camera.focalLength=space::kFocal*7.f*float(h)/side;
    const float correction=float(w)/h;
    const auto project=[&](Point point,uint8_t tag){
        const int bone=std::clamp(int(tag?tag-1:0),0,kBoneCount-1);
        auto world=_skeleton.world[bone].apply(point);
        auto v=cam(world);v.x*=correction;return v;
    };
    auto& projection=_surface->projection;
    projection.begin();
    _stats={};_stats.total=_surface->mesh.count;_stats.meshBuilds=_meshBuilds;_stats.indexBuilds=_indexBuilds;
    for(std::size_t i=0;i<_surface->mesh.count;++i){
        projection.passes[i]=-1;
        const auto& face=_surface->mesh.panels[i];
        const int bone=std::clamp(int(face.wheel?face.wheel-1:0),0,kBoneCount-1);
        const Point n=_skeleton.world[bone].rotate(_surface->mesh.normals[i]);
        const Point p0=_skeleton.world[bone].apply(face.point[0]);
        const float facing=dot(n,subtract(eye,p0));
        if(!_surface->mesh.twoSided[i] && facing<-.035f){++_stats.culled;continue;}
        projection.passes[i]=facing<=0?0:1;
    }
    for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<_surface->mesh.count;++i){
        if(projection.passes[i]!=pass)continue;
        const auto& face=_surface->mesh.panels[i];
        lets_and_go::PreparedCarPanel prepared{};
        projection.panel(prepared,camera,face,i,project);
        if(!prepared.visibility || prepared.right<0 || prepared.left>=w || prepared.bottom<0 || prepared.top>=h){
            ++_stats.offscreen;continue;}
        raster.preparedPanel(camera,prepared);++_stats.submitted;
    }
    {
        auto v=cam({character.ball.x,character.ball.y,character.ball.z});
        v.x*=correction;
        rasterizeBall(raster,camera,v,kBallR);
    }
    raster.blitScaled(canvas,(canvas.width()-side)/2,top,side,side);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(space::navigation,space::background);
    canvas.setTextSize(1);
    if(character.mode==Mode::Pose){
        char line[48];
        std::snprintf(line,sizeof(line),"POSE %s",boneName(character.selected));
        canvas.drawString(line,canvas.width()/2,18);
        for(const auto& button:{gundam_museum::layout::previous,gundam_museum::layout::next}){
            const int x=button.x+button.width/2,y=button.y+button.height/2;
            const int sign=button.x<canvas.width()/2?-1:1;
            for(int d=0;d<2;++d){
                canvas.drawLine(x-sign*4+d,y-9,x+sign*4+d,y,space::navigation);
                canvas.drawLine(x+sign*4+d,y,x-sign*4+d,y+9,space::navigation);
            }
        }
    }else if(character.action==Action::Kick||character.action==Action::Jump||
             character.action==Action::Fall||character.action==Action::Land||
             character.action==Action::Gesture||character.action==Action::Dance||
             character.danceMode){
        canvas.drawString(playActionHud(character),canvas.width()/2,18);
    }else if(character.clipIndex<kPlayClipCount){
        canvas.drawString(playClipHud(character.clipIndex),canvas.width()/2,18);
    }else if(const char* skill=autonSkillHud(view.skillHud)){
        canvas.drawString(skill,canvas.width()/2,18);
    }else if(view.padHint==1)canvas.drawString("CAL",canvas.width()/2,18);
    else if(view.padHint==2)canvas.drawString("PAD",canvas.width()/2,18);
    else if(view.padHint==3)canvas.drawString("PAD FAULT",canvas.width()/2,18);
    else canvas.drawString("A/B CLIP",canvas.width()/2,18);
}
} // namespace gundam_arena
