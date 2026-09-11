#pragma once
#include <hal/hal.h>

namespace lets_and_go {
// Explicit RGB565 source type makes this independent of the canvas swap flag.
// Submit a whole visible span instead of one draw call per pigment change.
inline void drawColorSpan(lgfx::LGFXBase& canvas,int x,int y,int count,const uint16_t* colors) {
    if(count<=0)return;
#ifdef ESP_PLATFORM
    canvas.pushImage(x,y,count,1,reinterpret_cast<const lgfx::rgb565_t*>(colors));
#else
    canvas.pushImage(x,y,count,1,colors);
#endif
}
} // namespace lets_and_go
