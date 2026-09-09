#include TRACK_HEADER
#include <cstdio>
#include <memory>
using namespace lets_and_go;
int main() {
 auto occlusion=std::make_unique<PencilOcclusion>();
 for(int size : {234,468}) {
  LGFX_Sprite canvas;canvas.createSprite(size,size==234 ? 233 : 466);
  for(auto id : {TrackId::SkyLoop,TrackId::TriCross}) {
   OverpassTrack course(id);PencilTrack track;track.open(course);
   for(int pose=0;pose<192;++pose)for(float lateral : {-.7f,0.f,.7f}) {
    auto camera=makeRacerChaseCamera(course.sample(course.length()*pose/192),lateral,size,canvas.height());
    canvas.fillScreen(0);
    drawPencilTrack(canvas,camera,track,PencilDetail::Low,occlusion.get(),false,true);
    uint64_t hash=14695981039346656037ull;
    for(auto pixel:canvas.frame()) {hash^=pixel!=0;hash*=1099511628211ull;}
    std::printf("%d %d %d %.1f %llu\n",size,int(id),pose,lateral,(unsigned long long)hash);
   }
  }
 }
}
