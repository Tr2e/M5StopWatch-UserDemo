/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "utils/settings/settings.h"
#include <mooncake_log.h>
#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_AMOLED.hpp>
#include <smooth_ui_toolkit.hpp>
#include <uitk/short_namespace.hpp>
#include <memory>
#include <algorithm>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const std::string_view _tag = "HAL-Display";

/* -------------------------------------------------------------------------- */
/*                               Amoled display                               */
/* -------------------------------------------------------------------------- */
static constexpr gpio_num_t cfg_pin_sclk = GPIO_NUM_40;
static constexpr gpio_num_t cfg_pin_io0  = GPIO_NUM_41;
static constexpr gpio_num_t cfg_pin_io1  = GPIO_NUM_42;
static constexpr gpio_num_t cfg_pin_io2  = GPIO_NUM_46;
static constexpr gpio_num_t cfg_pin_io3  = GPIO_NUM_45;
static constexpr gpio_num_t cfg_pin_cs   = GPIO_NUM_39;
static constexpr gpio_num_t cfg_pin_te   = GPIO_NUM_38;
static constexpr gpio_num_t cfg_pin_rst  = GPIO_NUM_NC;

class StopWatchFrameBuffer : public lgfx::Panel_AMOLED_Framebuffer {
public:
    explicit StopWatchFrameBuffer(lgfx::Panel_AMOLED* panel):Panel_AMOLED_Framebuffer(panel) {}
    ~StopWatchFrameBuffer() override {
        shutdownAsync();
    }
    uint8_t* line(uint_fast16_t y) {
        return _lines_buffer && y<_cfg.panel_height ? _lines_buffer[y] : nullptr;
    }
    void mark(uint_fast16_t x,uint_fast16_t y,uint_fast16_t w,uint_fast16_t h) {
        if(!w || !h)return;
        if(x<_range_mod.left)_range_mod.left=x;
        if(x+w-1>_range_mod.right)_range_mod.right=x+w-1;
        if(y<_range_mod.top)_range_mod.top=y;
        if(y+h-1>_range_mod.bottom)_range_mod.bottom=y+h-1;
    }
    bool setAsync(bool enabled) {
        if(enabled==_async)return enabled ? _asyncReady : true;
        if(enabled) {
            if(!prepareAsync())return false;
            _async=true;return true;
        }
        waitPresent();_async=false;
        // Keep the framebuffer API on the pixels that were most recently
        // submitted when the direct-rendered app hands control back to LVGL.
        selectRenderBuffer(_lastSubmitted);
        return true;
    }
    uint32_t lastPresentUs() const{return _lastPresentUs;}

