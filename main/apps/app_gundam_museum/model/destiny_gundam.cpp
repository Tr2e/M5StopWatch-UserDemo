#include "destiny_gundam.h"
#include "sd_model_builder.h"

namespace gundam_museum {
namespace {
using sd_model::Builder;
// SDEX 009 painted product: azure torso/shoulders, WHITE knees, yellow
// two-blade antenna, red/black wings and olive folded long-range cannon.
constexpr uint16_t white=0xffff,ivory=0xef7d,blue=0x1b17;
constexpr uint16_t red=0xd126,gold=0xfea6,frame=0x52aa,black=0x18c4,green=0x05ac,olive=0x6b67;

void legs(Builder& b,bool detail){
    for(float s:{-1.f,1.f}){
        const Point hip{s*.29f,1.10f,s<0?.04f:0};
        const float spread=s*(s<0?.23f:.19f),ankleX=hip.x+s*.18f;
        b.at(Part::Feet,{ankleX,0,hip.z},0,0,s*(s<0?.25f:.21f));
        b.shell({{.025f,.31f,.43f,.14f},{.09f,.34f,.46f,.15f},{.18f,.31f,.42f,.13f}},red,12);
        b.cover({{-.25f,.19f,.43f},{.25f,.19f,.43f},{.28f,.31f,.31f},{.18f,.36f,.20f},{-.18f,.36f,.20f},{-.28f,.31f,.31f}},.05f,white,.008f);
        b.at(Part::Thighs,hip,spread);b.shell({{-.34f,.16f,.15f},{-.17f,.19f,.18f},{.03f,.17f,.16f}},white,12);
        b.at(Part::Knees,hip,spread);b.tube({-.13f,-.36f,0},{.13f,-.36f,0},.09f,.09f,frame,false,10);
        b.at(Part::Shins,hip,spread);
        b.shell({{-.76f,.22f,.21f,-.02f},{-.65f,.25f,.25f,-.04f},{-.48f,.21f,.22f,-.03f},{-.34f,.15f,.16f}},white,12);
        b.cover({{-.10f,-.28f,.21f},{.10f,-.28f,.21f},{.17f,-.40f,.25f},{.10f,-.54f,.30f},{-.10f,-.54f,.30f},{-.17f,-.40f,.25f}},.07f,frame,.003f);
        b.cover({{-.085f,-.29f,.222f},{.085f,-.29f,.222f},{.145f,-.40f,.269f},{.085f,-.51f,.311f},{-.085f,-.51f,.311f},{-.145f,-.40f,.269f}},.025f,white,.005f);
        b.cover({{-.23f,-.66f,.245f},{.23f,-.66f,.245f},{.25f,-.77f,.29f},{.13f,-.80f,.303f},{-.13f,-.80f,.303f},{-.25f,-.77f,.29f}},.045f,white,.006f);
        if(detail){
            b.cover({{s*.15f,-.45f,-.13f},{s*.27f,-.50f,-.20f},{s*.28f,-.68f,-.22f},{s*.12f,-.62f,-.18f}},.04f,ivory,.004f);
        }
        b.at(Part::Shins);b.tube({ankleX,.27f,hip.z},{ankleX,.39f,hip.z},.075f,.075f,frame,false,8);
    }
}

void body(Builder& b,bool detail,DestinyAssembly* a){
    b.at(Part::Waist);b.shell({{1.02f,.30f,.22f},{1.23f,.35f,.26f},{1.38f,.31f,.22f}},frame,12);
    for(float s:{-1.f,1.f}){
        b.cover({{s*.07f,1.38f,.31f},{s*.35f,1.37f,.30f},{s*.48f,1.10f,.37f},{s*.28f,1.01f,.43f},{s*.10f,1.06f,.42f}},.06f,white,.008f);
        b.at(Part::Waist,{s*.41f,1.31f,-.03f},s*.28f,0,s*.22f);
        b.shell({{-.25f,.17f,.21f},{-.11f,.19f,.22f},{.03f,.15f,.18f}},white,10);
    }
    b.at(Part::Waist);b.cover({{-.105f,1.39f,.35f},{.105f,1.39f,.35f},{.12f,1.06f,.45f},{0,.99f,.48f},{-.12f,1.06f,.45f}},.05f,white,.007f);
    b.cover({{-.086f,1.38f,.374f},{.086f,1.38f,.374f},{.072f,1.17f,.445f},{-.072f,1.17f,.445f}},.025f,blue,.005f);
    b.cover({{-.047f,1.33f,.404f},{.047f,1.33f,.404f},{.042f,1.22f,.442f},{-.042f,1.22f,.442f}},.012f,red,.003f);
    b.at(Part::Torso);b.shell({{1.38f,.31f,.23f},{1.50f,.36f,.27f},{1.60f,.34f,.24f}},red,12);
    b.shell({{1.56f,.36f,.27f},{1.72f,.47f,.34f},{1.91f,.48f,.31f},{2.00f,.31f,.22f}},blue,14);
    b.cover({{-.15f,1.92f,.34f},{.15f,1.92f,.34f},{.20f,1.63f,.43f},{.11f,1.55f,.45f},{-.11f,1.55f,.45f},{-.20f,1.63f,.43f}},.07f,blue,.008f);
    for(float s:{-1.f,1.f}){
        b.cover({{s*.18f,1.84f,.397f},{s*.43f,1.88f,.34f},{s*.38f,1.68f,.405f},{s*.20f,1.65f,.433f}},.035f,white,.005f);
        if(detail){
            b.cover({{s*.22f,1.80f,.421f},{s*.38f,1.83f,.381f},{s*.34f,1.73f,.414f},{s*.22f,1.71f,.434f}},.012f,black,.003f);
            b.cover({{s*.22f,1.756f,.435f},{s*.359f,1.785f,.399f},{s*.354f,1.768f,.405f},{s*.22f,1.741f,.439f}},.007f,frame,.001f);
        }
    }
    b.cover({{-.095f,1.69f,.451f},{.095f,1.69f,.451f},{.074f,1.54f,.453f},{-.074f,1.54f,.453f}},.025f,blue,.004f);
    if(detail)for(int k=0;k<2;++k)b.box(0,1.77f-k*.09f,.446f,.23f,.018f,.012f,0x19cf);
    b.at(Part::Torso);b.shell({{1.98f,.15f,.13f},{2.09f,.17f,.15f}},frame,10);
    if(a)a->backpackMount.begin=b.m.count;
    b.at(Part::Backpack);b.tube({0,1.82f,-.17f},{0,1.82f,-.49f},.11f,.13f,frame,false,10);
    if(a)a->backpackMount.end=b.m.count;
    b.box(0,1.83f,-.54f,.51f,.45f,.20f,red);
}

void head(Builder& b,bool detail,DestinyAssembly* a){
    b.at(Part::Head);
    if(a)a->headMount.begin=b.m.count;
    b.tube({0,1.96f,-.02f},{0,2.20f,-.02f},.11f,.15f,frame,false,8);
    if(a)a->headMount.end=b.m.count;
    if(a)a->helmet.begin=b.m.count;
    // Open face aperture: never lay eyes/mask on a closed spherical shell.
    const sd_model::Ring rings[]={{2.06f,.34f,.30f,-.02f},{2.18f,.48f,.43f,-.04f},{2.42f,.55f,.49f,-.05f},
        {2.68f,.53f,.47f,-.07f},{2.86f,.44f,.38f,-.09f},{2.98f,.29f,.25f,-.10f},{3.04f,.10f,.10f,-.11f}};
    const auto p=[](sd_model::Ring r,int i){const float t=2*sd_model::pi*i/24;return Point{std::sin(t)*r.w,r.y,r.z+std::cos(t)*r.d};};
    for(int row=0;row<6;++row)for(int i=0;i<24;++i){
        if(row<3&&(i<4||i>=20))continue;
        auto u=p(rings[row],i),v=p(rings[row],i+1);
        b.face(u,v,p(rings[row+1],i+1),p(rings[row+1],i),ivory,{u.x+v.x,0,u.z+v.z+.12f},true);
    }
    b.box(0,2.13f,-.06f,.39f,.14f,.36f,ivory);
    b.cover({{-.115f,2.83f,.36f},{.115f,2.83f,.36f},{.10f,3.10f,.20f},{-.10f,3.10f,.20f}},.10f,white,.007f);
    for(float s:{-1.f,1.f}){
        // Continuous cheek returns to the actual aperture rim. The side view
        // must show armor here, not a floating face in front of the back skull.
        const Point edge[]={{s*.30f,2.06f,.40f},{s*.50f,2.18f,.27f},{s*.49f,2.42f,.34f},{s*.48f,2.68f,.29f}};
        for(int row=0;row<3;++row){auto u=p(rings[row],s>0?4:20),v=p(rings[row+1],s>0?4:20);
            b.face(edge[row],u,v,edge[row+1],ivory,{s,0,0},true);
        }
        b.cover({{s*.35f,2.52f,.43f},{s*.48f,2.66f,.29f},{s*.50f,2.19f,.27f},{s*.30f,2.08f,.40f}},.06f,white,.007f);
        b.cover({{0,2.67f,.49f},{s*.43f,2.70f,.34f},{s*.37f,2.575f,.42f},{0,2.51f,.55f}},.04f,white,.006f);
        b.face({0,2.67f,.49f},{s*.43f,2.70f,.34f},{s*.38f,2.82f,.18f},{0,2.86f,.29f},white,{0,1,1},true);
        b.face({0,2.40f,.49f},{s*.23f,2.37f,.40f},{s*.19f,2.20f,.38f},{0,2.13f,.46f},white,{0,0,1},true);
        b.face({s*.23f,2.37f,.40f},{s*.29f,2.39f,.30f},{s*.26f,2.17f,.30f},{s*.19f,2.20f,.38f},ivory,{s,0,1},true);
        const auto eye=[&](float x,float y){return Point{s*x,y,.525f-.32f*x};};
        const std::array<Point,4> opening={eye(.035f,2.515f),eye(.335f,2.565f),eye(.302f,2.415f),eye(.075f,2.385f)};
        buildEyeSocket(b,opening,.072f,ivory,black,green,a?&a->eyes[s>0]:nullptr);
        b.face(opening[0],{0,2.51f,.55f},{s*.37f,2.575f,.42f},opening[1],ivory,{0,0,1},true);
        b.face(opening[1],{s*.35f,2.52f,.43f},{s*.338f,2.415f,.423f},opening[2],ivory,{s,0,1},true);
        // Lower orbital ledge and medial bridge belong to the mask, in FRONT
        // of the inset lens. No white mask surface runs behind the eye window.
        b.face(opening[3],opening[2],{s*.23f,2.37f,.40f},{0,2.40f,.49f},white,{0,0,1},true);
        b.face({0,2.51f,.55f},opening[0],opening[3],{0,2.40f,.49f},ivory,{0,0,1},true);
        // The red tear ducts are a Destiny identity cue, not red eyebrows.
        b.cover({{s*.10f,2.38f,.48f},{s*.126f,2.386f,.466f},{s*.18f,2.285f,.418f},{s*.158f,2.31f,.433f}},.012f,red,.002f);
        if(detail){
            b.at(Part::Head,{s*.48f,2.49f,.25f},0,0,s*.78f);
            b.tube({0,0,-.03f},{0,0,.065f},.047f,.04f,frame,true,8);
            b.at(Part::Head);
            for(int k=0;k<2;++k)b.cover({{s*.34f,2.30f+k*.07f,.411f},{s*.46f,2.35f+k*.07f,.331f},{s*.46f,2.365f+k*.07f,.331f},{s*.34f,2.315f+k*.07f,.411f}},.012f,frame,.002f);
        }
    }
    b.cover({{-.095f,2.88f,.40f},{.095f,2.88f,.40f},{.067f,2.61f,.53f},{0,2.57f,.55f},{-.067f,2.61f,.53f}},.04f,red,.007f);
    // Raised U-shaped mouth/chin boundary, following the official mask.
    for(float s:{-1.f,1.f}){
        b.cover({{s*.08f,2.22f,.478f},{s*.14f,2.29f,.459f},{s*.14f,2.15f,.448f},{s*.08f,2.15f,.462f}},.025f,white,.003f);
        b.cover({{s*.14f,2.28f,.465f},{s*.082f,2.19f,.486f},{s*.10f,2.20f,.481f},{s*.15f,2.265f,.468f}},.01f,frame,.001f);
        b.cover({{s*.082f,2.19f,.486f},{s*.085f,2.13f,.466f},{s*.10f,2.16f,.473f},{s*.10f,2.20f,.481f}},.01f,frame,.001f);
    }
    b.cover({{-.074f,2.185f,.49f},{.074f,2.185f,.49f},{.058f,2.07f,.457f},{0,2.045f,.436f},{-.058f,2.07f,.457f}},.05f,red,.006f);
    b.cover({{-.072f,2.88f,.35f},{.072f,2.88f,.35f},{.061f,3.04f,.256f},{-.061f,3.04f,.256f}},.018f,black,.002f);
    b.cover({{-.052f,2.902f,.347f},{.052f,2.902f,.347f},{.043f,3.02f,.278f},{-.043f,3.02f,.278f}},.012f,green,.002f);
    // Exactly TWO yellow blades, with diminishing width and depth (SDEX009).
    for(float s:{-1.f,1.f}){
        if(a)a->fins[s>0].begin=b.m.count;
        struct Section{float x,y,z,w,d;};
        const Section rows[]={{.075f,2.78f,.424f,.17f,.09f},{.30f,2.84f,.40f,.15f,.075f},{.69f,3.21f,.29f,.085f,.04f},{1.02f,3.52f,.22f,.018f,.012f}};
        const auto v=[&](Section r,int k){constexpr float u[]={-1,0,1,0},d[]={0,1,0,-1};return Point{s*(r.x-.70f*u[k]*r.w*.5f),r.y+.71f*u[k]*r.w*.5f,r.z+d[k]*r.d*.5f};};
        for(int row=0;row<3;++row)for(int k=0;k<4;++k)b.face(v(rows[row],k),v(rows[row],(k+1)%4),v(rows[row+1],(k+1)%4),v(rows[row+1],k),gold,{0,0,k<2?1.f:-1.f},true);
        for(int end:{0,3})for(int k=0;k<4;++k)b.face({s*rows[end].x,rows[end].y,rows[end].z},v(rows[end],k),v(rows[end],(k+1)%4),v(rows[end],(k+1)%4),gold,{0,end?1.f:-1.f,0},true);
        if(a)a->fins[s>0].end=b.m.count;
    }
    if(a)a->helmet.end=b.m.count;
}

void armFrame(Builder& b,Part p,float s){b.at(p,{s*.76f,1.92f,0},s*(s<0?.15f:.10f),s<0?-.08f:-.03f);}
Point armAnchor(Builder& b,float s,Point p){armFrame(b,Part::Hands,s);return b.transform(p);}
Point handAnchor(Builder& b,float s){return armAnchor(b,s,{0,-.81f,.10f});}
void arms(Builder& b,bool detail,DestinyAssembly* a){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);b.tube({-.14f,0,0},{.14f,0,0},.10f,.10f,frame,false,8);
        b.shell({{-.20f,.23f,.21f},{.04f,.31f,.28f},{.20f,.28f,.25f},{.27f,.20f,.19f}},blue,10);
        b.cover({{-.23f,.15f,.28f},{.24f,.15f,.28f},{.28f,-.08f,.30f},{.07f,-.22f,.33f},{-.16f,-.18f,.32f}},.045f,blue,.006f);
        b.cover({{s*.17f,.12f,.03f},{s*.43f,.34f,-.02f},{s*.48f,.43f,-.06f},{s*.44f,.14f,.03f},{s*.25f,-.04f,.10f}},.08f,white,.005f);
        if(detail)b.cover({{s*.27f,.15f,.047f},{s*.41f,.28f,.012f},{s*.39f,.15f,.042f},{s*.27f,.055f,.079f}},.01f,black,.002f);
        armFrame(b,Part::Arms,s);b.shell({{-.29f,.12f,.12f},{-.14f,.15f,.15f}},white,10);
        b.tube({-.11f,-.34f,0},{.11f,-.34f,0},.08f,.08f,frame,false,8);
        b.at(Part::Arms,armAnchor(b,s,{0,-.43f,.02f}),s*.10f,-.08f);
        if(a)a->forearms[s>0].begin=b.m.count;
        b.shell({{-.20f,.15f,.16f},{-.07f,.18f,.19f},{.12f,.16f,.17f},{.20f,.12f,.13f}},white,10);
        if(detail)b.cover({{-.10f,.04f,.19f},{.10f,.04f,.19f},{.12f,-.15f,.21f},{-.10f,-.16f,.21f}},.025f,blue,.004f);
        if(a)a->forearms[s>0].end=b.m.count;
        const auto wrist=armAnchor(b,s,{0,-.60f,.04f}),hand=handAnchor(b,s);
        if(a)a->wrists[s>0].begin=b.m.count;
        b.at(Part::Hands);b.tube(wrist,{hand.x,hand.y+.02f,hand.z-.10f},.052f,.052f,frame,false,8);
        if(a)a->wrists[s>0].end=b.m.count;
        if(a)a->palms[s>0].begin=b.m.count;
        b.at(Part::Hands,hand,s*.10f,s<0?.72f:0,-.05f);b.box(0,0,0,.23f,.19f,.18f,frame);
        if(a)a->palms[s>0].end=b.m.count;
    }
}

