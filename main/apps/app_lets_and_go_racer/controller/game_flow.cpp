#include "game_flow.h"

namespace lets_and_go {

uint8_t RaceSetup::rivalCount() const
{
    uint8_t value = rivalMask;
    uint8_t count = 0;
    while (value != 0u) {
        count += static_cast<uint8_t>(value & 1u);
        value >>= 1u;
    }
    return count;
}

void GameFlow::reset()
{
    _screen = GameScreen::InputCheck;
    _setup = {};
}

bool GameFlow::transition(GameScreen expected, GameScreen next)
{
    if (_screen != expected) return false;
    _screen = next;
    return true;
}

void GameFlow::clearRivals()
{
    _setup.rivalMask = 0u;
}

bool GameFlow::confirmInputAvailable()
{
    return transition(GameScreen::InputCheck, GameScreen::InputCalibration);
}

bool GameFlow::completeCalibration(bool succeeded)
{
    if (_screen != GameScreen::InputCalibration || !succeeded) return false;
    _screen = GameScreen::CarSelect;
    return true;
}

bool GameFlow::selectPlayerCar(CarId car)
{
    if (_screen != GameScreen::CarSelect || !isValidCar(car)) return false;
    if (_setup.playerCar != car) clearRivals();
    _setup.playerCar = car;
    _setup.playerConfirmed = false;
    return true;
}

bool GameFlow::confirmPlayerCar()
{
    if (_screen != GameScreen::CarSelect || !isValidCar(_setup.playerCar)) {
        return false;
    }
    _setup.playerConfirmed = true;
    _screen = GameScreen::CarShowcase;
    return true;
}

bool GameFlow::completeCarShowcase()
{
    return transition(GameScreen::CarShowcase, GameScreen::RivalSelect);
}

bool GameFlow::toggleRival(CarId car)
{
    if (_screen != GameScreen::RivalSelect || !isValidCar(car) ||
        car == _setup.playerCar) {
        return false;
    }
    const uint8_t mask = carMask(car);
    if ((_setup.rivalMask & mask) != 0u) {
        _setup.rivalMask &= static_cast<uint8_t>(~mask);
        return true;
    }
    if (_setup.rivalCount() >= kMaximumRivals) return false;
    _setup.rivalMask |= mask;
    return true;
}

bool GameFlow::confirmRivals()
{
    if (_screen != GameScreen::RivalSelect ||
        _setup.hasRival(_setup.playerCar) ||
        _setup.rivalCount() > kMaximumRivals) {
        return false;
    }
    _screen = GameScreen::TrackSelect;
    return true;
}

bool GameFlow::selectTrack(TrackId track)
{
    if (_screen != GameScreen::TrackSelect || !isValidTrack(track)) return false;
    _setup.track = track;
    return true;
}

bool GameFlow::confirmTrack()
{
    if (_screen != GameScreen::TrackSelect || !isValidTrack(_setup.track)) return false;
    _screen = GameScreen::GridIntro;
    return true;
}

bool GameFlow::completeGridIntro()
{
    return transition(GameScreen::GridIntro, GameScreen::Countdown);
}

bool GameFlow::completeCountdown()
{
    return transition(GameScreen::Countdown, GameScreen::Racing);
}

bool GameFlow::togglePause()
{
    if (_screen == GameScreen::Racing) {
        _screen = GameScreen::Paused;
        return true;
    }
    if (_screen == GameScreen::Paused) {
        _screen = GameScreen::Racing;
        return true;
    }
    return false;
}

bool GameFlow::finishRace()
{
    return transition(GameScreen::Racing, GameScreen::Finish);
}

bool GameFlow::showResults()
{
    return transition(GameScreen::Finish, GameScreen::Results);
}

bool GameFlow::retrySameRace()
{
    return transition(GameScreen::Results, GameScreen::GridIntro);
}

bool GameFlow::backToGarage()
{
    if (_screen != GameScreen::Results) return false;
    clearRivals();
    _setup.playerConfirmed = false;
    _screen = GameScreen::CarSelect;
    return true;
}

bool GameFlow::back()
{
    switch (_screen) {
        case GameScreen::InputCalibration:
            _screen = GameScreen::InputCheck;
            return true;
        case GameScreen::CarSelect:
            _screen = GameScreen::InputCheck;
            return true;
        case GameScreen::CarShowcase:
            _setup.playerConfirmed = false;
            _screen = GameScreen::CarSelect;
            return true;
        case GameScreen::RivalSelect:
            clearRivals();
            _setup.playerConfirmed = false;
            _screen = GameScreen::CarSelect;
            return true;
        case GameScreen::TrackSelect:
            _screen = GameScreen::RivalSelect;
            return true;
        // Pause/resume must go through togglePause so the controller stays in sync.
        case GameScreen::Results:
            return backToGarage();
        default:
            return false;
    }
}

void GameFlow::requestExit()
{
    _screen = GameScreen::ExitRequested;
}

const char* gameScreenLabel(GameScreen screen)
{
    switch (screen) {
        case GameScreen::InputCheck: return "INPUT CHECK";
        case GameScreen::InputCalibration: return "CALIBRATION";
        case GameScreen::CarSelect: return "SELECT MACHINE";
        case GameScreen::CarShowcase: return "MACHINE READY";
        case GameScreen::RivalSelect: return "SELECT RIVALS";
        case GameScreen::TrackSelect: return "SELECT COURSE";
        case GameScreen::GridIntro: return "STARTING GRID";
        case GameScreen::Countdown: return "COUNTDOWN";
        case GameScreen::Racing: return "RACING";
        case GameScreen::Paused: return "PAUSED";
        case GameScreen::Finish: return "FINISH";
        case GameScreen::Results: return "RESULTS";
        case GameScreen::ExitRequested: return "EXIT";
    }
    return "UNKNOWN";
}

}  // namespace lets_and_go
