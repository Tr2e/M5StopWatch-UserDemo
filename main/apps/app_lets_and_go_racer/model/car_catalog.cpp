#include "car_catalog.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {

constexpr uint16_t kWhite = 0xef7du;
constexpr uint16_t kBlue = 0x3275u;
constexpr uint16_t kRed = 0xc9a7u;
constexpr uint16_t kGreen = 0x36a8u;
constexpr uint16_t kBlack = 0x2145u;
constexpr uint16_t kYellow = 0xe5cau;

// Catalogue envelope for legacy wire inspection; solid bodies are independently
// authored in car_display_mesh.cpp. A zero kit height means not published.
constexpr std::array<CarProfileStation,7> kAddedProfile{{
    {-1,.48f,.09f,.26f,.30f},{-.72f,.56f,.10f,.35f,.40f},
    {-.38f,.48f,.12f,.32f,.45f},{0,.25f,.12f,.31f,.43f},
    {.34f,.44f,.10f,.28f,.32f},{.70f,.52f,.09f,.24f,.27f},
    {1,.30f,.08f,.13f,.16f}}};

constexpr std::array<CarSpec, kCarCount> kCars = {{
    {
        CarId::CycloneMagnum, "Cyclone Magnum", "MAGNUM",
        {155, 97, 40}, {0.96f, 0.94f, 0.76f, 0.78f},
        kWhite, kBlue, kGreen,
        {{{-1.00f, 0.58f, 0.10f, 0.24f, 0.30f},
          {-0.72f, 0.62f, 0.09f, 0.28f, 0.38f},
          {-0.38f, 0.60f, 0.08f, 0.34f, 0.48f},
          { 0.00f, 0.52f, 0.08f, 0.38f, 0.56f},
          { 0.34f, 0.50f, 0.07f, 0.31f, 0.44f},
          { 0.70f, 0.38f, 0.06f, 0.22f, 0.30f},
          { 1.00f, 0.12f, 0.08f, 0.14f, 0.17f}}},
        0.58f, -0.58f, 0.22f, -0.72f, 0.62f, 0.62f, 0.18f, 1,
    },
    {
        CarId::HurricaneSonic, "Hurricane Sonic", "SONIC",
        {155, 97, 40}, {0.84f, 0.82f, 0.98f, 0.96f},
        kWhite, kRed, kYellow,
        {{{-1.00f, 0.55f, 0.09f, 0.22f, 0.28f},
          {-0.72f, 0.63f, 0.08f, 0.27f, 0.35f},
          {-0.38f, 0.61f, 0.07f, 0.31f, 0.43f},
          { 0.00f, 0.57f, 0.07f, 0.35f, 0.50f},
          { 0.34f, 0.59f, 0.06f, 0.27f, 0.38f},
          { 0.70f, 0.52f, 0.06f, 0.20f, 0.26f},
          { 1.00f, 0.20f, 0.07f, 0.13f, 0.15f}}},
        0.58f, -0.58f, 0.21f, -0.78f, 0.48f, 0.50f, 0.22f, 1,
    },
    {
        CarId::NeoTridaggerZmc, "Neo Tridagger ZMC", "TRIDAGGER",
        {132, 90, 46}, {0.88f, 0.86f, 0.90f, 0.84f},
        kBlack, kRed, kRed,
        {{{-1.00f, 0.48f, 0.09f, 0.22f, 0.28f},
          {-0.72f, 0.57f, 0.08f, 0.25f, 0.34f},
          {-0.38f, 0.54f, 0.07f, 0.30f, 0.49f},
          { 0.00f, 0.49f, 0.07f, 0.36f, 0.60f},
          { 0.34f, 0.51f, 0.06f, 0.28f, 0.42f},
          { 0.70f, 0.41f, 0.06f, 0.18f, 0.25f},
          { 1.00f, 0.08f, 0.08f, 0.12f, 0.15f}}},
        0.56f, -0.56f, 0.22f, -0.78f, 0.52f, 0.52f, 0.16f, 1,
    },
    {
        CarId::BrockenGigant, "Brocken Gigant", "BROCKEN",
        {156, 97, 38}, {0.92f, 0.76f, 0.68f, 0.99f},
        kRed, kBlue, kYellow,
        {{{-1.00f, 0.55f, 0.08f, 0.20f, 0.25f},
          {-0.72f, 0.62f, 0.07f, 0.24f, 0.30f},
          {-0.38f, 0.61f, 0.06f, 0.28f, 0.38f},
          { 0.00f, 0.58f, 0.06f, 0.32f, 0.44f},
          { 0.34f, 0.64f, 0.05f, 0.30f, 0.38f},
          { 0.70f, 0.64f, 0.05f, 0.26f, 0.31f},
          { 1.00f, 0.42f, 0.06f, 0.20f, 0.23f}}},
        0.62f, -0.56f, 0.23f, -0.72f, 0.48f, 0.58f, 0.14f, 0,
    },
    {CarId::SpinCobra,"Spin Cobra","COBRA",{150,97,38},
     {.86f,.90f,.97f,.88f},kBlue,kYellow,kYellow,kAddedProfile,
     .52f,-.55f,.175f,-.84f,.50f,.46f,.15f,1},
    {CarId::BeakSpider,"Beak Spider","SPIDER",{132,90,41},
     {.93f,.86f,.80f,.85f},kBlack,kRed,kRed,kAddedProfile,
     .52f,-.55f,.175f,-.80f,.54f,.49f,.08f,3},
    {CarId::RayStinger,"Ray Stinger","STINGER",{150,97,0},
     {.95f,.92f,.74f,.82f},0xbdf7,kRed,kWhite,kAddedProfile,
     .52f,-.55f,.175f,-.66f,.55f,.04f,.22f,0},
    {CarId::Diospada,"Diospada","DIOSPADA",{155,97,0},
     {.89f,.86f,.92f,.94f},kRed,kWhite,kWhite,kAddedProfile,
     .52f,-.55f,.175f,-.84f,.57f,.50f,.22f,1},
}};