    void display(uint_fast16_t x,uint_fast16_t y,uint_fast16_t w,uint_fast16_t h) override {
        if(!_async) {
            Panel_AMOLED_Framebuffer::display(x,y,w,h);return;
        }
        if(w && h) {
            _range_mod.left=std::min<int_fast16_t>(_range_mod.left,x);
            _range_mod.right=std::max<int_fast16_t>(_range_mod.right,x+w-1);
            _range_mod.top=std::min<int_fast16_t>(_range_mod.top,y);
            _range_mod.bottom=std::max<int_fast16_t>(_range_mod.bottom,y+h-1);
        }
        if(_range_mod.empty())return;

        // Two buffers are sufficient because a complete RX-78 render is much
        // longer than one panel transfer. Wait only when the previous transfer
        // has not completed before the next finished frame is submitted.
        waitPresent();
        _pendingLeft=uint16_t(_range_mod.left)&~1u;
        _pendingTop=uint16_t(_range_mod.top)&~1u;
        const uint16_t right=uint16_t(_range_mod.right)|1u;
        const uint16_t bottom=uint16_t(_range_mod.bottom)|1u;
        _pendingWidth=right-_pendingLeft+1;
        _pendingHeight=bottom-_pendingTop+1;
        _pendingBuffer=_buffers[_renderIndex];
        _lastSubmitted=_renderIndex;
        _range_mod.top=INT16_MAX;_range_mod.left=INT16_MAX;
        _range_mod.right=0;_range_mod.bottom=0;
        selectRenderBuffer(_renderIndex^1u);
        _inFlight=true;
        xSemaphoreGive(_presentStart);
    }

private:
    static void presentTask(void* argument) {
        auto& framebuffer=*static_cast<StopWatchFrameBuffer*>(argument);
        for(;;) {
            xSemaphoreTake(framebuffer._presentStart,portMAX_DELAY);
            if(framebuffer._stopping)break;
            const auto started=esp_timer_get_time();
            framebuffer.presentPending();
            framebuffer._lastPresentUs=uint32_t(esp_timer_get_time()-started);
            xSemaphoreGive(framebuffer._presentDone);
        }
        xSemaphoreGive(framebuffer._presentDone);
        vTaskDelete(nullptr);
    }
    bool prepareAsync() {
        if(_asyncReady)return true;
        if(!_frame_buffer || !_lines_buffer || _cfg.panel_height<1)return false;
        _buffers[0]=_frame_buffer;
        _stride=_cfg.panel_height>1 ? std::size_t(_lines_buffer[1]-_lines_buffer[0]) :
            std::size_t((_cfg.panel_width+3)&~3u)*(_write_bits>>3);
        _buffers[1]=static_cast<uint8_t*>(lgfx::heap_alloc_psram(_stride*_cfg.panel_height));
        if(!_buffers[1])return false;
        _presentStart=xSemaphoreCreateBinary();
        _presentDone=xSemaphoreCreateBinary();
        if(!_presentStart || !_presentDone ||
           xTaskCreatePinnedToCore(presentTask,"display_present",4096,this,
                                  tskIDLE_PRIORITY+3,&_presentTask,1)!=pdPASS) {
            shutdownAsync();return false;
        }
        _renderIndex=0;_lastSubmitted=0;_asyncReady=true;return true;
    }
    void selectRenderBuffer(uint8_t index) {
        if(!_buffers[index])return;
        _renderIndex=index;_frame_buffer=_buffers[index];
        for(uint_fast16_t y=0;y<_cfg.panel_height;++y)
            _lines_buffer[y]=_frame_buffer+std::size_t(y)*_stride;
    }
    void waitPresent() {
        if(!_inFlight)return;
        xSemaphoreTake(_presentDone,portMAX_DELAY);_inFlight=false;
    }
    void presentPending() {
        _panel->setWindow(_pendingLeft,_pendingTop,
                          _pendingLeft+_pendingWidth-1,_pendingTop+_pendingHeight-1);
        auto* bus=_panel->getBus();
        const std::size_t rowBytes=std::size_t(_pendingWidth)*(_write_bits>>3);
        // Amortize QSPI DMA descriptor setup while retaining two alternating
        // internal buffers. The framebuffer stride includes the panel padding,
        // so pack a few visible rows rather than transmitting that padding.
        constexpr uint_fast16_t rowsPerDma=4;
        const std::size_t dmaBytes=rowBytes*rowsPerDma;
        uint8_t* dma[2]{bus->getDMABuffer(dmaBytes),bus->getDMABuffer(dmaBytes)};
        _panel->start_qspi();
        const auto* source=_pendingBuffer+std::size_t(_pendingTop)*_stride+
                           std::size_t(_pendingLeft)*(_write_bits>>3);
        for(uint_fast16_t row=0,chunk=0;row<_pendingHeight;row+=rowsPerDma,++chunk) {
            const auto rows=std::min<uint_fast16_t>(rowsPerDma,_pendingHeight-row);
            auto* output=dma[chunk&1u];
            for(uint_fast16_t packed=0;packed<rows;++packed)
                std::memcpy(output+std::size_t(packed)*rowBytes,
                            source+std::size_t(row+packed)*_stride,rowBytes);
            bus->writeBytes(output,rowBytes*rows,false,true);
        }
        bus->wait();
        _panel->end_qspi();
    }
    void shutdownAsync() {
        if(_asyncReady) {
            waitPresent();_async=false;_stopping=true;
            xSemaphoreGive(_presentStart);
            xSemaphoreTake(_presentDone,portMAX_DELAY);
            _presentTask=nullptr;
        }
        if(_presentStart){vSemaphoreDelete(_presentStart);_presentStart=nullptr;}
        if(_presentDone){vSemaphoreDelete(_presentDone);_presentDone=nullptr;}
        if(_buffers[0])selectRenderBuffer(0);
        if(_buffers[1]){lgfx::heap_free(_buffers[1]);_buffers[1]=nullptr;}
        _buffers[0]=nullptr;_asyncReady=false;_stopping=false;_inFlight=false;
    }
    uint8_t* _buffers[2]{};
    uint8_t* _pendingBuffer=nullptr;
    std::size_t _stride=0;
    SemaphoreHandle_t _presentStart=nullptr,_presentDone=nullptr;
    TaskHandle_t _presentTask=nullptr;
    uint16_t _pendingLeft=0,_pendingTop=0,_pendingWidth=0,_pendingHeight=0;
    volatile uint32_t _lastPresentUs=0;
    uint8_t _renderIndex=0,_lastSubmitted=0;
    bool _async=false,_asyncReady=false,_inFlight=false,_stopping=false;
};

