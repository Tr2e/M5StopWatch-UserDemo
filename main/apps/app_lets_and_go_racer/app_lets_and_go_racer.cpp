#include "app_lets_and_go_racer.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace {
constexpr uint32_t kShellFrameIntervalMs = 100u;
constexpr uint16_t kPaperColor = 0xef3au;
constexpr uint16_t kPencilColor = 0x52abu;
constexpr uint16_t kPencilFaintColor = 0x9cf3u;
}

AppLetsAndGoRacer::AppLetsAndGoRacer()
{
    setAppInfo().name = "Let's & Go!!";
    setAppInfo().icon = (void*)&icon_lets_and_go;
}

void AppLetsAndGoRacer::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppLetsAndGoRacer::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _keys = std::make_unique<input::KeyManager>();
    _flow.reset();
    _lastFrameMs = 0;
    GetHAL().stopLvglUpdate();
    renderShell(GetHAL().millis());
}

void AppLetsAndGoRacer::onRunning()
{
    GetHAL().updateButtonStates();
    if (_keys && _keys->update(false) == input::KeyEvent::GoHome) {
        _flow.requestExit();
    }
    if (_flow.screen() == lets_and_go::GameScreen::ExitRequested) {
        close();
        return;
    }

    const uint32_t nowMs = GetHAL().millis();
    if (_lastFrameMs == 0u || nowMs - _lastFrameMs >= kShellFrameIntervalMs) {
        renderShell(nowMs);
    }
}

void AppLetsAndGoRacer::renderShell(uint32_t nowMs)
{
    _lastFrameMs = nowMs;
    auto& canvas = GetHAL().getDisplay();
    canvas.fillScreen(kPaperColor);
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextColor(kPencilColor, kPaperColor);
    canvas.setTextSize(2);
    canvas.drawString("LET'S & GO!!", canvas.width() / 2, canvas.height() / 2 - 28);
    canvas.setTextColor(kPencilFaintColor, kPaperColor);
    canvas.setTextSize(1);
    canvas.drawString(lets_and_go::gameScreenLabel(_flow.screen()),
                      canvas.width() / 2, canvas.height() / 2 + 18);
}

void AppLetsAndGoRacer::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _keys.reset();
    _flow.reset();
    _lastFrameMs = 0;
    GetHAL().startLvglUpdate();
}
