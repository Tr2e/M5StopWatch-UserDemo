#include "../main/apps/app_fruit_snake/fruit_snake_engine.h"

#include <cmath>
#include <iostream>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool inside(const fruit_snake::Engine& engine, fruit_snake::Point point)
{
    const float dx = point.x - engine.width() * 0.5f;
    const float dy = point.y - engine.height() * 0.5f;
    return dx * dx + dy * dy <= engine.playRadius() * engine.playRadius();
}

}  // namespace

int main()
{
    using namespace fruit_snake;
    Engine engine;
    engine.reset(466, 466, 0x12345678u, 100u);

    bool valid = check(engine.segmentCount() == 7, "snake did not start at child-friendly length");
    std::size_t activeFruitCount = 0;
    for (const Fruit& fruit : engine.fruits()) {
        if (fruit.active) {
            ++activeFruitCount;
            valid &= check(inside(engine, fruit.position), "initial fruit is outside the play field");
        }
    }
    valid &= check(activeFruitCount == 3, "initial fruit garden must contain three fruits");

    const std::size_t beforeAdd = activeFruitCount;
    valid &= check(engine.tap(18, 18, 150u) == TapResult::FruitAdded,
                   "background tap did not add fruit");
    activeFruitCount = 0;
    for (const Fruit& fruit : engine.fruits()) {
        if (fruit.active) {
            ++activeFruitCount;
            valid &= check(inside(engine, fruit.position), "corner tap placed fruit outside field");
        }
    }
    valid &= check(activeFruitCount == beforeAdd + 1, "tap did not increase fruit count");

    const Point head = engine.segments()[0];
    valid &= check(engine.tap(static_cast<int>(head.x), static_cast<int>(head.y), 200u) ==
                       TapResult::SnakeShortened,
                   "snake tap did not shorten it");
    valid &= check(engine.segmentCount() == 6, "snake lost the wrong number of segments");
    while (engine.segmentCount() > kMinimumSegments) engine.shorten(210u);
    valid &= check(!engine.shorten(220u) && engine.segmentCount() == kMinimumSegments,
                   "snake shortened below its safe minimum");
    valid &= check(engine.feedback().kind == FeedbackKind::Tickled,
                   "minimum-length snake did not switch to tickle feedback");

    for (uint32_t now = 240u; now < 30240u; now += 20u) engine.update(now);
    valid &= check(engine.segmentCount() >= kMinimumSegments,
                   "autoplay invalidated the snake length");
    valid &= check(inside(engine, engine.segments()[0]),
                   "edge handling allowed the snake head to leave the play field");
    valid &= check(engine.feedback().kind != FeedbackKind::None,
                   "autoplay produced no friendly interaction feedback");
    return valid ? 0 : 1;
}
