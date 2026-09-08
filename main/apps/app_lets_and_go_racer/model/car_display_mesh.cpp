#include "car_display_mesh.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace lets_and_go {
namespace {
constexpr uint16_t white=0xf7be, graphite=0x2946, tire=0x18c3;
constexpr uint16_t blue=0x3275,red=0xc9a7,silver=0xb5d6,glass=0x2128;
uint16_t shade(uint16_t c,float f) {
    const int r=std::clamp(int(((c>>11)&31)*f),0,31);
    const int g=std::clamp(int(((c>>5)&63)*f),0,63);
    const int b=std::clamp(int((c&31)*f),0,31);
    return uint16_t((r<<11)|(g<<5)|b);
}
struct Section {float z,width,edge,roof;};
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
    int segments,steps;
    void quad(CarPoint a,CarPoint b,CarPoint c,CarPoint d,uint16_t color,
              CarPaint paint=CarPaint::Solid,float u0=0,float u1=1,float v0=0,float v1=1,
              uint8_t wheel=0) {
        if(mesh.count==mesh.panels.size()) {mesh.overflowed=true;return;}
        auto& p=mesh.panels[mesh.count++];
        p={{{a,b,c,d}},color,0,wheel,0xffffu,paint,
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
    void skin(float center,std::initializer_list<Section> sections,uint16_t color,
              CarPaint paint=CarPaint::Solid,float floor=.13f) {
        constexpr float cross[]={-1,-.88f,-.58f,0,.58f,.88f,1};
        const float start=sections.begin()->z,span=(sections.end()-1)->z-start;
        const auto point=[center](Section p,float u) {
            return CarPoint{center+p.width*u,p.edge+(p.roof-p.edge)*std::sqrt(std::max(0.f,1-u*u)),p.z};
        };
        const auto interpolate=[](Section a,Section b,float t) {
            // Smooth interpolation within each measured station interval. Extra
            // stations specify silhouette, not a generic pointed hull template.
            const float s=t*t*(3-2*t);
            return Section{a.z+(b.z-a.z)*t,a.width+(b.width-a.width)*s,
                a.edge+(b.edge-a.edge)*s,a.roof+(b.roof-a.roof)*s};
        };
        for(auto it=sections.begin()+1;it!=sections.end();++it) for(int step=0;step<steps;++step) {
            const auto a=interpolate(*(it-1),*it,float(step)/steps);
            const auto b=interpolate(*(it-1),*it,float(step+1)/steps);
            for(int c=0;c<6;++c) {
                quad(point(a,cross[c]),point(a,cross[c+1]),point(b,cross[c+1]),point(b,cross[c]),
                     color,
                     paint,(cross[c]+1)*.5f,(cross[c+1]+1)*.5f,(a.z-start)/span,(b.z-start)/span);
                if(mesh.count) mesh.panels[mesh.count-1].light=uint8_t(255*(.72f+.28f*(1-std::abs((cross[c]+cross[c+1])*.5f))));
            }
            for(float side : {-1.f,1.f}) {
                const auto p=point(a,side),q=point(b,side);
                quad({p.x,floor,p.z},p,q,{q.x,floor,q.z},shade(color,.67f));
            }
        }
        for(const auto s : {*sections.begin(),*(sections.end()-1)}) for(int c=0;c<6;++c) {
            const auto a=point(s,cross[c]),b=point(s,cross[c+1]);
            quad({a.x,floor,a.z},a,b,{b.x,floor,b.z},shade(color,.76f));
        }
    }
    void wheel(float side,float axle,uint16_t hub,bool cap,uint8_t index) {
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
            quad(p(.565f,0,a),p(.565f,.104f,a),p(.565f,.104f,b),p(.565f,0,a),
                 cap ? 0x5acb : shade(hub,.25f));
            // Inner side is closed too; the old open ring vanished from reverse views.
            quad(p(.365f,0,a),p(.365f,.145f,b),p(.365f,.145f,a),p(.365f,0,a),tire);
        }
        if(!cap) for(int s=0;s<5;++s) {
            const float a=s*tau/5;
            quad(p(.567f,.024f,a-.7f),p(.567f,.11f,a-.28f),
                 p(.567f,.11f,a+.28f),p(.567f,.024f,a+.7f),hub,
                 CarPaint::Solid,0,1,0,1,index);
        }
    }
    void roller(float x,float z,uint16_t color) {
        const int n=segments/2;
        for(int layer=0;layer<2;++layer) for(int i=0;i<n;++i) {
            const float a=i*6.2831853f/n,b=(i+1)*6.2831853f/n;
            const float y=.11f+layer*.075f;
            const CarPoint p{x+.069f*std::cos(a),y,z+.069f*std::sin(a)};
            const CarPoint q{x+.069f*std::cos(b),y,z+.069f*std::sin(b)};
            quad({x,y,z},p,q,{x,y,z},color);
            quad({p.x,y-.018f,p.z},p,q,{q.x,y-.018f,q.z},shade(color,.68f));
        }
        box(x-.019f,x+.019f,.11f,.203f,z-.019f,z+.019f,silver);
    }
    void wing(uint16_t color,float y,CarPaint paint,bool neo=false) {
        box(-.48f,.48f,y-.028f,y,-.94f,-.72f,color,paint);
        for(float side : {-1.f,1.f}) {
            box(side*.25f-.035f,side*.25f+.035f,.32f,y,-.84f,-.78f,graphite);
            const float x=side*.49f;
            // Endplates merge down into the rear cowl instead of sitting on stilts.
            quad({x,.29f,-.74f},{x,y+.07f,-.72f},{x,y+.025f,-.96f},{x,y-.02f,-.97f},
                 neo ? graphite : white);
            quad({x+side*.003f,.31f,-.75f},{x+side*.003f,y+.045f,-.73f},
                 {x+side*.003f,y+.013f,-.94f},{x+side*.003f,y-.015f,-.94f},color);
        }
    }
};

void magnum(Builder& b,bool sonic) {
    const auto color=sonic ? red : blue;
    b.skin(0,{{-.64f,.22f,.26f,.32f},{-.43f,.27f,.28f,.35f},
        {-.16f,.285f,.25f,.32f},{.10f,.28f,.23f,.29f},{.35f,.255f,.20f,.24f},
        {.60f,.175f,.135f,.17f},{.82f,.042f,.11f,.12f}},white,
        sonic ? CarPaint::SonicHood : CarPaint::MagnumHood);
    b.skin(0,{{-.47f,.09f,.34f,.40f},{-.36f,.145f,.335f,.43f},
        {-.17f,.17f,.285f,.43f},{.02f,.133f,.27f,.365f},{.18f,.075f,.24f,.265f}},
        glass,CarPaint::Glass,.24f);
    for(float s : {-1.f,1.f}) {
        b.skin(s*.37f,{{-.81f,.14f,.275f,.315f},{-.65f,.172f,.32f,.38f},
            {-.49f,.172f,.32f,.39f},{-.30f,.16f,.255f,.32f},{-.14f,.115f,.22f,.25f}},
            white,sonic ? CarPaint::SonicCowl : CarPaint::MagnumCowl,.22f);
        // Minimal front cowls stop ahead of the axle; most of the tire is exposed.
        b.skin(s*.438f,{{.49f,.08f,.29f,.35f},{.60f,.10f,.245f,.30f},
            {.73f,.107f,.15f,.205f},{.82f,.085f,.12f,.15f}},
            color,CarPaint::Eye,.105f);
        if(sonic) {
            b.skin(s*.40f,{{-.71f,.14f,.335f,.385f},{-.43f,.15f,.31f,.37f}},
                   red,CarPaint::SonicWing,.31f);
            for(int rib=0;rib<3;++rib)
                b.box(s*.4f-.135f,s*.4f+.135f,.365f,.378f,-.64f+rib*.06f,-.629f+rib*.06f,white);
            b.box(s*.52f-.016f,s*.52f+.016f,.12f,.165f,.66f,.79f,0x246d);
        }
    }
    if(sonic) {
        b.quad({-.30f,.31f,.51f},{.30f,.31f,.51f},{.22f,.18f,.78f},{-.22f,.18f,.78f},
               graphite,CarPaint::FrontWing);
    }
    b.wing(color,.44f,sonic ? CarPaint::SonicWing : CarPaint::MagnumWing);
    b.skin(0,{{-.58f,.08f,.365f,.40f},{-.48f,.075f,.395f,.455f},
        {-.42f,.06f,.35f,.37f}},white,CarPaint::Solid,.33f);
    b.box(-.048f,.048f,.392f,.422f,-.426f,-.418f,graphite);
}

void neo(Builder& b) {
    b.skin(0,{{-.67f,.25f,.26f,.32f},{-.39f,.27f,.285f,.35f},
        {-.10f,.29f,.245f,.31f},{.19f,.295f,.18f,.25f},{.46f,.31f,.135f,.20f},
        {.70f,.245f,.105f,.165f},{.82f,.18f,.095f,.13f}},graphite,CarPaint::NeoHood);
    b.skin(0,{{-.46f,.11f,.35f,.425f},{-.28f,.165f,.30f,.465f},
        {-.07f,.176f,.255f,.42f},{.15f,.13f,.23f,.32f},{.29f,.07f,.21f,.235f}},
        0x9c4c,CarPaint::BronzeGlass,.21f);
    for(float s : {-1.f,1.f}) {
        b.skin(s*.38f,{{-.79f,.135f,.25f,.32f},{-.61f,.17f,.31f,.39f},
            {-.45f,.17f,.32f,.39f},{-.22f,.13f,.23f,.28f}},graphite,CarPaint::Flame,.21f);
        b.skin(s*.365f,{{.20f,.125f,.18f,.24f},{.42f,.155f,.285f,.32f},
            {.57f,.16f,.265f,.31f},{.74f,.155f,.11f,.17f},{.86f,.11f,.09f,.135f}},
            graphite,CarPaint::Flame,.095f);
        b.box(s*.50f-.02f,s*.50f+.02f,.095f,.125f,-.60f,.55f,blue);
    }
    b.wing(graphite,.51f,CarPaint::TridaggerWing,true);
    b.box(-.018f,.018f,.51f,.565f,-.94f,-.72f,graphite);
    b.box(-.075f,.075f,.35f,.43f,-.59f,-.46f,graphite);
}

void brocken(Builder& b) {
    b.skin(0,{{-.80f,.235f,.18f,.24f},{-.58f,.29f,.24f,.32f},
        {-.31f,.30f,.255f,.40f},{-.05f,.305f,.25f,.43f},
        {.22f,.285f,.20f,.30f},{.53f,.23f,.13f,.18f},{.81f,.12f,.105f,.14f}},
        red,CarPaint::BrockenShell);
    for(float s : {-1.f,1.f}) {
        b.skin(s*.38f,{{-.79f,.145f,.18f,.25f},{-.55f,.165f,.24f,.28f},
            {-.30f,.13f,.22f,.275f}},red,CarPaint::Tiger,.14f);
        b.skin(s*.395f,{{.28f,.13f,.25f,.28f},{.49f,.145f,.245f,.295f},
            {.70f,.135f,.12f,.205f},{.84f,.115f,.105f,.15f}},red,CarPaint::Tiger,.09f);
        b.box(s*.575f-.016f,s*.575f+.016f,.16f,.19f,-.60f,.61f,silver);
        for(float z : {-.55f,.52f})
            b.skin(s*.515f,{{z-.10f,.065f,.13f,.175f},{z,.075f,.14f,.20f},
                {z+.10f,.065f,.13f,.175f}},red,CarPaint::Solid,.11f);
        b.box(s*.15f-.016f,s*.15f+.016f,.275f,.45f,-.66f,-.60f,silver);
    }
    b.box(-.175f,.175f,.215f,.31f,.22f,.45f,graphite);
    const int shellSteps=b.steps;
    b.steps=1; // Millimetre-wide motor ribs do not need the shell's tessellation.
    for(int rib=0;rib<7;++rib)
        b.skin(-.165f+rib*.051f,{{.22f,.014f,.29f,.32f},{.33f,.014f,.31f,.35f},
            {.45f,.014f,.25f,.29f}},red,CarPaint::Solid,.21f);
    b.steps=shellSteps;
    b.quad({-.105f,.183f,.54f},{.105f,.183f,.54f},{.085f,.148f,.76f},{-.085f,.148f,.76f},
           red,CarPaint::BrockenHood);
    b.box(-.11f,.11f,.118f,.143f,.80f,.825f,silver);
}
} // namespace

