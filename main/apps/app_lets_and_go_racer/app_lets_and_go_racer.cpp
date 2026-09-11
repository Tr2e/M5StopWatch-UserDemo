#include "app_lets_and_go_racer.h"
#include "lets_and_go_config.h"
#include "../common/performance/display_frame_scope.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

#include <algorithm>
#include <esp_timer.h>

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
    GetHAL().stopLvglUpdate();
    GetHAL().lvglLock(); // Wait for a touch read already in flight on LVGL.
    GetHAL().lvglUnlock();
    _deviceControls = false;
    _renderDirty = true;
    _inspectionPresentation.reset();
    _inspectionFrames.reset();
    _raceFrames.reset();
    _perfStartedMs = GetHAL().millis();
    _perfFrames = _perfPeakUs = 0;
    _perfDrawUs = _perfPresentUs = _perfInputUs = 0;
    // Match Vector Run's external-controller startup sequence. PORT.A's red
    // wire is supplied by the PMIC-controlled 5VINOUT rail, so the Joystick2
    // probe must not start until that rail has had time to settle.
    _externalPower = GetHAL().setGrove5VPower(true);
    GetHAL().delay(20);
    if (_externalPower) {
        _racerInput = std::make_unique<lets_and_go::HardwareRacerInputProvider>();
        _racerInput->open();
    }
    _menuAxis.reset();
    _garageBudget.reset();
    _raceBudget.reset();
    _resultsSelection.reset();
    _progress = lets_and_go::RaceProgressStore::load();
    _flow.reset();
    _deviceInput.open();
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
    _feedbackWallActive = false;
    const auto& feedbackConfig = GetHAL().getButtonConfig(true);
    _feedbackVibrateEnabled = feedbackConfig.vibrateEnabled;
    _screenStartedMs = _lastUpdateMs;
    GetHAL().stopLvglUpdate();
    auto& display = GetHAL().getDisplay();
    _directFrameBuffer = lets_and_go::tuning::kDirectFramebuffer &&
                         GetHAL().hasDisplayFrameBuffer();
    _renderer.open(display.width(), display.height());
    _raceRenderer.open(display.width(), display.height(), true, true, true, false);
    _raceRenderer.setEdgeUpscale(true);
    if (_directFrameBuffer) {
        app_performance::DisplayFrameScope frame(display);
        _renderer.render(display,_flow,_selection,0u,lets_and_go::PencilDetail::High);
        frame.finish();
    } else {
        _renderer.render(_flow, _selection, 0u, lets_and_go::PencilDetail::High);
        GetHAL().updateCanvas();
    }
    mclog::tagInfo(getAppInfo().name,"render backend: {}",
                   _directFrameBuffer ? "DISPLAY_FRAMEBUFFER" : "CANVAS_FALLBACK");
    _deviceInput.presentScreen(_flow.screen());
    if (_racerInput) _racerInput->presentScreen();
}

