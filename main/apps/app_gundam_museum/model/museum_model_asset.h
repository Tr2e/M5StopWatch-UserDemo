#pragma once

#include "rx78.h"
#include "../../common/soft3d/asset/model_asset_storage.h"

namespace gundam_museum {

using MuseumModelAssetStorage=soft3d::ModelAssetStorage<Mesh::capacity,Mesh::capacity,256,1,32>;

struct MuseumPanelAssetView {
    const Mesh* mesh=nullptr;
    std::size_t index=0;
    soft3d::Vec3 point(std::size_t corner) const {
        const auto p=mesh->panels[index].point[corner];return {p.x,p.y,p.z};
    }
    uint16_t rigidPart() const{return mesh->panels[index].rigidPart;}
    soft3d::Material material() const {
        const auto& face=mesh->panels[index];
        const auto kind=face.paint==lets_and_go::CarPaint::Solid
            ? soft3d::MaterialKind::Solid:soft3d::MaterialKind::Program;
        return {face.color,kind,uint8_t(face.paint),face.light,0};
    }
    uint8_t flags() const {
        return mesh->twoSided[index]?soft3d::PrimitiveTwoSided:0;
    }
    soft3d::Vec3 normal() const {
        const auto p=mesh->normals[index];return {p.x,p.y,p.z};
    }
    soft3d::Vec3 anchor() const {
        const auto p=mesh->anchors[index];return {p.x,p.y,p.z};
    }
};

inline soft3d::AssetError buildMuseumModelAsset(MuseumModelAssetStorage& output,const Mesh& mesh,
                                                const char* name,uint32_t flags,
                                                soft3d::AssetProfile profile) {
    return soft3d::buildPanelAsset(output,mesh.count,[&](std::size_t index) {
        return MuseumPanelAssetView{&mesh,index};
    },{name,flags,profile,.025f});
}

struct MuseumAssetRegistration {
    const char* name="museum_model";
    uint32_t flags=soft3d::AssetDeterministicOrder;
    soft3d::AssetProfile profile=soft3d::AssetProfile::General;
};

inline MuseumAssetRegistration museumAssetRegistration(ModelId model) {
    using namespace soft3d;
    switch(model) {
    case ModelId::Rx78:
        return {"rx78",AssetDeterministicOrder|AssetAllSolid|AssetNearPlaneEnvelope|
                       AssetPrecomputedCullPlanes|AssetFastIndexedCommands,AssetProfile::SolidStatic};
    case ModelId::CharZaku:return {"char_zaku",AssetDeterministicOrder|AssetAllSolid,AssetProfile::SolidStatic};
    case ModelId::NuGundam:return {"nu_gundam",AssetDeterministicOrder|AssetAllSolid,AssetProfile::SolidStatic};
    case ModelId::Sazabi:return {"sazabi",AssetDeterministicOrder|AssetAllSolid,AssetProfile::SolidStatic};
    case ModelId::StrikeGundam:return {"strike_gundam",AssetDeterministicOrder|AssetAllSolid,AssetProfile::SolidStatic};
    case ModelId::DestinyGundam:return {"destiny_gundam",AssetDeterministicOrder|AssetAllSolid,AssetProfile::SolidStatic};
    }
    return {};
}

} // namespace gundam_museum
