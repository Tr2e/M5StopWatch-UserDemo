#pragma once
#include "rx78.h"
namespace gundam_museum {
enum class StrikeStage : uint8_t { Blockout, Identity, Final };
struct StrikeAssembly {
    struct Range {size_t begin=0,end=0;};
    std::array<Range,2> vents{}; // Mirrored chest openings and blades.
 std::array<Range,2> palms{}; // Rigid fist shells, excluding wrist pins.
 std::array<Range,5> aile{}; // center, left/right main wings, left/right pods
};
void buildStrikeGundam(Mesh&,BuildOptions={},StrikeStage=StrikeStage::Final,StrikeAssembly* =nullptr);
}
