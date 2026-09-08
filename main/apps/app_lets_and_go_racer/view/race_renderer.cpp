#include "race_renderer.h"

#include "track_projection.h"

#include <hal/hal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace lets_and_go {
namespace {

constexpr uint16_t kPaper = 0xef3au;
constexpr uint16_t kPencil = 0x4269u;
constexpr uint16_t kFaint = 0x9cd3u;
constexpr uint16_t kWarning = 0xd945u;

CarPoint spinWheel(CarPoint point, const CarSpec& spec, WireStroke stroke,
                   float cosine, float sine)
{
    if (stroke != WireStroke::Mechanical ||
        std::abs(std::abs(point.x) - 0.59f) > 0.02f) return point;
    const float axle = std::abs(point.z - spec.frontAxleZ) <
                               std::abs(point.z - spec.rearAxleZ)
                           ? spec.frontAxleZ : spec.rearAxleZ;
    const float y = point.y - spec.wheelRadius;
    const float z = point.z - axle;
    point.y = spec.wheelRadius + y * cosine - z * sine;
    point.z = axle + y * sine + z * cosine;
    return point;
}

struct CarPose {
    TrackVec3 base;
    TrackVec3 lateral;
    TrackVec3 forward;
    TrackVec3 up;
};

CarPose makeCarPose(const TrackFrame& frame, const RaceCarSnapshot& car)
{
    const float cosine = std::cos(car.motion.headingOffset);
    const float sine = std::sin(car.motion.headingOffset);
    CarPose pose;
    pose.base = trackAdd(frame.center,
                         trackScale(frame.lateral, car.motion.lateralOffset));
    pose.base.y += std::sin(frame.bankRadians) * car.motion.lateralOffset;
    const TrackVec3 right = trackNormalize(trackAdd(frame.lateral,
        {0.0f, std::sin(frame.bankRadians), 0.0f}));
    const TrackVec3 forward = trackNormalize(trackSubtract(frame.tangent,
        trackScale(right, trackDot(frame.tangent, right))));
    pose.up = trackNormalize(trackCross(forward, right));
    pose.lateral = trackAdd(trackScale(right, cosine), trackScale(forward, -sine));
    pose.forward = trackAdd(trackScale(forward, cosine), trackScale(right, sine));
    return pose;
}

TrackVec3 carPointToWorld(CarPoint point, const CarPose& pose)
{
    constexpr float kCarWorldScale = 0.34f;
    TrackVec3 result = pose.base;
    result = trackAdd(result, trackScale(pose.lateral, point.x * kCarWorldScale));
    result = trackAdd(result, trackScale(pose.forward, point.z * kCarWorldScale));
    result = trackAdd(result, trackScale(pose.up, 0.015f + point.y * kCarWorldScale));
    return result;
}

void drawRaceCar(LGFX_Sprite& canvas, const TrackCamera& camera,
                 const OverpassTrack& track, const RaceCarSnapshot& car,
                 const CompactRaceMesh& mesh, const PencilOcclusion& occlusion,
                 PencilDetail detail)
{
    const TrackFrame frame = track.sample(car.motion.distance);
    const CarPose pose = makeCarPose(frame, car);
    const CarSpec& spec = carSpec(car.car);
    const float wheelPhase = car.motion.distance / (0.34f * spec.wheelRadius);
    const float wheelCosine = std::cos(wheelPhase);
    const float wheelSine = std::sin(wheelPhase);
    for (std::size_t index = 0; index < mesh.lineCount; ++index) {
        const WireLine& line = mesh.lines[index];
        if (!car.player && detail != PencilDetail::High &&
            line.stroke == WireStroke::Mechanical && (index & 1u) != 0u) continue;
        const CarPoint from = spinWheel(line.from, spec, line.stroke,
                                        wheelCosine, wheelSine);
        const CarPoint to = spinWheel(line.to, spec, line.stroke,
                                      wheelCosine, wheelSine);
        const uint16_t color = line.stroke == WireStroke::Accent
                                   ? spec.accentColor : line.stroke == WireStroke::Mechanical
                                   ? spec.wheelColor : spec.bodyColor == 0x2145u ? spec.bodyColor : kPencil;
        occlusion.drawLine(canvas, camera,
                          carPointToWorld(from, pose), carPointToWorld(to, pose), color);
    }
}

void drawSpeedLines(LGFX_Sprite& canvas, const RaceCarSnapshot& player,
                    uint32_t elapsedMs, PencilDetail detail)
{
    if (player.motion.speed < 8.0f) return;
    int count = player.motion.speed > 18.0f ? 10 : player.motion.speed > 12.0f ? 6 : 2;
    if (detail == PencilDetail::Medium) count = std::max(2, count - 2);
    if (detail == PencilDetail::Low) count = std::max(1, count / 2);
    for (int index = 0; index < count; ++index) {
        const float travel = std::fmod(elapsedMs * 0.00065f + index * 0.173f, 1.0f);
        const int side = (index & 1) == 0 ? -1 : 1;
        const int x = 233 + side * static_cast<int>(80 + 140 * travel);
        const int y = 175 + static_cast<int>(200 * travel);
        const int length = 10 + static_cast<int>(player.motion.speed * 0.7f);
        canvas.drawLine(x, y, x + side * length, y + 8 + static_cast<int>(10 * travel), kFaint);
    }
}

void drawPaperAndMountains(LGFX_Sprite& canvas, PencilDetail detail)
{
    uint32_t hash = 0xa341316cu;
    const int textureCount = detail == PencilDetail::High ? 26
                             : detail == PencilDetail::Medium ? 16 : 8;
    for (int index = 0; index < textureCount; ++index) {
        hash = hash * 1664525u + 1013904223u;
        const int x = 40 + static_cast<int>((hash >> 8u) % 386u);
        hash = hash * 1664525u + 1013904223u;
        const int y = 38 + static_cast<int>((hash >> 8u) % 388u);
        canvas.drawLine(x, y, x + 3, y, kFaint);
    }
    constexpr int kHorizon = 148;
    const int mountainStep = detail == PencilDetail::Low ? 76 : 38;
    for (int x = 38; x < 430; x += mountainStep) {
        const int peak = kHorizon - 9 - ((x * 13) % 24);
        canvas.drawLine(x - 38, kHorizon, x, peak, kFaint);
        canvas.drawLine(x, peak, x + 39, kHorizon, kFaint);
    }
}

void drawHud(LGFX_Sprite& canvas, const RaceSnapshot& race,
             const OverpassTrack& track,
             const std::array<int16_t, 32u>& mapX,
             const std::array<int16_t, 32u>& mapY)
{
    const RaceCarSnapshot& player = race.player();
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(kPencil, kPaper);
    canvas.setTextSize(2);
    char value[32] = {};
    std::snprintf(value, sizeof(value), "LAP %u/3", static_cast<unsigned>(
        std::min<uint8_t>(kRaceLapCount, static_cast<uint8_t>(player.completedLaps + 1u))));
    canvas.drawString(value, 157, 55);
    std::snprintf(value, sizeof(value), "P%u/%u", static_cast<unsigned>(player.position),
                  static_cast<unsigned>(race.carCount));
    canvas.drawString(value, 312, 55);
    canvas.setTextSize(1);
    std::snprintf(value, sizeof(value), "%03d km/h",
                  static_cast<int>(std::lround(player.motion.speed * 3.6f)));
    canvas.drawString(value, 159, 411);
    canvas.drawRect(263, 408, 86, 10, kPencil);
    canvas.fillRect(265, 410, static_cast<int>(82.0f * player.motion.boostCharge),
                    6, carSpec(player.car).accentColor);
    canvas.drawString("BOOST", 306, 394);

    for (std::size_t index = 1; index < mapX.size(); ++index) {
        canvas.drawLine(mapX[index - 1u], mapY[index - 1u], mapX[index], mapY[index], kFaint);
    }
    canvas.drawLine(mapX.back(), mapY.back(), mapX.front(), mapY.front(), kFaint);
    for (std::size_t index = 0; index < race.carCount; ++index) {
        const RaceCarSnapshot& car = race.cars[index];
        const TrackFrame marker = track.sample(car.motion.distance);
        const int x = static_cast<int>(365.0f + marker.center.x * 2.2f);
        const int y = static_cast<int>(104.0f + marker.center.z * 1.45f);
        const uint16_t color = carSpec(car.car).accentColor;
        if (car.player) {
            canvas.fillCircle(x, y, 3, color);
        } else {
            canvas.drawCircle(x, y, 2, color);
        }
    }
}

void formatRaceTime(float seconds, char* output, std::size_t capacity)
{
    const uint32_t milliseconds = seconds > 0.0f
        ? static_cast<uint32_t>(seconds * 1000.0f + 0.5f) : 0u;
    const uint32_t minutes = milliseconds / 60000u;
    const uint32_t remainder = milliseconds % 60000u;
    std::snprintf(output, capacity, "%02u:%02u.%03u",
                  static_cast<unsigned>(minutes),
                  static_cast<unsigned>(remainder / 1000u),
                  static_cast<unsigned>(remainder % 1000u));
}

void drawResults(LGFX_Sprite& canvas, const RaceSnapshot& race,
                 const ResultsSelection& selection)
{
    canvas.fillScreen(kPaper);
    drawPaperAndMountains(canvas, PencilDetail::High);
    const RaceCarSnapshot& player = race.player();
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(carSpec(player.car).accentColor, kPaper);
    canvas.setTextSize(4);
    char position[20] = {};
    std::snprintf(position, sizeof(position), "PLACE %u/%u",
                  static_cast<unsigned>(player.position),
                  static_cast<unsigned>(race.carCount));
    canvas.drawString(position, canvas.width() / 2, 92);
    char time[20] = {};
    formatRaceTime(player.finished ? player.finishSeconds : race.elapsedSeconds, time, sizeof(time));
    canvas.setTextSize(2);
    canvas.setTextColor(kPencil, kPaper);
    canvas.drawString(time, canvas.width() / 2, 145);
    char best[32] = "BEST --:--.---";
    if (player.bestLapSeconds > 0.0f) {
        char lap[20] = {};
        formatRaceTime(player.bestLapSeconds, lap, sizeof(lap));
        std::snprintf(best, sizeof(best), "BEST %s", lap);
    }
    canvas.setTextSize(1);
    canvas.setTextColor(kFaint, kPaper);
    canvas.drawString(best, canvas.width() / 2, 177);
    for (int index = 0; index < static_cast<int>(ResultAction::Count); ++index) {
        const ResultAction action = static_cast<ResultAction>(index);
        const bool selected = action == selection.cursor();
        const int y = 245 + index * 50;
        if (selected) canvas.drawRoundRect(116, y - 17, 234, 36, 8,
                                           carSpec(player.car).accentColor);
        canvas.setTextSize(selected ? 2 : 1);
        canvas.setTextColor(selected ? kPencil : kFaint, kPaper);
        canvas.drawString(resultActionLabel(action), canvas.width() / 2, y);
    }
}

}  // namespace

