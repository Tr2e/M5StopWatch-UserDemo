#pragma once
#include "../../app_gundam_museum/model/rx78.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace gundam_arena {
using gundam_museum::Point;
using gundam_museum::Part;
using gundam_museum::Mesh;
using gundam_museum::dot;
using gundam_museum::cross;
using gundam_museum::subtract;

enum class BoneId : uint8_t {
    Root, Pelvis, Chest, Neck, Head,
    LShoulder, LUpperArm, LForearm, LHand,
    RShoulder, RUpperArm, RForearm, RHand,
    LThigh, LShin, LFoot,
    RThigh, RShin, RFoot,
    Count
};

inline constexpr int kBoneCount=int(BoneId::Count);
inline constexpr float kPi=3.14159265359f;
inline constexpr float kHipY=1.02f,kHipX=.285f,kSpread=.12f;
inline constexpr float kThighLen=.33f,kShinLen=.44f,kUpperOffset=.14f;
inline constexpr float kChestY=1.50f,kNeckY=2.025f-kUpperOffset;
inline constexpr float kShoulderX=.72f,kShoulderY=1.90f-kUpperOffset;
inline constexpr float kUpperArmLen=.405f,kForearmLen=.35f;
inline constexpr float kArmOut=.18f,kElbowBend=.22f,kFootYaw=.15f;
inline constexpr float kSoleY=.025f;

inline BoneId parentOf(BoneId bone){
    switch(bone){
        case BoneId::Pelvis: return BoneId::Root;
        case BoneId::Chest: return BoneId::Pelvis;
        case BoneId::Neck: return BoneId::Chest;
        case BoneId::Head: return BoneId::Neck;
        case BoneId::LShoulder: case BoneId::RShoulder: return BoneId::Chest;
        case BoneId::LUpperArm: return BoneId::LShoulder;
        case BoneId::RUpperArm: return BoneId::RShoulder;
        case BoneId::LForearm: return BoneId::LUpperArm;
        case BoneId::RForearm: return BoneId::RUpperArm;
        case BoneId::LHand: return BoneId::LForearm;
        case BoneId::RHand: return BoneId::RForearm;
        case BoneId::LThigh: case BoneId::RThigh: return BoneId::Pelvis;
        case BoneId::LShin: return BoneId::LThigh;
        case BoneId::RShin: return BoneId::RThigh;
        case BoneId::LFoot: return BoneId::LShin;
        case BoneId::RFoot: return BoneId::RShin;
        default: return BoneId::Count;
    }
}

inline const char* boneName(BoneId bone){
    static constexpr const char* names[]={
        "Root","Pelvis","Chest","Neck","Head",
        "LShoulder","LUpperArm","LForearm","LHand",
        "RShoulder","RUpperArm","RForearm","RHand",
        "LThigh","LShin","LFoot","RThigh","RShin","RFoot"};
    const int i=int(bone);return i>=0&&i<kBoneCount?names[i]:"?";
}

struct JointEuler { float roll=0,pitch=0,yaw=0; };
struct JointLimit { float rollMin=0,rollMax=0,pitchMin=0,pitchMax=0,yawMin=0,yawMax=0; };

inline JointLimit limitOf(BoneId bone){
    switch(bone){
        case BoneId::Head: return {-.20f,.20f,-.26f,.44f,-.87f,.87f};
        case BoneId::Neck: return {-.10f,.10f,-.12f,.18f,-.35f,.35f};
        case BoneId::Chest: return {-.12f,.12f,-.15f,.20f,-.40f,.40f};
        case BoneId::Pelvis: return {-.10f,.10f,-.20f,.25f,-kPi,kPi};
        case BoneId::LShoulder: case BoneId::RShoulder: return {-.55f,.55f,-1.10f,.55f,-.70f,.70f};
        case BoneId::LUpperArm: case BoneId::RUpperArm: return {-.35f,.35f,-1.40f,.80f,-.80f,.80f};
        case BoneId::LForearm: case BoneId::RForearm: return {-.20f,.20f,-2.20f,.05f,-.40f,.40f};
        case BoneId::LHand: case BoneId::RHand: return {-.40f,.40f,-.70f,.70f,-.60f,.60f};
        case BoneId::LThigh: case BoneId::RThigh: return {-.35f,.35f,-.80f,.90f,-.40f,.40f};
        case BoneId::LShin: case BoneId::RShin: return {-.08f,.08f,0.f,1.80f,-.12f,.12f};
        case BoneId::LFoot: case BoneId::RFoot: return {-.15f,.15f,-.45f,.35f,-.35f,.35f};
        default: return {};
    }
}

