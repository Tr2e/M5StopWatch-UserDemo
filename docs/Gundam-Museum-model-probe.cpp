#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include <iostream>
#include <memory>
using namespace lets_and_go;
int main() {
  std::cout << "sizeof CarPanel=" << sizeof(CarPanel) << " mesh=" << sizeof(CarDisplayMesh)
            << " surface=" << sizeof(GarageSurfaceCache) << " renderer=" << sizeof(GarageRenderer) << '\n';
  auto mesh=std::make_unique<CarDisplayMesh>();
  auto cache=std::make_unique<GarageInspectionCache>();
  for(unsigned id=0;id<kCarCount;++id) {
    buildCarDisplayMesh(static_cast<CarId>(id),*mesh);
    cache->index(*mesh);
    size_t solid=0,triangles=0;
    for(size_t i=0;i<mesh->count;++i) {
      const auto &f=mesh->panels[i];solid+=f.paint==CarPaint::Solid;
      for(int j=1;j<3;++j){
        auto a=f.point[0],b=f.point[j],c=f.point[j+1];
        float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
        float x=uy*vz-uz*vy,y=uz*vx-ux*vz,z=ux*vy-uy*vx;
        triangles+=(x*x+y*y+z*z)>1e-16f;
      }
    }
    std::cout << carSpec(static_cast<CarId>(id)).shortName << " panels=" << mesh->count
              << " nondegenerate_triangles=" << triangles << " unique_vertices=" << cache->count
              << " solid_panels=" << solid << " overflow=" << mesh->overflowed << '\n';
  }
}
