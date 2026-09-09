#include "car_mesh_builder.h"

namespace lets_and_go {
namespace {
using namespace mesh_parts;

void cyclone(Builder& b) {
    // Nose flares at the front shoulder, then contracts BEFORE the cockpit.
    // The .16 waist leaves a real open channel to the .24 inner rear cowl.
    b.part=CarPart::Nose;
    b.chine({{-.71f,.125f,.22f,.29f,.32f},{-.53f,.155f,.225f,.305f,.33f},
        {-.30f,.16f,.21f,.29f,.31f},{-.10f,.17f,.19f,.275f,.29f},
        {.04f,.235f,.17f,.27f,.285f},{.25f,.305f,.135f,.225f,.255f},
        {.43f,.245f,.115f,.175f,.215f},{.66f,.135f,.09f,.12f,.15f},
        {.84f,.030f,.083f,.105f,.12f}},white,CarPaint::MagnumHood,CarPaint::MagnumNoseSide);
    // Slanted, nearly flat windscreen with white sill rails; not an oval bubble.
    b.part=CarPart::Canopy;
    b.chine({{-.53f,.105f,.315f,.40f,.445f},{-.39f,.131f,.30f,.415f,.45f},
        {-.16f,.137f,.28f,.355f,.389f},{.055f,.084f,.267f,.274f,.284f}},
        glass,CarPaint::MagnumCanopy,CarPaint::MagnumCanopy);
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::Canopy;
        b.quad({s*.112f,.316f,-.54f},{s*.144f,.31f,-.54f},
               {s*.151f,.279f,-.15f},{s*.138f,.28f,-.15f},white);
        b.quad({s*.138f,.28f,-.15f},{s*.151f,.279f,-.15f},
               {s*.098f,.271f,.063f},{s*.083f,.272f,.063f},white);
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.80f,.255f,.535f,.34f,.37f},{-.67f,.245f,.545f,.373f,.407f},
            {-.48f,.24f,.551f,.382f,.422f},{-.29f,.275f,.53f,.35f,.395f},
            {-.14f,.335f,.465f,.275f,.315f},{-.075f,.365f,.405f,.25f,.273f}},
            white,CarPaint::MagnumCowl,CarPaint::MagnumCowlSide);
        // Two short molded webs connect the pods; the rest of the channel stays open.
        b.part=CarPart::SideWeb;
        b.box(std::min(s*.15f,s*.28f),std::max(s*.15f,s*.28f),.215f,.235f,-.33f,-.27f,white);
        b.box(std::min(s*.13f,s*.31f),std::max(s*.13f,s*.31f),.245f,.265f,-.69f,-.64f,white);
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.585f,.325f,.445f,.363f,.393f},{.64f,.325f,.475f,.345f,.37f},
            {.735f,.325f,.53f,.245f,.268f},{.84f,.365f,.51f,.13f,.151f}},blue,CarPaint::Eye,
            CarPaint::Solid,.012f);
        b.box(std::min(s*.365f,s*.49f),std::max(s*.365f,s*.49f),.10f,.133f,.825f,.84f,blue);
        // Visible chassis side tray below the waist opening, separated from the white shell.
        b.part=CarPart::Chassis;
        b.quad({s*.24f,.105f,-.34f},{s*.36f,.105f,-.27f},
               {s*.345f,.105f,.33f},{s*.24f,.105f,.40f},graphite);
        b.box(s*.31f-.014f,s*.31f+.014f,.105f,.16f,-.15f,.20f,graphite);
    }
    // Raised rear intake and sloping black opening behind the canopy.
    b.part=CarPart::Intake;
    b.chine({{-.69f,.09f,.31f,.365f,.405f},{-.60f,.085f,.325f,.41f,.463f},
             {-.53f,.068f,.325f,.40f,.43f}},white,CarPaint::Solid);
    b.quad({-.059f,.431f,-.531f},{.059f,.431f,-.531f},
           {.070f,.461f,-.596f},{-.070f,.461f,-.596f},graphite,CarPaint::MagnumVent);
    // Cambered aerofoil, two leaning mounts, swept side plates.
    b.part=CarPart::RearWing;
    b.chine({{-.96f,.49f,.452f,.473f,.478f},{-.85f,.49f,.435f,.46f,.467f},
             {-.735f,.49f,.431f,.451f,.456f}},blue,CarPaint::MagnumWing);
    for(float s : {-1.f,1.f}) {
        b.quad({s*.215f,.30f,-.68f},{s*.255f,.30f,-.68f},
               {s*.255f,.447f,-.83f},{s*.215f,.447f,-.83f},graphite);
        const float x=s*.503f;
        b.quad({x,.31f,-.73f},{x,.52f,-.72f},{x,.54f,-.93f},{x,.44f,-.98f},white);
        b.quad({x+s*.002f,.327f,-.746f},{x+s*.002f,.507f,-.738f},
               {x+s*.002f,.524f,-.917f},{x+s*.002f,.443f,-.96f},blue);
    }
}

