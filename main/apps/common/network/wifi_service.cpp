/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "wifi_service.h"
#include <esp_sntp.h>
#include <esp_wifi_types.h>
#include <freertos/FreeRTOS.h>
#include <hal/hal.h>
#include <hal/utils/settings/settings.h>
#include <ssid_manager.h>
#include <sys/time.h>
#include <wifi_manager.h>

namespace network {

namespace {

void on_time_sync(struct timeval*)
{
    GetHAL().syncSystemTimeToRtc();
    WifiService::GetInstance().notifyTimeSynced();
}

void start_time_sync()
{
    static bool initialized = false;
    if (!initialized) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_set_time_sync_notification_cb(on_time_sync);
        esp_sntp_init();
        initialized = true;
        return;
    }
    esp_sntp_restart();
}

}  // namespace

WifiService& WifiService::GetInstance()
{
    static WifiService instance;
    return instance;
}

bool WifiService::initialize()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_initialized) {
            return true;
        }
    }

    auto& wifi = WifiManager::GetInstance();
    WifiManagerConfig config;
    config.ssid_prefix = "M5StopWatch";
    config.language    = "en";

    if (!wifi.Initialize(config)) {
        setState(WifiState::Failed, "Wi-Fi hardware could not start.");
        return false;
    }

    wifi.SetEventCallback([this](WifiEvent event, const std::string& data) {
        onWifiEvent(static_cast<int>(event), data);
    });

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _initialized                     = true;
        _status.has_saved_network        = !SsidManager::GetInstance().GetSsidList().empty();
        _status.time_synced              = false;
    }

    if (getStatus().has_saved_network) {
        setState(WifiState::Connecting, "Connecting to saved network...");
        wifi.StartStation();
    } else {
        setState(WifiState::Unconfigured);
    }
    return true;
}

void WifiService::update()
{
    bool start_station = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_start_station_pending && static_cast<int32_t>(GetHAL().millis() - _start_station_at) >= 0) {
            _start_station_pending = false;
            start_station          = _status.has_saved_network;
        }
    }
    if (start_station) {
        Settings settings("system", false);
        GetHAL().setTimezone(settings.GetString("tz", "GMT0"));
        setState(WifiState::Connecting, "Connecting to saved network...");
        WifiManager::GetInstance().StartStation();
    }
}

WifiStatus WifiService::getStatus() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _status;
}

void WifiService::beginProvisioning()
{
    if (!_initialized) {
        return;
    }
    setState(WifiState::Provisioning, "Connect a phone to the device hotspot.");
    WifiManager::GetInstance().StartConfigAp();
}

void WifiService::cancelProvisioning()
{
    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsConfigMode()) {
        wifi.StopConfigAp();
    }
    if (!getStatus().has_saved_network) {
        setState(WifiState::Unconfigured);
    }
}

void WifiService::forgetSavedNetworks()
{
    auto& wifi = WifiManager::GetInstance();
    SsidManager::GetInstance().Clear();
    wifi.StopStation();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _status.has_saved_network = false;
        _status.ssid.clear();
        _status.ip_address.clear();
        _status.rssi = 0;
        _status.time_synced = false;
    }
    beginProvisioning();
}

void WifiService::retryConnection()
{
    const auto status = getStatus();
    if (!status.has_saved_network) {
        beginProvisioning();
        return;
    }
    auto& wifi = WifiManager::GetInstance();
    wifi.StopStation();
    setState(WifiState::Connecting, "Retrying saved network...");
    wifi.StartStation();
}

void WifiService::notifyTimeSynced()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _status.time_synced = true;
}

void WifiService::onWifiEvent(int raw_event, const std::string& data)
{
    const auto event = static_cast<WifiEvent>(raw_event);
    switch (event) {
        case WifiEvent::Scanning:
            setState(WifiState::Scanning, "Looking for saved networks...");
            break;
        case WifiEvent::Connecting:
            setState(WifiState::Connecting, "Connecting to " + data + "...");
            break;
        case WifiEvent::Connected: {
            auto& wifi = WifiManager::GetInstance();
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _status.state = WifiState::Online;
                _status.ssid = wifi.GetSsid();
                _status.ip_address = wifi.GetIpAddress();
                _status.rssi = wifi.GetRssi();
                _status.detail = "Connected";
                _status.time_synced = false;
            }
            start_time_sync();
            break;
        }
        case WifiEvent::Disconnected:
            if (WifiManager::GetInstance().IsConfigMode()) {
                break;
            }
            if (data == std::to_string(WIFI_REASON_NO_AP_FOUND)) {
                setState(WifiState::Failed, "Saved network was not found. Check the router, then retry.");
            } else if (data == std::to_string(WIFI_REASON_AUTH_FAIL)) {
                setState(WifiState::Failed, "Wi-Fi password was rejected. Set up Wi-Fi again.");
            } else {
                setState(getStatus().has_saved_network ? WifiState::Retrying : WifiState::Unconfigured,
                         data.empty() ? "Connection lost. Retrying..."
                                      : "Connection lost (reason " + data + "). Retrying...");
            }
            break;
        case WifiEvent::ConfigModeEnter:
            setState(WifiState::Provisioning, "Connect a phone to the device hotspot.");
            break;
        case WifiEvent::ConfigModeExit: {
            std::lock_guard<std::mutex> lock(_mutex);
            _status.has_saved_network = !SsidManager::GetInstance().GetSsidList().empty();
            _start_station_pending = _status.has_saved_network;
            _start_station_at = GetHAL().millis() + 250;
            if (!_status.has_saved_network) {
                _status.state = WifiState::Unconfigured;
                _status.detail = wifiStateDetail(_status.state);
            }
            break;
        }
    }
}

void WifiService::setState(WifiState state, const std::string& detail)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _status.state = state;
    _status.detail = detail.empty() ? wifiStateDetail(state) : detail;
    if (state != WifiState::Online) {
        _status.time_synced = false;
    }
}

const char* wifiStateTitle(WifiState state)
{
    switch (state) {
        case WifiState::Unconfigured: return "Wi-Fi not set up";
        case WifiState::Provisioning: return "Set up Wi-Fi";
        case WifiState::Scanning: return "Looking for Wi-Fi";
        case WifiState::Connecting: return "Connecting";
        case WifiState::Online: return "Wi-Fi connected";
        case WifiState::Retrying: return "Connection lost";
        case WifiState::Failed: return "Wi-Fi unavailable";
    }
    return "Wi-Fi";
}

const char* wifiStateDetail(WifiState state)
{
    switch (state) {
        case WifiState::Unconfigured: return "No saved network. Start setup to connect.";
        case WifiState::Provisioning: return "Connect a phone to the device hotspot.";
        case WifiState::Scanning: return "Looking for saved networks...";
        case WifiState::Connecting: return "Connecting to saved network...";
        case WifiState::Online: return "Connected";
        case WifiState::Retrying: return "Connection lost. Retrying...";
        case WifiState::Failed: return "Wi-Fi hardware could not start.";
    }
    return "";
}

}  // namespace network
