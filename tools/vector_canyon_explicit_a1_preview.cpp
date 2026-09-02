#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <string>

namespace {

constexpr int kImageWidth = 450;
constexpr int kImageHeight = 450;
constexpr int kPrincipalX = 225;
constexpr int kPrincipalY = 156;
constexpr float kFocalLength = 372.0f;
constexpr float kNearPlane = 0.20f;
constexpr std::size_t kSliceCount = 34;
constexpr std::size_t kProfileCount = 25;
constexpr float kFirstSliceZ = 1.35f;
constexpr float kSliceSpacing = 0.86f;
constexpr float kFloorHalfWidth = 2.18f;
constexpr float kWallHeight = 3.80f;

struct Pixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct ProfilePoint {
    float lateral = 0.0f;
    float height = 0.0f;
};

using Image = std::array<Pixel, kImageWidth * kImageHeight>;
using Profile = std::array<ProfilePoint, kProfileCount>;
using CanyonMesh = std::array<std::array<Vec3, kProfileCount>, kSliceCount>;

struct CurveSample {
    Vec2 center;
    Vec2 tangent;
    Vec2 normal;
    float arcLength = 0.0f;
};

using CurveSamples = std::array<CurveSample, kSliceCount>;

enum class CanyonSide : uint8_t {
    Left,
    Right,
};

struct ShoulderEvent {
    CanyonSide side = CanyonSide::Left;
    float centerArcLength = 0.0f;
    float halfLength = 1.0f;
    float amplitude = 0.0f;
};

struct CanyonBoundary {
    float leftWidth = kFloorHalfWidth;
    float rightWidth = kFloorHalfWidth;
    float leftIntrusion = 0.0f;
    float rightIntrusion = 0.0f;
};

using CanyonBoundaries = std::array<CanyonBoundary, kSliceCount>;

constexpr Pixel kNearColor{78, 228, 252};
constexpr Pixel kMidColor{31, 159, 202};
constexpr Pixel kFarColor{14, 75, 111};
constexpr Pixel kEdgeColor{119, 244, 255};
constexpr Pixel kGuideColor{25, 72, 91};
constexpr Pixel kAxisColor{92, 234, 150};
constexpr Pixel kWarningColor{255, 170, 70};

float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

Vec3 operator*(Vec3 value, float scale) { return {value.x * scale, value.y * scale, value.z * scale}; }

float length(Vec3 value) { return std::sqrt(dot(value, value)); }

Vec3 normalize(Vec3 value)
{
    const float magnitude = length(value);
    return magnitude > 0.00001f ? value * (1.0f / magnitude) : Vec3{};
}

Vec3 cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }

Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }

Vec2 operator*(Vec2 value, float scale) { return {value.x * scale, value.y * scale}; }

float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

float length(Vec2 value) { return std::sqrt(dot(value, value)); }

Vec2 normalize(Vec2 value)
{
    const float magnitude = length(value);
    return magnitude > 0.00001f ? value * (1.0f / magnitude) : Vec2{};
}

void putPixel(Image& image, int x, int y, Pixel color)
{
    if (x < 0 || x >= kImageWidth || y < 0 || y >= kImageHeight) return;
    image[static_cast<std::size_t>(y) * kImageWidth + static_cast<std::size_t>(x)] = color;
}

bool clipTest(float p, float q, float& t0, float& t1)
{
    if (std::abs(p) < 0.00001f) return q >= 0.0f;
    const float ratio = q / p;
    if (p < 0.0f) {
        if (ratio > t1) return false;
        t0 = std::max(t0, ratio);
    } else {
        if (ratio < t0) return false;
        t1 = std::min(t1, ratio);
    }
    return true;
}

bool clipToImage(float& x0, float& y0, float& x1, float& y1)
{
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    float t0 = 0.0f;
    float t1 = 1.0f;
    if (!clipTest(-dx, x0, t0, t1) || !clipTest(dx, static_cast<float>(kImageWidth - 1) - x0, t0, t1) ||
        !clipTest(-dy, y0, t0, t1) || !clipTest(dy, static_cast<float>(kImageHeight - 1) - y0, t0, t1)) {
        return false;
    }
    const float sourceX = x0;
    const float sourceY = y0;
    x0 = sourceX + t0 * dx;
    y0 = sourceY + t0 * dy;
    x1 = sourceX + t1 * dx;
    y1 = sourceY + t1 * dy;
    return true;
}