void sonic(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.72f,.13f,.23f,.30f,.34f},{-.49f,.17f,.225f,.305f,.33f},
        {-.22f,.18f,.20f,.285f,.30f},{-.04f,.20f,.185f,.27f,.29f},
        {.24f,.305f,.13f,.23f,.265f},{.43f,.25f,.12f,.18f,.225f},
        {.65f,.125f,.095f,.13f,.17f},{.83f,.044f,.09f,.12f,.14f}},
        white,CarPaint::SonicHood);
    b.part=CarPart::Canopy;
    b.chine({{-.52f,.118f,.31f,.41f,.46f},{-.37f,.148f,.30f,.42f,.46f},
        {-.15f,.142f,.28f,.36f,.395f},{.065f,.084f,.267f,.28f,.295f}},
        glass,CarPaint::SonicCanopy,CarPaint::SonicCanopy);
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::RearCowl;
        // Rear cowls rise into the stepped wing ramps, not two rounded pods
        // underneath a disconnected flat spoiler.
        b.cowl(s,{{-.93f,.26f,.51f,.458f,.48f},{-.76f,.25f,.535f,.425f,.452f},
            {-.58f,.255f,.55f,.383f,.413f},{-.43f,.27f,.55f,.38f,.414f},
            {-.25f,.305f,.52f,.315f,.36f},{-.10f,.335f,.48f,.265f,.30f}},
            white,CarPaint::SonicCowl,CarPaint::SonicSide,.014f);
        for(int rib=0;rib<3;++rib) {
            const float z=-.86f+rib*.085f;
            const float t=z<-.76f ? (z+.93f)/.17f : (z+.76f)/.18f;
            const float crown=(z<-.76f ? .48f+(.452f-.48f)*t : .452f+(.413f-.452f)*t)+.008f;
            const float edge=(z<-.76f ? .458f+(.425f-.458f)*t : .425f+(.383f-.425f)*t)+.008f;
            // Follow the cowl cross-section; a single sloped strip was buried
            // under the broad centre facet and disappeared in the top view.
            b.cowl(s,{{z,.255f,.535f,edge,crown},{z+.018f,.255f,.535f,edge-.004f,crown-.004f}},
                   white,CarPaint::Solid,CarPaint::Solid,.006f);
        }
        b.part=CarPart::SideWeb;
        b.box(std::min(s*.16f,s*.30f),std::max(s*.16f,s*.30f),.215f,.238f,-.34f,-.28f,white);
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.555f,.325f,.445f,.386f,.414f},{.64f,.32f,.49f,.342f,.376f},
            {.745f,.33f,.53f,.24f,.267f},{.84f,.365f,.515f,.135f,.16f}},
            red,CarPaint::SonicFront,CarPaint::SonicSide,.012f);
        b.box(std::min(s*.365f,s*.49f),std::max(s*.365f,s*.49f),.10f,.137f,.825f,.84f,0x246d);
        b.part=CarPart::FrontBridge;
        b.quad({s*.12f,.285f,.61f},{s*.325f,.39f,.565f},
               {s*.32f,.265f,.795f},{s*.08f,.235f,.77f},silver);
    }
    b.quad({-.12f,.285f,.61f},{.12f,.285f,.61f},{.08f,.235f,.77f},{-.08f,.235f,.77f},
           silver,CarPaint::FrontWing);
    b.part=CarPart::Intake;
    b.chine({{-.70f,.083f,.33f,.395f,.421f},{-.60f,.08f,.34f,.435f,.48f},
        {-.53f,.065f,.33f,.425f,.447f}},white,CarPaint::Solid);
    b.quad({-.057f,.448f,-.531f},{.057f,.448f,-.531f},
           {.069f,.478f,-.596f},{-.069f,.478f,-.596f},graphite,CarPaint::MagnumVent);
    b.part=CarPart::RearWing;
    b.chine({{-.96f,.52f,.457f,.487f,.495f},{-.86f,.51f,.445f,.469f,.484f},
        {-.76f,.49f,.43f,.452f,.462f}},red,CarPaint::SonicWing);
    for(float s : {-1.f,1.f})
        b.quad({s*.525f,.40f,-.76f},{s*.525f,.505f,-.73f},
               {s*.525f,.54f,-.965f},{s*.525f,.45f,-.965f},white);
}

