#include "../main/apps/app_lets_and_go_racer/controller/garage_selection.h"

#include <iostream>

namespace {
using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool enterCarSelect(GameFlow& flow)
{
    return flow.confirmInputAvailable() && flow.completeCalibration(true);
}

bool validatePlayerWrapAndActivation()
{
    GameFlow flow;
    GarageSelection selection;
    selection.reset();
    bool valid = check(enterCarSelect(flow), "failed to enter car select");
    selection.movePlayer(-1);
    valid &= check(selection.playerCursor() == CarId::BrockenGigant,
                   "player cursor did not wrap backward");
    valid &= check(selection.activatePlayer(flow) &&
                       flow.setup().playerCar == CarId::BrockenGigant &&
                       flow.screen() == GameScreen::CarShowcase,
                   "player activation failed");
    return valid;
}

bool validateRivalSkipToggleAndDone()
{
    GameFlow flow;
    GarageSelection selection;
    selection.reset(CarId::HurricaneSonic);
    bool valid = check(enterCarSelect(flow) &&
                           flow.selectPlayerCar(CarId::HurricaneSonic) &&
                           flow.confirmPlayerCar() && flow.completeCarShowcase(),
                       "failed to enter rival select");
    selection.syncPlayer(CarId::HurricaneSonic);
    valid &= check(selection.rivalCursorCar() != CarId::HurricaneSonic,
                   "rival cursor rested on player car");
    const CarId stationary = selection.rivalCursorCar();
    selection.moveRival(0, flow.setup().playerCar);
    valid &= check(selection.rivalCursorCar() == stationary,
                   "zero direction unexpectedly moved rival cursor");
    valid &= check(selection.activateRival(flow) && flow.setup().rivalCount() == 1,
                   "rival toggle failed");
    valid &= check(selection.cancelRival(flow) && flow.setup().rivalCount() == 0 &&
                       flow.screen() == GameScreen::RivalSelect,
                   "cancel did not remove the selected rival in place");
    valid &= check(selection.cancelRival(flow) &&
                       flow.screen() == GameScreen::CarSelect,
                   "cancel on an unselected rival did not return to car select");
    valid &= check(flow.confirmPlayerCar() && flow.completeCarShowcase(),
                   "failed to re-enter rival select after cancel test");
    selection.syncPlayer(CarId::HurricaneSonic);
    valid &= check(selection.activateRival(flow) && flow.setup().rivalCount() == 1,
                   "rival could not be selected after re-entering selection");
    for (int step = 0; step < 5 && !selection.rivalCursorIsDone(); ++step) {
        selection.moveRival(1, flow.setup().playerCar);
    }
    valid &= check(selection.rivalCursorIsDone(), "rival cursor never reached done");
    valid &= check(selection.activateRival(flow) &&
                       flow.screen() == GameScreen::TrackSelect,
                   "done item did not confirm rivals");
    return valid;
}

bool validateSoloDoneReachable()
{
    GameFlow flow;
    GarageSelection selection;
    selection.reset(CarId::CycloneMagnum);
    bool valid = check(enterCarSelect(flow) &&
                           flow.selectPlayerCar(CarId::CycloneMagnum) &&
                           flow.confirmPlayerCar() && flow.completeCarShowcase(),
                       "failed to prepare solo selection");
    for (int step = 0; step < 5 && !selection.rivalCursorIsDone(); ++step) {
        selection.moveRival(-1, flow.setup().playerCar);
    }
    valid &= check(selection.rivalCursorIsDone() && selection.activateRival(flow) &&
                       flow.setup().rivalCount() == 0,
                   "solo done path failed");
    return valid;
}
}  // namespace

int main()
{
    return validatePlayerWrapAndActivation() && validateRivalSkipToggleAndDone() &&
                   validateSoloDoneReachable()
               ? 0
               : 1;
}
