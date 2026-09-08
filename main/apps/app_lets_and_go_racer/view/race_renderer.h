#pragma once

#include "../controller/game_flow.h"
#include "../controller/race_controller.h"
#include "../controller/results_selection.h"
#include "../model/car_catalog.h"
#include "render_budget.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace lets_and_go {

struct CompactRaceMesh {
    static constexpr std::size_t kMaximumLines = 64u;
    std::array<WireLine, kMaximumLines> lines{};
    std::size_t lineCount = 0;
};

class RaceRenderer {
public:
    void open(int width, int height);
    void close();
    void render(const GameFlow& flow, const RaceController& race,
                const ResultsSelection& results,
                uint32_t screenElapsedMs, bool pausedForInputLoss,
                PencilDetail detail);

private:
    std::array<CompactRaceMesh, kCarCount> _meshes{};
    std::array<int16_t, 32u> _mapX{};
    std::array<int16_t, 32u> _mapY{};
    int _width = 0;
    int _height = 0;
};

static_assert(sizeof(RaceRenderer) <= 8000u,
              "race renderer cache exceeded its reviewed resident budget");

}  // namespace lets_and_go