void neo(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.73f,.19f,.18f,.275f,.31f},{-.51f,.22f,.20f,.28f,.33f},
        {-.27f,.255f,.15f,.235f,.275f},{0,.29f,.10f,.215f,.25f},
        {.24f,.325f,.10f,.18f,.205f},{.47f,.34f,.09f,.14f,.165f},
        {.70f,.29f,.09f,.12f,.13f},{.83f,.19f,.085f,.105f,.12f}},graphite,CarPaint::NeoHood);
    // Separate central dagger ridge; the broad flat deck does not become a bubble nose.
    b.chine({{.49f,.062f,.145f,.17f,.23f},{.66f,.05f,.13f,.17f,.211f},
        {.86f,.026f,.083f,.105f,.12f}},graphite,CarPaint::Solid);
    b.part=CarPart::Canopy;
    b.chine({{-.49f,.115f,.29f,.415f,.46f},{-.34f,.18f,.275f,.445f,.49f},
        {-.12f,.19f,.245f,.39f,.435f},{.16f,.15f,.23f,.295f,.335f},
        {.29f,.13f,.19f,.22f,.24f}},0xacce,CarPaint::NeoCanopy,CarPaint::NeoCanopy);
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.80f,.255f,.53f,.32f,.35f},{-.64f,.24f,.55f,.37f,.41f},
            {-.48f,.24f,.54f,.373f,.40f},{-.25f,.27f,.49f,.27f,.32f},
            {-.14f,.30f,.41f,.235f,.255f}},graphite,CarPaint::Flame,CarPaint::Flame,.012f);
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.05f,.28f,.42f,.215f,.26f},{.24f,.29f,.45f,.245f,.30f},
            {.36f,.30f,.415f,.32f,.365f},{.50f,.31f,.352f,.365f,.38f},
            {.66f,.31f,.353f,.30f,.325f},{.83f,.33f,.47f,.12f,.155f}},
            graphite,CarPaint::Flame,CarPaint::Flame,.012f);
        b.part=CarPart::SideGuard;
        b.box(s*.50f-.018f,s*.50f+.018f,.095f,.13f,-.10f,.30f,blue);
        b.box(std::min(s*.27f,s*.55f),std::max(s*.27f,s*.55f),.10f,.135f,-.14f,-.07f,blue);
        b.box(s*.55f-.022f,s*.55f+.022f,.12f,.195f,-.127f,-.083f,blue);
    }
    b.part=CarPart::Intake;
    b.chine({{-.65f,.085f,.31f,.39f,.44f},{-.55f,.07f,.32f,.44f,.472f},
        {-.50f,.07f,.32f,.41f,.445f}},graphite,CarPaint::Solid);
    b.quad({-.059f,.446f,-.499f},{.059f,.446f,-.499f},
           {.06f,.47f,-.55f},{-.06f,.47f,-.55f},silver,CarPaint::MagnumVent);
    b.part=CarPart::RearWing;
    for(float s : {-1.f,1.f}) {
        if(s<0)b.quad({s*.50f,.565f,-.96f},{0,.52f,-.77f},
                      {0,.50f,-.66f},{s*.50f,.55f,-.74f},graphite,CarPaint::NeoWingLeft);
        else b.quad({0,.52f,-.77f},{s*.50f,.565f,-.96f},
                    {s*.50f,.55f,-.74f},{0,.50f,-.66f},graphite,CarPaint::TridaggerWing);
        b.quad({s*.13f,.31f,-.66f},{s*.19f,.31f,-.66f},
               {s*.19f,.526f,-.76f},{s*.13f,.526f,-.76f},graphite);
        b.quad({s*.51f,.55f,-.73f},{s*.51f,.595f,-.73f},
               {s*.51f,.61f,-.965f},{s*.51f,.563f,-.965f},graphite);
    }
    b.box(-.012f,.012f,.51f,.587f,-.765f,-.68f,graphite);
}

