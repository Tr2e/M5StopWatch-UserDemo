#pragma once
#include "rx78.h"
#include "../../app_lets_and_go_racer/model/car_mesh_builder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace gundam_museum::sd_model {
constexpr float pi=3.14159265359f;
struct Ring {float y,w,d,z=0;};

// Small shared authoring vocabulary for the post-v5 SD exhibits. It only
// centralizes transforms and primitive topology; each suit keeps independent
// proportions, sections, attachment coordinates and review gates.
class Builder {
public:
    Mesh& m;BuildOptions options;Part part=Part::Torso;
    Point origin{};float roll=0,pitch=0,yaw=0;
    void at(Part p,Point o={},float r=0,float t=0,float a=0){part=p;origin=o;roll=r;pitch=t;yaw=a;}
    Point transform(Point p)const{
        const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        p={p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};p={p.x*cy+p.z*sy,p.y,p.z*cy-p.x*sy};
        const float c=std::cos(roll),s=std::sin(roll);
        return {origin.x+p.x*c-p.y*s,origin.y+p.x*s+p.y*c,origin.z+p.z};
    }
    void face(Point a,Point b,Point c,Point d,uint16_t color,Point outward,bool twoSided=false){
        auto n=cross(subtract(b,a),subtract(c,a));float len=std::sqrt(dot(n,n));if(len<1e-8f)return;
        if(std::abs(dot(n,subtract(d,a)))>1e-6f*len){face(a,b,c,c,color,outward,twoSided);face(a,c,d,d,color,outward,twoSided);return;}
        if(dot(n,outward)<0){if(c.x==d.x&&c.y==d.y&&c.z==d.z){std::swap(b,c);d=c;}else std::swap(b,d);}
        a=transform(a);b=transform(b);c=transform(c);d=transform(d);
        n=cross(subtract(b,a),subtract(c,a));len=std::sqrt(dot(n,n));if(len<1e-8f)return;
        if(m.count>=Mesh::capacity){m.overflowed=true;return;}n={n.x/len,n.y/len,n.z/len};
        const float light=.56f+.36f*std::max(0.f,dot(n,Point{-.46f,.65f,.60f}))+.13f*std::max(0.f,dot(n,Point{.70f,.25f,-.67f}));
        lets_and_go::mesh_parts::MeshWriter writer{{m.panels.data(),Mesh::capacity},m.count,false};
        lets_and_go::mesh_parts::Builder base{writer,12};
        base.quad(a,b,c,d,lets_and_go::mesh_parts::shade(options.gray?uint16_t(0xdedb):color,light));
        m.normals[m.count]=n;m.parts[m.count]=part;m.twoSided[m.count]=twoSided || part==Part::Head || part==Part::Shield || part==Part::Sabers || part==Part::Rifle || part==Part::Hands || part==Part::Arms || part==Part::Shoulders || part==Part::Shins;m.count=writer.count;
    }
    void box(float x,float y,float z,float w,float h,float d,uint16_t color,bool twoSided=false){
        const float l=x-w/2,r=x+w/2,b=y-h/2,t=y+h/2,f=z+d/2,k=z-d/2;
        face({l,b,f},{r,b,f},{r,t,f},{l,t,f},color,{0,0,1},twoSided);face({r,b,k},{l,b,k},{l,t,k},{r,t,k},color,{0,0,-1},twoSided);
        face({l,b,k},{l,b,f},{l,t,f},{l,t,k},color,{-1,0,0},twoSided);face({r,b,f},{r,b,k},{r,t,k},{r,t,f},color,{1,0,0},twoSided);
        face({l,t,f},{r,t,f},{r,t,k},{l,t,k},color,{0,1,0},twoSided);face({l,b,k},{r,b,k},{r,b,f},{l,b,f},color,{0,-1,0},twoSided);
    }
    void shell(std::initializer_list<Ring> rings,uint16_t color,int count=10,bool openTop=false){
        const auto p=[&](Ring r,int i){float a=2*pi*i/count;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
        for(auto it=rings.begin()+1;it!=rings.end();++it)for(int i=0;i<count;++i){auto a=*(it-1),b=*it;auto u=p(a,i),v=p(a,i+1);face(u,v,p(b,i+1),p(b,i),color,{u.x+v.x,0,u.z+v.z-2*a.z});}
        for(int end=0;end<2;++end){if(end&&openTop)continue;auto r=end?*(rings.end()-1):*rings.begin();for(int i=1;i<count-1;++i)face(p(r,0),p(r,i),p(r,i+1),p(r,i+1),color,{0,end?1.f:-1.f,0});}
    }
    void cover(std::initializer_list<Point> points,float depth,uint16_t color,float bevel=.01f,bool twoSided=false){
        Point center{};for(auto p:points){center.x+=p.x;center.y+=p.y;center.z+=p.z;}const float f=1.f/points.size();center={center.x*f,center.y*f,center.z*f};
        const auto inset=[&](Point p){return Point{center.x+(p.x-center.x)*.90f,center.y+(p.y-center.y)*.90f,center.z+(p.z-center.z)*.90f+bevel};};
        const auto back=[&](Point p){p.z-=depth;return p;};const Point fc{center.x,center.y,center.z+bevel};
        // Covers are independently layered thin shells. Retain both sides at
        // quantized grazing angles; their back/edge pixels otherwise vanish
        // even though the enclosing armor remains visible.
        (void)twoSided;
        for(size_t i=0;i<points.size();++i){auto a=points.begin()[i],b=points.begin()[(i+1)%points.size()],u=inset(a),v=inset(b);
            face(fc,u,v,v,color,{0,0,1},true);face(a,b,v,u,color,{0,0,1},true);face(a,back(a),back(b),b,color,{a.x+b.x-2*center.x,a.y+b.y-2*center.y,0},true);face(back(center),back(b),back(a),back(a),color,{0,0,-1},true);}
    }
    void tube(Point a,Point b,float r0,float r1,uint16_t color,bool hollow=false,int count=10,bool omitStart=false,bool twoSided=false){
        auto axis=subtract(b,a);const float len=std::sqrt(dot(axis,axis));axis={axis.x/len,axis.y/len,axis.z/len};
        auto u=cross(axis,std::abs(axis.y)<.9f?Point{0,1,0}:Point{1,0,0});const float ul=std::sqrt(dot(u,u));u={u.x/ul,u.y/ul,u.z/ul};const auto v=cross(axis,u);
        const auto p=[&](Point c,float r,int i){float co=std::cos(2*pi*i/count),si=std::sin(2*pi*i/count);return Point{c.x+r*(u.x*co+v.x*si),c.y+r*(u.y*co+v.y*si),c.z+r*(u.z*co+v.z*si)};};
        for(int i=0;i<count;++i){auto x=p(a,r0,i),y=p(a,r0,i+1);face(x,y,p(b,r1,i+1),p(b,r1,i),color,subtract(x,a),hollow||twoSided);
            if(!omitStart||options.keepBuriedFaces)face(a,y,x,x,color,{-axis.x,-axis.y,-axis.z},twoSided);else ++m.buriedOmitted;
            if(!hollow){face(b,p(b,r1,i),p(b,r1,i+1),p(b,r1,i+1),color,axis,twoSided);continue;}
            auto inner=Point{b.x-axis.x*std::min(.07f,len*.5f),b.y-axis.y*std::min(.07f,len*.5f),b.z-axis.z*std::min(.07f,len*.5f)};
            face(p(b,r1,i),p(b,r1,i+1),p(b,r1*.7f,i+1),p(b,r1*.7f,i),color,axis,true);face(p(b,r1*.7f,i),p(b,r1*.7f,i+1),p(inner,r1*.62f,i+1),p(inner,r1*.62f,i),color,subtract(b,p(b,r1,i)),true);}
    }
    void polyTube(std::initializer_list<Point> path,float radius,uint16_t color,int count=8){for(auto i=path.begin()+1;i!=path.end();++i)tube(*(i-1),*i,radius,radius,color,false,count);}
};
} // namespace gundam_museum::sd_model
