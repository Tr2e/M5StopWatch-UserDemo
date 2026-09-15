#include "char_zaku.h"
#include "sd_model_builder.h"

namespace gundam_museum {
namespace {
using sd_model::Builder;using sd_model::Ring;
constexpr uint16_t salmon=0xeb0c,pink=0xf34f,deepRed=0x99e7;
constexpr uint16_t frame=0x39e7,black=0x1082,eye=0xf9d3,metal=0x7bef;
constexpr uint16_t purple=0x8019,gold=0xfec0;

void legs(Builder& b){
    for(float s:{-1.f,1.f}){
        const Point hip{s*.29f,1.12f,s<0?.04f:0};const float spread=s*(s<0?.24f:.20f);
        b.at(Part::Feet,{hip.x+s*.16f,.0f,hip.z},0,0,s*(s<0?.25f:.20f));
        b.shell({{.025f,.32f,.40f,.12f},{.11f,.36f,.44f,.15f},{.23f,.30f,.36f,.12f},{.30f,.22f,.25f,.02f}},deepRed,12);
        // The SDCS kit retains the black Zaku toe and sole as a large read at
        // native size, rather than reducing the foot to a single red block.
        b.box(0,.105f,.52f,.52f,.16f,.24f,black,true);
        b.at(Part::Thighs,hip,spread);b.shell({{-.34f,.16f,.16f},{-.13f,.20f,.19f},{.04f,.17f,.16f}},salmon,10);
        b.at(Part::Knees,hip,spread);b.tube({-.10f,-.36f,0},{.10f,-.36f,0},.09f,.09f,frame,false,8);
        b.at(Part::Shins,hip,spread);b.shell({{-.72f,.22f,.21f},{-.55f,.27f,.27f,-.02f},{-.31f,.19f,.19f,-.02f}},pink,12);
        b.at(Part::Shins,{hip.x+s*.16f,.38f,hip.z},0,0,s*(s<0?.25f:.20f));
        b.cover({{-.22f,.13f,.25f},{.22f,.13f,.25f},{.25f,-.09f,.30f},{-.25f,-.09f,.30f}},.045f,salmon,.008f);
    }
}
void torso(Builder& b,bool detail,ZakuAssembly* a){
    b.at(Part::Waist);b.shell({{1.04f,.35f,.27f},{1.24f,.39f,.30f},{1.39f,.35f,.28f}},deepRed,12);
    for(float s:{-1.f,1.f})b.cover({{s*.05f,1.36f,.31f},{s*.34f,1.36f,.30f},{s*.45f,1.06f,.34f},{s*.10f,1.02f,.36f}},.06f,pink,.005f);
    b.cover({{-.10f,1.38f,.34f},{.10f,1.38f,.34f},{.11f,1.04f,.39f},{0,.99f,.40f},{-.11f,1.04f,.39f}},.05f,salmon,.006f);
    b.at(Part::Torso);b.shell({{1.36f,.36f,.28f},{1.58f,.47f,.36f},{1.89f,.55f,.39f},{2.01f,.43f,.31f}},deepRed,12);
    // Black rounded central chest with salmon side armor is a primary Zaku
    // identity split visible in every official front/quarter product view.
    b.cover({{-.27f,1.91f,.40f},{.27f,1.91f,.40f},{.31f,1.69f,.48f},{.23f,1.51f,.50f},{-.23f,1.51f,.50f},{-.31f,1.69f,.48f}},.08f,black,.009f,true);
    for(float s:{-1.f,1.f}){
        b.cover({{s*.26f,1.94f,.38f},{s*.48f,1.88f,.34f},{s*.43f,1.58f,.42f},{s*.23f,1.53f,.48f}},.055f,salmon,.007f,true);
    }
    if(detail){
        b.at(Part::Torso);if(a)a->waistHose.begin=b.m.count;
        for(float s:{-1.f,1.f})b.polyTube({{0,1.48f,.52f},{s*.29f,1.47f,.48f},{s*.48f,1.36f,.28f},{s*.42f,1.24f,.02f}},.065f,salmon,10);
        if(a)a->waistHose.end=b.m.count;
        b.box(0,1.78f,.505f,.10f,.10f,.025f,metal,true);
    }
    b.shell({{1.97f,.17f,.15f},{2.08f,.20f,.17f}},frame,10);
    b.at(Part::Backpack);b.box(0,1.72f,-.38f,.48f,.55f,.18f,black);
    for(float s:{-1.f,1.f})b.tube({s*.14f,1.47f,-.48f},{s*.14f,1.28f,-.58f},.07f,.09f,frame,true,10);
}
void head(Builder& b,bool detail,ZakuAssembly* a){
    b.at(Part::Head);
    b.shell({{2.08f,.36f,.35f,-.03f},{2.18f,.51f,.45f,-.04f},{2.43f,.58f,.51f,-.05f},{2.69f,.54f,.48f,-.07f},{2.86f,.43f,.38f,-.09f},{2.96f,.25f,.22f,-.10f},{3.00f,.08f,.08f,-.10f}},pink,24);
    // The visor is a recessed structural band; the eye is a separate lens.
    b.cover({{-.47f,2.58f,.43f},{.47f,2.58f,.43f},{.45f,2.42f,.50f},{-.45f,2.42f,.50f}},.055f,black,.004f,true);
    b.tube({0,2.50f,.493f},{0,2.50f,.516f},.057f,.055f,eye,false,14);
    b.cover({{-.26f,2.38f,.50f},{.26f,2.38f,.50f},{.30f,2.18f,.48f},{.17f,2.10f,.46f},{-.17f,2.10f,.46f},{-.30f,2.18f,.48f}},.12f,salmon,.009f);
    b.box(0,2.29f,.58f,.23f,.12f,.12f,deepRed);
    if(detail){
        if(a)a->headHose.begin=b.m.count;
        for(float s:{-1.f,1.f})b.polyTube({{s*.39f,2.43f,.32f},{s*.54f,2.35f,.31f},{s*.52f,2.22f,.40f},{s*.28f,2.10f,.54f}},.068f,salmon,10);
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
Point handAnchor(Builder& b,float s){return anchor(b,s,{0,-.82f,s<0?.55f:.37f});}
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
            b.at(Part::Shoulders,{s*.80f,1.94f,0},.10f);b.shell({{-.26f,.30f,.28f},{.13f,.36f,.34f},{.25f,.29f,.28f}},salmon,12);
            if(a)a->leftSpikes.begin=b.m.count;
            b.tube({.20f,.03f,0},{.31f,.03f,0},.16f,.15f,pink,false,12);
            const std::pair<Point,Point> spikes[]={{{.25f,.09f,0},{.68f,.12f,0}},{{.17f,.22f,0},{.40f,.50f,0}},{{.19f,-.13f,.08f},{.37f,-.34f,.17f}}};
            for(const auto& spike:spikes)b.tube(spike.first,spike.second,.095f,.012f,pink,false,10,false,true);
            if(a)a->leftSpikes.end=b.m.count;
        }
        armFrame(b,Part::Arms,s);b.shell({{-.30f,.14f,.15f},{-.08f,.17f,.18f}},salmon,10);b.tube({0,-.12f,0},{0,-.54f,0},.13f,.15f,frame,false,10);
        b.at(Part::Arms,anchor(b,s,{0,-.58f,.03f}),s<0?-.16f:.10f);b.shell({{-.18f,.17f,.18f},{.13f,.19f,.20f},{.22f,.15f,.16f}},pink,10);
        const auto wrist=anchor(b,s,{0,-.66f,.03f}),hand=handAnchor(b,s);b.at(Part::Hands);b.tube(wrist,hand,.055f,.055f,frame,false,8);
        if(a)a->palms[s>0].begin=b.m.count;
        b.at(Part::Hands,hand,s<0?-.16f:.10f);b.box(0,0,0,.25f,.20f,.19f,frame);
        if(a)a->palms[s>0].end=b.m.count;
    }
    if(detail){b.at(Part::Shoulders);b.box(-.72f,1.93f,-.18f,.12f,.31f,.12f,frame);}
}
void equipment(Builder& b,ZakuAssembly* a){
    // Zaku machine gun in the right fist, including drum and hollow muzzle.
    const Point grip=handAnchor(b,-1);b.at(Part::Rifle,grip,-.16f,0,-.12f);
    if(a)a->rifle.begin=b.m.count;
    b.box(0,.02f,.05f,.11f,.28f,.10f,frame);b.box(0,.22f,.36f,.20f,.18f,.64f,black);
    b.tube({0,.23f,.28f},{0,.23f,.78f},.06f,.035f,metal,true,10);b.tube({0,.37f,.34f},{0,.37f,.48f},.23f,.23f,black,false,14);
    b.box(0,.04f,.47f,.09f,.28f,.08f,frame);b.tube({0,.30f,.72f},{0,.30f,.88f},.055f,.04f,metal,true,10);
    if(a)a->rifle.end=b.m.count;
    // Stowed heat hawk remains visible without crossing the skirt.
    b.at(Part::Waist);b.tube({.38f,1.14f,-.18f},{.47f,1.12f,-.28f},.055f,.055f,frame,false,8);
    b.at(Part::Sabers,{.54f,1.08f,-.34f},-.18f,0,.15f);
    if(a)a->heatHawk.begin=b.m.count;
    b.tube({0,-.25f,0},{0,.28f,0},.042f,.042f,purple,false,8);
    b.cover({{-.04f,.24f,.03f},{.28f,.33f,.03f},{.36f,.17f,.03f},{.09f,.08f,.03f}},.055f,gold,.008f,true);
    if(a)a->heatHawk.end=b.m.count;
}
} // namespace

void buildCharZaku(Mesh& mesh,BuildOptions options,ZakuStage stage,ZakuAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly={};if(stage==ZakuStage::Blockout)options.gray=true;
    Builder b{mesh,options};legs(b);torso(b,stage==ZakuStage::Final,assembly);head(b,stage!=ZakuStage::Blockout,assembly);arms(b,stage==ZakuStage::Final,assembly);if(options.equipment)equipment(b,assembly);
}
} // namespace gundam_museum
