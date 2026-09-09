#include "../main/apps/app_lets_and_go_racer/input/device_control_logic.h"
#include <iostream>
#include <utility>

using namespace lets_and_go;
int main() {
    DeviceControlLogic controls;
    bool ok=true;
    const auto check=[&](bool value,const char* reason) {
        if(!value) {std::cerr<<reason<<'\n';ok=false;}
    };
    const auto show=[&](GameScreen screen) {
        controls.setScreen(screen);controls.presentScreen(screen);controls.touch(false,0,0);
    };
    const auto tap=[&](int x,int y) {
        controls.touch(false,0,0);controls.touch(true,x,y);controls.touch(false,0,0);
        return controls.consume(true);
    };
    const auto visibleTarget=[&](GameScreen screen,home_layout::Rect rect,TouchAction action) {
        for(int y=rect.y;y<rect.y+rect.height;++y)for(int x=rect.x;x<rect.x+rect.width;++x)
            if(menuTouchTarget(screen,x,y)!=action) {
                check(false,"visible button pixel maps to another action");return;
            }
    };
    visibleTarget(GameScreen::CarSelect,home_layout::viewAction,TouchAction::View);
    visibleTarget(GameScreen::CarSelect,home_layout::carSelect,TouchAction::Confirm);
    visibleTarget(GameScreen::RivalSelect,home_layout::rivalToggle,TouchAction::Confirm);
    visibleTarget(GameScreen::RivalSelect,home_layout::setupNext,TouchAction::Advance);
    visibleTarget(GameScreen::TrackSelect,home_layout::setupNext,TouchAction::Confirm);
    for(int i=0;i<3;++i)visibleTarget(GameScreen::Results,race_ui_layout::resultRow(i),
                                    static_cast<TouchAction>(int(TouchAction::Retry)+i));
    show(GameScreen::InputCheck);
    // Entire GPIO clicks and touch gestures survive a stalled consumer.
    for(uint32_t t=0;t<=400;t+=10)controls.buttons(false,t>=50&&t<150,t);
    check(controls.consume(true).input.confirmPressed,"lost slow-frame button click");
    check(!controls.consume(true).input.confirmPressed,"button click replayed");
    check(!tap(233,331).input.confirmPressed,"status card selected device mode");
    check(tap(233,248).input.confirmPressed,"PLAY button failed");
    show(GameScreen::InputCalibration);
    check(!tap(233,184).input.confirmPressed,"calibration bar confirmed");
    check(tap(233,248).input.confirmPressed,"calibration PLAY failed");
    GameFlow flow;
    check(flow.useDeviceControls() && !flow.useDeviceControls(),"device mode re-entered selection");
    show(GameScreen::CarSelect);
    check(tap(123,366).navigation==-1 && tap(345,366).navigation==1,"visible car arrows failed");
    check(tap(233,98).view==1,"view chip failed");
    check(tap(116,70).input.cancelPressed,"visible back arrow failed");
    check(tap(233,420).input.confirmPressed,"SELECT failed");
    check(!tap(233,240).input.confirmPressed,"hidden car-area confirm remained");
    check(tap(360,240).navigation==0,"hidden side navigation remained");
    check(tap(233,150).view==0,"hidden view hot zone remained");
    show(GameScreen::RivalSelect);
    check(!tap(233,240).input.confirmPressed,"rival model toggled selection");
    check(!tap(233,122).input.cancelPressed,"selected slots backed out");
    check(tap(233,364).input.confirmPressed,"ADD/REMOVE failed");
    // No action on first contact; use the final stable point inside the same target.
    controls.touch(true,233,416);
    check(!controls.consume(true).advance,"NEXT activated on press");
    controls.touch(true,235,414);controls.touch(false,0,0);
    auto next=controls.consume(true);
    check(next.advance&&!next.input.confirmPressed&&next.touchTrace.accepted,"NEXT release failed");
    check(!controls.consume(true).advance,"NEXT replayed");
    for(const auto p:{std::pair<int,int>{142,395},{324,436},{139,416},{233,440}})
        check(tap(p.first,p.second).advance,"NEXT edge or allowance missed");
    controls.touch(true,233,388);controls.touch(true,233,398);controls.touch(false,0,0);
    check(controls.consume(true).advance,"initial contact settling failed");
    controls.touch(true,233,416);controls.touch(true,233,364);controls.touch(false,0,0);
    auto dragged=controls.consume(true);
    check(!dragged.advance&&!dragged.input.confirmPressed,"drag executed neighboring ADD button");
    controls.touch(true,233,416);controls.invalidateTouch();controls.touch(false,0,0);
    check(!controls.consume(true).advance,"read failure became a release click");
    controls.touch(true,233,416);controls.touch(false,0,0);
    check(controls.consume(true).advance,"touch did not recover after valid release");
    // A new page cannot accept input before its first canvas is presented.
    controls.setScreen(GameScreen::TrackSelect);
    check(!tap(233,416).input.confirmPressed,"unpresented page accepted tap");
    controls.touch(true,233,416);controls.presentScreen(GameScreen::TrackSelect);
    controls.touch(true,233,416);controls.touch(false,0,0);
    check(!controls.consume(true).input.confirmPressed,"held touch crossed presentation gate");
    check(tap(233,416).input.confirmPressed,"START RACE failed after release");
    check(!tap(233,240).input.confirmPressed,"track preview started race");
    check(tap(345,358).navigation==1,"visible course arrow failed");
    show(GameScreen::Racing);controls.buttons(false,false,410);
    controls.touch(true,374,240);
    check(controls.consume(true).input.steer==1.f,"live steering failed");
    controls.touch(false,0,0);check(controls.consume(true).input.steer==0,"released steer stuck");
    for(uint32_t t=420;t<=1030;t+=10)controls.buttons(true,false,t);
    auto input=controls.consume(true).input;
    check(input.brakeHeld&&!input.pausePressed&&!input.cancelPressed,"brake hold backed out");
    controls.touch(true,234,55);check(controls.consume(true).input.pausePressed,"top pause failed");
    controls.setScreen(GameScreen::Paused);controls.presentScreen(GameScreen::Paused);
    controls.touch(true,233,233);controls.touch(false,0,0);
    check(!controls.consume(true).input.pausePressed,"pause touch immediately resumed");
    check(tap(233,233).input.pausePressed,"resume panel failed");
    for(uint32_t t=1040;t<=2400;t+=10)controls.buttons(t<2100,t<2100,t);
    controls.setScreen(GameScreen::Results); // Finish may race with a buffered exit.
    input=controls.consume(true).input;
    check(input.exitPressed&&!input.confirmPressed&&!input.pausePressed&&!input.boostHeld,"exit chord leaked action");
    check(!controls.consume(true).input.exitPressed,"exit replayed");
    controls.reset();show(GameScreen::Results);
    for(int i=0;i<3;++i) {
        const auto rect=race_ui_layout::resultRow(i);
        auto result=tap(rect.x+rect.width/2,rect.y+rect.height/2);
        check(result.resultAction==i&&!result.input.confirmPressed,"result row selected wrong action");
    }
    check(tap(233,216).resultAction==-1,"time card activated action");
    check(tap(-1,416).resultAction==-1 && tap(1000,416).resultAction==-1,"invalid coordinates activated action");
    controls.touch(true,233,316);controls.touch(false,0,0);
    controls.setScreen(GameScreen::CarSelect);controls.presentScreen(GameScreen::CarSelect);
    auto cleared=controls.consume(true);
    check(cleared.resultAction==-1&&!cleared.input.confirmPressed,"old result action crossed screen");
    if(ok)std::cout<<"Device controls: precise targets, release taps, settling/drag cancellation, read failure, presentation/release gates, slow GPIO, steering, chord and results passed\n";
    return ok?0:1;
}
