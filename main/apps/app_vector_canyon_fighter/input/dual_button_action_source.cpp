#include "dual_button_action_source.h"

#include <driver/gpio.h>

namespace vector_canyon_fighter {
namespace {

constexpr gpio_num_t kRedButtonPin = GPIO_NUM_3;
constexpr gpio_num_t kBlueButtonPin = GPIO_NUM_4;

}  // namespace

void DualButtonActionSource::open()
{
    if (_opened) return;
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << kRedButtonPin) | (1ULL << kBlueButtonPin);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    _opened = gpio_config(&config) == ESP_OK;
    _redButton.reset();
    _blueButton.reset();
    _buttonActions.reset();
    _lastSampleMs = 0;
}

FlightActionSample DualButtonActionSource::sampleActions(uint32_t nowMs)
{
    FlightActionSample result;
    if (!_opened) return result;

    const ButtonTransition red = _redButton.update(
        gpio_get_level(kRedButtonPin) == 0, nowMs);
    const ButtonTransition blue = _blueButton.update(
        gpio_get_level(kBlueButtonPin) == 0, nowMs);
    result.actions = _buttonActions.update(
        red.pressed, blue.pressed, red.clicked, blue.clicked,
        red.holdStarted, blue.holding);
    result.valid = true;
    _lastSampleMs = nowMs;
    return result;
}

FlightActionStatus DualButtonActionSource::actionStatus(uint32_t) const
{
    FlightActionStatus result;
    result.source = FlightActionSource::DualButton;
    // A passive switch has no identity/heartbeat. "connected" therefore
    // means the GPIO path is configured, not that a cable is physically seen.
    result.connected = _opened;
    result.readiness = _opened ? InputReadiness::Ready
                               : InputReadiness::Disconnected;
    result.lastValidSampleMs = _lastSampleMs;
    return result;
}

void DualButtonActionSource::close()
{
    if (!_opened) return;
    gpio_reset_pin(kRedButtonPin);
    gpio_reset_pin(kBlueButtonPin);
    _opened = false;
}

}  // namespace vector_canyon_fighter
