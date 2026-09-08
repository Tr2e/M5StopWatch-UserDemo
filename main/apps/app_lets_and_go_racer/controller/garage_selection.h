#pragma once

#include "game_flow.h"

#include <cstddef>
#include <cstdint>

namespace lets_and_go {

class GarageSelection {
public:
    static constexpr std::size_t kRivalDoneIndex = kCarCount;

    void reset(CarId player = CarId::CycloneMagnum);
    void syncPlayer(CarId player);
    void movePlayer(int direction);
    void moveRival(int direction, CarId player);
    bool activatePlayer(GameFlow& flow);
    bool activateRival(GameFlow& flow);
    bool cancelRival(GameFlow& flow);

    constexpr CarId playerCursor() const
    {
        return static_cast<CarId>(_playerIndex);
    }
    constexpr std::size_t rivalCursorIndex() const { return _rivalIndex; }
    constexpr bool rivalCursorIsDone() const { return _rivalIndex == kRivalDoneIndex; }
    constexpr CarId rivalCursorCar() const
    {
        return static_cast<CarId>(_rivalIndex < kCarCount ? _rivalIndex : 0u);
    }

private:
    std::size_t _playerIndex = 0;
    std::size_t _rivalIndex = 0;
};

}  // namespace lets_and_go
