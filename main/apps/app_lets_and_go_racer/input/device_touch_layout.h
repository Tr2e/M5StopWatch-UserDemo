#pragma once
#include "../controller/game_flow.h"
#include "../controller/race_ui_layout.h"

namespace lets_and_go {
enum class TouchAction { None,Confirm,Back,Previous,Next,View,Advance,Retry,Garage,Exit,Resume,Inspect };
inline const char* touchActionLabel(TouchAction action) {
    switch(action) {
        case TouchAction::Confirm:return "confirm";
        case TouchAction::Back:return "back";
        case TouchAction::Previous:return "previous";
        case TouchAction::Next:return "next";
        case TouchAction::View:return "view";
        case TouchAction::Advance:return "advance";
        case TouchAction::Retry:return "retry";
        case TouchAction::Garage:return "garage";
        case TouchAction::Exit:return "exit";
        case TouchAction::Resume:return "resume";
        case TouchAction::Inspect:return "inspect";
        default:return "none";
    }
}
inline TouchAction menuTouchTargetPass(GameScreen screen,int x,int y,bool expanded) {
    using namespace home_layout;
    const auto hit=[&](Rect rect,int padding=4) {
        return expanded ? rect.near(x,y,padding) : rect.contains(x,y);
    };
    if(screen==GameScreen::InputCheck || screen==GameScreen::InputCalibration)
        return hit(deviceAction) ? TouchAction::Confirm : TouchAction::None;
    if(screen==GameScreen::Results) {
        for(int i=0;i<3;++i)if(hit(race_ui_layout::resultRow(i),5))
            return static_cast<TouchAction>(int(TouchAction::Retry)+i);
        return TouchAction::None;
    }
    if(screen==GameScreen::Paused)
        return hit(Rect{100,185,266,81}) ? TouchAction::Resume : TouchAction::None;
    if(screen==GameScreen::CarInspect) {
        if(hit(setupBack))return TouchAction::Back;
        if(hit(inspectReset))return TouchAction::Confirm;
        return TouchAction::None;
    }
    if(screen!=GameScreen::CarSelect && screen!=GameScreen::RivalSelect && screen!=GameScreen::TrackSelect)
        return TouchAction::None;
    if(hit(setupBack))return TouchAction::Back;
    if(screen==GameScreen::CarSelect) {
        if(hit(inspectAction))return TouchAction::Inspect;
        if(hit(viewAction))return TouchAction::View;
        if(hit(carSelect))return TouchAction::Confirm;
    } else {
        if(hit(setupNext))return screen==GameScreen::RivalSelect ? TouchAction::Advance : TouchAction::Confirm;
        // Smaller allowance keeps the status row and NEXT separate.
        if(screen==GameScreen::RivalSelect && hit(rivalToggle,3))return TouchAction::Confirm;
    }
    const int arrowY=screen==GameScreen::CarSelect ? 366 : screen==GameScreen::RivalSelect ? 331 : 358;
    if(hit(previous(arrowY)))return TouchAction::Previous;
    if(hit(next(arrowY)))return TouchAction::Next;
    return TouchAction::None;
}
inline bool touchOnDisplay(int x,int y) {
    if(x<0 || x>=468 || y<0 || y>=466)return false;
    const int dx=x-234,dy=y-233;
    return dx*dx+dy*dy<=233*233;
}
inline TouchAction menuTouchTarget(GameScreen screen,int x,int y) {
    if(!touchOnDisplay(x,y))return TouchAction::None;
    // A visible control always wins over a neighbour's expanded touch allowance.
    const auto exact=menuTouchTargetPass(screen,x,y,false);
    return exact!=TouchAction::None ? exact : menuTouchTargetPass(screen,x,y,true);
}
} // namespace lets_and_go