inline JointEuler clampJoint(BoneId bone,JointEuler e){
    const auto lim=limitOf(bone);
    const auto sat=[](float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;};
    return {sat(e.roll,lim.rollMin,lim.rollMax),sat(e.pitch,lim.pitchMin,lim.pitchMax),
            sat(e.yaw,lim.yawMin,lim.yawMax)};
}

struct Affine {
    float r00=1,r01=0,r02=0,r10=0,r11=1,r12=0,r20=0,r21=0,r22=1;
    Point t{};
    Point apply(Point p)const{
        return {r00*p.x+r01*p.y+r02*p.z+t.x,r10*p.x+r11*p.y+r12*p.z+t.y,r20*p.x+r21*p.y+r22*p.z+t.z};
    }
    Point rotate(Point p)const{
        return {r00*p.x+r01*p.y+r02*p.z,r10*p.x+r11*p.y+r12*p.z,r20*p.x+r21*p.y+r22*p.z};
    }
    Affine inverse()const{
        Affine i;
        i.r00=r00;i.r01=r10;i.r02=r20;i.r10=r01;i.r11=r11;i.r12=r21;i.r20=r02;i.r21=r12;i.r22=r22;
        i.t=i.rotate({-t.x,-t.y,-t.z});
        return i;
    }
};

inline Affine mul(const Affine& a,const Affine& b){
    Affine o;
    o.r00=a.r00*b.r00+a.r01*b.r10+a.r02*b.r20;
    o.r01=a.r00*b.r01+a.r01*b.r11+a.r02*b.r21;
    o.r02=a.r00*b.r02+a.r01*b.r12+a.r02*b.r22;
    o.r10=a.r10*b.r00+a.r11*b.r10+a.r12*b.r20;
    o.r11=a.r10*b.r01+a.r11*b.r11+a.r12*b.r21;
    o.r12=a.r10*b.r02+a.r11*b.r12+a.r12*b.r22;
    o.r20=a.r20*b.r00+a.r21*b.r10+a.r22*b.r20;
    o.r21=a.r20*b.r01+a.r21*b.r11+a.r22*b.r21;
    o.r22=a.r20*b.r02+a.r21*b.r12+a.r22*b.r22;
    o.t=a.rotate(b.t);o.t.x+=a.t.x;o.t.y+=a.t.y;o.t.z+=a.t.z;
    return o;
}

inline Affine eulerTRS(Point origin,float roll,float pitch,float yaw){
    const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
    const float cr=std::cos(roll),sr=std::sin(roll);
    Affine rx;rx.r11=cp;rx.r12=-sp;rx.r21=sp;rx.r22=cp;
    Affine ry;ry.r00=cy;ry.r02=sy;ry.r20=-sy;ry.r22=cy;
    Affine rz;rz.r00=cr;rz.r01=-sr;rz.r10=sr;rz.r11=cr;
    Affine r=mul(rz,mul(ry,rx));
    r.t=origin;
    return r;
}

struct SkeletonPose {
    std::array<JointEuler,kBoneCount> anim{};
    Point root{};
    float rootYaw=0;
};

struct Skeleton {
    std::array<Affine,kBoneCount> restLocal{};
    std::array<Affine,kBoneCount> restWorld{};
    std::array<Affine,kBoneCount> world{};
};

inline float length(Point p){return std::sqrt(dot(p,p));}
inline Point add(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point scale(Point p,float s){return {p.x*s,p.y*s,p.z*s};}
inline Point normalize(Point p){const float l=length(p);return l<1e-8f?Point{0,-1,0}:scale(p,1.f/l);}

void makeBindSkeleton(Skeleton& sk);
void evaluateSkeleton(Skeleton& sk,const SkeletonPose& pose);
void localizeMesh(Mesh& mesh,const Skeleton& bind);
void buildRx78Rigged(Mesh& mesh,Skeleton& bind);
} // namespace gundam_arena
