#include "app_lets_and_go_racer.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace {
constexpr uint32_t kGarageFrameIntervalMs = 33u;
constexpr uint32_t kShowcaseDurationMs = 1500u;
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
    _racerInput = std::make_unique<lets_and_go::HardwareRacerInputProvider>();
    _racerInput->open();
    _menuAxis.reset();
    _flow.reset();
    _selection.reset(_flow.setup().playerCar);
    _lastFrameMs = 0;
    _screenStartedMs = GetHAL().millis();
    GetHAL().stopLvglUpdate();
    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _renderer.render(_flow, _selection, 0u);
}

void AppLetsAndGoRacer::onRunning()
{
    GetHAL().updateButtonStates();
    const uint32_t nowMs = GetHAL().millis();
    const lets_and_go::RacerInput racerInput =
        _racerInput ? _racerInput->sample(nowMs) : lets_and_go::RacerInput{};
    const lets_and_go::RacerInputStatus racerStatus =
        _racerInput ? _racerInput->status(nowMs) : lets_and_go::RacerInputStatus{};
    if (_flow.screen() == lets_and_go::GameScreen::InputCheck &&
        racerStatus.axesConnected && racerStatus.actionsConfigured &&
        _flow.confirmInputAvailable()) {
        _racerInput->requestCalibration(nowMs);
        _screenStartedMs = nowMs;
    } else if (_flow.screen() == lets_and_go::GameScreen::InputCalibration &&
        racerStatus.ready() && _flow.completeCalibration(true)) {
        _screenStartedMs = nowMs;
    }
    handleRacerInput(racerInput, nowMs);
    const input::KeyEvent event = _keys ? _keys->update(false) : input::KeyEvent::None;
    if (event == input::KeyEvent::GoHome) {
        _flow.requestExit();
    } else if (event != input::KeyEvent::None) {
        handleKey(event, nowMs);
    }
    if (_flow.screen() == lets_and_go::GameScreen::ExitRequested) {
        close();
        return;
    }

    if (_flow.screen() == lets_and_go::GameScreen::CarShowcase &&
        nowMs - _screenStartedMs >= kShowcaseDurationMs &&
        _flow.completeCarShowcase()) {
        _selection.syncPlayer(_flow.setup().playerCar);
        _screenStartedMs = nowMs;
    }
    if (_lastFrameMs == 0u || nowMs - _lastFrameMs >= kGarageFrameIntervalMs) {
        _lastFrameMs = nowMs;
        _renderer.render(_flow, _selection, nowMs - _screenStartedMs);
    }
}

void AppLetsAndGoRacer::handleRacerInput(const lets_and_go::RacerInput& input,
                                         uint32_t nowMs)
{
    using lets_and_go::GameScreen;
    if (input.exitPressed) {
        _flow.requestExit();
        return;
    }
    const GameScreen before = _flow.screen();
    const int navigation = _menuAxis.update(input.valid ? input.steer : 0.0f, nowMs);
    if (navigation != 0) {
        if (before == GameScreen::CarSelect) {
            _selection.movePlayer(navigation);
        } else if (before == GameScreen::RivalSelect) {
            _selection.moveRival(navigation, _flow.setup().playerCar);
        }
    }
    if (input.cancelPressed) {
        _flow.back();
    } else if (input.confirmPressed) {
        switch (before) {
            case GameScreen::CarSelect: _selection.activatePlayer(_flow); break;
            case GameScreen::RivalSelect: _selection.activateRival(_flow); break;
            case GameScreen::TrackSelect: _flow.confirmTrack(); break;
            default: break;
        }
    }
    if (_flow.screen() != before || navigation != 0) _screenStartedMs = nowMs;
}

void AppLetsAndGoRacer::handleKey(input::KeyEvent event, uint32_t nowMs)
{
    using lets_and_go::GameScreen;
    const GameScreen before = _flow.screen();
    if (event == input::KeyEvent::GoPrevious) {
        if (before == GameScreen::CarSelect) {
            _selection.movePlayer(-1);
        } else if (before == GameScreen::RivalSelect) {
            _selection.moveRival(1, _flow.setup().playerCar);
        }
    } else if (event == input::KeyEvent::GoNext) {
        switch (before) {
            case GameScreen::InputCheck: _flow.confirmInputAvailable(); break;
            case GameScreen::InputCalibration: _flow.completeCalibration(true); break;
            case GameScreen::CarSelect: _selection.activatePlayer(_flow); break;
            case GameScreen::RivalSelect: _selection.activateRival(_flow); break;
            case GameScreen::TrackSelect: _flow.confirmTrack(); break;
            default: break;
        }
    }
    if (_flow.screen() != before || event == input::KeyEvent::GoPrevious) {
        _screenStartedMs = nowMs;
    }
}

void AppLetsAndGoRacer::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _keys.reset();
    if (_racerInput) _racerInput->close();
    _racerInput.reset();
    _menuAxis.reset();
    _renderer.close();
    _flow.reset();
    _selection.reset();
    _lastFrameMs = 0;
    _screenStartedMs = 0;
    GetHAL().startLvglUpdate();
}
