#include "../main/apps/app_gundam_museum/view/museum_renderer.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
using namespace gundam_museum;
using Clock=std::chrono::steady_clock;
void checkProjectionBoundaries(){
    auto mesh=std::make_unique<Mesh>();auto cache=std::make_unique<MuseumProjectionCache>();
    // Full-capacity, entirely unique geometry must terminate its hash probe.
    mesh->count=Mesh::capacity;
    for(std::size_t i=0;i<MuseumProjectionCache::corners;++i)
        mesh->panels[i/4].point[i%4]={float(i),0,1};
    cache->index(*mesh);assert(cache->count==MuseumProjectionCache::corners);
    for(std::size_t i=0;i<MuseumProjectionCache::corners;++i)assert(cache->indices[i]==i);
    // Signed zero shares a vertex; transform tags do not.
    *mesh=Mesh{};mesh->count=2;mesh->panels[1].point[0].x=-0.f;
    cache->index(*mesh);assert(cache->count==1);
    mesh->panels[1].wheel=1;cache->index(*mesh);assert(cache->count==2);
    // A triangle crossing the near plane retains the production clip path.
    mesh->count=1;mesh->panels[0].point={{{-.2f,-.2f,.1f},{.2f,-.2f,1},{.2f,.2f,1},{-.2f,.2f,.1f}}};
    cache->index(*mesh);cache->begin();
    lets_and_go::TrackCamera camera{};
    auto transform=[](Point p,uint8_t){return lets_and_go::TrackCameraPoint{p.x,p.y,p.z};};
    lets_and_go::PreparedCarPanel a,b;
    cache->panel(a,camera,mesh->panels[0],0,transform);
    lets_and_go::prepareCarPanel(b,camera,mesh->panels[0],transform);
    assert(a.visibility==2 && a.visibility==b.visibility);
    assert(a.left==b.left && a.right==b.right && a.top==b.top && a.bottom==b.bottom);
    for(unsigned i=0;i<4;++i)assert(a.camera[i].x==b.camera[i].x && a.camera[i].y==b.camera[i].y && a.camera[i].z==b.camera[i].z);
}
void checkSolidRaster(){
    lets_and_go::CarSurfaceRaster<32,32> raster;
    LGFX_Sprite canvas;canvas.createSprite(32,32);
    for(auto paint:{lets_and_go::CarPaint::Solid,lets_and_go::CarPaint::Eye,lets_and_go::CarPaint::Glass})
    for(uint8_t light:{0,99,255})for(int size:{21,32}){
        std::vector<uint16_t> frames[2];std::vector<uint16_t> depths[2];
        for(int mode=0;mode<2;++mode){
            canvas.fillScreen(0x0863);raster.begin(0,0,size,size);raster.setSolidFastPath(mode==1);
            raster.triangle({-2,1,.15f,0,0},{27,3,.2f,.2f,0},{7,29,.25f,0,.25f},0xef5d,paint,light);
            raster.triangle({1,3,.15f,0,0},{24,5,.2f,.2f,0},{9,27,.25f,0,.25f},0xf800,paint,light);
            raster.blitScaled(canvas,0,0,32,32);frames[mode]=canvas.frame();
            for(int y=0;y<size;++y)for(int x=0;x<size;++x)depths[mode].push_back(raster.depthAt(x,y));
        }
        assert(frames[0]==frames[1] && depths[0]==depths[1]);
        assert(std::any_of(depths[0].begin(),depths[0].end(),[](auto d){return d!=0;}));
    }
}
int main(){
    checkProjectionBoundaries();checkSolidRaster();
    MuseumRenderer renderer;assert(renderer.open());
    LGFX_Sprite canvas;canvas.createSprite(468,466);
    std::size_t cases=0,oldTransforms=0,newTransforms=0;
    for(auto model:{ModelId::Rx78,ModelId::CharZaku,ModelId::NuGundam,ModelId::Sazabi,ModelId::StrikeGundam}) {
        for(int percent:{65,90,100}) {
            std::vector<double> times[2];
            std::size_t oldCount=0,newCount=0;
            View view;view.model=model;
            for(bool equipment:{false,true})for(bool detail:{false,true})
            for(float pitch:{-.2f,.1f,.7f})for(int yaw=0;yaw<24;++yaw){
                view.equipment=equipment;view.detail=detail;view.pitch=pitch;
                view.yaw=float(yaw)*6.2831853f/24;
                // Warm model creation separately; alternate A/B order to avoid
                // charging cold builds or a consistently first pass to A.
                renderer.render(canvas,view,percent);
                std::vector<uint16_t> frames[2];
                for(int i=0;i<2;++i){
                    const int mode=(i+yaw)%2;renderer.setOptimizations(mode==1);
                    const auto start=Clock::now();
                    renderer.render(canvas,view,percent,true,false,false,true);
                    const auto end=Clock::now();
                    times[mode].push_back(std::chrono::duration<double,std::micro>(end-start).count());
                    frames[mode]=canvas.frame();
                    if(mode)newCount+=renderer.stats().transformed;
                    else oldCount+=renderer.stats().transformed;
                }
                assert(frames[0]==frames[1]);++cases;
            }
            std::cout<<"model="<<int(model)<<" percent="<<percent<<" samples="<<times[0].size();
            for(int mode=0;mode<2;++mode){
                double sum=0;for(auto value:times[mode])sum+=value;
                std::sort(times[mode].begin(),times[mode].end());
                std::cout<<(mode?" optimized":" reference")<<"_mean_us="<<sum/times[mode].size()
                         <<" p95_us="<<times[mode][times[mode].size()*95/100];
            }
            std::cout<<" unique_vertices="<<renderer.stats().vertices<<" transforms_before="<<oldCount<<" transforms_after="<<newCount<<'\n';
            oldTransforms+=oldCount;newTransforms+=newCount;
        }
    }
    // Continuous drag angles catch errors hidden by a fixed 15-degree grid.
    std::mt19937 random(78);std::uniform_real_distribution<float> yaw(-3.141593f,3.141593f),pitch(-.2f,.7f);
    for(auto model:{ModelId::Rx78,ModelId::CharZaku,ModelId::NuGundam,ModelId::Sazabi,ModelId::StrikeGundam})for(int i=0;i<256;++i){
        View v;v.model=model;v.yaw=yaw(random);v.pitch=pitch(random);
        renderer.setOptimizations(false);renderer.render(canvas,v,65);const auto before=canvas.frame();
        renderer.setOptimizations(true);renderer.render(canvas,v,65);
        if(before!=canvas.frame())std::cerr<<"continuous angle mismatch model="<<int(model)<<" yaw="<<v.yaw<<" pitch="<<v.pitch<<'\n';
        assert(before==canvas.frame());++cases;
    }
    std::cout<<"pixel_identical_cases="<<cases<<" working_bytes="<<MuseumRenderer::workingBytes()
             <<" transforms_before="<<oldTransforms<<" transforms_after="<<newTransforms<<'\n';
}
