#include "fruit_snake_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fruit_snake {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;
constexpr float kSnakeSpeed = 54.0f;
constexpr float kSegmentSpacing = 19.0f;
constexpr float kHeadRadius = 24.0f;
constexpr float kFruitEatRadius = 27.0f;
constexpr float kTapRadius = 31.0f;
constexpr float kGridStep = 22.0f;
constexpr uint32_t kAutoFruitDelayMs = 1200;

float distanceSquared(Point a, Point b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float wrapAngle(float angle)
{
    while (angle > kPi) angle -= kTwoPi;
    while (angle < -kPi) angle += kTwoPi;
    return angle;
}

}  // namespace

void Engine::reset(int width, int height, uint32_t seed, uint32_t nowMs)
{
    _width = std::max(1, width);
    _height = std::max(1, height);
    _playRadius = static_cast<float>(std::min(_width, _height)) * 0.5f - 31.0f;
    _rngState = seed == 0 ? 1 : seed;
    _lastUpdateMs = nowMs;
    _lastFruitPresentMs = nowMs;
    _edgeTurnUntilMs = 0;
    _fruitsEaten = 0;
    _heading = -0.30f;
    _feedback = {};
    _fruits = {};

    _segmentCount = 7;
    const Point center{_width * 0.5f, _height * 0.53f};
    for (std::size_t i = 0; i < _segmentCount; ++i) {
        _segments[i] = {center.x - static_cast<float>(i) * kSegmentSpacing,
                        center.y + static_cast<float>(i) * 1.7f};
    }
    addFruitNear(static_cast<int>(_width * 0.72f), static_cast<int>(_height * 0.30f), nowMs);
    addFruitNear(static_cast<int>(_width * 0.30f), static_cast<int>(_height * 0.25f), nowMs);
    addFruitNear(static_cast<int>(_width * 0.65f), static_cast<int>(_height * 0.72f), nowMs);
    _feedback = {};
}

uint32_t Engine::nextRandom()
{
    _rngState ^= _rngState << 13;
    _rngState ^= _rngState >> 17;
    _rngState ^= _rngState << 5;
    return _rngState;
}

float Engine::randomUnit()
{
    return static_cast<float>(nextRandom() & 0xffffu) / 65535.0f;
}

bool Engine::pointIsInside(Point point, float inset) const
{
    const float dx = point.x - _width * 0.5f;
    const float dy = point.y - _height * 0.5f;
    const float radius = std::max(8.0f, _playRadius - inset);
    return dx * dx + dy * dy <= radius * radius;
}

bool Engine::pointIsSafe(Point point, int ignoredFruit) const
{
    if (!pointIsInside(point, 18.0f)) return false;
    for (std::size_t i = 0; i < _segmentCount; ++i) {
        if (distanceSquared(point, _segments[i]) < 45.0f * 45.0f) return false;
    }
    for (std::size_t i = 0; i < _fruits.size(); ++i) {
        if (static_cast<int>(i) == ignoredFruit || !_fruits[i].active) continue;
        if (distanceSquared(point, _fruits[i].position) < 44.0f * 44.0f) return false;
    }
    return true;
}

Point Engine::nearestSafeGridPoint(Point requested, int ignoredFruit) const
{
    const Point center{_width * 0.5f, _height * 0.5f};
    const float dx = requested.x - center.x;
    const float dy = requested.y - center.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance > _playRadius - 22.0f && distance > 0.001f) {
        const float scale = (_playRadius - 22.0f) / distance;
        requested = {center.x + dx * scale, center.y + dy * scale};
    }
    requested.x = std::round(requested.x / kGridStep) * kGridStep;
    requested.y = std::round(requested.y / kGridStep) * kGridStep;
    if (pointIsSafe(requested, ignoredFruit)) return requested;

    Point best = center;
    float bestDistance = std::numeric_limits<float>::max();
    for (int ring = 1; ring <= 9; ++ring) {
        for (int gy = -ring; gy <= ring; ++gy) {
            for (int gx = -ring; gx <= ring; ++gx) {
                if (std::abs(gx) != ring && std::abs(gy) != ring) continue;
                const Point candidate{requested.x + gx * kGridStep,
                                      requested.y + gy * kGridStep};
                if (!pointIsSafe(candidate, ignoredFruit)) continue;
                const float candidateDistance = distanceSquared(candidate, requested);
                if (candidateDistance < bestDistance) {
                    best = candidate;
                    bestDistance = candidateDistance;
                }
            }
        }
        if (bestDistance < std::numeric_limits<float>::max()) break;
    }
    return best;
}

