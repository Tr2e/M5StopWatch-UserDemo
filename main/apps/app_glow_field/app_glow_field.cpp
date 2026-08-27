#include "app_glow_field.h"
#include <algorithm>
#include <assets/assets.h>
#include <cmath>
#include <esp_timer.h>
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
constexpr uint32_t kAppearanceScaleRepeatMs = 160;
constexpr int kRippleRetriggerGuardRadius = 62;
constexpr uint8_t kMinShapeScalePercent = 80;
constexpr uint8_t kMaxShapeScalePercent = 150;
constexpr uint8_t kShapeScaleStepPercent = 5;
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

const char* dotShapeName(glow_field::DotShape shape)
{
    switch (shape) {
        case glow_field::DotShape::Star: return "star";
        case glow_field::DotShape::Hexagon: return "hexagon";
        case glow_field::DotShape::Circle: return "circle";
        case glow_field::DotShape::Triangle: return "triangle";
        case glow_field::DotShape::SymbolMix: return "symbol-mix";
    }
    return "unknown";
}

const char* interactionModeName(glow_field::InteractionMode mode)
{
    switch (mode) {
        case glow_field::InteractionMode::Ripple: return "ripple";
        case glow_field::InteractionMode::Paint: return "paint";
        case glow_field::InteractionMode::AudioReactive: return "audio";
    }
    return "unknown";
}

}  // namespace

