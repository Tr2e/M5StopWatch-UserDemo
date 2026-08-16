/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "csi_sentinel.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <iterator>
#include <esp_err.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <lwip/sockets.h>
#include <unistd.h>

namespace ruview {

namespace {
constexpr uint32_t kCalibrationDurationMs = 30000;
constexpr uint32_t kRecoveryCalibrationDurationMs = 15000;
constexpr uint32_t kMinimumActivityBeforeRecoveryMs = 12000;
constexpr uint32_t kSettledDriftDurationMs = 6000;
constexpr uint32_t kSignalTimeoutMs = 3000;
constexpr uint32_t kEnableRetryMs = 1000;
constexpr uint32_t kTrafficProbeIntervalMs = 100;
constexpr float kStationaryMotionLimit = 0.18f;
constexpr float kMinimumBinNoiseFloor = 0.025f;
constexpr float kSettledTransientScore = 14.0f;
constexpr float kDefaultActivityEnterScore = 42.0f;
constexpr float kDefaultActivityExitScore = 24.0f;
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
    std::fill(std::begin(_baseline_profile_mean), std::end(_baseline_profile_mean), 0.0f);
    std::fill(std::begin(_baseline_profile_m2), std::end(_baseline_profile_m2), 0.0f);
    std::fill(std::begin(_filtered_profile), std::end(_filtered_profile), 0.0f);
    std::fill(std::begin(_previous_profile), std::end(_previous_profile), 0.0f);
    _profile_initialized = false;
    _recovering_baseline = false;
    _settled_drift_ms = 0;
    _noise_score_mean = 0.0f;
    _noise_score_m2 = 0.0f;
    _noise_score_samples = 0;
    _adaptive_enter_score = kDefaultActivityEnterScore;
    _adaptive_exit_score = kDefaultActivityExitScore;
    _activity_votes = 0;
    _clear_votes = 0;
    _activity_latched = false;
    _activity_started_ms = 0;
    _last_activity_ended_ms = 0;
    _last_activity_duration_ms = 0;
    _activity_event_count = 0;
    _activity_peak_score = 0.0f;
    _last_activity_peak_score = 0.0f;
}

void CsiSentinel::cycleSensitivity()
{
    switch (_sensitivity) {
        case Sensitivity::Low: _sensitivity = Sensitivity::Medium; break;
        case Sensitivity::Medium: _sensitivity = Sensitivity::High; break;
        case Sensitivity::High: _sensitivity = Sensitivity::Low; break;
    }
    _activity_votes = 0;
    _clear_votes = 0;
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
    config.dump_ack_en = true;

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
    if (_probe_socket >= 0) {
        close(_probe_socket);
        _probe_socket = -1;
    }
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

void CsiSentinel::sendTrafficProbe(uint32_t now_ms)
{
    if (now_ms - _last_probe_ms < kTrafficProbeIntervalMs) {
        return;
    }
    _last_probe_ms = now_ms;

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {};
    if (!netif || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.gw.addr == 0) {
        return;
    }
    if (_probe_socket < 0) {
        _probe_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_probe_socket < 0) {
            return;
        }
    }

    sockaddr_in destination = {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(9);
    destination.sin_addr.s_addr = ip_info.gw.addr;
    const uint32_t payload[4] = {0x52555657, ++_probe_sequence, now_ms, 0};
    sendto(_probe_socket, payload, sizeof(payload), MSG_DONTWAIT,
           reinterpret_cast<const sockaddr*>(&destination), sizeof(destination));
}

float CsiSentinel::calculateActivityScore() const
{
    float baseline_z[kFeatureBinCount] = {};
    float transient_z[kFeatureBinCount] = {};
    for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
        const float variance = _calibration_samples > 1
                                   ? _baseline_profile_m2[bin] /
                                         static_cast<float>(_calibration_samples - 1)
                                   : 0.0f;
        const float sigma = std::max(kMinimumBinNoiseFloor,
                                     std::sqrt(std::max(0.0f, variance)));
        baseline_z[bin] = std::min(8.0f,
                                   std::fabs(_filtered_profile[bin] -
                                             _baseline_profile_mean[bin]) /
                                       sigma);
        transient_z[bin] = std::min(8.0f,
                                    std::fabs(_filtered_profile[bin] -
                                              _previous_profile[bin]) /
                                        sigma);
    }
    std::sort(std::begin(baseline_z), std::end(baseline_z), std::greater<float>());
    std::sort(std::begin(transient_z), std::end(transient_z), std::greater<float>());
    const float strongest_baseline = (baseline_z[0] + baseline_z[1] + baseline_z[2]) / 3.0f;
    const float strongest_transient = (transient_z[0] + transient_z[1] + transient_z[2]) / 3.0f;
    return std::min(100.0f, (strongest_baseline * 0.78f + strongest_transient * 0.22f) * 18.0f);
}

float CsiSentinel::calculateTransientScore() const
{
    float transient_z[kFeatureBinCount] = {};
    for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
        const float variance = _calibration_samples > 1
                                   ? _baseline_profile_m2[bin] /
                                         static_cast<float>(_calibration_samples - 1)
                                   : 0.0f;
        const float sigma = std::max(kMinimumBinNoiseFloor,
                                     std::sqrt(std::max(0.0f, variance)));
        transient_z[bin] = std::min(8.0f,
                                    std::fabs(_filtered_profile[bin] -
                                              _previous_profile[bin]) /
                                        sigma);
    }
    std::sort(std::begin(transient_z), std::end(transient_z), std::greater<float>());
    return std::min(100.0f, (transient_z[0] + transient_z[1] + transient_z[2]) * 6.0f);
}

