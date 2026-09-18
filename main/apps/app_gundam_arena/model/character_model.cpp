#include "character_model.h"
#include "arena_kick_clip.h"
#include "arena_gesture_clips.h"
#include "arena_dance_clips.h"
#include "arena_log.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gundam_arena {
namespace {
constexpr float kLookNeckShare=.25f;
constexpr float kLookBlendFloor=.001f;

float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
float wrapPi(float a){return std::remainder(a,2.f*kPi);}

void twoBonePitch(float hipX,float hipY,float targetX,float targetY,float l1,float l2,
                  float& thighPitch,float& shinPitch){
    float dx=targetX-hipX,dy=targetY-hipY;
    float d=std::sqrt(dx*dx+dy*dy);
    d=clampf(d,.08f,l1+l2-.02f);
    const float dir=std::atan2(dx,-dy);
    const float cosA=clampf((l1*l1+d*d-l2*l2)/(2.f*l1*d),-1.f,1.f);
    const float extra=std::acos(cosA);
    const float cosK=clampf((l1*l1+l2*l2-d*d)/(2.f*l1*l2),-1.f,1.f);
    thighPitch=clampf(dir-extra,-.80f,.90f);
    shinPitch=clampf(kPi-std::acos(cosK),0.f,1.80f);
}

int nextPoseBone(BoneId cur,int step){
    static constexpr BoneId cycle[]={
        BoneId::Head,BoneId::Neck,BoneId::Chest,BoneId::Pelvis,
        BoneId::LShoulder,BoneId::LUpperArm,BoneId::LForearm,BoneId::LHand,
        BoneId::RShoulder,BoneId::RUpperArm,BoneId::RForearm,BoneId::RHand,
        BoneId::LThigh,BoneId::LShin,BoneId::LFoot,
        BoneId::RThigh,BoneId::RShin,BoneId::RFoot};
    constexpr int n=int(sizeof(cycle)/sizeof(cycle[0]));
    int i=0;for(;i<n;++i)if(cycle[i]==cur)break;
    if(i==n)i=0;
    i=(i+(step>0?1:n-1))%n;
    return int(cycle[i]);
}

float sagittalFromPlant(const CharacterModel& c,float hipLocalX,float plantX,float plantZ){
    const float cy=std::cos(c.heading),sy=std::sin(c.heading);
    const float hx=c.x+hipLocalX*cy,hz=c.z-hipLocalX*sy;
    return -((plantX-hx)*sy+(plantZ-hz)*cy);
}

void capturePlant(const CharacterModel& c,float hipLocalX,float dx,float& plantX,float& plantZ){
    const float cy=std::cos(c.heading),sy=std::sin(c.heading);
    const float hx=c.x+hipLocalX*cy,hz=c.z-hipLocalX*sy;
    plantX=hx-dx*sy;
    plantZ=hz-dx*cy;
}

void plantFeet(CharacterModel& c,float leftThigh,float leftShin,float rightThigh,float rightShin,
               float liftL=0,float liftR=0){
    const auto pitch=[&](float thigh,float shin,float lift){
        return clampf(-(thigh+shin)-.55f*lift,-.45f,.35f);
    };
    c.pose.anim[int(BoneId::LFoot)].pitch=pitch(leftThigh,leftShin,liftL);
    c.pose.anim[int(BoneId::RFoot)].pitch=pitch(rightThigh,rightShin,liftR);
}

void applyIdleIk(CharacterModel& c){
    c.leftPlanted=c.rightPlanted=false;
    plantFeet(c,0,0,0,0);
}

void applyWalk(CharacterModel& c,float move){
    c.walkPhase+=std::abs(move)*kStep*8.5f;
    if(c.walkPhase>2*kPi)c.walkPhase-=2*kPi;
    const float swing=std::sin(c.walkPhase);
    const float cadence=std::cos(c.walkPhase);
    const float stride=.28f*(move>=0.f?1.f:-1.f);
    const bool leftSwing=cadence<0.f,rightSwing=cadence>0.f;
    const float ankleY=.28f;
    float leftDx=stride*swing,rightDx=-stride*swing;
    float liftL=0.f,liftR=0.f;
    if(leftSwing){
        c.leftPlanted=false;
        liftL=std::max(0.f,-cadence)*.18f;
    }else{
        if(!c.leftPlanted){
            capturePlant(c,-kHipX,leftDx,c.leftPlantX,c.leftPlantZ);
            c.leftPlanted=true;
        }
        leftDx=clampf(sagittalFromPlant(c,-kHipX,c.leftPlantX,c.leftPlantZ),-.40f,.40f);
    }
    if(rightSwing){
        c.rightPlanted=false;
        liftR=std::max(0.f,cadence)*.18f;
    }else{
        if(!c.rightPlanted){
            capturePlant(c,kHipX,rightDx,c.rightPlantX,c.rightPlantZ);
            c.rightPlanted=true;
        }
        rightDx=clampf(sagittalFromPlant(c,kHipX,c.rightPlantX,c.rightPlantZ),-.40f,.40f);
    }
    float lt,ls,rt,rs;
    twoBonePitch(-kHipX,kHipY,-kHipX+leftDx,ankleY+liftL,kThighLen,kShinLen,lt,ls);
    twoBonePitch(kHipX,kHipY,kHipX+rightDx,ankleY+liftR,kThighLen,kShinLen,rt,rs);
    c.pose.anim[int(BoneId::LThigh)].pitch=lt;
    c.pose.anim[int(BoneId::LShin)].pitch=ls;
    c.pose.anim[int(BoneId::RThigh)].pitch=rt;
    c.pose.anim[int(BoneId::RShin)].pitch=rs;
    plantFeet(c,lt,ls,rt,rs,liftL,liftR);
    c.pose.anim[int(BoneId::LUpperArm)].pitch=-.35f*swing;
    c.pose.anim[int(BoneId::RUpperArm)].pitch=.35f*swing;
    c.pose.anim[int(BoneId::LForearm)].pitch=-.25f;
    c.pose.anim[int(BoneId::RForearm)].pitch=-.25f;
}

float smooth01(float t){t=clampf(t,0.f,1.f);return t*t*(3.f-2.f*t);}

struct JumpKey {
    float thigh,shin,arm,fore,shoulder,pelvis,chest;
};

// 腿：地面由钉地 IK + Root 下沉。臂/空中膝按 SD 可读幅度，不抄 CMU 映射角。
constexpr JumpKey kJumpCrouchKey{-.34f,.58f,-.85f,-.75f,.20f,.16f,.12f};
constexpr JumpKey kJumpExtendKey{-.12f,.20f,.80f,-.18f,-.28f,.06f,.04f};
constexpr JumpKey kJumpApexKey{-.55f,1.10f,.35f,-.62f,-.12f,.02f,.02f};
constexpr JumpKey kJumpDropKey{-.32f,.70f,.12f,-.40f,-.06f,.08f,.06f};
constexpr JumpKey kJumpLandKey{-.28f,.60f,-.35f,-.55f,.08f,.12f,.08f};

JumpKey mixJumpKey(const JumpKey& a,const JumpKey& b,float t){
    t=clampf(t,0.f,1.f);
    return {a.thigh+(b.thigh-a.thigh)*t,a.shin+(b.shin-a.shin)*t,
            a.arm+(b.arm-a.arm)*t,a.fore+(b.fore-a.fore)*t,
            a.shoulder+(b.shoulder-a.shoulder)*t,
            a.pelvis+(b.pelvis-a.pelvis)*t,a.chest+(b.chest-a.chest)*t};
}

JumpKey scaleJumpKey(const JumpKey& k,float s){
    return mixJumpKey({},k,s);
}

void writeJumpKeys(CharacterModel& c,const JumpKey& k){
    c.pose.anim[int(BoneId::LThigh)].pitch=k.thigh-.03f;
    c.pose.anim[int(BoneId::RThigh)].pitch=k.thigh+.02f;
    c.pose.anim[int(BoneId::LShin)].pitch=k.shin;
    c.pose.anim[int(BoneId::RShin)].pitch=k.shin+.04f;
    c.pose.anim[int(BoneId::LUpperArm)].pitch=k.arm;
    c.pose.anim[int(BoneId::RUpperArm)].pitch=k.arm-.06f;
    c.pose.anim[int(BoneId::LForearm)].pitch=k.fore;
    c.pose.anim[int(BoneId::RForearm)].pitch=k.fore-.08f;
    c.pose.anim[int(BoneId::LShoulder)].pitch=k.shoulder;
    c.pose.anim[int(BoneId::RShoulder)].pitch=k.shoulder;
    c.pose.anim[int(BoneId::Pelvis)].pitch=k.pelvis;
    c.pose.anim[int(BoneId::Chest)].pitch=k.chest;
    plantFeet(c,k.thigh-.03f,k.shin,k.thigh+.02f,k.shin+.04f);
}

JumpKey squatKey(const JumpKey& body,float dip,float bodyW){
    JumpKey k=scaleJumpKey(body,bodyW);
    twoBonePitch(0.f,kHipY,0.f,kPlantAnkleY+dip,kThighLen,kShinLen,k.thigh,k.shin);
    return k;
}

void applyJumpPose(CharacterModel& c){
    c.leftPlanted=c.rightPlanted=false;
    if(c.grounded){
        const float load=smooth01(c.clipT/kJumpDip);
        const float dip=kJumpSquatY*load;
        writeJumpKeys(c,squatKey(kJumpCrouchKey,dip,load));
        c.pose.root.y=c.y-dip;
        return;
    }
    const float rise=smooth01(c.clipT/.08f);
    const JumpKey coiled=squatKey(kJumpCrouchKey,kJumpSquatY,1.f);
    c.pose.root.y=c.y-kJumpSquatY*(1.f-rise);
    if(rise<1.f){
        writeJumpKeys(c,mixJumpKey(coiled,kJumpExtendKey,rise));
        return;
    }
    const float s=clampf(.5f-.5f*(c.vy/kJumpVel),0.f,1.f);
    const float a=s<.5f?smooth01(s*2.f):smooth01((s-.5f)*2.f);
    const JumpKey ballistic=s<.5f?mixJumpKey(kJumpExtendKey,kJumpApexKey,a)
                                 :mixJumpKey(kJumpApexKey,kJumpDropKey,a);
    writeJumpKeys(c,ballistic);
}

void applyLandPose(CharacterModel& c){
    c.leftPlanted=c.rightPlanted=false;
    const float w=smooth01(clampf(c.landT/kJumpLand,0.f,1.f));
    const float dip=kJumpLandY*w;
    writeJumpKeys(c,squatKey(kJumpLandKey,dip,w));
    c.pose.root.y=c.y-dip;
}

void applyRoot(CharacterModel& c){
    c.pose.root={c.x,c.y,c.z};
    c.pose.rootYaw=c.heading;
}

void sampleClip(CharacterModel& c,const float* joints,int frames,float fps){
    const int last=std::max(0,frames-1);
    const float t=clampf(c.clipT*fps,0.f,float(last));
    const int i0=int(t);
    const int i1=i0>=last?last:i0+1;
    const float u=t-float(i0);
    for(int b=1;b<kBoneCount;++b){
        const int a0=(i0*18+(b-1))*3;
        const int a1=(i1*18+(b-1))*3;
        JointEuler e{
            joints[a0]+(joints[a1]-joints[a0])*u,
            joints[a0+1]+(joints[a1+1]-joints[a0+1])*u,
            joints[a0+2]+(joints[a1+2]-joints[a0+2])*u};
        c.pose.anim[b]=clampJoint(BoneId(b),e);
    }
}

bool advanceClip(CharacterModel& c,float dt,int frames,float fps){
    c.clipT+=dt;
    if(frames<=1)return true;
    return c.clipT>=float(frames-1)/fps;
}

void applyKick(CharacterModel& c,float dt){
    sampleClip(c,kKickJoints,kKickFrames,kKickFps);
    if(advanceClip(c,dt,kKickFrames,kKickFps)){
        c.action=Action::Idle;
        c.clipT=0;
        c.leftPlanted=c.rightPlanted=false;
        c.ball.struck=false;
        c.kickToeHad=false;
    }
}

void applyGesture(CharacterModel& c,float dt){
    const int g=std::clamp(c.playGestureId,0,kGestureCount-1);
    const int frames=kGestureFrames[g];
    const float fps=kGestureFps;
    sampleClip(c,kGestureJoints+kGestureOffset[g],frames,fps);
    const int last=std::max(0,frames-1);
    const float t=clampf(c.clipT*fps,0.f,float(last));
    const int i0=int(t);
    const int i1=i0>=last?last:i0+1;
    const float u=t-float(i0);
    const float dip=kGestureRootDip[g][i0]+(kGestureRootDip[g][i1]-kGestureRootDip[g][i0])*u;
    c.pose.root.y=c.y-dip;
    if(advanceClip(c,dt,frames,fps)){
        if(isLocoGesture(g) && c.gaitSpeed>0.f){
            const float loop=float(std::max(1,frames-1))/fps;
            while(c.clipT>=loop)c.clipT-=loop;
        }else{
            c.action=Action::Idle;
            c.clipT=0;
            c.leftPlanted=c.rightPlanted=false;
        }
    }
}

void sampleDance(CharacterModel& c){
    static_assert(kDanceCount==kDanceClipCount,"dance bank count drifted");
    const int d=std::clamp(c.danceId,0,kDanceCount-1);
    const int frames=kDanceFrames[d];
    const int last=std::max(0,frames-1);
    const float t=clampf(c.clipT*kDanceFps,0.f,float(last));
    const int i0=int(t);
    const int i1=i0>=last?last:i0+1;
    const float u=t-float(i0);
    const float s=1.f/float(kDanceScale);
    const int16_t* joints=kDanceJoints+kDanceOffset[d];
    for(int b=1;b<kBoneCount;++b){
        const int a0=(i0*18+(b-1))*3;
        const int a1=(i1*18+(b-1))*3;
        JointEuler e{
            (float(joints[a0])+(float(joints[a1])-float(joints[a0]))*u)*s,
            (float(joints[a0+1])+(float(joints[a1+1])-float(joints[a0+1]))*u)*s,
            (float(joints[a0+2])+(float(joints[a1+2])-float(joints[a0+2]))*u)*s};
        c.pose.anim[b]=clampJoint(BoneId(b),e);
    }
    const int16_t* dips=kDanceRootDip+kDanceDipOffset[d];
    const float dip=(float(dips[i0])+(float(dips[i1])-float(dips[i0]))*u)*s;
    c.pose.root.y=c.y-dip;
}

void applyDance(CharacterModel& c,float dt){
    const int d=std::clamp(c.danceId,0,kDanceCount-1);
    const int frames=kDanceFrames[d];
    sampleDance(c);
    const int start=std::clamp(kDanceLoopStart[d],0,std::max(0,frames-1));
    const int stop=std::clamp(kDanceLoopEnd[d],start+1,frames);
    const float t0=float(start)/kDanceFps;
    const float t1=float(stop)/kDanceFps;
    c.clipT+=dt;
    if(c.clipT>=t1){
        const float span=t1-t0;
        while(c.clipT>=t1)c.clipT-=span;
        if(c.clipT<t0)c.clipT=t0;
    }
}

int nextDanceClip(int id,int step){
    const int n=kDanceClipCount;
    id=std::clamp(id,0,n-1);
    return (id+(step>0?1:n-1))%n;
}

int nextPlayClip(int index,int step){
    if(index==kPlayClipNone)return step>0?0:kPlayClipCount-1;
    const int n=kPlayClipCount;
    return (index+(step>0?1:n-1))%n;
}

void beginRingClip(CharacterModel& c,int slot){
    c.clipIndex=slot;
    if(slot==0)playKick(c);
    else if(slot==1)playJump(c);
    else playGesture(c,slot-2);
}

Point closestOnSeg(Point a,Point b,Point p){
    const Point ab=subtract(b,a);
    const float denom=dot(ab,ab);
    const float t=denom<1e-8f?0.f:clampf(dot(subtract(p,a),ab)/denom,0.f,1.f);
    return add(a,scale(ab,t));
}

Skeleton& collisionSkeleton(){
    static Skeleton sk;
    static bool ready=false;
    if(!ready){makeBindSkeleton(sk);ready=true;}
    return sk;
}

void stepBall(CharacterModel& c,float dt){
    auto& b=c.ball;
    if(c.action==Action::Kick && !b.struck){
        auto& sk=collisionSkeleton();
        evaluateSkeleton(sk,c.pose);
        const Affine& foot=sk.world[int(BoneId::LFoot)];
        const Point ankle=foot.t;
        const Point toe=foot.apply({0.f,kFootToeY,kFootToeZ});
        const Point center{b.x,b.y,b.z};
        Point hit=closestOnSeg(ankle,toe,center);
        float gap=length(subtract(center,hit));
        Point vel{};
        if(c.kickToeHad){
            vel=scale(subtract(toe,c.kickToe),dt>1e-6f?1.f/dt:0.f);
            const Point swept=closestOnSeg(c.kickToe,toe,center);
            const float sweptGap=length(subtract(center,swept));
            if(sweptGap<gap){gap=sweptGap;hit=swept;}
        }
        c.kickToe=toe;
        c.kickToeHad=true;
        float fwd=0.f;
        if(c.kickToeHad && dt>1e-6f)fwd=vel.x*std::sin(c.heading)+vel.z*std::cos(c.heading);
        if(gap<c.kickMinGap)c.kickMinGap=gap;
        if(fwd>c.kickMaxFwd)c.kickMaxFwd=fwd;
        if(fwd>1.5f && gap<c.kickMinGapFwd)c.kickMinGapFwd=gap;
        if(gap<kBallR+kFootR){
            if(fwd>1.5f){
                Point n=subtract(center,hit);
                const float nl=length(n);
                if(nl<1e-4f){
                    n={std::sin(c.heading),.35f,std::cos(c.heading)};
                }else n=scale(n,1.f/nl);
                const float along=std::max(4.8f,dot(vel,n));
                b.vx+=n.x*along;
                b.vy+=std::max(kKickMinLift,n.y*along);
                b.vz+=n.z*along;
                b.struck=true;
                arenaKickLog("hit gap={} fwd={} along={} ball={},{},{} toe={},{},{}",
                    gap,fwd,along,b.x,b.y,b.z,toe.x,toe.y,toe.z);
            }
        }
    }else c.kickToeHad=false;
    b.vy-=kGravity*dt;
    b.x=clampf(b.x+b.vx*dt,-kArenaHalfExtent,kArenaHalfExtent);
    b.y+=b.vy*dt;
    b.z=clampf(b.z+b.vz*dt,-kArenaHalfExtent,kArenaHalfExtent);
    if(b.y<kBallR){
        b.y=kBallR;
        if(b.vy<0.f)b.vy*=-.38f;
        b.vx*=.84f;b.vz*=.84f;
        if(std::abs(b.vy)<.35f)b.vy=0;
        if(b.vx*b.vx+b.vz*b.vz<.04f){b.vx=0;b.vz=0;}
    }
}

void applyTurnFollow(CharacterModel& c){
    const float yaw=clampf(c.angularSpeed*.12f,-.18f,.18f);
    c.pose.anim[int(BoneId::Pelvis)].yaw=yaw*.45f;
    c.pose.anim[int(BoneId::Chest)].yaw=yaw;
    c.pose.anim[int(BoneId::Head)].yaw=clampf(yaw*1.15f,-.87f,.87f);
}

void applyLookAt(CharacterModel& c,float dt){
    if(c.action!=Action::Idle && c.action!=Action::Walk && c.action!=Action::Turn){
        c.lookYaw=c.pose.anim[int(BoneId::Head)].yaw;
        c.lookPitch=c.pose.anim[int(BoneId::Head)].pitch;
        c.lookNeck=c.pose.anim[int(BoneId::Neck)].yaw;
        return;
    }
    float yawT=0.f,pitchT=0.f;
    if(c.lookEnabled){
        auto& sk=collisionSkeleton();
        evaluateSkeleton(sk,c.pose);
        const Point head=sk.world[int(BoneId::Head)].t;
        const float dx=c.lookX-head.x,dy=c.lookY-head.y,dz=c.lookZ-head.z;
        const float cy=std::cos(c.heading),sy=std::sin(c.heading);
        const float localX=dx*cy+dz*(-sy);
        const float localZ=dx*sy+dz*cy;
        const float horiz=std::sqrt(localX*localX+localZ*localZ);
        if(horiz>=.02f || std::fabs(dy)>=.02f){
            const JointLimit headL=limitOf(BoneId::Head);
            yawT=clampf(std::atan2(localX,localZ),headL.yawMin,headL.yawMax);
            pitchT=clampf(std::atan2(-dy,std::max(horiz,.001f)),headL.pitchMin,headL.pitchMax);
        }
    }
    const float a=1.f-std::exp(-dt/kLookTau);
    c.lookYaw+=(yawT-c.lookYaw)*a;
    c.lookPitch+=(pitchT-c.lookPitch)*a;
    c.lookNeck+=(yawT*kLookNeckShare-c.lookNeck)*a;
    if(!c.lookEnabled && std::fabs(c.lookYaw)<kLookBlendFloor && std::fabs(c.lookPitch)<kLookBlendFloor){
        c.lookYaw=c.lookPitch=c.lookNeck=0;
        return;
    }
    const JointLimit neckL=limitOf(BoneId::Neck);
    c.pose.anim[int(BoneId::Head)].yaw=c.lookYaw;
    c.pose.anim[int(BoneId::Head)].pitch=c.lookPitch;
    c.pose.anim[int(BoneId::Neck)].yaw=clampf(c.lookNeck,neckL.yawMin,neckL.yawMax);
}

void seekAxes(CharacterModel& c,float& forward,float& turn){
    float targetYaw=c.heading;
    if(c.hasWalkTo){
        const float dx=c.walkToX-c.x,dz=c.walkToZ-c.z;
        const float dist=std::sqrt(dx*dx+dz*dz);
        if(dist<kWalkArrive){
            clearSeek(c);
            forward=0;
            turn=0;
            return;
        }
        targetYaw=std::atan2(dx,dz);
    }else if(c.hasFaceYaw)targetYaw=c.faceYaw;
    else return;
    const float err=wrapPi(targetYaw-c.heading);
    if(c.hasFaceYaw && !c.hasWalkTo && std::fabs(err)<kFaceArrive){
        clearSeek(c);
        forward=0;
        turn=0;
        return;
    }
    turn=clampf(err/.60f,-1.f,1.f);
    forward=(c.hasWalkTo && std::fabs(err)<kTurnThenWalk)?1.f:0.f;
}
}

