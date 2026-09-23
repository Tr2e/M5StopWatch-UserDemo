#pragma once

#include <cstdint>

namespace soft3d {

enum class CompositeMode : uint8_t {Automatic,Native,ScaledSparse};
enum class CoreMode : uint8_t {Automatic,Single,DualBands};

struct RenderCapabilities {
    bool solidFastPath=true;
    bool generalMaterials=true;
    bool nearClipping=true;
    bool indexedCommands=true;
    bool sparseDepth=true;
    bool internalScratch=true;
    bool dualCoreBands=true;
    bool nativeFrameBuffer=true;
    bool asynchronousPresent=true;
};

struct RenderProfile {
    CoreMode cores=CoreMode::Automatic;
    CompositeMode composite=CompositeMode::Automatic;
    uint16_t internalScalePercent=100;
    uint16_t dualCorePrimitiveThreshold=0;
    uint32_t dualCorePixelThreshold=0;
    uint32_t internalReserveBytes=32u*1024u;
};

struct FrameWorkload {
    uint32_t uniqueVertices=0;
    uint32_t visiblePrimitives=0;
    uint32_t testedPixels=0;
    uint32_t writtenPixels=0;
    uint32_t solidPrimitives=0;
    uint32_t generalPrimitives=0;
};

inline CoreMode selectCoreMode(const RenderProfile& profile,const RenderCapabilities& caps,
                               const FrameWorkload& workload) {
    if(profile.cores!=CoreMode::Automatic)return profile.cores;
    if(!caps.dualCoreBands)return CoreMode::Single;
    return workload.visiblePrimitives>=profile.dualCorePrimitiveThreshold &&
           workload.testedPixels>=profile.dualCorePixelThreshold
        ? CoreMode::DualBands:CoreMode::Single;
}

inline CompositeMode selectCompositeMode(const RenderProfile& profile,int internalWidth,
                                         int internalHeight,int outputWidth,int outputHeight) {
    if(profile.composite!=CompositeMode::Automatic)return profile.composite;
    return internalWidth==outputWidth && internalHeight==outputHeight
        ? CompositeMode::Native:CompositeMode::ScaledSparse;
}

} // namespace soft3d