void drawLine(Image& image, float sourceX0, float sourceY0, float sourceX1, float sourceY1, Pixel color)
{
    if (!clipToImage(sourceX0, sourceY0, sourceX1, sourceY1)) return;
    int x0 = static_cast<int>(std::lround(sourceX0));
    int y0 = static_cast<int>(std::lround(sourceY0));
    const int x1 = static_cast<int>(std::lround(sourceX1));
    const int y1 = static_cast<int>(std::lround(sourceY1));
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

void drawPoint(Image& image, float x, float y, Pixel color)
{
    const int centerX = static_cast<int>(std::lround(x));
    const int centerY = static_cast<int>(std::lround(y));
    for (int offsetY = -2; offsetY <= 2; ++offsetY) {
        for (int offsetX = -2; offsetX <= 2; ++offsetX) {
            if (std::abs(offsetX) + std::abs(offsetY) <= 3) putPixel(image, centerX + offsetX, centerY + offsetY, color);
        }
    }
}

void applyRoundMask(Image& image)
{
    constexpr int kCenter = kImageWidth / 2;
    constexpr int kRadius = kImageWidth / 2;
    for (int y = 0; y < kImageHeight; ++y) {
        for (int x = 0; x < kImageWidth; ++x) {
            const int dx = x - kCenter;
            const int dy = y - kCenter;
            if (dx * dx + dy * dy > kRadius * kRadius) {
                image[static_cast<std::size_t>(y) * kImageWidth + static_cast<std::size_t>(x)] = {};
            }
        }
    }
}

void writePpm(const Image& image, const std::string& path)
{
    std::ofstream output(path, std::ios::binary);
    output << "P6\n" << kImageWidth << ' ' << kImageHeight << "\n255\n";
    output.write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size() * 3));
}

Profile makeProfile()
{
    // The profile is intentionally semantic and deterministic:
    // flat plateau -> short cap -> steep face -> short toe -> flat floor,
    // mirrored around the canyon center. No noise or ridge height field.
    return {{
        {-7.00f, kWallHeight}, {-5.20f, kWallHeight}, {-3.65f, kWallHeight},
        {-3.32f, kWallHeight}, {-3.04f, 3.34f}, {-2.67f, 1.06f}, {-2.38f, 0.38f},
        {-kFloorHalfWidth, 0.0f},
        {-1.744f, 0.0f}, {-1.308f, 0.0f}, {-0.872f, 0.0f}, {-0.436f, 0.0f},
        {0.0f, 0.0f},
        {0.436f, 0.0f}, {0.872f, 0.0f}, {1.308f, 0.0f}, {1.744f, 0.0f},
        {kFloorHalfWidth, 0.0f},
        {2.38f, 0.38f}, {2.67f, 1.06f}, {3.04f, 3.34f}, {3.32f, kWallHeight},
        {3.65f, kWallHeight}, {5.20f, kWallHeight}, {7.00f, kWallHeight},
    }};
}

bool validateProfile(const Profile& profile)
{
    constexpr float kEpsilon = 0.0001f;
    for (std::size_t point = 7; point <= 17; ++point) {
        if (std::abs(profile[point].height) > kEpsilon) return false;
    }
    for (std::size_t point = 0; point <= 3; ++point) {
        if (std::abs(profile[point].height - kWallHeight) > kEpsilon) return false;
    }
    for (std::size_t point = 21; point < profile.size(); ++point) {
        if (std::abs(profile[point].height - kWallHeight) > kEpsilon) return false;
    }
    for (std::size_t point = 0; point < profile.size(); ++point) {
        const std::size_t mirror = profile.size() - 1 - point;
        if (std::abs(profile[point].lateral + profile[mirror].lateral) > kEpsilon ||
            std::abs(profile[point].height - profile[mirror].height) > kEpsilon) {
            return false;
        }
    }
    const float mainFaceRise = profile[4].height - profile[5].height;
    const float mainFaceRetreat = std::abs(profile[4].lateral - profile[5].lateral);
    const float mainFaceDegrees = std::atan2(mainFaceRise, mainFaceRetreat) * 57.2957795f;
    return mainFaceDegrees >= 78.0f && mainFaceDegrees <= 85.0f;
}

CanyonMesh makeStraightMesh(const Profile& profile)
{
    CanyonMesh mesh{};
    for (std::size_t slice = 0; slice < kSliceCount; ++slice) {
        const float worldZ = kFirstSliceZ + static_cast<float>(slice) * kSliceSpacing;
        for (std::size_t point = 0; point < kProfileCount; ++point) {
            mesh[slice][point] = {profile[point].lateral, profile[point].height, worldZ};
        }
    }
    return mesh;
}

bool validateStraightMesh(const CanyonMesh& mesh)
{
    constexpr float kEpsilon = 0.0001f;
    for (std::size_t slice = 0; slice < mesh.size(); ++slice) {
        const float expectedZ = kFirstSliceZ + static_cast<float>(slice) * kSliceSpacing;
        for (std::size_t point = 0; point < kProfileCount; ++point) {
            if (std::abs(mesh[slice][point].z - expectedZ) > kEpsilon) return false;
        }
        if (std::abs(mesh[slice][12].x) > kEpsilon || std::abs(mesh[slice][12].y) > kEpsilon) return false;
    }
    return true;
}

float centripetalKnot(float previousKnot, Vec2 from, Vec2 to)
{
    return previousKnot + std::sqrt(std::max(length(to - from), 0.00001f));
}

Vec2 timedBlend(Vec2 from, Vec2 to, float fromKnot, float toKnot, float knot)
{
    const float denominator = std::max(toKnot - fromKnot, 0.00001f);
    const float weight = (knot - fromKnot) / denominator;
    return from * (1.0f - weight) + to * weight;
}

