#pragma once
#include "rx78_bones.h"

namespace gundam_arena {
enum class Mode : uint8_t { Play, Pose };
enum class Action : uint8_t { Idle, Walk, Turn, Jump, Fall, Land, Kick, Gesture };

inline constexpr float kArenaHalfExtent=16.f;
inline constexpr float kGravity=18.f,kJumpVel=4.f,kWalkSpeed=2.0f,kTurnSpeed=1.8f;
inline constexpr float kJumpCrouch=.20f,kJumpDip=.12f,kJumpLand=.20f;
inline constexpr float kJumpSquatY=.14f,kJumpLandY=.10f,kPlantAnkleY=.26f;
inline constexpr float kStep=1.f/60.f;
inline constexpr float kBallR=.16f,kFootR=.20f;
inline constexpr float kFootToeY=-.18f,kFootToeZ=.20f;
inline constexpr float kBallSpawnX=-.42f,kBallSpawnZ=.74f;
inline constexpr int kPlayClipCount=8;
inline constexpr int kPlayClipNone=8;
inline constexpr float kStrikePosTol=.05f;
inline constexpr float kStrikeFaceTol=.05f;
inline constexpr float kWalkArrive=.03f;
inline constexpr float kFaceArrive=.04f;
inline constexpr float kTurnThenWalk=.80f;
inline constexpr float kStickDeadzone=.18f;
inline constexpr float kLookTau=.15f;
inline constexpr float kKickMinLift=5.f;

inline const char* playClipHud(int index){
    static constexpr const char* names[]={
        "KICK","JUMP","WAVE L","WAVE R","WAVE 2","UP L","UP R","UP 2"};
    if(index>=0 && index<kPlayClipCount)return names[index];
    return "A/B CLIP";
}

struct ArenaInput {
    float forward=0,turn=0;
    int clipStep=0;
    bool valid=true;
    int jointStep=0;
    float poseYaw=0,posePitch=0;
    bool poseReset=false,toggleMode=false,exit=false;
};

struct ArenaBall {
    float x=kBallSpawnX,y=kBallR,z=kBallSpawnZ;
    float vx=0,vy=0,vz=0;
    bool struck=false;
};

struct KickStance {
    float x=0,z=0;
};

struct CharacterModel {
    Mode mode=Mode::Play;
    Action action=Action::Idle;
    float x=0,y=0,z=0;
    float heading=0,forwardSpeed=0,angularSpeed=0,vy=0;
    bool grounded=true;
    float walkPhase=0,landT=0,clipT=0;
    int clipIndex=kPlayClipNone;
    int playGestureId=0;
    bool leftPlanted=false,rightPlanted=false;
    float leftPlantX=0,leftPlantZ=0,rightPlantX=0,rightPlantZ=0;
    BoneId selected=BoneId::Head;
    SkeletonPose pose{};
    ArenaBall ball{};
    Point kickToe{};
    bool kickToeHad=false;
    bool lookEnabled=false;
    float lookX=0,lookY=0,lookZ=0;
    float lookYaw=0,lookPitch=0,lookNeck=0;
    bool hasWalkTo=false;
    float walkToX=0,walkToZ=0;
    bool hasFaceYaw=false;
    float faceYaw=0;
    uint32_t meshBuilds=0;
    float kickMinGap=9.f;
    float kickMaxFwd=-9.f;
    float kickMinGapFwd=9.f;
};

void resetCharacter(CharacterModel& c);
void stepCharacter(CharacterModel& c,const ArenaInput& in,float dt);
Point boneWorld(const Skeleton& sk,BoneId bone);

void playKick(CharacterModel& c);
void playJump(CharacterModel& c);
void playGesture(CharacterModel& c,int id);
bool characterBusy(const CharacterModel& c);
void setLookAt(CharacterModel& c,Point world);
void clearLookAt(CharacterModel& c);
void faceYaw(CharacterModel& c,float yaw);
void walkTo(CharacterModel& c,float x,float z);
void clearSeek(CharacterModel& c);
KickStance kickStance(const CharacterModel& c);
KickStance kickStanceAt(const CharacterModel& c,float heading);
float kickHeading(const CharacterModel& c);
float kickAlignErr(const CharacterModel& c);
bool inStrikeRange(const CharacterModel& c);

inline const char* playActionHud(const CharacterModel& c){
    if(c.action==Action::Kick)return "KICK";
    if(c.action==Action::Jump || c.action==Action::Fall || c.action==Action::Land)return "JUMP";
    if(c.action==Action::Gesture)return playClipHud(c.playGestureId+2);
    return playClipHud(c.clipIndex);
}
} // namespace gundam_arena
