#pragma once

#include "fruit_snake_engine.h"

#include <cstdint>

namespace fruit_snake {

class Renderer {
public:
    void open(int width, int height, uint32_t nowMs);
    void render(const Engine& engine, uint32_t nowMs);
    void close();

private:
    int _width = 0;
    int _height = 0;
    uint32_t _openedMs = 0;
    uint32_t _lastFrameMs = 0;
};

}  // namespace fruit_snake
