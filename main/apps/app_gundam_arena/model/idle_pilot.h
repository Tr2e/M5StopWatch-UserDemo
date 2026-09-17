#pragma once
#include "character_model.h"
#include "../view/arena_space.h"
#include <cmath>

namespace gundam_arena {
inline constexpr float kAutonDelay=.80f;
inline constexpr float kKickAttend=1.20f;
inline constexpr float kStanceRetarget=.55f;
inline constexpr float kLeapDist=1.8f;
inline constexpr float kLeapCooldown=4.f;
inline constexpr float kSignalCooldown=12.f;
inline constexpr float kOperatorMemory=8.f;
inline constexpr float kStruckRecent=1.20f;
inline constexpr float kSignalBallSpeed=1.5f;

enum class ControlMode : uint8_t { Pilot, Auton };
enum class AutonSkill : uint8_t { Hold, Attend, Approach, Strike, Leap, Signal };

struct IdlePilot {
    ControlMode control=ControlMode::Pilot;
    AutonSkill skill=AutonSkill::Hold;
    float stickIdleAge=0;
    float attendKickT=0;
    float seekX=0,seekZ=0;
    bool haveSeek=false;
    float operatorMemory=0;
    float leapCooldown=0;
    float signalCooldown=0;
    float struckRecent=0;
};

inline void resetIdlePilot(IdlePilot& p){ p={}; }

inline bool stickLive(const ArenaInput& in){
    return std::fabs(in.forward)>kStickDeadzone || std::fabs(in.turn)>kStickDeadzone;
}

inline bool ballAir(const CharacterModel& c){
    return c.ball.y>kBallR+.02f;
}

inline float ballHorizDist(const CharacterModel& c){
    const float dx=c.ball.x-c.x,dz=c.ball.z-c.z;
    return std::sqrt(dx*dx+dz*dz);
}

inline float ballHorizSpeed(const CharacterModel& c){
    return std::sqrt(c.ball.vx*c.ball.vx+c.ball.vz*c.ball.vz);
}

inline bool operatorPresent(const IdlePilot& p){ return p.operatorMemory>0.f; }

inline void decayTimer(float& t,float dt){ t=t>dt?t-dt:0.f; }

inline bool inLeapTrigger(const IdlePilot& p,const CharacterModel& c){
    if(!c.grounded || characterBusy(c))return false;
    if(inStrikeRange(c))return false;
    if(!ballAir(c))return false;
    if(ballHorizDist(c)>=kLeapDist)return false;
    if(p.leapCooldown>0.f)return false;
    if(c.ball.vy<0.f)return true;
    return p.struckRecent>0.f;
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

inline void lookCamera(CharacterModel& c,const ArenaView* view){
    if(!view){lookBall(c);return;}
    const Point e=ArenaCamera(*view).eye();
    setLookAt(c,e);
}

inline void attendBall(IdlePilot& p,CharacterModel& c){
    p.skill=AutonSkill::Attend;
    p.haveSeek=false;
    clearSeek(c);
    lookBall(c);
}

inline int waveGestureId(const CharacterModel& c,const ArenaView* view){
    if(!view)return 0;
    const Point e=ArenaCamera(*view).eye();
    const float dx=e.x-c.x,dz=e.z-c.z;
    const float localX=dx*std::cos(c.heading)+dz*(-std::sin(c.heading));
    return localX<0.f?0:1;
}

inline void playSignal(IdlePilot& p,CharacterModel& c,const ArenaView* view,bool celebrate){
    p.skill=AutonSkill::Signal;
    p.haveSeek=false;
    clearSeek(c);
    playGesture(c,celebrate?5:waveGestureId(c,view));
    lookCamera(c,view);
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

inline void autonAct(IdlePilot& p,CharacterModel& c,float dt,const ArenaView* view){
    lookBall(c);
    if(characterBusy(c)){
        if(c.action==Action::Kick)p.skill=AutonSkill::Strike;
        else if(c.action==Action::Gesture)p.skill=AutonSkill::Signal;
        else if(c.action==Action::Jump || c.action==Action::Fall || c.action==Action::Land)
            p.skill=AutonSkill::Leap;
        return;
    }
    if(p.skill==AutonSkill::Strike){
        p.attendKickT=kKickAttend;
        attendBall(p,c);
        return;
    }
    if(p.skill==AutonSkill::Leap){
        p.leapCooldown=kLeapCooldown;
        attendBall(p,c);
        return;
    }
    if(p.skill==AutonSkill::Signal){
        p.signalCooldown=kSignalCooldown;
        attendBall(p,c);
        return;
    }
    if(p.attendKickT>0.f){
        p.attendKickT=p.attendKickT>dt?p.attendKickT-dt:0.f;
        attendBall(p,c);
        return;
    }
    if(inLeapTrigger(p,c)){
        p.skill=AutonSkill::Leap;
        p.haveSeek=false;
        clearSeek(c);
        playJump(c);
        lookBall(c);
        return;
    }
    if(ballAir(c)){
        if(operatorPresent(p) && p.signalCooldown<=0.f && p.struckRecent>0.f
           && ballHorizSpeed(c)>kSignalBallSpeed){
            playSignal(p,c,view,true);
            return;
        }
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
    if(p.skill==AutonSkill::Hold && operatorPresent(p) && p.signalCooldown<=0.f){
        playSignal(p,c,view,false);
        return;
    }
    approachStance(p,c);
}

inline void stepIdlePilot(IdlePilot& p,CharacterModel& c,const ArenaInput& in,float dt,
                          const ArenaView* view=nullptr,bool orbitCue=false){
    decayTimer(p.leapCooldown,dt);
    decayTimer(p.signalCooldown,dt);
    decayTimer(p.struckRecent,dt);
    decayTimer(p.operatorMemory,dt);
    if(c.ball.struck)p.struckRecent=kStruckRecent;
    if(stickLive(in) || (c.grounded && in.clipStep!=0) || orbitCue)
        p.operatorMemory=kOperatorMemory;

    const bool pose=c.mode==Mode::Pose || in.toggleMode;
    const bool clipBack=c.grounded && in.clipStep!=0;
    if(pose || clipBack || stickLive(in) || c.mode!=Mode::Play){
        goPilot(p,c);
        return;
    }
    if(p.control==ControlMode::Auton){
        autonAct(p,c,dt,view);
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
        autonAct(p,c,dt,view);
    }
}
} // namespace gundam_arena
