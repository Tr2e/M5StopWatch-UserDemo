#include "rx78.h"
#include "../../app_lets_and_go_racer/model/car_mesh_builder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace gundam_museum {
namespace {
constexpr uint16_t ivory=0xe75c,white=0xf7be,blue=0x449a,red=0xc986,yellow=0xff28;
constexpr uint16_t frame=0x7bf0,dark=0x39e7,black=0x10a3,eye=0xff4b;
constexpr float pi=3.14159265359f;
struct Ring {float y,x,z,w,d;};
class Builder {
public:
    Mesh& out;BuildOptions options;
    Part part=Part::Torso;
    Point origin{};float angle=0,legSide=0,tilt=0,turn=0,elbowBend=0,elbowRoll=0;
    static Point rotate(Point p,float roll,float pitch,float yaw){
        const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        p={p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};
        p={p.x*cy+p.z*sy,p.y,p.z*cy-p.x*sy};
        const float c=std::cos(roll),s=std::sin(roll);
        return {p.x*c-p.y*s,p.x*s+p.y*c,p.z};
    }
    Point transform(Point p) const {
        if(elbowBend || elbowRoll){p.y+=.346f;p=rotate(p,elbowRoll,elbowBend,0);p.y-=.346f;}
        p=rotate(p,angle,tilt,turn);p={origin.x+p.x,origin.y+p.y,origin.z+p.z};
        // Keep the crown at the exhibit's established height. The front
        // reference has a shorter helmet/face below it and more neck clearance.
        if(part==Part::Head)p.y=3.0452f+(p.y-3.145f)*.79f;
        if(legSide){
            // Whole limb rotates about the hip, including joint and ankle guard.
            // A slight toe-out and splay give the kit its planted display stance.
            p.x-=legSide*.235f;
            const bool lower=part==Part::Feet || part==Part::Shins || part==Part::Knees;
            const float bend=options.pose==Pose::Salute?(legSide>0?.50f:.035f):options.pose==Pose::Saber?(legSide>0?.42f:.22f):0.f;
            if(lower){p.y-=1.10f;p.z+=.045f;p=rotate(p,0,bend,0);p.y+=1.10f;p.z-=.045f;}
            p.y-=1.74f;
            const float spread=options.pose==Pose::Salute?(legSide>0?.52f:.48f):options.pose==Pose::Saber?.055f:(legSide>0?.29f:.10f);
            p=rotate(p,legSide*spread,options.pose==Pose::Saber?(legSide>0?-.65f:.12f):options.pose==Pose::Display?(legSide>0?-.08f:0.f):0.f,legSide*.18f);
            p.x+=legSide*.235f;p.y+=1.74f-(options.pose==Pose::Display?legSide*.018f:0.f);
        }
        if(options.pose==Pose::Display && part>=Part::Torso && part!=Part::Bazooka){
            p.y-=1.98f;p=rotate(p,-.075f,0,0);p.y+=1.98f;
        }
        if(options.pose==Pose::Saber){p.y-=1.75f;p=rotate(p,.55f,-.12f,0);p.y+=1.75f;}
        return p;
    }
    void at(Part p,Point o={},float a=0){part=p;origin=o;angle=a;tilt=turn=elbowBend=elbowRoll=0;}
    void face(Point a,Point b,Point c,Point d,uint16_t color,Point outward,bool buried=false) {
        if(buried && !options.keepBuriedFaces){++out.buriedOmitted;return;}
        auto n=cross(subtract(b,a),subtract(c,a));
        // Changing both width and depth between rings can warp a corner quad.
        // Store its triangles separately so each culling plane is exact.
        if(std::abs(dot(n,subtract(d,a)))>1e-6f*std::sqrt(dot(n,n))){
            face(a,b,c,c,color,outward);face(a,c,d,d,color,outward);return;
        }
        if(dot(n,outward)<0){
            if(c.x==d.x && c.y==d.y && c.z==d.z){std::swap(b,c);d=c;}
            else std::swap(b,d);
        }
        a=transform(a);b=transform(b);c=transform(c);d=transform(d);
        n=cross(subtract(b,a),subtract(c,a));
        const float length=std::sqrt(dot(n,n));if(length<1e-8f)return;
        if(out.count==out.capacity){out.overflowed=true;return;}
        n={n.x/length,n.y/length,n.z/length};
        // Directional, matte, per-face light. No per-pixel material evaluation.
        const float lighting=.56f+.36f*std::max(0.f,dot(n,Point{-.46f,.65f,.60f}))
            +.13f*std::max(0.f,dot(n,Point{.70f,.25f,-.67f}));
        lets_and_go::mesh_parts::MeshWriter writer{{out.panels.data(),out.capacity},out.count,false};
        lets_and_go::mesh_parts::Builder base{writer,12};
        base.quad(a,b,c,d,lets_and_go::mesh_parts::shade(options.gray?ivory:color,lighting));
        out.normals[out.count]=n;out.parts[out.count]=part;out.twoSided[out.count]=false;out.count=writer.count;
    }
    void triangle(Point a,Point b,Point c,uint16_t color,Point n){
        const auto first=out.count;face(a,b,c,c,color,n);
        // Small surface markings/recess backs are deliberately two-sided.
        // They are not closed armour shells, and must not use solid culling.
        for(auto i=first;i<out.count;++i)out.twoSided[i]=true;
    }
    void patch(Point a,Point b,Point c,Point d,uint16_t color,Point n){
        const auto first=out.count;face(a,b,c,d,color,n);
        // Surface inserts have no rear shell of their own.
        for(auto i=first;i<out.count;++i)out.twoSided[i]=true;
    }
    void box(float x,float y,float z,float w,float h,float d,uint16_t color,bool buriedTop=false,bool buriedBottom=false){
        const float l=x-w/2,r=x+w/2,b=y-h/2,t=y+h/2,f=z+d/2,k=z-d/2;
        face({l,b,f},{r,b,f},{r,t,f},{l,t,f},color,{0,0,1});
        face({r,b,k},{l,b,k},{l,t,k},{r,t,k},color,{0,0,-1});
        face({l,b,k},{l,b,f},{l,t,f},{l,t,k},color,{-1,0,0});
        face({r,b,f},{r,b,k},{r,t,k},{r,t,f},color,{1,0,0});
        face({l,t,f},{r,t,f},{r,t,k},{l,t,k},color,{0,1,0},buriedTop);
        face({l,b,k},{r,b,k},{r,b,f},{l,b,f},color,{0,-1,0},buriedBottom);
    }
    // Eight-sided chamfered armour. Each ring is planar and convex.
    void armor(std::initializer_list<Ring> rings,uint16_t color,bool buriedTop=false,bool buriedBottom=false,bool openFront=false,bool openBottom=false,bool recessedFront=false){
        const auto first=out.count;
        const auto point=[](Ring r,int i){
            constexpr float x[]={-.72f,.72f,1,1,.72f,-.72f,-1,-1};
            constexpr float z[]={1,1,.72f,-.72f,-1,-1,-.72f,.72f};
            return Point{r.x+x[i]*r.w,r.y,r.z+z[i]*r.d};
        };
        for(auto it=rings.begin()+1;it!=rings.end();++it){
            auto a=*(it-1),b=*it;
            for(int i=0;i<8;++i){auto p=point(a,i),q=point(a,(i+1)%8);
                if(openFront && (i==0 || i==1 || i==7))continue;
                const Point outward{(p.x+q.x)/2-a.x,0,(p.z+q.z)/2-a.z};
                if(recessedFront && i==0 && it-rings.begin()<=2){
                    // A6 cockpit: two recessed panels in the lower tongue.
                    // Replace the front surface, so the recess is not covered.
                    const Point corners[]={p,q,point(b,1),point(b,0)};
                    const auto sample=[&](float u,float v){
                        const float x=(p.x*(1-u)+q.x*u)*(1-v)+(point(b,0).x*(1-u)+point(b,1).x*u)*v;
                        return Point{x,a.y*(1-v)+b.y*v,(a.z+a.d)*(1-v)+(b.z+b.d)*v};};
                    const Point inset[]={sample(.13f,.10f),sample(.87f,.10f),sample(.87f,.90f),sample(.13f,.90f)};
                    for(int k=0;k<4;++k){auto r=inset[k],s=inset[(k+1)%4];
                        face(corners[k],corners[(k+1)%4],s,r,color,{0,0,1});
                        face(r,s,{s.x,s.y,s.z-.009f},{r.x,r.y,r.z-.009f},color,
                             {-(r.x+s.x)*.5f,(a.y+b.y-r.y-s.y)*.5f,0});
                    }
                    const auto back=[](Point r){r.z-=.009f;return r;};
                    face(back(inset[0]),back(inset[1]),back(inset[2]),back(inset[3]),0x3397,{0,0,1});
                    continue;
                }
                face(p,q,point(b,(i+1)%8),point(b,i),color,outward);
            }
        }
        for(int end=0;end<2;++end){
            if(!end && openBottom)continue;
            const auto r=end?*(rings.end()-1):*rings.begin();
            for(int i=1;i<7;i+=2)
                face(point(r,0),point(r,i),point(r,i+1),point(r,i+2),color,{0,end?1.f:-1.f,0},end?buriedTop:buriedBottom);
        }
        // The face aperture exposes the inside of the helmet at grazing views.
        if(openFront)for(auto i=first;i<out.count;++i)out.twoSided[i]=true;
    }
    // Rounded limb/helmet profiles have authored axial stations. They are not
    // chamfered boxes: the broad calf and dome retain curved side silhouettes.
    void rounded(std::initializer_list<Ring> rings,uint16_t color,int segments=16,bool aperture=false){
        const auto first=out.count;
        const auto point=[&](Ring r,int i){const float a=2*pi*i/segments;
            return Point{r.x+std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
        for(auto it=rings.begin()+1;it!=rings.end();++it){
            const auto a=*(it-1),b=*it;
            for(int i=0;i<segments;++i){
                if(aperture && (i<segments/8 || i>=segments-segments/8))continue;
                auto p=point(a,i),q=point(a,i+1);
                face(p,q,point(b,i+1),point(b,i),color,{(p.x+q.x)/2-a.x,0,(p.z+q.z)/2-a.z});
            }
        }
        for(int end=0;end<2;++end){auto r=end?*(rings.end()-1):*rings.begin();
            for(int i=1;i<segments-1;i+=2)
                face(point(r,0),point(r,i),point(r,i+1),point(r,i+2),color,{0,end?1.f:-1.f,0});
        }
        if(aperture)for(auto i=first;i<out.count;++i)out.twoSided[i]=true;
    }
    // Convex XY outline with a real rear shell and side walls.
    void plate(std::initializer_list<Point> outline,float thickness,uint16_t color){
        Point center{};
        for(auto p:outline){center.x+=p.x;center.y+=p.y;center.z+=p.z;}
        center.x/=outline.size();center.y/=outline.size();center.z/=outline.size();
        // Pair adjacent fan triangles where they share a plane. face() still
        // splits folded quads, preserving the authored surface and winding.
        for(size_t i=0;i<outline.size();i+=2){
            auto a=*(outline.begin()+i),b=*(outline.begin()+(i+1)%outline.size());
            auto c=i+1<outline.size()?*(outline.begin()+(i+2)%outline.size()):b;
            face(center,a,b,c,color,{0,0,1});
            face({center.x,center.y,center.z-thickness},{a.x,a.y,a.z-thickness},
                 {b.x,b.y,b.z-thickness},{c.x,c.y,c.z-thickness},color,{0,0,-1});
        }
        for(size_t i=0;i<outline.size();++i){auto a=*(outline.begin()+i),b=*(outline.begin()+(i+1)%outline.size());
            Point ar{a.x,a.y,a.z-thickness},br{b.x,b.y,b.z-thickness};
            face(a,ar,br,b,color,{(a.x+b.x)/2-center.x,(a.y+b.y)/2-center.y,0});
        }
    }
    void shoulder(float side){
        // B13 carries the depth; B14 is the inset front cover. The cover is
        // planar, with a narrow bevel instead of a triangulated bulging face.
        const Point rim[]={{-.139f,-.108f,.125f},{.074f,-.055f,.125f},
            {.15f,.005f,.125f},{.171f,.204f,.125f},{.143f,.232f,.125f},
            {-.118f,.215f,.125f},{-.147f,.185f,.125f}};
        const Point center{.004f,.065f,.125f};
        const auto p=[&](int i,float scale,float z){const auto r=rim[i%7];
            return Point{side*(center.x+(r.x-center.x)*scale),center.y+(r.y-center.y)*scale,z};};
        for(int i=0;i<7;++i){
            const auto a=p(i,1,.106f),c=p(i+1,1,.106f);
            face(a,c,p(i+1,.88f,.145f),p(i,.88f,.145f),ivory,{0,0,1});
            face(a,p(i,1,-.162f),p(i+1,1,-.162f),c,ivory,
                 {a.x-side*center.x,a.y-center.y,0});
            face({side*center.x,center.y,.145f},p(i,.88f,.145f),p(i+1,.88f,.145f),p(i+1,.88f,.145f),white,{0,0,1});
            face({side*center.x,center.y,-.162f},p(i,1,-.162f),p(i+1,1,-.162f),p(i+1,1,-.162f),ivory,{0,0,-1});
        }
    }
    void ankleGuard(){
        // B30/B31: an open-backed U, with a real inner surface around B29.
        // The rear ends meet the side hinge discs; no cap fills the ankle gap.
        constexpr float x[]={-.165f,-.165f,-.12f,.12f,.165f,.165f};
        constexpr float z[]={-.07f,.105f,.19f,.19f,.105f,-.07f};
        constexpr float ix[]={-.137f,-.137f,-.109f,.109f,.137f,.137f};
        constexpr float iz[]={-.07f,.092f,.159f,.159f,.092f,-.07f};
        const auto p=[&](int i,bool inner,bool top){return Point{
            (inner?ix[i]:x[i])*(top?1.f:1.06f),top?.346f:.228f,
            (inner?iz[i]:z[i])+(top?0:.018f)};};
        for(int i=0;i<5;++i){
            const Point n{(x[i]+x[i+1])*.5f,0,(z[i]+z[i+1])*.5f+.05f};
            face(p(i,false,false),p(i+1,false,false),p(i+1,false,true),p(i,false,true),white,n);
            face(p(i,true,true),p(i+1,true,true),p(i+1,true,false),p(i,true,false),ivory,{-n.x,0,-n.z});
            face(p(i,false,true),p(i+1,false,true),p(i+1,true,true),p(i,true,true),ivory,{0,1,0});
            face(p(i,true,false),p(i+1,true,false),p(i+1,false,false),p(i,false,false),ivory,{0,-1,0});
        }
        for(int i:{0,5})face(p(i,false,false),p(i,false,true),p(i,true,true),p(i,true,false),ivory,{0,0,-1});
    }
    void cheek(float side){
        // B19: four short vents and the larger bottom recess. Tile the front
        // around five openings; inset floors and walls replace painted bars.
        const auto p=[&](float u,float t,float depth=0){
            const float x=(.121f+.008f*t)*(1-u)+.171f*u;
            const float y=(2.756f+.229f*t)*(1-u)+(2.775f+.220f*t)*u;
            const float z=.102f+.12f*(y-2.756f)-(x-.121f);
            return Point{side*x,y,z-depth};
        };
        constexpr float us[]={0,.22f,.76f,1};
        constexpr float ts[]={0,.055f,.175f,.275f,.33f,.455f,.51f,.635f,.69f,.815f,.87f,1};
        for(int t=0;t<11;++t)for(int u=0;u<3;++u){
            // The two uninterrupted border strips are planar. Emit each
            // once rather than slicing it at every vent edge.
            if(u!=1){
                if(t==0)face(p(us[u],0),p(us[u+1],0),p(us[u+1],1),p(us[u],1),ivory,{side,0,1});
                continue;
            }
            const bool hole=u==1 && t>0 && t<10 && t%2==1;
            const float depth=hole?.009f:0;
            face(p(us[u],ts[t],depth),p(us[u+1],ts[t],depth),
                 p(us[u+1],ts[t+1],depth),p(us[u],ts[t+1],depth),hole?dark:ivory,{side,0,1});
            if(!hole)continue;
            const Point corners[]={p(us[u],ts[t]),p(us[u+1],ts[t]),p(us[u+1],ts[t+1]),p(us[u],ts[t+1])};
            const auto center=p((us[u]+us[u+1])*.5f,(ts[t]+ts[t+1])*.5f);
            for(int i=0;i<4;++i){auto a=corners[i],c=corners[(i+1)%4];
                face(a,c,{c.x,c.y,c.z-depth},{a.x,a.y,a.z-depth},ivory,
                     {center.x-(a.x+c.x)*.5f,center.y-(a.y+c.y)*.5f,0});
            }
        }
        face(p(0,0,.120f),p(1,0,.120f),p(1,1,.120f),p(0,1,.120f),ivory,{-side,0,-1});
        const Point corners[]={p(0,0),p(1,0),p(1,1),p(0,1)};
        const auto center=p(.5f,.5f);
        for(int i=0;i<4;++i){auto a=corners[i],c=corners[(i+1)%4];
            face(a,{a.x,a.y,a.z-.120f},{c.x,c.y,c.z-.120f},c,ivory,
                 {(a.x+c.x)*.5f-center.x,(a.y+c.y)*.5f-center.y,0});
        }
    }
    void sole(){
        // Visible in the low action reference: three real recesses, with lip,
        // sidewalls and recessed floors. No invisible solid cap seals them.
        const Point outer[]={{-.126f,.025f,.44f},{.126f,.025f,.44f},
            {.175f,.025f,.3378f},{.175f,.025f,-.1878f},{.126f,.025f,-.29f},
            {-.126f,.025f,-.29f},{-.175f,.025f,-.1878f},{-.175f,.025f,.3378f}};
        const Point inner[]={{-.126f,.025f,.32f},{.126f,.025f,.32f},
            {.126f,.025f,.32f},{.126f,.025f,-.215f},{.126f,.025f,-.215f},
            {-.126f,.025f,-.215f},{-.126f,.025f,-.215f},{-.126f,.025f,.32f}};
        for(int i=0;i<8;++i)face(outer[i],outer[(i+1)%8],inner[(i+1)%8],inner[i],red,{0,-1,0});
        constexpr float xs[]={-.126f,-.084f,-.012f,.012f,.084f,.126f};
        constexpr float zs[]={-.215f,-.08f,.095f,.32f};
        for(int z=0;z<3;++z)for(int x=0;x<5;++x){
            if((z==0 && x>0 && x<4) || (z==2 && (x==1 || x==3)))continue;
            face({xs[x],.025f,zs[z]},{xs[x+1],.025f,zs[z]},
                 {xs[x+1],.025f,zs[z+1]},{xs[x],.025f,zs[z+1]},red,{0,-1,0});
        }
        const float holes[][4]={{-.084f,.084f,-.215f,-.08f},
            {-.084f,-.012f,.095f,.32f},{.012f,.084f,.095f,.32f}};
        for(const auto& h:holes){
            const float l=h[0],r=h[1],back=h[2],front=h[3];
            face({l,.06f,back},{r,.06f,back},{r,.06f,front},{l,.06f,front},dark,{0,-1,0});
            face({l,.025f,back},{l,.06f,back},{l,.06f,front},{l,.025f,front},red,{1,0,0});
            face({r,.025f,front},{r,.06f,front},{r,.06f,back},{r,.025f,back},red,{-1,0,0});
            face({l,.025f,back},{r,.025f,back},{r,.06f,back},{l,.06f,back},red,{0,0,1});
            face({r,.025f,front},{l,.025f,front},{l,.06f,front},{r,.06f,front},red,{0,0,-1});
        }
    }
    void tube(Point a,Point b,float radius,uint16_t color,int segments=8,bool buriedEnds=false,bool buriedStart=false){
        Point axis=subtract(b,a);float length=std::sqrt(dot(axis,axis));
        axis={axis.x/length,axis.y/length,axis.z/length};
        Point u=cross(axis,std::abs(axis.y)<.9f?Point{0,1,0}:Point{1,0,0});
        float ul=std::sqrt(dot(u,u));u={u.x/ul,u.y/ul,u.z/ul};const auto v=cross(axis,u);
        const auto p=[&](Point o,int i,float r){float c=std::cos(i*2*pi/segments)*r,s=std::sin(i*2*pi/segments)*r;
            return Point{o.x+u.x*c+v.x*s,o.y+u.y*c+v.y*s,o.z+u.z*c+v.z*s};};
        for(int i=0;i<segments;++i){
            auto x=p(a,i,radius),y=p(a,i+1,radius);
            face(x,y,p(b,i+1,radius),p(b,i,radius),color,subtract(x,a));
            if(i%2==0){
                face(a,x,y,p(a,i+2,radius),color,{-axis.x,-axis.y,-axis.z},buriedEnds || buriedStart);
                face(b,p(b,i,radius),p(b,i+1,radius),p(b,i+2,radius),color,axis,buriedEnds);
            }
        }
    }
    // Open nozzle: annular lip, visible inner wall and recessed dark back.
    void nozzle(Point center,float radius,float depth,uint16_t color){
        const auto p=[&](int i,float r,float z){float a=i*2*pi/10;
            return Point{center.x+std::cos(a)*r,center.y+std::sin(a)*r,center.z+z};};
        for(int i=0;i<10;++i){
            face(p(i,radius,0),p(i+1,radius,0),p(i+1,radius*.69f,0),p(i,radius*.69f,0),color,{0,0,-1});
            auto a=p(i,radius*.69f,0),b=p(i+1,radius*.69f,0);
            face(a,b,p(i+1,radius*.52f,depth),p(i,radius*.52f,depth),dark,{center.x-a.x,center.y-a.y,0});
            triangle({center.x,center.y,center.z+depth},p(i,radius*.52f,depth),p(i+1,radius*.52f,depth),black,{0,0,-1});
            face(p(i,radius,0),p(i,radius*.78f,depth),p(i+1,radius*.78f,depth),p(i+1,radius,0),color,{std::cos(i*2*pi/10),std::sin(i*2*pi/10),0});
            // Close the rear annulus: the lower lip extends below the backpack
            // and exposes this mounting face from a low frontal camera.
            face(p(i,radius*.78f,depth),p(i,radius*.52f,depth),p(i+1,radius*.52f,depth),p(i+1,radius*.78f,depth),color,{0,0,1});
        }
    }
};

void legs(Builder& b){
    for(float side:{-1.f,1.f}){
        const float x=side*.235f;b.legSide=side;
        b.at(Part::Feet,{x,0,.035f});
        b.armor({{.025f,0,.075f,.175f,.365f},{.105f,0,.075f,.18f,.365f},{.185f,0,.025f,.155f,.315f}},red,false,false,false,true);
        b.sole();
        b.armor({{.185f,0,-.032f,.144f,.218f},{.245f,0,-.09f,.132f,.19f},{.29f,0,-.10f,.115f,.14f}},ivory);
        b.at(Part::Shins,{x,0,0});
        b.tube({0,.22f,-.06f},{0,.47f,-.06f},.08f,frame,8,true);
        // C22/C23 remains visible between the shin opening and ankle guard.
        b.box(0,.391f,.014f,.10f,.064f,.025f,dark);
        for(float v:{-1.f,1.f})b.box(v*.031f,.391f,.036f,.016f,.042f,.022f,frame);
        // The calf swells below the knee, then contracts into a narrow ankle.
        b.rounded({{.425f,0,-.035f,.130f,.15f},{.49f,0,-.035f,.112f,.13f},
                   {.57f,0,-.043f,.119f,.163f},{.70f,0,-.05f,.174f,.216f},{.80f,0,-.065f,.19f,.225f},
                   {.95f,0,-.065f,.17f,.21f},{1.055f,0,-.035f,.115f,.14f}},ivory);
        // Calf shell and continuous knee-to-shin plate are separate pieces.
        for(float v:{-1.f,1.f}){
            b.patch({v*.044f,.68f,.184f},{v*.057f,.68f,.176f},{v*.103f,.82f,.204f},{v*.080f,.82f,.215f},dark,{0,0,1});
            b.tube({v*.176f,.705f,-.05f},{v*.121f,.574f,-.043f},.0022f,frame,4);
            b.tube({v*.121f,.574f,-.043f},{v*.114f,.435f,-.02f},.0022f,frame,4);
            b.tube({v*.11f,.30f,-.06f},{v*.164f,.30f,-.06f},.076f,ivory,8);
            b.tube({v*.1645f,.30f,-.06f},{v*.167f,.30f,-.06f},.046f,frame,8);
            b.patch({v*.168f,.267f,-.087f},{v*.168f,.327f,-.027f},
                    {v*.168f,.336f,-.036f},{v*.168f,.276f,-.096f},ivory,{v,0,0});
        }
        b.ankleGuard();
        b.at(Part::Knees,{x,0,0});
        b.tube({-.135f,1.105f,-.045f},{.135f,1.105f,-.045f},.105f,frame,10);
        for(float v:{-1.f,1.f}){
            b.tube({v*.132f,1.105f,-.045f},{v*.144f,1.105f,-.045f},.101f,ivory,12);
            b.tube({v*.1445f,1.105f,-.045f},{v*.146f,1.105f,-.045f},.064f,frame,12);
            b.patch({v*.147f,1.059f,-.081f},{v*.147f,1.141f,.001f},
                    {v*.147f,1.151f,-.009f},{v*.147f,1.069f,-.091f},ivory,{v,0,0});
        }
        // B24 is one long front shell, including the flared lower shin.
        b.armor({{.425f,0,.133f,.13f,.027f},{.54f,0,.112f,.088f,.025f},
                 {.65f,0,.15f,.042f,.017f},{.80f,0,.16f,.102f,.055f},
                 {1.055f,0,.13f,.127f,.079f},{1.22f,0,.084f,.095f,.053f}},white);
        b.patch({-.015f,1.142f,.151f},{.015f,1.142f,.151f},{.015f,1.178f,.141f},{-.015f,1.178f,.141f},frame,{0,0,1});
        b.at(Part::Thighs,{x,0,0});
        b.armor({{1.19f,0,-.03f,.105f,.12f},{1.34f,0,-.015f,.137f,.153f},{1.69f,0,-.025f,.145f,.151f}},ivory);
        // The lower hip-stem cap is enclosed by B22 in every authored pose.
        b.tube({0,1.63f,-.025f},{0,1.84f,-.025f},.08f,frame,8,false,true);
    }
    b.legSide=0;
}

void body(Builder& b){
    b.at(Part::Waist);
    b.box(0,1.81f,-.035f,.44f,.23f,.31f,frame);
    b.armor({{1.89f,0,0,.315f,.20f},{2.03f,0,-.005f,.275f,.185f}},ivory);
    for(float v:{-1.f,1.f}){
        // B5 front skirts: chamfered lower corners, a sloping front, and
        // a separate projecting central codpiece instead of one apron.
        b.plate({{v*.078f,1.66f,.303f},{v*.285f,1.68f,.300f},
                 {v*.335f,1.745f,.286f},{v*.287f,1.978f,.220f},
                 {v*.081f,1.989f,.224f},{v*.071f,1.75f,.284f}},.087f,ivory);
        b.armor({{1.825f,v*.172f,.26f,.084f,.022f},{1.965f,v*.164f,.224f,.078f,.025f}},yellow);
        b.armor({{1.69f,v*.367f,-.03f,.088f,.192f},{1.94f,v*.302f,-.035f,.055f,.148f}},ivory);
        b.armor({{1.61f,v*.177f,-.212f,.155f,.065f},{1.92f,v*.148f,-.158f,.12f,.055f}},ivory);
    }
    b.armor({{1.595f,0,.243f,.058f,.049f},{1.875f,0,.173f,.068f,.05f}},ivory);
    b.armor({{1.885f,0,.195f,.068f,.032f},{2.01f,0,.166f,.077f,.032f}},red);
    b.triangle({-.054f,1.965f,.215f},{0,1.90f,.231f},{0,1.932f,.225f},yellow,{0,0,1});
    b.triangle({0,1.932f,.225f},{0,1.90f,.231f},{.054f,1.965f,.215f},yellow,{0,0,1});
    b.at(Part::Torso);
    b.armor({{2.015f,0,-.008f,.205f,.15f},{2.175f,0,-.014f,.223f,.164f}},red);
    b.armor({{2.185f,0,-.021f,.263f,.177f},{2.37f,0,-.035f,.305f,.208f}},red);
    b.box(0,2.18f,-.02f,.439f,.014f,.302f,frame);
    // Blue chest flares over the red rib cage. A forward ledge overhangs the vents.
    b.armor({{2.295f,0,-.042f,.31f,.222f},{2.49f,0,-.046f,.39f,.265f},{2.665f,0,-.08f,.322f,.212f}},blue);
    b.plate({{-.36f,2.492f,.282f},{.36f,2.492f,.282f},{.322f,2.665f,.14f},{-.322f,2.665f,.14f}},.024f,blue);
    for(float v:{-1.f,1.f}){
        b.patch({v*.167f,2.16f,.17f},{v*.17f,2.16f,.17f},{v*.22f,2.29f,.193f},{v*.215f,2.29f,.193f},dark,{0,0,1});
    }
    for(float v:{-1.f,1.f}){
        b.box(v*.255f,2.405f,.215f,.183f,.177f,.05f,dark);
        for(int k=0;k<3;++k)b.armor({{2.33f+k*.053f,v*.255f,.239f,.083f,.03f},
            {2.356f+k*.053f,v*.255f,.239f,.083f,.03f}},yellow);
    }
    // Continuous cockpit shell, with the recessed lower cover modeled in its
    // profile. Overlay planes crossing the old shell caused a serrated seam.
    b.armor({{2.04f,0,.158f,.063f,.05f},{2.17f,0,.18f,.080f,.055f},
             {2.325f,0,.20f,.096f,.06f},{2.332f,0,.259f,.104f,.061f},
             {2.435f,0,.239f,.109f,.061f},{2.50f,0,.206f,.112f,.054f}},blue,false,false,false,false,true);
    // The yellow throat guard descends into the chest, forming a deep U.
    // A flat horizontal collar loses one of this kit's strongest identifiers.
    b.plate({{-.127f,2.565f,.275f},{.127f,2.565f,.275f},
             {.175f,2.712f,.094f},{-.175f,2.712f,.094f}},.017f,yellow);
    for(float v:{-1.f,1.f}){
        b.plate({{v*.126f,2.565f,.275f},{v*.164f,2.573f,.258f},
                 {v*.23f,2.705f,.091f},{v*.175f,2.712f,.094f}},.018f,yellow);
        b.plate({{v*.175f,2.712f,.094f},{v*.23f,2.705f,.091f},
                 {v*.168f,2.754f,-.157f},{v*.106f,2.737f,-.141f}},.022f,yellow);
    }
    for(int k=0;k<4;++k){
        const float y=2.592f+k*.026f,z=.275f-(y-2.565f)*1.2313f;
        const float w=.115f+(y-2.592f)*.30f;
        b.box(0,y,z+.009f,w*2,.010f,.016f,yellow);
    }
    b.tube({0,2.64f,-.05f},{0,2.79f,-.05f},.073f,frame,8);
    b.at(Part::Backpack);
    b.armor({{2.24f,0,-.335f,.24f,.125f},{2.61f,0,-.345f,.26f,.145f},{2.65f,0,-.32f,.22f,.115f}},frame);
    for(float v:{-1.f,1.f}){
        b.tube({v*.105f,2.54f,-.477f},{v*.105f,2.54f,-.495f},.049f,frame,12);
        b.tube({v*.105f,2.54f,-.4955f},{v*.105f,2.54f,-.497f},.034f,dark,12);
    }
    for(float v:{-1.f,1.f}){
        b.nozzle({v*.143f,2.30f,-.53f},.095f,.08f,frame);
        b.at(Part::Sabers);
        b.tube({v*.205f,2.57f,-.325f},{v*.255f,3.03f,-.34f},.033f,ivory,8);
        b.tube({v*.244f,2.93f,-.337f},{v*.248f,2.968f,-.339f},.040f,white,8);
        b.at(Part::Backpack);
    }
}

void head(Builder& b){
    b.at(Part::Head);
    // Rear helmet shell leaves an actual opening for the inset face.
    b.rounded({{2.752f,0,-.067f,.153f,.12f},{2.86f,0,-.067f,.174f,.153f},
               {2.96f,0,-.065f,.173f,.16f},{3.008f,0,-.065f,.163f,.153f}},ivory,16,true);
    // Preserve the aperture ceiling at this join: its grazing pixels remain
    // visible at high-pitch head-study angles in the production raster.
    b.rounded({{3.008f,0,-.065f,.163f,.153f},{3.057f,0,-.066f,.15f,.146f},
               {3.10f,0,-.071f,.124f,.124f},{3.13f,0,-.075f,.082f,.086f},
               {3.145f,0,-.077f,.035f,.04f}},ivory,16);
    // HGUC 191 face proportions come from the frontal kit close-up. The
    // mask occupies about 0.39 of the helmet width, independently of the
    // wider eye aperture. White cheek pillars frame its tapered lower edge.
    b.box(0,2.902f,.075f,.205f,.185f,.025f,black);
    for(float v:{-1.f,1.f}){
        b.patch({v*.006f,2.983f,0.187000f},{v*.113f,2.982f,0.156000f},
                {v*.141f,3.026f,0.079000f},{v*.027f,3.024f,0.138000f},white,{0,0,1});
        // A3 red eye surround is a broad W below separate black sockets.
        b.patch({0,2.945f,0.135000f},{v*.106f,2.973f,0.104000f},
                {v*.087f,2.904f,0.125000f},{0,2.922f,0.166000f},red,{0,0,1});
        b.patch({v*.016f,2.973f,0.153000f},{v*.100f,2.974f,0.126000f},
                {v*.091f,2.935f,0.139000f},{v*.037f,2.931f,0.166000f},black,{0,0,1});
        b.patch({v*.027f,2.968f,0.159000f},{v*.073f,2.969f,0.139000f},
                {v*.069f,2.952f,0.152000f},{v*.046f,2.951f,0.168000f},eye,{0,0,1});
        b.cheek(v);
        b.plate({{v*.076f,2.756f,.132f},{v*.121f,2.769f,.10725f},
                 {v*.130f,2.904f,.1023f},{v*.077f,2.883f,.13145f}},.014f,ivory);
        b.plate({{v*.077f,2.883f,.13145f},{v*.130f,2.904f,.1023f},
                 {v*.132f,2.984f,.1012f},{v*.108f,2.972f,.1144f}},.014f,ivory);
        b.tube({v*.143f,3.032f,.04f},{v*.143f,3.032f,.072f},.015f,yellow,8);
        b.tube({v*.143f,3.032f,.0725f},{v*.143f,3.032f,.074f},.007f,dark,8);
        // B25 is a narrow folded mask, tapering around the long red chin.
        b.patch({0,2.924f,0.159000f},{v*.063f,2.904f,0.123000f},
                {v*.059f,2.837f,0.122000f},{0,2.846f,0.159000f},white,{0,0,1});
        b.patch({0,2.846f,0.159000f},{v*.059f,2.837f,0.122000f},
                {v*.037f,2.791f,0.115000f},{0,2.786f,0.147000f},white,{0,0,1});
        b.patch({v*.063f,2.904f,0.123000f},{v*.070f,2.906f,0.100000f},
                {v*.067f,2.824f,0.094000f},{v*.059f,2.837f,0.122000f},ivory,{v,0,1});
        b.patch({v*.059f,2.837f,0.122000f},{v*.067f,2.824f,0.094000f},
                {v*.037f,2.791f,0.094000f},{v*.037f,2.791f,0.115000f},ivory,{v,-1,1});
        for(int k=0;k<2;++k){
            const float y=2.884f-k*.017f;
            b.patch({0,y,0.161000f},{v*.021f,y-.007f,0.149000f},
                    {v*.021f,y-.011f,0.149000f},{0,y-.004f,0.161000f},frame,{0,0,1});
        }
    }
    b.triangle({-.022f,2.984f,.188f},{.022f,2.984f,.188f},{0,2.982f,.197f},white,{0,0,1});
    b.armor({{2.748f,0,.113f,.020f,.023f},{2.776f,0,.150f,.022f,.026f},
             {2.823f,0,.142f,.022f,.021f},{2.837f,0,.129f,.015f,.014f}},red);
    b.armor({{3.011f,0,.12f,.036f,.026f},{3.196f,0,.055f,.036f,.075f},
             {3.218f,0,0.f,.030f,.066f}},white);
    b.armor({{2.991f,0,.179f,.002f,.015f},{3.057f,0,.160f,.041f,.028f},
             {3.132f,0,.118f,.030f,.025f}},red);
    // Front and rear main-camera windows follow the crest, not the dome.
    b.patch({-.024f,3.142f,0.140000f},{.024f,3.142f,0.140000f},
            {.021f,3.189f,0.137000f},{-.021f,3.189f,0.137000f},ivory,{0,0,1});
    b.patch({-.017f,3.149f,0.141000f},{.017f,3.149f,0.141000f},
            {.015f,3.182f,0.139000f},{-.015f,3.182f,0.139000f},0x6c92,{0,0,1});
    b.patch({-.023f,3.105f,-.194f},{.023f,3.105f,-.194f},
            {.021f,3.136f,-.167f},{-.021f,3.136f,-.167f},ivory,{0,0,-1});
    b.patch({-.016f,3.112f,-.189f},{.016f,3.112f,-.189f},
            {.015f,3.130f,-.173f},{-.015f,3.130f,-.173f},0x6c92,{0,0,-1});
    for(float v:{-1.f,1.f}){
        const Point a{v*.032f,3.056f,.163f},e{v*.032f,3.090f,.162f};
        const Point c{v*.245f,3.250f,.076f},d{v*.220f,3.201f,.087f};
        const Point ar{a.x,a.y,a.z-.014f},er{e.x,e.y,e.z-.014f};
        const Point cr{c.x,c.y,c.z-.014f},dr{d.x,d.y,d.z-.014f};
        b.face(a,e,c,d,ivory,{0,0,1});b.face(ar,dr,cr,er,ivory,{0,0,-1});
        b.face(e,er,cr,c,ivory,{-v,1,0});b.face(c,cr,dr,d,ivory,{v,0,0});
        b.face(d,dr,ar,a,ivory,{v,-1,0});b.face(a,ar,er,e,ivory,{-v,0,0});
    }
}

void arms(Builder& b){
    for(float v:{-1.f,1.f}){
        const bool salute=b.options.pose==Pose::Salute,saber=b.options.pose==Pose::Saber;
        const float roll=salute?(v>0?2.18f:-.95f):saber?(v>0?1.02f:-2.12f):v*.16f;
        b.at(Part::Shoulders,{v*.485f,2.52f,-.025f},salute?(v>0?1.0f:-.12f):saber?v*.5f:0);
        b.tube({-v*.17f,0,0},{v*.10f,0,0},.093f,frame,10);
        b.shoulder(v);
        b.at(Part::Arms,{v*.513f,2.43f,-.01f},roll);
        b.tilt=salute?-.13f:saber?-.04f:0;
        b.tube({0,-.10f,0},{0,.09f,0},.069f,frame,8,false,true);
        b.armor({{-.295f,0,0,.10f,.111f},{-.055f,0,0,.113f,.12f}},ivory);
        b.tube({-.117f,-.346f,0},{.117f,-.346f,0},.093f,frame,8);
        for(float q:{-1.f,1.f}){
            b.tube({q*.116f,-.346f,0},{q*.125f,-.346f,0},.083f,ivory,10);
            b.tube({q*.1255f,-.346f,0},{q*.127f,-.346f,0},.056f,frame,10);
            b.patch({q*.128f,-.384f,-.029f},{q*.128f,-.317f,.038f},
                    {q*.128f,-.308f,.029f},{q*.128f,-.375f,-.038f},ivory,{q,0,0});
        }
        b.elbowBend=salute?-.1f:saber?-.07f:-.25f;
        b.elbowRoll=salute?(v>0?2.04f:-.15f):0;
        // B15/B16 front cover stops below the exposed elbow mechanism.
        b.armor({{-.757f,0,.012f,.102f,.104f},{-.69f,0,.013f,.111f,.113f},
                 {-.48f,0,.003f,.137f,.133f},{-.407f,0,0,.105f,.099f}},ivory);
        b.plate({{-.073f,-.707f,.128f},{.073f,-.707f,.128f},{.093f,-.667f,.131f},
                 {.109f,-.488f,.144425f},{.08f,-.451f,.1472f},{-.08f,-.451f,.1472f},
                 {-.109f,-.488f,.144425f},{-.093f,-.667f,.131f}},.012f,white);
        b.tube({0,-.825f,.01f},{0,-.716f,.01f},.063f,frame,8);
        b.part=Part::Hands;
        b.armor({{-.916f,0,.029f,.09f,.074f},{-.802f,0,.015f,.10f,.085f}},frame);
        const bool openHand=(salute || saber) && v<0;
        if(openHand){
            for(int k=0;k<4;++k){
                const float x=-.069f+k*.046f;
                b.tube({x,-.865f,.07f},{x*1.3f,-.987f,.08f},.018f,frame,6);
                b.tube({x*1.3f,-.987f,.08f},{x*1.5f,-1.04f+std::abs(x)*.5f,.12f},.016f,frame,6);
            }
            b.tube({-.087f,-.831f,.06f},{-.15f,-.89f,.077f},.026f,frame,8);
        }else{
            for(int k=0;k<4;++k)b.armor({{-.923f,-.069f+k*.046f,.092f,.019f,.018f},
                {-.837f,-.069f+k*.046f,.079f,.019f,.02f}},frame);
            b.tube({-v*.084f,-.829f,.056f},{-v*.108f,-.886f,.085f},.031f,frame,8);
        }
        if(saber && v>0){
            b.part=Part::Sabers;
            b.tube({0,-.795f,.092f},{0,-.99f,.092f},.033f,ivory,10);
            b.tube({0,-.99f,.092f},{0,-2.005f,.092f},.019f,0xfa36,10);
        }
    }
    b.at(Part::Torso);
}

void equipment(Builder& b){
    b.at(Part::Rifle,{-.674f,1.59f,.16f},-.48f);b.tilt=-.36f;
    // C33/C34 receiver silhouette, separate pistol grip, stock and round sight.
    b.plate({{-.075f,-.30f,.16f},{.085f,-.22f,.16f},{.09f,.20f,.16f},
             {.048f,.29f,.16f},{-.093f,.22f,.16f},{-.117f,-.15f,.16f}},.105f,frame);
    b.plate({{-.014f,-.10f,.166f},{.024f,-.055f,.166f},{.026f,.18f,.166f},{-.016f,.155f,.166f}},.01f,dark);
    b.box(-.069f,.023f,.173f,.019f,.307f,.014f,frame);
    for(int k=0;k<3;++k)b.box(.065f,-.095f+k*.09f,.173f,.02f,.012f,.012f,dark);
    b.box(.15f,.055f,.08f,.18f,.082f,.075f,dark);
    b.plate({{.04f,.255f,.13f},{.19f,.31f,.13f},{.16f,.39f,.13f},{-.03f,.35f,.13f}},.07f,frame);
    b.tube({0,-.22f,.10f},{0,-.43f,.10f},.051f,frame,12);
    b.tube({0,-.40f,.10f},{0,-.81f,.10f},.032f,frame,12);
    b.tube({0,-.75f,.10f},{0,-.86f,.10f},.025f,frame,12);
    b.tube({0,-.8605f,.10f},{0,-.862f,.10f},.016f,dark,12);
    b.box(-.13f,-.08f,.10f,.16f,.055f,.055f,frame);
    b.tube({-.18f,-.05f,.10f},{-.18f,-.20f,.10f},.076f,frame,16);
    b.tube({-.18f,-.201f,.10f},{-.18f,-.204f,.10f},.059f,yellow,16);
    b.plate({{.06f,-.19f,.12f},{.22f,-.20f,.12f},{.24f,-.255f,.12f},{.08f,-.235f,.12f}},.055f,frame);
    b.at(Part::Bazooka,{0,1.72f,-.405f});
    b.box(-.30f,.005f,0,.57f,.14f,.145f,frame);
    b.plate({{-.565f,.01f,-.077f},{-.33f,.01f,-.077f},{-.29f,-.34f,-.077f},{-.52f,-.34f,-.077f}},.067f,frame);
    for(int k=0;k<5;++k)b.box(-.427f,-.055f-k*.055f,-.149f,.16f,.014f,.012f,dark);
    b.tube({-.02f,0,0},{1.41f,0,0},.06f,frame,12);
    b.box(-.01f,-.118f,0,.075f,.19f,.07f,frame);
    b.at(Part::Bazooka,{1.5f,1.72f,-.405f});b.turn=-pi/2;
    b.nozzle({0,0,0},.09f,.11f,frame);
    b.at(Part::Shield,{.84f,2.02f,.21f},.04f);b.turn=-.16f;b.tilt=-.17f;
    // B7/A15: long upper-wide shield with a shallow central fold, not a box.
    b.plate({{-.21f,-.99f,.075f},{.21f,-.99f,.075f},{.28f,-.90f,.065f},
             {.32f,.03f,.070f},{.29f,.88f,.065f},{.22f,.99f,.075f},
             {-.22f,.99f,.075f},{-.29f,.88f,.065f},{-.32f,.03f,.070f},{-.28f,-.90f,.065f}},.095f,ivory);
    for(float v:{-1.f,1.f})b.plate({{0,-.918f,.132f},{v*.24f,-.88f,.091f},
        {v*.281f,.03f,.102f},{v*.252f,.84f,.086f},{v*.191f,.925f,.096f},{0,.925f,.143f}},.016f,red);
    b.box(0,.80f,.13f,.41f,.104f,.028f,ivory);
    b.box(0,.80f,.148f,.343f,.061f,.014f,frame);
    b.box(0,.80f,.157f,.289f,.033f,.007f,dark);
    const float z=.148f;
    b.plate({{-.035f,-.148f,z},{.035f,-.148f,z},{.025f,.41f,z},{0,.455f,z},{-.025f,.41f,z}},.006f,yellow);
    b.plate({{-.035f,-.148f,z},{-.024f,-.86f,z},{0,-.918f,z},{.024f,-.86f,z},{.035f,-.148f,z}},.006f,yellow);
    b.triangle({0,-.093f,z},{-.233f,-.08f,.108f},{0,-.203f,z},yellow,{0,0,1});
    b.triangle({0,-.093f,z},{0,-.203f,z},{.233f,-.08f,.108f},yellow,{0,0,1});
    b.box(0,-.148f,z,.048f,.074f,.002f,yellow);
    for(float v:{-1.f,1.f}){
        b.box(v*.20f,0,-.045f,.028f,1.66f,.028f,frame);
        b.box(v*.24f,.62f,.108f,.058f,.075f,.025f,red);
        b.tube({v*.24f,.62f,.121f},{v*.24f,.62f,.127f},.016f,dark,8);
    }
    for(int k=0;k<5;++k)b.box(0,-.75f+k*.37f,-.047f,.42f,.021f,.03f,frame);
    b.box(-.12f,-.085f,-.10f,.14f,.14f,.11f,frame);
}
} // namespace

void buildRx78(Mesh& mesh,BuildOptions options){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;
    Builder b{mesh,options};legs(b);body(b);head(b);arms(b);if(options.equipment && options.pose==Pose::Display)equipment(b);
}
} // namespace gundam_museum
