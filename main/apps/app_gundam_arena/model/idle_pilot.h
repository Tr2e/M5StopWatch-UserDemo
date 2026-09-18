#pragma once
#include "character_model.h"
#include "arena_log.h"
#include "../view/arena_space.h"
#include <algorithm>
#include <cmath>

namespace gundam_arena {
inline constexpr float kAutonDelay=.80f;
inline constexpr float kKickAttend=1.20f;
inline constexpr float kStrikeThink=.60f;
inline constexpr float kStanceRetarget=.55f;
inline constexpr float kMissInhibit=2.f;
inline constexpr float kMissInhibitCap=.20f;
inline constexpr float kLeapDist=4.f;
inline constexpr float kLeapCooldown=4.f;
inline constexpr float kSignalCooldown=12.f;
inline constexpr float kOperatorMemory=8.f;
inline constexpr float kStruckRecent=3.f;
inline constexpr float kSignalBallSpeed=1.5f;
inline constexpr float kVigilanceRise=.40f;
inline constexpr float kVigilanceFall=2.f;
inline constexpr float kCuriosityTau=.60f;
inline constexpr float kPlayRise=1.f;
inline constexpr float kPlayFall=.50f;
inline constexpr float kComposureFall=4.f;
inline constexpr float kHoldMin=2.f;
inline constexpr float kAttendMin=1.20f;
inline constexpr float kFaceMin=1.50f;
inline constexpr float kApproachMin=.80f;
inline constexpr float kPreemptEarly=.25f;
inline constexpr float kPreemptLate=.08f;
inline constexpr float kFaceBoostErr=.35f;
inline constexpr float kHoldLook=.25f;
inline constexpr float kPlayInhibit=1.50f;
inline constexpr float kPlayInhibitCap=.25f;
inline constexpr float kDoubleStrikeWindow=4.f;
inline constexpr float kDoubleStrikeInhibit=2.f;
inline constexpr float kDoubleStrikeCap=.12f;
inline constexpr float kCelebrateBoost=.55f;
inline constexpr float kGaitDashDist=8.f;
inline constexpr float kGaitRunDist=3.f;
inline constexpr float kGaitNearWalk=2.5f;
inline constexpr float kBrakeDashRun=.35f;
inline constexpr float kBrakeRunWalk=.45f;
inline constexpr float kBrakeWalkStop=.35f;
inline constexpr float kWallPad=1.2f;
inline constexpr float kGaitStop=.08f;
inline constexpr float kOperatorFade=3.f;
inline constexpr int kGestureBow=6;
inline constexpr int kGestureBye=7;
inline constexpr int kGestureBye2=8;
inline constexpr int kGesturePunch=10;
inline constexpr int kSkillCount=7;

enum class ControlMode : uint8_t { Pilot, Auton };
enum class AutonSkill : uint8_t { Hold, Attend, Face, Approach, Strike, Leap, Signal };
enum class AutonGait : uint8_t { Stop, Walk, Run, Dash };

struct IdlePilot {
    ControlMode control=ControlMode::Pilot;
    AutonSkill skill=AutonSkill::Hold;
    AutonGait gait=AutonGait::Stop;
    float stickIdleAge=0;
    float attendKickT=0;
    float seekX=0,seekZ=0,seekHeading=0;
    float seekBallX=0,seekBallZ=0;
    bool haveSeek=false;
    float operatorMemory=0;
    float leapCooldown=0;
    float signalCooldown=0;
    float struckRecent=0;
    float vigilance=0,curiosity=0,play=0,social=0,composure=0;
    float skillAge=0;
    float skillScore=0;
    float idleAge=0;
    float lastStrikeRecent=0;
    float playInhibit=0;
    float playCap=1.f;
    float thinkT=0;
    uint32_t autonTick=0;
    bool kickConnected=false;
    bool braking=false;
    float brakeT=0,brakeDur=0,brakeFrom=0,brakeTo=0;
    AutonGait brakeNext=AutonGait::Stop;
    AutonGait brakeGoal=AutonGait::Stop;
    bool havePending=false;
    AutonSkill pendingSkill=AutonSkill::Hold;
    float pendingScore=0;
};

struct SkillScores {
    float v[kSkillCount]{};
    float operator[](AutonSkill s) const { return v[int(s)]; }
    float& operator[](AutonSkill s){ return v[int(s)]; }
};

inline const char* autonSkillName(AutonSkill s){
    static constexpr const char* names[]={"Hold","Attend","Face","Approach","Strike","Leap","Signal"};
    const int i=int(s);
    return (i>=0 && i<kSkillCount)?names[i]:"?";
}

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

inline float ballSpeed3(const CharacterModel& c){
    return std::sqrt(c.ball.vx*c.ball.vx+c.ball.vy*c.ball.vy+c.ball.vz*c.ball.vz);
}

inline bool operatorPresent(const IdlePilot& p){ return p.operatorMemory>0.f; }

inline void decayTimer(float& t,float dt){ t=t>dt?t-dt:0.f; }

inline float ballBearing(const CharacterModel& c){
    return std::remainder(std::atan2(c.ball.x-c.x,c.ball.z-c.z)-c.heading,2.f*kPi);
}

inline float facingErr(const CharacterModel& c){ return ballBearing(c); }

inline bool inLeapTrigger(const IdlePilot& p,const CharacterModel& c){
    if(!c.grounded || characterBusy(c))return false;
    if(inStrikeRange(c))return false;
    if(!ballAir(c))return false;
    if(ballHorizDist(c)>=kLeapDist)return false;
    if(p.leapCooldown>0.f)return false;
    if(c.ball.vy<0.f)return true;
    return p.struckRecent>0.f;
}

inline bool celebrateSignal(const IdlePilot& p,const CharacterModel& c){
    return p.signalCooldown<=0.f && p.struckRecent>0.f
        && ballHorizSpeed(c)>kSignalBallSpeed;
}

inline float approachExp(float cur,float target,float dt,float tauRise,float tauFall){
    const float tau=target>cur?tauRise:tauFall;
    if(tau<=1e-5f)return target;
    return cur+(target-cur)*(1.f-std::exp(-dt/tau));
}

inline bool ballStill(const CharacterModel& c){
    return ballHorizSpeed(c)<.15f && !ballAir(c);
}

inline float playDesire(const IdlePilot& p,const CharacterModel& c){
    float desire=.10f;
    const bool inArena=std::fabs(c.ball.x)<=kArenaHalfExtent && std::fabs(c.ball.z)<=kArenaHalfExtent;
    if(!ballAir(c) && inArena)desire=inStrikeRange(c)?.72f:.85f;
    else if(ballAir(c) && ballHorizDist(c)<kLeapDist)desire=.60f;
    if(p.playInhibit>0.f)desire=std::min(desire,p.playCap);
    return desire;
}

inline float skillJitter(uint32_t tick,int i){
    const uint32_t h=tick*374761393u+uint32_t(i)*668265263u;
    return (float((h>>8)&255u)/255.f-.5f)*.04f;
}

inline const char* autonGaitName(AutonGait g){
    static constexpr const char* names[]={"Stop","WALK","RUN","DASH"};
    const int i=int(g);
    return (i>=0 && i<4)?names[i]:"?";
}

inline float gaitTop(AutonGait g){
    switch(g){
    case AutonGait::Dash: return kDashSpeed;
    case AutonGait::Run: return kRunSpeed;
    case AutonGait::Walk: return kWalkSpeed;
    default: return 0.f;
    }
}

inline AutonGait downGait(AutonGait g){
    if(g==AutonGait::Dash)return AutonGait::Run;
    if(g==AutonGait::Run)return AutonGait::Walk;
    return AutonGait::Stop;
}

inline float stageBrakeTime(AutonGait from,AutonGait to){
    if(from==AutonGait::Dash && to==AutonGait::Run)return kBrakeDashRun;
    if(from==AutonGait::Run && to==AutonGait::Walk)return kBrakeRunWalk;
    if(from==AutonGait::Walk && to==AutonGait::Stop)return kBrakeWalkStop;
    return kBrakeWalkStop;
}

inline float stopDistForSpeed(float speed){
    float d=0.f;
    if(speed>kRunSpeed+.05f){
        d+=.5f*(speed+kRunSpeed)*kBrakeDashRun;
        speed=kRunSpeed;
    }
    if(speed>kWalkSpeed+.05f){
        d+=.5f*(speed+kWalkSpeed)*kBrakeRunWalk;
        speed=kWalkSpeed;
    }
    if(speed>kGaitStop)d+=.5f*speed*kBrakeWalkStop;
    return d+kWallPad;
}

inline float wallAhead(const CharacterModel& c){
    const float hx=std::sin(c.heading),hz=std::cos(c.heading);
    float t=1e9f;
    if(hx>1e-4f)t=std::min(t,(kArenaHalfExtent-c.x)/hx);
    if(hx<-1e-4f)t=std::min(t,(-kArenaHalfExtent-c.x)/hx);
    if(hz>1e-4f)t=std::min(t,(kArenaHalfExtent-c.z)/hz);
    if(hz<-1e-4f)t=std::min(t,(-kArenaHalfExtent-c.z)/hz);
    return std::max(0.f,t);
}

inline bool skillNeedsHalt(AutonSkill s){
    return s!=AutonSkill::Approach;
}

inline bool autonMoving(const IdlePilot& p,const CharacterModel& c){
    return p.gait>AutonGait::Stop || c.gaitSpeed>kGaitStop || locoPlaying(c);
}

inline void clearGait(IdlePilot& p,CharacterModel& c){
    p.gait=AutonGait::Stop;
    p.braking=false;
    p.brakeT=p.brakeDur=p.brakeFrom=p.brakeTo=0;
    p.brakeNext=AutonGait::Stop;
    p.brakeGoal=AutonGait::Stop;
    p.havePending=false;
    p.pendingSkill=AutonSkill::Hold;
    p.pendingScore=0;
    c.gaitSpeed=0;
    c.gaitCoast=false;
    stopLoco(c);
}

inline void applyGaitPose(IdlePilot& p,CharacterModel& c,AutonGait g){
    p.gait=g;
    c.gaitSpeed=gaitTop(g);
    if(g==AutonGait::Dash){
        if(!locoPlaying(c) || c.playGestureId!=kGestureDash)playGesture(c,kGestureDash);
    }else if(g==AutonGait::Run){
        if(!locoPlaying(c) || c.playGestureId!=kGestureRun)playGesture(c,kGestureRun);
    }else{
        stopLoco(c);
    }
}

inline void beginBrakeStage(IdlePilot& p,CharacterModel& c,AutonGait goal){
    p.brakeGoal=goal;
    AutonGait from=p.gait;
    if(from==AutonGait::Stop)from=c.gaitSpeed>kRunSpeed+.05f?AutonGait::Dash
        :(c.gaitSpeed>kWalkSpeed+.05f?AutonGait::Run
        :(c.gaitSpeed>kGaitStop?AutonGait::Walk:AutonGait::Stop));
    if(from==AutonGait::Stop || int(goal)>=int(from)){
        applyGaitPose(p,c,goal);
        p.braking=false;
        return;
    }
    const AutonGait step=downGait(from);
    p.braking=true;
    p.brakeT=0;
    p.brakeDur=stageBrakeTime(from,step);
    p.brakeFrom=std::max(c.gaitSpeed,gaitTop(from));
    p.brakeTo=gaitTop(step);
    p.brakeNext=step;
    if(step==AutonGait::Run){
        if(!locoPlaying(c) || c.playGestureId!=kGestureRun)playGesture(c,kGestureRun);
    }else stopLoco(c);
    p.gait=from;
    c.gaitSpeed=p.brakeFrom;
    arenaKickLog("brake {} -> {} goal={} from={} to={}",autonGaitName(from),autonGaitName(step),
        autonGaitName(goal),p.brakeFrom,p.brakeTo);
}

inline AutonGait pickApproachGait(float dist,float wall,float speed){
    AutonGait g=AutonGait::Walk;
    if(dist>kGaitDashDist)g=AutonGait::Dash;
    else if(dist>kGaitRunDist)g=AutonGait::Run;
    const float closing=std::max(speed,gaitTop(g));
    while(g>AutonGait::Walk && dist<stopDistForSpeed(closing)-kWallPad+.60f)g=downGait(g);
    if(dist<=kGaitNearWalk)g=AutonGait::Walk;
    while(g>AutonGait::Walk && wall<stopDistForSpeed(gaitTop(g)))g=downGait(g);
    return g;
}

inline void faceInterior(CharacterModel& c){
    float nx=0.f,nz=0.f;
    if(c.x>kArenaHalfExtent-2.5f)nx=-1.f;
    if(c.x<-kArenaHalfExtent+2.5f)nx=1.f;
    if(c.z>kArenaHalfExtent-2.5f)nz=-1.f;
    if(c.z<-kArenaHalfExtent+2.5f)nz=1.f;
    if(std::fabs(nx)+std::fabs(nz)<1e-4f){
        nx=-c.x;nz=-c.z;
    }
    faceYaw(c,std::atan2(nx,nz));
}

inline void goPilot(IdlePilot& p,CharacterModel& c){
    p.control=ControlMode::Pilot;
    p.skill=AutonSkill::Hold;
    p.stickIdleAge=0;
    p.attendKickT=0;
    p.haveSeek=false;
    p.skillAge=0;
    p.skillScore=0;
    p.thinkT=0;
    p.autonTick=0;
    p.vigilance=p.curiosity=p.play=p.social=p.composure=0;
    clearGait(p,c);
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

inline void adoptSkill(IdlePilot& p,AutonSkill s,float score){
    if(p.skill!=s){
        arenaKickLog("skill {} -> {} score={}",autonSkillName(p.skill),autonSkillName(s),score);
        p.skill=s;
        p.skillAge=0;
        p.haveSeek=false;
    }
    p.skillScore=score;
}

inline void attendBall(IdlePilot& p,CharacterModel& c,const ArenaView* view=nullptr){
    (void)view;
    adoptSkill(p,AutonSkill::Attend,p.skillScore);
    p.haveSeek=false;
    clearSeek(c);
    if(!p.braking)clearGait(p,c);
    lookBall(c);
}

inline void holdIdle(IdlePilot& p,CharacterModel& c,const ArenaView* view=nullptr){
    adoptSkill(p,AutonSkill::Hold,p.skillScore);
    p.haveSeek=false;
    clearSeek(c);
    if(!p.braking)clearGait(p,c);
    if(ballStill(c) && operatorPresent(p) && view)lookCamera(c,view);
    else if(std::fabs(ballBearing(c))>kHoldLook)lookBall(c);
    else c.lookEnabled=false;
}

inline int waveGestureId(const CharacterModel& c,const ArenaView* view){
    if(!view)return 0;
    const Point e=ArenaCamera(*view).eye();
    const float dx=e.x-c.x,dz=e.z-c.z;
    const float localX=dx*std::cos(c.heading)+dz*(-std::sin(c.heading));
    return localX<0.f?0:1;
}

inline int pickSignalGesture(const IdlePilot& p,const CharacterModel& c,const ArenaView* view){
    if(celebrateSignal(p,c))return 5;
    if(p.playInhibit>0.f && p.playCap<=kMissInhibitCap+.01f && p.play>.25f)return kGesturePunch;
    if(operatorPresent(p) && p.operatorMemory>0.f && p.operatorMemory<kOperatorFade)
        return p.operatorMemory<kOperatorFade*.5f?kGestureBye2:kGestureBye;
    if(p.idleAge>4.f && p.play<.35f)return kGestureBow;
    return waveGestureId(c,view);
}

inline void playSignal(IdlePilot& p,CharacterModel& c,const ArenaView* view,bool celebrate){
    (void)celebrate;
    adoptSkill(p,AutonSkill::Signal,p.skillScore);
    p.haveSeek=false;
    clearSeek(c);
    clearGait(p,c);
    if(c.action!=Action::Gesture || locoPlaying(c))
        playGesture(c,pickSignalGesture(p,c,view));
    lookCamera(c,view);
}

inline void faceBall(IdlePilot& p,CharacterModel& c){
    adoptSkill(p,AutonSkill::Face,p.skillScore);
    if(!p.braking)clearGait(p,c);
    lookBall(c);
    faceYaw(c,kickHeading(c));
}

inline void approachStance(IdlePilot& p,CharacterModel& c){
    adoptSkill(p,AutonSkill::Approach,p.skillScore);
    lookBall(c);
    const float heading=kickHeading(c);
    const KickStance s=kickStanceAt(c,heading);
    const float bdx=c.ball.x-p.seekBallX,bdz=c.ball.z-p.seekBallZ;
    const bool ballMoved=bdx*bdx+bdz*bdz>kStanceRetarget*kStanceRetarget;
    if(!p.haveSeek || ballMoved){
        walkTo(c,s.x,s.z);
        p.seekX=s.x;
        p.seekZ=s.z;
        p.seekHeading=heading;
        p.seekBallX=c.ball.x;
        p.seekBallZ=c.ball.z;
        p.haveSeek=true;
        arenaKickLog("seek x={} z={} h={} from={},{} ball={},{} moved={}",
            s.x,s.z,heading,c.x,c.z,c.ball.x,c.ball.z,ballMoved);
    }else if(!c.hasWalkTo && !inStrikeRange(c)){
        const float err=std::remainder(c.heading-p.seekHeading,2.f*kPi);
        if(std::fabs(err)>kFaceArrive)faceYaw(c,p.seekHeading);
        else walkTo(c,p.seekX,p.seekZ);
    }
    const float dx=c.x-s.x,dz=c.z-s.z;
    const float dist=std::sqrt(dx*dx+dz*dz);
    const float wall=wallAhead(c);
    const float closing=std::max(c.gaitSpeed,gaitTop(p.gait));
    if(wall<stopDistForSpeed(closing)){
        if(dist>kGaitNearWalk){
            faceInterior(c);
            beginBrakeStage(p,c,AutonGait::Stop);
            return;
        }
        if(p.gait>AutonGait::Walk || c.gaitSpeed>kWalkSpeed+.05f){
            beginBrakeStage(p,c,AutonGait::Walk);
            return;
        }
    }
    const AutonGait want=pickApproachGait(dist,wall,c.gaitSpeed);
    if(p.braking)return;
    if(int(want)<int(p.gait) || (p.gait==AutonGait::Stop && c.gaitSpeed>gaitTop(want)+.05f)){
        beginBrakeStage(p,c,want);
        return;
    }
    if(want!=p.gait || (want>AutonGait::Walk && !locoPlaying(c))){
        applyGaitPose(p,c,want);
        arenaKickLog("approach/{} dist={} wall={}",autonGaitName(want),dist,wall);
    }else c.gaitSpeed=gaitTop(want);
}

inline void startStrike(IdlePilot& p,CharacterModel& c){
    adoptSkill(p,AutonSkill::Strike,p.skillScore);
    p.haveSeek=false;
    p.kickConnected=false;
    const KickStance now=kickStance(c);
    const float dx=c.x-now.x,dz=c.z-now.z;
    arenaKickLog("strike x={} z={} h={} kh={} ball={},{},{} dist={} align={} walk={} face={}",
        c.x,c.z,c.heading,kickHeading(c),c.ball.x,c.ball.y,c.ball.z,
        std::sqrt(dx*dx+dz*dz),kickAlignErr(c),c.hasWalkTo,c.hasFaceYaw);
    clearSeek(c);
    clearGait(p,c);
    if(c.action!=Action::Kick)playKick(c);
    lookBall(c);
}

inline void startLeap(IdlePilot& p,CharacterModel& c){
    adoptSkill(p,AutonSkill::Leap,p.skillScore);
    p.haveSeek=false;
    clearSeek(c);
    clearGait(p,c);
    if(!characterBusy(c))playJump(c);
    lookBall(c);
}

inline void updateDrives(IdlePilot& p,const CharacterModel& c,float dt,bool entered){
    const float speed=ballHorizSpeed(c);
    const float dx=c.ball.x-c.x,dz=c.ball.z-c.z;
    const float dist=std::sqrt(dx*dx+dz*dz);
    float incoming=0.f;
    if(dist>1e-4f){
        const float radial=(dx*c.ball.vx+dz*c.ball.vz)/dist;
        incoming=std::max(0.f,-radial);
    }
    float vigT=std::clamp(speed/2.5f+incoming/2.f,0.f,1.f);
    if(ballAir(c) && dist<kLeapDist)vigT=std::max(vigT,.70f);

    const float err=std::fabs(ballBearing(c));
    const bool moving=ballSpeed3(c)>.15f || p.struckRecent>0.f;
    float curT=.40f;
    if(moving)curT=.90f;
    else if(err<.25f)curT=.20f;
    else curT=.55f;
    if(p.idleAge>2.f)curT=std::max(curT,.65f);

    const float desire=playDesire(p,c);
    float socialT=0.f;
    if(p.signalCooldown<=0.f && operatorPresent(p))
        socialT=std::clamp(p.operatorMemory/kOperatorMemory,0.f,1.f);

    if(entered){
        p.vigilance=vigT;
        p.curiosity=curT;
        p.play=0.f;
        p.composure=0.f;
        p.thinkT=0.f;
        p.social=operatorPresent(p)?1.f:0.f;
        return;
    }

    p.vigilance=approachExp(p.vigilance,vigT,dt,kVigilanceRise,kVigilanceFall);
    p.curiosity=approachExp(p.curiosity,curT,dt,kCuriosityTau,kCuriosityTau);
    p.play=approachExp(p.play,desire,dt,kPlayRise,kPlayFall);
    if(socialT>p.social)p.social=socialT;
    else p.social=approachExp(p.social,socialT,dt,.40f,.80f);
    p.composure=approachExp(p.composure,0.f,dt,kComposureFall,kComposureFall);
}

inline SkillScores scoreAutonSkills(const IdlePilot& p,const CharacterModel& c){
    SkillScores s{};
    const float cu=p.curiosity,vi=p.vigilance,pl=p.play,so=p.social,co=p.composure;
    const float align=std::fabs(kickAlignErr(c));
    s[AutonSkill::Attend]=.70f*cu+.50f*vi;
    if(!ballAir(c) && align>kStrikeFaceTol){
        const KickStance want=kickStanceAt(c,kickHeading(c));
        const float wx=c.x-want.x,wz=c.z-want.z;
        if(wx*wx+wz*wz>1.f){
            s[AutonSkill::Face]=s[AutonSkill::Attend]+.25f;
            if(align>kTurnThenWalk)s[AutonSkill::Face]+=.35f;
        }
    }
    if(!ballAir(c) && !inStrikeRange(c))
        s[AutonSkill::Approach]=.80f*pl+.40f*cu;
    if(inStrikeRange(c) && p.thinkT>=kStrikeThink)s[AutonSkill::Strike]=.95f*pl;
    if(inLeapTrigger(p,c))s[AutonSkill::Leap]=.75f*pl+.45f*cu+.20f*vi+.40f;
    if(operatorPresent(p) && p.signalCooldown<=0.f)
        s[AutonSkill::Signal]=std::max(0.f,.85f*so-.50f*pl);
    if(so>.70f && pl<.35f)s[AutonSkill::Signal]+=.40f;
    if(celebrateSignal(p,c)){
        s[AutonSkill::Signal]=std::max(s[AutonSkill::Signal]+kCelebrateBoost,1.05f);
        s[AutonSkill::Attend]*=.70f;
        s[AutonSkill::Approach]*=.50f;
    }
    s[AutonSkill::Hold]=.60f*co+.25f*(1.f-cu)+.20f*(1.f-pl);
    for(int i=0;i<kSkillCount;++i){
        s.v[i]+=.003f*float(i);
        if(s.v[i]>.01f)s.v[i]+=skillJitter(p.autonTick,i);
    }
    const bool chase=s[AutonSkill::Strike]>.40f ||
        (s[AutonSkill::Approach]>.40f && p.playInhibit<=0.f && !ballAir(c));
    if(chase && p.struckRecent<=0.f){
        s[AutonSkill::Signal]*=.40f;
        s[AutonSkill::Leap]*=.30f;
    }
    if(inLeapTrigger(p,c))s[AutonSkill::Signal]*=.35f;
    if(s[AutonSkill::Hold]>.50f){
        s[AutonSkill::Approach]*=.70f;
        s[AutonSkill::Strike]*=.70f;
        s[AutonSkill::Leap]*=.70f;
    }
    return s;
}

inline float skillMinTime(AutonSkill s,const CharacterModel& c){
    switch(s){
    case AutonSkill::Hold: return kHoldMin;
    case AutonSkill::Attend: return kAttendMin;
    case AutonSkill::Face: return std::fabs(kickAlignErr(c))<kFaceArrive?0.f:kFaceMin;
    case AutonSkill::Approach: return kApproachMin;
    default: return 0.f;
    }
}

inline AutonSkill electSkill(const IdlePilot& p,const SkillScores& scores,const CharacterModel& c){
    AutonSkill best=AutonSkill::Hold;
    float bestS=scores[best];
    for(int i=1;i<kSkillCount;++i){
        if(scores.v[i]>bestS){
            bestS=scores.v[i];
            best=AutonSkill(i);
        }
    }
    const float cur=scores[p.skill];
    const float need=p.skillAge<skillMinTime(p.skill,c)?kPreemptEarly:kPreemptLate;
    if(best!=p.skill && bestS+1e-6f>=cur+need)return best;
    return p.skill;
}

inline void applySkill(IdlePilot& p,CharacterModel& c,const ArenaView* view,AutonSkill s);

inline bool stepBrake(IdlePilot& p,CharacterModel& c,float dt,const ArenaView* view){
    if(!p.braking)return false;
    p.brakeT+=dt;
    const float dur=std::max(p.brakeDur,1e-4f);
    const float u=std::min(1.f,p.brakeT/dur);
    const float s=u*u*(3.f-2.f*u);
    c.gaitSpeed=p.brakeFrom+(p.brakeTo-p.brakeFrom)*s;
    if(p.brakeNext==AutonGait::Run && (!locoPlaying(c) || c.playGestureId!=kGestureRun))
        playGesture(c,kGestureRun);
    if(p.brakeT+1e-6f<p.brakeDur)return true;
    applyGaitPose(p,c,p.brakeNext);
    p.braking=false;
    if(p.brakeNext!=p.brakeGoal){
        beginBrakeStage(p,c,p.brakeGoal);
        return true;
    }
    if(p.havePending && p.brakeGoal==AutonGait::Stop){
        const AutonSkill s=p.pendingSkill;
        const float score=p.pendingScore;
        p.havePending=false;
        p.skillScore=score;
        applySkill(p,c,view,s);
    }
    return true;
}

inline void applySkill(IdlePilot& p,CharacterModel& c,const ArenaView* view,AutonSkill s){
    switch(s){
    case AutonSkill::Hold: holdIdle(p,c,view); break;
    case AutonSkill::Attend: attendBall(p,c,view); break;
    case AutonSkill::Face: faceBall(p,c); break;
    case AutonSkill::Approach: approachStance(p,c); break;
    case AutonSkill::Strike: startStrike(p,c); break;
    case AutonSkill::Leap: startLeap(p,c); break;
    case AutonSkill::Signal: playSignal(p,c,view,celebrateSignal(p,c)); break;
    }
}

inline void finishStrike(IdlePilot& p,CharacterModel& c,const ArenaView* view){
    if(p.kickConnected){
        if(p.lastStrikeRecent>0.f){
            p.playInhibit=kDoubleStrikeInhibit;
            p.playCap=kDoubleStrikeCap;
        }else{
            p.playInhibit=kPlayInhibit;
            p.playCap=kPlayInhibitCap;
        }
        p.lastStrikeRecent=kDoubleStrikeWindow;
        p.play=std::min(p.play,p.playCap);
    }else{
        p.playInhibit=kMissInhibit;
        p.playCap=kMissInhibitCap;
        p.play=std::min(p.play,p.playCap);
        p.haveSeek=false;
        p.kickConnected=false;
    }
    p.composure=1.f;
    p.thinkT=0;
    p.attendKickT=kKickAttend;
    p.skill=AutonSkill::Attend;
    p.skillAge=0;
    arenaKickLog("end hit={} minGap={} minGapFwd={} maxFwd={} need={}",
        p.kickConnected,c.kickMinGap,c.kickMinGapFwd,c.kickMaxFwd,kBallR+kFootR);
    attendBall(p,c,view);
}

inline void finishLeap(IdlePilot& p,CharacterModel& c,const ArenaView* view){
    p.leapCooldown=kLeapCooldown;
    p.playInhibit=kPlayInhibit;
    p.playCap=kPlayInhibitCap;
    p.play=std::min(p.play,p.playCap);
    p.composure=1.f;
    p.skill=AutonSkill::Attend;
    p.skillAge=0;
    attendBall(p,c,view);
}

inline void finishSignal(IdlePilot& p,CharacterModel& c,const ArenaView* view){
    p.signalCooldown=kSignalCooldown;
    p.social=0.f;
    p.skill=AutonSkill::Hold;
    p.skillAge=0;
    holdIdle(p,c,view);
}

inline void autonAct(IdlePilot& p,CharacterModel& c,float dt,const ArenaView* view,bool entered=false){
    if(entered)p.operatorMemory=kOperatorMemory;
    if(p.skill==AutonSkill::Hold)p.idleAge+=dt;
    else p.idleAge=entered?p.stickIdleAge:0.f;
    updateDrives(p,c,dt,entered);
    if(characterBusy(c)){
        if(c.action==Action::Kick){
            p.skill=AutonSkill::Strike;
            if(c.ball.struck)p.kickConnected=true;
        }
        else if(c.action==Action::Gesture && !locoPlaying(c))p.skill=AutonSkill::Signal;
        else if(c.action==Action::Jump || c.action==Action::Fall || c.action==Action::Land)
            p.skill=AutonSkill::Leap;
        return;
    }
    if(p.skill==AutonSkill::Strike){
        finishStrike(p,c,view);
        return;
    }
    if(p.skill==AutonSkill::Leap){
        finishLeap(p,c,view);
        return;
    }
    if(p.skill==AutonSkill::Signal){
        finishSignal(p,c,view);
        return;
    }
    if(stepBrake(p,c,dt,view))return;
    p.skillAge+=dt;
    if(p.attendKickT>0.f){
        p.attendKickT=p.attendKickT>dt?p.attendKickT-dt:0.f;
        if(autonMoving(p,c)){
            p.havePending=true;
            p.pendingSkill=AutonSkill::Attend;
            p.pendingScore=p.skillScore;
            beginBrakeStage(p,c,AutonGait::Stop);
            return;
        }
        attendBall(p,c,view);
        return;
    }
    if(p.skill==AutonSkill::Attend || p.skill==AutonSkill::Face || p.skill==AutonSkill::Approach)
        p.thinkT+=dt;
    ++p.autonTick;
    const SkillScores scores=scoreAutonSkills(p,c);
    const AutonSkill best=electSkill(p,scores,c);
    if(best!=p.skill && skillNeedsHalt(best) && autonMoving(p,c)){
        p.havePending=true;
        p.pendingSkill=best;
        p.pendingScore=scores[best];
        if(best==AutonSkill::Strike || best==AutonSkill::Face)c.gaitCoast=false;
        else{
            clearSeek(c);
            const float closing=std::max(c.gaitSpeed,gaitTop(p.gait));
            c.gaitCoast=wallAhead(c)>stopDistForSpeed(closing);
        }
        beginBrakeStage(p,c,AutonGait::Stop);
        return;
    }
    applySkill(p,c,view,best);
}

inline void stepIdlePilot(IdlePilot& p,CharacterModel& c,const ArenaInput& in,float dt,
                          const ArenaView* view=nullptr,bool orbitCue=false){
    decayTimer(p.leapCooldown,dt);
    decayTimer(p.signalCooldown,dt);
    decayTimer(p.struckRecent,dt);
    decayTimer(p.operatorMemory,dt);
    decayTimer(p.lastStrikeRecent,dt);
    decayTimer(p.playInhibit,dt);
    if(c.ball.struck)p.struckRecent=kStruckRecent;
    if(stickLive(in) || (c.grounded && in.clipStep!=0 && !c.danceMode) ||
       in.danceToggle || orbitCue)
        p.operatorMemory=kOperatorMemory;

    const bool pose=c.mode==Mode::Pose || in.toggleMode;
    const bool clipBack=c.grounded && in.clipStep!=0 && !c.danceMode && !in.danceToggle;
    if(pose || stickLive(in) || in.danceToggle || c.danceMode || c.mode!=Mode::Play){
        goPilot(p,c);
        return;
    }
    if(clipBack){
        goPilot(p,c);
        return;
    }
    if(p.control==ControlMode::Auton){
        autonAct(p,c,dt,view,false);
        return;
    }
    clearLookAt(c);
    if(!c.grounded || (c.action==Action::Jump && c.grounded)){
        p.stickIdleAge=0;
        p.idleAge=0;
        return;
    }
    p.stickIdleAge+=dt;
    p.idleAge=p.stickIdleAge;
    if(p.stickIdleAge>=kAutonDelay){
        p.control=ControlMode::Auton;
        autonAct(p,c,dt,view,true);
    }
}
} // namespace gundam_arena