void resetCharacter(CharacterModel& c){
    c={};
    c.ball.x=kBallSpawnX;
    c.ball.y=kBallR;
    c.ball.z=kBallSpawnZ;
}

Point boneWorld(const Skeleton& sk,BoneId bone){return sk.world[int(bone)].t;}

void playKick(CharacterModel& c){
    stopDance(c);
    c.clipT=0;
    c.walkPhase=0;
    c.leftPlanted=c.rightPlanted=false;
    c.action=Action::Kick;
    c.ball.struck=false;
    c.kickToeHad=false;
    c.kickMinGap=9.f;
    c.kickMaxFwd=-9.f;
    c.kickMinGapFwd=9.f;
}

void playJump(CharacterModel& c){
    stopDance(c);
    c.clipT=0;
    c.walkPhase=0;
    c.leftPlanted=c.rightPlanted=false;
    c.action=Action::Jump;
    c.vy=0;
    c.grounded=true;
    c.y=0;
}

void playGesture(CharacterModel& c,int id){
    stopDance(c);
    c.playGestureId=std::clamp(id,0,kGestureCount-1);
    c.clipT=0;
    c.walkPhase=0;
    c.leftPlanted=c.rightPlanted=false;
    c.action=Action::Gesture;
}

void playDance(CharacterModel& c,int id){
    stopLoco(c);
    c.danceId=std::clamp(id,0,kDanceClipCount-1);
    c.danceMode=true;
    const int d=c.danceId;
    const int start=std::clamp(kDanceLoopStart[d],0,std::max(0,kDanceFrames[d]-1));
    c.clipT=float(start)/kDanceFps;
    c.walkPhase=0;
    c.gaitSpeed=0;
    c.gaitCoast=false;
    c.leftPlanted=c.rightPlanted=false;
    c.action=Action::Dance;
    clearSeek(c);
}

