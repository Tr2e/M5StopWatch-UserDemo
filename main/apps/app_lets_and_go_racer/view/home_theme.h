#pragma once
#include "../controller/home_layout.h"
#include "../input/racer_input.h"
#include <hal/hal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace lets_and_go::home_theme {
inline constexpr uint16_t background=0x1083, panel=0x18e5, line=0x39c8;
inline constexpr uint16_t white=0xf7be, muted=0xa554, red=0xe187, blue=0x2c5f;
inline constexpr uint16_t green=0x6e0f;

inline void label(lgfx::LGFXBase& canvas,const char* text,int x,int y,int size,
                  uint16_t color=white,uint16_t fill=background) {
    canvas.setTextDatum(textdatum_t::middle_center);
    canvas.setTextSize(size);canvas.setTextColor(color,fill);canvas.drawString(text,x,y);
}
inline void backdrop(lgfx::LGFXBase& canvas) {
    canvas.fillScreen(background);
}
inline void stripes(lgfx::LGFXBase& canvas,int center,int y) {
    canvas.fillRect(center-48,y,44,4,red);
    canvas.fillRect(center+4,y,44,4,blue);
}
inline void action(lgfx::LGFXBase& canvas,home_layout::Rect rect,const char* title,
                   const char* hint=nullptr,uint16_t fill=red) {
    canvas.fillRect(rect.x,rect.y,rect.width,rect.height,fill);
    canvas.fillTriangle(rect.x,rect.y,rect.x+10,rect.y,rect.x,rect.y+10,background);
    canvas.fillTriangle(rect.x+rect.width-1,rect.y+rect.height-1,
        rect.x+rect.width-11,rect.y+rect.height-1,
        rect.x+rect.width-1,rect.y+rect.height-11,background);
    const int cx=rect.x+rect.width/2;
    label(canvas,title,cx,rect.y+(hint ? 22 : rect.height/2),2,white,fill);
    if(hint)label(canvas,hint,cx,rect.y+44,1,white,fill);
}
inline void setupHeader(lgfx::LGFXBase& canvas,const char* title,bool back=true,uint16_t fill=background) {
    label(canvas,"MINI 4WD",canvas.width()/2,43,2,white,fill);
    label(canvas,title,canvas.width()/2,70,1,muted,fill);
    if(back) {
        canvas.drawLine(123,63,116,70,muted);
        canvas.drawLine(116,70,123,77,muted);
    }
}
inline void arrows(lgfx::LGFXBase& canvas,int y) {
    canvas.fillCircle(123,y,20,panel);
    canvas.fillCircle(canvas.width()-123,y,20,panel);
    canvas.fillTriangle(117,y,129,y-7,129,y+7,red);
    canvas.fillTriangle(canvas.width()-117,y,canvas.width()-129,y-7,canvas.width()-129,y+7,blue);
}
inline void entry(lgfx::LGFXBase& canvas,bool calibrating,const RacerInputStatus& status) {
    backdrop(canvas);const int cx=canvas.width()/2;
    label(canvas,"RACE GARAGE",cx,46,1,muted);
    label(canvas,"MINI 4WD",cx,80,3);
    stripes(canvas,cx,108);
    label(canvas,calibrating ? "CENTER JOYSTICK" : "MINI 4WD",cx,151,calibrating ? 2 : 4);
    if(calibrating) {
        const float progress=std::isfinite(status.calibrationProgress) ?
            std::clamp(status.calibrationProgress,0.f,1.f) : 0.f;
        canvas.fillRect(cx-100,179,200,8,line);
        canvas.fillRect(cx-100,179,int(200*progress),8,blue);
        label(canvas,"KEEP STILL TO CALIBRATE",cx,201,1,muted);
    } else label(canvas,"CHOOSE YOUR CONTROLS",cx,187,1,muted);
    action(canvas,home_layout::deviceAction,"PLAY ON DEVICE","B / TAP TO ENTER");
    canvas.fillRect(cx-139,294,278,85,panel);
    label(canvas,"EXTERNAL CONTROLS",cx,307,1,muted,panel);
    label(canvas,status.axesConnected ? "JOYSTICK2: CONNECTED" : "JOYSTICK2: WAITING",
          cx,331,1,status.axesConnected ? green : muted,panel);
    label(canvas,status.actionsConfigured ? "BUTTONS: CONFIGURED" : "BUTTONS: CHECK SETTINGS",
          cx,354,1,status.actionsConfigured ? white : muted,panel);
    label(canvas,status.readiness==RacerInputReadiness::Fault ? "INPUT FAULT - CHECK CABLE" :
          "AUTO START WHEN READY",cx,393,1,
          status.readiness==RacerInputReadiness::Fault ? red : muted);
    label(canvas,"HOLD A+B TO EXIT",cx,423,1,muted);
}
} // namespace lets_and_go::home_theme
