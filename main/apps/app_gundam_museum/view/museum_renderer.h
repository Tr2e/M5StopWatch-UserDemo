#pragma once
#include "../model/rx78.h"
#include "../../app_lets_and_go_racer/view/car_surface_raster.h"
#include <memory>

namespace gundam_museum {
struct View {
    float yaw=-.40f,pitch=.10f;
    bool equipment=true,detail=false,automatic=false;
};
struct RenderStats {std::size_t total=0,culled=0,submitted=0,offscreen=0;};
class MuseumRenderer {
public:
    bool open();
    void close();
    bool ready() const{return bool(_surface);}
    // CPU culling is optional for regression against the same production raster.
    void render(lgfx::LGFXBase& canvas,const View& view,int percent=100,bool cull=true,bool gray=false,bool keepBuried=false,bool partial=false);
    const RenderStats& stats() const{return _stats;}
    const Mesh& mesh() const{return _surface->mesh;}
    static std::size_t workingBytes();
private:
    struct Surface {
        Mesh mesh;
        lets_and_go::CarSurfaceRaster<352,288> raster;
    };
    std::unique_ptr<Surface> _surface;
    RenderStats _stats{};
    bool _cached=false,_equipment=false,_gray=false,_buried=false;
};
} // namespace gundam_museum
