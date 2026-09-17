#pragma once
#include "character_model.h"
#include <cmath>

namespace gundam_arena {
inline constexpr float kAutonDelay=.80f;

enum class ControlMode : uint8_t { Pilot, Auton };

struct IdlePilot {
    ControlMode control=ControlMode::Pilot;
    float stickIdleAge=0;
};

inline void resetIdlePilot(IdlePilot& p){ p={}; }

inline bool stickLive(const ArenaInput& in){
    return std::fabs(in.forward)>kStickDeadzone || std::fabs(in.turn)>kStickDeadzone;
}

inline void stepIdlePilot(IdlePilot& p,CharacterModel& c,const ArenaInput& in,float dt){
    const bool pose=c.mode==Mode::Pose || in.toggleMode;
    const bool clipBack=c.grounded && in.clipStep!=0;
    if(pose || clipBack || stickLive(in)){
        p.control=ControlMode::Pilot;
        p.stickIdleAge=0;
        clearLookAt(c);
        return;
    }
    if(c.mode!=Mode::Play){
        p.control=ControlMode::Pilot;
        p.stickIdleAge=0;
        clearLookAt(c);
        return;
    }
    if(p.control==ControlMode::Auton){
        setLookAt(c,{c.ball.x,c.ball.y,c.ball.z});
        return;
    }
    clearLookAt(c);
    if(!c.grounded || (c.action==Action::Jump && c.grounded)){
        p.stickIdleAge=0;
        return;
    }
    p.stickIdleAge+=dt;
    if(p.stickIdleAge>=kAutonDelay){
        p.control=ControlMode::Auton;
        setLookAt(c,{c.ball.x,c.ball.y,c.ball.z});
    }
}
} // namespace gundam_arena
