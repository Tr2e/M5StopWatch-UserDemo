#include "app_lets_and_go_racer.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

#include <algorithm>

namespace {
constexpr uint32_t kGarageFrameIntervalMs = 33u;
constexpr uint32_t kShowcaseDurationMs = 1500u;
constexpr uint32_t kGridIntroDurationMs = 1300u;
constexpr uint32_t kCountdownDurationMs = 3000u;
constexpr uint32_t kFinishDurationMs = 1200u;

bool usesRaceRenderer(lets_and_go::GameScreen screen)
{
    using lets_and_go::GameScreen;
    return screen == GameScreen::GridIntro || screen == GameScreen::Countdown ||
           screen == GameScreen::Racing || screen == GameScreen::Paused ||
           screen == GameScreen::Finish || screen == GameScreen::Results;
}
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
    _resultsSelection.reset();
    _progress = lets_and_go::RaceProgressStore::load();
    _flow.reset();
    _selection.reset(_progress.lastCar);
    _lastFrameMs = 0;
    _lastUpdateMs = GetHAL().millis();
    _raceSeed = 0;
    _inputInvalidSinceMs = 0;
    _pausedForInputLoss = false;
    _screenStartedMs = _lastUpdateMs;
    GetHAL().stopLvglUpdate();
    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _raceRenderer.open(display.width(), display.height());
    _renderer.render(_flow, _selection, 0u);
}

