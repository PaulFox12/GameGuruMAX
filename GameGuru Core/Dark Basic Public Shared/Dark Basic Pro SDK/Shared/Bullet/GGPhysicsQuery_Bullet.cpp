// The Bullet backend of GGPhysicsQuery.h. It is the query layer's only file that includes Bullet, so a move to another
// physics engine replaces this file alone and no script or binding changes.

#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h"
#include "..\..\..\..\GameGuru\Include\GGPhysicsQuery.h"

extern btDiscreteDynamicsWorld* g_dynamicsWorld;
extern float gSc;
int ODEFindObjectNumberOfBody(const btCollisionObject* pObject);

// the object number a body stands for: the one its creation stored (setUserIndex), else found in the physics object list; the
// ground is 0, and a ghost (the player's capsule) or a body with no object -1
static int PhysicsQuery_ObjectNumber(const btCollisionObject* pObject, short iGroup)
{
	int iObject = pObject->getUserIndex();
	if (iObject >= 0) return iObject;
	if (iGroup & PHYSICS_LAYER_TERRAIN) return 0;
	if (!btRigidBody::upcast(pObject)) return -1;
	return ODEFindObjectNumberOfBody(pObject);
}

// the layers are the bodies' own collision groups (COL_TERRAIN, COL_OBJECT, COL_CAPSULECHAR, COL_OBJECT_DYNAMIC)
static bool PhysicsQuery_Wanted(btBroadphaseProxy* pProxy, int iLayers, const int* pIgnore, int iIgnoreCount)
{
	if (!(pProxy->m_collisionFilterGroup & iLayers)) return false;
	if (iIgnoreCount > 0)
	{
		int iObject = PhysicsQuery_ObjectNumber((const btCollisionObject*)pProxy->m_clientObject, pProxy->m_collisionFilterGroup);
		for (int i = 0; i < iIgnoreCount; i++)
		{
			if (iObject > 0 && pIgnore[i] == iObject) return false;
		}
	}
	return true;
}

static btTransform PhysicsQuery_BoxTransform(const float* pCentre, float fYawDegrees)
{
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(pCentre[0] / gSc, pCentre[1] / gSc, pCentre[2] / gSc));
	transform.setRotation(btQuaternion(btVector3(0, 1, 0), fYawDegrees * SIMD_RADS_PER_DEG));
	return transform;
}

struct PhysicsQueryRayCallback : public btCollisionWorld::ClosestRayResultCallback
{
	int iLayers;
	const int* pIgnore;
	int iIgnoreCount;
	PhysicsQueryRayCallback(const btVector3& from, const btVector3& to) : btCollisionWorld::ClosestRayResultCallback(from, to) {}
	virtual bool needsCollision(btBroadphaseProxy* pProxy) const
	{
		return PhysicsQuery_Wanted(pProxy, iLayers, pIgnore, iIgnoreCount);
	}
};

struct PhysicsQuerySweepCallback : public btCollisionWorld::ClosestConvexResultCallback
{
	int iLayers;
	const int* pIgnore;
	int iIgnoreCount;
	PhysicsQuerySweepCallback(const btVector3& from, const btVector3& to) : btCollisionWorld::ClosestConvexResultCallback(from, to) {}
	virtual bool needsCollision(btBroadphaseProxy* pProxy) const
	{
		return PhysicsQuery_Wanted(pProxy, iLayers, pIgnore, iIgnoreCount);
	}
};

// the bodies whose bounds meet the box's, in the wanted layers
struct PhysicsQueryAabbCallback : public btBroadphaseAabbCallback
{
	int iLayers;
	const int* pIgnore;
	int iIgnoreCount;
	btAlignedObjectArray<const btCollisionObject*> candidates;
	virtual bool process(const btBroadphaseProxy* pProxy)
	{
		if (PhysicsQuery_Wanted((btBroadphaseProxy*)pProxy, iLayers, pIgnore, iIgnoreCount)) candidates.push_back((const btCollisionObject*)pProxy->m_clientObject);
		return true;
	}
};

// set by any contact between the box and one convex or compound body
struct PhysicsQueryTouchCallback : public btCollisionWorld::ContactResultCallback
{
	bool bTouch;
	PhysicsQueryTouchCallback() : bTouch(false) {}
	virtual btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* pWrap0, int iPart0, int iIndex0, const btCollisionObjectWrapper* pWrap1, int iPart1, int iIndex1)
	{
		bTouch = true;
		return 0;
	}
};