AppGlowField::AppGlowField()
{
    setAppInfo().name = "Glow Field";
    setAppInfo().icon = (void*)&icon_glow_field;
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
    _audio = std::make_unique<glow_field::AudioReactiveController>();
    _touching = false;
    _lastHapticMs = 0;
    _lastTouchSampleMs = 0;
    _lastColorUpdateMs = 0;
    _lastColorPreviewMs = 0;
    _lastColorTouchMs = 0;
    _lastColorCruiseMs = 0;
    _lastRippleTriggerMs = 0;
    _lastAppearanceScaleMs = 0;
    _rngState = GetHAL().millis() ^ 0x9E3779B9u;
    _mode = glow_field::InteractionMode::Ripple;
    _renderScene = glow_field::RenderScene::Interactive;
    _audioFrame = {};
    _audioStatsStartedMs = 0;
    _lastAudioSpectrumSequence = GetHAL().getAudioSpectrum().sequence;
    _audioHopCount = 0;
    _engineAudioApplyCount = 0;
    _audioHopTimeMaxUs = 0;
    _controllerTimeMaxUs = 0;
    _engineAudioTimeMaxUs = 0;
    _onsetCount = 0;
    _sparkCount = 0;
    _bloomCount = 0;
    _audioHopTimeTotalUs = 0;
    _controllerTimeTotalUs = 0;
    _engineAudioTimeTotalUs = 0;
    _colorSelectionMode = false;
    _colorTouching = false;
    _colorMovedSincePreview = false;
    _colorCruising = false;
    _appearanceMode = false;
    _appearanceScaleRepeating = false;
    _dotShape = glow_field::DotShape::SymbolMix;
    _shapeScalePercent = 100;
    _selectedHue = 79;
    _selectedHueFine = 79.0f;
    _lastRippleX = 0;
    _lastRippleY = 0;
    _colorAngularSpeed = 0.0f;

    GetHAL().stopLvglUpdate();
    auto& display = GetHAL().getDisplay();
    _engine->reset(display.width(), display.height());
    _renderer->open(display.width(), display.height(), _engine->dotCount());
    _audio->reset(GetHAL().millis());
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
    if (_appearanceMode) {
        if (GetHAL().btnA.wasHold()) {
            toggleAppearanceMode(nowMs);
        } else {
            updateAppearanceButtons(nowMs, keyEvent);
        }
    } else if (GetHAL().btnB.wasHold()) {
        toggleColorSelection(nowMs);
    } else if (_colorSelectionMode) {
        updateColorSelectionButtons(nowMs);
    } else if (GetHAL().btnA.wasHold()) {
        toggleAppearanceMode(nowMs);
    } else if (keyEvent == input::KeyEvent::GoPrevious) {
        if (_mode == glow_field::InteractionMode::Paint) {
            triggerRandomPaint(nowMs);
        } else {
            triggerRandomRipple(nowMs);
        }
    } else if (keyEvent == input::KeyEvent::GoNext) {
        cycleInteractionMode(nowMs);
    }

    const bool audioActive = _mode == glow_field::InteractionMode::AudioReactive ||
                             (_engine && _engine->audioOutputActive());
    if (audioActive) updateAudioReactive(nowMs);

    if (_appearanceMode) {
        if (_touching) {
            _engine->endTouch();
            _touching = false;
        }
    } else if (_colorSelectionMode) {
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

    if (audioActive) applyAudioVisual(nowMs);

    _engine->update(nowMs);
    _renderer->render(*_engine, nowMs, _mode, _modeNoticeUntilMs,
                      _renderScene, _colorSelectionMode, _selectedHue, _dotShape,
                      _shapeScalePercent, _appearanceMode);
    reportAudioStats(nowMs);
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
    syncAudioVisualPause(nowMs);
    mclog::tagInfo(getAppInfo().name, "color-selection={}", _colorSelectionMode ? "on" : "off");
}

void AppGlowField::toggleAppearanceMode(uint32_t nowMs)
{
    _appearanceMode = !_appearanceMode;
    _renderScene = glow_field::RenderScene::Interactive;
    _engine->endTouch();
    _touching = false;
    _appearanceScaleRepeating = false;
    _lastAppearanceScaleMs = nowMs;
    GetHAL().vibrate(24, 55);
    syncAudioVisualPause(nowMs);
    mclog::tagInfo(getAppInfo().name, "appearance={}", _appearanceMode ? "on" : "off");
}

void AppGlowField::updateAppearanceButtons(uint32_t nowMs, input::KeyEvent keyEvent)
{
    if (keyEvent == input::KeyEvent::GoPrevious) {
        cycleDotShape();
        GetHAL().vibrate(18, 40);
        return;
    }

    if (GetHAL().btnB.wasHold()) {
        _appearanceScaleRepeating = true;
        _lastAppearanceScaleMs = nowMs;
        adjustShapeScale(-kShapeScaleStepPercent);
        return;
    }

    if (_appearanceScaleRepeating) {
        if (!GetHAL().btnB.isHolding()) {
            _appearanceScaleRepeating = false;
        } else if (nowMs - _lastAppearanceScaleMs >= kAppearanceScaleRepeatMs) {
            _lastAppearanceScaleMs = nowMs;
            adjustShapeScale(-kShapeScaleStepPercent);
        }
        return;
    }

    if (keyEvent == input::KeyEvent::GoNext) {
        adjustShapeScale(kShapeScaleStepPercent);
    }
}

void AppGlowField::cycleDotShape()
{
    switch (_dotShape) {
        case glow_field::DotShape::Star:
            _dotShape = glow_field::DotShape::Hexagon;
            break;
        case glow_field::DotShape::Hexagon:
            _dotShape = glow_field::DotShape::Circle;
            break;
        case glow_field::DotShape::Circle:
            _dotShape = glow_field::DotShape::Triangle;
            break;
        case glow_field::DotShape::Triangle:
            _dotShape = glow_field::DotShape::SymbolMix;
            break;
        case glow_field::DotShape::SymbolMix:
            _dotShape = glow_field::DotShape::Star;
            break;
    }
    mclog::tagInfo(getAppInfo().name, "shape={}", dotShapeName(_dotShape));
}

void AppGlowField::adjustShapeScale(int delta)
{
    const int next = std::clamp<int>(static_cast<int>(_shapeScalePercent) + delta,
                                     kMinShapeScalePercent, kMaxShapeScalePercent);
    if (next == _shapeScalePercent) return;
    _shapeScalePercent = static_cast<uint8_t>(next);
    GetHAL().vibrate(14, 32);
    mclog::tagInfo(getAppInfo().name, "shape-scale={}%", _shapeScalePercent);
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
    const bool symbolMix = _dotShape == glow_field::DotShape::SymbolMix;
    if (symbolMix) {
        const int diameter = maxRadius * 2 + 1;
        for (int i = 0; i < count; ++i) {
            int offsetX = 0;
            int offsetY = 0;
            do {
                offsetX = static_cast<int>(nextRandom() % static_cast<uint32_t>(diameter)) -
                          maxRadius;
                offsetY = static_cast<int>(nextRandom() % static_cast<uint32_t>(diameter)) -
                          maxRadius;
            } while (offsetX * offsetX + offsetY * offsetY > maxRadius * maxRadius);
            _engine->triggerRipple(centerX + offsetX, centerY + offsetY, nowMs, 0, true, false);
        }
        return;
    }
    const float angle = (_selectedHueFine - 90.0f) * kPi / 180.0f;
    const float dirX = std::cos(angle);
    const float dirY = std::sin(angle);
    const uint32_t span = static_cast<uint32_t>(std::max(1, maxRadius - minRadius + 1));
    for (int i = 0; i < count; ++i) {
        const int radius = minRadius + static_cast<int>(nextRandom() % span);
        _engine->triggerRipple(centerX + static_cast<int>(dirX * radius),
                               centerY + static_cast<int>(dirY * radius), nowMs,
                               colorIndex(_selectedHue), false);
    }
}

uint32_t AppGlowField::nextRandom()
{
    _rngState ^= _rngState << 13;
    _rngState ^= _rngState >> 17;
    _rngState ^= _rngState << 5;
    return _rngState;
}

void AppGlowField::triggerRandomRipple(uint32_t nowMs)
{
    const int width = GetHAL().getDisplay().width();
    const int height = GetHAL().getDisplay().height();
    const int centerX = width / 2;
    const int centerY = height / 2;
    const int maxRadius = std::min(width, height) / 2 - 28;
    const int diameter = maxRadius * 2 + 1;
    int offsetX = 0;
    int offsetY = 0;
    do {
        offsetX = static_cast<int>(nextRandom() % static_cast<uint32_t>(diameter)) - maxRadius;
        offsetY = static_cast<int>(nextRandom() % static_cast<uint32_t>(diameter)) - maxRadius;
    } while (offsetX * offsetX + offsetY * offsetY > maxRadius * maxRadius);

    const bool symbolMix = _dotShape == glow_field::DotShape::SymbolMix;
    _engine->triggerRipple(centerX + offsetX, centerY + offsetY, nowMs,
                           colorIndex(_selectedHue), symbolMix, symbolMix);
    GetHAL().vibrate(26, 58);
}

void AppGlowField::triggerRandomPaint(uint32_t nowMs)
{
    const int width = GetHAL().getDisplay().width();
    const int height = GetHAL().getDisplay().height();
    const int centerX = width / 2;
    const int centerY = height / 2;
    const int maxRadius = std::min(width, height) / 2 - 28;
    const int diameter = maxRadius * 2 + 1;
    int offsetX = 0;
    int offsetY = 0;
    do {
        offsetX = static_cast<int>(nextRandom() % static_cast<uint32_t>(diameter)) - maxRadius;
        offsetY = static_cast<int>(nextRandom() % static_cast<uint32_t>(diameter)) - maxRadius;
    } while (offsetX * offsetX + offsetY * offsetY > maxRadius * maxRadius);

    const bool symbolMix = _dotShape == glow_field::DotShape::SymbolMix;
    _engine->triggerPaintPoint(centerX + offsetX, centerY + offsetY, nowMs,
                               colorIndex(_selectedHue), symbolMix, symbolMix);
    GetHAL().vibrate(26, 58);
}

void AppGlowField::updateTouch(uint32_t nowMs)
{
    const Hal::TouchPoint touch = GetHAL().getTouchPoint();
    if (touch.num > 0 && touch.x >= 0 && touch.y >= 0) {
        if (!_touching) {
            bool accepted = true;
            if (_mode == glow_field::InteractionMode::Paint) {
                _engine->beginTouch(touch.x, touch.y, nowMs, colorIndex(_selectedHue),
                                    _dotShape == glow_field::DotShape::SymbolMix);
            } else {
                const int dx = touch.x - _lastRippleX;
                const int dy = touch.y - _lastRippleY;
                const bool likelyDuplicate = _lastRippleTriggerMs != 0 &&
                                             nowMs - _lastRippleTriggerMs <
                                                 kRippleRetriggerGuardMs &&
                                             dx * dx + dy * dy <
                                                 kRippleRetriggerGuardRadius *
                                                     kRippleRetriggerGuardRadius;
                if (!likelyDuplicate) {
                    _engine->triggerRipple(touch.x, touch.y, nowMs, colorIndex(_selectedHue),
                                           _dotShape == glow_field::DotShape::SymbolMix, false);
                    _lastRippleTriggerMs = nowMs;
                    _lastRippleX = static_cast<int16_t>(touch.x);
                    _lastRippleY = static_cast<int16_t>(touch.y);
                } else {
                    accepted = false;
                }
            }
            _touching = true;
            if (accepted && (_lastHapticMs == 0 || nowMs - _lastHapticMs >= 75)) {
                GetHAL().vibrate(30, 65);
                _lastHapticMs = nowMs;
            }
        } else if (_mode == glow_field::InteractionMode::Paint) {
            _engine->moveTouch(touch.x, touch.y, nowMs);
        }
    } else if (_touching) {
        _engine->endTouch();
        _touching = false;
    }
}

void AppGlowField::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    GetHAL().stopVibrate();
    if (_engine) _engine->clearAudioReaction(true);
    if (_renderer) _renderer->close();
    _renderer.reset();
    _engine.reset();
    _audio.reset();
    GetHAL().startLvglUpdate();
    _keys.reset();
}

