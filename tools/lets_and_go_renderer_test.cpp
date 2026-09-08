#include "../main/apps/app_lets_and_go_racer/view/garage_renderer.h"
#include "../main/apps/app_lets_and_go_racer/view/race_renderer.h"
#include <hal/hal.h>
#include <filesystem>
#include <iostream>

using namespace lets_and_go;

int main(int argc, char** argv)
{
    const std::string directory = argc > 1 ? argv[1] : "/tmp/lets-go-frames";
    std::filesystem::create_directories(directory);
    auto& canvas = GetHAL().getDisplay();
    bool valid = true;
    const auto checkText = [&] {
        for (const auto& label : canvas.texts) {
            const float halfWidth = label.value.size() * 3.0f * label.size;
            const float halfHeight = 4.0f * label.size;
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                const float x = label.x + sx * halfWidth - 233;
                const float y = label.y + sy * halfHeight - 233;
                if (x * x + y * y > 225 * 225) {
                    std::cerr << "Text outside round-screen safe area: " << label.value << '\n';
                    valid = false;
                }
            }
            if (label.value.find("DEVELOPMENT") != std::string::npos) valid = false;
        }
    };
    const auto save = [&](const std::string& name) {
        checkText(); canvas.save(directory + "/" + name + ".ppm");
    };

    const TrackCamera testCamera = makeTrackLookAtCamera({0, 0, 0}, {0, 0, 1}, 466, 466);
    const auto testSurface = projectPencilSurface(testCamera, {-1,-1,2}, {1,-1,2}, {0,1,2}, 466, 466);
    float enter = 0, leave = 0;
    if (!pencilHiddenInterval(testSurface, {0,233}, {466,233}, 0.25f, 0.25f, enter, leave) ||
        !(enter > 0 && enter < 0.5f && leave > 0.5f && leave < 1) ||
        pencilHiddenInterval(testSurface, {0,233}, {466,233}, 1.0f, 1.0f, enter, leave)) {
        std::cerr << "Bridge occlusion ignored perspective depth or visible line fragments\n";
        valid = false;
    }
    const auto reversed = projectPencilSurface(testCamera, {0,1,2}, {1,-1,2}, {-1,-1,2}, 466, 466);
    if (!pencilHiddenInterval(reversed, {0,233}, {466,233}, 0.25f, 0.25f, enter, leave)) valid = false;
    uint32_t random = 1u;
    const auto coordinate = [&] {
        random = random * 1664525u + 1013904223u;
        return static_cast<float>(random & 0xffffu) / 65535.0f * 80.0f - 40.0f;
    };
    for (int i = 0; i < 8000; ++i) {
        const TrackVec3 a{coordinate(), coordinate(), coordinate()};
        const TrackVec3 b{coordinate(), coordinate(), coordinate()};
        const TrackVec3 c{coordinate(), coordinate(), coordinate()};
        const auto surface = projectPencilSurface(testCamera, a, b, c, 466, 466);
        for (std::size_t p = 0; p < surface.count; ++p) {
            const auto point = surface.points[p];
            if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < -0.01f ||
                point.y < -0.01f || point.x > 465.01f || point.y > 465.01f) valid = false;
        }
    }
    GarageRenderer garage;
    garage.open(466, 466);
    GameFlow flow;
    GarageSelection selection;
    garage.render(flow, selection, 0u, PencilDetail::High);
    save("input");
    flow.confirmInputAvailable();
    RacerInputStatus status;
    status.axesConnected = status.actionsConfigured = true;
    status.readiness = RacerInputReadiness::Calibrating;
    status.calibrationProgress = 0.60f;
    garage.render(flow, selection, 0u, PencilDetail::High, status);
    save("calibration");
    flow.completeCalibration(true);
    for (std::size_t i = 0; i < kCarCount; ++i) {
        selection.reset(static_cast<CarId>(i));
        garage.render(flow, selection, 500u, PencilDetail::High);
        const auto& spec = carSpec(static_cast<CarId>(i));
        const auto& pixels = canvas.frame();
        if (std::count(pixels.begin(), pixels.end(), spec.accentColor) < 10 ||
            std::count(pixels.begin(), pixels.end(), spec.wheelColor) < 10) {
            std::cerr << "Official accent/wheel color missing from garage\n";
            valid = false;
        }
        save("car-" + std::to_string(i));
    }
    selection.reset();
    flow.confirmPlayerCar();
    garage.render(flow, selection, 1000u, PencilDetail::High);
    save("showcase");
    flow.completeCarShowcase();
    flow.toggleRival(CarId::HurricaneSonic);
    flow.toggleRival(CarId::NeoTridaggerZmc);
    flow.toggleRival(CarId::BrockenGigant);
    garage.render(flow, selection, 500u, PencilDetail::High);
    save("rivals");
    flow.confirmRivals();
    garage.render(flow, selection, 0u, PencilDetail::High);
    save("track");
    for (unsigned orbit = 0; orbit < 16u; ++orbit) {
        garage.render(flow, selection, orbit * 2454u, PencilDetail::Low);
        checkText();
    }
    flow.confirmTrack();
    RaceController race;
    race.prepare(flow.setup(), 0x12345678u);
    RaceRenderer renderer;
    ResultsSelection results;
    renderer.open(466, 466);
    renderer.render(flow, race, results, 0u, false, PencilDetail::High);
    save("grid");
    flow.completeGridIntro();
    renderer.render(flow, race, results, 1000u, false, PencilDetail::High);
    save("countdown");
    flow.completeCountdown();
    RacerInput input;
    input.valid = true;
    for (int frame = 0; frame < 60 * 60 && !race.snapshot().playerFinished; ++frame) {
        race.stepFixed(input);
        if (frame % 45 == 0 && frame < 540) {
            renderer.render(flow, race, results, frame * 1000u / 60u, false, PencilDetail::High);
            save("race-" + std::to_string(frame / 45));
        }
    }
    flow.togglePause();
    renderer.render(flow, race, results, 200u, true, PencilDetail::High);
    save("paused");
    const auto pausedPixels = canvas.frame();
    renderer.render(flow, race, results, 5000u, true, PencilDetail::High);
    if (pausedPixels != canvas.frame()) {
        std::cerr << "Paused image was not frozen\n"; valid = false;
    }
    flow.togglePause();
    flow.finishRace();
    flow.showResults();
    renderer.render(flow, race, results, 0u, false, PencilDetail::High);
    save("results");
    // Exercise lower detail through a complete closed loop, including lapped
    // vehicles at the same physical location and both bridge approaches.
    flow.retrySameRace(); flow.completeGridIntro(); flow.completeCountdown();
    for (int sample = 0; sample < 96; ++sample) {
        // Test-only snapshot injection; renderer is read-only and physics is not run.
        auto& state = const_cast<RaceSnapshot&>(race.snapshot());
        state.cars[state.playerIndex].motion.distance = sample * race.track().length() / 96;
        state.cars[0].motion.distance = state.player().motion.distance + race.track().length() + 1;
        renderer.render(flow, race, results, 0u, false,
                        sample % 2 == 0 ? PencilDetail::Low : PencilDetail::Medium);
        checkText();
        if (sample == 48) save("bridge-lower");
        if (sample == 0) save("bridge-upper");
    }
    std::cout << "Renderer cache bytes: garage=" << sizeof(GarageRenderer)
              << " race=" << sizeof(RaceRenderer) << '\n';
    std::cout << "Production renderer frames: " << directory << '\n';
    return valid ? 0 : 1;
}