void brocken(Builder& b) {
    // Three separate assemblies: cabin, exposed front motor and detachable nose.
    b.part=CarPart::Canopy;
    b.chine({{-.73f,.17f,.19f,.29f,.31f},{-.54f,.24f,.18f,.33f,.405f},
        {-.36f,.265f,.18f,.40f,.46f},{-.14f,.26f,.18f,.385f,.452f},
        {.095f,.24f,.18f,.27f,.29f}},red,CarPaint::BrockenCabin,CarPaint::BrockenCabinSide);
    b.part=CarPart::Nose;
    b.chine({{.455f,.18f,.115f,.235f,.27f},{.64f,.155f,.10f,.17f,.205f},
        {.84f,.15f,.09f,.11f,.14f}},red,CarPaint::BrockenHood);
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.84f,.25f,.54f,.28f,.315f},{-.70f,.28f,.54f,.34f,.37f},
            {-.55f,.30f,.52f,.373f,.39f},{-.45f,.305f,.515f,.355f,.375f},
            {-.36f,.31f,.50f,.29f,.33f},
            {-.18f,.28f,.425f,.26f,.29f}},red,CarPaint::Tiger,CarPaint::Tiger,.012f);
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.30f,.20f,.45f,.325f,.35f},{.52f,.20f,.52f,.375f,.40f},
            {.60f,.20f,.52f,.355f,.38f},{.72f,.20f,.515f,.26f,.30f},{.85f,.20f,.49f,.15f,.18f}},
            red,CarPaint::Tiger,CarPaint::Tiger,.012f);
        b.quad({s*.21f,.115f,.853f},{s*.48f,.115f,.853f},
               {s*.48f,.171f,.853f},{s*.21f,.171f,.853f},red,CarPaint::BrockenLamp);
        b.part=CarPart::SideWeb;
        b.cowl(s,{{-.28f,.255f,.41f,.20f,.28f},{-.08f,.245f,.425f,.22f,.30f},
            {.12f,.23f,.43f,.22f,.29f},{.30f,.20f,.45f,.265f,.325f}},
            red,CarPaint::Tiger,CarPaint::Tiger,.012f);
        b.part=CarPart::SideGuard;
        b.tube({s*.605f,.18f,-.80f},{s*.605f,.18f,-.14f},.014f,silver);
        b.tube({s*.605f,.18f,.14f},{s*.605f,.18f,.78f},.014f,silver);
        for(float z : {-.80f,.78f}) {
            b.box(std::min(s*.48f,s*.625f),std::max(s*.48f,s*.625f),.125f,.205f,z-.035f,z+.035f,red);
        }
        b.box(std::min(s*.565f,s*.63f),std::max(s*.565f,s*.63f),.125f,.245f,-.16f,.16f,red);
        b.box(std::min(s*.41f,s*.61f),std::max(s*.41f,s*.61f),.13f,.19f,-.10f,.10f,red);
        b.part=CarPart::TailFin;
        b.tube({s*.17f,.285f,-.73f},{s*.17f,.47f,-.64f},.013f,silver);
        b.tube({s*.17f,.47f,-.64f},{s*.17f,.285f,-.52f},.013f,silver);
        b.part=CarPart::MotorBlock;
        b.box(std::min(s*.20f,s*.32f),std::max(s*.20f,s*.32f),.25f,.33f,.19f,.39f,red);
        b.quad({s*.22f,.332f,.20f},{s*.31f,.332f,.20f},
               {s*.31f,.332f,.385f},{s*.22f,.332f,.385f},silver,CarPaint::BrockenArmor);
    }
    b.part=CarPart::MotorBlock;
    b.box(-.20f,.20f,.16f,.285f,.14f,.44f,graphite);
    const float z[]={.15f,.22f,.30f,.38f,.445f},y[]={.29f,.35f,.367f,.34f,.27f};
    for(int rib=0;rib<7;++rib) {
        const float x=-.17f+rib*.056f;
        for(int i=0;i<4;++i) {
            b.quad({x-.015f,y[i],z[i]},{x+.015f,y[i],z[i]},
                   {x+.015f,y[i+1],z[i+1]},{x-.015f,y[i+1],z[i+1]},red);
            for(float s : {-1.f,1.f})
                b.quad({x+s*.015f,y[i]-.019f,z[i]},{x+s*.015f,y[i],z[i]},
                       {x+s*.015f,y[i+1],z[i+1]},{x+s*.015f,y[i+1]-.019f,z[i+1]},shade(red,.72f));
        }
    }
    b.part=CarPart::Intake;
    b.quad({-.125f,.273f,.456f},{.125f,.273f,.456f},
           {.105f,.242f,.51f},{-.105f,.242f,.51f},graphite,CarPaint::MagnumVent);
    for(int rib=0;rib<4;++rib)b.box(-.10f+rib*.062f,-.085f+rib*.062f,.24f,.277f,.455f,.50f,red);
    b.part=CarPart::SideGuard;
    b.tube({-.11f,.145f,.856f},{.11f,.145f,.856f},.018f,silver);
    b.part=CarPart::Canopy;
    b.box(-.035f,.035f,.405f,.474f,-.525f,-.44f,red);
    b.quad({-.025f,.421f,-.439f},{.025f,.421f,-.439f},
           {.025f,.465f,-.439f},{-.025f,.465f,-.439f},graphite);
}
// R23 bodies are authored from the Tamiya 19450/19408/19438/19443 photos.
// Common rolling hardware is shared, not the silhouettes or body panels.
void cobra(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.65f,.14f,.22f,.32f,.36f},{-.40f,.17f,.22f,.32f,.36f},
        {-.12f,.155f,.18f,.30f,.32f},{.16f,.225f,.12f,.265f,.305f},
        {.40f,.25f,.10f,.22f,.275f},{.67f,.23f,.08f,.15f,.19f},
        {.81f,.30f,.08f,.13f,.15f}},blue,CarPaint::CobraHood);
    b.part=CarPart::Canopy;
    b.chine({{-.51f,.11f,.33f,.46f,.49f},{-.35f,.14f,.31f,.45f,.49f},
        {-.10f,.15f,.30f,.37f,.405f},{.12f,.10f,.29f,.30f,.31f}},glass,CarPaint::Glass);
    for(float s:{-1.f,1.f}) {
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.20f,.25f,.44f,.29f,.33f},{.36f,.27f,.54f,.345f,.38f},
            {.52f,.29f,.566f,.383f,.414f},{.64f,.29f,.55f,.35f,.386f},
            {.76f,.31f,.51f,.225f,.27f},{.855f,.34f,.47f,.13f,.16f}},blue,CarPaint::CobraFlame);
        b.quad({s*.35f,.340f,.700f},{s*.45f,.342f,.700f},
            {s*.43f,.182f,.848f},{s*.355f,.180f,.848f},silver,CarPaint::CobraLamp);
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.81f,.27f,.53f,.31f,.35f},{-.66f,.24f,.55f,.373f,.41f},
            {-.50f,.24f,.55f,.385f,.42f},{-.33f,.27f,.51f,.33f,.40f}},blue,CarPaint::CobraFlame);
        b.part=CarPart::SideWeb;
        b.quad({s*.20f,.22f,-.22f},{s*.50f,.21f,-.23f},
            {s*.51f,.20f,.24f},{s*.23f,.245f,.18f},blue,CarPaint::CobraFlame);
        b.quad({s*.50f,.21f,-.23f},{s*.51f,.20f,.24f},
            {s*.50f,.135f,.19f},{s*.49f,.145f,-.19f},blue);
        b.part=CarPart::Intake;
        b.duct(s,.385f,.355f,-.19f,.103f,.053f,.145f,blue);
        // Tall cockpit sill and triangular side window are separate from the hood.
        b.part=CarPart::Canopy;
        b.quad({s*.15f,.305f,-.41f},{s*.135f,.455f,-.40f},
            {s*.155f,.31f,-.08f},{s*.16f,.30f,-.08f},blue);
        b.quad({s*.152f,.329f,-.36f},{s*.143f,.416f,-.36f},
            {s*.157f,.327f,-.17f},{s*.157f,.327f,-.17f},glass);
        b.part=CarPart::RearWing;
        b.quad({s*.245f,.36f,-.67f},{s*.32f,.36f,-.68f},
            {s*.48f,.57f,-.955f},{s*.31f,.51f,-.92f},blue);
        b.box(std::min(s*.20f,s*.27f),std::max(s*.20f,s*.27f),.28f,.38f,-.71f,-.65f,blue);
    }
    b.part=CarPart::RearWing;
    b.chine({{-.94f,.40f,.465f,.49f,.50f},{-.86f,.34f,.455f,.48f,.49f}},blue,CarPaint::Solid);
    b.part=CarPart::Canopy;
    b.chine({{-.55f,.10f,.46f,.485f,.50f},{-.39f,.13f,.45f,.47f,.485f}},blue,CarPaint::Solid);
    b.part=CarPart::FrontBridge;
    b.chine({{.80f,.42f,.095f,.14f,.17f},{.875f,.41f,.09f,.12f,.14f}},blue,CarPaint::CobraBridge);
}

