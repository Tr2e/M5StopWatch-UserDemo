#include "rx78.h"
#include "../../app_lets_and_go_racer/model/car_mesh_builder.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace gundam_museum {
namespace {
constexpr uint16_t ivory=0xe75c,white=0xf7be,blue=0x2b75,red=0xc986,yellow=0xf64a;
constexpr uint16_t frame=0x526a,dark=0x2105,black=0x10a3,eye=0xff4b;
constexpr float pi=3.14159265359f;
struct Ring {float y,x,z,w,d;};
class Builder {
public:
    Mesh& out;BuildOptions options;
    Part part=Part::Torso;
    Point origin{};float angle=0;
    Point transform(Point p) const {
        const float c=std::cos(angle),s=std::sin(angle);
        return {origin.x+p.x*c-p.y*s,origin.y+p.x*s+p.y*c,origin.z+p.z};
    }
    void at(Part p,Point o={},float a=0){part=p;origin=o;angle=a;}
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
        const float lighting=.67f+.33f*std::max(0.f,dot(n,Point{-.36f,.72f,.59f}));
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
    void armor(std::initializer_list<Ring> rings,uint16_t color,bool buriedTop=false,bool buriedBottom=false){
        const auto point=[](Ring r,int i){
            constexpr float x[]={-.72f,.72f,1,1,.72f,-.72f,-1,-1};
            constexpr float z[]={1,1,.72f,-.72f,-1,-1,-.72f,.72f};
            return Point{r.x+x[i]*r.w,r.y,r.z+z[i]*r.d};
        };
        for(auto it=rings.begin()+1;it!=rings.end();++it){
            auto a=*(it-1),b=*it;
            for(int i=0;i<8;++i){auto p=point(a,i),q=point(a,(i+1)%8);
                const Point outward{(p.x+q.x)/2-a.x,0,(p.z+q.z)/2-a.z};
                face(p,q,point(b,(i+1)%8),point(b,i),color,outward);
            }
        }
        for(int end=0;end<2;++end){
            const auto r=end?*(rings.end()-1):*rings.begin();
            for(int i=1;i<7;i+=2)
                face(point(r,0),point(r,i),point(r,i+1),point(r,i+2),color,{0,end?1.f:-1.f,0},end?buriedTop:buriedBottom);
        }
    }
    void tube(Point a,Point b,float radius,uint16_t color,int segments=8,bool buriedEnds=false){
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
                face(a,x,y,p(a,i+2,radius),color,{-axis.x,-axis.y,-axis.z},buriedEnds);
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
        }
    }
};

void legs(Builder& b){
    for(float side:{-1.f,1.f}){
        const float x=side*.265f;
        b.at(Part::Feet,{x,0,.045f});
        b.armor({{.025f,0,.06f,.18f,.31f},{.12f,0,.07f,.19f,.32f},{.20f,0,.03f,.16f,.28f}},red);
        b.armor({{.12f,0,-.01f,.166f,.245f},{.28f,0,-.06f,.142f,.205f},{.32f,0,-.09f,.115f,.14f}},ivory);
        b.at(Part::Shins,{x,0,0});
        // Ankle joint ends lie completely inside the shoe and shin armour.
        b.tube({0,.23f,-.07f},{0,.43f,-.07f},.085f,frame,8,true);
        b.armor({{.32f,0,-.015f,.14f,.17f},{.49f,0,-.04f,.158f,.18f},
                 {.82f,0,-.06f,.17f,.19f},{1.04f,0,-.025f,.125f,.15f}},ivory);
        // Separate shin ridge and ankle guard, as B24/B27/B28 and B29.
        b.armor({{.43f,0,.133f,.067f,.034f},{.91f,0,.12f,.087f,.065f},{1.035f,0,.09f,.07f,.036f}},white);
        b.armor({{.23f,0,.065f,.18f,.15f},{.34f,0,.04f,.16f,.155f}},white);
        b.at(Part::Knees,{x,0,0});
        b.tube({-.125f,1.10f,-.01f},{.125f,1.10f,-.01f},.093f,frame,10);
        b.armor({{1.015f,0,.10f,.12f,.082f},{1.16f,0,.10f,.13f,.103f},{1.235f,0,.064f,.11f,.075f}},white);
        b.at(Part::Thighs,{x,0,0});
        b.armor({{1.18f,0,-.015f,.10f,.115f},{1.53f,0,-.022f,.138f,.142f},{1.72f,0,-.04f,.12f,.13f}},ivory);
        b.tube({0,1.65f,-.04f},{0,1.84f,-.04f},.08f,frame,8,true);
    }
}

