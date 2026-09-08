#pragma once

#include "player_progress.h"

namespace lets_and_go {

class RaceProgressStore {
public:
    static PlayerProgress load();
    static bool save(const PlayerProgress& progress);
};

}  // namespace lets_and_go
