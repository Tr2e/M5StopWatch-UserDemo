#include "typhoon_data.h"

#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <wifi_manager.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace typhoon {
namespace {

constexpr uint8_t kMaxStorms = 4;
constexpr size_t kResponseCapacity = 192 * 1024;
constexpr uint32_t kRefreshPeriodMs = 30UL * 60UL * 1000UL;

struct HttpBuffer {
    char* data = nullptr;
    size_t size = 0;
    size_t capacity = 0;
};

esp_err_t httpEvent(esp_http_client_event_t* event)
{
    auto* buffer = static_cast<HttpBuffer*>(event->user_data);
    if (!buffer || event->event_id != HTTP_EVENT_ON_DATA || !event->data_len) return ESP_OK;
    if (buffer->size + event->data_len + 1 > buffer->capacity) return ESP_ERR_NO_MEM;
    std::memcpy(buffer->data + buffer->size, event->data, event->data_len);
    buffer->size += event->data_len;
    buffer->data[buffer->size] = 0;
    return ESP_OK;
}

bool httpGet(const char* url, HttpBuffer& buffer)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 25000;
    config.event_handler = httpEvent;
    config.user_data = &buffer;
    config.buffer_size = 4096;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;

    esp_http_client_set_header(client, "User-Agent", "TyphoonCompass/1.1");
    const esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return err == ESP_OK && status == 200 && buffer.size > 0;
}

const char* jsonpObject(char* body)
{
    char* begin = std::strchr(body, '{');
    char* end = std::strrchr(body, '}');
    if (!begin || !end || end <= begin) return nullptr;
    end[1] = 0;
    return begin;
}

uint8_t grade(const char* value)
{
    if (!value) return 0;
    if (!std::strcmp(value, "SuperTY") || !std::strcmp(value, "Super TY")) return 5;
    if (!std::strcmp(value, "STY")) return 4;
    if (!std::strcmp(value, "TY")) return 3;
    if (!std::strcmp(value, "STS")) return 2;
    if (!std::strcmp(value, "TS")) return 1;
    return 0;
}

uint16_t msToKt(float value)
{
    return static_cast<uint16_t>(std::clamp(value * 1.943844f + 0.5f, 0.0f, 255.0f));
}

float nmcRadiusKm(JsonVariant radii, const char* label)
{
    if (!radii.is<JsonArray>()) return 0.0f;
    for (JsonVariant row : radii.as<JsonArray>()) {
        if (!row.is<JsonArray>() || row.as<JsonArray>().size() < 5) continue;
        const char* tag = row[0] | "";
        if (std::strcmp(tag, label) != 0) continue;
        const float ne = row[1] | 0.0f;
        const float se = row[2] | 0.0f;
        const float sw = row[3] | 0.0f;
        const float nw = row[4] | 0.0f;
        return (ne + se + sw + nw) * 0.25f;
    }
    return 0.0f;
}

float averageRadius(JsonArray value)
{
    if (value.size() < 5) return 0.0f;
    float total = 0.0f;
    int count = 0;
    for (int i = 1; i <= 4; ++i) {
        const float radius = value[i] | 0.0f;
        if (radius > 0.0f) {
            total += radius;
            ++count;
        }
    }
    return count ? total / count : 0.0f;
}

void radii(JsonVariant point, float& r7, float& r10, float& r12)
{
    r7 = r10 = r12 = 0.0f;
    if (!point.is<JsonArray>()) return;
    JsonArray radiusValues = point[10].as<JsonArray>();
    for (JsonVariant value : radiusValues) {
        if (!value.is<JsonArray>() || value.as<JsonArray>().size() < 2) continue;
        const char* label = value[0] | "";
        const float radius = averageRadius(value.as<JsonArray>());
        if (!std::strcmp(label, "30KTS")) r7 = radius;
        else if (!std::strcmp(label, "50KTS")) r10 = radius;
        else if (!std::strcmp(label, "64KTS")) r12 = radius;
    }
    if (r7 < 40.0f) r7 = 120.0f;
    if (r10 < 20.0f) r10 = r7 * 0.55f;
    if (r12 < 10.0f) r12 = r10 * 0.5f;
}