void AppGlowField::cycleInteractionMode(uint32_t nowMs)
{
    _engine->endTouch();
    _touching = false;
    glow_field::InteractionMode next = glow_field::InteractionMode::Ripple;
    switch (_mode) {
        case glow_field::InteractionMode::Ripple:
            next = glow_field::InteractionMode::Paint;
            break;
        case glow_field::InteractionMode::Paint:
            next = glow_field::InteractionMode::AudioReactive;
            break;
        case glow_field::InteractionMode::AudioReactive:
            next = glow_field::InteractionMode::Ripple;
            break;
    }

    if (_mode == glow_field::InteractionMode::AudioReactive &&
        next != glow_field::InteractionMode::AudioReactive) {
        _engine->setAudioReactionEnabled(false, nowMs);
    }
    if (next == glow_field::InteractionMode::AudioReactive) {
        _audio->reset(nowMs);
        _audio->setVisualPaused(_colorSelectionMode || _appearanceMode, nowMs);
        _engine->setAudioReactionEnabled(true, nowMs);
        _engine->setAudioVisualPaused(_colorSelectionMode || _appearanceMode, nowMs);
        _audioStatsStartedMs = nowMs;
        _lastAudioSpectrumSequence = GetHAL().getAudioSpectrum().sequence;
        _audioHopCount = 0;
        _engineAudioApplyCount = 0;
        _audioHopTimeTotalUs = 0;
        _audioHopTimeMaxUs = 0;
        _controllerTimeTotalUs = 0;
        _controllerTimeMaxUs = 0;
        _engineAudioTimeTotalUs = 0;
        _engineAudioTimeMaxUs = 0;
        _onsetCount = 0;
        _sparkCount = 0;
        _bloomCount = 0;
    }

    _mode = next;
    _modeNoticeUntilMs = nowMs + 900;
    mclog::tagInfo(getAppInfo().name, "mode={}", interactionModeName(_mode));
}

