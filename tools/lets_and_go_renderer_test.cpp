#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_renderer.h"
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

bool validateTrackPaint()
{
    OverpassTrack course;
    PencilTrack geometry;
    geometry.open(course);
    PencilOcclusion occlusion;
    auto& canvas=GetHAL().getCanvas();
    bool valid=true;
    std::size_t maxSurfaces=0;
    for(auto detail : {PencilDetail::Low,PencilDetail::Medium,PencilDetail::High}) {
        for(int sample=0;sample<48;++sample) {
            const auto frame=course.sample(course.length()*sample/48);
            const auto camera=makeRacerChaseCamera(frame,0,466,466);
            canvas.fillScreen(0xef3a);
            drawPencilTrack(canvas,camera,geometry,detail,&occlusion);
            maxSurfaces=std::max(maxSurfaces,occlusion.count);
            if(occlusion.overflowed || occlusion.count==0 ||
               occlusion.count>4*PencilTrack::kSegments) valid=false;
            for(std::size_t i=0;i<occlusion.count;++i)
                if(occlusion.surfaces[i].count<3) valid=false;
            const auto& pixels=canvas.frame();
            if(std::count(pixels.begin(),pixels.end(),track_paint::coral)<8 ||
               std::count(pixels.begin(),pixels.end(),track_paint::chalk)<8)
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
    std::cout << "Track paint: 144 full-course/detail poses, max occluders="
              << maxSurfaces << '\n';
    if(!valid) std::cerr << "Track paint geometry, palette or bridge occlusion regressed\n";
    return valid;
}

int main(int argc, char** argv)
{
    const std::string directory = argc > 1 ? argv[1] : "/tmp/lets-go-frames";
    std::filesystem::create_directories(directory);
    auto& canvas = GetHAL().getDisplay();
    bool valid = validateCarRaster() && validateTrackPaint();
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
    for (unsigned orbit = 0; orbit < 16u; ++orbit) {
        garage.render(flow, selection, orbit * 2454u, PencilDetail::Low);
        checkText();
        const auto& pixels=canvas.frame();
        for(int y=0;y<466;++y) for(int x=0;x<466;++x) {
            const auto color=pixels[y*466+x];
            if(color!=track_paint::road && color!=track_paint::roadLight &&
               color!=track_paint::coral && color!=track_paint::fascia) continue;
            const int dx=x-233,dy=y-233;
            if(y<110 || y>356 || dx*dx+dy*dy>220*220) {
                std::cerr << "Track overview entered text/screen margin at " << x << "," << y << '\n';
                valid=false;
                break;
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
