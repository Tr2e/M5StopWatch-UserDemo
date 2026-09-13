#include "../main/apps/app_gundam_museum/view/museum_renderer.h"
#include "../main/apps/app_gundam_museum/controller/museum_controller.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace gundam_museum;
void controls(){
    MuseumController c;lets_and_go::DeviceControlFrame f;f.input.valid=true;
    c.update(f,1000);assert(c.percent(1000)==100);
    f.preview={1,30,20,true,true};assert(c.update(f,1010));
    assert(std::abs(c.view().yaw+.04f)<.0001f && c.percent(1010)==65);
    f.preview={1,30,20,false,true};c.update(f,1050);
    f.preview.changed=false;c.update(f,1250);assert(c.percent(1250)==100);
    f.input.confirmPressed=true;c.update(f,1260);f.input.confirmPressed=false;
    f.preview={1,120,40,true,true};c.update(f,1270);assert(std::abs(c.view().yaw+.40f)<.0001f);
    f.preview={2,0,300,true,true};c.update(f,1280);assert(c.view().pitch==.70f);
    f.preview.dy=299;c.update(f,1290);assert(c.view().pitch<.70f);
    f.input.valid=false;c.update(f,1300);const auto interrupted=c.view();
    f.input.valid=true;f.preview.dx=100;c.update(f,1310);assert(c.view().yaw==interrupted.yaw);
    f.preview={};f.navigation=1;c.update(f,1320);assert(!c.view().equipment && !c.view().detail);
    c.update(f,1330);assert(c.view().detail);c.update(f,1340);assert(c.view().equipment && !c.view().detail);
    f.navigation=0;f.autoToggle=true;c.update(f,1350);f.autoToggle=false;
    const float start=c.view().yaw;c.update(f,2350);assert(c.view().automatic && c.view().yaw!=start);
    f.preview={3,1,0,true,true};c.update(f,2400);assert(!c.view().automatic);
    f.input.valid=false;f.input.cancelPressed=true;c.update(f,2500);assert(c.exitRequested());
    c.reset();assert(!c.exitRequested());f={};f.input.valid=true;
    c.update(f,0xfffffff0u);f.autoToggle=true;c.update(f,0xfffffff1u);f.autoToggle=false;c.update(f,30);
    assert(std::isfinite(c.view().yaw));
    // Entire touch gesture between slow render frames is delivered once.
    lets_and_go::DeviceControlLogic logic;logic.setScreen(lets_and_go::GameScreen::CarInspect);
    logic.presentScreen(lets_and_go::GameScreen::CarInspect);logic.touch(false,0,0);
    logic.touch(true,210,230);logic.touch(true,250,240);logic.touch(false,250,240);
    c.reset();assert(c.update(logic.consume(true),400));const auto moved=c.view();
    c.update(logic.consume(true),410);assert(c.view().yaw==moved.yaw);
}
int main(int argc,char** argv){
    controls();
    const std::string out=argc>1?argv[1]:"/tmp/gundam-museum";
    MuseumRenderer renderer;assert(renderer.open());
    LGFX_Sprite canvas;canvas.createSprite(468,466);
    View view;
    const auto save=[&](const std::string& name){canvas.save(out+"/"+name+".ppm");};
    renderer.render(canvas,view);save("rx78-equipped");
    std::cout<<"working_bytes="<<renderer.workingBytes()<<" panels="<<renderer.mesh().count
             <<" omitted="<<renderer.mesh().buriedOmitted<<" submitted="<<renderer.stats().submitted
             <<" culled="<<renderer.stats().culled<<'\n';
    assert(!renderer.mesh().overflowed && renderer.mesh().count>400 && renderer.mesh().count<Mesh::capacity);
    std::array<unsigned,static_cast<unsigned>(Part::Count)> parts{};
    for(size_t i=0;i<renderer.mesh().count;++i){
        const auto& mesh=renderer.mesh();++parts[static_cast<unsigned>(mesh.parts[i])];
        assert(std::abs(dot(mesh.normals[i],mesh.normals[i])-1)<.0001f);
        for(auto p:mesh.panels[i].point)assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z));
    }
    for(auto n:parts)assert(n>0);
    view.equipment=false;view.yaw=0;view.pitch=.04f;renderer.render(canvas,view);save("rx78-front");
    view.yaw=3.14159265f;renderer.render(canvas,view);save("rx78-rear");
    view.yaw=1.5707963f;renderer.render(canvas,view);save("rx78-side");
    view.yaw=-1.5707963f;renderer.render(canvas,view);save("rx78-other-side");
    view.yaw=-.45f;view.pitch=.75f;renderer.render(canvas,view);save("rx78-top");
    view.yaw=-.40f;view.pitch=.10f;renderer.render(canvas,view);save("rx78-unarmed");
    renderer.render(canvas,view,65);save("rx78-drag");
    renderer.render(canvas,view,100,true,true);save("rx78-gray");
    view.detail=true;renderer.render(canvas,view);save("rx78-head");
    unsigned cases=0;std::size_t cullDiff=0,buriedDiff=0,totalCull=0,totalFaces=0,coverageDiff=0,interiorDiff=0,maxDiff=0;
    std::size_t partialDiff=0;
    for(Pose pose:{Pose::Display,Pose::Salute,Pose::Saber})for(int percent:{65,90,100})for(bool equipment:{false,true})for(bool detail:{false,true})for(float pitch:{-.20f,.10f,.70f})for(int i=0;i<24;++i){
        view={float(i)*6.2831853f/24,pitch,equipment,detail,false,pose};
        renderer.render(canvas,view,percent,false,false,false);const auto reference=canvas.frame();
        renderer.render(canvas,view,percent);totalCull+=renderer.stats().culled;totalFaces+=renderer.stats().total;
        const auto optimized=canvas.frame();
        const auto edge=[&](const std::vector<uint16_t>& pixels,size_t p,bool coverage){
            const int x=int(p)%468,y=int(p)/468;
            const int radius=(100+percent-1)/percent;
            for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
                const int qx=x+dx,qy=y+dy;if(qx<0||qx>=468||qy<0||qy>=466)continue;
                const auto a=pixels[p],b=pixels[qy*468+qx];
                if(coverage?((a==0x0863)!=(b==0x0863)):a!=b)return true;
            }
            return false;
        };
        std::size_t cd=0;for(size_t p=0;p<reference.size();++p){
            if(reference[p]==optimized[p])continue;
            ++cd;const bool coverage=(reference[p]==0x0863)!=(optimized[p]==0x0863);coverageDiff+=coverage;
            if(!edge(reference,p,coverage) || !edge(optimized,p,coverage)){
                if(!interiorDiff){std::cout<<"first_interior "<<equipment<<' '<<detail<<' '<<pitch<<' '<<i<<" xy="<<p%468<<','<<p/468<<" colors="<<reference[p]<<','<<optimized[p]<<'\n';save("debug-interior");}
                ++interiorDiff;
            }
        }
        maxDiff=std::max(maxDiff,cd);
        if(cd && !cullDiff){
            std::cout<<"first_cull_case "<<equipment<<' '<<detail<<' '<<pitch<<' '<<i<<" pixels="<<cd<<'\n';
            save("debug-cull-on");renderer.render(canvas,view,percent,false,false,false);save("debug-cull-off");
        }
        cullDiff+=cd;
        renderer.render(canvas,view,percent,true,false,true);assert(!renderer.mesh().overflowed);
        std::size_t bd=0;for(size_t p=0;p<reference.size();++p)bd+=optimized[p]!=canvas.frame()[p];
        if(bd && !buriedDiff){std::cout<<"first_buried_case "<<equipment<<' '<<detail<<' '<<pitch<<' '<<i<<" pixels="<<bd<<'\n';save("debug-buried");}
        buriedDiff+=bd;
        renderer.render(canvas,view,percent,true,false,false,true);
        for(size_t p=0;p<optimized.size();++p)partialDiff+=optimized[p]!=canvas.frame()[p];
        ++cases;
    }
    std::cout<<"cases="<<cases<<" cull_pixel_diff="<<cullDiff<<" buried_pixel_diff="<<buriedDiff
             <<" coverage_diff="<<coverageDiff<<" interior_diff="<<interiorDiff<<" max_diff="<<maxDiff
             <<" partial_diff="<<partialDiff<<" culled_ratio="<<double(totalCull)/double(totalFaces)<<'\n';
    // Q13 depth and coverage epsilon can differ at a one-pixel silhouette.
    // No changed interior pixels, missing features, or buried-cap differences.
    if(interiorDiff || maxDiff>8 || buriedDiff || partialDiff)return 2;
    for(int i=0;i<10;++i){renderer.close();assert(!renderer.ready());assert(renderer.open());renderer.render(canvas,View{});}
    MuseumController tour;lets_and_go::DeviceControlFrame f;f.input.valid=true;
    tour.update(f,1000);f.autoToggle=true;tour.update(f,1000);f.autoToggle=false;
    for(int i=0;i<48;++i){tour.update(f,1000+uint32_t(i)*20000/48);renderer.render(canvas,tour.view(),90);save("tour-"+std::to_string(i));}
    std::cout<<"Geometry, bounded silhouette comparison, exact buried/partial equality, controls and reentry passed\n";
}
