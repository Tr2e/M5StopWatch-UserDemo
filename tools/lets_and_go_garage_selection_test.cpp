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
    valid &= check(selection.playerCursor() == CarId::Diospada,
                   "player cursor did not wrap backward");
    valid &= check(selection.activatePlayer(flow) &&
                       flow.setup().playerCar == CarId::Diospada &&
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
    for (std::size_t step = 0; step <= kCarCount && !selection.rivalCursorIsDone(); ++step) {
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
    for (std::size_t step = 0; step <= kCarCount && !selection.rivalCursorIsDone(); ++step) {
        selection.moveRival(-1, flow.setup().playerCar);
    }
    valid &= check(selection.rivalCursorIsDone() && selection.activateRival(flow) &&
                       flow.setup().rivalCount() == 0,
                   "solo done path failed");
    return valid;
}

bool validateExpandedRoster()
{
    bool valid=true;
    for(std::size_t player=0;player<kCarCount;++player) {
        const auto id=static_cast<CarId>(player);
        GameFlow flow;GarageSelection selection;selection.reset(id);
        enterCarSelect(flow);selection.activatePlayer(flow);flow.completeCarShowcase();selection.syncPlayer(id);
        unsigned visited=0;bool ready=false;
        for(std::size_t i=0;i<kCarCount;++i) {
            if(selection.rivalCursorIsDone())ready=true;
            else {
                const auto candidate=selection.rivalCursorCar();visited|=carMask(candidate);
                valid &= check(candidate!=id,"player offered as rival");
            }
            selection.moveRival(1,id);
        }
        valid &= check(ready && visited==(255u ^ carMask(id)),"cannot reach all seven rivals and READY");
        unsigned added=0;
        for(std::size_t rival=0;rival<kCarCount;++rival)if(rival!=player) {
            const bool changed=flow.toggleRival(static_cast<CarId>(rival));
            valid &= check(changed==(added<kMaximumRivals),"rival limit not enforced");++added;
        }
        valid &= check(flow.setup().rivalCount()==kMaximumRivals,"expanded roster changed grid capacity");
    }
    return valid;
}
}  // namespace

int main()
{
    return validatePlayerWrapAndActivation() && validateRivalSkipToggleAndDone() &&
                   validateSoloDoneReachable() && validateExpandedRoster()
               ? 0
               : 1;
}
