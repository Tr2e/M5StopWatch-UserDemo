#pragma once
#if defined(ESP_PLATFORM)
#include <mooncake_log.h>
#define arenaKickLog(...) mclog::tagInfo("ArenaKick", __VA_ARGS__)
#else
template<class... Ts>
inline void arenaKickLog(Ts const&...) {}
#endif