uint32_t CsiSentinel::calibrationTargetMs() const
{
    return _recovering_baseline ? kRecoveryCalibrationDurationMs : kCalibrationDurationMs;
}

void CsiSentinel::beginAutomaticRecovery(uint32_t now_ms)
{
    if (_activity_latched) {
        _last_activity_ended_ms = now_ms;
        _last_activity_duration_ms = now_ms - _activity_started_ms;
        _last_activity_peak_score = _activity_peak_score;
    }
    _activity_latched = false;
    _activity_votes = 0;
    _clear_votes = 0;
    _settled_drift_ms = 0;
    _calibrated = false;
    _recovering_baseline = true;
    _calibration_stationary_ms = 0;
    _calibration_samples = 0;
    _baseline_mean = 0.0f;
    _baseline_m2 = 0.0f;
    std::fill(std::begin(_baseline_profile_mean), std::end(_baseline_profile_mean), 0.0f);
    std::fill(std::begin(_baseline_profile_m2), std::end(_baseline_profile_m2), 0.0f);
    _noise_score_mean = 0.0f;
    _noise_score_m2 = 0.0f;
    _noise_score_samples = 0;
    _adaptive_enter_score = kDefaultActivityEnterScore;
    _adaptive_exit_score = kDefaultActivityExitScore;
}

float CsiSentinel::activeEnterScore() const
{
    const float scale = _sensitivity == Sensitivity::Low ? 1.22f
                        : _sensitivity == Sensitivity::High ? 0.82f
                                                            : 1.0f;
    return std::clamp(_adaptive_enter_score * scale, 26.0f, 85.0f);
}

float CsiSentinel::activeExitScore() const
{
    const float scale = _sensitivity == Sensitivity::Low ? 1.22f
                        : _sensitivity == Sensitivity::High ? 0.82f
                                                            : 1.0f;
    return std::min(activeEnterScore() - 6.0f,
                    std::clamp(_adaptive_exit_score * scale, 14.0f, 55.0f));
}

void CsiSentinel::csiCallback(void* context, wifi_csi_info_t* info)
{
    if (context && info) {
        static_cast<CsiSentinel*>(context)->acceptFrame(*info);
    }
}