class Panel_CO5300 : public lgfx::Panel_AMOLED {
public:
    Panel_CO5300(void)
    {
        _cfg.memory_width = _cfg.panel_width = 480;
        _cfg.memory_height = _cfg.panel_height = 480;
        _write_depth                           = lgfx::color_depth_t::rgb565_2Byte;
        _read_depth                            = lgfx::color_depth_t::rgb565_2Byte;
    }

    bool initPanelFb() {
        if(_panel_fb)return true;
        _panel_fb=new StopWatchFrameBuffer(this);
        _panel_fb->config(_cfg);
        _panel_fb->setColorDepth(_write_depth);
        _panel_fb->setRotation(getRotation());
        return _panel_fb->init(false);
    }
    uint8_t* frameBufferLine(uint_fast16_t y) {
        return _panel_fb ? static_cast<StopWatchFrameBuffer*>(_panel_fb)->line(y) : nullptr;
    }
    void markFrameBuffer(uint_fast16_t x,uint_fast16_t y,uint_fast16_t w,uint_fast16_t h) {
        if(_panel_fb)static_cast<StopWatchFrameBuffer*>(_panel_fb)->mark(x,y,w,h);
    }

    const uint8_t *getInitCommands(uint8_t listno) const override
    {
        static constexpr uint8_t list0[] = {
            0x11, 0 + CMD_INIT_DELAY,
            150,  // Sleep out
            0xC4, 1,
            0x80, 0x35,
            1,    0x80,
            0x44, 2,
            0x01, 0xD2,  // Tear Effect Line = 0x1D2 == 466
            0x53, 1,
            0x20, 0x20,
            0,    0x36,
            1,    0,
            0x51, 1,
            0xA0, 0x29,
            0,    0xff,
            0xff  // end
        };
        switch (listno) {
            case 0:
                return list0;
            default:
                return nullptr;
        }
    }
};

class M5StopWatch : public M5GFX {
    lgfx::Bus_SPI _bus_instance;
    Panel_CO5300 _panel_instance;
    bool _frame_buffer_available = false;

public:
    M5StopWatch(void)
    {
    }

    bool hasFrameBuffer() const
    {
        return _frame_buffer_available;
    }

    uint8_t* frameBufferLine(uint_fast16_t y) {return _panel_instance.frameBufferLine(y);}
    void markFrameBuffer(uint_fast16_t x,uint_fast16_t y,uint_fast16_t w,uint_fast16_t h) {
        _panel_instance.markFrameBuffer(x,y,w,h);
    }
    bool setFrameBufferAsync(bool enabled) {
        auto* panel=_panel_instance.getPanelFb();
        return panel && static_cast<StopWatchFrameBuffer*>(panel)->setAsync(enabled);
    }
    uint32_t frameBufferPresentUs() {
        auto* panel=_panel_instance.getPanelFb();
        return panel ? static_cast<const StopWatchFrameBuffer*>(panel)->lastPresentUs() : 0;
    }

