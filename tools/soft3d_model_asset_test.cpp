#include "../main/apps/common/soft3d/asset/builtin_samples.h"
#include "../main/apps/common/soft3d/raster/surface_raster.h"
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
    auto crate=soft3d::samples::makeCrate();
    assert(crate.error==soft3d::AssetError::None);
    assert(crate.storage.asset.positions.size==8 && crate.storage.asset.primitives.size==6);
    assert(crate.storage.asset.skeleton.empty());
    soft3d::FrameWorkload crateWork{};
    const auto crateHash=render(crate,[](soft3d::Vec3 p,uint16_t) {
        return soft3d::CameraPoint{p.x,p.y-0.45f,p.z+3.f};
    },crateWork);
    assert(crateWork.uniqueVertices==8 && crateWork.visiblePrimitives==6 && crateWork.generalPrimitives==0);

    auto chain=soft3d::samples::makeRigidChain();
    assert(chain.error==soft3d::AssetError::None);
    assert(chain.storage.asset.positions.size==64 && chain.storage.asset.primitives.size==48);
    assert(chain.storage.asset.skeleton.size==8);
    soft3d::FrameWorkload chainWork{};
    const auto chainHash=render(chain,[](soft3d::Vec3 p,uint16_t bone) {
        const float lift=(bone&1u)?.10f:0.f;
        return soft3d::CameraPoint{p.x,p.y-0.45f+lift,p.z+3.2f};
    },chainWork);
    assert(chainWork.uniqueVertices==64 && chainWork.visiblePrimitives==48 && chainWork.generalPrimitives==0);
    assert(crateHash!=chainHash);
    std::cout<<"soft3d assets: crate vertices="<<crateWork.uniqueVertices
             <<" primitives="<<crateWork.visiblePrimitives<<" hash="<<crateHash
             <<" rigid vertices="<<chainWork.uniqueVertices
             <<" primitives="<<chainWork.visiblePrimitives<<" hash="<<chainHash<<"\n";
}