void AppLetsAndGoRacer::onRunning()
{
    const uint64_t inputStartedUs = esp_timer_get_time();
    const uint32_t nowMs = GetHAL().millis();
    const auto screenBefore = _flow.screen();
    const auto inspectionBefore = _inspection.state();
    auto device = _deviceInput.sample(nowMs);
    if (screenBefore==lets_and_go::GameScreen::CarInspect) {
        if (device.input.valid && device.preview.changed) {
            if (_inspection.drag(device.preview)) _renderDirty=true;
        } else if (!device.input.valid) _inspection.release();
    }
    if (screenBefore==lets_and_go::GameScreen::CarSelect && device.input.valid) {
        if (device.preview.changed) {
            _garageView.drag(device.preview,nowMs);
            _renderDirty=true;
        }
    } else if (_garageView.dragging()) {
        _garageView.endDrag(nowMs);_renderDirty=true;
    }
    if (!_deviceControls && device.input.confirmPressed && device.input.valid &&
        _flow.useDeviceControls()) {
        if (_racerInput) _racerInput->close();
        _racerInput.reset();
        if (_externalPower) GetHAL().setGrove5VPower(false);
        _externalPower = false;
        _deviceControls = true;
        _screenStartedMs = nowMs;
        device = {}; // The mode-selection tap cannot also select a car.
        mclog::tagInfo(getAppInfo().name, "controls: DEVICE (buttons + touch)");
    }
    const float deltaSeconds =
        std::min<uint32_t>(nowMs - _lastUpdateMs, 250u) * 0.001f;
    _lastUpdateMs = nowMs;
    lets_and_go::RacerInput racerInput = _deviceControls ? device.input :
        (_racerInput ? _racerInput->sample(nowMs) : lets_and_go::RacerInput{});
    const lets_and_go::RacerInputStatus racerStatus =
        _racerInput ? _racerInput->status(nowMs) : lets_and_go::RacerInputStatus{};
    if (!_deviceControls && _flow.screen() == lets_and_go::GameScreen::InputCheck &&
        racerStatus.axesConnected && racerStatus.actionsConfigured &&
        _flow.confirmInputAvailable()) {
        _racerInput->requestCalibration(nowMs);
        mclog::tagInfo(getAppInfo().name, "controls connected; calibrating");
        _screenStartedMs = nowMs;
    } else if (_flow.screen() == lets_and_go::GameScreen::InputCalibration &&
        racerStatus.ready() && _flow.completeCalibration(true)) {
        mclog::tagInfo(getAppInfo().name, "controls ready; SELECT MACHINE");
        _screenStartedMs = nowMs;
    }
    if (_flow.screen() != screenBefore) {
        // Detection/calibration changed the page after this sample was taken.
        // Do not let an old blue click select a car before the garage appears.
        const bool exit = racerInput.exitPressed;
        racerInput = {};
        racerInput.exitPressed = exit;
    }
    if (screenBefore==lets_and_go::GameScreen::CarSelect && device.input.valid && device.inspect &&
        !racerInput.exitPressed && !racerInput.cancelPressed && !racerInput.pausePressed &&
        !device.input.exitPressed && _flow.inspectCar()) {
        _inspection={};_inspectionRender.reset();_screenStartedMs=nowMs;_renderDirty=true;
        racerInput={}; // Entry cannot replay a queued garage direction or confirmation.
        device={};
    } else if (screenBefore==lets_and_go::GameScreen::CarInspect && device.input.valid) {
        // Touch back/reset remain available with external driving controls.
        racerInput.confirmPressed |= device.input.confirmPressed;
        racerInput.cancelPressed |= device.input.cancelPressed;
    }
    handleRacerInput(racerInput, nowMs, _deviceControls ? device.navigation : 0,
                     _deviceControls ? device.view : 0, _deviceControls && device.advance,
                     device.resultActionFor(screenBefore));
    if (device.input.exitPressed) _flow.requestExit();
    _perfInputUs += esp_timer_get_time() - inputStartedUs;
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
    updateHaptics(racerInput, nowMs);
    if (_flow.screen() != screenBefore) {
        _garageView.endDrag(nowMs);
        _inspection.release();
        _renderDirty = true;
        _deviceInput.setScreen(_flow.screen());
        if (_racerInput) {
            using lets_and_go::GameScreen;
            using lets_and_go::RacerNavigationMode;
            const auto next = _flow.screen();
            _racerInput->setNavigationMode((next == GameScreen::CarSelect || next == GameScreen::CarInspect) ? RacerNavigationMode::Garage :
                next == GameScreen::Results ? RacerNavigationMode::Results :
                (next == GameScreen::RivalSelect || next == GameScreen::TrackSelect)
                    ? RacerNavigationMode::Horizontal : RacerNavigationMode::None);
        }
    }
    using lets_and_go::GameScreen;
    const auto screen = _flow.screen();
    if (screen==GameScreen::CarInspect) {
        const auto pose=_inspection.state();
        const bool changed=screenBefore==GameScreen::CarInspect &&
            (pose.yaw!=inspectionBefore.yaw || pose.pitch!=inspectionBefore.pitch);
        const bool stickHeld=!_deviceControls && racerInput.valid && !racerInput.menuBlocked &&
            (std::abs(racerInput.steer)>.30f || std::abs(racerInput.viewAxis)>.30f);
        if (_inspectionRender.update(nowMs,changed,_inspection.touchActive() || stickHeld)) _renderDirty=true;
    }
    // Draw once after a transition ends so a static view reaches its exact pose.
    const bool garageAnimated = _garageView.animating(nowMs) || _garageView.animating(_lastFrameMs);
    const bool animated = screen == GameScreen::CarShowcase || screen == GameScreen::TrackSelect ||
        screen == GameScreen::Racing || screen == GameScreen::Countdown || screen == GameScreen::Finish ||
        screen == GameScreen::GridIntro || (screen == GameScreen::CarSelect && garageAnimated);
    const bool statusRefresh = (screen == GameScreen::InputCheck || screen == GameScreen::InputCalibration) &&
                               nowMs - _lastFrameMs >= 250u;
    if ((_renderDirty || animated || statusRefresh) && (_lastFrameMs == 0u ||
        nowMs - _lastFrameMs >= lets_and_go::tuning::kFrameIntervalMs)) {
        _lastFrameMs = nowMs;
        _renderDirty = false;
        const uint64_t renderStartedUs = esp_timer_get_time();
        const bool raceView = usesRaceRenderer(_flow.screen());
        const bool partial = _inspectionPresentation.partial(screen,_selection.playerCursor());
        uint64_t drawFinishedUs = 0;
        if (raceView && _directFrameBuffer) {
            auto& display=GetHAL().getDisplay();
            app_performance::DisplayFrameScope frame(display);
            _raceRenderer.render(display,nullptr,_flow,_race,_resultsSelection,
                                 nowMs-_screenStartedMs,_pausedForInputLoss,
                                 _raceBudget.detail(),_deviceControls);
            drawFinishedUs = esp_timer_get_time();
            frame.finish();
        } else if (raceView) {
            _raceRenderer.render(_flow, _race, _resultsSelection,
                                 nowMs - _screenStartedMs,
                                 _pausedForInputLoss, _raceBudget.detail(), _deviceControls);
            drawFinishedUs = esp_timer_get_time();
            GetHAL().updateCanvas();
        } else if (_directFrameBuffer) {
            auto& display=GetHAL().getDisplay();
            const auto region=screen==GameScreen::CarSelect ?
                lets_and_go::selectionRefreshRegion(display.width()) :
                lets_and_go::inspectionRefreshRegion(display.width());
            const auto renderGarage=[&] {
                _renderer.render(display,_flow,_selection,nowMs-_screenStartedMs,
                    _garageBudget.detail(),racerStatus,
                    screen==GameScreen::CarInspect ? _inspection.state() : _garageView.state(nowMs),_deviceControls,
                    screen==GameScreen::CarInspect ? _inspectionRender.percent() :
                    screen==GameScreen::CarSelect ? _garageView.renderPercent(nowMs) : 100,
                    partial,screen==GameScreen::CarInspect ? _inspectionRender.displayPercent() : 100);
            };
            if (partial) {
                app_performance::DisplayFrameScope frame(display,
                    {region.x,region.y,region.width,region.height});
                renderGarage();
                drawFinishedUs = esp_timer_get_time();
                frame.finish();
            } else {
                app_performance::DisplayFrameScope frame(display);
                renderGarage();
                drawFinishedUs = esp_timer_get_time();
                frame.finish();
            }
        } else {
            _renderer.render(_flow, _selection, nowMs - _screenStartedMs,
                             _garageBudget.detail(), racerStatus,
                             screen==GameScreen::CarInspect ? _inspection.state() : _garageView.state(nowMs), _deviceControls,
                             screen==GameScreen::CarInspect ? _inspectionRender.percent() :
                             screen==GameScreen::CarSelect ? _garageView.renderPercent(nowMs) : 100,
                             partial,screen==GameScreen::CarInspect ? _inspectionRender.displayPercent() : 100);
            drawFinishedUs = esp_timer_get_time();
            if(partial) {
                const auto region=screen==GameScreen::CarSelect ?
                    lets_and_go::selectionRefreshRegion(GetHAL().getCanvas().width()) :
                    lets_and_go::inspectionRefreshRegion(GetHAL().getCanvas().width());
                GetHAL().updateCanvasRegion(region.x,region.y,region.width,region.height);
            } else GetHAL().updateCanvas();
        }
        _inspectionPresentation.presented(screen,_selection.playerCursor());
        if (!raceView) _garageView.presented(nowMs);
        _deviceInput.presentScreen(_flow.screen());
        if (_racerInput) _racerInput->presentScreen();
        const uint64_t presentFinishedUs = esp_timer_get_time();
        if(screen==GameScreen::CarInspect) {
            _inspectionFrames.record(_selection.playerCursor(),_renderer.inspectionPercent(),
                uint32_t(drawFinishedUs-renderStartedUs),uint32_t(presentFinishedUs-drawFinishedUs));
        } else _inspectionFrames.reset();
        if(screen==GameScreen::Racing) {
            _raceFrames.record(_race.snapshot().player().car,100,
                uint32_t(drawFinishedUs-renderStartedUs),uint32_t(presentFinishedUs-drawFinishedUs));
        } else _raceFrames.reset();
        const uint32_t renderUs = presentFinishedUs - renderStartedUs;
        const uint32_t renderMs = (renderUs + 999u) / 1000u;
        _perfDrawUs += drawFinishedUs - renderStartedUs;
        _perfPresentUs += presentFinishedUs - drawFinishedUs;
        _perfPeakUs = std::max(_perfPeakUs, renderUs);
        ++_perfFrames;
        const uint16_t clampCount = _race.prepared()
                                        ? _race.snapshot().simulationClampCount : 0u;
        const bool simulationClamped = clampCount != _lastClampCount;
        _lastClampCount = clampCount;
        if (raceView) {
            _raceBudget.observe(renderMs, simulationClamped);
        } else if (screen!=GameScreen::CarInspect) {
            _garageBudget.observe(renderMs, false);
        }
    }
    if (nowMs - _perfStartedMs >= 2000u) {
        const uint32_t elapsed = nowMs - _perfStartedMs;
        const uint32_t frames = std::max<uint32_t>(1u, _perfFrames);
        mclog::tagInfo("RacerPerf", "screen={} source={} frames={} fps_x10={} draw_us={} present_us={} peak_us={} input_total_us={} detail={}",
            lets_and_go::gameScreenLabel(screen), _deviceControls ? "device" : "external", _perfFrames,
            _perfFrames * 10000u / elapsed, uint32_t(_perfDrawUs / frames),
            uint32_t(_perfPresentUs / frames), _perfPeakUs, uint32_t(_perfInputUs),
            lets_and_go::pencilDetailLabel(screen==GameScreen::CarInspect ? lets_and_go::PencilDetail::High :
                usesRaceRenderer(screen) ? _raceBudget.detail() : _garageBudget.detail()));
        if (screen==GameScreen::CarInspect) {
            mclog::tagInfo("InspectionPerf","car={} interacting={} last_scale_pct={} requested_scale_pct={} requested_display_pct={}",
                lets_and_go::carSpec(_selection.playerCursor()).shortName,_inspectionRender.interacting(),
                _renderer.inspectionPercent(),_inspectionRender.percent(),_inspectionRender.displayPercent());
            const auto recent=_inspectionFrames.summary();
            if(_perfFrames && recent.count)
                mclog::tagInfo("InspectionFrames","window=last64 car={} scale_pct={} n={} draw_us={} present_us={} p95_us={} max_us={}",
                    lets_and_go::carSpec(_selection.playerCursor()).shortName,_inspectionFrames.percent(),
                    recent.count,recent.drawUs,recent.presentUs,recent.p95Us,recent.maxUs);
        }
        if(screen==GameScreen::Racing && _perfFrames) {
            const auto recent=_raceFrames.summary();
            if(recent.count)mclog::tagInfo("RaceFrames","window=last64 car={} track={} n={} draw_us={} present_us={} p95_us={} max_us={}",
                int(_race.snapshot().player().car),int(_race.track().id()),recent.count,
                recent.drawUs,recent.presentUs,recent.p95Us,recent.maxUs);
        }
        _perfStartedMs = nowMs;
        _perfFrames = _perfPeakUs = 0;
        _perfDrawUs = _perfPresentUs = _perfInputUs = 0;
    }
}

