#include "char_zaku.h"
#include "sd_model_builder.h"
#include "sd_curved_parts.h"

namespace gundam_museum {
namespace {
using sd_model::Builder;using sd_model::Ring;
constexpr uint16_t salmon=0xdb6e,pink=0xe3cf,deepRed=0x99c7;
constexpr uint16_t frame=0x39e7,black=0x1082,eye=0xf9d3;
constexpr uint16_t purple=0x8019,gold=0xfec0;
constexpr float headLowering=.14f;

// SDCS #14 MS-06S Char's Zaku II (4573102588623). SD frame only; CS frame
// parts on the same manual pages are not modeled. Mapping: C17 boot, B2 thigh
// /shin, B1 crimson chest and skirts, C3 black chest, C4 backpack, B2-13 right
// shield-as-pauldron, B2-5 spiked dome, B2 helmet/hose/antenna, C Zaku MG.

void legs(Builder& b){
    const auto armor=[&](std::initializer_list<Ring> rings,uint16_t color,int count){
        const auto p=[&](Ring r,int i){const float a=2*sd_model::pi*i/count;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
        for(auto it=rings.begin()+1;it!=rings.end();++it)for(int i=0;i<count;++i){
            const auto lo=*(it-1),hi=*it;const auto u=p(lo,i),v=p(lo,i+1);
            b.face(u,v,p(hi,i+1),p(hi,i),color,{u.x+v.x,0,u.z+v.z-2*lo.z},true);
        }
        for(int end=0;end<2;++end){const auto r=end?*(rings.end()-1):*rings.begin();
            for(int i=1;i<count-1;++i)b.face(p(r,0),p(r,i),p(r,i+1),p(r,i+1),color,{0,end?1.f:-1.f,0},true);}
    };
    for(float s:{-1.f,1.f}){
        const Point hip{s*.28f,1.14f,s<0?.04f:0};const float spread=s*(s<0?.22f:.18f);
        b.at(Part::Feet,{hip.x+s*.14f,.0f,hip.z},0,0,s*(s<0?.22f:.18f));
        armor({{.025f,.28f,.36f,.06f},{.12f,.26f,.34f,.06f},{.24f,.22f,.28f,.04f},{.34f,.16f,.18f,.00f}},black,12);
        b.at(Part::Thighs,hip,spread);
        b.tube({0,.02f,0},{0,-.22f,0},.10f,.09f,frame,false,8);
        armor({{-.04f,.19f,.17f},{-.20f,.22f,.20f},{-.36f,.18f,.16f}},salmon,12);
        b.at(Part::Knees,hip,spread);b.tube({-.08f,-.40f,0},{.08f,-.40f,0},.085f,.085f,frame,false,8);
        b.at(Part::Shins,hip,spread);
        armor({{-.44f,.15f,.15f},{-.58f,.18f,.17f},{-.72f,.22f,.20f},{-.80f,.24f,.22f}},salmon,12);
    }
}
void torso(Builder& b,bool detail,ZakuAssembly* a){
    b.at(Part::Waist);b.shell({{1.06f,.33f,.25f},{1.24f,.38f,.29f},{1.38f,.34f,.27f}},deepRed,10);
    for(float s:{-1.f,1.f}){
        const int side=s>0;
        // B1-3 front skirt: one crimson shell authored as left/right halves.
        b.at(Part::Waist);
        if(a)a->skirts.front[side].begin=b.m.count;
        b.cover({{s*.05f,1.38f,.42f},{s*.42f,1.36f,.40f},{s*.48f,1.04f,.46f},{s*.10f,.94f,.48f}},.08f,deepRed,.006f);
        if(a)a->skirts.front[side].end=b.m.count;
        b.at(Part::Waist,{},0,0,sd_model::pi);
        const int rearSide=s<0;
        if(a)a->skirts.rear[rearSide].begin=b.m.count;
        b.cover({{s*.05f,1.36f,.36f},{s*.40f,1.34f,.36f},{s*.44f,1.04f,.42f},{s*.10f,.94f,.44f}},.08f,deepRed,.006f);
        if(a)a->skirts.rear[rearSide].end=b.m.count;
    }
    // B1-2 crimson wrap; C3 black chest sits on the front, not inside the shell.
    b.at(Part::Torso);b.shell({{1.38f,.34f,.27f},{1.56f,.48f,.36f},{1.80f,.54f,.40f},{1.98f,.42f,.32f}},deepRed,12);
    b.at(Part::Torso);b.box(0,1.72f,.50f,.46f,.30f,.10f,black,true);
    b.at(Part::Torso);b.shell({{1.96f,.20f,.16f},{2.08f,.22f,.18f}},salmon,10);
    if(detail){
        b.at(Part::Torso);if(a)a->waistHose.begin=b.m.count;
        for(float s:{-1.f,1.f})sd_curved::hose(b,{{s*.22f,1.46f,.42f},{s*.38f,1.48f,.28f},{s*.48f,1.46f,.04f},{s*.40f,1.44f,-.28f}},.060f,salmon,10,6);
        if(a)a->waistHose.end=b.m.count;
        b.box(0,1.82f,.505f,.065f,.065f,.016f,frame,true);
    }
    // C4 backpack: larger two-nozzle pack, not a small cap.
    b.at(Part::Backpack,{0,0,-.30f});b.shell({{1.44f,.30f,.16f},{1.56f,.36f,.24f},{1.84f,.36f,.24f},{1.98f,.26f,.14f}},black,10);
    b.at(Part::Backpack);
    for(float s:{-1.f,1.f})b.tube({s*.13f,1.48f,-.52f},{s*.13f,1.28f,-.62f},.08f,.10f,frame,true,8);
}
void head(Builder& b,bool detail,ZakuAssembly* a){
    b.at(Part::Head);
    if(a)a->headMount.begin=b.m.count;
    b.tube({0,2.02f,-.03f},{0,2.31f-headLowering,-.03f},.155f,.17f,frame,false,8);
    if(a)a->headMount.end=b.m.count;
    b.at(Part::Head,{0,-headLowering,0});
    if(a)a->headBody.begin=b.m.count;
    if(a)a->headSocket.begin=b.m.count;
    const auto cupPoint=[](int i,float y){
        const float angle=1.22f+(2*sd_model::pi-2.44f)*i/11;
        return Point{std::sin(angle)*.50f,y,-.04f+std::cos(angle)*.44f};
    };
    for(int i=0;i<12;++i){
        const int j=(i+1)%12;const auto p=cupPoint(i,2.23f),q=cupPoint(j,2.23f);
        const auto u=cupPoint(i,2.255f),v=cupPoint(j,2.255f);
        b.face({0,2.23f,-.04f},q,p,p,frame,{0,-1,0},true);
        b.face({0,2.255f,-.04f},u,v,v,frame,{0,1,0},true);
        b.face(p,q,v,u,frame,{p.x+q.x,0,p.z+q.z+.08f},true);
    }
    if(a)a->headSocket.end=b.m.count;
    b.shell({{2.52f,.55f,.48f,-.05f},{2.64f,.55f,.48f,-.05f},{2.76f,.50f,.44f,-.055f},{2.87f,.40f,.36f,-.06f},{2.95f,.27f,.24f,-.06f},{3.00f,.05f,.05f,-.06f}},pink,16);
    sd_curved::arc(b,{{2.23f,.50f,.44f,-.04f},{2.40f,.56f,.48f,-.05f},{2.54f,.55f,.48f,-.05f}},pink,1.22f,2*sd_model::pi-1.22f,12);
    sd_curved::arc(b,{{2.34f,.51f,.445f,-.035f},{2.53f,.535f,.46f,-.04f}},black,-1.20f,1.20f,10);
    b.tube({0,2.435f,.416f},{0,2.435f,.438f},.062f,.060f,eye,false,12);
    b.cover({{-.21f,2.30f,.51f},{.21f,2.30f,.51f},{.20f,2.19f,.55f},{.09f,2.11f,.54f},{-.09f,2.11f,.54f},{-.20f,2.19f,.55f}},.13f,salmon,.008f);
    b.cover({{-.12f,2.225f,.566f},{.12f,2.225f,.566f},{.075f,2.155f,.572f},{-.075f,2.155f,.572f}},.025f,black,.003f);
    if(detail){
        if(a)a->headHose.begin=b.m.count;
        for(float s:{-1.f,1.f})sd_curved::hose(b,{{s*.18f,2.295f,.53f},{s*.43f,2.29f,.48f},{s*.57f,2.32f,.17f},{s*.50f,2.38f,-.24f},{s*.30f,2.40f,-.46f}},.077f,salmon,12,6);
        if(a)a->headHose.end=b.m.count;
        for(float s:{-1.f,1.f}){
            b.at(Part::Head,{0,-headLowering,0});
            b.tube({s*.46f,2.48f,.10f},{s*.58f,2.48f,.10f},.10f,.10f,deepRed,false,10);
        }
    }
    if(a)a->headBody.end=b.m.count;
    b.at(Part::Head,{0,-headLowering,0});
    if(a)a->antenna.begin=b.m.count;
    // B2 blade antenna: broad root, short, swept forward. Not a tall needle.
    b.cover({{-.090f,2.90f,.00f},{.090f,2.90f,.00f},{.032f,3.20f,.14f},{-.032f,3.20f,.14f}},.10f,salmon,.006f,true);
    if(a)a->antenna.end=b.m.count;
}
void armFrame(Builder& b,Part p,float s){b.at(p,{s*.78f,1.90f,0},s*(s<0?.16f:.10f),s<0?-.08f:0);}
Point anchor(Builder& b,float s,Point p){armFrame(b,Part::Hands,s);return b.transform(p);}
void forearmFrame(Builder& b,Part p,float s){b.at(p,anchor(b,s,{0,-.63f,.07f}),s<0?-.16f:.10f,-.22f);}
Point handAnchor(Builder& b,float s){forearmFrame(b,Part::Hands,s);return b.transform({0,s<0?-.38f:-.26f,.02f});}
void handFrame(Builder& b,Part p,float s){b.at(p,handAnchor(b,s),s<0?-.16f:.10f,s<0?.78f:-.22f);}
void leftPauldron(Builder& b){
    const Ring rings[]={{-.28f,.36f,.35f},{-.20f,.38f,.36f},{.01f,.36f,.35f},{.16f,.31f,.30f},{.26f,.21f,.22f},{.29f,.10f,.12f}};
    const auto point=[](Ring r,int i){
        const float angle=2*sd_model::pi*i/12,x=std::sin(angle)*r.w;
        return Point{x<0?x*.35f:x,r.y,std::cos(angle)*r.d};
    };
    for(size_t row=1;row<6;++row)for(int i=0;i<12;++i){
        const auto p=point(rings[row-1],i),q=point(rings[row-1],i+1);
        b.face(p,q,point(rings[row],i+1),point(rings[row],i),salmon,{p.x+q.x,0,p.z+q.z});
    }
    for(int end=0;end<2;++end)for(int i=1;i<11;++i){
        const auto r=rings[end?5:0];
        b.face(point(r,0),point(r,i),point(r,i+1),point(r,i+1),salmon,{0,end?1.f:-1.f,0});
    }
}
void rightShield(Builder& b,ZakuAssembly* a){
    // B2-13 is the right shoulder armor: a thick rounded slab on the joint,
    // not a thin plate hung beside the arm on a long rod.
    if(a)a->rightShieldMount.begin=b.m.count;
    b.at(Part::Shield);
    b.tube({-.82f,1.92f,.00f},{-1.08f,1.90f,-.04f},.050f,.055f,frame,false,8);
    if(a)a->rightShieldMount.end=b.m.count;
    if(a)a->rightShield.begin=b.m.count;
    b.at(Part::Shield,{-1.24f,1.92f,-.08f},.03f,-.02f,-.06f);
    b.box(0,0,0,.50f,1.04f,.28f,salmon,true);
    if(a)a->rightShield.end=b.m.count;
}
void arms(Builder& b,bool detail,ZakuAssembly* a){
    for(float s:{-1.f,1.f}){
        const int side=s>0;
        b.at(Part::Arms);if(a)a->armMounts[side].begin=b.m.count;
        b.tube({s*.44f,1.90f,0},{s*.78f,1.90f,0},.115f,.115f,frame,false,8);
        if(a)a->armMounts[side].end=b.m.count;
        armFrame(b,Part::Shoulders,s);b.tube({-.15f,0,0},{.15f,0,0},.11f,.11f,frame,false,8);
        if(s<0)rightShield(b,a);
        else{
            b.at(Part::Shoulders,{s*.80f,2.13f,0},.10f);leftPauldron(b);
            if(a)a->leftSpikes.begin=b.m.count;
            b.tube({.14f,.02f,.24f},{.14f,.02f,.38f},.155f,.13f,pink,false,10);
            const std::pair<Point,Point> spikes[]={{{.25f,.13f,0},{.53f,.37f,0}},{{.0f,.21f,-.08f},{.12f,.45f,-.18f}},{{.26f,-.10f,-.15f},{.49f,-.10f,-.28f}}};
            for(const auto& spike:spikes)b.tube(spike.first,spike.second,.095f,.012f,pink,false,8,false,true);
            if(a)a->leftSpikes.end=b.m.count;
        }
        armFrame(b,Part::Arms,s);b.shell({{-.28f,.16f,.16f},{-.08f,.18f,.18f},{.10f,.16f,.16f}},salmon,8);b.tube({0,-.10f,0},{0,-.52f,0},.12f,.14f,frame,false,8);
        forearmFrame(b,Part::Arms,s);
        if(a)a->forearms[s>0].begin=b.m.count;
        b.shell({{-.22f,.17f,.18f},{-.13f,.21f,.22f},{.06f,.22f,.23f},{.20f,.17f,.18f}},pink,12);
        if(a)a->forearms[s>0].end=b.m.count;
        forearmFrame(b,Part::Arms,s);const auto cuff=b.transform({0,-.16f,0});
        handFrame(b,Part::Hands,s);const auto socket=b.transform({0,0,-.09f});
        if(a)a->wrists[s>0].begin=b.m.count;
        b.at(Part::Hands);b.tube(cuff,socket,.055f,.055f,frame,false,8);
        if(a)a->wrists[s>0].end=b.m.count;
        if(a)a->palms[s>0].begin=b.m.count;
        handFrame(b,Part::Hands,s);b.box(0,s<0?0.f:-.08f,0,.25f,.20f,.22f,salmon);
        if(a)a->palms[s>0].end=b.m.count;
    }
    (void)detail;
}
void equipment(Builder& b,ZakuAssembly* a){
    handFrame(b,Part::Rifle,-1);
    if(a)a->rifle.begin=b.m.count;
    if(a)a->rifleGrip.begin=b.m.count;
    b.box(0,0,.06f,.10f,.18f,.16f,frame);
    if(a)a->rifleGrip.end=b.m.count;
    b.box(0,.18f,.24f,.14f,.15f,.42f,black);
    b.tube({0,.18f,.42f},{0,.18f,1.08f},.055f,.038f,frame,true,10);
    if(a)a->drum.begin=b.m.count;
    // Official drum is a side disk with a cross, not a pancake on top of the receiver.
    b.tube({-.10f,.30f,.40f},{-.18f,.30f,.40f},.16f,.16f,frame,false,14);
    b.box(-.185f,.30f,.40f,.02f,.18f,.04f,black,true);
    b.box(-.185f,.30f,.40f,.02f,.04f,.18f,black,true);
    if(a)a->drum.end=b.m.count;
    for(int i=0;i<4;++i)b.tube({0,.18f,.52f+i*.12f},{0,.18f,.545f+i*.12f},.062f-i*.004f,.062f-i*.004f,black,false,8);
    b.box(0,.32f,.38f,.05f,.09f,.10f,frame);
    if(a)a->rifle.end=b.m.count;
    b.at(Part::Waist);if(a)a->heatHawkMount.begin=b.m.count;
    b.tube({.38f,1.14f,-.18f},{.47f,1.12f,-.28f},.055f,.055f,frame,false,8);
    b.tube({.47f,1.12f,-.28f},{.551f,1.139f,-.34f},.052f,.046f,frame,false,8);
    if(a)a->heatHawkMount.end=b.m.count;
    b.at(Part::Sabers,{.58f,1.04f,-.38f},-.16f,0,.18f);
    if(a)a->heatHawk.begin=b.m.count;
    b.tube({0,-.32f,0},{0,.34f,0},.042f,.042f,purple,false,8);
    for(int i=0;i<6;++i){
        const float t0=-.90f+i*1.8f/6,t1=-.90f+(i+1)*1.8f/6;
        const auto p=[](float t,float r,float z){return Point{r*std::cos(t),.26f+r*std::sin(t),z};};
        b.face(p(t0,.10f,.03f),p(t0,.20f,.03f),p(t1,.20f,.03f),p(t1,.10f,.03f),purple,{0,0,1},true);
        b.face(p(t0,.20f,.03f),p(t0,.42f,.03f),p(t1,.42f,.03f),p(t1,.20f,.03f),gold,{0,0,1},true);
        b.face(p(t0,.42f,.03f),p(t0,.42f,-.03f),p(t1,.42f,-.03f),p(t1,.42f,.03f),gold,{1,0,0},true);
        b.face(p(t1,.10f,-.03f),p(t1,.42f,-.03f),p(t0,.42f,-.03f),p(t0,.10f,-.03f),purple,{0,0,-1},true);
    }
    b.box(.08f,.26f,0,.18f,.08f,.08f,purple,true);
    if(a)a->heatHawk.end=b.m.count;
}
} // namespace

void buildCharZaku(Mesh& mesh,BuildOptions options,ZakuStage stage,ZakuAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly={};if(stage==ZakuStage::Blockout)options.gray=true;
    Builder b{mesh,options};legs(b);torso(b,stage==ZakuStage::Final,assembly);head(b,stage!=ZakuStage::Blockout,assembly);arms(b,stage==ZakuStage::Final,assembly);if(options.equipment)equipment(b,assembly);
}
} // namespace gundam_museum
