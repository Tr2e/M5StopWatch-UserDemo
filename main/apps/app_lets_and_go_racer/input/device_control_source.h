#pragma once
#include "device_control_logic.h"
#include <atomic>
#include <mutex>

namespace lets_and_go {
class DeviceControlSource {
public:
    ~DeviceControlSource() { close(); }
    void open();
    void close();
    DeviceControlFrame sample(uint32_t nowMs);
    void setScreen(GameScreen screen);
private:
    void buttonsTask();
    void touchTask();
    std::mutex _mutex;
    DeviceControlLogic _logic;
    std::atomic<bool> _running{false}, _buttonsExited{true}, _touchExited{true};
    uint32_t _buttonTime = 0, _touchTime = 0;
};
} // namespace lets_and_go
