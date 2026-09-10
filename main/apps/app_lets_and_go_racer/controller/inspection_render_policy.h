#pragma once
#include <cstdint>

namespace lets_and_go {
// Raster and destination percentages are relative to the native vehicle region.
// A brief intermediate frame restores size and detail together after release.
class InspectionRenderPolicy {
public:
    void reset() { *this={}; }
    int percent() const { return _percent; }
    int displayPercent() const { return _percent==65 ? 85 : _percent==82 ? 93 : 100; }
    bool interacting() const { return _percent==65; }
    bool update(uint32_t nowMs,bool poseChanged,bool inputHeld) {
        const int before=_percent;
        if(poseChanged) { _percent=65;_lastActiveMs=nowMs; }
        if(_percent!=100) {
            if(inputHeld || (_held && !inputHeld))_lastActiveMs=nowMs;
            const uint32_t age=nowMs-_lastActiveMs;
            _percent=inputHeld || age<150u ? 65 : age<300u ? 82 : 100;
        }
        _held=inputHeld;
        return _percent!=before;
    }
private:
    uint32_t _lastActiveMs=0;
    int _percent=100;
    bool _held=false;
};
} // namespace lets_and_go
