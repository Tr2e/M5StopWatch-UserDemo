#include "../main/apps/app_lets_and_go_racer/input/device_control_logic.h"
#include <iostream>

using namespace lets_and_go;
int main() {
    DeviceControlLogic controls;
    bool ok=true;
    const auto check=[&](bool value,const char* reason) {
        if(!value) { std::cerr<<reason<<'\n';ok=false; }
    };
    // A complete 100ms click happens while the consumer is stalled for 400ms.
    for(uint32_t t=0;t<=400;t+=10) controls.buttons(false,t>=50&&t<150,t);
    controls.touch(false,0,0);
    check(controls.consume(true).input.confirmPressed,"lost device mode selection");
    check(!controls.consume(true).input.confirmPressed,"replayed device mode selection");
    GameFlow flow;
    check(flow.useDeviceControls()&&flow.screen()==GameScreen::CarSelect,"explicit local mode failed");
    check(!flow.useDeviceControls(),"local mode reset active selection");
    controls.setScreen(GameScreen::CarSelect);
    controls.buttons(false,false,410);
    controls.touch(false,0,0);
    controls.touch(true,360,240);
    controls.touch(false,0,0);
    check(controls.consume(true).navigation==1,"lost menu touch during slow frame");
    check(controls.consume(true).navigation==0,"replayed menu navigation");
    controls.touch(true,233,140);
    check(controls.consume(true).view==1,"view target failed");
    controls.setScreen(GameScreen::RivalSelect);
    controls.touch(true,233,140);
    check(!controls.consume(true).input.confirmPressed,"held touch leaked across screens");
    controls.setScreen(GameScreen::Racing);
    controls.touch(false,0,0);
    controls.buttons(false,false,420);
    controls.touch(true,373,240);
    check(controls.consume(true).input.steer==1.f,"touch steering failed");
    check(!controls.consume(false).input.valid,"unhealthy input reported ready");
    controls.touch(false,0,0);
    check(controls.consume(true).input.steer==0,"released steering stuck");
    for(uint32_t t=430;t<=1030;t+=10) controls.buttons(true,false,t);
    auto input=controls.consume(true).input;
    check(input.brakeHeld&&!input.pausePressed&&!input.cancelPressed,"brake hold paused/backed out");
    controls.touch(true,233,80);
    check(controls.consume(true).input.pausePressed,"touch pause failed");
    controls.setScreen(GameScreen::Paused);
    controls.touch(true,233,80);
    check(!controls.consume(true).input.pausePressed,"pause touch immediately resumed");
    controls.touch(false,0,0);
    controls.touch(true,233,233);
    check(controls.consume(true).input.pausePressed,"resume tap failed");
    for(uint32_t t=1040;t<=2400;t+=10) controls.buttons(t<2100,t<2100,t);
    input=controls.consume(true).input;
    check(input.exitPressed&&!input.confirmPressed&&!input.pausePressed&&!input.boostHeld,
          "exit chord lost or leaked another action");
    check(!controls.consume(true).input.exitPressed,"exit replayed");
    controls.reset();
    check(!controls.consume(true).input.exitPressed,"reopen kept old exit");
    if(ok)std::cout<<"Device controls: slow-frame clicks/taps, source choice, steering, pause, chord, screen/reopen isolation passed\n";
    return ok?0:1;
}
