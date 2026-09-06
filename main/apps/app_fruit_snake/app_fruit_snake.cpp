#include "app_fruit_snake.h"

#include <assets/assets.h>
#include <cmath>
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;

namespace {
constexpr uint32_t kTouchSampleIntervalMs = 10;

fruit_snake::Sound soundForFeedback(fruit_snake::FeedbackKind kind)
{
    switch (kind) {
        case fruit_snake::FeedbackKind::FruitAdded: return fruit_snake::Sound::FruitAdded;
        case fruit_snake::FeedbackKind::FruitEaten: return fruit_snake::Sound::FruitEaten;
        case fruit_snake::FeedbackKind::SnakeShortened: return fruit_snake::Sound::SnakeShortened;
        case fruit_snake::FeedbackKind::Tickled: return fruit_snake::Sound::Tickled;
        case fruit_snake::FeedbackKind::EdgeBounce: return fruit_snake::Sound::EdgeBounce;
        case fruit_snake::FeedbackKind::None: return fruit_snake::Sound::FruitAdded;
    }
    return fruit_snake::Sound::FruitAdded;
}
}

AppFruitSnake::AppFruitSnake()
{
    setAppInfo().name = "Fruit Snake";
    setAppInfo().icon = (void*)&icon_fruit_snake;
}

void AppFruitSnake::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppFruitSnake::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _keys = std::make_unique<input::KeyManager>();
    _touching = false;
    _lastTouchSampleMs = 0;
    _handledFeedbackStartedMs = 0;
    _handledFeedbackKind = fruit_snake::FeedbackKind::None;
    _sfx.reset();
    GetHAL().stopLvglUpdate();
    auto& display = GetHAL().getDisplay();
    const uint32_t nowMs = GetHAL().millis();
    _engine.reset(display.width(), display.height(), nowMs ^ 0xF17A5EEDu, nowMs);
    _renderer.open(display.width(), display.height(), nowMs);
}

void AppFruitSnake::onRunning()
{
    GetHAL().updateButtonStates();
    const input::KeyEvent keyEvent = _keys ? _keys->update(false) : input::KeyEvent::None;
    if (keyEvent == input::KeyEvent::GoHome) {
        close();
        return;
    }

    const uint32_t nowMs = GetHAL().millis();
    if (keyEvent == input::KeyEvent::GoPrevious) {
        const auto& head = _engine.segments()[0];
        _engine.addFruitNear(static_cast<int>(head.x + std::cos(_engine.heading()) * 105.0f),
                             static_cast<int>(head.y + std::sin(_engine.heading()) * 105.0f),
                             nowMs);
        GetHAL().vibrate(24, 45);
    } else if (keyEvent == input::KeyEvent::GoNext) {
        _engine.shorten(nowMs);
        GetHAL().vibrate(24, 45);
    }

    if (_lastTouchSampleMs == 0 || nowMs - _lastTouchSampleMs >= kTouchSampleIntervalMs) {
        _lastTouchSampleMs = nowMs;
        const Hal::TouchPoint touch = GetHAL().getTouchPoint();
        if (touch.num > 0 && touch.x >= 0 && touch.y >= 0) {
            if (!_touching) {
                const fruit_snake::TapResult result = _engine.tap(touch.x, touch.y, nowMs);
                GetHAL().vibrate(result == fruit_snake::TapResult::FruitAdded ? 28 : 36,
                                 result == fruit_snake::TapResult::Tickled ? 42 : 58);
                _touching = true;
            }
        } else {
            _touching = false;
        }
    }

    _engine.update(nowMs);
    const fruit_snake::Feedback& feedback = _engine.feedback();
    if (feedback.kind != fruit_snake::FeedbackKind::None &&
        (feedback.startedMs != _handledFeedbackStartedMs ||
         feedback.kind != _handledFeedbackKind)) {
        _sfx.play(soundForFeedback(feedback.kind), nowMs);
        _handledFeedbackStartedMs = feedback.startedMs;
        _handledFeedbackKind = feedback.kind;
    }
    _renderer.render(_engine, nowMs);
}

void AppFruitSnake::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    GetHAL().stopVibrate();
    _renderer.close();
    GetHAL().startLvglUpdate();
    _keys.reset();
    _touching = false;
}
