#pragma once

#include "vector_canyon_renderer.h"

#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppVectorCanyonFighter : public mooncake::AppAbility {
public:
    AppVectorCanyonFighter();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _keys;
    vector_canyon_fighter::Renderer _renderer;
    uint32_t _lastFrameMs = 0;
};
