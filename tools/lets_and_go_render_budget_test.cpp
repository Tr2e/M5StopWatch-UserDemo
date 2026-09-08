#include "../main/apps/app_lets_and_go_racer/view/render_budget.h"

#include <iostream>

int main()
{
    using namespace lets_and_go;
    FrameBudgetController budget;
    for (int frame = 0; frame < 9; ++frame) budget.observe(45u, false);
    if (budget.detail() != PencilDetail::High) return 1;
    budget.observe(45u, false);
    if (budget.detail() != PencilDetail::Medium) return 2;
    for (int frame = 0; frame < 10; ++frame) budget.observe(60u, false);
    if (budget.detail() != PencilDetail::Low) return 3;
    for (int frame = 0; frame < 120; ++frame) budget.observe(24u, false);
    if (budget.detail() != PencilDetail::Medium) return 4;
    for (int frame = 0; frame < 150; ++frame) budget.observe(24u, false);
    if (budget.detail() != PencilDetail::High) return 5;
    if (budget.stats().frameCount != 290u || budget.stats().peakRenderMs != 60u ||
        budget.stats().lastRenderMs != 24u ||
        budget.stats().detailTransitions != 4u) return 6;
    budget.reset();
    for (int frame = 0; frame < 10; ++frame) budget.observe(20u, true);
    if (budget.detail() != PencilDetail::Medium) return 7;
    if (budget.stats().frameCount != 10u || budget.stats().peakRenderMs != 20u ||
        budget.stats().detailTransitions != 1u) return 8;
    if (std::string(pencilDetailLabel(budget.detail())) != "medium") return 9;
    return 0;
}
