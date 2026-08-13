/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "workers.h"
#include <apps/common/network/wifi_service.h>
#include <assets/assets.h>
#include <wifi_manager.h>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace setup_workers;

namespace {

void style_action_button(Button& button, uint32_t color, uint32_t text_color)
{
    button.setSize(374, 64);
    button.setRadius(32);
    button.setBorderWidth(0);
    button.setShadowWidth(0);
    button.setBgColor(lv_color_hex(color));
    button.label().setTextFont(&lv_font_montserrat_18);
    button.label().setTextColor(lv_color_hex(text_color));
    button.label().align(LV_ALIGN_CENTER, 0, 0);
}

}  // namespace

class WifiWorker::WifiView {
public:
    WifiView()
    {
        _panel = std::make_unique<Container>(lv_screen_active());
        _panel->setSize(466, 466);
        _panel->setAlign(LV_ALIGN_CENTER);
        _panel->setRadius(0);
        _panel->setBorderWidth(0);
        _panel->setPaddingAll(0);
        _panel->setBgColor(lv_color_black());
        _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

        _title = std::make_unique<Label>(_panel->get());
        _title->setTextFont(&MontserratSemiBold26);
        _title->setTextColor(lv_color_hex(0xFFFFFF));
        _title->align(LV_ALIGN_TOP_MID, 0, 54);

        _status = std::make_unique<Label>(_panel->get());
        _status->setTextFont(&lv_font_montserrat_18);
        _status->setTextColor(lv_color_hex(0xFFFFFF));
        _status->align(LV_ALIGN_TOP_MID, 0, 112);
        _status->setWidth(380);
        _status->setTextAlign(LV_TEXT_ALIGN_CENTER);

        _detail = std::make_unique<Label>(_panel->get());
        _detail->setTextFont(&lv_font_montserrat_14);
        _detail->setTextColor(lv_color_hex(0xBFBFBF));
        _detail->align(LV_ALIGN_TOP_MID, 0, 164);
        _detail->setWidth(380);
        _detail->setTextAlign(LV_TEXT_ALIGN_CENTER);

        _primary = std::make_unique<Button>(_panel->get());
        _primary->align(LV_ALIGN_BOTTOM_MID, 0, -112);
        style_action_button(*_primary, 0x4AD78C, 0x0F5831);
        _primary->onClick().connect([this]() { _primary_requested = true; });

        _secondary = std::make_unique<Button>(_panel->get());
        _secondary->align(LV_ALIGN_BOTTOM_MID, 0, -34);
        style_action_button(*_secondary, 0x343434, 0xFFFFFF);
        _secondary->onClick().connect([this]() { _secondary_requested = true; });
    }

    void update(const network::WifiStatus& status)
    {
        _title->setText(network::wifiStateTitle(status.state));
        std::string detail = status.detail;
        if (status.state == network::WifiState::Provisioning) {
            detail = "Join " + WifiManager::GetInstance().GetApSsid() + "\nOpen " +
                     WifiManager::GetInstance().GetApWebUrl();
        } else if (status.state == network::WifiState::Online) {
            detail = status.ssid + "\nIP: " + status.ip_address +
                     (status.time_synced ? "\nTime synchronized" : "\nSynchronizing time…");
        }
        _status->setText(network::wifiStateDetail(status.state));
        _detail->setText(detail);

        const bool provisioning = status.state == network::WifiState::Provisioning;
        const bool online = status.state == network::WifiState::Online;
        const bool configured = status.has_saved_network;
        _primary->label().setText(provisioning ? "Cancel setup"
                                  : online ? "Reconnect"
                                  : configured ? "Retry connection" : "Set up Wi-Fi");
        _secondary->label().setText(configured ? "Forget saved networks" : "Cancel setup");
    }

    bool consumePrimaryRequested()
    {
        const bool requested = _primary_requested;
        _primary_requested = false;
        return requested;
    }

    bool consumeSecondaryRequested()
    {
        const bool requested = _secondary_requested;
        _secondary_requested = false;
        return requested;
    }

private:
    std::unique_ptr<Container> _panel;
    std::unique_ptr<Label> _title;
    std::unique_ptr<Label> _status;
    std::unique_ptr<Label> _detail;
    std::unique_ptr<Button> _primary;
    std::unique_ptr<Button> _secondary;
    bool _primary_requested = false;
    bool _secondary_requested = false;
};

WifiWorker::WifiWorker()
{
    _view = std::make_unique<WifiView>();
    _next_update_tick = 0;
}

WifiWorker::~WifiWorker() = default;

void WifiWorker::update()
{
    auto& service = network::WifiService::GetInstance();
    const auto status = service.getStatus();

    if (_view->consumePrimaryRequested()) {
        if (status.state == network::WifiState::Provisioning) {
            service.cancelProvisioning();
        } else if (status.has_saved_network) {
            service.retryConnection();
        } else {
            service.beginProvisioning();
        }
    }
    if (_view->consumeSecondaryRequested()) {
        if (status.has_saved_network) {
            service.forgetSavedNetworks();
        } else {
            service.cancelProvisioning();
            _is_done = true;
            return;
        }
    }

    const auto now = GetHAL().millis();
    if (now >= _next_update_tick) {
        _next_update_tick = now + 250;
        _view->update(service.getStatus());
    }
}
