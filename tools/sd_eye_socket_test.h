#pragma once
#include "../main/apps/app_gundam_museum/model/sd_eye_socket.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

namespace sd_eye_check {
using namespace gundam_museum;
using Range=EyeSocketAssembly::Range;
inline float recess(const Mesh& m,Range lens,float z0,float slope){
    float depth=100;
    for(size_t i=lens.begin;i<lens.end;++i)for(auto p:m.panels[i].point)
        depth=std::min(depth,z0-slope*std::abs(p.x)-p.z);
    return depth;
}
inline unsigned sharedVertices(const Mesh& m,Range a,Range b){
    unsigned count=0;
    for(size_t i=a.begin;i<a.end;++i)for(auto p:m.panels[i].point){bool found=false;
        for(size_t j=b.begin;j<b.end;++j)for(auto q:m.panels[j].point)
            found|=std::abs(p.x-q.x)+std::abs(p.y-q.y)+std::abs(p.z-q.z)<1e-6f;
        count+=found;
    }return count;
}
inline float frontSurface(const Mesh& m,float x,float y){
    float z=-100;
    for(size_t i=0;i<m.count;++i)if(m.parts[i]==Part::Head){auto a=m.panels[i].point;
        for(int t=0;t<2;++t){auto p=a[0],q=a[t+1],r=a[t+2];
            float den=(q.y-r.y)*(p.x-r.x)+(r.x-q.x)*(p.y-r.y);if(std::abs(den)<1e-8f)continue;
            float u=((q.y-r.y)*(x-r.x)+(r.x-q.x)*(y-r.y))/den,v=((r.y-p.y)*(x-r.x)+(p.x-r.x)*(y-r.y))/den;
            if(u>=0&&v>=0&&u+v<=1)z=std::max(z,u*p.z+v*q.z+(1-u-v)*r.z);
        }
    }return z;
}
inline void check(const Mesh& m,const std::array<EyeSocketAssembly,2>& eyes,float z0,float slope){
    for(int side=0;side<2;++side){auto a=eyes[side];assert(a.rim.end>a.rim.begin&&a.walls.end>a.walls.begin&&a.lens.end>a.lens.begin);
        const auto depth=recess(m,a.lens,z0,slope);
        assert(depth>.05f&&depth<.09f);
        assert(sharedVertices(m,a.rim,a.walls)>=4&&sharedVertices(m,a.walls,a.lens)>=4);
        float xl=100,xh=-100,yl=100,yh=-100;Point c{};
        // First lens panel is the large planar optical surface, not its bevel.
        for(auto p:m.panels[a.lens.begin].point){c.x+=p.x*.25f;c.y+=p.y*.25f;c.z+=p.z*.25f;
            xl=std::min(xl,p.x);xh=std::max(xh,p.x);yl=std::min(yl,p.y);yh=std::max(yh,p.y);}
        const float aspect=(yh-yl)/(xh-xl);assert(aspect>.48f&&aspect<.8f);
        // Sample the center and four inset corners against ALL head surfaces;
        // a leftover mask/cap over the opening must be caught.
        assert(std::abs(frontSurface(m,c.x,c.y)-c.z)<.0001f);
        for(float inset:{.55f,.90f})for(auto p:m.panels[a.lens.begin].point){auto q=Point{c.x+(p.x-c.x)*inset,c.y+(p.y-c.y)*inset,c.z+(p.z-c.z)*inset};
            if(std::abs(frontSurface(m,q.x,q.y)-q.z)>=.0001f)
                std::cerr<<"eye occlusion side="<<side<<" x="<<q.x<<" y="<<q.y<<" lens_z="<<q.z<<" front_z="<<frontSurface(m,q.x,q.y)<<'\n';
            assert(std::abs(frontSurface(m,q.x,q.y)-q.z)<.0001f);
        }
        std::cout<<"eye_socket side="<<side<<" recess="<<depth<<" large_eye_aspect="<<aspect<<'\n';
        auto bad=std::make_unique<Mesh>(m);
        for(size_t i=a.lens.begin;i<a.lens.end;++i)for(auto& p:bad->panels[i].point)p.z+=.09f;
        assert(recess(*bad,a.lens,z0,slope)<0&&sharedVertices(*bad,a.walls,a.lens)==0);
        *bad=m;
        auto& cap=bad->panels[bad->count];cap=m.panels[a.lens.begin];for(auto& p:cap.point)p.z+=.025f;
        bad->parts[bad->count++]=Part::Head;
        assert(frontSurface(*bad,c.x,c.y)-c.z>.02f);
    }
}
} // namespace sd_eye_check