void fillTrackPtFromAnalysis(JsonVariant p, int64_t nowTs, float fallbackR12, NmcTrackPt& out)
{
    const int64_t ts = p[2] | nowTs;
    out.hour = static_cast<int16_t>((ts - nowTs) / 3600000LL);
    out.lon = p[4] | 0.0f;
    out.lat = p[5] | 0.0f;
    out.pressure = static_cast<uint16_t>(p[6] | 1000);
    out.wind_kt = static_cast<uint8_t>(msToKt(p[7] | 0.0f));
    out.cat = grade(p[3] | "TS");
    const float r12p = nmcRadiusKm(p[10], "64KTS");
    out.r12_km = (r12p > 1.0f) ? r12p : fallbackR12;
}

// Build full path: past (analysis) → NOW → BABJ forecast.
// hour is relative to NOW: negative = history, 0 = now, positive = forecast.
bool parseTrack(JsonArray points, JsonVariant last, NmcTrack& track, float fallbackR12)
{
    std::memset(&track, 0, sizeof(track));
    if (points.isNull() || points.size() == 0 || !last.is<JsonArray>()) return false;

    const int64_t nowTs = last[2] | 0LL;
    const int nPts = static_cast<int>(points.size());

    // Reserve room for NOW + forecast; keep denser past for hour-scaled timeline.
    constexpr int kPastWant = 10;
    constexpr int kForecastReserve = 10;
    const int pastBudget = static_cast<int>(kNmcTrackMax) - 1 - kForecastReserve;
    const int pastWant = pastBudget > kPastWant ? kPastWant : (pastBudget > 0 ? pastBudget : 0);

    int step = 1;
    if (nPts > pastWant + 1) step = nPts / (pastWant + 1);
    if (step < 1) step = 1;

    for (int i = nPts - 1 - pastWant * step; i < nPts - 1; i += step) {
        if (i < 0) continue;
        if (track.n >= kNmcTrackMax - kForecastReserve - 1) break;
        JsonVariant p = points[i];
        if (!p.is<JsonArray>() || p.as<JsonArray>().size() < 8) continue;
        fillTrackPtFromAnalysis(p, nowTs, fallbackR12, track.pts[track.n]);
        ++track.n;
    }

    // NOW (latest analysis)
    track.now_idx = track.n;
    if (track.n < kNmcTrackMax) {
        NmcTrackPt& now = track.pts[track.n++];
        now.hour = 0;
        now.lon = last[4] | 0.0f;
        now.lat = last[5] | 0.0f;
        now.pressure = static_cast<uint16_t>(last[6] | 1000);
        now.wind_kt = static_cast<uint8_t>(msToKt(last[7] | 0.0f));
        now.cat = grade(last[3] | "TS");
        now.r12_km = fallbackR12;
    }

    // BABJ forecast attached to the latest analysis point: last[11]["BABJ"]
    JsonArray lastArr = last.as<JsonArray>();
    if (lastArr.size() > 11 && last[11].is<JsonObject>()) {
        JsonArray babj = last[11]["BABJ"];
        if (!babj.isNull()) {
            for (JsonVariant f : babj) {
                if (track.n >= kNmcTrackMax) break;
                if (!f.is<JsonArray>() || f.as<JsonArray>().size() < 8) continue;
                JsonArray fa = f.as<JsonArray>();
                NmcTrackPt& tp = track.pts[track.n++];
                tp.hour = static_cast<int16_t>(fa[0] | 0);
                // Skip non-future / duplicate NOW rows if any
                if (tp.hour <= 0) {
                    --track.n;
                    continue;
                }
                tp.lon = fa[2] | 0.0f;
                tp.lat = fa[3] | 0.0f;
                tp.pressure = static_cast<uint16_t>(fa[4] | 1010);
                tp.wind_kt = static_cast<uint8_t>(msToKt(fa[5] | 0.0f));
                // Index 7 is intensity grade in NMC BABJ rows
                tp.cat = grade(fa[7] | "TS");
                tp.r12_km = fallbackR12 * 0.85f;
            }
        }
    }

    return track.n > 0;
}

