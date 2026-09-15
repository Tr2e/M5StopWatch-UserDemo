#pragma once
#include "rx78.h"
#include "sd_eye_socket.h"
#include "sd_skirt_assembly.h"
namespace gundam_museum {
enum class StrikeStage : uint8_t { Blockout, Identity, Final };
struct StrikeAssembly {
    std::array<EyeSocketAssembly,2> eyes{};
    struct Range {size_t begin=0,end=0;};
    std::array<Range,2> vents{}; // Mirrored chest openings and blades.
 std::array<Range,2> palms{}; // Rigid fist shells, excluding wrist pins.
 std::array<Range,5> aile{}; // center, left/right main wings, left/right pods
 SkirtAssembly skirts{};
};
void buildStrikeGundam(Mesh&,BuildOptions={},StrikeStage=StrikeStage::Final,StrikeAssembly* =nullptr);
}
