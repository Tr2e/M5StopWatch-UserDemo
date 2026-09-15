#include "nu_gundam.h"
#include "../../app_lets_and_go_racer/model/car_mesh_builder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace gundam_museum {
namespace {
// BB Senshi 387: independent SD proportions and assembly, see the SD Nu model card.
constexpr float pi=3.14159265359f;
constexpr uint16_t white=0xf7be,ivory=0xdedb,navy=0x18e7,red=0xd946;
constexpr uint16_t gold=0xfe48,frame=0x632d,black=0x18c4,green=0x3e89;
struct Ring {float y,w,d,z=0;};
class SdNuBuilder {
public:
    Mesh& m;BuildOptions options;Part part=Part::Torso;
    Point origin{};float roll=0,pitch=0,yaw=0;
    void at(Part p,Point o={},float r=0,float t=0,float a=0){part=p;origin=o;roll=r;pitch=t;yaw=a;}
    Point transform(Point p)const{
        const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        p={p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};
        p={p.x*cy+p.z*sy,p.y,p.z*cy-p.x*sy};
        const float c=std::cos(roll),s=std::sin(roll);
        Point q{origin.x+p.x*c-p.y*s,origin.y+p.x*s+p.y*c,origin.z+p.z};
        // BB387's helmet measures about 37% of body height in the near-front
        // product photographs. Resize the authored head around its neck seat.
        if(part==Part::Head){q.x*=1.08f;q.y=2.01f+(q.y-2.01f)*1.13f;q.z*=1.02f;}
        if(part==Part::Torso)q.x*=1.10f;
        return q;
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
        m.normals[m.count]=n;m.parts[m.count]=part;// The open palm is assembled from thin fingers; retain both sides of
        // its socket walls at the production raster's quantized grazing edges.
        // The collar and vent blades are thin inserts. Their grazing rims can
        // occupy a pixel even when the continuous normal points slightly away.
        m.twoSided[m.count]=insert || part==Part::Hands || part==Part::Funnels || (part==Part::Torso && color==gold);
        m.count=writer.count;
    }
    void box(float x,float y,float z,float w,float h,float d,uint16_t color){
        const float l=x-w/2,r=x+w/2,b=y-h/2,t=y+h/2,f=z+d/2,k=z-d/2;
        face({l,b,f},{r,b,f},{r,t,f},{l,t,f},color,{0,0,1});face({r,b,k},{l,b,k},{l,t,k},{r,t,k},color,{0,0,-1});
        face({l,b,k},{l,b,f},{l,t,f},{l,t,k},color,{-1,0,0});face({r,b,f},{r,b,k},{r,t,k},{r,t,f},color,{1,0,0});
        face({l,t,f},{r,t,f},{r,t,k},{l,t,k},color,{0,1,0});face({l,b,k},{r,b,k},{r,b,f},{l,b,f},color,{0,-1,0});
    }
    void shell(std::initializer_list<Ring> rings,uint16_t color,int count=8){
        const auto p=[&](Ring r,int i){
            if(count==8){constexpr float xs[]={-.72f,.72f,1,1,.72f,-.72f,-1,-1},zs[]={1,1,.70f,-.70f,-1,-1,-.70f,.70f};return Point{xs[i%8]*r.w,r.y,r.z+zs[i%8]*r.d};}
            const float a=2*pi*i/count;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
        for(auto it=rings.begin()+1;it!=rings.end();++it)for(int i=0;i<count;++i){const auto a=*(it-1),b=*it;auto u=p(a,i),v=p(a,i+1);face(u,v,p(b,i+1),p(b,i),color,{(u.x+v.x)/2,0,(u.z+v.z)/2-a.z});}
        for(int end=0;end<2;++end){auto r=end?*(rings.end()-1):*rings.begin();for(int i=1;i<count-1;++i)face(p(r,0),p(r,i),p(r,i+1),p(r,i+1),color,{0,end?1.f:-1.f,0});}
    }
    // Convex planar covers: front remains planar; bevel and side walls separate.
    void cover(std::initializer_list<Point> points,float depth,uint16_t color,float bevel=.015f){
        Point center{};for(auto p:points){center.x+=p.x;center.y+=p.y;center.z+=p.z;}
        const float f=1.f/points.size();center={center.x*f,center.y*f,center.z*f};
        const auto inset=[&](Point p){return Point{center.x+(p.x-center.x)*.90f,center.y+(p.y-center.y)*.90f,center.z+(p.z-center.z)*.90f+bevel};};
        const auto back=[&](Point p){p.z-=depth;return p;};
        const Point frontCenter{center.x,center.y,center.z+bevel};
        for(size_t i=0;i<points.size();++i){auto a=points.begin()[i],b=points.begin()[(i+1)%points.size()],u=inset(a),v=inset(b);
            face(frontCenter,u,v,v,color,{0,0,1},part==Part::Head);face(a,b,v,u,color,{0,0,1},part==Part::Head);
            face(a,back(a),back(b),b,color,{(a.x+b.x)/2-center.x,(a.y+b.y)/2-center.y,0},part==Part::Head);
            face(back(center),back(b),back(a),back(a),color,{0,0,-1},part==Part::Head);}
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

void body(SdNuBuilder& b){
    for(float s:{-1.f,1.f}){
        const float spread=s<0?.26f:.23f;
        const Point hip{s*.285f,1.02f,s<0?.07f:0};
        const float ankleX=hip.x+s*.74f*std::sin(spread);
        b.at(Part::Feet,{ankleX,0,hip.z},0,0,s*(s<0?.36f:.29f));
        // A1-11/12 over B1-7/8: long black toes with a white stepped instep.
        b.shell({{.025f,.275f,.39f,.12f},{.085f,.30f,.42f,.14f},
            {.18f,.29f,.39f,.13f},{.265f,.22f,.24f,.015f}},navy);
        b.cover({{-.14f,.30f,.14f},{.14f,.30f,.14f},{.21f,.19f,.41f},{-.21f,.19f,.41f}},.025f,white,.005f);
        b.box(0,.265f,.235f,.08f,.10f,.19f,white);
        b.cover({{-.045f,.155f,.55f},{.045f,.155f,.55f},{.045f,.037f,.57f},{-.045f,.037f,.57f}},.015f,gold,.003f);
        b.at(Part::Thighs,hip,s*spread);
        b.tube({0,.02f,0},{0,-.22f,0},.11f,.11f,frame,false,8);
        b.shell({{-.30f,.17f,.17f},{-.075f,.19f,.185f}},white);
        b.at(Part::Knees,hip,s*spread);
        b.tube({-.19f,-.33f,0},{.19f,-.33f,0},.09f,.09f,frame,false,8);
        b.cover({{-.15f,-.22f,.18f},{.15f,-.22f,.18f},{.19f,-.39f,.24f},{0,-.46f,.265f},{-.19f,-.39f,.24f}},.05f,white);
        b.at(Part::Shins,hip,s*spread);
        b.shell({{-.68f,.20f,.19f},{-.55f,.23f,.235f,-.03f},{-.43f,.17f,.18f}},white);
        b.tube({0,-.62f,0},{0,-.77f,0},.10f,.09f,frame,false,8);
        b.at(Part::Shins,{ankleX,.36f,hip.z},0,0,s*(s<0?.36f:.29f));
        // Open cuff; front band does not close through the foot.
        b.cover({{-.26f,.06f,.26f},{.26f,.06f,.26f},{.24f,-.06f,.28f},{-.24f,-.06f,.28f}},.035f,white,.006f);
        for(float side:{-1.f,1.f}){
            b.box(side*.25f,.015f,.025f,.04f,.14f,.40f,ivory);
            b.box(side*.251f,.17f,.085f,.045f,.13f,.105f,gold);
            b.box(side*.276f,.17f,.085f,.009f,.08f,.045f,navy);
        }
    }
    b.at(Part::Waist);
    b.shell({{1.0f,.28f,.21f},{1.30f,.34f,.24f},{1.38f,.30f,.215f}},frame);
    b.shell({{1.28f,.35f,.255f},{1.38f,.34f,.24f}},white);
    for(float s:{-1.f,1.f}){
        const auto p=[&](float x,float y,float dz=0){return Point{s*x,y,.25f+(1.35f-y)*.34f+dz};};
        b.cover({p(.105f,1.35f),p(.35f,1.35f),p(.47f,1.05f),p(.29f,.94f),p(.12f,1.02f)},.065f,white);
        b.cover({p(.17f,1.31f,.014f),p(.32f,1.31f,.014f),p(.35f,1.22f,.014f),p(.17f,1.22f,.014f)},.012f,frame,.003f);
        b.cover({p(.17f,1.19f,.013f),p(.35f,1.19f,.013f),p(.39f,1.07f,.013f),p(.24f,1.00f,.013f)},.013f,ivory,.004f);
        b.at(Part::Waist,{s*.37f,1.30f,-.02f},s*.34f,0,s*.33f);
        b.shell({{-.27f,.125f,.23f},{.02f,.105f,.20f}},white);
        b.at(Part::Waist,{},0,0,pi);
        b.cover({{s*.07f,1.33f,.255f},{s*.33f,1.33f,.255f},{s*.43f,1.03f,.34f},{s*.10f,.99f,.35f}},.04f,ivory);
    }
    b.at(Part::Waist);
    b.cover({{-.10f,1.37f,.26f},{.10f,1.37f,.26f},{.12f,.99f,.395f},{-.12f,.99f,.395f}},.07f,white);
    b.cover({{-.071f,1.35f,.283f},{.071f,1.35f,.283f},{.055f,1.19f,.34f},{-.055f,1.19f,.34f}},.015f,red,.007f);
    b.face({-.039f,1.31f,.307f},{0,1.27f,.326f},{.04f,1.32f,.306f},{.008f,1.285f,.32f},gold,{0,0,1},true);
    b.at(Part::Torso);
    b.shell({{1.34f,.29f,.22f},{1.53f,.33f,.265f}},navy);
    b.box(0,1.40f,.274f,.18f,.15f,.04f,red);
    // B1-10/11 shell has sloped clavicles and a projecting central breastplate.
    b.shell({{1.46f,.35f,.285f},{1.65f,.43f,.35f},{1.82f,.40f,.32f},{1.91f,.29f,.20f}},navy);
    b.cover({{-.10f,1.85f,.285f},{.10f,1.85f,.285f},{.14f,1.56f,.415f},{.075f,1.45f,.402f},{-.075f,1.45f,.402f},{-.14f,1.56f,.415f}},.10f,navy,.006f);
    b.cover({{-.15f,1.915f,.22f},{.15f,1.915f,.22f},{.10f,1.80f,.322f},{-.10f,1.80f,.322f}},.023f,white,.005f);
    for(float s:{-1.f,1.f}){
        const float l=s<0?-.395f:.18f,r=s<0?-.18f:.395f;
        b.opening({l,1.55f,.365f},{r,1.55f,.365f},{r,1.685f,.365f},{l,1.685f,.365f},.026f,gold);
        for(int k=0;k<3;++k)b.box((l+r)/2,1.578f+k*.036f,.352f,(r-l)*.79f,.022f,.02f,gold);
        b.box(s*.26f,1.795f,.317f,.055f,.055f,.034f,frame);
        if(s>0)b.box(s*.26f,1.799f,.336f,.025f,.037f,.012f,green);
    }
    b.tube({0,1.82f,0},{0,2.05f,0},.115f,.115f,frame,false,8,true);
    b.at(Part::Backpack);
    b.shell({{1.42f,.30f,.13f,-.40f},{1.87f,.32f,.17f,-.40f},{1.99f,.25f,.15f,-.40f}},navy);
    for(float s:{-1.f,1.f})for(int k=0;k<2;++k)
        b.tube({s*.19f,1.74f-k*.18f,-.51f},{s*.19f,1.72f-k*.18f,-.68f},.072f,.085f,frame,true,8);
    b.tube({-.20f,1.93f,-.50f},{-.20f,1.93f,-.67f},.145f,.145f,gold,false,12);
    b.box(-.20f,1.93f,-.674f,.048f,.26f,.015f,navy);
    b.at(Part::Sabers);
    b.tube({-.24f,1.98f,-.46f},{-.34f,2.39f,-.49f},.032f,.028f,white,false,8);
    b.tube({-.31f,2.26f,-.48f},{-.335f,2.36f,-.49f},.046f,.039f,ivory,false,8);
}

void head(SdNuBuilder& b,bool detail,NuAssembly* assembly){
    b.at(Part::Head);
    const Ring rings[]={{2.01f,.38f,.34f,-.04f},{2.12f,.50f,.44f,-.06f},
        {2.34f,.54f,.51f,-.06f},{2.54f,.53f,.50f,-.06f},{2.70f,.48f,.44f,-.07f},
        {2.82f,.37f,.35f,-.08f},{2.90f,.23f,.24f,-.09f},{2.94f,.10f,.12f,-.09f}};
    const auto p=[](Ring r,int i){float a=2*pi*i/24;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
    for(int row=0;row<7;++row)for(int i=0;i<24;++i){
        if(row<4&&(i<4||i>=20))continue;
        auto a=p(rings[row],i),c=p(rings[row],i+1);
        b.face(a,c,p(rings[row+1],i+1),p(rings[row+1],i),ivory,{a.x+c.x,0,a.z+c.z+.12f},true);
    }
    for(int i=0;i<24;++i){auto a=p(rings[7],i),c=p(rings[7],i+1);b.face(a,c,{0,2.95f,-.09f},{0,2.95f,-.09f},ivory,{0,1,0},true);}
    // A2-26/27 wraparound cheeks and an open front cavity; C2-6 is recessed.
    for(float s:{-1.f,1.f}){
        b.cover({{s*.335f,2.39f,.37f},{s*.43f,2.66f,.33f},{s*.47f,2.14f,.34f},{s*.28f,2.04f,.40f}},.065f,white,.006f);
        b.face({s*.43f,2.66f,.33f},{s*.47f,2.14f,.34f},{s*.51f,2.14f,.09f},{s*.48f,2.66f,.12f},ivory,{s,0,0},true);
        b.cover({{0,2.62f,.47f},{s*.40f,2.65f,.32f},{s*.35f,2.51f,.39f},{0,2.45f,.515f}},.035f,white,.005f);
        b.face({0,2.34f,.447f},{s*.225f,2.32f,.368f},{s*.20f,2.16f,.355f},{0,2.105f,.43f},white,{0,0,1},true);
        b.face({s*.225f,2.32f,.368f},{s*.27f,2.34f,.30f},{s*.24f,2.14f,.30f},{s*.20f,2.16f,.355f},ivory,{s,0,0},true);
        b.face({0,2.105f,.43f},{s*.20f,2.16f,.355f},{s*.18f,2.09f,.30f},{0,2.06f,.35f},ivory,{0,-1,0},true);
        if(detail){
            const auto eye=[&](float x,float y){return Point{s*x,y,.515f-.35f*x};};
            // Invert the socket's optical inset: the accepted green outline
            // stays the same size instead of becoming the old black plate.
            std::array<Point,4> opening={eye(.044f,2.429f),eye(.296f,2.489f),eye(.279f,2.389f),eye(.107f,2.355f)};
            for(auto& p:opening){p.x=s*(.1815f+(s*p.x-.1815f)/.88f);p.y=2.4155f+(p.y-2.4155f)/.88f;p.z=.515f-.35f*std::abs(p.x);}
            buildEyeSocket(b,opening,.072f,black,black,green,assembly?&assembly->eyes[s>0]:nullptr);
            // The lower socket edge returns to the mask instead of floating
            // above a separate colored cover.
            b.face(opening[3],opening[2],{s*.225f,2.32f,.368f},{0,2.34f,.447f},white,{0,0,1},true);
            b.face(opening[2],opening[1],{s*.35f,2.51f,.39f},{s*.335f,2.39f,.37f},ivory,{s,0,0},true);
            b.face({s*.35f,2.51f,.39f},{s*.40f,2.65f,.32f},{s*.43f,2.66f,.33f},{s*.335f,2.39f,.37f},ivory,{s,0,0},true);
            b.tube({s*.35f,2.705f,.268f},{s*.35f,2.705f,.325f},.052f,.06f,ivory,true,10);
            b.at(Part::Head,{s*.462f,2.41f,.285f},0,0,s*.82f);
            b.opening({-.037f,-.18f,.03f},{.037f,-.18f,.03f},{.037f,.12f,.03f},{-.037f,.12f,.03f},.045f,ivory);
            for(int k=0;k<4;++k)b.cover({{-.048f,-.15f+k*.072f,.04f},{.048f,-.15f+k*.072f,.04f},
                {.037f,-.121f+k*.072f,.018f},{-.037f,-.121f+k*.072f,.018f}},.015f,white,.002f);
            b.at(Part::Head);
            // Small black cheek vents stay outside the mask without enlarging it.
            b.opening({s*.231f,2.15f,.404f},{s*.29f,2.18f,.39f},{s*.29f,2.24f,.39f},{s*.231f,2.235f,.404f},.018f,white);
        }
    }
    b.shell({{2.015f,.05f,.05f,.34f},{2.065f,.07f,.058f,.388f},{2.17f,.048f,.04f,.416f},{2.19f,.025f,.025f,.415f}},red);
    if(detail)for(int k=0;k<3;++k)for(float s:{-1.f,1.f}){
        float y=2.295f-k*.034f;b.face({0,y,.451f},{s*.07f,y-.015f,.427f},{s*.07f,y-.023f,.427f},{0,y-.008f,.451f},frame,{0,0,1},true);
    }
    // Long dorsal camera crest is solidly attached to the cranium.
    const Point crest[]={{0,2.71f,.40f},{0,3.01f,.17f},{0,3.035f,-.11f},{0,2.94f,-.37f},{0,2.71f,-.575f}};
    for(int i=0;i<4;++i){auto a=crest[i],c=crest[i+1];
        b.face({-.09f,a.y,a.z},{.09f,a.y,a.z},{.09f,c.y,c.z},{-.09f,c.y,c.z},white,{0,.7f,(a.z+c.z)/2},true);
        for(float s:{-1.f,1.f})b.face({s*.09f,a.y,a.z},{s*.09f,c.y,c.z},{s*.09f,c.y-.21f,c.z*.86f},{s*.09f,a.y-.21f,a.z*.86f},ivory,{s,0,0},true);
    }
    b.cover({{-.059f,2.93f,.236f},{.059f,2.93f,.236f},{.065f,2.77f,.359f},{-.065f,2.77f,.359f}},.015f,green,.003f);
    b.at(Part::Head,{},0,0,pi);
    b.cover({{-.06f,2.82f,.482f},{.06f,2.82f,.482f},{.06f,2.72f,.571f},{-.06f,2.72f,.571f}},.015f,green,.003f);
    b.at(Part::Head);
    b.cover({{-.07f,2.845f,.414f},{.07f,2.845f,.414f},{.135f,2.62f,.492f},{0,2.475f,.543f},{-.135f,2.62f,.492f}},.05f,navy,.009f);
    // C1-3: four fins. Outer pair broad and swept, inner pair nearly vertical.
    for(float s:{-1.f,1.f})for(int blade=0;blade<2;++blade){
        struct Section {float x,y,z,w,d;};
        const Section outer[]={{.105f,2.60f,.46f,.19f,.085f},{.35f,2.84f,.37f,.14f,.064f},
            {.67f,3.12f,.23f,.075f,.036f},{.96f,3.37f,.12f,.020f,.015f}};
        const Section inner[]={{.19f,2.73f,.32f,.09f,.065f},{.23f,2.98f,.26f,.065f,.045f},
            {.27f,3.23f,.21f,.040f,.028f},{.29f,3.39f,.18f,.012f,.012f}};
        const Section* rows=blade?inner:outer;
        const auto vertex=[&](Section r,int k){constexpr float u[]={-1,0,1,0},v[]={0,1,0,-1};
            const float nx=blade?-.99f:-.70f,ny=blade?.12f:.71f;
            return Point{s*(r.x+nx*u[k]*r.w*.5f),r.y+ny*u[k]*r.w*.5f,r.z+v[k]*r.d*.5f};};
        for(int j=0;j<3;++j)for(int k=0;k<4;++k)b.face(vertex(rows[j],k),vertex(rows[j],(k+1)%4),vertex(rows[j+1],(k+1)%4),vertex(rows[j+1],k),gold,{0,0,k<2?1.f:-1.f},true);
        for(int end:{0,3})for(int k=0;k<4;++k){auto r=rows[end];b.face({s*r.x,r.y,r.z},vertex(r,k),vertex(r,(k+1)%4),vertex(r,(k+1)%4),gold,{end?s:-s,end?1.f:-1.f,0},true);}
    }
}

void armFrame(SdNuBuilder& b,Part part,float side){
    b.at(part,{side*.69f,1.87f,0},side*(side<0?.27f:.18f),side<0?-.12f:-.06f);
}
Point armAnchor(SdNuBuilder& b,float side,Point local){armFrame(b,Part::Hands,side);return b.transform(local);}
void forearmFrame(SdNuBuilder& b,Part part,float side){
    b.at(part,armAnchor(b,side,{0,-.35f,0}),side<0?-.58f:.18f,side<0?-.30f:-.08f);
}
Point forearmAnchor(SdNuBuilder& b,float side,Point local){forearmFrame(b,Part::Hands,side);return b.transform(local);}
void handFrame(SdNuBuilder& b,Part part,float side){
    auto wrist=forearmAnchor(b,side,{0,-.30f,side<0?.13f:0.f});
    b.at(part,wrist,side<0?-.55f:.18f,side<0?.85f:-.08f,side<0?-.06f:0.f);
}
void emblem(SdNuBuilder& b,float x,float y,float z,float size){
    // Main Amuro insignia: authored surface polygons, no invented microtext.
    b.face({x-size*.5f,y+size*.5f,z},{x-size*.5f,y-size*.5f,z},{x+size*.45f,y+size*.27f,z},{x+size*.45f,y+size*.27f,z},red,{0,0,1},true);
    b.face({x-size*.28f,y+size*.08f,z+.001f},{x-size*.28f,y-size*.18f,z+.001f},{x+size*.01f,y+size*.04f,z+.001f},{x+size*.01f,y+size*.04f,z+.001f},white,{0,0,1},true);
    b.face({x-size*.30f,y+size*.37f,z+.002f},{x+size*.55f,y-size*.32f,z+.002f},{x+size*.13f,y+size*.40f,z+.002f},{x+size*.13f,y+size*.40f,z+.002f},red,{0,0,1},true);
}
void arms(SdNuBuilder& b,bool detail){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);
        b.tube({-.14f,0,0},{.14f,0,0},.12f,.12f,frame,false,8);
        b.shell({{-.21f,.23f,.23f},{.08f,.29f,.255f},{.19f,.24f,.205f}},white);
        b.cover({{-.26f,.10f,.24f},{.19f,.13f,.24f},{.26f,.035f,.24f},{.18f,-.17f,.24f},{-.13f,-.22f,.24f},{-.24f,-.13f,.24f}},.026f,white,.005f);
        if(detail){
            b.cover({{s*.15f,.09f,.25f},{s*.25f,.02f,.25f},{s*.205f,-.06f,.25f},{s*.11f,.015f,.25f}},.008f,frame,.002f);
            if(s>0)emblem(b,-.07f,.015f,.262f,.13f);
        }
        armFrame(b,Part::Arms,s);
        b.shell({{-.33f,.11f,.14f},{-.15f,.13f,.15f}},white);
        b.tube({-.13f,-.35f,0},{.13f,-.35f,0},.09f,.09f,frame,false,8);
        forearmFrame(b,Part::Arms,s);
        b.shell({{-.235f,.14f,.17f},{-.055f,.17f,.19f}},white);
        b.cover({{-.15f,-.05f,.175f},{.15f,-.05f,.175f},{.12f,-.22f,.19f},{-.12f,-.22f,.19f}},.025f,white,.008f);
        auto cuff=forearmAnchor(b,s,{0,-.23f,0});
        handFrame(b,Part::Hands,s);auto socket=b.transform({0,0,-.11f});
        b.at(Part::Hands);b.tube(cuff,socket,.057f,.057f,frame,false,8);
        handFrame(b,Part::Hands,s);
        b.box(0,0,-.082f,.23f,.19f,.055f,navy);
        b.box(-.092f,0,0,.055f,.19f,.16f,navy);b.box(.092f,0,0,.055f,.19f,.16f,navy);
        for(int i=0;i<3;++i)b.box(-.071f+i*.071f,0,.082f,.062f,.19f,.055f,navy);
    }
}
void equipment(SdNuBuilder& b,bool detail,NuAssembly* assembly){
    // A2-24/25 rifle grip uses the palm frame, with its stock above the wrist.
    handFrame(b,Part::Rifle,-1);
    b.box(0,0,0,.075f,.25f,.065f,navy);
    b.box(0,.21f,.32f,.12f,.20f,.63f,white);
    b.box(0,.235f,.36f,.128f,.08f,.72f,navy);
    b.tube({0,.20f,.59f},{0,.20f,1.12f},.04f,.027f,white,true,8);
    b.tube({0,.20f,.88f},{0,.20f,.94f},.057f,.052f,ivory,false,8);
    b.box(0,.13f,.46f,.12f,.24f,.14f,ivory);
    b.box(0,.385f,.105f,.10f,.06f,.29f,navy);
    b.box(0,.32f,-.03f,.10f,.11f,.035f,navy);
    b.box(0,.32f,.235f,.10f,.11f,.035f,navy);
    // A2-21 shield: narrow white kite, pointed lower tip, real arm peg.
    auto anchor=forearmAnchor(b,1,{.12f,-.13f,0});
    const Point origin{anchor.x+.33f,anchor.y+.12f,anchor.z+.34f};
    b.at(Part::Shield,origin,.26f,0,.70f);auto mount=b.transform({0,0,-.16f});
    b.at(Part::Shield);b.tube(anchor,mount,.033f,.033f,frame,false,8);
    b.at(Part::Shield,origin,.26f,0,.70f);
    b.box(0,.08f,-.10f,.09f,.035f,.10f,frame);b.box(0,-.08f,-.10f,.09f,.035f,.10f,frame);
    b.box(0,0,-.16f,.09f,.19f,.03f,frame);
    b.cover({{-.19f,.65f,.045f},{.19f,.65f,.045f},{.25f,.32f,.045f},{.19f,-.43f,.045f},{0,-.78f,.045f},{-.19f,-.43f,.045f},{-.25f,.32f,.045f}},.095f,white,.01f);
    b.cover({{-.13f,.59f,.065f},{.13f,.59f,.065f},{.17f,.30f,.065f},{0,-.64f,.065f},{-.17f,.30f,.065f}},.018f,ivory,.013f);
    if(detail)emblem(b,0,.34f,.10f,.30f);
    // Stored A2-32/33 bazooka is attached behind the center backpack.
    b.at(Part::Bazooka,{.02f,1.36f,-.88f},0,0,pi);
    b.tube({0,-1.04f,0},{0,.69f,0},.075f,.092f,white,true,8);
    b.tube({0,-.92f,0},{0,-.80f,0},.12f,.12f,ivory,false,8);
    b.box(0,.46f,0,.22f,.43f,.24f,white);
    b.box(0,.26f,-.11f,.09f,.15f,.16f,frame);
    b.box(-.10f,.57f,.02f,.07f,.15f,.10f,ivory);
    b.at(Part::Bazooka);b.box(.02f,1.62f,-.65f,.09f,.07f,.22f,frame);
    // Six separate fin funnels, same upper tips and graduated lower hinges.
    // A1-13..19/B1-1..4: two unfold, four fold back. No hidden launcher rods.
    b.at(Part::Funnels);b.box(.42f,2.04f,-.64f,.20f,.14f,.25f,navy);
    b.tube({.42f,2.04f,-.75f},{.54f,2.09f,-.83f},.06f,.06f,frame,false,8);
    for(int i=0;i<6;++i){
        const float angle=.10f+i*.115f;
        const Point o{.51f+i*.215f,1.68f+i*.12f,-.85f};
        if(assembly)assembly->funnels[i].begin=b.m.count;
        b.at(Part::Funnels,o,angle);
        b.cover({{-.035f,1.23f,.04f},{.035f,1.23f,.04f},{.105f,.12f,.04f},{-.105f,.12f,.04f}},.09f,white,.006f);
        if(detail){
            b.cover({{-.020f,1.16f,.051f},{.020f,1.16f,.051f},{.043f,.26f,.051f},{-.043f,.26f,.051f}},.008f,ivory,.005f);
            b.opening({-.04f,.36f,.065f},{.04f,.36f,.065f},{.035f,.46f,.065f},{-.035f,.46f,.065f},.02f,white);
        }
        b.box(0,.015f,0,.21f,.20f,.13f,i>=4?gold:white);
        b.opening({-.072f,-.055f,.08f},{.072f,-.055f,.08f},{.072f,.086f,.08f},{-.072f,.086f,.08f},.025f,i>=4?gold:ivory);
        const bool extended=i==1||i==5;
        if(extended){
            b.cover({{-.088f,-.11f,.023f},{.088f,-.11f,.023f},{.061f,-1.10f,.023f},{-.061f,-1.10f,.023f}},.075f,navy,.005f);
            if(detail)b.box(0,-.35f,.036f,.057f,.10f,.009f,ivory);
        }else{
            b.cover({{-.030f,1.20f,-.064f},{.030f,1.20f,-.064f},{.085f,.07f,-.064f},{-.085f,.07f,-.064f}},.045f,navy,.003f);
        }
        if(assembly)assembly->funnels[i].end=b.m.count;
        // Short inter-funnel connector remains behind the visible seams.
        if(i>0){
            const float previousAngle=.10f+(i-1)*.115f;
            const Point a{.51f+(i-1)*.215f-std::sin(previousAngle)*.30f,
                1.68f+(i-1)*.12f+std::cos(previousAngle)*.30f,-.945f};
            const Point c{o.x-std::sin(angle)*.30f,o.y+std::cos(angle)*.30f,-.945f};
            b.at(Part::Funnels);b.tube(a,c,.020f,.020f,frame,false,6);
        }
    }
}
} // namespace
void buildNuGundam(Mesh& mesh,BuildOptions options,NuStage stage,NuAssembly* assembly){
    if(assembly)*assembly=NuAssembly{};
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;
    if(stage==NuStage::Blockout)options.gray=true;
    SdNuBuilder b{mesh,options};body(b);head(b,stage!=NuStage::Blockout,assembly);
    arms(b,stage==NuStage::Final);if(options.equipment)equipment(b,stage==NuStage::Final,assembly);
}
} // namespace gundam_museum
