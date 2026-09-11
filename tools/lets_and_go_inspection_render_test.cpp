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
    presentation.presented(GameScreen::CarSelect,car);
    assert(presentation.partial(GameScreen::CarSelect,car));
    assert(!presentation.partial(GameScreen::CarSelect,CarId::HurricaneSonic));
    presentation.presented(GameScreen::CarInspect,car);
    assert(!presentation.partial(GameScreen::CarSelect,car));
    GarageViewController preview;preview.reset(car,0);
    assert(preview.renderPercent(0)==100);
    preview.drag({1,30,20,true,true},10);
    assert(preview.renderPercent(10)==65 && preview.renderPercent(1000)==65);
    preview.endDrag(1000);
    assert(preview.renderPercent(1519)==65 && preview.renderPercent(1520)==100);
    preview.changeView(1,1600);
    assert(preview.renderPercent(1800)==65 && preview.renderPercent(2200)==100);
    assert(preview.animating(2000)); // Native SIDE wheel rotation.
    preview.selectCar(CarId::HurricaneSonic,2300);
    assert(preview.renderPercent(2500)==65 && preview.renderPercent(3000)==100);
    preview.drag({2,10,10,true,true},3100);preview.endDrag(3200);
    preview.drag({3,5,5,true,true},3210);assert(preview.renderPercent(3220)==65);
    preview.reset(car,UINT32_MAX-100);preview.changeView(1,UINT32_MAX-100);
    assert(preview.renderPercent(418)==65 && preview.renderPercent(419)==100);
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
    assert(!quality.update(799,false,false));
    assert(quality.update(900,false,false) && quality.percent()==65 && quality.displayPercent()>85);
    const int recoveringDisplay=quality.displayPercent();
    assert(quality.update(1100,false,false) && quality.percent()==65 &&
           quality.displayPercent()>recoveringDisplay && quality.displayPercent()<100);
    assert(quality.update(1320,false,false) && quality.percent()==100 && quality.displayPercent()==100);
    assert(!quality.update(1400,false,false));
    // A slow frame can deliver a whole short gesture, then skip the intermediate.
    assert(camera.drag({2,0,20,false,true}));
    assert(quality.update(1500,true,false));
    assert(quality.update(2200,false,false) && quality.percent()==100);
    // A held joystick remains compact throughout its repeat delay and at a limit.
    quality.update(2300,true,true);
    for(uint32_t time=2310;time<3100;time+=10)
        assert(!quality.update(time,false,true) && quality.percent()==65);
    assert(!quality.update(3100,false,false));
    assert(quality.update(3300,false,false) && quality.percent()==65 && quality.displayPercent()>85);
    // Touching again during recovery cancels enlargement even before movement.
    assert(quality.update(3310,false,true) && quality.percent()==65);
    assert(!quality.update(3500,false,true));
    assert(!quality.update(3600,false,false));
    assert(quality.update(3800,false,false) && quality.percent()==65 && quality.displayPercent()>85);
    assert(quality.update(3810,true,false) && quality.percent()==65);
    assert(quality.update(4430,false,false) && quality.percent()==100);
    camera.drag({3,10,10,true,true});quality.update(4500,true,true);
    camera.release();quality.update(4600,false,camera.touchActive());
    assert(quality.update(5220,false,false) && quality.percent()==100);
    quality.reset();
    quality.update(UINT32_MAX-50,true,false);
    assert(quality.percent()==65 && !quality.update(48,false,false));
    assert(quality.update(149,false,false) && quality.percent()==65 && quality.displayPercent()>85);
    assert(quality.update(569,false,false) && quality.percent()==100);
    InspectionAutoController tour;
    GarageViewState origin{};origin.yaw=1.1f;origin.pitch=.22f;
    assert(!tour.enabled());tour.start(1000,origin);assert(tour.enabled());
    assert(std::abs(tour.state(1000).yaw-origin.yaw)<.0001f);
    const auto front=tour.state(1000+InspectionAutoController::kEntryMs);
    assert(std::abs(front.yaw+.65f)<.0001f && std::abs(front.pitch-.5713375f)<.0001f);
    const auto side=tour.state(1000+InspectionAutoController::kEntryMs+8000);
    assert(std::abs(side.yaw+1.5707963f)<.0001f && std::abs(side.pitch-.34f)<.0001f);
    // A completed tour loops the camera only; car changes remain user-driven.
    const auto looped=tour.state(1000+InspectionAutoController::kEntryMs+
                                 InspectionAutoController::kTourMs);
    assert(std::abs(looped.yaw+.65f)<.0001f && std::abs(looped.pitch-.5713375f)<.0001f);
    tour.stop();assert(!tour.enabled());
    tour.start(UINT32_MAX-1000,origin);
    const auto wrapped=tour.state(InspectionAutoController::kEntryMs+31000-1001);
    assert(std::isfinite(wrapped.yaw) && std::isfinite(wrapped.pitch));
    std::cout<<"Inspection render: compact touch/stick, eased recovery, re-grab, slow-frame flick, cancellation and rollover passed\n";
}
