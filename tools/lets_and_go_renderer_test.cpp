#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_car_pose.h"
#include "../main/apps/app_lets_and_go_racer/view/scene_edge_upscale.h"
#include "../main/apps/app_lets_and_go_racer/view/garage_car_transform.h"
#include "../main/apps/app_lets_and_go_racer/controller/car_inspection_controller.h"
#include "../main/apps/app_lets_and_go_racer/controller/inspection_presentation.h"
#include "../main/apps/app_lets_and_go_racer/input/device_control_logic.h"
#include <hal/hal.h>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <limits>

using namespace lets_and_go;

bool validateInspectionDirections(const CarSpec& spec,const CarDisplayMesh& mesh)
{
    for(int azimuth=0;azimuth<18;++azimuth) {
        CarInspectionController initial;
        initial.turn(azimuth,0);
        const auto pose=initial.state();
        const GarageCarTransform transform(spec,pose.yaw,pose.pitch,0);
        // Track the nearest authored surface point through the same transform,
        // perspective and automatic fit used by VIEW CAR. This catches a sign
        // error even if touch and stick agree with each other on the wrong sign.
        float nearest=std::numeric_limits<float>::max();
        CarPoint anchor{};uint8_t wheel=0;
        for(std::size_t panel=0;panel<mesh.count;++panel)for(auto vertex:mesh.panels[panel].point) {
            const auto point=transform(vertex,mesh.panels[panel].wheel);
            if(point.z<nearest) {nearest=point.z;anchor=vertex;wheel=mesh.panels[panel].wheel;}
        }
        const auto project=[&](GarageViewState view) {
            TrackCamera camera{};camera.principalX=234;camera.principalY=245;
            camera.focalLength=inspectionScale(spec,mesh,view.yaw,view.pitch)*5.8f;
            TrackScreenPoint point{};
            projectTrackPoint(camera,GarageCarTransform(spec,view.yaw,view.pitch,0)(anchor,wheel),point);
            return point;
        };
        const auto before=project(pose);
        for(bool vertical : {false,true})for(int direction : {-1,1})for(bool joystick : {false,true}) {
            auto moved=initial;
            if(joystick) {
                RacerScreenInput context;
                RawRacerInput raw;raw.axesValid=raw.actionsValid=true;
                context.changeScreen(RacerNavigationMode::Garage);context.presentScreen();
                for(uint32_t time=0;time<=400;time+=10) {
                    const float axis=time>=50 && time<150 ? direction*.9f : 0.f;
                    raw.steer=vertical ? 0.f : axis;raw.viewAxis=vertical ? -axis : 0.f;
                    context.publish(raw,time/10,time);
                }
                const auto input=context.consume();
                moved.turn(input.navigationStep,input.viewStep);
            } else {
                DeviceControlLogic touch;
                touch.setScreen(GameScreen::CarInspect);touch.presentScreen(GameScreen::CarInspect);
                touch.touch(false,0,0);touch.touch(true,234,230);
                touch.touch(true,234+(vertical ? 0 : direction*20),230+(vertical ? direction*20 : 0));
                touch.touch(false,0,0);
                moved.drag(touch.consume(true).preview);
            }
            const auto after=project(moved.state());
            if(direction*(vertical ? after.y-before.y : after.x-before.x)<=.1f) {
                std::cerr<<"Inspection surface opposes input: "<<spec.shortName<<" azimuth="<<azimuth
                         <<" vertical="<<vertical<<" direction="<<direction<<" joystick="<<joystick<<'\n';
                return false;
            }
        }
    }
    return true;
}

bool validateSelectionDirections(const CarSpec& spec,const CarDisplayMesh& mesh)
{
    for(int azimuth=0;azimuth<18;++azimuth) {
        GarageViewController initial;initial.reset(spec.id,0);
        initial.drag({1,azimuth*29,0,true,true},10);
        const auto pose=initial.state(10);
        const GarageCarTransform transform(spec,pose.yaw,pose.pitch,0);
        float nearest=std::numeric_limits<float>::max();CarPoint anchor{};uint8_t wheel=0;
        for(std::size_t panel=0;panel<mesh.count;++panel)for(auto vertex:mesh.panels[panel].point) {
            const auto point=transform(vertex,mesh.panels[panel].wheel);
            if(point.z<nearest) {nearest=point.z;anchor=vertex;wheel=mesh.panels[panel].wheel;}
        }
        const auto project=[&](GarageViewState view) {
            TrackCamera camera{};camera.principalX=234;camera.principalY=view.centerY;
            camera.focalLength=view.scale*5.8f;TrackScreenPoint point{};
            projectTrackPoint(camera,GarageCarTransform(spec,view.yaw,view.pitch,0)(anchor,wheel),point);
            return point;
        };
        const auto before=project(pose);
        for(bool vertical:{false,true})for(int direction:{-1,1}) {
            auto moved=initial;
            moved.drag({2,vertical ? 0 : direction*20,vertical ? direction*20 : 0,true,true},20);
            const auto after=project(moved.state(20));
            if(direction*(vertical ? after.y-before.y : after.x-before.x)<=.1f) {
                std::cerr<<"Selection surface opposes drag: "<<spec.shortName<<'\n';return false;
            }
        }
    }
    return true;
}

bool validateCarRaster()
{
    // Compact active strides, transparent scaled output and restoring the
    // native dimensions all share the production raster allocation.
    CarSurfaceRaster<8,8> scaled;
    LGFX_Sprite small,large;small.createSprite(4,4);large.createSprite(8,8);
    small.fillScreen(0x1234);large.fillScreen(0x1234);
    scaled.begin(0,0,4,4);
    scaled.triangle({0,0,1,0,0},{4,0,1,0,0},{0,4,1,0,0},0xffff,CarPaint::Solid,255);
    scaled.blit(small);scaled.blitScaled(large,0,0,8,8);
    for(int y=0;y<8;++y)for(int x=0;x<8;++x)
        if(large.frame()[y*8+x]!=small.frame()[(y/2)*4+x/2])return false;
    scaled.begin(0,0);large.fillScreen(0x1234);
    scaled.triangle({6,6,1,0,0},{8,6,1,0,0},{6,8,1,0,0},0xffff,CarPaint::Solid,255);
    scaled.blit(large);
    if(large.frame()[6*8+6]!=0xffff || large.frame()[0]!=0x1234)return false;
    // Physical rolling contract shared by garage and race: the top of each
    // wheel moves toward the nose (+z), the contact point toward the rear.
    for(uint8_t wheel=1;wheel<=4;++wheel) {
        const float axle=wheel<=2 ? kModelFrontAxle : kModelRearAxle;
        const auto top=animateCarPanelPoint({0,2*kModelWheelRadius,axle},wheel,
                                            std::cos(.1f),std::sin(.1f));
        const auto bottom=animateCarPanelPoint({0,0,axle},wheel,
                                               std::cos(.1f),std::sin(.1f));
        if(top.z<=axle || bottom.z>=axle) {
            std::cerr<<"Wheel physical rolling direction reversed\n";return false;
        }
    }
    auto atlas=std::make_unique<RacePaintAtlas>();
    for(unsigned paint=1;paint<unsigned(CarPaint::Count);++paint)
        for(auto base:{uint16_t(0xc9a7),uint16_t(0x3275)})for(auto light:{uint8_t(223),uint8_t(255)}) {
            atlas->clear();
            if(!atlas->add(CarPaint(paint),base,light))return false;
            const auto* texture=atlas->find(CarPaint(paint),base,light);
            if(!texture)return false;
            for(unsigned y=0;y<32;++y)for(unsigned x=0;x<32;++x) {
                const float u=(x+.5f)/32,v=(y+.5f)/32;
                auto expected=carPaintColor(CarPaint(paint),base,u,v);
                if(light!=255)expected=carTint(expected,light/255.f);
                if(RacePaintAtlas::sample(texture,u,v)!=expected)return false;
            }
            if(RacePaintAtlas::sample(texture,-1,-1)!=texture[0] ||
               RacePaintAtlas::sample(texture,1,2)!=texture[1023])return false;
        }
    atlas->clear();
    for(unsigned i=0;i<RacePaintAtlas::kCapacity;++i)
        if(!atlas->add(CarPaint::NeoHood,uint16_t(i),255))return false;
    if(atlas->add(CarPaint::NeoHood,65535,255) || atlas->find(CarPaint::NeoHood,65535,255))return false;
    atlas->clear();if(atlas->count()!=0 || atlas->find(CarPaint::NeoHood,0,255))return false;
    auto mesh=std::make_unique<RaceSurfaceMesh>();
    std::array<uint16_t,8192> slots{};
    for(std::size_t car=0;car<kCarCount;++car)for(auto detail:{CarSurfaceDetail::Minimal,CarSurfaceDetail::Low,CarSurfaceDetail::Medium}) {
        const auto built=buildCarSurfaceInto(static_cast<CarId>(car),mesh->panels.data(),mesh->panels.size(),detail);
        if(built.overflowed)return false;
        mesh->count=built.count;mesh->indexVertices(slots);
        for(std::size_t corner=0;corner<mesh->count*4;++corner) {
            const auto index=mesh->cornerIndex[corner];
            if(index>=mesh->vertexCount)return false;
            const auto key=mesh->vertexCorner[index];
            if(key>=mesh->count*4)return false;
            const auto& a=mesh->panels[corner/4];const auto& b=mesh->panels[key/4];
            const auto p=a.point[corner%4],q=b.point[key%4];
            if(p.x!=q.x || p.y!=q.y || p.z!=q.z || a.wheel!=b.wheel)return false;
        }
        if(detail==CarSurfaceDetail::Low)std::cout << "Race vertex cache: car=" << car
            << " corners=" << mesh->count*4 << " unique=" << mesh->vertexCount << '\n';
    }
    auto& canvas=GetHAL().getCanvas();
    CarSurfaceRaster<32,32> raster;
    if(!raster.preferInternalOcclusionRows())return false;
    const CarScreenVertex a{180,180,.5f,0,0},b{212,180,.5f,.5f,0},c{180,212,.5f,0,.5f};
    auto farA=a,farB=b,farC=c;
    farA.depth=farB.depth=farC.depth=.25f;
    bool valid=true;
    uint32_t bits=19;
    for(int i=0;i<100000;++i) {
        bits=bits*1664525u+1013904223u;
        float value;std::memcpy(&value,&bits,sizeof(value));
        if(std::isfinite(value) &&
           (rasterFloor(value)!=std::floor(value) || rasterCeil(value)!=std::ceil(value)))valid=false;
    }
    for(float value:{-8388608.f,-1.5f,-1.f,-.5f,0.f,.5f,1.f,1.5f,8388608.f}) {
        for(float v:{value,std::nextafter(value,-INFINITY),std::nextafter(value,INFINITY)})
            if(rasterFloor(v)!=std::floor(v) || rasterCeil(v)!=std::ceil(v))valid=false;
    }
    for(bool reverse : {false,true}) {
        raster.begin(180,180);
        if(reverse) raster.triangle(a,b,c,0xc9a7,CarPaint::Solid,255);
        raster.triangle(farA,farB,farC,0x3275,CarPaint::Solid,255);
        if(!reverse) raster.triangle(a,b,c,0xc9a7,CarPaint::Solid,255);
        canvas.fillScreen(0xef3a);raster.blit(canvas);
        if(canvas.frame()[185*466+185]!=0xc9a7 || raster.depthAt(5,5)!=4096)valid=false;
    }
    auto camera=makeTrackLookAtCamera({0,0,0},{0,0,1},466,466);
    camera.principalX=camera.principalY=196;camera.focalLength=32;
    PencilOcclusion occlusion;
    occlusion.append(projectPencilSurface(camera,{-1,1,1},{1,1,1},{-1,-1,1},466,466));
    canvas.fillScreen(0xef3a);raster.blit(canvas,&occlusion);
    const auto nativeOcclusion=canvas.frame();
    // No optional allocation: filtering must degrade to the reference path.
    CarSurfaceRaster<32,32> fallback;
    fallback.begin(180,180);fallback.triangle(a,b,c,0xc9a7,CarPaint::Solid,255);
    canvas.fillScreen(0xef3a);fallback.blit(canvas,&occlusion);
    if(nativeOcclusion!=canvas.frame())valid=false;
    auto smallOcclusion=std::make_unique<PencilOcclusion>();
    smallOcclusion->count=occlusion.count;
    for(std::size_t i=0;i<occlusion.count;++i) {
        auto face=occlusion.surfaces[i];
        for(auto& p:face.points){p.x*=.5f;p.y*=.5f;}
        face.minX*=.5f;face.maxX*=.5f;face.minY*=.5f;face.maxY*=.5f;
        face.inverseDepth.x*=2;face.inverseDepth.y*=2;
        smallOcclusion->surfaces[i]=face;
    }
    canvas.fillScreen(0xef3a);raster.blit(canvas,smallOcclusion.get(),.5f);
    if(nativeOcclusion!=canvas.frame())valid=false;
    if(canvas.frame()[185*466+185]!=0xef3a)valid=false;
    occlusion.count=0;
    occlusion.append(projectPencilSurface(camera,{-3,3,3},{3,3,3},{-3,-3,3},466,466));
    canvas.fillScreen(0xef3a);raster.blit(canvas,&occlusion);
    if(canvas.frame()[185*466+185]!=0xc9a7)valid=false;
    raster.begin(180,180);
    raster.cameraTriangle(camera,{-.2f,.2f,.1f,0,0},{.4f,.2f,1,1,0},
                          {-.2f,-.4f,1,0,1},0xc9a7,CarPaint::Solid,255);
    int painted=0;
    for(int y=0;y<32;++y)for(int x=0;x<32;++x) {
        if(raster.depthAt(x,y))++painted;
        if(raster.depthAt(x,y)>40960)valid=false;
    }
    if(!painted)valid=false;
    // Tile boundaries must not change depth/UV evaluation or cut a close car.
    CarSurfaceRaster<64,32> wide;
    const CarScreenVertex d{244,200,.38f,.38f,.38f};
    wide.begin(180,180);wide.triangle(a,b,d,0xf7be,CarPaint::MagnumHood,255);
    canvas.fillScreen(0xef3a);wide.blit(canvas);
    const auto whole=canvas.frame();
    canvas.fillScreen(0xef3a);
    for(int x : {180,212}) {
        raster.begin(x,180);raster.triangle(a,b,d,0xf7be,CarPaint::MagnumHood,255);raster.blit(canvas);
    }
    if(canvas.frame()!=whole)valid=false;
    uint32_t random=7;
    const auto value=[&] {random=random*1664525u+1013904223u;return (random&65535u)/65535.f;};
    for(int i=0;i<1000;++i) {
        raster.begin(180,180);
        const auto vertex=[&] {return CarSurfaceVertex{value()*4-2,value()*4-2,value()*2-.2f,value(),value()};};
        const auto p=vertex(),q=vertex(),r=vertex();
        raster.cameraTriangle(camera,p,q,r,0xf7be,CarPaint::MagnumHood,255);
    }
    // Exercise the full candidate array, including inclusive scanline bounds.
    const auto retainedSurface=occlusion.surfaces[0];
    for(float edge:{185.5f,std::nextafter(185.5f,-INFINITY),std::nextafter(185.5f,INFINITY)}) {
        occlusion.count=PencilOcclusion::kCapacity;
        for(auto& surface:occlusion.surfaces) {surface=retainedSurface;surface.minY=edge;surface.maxY=195.5f;}
        raster.begin(180,180);raster.triangle(a,b,c,0xc9a7,CarPaint::Solid,255);
        canvas.fillScreen(0xef3a);raster.blit(canvas,&occlusion,1.f,false);
        const auto reference=canvas.frame();
        canvas.fillScreen(0xef3a);raster.blit(canvas,&occlusion,1.f,true);
        if(reference!=canvas.frame()) {std::cerr<<"Full-capacity row bounds mismatch\n";valid=false;}
    }
    for(int sample=0;sample<240;++sample) {
        raster.begin(180,180);
        raster.triangle(a,b,c,0xc9a7,CarPaint::Solid,255);
        occlusion.count=0;
        for(int i=0;i<20;++i) {
            const auto point=[&] {return TrackVec3{value()*6-3,value()*6-3,value()*4+.21f};};
            const auto p=point(),q=point(),r=point();
            occlusion.append(projectPencilSurface(camera,p,q,r,466,466));
        }
        for(float scale:{1.f,.5f}) {
            canvas.fillScreen(0xef3a);raster.blit(canvas,&occlusion,scale,false);
            const auto reference=canvas.frame();
            canvas.fillScreen(0xef3a);raster.blit(canvas,&occlusion,scale,true);
            if(reference!=canvas.frame()) {std::cerr<<"Random row occlusion mismatch\n";valid=false;}
        }
    }
    PreparedCarPanel prepared;
    for(int i=0;i<120;++i) {
        CarPanel face{};face.color=0xf7be;face.paint=CarPaint::MagnumHood;
        face.light=196;face.u1=face.v1=255;
        // Reuse one cache slot across projected, clipped and fully hidden faces.
        for(auto& p:face.point)p={value()*2-1,value()*2-1,
            i%3==0 ? 1+value() : i%3==1 ? value()*.6f-.2f : -.1f};
        const auto identity=[](CarPoint p,uint8_t) {return p;};
        prepareCarPanel(prepared,camera,face,identity);
        wide.begin(180,180);wide.panel(camera,face,identity);
        canvas.fillScreen(0xef3a);wide.blit(canvas);
        const auto expected=canvas.frame();
        canvas.fillScreen(0xef3a);
        for(int x:{180,212}) {
            raster.begin(x,180);raster.preparedPanel(camera,prepared);raster.blit(canvas);
            for(int y=0;y<32;++y)for(int dx=0;dx<32;++dx)
                if(raster.depthAt(dx,y)!=wide.depthAt(x-180+dx,y))valid=false;
        }
        if(canvas.frame()!=expected)valid=false;
    }
    std::cout << "Solid car raster: depth ordering, bridge visibility, near clipping, UV tiles, 1000 fuzz triangles and 120 prepared-panel comparisons\n";
    if(!valid)std::cerr << "Solid car raster regression\n";
    return valid;
}

