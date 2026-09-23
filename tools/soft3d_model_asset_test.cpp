#include "../main/apps/common/soft3d/asset/builtin_samples.h"
#include "../main/apps/common/soft3d/raster/surface_raster.h"
#include "../main/apps/common/soft3d/runtime/scratch.h"
#include <hal/hal.h>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace {
uint64_t hashCanvas(const LGFX_Sprite& canvas) {
    uint64_t hash=1469598103934665603ull;
    for(const auto pixel:canvas.frame()){hash^=pixel;hash*=1099511628211ull;}
    return hash;
}
template<class AssetResult,class Transform>
uint64_t render(AssetResult& result,Transform transform,soft3d::FrameWorkload& work) {
    assert(result.error==soft3d::AssetError::None);
    soft3d::SurfaceRaster<96,96> raster;
    raster.setSolidFastPath(true);raster.setSolidSpanFastPath(true);
    raster.setTrustedSolidDepthFastPath(true);raster.setSolidQuadFastPath(true);
    raster.begin(0,0);
    soft3d::Camera camera{};camera.principalX=48;camera.principalY=68;camera.focalLength=54;
    soft3d::ModelInstance instance{};instance.asset=&result.storage.asset;
    work=soft3d::renderModelAsset(raster,camera,instance,transform);
    LGFX_Sprite canvas;canvas.createSprite(96,96);canvas.fillScreen(0x0841);raster.blit(canvas);
    return hashCanvas(canvas);
}
}

