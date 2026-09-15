#pragma once
#include "rx78.h"

namespace gundam_museum {
struct EyeSocketAssembly {
    struct Range { size_t begin=0,end=0; };
    Range rim,walls,lens;
};

// A real aperture, not black and colored covers stacked on the mask.
// The caller owns the silhouette and its connection to brow/cheek/mask.
// Opening is a convex planar polygon; front is +Z. Four-corner callers keep
// their exact original topology, while RX-78 retains its five-corner contour.
template<class Builder,size_t N>
void buildEyeSocket(Builder& b,const std::array<Point,N>& opening,float depth,
                    uint16_t rimColor,uint16_t wallColor,uint16_t lensColor,
                    EyeSocketAssembly* assembly=nullptr){
    static_assert(N>=3,"An eye opening needs at least three corners");
    Point c{};for(auto p:opening){c.x+=p.x*(1.f/N);c.y+=p.y*(1.f/N);c.z+=p.z*(1.f/N);}
    const auto inset=[&](Point p,float scale,float recess){return Point{
        c.x+(p.x-c.x)*scale,c.y+(p.y-c.y)*scale,c.z+(p.z-c.z)*scale-recess};};
    std::array<Point,N> lip{},back{},glass{},bevel{};
    for(size_t i=0;i<N;++i){lip[i]=inset(opening[i],.96f,.005f);
        back[i]=inset(opening[i],.94f,depth);
        glass[i]=inset(opening[i],.88f,depth-.008f);
        bevel[i]=inset(opening[i],.89f,depth);
    }
    if(assembly)assembly->rim.begin=b.m.count;
    for(size_t i=0;i<N;++i){size_t j=(i+1)%N;b.face(opening[i],opening[j],lip[j],lip[i],rimColor,{0,0,1},true);}
    if(assembly){assembly->rim.end=b.m.count;assembly->walls.begin=b.m.count;}
    for(size_t i=0;i<N;++i){size_t j=(i+1)%N;
        b.face(lip[i],lip[j],back[j],back[i],wallColor,{c.x-lip[i].x,c.y-lip[i].y,0},true);
        b.face(back[i],back[j],bevel[j],bevel[i],wallColor,{0,0,1},true);
    }
    if(assembly){assembly->walls.end=b.m.count;assembly->lens.begin=b.m.count;}
    if constexpr(N==4)b.face(glass[0],glass[1],glass[2],glass[3],lensColor,{0,0,1},true);
    else for(size_t i=1;i+1<N;++i)b.face(glass[0],glass[i],glass[i+1],glass[i+1],lensColor,{0,0,1},true);
    for(size_t i=0;i<N;++i){size_t j=(i+1)%N;b.face(glass[i],glass[j],bevel[j],bevel[i],lensColor,{0,0,1},true);}
    if(assembly)assembly->lens.end=b.m.count;
}
} // namespace gundam_museum