Vec2 centripetalCatmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float normalizedT)
{
    const float t0 = 0.0f;
    const float t1 = centripetalKnot(t0, p0, p1);
    const float t2 = centripetalKnot(t1, p1, p2);
    const float t3 = centripetalKnot(t2, p2, p3);
    const float t = t1 + std::clamp(normalizedT, 0.0f, 1.0f) * (t2 - t1);
    const Vec2 a1 = timedBlend(p0, p1, t0, t1, t);
    const Vec2 a2 = timedBlend(p1, p2, t1, t2, t);
    const Vec2 a3 = timedBlend(p2, p3, t2, t3, t);
    const Vec2 b1 = timedBlend(a1, a2, t0, t2, t);
    const Vec2 b2 = timedBlend(a2, a3, t1, t3, t);
    return timedBlend(b1, b2, t1, t2, t);
}

CurveSamples makeCurvedSamples()
{
    constexpr std::array<Vec2, 11> kControls = {{
        {-0.30f, -2.5f}, {0.00f, 2.0f}, {0.72f, 6.5f}, {0.30f, 11.0f},
        {-1.00f, 15.5f}, {-1.42f, 20.0f}, {-0.18f, 24.5f}, {1.36f, 29.0f},
        {1.82f, 33.5f}, {0.78f, 38.0f}, {-0.42f, 42.5f},
    }};
    constexpr std::size_t kSamplesPerSegment = 48;
    constexpr std::size_t kDenseCapacity = 512;
    std::array<Vec2, kDenseCapacity> dense{};
    std::array<float, kDenseCapacity> cumulative{};
    std::size_t denseCount = 0;

    for (std::size_t segment = 0; segment + 3 < kControls.size(); ++segment) {
        for (std::size_t step = 0; step <= kSamplesPerSegment; ++step) {
            if (segment > 0 && step == 0) continue;
            const float t = static_cast<float>(step) / static_cast<float>(kSamplesPerSegment);
            dense[denseCount++] = centripetalCatmullRom(kControls[segment], kControls[segment + 1],
                                                        kControls[segment + 2], kControls[segment + 3], t);
        }
    }

    cumulative[0] = 0.0f;
    for (std::size_t point = 1; point < denseCount; ++point) {
        cumulative[point] = cumulative[point - 1] + length(dense[point] - dense[point - 1]);
    }

    CurveSamples samples{};
    const float totalLength = cumulative[denseCount - 1];
    std::size_t denseCursor = 1;
    for (std::size_t sample = 0; sample < samples.size(); ++sample) {
        const float target = totalLength * static_cast<float>(sample) / static_cast<float>(samples.size() - 1);
        while (denseCursor + 1 < denseCount && cumulative[denseCursor] < target) ++denseCursor;
        const float segmentLength = std::max(cumulative[denseCursor] - cumulative[denseCursor - 1], 0.00001f);
        const float blend = (target - cumulative[denseCursor - 1]) / segmentLength;
        samples[sample].center = dense[denseCursor - 1] * (1.0f - blend) + dense[denseCursor] * blend;
        samples[sample].arcLength = target;
    }

    for (std::size_t sample = 0; sample < samples.size(); ++sample) {
        const Vec2 previous = samples[sample == 0 ? sample : sample - 1].center;
        const Vec2 next = samples[sample + 1 < samples.size() ? sample + 1 : sample].center;
        samples[sample].tangent = normalize(next - previous);
        samples[sample].normal = {samples[sample].tangent.y, -samples[sample].tangent.x};
    }
    return samples;
}

CanyonMesh makeCurvedMesh(const Profile& profile, const CurveSamples& samples)
{
    CanyonMesh mesh{};
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        for (std::size_t point = 0; point < profile.size(); ++point) {
            const Vec2 ground = samples[slice].center + samples[slice].normal * profile[point].lateral;
            mesh[slice][point] = {ground.x, profile[point].height, ground.y};
        }
    }
    return mesh;
}

float compactShoulderBump(float normalizedDistance)
{
    const float x = std::clamp(1.0f - std::abs(normalizedDistance), 0.0f, 1.0f);
    return x * x * x * (10.0f + x * (-15.0f + 6.0f * x));
}

CanyonBoundaries makeBoundaries(const CurveSamples& samples, const ShoulderEvent& event)
{
    CanyonBoundaries boundaries{};
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        const float normalizedDistance = (samples[slice].arcLength - event.centerArcLength) / event.halfLength;
        const float intrusion = event.amplitude * compactShoulderBump(normalizedDistance);
        if (event.side == CanyonSide::Left) {
            boundaries[slice].leftIntrusion = intrusion;
            boundaries[slice].leftWidth -= intrusion;
        } else {
            boundaries[slice].rightIntrusion = intrusion;
            boundaries[slice].rightWidth -= intrusion;
        }
    }
    return boundaries;
}

float deformedLateral(const Profile& profile, std::size_t point, const CanyonBoundary& boundary)
{
    if (point < 7) return profile[point].lateral + boundary.leftIntrusion;
    if (point > 17) return profile[point].lateral - boundary.rightIntrusion;
    if (point <= 12) return profile[point].lateral * boundary.leftWidth / kFloorHalfWidth;
    return profile[point].lateral * boundary.rightWidth / kFloorHalfWidth;
}

Profile deformProfile(const Profile& profile, const CanyonBoundary& boundary)
{
    Profile deformed = profile;
    for (std::size_t point = 0; point < profile.size(); ++point) {
        deformed[point].lateral = deformedLateral(profile, point, boundary);
    }
    return deformed;
}

