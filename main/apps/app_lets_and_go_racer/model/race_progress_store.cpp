#include "race_progress_store.h"

#include <nvs.h>

namespace lets_and_go {
namespace {

constexpr const char* kNamespace = "letsgo";
constexpr const char* kProgressKey = "progress";

}  // namespace

PlayerProgress RaceProgressStore::load()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return {};
    std::array<uint8_t, sizeof(PlayerProgress)> bytes{};
    std::size_t size = bytes.size();
    const esp_err_t result = nvs_get_blob(handle, kProgressKey, bytes.data(), &size);
    nvs_close(handle);
    if (result != ESP_OK) return {};
    return decodePlayerProgress(bytes.data(), size);
}

bool RaceProgressStore::save(const PlayerProgress& progress)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const PlayerProgress sanitized = sanitizePlayerProgress(progress);
    const bool written = nvs_set_blob(handle, kProgressKey, &sanitized,
                                      sizeof(sanitized)) == ESP_OK &&
                         nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return written;
}

}  // namespace lets_and_go
