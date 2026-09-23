#pragma once

#include "../asset/model_asset.h"
#include <array>
#include <cstring>
#include <memory>
#include <new>

namespace soft3d {

struct VertexKey {Vec3 position{};uint16_t rigidPart=0;};
struct VertexIndexResult {std::size_t count=0;bool usedHashTable=false;};

inline uint32_t vertexKeyHash(const VertexKey& key) {
    uint32_t hash=2166136261u;
    for(float value:{key.position.x,key.position.y,key.position.z}) {
        uint32_t bits=0;
        if(value!=0)std::memcpy(&bits,&value,sizeof(bits));
        hash=(hash^bits)*16777619u;
    }
    return (hash^key.rigidPart)*16777619u;
}

inline bool sameVertexKey(const VertexKey& left,const VertexKey& right) {
    return left.position.x==right.position.x && left.position.y==right.position.y &&
           left.position.z==right.position.z && left.rigidPart==right.rigidPart;
}

// Shared deterministic FNV/open-addressing indexer used by Museum, Racer and
// asset conversion. HashSlots must be a power of two and at least twice the
// expected unique-vertex count. Allocation failure deliberately falls back to
// one vertex per corner, preserving correctness and draw order.
template<std::size_t HashSlots,class Accessor>
VertexIndexResult indexSharedVertices(std::size_t cornerCount,Accessor vertex,
                                      uint16_t* cornerIndex,uint16_t* representatives=nullptr,
                                      std::array<uint16_t,HashSlots>* callerSlots=nullptr) {
    static_assert(HashSlots && !(HashSlots&(HashSlots-1)),"hash table must be power of two");
    std::unique_ptr<std::array<uint16_t,HashSlots>> owned;
    auto* slots=callerSlots;
    if(!slots) {
        owned.reset(new(std::nothrow) std::array<uint16_t,HashSlots>{});
        slots=owned.get();
    } else slots->fill(0);
    std::size_t count=0;
    for(std::size_t corner=0;corner<cornerCount;++corner) {
        if(!slots) {
            cornerIndex[corner]=uint16_t(count);
            if(representatives)representatives[count]=uint16_t(corner);
            ++count;continue;
        }
        const auto key=vertex(corner);
        std::size_t slot=vertexKeyHash(key)&(HashSlots-1);
        while((*slots)[slot]) {
            const auto representative=representatives
                ? representatives[(*slots)[slot]-1]
                : uint16_t((*slots)[slot]-1);
            // Without a representative array, slots stores corner+1. With
            // one, slots stores unique-index+1 to match Racer's existing ABI.
            const auto existing=representatives?vertex(representative):vertex(representative);
            if(sameVertexKey(key,existing))break;
            slot=(slot+1)&(HashSlots-1);
        }
        if(!(*slots)[slot]) {
            if(representatives) {
                representatives[count]=uint16_t(corner);
                (*slots)[slot]=uint16_t(count+1);
            } else (*slots)[slot]=uint16_t(corner+1);
            cornerIndex[corner]=uint16_t(count++);
        } else if(representatives)cornerIndex[corner]=uint16_t((*slots)[slot]-1);
        else {
            const auto representative=std::size_t((*slots)[slot]-1);
            cornerIndex[corner]=cornerIndex[representative];
        }
    }
    return {count,slots!=nullptr};
}

} // namespace soft3d
