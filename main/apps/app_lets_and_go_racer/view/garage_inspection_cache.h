#pragma once
#include "garage_car_transform.h"
#include "../../common/soft3d/frontend/shared_vertex_index.h"

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
        count=soft3d::indexSharedVertices<16384>(mesh.count*4,[&](std::size_t corner) {
            const auto& face=mesh.panels[corner/4];const auto p=face.point[corner%4];
            return soft3d::VertexKey{{p.x,p.y,p.z},face.rigidPart};
        },cornerIndex.data(),vertexCorner.data()).count;
        indexed=true;
    }

    void prepare(const CarSpec& spec,const CarDisplayMesh& mesh,float yaw,float pitch,float wheelPhase) {
        if(!indexed)index(mesh);
        const GarageCarTransform transform(spec,yaw,pitch,wheelPhase);
        for(std::size_t i=0;i<count;++i) {
            const auto corner=vertexCorner[i];const auto& face=mesh.panels[corner/4];
            vertices[i]=transform(face.point[corner%4],face.wheel);
        }
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
