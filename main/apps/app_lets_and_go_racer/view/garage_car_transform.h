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

} // namespace lets_and_go
