#include "app_gundam_arena.h"
#include "../common/performance/display_frame_scope.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <esp_timer.h>

AppGundamArena::AppGundamArena(){
    setAppInfo().name="Gundam Arena";
    setAppInfo().icon=(void*)&icon_gundam_museum;
}
void AppGundamArena::onOpen(){
    GetHAL().stopLvglUpdate();
    GetHAL().lvglLock();
    GetHAL().lvglUnlock();
    _keys=std::make_unique<input::KeyManager>();
    _controller.reset();_lastFrame=0;_lastLog=0;
    _direct=GetHAL().hasDisplayFrameBuffer();
    _padNavMode=gundam_arena::Mode::Play;
    const bool ready=_renderer.open();
    mclog::tagInfo("Arena","open ready={} working_bytes={} panels={} mesh_builds={}",
        ready,_renderer.workingBytes(),ready?_renderer.mesh().count:0,_renderer.meshBuilds());
    _externalPower=GetHAL().setGrove5VPower(true);
    GetHAL().delay(20);
    if(_externalPower){
        _pad=std::make_unique<lets_and_go::HardwareRacerInputProvider>();
        _pad->open();
        _pad->presentScreen();
        mclog::tagInfo("Arena","controls: Joystick2 + Dual Button");
    }else mclog::tagWarn("Arena","Grove 5V unavailable; touch fallback only");
    _input.open();
    _input.setScreen(lets_and_go::GameScreen::ArenaPlay);
    draw(GetHAL().millis());
    _input.presentScreen(lets_and_go::GameScreen::ArenaPlay);
    if(_pad)_pad->presentScreen();
}
void AppGundamArena::onRunning(){
    GetHAL().updateButtonStates();
    if(_keys && _keys->update(false)==input::KeyEvent::GoHome){close();return;}
    const uint32_t now=GetHAL().millis();
    auto input=_input.sample(now);
    if(_pad){
        const auto pad=_pad->sample(now);
        gundam_arena::mergeExternalPad(input,pad);
        const auto status=_pad->status(now);
        _controller.setPadHint(status.readiness==lets_and_go::RacerInputReadiness::Fault?3:
            status.readiness==lets_and_go::RacerInputReadiness::Ready?2:
            status.readiness==lets_and_go::RacerInputReadiness::Calibrating?1:0);
        const auto mode=_controller.character().mode;
        if(mode!=_padNavMode){
            _padNavMode=mode;
            _pad->setNavigationMode(mode==gundam_arena::Mode::Pose?
                lets_and_go::RacerNavigationMode::Horizontal:lets_and_go::RacerNavigationMode::None);
            _pad->presentScreen();
        }
    }
    if(!_controller.update(input,now) || _controller.exitRequested()){close();return;}
    if(_lastFrame!=0 && now-_lastFrame<33){GetHAL().delay(5);return;}
    _lastFrame=now;
    draw(now);
    GetHAL().delay(5);
}
void AppGundamArena::draw(uint32_t now){
    const uint64_t start=esp_timer_get_time();
    uint64_t rendered=0;
    if(_direct){
        auto& display=GetHAL().getDisplay();
        const app_performance::DisplayRegion region{0,0,display.width(),display.height()};
        app_performance::DisplayFrameScope frame(display,region);
        _renderer.render(display,_controller.character(),_controller.view());
        rendered=esp_timer_get_time();frame.finish();
    }else{
        _renderer.render(GetHAL().getCanvas(),_controller.character(),_controller.view());
        rendered=esp_timer_get_time();
        GetHAL().updateCanvas();
    }
    if(now-_lastLog>=2000){
        _lastLog=now;const auto stats=_renderer.stats();
        mclog::tagInfo("ArenaStage","panels={} submitted={} mesh_builds={} index_builds={} draw_us={} present_us={}",
            stats.total,stats.submitted,_renderer.meshBuilds(),_renderer.indexBuilds(),
            uint32_t(rendered-start),uint32_t(esp_timer_get_time()-rendered));
    }
}
void AppGundamArena::onClose(){
    _input.close();
    if(_pad)_pad->close();
    _pad.reset();
    if(_externalPower)GetHAL().setGrove5VPower(false);
    _externalPower=false;
    _renderer.close();_controller.reset();_keys.reset();_direct=false;
    GetHAL().startLvglUpdate();
}
