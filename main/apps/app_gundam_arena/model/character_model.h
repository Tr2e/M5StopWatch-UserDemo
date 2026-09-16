#pragma once
#include "rx78_bones.h"

namespace gundam_arena {
enum class Mode : uint8_t { Play, Pose };
enum class Action : uint8_t { Idle, Walk, Turn, Jump, Fall, Land };

struct ArenaInput {
    float forward=0,turn=0;
    bool jump=false,valid=true;
    int jointStep=0;
    float poseYaw=0,posePitch=0;
    bool poseReset=false,toggleMode=false,exit=false;
};

struct CharacterModel {
    Mode mode=Mode::Play;
    Action action=Action::Idle;
    float x=0,y=0,z=0;
    float heading=0,forwardSpeed=0,angularSpeed=0,vy=0;
    bool grounded=true;
    float walkPhase=0,landT=0;
    bool leftPlanted=false,rightPlanted=false;
    float leftPlantX=0,leftPlantZ=0,rightPlantX=0,rightPlantZ=0;
    BoneId selected=BoneId::Head;
    SkeletonPose pose{};
    uint32_t meshBuilds=0;
};

inline constexpr float kArenaHalfExtent=16.f;
inline constexpr float kGravity=18.f,kJumpVel=6.2f,kWalkSpeed=2.0f,kTurnSpeed=1.8f;
inline constexpr float kStep=1.f/60.f;

void resetCharacter(CharacterModel& c);
void stepCharacter(CharacterModel& c,const ArenaInput& in,float dt);
Point boneWorld(const Skeleton& sk,BoneId bone);
} // namespace gundam_arena