int main() {
    static_assert(sizeof(soft3d::RigidRenderScratch<64,8>)==2196,
                  "update the asset report scratch formula and integration guide");
    soft3d::Scratch<std::array<uint16_t,32>> scratch;
    assert(!scratch.allocateIfBudget(0) && scratch.get()==nullptr);
    assert(scratch.allocateIfBudget(64u*1024u) && scratch.get()!=nullptr);
    soft3d::ScratchBuffer<uint16_t> scratchBuffer;
    assert(!scratchBuffer.allocateIfBudget(64,0) && scratchBuffer.get()==nullptr);
    assert(scratchBuffer.allocateIfBudget(64,64u*1024u) && scratchBuffer.capacity()==64);
    const auto profile=soft3d::game30Profile();
    soft3d::RenderCapabilities capabilities{};
    assert(soft3d::selectCoreMode(profile,capabilities,{8,6,400,300,0,6,0})==soft3d::CoreMode::Single);
    assert(soft3d::selectCoreMode(profile,capabilities,{64,48,4000,2200,0,48,0})==soft3d::CoreMode::Single);
    assert(soft3d::selectCoreMode(profile,capabilities,{2670,1965,41468,30595,10900,1965,0})==soft3d::CoreMode::DualBands);
    assert(soft3d::selectCoreMode(profile,capabilities,{4,1,20000,19000,1000,1,0})==soft3d::CoreMode::DualBands);
    capabilities.dualCoreBands=false;
    assert(soft3d::selectCoreMode(profile,capabilities,{2670,1965,41468,30595,10900,1965,0})==soft3d::CoreMode::Single);
    capabilities.dualCoreBands=true;
    assert(soft3d::selectMaterialMode(profile,capabilities,{8,6,0,0,0,6,0})==soft3d::MaterialMode::SolidFast);
    assert(soft3d::selectMaterialMode(profile,capabilities,{8,6,0,0,0,5,1})==soft3d::MaterialMode::General);
    assert(soft3d::selectScratchMode(profile,65536,16384)==soft3d::ScratchMode::Internal);
    assert(soft3d::selectScratchMode(profile,40000,16384)==soft3d::ScratchMode::Fallback);
    assert(soft3d::classifyFrameRate(33333)==soft3d::FrameRateGrade::Fps30);
    assert(soft3d::classifyFrameRate(66667)==soft3d::FrameRateGrade::Fps15);
    assert(soft3d::classifyFrameRate(66668)==soft3d::FrameRateGrade::Below15);
    auto crate=soft3d::samples::makeCrate();
    assert(crate.error==soft3d::AssetError::None);
    {
        auto invalid=crate.storage.asset;
        invalid.orderedPrimitiveIndices.size-=1;
        assert(soft3d::validate(invalid)==soft3d::AssetError::InvalidOrder);
        std::array<soft3d::Material,1> material{{
            {0xffff,soft3d::MaterialKind::Program,1,255,0}
        }};
        invalid=crate.storage.asset;invalid.materials={material.data(),material.size()};
        assert(soft3d::validate(invalid)==soft3d::AssetError::InvalidMetadata);
    }
    assert(crate.storage.asset.positions.size==8 && crate.storage.asset.primitives.size==6);
    assert(crate.storage.asset.skeleton.empty());
    soft3d::FrameWorkload crateWork{};
    const auto crateHash=render(crate,[](soft3d::Vec3 p,uint16_t) {
        return soft3d::CameraPoint{p.x,p.y-0.45f,p.z+3.f};
    },crateWork);
    assert(crateWork.uniqueVertices==8 && crateWork.generalPrimitives==0);
    assert(crateWork.totalTriangles==12 && crateWork.submittedTriangles>0 &&
           crateWork.submittedTriangles<crateWork.totalTriangles);
    assert(crateWork.totalQuads==6 && crateWork.submittedQuads==crateWork.visiblePrimitives);
    {
        soft3d::SurfaceRaster<96,96> raster;raster.setSolidFastPath(true);raster.begin(0,0);
        soft3d::Camera camera{};camera.principalX=48;camera.principalY=68;camera.focalLength=54;
        soft3d::ModelInstance instance{};instance.asset=&crate.storage.asset;
        instance.world.value={{1,0,0,0,0,1,0,-.45f,0,0,1,3.f}};
        std::array<soft3d::ModelInstance,1> instances{{instance}};
        const auto sceneWork=soft3d::renderScene(raster,camera,{{instances.data(),instances.size()}});
        assert(sceneWork.uniqueVertices==8 &&
               sceneWork.totalTriangles==crateWork.totalTriangles &&
               sceneWork.submittedTriangles==crateWork.submittedTriangles &&
               sceneWork.totalQuads==crateWork.totalQuads &&
               sceneWork.submittedQuads==crateWork.submittedQuads);
        LGFX_Sprite canvas;canvas.createSprite(96,96);canvas.fillScreen(0x0841);raster.blit(canvas);
        assert(hashCanvas(canvas)==crateHash);

        soft3d::SurfaceRaster<96,96> hiddenRaster;hiddenRaster.begin(0,0);
        instances[0].visibilityMask=0;
        const auto hiddenWork=soft3d::renderScene(
            hiddenRaster,camera,{{instances.data(),instances.size()}});
        assert(hiddenWork.totalTriangles==0 && hiddenWork.submittedTriangles==0 &&
               hiddenWork.totalQuads==0 && hiddenWork.submittedQuads==0);
        instances[0].visibilityMask=0xffffu;

        soft3d::SurfaceRaster<96,96> cachedRaster;
        cachedRaster.setSolidFastPath(true);cachedRaster.setSolidSpanFastPath(true);
        cachedRaster.setTrustedSolidDepthFastPath(true);cachedRaster.setSolidQuadFastPath(true);
        cachedRaster.begin(0,0);
        soft3d::RigidRenderScratch<64,8> cache{};
        const auto cachedSceneWork=soft3d::renderSceneCachedIndexed(
            cachedRaster,camera,{{instances.data(),instances.size()}},cache);
        assert(cachedSceneWork.totalTriangles==sceneWork.totalTriangles &&
               cachedSceneWork.submittedTriangles==sceneWork.submittedTriangles &&
               cachedSceneWork.totalQuads==sceneWork.totalQuads &&
               cachedSceneWork.submittedQuads==sceneWork.submittedQuads);
        assert(cache.transformedVertices==8 && cache.projectedVertexCount==8 &&
               cache.composedTransforms==0);
        LGFX_Sprite cachedCanvas;cachedCanvas.createSprite(96,96);
        cachedCanvas.fillScreen(0x0841);cachedRaster.blit(cachedCanvas);
        assert(hashCanvas(cachedCanvas)==crateHash);
    }

    auto chain=soft3d::samples::makeRigidChain();
    assert(chain.error==soft3d::AssetError::None);
    assert(chain.storage.asset.positions.size==64 && chain.storage.asset.primitives.size==48);
    assert(chain.storage.asset.skeleton.size==8);
    std::array<soft3d::BoneTransform,8> pose{};
    for(auto& bone:pose)bone.value={{1,0,0,0,0,1,0,0,0,0,1,0}};
    pose[1].value[7]=.10f;
    soft3d::ModelInstance posed{};posed.asset=&chain.storage.asset;posed.pose={pose.data(),pose.size()};
    posed.world.value={{1,0,0,0,0,1,0,-.45f,0,0,1,3.2f}};
    const auto moved=soft3d::transformInstancePoint(posed,chain.storage.asset.positions[8],1);
    assert(std::abs(moved.y-(chain.storage.asset.positions[8].y-.35f))<.0001f);
    soft3d::FrameWorkload chainWork{};
    const auto chainHash=render(chain,[](soft3d::Vec3 p,uint16_t bone) {
        const float lift=(bone&1u)?.10f:0.f;
        return soft3d::CameraPoint{p.x,p.y-0.45f+lift,p.z+3.2f};
    },chainWork);
    assert(chainWork.uniqueVertices==64 && chainWork.generalPrimitives==0);
    assert(chainWork.totalTriangles==96 && chainWork.submittedTriangles>0 &&
           chainWork.submittedTriangles<chainWork.totalTriangles);
    assert(chainWork.totalQuads==48 && chainWork.submittedQuads==chainWork.visiblePrimitives);
    {
        soft3d::SurfaceRaster<96,96> referenceRaster;
        referenceRaster.setSolidFastPath(true);referenceRaster.begin(0,0);
        soft3d::Camera camera{};camera.principalX=48;camera.principalY=68;camera.focalLength=54;
        const auto referenceWork=soft3d::renderModelAsset(referenceRaster,camera,posed);
        assert(referenceWork.uniqueVertices==64 &&
               referenceWork.totalTriangles==chainWork.totalTriangles &&
               referenceWork.submittedTriangles==chainWork.submittedTriangles &&
               referenceWork.totalQuads==chainWork.totalQuads &&
               referenceWork.submittedQuads==chainWork.submittedQuads);
        LGFX_Sprite referenceCanvas;referenceCanvas.createSprite(96,96);
        referenceCanvas.fillScreen(0x0841);referenceRaster.blit(referenceCanvas);
        const auto referenceHash=hashCanvas(referenceCanvas);

        soft3d::SurfaceRaster<96,96> raster;raster.setSolidFastPath(true);raster.begin(0,0);
        soft3d::RigidRenderScratch<64,8> cache{};
        const auto cachedWork=soft3d::renderRigidModelAssetCached(raster,camera,posed,cache);
        assert(cachedWork.uniqueVertices==64 &&
               cachedWork.totalTriangles==referenceWork.totalTriangles &&
               cachedWork.submittedTriangles==referenceWork.submittedTriangles &&
               cachedWork.totalQuads==referenceWork.totalQuads &&
               cachedWork.submittedQuads==referenceWork.submittedQuads);
        assert(cache.transformedVertices==64 && cache.projectedVertexCount==64 &&
               cache.composedTransforms==8);
        LGFX_Sprite canvas;canvas.createSprite(96,96);canvas.fillScreen(0x0841);raster.blit(canvas);
        assert(hashCanvas(canvas)==referenceHash);

        soft3d::SurfaceRaster<96,96> fallbackRaster;
        fallbackRaster.setSolidFastPath(true);fallbackRaster.begin(0,0);
        soft3d::RigidRenderScratch<8,1> undersizedCache{};
        const auto fallbackWork=soft3d::renderRigidModelAssetCached(
            fallbackRaster,camera,posed,undersizedCache);
        assert(fallbackWork.uniqueVertices==64 &&
               fallbackWork.totalTriangles==referenceWork.totalTriangles &&
               fallbackWork.submittedTriangles==referenceWork.submittedTriangles &&
               fallbackWork.totalQuads==referenceWork.totalQuads &&
               fallbackWork.submittedQuads==referenceWork.submittedQuads);
        LGFX_Sprite fallbackCanvas;fallbackCanvas.createSprite(96,96);
        fallbackCanvas.fillScreen(0x0841);fallbackRaster.blit(fallbackCanvas);
        assert(hashCanvas(fallbackCanvas)==referenceHash);
    }
    assert(crateHash!=chainHash);
    std::cout<<"soft3d assets: crate vertices="<<crateWork.uniqueVertices
             <<" primitives="<<crateWork.visiblePrimitives<<" hash="<<crateHash
             <<" rigid vertices="<<chainWork.uniqueVertices
             <<" primitives="<<chainWork.visiblePrimitives<<" hash="<<chainHash<<"\n";
}
