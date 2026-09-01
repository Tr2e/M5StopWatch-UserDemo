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

constexpr std::array<Vec3, 32> kShipVertices = {{
    {0.0f, 0.02f, 3.55f},     // 0 needle nose
    {0.0f, 0.26f, 2.15f},     // 1 upper fuselage
    {-0.42f, 0.02f, 1.38f},   // 2 forward shoulder L
    {0.42f, 0.02f, 1.38f},    // 3 forward shoulder R
    {0.0f, 0.70f, 0.82f},     // 4 canopy crown
    {-0.43f, 0.36f, -0.02f},  // 5 canopy rear L
    {0.43f, 0.36f, -0.02f},   // 6 canopy rear R
    {-0.54f, -0.04f, 0.08f},  // 7 fuselage waist L
    {0.54f, -0.04f, 0.08f},   // 8 fuselage waist R
    {-0.78f, -0.06f, 0.62f},  // 9 wing root front L
    {0.78f, -0.06f, 0.62f},   // 10 wing root front R
    {-3.10f, -0.14f, -0.42f}, // 11 swept wing tip L
    {3.10f, -0.14f, -0.42f},  // 12 swept wing tip R
    {-2.45f, -0.16f, -1.20f}, // 13 wing trailing outer L
    {2.45f, -0.16f, -1.20f},  // 14 wing trailing outer R
    {-1.18f, -0.10f, -1.67f}, // 15 wing trailing inner L
    {1.18f, -0.10f, -1.67f},  // 16 wing trailing inner R
    {-0.84f, 0.22f, -1.40f},  // 17 engine upper L
    {0.84f, 0.22f, -1.40f},   // 18 engine upper R
    {-0.84f, -0.34f, -1.82f}, // 19 engine lower L
    {0.84f, -0.34f, -1.82f},  // 20 engine lower R
    {-0.66f, 0.12f, -2.18f},  // 21 nozzle L
    {0.66f, 0.12f, -2.18f},   // 22 nozzle R
    {0.0f, -0.04f, -2.40f},   // 23 center tail
    {-0.58f, 0.24f, -1.72f},  // 24 tail base L
    {0.58f, 0.24f, -1.72f},   // 25 tail base R
    {-0.67f, 1.18f, -1.94f},  // 26 vertical tail L
    {0.67f, 1.18f, -1.94f},   // 27 vertical tail R
    {-1.02f, 0.00f, 1.10f},   // 28 canard L
    {1.02f, 0.00f, 1.10f},    // 29 canard R
    {0.0f, 0.78f, -0.12f},    // 30 canopy rear crown
    {0.0f, -0.34f, -1.58f},   // 31 keel
}};

constexpr std::array<ShipEdge, 52> kShipEdges = {{
    {0, 1, 1}, {0, 2, 1}, {0, 3, 1}, {1, 2, 0}, {1, 3, 0},
    {1, 4, 2}, {4, 5, 2}, {4, 6, 2}, {5, 6, 2}, {5, 30, 2}, {6, 30, 2}, {4, 30, 2},
    {2, 7, 1}, {3, 8, 1}, {2, 28, 1}, {3, 29, 1}, {28, 7, 1}, {29, 8, 1},
    {7, 8, 0}, {7, 23, 0}, {8, 23, 0}, {7, 31, 0}, {8, 31, 0}, {31, 23, 0},
    {9, 11, 1}, {10, 12, 1}, {11, 13, 1}, {12, 14, 1}, {13, 15, 1}, {14, 16, 1},
    {15, 7, 1}, {16, 8, 1}, {9, 7, 0}, {10, 8, 0}, {9, 13, 0}, {10, 14, 0},
    {7, 17, 1}, {8, 18, 1}, {17, 19, 1}, {18, 20, 1}, {19, 21, 1}, {20, 22, 1},
    {17, 21, 0}, {18, 22, 0}, {21, 23, 1}, {22, 23, 1},
    {17, 24, 1}, {18, 25, 1}, {24, 26, 1}, {25, 27, 1}, {26, 21, 1}, {27, 22, 1},
}};

int projectX(int centerX, float worldX, float z)
{
    return centerX + static_cast<int>(kTerrainFocalLength * worldX / z);
}

