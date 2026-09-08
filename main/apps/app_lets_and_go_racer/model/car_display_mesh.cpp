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
// Cyclone's molded body has hard chines and separate open wheel cowls.
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
    int segments,steps;
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
    void wheel(float side,float axle,uint16_t hub,bool cap,uint8_t index,bool broad=false) {
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
            const float blade=broad ? .43f : .28f;
            quad(p(.567f,.024f,a-.7f),p(.567f,.11f,a-blade),
                 p(.567f,.11f,a+blade),p(.567f,.024f,a+.7f),hub,
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

void cyclone(Builder& b) {
    // Nose flares at the front shoulder, then contracts BEFORE the cockpit.
    // The .16 waist leaves a real open channel to the .24 inner rear cowl.
    b.part=CarPart::Nose;
    b.chine({{-.71f,.125f,.22f,.29f,.32f},{-.53f,.155f,.225f,.305f,.33f},
        {-.30f,.16f,.21f,.29f,.31f},{-.10f,.17f,.19f,.275f,.29f},
        {.04f,.235f,.17f,.27f,.285f},{.25f,.305f,.135f,.225f,.255f},
        {.43f,.245f,.115f,.175f,.215f},{.66f,.135f,.09f,.12f,.15f},
        {.84f,.030f,.083f,.105f,.12f}},white,CarPaint::MagnumHood,CarPaint::MagnumNoseSide);
    // Slanted, nearly flat windscreen with white sill rails; not an oval bubble.
    b.part=CarPart::Canopy;
    b.chine({{-.53f,.105f,.315f,.40f,.445f},{-.39f,.131f,.30f,.415f,.45f},
        {-.16f,.137f,.28f,.355f,.389f},{.055f,.084f,.267f,.274f,.284f}},
        glass,CarPaint::MagnumCanopy,CarPaint::MagnumCanopy);
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::Canopy;
        b.quad({s*.112f,.316f,-.54f},{s*.144f,.31f,-.54f},
               {s*.151f,.279f,-.15f},{s*.138f,.28f,-.15f},white);
        b.quad({s*.138f,.28f,-.15f},{s*.151f,.279f,-.15f},
               {s*.098f,.271f,.063f},{s*.083f,.272f,.063f},white);
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.80f,.255f,.535f,.34f,.37f},{-.67f,.245f,.545f,.373f,.407f},
            {-.48f,.24f,.551f,.382f,.422f},{-.29f,.275f,.53f,.35f,.395f},
            {-.14f,.335f,.465f,.275f,.315f},{-.075f,.365f,.405f,.25f,.273f}},
            white,CarPaint::MagnumCowl,CarPaint::MagnumCowlSide);
        // Two short molded webs connect the pods; the rest of the channel stays open.
        b.part=CarPart::SideWeb;
        b.box(std::min(s*.15f,s*.28f),std::max(s*.15f,s*.28f),.215f,.235f,-.33f,-.27f,white);
        b.box(std::min(s*.13f,s*.31f),std::max(s*.13f,s*.31f),.245f,.265f,-.69f,-.64f,white);
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.585f,.325f,.445f,.363f,.393f},{.64f,.325f,.475f,.345f,.37f},
            {.735f,.325f,.53f,.245f,.268f},{.84f,.365f,.51f,.13f,.151f}},blue,CarPaint::Eye,
            CarPaint::Solid,.012f);
        b.box(std::min(s*.365f,s*.49f),std::max(s*.365f,s*.49f),.10f,.133f,.825f,.84f,blue);
        // Visible chassis side tray below the waist opening, separated from the white shell.
        b.part=CarPart::Chassis;
        b.quad({s*.24f,.105f,-.34f},{s*.36f,.105f,-.27f},
               {s*.345f,.105f,.33f},{s*.24f,.105f,.40f},graphite);
        b.box(s*.31f-.014f,s*.31f+.014f,.105f,.16f,-.15f,.20f,graphite);
    }
    // Raised rear intake and sloping black opening behind the canopy.
    b.part=CarPart::Intake;
    b.chine({{-.69f,.09f,.31f,.365f,.405f},{-.60f,.085f,.325f,.41f,.463f},
             {-.53f,.068f,.325f,.40f,.43f}},white,CarPaint::Solid);
    b.quad({-.059f,.431f,-.531f},{.059f,.431f,-.531f},
           {.070f,.461f,-.596f},{-.070f,.461f,-.596f},graphite,CarPaint::MagnumVent);
    // Cambered aerofoil, two leaning mounts, swept side plates.
    b.part=CarPart::RearWing;
    b.chine({{-.96f,.49f,.452f,.473f,.478f},{-.85f,.49f,.435f,.46f,.467f},
             {-.735f,.49f,.431f,.451f,.456f}},blue,CarPaint::MagnumWing);
    for(float s : {-1.f,1.f}) {
        b.quad({s*.215f,.30f,-.68f},{s*.255f,.30f,-.68f},
               {s*.255f,.447f,-.83f},{s*.215f,.447f,-.83f},graphite);
        const float x=s*.503f;
        b.quad({x,.31f,-.73f},{x,.52f,-.72f},{x,.54f,-.93f},{x,.44f,-.98f},white);
        b.quad({x+s*.002f,.327f,-.746f},{x+s*.002f,.507f,-.738f},
               {x+s*.002f,.524f,-.917f},{x+s*.002f,.443f,-.96f},blue);
    }
}

