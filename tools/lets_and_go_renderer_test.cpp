#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/garage_car_transform.h"
#include <hal/hal.h>
#include <filesystem>
#include <iostream>

using namespace lets_and_go;

bool validateCarRaster()
{
    auto& canvas=GetHAL().getCanvas();
    CarSurfaceRaster<32,32> raster;
    const CarScreenVertex a{180,180,.5f,0,0},b{212,180,.5f,.5f,0},c{180,212,.5f,0,.5f};
    auto farA=a,farB=b,farC=c;
    farA.depth=farB.depth=farC.depth=.25f;
    bool valid=true;
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
    std::cout << "Solid car raster: depth ordering, bridge visibility, near clipping, UV tiles and 1000 fuzz triangles\n";
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
    for(std::size_t i=0;i<kCarCount;++i) {
        GameFlow previewFlow;
        previewFlow.confirmInputAvailable(); previewFlow.completeCalibration(true);
        previewFlow.confirmPlayerCar();
        selection.reset(static_cast<CarId>(i));
        for(uint32_t time : {0u,120u,500u,900u,2500u}) {
            for(auto quality : {PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
                garage.render(previewFlow,selection,time,quality);
                checkText();
            }
        }
        save("showcase-"+std::to_string(i));
    }
    selection.reset();
    flow.confirmPlayerCar();
    garage.render(flow, selection, 1000u, PencilDetail::High);
    save("showcase");
    flow.completeCarShowcase();
    flow.toggleRival(CarId::HurricaneSonic);
    flow.toggleRival(CarId::NeoTridaggerZmc);
    flow.toggleRival(CarId::BrockenGigant);
    garage.render(flow, selection, 500u, PencilDetail::High);
    save("rivals");
    flow.confirmRivals();
    garage.render(flow, selection, 0u, PencilDetail::High);
    save("track");
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
    flow.togglePause();
    renderer.render(flow, race, results, 200u, true, PencilDetail::High);
    save("paused");
    const auto pausedPixels = canvas.frame();
    renderer.render(flow, race, results, 5000u, true, PencilDetail::High);
    if (pausedPixels != canvas.frame()) {
        std::cerr << "Paused image was not frozen\n"; valid = false;
    }
    flow.togglePause();
    flow.finishRace();
    flow.showResults();
    renderer.render(flow, race, results, 0u, false, PencilDetail::High);
    save("results");
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
    }
    // All-new and mixed rosters, including a solo quality change between full
    // grids. Reused caches must match a clean renderer byte for byte.
    for(unsigned cycle=0;cycle<6;++cycle) {
        RaceSetup setup;setup.playerCar=cycle<3 ? CarId::Diospada : CarId::SpinCobra;
        if(cycle%3!=1)setup.rivalMask=cycle<3 ? uint8_t(0x70) : uint8_t(0x89);
        RaceController run;run.prepare(setup,42);
        const auto detail=cycle%3==0 ? PencilDetail::High : PencilDetail::Low;
        renderer.render(flow,run,results,0,false,detail);
        const auto cached=canvas.frame();
        RaceRenderer clean;clean.open(466,466);clean.render(flow,run,results,0,false,detail);
        if(cached!=canvas.frame()) {std::cerr << "Roster/quality cache mismatch\n";valid=false;}
        clean.close();
        if(cycle==0)save("race-new-roster");
    }
    // Repeated entry releases large PSRAM-oriented storage instead of keeping
    // it resident in the Launcher for the lifetime of the installed App object.
    for(int cycle=0;cycle<3;++cycle) {
        garage.close();renderer.close();
        garage.open(466,466);renderer.open(466,466);
        renderer.render(flow,race,results,0,false,PencilDetail::High);
        checkText();
    }
    garage.close();renderer.close();
    std::cout << "Production renderer frames: " << directory << '\n';
    return valid ? 0 : 1;
}
