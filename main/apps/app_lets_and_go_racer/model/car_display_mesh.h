#pragma once
#include "car_catalog.h"

namespace lets_and_go {

enum class CarSurfaceDetail : uint8_t { Low, Medium, High };
enum class CarPart : uint8_t {
    Unspecified, Chassis, Wheel, Roller, Nose, Canopy, RearCowl, FrontCowl,
    SideWeb, Intake, RearWing, FrontBridge, SideGuard, MotorBlock, TailFin
};
enum class CarPaint : uint8_t {
    Solid, MagnumHood, MagnumCowl, MagnumWing, SonicHood, SonicCowl, SonicWing,
    Flame, TridaggerWing, Tiger, BrockenHood, Glass, BronzeGlass, BlueGlass, Eye,
    BrockenShell, FrontWing, NeoHood, MagnumCanopy, MagnumCowlSide,
    MagnumNoseSide, MagnumVent, SonicCanopy, SonicSide, SonicFront,
    NeoCanopy, NeoWingLeft, BrockenCabin, BrockenLamp, BrockenArmor, BrockenCabinSide,
    CobraFlame, CobraHood, CobraLamp, SpiderWeb, SpiderCowl, SpiderWing,
    StingerHood, StingerCowl, StingerLamp, DiospadaHood, DiospadaWing, DiospadaSide,
    CobraBridge, DiospadaLouver, SpiderHood, SpiderCanopy,
    Count
};

// Authored curved-body panels shared by the garage and race solid renderers.
// Livery is sampled in panel UV space; product photos are not embedded assets.
struct CarPanel {
    std::array<CarPoint, 4> point{};
    uint16_t color = 0;
    CarPart part = CarPart::Unspecified; // Structural QA tag; no additional storage.
    uint8_t wheel = 0; // 1..4: only hub spokes rotate; tire envelopes stay round.
    uint16_t parent = 0xffffu; // Reserved legacy attachment metadata (not depth order).
    CarPaint paint = CarPaint::Solid;
    uint8_t u0=0,u1=255,v0=0,v1=255;
    uint8_t light=255;
};

struct CarDisplayMesh {
    static constexpr std::size_t kMaximumPanels = 2048u;
    std::array<CarPanel, kMaximumPanels> panels{};
    std::size_t count = 0;
    bool overflowed = false;
};

void buildCarDisplayMesh(CarId car, CarDisplayMesh& mesh,
                        CarSurfaceDetail detail = CarSurfaceDetail::High);
struct CarSurfaceBuildResult {std::size_t count;bool overflowed;};
CarSurfaceBuildResult buildCarSurfaceInto(CarId car,CarPanel* panels,std::size_t capacity,
                                        CarSurfaceDetail detail);
CarPoint animateCarPanelPoint(CarPoint point, uint8_t wheel, float cosine, float sine);

// Authored x points right in the garage's front view: the vehicle's LEFT.
// Track/look-at cameras use vehicle-right/up/forward. Reflect only the model
// coordinate, never the car's steering, lateral offset or world position.
inline CarPoint carPointInTrackBasis(CarPoint point) {return {-point.x,point.y,point.z};}

} // namespace lets_and_go
