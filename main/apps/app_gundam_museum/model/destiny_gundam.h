#pragma once
#include "rx78.h"

namespace gundam_museum {
enum class DestinyStage : uint8_t { Blockout, Identity, Final };
struct DestinyAssembly {
    struct Range { size_t begin=0,end=0; };
    std::array<Range,2> palms{},wrists{},forearms{},fins{},wingMounts{},wings{};
    Range headMount,helmet,backpackMount,rifle,shieldMount,shield;
    Range swordMount,sword,cannonMount,cannon;
};
void buildDestinyGundam(Mesh&,BuildOptions={},DestinyStage=DestinyStage::Final,DestinyAssembly* =nullptr);
} // namespace gundam_museum