void prism(Builder& b,const Point* p,size_t n,float depth,uint16_t color){
    Point center{};for(size_t i=0;i<n;++i){center.x+=p[i].x/n;center.y+=p[i].y/n;center.z+=p[i].z/n;}
    const Point rear{center.x,center.y,center.z-depth};
    for(size_t i=0;i<n;++i){const auto q=p[(i+1)%n];const Point a{p[i].x,p[i].y,p[i].z-depth},c{q.x,q.y,q.z-depth};
        b.face(center,p[i],q,q,color,{0,0,1},true);b.face(rear,c,a,a,color,{0,0,-1},true);
        b.face(p[i],a,c,q,color,{p[i].x+q.x-2*center.x,p[i].y+q.y-2*center.y,0},true);
    }
}

void backpackAndWeapons(Builder& b,bool detail,DestinyAssembly* a){
    for(int side=0;side<2;++side){const float s=side?1.f:-1.f;
        b.at(Part::Backpack);if(a)a->wingMounts[side].begin=b.m.count;
        b.tube({s*.18f,1.88f,-.52f},{s*.43f,1.95f,-.69f},.065f,.075f,frame,false,8);
        if(a)a->wingMounts[side].end=b.m.count;
        b.at(Part::Aile);if(a)a->wings[side].begin=b.m.count;
        // Neutral product pose: two red feather lobes fold down from the
        // upright black root, rather than a horizontal aircraft-like plank.
        const Point root[]={{s*.36f,1.79f,-.71f},{s*.39f,2.43f,-.71f},{s*.57f,3.02f,-.71f},{s*.78f,2.88f,-.71f},{s*.99f,2.40f,-.71f},{s*.88f,1.68f,-.71f}};
        prism(b,root,6,.12f,red);
        const Point rootBlack[]={{s*.38f,1.83f,-.699f},{s*.41f,2.42f,-.699f},{s*.58f,2.97f,-.699f},{s*.75f,2.83f,-.699f},{s*.95f,2.39f,-.699f},{s*.85f,1.73f,-.699f}};
        prism(b,rootBlack,6,.008f,0x31a6);
        Point rearRoot[6];for(int i=0;i<6;++i){rearRoot[i]=rootBlack[i];rearRoot[i].z=-.841f;}prism(b,rearRoot,6,.008f,0x31a6);
        const Point lower[]={{s*.48f,2.04f,-.85f},{s*.82f,2.27f,-.85f},{s*1.64f,1.15f,-.85f},{s*1.78f,.88f,-.85f},{s*1.55f,1.00f,-.85f},{s*.65f,1.62f,-.85f}};
        prism(b,lower,6,.075f,red);
        const Point blackFeather[]={{s*.51f,2.04f,-.837f},{s*.78f,2.23f,-.837f},{s*1.49f,1.37f,-.837f},{s*1.54f,1.23f,-.837f},{s*1.27f,1.35f,-.837f},{s*.65f,1.73f,-.837f}};
        prism(b,blackFeather,6,.008f,0x31a6);
        Point rearFeather[6];for(int i=0;i<6;++i){rearFeather[i]=blackFeather[i];rearFeather[i].z=-.936f;}prism(b,rearFeather,6,.008f,0x31a6);
        for(int k=0;k<2;++k){
            const float y=2.45f-k*.28f;
            const Point feather[]={{s*.65f,y,-.86f},{s*.82f,y+.07f,-.86f},{s*(1.52f-k*.18f),y-.27f,-.86f},{s*(1.49f-k*.18f),y-.31f,-.86f},{s*.67f,y-.10f,-.86f}};
            prism(b,feather,5,.045f,red);
        }
        if(detail){const Point tail[]={{s*.57f,1.85f,-.94f},{s*.73f,1.83f,-.94f},{s*1.11f,.89f,-.94f},{s*1.03f,.95f,-.94f}};prism(b,tail,4,.035f,red);}
        if(a)a->wings[side].end=b.m.count;
    }
    // Right-side Arondight and left-side long-range cannon remain visibly
    // connected to the backpack in the neutral official product pose.
    b.at(Part::Backpack);if(a)a->swordMount.begin=b.m.count;
    b.tube({-.18f,1.70f,-.61f},{-.32f,1.62f,-1.08f},.045f,.052f,red,false,8);
    if(a)a->swordMount.end=b.m.count;
    b.at(Part::Sabers);if(a)a->sword.begin=b.m.count;
    b.cover({{-.41f,2.32f,-1.04f},{-.23f,2.32f,-1.04f},{-.25f,.52f,-1.04f},{-.32f,.32f,-1.04f},{-.40f,.54f,-1.04f}},.09f,0x6c7b,.004f);
    b.cover({{-.385f,1.54f,-1.027f},{-.265f,1.54f,-1.027f},{-.28f,.52f,-1.027f},{-.32f,.36f,-1.027f},{-.38f,.54f,-1.027f}},.012f,white,.003f);
    b.tube({-.32f,2.28f,-1.08f},{-.32f,2.83f,-1.08f},.036f,.026f,frame,false,8);
    b.box(-.32f,2.24f,-1.08f,.27f,.08f,.13f,0x6c7b);
    if(a)a->sword.end=b.m.count;
    b.at(Part::Backpack);if(a)a->cannonMount.begin=b.m.count;
    b.tube({.18f,1.70f,-.61f},{.32f,1.65f,-1.08f},.045f,.052f,red,false,8);
    if(a)a->cannonMount.end=b.m.count;
    b.at(Part::Bazooka);if(a)a->cannon.begin=b.m.count;
    b.cover({{.23f,2.74f,-1.035f},{.39f,2.74f,-1.035f},{.43f,1.75f,-1.035f},{.38f,.46f,-1.035f},{.31f,.36f,-1.035f},{.25f,.53f,-1.035f}},.10f,olive,.004f);
    b.box(.32f,1.68f,-1.08f,.21f,.26f,.16f,olive);
    b.box(.31f,2.72f,-1.08f,.16f,.07f,.11f,red);
    if(detail)for(int k=0;k<4;++k)b.box(.31f,2.53f-k*.10f,-1.028f,.07f,.027f,.009f,black);
    if(a)a->cannon.end=b.m.count;
}

