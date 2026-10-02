// Physics queries for Lua (PhysicsRay, PhysicsSweepBox, PhysicsOverlapBox), in the game's own terms: no physics engine type
// appears here, so the backend behind them (GGPhysicsQuery_Bullet.cpp now; Jolt's NarrowPhaseQuery if the physics moves)
// can change without any script changing. Positions and sizes are in the game's units, yaw in degrees about Y as an
// object's angle Y is.

#ifndef _H_GGPHYSICSQUERY
#define _H_GGPHYSICSQUERY

// which bodies a query meets; they add up
#define PHYSICS_LAYER_TERRAIN 1 // the ground, and entities built as terrain colliders
#define PHYSICS_LAYER_STATIC 2 // static entities
#define PHYSICS_LAYER_CHARACTER 4 // character capsules
#define PHYSICS_LAYER_DYNAMIC 8 // moving bodies
#define PHYSICS_LAYER_DEFAULT (PHYSICS_LAYER_TERRAIN | PHYSICS_LAYER_STATIC | PHYSICS_LAYER_DYNAMIC)
#define PHYSICS_QUERY_MAX_IGNORE 8

struct PhysicsHit
{
	bool hit;
	float fraction; // how far along the ray or the motion, 0 to 1
	float point[3];
	float normal[3];
	int object; // the object number of what was hit; 0 the ground, -1 a character capsule or a body with no object
};

// the nearest body the segment from pFrom to pTo meets. Jolt: NarrowPhaseQuery::CastRay, the layers as an object layer
// filter and the ignore list as a BodyFilter
bool PhysicsQuery_Ray(const float* pFrom, const float* pTo, int iLayers, const int* pIgnore, int iIgnoreCount, PhysicsHit* pHit);

// the first body a box (centre, half sizes, yaw) meets as it moves by pMotion. Jolt: NarrowPhaseQuery::CastShape with a
// BoxShape
bool PhysicsQuery_SweepBox(const float* pCentre, const float* pHalf, float fYawDegrees, const float* pMotion, int iLayers, const int* pIgnore, int iIgnoreCount, PhysicsHit* pHit);

// the bodies a box touches now: their object numbers in pObjects, each once, and how many (at most iMax). Jolt:
// NarrowPhaseQuery::CollideShape with a BoxShape
int PhysicsQuery_OverlapBox(const float* pCentre, const float* pHalf, float fYawDegrees, int iLayers, const int* pIgnore, int iIgnoreCount, int* pObjects, int iMax);

// what the physics did in its last step, to find where its time goes. Jolt: the PhysicsSystem's active body and contact counts
struct PhysicsStats
{
	int subSteps; // simulation steps taken in the last update
	int awakeBodies; // bodies the simulation is moving or settling
	int manifolds; // body pairs in contact
	int contactPoints; // their contact points
	int busiestObject; // the object with the most contact points, the ground and character capsules left out; -1 none
	int busiestPoints;
};
void PhysicsQuery_Stats(PhysicsStats* pStats);

// the bodies with the most contact points in the last step, most first (the ground and character capsules left out, as in
// PhysicsQuery_Stats); how many were written, at most iMax. Jolt: the contact listener's counts per body, and for the
// sleep fields its bodies' IsActive and motion properties (Jolt has no islands to report)
struct PhysicsStatsBody
{
	int object; // -1 unknown
	int points; // its contact points over all its pairs
	bool awake;
	float speed; // linear, in units a second
	int layer; // its PHYSICS_LAYER_ group bits
	float spin; // angular, in radians a second
	int state; // 1 active, 2 asleep, 3 ready to sleep (its island isn't), 4 kept awake (always active), 5 not simulated
	float still; // seconds it has been under its sleep speeds; it sleeps after 2, once every body in its island is ready
	float sleepSpeed; // its sleep speeds: linear, units a second
	float sleepSpin; // and angular, radians a second
	int island; // the bodies touching it, directly or through others, share its island and sleep together; -1 none
};
int PhysicsQuery_StatsTop(PhysicsStatsBody* pBodies, int iMax);

// puts an object's moving body to sleep with every moving body in its island (those touching it, directly or through
// others), their velocities zeroed: a body put to sleep alone is woken again at the next step while any body in its
// island is awake. They wake as any sleeping body does (a force, a velocity set, an awake body touching them). How many
// bodies slept; 0 when the object has no moving body, or its island holds a character capsule, a body kept awake or a
// body still moving (faster than twice its sleep speeds: try again later). Jolt: BodyInterface::DeactivateBody on the
// body and the moving bodies in contact with it
int PhysicsQuery_SleepIsland(int iObject);

#endif
