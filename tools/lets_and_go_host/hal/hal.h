#pragma once

// Host-only pixel sink. Production renderers are compiled unchanged against
// this HAL; no screenshot geometry is reimplemented in the test driver.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace host_font {
#define PROGMEM
#include "../../../components/M5GFX/src/lgfx/Fonts/glcdfont.h"
#undef PROGMEM
}
enum class textdatum_t { middle_center };

class LGFX_Device {
public:
    struct Text { std::string value; int x, y, size; };
    std::vector<Text> texts;
    int width() const { return _width; }
    int height() const { return _height; }
    void startWrite() { ++writeDepth; }
    void endWrite() {
        if (writeDepth > 0 && --writeDepth == 0 && autoDisplay) ++displayCount;
    }
    void setAutoDisplay(bool enabled) { autoDisplay = enabled; }
    void display() { ++displayCount; }
    void getClipRect(int32_t* x,int32_t* y,int32_t* w,int32_t* h) const {
        *x=clipX;*y=clipY;*w=clipW;*h=clipH;
    }
    void setClipRect(int32_t x,int32_t y,int32_t w,int32_t h) {
        clipX=std::max<int32_t>(0,x);clipY=std::max<int32_t>(0,y);
        clipW=std::max<int32_t>(0,std::min<int32_t>(_width-clipX,w));
        clipH=std::max<int32_t>(0,std::min<int32_t>(_height-clipY,h));
    }
    unsigned physicalDisplays() const { return displayCount; }
    void resetDisplayCount() { displayCount=0; }
    void setPsram(bool) {}
    void setColorDepth(int) {}
    void* createSprite(int width, int height) {
        _width=width; _height=height;
        clipX=clipY=0;clipW=width;clipH=height;
        pixels.assign(width*height,0); return pixels.data();
    }
    void* getBuffer() { return pixels.data(); }
    void fillScreen(uint16_t color) { std::fill(pixels.begin(),pixels.end(),color); texts.clear(); }
    void pixel(int x, int y, uint16_t color) {
        if (x >= clipX && y >= clipY && x < clipX+clipW && y < clipY+clipH &&
            x >= 0 && y >= 0 && x < width() && y < height()) pixels[y * width() + x] = color;
    }
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            pixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            const int twice = error * 2;
            if (twice >= dy) { error += dy; x0 += sx; }
            if (twice <= dx) { error += dx; y0 += sy; }
        }
    }
    void fillRect(int x, int y, int w, int h, uint16_t color) {
        for (int py = std::max(0, y); py < std::min(height(), y + h); ++py)
            for (int px = std::max(0, x); px < std::min(width(), x + w); ++px) pixel(px, py, color);
    }
    void pushImage(int x,int y,int w,int h,const uint16_t* colors) {
        for(int row=0;row<h;++row)for(int col=0;col<w;++col)
            pixel(x+col,y+row,colors[row*w+col]);
    }
    void pushImage(int x,int y,int w,int h,const uint16_t* colors,uint16_t transparent) {
        for(int row=0;row<h;++row)for(int col=0;col<w;++col)
            if(colors[row*w+col]!=transparent)pixel(x+col,y+row,colors[row*w+col]);
    }
    void drawRect(int x, int y, int w, int h, uint16_t color) {
        drawLine(x, y, x + w - 1, y, color); drawLine(x, y, x, y + h - 1, color);
        drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
        drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    }
    void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
        drawLine(x + r, y, x + w - r - 1, y, color);
        drawLine(x + r, y + h - 1, x + w - r - 1, y + h - 1, color);
        drawLine(x, y + r, x, y + h - r - 1, color);
        drawLine(x + w - 1, y + r, x + w - 1, y + h - r - 1, color);
        for (int px = 0; px <= r; ++px) {
            const int py = static_cast<int>(std::lround(std::sqrt(r * r - px * px)));
            for (int sx : {-1, 1}) for (int sy : {-1, 1}) {
                pixel((sx < 0 ? x + r : x + w - r - 1) + sx * px,
                      (sy < 0 ? y + r : y + h - r - 1) + sy * py, color);
            }
        }
    }
    void drawCircle(int x, int y, int r, uint16_t color) {
        for (int i = 0; i < 720; ++i) {
            const float a = i * 0.00872664626f;
            pixel(x + std::lround(std::cos(a) * r), y + std::lround(std::sin(a) * r), color);
        }
    }
    void fillCircle(int x, int y, int r, uint16_t color) {
        for (int py = -r; py <= r; ++py) for (int px = -r; px <= r; ++px)
            if (px * px + py * py <= r * r) pixel(x + px, y + py, color);
    }
    void fillTriangle(int ax, int ay, int bx, int by, int cx, int cy, uint16_t color) {
        const auto edge = [](int x, int y, int u, int v, int px, int py) {
            return static_cast<int64_t>(px - x) * (v - y) - static_cast<int64_t>(py - y) * (u - x);
        };
        if (edge(ax, ay, bx, by, cx, cy) == 0) return;
        for (int y = std::max(0, std::min({ay, by, cy})); y <= std::min(height() - 1, std::max({ay, by, cy})); ++y)
            for (int x = std::max(0, std::min({ax, bx, cx})); x <= std::min(width() - 1, std::max({ax, bx, cx})); ++x) {
                const auto a = edge(ax, ay, bx, by, x, y), b = edge(bx, by, cx, cy, x, y), c = edge(cx, cy, ax, ay, x, y);
                if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) pixel(x, y, color);
            }
    }
    void setTextDatum(textdatum_t) {}
    void setTextSize(int value) { textSize = value; }
    void setTextColor(uint16_t fg, uint16_t bg) { foreground = fg; background = bg; }
    void drawString(const char* value, int x, int y) {
        const std::string s(value);
        texts.push_back({s, x, y, textSize});
        int left = x - static_cast<int>(s.size()) * 6 * textSize / 2;
        const int top = y - 4 * textSize;
        fillRect(left, top, static_cast<int>(s.size()) * 6 * textSize, 8 * textSize, background);
        for (unsigned char ch : s) {
            for (int col = 0; col < 5; ++col) for (int row = 0; row < 8; ++row)
                if (host_font::font[ch * 5 + col] & (1u << row))
                    fillRect(left + col * textSize, top + row * textSize, textSize, textSize, foreground);
            left += 6 * textSize;
        }
    }
    void save(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        out << "P6\n" << width() << ' ' << height() << "\n255\n";
        const int radius=std::min(width(),height())/2;
        for (int y = 0; y < height(); ++y) for (int x = 0; x < width(); ++x) {
            const uint16_t c = (x-width()/2)*(x-width()/2)+(y-height()/2)*(y-height()/2)<radius*radius
                                   ? pixels[y * width() + x] : 0;
            const unsigned char rgb[] = {static_cast<unsigned char>(((c >> 11) & 31) * 255 / 31),
                static_cast<unsigned char>(((c >> 5) & 63) * 255 / 63), static_cast<unsigned char>((c & 31) * 255 / 31)};
            out.write(reinterpret_cast<const char*>(rgb), 3);
        }
    }
    const auto& frame() const { return pixels; }
private:
    int _width=466, _height=466;
    std::vector<uint16_t> pixels=std::vector<uint16_t>(466u*466u);
    int textSize = 1;
    uint16_t foreground = 0, background = 0xffff;
    int32_t clipX=0,clipY=0,clipW=466,clipH=466;
    unsigned writeDepth=0,displayCount=0;
    bool autoDisplay=true;
};
namespace lgfx { using LGFXBase = ::LGFX_Device; }
using LGFX_Sprite = LGFX_Device;
struct HostHal {
    LGFX_Device canvas;
    LGFX_Device& getDisplay() { return canvas; }
    LGFX_Sprite& getCanvas() { return canvas; }
    bool hasDisplayFrameBuffer() const { return displayFrameBufferAvailable; }
    void setDisplayFrameBufferAvailable(bool available) { displayFrameBufferAvailable=available; }
    void updateCanvas() {}
private:
    bool displayFrameBufferAvailable=true;
};
inline HostHal& GetHAL() { static HostHal hal; return hal; }
