#pragma once
#include "../model/character_model.h"
#include "arena_space.h"
#include "../../app_gundam_museum/view/museum_projection_cache.h"
#include "../../app_lets_and_go_racer/view/car_surface_raster.h"
#include <memory>

namespace gundam_arena {
struct ArenaRenderStats {
    std::size_t total=0,culled=0,submitted=0,offscreen=0,vertices=0,transformed=0;
    uint32_t meshBuilds=0,indexBuilds=0;
};

class ArenaRenderer {
public:
    bool open();
    void close();
    bool ready()const{return bool(_surface);}
    void render(lgfx::LGFXBase& canvas,const CharacterModel& character,const ArenaView& view);
    const ArenaRenderStats& stats()const{return _stats;}
    const Mesh& mesh()const{return _surface->mesh;}
    const Skeleton& skeleton()const{return _skeleton;}
    Skeleton& skeleton(){return _skeleton;}
    static std::size_t workingBytes();
    uint32_t meshBuilds()const{return _meshBuilds;}
    uint32_t indexBuilds()const{return _indexBuilds;}
private:
    struct Surface {
        Mesh mesh;
        lets_and_go::CarSurfaceRaster<424,424> raster;
        gundam_museum::MuseumProjectionCache projection;
    };
    std::unique_ptr<Surface> _surface;
    Skeleton _bind{},_skeleton{};
    ArenaRenderStats _stats{};
    uint32_t _meshBuilds=0,_indexBuilds=0;
};
} // namespace gundam_arena