int projectY(float worldY, float z)
{
    return kHorizonY - static_cast<int>(kTerrainVerticalScale * worldY / z);
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
    auto& canvas = GetHAL().getCanvas();
    const int centerX = _width / 2;
    const uint16_t terrainPrimary = display.color565(82, 238, 157);
    const uint16_t terrainMid = display.color565(42, 156, 99);
    const uint16_t terrainSecondary = display.color565(22, 88, 57);
    const uint16_t shipColor = display.color565(214, 244, 255);
    const uint16_t shipDim = display.color565(52, 112, 136);
    const uint16_t canopyColor = display.color565(74, 224, 255);
    const uint16_t exhaust = display.color565(255, 166, 68);
    const uint16_t hudColor = display.color565(120, 188, 170);
    const uint16_t caution = display.color565(255, 156, 62);

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
        for (std::size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            const float height = behind.surfaceHeights[column] +
                                 (ahead.surfaceHeights[column] - behind.surfaceHeights[column]) * blend;
            rows[rowCount].x[column] =
                projectX(centerX, TerrainStream::columnX(column) - flight.lateralOffset, kTerrainNearPlane);
            rows[rowCount].y[column] = projectY(height - flight.altitude, kTerrainNearPlane);
        }
        sourceRows[rowCount] = firstVisible;
        ++rowCount;
    }

    for (std::size_t row = firstVisible; row < slices.size(); ++row) {
        for (std::size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            rows[rowCount].x[column] =
                projectX(centerX, TerrainStream::columnX(column) - flight.lateralOffset, slices[row].z);
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
        for (std::size_t column = 1; column < TerrainStream::kColumnCount; ++column) {
            canvas.drawLine(rows[row].x[column - 1], rows[row].y[column - 1], rows[row].x[column],
                            rows[row].y[column], rowColor(row));
        }
    };

    if (rowCount == 0) return;
    drawRow(rowCount - 1);
    for (std::size_t farRow = rowCount - 1; farRow > 0; --farRow) {
        const std::size_t nearRow = farRow - 1;
        drawRow(nearRow);
        for (std::size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            canvas.drawLine(rows[nearRow].x[column], rows[nearRow].y[column], rows[farRow].x[column],
                            rows[farRow].y[column], rowColor(nearRow));
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

    const float exhaustTailZ = -2.72f - 0.70f * flight.boostAmount;
    const std::array<Vec3, 4> exhaustPoints = {{
        {-0.72f, -0.30f, -1.82f}, {-0.72f, -0.30f, exhaustTailZ},
        {0.72f, -0.30f, -1.82f}, {0.72f, -0.30f, exhaustTailZ},
    }};
    for (int engine = 0; engine < 2; ++engine) {
        const auto from = projectShip(exhaustPoints[engine * 2]);
        const auto to = projectShip(exhaustPoints[engine * 2 + 1]);
        canvas.drawLine(from[0], from[1], to[0], to[1], exhaust);
    }

    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setCursor(52, 52);
    canvas.print("VECTOR RUN");
    canvas.setCursor(_width - 108, 52);
    canvas.print("MAP SCAN");

    canvas.drawLine(vanishingX - 18, kHorizonY, vanishingX - 7, kHorizonY, hudColor);
    canvas.drawLine(vanishingX + 7, kHorizonY, vanishingX + 18, kHorizonY, hudColor);
    canvas.drawLine(vanishingX, kHorizonY - 12, vanishingX, kHorizonY - 5, hudColor);
    canvas.drawLine(vanishingX, kHorizonY + 5, vanishingX, kHorizonY + 12, hudColor);

    const int speedSegments = std::clamp(static_cast<int>((flight.speed - 42.0f) / 15.0f), 0, 6);
    canvas.setCursor(42, _height - 83);
    canvas.print("SPD");
    canvas.setCursor(42, _height - 68);
    canvas.printf("%03d", static_cast<int>(flight.speed));
    for (int segment = 0; segment < 6; ++segment) {
        const int y = _height - 48 + segment * 6;
        canvas.drawLine(43, y, 43 + (segment < speedSegments ? 15 : 7), y,
                        segment < speedSegments ? hudColor : terrainSecondary);
    }
    canvas.setCursor(72, _height - 68);
    canvas.print(flight.boostAmount > 0.0f ? "BOOST" : "CRUISE");

    canvas.setCursor(_width - 120, _height - 83);
    canvas.printf("ALT %+03d", static_cast<int>(flight.altitude * 10.0f));
    canvas.setCursor(_width - 120, _height - 68);
    canvas.printf("ROLL %+03d", static_cast<int>(flight.roll));
    canvas.setCursor(_width - 120, _height - 53);
    canvas.print(inputReady ? "INPUT IMU" : "IMU CAL");
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

    GetHAL().updateCanvas();
}

}  // namespace vector_canyon_fighter
