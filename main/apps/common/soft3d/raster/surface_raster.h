#pragma once

#ifndef STOPWATCH_COLLECT_TOPOLOGY_STATS
#define STOPWATCH_COLLECT_TOPOLOGY_STATS 1
#endif

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
template<int Width,int Height>
using MeasuredSurfaceRaster=lets_and_go::CarSurfaceRaster<Width,Height,true>;
using RasterMetrics=lets_and_go::SurfaceRasterMetrics;
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

inline Vec3 transformPoint(const std::array<float,12>& transform,Vec3 point) {
    return {transform[0]*point.x+transform[1]*point.y+transform[2]*point.z+transform[3],
            transform[4]*point.x+transform[5]*point.y+transform[6]*point.z+transform[7],
            transform[8]*point.x+transform[9]*point.y+transform[10]*point.z+transform[11]};
}

// Compose two row-major affine transforms so each rigid vertex needs one
// matrix application instead of a bone transform followed by a world transform.
inline std::array<float,12> composeTransform(const std::array<float,12>& outer,
                                             const std::array<float,12>& inner) {
    std::array<float,12> result{};
    for(unsigned row=0;row<3;++row) {
        const unsigned r=row*4;
        for(unsigned column=0;column<3;++column)
            result[r+column]=outer[r]*inner[column]+outer[r+1]*inner[4+column]+
                             outer[r+2]*inner[8+column];
        result[r+3]=outer[r]*inner[3]+outer[r+1]*inner[7]+
                    outer[r+2]*inner[11]+outer[r+3];
    }
    return result;
}

inline CameraPoint transformInstancePoint(const ModelInstance& instance,Vec3 point,uint16_t rigidPart) {
    if(!instance.pose.empty() && rigidPart<instance.pose.size)
        point=transformPoint(instance.pose[rigidPart].value,point);
    point=transformPoint(instance.world.value,point);
    return {point.x,point.y,point.z};
}

inline uint32_t primitiveTriangleCount(PrimitiveTopology topology) {
    return topology==PrimitiveTopology::Quad?2u:1u;
}

enum class PrimitiveDisposition : uint8_t {Submit,Backface,Offscreen};

// Prepare once, then reuse the projected/bounded panel for raster submission.
// Backface rejection is deliberately limited to primitives wholly in front of
// the near plane; intersecting primitives keep the rasterizer's reference clip.
template<class Raster,class CameraType>
PrimitiveDisposition preparePrimitive(PreparedPanel& prepared,const Raster& raster,
        const CameraType& camera,const std::array<SurfaceVertex,4>& vertex,
        PrimitiveTopology topology,uint8_t flags,uint16_t color,
        lets_and_go::CarPaint paint,uint8_t light,
        const std::array<ScreenVertex,4>* projected=nullptr) {
    const unsigned cornerCount=unsigned(topology);
    bool allInFront=true;
    for(unsigned i=0;i<cornerCount;++i)
        allInFront=allInFront && vertex[i].z>=lets_and_go::kTrackNearPlane;
    if(allInFront && !(flags&PrimitiveTwoSided)) {
        const auto& a=vertex[0];const auto& b=vertex[1];const auto& c=vertex[2];
        const float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z;
        const float vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
        const float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
        const float facing=nx*(-a.x)+ny*(-a.y)+nz*(-a.z);
        if(facing<-.000001f)return PrimitiveDisposition::Backface;
    }

    prepared.color=color;prepared.paint=paint;prepared.light=light;
    new (&prepared.camera) decltype(prepared.camera);
    unsigned front=0;
    for(unsigned i=0;i<4;++i) {
        prepared.camera[i]=vertex[i];
        if(vertex[i].z>=lets_and_go::kTrackNearPlane)++front;
    }
    prepared.visibility=front==0?0:front==4?1:2;
    if(!prepared.visibility)return PrimitiveDisposition::Offscreen;
    prepared.left=prepared.top=1e20f;prepared.right=prepared.bottom=-1e20f;
    const auto bound=[&](ScreenVertex point) {
        prepared.left=std::min(prepared.left,point.x);
        prepared.right=std::max(prepared.right,point.x);
        prepared.top=std::min(prepared.top,point.y);
        prepared.bottom=std::max(prepared.bottom,point.y);
    };
    if(front==4) {
        new (&prepared.screen) decltype(prepared.screen);
        if(projected)prepared.screen=*projected;
        else for(unsigned i=0;i<4;++i)
            prepared.screen[i]=lets_and_go::projectCarSurface(camera,vertex[i]);
        for(unsigned i=0;i<4;++i)bound(prepared.screen[i]);
    } else {
        const unsigned triangleCount=primitiveTriangleCount(topology);
        for(unsigned triangle=0;triangle<triangleCount;++triangle) {
            const std::array<unsigned,3> index=triangle==0
                ?std::array<unsigned,3>{{0,1,2}}:std::array<unsigned,3>{{0,2,3}};
            for(unsigned i=0;i<3;++i) {
                const auto p=vertex[index[i]],q=vertex[index[(i+1)%3]];
                if(p.z>=lets_and_go::kTrackNearPlane)
                    bound(lets_and_go::projectCarSurface(camera,p));
                if((p.z>=lets_and_go::kTrackNearPlane)!=(q.z>=lets_and_go::kTrackNearPlane)) {
                    const float t=(lets_and_go::kTrackNearPlane-p.z)/(q.z-p.z);
                    bound(lets_and_go::projectCarSurface(camera,
                        {p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,lets_and_go::kTrackNearPlane,0,0}));
                }
            }
        }
    }
    const int left=raster.viewportX(),top=raster.viewportY();
    if(prepared.right<left || prepared.left>=left+raster.viewportWidth() ||
       prepared.bottom<top || prepared.top>=top+raster.viewportHeight())
        return PrimitiveDisposition::Offscreen;
    return PrimitiveDisposition::Submit;
}

