#include "../main/apps/common/performance/display_frame_scope.h"

#include <cstdlib>
#include <iostream>

int main()
{
    auto& display = GetHAL().getDisplay();
    display.createSprite(468, 466);
    display.fillScreen(0);
    display.resetDisplayCount();

    {
        app_performance::DisplayFrameScope frame(
            display, {40, 70, 388, 322});
        display.fillRect(0, 0, 468, 466, 0x1234);
        if (display.physicalDisplays() != 0) {
            std::cerr << "frame committed before the outer transaction closed\n";
            return EXIT_FAILURE;
        }
    }

    if (display.physicalDisplays() != 1) {
        std::cerr << "frame did not commit exactly once\n";
        return EXIT_FAILURE;
    }
    const auto& pixels = display.frame();
    if (pixels[69u * 468u + 40u] != 0 ||
        pixels[70u * 468u + 40u] != 0x1234 ||
        pixels[391u * 468u + 427u] != 0x1234 ||
        pixels[392u * 468u + 427u] != 0) {
        std::cerr << "scoped clip did not bound framebuffer writes\n";
        return EXIT_FAILURE;
    }

    display.fillRect(0, 0, 1, 1, 0xabcd);
    if (display.frame()[0] != 0xabcd) {
        std::cerr << "frame scope did not restore the previous clip\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
