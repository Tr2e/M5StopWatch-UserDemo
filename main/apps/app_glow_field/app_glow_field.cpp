#include "app_glow_field.h"

#include <algorithm>
#include <assets/assets.h>
#include <cmath>
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;

namespace {

constexpr uint32_t kTouchSampleIntervalMs = 10;
constexpr uint32_t kColorUpdateIntervalMs = 40;
constexpr uint32_t kColorPreviewSlowIntervalMs = 320;
constexpr uint32_t kColorPreviewFastIntervalMs = 220;
constexpr uint32_t kColorCruiseLapMs = 8000;
constexpr uint32_t kColorCruisePreviewIntervalMs = 500;
constexpr uint32_t kRippleRetriggerGuardMs = 180;
constexpr int kRippleRetriggerGuardRadius = 62;
constexpr float kPi = 3.14159265358979323846f;
constexpr uint8_t kHueSlots = 24;

uint8_t colorIndex(uint16_t hue)
{
    return static_cast<uint8_t>((hue % 360u) * kHueSlots / 360u);
}

float wrapHue(float hue)
{
    while (hue < 0.0f) hue += 360.0f;
    while (hue >= 360.0f) hue -= 360.0f;
    return hue;
}

}  // namespace

AppGlowField::AppGlowField()
{
    setAppInfo().name = "Glow Field";
    setAppInfo().icon = (void*)&icon_typhoon;
}

void AppGlowField::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppGlowField::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _keys = std::make_unique<input::KeyManager>();
    _engine = std::make_unique<glow_field::Engine>();
    _renderer = std::make_unique<glow_field::Renderer>();
    _touching = false;
    _lastHapticMs = 0;
    _lastTouchSampleMs = 0;
    _lastColorUpdateMs = 0;
    _lastColorPreviewMs = 0;
    _lastColorTouchMs = 0;
    _lastColorCruiseMs = 0;
    _lastRippleTriggerMs = 0;
    _rngState = GetHAL().millis() ^ 0x9E3779B9u;
    _mode = InteractionMode::Ripple;
    _renderScene = glow_field::RenderScene::Interactive;
    _colorSelectionMode = false;
    _colorTouching = false;
    _colorMovedSincePreview = false;
    _colorCruising = false;
    _selectedHue = 79;
    _selectedHueFine = 79.0f;
    _lastRippleX = 0;
    _lastRippleY = 0;
    _colorAngularSpeed = 0.0f;

    GetHAL().stopLvglUpdate();
    auto& display = GetHAL().getDisplay();
    _engine->reset(display.width(), display.height());
    _renderer->open(display.width(), display.height(), _engine->dotCount());
    _modeNoticeUntilMs = GetHAL().millis() + 900;
}

void AppGlowField::onRunning()
{
    GetHAL().updateButtonStates();
    const input::KeyEvent keyEvent = _keys ? _keys->update(false) : input::KeyEvent::None;
    if (keyEvent == input::KeyEvent::GoHome) {
        close();
        return;
    }
    if (!_engine || !_renderer) return;

    const uint32_t nowMs = GetHAL().millis();
    if (GetHAL().btnB.wasHold()) {
        toggleColorSelection(nowMs);
    } else if (_colorSelectionMode) {
        updateColorSelectionButtons(nowMs);
    } else if (GetHAL().btnA.wasHold()) {
        cycleBenchmarkScene();
        _modeNoticeUntilMs = nowMs + 900;
    } else if (keyEvent == input::KeyEvent::GoPrevious) {
        _engine->clear();
        _touching = false;
    } else if (keyEvent == input::KeyEvent::GoNext) {
        _engine->endTouch();
        _touching = false;
        _mode = _mode == InteractionMode::Ripple ? InteractionMode::Paint : InteractionMode::Ripple;
        _modeNoticeUntilMs = nowMs + 900;
        mclog::tagInfo(getAppInfo().name, "mode={}",
                       _mode == InteractionMode::Ripple ? "ripple" : "paint");
    }

    if (_colorSelectionMode) {
        if (!_colorCruising && _renderScene == glow_field::RenderScene::Interactive &&
            (_lastTouchSampleMs == 0 || nowMs - _lastTouchSampleMs >= kTouchSampleIntervalMs)) {
            _lastTouchSampleMs = nowMs;
            updateColorSelectionTouch(nowMs);
        }
    } else if (_renderScene == glow_field::RenderScene::Interactive &&
        (_lastTouchSampleMs == 0 || nowMs - _lastTouchSampleMs >= kTouchSampleIntervalMs)) {
        _lastTouchSampleMs = nowMs;
        updateTouch(nowMs);
    } else if (_renderScene != glow_field::RenderScene::Interactive && _touching) {
        _engine->endTouch();
        _touching = false;
    }

    _engine->update(nowMs);
    _renderer->render(*_engine, nowMs, _mode == InteractionMode::Ripple, _modeNoticeUntilMs,
                      _renderScene, _colorSelectionMode, _selectedHue);
}

