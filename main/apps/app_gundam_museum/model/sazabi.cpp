#include "sazabi.h"
#include "sd_model_builder.h"

namespace gundam_museum {
namespace {
using sd_model::Builder;using sd_model::Ring;
constexpr uint16_t red=0xd946,bright=0xf208,deep=0x9145,wine=0x68e4;
constexpr uint16_t frame=0x39e7,black=0x1082,gold=0xfdc7,green=0x07ec,metal=0x7bef;

void legs(Builder& b,bool detail){
    for(float s:{-1.f,1.f}){
        const Point hip{s*.31f,1.13f,s<0?.045f:0};const float spread=s*(s<0?.22f:.18f),footYaw=s*(s<0?.24f:.20f);
        b.at(Part::Feet,{hip.x+s*.18f,0,hip.z},0,0,footYaw);
        b.shell({{.025f,.36f,.45f,.14f},{.11f,.40f,.49f,.17f},{.23f,.34f,.40f,.13f},{.30f,.25f,.28f,.03f}},black,12);
        b.cover({{-.28f,.28f,.30f},{.28f,.28f,.30f},{.34f,.08f,.47f},{.24f,.025f,.51f},{-.24f,.025f,.51f},{-.34f,.08f,.47f}},.05f,red,.008f);
        b.at(Part::Thighs,hip,spread);b.shell({{-.34f,.18f,.18f},{-.13f,.22f,.21f},{.05f,.18f,.17f}},red,12);
        b.at(Part::Knees,hip,spread);b.tube({-.11f,-.36f,0},{.11f,-.36f,0},.10f,.10f,frame,false,10);
        b.at(Part::Shins,hip,spread);
        b.shell({{-.75f,.30f,.27f,-.02f},{-.59f,.34f,.32f,-.04f},{-.39f,.24f,.23f,-.02f}},bright,12);
        b.cover({{-.12f,-.40f,.31f},{.12f,-.40f,.31f},{.17f,-.67f,.34f},{0,-.77f,.37f},{-.17f,-.67f,.34f}},.055f,red,.007f);
        if(detail){
            for(float side:{-1.f,1.f}){b.at(Part::Shins,hip,spread);b.box(side*.25f,-.59f,.18f,.065f,.22f,.19f,gold,true);}
            b.at(Part::Shins,{hip.x+s*.18f,.39f,hip.z},0,0,footYaw);b.tube({0,-.02f,-.26f},{0,-.05f,-.36f},.105f,.075f,frame,true,10);
        }
    }
}
void body(Builder& b,bool detail){
    b.at(Part::Waist);b.shell({{1.03f,.40f,.31f},{1.27f,.47f,.35f},{1.43f,.40f,.30f}},frame,12);
    for(float s:{-1.f,1.f}){
        b.cover({{s*.06f,1.41f,.34f},{s*.38f,1.39f,.34f},{s*.52f,1.08f,.39f},{s*.31f,.99f,.42f},{s*.10f,1.05f,.42f}},.075f,red,.007f);
        b.at(Part::Waist,{s*.43f,1.34f,-.02f},s*.25f,0,s*.23f);b.shell({{-.24f,.15f,.25f},{-.03f,.19f,.27f},{.08f,.15f,.24f}},bright,10);
    }
    b.at(Part::Waist);b.cover({{-.13f,1.43f,.38f},{.13f,1.43f,.38f},{.13f,1.03f,.45f},{0,.98f,.47f},{-.13f,1.03f,.45f}},.065f,bright,.008f);
    if(detail){for(float s:{-1.f,1.f})for(int i=0;i<3;++i)b.box(s*(.055f+i*.055f),1.22f-i*.035f,.48f,.035f,.13f,.025f,gold,true);}
    b.at(Part::Torso);b.shell({{1.39f,.42f,.31f},{1.61f,.53f,.40f},{1.91f,.65f,.46f},{2.03f,.45f,.34f}},deep,12);
    b.cover({{-.31f,1.96f,.39f},{.31f,1.96f,.39f},{.45f,1.67f,.50f},{.22f,1.47f,.52f},{-.22f,1.47f,.52f},{-.45f,1.67f,.50f}},.08f,red,.009f);
    b.cover({{-.17f,1.93f,.48f},{.17f,1.93f,.48f},{.20f,1.57f,.56f},{0,1.48f,.57f},{-.20f,1.57f,.56f}},.04f,bright,.006f);
    if(detail){
        for(float s:{-1.f,1.f}){b.cover({{s*.22f,1.85f,.53f},{s*.47f,1.82f,.48f},{s*.38f,1.66f,.53f},{s*.21f,1.67f,.56f}},.025f,black,.004f,true);b.box(s*.33f,1.74f,.555f,.12f,.025f,.03f,gold,true);}
    }
    b.shell({{1.98f,.18f,.16f},{2.10f,.22f,.19f}},frame,10);
    b.at(Part::Backpack);b.box(0,1.76f,-.42f,.50f,.56f,.18f,wine);
    // The two funnel racks remain visible beside the helmet in the complete
    // rear view; a slight overlap with the central backpack is the mount.
    for(float s:{-1.f,1.f}){
        b.box(s*.55f,1.94f,-.50f,.22f,.42f,.18f,wine);
        b.tube({s*.22f,1.85f,-.48f},{s*.49f,1.91f,-.52f},.045f,.055f,frame,false,8);
    }
    for(float s:{-1.f,1.f})b.tube({s*.15f,1.49f,-.51f},{s*.15f,1.30f,-.64f},.08f,.11f,metal,true,10);
}
void head(Builder& b,bool detail){
    b.at(Part::Head);
    b.shell({{2.08f,.36f,.37f,-.035f},{2.20f,.53f,.48f,-.04f},{2.45f,.60f,.54f,-.055f},{2.68f,.56f,.50f,-.07f},{2.84f,.45f,.40f,-.09f},{2.94f,.28f,.25f,-.10f},{2.98f,.09f,.09f,-.10f}},red,24);
    b.cover({{-.48f,2.57f,.48f},{.48f,2.57f,.48f},{.43f,2.42f,.55f},{-.43f,2.42f,.55f}},.05f,black,.004f,true);
    b.tube({0,2.50f,.540f},{0,2.50f,.558f},.058f,.056f,green,false,14);
    for(float s:{-1.f,1.f}){
        b.cover({{s*.12f,2.39f,.54f},{s*.42f,2.39f,.48f},{s*.49f,2.11f,.39f},{s*.30f,2.04f,.43f},{s*.13f,2.12f,.53f}},.075f,bright,.008f,true);
        b.face({s*.35f,2.30f,.48f},{s*.50f,2.16f,.37f},{s*.55f,1.94f,.23f},{s*.39f,2.03f,.43f},red,{s,0,1},true);
        if(detail)b.tube({s*.48f,2.47f,.22f},{s*.54f,2.45f,.15f},.07f,.06f,frame,true,10);
    }
    b.cover({{-.18f,2.40f,.56f},{.18f,2.40f,.56f},{.18f,2.14f,.57f},{0,2.06f,.59f},{-.18f,2.14f,.57f}},.04f,deep,.005f,true);
    // Central commander crest and the two short side fins are separate tapered solids.
    b.cover({{-.09f,2.88f,.37f},{.09f,2.88f,.37f},{.045f,3.48f,.08f},{0,3.54f,.05f},{-.045f,3.48f,.08f}},.075f,bright,.006f,true);
    for(float s:{-1.f,1.f})b.face({s*.20f,2.83f,.34f},{s*.44f,3.14f,.18f},{s*.32f,2.79f,.35f},{s*.32f,2.79f,.35f},gold,{0,0,1},true);
}
void armFrame(Builder& b,Part p,float s){b.at(p,{s*.86f,1.91f,-.01f},s*(s<0?.17f:.10f),s<0?-.10f:-.03f);}
Point armAnchor(Builder& b,float s,Point p){armFrame(b,Part::Hands,s);return b.transform(p);}
Point handAnchor(Builder& b,float s){return armAnchor(b,s,{0,-.88f,s<0?.43f:.38f});}
void arms(Builder& b,bool detail,SazabiAssembly* a){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);
        if(a)a->shoulders[s>0].begin=b.m.count;
        b.tube({-.17f,0,0},{.17f,0,0},.12f,.12f,frame,false,10);
        b.shell({{-.27f,.34f,.29f},{.08f,.45f,.39f},{.25f,.37f,.32f}},bright,12);
        b.cover({{-.32f,.18f,.33f},{.31f,.22f,.33f},{.45f,-.10f,.31f},{.20f,-.36f,.33f},{-.18f,-.33f,.33f},{-.42f,-.07f,.31f}},.07f,red,.007f);
        b.face({s*.05f,.18f,.38f},{s*.42f,.13f,.34f},{s*.58f,-.03f,.25f},{s*.18f,-.05f,.34f},bright,{s,0,1},true);
        if(detail){for(int i=0;i<3;++i)b.box(-.19f+i*.17f,-.22f,.35f,.08f,.035f,.04f,gold,true);}
        if(a)a->shoulders[s>0].end=b.m.count;
        armFrame(b,Part::Arms,s);b.shell({{-.34f,.15f,.17f},{-.10f,.19f,.21f}},red,10);b.tube({0,-.10f,0},{0,-.56f,0},.14f,.16f,frame,false,10);
        b.at(Part::Arms,armAnchor(b,s,{0,-.58f,.05f}),s<0?-.20f:.13f);b.shell({{-.22f,.19f,.21f},{.13f,.22f,.24f},{.24f,.17f,.18f}},bright,10);
        const auto wrist=armAnchor(b,s,{0,-.70f,.05f}),hand=handAnchor(b,s);b.at(Part::Hands);b.tube(wrist,hand,.06f,.06f,frame,false,8);
        if(a)a->palms[s>0].begin=b.m.count;
        b.at(Part::Hands,hand,s<0?-.20f:.13f);b.box(0,0,0,.27f,.21f,.20f,frame);
        if(a)a->palms[s>0].end=b.m.count;
    }
}
void funnels(Builder& b,SazabiAssembly* a){
    // Six physical funnel bodies in two banks; each range remains independently testable.
    int unit=0;for(float side:{-1.f,1.f})for(int row=0;row<3;++row,++unit){
        const Point base{side*(.55f+row*.15f),2.00f-row*.11f,-.61f-row*.025f};
        b.at(Part::Funnels);
        if(a)a->funnels[unit].begin=b.m.count;
        b.tube(base,{base.x+side*.08f,base.y+.32f,base.z-.11f},.055f,.038f,deep,true,10);
        b.box(base.x,base.y-.045f,base.z+.035f,.09f,.10f,.085f,frame,true);
        if(a)a->funnels[unit].end=b.m.count;
    }
}
void equipment(Builder& b,SazabiAssembly* a){
    const auto right=handAnchor(b,-1);
    b.at(Part::Rifle,right,-.20f,0,-.08f);
    if(a)a->rifle.begin=b.m.count;
    b.box(0,-.02f,.04f,.10f,.25f,.10f,frame);b.box(0,.19f,.38f,.18f,.20f,.67f,black);
    b.tube({0,.21f,.63f},{0,.21f,1.18f},.045f,.027f,metal,true,10);b.box(0,.31f,.39f,.28f,.17f,.31f,deep);
    b.tube({0,.38f,.39f},{0,.38f,.56f},.075f,.052f,green,true,10);b.box(0,.02f,.48f,.09f,.30f,.08f,frame);
    if(a)a->rifle.end=b.m.count;
    const auto left=armAnchor(b,1,{.14f,-.34f,.05f});const Point center{left.x+.48f,left.y+.01f,left.z+.38f};
    b.at(Part::Shield);b.tube(left,{center.x-.12f,center.y,center.z-.13f},.04f,.04f,frame,false,8);b.at(Part::Shield,center,.18f,0,.46f);
    if(a)a->shield.begin=b.m.count;
    b.box(0,0,-.16f,.13f,.35f,.06f,frame,true);
    b.cover({{-.28f,.76f,.05f},{.28f,.76f,.05f},{.39f,.34f,.05f},{.28f,-.69f,.05f},{0,-.82f,.05f},{-.28f,-.69f,.05f},{-.39f,.34f,.05f}},.13f,deep,.009f,true);
    b.cover({{-.21f,.65f,.075f},{.21f,.65f,.075f},{.29f,.29f,.075f},{.18f,-.58f,.075f},{0,-.70f,.075f},{-.18f,-.58f,.075f},{-.29f,.29f,.075f}},.025f,black,.006f,true);
    b.cover({{-.035f,.47f,.102f},{.035f,.47f,.102f},{.05f,-.20f,.102f},{0,-.34f,.102f},{-.05f,-.20f,.102f}},.018f,gold,.004f,true);
    if(a)a->shield.end=b.m.count;
    // Beam tomahawk clipped to the shield back, visible from rear rotation.
    b.at(Part::Sabers,center,.18f,0,.46f);b.tube({0,-.38f,-.18f},{0,.30f,-.18f},.04f,.04f,frame,false,8);
    b.cover({{-.04f,.24f,-.14f},{.25f,.38f,-.14f},{.34f,.22f,-.14f},{.08f,.09f,-.14f}},.05f,gold,.006f,true);
    funnels(b,a);
}
} // namespace

void buildSazabi(Mesh& mesh,BuildOptions options,SazabiStage stage,SazabiAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly={};if(stage==SazabiStage::Blockout)options.gray=true;
    Builder b{mesh,options};legs(b,stage==SazabiStage::Final);body(b,stage==SazabiStage::Final);head(b,stage!=SazabiStage::Blockout);arms(b,stage==SazabiStage::Final,assembly);if(options.equipment)equipment(b,assembly);
}
} // namespace gundam_museum