bool Engine::addFruitNear(int x, int y, uint32_t nowMs)
{
    int slot = -1;
    uint32_t oldest = std::numeric_limits<uint32_t>::max();
    for (std::size_t i = 0; i < _fruits.size(); ++i) {
        if (!_fruits[i].active) {
            slot = static_cast<int>(i);
            break;
        }
        if (_fruits[i].bornMs < oldest) {
            oldest = _fruits[i].bornMs;
            slot = static_cast<int>(i);
        }
    }
    if (slot < 0) return false;

    const Point position = nearestSafeGridPoint(
        {static_cast<float>(x), static_cast<float>(y)}, slot);
    _fruits[static_cast<std::size_t>(slot)] = {
        position,
        static_cast<FruitKind>(nextRandom() % 8u),
        nowMs,
        true,
    };
    _lastFruitPresentMs = nowMs;
    setFeedback(FeedbackKind::FruitAdded, position, nowMs);
    return true;
}

bool Engine::shorten(uint32_t nowMs)
{
    if (_segmentCount <= kMinimumSegments) {
        setFeedback(FeedbackKind::Tickled, _segments[0], nowMs);
        return false;
    }
    const Point tail = _segments[_segmentCount - 1];
    --_segmentCount;
    setFeedback(FeedbackKind::SnakeShortened, tail, nowMs);
    return true;
}

TapResult Engine::tap(int x, int y, uint32_t nowMs)
{
    const Point touch{static_cast<float>(x), static_cast<float>(y)};
    for (std::size_t i = 0; i < _segmentCount; ++i) {
        if (distanceSquared(touch, _segments[i]) <= kTapRadius * kTapRadius) {
            return shorten(nowMs) ? TapResult::SnakeShortened : TapResult::Tickled;
        }
    }
    addFruitNear(x, y, nowMs);
    return TapResult::FruitAdded;
}

