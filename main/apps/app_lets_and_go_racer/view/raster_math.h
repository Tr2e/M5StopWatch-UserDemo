#pragma once

namespace lets_and_go {
// Single-precision values outside this interval are already integral. Keep
// conversion bounded, including for off-screen projections, NaN and infinity.
// Xtensa otherwise calls libm and spills live FPU registers at each pixel edge.
inline float rasterFloor(float value) {
    if (!(value > -8388608.f && value < 8388608.f)) return value;
    const int integer=static_cast<int>(value);
    return static_cast<float>(integer-(value<static_cast<float>(integer)));
}
inline float rasterCeil(float value) {
    if (!(value > -8388608.f && value < 8388608.f)) return value;
    const int integer=static_cast<int>(value);
    return static_cast<float>(integer+(value>static_cast<float>(integer)));
}
} // namespace lets_and_go