void AppGlowField::syncAudioVisualPause(uint32_t nowMs)
{
    const bool paused = _colorSelectionMode || _appearanceMode;
    if (_audio) _audio->setVisualPaused(paused, nowMs);
    if (_engine) _engine->setAudioVisualPaused(paused, nowMs);
}

void AppGlowField::updateAudioReactive(uint32_t nowMs)
{
    if (!_audio || !_engine) return;

    const int64_t hopStartedUs = esp_timer_get_time();
    GetHAL().updateAudioSpectrum();
    const auto& spectrum = GetHAL().getAudioSpectrum();
    if (spectrum.sequence == _lastAudioSpectrumSequence) return;
    _lastAudioSpectrumSequence = spectrum.sequence;
    const uint32_t hopUs = spectrum.processUs != 0
                               ? spectrum.processUs
                               : static_cast<uint32_t>(esp_timer_get_time() - hopStartedUs);
    ++_audioHopCount;
    _audioHopTimeTotalUs += hopUs;
    _audioHopTimeMaxUs = std::max(_audioHopTimeMaxUs, hopUs);

    const int64_t controllerStartedUs = esp_timer_get_time();
    _audioFrame = _audio->update(spectrum.bands, spectrum.transientBands,
                                 spectrum.peakFrequencyHz, spectrum.signalActive,
                                 spectrum.signalConfidence, spectrum.inputRmsDbfs,
                                 spectrum.signalToNoiseDb, nowMs);
    const uint32_t controllerUs = static_cast<uint32_t>(esp_timer_get_time() - controllerStartedUs);
    _controllerTimeTotalUs += controllerUs;
    _controllerTimeMaxUs = std::max(_controllerTimeMaxUs, controllerUs);
}

