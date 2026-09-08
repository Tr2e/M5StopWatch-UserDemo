#include "../main/apps/app_lets_and_go_racer/model/car_catalog.h"
#include "../main/apps/app_lets_and_go_racer/model/car_display_mesh.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace {
using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool finitePoint(const CarPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::isfinite(point.z);
}

bool validateCatalog()
{
    const auto& cars = carCatalog();
    bool valid = check(cars.size() == 4u, "official launch roster changed");
    uint8_t ids = 0;
    for (const auto& car : cars) {
        valid &= check(isValidCar(car.id), "catalog contains invalid car id");
        valid &= check((ids & carMask(car.id)) == 0u, "catalog car id repeated");
        ids |= carMask(car.id);
        valid &= check(car.officialName[0] != '\0' && car.shortName[0] != '\0',
                       "car name is empty");
        valid &= check(car.dimensions.length >= 130 && car.dimensions.length <= 160 &&
                           car.dimensions.width >= 88 && car.dimensions.width <= 100 &&
                           car.dimensions.height >= 35 && car.dimensions.height <= 48,
                       "official dimensions left reviewed Mini 4WD range");
        for (const auto& station : car.profile) {
            valid &= check(station.halfWidth > 0.0f && station.halfWidth <= 0.66f &&
                               station.sillY >= 0.0f && station.centerY >= station.deckY,
                           "invalid profile station");
        }
    }
    valid &= check(std::strcmp(cars[0].officialName, "Cyclone Magnum") == 0 &&
                       std::strcmp(cars[1].officialName, "Hurricane Sonic") == 0 &&
                       std::strcmp(cars[2].officialName, "Neo Tridagger ZMC") == 0 &&
                       std::strcmp(cars[3].officialName, "Brocken Gigant") == 0,
                   "official roster names changed");
    valid &= check(cars[0].dimensions.length == 155 && cars[0].dimensions.width == 97 &&
                       cars[0].dimensions.height == 40 &&
                       cars[1].dimensions.length == 155 && cars[1].dimensions.width == 97 &&
                       cars[1].dimensions.height == 40 &&
                       cars[2].dimensions.length == 132 && cars[2].dimensions.width == 90 &&
                       cars[2].dimensions.height == 46 &&
                       cars[3].dimensions.length == 156 && cars[3].dimensions.width == 97 &&
                       cars[3].dimensions.height == 38,
                   "reviewed official dimensions changed");
    return valid;
}

bool validateWireframes()
{
    bool valid = true;
    std::array<std::size_t, kCarCount> showcaseCounts{};
    for (std::size_t index = 0; index < kCarCount; ++index) {
        const CarId id = static_cast<CarId>(index);
        const CarWireframe race = buildCarWireframe(id, CarLod::Race);
        const CarWireframe showcase = buildCarWireframe(id, CarLod::Showcase);
        std::cout << carSpec(id).shortName << " wire lines: race=" << race.lineCount
                  << " reference=" << showcase.lineCount << '\n';
        std::array<WireLine, 128u> compact{};
        const auto built = buildCarWireframeInto(id, CarLod::Race, compact.data(), compact.size());
        valid &= check(!built.overflowed && built.lineCount == race.lineCount,
                       "in-place race mesh differs from reference builder");
        for (std::size_t line = 0; line < built.lineCount; ++line) {
            const auto& a = compact[line];
            const auto& b = race.lines[line];
            valid &= check(a.from.x == b.from.x && a.from.y == b.from.y && a.from.z == b.from.z &&
                               a.to.x == b.to.x && a.to.y == b.to.y && a.to.z == b.to.z && a.stroke == b.stroke,
                           "in-place mesh geometry changed");
        }
        const auto shortBuffer = buildCarWireframeInto(id, CarLod::Race, compact.data(), 2u);
        valid &= check(shortBuffer.overflowed && shortBuffer.lineCount == 2u &&
                           buildCarWireframeInto(id, CarLod::Showcase, nullptr, 0u).overflowed,
                       "mesh capacity limit was not enforced");
        showcaseCounts[index] = showcase.lineCount;
        valid &= check(race.lineCount >= 100u && race.lineCount <= 128u,
                       "race LOD left the line budget");
        valid &= check(showcase.lineCount >= 140u && showcase.lineCount <= 192u,
                       "showcase LOD left the line budget");
        valid &= check(showcase.lineCount > race.lineCount,
                       "showcase LOD is not richer than race LOD");
        valid &= check(!race.overflowed && !showcase.overflowed,
                       "wireframe exceeded fixed storage");
        for (std::size_t line = 0; line < showcase.lineCount; ++line) {
            valid &= check(finitePoint(showcase.lines[line].from) &&
                               finitePoint(showcase.lines[line].to),
                           "wireframe contains non-finite point");
            const auto inBounds = [](const CarPoint& point) {
                return std::abs(point.x) <= 0.85f && point.y >= 0.0f &&
                       point.y <= 0.80f && std::abs(point.z) <= 1.10f;
            };
            valid &= check(inBounds(showcase.lines[line].from) &&
                               inBounds(showcase.lines[line].to),
                           "wireframe escaped normalized car bounds");
        }
    }
    valid &= check(carSpec(CarId::HurricaneSonic).wingPlanes == 1 &&
                       carSpec(CarId::BrockenGigant).wingPlanes == 0,
                   "official wing configuration regressed");
    valid &= check(carSpec(CarId::BrockenGigant).profile[5].halfWidth >
                       carSpec(CarId::CycloneMagnum).profile[5].halfWidth,
                   "Brocken Gigant lost its broad front-motor silhouette");
    valid &= check(carSpec(CarId::NeoTridaggerZmc).dimensions.height >
                       carSpec(CarId::HurricaneSonic).dimensions.height,
                   "Neo Tridagger ZMC height signature changed");
    return valid;
}

