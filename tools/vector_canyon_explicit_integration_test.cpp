#include "../main/apps/app_vector_canyon_fighter/explicit_canyon_projection.h"
#include "../main/apps/app_vector_canyon_fighter/input/input_provider.h"
#include "../main/apps/app_vector_canyon_fighter/model/collision_model.h"
#include "../main/apps/app_vector_canyon_fighter/model/flight_model.h"
#include "../main/apps/app_vector_canyon_fighter/vector_canyon_config.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace {

using namespace vector_canyon_fighter;

bool check(bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

class TestInputProvider final : public InputProvider {
public:
    void open() override { _open = true; }
    FlightInput sample(uint32_t) override
    {
        FlightInput input;
        input.valid = _open && _calibrated;
        return input;
    }
    InputStatus status(uint32_t) const override
    {
        InputStatus result;
        result.axisSource = FlightAxisSource::Imu;
        result.actionSource = FlightActionSource::BodyButtons;
        result.axesConnected = _open;
        result.actionsConnected = _open;
        result.calibrationSupported = true;
        result.calibrationProgress = _calibrated ? 1.0f : 0.0f;
        result.readiness = !_open ? InputReadiness::Disconnected
                                  : (_calibrated ? InputReadiness::Ready
                                                 : InputReadiness::Calibrating);
        return result;
    }
    void requestCalibration(uint32_t) override { _calibrated = false; }
    void close() override { _open = false; }

private:
    bool _open = false;
    bool _calibrated = true;
};

bool validateAsymmetricCollision()
{
    bool valid = true;
    ExplicitCanyonStream stream;
    stream.reset(0xC4A71001u);
    FlightState flight{};
    flight.altitude = 0.5f;

    const CanyonShoulderEvent leftEvent = stream.eventAtIndex(0);
    const AircraftCollisionStation wing = kAircraftCollisionStations[kAircraftWingStationIndex];
    stream.update((leftEvent.centerWorldS - wing.forwardLead) /
                  ExplicitCanyonStream::kForwardDistanceScale);
    const CanyonBoundary leftBoundary = stream.boundaryAt(leftEvent.centerWorldS);
    CollisionStatus status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(status.leftClearance < status.rightClearance,
                   "M7 left shoulder event did not reduce only left clearance");
    valid &= check(!status.collided, "M7 centered ship collided inside the left event");

    const float leftWall = leftBoundary.leftWidth +
                           explicitCanyonWallOutsetAtHeight(flight.altitude);
    flight.lateralOffset = -leftWall + wing.halfWidth - 0.01f;
    status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(status.collided && status.leftClearance < 0.0f && status.rightClearance > 0.0f,
                   "M7 left collision used a symmetric or wrong-side boundary");
    valid &= check(status.impactHazard == CollisionHazard::LeftWall,
                   "M8 left collision did not identify the left wall");

    const CanyonShoulderEvent rightEvent = stream.eventAtIndex(1);
    stream.update((rightEvent.centerWorldS - wing.forwardLead) /
                  ExplicitCanyonStream::kForwardDistanceScale);
    const CanyonBoundary rightBoundary = stream.boundaryAt(rightEvent.centerWorldS);
    const float rightWall = rightBoundary.rightWidth +
                            explicitCanyonWallOutsetAtHeight(flight.altitude);
    flight.lateralOffset = rightWall - wing.halfWidth + 0.01f;
    status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(status.collided && status.rightClearance < 0.0f && status.leftClearance > 0.0f,
                   "M7 right collision used a symmetric or wrong-side boundary");
    valid &= check(status.impactHazard == CollisionHazard::RightWall,
                   "M8 right collision did not identify the right wall");

    flight.lateralOffset = 0.0f;
    flight.altitude = kAircraftFloorClearance - 0.01f;
    status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(status.collided && status.floorClearance < 0.0f,
                   "M7 flat floor collision threshold changed");
    valid &= check(status.impactHazard == CollisionHazard::Floor,
                   "M8 floor collision did not identify the floor");
    flight.altitude = 0.5f;
    status = evaluateExplicitCanyonCollision(flight, stream);
    valid &= check(!status.warning,
                   "M7 neutral altitude incorrectly produces a permanent terrain warning");

    std::cout << "left_event_widths=" << leftBoundary.leftWidth << ',' << leftBoundary.rightWidth << '\n';
    std::cout << "right_event_widths=" << rightBoundary.leftWidth << ',' << rightBoundary.rightWidth << '\n';
    return valid;
}

bool validateLookAheadWarning()
{
    ExplicitCanyonStream stream;
    stream.reset(0xC4A71001u);
    const CanyonShoulderEvent event = stream.eventAtIndex(0);
    const float warningWorldS = event.centerWorldS - event.halfLength - 0.10f;
    stream.update(warningWorldS / ExplicitCanyonStream::kForwardDistanceScale);
    FlightState flight{};
    flight.altitude = 0.5f;
    flight.lateralOffset = -1.34f;
    const CollisionStatus status = evaluateExplicitCanyonCollision(flight, stream);
    bool valid = check(!status.collided,
                       "M7 look-ahead warning incorrectly collides before the shoulder arrives");
    valid &= check(status.warning && status.warningClearance < 0.48f,
                   "M7 look-ahead did not warn about the approaching left shoulder");
    std::cout << "look_ahead_current_clearance=" << status.clearance << '\n';
    std::cout << "look_ahead_warning_clearance=" << status.warningClearance << '\n';
    return valid;
}

bool validateFlightAndCameraSeparation()
{
    ExplicitCanyonStream stream;
    stream.reset(0xC4A71001u);
    FlightModel model;
    model.reset();
    FlightInput input{};
    input.valid = true;
    input.steer = 1.0f;
    input.throttle = 0.7f;
    for (int step = 0; step < 120; ++step) model.step(input, 1.0f / 60.0f);
    stream.update(model.state().forwardDistance);

    bool valid = check(model.state().lateralOffset > 1.0f,
                       "M7 generic flight input did not move the ship laterally");
    const CanyonRouteFrame route = stream.routeFrameAt(stream.playerWorldS());
    const CanyonCamera camera = makeExplicitCanyonChaseCamera(
        route, model.state().lateralOffset, model.state().altitude,
        model.state().pitch, 468, 466);
    valid &= check(
        std::abs(camera.position.x -
                 (route.centerX + camera.right.x * model.state().lateralOffset)) < 0.0001f &&
            std::abs(camera.position.z -
                     (route.centerZ + camera.right.z * model.state().lateralOffset)) < 0.0001f,
        "M8 chase camera did not follow player lateral offset exactly once");
    const CanyonWorldPoint before = stream.worldPoint(
        8, static_cast<std::size_t>(CanyonProfilePoint::FloorCenter));
    FlightInput reverse = input;
    reverse.steer = -1.0f;
    for (int step = 0; step < 120; ++step) model.step(reverse, 1.0f / 60.0f);
    const CanyonWorldPoint after = stream.worldPoint(
        8, static_cast<std::size_t>(CanyonProfilePoint::FloorCenter));
    valid &= check(before.x == after.x && before.y == after.y && before.z == after.z,
                   "M7 ship lateral input mutated cached terrain geometry");
    std::cout << "flight_lateral_after_right=" << model.state().lateralOffset << '\n';
    return valid;
}

}  // namespace

int main()
{
    static_assert(VECTOR_CANYON_EXPLICIT_PREVIEW == 0,
                  "M7 must leave isolated preview mode");
    static_assert(VECTOR_CANYON_EXPLICIT_EVENT_STREAM == 0,
                  "M7 event stream must be driven by FlightModel, not preview schedule");
    static_assert(std::is_base_of_v<InputProvider, TestInputProvider>,
                  "M7 input integration must remain provider-polymorphic");

    bool valid = validateAsymmetricCollision();
    valid &= validateLookAheadWarning();
    valid &= validateFlightAndCameraSeparation();
    return valid ? 0 : 1;
}
