#include "rx78_bones.h"
#include <algorithm>

namespace gundam_arena {
namespace {
Affine boneRestWorld(BoneId bone){
    const float s= (bone==BoneId::LShoulder||bone==BoneId::LUpperArm||bone==BoneId::LForearm||
                    bone==BoneId::LHand||bone==BoneId::LThigh||bone==BoneId::LShin||bone==BoneId::LFoot)?-1.f:1.f;
    switch(bone){
        case BoneId::Root: return {};
        case BoneId::Pelvis: return eulerTRS({0,kHipY,0},0,0,0);
        case BoneId::Chest: return eulerTRS({0,kChestY,0},0,0,0);
        case BoneId::Neck: case BoneId::Head: return eulerTRS({0,kNeckY,0},0,0,0);
        case BoneId::LShoulder: case BoneId::RShoulder:
        case BoneId::LUpperArm: case BoneId::RUpperArm:
            return eulerTRS({s*kShoulderX,kShoulderY,0},s*kArmOut,0,0);
        case BoneId::LForearm: case BoneId::RForearm: {
            const Affine arm=eulerTRS({s*kShoulderX,kShoulderY,0},s*kArmOut,0,0);
            const Point elbow=arm.apply({0,-kUpperArmLen,0});
            return eulerTRS(elbow,s*kArmOut,kElbowBend,0);
        }
        case BoneId::LHand: case BoneId::RHand: {
            const Affine arm=eulerTRS({s*kShoulderX,kShoulderY,0},s*kArmOut,0,0);
            const Point elbow=arm.apply({0,-kUpperArmLen,0});
            const Affine fore=eulerTRS(elbow,s*kArmOut,kElbowBend,0);
            return eulerTRS(fore.apply({0,-kForearmLen,0}),s*kArmOut,kElbowBend,0);
        }
        case BoneId::LThigh: case BoneId::RThigh:
            return eulerTRS({s*kHipX,kHipY,0},s*kSpread,0,0);
        case BoneId::LShin: case BoneId::RShin: {
            const Affine thigh=eulerTRS({s*kHipX,kHipY,0},s*kSpread,0,0);
            return eulerTRS(thigh.apply({0,-kThighLen,0}),s*kSpread,0,0);
        }
        case BoneId::LFoot: case BoneId::RFoot: {
            const Affine thigh=eulerTRS({s*kHipX,kHipY,0},s*kSpread,0,0);
            const Affine shin=eulerTRS(thigh.apply({0,-kThighLen,0}),s*kSpread,0,0);
            return eulerTRS(shin.apply({0,-kShinLen,0}),0,0,s*kFootYaw);
        }
        default: return {};
    }
}
}

void makeBindSkeleton(Skeleton& sk){
    for(int i=0;i<kBoneCount;++i){
        const auto bone=BoneId(i);
        sk.restWorld[i]=boneRestWorld(bone);
        const auto p=parentOf(bone);
        if(p==BoneId::Count)sk.restLocal[i]=sk.restWorld[i];
        else sk.restLocal[i]=mul(sk.restWorld[int(p)].inverse(),sk.restWorld[i]);
        sk.world[i]=sk.restWorld[i];
    }
}

void evaluateSkeleton(Skeleton& sk,const SkeletonPose& pose){
    for(int i=0;i<kBoneCount;++i){
        const auto bone=BoneId(i);
        const auto e=clampJoint(bone,pose.anim[i]);
        Affine anim=eulerTRS({0,0,0},e.roll,e.pitch,e.yaw);
        Affine local=mul(sk.restLocal[i],anim);
        const auto p=parentOf(bone);
        if(bone==BoneId::Root){
            Affine root=eulerTRS(pose.root,0,0,pose.rootYaw);
            sk.world[i]=mul(root,local);
        }else sk.world[i]=mul(sk.world[int(p)],local);
    }
}

void localizeMesh(Mesh& mesh,const Skeleton& bind){
    for(std::size_t i=0;i<mesh.count;++i){
        const int bone=int(mesh.panels[i].rigidPart?mesh.panels[i].rigidPart-1:0);
        const int id=std::clamp(bone,0,kBoneCount-1);
        const Affine inv=bind.restWorld[id].inverse();
        for(auto& p:mesh.panels[i].point)p=inv.apply(p);
        mesh.normals[i]=inv.rotate(mesh.normals[i]);
        const float nlen=length(mesh.normals[i]);
        if(nlen>1e-8f)mesh.normals[i]=scale(mesh.normals[i],1.f/nlen);
    }
}
} // namespace gundam_arena
