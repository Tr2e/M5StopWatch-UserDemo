#include "../main/apps/app_gundam_arena/view/arena_renderer.h"
#include "../main/apps/app_gundam_arena/controller/arena_controller.h"
#include "../main/apps/app_gundam_arena/model/arena_kick_clip.h"
#include "../tools/arena_motion/arena_walk_clip.h"
#include "../main/apps/app_lets_and_go_racer/input/device_control_logic.h"
#include <algorithm>
#include <array>
#include <cassert>
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
    static_assert(kKickFrames==44,"kick clip window drifted");
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
    in.turn=0;in.jump=true;stepCharacter(c,in,kStep);in.jump=false;
    bool leftGround=false;
    for(int i=0;i<180;++i){stepCharacter(c,in,kStep);if(!c.grounded)leftGround=true;}
    assert(leftGround && c.grounded && c.y>-1e-4f);

    c={};resetCharacter(c);
    in={};in.valid=true;in.kick=true;
    stepCharacter(c,in,kStep);in.kick=false;
    assert(c.action==Action::Kick && c.grounded);
    float minLeft=0,minRight=0,chamber=0;
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
        if(c.clipT<.40f)chamber=std::max(chamber,c.pose.anim[int(BoneId::LThigh)].pitch);
        peakY=std::max(peakY,c.ball.y);
        if(c.clipT<.55f)assert(!c.ball.struck);
        if(c.ball.struck && !wasStruck){++hits;hitT=c.clipT;}
        wasStruck=c.ball.struck;
        if(c.action==Action::Idle && i>10){finished=true;break;}
    }
    assert(finished && minLeft<-.50f && minLeft<minRight-0.15f);
    assert(chamber>.40f);
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
    in={};in.valid=true;in.kick=true;
    stepCharacter(c,in,kStep);in.kick=false;
    for(int i=0;i<90;++i)stepCharacter(c,in,kStep);
    assert(!c.ball.struck && std::abs(c.ball.z+1.2f)<.02f);

    c={};resetCharacter(c);
    in={};in.valid=true;in.kick=true;
    stepCharacter(c,in,kStep);in.kick=false;
    for(int i=0;i<80;++i)stepCharacter(c,in,kStep);
    assert(c.ball.struck);
    const float airZ=c.ball.z,airY=c.ball.y;
    in.toggleMode=true;stepCharacter(c,in,kStep);in.toggleMode=false;
    assert(c.mode==Mode::Pose);
    for(int i=0;i<20;++i)stepCharacter(c,in,kStep);
    assert(std::abs(c.ball.z-airZ)>1e-4f || std::abs(c.ball.y-airY)>1e-4f);

    c={};resetCharacter(c);
    in={};in.valid=true;in.jump=true;
    stepCharacter(c,in,kStep);in.jump=false;
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
    in.jump=true;stepCharacter(c,in,kStep);in.jump=false;
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
    in.jump=true;stepCharacter(live,in,kStep);in.jump=false;
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
    in={};in.kick=true;in.valid=true;
    stepCharacter(live,in,kStep);in.kick=false;
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
    assert(bodyJump.character().action==Action::Jump);
    lets_and_go::DeviceControlFrame rest{};
    rest.input.valid=true;
    for(int i=3;i<24;++i)bodyJump.update(rest,16u*(i+1));
    assert(bodyJump.character().action==Action::Jump && !bodyJump.character().grounded);

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
