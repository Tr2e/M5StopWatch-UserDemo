#include "../../../main/apps/app_lets_and_go_racer/view/garage_car_transform.h"
#include <hal/hal.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <set>
#include <tuple>
using namespace lets_and_go;
using Clock=std::chrono::steady_clock;
template<int W,int H> double measure(const CarSpec& spec,const CarDisplayMesh& mesh,
                                    const std::array<float,18>& scales,const RacePaintAtlas* atlas) {
    auto raster=std::make_unique<CarSurfaceRaster<W,H>>();
    raster->setPaintAtlas(atlas);
    LGFX_Sprite canvas;canvas.createSprite(W,H);
    double total=0;
    for(int repeat=0;repeat<3;++repeat)for(int frame=0;frame<18;++frame) {
        const float ratio=float(W)/352;
        TrackCamera camera{};camera.principalX=W/2;camera.principalY=H*.55f;
        camera.focalLength=scales[frame]*5.8f*ratio;
        GarageCarTransform transform(spec,frame*6.2831853f/18,.65f,0);
        const auto start=Clock::now();
        raster->begin(0,0);
        for(std::size_t i=0;i<mesh.count;++i)raster->panel(camera,mesh.panels[i],transform);
        raster->blit(canvas);
        total+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
    }
    return total/54;
}
int main() {
    std::puts("car,high_panels,low_panels,unique_vertices,atlas_slots,fit_ms,native_high_ms,native_low_ms,atlas32_high_ms,half_high_ms,half_atlas32_high_ms,three_quarter_high_ms");
    for(unsigned car=0;car<kCarCount;++car) {
        auto high=std::make_unique<CarDisplayMesh>(),low=std::make_unique<CarDisplayMesh>();
        auto atlas=std::make_unique<RacePaintAtlas>();
        const auto id=CarId(car);const auto& spec=carSpec(id);
        buildCarDisplayMesh(id,*high,CarSurfaceDetail::High);buildCarDisplayMesh(id,*low,CarSurfaceDetail::Low);
        std::set<std::tuple<float,float,float,uint8_t>> vertices;
        for(std::size_t i=0;i<high->count;++i) {
            const auto& panel=high->panels[i];atlas->add(panel.paint,panel.color,panel.light);
            for(auto p:panel.point)vertices.emplace(p.x,p.y,p.z,panel.wheel);
        }
        std::array<float,18> scales{};
        const auto start=Clock::now();
        for(int frame=0;frame<18;++frame)scales[frame]=inspectionScale(spec,*high,frame*6.2831853f/18,.65f);
        const double fit=std::chrono::duration<double,std::milli>(Clock::now()-start).count()/18;
        const double native=measure<352,288>(spec,*high,scales,nullptr);
        const double nativeLow=measure<352,288>(spec,*low,scales,nullptr);
        const double textured=measure<352,288>(spec,*high,scales,atlas.get());
        const double threeQuarter=measure<264,216>(spec,*high,scales,nullptr);
        const double half=measure<176,144>(spec,*high,scales,nullptr);
        const double halfTextured=measure<176,144>(spec,*high,scales,atlas.get());
        std::printf("%s,%zu,%zu,%zu,%u,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",spec.shortName,high->count,low->count,vertices.size(),atlas->count(),fit,native,nativeLow,textured,half,halfTextured,threeQuarter);
    }
}
