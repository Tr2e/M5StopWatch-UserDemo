#include "../main/apps/app_lets_and_go_racer/controller/car_inspection_controller.h"
#include "../main/apps/app_lets_and_go_racer/controller/inspection_render_policy.h"
#include "../main/apps/app_lets_and_go_racer/controller/inspection_presentation.h"
#include "../main/apps/app_lets_and_go_racer/view/inspection_frame_window.h"
#include <cassert>
#include <iostream>
using namespace lets_and_go;
int main() {
    InspectionFrameWindow frames;
    assert(frames.summary().count==0);
    for(unsigned i=1;i<=20;++i)frames.record(CarId::CycloneMagnum,77,i*100,10);
    auto measured=frames.summary();
    assert(measured.count==20 && measured.drawUs==1050 && measured.presentUs==10);
    assert(measured.p95Us==1910 && measured.maxUs==2010);
    frames.record(CarId::CycloneMagnum,100,300,20);
    assert(frames.summary().count==1 && frames.summary().p95Us==320);
    frames.record(CarId::HurricaneSonic,100,200,20);
    assert(frames.summary().count==1 && frames.summary().maxUs==220);
    frames.reset();
    for(unsigned i=1;i<=100;++i)frames.record(CarId::CycloneMagnum,77,i*100,10);
    measured=frames.summary();
    assert(measured.count==64 && measured.drawUs==6850 && measured.p95Us==9710 && measured.maxUs==10010);
    InspectionPresentation presentation;
    const auto car=CarId::CycloneMagnum;
    assert(!presentation.partial(GameScreen::CarInspect,car));
    presentation.presented(GameScreen::CarSelect,car);
    assert(!presentation.partial(GameScreen::CarInspect,car));
    // A transition that has not been presented yet must still draw in full.
    assert(!presentation.partial(GameScreen::CarInspect,car));
    presentation.presented(GameScreen::CarInspect,car);
    assert(presentation.partial(GameScreen::CarInspect,car));
    assert(!presentation.partial(GameScreen::CarInspect,CarId::HurricaneSonic));
    assert(!presentation.partial(GameScreen::CarSelect,car));
    presentation.presented(GameScreen::CarSelect,car);
    assert(!presentation.partial(GameScreen::CarInspect,car));
    presentation.reset();assert(!presentation.partial(GameScreen::CarInspect,car));
    CarInspectionController camera;
    InspectionRenderPolicy quality;
    assert(quality.percent()==100 && quality.displayPercent()==100);
    assert(!quality.update(0,false,true)); // A stationary touch does not shrink.
    assert(camera.drag({1,20,20,true,true}));
    assert(quality.update(10,true,camera.touchActive()));
    assert(quality.percent()==65 && quality.displayPercent()==85);
    assert(!camera.drag({1,20,20,true,true}));
    assert(!quality.update(500,false,true)); // Held touch does not bounce in size.
    assert(!camera.drag({1,20,20,false,true}));
    assert(!quality.update(700,false,false));
    assert(!quality.update(849,false,false));
    assert(quality.update(850,false,false) && quality.percent()==82 && quality.displayPercent()==93);
    assert(!quality.update(999,false,false));
    assert(quality.update(1000,false,false) && quality.percent()==100 && quality.displayPercent()==100);
    assert(!quality.update(1100,false,false));
    // A slow frame can deliver a whole short gesture, then skip the intermediate.
    assert(camera.drag({2,0,20,false,true}));
    assert(quality.update(1200,true,false));
    assert(quality.update(1700,false,false) && quality.percent()==100);
    // A held joystick remains compact throughout its repeat delay and at a limit.
    quality.update(1800,true,true);
    for(uint32_t time=1810;time<2600;time+=10)
        assert(!quality.update(time,false,true) && quality.percent()==65);
    assert(!quality.update(2600,false,false));
    assert(quality.update(2750,false,false) && quality.percent()==82);
    // Touching again during recovery cancels enlargement even before movement.
    assert(quality.update(2760,false,true) && quality.percent()==65);
    assert(!quality.update(3000,false,true));
    assert(!quality.update(3100,false,false));
    assert(quality.update(3250,false,false) && quality.percent()==82);
    assert(quality.update(3260,true,false) && quality.percent()==65);
    assert(quality.update(3560,false,false) && quality.percent()==100);
    camera.drag({3,10,10,true,true});quality.update(4000,true,true);
    camera.release();quality.update(4100,false,camera.touchActive());
    assert(quality.update(4400,false,false) && quality.percent()==100);
    quality.reset();
    quality.update(UINT32_MAX-50,true,false);
    assert(quality.percent()==65 && !quality.update(98,false,false));
    assert(quality.update(99,false,false) && quality.percent()==82);
    assert(quality.update(249,false,false) && quality.percent()==100);
    std::cout<<"Inspection render: compact touch/stick, staged recovery, re-grab, slow-frame flick, cancellation and rollover passed\n";
}
