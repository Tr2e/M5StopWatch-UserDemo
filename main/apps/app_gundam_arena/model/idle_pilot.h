#pragma once
#include "character_model.h"
#include <cmath>

namespace gundam_arena {
inline constexpr float kAutonDelay=.80f;
inline constexpr float kKickAttend=1.20f;
inline constexpr float kStanceRetarget=.55f;

enum class ControlMode : uint8_t { Pilot, Auton };
enum class AutonSkill : uint8_t { Hold, Attend, Approach, Strike };

struct IdlePilot {
    ControlMode control=ControlMode::Pilot;
    AutonSkill skill=AutonSkill::Hold;
    float stickIdleAge=0;
    float attendKickT=0;
    float seekX=0,seekZ=0;
    bool haveSeek=false;
};

inline void resetIdlePilot(IdlePilot& p){ p={}; }

inline bool stickLive(const ArenaInput& in){
    return std::fabs(in.forward)>kStickDeadzone || std::fabs(in.turn)>kStickDeadzone;
}

inline bool ballAir(const CharacterModel& c){
    return c.ball.y>kBallR+.02f;
}

inline void goPilot(IdlePilot& p,CharacterModel& c){
    p.control=ControlMode::Pilot;
    p.skill=AutonSkill::Hold;
    p.stickIdleAge=0;
    p.attendKickT=0;
    p.haveSeek=false;
    clearLookAt(c);
    clearSeek(c);
}

inline void lookBall(CharacterModel& c){
    setLookAt(c,{c.ball.x,c.ball.y,c.ball.z});
}

inline void attendBall(IdlePilot& p,CharacterModel& c){
    p.skill=AutonSkill::Attend;
    p.haveSeek=false;
    clearSeek(c);
    lookBall(c);
}

inline void approachStance(IdlePilot& p,CharacterModel& c){
    p.skill=AutonSkill::Approach;
    lookBall(c);
    const KickStance s=kickStance(c);
    const float dx=s.x-p.seekX,dz=s.z-p.seekZ;
    const bool arrived=!c.hasWalkTo;
    if(!p.haveSeek || dx*dx+dz*dz>kStanceRetarget*kStanceRetarget || arrived){
        walkTo(c,s.x,s.z);
        p.seekX=s.x;
        p.seekZ=s.z;
        p.haveSeek=true;
    }
}

inline void autonAct(IdlePilot& p,CharacterModel& c,float dt){
    lookBall(c);
    if(characterBusy(c)){
        if(c.action==Action::Kick)p.skill=AutonSkill::Strike;
        return;
    }
    if(p.skill==AutonSkill::Strike){
        p.attendKickT=kKickAttend;
        attendBall(p,c);
        return;
    }
    if(p.attendKickT>0.f){
        p.attendKickT=p.attendKickT>dt?p.attendKickT-dt:0.f;
        attendBall(p,c);
        return;
    }
    if(ballAir(c)){
        attendBall(p,c);
        return;
    }
    if(inStrikeRange(c)){
        p.skill=AutonSkill::Strike;
        p.haveSeek=false;
        clearSeek(c);
        playKick(c);
        return;
    }
    approachStance(p,c);
}

inline void stepIdlePilot(IdlePilot& p,CharacterModel& c,const ArenaInput& in,float dt){
    const bool pose=c.mode==Mode::Pose || in.toggleMode;
    const bool clipBack=c.grounded && in.clipStep!=0;
    if(pose || clipBack || stickLive(in) || c.mode!=Mode::Play){
        goPilot(p,c);
        return;
    }
    if(p.control==ControlMode::Auton){
        autonAct(p,c,dt);
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
        autonAct(p,c,dt);
    }
}
} // namespace gundam_arena
