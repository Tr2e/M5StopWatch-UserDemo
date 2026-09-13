#pragma once
#include "../view/museum_renderer.h"
#include "../../app_lets_and_go_racer/input/device_control_logic.h"
#include <algorithm>
#include <cmath>

namespace gundam_museum {
class MuseumController {
public:
    void reset(){*this={};}
    const View& view()const{return _view;}
    bool exitRequested()const{return _exit;}
    int percent(uint32_t now)const{return _view.automatic?90:(_touch || now-_lastMotion<180?65:100);}
    bool update(const lets_and_go::DeviceControlFrame& input,uint32_t now){
        const uint32_t elapsed=_hasTime?now-_lastTime:0;_lastTime=now;_hasTime=true;
        bool dirty=false;
        // Exit remains usable when the touchscreen is invalid/stale.
        if(input.input.exitPressed || input.input.cancelPressed){_exit=true;return false;}
        if(input.input.confirmPressed){
            _view.yaw=-.40f;_view.pitch=.10f;_view.automatic=false;_touch=false;_blockedGesture=true;dirty=true;
        }
        if(input.input.valid){
            if(input.navigation){
                _mode=(_mode+(input.navigation>0?1:2))%3;
                _view.equipment=_mode==0;_view.detail=_mode==2;
                _view.automatic=false;_touch=false;_blockedGesture=true;dirty=true;
            }
            if(input.autoToggle){_view.automatic=!_view.automatic;_touch=false;_blockedGesture=true;dirty=true;}
            const auto& drag=input.preview;
            if(drag.changed){
                if(drag.gesture!=_gesture){_gesture=drag.gesture;_lastDx=_lastDy=0;_blockedGesture=false;}
                if(!_blockedGesture){
                    const int dx=drag.dx-_lastDx,dy=drag.dy-_lastDy;
                    if(dx || dy){
                        _view.yaw=std::remainder(_view.yaw+dx*.012f,6.2831853f);
                        _view.pitch=std::clamp(_view.pitch+dy*.008f,-.20f,.70f);
                        _view.automatic=false;_lastMotion=now;dirty=true;
                    }
                    _lastDx=drag.dx;_lastDy=drag.dy;_touch=drag.active;
                }
            }
        }else{_touch=false;_blockedGesture=true;}
        if(_view.automatic && elapsed){
            _view.yaw=std::remainder(_view.yaw-float(elapsed%20000)*6.2831853f/20000.f,6.2831853f);dirty=true;
        }
        const int quality=percent(now);dirty|=quality!=_lastQuality;_lastQuality=quality;
        return dirty;
    }
private:
    View _view{};
    uint32_t _lastTime=0,_lastMotion=uint32_t(0)-1000,_gesture=0;
    int _lastDx=0,_lastDy=0,_mode=0,_lastQuality=100;
    bool _hasTime=false,_touch=false,_blockedGesture=false,_exit=false;
};
} // namespace gundam_museum
