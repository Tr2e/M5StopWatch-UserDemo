#pragma once
#include "../model/car_catalog.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace lets_and_go {

enum class GarageView : uint8_t { Front, Side, Rear, Top, Count };
struct PreviewDrag {
    uint32_t gesture=0;
    int dx=0,dy=0;
    bool active=false,changed=false;
};
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
        _wheelPresentedMs=nowMs;
        _dragging=false;_dragId=0;
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
        if (_dragging) {
            value=_from;
            value.preset=_view;
            // Reserve room for every azimuth, including long diagonal bodies.
            value.scale=std::min(value.scale,90.f);value.centerY=230.f;
            value.carSlide=0;value.carZoom=1;
        }
        value.wheelPhase=_restWheelPhase;
        // Start only after arriving at SIDE. Leaving SIDE freezes the current
        // phase rather than snapping the spokes back to their authored pose.
        const auto elapsed=uint32_t(nowMs-_viewStarted);
        if(!_dragging && _view==GarageView::Side && elapsed>kTransitionMs)
            value.wheelPhase=float(std::fmod(double(_restWheelPhase)+
                std::min(.25, double(std::min(nowMs-_wheelPresentedMs,
                                             elapsed-kTransitionMs))*.0015),6.283185307));
        if(_carMoving && !_dragging) {
            const float t=std::min(1.f,float(nowMs-_carStarted)/kTransitionMs);
            const float tail=(1.f-t)*(1.f-t)*(1.f-t);
            value.carSlide=24.f*_carDirection*tail;
            value.carZoom=1.f-.10f*tail;
        }
        return value;
    }
    bool dragging() const { return _dragging; }
    int renderPercent(uint32_t nowMs) const {
        if(_dragging)return 65;
        // Keep wheel-only SIDE animation native. Restore exact native sampling
        // on the final transition frame, including after a long rendering stall.
        const bool moving=(_viewMoving && nowMs-_viewStarted<kTransitionMs) ||
                          (_carMoving && nowMs-_carStarted<kTransitionMs);
        return moving ? 85 : 100;
    }
    void endDrag(uint32_t nowMs) {
        if (!_dragging) return;
        _from=state(nowMs);_dragging=false;
        _to.yaw=_from.yaw+std::remainder(_to.yaw-_from.yaw,6.2831853f);
        _viewStarted=_wheelPresentedMs=nowMs;_viewMoving=true;_carMoving=false;
    }
    void drag(const PreviewDrag& drag,uint32_t nowMs) {
        if (!drag.changed) return;
        if (drag.gesture!=_dragId) {
            _from=state(nowMs);_restWheelPhase=_from.wheelPhase;
            _lastDragDx=_lastDragDy=0;_dragId=drag.gesture;
            _dragging=true;_viewMoving=false;
        }
        if (!_dragging) return;
        // Match the projected near surface: positive screen dy increases pitch.
        // Consume deltas so reversing at either clamp responds immediately.
        _from.yaw=std::remainder(_from.yaw+float(drag.dx-_lastDragDx)*.012f,6.2831853f);
        _from.pitch=std::clamp(_from.pitch+float(drag.dy-_lastDragDy)*.008f,.12f,1.5707963f);
        _lastDragDx=drag.dx;_lastDragDy=drag.dy;
        if (!drag.active) endDrag(nowMs);
    }
    // Commit once per displayed preview, not once per input/update tick.
    // Positive phase rolls toward model +z. At 7 rad/s slow frames alias
    // repeated spokes into reverse motion; keep each step below half of the
    // smallest spoke spacing (six spokes: pi/6), even after a long stall.
    void presented(uint32_t nowMs) {
        _restWheelPhase=state(nowMs).wheelPhase;
        _wheelPresentedMs=nowMs;
    }
    bool animating(uint32_t nowMs) const {
        return _dragging || _view == GarageView::Side ||
               (_viewMoving && nowMs - _viewStarted < kTransitionMs) ||
               (_carMoving && nowMs - _carStarted < kTransitionMs);
    }
    void changeView(int direction,uint32_t nowMs) {
        if(!direction)return;
        _from=state(nowMs);
        _dragging=false;
        _restWheelPhase=_from.wheelPhase;
        _wheelPresentedMs=nowMs;
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
        endDrag(nowMs);
        _carDirection=(int(car)-int(_car)+int(kCarCount))%int(kCarCount)==1 ? 1 : -1;
        _car=car;_carStarted=nowMs;_carMoving=true;
    }
private:
    GarageViewState _from{},_to{};
    CarId _car=CarId::CycloneMagnum;
    GarageView _view=GarageView::Front;
    uint32_t _viewStarted=0,_carStarted=0;
    uint32_t _wheelPresentedMs=0;
    int _carDirection=1;
    float _restWheelPhase=0;
    bool _viewMoving=false,_carMoving=false;
    uint32_t _dragId=0;
    int _lastDragDx=0,_lastDragDy=0;
    bool _dragging=false;
};
static_assert(sizeof(GarageViewController)<=112,"garage motion fixed state budget");

} // namespace lets_and_go