bool validateTwistedTrackDecks()
{
    auto& canvas=GetHAL().getCanvas();canvas.createSprite(234,233);
    auto surfaces=std::make_unique<PencilOcclusion>();
    bool valid=true;int cases=0,maxMissing=0;
    // Independent reference: draw BOTH deck faces and let depth resolve them.
    // It makes no top/bottom decision, so it catches a shared-normal regression.
    for(auto id:{TrackId::SkyLoop,TrackId::TriCross,TrackId::GrandSpiral}) {
        OverpassTrack road(id);PencilTrack track;track.open(road);
        const int begin=id==TrackId::GrandSpiral ? 252 : id==TrackId::TriCross ? 88 : 0;
        const int end=id==TrackId::GrandSpiral ? 280 : id==TrackId::TriCross ? 106 : 25;
        for(int frame=begin;frame<=end;++frame)for(float lane:{-1.36f,0.f,1.36f}) {
            const auto camera=makeRacerChaseCamera(road.sample(road.length()*frame/720),lane,234,233);
            canvas.fillScreen(track_paint::floor);
            drawPencilTrack(canvas,camera,track,PencilDetail::Low,surfaces.get(),false);
            const auto actual=canvas.frame();
            surfaces->count=0;surfaces->overflowed=false;
            const auto quad=[&](TrackVec3 a,TrackVec3 b,TrackVec3 c,TrackVec3 d,uint16_t color) {
                for(auto triangle:{std::array<TrackVec3,3>{a,b,c},std::array<TrackVec3,3>{a,c,d}}) {
                    auto face=projectPencilSurface(camera,triangle[0],triangle[1],triangle[2],234,233);
                    face.color=color;surfaces->append(face);
                }
            };
            for(std::size_t i=0;i<track.count;++i) {
                auto a=track.left[i],b=track.right[i],c=track.right[i+1],d=track.left[i+1];
                const auto paint=track_paint::module(i,track.count);
                const TrackVec3 drop{0,-track_paint::deckThickness,0},lift{0,track_paint::wallHeight,0};
                for(auto edge:{std::array<TrackVec3,2>{a,d},std::array<TrackVec3,2>{b,c}})
                    quad(trackAdd(edge[0],drop),trackAdd(edge[1],drop),trackAdd(edge[1],lift),trackAdd(edge[0],lift),paint.wall);
                quad(a,b,c,d,paint.deck);
                quad(trackAdd(a,drop),trackAdd(b,drop),trackAdd(c,drop),trackAdd(d,drop),track_paint::underside);
            }
            if(surfaces->overflowed)valid=false;
            canvas.fillScreen(track_paint::floor);surfaces->paint(canvas);
            int missing=0;
            for(int y=80;y<225;++y)for(int x=15;x<220;++x) {
                const auto i=y*234+x;
                if(actual[i]==track_paint::floor && canvas.frame()[i]!=track_paint::floor)++missing;
            }
            // At most a single edge-rounding pixel was seen in the dense scan;
            // the old path loses 283 pixels in one triangle at Grand Spiral 265.
            if(missing>2) {std::cerr<<"Twisted road hole: track="<<int(id)<<" frame="<<frame<<" pixels="<<missing<<'\n';valid=false;}
            maxMissing=std::max(maxMissing,missing);++cases;
        }
    }
    canvas.createSprite(466,466);
    std::cout<<"Twisted deck coverage: "<<cases<<" poses, max missing edge pixels="<<maxMissing<<'\n';
    return valid;
}

