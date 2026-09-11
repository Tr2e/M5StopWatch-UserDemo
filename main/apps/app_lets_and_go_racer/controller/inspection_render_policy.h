#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace lets_and_go {
// Raster and destination percentages are relative to the native vehicle region.
// Keep the costly raster compact while the displayed model eases back to its
// native footprint. Only the static endpoint requests the full native raster.
class InspectionRenderPolicy {
public:
    static constexpr uint32_t kSettleMs=100;
    static constexpr uint32_t kRecoveryMs=520;
    void reset() { *this={}; }
    int percent() const { return _percent; }
    int displayPercent() const { return _displayPercent; }
    bool interacting() const { return _held; }
    bool update(uint32_t nowMs,bool poseChanged,bool inputHeld) {
        const int beforePercent=_percent,beforeDisplay=_displayPercent;
        if(poseChanged) {_percent=65;_displayPercent=85;_lastActiveMs=nowMs;}
        if(_percent!=100) {
            if(inputHeld || (_held && !inputHeld))_lastActiveMs=nowMs;
            const uint32_t age=nowMs-_lastActiveMs;
            if(inputHeld || age<kSettleMs) {
                _percent=65;_displayPercent=85;
            } else if(age<kSettleMs+kRecoveryMs) {
                const float t=float(age-kSettleMs)/float(kRecoveryMs);
                const float ease=t*t*(3.f-2.f*t);
                _percent=65;
                _displayPercent=std::clamp(85+int(std::lround(15.f*ease)),85,99);
            } else {
                _percent=100;_displayPercent=100;
            }
        }
        _held=inputHeld;
        return _percent!=beforePercent || _displayPercent!=beforeDisplay;
    }
private:
    uint32_t _lastActiveMs=0;
    int _percent=100;
    int _displayPercent=100;
    bool _held=false;
};
} // namespace lets_and_go
