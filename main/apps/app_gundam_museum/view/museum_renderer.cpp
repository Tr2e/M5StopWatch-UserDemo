#include "museum_renderer.h"
#include "../model/nu_gundam.h"
#include "../model/strike_gundam.h"
#include "museum_layout.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace gundam_museum {
namespace {
constexpr uint16_t background=0x0863,white=0xef5d;
void label(lgfx::LGFXBase& c,const char* text,int x,int y,int size,uint16_t color=white){
    c.setTextDatum(textdatum_t::middle_center);c.setTextColor(color,background);c.setTextSize(size);c.drawString(text,x,y);
}
struct Camera {
    float cy,sy,cp,sp,pivot;
    Camera(const View& v):cy(std::cos(v.yaw)),sy(std::sin(v.yaw)),cp(std::cos(v.pitch)),sp(std::sin(v.pitch)),
        pivot(v.model==ModelId::StrikeGundam?(v.detail?2.78f:1.72f):v.model==ModelId::NuGundam?(v.detail?2.68f:1.65f):(v.detail?2.41f:1.49f)){}
    lets_and_go::TrackCameraPoint operator()(Point p,uint8_t=0)const{
        p.y-=pivot;const float x=p.x*cy+p.z*sy,z=p.z*cy-p.x*sy;
        return {x,p.y*cp-z*sp,7.f-(z*cp+p.y*sp)};
    }
    Point eye()const{return {-7*sy*cp,7*sp+pivot,7*cy*cp};}
};
}
bool MuseumRenderer::open(){close();_surface.reset(new(std::nothrow) Surface{});return bool(_surface);}
void MuseumRenderer::close(){_surface.reset();_cached=false;_stats={};}
std::size_t MuseumRenderer::workingBytes(){return sizeof(Surface);}
void MuseumRenderer::render(lgfx::LGFXBase& canvas,const View& view,int percent,bool cull,bool gray,bool keepBuried,bool partial){
    if(partial)canvas.fillRect(0,layout::top,canvas.width(),layout::side,background);
    else canvas.fillScreen(background);
    if(!_surface){label(canvas,"MODEL MEMORY UNAVAILABLE",canvas.width()/2,220,1);return;}
    const bool nu=view.model==ModelId::NuGundam,strike=view.model==ModelId::StrikeGundam;
    if(!_cached || _model!=view.model || _equipment!=view.equipment || _gray!=gray || _buried!=keepBuried || _pose!=view.pose){
        if(strike)buildStrikeGundam(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else if(nu)buildNuGundam(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        else buildRx78(_surface->mesh,{view.equipment,keepBuried,gray,view.pose});
        _model=view.model;
        _pose=view.pose;
        _equipment=view.equipment;_gray=gray;_buried=keepBuried;_cached=true;
    }
    // One fixed envelope per exhibit, no angle-dependent auto-fit breathing.
    const Camera transform(view);const auto eye=transform.eye();
    const int p=std::clamp(percent,50,100),w=(424*p+50)/100,h=(424*p+50)/100;
    auto& raster=_surface->raster;raster.begin(0,0,w,h);
    lets_and_go::TrackCamera camera{};
    camera.principalX=w*.5f;camera.principalY=h*.5f;
    const float scale=strike?(view.detail?148.f:82.f):nu?(view.detail?148.f:78.f):(view.detail?164.f:98.f);
    camera.focalLength=scale*7.f*float(h)/layout::side;
    const float correction=float(w)/h;
    const auto project=[&](Point point,uint8_t tag){auto v=transform(point,tag);v.x*=correction;return v;};
    _stats={};_stats.total=_surface->mesh.count;
    // The diagnostic path draws backfaces first. Quantized equal depth must
    // not let an invisible reverse face overwrite a visible front face.
    for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<_surface->mesh.count;++i){
        if(view.detail && _surface->mesh.parts[i]!=Part::Head)continue;
        const auto& face=_surface->mesh.panels[i];
        const float facing=dot(_surface->mesh.normals[i],subtract(eye,face.point[0]));
        const bool back=facing<=0;
        if(pass==0 && !back)continue;
        if(pass==1 && back)continue;
        // Keep a small grazing-angle band: Q13 depth and pixel-centre coverage
        // can still expose a silhouette sample of an almost edge-on panel.
        // Nu's thin backpack mounting rims need both sides at the enlarged
        // raster: quantized edge coverage can survive behind the continuous face.
        const bool retainMountRim=nu && _surface->mesh.parts[i]==Part::Backpack;
        if(cull && !_surface->mesh.twoSided[i] && !retainMountRim && facing<-.035f){++_stats.culled;continue;}
        lets_and_go::PreparedCarPanel prepared{};
        lets_and_go::prepareCarPanel(prepared,camera,face,project);
        if(!prepared.visibility || prepared.right<0 || prepared.left>=w || prepared.bottom<0 || prepared.top>=h){++_stats.offscreen;continue;}
        raster.preparedPanel(camera,prepared);++_stats.submitted;
    }
    raster.blitScaled(canvas,(canvas.width()-layout::side)/2,layout::top,layout::side,layout::side);
    // Only two native-size navigation chevrons; the rest is the model.
    for(const auto& button:{layout::previous,layout::next}){
        const int x=button.x+button.width/2,y=button.y+button.height/2;
        const int sign=button.x<canvas.width()/2?-1:1;
        for(int d=0;d<2;++d){
            canvas.drawLine(x-sign*4+d,y-9,x+sign*4+d,y,white);
            canvas.drawLine(x+sign*4+d,y,x-sign*4+d,y+9,white);
        }
    }
}
} // namespace gundam_museum