void stopDance(CharacterModel& c){
    if(!c.danceMode && c.action!=Action::Dance)return;
    c.danceMode=false;
    if(c.action==Action::Dance){
        c.action=Action::Idle;
        c.clipT=0;
        c.leftPlanted=c.rightPlanted=false;
    }
}

bool isLocoGesture(int id){
    return id==kGestureRun || id==kGestureDash;
}

bool locoPlaying(const CharacterModel& c){
    return c.action==Action::Gesture && isLocoGesture(c.playGestureId);
}

void stopLoco(CharacterModel& c){
    if(!locoPlaying(c))return;
    c.action=Action::Idle;
    c.clipT=0;
    c.leftPlanted=c.rightPlanted=false;
}

bool characterBusy(const CharacterModel& c){
    if(!c.grounded)return true;
    if(c.danceMode || c.action==Action::Dance)return true;
    if(c.action==Action::Kick || c.action==Action::Land)return true;
    if(c.action==Action::Gesture && !isLocoGesture(c.playGestureId))return true;
    if(c.action==Action::Jump)return true;
    return false;
}

void setLookAt(CharacterModel& c,Point world){
    c.lookEnabled=true;
    c.lookX=world.x;
    c.lookY=world.y;
    c.lookZ=world.z;
}

void clearLookAt(CharacterModel& c){
    c.lookEnabled=false;
    c.lookYaw=c.lookPitch=c.lookNeck=0;
}

