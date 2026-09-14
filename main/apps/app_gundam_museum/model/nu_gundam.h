#pragma once
#include "rx78.h"
namespace gundam_museum {
enum class NuStage : uint8_t { Blockout, Identity, Final };
// Optional construction boundaries for host assembly checks; no runtime storage.
struct NuAssembly {
    struct Range {size_t begin=0,end=0;};
    std::array<Range,6> funnels{};
};
void buildNuGundam(Mesh& mesh,BuildOptions options={},NuStage stage=NuStage::Final,NuAssembly* assembly=nullptr);
} // namespace gundam_museum
