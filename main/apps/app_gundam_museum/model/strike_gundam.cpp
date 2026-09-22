#include "strike_gundam.h"
#include "../../app_lets_and_go_racer/model/car_mesh_builder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace gundam_museum {
namespace {
// SDEX 002 Aile Strike; newly authored SD geometry, not scaled HGCE 171.
constexpr float pi=3.14159265359f;
constexpr uint16_t white=0xffff,ivory=0xef7d,blue=0x2256,red=0xd126;
constexpr uint16_t gold=0xfea6,frame=0x52aa,black=0x18c4,cameraBlue=0x3397;
struct Ring {float y,w,d,z=0;};
class SdStrikeBuilder {
public:
    Mesh& m;BuildOptions options;Part part=Part::Torso;
    Point origin{};float roll=0,pitch=0,yaw=0;
    void at(Part p,Point o={},float r=0,float t=0,float a=0){part=p;origin=o;roll=r;pitch=t;yaw=a;}
    Point transform(Point p)const{
        const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        p={p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};
        p={p.x*cy+p.z*sy,p.y,p.z*cy-p.x*sy};
        const float c=std::cos(roll),s=std::sin(roll);
        return {origin.x+p.x*c-p.y*s,origin.y+p.x*s+p.y*c,origin.z+p.z};
    }
    void face(Point a,Point b,Point c,Point d,uint16_t color,Point outward,bool insert=false){
        auto n=cross(subtract(b,a),subtract(c,a));float len=std::sqrt(dot(n,n));
        if(len<1e-8f)return;
        if(std::abs(dot(n,subtract(d,a)))>1e-6f*len){face(a,b,c,c,color,outward,insert);face(a,c,d,d,color,outward,insert);return;}
        if(dot(n,outward)<0){if(c.x==d.x&&c.y==d.y&&c.z==d.z){std::swap(b,c);d=c;}else std::swap(b,d);}
        a=transform(a);b=transform(b);c=transform(c);d=transform(d);
        n=cross(subtract(b,a),subtract(c,a));len=std::sqrt(dot(n,n));if(len<1e-8f)return;
        if(m.count>=Mesh::capacity){m.overflowed=true;return;}
        n={n.x/len,n.y/len,n.z/len};
        const float light=.56f+.36f*std::max(0.f,dot(n,Point{-.46f,.65f,.60f}))+.13f*std::max(0.f,dot(n,Point{.70f,.25f,-.67f}));
        lets_and_go::mesh_parts::MeshWriter writer{{m.panels.data(),Mesh::capacity},m.count,false};
        lets_and_go::mesh_parts::Builder base{writer,12};
        base.quad(a,b,c,d,lets_and_go::mesh_parts::shade(options.gray?ivory:color,light));
        m.normals[m.count]=n;m.anchors[m.count]=a;m.parts[m.count]=part;// The open palm is assembled from thin fingers; retain both sides of
        // its socket walls at the production raster's quantized grazing edges.
        // The collar and vent blades are thin inserts. Their grazing rims can
        // occupy a pixel even when the continuous normal points slightly away.
        m.twoSided[m.count]=insert || part==Part::Hands || part==Part::Aile || (part==Part::Torso && color==gold);
        m.count=writer.count;
    }
    void box(float x,float y,float z,float w,float h,float d,uint16_t color){
        const float l=x-w/2,r=x+w/2,b=y-h/2,t=y+h/2,f=z+d/2,k=z-d/2;
        face({l,b,f},{r,b,f},{r,t,f},{l,t,f},color,{0,0,1});face({r,b,k},{l,b,k},{l,t,k},{r,t,k},color,{0,0,-1});
        face({l,b,k},{l,b,f},{l,t,f},{l,t,k},color,{-1,0,0});face({r,b,f},{r,b,k},{r,t,k},{r,t,f},color,{1,0,0});
        face({l,t,f},{r,t,f},{r,t,k},{l,t,k},color,{0,1,0});face({l,b,k},{r,b,k},{r,b,f},{l,b,f},color,{0,-1,0});
    }
    void shell(std::initializer_list<Ring> rings,uint16_t color,int count=8,bool openTop=false){
        const auto p=[&](Ring r,int i){
            if(count==8){constexpr float xs[]={-.72f,.72f,1,1,.72f,-.72f,-1,-1},zs[]={1,1,.70f,-.70f,-1,-1,-.70f,.70f};return Point{xs[i%8]*r.w,r.y,r.z+zs[i%8]*r.d};}
            const float a=2*pi*i/count;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
        for(auto it=rings.begin()+1;it!=rings.end();++it)for(int i=0;i<count;++i){const auto a=*(it-1),b=*it;auto u=p(a,i),v=p(a,i+1);face(u,v,p(b,i+1),p(b,i),color,{(u.x+v.x)/2,0,(u.z+v.z)/2-a.z});}
        for(int end=0;end<2;++end){if(end&&openTop)continue;auto r=end?*(rings.end()-1):*rings.begin();for(int i=1;i<count-1;++i)face(p(r,0),p(r,i),p(r,i+1),p(r,i+1),color,{0,end?1.f:-1.f,0});}
    }
    // Convex planar covers: front remains planar; bevel and side walls separate.
    void cover(std::initializer_list<Point> points,float depth,uint16_t color,float bevel=.015f){
        Point center{};for(auto p:points){center.x+=p.x;center.y+=p.y;center.z+=p.z;}
        const float f=1.f/points.size();center={center.x*f,center.y*f,center.z*f};
        const auto inset=[&](Point p){return Point{center.x+(p.x-center.x)*.90f,center.y+(p.y-center.y)*.90f,center.z+(p.z-center.z)*.90f+bevel};};
        const auto back=[&](Point p){p.z-=depth;return p;};
        const Point frontCenter{center.x,center.y,center.z+bevel};
        for(size_t i=0;i<points.size();++i){auto a=points.begin()[i],b=points.begin()[(i+1)%points.size()],u=inset(a),v=inset(b);
            face(frontCenter,u,v,v,color,{0,0,1},part==Part::Head || part==Part::Arms || (part==Part::Torso && color==frame));face(a,b,v,u,color,{0,0,1},part==Part::Head || part==Part::Arms || (part==Part::Torso && color==frame));
            face(a,back(a),back(b),b,color,{(a.x+b.x)/2-center.x,(a.y+b.y)/2-center.y,0},part==Part::Head || part==Part::Arms || (part==Part::Torso && color==frame));
            face(back(center),back(b),back(a),back(a),color,{0,0,-1},part==Part::Head || part==Part::Arms || (part==Part::Torso && color==frame));}
    }
    void opening(Point a,Point b,Point c,Point d,float depth,uint16_t border){
        const Point center{(a.x+b.x+c.x+d.x)/4,(a.y+b.y+c.y+d.y)/4,(a.z+b.z+c.z+d.z)/4};
        const Point outer[]={a,b,c,d};Point inner[4],back[4];
        for(int i=0;i<4;++i){auto p=outer[i];inner[i]={center.x+(p.x-center.x)*.82f,center.y+(p.y-center.y)*.72f,center.z+(p.z-center.z)*.72f};back[i]={inner[i].x,inner[i].y,inner[i].z-depth};}
        for(int i=0;i<4;++i){int j=(i+1)%4;face(outer[i],outer[j],inner[j],inner[i],border,{0,0,1},true);face(inner[i],inner[j],back[j],back[i],frame,{center.x-inner[i].x,center.y-inner[i].y,0},true);}
        face(back[0],back[1],back[2],back[3],black,{0,0,1},true);
    }
    void tube(Point a,Point b,float r0,float r1,uint16_t color,bool hollow=false,int count=10,bool buriedStart=false){
        auto axis=subtract(b,a);const float len=std::sqrt(dot(axis,axis));axis={axis.x/len,axis.y/len,axis.z/len};
        auto u=cross(axis,std::abs(axis.y)<.9f?Point{0,1,0}:Point{1,0,0});const float ul=std::sqrt(dot(u,u));u={u.x/ul,u.y/ul,u.z/ul};const auto v=cross(axis,u);
        const auto p=[&](Point c,float r,int i){const float co=std::cos(2*pi*i/count),si=std::sin(2*pi*i/count);return Point{c.x+r*(u.x*co+v.x*si),c.y+r*(u.y*co+v.y*si),c.z+r*(u.z*co+v.z*si)};};
        for(int i=0;i<count;++i){auto x=p(a,r0,i),y=p(a,r0,i+1);face(x,y,p(b,r1,i+1),p(b,r1,i),color,subtract(x,a));
            if(!buriedStart||options.keepBuriedFaces)face(a,y,x,x,color,{-axis.x,-axis.y,-axis.z});else ++m.buriedOmitted;
            if(!hollow){face(b,p(b,r1,i),p(b,r1,i+1),p(b,r1,i+1),color,axis);continue;}
            const float recess=std::min(.08f,len*.65f);auto inner=Point{b.x-axis.x*recess,b.y-axis.y*recess,b.z-axis.z*recess};
            face(p(b,r1,i),p(b,r1,i+1),p(b,r1*.72f,i+1),p(b,r1*.72f,i),color,axis,true);
            face(p(b,r1*.72f,i),p(b,r1*.72f,i+1),p(inner,r1*.66f,i+1),p(inner,r1*.66f,i),frame,subtract(b,p(b,r1,i)),true);
            face(inner,p(inner,r1*.66f,i),p(inner,r1*.66f,i+1),p(inner,r1*.66f,i+1),black,axis,true);}
    }
};

void body(SdStrikeBuilder& b,bool detail,StrikeAssembly* assembly){
    for(float s:{-1.f,1.f}){
        const float spread=s<0?.27f:.23f;const Point hip{s*.29f,1.11f,s<0?.05f:0};
        const float ankleX=hip.x+s*.83f*std::sin(spread);
        b.at(Part::Feet,{ankleX,0,hip.z},0,0,s*(s<0?.37f:.30f));
        b.shell({{.025f,.29f,.39f,.10f},{.095f,.32f,.43f,.12f},{.19f,.28f,.38f,.13f},{.28f,.23f,.25f,.03f}},red);
        b.cover({{-.12f,.32f,.10f},{.12f,.32f,.10f},{.22f,.215f,.40f},{.13f,.20f,.44f},{-.13f,.20f,.44f},{-.22f,.215f,.40f}},.024f,white,.008f);
        b.at(Part::Thighs,hip,s*spread);
        b.tube({0,.03f,0},{0,-.22f,0},.11f,.10f,frame,false,8);
        b.shell({{-.31f,.145f,.15f},{-.20f,.20f,.18f},{-.05f,.17f,.165f}},white,12);
        b.at(Part::Knees,hip,s*spread);
        b.tube({-.19f,-.37f,0},{.19f,-.37f,0},.092f,.092f,frame,false,10);
        // A3: oval knee face with dark central inset, not an RX-78 kneecap.
        b.shell({{-.55f,.10f,.045f,.18f},{-.49f,.17f,.08f,.18f},{-.31f,.18f,.09f,.17f},{-.24f,.10f,.04f,.18f}},white,12);
        if(detail)b.cover({{-.07f,-.285f,.295f},{.07f,-.285f,.295f},{.055f,-.50f,.295f},{0,-.54f,.295f},{-.055f,-.50f,.295f}},.10f,frame,.006f);
        b.at(Part::Shins,hip,s*spread);
        b.shell({{-.75f,.20f,.19f},{-.64f,.22f,.22f,-.04f},{-.49f,.17f,.185f,-.025f}},white);
        b.tube({0,-.68f,0},{0,-.84f,0},.09f,.085f,frame,false,8);
        b.at(Part::Shins,{ankleX,.355f,hip.z},0,0,s*(s<0?.37f:.30f));
        b.cover({{-.27f,.085f,.23f},{.27f,.085f,.23f},{.29f,-.04f,.295f},{.17f,-.09f,.31f},{-.17f,-.09f,.31f},{-.29f,-.04f,.295f}},.042f,white,.006f);
        for(float side:{-1.f,1.f})b.box(side*.25f,.012f,.015f,.05f,.16f,.42f,ivory);
    }
    b.at(Part::Waist);
    b.shell({{1.06f,.29f,.22f},{1.38f,.34f,.23f},{1.43f,.30f,.22f}},frame);
    b.shell({{1.35f,.36f,.265f},{1.44f,.33f,.23f}},white);
    for(float s:{-1.f,1.f}){
        const int side=s>0;
        b.at(Part::Waist);
        const auto p=[&](float x,float y,float dz=0){return Point{s*x,y,.26f+(1.42f-y)*.32f+dz};};
        if(assembly)assembly->skirts.front[side].begin=b.m.count;
        b.cover({p(.10f,1.42f),p(.37f,1.42f),p(.48f,1.16f),p(.30f,1.02f),p(.14f,1.08f)},.07f,white);
        if(detail){
            b.cover({p(.16f,1.35f,.016f),p(.19f,1.35f,.016f),p(.21f,1.12f,.016f),p(.18f,1.13f,.016f)},.012f,frame,.003f);
            b.cover({p(.30f,1.13f,.016f),p(.44f,1.23f,.016f),p(.41f,1.16f,.016f),p(.29f,1.08f,.016f)},.012f,frame,.003f);
        }
        if(assembly)assembly->skirts.front[side].end=b.m.count;
        b.at(Part::Waist,{s*.38f,1.39f,-.04f},s*.38f,0,s*.30f);
        if(assembly)assembly->skirts.side[side].begin=b.m.count;
        b.shell({{-.31f,.105f,.23f},{-.11f,.14f,.235f},{.015f,.11f,.21f}},white);
        if(assembly)assembly->skirts.side[side].end=b.m.count;
        b.at(Part::Waist,{},0,0,pi);
        const int rearSide=s<0;
        if(assembly)assembly->skirts.rear[rearSide].begin=b.m.count;
        b.cover({{s*.06f,1.40f,.25f},{s*.32f,1.40f,.25f},{s*.40f,1.06f,.34f},{s*.08f,1.04f,.345f}},.04f,ivory);
        if(assembly)assembly->skirts.rear[rearSide].end=b.m.count;
    }
    b.at(Part::Waist);
    b.cover({{-.12f,1.45f,.275f},{.12f,1.45f,.275f},{.10f,1.03f,.409f},{0,.985f,.424f},{-.10f,1.03f,.409f}},.06f,white);
    b.cover({{-.083f,1.41f,.307f},{.083f,1.41f,.307f},{.055f,1.30f,.342f},{-.055f,1.30f,.342f}},.015f,blue,.004f);
    b.cover({{-.04f,1.255f,.365f},{.04f,1.255f,.365f},{.025f,1.10f,.415f},{-.025f,1.10f,.415f}},.013f,frame,.003f);
    b.at(Part::Torso);
    b.shell({{1.44f,.31f,.22f},{1.65f,.39f,.285f}},red);
    b.shell({{1.60f,.39f,.295f},{1.77f,.475f,.35f},{1.92f,.46f,.28f},{1.98f,.35f,.21f}},blue);
    b.cover({{-.135f,1.96f,.265f},{.135f,1.96f,.265f},{.18f,1.72f,.41f},{.12f,1.64f,.422f},{-.12f,1.64f,.422f},{-.18f,1.72f,.41f}},.095f,blue,.008f);
    b.cover({{-.10f,1.635f,.412f},{.10f,1.635f,.412f},{.075f,1.45f,.31f},{-.075f,1.45f,.31f}},.045f,red,.004f);
    if(detail){
        for(float s:{-1.f,1.f}){
            // Define one opening in inner/outer coordinates, then mirror X.
            // Both inner edges rise toward the center; reusing left/right Y
            // values on the opposite side incorrectly shears one vent.
            if(assembly)assembly->vents[s>0].begin=b.m.count;
            const Point outerBottom{s*.425f,1.70f,.409f},innerBottom{s*.205f,1.735f,.409f};
            const Point innerTop{s*.205f,1.81f,.409f},outerTop{s*.425f,1.775f,.409f};
            b.opening(outerBottom,innerBottom,innerTop,outerTop,.035f,blue);
            b.at(Part::Torso,{s*.315f,1.755f,.383f},-s*std::atan(.035f/.22f));
            b.box(0,0,0,.16f,.016f,.013f,frame);
            b.at(Part::Torso);
            b.face(outerBottom,innerBottom,{s*.205f,1.735f,.34f},{s*.425f,1.70f,.34f},blue,{0,-1,0});
            b.face(outerTop,innerTop,{s*.205f,1.81f,.32f},{s*.425f,1.775f,.32f},blue,{0,1,0});
            b.face(outerBottom,outerTop,{s*.425f,1.775f,.32f},{s*.425f,1.70f,.34f},blue,{s,0,0},true);
            b.face(innerBottom,innerTop,{s*.205f,1.81f,.32f},{s*.205f,1.735f,.34f},blue,{-s,0,0},true);
            if(assembly)assembly->vents[s>0].end=b.m.count;
            b.cover({{s*.14f,1.635f,.416f},{s*.19f,1.695f,.429f},{s*.155f,1.48f,.34f},{s*.12f,1.465f,.347f}},.018f,frame,.005f);
            b.cover({{s*.22f,1.53f,.307f},{s*.34f,1.54f,.307f},{s*.31f,1.47f,.277f},{s*.21f,1.47f,.277f}},.015f,ivory,.005f);
        }
    }
    b.shell({{1.95f,.17f,.14f},{2.025f,.19f,.15f}},frame);
    b.tube({0,1.88f,0},{0,2.095f,0},.10f,.105f,frame,false,10,true);
    b.at(Part::Backpack);
    b.box(0,1.82f,-.34f,.31f,.29f,.12f,frame);
    b.at(Part::Backpack,{},0,0,pi);
    b.opening({-.07f,1.78f,.408f},{.07f,1.78f,.408f},{.07f,1.9f,.408f},{-.07f,1.9f,.408f},.04f,frame);
}

void head(SdStrikeBuilder& b,bool detail,StrikeAssembly* assembly){
    b.at(Part::Head);
    const Ring rings[]={{2.06f,.38f,.32f,-.025f},{2.17f,.47f,.42f,-.045f},
        {2.38f,.55f,.48f,-.04f},{2.60f,.54f,.48f,-.045f},{2.77f,.49f,.42f,-.05f},
        {2.91f,.37f,.32f,-.065f},{2.99f,.23f,.22f,-.075f},{3.025f,.10f,.12f,-.08f}};
    const auto p=[](Ring r,int i){float a=2*pi*i/24;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
    for(int row=0;row<7;++row)for(int i=0;i<24;++i){
        if(row<3&&(i<4||i>=20))continue;
        auto a=p(rings[row],i),c=p(rings[row],i+1);
        b.face(a,c,p(rings[row+1],i+1),p(rings[row+1],i),ivory,{a.x+c.x,0,a.z+c.z+.08f},true);
    }
    for(int i=0;i<24;++i)b.face(p(rings[7],i),p(rings[7],i+1),{0,3.04f,-.08f},{0,3.04f,-.08f},ivory,{0,1,0},true);
    for(float s:{-1.f,1.f}){
        // Close the actual aperture side, independent of the decorative cheek
        // cover. A neck contact test cannot detect a detached face frame.
        const Point edge[]={{s*.285f,2.06f,.395f},{s*.49f,2.17f,.305f},{s*.475f,2.38f,.33f},{s*.46f,2.60f,.30f}};
        for(int row=0;row<3;++row)b.face(edge[row],p(rings[row],s>0?4:20),p(rings[row+1],s>0?4:20),edge[row+1],ivory,{s,0,0},true);
        b.cover({{s*.345f,2.49f,.382f},{s*.46f,2.71f,.30f},{s*.49f,2.17f,.305f},{s*.285f,2.075f,.395f}},.065f,white,.006f);
        b.face({s*.46f,2.71f,.30f},{s*.49f,2.17f,.305f},{s*.50f,2.17f,.11f},{s*.50f,2.65f,.115f},ivory,{s,0,0},true);
        b.cover({{0,2.675f,.47f},{s*.44f,2.72f,.295f},{s*.37f,2.56f,.385f},{0,2.50f,.53f}},.04f,white,.006f);
        b.face({0,2.40f,.457f},{s*.21f,2.37f,.365f},{s*.19f,2.18f,.35f},{0,2.12f,.435f},white,{0,0,1},true);
        b.face({s*.21f,2.37f,.365f},{s*.26f,2.39f,.30f},{s*.23f,2.16f,.30f},{s*.19f,2.18f,.35f},ivory,{s,0,0},true);
        b.face({0,2.12f,.435f},{s*.19f,2.18f,.35f},{s*.15f,2.10f,.305f},{0,2.09f,.37f},ivory,{0,-1,0},true);
        if(detail){
            // Keep the requested large SD eyes inside an actual recessed orbit.
            const auto eye=[&](float x,float y){return Point{s*x,y,.51f-.32f*x};};
            const std::array<Point,4> opening={eye(.03f,2.51f),eye(.33f,2.565f),
                eye(.295f,2.415f),eye(.085f,2.385f)};
            buildEyeSocket(b,opening,.072f,red,black,gold,assembly?&assembly->eyes[s>0]:nullptr);
            b.face(opening[1],{s*.345f,2.49f,.382f},{s*.334f,2.415f,.384f},opening[2],ivory,{s,0,1},true);
            b.face(opening[3],opening[2],{s*.21f,2.37f,.365f},{0,2.40f,.457f},white,{0,0,1},true);
            b.face({0,2.50f,.53f},opening[0],opening[3],{0,2.40f,.457f},ivory,{0,0,1},true);
            b.at(Part::Head,{s*.477f,2.575f,.28f},0,0,s*.82f);
            b.opening({-.047f,-.06f,.035f},{.047f,-.06f,.035f},{.047f,.08f,.035f},{-.047f,.08f,.035f},.035f,frame);
            b.tube({0,.015f,.025f},{0,.015f,.07f},.039f,.039f,ivory,true,8);
            b.at(Part::Head);
            b.face({s*.48f,2.46f,.25f},{s*.50f,2.47f,.20f},{s*.475f,2.39f,.23f},{s*.475f,2.39f,.23f},frame,{s,0,1},true);
        }
    }
    b.shell({{2.06f,.038f,.045f,.35f},{2.09f,.061f,.052f,.386f},{2.22f,.043f,.037f,.421f},{2.24f,.023f,.022f,.415f}},red);
    if(detail)for(int k=0;k<2;++k)for(float s:{-1.f,1.f}){float y=2.315f-k*.038f;
        b.face({0,y,.459f},{s*.055f,y-.017f,.434f},{s*.055f,y-.025f,.434f},{0,y-.008f,.459f},black,{0,0,1},true);}
    const Point crest[]={{0,2.78f,.405f},{0,3.12f,.22f},{0,3.15f,-.075f},{0,3.05f,-.345f},{0,2.79f,-.535f}};
    for(int i=0;i<4;++i){auto a=crest[i],c=crest[i+1];
        b.face({-.105f,a.y,a.z},{.105f,a.y,a.z},{.105f,c.y,c.z},{-.105f,c.y,c.z},white,{0,.7f,(a.z+c.z)/2},true);
        for(float s:{-1.f,1.f})b.face({s*.105f,a.y,a.z},{s*.105f,c.y,c.z},{s*.105f,c.y-.20f,c.z*.84f},{s*.105f,a.y-.20f,a.z*.84f},ivory,{s,0,0},true);
    }
    b.cover({{-.072f,3.07f,.253f},{.072f,3.07f,.253f},{.075f,2.85f,.373f},{-.075f,2.85f,.373f}},.013f,cameraBlue,.003f);
    b.at(Part::Head,{},0,0,pi);
    b.cover({{-.066f,2.94f,.431f},{.066f,2.94f,.431f},{.063f,2.82f,.519f},{-.063f,2.82f,.519f}},.014f,cameraBlue,.003f);
    b.at(Part::Head);
    b.cover({{-.09f,2.86f,.41f},{.09f,2.86f,.41f},{.059f,2.595f,.522f},{0,2.56f,.545f},{-.059f,2.595f,.522f}},.04f,red,.007f);
    for(float s:{-1.f,1.f})for(int blade=0;blade<2;++blade){
        struct Section {float x,y,z,w,d;};
        const Section outer[]={{.17f,2.69f,.39f,.16f,.075f},{.33f,2.86f,.36f,.135f,.058f},
            {.64f,3.13f,.235f,.082f,.033f},{.89f,3.48f,.145f,.018f,.014f}};
        const Section inner[]={{.20f,2.80f,.325f,.095f,.068f},{.25f,3.08f,.27f,.073f,.050f},
            {.31f,3.31f,.22f,.044f,.029f},{.35f,3.53f,.18f,.020f,.017f}};
        const auto* rows=blade?inner:outer;
        const auto vertex=[&](Section r,int k){constexpr float u[]={-1,0,1,0},v[]={0,1,0,-1};
            return Point{s*(r.x-(blade?.985f:.79f)*u[k]*r.w*.5f),r.y+(blade?.17f:.61f)*u[k]*r.w*.5f,r.z+v[k]*r.d*.5f};};
        for(int j=0;j<3;++j)for(int k=0;k<4;++k)b.face(vertex(rows[j],k),vertex(rows[j],(k+1)%4),vertex(rows[j+1],(k+1)%4),vertex(rows[j+1],k),blade?gold:white,{0,0,k<2?1.f:-1.f},true);
        for(int end:{0,3})for(int k=0;k<4;++k){auto r=rows[end];b.face({s*r.x,r.y,r.z},vertex(r,k),vertex(r,(k+1)%4),vertex(r,(k+1)%4),blade?gold:white,{end?s:-s,end?1.f:-1.f,0},true);}
    }
}

void armFrame(SdStrikeBuilder& b,Part part,float side){
    b.at(part,{side*.735f,1.92f,0},side*(side<0?.26f:.16f),side<0?-.12f:-.07f);
}
Point armAnchor(SdStrikeBuilder& b,float side,Point p){armFrame(b,Part::Hands,side);return b.transform(p);}
void forearmFrame(SdStrikeBuilder& b,Part part,float side){b.at(part,armAnchor(b,side,{0,-.385f,0}),side<0?-.58f:.16f,side<0?-.30f:-.08f);}
Point forearmAnchor(SdStrikeBuilder& b,float side,Point p){forearmFrame(b,Part::Hands,side);return b.transform(p);}
void handFrame(SdStrikeBuilder& b,Part part,float side){
    b.at(part,forearmAnchor(b,side,{0,side<0?-.36f:-.30f,side<0?.12f:0.f}),side<0?-.55f:.16f,side<0?.85f:-.08f,side<0?-.06f:0);
}
void arms(SdStrikeBuilder& b,bool detail,StrikeAssembly* assembly){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);
        b.tube({-.14f,0,0},{.14f,0,0},.105f,.105f,frame,false,8);
        b.shell({{-.235f,.22f,.205f},{.09f,.29f,.25f},{.17f,.245f,.23f}},white,8,detail);
        // A1 top pocket, returned walls and dark front frame.
        b.cover({{-.25f,.08f,.25f},{.23f,.115f,.25f},{.29f,-.015f,.25f},{.08f,-.26f,.25f},{-.12f,-.23f,.25f},{-.26f,-.07f,.25f}},.032f,frame,.005f);
        b.cover({{-.215f,.07f,.264f},{.20f,.10f,.264f},{.245f,-.01f,.264f},{.07f,-.205f,.264f},{-.10f,-.18f,.264f},{-.22f,-.055f,.264f}},.015f,white,.007f);
        if(detail){
            constexpr float xs[]={-.72f,.72f,1,1,.72f,-.72f,-1,-1},zs[]={1,1,.70f,-.70f,-1,-1,-.70f,.70f};
            for(int i=0;i<8;++i){int j=(i+1)%8;
                Point a{xs[i]*.245f,.17f,zs[i]*.23f},c{xs[j]*.245f,.17f,zs[j]*.23f};
                Point u{a.x*.63f,.17f,a.z*.62f},v{c.x*.63f,.17f,c.z*.62f};
                Point q=u,r=v;q.y-=.08f;r.y-=.08f;
                b.face(a,c,v,u,white,{0,1,0});b.face(u,v,r,q,frame,{-u.x,0,-u.z},true);
                b.face({0,.09f,0},q,r,r,black,{0,1,0},true);
            }
            b.tube({0,-.20f,.25f},{0,-.20f,.284f},.043f,.043f,frame,false,8);
        }
        armFrame(b,Part::Arms,s);
        b.shell({{-.345f,.105f,.12f},{-.19f,.135f,.14f}},white);
        b.tube({-.13f,-.385f,0},{.13f,-.385f,0},.085f,.085f,frame,false,8);
        forearmFrame(b,Part::Arms,s);
        b.shell({{-.20f,.145f,.16f},{-.06f,.16f,.19f},{.02f,.13f,.16f}},white);
        b.cover({{-.12f,-.05f,.215f},{.12f,-.05f,.215f},{.13f,-.18f,.215f},{.09f,-.26f,.215f},{-.12f,-.25f,.215f}},.022f,white,.006f);
        if(detail)b.cover({{-.035f,-.045f,.231f},{-.01f,-.045f,.231f},{.02f,-.12f,.231f},{.10f,-.14f,.231f},{.10f,-.16f,.231f},{-.002f,-.13f,.231f}},.008f,frame,.002f);
        auto cuff=forearmAnchor(b,s,{0,-.24f,0});handFrame(b,Part::Hands,s);auto socket=b.transform({0,0,-.11f});
        if(assembly)assembly->wrists[s>0].begin=b.m.count;
        b.at(Part::Hands);b.tube(cuff,socket,.057f,.057f,frame,false,8);
        if(assembly)assembly->wrists[s>0].end=b.m.count;
        handFrame(b,Part::Hands,s);
        if(assembly)assembly->palms[s>0].begin=b.m.count;
        b.box(0,0,-.082f,.23f,.19f,.055f,frame);
        b.box(-.092f,0,0,.055f,.19f,.16f,frame);b.box(.092f,0,0,.055f,.19f,.16f,frame);
        for(int i=0;i<3;++i)b.box(-.071f+i*.071f,0,.082f,.062f,.19f,.055f,frame);
        if(assembly)assembly->palms[s>0].end=b.m.count;
    }
}
void equipment(SdStrikeBuilder& b,bool detail,StrikeAssembly* assembly){
    handFrame(b,Part::Rifle,-1);
    b.box(0,0,0,.075f,.22f,.065f,black);
    b.box(0,.21f,.33f,.12f,.19f,.59f,frame);
    b.box(0,.24f,.37f,.14f,.08f,.66f,black);
    b.tube({0,.21f,.61f},{0,.21f,1.12f},.039f,.024f,black,true,8);
    b.box(0,.21f,.79f,.10f,.105f,.39f,frame);
    b.box(0,.21f,.80f,.054f,.115f,.36f,black);
    b.box(0,.35f,.17f,.065f,.12f,.10f,frame);
    b.tube({0,.38f,.205f},{0,.38f,.25f},.04f,.04f,black,true,8);
    b.box(0,.13f,.45f,.11f,.24f,.08f,black);
    const auto anchor=forearmAnchor(b,1,{.125f,-.13f,0});
    const Point origin{anchor.x+.26f,anchor.y+.09f,anchor.z+.27f};
    b.at(Part::Shield,origin,.28f,0,.70f);auto mount=b.transform({0,0,-.16f});
    b.at(Part::Shield);b.tube(anchor,mount,.032f,.032f,frame,false,8);
    b.at(Part::Shield,origin,.28f,0,.70f);
    b.box(0,.085f,-.10f,.10f,.035f,.10f,frame);b.box(0,-.085f,-.10f,.10f,.035f,.10f,frame);
    b.box(0,0,-.16f,.10f,.20f,.032f,frame);
    b.cover({{-.19f,.76f,.04f},{.19f,.76f,.04f},{.245f,.45f,.04f},{.16f,-.67f,.04f},{0,-.78f,.04f},{-.16f,-.67f,.04f},{-.245f,.45f,.04f}},.10f,white,.008f);
    b.cover({{-.148f,.70f,.064f},{.148f,.70f,.064f},{.199f,.43f,.064f},{.13f,-.62f,.064f},{0,-.71f,.064f},{-.13f,-.62f,.064f},{-.199f,.43f,.064f}},.017f,red,.010f);
    b.box(0,.59f,.091f,.405f,.07f,.044f,white);
    b.opening({-.086f,.425f,.095f},{.086f,.425f,.095f},{.086f,.495f,.095f},{-.086f,.495f,.095f},.021f,white);
    b.cover({{0,.17f,.101f},{.032f,.08f,.101f},{.028f,-.66f,.101f},{0,-.745f,.101f},{-.028f,-.66f,.101f},{-.032f,.08f,.101f}},.026f,gold,.005f);
    for(float side:{-1.f,1.f})b.box(side*.184f,.365f,.085f,.105f,.065f,.06f,gold);
    // D7 center frame and physical backpack plug. Wings lie in the X/Z plane.
    b.at(Part::Aile);if(assembly)assembly->aile[0].begin=b.m.count;
    b.box(0,1.84f,-.465f,.095f,.075f,.20f,frame);
    b.shell({{1.76f,.23f,.17f,-.66f},{2.06f,.28f,.22f,-.68f},{2.14f,.23f,.20f,-.69f}},0x31a6);
    b.box(0,2.145f,-.84f,.30f,.045f,.52f,frame);
    if(assembly)assembly->aile[0].end=b.m.count;
    for(float s:{-1.f,1.f}){
        const int unit=s<0?1:2;b.at(Part::Aile);
        if(assembly)assembly->aile[unit].begin=b.m.count;
        // Closed, thin prism: upper red leading edge, swept black trailing edge.
        const Point top[]={{s*.25f,2.105f,-.52f},{s*1.93f,2.35f,-.71f},
            {s*1.85f,2.33f,-.82f},{s*.55f,2.149f,-.95f},{s*.25f,2.105f,-.85f}};
        Point center{};for(auto p:top){center.x+=p.x/5;center.y+=p.y/5;center.z+=p.z/5;}
        for(int i=0;i<5;++i){auto a=top[i],c=top[(i+1)%5];auto d=a,e=c;d.y-=.035f;e.y-=.035f;auto under=center;under.y-=.035f;
            b.face(center,a,c,c,0x31a6,{0,1,0},true);b.face(under,e,d,d,0x31a6,{0,-1,0},true);
            b.face(a,d,e,c,0x31a6,{(a.x+c.x)/2-center.x,0,(a.z+c.z)/2-center.z},true);
        }
        b.face({s*.27f,2.110f,-.521f},{s*1.93f,2.355f,-.71f},{s*1.86f,2.345f,-.755f},{s*.37f,2.125f,-.625f},red,{0,1,0},true);
        b.cover({{s*.25f,2.085f,-.78f},{s*.72f,2.15f,-.89f},{s*.68f,2.143f,-1.015f},{s*.25f,2.085f,-.90f}},.032f,0x31a6,.002f);
        if(assembly)assembly->aile[unit].end=b.m.count;
        // D5/D6 pods mount lower, clear of main wings and the shoulder backs.
        b.at(Part::Aile,{s*.47f,1.58f,-.83f},0,-.24f);
        if(assembly)assembly->aile[unit+2].begin=b.m.count;
        b.shell({{-.18f,.17f,.17f},{-.11f,.23f,.24f},{.17f,.22f,.24f},{.29f,.13f,.15f}},frame,12);
        b.tube({0,-.08f,-.20f},{0,-.19f,-.31f},.10f,.11f,0x31a6,true,8);
        // Long lower vanes on the local rear plane taper in width AND depth.
        b.cover({{-.06f,.10f,-.12f},{.055f,.10f,-.12f},{.065f,-.83f,-.12f},{.02f,-.93f,-.12f},{-.04f,-.81f,-.12f}},.05f,red,.003f);
        b.cover({{s*.13f,.05f,-.04f},{s*.23f,.07f,-.04f},{s*.69f,-.50f,-.04f},{s*.62f,-.53f,-.04f}},.05f,red,.003f);
        if(detail){
            b.at(Part::Aile,{s*.70f,1.58f,-.83f},0,0,s*pi/2);
            b.opening({-.095f,-.075f,.016f},{.095f,-.075f,.016f},{.095f,.085f,.016f},{-.095f,.085f,.016f},.03f,gold);
            for(int k=0;k<3;++k)b.box(0,-.045f+k*.047f,.0f,.14f,.021f,.024f,gold);
        }
        if(assembly)assembly->aile[unit+2].end=b.m.count;
        b.at(Part::Aile);b.tube({s*.18f,1.87f,-.72f},{s*.47f,1.82f,-.85f},.06f,.06f,frame,false,8);
        b.at(Part::Sabers);b.tube({s*.235f,2.02f,-.68f},{s*.235f,2.39f,-.68f},.028f,.025f,frame,false,8);
        b.tube({s*.235f,2.25f,-.68f},{s*.235f,2.31f,-.68f},.039f,.039f,0x31a6,false,8);
    }
}
} // namespace
void buildStrikeGundam(Mesh& mesh,BuildOptions options,StrikeStage stage,StrikeAssembly* assembly){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;if(assembly)*assembly=StrikeAssembly{};
    if(stage==StrikeStage::Blockout)options.gray=true;
    SdStrikeBuilder b{mesh,options};body(b,stage==StrikeStage::Final,assembly);head(b,stage!=StrikeStage::Blockout,assembly);
    arms(b,stage==StrikeStage::Final,assembly);if(options.equipment)equipment(b,stage==StrikeStage::Final,assembly);
}
} // namespace gundam_museum