    // static constexpr int in_i2c_port                   = 0;  // I2C_NUM_0

    bool init_impl(bool use_reset, bool use_clear) override
    {
        {
            auto cfg = _bus_instance.config();

            cfg.freq_write = 80000000;
            cfg.freq_read  = 10000000;  // irrelevant

            cfg.pin_sclk = cfg_pin_sclk;
            cfg.pin_io0  = cfg_pin_io0;
            cfg.pin_io1  = cfg_pin_io1;
            cfg.pin_io2  = cfg_pin_io2;
            cfg.pin_io3  = cfg_pin_io3;

            cfg.spi_host    = SPI2_HOST;
            cfg.spi_mode    = 0;  // SPI_MODE0;
            cfg.spi_3wire   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg         = _panel_instance.config();
            cfg.pin_rst      = cfg_pin_rst;
            cfg.pin_cs       = cfg_pin_cs;
            cfg.panel_width  = 468;
            cfg.panel_height = 466;
            cfg.offset_x     = 6;
            cfg.offset_y     = 0;

            cfg.readable = false;

            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);

        lgfx::pinMode(cfg_pin_te, lgfx::pin_mode_t::input_pullup);
        // lgfx::i2c::init(in_i2c_port);

        // io_expander.digitalWrite(PY32_L3B_EN_PIN, 1);
        // io_expander.digitalWrite(PY32_OLED_RST_PIN, 1);

        if (!LGFX_Device::init_impl(use_reset, use_clear)) return false;

        _frame_buffer_available = enableFrameBuffer(true);

        _panel_instance.setBrightness(128);

        return true;
    }

    bool enableFrameBuffer(bool auto_display = false)
    {
        if (_panel_instance.initPanelFb()) {
            auto fbPanel = _panel_instance.getPanelFb();
            if (fbPanel) {
                fbPanel->setBus(&_bus_instance);
                fbPanel->setAutoDisplay(auto_display);
                setPanel(fbPanel);
                return true;
            }
        }
        return false;
    }

    void disableFrameBuffer()
    {
        auto fbPanel = _panel_instance.getPanelFb();
        if (fbPanel) {
            _panel_instance.deinitPanelFb();
            setPanel(&_panel_instance);
        }
    }

    void setBrightness(uint8_t brightness)
    {
        _panel_instance.setBrightness(brightness);
    }
};

static std::unique_ptr<M5StopWatch> _display;
static std::unique_ptr<LGFX_Sprite> _canvas;

void Hal::display_init()
{
    mclog::tagInfo(_tag, "display init");

    _display = std::make_unique<M5StopWatch>();
    if (!_display->init()) {
        mclog::tagError(_tag, "display init failed");
        _display.reset();
        _display_frame_buffer_available = false;
        return;
    }
    _display_frame_buffer_available = _display->hasFrameBuffer();

    mclog::tagInfo(_tag, "create full screen canvas");
    _canvas = std::make_unique<LGFX_Sprite>(_display.get());
    _canvas->setPsram(true);
    if (!_canvas->createSprite(_display->width(), _display->height())) {
        mclog::tagError(_tag, "canvas init failed");
        _canvas.reset();
    }

    // Load brightness from settings
    auto brightness = getBackLightBrightness(true);
    setBackLightBrightness(brightness, false);
}

LGFX_Device &Hal::getDisplay()
{
    return *_display;
}

LGFX_Sprite &Hal::getCanvas()
{
    return *_canvas;
}

bool Hal::hasDisplayFrameBuffer() const
{
    return _display_frame_buffer_available;
}

uint8_t* Hal::getDisplayFrameBufferLine(int y)
{
    return _display && y>=0 ? _display->frameBufferLine(uint_fast16_t(y)) : nullptr;
}

