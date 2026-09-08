#pragma once

#include "controller/game_flow.h"
#include "controller/garage_selection.h"
#include "controller/race_controller.h"
#include "controller/results_selection.h"
#include "input/hardware_racer_input_provider.h"
#include "input/racer_input_logic.h"
#include "view/garage_renderer.h"
#include "view/race_renderer.h"
#include "model/race_progress_store.h"
#include "audio/racer_audio.h"

#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppLetsAndGoRacer : public mooncake::AppAbility {
public:
    AppLetsAndGoRacer();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    void handleKey(input::KeyEvent event, uint32_t nowMs);
    void handleRacerInput(const lets_and_go::RacerInput& input, uint32_t nowMs);
    void prepareRace(uint32_t nowMs);
    void persistSelectedCar();
    void updateFeedback(const lets_and_go::RacerInput& input, uint32_t nowMs);
    void playSound(lets_and_go::SoundCue cue);
    void vibrateFeedback(uint8_t strength, uint16_t durationMs);

    std::unique_ptr<input::KeyManager> _keys;
    std::unique_ptr<lets_and_go::RacerAudio> _audio;
    std::unique_ptr<lets_and_go::RacerInputProvider> _racerInput;
    lets_and_go::MenuAxisRepeater _menuAxis;
    lets_and_go::GameFlow _flow;
    lets_and_go::GarageSelection _selection;
    lets_and_go::RaceController _race;
    lets_and_go::ResultsSelection _resultsSelection;
    lets_and_go::PlayerProgress _progress;
    lets_and_go::GarageRenderer _renderer;
    lets_and_go::RaceRenderer _raceRenderer;
    lets_and_go::FrameBudgetController _garageBudget;
    lets_and_go::FrameBudgetController _raceBudget;
    uint32_t _lastFrameMs = 0;
    uint32_t _screenStartedMs = 0;
    uint32_t _lastUpdateMs = 0;
    uint32_t _raceSeed = 0;
    uint32_t _inputInvalidSinceMs = 0;
    uint32_t _lastWallFeedbackMs = 0;
    uint16_t _lastClampCount = 0;
    lets_and_go::GameScreen _feedbackScreen = lets_and_go::GameScreen::InputCheck;
    uint8_t _feedbackCountdown = 255u;
    uint8_t _feedbackLap = 0u;
    bool _feedbackBoost = false;
    bool _feedbackBrake = false;
    bool _feedbackWallActive = false;
    bool _feedbackSfxEnabled = true;
    bool _feedbackVibrateEnabled = true;
    bool _pausedForInputLoss = false;
};
