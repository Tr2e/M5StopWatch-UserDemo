#pragma once

#include <cstddef>
#include <cstdint>

namespace soft3d {

enum class CompositeMode : uint8_t {Automatic,Native,ScaledSparse};
enum class CoreMode : uint8_t {Automatic,Single,DualBands};
enum class MaterialMode : uint8_t {Automatic,SolidFast,General};
enum class ScratchMode : uint8_t {Internal,Fallback};
enum class FrameRateGrade : uint8_t {Fps30,Fps24,Fps20,Fps15,Below15};

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
    MaterialMode materials=MaterialMode::Automatic;
    uint16_t internalScalePercent=100;
    // ESP32-S3 thresholds are workload gates, not model-name checks. They are
    // initialized from the measured crate/rigid/RX-78 calibration set.
    uint16_t dualCorePrimitiveThreshold=256;
    uint32_t dualCorePixelThreshold=18000;
    uint32_t internalReserveBytes=32u*1024u;
};

struct FrameWorkload {
    uint32_t uniqueVertices=0;
    uint32_t visiblePrimitives=0;
    uint32_t testedPixels=0;
    uint32_t writtenPixels=0;
    uint32_t depthRejectedPixels=0;
    uint32_t solidPrimitives=0;
    uint32_t generalPrimitives=0;
    uint32_t totalTriangles=0;
    uint32_t submittedTriangles=0;
    uint32_t culledPrimitives=0;
    uint32_t offscreenPrimitives=0;
    uint32_t totalQuads=0;
    uint32_t submittedQuads=0;
};

constexpr RenderProfile game30Profile(){return {};}
constexpr RenderProfile balanced24Profile(){
    RenderProfile profile{};profile.dualCorePrimitiveThreshold=192;
    profile.dualCorePixelThreshold=14000;return profile;
}
constexpr RenderProfile stress15Profile(){
    RenderProfile profile{};profile.dualCorePrimitiveThreshold=128;
    profile.dualCorePixelThreshold=10000;return profile;
}

inline CoreMode selectCoreMode(const RenderProfile& profile,const RenderCapabilities& caps,
                               const FrameWorkload& workload) {
    if(profile.cores!=CoreMode::Automatic)return profile.cores;
    if(!caps.dualCoreBands)return CoreMode::Single;
    return workload.visiblePrimitives>=profile.dualCorePrimitiveThreshold ||
           workload.testedPixels>=profile.dualCorePixelThreshold
        ? CoreMode::DualBands:CoreMode::Single;
}

inline MaterialMode selectMaterialMode(const RenderProfile& profile,
                                       const RenderCapabilities& caps,
                                       const FrameWorkload& workload) {
    if(profile.materials==MaterialMode::SolidFast)
        return caps.solidFastPath && workload.generalPrimitives==0
            ? MaterialMode::SolidFast:MaterialMode::General;
    if(profile.materials==MaterialMode::General)return MaterialMode::General;
    return caps.solidFastPath && workload.generalPrimitives==0
        ? MaterialMode::SolidFast:MaterialMode::General;
}

inline ScratchMode selectScratchMode(const RenderProfile& profile,std::size_t availableInternalBytes,
                                     std::size_t requestedBytes) {
    return availableInternalBytes>=requestedBytes+profile.internalReserveBytes
        ? ScratchMode::Internal:ScratchMode::Fallback;
}

inline FrameRateGrade classifyFrameRate(uint32_t p95FrameUs) {
    if(p95FrameUs<=33333)return FrameRateGrade::Fps30;
    if(p95FrameUs<=41667)return FrameRateGrade::Fps24;
    if(p95FrameUs<=50000)return FrameRateGrade::Fps20;
    if(p95FrameUs<=66667)return FrameRateGrade::Fps15;
    return FrameRateGrade::Below15;
}

inline CompositeMode selectCompositeMode(const RenderProfile& profile,int internalWidth,
                                         int internalHeight,int outputWidth,int outputHeight) {
    if(profile.composite!=CompositeMode::Automatic)return profile.composite;
    return internalWidth==outputWidth && internalHeight==outputHeight
        ? CompositeMode::Native:CompositeMode::ScaledSparse;
}

} // namespace soft3d
