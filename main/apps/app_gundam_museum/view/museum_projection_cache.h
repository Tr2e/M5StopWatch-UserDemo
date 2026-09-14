#pragma once
#include "../model/rx78.h"
#include "../../app_lets_and_go_racer/view/car_surface_raster.h"
#include <cstring>
#include <memory>

namespace gundam_museum {
// View Car's shared-vertex approach, sized for the museum's 4096 panels.
// Project lazily: vertices belonging only to culled panels cost no transforms.
// Face colors, UVs and the original two-pass draw order remain independent.
struct MuseumProjectionCache {
    static constexpr std::size_t corners=Mesh::capacity*4;
    std::array<uint16_t,corners> indices{};
    std::array<lets_and_go::TrackCameraPoint,corners> projected{};
    std::array<bool,corners> ready{};
    std::array<int8_t,Mesh::capacity> passes{};
    std::size_t count=0,transformed=0;

    void index(const Mesh& mesh) {
        // At most 50% occupied, including a mesh with no shared vertices.
        auto slots=std::unique_ptr<std::array<uint16_t,corners*2>>(
            new(std::nothrow) std::array<uint16_t,corners*2>{});
        count=0;
        for(std::size_t corner=0;corner<mesh.count*4;++corner) {
            if(!slots){indices[corner]=uint16_t(count++);continue;}
            const auto p=mesh.panels[corner/4].point[corner%4];
            uint32_t hash=2166136261u;
            for(float value:{p.x,p.y,p.z}) {
                uint32_t bits=0;if(value!=0)std::memcpy(&bits,&value,sizeof(bits));
                hash=(hash^bits)*16777619u;
            }
            hash=(hash^mesh.panels[corner/4].wheel)*16777619u;
            std::size_t slot=hash&(slots->size()-1);
            while((*slots)[slot]) {
                const auto key=(*slots)[slot]-1;
                const auto q=mesh.panels[key/4].point[key%4];
                if(p.x==q.x && p.y==q.y && p.z==q.z &&
                   mesh.panels[corner/4].wheel==mesh.panels[key/4].wheel)break;
                slot=(slot+1)&(slots->size()-1);
            }
            if(!(*slots)[slot]) {(*slots)[slot]=uint16_t(corner+1);indices[corner]=uint16_t(count++);}
            else indices[corner]=indices[(*slots)[slot]-1];
        }
    }
    void begin(){std::fill_n(ready.begin(),count,false);transformed=0;}

    template<class Transform> void panel(lets_and_go::PreparedCarPanel& result,
        const lets_and_go::TrackCamera& camera,const lets_and_go::CarPanel& face,
        std::size_t index,Transform transform) {
        for(unsigned i=0;i<4;++i) {
            const auto key=indices[index*4+i];
            if(!ready[key]) {
                const auto p=transform(face.point[i],face.wheel);++transformed;
                if(p.z<lets_and_go::kTrackNearPlane)projected[key]={0,0,0};
                else {
                    const float inverse=1/p.z;
                    projected[key]={camera.principalX+camera.focalLength*p.x*inverse,
                                   camera.principalY-camera.focalLength*p.y*inverse,inverse};
                }
                ready[key]=true;
            }
            if(projected[key].z==0) {
                lets_and_go::prepareCarPanel(result,camera,face,transform);return;
            }
        }
        result.visibility=1;result.color=face.color;result.paint=face.paint;result.light=face.light;
        result.left=result.top=1e20f;result.right=result.bottom=-1e20f;
        new (&result.screen) decltype(result.screen);
        for(unsigned i=0;i<4;++i) {
            const auto p=projected[indices[index*4+i]];
            result.screen[i]={p.x,p.y,p.z,((i==0 || i==3 ? face.u0 : face.u1)/255.f)*p.z,
                                         ((i<2 ? face.v0 : face.v1)/255.f)*p.z};
            result.left=std::min(result.left,p.x);result.right=std::max(result.right,p.x);
            result.top=std::min(result.top,p.y);result.bottom=std::max(result.bottom,p.y);
        }
    }
};
} // namespace gundam_museum