void spider(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.70f,.12f,.21f,.30f,.34f},{-.42f,.15f,.20f,.30f,.34f},
        {-.12f,.17f,.16f,.285f,.31f},{.18f,.27f,.12f,.25f,.30f},
        {.43f,.30f,.10f,.22f,.255f},{.70f,.35f,.09f,.17f,.19f},
        {.85f,.44f,.085f,.13f,.15f}},graphite,CarPaint::SpiderHood);
    b.part=CarPart::Canopy;
    b.chine({{-.46f,.115f,.31f,.435f,.465f},{-.30f,.15f,.285f,.415f,.445f},
        {-.08f,.16f,.265f,.35f,.382f},{.13f,.13f,.26f,.275f,.302f}},glass,CarPaint::SpiderCanopy,CarPaint::SpiderCanopy);
    for(float s:{-1.f,1.f}) {
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.26f,.26f,.43f,.30f,.335f},{.40f,.29f,.54f,.355f,.39f},
            {.55f,.31f,.55f,.375f,.405f},{.68f,.31f,.55f,.315f,.35f},
            {.81f,.32f,.525f,.17f,.20f}},graphite,CarPaint::SpiderWeb);
        b.quad({s*.36f,.207f,.802f},{s*.49f,.205f,.803f},
            {s*.48f,.141f,.858f},{s*.34f,.143f,.86f},red,CarPaint::StingerLamp);
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.81f,.26f,.53f,.32f,.35f},{-.66f,.25f,.55f,.373f,.414f},
            {-.48f,.24f,.55f,.38f,.423f},{-.28f,.26f,.48f,.26f,.32f}},graphite,CarPaint::SpiderCowl);
        b.part=CarPart::SideGuard;
        b.quad({s*.595f,.13f,.78f},{s*.595f,.13f,.25f},
            {s*.58f,.24f,.26f},{s*.58f,.18f,.78f},graphite,CarPaint::SpiderWeb);
        b.quad({s*.47f,.14f,.87f},{s*.595f,.13f,.78f},
            {s*.58f,.18f,.78f},{s*.48f,.17f,.85f},graphite,CarPaint::SpiderWeb);
        b.quad({s*.595f,.13f,.25f},{s*.58f,.24f,.26f},
            {s*.55f,.21f,.22f},{s*.55f,.13f,.22f},graphite);
        b.part=CarPart::SideWeb;
        b.box(std::min(s*.15f,s*.29f),std::max(s*.15f,s*.29f),.19f,.225f,-.28f,-.22f,graphite);
        b.part=CarPart::Chassis;
        b.box(std::min(s*.23f,s*.57f),std::max(s*.23f,s*.57f),.10f,.135f,-.17f,-.12f,blue);
        b.box(std::min(s*.17f,s*.27f),std::max(s*.17f,s*.27f),.25f,.29f,-.68f,-.47f,0x2495);
        b.part=CarPart::RearWing;
        b.quad({s*.48f,.31f,-.75f},{s*.48f,.60f,-.96f},
            {s*.48f,.59f,-.64f},{s*.48f,.36f,-.60f},graphite);
    }
    b.part=CarPart::RearWing;
    for(int i=0;i<3;++i) {
        const float z=-.95f+i*.13f,y=.565f-i*.055f;
        b.chine({{z,.48f,y-.02f,y,y+.007f},{z+.068f,.48f,y-.02f,y,y+.007f}},
                 graphite,i==0 ? CarPaint::SpiderWing : CarPaint::Solid);
    }
    b.part=CarPart::TailFin;
    b.chine({{-.96f,.027f,.54f,.60f,.608f},{-.66f,.04f,.43f,.53f,.54f},
        {-.45f,.075f,.30f,.37f,.39f}},graphite,CarPaint::SpiderWeb);
    b.part=CarPart::FrontBridge;
    b.chine({{.87f,.53f,.085f,.115f,.12f},{.935f,.54f,.075f,.09f,.095f}},graphite,CarPaint::Solid);
}

