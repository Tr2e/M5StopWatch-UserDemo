#pragma once
#include "car_catalog.h"

namespace lets_and_go {

enum class CarSurfaceDetail : uint8_t { Low, Medium, High };

// Small solid panels with selected pencil edges. This is an authored model,
// not a triangulated seven-section generic body or a bitmap of a product photo.
struct CarPanel {
    std::array<CarPoint, 4> point{};
    uint16_t color = 0;
    uint8_t edges = 0;
    uint8_t wheel = 0; // 1..4: only hub spokes rotate; tire envelopes stay round.
    uint16_t parent = 0xffffu; // Paint attached glass/decals with their underlying face.
};

struct CarDisplayMesh {
    static constexpr std::size_t kMaximumPanels = 576u;
    std::array<CarPanel, kMaximumPanels> panels{};
    std::size_t count = 0;
    bool overflowed = false;
};

void buildCarDisplayMesh(CarId car, CarDisplayMesh& mesh,
                        CarSurfaceDetail detail = CarSurfaceDetail::High);
CarPoint animateCarPanelPoint(CarPoint point, uint8_t wheel, float cosine, float sine);

} // namespace lets_and_go