bool parseStorm(uint32_t id, const char* fallbackName, StormData& storm, NmcTrack& track)
{
    char url[128];
    std::snprintf(url, sizeof(url),
                  "http://typhoon.nmc.cn/weatherservice/typhoon/jsons/view_%lu",
                  static_cast<unsigned long>(id));
    HttpBuffer buffer;
    buffer.capacity = kResponseCapacity;
    buffer.data = static_cast<char*>(heap_caps_malloc(buffer.capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer.data || !httpGet(url, buffer)) {
        if (buffer.data) heap_caps_free(buffer.data);
        return false;
    }
    const char* json = jsonpObject(buffer.data);
    if (!json) {
        heap_caps_free(buffer.data);
        return false;
    }
    JsonDocument document;
    if (deserializeJson(document, json)) {
        heap_caps_free(buffer.data);
        return false;
    }
    JsonArray typhoon = document["typhoon"];
    JsonArray points = typhoon[8];
    if (typhoon.isNull() || points.isNull() || points.size() == 0) {
        heap_caps_free(buffer.data);
        return false;
    }
    JsonVariant last = points[points.size() - 1];
    std::memset(&storm, 0, sizeof(storm));
    storm.lat = last[5] | 0.0f;
    storm.lon = last[4] | 0.0f;
    storm.pressure = static_cast<uint16_t>(last[6] | 1010);
    storm.wind = msToKt(last[7] | 0.0f);
    storm.category = grade(last[3] | "TD");
    radii(last, storm.r7, storm.r10, storm.r12);
    const char* name = typhoon[1] | fallbackName;
    std::strncpy(storm.name, name && name[0] ? name : "STORM", sizeof(storm.name) - 1);
    parseTrack(points, last, track, storm.r12);
    heap_caps_free(buffer.data);
    return true;
}

bool fetchActive(TyphoonSnapshot& result)
{
    if (!WifiManager::GetInstance().IsConnected()) return false;
    HttpBuffer buffer;
    buffer.capacity = kResponseCapacity;
    buffer.data = static_cast<char*>(heap_caps_malloc(buffer.capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!buffer.data) return false;
    const bool fetched = httpGet("http://typhoon.nmc.cn/weatherservice/typhoon/jsons/list_default", buffer);
    const char* json = fetched ? jsonpObject(buffer.data) : nullptr;
    if (!json) {
        heap_caps_free(buffer.data);
        return false;
    }
    JsonDocument document;
    if (deserializeJson(document, json)) {
        heap_caps_free(buffer.data);
        return false;
    }
    JsonArray list = document["typhoonList"];
    if (list.isNull()) {
        heap_caps_free(buffer.data);
        return false;
    }

    TyphoonSnapshot parsed;
    for (JsonVariant row : list) {
        if (parsed.count >= kMaxStorms || !row.is<JsonArray>() || row.as<JsonArray>().size() < 8) break;
        if (std::strcmp(row[7] | "", "start") != 0) continue;
        const uint32_t id = row[0] | 0UL;
        const char* name = row[1] | "STORM";
        if (parseStorm(id, name, parsed.storms[parsed.count], parsed.tracks[parsed.count])) ++parsed.count;
    }
    heap_caps_free(buffer.data);
    if (!parsed.count) return false;
    parsed.live = true;
    parsed.updated_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    result = parsed;
    return true;
}

}  // namespace

struct DataService::Impl {
    mutable std::mutex mutex;
    TyphoonSnapshot state;
    FetchState fetch_state = FetchState::Idle;
    TaskHandle_t task = nullptr;
    volatile bool stop = false;
};

DataService::DataService() : _impl(std::make_unique<Impl>()) {}
DataService::~DataService() { stop(); }

void DataService::start()
{
    if (_impl->task) return;
    _impl->stop = false;
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        _impl->fetch_state = FetchState::Loading;
    }
    xTaskCreate(
        [](void* arg) {
            auto* impl = static_cast<Impl*>(arg);
            while (!impl->stop) {
                TyphoonSnapshot fetched;
                {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    impl->state.loading = true;
                }
                if (fetchActive(fetched)) {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    impl->state = fetched;
                    impl->fetch_state = FetchState::Ready;
                } else {
                    std::lock_guard<std::mutex> lock(impl->mutex);
                    impl->state.loading = false;
                    impl->fetch_state = FetchState::Failed;
                }
                for (uint32_t elapsed = 0; elapsed < kRefreshPeriodMs && !impl->stop; elapsed += 1000)
                    vTaskDelay(pdMS_TO_TICKS(1000));
            }
            impl->task = nullptr;
            vTaskDelete(nullptr);
        },
        "typhoon_nmc", 12288, _impl.get(), 3, &_impl->task);
}

void DataService::stop()
{
    if (!_impl || !_impl->task) return;
    _impl->stop = true;
    while (_impl->task) vTaskDelay(pdMS_TO_TICKS(10));
}

void DataService::copySnapshot(TyphoonSnapshot& out) const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    out = _impl->state;
}

uint32_t DataService::updatedMs() const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    return _impl->state.updated_ms;
}

FetchState DataService::fetchState() const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    return _impl->fetch_state;
}

}  // namespace typhoon
