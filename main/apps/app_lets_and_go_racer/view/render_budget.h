#pragma once

#include <cstdint>

namespace lets_and_go {

enum class PencilDetail : uint8_t {
    Low,
    Medium,
    High,
};

struct FrameBudgetStats {
    uint32_t frameCount = 0;
    uint32_t lastRenderMs = 0;
    uint32_t peakRenderMs = 0;
    uint32_t detailTransitions = 0;
};

inline const char* pencilDetailLabel(PencilDetail detail)
{
    switch (detail) {
        case PencilDetail::Low: return "low";
        case PencilDetail::Medium: return "medium";
        case PencilDetail::High: return "high";
    }
    return "unknown";
}

class FrameBudgetController {
public:
    void reset()
    {
        _detail = PencilDetail::High;
        _overloadedFrames = 0;
        _healthyFrames = 0;
        _stats = {};
    }

    void observe(uint32_t renderMs, bool simulationClamped)
    {
        ++_stats.frameCount;
        _stats.lastRenderMs = renderMs;
        if (renderMs > _stats.peakRenderMs) _stats.peakRenderMs = renderMs;
        const PencilDetail before = _detail;
        const bool severe = renderMs > 54u || simulationClamped;
        const bool overloaded = severe || renderMs > 39u;
        const bool healthy = renderMs <= 29u && !simulationClamped;
        if (overloaded) {
            _overloadedFrames = _overloadedFrames < 30u
                ? static_cast<uint8_t>(_overloadedFrames + 1u) : _overloadedFrames;
            _healthyFrames = 0;
        } else if (healthy) {
            _healthyFrames = _healthyFrames < 180u
                ? static_cast<uint8_t>(_healthyFrames + 1u) : _healthyFrames;
            _overloadedFrames = 0;
        } else {
            _overloadedFrames = 0;
            _healthyFrames = 0;
        }

        if (_detail == PencilDetail::High && _overloadedFrames >= 10u) {
            _detail = PencilDetail::Medium;
            _overloadedFrames = 0;
        } else if (_detail == PencilDetail::Medium && _overloadedFrames >= 10u) {
            _detail = PencilDetail::Low;
            _overloadedFrames = 0;
        } else if (_detail == PencilDetail::Low && _healthyFrames >= 120u) {
            _detail = PencilDetail::Medium;
            _healthyFrames = 0;
        } else if (_detail == PencilDetail::Medium && _healthyFrames >= 150u) {
            _detail = PencilDetail::High;
            _healthyFrames = 0;
        }
        if (_detail != before) ++_stats.detailTransitions;
    }

    PencilDetail detail() const { return _detail; }
    const FrameBudgetStats& stats() const { return _stats; }

private:
    PencilDetail _detail = PencilDetail::High;
    uint8_t _overloadedFrames = 0;
    uint8_t _healthyFrames = 0;
    FrameBudgetStats _stats;
};

static_assert(sizeof(FrameBudgetController) <= 24u,
              "frame budget controller exceeded fixed state budget");

}  // namespace lets_and_go
