#pragma once
#include "home_layout.h"

namespace lets_and_go::race_ui_layout {
inline constexpr home_layout::Rect resultRow(int index) {
    return {122,289+50*index,224,44};
}
// Keep all permanent text above the driving area; the minimap remains at right.
inline constexpr home_layout::Rect instruments{116,80,180,35};
} // namespace lets_and_go::race_ui_layout
