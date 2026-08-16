/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "csi_sentinel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

namespace ruview {

namespace {
constexpr uint32_t kCalibrationDurationMs = 30000;
constexpr uint32_t kSignalTimeoutMs = 3000;
constexpr uint32_t kEnableRetryMs = 1000;
constexpr float kStationaryMotionLimit = 0.18f;
constexpr float kMinimumNoiseFloor = 0.035f;
}

bool CsiSentinel::start()
{
    if (_running) {
        return true;
    }
    _running = true;
    _error = nullptr;
    resetCalibration();
    return enableCsi();
}

void CsiSentinel::stop()
{
    disableCsi();
    _running = false;
}

void CsiSentinel::resetCalibration()
{
    _calibrated = false;
    _calibration_stationary_ms = 0;
    _calibration_samples = 0;
    _baseline_mean = 0.0f;
    _baseline_m2 = 0.0f;
    _filtered_signal = 0.0f;
    _previous_signal = 0.0f;
    _activity_votes = 0;
    _clear_votes = 0;
    _activity_latched = false;
}

bool CsiSentinel::enableCsi()
{
    if (!_running || _csi_enabled) {
        return _csi_enabled;
    }

    wifi_ap_record_t ap = {};
    _has_ap_filter = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
    if (!_has_ap_filter) {
        return false;
    }
    std::memcpy(_ap_bssid, ap.bssid, sizeof(_ap_bssid));

    bool promiscuous = false;
    if (esp_wifi_get_promiscuous(&promiscuous) != ESP_OK) {
        _error = "Wi-Fi monitor state unavailable";
        return false;
    }
    _was_promiscuous = promiscuous;

    wifi_csi_config_t config = {};
    config.lltf_en = true;
    config.htltf_en = true;
    config.stbc_htltf2_en = true;
    config.ltf_merge_en = true;
    config.channel_filter_en = true;
    config.manu_scale = false;
    config.shift = 0;
    config.dump_ack_en = false;

    esp_err_t result = esp_wifi_set_promiscuous(true);
    if (result == ESP_OK) result = esp_wifi_set_csi_config(&config);
    if (result == ESP_OK) result = esp_wifi_set_csi_rx_cb(&CsiSentinel::csiCallback, this);
    if (result == ESP_OK) result = esp_wifi_set_csi(true);
    if (result != ESP_OK) {
        esp_wifi_set_csi_rx_cb(nullptr, nullptr);
        if (!_was_promiscuous) esp_wifi_set_promiscuous(false);
        _error = esp_err_to_name(result);
        return false;
    }

    _csi_enabled = true;
    _error = nullptr;
    return true;
}

void CsiSentinel::disableCsi()
{
    if (!_csi_enabled) {
        return;
    }
    esp_wifi_set_csi(false);
    esp_wifi_set_csi_rx_cb(nullptr, nullptr);
    if (!_was_promiscuous) {
        esp_wifi_set_promiscuous(false);
    }
    _csi_enabled = false;
}

void CsiSentinel::csiCallback(void* context, wifi_csi_info_t* info)
{
    if (context && info) {
        static_cast<CsiSentinel*>(context)->acceptFrame(*info);
    }
}

void CsiSentinel::acceptFrame(const wifi_csi_info_t& info)
{
    if (!info.buf || info.len < 8) {
        return;
    }

    // Keep one stable RF path: frames transmitted by the associated access
    // point. Other nearby clients would otherwise dominate the baseline.
    if (_has_ap_filter && std::memcmp(info.mac, _ap_bssid, sizeof(_ap_bssid)) != 0) {
        return;
    }

    const uint16_t offset = info.first_word_invalid ? 4 : 0;
    double power_sum = 0.0;
    uint16_t pair_count = 0;
    for (uint16_t index = offset; index + 1 < info.len; index += 2) {
        const float imaginary = static_cast<float>(info.buf[index]);
        const float real = static_cast<float>(info.buf[index + 1]);
        power_sum += real * real + imaginary * imaginary;
        ++pair_count;
    }
    if (pair_count == 0) {
        return;
    }

    // Log power keeps AGC jumps bounded while retaining small environmental
    // changes. Only aggregate scalars in the Wi-Fi task callback.
    const float signal = std::log1p(static_cast<float>(power_sum / pair_count));
    portENTER_CRITICAL(&_sample_mux);
    _signal_sum += signal;
    _latest_rssi = info.rx_ctrl.rssi;
    ++_pending_frames;
    ++_total_frames;
    portEXIT_CRITICAL(&_sample_mux);
}

SentinelSnapshot CsiSentinel::update(uint32_t now_ms, float device_motion)
{
    SentinelSnapshot snapshot;
    snapshot.device_motion = device_motion;
    snapshot.device_stationary = device_motion < kStationaryMotionLimit;

    if (!_running) {
        snapshot.state = SentinelState::Error;
        return snapshot;
    }

    if (!_csi_enabled && now_ms - _last_enable_attempt_ms >= kEnableRetryMs) {
        _last_enable_attempt_ms = now_ms;
        enableCsi();
    }
    if (!_csi_enabled) {
        snapshot.state = _error ? SentinelState::Error : SentinelState::WaitingForWifi;
        return snapshot;
    }

    float signal_sum = 0.0f;
    uint32_t frame_count = 0;
    portENTER_CRITICAL(&_sample_mux);
    signal_sum = _signal_sum;
    frame_count = _pending_frames;
    snapshot.rssi_dbm = _latest_rssi;
    snapshot.total_frames = _total_frames;
    _signal_sum = 0.0f;
    _pending_frames = 0;
    portEXIT_CRITICAL(&_sample_mux);

    const uint32_t elapsed_ms = _last_update_ms == 0 ? 0 : now_ms - _last_update_ms;
    _last_update_ms = now_ms;
    if (elapsed_ms > 0) {
        snapshot.frame_rate_hz = frame_count * 1000.0f / elapsed_ms;
    }

    if (frame_count == 0) {
        snapshot.calibrated = _calibrated;
        snapshot.calibration_progress = std::min(1.0f, static_cast<float>(_calibration_stationary_ms) /
                                                          static_cast<float>(kCalibrationDurationMs));
        snapshot.state = (_last_frame_ms == 0 || now_ms - _last_frame_ms > kSignalTimeoutMs)
                             ? SentinelState::WaitingForSignal
                             : (_calibrated ? SentinelState::Still : SentinelState::Calibrating);
        return snapshot;
    }
    _last_frame_ms = now_ms;

    const float raw_signal = signal_sum / static_cast<float>(frame_count);
    if (_filtered_signal == 0.0f) {
        _filtered_signal = raw_signal;
        _previous_signal = raw_signal;
    } else {
        _filtered_signal += 0.22f * (raw_signal - _filtered_signal);
    }
    snapshot.signal_level = _filtered_signal;

    if (!snapshot.device_stationary) {
        _activity_votes = 0;
        _clear_votes = 0;
        snapshot.state = SentinelState::DeviceMoving;
    } else if (!_calibrated) {
        const uint32_t accepted_ms = std::min<uint32_t>(elapsed_ms, 500);
        _calibration_stationary_ms += accepted_ms;
        ++_calibration_samples;
        const float delta = _filtered_signal - _baseline_mean;
        _baseline_mean += delta / static_cast<float>(_calibration_samples);
        _baseline_m2 += delta * (_filtered_signal - _baseline_mean);
        if (_calibration_stationary_ms >= kCalibrationDurationMs && _calibration_samples >= 30) {
            _calibrated = true;
        }
        snapshot.state = SentinelState::Calibrating;
    } else {
        const float variance = _calibration_samples > 1
                                   ? _baseline_m2 / static_cast<float>(_calibration_samples - 1)
                                   : 0.0f;
        const float sigma = std::max(kMinimumNoiseFloor, std::sqrt(std::max(0.0f, variance)));
        const float baseline_z = std::fabs(_filtered_signal - _baseline_mean) / sigma;
        const float transient_z = std::fabs(_filtered_signal - _previous_signal) / sigma;
        const float combined = baseline_z * 0.82f + transient_z * 0.18f;
        snapshot.activity_score = std::min(100.0f, combined * 18.0f);

        if (snapshot.activity_score >= 55.0f) {
            _activity_votes = std::min<uint8_t>(10, _activity_votes + 1);
            _clear_votes = 0;
        } else if (snapshot.activity_score < 30.0f) {
            _clear_votes = std::min<uint8_t>(20, _clear_votes + 1);
            if (_activity_votes > 0) --_activity_votes;
        }
        if (_activity_votes >= 3) _activity_latched = true;
        if (_clear_votes >= 8) _activity_latched = false;

        // Slowly follow long-term room drift only while the scene is quiet.
        if (!_activity_latched && snapshot.activity_score < 20.0f) {
            _baseline_mean += 0.002f * (_filtered_signal - _baseline_mean);
        }
        snapshot.state = _activity_latched ? SentinelState::Activity : SentinelState::Still;
    }

    _previous_signal = _filtered_signal;
    snapshot.calibrated = _calibrated;
    snapshot.calibration_progress = std::min(1.0f, static_cast<float>(_calibration_stationary_ms) /
                                                      static_cast<float>(kCalibrationDurationMs));
    return snapshot;
}

const char* CsiSentinel::errorMessage() const
{
    return _error ? _error : "CSI unavailable";
}

const char* sentinelStateTitle(SentinelState state)
{
    switch (state) {
        case SentinelState::WaitingForWifi: return "CONNECT WI-FI";
        case SentinelState::WaitingForSignal: return "WAITING FOR SIGNAL";
        case SentinelState::Calibrating: return "CALIBRATING";
        case SentinelState::Still: return "ROOM STILL";
        case SentinelState::Activity: return "ACTIVITY";
        case SentinelState::DeviceMoving: return "WATCH MOVING";
        case SentinelState::Error: return "CSI ERROR";
    }
    return "RUVIEW";
}

const char* sentinelStateDetail(SentinelState state)
{
    switch (state) {
        case SentinelState::WaitingForWifi: return "Connect the watch to a 2.4 GHz router";
        case SentinelState::WaitingForSignal: return "No CSI frames yet - keep router traffic active";
        case SentinelState::Calibrating: return "Leave the watch untouched";
        case SentinelState::Still: return "No significant RF disturbance";
        case SentinelState::Activity: return "RF environment changed";
        case SentinelState::DeviceMoving: return "Place the watch on a stable surface";
        case SentinelState::Error: return "Restart Wi-Fi or reopen RuView";
    }
    return "";
}

}  // namespace ruview
