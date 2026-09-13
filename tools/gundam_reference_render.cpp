// Structural inspection only: same authored asset and production raster, larger
// host tile. These frames are not native device screenshots or FPS evidence.
#include "../main/apps/app_gundam_museum/view/museum_renderer.h"
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
    struct Shot{const char* name;float yaw,pitch,pivot,scale;Pose pose;bool equipment,gray;float centerX=0;Part only=Part::Count;};
    const Shot shots[]={
        {"standing",-.40f,.025f,1.65f,159,Pose::Display,true,false},
        {"front",0,.025f,1.61f,174,Pose::Display,false,false},
        {"rear",3.14159265f,.025f,1.61f,174,Pose::Display,false,false},
        {"rear-equipped",3.14159265f,.05f,1.61f,164,Pose::Display,true,false},
        {"side",1.57079633f,.025f,1.61f,174,Pose::Display,false,false},
        {"other-side",-1.57079633f,.025f,1.61f,174,Pose::Display,false,false},
        {"gray",-.40f,.025f,1.61f,174,Pose::Display,false,true},
        {"head",-.40f,.04f,2.92f,820,Pose::Display,false,false},
        {"head-front",0,.025f,2.92f,820,Pose::Display,false,false},
        {"head-side",1.57079633f,.025f,2.92f,820,Pose::Display,false,false},
        {"salute",.16f,.08f,1.56f,177,Pose::Salute,false,false},
        {"saber",-.40f,-.12f,1.65f,150,Pose::Saber,false,false,.35f},
        {"rifle",-.40f,.025f,1.43f,345,Pose::Display,true,false,-.82f,Part::Rifle},
        {"shield",-.40f,.025f,2.02f,265,Pose::Display,true,false,.84f,Part::Shield},
        {"chest",-.40f,.025f,2.32f,650,Pose::Display,false,false},
    };
    for(const auto& s:shots){
        buildRx78(*mesh,{s.equipment,false,s.gray,s.pose});assert(!mesh->overflowed);
        const float cy=cos(s.yaw),sy=sin(s.yaw),cp=cos(s.pitch),sp=sin(s.pitch);
        const Point eye{s.centerX-7*sy*cp,7*sp+s.pivot,7*cy*cp};
        const auto transform=[&](Point p,uint8_t){p.x-=s.centerX;p.y-=s.pivot;const float x=p.x*cy+p.z*sy,z=p.z*cy-p.x*sy;
            return lets_and_go::TrackCameraPoint{x,p.y*cp-z*sp,7-(z*cp+p.y*sp)};};
        lets_and_go::TrackCamera camera{};camera.principalX=320;camera.principalY=320;camera.focalLength=s.scale*7;
        canvas.fillScreen(0x1083);raster->begin(0,0);
        for(int pass=0;pass<2;++pass)for(size_t i=0;i<mesh->count;++i){
            if(s.only!=Part::Count && mesh->parts[i]!=s.only)continue;
            const float facing=dot(mesh->normals[i],subtract(eye,mesh->panels[i].point[0]));
            if((pass==0)!=(facing<=0))continue;
            if(!mesh->twoSided[i] && facing<-.035f)continue;
            lets_and_go::PreparedCarPanel panel{};
            lets_and_go::prepareCarPanel(panel,camera,mesh->panels[i],transform);
            if(panel.visibility && panel.right>=0 && panel.left<640 && panel.bottom>=0 && panel.top<640)raster->preparedPanel(camera,panel);
        }
        raster->blitScaled(canvas,0,0,640,640);
        canvas.setTextColor(0xef5d,0x1083);canvas.setTextSize(1);
        canvas.drawString("RX-78-2 / STRUCTURE STUDY / SAME C++ ASSET + RASTER",320,18);
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
