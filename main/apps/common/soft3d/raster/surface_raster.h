#pragma once

// Compatibility bridge for the first extraction step. The proven pixel
// kernel remains byte-for-byte in the Racer header while all new callers use
// the neutral soft3d API. The material adapter below is the only place that
// maps an asset material to the legacy programmable-material enum.
#include "../../../app_lets_and_go_racer/view/car_surface_raster.h"
#include "../asset/model_asset.h"
#include "../runtime/render_profile.h"

namespace soft3d {

template<int Width,int Height>
using SurfaceRaster=lets_and_go::CarSurfaceRaster<Width,Height>;
using Camera=lets_and_go::TrackCamera;
using CameraPoint=lets_and_go::TrackCameraPoint;
using SurfaceVertex=lets_and_go::CarSurfaceVertex;
using ScreenVertex=lets_and_go::CarScreenVertex;
using PreparedPanel=lets_and_go::PreparedCarPanel;
using PreparedSolidPanel=lets_and_go::PreparedSolidPanel;
using SolidCommand=lets_and_go::PreparedSolidRasterPanel;
using IndexedSolidCommand=lets_and_go::PreparedIndexedSolidRasterPanel;

static_assert(sizeof(IndexedSolidCommand)==12,"indexed solid ABI must remain 12 bytes");

struct SolidMaterialAdapter {
    lets_and_go::CarPaint operator()(const Material&) const{return lets_and_go::CarPaint::Solid;}
};

template<class Raster,class CameraType,class TransformPoint,
         class MaterialAdapter=SolidMaterialAdapter>
FrameWorkload renderModelAsset(Raster& raster,const CameraType& camera,
                               const ModelInstance& instance,TransformPoint transform,
                               MaterialAdapter materialAdapter={}) {
    FrameWorkload work{};
    if(!instance.asset || instance.lod>=instance.asset->lods.size)return work;
    const auto& asset=*instance.asset;const auto& lod=asset.lods[instance.lod];
    work.uniqueVertices=uint32_t(asset.positions.size);
    for(uint32_t draw=0;draw<lod.primitiveCount;++draw) {
        const auto ordered=lod.firstPrimitive+draw;
        const auto primitiveIndex=asset.orderedPrimitiveIndices.empty()
            ? ordered:asset.orderedPrimitiveIndices[ordered];
        if(primitiveIndex>=asset.primitives.size)continue;
        const auto& primitive=asset.primitives[primitiveIndex];
        const auto& material=asset.materials[primitive.material];
        std::array<SurfaceVertex,4> vertex{};
        for(unsigned i=0;i<4;++i) {
            const auto point=transform(asset.positions[primitive.index[i]],primitive.rigidPart);
            vertex[i]={point.x,point.y,point.z,0,0};
        }
        const auto program=materialAdapter(material);
        raster.cameraTriangle(camera,vertex[0],vertex[1],vertex[2],material.color,program,material.light);
        if(primitive.topology==PrimitiveTopology::Quad)
            raster.cameraTriangle(camera,vertex[0],vertex[2],vertex[3],material.color,program,material.light);
        ++work.visiblePrimitives;
        if(material.kind==MaterialKind::Solid)++work.solidPrimitives;
        else ++work.generalPrimitives;
    }
    return work;
}

} // namespace soft3d
