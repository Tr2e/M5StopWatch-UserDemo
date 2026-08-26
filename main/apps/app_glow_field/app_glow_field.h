#pragma once

#include "glow_field_engine.h"
#include "glow_field_renderer.h"

#include <apps/common/key_manager/key_manager.h>
#include <memory>
#include <mooncake.h>

class AppGlowField : public mooncake::AppAbility {
public:
    AppGlowField();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class InteractionMode : uint8_t {
        Paint,
        Ripple,
    };

    std::unique_ptr<input::KeyManager> _keys;
    std::unique_ptr<glow_field::Engine> _engine;
    std::unique_ptr<glow_field::Renderer> _renderer;
    uint32_t _lastHapticMs = 0;
    uint32_t _modeNoticeUntilMs = 0;
    InteractionMode _mode = InteractionMode::Ripple;
    bool _touching = false;
};
