#pragma once

#include "glow_field_audio_reactive.h"
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
    std::unique_ptr<input::KeyManager> _keys;
    std::unique_ptr<glow_field::Engine> _engine;
    std::unique_ptr<glow_field::Renderer> _renderer;
    std::unique_ptr<glow_field::AudioReactiveController> _audio;
    uint32_t _lastHapticMs = 0;
    uint32_t _lastTouchSampleMs = 0;
    uint32_t _modeNoticeUntilMs = 0;
    uint32_t _lastColorUpdateMs = 0;
    uint32_t _lastColorPreviewMs = 0;
    uint32_t _lastColorTouchMs = 0;
    uint32_t _lastColorCruiseMs = 0;
    uint32_t _lastRippleTriggerMs = 0;
    uint32_t _lastAppearanceScaleMs = 0;
    uint32_t _audioStatsStartedMs = 0;
    uint32_t _lastAudioSpectrumSequence = 0;
    uint32_t _audioHopCount = 0;
    uint32_t _engineAudioApplyCount = 0;
    uint32_t _audioHopTimeMaxUs = 0;
    uint32_t _controllerTimeMaxUs = 0;
    uint32_t _engineAudioTimeMaxUs = 0;
    uint32_t _onsetCount = 0;
    uint32_t _sparkCount = 0;
    uint32_t _bloomCount = 0;
    uint64_t _audioHopTimeTotalUs = 0;
    uint64_t _controllerTimeTotalUs = 0;
    uint64_t _engineAudioTimeTotalUs = 0;
    uint32_t _rngState = 1;
    glow_field::InteractionMode _mode = glow_field::InteractionMode::Ripple;
    glow_field::RenderScene _renderScene = glow_field::RenderScene::Interactive;
    glow_field::AudioReactiveFrame _audioFrame = {};
    bool _touching = false;
    bool _colorSelectionMode = false;
    bool _colorTouching = false;
    bool _colorMovedSincePreview = false;
    bool _colorCruising = false;
    bool _appearanceMode = false;
    bool _appearanceScaleRepeating = false;
    glow_field::DotShape _dotShape = glow_field::DotShape::SymbolMix;
    uint8_t _shapeScalePercent = 100;
    uint16_t _selectedHue = 79;
    int16_t _lastRippleX = 0;
    int16_t _lastRippleY = 0;
    float _lastColorAngle = 0.0f;
    float _colorAngularSpeed = 0.0f;
    float _selectedHueFine = 79.0f;

    void toggleColorSelection(uint32_t nowMs);
    void toggleAppearanceMode(uint32_t nowMs);
    void updateAppearanceButtons(uint32_t nowMs, input::KeyEvent keyEvent);
    void cycleDotShape();
    void adjustShapeScale(int delta);
    void updateColorSelectionButtons(uint32_t nowMs);
    void stepColorClockwise(uint32_t nowMs);
    void setSelectedHue(float hue);
    void updateColorSelectionTouch(uint32_t nowMs);
    void spawnColorPreview(uint32_t nowMs, int count);
    void triggerRandomRipple(uint32_t nowMs);
    void triggerRandomPaint(uint32_t nowMs);
    uint32_t nextRandom();
    void updateTouch(uint32_t nowMs);
    void cycleInteractionMode(uint32_t nowMs);
    void syncAudioVisualPause(uint32_t nowMs);
    void updateAudioReactive(uint32_t nowMs);
    void applyAudioVisual(uint32_t nowMs);
    void reportAudioStats(uint32_t nowMs);
};
