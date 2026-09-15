#pragma once
#include <array>
#include <cstddef>

namespace gundam_museum {
struct SkirtAssembly {
    struct Range { size_t begin=0,end=0; };
    // Index 0 is the suit's right side (negative model X), index 1 its left.
    std::array<Range,2> front{},side{},rear{};
};
} // namespace gundam_museum
