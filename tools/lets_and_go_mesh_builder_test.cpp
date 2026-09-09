#include "../main/apps/app_lets_and_go_racer/model/car_mesh_builder.h"
#include <cassert>
#include <iostream>

using namespace lets_and_go;
using namespace lets_and_go::mesh_parts;

int main() {
    std::array<CarPanel, 256> panels{};
    MeshWriter empty{{nullptr, 0}};
    Builder emptyBuilder{empty, 24};
    emptyBuilder.box(0, 1, 0, 1, 0, 1, white);
    assert(empty.count == 0 && empty.overflowed);

    MeshWriter bounded{{panels.data(), 1}};
    Builder boundedBuilder{bounded, 24};
    boundedBuilder.part = CarPart::Nose;
    boundedBuilder.box(0, 1, 0, 1, 0, 1, white);
    assert(bounded.count == 1 && bounded.overflowed);
    assert(panels[0].part == CarPart::Nose && panels[0].paint == CarPaint::Solid);

    for(int segments : {12, 18, 24}) {
        MeshWriter mesh{{panels.data(), panels.size()}};
        Builder b{mesh, segments};
        b.part = CarPart::RearCowl;
        const auto stations = {CowlStation{-.5f,.2f,.4f,.3f,.4f},
                               CowlStation{.5f,.25f,.45f,.32f,.42f}};
        b.cowl(1, stations, blue, CarPaint::MagnumCowl);
        const auto half = mesh.count;
        b.cowl(-1, stations, blue, CarPaint::MagnumCowl);
        assert(mesh.count == 2 * half && !mesh.overflowed);
        for(std::size_t i = 0; i < half; ++i) {
            const auto& a = panels[i];
            const auto& c = panels[i + half];
            assert(a.u0 == c.u0 && a.u1 == c.u1 && a.v0 == c.v0 && a.v1 == c.v1);
            for(int p = 0; p < 4; ++p) {
                assert(a.point[p].x == -c.point[p].x);
                assert(a.point[p].y == c.point[p].y && a.point[p].z == c.point[p].z);
            }
        }
        mesh.count = 0;
        b.part = CarPart::Intake;
        b.duct(1, .3f, .4f, .6f, .05f, .03f, .1f, silver, graphite);
        assert(mesh.count == std::size_t(segments / 2 * 4) && !mesh.overflowed);
        for(std::size_t i = 0; i < mesh.count; i += 4) {
            assert(panels[i].part == CarPart::Intake);
            assert(panels[i].point[0].z > panels[i+3].point[0].z + .09f);
            assert(panels[i+1].point[0].z > panels[i+1].point[1].z);
            assert(panels[i+2].color == graphite);
        }
        mesh.count = 0;
        b.tube({0,0,0}, {0,0,0}, .1f, silver);
        assert(mesh.count == 0 && !mesh.overflowed);
        b.tube({0,0,0}, {0,1,0}, .1f, silver);
        assert(mesh.count == std::size_t(segments == 24 ? 10 : segments == 18 ? 8 : 6));
    }
    std::cout << "Mesh parts: bounded output, mirrored cowl UV, recessed ducts, tube basis (3 LODs)\n";
}