inline void accumulateWorkload(FrameWorkload& total,const FrameWorkload& work) {
    total.uniqueVertices+=work.uniqueVertices;
    total.visiblePrimitives+=work.visiblePrimitives;
    total.testedPixels+=work.testedPixels;total.writtenPixels+=work.writtenPixels;
    total.depthRejectedPixels+=work.depthRejectedPixels;
    total.solidPrimitives+=work.solidPrimitives;total.generalPrimitives+=work.generalPrimitives;
    total.totalTriangles+=work.totalTriangles;total.submittedTriangles+=work.submittedTriangles;
    total.culledPrimitives+=work.culledPrimitives;
    total.offscreenPrimitives+=work.offscreenPrimitives;
    total.totalQuads+=work.totalQuads;total.submittedQuads+=work.submittedQuads;
}

template<class Raster,class CameraType,class TransformPoint,
         class MaterialAdapter=SolidMaterialAdapter>
FrameWorkload renderModelAsset(Raster& raster,const CameraType& camera,
                               const ModelInstance& instance,TransformPoint transform,
                               MaterialAdapter materialAdapter={}) {
    FrameWorkload work{};
    if(!instance.asset || !instance.visibilityMask || instance.lod>=instance.asset->lods.size)
        return work;
    const auto& asset=*instance.asset;const auto& lod=asset.lods[instance.lod];
    work.uniqueVertices=uint32_t(asset.positions.size);
    for(uint32_t draw=0;draw<lod.primitiveCount;++draw) {
        const auto ordered=lod.firstPrimitive+draw;
        const auto primitiveIndex=asset.orderedPrimitiveIndices.empty()
            ? ordered:asset.orderedPrimitiveIndices[ordered];
        if(primitiveIndex>=asset.primitives.size)continue;
        const auto& primitive=asset.primitives[primitiveIndex];
        const auto& material=asset.materials[primitive.material];
        const bool quad=primitive.topology==PrimitiveTopology::Quad;
        const auto triangleCount=quad?2u:1u;
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
        work.totalTriangles+=triangleCount;
        work.totalQuads+=uint32_t(quad);
#endif
        std::array<SurfaceVertex,4> vertex{};
        for(unsigned i=0;i<4;++i) {
            const auto point=transform(asset.positions[primitive.index[i]],primitive.rigidPart);
            vertex[i]={point.x,point.y,point.z,0,0};
        }
        const auto program=materialAdapter(material);
        PreparedPanel prepared{};
        const auto disposition=preparePrimitive(prepared,raster,camera,vertex,
            primitive.topology,primitive.flags,material.color,program,material.light);
        if(disposition==PrimitiveDisposition::Backface){++work.culledPrimitives;continue;}
        if(disposition==PrimitiveDisposition::Offscreen){++work.offscreenPrimitives;continue;}
        if(material.kind==MaterialKind::Solid)
            raster.preparedSolidPanelRows(camera,lets_and_go::compactSolidPanel(prepared),
                raster.viewportY(),raster.viewportY()+raster.viewportHeight()-1);
        else raster.preparedPanel(camera,prepared);
        ++work.visiblePrimitives;
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
        work.submittedTriangles+=triangleCount;
        work.submittedQuads+=uint32_t(quad);
#endif
        if(material.kind==MaterialKind::Solid)++work.solidPrimitives;
        else ++work.generalPrimitives;
    }
    return work;
}

