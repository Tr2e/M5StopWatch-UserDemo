#pragma once

#include "../model/game_types.h"

#include <cstdint>

namespace lets_and_go {

enum class GameScreen : uint8_t {
    InputCheck,
    InputCalibration,
    CarSelect,
    CarShowcase,
    RivalSelect,
    TrackSelect,
    GridIntro,
    Countdown,
    Racing,
    Paused,
    Finish,
    Results,
    ExitRequested,
};

struct RaceSetup {
    CarId playerCar = CarId::CycloneMagnum;
    TrackId track = TrackId::SkyLoop;
    uint8_t rivalMask = 0;
    bool playerConfirmed = false;

    constexpr bool hasRival(CarId car) const
    {
        return (rivalMask & carMask(car)) != 0u;
    }

    uint8_t rivalCount() const;
};

class GameFlow {
public:
    void reset();

    constexpr GameScreen screen() const { return _screen; }
    constexpr const RaceSetup& setup() const { return _setup; }

    bool confirmInputAvailable();
    bool completeCalibration(bool succeeded);
    bool selectPlayerCar(CarId car);
    bool confirmPlayerCar();
    bool completeCarShowcase();
    bool toggleRival(CarId car);
    bool confirmRivals();
    bool selectTrack(TrackId track);
    bool confirmTrack();
    bool completeGridIntro();
    bool completeCountdown();
    bool togglePause();
    bool finishRace();
    bool showResults();
    bool retrySameRace();
    bool backToGarage();
    bool back();
    void requestExit();

private:
    bool transition(GameScreen expected, GameScreen next);
    void clearRivals();

    GameScreen _screen = GameScreen::InputCheck;
    RaceSetup _setup{};
};

const char* gameScreenLabel(GameScreen screen);

}  // namespace lets_and_go
