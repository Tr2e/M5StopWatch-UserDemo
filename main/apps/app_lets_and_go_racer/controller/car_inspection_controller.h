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
    void release() { _touch=false; }
    void drag(const PreviewDrag& input) {
        if (!input.changed) return;
        if (input.gesture!=_gesture) {
            _gesture=input.gesture;_yaw=_pose.yaw;_pitch=_pose.pitch;_touch=true;
        }
        if (!_touch) return;
        _pose.yaw=std::remainder(_yaw+input.dx*.012f,6.2831853f);
        _pose.pitch=std::clamp(_pitch-input.dy*.008f,.12f,1.5707963f);
        _touch=input.active;
    }
    bool turn(int horizontal,int vertical) {
        if (_touch || (!horizontal && !vertical)) return false;
        _pose.yaw=std::remainder(_pose.yaw+horizontal*.34906585f,6.2831853f);
        // Menu vertical steps use screen coordinates (+down), just like touch dy.
        _pose.pitch=std::clamp(_pose.pitch-vertical*.17453293f,.12f,1.5707963f);
        return true;
    }
private:
    GarageViewState _pose{};
    uint32_t _gesture=0;
    float _yaw=0,_pitch=0;
    bool _touch=false;
};
} // namespace lets_and_go
