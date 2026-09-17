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
    static_assert(kGestureCount==6 && kGestureFrames[0]==50,"gesture bank drifted");
    static_assert(kGestureFlashBytes[0]==10800 && kGestureFlashBytes[5]==10800,"gesture flash size drifted");
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
        "raise-up-left-hand_normal","raise-up-right-hand_normal","raise-up-both-hands_normal"};
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
        assert(length(subtract(lh,head))>.16f);
        assert(length(subtract(rh,head))>.16f);
        if(g==2 || g==5)assert(!(lh.x>-.08f && rh.x<.08f));
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
        for(int i=0;i<400;++i){
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
        IdlePilot p;resetIdlePilot(p);
        CharacterModel m;resetCharacter(m);
        m.ball.x=0.f;m.ball.z=-3.f;
        ArenaInput idle{};idle.valid=true;
        for(int i=0;i<52;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
        }
        assert(p.control==ControlMode::Auton);
        for(int i=0;i<180;++i){
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
        for(int i=0;i<90;++i){
            stepIdlePilot(p,m,idle,kStep);
            stepCharacter(m,idle,kStep);
            assert(m.action!=Action::Kick);
            assert(m.action!=Action::Jump);
            assert(m.action!=Action::Gesture);
            assert(!inStrikeRange(m));
        }
        assert(p.control==ControlMode::Auton);
        assert(p.skill==AutonSkill::Approach);
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
        assert(inLeapTrigger(p,m) || m.action==Action::Jump);
        for(int i=0;i<30 && m.action!=Action::Jump;++i){
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