void faceYaw(CharacterModel& c,float yaw){
    c.hasFaceYaw=true;
    c.faceYaw=yaw;
    c.hasWalkTo=false;
}

void walkTo(CharacterModel& c,float x,float z){
    c.hasWalkTo=true;
    c.walkToX=std::clamp(x,-kArenaHalfExtent,kArenaHalfExtent);
    c.walkToZ=std::clamp(z,-kArenaHalfExtent,kArenaHalfExtent);
    c.hasFaceYaw=false;
}

void clearSeek(CharacterModel& c){
    c.hasWalkTo=false;
    c.hasFaceYaw=false;
}

KickStance kickStanceAt(const CharacterModel& c,float heading){
    const float ox=kBallSpawnX,oz=kBallSpawnZ;
    const float exX=std::cos(heading),exZ=-std::sin(heading);
    const float ezX=std::sin(heading),ezZ=std::cos(heading);
    KickStance s{};
    s.x=std::clamp(c.ball.x-ox*exX-oz*ezX,-kArenaHalfExtent,kArenaHalfExtent);
    s.z=std::clamp(c.ball.z-ox*exZ-oz*ezZ,-kArenaHalfExtent,kArenaHalfExtent);
    return s;
}

KickStance kickStance(const CharacterModel& c){ return kickStanceAt(c,c.heading); }

