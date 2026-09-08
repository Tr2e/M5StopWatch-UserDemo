#include "garage_selection.h"

namespace lets_and_go {
namespace {

static_assert(kCarCount >= 2u, "garage rival navigation needs at least two cars");

std::size_t wrappedStep(std::size_t current, int direction, std::size_t count)
{
    if (count == 0u || direction == 0) return current;
    if (direction > 0) return (current + 1u) % count;
    return (current + count - 1u) % count;
}

}  // namespace

void GarageSelection::reset(CarId player)
{
    _playerIndex = isValidCar(player) ? static_cast<std::size_t>(player) : 0u;
    _rivalIndex = 0u;
    if (_rivalIndex == _playerIndex) moveRival(1, playerCursor());
}

void GarageSelection::syncPlayer(CarId player)
{
    if (!isValidCar(player)) return;
    _playerIndex = static_cast<std::size_t>(player);
    if (_rivalIndex == _playerIndex) moveRival(1, player);
}

void GarageSelection::movePlayer(int direction)
{
    _playerIndex = wrappedStep(_playerIndex, direction, kCarCount);
}

void GarageSelection::moveRival(int direction, CarId player)
{
    if (direction == 0) return;
    constexpr std::size_t kItemCount = kCarCount + 1u;
    do {
        _rivalIndex = wrappedStep(_rivalIndex, direction, kItemCount);
    } while (_rivalIndex < kCarCount &&
             _rivalIndex == static_cast<std::size_t>(player));
}

bool GarageSelection::activatePlayer(GameFlow& flow)
{
    return flow.selectPlayerCar(playerCursor()) && flow.confirmPlayerCar();
}

bool GarageSelection::activateRival(GameFlow& flow)
{
    if (rivalCursorIsDone()) return flow.confirmRivals();
    return flow.toggleRival(rivalCursorCar());
}

}  // namespace lets_and_go
