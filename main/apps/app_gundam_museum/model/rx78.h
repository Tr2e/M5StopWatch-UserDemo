#pragma once
#include "../../app_lets_and_go_racer/model/car_display_mesh.h"
#include <array>
#include <cstdint>

namespace gundam_museum {
using Point=lets_and_go::CarPoint;
enum class Part : uint8_t { Feet,Shins,Knees,Thighs,Waist,Torso,Head,Shoulders,Arms,Hands,Backpack,Sabers,Rifle,Shield,Bazooka,Funnels,Aile,Count };
// Legacy pose IDs are retained for RX-78 asset regression fixtures.
enum class Pose : uint8_t { Display, Salute, Saber };
struct Mesh {
    static constexpr std::size_t capacity=4096;
    std::array<lets_and_go::CarPanel,capacity> panels{};
    std::array<Point,capacity> normals{};
    std::array<Point,capacity> anchors{};
    std::array<float,capacity> planeOffsets{};
    std::array<Part,capacity> parts{};
    std::array<bool,capacity> twoSided{};
    std::size_t count=0,buriedOmitted=0;
    bool overflowed=false;
};
struct BuildOptions { bool equipment=true; bool keepBuriedFaces=false; bool gray=false; Pose pose=Pose::Display; };
// User-baseline SD RX-78-2, SDCS structure references and user-specified pose.
// Authored proportions, not measured CAD.
// +Y up, +Z front, +X the model's left. Nominal sole Y=.025 before articulation.
struct Rx78Assembly;
void buildRx78(Mesh& mesh,BuildOptions options={},Rx78Assembly* assembly=nullptr);
inline Point subtract(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point cross(Point a,Point b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float dot(Point a,Point b){return a.x*b.x+a.y*b.y+a.z*b.z;}
} // namespace gundam_museum