void Hal::markDisplayFrameBufferModified(int x,int y,int width,int height)
{
    if(_display && x>=0 && y>=0 && width>0 && height>0)
        _display->markFrameBuffer(uint_fast16_t(x),uint_fast16_t(y),uint_fast16_t(width),uint_fast16_t(height));
}

bool Hal::setDisplayFrameBufferAsync(bool enabled)
{
    return _display && _display->setFrameBufferAsync(enabled);
}

uint32_t Hal::getDisplayFrameBufferPresentUs() const
{
    return _display ? _display->frameBufferPresentUs() : 0;
}

void Hal::updateCanvas()
{
    _canvas->pushSprite(0, 0);
}

void Hal::updateCanvasRegion(int x,int y,int width,int height)
{
    int32_t oldX,oldY,oldWidth,oldHeight;
    _display->getClipRect(&oldX,&oldY,&oldWidth,&oldHeight);
    const int left=std::max(x,int(oldX)),top=std::max(y,int(oldY));
    const int right=std::min(x+width,int(oldX+oldWidth));
    const int bottom=std::min(y+height,int(oldY+oldHeight));
    if(right<=left || bottom<=top)return;
    // Destination clipping preserves the full sprite's source stride and
    // RGB565 conversion. Restore the previous clip for every other app.
    _display->setClipRect(left,top,right-left,bottom-top);
    _canvas->pushSprite(0,0);
    _display->setClipRect(oldX,oldY,oldWidth,oldHeight);
}

void Hal::setBackLightBrightness(int brightness, bool saveToSettings)
{
    _bl_brightness = uitk::clamp(brightness, 0, 100);

    int set_target = uitk::map_range(_bl_brightness, 0, 100, 0, 255);
    _display->setBrightness(set_target);

    if (saveToSettings) {
        Settings settings(std::string(Hal::SettingsNs), true);
        settings.SetInt("bl_lev", _bl_brightness);
        mclog::tagInfo(_tag, "brightness saved to settings: {}", _bl_brightness);
    }
}

int Hal::getBackLightBrightness(bool loadFromSettings)
{
    if (loadFromSettings) {
        Settings settings(std::string(Hal::SettingsNs), false);
        _bl_brightness = settings.GetInt("bl_lev", 80);
        _bl_brightness = uitk::clamp(_bl_brightness, 10, 100);
        mclog::tagInfo(_tag, "brightness loaded from settings: {}", _bl_brightness);
    }
    return _bl_brightness;
}

/* -------------------------------------------------------------------------- */
/*                                  Touchpad                                  */
/* -------------------------------------------------------------------------- */
#include "drivers/cst820/cst820.h"

static std::unique_ptr<Cst820> _cst820;

void Hal::touchpad_init()
{
    mclog::tagInfo(_tag, "touchpad init");

    ioe_tp_reset();

    _cst820 = std::make_unique<Cst820>();
    if (!_cst820->begin(i2c_bus_get_internal_bus_handle(_i2c_bus))) {
        mclog::tagError(_tag, "touchpad init failed");
        _cst820.reset();
    }
}

Hal::TouchPoint Hal::getTouchPoint()
{
    Hal::TouchPoint point;
    if (_cst820 && _cst820->read()) {
        point.valid = true;
        point.num = _cst820->isPressed() ? _cst820->getFingerNum() : 0;
        if (point.num > 0) {
            point.x = _cst820->getX();
            point.y = _cst820->getY();
        }
    }
    return point;
}

/* -------------------------------------------------------------------------- */
/*                                    Lvgl                                    */
/* -------------------------------------------------------------------------- */
// https://github.com/m5stack/lv_m5_emulator/blob/main/src/utility/lvgl_port_m5stack.cpp
#include <cstdlib>  // for aligned_alloc
#include <cstring>  // for memset
#include <lvgl.h>
#include <atomic>

static SemaphoreHandle_t xGuiSemaphore;
static std::atomic<bool> _lvgl_update_enabled = false;