CanyonMesh makeShoulderMesh(const Profile& profile, const CurveSamples& samples,
                            const CanyonBoundaries& boundaries)
{
    CanyonMesh mesh{};
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        for (std::size_t point = 0; point < profile.size(); ++point) {
            const float lateral = deformedLateral(profile, point, boundaries[slice]);
            const Vec2 ground = samples[slice].center + samples[slice].normal * lateral;
            mesh[slice][point] = {ground.x, profile[point].height, ground.y};
        }
    }
    return mesh;
}

bool validateCurvedMesh(const CanyonMesh& mesh, const CurveSamples& samples)
{
    constexpr float kEpsilon = 0.001f;
    const float targetSpacing = samples.back().arcLength / static_cast<float>(samples.size() - 1);
    float minCenterX = samples.front().center.x;
    float maxCenterX = samples.front().center.x;
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        minCenterX = std::min(minCenterX, samples[slice].center.x);
        maxCenterX = std::max(maxCenterX, samples[slice].center.x);
        if (std::abs(dot(samples[slice].tangent, samples[slice].normal)) > kEpsilon) return false;
        if (std::abs(length(samples[slice].tangent) - 1.0f) > kEpsilon ||
            std::abs(length(samples[slice].normal) - 1.0f) > kEpsilon) {
            return false;
        }
        if (slice > 0) {
            const float spacing = length(samples[slice].center - samples[slice - 1].center);
            if (std::abs(spacing - targetSpacing) > 0.02f) return false;
            if (samples[slice].center.y <= samples[slice - 1].center.y) return false;
            const float tangentDot = std::clamp(dot(samples[slice - 1].tangent, samples[slice].tangent), -1.0f, 1.0f);
            const float curvature = std::acos(tangentDot) / std::max(spacing, 0.0001f);
            if (curvature * 7.0f >= 0.95f) return false;

            for (const std::size_t rail : {std::size_t{0}, std::size_t{24}}) {
                const Vec2 railDelta{mesh[slice][rail].x - mesh[slice - 1][rail].x,
                                     mesh[slice][rail].z - mesh[slice - 1][rail].z};
                const Vec2 averageTangent = normalize(samples[slice - 1].tangent + samples[slice].tangent);
                if (dot(railDelta, averageTangent) <= 0.0f) return false;
            }
        }
    }
    return maxCenterX - minCenterX >= 2.5f;
}

float meshLateral(const CanyonMesh& mesh, const CurveSamples& samples, std::size_t slice, std::size_t point)
{
    const Vec2 delta{mesh[slice][point].x - samples[slice].center.x,
                     mesh[slice][point].z - samples[slice].center.y};
    return dot(delta, samples[slice].normal);
}

bool validateShoulderMesh(const Profile& profile, const CanyonMesh& mesh, const CurveSamples& samples,
                          const CanyonBoundaries& boundaries, const ShoulderEvent& event)
{
    constexpr float kEpsilon = 0.001f;
    constexpr float kShipFullWidthWithWarnings = 1.44f;
    float maximumIntrusion = 0.0f;
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        const CanyonBoundary& boundary = boundaries[slice];
        const float activeIntrusion = event.side == CanyonSide::Left ? boundary.leftIntrusion : boundary.rightIntrusion;
        maximumIntrusion = std::max(maximumIntrusion, activeIntrusion);
        if (boundary.leftWidth + boundary.rightWidth <= kShipFullWidthWithWarnings) return false;
        if (event.side == CanyonSide::Left &&
            (std::abs(boundary.rightIntrusion) > kEpsilon || std::abs(boundary.rightWidth - kFloorHalfWidth) > kEpsilon)) {
            return false;
        }
        if (event.side == CanyonSide::Right &&
            (std::abs(boundary.leftIntrusion) > kEpsilon || std::abs(boundary.leftWidth - kFloorHalfWidth) > kEpsilon)) {
            return false;
        }

        const float normalizedDistance = (samples[slice].arcLength - event.centerArcLength) / event.halfLength;
        if (std::abs(normalizedDistance) >= 1.0f && activeIntrusion != 0.0f) return false;
        for (std::size_t point = 0; point < profile.size(); ++point) {
            const float expectedLateral = deformedLateral(profile, point, boundary);
            if (std::abs(meshLateral(mesh, samples, slice, point) - expectedLateral) > kEpsilon ||
                std::abs(mesh[slice][point].y - profile[point].height) > kEpsilon) {
                return false;
            }
        }

        const float leftEdge = meshLateral(mesh, samples, slice, 7);
        const float rightEdge = meshLateral(mesh, samples, slice, 17);
        if (std::abs(leftEdge + boundary.leftWidth) > kEpsilon ||
            std::abs(rightEdge - boundary.rightWidth) > kEpsilon) {
            return false;
        }
        for (std::size_t point = 0; point < 7; ++point) {
            const float expectedOffset = profile[point].lateral - profile[7].lateral;
            if (std::abs((meshLateral(mesh, samples, slice, point) - leftEdge) - expectedOffset) > kEpsilon) {
                return false;
            }
        }
        for (std::size_t point = 18; point < profile.size(); ++point) {
            const float expectedOffset = profile[point].lateral - profile[17].lateral;
            if (std::abs((meshLateral(mesh, samples, slice, point) - rightEdge) - expectedOffset) > kEpsilon) {
                return false;
            }
        }

        if (slice > 0) {
            if (std::abs(boundary.leftWidth - boundaries[slice - 1].leftWidth) > 0.40f ||
                std::abs(boundary.rightWidth - boundaries[slice - 1].rightWidth) > 0.40f) {
                return false;
            }
            const Vec2 averageTangent = normalize(samples[slice - 1].tangent + samples[slice].tangent);
            for (const std::size_t rail : {std::size_t{0}, std::size_t{24}}) {
                const Vec2 railDelta{mesh[slice][rail].x - mesh[slice - 1][rail].x,
                                     mesh[slice][rail].z - mesh[slice - 1][rail].z};
                if (dot(railDelta, averageTangent) <= 0.0f) return false;
            }
        }
    }
    return std::abs(maximumIntrusion - event.amplitude) < kEpsilon &&
           std::abs(boundaries.front().leftWidth - kFloorHalfWidth) < kEpsilon &&
           std::abs(boundaries.front().rightWidth - kFloorHalfWidth) < kEpsilon &&
           std::abs(boundaries.back().leftWidth - kFloorHalfWidth) < kEpsilon &&
           std::abs(boundaries.back().rightWidth - kFloorHalfWidth) < kEpsilon;
}

