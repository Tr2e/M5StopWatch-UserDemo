#pragma once

namespace lets_and_go::home_layout {
struct Rect {
    int x,y,width,height;
    constexpr bool contains(int px,int py) const {
        return px>=x && px<x+width && py>=y && py<y+height;
    }
    constexpr bool near(int px,int py,int padding=4) const {
        return px>=x-padding && px<x+width+padding && py>=y-padding && py<y+height+padding;
    }
};
// Shared by the native 466/468-wide home rendering and the touch input worker.
inline constexpr Rect deviceAction{91,218,284,60};
inline constexpr Rect viewAction{138,84,192,24};
inline constexpr Rect carSelect{155,407,156,30};
inline constexpr Rect setupBack{95,54,42,32};
inline constexpr Rect rivalToggle{155,348,156,32};
inline constexpr Rect setupNext{141,394,184,44};
inline constexpr Rect previous(int y) { return {98,y-22,50,44}; }
inline constexpr Rect next(int y,int width=468) { return {width-148,y-22,50,44}; }
} // namespace lets_and_go::home_layout
