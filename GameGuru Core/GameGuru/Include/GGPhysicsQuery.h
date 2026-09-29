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

#endif
