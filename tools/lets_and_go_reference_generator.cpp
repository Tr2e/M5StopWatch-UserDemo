#include "../main/apps/app_lets_and_go_racer/controller/race_controller.h"
#include "../main/apps/app_lets_and_go_racer/model/car_catalog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using namespace lets_and_go;

std::string rgb565(uint16_t color)
{
    const int red = ((color >> 11u) & 0x1fu) * 255 / 31;
    const int green = ((color >> 5u) & 0x3fu) * 255 / 63;
    const int blue = (color & 0x1fu) * 255 / 31;
    std::ostringstream output;
    output << '#' << std::hex << std::setfill('0') << std::setw(2) << red
           << std::setw(2) << green << std::setw(2) << blue;
    return output.str();
}

const char* strokeColor(const CarSpec& spec, WireStroke stroke)
{
    (void)spec;
    return stroke == WireStroke::Mechanical ? "#59616b" : "#33485b";
}

void drawCar(std::ostream& svg, CarId id, float centerX, float centerY)
{
    const CarSpec& spec = carSpec(id);
    const CarWireframe mesh = buildCarWireframe(id, CarLod::Showcase);
    const std::string accent = rgb565(spec.accentColor);
    svg << "<g stroke-linecap=\"round\" stroke-linejoin=\"round\" fill=\"none\">\n";
    for (std::size_t index = 0; index < mesh.lineCount; ++index) {
        const WireLine& line = mesh.lines[index];
        const auto project = [centerX, centerY](CarPoint point) {
            return std::pair<float, float>{centerX + (point.x * 0.78f + point.z * 0.20f) * 98.0f,
                                           centerY + (point.z * 0.27f - point.y * 0.92f) * 98.0f};
        };
        const auto from = project(line.from);
        const auto to = project(line.to);
        const std::string color = line.stroke == WireStroke::Accent
                                      ? accent : strokeColor(spec, line.stroke);
        svg << "<line x1=\"" << from.first << "\" y1=\"" << from.second
            << "\" x2=\"" << to.first << "\" y2=\"" << to.second
            << "\" stroke=\"" << color << "\" stroke-width=\""
            << (line.stroke == WireStroke::Accent ? 2.5f : 1.45f) << "\"/>\n";
    }
    svg << "</g><text x=\"" << centerX << "\" y=\"" << centerY + 105.0f
        << "\" class=\"car-name\">" << spec.officialName << "</text>\n";
}

void drawTrack(std::ostream& svg, const OverpassTrack& track)
{
    constexpr int segments = 112;
    for (TrackLayer layer : {TrackLayer::Lower, TrackLayer::Transition, TrackLayer::Upper}) {
        for (int index = 0; index < segments; ++index) {
            const float fromDistance = track.length() * static_cast<float>(index) / segments;
            const float toDistance = track.length() * static_cast<float>(index + 1) / segments;
            if (track.layer((fromDistance + toDistance) * 0.5f) != layer) continue;
            for (float side : {-1.0f, 1.0f}) {
                const TrackVec3 from = track.edge(fromDistance, side);
                const TrackVec3 to = track.edge(toDistance, side);
                const char* color = layer == TrackLayer::Upper ? "#c4473d" : "#47647a";
                const float width = layer == TrackLayer::Upper ? 3.0f : 2.0f;
                svg << "<line x1=\"" << 350.0f + from.x * 11.0f << "\" y1=\""
                    << 565.0f + from.z * 11.0f << "\" x2=\""
                    << 350.0f + to.x * 11.0f << "\" y2=\""
                    << 565.0f + to.z * 11.0f << "\" stroke=\"" << color
                    << "\" stroke-width=\"" << width << "\" stroke-linecap=\"round\"/>\n";
            }
        }
    }
}

RaceSnapshot runReferenceRace(uint32_t seed)
{
    RaceSetup setup;
    setup.playerCar = CarId::CycloneMagnum;
    setup.rivalMask = carMask(CarId::HurricaneSonic) |
                      carMask(CarId::NeoTridaggerZmc) |
                      carMask(CarId::BrockenGigant);
    RaceController race;
    race.prepare(setup, seed);
    RacerInput input;
    input.valid = true;
    for (int step = 0; step < 60 * 180 && !race.snapshot().playerFinished; ++step) {
        const auto& player = race.snapshot().player();
        const TrackFrame frame = race.track().sample(player.motion.distance);
        input.steer = std::clamp(-frame.curvature * 1.8f -
                                     player.motion.lateralOffset * 0.8f,
                                 -1.0f, 1.0f);
        input.boostHeld = std::abs(frame.curvature) < 0.025f;
        input.brakeHeld = false;
        race.stepFixed(input);
    }
    return race.snapshot();
}

