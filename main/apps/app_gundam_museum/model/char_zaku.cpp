#include "char_zaku.h"
#include "sd_model_builder.h"
#include "sd_curved_parts.h"

namespace gundam_museum {
namespace {
using sd_model::Builder;using sd_model::Ring;
constexpr uint16_t salmon=0xdb6e,pink=0xe3cf,deepRed=0x99c7;
constexpr uint16_t frame=0x39e7,black=0x1082,eye=0xf9d3;
constexpr uint16_t purple=0x8019,gold=0xfec0;

void legs(Builder& b){
    for(float s:{-1.f,1.f}){
        const Point hip{s*.29f,1.12f,s<0?.04f:0};const float spread=s*(s<0?.24f:.20f);
        b.at(Part::Feet,{hip.x+s*.16f,.0f,hip.z},0,0,s*(s<0?.25f:.20f));
        // The sole and armor meet at a shared ring: no intersecting toe box.
        b.shell({{.025f,.34f,.43f,.12f},{.10f,.37f,.47f,.15f},{.20f,.35f,.45f,.14f}},black,16,true);
        b.shell({{.20f,.35f,.45f,.14f},{.25f,.31f,.39f,.11f},{.30f,.23f,.27f,.03f}},salmon,16);
        b.at(Part::Thighs,hip,spread);b.shell({{-.31f,.19f,.18f},{-.13f,.22f,.21f},{.04f,.20f,.18f}},frame,12);
        b.at(Part::Knees,hip,spread);b.tube({-.10f,-.36f,0},{.10f,-.36f,0},.09f,.09f,frame,false,8);
        b.at(Part::Shins,hip,spread);b.shell({{-.77f,.35f,.32f},{-.70f,.34f,.30f},{-.61f,.29f,.26f},{-.50f,.28f,.28f},{-.37f,.25f,.27f},{-.29f,.20f,.21f}},pink,16);
        b.at(Part::Shins,{hip.x+s*.16f,.38f,hip.z},0,0,s*(s<0?.25f:.20f));
        b.cover({{-.09f,.04f,.30f},{.09f,.04f,.30f},{.10f,-.07f,.34f},{-.10f,-.07f,.34f}},.025f,salmon,.008f);
    }
}
void torso(Builder& b,bool detail,ZakuAssembly* a){
    b.at(Part::Waist);b.shell({{1.04f,.35f,.27f},{1.24f,.39f,.30f},{1.39f,.35f,.28f}},deepRed,12);
    for(float s:{-1.f,1.f})b.cover({{s*.05f,1.36f,.31f},{s*.34f,1.36f,.30f},{s*.45f,1.06f,.34f},{s*.10f,1.02f,.36f}},.06f,pink,.005f);
    b.cover({{-.10f,1.38f,.34f},{.10f,1.38f,.34f},{.11f,1.04f,.39f},{0,.99f,.40f},{-.11f,1.04f,.39f}},.05f,salmon,.006f);
    b.at(Part::Torso);b.shell({{1.36f,.36f,.28f},{1.58f,.47f,.36f},{1.89f,.55f,.39f},{2.01f,.43f,.31f}},deepRed,12);
    // Black rounded central chest with salmon side armor is a primary Zaku
    // identity split visible in every official front/quarter product view.
    b.at(Part::Torso,{0,0,.25f});
    b.shell({{1.47f,.20f,.15f},{1.54f,.29f,.22f},{1.67f,.33f,.26f},{1.81f,.32f,.25f},{1.94f,.26f,.18f}},black,16);
    b.at(Part::Torso);
    for(float s:{-1.f,1.f}){
        b.cover({{s*.26f,1.94f,.38f},{s*.48f,1.88f,.34f},{s*.43f,1.58f,.42f},{s*.23f,1.53f,.48f}},.055f,salmon,.007f,true);
    }
    if(detail){
        b.at(Part::Torso);if(a)a->waistHose.begin=b.m.count;
        for(float s:{-1.f,1.f})sd_curved::hose(b,{{0,1.43f,.51f},{s*.30f,1.43f,.48f},{s*.48f,1.43f,.20f},{s*.40f,1.44f,-.28f}},.066f,salmon,12,6);
        if(a)a->waistHose.end=b.m.count;
        b.box(0,1.82f,.505f,.065f,.065f,.016f,frame,true);
    }
    b.shell({{1.97f,.17f,.15f},{2.08f,.20f,.17f}},frame,10);
    b.at(Part::Backpack,{0,0,-.38f});b.shell({{1.46f,.25f,.10f},{1.55f,.30f,.17f},{1.84f,.30f,.17f},{1.96f,.22f,.11f}},black,12);
    b.at(Part::Backpack);
    for(float s:{-1.f,1.f})b.tube({s*.14f,1.47f,-.48f},{s*.14f,1.28f,-.58f},.07f,.09f,frame,true,10);
}
void head(Builder& b,bool detail,ZakuAssembly* a){
    b.at(Part::Head);
    b.shell({{2.52f,.55f,.48f,-.05f},{2.64f,.55f,.48f,-.05f},{2.76f,.50f,.44f,-.055f},{2.87f,.40f,.36f,-.06f},{2.95f,.27f,.24f,-.06f},{3.00f,.05f,.05f,-.06f}},pink,20);
    sd_curved::arc(b,{{2.23f,.50f,.44f,-.04f},{2.40f,.56f,.48f,-.05f},{2.54f,.55f,.48f,-.05f}},pink,1.22f,2*sd_model::pi-1.22f,14);
    // The visor is a recessed structural band; the eye is a separate lens.
    sd_curved::arc(b,{{2.34f,.51f,.445f,-.035f},{2.53f,.535f,.46f,-.04f}},black,-1.20f,1.20f,12);
    b.tube({0,2.435f,.416f},{0,2.435f,.438f},.062f,.060f,eye,false,14);
    b.cover({{-.21f,2.30f,.51f},{.21f,2.30f,.51f},{.20f,2.19f,.55f},{.09f,2.11f,.54f},{-.09f,2.11f,.54f},{-.20f,2.19f,.55f}},.13f,salmon,.008f);
    b.cover({{-.12f,2.225f,.566f},{.12f,2.225f,.566f},{.075f,2.155f,.572f},{-.075f,2.155f,.572f}},.025f,black,.003f);
    if(detail){
        if(a)a->headHose.begin=b.m.count;
        for(float s:{-1.f,1.f})sd_curved::hose(b,{{s*.18f,2.295f,.53f},{s*.43f,2.29f,.48f},{s*.57f,2.32f,.17f},{s*.50f,2.38f,-.24f},{s*.30f,2.40f,-.46f}},.077f,salmon,15,6);
        if(a)a->headHose.end=b.m.count;
        for(float s:{-1.f,1.f}){
            b.at(Part::Head);
            b.tube({s*.46f,2.48f,.10f},{s*.58f,2.48f,.10f},.10f,.10f,deepRed,false,12);
        }
    }
    // Official SDCS blade antenna: broad at the helmet, shorter than the old
    // needle and visibly swept toward the face in side view.
    b.at(Part::Head);
    b.cover({{-.080f,2.88f,.02f},{.080f,2.88f,.02f},{.025f,3.31f,.13f},{-.025f,3.31f,.13f}},.095f,salmon,.006f,true);
}
void armFrame(Builder& b,Part p,float s){b.at(p,{s*.78f,1.90f,0},s*(s<0?.16f:.10f),s<0?-.08f:0);}
Point anchor(Builder& b,float s,Point p){armFrame(b,Part::Hands,s);return b.transform(p);}
Point handAnchor(Builder& b,float s){return anchor(b,s,{0,-1.02f,.16f});}
void arms(Builder& b,bool detail,ZakuAssembly* a){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);b.tube({-.15f,0,0},{.15f,0,0},.11f,.11f,frame,false,8);
        if(s<0){ // right shoulder rectangular shield
            if(a)a->rightShield.begin=b.m.count;
            // Keep the rear-facing shield visibly attached to the shoulder;
            // the connector is part of the shield assembly and its collision gate.
            b.at(Part::Shield);
            b.tube({-.88f,1.95f,-.08f},{-1.08f,1.96f,-.21f},.045f,.050f,frame,false,8);
            b.at(Part::Shield,{s*1.24f,1.96f,-.25f},.05f,0,s*.08f);
            b.box(0,-.02f,0,.58f,1.04f,.13f,pink,true);
            b.box(0,.01f,.075f,.43f,.83f,.025f,salmon,true);
            b.box(.22f,-.32f,-.10f,.11f,.25f,.10f,frame,true);
            if(a)a->rightShield.end=b.m.count;
        }else{ // left spiked pauldron
            b.at(Part::Shoulders,{s*.80f,2.13f,0},.10f);b.shell({{-.28f,.36f,.35f},{-.20f,.38f,.36f},{.01f,.36f,.35f},{.16f,.31f,.30f},{.26f,.21f,.22f},{.29f,.10f,.12f}},salmon,16);
            if(a)a->leftSpikes.begin=b.m.count;
            b.tube({.12f,.03f,.28f},{.13f,.03f,.36f},.145f,.13f,pink,false,12);
            const std::pair<Point,Point> spikes[]={{{.25f,.13f,0},{.53f,.37f,0}},{{.0f,.21f,-.08f},{.12f,.45f,-.18f}},{{.26f,-.10f,-.15f},{.49f,-.10f,-.28f}}};
            for(const auto& spike:spikes)b.tube(spike.first,spike.second,.095f,.012f,pink,false,10,false,true);
            if(a)a->leftSpikes.end=b.m.count;
        }
        armFrame(b,Part::Arms,s);b.shell({{-.30f,.14f,.15f},{-.08f,.17f,.18f}},salmon,10);b.tube({0,-.12f,0},{0,-.54f,0},.13f,.15f,frame,false,10);
        b.at(Part::Arms,anchor(b,s,{0,-.63f,.07f}),s<0?-.16f:.10f,-.22f);b.shell({{-.22f,.17f,.18f},{-.13f,.21f,.22f},{.06f,.22f,.23f},{.20f,.17f,.18f}},pink,14);
        const auto wrist=anchor(b,s,{0,-.84f,.08f}),hand=handAnchor(b,s);b.at(Part::Hands);b.tube(wrist,{hand.x,hand.y+.015f,hand.z-.09f},.055f,.055f,frame,false,8);
        if(a)a->palms[s>0].begin=b.m.count;
        b.at(Part::Hands,hand,s<0?-.16f:.10f,s<0?.78f:0);b.box(0,0,0,.25f,.20f,.22f,salmon);
        if(a)a->palms[s>0].end=b.m.count;
    }
    if(detail){b.at(Part::Shoulders);b.box(-.72f,1.93f,-.18f,.12f,.31f,.12f,frame);}
}
void equipment(Builder& b,ZakuAssembly* a){
    // Zaku machine gun in the right fist, including drum and hollow muzzle.
    const Point grip=handAnchor(b,-1);b.at(Part::Rifle,grip,-.16f,.78f,-.12f);
    if(a)a->rifle.begin=b.m.count;
    b.box(0,.02f,.05f,.10f,.25f,.10f,frame);b.box(0,.21f,.28f,.16f,.16f,.58f,black);
    b.tube({0,.21f,.46f},{0,.21f,1.12f},.065f,.040f,frame,true,12);
    if(a)a->drum.begin=b.m.count;
    b.tube({0,.30f,.27f},{0,.39f,.27f},.23f,.23f,frame,false,16);
    if(a)a->drum.end=b.m.count;
    for(int i=0;i<4;++i)b.tube({0,.21f,.58f+i*.115f},{0,.21f,.605f+i*.115f},.070f-i*.006f,.070f-i*.006f,black,false,10);
    b.box(0,.32f,.50f,.06f,.09f,.10f,frame);
    if(a)a->rifle.end=b.m.count;
    // Stowed heat hawk remains visible without crossing the skirt.
    b.at(Part::Waist);b.tube({.38f,1.14f,-.18f},{.47f,1.12f,-.28f},.055f,.055f,frame,false,8);
    b.at(Part::Sabers,{.54f,1.08f,-.34f},-.18f,0,.15f);
    if(a)a->heatHawk.begin=b.m.count;
    b.tube({0,-.25f,0},{0,.28f,0},.042f,.042f,purple,false,8);
    // Curved axe cheek with an outer cutting rim, not a solid gold rectangle.
    for(int i=0;i<6;++i){
        const float t0=-.85f+i*1.7f/6,t1=-.85f+(i+1)*1.7f/6;
        const auto p=[](float t,float r,float z){return Point{r*std::cos(t),.22f+r*std::sin(t),z};};
        b.face(p(t0,.09f,.03f),p(t0,.30f,.03f),p(t1,.30f,.03f),p(t1,.09f,.03f),purple,{0,0,1},true);
        b.face(p(t0,.30f,.03f),p(t0,.35f,.03f),p(t1,.35f,.03f),p(t1,.30f,.03f),gold,{0,0,1},true);
        b.face(p(t0,.35f,.03f),p(t0,.35f,-.025f),p(t1,.35f,-.025f),p(t1,.35f,.03f),gold,{1,0,0},true);
        b.face(p(t1,.09f,-.025f),p(t1,.35f,-.025f),p(t0,.35f,-.025f),p(t0,.09f,-.025f),purple,{0,0,-1},true);
    }
    b.box(.075f,.22f,0,.16f,.07f,.07f,purple,true);
    if(a)a->heatHawk.end=b.m.count;
}
} // namespace

void buildCharZaku(Mesh& mesh,BuildOptions options,ZakuStage stage,ZakuAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly={};if(stage==ZakuStage::Blockout)options.gray=true;
    Builder b{mesh,options};legs(b);torso(b,stage==ZakuStage::Final,assembly);head(b,stage!=ZakuStage::Blockout,assembly);arms(b,stage==ZakuStage::Final,assembly);if(options.equipment)equipment(b,assembly);
}
} // namespace gundam_museum