void body(Builder& b){
    b.at(Part::Waist);
    b.box(0,1.80f,-.035f,.44f,.25f,.30f,frame);
    b.armor({{1.85f,0,0,.32f,.20f},{2.03f,0,0,.285f,.185f}},ivory);
    for(float s:{-1.f,1.f}){
        b.armor({{1.59f,s*.19f,.19f,.153f,.07f},{1.88f,s*.155f,.13f,.132f,.075f}},ivory);
        b.box(s*.168f,1.825f,.22f,.14f,.12f,.018f,yellow);
        b.armor({{1.67f,s*.35f,-.015f,.10f,.19f},{1.92f,s*.30f,-.015f,.07f,.165f}},ivory);
        b.armor({{1.61f,s*.17f,-.19f,.15f,.055f},{1.90f,s*.15f,-.15f,.125f,.055f}},ivory);
    }
    b.armor({{1.65f,0,.18f,.078f,.08f},{1.94f,0,.17f,.095f,.065f}},red);
    b.triangle({-.063f,1.85f,.246f},{0,1.775f,.27f},{0,1.82f,.269f},yellow,{0,0,1});
    b.triangle({0,1.82f,.269f},{0,1.775f,.27f},{.063f,1.85f,.246f},yellow,{0,0,1});
    b.at(Part::Torso);
    b.armor({{2.00f,0,-.005f,.24f,.16f},{2.16f,0,-.01f,.265f,.195f},{2.35f,0,-.025f,.295f,.20f}},red);
    b.armor({{2.27f,0,-.04f,.31f,.205f},{2.46f,0,-.05f,.40f,.225f},{2.64f,0,-.065f,.345f,.195f}},blue);
    // Chest centre projects between the two recessed yellow vent assemblies.
    b.armor({{2.13f,0,.16f,.10f,.052f},{2.36f,0,.18f,.105f,.066f},{2.52f,0,.15f,.17f,.061f}},blue);
    for(float s:{-1.f,1.f}){
        b.box(s*.252f,2.425f,.176f,.173f,.186f,.045f,dark);
        for(int k=0;k<3;++k)b.box(s*.252f,2.368f+k*.056f,.211f,.157f,.030f,.05f,yellow);
    }
    b.armor({{2.62f,0,-.055f,.17f,.135f},{2.67f,0,-.055f,.15f,.115f}},yellow);
    b.tube({0,2.62f,-.04f},{0,2.82f,-.04f},.083f,frame,8,true);
    b.at(Part::Backpack);
    b.armor({{2.21f,0,-.335f,.26f,.13f},{2.59f,0,-.335f,.28f,.15f},{2.65f,0,-.32f,.235f,.125f}},frame);
    b.box(0,2.50f,-.493f,.24f,.105f,.012f,dark);
    for(float s:{-1.f,1.f}){
        b.nozzle({s*.15f,2.29f,-.535f},.10f,.095f,frame);
        b.at(Part::Sabers);
        b.tube({s*.205f,2.56f,-.325f},{s*.245f,3.05f,-.34f},.038f,ivory,8);
        b.tube({s*.236f,2.95f,-.337f},{s*.239f,2.99f,-.339f},.044f,white,8);
        b.at(Part::Backpack);
    }
}

void head(Builder& b){
    b.at(Part::Head);
    // Helmet volume; a separate face mask and cheeks sit ahead of it.
    b.armor({{2.77f,0,-.035f,.135f,.115f},{2.91f,0,-.05f,.176f,.147f},
             {3.065f,0,-.055f,.16f,.14f},{3.145f,0,-.065f,.09f,.085f}},ivory);
    b.box(0,2.935f,.096f,.25f,.115f,.055f,black);
    for(float s:{-1.f,1.f}){
        b.triangle({s*.018f,2.963f,.128f},{s*.121f,2.971f,.128f},{s*.071f,2.945f,.140f},eye,{0,0,1});
        b.armor({{2.785f,s*.125f,.068f,.034f,.073f},{2.935f,s*.148f,.07f,.04f,.08f}},white);
        for(int k=0;k<3;++k)b.box(s*.149f,2.81f+k*.038f,.147f,.039f,.013f,.008f,frame);
        b.tube({s*.161f,3.015f,-.02f},{s*.185f,3.015f,-.02f},.034f,frame,8);
    }
    // Projecting muzzle: chamfered face guard, red chin, two face slits.
    b.armor({{2.80f,0,.123f,.054f,.045f},{2.88f,0,.143f,.085f,.063f},{2.93f,0,.125f,.05f,.04f}},white);
    b.armor({{2.757f,0,.10f,.035f,.043f},{2.82f,0,.143f,.045f,.048f}},red);
    for(float s:{-1.f,1.f})b.box(s*.026f,2.855f,.208f,.011f,.045f,.007f,frame);
    b.armor({{2.995f,0,.076f,.040f,.038f},{3.135f,0,.018f,.040f,.071f},{3.165f,0,-.046f,.034f,.048f}},white);
    b.box(0,3.079f,.119f,.069f,.076f,.024f,red);
    b.box(0,3.132f,-.13f,.055f,.038f,.014f,red);
    // V-fin blades have thickness and a closed rear; no double-sided paper fins.
    for(float s:{-1.f,1.f}){
        const Point a{s*.028f,3.053f,.16496f},e{s*.028f,3.099f,.16496f};
        const Point c{s*.35f,3.253f,.107f},d{s*.315f,3.199f,.1133f};
        const Point ar{a.x,a.y,a.z-.018f},er{e.x,e.y,e.z-.018f};
        const Point cr{c.x,c.y,c.z-.018f},dr{d.x,d.y,d.z-.018f};
        b.face(a,e,c,d,ivory,{0,0,1});b.face(ar,dr,cr,er,ivory,{0,0,-1});
        b.face(e,er,cr,c,ivory,{-s,1,0});b.face(c,cr,dr,d,ivory,{s,0,0});
        b.face(d,dr,ar,a,ivory,{s,-1,0});b.face(a,ar,er,e,ivory,{-s,0,0});
    }
}

