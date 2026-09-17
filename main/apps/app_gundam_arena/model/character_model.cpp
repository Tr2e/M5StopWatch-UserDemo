#include "character_model.h"
#include "arena_kick_clip.h"
#include <algorithm>
#include <cmath>

namespace gundam_arena {
namespace {
float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}

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

void writeJumpKeys(CharacterModel& c,float thigh,float shin,float arm,float fore,
                   float shoulder,float pelvis,float chest){
    c.pose.anim[int(BoneId::LThigh)].pitch=thigh-.03f;
    c.pose.anim[int(BoneId::RThigh)].pitch=thigh+.02f;
    c.pose.anim[int(BoneId::LShin)].pitch=shin;
    c.pose.anim[int(BoneId::RShin)].pitch=shin+.04f;
    c.pose.anim[int(BoneId::LUpperArm)].pitch=arm;
    c.pose.anim[int(BoneId::RUpperArm)].pitch=arm-.06f;
    c.pose.anim[int(BoneId::LForearm)].pitch=fore;
    c.pose.anim[int(BoneId::RForearm)].pitch=fore-.08f;
    c.pose.anim[int(BoneId::LShoulder)].pitch=shoulder;
    c.pose.anim[int(BoneId::RShoulder)].pitch=shoulder;
    c.pose.anim[int(BoneId::Pelvis)].pitch=pelvis;
    c.pose.anim[int(BoneId::Chest)].pitch=chest;
    plantFeet(c,thigh-.03f,shin,thigh+.02f,shin+.04f);
}

void applyJumpPose(CharacterModel& c){
    c.leftPlanted=c.rightPlanted=false;
    if(c.grounded){
        const float load=smooth01(c.clipT/kJumpDip);
        writeJumpKeys(c,-.52f*load,.98f*load,-.48f*load,-.72f*load,.12f*load,-.18f*load,-.12f*load);
        return;
    }
    const float rise=smooth01(c.clipT/.14f);
    const float s=clampf(.5f-.5f*(c.vy/kJumpVel),0.f,1.f);
    const float a=s<.5f?smooth01(s*2.f):smooth01((s-.5f)*2.f);
    const float thigh0=s<.5f?(-.08f+(-.52f+.08f)*a):(-.52f+(-.14f+.52f)*a);
    const float shin0=s<.5f?(.14f+(.90f-.14f)*a):(.90f+(.28f-.90f)*a);
    const float arm0=s<.5f?(.78f+(.50f-.78f)*a):(.50f+(.18f-.50f)*a);
    const float fore0=s<.5f?(-.25f+(-.55f+.25f)*a):(-.55f+(-.35f+.55f)*a);
    const float pel0=s<.5f?(-.04f+(.06f+.04f)*a):(.06f+(-.08f-.06f)*a);
    const float chest0=s<.5f?(-.02f+(.08f+.02f)*a):(.08f+(-.04f-.08f)*a);
    const float crouch=1.f-rise;
    writeJumpKeys(c,
        crouch*-.52f+(1.f-crouch)*thigh0,
        crouch*.98f+(1.f-crouch)*shin0,
        crouch*-.48f+(1.f-crouch)*arm0,
        crouch*-.72f+(1.f-crouch)*fore0,
        crouch*.12f+(1.f-crouch)*(-.32f+.22f*s),
        crouch*-.18f+(1.f-crouch)*pel0,
        crouch*-.12f+(1.f-crouch)*chest0);
}

void applyLandPose(CharacterModel& c){
    c.leftPlanted=c.rightPlanted=false;
    const float w=smooth01(clampf(c.landT/kJumpLand,0.f,1.f));
    const float thigh=-.36f*w,shin=.72f*w,arm=-.10f*w,fore=-.42f*w;
    c.pose.anim[int(BoneId::LThigh)].pitch=thigh;
    c.pose.anim[int(BoneId::RThigh)].pitch=thigh+.03f;
    c.pose.anim[int(BoneId::LShin)].pitch=shin;
    c.pose.anim[int(BoneId::RShin)].pitch=shin-.04f;
    c.pose.anim[int(BoneId::LUpperArm)].pitch=arm;
    c.pose.anim[int(BoneId::RUpperArm)].pitch=arm-.04f;
    c.pose.anim[int(BoneId::LForearm)].pitch=fore;
    c.pose.anim[int(BoneId::RForearm)].pitch=fore;
    c.pose.anim[int(BoneId::Pelvis)].pitch=-.16f*w;
    c.pose.anim[int(BoneId::Chest)].pitch=-.10f*w;
    plantFeet(c,thigh,shin,thigh+.03f,shin-.04f);
}

void applyRoot(CharacterModel& c){
    c.pose.root={c.x,c.y,c.z};
    c.pose.rootYaw=c.heading;
}

void applyKick(CharacterModel& c,float dt){
    const int last=kKickFrames-1;
    const float t=clampf(c.clipT*kKickFps,0.f,float(last));
    const int i0=int(t);
    const int i1=i0>=last?last:i0+1;
    const float u=t-float(i0);
    for(int b=1;b<kBoneCount;++b){
        const int a0=(i0*18+(b-1))*3;
        const int a1=(i1*18+(b-1))*3;
        JointEuler e{
            kKickJoints[a0]+(kKickJoints[a1]-kKickJoints[a0])*u,
            kKickJoints[a0+1]+(kKickJoints[a1+1]-kKickJoints[a0+1])*u,
            kKickJoints[a0+2]+(kKickJoints[a1+2]-kKickJoints[a0+2])*u};
        c.pose.anim[b]=clampJoint(BoneId(b),e);
    }
    c.clipT+=dt;
    if(c.clipT>=float(last)/kKickFps){
        c.action=Action::Idle;
        c.clipT=0;
        c.leftPlanted=c.rightPlanted=false;
        c.ball.struck=false;
        c.kickToeHad=false;
    }
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
        if(gap<kBallR+kFootR){
            const float fwd=vel.x*std::sin(c.heading)+vel.z*std::cos(c.heading);
            if(fwd>1.5f){
                Point n=subtract(center,hit);
                const float nl=length(n);
                if(nl<1e-4f){
                    n={std::sin(c.heading),.35f,std::cos(c.heading)};
                }else n=scale(n,1.f/nl);
                const float along=std::max(4.8f,dot(vel,n));
                b.vx+=n.x*along;
                b.vy+=std::max(2.6f,n.y*along);
                b.vz+=n.z*along;
                b.struck=true;
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
}

void resetCharacter(CharacterModel& c){
    c={};
    c.ball.x=kBallSpawnX;
    c.ball.y=kBallR;
    c.ball.z=kBallSpawnZ;
}

Point boneWorld(const Skeleton& sk,BoneId bone){return sk.world[int(bone)].t;}

void stepCharacter(CharacterModel& c,const ArenaInput& in,float dt){
    if(in.toggleMode){
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

    const bool coiled=c.action==Action::Jump&&c.grounded;
    const bool busy=c.action==Action::Land||c.action==Action::Kick||coiled;
    if(c.action==Action::Land){
        c.landT-=dt;if(c.landT<=0)c.action=Action::Idle;
    }

    const float forward=in.valid?clampf(in.forward,-1.f,1.f):0.f;
    const float turn=in.valid?clampf(in.turn,-1.f,1.f):0.f;
    if(!busy){
        if(in.kick && c.grounded){
            c.action=Action::Kick;c.clipT=0;c.walkPhase=0;c.ball.struck=false;c.kickToeHad=false;
        }else if(in.jump && c.grounded){
            c.action=Action::Jump;c.clipT=0;c.walkPhase=0;c.vy=0;
        }else if(c.grounded){
            if(std::abs(forward)>.18f)c.action=Action::Walk;
            else if(std::abs(turn)>.18f)c.action=Action::Turn;
            else {c.action=Action::Idle;c.walkPhase=0;}
        }
    }

    if(c.action==Action::Jump&&c.grounded){
        c.clipT+=dt;
        if(c.clipT>=kJumpCrouch){c.grounded=false;c.vy=kJumpVel;c.clipT=0;}
    }

    const float air=c.grounded?1.f:.45f;
    c.forwardSpeed=(c.grounded||in.valid)?forward*kWalkSpeed*air:c.forwardSpeed*.98f;
    c.angularSpeed=turn*kTurnSpeed*(c.grounded?1.f:.6f);
    if(c.action!=Action::Kick && !(c.action==Action::Jump&&c.grounded)){
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
    else if(c.action==Action::Walk)applyWalk(c,forward==0.f?1.f:forward);
    else if(c.action==Action::Turn)applyWalk(c,turn>=0.f?.35f:-.35f);
    else if(c.action==Action::Jump||c.action==Action::Fall){
        applyJumpPose(c);
        if(!c.grounded)c.clipT+=dt;
    }else if(c.action==Action::Land)applyLandPose(c);
    else applyIdleIk(c);
    if(c.action!=Action::Kick)applyTurnFollow(c);
    c.pose.anim[int(BoneId::Head)].yaw=clampf(c.pose.anim[int(BoneId::Head)].yaw,-.87f,.87f);
    stepBall(c,dt);
}
} // namespace gundam_arena
