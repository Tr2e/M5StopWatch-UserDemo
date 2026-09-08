#pragma once
#include "../model/car_display_mesh.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace lets_and_go {
inline uint16_t carTint(uint16_t c,float light) {
    const int r=std::clamp(int(((c>>11)&31)*light),0,31);
    const int g=std::clamp(int(((c>>5)&63)*light),0,63);
    const int b=std::clamp(int((c&31)*light),0,31);
    return uint16_t((r<<11)|(g<<5)|b);
}
// Small UV-space lettering. It is painted on the actual shell/wing surface,
// so it foreshortens and is occluded with the model rather than floating UI.
inline bool carLetter(const char* text,float u,float v,float left,float top,float width,float height) {
    if(u<left || v<top || u>=left+width || v>=top+height)return false;
    static constexpr uint8_t font[26][5]={
        {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
        {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
        {65,65,127,65,65},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
        {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
        {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
        {63,64,64,64,63},{31,32,64,32,31},{127,32,24,32,127},{99,20,8,20,99},
        {3,4,120,4,3},{97,81,73,69,67}};
    const int length=int(std::strlen(text));
    const int x=int((u-left)/width*length*6),y=int((v-top)/height*7);
    const int letter=x/6,col=x%6;
    return letter<length && col<5 && y<7 && text[letter]>='A' && text[letter]<='Z' &&
           (font[text[letter]-'A'][col]&(1u<<y));
}

inline uint16_t carPaintColor(CarPaint paint,uint16_t base,float u,float v) {
    constexpr uint16_t white=0xf7be,blue=0x3275,red=0xc9a7,gold=0xe5ca,dark=0x2946;
    const float center=std::abs(u-.5f);
    switch(paint) {
    case CarPaint::Solid:return base;
    case CarPaint::Glass:
    case CarPaint::BronzeGlass:
    case CarPaint::BlueGlass: {
        const uint16_t glass=paint==CarPaint::BronzeGlass ? 0xacce :
                             paint==CarPaint::BlueGlass ? 0x2a98 : 0x3188;
        if(u<.025f || u>.975f || v<.025f || v>.975f)return 0xb5d6;
        const float streak=std::max(0.f,1-std::abs(u-(.34f+.12f*v))*7);
        return carTint(glass,.60f+streak*.60f+(1-v)*.16f);
    }
    case CarPaint::MagnumWing:
    case CarPaint::SonicWing:
    case CarPaint::TridaggerWing: {
        const char* name=paint==CarPaint::MagnumWing ? "MAGNUM" :
                         paint==CarPaint::SonicWing ? "SONIC" : "ZMC";
        const uint16_t color=paint==CarPaint::MagnumWing ? blue :
                             paint==CarPaint::SonicWing ? red : dark;
        if(v<.075f || v>.93f || u<.025f || u>.975f)return white;
        if(carLetter(name,u,v,.11f,.26f,.78f,.52f))return paint==CarPaint::MagnumWing ? gold : white;
        return color;
    }
    case CarPaint::MagnumHood: {
        // Broad blue shoulders beside the cockpit become one tapered nose wedge.
        const float band=v<.43f ? .47f : .32f-.20f*((v-.43f)/.57f);
        uint16_t color=center<band ? blue : white;
        if(v>.57f && v<.94f && center>band+.065f && center<band+.12f)color=red;
        if(carLetter("MAGNUM",u,v,.28f,.55f,.44f,.047f))color=white;
        if(v>.65f && v<.745f && center<.08f && int(v*140+u*90)%3)color=gold;
        return color;
    }
    case CarPaint::SonicHood: {
        const float band=v<.40f ? .46f : .39f-.16f*v;
        uint16_t color=center<band ? red : white;
        if(v>.56f && v<.92f && center>band+.045f && center<band+.075f)color=red;
        if(carLetter("SONIC",u,v,.29f,.53f,.42f,.065f))color=white;
        if(v>.64f && v<.73f && center<.06f && int(v*130+u*80)%3)color=gold;
        return color;
    }
    case CarPaint::MagnumCowl: {
        // u always runs inner -> outer on BOTH pods. The white/red lightning
        // therefore mirrors across the car instead of blue appearing on one inner wall.
        const float bolt=v<.25f ? .20f+v*.75f : v<.43f ? .39f-(v-.25f)*.9f :
                         v<.60f ? .23f+(v-.43f)*1.7f : .52f-(v-.60f)*.8f;
        if(u>.77f-.12f*v)return blue;
        if(v>.07f && v<.94f && std::abs(u-bolt)<.07f*(1-.5f*v))return red;
        if(v>.12f && v<.82f && std::abs(u-(bolt+.23f))<.045f)return red;
        return white;
    }
    case CarPaint::MagnumCanopy:
        if(u<.04f || u>.96f || v<.035f || v>.96f)return white;
        return carTint(0x3188,.72f+.40f*std::max(0.f,1-std::abs(u-.36f)*3));
    case CarPaint::MagnumCowlSide:
        return (v>.12f && v<.84f && u>.2f && u<.4f) ? red : blue;
    case CarPaint::MagnumNoseSide:
        return v<.48f ? blue : carTint(white,.79f);
    case CarPaint::MagnumVent:
        return u<.06f || u>.94f || v<.08f || v>.92f ? 0x7bef : 0x18c3;
    case CarPaint::SonicCowl: {
        if(v<.44f)return u<.08f || u>.93f ? white : red;
        const float vent=.31f*(1-(v-.51f)/.44f);
        if(v>.51f && v<.94f && std::abs(u-.52f)<vent)
            return std::abs(u-.52f)>vent-.035f ? 0x7bef :
                   (int(u*110)+int(v*100))%3 ? 0x5acb : dark;
        return u>.88f ? red : white;
    }
    case CarPaint::SonicCanopy:
        if(u<.035f || u>.965f || v<.025f || v>.97f)return white;
        return carTint(0x3188,.7f+.5f*std::max(0.f,1-std::abs(u-.34f)*3));
    case CarPaint::SonicSide:
        return u<.18f ? white : u<.30f ? red : 0x246d;
    case CarPaint::SonicFront:
        if(u<.045f || u>.95f)return white;
        if(v>.80f)return 0x246d;
        if(v>.74f)return white;
        if(v>.19f && v<.65f && std::abs(u-.48f)<.15f+.07f*v)
            return (int(u*70)+int(v*80))%5<2 ? dark : v<.35f ? gold : 0xb5d6;
        return red;
    case CarPaint::Eye:
        if(u<.06f || u>.94f || v<.10f || v>.94f)return base;
        if(v>.16f && center<.055f+.25f*v) {
            if(v>.73f && int(u*44+v*55)%4<2)return dark;
            return center<.035f+.25f*v ? gold : white;
        }
        return base;
    case CarPaint::Flame: {
        const float repeat=u*2.1f+.13f;
        const float tooth=1-std::abs((repeat-std::floor(repeat))*2-1);
        const float tip=.25f+.67f*tooth;
        if(v<tip && v>.04f) {
            if(v>tip-.13f)return 0xeeca;
            if(v>tip-.25f)return 0xe3e7;
            return red;
        }
        return base;
    }
    case CarPaint::Tiger: {
        const float stripe=v*4.3f+u*.9f;
        const float f=stripe-std::floor(stripe);
        return f<.18f && (center>.14f || v<.35f) ? dark : red;
    }
    case CarPaint::BrockenHood:
        if(carLetter("BROCKEN",u,v,.08f,.62f,.84f,.21f))return white;
        if(u>.25f && u<.75f && v>.1f && v<.51f)
            return (u<.34f || v<.20f || v>.40f || (u>.59f && v>.26f)) ? gold : dark;
        return red;
    case CarPaint::BrockenShell:
        if(v>.30f && v<.64f && center<.34f-.12f*(v-.30f))
            return carPaintColor(CarPaint::BlueGlass,blue,(u-.14f)/.72f,(v-.30f)/.34f);
        return carPaintColor(CarPaint::Tiger,red,u,v);
    case CarPaint::FrontWing:
        if(carLetter("SONIC",u,v,.28f,.55f,.44f,.22f))return 0x18c3;
        return carTint(0x7bef,.78f+.20f*(1-v));
    case CarPaint::NeoHood: {
        const float border=.38f-.10f*v;
        if(v>.48f && center>border-.04f && center<border)return red;
        if(v>.48f && center>border && center<border+.014f)return white;
        if(v>.39f && v<.54f && center<.075f) {
            const float y=(v-.39f)/.15f;
            if(y<.16f || y>.84f || (y>.42f && y<.57f) ||
               (y<.5f ? u>.54f : u<.46f))return red;
            return white;
        }
        return base;
    }
    }
    return base;
}
} // namespace lets_and_go
