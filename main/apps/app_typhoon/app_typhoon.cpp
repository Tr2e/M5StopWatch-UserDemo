#include "app_typhoon.h"
#include "typhoon_view.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>

using namespace mooncake;

AppTyphoon::AppTyphoon()
{
    setAppInfo().name = "Typhoon";
    setAppInfo().icon = (void*)&icon_typhoon;
}

void AppTyphoon::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppTyphoon::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    _keys = std::make_unique<input::KeyManager>();
    GetHAL().stopLvglUpdate();
    GetHAL().updateImuData();
    _view = std::make_unique<typhoon::View>();
    _view->open();
    // Delay NMC task until after first frames — same spirit as StickS3
    // (fetch only after WiFi/boot), avoids heap/stack pressure on open.
    _data = std::make_unique<typhoon::DataService>();
    _data_started = false;
    _last_fetch_state = typhoon::FetchState::Idle;
    _open_ms = GetHAL().millis();
    _last_snap_ms = 0;
}

void AppTyphoon::onRunning()
{
    GetHAL().updateButtonStates();
    if (!_view) return;

    if (_keys && _keys->update(false) == input::KeyEvent::GoHome) {
        close();
        return;
    }

    if (_data && !_data_started && GetHAL().millis() - _open_ms > 1500) {
        _data->start();
        _data_started = true;
    }

    if (_data && _data_started) {
        const auto fetch_state = _data->fetchState();
        if (fetch_state != _last_fetch_state) {
            _last_fetch_state = fetch_state;
            _view->setFetchState(fetch_state);
        }
        const uint32_t updated = _data->updatedMs();
        if (updated != 0 && updated != _last_snap_ms) {
            typhoon::TyphoonSnapshot snap;
            _data->copySnapshot(snap);
            _last_snap_ms = updated;
            _view->setSnapshot(snap);
        }
    }

    GetHAL().updateImuData();
    _view->update();
}

void AppTyphoon::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    _data.reset();
    _view.reset();
    GetHAL().getDisplay().fillScreen(TFT_BLACK);
    GetHAL().startLvglUpdate();
    _keys.reset();
}
