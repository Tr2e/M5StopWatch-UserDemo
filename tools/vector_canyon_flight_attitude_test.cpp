#include "../main/apps/app_vector_canyon_fighter/model/aircraft_geometry.h"
#include "../main/apps/app_vector_canyon_fighter/model/flight_model.h"

#include <cmath>
#include <iostream>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

void run(FlightModel& model, const FlightInput& input, int frames)
{
    for (int frame = 0; frame < frames; ++frame) {
        model.step(input, 1.0f / 60.0f);
    }
}

bool validatePoseDynamics()
{
    FlightModel model;
    model.reset();
    FlightInput input;
    input.valid = true;
    input.steer = 1.0f;
    run(model, input, 60);
    bool valid = check(model.state().roll < -29.0f &&
                           model.state().turnYaw > 7.7f,
                       "P2 right command does not produce a strong right bank and turn yaw");
    valid &= check(std::fabs(model.state().pitch) < 0.01f,
                   "P2 horizontal command leaked into pitch");

    input.steer = 0.0f;
    input.pitch = 1.0f;
    run(model, input, 75);
    valid &= check(std::fabs(model.state().roll) < 0.10f &&
                       std::fabs(model.state().turnYaw) < 0.10f &&
                       model.state().pitch > 17.8f,
                   "P2 neutral bank or climb pose does not settle correctly");

    input.pitch = 0.0f;
    run(model, input, 90);
    valid &= check(std::fabs(model.state().pitch) < 0.10f,
                   "P2 released controls leave a persistent visual attitude");
    return valid;
}

bool validateYawPerspective()
{
    const AircraftScreenOffset neutralNose =
        projectAircraftPose(0.0f, 0.0f, 6.5f, 0.0f, 0.0f, 0.0f);
    const AircraftScreenOffset yawedNose =
        projectAircraftPose(0.0f, 0.0f, 6.5f, 0.0f, 0.0f, 8.0f);
    const AircraftScreenOffset yawedTail =
        projectAircraftPose(0.0f, 0.0f, kAircraftFuselageTailZ,
                            0.0f, 0.0f, 8.0f);
    bool valid = check(yawedNose.x > neutralNose.x + 10.0f,
                       "P2 positive yaw does not point the nose into a right turn");
    valid &= check(yawedNose.x > yawedTail.x + 14.0f,
                   "P2 yaw projection translates the whole fighter instead of rotating it");
    return valid;
}

}  // namespace

int main()
{
    bool valid = validatePoseDynamics();
    valid &= validateYawPerspective();
    return valid ? 0 : 1;
}
