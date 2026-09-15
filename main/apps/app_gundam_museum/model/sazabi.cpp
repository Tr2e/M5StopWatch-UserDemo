#include "sazabi.h"
#include "sd_curved_parts.h"

namespace gundam_museum {
namespace {
using sd_model::Builder;
constexpr uint16_t red=0xd946,bright=0xe9e7,deep=0xa126;
constexpr uint16_t frame=0x39e7,black=0x1082,gold=0xe486,green=0x07ec;
void legs(Builder& b,bool detail){
    for(float s:{-1.f,1.f}){
        const Point hip{s*.31f,1.13f,s<0?.045f:0};const float spread=s*(s<0?.22f:.18f),footYaw=s*(s<0?.24f:.20f);
        b.at(Part::Feet,{hip.x+s*.18f,0,hip.z},0,0,footYaw);
        b.shell({{.025f,.36f,.45f,.14f},{.075f,.38f,.47f,.15f}},black,12);
        b.shell({{.075f,.38f,.47f,.15f},{.15f,.37f,.45f,.15f},{.25f,.30f,.33f,.08f},{.31f,.23f,.24f}},red,12);
        b.at(Part::Knees);b.tube({hip.x+s*.18f,.25f,hip.z},{hip.x+s*.17f,.46f,hip.z},.09f,.09f,frame,false,8);
        b.at(Part::Thighs,hip,spread);b.shell({{-.34f,.18f,.18f},{-.13f,.22f,.21f},{.05f,.18f,.17f}},red,12);
        b.at(Part::Knees,hip,spread);b.tube({-.12f,-.36f,0},{.12f,-.36f,0},.11f,.11f,frame,false,10);
        b.at(Part::Shins,hip,spread);
        b.shell({{-.77f,.31f,.29f,-.02f},{-.70f,.35f,.34f,-.04f},{-.57f,.32f,.32f,-.06f},{-.43f,.27f,.26f,-.04f},{-.31f,.20f,.20f}},red,14);
        b.cover({{-.15f,-.39f,.25f},{.15f,-.39f,.25f},{.19f,-.67f,.33f},{0,-.75f,.35f},{-.19f,-.67f,.33f}},.045f,bright,.008f);
        b.cover({{s*.17f,-.40f,-.15f},{s*.33f,-.49f,-.27f},{s*.41f,-.71f,-.32f},{s*.17f,-.67f,-.36f}},.055f,red,.005f);
        if(detail)b.tube({s*.23f,-.59f,-.24f},{s*.30f,-.64f,-.36f},.075f,.09f,gold,true,10);
    }
}
void body(Builder& b,bool detail){
    b.at(Part::Waist);b.shell({{1.03f,.39f,.30f},{1.25f,.46f,.34f},{1.39f,.37f,.27f}},deep,12);
    for(float s:{-1.f,1.f}){
        b.at(Part::Waist);b.cover({{s*.06f,1.35f,.35f},{s*.36f,1.34f,.33f},{s*.51f,1.06f,.39f},{s*.28f,.99f,.45f},{s*.10f,1.05f,.44f}},.065f,red,.008f);
        b.at(Part::Waist,{s*.43f,1.30f,-.04f},s*.27f,0,s*.23f);
        b.shell({{-.27f,.20f,.27f},{-.13f,.21f,.28f},{.02f,.17f,.24f},{.10f,.12f,.18f}},red,10);
    }
    b.at(Part::Waist);b.cover({{-.105f,1.37f,.39f},{.105f,1.37f,.39f},{.12f,1.05f,.46f},{0,.98f,.48f},{-.12f,1.05f,.46f}},.045f,bright,.008f);
    b.at(Part::Torso);b.shell({{1.36f,.34f,.26f},{1.49f,.39f,.29f},{1.58f,.36f,.27f}},black,12);
    if(detail){for(float s:{-1.f,1.f})sd_curved::hose(b,{{s*.08f,1.47f,.32f},{s*.30f,1.46f,.31f},{s*.40f,1.47f,.08f},{s*.28f,1.49f,-.27f}},.058f,gold,9,6);
        b.tube({0,1.47f,.31f},{0,1.47f,.39f},.093f,.09f,red,false,12);b.tube({0,1.47f,.39f},{0,1.47f,.402f},.04f,.038f,gold,true,10);}
    b.at(Part::Torso);b.shell({{1.55f,.36f,.29f},{1.73f,.53f,.37f},{1.91f,.53f,.34f},{1.99f,.36f,.25f}},deep,14);
    b.at(Part::Torso,{0,0,.24f});
    b.shell({{1.59f,.39f,.15f},{1.65f,.52f,.23f},{1.77f,.57f,.25f},{1.88f,.53f,.21f},{1.95f,.36f,.10f}},red,18);
    b.box(0,1.78f,.252f,.014f,.12f,.008f,deep,true);
    b.at(Part::Torso);b.shell({{1.96f,.17f,.15f},{2.14f,.18f,.16f}},frame,10);
    b.at(Part::Backpack);b.box(0,1.81f,-.48f,.52f,.51f,.22f,black);
    for(float s:{-1.f,1.f}){
        b.tube({s*.19f,1.83f,-.47f},{s*.57f,1.79f,-.75f},.065f,.07f,frame,false,8);
        b.tube({s*.13f,1.78f,-.60f},{s*.13f,1.77f,-.70f},.06f,.065f,gold,true,10,false,true);
        // Propellant tanks are distinct from the six stowed funnel ports.
        b.tube({s*.22f,1.67f,-.69f},{s*.36f,.88f,-1.01f},.13f,.13f,black,false,12);
        b.tube({s*.33f,1.07f,-.93f},{s*.34f,1.03f,-.95f},.138f,.138f,frame,false,12);
    }
}
void head(Builder& b,bool detail,SazabiAssembly* a){
    b.at(Part::Head);
    // An internal post overlaps the torso collar below and the helmet cup
    // above. Its dark material remains visible as the intended short neck.
    if(a)a->headMount.begin=b.m.count;
    b.tube({0,2.05f,-.04f},{0,2.32f,-.04f},.145f,.17f,frame,false,6);
    if(a)a->headMount.end=b.m.count;
    if(a)a->crown.begin=b.m.count;
    b.shell({{2.57f,.52f,.44f,-.06f},{2.69f,.49f,.43f,-.08f},{2.82f,.40f,.35f,-.09f},{2.93f,.27f,.24f,-.10f},{2.98f,.09f,.09f,-.10f}},red,20);
    if(a)a->crown.end=b.m.count;
    sd_curved::arc(b,{{2.13f,.36f,.32f,-.04f},{2.30f,.49f,.43f,-.04f},{2.57f,.52f,.44f,-.06f}},red,1.08f,2*sd_model::pi-1.08f,16);
    // SDEX 017: tapered oblique window with a recessed back, not a painted
    // equal-width V strip. All three boundaries share the brow's curved rim.
    for(float s:{-1.f,1.f}){
        const auto brow=[&](float t,float v){return Point{s*.43f*t,(2.49f+.13f*t)*(1-v)+(2.91f-.25f*t*t)*v,(.55f-.21f*t*t)*(1-v)+(.30f-.08f*t*t)*v};};
        const auto lower=[&](float t){return Point{s*.43f*t,2.405f+.195f*t,.55f-.21f*t*t};};
        const auto back=[](Point p){p.z-=.070f;return p;};
        if(a)a->eyeWindow[s>0].begin=b.m.count;
        for(int i=0;i<6;++i){const float t=i/6.f,u=(i+1)/6.f;auto p=brow(t,0),q=brow(u,0),r=lower(u),v=lower(t);
            b.face(p,q,back(q),back(p),black,{0,-1,0},true);
            b.face(v,r,back(r),back(v),black,{0,1,0},true);
            b.face(back(p),back(q),back(r),back(v),black,{0,0,1},true);
            if(i==5)b.face(q,r,back(r),back(q),black,{s,0,0},true);
        }
        if(a)a->eyeWindow[s>0].end=b.m.count;
        for(int i=0;i<6;++i){const float t=i/6.f,u=(i+1)/6.f;
            auto p=lower(t),q=lower(u);
            b.face(p,q,{0,2.37f,.58f},{0,2.37f,.58f},red,{0,0,1},true);
            // Spend facets on the actual window; two vertical brow bands
            // preserve the crown silhouette within the fixed mesh capacity.
            for(int j=0;j<2;++j)b.face(brow(t,j/2.f),brow(u,j/2.f),brow(u,(j+1)/2.f),brow(t,(j+1)/2.f),red,{s*.2f,.2f,1},true);
        }
        b.face(lower(1),{s*.258f,2.37f,.47f},{0,2.37f,.58f},{0,2.37f,.58f},red,{0,0,1},true);
        b.cover({{s*.35f,2.54f,.38f},{s*.49f,2.46f,.24f},{s*.54f,2.17f,.20f},{s*.36f,2.14f,.38f},{s*.22f,2.37f,.47f}},.055f,bright,.006f);
    }
    if(a)a->mask.begin=b.m.count;
    // The mask now has a physical chain: helmet post -> internal face beam ->
    // backing plate -> red face armor. The old red diamond touched at one
    // screen point only and read as a floating decoration in rotation.
    b.tube({0,2.27f,.05f},{0,2.27f,.43f},.06f,.075f,deep,false,6);
    b.cover({{-.30f,2.39f,.435f},{.30f,2.39f,.435f},{.25f,2.14f,.405f},{-.25f,2.14f,.405f}},.035f,deep,.004f,true);
    for(float s:{-1.f,1.f}){
        const Point top{0,2.37f,.58f},side{s*.258f,2.37f,.47f},tip{0,2.12f,.57f};
        const auto rear=[](Point p){p.z-=.045f;return p;};
        b.face(top,side,tip,tip,red,{0,0,1},true);
        b.face(rear(top),rear(side),rear(tip),rear(tip),red,{0,0,-1},true);
        b.face(top,side,rear(side),rear(top),red,{0,1,0},true);
        b.face(side,tip,rear(tip),rear(side),red,{s,-1,0},true);
    }
    if(a)a->mask.end=b.m.count;
    if(a)a->eyeLens.begin=b.m.count;
    b.tube({0,2.452f,.465f},{0,2.452f,.490f},.037f,.035f,green,false,10);
    if(a)a->eyeLens.end=b.m.count;
    // All three prongs are red; gold belongs to the vents and waist pipes.
    b.cover({{-.065f,2.82f,.31f},{.065f,2.82f,.31f},{.025f,3.54f,.02f},{0,3.59f,0},{-.025f,3.54f,.02f}},.065f,bright,.006f);
    for(float s:{-1.f,1.f})b.cover({{s*.08f,2.88f,.32f},{s*.23f,2.92f,.23f},{s*.32f,3.46f,.12f},{s*.27f,3.46f,.14f},{s*.13f,3.02f,.30f}},.06f,red,.005f);
    if(detail){b.cover({{-.052f,2.78f,.363f},{.052f,2.78f,.363f},{.039f,2.96f,.289f},{-.039f,2.96f,.289f}},.012f,black,.002f);
        b.cover({{-.032f,2.80f,.370f},{.032f,2.80f,.370f},{.025f,2.94f,.312f},{-.025f,2.94f,.312f}},.008f,green,.002f);}
}
void armFrame(Builder& b,Part p,float s){b.at(p,{s*.85f,2.00f,-.01f},s*(s<0?.17f:.10f),s<0?-.10f:-.03f);}
Point armAnchor(Builder& b,float s,Point p){armFrame(b,Part::Hands,s);return b.transform(p);}
Point handAnchor(Builder& b,float s){return armAnchor(b,s,{0,-1.05f,.17f});}
void arms(Builder& b,bool detail,SazabiAssembly* a){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);if(a)a->shoulders[s>0].begin=b.m.count;
        b.tube({-.17f,0,0},{.17f,0,0},.12f,.12f,frame,false,10);
        b.shell({{-.16f,.30f,.27f},{.04f,.41f,.35f},{.21f,.42f,.34f},{.35f,.35f,.28f},{.39f,.24f,.22f}},red,12);
        b.cover({{-s*.28f,.27f,.37f},{s*.31f,.38f,.36f},{s*.43f,.25f,.35f},{s*.39f,-.02f,.39f},{s*.12f,-.15f,.41f},{-s*.25f,.02f,.40f}},.035f,bright,.007f);
        if(detail)for(int i=0;i<2;++i){const float y=.12f+i*.13f;
            b.face({-s*.06f,y,.415f},{s*.23f,y+.065f,.408f},{s*.23f,y+.075f,.408f},{-s*.06f,y+.01f,.415f},deep,{0,0,1},true);}
        b.cover({{-.20f,-.09f,.33f},{.17f,-.10f,.34f},{.22f,-.30f,.34f},{.12f,-.49f,.32f},{-.12f,-.47f,.32f},{-.23f,-.27f,.34f}},.055f,red,.006f);
        if(detail)for(int i=0;i<2;++i){b.box(0,-.20f-i*.15f,.353f,.12f,.105f,.028f,gold,true);b.box(0,-.20f-i*.15f,.372f,.071f,.066f,.020f,black,true);}
        if(a)a->shoulders[s>0].end=b.m.count;
        armFrame(b,Part::Arms,s);b.tube({0,-.22f,0},{0,-.57f,0},.14f,.14f,frame,false,10);
        b.at(Part::Arms,armAnchor(b,s,{0,-.65f,.05f}),s<0?-.20f:.13f,-.18f);
        b.shell({{-.21f,.18f,.20f},{-.14f,.23f,.24f},{.02f,.25f,.25f},{.15f,.23f,.21f},{.23f,.17f,.17f}},red,12);
        const auto wrist=armAnchor(b,s,{0,-.86f,.08f}),hand=handAnchor(b,s);b.at(Part::Hands);b.tube(wrist,{hand.x,hand.y+.01f,hand.z-.10f},.055f,.055f,frame,false,8);
        if(a)a->palms[s>0].begin=b.m.count;
        b.at(Part::Hands,hand,s<0?-.20f:.13f,s<0?.78f:0,-.08f);b.box(0,0,0,.27f,.21f,.20f,frame);
        if(a)a->palms[s>0].end=b.m.count;
    }
}
void funnels(Builder& b,SazabiAssembly* a){
    for(int side=0;side<2;++side){const float s=side?1.f:-1.f;
        b.at(Part::Funnels,{s*.60f,2.04f,-.81f},s*.15f,-.48f);
        if(a)a->containers[side].begin=b.m.count;
        // An open angular container per side; three ports on its forward face.
        b.box(-.135f,.15f,0,.055f,.72f,.34f,black);b.box(.135f,.15f,0,.055f,.72f,.34f,black);
        b.box(0,.15f,-.145f,.22f,.72f,.05f,black);b.box(0,.15f,.145f,.22f,.72f,.05f,black);
        b.box(0,-.22f,0,.32f,.07f,.34f,black);
        if(a)a->containers[side].end=b.m.count;
        for(int row=0;row<3;++row){const int n=side*3+row;const float y=-.10f+row*.21f;
            if(a)a->funnels[n].begin=b.m.count;
            b.tube({0,y,.17f},{0,y,.21f},.073f,.073f,deep,true,10);
            if(a)a->funnels[n].end=b.m.count;
        }
    }
}
void equipment(Builder& b,SazabiAssembly* a){
    const auto right=handAnchor(b,-1);b.at(Part::Rifle,right,-.20f,.78f,-.08f);
    if(a)a->rifle.begin=b.m.count;
    b.box(0,0,.02f,.10f,.27f,.09f,frame);b.box(0,.24f,.23f,.18f,.20f,.48f,black);
    b.box(0,.23f,.67f,.115f,.13f,.65f,black);b.box(0,.25f,1.05f,.15f,.18f,.22f,black);
    b.box(0,.13f,.37f,.14f,.24f,.20f,black);b.box(0,.38f,.19f,.15f,.09f,.26f,deep);
    b.box(0,.26f,1.163f,.057f,.063f,.015f,frame,true);
    if(a)a->rifle.end=b.m.count;
    const auto left=armAnchor(b,1,{.15f,-.53f,.05f});const Point center{left.x+.59f,left.y-.10f,left.z+.24f};
    if(a)a->shieldMount.begin=b.m.count;
    b.at(Part::Shield);b.tube(left,{center.x-.07f,center.y,center.z-.10f},.045f,.045f,frame,false,8);
    if(a)a->shieldMount.end=b.m.count;
    b.at(Part::Shield,center,.08f,0,.46f);if(a)a->shield.begin=b.m.count;
    b.cover({{-.22f,.77f,.04f},{.18f,.79f,.04f},{.32f,.51f,.04f},{.26f,.12f,.04f},{.103f,-.62f,.04f},{-.102f,-.62f,.04f},{-.25f,.11f,.04f},{-.32f,.49f,.04f}},.13f,red,.009f);
    for(float s:{-1.f,1.f})b.cover({{s*.018f,-.615f,.04f},{s*.104f,-.615f,.04f},{s*.065f,-.82f,.04f},{s*.018f,-.82f,.04f}},.13f,red,.003f);
    b.cover({{-.19f,.68f,.071f},{.17f,.70f,.071f},{.26f,.45f,.071f},{.11f,.22f,.071f},{.06f,-.52f,.071f},{-.05f,-.52f,.071f},{-.12f,.23f,.071f},{-.25f,.46f,.071f}},.019f,black,.004f);
    b.cover({{-.018f,.66f,.04f},{.018f,.66f,.04f},{.018f,.96f,-.01f},{-.018f,.96f,-.01f}},.04f,red,.003f);
    for(float s:{-1.f,1.f})b.face({0,.43f,.112f},{s*.17f,.55f,.112f},{s*.10f,.34f,.112f},{s*.10f,.34f,.112f},gold,{0,0,1},true);
    b.face({-.033f,.51f,.114f},{.033f,.51f,.114f},{0,.14f,.114f},{0,.14f,.114f},gold,{0,0,1},true);
    if(a)a->shield.end=b.m.count;
    b.at(Part::Sabers,center,.08f,0,.46f);b.tube({0,-.40f,-.14f},{0,.32f,-.14f},.033f,.033f,frame,false,8);
    b.cover({{-.05f,.22f,-.12f},{.15f,.31f,-.12f},{.18f,.20f,-.12f},{.04f,.12f,-.12f}},.035f,frame,.004f);
    funnels(b,a);
}
} // namespace
void buildSazabi(Mesh& mesh,BuildOptions options,SazabiStage stage,SazabiAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly={};if(stage==SazabiStage::Blockout)options.gray=true;
    Builder b{mesh,options};legs(b,stage==SazabiStage::Final);body(b,stage==SazabiStage::Final);head(b,stage!=SazabiStage::Blockout,assembly);arms(b,stage==SazabiStage::Final,assembly);if(options.equipment)equipment(b,assembly);
}
} // namespace gundam_museum