bool validateTrackPaint(TrackId id = TrackId::SkyLoop)
{
    OverpassTrack course(id);
    PencilTrack geometry;
    geometry.open(course);
    PencilOcclusion occlusion;
    auto& canvas=GetHAL().getCanvas();
    bool valid=true;
    // Continuous surfaces must be independent of painter order. These planes
    // intersect on screen, reproducing a raised wall crossing adjacent decks.
    auto testCamera=makeTrackLookAtCamera({0,0,0},{0,0,1},466,466);
    testCamera.principalX=testCamera.principalY=200;testCamera.focalLength=80;
    auto far=projectPencilSurface(testCamera,{-1,1,3},{1,1,3},{0,-1,3},466,466);
    auto near=projectPencilSurface(testCamera,{-.6f,.6f,1},{.6f,.6f,1},{0,-.6f,1},466,466);
    far.color=track_paint::blue;near.color=track_paint::coral;
    canvas.fillScreen(track_paint::night);
    occlusion.append(far);occlusion.append(near);occlusion.paint(canvas);
    const auto ordered=canvas.frame();
    occlusion.count=0;occlusion.append(near);occlusion.append(far);
    canvas.fillScreen(track_paint::night);occlusion.paint(canvas);
    if(canvas.frame()!=ordered || canvas.frame()[200*466+200]!=track_paint::coral ||
       canvas.frame()[25*466+25]!=track_paint::night) {
        std::cerr << "Track scanline depth order failed\n";valid=false;
    }
    // The projection/color cache is shared with map markers and remains inside
    // the circular instrument, including the four-pixel player locator.
    TrackMiniMap map;map.open(geometry);
    // Compare against an actual elevated camera basis, not a second copy of
    // the map formula. East is right, forward (+z) is up, height is up.
    const auto topCamera=makeTrackLookAtCamera({0,100,-25},{0,0,0},466,466);
    const auto origin=map.project({0,0,0});
    for(auto axis : {TrackVec3{4,0,0},TrackVec3{0,0,4},TrackVec3{0,4,0}}) {
        const auto projected=map.project(axis);
        const auto delta=trackSubtract(axis,TrackVec3{});
        if((projected.x-origin.x)*trackDot(delta,topCamera.right)<0 ||
           ((projected.y-origin.y)*(-trackDot(delta,topCamera.up))<=0 && axis.x==0)) {
            std::cerr<<"Minimap camera basis is reflected\n";valid=false;
        }
    }
    // The locator and ribbon must advance together around either course,
    // including the finish-line wrap, with the same tangent as the 3D world.
    for(int i=0;i<720;++i) {
        const float s=course.length()*i/720;
        const auto frame=course.sample(s);
        const auto a=map.project(frame.center);
        const auto b=map.project(course.sample(s+.8f).center);
        const float dx=trackDot(frame.tangent,topCamera.right);
        const float dy=-trackDot(frame.tangent,topCamera.up);
        if((b.x-a.x)*dx+(b.y-a.y)*dy<-.05f) {
            std::cerr<<"Minimap travel opposes track tangent\n";valid=false;
        }
    }
    canvas.fillScreen(track_paint::night);map.draw(canvas);
    for(std::size_t index=0;index<=map.count;++index)for(auto p:{map.section[index].left,map.section[index].right}) {
        const int x=p.x-TrackMiniMap::centerX,y=p.y-TrackMiniMap::centerY;
        if(x*x+y*y>(TrackMiniMap::radius-4)*(TrackMiniMap::radius-4)) {
            std::cerr << "Map point outside locator-safe radius: " << x << ',' << y << '\n';valid=false;
        }
    }
    for(int i=0;i<720;++i) {
        const auto p=map.project(course.sample(course.length()*i/720).center);
        if(std::hypot(float(p.x-TrackMiniMap::centerX),float(p.y-TrackMiniMap::centerY))>TrackMiniMap::radius-4) {
            std::cerr << "player marker outside minimap\n";valid=false;
        }
    }
    for(auto color:{track_paint::road,track_paint::coral,track_paint::blue})
        if(std::count(canvas.frame().begin(),canvas.frame().end(),color)<8) {
            std::cerr << "Map deck color missing: " << color << '\n';valid=false;
        }
    for(auto detail:{PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
        track_paint::backdrop(canvas,detail);
        if(canvas.frame()[50*466+233]!=track_paint::night ||
           canvas.frame()[400*466+233]!=track_paint::floor)valid=false;
    }
    std::size_t maxSurfaces=0;
    for(auto detail : {PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
        for(int sample=0;sample<48;++sample) {
            const auto frame=course.sample(course.length()*sample/48);
            const auto camera=makeRacerChaseCamera(frame,0,466,466);
            canvas.fillScreen(0xef3a);
            drawPencilTrack(canvas,camera,geometry,detail,&occlusion);
            if(id==TrackId::GrandSpiral) {
                const auto culled=canvas.frame();
                const auto count=occlusion.count;
                geometry.cullSegments=false;canvas.fillScreen(0xef3a);
                drawPencilTrack(canvas,camera,geometry,detail,&occlusion);
                geometry.cullSegments=true;
                if(canvas.frame()!=culled || count!=occlusion.count) {
                    std::cerr<<"Conservative cull changed visible geometry\n";valid=false;
                }
            }
            maxSurfaces=std::max(maxSurfaces,occlusion.count);
            if(occlusion.overflowed || occlusion.count==0 ||
               occlusion.count>PencilOcclusion::kCapacity) valid=false;
            for(std::size_t i=0;i<occlusion.count;++i)
                if(occlusion.surfaces[i].count<3) valid=false;
            const auto& pixels=canvas.frame();
            if(std::count_if(pixels.begin(),pixels.end(),[](uint16_t c) {
                   return c==track_paint::road || c==track_paint::blue || c==track_paint::coral;
               })<100)
                valid=false;
            const auto solidSurfaces=occlusion.surfaces;
            const auto solidCount=occlusion.count;
            canvas.fillScreen(0xef3a);
            drawPencilTrack(canvas,camera,geometry,detail,&occlusion,false,true);
            if(occlusion.overflowed || solidCount!=occlusion.count)valid=false;
            // Track style must preserve every visibility plane queried by the
            // frozen car renderer, including bridge undersides and wall edges.
            for(std::size_t f=0;f<solidCount;++f) {
                const auto& x=solidSurfaces[f];const auto& y=occlusion.surfaces[f];
                if(x.count!=y.count || x.inverseDepth.x!=y.inverseDepth.x ||
                   x.inverseDepth.y!=y.inverseDepth.y || x.inverseDepth.z!=y.inverseDepth.z)
                    valid=false;
                for(std::size_t p=0;p<x.count;++p)
                    if(x.points[p].x!=y.points[p].x || x.points[p].y!=y.points[p].y)valid=false;
            }
        }
    }
    // Actual bridge underside (not an arbitrary triangle) must occlude a line
    // on the upper deck while leaving a lower, camera-side line visible.
    const auto& a=geometry.left[0];
    const auto& b=geometry.right[0];
    const auto midpoint=track_paint::mix(a,b,.5f);
    const auto camera=makeTrackLookAtCamera({midpoint.x,midpoint.y-2,midpoint.z},
        {midpoint.x+.001f,midpoint.y,midpoint.z+.001f},466,466);
    canvas.fillScreen(0xef3a);
    drawPencilTrack(canvas,camera,geometry,PencilDetail::High,&occlusion);
    if(occlusion.overflowed) valid=false;
    bool hidden=false;
    const TrackVec3 upper=track_paint::mix(midpoint,
        track_paint::mix(geometry.left[1],geometry.right[1],.5f),.2f);
    TrackScreenPoint screen{};
    const auto point=trackToCamera(camera,upper);
    if(!projectTrackPoint(camera,point,screen)) valid=false;
    for(std::size_t i=0;i<occlusion.count;++i) {
        float enter=0,leave=0;
        if(pencilHiddenInterval(occlusion.surfaces[i],screen,{screen.x+1,screen.y},
                                1/point.z,1/point.z,enter,leave)) hidden=true;
    }
    if(!hidden) valid=false;
    TrackVec3 lower=upper;
    lower.y-=.6f;
    const auto lowerCamera=trackToCamera(camera,lower);
    TrackScreenPoint lowerScreen{};
    if(!projectTrackPoint(camera,lowerCamera,lowerScreen)) valid=false;
    for(std::size_t i=0;i<occlusion.count;++i) {
        float enter=0,leave=0;
        if(pencilHiddenInterval(occlusion.surfaces[i],lowerScreen,
                               {lowerScreen.x+1,lowerScreen.y},1/lowerCamera.z,
                               1/lowerCamera.z,enter,leave)) valid=false;
    }
    // Explicit capacity guard must flag overflow without indexing past storage.
    const auto face=occlusion.surfaces[0];
    occlusion.count=occlusion.surfaces.size();
    occlusion.append(face);
    if(!occlusion.overflowed || occlusion.count!=occlusion.surfaces.size()) valid=false;
    std::cout << "Track paint: order-independent depth, dark backdrop, active-section map, 144 poses, max occluders="
              << maxSurfaces << '\n';
    if(!valid) std::cerr << "Track paint geometry, palette or bridge occlusion regressed\n";
    return valid;
}

void captureCarStructures(const std::string& directory)
{
    // Inspection cameras use the production mesh/material/raster directly.
    // These views are QA artifacts, not extra in-game cameras or concept art.
    auto raster=std::make_unique<CarSurfaceRaster<466,466>>();
    auto mesh=std::make_unique<CarDisplayMesh>();
    auto& canvas=GetHAL().getCanvas();
    struct View {const char* name;TrackVec3 eye;CarSurfaceDetail detail;};
    const char* names[]={"magnum","sonic","neo","brocken","cobra","spider","stinger","diospada"};
    for(unsigned car=0;car<kCarCount;++car) for(const auto view : {
        View{"top",{0,3.2f,.001f},CarSurfaceDetail::High},
        View{"side",{3.1f,.75f,0},CarSurfaceDetail::High},
        View{"front",{0,1.4f,3.1f},CarSurfaceDetail::High},
        View{"rear",{1.8f,1.6f,-2.6f},CarSurfaceDetail::High},
        View{"opposite",{-1.8f,1.7f,2.6f},CarSurfaceDetail::High},
        View{"medium",{1.8f,1.7f,2.6f},CarSurfaceDetail::Medium},
        View{"low",{1.8f,1.7f,2.6f},CarSurfaceDetail::Low}}) {
        canvas.fillScreen(0xef3a);
        const auto camera=makeTrackLookAtCamera(view.eye,{0,.20f,0},466,466,1.02f);
        buildCarDisplayMesh(static_cast<CarId>(car),*mesh,view.detail);
        raster->begin(0,0);
        for(std::size_t i=0;i<mesh->count;++i)
            raster->panel(camera,mesh->panels[i],[&](CarPoint p,uint8_t wheel) {
                p=animateCarPanelPoint(p,wheel,.8f,.6f);
                p=carPointInTrackBasis(p);
                return trackToCamera(camera,{p.x,p.y,p.z});
            });
        raster->blit(canvas);
        canvas.save(directory+"/"+names[car]+"-"+view.name+".ppm");
    }
}

bool validateRaceCarRoadContact()
{
    float legacyPenetration=0,newMinimumClearance=1e9f,maxSupportLift=0,maxAttitudeStep=0;
    std::size_t poses=0;
    for(auto id:{TrackId::SkyLoop,TrackId::TriCross,TrackId::GrandSpiral}) {
        OverpassTrack track(id);PencilTrack road;road.open(track);
        for(int sample=0;sample<720;++sample) for(float lane:{-1.36f,0.f,1.36f})
            for(float heading:{-.2f,0.f,.2f}) {
                RaceCarSnapshot car;car.active=true;car.motion.distance=track.length()*sample/720;
                car.motion.lateralOffset=lane;car.motion.headingOffset=heading;
                const auto pose=makeRaceCarPose(track,road,car);
                maxSupportLift=std::max(maxSupportLift,pose.supportLift);

                const auto frame=track.sample(car.motion.distance);
                auto legacyBase=trackAdd(frame.center,trackScale(frame.lateral,lane));
                legacyBase.y+=std::sin(frame.bankRadians)*lane;
                const auto right=trackNormalize(trackAdd(frame.lateral,{0,std::sin(frame.bankRadians),0}));
                auto forward=trackNormalize(trackSubtract(frame.tangent,
                    trackScale(right,trackDot(frame.tangent,right))));
                const auto up=trackNormalize(trackCross(forward,right));
                const float cosine=std::cos(heading),sine=std::sin(heading);
                const auto legacyLateral=trackAdd(trackScale(right,cosine),trackScale(forward,-sine));
                const auto legacyForward=trackAdd(trackScale(forward,cosine),trackScale(right,sine));

                for(float modelX:{-.553f,.553f}) for(float modelZ:{kModelRearAxle,kModelFrontAxle}) {
                    const float x=-modelX*kRaceCarWorldScale,z=modelZ*kRaceCarWorldScale;
                    const float longitudinal=-x*sine+z*cosine;
                    const float lateral=x*cosine+z*sine;
                    const auto support=road.roadPoint(track,car.motion.distance+longitudinal,lane+lateral);
                    auto oldContact=trackAdd(legacyBase,trackAdd(trackScale(legacyLateral,x),
                        trackScale(legacyForward,z)));
                    oldContact=trackAdd(oldContact,trackScale(up,kRaceCarRoadClearance));
                    legacyPenetration=std::max(legacyPenetration,
                        trackDot(trackSubtract(support,oldContact),up));

                    const auto contact=raceCarPointToWorld({modelX,0,modelZ},pose);
                    newMinimumClearance=std::min(newMinimumClearance,
                        trackDot(trackSubtract(contact,support),pose.up));
                }
                ++poses;
            }
        // At the fastest tuned speed a 30 FPS frame advances about 0.8 world
        // units. The visual attitude must follow the continuous course frame
        // instead of snapping to each 1-2 unit road triangle.
        const int frameCount=std::max(1,int(std::ceil(track.length()/.8f)));
        for(float lane:{-1.36f,0.f,1.36f}) for(float heading:{-.2f,0.f,.2f}) {
            RaceCarPose previous{};bool first=true;
            for(int frameIndex=0;frameIndex<frameCount;++frameIndex) {
                RaceCarSnapshot car;car.active=true;
                car.motion.distance=track.length()*frameIndex/frameCount;
                car.motion.lateralOffset=lane;car.motion.headingOffset=heading;
                const auto pose=makeRaceCarPose(track,road,car);
                if(!first) {
                    const float dot=std::clamp(trackDot(previous.up,pose.up),-1.f,1.f);
                    maxAttitudeStep=std::max(maxAttitudeStep,std::acos(dot));
                }
                previous=pose;first=false;
            }
        }
    }
    std::cout<<"Road contact: "<<poses<<" poses, legacy penetration="<<legacyPenetration
             <<", minimum clearance="<<newMinimumClearance<<", max support lift="<<maxSupportLift
             <<", max 0.8-unit attitude step="<<maxAttitudeStep*57.2957795f<<"deg\n";
    if(newMinimumClearance<kRaceCarRoadClearance-.00002f ||
       maxSupportLift>.10f || maxAttitudeStep>.21f || !std::isfinite(newMinimumClearance)) {
        std::cerr<<"Race car entered the road or snapped between rendered triangles\n";return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    const std::string directory = argc > 1 ? argv[1] : "/tmp/lets-go-frames";
    std::filesystem::create_directories(directory);
    captureCarStructures(directory);
    auto& canvas = GetHAL().getDisplay();
    bool valid = validateRaceCarRoadContact() && validateTwistedTrackDecks() && validateCarRaster() &&
                 validateTrackPaint() && validateTrackPaint(TrackId::TriCross) &&
                 validateTrackPaint(TrackId::GrandSpiral);
    const auto checkText = [&] {
        for (const auto& label : canvas.texts) {
            const float halfWidth = label.value.size() * 3.0f * label.size;
            const float halfHeight = 4.0f * label.size;
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                const float x = label.x + sx * halfWidth - 233;
                const float y = label.y + sy * halfHeight - 233;
                if (x * x + y * y > 225 * 225) {
                    std::cerr << "Text outside round-screen safe area: " << label.value << '\n';
                    valid = false;
                }
            }
            if (label.value.find("DEVELOPMENT") != std::string::npos) valid = false;
        }
    };
    const auto save = [&](const std::string& name) {
        checkText(); canvas.save(directory + "/" + name + ".ppm");
    };

    const TrackCamera testCamera = makeTrackLookAtCamera({0, 0, 0}, {0, 0, 1}, 466, 466);
    const auto testSurface = projectPencilSurface(testCamera, {-1,-1,2}, {1,-1,2}, {0,1,2}, 466, 466);
    float enter = 0, leave = 0;
    if (!pencilHiddenInterval(testSurface, {0,233}, {466,233}, 0.25f, 0.25f, enter, leave) ||
        !(enter > 0 && enter < 0.5f && leave > 0.5f && leave < 1) ||
        pencilHiddenInterval(testSurface, {0,233}, {466,233}, 1.0f, 1.0f, enter, leave)) {
        std::cerr << "Bridge occlusion ignored perspective depth or visible line fragments\n";
        valid = false;
    }
    const auto reversed = projectPencilSurface(testCamera, {0,1,2}, {1,-1,2}, {-1,-1,2}, 466, 466);
    if (!pencilHiddenInterval(reversed, {0,233}, {466,233}, 0.25f, 0.25f, enter, leave)) valid = false;
    uint32_t random = 1u;
    const auto coordinate = [&] {
        random = random * 1664525u + 1013904223u;
        return static_cast<float>(random & 0xffffu) / 65535.0f * 80.0f - 40.0f;
    };
    for (int i = 0; i < 8000; ++i) {
        const TrackVec3 a{coordinate(), coordinate(), coordinate()};
        const TrackVec3 b{coordinate(), coordinate(), coordinate()};
        const TrackVec3 c{coordinate(), coordinate(), coordinate()};
        const auto surface = projectPencilSurface(testCamera, a, b, c, 466, 466);
        for (std::size_t p = 0; p < surface.count; ++p) {
            const auto point = surface.points[p];
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < -0.01f ||
                point.y < -0.01f || point.x > 465.01f || point.y > 465.01f) valid = false;
        }
    }
    GarageRenderer garage;
    garage.open(466, 466);
    GameFlow flow;
    GarageSelection selection;
    garage.render(flow, selection, 0u, PencilDetail::High);
    save("input");
    garage.render(flow, selection, 0, PencilDetail::High, {}, {}, true);
    save("device-input");
    flow.confirmInputAvailable();
    RacerInputStatus status;
    status.axesConnected = status.actionsConfigured = true;
    status.readiness = RacerInputReadiness::Calibrating;
    status.calibrationProgress = 0.60f;
    garage.render(flow, selection, 0u, PencilDetail::High, status);
    save("calibration");
    flow.completeCalibration(true);
    for (std::size_t i = 0; i < kCarCount; ++i) {
        selection.reset(static_cast<CarId>(i));
        garage.render(flow, selection, 500u, PencilDetail::High);
        const auto& spec = carSpec(static_cast<CarId>(i));
        const auto& pixels = canvas.frame();
        const auto accentPixels=spec.id==CarId::BrockenGigant
            ? std::count_if(pixels.begin(),pixels.end(),[](uint16_t c) {
                return (c&31)>((c>>11)&31)*1.4f && (c&31)>8;
              })
            : std::count(pixels.begin(),pixels.end(),spec.accentColor);
        if (accentPixels < (spec.id==CarId::BrockenGigant ? 80 : 10) ||
            std::count(pixels.begin(), pixels.end(), spec.wheelColor) < 10) {
            std::cerr << spec.shortName << ": official accent/wheel color missing from garage\n";
            valid = false;
        }
        save("car-" + std::to_string(i));
    }
    garage.render(flow, selection, 500u, PencilDetail::High, {}, {}, true);
    save("device-car");
    // Compare cached selection geometry with the old renderer, then full and
    // partial redraws across drag, return, view switches and moving wheels.
    for(unsigned car=0;car<kCarCount;++car) {
        const auto id=static_cast<CarId>(car);selection.reset(id);
        GarageViewController preview;preview.reset(id,0);
        const auto region=selectionRefreshRegion(canvas.width());
        garage.render(flow,selection,0,PencilDetail::High,{},preview.state(0),true);
        auto preceding=canvas.frame();
        for(int sample=0;sample<20;++sample) {
            const uint32_t now=1000+sample*100;
            if(sample==1)preview.drag({1,80,40,true,true},now);
            if(sample==2)preview.drag({1,180,-20,true,true},now);
            if(sample==3)preview.endDrag(now);
            if(sample==8 || sample==14)preview.changeView(1,now);
            const auto pose=preview.state(now);
            garage.setSelectionOptimizations(false);
            garage.render(flow,selection,now,PencilDetail::High,{},pose,true);
            const auto legacy=canvas.frame();
            garage.setSelectionOptimizations(true);
            garage.render(flow,selection,now,PencilDetail::High,{},pose,true);
            if(canvas.frame()!=legacy) {std::cerr<<"Selection shared vertices changed native pixels\n";return 1;}
            if(sample==2)save("selection-"+std::to_string(car)+"-native-drag");
            std::memcpy(canvas.getBuffer(),preceding.data(),preceding.size()*sizeof(uint16_t));
            garage.render(flow,selection,now,PencilDetail::High,{},pose,true,preview.renderPercent(now),true);
            const auto partial=canvas.frame();
            garage.render(flow,selection,now,PencilDetail::High,{},pose,true,preview.renderPercent(now),false);
            if(canvas.frame()!=partial) {std::cerr<<"Selection partial redraw mismatch\n";return 1;}
            for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x)
                if(!region.contains(x,y) && canvas.frame()[y*canvas.width()+x]!=preceding[y*canvas.width()+x]) {
                    std::cerr<<"Selection update escaped refresh region\n";return 1;
                }
            if(sample==2 || sample==4 || sample==7)save("selection-"+std::to_string(car)+"-phase-"+std::to_string(sample));
            preceding=canvas.frame();preview.presented(now);
        }
        // A cached mesh from a different car must reject partial background reuse.
        selection.reset(static_cast<CarId>((car+1)%kCarCount));
        garage.render(flow,selection,4000,PencilDetail::High,{},preview.state(4000),true,100,true);
        const auto switched=canvas.frame();
        garage.render(flow,selection,4000,PencilDetail::High,{},preview.state(4000),true,100,false);
        if(switched!=canvas.frame()) {std::cerr<<"Selection car switch reuse mismatch\n";return 1;}
    }
    // Actual production cameras, not just the separate model-inspection views.
    // Check every panel through all presets/transitions and all quality tiers.
    auto framingMesh=std::make_unique<CarDisplayMesh>();
    auto fittingCache=std::make_unique<GarageInspectionCache>();
    for(unsigned car=0;car<kCarCount;++car) for(auto quality:
        {PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
        const auto id=static_cast<CarId>(car);selection.reset(id);
        buildCarDisplayMesh(id,*framingMesh,quality==PencilDetail::High ? CarSurfaceDetail::High :
                            quality==PencilDetail::Medium ? CarSurfaceDetail::Medium : CarSurfaceDetail::Low);
        GarageViewController motion;motion.reset(id,0);
        if(quality==PencilDetail::High) {
            fittingCache->indexed=false;
            if(!validateInspectionDirections(carSpec(id),*framingMesh) ||
               !validateSelectionDirections(carSpec(id),*framingMesh))return 1;
            if(!flow.inspectCar())return 1;
            garage.render(flow,selection,1000,PencilDetail::High);
            if(car==0) {
                canvas.texts.clear();
                garage.render(flow,selection,1000,PencilDetail::Low,{},inspectionDefaultPose(),true,65,false,85,true);
                bool title=false,button=false;
                for(const auto& text:canvas.texts) {
                    title|=text.value=="HIGH DETAIL / AUTO TOUR";
                    button|=text.value=="AUTO ON";
                }
                if(!title || !button || garage.inspectionPercent()!=90) {
                    std::cerr<<"Inspection auto did not enforce high-detail 90% presentation\n";return 1;
                }
                save("inspection-auto");
            }
            garage.render(flow,selection,1000,PencilDetail::High);
            const auto fixedUI=canvas.frame();
            const auto refresh=inspectionRefreshRegion(canvas.width());
            for(int azimuth=0;azimuth<16;++azimuth)for(int elevation=0;elevation<5;++elevation) {
                GarageViewState pose{};
                pose.yaw=azimuth*6.2831853f/16;pose.pitch=.12f+elevation*1.4507963f/4;
                pose.scale=inspectionScale(carSpec(id),*framingMesh,pose.yaw,pose.pitch);
                if(fittingCache->fit(carSpec(id),*framingMesh,pose.yaw,pose.pitch)!=pose.scale) {
                    std::cerr<<"Cached inspection fit changed\n";return 1;
                }
                TrackCamera camera{};camera.principalX=234;camera.principalY=245;camera.focalLength=pose.scale*5.8f;
                const GarageCarTransform transform(carSpec(id),pose.yaw,pose.pitch,0);
                for(std::size_t panel=0;panel<framingMesh->count;++panel)for(auto vertex:framingMesh->panels[panel].point) {
                    TrackScreenPoint point{};
                    if(!projectTrackPoint(camera,transform(vertex,framingMesh->panels[panel].wheel),point) ||
                        point.x<69 || point.x>399 || point.y<110 || point.y>360) {
                        std::cerr<<"Inspection fit failed car="<<car<<'\n';return 1;
                    }
                }
                for(int percent:{77,80,100}) {
                    garage.render(flow,selection,1000,PencilDetail::High,{},pose,false,percent,true);
                    const auto reused=canvas.frame();
                    garage.render(flow,selection,1000,PencilDetail::High,{},pose,false,percent);
                    if(reused!=canvas.frame()) {
                        std::cerr<<"Inspection background reuse changed pixels\n";return 1;
                    }
                    for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x) {
                        if(!refresh.contains(x,y) && canvas.frame()[y*canvas.width()+x]!=fixedUI[y*canvas.width()+x]) {
                            std::cerr<<"Inspection changed pixels outside refresh region\n";return 1;
                        }
                    }
                }
                for(const auto setting: {std::pair<int,int>{65,85},{82,93}}) {
                    garage.render(flow,selection,1000,PencilDetail::High,{},pose,false,setting.first,true,setting.second);
                    const auto compact=canvas.frame();
                    garage.render(flow,selection,1000,PencilDetail::High,{},pose,false,setting.first,false,setting.second);
                    if(compact!=canvas.frame()) {std::cerr<<"Compact reuse left stale pixels\n";return 1;}
                    for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x)
                        if(!refresh.contains(x,y) && canvas.frame()[y*canvas.width()+x]!=fixedUI[y*canvas.width()+x]) {
                            std::cerr<<"Compact frame changed static UI\n";return 1;
                        }
                    if(azimuth==2 && elevation==2)
                        save("inspection-"+std::to_string(car)+"-compact-"+std::to_string(setting.first));
                }
                if(azimuth==2 && elevation==2) {
                    garage.render(flow,selection,1000,PencilDetail::Low,{},pose);
                    const auto lowRequest=canvas.frame();
                    garage.render(flow,selection,1000,PencilDetail::High,{},pose);
                    if(lowRequest!=canvas.frame()) {std::cerr<<"Inspection detail downgraded\n";return 1;}
                    save("inspection-"+std::to_string(car));
                    const auto native=canvas.frame();
                    for(int percent : {85,80,77,75,50}) {
                        garage.render(flow,selection,1000,PencilDetail::Low,{},pose,false,percent);
                        if(garage.inspectionPercent()!=percent || canvas.frame()==native)return 1;
                        for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x) {
                            if(x>=58 && x<410 && y>=83 && y<371)continue;
                            if(canvas.frame()[y*canvas.width()+x]!=native[y*canvas.width()+x]) {
                                std::cerr<<"Inspection scaling changed native UI\n";return 1;
                            }
                        }
                        save("inspection-"+std::to_string(car)+"-scale-"+std::to_string(percent));
                    }
                    garage.render(flow,selection,1000,PencilDetail::High,{},pose,false,100);
                    if(canvas.frame()!=native) {std::cerr<<"Inspection failed to restore exact native frame\n";return 1;}
                }
            }
            if(!flow.back() || flow.screen()!=GameScreen::CarSelect)return 1;
        }
        // Exercise arbitrary touch poses and the entire return path against
        // production mesh vertices, not a separate bounding-box approximation.
        for(int preset=0;preset<4;++preset) for(int azimuth=0;azimuth<16;++azimuth)
        for(int elevation=0;elevation<5;++elevation) {
            motion.reset(id,0);
            for(int p=0;p<preset;++p)motion.changeView(1,p*500);
            const auto home=motion.state(2000);
            const int dx=std::lround((azimuth*6.2831853f/16-home.yaw)/.012f);
            const int dy=std::lround(((.12f+elevation*1.4507963f/4)-home.pitch)/.008f);
            motion.drag({1,dx,dy,true,true},2000);
            if(car==0 && quality==PencilDetail::High && preset==0 && elevation==2 && azimuth%4==0) {
                garage.render(flow,selection,2000,quality,{},motion.state(2000));
                save("drag-"+std::to_string(azimuth));
            }
            motion.endDrag(2000);
            for(uint32_t elapsed:{0u,75u,150u,260u,390u,520u}) {
                const auto pose=motion.state(2000+elapsed);
                TrackCamera camera{};camera.principalX=233;
                camera.principalY=std::lround(pose.centerY);camera.focalLength=pose.scale*5.8f;
                const GarageCarTransform transform(carSpec(id),pose.yaw,pose.pitch,pose.wheelPhase);
                for(std::size_t panel=0;panel<framingMesh->count;++panel) {
                    const auto& face=framingMesh->panels[panel];
                    for(auto point:face.point) {
                        TrackScreenPoint screen{};
                        if(!projectTrackPoint(camera,transform(point,face.wheel),screen) ||
                           screen.x<57 || screen.x>408 ||
                           screen.y<camera.principalY-162 || screen.y>camera.principalY+125 ||
                           screen.y<109 || screen.y>354 || std::hypot(screen.x-233,screen.y-233)>220) {
                            std::cerr<<"Drag framing failed car="<<car<<" preset="<<preset<<" azimuth="<<azimuth<<" elevation="<<elevation<<" elapsed="<<elapsed<<'\n';
                            return 1;
                        }
                    }
                }
            }
        }
        motion.reset(id,0);
        for(unsigned transition=0;transition<9;++transition) {
            const uint32_t start=transition*1000;
            if(transition)motion.changeView(transition<=4 ? 1 : -1,start);
            const unsigned preset=unsigned(motion.state(start).preset);
            for(uint32_t elapsed:{0u,75u,150u,260u,390u,520u}) {
                const auto pose=motion.state(start+elapsed);
                garage.render(flow,selection,elapsed,quality,{},pose);checkText();
                TrackCamera camera{};camera.principalX=233+std::lround(pose.carSlide);
                camera.principalY=std::lround(pose.centerY);camera.focalLength=pose.scale*pose.carZoom*5.8f;
                const GarageCarTransform transform(carSpec(id),pose.yaw,pose.pitch,pose.wheelPhase);
                bool framed=true;
                for(std::size_t panel=0;panel<framingMesh->count;++panel) {
                    const auto& face=framingMesh->panels[panel];
                    for(auto p:face.point) {
                        TrackScreenPoint screen{};
                        if(!projectTrackPoint(camera,transform(p,face.wheel),screen) ||
                           screen.x<camera.principalX-176 || screen.x>camera.principalX+175 ||
                           screen.y<camera.principalY-162 || screen.y>camera.principalY+125 ||
                           screen.y<109 || screen.y>354 ||
                           std::hypot(screen.x-233,screen.y-233)>220) {
                            if(framed && car==0 && quality==PencilDetail::Low)
                                std::cerr << "Bad vertex " << screen.x << ',' << screen.y << " centerY=" << camera.principalY << '\n';
                            framed=false;
                        }
                    }
                }
                if(!framed) {
                    std::cerr << "Garage framing failed: car=" << car << " preset=" << preset << " t=" << elapsed << '\n';
                    valid=false;
                }
                if(car==0 && transition==1 && quality==PencilDetail::High)
                    save("turn-"+std::to_string(elapsed));
            }
            garage.render(flow,selection,600,quality,{},motion.state(start+600));
            const auto first=canvas.frame();
            if(quality==PencilDetail::High && transition<4)save("view-"+std::to_string(car)+"-"+std::to_string(preset));
            garage.render(flow,selection,780,quality,{},motion.state(start+780));
            const bool moves=first!=canvas.frame();
            // Neo's official smooth caps/dishes have no spokes; their perfectly
            // rotationally symmetric surface has no visible phase difference.
            const bool hasSpokes=id!=CarId::NeoTridaggerZmc && id!=CarId::BeakSpider;
            if(moves!=(preset==1 && hasSpokes) ||
               (preset!=1 && motion.state(start+600).wheelPhase!=motion.state(start+780).wheelPhase)) {
                std::cerr << "Garage wheels must move ONLY in side view\n";valid=false;
            }
        }
    }
    std::cout << "Garage cameras: 8 cars x 9 forward/reverse states x 3 LODs x 6 poses; side-only wheel pixel checks\n";
    // The exact same production cache is exercised across every selection,
    // entrance scale, wheel phase and quality tier (not just a single hero shot).
    std::array<uint64_t,kCarCount> showcaseHashes{};
    for(std::size_t i=0;i<kCarCount;++i) {
        GameFlow previewFlow;
        previewFlow.confirmInputAvailable(); previewFlow.completeCalibration(true);
        previewFlow.selectPlayerCar(static_cast<CarId>(i));
        previewFlow.confirmPlayerCar();
        selection.reset(static_cast<CarId>(i));
        for(uint32_t time : {0u,120u,500u,900u,2500u}) {
            for(auto quality : {PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
                garage.render(previewFlow,selection,time,quality);
                checkText();
            }
        }
        const auto settledShowcase=canvas.frame();
        garage.render(previewFlow,selection,777u,PencilDetail::High);
        if(settledShowcase!=canvas.frame()) {
            std::cerr<<"Settled showcase must have no vibration or wind lines\n";valid=false;
        }
        uint64_t hash=1469598103934665603ULL;
        for(auto pixel:canvas.frame())hash=(hash^pixel)*1099511628211ULL;
        for(std::size_t previous=0;previous<i;++previous)if(showcaseHashes[previous]==hash) {
            std::cerr<<"Different machines produced identical showcase images\n";valid=false;
        }
        showcaseHashes[i]=hash;
        save("showcase-"+std::to_string(i));
    }
    selection.reset();
    flow.confirmPlayerCar();
    garage.render(flow, selection, 1000u, PencilDetail::High);
    save("showcase");
    flow.completeCarShowcase();
    flow.toggleRival(CarId::HurricaneSonic);
    flow.toggleRival(CarId::NeoTridaggerZmc);
    garage.render(flow, selection, 500u, PencilDetail::High);
    save("rivals");
    garage.render(flow, selection, 500u, PencilDetail::High, {}, {}, true);
    save("device-rivals");
    flow.confirmRivals();
    garage.render(flow, selection, 0u, PencilDetail::High);
    save("track");
    garage.render(flow, selection, 0u, PencilDetail::High, {}, {}, true);
    save("device-track");
    for(auto track:{TrackId::SkyLoop,TrackId::TriCross,TrackId::GrandSpiral,TrackId::SkyLoop}) {
    flow.selectTrack(track);
    for(auto detail:{PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) for (unsigned orbit = 0; orbit < 16u; ++orbit) {
        garage.render(flow, selection, orbit * 2454u, detail);
        checkText();
        if(orbit==0 && detail==PencilDetail::High)save("track-"+std::to_string(int(track)));
        const auto& pixels=canvas.frame();
        for(int y=0;y<466;++y) for(int x=0;x<466;++x) {
            if(y>=87 && y<=89 && x>=215 && x<251)continue; // Header tricolor mark.
            const auto color=pixels[y*466+x];
            if(color!=track_paint::road && color!=track_paint::roadLight &&
               color!=track_paint::coral && color!=track_paint::blue &&
               color!=track_paint::fascia) continue;
            const int dx=x-233,dy=y-233;
            if(y<110 || y>356 || dx*dx+dy*dy>220*220) {
                std::cerr << "Track overview entered text/screen margin at " << x << "," << y << '\n';
                valid=false;
                break;
            }
        }
    }
    }
    flow.confirmTrack();
    RaceController race;
    race.prepare(flow.setup(), 0x12345678u);
    RaceRenderer renderer;
    ResultsSelection results;
    renderer.open(466, 466);
    renderer.render(flow, race, results, 0u, false, PencilDetail::High);
    save("grid");
    flow.completeGridIntro();
    renderer.render(flow, race, results, 1000u, false, PencilDetail::High);
    save("countdown");
    flow.completeCountdown();
    RacerInput input;
    input.valid = true;
    for (int frame = 0; frame < 60 * 60 && !race.snapshot().playerFinished; ++frame) {
        race.stepFixed(input);
        if (frame % 45 == 0 && frame < 540) {
            renderer.render(flow, race, results, frame * 1000u / 60u, false, PencilDetail::High);
            save("race-" + std::to_string(frame / 45));
        }
    }
    renderer.render(flow, race, results, 0u, false, PencilDetail::High, true);
    save("device-race");
    flow.togglePause();
    renderer.render(flow, race, results, 200u, true, PencilDetail::High);
    save("paused");
    const auto pausedPixels = canvas.frame();
    renderer.render(flow, race, results, 5000u, true, PencilDetail::High);
    if (pausedPixels != canvas.frame()) {
        std::cerr << "Paused image was not frozen\n"; valid = false;
    }
    renderer.render(flow, race, results, 200u, true, PencilDetail::High, true);
    save("device-paused");
    flow.togglePause();
    flow.finishRace();
    flow.showResults();
    renderer.render(flow, race, results, 0u, false, PencilDetail::High);
    save("results");
    renderer.render(flow, race, results, 0u, false, PencilDetail::High, true);
    save("device-results");
    // Exercise lower detail through a complete closed loop, including lapped
    // vehicles at the same physical location and both bridge approaches.
    flow.retrySameRace(); flow.completeGridIntro(); flow.completeCountdown();
    for (int sample = 0; sample < 96; ++sample) {
        // Test-only snapshot injection; renderer is read-only and physics is not run.
        auto& state = const_cast<RaceSnapshot&>(race.snapshot());
        state.cars[state.playerIndex].motion.distance = sample * race.track().length() / 96;
        state.cars[0].motion.distance = state.player().motion.distance + race.track().length() + 1;
        renderer.render(flow, race, results, 0u, false,
                        sample % 2 == 0 ? PencilDetail::Low : PencilDetail::Medium);
        checkText();
        if (sample == 48) save("bridge-lower");
        if (sample == 0) save("bridge-upper");
        if (sample == 24) save("curve-red");
        if (sample == 72) save("curve-blue");
        if (sample == 44) save("bridge-under");
    }
    // Reuse the same opened renderer across courses; stale road/minimap caches
    // would make the following image differ from a freshly opened renderer.
    for(auto track:{TrackId::GrandSpiral,TrackId::TriCross,TrackId::SkyLoop}) {
        auto setup=flow.setup();setup.track=track;race.prepare(setup,0x12345678u);
        for(int sample=0;sample<96;++sample) {
            auto& state=const_cast<RaceSnapshot&>(race.snapshot());
            for(std::size_t i=0;i<state.carCount;++i)
                state.cars[i].motion.distance=sample*race.track().length()/96+i*1.5f;
            const auto detail=sample%3==0 ? PencilDetail::High : sample%3==1 ? PencilDetail::Medium : PencilDetail::Low;
            renderer.render(flow,race,results,0,false,detail);checkText();
            if(track==TrackId::TriCross && sample%16==0)save("tri-race-"+std::to_string(sample));
            if(track==TrackId::GrandSpiral && sample%4==0)save("spiral-race-"+std::to_string(sample));
        }
        renderer.render(flow,race,results,0,false,PencilDetail::High);
        const auto reused=canvas.frame();
        renderer.close();renderer.open(466,466);
        renderer.render(flow,race,results,0,false,PencilDetail::High);
        if(reused!=canvas.frame()) {std::cerr << "stale track/minimap cache after course switch\n";valid=false;}
    }
    std::cout << "Renderer cache bytes: garage=" << sizeof(GarageRenderer)
              << " race=" << sizeof(RaceRenderer) << '\n';
    std::cout << "Open-only surface storage: garage=" << sizeof(GarageSurfaceCache)
              << " race=" << sizeof(RaceSurfaceCache) << '\n';
    for(std::size_t id=0;id<kCarCount;++id) {
        GameFlow drive;
        drive.confirmInputAvailable();drive.completeCalibration(true);
        drive.selectPlayerCar(static_cast<CarId>(id));drive.confirmPlayerCar();
        drive.completeCarShowcase();
        for(std::size_t rival=0;rival<kCarCount;++rival)
            if(rival!=id)drive.toggleRival(static_cast<CarId>(rival));
        drive.confirmRivals();drive.confirmTrack();drive.completeGridIntro();drive.completeCountdown();
        RaceController run;run.prepare(drive.setup(),0x12345678u);
        renderer.render(drive,run,results,0,false,PencilDetail::High);
        save("race-car-"+std::to_string(id));
        // A nearby following opponent exercises foreground bounds. Exact near
        // clipping and multi-tile equivalence are tested in validateCarRaster.
        auto& state=const_cast<RaceSnapshot&>(run.snapshot());
        const auto opponent=(state.playerIndex+1)%state.carCount;
        state.cars[opponent].motion.distance=state.player().motion.distance-.7f;
        state.cars[opponent].motion.lateralOffset=.65f;
        renderer.render(drive,run,results,0,false,PencilDetail::High);
        if(id==0)save("race-close");
        // Exercise every catalogue car as a nearby opponent, not only as the
        // player. Use a legal two-car grid with a different player machine.
        GameFlow closeFlow;
        closeFlow.confirmInputAvailable();closeFlow.completeCalibration(true);
        closeFlow.selectPlayerCar(static_cast<CarId>((id+1)%kCarCount));closeFlow.confirmPlayerCar();
        closeFlow.completeCarShowcase();closeFlow.toggleRival(static_cast<CarId>(id));
        closeFlow.confirmRivals();closeFlow.confirmTrack();closeFlow.completeGridIntro();closeFlow.completeCountdown();
        RaceController closeRace;closeRace.prepare(closeFlow.setup(),0x12345678u);
        auto& nearState=const_cast<RaceSnapshot&>(closeRace.snapshot());
        auto& nearRival=nearState.cars[1-nearState.playerIndex];
        nearRival.motion.distance=nearState.player().motion.distance-.15f;
        nearRival.motion.lateralOffset=.55f;
        renderer.render(closeFlow,closeRace,results,0,false,PencilDetail::High);
        save("race-close-car-"+std::to_string(id));
    }
    // All-new and mixed rosters, including a solo quality change between full
    // grids. Reused caches must match a clean renderer byte for byte.
    for(unsigned cycle=0;cycle<6;++cycle) {
        RaceSetup setup;setup.playerCar=cycle<3 ? CarId::Diospada : CarId::SpinCobra;
        if(cycle%3!=1)setup.rivalMask=cycle<3 ? uint8_t(0x60) : uint8_t(0x81);
        RaceController run;run.prepare(setup,42);
        const auto detail=cycle%3==0 ? PencilDetail::High : PencilDetail::Low;
        renderer.render(flow,run,results,0,false,detail);
        const auto cached=canvas.frame();
        RaceRenderer clean;clean.open(466,466);clean.render(flow,run,results,0,false,detail);
        if(cached!=canvas.frame()) {std::cerr << "Roster/quality cache mismatch\n";valid=false;}
        clean.close();
        if(cycle==0)save("race-new-roster");
    }
    // Half-resolution scenes must retain native HUD/control text. Cover each
    // course, bridge approaches, nearby opponents and renderer re-entry.
    RaceRenderer half;
    half.open(466,466,true);
    if(!half.halfResolutionActive()) {std::cerr<<"Square scene fell back to native\n";valid=false;}
    for(auto track:{TrackId::SkyLoop,TrackId::TriCross}) {
        auto setup=flow.setup();setup.track=track;race.prepare(setup,42);
        for(int sample=0;sample<24;++sample) {
            auto& state=const_cast<RaceSnapshot&>(race.snapshot());
            for(std::size_t i=0;i<state.carCount;++i)
                state.cars[i].motion.distance=sample*race.track().length()/24+i*.5f;
            renderer.render(flow,race,results,0,false,PencilDetail::Low,true);
            const auto native=canvas.frame();
            const auto nativeText=canvas.texts;
            if(track==TrackId::TriCross && sample==0)save("resolution-native");
            canvas.texts.clear();
            half.render(flow,race,results,0,false,PencilDetail::Low,true);
            checkText();
            if(canvas.texts.size()!=nativeText.size())valid=false;
            else for(std::size_t i=0;i<nativeText.size();++i) {
                const auto& a=nativeText[i];const auto& b=canvas.texts[i];
                if(a.value!=b.value || a.x!=b.x || a.y!=b.y || a.size!=b.size)valid=false;
            }
            // Interior strips avoid rounded transparent corners of HUD cards.
            for(auto rect:{std::array<int,4>{119,36,346,73},{128,82,284,113}})
                for(int y=rect[1];y<rect[3];++y)for(int x=rect[0];x<rect[2];++x)
                    if(native[y*466+x]!=canvas.frame()[y*466+x])valid=false;
            if(sample%6==0)save("half-"+std::to_string(int(track))+"-"+std::to_string(sample));
            if(track==TrackId::TriCross && sample==0)save("resolution-half");
        }
        const auto before=canvas.frame();
        half.close();half.open(466,466,true);
        half.render(flow,race,results,0,false,PencilDetail::Low,true);
        if(before!=canvas.frame()){std::cerr<<"Half-resolution re-entry mismatch\n";valid=false;}
    }
    flow.togglePause();
    half.render(flow,race,results,100,true,PencilDetail::Low,true);
    const auto halfPaused=canvas.frame();
    half.render(flow,race,results,6000,true,PencilDetail::Low,true);
    if(halfPaused!=canvas.frame())valid=false;
    save("half-paused");
    flow.togglePause();flow.finishRace();flow.showResults();
    renderer.render(flow,race,results,0,false,PencilDetail::Low,true);
    const auto nativeResults=canvas.frame();
    half.render(flow,race,results,0,false,PencilDetail::Low,true);
    if(nativeResults!=canvas.frame())valid=false;
    half.close();
    // Device HAL is 468 x 466, not the historical 466-square capture size.
    {
        // A staircase should become a one-pixel diagonal without new colors.
        const uint16_t above[]{0,0,0xf81f},middle[]{0,0xf81f,0xf81f},below[]{0xf81f,0xf81f,0xf81f};
        std::array<uint16_t,6> top{},bottom{};
        upscaleEdgeRow(above,middle,below,3,top.data(),bottom.data());
        if(top!=std::array<uint16_t,6>{0,0,0,0xf81f,0xf81f,0xf81f} ||
           bottom!=std::array<uint16_t,6>{0,0xf81f,0xf81f,0xf81f,0xf81f,0xf81f})valid=false;
        for(uint16_t color:{uint16_t(0),uint16_t(0xffff),uint16_t(0x1234)}) {
            const uint16_t single[]{color};
            upscaleEdgeRow(single,single,single,1,top.data(),bottom.data());
            if(top[0]!=color || top[1]!=color || bottom[0]!=color || bottom[1]!=color)valid=false;
        }
    }
    // Catch a silently disabled optimization and exercise the wider copy row.
    canvas.createSprite(468,466);
    renderer.close();renderer.open(468,466);
    half.open(468,466,true);
    if(!half.halfResolutionActive()) {std::cerr<<"Device scene fell back to native\n";valid=false;}
    flow.retrySameRace();flow.completeGridIntro();flow.completeCountdown();
    for(auto track:{TrackId::SkyLoop,TrackId::TriCross}) {
        auto setup=flow.setup();setup.track=track;race.prepare(setup,42);
        for(int sample=0;sample<12;++sample) {
            auto& state=const_cast<RaceSnapshot&>(race.snapshot());
            for(std::size_t i=0;i<state.carCount;++i)
                state.cars[i].motion.distance=sample*race.track().length()/12+i*.5f;
            renderer.render(flow,race,results,0,false,PencilDetail::Low,true);
            const auto native=canvas.frame();
            if(track==TrackId::TriCross && sample==0)save("device-resolution-native");
            half.render(flow,race,results,0,false,PencilDetail::Low,true);
            if(native==canvas.frame()) {std::cerr<<"Device scene remained native\n";valid=false;}
            const auto bulk=canvas.frame();
            half.setBulkSceneCopy(false);
            half.render(flow,race,results,0,false,PencilDetail::Low,true);
            if(bulk!=canvas.frame()) {std::cerr<<"Bulk scene copy changed RGB565 pixels\n";valid=false;}
            half.setBulkSceneCopy(true);
            for(int y=36;y<73;++y)for(int x=119;x<346;++x)
                if(native[y*468+x]!=canvas.frame()[y*468+x])valid=false;
            if(track==TrackId::TriCross && sample==0)save("device-resolution-half");
        }
    }
    const auto deviceBefore=canvas.frame();
    half.close();half.open(468,466,true);
    half.render(flow,race,results,0,false,PencilDetail::Low,true);
    if(deviceBefore!=canvas.frame())valid=false;
    half.close();canvas.createSprite(466,466);
    // Repeated entry releases large PSRAM-oriented storage instead of keeping
    // it resident in the Launcher for the lifetime of the installed App object.
    for(int cycle=0;cycle<3;++cycle) {
        garage.close();renderer.close();
        garage.open(466,466);renderer.open(466,466);
        renderer.render(flow,race,results,0,false,PencilDetail::High);
        checkText();
    }
    garage.close();renderer.close();
    // Race materials/cache variant: all eight identities as player and near
    // opponent, both courses, solo/roster/detail changes, and close/re-entry.
    canvas.createSprite(468,466);
    // Home UI evidence at the actual device width, including input faults and
    // invalid calibration progress; all existing race captures remain below.
    {
        GameFlow hudFlow;hudFlow.useDeviceControls();hudFlow.confirmPlayerCar();hudFlow.completeCarShowcase();
        hudFlow.toggleRival(CarId::HurricaneSonic);hudFlow.confirmRivals();hudFlow.confirmTrack();
        RaceController hudRace;hudRace.prepare(hudFlow.setup(),42);
        RaceRenderer hud;hud.open(468,466,true,true,true);
        ResultsSelection actions;
        hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);save("ui-grid-native");
        hudFlow.completeGridIntro();
        for(uint32_t elapsed:{0u,1000u,2000u}) {
            hud.render(hudFlow,hudRace,actions,elapsed,false,PencilDetail::Low,true);
            save("ui-countdown-"+std::to_string(3-elapsed/1000));
            int left=468,right=-1,top=466,bottom=-1;
            for(int y=193;y<=273;++y)for(int x=202;x<=266;++x)
                if(canvas.frame()[y*468+x]==0xf7be) {
                    left=std::min(left,x);right=std::max(right,x);
                    top=std::min(top,y);bottom=std::max(bottom,y);
                }
            if(right<left || left+right!=468 || top+bottom!=466) {
                std::cerr<<"Countdown visible digit is not centred on the device\n";valid=false;
            }
        }
        hudFlow.completeCountdown();
        for(unsigned car=0;car<kCarCount;++car) {
            auto setup=hudFlow.setup();setup.playerCar=CarId(car);setup.rivalMask=0;
            hudRace.prepare(setup,42);
            auto& state=const_cast<RaceSnapshot&>(hudRace.snapshot());
            state.cars[state.playerIndex].motion.distance=12;
            // Half-scene blits replace pixels without clearing host text metadata.
            canvas.texts.clear();
            hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);
            for(const auto& t:canvas.texts)if(t.y+4*t.size>115) {
                std::cerr<<"Persistent race text entered the driving area\n";valid=false;
            }
            save("ui-racing-native-"+std::to_string(car));
        }
        // Wall contact uses one warning triangle on the impacted side. It must
        // not alter the centre driving view or reintroduce a full-screen ring.
        auto& warningState=const_cast<RaceSnapshot&>(hudRace.snapshot());
        auto& warningCar=warningState.cars[warningState.playerIndex];
        for(float lateral:{-1.f,1.f}) {
            warningCar.motion.lateralOffset=lateral;
            warningCar.motion.wallImpact=0;
            hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);
            const auto cleanWarning=canvas.frame();
            warningCar.motion.wallImpact=1;
            hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);
            const auto& warned=canvas.frame();
            int changed=0,left=468,right=-1,top=466,bottom=-1;
            for(int y=0;y<466;++y)for(int x=0;x<468;++x) {
                if(warned[y*468+x]==cleanWarning[y*468+x])continue;
                ++changed;left=std::min(left,x);right=std::max(right,x);
                top=std::min(top,y);bottom=std::max(bottom,y);
            }
            const bool expectedLeft=lateral<0;
            const int warningCenter=expectedLeft ? 34 : 434;
            const bool hasBang=warned[205*468+warningCenter]==0xf7be &&
                               warned[223*468+warningCenter]==0xf7be;
            if(changed<500 || !hasBang || top<188 || bottom>235 ||
               (expectedLeft ? right>58 : left<410)) {
                std::cerr<<"Collision warning escaped impacted edge: lateral="<<lateral
                         <<" changed="<<changed<<" bounds="<<left<<','<<top<<".."<<right<<','<<bottom<<'\n';
                valid=false;
            }
            save(expectedLeft ? "ui-wall-warning-left" : "ui-wall-warning-right");
        }
        warningCar.motion.wallImpact=0;
        hudFlow.togglePause();
        hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);save("ui-paused-native");
        hudFlow.togglePause();hudFlow.finishRace();
        hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);save("ui-finish-native");
        hudFlow.showResults();
        hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);save("ui-results-solo-native");
        auto completed=hudFlow.setup();completed.playerCar=CarId::HurricaneSonic;
        completed.rivalMask=carMask(CarId::CycloneMagnum)|carMask(CarId::NeoTridaggerZmc);
        hudRace.prepare(completed,42);
        auto& finished=const_cast<RaceSnapshot&>(hudRace.snapshot());
        auto& finisher=finished.cars[finished.playerIndex];
        finisher.finished=true;finisher.finishSeconds=27.532f;finisher.bestLapSeconds=8.301f;
        finisher.position=2;finisher.completedLaps=3;finished.playerFinished=true;
        for(int action=0;action<3;++action) {
            actions.select(ResultAction(action));
            hud.render(hudFlow,hudRace,actions,0,false,PencilDetail::Low,true);
            save("ui-results-native-"+std::to_string(action));
        }
        hud.close();
    }
    {
        GarageRenderer home;home.open(468,466);
        GameFlow entry;GarageSelection chosen;RacerInputStatus input;
        input.actionsConfigured=true;
        home.render(entry,chosen,0,PencilDetail::High,input);
        save("home-waiting-native");
        input.readiness=RacerInputReadiness::Fault;
        home.render(entry,chosen,0,PencilDetail::Low,input);
        save("home-fault-native");
        entry.confirmInputAvailable();input.axesConnected=true;
        input.readiness=RacerInputReadiness::Calibrating;
        input.calibrationProgress=.6f;
        home.render(entry,chosen,0,PencilDetail::High,input);
        save("home-calibration-native");
        input.calibrationProgress=std::numeric_limits<float>::quiet_NaN();
        home.render(entry,chosen,0,PencilDetail::Low,input);
        save("home-calibration-invalid-native");
        entry.useDeviceControls();
        for(unsigned car=0;car<kCarCount;++car) {
            chosen.reset(CarId(car));
            home.render(entry,chosen,0,PencilDetail::High,{}, {},true);
            save("home-machine-native-"+std::to_string(car));
        }
        GameFlow setup;GarageSelection rivals;
        setup.useDeviceControls();setup.selectPlayerCar(CarId::HurricaneSonic);
        setup.confirmPlayerCar();rivals.reset(CarId::HurricaneSonic);
        home.render(setup,rivals,900,PencilDetail::High,{}, {},true);save("setup-confirm-native");
        setup.completeCarShowcase();
        home.render(setup,rivals,0,PencilDetail::High,{}, {},true);save("setup-rivals-empty-native");
        setup.toggleRival(CarId::CycloneMagnum);
        home.render(setup,rivals,0,PencilDetail::High,{}, {},true);save("setup-rivals-one-native");
        setup.toggleRival(CarId::NeoTridaggerZmc);
        home.render(setup,rivals,0,PencilDetail::High,{}, {},true);save("setup-rivals-full-selected-native");
        rivals.moveRival(1,setup.setup().playerCar);rivals.moveRival(1,setup.setup().playerCar);
        home.render(setup,rivals,0,PencilDetail::High,{}, {},true);save("setup-rivals-full-unselected-native");
        while(!rivals.rivalCursorIsDone())rivals.moveRival(1,setup.setup().playerCar);
        home.render(setup,rivals,0,PencilDetail::High,{}, {},true);save("setup-rivals-ready-native");
        setup.toggleRival(CarId::CycloneMagnum);setup.toggleRival(CarId::NeoTridaggerZmc);
        home.render(setup,rivals,0,PencilDetail::High,{}, {},true);save("setup-rivals-solo-native");
        setup.confirmRivals();
        for(auto track:{TrackId::SkyLoop,TrackId::TriCross}) {
            setup.selectTrack(track);
            home.render(setup,rivals,0,PencilDetail::High,{}, {},true);
            save("setup-course-native-"+std::to_string(int(track)));
        }
        for(unsigned car=0;car<kCarCount;++car) {
            GameFlow roster;GarageSelection cursor;
            const auto player=CarId((car+1)%kCarCount);
            roster.useDeviceControls();roster.selectPlayerCar(player);roster.confirmPlayerCar();
            roster.completeCarShowcase();cursor.reset(player);
            while(cursor.rivalCursorCar()!=CarId(car) || cursor.rivalCursorIsDone())cursor.moveRival(1,player);
            home.render(roster,cursor,0,PencilDetail::High,{}, {},true);
            save("setup-rival-machine-"+std::to_string(car));
        }
        home.close();
    }
    RaceRenderer cached,original,hero,wire;
    // The new touch UI must still expose a complete external-only path.
    {
        GarageRenderer menu; menu.open(468,466);
        RaceRenderer racing; racing.open(468,466,true,true,true);
        GameFlow external; GarageSelection cursor; ResultsSelection result;
        cursor.reset(); // Match App::onOpen; the player is not a rival candidate.
        const auto requireText = [&](const char* expected) {
            for (const auto& text : canvas.texts) if (text.value == expected) return;
            std::cerr << "Missing external control hint: " << expected << '\n'; valid=false;
        };
        external.confirmInputAvailable(); external.completeCalibration(true);
        menu.render(external,cursor,0,PencilDetail::High,{}, {},false);
        requireText("BLUE: SELECT"); save("external-machine-native");
        cursor.activatePlayer(external); external.completeCarShowcase();
        menu.render(external,cursor,0,PencilDetail::High,{}, {},false);
        requireText("L/R / BLUE SELECT"); requireText("READY >");
        save("external-rival-native");
        for(std::size_t i=0;i<kCarCount && !cursor.rivalCursorIsDone();++i)
            cursor.moveRival(1,external.setup().playerCar);
        menu.render(external,cursor,0,PencilDetail::High,{}, {},false);
        requireText("SOLO RUN"); requireText("BLUE: NEXT"); save("external-ready-native");
        cursor.activateRival(external);
        menu.render(external,cursor,0,PencilDetail::High,{}, {},false);
        requireText("START RACE"); requireText("L/R COURSE / BLUE GO"); save("external-course-native");
        external.confirmTrack();
        RaceController run; run.prepare(external.setup(),42);
        canvas.texts.clear(); racing.render(external,run,result,0,false,PencilDetail::Low,false);
        requireText("RED BRAKE / BLUE BOOST"); requireText("JOYSTICK STEER / HOLD RED PAUSE");
        save("external-grid-native");
        external.completeGridIntro(); external.completeCountdown(); external.togglePause();
        canvas.texts.clear(); racing.render(external,run,result,0,true,PencilDetail::Low,false);
        requireText("HOLD RED WHEN INPUT READY"); save("external-paused-native");
        external.togglePause(); external.finishRace(); external.showResults();
        for(int action=0;action<int(ResultAction::Count);++action) {
            result.select(ResultAction(action));
            canvas.texts.clear(); racing.render(external,run,result,0,false,PencilDetail::Low,false);
            requireText("U/D BLUE OR TAP"); requireText("RETRY"); requireText("GARAGE"); requireText("EXIT");
            save("external-results-native-"+std::to_string(action));
        }
        menu.close(); racing.close();
    }
    cached.open(468,466,true,true);original.open(468,466,true);
    hero.open(468,466,true,true,true);
    wire.open(468,466,true,true,true,true);
    for(unsigned car=0;car<kCarCount;++car) {
        GameFlow drive;drive.useDeviceControls();
        drive.selectPlayerCar(CarId(car));drive.confirmPlayerCar();drive.completeCarShowcase();
        for(unsigned rival=1;rival<=kMaximumRivals;++rival)drive.toggleRival(CarId((car+rival)%kCarCount));
        drive.confirmRivals();drive.confirmTrack();drive.completeGridIntro();drive.completeCountdown();
        auto setup=drive.setup();setup.track=car%2 ? TrackId::SkyLoop : TrackId::TriCross;
        RaceController run;run.prepare(setup,42);
        for(int sample=0;sample<4;++sample) {
            auto& state=const_cast<RaceSnapshot&>(run.snapshot());
            for(std::size_t i=0;i<state.carCount;++i)
                state.cars[i].motion.distance=sample*run.track().length()/4+i*.35f;
            const auto detail=sample%2 ? PencilDetail::Medium : PencilDetail::Low;
            original.render(drive,run,results,0,false,detail,true);
            const auto baseline=canvas.frame();
            if(sample==0)save("material-original-"+std::to_string(car));
            cached.render(drive,run,results,0,false,detail,true);
            if(sample==0)save("material-race-"+std::to_string(car));
            hero.setIncrementalCarRaster(false);
            hero.render(drive,run,results,0,false,detail,true);
            if(sample==0)save("player-quality-"+std::to_string(car));
            for(int y=79;y<=165;++y)for(int x=311;x<=397;++x)
                if((x-354)*(x-354)+(y-122)*(y-122)<42*42 &&
                   baseline[y*468+x]!=canvas.frame()[y*468+x]) {
                    std::cerr<<"Cached minimap pixel mismatch\n";valid=false;
                }
            const auto solidFrame=canvas.frame();
            hero.setIncrementalCarRaster(true);
            hero.render(drive,run,results,0,false,detail,true);
            if(solidFrame!=canvas.frame()) {std::cerr<<"Incremental car raster mismatch\n";valid=false;}
            hero.setIncrementalCarRaster(false);
            if(sample==0)save("edge-"+std::to_string(car)+"-nearest");
            hero.setEdgeUpscale(true);
            if(!hero.edgeUpscaleActive()) {std::cerr<<"Edge row allocation failed on host\n";valid=false;}
            hero.render(drive,run,results,0,false,detail,true);
            const auto filtered=canvas.frame();
            if(sample==0)save("edge-"+std::to_string(car)+"-filtered");
            hero.setBulkSceneCopy(false);
            hero.render(drive,run,results,0,false,detail,true);
            if(filtered!=canvas.frame()) {std::cerr<<"Edge upscale copy path mismatch\n";valid=false;}
            hero.setBulkSceneCopy(true);
            // Native HUD and map are drawn after the filtered scene.
            for(int y=40;y<69;++y)for(int x=120;x<340;++x)
                if(solidFrame[y*468+x]!=filtered[y*468+x]) {std::cerr<<"Edge upscale changed native HUD\n";valid=false;}
            hero.setEdgeUpscale(false);
            hero.render(drive,run,results,0,false,detail,true);
            if(solidFrame!=canvas.frame()) {std::cerr<<"Edge upscale mode restoration mismatch\n";valid=false;}
            hero.setRowOcclusionFilter(false);
            hero.render(drive,run,results,0,false,detail,true);
            if(solidFrame!=canvas.frame()) {std::cerr<<"Row occlusion filter mismatch\n";valid=false;}
            hero.setRowOcclusionFilter(true);
            wire.render(drive,run,results,0,false,detail,true);
            if(car<2)save("wireframe-"+std::to_string(car)+"-"+std::to_string(sample));
            for(int y=79;y<=165;++y)for(int x=311;x<=397;++x)
                if((x-354)*(x-354)+(y-122)*(y-122)<42*42 &&
                   solidFrame[y*468+x]!=canvas.frame()[y*468+x]) {
                    std::cerr<<"Wireframe minimap mismatch\n";valid=false;
                }
        }
        // Keep this identity as a nearby opponent of another player.
        auto& state=const_cast<RaceSnapshot&>(run.snapshot());
        const auto target=state.playerIndex;
        state.cars[target].player=false;state.playerIndex=(target+1)%state.carCount;
        state.cars[state.playerIndex].player=true;
        state.cars[target].motion.distance=state.player().motion.distance-.15f;
        state.cars[target].motion.lateralOffset=.55f;
        cached.render(drive,run,results,0,false,PencilDetail::Low,true);
        save("material-opponent-"+std::to_string(car));
        state.cars[state.playerIndex].motion.speed=25.f;
        hero.render(drive,run,results,0,false,PencilDetail::Low,true);
        save("player-quality-opponent-"+std::to_string(car));
        const auto heroBefore=canvas.frame();
        hero.close();hero.open(468,466,true,true,true);
        hero.render(drive,run,results,9000,false,PencilDetail::Low,true);
        if(heroBefore!=canvas.frame()) {std::cerr<<"Player quality re-entry / speed effect regression\n";valid=false;}
        wire.render(drive,run,results,0,false,PencilDetail::Low,true);
        if(car<2)save("wireframe-opponent-"+std::to_string(car));
        const auto wireBefore=canvas.frame();
        wire.close();wire.open(468,466,true,true,true,true);
        wire.render(drive,run,results,9000,false,PencilDetail::Low,true);
        if(wireBefore!=canvas.frame()) {std::cerr<<"Wireframe re-entry regression\n";valid=false;}
        cached.render(drive,run,results,0,false,PencilDetail::Low,true);
        const auto before=canvas.frame();
        cached.close();cached.open(468,466,true,true);
        cached.render(drive,run,results,0,false,PencilDetail::Low,true);
        if(before!=canvas.frame()) {std::cerr<<"Race material re-entry mismatch\n";valid=false;}
        setup.rivalMask=0;run.prepare(setup,42);
        cached.render(drive,run,results,0,false,PencilDetail::High,true);
    }
    // Actual device quality path: half-resolution scene, native player, edge
    // refinement, both extreme steering positions and near-clipped opponents.
    hero.setEdgeUpscale(true);
    for(unsigned car=0;car<kCarCount;++car) {
        GameFlow drive;drive.useDeviceControls();drive.selectPlayerCar(CarId(car));drive.confirmPlayerCar();drive.completeCarShowcase();
        drive.toggleRival(CarId((car+1)%kCarCount));drive.toggleRival(CarId((car+2)%kCarCount));
        drive.confirmRivals();drive.selectTrack(TrackId::GrandSpiral);drive.confirmTrack();drive.completeGridIntro();drive.completeCountdown();
        RaceController run;run.prepare(drive.setup(),42);
        for(int sample=0;sample<16;++sample) {
            auto& state=const_cast<RaceSnapshot&>(run.snapshot());
            for(std::size_t i=0;i<state.carCount;++i) {
                auto& c=state.cars[i];c.motion.distance=sample*run.track().length()/16;
                c.motion.speed=12.f;c.motion.lateralOffset=c.player ? (sample%3-1)*1.3f : (i%2 ? .55f : -.55f);
                if(!c.player)c.motion.distance+=i%2 ? .5f : sample%4==3 ? -2.2f : -.25f;
            }
            hero.setTrackCulling(true);hero.render(drive,run,results,0,false,PencilDetail::Low,true);
            const auto culled=canvas.frame();
            hero.setIncrementalCarRaster(true);
            hero.render(drive,run,results,0,false,PencilDetail::Low,true);
            if(culled!=canvas.frame()) {std::cerr<<"Device-quality incremental car raster mismatch\n";valid=false;}
            hero.setIncrementalCarRaster(false);
            if(car==0)save("spiral-device-"+std::to_string(sample));
            hero.setTrackCulling(false);hero.render(drive,run,results,0,false,PencilDetail::Low,true);
            if(culled!=canvas.frame()) {std::cerr<<"Device-quality spiral culling mismatch\n";valid=false;}
        }
    }
    std::cout<<"Grand Spiral device-quality path: 128 three-car poses, culling/raster equality\n";
    // Explicit generic targets must match the compatibility Sprite path. The
    // race comparison also exercises row pushImage instead of raw Sprite copy.
    LGFX_Sprite genericTarget;
    genericTarget.createSprite(468,466);
    GarageRenderer garageTarget;
    GameFlow garageFlow;
    GarageSelection garageSelection;
    garageTarget.open(468,466);
    garageFlow.confirmInputAvailable();garageFlow.completeCalibration(true);
    garageTarget.render(garageFlow,garageSelection,321,PencilDetail::High);
    const auto garageExpected=canvas.frame();
    garageTarget.render(genericTarget,garageFlow,garageSelection,321,PencilDetail::High);
    if(garageExpected!=genericTarget.frame()) {
        std::cerr<<"Generic garage render target mismatch\n";valid=false;
    }
    GameFlow targetFlow;targetFlow.useDeviceControls();
    targetFlow.selectPlayerCar(CarId::CycloneMagnum);targetFlow.confirmPlayerCar();
    targetFlow.completeCarShowcase();targetFlow.confirmRivals();targetFlow.confirmTrack();
    targetFlow.completeGridIntro();targetFlow.completeCountdown();
    RaceController targetRace;targetRace.prepare(targetFlow.setup(),42);
    RaceRenderer raceTarget;
    raceTarget.open(468,466,true,true,true);raceTarget.setEdgeUpscale(true);
    hero.render(targetFlow,targetRace,results,0,false,PencilDetail::Low,true);
    const auto raceExpected=canvas.frame();
    const auto writesBefore=genericTarget.fullWidthImageWrites();
    const auto rowsBefore=genericTarget.fullWidthImageRows();
    raceTarget.render(genericTarget,nullptr,targetFlow,targetRace,results,0,false,PencilDetail::Low,true);
    if(raceExpected!=genericTarget.frame()) {
        std::cerr<<"Generic race render target mismatch\n";valid=false;
    }
    if(genericTarget.fullWidthImageWrites()-writesBefore!=233u ||
       genericTarget.fullWidthImageRows()-rowsBefore!=466u) {
        std::cerr<<"Half-resolution rows were not submitted in pairs\n";valid=false;
    }
    targetFlow.finishRace();targetFlow.showResults();
    raceTarget.render(targetFlow,targetRace,results,0,false,PencilDetail::Low,true);
    const auto resultsExpected=canvas.frame();
    raceTarget.render(genericTarget,nullptr,targetFlow,targetRace,results,0,false,PencilDetail::Low,true);
    if(resultsExpected!=genericTarget.frame()) {
        std::cerr<<"Generic results render target mismatch\n";valid=false;
    }
    garageTarget.close();raceTarget.close();
    cached.close();original.close();hero.close();wire.close();canvas.createSprite(466,466);
    std::cout << "Production renderer frames: " << directory << '\n';
    return valid ? 0 : 1;
}
