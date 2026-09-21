#pragma once
#include "../model/rx78.h"
#include "../../app_lets_and_go_racer/view/car_surface_raster.h"
#include "museum_projection_cache.h"
#include "museum_wireframe.h"
#include <memory>

namespace gundam_museum {
struct View {
    float yaw=-.40f,pitch=.10f;
    bool equipment=true,detail=false,automatic=false;
    Pose pose=Pose::Display;
    ModelId model=ModelId::Rx78;
};
struct RenderStats {
    std::size_t total=0,culled=0,submitted=0,offscreen=0,vertices=0,transformed=0;
    uint32_t clearUs=0,prepareUs=0,rasterUs=0,blitUs=0;
};
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
    // Diagnostic A/B switch; production always uses the optimized path.
    void setOptimizations(bool enabled){_optimizations=enabled;}
    void setSolidSpanFastPath(bool enabled){_solidSpanFastPath=enabled;}
    void setTrustedSolidDepthFastPath(bool enabled){_trustedSolidDepthFastPath=enabled;}
    void setDirectSpanFastPath(bool enabled){_directSpanFastPath=enabled;}
    void setNativeFrameBufferFastPath(bool enabled){_nativeFrameBufferFastPath=enabled;}
    void setSparseDepthClearFastPath(bool enabled){_sparseDepthClearFastPath=enabled;}
    // Diagnostic isolation of model coverage; the product keeps the room on.
    void setSpaceEnabled(bool enabled){_spaceEnabled=enabled;}
    // Diagnostic coverage of the last render, in active raster coordinates.
    // Callers must keep x/y inside that render's internal sampling dimensions.
    bool modelSampleCovered(int x,int y) const{return _surface && _surface->raster.depthAt(x,y)!=0;}
    int sampleWidth() const{return _surface?_surface->raster.width():0;}
    int sampleHeight() const{return _surface?_surface->raster.height():0;}
private:
    struct Surface {
        Mesh mesh;
        lets_and_go::CarSurfaceRaster<424,424> raster;
        MuseumProjectionCache projection;
        EdgeFilter edges;
        std::array<uint8_t,(424*424+7)/8> occupiedDepth{};
    };
    std::unique_ptr<Surface> _surface;
    RenderStats _stats{};
    bool _cached=false,_equipment=false,_gray=false,_buried=false;
    Pose _pose=Pose::Display;
    ModelId _model=ModelId::Rx78;
    bool _optimizations=true;
    bool _solidSpanFastPath=true;
    bool _trustedSolidDepthFastPath=true;
    bool _directSpanFastPath=true;
    bool _nativeFrameBufferFastPath=true;
    bool _sparseDepthClearFastPath=true;
    bool _spaceEnabled=true;
};
} // namespace gundam_museum