void stinger(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.72f,.11f,.22f,.33f,.39f},{-.46f,.12f,.22f,.35f,.40f},
        {-.16f,.18f,.19f,.29f,.34f},{.15f,.25f,.12f,.235f,.28f},
        {.42f,.22f,.10f,.17f,.22f},{.68f,.12f,.09f,.115f,.16f},
        {.84f,.02f,.08f,.11f,.12f}},silver,CarPaint::Solid);
    b.chine({{-.20f,.035f,.30f,.385f,.405f},{.06f,.048f,.26f,.29f,.33f},
        {.42f,.036f,.19f,.23f,.265f},{.84f,.008f,.10f,.125f,.135f}},silver,CarPaint::Solid);
    b.part=CarPart::Canopy;
    b.chine({{-.58f,.095f,.35f,.46f,.49f},{-.39f,.135f,.33f,.44f,.48f},
        {-.17f,.13f,.29f,.365f,.40f},{.06f,.065f,.25f,.275f,.29f}},glass,CarPaint::Glass);
    for(float s:{-1.f,1.f}) {
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.25f,.22f,.43f,.285f,.32f},{.41f,.28f,.55f,.354f,.395f},
            {.53f,.30f,.55f,.38f,.413f},{.67f,.27f,.50f,.32f,.357f},
            {.83f,.31f,.43f,.14f,.17f},{.91f,.36f,.385f,.10f,.115f}},silver,CarPaint::StingerHood,CarPaint::Solid,.012f);
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.82f,.24f,.53f,.32f,.36f},{-.68f,.22f,.55f,.38f,.44f},
            {-.49f,.23f,.55f,.39f,.45f},{-.37f,.25f,.50f,.35f,.435f}},silver,CarPaint::StingerCowl);
        b.part=CarPart::Intake;
        for(float x:{.31f,.43f})b.duct(s,x,.40f,-.26f,.052f,.052f,.10f,0xe5ca,graphite);
        b.part=CarPart::SideWeb;
        b.box(std::min(s*.15f,s*.29f),std::max(s*.15f,s*.29f),.18f,.22f,-.29f,-.20f,silver);
        b.cowl(s,{{-.30f,.17f,.44f,.30f,.34f},{-.14f,.18f,.46f,.31f,.35f},
            {.10f,.20f,.38f,.245f,.295f},{.24f,.235f,.29f,.235f,.26f}},silver,CarPaint::Solid);
        b.part=CarPart::Intake;
        for(int i=0;i<3;++i) {
            const float y=.18f+i*.035f;
            b.tube({s*.31f,y,-.25f},{s*.42f,y,-.10f},.015f,silver);
            b.tube({s*.42f,y,-.10f},{s*.43f,y,.07f},.015f,silver);
            b.tube({s*.43f,y,.07f},{s*.32f,y,.20f},.015f,silver);
        }
        b.part=CarPart::FrontBridge;
        b.quad({s*.10f,.16f,.72f},{s*.25f,.17f,.73f},
            {s*.25f,.125f,.84f},{s*.07f,.13f,.84f},red,CarPaint::StingerLamp);
    }
    b.part=CarPart::TailFin;
    const CarPoint outline[]={{0,.39f,-.52f},{0,.42f,-.87f},{0,.59f,-.88f},{0,.53f,-.68f}};
    for(float s:{-1.f,1.f})b.quad({s*.006f,.39f,-.52f},{s*.006f,.42f,-.87f},
        {s*.006f,.59f,-.88f},{s*.006f,.53f,-.68f},silver);
    for(int i=0;i<4;++i) {
        const auto a=outline[i],c=outline[(i+1)%4];
        b.quad({-.006f,a.y,a.z},{.006f,a.y,a.z},{.006f,c.y,c.z},{-.006f,c.y,c.z},silver);
    }
}

