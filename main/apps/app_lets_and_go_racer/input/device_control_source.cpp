#include "device_control_source.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/hal.h>
#include <mooncake_log.h>

namespace lets_and_go {
namespace { uint32_t timeMs() { return uint32_t(esp_timer_get_time() / 1000); } }
void DeviceControlSource::open() {
    if (_running.exchange(true)) return;
    _logic.reset();
    _buttonTime = _touchTime = 0;
    _buttonsExited.store(false);
    _touchExited.store(false);
    if (xTaskCreatePinnedToCore([](void* p) { static_cast<DeviceControlSource*>(p)->buttonsTask(); },
        "racer_keys", 3072, this, 2, nullptr, 1) != pdPASS) {
        _buttonsExited.store(true);
        mclog::tagError("RacerInput", "device button task unavailable");
    }
    if (xTaskCreatePinnedToCore([](void* p) { static_cast<DeviceControlSource*>(p)->touchTask(); },
        "racer_touch", 4096, this, 2, nullptr, 1) != pdPASS) {
        _touchExited.store(true);
        mclog::tagError("RacerInput", "device touch task unavailable");
    }
}
void DeviceControlSource::buttonsTask() {
    while (_running.load()) {
        const uint32_t now = timeMs();
        const bool a = gpio_get_level(GPIO_NUM_2) == 0;
        const bool b = gpio_get_level(GPIO_NUM_1) == 0;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _logic.buttons(a, b, now);
            _buttonTime = now;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    _buttonsExited.store(true);
    vTaskDelete(nullptr);
}
void DeviceControlSource::touchTask() {
    while (_running.load()) {
        // Internal touch I²C may wait; never hold the GPIO/snapshot mutex here.
        const auto point = GetHAL().getTouchPoint();
        const uint32_t now = timeMs();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(point.valid) {
                _logic.touch(point.num > 0 && point.x >= 0 && point.y >= 0, point.x, point.y);
                _touchTime = now;
            } else _logic.invalidateTouch();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    _touchExited.store(true);
    vTaskDelete(nullptr);
}
DeviceControlFrame DeviceControlSource::sample(uint32_t) {
    DeviceControlFrame result;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const uint32_t now = timeMs();
        result=_logic.consume(_running.load() && _buttonTime != 0 && _touchTime != 0 &&
                             now - _buttonTime <= 100 && now - _touchTime <= 150);
    }
    if(result.touchTrace.ready) {
        const auto& t=result.touchTrace;
        mclog::tagInfo("RacerTouch","screen={} down={},{} up={},{} target={} accepted={}",
            gameScreenLabel(t.screen),t.startX,t.startY,t.endX,t.endY,touchActionLabel(t.target),t.accepted);
    }
    return result;
}
void DeviceControlSource::setScreen(GameScreen screen) {
    std::lock_guard<std::mutex> lock(_mutex);
    _logic.setScreen(screen);
}
void DeviceControlSource::presentScreen(GameScreen screen) {
    std::lock_guard<std::mutex> lock(_mutex);
    _logic.presentScreen(screen);
}
void DeviceControlSource::close() {
    _running.store(false);
    while (!_buttonsExited.load() || !_touchExited.load()) vTaskDelay(1);
    std::lock_guard<std::mutex> lock(_mutex);
    _logic.reset();
}
} // namespace lets_and_go
