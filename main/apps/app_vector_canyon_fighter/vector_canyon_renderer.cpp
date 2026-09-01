#include "vector_canyon_renderer.h"

#include <hal/hal.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace vector_canyon_fighter {
namespace {

constexpr int kHorizonY = 150;
constexpr int kShipCenterY = 335;
constexpr float kTerrainFocalLength = 112.0f;
constexpr float kTerrainVerticalScale = 94.0f;
constexpr float kShipFocalLength = 62.0f;
constexpr float kTerrainNearPlane = 0.22f;

struct ProjectedTerrainRow {
    std::array<int16_t, TerrainStream::kColumnCount> x;
    std::array<int16_t, TerrainStream::kColumnCount> y;
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct ShipEdge {
    uint8_t from;
    uint8_t to;
    uint8_t style;
};

constexpr std::array<Vec3, 50> kShipVertices = {{
    {0.00f, 0.02f, 3.70f},   {0.00f, 0.24f, 2.28f},   {-0.40f, 0.02f, 1.38f},
    {0.40f, 0.02f, 1.38f},   {0.00f, 0.72f, 0.88f},   {0.00f, 0.78f, -0.18f},
    {-0.44f, 0.34f, -0.08f}, {0.44f, 0.34f, -0.08f}, {-0.56f, -0.04f, 0.02f},
    {0.56f, -0.04f, 0.02f},  {0.00f, -0.28f, 0.92f}, {0.00f, -0.34f, -1.54f},
    {0.00f, 0.14f, -2.18f},  {0.00f, -0.22f, -2.34f},

    {-0.62f, 0.16f, 0.55f},  {-1.65f, 0.38f, -0.05f}, {-3.00f, 0.62f, -0.72f},
    {0.62f, 0.16f, 0.55f},   {1.65f, 0.38f, -0.05f},  {3.00f, 0.62f, -0.72f},
    {-0.62f, -0.14f, 0.10f}, {-1.55f, -0.42f, -0.55f}, {-2.75f, -0.68f, -1.28f},
    {0.62f, -0.14f, 0.10f},  {1.55f, -0.42f, -0.55f},  {2.75f, -0.68f, -1.28f},

    {-1.65f, 0.48f, -0.25f}, {-1.65f, 0.04f, -0.25f}, {-1.65f, 0.36f, -1.55f},
    {-1.65f, -0.08f, -1.55f}, {-0.70f, 0.00f, -0.35f}, {-0.70f, -0.44f, -0.35f},
    {-0.70f, -0.10f, -1.72f}, {-0.70f, -0.54f, -1.72f}, {0.70f, 0.00f, -0.35f},
    {0.70f, -0.44f, -0.35f}, {0.70f, -0.10f, -1.72f}, {0.70f, -0.54f, -1.72f},
    {1.65f, 0.48f, -0.25f},  {1.65f, 0.04f, -0.25f},  {1.65f, 0.36f, -1.55f},
    {1.65f, -0.08f, -1.55f},

    {-0.52f, 0.20f, -1.50f}, {-0.64f, 1.02f, -1.88f}, {0.52f, 0.20f, -1.50f},
    {0.64f, 1.02f, -1.88f}, {-3.06f, 0.62f, -1.62f}, {3.06f, 0.62f, -1.62f},
    {-2.80f, -0.68f, -2.04f}, {2.80f, -0.68f, -2.04f},
}};

constexpr std::array<ShipEdge, 78> kShipEdges = {{
    {0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {1, 2, 0}, {1, 3, 0}, {1, 4, 2},
    {4, 5, 2}, {4, 6, 2}, {4, 7, 2}, {5, 6, 2}, {5, 7, 2}, {6, 7, 2},
    {2, 8, 1}, {3, 9, 1}, {2, 10, 0}, {3, 10, 0}, {8, 9, 0}, {8, 11, 1},
    {9, 11, 1}, {10, 11, 0}, {8, 12, 1}, {9, 12, 1}, {11, 13, 0}, {12, 13, 1},

    {2, 14, 1}, {14, 15, 1}, {15, 16, 1}, {16, 8, 1}, {14, 16, 0}, {15, 8, 0},
    {3, 17, 1}, {17, 18, 1}, {18, 19, 1}, {19, 9, 1}, {17, 19, 0}, {18, 9, 0},
    {8, 20, 1}, {20, 21, 1}, {21, 22, 1}, {22, 11, 1}, {20, 22, 0}, {21, 11, 0},
    {9, 23, 1}, {23, 24, 1}, {24, 25, 1}, {25, 11, 1}, {23, 25, 0}, {24, 11, 0},

    {26, 27, 1}, {26, 28, 1}, {27, 29, 1}, {28, 29, 1}, {15, 26, 0},
    {38, 39, 1}, {38, 40, 1}, {39, 41, 1}, {40, 41, 1}, {18, 38, 0},
    {30, 31, 1}, {30, 32, 1}, {31, 33, 1}, {32, 33, 1}, {20, 30, 0},
    {34, 35, 1}, {34, 36, 1}, {35, 37, 1}, {36, 37, 1}, {23, 34, 0},

    {8, 42, 1}, {42, 43, 1}, {43, 12, 1}, {9, 44, 1}, {44, 45, 1}, {45, 12, 1},
    {16, 46, 1}, {19, 47, 1}, {22, 48, 1}, {25, 49, 1},
}};

int projectX(int centerX, float worldX, float z)
{
    return centerX + static_cast<int>(kTerrainFocalLength * worldX / z);
}

int projectY(float worldY, float z)
{
    return kHorizonY - static_cast<int>(kTerrainVerticalScale * worldY / z);
}

std::size_t columnStride(std::size_t sourceRow)
{
    if (sourceRow > 17) return 3;
    if (sourceRow > 7) return 2;
    return 1;
}

}  // namespace

void Renderer::open(int width, int height)
{
    _width = width;
    _height = height;
}

void Renderer::close()
{
    _width = 0;
    _height = 0;
}

void Renderer::render(const FlightState& flight, const TerrainStream& terrain, const CollisionStatus& collision, bool inputReady)
{
    if (_width <= 0 || _height <= 0) return;

    auto& display = GetHAL().getDisplay();
    auto& canvas = display;
    const int centerX = _width / 2;
    const uint16_t terrainPrimary = display.color565(255, 113, 56);
    const uint16_t terrainMid = display.color565(182, 70, 36);
    const uint16_t terrainSecondary = display.color565(87, 39, 29);
    const uint16_t shipColor = display.color565(233, 242, 242);
    const uint16_t shipDim = display.color565(71, 102, 109);
    const uint16_t canopyColor = display.color565(88, 166, 216);
    const uint16_t exhaust = display.color565(255, 209, 102);
    const uint16_t hudColor = display.color565(128, 216, 232);
    const uint16_t hudDim = display.color565(39, 95, 105);
    const uint16_t hudAccent = display.color565(227, 251, 255);
    const uint16_t caution = display.color565(255, 225, 90);

    // The StopWatch display already owns a PSRAM framebuffer. Drawing into a
    // second full-screen sprite and pushing it duplicated the entire 468x466
    // memory copy before every panel transfer. Hold one outer transaction so
    // nested drawing calls only update the framebuffer; endWrite submits the
    // completed frame once.
    display.startWrite();
    canvas.fillScreen(TFT_BLACK);

    std::array<ProjectedTerrainRow, TerrainStream::kSliceCount + 1> rows = {};
    std::array<std::size_t, TerrainStream::kSliceCount + 1> sourceRows = {};
    std::size_t rowCount = 0;
    const auto& slices = terrain.slices();

    std::size_t firstVisible = 0;
    while (firstVisible < slices.size() && slices[firstVisible].z < kTerrainNearPlane) ++firstVisible;

    if (firstVisible > 0 && firstVisible < slices.size()) {
        const auto& behind = slices[firstVisible - 1];
        const auto& ahead = slices[firstVisible];
        const float blend = std::clamp((kTerrainNearPlane - behind.z) / (ahead.z - behind.z), 0.0f, 1.0f);
        const float center = behind.center + (ahead.center - behind.center) * blend;
        for (std::size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            const float height = behind.surfaceHeights[column] +
                                 (ahead.surfaceHeights[column] - behind.surfaceHeights[column]) * blend;
            rows[rowCount].x[column] = projectX(
                centerX, TerrainStream::columnWorldX(center, column) - flight.lateralOffset, kTerrainNearPlane);
            rows[rowCount].y[column] = projectY(height - flight.altitude, kTerrainNearPlane);
        }
        sourceRows[rowCount] = firstVisible;
        ++rowCount;
    }

    for (std::size_t row = firstVisible; row < slices.size(); ++row) {
        for (std::size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            rows[rowCount].x[column] =
                projectX(centerX, TerrainStream::columnWorldX(slices[row].center, column) - flight.lateralOffset,
                         slices[row].z);
            rows[rowCount].y[column] = projectY(slices[row].surfaceHeights[column] - flight.altitude, slices[row].z);
        }
        sourceRows[rowCount] = row;
        ++rowCount;
    }
    const int vanishingX = projectX(centerX, slices.back().center - flight.lateralOffset, slices.back().z);

    const auto rowColor = [&](std::size_t row) {
        return sourceRows[row] > 17 ? terrainSecondary : (sourceRows[row] > 7 ? terrainMid : terrainPrimary);
    };
    const auto drawRow = [&](std::size_t row) {
        const std::size_t stride = columnStride(sourceRows[row]);
        std::size_t previous = 0;
        for (std::size_t column = stride; column < TerrainStream::kColumnCount; column += stride) {
            canvas.drawLine(rows[row].x[previous], rows[row].y[previous], rows[row].x[column], rows[row].y[column],
                            rowColor(row));
            previous = column;
        }
        const std::size_t last = TerrainStream::kColumnCount - 1;
        if (previous != last) {
            canvas.drawLine(rows[row].x[previous], rows[row].y[previous], rows[row].x[last], rows[row].y[last],
                            rowColor(row));
        }
    };

    if (rowCount == 0) {
        display.endWrite();
        return;
    }
    drawRow(rowCount - 1);
    for (std::size_t farRow = rowCount - 1; farRow > 0; --farRow) {
        const std::size_t nearRow = farRow - 1;
        drawRow(nearRow);
        const std::size_t stride = std::max(columnStride(sourceRows[nearRow]), columnStride(sourceRows[farRow]));
        for (std::size_t column = 0; column < TerrainStream::kColumnCount; column += stride) {
            canvas.drawLine(rows[nearRow].x[column], rows[nearRow].y[column], rows[farRow].x[column],
                            rows[farRow].y[column], rowColor(nearRow));
        }
        const std::size_t last = TerrainStream::kColumnCount - 1;
        if (last % stride != 0) {
            canvas.drawLine(rows[nearRow].x[last], rows[nearRow].y[last], rows[farRow].x[last], rows[farRow].y[last],
                            rowColor(nearRow));
        }
    }

    const int shipX = centerX + static_cast<int>(flight.lateralOffset * 12.0f);
    const int shipY = kShipCenterY - static_cast<int>(flight.pitch * 0.8f);
    const float radians = flight.roll * 0.0174532925f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const auto projectShip = [&](const Vec3& point) {
        const float depth = point.z + 5.7f;
        const float localX = kShipFocalLength * point.x / depth;
        const float viewY = point.y + point.z * (0.52f + flight.pitch * 0.0025f);
        const float localY = -kShipFocalLength * viewY / depth;
        return std::array<int, 2>{
            shipX + static_cast<int>(localX * cosine - localY * sine),
            shipY + static_cast<int>(localX * sine + localY * cosine),
        };
    };

    for (const auto& edge : kShipEdges) {
        const auto from = projectShip(kShipVertices[edge.from]);
        const auto to = projectShip(kShipVertices[edge.to]);
        const uint16_t color = edge.style == 2 ? canopyColor : (edge.style == 1 ? shipColor : shipDim);
        if (edge.style == 1) canvas.drawLine(from[0], from[1] + 1, to[0], to[1] + 1, shipDim);
        canvas.drawLine(from[0], from[1], to[0], to[1], color);
    }

    const float exhaustTailZ = -2.30f - 0.86f * flight.boostAmount;
    const std::array<Vec3, 8> exhaustPoints = {{
        {-1.65f, 0.14f, -1.55f}, {-1.65f, 0.14f, exhaustTailZ},
        {-0.70f, -0.32f, -1.72f}, {-0.70f, -0.32f, exhaustTailZ},
        {0.70f, -0.32f, -1.72f}, {0.70f, -0.32f, exhaustTailZ},
        {1.65f, 0.14f, -1.55f}, {1.65f, 0.14f, exhaustTailZ},
    }};
    for (int engine = 0; engine < 4; ++engine) {
        const auto from = projectShip(exhaustPoints[engine * 2]);
        const auto to = projectShip(exhaustPoints[engine * 2 + 1]);
        canvas.drawLine(from[0], from[1], to[0], to[1], exhaust);
    }

    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setTextSize(1);

    const int heading = static_cast<int>(flight.heading + 0.5f) % 360;
    canvas.setCursor(92, 50);
    canvas.print("VR-01");
    canvas.setCursor(_width - 120, 50);
    canvas.print(inputReady ? "NAV" : "CAL");
    canvas.setCursor(centerX - 25, 42);
    canvas.print("HDG");
    canvas.setCursor(centerX + 1, 42);
    canvas.printf("%03d", heading);
    canvas.drawLine(centerX - 55, 63, centerX + 55, 63, hudDim);
    for (int tick = -2; tick <= 2; ++tick) {
        const int x = centerX + tick * 22;
        const int length = tick == 0 ? 8 : 4;
        canvas.drawLine(x, 63, x, 63 + length, tick == 0 ? hudAccent : hudColor);
    }
    canvas.drawLine(centerX - 4, 75, centerX, 69, hudAccent);
    canvas.drawLine(centerX, 69, centerX + 4, 75, hudAccent);

    // Bank scale: fixed ticks with a moving cyan pointer.
    constexpr int kBankCenterY = 112;
    constexpr int kBankRadius = 30;
    for (int angle = -45; angle <= 45; angle += 15) {
        const float bankRadians = static_cast<float>(angle - 90) * 0.0174532925f;
        const int x1 = centerX + static_cast<int>(std::cos(bankRadians) * kBankRadius);
        const int y1 = kBankCenterY + static_cast<int>(std::sin(bankRadians) * kBankRadius);
        const int tickLength = angle == 0 ? 7 : (angle % 30 == 0 ? 5 : 3);
        const int x2 = centerX + static_cast<int>(std::cos(bankRadians) * (kBankRadius - tickLength));
        const int y2 = kBankCenterY + static_cast<int>(std::sin(bankRadians) * (kBankRadius - tickLength));
        canvas.drawLine(x1, y1, x2, y2, angle == 0 ? hudColor : hudDim);
    }
    const float bankPointerRadians = (-flight.roll - 90.0f) * 0.0174532925f;
    const int bankPointerX = centerX + static_cast<int>(std::cos(bankPointerRadians) * (kBankRadius - 10));
    const int bankPointerY = kBankCenterY + static_cast<int>(std::sin(bankPointerRadians) * (kBankRadius - 10));
    canvas.drawCircle(bankPointerX, bankPointerY, 2, hudAccent);

    // Pitch ladder rotates with roll while labels remain upright for legibility.
    const int attitudeY = kHorizonY + static_cast<int>(flight.pitch * 1.25f);
    const float attitudeRadians = -flight.roll * 0.0174532925f;
    const float attitudeCosine = std::cos(attitudeRadians);
    const float attitudeSine = std::sin(attitudeRadians);
    const auto drawAttitudeLine = [&](int x1, int y1, int x2, int y2, uint16_t color) {
        const auto rotateX = [&](int x, int y) {
            return vanishingX + static_cast<int>(x * attitudeCosine - y * attitudeSine);
        };
        const auto rotateY = [&](int x, int y) {
            return attitudeY + static_cast<int>(x * attitudeSine + y * attitudeCosine);
        };
        canvas.drawLine(rotateX(x1, y1), rotateY(x1, y1), rotateX(x2, y2), rotateY(x2, y2), color);
    };
    for (int level = -2; level <= 2; ++level) {
        const int localY = -level * 18;
        const int halfWidth = level == 0 ? 78 : (std::abs(level) == 1 ? 35 : 27);
        const int gap = level == 0 ? 14 : 11;
        const uint16_t color = level == 0 ? hudColor : hudDim;
        drawAttitudeLine(-halfWidth, localY, -gap, localY, color);
        drawAttitudeLine(gap, localY, halfWidth, localY, color);
        if (level != 0) {
            drawAttitudeLine(-halfWidth, localY, -halfWidth, localY + (level > 0 ? 5 : -5), color);
            drawAttitudeLine(halfWidth, localY, halfWidth, localY + (level > 0 ? 5 : -5), color);
            canvas.setTextColor(hudDim, TFT_BLACK);
            canvas.setCursor(vanishingX - halfWidth - 17, attitudeY + localY - 3);
            canvas.printf("%d", std::abs(level) * 10);
            canvas.setCursor(vanishingX + halfWidth + 6, attitudeY + localY - 3);
            canvas.printf("%d", std::abs(level) * 10);
        }
    }
    canvas.setTextColor(hudColor, TFT_BLACK);

    // Flight-path marker follows the canyon vanishing point. The W marker is
    // the fixed aircraft datum, so their separation communicates flight path.
    canvas.drawCircle(vanishingX, kHorizonY, 5, hudAccent);
    canvas.drawLine(vanishingX - 14, kHorizonY, vanishingX - 5, kHorizonY, hudAccent);
    canvas.drawLine(vanishingX + 5, kHorizonY, vanishingX + 14, kHorizonY, hudAccent);
    canvas.drawLine(vanishingX, kHorizonY - 10, vanishingX, kHorizonY - 5, hudAccent);
    constexpr int kDatumY = 188;
    canvas.drawLine(centerX - 24, kDatumY, centerX - 8, kDatumY, hudColor);
    canvas.drawLine(centerX - 8, kDatumY, centerX, kDatumY + 7, hudColor);
    canvas.drawLine(centerX, kDatumY + 7, centerX + 8, kDatumY, hudColor);
    canvas.drawLine(centerX + 8, kDatumY, centerX + 24, kDatumY, hudColor);

    // Speed tape with primary/secondary ticks and a boxed active readout.
    canvas.setCursor(39, 127);
    canvas.print("SPD");
    canvas.drawRect(34, 139, 35, 15, hudDim);
    canvas.setCursor(40, 143);
    canvas.printf("%03d", static_cast<int>(flight.speed + 0.5f));
    canvas.drawLine(45, 160, 45, 264, hudDim);
    const int speedMarkerY = 255 - std::clamp(static_cast<int>((flight.speed - 42.0f) * 1.05f), 0, 95);
    for (int tick = 0; tick <= 10; ++tick) {
        const int y = 160 + tick * 10;
        const bool major = tick % 2 == 0;
        canvas.drawLine(45, y, major ? 59 : 52, y, major ? hudColor : hudDim);
    }
    canvas.drawLine(40, speedMarkerY, 61, speedMarkerY, hudAccent);
    canvas.drawLine(61, speedMarkerY, 65, speedMarkerY - 3, hudAccent);
    canvas.drawLine(61, speedMarkerY, 65, speedMarkerY + 3, hudAccent);
    canvas.setTextColor(hudDim, TFT_BLACK);
    canvas.setCursor(35, 270);
    canvas.printf("T%02d", std::clamp(static_cast<int>((flight.speed - 42.0f) * 0.67f), 0, 60));

    // Altitude tape mirrors speed and adds a pitch-derived trend cue.
    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setCursor(_width - 72, 127);
    canvas.print("ALT");
    canvas.drawRect(_width - 81, 139, 42, 15, hudDim);
    canvas.setCursor(_width - 77, 143);
    canvas.printf("%+03d", static_cast<int>(flight.altitude * 10.0f));
    canvas.drawLine(_width - 45, 160, _width - 45, 264, hudDim);
    const int altitudeMarkerY = 208 - std::clamp(static_cast<int>(flight.altitude * 32.0f), -42, 42);
    for (int tick = 0; tick <= 10; ++tick) {
        const int y = 160 + tick * 10;
        const bool major = tick % 2 == 0;
        canvas.drawLine(_width - 45, y, _width - (major ? 59 : 52), y, major ? hudColor : hudDim);
    }
    canvas.drawLine(_width - 61, altitudeMarkerY, _width - 40, altitudeMarkerY, hudAccent);
    canvas.drawLine(_width - 61, altitudeMarkerY, _width - 65, altitudeMarkerY - 3, hudAccent);
    canvas.drawLine(_width - 61, altitudeMarkerY, _width - 65, altitudeMarkerY + 3, hudAccent);
    canvas.setTextColor(hudDim, TFT_BLACK);
    canvas.setCursor(_width - 75, 270);
    canvas.printf("P%+03d", static_cast<int>(flight.pitch));

    // Compact course-deviation scale sits above the fighter, not over terrain.
    constexpr int kCourseY = 294;
    canvas.drawLine(centerX - 52, kCourseY, centerX + 52, kCourseY, hudDim);
    for (int tick = -2; tick <= 2; ++tick) {
        const int x = centerX + tick * 21;
        canvas.drawLine(x, kCourseY - 3, x, kCourseY + 3, tick == 0 ? hudColor : hudDim);
    }
    const int deviationX = centerX + std::clamp(static_cast<int>(flight.lateralOffset * 19.0f), -42, 42);
    canvas.drawLine(deviationX - 4, kCourseY - 9, deviationX, kCourseY - 4, hudAccent);
    canvas.drawLine(deviationX, kCourseY - 4, deviationX + 4, kCourseY - 9, hudAccent);

    canvas.setCursor(92, _height - 48);
    canvas.print(inputReady ? "IN IMU" : "IN CAL");
    canvas.setCursor(_width - 143, _height - 48);
    canvas.setTextColor(flight.boostAmount > 0.05f ? caution : hudColor, TFT_BLACK);
    canvas.print(flight.boostAmount > 0.05f ? "THR BOOST" : "THR CRZ");
    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setCursor(centerX - 34, _height - 48);
    canvas.printf("R%+03d P%+03d", static_cast<int>(flight.roll), static_cast<int>(flight.pitch));
    if (flight.collided) {
        canvas.setTextColor(caution, TFT_BLACK);
        canvas.drawLine(centerX - 52, 190, centerX + 52, 190, caution);
        canvas.setCursor(centerX - 37, 198);
        canvas.print("IMPACT: A HOLD");
    } else if (flight.paused) {
        canvas.setTextColor(hudColor, TFT_BLACK);
        canvas.setCursor(centerX - 20, 198);
        canvas.print("PAUSED");
    } else if (collision.warning) {
        canvas.setTextColor(caution, TFT_BLACK);
        canvas.drawLine(centerX - 34, 190, centerX + 34, 190, caution);
        canvas.setCursor(centerX - 23, 198);
        canvas.print("TERRAIN");
    }

    display.endWrite();
}

}  // namespace vector_canyon_fighter
