#pragma once

#include <cstdint>

namespace vector_canyon_fighter {

enum class TerrainRenderDetail : uint8_t {
    Low,
    Medium,
    High,
};

inline constexpr const char* terrainRenderDetailLabel(TerrainRenderDetail detail)
{
    switch (detail) {
        case TerrainRenderDetail::Low: return "LOW";
        case TerrainRenderDetail::Medium: return "MED";
        default: return "HIGH";
    }
}

// Five-second performance windows drive a deliberately slow hysteresis loop.
// Normal single-frame spikes never alter the image. Only sustained missed
// budgets remove optional terrain detail, and recovery takes longer than
// degradation so the wireframe cannot flicker between density levels.
class RenderBudgetController {
public:
    void reset()
    {
        _detail = TerrainRenderDetail::High;
        _overBudgetWindows = 0;
        _healthyWindows = 0;
    }

    void observe(uint32_t fpsTenths, uint32_t averageRenderMs,
                 uint16_t simulationClampCount)
    {
        // H9 hardware baseline with the full H8 far field is 20.7--21.7 FPS
        // and 43--45 ms average render time. That accepted load must remain
        // High: dropping the 68 face diagonals did not measurably improve it.
        constexpr uint32_t kHighToMediumFpsTenths = 190;
        constexpr uint32_t kHighToMediumRenderMs = 50;
        constexpr uint32_t kMediumToLowFpsTenths = 160;
        constexpr uint32_t kMediumToLowRenderMs = 60;
        constexpr uint32_t kRecoveryFpsTenths = 205;
        constexpr uint32_t kRecoveryRenderMs = 47;
        constexpr uint8_t kDegradeWindows = 2;
        constexpr uint8_t kRecoveryWindows = 4;

        const bool severe = fpsTenths < kMediumToLowFpsTenths ||
                            averageRenderMs > kMediumToLowRenderMs ||
                            simulationClampCount > 3;
        const bool overloaded = severe ||
                                fpsTenths < kHighToMediumFpsTenths ||
                                averageRenderMs > kHighToMediumRenderMs ||
                                simulationClampCount > 1;
        const bool healthy = fpsTenths >= kRecoveryFpsTenths &&
                             averageRenderMs <= kRecoveryRenderMs &&
                             simulationClampCount == 0;

        if (_detail == TerrainRenderDetail::High) {
            _healthyWindows = 0;
            _overBudgetWindows = overloaded
                ? static_cast<uint8_t>(_overBudgetWindows + 1)
                : 0;
            if (_overBudgetWindows >= kDegradeWindows) {
                _detail = TerrainRenderDetail::Medium;
                _overBudgetWindows = 0;
            }
            return;
        }

        if (_detail == TerrainRenderDetail::Medium) {
            _overBudgetWindows = severe
                ? static_cast<uint8_t>(_overBudgetWindows + 1)
                : 0;
            _healthyWindows = healthy
                ? static_cast<uint8_t>(_healthyWindows + 1)
                : 0;
            if (_overBudgetWindows >= kDegradeWindows) {
                _detail = TerrainRenderDetail::Low;
                _overBudgetWindows = 0;
                _healthyWindows = 0;
            } else if (_healthyWindows >= kRecoveryWindows) {
                _detail = TerrainRenderDetail::High;
                _overBudgetWindows = 0;
                _healthyWindows = 0;
            }
            return;
        }

        _overBudgetWindows = 0;
        _healthyWindows = healthy
            ? static_cast<uint8_t>(_healthyWindows + 1)
            : 0;
        if (_healthyWindows >= kRecoveryWindows) {
            _detail = TerrainRenderDetail::Medium;
            _healthyWindows = 0;
        }
    }

    TerrainRenderDetail detail() const { return _detail; }

private:
    TerrainRenderDetail _detail = TerrainRenderDetail::High;
    uint8_t _overBudgetWindows = 0;
    uint8_t _healthyWindows = 0;
};

static_assert(sizeof(RenderBudgetController) <= 3,
              "Render budget hysteresis exceeded its fixed state budget");

}  // namespace vector_canyon_fighter