void AppGlowField::toggleColorSelection(uint32_t nowMs)
{
    _colorSelectionMode = !_colorSelectionMode;
    _renderScene = glow_field::RenderScene::Interactive;
    _engine->endTouch();
    _touching = false;
    _colorTouching = false;
    _colorMovedSincePreview = false;
    _colorCruising = false;
    _colorAngularSpeed = 0.0f;
    _lastColorTouchMs = 0;
    _lastColorPreviewMs = nowMs;
    GetHAL().vibrate(24, 55);
    mclog::tagInfo(getAppInfo().name, "color-selection={}", _colorSelectionMode ? "on" : "off");
}

void AppGlowField::setSelectedHue(float hue)
{
    _selectedHueFine = wrapHue(hue);
    _selectedHue = static_cast<uint16_t>(_selectedHueFine + 0.5f) % 360u;
}

void AppGlowField::stepColorClockwise(uint32_t nowMs)
{
    const uint8_t nextSlot = static_cast<uint8_t>((colorIndex(_selectedHue) + 1) % kHueSlots);
    setSelectedHue(static_cast<float>(nextSlot * 360u / kHueSlots + 360u / (kHueSlots * 2)));
    spawnColorPreview(nowMs, 1);
    _lastColorPreviewMs = nowMs;
    GetHAL().vibrate(18, 40);
}

void AppGlowField::updateColorSelectionButtons(uint32_t nowMs)
{
    if (GetHAL().btnA.wasClicked()) {
        _colorCruising = false;
        stepColorClockwise(nowMs);
        return;
    }

    if (GetHAL().btnA.wasHold()) {
        _colorCruising = true;
        _lastColorCruiseMs = nowMs;
        spawnColorPreview(nowMs, 1);
        _lastColorPreviewMs = nowMs;
        GetHAL().vibrate(18, 40);
    }

    if (!_colorCruising) return;
    if (!GetHAL().btnA.isHolding()) {
        _colorCruising = false;
        return;
    }

    const uint32_t elapsedMs = nowMs - _lastColorCruiseMs;
    if (elapsedMs == 0) return;
    _lastColorCruiseMs = nowMs;
    setSelectedHue(_selectedHueFine + static_cast<float>(elapsedMs) * 360.0f /
                                          static_cast<float>(kColorCruiseLapMs));

    if (nowMs - _lastColorPreviewMs >= kColorCruisePreviewIntervalMs) {
        spawnColorPreview(nowMs, 1);
        _lastColorPreviewMs = nowMs;
    }
}

void AppGlowField::updateColorSelectionTouch(uint32_t nowMs)
{
    const Hal::TouchPoint touch = GetHAL().getTouchPoint();
    const int centerX = GetHAL().getDisplay().width() / 2;
    const int centerY = GetHAL().getDisplay().height() / 2;
    const int pathRadius = std::min(centerX * 2, centerY * 2) / 2 - 12;
    const int touchInnerRadius = pathRadius - 22;
    const int touchOuterRadius = pathRadius + 22;

    if (touch.num <= 0 || touch.x < 0 || touch.y < 0) {
        _colorTouching = false;
        _colorMovedSincePreview = false;
        _colorAngularSpeed *= 0.7f;
        return;
    }

    const int dx = touch.x - centerX;
    const int dy = touch.y - centerY;
    const int distanceSquared = dx * dx + dy * dy;
    if (distanceSquared < touchInnerRadius * touchInnerRadius ||
        distanceSquared > touchOuterRadius * touchOuterRadius) {
        _colorTouching = false;
        return;
    }

    float angle = std::atan2(static_cast<float>(dy), static_cast<float>(dx)) * 180.0f / kPi + 90.0f;
    if (angle < 0.0f) angle += 360.0f;
    if (angle >= 360.0f) angle -= 360.0f;

    const bool touchBegan = !_colorTouching;
    if (touchBegan) {
        _colorTouching = true;
        _lastColorAngle = angle;
        _lastColorTouchMs = nowMs;
        _colorAngularSpeed = 0.0f;
        _colorMovedSincePreview = false;
    } else {
        float delta = angle - _lastColorAngle;
        if (delta > 180.0f) delta -= 360.0f;
        if (delta < -180.0f) delta += 360.0f;
        const uint32_t elapsedMs = std::max<uint32_t>(1, nowMs - _lastColorTouchMs);
        const float instantaneousSpeed = std::abs(delta) * 1000.0f / static_cast<float>(elapsedMs);
        _colorAngularSpeed = _colorAngularSpeed * 0.72f + instantaneousSpeed * 0.28f;
        if (std::abs(delta) >= 0.8f) _colorMovedSincePreview = true;
        _lastColorAngle = angle;
        _lastColorTouchMs = nowMs;
    }

    if (_lastColorUpdateMs == 0 || nowMs - _lastColorUpdateMs >= kColorUpdateIntervalMs) {
        setSelectedHue(angle);
        _lastColorUpdateMs = nowMs;
    }

    if (touchBegan && nowMs - _lastColorPreviewMs >= kColorPreviewFastIntervalMs) {
        spawnColorPreview(nowMs, 1);
        _lastColorPreviewMs = nowMs;
    }

    const bool fast = _colorAngularSpeed >= 300.0f;
    const uint32_t previewInterval = fast ? kColorPreviewFastIntervalMs : kColorPreviewSlowIntervalMs;
    if (_colorMovedSincePreview && nowMs - _lastColorPreviewMs >= previewInterval) {
        // Keep the exact same full Ripple used by normal taps, but emit one
        // well-spaced preview at a time. Bursting 2-3 full-screen afterglows
        // every 65ms only replaced active ripples and saturated panel updates.
        spawnColorPreview(nowMs, 1);
        _lastColorPreviewMs = nowMs;
        _colorMovedSincePreview = false;
    }
}

