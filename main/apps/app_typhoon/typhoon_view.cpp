#include "typhoon_view.h"
#include "typhoon_engine.h"

#include <hal/hal.h>

namespace typhoon {

constexpr int kCenter = 233;

View::View() : _engine(std::make_unique<Engine>()) {}

View::~View()
{
    close();
}

void View::open()
{
    _engine->open();
}

void View::close()
{
    if (_engine) _engine->close();
}

void View::update()
{
    if (!_engine) return;
    _engine->update();

    LGFX_Sprite* legacy = _engine->legacySprite();
    if (!legacy) return;

    const float scale = _engine->presentScale();
    auto& canvas = GetHAL().getCanvas();
    canvas.fillScreen(TFT_BLACK);
    GetHAL().feedTheDog();
    legacy->pushRotateZoom(&canvas, static_cast<float>(kCenter), static_cast<float>(kCenter),
                           0.0f, scale, scale);
    GetHAL().feedTheDog();
    GetHAL().updateCanvas();
}

void View::setSnapshot(const TyphoonSnapshot& snapshot)
{
    if (_engine) _engine->applySnapshot(snapshot);
}

}  // namespace typhoon
