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
    int percent(uint32_t now)const{
        // Nu hidden-line drag fills a 65% Z-buffer; strokes are expanded to 1px.
        if(_view.model==ModelId::NuGundam)return _touch?65:100;
        return (_touch || now-_lastMotion<180?65:100);
    }
    bool update(const lets_and_go::DeviceControlFrame& input,uint32_t now){
        bool dirty=false;
        // Exit remains usable when the touchscreen is invalid/stale.
        if(input.input.exitPressed || input.input.cancelPressed){_exit=true;return false;}
        if(input.input.valid){
            if(input.navigation){
                // One complete exhibit per model; study views remain host-only.
                // Zaku, Sazabi, Strike and Destiny stay in the renderer for
                // host tests, but are not in the product browse cycle.
                constexpr ModelId models[]={
                    ModelId::Rx78,
                    // ModelId::CharZaku,
                    ModelId::NuGundam,
                    // ModelId::Sazabi,
                    // ModelId::StrikeGundam,
                    // ModelId::DestinyGundam,
                };
                constexpr int count=int(sizeof(models)/sizeof(models[0]));
                _mode=(_mode+(input.navigation>0?1:count-1))%count;
                _view.model=models[_mode];
                _view.equipment=true;_view.detail=false;
                _view.automatic=false;_touch=false;_blockedGesture=true;dirty=true;
            }
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
        const int quality=percent(now);dirty|=quality!=_lastQuality;_lastQuality=quality;
        return dirty;
    }
private:
    View _view{};
    uint32_t _lastMotion=uint32_t(0)-1000,_gesture=0;
    int _lastDx=0,_lastDy=0,_mode=0,_lastQuality=100;
    bool _touch=false,_blockedGesture=false,_exit=false;
};
} // namespace gundam_museum
