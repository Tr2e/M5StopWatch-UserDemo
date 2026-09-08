#include "results_selection.h"

namespace lets_and_go {

void ResultsSelection::move(int direction)
{
    if (direction == 0) return;
    constexpr int count = static_cast<int>(ResultAction::Count);
    const int current = static_cast<int>(_cursor);
    _cursor = static_cast<ResultAction>((current + (direction > 0 ? 1 : count - 1)) % count);
}

bool ResultsSelection::activate(GameFlow& flow) const
{
    switch (_cursor) {
        case ResultAction::Retry: return flow.retrySameRace();
        case ResultAction::Garage: return flow.backToGarage();
        case ResultAction::Exit:
            flow.requestExit();
            return true;
        case ResultAction::Count: return false;
    }
    return false;
}

const char* resultActionLabel(ResultAction action)
{
    switch (action) {
        case ResultAction::Retry: return "RETRY";
        case ResultAction::Garage: return "GARAGE";
        case ResultAction::Exit: return "EXIT";
        case ResultAction::Count: return "";
    }
    return "";
}

}  // namespace lets_and_go
