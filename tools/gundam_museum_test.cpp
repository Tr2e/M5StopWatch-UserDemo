#include "../main/apps/app_gundam_museum/view/museum_renderer.h"
#include "../main/apps/app_gundam_museum/view/museum_space.h"
#include "../main/apps/app_gundam_museum/view/museum_wireframe.h"
#include "../main/apps/app_gundam_museum/controller/museum_controller.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "sd_rx78_geometry_test.h"
#include "sd_nu_geometry_test.h"
#include "sd_strike_geometry_test.h"
#include "sd_zaku_geometry_test.h"
#include "sd_sazabi_geometry_test.h"
#include "sd_destiny_geometry_test.h"
#include "sd_rx78_equipment_test.h"
using namespace gundam_museum;
void controls(){
    MuseumController c;lets_and_go::DeviceControlFrame f;f.input.valid=true;
    c.update(f,1000);assert(c.percent(1000)==100);
    f.preview={1,30,20,true,true};assert(c.update(f,1010));
    assert(std::abs(c.view().yaw+.04f)<.0001f && c.percent(1010)==65);
    f.preview={1,30,20,false,true};c.update(f,1050);
    f.preview.changed=false;c.update(f,1250);assert(c.percent(1250)==100);
    const float heldYaw=c.view().yaw;f.input.confirmPressed=true;c.update(f,1260);f.input.confirmPressed=false;assert(c.view().yaw==heldYaw);
    f.preview={1,120,40,true,true};c.update(f,1270);assert(c.view().yaw!=heldYaw);
    f.preview={2,0,300,true,true};c.update(f,1280);assert(c.view().pitch==.70f);
    f.preview.dy=299;c.update(f,1290);assert(c.view().pitch<.70f);
    f.input.valid=false;c.update(f,1300);const auto interrupted=c.view();
    f.input.valid=true;f.preview.dx=100;c.update(f,1310);assert(c.view().yaw==interrupted.yaw);
    f.preview={};f.navigation=1;
    for(ModelId expected:{ModelId::NuGundam,ModelId::Rx78}){
        c.update(f,1320);assert(c.view().model==expected && c.view().equipment && !c.view().detail);
    }
    f.navigation=-1;
    for(ModelId expected:{ModelId::NuGundam,ModelId::Rx78}){
        c.update(f,1330);assert(c.view().model==expected && c.view().equipment && !c.view().detail);
    }
    f.navigation=1;c.update(f,1340);f.navigation=0;f.input.confirmPressed=true;c.update(f,1346);
    assert(c.view().model==ModelId::NuGundam && c.view().equipment && !c.view().detail);
    f.input.confirmPressed=false;
    f.navigation=0;f.autoToggle=true;c.update(f,1350);f.autoToggle=false;
    const float start=c.view().yaw;c.update(f,2350);assert(!c.view().automatic && c.view().yaw==start);
    f.preview={3,1,0,true,true};c.update(f,2400);assert(!c.view().automatic);
    f.input.valid=false;f.input.cancelPressed=true;c.update(f,2500);assert(c.exitRequested());
    c.reset();assert(!c.exitRequested());f={};f.input.valid=true;
    c.update(f,0xfffffff0u);f.autoToggle=true;c.update(f,0xfffffff1u);f.autoToggle=false;c.update(f,30);
    assert(std::isfinite(c.view().yaw));
    // Entire touch gesture between slow render frames is delivered once.
    lets_and_go::DeviceControlLogic logic;logic.setScreen(lets_and_go::GameScreen::MuseumInspect);
    logic.presentScreen(lets_and_go::GameScreen::MuseumInspect);logic.touch(false,0,0);
    logic.touch(true,210,230);logic.touch(true,250,240);logic.touch(false,250,240);
    c.reset();assert(c.update(logic.consume(true),400));const auto moved=c.view();
    c.update(logic.consume(true),410);assert(c.view().yaw==moved.yaw);
    using lets_and_go::GameScreen;using lets_and_go::TouchAction;
    assert(lets_and_go::menuTouchTarget(GameScreen::MuseumInspect,32,233)==TouchAction::Previous);
    assert(lets_and_go::menuTouchTarget(GameScreen::MuseumInspect,436,233)==TouchAction::Next);
    for(auto p:{std::pair<int,int>{170,414},{298,414},{116,69},{123,350}})
        assert(lets_and_go::menuTouchTarget(GameScreen::MuseumInspect,p.first,p.second)==TouchAction::None);
    // Racer's original touch actions remain intact.
    assert(lets_and_go::menuTouchTarget(GameScreen::CarInspect,170,414)==TouchAction::Auto);
    assert(lets_and_go::menuTouchTarget(GameScreen::CarInspect,298,414)==TouchAction::Confirm);
    logic.touch(true,436,233);logic.touch(false,436,233);
    auto click=logic.consume(true);assert(click.navigation==1);c.update(click,500);
    assert(c.view().model==ModelId::NuGundam);
    assert(logic.consume(true).navigation==0);
    logic.touch(true,32,233);logic.touch(true,100,233);logic.touch(false,100,233);
    assert(logic.consume(true).navigation==0);
    // A drag started on the model keeps ownership while crossing an arrow.
    logic.touch(true,230,233);logic.touch(true,436,233);logic.touch(false,436,233);
    auto drag=logic.consume(true);assert(drag.preview.changed && drag.navigation==0);
    c.update(drag,600);const float yaw=c.view().yaw;c.update(logic.consume(true),10000);
    assert(c.view().yaw==yaw && !c.view().automatic);

}
int main(int argc,char** argv){
    controls();
    const std::string out=argc>1?argv[1]:"/tmp/gundam-museum";
    MuseumRenderer renderer;assert(renderer.open());
    LGFX_Sprite canvas;canvas.createSprite(468,466);
    const bool strike=argc>2 && std::string(argv[2])=="strike";
    const bool nu=argc>2 && std::string(argv[2])=="nu";
    const bool zaku=argc>2 && std::string(argv[2])=="zaku";
    const bool sazabi=argc>2 && std::string(argv[2])=="sazabi";
    const bool destiny=argc>2 && std::string(argv[2])=="destiny";
    const ModelId model=destiny?ModelId::DestinyGundam:strike?ModelId::StrikeGundam:nu?ModelId::NuGundam:zaku?ModelId::CharZaku:sazabi?ModelId::Sazabi:ModelId::Rx78;
    View view;view.model=model;
    const std::string prefix=destiny?"destiny":strike?"strike":nu?"nu":zaku?"zaku":sazabi?"sazabi":"rx78";
    const auto save=[&](const std::string& name){canvas.save(out+"/"+(name.rfind("rx78-",0)==0?prefix+name.substr(4):name)+".ppm");};
    renderer.render(canvas,view);save("rx78-equipped");
    if(nu){
        unsigned ink=0;for(auto pixel:canvas.frame())ink+=pixel==hiddenLineInk;
        assert(ink>800);
    }
    std::cout<<"working_bytes="<<renderer.workingBytes()<<" panels="<<renderer.mesh().count
             <<" omitted="<<renderer.mesh().buriedOmitted<<" submitted="<<renderer.stats().submitted
             <<" culled="<<renderer.stats().culled<<'\n';
    assert(!renderer.mesh().overflowed && renderer.mesh().count>400 && renderer.mesh().count<Mesh::capacity);
    std::array<unsigned,static_cast<unsigned>(Part::Count)> parts{};
    for(size_t i=0;i<renderer.mesh().count;++i){
        const auto& mesh=renderer.mesh();++parts[static_cast<unsigned>(mesh.parts[i])];
        assert(std::abs(dot(mesh.normals[i],mesh.normals[i])-1)<.0001f);
        assert(mesh.anchors[i].x==mesh.panels[i].point[0].x &&
               mesh.anchors[i].y==mesh.panels[i].point[0].y &&
               mesh.anchors[i].z==mesh.panels[i].point[0].z);
        for(auto p:mesh.panels[i].point)assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z));
    }
    for(unsigned i=0;i<static_cast<unsigned>(Part::Bazooka);++i)assert(parts[i]>0);
    assert((nu||destiny)?parts[static_cast<unsigned>(Part::Bazooka)]>0:parts[static_cast<unsigned>(Part::Bazooka)]==0);
    assert((strike||destiny)?parts[static_cast<unsigned>(Part::Aile)]>0:parts[static_cast<unsigned>(Part::Aile)]==0);
    assert((nu||sazabi)?parts[static_cast<unsigned>(Part::Funnels)]>0:parts[static_cast<unsigned>(Part::Funnels)]==0);
    if(!nu && !strike && !zaku && !sazabi && !destiny){checkSdRx78Geometry(renderer.mesh());sd_equipment_check::check(renderer.mesh());}
    if(nu)sd_nu_check::check(renderer.mesh());
    if(strike)sd_strike_check::check(renderer.mesh());
    if(zaku)sd_zaku_check::check(renderer.mesh());
    if(sazabi)sd_sazabi_check::check(renderer.mesh());
    if(destiny)sd_destiny_check::check(renderer.mesh());
    view.equipment=false;view.yaw=0;view.pitch=.04f;renderer.render(canvas,view);save("rx78-front");
    view.yaw=3.14159265f;renderer.render(canvas,view);save("rx78-rear");
    view.yaw=1.5707963f;renderer.render(canvas,view);save("rx78-side");
    view.yaw=-1.5707963f;renderer.render(canvas,view);save("rx78-other-side");
    view.yaw=-.45f;view.pitch=.75f;renderer.render(canvas,view);save("rx78-top");
    view.yaw=-.40f;view.pitch=.10f;renderer.render(canvas,view);save("rx78-unarmed");
    renderer.render(canvas,view,65);save("rx78-drag");
    view.equipment=true;renderer.render(canvas,view,65);save("rx78-equipped-drag");
    view.equipment=false;
    renderer.render(canvas,view,100,true,true);save("rx78-gray");
    view.detail=true;renderer.render(canvas,view);save("rx78-head");
    // Keep the original model-only silhouette/coverage gates unchanged.
    // Room composition and occlusion are checked independently below.
    renderer.setSpaceEnabled(false);
    unsigned cases=0;std::size_t cullDiff=0,buriedDiff=0,totalCull=0,totalFaces=0,coverageDiff=0,interiorDiff=0,maxDiff=0;
    std::size_t partialDiff=0,frameEdge=0,circleEdge=0;
    for(Pose pose:{Pose::Display})for(int percent:{65,90,100})for(bool equipment:{false,true})for(bool detail:{false,true})for(float pitch:{-.20f,.10f,.70f})for(int i=0;i<24;++i){
        view={float(i)*6.2831853f/24,pitch,equipment,detail,false,pose,model};
        renderer.render(canvas,view,percent,false,false,false);const auto reference=canvas.frame();
        renderer.render(canvas,view,percent);totalCull+=renderer.stats().culled;totalFaces+=renderer.stats().total;
        const auto optimized=canvas.frame();
        assert(canvas.texts.empty());
        if(equipment && !detail && percent==100)for(size_t p=0;p<optimized.size();++p){
            if(optimized[p]==0x0863)continue;
            const int x=int(p)%468,y=int(p)/468;
            if((x-234)*(x-234)+(y-233)*(y-233)>=230*230)++circleEdge;
            if((x==22 || x==445 || y==21 || y==444) && x>=22 && x<=445 && y>=21 && y<=444)++frameEdge;
        }

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
        const auto optimizedCount=renderer.mesh().count,omitted=renderer.mesh().buriedOmitted;
        renderer.render(canvas,view,percent,true,false,true);assert(!renderer.mesh().overflowed);
        assert(omitted==(nu?8u:strike?10u:(zaku||sazabi||destiny)?0u:12u));assert(renderer.mesh().count==optimizedCount+omitted);
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
    std::cout<<"full_exhibit circle_edge="<<circleEdge<<" viewport_edge="<<frameEdge<<'\n';
    assert(circleEdge==0 && frameEdge==0);
    // A cached exhibit must be replaced even when all camera/equipment fields match.
    View original;original.model=model;renderer.render(canvas,original);const auto identity=canvas.frame();
    for(ModelId id:{ModelId::Rx78,ModelId::CharZaku,ModelId::NuGundam,ModelId::Sazabi,ModelId::StrikeGundam,ModelId::DestinyGundam}){
        if(id==model)continue;
        View other=original;other.model=id;
        renderer.render(canvas,other);assert(canvas.frame()!=identity);
        renderer.render(canvas,original);assert(canvas.frame()==identity);
    }
    View moved=original;moved.yaw+=1.2f;moved.pitch=.6f;
    renderer.render(canvas,moved);const auto movedFull=canvas.frame();
    renderer.render(canvas,original);renderer.render(canvas,moved,100,true,false,false,true);
    assert(canvas.frame()==movedFull);
    for(int i=0;i<10;++i){renderer.close();assert(!renderer.ready());assert(renderer.open());renderer.render(canvas,original);assert(canvas.frame()==identity);}
    if(!strike && !zaku && !sazabi && !destiny){
        for(int percent:{65,100})for(float pitch:{-.2f,.1f,.7f}){
            View sample;sample.model=model;sample.pitch=pitch;sample.yaw=-.4f;
            renderer.setOptimizations(false);renderer.render(canvas,sample,percent);const auto reference=canvas.frame();
            renderer.setOptimizations(true);renderer.render(canvas,sample,percent);
            assert(canvas.frame()==reference);
        }
        renderer.setOptimizations(true);
    }
    LGFX_Sprite room;room.createSprite(468,466);room.fillScreen(space::background);
    View roomView;roomView.model=model;
    space::draw(room,roomView);room.save(out+"/space-empty.ppm");
    const auto defaultRoom=room.frame();
    View turnedRoom=roomView;turnedRoom.yaw+=.35f;
    room.fillScreen(space::background);space::draw(room,turnedRoom);
    // Use a non-symmetry angle: a cube legitimately repeats every quarter turn.
    assert(room.frame()!=defaultRoom);
    assert(std::abs((space::upper.x-space::lower.x)-(space::upper.y-space::lower.y))<1e-6f);
    assert(std::abs((space::upper.x-space::lower.x)-(space::upper.z-space::lower.z))<1e-6f);
    View axisView;axisView.yaw=0;axisView.pitch=0;
    MuseumCamera axisCamera(axisView);auto probe=axisCamera({1,axisCamera.pivot+2,3});
    assert(std::abs(probe.x-1)<1e-6f && std::abs(probe.y-2)<1e-6f && std::abs(probe.z-4)<1e-6f);
    axisView.yaw=1.57079632679f;MuseumCamera quarterCamera(axisView);
    probe=quarterCamera({1,quarterCamera.pivot+2,3});
    assert(std::abs(probe.x-3)<1e-5f && std::abs(probe.y-2)<1e-5f && std::abs(probe.z-8)<1e-5f);
    lets_and_go::TrackCamera lineCamera{};lineCamera.focalLength=686;lineCamera.principalX=234;lineCamera.principalY=233;
    float lx0,ly0,lx1,ly1;
    assert(!space::projectLine(axisCamera,lineCamera,{1,axisCamera.pivot,8},{0,axisCamera.pivot,9},lx0,ly0,lx1,ly1));
    assert(space::projectLine(axisCamera,lineCamera,{1,axisCamera.pivot,8},{0,axisCamera.pivot,6},lx0,ly0,lx1,ly1));
    assert(space::clipLine(lx0,ly0,lx1,ly1,468,466));
    assert(lx0>=0 && lx0<=467 && lx1>=0 && lx1<=467);
    std::size_t sceneCases=0,coveredGrid=0,visibleGrid=0,inkColoredModel=0,gutterChanges=0;
    for(int percent:{65,100})for(float pitch:{-.2f,.1f,.7f})for(int i=0;i<6;++i){
        View scene;scene.model=model;scene.pitch=pitch;scene.yaw=-.4f+i*6.2831853f/6;
        renderer.setSpaceEnabled(false);renderer.render(canvas,scene,percent);const auto flat=canvas.frame();
        renderer.setSpaceEnabled(true);renderer.render(canvas,scene,percent);const auto complete=canvas.frame();
        for(size_t i=0;i<renderer.mesh().count;++i)for(auto p:renderer.mesh().panels[i].point)assert(space::contains(p));
        room.fillScreen(space::background);space::draw(room,scene);const auto backdrop=room.frame();
        for(size_t p=0;p<flat.size();++p){
            const int x=int(p%468)-22,y=int(p/468)-layout::top;
            const int active=renderer.sampleWidth();
            const bool covered=x>=0 && x<layout::side && y>=0 && y<layout::side && active>0 &&
                renderer.modelSampleCovered((2*x+1)*active/(2*layout::side),(2*y+1)*active/(2*layout::side));
            // Nu's shaded navy can equal the ink background exactly. Depth,
            // not a color key, distinguishes opaque armor from empty space.
            const bool modelPixel=covered || flat[p]!=space::diagnosticBackground;
            inkColoredModel+=covered && flat[p]==space::diagnosticBackground;
            assert(complete[p]==(modelPixel?flat[p]:backdrop[p]));
            coveredGrid+=modelPixel && backdrop[p]!=space::background;
            visibleGrid+=!modelPixel && backdrop[p]!=space::background;
        }
        View previous=scene;previous.yaw+=.8f;previous.pitch=.6f;
        previous.model=model==ModelId::Rx78?ModelId::NuGundam:ModelId::Rx78;
        renderer.render(canvas,previous,percent==65?100:65);
        const auto beforePartial=canvas.frame();
        renderer.render(canvas,scene,percent,true,false,false,true);
        assert(canvas.frame()==complete);
        for(int y=0;y<466;++y)if(y<layout::top || y>=layout::top+layout::side)
            for(int x=0;x<468;++x)gutterChanges+=canvas.frame()[y*468+x]!=beforePartial[y*468+x];
        ++sceneCases;
    }
    assert(coveredGrid>100 && visibleGrid>1000 && gutterChanges>100);
    // Drawing the room must respect an enclosing display clip and restore it.
    room.fillScreen(0xffff);room.setClipRect(37,53,391,350);space::draw(room,roomView);
    int32_t cx,cy,cw,ch;room.getClipRect(&cx,&cy,&cw,&ch);
    assert(cx==37 && cy==53 && cw==391 && ch==350);
    for(int y=0;y<466;++y)for(int x=0;x<468;++x)
        if(x<37 || x>=428 || y<53 || y>=403)assert(room.frame()[y*468+x]==0xffff);
    std::cout<<"space_composition_cases="<<sceneCases<<" covered_grid="<<coveredGrid
             <<" visible_grid="<<visibleGrid<<" ink_colored_model="<<inkColoredModel
             <<" gutter_changes="<<gutterChanges
             <<" cube_contains_model=1 model_pixels_unchanged=1 mixed_partial_exact=1 clip_restored=1\n";
    // Offline orbit evidence samples manual camera poses; no product auto mode.
    for(int i=0;i<48;++i){View tourView;tourView.model=model;
        tourView.yaw=-.4f-float(i)*6.2831853f/48;renderer.render(canvas,tourView,100);save("tour-"+std::to_string(i));}
    std::cout<<"Geometry, bounded silhouette comparison, exact buried/partial equality, controls and reentry passed\n";
}