bool validateDisplayMeshes()
{
    bool valid=true;
    for (const auto& spec : carCatalog()) {
        CarDisplayMesh mesh;
        buildCarDisplayMesh(spec.id,mesh);
        std::cout << spec.shortName << " display panels: " << mesh.count << '\n';
        valid &= check(!mesh.overflowed && mesh.count > 350 &&
                       mesh.count < mesh.panels.size(), "display mesh exceeded fixed budget");
        unsigned spokes=0;
        float maxY=0;
        for (std::size_t i=0;i<mesh.count;++i) {
            const auto& face=mesh.panels[i];
            valid &= check(face.wheel<=4 && face.edges<=15, "invalid panel metadata");
            valid &= check(face.u0<=face.u1 && face.v0<=face.v1 &&
                           static_cast<unsigned>(face.paint)<=static_cast<unsigned>(CarPaint::NeoHood),
                           "invalid solid surface UV/material");
            valid &= check(face.parent==0xffffu || (face.parent<i &&
                           mesh.panels[face.parent].parent==0xffffu),
                           "decal parent is invalid, cyclic or nested");
            if(face.wheel) ++spokes;
            for (const auto p : face.point) {
                maxY=std::max(maxY,p.y);
                valid &= check(finitePoint(p) && std::abs(p.x)<.7f &&
                               std::abs(p.z)<1.01f && p.y>=0 && p.y<.71f,
                               "display vertex outside normalized envelope");
                const float axle=face.wheel<=2 ? kModelFrontAxle : kModelRearAxle;
                const auto q=animateCarPanelPoint(p,face.wheel,0,1);
                if(face.wheel) {
                    const float r=std::hypot(p.y-kModelWheelRadius,p.z-axle);
                    valid &= check(std::abs(r-std::hypot(q.y-kModelWheelRadius,q.z-axle))<1e-6f &&
                                   q.x==p.x, "hub rotation changed radius or axle");
                } else {
                    valid &= check(q.x==p.x && q.y==p.y && q.z==p.z,
                                   "non-wheel body panel rotates");
                }
            }
        }
        valid &= check(spokes==(spec.id==CarId::NeoTridaggerZmc ? 10u : 20u),
                       "five-spoke wheels or Tridagger front caps regressed");
        if(spec.id==CarId::BrockenGigant)
            valid &= check(maxY<.50f && spec.bodyColor==0xc9a7 && spec.wheelColor==0xe5ca,
                           "Brocken tall wing or incorrect livery returned");
        const auto highCount=mesh.count;
        buildCarDisplayMesh(spec.id,mesh,CarSurfaceDetail::Medium);
        const auto mediumCount=mesh.count;
        std::cout << spec.shortName << " medium race panels: " << mediumCount << '\n';
        valid &= check(mediumCount<=1536,"medium race geometry exceeded storage");
        valid &= check(!mesh.overflowed, "medium surface mesh overflow");
        buildCarDisplayMesh(spec.id,mesh,CarSurfaceDetail::Low);
        std::cout << spec.shortName << " solid race panels: " << mesh.count << '\n';
        valid &= check(!mesh.overflowed && mesh.count<mediumCount && mediumCount<highCount,
                       "adaptive surface quality does not reduce geometry");
        valid &= check(mesh.count<=1024, "race solid mesh exceeds compact storage");
        for(std::size_t i=0;i<mesh.count;++i)
            valid &= check(mesh.panels[i].parent==0xffffu || mesh.panels[i].parent<i,
                           "low detail invalidated attached decal index");
    }
    CarDisplayMesh fallback,magnum;
    buildCarDisplayMesh(static_cast<CarId>(255),fallback);
    buildCarDisplayMesh(CarId::CycloneMagnum,magnum);
    valid &= check(fallback.count==magnum.count && !fallback.overflowed,
                   "invalid car fallback changed display geometry");
    for(std::size_t i=0;i<magnum.count;++i)
        valid &= check(fallback.panels[i].color==magnum.panels[i].color,
                       "invalid car fallback changed livery");
    std::array<CarPanel,3> small{};
    small[2].color=0x1234;
    const auto limited=buildCarSurfaceInto(CarId::BrockenGigant,small.data(),2,CarSurfaceDetail::High);
    valid &= check(limited.overflowed && limited.count==2 && small[2].color==0x1234 &&
                   buildCarSurfaceInto(CarId::CycloneMagnum,nullptr,0,CarSurfaceDetail::Low).overflowed,
                   "solid mesh in-place builder exceeded buffer capacity");
    return valid;
}
}  // namespace

int main()
{
    return validateCatalog() && validateWireframes() && validateDisplayMeshes() ? 0 : 1;
}