void diospada(Builder& b) {
    b.part=CarPart::Nose;
    b.chine({{-.70f,.13f,.22f,.29f,.32f},{-.43f,.17f,.20f,.30f,.34f},
        {-.14f,.18f,.17f,.29f,.33f},{.15f,.20f,.14f,.26f,.29f},
        {.42f,.185f,.10f,.21f,.245f},{.70f,.24f,.08f,.16f,.19f},
        {.85f,.40f,.08f,.13f,.15f}},red,CarPaint::DiospadaHood);
    b.chine({{.20f,.035f,.26f,.29f,.31f},{.43f,.030f,.21f,.255f,.27f},
        {.73f,.012f,.15f,.18f,.19f}},red,CarPaint::Solid);
    b.part=CarPart::Canopy;
    b.chine({{-.47f,.12f,.31f,.415f,.44f},{-.33f,.15f,.30f,.42f,.455f},
        {-.13f,.155f,.28f,.37f,.405f},{.04f,.13f,.27f,.29f,.315f}},glass,CarPaint::DiospadaCanopy);
    for(float s:{-1.f,1.f}) {
        b.part=CarPart::FrontCowl;
        b.cowl(s,{{.25f,.25f,.44f,.29f,.33f},{.41f,.30f,.54f,.355f,.39f},
            {.54f,.31f,.55f,.38f,.41f},{.65f,.30f,.55f,.35f,.38f},
            {.77f,.29f,.53f,.22f,.26f},{.86f,.32f,.46f,.14f,.17f}},red,CarPaint::Solid);
        b.quad({s*.31f,.262f,.768f},{s*.475f,.262f,.766f},
            {s*.44f,.18f,.85f},{s*.31f,.178f,.85f},silver,CarPaint::DiospadaLamp);
        b.part=CarPart::RearCowl;
        b.cowl(s,{{-.84f,.25f,.53f,.32f,.355f},{-.69f,.25f,.55f,.375f,.41f},
            {-.51f,.24f,.55f,.39f,.42f},{-.29f,.24f,.49f,.295f,.34f}},red,CarPaint::Solid);
        // A forward-canted recessed side intake, framed by a swept roof/sill.
        b.part=CarPart::SideWeb;
        b.cowl(s,{{-.29f,.16f,.49f,.31f,.35f},{-.14f,.20f,.49f,.32f,.355f},
            {.09f,.225f,.46f,.325f,.35f},{.27f,.24f,.40f,.30f,.335f}},red,CarPaint::DiospadaSide);
        b.box(std::min(s*.23f,s*.47f),std::max(s*.23f,s*.47f),.105f,.14f,-.24f,.23f,red);
        const auto mouth=[s](float r,float a,float depth) {
            const float z=.10f+.22f*r*std::cos(a);
            return CarPoint{s*(.39f-.50f*(z-.10f)-depth),.24f+.095f*r*std::sin(a),z};
        };
        for(int i=0;i<b.segments/2;++i) {
            const float a=i*6.2831853f/(b.segments/2),c=(i+1)*6.2831853f/(b.segments/2);
            b.quad(mouth(1,a,0),mouth(.78f,a,0),mouth(.78f,c,0),mouth(1,c,0),red);
            b.quad(mouth(.78f,a,0),mouth(.65f,a,.10f),mouth(.65f,c,.10f),mouth(.78f,c,0),graphite);
            b.quad(mouth(0,a,.10f),mouth(.65f,a,.10f),mouth(.65f,c,.10f),mouth(0,a,.10f),0x1082);
        }
        b.part=CarPart::RearWing;
        // Three true slots on each rolled shoulder: no dark paint masquerading
        // as holes, and no rear face sealing them again from the opposite view.
        const auto wing=[s](float u,float v,float inner=0.f) {
            const float x=u<.35f ? .34f+u*.20f : .41f+(u-.35f)*(.09f/.65f);
            const float y=u<.35f ? .575f-u*.043f : .56f-(u-.35f)*(.20f/.65f);
            return CarPoint{s*(x-inner*.012f),y-.015f*v-inner*.008f,-.94f+.24f*v};
        };
        constexpr float us[]={0,.35f,.68f,.88f,1};
        constexpr float vs[]={0,.12f,.29f,.39f,.56f,.66f,.83f,1};
        for(int u=0;u<4;++u)for(int v=0;v<7;++v) {
            if(u==2 && (v==1 || v==3 || v==5))continue;
            b.quad(wing(us[u],vs[v]),wing(us[u+1],vs[v]),
                   wing(us[u+1],vs[v+1]),wing(us[u],vs[v+1]),red);
        }
        for(int v:{1,3,5}) {
            const float lo=vs[v],hi=vs[v+1];
            for(float u:{.68f,.88f})b.quad(wing(u,lo),wing(u,hi),wing(u,hi,1),wing(u,lo,1),shade(red,.7f));
            for(float z:{lo,hi})b.quad(wing(.68f,z),wing(.88f,z),wing(.88f,z,1),wing(.68f,z,1),shade(red,.7f));
        }
    }
    b.part=CarPart::RearWing;
    b.chine({{-.94f,.34f,.545f,.575f,.585f},{-.70f,.34f,.53f,.56f,.572f}},red,CarPaint::DiospadaWing);
    b.part=CarPart::FrontBridge;
    b.chine({{.81f,.45f,.09f,.16f,.17f},{.88f,.41f,.085f,.12f,.135f}},red,CarPaint::Solid);
}
} // namespace