float kickHeading(const CharacterModel& c){
    const float toBall=std::atan2(c.ball.x-c.x,c.ball.z-c.z);
    const float local=std::atan2(kBallSpawnX,kBallSpawnZ);
    return std::remainder(toBall-local,2.f*kPi);
}

float kickAlignErr(const CharacterModel& c){
    return std::remainder(c.heading-kickHeading(c),2.f*kPi);
}

bool inStrikeRange(const CharacterModel& c){
    if(characterBusy(c))return false;
    if(c.hasWalkTo || c.hasFaceYaw)return false;
    if(c.ball.y>kBallR+.04f)return false;
    const KickStance s=kickStance(c);
    const float dx=c.x-s.x,dz=c.z-s.z;
    if(dx*dx+dz*dz>=kStrikePosTol*kStrikePosTol)return false;
    return std::fabs(kickAlignErr(c))<kStrikeFaceTol;
}

void stepCharacter(CharacterModel& c,const ArenaInput& in,float dt){
    if(in.toggleMode){
        if(c.danceMode)stopDance(c);
        c.mode=c.mode==Mode::Play?Mode::Pose:Mode::Play;
        if(c.mode==Mode::Pose){c.action=Action::Idle;c.clipT=0;}
    }
    if(c.mode==Mode::Pose){
        if(in.jointStep)c.selected=BoneId(nextPoseBone(c.selected,in.jointStep));
        auto& e=c.pose.anim[int(c.selected)];
        if(in.poseReset)e={};
        else {
            e.yaw+=in.poseYaw;e.pitch+=in.posePitch;
            e=clampJoint(c.selected,e);
        }
        applyRoot(c);
        stepBall(c,dt);
        return;
    }

    const bool clipHold=characterBusy(c);
    const bool loco=locoPlaying(c);
    if(c.action==Action::Land){
        c.landT-=dt;if(c.landT<=0)c.action=Action::Idle;
    }

    const float stickF=in.valid?clampf(in.forward,-1.f,1.f):0.f;
    const float stickT=in.valid?clampf(in.turn,-1.f,1.f):0.f;
    const bool playerStick=std::fabs(stickF)>kStickDeadzone || std::fabs(stickT)>kStickDeadzone;
    if(playerStick){
        clearSeek(c);
        c.gaitSpeed=0;
        c.gaitCoast=false;
        if(c.danceMode)stopDance(c);
    }
    if(c.grounded && in.danceToggle && !playerStick){
        if(c.danceMode)stopDance(c);
        else playDance(c,c.danceId);
    }
    float forward=stickF,turn=stickT;
    if(!playerStick && !characterBusy(c) && (c.hasWalkTo || c.hasFaceYaw))
        seekAxes(c,forward,turn);

    if(c.danceMode){
        if(c.grounded)c.action=Action::Dance;
        if(c.grounded && in.clipStep)playDance(c,nextDanceClip(c.danceId,in.clipStep));
    }else if(c.grounded && in.clipStep){
        beginRingClip(c,nextPlayClip(c.clipIndex,in.clipStep));
        c.gaitSpeed=0;
        c.gaitCoast=false;
    }else if(c.grounded && !c.danceMode && !clipHold && (playerStick || !loco)){
        const bool translating=std::abs(forward)>.18f
            || (!playerStick && c.gaitCoast && c.gaitSpeed>0.25f);
        if(translating)c.action=Action::Walk;
        else if(std::abs(turn)>.18f)c.action=Action::Turn;
        else if(!loco){c.action=Action::Idle;c.walkPhase=0;}
    }

    if(c.action==Action::Jump&&c.grounded){
        c.clipT+=dt;
        if(c.clipT>=kJumpCrouch){c.grounded=false;c.vy=kJumpVel;c.clipT=0;}
    }

    const float air=c.grounded?1.f:.45f;
    float want=forward*kWalkSpeed*air;
    if(c.gaitSpeed>0.f){
        if(c.hasWalkTo)want=forward*c.gaitSpeed*air;
        else if(c.gaitCoast || std::fabs(forward)>kStickDeadzone)want=c.gaitSpeed*air;
        else want=0.f;
    }
    if(c.hasWalkTo){
        const float dx=c.walkToX-c.x,dz=c.walkToZ-c.z;
        const float dist=std::sqrt(dx*dx+dz*dz);
        const float cap=dist/std::max(dt,1e-4f);
        if(want>cap)want=cap;
    }
    if(want>0.f){
        const float hx=std::sin(c.heading),hz=std::cos(c.heading);
        float wall=1e9f;
        if(hx>1e-4f)wall=std::min(wall,(kArenaHalfExtent-c.x)/hx);
        if(hx<-1e-4f)wall=std::min(wall,(-kArenaHalfExtent-c.x)/hx);
        if(hz>1e-4f)wall=std::min(wall,(kArenaHalfExtent-c.z)/hz);
        if(hz<-1e-4f)wall=std::min(wall,(-kArenaHalfExtent-c.z)/hz);
        wall=std::max(0.f,wall);
        const float cap=wall/std::max(dt,1e-4f);
        if(want>cap)want=cap;
    }
    c.forwardSpeed=(c.grounded||in.valid)?want:c.forwardSpeed*.98f;
    c.angularSpeed=turn*kTurnSpeed*(c.grounded?1.f:.6f);
    const bool lockRoot=c.action==Action::Kick
        || c.action==Action::Dance
        || (c.action==Action::Gesture && !locoPlaying(c))
        || (c.action==Action::Jump&&c.grounded);
    if(!lockRoot){
        c.heading=std::remainder(c.heading+c.angularSpeed*dt,2.f*kPi);
        c.x=clampf(c.x+std::sin(c.heading)*c.forwardSpeed*dt,-kArenaHalfExtent,kArenaHalfExtent);
        c.z=clampf(c.z+std::cos(c.heading)*c.forwardSpeed*dt,-kArenaHalfExtent,kArenaHalfExtent);
    }
    if(!c.grounded){
        c.vy-=kGravity*dt;
        c.y+=c.vy*dt;
        if(c.y<=0){c.y=0;c.vy=0;c.grounded=true;c.action=Action::Land;c.landT=kJumpLand;}
        else c.action=c.vy>0?Action::Jump:Action::Fall;
    }

    c.pose.anim={};
    applyRoot(c);
    if(c.action==Action::Kick)applyKick(c,dt);
    else if(c.action==Action::Gesture)applyGesture(c,dt);
    else if(c.action==Action::Dance)applyDance(c,dt);
    else if(c.action==Action::Walk)applyWalk(c,forward==0.f?1.f:forward);
    else if(c.action==Action::Turn)applyWalk(c,turn>=0.f?.35f:-.35f);
    else if(c.action==Action::Jump||c.action==Action::Fall){
        applyJumpPose(c);
        if(!c.grounded)c.clipT+=dt;
    }else if(c.action==Action::Land)applyLandPose(c);
    else applyIdleIk(c);
    if(c.action!=Action::Kick && c.action!=Action::Gesture && c.action!=Action::Dance)applyTurnFollow(c);
    applyLookAt(c,dt);
    c.pose.anim[int(BoneId::Head)].yaw=clampf(c.pose.anim[int(BoneId::Head)].yaw,-.87f,.87f);
    stepBall(c,dt);
}
} // namespace gundam_arena