struct CarMeshWriter {
    WireLine* lines;
    std::size_t capacity;
    std::size_t lineCount = 0;
    bool overflowed = false;
};

void addLine(CarMeshWriter& mesh, CarPoint from, CarPoint to, WireStroke stroke)
{
    if (mesh.lineCount >= mesh.capacity) {
        mesh.overflowed = true;
        return;
    }
    mesh.lines[mesh.lineCount++] = {from, to, stroke};
}

void addRing(CarMeshWriter& mesh, float x, float centerY, float centerZ,
             float radius, int segments, WireStroke stroke, bool horizontal)
{
    constexpr float kTau = 6.28318530718f;
    for (int index = 0; index < segments; ++index) {
        const float a = kTau * static_cast<float>(index) / static_cast<float>(segments);
        const float b = kTau * static_cast<float>(index + 1) / static_cast<float>(segments);
        if (horizontal) {
            addLine(mesh, {x + std::cos(a) * radius, centerY, centerZ + std::sin(a) * radius},
                    {x + std::cos(b) * radius, centerY, centerZ + std::sin(b) * radius}, stroke);
        } else {
            addLine(mesh, {x, centerY + std::sin(a) * radius, centerZ + std::cos(a) * radius},
                    {x, centerY + std::sin(b) * radius, centerZ + std::cos(b) * radius}, stroke);
        }
    }
}

void quadWire(CarMeshWriter& mesh, CarPoint a, CarPoint b, CarPoint c, CarPoint d,
              WireStroke stroke)
{
    addLine(mesh,a,b,stroke); addLine(mesh,b,c,stroke);
    addLine(mesh,c,d,stroke); addLine(mesh,d,a,stroke);
}

