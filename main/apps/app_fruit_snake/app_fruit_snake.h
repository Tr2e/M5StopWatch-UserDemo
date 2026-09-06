#pragma once

#include "fruit_snake_engine.h"
#include "fruit_snake_renderer.h"

#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppFruitSnake : public mooncake::AppAbility {
public:
    AppFruitSnake();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _keys;
    fruit_snake::Engine _engine;
    fruit_snake::Renderer _renderer;
    uint32_t _lastTouchSampleMs = 0;
    bool _touching = false;
};
