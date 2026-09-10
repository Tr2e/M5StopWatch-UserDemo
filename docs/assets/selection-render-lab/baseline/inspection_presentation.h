#pragma once
#include "game_flow.h"

namespace lets_and_go {
struct InspectionRefreshRegion {
    int x,y,width,height;
    bool contains(int px,int py) const {
        return px>=x && px<x+width && py>=y && py<y+height;
    }
};

// Includes the car raster and the complete native shadow through all poses.
inline InspectionRefreshRegion inspectionRefreshRegion(int width) {return {40,70,width-80,322};}

class InspectionPresentation {
public:
    void reset() { *this={}; }
    bool partial(GameScreen screen,CarId car) const {
        return _presented && screen==GameScreen::CarInspect &&
            _screen==GameScreen::CarInspect && car==_car;
    }
    void presented(GameScreen screen,CarId car) {
        _presented=true;_screen=screen;_car=car;
    }
private:
    GameScreen _screen=GameScreen::InputCheck;
    CarId _car=CarId::Count;
    bool _presented=false;
};
} // namespace lets_and_go
