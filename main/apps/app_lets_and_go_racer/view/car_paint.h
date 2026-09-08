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
    case CarPaint::Count:
    case CarPaint::Solid:return base;
    case CarPaint::CobraFlame: {
        const float zig=v<.35f ? .24f+v*.8f : v<.6f ? .52f-(v-.35f)*1.1f : .245f+(v-.6f)*.6f;
        const float width=.17f*(1.f-v*.7f);
        if(std::abs(u-zig)<width)return gold;
        if(std::abs(u-zig)<width+.055f)return red;
        return blue;
    }
    case CarPaint::CobraHood:
        if(v>.17f && v<.72f && center<.26f) {
            if(std::abs(std::fmod(v*8.f+center*.7f,1.f))<.10f)return 0x7bef;
            return carTint(0xdedb, .83f+.17f*(1-center*2));
        }
        return blue;
    case CarPaint::CobraBridge:
        return carLetter("SPIN COBRA",u,v,.10f,.20f,.80f,.60f) ? white : blue;
    case CarPaint::CobraLamp: {
        const float ellipse=(u-.5f)*(u-.5f)*3.5f+(v-.5f)*(v-.5f)*3.5f;
        if(ellipse>.70f)return dark;
        if(ellipse>.46f)return 0xbdf7;
        return (int(u*14)+int(v*11))%3==0 ? white : 0x7bef;
    }
    case CarPaint::SpiderWeb: {
        const float x=u-.5f,y=v-.35f;
        const float ring=std::fmod((std::abs(x)+std::abs(y)*.70f)*4.f,1.f);
        const float spoke=std::min({std::abs(x),std::abs(y),std::abs(x-y*.7f),std::abs(x+y*.7f)});
        if(ring<.035f || spoke<.008f)return white;
        if(ring<.075f || spoke<.018f)return 0x44bf;
        return 0x1082;
    }
    case CarPaint::SpiderCowl:
        return u>.48f-v*.30f && u<.80f-v*.16f ? red : 0x1082;
    case CarPaint::SpiderWing:
        return carLetter("BEAK SPIDER",u,v,.06f,.22f,.88f,.56f) ? white : 0x1082;
    case CarPaint::StingerHood:
        if(v>.40f && v<.82f && center<.27f &&
           std::fmod(v*12.f+center*7.f,1.f)<.27f)return 0x1082;
        return carTint(0xbdf7,.82f+.18f*(1-center*2));
    case CarPaint::StingerCowl: {
        const float crack=std::fmod(u*5.f+std::abs(v-.4f)*4.f,1.f);
        return crack<.055f || std::fmod(v*6.f+std::abs(u-.3f)*2.f,1.f)<.04f ? red : 0xbdf7;
    }
    case CarPaint::StingerLamp:
        return u<.04f || u>.96f || v<.06f || v>.94f ? dark :
            int(u*10)%2 ? red : 0xfbcf;
    case CarPaint::DiospadaHood:
        if(center<.23f+.05f*v) {
            if(center<.025f && v>.37f)return red;
            return (int(u*110)+int(v*100))%3 ? 0x4208 : 0x8410;
        }
        if(v<.45f && center>.32f && center<.40f)return white;
        return red;
    case CarPaint::DiospadaWing:
        return carLetter("DIOSPADA",u,v,.07f,.23f,.86f,.49f) ? white : red;
    case CarPaint::DiospadaSide:
        if(v>.12f && v<.33f && u>.12f && u<.82f)return white;
        return red;
    case CarPaint::DiospadaLouver:
        if(v<.30f && u>.14f && u<.90f && int(u*11)%3==1)return dark;
        return red;
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
    case CarPaint::NeoWingLeft:
        if(v<.06f || v>.95f)return 0xb5d6;
        return carLetter("TRIDAGGER",u,v,.06f,.3f,.90f,.43f) ? white : dark;
    case CarPaint::NeoCanopy:
        if(u<.03f || u>.97f || v<.025f || v>.97f ||
           std::abs(u-.17f)<.011f || std::abs(u-.83f)<.011f)return 0x7bef;
        return carTint(0xac6d,.78f+.28f*(1-u));
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
        // Irregular red tongues with narrow yellow tips, not repeated yellow leaves.
        constexpr float tips[]={.77f,.93f,.76f,.52f,.57f,.39f,.48f,.74f,.88f,.67f,.80f,.62f};
        const float along=std::clamp(u,0.f,1.f)*11;
        const int segment=std::min(10,int(along));
        const float tip=tips[segment]+(tips[segment+1]-tips[segment])*(along-segment);
        if(v<tip && v>.04f) {
            if(v>tip-.04f)return 0xeeca;
            if(v>tip-.085f)return 0xe3e7;
            return red;
        }
        return base;
    }
    case CarPaint::Tiger: {
        const float jag=v<.35f ? v*.35f : v<.65f ? .12f-(v-.35f)*.6f : -.06f+(v-.65f)*.3f;
        const float stripe=u+jag;
        return (std::abs(stripe-.25f)<.055f+.035f*(1-v) ||
                std::abs(stripe-.71f)<.035f+.055f*v) ? dark : red;
    }
    case CarPaint::BrockenHood:
        if(carLetter("BROCKEN",u,v,.08f,.62f,.84f,.21f))return white;
        if(u>.25f && u<.75f && v>.1f && v<.51f)
            return carLetter("G",u,v,.27f,.11f,.46f,.40f) ? gold : dark;
        return red;
    case CarPaint::BrockenShell:
        if(v>.30f && v<.64f && center<.34f-.12f*(v-.30f))
            return carPaintColor(CarPaint::BlueGlass,blue,(u-.14f)/.72f,(v-.30f)/.34f);
        return carPaintColor(CarPaint::Tiger,red,u,v);
    case CarPaint::BrockenCabin:
        if(v>.49f && v<.97f && center<.47f)
            return carPaintColor(CarPaint::BlueGlass,blue,u,(v-.49f)/.48f);
        if(v>.23f && v<.47f && center>.24f)
            return int(v*36)%3==0 ? dark : red;
        return red;
    case CarPaint::BrockenCabinSide:
        if(v>.49f && v<.97f && u>.45f && u<.94f)
            return carPaintColor(CarPaint::BlueGlass,blue,.70f,(v-.49f)/.48f);
        return red;
    case CarPaint::BrockenLamp:
        if(u<.03f || u>.97f || v<.12f || v>.88f)return dark;
        for(float eye : {.27f,.73f}) {
            const float oval=(u-eye)*(u-eye)+(v-.47f)*(v-.47f)*.16f;
            if(std::abs(oval-.020f)<.007f)return dark;
        }
        return gold;
    case CarPaint::BrockenArmor:
        if(u<.07f || u>.93f || v<.05f || v>.95f)return dark;
        return (std::abs(u-.5f)<.13f && std::abs(v-.45f)<.12f) ? dark : 0x94b2;
    case CarPaint::FrontWing:
        if(carLetter("SONIC",u,v,.28f,.55f,.44f,.22f))return 0x18c3;
        return carTint(0x7bef,.78f+.20f*(1-v));
    case CarPaint::NeoHood: {
        const float border=.38f-.10f*v;
        if(v>.58f) {
            const float chevron=v-center*.29f;
            for(float stripe : {.63f,.74f,.85f}) {
                if(std::abs(chevron-stripe)<.010f)return red;
                if(std::abs(chevron-stripe-.017f)<.004f)return white;
            }
        }
        if(v>.48f && v<.65f && center>border-.04f && center<border)return red;
        if(v>.69f && v<.76f && center<.13f) {
            const float y=(v-.69f)/.07f;
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
