#pragma once
#include "garage_view_controller.h"

namespace lets_and_go {

// Dedicated presentation state: inspecting never changes the chosen car or
// the garage preset. Touch owns the camera until release; joystick steps are
// buffered by the existing background menu input, including during slow frames.
class CarInspectionController {
public:
    void reset() { _pose={};_touch=false; }
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
} // namespace lets_and_go
