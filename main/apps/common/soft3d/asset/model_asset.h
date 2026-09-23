#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace soft3d {

template<class T> struct Span {
    const T* data=nullptr;
    std::size_t size=0;
    constexpr const T& operator[](std::size_t index) const{return data[index];}
    constexpr const T* begin() const{return data;}
    constexpr const T* end() const{return data+size;}
    constexpr bool empty() const{return size==0;}
};

struct Vec3 {float x=0,y=0,z=0;};
struct Bounds {Vec3 minimum{},maximum{},center{};float radius=0,groundY=0;};

enum class PrimitiveTopology : uint8_t {Triangle=3,Quad=4};
enum class MaterialKind : uint8_t {Solid,Program};
enum class AssetProfile : uint8_t {SolidStatic,SolidRigid,General};

enum PrimitiveFlags : uint8_t {
    PrimitiveTwoSided=1u<<0,
    PrimitiveNearClipRisk=1u<<1
};

struct Material {
    uint16_t color=0;
    MaterialKind kind=MaterialKind::Solid;
    uint8_t program=0;
    uint8_t light=255;
    uint8_t flags=0;
};

// A triangle repeats index[2] in index[3]. rigidPart is an explicit rigid
// skeleton binding; zero means the root. It is never overloaded with a
// vehicle- or model-specific concept such as a wheel.
struct Primitive {
    std::array<uint16_t,4> index{};
    uint16_t material=0;
    uint16_t rigidPart=0;
    PrimitiveTopology topology=PrimitiveTopology::Quad;
    uint8_t flags=0;
};

struct LodRange {
    uint32_t firstPrimitive=0;
    uint32_t primitiveCount=0;
    float maximumScreenRadius=0;
};

struct Bone {int16_t parent=-1;uint16_t nameHash=0;Vec3 pivot{};};
struct BoneTransform {
    // Row-major rigid 3x4 transform. Identity is the all-zero default plus
    // ones on the diagonal supplied by callers that animate a skeleton.
    std::array<float,12> value{};
};

enum AssetFlags : uint32_t {
    AssetDeterministicOrder=1u<<0,
    AssetAllSolid=1u<<1,
    AssetNearPlaneEnvelope=1u<<2,
    AssetRigidSkeleton=1u<<3
    ,AssetPrecomputedCullPlanes=1u<<4
    ,AssetFastIndexedCommands=1u<<5
};

struct AssetMemoryInfo {
    uint32_t residentBytes=0;
    uint32_t maximumProjectionBytes=0;
    uint32_t maximumCommandBytes=0;
    uint16_t maximumVisiblePrimitives=0;
};

struct ModelAsset {
    const char* name=nullptr;
    Span<Vec3> positions{};
    Span<Vec3> normals{};      // One entry per primitive.
    Span<Vec3> anchors{};      // One entry per primitive.
    Span<Primitive> primitives{};
    Span<Material> materials{};
    Span<uint16_t> orderedPrimitiveIndices{};
    Span<LodRange> lods{};
    Span<Bone> skeleton{};
    Bounds bounds{};
    AssetMemoryInfo memory{};
    uint32_t flags=AssetDeterministicOrder;
    AssetProfile profile=AssetProfile::SolidStatic;
};

struct Transform {
    std::array<float,12> value{{1,0,0,0,0,1,0,0,0,0,1,0}};
};

struct ModelInstance {
    const ModelAsset* asset=nullptr;
    Transform world{};
    uint16_t lod=0;
    uint16_t visibilityMask=0xffffu;
    Span<BoneTransform> pose{};
};

struct SceneView {Span<ModelInstance> instances{};};

enum class AssetError : uint8_t {
    None,MissingData,NonFinitePosition,IndexOutOfRange,MaterialOutOfRange,
    InvalidTopology,InvalidLod,InvalidSkeleton,BoundsMismatch
};

inline AssetError validate(const ModelAsset& asset) {
    if(!asset.name || asset.positions.empty() || asset.primitives.empty() ||
       asset.materials.empty() || asset.lods.empty())return AssetError::MissingData;
    for(const auto& point:asset.positions)
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))
            return AssetError::NonFinitePosition;
    for(const auto& primitive:asset.primitives) {
        const unsigned count=unsigned(primitive.topology);
        if(count!=3 && count!=4)return AssetError::InvalidTopology;
        if(primitive.material>=asset.materials.size)return AssetError::MaterialOutOfRange;
        if(primitive.rigidPart>=asset.skeleton.size && primitive.rigidPart!=0)
            return AssetError::InvalidSkeleton;
        for(unsigned i=0;i<count;++i)if(primitive.index[i]>=asset.positions.size)
            return AssetError::IndexOutOfRange;
        if(count==3 && primitive.index[3]!=primitive.index[2])return AssetError::InvalidTopology;
    }
    for(const auto& lod:asset.lods)
        if(lod.firstPrimitive>asset.primitives.size ||
           lod.primitiveCount>asset.primitives.size-lod.firstPrimitive)
            return AssetError::InvalidLod;
    const auto& b=asset.bounds;
    for(const auto& p:asset.positions)
        if(p.x<b.minimum.x||p.y<b.minimum.y||p.z<b.minimum.z||
           p.x>b.maximum.x||p.y>b.maximum.y||p.z>b.maximum.z)
            return AssetError::BoundsMismatch;
    return AssetError::None;
}

static_assert(std::is_trivially_copyable_v<ModelAsset>);
static_assert(sizeof(Primitive)==14,"runtime primitive layout changed");

} // namespace soft3d
