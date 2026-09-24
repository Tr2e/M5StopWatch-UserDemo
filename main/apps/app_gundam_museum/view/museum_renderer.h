#pragma once
#include "../model/rx78.h"
#include "../model/museum_model_asset.h"
#include "../../common/soft3d/raster/surface_raster.h"
#include "museum_projection_cache.h"
#include <memory>

namespace gundam_museum {
#ifdef ESP_PLATFORM
struct MuseumParallelWorker;
#endif
struct View {
    float yaw=-.40f,pitch=.10f;
    bool equipment=true,detail=false,automatic=false;
    Pose pose=Pose::Display;
};
struct RenderStats {
    std::size_t total=0,culled=0,submitted=0,offscreen=0,vertices=0,transformed=0;
    std::size_t totalTriangles=0,submittedTriangles=0;
    std::size_t totalQuads=0,submittedQuads=0;
    uint32_t clearUs=0,prepareUs=0,rasterUs=0,blitUs=0,overlayUs=0;
    uint32_t backgroundUs=0,spaceUs=0,depthClearUs=0;
    uint32_t panelPrepareUs=0,spaceWaitUs=0,mainRasterUs=0,workerRasterUs=0;
    uint32_t workerStackFree=0;
    uint32_t internalProjectedBytes=0,internalDepthBytes=0,internalCommandBytes=0;
    uint32_t fastPathFlags=0;
};
class MuseumRenderer {
public:
    MuseumRenderer();
    ~MuseumRenderer();
    bool open(int preferredPercent=100);
    void close();
    bool ready() const{return bool(_surface);}
    // CPU culling is optional for regression against the same production raster.
    void render(lgfx::LGFXBase& canvas,const View& view,int percent=100,bool cull=true,bool gray=false,bool keepBuried=false,bool partial=false);
    const RenderStats& stats() const{return _stats;}
    const Mesh& mesh() const{return _surface->mesh;}
    const soft3d::ModelAsset* asset() const{return _surface?_surface->instance.asset:nullptr;}
    static std::size_t workingBytes();
    void configure(const soft3d::RenderProfile& profile,
                   const soft3d::RenderCapabilities& capabilities={}) {
        _profile=profile;_capabilities=capabilities;
    }
    // Diagnostic A/B switch; production always uses the optimized path.
    void setOptimizations(bool enabled){_optimizations=enabled;}
    void setSolidSpanFastPath(bool enabled){_solidSpanFastPath=enabled;}
    void setTrustedSolidDepthFastPath(bool enabled){_trustedSolidDepthFastPath=enabled;}
    void setSolidQuadFastPath(bool enabled){_solidQuadFastPath=enabled;}
    void setDirectSpanFastPath(bool enabled){_directSpanFastPath=enabled;}
    void setNativeFrameBufferFastPath(bool enabled){_nativeFrameBufferFastPath=enabled;}
    void setSparseCompositeFastPath(bool enabled){_sparseCompositeFastPath=enabled;}
    void setNativeClearFastPath(bool enabled){_nativeClearFastPath=enabled;}
    void setSparseDepthClearFastPath(bool enabled){_sparseDepthClearFastPath=enabled;}
    void setParallelRasterFastPath(bool enabled){_parallelRasterFastPath=enabled;}
    void setSplitColorFastPath(bool enabled){_splitColorFastPath=enabled;}
    void setCompactPanelPrepareFastPath(bool enabled){_compactPanelPrepareFastPath=enabled;}
    void setInternalProjectionReadyFastPath(bool enabled){_internalProjectionReadyFastPath=enabled;}
    void setInternalProjectedPointFastPath(bool enabled){_internalProjectedPointFastPath=enabled;}
    void setInternalFacePassFastPath(bool enabled){_internalFacePassFastPath=enabled;}
    void setInternalBandIndexFastPath(bool enabled){_internalBandIndexFastPath=enabled;}
    void setIndexedPanelFastPath(bool enabled){_indexedPanelFastPath=enabled;}
    void setSplitDepthFastPath(bool enabled){_splitDepthFastPath=enabled;}
    void setSparseDepthSpanClearFastPath(bool enabled){_sparseDepthSpanClearFastPath=enabled;}
    // Diagnostic isolation of model coverage; the product keeps the room on.
    void setSpaceEnabled(bool enabled){_spaceEnabled=enabled;}
    void setBackgroundColor(uint16_t color){_backgroundColor=color;_backgroundColorOverride=true;}
    // Diagnostic coverage of the last render, in active raster coordinates.
    // Callers must keep x/y inside that render's internal sampling dimensions.
    bool modelSampleCovered(int x,int y) const{return _surface && _surface->raster.depthAt(x,y)!=0;}
    int sampleWidth() const{return _surface?_surface->raster.width():0;}
    int sampleHeight() const{return _surface?_surface->raster.height():0;}
private:
    struct Surface {
        struct FrameBufferState {
            uint8_t* base=nullptr;
            lets_and_go::CarSparseBlitBounds dirty{};
            bool initialized=false;
        };
        union PreparedCommands {
            lets_and_go::PreparedSolidRasterPanel solid[Mesh::capacity];
            lets_and_go::PreparedIndexedSolidRasterPanel indexed[Mesh::capacity];
            PreparedCommands() {}
            ~PreparedCommands() {}
        };
        Mesh mesh;
        MuseumModelAssetStorage assetStorage;
        soft3d::ModelInstance instance;
        soft3d::SurfaceRaster<424,424> raster;
        MuseumProjectionCache projection;
        std::array<uint8_t,(424*424+7)/8> occupiedDepth{};
        lets_and_go::RenderScratchBuffer<uint8_t> fastOccupiedDepth;
        PreparedCommands preparedPanels;
        lets_and_go::RenderScratch<std::array<uint16_t,276*138>> fastLowerColor;
        lets_and_go::RenderScratch<std::array<uint16_t,276*138>> fastLowerDepth;
        lets_and_go::RenderScratchBuffer<lets_and_go::PreparedIndexedSolidRasterPanel> fastIndexedPanels;
        std::size_t topologyTriangles=0,topologyQuads=0;
        std::array<uint8_t,(Mesh::capacity+7)/8> topologyQuadBits{};
        std::array<FrameBufferState,2> frameBuffers{};
        FrameBufferState& frameBufferState(uint8_t* base) {
            for(auto& state:frameBuffers)if(state.base==base)return state;
            for(auto& state:frameBuffers)if(!state.base){state.base=base;return state;}
            frameBuffers[0]={base};return frameBuffers[0];
        }
    };
    std::unique_ptr<Surface> _surface;
#ifdef ESP_PLATFORM
    std::unique_ptr<MuseumParallelWorker> _parallelWorker;
#endif
    RenderStats _stats{};
    soft3d::RenderProfile _profile=soft3d::game30Profile();
    soft3d::RenderCapabilities _capabilities{};
    bool _cached=false,_equipment=false,_gray=false,_buried=false;
    Pose _pose=Pose::Display;
    bool _optimizations=true;
    bool _solidSpanFastPath=true;
    bool _trustedSolidDepthFastPath=true;
    bool _solidQuadFastPath=true;
    bool _directSpanFastPath=true;
    bool _nativeFrameBufferFastPath=true;
    bool _sparseCompositeFastPath=true;
    bool _nativeClearFastPath=true;
    bool _sparseDepthClearFastPath=true;
    bool _parallelRasterFastPath=true;
    bool _splitColorFastPath=true;
    bool _compactPanelPrepareFastPath=true;
    bool _internalProjectionReadyFastPath=true;
    bool _internalProjectedPointFastPath=true;
    bool _internalFacePassFastPath=true;
    bool _internalBandIndexFastPath=true;
    bool _indexedPanelFastPath=true;
    bool _splitDepthFastPath=true;
    bool _sparseDepthSpanClearFastPath=true;
    bool _spaceEnabled=true;
    bool _backgroundColorOverride=false;
    uint16_t _backgroundColor=0;
};
} // namespace gundam_museum