int Engine::nearestFruitIndex() const
{
    int nearest = -1;
    float bestDistance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < _fruits.size(); ++i) {
        if (!_fruits[i].active) continue;
        const float candidate = distanceSquared(_segments[0], _fruits[i].position);
        if (candidate < bestDistance) {
            bestDistance = candidate;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

void Engine::steerToward(float targetAngle, float maxStep)
{
    const float difference = wrapAngle(targetAngle - _heading);
    _heading = wrapAngle(_heading + std::clamp(difference, -maxStep, maxStep));
}

void Engine::chooseInwardHeading(uint32_t nowMs, const Point& position)
{
    const Point center{_width * 0.5f, _height * 0.5f};
    const float inward = std::atan2(center.y - position.y, center.x - position.x);
    const float playfulJitter = (randomUnit() - 0.5f) * 1.35f;
    _heading = wrapAngle(inward + playfulJitter);
    _edgeTurnUntilMs = nowMs + 620;
    setFeedback(FeedbackKind::EdgeBounce, position, nowMs);
}

void Engine::followHead()
{
    for (std::size_t i = 1; i < _segmentCount; ++i) {
        const Point leader = _segments[i - 1];
        Point& follower = _segments[i];
        const float dx = leader.x - follower.x;
        const float dy = leader.y - follower.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= kSegmentSpacing || distance < 0.001f) continue;
        const float movement = distance - kSegmentSpacing;
        follower.x += dx / distance * movement;
        follower.y += dy / distance * movement;
    }
}

void Engine::eatNearbyFruit(uint32_t nowMs)
{
    for (Fruit& fruit : _fruits) {
        if (!fruit.active ||
            distanceSquared(_segments[0], fruit.position) > kFruitEatRadius * kFruitEatRadius) {
            continue;
        }
        const Point eatenAt = fruit.position;
        fruit.active = false;
        ++_fruitsEaten;
        if (_segmentCount < kMaximumSegments) {
            _segments[_segmentCount] = _segments[_segmentCount - 1];
            ++_segmentCount;
        }
        setFeedback(FeedbackKind::FruitEaten, eatenAt, nowMs);
        _lastFruitPresentMs = nowMs;
        return;
    }
}

void Engine::step(float dt, uint32_t nowMs)
{
    Point& head = _segments[0];
    const Point center{_width * 0.5f, _height * 0.5f};
    const float centerDx = head.x - center.x;
    const float centerDy = head.y - center.y;
    const float centerDistance = std::sqrt(centerDx * centerDx + centerDy * centerDy);

    if (centerDistance >= _playRadius - kHeadRadius) {
        if (nowMs >= _edgeTurnUntilMs) chooseInwardHeading(nowMs, head);
        if (centerDistance > 0.001f) {
            const float scale = (_playRadius - kHeadRadius) / centerDistance;
            head.x = center.x + centerDx * scale;
            head.y = center.y + centerDy * scale;
        }
    } else if (nowMs >= _edgeTurnUntilMs) {
        const int targetIndex = nearestFruitIndex();
        if (targetIndex >= 0) {
            const Point target = _fruits[static_cast<std::size_t>(targetIndex)].position;
            const float targetAngle = std::atan2(target.y - head.y, target.x - head.x);
            steerToward(targetAngle, 1.28f * dt);
        } else {
            _heading = wrapAngle(_heading + std::sin(nowMs * 0.0013f) * 0.10f * dt);
        }
    }

    head.x += std::cos(_heading) * kSnakeSpeed * dt;
    head.y += std::sin(_heading) * kSnakeSpeed * dt;
    followHead();

    for (std::size_t i = 5; i < _segmentCount; ++i) {
        if (distanceSquared(head, _segments[i]) < 25.0f * 25.0f) {
            _heading = wrapAngle(_heading + (randomUnit() < 0.5f ? -0.85f : 0.85f));
            _edgeTurnUntilMs = nowMs + 330;
            break;
        }
    }
    eatNearbyFruit(nowMs);
}

void Engine::update(uint32_t nowMs)
{
    if (_lastUpdateMs == 0) {
        _lastUpdateMs = nowMs;
        return;
    }
    const uint32_t elapsedMs = std::min<uint32_t>(nowMs - _lastUpdateMs, 80u);
    _lastUpdateMs = nowMs;
    if (elapsedMs > 0) step(static_cast<float>(elapsedMs) * 0.001f, nowMs);

    bool hasFruit = false;
    for (const Fruit& fruit : _fruits) hasFruit = hasFruit || fruit.active;
    if (hasFruit) {
        _lastFruitPresentMs = nowMs;
    } else if (nowMs - _lastFruitPresentMs >= kAutoFruitDelayMs) {
        const float angle = randomUnit() * kTwoPi;
        const float radius = _playRadius * (0.30f + randomUnit() * 0.45f);
        addFruitNear(static_cast<int>(_width * 0.5f + std::cos(angle) * radius),
                     static_cast<int>(_height * 0.5f + std::sin(angle) * radius), nowMs);
    }
}

void Engine::setFeedback(FeedbackKind kind, Point position, uint32_t nowMs)
{
    _feedback = {kind, position, nowMs};
}

}  // namespace fruit_snake