CarSurfaceBuildResult buildCarSurfaceInto(CarId car,CarPanel* panels,std::size_t capacity,
                                         CarSurfaceDetail detail) {
    car=carSpec(car).id;
    MeshWriter mesh{{panels,panels ? capacity : 0}};
    Builder b{mesh,detail==CarSurfaceDetail::High ? 24 : detail==CarSurfaceDetail::Medium ? 18 : 12,
              detail==CarSurfaceDetail::High ? 3 : detail==CarSurfaceDetail::Medium ? 2 : 1};
    const auto& spec=carSpec(car);
    b.box(-.245f,.245f,.05f,.105f,-.79f,.83f,graphite);
    // Contoured bumper stays instead of a rectangular full-width plank.
    for(float s : {-1.f,1.f}) {
        b.quad({0,.10f,.80f},{s*.48f,.10f,.83f},{s*.58f,.10f,.94f},{0,.10f,.91f},graphite);
        b.quad({0,.10f,-.74f},{s*.49f,.10f,-.77f},{s*.57f,.10f,-.86f},{0,.10f,-.83f},graphite);
        b.wheel(s,kModelFrontAxle,spec.wheelColor,car==CarId::NeoTridaggerZmc,s<0 ? 1 : 2);
        b.wheel(s,kModelRearAxle,spec.wheelColor,false,s<0 ? 3 : 4);
        b.roller(s*.55f,.90f,car==CarId::CycloneMagnum || car==CarId::NeoTridaggerZmc ? blue : red);
        b.roller(s*.55f,-.84f,car==CarId::CycloneMagnum ? blue : car==CarId::NeoTridaggerZmc ? 0x246d : red);
    }
    switch(car) {
        case CarId::CycloneMagnum:magnum(b,false);break;
        case CarId::HurricaneSonic:magnum(b,true);break;
        case CarId::NeoTridaggerZmc:neo(b);break;
        case CarId::BrockenGigant:brocken(b);break;
        default:magnum(b,false);break;
    }
    return {mesh.count,mesh.overflowed};
}
void buildCarDisplayMesh(CarId car,CarDisplayMesh& mesh,CarSurfaceDetail detail) {
    const auto result=buildCarSurfaceInto(car,mesh.panels.data(),mesh.panels.size(),detail);
    mesh.count=result.count;mesh.overflowed=result.overflowed;
}
CarPoint animateCarPanelPoint(CarPoint p,uint8_t wheel,float cosine,float sine) {
    if(!wheel)return p;
    const float axle=wheel<=2 ? kModelFrontAxle : kModelRearAxle;
    const float y=p.y-kModelWheelRadius,z=p.z-axle;
    p.y=kModelWheelRadius+y*cosine-z*sine;p.z=axle+y*sine+z*cosine;return p;
}
} // namespace lets_and_go
