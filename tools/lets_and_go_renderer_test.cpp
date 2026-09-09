#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/garage_car_transform.h"
#include <hal/hal.h>
#include <filesystem>
#include <iostream>
#include <cstring>

using namespace lets_and_go;

bool validateCarRaster()
{
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
    canvas.fillScreen(track_paint::night);map.draw(canvas);
    for(const auto& section:map.section)for(auto p:{section.left,section.right}) {
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
    std::cout << "Track paint: order-independent depth, dark backdrop, 96-section map, 144 poses, max occluders="
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

int main(int argc, char** argv)
{
    const std::string directory = argc > 1 ? argv[1] : "/tmp/lets-go-frames";
    std::filesystem::create_directories(directory);
    captureCarStructures(directory);
    auto& canvas = GetHAL().getDisplay();
    bool valid = validateCarRaster() && validateTrackPaint() && validateTrackPaint(TrackId::TriCross);
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
    // Actual production cameras, not just the separate model-inspection views.
    // Check every panel through all presets/transitions and all quality tiers.
    auto framingMesh=std::make_unique<CarDisplayMesh>();
    for(unsigned car=0;car<kCarCount;++car) for(auto quality:
        {PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
        const auto id=static_cast<CarId>(car);selection.reset(id);
        buildCarDisplayMesh(id,*framingMesh,quality==PencilDetail::High ? CarSurfaceDetail::High :
                            quality==PencilDetail::Medium ? CarSurfaceDetail::Medium : CarSurfaceDetail::Low);
        GarageViewController motion;motion.reset(id,0);
        for(unsigned transition=0;transition<9;++transition) {
            const uint32_t start=transition*1000;
            if(transition)motion.changeView(transition<=4 ? 1 : -1,start);
            const unsigned preset=unsigned(motion.state(start).preset);
            for(uint32_t elapsed:{0u,50u,100u,175u,250u,350u}) {
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
            garage.render(flow,selection,500,quality,{},motion.state(start+500));
            const auto first=canvas.frame();
            if(quality==PencilDetail::High && transition<4)save("view-"+std::to_string(car)+"-"+std::to_string(preset));
            garage.render(flow,selection,680,quality,{},motion.state(start+680));
            const bool moves=first!=canvas.frame();
            // Neo's official smooth caps/dishes have no spokes; their perfectly
            // rotationally symmetric surface has no visible phase difference.
            const bool hasSpokes=id!=CarId::NeoTridaggerZmc && id!=CarId::BeakSpider;
            if(moves!=(preset==0 || (preset==1 && hasSpokes)) ||
               (preset!=1 && motion.state(start+500).wheelPhase!=motion.state(start+680).wheelPhase)) {
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
    for(auto track:{TrackId::SkyLoop,TrackId::TriCross,TrackId::SkyLoop}) {
    flow.selectTrack(track);
    for(auto detail:{PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) for (unsigned orbit = 0; orbit < 16u; ++orbit) {
        garage.render(flow, selection, orbit * 2454u, detail);
        checkText();
        if(orbit==0 && detail==PencilDetail::High)save(track==TrackId::SkyLoop ? "track-0" : "track-1");
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
    for(auto track:{TrackId::TriCross,TrackId::SkyLoop}) {
        auto setup=flow.setup();setup.track=track;race.prepare(setup,0x12345678u);
        for(int sample=0;sample<96;++sample) {
            auto& state=const_cast<RaceSnapshot&>(race.snapshot());
            for(std::size_t i=0;i<state.carCount;++i)
                state.cars[i].motion.distance=sample*race.track().length()/96+i*1.5f;
            const auto detail=sample%3==0 ? PencilDetail::High : sample%3==1 ? PencilDetail::Medium : PencilDetail::Low;
            renderer.render(flow,race,results,0,false,detail);checkText();
            if(track==TrackId::TriCross && sample%16==0)save("tri-race-"+std::to_string(sample));
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
            for(auto rect:{std::array<int,4>{119,36,346,73},{128,384,345,426},{103,85,247,122}})
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
    RaceRenderer cached,original,hero,wire;
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
            hero.render(drive,run,results,0,false,detail,true);
            if(sample==0)save("player-quality-"+std::to_string(car));
            for(int y=79;y<=165;++y)for(int x=311;x<=397;++x)
                if((x-354)*(x-354)+(y-122)*(y-122)<42*42 &&
                   baseline[y*468+x]!=canvas.frame()[y*468+x]) {
                    std::cerr<<"Cached minimap pixel mismatch\n";valid=false;
                }
            const auto solidFrame=canvas.frame();
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
    cached.close();original.close();hero.close();wire.close();canvas.createSprite(466,466);
    std::cout << "Production renderer frames: " << directory << '\n';
    return valid ? 0 : 1;
}
