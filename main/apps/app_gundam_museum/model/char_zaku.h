#pragma once
#include "rx78.h"
namespace gundam_museum {
enum class ZakuStage : uint8_t { Blockout, Identity, Final };
struct ZakuAssembly {
    struct Range{size_t begin=0,end=0;};
    Range rightShield,leftSpikes,headHose,waistHose,rifle,drum,heatHawk;
    std::array<Range,2> palms{};
};
void buildCharZaku(Mesh&,BuildOptions={},ZakuStage=ZakuStage::Final,ZakuAssembly* =nullptr);
}
