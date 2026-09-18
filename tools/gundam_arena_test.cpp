#include "../main/apps/app_gundam_arena/view/arena_renderer.h"
#include "../main/apps/app_gundam_arena/controller/arena_controller.h"
#include "../main/apps/app_gundam_arena/model/arena_kick_clip.h"
#include "../main/apps/app_gundam_arena/model/arena_gesture_clips.h"
#include "../tools/arena_motion/arena_walk_clip.h"
#include "../main/apps/app_lets_and_go_racer/input/device_control_logic.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

using namespace gundam_arena;

static bool finitePoint(Point p){
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}

static uint16_t pixelAt(const LGFX_Sprite& canvas,int x,int y){
    return canvas.frame()[std::size_t(y)*canvas.width()+x];
}

static int countChanged(const LGFX_Sprite& a,const LGFX_Sprite& b){
    int n=0;
    for(int y=0;y<a.height();++y)for(int x=0;x<a.width();++x)
        if(pixelAt(a,x,y)!=pixelAt(b,x,y))++n;
    return n;
}

static void playSlot(CharacterModel& c,int slot){
    c.clipIndex=slot<=0?kPlayClipNone:slot-1;
    ArenaInput in{};in.valid=true;in.clipStep=1;
    stepCharacter(c,in,kStep);
}

static int yawReversals(const float* y,int n){
    int c=0;
    for(int i=2;i<n;++i){
        const float a=y[i-1]-y[i-2],b=y[i]-y[i-1];
        if(a*b<0.f && std::fabs(a)+std::fabs(b)>0.012f)++c;
    }
    return c;
}

struct GestureSamp {
    int n=0;
    float hy[180]{}, ly[180]{}, ry[180]{}, lx[180]{}, rx[180]{}, lz[180]{}, rz[180]{};
    float lDist[180]{}, rDist[180]{};
    float lY[180]{}, rY[180]{}, lP[180]{}, rP[180]{};
    float chestP[180]{}, rootY[180]{}, cy[180]{};
};

static GestureSamp sampleGesture(int slot){
    CharacterModel c;resetCharacter(c);
    ArenaInput in{};in.valid=true;
    Skeleton sk{};
    makeBindSkeleton(sk);
    playSlot(c,slot);
    GestureSamp s{};
    for(int i=0;i<260 && c.action==Action::Gesture;++i){
        evaluateSkeleton(sk,c.pose);
        const Point h=sk.world[int(BoneId::Head)].t;
        const Point lh=sk.world[int(BoneId::LHand)].t;
        const Point rh=sk.world[int(BoneId::RHand)].t;
        s.hy[s.n]=h.y;
        s.ly[s.n]=lh.y;
        s.ry[s.n]=rh.y;
        s.lx[s.n]=lh.x;
        s.rx[s.n]=rh.x;
        s.lz[s.n]=lh.z;
        s.rz[s.n]=rh.z;
        s.lDist[s.n]=length(subtract(lh,h));
        s.rDist[s.n]=length(subtract(rh,h));
        s.lY[s.n]=c.pose.anim[int(BoneId::LUpperArm)].yaw;
        s.rY[s.n]=c.pose.anim[int(BoneId::RUpperArm)].yaw;
        s.lP[s.n]=c.pose.anim[int(BoneId::LUpperArm)].pitch;
        s.rP[s.n]=c.pose.anim[int(BoneId::RUpperArm)].pitch;
        s.chestP[s.n]=c.pose.anim[int(BoneId::Chest)].pitch;
        s.rootY[s.n]=c.pose.root.y;
        s.cy[s.n]=c.y;
        ++s.n;
        stepCharacter(c,in,kStep);
    }
    return s;
}

static void holdWindow(const GestureSamp& s,int& a0,int& a1){
    a0=std::min(20,std::max(0,s.n/5));
    a1=std::max(a0+8,s.n-24);
}

static float robotScreenHeight(const LGFX_Sprite& canvas){
    const int cx=canvas.width()/2,cy=canvas.height()/2,r=std::min(canvas.width(),canvas.height())/2;
    int top=canvas.height(),bottom=-1;
    for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x){
        const int dx=x-cx,dy=y-cy;
        if(dx*dx+dy*dy>=r*r)continue;
        const auto p=pixelAt(canvas,x,y);
        if(p==space::background||p==space::grid||p==space::seam||p==space::navigation||
           p==space::ball||p==space::ballRim)continue;
        top=std::min(top,y);bottom=std::max(bottom,y);
    }
    return bottom>=top?float(bottom-top+1)/float(2*r):0.f;
}

