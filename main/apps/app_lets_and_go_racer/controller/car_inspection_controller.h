#pragma once
#include "garage_view_controller.h"

#include <array>

namespace lets_and_go {

// Dedicated camera state. Browsing may move the garage cursor, but this class
// never commits the chosen car or changes the garage preset. Touch owns the
// camera until release; joystick steps remain buffered during slow frames.
class CarInspectionController {
public:
    void reset() { _pose={};_touch=false; }
    void setState(const GarageViewState& pose) {
        _pose=pose;_touch=false;
    }
    GarageViewState state() const { return _pose; }
    bool touchActive() const { return _touch; }
    void release() { _touch=false; }
    bool drag(const PreviewDrag& input) {
        if (!input.changed) return false;
        if (input.gesture!=_gesture) {
            _gesture=input.gesture;_lastDx=_lastDy=0;_touch=true;
        }
        if (!_touch) return false;
        // Consume displacement since the last sample. Clamping an angle based
        // on the gesture origin accumulates overshoot and makes reversal stick.
        const bool changed=rotate((input.dx-_lastDx)*.012f,(input.dy-_lastDy)*.008f);
        _lastDx=input.dx;_lastDy=input.dy;
        _touch=input.active;
        return changed;
    }
    bool turn(int horizontal,int vertical) {
        if (_touch || (!horizontal && !vertical)) return false;
        return rotate(horizontal*.34906585f,vertical*.17453293f);
    }
private:
    bool rotate(float horizontal,float vertical) {
        const float yaw=std::remainder(_pose.yaw+horizontal,6.2831853f);
        // The renderer pitches after yaw: positive pitch moves the near-facing
        // surface DOWN on screen. Finger dy and menu steps both use +down.
        const float pitch=std::clamp(_pose.pitch+vertical,.12f,1.5707963f);
        const bool changed=yaw!=_pose.yaw || pitch!=_pose.pitch;
        _pose.yaw=yaw;_pose.pitch=pitch;
        return changed;
    }
    GarageViewState _pose{};
    uint32_t _gesture=0;
    int _lastDx=0,_lastDy=0;
    bool _touch=false;
};

// Time-based showroom tour. Rendering may be slower than the nominal frame
// interval, so the camera is sampled from monotonic time and never queues old
// poses. The unwrapped keyframe yaw completes one full orbit without a visual
// jump when the next car is selected.
class InspectionAutoController {
public:
    static constexpr uint32_t kEntryMs=1200;
    static constexpr uint32_t kTourMs=30000;

    void reset() { *this={}; }
    bool enabled() const { return _enabled; }
    void start(uint32_t nowMs,const GarageViewState& origin) {
        _enabled=true;_startedMs=nowMs;_origin=origin;
    }
    void stop() { _enabled=false; }
    GarageViewState state(uint32_t nowMs) const {
        if(!_enabled)return _origin;
        const uint32_t age=nowMs-_startedMs;
        const auto front=keyframes()[0].view;
        if(age<kEntryMs) {
            const float t=float(age)/float(kEntryMs);
            const float ease=t*t*(3.f-2.f*t);
            auto target=front;
            target.yaw=_origin.yaw+std::remainder(front.yaw-_origin.yaw,6.2831853f);
            return interpolate(_origin,target,ease);
        }
        const uint32_t tourAge=(age-kEntryMs)%kTourMs;
        const auto& frames=keyframes();
        for(std::size_t i=1;i<frames.size();++i) {
            if(tourAge>frames[i].atMs)continue;
            const auto& from=frames[i-1];const auto& to=frames[i];
            const float span=float(to.atMs-from.atMs);
            const float t=span>0.f ? float(tourAge-from.atMs)/span : 1.f;
            const float ease=t*t*(3.f-2.f*t);
            auto result=interpolate(from.view,to.view,ease);
            result.yaw=std::remainder(result.yaw,6.2831853f);
            return result;
        }
        return front;
    }
private:
    struct Keyframe { uint32_t atMs;GarageViewState view; };
    static GarageViewState pose(float yaw,float pitch) {
        GarageViewState result{};result.yaw=yaw;result.pitch=pitch;return result;
    }
    static const std::array<Keyframe,8>& keyframes() {
        static const std::array<Keyframe,8> value{{
            {0,pose(-.65f,.5713375f)},
            {3000,pose(-.65f,.5713375f)},
            {8000,pose(-1.5707963f,.34f)},
            {10500,pose(-1.5707963f,.34f)},
            {15500,pose(-3.1415926f,.50f)},
            {18000,pose(-3.1415926f,.50f)},
            {25000,pose(-4.7123890f,.95f)},
            {30000,pose(-6.9331853f,.5713375f)},
        }};
        return value;
    }
    static GarageViewState interpolate(const GarageViewState& from,
                                        const GarageViewState& to,float t) {
        auto result=from;
        result.yaw=from.yaw+(to.yaw-from.yaw)*t;
        result.pitch=from.pitch+(to.pitch-from.pitch)*t;
        result.scale=from.scale+(to.scale-from.scale)*t;
        result.centerY=from.centerY+(to.centerY-from.centerY)*t;
        result.carSlide=0;result.carZoom=1;result.wheelPhase=0;
        return result;
    }
    GarageViewState _origin{};
    uint32_t _startedMs=0;
    bool _enabled=false;
};
static_assert(sizeof(InspectionAutoController)<=48,"inspection auto fixed state budget");
} // namespace lets_and_go
