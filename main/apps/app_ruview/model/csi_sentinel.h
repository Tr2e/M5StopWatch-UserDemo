/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>

struct wifi_csi_info_t;

namespace ruview {

enum class SentinelState : uint8_t {
    WaitingForWifi,
    WaitingForSignal,
    Calibrating,
    Still,
    Activity,
    DeviceMoving,
    Error,
};

enum class Sensitivity : uint8_t {
    Low,
    Medium,
    High,
};

struct SentinelSnapshot {
    SentinelState state = SentinelState::WaitingForWifi;
    float calibration_progress = 0.0f;
    float activity_score = 0.0f;
    float signal_level = 0.0f;
    float frame_rate_hz = 0.0f;
    float device_motion = 0.0f;
    int rssi_dbm = 0;
    uint32_t total_frames = 0;
    bool calibrated = false;
    bool device_stationary = true;
    Sensitivity sensitivity = Sensitivity::Medium;
    float trigger_score = 42.0f;
};

class CsiSentinel {
public:
    bool start();
    void stop();
    void resetCalibration();
    void cycleSensitivity();
    SentinelSnapshot update(uint32_t now_ms, float device_motion);
    const char* errorMessage() const;

private:
    static constexpr uint8_t kFeatureBinCount = 8;

    static void csiCallback(void* context, wifi_csi_info_t* info);
    void acceptFrame(const wifi_csi_info_t& info);
    bool enableCsi();
    void disableCsi();
    void sendTrafficProbe(uint32_t now_ms);
    float calculateActivityScore() const;
    float activeEnterScore() const;
    float activeExitScore() const;

    portMUX_TYPE _sample_mux = portMUX_INITIALIZER_UNLOCKED;
    float _signal_sum = 0.0f;
    float _profile_sum[kFeatureBinCount] = {};
    int _latest_rssi = 0;
    uint32_t _pending_frames = 0;
    uint32_t _total_frames = 0;

    bool _running = false;
    bool _csi_enabled = false;
    bool _was_promiscuous = false;
    bool _has_ap_filter = false;
    uint8_t _ap_bssid[6] = {};
    const char* _error = nullptr;
    int _probe_socket = -1;
    uint32_t _last_probe_ms = 0;
    uint32_t _probe_sequence = 0;
    uint32_t _last_enable_attempt_ms = 0;
    uint32_t _last_update_ms = 0;
    uint32_t _last_frame_ms = 0;

    bool _calibrated = false;
    uint32_t _calibration_stationary_ms = 0;
    uint32_t _calibration_samples = 0;
    float _baseline_mean = 0.0f;
    float _baseline_m2 = 0.0f;
    float _filtered_signal = 0.0f;
    float _previous_signal = 0.0f;
    float _baseline_profile_mean[kFeatureBinCount] = {};
    float _baseline_profile_m2[kFeatureBinCount] = {};
    float _filtered_profile[kFeatureBinCount] = {};
    float _previous_profile[kFeatureBinCount] = {};
    bool _profile_initialized = false;
    float _noise_score_mean = 0.0f;
    float _noise_score_m2 = 0.0f;
    uint32_t _noise_score_samples = 0;
    float _adaptive_enter_score = 42.0f;
    float _adaptive_exit_score = 24.0f;
    Sensitivity _sensitivity = Sensitivity::Medium;
    uint8_t _activity_votes = 0;
    uint8_t _clear_votes = 0;
    bool _activity_latched = false;
};

const char* sentinelStateTitle(SentinelState state);
const char* sentinelStateDetail(SentinelState state);
const char* sensitivityName(Sensitivity sensitivity);

}  // namespace ruview
