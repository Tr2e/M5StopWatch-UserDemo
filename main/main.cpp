/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <smooth_ui_toolkit.hpp>
#include <uitk/short_namespace.hpp>
#include <mooncake_log.h>
#include <mooncake.h>
#include <apps/apps.h>
#include <hal/hal.h>
#include <lv_demos.h>
#include <apps/common/audio/audio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_sntp.h>
#include <ssid_manager.h>
#include <sys/time.h>
#include <hal/utils/settings/settings.h>
#include <wifi_manager.h>

static void sync_time_to_rtc(struct timeval*)
{
    GetHAL().syncSystemTimeToRtc();
}

static void start_network_time_sync()
{
    static bool initialized = false;
    if (!initialized) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.nist.gov");
        esp_sntp_set_time_sync_notification_cb(sync_time_to_rtc);
        esp_sntp_init();
        initialized = true;
    } else {
        esp_sntp_restart();
    }
}

static void init_wifi()
{
    auto& wifi = WifiManager::GetInstance();

    WifiManagerConfig config;
    config.ssid_prefix = "M5StopWatch";
    config.language    = "zh-CN";
    wifi.Initialize(config);

    // Once the captive portal has saved credentials and exited, resume STA mode
    // automatically. The small delay lets the HTTP response finish first.
    wifi.SetEventCallback([](WifiEvent event, const std::string&) {
        if (event == WifiEvent::Connected) {
            start_network_time_sync();
            return;
        }
        if (event != WifiEvent::ConfigModeExit) {
            return;
        }

        xTaskCreate(
            [](void*) {
                vTaskDelay(pdMS_TO_TICKS(250));
                auto& wifi = WifiManager::GetInstance();
                if (!wifi.IsConfigMode() && !SsidManager::GetInstance().GetSsidList().empty()) {
                    // Apply the timezone captured by the phone's browser before
                    // the first SNTP result is rendered on the watch face.
                    Settings settings("system", false);
                    GetHAL().setTimezone(settings.GetString("tz", "GMT0"));
                    wifi.StartStation();
                }
                vTaskDelete(nullptr);
            },
            "wifi_resume_sta", 4096, nullptr, 5, nullptr);
    });

    if (SsidManager::GetInstance().GetSsidList().empty()) {
        // First boot / no saved credentials: start the phone-based captive portal.
        wifi.StartConfigAp();
    } else {
        // Saved credentials are kept in NVS and retried with backoff by the component.
        wifi.StartStation();
    }
}

using namespace mooncake;
using namespace smooth_ui_toolkit;

extern "C" void app_main(void)
{
    // Setup logger
    mclog::set_level(mclog::level_info);
    mclog::set_time_format(mclog::time_format_unix_milliseconds);

    // HAL init
    GetHAL().init();
    init_wifi();

    // Setup ui hal
    ui_hal::on_delay([](uint32_t ms) { GetHAL().delay(ms); });
    ui_hal::on_get_tick([]() { return GetHAL().millis(); });

    // Install apps
    GetMooncake().installApp(std::make_unique<AppLauncher>());
    GetMooncake().installApp(std::make_unique<AppTyphoon>());
    GetMooncake().installApp(std::make_unique<AppAlarmClock>());
    GetMooncake().installApp(std::make_unique<AppWatchFace>());
    GetMooncake().installApp(std::make_unique<AppStopWatch>());
    GetMooncake().installApp(std::make_unique<AppBadge>());
    GetMooncake().installApp(std::make_unique<AppImu>());
    GetMooncake().installApp(std::make_unique<AppFft>());
    GetMooncake().installApp(std::make_unique<AppLuckyWheel>());
    GetMooncake().installApp(std::make_unique<AppSetup>());
    // GetMooncake().installApp(std::make_unique<AppTemplate>());

    // Main loop
    while (1) {
        GetHAL().feedTheDog();
        GetMooncake().update();
    }
}
