#pragma once

#include <hal/hal.h>

namespace app_performance {

struct DisplayRegion {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Owns the outermost M5GFX transaction for one direct-rendered frame. The
// AMOLED framebuffer auto-commits when the outer transaction closes. Keeping
// the clip active until then prevents partial frames from expanding their
// dirty range through unchanged UI draws outside the requested region.
class DisplayFrameScope {
public:
    explicit DisplayFrameScope(LGFX_Device& display)
        : _display(display)
    {
        begin(nullptr);
    }

    DisplayFrameScope(LGFX_Device& display, DisplayRegion region)
        : _display(display)
    {
        begin(&region);
    }

    ~DisplayFrameScope()
    {
        finish();
    }

    DisplayFrameScope(const DisplayFrameScope&) = delete;
    DisplayFrameScope& operator=(const DisplayFrameScope&) = delete;

    void finish()
    {
        if (!_active) return;
        _display.endWrite();
        _display.setClipRect(_old_x, _old_y, _old_width, _old_height);
        _active = false;
    }

private:
    void begin(const DisplayRegion* region)
    {
        _display.getClipRect(&_old_x, &_old_y, &_old_width, &_old_height);
        if (region) {
            _display.setClipRect(region->x, region->y,
                                 region->width, region->height);
        }
        // Direct-rendered apps require the framebuffer's outer endWrite to
        // submit. Other direct apps restore this same global default on exit.
        _display.setAutoDisplay(true);
        _display.startWrite();
        _active = true;
    }

    LGFX_Device& _display;
    int32_t _old_x = 0;
    int32_t _old_y = 0;
    int32_t _old_width = 0;
    int32_t _old_height = 0;
    bool _active = false;
};

}  // namespace app_performance