void AppLetsAndGoRacer::onRunning()
{
    GetHAL().updateButtonStates();
    const uint32_t nowMs = GetHAL().millis();
    const float deltaSeconds =
        std::min<uint32_t>(nowMs - _lastUpdateMs, 250u) * 0.001f;
    _lastUpdateMs = nowMs;
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

    if (_flow.screen() == lets_and_go::GameScreen::GridIntro &&
        nowMs - _screenStartedMs >= kGridIntroDurationMs &&
        _flow.completeGridIntro()) {
        _screenStartedMs = nowMs;
    } else if (_flow.screen() == lets_and_go::GameScreen::Countdown &&
               nowMs - _screenStartedMs >= kCountdownDurationMs &&
               _flow.completeCountdown()) {
        _screenStartedMs = nowMs;
    } else if (_flow.screen() == lets_and_go::GameScreen::Racing) {
        if (!racerInput.valid) {
            if (_inputInvalidSinceMs == 0u) _inputInvalidSinceMs = nowMs;
            if (nowMs - _inputInvalidSinceMs >= 600u && _flow.togglePause()) {
                _race.setPaused(true);
                _pausedForInputLoss = true;
                _screenStartedMs = nowMs;
            }
        } else {
            _inputInvalidSinceMs = 0u;
        }
        _race.advance(racerInput, deltaSeconds);
        if (_race.snapshot().playerFinished && _flow.finishRace()) {
            _screenStartedMs = nowMs;
        }
    } else if (_flow.screen() == lets_and_go::GameScreen::Finish &&
               nowMs - _screenStartedMs >= kFinishDurationMs &&
               _flow.showResults()) {
        _resultsSelection.reset();
        _progress.lastCar = _flow.setup().playerCar;
        if (lets_and_go::recordBestLap(_progress, _flow.setup().track,
                                       _race.snapshot().player().bestLapSeconds) &&
            !lets_and_go::RaceProgressStore::save(_progress)) {
            mclog::tagWarn(getAppInfo().name, "failed to save best lap");
        }
        _screenStartedMs = nowMs;
    }

    if (_flow.screen() == lets_and_go::GameScreen::CarShowcase &&
        nowMs - _screenStartedMs >= kShowcaseDurationMs &&
        _flow.completeCarShowcase()) {
        _selection.syncPlayer(_flow.setup().playerCar);
        _screenStartedMs = nowMs;
    }
    if (_lastFrameMs == 0u || nowMs - _lastFrameMs >= kGarageFrameIntervalMs) {
        _lastFrameMs = nowMs;
        if (usesRaceRenderer(_flow.screen())) {
            _raceRenderer.render(_flow, _race, _resultsSelection,
                                 nowMs - _screenStartedMs,
                                 _pausedForInputLoss);
        } else {
            _renderer.render(_flow, _selection, nowMs - _screenStartedMs);
        }
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
    if (input.pausePressed &&
        (before == GameScreen::Racing || before == GameScreen::Paused)) {
        if (_flow.togglePause()) {
            _race.setPaused(_flow.screen() == GameScreen::Paused);
            _pausedForInputLoss = false;
            _screenStartedMs = nowMs;
        }
        return;
    }
    const bool acceptsNavigation = before == GameScreen::CarSelect ||
                                   before == GameScreen::RivalSelect ||
                                   before == GameScreen::Results;
    if (!acceptsNavigation) _menuAxis.reset();
    const int navigation = acceptsNavigation
        ? _menuAxis.update(input.valid ? input.steer : 0.0f, nowMs) : 0;
    if (navigation != 0) {
        if (before == GameScreen::CarSelect) {
            _selection.movePlayer(navigation);
        } else if (before == GameScreen::RivalSelect) {
            _selection.moveRival(navigation, _flow.setup().playerCar);
        } else if (before == GameScreen::Results) {
            _resultsSelection.move(navigation);
        }
    }
    if (input.cancelPressed) {
        if (_flow.back() && before == GameScreen::Results) {
            _raceSeed = 0u;
            _selection.reset(_progress.lastCar);
        }
    } else if (input.confirmPressed) {
        switch (before) {
            case GameScreen::CarSelect:
                if (_selection.activatePlayer(_flow)) {
                    persistSelectedCar();
                }
                break;
            case GameScreen::RivalSelect: _selection.activateRival(_flow); break;
            case GameScreen::TrackSelect:
                if (_flow.confirmTrack()) prepareRace(nowMs);
                break;
            case GameScreen::Results: {
                const lets_and_go::ResultAction action = _resultsSelection.cursor();
                if (_resultsSelection.activate(_flow)) {
                    if (action == lets_and_go::ResultAction::Retry) {
                        prepareRace(nowMs);
                    } else if (action == lets_and_go::ResultAction::Garage) {
                        _raceSeed = 0u;
                        _selection.reset(_progress.lastCar);
                    }
                }
                break;
            }
            default: break;
        }
    }
    if (_flow.screen() != before || navigation != 0) _screenStartedMs = nowMs;
}

void AppLetsAndGoRacer::prepareRace(uint32_t nowMs)
{
    _inputInvalidSinceMs = 0u;
    _pausedForInputLoss = false;
    if (_raceSeed == 0u) {
        _raceSeed = nowMs ^ (static_cast<uint32_t>(_flow.setup().rivalMask) << 16u) ^
                    (static_cast<uint32_t>(_flow.setup().playerCar) << 24u) ^ 0x4c264721u;
    }
    _race.prepare(_flow.setup(), _raceSeed);
}

void AppLetsAndGoRacer::persistSelectedCar()
{
    if (_progress.lastCar == _flow.setup().playerCar) return;
    _progress.lastCar = _flow.setup().playerCar;
    if (!lets_and_go::RaceProgressStore::save(_progress)) {
        mclog::tagWarn(getAppInfo().name, "failed to save selected car");
    }
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
        } else if (before == GameScreen::Results) {
            _resultsSelection.move(1);
        }
    } else if (event == input::KeyEvent::GoNext) {
        switch (before) {
            case GameScreen::InputCheck: _flow.confirmInputAvailable(); break;
            case GameScreen::InputCalibration: _flow.completeCalibration(true); break;
            case GameScreen::CarSelect:
                if (_selection.activatePlayer(_flow)) {
                    persistSelectedCar();
                }
                break;
            case GameScreen::RivalSelect: _selection.activateRival(_flow); break;
            case GameScreen::TrackSelect:
                if (_flow.confirmTrack()) prepareRace(nowMs);
                break;
            case GameScreen::Results: {
                const lets_and_go::ResultAction action = _resultsSelection.cursor();
                if (_resultsSelection.activate(_flow)) {
                    if (action == lets_and_go::ResultAction::Retry) {
                        prepareRace(nowMs);
                    } else if (action == lets_and_go::ResultAction::Garage) {
                        _raceSeed = 0u;
                        _selection.reset(_progress.lastCar);
                    }
                }
                break;
            }
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
    _raceRenderer.close();
    _flow.reset();
    _selection.reset();
    _resultsSelection.reset();
    _lastFrameMs = 0;
    _screenStartedMs = 0;
    _lastUpdateMs = 0;
    _raceSeed = 0;
    _inputInvalidSinceMs = 0;
    _pausedForInputLoss = false;
    GetHAL().startLvglUpdate();
}
