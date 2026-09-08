#include "../main/apps/app_lets_and_go_racer/controller/results_selection.h"
#include "../main/apps/app_lets_and_go_racer/model/player_progress.h"

#include <iostream>

namespace {
using namespace lets_and_go;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

GameFlow resultsFlow()
{
    GameFlow flow;
    flow.confirmInputAvailable();
    flow.completeCalibration(true);
    flow.confirmPlayerCar();
    flow.completeCarShowcase();
    flow.confirmRivals();
    flow.confirmTrack();
    flow.completeGridIntro();
    flow.completeCountdown();
    flow.finishRace();
    flow.showResults();
    return flow;
}

bool validateResultActions()
{
    ResultsSelection selection;
    GameFlow retry = resultsFlow();
    bool valid = check(selection.activate(retry) && retry.screen() == GameScreen::GridIntro,
                       "retry action failed");
    selection.move(1);
    GameFlow garage = resultsFlow();
    valid &= check(selection.cursor() == ResultAction::Garage &&
                       selection.activate(garage) && garage.screen() == GameScreen::CarSelect,
                   "garage action failed");
    selection.move(1);
    GameFlow exit = resultsFlow();
    valid &= check(selection.activate(exit) && exit.screen() == GameScreen::ExitRequested,
                   "exit action failed");
    selection.move(1);
    valid &= check(selection.cursor() == ResultAction::Retry, "result menu did not wrap");
    return valid;
}

bool validateProgress()
{
    PlayerProgress progress;
    bool valid = check(recordBestLap(progress, TrackId::SkyLoop, 12.345f) &&
                           progress.bestLapMilliseconds[0] == 12345u,
                       "first best lap was not recorded");
    valid &= check(!recordBestLap(progress, TrackId::SkyLoop, 13.0f) &&
                       recordBestLap(progress, TrackId::SkyLoop, 11.0f),
                   "best lap replacement rule failed");
    progress.lastCar = static_cast<CarId>(99u);
    const PlayerProgress repaired = sanitizePlayerProgress(progress);
    valid &= check(repaired.lastCar == CarId::CycloneMagnum &&
                       repaired.bestLapMilliseconds[0] == 11000u,
                   "invalid car repair erased a valid best lap");
    progress.version = 99u;
    valid &= check(sanitizePlayerProgress(progress).lastCar == CarId::CycloneMagnum &&
                       sanitizePlayerProgress(progress).bestLapMilliseconds[0] == 0u,
                   "corrupt progress did not reset safely");
    struct Legacy { uint8_t version=1; CarId lastCar=CarId::HurricaneSonic; uint32_t lap=12345; } legacy;
    auto migrated=decodePlayerProgress(&legacy,sizeof(legacy));
    valid &= check(migrated.lastCar==legacy.lastCar && migrated.bestLapMilliseconds[0]==12345 &&
                   migrated.bestLapMilliseconds[1]==0,"V1 record migration failed");
    valid &= check(recordBestLap(migrated,TrackId::TriCross,20.f) && migrated.bestLapMilliseconds[0]==12345 &&
                   migrated.bestLapMilliseconds[1]==20000,"track records are not independent");
    auto reloaded=decodePlayerProgress(&migrated,sizeof(migrated));
    valid &= check(reloaded.bestLapMilliseconds==migrated.bestLapMilliseconds,"V2 roundtrip failed");
    migrated.lastCar=CarId::Diospada;
    valid &= check(decodePlayerProgress(&migrated,sizeof(migrated)).lastCar==CarId::Diospada,
                   "eighth car does not survive progress roundtrip");
    valid &= check(decodePlayerProgress(&legacy,sizeof(legacy)-1).bestLapMilliseconds[0]==0,
                   "truncated legacy save accepted");
    return valid;
}
}  // namespace

int main()
{
    return validateResultActions() && validateProgress() ? 0 : 1;
}
