#include SCENE_HEADER
#include <memory>
using namespace lets_and_go;
int main(int argc,char**argv){auto& canvas=GetHAL().getCanvas();canvas.createSprite(234,233);OverpassTrack road(TrackId::GrandSpiral);PencilTrack track;track.open(road);auto surfaces=std::make_unique<PencilOcclusion>();auto camera=makeRacerChaseCamera(road.sample(road.length()*265/720),0,234,233);canvas.fillScreen(track_paint::floor);drawPencilTrack(canvas,camera,track,PencilDetail::Low,surfaces.get(),false);canvas.save(argv[1]);return argc<2;}