void AppGlowField::applyAudioVisual(uint32_t nowMs)
{
    if (!_engine) return;
    const bool symbolMix = _dotShape == glow_field::DotShape::SymbolMix;
    const int64_t engineStartedUs = esp_timer_get_time();
    const glow_field::AudioApplyResult applied =
        _engine->applyAudioFrame(_audioFrame, nowMs, symbolMix, colorIndex(_selectedHue));
    const uint32_t engineUs = static_cast<uint32_t>(esp_timer_get_time() - engineStartedUs);
    ++_engineAudioApplyCount;
    _engineAudioTimeTotalUs += engineUs;
    _engineAudioTimeMaxUs = std::max(_engineAudioTimeMaxUs, engineUs);
    if (applied.onsetTriggered) ++_onsetCount;
    _sparkCount += applied.sparksSpawned;
    _bloomCount += applied.bloomsSpawned;
}

void AppGlowField::reportAudioStats(uint32_t nowMs)
{
    if (_mode != glow_field::InteractionMode::AudioReactive &&
        !(_engine && _engine->audioOutputActive())) {
        return;
    }
    if (_audioStatsStartedMs == 0) {
        _audioStatsStartedMs = nowMs;
        return;
    }
    const uint32_t elapsedMs = nowMs - _audioStatsStartedMs;
    if (elapsedMs < 10000 || _audioHopCount == 0) return;

    const float audioFps = static_cast<float>(_audioHopCount) * 1000.0f / static_cast<float>(elapsedMs);
    const float audioAvg = static_cast<float>(_audioHopTimeTotalUs) /
                           static_cast<float>(_audioHopCount) / 1000.0f;
    const float controllerAvg = static_cast<float>(_controllerTimeTotalUs) /
                                static_cast<float>(_audioHopCount) / 1000.0f;
    const float engineAvg = _engineAudioApplyCount == 0
                                ? 0.0f
                                : static_cast<float>(_engineAudioTimeTotalUs) /
                                      static_cast<float>(_engineAudioApplyCount) / 1000.0f;
    mclog::tagInfo("GlowField.Audio",
                   "audioFps={:.1f} audioAvg={:.2f}ms audioMax={:.2f}ms "
                   "ctrlAvg={:.2f}ms ctrlMax={:.2f}ms mapAvg={:.2f}ms mapMax={:.2f}ms "
                   "gate={} input={:.1f}dB snr={:.1f}dB conf={:.2f} activeDots={:.1f}% "
                   "onsets={} blooms={} sparks={} dirtyBandsAvg={:.1f} bass={:.2f} overall={:.2f}",
                   audioFps, audioAvg, static_cast<float>(_audioHopTimeMaxUs) / 1000.0f,
                   controllerAvg, static_cast<float>(_controllerTimeMaxUs) / 1000.0f, engineAvg,
                   static_cast<float>(_engineAudioTimeMaxUs) / 1000.0f,
                   _audioFrame.signalActive ? "open" : "closed", _audioFrame.inputRmsDbfs,
                   _audioFrame.signalToNoiseDb, _audioFrame.signalConfidence,
                   _engine ? _engine->audioActiveDotRatio() * 100.0f : 0.0f,
                   _onsetCount, _bloomCount, _sparkCount,
                   _renderer ? _renderer->dirtyBandsAverage() : 0.0f, _audioFrame.groups[0],
                   _audioFrame.overallEnergy);

    _audioStatsStartedMs = nowMs;
    _audioHopCount = 0;
    _engineAudioApplyCount = 0;
    _audioHopTimeTotalUs = 0;
    _audioHopTimeMaxUs = 0;
    _controllerTimeTotalUs = 0;
    _controllerTimeMaxUs = 0;
    _engineAudioTimeTotalUs = 0;
    _engineAudioTimeMaxUs = 0;
    _onsetCount = 0;
    _sparkCount = 0;
    _bloomCount = 0;
}
