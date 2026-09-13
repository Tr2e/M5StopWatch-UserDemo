#include "museum_renderer.h"
#include "../../app_lets_and_go_racer/controller/home_layout.h"
#include <algorithm>
#include <cmath>
#include <new>

namespace gundam_museum {
namespace {
constexpr uint16_t background=0x0863,white=0xef5d,muted=0x8c72,accent=0xf629,line=0x2948;
void label(lgfx::LGFXBase& c,const char* text,int x,int y,int size,uint16_t color=white){
    c.setTextDatum(textdatum_t::middle_center);c.setTextColor(color,background);c.setTextSize(size);c.drawString(text,x,y);
}
struct Camera {
    float cy,sy,cp,sp,pivot;
    Camera(const View& v):cy(std::cos(v.yaw)),sy(std::sin(v.yaw)),cp(std::cos(v.pitch)),sp(std::sin(v.pitch)),pivot(v.detail?2.97f:1.68f){}
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
    if(partial)canvas.fillRect(40,100,canvas.width()-80,288,background);
    else canvas.fillScreen(background);
    if(!_surface){label(canvas,"MODEL MEMORY UNAVAILABLE",canvas.width()/2,220,1);return;}
    if(!_cached || _equipment!=view.equipment || _gray!=gray || _buried!=keepBuried){
        buildRx78(_surface->mesh,{view.equipment,keepBuried,gray});
        _equipment=view.equipment;_gray=gray;_buried=keepBuried;_cached=true;
    }
    if(!partial){
        label(canvas,"GUNDAM MUSEUM",canvas.width()/2,43,2);
        label(canvas,"<",116,69,2,accent);
        label(canvas,"COLLECTION  /  001",canvas.width()/2,77,1,muted);
    }
    label(canvas,view.detail?"HEAD STUDY":view.equipment?"RX-78-2  /  EQUIPPED":"RX-78-2  /  UNARMED",canvas.width()/2,104,1,accent);
    // One fixed envelope per exhibit, no angle-dependent auto-fit breathing.
    const Camera transform(view);const auto eye=transform.eye();
    const int p=std::clamp(percent,50,100),w=(352*p+50)/100,h=(288*p+50)/100;
    auto& raster=_surface->raster;raster.begin(0,0,w,h);
    lets_and_go::TrackCamera camera{};
    camera.principalX=w*.5f;camera.principalY=h*.5f;
    camera.focalLength=(view.detail?325.f:72.f)*7.f*float(h)/288.f;
    const float correction=(float(w)/352)/(float(h)/288);
    const auto project=[&](Point point,uint8_t tag){auto v=transform(point,tag);v.x*=correction;return v;};
    _stats={};_stats.total=_surface->mesh.count;
    // The diagnostic path draws backfaces first. Quantized equal depth must
    // not let an invisible reverse face overwrite a visible front face.
    for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<_surface->mesh.count;++i){
        const auto& face=_surface->mesh.panels[i];
        const float facing=dot(_surface->mesh.normals[i],subtract(eye,face.point[0]));
        const bool back=facing<=0;
        if(pass==0 && !back)continue;
        if(pass==1 && back)continue;
        // Keep a small grazing-angle band: Q13 depth and pixel-centre coverage
        // can still expose a silhouette sample of an almost edge-on panel.
        if(cull && !_surface->mesh.twoSided[i] && facing<-.035f){++_stats.culled;continue;}
        lets_and_go::PreparedCarPanel prepared{};
        lets_and_go::prepareCarPanel(prepared,camera,face,project);
        if(!prepared.visibility || prepared.right<0 || prepared.left>=w || prepared.bottom<0 || prepared.top>=h){++_stats.offscreen;continue;}
        raster.preparedPanel(camera,prepared);++_stats.submitted;
    }
    // Raster and UI share the production RGB565 surface; controls stay native.
    raster.blitScaled(canvas,canvas.width()/2-176,100,352,288);
    if(view.detail)canvas.fillRect(158,368,152,20,background);
    canvas.fillRect(164,368,140,1,line);
    label(canvas,"<",123,350,2,muted);label(canvas,">",345,350,2,muted);
    label(canvas,"RX-78-2",canvas.width()/2,379,2);
    if(!partial){
        using namespace lets_and_go::home_layout;
        for(const auto& r:{inspectAuto,inspectReset})canvas.drawRect(r.x,r.y,r.width,r.height,line);
        label(canvas,view.automatic?"STOP":"AUTO",170,414,2,accent);
        label(canvas,"RESET",298,414,2);
        label(canvas,"DRAG / A VIEW / B RESET",canvas.width()/2,446,1,muted);
    }
}
} // namespace gundam_museum
