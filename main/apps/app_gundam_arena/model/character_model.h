#pragma once
#include "rx78_bones.h"

namespace gundam_arena {
enum class Mode : uint8_t { Play, Pose };
enum class Action : uint8_t { Idle, Walk, Turn, Jump, Fall, Land, Kick };

inline constexpr float kArenaHalfExtent=16.f;
inline constexpr float kGravity=18.f,kJumpVel=6.2f,kWalkSpeed=2.0f,kTurnSpeed=1.8f;
inline constexpr float kJumpCrouch=.20f,kJumpDip=.12f,kJumpLand=.20f;
inline constexpr float kStep=1.f/60.f;
inline constexpr float kBallR=.16f,kFootR=.14f;
inline constexpr float kFootToeY=-.18f,kFootToeZ=.20f;
inline constexpr float kBallSpawnX=-.42f,kBallSpawnZ=.74f;

struct ArenaInput {
    float forward=0,turn=0;
    bool jump=false,kick=false,valid=true;
    int jointStep=0;
    float poseYaw=0,posePitch=0;
    bool poseReset=false,toggleMode=false,exit=false;
};

struct ArenaBall {
    float x=kBallSpawnX,y=kBallR,z=kBallSpawnZ;
    float vx=0,vy=0,vz=0;
    bool struck=false;
};

struct CharacterModel {
    Mode mode=Mode::Play;
    Action action=Action::Idle;
    float x=0,y=0,z=0;
    float heading=0,forwardSpeed=0,angularSpeed=0,vy=0;
    bool grounded=true;
    float walkPhase=0,landT=0,clipT=0;
    bool leftPlanted=false,rightPlanted=false;
    float leftPlantX=0,leftPlantZ=0,rightPlantX=0,rightPlantZ=0;
    BoneId selected=BoneId::Head;
    SkeletonPose pose{};
    ArenaBall ball{};
    Point kickToe{};
    bool kickToeHad=false;
    uint32_t meshBuilds=0;
};

void resetCharacter(CharacterModel& c);
void stepCharacter(CharacterModel& c,const ArenaInput& in,float dt);
Point boneWorld(const Skeleton& sk,BoneId bone);
} // namespace gundam_arena
