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

struct PhysicsQueryOverlapCallback : public btCollisionWorld::ContactResultCallback
{
	int iLayers;
	const int* pIgnore;
	int iIgnoreCount;
	const btCollisionObject* pSelf;
	int* pObjects;
	int iMax;
	int iCount;
	virtual bool needsCollision(btBroadphaseProxy* pProxy) const
	{
		return PhysicsQuery_Wanted(pProxy, iLayers, pIgnore, iIgnoreCount);
	}
	virtual btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* pWrap0, int iPart0, int iIndex0, const btCollisionObjectWrapper* pWrap1, int iPart1, int iIndex1)
	{
		// the other body is whichever side isn't the query's box; each one counts once
		const btCollisionObject* pOther = pWrap0->getCollisionObject() == pSelf ? pWrap1->getCollisionObject() : pWrap0->getCollisionObject();
		const btBroadphaseProxy* pProxy = pOther->getBroadphaseHandle();
		int iObject = PhysicsQuery_ObjectNumber(pOther, pProxy ? pProxy->m_collisionFilterGroup : 0);
		for (int i = 0; i < iCount; i++)
		{
			if (pObjects[i] == iObject) return 0;
		}
		if (iCount < iMax) pObjects[iCount++] = iObject;
		return 0;
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
	PhysicsQueryOverlapCallback callback;
	callback.iLayers = iLayers;
	callback.pIgnore = pIgnore;
	callback.iIgnoreCount = iIgnoreCount;
	callback.pSelf = &query;
	callback.pObjects = pObjects;
	callback.iMax = iMax;
	callback.iCount = 0;
	g_dynamicsWorld->contactTest(&query, callback);
	return callback.iCount;
}
