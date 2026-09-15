#pragma once
#include "rx78.h"
#include "sd_skirt_assembly.h"
namespace gundam_museum {
enum class ZakuStage : uint8_t { Blockout, Identity, Final };
struct ZakuAssembly {
    struct Range{size_t begin=0,end=0;};
    Range rightShieldMount,rightShield,leftSpikes,headHose,waistHose,headMount,headBody,headSocket,antenna,rifleGrip,rifle,drum,heatHawkMount,heatHawk;
    std::array<Range,2> armMounts{},wrists{},palms{},forearms{};
    SkirtAssembly skirts{};
};
void buildCharZaku(Mesh&,BuildOptions={},ZakuStage=ZakuStage::Final,ZakuAssembly* =nullptr);
}
