/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace network {

enum class WifiState : uint8_t {
    Unconfigured,
    Provisioning,
    Scanning,
    Connecting,
    Online,
    Retrying,
    Failed,
};

struct WifiStatus {
    WifiState state = WifiState::Unconfigured;
    std::string ssid;
    std::string ip_address;
    std::string detail;
    int rssi = 0;
    bool has_saved_network = false;
    bool time_synced = false;
};

class WifiService {
public:
    static WifiService& GetInstance();

    bool initialize();
    void update();
    WifiStatus getStatus() const;

    void beginProvisioning();
    void cancelProvisioning();
    void forgetSavedNetworks();
    void retryConnection();
    void notifyTimeSynced();

private:
    WifiService() = default;
    void onWifiEvent(int event, const std::string& data);
    void setState(WifiState state, const std::string& detail = "");

    mutable std::mutex _mutex;
    WifiStatus _status;
    bool _initialized = false;
    bool _start_station_pending = false;
    uint32_t _start_station_at = 0;
};

const char* wifiStateTitle(WifiState state);
const char* wifiStateDetail(WifiState state);

}  // namespace network