CarSurfaceBuildResult buildCarSurfaceInto(CarId car,CarPanel* panels,std::size_t capacity,
                                         CarSurfaceDetail detail) {
    car=carSpec(car).id;
    MeshWriter mesh{{panels,panels ? capacity : 0}};
    Builder b{mesh,detail==CarSurfaceDetail::High ? 24 : detail==CarSurfaceDetail::Medium ? 18 :
                   detail==CarSurfaceDetail::Minimal ? 8 : 12};
    const auto& spec=carSpec(car);
    b.part=CarPart::Chassis;
    const auto chassis=car==CarId::SpinCobra ? white : car==CarId::BeakSpider || car==CarId::RayStinger ? blue : graphite;
    b.box(-.245f,.245f,.05f,.105f,-.79f,.83f,chassis);
    // Contoured bumper stays instead of a rectangular full-width plank.
    for(float s : {-1.f,1.f}) {
        b.part=CarPart::Chassis;
        b.quad({0,.10f,.80f},{s*.48f,.10f,.83f},{s*.58f,.10f,.94f},{0,.10f,.91f},chassis);
        if(car!=CarId::BeakSpider)
            b.quad({0,.10f,-.74f},{s*.49f,.10f,-.77f},{s*.57f,.10f,-.86f},{0,.10f,-.83f},chassis);
        if(car==CarId::RayStinger)
            b.box(std::min(s*.23f,s*.57f),std::max(s*.23f,s*.57f),.10f,.13f,.04f,.10f,chassis);
        b.part=CarPart::Wheel;
        const bool broad=car==CarId::CycloneMagnum || car==CarId::HurricaneSonic;
        const int spokes=car==CarId::BrockenGigant ? 6 : car==CarId::SpinCobra ? 3 : 5;
        const bool dish=car==CarId::BeakSpider;
        b.wheel(s,kModelFrontAxle,spec.wheelColor,car==CarId::NeoTridaggerZmc,s<0 ? 1 : 2,broad,dish,spokes);
        b.wheel(s,kModelRearAxle,spec.wheelColor,false,s<0 ? 3 : 4,broad,dish || car==CarId::NeoTridaggerZmc,spokes);
        b.part=CarPart::Roller;
        const auto roller=car>=CarId::SpinCobra ? (car==CarId::BeakSpider ? blue : silver) :
            car==CarId::CycloneMagnum || car==CarId::NeoTridaggerZmc ? blue : red;
        if(car==CarId::BeakSpider || car==CarId::RayStinger)b.roller(s*.55f,.90f,roller,1,.18f,.024f);
        else b.roller(s*.55f,.90f,roller);
        if(car==CarId::RayStinger) {
            b.roller(s*.55f,.07f,silver,2,.14f,.025f);
            b.roller(s*.55f,-.84f,silver,1,.18f,.025f);
        }
        else if(car==CarId::BeakSpider)b.roller(s*.55f,-.105f,0x2495,1,.195f,.024f);
        else if(car==CarId::NeoTridaggerZmc)b.roller(s*.55f,-.105f,0x246d,1,.195f);
        else if(car==CarId::BrockenGigant)b.roller(s*.55f,-.84f,red,1,.20f,.09f);
        else b.roller(s*.55f,-.84f,car>=CarId::SpinCobra ? roller : car==CarId::CycloneMagnum ? blue : red);
    }
    b.part=CarPart::Unspecified;
    switch(car) {
        case CarId::CycloneMagnum:cyclone(b);break;
        case CarId::HurricaneSonic:sonic(b);break;
        case CarId::NeoTridaggerZmc:neo(b);break;
        case CarId::BrockenGigant:brocken(b);break;
        case CarId::SpinCobra:cobra(b);break;
        case CarId::BeakSpider:spider(b);break;
        case CarId::RayStinger:stinger(b);break;
        case CarId::Diospada:diospada(b);break;
        default:cyclone(b);break;
    }
    return {mesh.count,mesh.overflowed};
}
void buildCarDisplayMesh(CarId car,CarDisplayMesh& mesh,CarSurfaceDetail detail) {
    const auto result=buildCarSurfaceInto(car,mesh.panels.data(),mesh.panels.size(),detail);
    mesh.count=result.count;mesh.overflowed=result.overflowed;
}
CarPoint animateCarPanelPoint(CarPoint p,uint8_t wheel,float cosine,float sine) {
    if(!wheel)return p;
    const float axle=wheel<=2 ? kModelFrontAxle : kModelRearAxle;
    const float y=p.y-kModelWheelRadius,z=p.z-axle;
    p.y=kModelWheelRadius+y*cosine-z*sine;p.z=axle+y*sine+z*cosine;return p;
}
} // namespace lets_and_go