bool writeSummary(const std::string& path, uint32_t seed, const RaceSnapshot& race)
{
    std::ofstream output(path);
    if (!output) return false;
    output << std::fixed << std::setprecision(3)
           << "{\n  \"formatVersion\": 1,\n  \"seed\": \"0x" << std::hex << seed
           << std::dec << "\",\n  \"track\": \"SKY LOOP 01\",\n"
           << "  \"laps\": 3,\n  \"elapsedSeconds\": " << race.elapsedSeconds
           << ",\n  \"playerPosition\": "
           << static_cast<unsigned>(race.player().position)
           << ",\n  \"playerBestLapSeconds\": " << race.player().bestLapSeconds
           << ",\n  \"standingsAtPlayerFinish\": [\n";
    for (std::size_t position = 1; position <= race.carCount; ++position) {
        for (std::size_t index = 0; index < race.carCount; ++index) {
            const auto& car = race.cars[index];
            if (car.position != position) continue;
            output << "    {\"position\": " << position << ", \"car\": \""
                   << carSpec(car.car).officialName << "\", \"finished\": "
                   << (car.finished ? "true" : "false") << "}";
            if (position != race.carCount) output << ',';
            output << '\n';
        }
    }
    output << "  ]\n}\n";
    return true;
}

bool writeSvg(const std::string& path, uint32_t seed, const RaceSnapshot& race)
{
    std::ofstream svg(path);
    if (!svg) return false;
    svg << std::fixed << std::setprecision(2)
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"800\" viewBox=\"0 0 1200 800\">\n"
        << "<rect width=\"1200\" height=\"800\" rx=\"28\" fill=\"#e8ddc4\"/>\n"
        << "<style>text{font-family:monospace;fill:#33485b}.title{font-size:30px;font-weight:bold}.sub{font-size:15px}.car-name{font-size:13px;text-anchor:middle;font-weight:bold}.metric{font-size:18px}</style>\n"
        << "<text x=\"54\" y=\"58\" class=\"title\">LET&apos;S &amp; GO!! — STATIC REFERENCE</text>\n"
        << "<text x=\"54\" y=\"84\" class=\"sub\">actual Showcase wireframes · SKY LOOP 01 · seed 0x"
        << std::hex << seed << std::dec << "</text>\n";
    for (std::size_t index = 0; index < kCarCount; ++index) {
        drawCar(svg, static_cast<CarId>(index), 165.0f + index * 285.0f, 225.0f);
    }
    svg << "<rect x=\"45\" y=\"382\" width=\"620\" height=\"368\" rx=\"22\" fill=\"none\" stroke=\"#8a765e\" stroke-width=\"2\"/>\n"
        << "<text x=\"70\" y=\"420\" class=\"metric\">SKY LOOP 01 / NO CENTER DIVIDER</text>\n";
    OverpassTrack track;
    drawTrack(svg, track);
    svg << "<rect x=\"700\" y=\"382\" width=\"455\" height=\"368\" rx=\"22\" fill=\"#dfd1b4\" stroke=\"#8a765e\" stroke-width=\"2\"/>\n"
        << "<text x=\"730\" y=\"425\" class=\"title\">REFERENCE RACE</text>\n"
        << "<text x=\"730\" y=\"470\" class=\"metric\">LAPS       3 / 3</text>\n"
        << "<text x=\"730\" y=\"507\" class=\"metric\">TIME       " << race.elapsedSeconds << " s</text>\n"
        << "<text x=\"730\" y=\"544\" class=\"metric\">BEST LAP   " << race.player().bestLapSeconds << " s</text>\n"
        << "<text x=\"730\" y=\"581\" class=\"metric\">PLACE      "
        << static_cast<unsigned>(race.player().position) << " / " << race.carCount << "</text>\n"
        << "<text x=\"730\" y=\"635\" class=\"sub\">30 FPS render target</text>\n"
        << "<text x=\"730\" y=\"661\" class=\"sub\">60 Hz deterministic simulation</text>\n"
        << "<text x=\"730\" y=\"687\" class=\"sub\">466 × 466 circular AMOLED safe area</text>\n"
        << "<path d=\"M1090 410 l18 -10 l18 10 l-18 10 z\" fill=\"#d63732\"/><path d=\"M1118 410 l18 -10 l18 10 l-18 10 z\" fill=\"#3267a8\"/>\n"
        << "</svg>\n";
    return true;
}
}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: lets_and_go_reference_generator OUTPUT.svg OUTPUT.json\n";
        return 2;
    }
    constexpr uint32_t seed = 0x12345678u;
    const RaceSnapshot race = runReferenceRace(seed);
    if (!race.playerFinished || !writeSvg(argv[1], seed, race) ||
        !writeSummary(argv[2], seed, race)) {
        std::cerr << "failed to create deterministic reference artifacts\n";
        return 1;
    }
    return 0;
}
