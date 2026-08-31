#include "app_vector_canyon_fighter.h"

#include <hal/hal.h>
#include <mooncake_log.h>

namespace {
constexpr uint32_t kFrameIntervalMs = 33;
}

AppVectorCanyonFighter::AppVectorCanyonFighter()
{
    setAppInfo().name = "Vector Run";
}

void AppVectorCanyonFighter::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppVectorCanyonFighter::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _keys = std::make_unique<input::KeyManager>();
    GetHAL().stopLvglUpdate();

    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _lastFrameMs = 0;
}

void AppVectorCanyonFighter::onRunning()
{
    GetHAL().updateButtonStates();
    if (_keys && _keys->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

    const uint32_t nowMs = GetHAL().millis();
    if (_lastFrameMs != 0 && nowMs - _lastFrameMs < kFrameIntervalMs) return;
    _lastFrameMs = nowMs;
    _renderer.renderStaticScene();
}

void AppVectorCanyonFighter::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _renderer.close();
    _keys.reset();
    GetHAL().startLvglUpdate();
}
