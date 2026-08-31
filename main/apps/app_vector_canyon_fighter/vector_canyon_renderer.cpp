#include "vector_canyon_renderer.h"

#include <hal/hal.h>

#include <array>
#include <cmath>

namespace vector_canyon_fighter {
namespace {

constexpr int kHorizonY = 142;
constexpr int kShipCenterY = 335;
constexpr float kFocalLength = 156.0f;

struct CanyonRing {
    int leftX;
    int rightX;
    int floorY;
    int leftTopY;
    int rightTopY;
};

int projectX(int centerX, float worldX, float z)
{
    return centerX + static_cast<int>(kFocalLength * worldX / z);
}

int projectY(float worldY, float z)
{
    return kHorizonY - static_cast<int>(kFocalLength * worldY / z);
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

void Renderer::render(const FlightState& flight, const TerrainStream& terrain, bool inputReady)
{
    if (_width <= 0 || _height <= 0) return;

    auto& display = GetHAL().getDisplay();
    auto& canvas = GetHAL().getCanvas();
    const int centerX = _width / 2;
    const uint16_t terrainPrimary = display.color565(75, 214, 155);
    const uint16_t terrainSecondary = display.color565(24, 91, 70);
    const uint16_t shipColor = display.color565(205, 238, 255);
    const uint16_t shipDim = display.color565(49, 93, 116);
    const uint16_t exhaust = display.color565(255, 156, 62);
    const uint16_t hudColor = display.color565(138, 198, 185);

    canvas.fillScreen(TFT_BLACK);

    std::array<CanyonRing, TerrainStream::kSliceCount> rings = {};
    const auto& slices = terrain.slices();
    for (std::size_t i = 0; i < slices.size(); ++i) {
        const auto& slice = slices[i];
        const float left = slice.center - slice.halfWidth - flight.lateralOffset;
        const float right = slice.center + slice.halfWidth - flight.lateralOffset;
        rings[i] = {
            projectX(centerX, left, slice.z),
            projectX(centerX, right, slice.z),
            projectY(slice.floor - flight.altitude, slice.z),
            projectY(slice.leftWall - flight.altitude, slice.z),
            projectY(slice.rightWall - flight.altitude, slice.z),
        };
    }

    for (std::size_t i = 0; i < rings.size(); ++i) {
        const auto& ring = rings[i];
        const uint16_t color = (i > 4) ? terrainSecondary : terrainPrimary;
        canvas.drawLine(ring.leftX, ring.floorY, ring.rightX, ring.floorY, color);
        canvas.drawLine(ring.leftX, ring.floorY, ring.leftX, ring.leftTopY, color);
        canvas.drawLine(ring.rightX, ring.floorY, ring.rightX, ring.rightTopY, color);

        if (i == 0) continue;
        const auto& previous = rings[i - 1];
        canvas.drawLine(previous.leftX, previous.floorY, ring.leftX, ring.floorY, color);
        canvas.drawLine(previous.rightX, previous.floorY, ring.rightX, ring.floorY, color);
        canvas.drawLine(previous.leftX, previous.leftTopY, ring.leftX, ring.leftTopY, color);
        canvas.drawLine(previous.rightX, previous.rightTopY, ring.rightX, ring.rightTopY, color);
    }

    const int shipX = centerX + static_cast<int>(flight.lateralOffset * 12.0f);
    const int shipY = kShipCenterY - static_cast<int>(flight.pitch * 0.7f);
    const float radians = flight.roll * 0.0174532925f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const auto drawShipLine = [&](int x1, int y1, int x2, int y2, uint16_t color) {
        const auto rotateX = [&](int x, int y) { return shipX + static_cast<int>(x * cosine - y * sine); };
        const auto rotateY = [&](int x, int y) { return shipY + static_cast<int>(x * sine + y * cosine); };
        canvas.drawLine(rotateX(x1, y1), rotateY(x1, y1), rotateX(x2, y2), rotateY(x2, y2), color);
    };
    drawShipLine(0, -31, -31, 20, shipColor);
    drawShipLine(0, -31, 31, 20, shipColor);
    drawShipLine(-31, 20, -12, 28, shipColor);
    drawShipLine(31, 20, 12, 28, shipColor);
    drawShipLine(-12, 28, 0, 8, shipDim);
    drawShipLine(12, 28, 0, 8, shipDim);
    drawShipLine(-12, 28, -19, 43, shipColor);
    drawShipLine(12, 28, 19, 43, shipColor);
    drawShipLine(-19, 43, -9, 48, shipDim);
    drawShipLine(19, 43, 9, 48, shipDim);
    const int exhaustLength = 12 + static_cast<int>(18.0f * flight.boostAmount);
    drawShipLine(-9, 48, -9, 48 + exhaustLength, exhaust);
    drawShipLine(9, 48, 9, 48 + exhaustLength, exhaust);

    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setCursor(centerX - 34, 42);
    canvas.print("VECTOR RUN");
    canvas.setCursor(50, _height - 64);
    canvas.printf("SPD %03d", static_cast<int>(flight.speed));
    canvas.setCursor(50, _height - 46);
    canvas.print(flight.boostAmount > 0.0f ? "BOOST" : "CRUISE");
    canvas.setCursor(_width - 126, _height - 64);
    canvas.print(inputReady ? "IMU READY" : "IMU CAL");

    GetHAL().updateCanvas();
}

}  // namespace vector_canyon_fighter