template<class Raster,class CameraType>
FrameWorkload renderModelAsset(Raster& raster,const CameraType& camera,
                               const ModelInstance& instance) {
    return renderModelAsset(raster,camera,instance,[&](Vec3 point,uint16_t rigidPart) {
        return transformInstancePoint(instance,point,rigidPart);
    });
}

template<std::size_t MaxVertices,std::size_t MaxBones>
struct RigidRenderScratch {
    std::array<CameraPoint,MaxVertices> vertices{};
    std::array<CameraPoint,MaxVertices> projectedVertices{};
    std::array<uint16_t,MaxVertices> rigidParts{};
    std::array<uint8_t,MaxVertices> validVertices{};
    std::array<uint8_t,MaxVertices> validProjected{};
    std::array<std::array<float,12>,MaxBones> combinedTransforms{};
    std::array<uint8_t,MaxBones> validTransforms{};
    uint32_t transformedVertices=0,projectedVertexCount=0,composedTransforms=0;

    void reset(std::size_t vertexCount,std::size_t boneCount) {
        std::fill_n(validVertices.begin(),std::min(vertexCount,MaxVertices),uint8_t(0));
        std::fill_n(validProjected.begin(),std::min(vertexCount,MaxVertices),uint8_t(0));
        std::fill_n(validTransforms.begin(),std::min(boneCount,MaxBones),uint8_t(0));
        transformedVertices=0;projectedVertexCount=0;composedTransforms=0;
    }
};

// Fast path for one-bone-per-primitive assets. Indexed vertices are transformed
// once per instance and bone/world matrices are composed once per used bone.
// If an asset aliases one index across different rigid parts, that corner falls
// back to the reference transform so the cache never changes rendering semantics.
template<class Raster,class CameraType,std::size_t MaxVertices,std::size_t MaxBones,
         class MaterialAdapter=SolidMaterialAdapter>
