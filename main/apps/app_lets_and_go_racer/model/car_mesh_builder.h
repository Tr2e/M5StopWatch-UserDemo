#pragma once
#include "car_display_mesh.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

// Production mesh parts, shared by all eight authored car bodies.
// This is the StopWatch adapter, not an engine-independent mesh format.
// Coordinates, hardware dimensions and paint IDs follow car_display_mesh.h.
// Caller supplies bounded storage; stations must contain >=2 rows with
// increasing z, and segments must be 12, 18 or 24. See the modeling toolkit.
namespace lets_and_go::mesh_parts {
constexpr uint16_t white=0xf7be, graphite=0x2946, tire=0x18c3;
constexpr uint16_t blue=0x3275,red=0xc9a7,silver=0xb5d6,glass=0x2128;
inline uint16_t shade(uint16_t c,float f) {
    const int r=std::clamp(int(((c>>11)&31)*f),0,31);
    const int g=std::clamp(int(((c>>5)&63)*f),0,63);
    const int b=std::clamp(int((c&31)*f),0,31);
    return uint16_t((r<<11)|(g<<5)|b);
}
// The molded bodies have authored chines and separate open wheel cowls.
// These stations describe actual edges, not the radius of a generic oval hull.
struct ChineStation {float z,width,sill,edge,deck;};
struct CowlStation {float z,inner,outer,edge,crown;};
struct PanelSpan {
    CarPanel* data;
    std::size_t capacity;
    std::size_t size() const {return capacity;}
    CarPanel& operator[](std::size_t i) {return data[i];}
};
struct MeshWriter {
    PanelSpan panels;
    std::size_t count=0;
    bool overflowed=false;
};
class Builder {
public:
    MeshWriter& mesh;
    int segments;
    CarPart part=CarPart::Unspecified;
    void quad(CarPoint a,CarPoint b,CarPoint c,CarPoint d,uint16_t color,
              CarPaint paint=CarPaint::Solid,float u0=0,float u1=1,float v0=0,float v1=1,
              uint8_t wheel=0) {
        if(mesh.count==mesh.panels.size()) {mesh.overflowed=true;return;}
        auto& p=mesh.panels[mesh.count++];
        p={{{a,b,c,d}},color,part,wheel,0xffffu,paint,
           uint8_t(std::clamp(u0,0.f,1.f)*255),uint8_t(std::clamp(u1,0.f,1.f)*255),
           uint8_t(std::clamp(v0,0.f,1.f)*255),uint8_t(std::clamp(v1,0.f,1.f)*255)};
    }
    void box(float x0,float x1,float y0,float y1,float z0,float z1,uint16_t color,
             CarPaint paint=CarPaint::Solid) {
        quad({x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1},color,paint);
        quad({x0,y0,z0},{x0,y1,z0},{x0,y1,z1},{x0,y0,z1},shade(color,.7f));
        quad({x1,y0,z1},{x1,y1,z1},{x1,y1,z0},{x1,y0,z0},shade(color,.8f));
        quad({x0,y0,z1},{x0,y1,z1},{x1,y1,z1},{x1,y0,z1},shade(color,.82f));
        quad({x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z0},shade(color,.65f));
    }
    void chine(std::initializer_list<ChineStation> stations,uint16_t color,
               CarPaint top,CarPaint sidePaint=CarPaint::Solid) {
        const float start=stations.begin()->z,span=(stations.end()-1)->z-start;
        const auto row=[](ChineStation s,int column) {
            constexpr float x[]={-1,-.66f,0,.66f,1};
            return CarPoint{s.width*x[column],column==0 || column==4 ? s.edge : s.deck,s.z};
        };
        for(auto s=stations.begin()+1;s!=stations.end();++s) {
            const auto a=*(s-1),b=*s;
            for(int c=0;c<4;++c) {
                quad(row(a,c),row(a,c+1),row(b,c+1),row(b,c),color,top,
                     c==0 ? 0.f : c==1 ? .17f : c==2 ? .5f : .83f,
                     c==0 ? .17f : c==1 ? .5f : c==2 ? .83f : 1.f,
                     (a.z-start)/span,(b.z-start)/span);
                if(mesh.count && !mesh.overflowed)mesh.panels[mesh.count-1].light=c==0 || c==3 ? 223 : 255;
            }
            for(float side : {-1.f,1.f})
                quad({side*a.width,a.sill,a.z},{side*a.width,a.edge,a.z},
                     {side*b.width,b.edge,b.z},{side*b.width,b.sill,b.z},color,sidePaint,
                     0,1,(a.z-start)/span,(b.z-start)/span);
        }
        for(const auto s : {*stations.begin(),*(stations.end()-1)})
            for(int c=0;c<4;++c) {
                const auto a=row(s,c),b=row(s,c+1);
                quad({a.x,s.sill,s.z},a,b,{b.x,s.sill,s.z},shade(color,.77f));
            }
    }
    void cowl(float side,std::initializer_list<CowlStation> stations,uint16_t color,
              CarPaint paint,CarPaint sidePaint=CarPaint::Solid,float outerLip=.042f) {
        const float start=stations.begin()->z,span=(stations.end()-1)->z-start;
        const auto point=[side](CowlStation s,int c) {
            constexpr float u[]={0,.16f,.63f,1};
            return CarPoint{side*(s.inner+(s.outer-s.inner)*u[c]),
                            c==0 || c==3 ? s.edge : s.crown,s.z};
        };
        for(auto it=stations.begin()+1;it!=stations.end();++it) {
            const auto a=*(it-1),b=*it;
            for(int c=0;c<3;++c) {
                constexpr float u[]={0,.16f,.63f,1};
                quad(point(a,c),point(a,c+1),point(b,c+1),point(b,c),color,paint,
                     u[c],u[c+1],(a.z-start)/span,(b.z-start)/span);
            }
            // Hollow shell lips, not a solid box extending through the tire.
            for(int c : {0,3}) {
                auto p=point(a,c),q=point(b,c);
                const float depth=c==3 ? outerLip : .020f;
                quad({p.x+side*(c==3 ? .012f : 0),p.y-depth,p.z},p,q,
                     {q.x+side*(c==3 ? .012f : 0),q.y-depth,q.z},color,sidePaint,
                     0,1,(a.z-start)/span,(b.z-start)/span);
            }
        }
        for(const auto s : {*stations.begin(),*(stations.end()-1)})
            for(int c=0;c<3;++c) {
                auto p=point(s,c),q=point(s,c+1);
                quad({p.x,p.y-.020f,p.z},p,q,{q.x,q.y-.020f,q.z},shade(color,.78f));
            }
    }
    void tube(CarPoint from,CarPoint to,float radius,uint16_t color) {
        // Open ends enter the adjoining body/pod; no hidden end-cap fans.
        const float dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z;
        const float length=std::sqrt(dx*dx+dy*dy+dz*dz);
        if(length<1e-6f)return;
        const CarPoint d{dx/length,dy/length,dz/length};
        const CarPoint ref=std::abs(d.y)<.9f ? CarPoint{0,1,0} : CarPoint{1,0,0};
        CarPoint u{d.y*ref.z-d.z*ref.y,d.z*ref.x-d.x*ref.z,d.x*ref.y-d.y*ref.x};
        const float ul=std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z);
        u={u.x/ul,u.y/ul,u.z/ul};
        const CarPoint v{d.y*u.z-d.z*u.y,d.z*u.x-d.x*u.z,d.x*u.y-d.y*u.x};
        const auto point=[&](CarPoint p,float a) {
            const float c=radius*std::cos(a),s=radius*std::sin(a);
            return CarPoint{p.x+u.x*c+v.x*s,p.y+u.y*c+v.y*s,p.z+u.z*c+v.z*s};
        };
        const int n=segments==24 ? 10 : segments==18 ? 8 : 6;
        for(int i=0;i<n;++i) {
            const float a=i*6.2831853f/n,b=(i+1)*6.2831853f/n;
            quad(point(from,a),point(from,b),point(to,b),point(to,a),shade(color,.75f+.25f*std::abs(std::cos(a))));
        }
    }
    void wheel(float side,float axle,uint16_t hub,bool cap,uint8_t index,bool broad=false,bool dish=false,int spokeCount=5) {
        constexpr float tau=6.2831853f;
        const auto p=[&](float x,float r,float a) {
            return CarPoint{side*x,kModelWheelRadius+std::sin(a)*r,axle+std::cos(a)*r};
        };
        for(int i=0;i<segments;++i) {
            const float a=i*tau/segments,b=(i+1)*tau/segments;
            const uint16_t tread=shade(tire,1+.3f*std::sin((a+b)*.5f));
            quad(p(.365f,.145f,a),p(.39f,.175f,a),p(.39f,.175f,b),p(.365f,.145f,b),tire);
            quad(p(.39f,.175f,a),p(.535f,.175f,a),p(.535f,.175f,b),p(.39f,.175f,b),tread);
            quad(p(.535f,.175f,a),p(.56f,.15f,a),p(.56f,.15f,b),p(.535f,.175f,b),tire);
            quad(p(.56f,.15f,a),p(.562f,.119f,a),p(.562f,.119f,b),p(.56f,.15f,b),tire);
            quad(p(.563f,.119f,a),p(.564f,.104f,a),p(.564f,.104f,b),p(.563f,.119f,b),hub);
            const float diskX=dish ? .545f : .565f,diskR=dish ? .085f : .104f;
            quad(p(diskX,0,a),p(diskX,diskR,a),p(diskX,diskR,b),p(diskX,0,a),
                 cap ? 0x5acb : shade(hub,dish ? .58f : .25f));
            if(dish)quad(p(.564f,.104f,a),p(.545f,.085f,a),p(.545f,.085f,b),p(.564f,.104f,b),hub);
            // Inner side is closed too; the old open ring vanished from reverse views.
            quad(p(.365f,0,a),p(.365f,.145f,b),p(.365f,.145f,a),p(.365f,0,a),tire);
        }
        if(!cap && !dish) for(int s=0;s<spokeCount;++s) {
            const float a=s*tau/spokeCount;
            if(spokeCount==6) {
                quad(p(.567f,.025f,a-.4f),p(.567f,.07f,a-.08f),
                     p(.567f,.07f,a+.36f),p(.567f,.025f,a+.4f),hub,
                     CarPaint::Solid,0,1,0,1,index);
                quad(p(.567f,.07f,a-.08f),p(.567f,.11f,a+.15f),
                     p(.567f,.11f,a+.61f),p(.567f,.07f,a+.36f),hub,
                     CarPaint::Solid,0,1,0,1,index);
                continue;
            }
            const float blade=broad ? .43f : .28f;
            quad(p(.567f,.024f,a-.7f),p(.567f,.11f,a-blade),
                 p(.567f,.11f,a+blade),p(.567f,.024f,a+.7f),hub,
                 CarPaint::Solid,0,1,0,1,index);
        }
    }
    // A recessed duct, not a black disc over a closed cowl. +z is the mouth.
    // Keep the rear cap behind the lip so side views reveal the tunnel wall.
    void duct(float side,float x,float y,float z,float rx,float ry,float depth,uint16_t rim,uint16_t shell=0) {
        const int n=std::max(6,segments/2); // Preserve the recessed mouth at minimal race LOD.
        const auto p=[&](float radius,float a,float dz) {
            return CarPoint{side*(x+rx*radius*std::cos(a)),y+ry*radius*std::sin(a),z+dz};
        };
        for(int i=0;i<n;++i) {
            const float a=i*6.2831853f/n,c=(i+1)*6.2831853f/n;
            quad(p(1,a,0),p(.79f,a,0),p(.79f,c,0),p(1,c,0),rim);
            quad(p(.79f,a,0),p(.64f,a,-depth),p(.64f,c,-depth),p(.79f,c,0),shade(graphite,.65f));
            quad(p(1,a,-depth),p(1,a,0),p(1,c,0),p(1,c,-depth),shell ? shell : rim);
            quad(p(0,a,-depth),p(.64f,a,-depth),p(.64f,c,-depth),p(0,a,-depth),0x1082);
        }
    }
    void roller(float x,float z,uint16_t color,int layers=2,float baseY=.11f,float thickness=.018f) {
        const int n=segments/2;
        for(int layer=0;layer<layers;++layer) for(int i=0;i<n;++i) {
            const float a=i*6.2831853f/n,b=(i+1)*6.2831853f/n;
            const float y=baseY+layer*.075f;
            const CarPoint p{x+.069f*std::cos(a),y,z+.069f*std::sin(a)};
            const CarPoint q{x+.069f*std::cos(b),y,z+.069f*std::sin(b)};
            quad({x,y,z},p,q,{x,y,z},color);
            quad({p.x,y-thickness,p.z},p,q,{q.x,y-thickness,q.z},shade(color,.68f));
        }
        box(x-.019f,x+.019f,baseY,baseY+(layers-1)*.075f+.018f,z-.019f,z+.019f,silver);
    }
};

} // namespace lets_and_go::mesh_parts