void sonic(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.72f,.13f,.23f,.30f,.34f},{-.49f,.17f,.225f,.305f,.33f},
        {-.22f,.18f,.20f,.285f,.30f},{-.04f,.20f,.185f,.27f,.29f},
        {.24f,.305f,.13f,.23f,.265f},{.43f,.25f,.12f,.18f,.225f},
        {.65f,.125f,.095f,.13f,.17f},{.83f,.044f,.09f,.12f,.14f}},
        white,CarPaint::SonicHood);
    b.part=CarPart::Canopy;
    b.chine({{-.52f,.118f,.31f,.41f,.46f},{-.37f,.148f,.30f,.42f,.46f},
        {-.15f,.142f,.28f,.36f,.395f},{.065f,.084f,.267f,.28f,.295f}},
        glass,CarPaint::SonicCanopy,CarPaint::SonicCanopy);
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::RearCowl;
        // Rear cowls rise into the stepped wing ramps, not two rounded pods
        // underneath a disconnected flat spoiler.
        b.cowl(s,{{-.93f,.26f,.51f,.458f,.48f},{-.76f,.25f,.535f,.425f,.452f},
            {-.58f,.255f,.55f,.383f,.413f},{-.43f,.27f,.55f,.38f,.414f},
            {-.25f,.305f,.52f,.315f,.36f},{-.10f,.335f,.48f,.265f,.30f}},
            white,CarPaint::SonicCowl,CarPaint::SonicSide,.014f);
        for(int rib=0;rib<3;++rib) {
            const float z=-.86f+rib*.085f;
            const float t=z<-.76f ? (z+.93f)/.17f : (z+.76f)/.18f;
            const float crown=(z<-.76f ? .48f+(.452f-.48f)*t : .452f+(.413f-.452f)*t)+.008f;
            const float edge=(z<-.76f ? .458f+(.425f-.458f)*t : .425f+(.383f-.425f)*t)+.008f;
            // Follow the cowl cross-section; a single sloped strip was buried
            // under the broad centre facet and disappeared in the top view.
            b.cowl(s,{{z,.255f,.535f,edge,crown},{z+.018f,.255f,.535f,edge-.004f,crown-.004f}},
                   white,CarPaint::Solid,CarPaint::Solid,.006f);
        }
        b.part=CarPart::SideWeb;
        b.box(std::min(s*.16f,s*.30f),std::max(s*.16f,s*.30f),.215f,.238f,-.34f,-.28f,white);
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.555f,.325f,.445f,.386f,.414f},{.64f,.32f,.49f,.342f,.376f},
            {.745f,.33f,.53f,.24f,.267f},{.84f,.365f,.515f,.135f,.16f}},
            red,CarPaint::SonicFront,CarPaint::SonicSide,.012f);
        b.box(std::min(s*.365f,s*.49f),std::max(s*.365f,s*.49f),.10f,.137f,.825f,.84f,0x246d);
        b.part=CarPart::FrontBridge;
        b.quad({s*.12f,.285f,.61f},{s*.325f,.39f,.565f},
               {s*.32f,.265f,.795f},{s*.08f,.235f,.77f},silver);
    }
    b.quad({-.12f,.285f,.61f},{.12f,.285f,.61f},{.08f,.235f,.77f},{-.08f,.235f,.77f},
           silver,CarPaint::FrontWing);
    b.part=CarPart::Intake;
    b.chine({{-.70f,.083f,.33f,.395f,.421f},{-.60f,.08f,.34f,.435f,.48f},
        {-.53f,.065f,.33f,.425f,.447f}},white,CarPaint::Solid);
    b.quad({-.057f,.448f,-.531f},{.057f,.448f,-.531f},
           {.069f,.478f,-.596f},{-.069f,.478f,-.596f},graphite,CarPaint::MagnumVent);
    b.part=CarPart::RearWing;
    b.chine({{-.96f,.52f,.457f,.487f,.495f},{-.86f,.51f,.445f,.469f,.484f},
        {-.76f,.49f,.43f,.452f,.462f}},red,CarPaint::SonicWing);
    for(float s : {-1.f,1.f})
        b.quad({s*.525f,.40f,-.76f},{s*.525f,.505f,-.73f},
               {s*.525f,.54f,-.965f},{s*.525f,.45f,-.965f},white);
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
    b.part=CarPart::Chassis;
    b.box(-.245f,.245f,.05f,.105f,-.79f,.83f,graphite);
    // Contoured bumper stays instead of a rectangular full-width plank.
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::Chassis;
        b.quad({0,.10f,.80f},{s*.48f,.10f,.83f},{s*.58f,.10f,.94f},{0,.10f,.91f},graphite);
        b.quad({0,.10f,-.74f},{s*.49f,.10f,-.77f},{s*.57f,.10f,-.86f},{0,.10f,-.83f},graphite);
        b.part=CarPart::Wheel;
        const bool broad=car==CarId::CycloneMagnum || car==CarId::HurricaneSonic;
        b.wheel(s,kModelFrontAxle,spec.wheelColor,car==CarId::NeoTridaggerZmc,s<0 ? 1 : 2,broad);
        b.wheel(s,kModelRearAxle,spec.wheelColor,false,s<0 ? 3 : 4,broad);
        b.part=CarPart::Roller;
        b.roller(s*.55f,.90f,car==CarId::CycloneMagnum || car==CarId::NeoTridaggerZmc ? blue : red);
        b.roller(s*.55f,-.84f,car==CarId::CycloneMagnum ? blue : car==CarId::NeoTridaggerZmc ? 0x246d : red);
    }
    b.part=CarPart::Unspecified;
    switch(car) {
        case CarId::CycloneMagnum:cyclone(b);break;
        case CarId::HurricaneSonic:sonic(b);break;
        case CarId::NeoTridaggerZmc:neo(b);break;
        case CarId::BrockenGigant:brocken(b);break;
        default:cyclone(b);break;
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