void appendShoulderMetrics(std::ofstream& output, const char* name, const Profile& profile, const CanyonMesh& mesh,
                           const CurveSamples& samples, const CanyonBoundaries& boundaries,
                           const ShoulderEvent& event)
{
    float minimumWidth = 1000.0f;
    float minimumLeftWidth = 1000.0f;
    float minimumRightWidth = 1000.0f;
    float maximumBoundaryStep = 0.0f;
    float maximumCollisionMismatch = 0.0f;
    float maximumRigidWallError = 0.0f;
    float maximumIntrusion = 0.0f;
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        const CanyonBoundary& boundary = boundaries[slice];
        minimumLeftWidth = std::min(minimumLeftWidth, boundary.leftWidth);
        minimumRightWidth = std::min(minimumRightWidth, boundary.rightWidth);
        minimumWidth = std::min(minimumWidth, boundary.leftWidth + boundary.rightWidth);
        maximumIntrusion = std::max(maximumIntrusion,
                                    event.side == CanyonSide::Left ? boundary.leftIntrusion : boundary.rightIntrusion);
        const float leftEdge = meshLateral(mesh, samples, slice, 7);
        const float rightEdge = meshLateral(mesh, samples, slice, 17);
        maximumCollisionMismatch =
            std::max(maximumCollisionMismatch,
                     std::max(std::abs(leftEdge + boundary.leftWidth), std::abs(rightEdge - boundary.rightWidth)));
        for (std::size_t point = 0; point < 7; ++point) {
            const float rigidError = std::abs((meshLateral(mesh, samples, slice, point) - leftEdge) -
                                              (profile[point].lateral - profile[7].lateral));
            maximumRigidWallError = std::max(maximumRigidWallError, rigidError);
        }
        for (std::size_t point = 18; point < profile.size(); ++point) {
            const float rigidError = std::abs((meshLateral(mesh, samples, slice, point) - rightEdge) -
                                              (profile[point].lateral - profile[17].lateral));
            maximumRigidWallError = std::max(maximumRigidWallError, rigidError);
        }
        if (slice > 0) {
            maximumBoundaryStep =
                std::max(maximumBoundaryStep,
                         std::max(std::abs(boundary.leftWidth - boundaries[slice - 1].leftWidth),
                                  std::abs(boundary.rightWidth - boundaries[slice - 1].rightWidth)));
        }
    }

    output << name << ".event_center_arc=" << event.centerArcLength << '\n';
    output << name << ".event_half_length=" << event.halfLength << '\n';
    output << name << ".event_amplitude=" << event.amplitude << '\n';
    output << name << ".maximum_sampled_intrusion=" << maximumIntrusion << '\n';
    output << name << ".minimum_left_width=" << minimumLeftWidth << '\n';
    output << name << ".minimum_right_width=" << minimumRightWidth << '\n';
    output << name << ".minimum_total_width=" << minimumWidth << '\n';
    output << name << ".maximum_boundary_step=" << maximumBoundaryStep << '\n';
    output << name << ".maximum_collision_edge_mismatch=" << maximumCollisionMismatch << '\n';
    output << name << ".maximum_rigid_wall_error=" << maximumRigidWallError << '\n';
}