#define LV_BUFFER_LINE 120

static void lvgl_tick_timer(void *arg)
{
    (void)arg;
    lv_tick_inc(10);
}

static void lvgl_rtos_task(void *pvParameter)
{
    (void)pvParameter;
    while (1) {
        if (_lvgl_update_enabled && pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    M5GFX &gfx = *(M5GFX *)lv_display_get_driver_data(disp);

    uint32_t w      = (area->x2 - area->x1 + 1);
    uint32_t h      = (area->y2 - area->y1 + 1);
    uint32_t pixels = w * h;

    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);

    // Critical fix: Use safe pixel writing method to avoid M5GFX SIMD optimizations
    // Break large transfers into small chunks to avoid problematic copy_rgb_fast function
    const uint32_t SAFE_CHUNK_SIZE = 8192;  // 8K pixels per chunk, suitable for small buffer settings

    if (pixels > SAFE_CHUNK_SIZE) {
        // Chunked transmission for large data
        const lgfx::rgb565_t *src = (const lgfx::rgb565_t *)px_map;
        uint32_t remaining        = pixels;
        uint32_t offset           = 0;

        while (remaining > 0) {
            uint32_t chunk_size = (remaining > SAFE_CHUNK_SIZE) ? SAFE_CHUNK_SIZE : remaining;
            gfx.writePixels(src + offset, chunk_size);
            offset += chunk_size;
            remaining -= chunk_size;
        }
    } else {
        // Direct transmission for small data
        gfx.writePixels((lgfx::rgb565_t *)px_map, pixels);
    }

    gfx.endWrite();

    lv_display_flush_ready(disp);
}

static void lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    M5GFX &gfx = *(M5GFX *)lv_indev_get_driver_data(indev);

    auto tp = GetHAL().getTouchPoint();
    if (tp.num == 0) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;
    }
}

void Hal::lvgl_init()
{
    mclog::tagInfo(_tag, "lvgl init");

    lv_init();

    static lv_display_t *disp = lv_display_create(_display->width(), _display->height());
    if (disp == NULL) {
        printf("lv_display_create failed\n");
        return;
    }

    lv_display_set_driver_data(disp, _display.get());
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    static uint8_t *buf1 = (uint8_t *)heap_caps_malloc(_display->width() * LV_BUFFER_LINE, MALLOC_CAP_SPIRAM);
    static uint8_t *buf2 = (uint8_t *)heap_caps_malloc(_display->width() * LV_BUFFER_LINE, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, (void *)buf1, (void *)buf2, _display->width() * LV_BUFFER_LINE,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lvTouchpad = lv_indev_create();
    LV_ASSERT_MALLOC(lvTouchpad);
    if (lvTouchpad == NULL) {
        printf("lv_indev_create failed\n");
        return;
    }
    lv_indev_set_driver_data(lvTouchpad, _display.get());
    lv_indev_set_type(lvTouchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvTouchpad, lvgl_read_cb);
    lv_indev_set_display(lvTouchpad, disp);

    xGuiSemaphore                                     = xSemaphoreCreateMutex();
    const esp_timer_create_args_t periodic_timer_args = {.callback = &lvgl_tick_timer, .name = "lvgl_tick_timer"};
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000));
    xTaskCreate(lvgl_rtos_task, "lvgl_rtos_task", 4096 * 4, NULL, 1, NULL);

    startLvglUpdate();

    {
        LvglLockGuard lock;
        uitk::lvgl_cpp::ScreenActive screen;
        screen.setBgColor(lv_color_black());
        GetHAL().bootLogo = std::make_unique<BootLogo>();
    }
}

bool Hal::lvglLock()
{
    return xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE ? true : false;
}

void Hal::lvglUnlock()
{
    xSemaphoreGive(xGuiSemaphore);
}

void Hal::startLvglUpdate()
{
    _lvgl_update_enabled = true;
}

void Hal::stopLvglUpdate()
{
    _lvgl_update_enabled = false;
}
