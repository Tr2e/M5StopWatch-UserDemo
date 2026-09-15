#pragma once
#include "rx78.h"

namespace gundam_museum {
struct EyeSocketAssembly {
    struct Range { size_t begin=0,end=0; };
    Range rim,walls,lens;
};

// A real aperture, not black and colored covers stacked on the mask.
// The caller owns the silhouette and its connection to brow/cheek/mask.
// All four opening points lie on a plane; front is +Z.
template<class Builder>
void buildEyeSocket(Builder& b,const std::array<Point,4>& opening,float depth,
                    uint16_t rimColor,uint16_t wallColor,uint16_t lensColor,
                    EyeSocketAssembly* assembly=nullptr){
    Point c{};for(auto p:opening){c.x+=p.x*.25f;c.y+=p.y*.25f;c.z+=p.z*.25f;}
    const auto inset=[&](Point p,float scale,float recess){return Point{
        c.x+(p.x-c.x)*scale,c.y+(p.y-c.y)*scale,c.z+(p.z-c.z)*scale-recess};};
    std::array<Point,4> lip{},back{},glass{},bevel{};
    for(int i=0;i<4;++i){lip[i]=inset(opening[i],.96f,.005f);
        back[i]=inset(opening[i],.94f,depth);
        glass[i]=inset(opening[i],.88f,depth-.008f);
        bevel[i]=inset(opening[i],.89f,depth);
    }
    if(assembly)assembly->rim.begin=b.m.count;
    for(int i=0;i<4;++i){int j=(i+1)%4;b.face(opening[i],opening[j],lip[j],lip[i],rimColor,{0,0,1},true);}
    if(assembly){assembly->rim.end=b.m.count;assembly->walls.begin=b.m.count;}
    for(int i=0;i<4;++i){int j=(i+1)%4;
        b.face(lip[i],lip[j],back[j],back[i],wallColor,{c.x-lip[i].x,c.y-lip[i].y,0},true);
        b.face(back[i],back[j],bevel[j],bevel[i],wallColor,{0,0,1},true);
    }
    if(assembly){assembly->walls.end=b.m.count;assembly->lens.begin=b.m.count;}
    b.face(glass[0],glass[1],glass[2],glass[3],lensColor,{0,0,1},true);
    for(int i=0;i<4;++i){int j=(i+1)%4;b.face(glass[i],glass[j],bevel[j],bevel[i],lensColor,{0,0,1},true);}
    if(assembly)assembly->lens.end=b.m.count;
}
} // namespace gundam_museum
