#pragma once
#include "garage_car_transform.h"

namespace lets_and_go {

// Index geometry only; face UVs, lights and authored draw order stay separate.
// The same 12-byte slot holds a camera point during fitting, then x/y/inverse-z
// after projection. No second array of projected vertices is needed.
struct GarageInspectionCache {
    static constexpr std::size_t kCorners=CarDisplayMesh::kMaximumPanels*4;
    std::array<uint16_t,kCorners> cornerIndex{},vertexCorner{};
    std::array<TrackCameraPoint,kCorners> vertices{};
    std::size_t count=0;
    bool indexed=false;

    void index(const CarDisplayMesh& mesh) {
        auto slots=std::unique_ptr<std::array<uint16_t,16384>>(
            new(std::nothrow) std::array<uint16_t,16384>{});
        count=0;
        for(std::size_t corner=0;corner<mesh.count*4;++corner) {
            if(!slots) {cornerIndex[corner]=vertexCorner[corner]=uint16_t(corner);++count;continue;}
            const auto& face=mesh.panels[corner/4];const auto p=face.point[corner%4];
            uint32_t hash=2166136261u;
            for(float value:{p.x,p.y,p.z}) {
                uint32_t bits=0;if(value!=0)std::memcpy(&bits,&value,sizeof(bits));
                hash=(hash^bits)*16777619u;
            }
            hash=(hash^face.wheel)*16777619u;
            std::size_t slot=hash&(slots->size()-1);
            while((*slots)[slot]) {
                const auto key=vertexCorner[(*slots)[slot]-1];
                const auto& candidate=mesh.panels[key/4];const auto q=candidate.point[key%4];
                if(p.x==q.x && p.y==q.y && p.z==q.z && face.wheel==candidate.wheel)break;
                slot=(slot+1)&(slots->size()-1);
            }
            if(!(*slots)[slot]) {
                vertexCorner[count]=uint16_t(corner);(*slots)[slot]=uint16_t(++count);
            }
            cornerIndex[corner]=(*slots)[slot]-1;
        }
        indexed=true;
    }

    float fit(const CarSpec& spec,const CarDisplayMesh& mesh,float yaw,float pitch) {
        if(!indexed)index(mesh);
        TrackCamera camera{};camera.principalX=0;camera.principalY=0;camera.focalLength=5.8f;
        const GarageCarTransform transform(spec,yaw,pitch,0);
        float scale=150.f;bool fits=true;
        for(std::size_t i=0;i<count;++i) {
            const auto corner=vertexCorner[i];const auto& face=mesh.panels[corner/4];
            const auto p=transform(face.point[corner%4],face.wheel);vertices[i]=p;
            TrackScreenPoint point{};
            if(!projectTrackPoint(camera,p,point)) {fits=false;continue;}
            if(std::abs(point.x)>.0001f)scale=std::min(scale,165.f/std::abs(point.x));
            if(point.y<-.0001f)scale=std::min(scale,-135.f/point.y);
            if(point.y>.0001f)scale=std::min(scale,115.f/point.y);
        }
        return fits ? scale*.98f : 90.f;
    }

    void project(const TrackCamera& camera,float horizontalCorrection) {
        for(std::size_t i=0;i<count;++i) {
            auto p=vertices[i];p.x*=horizontalCorrection;
            if(p.z<kTrackNearPlane) {vertices[i]={0,0,0};continue;}
            const float inverse=1/p.z;
            vertices[i]={camera.principalX+camera.focalLength*p.x*inverse,
                         camera.principalY-camera.focalLength*p.y*inverse,inverse};
        }
    }

    template<class Transform> void panel(PreparedCarPanel& result,const TrackCamera& camera,
                                        const CarPanel& face,std::size_t index,Transform transform) const {
        for(unsigned i=0;i<4;++i)if(vertices[cornerIndex[index*4+i]].z==0) {
            prepareCarPanel(result,camera,face,transform);return;
        }
        result.visibility=1;result.color=face.color;result.paint=face.paint;result.light=face.light;
        result.left=result.top=1e20f;result.right=result.bottom=-1e20f;
        new (&result.screen) decltype(result.screen);
        for(unsigned i=0;i<4;++i) {
            const auto p=vertices[cornerIndex[index*4+i]];
            result.screen[i]={p.x,p.y,p.z,((i==0 || i==3 ? face.u0 : face.u1)/255.f)*p.z,
                                         ((i<2 ? face.v0 : face.v1)/255.f)*p.z};
            result.left=std::min(result.left,p.x);result.right=std::max(result.right,p.x);
            result.top=std::min(result.top,p.y);result.bottom=std::max(result.bottom,p.y);
        }
    }
};
} // namespace lets_and_go
