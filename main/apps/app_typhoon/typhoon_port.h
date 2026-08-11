#pragma once

#include <cstdint>
#include <string>

namespace typhoon {

bool tcImuGetAccel(float* ax, float* ay, float* az);
bool tcImuGetGyro(float* gx, float* gy, float* gz);

bool tcWifiConnected();
int tcWifiStatus();
const char* tcWifiSsid();
const char* tcWifiIpStr();
int tcWifiRssi();

int tcWifiScan();
const char* tcWifiScanSsid(int i);
int tcWifiScanRssi(int i);
bool tcWifiScanOpen(int i);
void tcWifiScanDelete();

void tcWifiDisconnect(bool erase);
void tcWifiModeSta();
void tcWifiSetAutoReconnect(bool on);
bool tcWifiBegin(const char* ssid, const char* pass);

void tcWifiPrefsOpen(bool ro);
void tcWifiPrefsPutSsid(const char* ssid);
void tcWifiPrefsPutPass(const char* pass);
std::string tcWifiPrefsGetSsid();
std::string tcWifiPrefsGetPass();
void tcWifiPrefsClear();
void tcWifiPrefsClose();

// IP geolocation via public HTTP API (requires system WifiManager STA).
// Fills lat/lon and optional city name; returns false if offline/parse fail.
bool tcIpGeolocate(float* lat, float* lon, char* city, size_t cityLen);

}  // namespace typhoon
