// Structural inspection only: same authored asset and production raster, larger
// host tile. These frames are not native device screenshots or FPS evidence.
#include "../main/apps/app_gundam_museum/view/museum_renderer.h"
#include "../main/apps/app_gundam_museum/model/nu_gundam.h"
#include "../main/apps/app_gundam_museum/model/strike_gundam.h"
#include "../main/apps/app_gundam_museum/model/char_zaku.h"
#include "../main/apps/app_gundam_museum/model/sazabi.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
using namespace gundam_museum;
int main(int argc,char** argv){
    const std::string out=argc>1?argv[1]:"/tmp/gundam-reference";
    auto mesh=std::make_unique<Mesh>();
    auto raster=std::make_unique<lets_and_go::CarSurfaceRaster<640,640>>();
    LGFX_Sprite canvas;canvas.createSprite(640,640);
    struct Shot{const char* name;float yaw,pitch,pivot,scale;Pose pose;bool equipment,gray;
        float centerX=0;Part only=Part::Count,through=Part::Count;int limb=0;};
    const Shot rxShots[]={
        {"standing",-.40f,.025f,1.49f,173,Pose::Display,true,false},
        {"front",0,.025f,1.49f,179,Pose::Display,false,false},
        {"rear",3.14159265f,.025f,1.49f,179,Pose::Display,false,false},
        {"rear-equipped",3.14159265f,.05f,1.49f,173,Pose::Display,true,false},
        {"side",3.14159265f/2,.025f,1.49f,179,Pose::Display,false,false},
        {"other-side",-3.14159265f/2,.025f,1.49f,179,Pose::Display,false,false},
        {"quarter-right",.8f,.025f,1.49f,179,Pose::Display,false,false},
        {"gray",-.40f,.025f,1.49f,179,Pose::Display,false,true},
        {"gray-equipped",-.40f,.025f,1.49f,173,Pose::Display,true,true},
        {"face-front",0,0,2.41f,345,Pose::Display,false,false,0,Part::Head},
        {"face-quarter",-.90f,.035f,2.41f,345,Pose::Display,false,false,0,Part::Head},
        {"face-side",-3.14159265f/2,0,2.41f,345,Pose::Display,false,false,0,Part::Head},
        {"chest",-.4f,.025f,1.53f,510,Pose::Display,false,false,0,Part::Torso},
        {"arm",-.4f,.025f,1.26f,400,Pose::Display,false,false,-.7f,Part::Shoulders,Part::Hands,-1},
        {"leg",-.4f,.025f,.65f,420,Pose::Display,false,false,.46f,Part::Feet,Part::Thighs,1},
        {"leg-side",3.14159265f/2,.025f,.65f,420,Pose::Display,false,false,.46f,Part::Feet,Part::Thighs,1},
        {"rifle",-.7f,.025f,.68f,350,Pose::Display,true,false,-.98f,Part::Rifle},
        {"shield",-.2f,.025f,1.19f,375,Pose::Display,true,false,.86f,Part::Shield},
        {"grip-assembly",-.8f,.08f,1.10f,510,Pose::Display,true,false,-.95f,Part::Shoulders,Part::Rifle,-1},
        {"grip-side",1.57079633f,.08f,1.10f,510,Pose::Display,true,false,-.95f,Part::Shoulders,Part::Rifle,-1},
        {"shield-mount",-2.15f,.10f,1.25f,440,Pose::Display,true,false,1.0f,Part::Shoulders,Part::Shield,1},
        {"shield-side",-1.57079633f,.08f,1.25f,420,Pose::Display,true,false,1.0f,Part::Shoulders,Part::Shield,1},
        {"top",-.4f,.7f,1.49f,155,Pose::Display,true,false},
        {"underside",-.4f,-.35f,1.49f,155,Pose::Display,true,false},
    };
    const bool strike=argc>2 && std::string(argv[2])=="strike";
    const bool nu=argc>2 && std::string(argv[2])=="nu";
    const bool zaku=argc>2 && std::string(argv[2])=="zaku";
    const bool sazabi=argc>2 && std::string(argv[2])=="sazabi";
    const NuStage stage=argc>3 && std::string(argv[3])=="blockout"?NuStage::Blockout:
        argc>3 && std::string(argv[3])=="identity"?NuStage::Identity:NuStage::Final;
    const Shot nuShots[]={
        {"standing",-.40f,.04f,1.65f,145,Pose::Display,true,false},
        {"front",0,0,1.75f,145,Pose::Display,false,false},
        {"rear",3.14159265f,0,1.75f,145,Pose::Display,false,false},
        {"rear-equipped",3.14159265f,.05f,1.65f,145,Pose::Display,true,false},
        {"side",1.57079633f,0,1.75f,145,Pose::Display,false,false},
        {"other-side",-1.57079633f,0,1.75f,145,Pose::Display,false,false},
        {"gray",-.40f,0,1.75f,145,Pose::Display,false,true},
        {"gray-equipped",-.40f,.025f,1.65f,145,Pose::Display,true,true},
        {"face-front",0,0,2.80f,260,Pose::Display,false,false,0,Part::Head},
        {"face-quarter",-.80f,.02f,2.80f,260,Pose::Display,false,false,0,Part::Head},
        {"face-side",-1.57079633f,0,2.80f,260,Pose::Display,false,false,0,Part::Head},
        {"chest",-.40f,.025f,1.68f,560,Pose::Display,false,false,0,Part::Torso},
        {"leg",-.40f,.025f,.56f,390,Pose::Display,false,false,.42f,Part::Feet,Part::Thighs,1},
        {"leg-side",1.57079633f,.025f,.56f,390,Pose::Display,false,false,.42f,Part::Feet,Part::Thighs,1},
        {"arm",-.40f,.025f,1.45f,400,Pose::Display,false,false,-.85f,Part::Shoulders,Part::Hands,-1},
        {"funnels",0,.025f,2.1f,175,Pose::Display,true,false,1.2f,Part::Funnels},
        {"funnels-rear",3.14159265f,.025f,2.1f,175,Pose::Display,true,false,1.2f,Part::Funnels},
        {"shield",-.20f,.025f,1.4f,340,Pose::Display,true,false,1.3f,Part::Shield},
        {"rifle",-.40f,.025f,.8f,340,Pose::Display,true,false,-1.15f,Part::Rifle},
        {"grip-assembly",-.8f,.08f,1.12f,430,Pose::Display,true,false,-.95f,Part::Shoulders,Part::Rifle,-1},
        {"grip-side",1.57079633f,.08f,1.12f,430,Pose::Display,true,false,-.95f,Part::Shoulders,Part::Rifle,-1},
        {"shield-mount",-2.15f,.10f,1.35f,390,Pose::Display,true,false,1.0f,Part::Shoulders,Part::Shield,1},
        {"top",-.40f,.70f,1.65f,140,Pose::Display,true,false},
        {"underside",-.40f,-.35f,1.65f,140,Pose::Display,true,false},
    };
    const Shot strikeShots[]={
        {"standing",-.40f,.04f,1.72f,140,Pose::Display,true,false},
        {"front",0,.025f,1.74f,143,Pose::Display,false,false},
        {"rear",3.14159265f,.025f,1.74f,143,Pose::Display,false,false},
        {"rear-equipped",3.14159265f,.05f,1.72f,140,Pose::Display,true,false},
        {"side",1.57079633f,.025f,1.74f,143,Pose::Display,false,false},
        {"other-side",-1.57079633f,.025f,1.74f,143,Pose::Display,false,false},
        {"gray",-.40f,.025f,1.74f,143,Pose::Display,false,true},
        {"gray-equipped",-.40f,.025f,1.72f,140,Pose::Display,true,true},
        {"face-front",0,0,2.82f,275,Pose::Display,false,false,0,Part::Head},
        {"face-quarter",-.80f,.02f,2.82f,275,Pose::Display,false,false,0,Part::Head},
        {"face-side",-1.57079633f,0,2.82f,275,Pose::Display,false,false,0,Part::Head},
        {"chest",-.40f,.025f,1.72f,500,Pose::Display,false,false,0,Part::Torso},
        {"chest-front",0,0,1.72f,500,Pose::Display,false,false,0,Part::Torso},
        {"leg",-.40f,.025f,0.65f,380,Pose::Display,false,false,.43f,Part::Feet,Part::Thighs,1},
        {"leg-side",1.57079633f,.025f,0.65f,380,Pose::Display,false,false,.43f,Part::Feet,Part::Thighs,1},
        {"arm",-.40f,.025f,1.55f,400,Pose::Display,false,false,-.86f,Part::Shoulders,Part::Hands,-1},
        {"aile",0,.75f,1.6f,140,Pose::Display,true,false,0,Part::Aile},
        {"aile-rear",3.14159265f,.15f,1.6f,140,Pose::Display,true,false,0,Part::Aile},
        {"shield",-.20f,.025f,1.55f,245,Pose::Display,true,false,1.30f,Part::Shield},
        {"rifle",-.70f,.025f,0.98f,245,Pose::Display,true,false,-1.35f,Part::Rifle},
        {"grip-assembly",-.8f,.08f,1.2f,400,Pose::Display,true,false,-.95f,Part::Shoulders,Part::Rifle,-1},
        {"grip-side",1.57079633f,.08f,1.2f,400,Pose::Display,true,false,-.95f,Part::Shoulders,Part::Rifle,-1},
        {"shield-mount",-2.15f,.10f,1.4f,360,Pose::Display,true,false,1.08f,Part::Shoulders,Part::Shield,1},
        {"top",-.40f,.70f,1.72f,130,Pose::Display,true,false},
        {"underside",-.40f,-.35f,1.72f,130,Pose::Display,true,false},
    };
    const Shot zakuShots[]={
        {"standing",-.40f,.04f,1.67f,150,Pose::Display,true,false},{"front",0,.02f,1.67f,153,Pose::Display,false,false},
        {"rear",3.14159265f,.02f,1.67f,153,Pose::Display,false,false},{"rear-equipped",3.14159265f,.05f,1.67f,150,Pose::Display,true,false},
        {"side",1.57079633f,.02f,1.67f,153,Pose::Display,false,false},{"other-side",-1.57079633f,.02f,1.67f,153,Pose::Display,false,false},
        {"quarter-right",.80f,.02f,1.67f,153,Pose::Display,false,false},
        {"gray",-.40f,.02f,1.67f,153,Pose::Display,false,true},{"gray-equipped",-.40f,.02f,1.67f,150,Pose::Display,true,true},
        {"face-front",0,0,2.56f,285,Pose::Display,false,false,0,Part::Head},{"face-quarter",-.8f,.02f,2.56f,285,Pose::Display,false,false,0,Part::Head},
        {"face-side",-1.57079633f,0,2.56f,285,Pose::Display,false,false,0,Part::Head},{"chest",-.4f,.02f,1.70f,510,Pose::Display,false,false,0,Part::Torso},
        {"chest-front",0,0,1.70f,510,Pose::Display,false,false,0,Part::Torso},
        {"arm-right",-.4f,.02f,1.62f,370,Pose::Display,true,false,-.82f,Part::Shoulders,Part::Hands,-1},
        {"arm-left",.4f,.02f,1.75f,350,Pose::Display,true,false,.88f,Part::Shoulders,Part::Hands,1},
        {"leg",-.4f,.02f,.60f,390,Pose::Display,false,false,.43f,Part::Feet,Part::Thighs,1},
        {"leg-side",1.57079633f,.02f,.60f,390,Pose::Display,false,false,.43f,Part::Feet,Part::Thighs,1},
        {"rifle",-.7f,.02f,1.22f,270,Pose::Display,true,false,-1.0f,Part::Rifle},{"heat-hawk",.7f,.02f,1.10f,350,Pose::Display,true,false,.62f,Part::Sabers},
        {"head-mount",-.70f,.02f,2.14f,390,Pose::Display,false,false,0,Part::Torso,Part::Head},
        {"heat-hawk-mount",2.60f,.02f,1.15f,470,Pose::Display,true,false,.46f,Part::Waist,Part::Sabers},
        {"shield",-.2f,.02f,1.92f,300,Pose::Display,true,false,-1.18f,Part::Shield},
        {"grip-assembly",-.8f,.08f,1.12f,430,Pose::Display,true,false,-1.04f,Part::Arms,Part::Rifle,-1},
        {"grip-side",1.57079633f,.08f,1.12f,430,Pose::Display,true,false,-1.04f,Part::Arms,Part::Rifle,-1},
        {"shield-mount",-2.15f,.10f,1.55f,390,Pose::Display,true,false,-1.05f,Part::Shoulders,Part::Shield,-1},
        {"shield-side",-1.57079633f,.08f,1.55f,390,Pose::Display,true,false,-1.05f,Part::Shoulders,Part::Shield,-1},
        {"top",-.4f,.70f,1.67f,140,Pose::Display,true,false},{"underside",-.4f,-.30f,1.67f,140,Pose::Display,true,false},
    };
    const Shot sazabiShots[]={
        {"standing",-.40f,.04f,1.70f,137,Pose::Display,true,false},{"front",0,.02f,1.70f,140,Pose::Display,false,false},
        {"rear",3.14159265f,.02f,1.70f,140,Pose::Display,false,false},{"rear-equipped",3.14159265f,.05f,1.70f,137,Pose::Display,true,false},
        {"side",1.57079633f,.02f,1.70f,140,Pose::Display,false,false},{"other-side",-1.57079633f,.02f,1.70f,140,Pose::Display,false,false},
        {"gray",-.40f,.02f,1.70f,140,Pose::Display,false,true},{"gray-equipped",-.40f,.02f,1.70f,137,Pose::Display,true,true},
        {"face-front",0,0,2.58f,275,Pose::Display,false,false,0,Part::Head},{"face-quarter",-.8f,.02f,2.58f,275,Pose::Display,false,false,0,Part::Head},
        {"face-side",-1.57079633f,0,2.58f,275,Pose::Display,false,false,0,Part::Head},{"chest",-.4f,.02f,1.72f,470,Pose::Display,false,false,0,Part::Torso},
        {"head-mount",-.70f,.02f,2.15f,390,Pose::Display,false,false,0,Part::Torso,Part::Head},
        {"mask-assembly",-.80f,.02f,2.38f,330,Pose::Display,false,false,0,Part::Head},
        {"leg",-.4f,.02f,.60f,360,Pose::Display,false,false,.48f,Part::Feet,Part::Thighs,1},{"leg-side",1.57079633f,.02f,.60f,360,Pose::Display,false,false,.48f,Part::Feet,Part::Thighs,1},
        {"shoulder",-.35f,.02f,1.83f,315,Pose::Display,false,false,.95f,Part::Shoulders,Part::Arms,1},
        {"funnels",0,.25f,2.15f,220,Pose::Display,true,false,0,Part::Funnels},{"funnels-rear",3.14159265f,.15f,2.10f,220,Pose::Display,true,false,0,Part::Funnels},
        {"rifle",-.7f,.02f,1.22f,245,Pose::Display,true,false,-1.1f,Part::Rifle},{"shield",-.2f,.02f,1.52f,250,Pose::Display,true,false,1.3f,Part::Shield},
        {"grip-assembly",-.8f,.08f,1.1f,420,Pose::Display,true,false,-1.05f,Part::Arms,Part::Rifle,-1},
        {"grip-side",1.57079633f,.08f,1.1f,420,Pose::Display,true,false,-1.05f,Part::Arms,Part::Rifle,-1},
        {"shield-mount",-2.15f,.10f,1.35f,360,Pose::Display,true,false,1.2f,Part::Shoulders,Part::Shield,1},
        {"shield-side",-1.57079633f,.08f,1.35f,360,Pose::Display,true,false,1.2f,Part::Shoulders,Part::Shield,1},
        {"backpack",2.75f,.3f,1.75f,300,Pose::Display,true,false,0,Part::Backpack,Part::Funnels},
        {"top",-.4f,.70f,1.70f,125,Pose::Display,true,false},{"underside",-.4f,-.30f,1.70f,125,Pose::Display,true,false},
    };
    const Shot* shots=strike?strikeShots:nu?nuShots:zaku?zakuShots:sazabi?sazabiShots:rxShots;
    const size_t shotCount=strike?sizeof(strikeShots)/sizeof(Shot):nu?sizeof(nuShots)/sizeof(Shot):zaku?sizeof(zakuShots)/sizeof(Shot):sazabi?sizeof(sazabiShots)/sizeof(Shot):sizeof(rxShots)/sizeof(Shot);
    for(size_t shot=0;shot<shotCount;++shot){const auto& s=shots[shot];
        if(strike)buildStrikeGundam(*mesh,{s.equipment,false,s.gray,s.pose},static_cast<StrikeStage>(stage));
        else if(nu)buildNuGundam(*mesh,{s.equipment,false,s.gray,s.pose},stage);
        else if(zaku)buildCharZaku(*mesh,{s.equipment,false,s.gray,s.pose},static_cast<ZakuStage>(stage));
        else if(sazabi)buildSazabi(*mesh,{s.equipment,false,s.gray,s.pose},static_cast<SazabiStage>(stage));
        else buildRx78(*mesh,{s.equipment,false,s.gray,s.pose});
        assert(!mesh->overflowed);
        const float cy=cos(s.yaw),sy=sin(s.yaw),cp=cos(s.pitch),sp=sin(s.pitch);
        const Point eye{s.centerX-7*sy*cp,7*sp+s.pivot,7*cy*cp};
        const auto transform=[&](Point p,uint8_t){p.x-=s.centerX;p.y-=s.pivot;const float x=p.x*cy+p.z*sy,z=p.z*cy-p.x*sy;
            return lets_and_go::TrackCameraPoint{x,p.y*cp-z*sp,7-(z*cp+p.y*sp)};};
        lets_and_go::TrackCamera camera{};camera.principalX=320;camera.principalY=320;camera.focalLength=s.scale*7;
        canvas.fillScreen(0x1083);raster->begin(0,0);
        for(int pass=0;pass<2;++pass)for(size_t i=0;i<mesh->count;++i){
            if(s.only!=Part::Count && (mesh->parts[i]<s.only || mesh->parts[i]>(s.through==Part::Count?s.only:s.through)))continue;
            if(s.limb){
                float x=0;for(auto p:mesh->panels[i].point)x+=p.x;
                if(x*s.limb<=0)continue;
            }
            const float facing=dot(mesh->normals[i],subtract(eye,mesh->panels[i].point[0]));
            if((pass==0)!=(facing<=0))continue;
            if(!mesh->twoSided[i] && facing<-.035f)continue;
            lets_and_go::PreparedCarPanel panel{};
            lets_and_go::prepareCarPanel(panel,camera,mesh->panels[i],transform);
            if(panel.visibility && panel.right>=0 && panel.left<640 && panel.bottom>=0 && panel.top<640)raster->preparedPanel(camera,panel);
        }
        raster->blitScaled(canvas,0,0,640,640);
        canvas.setTextColor(0xef5d,0x1083);canvas.setTextSize(1);
        canvas.drawString(strike?"SD AILE STRIKE / SDEX 002 / SAME ASSET + RASTER":nu?"SD RX-93 NU / BB 387 / SAME ASSET + RASTER":zaku?"SD MS-06S CHAR ZAKU II / SAME ASSET + RASTER":sazabi?"SD MSN-04 SAZABI / SDEX 017 / SAME ASSET + RASTER":"SD RX-78-2 / POSE V5 / SAME ASSET + RASTER",320,18);
        canvas.drawString(s.name,320,622);
        std::ofstream file(out+"/study-"+s.name+".ppm",std::ios::binary);
        file<<"P6\n640 640\n255\n";
        for(auto c:canvas.frame()){
            const unsigned char rgb[]={static_cast<unsigned char>(((c>>11)&31)*255/31),
                static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};
            file.write(reinterpret_cast<const char*>(rgb),3);
        }
        printf("%s panels=%zu pose=%u yaw=%.3f pitch=%.3f pivot=%.3f focal=%.0f\n",s.name,mesh->count,unsigned(s.pose),s.yaw,s.pitch,s.pivot,s.scale*7);
    }
}