FrameWorkload renderIndexedModelAssetCached(Raster& raster,const CameraType& camera,
        const ModelInstance& instance,RigidRenderScratch<MaxVertices,MaxBones>& scratch,
        MaterialAdapter materialAdapter={}) {
    if(!instance.asset || !instance.visibilityMask || instance.lod>=instance.asset->lods.size)
        return {};
    const auto& asset=*instance.asset;
    if(asset.positions.size>MaxVertices || instance.pose.size>MaxBones)
        return renderModelAsset(raster,camera,instance);
    scratch.reset(asset.positions.size,instance.pose.size);
    const auto& lod=asset.lods[instance.lod];FrameWorkload work{};
    work.uniqueVertices=uint32_t(asset.positions.size);
    for(uint32_t draw=0;draw<lod.primitiveCount;++draw) {
        const auto ordered=lod.firstPrimitive+draw;
        const auto primitiveIndex=asset.orderedPrimitiveIndices.empty()
            ? ordered:asset.orderedPrimitiveIndices[ordered];
        if(primitiveIndex>=asset.primitives.size)continue;
        const auto& primitive=asset.primitives[primitiveIndex];
        const auto& material=asset.materials[primitive.material];
        const bool quad=primitive.topology==PrimitiveTopology::Quad;
        const auto triangleCount=quad?2u:1u;
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
        work.totalTriangles+=triangleCount;
        work.totalQuads+=uint32_t(quad);
#endif
        std::array<SurfaceVertex,4> vertex{};
        std::array<ScreenVertex,4> projected{};bool allProjected=true;
        for(unsigned i=0;i<4;++i) {
            const auto index=primitive.index[i];CameraPoint point{};
            const bool cached=scratch.validVertices[index] &&
                              scratch.rigidParts[index]==primitive.rigidPart;
            const bool cacheable=!scratch.validVertices[index];
            if(cached) {
                point=scratch.vertices[index];
            } else {
                if(!instance.pose.empty() && primitive.rigidPart<instance.pose.size) {
                    if(!scratch.validTransforms[primitive.rigidPart]) {
                        scratch.combinedTransforms[primitive.rigidPart]=composeTransform(
                            instance.world.value,instance.pose[primitive.rigidPart].value);
                        scratch.validTransforms[primitive.rigidPart]=1;
                        ++scratch.composedTransforms;
                    }
                    const auto value=transformPoint(
                        scratch.combinedTransforms[primitive.rigidPart],asset.positions[index]);
                    point={value.x,value.y,value.z};
                } else {
                    const auto value=transformPoint(instance.world.value,asset.positions[index]);
                    point={value.x,value.y,value.z};
                }
                if(cacheable) {
                    scratch.vertices[index]=point;scratch.rigidParts[index]=primitive.rigidPart;
                    scratch.validVertices[index]=1;++scratch.transformedVertices;
                }
            }
            vertex[i]={point.x,point.y,point.z,0,0};
            if(point.z>=lets_and_go::kTrackNearPlane) {
                CameraPoint screen{};
                if(cached && scratch.validProjected[index])screen=scratch.projectedVertices[index];
                else {
                    const auto value=lets_and_go::projectCarSurface(camera,vertex[i]);
                    screen={value.x,value.y,value.depth};
                    if(cacheable) {
                        scratch.projectedVertices[index]=screen;
                        scratch.validProjected[index]=1;++scratch.projectedVertexCount;
                    }
                }
                projected[i]={screen.x,screen.y,screen.z,0,0};
            } else allProjected=false;
        }
        const auto program=materialAdapter(material);
        PreparedPanel prepared{};
        const auto disposition=preparePrimitive(prepared,raster,camera,vertex,
            primitive.topology,primitive.flags,material.color,program,material.light,
            allProjected?&projected:nullptr);
        if(disposition==PrimitiveDisposition::Backface){++work.culledPrimitives;continue;}
        if(disposition==PrimitiveDisposition::Offscreen){++work.offscreenPrimitives;continue;}
        if(material.kind==MaterialKind::Solid)
            raster.preparedSolidPanelRows(camera,lets_and_go::compactSolidPanel(prepared),
                raster.viewportY(),raster.viewportY()+raster.viewportHeight()-1);
        else raster.preparedPanel(camera,prepared);
        ++work.visiblePrimitives;
#if STOPWATCH_COLLECT_TOPOLOGY_STATS
        work.submittedTriangles+=triangleCount;
        work.submittedQuads+=uint32_t(quad);
#endif
        if(material.kind==MaterialKind::Solid)++work.solidPrimitives;
        else ++work.generalPrimitives;
    }
    return work;
}

template<class Raster,class CameraType,std::size_t MaxVertices,std::size_t MaxBones>
FrameWorkload renderRigidModelAssetCached(Raster& raster,const CameraType& camera,
        const ModelInstance& instance,RigidRenderScratch<MaxVertices,MaxBones>& scratch) {
    return renderIndexedModelAssetCached(raster,camera,instance,scratch);
}

template<class Raster,class CameraType,std::size_t MaxVertices,std::size_t MaxBones>
FrameWorkload renderSceneCachedIndexed(Raster& raster,const CameraType& camera,
        const SceneView& scene,RigidRenderScratch<MaxVertices,MaxBones>& scratch) {
    FrameWorkload total{};
    for(const auto& instance:scene.instances) {
        const auto work=renderIndexedModelAssetCached(raster,camera,instance,scratch);
        accumulateWorkload(total,work);
    }
    return total;
}

template<class Raster,class CameraType,std::size_t MaxVertices,std::size_t MaxBones>
FrameWorkload renderSceneCachedRigid(Raster& raster,const CameraType& camera,
        const SceneView& scene,RigidRenderScratch<MaxVertices,MaxBones>& scratch) {
    return renderSceneCachedIndexed(raster,camera,scene,scratch);
}

template<class Raster,class CameraType>
FrameWorkload renderScene(Raster& raster,const CameraType& camera,const SceneView& scene) {
    FrameWorkload total{};
    for(const auto& instance:scene.instances) {
        const auto work=renderModelAsset(raster,camera,instance);
        accumulateWorkload(total,work);
    }
    return total;
}

} // namespace soft3d
