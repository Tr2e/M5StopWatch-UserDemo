#pragma once

#include <M5GFX.h>
#include <cstdint>
#include "typhoon_data.h"

namespace typhoon {

class Engine {
public:
    Engine() = default;
    ~Engine();

    void open();
    void close();
    void update();
    void applySnapshot(const TyphoonSnapshot& snapshot);
    LGFX_Sprite* legacySprite();
    // Scale for pushing virtual canvas → 466 round panel (1.0 = native).
    float presentScale() const;
};

}  // namespace typhoon
