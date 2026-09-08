#include "../main/apps/app_lets_and_go_racer/model/car_catalog.h"

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
        std::array<WireLine, 64u> compact{};
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
        valid &= check(race.lineCount >= 35u && race.lineCount <= 60u,
                       "race LOD left the line budget");
        valid &= check(showcase.lineCount >= 80u && showcase.lineCount <= 140u,
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
    valid &= check(showcaseCounts[1] > showcaseCounts[0],
                   "Hurricane Sonic lost its three-plane rear wing signature");
    valid &= check(carSpec(CarId::BrockenGigant).profile[5].halfWidth >
                       carSpec(CarId::CycloneMagnum).profile[5].halfWidth,
                   "Brocken Gigant lost its broad front-motor silhouette");
    valid &= check(carSpec(CarId::NeoTridaggerZmc).dimensions.height >
                       carSpec(CarId::HurricaneSonic).dimensions.height,
                   "Neo Tridagger ZMC height signature changed");
    return valid;
}
}  // namespace

int main()
{
    return validateCatalog() && validateWireframes() ? 0 : 1;
}