int main(int argc,char** argv){
    const std::string out=argc>1?argv[1]:"/tmp/gundam-arena";
    static_assert(kWalkFrames==60,"walk comparison clip window drifted");
    static_assert(kKickFrames==39,"kick clip window drifted");
    static_assert(kGestureCount==13 && kGestureFrames[0]==50,"gesture bank drifted");
    static_assert(kGestureFrames[11]>kGestureFrames[12] && kGestureFrames[12]>50,"dance windows drifted");
    static_assert(kGestureFlashBytes[0]==10800,"gesture flash size drifted");
    static_assert(sizeof(kGestureRootDip)/sizeof(kGestureRootDip[0])==13,"gesture root dip drifted");
    const auto kickPitch=[](int frame,BoneId bone){
        return kKickJoints[frame*18*3+(int(bone)-1)*3+1];
    };
    const float strikeRua=kickPitch(25,BoneId::RUpperArm);
    const float strikeLua=kickPitch(25,BoneId::LUpperArm);
    assert(strikeLua>-.02f && strikeLua<.12f);
    assert(strikeRua<-.50f);
    for(int f=26;f<=30;++f){
        assert(std::abs(kickPitch(f,BoneId::RUpperArm)-strikeRua)<1e-4f);
        assert(std::abs(kickPitch(f,BoneId::LUpperArm)-strikeLua)<1e-4f);
        assert(kickPitch(f,BoneId::LForearm)>-.55f);
        assert(kickPitch(f,BoneId::RForearm)>-.55f);
    }
    float prevThigh=kickPitch(0,BoneId::LThigh);
    float prevShin=kickPitch(0,BoneId::LShin);
    bool afterChamber=false;
    for(int f=1;f<kKickFrames;++f){
        const float lt=kickPitch(f,BoneId::LThigh);
        const float ls=kickPitch(f,BoneId::LShin);
        if(prevThigh>.50f)afterChamber=true;
        if(afterChamber && lt<prevThigh-0.01f){
            assert(ls<=prevShin+0.04f);
            assert(lt<=prevThigh+0.02f);
        }
        prevThigh=lt;
        prevShin=ls;
    }
    Mesh mesh;Skeleton bind;
    buildRx78Rigged(mesh,bind);
    assert(!mesh.overflowed && mesh.count>400 && mesh.count<Mesh::capacity);
    std::array<unsigned,kBoneCount> bones{};
    for(size_t i=0;i<mesh.count;++i){
        const int bone=int(mesh.panels[i].wheel?mesh.panels[i].wheel-1:0);
        assert(bone>=0 && bone<kBoneCount);
        ++bones[bone];
        for(auto p:mesh.panels[i].point)assert(finitePoint(p));
        assert(finitePoint(mesh.normals[i]));
        assert(mesh.parts[i]!=Part::Rifle && mesh.parts[i]!=Part::Shield && mesh.parts[i]!=Part::Sabers);
    }
    assert(bones[int(BoneId::Head)]>0);
    assert(bones[int(BoneId::LForearm)]>0 && bones[int(BoneId::RForearm)]>0);
    assert(bones[int(BoneId::LShin)]>0 && bones[int(BoneId::RShin)]>0);
    assert(bones[int(BoneId::LHand)]>0 && bones[int(BoneId::RHand)]>0);

    SkeletonPose pose{};
    Skeleton sk=bind;
    evaluateSkeleton(sk,pose);
    Point head0=sk.world[int(BoneId::Head)].t;
    Point hand0=sk.world[int(BoneId::RHand)].t;
    pose.anim[int(BoneId::Head)].yaw=.40f;
    evaluateSkeleton(sk,pose);
    Point head1=sk.world[int(BoneId::Head)].t;
    assert(std::abs(head1.x-head0.x)<.20f && std::abs(head1.y-head0.y)<.20f);
    pose={};
    pose.anim[int(BoneId::RForearm)].pitch=-1.0f;
    evaluateSkeleton(sk,pose);
    Point hand1=sk.world[int(BoneId::RHand)].t;
    assert(length(subtract(hand1,hand0))>.05f);
    Point chest1=sk.world[int(BoneId::Chest)].t;
    pose={};
    evaluateSkeleton(sk,pose);
    Point chest2=sk.world[int(BoneId::Chest)].t;
    assert(std::abs(chest1.x-chest2.x)<1e-5f);

    CharacterModel c;resetCharacter(c);
    assert(c.ball.z>.5f && std::abs(c.ball.y-kBallR)<1e-4f);
    ArenaInput in{};in.valid=true;
    for(int i=0;i<12;++i)stepCharacter(c,in,kStep);
    assert(c.action==Action::Idle);
    assert(std::abs(c.pose.anim[int(BoneId::LThigh)].pitch)<.05f);
    assert(std::abs(c.pose.anim[int(BoneId::RThigh)].pitch)<.05f);
    assert(std::abs(c.pose.anim[int(BoneId::LShin)].pitch)<.05f);
    assert(std::abs(c.pose.anim[int(BoneId::RShin)].pitch)<.05f);
    assert(std::abs(c.pose.anim[int(BoneId::LFoot)].pitch)<.05f);
    assert(std::abs(c.pose.anim[int(BoneId::RFoot)].pitch)<.05f);
    evaluateSkeleton(sk,c.pose);
    assert(sk.world[int(BoneId::LFoot)].t.y<.35f && sk.world[int(BoneId::RFoot)].t.y<.35f);

    in.forward=1;
    for(int i=0;i<120;++i)stepCharacter(c,in,kStep);
    assert(c.z>0.2f && c.action==Action::Walk);
    assert(std::abs(c.x)<=kArenaHalfExtent && std::abs(c.z)<=kArenaHalfExtent);
    const float forwardZ=c.z;
    in.forward=-1;
    for(int i=0;i<120;++i)stepCharacter(c,in,kStep);
    assert(c.z<forwardZ);
    in.forward=0;in.turn=1;
    const float heading0=c.heading;
    ArenaView locked{};
    const float cam0=locked.camYaw;
    for(int i=0;i<90;++i){
        stepCharacter(c,in,kStep);
        updateFollowView(locked,c,kStep);
    }
    assert(std::abs(std::remainder(c.heading-heading0,2.f*kPi))>.2f);
    assert(std::abs(std::remainder(locked.camYaw-cam0,2.f*kPi))<.05f);
    in.turn=0;playSlot(c,1);
    bool leftGround=false;
    for(int i=0;i<180;++i){stepCharacter(c,in,kStep);if(!c.grounded)leftGround=true;}
    assert(leftGround && c.grounded && c.y>-1e-4f);

    c={};resetCharacter(c);
    playSlot(c,0);
    assert(c.action==Action::Kick && c.grounded);
    float minLeft=0,minRight=0,chamber=0,minPelvis=9,minChest=9;
    float minLua=9,minLfa=9,minRfa=9,minRua=9;
    bool finished=false;
    const float ballX0=c.ball.x,ballZ0=c.ball.z;
    int hits=0;
    float peakY=c.ball.y;
    bool wasStruck=false;
    float hitT=-1;
    for(int i=0;i<180;++i){
        stepCharacter(c,in,kStep);
        minRight=std::min(minRight,c.pose.anim[int(BoneId::RThigh)].pitch);
        minLeft=std::min(minLeft,c.pose.anim[int(BoneId::LThigh)].pitch);
        minPelvis=std::min(minPelvis,c.pose.anim[int(BoneId::Pelvis)].pitch);
        minChest=std::min(minChest,c.pose.anim[int(BoneId::Chest)].pitch);
        minLua=std::min(minLua,c.pose.anim[int(BoneId::LUpperArm)].pitch);
        minRua=std::min(minRua,c.pose.anim[int(BoneId::RUpperArm)].pitch);
        minLfa=std::min(minLfa,c.pose.anim[int(BoneId::LForearm)].pitch);
        minRfa=std::min(minRfa,c.pose.anim[int(BoneId::RForearm)].pitch);
        if(c.pose.anim[int(BoneId::LThigh)].pitch<-.50f){
            assert(c.pose.anim[int(BoneId::Pelvis)].pitch>.05f);
            assert(c.pose.anim[int(BoneId::Chest)].pitch>.05f);
            assert(c.pose.anim[int(BoneId::RUpperArm)].pitch<-.30f);
        }
        if(c.clipT<.40f)chamber=std::max(chamber,c.pose.anim[int(BoneId::LThigh)].pitch);
        peakY=std::max(peakY,c.ball.y);
        if(c.clipT<.55f)assert(!c.ball.struck);
        if(c.ball.struck && !wasStruck){++hits;hitT=c.clipT;}
        wasStruck=c.ball.struck;
        if(c.action==Action::Idle && i>10){finished=true;break;}
    }
    assert(finished && minLeft<-.50f && minLeft<minRight-0.15f);
    assert(chamber>.40f);
    assert(minPelvis>.03f && minChest>.05f);
    assert(minLua>-.02f && minLfa>-.55f && minRfa>-.55f && minRua<-.30f);
    assert(hits==1 && hitT>=.55f);
    assert(peakY>kBallR+.12f);
    const float fly=std::hypot(c.ball.x-ballX0,c.ball.z-ballZ0);
    assert(fly>.40f);
    assert(c.pose.anim[int(BoneId::LShin)].pitch>=-1e-4f);
    assert(c.pose.anim[int(BoneId::RShin)].pitch>=-1e-4f);
    for(int i=0;i<240;++i)stepCharacter(c,in,kStep);
    assert(std::abs(c.ball.y-kBallR)<.02f);
    assert(std::abs(c.ball.x)<=kArenaHalfExtent && std::abs(c.ball.z)<=kArenaHalfExtent);

    c={};resetCharacter(c);
    const auto idleBall=c.ball;
    in={};in.valid=true;
    for(int i=0;i<60;++i)stepCharacter(c,in,kStep);
    assert(std::abs(c.ball.x-idleBall.x)<1e-5f && std::abs(c.ball.z-idleBall.z)<1e-5f);

    c={};resetCharacter(c);
    c.ball.x=0;c.ball.z=-1.2f;c.ball.y=kBallR;
    playSlot(c,0);
    for(int i=0;i<90;++i)stepCharacter(c,in,kStep);
    assert(!c.ball.struck && std::abs(c.ball.z+1.2f)<.02f);

    c={};resetCharacter(c);
    playSlot(c,0);
    for(int i=0;i<60;++i)stepCharacter(c,in,kStep);
    assert(c.ball.struck);
    const float airZ=c.ball.z,airY=c.ball.y;
    in.toggleMode=true;stepCharacter(c,in,kStep);in.toggleMode=false;
    assert(c.mode==Mode::Pose);
    for(int i=0;i<20;++i)stepCharacter(c,in,kStep);
    assert(std::abs(c.ball.z-airZ)>1e-4f || std::abs(c.ball.y-airY)>1e-4f);

    c={};resetCharacter(c);
    playSlot(c,1);
    assert(c.action==Action::Jump && c.grounded);
    for(int i=0;i<8;++i){
        stepCharacter(c,in,kStep);
        assert(c.grounded && c.action==Action::Jump);
    }
    assert(c.pose.anim[int(BoneId::LThigh)].pitch<-.50f);
    assert(c.pose.anim[int(BoneId::RThigh)].pitch<-.50f);
    assert(c.pose.anim[int(BoneId::LShin)].pitch>.80f);
    assert(c.pose.anim[int(BoneId::Pelvis)].pitch>.05f);
    assert(c.pose.anim[int(BoneId::Chest)].pitch>.04f);
    assert(c.pose.root.y<-.08f);
    assert(c.pose.anim[int(BoneId::LUpperArm)].pitch<-.40f);
    float minThigh=c.pose.anim[int(BoneId::LThigh)].pitch;
    float minShin=c.pose.anim[int(BoneId::LShin)].pitch;
    float maxArm=c.pose.anim[int(BoneId::LUpperArm)].pitch;
    float jumpPeak=0.f;
    bool leftPad=false,sawFall=false,sawLand=false;
    for(int i=0;i<180;++i){
        stepCharacter(c,in,kStep);
        if(!c.grounded){
            leftPad=true;
            jumpPeak=std::max(jumpPeak,c.y);
        }
        if(!c.grounded && (c.action==Action::Jump||c.action==Action::Fall)){
            assert(c.pose.anim[int(BoneId::LThigh)].pitch<.05f);
            assert(c.pose.anim[int(BoneId::RThigh)].pitch<.05f);
            assert(c.pose.anim[int(BoneId::LShin)].pitch>.10f);
            assert(c.pose.anim[int(BoneId::Pelvis)].pitch>=0.f);
            assert(c.pose.anim[int(BoneId::Chest)].pitch>=0.f);
            minThigh=std::min(minThigh,c.pose.anim[int(BoneId::LThigh)].pitch);
            minShin=std::min(minShin,c.pose.anim[int(BoneId::LShin)].pitch);
            maxArm=std::max(maxArm,c.pose.anim[int(BoneId::LUpperArm)].pitch);
        }
        if(c.action==Action::Fall)sawFall=true;
        if(c.action==Action::Land){
            if(!sawLand){
                assert(c.pose.anim[int(BoneId::LThigh)].pitch<0.f);
                assert(c.pose.anim[int(BoneId::LShin)].pitch>.40f);
                assert(c.pose.anim[int(BoneId::Pelvis)].pitch>0.f);
                assert(c.pose.root.y<-.04f);
            }
            sawLand=true;
        }
        if(c.grounded && c.action==Action::Idle && sawLand)break;
    }
    assert(leftPad && sawFall && sawLand && c.grounded);
    assert(minShin<.40f && maxArm>.55f);
    assert(jumpPeak>.20f && jumpPeak<.70f);

    c={};resetCharacter(c);
    in={};in.valid=true;in.forward=1;
    for(int i=0;i<12;++i)stepCharacter(c,in,kStep);
    assert(c.action==Action::Walk);
    assert(c.walkPhase>0.f);
    playSlot(c,1);
    assert(c.action==Action::Jump && c.grounded);
    for(int i=0;i<20 && c.grounded;++i)stepCharacter(c,in,kStep);
    assert(c.action==Action::Jump && !c.grounded);

    c={};resetCharacter(c);
    in={};in.forward=1;in.valid=true;
    int wraps=0,ipsilateral=0,samples=0;
    float lastPhase=c.walkPhase;
    float stanceSpan=0;
    Point stance0{};
    bool stanceArmed=false;
    for(int i=0;i<9000 && wraps<20;++i){
        stepCharacter(c,in,kStep);
        if(c.walkPhase<lastPhase)++wraps;
        lastPhase=c.walkPhase;
        const float swing=std::sin(c.walkPhase);
        if(std::abs(swing)>.25f){
            ++samples;
            const float leftArm=c.pose.anim[int(BoneId::LUpperArm)].pitch;
            const float rightArm=c.pose.anim[int(BoneId::RUpperArm)].pitch;
            if(leftArm*swing>0.f)++ipsilateral;
            if(leftArm*rightArm>0.f)++ipsilateral;
        }
        assert(c.pose.anim[int(BoneId::LShin)].pitch>=-1e-4f);
        assert(c.pose.anim[int(BoneId::RShin)].pitch>=-1e-4f);
        evaluateSkeleton(sk,c.pose);
        const float cadence=std::cos(c.walkPhase);
        if(cadence>=0.f){
            const Point foot=sk.world[int(BoneId::LFoot)].t;
            if(!stanceArmed){stance0=foot;stanceArmed=true;}
            stanceSpan=std::max(stanceSpan,length(subtract(foot,stance0)));
        }else stanceArmed=false;
        assert(std::isfinite(c.x)&&std::isfinite(c.z)&&std::abs(c.x)<=kArenaHalfExtent+1e-4f);
        assert(std::abs(c.z)<=kArenaHalfExtent+1e-4f);
    }
    assert(wraps>=20 && samples>100 && ipsilateral==0);
    assert(stanceSpan<.45f);

    c={};resetCharacter(c);
    in={};in.valid=true;
    Action seen[kPlayClipCount]{};
    for(int n=0;n<kPlayClipCount;++n){
        in.clipStep=1;stepCharacter(c,in,kStep);in.clipStep=0;
        seen[n]=c.action;
        assert(c.clipIndex==n);
    }
    assert(seen[0]==Action::Kick);
    assert(seen[1]==Action::Jump);
    for(int n=2;n<kPlayClipCount;++n)assert(seen[n]==Action::Gesture);
    in.clipStep=1;stepCharacter(c,in,kStep);in.clipStep=0;
    assert(c.action==Action::Kick && c.clipIndex==0);
    c={};resetCharacter(c);
    in={};in.valid=true;in.clipStep=-1;
    stepCharacter(c,in,kStep);in.clipStep=0;
    assert(c.action==Action::Gesture && c.clipIndex==kPlayClipCount-1);
    playSlot(c,2);
    assert(c.action==Action::Gesture && c.clipIndex==2);
    in.clipStep=1;stepCharacter(c,in,kStep);in.clipStep=0;
    assert(c.action==Action::Gesture && c.clipIndex==3);
    playSlot(c,2);
    for(int i=0;i<18;++i)stepCharacter(c,in,kStep);
    assert(c.action==Action::Gesture);
    assert(std::abs(c.pose.anim[int(BoneId::LUpperArm)].pitch)
           +std::abs(c.pose.anim[int(BoneId::RUpperArm)].pitch)>.20f);
    playSlot(c,1);
    for(int i=0;i<24 && c.grounded;++i)stepCharacter(c,in,kStep);
    assert(!c.grounded);
    in.clipStep=1;stepCharacter(c,in,kStep);in.clipStep=0;
    assert(c.action==Action::Jump || c.action==Action::Fall);
    assert(!c.grounded);

    const char* wantSrc[]={
        "wave-left-hand_active","wave-right-hand_active","wave-both-hands_normal",
        "raise-up-left-hand_normal","raise-up-right-hand_normal","raise-up-both-hands_normal",
        "bow_normal","bye_normal","byebye_normal",
        "guide_normal","punch_normal","dance-long_normal","dance-short_normal"};
    for(int g=0;g<kGestureCount;++g){
        const std::string src=kGestureSource[g];
        assert(src.find(wantSrc[g])!=std::string::npos);
        c={};resetCharacter(c);
        in={};in.valid=true;
        playSlot(c,2+g);
        for(int i=0;i<50;++i)stepCharacter(c,in,kStep);
        assert(c.action==Action::Gesture);
        evaluateSkeleton(sk,c.pose);
        const Point head=sk.world[int(BoneId::Head)].t;
        const Point lh=sk.world[int(BoneId::LHand)].t;
        const Point rh=sk.world[int(BoneId::RHand)].t;
        const Point ls=sk.world[int(BoneId::LShoulder)].t;
        const Point rs=sk.world[int(BoneId::RShoulder)].t;
        const Point lf=sk.world[int(BoneId::LFoot)].t;
        const Point rf=sk.world[int(BoneId::RFoot)].t;
        assert(lf.y<.40f && rf.y<.40f);
        assert(length(subtract(ls,rs))>.90f);
        if(g<6){
            assert(length(subtract(lh,head))>.16f);
            assert(length(subtract(rh,head))>.16f);
            if(g==2 || g==5)assert(!(lh.x>-.08f && rh.x<.08f));
        }
    }

    {
        c={};resetCharacter(c);
        in={};in.valid=true;
        playSlot(c,8);
        float minCh=9.f,maxCh=-9.f,minPel=9.f,maxPel=-9.f;
        for(int i=0;i<120 && c.action==Action::Gesture;++i){
            minCh=std::min(minCh,c.pose.anim[int(BoneId::Chest)].pitch);
            maxCh=std::max(maxCh,c.pose.anim[int(BoneId::Chest)].pitch);
            minPel=std::min(minPel,c.pose.anim[int(BoneId::Pelvis)].pitch);
            maxPel=std::max(maxPel,c.pose.anim[int(BoneId::Pelvis)].pitch);
            stepCharacter(c,in,kStep);
        }
        assert(maxCh-minCh>0.10f || maxPel-minPel>0.10f);
        assert(maxCh>0.12f);
    }

    auto yawSpan=[&](const GestureSamp& s,bool left){
        int a0,a1;holdWindow(s,a0,a1);
        float lo=9.f,hi=-9.f;
        for(int i=a0;i<a1;++i){
            const float y=left?s.lY[i]:s.rY[i];
            lo=std::min(lo,y);hi=std::max(hi,y);
        }
        return hi-lo;
    };

    for(int g=0;g<=1;++g){
        const auto s=sampleGesture(2+g);
        int a0,a1;holdWindow(s,a0,a1);
        const bool arenaR=(g==0);
        int raised=0,otherUp=0,m=0;
        float yaws[180]{};
        for(int i=a0;i<a1;++i){
            const float ah=arenaR?s.ry[i]:s.ly[i];
            const float oh=arenaR?s.ly[i]:s.ry[i];
            if(ah>s.hy[i])++raised;
            if(oh>s.hy[i]-0.02f)++otherUp;
            yaws[m++]=arenaR?s.rY[i]:s.lY[i];
        }
        assert(raised>m/2);
        assert(otherUp<m/5);
        assert(yawSpan(s,!arenaR)>0.12f);
        assert(yawReversals(yaws,m)>=2);
    }

    {
        const auto s=sampleGesture(4);
        int a0,a1;holdWindow(s,a0,a1);
        int both=0,n=0;
        for(int i=a0;i<a1;++i){
            if(s.ly[i]>s.hy[i] && s.ry[i]>s.hy[i])++both;
            ++n;
        }
        assert(both>n/2);
        assert(yawSpan(s,true)>0.15f || yawSpan(s,false)>0.15f);
    }

    for(int g=3;g<=4;++g){
        const auto s=sampleGesture(2+g);
        int a0,a1;holdWindow(s,a0,a1);
        const bool arenaR=(g==3);
        int raised=0,wide=0,n=0;
        float lo=9.f,hi=-9.f;
        for(int i=a0;i<a1;++i){
            const float ah=arenaR?s.ry[i]:s.ly[i];
            const float oh=arenaR?s.ly[i]:s.ry[i];
            const float yaw=arenaR?s.rY[i]:s.lY[i];
            if(ah>s.hy[i] && oh<s.hy[i])++raised;
            if(std::abs(arenaR?s.rx[i]:s.lx[i])>0.70f)++wide;
            lo=std::min(lo,yaw);hi=std::max(hi,yaw);
            ++n;
        }
        assert(raised>n/2);
        assert(wide>n/2);
        assert(hi-lo<0.08f);
    }

    {
        const auto s=sampleGesture(7);
        int a0,a1;holdWindow(s,a0,a1);
        int both=0,wide=0,n=0;
        float lLo=9.f,lHi=-9.f,rLo=9.f,rHi=-9.f;
        for(int i=a0;i<a1;++i){
            if(s.ly[i]>s.hy[i] && s.ry[i]>s.hy[i])++both;
            if(std::abs(s.lx[i])>0.70f && std::abs(s.rx[i])>0.70f)++wide;
            lLo=std::min(lLo,s.lY[i]);lHi=std::max(lHi,s.lY[i]);
            rLo=std::min(rLo,s.rY[i]);rHi=std::max(rHi,s.rY[i]);
            ++n;
        }
        assert(both>n/2);
        assert(wide>n/2);
        assert(lHi-lLo<0.08f && rHi-rLo<0.08f);
    }

    {
        const auto guide=sampleGesture(11);
        const auto bye=sampleGesture(9);
        const auto bye2=sampleGesture(10);
        int a0,a1;
        holdWindow(guide,a0,a1);
        float maxZ=-9.f,gyLo=9.f,gyHi=-9.f;
        int pointHold=0,gn=0;
        for(int i=a0;i<a1;++i){
            const float z=std::max(guide.lz[i],guide.rz[i]);
            maxZ=std::max(maxZ,z);
            if(z>0.35f)++pointHold;
            const float yaw=std::abs(guide.lz[i])>std::abs(guide.rz[i])?guide.lY[i]:guide.rY[i];
            gyLo=std::min(gyLo,yaw);gyHi=std::max(gyHi,yaw);
            ++gn;
        }
        assert(maxZ>0.35f);
        assert(pointHold>gn/3);
        assert(gyHi-gyLo<0.20f);

        holdWindow(bye,a0,a1);
        int oneArm=0,notFlat=0,notOver=0,bn=0;
        for(int i=a0;i<a1;++i){
            const float ah=std::max(bye.ly[i],bye.ry[i]);
            const bool lUp=bye.ly[i]>1.65f;
            const bool rUp=bye.ry[i]>1.65f;
            if(lUp!=rUp)++oneArm;
            if(ah>1.65f)++notFlat;
            if(ah<bye.hy[i]+0.10f)++notOver;
            ++bn;
        }
        assert(oneArm>bn/2);
        assert(notFlat>bn/2);
        assert(notOver>bn/2);
        assert(yawSpan(bye,true)>0.12f || yawSpan(bye,false)>0.12f);

        holdWindow(bye2,a0,a1);
        int headish=0,b2n=0;
        for(int i=a0;i<a1;++i){
            if(std::abs(bye2.ly[i]-bye2.hy[i])<0.45f && std::abs(bye2.ry[i]-bye2.hy[i])<0.45f)
                ++headish;
            ++b2n;
        }
        assert(headish>b2n/3);
        const float waveYaw=std::max(yawSpan(sampleGesture(2),true),yawSpan(sampleGesture(2),false));
        const float bye2Yaw=std::max(yawSpan(bye2,true),yawSpan(bye2,false));
        assert(bye2Yaw+0.02f<waveYaw);
        int bye2Both=0;
        holdWindow(bye2,a0,a1);
        for(int i=a0;i<a1;++i){
            if(bye2.ly[i]>1.65f && bye2.ry[i]>1.65f)++bye2Both;
        }
        assert(bye2Both>b2n/3);
    }

    {
        const auto s=sampleGesture(12);
        float maxLZ=-9.f,maxRZ=-9.f,minP=9.f,maxP=-9.f,maxY=-9.f,minY=9.f;
        int lBurst=0,rBurst=0,firstL=-1,firstR=-1;
        bool lHot=false,rHot=false;
        for(int i=0;i<s.n;++i){
            maxLZ=std::max(maxLZ,s.lz[i]);
            maxRZ=std::max(maxRZ,s.rz[i]);
            const float y=std::max(s.ly[i],s.ry[i]);
            const float p=s.lz[i]>=s.rz[i]?s.lP[i]:s.rP[i];
            minP=std::min(minP,p);
            maxP=std::max(maxP,p);
            maxY=std::max(maxY,y);
            minY=std::min(minY,y);
            const bool lNow=s.lz[i]>0.35f && s.lz[i]>s.rz[i]+0.08f;
            const bool rNow=s.rz[i]>0.35f && s.rz[i]>s.lz[i]+0.08f;
            if(lNow && !lHot){++lBurst;if(firstL<0)firstL=i;}
            if(rNow && !rHot){++rBurst;if(firstR<0)firstR=i;}
            lHot=lNow;rHot=rNow;
        }
        assert(maxLZ>0.35f && maxRZ>0.35f);
        assert(minP>-0.70f);
        assert(maxP<0.20f);
        assert(maxY<s.hy[s.n/2]+0.05f);
        assert(minY>0.90f);
        assert(lBurst==1 && rBurst==1);
        assert(firstL>=0 && firstR>=0 && firstL!=firstR);
    }

    for(int g=11;g<=12;++g){
        c={};resetCharacter(c);
        in={};in.valid=true;
        playSlot(c,2+g);
        bool dipped=false;
        int sameLim=0,n=0;
        for(int i=0;i<120 && c.action==Action::Gesture;++i){
            if(c.pose.root.y<c.y-0.06f)dipped=true;
            evaluateSkeleton(sk,c.pose);
            assert(sk.world[int(BoneId::LFoot)].t.y<.45f);
            assert(sk.world[int(BoneId::RFoot)].t.y<.45f);
            const float lp=c.pose.anim[int(BoneId::LUpperArm)].pitch;
            const float rp=c.pose.anim[int(BoneId::RUpperArm)].pitch;
            const bool lLo=lp<-1.38f,rLo=rp<-1.38f,lHi=lp>0.78f,rHi=rp>0.78f;
            if((lLo&&rLo)||(lHi&&rHi))++sameLim;
            ++n;
            stepCharacter(c,in,kStep);
        }
        assert(dipped);
        assert(sameLim*4<n);
    }
    {
        const auto dL=sampleGesture(13);
        const auto dS=sampleGesture(14);
        assert(dL.n>dS.n+15);
        assert(dS.n>110);
    }

    {
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        playSlot(m,2);
        assert(m.action==Action::Gesture && m.clipIndex==2);
        playKick(m);
        assert(m.action==Action::Kick && m.clipIndex==2);
        for(int i=0;i<90;++i)stepCharacter(m,idle,kStep);
        assert(m.action==Action::Idle && m.clipIndex==2);
        idle.clipStep=1;stepCharacter(m,idle,kStep);idle.clipStep=0;
        assert(m.action==Action::Gesture && m.clipIndex==3);
    }

    {
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        setLookAt(m,{3.f,1.7f,0.f});
        stepCharacter(m,idle,kStep);
        const float lookYaw=m.pose.anim[int(BoneId::Head)].yaw;
        assert(lookYaw>0.05f && lookYaw*3.f>0.f);
        clearLookAt(m);
        stepCharacter(m,idle,kStep);
        assert(std::abs(m.pose.anim[int(BoneId::Head)].yaw)<1e-4f);
    }

    {
        CharacterModel m;resetCharacter(m);
        ArenaInput go{};go.valid=true;go.forward=1.f;
        setLookAt(m,{-4.f,1.7f,8.f});
        for(int i=0;i<40;++i)stepCharacter(m,go,kStep);
        assert(m.action==Action::Walk);
        assert(m.pose.anim[int(BoneId::Head)].yaw<0.f);
        assert(m.z>0.6f);
        assert(std::abs(m.x)<0.12f);
    }

    {
        CharacterModel kickA;resetCharacter(kickA);
        CharacterModel kickB=kickA;
        playKick(kickA);playKick(kickB);
        setLookAt(kickB,{5.f,2.f,0.f});
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<12;++i){
            stepCharacter(kickA,idle,kStep);
            stepCharacter(kickB,idle,kStep);
        }
        assert(kickA.action==Action::Kick && kickB.action==Action::Kick);
        assert(std::abs(kickA.pose.anim[int(BoneId::Head)].yaw-kickB.pose.anim[int(BoneId::Head)].yaw)<1e-5f);
        assert(std::abs(kickA.pose.anim[int(BoneId::Head)].pitch-kickB.pose.anim[int(BoneId::Head)].pitch)<1e-5f);

        CharacterModel jumpA;resetCharacter(jumpA);
        CharacterModel jumpB=jumpA;
        playJump(jumpA);playJump(jumpB);
        setLookAt(jumpB,{5.f,2.f,0.f});
        for(int i=0;i<8;++i){
            stepCharacter(jumpA,idle,kStep);
            stepCharacter(jumpB,idle,kStep);
            assert(jumpA.grounded && jumpB.grounded);
        }
        assert(std::abs(jumpA.pose.anim[int(BoneId::Head)].yaw-jumpB.pose.anim[int(BoneId::Head)].yaw)<1e-5f);
        assert(std::abs(jumpA.pose.anim[int(BoneId::Head)].pitch-jumpB.pose.anim[int(BoneId::Head)].pitch)<1e-5f);
    }

    {
        CharacterModel m;resetCharacter(m);
        assert(inStrikeRange(m));
        ArenaInput idle{};idle.valid=true;
        m.x=3.f;m.z=-1.5f;
        const KickStance stance=kickStance(m);
        const float startDist=std::sqrt((m.x-stance.x)*(m.x-stance.x)+(m.z-stance.z)*(m.z-stance.z));
        assert(startDist>2.8f);
        walkTo(m,stance.x,stance.z);
        bool turnedFirst=false;
        bool walkedAfterFace=false;
        for(int i=0;i<360 && m.hasWalkTo;++i){
            const float dx=stance.x-m.x,dz=stance.z-m.z;
            const float want=std::atan2(dx,dz);
            const float err=std::remainder(want-m.heading,2.f*kPi);
            stepCharacter(m,idle,kStep);
            if(std::abs(err)>kTurnThenWalk){
                assert(std::abs(m.forwardSpeed)<1e-4f);
                turnedFirst=true;
            }else if(m.hasWalkTo)walkedAfterFace=true;
        }
        assert(turnedFirst && walkedAfterFace);
        assert(!m.hasWalkTo);
        assert(m.action==Action::Idle);
        const float endDist=std::sqrt((m.x-stance.x)*(m.x-stance.x)+(m.z-stance.z)*(m.z-stance.z));
        assert(endDist<kWalkArrive);
    }

    {
        CharacterModel m;resetCharacter(m);
        m.ball.x=0.f;m.ball.z=-3.f;
        ArenaInput idle{};idle.valid=true;
        const KickStance stance=kickStance(m);
        walkTo(m,stance.x,stance.z);
        for(int i=0;i<360 && m.hasWalkTo;++i){
            if(std::abs(m.heading)<0.50f)assert(std::abs(m.x)<0.08f);
            stepCharacter(m,idle,kStep);
        }
        assert(!m.hasWalkTo);
        const float endDist=std::sqrt((m.x-stance.x)*(m.x-stance.x)+(m.z-stance.z)*(m.z-stance.z));
        assert(endDist<kWalkArrive);
    }

    {
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        playKick(m);
        const float x0=m.x,z0=m.z,h0=m.heading;
        walkTo(m,4.f,4.f);
        assert(characterBusy(m));
        for(int i=0;i<20;++i){
            stepCharacter(m,idle,kStep);
            assert(m.action==Action::Kick);
            assert(std::abs(m.x-x0)<1e-5f && std::abs(m.z-z0)<1e-5f);
            assert(std::abs(std::remainder(m.heading-h0,2.f*kPi))<1e-5f);
        }
    }

    {
        CharacterModel m;resetCharacter(m);
        m.ball.x=8.f;m.ball.z=8.f;
        assert(!inStrikeRange(m));
        ArenaInput kick{};kick.valid=true;kick.clipStep=1;
        stepCharacter(m,kick,kStep);
        assert(m.action==Action::Kick);
        assert(m.clipIndex==0);
        assert(!inStrikeRange(m));
    }

    {
        const auto tick=[&](IdlePilot& p,CharacterModel& m,ArenaInput in){
            stepIdlePilot(p,m,in,kStep);
            stepCharacter(m,in,kStep);
        };
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        m.ball.x=2.5f;m.ball.y=kBallR;m.ball.z=.20f;
        for(int i=0;i<40;++i)tick(p,m,idle);
        assert(p.control==ControlMode::Pilot);
        for(int i=0;i<12;++i)tick(p,m,idle);
        assert(p.control==ControlMode::Auton);
        while(m.action==Action::Gesture)tick(p,m,idle);
        for(int i=0;i<12;++i)tick(p,m,idle);
        if(!m.lookEnabled){
            for(int i=0;i<180 && !m.lookEnabled;++i)tick(p,m,idle);
        }
        const float yaw=m.pose.anim[int(BoneId::Head)].yaw;
        assert(yaw>0.05f && yaw*(m.ball.x-m.x)>0.f);
        assert(m.action!=Action::Kick);

        ArenaInput go=idle;go.forward=1.f;
        tick(p,m,go);
        assert(p.control==ControlMode::Pilot);
        assert(std::abs(m.pose.anim[int(BoneId::Head)].yaw)<1e-4f);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.mode=Mode::Pose;
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<60;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Pilot);
        assert(!m.lookEnabled);
        assert(std::abs(m.pose.anim[int(BoneId::Head)].yaw)<1e-4f);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        idle.valid=false;idle.forward=0;idle.turn=0;
        for(int i=0;i<10;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        assert(m.lookEnabled);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        const int ring=m.clipIndex;
        idle.clipStep=1;
        stepIdlePilot(p,m,idle,kStep);
        stepCharacter(m,idle,kStep);
        assert(p.control==ControlMode::Pilot);
        assert(m.clipIndex==(ring==kPlayClipNone?0:ring+1));
        assert(m.action==Action::Kick);
        assert(!m.lookEnabled);
    }

    {
        const auto tick=[&](IdlePilot& p,CharacterModel& m,ArenaInput in){
            stepIdlePilot(p,m,in,kStep);
            stepCharacter(m,in,kStep);
        };
        const auto waitAuton=[&](IdlePilot& p,CharacterModel& m){
            ArenaInput idle{};idle.valid=true;
            for(int i=0;i<52;++i)tick(p,m,idle);
            assert(p.control==ControlMode::Auton);
        };

        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=kBallSpawnX;m.ball.z=kBallSpawnZ+3.f;
        waitAuton(p,m);
        const int ring=m.clipIndex;
        bool sawMove=false;
        bool kicked=false;
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<800;++i){
            const KickStance s=kickStance(m);
            const float dist=std::sqrt((m.x-s.x)*(m.x-s.x)+(m.z-s.z)*(m.z-s.z));
            if(dist>=kStrikePosTol)assert(m.action!=Action::Kick);
            if(m.action==Action::Walk || m.action==Action::Turn)sawMove=true;
            tick(p,m,idle);
            if(m.action==Action::Kick){kicked=true;break;}
        }
        assert(sawMove && kicked);
        assert(p.skill==AutonSkill::Strike);
        assert(m.clipIndex==ring);
        for(int i=0;i<20;++i){
            assert(m.action==Action::Kick);
            assert(p.skill==AutonSkill::Strike);
            tick(p,m,idle);
        }
        while(m.action==Action::Kick)tick(p,m,idle);
        tick(p,m,idle);
        assert(p.skill==AutonSkill::Attend);
        for(int i=0;i<int(kKickAttend/kStep)-1;++i){
            tick(p,m,idle);
            assert(m.action!=Action::Kick);
        }
    }

    {
        {
            CharacterModel posed;resetCharacter(posed);
            posed.ball.x=0.f;posed.ball.y=kBallR;posed.ball.z=2.6f;
            const float h=kickHeading(posed);
            const KickStance s=kickStanceAt(posed,h);
            posed.x=s.x;posed.z=s.z;posed.heading=h;
            assert(inStrikeRange(posed));
            playKick(posed);
            ArenaInput idle{};idle.valid=true;
            bool posedHit=false;
            while(posed.action==Action::Kick){
                stepCharacter(posed,idle,kStep);
                if(posed.ball.struck)posedHit=true;
            }
            assert(posedHit);
        }

        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        const auto tick=[&]{stepIdlePilot(p,m,idle,kStep);stepCharacter(m,idle,kStep);};
        for(int i=0;i<52;++i)tick();
        for(int i=0;i<36 && m.action==Action::Kick;++i)tick();
        assert(m.action!=Action::Kick);
        while(m.action==Action::Gesture)tick();
        for(int i=0;i<400 && m.action!=Action::Kick;++i)tick();
        assert(m.action==Action::Kick);
        while(m.action==Action::Kick)tick();
        m.ball.x=0.f;m.ball.y=kBallR;m.ball.z=2.6f;
        m.ball.vx=m.ball.vy=m.ball.vz=0.f;m.ball.struck=false;
        p.attendKickT=0;p.playInhibit=0;p.playCap=1.f;p.play=1.f;p.haveSeek=false;
        p.skillAge=kAttendMin;
        p.thinkT=kStrikeThink;
        bool second=false;
        bool hit=false;
        for(int i=0;i<900;++i){
            const KickStance s=kickStance(m);
            const float dist=std::sqrt((m.x-s.x)*(m.x-s.x)+(m.z-s.z)*(m.z-s.z));
            if(dist>=kStrikePosTol)assert(m.action!=Action::Kick);
            tick();
            if(m.action==Action::Kick){second=true;break;}
        }
        assert(second);
        while(m.action==Action::Kick){
            if(m.ball.struck)hit=true;
            tick();
            if(m.ball.struck)hit=true;
        }
        assert(hit);
    }

    {
        CharacterModel m;resetCharacter(m);
        m.x=-.656f;m.z=3.770f;m.heading=.302f;
        m.ball.x=-.817f;m.ball.y=kBallR;m.ball.z=4.633f;
        m.ball.vx=m.ball.vy=m.ball.vz=0.f;
        playKick(m);
        ArenaInput idle{};idle.valid=true;
        bool devicePoseHit=false;
        while(m.action==Action::Kick){
            stepCharacter(m,idle,kStep);
            if(m.ball.struck)devicePoseHit=true;
        }
        assert(devicePoseHit);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=0.f;m.ball.z=-3.f;
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        for(int i=0;i<400;++i){
            if(std::abs(m.heading)<0.50f)assert(std::abs(m.x)<0.08f);
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=8.f;m.ball.z=8.f;
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<200;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
            assert(m.action!=Action::Kick);
            assert(m.action!=Action::Jump);
        }
        assert(p.control==ControlMode::Auton);
        while(m.action==Action::Gesture){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        bool approached=false;
        for(int i=0;i<400;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
            if(p.skill==AutonSkill::Approach || p.skill==AutonSkill::Face)approached=true;
        }
        assert(approached);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        for(int i=0;i<36 && m.action==Action::Kick;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(m.action!=Action::Kick);
        while(m.action==Action::Gesture){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        for(int i=0;i<400 && m.action!=Action::Kick;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(m.action==Action::Kick);
        assert(m.action!=Action::Jump);
        assert(m.clipIndex==kPlayClipNone);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
        const int ring=m.clipIndex;
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        assert(inLeapTrigger(p,m) || m.action==Action::Jump || p.skill==AutonSkill::Attend);
        for(int i=0;i<120 && m.action!=Action::Jump;++i){
            m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(m.action==Action::Jump);
        assert(p.skill==AutonSkill::Leap);
        assert(m.clipIndex==ring);
        while(characterBusy(m)){
            m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
        stepIdlePilot(p,m,idle,kStep);
        stepCharacter(m,idle,kStep);
        assert(p.leapCooldown>0.f);
        for(int i=0;i<int(kLeapCooldown/kStep)-2;++i){
            m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
            assert(m.action!=Action::Jump);
        }
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=8.f;m.ball.z=8.f;
        ArenaInput go{};go.valid=true;go.forward=1.f;
        stepIdlePilot(p,m,go,kStep);
        stepCharacter(m,go,kStep);
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        assert(m.action==Action::Gesture);
        assert(p.skill==AutonSkill::Signal);
        assert(m.clipIndex==kPlayClipNone);
        assert(m.playGestureId==0 || m.playGestureId==1);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        ArenaView cam{};
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep,&cam,false);
            stepCharacter(m,idle,kStep);
        }
        while(m.action==Action::Gesture){
            stepIdlePilot(p,m,idle,kStep,&cam,false);
            stepCharacter(m,idle,kStep);
        }
        for(int i=0;i<400 && m.action!=Action::Kick;++i){
            stepIdlePilot(p,m,idle,kStep,&cam,false);
            stepCharacter(m,idle,kStep);
        }
        while(m.action==Action::Kick){
            stepIdlePilot(p,m,idle,kStep,&cam,false);
            stepCharacter(m,idle,kStep);
        }
        for(int i=0;i<int(kKickAttend/kStep)+2;++i){
            stepIdlePilot(p,m,idle,kStep,&cam,false);
            stepCharacter(m,idle,kStep);
        }
        m.ball.x=6.f;m.ball.z=6.f;m.ball.y=1.1f;m.ball.vy=-.4f;
        m.ball.vx=2.5f;m.ball.vz=2.5f;m.ball.struck=true;
        p.struckRecent=kStruckRecent;
        p.operatorMemory=kOperatorMemory;
        p.signalCooldown=0;
        p.attendKickT=0;
        stepIdlePilot(p,m,idle,kStep,&cam,false);
        stepCharacter(m,idle,kStep);
        assert(m.action==Action::Gesture);
        assert(m.playGestureId==5);
        assert(m.clipIndex==kPlayClipNone);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        p.play=1.f;p.curiosity=1.f;p.vigilance=1.f;p.social=1.f;
        m.ball.x=8.f;m.ball.z=8.f;
        SkillScores s=scoreAutonSkills(p,m);
        assert(s[AutonSkill::Strike]<=.02f);
        assert(s[AutonSkill::Leap]<=.02f);
        assert(s[AutonSkill::Signal]<=.02f);
        p.operatorMemory=kOperatorMemory;
        s=scoreAutonSkills(p,m);
        assert(s[AutonSkill::Signal]>.05f);
        resetCharacter(m);
        p.operatorMemory=0;
        p.thinkT=kStrikeThink;
        s=scoreAutonSkills(p,m);
        assert(s[AutonSkill::Strike]>.50f);
        assert(s[AutonSkill::Leap]<=.02f);
        m.ball.x=.20f;m.ball.z=.20f;m.ball.y=.80f;m.ball.vy=-1.2f;
        s=scoreAutonSkills(p,m);
        assert(s[AutonSkill::Strike]<=.02f);
        assert(s[AutonSkill::Leap]>.50f);
        p.leapCooldown=kLeapCooldown;
        s=scoreAutonSkills(p,m);
        assert(s[AutonSkill::Leap]<=.02f);
    }

    {
        CharacterModel m;resetCharacter(m);
        assert(inStrikeRange(m));
        m.heading=.22f;
        assert(!inStrikeRange(m));
        m.heading=1.f;
        assert(!inStrikeRange(m));
        const KickStance s=kickStanceAt(m,kickHeading(m));
        m.x=s.x;m.z=s.z;m.heading=kickHeading(m);
        assert(inStrikeRange(m));
        m.heading=kickHeading(m)+.80f;
        assert(!inStrikeRange(m));
    }

    {
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        setLookAt(m,{3.f,1.7f,0.f});
        stepCharacter(m,idle,kStep);
        const float yaw1=m.pose.anim[int(BoneId::Head)].yaw;
        for(int i=0;i<40;++i)stepCharacter(m,idle,kStep);
        const float yaw2=m.pose.anim[int(BoneId::Head)].yaw;
        assert(yaw1>0.05f && yaw2>yaw1+0.05f);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=0.f;m.ball.z=-3.f;m.ball.y=kBallR;m.ball.vy=0.f;
        ArenaInput idle{};idle.valid=true;
        bool sawFace=false;
        for(int i=0;i<400;++i){
            m.ball.x=0.f;m.ball.z=-3.f;m.ball.y=kBallR;m.ball.vy=0.f;
            m.ball.vx=m.ball.vz=0.f;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
            if(p.skill==AutonSkill::Face && m.hasFaceYaw)sawFace=true;
            assert(m.action!=Action::Jump);
        }
        assert(p.control==ControlMode::Auton);
        assert(sawFace);
    }

    {
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        while(m.action==Action::Gesture){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        for(int i=0;i<400 && m.action!=Action::Kick;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(m.action==Action::Kick);
        while(m.action==Action::Kick){
            m.ball.x=kBallSpawnX;m.ball.z=kBallSpawnZ;m.ball.y=kBallR;
            m.ball.vx=m.ball.vy=m.ball.vz=0;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        for(int i=0;i<int(kKickAttend/kStep)+2;++i){
            m.ball.x=kBallSpawnX;m.ball.z=kBallSpawnZ;m.ball.y=kBallR;
            m.ball.vx=m.ball.vy=m.ball.vz=0;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        p.attendKickT=0;
        p.skillAge=kAttendMin;
        p.lastStrikeRecent=kDoubleStrikeWindow;
        p.playInhibit=kDoubleStrikeInhibit;
        p.playCap=kDoubleStrikeCap;
        p.play=kDoubleStrikeCap;
        for(int i=0;i<int(kDoubleStrikeInhibit/kStep)-2;++i){
            m.ball.x=kBallSpawnX;m.ball.z=kBallSpawnZ;m.ball.y=kBallR;
            m.ball.vx=m.ball.vy=m.ball.vz=0;
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
            assert(m.action!=Action::Kick);
        }
    }

    c={};resetCharacter(c);
    c.mode=Mode::Pose;c.selected=BoneId::Head;
    in={};in.poseYaw=.5f;stepCharacter(c,in,kStep);
    assert(std::abs(c.pose.anim[int(BoneId::Head)].yaw)>.2f);
    in={};in.poseYaw=3.f;stepCharacter(c,in,kStep);
    assert(c.pose.anim[int(BoneId::Head)].yaw<=.87f+1e-5f);

    ArenaRenderer renderer;assert(renderer.open());
    const auto builds=renderer.meshBuilds();
    const auto indexes=renderer.indexBuilds();
    LGFX_Sprite canvas;canvas.createSprite(468,466);
    LGFX_Sprite before;before.createSprite(468,466);
    ArenaView view{};
    CharacterModel live;resetCharacter(live);
    renderer.render(before,live,view);
    renderer.render(canvas,live,view);
    canvas.save(out+"/arena-idle.ppm");
    const float occupancy=robotScreenHeight(canvas);
    assert(occupancy>=.45f && occupancy<=.55f);
    int ballPixels=0;
    for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x){
        const auto p=pixelAt(canvas,x,y);
        if(p==space::ball||p==space::ballRim)++ballPixels;
    }
    assert(ballPixels>15);

    {
        const auto countBall=[&](const CharacterModel& model){
            renderer.render(canvas,model,view);
            int n=0;
            for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x){
                const auto p=pixelAt(canvas,x,y);
                if(p==space::ball||p==space::ballRim)++n;
            }
            return n;
        };
        CharacterModel hid=live;
        hid.ball.x=0;hid.ball.y=1.08f;hid.ball.z=.18f;
        const int hidden=countBall(hid);
        CharacterModel near=live;
        near.ball.x=0;near.ball.y=1.08f;near.ball.z=-1.15f;
        const int front=countBall(near);
        CharacterModel side=live;
        side.ball.x=-2.4f;side.ball.y=kBallR;side.ball.z=0;
        const int shown=countBall(side);
        assert(front>hidden+20);
        assert(shown>hidden+15);
    }

    in={};in.forward=1;in.valid=true;
    for(int i=0;i<24;++i){
        live.pose.anim[int(BoneId::Head)].yaw=.04f*i;
        renderer.render(canvas,live,view);
    }
    canvas.save(out+"/arena-head.ppm");
    assert(renderer.meshBuilds()==builds && renderer.indexBuilds()==indexes);

    live={};resetCharacter(live);
    in={};in.forward=1;in.valid=true;
    for(int i=0;i<300;++i){
        stepCharacter(live,in,kStep);
        updateFollowView(view,live,kStep);
        renderer.render(canvas,live,view);
    }
    canvas.save(out+"/arena-walk.ppm");
    assert(renderer.meshBuilds()==builds && renderer.indexBuilds()==indexes);
    assert(countChanged(before,canvas)>80);

    live={};resetCharacter(live);view={};
    in={};in.turn=1;in.valid=true;
    for(int i=0;i<180;++i){
        stepCharacter(live,in,kStep);
        updateFollowView(view,live,kStep);
        renderer.render(canvas,live,view);
    }
    canvas.save(out+"/arena-turn.ppm");

    live={};resetCharacter(live);view={};
    in={};in.forward=1;in.valid=true;
    for(int i=0;i<420;++i){
        stepCharacter(live,in,kStep);
        updateFollowView(view,live,kStep);
    }
    renderer.render(canvas,live,view);
    canvas.save(out+"/arena-far.ppm");
    assert(std::abs(live.z)<=kArenaHalfExtent);

    live={};resetCharacter(live);view={};
    in={};in.forward=1;in.valid=true;
    for(int i=0;i<12;++i)stepCharacter(live,in,kStep);
    playSlot(live,1);
    for(int i=0;i<24 && live.grounded;++i)stepCharacter(live,in,kStep);
    for(int i=0;i<18;++i){
        stepCharacter(live,in,kStep);
        updateFollowView(view,live,kStep);
    }
    renderer.render(canvas,live,view);
    canvas.save(out+"/arena-jump.ppm");
    assert(!live.grounded || live.action==Action::Land || live.action==Action::Jump);
    assert(renderer.meshBuilds()==builds && renderer.indexBuilds()==indexes);

    live={};resetCharacter(live);view={};
    playSlot(live,0);
    assert(live.action==Action::Kick);
    for(int i=0;i<36;++i){
        stepCharacter(live,in,kStep);
        updateFollowView(view,live,kStep);
    }
    renderer.render(canvas,live,view);
    canvas.save(out+"/arena-kick.ppm");
    assert(live.action==Action::Kick);
    int kickBall=0;
    for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x){
        const auto p=pixelAt(canvas,x,y);
        if(p==space::ball||p==space::ballRim)++kickBall;
    }
    assert(kickBall>20);
    assert(renderer.meshBuilds()==builds && renderer.indexBuilds()==indexes);

    std::cout<<"p5";
    for(int g=0;g<kGestureCount;++g){
        live={};resetCharacter(live);view={};
        in={};in.valid=true;
        playSlot(live,2+g);
        for(int i=0;i<50;++i){
            stepCharacter(live,in,kStep);
            updateFollowView(view,live,kStep);
        }
        renderer.render(canvas,live,view);
        std::array<double,8> samples{};
        for(int s=0;s<8;++s){
            const auto t0=std::chrono::steady_clock::now();
            renderer.render(canvas,live,view);
            const auto t1=std::chrono::steady_clock::now();
            samples[s]=std::chrono::duration<double,std::micro>(t1-t0).count();
        }
        std::sort(samples.begin(),samples.end());
        const double hostUs=.5*(samples[3]+samples[4]);
        std::cout<<" "<<kGestureHud[g]<<"="<<kGestureFlashBytes[g]<<"B/"<<int(hostUs+.5)<<"us";
        if(g==0)canvas.save(out+"/arena-wave-l.ppm");
        if(g==2)canvas.save(out+"/arena-wave2.ppm");
        if(g==5)canvas.save(out+"/arena-up2.ppm");
    }
    std::cout<<"\n";

    lets_and_go::DeviceControlLogic logic;
    logic.setScreen(lets_and_go::GameScreen::ArenaPlay);
    logic.presentScreen(lets_and_go::GameScreen::ArenaPlay);
    logic.touch(false,0,0);
    assert(lets_and_go::menuTouchTarget(lets_and_go::GameScreen::ArenaPlay,32,233)==lets_and_go::TouchAction::Previous);
    logic.touch(true,234,300);
    auto frame=logic.consume(true);
    assert(frame.input.viewAxis>0.4f && std::abs(frame.input.steer)<.05f);
    logic.touch(true,360,363);
    frame=logic.consume(true);
    assert(frame.input.steer>0.4f);
    logic.touch(false,0,0);
    frame=logic.consume(true);
    assert(frame.input.steer==0 && frame.input.viewAxis==0);
    logic.touch(true,234,80);
    logic.touch(true,260,90);
    frame=logic.consume(true);
    assert(frame.preview.active && frame.input.steer==0);

    lets_and_go::DeviceControlFrame merged{};
    merged.input.valid=true;merged.input.steer=.2f;
    lets_and_go::RacerInput pad{};
    pad.valid=true;pad.steer=-.7f;pad.viewAxis=.9f;
    mergeExternalPad(merged,pad);
    assert(merged.input.steer==-.7f && merged.input.viewAxis==.9f && merged.input.valid);
    merged={};merged.input.valid=true;merged.input.steer=1.f;
    pad={};pad.confirmPressed=true;
    mergeExternalPad(merged,pad);
    assert(merged.input.confirmPressed && merged.input.steer==1.f && merged.input.valid);
    merged={};pad={};pad.pausePressed=true;pad.navigationStep=1;
    mergeExternalPad(merged,pad);
    assert(merged.input.pausePressed && merged.navigation==1 && merged.input.valid);

    ArenaController padTurn;
    padTurn.reset();
    lets_and_go::DeviceControlFrame rightStick{};
    rightStick.input.valid=true;
    rightStick.input.steer=1.f;
    const float headingBefore=padTurn.character().heading;
    for(int i=0;i<30;++i)padTurn.update(rightStick,16u*(i+1));
    assert(std::remainder(padTurn.character().heading-headingBefore,2.f*kPi)<0.f);

    ArenaController lookUp;
    lookUp.reset();
    lets_and_go::DeviceControlFrame orbit{};
    orbit.input.valid=true;
    orbit.preview.active=true;
    orbit.preview.changed=true;
    orbit.preview.dy=-400;
    lookUp.update(orbit,16);
    assert(lookUp.view().pitch<=-75.f*kPi/180.f+1e-4f);

    ArenaController bodyKick;
    bodyKick.reset();
    lets_and_go::DeviceControlFrame aClick{};
    aClick.input.cancelPressed=true;
    for(int i=0;i<3;++i)bodyKick.update(aClick,16u*(i+1));
    assert(bodyKick.character().action==Action::Kick);
    ArenaController bodyJump;
    bodyJump.reset();
    lets_and_go::DeviceControlFrame bClick{};
    bClick.input.confirmPressed=true;
    for(int i=0;i<3;++i)bodyJump.update(bClick,16u*(i+1));
    assert(bodyJump.character().action==Action::Gesture);
    assert(bodyJump.character().clipIndex==kPlayClipCount-1);
    ArenaController bodyHop;
    bodyHop.reset();
    lets_and_go::DeviceControlFrame aOnce{};
    aOnce.input.cancelPressed=true;
    bodyHop.update(aOnce,16);
    aOnce.input.cancelPressed=false;
    bodyHop.update(aOnce,32);
    aOnce.input.cancelPressed=true;
    bodyHop.update(aOnce,48);
    assert(bodyHop.character().action==Action::Jump);
    lets_and_go::DeviceControlFrame rest{};
    rest.input.valid=true;
    for(int i=3;i<24;++i)bodyHop.update(rest,16u*(i+1));
    assert(bodyHop.character().action==Action::Jump && !bodyHop.character().grounded);

    renderer.render(canvas,live,lookUp.view());
    canvas.save(out+"/arena-lookup.ppm");
    int floorPixels=0;
    for(int y=0;y<canvas.height();++y)for(int x=0;x<canvas.width();++x){
        const auto p=pixelAt(canvas,x,y);
        if(p==space::grid||p==space::seam)++floorPixels;
    }
    assert(floorPixels>80);

    std::cout<<"panels="<<mesh.count<<" mesh_builds="<<builds<<" index_builds="<<indexes
             <<" occupancy="<<occupancy<<" head_faces="<<bones[int(BoneId::Head)]<<"\n";
    std::cout<<"gundam_arena ok\n";
    return 0;
}
