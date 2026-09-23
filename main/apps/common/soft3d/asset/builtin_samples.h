#pragma once

#include "model_asset_storage.h"

namespace soft3d::samples {

struct SamplePanel {
    std::array<Vec3,4> points{};
    uint16_t color=0xffff;
    uint16_t bone=0;
    bool twoSided=false;
};

struct SamplePanelView {
    const SamplePanel* value=nullptr;
    Vec3 point(std::size_t index) const{return value->points[index];}
    uint16_t rigidPart() const{return value->bone;}
    Material material() const{return {value->color,MaterialKind::Solid,0,255,0};}
    uint8_t flags() const{return value->twoSided?PrimitiveTwoSided:0;}
    Vec3 anchor() const{return value->points[0];}
    Vec3 normal() const {
        const auto a=value->points[0],b=value->points[1],c=value->points[2];
        const Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
        const Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        const float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
        return length>0?Vec3{n.x/length,n.y/length,n.z/length}:Vec3{};
    }
};

// A small non-character static scene: six quad faces and one material.
inline auto makeCrate() {
    using Storage=ModelAssetStorage<8,6,1>;
    struct Result {Storage storage;AssetError error=AssetError::None;};
    Result result{};
    constexpr float l=-.5f,r=.5f,b=0,t=1,k=-.5f,f=.5f;
    const std::array<SamplePanel,6> panels{{
        {{{{l,b,f},{r,b,f},{r,t,f},{l,t,f}}},0xc386},
        {{{{r,b,k},{l,b,k},{l,t,k},{r,t,k}}},0xc386},
        {{{{l,b,k},{l,b,f},{l,t,f},{l,t,k}}},0xc386},
        {{{{r,b,f},{r,b,k},{r,t,k},{r,t,f}}},0xc386},
        {{{{l,t,f},{r,t,f},{r,t,k},{l,t,k}}},0xc386},
        {{{{l,b,k},{r,b,k},{r,b,f},{l,b,f}}},0xc386}
    }};
    result.error=buildPanelAsset(result.storage,panels.size(),[&](std::size_t i) {
        return SamplePanelView{&panels[i]};
    },{"crate",AssetDeterministicOrder|AssetAllSolid,AssetProfile::SolidStatic,0});
    return result;
}

// A medium rigid benchmark made of eight linked boxes. Each box is bound to
// one explicit bone; no wheel/business field is involved.
inline auto makeRigidChain() {
    using Storage=ModelAssetStorage<64,48,8,1,8>;
    struct Result {Storage storage;AssetError error=AssetError::None;};
    Result result{};std::array<SamplePanel,48> panels{};std::size_t count=0;
    for(uint16_t bone=0;bone<8;++bone) {
        const float x=float(bone)*.32f-1.12f,l=x-.14f,r=x+.14f,b=.15f,t=.75f,k=-.14f,f=.14f;
        const uint16_t color=uint16_t(0x39e7u+bone*0x0821u);
        const std::array<std::array<Vec3,4>,6> faces{{
            {{{l,b,f},{r,b,f},{r,t,f},{l,t,f}}},{{{r,b,k},{l,b,k},{l,t,k},{r,t,k}}},
            {{{l,b,k},{l,b,f},{l,t,f},{l,t,k}}},{{{r,b,f},{r,b,k},{r,t,k},{r,t,f}}},
            {{{l,t,f},{r,t,f},{r,t,k},{l,t,k}}},{{{l,b,k},{r,b,k},{r,b,f},{l,b,f}}}
        }};
        for(const auto& face:faces)panels[count++]={face,color,bone,false};
    }
    result.storage.boneCount=8;
    for(unsigned i=0;i<8;++i)result.storage.bones[i]={int16_t(i?i-1:-1),uint16_t(0x100u+i),{float(i)*.32f-1.12f,.45f,0}};
    result.error=buildPanelAsset(result.storage,count,[&](std::size_t i) {
        return SamplePanelView{&panels[i]};
    },{"rigid_chain",AssetDeterministicOrder|AssetAllSolid|AssetRigidSkeleton,AssetProfile::SolidRigid,.15f});
    // buildPanelAsset clears storage, so install the skeleton after geometry
    // conversion and reseal the immutable descriptor.
    result.storage.boneCount=8;
    for(unsigned i=0;i<8;++i)result.storage.bones[i]={int16_t(i?i-1:-1),uint16_t(0x100u+i),{float(i)*.32f-1.12f,.45f,0}};
    result.storage.seal("rigid_chain",result.storage.asset.bounds,
        AssetDeterministicOrder|AssetAllSolid|AssetRigidSkeleton,AssetProfile::SolidRigid);
    result.error=validate(result.storage.asset);
    return result;
}

} // namespace soft3d::samples
