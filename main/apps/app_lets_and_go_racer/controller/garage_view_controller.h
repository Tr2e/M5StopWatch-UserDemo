#pragma once
#include "../model/car_catalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace lets_and_go {

enum class GarageView : uint8_t { Front, Side, Rear, Top, Count };
struct GarageViewState {
    float yaw=-.65f,pitch=.5713375f,scale=140.f,centerY=252.f;
    float carSlide=0.f,carZoom=1.f;
    float wheelPhase=0.f;
    GarageView preset=GarageView::Front;
};
inline const char* garageViewLabel(GarageView view) {
    switch(view) {
        case GarageView::Side:return "SIDE";
        case GarageView::Rear:return "REAR";
        case GarageView::Top:return "TOP";
        default:return "FRONT 3/4";
    }
}

// Presentation clock is monotonic app time, independent of menu selection's
// screenElapsedMs. Retarget from the current pose, not the previous preset.
class GarageViewController {
public:
    static constexpr uint32_t kTransitionMs=350;
    void reset(CarId car,uint32_t nowMs) {
        _from=_to={};_view=GarageView::Front;_car=car;
        _viewStarted=nowMs;_carStarted=nowMs;_viewMoving=false;_carMoving=false;
        _restWheelPhase=0;
    }
    GarageViewState state(uint32_t nowMs) const {
        auto value=_to;
        if(_viewMoving) {
            const float t=std::min(1.f,float(nowMs-_viewStarted)/kTransitionMs);
            const float ease=t*t*(3.f-2.f*t);
            value.yaw=_from.yaw+(_to.yaw-_from.yaw)*ease;
            value.pitch=_from.pitch+(_to.pitch-_from.pitch)*ease;
            value.scale=_from.scale+(_to.scale-_from.scale)*ease;
            // Pull back gently during an orbit: an oblique long chassis can
            // project taller than either endpoint (especially TOP <-> FRONT).
            // Zero at both ends also preserves interrupted-transition continuity.
            value.scale*=1.f-.20f*std::sin(3.1415926f*t);
            value.centerY=_from.centerY+(_to.centerY-_from.centerY)*ease;
        }
        value.preset=_view;
        value.wheelPhase=_restWheelPhase;
        // Start only after arriving at SIDE. Leaving SIDE freezes the current
        // phase rather than snapping the spokes back to their authored pose.
        const auto elapsed=uint32_t(nowMs-_viewStarted);
        if(_view==GarageView::Side && elapsed>kTransitionMs)
            value.wheelPhase=float(std::fmod(double(_restWheelPhase)+
                                   double(elapsed-kTransitionMs)*.007,6.283185307));
        if(_carMoving) {
            const float t=std::min(1.f,float(nowMs-_carStarted)/kTransitionMs);
            const float tail=(1.f-t)*(1.f-t)*(1.f-t);
            value.carSlide=24.f*_carDirection*tail;
            value.carZoom=1.f-.10f*tail;
        }
        return value;
    }
    bool animating(uint32_t nowMs) const {
        return _view == GarageView::Side ||
               (_viewMoving && nowMs - _viewStarted < kTransitionMs) ||
               (_carMoving && nowMs - _carStarted < kTransitionMs);
    }
    void changeView(int direction,uint32_t nowMs) {
        if(!direction)return;
        _from=state(nowMs);
        _restWheelPhase=_from.wheelPhase;
        _from.yaw=std::remainder(_from.yaw,6.2831853f);
        const int next=(int(_view)+(direction>0 ? 1 : 3))%4;
        _view=static_cast<GarageView>(next);
        constexpr std::array<GarageViewState,4> poses{{
            {}, {-1.5707963f,.12f,140.f,252.f},
            {-3.1415926f,.48f,132.f,245.f}, {0.f,1.5707963f,106.f,230.f}
        }};
        _to=poses[next];
        _to.yaw=_from.yaw+std::remainder(_to.yaw-_from.yaw,6.2831853f);
        _viewStarted=nowMs;_viewMoving=true;
    }
    void selectCar(CarId car,uint32_t nowMs) {
        if(car==_car)return;
        _carDirection=(int(car)-int(_car)+int(kCarCount))%int(kCarCount)==1 ? 1 : -1;
        _car=car;_carStarted=nowMs;_carMoving=true;
    }
private:
    GarageViewState _from{},_to{};
    CarId _car=CarId::CycloneMagnum;
    GarageView _view=GarageView::Front;
    uint32_t _viewStarted=0,_carStarted=0;
    int _carDirection=1;
    float _restWheelPhase=0;
    bool _viewMoving=false,_carMoving=false;
};
static_assert(sizeof(GarageViewController)<=96,"garage motion fixed state budget");

} // namespace lets_and_go