void writeCurveMetrics(const CurveSamples& samples, const std::string& path)
{
    float minimumSpacing = 1000.0f;
    float maximumSpacing = 0.0f;
    float maximumFrameDot = 0.0f;
    float maximumCurvatureOffsetProduct = 0.0f;
    float minimumCenterX = samples.front().center.x;
    float maximumCenterX = samples.front().center.x;
    for (std::size_t slice = 0; slice < samples.size(); ++slice) {
        minimumCenterX = std::min(minimumCenterX, samples[slice].center.x);
        maximumCenterX = std::max(maximumCenterX, samples[slice].center.x);
        maximumFrameDot = std::max(maximumFrameDot, std::abs(dot(samples[slice].tangent, samples[slice].normal)));
        if (slice == 0) continue;
        const float spacing = length(samples[slice].center - samples[slice - 1].center);
        minimumSpacing = std::min(minimumSpacing, spacing);
        maximumSpacing = std::max(maximumSpacing, spacing);
        const float tangentDot = std::clamp(dot(samples[slice - 1].tangent, samples[slice].tangent), -1.0f, 1.0f);
        const float curvature = std::acos(tangentDot) / std::max(spacing, 0.0001f);
        maximumCurvatureOffsetProduct = std::max(maximumCurvatureOffsetProduct, curvature * 7.0f);
    }

    std::ofstream output(path);
    output << std::fixed << std::setprecision(6);
    output << "slice_count=" << samples.size() << '\n';
    output << "arc_length=" << samples.back().arcLength << '\n';
    output << "target_arc_spacing=" << samples.back().arcLength / static_cast<float>(samples.size() - 1) << '\n';
    output << "minimum_chord_spacing=" << minimumSpacing << '\n';
    output << "maximum_chord_spacing=" << maximumSpacing << '\n';
    output << "maximum_abs_dot_t_n=" << maximumFrameDot << '\n';
    output << "maximum_curvature_outer_offset_product=" << maximumCurvatureOffsetProduct << '\n';
    output << "centerline_lateral_span=" << maximumCenterX - minimumCenterX << '\n';
}

void writeShoulderMetrics(const Profile& profile, const CanyonMesh& leftMesh, const CanyonMesh& rightMesh,
                          const CurveSamples& samples, const CanyonBoundaries& leftBoundaries,
                          const CanyonBoundaries& rightBoundaries, const ShoulderEvent& leftEvent,
                          const ShoulderEvent& rightEvent, const std::string& path)
{
    std::ofstream output(path);
    output << std::fixed << std::setprecision(6);
    output << "ship_full_width_with_warning_margins=1.440000\n";
    appendShoulderMetrics(output, "left", profile, leftMesh, samples, leftBoundaries, leftEvent);
    appendShoulderMetrics(output, "right", profile, rightMesh, samples, rightBoundaries, rightEvent);
}

struct Camera {
    Vec3 position;
    Vec3 right;
    Vec3 up;
    Vec3 forward;
};

Camera makeCamera()
{
    Camera camera{};
    camera.position = {0.0f, 0.95f, 0.0f};
    camera.forward = normalize(Vec3{0.0f, 0.20f, 14.0f} - camera.position);
    camera.right = normalize(cross(Vec3{0.0f, 1.0f, 0.0f}, camera.forward));
    camera.up = normalize(cross(camera.forward, camera.right));
    return camera;
}

bool project(const Camera& camera, Vec3 world, Vec2& screen)
{
    const Vec3 relative = world - camera.position;
    const float cameraX = dot(relative, camera.right);
    const float cameraY = dot(relative, camera.up);
    const float cameraZ = dot(relative, camera.forward);
    if (cameraZ < kNearPlane) return false;
    screen.x = static_cast<float>(kPrincipalX) + kFocalLength * cameraX / cameraZ;
    screen.y = static_cast<float>(kPrincipalY) - kFocalLength * cameraY / cameraZ;
    return true;
}

Pixel depthColor(std::size_t slice)
{
    if (slice < 9) return kNearColor;
    if (slice < 21) return kMidColor;
    return kFarColor;
}

bool isStructuralRail(std::size_t profileIndex)
{
    return profileIndex == 3 || profileIndex == 7 || profileIndex == 12 || profileIndex == 17 ||
           profileIndex == 21;
}

void renderPerspective(const CanyonMesh& mesh, const std::string& path)
{
    Image image{};
    const Camera camera = makeCamera();
    std::array<std::array<Vec2, kProfileCount>, kSliceCount> projected{};
    std::array<std::array<bool, kProfileCount>, kSliceCount> visible{};

    for (std::size_t slice = 0; slice < kSliceCount; ++slice) {
        for (std::size_t point = 0; point < kProfileCount; ++point) {
            visible[slice][point] = project(camera, mesh[slice][point], projected[slice][point]);
        }
    }

    // Draw far to near. Each complete cross-section is a rib in the local N/Up plane.
    for (std::size_t reverse = kSliceCount; reverse > 0; --reverse) {
        const std::size_t slice = reverse - 1;
        const Pixel color = depthColor(slice);
        for (std::size_t point = 1; point < kProfileCount; ++point) {
            if (!visible[slice][point - 1] || !visible[slice][point]) continue;
            drawLine(image, projected[slice][point - 1].x, projected[slice][point - 1].y,
                     projected[slice][point].x, projected[slice][point].y, color);
        }
    }

    // Fixed semantic profile indices form longitudinal rails.
    for (std::size_t point = 0; point < kProfileCount; ++point) {
        for (std::size_t slice = 1; slice < kSliceCount; ++slice) {
            if (!visible[slice - 1][point] || !visible[slice][point]) continue;
            const Pixel color = isStructuralRail(point) ? kEdgeColor : depthColor(slice - 1);
            drawLine(image, projected[slice - 1][point].x, projected[slice - 1][point].y,
                     projected[slice][point].x, projected[slice][point].y, color);
        }
    }

    // Sparse, deterministic diagonals make the two cliff faces read as low-poly surfaces.
    constexpr std::array<std::size_t, 4> kFaceStarts = {3, 4, 18, 19};
    for (std::size_t slice = 0; slice + 1 < kSliceCount; slice += 2) {
        for (const std::size_t faceStart : kFaceStarts) {
            if (!visible[slice][faceStart] || !visible[slice + 1][faceStart + 1]) continue;
            drawLine(image, projected[slice][faceStart].x, projected[slice][faceStart].y,
                     projected[slice + 1][faceStart + 1].x, projected[slice + 1][faceStart + 1].y,
                     depthColor(slice));
        }
    }

    applyRoundMask(image);
    writePpm(image, path);
}

