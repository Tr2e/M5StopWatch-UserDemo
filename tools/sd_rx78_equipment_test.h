#pragma once
#include <cassert>
#include <cmath>
#include <iostream>

namespace sd_equipment_check {
using namespace gundam_museum;
// Proper surface crossings only: coplanar/tangent contact and closed-volume
// containment require separate inspection. No raster or screen-space inference.
inline bool crossing(Point a,Point b,Point p,Point q,Point r,Point& at){
    const auto direction=subtract(b,a),e1=subtract(q,p),e2=subtract(r,p);
    const auto h=cross(direction,e2);const float den=dot(e1,h);
    if(std::abs(den)<1e-8f)return false;
    const float f=1/den;const auto s=subtract(a,p);const float u=f*dot(s,h);
    if(u<1e-5f || u>1-1e-5f)return false;
    const auto v=cross(s,e1);const float w=f*dot(direction,v),t=f*dot(e2,v);
    if(w<1e-5f || u+w>1-1e-5f || t<1e-5f || t>1-1e-5f)return false;
    at={a.x+t*direction.x,a.y+t*direction.y,a.z+t*direction.z};return true;
}
struct Result {unsigned unintended=0,mountPairs=0;};
inline Result inspect(const Mesh& mesh){
    Result result;
    for(size_t i=0;i<mesh.count;++i){
        const auto equipment=mesh.parts[i];
        if(equipment!=Part::Rifle && equipment!=Part::Shield)continue;
        for(size_t j=0;j<mesh.count;++j){
            const auto body=mesh.parts[j];
            if(body==equipment || (body==Part::Rifle && equipment==Part::Shield))continue;
            const auto& a=mesh.panels[i].point;const auto& b=mesh.panels[j].point;
            bool touched=false,permitted=true;
            for(int ta=0;ta<2;++ta)for(int tb=0;tb<2;++tb){
                Point x[]={a[0],a[ta+1],a[ta+2]},y[]={b[0],b[tb+1],b[tb+2]};
                for(int e=0;e<3;++e)for(int direction=0;direction<2;++direction){
                    Point at;const bool hit=direction?crossing(y[e],y[(e+1)%3],x[0],x[1],x[2],at):
                        crossing(x[e],x[(e+1)%3],y[0],y[1],y[2],at);
                    if(!hit)continue;
                    touched=true;
                    // Only the small B16 peg insertion region may cross the
                    // simplified forearm wall (no hidden internal socket mesh).
                    // Never exempt the shield/arm pair as a whole.
                    permitted &= equipment==Part::Shield && body==Part::Arms &&
                        at.x>.92f && at.x<.97f && at.y>1.20f && at.y<1.31f && at.z>.145f && at.z<.215f;
                }
            }
            if(touched){if(permitted)++result.mountPairs;else ++result.unintended;}
        }
    }
    return result;
}
inline void check(const Mesh& mesh){
    const auto r=inspect(mesh);
    std::cout<<"sd_equipment unintended_surface_pairs="<<r.unintended
             <<" permitted_peg_pairs="<<r.mountPairs<<'\n';
    assert(r.unintended==0);
    assert(r.mountPairs>0 && r.mountPairs<=24); // attached, bounded insertion
}
} // namespace sd_equipment_check