// a triangle, in the box's own space, against the box: the separating axis test on the box's three axes, the triangle's
// normal and the nine edge cross products
static bool PhysicsQuery_BoxTriangle(const btVector3& half, const btVector3* v)
{
	for (int a = 0; a < 3; a++)
	{
		if (btMin(v[0][a], btMin(v[1][a], v[2][a])) > half[a]) return false;
		if (btMax(v[0][a], btMax(v[1][a], v[2][a])) < -half[a]) return false;
	}
	const btVector3 edges[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
	btVector3 axes[10];
	axes[0] = edges[0].cross(edges[1]);
	for (int i = 0; i < 3; i++)
	{
		axes[1 + i * 3] = btVector3(0, -edges[i].z(), edges[i].y());
		axes[2 + i * 3] = btVector3(edges[i].z(), 0, -edges[i].x());
		axes[3 + i * 3] = btVector3(-edges[i].y(), edges[i].x(), 0);
	}
	for (int i = 0; i < 10; i++)
	{
		const btVector3& axis = axes[i];
		btScalar p0 = axis.dot(v[0]), p1 = axis.dot(v[1]), p2 = axis.dot(v[2]);
		btScalar r = half.x() * btFabs(axis.x()) + half.y() * btFabs(axis.y()) + half.z() * btFabs(axis.z());
		if (btMin(p0, btMin(p1, p2)) > r || btMax(p0, btMax(p1, p2)) < -r) return false;
	}
	return true;
}

// a triangle mesh's triangles within the box's bounds, until one touches the box (Bullet's own contact test would build a
// full contact for every one of them)
struct PhysicsQueryBoxTriangleCallback : public btTriangleCallback
{
	btTransform shapeToBox;
	btVector3 half;
	bool bTouch;
	virtual void processTriangle(btVector3* pTriangle, int iPart, int iIndex)
	{
		if (bTouch) return;
		btVector3 v[3] = { shapeToBox(pTriangle[0]), shapeToBox(pTriangle[1]), shapeToBox(pTriangle[2]) };
		bTouch = PhysicsQuery_BoxTriangle(half, v);
	}
};

bool PhysicsQuery_Ray(const float* pFrom, const float* pTo, int iLayers, const int* pIgnore, int iIgnoreCount, PhysicsHit* pHit)
{
	pHit->hit = false;
	pHit->object = 0;
	if (!g_dynamicsWorld) return false;
	btVector3 from(pFrom[0] / gSc, pFrom[1] / gSc, pFrom[2] / gSc);
	btVector3 to(pTo[0] / gSc, pTo[1] / gSc, pTo[2] / gSc);
	PhysicsQueryRayCallback callback(from, to);
	callback.iLayers = iLayers;
	callback.pIgnore = pIgnore;
	callback.iIgnoreCount = iIgnoreCount;
	g_dynamicsWorld->rayTest(from, to, callback);
	if (!callback.hasHit()) return false;
	pHit->hit = true;
	pHit->fraction = callback.m_closestHitFraction;
	pHit->point[0] = callback.m_hitPointWorld.x() * gSc;
	pHit->point[1] = callback.m_hitPointWorld.y() * gSc;
	pHit->point[2] = callback.m_hitPointWorld.z() * gSc;
	pHit->normal[0] = callback.m_hitNormalWorld.x();
	pHit->normal[1] = callback.m_hitNormalWorld.y();
	pHit->normal[2] = callback.m_hitNormalWorld.z();
	const btBroadphaseProxy* pProxy = callback.m_collisionObject->getBroadphaseHandle();
	pHit->object = PhysicsQuery_ObjectNumber(callback.m_collisionObject, pProxy ? pProxy->m_collisionFilterGroup : 0);
	return true;
}

bool PhysicsQuery_SweepBox(const float* pCentre, const float* pHalf, float fYawDegrees, const float* pMotion, int iLayers, const int* pIgnore, int iIgnoreCount, PhysicsHit* pHit)
{
	pHit->hit = false;
	pHit->object = 0;
	if (!g_dynamicsWorld) return false;
	btBoxShape box(btVector3(fabsf(pHalf[0]) / gSc, fabsf(pHalf[1]) / gSc, fabsf(pHalf[2]) / gSc));
	btTransform from = PhysicsQuery_BoxTransform(pCentre, fYawDegrees);
	btTransform to = from;
	to.setOrigin(from.getOrigin() + btVector3(pMotion[0] / gSc, pMotion[1] / gSc, pMotion[2] / gSc));
	PhysicsQuerySweepCallback callback(from.getOrigin(), to.getOrigin());
	callback.iLayers = iLayers;
	callback.pIgnore = pIgnore;
	callback.iIgnoreCount = iIgnoreCount;
	g_dynamicsWorld->convexSweepTest(&box, from, to, callback);
	if (!callback.hasHit()) return false;
	pHit->hit = true;
	pHit->fraction = callback.m_closestHitFraction;
	pHit->point[0] = callback.m_hitPointWorld.x() * gSc;
	pHit->point[1] = callback.m_hitPointWorld.y() * gSc;
	pHit->point[2] = callback.m_hitPointWorld.z() * gSc;
	pHit->normal[0] = callback.m_hitNormalWorld.x();
	pHit->normal[1] = callback.m_hitNormalWorld.y();
	pHit->normal[2] = callback.m_hitNormalWorld.z();
	const btBroadphaseProxy* pProxy = callback.m_hitCollisionObject->getBroadphaseHandle();
	pHit->object = PhysicsQuery_ObjectNumber(callback.m_hitCollisionObject, pProxy ? pProxy->m_collisionFilterGroup : 0);
	return true;
}

int PhysicsQuery_OverlapBox(const float* pCentre, const float* pHalf, float fYawDegrees, int iLayers, const int* pIgnore, int iIgnoreCount, int* pObjects, int iMax)
{
	if (!g_dynamicsWorld) return 0;
	btBoxShape box(btVector3(fabsf(pHalf[0]) / gSc, fabsf(pHalf[1]) / gSc, fabsf(pHalf[2]) / gSc));
	btCollisionObject query;
	query.setCollisionShape(&box);
	query.setWorldTransform(PhysicsQuery_BoxTransform(pCentre, fYawDegrees));
	const btTransform& boxTransform = query.getWorldTransform();
	btVector3 aabbMin, aabbMax;
	box.getAabb(boxTransform, aabbMin, aabbMax);
	PhysicsQueryAabbCallback bodies;
	bodies.iLayers = iLayers;
	bodies.pIgnore = pIgnore;
	bodies.iIgnoreCount = iIgnoreCount;
	g_dynamicsWorld->getBroadphase()->aabbTest(aabbMin, aabbMax, bodies);
	int iCount = 0;
	for (int c = 0; c < bodies.candidates.size(); c++)
	{
		const btCollisionObject* pOther = bodies.candidates[c];
		const btCollisionShape* pShape = pOther->getCollisionShape();
		bool bTouch = false;
		if (pShape->isConcave())
		{
			PhysicsQueryBoxTriangleCallback triangles;
			triangles.shapeToBox = boxTransform.inverse() * pOther->getWorldTransform();
			triangles.half = box.getHalfExtentsWithMargin();
			triangles.bTouch = false;
			btVector3 localMin, localMax;
			box.getAabb(pOther->getWorldTransform().inverse() * boxTransform, localMin, localMax);
			((const btConcaveShape*)pShape)->processAllTriangles(&triangles, localMin, localMax);
			bTouch = triangles.bTouch;
		}
		else
		{
			PhysicsQueryTouchCallback touch;
			g_dynamicsWorld->contactPairTest(&query, (btCollisionObject*)pOther, touch);
			bTouch = touch.bTouch;
		}
		if (!bTouch) continue;

		// each object counts once
		const btBroadphaseProxy* pProxy = pOther->getBroadphaseHandle();
		int iObject = PhysicsQuery_ObjectNumber(pOther, pProxy ? pProxy->m_collisionFilterGroup : 0);
		bool bListed = false;
		for (int i = 0; i < iCount; i++)
		{
			if (pObjects[i] == iObject) bListed = true;
		}
		if (!bListed && iCount < iMax) pObjects[iCount++] = iObject;
	}
	return iCount;
}