float topX(float worldX) { return 225.0f + worldX * 28.0f; }

float topY(float worldZ)
{
    const float lastZ = kFirstSliceZ + static_cast<float>(kSliceCount - 1) * kSliceSpacing;
    return 421.0f - (worldZ - kFirstSliceZ) * 390.0f / (lastZ - kFirstSliceZ);
}

void renderTopDebug(const CanyonMesh& mesh, const std::string& path)
{
    Image image{};
    constexpr std::array<std::size_t, 7> kSemanticRails = {0, 3, 7, 12, 17, 21, 24};
    for (const std::size_t point : kSemanticRails) {
        const Pixel color = point == 12 ? kAxisColor : ((point == 7 || point == 17) ? kEdgeColor : kMidColor);
        for (std::size_t slice = 1; slice < kSliceCount; ++slice) {
            drawLine(image, topX(mesh[slice - 1][point].x), topY(mesh[slice - 1][point].z),
                     topX(mesh[slice][point].x), topY(mesh[slice][point].z), color);
        }
    }

    // Cross-section normals: every fourth rib spans the complete explicit profile.
    for (std::size_t slice = 0; slice < kSliceCount; slice += 4) {
        drawLine(image, topX(mesh[slice][0].x), topY(mesh[slice][0].z), topX(mesh[slice][24].x),
                 topY(mesh[slice][24].z), kGuideColor);
    }

    // Three local frame markers. Green is T, orange is N; they are perpendicular in world space.
    constexpr std::array<std::size_t, 3> kFrameSlices = {6, 17, 28};
    for (const std::size_t slice : kFrameSlices) {
        const float cx = topX(0.0f);
        const float cy = topY(mesh[slice][12].z);
        drawLine(image, cx, cy, cx, topY(mesh[slice][12].z + 2.2f), kAxisColor);
        drawLine(image, topX(-1.4f), cy, topX(1.4f), cy, kWarningColor);
        drawPoint(image, cx, cy, kEdgeColor);
    }

    applyRoundMask(image);
    writePpm(image, path);
}

float curvedTopX(float worldX) { return 225.0f + worldX * 22.0f; }

float curvedTopY(float worldZ) { return 425.0f - (worldZ - 1.5f) * 10.55f; }

void renderCurvedTopDebug(const CanyonMesh& mesh, const CurveSamples& samples, const std::string& path)
{
    Image image{};
    constexpr std::array<std::size_t, 7> kSemanticRails = {0, 3, 7, 12, 17, 21, 24};
    for (const std::size_t point : kSemanticRails) {
        const Pixel color = point == 12 ? kAxisColor : ((point == 7 || point == 17) ? kEdgeColor : kMidColor);
        for (std::size_t slice = 1; slice < mesh.size(); ++slice) {
            drawLine(image, curvedTopX(mesh[slice - 1][point].x), curvedTopY(mesh[slice - 1][point].z),
                     curvedTopX(mesh[slice][point].x), curvedTopY(mesh[slice][point].z), color);
        }
    }

    for (std::size_t slice = 0; slice < mesh.size(); slice += 4) {
        drawLine(image, curvedTopX(mesh[slice][0].x), curvedTopY(mesh[slice][0].z),
                 curvedTopX(mesh[slice][24].x), curvedTopY(mesh[slice][24].z), kGuideColor);
    }

    constexpr std::array<std::size_t, 4> kFrameSlices = {4, 12, 21, 29};
    for (const std::size_t slice : kFrameSlices) {
        const Vec2 center = samples[slice].center;
        const Vec2 tangentEnd = center + samples[slice].tangent * 2.2f;
        const Vec2 normalLeft = center - samples[slice].normal * 1.45f;
        const Vec2 normalRight = center + samples[slice].normal * 1.45f;
        drawLine(image, curvedTopX(center.x), curvedTopY(center.y), curvedTopX(tangentEnd.x),
                 curvedTopY(tangentEnd.y), kAxisColor);
        drawLine(image, curvedTopX(normalLeft.x), curvedTopY(normalLeft.y), curvedTopX(normalRight.x),
                 curvedTopY(normalRight.y), kWarningColor);
        drawPoint(image, curvedTopX(center.x), curvedTopY(center.y), kEdgeColor);
    }

    applyRoundMask(image);
    writePpm(image, path);
}

