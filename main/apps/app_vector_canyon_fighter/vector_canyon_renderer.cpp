#include "vector_canyon_renderer.h"

#include <hal/hal.h>

#include <array>

namespace vector_canyon_fighter {
namespace {

constexpr int kHorizonY = 142;
constexpr int kShipCenterY = 335;
constexpr std::array<float, 8> kDepths = {1.0f, 1.35f, 1.8f, 2.4f, 3.2f, 4.3f, 5.8f, 7.8f};

struct CanyonRing {
    int leftX;
    int rightX;
    int floorY;
    int leftTopY;
    int rightTopY;
};

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

void Renderer::renderStaticScene()
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

    std::array<CanyonRing, kDepths.size()> rings = {};
    for (std::size_t i = 0; i < kDepths.size(); ++i) {
        const float depth = kDepths[i];
        const float inverseDepth = 1.0f / depth;
        const int halfWidth = static_cast<int>(18 + 184 * inverseDepth);
        const int floorY = static_cast<int>(kHorizonY + 225 * inverseDepth);
        const int leftTopY = static_cast<int>(kHorizonY - 12 + 60 * inverseDepth);
        const int rightTopY = static_cast<int>(kHorizonY - 6 + 52 * inverseDepth);
        rings[i] = {centerX - halfWidth, centerX + halfWidth, floorY, leftTopY, rightTopY};
    }

    for (std::size_t i = 0; i < rings.size(); ++i) {
        const auto& ring = rings[i];
        const uint16_t color = (i < 3) ? terrainSecondary : terrainPrimary;
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

    const int shipX = centerX;
    const int shipY = kShipCenterY;
    canvas.drawLine(shipX, shipY - 31, shipX - 31, shipY + 20, shipColor);
    canvas.drawLine(shipX, shipY - 31, shipX + 31, shipY + 20, shipColor);
    canvas.drawLine(shipX - 31, shipY + 20, shipX - 12, shipY + 28, shipColor);
    canvas.drawLine(shipX + 31, shipY + 20, shipX + 12, shipY + 28, shipColor);
    canvas.drawLine(shipX - 12, shipY + 28, shipX, shipY + 8, shipDim);
    canvas.drawLine(shipX + 12, shipY + 28, shipX, shipY + 8, shipDim);
    canvas.drawLine(shipX - 12, shipY + 28, shipX - 19, shipY + 43, shipColor);
    canvas.drawLine(shipX + 12, shipY + 28, shipX + 19, shipY + 43, shipColor);
    canvas.drawLine(shipX - 19, shipY + 43, shipX - 9, shipY + 48, shipDim);
    canvas.drawLine(shipX + 19, shipY + 43, shipX + 9, shipY + 48, shipDim);
    canvas.drawLine(shipX - 9, shipY + 48, shipX - 9, shipY + 60, exhaust);
    canvas.drawLine(shipX + 9, shipY + 48, shipX + 9, shipY + 60, exhaust);

    canvas.setTextColor(hudColor, TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setCursor(centerX - 34, 42);
    canvas.print("VECTOR RUN");
    canvas.setCursor(50, _height - 64);
    canvas.print("SPD 072");
    canvas.setCursor(50, _height - 46);
    canvas.print("CRUISE");
    canvas.setCursor(_width - 126, _height - 64);
    canvas.print("IMU READY");

    GetHAL().updateCanvas();
}

}  // namespace vector_canyon_fighter
