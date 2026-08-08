/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "workers.h"
#include <assets/assets.h>
#include <mooncake_log.h>
#include <ssid_manager.h>
#include <wifi_manager.h>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace setup_workers;

class WifiWorker::WifiView {
public:
    WifiView()
    {
        _panel = std::make_unique<Container>(lv_screen_active());
        _panel->setSize(466, 466);
        _panel->setAlign(LV_ALIGN_CENTER);
        _panel->setRadius(0);
        _panel->setBorderWidth(0);
        _panel->setPadding(42, 92, 42, 42);
        _panel->setBgColor(lv_color_black());
        _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

        _title = std::make_unique<Label>(_panel->get());
        _title->setTextFont(&MontserratSemiBold26);
        _title->setTextColor(lv_color_hex(0xFFFFFF));
        _title->align(LV_ALIGN_TOP_MID, 0, 0);
        _title->setText("Wi-Fi");

        _status = std::make_unique<Label>(_panel->get());
        _status->setTextFont(&lv_font_montserrat_18);
        _status->setTextColor(lv_color_hex(0xFFFFFF));
        _status->align(LV_ALIGN_TOP_MID, 0, 72);
        _status->setWidth(380);
        _status->setTextAlign(LV_TEXT_ALIGN_CENTER);

        _detail = std::make_unique<Label>(_panel->get());
        _detail->setTextFont(&lv_font_montserrat_14);
        _detail->setTextColor(lv_color_hex(0xBFBFBF));
        _detail->align(LV_ALIGN_TOP_MID, 0, 150);
        _detail->setWidth(380);
        _detail->setTextAlign(LV_TEXT_ALIGN_CENTER);
    }

    void setText(const std::string& status, const std::string& detail)
    {
        _status->setText(status);
        _detail->setText(detail);
    }

private:
    std::unique_ptr<Container> _panel;
    std::unique_ptr<Label> _title;
    std::unique_ptr<Label> _status;
    std::unique_ptr<Label> _detail;
};

WifiWorker::WifiWorker()
{
    auto& wifi        = WifiManager::GetInstance();
    _restore_station  = !SsidManager::GetInstance().GetSsidList().empty();

    if (_restore_station || !wifi.IsConfigMode()) {
        wifi.StartConfigAp();
    }

    _view             = std::make_unique<WifiView>();
    _next_update_tick = 0;
}

WifiWorker::~WifiWorker()
{
    auto& wifi = WifiManager::GetInstance();
    if (wifi.IsConfigMode() && _restore_station) {
        wifi.StopConfigAp();
        wifi.StartStation();
    }
}

void WifiWorker::update()
{
    const auto now = GetHAL().millis();
    if (now < _next_update_tick || !_view) {
        return;
    }
    _next_update_tick = now + 500;

    auto& wifi = WifiManager::GetInstance();
    const bool has_saved_wifi = !SsidManager::GetInstance().GetSsidList().empty();

    // The portal exits only after the H5 form has been submitted successfully.
    // Once the saved network is connected, return all the way to Launcher.
    if (!_config_exit_seen && !wifi.IsConfigMode() && has_saved_wifi) {
        _config_exit_seen = true;
    }
    if (_config_exit_seen && wifi.IsConnected()) {
        _exit_to_launcher = true;
        _is_done           = true;
        return;
    }

    if (wifi.IsConfigMode()) {
        _view->setText(
            "Connect phone to device Wi-Fi",
            fmt::format("{}\nOpen http://192.168.4.1\nThe phone may open this page automatically.", wifi.GetApSsid()));
    } else if (wifi.IsConnected()) {
        _view->setText("Wi-Fi connected", fmt::format("{}\nIP: {}", wifi.GetSsid(), wifi.GetIpAddress()));
    } else if (has_saved_wifi) {
        _view->setText("Connecting...", "Saved Wi-Fi is being retried.");
    } else {
        _view->setText("Wi-Fi setup", "Connect to the device Wi-Fi to configure it.");
    }
}
