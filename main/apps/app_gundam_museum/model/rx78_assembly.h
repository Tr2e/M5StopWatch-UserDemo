#pragma once
#include "sd_eye_socket.h"

namespace gundam_museum {
// Optional host diagnostics, not part of the runtime mesh/cache.
struct Rx78Assembly {
    std::array<EyeSocketAssembly,2> eyes{};
};
}
