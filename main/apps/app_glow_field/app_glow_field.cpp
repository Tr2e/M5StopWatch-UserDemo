#include "app_glow_field.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;

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
    _mode = InteractionMode::Ripple;

    GetHAL().stopLvglUpdate();
    auto& display = GetHAL().getDisplay();
    _engine->reset(display.width(), display.height());
    _renderer->open(display.width(), display.height(), _engine->dotCount());
    _modeNoticeUntilMs = GetHAL().millis() + 900;
    GetHAL().getCanvas().fillScreen(TFT_BLACK);
    GetHAL().updateCanvas();
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
    if (keyEvent == input::KeyEvent::GoPrevious) {
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

    const Hal::TouchPoint touch = GetHAL().getTouchPoint();
    if (touch.num > 0 && touch.x >= 0 && touch.y >= 0) {
        if (!_touching) {
            if (_mode == InteractionMode::Ripple) {
                _engine->triggerRipple(touch.x, touch.y, nowMs);
            } else {
                _engine->beginTouch(touch.x, touch.y, nowMs);
            }
            _touching = true;
            if (_lastHapticMs == 0 || nowMs - _lastHapticMs >= 75) {
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

    _engine->update(nowMs);
    _renderer->render(*_engine, nowMs, _mode == InteractionMode::Ripple, _modeNoticeUntilMs);
}

void AppGlowField::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    GetHAL().stopVibrate();
    _renderer.reset();
    _engine.reset();
    GetHAL().getCanvas().fillScreen(TFT_BLACK);
    GetHAL().updateCanvas();
    GetHAL().getDisplay().fillScreen(TFT_BLACK);
    GetHAL().startLvglUpdate();
    _keys.reset();
}
