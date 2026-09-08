#include "app_lets_and_go_racer.h"
#include "lets_and_go_config.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

#include <algorithm>

namespace {
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
    _garageBudget.reset();
    _raceBudget.reset();
    _resultsSelection.reset();
    _progress = lets_and_go::RaceProgressStore::load();
    _flow.reset();
    _selection.reset(_progress.lastCar);
    _lastFrameMs = 0;
    _lastUpdateMs = GetHAL().millis();
    _garageNavigation.reset();
    _garageView.reset(_progress.lastCar,_lastUpdateMs);
    _raceSeed = 0;
    _inputInvalidSinceMs = 0;
    _pausedForInputLoss = false;
    _lastWallFeedbackMs = 0u;
    _lastClampCount = 0u;
    _feedbackScreen = _flow.screen();
    _feedbackCountdown = 255u;
    _feedbackLap = 0u;
    _feedbackBoost = false;
    _feedbackBrake = false;
    _feedbackWallActive = false;
    const auto& feedbackConfig = GetHAL().getButtonConfig(true);
    _feedbackSfxEnabled = feedbackConfig.sfxEnabled;
    _feedbackVibrateEnabled = feedbackConfig.vibrateEnabled;
    if(_feedbackSfxEnabled) {
        _audio=std::make_unique<lets_and_go::RacerAudio>(GetHAL().getAudioSampleRate());
        if(!_audio->open()) {
            mclog::tagWarn(getAppInfo().name,"audio stream unavailable; continuing silently");
            _audio.reset();
        }
    }
    _screenStartedMs = _lastUpdateMs;
    GetHAL().stopLvglUpdate();
    const auto& display = GetHAL().getDisplay();
    _renderer.open(display.width(), display.height());
    _raceRenderer.open(display.width(), display.height());
    _renderer.render(_flow, _selection, 0u, lets_and_go::PencilDetail::High);
    GetHAL().updateCanvas();
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
        nowMs - _screenStartedMs >= lets_and_go::tuning::kGridIntroDurationMs &&
        _flow.completeGridIntro()) {
        _screenStartedMs = nowMs;
    } else if (_flow.screen() == lets_and_go::GameScreen::Countdown &&
               nowMs - _screenStartedMs >= lets_and_go::tuning::kCountdownDurationMs &&
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
               nowMs - _screenStartedMs >= lets_and_go::tuning::kFinishDurationMs &&
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
        nowMs - _screenStartedMs >= lets_and_go::tuning::kShowcaseDurationMs &&
        _flow.completeCarShowcase()) {
        _selection.syncPlayer(_flow.setup().playerCar);
        _screenStartedMs = nowMs;
    }
    if(_flow.screen()==lets_and_go::GameScreen::CarSelect)
        _garageView.selectCar(_selection.playerCursor(),nowMs);
    updateFeedback(racerInput, nowMs);
    if (_lastFrameMs == 0u ||
        nowMs - _lastFrameMs >= lets_and_go::tuning::kFrameIntervalMs) {
        _lastFrameMs = nowMs;
        const uint32_t renderStartedMs = GetHAL().millis();
        const bool raceView = usesRaceRenderer(_flow.screen());
        if (raceView) {
            _raceRenderer.render(_flow, _race, _resultsSelection,
                                 nowMs - _screenStartedMs,
                                 _pausedForInputLoss, _raceBudget.detail());
        } else {
            _renderer.render(_flow, _selection, nowMs - _screenStartedMs,
                             _garageBudget.detail(), racerStatus,_garageView.state(nowMs));
        }
        GetHAL().updateCanvas();
        const uint32_t renderMs = GetHAL().millis() - renderStartedMs;
        const uint16_t clampCount = _race.prepared()
                                        ? _race.snapshot().simulationClampCount : 0u;
        const bool simulationClamped = clampCount != _lastClampCount;
        _lastClampCount = clampCount;
        if (raceView) {
            _raceBudget.observe(renderMs, simulationClamped);
        } else {
            _garageBudget.observe(renderMs, false);
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
    int navigation=0;
    if(before==GameScreen::CarSelect) {
        _menuAxis.reset();
        const bool navigate=input.valid && !input.menuBlocked;
        const auto step=_garageNavigation.update(navigate ? input.steer : 0.f,
                                                 navigate ? -input.viewAxis : 0.f,nowMs);
        navigation=step.car;
        if(step.view) {
            _garageView.changeView(step.view,nowMs);
            playSound(lets_and_go::SoundCue::Navigate);
        }
    } else {
        _garageNavigation.reset();
        navigation=acceptsNavigation ? _menuAxis.update(input.valid && !input.menuBlocked ? input.steer : 0.f,nowMs) : 0;
    }
    if (navigation != 0) {
        playSound(lets_and_go::SoundCue::Navigate);
        if (before == GameScreen::CarSelect) {
            _selection.movePlayer(navigation);
        } else if (before == GameScreen::RivalSelect) {
            _selection.moveRival(navigation, _flow.setup().playerCar);
        } else if (before == GameScreen::Results) {
            _resultsSelection.move(navigation);
        }
    }
    if (input.cancelPressed || input.pausePressed) {
        const bool changed = before == GameScreen::RivalSelect && !input.pausePressed
                                 ? _selection.cancelRival(_flow)
                                 : _flow.back();
        if(changed)playSound(lets_and_go::SoundCue::Back);
        if (changed && before == GameScreen::Results) {
            _raceSeed = 0u;
            _selection.reset(_progress.lastCar);
        }
    } else if (input.confirmPressed) {
        switch (before) {
            case GameScreen::CarSelect:
                if (_selection.activatePlayer(_flow)) {
                    playSound(lets_and_go::SoundCue::Confirm);
                    persistSelectedCar();
                }
                break;
            case GameScreen::RivalSelect:
                playSound(_selection.activateRival(_flow) ? lets_and_go::SoundCue::Confirm : lets_and_go::SoundCue::Reject);
                break;
            case GameScreen::TrackSelect:
                if (_flow.confirmTrack()) { playSound(lets_and_go::SoundCue::Confirm);prepareRace(nowMs); }
                break;
            case GameScreen::Results: {
                const lets_and_go::ResultAction action = _resultsSelection.cursor();
                if (_resultsSelection.activate(_flow)) {
                    playSound(lets_and_go::SoundCue::Confirm);
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
    _feedbackLap=0;
    _feedbackBoost=false;
    _feedbackBrake=false;
    _feedbackWallActive=false;
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

void AppLetsAndGoRacer::updateFeedback(const lets_and_go::RacerInput& input,
                                       uint32_t nowMs)
{
    using lets_and_go::GameScreen;
    using lets_and_go::SoundCue;
    using lets_and_go::MusicScene;
    const auto playCue = [this](const lets_and_go::tuning::FeedbackCue& cue,SoundCue sound) {
        playSound(sound);
        vibrateFeedback(cue.vibrationStrength, cue.vibrationDurationMs);
    };
    const GameScreen screen = _flow.screen();
    if(_audio) {
        MusicScene scene=MusicScene::Off;
        if(screen==GameScreen::CarSelect || screen==GameScreen::CarShowcase ||
           screen==GameScreen::RivalSelect || screen==GameScreen::TrackSelect || screen==GameScreen::GridIntro)
            scene=MusicScene::Garage;
        else if(screen==GameScreen::Racing)
            scene=_race.snapshot().player().completedLaps>=2 ? MusicScene::FinalLap : MusicScene::Race;
        else if(screen==GameScreen::Paused)scene=MusicScene::Paused;
        else if(screen==GameScreen::Finish || screen==GameScreen::Results)scene=MusicScene::Results;
        _audio->setScene(scene);
    }
    if (screen == GameScreen::Countdown) {
        const uint8_t tick = static_cast<uint8_t>(
            std::min<uint32_t>(2u, (nowMs - _screenStartedMs) / 1000u));
        if (tick != _feedbackCountdown) {
            _feedbackCountdown = tick;
            playCue(lets_and_go::tuning::kCountdownCue,SoundCue::Countdown);
        }
    } else {
        _feedbackCountdown = 255u;
    }
    if (screen != _feedbackScreen) {
        if (screen == GameScreen::Racing && _feedbackScreen == GameScreen::Countdown) {
            playCue(lets_and_go::tuning::kGoCue,SoundCue::Go);
        } else if (screen == GameScreen::Finish) {
            playCue(lets_and_go::tuning::kFinishCue,SoundCue::Finish);
        } else if(screen==GameScreen::Paused) {
            playSound(SoundCue::Pause);
        } else if(screen==GameScreen::Racing && _feedbackScreen==GameScreen::Paused) {
            playSound(SoundCue::Resume);
        }
        _feedbackScreen = screen;
    }
    if (screen == GameScreen::Racing && _race.prepared()) {
        const auto& player = _race.snapshot().player();
        const bool boosting = input.valid && input.boostHeld && !input.brakeHeld &&
                              player.motion.boostCharge > 0.02f;
        if (boosting && !_feedbackBoost) {
            playCue(lets_and_go::tuning::kBoostCue,SoundCue::Boost);
        }
        _feedbackBoost = boosting;
        const bool braking=input.valid && input.brakeHeld;
        if(braking && !_feedbackBrake)playSound(SoundCue::Brake);
        _feedbackBrake=braking;
        const bool wallHit = player.motion.wallImpact > 0.75f;
        if (wallHit && !_feedbackWallActive &&
            (_lastWallFeedbackMs == 0u || nowMs - _lastWallFeedbackMs >= 300u)) {
            _lastWallFeedbackMs = nowMs;
            playCue(lets_and_go::tuning::kWallCue,SoundCue::Wall);
        }
        _feedbackWallActive = wallHit;
        if (player.completedLaps != _feedbackLap) {
            _feedbackLap = player.completedLaps;
            if (_feedbackLap == 2u) {
                playCue(lets_and_go::tuning::kFinalLapCue,SoundCue::FinalLap);
            } else if(_feedbackLap==1u) {
                playSound(SoundCue::Lap);
            }
        }
    } else {
        _feedbackBoost = false;
        _feedbackBrake = false;
        _feedbackWallActive = false;
    }
}

void AppLetsAndGoRacer::playSound(lets_and_go::SoundCue cue)
{
    if (_audio) _audio->trigger(cue);
}

void AppLetsAndGoRacer::vibrateFeedback(uint8_t strength, uint16_t durationMs)
{
    if (_feedbackVibrateEnabled) GetHAL().vibrate(durationMs, strength);
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
            // Hardware readiness/calibration cannot be bypassed by body buttons.
            case GameScreen::CarSelect:
                if (_selection.activatePlayer(_flow)) {
                    playSound(lets_and_go::SoundCue::Confirm);
                    persistSelectedCar();
                }
                break;
            case GameScreen::RivalSelect:
                playSound(_selection.activateRival(_flow) ? lets_and_go::SoundCue::Confirm : lets_and_go::SoundCue::Reject);
                break;
            case GameScreen::TrackSelect:
                if (_flow.confirmTrack()) { playSound(lets_and_go::SoundCue::Confirm);prepareRace(nowMs); }
                break;
            case GameScreen::Results: {
                const lets_and_go::ResultAction action = _resultsSelection.cursor();
                if (_resultsSelection.activate(_flow)) {
                    playSound(lets_and_go::SoundCue::Confirm);
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
    const bool navigated = event == input::KeyEvent::GoPrevious &&
        (before == GameScreen::CarSelect || before == GameScreen::RivalSelect ||
         before == GameScreen::Results);
    if(navigated)playSound(lets_and_go::SoundCue::Navigate);
    if (_flow.screen() != before || navigated) {
        _screenStartedMs = nowMs;
    }
}

void AppLetsAndGoRacer::onClose()
{
    _audio.reset(); // Synchronize with the audio task before releasing owner memory.
    const auto& garageStats = _garageBudget.stats();
    const auto& raceStats = _raceBudget.stats();
    mclog::tagInfo(getAppInfo().name,
                   "render budget garage: frames={}, peak={}ms, detail={}, transitions={}",
                   garageStats.frameCount, garageStats.peakRenderMs,
                   lets_and_go::pencilDetailLabel(_garageBudget.detail()),
                   garageStats.detailTransitions);
    mclog::tagInfo(getAppInfo().name,
                   "render budget race: frames={}, peak={}ms, detail={}, transitions={}",
                   raceStats.frameCount, raceStats.peakRenderMs,
                   lets_and_go::pencilDetailLabel(_raceBudget.detail()),
                   raceStats.detailTransitions);
    mclog::tagInfo(getAppInfo().name, "on close");
    _keys.reset();
    if (_racerInput) _racerInput->close();
    _racerInput.reset();
    _menuAxis.reset();
    _renderer.close();
    _garageNavigation.reset();
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
    _garageBudget.reset();
    _raceBudget.reset();
    _lastWallFeedbackMs = 0u;
    _lastClampCount = 0u;
    _feedbackScreen = lets_and_go::GameScreen::InputCheck;
    _feedbackCountdown = 255u;
    _feedbackLap = 0u;
    _feedbackBoost = false;
    _feedbackBrake = false;
    _feedbackWallActive = false;
    GetHAL().startLvglUpdate();
}