float sectionX(float lateral) { return 225.0f + lateral * 27.0f; }

float sectionY(float height) { return 390.0f - height * 82.0f; }

void renderCrossSection(const Profile& profile, const std::string& path)
{
    Image image{};
    drawLine(image, 18.0f, sectionY(0.0f), 432.0f, sectionY(0.0f), kGuideColor);
    drawLine(image, sectionX(0.0f), 42.0f, sectionX(0.0f), 420.0f, kGuideColor);

    for (std::size_t point = 1; point < profile.size(); ++point) {
        const bool floor = point >= 8 && point <= 17;
        const bool plateau = point <= 3 || point >= 22;
        const Pixel color = floor ? kAxisColor : (plateau ? kMidColor : kNearColor);
        drawLine(image, sectionX(profile[point - 1].lateral), sectionY(profile[point - 1].height),
                 sectionX(profile[point].lateral), sectionY(profile[point].height), color);
    }
    for (const auto& point : profile) {
        drawPoint(image, sectionX(point.lateral), sectionY(point.height), kEdgeColor);
    }

    // Clear-width dimension line below the floor, with end caps at both collision boundaries.
    constexpr float kDimensionY = 412.0f;
    drawLine(image, sectionX(profile[7].lateral), kDimensionY, sectionX(profile[17].lateral), kDimensionY,
             kWarningColor);
    drawLine(image, sectionX(profile[7].lateral), kDimensionY - 7.0f, sectionX(profile[7].lateral),
             kDimensionY + 7.0f, kWarningColor);
    drawLine(image, sectionX(profile[17].lateral), kDimensionY - 7.0f, sectionX(profile[17].lateral),
             kDimensionY + 7.0f, kWarningColor);

    applyRoundMask(image);
    writePpm(image, path);
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) return 2;
    const std::string outputDirectory = argv[1];
    const Profile profile = makeProfile();
    if (!validateProfile(profile)) return 3;

    const bool renderA2 = argc == 3 && std::string(argv[2]) == "a2";
    const bool renderA3 = argc == 3 && std::string(argv[2]) == "a3";
    if (argc == 3 && !renderA2 && !renderA3) return 2;
    if (renderA2 || renderA3) {
        const CurveSamples samples = makeCurvedSamples();
        const CanyonMesh baseMesh = makeCurvedMesh(profile, samples);
        if (!validateCurvedMesh(baseMesh, samples)) return 4;
        if (renderA2) {
            renderPerspective(baseMesh, outputDirectory + "/a2-perspective.ppm");
            renderCurvedTopDebug(baseMesh, samples, outputDirectory + "/a2-top-debug.ppm");
            renderCrossSection(profile, outputDirectory + "/a2-cross-section.ppm");
            writeCurveMetrics(samples, outputDirectory + "/a2-metrics.txt");
        } else {
            constexpr std::size_t kEventCenterSlice = 13;
            const float halfLength = samples[6].arcLength - samples[0].arcLength;
            const ShoulderEvent leftEvent{CanyonSide::Left, samples[kEventCenterSlice].arcLength, halfLength, 1.15f};
            const ShoulderEvent rightEvent{CanyonSide::Right, samples[kEventCenterSlice].arcLength, halfLength, 1.15f};
            const CanyonBoundaries leftBoundaries = makeBoundaries(samples, leftEvent);
            const CanyonBoundaries rightBoundaries = makeBoundaries(samples, rightEvent);
            const CanyonMesh leftMesh = makeShoulderMesh(profile, samples, leftBoundaries);
            const CanyonMesh rightMesh = makeShoulderMesh(profile, samples, rightBoundaries);
            if (!validateShoulderMesh(profile, leftMesh, samples, leftBoundaries, leftEvent) ||
                !validateShoulderMesh(profile, rightMesh, samples, rightBoundaries, rightEvent)) {
                return 5;
            }
            renderPerspective(leftMesh, outputDirectory + "/a3-left-perspective.ppm");
            renderCurvedTopDebug(leftMesh, samples, outputDirectory + "/a3-left-top-debug.ppm");
            renderCrossSection(deformProfile(profile, leftBoundaries[kEventCenterSlice]),
                               outputDirectory + "/a3-left-cross-section.ppm");
            renderPerspective(rightMesh, outputDirectory + "/a3-right-perspective.ppm");
            renderCurvedTopDebug(rightMesh, samples, outputDirectory + "/a3-right-top-debug.ppm");
            renderCrossSection(deformProfile(profile, rightBoundaries[kEventCenterSlice]),
                               outputDirectory + "/a3-right-cross-section.ppm");
            writeShoulderMetrics(profile, leftMesh, rightMesh, samples, leftBoundaries, rightBoundaries, leftEvent,
                                 rightEvent, outputDirectory + "/a3-metrics.txt");
        }
    } else {
        const CanyonMesh mesh = makeStraightMesh(profile);
        if (!validateStraightMesh(mesh)) return 3;
        renderPerspective(mesh, outputDirectory + "/a1-perspective.ppm");
        renderTopDebug(mesh, outputDirectory + "/a1-top-debug.ppm");
        renderCrossSection(profile, outputDirectory + "/a1-cross-section.ppm");
    }
    return 0;
}
