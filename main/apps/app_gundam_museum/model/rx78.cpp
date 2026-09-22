#include "rx78.h"
#include "rx78_assembly.h"
#include "../../app_lets_and_go_racer/model/car_mesh_builder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace gundam_museum {
namespace {
// User photo proportion baseline (2026-09-14), independent of HGUC / SDEX ratios.
constexpr float pi=3.14159265359f;
constexpr uint16_t white=0xf7be,ivory=0xdedb,blue=0x229b,red=0xd946;
constexpr uint16_t gold=0xfe88,frame=0x632d,black=0x18c4;
struct Ring {float y,w,d,z=0;};
class SdBuilder {
public:
    Mesh& m;BuildOptions options;Part part=Part::Torso;
    Point origin{};float roll=0,pitch=0,yaw=0;
    void at(Part p,Point o={},float r=0,float t=0,float a=0){part=p;origin=o;roll=r;pitch=t;yaw=a;}
    Point transform(Point p)const{
        if(part==Part::Shoulders)p.z*=1.30f;
        if(part==Part::Arms)p.z*=1.20f;
        const float cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
        p={p.x,p.y*cp-p.z*sp,p.y*sp+p.z*cp};
        p={p.x*cy+p.z*sy,p.y,p.z*cy-p.x*sy};
        const float c=std::cos(roll),s=std::sin(roll);
        // Upper assembly sits on the shorter SD leg chain. Each part stays rigid.
        const float upperOffset=part>=Part::Waist?.14f:0.f;
        Point q{origin.x+p.x*c-p.y*s,origin.y+p.x*s+p.y*c,origin.z+p.z};
        // Authored part dimensions, applied before raster projection (never image warping).
        if(part==Part::Head){q.x*=.90f;q.y=3.06f+(q.y-3.06f)*1.06f;
            // Preserve face recess while giving the rear cranium genuine volume.
            if(q.z<.32f)q.z=.32f+(q.z-.32f)*1.23f;}
        if(part==Part::Torso){q.x*=1.12f;q.y=1.29f+(q.y-1.38f)*1.32f;q.z*=1.15f;}
        if(part==Part::Waist)q.z*=1.12f;
        q.y-=upperOffset;
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
        m.twoSided[m.count]=insert || part==Part::Hands || (part==Part::Torso && color==gold);
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
        // Pack adjacent cap triangles into one panel while preserving their
        // shared diagonal. The raster still receives the same two triangles,
        // but avoids a second panel prepare and a degenerate triangle call.
        for(int end=0;end<2;++end){auto r=end?*(rings.end()-1):*rings.begin();for(int i=1;i<count-1;i+=2){
            if(i+1<count-1)face(p(r,0),p(r,i),p(r,i+1),p(r,i+2),color,{0,end?1.f:-1.f,0});
            else face(p(r,0),p(r,i),p(r,i+1),p(r,i+1),color,{0,end?1.f:-1.f,0});
        }}
    }
    // Convex planar covers: front remains planar; bevel and side walls separate.
    void cover(std::initializer_list<Point> points,float depth,uint16_t color,float bevel=.015f){
        Point center{};for(auto p:points){center.x+=p.x;center.y+=p.y;center.z+=p.z;}
        const float f=1.f/points.size();center={center.x*f,center.y*f,center.z*f};
        const auto inset=[&](Point p){return Point{center.x+(p.x-center.x)*.90f,center.y+(p.y-center.y)*.90f,center.z+(p.z-center.z)*.90f+bevel};};
        const auto back=[&](Point p){p.z-=depth;return p;};
        const Point frontCenter{center.x,center.y,center.z+bevel};
        for(size_t i=0;i<points.size();i+=2){
            const auto a=points.begin()[i],b=points.begin()[(i+1)%points.size()];
            const auto u=inset(a),v=inset(b);const bool pair=i+1<points.size();
            const auto c=pair?points.begin()[(i+2)%points.size()]:b,w=inset(c);
            if(pair)face(frontCenter,u,v,w,color,{0,0,1},part==Part::Head);
            else face(frontCenter,u,v,v,color,{0,0,1},part==Part::Head);
            face(a,b,v,u,color,{0,0,1},part==Part::Head);
            face(a,back(a),back(b),b,color,{(a.x+b.x)/2-center.x,(a.y+b.y)/2-center.y,0},part==Part::Head);
            if(pair){
                face(b,c,w,v,color,{0,0,1},part==Part::Head);
                face(b,back(b),back(c),c,color,{(b.x+c.x)/2-center.x,(b.y+c.y)/2-center.y,0},part==Part::Head);
                face(back(center),back(c),back(b),back(a),color,{0,0,-1},part==Part::Head);
            }else face(back(center),back(b),back(a),back(a),color,{0,0,-1},part==Part::Head);
        }
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
        const float recess=std::min(.08f,len*.65f);const auto inner=Point{b.x-axis.x*recess,b.y-axis.y*recess,b.z-axis.z*recess};
        for(int i=0;i<count;i+=2){
            const int last=std::min(i+2,count);
            for(int side=i;side<last;++side){auto x=p(a,r0,side),y=p(a,r0,side+1);
                face(x,y,p(b,r1,side+1),p(b,r1,side),color,subtract(x,a));
                if(hollow){
                    face(p(b,r1,side),p(b,r1,side+1),p(b,r1*.72f,side+1),p(b,r1*.72f,side),color,axis,true);
                    face(p(b,r1*.72f,side),p(b,r1*.72f,side+1),p(inner,r1*.66f,side+1),p(inner,r1*.66f,side),frame,subtract(b,p(b,r1,side)),true);
                }
            }
            if(!buriedStart){
                if(last==i+2)face(a,p(a,r0,i+2),p(a,r0,i+1),p(a,r0,i),color,{-axis.x,-axis.y,-axis.z});
                else face(a,p(a,r0,i+1),p(a,r0,i),p(a,r0,i),color,{-axis.x,-axis.y,-axis.z});
            }else if(options.keepBuriedFaces)for(int cap=i;cap<last;++cap)
                face(a,p(a,r0,cap+1),p(a,r0,cap),p(a,r0,cap),color,{-axis.x,-axis.y,-axis.z});
            else m.buriedOmitted+=std::size_t(last-i);
            if(!hollow){
                if(last==i+2)face(b,p(b,r1,i),p(b,r1,i+1),p(b,r1,i+2),color,axis);
                else face(b,p(b,r1,i),p(b,r1,i+1),p(b,r1,i+1),color,axis);
            }else if(last==i+2)face(inner,p(inner,r1*.66f,i),p(inner,r1*.66f,i+1),p(inner,r1*.66f,i+2),black,axis,true);
            else face(inner,p(inner,r1*.66f,i),p(inner,r1*.66f,i+1),p(inner,r1*.66f,i+1),black,axis,true);
        }
    }
};

void body(SdBuilder& b){
    for(float s:{-1.f,1.f}){
        const float spread=s<0?.29f:.24f;
        const Point hip{s*.285f,1.02f,s<0?.085f:0};
        const float ankleX=hip.x+s*.79f*std::sin(spread);
        b.at(Part::Feet,{ankleX,0,hip.z},0,0,s*(s<0?.40f:.32f));
        b.shell({{.025f,.365f,.35f,.11f},{.09f,.385f,.38f,.13f},{.175f,.36f,.34f,.13f}},red);
        b.shell({{.175f,.36f,.25f,.045f},{.30f,.245f,.18f,-.005f}},white);
        b.cover({{-.225f,.302f,.17f},{.225f,.302f,.17f},{.32f,.177f,.315f},{-.32f,.177f,.315f}},.025f,white,.006f);
        b.at(Part::Thighs,hip,s*spread);
        b.tube({0,.02f,0},{0,-.20f,0},.10f,.10f,frame);
        b.shell({{-.30f,.16f,.155f},{-.10f,.19f,.18f},{.01f,.16f,.15f}},white);
        b.at(Part::Knees,hip,s*spread);
        b.tube({-.18f,-.33f,0},{.18f,-.33f,0},.10f,.10f,frame);
        b.cover({{-.15f,-.24f,.17f},{.15f,-.24f,.17f},{.13f,-.40f,.20f},{0,-.46f,.211f},{-.13f,-.40f,.20f}},.045f,white);
        b.at(Part::Shins,hip,s*spread);
        b.shell({{-.62f,.15f,.15f},{-.56f,.17f,.18f,-.015f},{-.45f,.18f,.17f,-.005f}},white);
        b.tube({0,-.59f,0},{0,-.77f,0},.10f,.09f,frame);
        // Open ankle cuff: three rigid walls, not a cap through the foot.
        b.at(Part::Shins,{ankleX,.36f,hip.z},0,0,s*(s<0?.40f:.32f));
        b.cover({{-.30f,.08f,.25f},{.30f,.08f,.25f},{.29f,-.065f,.28f},{-.29f,-.065f,.28f}},.065f,white);
        for(float side:{-1.f,1.f}){
            b.box(side*.295f,.008f,.015f,.055f,.15f,.44f,ivory);
            b.tube({side*.30f,.01f,0},{side*.34f,.01f,0},.082f,.082f,ivory,false,12);
        }
    }
    b.at(Part::Waist);
    b.shell({{1.04f,.29f,.21f},{1.30f,.36f,.23f},{1.39f,.31f,.20f}},frame);
    b.shell({{1.27f,.35f,.255f},{1.38f,.36f,.23f}},ivory);
    for(float s:{-1.f,1.f}){
        const auto skirt=[&](float x,float y){return Point{s*x,y,.24f+(1.35f-y)*.28f};};
        b.cover({skirt(.11f,1.34f),skirt(.37f,1.34f),skirt(.47f,1.02f),skirt(.39f,.94f),skirt(.13f,.98f)},.065f,white);
        b.cover({skirt(.16f,1.30f),skirt(.34f,1.30f),skirt(.36f,1.15f),skirt(.17f,1.15f)},.018f,gold,.03f);
        b.at(Part::Waist,{s*.37f,1.31f,0},s*.30f,0,s*.35f);
        b.shell({{-.27f,.115f,.225f},{-.03f,.10f,.21f},{.02f,.07f,.18f}},white);
        b.at(Part::Waist,{},0,0,pi);
        b.cover({{s*.06f,1.33f,.235f},{s*.35f,1.33f,.235f},{s*.41f,1.01f,.32f},{s*.09f,1.0f,.323f}},.05f,ivory);
        b.at(Part::Waist);
    }
    b.cover({{-.12f,1.34f,.255f},{.12f,1.34f,.255f},{.12f,1.0f,.36f},{-.12f,1.0f,.36f}},.06f,white);
    b.cover({{-.085f,1.32f,.288f},{.085f,1.32f,.288f},{.075f,1.16f,.338f},{-.075f,1.16f,.338f}},.014f,red,.009f);
    for(float s:{-1.f,1.f})b.face({0,1.19f,.349f},{s*.07f,1.29f,.319f},{s*.025f,1.27f,.325f},{0,1.23f,.337f},gold,{0,0,1},true);
    b.at(Part::Torso);
    b.shell({{1.38f,.30f,.18f},{1.51f,.35f,.22f},{1.62f,.39f,.245f}},red);
    b.shell({{1.48f,.36f,.27f},{1.64f,.435f,.32f},{1.77f,.43f,.30f},{1.88f,.38f,.205f}},blue);
    // Front chest is open behind each yellow vent; no old cap plugs the recess.
    b.cover({{-.13f,1.77f,.34f},{.13f,1.77f,.34f},{.16f,1.48f,.415f},{-.16f,1.48f,.415f}},.10f,blue);
    b.cover({{-.085f,1.67f,.383f},{.085f,1.67f,.383f},{.08f,1.54f,.416f},{-.08f,1.54f,.416f}},.01f,0x19f4,.008f);
    for(float s:{-1.f,1.f}){
        const float l=s<0?-.39f:.18f,r=s<0?-.18f:.39f;
        b.opening({l,1.54f,.355f},{r,1.54f,.355f},{r,1.715f,.355f},{l,1.715f,.355f},.025f,gold);
        for(int k=0;k<3;++k)b.box((l+r)/2,1.574f+k*.049f,.343f,(r-l)*.78f,.035f,.025f,gold);
        // Return the outer grille rim to the blue chest instead of floating it.
        b.face({l,1.54f,.295f},{l,1.54f,.355f},{l,1.715f,.355f},{l,1.715f,.295f},gold,{-1,0,0});
        b.face({r,1.54f,.355f},{r,1.54f,.295f},{r,1.715f,.295f},{r,1.715f,.355f},gold,{1,0,0});
        b.face({l,1.715f,.355f},{r,1.715f,.355f},{r,1.715f,.295f},{l,1.715f,.295f},gold,{0,1,0});
        b.face({l,1.54f,.295f},{r,1.54f,.295f},{r,1.54f,.355f},{l,1.54f,.355f},gold,{0,-1,0});
    }
    // B8 yellow collar wraps the neck and slopes onto the front chest.
    b.cover({{-.30f,1.90f,.20f},{.30f,1.90f,.20f},
        {.23f,1.81f,.315f},{-.23f,1.81f,.315f}},.035f,gold,.008f);
    for(float s:{-1.f,1.f})b.cover({{s*.20f,1.92f,-.035f},{s*.30f,1.92f,-.035f},
        {s*.30f,1.90f,.20f},{s*.20f,1.90f,.20f}},.035f,gold,.008f);
    b.tube({0,1.78f,0},{0,2.025f,0},.115f,.11f,frame,false,12,true);
    b.at(Part::Backpack);
    b.shell({{1.47f,.28f,.13f,-.33f},{1.83f,.31f,.15f,-.33f},{1.91f,.25f,.12f,-.33f}},frame);
    for(float s:{-1.f,1.f}){
        b.tube({s*.16f,1.59f,-.44f},{s*.16f,1.56f,-.60f},.085f,.115f,frame,true,12);
        b.tube({s*.17f,1.80f,-.45f},{s*.17f,1.80f,-.50f},.060f,.06f,frame,true);
        b.at(Part::Sabers);
        b.tube({s*.235f,1.80f,-.34f},{s*.34f,2.25f,-.37f},.039f,.035f,frame);
        b.tube({s*.315f,2.14f,-.363f},{s*.33f,2.205f,-.367f},.046f,.044f,frame);
        b.at(Part::Backpack);
    }
}

void head(SdBuilder& b,Rx78Assembly* assembly){
    b.at(Part::Head);
    // A2/A3: dome and wraparound jaw walls form a single open shell.
    const Ring rings[]={{1.99f,.40f,.30f,-.03f},{2.09f,.49f,.38f,-.03f},
        {2.23f,.52f,.435f,-.03f},{2.40f,.53f,.455f,-.03f},
        {2.60f,.51f,.43f,-.03f},{2.76f,.42f,.36f,-.04f},
        {2.85f,.30f,.275f,-.05f},{2.91f,.18f,.18f,-.06f},{2.93f,.10f,.10f,-.06f}};
    constexpr int last=sizeof(rings)/sizeof(Ring)-1;
    const auto p=[](Ring r,int i){float a=2*pi*i/24;return Point{std::sin(a)*r.w,r.y,r.z+std::cos(a)*r.d};};
    for(int row=0;row<last;++row)for(int i=0;i<24;++i){
        if(rings[row+1].y<=2.60f&&(i<4||i>=20))continue;
        auto a=p(rings[row],i),c=p(rings[row],i+1);
        b.face(a,c,p(rings[row+1],i+1),p(rings[row+1],i),ivory,{a.x+c.x,0,a.z+c.z+.06f},true);
    }
    for(int i=0;i<24;++i){auto a=p(rings[last],i),c=p(rings[last],i+1);b.face(a,c,{0,2.942f,-.06f},{0,2.942f,-.06f},ivory,{0,1,0},true);}
    // Lower rim has inward-facing lining, including the side opening returns.
    for(int i=4;i<20;++i){auto a=p(rings[0],i),c=p(rings[0],i+1);const auto q=[](Point v){return Point{v.x*.91f,v.y+.025f,-.03f+(v.z+.03f)*.91f};};
        b.face(a,c,q(c),q(a),ivory,{0,-1,0},true);
        auto u=q(a),v=q(c);b.face(u,v,{v.x,2.17f,v.z},{u.x,2.17f,u.z},frame,{-u.x,0,-u.z},true);
    }
    // The red inter-eye insert shares the actual socket/mask boundary rather
    // than covering the apertures or crossing the mask as a floating lip.
    for(float s:{-1.f,1.f}){
        // Accepted five-corner gold contour, expanded only to derive the rim.
        std::array<Point,5> opening={Point{s*.047f,2.416f,0},Point{s*.279f,2.447f,0},Point{s*.269f,2.355f,0},Point{s*.21f,2.311f,0},Point{s*.086f,2.345f,0}};
        for(auto& p:opening){p.x=s*(.1782f+(s*p.x-.1782f)/.88f);p.y=2.3748f+(p.y-2.3748f)/.88f;p.z=.49f-.25f*std::abs(p.x);}
        buildEyeSocket(b,opening,.072f,black,black,gold,assembly?&assembly->eyes[s>0]:nullptr);
        b.face(opening[0],opening[4],{0,2.315f,.433f},{0,2.430f,.50f},red,{0,0,1},true);
        b.face(opening[0],opening[1],{s*.367f,2.470f,.365f},{0,2.430f,.50f},white,{0,0,1},true);
        b.face(opening[4],opening[3],{s*.235f,2.285f,.35f},{0,2.315f,.433f},white,{0,0,1},true);
        b.face(opening[3],opening[2],{s*.31f,2.28f,.36f},{s*.235f,2.285f,.35f},ivory,{0,-1,0},true);
        b.face(opening[2],opening[1],{s*.335f,2.43f,.395f},{s*.31f,2.28f,.36f},ivory,{s,0,0},true);
        b.face(opening[1],{s*.367f,2.470f,.365f},{s*.335f,2.43f,.395f},{s*.335f,2.43f,.395f},ivory,{s,0,0},true);
        // Broad brow sweeps back to the side shell. Mask's ridge stays behind it.
        b.cover({{0,2.575f,.435f},{s*.405f,2.585f,.27f},{s*.367f,2.470f,.365f},{0,2.430f,.50f}},.04f,white,.005f);
        // Cheek front plane, wraparound facet and actual connection to rear shell.
        b.cover({{s*.335f,2.43f,.395f},{s*.435f,2.57f,.328f},{s*.45f,2.08f,.334f},{s*.295f,2.035f,.409f}},.045f,white,.008f);
        const Point a{s*.435f,2.57f,.328f},c{s*.45f,2.08f,.334f};
        b.face(a,c,{s*.49f,2.09f,.165f},{s*.475f,2.57f,.185f},ivory,{s,0,0},true);
        // A3 cheek rail uses separate crossbars and a recessed continuous cavity.
        b.at(Part::Head,{s*.466f,2.31f,.255f},0,0,s*.75f);
        b.opening({-.033f,-.20f,.025f},{.033f,-.20f,.025f},{.033f,.19f,.025f},{-.033f,.19f,.025f},.035f,ivory);
        for(int k=0;k<5;++k)b.box(0,-.165f+k*.073f,.013f,.060f,.022f,.022f,ivory);
        b.at(Part::Head);
        // Folded mask sides return into the eye/cheek aperture.
        b.face({0,2.315f,.433f},{s*.235f,2.285f,.35f},{s*.212f,2.17f,.342f},{0,2.115f,.425f},white,{0,0,1},true);
        b.face({s*.235f,2.285f,.35f},{s*.265f,2.283f,.298f},{s*.247f,2.14f,.30f},{s*.212f,2.17f,.342f},ivory,{s,0,0},true);
        b.face({0,2.115f,.425f},{s*.212f,2.17f,.342f},{s*.17f,2.105f,.307f},{0,2.085f,.365f},ivory,{0,-1,0},true);
        b.tube({s*.335f,2.67f,.248f},{s*.335f,2.67f,.31f},.047f,.052f,gold,true,12);
    }
    b.at(Part::Head);
    // Sculpted central chin, anchored inside the aperture.
    b.shell({{1.99f,.055f,.05f,.32f},{2.035f,.065f,.055f,.368f},{2.125f,.049f,.042f,.39f},{2.145f,.030f,.024f,.395f}},red);
    for(int k=0;k<2;++k)for(float s:{-1.f,1.f}){
        const float y=2.259f-k*.038f;
        b.face({0,y,.438f},{s*.054f,y-.013f,.419f},{s*.054f,y-.023f,.419f},{0,y-.01f,.438f},frame,{0,0,1},true);
    }
    // Crest follows the crown to the rear, with two camera windows.
    const Point crest[]={{0,2.68f,.43f},{0,3.025f,.24f},{0,3.06f,-.01f},
        {0,3.00f,-.23f},{0,2.85f,-.39f},{0,2.64f,-.475f}};
    for(int i=0;i<5;++i){auto a=crest[i],c=crest[i+1];
        if(i==0){
            b.opening({-.075f,a.y,a.z},{.075f,a.y,a.z},{.075f,c.y,c.z},{-.075f,c.y,c.z},.016f,white);
            const auto window=[](float x,float y){return Point{x,y,.43f-(y-2.68f)*(.19f/.345f)-.012f};};
            b.face(window(-.048f,2.785f),window(.048f,2.785f),window(.045f,2.965f),window(-.045f,2.965f),red,{0,0,1},true);
        }else if(i==4){
            b.at(Part::Head,{},0,0,pi);
            b.opening({-.075f,c.y,-c.z},{.075f,c.y,-c.z},{.075f,a.y,-a.z},{-.075f,a.y,-a.z},.016f,white);
            const auto window=[](float x,float y){return Point{x,y,.475f-(y-2.64f)*(.085f/.21f)-.012f};};
            b.face(window(-.045f,2.68f),window(.045f,2.68f),window(.045f,2.795f),window(-.045f,2.795f),red,{0,0,1},true);
            b.at(Part::Head);
        }else b.face({-.075f,a.y,a.z},{.075f,a.y,a.z},{.075f,c.y,c.z},{-.075f,c.y,c.z},white,{0,.7f,(a.z+c.z)/2},true);
        // Side walls enter the dome. A thin strip would leave a floating arch.
        for(float s:{-1.f,1.f})b.face({s*.075f,a.y,a.z},{s*.075f,c.y,c.z},
            {s*.075f,c.y-.25f,c.z*.89f},{s*.075f,a.y-.25f,a.z*.89f},ivory,{s,0,0},true);
    }
    b.cover({{-.087f,2.775f,.415f},{.087f,2.775f,.415f},
        {.123f,2.495f,.47f},{0,2.40f,.52f},{-.123f,2.495f,.47f}},.045f,red,.010f);
    // Section-built V-fin: broad wedge at the root, continuously narrowing
    // in both profile width and depth to a small rounded toy-like end cap.
    for(float s:{-1.f,1.f}){
        struct BladeSection {float x,y,z,width,depth;};
        const BladeSection sections[]={{.115f,2.585f,.425f,.205f,.09f},
            {.32f,2.70f,.365f,.165f,.068f},{.60f,2.875f,.265f,.105f,.043f},
            {.88f,3.055f,.17f,.045f,.024f},{1.00f,3.13f,.135f,.018f,.014f}};
        // The section normal lies across the blade, not along screen X/Y.
        const auto vertex=[&](BladeSection r,int k){
            constexpr float across[]={-1,0,1,0};
            constexpr float depth[]={0,1,0,-1};
            return Point{s*(r.x-.54f*across[k]*r.width*.5f),
                r.y+.84f*across[k]*r.width*.5f,r.z+depth[k]*r.depth*.5f};};
        for(int j=0;j<4;++j)for(int k=0;k<4;++k){
            int n=(k+1)%4;auto a=vertex(sections[j],k),c=vertex(sections[j],n);
            b.face(a,c,vertex(sections[j+1],n),vertex(sections[j+1],k),
                k<2?white:ivory,{0,0,k<2?1.f:-1.f},true);
        }
        for(int end:{0,4}){
            auto r=sections[end];Point center{s*r.x,r.y,r.z};
            for(int k=0;k<4;k+=2)b.face(center,vertex(r,k),vertex(r,(k+1)%4),vertex(r,(k+2)%4),
                ivory,{end?s:-s,end?1.f:-1.f,0},true);
        }
    }
}

void armFrame(SdBuilder& b,Part part,float side){
    b.at(part,{side*.72f,1.90f,0},side*(side<0?.26f:.16f),side<0?-.14f:-.08f);
}
Point armAnchor(SdBuilder& b,float side,Point local){
    armFrame(b,Part::Hands,side);auto p=b.transform(local);p.y+=.14f;return p;
}
void forearmFrame(SdBuilder& b,Part part,float side){
    const auto elbow=armAnchor(b,side,{0,-.405f,0});
    b.at(part,elbow,side<0?-.60f:.16f,side<0?-.30f:-.08f);
}
Point forearmAnchor(SdBuilder& b,float side,Point local){
    forearmFrame(b,Part::Hands,side);auto p=b.transform(local);p.y+=.14f;return p;
}
void handFrame(SdBuilder& b,Part part,float side){
    auto wrist=forearmAnchor(b,side,{0,-.350f,side<0?.14f:0.f});
    if(side<0)b.at(part,wrist,-.56f,.85f,-.06f);
    else b.at(part,wrist,.16f,-.08f);
}

void arms(SdBuilder& b){
    for(float s:{-1.f,1.f}){
        armFrame(b,Part::Shoulders,s);
        b.tube({-.13f,0,0},{.13f,0,0},.12f,.12f,frame);
        b.shell({{-.23f,.225f,.18f},{.15f,.275f,.21f},{.22f,.225f,.19f}},white);
        b.cover({{-.24f,.12f,.21f},{.24f,.12f,.21f},{.21f,-.19f,.19f},{-.19f,-.22f,.188f}},.025f,white,.009f);
        armFrame(b,Part::Arms,s);
        b.shell({{-.38f,.115f,.12f},{-.15f,.125f,.13f}},white);
        b.tube({-.125f,-.405f,0},{.125f,-.405f,0},.09f,.09f,frame);
        forearmFrame(b,Part::Arms,s);
        b.shell({{-.235f,.13f,.14f},{-.055f,.155f,.17f}},white);
        b.cover({{-.125f,-.065f,.17f},{.125f,-.065f,.17f},{.115f,-.220f,.155f},{-.115f,-.220f,.155f}},.025f,white,.007f);
        // Rear wrist pin connects the palm to the forearm; grip remains open.
        auto cuff=forearmAnchor(b,s,{0,-.240f,0});
        handFrame(b,Part::Hands,s);auto socket=b.transform({0,0,-.108f});socket.y+=.14f;
        b.at(Part::Hands);b.tube(cuff,socket,.06f,.058f,frame,false,12);
        handFrame(b,Part::Hands,s);
        b.box(0,0,-.081f,.23f,.19f,.055f,frame);
        b.box(-.092f,0,0,.055f,.19f,.16f,frame);
        b.box(.092f,0,0,.055f,.19f,.16f,frame);
        for(int i=0;i<3;++i)b.box(-.071f+i*.071f,0,.081f,.062f,.19f,.055f,frame);

    }
}
void equipment(SdBuilder& b){
    // B17/B19 grip and B14 palm use exactly the same coordinate frame.
    handFrame(b,Part::Rifle,-1);
    b.box(0,0,0,.085f,.25f,.07f,frame);
    b.box(0,.195f,.19f,.14f,.18f,.54f,frame);
    b.cover({{-.072f,.08f,.47f},{.072f,.08f,.47f},{.072f,.25f,.47f},{-.072f,.25f,.47f}},.03f,frame,.012f);
    b.tube({0,.17f,.45f},{0,.17f,.91f},.055f,.038f,frame,true,12);
    b.tube({0,.17f,.70f},{0,.17f,.77f},.070f,.060f,frame);
    b.box(0,.32f,.14f,.06f,.17f,.055f,frame);
    b.tube({0,.37f,.12f},{0,.37f,.19f},.085f,.085f,gold,true,12);
    b.box(.13f,.10f,.20f,.23f,.065f,.075f,frame);
    auto anchor=armAnchor(b,1,{0,-.51f,0});
    // C1-11/B13 plate is outboard and forward; B16 bridges to forearm.
    const Point shieldOrigin{anchor.x+.42f,anchor.y,anchor.z+.30f};
    b.at(Part::Shield,shieldOrigin,.42f,0,.72f);
    auto mount=b.transform({0,0,-.175f});mount.y+=.14f;
    const Point socket{anchor.x+.16f,anchor.y,anchor.z+.10f};
    b.at(Part::Shield);b.tube(socket,mount,.038f,.038f,frame,false,8);
    b.at(Part::Shield,shieldOrigin,.42f,0,.72f);
    // B16's U-shaped handle stands off the shield rear, with a forearm peg.
    b.box(0,.08f,-.10f,.09f,.035f,.09f,frame);
    b.box(0,-.08f,-.10f,.09f,.035f,.09f,frame);
    b.box(0,0,-.155f,.09f,.195f,.04f,frame);
    b.cover({{-.27f,.65f,.05f},{.27f,.65f,.05f},{.29f,-.37f,.05f},{.19f,-.65f,.05f},{-.19f,-.65f,.05f},{-.29f,-.37f,.05f}},.105f,white,.008f);
    b.cover({{-.215f,.59f,.076f},{.215f,.59f,.076f},{.235f,-.34f,.076f},{.155f,-.57f,.076f},{-.155f,-.57f,.076f},{-.235f,-.34f,.076f}},.035f,red,.015f);
    b.opening({-.115f,.40f,.109f},{.115f,.40f,.109f},{.115f,.515f,.109f},{-.115f,.515f,.109f},.018f,white);
    const Point center{0,-.19f,.15f};
    const Point star[]={{0,.16f,.117f},{.034f,-.15f,.117f},{.19f,-.19f,.117f},{.034f,-.225f,.117f},{0,-.49f,.117f},{-.034f,-.225f,.117f},{-.19f,-.19f,.117f},{-.034f,-.15f,.117f}};
    for(int i=0;i<8;++i)b.face(center,star[i],star[(i+1)%8],star[(i+1)%8],gold,{0,0,1},true);
}
} // namespace
void buildRx78(Mesh& mesh,BuildOptions options,Rx78Assembly* assembly){
    if(assembly)*assembly={};
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;
    SdBuilder b{mesh,options};body(b);head(b,assembly);arms(b);if(options.equipment)equipment(b);
}
} // namespace gundam_museum
