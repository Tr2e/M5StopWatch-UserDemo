#pragma once
#include "../model/character_model.h"
#include "../view/arena_renderer.h"
#include "../../app_lets_and_go_racer/input/device_control_logic.h"
#include <algorithm>
#include <cmath>

namespace gundam_arena {
inline void updateFollowView(ArenaView& view,const CharacterModel& c,float dt){
    const float a=1.f-std::exp(-dt*7.f);
    view.lookX+=(c.x-view.lookX)*a;
    view.lookZ+=(c.z-view.lookZ)*a;
    view.lookY+=(c.y*.35f+1.18f-view.lookY)*a;
    const float want=kPi+view.orbit;
    view.camYaw+=std::remainder(want-view.camYaw,2.f*kPi)*a;
}

inline void mergeExternalPad(lets_and_go::DeviceControlFrame& device,const lets_and_go::RacerInput& pad){
    device.input.confirmPressed|=pad.confirmPressed;
    device.input.pausePressed|=pad.pausePressed;
    device.input.exitPressed|=pad.exitPressed;
    device.input.cancelPressed|=pad.cancelPressed;
    if(pad.navigationStep)device.navigation=pad.navigationStep;
    if(pad.valid){
        device.input.steer=pad.steer;
        device.input.viewAxis=pad.viewAxis;
        device.input.valid=true;
    }else if(pad.confirmPressed||pad.pausePressed||pad.exitPressed)device.input.valid=true;
}

class ArenaController {
public:
    void reset(){*this={};resetCharacter(_character);}
    CharacterModel& character(){return _character;}
    const CharacterModel& character()const{return _character;}
    const ArenaView& view()const{return _view;}
    bool exitRequested()const{return _exit;}
    void setPadHint(uint8_t hint){_view.padHint=hint;}
    bool update(const lets_and_go::DeviceControlFrame& input,uint32_t now){
        if(input.input.exitPressed){_exit=true;return false;}
        ArenaInput in{};
        in.valid=input.input.valid;
        if(in.valid){
            in.turn=-input.input.steer;
            in.forward=input.input.viewAxis;
            if(_character.mode==Mode::Play){
                in.jump=input.input.confirmPressed;
                if(input.input.pausePressed)in.toggleMode=true;
                if(input.preview.changed && input.preview.active){
                    _view.orbit=std::remainder(_view.orbit+(input.preview.dx-_lastDx)*.012f,2.f*kPi);
                    _view.pitch=std::clamp(_view.pitch+(input.preview.dy-_lastDy)*.008f,-75.f*kPi/180.f,.55f);
                }
            }else{
                in.jointStep=input.navigation;
                if(input.preview.changed && input.preview.active){
                    in.poseYaw=(input.preview.dx-_lastDx)*.012f;
                    in.posePitch=-(input.preview.dy-_lastDy)*.010f;
                }
                in.poseReset=input.input.confirmPressed;
                if(input.input.pausePressed)in.toggleMode=true;
            }
            _lastDx=input.preview.dx;_lastDy=input.preview.dy;
            if(!input.preview.active)_lastDx=_lastDy=0;
        }else{in.forward=0;in.turn=0;}
        const uint32_t elapsed=_last==0?16:std::min<uint32_t>(now-_last,80);
        _last=now;
        _accumulator+=elapsed*.001f;
        int steps=0;
        while(_accumulator>=kStep && steps<5){
            stepCharacter(_character,in,kStep);
            updateFollowView(_view,_character,kStep);
            in.jump=false;in.toggleMode=false;in.jointStep=0;in.poseYaw=0;in.posePitch=0;
            _accumulator-=kStep;++steps;
        }
        if(steps==5)_accumulator=0;
        _view.percent=_character.mode==Mode::Pose?100:70;
        return true;
    }
private:
    CharacterModel _character{};
    ArenaView _view{};
    uint32_t _last=0;
    float _accumulator=0;
    int _lastDx=0,_lastDy=0;
    bool _exit=false;
};
} // namespace gundam_arena
