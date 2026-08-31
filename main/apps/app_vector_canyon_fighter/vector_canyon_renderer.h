#pragma once

#include <cstdint>

namespace vector_canyon_fighter {

class Renderer {
public:
    void open(int width, int height);
    void close();
    void renderStaticScene();

private:
    int _width = 0;
    int _height = 0;
};

}  // namespace vector_canyon_fighter
