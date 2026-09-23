#pragma once

#include "model_asset.h"
#include "../frontend/shared_vertex_index.h"
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <new>

namespace soft3d {

template<std::size_t MaxVertices,std::size_t MaxPrimitives,std::size_t MaxMaterials,
         std::size_t MaxLods=4,std::size_t MaxBones=32>
struct ModelAssetStorage {
    std::array<Vec3,MaxVertices> positions{};
    std::array<Vec3,MaxPrimitives> normals{},anchors{};
    std::array<Primitive,MaxPrimitives> primitives{};
    std::array<Material,MaxMaterials> materials{};
    std::array<uint16_t,MaxPrimitives> order{};
    std::array<LodRange,MaxLods> lods{};
    std::array<Bone,MaxBones> bones{};
    std::size_t positionCount=0,primitiveCount=0,materialCount=0,lodCount=0,boneCount=0;
    ModelAsset asset{};

    void clear() {
        positionCount=primitiveCount=materialCount=lodCount=boneCount=0;
        asset={};
    }
    void seal(const char* name,Bounds bounds,uint32_t flags,AssetProfile profile) {
        asset.name=name;
        asset.positions={positions.data(),positionCount};
        asset.normals={normals.data(),primitiveCount};
        asset.anchors={anchors.data(),primitiveCount};
        asset.primitives={primitives.data(),primitiveCount};
        asset.materials={materials.data(),materialCount};
        asset.orderedPrimitiveIndices={order.data(),primitiveCount};
        asset.lods={lods.data(),lodCount};
        asset.skeleton={bones.data(),boneCount};
        asset.bounds=bounds;asset.flags=flags;asset.profile=profile;
        asset.memory.residentBytes=uint32_t(positionCount*sizeof(Vec3)+primitiveCount*(sizeof(Primitive)+2*sizeof(Vec3))+
            materialCount*sizeof(Material)+primitiveCount*sizeof(uint16_t)+lodCount*sizeof(LodRange)+
            boneCount*sizeof(Bone));
        asset.memory.maximumProjectionBytes=uint32_t(positionCount*sizeof(Vec3));
        asset.memory.maximumCommandBytes=uint32_t(primitiveCount*12u);
        asset.memory.maximumVisiblePrimitives=uint16_t(std::min<std::size_t>(primitiveCount,65535));
    }
};

struct PanelAssetOptions {
    const char* name="unnamed";
    uint32_t flags=AssetDeterministicOrder;
    AssetProfile profile=AssetProfile::SolidStatic;
    float groundY=0;
};

constexpr std::size_t nextPowerOfTwo(std::size_t minimum) {
    std::size_t value=1;while(value<minimum)value<<=1;return value;
}

template<std::size_t MaxVertices,std::size_t MaxPrimitives,std::size_t MaxMaterials,
         std::size_t MaxLods,std::size_t MaxBones,class PanelAccessor>
AssetError buildPanelAsset(
    ModelAssetStorage<MaxVertices,MaxPrimitives,MaxMaterials,MaxLods,MaxBones>& output,
    std::size_t panelCount,PanelAccessor panel,const PanelAssetOptions& options={}) {
    output.clear();
    if(!panelCount || panelCount>MaxPrimitives)return AssetError::MissingData;
    auto cornerIndex=std::unique_ptr<std::array<uint16_t,MaxPrimitives*4>>(
        new(std::nothrow) std::array<uint16_t,MaxPrimitives*4>{});
    auto representatives=std::unique_ptr<std::array<uint16_t,MaxVertices>>(
        new(std::nothrow) std::array<uint16_t,MaxVertices>{});
    if(!cornerIndex || !representatives)return AssetError::MissingData;
    constexpr std::size_t hashSlots=nextPowerOfTwo(MaxVertices*2);
    const auto indexed=indexSharedVertices<hashSlots>(panelCount*4,[&](std::size_t corner) {
        const auto face=panel(corner/4);const auto p=face.point(corner%4);
        return VertexKey{p,face.rigidPart()};
    },cornerIndex->data(),representatives->data());
    if(indexed.count>MaxVertices)return AssetError::IndexOutOfRange;
    output.positionCount=indexed.count;
    Bounds bounds{};
    bounds.minimum={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    bounds.maximum={-bounds.minimum.x,-bounds.minimum.y,-bounds.minimum.z};
    for(std::size_t i=0;i<indexed.count;++i) {
        const auto face=panel((*representatives)[i]/4);
        const auto point=face.point((*representatives)[i]%4);
        output.positions[i]=point;
        bounds.minimum.x=std::min(bounds.minimum.x,point.x);bounds.maximum.x=std::max(bounds.maximum.x,point.x);
        bounds.minimum.y=std::min(bounds.minimum.y,point.y);bounds.maximum.y=std::max(bounds.maximum.y,point.y);
        bounds.minimum.z=std::min(bounds.minimum.z,point.z);bounds.maximum.z=std::max(bounds.maximum.z,point.z);
    }
    bounds.center={(bounds.minimum.x+bounds.maximum.x)*.5f,(bounds.minimum.y+bounds.maximum.y)*.5f,
                   (bounds.minimum.z+bounds.maximum.z)*.5f};bounds.groundY=options.groundY;
    for(std::size_t i=0;i<output.positionCount;++i) {
        const auto& point=output.positions[i];
        const float x=point.x-bounds.center.x,y=point.y-bounds.center.y,z=point.z-bounds.center.z;
        bounds.radius=std::max(bounds.radius,std::sqrt(x*x+y*y+z*z));
    }
    for(std::size_t i=0;i<panelCount;++i) {
        const auto face=panel(i);Material material=face.material();
        std::size_t materialIndex=0;
        while(materialIndex<output.materialCount) {
            const auto& value=output.materials[materialIndex];
            if(value.color==material.color&&value.kind==material.kind&&value.program==material.program&&
               value.light==material.light&&value.flags==material.flags)break;
            ++materialIndex;
        }
        if(materialIndex==output.materialCount) {
            if(output.materialCount==MaxMaterials)return AssetError::MaterialOutOfRange;
            output.materials[output.materialCount++]=material;
        }
        Primitive primitive{};
        for(unsigned corner=0;corner<4;++corner)primitive.index[corner]=(*cornerIndex)[i*4+corner];
        primitive.material=uint16_t(materialIndex);primitive.rigidPart=face.rigidPart();
        primitive.topology=primitive.index[2]==primitive.index[3]
            ? PrimitiveTopology::Triangle:PrimitiveTopology::Quad;
        primitive.flags=face.flags();
        output.primitives[i]=primitive;output.order[i]=uint16_t(i);
        output.normals[i]=face.normal();output.anchors[i]=face.anchor();
    }
    output.primitiveCount=panelCount;output.lods[0]={0,uint32_t(panelCount),9999.f};output.lodCount=1;
    if(output.materialCount==0)return AssetError::MissingData;
    output.seal(options.name,bounds,options.flags,options.profile);
    return validate(output.asset);
}

} // namespace soft3d
