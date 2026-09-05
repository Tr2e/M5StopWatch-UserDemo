#include "render_budget_controller.h"

#include <cstdlib>
#include <iostream>

using namespace vector_canyon_fighter;

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

void observeHealthy(RenderBudgetController& controller, int windows)
{
    for (int index = 0; index < windows; ++index) {
        controller.observe(285, 34, 0);
    }
}

}  // namespace

int main()
{
    bool valid = true;
    RenderBudgetController controller;
    valid &= check(controller.detail() == TerrainRenderDetail::High,
                   "controller did not start at full terrain detail");

    // The measured full-detail StopWatch baseline must not self-degrade.
    for (int index = 0; index < 8; ++index) {
        controller.observe(207, 45, 0);
    }
    valid &= check(controller.detail() == TerrainRenderDetail::High,
                   "accepted hardware baseline lost full terrain detail");

    controller.observe(180, 52, 0);
    valid &= check(controller.detail() == TerrainRenderDetail::High,
                   "one slow window changed visible terrain density");
    controller.observe(180, 52, 0);
    valid &= check(controller.detail() == TerrainRenderDetail::Medium,
                   "sustained slow rendering did not remove decoration");

    controller.observe(150, 62, 0);
    valid &= check(controller.detail() == TerrainRenderDetail::Medium,
                   "one severe window removed structural density");
    controller.observe(150, 62, 0);
    valid &= check(controller.detail() == TerrainRenderDetail::Low,
                   "sustained severe rendering did not reach safe detail");

    observeHealthy(controller, 3);
    valid &= check(controller.detail() == TerrainRenderDetail::Low,
                   "safe detail recovered before its hysteresis window");
    observeHealthy(controller, 1);
    valid &= check(controller.detail() == TerrainRenderDetail::Medium,
                   "safe detail did not recover one tier");
    observeHealthy(controller, 4);
    valid &= check(controller.detail() == TerrainRenderDetail::High,
                   "full detail did not recover after sustained headroom");

    controller.observe(285, 34, 2);
    controller.observe(285, 34, 2);
    valid &= check(controller.detail() == TerrainRenderDetail::Medium,
                   "simulation clamp pressure did not trigger protection");

    controller.reset();
    valid &= check(controller.detail() == TerrainRenderDetail::High,
                   "reset did not restore deterministic full detail");
    valid &= check(terrainRenderDetailLabel(TerrainRenderDetail::High)[0] == 'H' &&
                       terrainRenderDetailLabel(TerrainRenderDetail::Medium)[0] == 'M' &&
                       terrainRenderDetailLabel(TerrainRenderDetail::Low)[0] == 'L',
                   "detail diagnostics lost stable labels");

    std::cout << "render_budget_controller_size="
              << sizeof(RenderBudgetController) << '\n';
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