void handheld(Builder& b,DestinyAssembly* a){
    const auto right=handAnchor(b,-1);b.at(Part::Rifle,right,-.10f,.72f,-.05f);
    if(a)a->rifle.begin=b.m.count;
    b.box(0,0,.01f,.09f,.25f,.08f,frame);
    b.box(0,.20f,.29f,.14f,.18f,.52f,frame);
    b.box(0,.21f,.67f,.09f,.09f,.40f,black);
    b.tube({0,.21f,.78f},{0,.21f,1.04f},.034f,.022f,frame,true,8);
    b.box(0,.10f,.37f,.11f,.23f,.09f,black);
    b.box(0,.33f,.16f,.10f,.08f,.14f,red);
    if(a)a->rifle.end=b.m.count;
    const auto left=armAnchor(b,1,{.13f,-.54f,.07f});const Point center{left.x+.31f,left.y-.02f,left.z+.23f};
    b.at(Part::Shield);if(a)a->shieldMount.begin=b.m.count;
    b.tube(left,center,.038f,.042f,frame,false,8);
    if(a)a->shieldMount.end=b.m.count;
    b.at(Part::Shield,center,.10f,0,.42f);if(a)a->shield.begin=b.m.count;
    b.cover({{-.16f,.30f,.04f},{.16f,.30f,.04f},{.22f,.02f,.04f},{.13f,-.24f,.04f},{0,-.31f,.04f},{-.13f,-.24f,.04f},{-.22f,.02f,.04f}},.10f,blue,.008f);
    b.cover({{-.10f,.11f,.065f},{.10f,.11f,.065f},{.13f,-.03f,.065f},{.06f,-.14f,.065f},{-.06f,-.14f,.065f},{-.13f,-.03f,.065f}},.018f,frame,.003f);
    b.cover({{-.072f,.082f,.09f},{.072f,.082f,.09f},{.097f,-.03f,.09f},{.044f,-.108f,.09f},{-.044f,-.108f,.09f},{-.097f,-.03f,.09f}},.012f,gold,.003f);
    if(a)a->shield.end=b.m.count;
}
} // namespace

void buildDestinyGundam(Mesh& mesh,BuildOptions options,DestinyStage stage,DestinyAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly={};
    if(stage==DestinyStage::Blockout)options.gray=true;
    Builder b{mesh,options};const bool final=stage==DestinyStage::Final;
    legs(b,final);body(b,final,assembly);head(b,stage!=DestinyStage::Blockout,assembly);arms(b,final,assembly);
    if(options.equipment&&stage!=DestinyStage::Blockout){backpackAndWeapons(b,final,assembly);handheld(b,assembly);}
}
} // namespace gundam_museum
