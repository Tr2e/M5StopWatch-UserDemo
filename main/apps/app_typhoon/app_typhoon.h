#pragma once

#include <apps/common/key_manager/key_manager.h>
#include <mooncake.h>
#include <memory>
#include "typhoon_view.h"
#include "typhoon_data.h"

class AppTyphoon : public mooncake::AppAbility {
public:
    AppTyphoon();
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _keys;
    std::unique_ptr<typhoon::View> _view;
    std::unique_ptr<typhoon::DataService> _data;
    bool _data_started = false;
    uint32_t _open_ms = 0;
    uint32_t _last_snap_ms = 0;
};
