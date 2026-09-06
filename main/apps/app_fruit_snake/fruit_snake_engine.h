#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fruit_snake {

constexpr std::size_t kMinimumSegments = 3;
constexpr std::size_t kMaximumSegments = 24;
constexpr std::size_t kMaximumFruits = 6;

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

enum class FruitKind : uint8_t {
    Apple,
    Strawberry,
    Orange,
    Watermelon,
    Banana,
    Grapes,
    Cherries,
    Kiwi,
};

struct Fruit {
    Point position;
    FruitKind kind = FruitKind::Apple;
    uint32_t bornMs = 0;
    bool active = false;
};

enum class FeedbackKind : uint8_t {
    None,
    FruitAdded,
    FruitEaten,
    SnakeShortened,
    Tickled,
    EdgeBounce,
};

struct Feedback {
    FeedbackKind kind = FeedbackKind::None;
    Point position;
    uint32_t startedMs = 0;
};

enum class TapResult : uint8_t {
    FruitAdded,
    SnakeShortened,
    Tickled,
};

class Engine {
public:
    void reset(int width, int height, uint32_t seed, uint32_t nowMs = 0);
    void update(uint32_t nowMs);
    TapResult tap(int x, int y, uint32_t nowMs);
    bool addFruitNear(int x, int y, uint32_t nowMs);
    bool shorten(uint32_t nowMs);

    const std::array<Point, kMaximumSegments>& segments() const { return _segments; }
    std::size_t segmentCount() const { return _segmentCount; }
    const std::array<Fruit, kMaximumFruits>& fruits() const { return _fruits; }
    const Feedback& feedback() const { return _feedback; }
    uint32_t fruitsEaten() const { return _fruitsEaten; }
    float heading() const { return _heading; }
    int width() const { return _width; }
    int height() const { return _height; }
    float playRadius() const { return _playRadius; }

private:
    uint32_t nextRandom();
    float randomUnit();
    void step(float dt, uint32_t nowMs);
    void steerToward(float targetAngle, float maxStep);
    void chooseInwardHeading(uint32_t nowMs, const Point& position);
    void followHead();
    void eatNearbyFruit(uint32_t nowMs);
    int nearestFruitIndex() const;
    bool pointIsSafe(Point point, int ignoredFruit = -1) const;
    bool pointIsInside(Point point, float inset = 0.0f) const;
    Point nearestSafeGridPoint(Point requested, int ignoredFruit = -1) const;
    void setFeedback(FeedbackKind kind, Point position, uint32_t nowMs);

    std::array<Point, kMaximumSegments> _segments = {};
    std::array<Fruit, kMaximumFruits> _fruits = {};
    std::size_t _segmentCount = 0;
    Feedback _feedback;
    uint32_t _rngState = 1;
    uint32_t _lastUpdateMs = 0;
    uint32_t _lastFruitPresentMs = 0;
    uint32_t _edgeTurnUntilMs = 0;
    uint32_t _fruitsEaten = 0;
    float _heading = 0.0f;
    float _playRadius = 0.0f;
    int _width = 0;
    int _height = 0;
};

}  // namespace fruit_snake
