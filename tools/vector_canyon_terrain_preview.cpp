#include "../main/apps/app_vector_canyon_fighter/model/terrain_stream.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>

namespace {

constexpr int kWidth = 450;
constexpr int kHeight = 450;
constexpr int kCenterX = kWidth / 2;
constexpr int kHorizonY = 150;
constexpr float kFocalLength = 112.0f;
constexpr float kVerticalScale = 94.0f;
constexpr float kNearPlane = 0.22f;

struct Pixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

using Image = std::array<Pixel, kWidth * kHeight>;

void putPixel(Image& image, int x, int y, Pixel color)
{
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    image[static_cast<size_t>(y) * kWidth + static_cast<size_t>(x)] = color;
}

void drawLine(Image& image, int x0, int y0, int x1, int y1, Pixel color)
{
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        putPixel(image, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int twiceError = error * 2;
        if (twiceError >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twiceError <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

int projectX(float x, float z)
{
    return kCenterX + static_cast<int>(kFocalLength * x / z);
}

int projectY(float height, float z)
{
    return kHorizonY - static_cast<int>(kVerticalScale * height / z);
}

size_t columnStride(size_t sourceRow)
{
    if (sourceRow > 17) return 3;
    if (sourceRow > 7) return 2;
    return 1;
}

void render(const vector_canyon_fighter::TerrainStream& terrain, const std::string& path)
{
    using vector_canyon_fighter::TerrainStream;
    Image image{};
    std::array<std::array<int16_t, TerrainStream::kColumnCount>, TerrainStream::kSliceCount + 1> xs{};
    std::array<std::array<int16_t, TerrainStream::kColumnCount>, TerrainStream::kSliceCount + 1> ys{};
    std::array<size_t, TerrainStream::kSliceCount + 1> sourceRows{};
    const auto& slices = terrain.slices();
    size_t rowCount = 0;

    size_t firstVisible = 0;
    while (firstVisible < slices.size() && slices[firstVisible].z < kNearPlane) ++firstVisible;

    if (firstVisible > 0 && firstVisible < slices.size()) {
        const auto& behind = slices[firstVisible - 1];
        const auto& ahead = slices[firstVisible];
        const float blend = std::clamp((kNearPlane - behind.z) / (ahead.z - behind.z), 0.0f, 1.0f);
        const float center = behind.center + (ahead.center - behind.center) * blend;
        for (size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            const float height = behind.surfaceHeights[column] +
                                 (ahead.surfaceHeights[column] - behind.surfaceHeights[column]) * blend;
            xs[rowCount][column] =
                static_cast<int16_t>(projectX(TerrainStream::columnWorldX(center, column), kNearPlane));
            ys[rowCount][column] = static_cast<int16_t>(projectY(height, kNearPlane));
        }
        sourceRows[rowCount] = firstVisible;
        ++rowCount;
    }

    for (size_t row = firstVisible; row < slices.size(); ++row) {
        for (size_t column = 0; column < TerrainStream::kColumnCount; ++column) {
            xs[rowCount][column] = static_cast<int16_t>(
                projectX(TerrainStream::columnWorldX(slices[row].center, column), slices[row].z));
            ys[rowCount][column] =
                static_cast<int16_t>(projectY(slices[row].surfaceHeights[column], slices[row].z));
        }
        sourceRows[rowCount] = row;
        ++rowCount;
    }

    auto rowColor = [&](size_t row) {
        return sourceRows[row] > 17 ? Pixel{20, 82, 55} :
               sourceRows[row] > 7  ? Pixel{40, 154, 98} : Pixel{82, 238, 157};
    };
    auto drawRow = [&](size_t row) {
        const size_t stride = columnStride(sourceRows[row]);
        size_t previous = 0;
        const Pixel color = rowColor(row);
        for (size_t column = stride; column < TerrainStream::kColumnCount; column += stride) {
            drawLine(image, xs[row][previous], ys[row][previous], xs[row][column], ys[row][column], color);
            previous = column;
        }
        const size_t last = TerrainStream::kColumnCount - 1;
        if (previous != last) {
            drawLine(image, xs[row][previous], ys[row][previous], xs[row][last], ys[row][last], color);
        }
    };

    if (rowCount == 0) return;
    drawRow(rowCount - 1);
    for (size_t farRow = rowCount - 1; farRow > 0; --farRow) {
        const size_t nearRow = farRow - 1;
        drawRow(nearRow);
        const Pixel color = rowColor(nearRow);
        const size_t stride = std::max(columnStride(sourceRows[nearRow]), columnStride(sourceRows[farRow]));
        const size_t last = TerrainStream::kColumnCount - 1;
        for (size_t column = 0; column < TerrainStream::kColumnCount; column += stride) {
            drawLine(image, xs[nearRow][column], ys[nearRow][column], xs[farRow][column], ys[farRow][column],
                     color);
        }
        if (last % stride != 0) {
            drawLine(image, xs[nearRow][last], ys[nearRow][last], xs[farRow][last], ys[farRow][last], color);
        }
    }

    const int radius = kWidth / 2;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const int dx = x - kCenterX;
            const int dy = y - kHeight / 2;
            if (dx * dx + dy * dy > radius * radius) image[static_cast<size_t>(y) * kWidth + x] = {};
        }
    }

    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kWidth << ' ' << kHeight << "\n255\n";
    output.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size() * 3));
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    vector_canyon_fighter::TerrainStream terrain;
    terrain.reset(0xC4A71001u);
    render(terrain, std::string(argv[1]) + "/terrain-r1-a.ppm");
    terrain.update(240.0f);
    render(terrain, std::string(argv[1]) + "/terrain-r1-b.ppm");
    terrain.update(480.0f);
    render(terrain, std::string(argv[1]) + "/terrain-r1-c.ppm");
    return 0;
}