void RaceRenderer::open(int width, int height)
{
    _width = width;
    _height = height;
    for (std::size_t car = 0; car < kCarCount; ++car) {
        CompactRaceMesh& target = _meshes[car];
        const auto result = buildCarWireframeInto(static_cast<CarId>(car), CarLod::Race,
            target.lines.data(), target.lines.size());
        target.lineCount = result.lineCount;
    }
    OverpassTrack track;
    _trackGeometry.open(track);
    for (std::size_t index = 0; index < _mapX.size(); ++index) {
        const TrackFrame frame = track.sample(
            track.length() * static_cast<float>(index) / static_cast<float>(_mapX.size()));
        _mapX[index] = static_cast<int16_t>(365 + frame.center.x * 2.2f);
        _mapY[index] = static_cast<int16_t>(104 + frame.center.z * 1.45f);
    }
}

void RaceRenderer::close()
{
    _width = 0;
    _height = 0;
}

void RaceRenderer::render(const GameFlow& flow, const RaceController& race,
                          const ResultsSelection& results,
                          uint32_t screenElapsedMs, bool pausedForInputLoss,
                          PencilDetail detail)
{
    if (_width <= 0 || _height <= 0 || !race.prepared()) return;
    auto& canvas = GetHAL().getCanvas();
    if (flow.screen() == GameScreen::Results) {
        drawResults(canvas, race.snapshot(), results);
        return;
    }
    canvas.fillScreen(kPaper);
    drawPaperAndMountains(canvas, detail);
    const RaceSnapshot& snapshot = race.snapshot();
    const RaceCarSnapshot& player = snapshot.player();
    const TrackFrame playerFrame = race.track().sample(player.motion.distance);
    TrackVec3 cameraPosition = trackSubtract(playerFrame.center,
                                              trackScale(playerFrame.tangent, 3.8f));
    cameraPosition = trackAdd(cameraPosition,
                              trackScale(playerFrame.lateral, player.motion.lateralOffset));
    cameraPosition.y += 2.15f;
    TrackVec3 target = trackAdd(playerFrame.center, trackScale(playerFrame.tangent, 5.2f));
    target.y += 0.35f;
    const TrackCamera camera = makeTrackLookAtCamera(cameraPosition, target,
                                                     _width, _height, 0.78f);
    drawPencilTrack(canvas, camera, _trackGeometry, detail, &_occlusion);

    std::array<std::size_t, kMaximumRaceCars> order{};
    std::array<float, kMaximumRaceCars> depth{};
    for (std::size_t i = 0; i < snapshot.carCount; ++i) {
        order[i] = i;
        depth[i] = trackToCamera(camera, race.track().sample(snapshot.cars[i].motion.distance).center).z;
    }
    std::sort(order.begin(), order.begin() + snapshot.carCount,
              [&](std::size_t a, std::size_t b) { return depth[a] > depth[b]; });
    for (std::size_t slot = 0; slot < snapshot.carCount; ++slot) {
        const RaceCarSnapshot& car = snapshot.cars[order[slot]];
        if (depth[order[slot]] < kTrackNearPlane || !car.active ||
            (car.finished && !car.player)) continue;
        drawRaceCar(canvas, camera, race.track(), car,
                    _meshes[static_cast<std::size_t>(car.car)], _occlusion, detail);
    }
    if (flow.screen() == GameScreen::Racing)
        drawSpeedLines(canvas, player, screenElapsedMs, detail);

    canvas.setTextDatum(textdatum_t::middle_center);
    if (player.motion.wallImpact > 0.05f) {
        canvas.drawCircle(_width / 2, _height / 2, 194, kWarning);
        canvas.drawCircle(_width / 2 + 2, _height / 2 - 1, 188, kWarning);
    }
    drawHud(canvas, snapshot, race.track(), _mapX, _mapY);
    const GameScreen screen = flow.screen();
    if (screen == GameScreen::GridIntro) {
        canvas.setTextColor(kPencil, kPaper);
        canvas.setTextSize(3);
        canvas.drawString(snapshot.carCount == 1u ? "SOLO RUN" : "CHASE THE PACK", _width / 2, 218);
    } else if (screen == GameScreen::Countdown) {
        const int count = std::max(1, 3 - static_cast<int>(screenElapsedMs / 1000u));
        char number[4] = {};
        std::snprintf(number, sizeof(number), "%d", count);
        canvas.setTextColor(kWarning, kPaper);
        canvas.setTextSize(7);
        canvas.drawString(number, _width / 2, 222);
    } else if (screen == GameScreen::Paused) {
        canvas.setTextColor(kWarning, kPaper);
        canvas.setTextSize(3);
        canvas.drawString(pausedForInputLoss ? "INPUT LOST" : "PAUSED", _width / 2, 213);
        canvas.setTextSize(1);
        canvas.drawString("HOLD RED TO CONTINUE", _width / 2, 246);
    } else if (screen == GameScreen::Finish) {
        canvas.setTextColor(kWarning, kPaper);
        canvas.setTextSize(5);
        canvas.drawString("FINISH!", _width / 2, 220);
    }
}

}  // namespace lets_and_go
