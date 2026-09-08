#include "../main/apps/app_lets_and_go_racer/model/car_catalog.h"
#include "../main/apps/app_lets_and_go_racer/model/car_display_mesh.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <algorithm>

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
    bool valid = check(cars.size() == 8u, "eight-car roster changed");
    uint8_t ids = 0;
    for (const auto& car : cars) {
        valid &= check(isValidCar(car.id), "catalog contains invalid car id");
        valid &= check((ids & carMask(car.id)) == 0u, "catalog car id repeated");
        ids |= carMask(car.id);
        valid &= check(car.officialName[0] != '\0' && car.shortName[0] != '\0',
                       "car name is empty");
        valid &= check(car.dimensions.length >= 130 && car.dimensions.length <= 160 &&
                           car.dimensions.width >= 88 && car.dimensions.width <= 100 &&
                           ((car.id==CarId::RayStinger || car.id==CarId::Diospada) ? car.dimensions.height==0 :
                            car.dimensions.height >= 35 && car.dimensions.height <= 48),
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
            valid &= check(face.wheel<=4 && face.part<=CarPart::TailFin, "invalid panel metadata");
            valid &= check(face.u0<=face.u1 && face.v0<=face.v1 &&
                           face.paint<CarPaint::Count,
                           "invalid solid surface UV/material");
            valid &= check(face.parent==0xffffu || (face.parent<i &&
                           mesh.panels[face.parent].parent==0xffffu),
                           "decal parent is invalid, cyclic or nested");
            if(face.wheel) ++spokes;
            for (const auto p : face.point) {
                maxY=std::max(maxY,p.y);
                valid &= check(finitePoint(p) && std::abs(p.x)<.64f &&
                               std::abs(p.z)<1.f && p.y>=0 && p.y<.64f,
                               "display vertex escaped race renderer tile bounds");
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
        valid &= check(spokes==(spec.id==CarId::NeoTridaggerZmc || spec.id==CarId::BeakSpider ? 0u :
                               spec.id==CarId::BrockenGigant ? 48u : spec.id==CarId::SpinCobra ? 12u : 20u),
                       "reviewed spoke count or Tridagger cap/dish wheels regressed");
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

// Intersect a vertical probe with the same two triangles used by production.
float surfaceHeight(const CarDisplayMesh& mesh,float x,float z,CarPart part,bool all=false)
{
    float height=-1;
    for(std::size_t i=0;i<mesh.count;++i) {
        const auto& p=mesh.panels[i];
        if(!all && p.part!=part)continue;
        for(int triangle=0;triangle<2;++triangle) {
            const auto a=p.point[0],b=p.point[triangle+1],c=p.point[triangle+2];
            const float det=(b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);
            if(std::abs(det)<1e-8f)continue;
            const float u=((x-a.x)*(c.z-a.z)-(z-a.z)*(c.x-a.x))/det;
            const float v=((b.x-a.x)*(z-a.z)-(b.z-a.z)*(x-a.x))/det;
            if(u>=-1e-5f && v>=-1e-5f && u+v<=1.00001f)
                height=std::max(height,a.y+u*(b.y-a.y)+v*(c.y-a.y));
        }
    }
    return height;
}

bool validateMagnumStructure()
{
    bool valid=true;
    const auto trackPoint=carPointInTrackBasis({.3f,.4f,.5f});
    valid &= check(trackPoint.x==-.3f && trackPoint.y==.4f && trackPoint.z==.5f,
                   "garage-front and track-right handedness no longer agree");
    for(auto detail : {CarSurfaceDetail::Low,CarSurfaceDetail::Medium,CarSurfaceDetail::High}) {
        CarDisplayMesh mesh;
        buildCarDisplayMesh(CarId::CycloneMagnum,mesh,detail);
        std::array<unsigned,11> parts{};
        unsigned penetrations=0;
        float worst=0;
        CarPoint worstPoint{};
        CarPart worstPart=CarPart::Unspecified;
        for(std::size_t i=0;i<mesh.count;++i) {
            const auto& p=mesh.panels[i];
            ++parts[static_cast<unsigned>(p.part)];
            if((p.part==CarPart::RearCowl || p.part==CarPart::FrontCowl) &&
                p.paint!=CarPaint::Solid && p.point[0].x>0) {
                bool mirrored=false;
                for(std::size_t j=0;j<mesh.count && !mirrored;++j) {
                    const auto& q=mesh.panels[j];
                    if(p.part!=q.part || p.paint!=q.paint || p.u0!=q.u0 || p.u1!=q.u1 ||
                       p.v0!=q.v0 || p.v1!=q.v1 || p.light!=q.light)continue;
                    mirrored=true;
                    for(unsigned v=0;v<4;++v)
                        mirrored &= std::abs(p.point[v].x+q.point[v].x)<1e-6f &&
                                    p.point[v].y==q.point[v].y && p.point[v].z==q.point[v].z;
                }
                valid &= check(mirrored,"Magnum cowl paint/geometry no longer mirrors inner to outer");
            }
            if(p.part!=CarPart::RearCowl && p.part!=CarPart::FrontCowl && p.part!=CarPart::Nose)continue;
            // Probe triangle interiors as well as vertices: a flat panel can cut
            // through a round tire even when its corner points are outside it.
            for(int tri=0;tri<2;++tri) for(int a=0;a<=10;++a) for(int b=0;b<=10-a;++b) {
                const auto p0=p.point[0],p1=p.point[tri+1],p2=p.point[tri+2];
                const float u=a/10.f,v=b/10.f;
                const CarPoint q{p0.x+u*(p1.x-p0.x)+v*(p2.x-p0.x),
                    p0.y+u*(p1.y-p0.y)+v*(p2.y-p0.y),p0.z+u*(p1.z-p0.z)+v*(p2.z-p0.z)};
                const float x=std::abs(q.x);
                if(x<.365f || x>.56f)continue;
                const float radius=x<.39f ? .145f+(x-.365f)*1.2f :
                                   x>.535f ? .175f-(x-.535f) : .175f;
                for(float axle : {kModelFrontAxle,kModelRearAxle}) {
                    const float overlap=radius-std::hypot(q.y-kModelWheelRadius,q.z-axle);
                    if(overlap>worst) {worst=overlap;worstPoint=q;worstPart=p.part;}
                    if(overlap>.003f)++penetrations;
                }
            }
        }
        if(penetrations)std::cerr<<"Magnum shell/tire penetrations="<<penetrations<<" worst="<<worst
            <<" part="<<int(worstPart)<<" at "<<worstPoint.x<<','<<worstPoint.y<<','<<worstPoint.z<<'\n';
        valid &= check(!penetrations,"Magnum cowls intersect tire envelope");
        for(unsigned p=1;p<parts.size();++p)
            valid &= check(parts[p]>0,"Magnum lost a distinct structural component");
        for(float side : {-1.f,1.f}) {
            valid &= check(surfaceHeight(mesh,side*.29f,.25f,CarPart::Nose)>.20f &&
                           surfaceHeight(mesh,side*.20f,-.30f,CarPart::Nose)<0,
                           "Magnum broad shoulder/narrow waist contrast lost");
            for(float z : {-.55f,-.43f,-.18f})
                valid &= check(surfaceHeight(mesh,side*.21f,z,CarPart::Unspecified,true)<.20f,
                               "Magnum cockpit-to-rear-cowl air channel filled in");
        }
        valid &= check(surfaceHeight(mesh,0,-.39f,CarPart::Canopy)>.44f &&
                       surfaceHeight(mesh,0,.05f,CarPart::Canopy)<.30f,
                       "Magnum flat sloping canopy returned to bubble silhouette");
        // Surface height symmetry complements the exact mirrored UV checks above.
        for(float z : {-.67f,-.48f,-.29f,.64f,.735f})
            valid &= check(std::abs(surfaceHeight(mesh,.4f,z,CarPart::Unspecified,true)-
                                   surfaceHeight(mesh,-.4f,z,CarPart::Unspecified,true))<1e-5f,
                           "Magnum left/right structural symmetry changed");
    }
    std::cout<<"Magnum structure: shoulder/waist, open channels, canopy and tire clearance across 3 LODs\n";
    return valid;
}

bool validateRebuiltStructure(CarId car)
{
    bool valid=true;
    for(auto detail : {CarSurfaceDetail::Low,CarSurfaceDetail::Medium,CarSurfaceDetail::High}) {
        CarDisplayMesh mesh;
        buildCarDisplayMesh(car,mesh,detail);
        std::array<unsigned,static_cast<unsigned>(CarPart::TailFin)+1> parts{};
        float worst=0;
        CarPoint worstPoint{};
        CarPart worstPart=CarPart::Unspecified;
        unsigned penetrations=0;
        for(std::size_t i=0;i<mesh.count;++i) {
            const auto& p=mesh.panels[i];
            ++parts[static_cast<unsigned>(p.part)];
            if(p.part==CarPart::Wheel || p.part==CarPart::Roller || p.part==CarPart::Chassis ||
               p.part==CarPart::Unspecified)continue;
            if(p.paint!=CarPaint::Solid && (p.part==CarPart::FrontCowl || p.part==CarPart::RearCowl) && p.point[0].x>0) {
                bool mirrored=false;
                for(std::size_t j=0;j<mesh.count && !mirrored;++j) {
                    const auto& q=mesh.panels[j];
                    if(p.part!=q.part || p.paint!=q.paint || p.u0!=q.u0 || p.u1!=q.u1 ||
                       p.v0!=q.v0 || p.v1!=q.v1 || p.light!=q.light)continue;
                    mirrored=true;
                    for(unsigned v=0;v<4;++v)
                        mirrored &= std::abs(p.point[v].x+q.point[v].x)<1e-6f &&
                                    p.point[v].y==q.point[v].y && p.point[v].z==q.point[v].z;
                }
                valid &= check(mirrored,"rebuilt cowl lost mirrored geometry/UV");
            }
            for(int tri=0;tri<2;++tri) for(int a=0;a<=10;++a) for(int b=0;b<=10-a;++b) {
                const auto p0=p.point[0],p1=p.point[tri+1],p2=p.point[tri+2];
                const float u=a/10.f,v=b/10.f;
                const CarPoint q{p0.x+u*(p1.x-p0.x)+v*(p2.x-p0.x),
                    p0.y+u*(p1.y-p0.y)+v*(p2.y-p0.y),p0.z+u*(p1.z-p0.z)+v*(p2.z-p0.z)};
                const float x=std::abs(q.x);
                if(x<.365f || x>.56f)continue;
                const float radius=x<.39f ? .145f+(x-.365f)*1.2f :
                                   x>.535f ? .175f-(x-.535f) : .175f;
                for(float axle : {kModelFrontAxle,kModelRearAxle}) {
                    const float overlap=radius-std::hypot(q.y-kModelWheelRadius,q.z-axle);
                    if(overlap>worst) {worst=overlap;worstPoint=q;worstPart=p.part;}
                    if(overlap>.003f)++penetrations;
                }
            }
        }
        if(penetrations)std::cerr<<carSpec(car).shortName<<" shell/tire penetrations="<<penetrations
            <<" worst="<<worst<<" part="<<int(worstPart)<<" at "
            <<worstPoint.x<<','<<worstPoint.y<<','<<worstPoint.z<<'\n';
        valid &= check(!penetrations,"rebuilt body intersects tire envelope");
        for(auto part : {CarPart::Nose,CarPart::Canopy,CarPart::FrontCowl,CarPart::RearCowl})
            valid &= check(parts[static_cast<unsigned>(part)]>0,"rebuilt car lost named structure");
        if(car==CarId::HurricaneSonic) {
            valid &= check(parts[static_cast<unsigned>(CarPart::FrontBridge)]>=3 &&
                           parts[static_cast<unsigned>(CarPart::RearWing)]>0,
                           "Sonic connecting front wing or integrated rear wing missing");
            for(float s : {-1.f,1.f}) {
                valid &= check(surfaceHeight(mesh,s*.22f,-.50f,CarPart::Unspecified,true)<.20f,
                               "Sonic rear channel filled by generic central hull");
                valid &= check(surfaceHeight(mesh,s*.29f,.24f,CarPart::Nose)>.20f,
                               "Sonic nose lost wide shoulder");
            }
            valid &= check(surfaceHeight(mesh,0,.69f,CarPart::FrontBridge)>.24f &&
                           surfaceHeight(mesh,.31f,.69f,CarPart::FrontBridge)>.30f,
                           "Sonic front bridge must dip between raised cowls");
        }
        if(car==CarId::NeoTridaggerZmc) {
            valid &= check(surfaceHeight(mesh,0,.62f,CarPart::Nose)>
                           surfaceHeight(mesh,.2f,.62f,CarPart::Nose)+.045f,
                           "Neo lost separate central dagger ridge");
            valid &= check(surfaceHeight(mesh,.11f,.28f,CarPart::Canopy)>.22f,
                           "Neo windshield returned to pointed bubble");
            valid &= check(surfaceHeight(mesh,0,-.90f,CarPart::RearWing)<0 &&
                           surfaceHeight(mesh,.47f,-.90f,CarPart::RearWing)>.54f,
                           "Neo swept split wing returned to straight plank");
            for(float s : {-1.f,1.f})
                valid &= check(surfaceHeight(mesh,s*.46f,.52f,CarPart::FrontCowl)<0,
                               "Neo front tire opening filled by broad fender");
            for(std::size_t i=0;i<mesh.count;++i)if(mesh.panels[i].part==CarPart::Roller)
                for(const auto p : mesh.panels[i].point)
                    valid &= check(p.z>-.18f,"Neo side roller incorrectly placed at rear bumper");
        }
        if(car==CarId::BrockenGigant) {
            valid &= check(parts[static_cast<unsigned>(CarPart::RearWing)]==0 &&
                           parts[static_cast<unsigned>(CarPart::TailFin)]>0 &&
                           parts[static_cast<unsigned>(CarPart::MotorBlock)]>0 &&
                           parts[static_cast<unsigned>(CarPart::SideGuard)]>0,
                           "Brocken lost motor/pipe guards or gained a tall spoiler");
            valid &= check(surfaceHeight(mesh,0,.30f,CarPart::Canopy)<0 &&
                           surfaceHeight(mesh,0,.30f,CarPart::MotorBlock)>.34f &&
                           surfaceHeight(mesh,0,.60f,CarPart::Nose)>.20f,
                           "Brocken three separate assemblies merged into continuous hull");
            for(float s : {-1.f,1.f})
                valid &= check(surfaceHeight(mesh,s*.60f,.45f,CarPart::SideGuard)>.17f,
                               "Brocken outer pipe guard missing at wheel");
        }
        if(car==CarId::SpinCobra) {
            valid &= check(surfaceHeight(mesh,.44f,.52f,CarPart::FrontCowl)>.39f &&
                           parts[unsigned(CarPart::FrontBridge)]>0 && parts[unsigned(CarPart::Intake)]>0,
                           "Cobra wide front fenders/independent nose/intakes missing");
        }
        if(car==CarId::BeakSpider) {
            valid &= check(surfaceHeight(mesh,.30f,-.92f,CarPart::RearWing)>.56f &&
                           surfaceHeight(mesh,.30f,-.79f,CarPart::RearWing)>.50f &&
                           surfaceHeight(mesh,.30f,-.66f,CarPart::RearWing)>.45f,
                           "Spider three separate rear wing planes missing");
        }
        if(car==CarId::RayStinger) {
            valid &= check(parts[unsigned(CarPart::RearWing)]==0 && parts[unsigned(CarPart::TailFin)]==2 &&
                           parts[unsigned(CarPart::Intake)]>=48,
                           "Stinger four intakes/single fin structure regressed");
        }
        if(car==CarId::Diospada) {
            valid &= check(surfaceHeight(mesh,0,-.83f,CarPart::RearWing)>.56f &&
                           surfaceHeight(mesh,.40f,-.83f,CarPart::RearWing)>.55f &&
                           parts[unsigned(CarPart::SideWeb)]>=12,
                           "Diospada arched wing or open scoop rails missing");
        }
    }
    std::cout<<carSpec(car).shortName<<" structure: tire clearance, mirrored UV and authored components across 3 LODs\n";
    return valid;
}
}  // namespace

int main()
{
    return validateCatalog() && validateWireframes() && validateDisplayMeshes() && validateMagnumStructure() &&
           validateRebuiltStructure(CarId::HurricaneSonic) &&
           validateRebuiltStructure(CarId::NeoTridaggerZmc) &&
           validateRebuiltStructure(CarId::BrockenGigant) &&
           validateRebuiltStructure(CarId::SpinCobra) &&
           validateRebuiltStructure(CarId::BeakSpider) &&
           validateRebuiltStructure(CarId::RayStinger) &&
           validateRebuiltStructure(CarId::Diospada) ? 0 : 1;
}
