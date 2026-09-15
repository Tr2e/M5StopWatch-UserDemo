#pragma once
#include "rx78.h"
namespace gundam_museum {
enum class SazabiStage : uint8_t { Blockout, Identity, Final };
struct SazabiAssembly {
    struct Range{size_t begin=0,end=0;};
    std::array<Range,6> funnels{};std::array<Range,2> shoulders{},palms{},containers{};Range shield,rifle,shieldMount,crown;
};
void buildSazabi(Mesh&,BuildOptions={},SazabiStage=SazabiStage::Final,SazabiAssembly* =nullptr);
}
