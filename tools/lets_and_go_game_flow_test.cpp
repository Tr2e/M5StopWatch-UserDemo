#include "../main/apps/app_lets_and_go_racer/controller/game_flow.h"

#include <iostream>

namespace {

using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool advanceToRivalSelection(GameFlow& flow, CarId player)
{
    return flow.confirmInputAvailable() && flow.completeCalibration(true) &&
           flow.selectPlayerCar(player) && flow.confirmPlayerCar() &&
           flow.completeCarShowcase();
}

bool validateFullRaceAndRetry()
{
    GameFlow flow;
    bool valid = check(flow.screen() == GameScreen::InputCheck,
                       "flow did not start at input check");
    valid &= check(advanceToRivalSelection(flow, CarId::CycloneMagnum),
                   "failed to reach rival selection");
    valid &= check(!flow.toggleRival(CarId::CycloneMagnum),
                   "player car was accepted as a rival");
    valid &= check(flow.toggleRival(CarId::HurricaneSonic) &&
                       flow.toggleRival(CarId::NeoTridaggerZmc),
                   "failed to select two rivals");
    valid &= check(flow.setup().rivalCount() == 2 &&
                       !flow.toggleRival(CarId::BrockenGigant) &&
                       !flow.setup().hasRival(CarId::BrockenGigant),
                   "third rival was accepted despite the device cap");
    valid &= check(flow.toggleRival(CarId::NeoTridaggerZmc) &&
                       flow.toggleRival(CarId::BrockenGigant) && flow.setup().rivalCount()==2,
                   "full roster could not remove and replace a rival");
    valid &= check(flow.confirmRivals() &&
                       flow.moveTrack(1) && flow.setup().track==TrackId::TriCross &&
                       flow.moveTrack(1) && flow.setup().track==TrackId::SkyLoop &&
                       flow.moveTrack(-1) && flow.setup().track==TrackId::TriCross &&
                       !flow.selectTrack(TrackId::Count) && flow.confirmTrack() &&
                       flow.completeGridIntro() && flow.completeCountdown(),
                   "failed to reach racing state");
    valid &= check(flow.screen() == GameScreen::Racing,
                   "countdown did not enter racing");
    valid &= check(!flow.moveTrack(1) && flow.setup().track==TrackId::TriCross,"track changed during race");
    valid &= check(flow.togglePause() && flow.screen() == GameScreen::Paused &&
                       !flow.back() && flow.screen() == GameScreen::Paused &&
                       flow.togglePause() && flow.screen() == GameScreen::Racing,
                   "pause round trip failed");
    valid &= check(flow.finishRace() && flow.showResults(),
                   "race did not reach results");
    const RaceSetup beforeRetry = flow.setup();
    valid &= check(flow.retrySameRace() && flow.screen() == GameScreen::GridIntro,
                   "retry did not return to grid intro");
    valid &= check(flow.setup().playerCar == beforeRetry.playerCar &&
                       flow.setup().track == beforeRetry.track &&
                       flow.setup().rivalMask == beforeRetry.rivalMask,
                   "retry changed the selected race setup");
    return valid;
}

bool validateSoloAndGarageReset()
{
    GameFlow flow;
    bool valid = check(advanceToRivalSelection(flow, CarId::HurricaneSonic),
                       "failed to prepare solo race");
    valid &= check(flow.confirmRivals() && flow.setup().rivalCount() == 0,
                   "zero-rival mode was rejected");
    valid &= check(flow.confirmTrack() && flow.completeGridIntro() &&
                       flow.completeCountdown() && flow.finishRace() &&
                       flow.showResults(),
                   "solo race did not finish");
    valid &= check(flow.backToGarage() && flow.screen() == GameScreen::CarSelect,
                   "back to garage failed");
    valid &= check(flow.setup().rivalCount() == 0 &&
                       !flow.setup().playerConfirmed,
                   "garage reset retained transient selection state");
    return valid;
}

bool validateInvalidTransitionsAndBackPaths()
{
    GameFlow flow;
    bool valid = check(!flow.completeCountdown() && !flow.finishRace() &&
                           !flow.confirmPlayerCar(),
                       "out-of-order transition was accepted");
    valid &= check(flow.confirmInputAvailable() &&
                       !flow.completeCalibration(false) &&
                       flow.screen() == GameScreen::InputCalibration,
                   "failed calibration advanced the flow");
    valid &= check(flow.back() && flow.screen() == GameScreen::InputCheck,
                   "calibration back path failed");
    flow.requestExit();
    valid &= check(flow.screen() == GameScreen::ExitRequested && !flow.back(),
                   "exit request was not terminal");
    flow.reset();
    valid &= check(flow.screen() == GameScreen::InputCheck &&
                       flow.setup().rivalCount() == 0,
                   "reset did not restore initial state");
    return valid;
}

bool validateRivalToggleAndPlayerChange()
{
    GameFlow flow;
    bool valid = check(advanceToRivalSelection(flow, CarId::NeoTridaggerZmc),
                       "failed to enter rival selection");
    valid &= check(flow.toggleRival(CarId::CycloneMagnum) &&
                       flow.toggleRival(CarId::CycloneMagnum) &&
                       flow.setup().rivalCount() == 0,
                   "rival toggle did not remove an existing rival");
    valid &= check(flow.toggleRival(CarId::BrockenGigant) && flow.back() &&
                       flow.screen() == GameScreen::CarSelect,
                   "rival selection back path failed");
    valid &= check(flow.setup().rivalCount() == 0 &&
                       flow.selectPlayerCar(CarId::BrockenGigant),
                   "player change retained incompatible rivals");
    valid &= check(!flow.selectPlayerCar(static_cast<CarId>(99)),
                   "invalid player car was accepted");
    return valid;
}

}  // namespace

int main()
{
    GameFlow inspection;
    if(!check(!inspection.inspectCar() && inspection.useDeviceControls() && inspection.inspectCar() &&
              inspection.screen()==GameScreen::CarInspect && !inspection.confirmPlayerCar() &&
              !inspection.completeCarShowcase() && inspection.back() &&
              inspection.screen()==GameScreen::CarSelect && !inspection.setup().playerConfirmed,
              "inspection changed selection or auto-advanced"))return 1;
    return validateFullRaceAndRetry() && validateSoloAndGarageReset() &&
                   validateInvalidTransitionsAndBackPaths() &&
                   validateRivalToggleAndPlayerChange()
               ? 0
               : 1;
}
