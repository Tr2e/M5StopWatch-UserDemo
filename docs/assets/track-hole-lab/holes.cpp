#include "main/apps/app_lets_and_go_racer/view/pencil_scene.h"
#include <iostream>
#include <memory>
using namespace lets_and_go;
void both(const TrackCamera& camera,const PencilTrack& track,PencilOcclusion& out,LGFX_Sprite& canvas) {
 out.count=0;out.overflowed=false;
 auto tri=[&](TrackVec3 a,TrackVec3 b,TrackVec3 c,uint16_t color){auto s=projectPencilSurface(camera,a,b,c,234,233);s.color=color;out.append(s);};
 auto quad=[&](TrackVec3 a,TrackVec3 b,TrackVec3 c,TrackVec3 d,uint16_t color){tri(a,b,c,color);tri(a,c,d,color);};
 for(std::size_t i=0;i<track.count;++i){auto a=track.left[i],b=track.right[i],c=track.right[i+1],d=track.left[i+1];auto p=track_paint::module(i,track.count);
  TrackVec3 drop{0,-.22f,0},lift{0,.26f,0};
  for(auto edge:{std::array<TrackVec3,2>{a,d},std::array<TrackVec3,2>{b,c}})quad(trackAdd(edge[0],drop),trackAdd(edge[1],drop),trackAdd(edge[1],lift),trackAdd(edge[0],lift),p.wall);
  quad(a,b,c,d,p.deck);quad(trackAdd(a,drop),trackAdd(b,drop),trackAdd(c,drop),trackAdd(d,drop),track_paint::underside);
 }
 out.paint(canvas);
}
int main(){auto out=std::make_unique<PencilOcclusion>();auto& canvas=GetHAL().getCanvas();canvas.createSprite(234,233);int worst=0;
 for(auto id:{TrackId::SkyLoop,TrackId::TriCross,TrackId::GrandSpiral}){OverpassTrack road(id);PencilTrack track;track.open(road);
 for(int f=0;f<720;++f)for(float lane:{-1.36f,0.f,1.36f}){auto camera=makeRacerChaseCamera(road.sample(road.length()*f/720),lane,234,233);
  canvas.fillScreen(track_paint::floor);drawPencilTrack(canvas,camera,track,PencilDetail::Low,out.get(),false);auto before=canvas.frame();
  canvas.fillScreen(track_paint::floor);both(camera,track,*out,canvas);auto after=canvas.frame();int holes=0;
  for(int y=80;y<225;++y)for(int x=15;x<220;++x){auto i=y*234+x;if(before[i]==track_paint::floor && after[i]!=track_paint::floor)++holes;}
  if(holes>worst){worst=holes;std::cout<<"worst "<<worst<<" track="<<int(id)<<" f="<<f<<" lane="<<lane<<" overflow="<<out->overflowed<<std::endl;
   canvas.save("/tmp/track-hole-lab/both.ppm");canvas.fillScreen(track_paint::floor);drawPencilTrack(canvas,camera,track,PencilDetail::Low,out.get(),false);canvas.save("/tmp/track-hole-lab/before.ppm");}
 }
 }
}
