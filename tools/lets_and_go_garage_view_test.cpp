#include "../main/apps/app_lets_and_go_racer/controller/garage_view_controller.h"
#include "../main/apps/app_lets_and_go_racer/input/racer_input_logic.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace lets_and_go;
bool near(float a,float b){return std::abs(a-b)<.0001f;}
int main()
{
    GarageViewController motion;motion.reset(CarId::CycloneMagnum,0);
    assert(near(motion.state(1000).wheelPhase,0));
    const float idleYaw=motion.state(1000).yaw;
    assert(near(idleYaw,motion.state(9000).yaw));
    assert(!motion.animating(1000));
    motion.changeView(1,1000);
    assert(motion.state(1000).preset==GarageView::Side && near(motion.state(1000).yaw,idleYaw));
    assert(near(motion.state(1349).wheelPhase,0));
    assert(near(motion.state(1350).yaw,-1.5707963f));
    assert(!near(motion.state(1500).wheelPhase,motion.state(1600).wheelPhase));
    const auto side=motion.state(1600);
    motion.selectCar(CarId::HurricaneSonic,1600);
    assert(motion.state(1600).preset==GarageView::Side);
    assert(near(motion.state(1600).yaw,side.yaw) && motion.state(1600).carZoom<1);
    assert(near(motion.state(1950).carZoom,1) && near(motion.state(1950).carSlide,0));
    const auto frozen=motion.state(2000).wheelPhase;
    motion.changeView(1,2000);
    assert(near(motion.state(2350).wheelPhase,frozen) && near(motion.state(9000).wheelPhase,frozen));
    motion.changeView(1,2400);motion.changeView(1,2800);
    assert(motion.state(3150).preset==GarageView::Front);
    assert(motion.animating(3149) && !motion.animating(3150));
    assert(near(motion.state(3150).yaw,motion.state(9000).yaw));
    motion.selectCar(CarId::CycloneMagnum,3200);
    assert(motion.animating(3200));
    assert(!motion.animating(3550));
    // Rapid direction changes preserve the actual intermediate pose.
    motion.changeView(-1,3300);
    const auto middle=motion.state(3475);
    motion.changeView(1,3475);
    const auto retarget=motion.state(3475);
    assert(near(std::remainder(middle.yaw-retarget.yaw,6.2831853f),0));
    assert(near(middle.pitch,retarget.pitch) && near(middle.scale,retarget.scale));
    motion.reset(CarId::CycloneMagnum,UINT32_MAX-200);
    motion.changeView(1,UINT32_MAX-200);
    assert(near(motion.state(149).yaw,-1.5707963f));
    assert(near(motion.state(149).wheelPhase,0) && motion.state(249).wheelPhase>0);

    // At 3-30 FPS, including stalls, repeated spokes must never resolve to a
    // backwards step. Repeated reads within one frame must be side-effect free.
    for(uint32_t interval : {33u,100u,167u,200u,333u,400u,2000u}) {
        motion.reset(CarId::CycloneMagnum,0);motion.changeView(1,0);
        uint32_t time=350;
        motion.presented(time);
        for(int frame=0;frame<100;++frame) {
            const float previous=motion.state(time).wheelPhase;
            time+=interval;
            const float next=motion.state(time).wheelPhase;
            assert(near(next,motion.state(time).wheelPhase));
            for(int spokes : {3,5,6}) {
                const float perceived=std::remainder(next-previous,6.283185307f/spokes);
                assert(perceived>0 && perceived<=.25001f);
            }
            motion.presented(time);
            assert(near(next,motion.state(time).wheelPhase));
        }
    }

    GarageMenuNavigation nav;
    auto step=nav.update(.9f,.8f,100);assert(step.car==1 && step.view==0);
    step=nav.update(.7f,1.f,460);assert(step.car==1 && step.view==0);
    step=nav.update(0,1.f,500);assert(step.car==0 && step.view==0);
    nav.update(0,0,510);step=nav.update(.2f,1.f,520);assert(step.car==0 && step.view==1);
    step=nav.update(.95f,.7f,880);assert(step.car==0 && step.view==1);
    nav.update(std::numeric_limits<float>::quiet_NaN(),0,900);
    step=nav.update(-1,0,920);assert(step.car==-1 && step.view==0);
    std::cout << "Garage view: four presets, 350ms easing, retarget continuity, retained view, side-only wheels, axis lock, rollover; state="
              << sizeof(GarageViewController) << " bytes\n";
}
