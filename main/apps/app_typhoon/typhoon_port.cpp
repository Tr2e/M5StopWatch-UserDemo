#include "typhoon_port.h"

#include <ArduinoJson.h>
#include <hal/hal.h>
#include <wifi_manager.h>
#include <esp_http_client.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <lwip/ip4_addr.h>
#include <nvs.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace typhoon {
namespace {

struct ScanCache {
    std::vector<wifi_ap_record_t> aps;
};

ScanCache g_scan;
nvs_handle_t g_nvs = 0;

}  // namespace

bool tcImuGetAccel(float* ax, float* ay, float* az)
{
    const auto& imu = GetHAL().getImuData();
    *ax = imu.accelX;
    *ay = imu.accelY;
    *az = imu.accelZ;
    return true;
}

bool tcImuGetGyro(float* gx, float* gy, float* gz)
{
    const auto& imu = GetHAL().getImuData();
    *gx = imu.gyroX;
    *gy = imu.gyroY;
    *gz = imu.gyroZ;
    return true;
}

bool tcWifiConnected()
{
    return WifiManager::GetInstance().IsConnected();
}

int tcWifiStatus()
{
    return tcWifiConnected() ? 3 : 6;  // WL_CONNECTED / WL_DISCONNECTED
}

const char* tcWifiSsid()
{
    static char ssid[33] = {};
    wifi_ap_record_t info = {};
    if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) {
        std::memcpy(ssid, info.ssid, sizeof(ssid) - 1);
        return ssid;
    }
    ssid[0] = 0;
    return ssid;
}

const char* tcWifiIpStr()
{
    static char ip[16] = "0.0.0.0";
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return ip;
    esp_netif_ip_info_t info = {};
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
        std::snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
    }
    return ip;
}

int tcWifiRssi()
{
    wifi_ap_record_t info = {};
    if (esp_wifi_sta_get_ap_info(&info) == ESP_OK) return info.rssi;
    return -127;
}

int tcWifiScan()
{
    g_scan.aps.clear();
    wifi_scan_config_t cfg = {};
    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) return 0;
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count == 0) return 0;
    g_scan.aps.resize(count);
    esp_wifi_scan_get_ap_records(&count, g_scan.aps.data());
    return static_cast<int>(count);
}

const char* tcWifiScanSsid(int i)
{
    static char buf[33];
    if (i < 0 || i >= static_cast<int>(g_scan.aps.size())) {
        buf[0] = 0;
        return buf;
    }
    std::memcpy(buf, g_scan.aps[i].ssid, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

int tcWifiScanRssi(int i)
{
    if (i < 0 || i >= static_cast<int>(g_scan.aps.size())) return -127;
    return g_scan.aps[i].rssi;
}

bool tcWifiScanOpen(int i)
{
    if (i < 0 || i >= static_cast<int>(g_scan.aps.size())) return false;
    return g_scan.aps[i].authmode == WIFI_AUTH_OPEN;
}

void tcWifiScanDelete()
{
    g_scan.aps.clear();
}

void tcWifiDisconnect(bool erase)
{
    (void)erase;
    esp_wifi_disconnect();
}

void tcWifiModeSta()
{
    esp_wifi_set_mode(WIFI_MODE_STA);
}

void tcWifiSetAutoReconnect(bool on)
{
    (void)on;
}

bool tcWifiBegin(const char* ssid, const char* pass)
{
    (void)ssid;
    (void)pass;
    return tcWifiConnected();
}

void tcWifiPrefsOpen(bool ro)
{
    if (g_nvs) nvs_close(g_nvs);
    nvs_open("tcwifi", ro ? NVS_READONLY : NVS_READWRITE, &g_nvs);
}

void tcWifiPrefsPutSsid(const char* ssid)
{
    if (!g_nvs) return;
    nvs_set_str(g_nvs, "ssid", ssid);
    nvs_commit(g_nvs);
}

void tcWifiPrefsPutPass(const char* pass)
{
    if (!g_nvs) return;
    nvs_set_str(g_nvs, "pass", pass);
    nvs_commit(g_nvs);
}

std::string tcWifiPrefsGetSsid()
{
    if (!g_nvs) return {};
    char buf[64] = {};
    size_t len = sizeof(buf);
    if (nvs_get_str(g_nvs, "ssid", buf, &len) == ESP_OK) return buf;
    return {};
}

std::string tcWifiPrefsGetPass()
{
    if (!g_nvs) return {};
    char buf[64] = {};
    size_t len = sizeof(buf);
    if (nvs_get_str(g_nvs, "pass", buf, &len) == ESP_OK) return buf;
    return {};
}

void tcWifiPrefsClear()
{
    if (!g_nvs) return;
    nvs_erase_all(g_nvs);
    nvs_commit(g_nvs);
}

void tcWifiPrefsClose()
{
    if (g_nvs) {
        nvs_close(g_nvs);
        g_nvs = 0;
    }
}

namespace {

struct HttpBuf {
    char* data = nullptr;
    size_t size = 0;
    size_t capacity = 0;
};

esp_err_t geoHttpEvent(esp_http_client_event_t* event)
{
    auto* buffer = static_cast<HttpBuf*>(event->user_data);
    if (!buffer || event->event_id != HTTP_EVENT_ON_DATA || !event->data_len) return ESP_OK;
    if (buffer->size + event->data_len + 1 > buffer->capacity) return ESP_ERR_NO_MEM;
    std::memcpy(buffer->data + buffer->size, event->data, event->data_len);
    buffer->size += event->data_len;
    buffer->data[buffer->size] = 0;
    return ESP_OK;
}

}  // namespace

bool tcIpGeolocate(float* lat, float* lon, char* city, size_t cityLen)
{
    if (!lat || !lon) return false;
    if (!tcWifiConnected()) return false;

    HttpBuf buffer;
    buffer.capacity = 2048;
    buffer.data = static_cast<char*>(std::malloc(buffer.capacity));
    if (!buffer.data) return false;

    esp_http_client_config_t config = {};
    // HTTP (not HTTPS) keeps the dependency light; fields= limits payload.
    config.url = "http://ip-api.com/json/?fields=status,message,lat,lon,city";
    config.timeout_ms = 12000;
    config.event_handler = geoHttpEvent;
    config.user_data = &buffer;
    config.buffer_size = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        std::free(buffer.data);
        return false;
    }
    esp_http_client_set_header(client, "User-Agent", "TyphoonCompass/1.1");
    const esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    bool ok = false;
    if (err == ESP_OK && status == 200 && buffer.size > 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, buffer.data)) {
            const char* st = doc["status"] | "";
            if (!std::strcmp(st, "success")) {
                *lat = doc["lat"] | 0.0f;
                *lon = doc["lon"] | 0.0f;
                if (city && cityLen) {
                    const char* name = doc["city"] | "IP LOC";
                    std::snprintf(city, cityLen, "%.12s", name && name[0] ? name : "IP LOC");
                }
                ok = (*lat != 0.0f || *lon != 0.0f);
            }
        }
    }
    std::free(buffer.data);
    return ok;
}

}  // namespace typhoon