void AppGlowField::spawnColorPreview(uint32_t nowMs, int count)
{
    const int width = GetHAL().getDisplay().width();
    const int height = GetHAL().getDisplay().height();
    const int centerX = width / 2;
    const int centerY = height / 2;
    // Stay inside the circular field and off the color ring itself.
    const int maxRadius = std::min(width, height) / 2 - 48;
    const int minRadius = 24;
    const float angle = (_selectedHueFine - 90.0f) * kPi / 180.0f;
    const float dirX = std::cos(angle);
    const float dirY = std::sin(angle);
    const uint32_t span = static_cast<uint32_t>(std::max(1, maxRadius - minRadius + 1));
    for (int i = 0; i < count; ++i) {
        const int radius = minRadius + static_cast<int>(nextRandom() % span);
        _engine->triggerRipple(centerX + static_cast<int>(dirX * radius),
                               centerY + static_cast<int>(dirY * radius), nowMs,
                               colorIndex(_selectedHue));
    }
}

uint32_t AppGlowField::nextRandom()
{
    _rngState ^= _rngState << 13;
    _rngState ^= _rngState >> 17;
    _rngState ^= _rngState << 5;
    return _rngState;
}

void AppGlowField::updateTouch(uint32_t nowMs)
{
    const Hal::TouchPoint touch = GetHAL().getTouchPoint();
    if (touch.num > 0 && touch.x >= 0 && touch.y >= 0) {
        if (!_touching) {
            bool accepted = true;
            if (_mode == InteractionMode::Ripple) {
                const int dx = touch.x - _lastRippleX;
                const int dy = touch.y - _lastRippleY;
                const bool likelyDuplicate = _lastRippleTriggerMs != 0 &&
                                             nowMs - _lastRippleTriggerMs <
                                                 kRippleRetriggerGuardMs &&
                                             dx * dx + dy * dy <
                                                 kRippleRetriggerGuardRadius *
                                                     kRippleRetriggerGuardRadius;
                if (!likelyDuplicate) {
                    _engine->triggerRipple(touch.x, touch.y, nowMs, colorIndex(_selectedHue));
                    _lastRippleTriggerMs = nowMs;
                    _lastRippleX = static_cast<int16_t>(touch.x);
                    _lastRippleY = static_cast<int16_t>(touch.y);
                } else {
                    accepted = false;
                }
            } else {
                _engine->beginTouch(touch.x, touch.y, nowMs,
                                    colorIndex(_selectedHue));
            }
            _touching = true;
            if (accepted && (_lastHapticMs == 0 || nowMs - _lastHapticMs >= 75)) {
                GetHAL().vibrate(30, 65);
                _lastHapticMs = nowMs;
            }
        } else if (_mode == InteractionMode::Paint) {
            _engine->moveTouch(touch.x, touch.y, nowMs);
        }
    } else if (_touching) {
        _engine->endTouch();
        _touching = false;
    }
}

void AppGlowField::cycleBenchmarkScene()
{
    switch (_renderScene) {
        case glow_field::RenderScene::Interactive:
            _renderScene = glow_field::RenderScene::Solid;
            break;
        case glow_field::RenderScene::Solid:
            _renderScene = glow_field::RenderScene::IdleDots;
            break;
        case glow_field::RenderScene::IdleDots:
            _renderScene = glow_field::RenderScene::EnergizedDots;
            break;
        case glow_field::RenderScene::EnergizedDots:
            _renderScene = glow_field::RenderScene::Interactive;
            break;
    }
    _engine->clear();
    _touching = false;
}

void AppGlowField::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    GetHAL().stopVibrate();
    if (_renderer) _renderer->close();
    _renderer.reset();
    _engine.reset();
    GetHAL().startLvglUpdate();
    _keys.reset();
}