void AppLetsAndGoRacer::handleRacerInput(const lets_and_go::RacerInput& input,
                                         uint32_t nowMs, int deviceNavigation, int deviceView, bool deviceAdvance,
                                         int deviceResult)
{
    using lets_and_go::GameScreen;
    if (input.exitPressed) {
        _flow.requestExit();
        return;
    }
    const GameScreen before = _flow.screen();
    if (before==GameScreen::CarInspect) {
        if (input.cancelPressed || input.pausePressed) {
            _flow.back();_screenStartedMs=nowMs;_renderDirty=true;
        } else if (input.confirmPressed) {
            _inspection.reset();_renderDirty=true;
        } else if ((_deviceControls || input.valid) && _inspection.turn(_deviceControls ? deviceNavigation : input.navigationStep,
                                    _deviceControls ? deviceView : input.viewStep)) {
            _renderDirty=true;
        }
        return;
    }
    const bool resultTap=before==GameScreen::Results && deviceResult>=0 &&
                         deviceResult<int(lets_and_go::ResultAction::Count);
    if (input.confirmPressed || input.cancelPressed || input.pausePressed || deviceNavigation || deviceView || deviceAdvance || resultTap)
        _renderDirty = true;
    if (input.pausePressed &&
        (before == GameScreen::Racing || before == GameScreen::Paused)) {
        if (before == GameScreen::Paused && !input.valid) return;
        if (_flow.togglePause()) {
            _race.setPaused(_flow.screen() == GameScreen::Paused);
            _pausedForInputLoss = false;
            _screenStartedMs = nowMs;
        }
        return;
    }
    const bool acceptsNavigation = before == GameScreen::CarSelect ||
                                   before == GameScreen::TrackSelect ||
                                   before == GameScreen::RivalSelect ||
                                   before == GameScreen::Results;
    if (!acceptsNavigation) _menuAxis.reset();
    int navigation=0;
    if (_deviceControls) {
        _menuAxis.reset();
        _garageNavigation.reset();
        navigation = acceptsNavigation ? deviceNavigation : 0;
        if (before == GameScreen::CarSelect && deviceView) {
            _garageView.changeView(deviceView, nowMs);
        }
    } else {
        navigation = acceptsNavigation ? input.navigationStep : 0;
        if (before == GameScreen::CarSelect && input.viewStep) {
            _renderDirty = true;
            _garageView.changeView(input.viewStep, nowMs);
        }
    }
    if (navigation != 0) {
        _renderDirty = true;
        if (before == GameScreen::CarSelect) {
            _selection.movePlayer(navigation);
        } else if (before == GameScreen::RivalSelect) {
            _selection.moveRival(navigation, _flow.setup().playerCar);
        } else if (before == GameScreen::Results) {
            _resultsSelection.move(navigation);
        } else if (before == GameScreen::TrackSelect) {
            _flow.moveTrack(navigation);
        }
    }
    if (input.cancelPressed || input.pausePressed) {
        if (_deviceControls && before == GameScreen::CarSelect) { _flow.requestExit(); return; }
        const bool changed = before == GameScreen::RivalSelect && !input.pausePressed && !_deviceControls
                                 ? _selection.cancelRival(_flow)
                                 : _flow.back();
        if (changed && before == GameScreen::Results) {
            _raceSeed = 0u;
            _selection.reset(_progress.lastCar);
        }
    } else if (deviceAdvance && before == GameScreen::RivalSelect) {
        _flow.confirmRivals();
    } else if (input.confirmPressed || resultTap) {
        if(resultTap)_resultsSelection.select(static_cast<lets_and_go::ResultAction>(deviceResult));
        switch (before) {
            case GameScreen::CarSelect:
                if (_selection.activatePlayer(_flow)) {
                    persistSelectedCar();
                }
                break;
            case GameScreen::RivalSelect:
                _selection.activateRival(_flow);
                break;
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
    if (_flow.screen() != before || navigation != 0) {
        _screenStartedMs = nowMs;
        mclog::tagInfo(getAppInfo().name, "input: {} -> {} navigation={}",
                       lets_and_go::gameScreenLabel(before),
                       lets_and_go::gameScreenLabel(_flow.screen()), navigation);
    }
}

void AppLetsAndGoRacer::prepareRace(uint32_t nowMs)
{
    _raceFrames.reset();
    _feedbackLap=0;
    _feedbackBoost=false;
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

void AppLetsAndGoRacer::updateHaptics(const lets_and_go::RacerInput& input,
                                     uint32_t nowMs)
{
    using lets_and_go::GameScreen;
    const auto vibrateCue = [this](const lets_and_go::tuning::FeedbackCue& cue) {
        vibrateFeedback(cue.vibrationStrength, cue.vibrationDurationMs);
    };
    const GameScreen screen = _flow.screen();
    if (screen == GameScreen::Countdown) {
        const uint8_t tick = static_cast<uint8_t>(
            std::min<uint32_t>(2u, (nowMs - _screenStartedMs) / 1000u));
        if (tick != _feedbackCountdown) {
            _feedbackCountdown = tick;
            vibrateCue(lets_and_go::tuning::kCountdownCue);
        }
    } else {
        _feedbackCountdown = 255u;
    }
    if (screen != _feedbackScreen) {
        if (screen == GameScreen::Racing && _feedbackScreen == GameScreen::Countdown) {
            vibrateCue(lets_and_go::tuning::kGoCue);
        } else if (screen == GameScreen::Finish) {
            vibrateCue(lets_and_go::tuning::kFinishCue);
        }
        _feedbackScreen = screen;
    }
    if (screen == GameScreen::Racing && _race.prepared()) {
        const auto& player = _race.snapshot().player();
        const bool boosting = input.valid && input.boostHeld && !input.brakeHeld &&
                              player.motion.boostCharge > 0.02f;
        if (boosting && !_feedbackBoost) {
            vibrateCue(lets_and_go::tuning::kBoostCue);
        }
        _feedbackBoost = boosting;
        const bool wallHit = player.motion.wallImpact > 0.75f;
        if (wallHit && !_feedbackWallActive &&
            (_lastWallFeedbackMs == 0u || nowMs - _lastWallFeedbackMs >= 300u)) {
            _lastWallFeedbackMs = nowMs;
            vibrateCue(lets_and_go::tuning::kWallCue);
        }
        _feedbackWallActive = wallHit;
        if (player.completedLaps != _feedbackLap) {
            _feedbackLap = player.completedLaps;
            if (_feedbackLap == 2u) {
                vibrateCue(lets_and_go::tuning::kFinalLapCue);
            }
        }
    } else {
        _feedbackBoost = false;
        _feedbackWallActive = false;
    }
}

void AppLetsAndGoRacer::vibrateFeedback(uint8_t strength, uint16_t durationMs)
{
    if (_feedbackVibrateEnabled) GetHAL().vibrate(durationMs, strength);
}

void AppLetsAndGoRacer::onClose()
{
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
    _deviceInput.close();
    if (_racerInput) _racerInput->close();
    _racerInput.reset();
    if (_externalPower) GetHAL().setGrove5VPower(false);
    _externalPower = false;
    _directFrameBuffer = false;
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
    _feedbackWallActive = false;
    GetHAL().startLvglUpdate();
}
