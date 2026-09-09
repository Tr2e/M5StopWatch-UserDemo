#pragma once

#include "game_flow.h"

#include <cstdint>

namespace lets_and_go {

enum class ResultAction : uint8_t {
    Retry,
    Garage,
    Exit,
    Count,
};

class ResultsSelection {
public:
    void reset() { _cursor = ResultAction::Retry; }
    void move(int direction);
    void select(ResultAction action) { if(action<ResultAction::Count)_cursor=action; }
    ResultAction cursor() const { return _cursor; }
    bool activate(GameFlow& flow) const;

private:
    ResultAction _cursor = ResultAction::Retry;
};

const char* resultActionLabel(ResultAction action);

}  // namespace lets_and_go
