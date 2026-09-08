#include "car_catalog.h"

#include <algorithm>
#include <cmath>

namespace lets_and_go {
namespace {

constexpr uint16_t kWhite = 0xef7du;
constexpr uint16_t kBlue = 0x32bdu;
constexpr uint16_t kRed = 0xd945u;
constexpr uint16_t kGreen = 0x566bu;
constexpr uint16_t kBlack = 0x2145u;
constexpr uint16_t kYellow = 0xeec4u;
constexpr uint16_t kGray = 0x7befu;

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
        0.58f, -0.58f, 0.21f, -0.78f, 0.60f, 0.64f, 0.14f, 3,
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
        kBlack, kRed, kGray,
        {{{-1.00f, 0.55f, 0.08f, 0.20f, 0.25f},
          {-0.72f, 0.62f, 0.07f, 0.24f, 0.30f},
          {-0.38f, 0.61f, 0.06f, 0.28f, 0.38f},
          { 0.00f, 0.58f, 0.06f, 0.32f, 0.44f},
          { 0.34f, 0.64f, 0.05f, 0.30f, 0.38f},
          { 0.70f, 0.64f, 0.05f, 0.26f, 0.31f},
          { 1.00f, 0.42f, 0.06f, 0.20f, 0.23f}}},
        0.62f, -0.56f, 0.23f, -0.72f, 0.48f, 0.58f, 0.14f, 1,
    },
}};

void addLine(CarWireframe& mesh, CarPoint from, CarPoint to, WireStroke stroke)
{
    if (mesh.lineCount >= mesh.lines.size()) {
        mesh.overflowed = true;
        return;
    }
    mesh.lines[mesh.lineCount++] = {from, to, stroke};
}

void addRing(CarWireframe& mesh, float x, float centerY, float centerZ,
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

void addStation(CarWireframe& mesh, const CarProfileStation& station, bool detailed)
{
    const CarPoint leftSill{-station.halfWidth, station.sillY, station.z};
    const CarPoint rightSill{station.halfWidth, station.sillY, station.z};
    const CarPoint leftDeck{-station.halfWidth * 0.72f, station.deckY, station.z};
    const CarPoint rightDeck{station.halfWidth * 0.72f, station.deckY, station.z};
    const CarPoint center{0.0f, station.centerY, station.z};
    addLine(mesh, leftSill, rightSill, WireStroke::Body);
    addLine(mesh, leftSill, leftDeck, WireStroke::Body);
    addLine(mesh, rightSill, rightDeck, WireStroke::Body);
    addLine(mesh, leftDeck, rightDeck, WireStroke::Accent);
    if (detailed) {
        addLine(mesh, leftDeck, center, WireStroke::Accent);
        addLine(mesh, center, rightDeck, WireStroke::Accent);
    }
}

void connectStations(CarWireframe& mesh, const CarProfileStation& from,
                     const CarProfileStation& to)
{
    addLine(mesh, {-from.halfWidth, from.sillY, from.z},
            {-to.halfWidth, to.sillY, to.z}, WireStroke::Body);
    addLine(mesh, {from.halfWidth, from.sillY, from.z},
            {to.halfWidth, to.sillY, to.z}, WireStroke::Body);
    addLine(mesh, {-from.halfWidth * 0.72f, from.deckY, from.z},
            {-to.halfWidth * 0.72f, to.deckY, to.z}, WireStroke::Body);
    addLine(mesh, {from.halfWidth * 0.72f, from.deckY, from.z},
            {to.halfWidth * 0.72f, to.deckY, to.z}, WireStroke::Body);
    addLine(mesh, {0.0f, from.centerY, from.z},
            {0.0f, to.centerY, to.z}, WireStroke::Accent);
}

void addWing(CarWireframe& mesh, const CarSpec& spec, CarLod lod)
{
    const float rearZ = spec.wingZ - spec.wingChord * 0.5f;
    const float frontZ = spec.wingZ + spec.wingChord * 0.5f;
    for (uint8_t plane = 0; plane < spec.wingPlanes; ++plane) {
        const float y = spec.wingY + static_cast<float>(plane) * 0.055f;
        addLine(mesh, {-spec.wingHalfWidth, y, rearZ},
                {spec.wingHalfWidth, y, rearZ}, WireStroke::Accent);
        addLine(mesh, {-spec.wingHalfWidth, y, frontZ},
                {spec.wingHalfWidth, y, frontZ}, WireStroke::Accent);
        addLine(mesh, {-spec.wingHalfWidth, y, rearZ},
                {-spec.wingHalfWidth, y, frontZ}, WireStroke::Accent);
        if (lod == CarLod::Showcase) {
            addLine(mesh, {spec.wingHalfWidth, y, rearZ},
                    {spec.wingHalfWidth, y, frontZ}, WireStroke::Accent);
            addLine(mesh, {-spec.wingHalfWidth * 0.58f, y, frontZ},
                    {-spec.wingHalfWidth * 0.48f, 0.28f, frontZ}, WireStroke::Mechanical);
            addLine(mesh, {spec.wingHalfWidth * 0.58f, y, frontZ},
                    {spec.wingHalfWidth * 0.48f, 0.28f, frontZ}, WireStroke::Mechanical);
        }
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

CarWireframe buildCarWireframe(CarId id, CarLod lod)
{
    const CarSpec& spec = carSpec(id);
    CarWireframe mesh;
    const bool showcase = lod == CarLod::Showcase;
    constexpr std::array<std::size_t, 4> kRaceStations = {{0u, 2u, 4u, 6u}};

    if (showcase) {
        for (const auto& station : spec.profile) addStation(mesh, station, true);
        for (std::size_t index = 1; index < spec.profile.size(); ++index) {
            connectStations(mesh, spec.profile[index - 1], spec.profile[index]);
        }
    } else {
        for (const std::size_t index : kRaceStations) {
            addStation(mesh, spec.profile[index], false);
        }
        for (std::size_t index = 1; index < kRaceStations.size(); ++index) {
            connectStations(mesh, spec.profile[kRaceStations[index - 1]],
                            spec.profile[kRaceStations[index]]);
        }
    }

    const int wheelSegments = showcase ? 8 : 4;
    const float wheelX = 0.59f;
    for (const float axleZ : {spec.frontAxleZ, spec.rearAxleZ}) {
        addRing(mesh, -wheelX, spec.wheelRadius, axleZ, spec.wheelRadius,
                wheelSegments, WireStroke::Mechanical, false);
        addRing(mesh, wheelX, spec.wheelRadius, axleZ, spec.wheelRadius,
                wheelSegments, WireStroke::Mechanical, false);
    }
    if (showcase) {
        for (const float rollerZ : {-0.92f, 0.90f}) {
            addRing(mesh, -0.70f, 0.10f, rollerZ, 0.08f, 4,
                    WireStroke::Mechanical, true);
            addRing(mesh, 0.70f, 0.10f, rollerZ, 0.08f, 4,
                    WireStroke::Mechanical, true);
        }
    }
    addWing(mesh, spec, lod);
    return mesh;
}

}  // namespace lets_and_go