void CsiSentinel::acceptFrame(const wifi_csi_info_t& info)
{
    if (!info.buf || info.len < kFeatureBinCount * 2) {
        return;
    }

    // Keep one stable RF path: frames transmitted by the associated access
    // point. Other nearby clients would otherwise dominate the baseline.
    if (_has_ap_filter && std::memcmp(info.mac, _ap_bssid, sizeof(_ap_bssid)) != 0) {
        return;
    }

    const uint16_t offset = info.first_word_invalid ? 4 : 0;
    const uint16_t pair_count = (info.len - offset) / 2;
    float bin_sum[kFeatureBinCount] = {};
    uint16_t bin_count[kFeatureBinCount] = {};
    float signal_sum = 0.0f;
    for (uint16_t pair = 0; pair < pair_count; ++pair) {
        const uint16_t index = offset + pair * 2;
        const float imaginary = static_cast<float>(info.buf[index]);
        const float real = static_cast<float>(info.buf[index + 1]);
        const float feature = std::log1p(real * real + imaginary * imaginary);
        const uint8_t bin = std::min<uint8_t>(kFeatureBinCount - 1,
                                              pair * kFeatureBinCount / pair_count);
        bin_sum[bin] += feature;
        ++bin_count[bin];
        signal_sum += feature;
    }
    if (pair_count == 0) {
        return;
    }

    // Preserve coarse frequency-selective changes instead of collapsing every
    // subcarrier into one power value. Human motion often raises some bands
    // while lowering others, which a single average would cancel out.
    float profile[kFeatureBinCount] = {};
    for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
        if (bin_count[bin] > 0) {
            profile[bin] = bin_sum[bin] / static_cast<float>(bin_count[bin]);
        }
    }
    const float signal = signal_sum / static_cast<float>(pair_count);
    portENTER_CRITICAL(&_sample_mux);
    _signal_sum += signal;
    for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
        _profile_sum[bin] += profile[bin];
    }
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
    sendTrafficProbe(now_ms);
    snapshot.sensitivity = _sensitivity;
    snapshot.trigger_score = activeEnterScore();

    float signal_sum = 0.0f;
    float profile_sum[kFeatureBinCount] = {};
    uint32_t frame_count = 0;
    portENTER_CRITICAL(&_sample_mux);
    signal_sum = _signal_sum;
    std::copy(std::begin(_profile_sum), std::end(_profile_sum), std::begin(profile_sum));
    frame_count = _pending_frames;
    snapshot.rssi_dbm = _latest_rssi;
    snapshot.total_frames = _total_frames;
    _signal_sum = 0.0f;
    std::fill(std::begin(_profile_sum), std::end(_profile_sum), 0.0f);
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
                                                          static_cast<float>(calibrationTargetMs()));
        snapshot.state = (_last_frame_ms == 0 || now_ms - _last_frame_ms > kSignalTimeoutMs)
                             ? SentinelState::WaitingForSignal
                             : (_calibrated ? SentinelState::Still
                                            : (_recovering_baseline ? SentinelState::AdaptingBaseline
                                                                    : SentinelState::Calibrating));
        return snapshot;
    }
    _last_frame_ms = now_ms;

    const float raw_signal = signal_sum / static_cast<float>(frame_count);
    if (!_profile_initialized) {
        _filtered_signal = raw_signal;
        _previous_signal = raw_signal;
        for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
            _filtered_profile[bin] = profile_sum[bin] / static_cast<float>(frame_count);
            _previous_profile[bin] = _filtered_profile[bin];
        }
        _profile_initialized = true;
    } else {
        _filtered_signal += 0.22f * (raw_signal - _filtered_signal);
        for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
            const float sample = profile_sum[bin] / static_cast<float>(frame_count);
            _filtered_profile[bin] += 0.28f * (sample - _filtered_profile[bin]);
        }
    }
    snapshot.signal_level = _filtered_signal;

    if (!snapshot.device_stationary) {
        _activity_votes = 0;
        _clear_votes = 0;
        if (_activity_latched) {
            _activity_latched = false;
            _last_activity_ended_ms = now_ms;
            _last_activity_duration_ms = now_ms - _activity_started_ms;
            _last_activity_peak_score = _activity_peak_score;
        }
        snapshot.state = SentinelState::DeviceMoving;
    } else if (!_calibrated) {
        const uint32_t accepted_ms = std::min<uint32_t>(elapsed_ms, 500);
        _calibration_stationary_ms += accepted_ms;
        if (_calibration_samples >= 30 && _calibration_stationary_ms >= 10000) {
            const float noise_score = calculateActivityScore();
            ++_noise_score_samples;
            const float noise_delta = noise_score - _noise_score_mean;
            _noise_score_mean += noise_delta / static_cast<float>(_noise_score_samples);
            _noise_score_m2 += noise_delta * (noise_score - _noise_score_mean);
        }
        ++_calibration_samples;
        const float delta = _filtered_signal - _baseline_mean;
        _baseline_mean += delta / static_cast<float>(_calibration_samples);
        _baseline_m2 += delta * (_filtered_signal - _baseline_mean);
        for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
            const float bin_delta = _filtered_profile[bin] - _baseline_profile_mean[bin];
            _baseline_profile_mean[bin] += bin_delta / static_cast<float>(_calibration_samples);
            _baseline_profile_m2[bin] +=
                bin_delta * (_filtered_profile[bin] - _baseline_profile_mean[bin]);
        }
        if (_calibration_stationary_ms >= calibrationTargetMs() && _calibration_samples >= 30) {
            _calibrated = true;
            const float noise_variance = _noise_score_samples > 1
                                             ? _noise_score_m2 /
                                                   static_cast<float>(_noise_score_samples - 1)
                                             : 0.0f;
            const float noise_sigma = std::sqrt(std::max(0.0f, noise_variance));
            _adaptive_enter_score = std::clamp(_noise_score_mean + 3.5f * noise_sigma + 6.0f,
                                                32.0f, 70.0f);
            _adaptive_exit_score = std::clamp(_adaptive_enter_score * 0.55f, 17.0f, 38.0f);
            snapshot.trigger_score = activeEnterScore();
        }
        snapshot.state = _recovering_baseline ? SentinelState::AdaptingBaseline
                                              : SentinelState::Calibrating;
    } else {
        _recovering_baseline = false;
        snapshot.activity_score = calculateActivityScore();

        const bool was_activity = _activity_latched;
        if (snapshot.activity_score >= activeEnterScore()) {
            _activity_votes = std::min<uint8_t>(10, _activity_votes + 1);
            _clear_votes = 0;
        } else if (snapshot.activity_score < activeExitScore()) {
            _clear_votes = std::min<uint8_t>(20, _clear_votes + 1);
            if (_activity_votes > 0) --_activity_votes;
        }
        if (_activity_votes >= 2) _activity_latched = true;
        if (_clear_votes >= 6) _activity_latched = false;

        if (_activity_latched && !was_activity) {
            _activity_started_ms = now_ms;
            _activity_peak_score = snapshot.activity_score;
            ++_activity_event_count;
        } else if (_activity_latched) {
            _activity_peak_score = std::max(_activity_peak_score, snapshot.activity_score);
        } else if (was_activity) {
            _last_activity_ended_ms = now_ms;
            _last_activity_duration_ms = now_ms - _activity_started_ms;
            _last_activity_peak_score = _activity_peak_score;
        }

        const uint32_t activity_duration_ms = _activity_latched
                                                  ? now_ms - _activity_started_ms
                                                  : 0;
        if (_activity_latched &&
            activity_duration_ms >= kMinimumActivityBeforeRecoveryMs &&
            calculateTransientScore() < kSettledTransientScore) {
            _settled_drift_ms += std::min<uint32_t>(elapsed_ms, 500);
        } else {
            _settled_drift_ms = 0;
        }
        if (_settled_drift_ms >= kSettledDriftDurationMs) {
            beginAutomaticRecovery(now_ms);
            snapshot.state = SentinelState::AdaptingBaseline;
        }

        // Slowly follow long-term room drift only while the scene is quiet.
        if (!_activity_latched && snapshot.activity_score < 16.0f) {
            _baseline_mean += 0.002f * (_filtered_signal - _baseline_mean);
            for (uint8_t bin = 0; bin < kFeatureBinCount; ++bin) {
                _baseline_profile_mean[bin] +=
                    0.002f * (_filtered_profile[bin] - _baseline_profile_mean[bin]);
            }
        }
        if (snapshot.state != SentinelState::AdaptingBaseline) {
            snapshot.state = _activity_latched ? SentinelState::Activity : SentinelState::Still;
        }
    }

    _previous_signal = _filtered_signal;
    std::copy(std::begin(_filtered_profile), std::end(_filtered_profile),
              std::begin(_previous_profile));
    snapshot.calibrated = _calibrated;
    snapshot.calibration_progress = std::min(1.0f, static_cast<float>(_calibration_stationary_ms) /
                                                      static_cast<float>(calibrationTargetMs()));
    snapshot.has_activity_event = _activity_event_count > 0;
    snapshot.activity_event_active = _activity_latched;
    snapshot.activity_event_count = _activity_event_count;
    if (_activity_latched) {
        snapshot.activity_peak_score = _activity_peak_score;
        snapshot.activity_duration_ms = now_ms - _activity_started_ms;
    } else if (_last_activity_ended_ms != 0) {
        snapshot.activity_peak_score = _last_activity_peak_score;
        snapshot.activity_duration_ms = _last_activity_duration_ms;
        snapshot.last_activity_age_ms = now_ms - _last_activity_ended_ms;
    }
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
        case SentinelState::AdaptingBaseline: return "ADAPTING BASELINE";
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
        case SentinelState::AdaptingBaseline: return "Stable room change - learning new baseline";
        case SentinelState::Still: return "No significant RF disturbance";
        case SentinelState::Activity: return "RF environment changed";
        case SentinelState::DeviceMoving: return "Place the watch on a stable surface";
        case SentinelState::Error: return "Restart Wi-Fi or reopen RuView";
    }
    return "";
}

const char* sensitivityName(Sensitivity sensitivity)
{
    switch (sensitivity) {
        case Sensitivity::Low: return "LOW";
        case Sensitivity::Medium: return "MED";
        case Sensitivity::High: return "HIGH";
    }
    return "MED";
}

}  // namespace ruview
