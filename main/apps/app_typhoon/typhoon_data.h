#pragma once

#include <cstdint>
#include <memory>

namespace typhoon {

enum class FetchState : uint8_t { Idle, Loading, Ready, Failed };

struct NmcTrackPt {
    int16_t hour = 0;
    float lat = 0.0f;
    float lon = 0.0f;
    uint16_t pressure = 0;
    uint8_t wind_kt = 0;
    uint8_t cat = 0;
    float r12_km = 0.0f;
};

// past(~10) + NOW + BABJ forecast(~8–12) — keep modest for main-task stack.
constexpr uint8_t kNmcTrackMax = 24;

struct NmcTrack {
    uint8_t n = 0;
    uint8_t now_idx = 0;
    NmcTrackPt pts[kNmcTrackMax] = {};
};

struct NmcStorm {
    char name[12] = {};
    float lat = 0.0f;
    float lon = 0.0f;
    uint8_t cat = 0;
    uint8_t wind_kt = 0;
    uint16_t pressure = 0;
    float r7 = 0.0f;
    float r10 = 0.0f;
    float r12 = 0.0f;
};

struct StormData {
    char name[16] = {};
    float lat = 0.0f;
    float lon = 0.0f;
    uint8_t category = 0;
    uint16_t wind = 0;
    uint16_t pressure = 0;
    float r7 = 0.0f;
    float r10 = 0.0f;
    float r12 = 0.0f;
};

struct TyphoonSnapshot {
    StormData storms[4] = {};
    NmcTrack tracks[4] = {};
    uint8_t count = 0;
    bool live = false;
    bool loading = false;
    uint32_t updated_ms = 0;
};

class DataService {
public:
    DataService();
    ~DataService();

    void start();
    void stop();
    // Avoid returning large structs by value on the main task stack.
    void copySnapshot(TyphoonSnapshot& out) const;
    uint32_t updatedMs() const;
    FetchState fetchState() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace typhoon
