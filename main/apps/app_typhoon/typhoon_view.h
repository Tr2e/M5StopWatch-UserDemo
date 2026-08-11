#pragma once

#include <cstdint>
#include <memory>
#include "typhoon_data.h"

namespace typhoon {

class Engine;

class View {
public:
    View();
    ~View();

    void open();
    void close();
    void update();
    void setSnapshot(const TyphoonSnapshot& snapshot);

private:
    std::unique_ptr<Engine> _engine;
};

}  // namespace typhoon
