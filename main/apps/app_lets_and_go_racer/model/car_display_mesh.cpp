#include "car_display_mesh.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace lets_and_go {
namespace {
constexpr uint16_t white = 0xef7d, graphite = 0x2946, tire = 0x2124;
constexpr uint16_t blue = 0x3275, red = 0xc9a7;
constexpr uint16_t gold = 0xe5ca, teal = 0x246d, silver = 0xb5d6;
constexpr uint16_t glass = 0x2128, bronze = 0x9c4c, orange = 0xe3e7;

uint16_t shade(uint16_t color, float amount)
{
    const int r = std::clamp(static_cast<int>(((color >> 11) & 31) * amount), 0, 31);
    const int g = std::clamp(static_cast<int>(((color >> 5) & 63) * amount), 0, 63);
    const int b = std::clamp(static_cast<int>((color & 31) * amount), 0, 31);
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

struct Section { float z, width, shoulder, roof; };

class Builder {
public:
    CarDisplayMesh& mesh;
    int wheelSegments;
    void quad(CarPoint a, CarPoint b, CarPoint c, CarPoint d, uint16_t color,
              uint8_t edges = 15, uint8_t wheel = 0) {
        if (mesh.count >= mesh.panels.size()) { mesh.overflowed = true; return; }
        mesh.panels[mesh.count++] = {{{a, b, c, d}}, color, edges, wheel};
    }
    void triangle(CarPoint a, CarPoint b, CarPoint c, uint16_t color, uint8_t edges = 0) {
        quad(a,b,c,c,color,edges);
    }
    void box(float x0, float x1, float y0, float y1, float z0, float z1, uint16_t color) {
        quad({x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1},color);
        quad({x0,y0,z0},{x0,y1,z0},{x0,y1,z1},{x0,y0,z1},shade(color,.78f));
        quad({x1,y0,z1},{x1,y1,z1},{x1,y1,z0},{x1,y0,z0},shade(color,.90f));
        quad({x0,y0,z1},{x0,y1,z1},{x1,y1,z1},{x1,y0,z1},shade(color,.86f));
        quad({x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z0},shade(color,.72f));
    }
    void loft(float center, std::initializer_list<Section> sections, uint16_t color,
              float floor = .10f) {
        const auto cross = [center](Section p) {
            return std::array<CarPoint,4>{{{center-p.width,p.shoulder,p.z},
                {center-p.width*.63f,p.roof,p.z},{center+p.width*.63f,p.roof,p.z},
                {center+p.width,p.shoulder,p.z}}};
        };
        auto prev = sections.begin();
        for (auto it = prev + 1; it != sections.end(); ++it) {
            const auto a=cross(*prev), b=cross(*it);
            for (int face=0;face<3;++face)
                quad(a[face],a[face+1],b[face+1],b[face],
                     shade(color,face==1 ? 1.0f : face==0 ? .85f : .94f), 10);
            quad({a[0].x,floor,a[0].z},a[0],b[0],{b[0].x,floor,b[0].z},shade(color,.72f),10);
            quad(b[3],a[3],{a[3].x,floor,a[3].z},{b[3].x,floor,b[3].z},shade(color,.8f),10);
            prev=it;
        }
        for (const auto p : {*sections.begin(), *(sections.end()-1)}) {
            const auto a=cross(p);
            quad({a[0].x,floor,p.z},a[0],a[3],{a[3].x,floor,p.z},shade(color,.80f));
            quad(a[0],a[1],a[2],a[3],color,7);
        }
    }
    void wheel(float side, float axle, uint16_t hubColor, bool cap, uint8_t animation) {
        const int segments=wheelSegments;
        constexpr float tau=6.283185307f;
        const auto point=[&](float x,float radius,int i) {
            const float a=i*tau/segments;
            return CarPoint{side*x,kModelWheelRadius+std::sin(a)*radius,axle+std::cos(a)*radius};
        };
        for(int i=0;i<segments;++i) {
            const auto a=point(.382f,.155f,i), b=point(.397f,.175f,i);
            const auto c=point(.535f,.175f,i), d=point(.551f,.155f,i);
            quad(a,b,point(.397f,.175f,i+1),point(.382f,.155f,i+1),shade(tire,.9f),0);
            quad(b,c,point(.535f,.175f,i+1),point(.397f,.175f,i+1),
                 shade(tire,1.0f+.18f*std::sin((i+.5f)*tau/segments)),0);
            quad(c,d,point(.551f,.155f,i+1),point(.535f,.175f,i+1),tire,0);
            quad(d,point(.552f,.119f,i),point(.552f,.119f,i+1),point(.551f,.155f,i+1),tire,5);
            quad(point(.553f,.119f,i),point(.553f,.103f,i),point(.553f,.103f,i+1),point(.553f,.119f,i+1),hubColor,0);
            triangle({side*.5535f,kModelWheelRadius,axle},point(.5535f,.103f,i),point(.5535f,.103f,i+1),cap ? silver : graphite);
        }
        if (!cap) for(int spoke=0;spoke<5;++spoke) {
            const float a=spoke*tau/5;
            const auto p=[&](float r,float angle) { return CarPoint{side*.555f,
                kModelWheelRadius+std::sin(angle)*r,axle+std::cos(angle)*r}; };
            quad(p(.026f,a-.35f),p(.112f,a-.13f),p(.112f,a+.13f),p(.026f,a+.35f),hubColor,0,animation);
        }
    }
    void roller(float x,float z,uint16_t color) {
        const int segments=wheelSegments==6 ? 4 : 6;
        for(int i=0;i<segments;++i) {
            const float a=i*6.2831853f/segments,b=(i+1)*6.2831853f/segments;
            const CarPoint p{x+std::cos(a)*.073f,.13f,z+std::sin(a)*.073f};
            const CarPoint q{x+std::cos(b)*.073f,.13f,z+std::sin(b)*.073f};
            triangle({x,.13f,z},p,q,color);
            quad({p.x,.09f,p.z},p,q,{q.x,.09f,q.z},shade(color,.72f),2);
        }
        box(x-.018f,x+.018f,.132f,.147f,z-.018f,z+.018f,silver);
    }
    void wing(uint16_t color, float height, bool split) {
        box(-.49f,.49f,height-.025f,height,-.98f,-.76f,color);
        for(float side : {-1.f,1.f}) {
            box(side*.25f-.022f,side*.25f+.022f,.30f,height,-.86f,-.81f,graphite);
            const float x=side*.50f;
            quad({x,height-.035f,-.99f},{x,height+.055f,-.97f},
                 {x,height+.10f,-.77f},{x,height-.025f,-.74f},color);
        }
        if(split) box(-.018f,.018f,height+.002f,height+.06f,-.98f,-.76f,graphite);
    }
    void eyes(float side, float z0, float z1, float y0, float y1) {
        quad({side*.335f,y0,z0},{side*.50f,y0,z0},
             {side*.495f,y1,z1},{side*.39f,y1,z1},gold,15);
        triangle({side*.36f,y0+.003f,z0+.012f},{side*.475f,y0+.003f,z0+.015f},
                 {side*.41f,y1+.003f,z1-.02f},graphite);
    }
};

void magnumOrSonic(Builder& b, bool sonic)
{
    const uint16_t primary=sonic ? red : blue;
    b.loft(0,{{-.58f,.19f,.24f,.30f},{-.30f,.22f,.28f,.34f},
        {.16f,.205f,.22f,.29f},{.59f,.15f,.14f,.18f},{.87f,.045f,.105f,.12f}},white);
    b.loft(0,{{-.43f,.135f,.33f,.425f},{-.23f,.16f,.32f,.45f},
        {.08f,.115f,.245f,.335f},{.19f,.045f,.235f,.25f}},glass,.235f);
    b.quad({-.077f,.339f,.075f},{.077f,.339f,.075f},
           {.116f,.453f,-.23f},{-.116f,.453f,-.23f},shade(silver,.59f),0);
    b.quad({-.115f,.294f,.16f},{.115f,.294f,.16f},{.021f,.124f,.86f},{-.021f,.124f,.86f},primary,0);
    for(float side : {-1.f,1.f}) {
        const auto rearCowl=b.mesh.count;
        b.loft(side*.395f,{{-.79f,.147f,.32f,.365f},{-.57f,.155f,.335f,.405f},
            {-.27f,.12f,.24f,.285f}},white,.215f);
        b.loft(side*.438f,{{.47f,.10f,.28f,.36f},{.61f,.102f,.24f,.29f},
            {.81f,.085f,.135f,.17f}},primary,.115f);
        b.eyes(side,.63f,.78f,.28f,.195f);
        // Red lightning on Magnum's rear shoulders; Sonic has red panels and green trim.
        b.quad({side*.30f,.409f,-.57f},{side*.46f,.409f,-.57f},
               {side*.44f,.299f,-.29f},{side*.35f,.299f,-.29f},primary,0);
        b.mesh.panels[b.mesh.count-1].parent=static_cast<uint16_t>(rearCowl+6);
        b.triangle({side*.29f,.411f,-.54f},{side*.40f,.411f,-.57f},
                   {side*.42f,.322f,-.35f},red);
        b.mesh.panels[b.mesh.count-1].parent=static_cast<uint16_t>(rearCowl+6);
        b.triangle({side*.44f,.334f,-.38f},{side*.51f,.369f,-.48f},
                   {side*.48f,.410f,-.61f},sonic ? white : red);
        b.mesh.panels[b.mesh.count-1].parent=static_cast<uint16_t>(rearCowl+6);
        if(sonic) {
            b.quad({side*.53f,.125f,.80f},{side*.53f,.21f,.62f},
                   {side*.53f,.23f,.49f},{side*.53f,.10f,.77f},teal,0);
            // Three narrow ribs on EACH rear shoulder, not three floating wings.
            for(int rib=0;rib<3;++rib) {
                const float z=-.68f+rib*.065f;
                b.box(side*.40f-.13f,side*.40f+.13f,.39f,.405f,z,z+.015f,white);
            }
        } else {
            b.triangle({side*.14f,.20f,.56f},{side*.17f,.246f,.34f},
                       {side*.09f,.20f,.60f},red);
        }
    }
    if(sonic) b.quad({-.32f,.29f,.52f},{.32f,.29f,.52f},
                     {.22f,.18f,.78f},{-.22f,.18f,.78f},silver);
    b.wing(primary,.48f,false);
    b.box(-.07f,.07f,.398f,.442f,-.55f,-.45f,white);
    b.quad({-.054f,.441f,-.451f},{.054f,.441f,-.451f},
           {.046f,.404f,-.448f},{-.046f,.404f,-.448f},graphite,0);
}

void tridagger(Builder& b)
{
    b.loft(0,{{-.69f,.235f,.26f,.33f},{-.36f,.23f,.27f,.36f},
        {.22f,.23f,.18f,.255f},{.62f,.24f,.14f,.175f},{.86f,.11f,.10f,.12f}},graphite);
    b.loft(0,{{-.46f,.165f,.35f,.47f},{-.19f,.18f,.31f,.48f},
        {.16f,.12f,.23f,.30f},{.26f,.065f,.215f,.23f}},bronze,.23f);
    for(float side : {-1.f,1.f}) {
        b.loft(side*.40f,{{-.80f,.15f,.30f,.37f},{-.53f,.15f,.35f,.405f},
            {-.21f,.13f,.23f,.29f}},graphite,.23f);
        b.loft(side*.40f,{{.22f,.14f,.22f,.31f},{.49f,.15f,.29f,.35f},
            {.80f,.13f,.135f,.18f},{.88f,.07f,.105f,.12f}},graphite);
        // Layered angular flame tongues, conforming to each cowl slope.
        for(int flame=0;flame<2;++flame) {
            const float x=.31f+flame*.09f;
            b.triangle({side*x,.354f,.49f},{side*(x+.09f),.354f,.49f},
                       {side*(x+.035f),.185f,.80f},red);
            b.triangle({side*(x+.025f),.243f,.70f},{side*(x+.07f),.284f,.62f},
                       {side*(x+.04f),.184f,.80f},gold);
            b.triangle({side*x,.409f,-.53f},{side*(x+.10f),.409f,-.53f},
                       {side*(x+.02f),.300f,-.23f},red);
            b.triangle({side*(x+.015f),.325f,-.30f},{side*(x+.07f),.350f,-.36f},
                       {side*(x+.04f),.298f,-.23f},orange);
        }
        b.triangle({side*.02f,.18f,.63f},{side*.12f,.125f,.86f},
                   {side*.20f,.18f,.64f},red);
    }
    b.wing(graphite,.59f,true);
    b.box(-.065f,.065f,.419f,.464f,-.60f,-.48f,graphite);
}

void brocken(Builder& b)
{
    const auto bodyStart=b.mesh.count;
    b.loft(0,{{-.78f,.25f,.21f,.25f},{-.42f,.28f,.28f,.40f},
        {-.08f,.29f,.29f,.43f},{.24f,.25f,.24f,.30f},
        {.66f,.20f,.14f,.20f},{.84f,.12f,.12f,.14f}},red);
    b.quad({-.177f,.436f,-.08f},{.177f,.436f,-.08f},
           {.152f,.326f,.19f},{-.152f,.326f,.19f},blue);
    b.mesh.panels[b.mesh.count-1].parent=static_cast<uint16_t>(bodyStart+11);
    b.triangle({-.173f,.438f,-.08f},{-.03f,.438f,-.08f},{-.148f,.328f,.18f},0x2bba);
    b.mesh.panels[b.mesh.count-1].parent=static_cast<uint16_t>(bodyStart+11);
    for(float side : {-1.f,1.f}) {
        b.loft(side*.395f,{{.29f,.14f,.21f,.275f},{.53f,.14f,.21f,.275f},
            {.81f,.115f,.115f,.165f}},red,.095f);
        b.loft(side*.39f,{{-.79f,.15f,.23f,.27f},{-.43f,.145f,.24f,.285f},
            {-.23f,.12f,.18f,.21f}},red,.18f);
        b.box(side*.585f-.018f,side*.585f+.018f,.15f,.18f,-.57f,.58f,silver);
        for(float z : {-.56f,.57f})
            b.box(side*.50f-.09f,side*.50f+.09f,.13f,.19f,z-.07f,z+.07f,red);
        for(int stripe=0;stripe<3;++stripe) {
            const float z=.43f+stripe*.09f;
            b.triangle({side*.33f,.280f,z},{side*.43f,.280f,z-.04f},
                       {side*.40f,.201f,z+.12f},graphite);
        }
        b.box(side*.15f-.018f,side*.15f+.018f,.29f,.49f,-.65f,-.61f,silver);
        b.quad({side*.29f,.39f,-.18f},{side*.295f,.29f,-.08f},
               {side*.27f,.29f,-.035f},{side*.27f,.39f,-.13f},graphite,0);
    }
    // Front-mounted motor bulge with longitudinal cooling ribs.
    b.box(-.18f,.18f,.21f,.31f,.23f,.46f,graphite);
    for(int rib=0;rib<6;++rib) {
        const float x=-.165f+rib*.058f;
        b.box(x,x+.027f,.285f,.34f,.23f,.46f,red);
    }
    b.box(-.10f,.10f,.225f,.245f,.57f,.63f,graphite);
    b.box(-.10f,.10f,.13f,.16f,.80f,.83f,silver);
}
} // namespace

void buildCarDisplayMesh(CarId car, CarDisplayMesh& mesh, CarSurfaceDetail detail)
{
    car=carSpec(car).id;
    mesh.count=0; mesh.overflowed=false;
    Builder b{mesh,detail==CarSurfaceDetail::High ? 10 : detail==CarSurfaceDetail::Medium ? 8 : 6};
    const auto& spec=carSpec(car);
    b.box(-.235f,.235f,.06f,.095f,-.80f,.85f,graphite);
    b.box(-.55f,.55f,.065f,.093f,.82f,.94f,graphite);
    b.box(-.55f,.55f,.065f,.093f,-.89f,-.80f,graphite);
    for(float side : {-1.f,1.f}) {
        b.wheel(side,kModelFrontAxle,spec.wheelColor,car==CarId::NeoTridaggerZmc,side<0 ? 1 : 2);
        b.wheel(side,kModelRearAxle,spec.wheelColor,false,side<0 ? 3 : 4);
        const uint16_t roller=car==CarId::CycloneMagnum || car==CarId::NeoTridaggerZmc ? blue : red;
        b.roller(side*.55f,.90f,roller); b.roller(side*.55f,-.84f,roller);
    }
    switch(car) {
        case CarId::CycloneMagnum: magnumOrSonic(b,false); break;
        case CarId::HurricaneSonic: magnumOrSonic(b,true); break;
        case CarId::NeoTridaggerZmc: tridagger(b); break;
        case CarId::BrockenGigant: brocken(b); break;
        default: magnumOrSonic(b,false); break;
    }
}

CarPoint animateCarPanelPoint(CarPoint p,uint8_t wheel,float cosine,float sine)
{
    if(wheel==0) return p;
    const float axle=wheel<=2 ? kModelFrontAxle : kModelRearAxle;
    const float y=p.y-kModelWheelRadius,z=p.z-axle;
    p.y=kModelWheelRadius+y*cosine-z*sine;
    p.z=axle+y*sine+z*cosine;
    return p;
}
} // namespace lets_and_go
