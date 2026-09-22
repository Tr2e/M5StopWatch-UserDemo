#pragma once
#include "../model/rx78.h"
#include "../../app_lets_and_go_racer/view/car_surface_raster.h"
#include <cstring>
#include <memory>
#include <new>

namespace gundam_museum {
// View Car's shared-vertex approach, sized for the museum's 4096 panels.
// Project lazily: vertices belonging only to culled panels cost no transforms.
// Face colors, UVs and the original two-pass draw order remain independent.
struct MuseumProjectionCache {
    static constexpr std::size_t corners=Mesh::capacity*4;
    std::array<uint16_t,corners> indices{};
    std::array<lets_and_go::TrackCameraPoint,corners> projected{};
    std::array<uint8_t,corners> ready{};
    lets_and_go::RenderScratch<std::array<uint8_t,corners>> fastReady;
    lets_and_go::RenderScratchBuffer<lets_and_go::TrackCameraPoint> fastProjected;
    std::array<int8_t,Mesh::capacity> passes{};
    std::size_t count=0,transformed=0;
    float maximumHorizontalRadiusSquared=0,minY=0,maxY=0;

    void index(const Mesh& mesh) {
        // At most 50% occupied, including a mesh with no shared vertices.
        auto slots=std::unique_ptr<std::array<uint16_t,corners*2>>(
            new(std::nothrow) std::array<uint16_t,corners*2>{});
        count=0;maximumHorizontalRadiusSquared=0;
        minY=1e20f;maxY=-1e20f;
        for(std::size_t corner=0;corner<mesh.count*4;++corner) {
            if(!slots){indices[corner]=uint16_t(count++);continue;}
            const auto p=mesh.panels[corner/4].point[corner%4];
            maximumHorizontalRadiusSquared=std::max(maximumHorizontalRadiusSquared,p.x*p.x+p.z*p.z);
            minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
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
    bool nearPlaneSafe(float pivot) const {
        const float vertical=std::max(std::abs(minY-pivot),std::abs(maxY-pivot));
        constexpr float available=7.f-lets_and_go::kTrackNearPlane;
        return maximumHorizontalRadiusSquared+vertical*vertical<available*available;
    }
    bool preferInternalReady(){return fastReady.allocate();}
    bool preferInternalProjected(){return fastProjected.allocate(count);}
    void setInternalReadyFastPath(bool enabled){useFastReady=enabled && fastReady.get();}
    void setInternalProjectedFastPath(bool enabled){
        useFastProjected=enabled && fastProjected.get() && fastProjected.capacity()>=count;
    }
    uint8_t* readyData(){return useFastReady?fastReady.get()->data():ready.data();}
    lets_and_go::TrackCameraPoint* projectedData(){return useFastProjected?fastProjected.get():projected.data();}
    int8_t* passesData(std::size_t faceCount,bool useReadyTail) {
        // The internal ready allocation retains the 16K capacity ceiling, but
        // RX-78 uses only its active unique-vertex prefix. Face-pass state has
        // the same frame lifetime and can safely occupy the unused tail.
        if(useReadyTail && useFastReady && count+faceCount<=corners)
            return reinterpret_cast<int8_t*>(fastReady.get()->data()+count);
        return passes.data();
    }
    uint16_t* bandIndicesData(std::size_t faceCount,bool useReadyTail) {
        if(!useReadyTail || !useFastReady)return nullptr;
        std::size_t offset=count+faceCount;
        offset=(offset+alignof(uint16_t)-1)&~(alignof(uint16_t)-1);
        if(offset+2*faceCount*sizeof(uint16_t)>corners)return nullptr;
        return reinterpret_cast<uint16_t*>(fastReady.get()->data()+offset);
    }
    bool usingInternalProjected() const{return useFastProjected;}
    void begin(){std::fill_n(readyData(),count,uint8_t(0));transformed=0;}

    template<class Transform> void panel(lets_and_go::PreparedCarPanel& result,
        const lets_and_go::TrackCamera& camera,const lets_and_go::CarPanel& face,
        std::size_t index,Transform transform) {
        auto* status=readyData();auto* points=projectedData();
        for(unsigned i=0;i<4;++i) {
            const auto key=indices[index*4+i];
            if(!status[key]) {
                const auto p=transform(face.point[i],face.wheel);++transformed;
                if(p.z<lets_and_go::kTrackNearPlane)points[key]={0,0,0};
                else {
                    const float inverse=1/p.z;
                    points[key]={camera.principalX+camera.focalLength*p.x*inverse,
                                 camera.principalY-camera.focalLength*p.y*inverse,inverse};
                }
                status[key]=1;
            }
            if(points[key].z==0) {
                lets_and_go::prepareCarPanel(result,camera,face,transform);return;
            }
        }
        result.visibility=1;result.color=face.color;result.paint=face.paint;result.light=face.light;
        result.left=result.top=1e20f;result.right=result.bottom=-1e20f;
        new (&result.screen) decltype(result.screen);
        for(unsigned i=0;i<4;++i) {
            const auto p=points[indices[index*4+i]];
            result.screen[i]={p.x,p.y,p.z,((i==0 || i==3 ? face.u0 : face.u1)/255.f)*p.z,
                                         ((i<2 ? face.v0 : face.v1)/255.f)*p.z};
            result.left=std::min(result.left,p.x);result.right=std::max(result.right,p.x);
            result.top=std::min(result.top,p.y);result.bottom=std::max(result.bottom,p.y);
        }
    }

    // Solid parallel exhibits never consume perspective UVs or generic paint
    // state. Build their compact replay record directly while preserving the
    // same lazy projection, bounds and near-plane fallback as panel().
    template<bool TrustedNearPlane=false,class Transform> void solidPanel(lets_and_go::PreparedSolidRasterPanel& result,
        float& left,float& right,float& top,float& bottom,const lets_and_go::TrackCamera& camera,
        const lets_and_go::CarPanel& face,std::size_t index,Transform transform) {
        auto* status=readyData();auto* points=projectedData();
        for(unsigned i=0;i<4;++i) {
            const auto key=indices[index*4+i];
            if(!status[key]) {
                const auto p=transform(face.point[i],face.wheel);++transformed;
                if constexpr(!TrustedNearPlane) {
                    if(p.z<lets_and_go::kTrackNearPlane)points[key]={0,0,0};
                    else {
                        const float inverse=1/p.z;
                        points[key]={camera.principalX+camera.focalLength*p.x*inverse,
                                     camera.principalY-camera.focalLength*p.y*inverse,inverse};
                    }
                } else {
                    const float inverse=1/p.z;
                    points[key]={camera.principalX+camera.focalLength*p.x*inverse,
                                 camera.principalY-camera.focalLength*p.y*inverse,inverse};
                }
                status[key]=1;
            }
            if constexpr(!TrustedNearPlane)if(points[key].z==0) {
                lets_and_go::PreparedCarPanel generic{};
                lets_and_go::prepareCarPanel(generic,camera,face,transform);
                const auto selected=lets_and_go::compactSolidPanel(generic);
                result=lets_and_go::compactSolidRasterPanel(selected);
                if(indices[index*4+2]==indices[index*4+3])
                    result.visibility|=lets_and_go::kPreparedSolidTriangle;
                left=generic.left;right=generic.right;top=generic.top;bottom=generic.bottom;return;
            }
        }
        result.visibility=uint8_t(1|lets_and_go::kPreparedSolidTrustedDepth|
            (indices[index*4+2]==indices[index*4+3] ?
            lets_and_go::kPreparedSolidTriangle:0));result.color=face.color;result.light=face.light;
        left=top=1e20f;right=bottom=-1e20f;
        for(unsigned i=0;i<4;++i) {
            const auto p=points[indices[index*4+i]];
            result.vertex[i]={p.x,p.y,p.z};
            left=std::min(left,p.x);right=std::max(right,p.x);
            top=std::min(top,p.y);bottom=std::max(bottom,p.y);
        }
    }
    // Trusted RX-78 path: keep the exact projected triplets in the shared
    // cache and emit only their compact keys into the ordered command stream.
    template<class Transform> void solidIndexedPanel(lets_and_go::PreparedIndexedSolidRasterPanel& result,
        float& left,float& right,float& top,float& bottom,const lets_and_go::TrackCamera& camera,
        const lets_and_go::CarPanel& face,std::size_t index,Transform transform) {
        auto* status=readyData();auto* points=projectedData();
        result.visibility=uint8_t(1|lets_and_go::kPreparedSolidTrustedDepth|
            (indices[index*4+2]==indices[index*4+3] ? lets_and_go::kPreparedSolidTriangle:0));
        result.color=face.color;result.light=face.light;
        left=top=1e20f;right=bottom=-1e20f;
        for(unsigned i=0;i<4;++i) {
            const auto key=indices[index*4+i];
            if(!status[key]) {
                const auto p=transform(face.point[i],face.wheel);++transformed;
                const float inverse=1/p.z;
                points[key]={camera.principalX+camera.focalLength*p.x*inverse,
                             camera.principalY-camera.focalLength*p.y*inverse,inverse};
                status[key]=1;
            }
            result.vertex[i]=key;
            const auto p=points[key];
            left=std::min(left,p.x);right=std::max(right,p.x);
            top=std::min(top,p.y);bottom=std::max(bottom,p.y);
        }
    }
private:
    bool useFastReady=false;
    bool useFastProjected=false;
};
} // namespace gundam_museum
