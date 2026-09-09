#pragma once
#include "car_surface_raster.h"

namespace lets_and_go {

// Shared by the production preview and its full-mesh framing regression.
struct GarageCarTransform {
    float cosine,sine,pitchCosine,pitchSine,wheelCosine,wheelSine,widthScale;
    GarageCarTransform(const CarSpec& spec,float yaw,float pitch,float wheelPhase)
        :cosine(std::cos(yaw)),sine(std::sin(yaw)),pitchCosine(std::cos(pitch)),
         pitchSine(std::sin(pitch)),wheelCosine(std::cos(wheelPhase)),wheelSine(std::sin(wheelPhase)),
         widthScale((float(spec.dimensions.width)/spec.dimensions.length)/(97.f/155.f)) {}
    TrackCameraPoint operator()(CarPoint p,uint8_t wheel) const {
        p=animateCarPanelPoint(p,wheel,wheelCosine,wheelSine);
        p.x*=widthScale;
        const float x=p.x*cosine+p.z*sine,z=p.z*cosine-p.x*sine;
        return {x,p.y*pitchCosine-z*pitchSine,5.8f-(z*pitchCosine+p.y*pitchSine)};
    }
};

// Largest native-pixel view fitting the existing raster, with room for UI.
// Projection scales linearly with focal length; fit all authored vertices,
// including wheels, rather than guessing a car-specific bounding box.
inline float inspectionScale(const CarSpec& spec,const CarDisplayMesh& mesh,
                             float yaw,float pitch) {
    TrackCamera camera{};camera.principalX=0;camera.principalY=0;camera.focalLength=5.8f;
    const GarageCarTransform transform(spec,yaw,pitch,0);
    float scale=150.f;
    for(std::size_t i=0;i<mesh.count;++i)for(auto p:mesh.panels[i].point) {
        TrackScreenPoint point{};
        if(!projectTrackPoint(camera,transform(p,mesh.panels[i].wheel),point))return 90.f;
        if(std::abs(point.x)>.0001f)scale=std::min(scale,165.f/std::abs(point.x));
        if(point.y<-.0001f)scale=std::min(scale,-135.f/point.y);
        if(point.y>.0001f)scale=std::min(scale,115.f/point.y);
    }
    return scale*.98f;
}

} // namespace lets_and_go
