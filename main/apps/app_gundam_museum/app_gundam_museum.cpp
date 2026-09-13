#include "app_gundam_museum.h"
#include "../common/performance/display_frame_scope.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <esp_timer.h>

AppGundamMuseum::AppGundamMuseum(){setAppInfo().name="Gundam Museum";setAppInfo().icon=(void*)&icon_gundam_museum;}
void AppGundamMuseum::onOpen(){
    GetHAL().stopLvglUpdate();GetHAL().lvglLock();GetHAL().lvglUnlock();
    _controller.reset();_lastLog=0;_presented=false;
    _direct=GetHAL().hasDisplayFrameBuffer();
    const bool ready=_renderer.open();
    mclog::tagInfo("Museum","open ready={} working_bytes={}",ready,_renderer.workingBytes());
    _input.open();_input.setScreen(lets_and_go::GameScreen::CarInspect);
    draw(GetHAL().millis());_input.presentScreen(lets_and_go::GameScreen::CarInspect);
}
void AppGundamMuseum::onRunning(){
    auto input=_input.sample(GetHAL().millis());
    const uint32_t now=GetHAL().millis();
    const bool dirty=_controller.update(input,now);
    if(_controller.exitRequested()){close();return;}
    if(dirty)draw(now);
    GetHAL().delay(5);
}
void AppGundamMuseum::draw(uint32_t now){
    const uint64_t start=esp_timer_get_time();
    const bool partial=_presented && _lastAuto==_controller.view().automatic;
    uint64_t rendered=0;
    if(_direct){
        auto& display=GetHAL().getDisplay();
        const app_performance::DisplayRegion region=partial?app_performance::DisplayRegion{40,100,display.width()-80,288}:
            app_performance::DisplayRegion{0,0,display.width(),display.height()};
        app_performance::DisplayFrameScope frame(display,region);
        _renderer.render(display,_controller.view(),_controller.percent(now),true,false,false,partial);
        rendered=esp_timer_get_time();frame.finish();
    }else{
        _renderer.render(GetHAL().getCanvas(),_controller.view(),_controller.percent(now),true,false,false,partial);
        rendered=esp_timer_get_time();
        if(partial)GetHAL().updateCanvasRegion(40,100,GetHAL().getCanvas().width()-80,288);
        else GetHAL().updateCanvas();
    }
    _presented=true;_lastAuto=_controller.view().automatic;
    if(now-_lastLog>=2000){
        _lastLog=now;const auto stats=_renderer.stats();
        mclog::tagInfo("Museum","panels={} culled={} submitted={} scale={} draw_us={} present_us={}",
            stats.total,stats.culled,stats.submitted,_controller.percent(now),
            uint32_t(rendered-start),uint32_t(esp_timer_get_time()-rendered));
    }
}
void AppGundamMuseum::onClose(){
    _input.close();_renderer.close();_controller.reset();_direct=false;_presented=false;
    GetHAL().startLvglUpdate();
}
