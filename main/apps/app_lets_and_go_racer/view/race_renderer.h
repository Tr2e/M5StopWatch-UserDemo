#pragma once

#include "../controller/game_flow.h"
#include "../controller/race_controller.h"
#include "../controller/results_selection.h"
#include "../model/car_catalog.h"
#include "render_budget.h"
#include "pencil_scene.h"
#include "car_surface_raster.h"
#include "track_minimap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace lets_and_go {

struct RaceSurfaceMesh {
    static constexpr std::size_t kMaximumPanels=1536u;
    static constexpr std::size_t kMaximumVertices=kMaximumPanels*4u;
    static_assert(kMaximumVertices<8192u,"vertex hash table needs an empty slot");
    std::array<CarPanel,kMaximumPanels> panels{};
    std::array<uint16_t,kMaximumVertices> cornerIndex{},vertexCorner{};
    std::size_t vertexCount=0;
    std::size_t count=0;
    CarSurfaceDetail detail=CarSurfaceDetail::Medium;
    void indexVertices(std::array<uint16_t,8192>& slots) {
        slots.fill(0);vertexCount=0;
        for(std::size_t corner=0;corner<count*4;++corner) {
            const auto& face=panels[corner/4];const auto p=face.point[corner%4];
            uint32_t hash=2166136261u;
            for(float value:{p.x,p.y,p.z}) {
                uint32_t bits=0;if(value!=0)std::memcpy(&bits,&value,sizeof(bits));
                hash=(hash^bits)*16777619u;
            }
            hash=(hash^face.wheel)*16777619u;
            std::size_t slot=hash&(slots.size()-1);
            while(slots[slot]) {
                const auto key=vertexCorner[slots[slot]-1];
                const auto& candidate=panels[key/4];const auto q=candidate.point[key%4];
                if(p.x==q.x && p.y==q.y && p.z==q.z && face.wheel==candidate.wheel)break;
                slot=(slot+1)&(slots.size()-1);
            }
            if(!slots[slot]) {
                vertexCorner[vertexCount]=uint16_t(corner);
                slots[slot]=uint16_t(++vertexCount);
            }
            cornerIndex[corner]=slots[slot]-1;
        }
    }
};
struct RaceProjectedVertex {
    TrackCameraPoint camera;
    TrackScreenPoint screen;
    float inverse=0;
};
struct RaceSurfaceCache {
    std::array<RaceSurfaceMesh,kMaximumRaceCars> meshes{};
    std::array<CarId,kMaximumRaceCars> cars=[] {
        std::array<CarId,kMaximumRaceCars> ids{};
        ids.fill(CarId::Count);
        return ids;
    }();
    CarSurfaceRaster<112,112> raster{};
    // One-car scratch shared across the roster; project each panel once per frame.
    std::array<PreparedCarPanel,RaceSurfaceMesh::kMaximumPanels+12u> preparedPanels{};
    std::array<RaceProjectedVertex,RaceSurfaceMesh::kMaximumVertices> projectedVertices{};
    std::array<uint16_t,8192> vertexSlots{};
    PencilOcclusion occlusion{};
    CarSurfaceDetail detail=CarSurfaceDetail::Medium;
};
static_assert(sizeof(RaceSurfaceCache)<=906000u,"race surface working-set budget");

struct RaceCarStages {
    uint32_t prepareUs=0,rasterUs=0,blitUs=0;
    CarBlitWork work{};
};

#ifdef ESP_PLATFORM
using SceneUpscalePixel=lgfx::swap565_t;
#else
using SceneUpscalePixel=uint16_t;
#endif

struct RaceRenderStages {
    uint32_t roadSurfaces=0;
    uint32_t trackUs=0,playerUs=0,opponentsUs=0,upscaleUs=0,hudUs=0;
    RaceCarStages cars{};
};

class RaceRenderer {
public:
    void open(int width, int height, bool halfResolution = false, bool raceCaches = false,
              bool playerQuality = false, bool wireframeTrack = false);
    void close();
    void setTrackCulling(bool enabled) { _trackCulling=enabled; }
    void setEdgeUpscale(bool enabled) { _edgeUpscale=enabled; }
    bool edgeUpscaleActive() const { return _edgeUpscale && _edgeRow.get(); }
    void setRowOcclusionFilter(bool enabled) { _rowOcclusionFilter=enabled; }
    void setBulkSceneCopy(bool enabled) { _bulkSceneCopy=enabled; }
    const RaceRenderStages& stages() const { return _stages; }
    bool halfResolutionActive() const { return bool(_scene); }
    void render(lgfx::LGFXBase& canvas, LGFX_Sprite* canvasBuffer,
                const GameFlow& flow, const RaceController& race,
                const ResultsSelection& results,
                uint32_t screenElapsedMs, bool pausedForInputLoss,
                PencilDetail detail, bool deviceControls = false);
    void render(const GameFlow& flow, const RaceController& race,
                const ResultsSelection& results,
                uint32_t screenElapsedMs, bool pausedForInputLoss,
                PencilDetail detail, bool deviceControls = false);

private:
    std::unique_ptr<RaceSurfaceCache> _surface;
    std::unique_ptr<LGFX_Sprite> _scene;
    std::unique_ptr<LGFX_Sprite> _miniMapImage;
    std::unique_ptr<RacePaintAtlas> _paintAtlas;
    bool _trackCulling=true;
    PencilTrack _trackGeometry{};
    TrackId _cachedTrack = TrackId::Count;
    TrackMiniMap _miniMap{};
    int _width = 0;
    int _height = 0;
    RaceRenderStages _stages{};
    bool _rowOcclusionFilter=true;
    bool _bulkSceneCopy=true;
    bool _edgeUpscale=false;
    struct EdgeRows {
        std::array<std::array<uint16_t,240>,3> source;
        // Both destination rows are contiguous so the display backend can
        // submit one 2-row window instead of two 1-row windows.
        std::array<uint16_t,960> output;
    };
    RenderScratch<EdgeRows> _edgeRow;
    bool _playerQuality=false;
    bool _wireframeTrack=false;
};

static_assert(sizeof(RaceRenderer) <= 9000u,
              "race renderer cache exceeded its reviewed resident budget");

}  // namespace lets_and_go