void bodyWires(CarMeshWriter& mesh, CarId id, bool detailed)
{
    const bool brocken=id==CarId::BrockenGigant, neo=id==CarId::NeoTridaggerZmc;
    const float width=brocken ? .28f : neo ? .23f : .19f;
    const float nose=brocken ? .12f : neo ? .11f : .045f;
    // Separate central shell, windscreen and four cowls; no boat-shaped envelope.
    quadWire(mesh,{-width,.30f,-.48f},{width,.30f,-.48f},
        {nose,.12f,.86f},{-nose,.12f,.86f},WireStroke::Body);
    quadWire(mesh,{-width,.12f,-.48f},{width,.12f,-.48f},
        {nose,.09f,.86f},{-nose,.09f,.86f},WireStroke::Body);
    for(float side : {-1.f,1.f}) {
        addLine(mesh,{side*width,.12f,-.48f},{side*width,.30f,-.48f},WireStroke::Body);
        addLine(mesh,{side*nose,.09f,.86f},{side*nose,.12f,.86f},WireStroke::Body);
        quadWire(mesh,{side*.29f,.34f,-.76f},{side*.53f,.34f,-.76f},
            {side*.51f,.28f,-.27f},{side*.29f,.28f,-.27f},WireStroke::Body);
        quadWire(mesh,{side*.34f,.34f,.47f},{side*.54f,.34f,.47f},
            {side*.52f,.15f,.81f},{side*.34f,.15f,.81f},WireStroke::Accent);
        addLine(mesh,{side*.54f,.34f,.47f},{side*.55f,.12f,.54f},WireStroke::Body);
        if(brocken) {
            addLine(mesh,{side*.585f,.165f,-.57f},{side*.585f,.165f,.58f},WireStroke::Mechanical);
            addLine(mesh,{side*.15f,.29f,-.63f},{side*.15f,.49f,-.63f},WireStroke::Mechanical);
        }
        if(neo) {
            addLine(mesh,{side*.31f,.35f,.49f},{side*.37f,.18f,.80f},WireStroke::Accent);
            addLine(mesh,{side*.37f,.18f,.80f},{side*.44f,.35f,.49f},WireStroke::Accent);
        }
    }
    const float back=brocken ? -.08f : -.23f, roof=neo ? .48f : .45f;
    quadWire(mesh,{-.15f,roof,back},{.15f,roof,back},
        {.10f,brocken ? .326f : .30f,.19f},{-.10f,brocken ? .326f : .30f,.19f},WireStroke::Glass);
    addLine(mesh,{-.15f,roof,back},{-.135f,.33f,-.43f},WireStroke::Body);
    addLine(mesh,{.15f,roof,back},{.135f,.33f,-.43f},WireStroke::Body);
    for(float z : {-.85f,.88f})
        quadWire(mesh,{-.55f,.08f,z-.035f},{.55f,.08f,z-.035f},
            {.55f,.08f,z+.035f},{-.55f,.08f,z+.035f},WireStroke::Mechanical);
    if(!brocken) {
        const float h=neo ? .59f : .48f;
        quadWire(mesh,{-.50f,h,-.98f},{.50f,h,-.98f},
            {.50f,h,-.76f},{-.50f,h,-.76f},WireStroke::Accent);
        for(float side : {-1.f,1.f}) {
            addLine(mesh,{side*.25f,.30f,-.83f},{side*.25f,h,-.83f},WireStroke::Body);
            addLine(mesh,{side*.50f,h,-.98f},{side*.50f,h+.10f,-.77f},WireStroke::Body);
        }
    } else {
        for(int rib=0;rib<4;++rib) {
            const float x=-.15f+rib*.10f;
            addLine(mesh,{x,.34f,.23f},{x,.34f,.46f},WireStroke::Body);
        }
    }
    if(id==CarId::HurricaneSonic) {
        quadWire(mesh,{-.32f,.29f,.52f},{.32f,.29f,.52f},
            {.22f,.18f,.78f},{-.22f,.18f,.78f},WireStroke::Body);
        if(detailed) for(float side : {-1.f,1.f}) for(int rib=0;rib<3;++rib)
            addLine(mesh,{side*.29f,.405f,-.68f+rib*.065f},
                {side*.51f,.405f,-.68f+rib*.065f},WireStroke::Body);
    }
}

}  // namespace

const std::array<CarSpec, kCarCount>& carCatalog()
{
    return kCars;
}

const CarSpec& carSpec(CarId id)
{
    return kCars[isValidCar(id) ? static_cast<std::size_t>(id) : 0u];
}

CarMeshBuildResult buildCarWireframeInto(CarId id, CarLod lod,
                                         WireLine* output, std::size_t capacity)
{
    id = carSpec(id).id;
    CarMeshWriter mesh{output, output ? capacity : 0u};
    const bool detailed=lod==CarLod::Showcase;
    bodyWires(mesh,id,detailed);
    for(float axle : {kModelFrontAxle,kModelRearAxle}) for(float side : {-1.f,1.f}) {
        addRing(mesh,side*.553f,kModelWheelRadius,axle,kModelWheelRadius,
                detailed ? 12 : 8,WireStroke::Mechanical,false);
        if(detailed) addRing(mesh,side*.395f,kModelWheelRadius,axle,kModelWheelRadius,
                             6,WireStroke::Body,false);
        if(id!=CarId::NeoTridaggerZmc || axle==kModelRearAxle) {
            addLine(mesh,{side*.555f,kModelWheelRadius,axle},
                {side*.555f,kModelWheelRadius+.12f,axle},WireStroke::WheelSpoke);
            addLine(mesh,{side*.555f,kModelWheelRadius,axle},
                {side*.555f,kModelWheelRadius,axle+.12f},WireStroke::WheelSpoke);
        }
    }
    for(float side : {-1.f,1.f}) for(float z : {-.84f,.90f})
        addRing(mesh,side*.55f,.13f,z,.073f,4,WireStroke::Mechanical,true);
    return {mesh.lineCount, mesh.overflowed};
}

CarWireframe buildCarWireframe(CarId id, CarLod lod)
{
    CarWireframe mesh;
    const auto result = buildCarWireframeInto(id, lod, mesh.lines.data(), mesh.lines.size());
    mesh.lineCount = result.lineCount;
    mesh.overflowed = result.overflowed;
    return mesh;
}

}  // namespace lets_and_go
