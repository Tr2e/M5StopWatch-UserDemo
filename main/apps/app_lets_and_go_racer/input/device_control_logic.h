#pragma once

#include "racer_input_logic.h"
#include "device_touch_layout.h"
#include "../controller/game_flow.h"
#include "../controller/home_layout.h"
#include "../controller/race_ui_layout.h"
#include "../../app_vector_canyon_fighter/input/external_input_logic.h"

namespace lets_and_go {

struct TouchTrace {
    bool ready=false;
    GameScreen screen=GameScreen::InputCheck;
    int startX=0,startY=0,endX=0,endY=0;
    TouchAction target=TouchAction::None;
    bool accepted=false;
};
struct DeviceControlFrame {
    RacerInput input{};
    int navigation = 0;
    int view = 0;
    bool advance = false;
    int resultAction = -1;
    TouchTrace touchTrace{};
};

// Caller serializes GPIO/touch publication and consumption. A screen change
// cancels queued commands and requires release before accepting another gesture.
class DeviceControlLogic {
public:
    void reset() { *this = {}; }
    void setScreen(GameScreen screen) {
        if (_screen == screen) return;
        _screen = screen;
        const bool exit = _frame.input.exitPressed;
        _frame = {};
        _frame.input.exitPressed = exit; // Global exit must survive a concurrent page change.
        _buttonsArmed = false;
        _touchArmed = false;
        _presented = false;
        _touchDown = false;
        _candidate = TouchAction::None;
    }
    void presentScreen(GameScreen screen) { if(screen==_screen)_presented=true; }
    void invalidateTouch() {
        _gestureCanceled=true;_touchArmed=false;_frame.input.steer=0;
    }
    void buttons(bool aDown, bool bDown, uint32_t now) {
        const auto a = _a.update(aDown, now);
        const auto b = _b.update(bDown, now);
        const bool chord = _exit.update(a.pressed, b.pressed, now);
        _frame.input.exitPressed |= chord;
        if (a.pressed && b.pressed) {
            _buttonsArmed = false;
            _frame.input.confirmPressed = _frame.input.cancelPressed = false;
            _frame.input.pausePressed = false;
            _frame.navigation = _frame.view = 0;
            _frame.advance = false;
            _frame.resultAction = -1;
        }
        _frame.input.brakeHeld = _buttonsArmed && a.pressed && !b.pressed;
        _frame.input.boostHeld = _buttonsArmed && b.pressed && !a.pressed;
        if (_buttonsArmed) {
            if (_screen == GameScreen::Paused) {
                _frame.input.pausePressed |= b.clicked;
            } else if (_screen != GameScreen::Racing) {
                if (a.clicked) _frame.navigation = 1;
                _frame.input.confirmPressed |= b.clicked;
                _frame.input.cancelPressed |= a.holdStarted;
            }
        }
        if (!a.pressed && !b.pressed && _presented) _buttonsArmed = true;
    }
    void touch(bool down, int x, int y) {
        if (!down) {
            if(_touchDown && _screen!=GameScreen::Racing) {
                const bool accepted=_presented && _touchArmed && !_gestureCanceled &&
                    _candidate!=TouchAction::None && menuTouchTarget(_screen,_lastX,_lastY)==_candidate;
                if(accepted)dispatchTouch(_candidate);
                _frame.touchTrace={true,_screen,_startX,_startY,_lastX,_lastY,_candidate,accepted};
            }
            _touchDown=false;_touchArmed=_presented;_candidate=TouchAction::None;
            _gestureCanceled=false;_frame.input.steer=0;
            return;
        }
        _lastX=x;_lastY=y;
        if(!_touchDown) {
            _startX=x;_startY=y;
            _candidate=menuTouchTarget(_screen,x,y);
            _gestureCanceled=!_touchArmed || !_presented;
        }
        if(_touchArmed && _presented) {
            if(_screen==GameScreen::Racing) {
                if(!_touchDown)_steeringGesture=y>=130;
                if(_steeringGesture)_frame.input.steer=std::clamp((x-234)/140.f,-1.f,1.f);
                else if(!_touchDown)_frame.input.pausePressed=true;
            } else if(_touchDown && !_gestureCanceled) {
                const auto target=menuTouchTarget(_screen,x,y);
                // Allow a small settling motion at contact, but never drag from
                // one action onto another and execute the second action.
                if(_candidate==TouchAction::None) {
                    if(std::abs(x-_startX)<=16 && std::abs(y-_startY)<=16)_candidate=target;
                    else _gestureCanceled=true;
                } else if(target!=_candidate)_gestureCanceled=true;
            }
        }
        _touchDown=true;
    }
    DeviceControlFrame consume(bool healthy) {
        auto result = _frame;
        result.input.valid = healthy;
        if (!healthy) result.input.steer = 0;
        if (result.input.exitPressed) {
            result.input.confirmPressed = result.input.cancelPressed = false;
            result.input.pausePressed = result.input.brakeHeld = result.input.boostHeld = false;
            result.navigation = result.view = 0;
            result.advance = false;
            result.resultAction = -1;
        }
        _frame.input.confirmPressed = _frame.input.cancelPressed = false;
        _frame.input.pausePressed = _frame.input.exitPressed = false;
        _frame.navigation = _frame.view = 0;
        _frame.advance = false;
        _frame.resultAction = -1;
        _frame.touchTrace.ready=false;
        return result;
    }
private:
    void dispatchTouch(TouchAction action) {
        switch(action) {
            case TouchAction::Confirm:_frame.input.confirmPressed=true;break;
            case TouchAction::Back:_frame.input.cancelPressed=true;break;
            case TouchAction::Previous:_frame.navigation=-1;break;
            case TouchAction::Next:_frame.navigation=1;break;
            case TouchAction::View:_frame.view=1;break;
            case TouchAction::Advance:_frame.advance=true;break;
            case TouchAction::Retry:case TouchAction::Garage:case TouchAction::Exit:
                _frame.resultAction=int(action)-int(TouchAction::Retry);break;
            case TouchAction::Resume:_frame.input.pausePressed=true;break;
            default:break;
        }
    }
    bool _presented=false,_gestureCanceled=false;
    int _startX=0,_startY=0,_lastX=0,_lastY=0;
    TouchAction _candidate=TouchAction::None;
    vector_canyon_fighter::DebouncedActiveLowButton _a, _b;
    LongChordDetector _exit;
    DeviceControlFrame _frame{};
    GameScreen _screen = GameScreen::InputCheck;
    bool _buttonsArmed = false, _touchArmed = false, _touchDown = false;
    bool _steeringGesture = false;
};
} // namespace lets_and_go
