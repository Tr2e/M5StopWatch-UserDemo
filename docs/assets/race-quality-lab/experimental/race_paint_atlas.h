#pragma once
#include "car_paint.h"
#include <array>

namespace lets_and_go {
// Bounded, race-only RGB565 bake of the existing UV materials. Key includes
// authored base color and panel lighting; geometry/UV/occlusion stay unchanged.
template<unsigned Side,unsigned Capacity> class CarPaintAtlas {
public:
    static constexpr unsigned kSide=Side,kCapacity=Capacity;
    static_assert(Capacity<=64 && Side>0,"bounded atlas hash/storage");
    using Texture=std::array<uint16_t,kSide*kSide>;
    void clear() { _slots.fill(0);_count=0; }
    unsigned count() const { return _count; }
    const uint16_t* find(CarPaint paint,uint16_t base,uint8_t light) const {
        if(paint==CarPaint::Solid)return nullptr;
        const uint32_t key=makeKey(paint,base,light);
        auto slot=hash(key);
        while(_slots[slot]) {
            const auto i=_slots[slot]-1;
            if(_keys[i]==key)return _pixels[i].data();
            slot=(slot+1)&127u;
        }
        return nullptr;
    }
    bool add(CarPaint paint,uint16_t base,uint8_t light) {
        if(paint==CarPaint::Solid || find(paint,base,light))return true;
        if(_count==kCapacity)return false; // Missing entries use original shader.
        const uint32_t key=makeKey(paint,base,light);
        auto slot=hash(key);while(_slots[slot])slot=(slot+1)&127u;
        _keys[_count]=key;
        auto& pixels=_pixels[_count];
        for(unsigned y=0;y<kSide;++y)for(unsigned x=0;x<kSide;++x) {
            const auto color=carPaintColor(paint,base,(x+.5f)/kSide,(y+.5f)/kSide);
            pixels[y*kSide+x]=light==255 ? color : carTint(color,light/255.f);
        }
        _slots[slot]=uint8_t(++_count);
        return true;
    }
    static uint16_t sample(const uint16_t* pixels,float u,float v) {
        const auto x=unsigned(std::clamp(u*float(kSide),0.f,float(kSide-1)));
        const auto y=unsigned(std::clamp(v*float(kSide),0.f,float(kSide-1)));
        return pixels[y*kSide+x];
    }
private:
    static uint32_t makeKey(CarPaint p,uint16_t base,uint8_t light) {
        return uint32_t(p)|(uint32_t(base)<<8)|(uint32_t(light)<<24);
    }
    static unsigned hash(uint32_t key) {return (key*2654435761u)>>25;}
    std::array<uint8_t,128> _slots{};
    std::array<uint32_t,kCapacity> _keys{};
    std::array<Texture,kCapacity> _pixels;
    unsigned _count=0;
};
using RacePaintAtlas=CarPaintAtlas<32,64>;
using PlayerPaintAtlas=CarPaintAtlas<64,16>;
} // namespace lets_and_go