void arms(Builder& b){
    for(float s:{-1.f,1.f}){
        b.at(Part::Shoulders,{s*.495f,2.48f,-.03f});
        b.tube({-s*.20f,0,0},{s*.12f,0,0},.093f,frame,8,true);
        b.armor({{-.12f,s*.005f,0,.17f,.185f},{.115f,0,0,.205f,.19f},{.17f,-s*.015f,0,.15f,.145f}},ivory);
        b.at(Part::Arms,{s*.52f,2.37f,-.01f},s*.10f);
        b.armor({{-.29f,0,0,.10f,.11f},{-.04f,0,0,.115f,.12f}},ivory);
        b.tube({-.114f,-.345f,0},{.114f,-.345f,0},.077f,frame,8);
        b.armor({{-.71f,s*.005f,.017f,.095f,.104f},{-.43f,0,.015f,.129f,.135f},{-.39f,0,.005f,.11f,.11f}},ivory);
        b.box(0,-.50f,.153f,.13f,.15f,.026f,white);
        b.tube({0,-.79f,.015f},{0,-.665f,.015f},.065f,frame,8,true);
        b.part=Part::Hands;
        b.armor({{-.87f,0,.037f,.10f,.09f},{-.76f,0,.037f,.095f,.10f}},frame);
        for(int k=0;k<3;++k)b.box(-.054f+k*.053f,-.823f,.13f,.036f,.072f,.012f,dark);
    }
    b.at(Part::Torso);
}

void equipment(Builder& b){
    b.at(Part::Rifle,{-.61f,1.55f,.20f},-.12f);
    // Rifle rests muzzle-down: receiver, grip, round sight, barrel and open muzzle.
    b.box(.015f,.01f,-.02f,.09f,.23f,.10f,dark);
    b.armor({{-.25f,0,.075f,.08f,.072f},{.27f,0,.075f,.085f,.07f},{.32f,0,.075f,.057f,.052f}},frame);
    b.box(-.015f,.11f,.16f,.13f,.21f,.035f,dark);
    b.tube({0,-.26f,.075f},{0,-.78f,.075f},.036f,frame,8);
    b.tube({0,-.25f,.075f},{0,-.35f,.075f},.054f,dark,8);
    b.tube({.077f,.15f,.078f},{.15f,.15f,.078f},.067f,frame,10);
    b.box(.155f,.15f,.078f,.015f,.079f,.079f,yellow);
    b.box(.04f,-.12f,.10f,.19f,.065f,.085f,frame);
    b.at(Part::Shield,{.83f,1.84f,.20f},-.09f);
    // The shield is a shaped shell, not a decal: white rim, red inset, rear ribs.
    b.armor({{-.66f,0,0,.13f,.053f},{-.55f,0,0,.225f,.06f},{.65f,0,0,.225f,.06f},{.73f,0,0,.16f,.05f}},ivory);
    b.armor({{-.55f,0,.063f,.125f,.018f},{-.48f,0,.063f,.19f,.018f},{.59f,0,.063f,.19f,.018f},{.66f,0,.063f,.14f,.018f}},red);
    b.box(0,.53f,.087f,.235f,.078f,.012f,frame);
    b.box(0,.53f,.096f,.18f,.044f,.006f,dark);
    // Four tapered points of the yellow cross, all sitting on the red surface.
    const float z=.085f;
    b.triangle({-.029f,-.04f,z},{0,.32f,z},{.029f,-.04f,z},yellow,{0,0,1});
    b.triangle({-.029f,-.08f,z},{.029f,-.08f,z},{0,-.53f,z},yellow,{0,0,1});
    b.triangle({0,-.014f,z},{-.17f,.009f,z},{0,-.10f,z},yellow,{0,0,1});
    b.triangle({0,-.014f,z},{0,-.10f,z},{.17f,.009f,z},yellow,{0,0,1});
    b.box(0,-.06f,z,.05f,.09f,.002f,yellow);
    for(float s:{-1.f,1.f})b.box(s*.15f,0,-.069f,.022f,1.08f,.027f,frame);
    for(int k=0;k<4;++k)b.box(0,-.39f+k*.24f,-.07f,.30f,.019f,.024f,frame);
    b.box(-.10f,.01f,-.12f,.12f,.12f,.12f,frame);
}
} // namespace

void buildRx78(Mesh& mesh,BuildOptions options){
    mesh.count=mesh.buriedOmitted=0;mesh.overflowed=false;
    Builder b{mesh,options};legs(b);body(b);head(b);arms(b);if(options.equipment)equipment(b);
}
} // namespace gundam_museum
