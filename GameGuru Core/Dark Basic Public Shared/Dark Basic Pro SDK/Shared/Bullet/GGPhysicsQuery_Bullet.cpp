// The Bullet backend of GGPhysicsQuery.h. It is the query layer's only file that includes Bullet, so a move to another
// physics engine replaces this file alone and no script or binding changes.

#include <unordered_map>
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h"
#include "BulletCollision/CollisionShapes/btTriangleShape.h"
#include "BulletCollision/NarrowPhaseCollision/btContinuousConvexCollision.h"
#include "BulletCollision/NarrowPhaseCollision/btGjkEpaPenetrationDepthSolver.h"
#include "BulletCollision/NarrowPhaseCollision/btVoronoiSimplexSolver.h"
#include "..\..\..\..\GameGuru\Include\GGPhysicsQuery.h"
#include <mutex>

extern btDiscreteDynamicsWorld* g_dynamicsWorld;
extern float gSc;
extern std::recursive_mutex physicslock;
extern int g_iPhysicsSubSteps;
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

// a triangle, in the box's own space, against the volume the box sweeps along motion: the separating axis test on the
// box's axes, the triangle's normal, the swept sides (motion x box axes) and the edge cross products with the box's axes
// and the motion
static bool PhysicsQuery_SweptBoxTriangle(const btVector3& half, const btVector3& motion, const btVector3* v)
{
	const btVector3 edges[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
	btVector3 axes[19];
	axes[0] = btVector3(1, 0, 0);
	axes[1] = btVector3(0, 1, 0);
	axes[2] = btVector3(0, 0, 1);
	axes[3] = edges[0].cross(edges[1]);
	for (int a = 0; a < 3; a++) axes[4 + a] = motion.cross(axes[a]);
	for (int i = 0; i < 3; i++)
	{
		axes[7 + i * 4] = btVector3(0, -edges[i].z(), edges[i].y());
		axes[8 + i * 4] = btVector3(edges[i].z(), 0, -edges[i].x());
		axes[9 + i * 4] = btVector3(-edges[i].y(), edges[i].x(), 0);
		axes[10 + i * 4] = motion.cross(edges[i]);
	}
	for (int i = 0; i < 19; i++)
	{
		const btVector3& axis = axes[i];
		btScalar p0 = axis.dot(v[0]), p1 = axis.dot(v[1]), p2 = axis.dot(v[2]);
		btScalar r = half.x() * btFabs(axis.x()) + half.y() * btFabs(axis.y()) + half.z() * btFabs(axis.z());
		btScalar m = axis.dot(motion);
		if (btMin(p0, btMin(p1, p2)) > btMax(btScalar(0), m) + r) return false;
		if (btMax(p0, btMax(p1, p2)) < btMin(btScalar(0), m) - r) return false;
	}
	return true;
}

// a triangle mesh's triangles along a box sweep. Bullet's own sweep casts the box at every one of them over the whole
// motion; here a triangle the box can't reach before the nearest hit so far is dropped by the swept test, and the rest
// are cast as Bullet casts them (btTriangleConvexcastCallback) but only as far as that hit
struct PhysicsQuerySweepTriangleCallback : public btTriangleCallback
{
	const btConvexShape* pBox;
	btTransform boxFrom;
	btVector3 motion;
	btTransform worldToBox;
	btTransform shapeToWorld;
	btVector3 half;
	btScalar fMargin;
	const btCollisionObject* pObject;
	btCollisionWorld::ClosestConvexResultCallback* pResult;
	virtual void processTriangle(btVector3* pTriangle, int iPart, int iIndex)
	{
		btScalar fBest = pResult->m_closestHitFraction;
		if (fBest <= 0) return;
		btVector3 v[3];
		for (int i = 0; i < 3; i++) v[i] = worldToBox(shapeToWorld(pTriangle[i]));
		if (!PhysicsQuery_SweptBoxTriangle(half, worldToBox.getBasis() * (motion * fBest), v)) return;
		btTriangleShape triangle(pTriangle[0], pTriangle[1], pTriangle[2]);
		triangle.setMargin(fMargin);
		btVoronoiSimplexSolver simplex;
		btGjkEpaPenetrationDepthSolver epa;
		btContinuousConvexCollision caster(pBox, &triangle, &simplex, &epa);
		btConvexCast::CastResult cast;
		cast.m_fraction = 1;
		cast.m_allowedPenetration = 0;
		btTransform boxTo = boxFrom;
		boxTo.setOrigin(boxFrom.getOrigin() + motion * fBest);
		if (!caster.calcTimeOfImpact(boxFrom, boxTo, shapeToWorld, shapeToWorld, cast)) return;
		if (cast.m_normal.length2() <= btScalar(0.0001)) return;
		btScalar fFraction = cast.m_fraction * fBest;
		if (fFraction >= fBest) return;
		cast.m_normal.normalize();
		btCollisionWorld::LocalShapeInfo shapeInfo;
		shapeInfo.m_shapePart = iPart;
		shapeInfo.m_triangleIndex = iIndex;
		btCollisionWorld::LocalConvexResult result(pObject, &shapeInfo, cast.m_normal, cast.m_hitPoint, fFraction);
		pResult->addSingleResult(result, true);
	}
};

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

	// the bodies whose bounds meet the swept box's; triangle meshes by the swept test below, the rest as Bullet sweeps them
	btVector3 aabbMin, aabbMax, toMin, toMax;
	box.getAabb(from, aabbMin, aabbMax);
	box.getAabb(to, toMin, toMax);
	aabbMin.setMin(toMin);
	aabbMax.setMax(toMax);
	PhysicsQueryAabbCallback bodies;
	bodies.iLayers = iLayers;
	bodies.pIgnore = pIgnore;
	bodies.iIgnoreCount = iIgnoreCount;
	g_dynamicsWorld->getBroadphase()->aabbTest(aabbMin, aabbMax, bodies);
	btVector3 motion = to.getOrigin() - from.getOrigin();
	for (int c = 0; c < bodies.candidates.size() && callback.m_closestHitFraction > 0; c++)
	{
		btCollisionObject* pOther = (btCollisionObject*)bodies.candidates[c];
		const btCollisionShape* pShape = pOther->getCollisionShape();
		const btTransform& shapeToWorld = pOther->getWorldTransform();
		if (pShape->getShapeType() == TRIANGLE_MESH_SHAPE_PROXYTYPE)
		{
			PhysicsQuerySweepTriangleCallback triangles;
			triangles.pBox = &box;
			triangles.boxFrom = from;
			triangles.motion = motion;
			triangles.worldToBox = from.inverse();
			triangles.shapeToWorld = shapeToWorld;
			triangles.half = box.getHalfExtentsWithMargin() + btVector3(0.01f, 0.01f, 0.01f);
			triangles.fMargin = pShape->getMargin();
			triangles.pObject = pOther;
			triangles.pResult = &callback;
			btTransform worldToShape = shapeToWorld.inverse();
			btVector3 boxMinLocal, boxMaxLocal;
			box.getAabb(btTransform(worldToShape.getBasis() * from.getBasis()), boxMinLocal, boxMaxLocal);
			((btBvhTriangleMeshShape*)pShape)->performConvexcast(&triangles, worldToShape * from.getOrigin(), worldToShape * to.getOrigin(), boxMinLocal, boxMaxLocal);
		}
		else
		{
			btCollisionWorld::objectQuerySingle(&box, from, to, pOther, pShape, shapeToWorld, callback, 0);
		}
	}
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

void PhysicsQuery_Stats(PhysicsStats* pStats)
{
	pStats->subSteps = 0;
	pStats->awakeBodies = 0;
	pStats->manifolds = 0;
	pStats->contactPoints = 0;
	pStats->busiestObject = -1;
	pStats->busiestPoints = 0;
	if (!g_dynamicsWorld) return;
	physicslock.lock();
	pStats->subSteps = g_iPhysicsSubSteps;
	const btCollisionObjectArray& objects = g_dynamicsWorld->getCollisionObjectArray();
	for (int i = 0; i < objects.size(); i++)
	{
		if (!objects[i]->isStaticObject() && objects[i]->isActive()) pStats->awakeBodies++;
	}

	// the manifolds left by the last sub-step; a body's points summed over all its pairs, the ground's and the character
	// capsules' not counted, so what a capsule pushes against is the one named
	btDispatcher* pDispatcher = g_dynamicsWorld->getDispatcher();
	btAlignedObjectArray<const btCollisionObject*> bodies;
	btAlignedObjectArray<int> points;
	for (int m = 0; m < pDispatcher->getNumManifolds(); m++)
	{
		const btPersistentManifold* pManifold = pDispatcher->getManifoldByIndexInternal(m);
		int iPoints = pManifold->getNumContacts();
		if (iPoints == 0) continue;
		pStats->manifolds++;
		pStats->contactPoints += iPoints;
		const btCollisionObject* pPair[2] = { pManifold->getBody0(), pManifold->getBody1() };
		for (int b = 0; b < 2; b++)
		{
			const btBroadphaseProxy* pProxy = pPair[b]->getBroadphaseHandle();
			if (pProxy && (pProxy->m_collisionFilterGroup & (PHYSICS_LAYER_TERRAIN | PHYSICS_LAYER_CHARACTER))) continue;
			int iBody = bodies.findLinearSearch(pPair[b]);
			if (iBody == bodies.size())
			{
				bodies.push_back(pPair[b]);
				points.push_back(0);
			}
			points[iBody] += iPoints;
		}
	}
	int iBusiest = -1;
	for (int i = 0; i < bodies.size(); i++)
	{
		if (iBusiest < 0 || points[i] > points[iBusiest]) iBusiest = i;
	}
	if (iBusiest >= 0)
	{
		const btBroadphaseProxy* pProxy = bodies[iBusiest]->getBroadphaseHandle();
		pStats->busiestObject = PhysicsQuery_ObjectNumber(bodies[iBusiest], pProxy ? pProxy->m_collisionFilterGroup : 0);
		pStats->busiestPoints = points[iBusiest];
	}
	physicslock.unlock();
}

int PhysicsQuery_StatsTop(PhysicsStatsBody* pBodies, int iMax)
{
	if (!g_dynamicsWorld || iMax <= 0) return 0;
	physicslock.lock();

	// the same sums as PhysicsQuery_Stats: a body's points over all its pairs from the last sub-step
	btDispatcher* pDispatcher = g_dynamicsWorld->getDispatcher();
	btAlignedObjectArray<const btCollisionObject*> bodies;
	btAlignedObjectArray<int> points;
	for (int m = 0; m < pDispatcher->getNumManifolds(); m++)
	{
		const btPersistentManifold* pManifold = pDispatcher->getManifoldByIndexInternal(m);
		int iPoints = pManifold->getNumContacts();
		if (iPoints == 0) continue;
		const btCollisionObject* pPair[2] = { pManifold->getBody0(), pManifold->getBody1() };
		for (int b = 0; b < 2; b++)
		{
			const btBroadphaseProxy* pProxy = pPair[b]->getBroadphaseHandle();
			if (pProxy && (pProxy->m_collisionFilterGroup & (PHYSICS_LAYER_TERRAIN | PHYSICS_LAYER_CHARACTER))) continue;
			int iBody = bodies.findLinearSearch(pPair[b]);
			if (iBody == bodies.size())
			{
				bodies.push_back(pPair[b]);
				points.push_back(0);
			}
			points[iBody] += iPoints;
		}
	}

	// the busiest first, picked one at a time (iMax is small)
	int iCount = 0;
	while (iCount < iMax)
	{
		int iBest = -1;
		for (int i = 0; i < bodies.size(); i++)
		{
			if (points[i] > 0 && (iBest < 0 || points[i] > points[iBest])) iBest = i;
		}
		if (iBest < 0) break;
		const btCollisionObject* pBody = bodies[iBest];
		const btBroadphaseProxy* pProxy = pBody->getBroadphaseHandle();
		short iGroup = pProxy ? pProxy->m_collisionFilterGroup : 0;
		PhysicsStatsBody& out = pBodies[iCount++];
		out.object = PhysicsQuery_ObjectNumber(pBody, iGroup);
		out.points = points[iBest];
		out.awake = pBody->isActive();
		const btRigidBody* pRigid = btRigidBody::upcast(pBody);
		// the world runs in units divided by gSc
		out.speed = pRigid ? pRigid->getLinearVelocity().length() * gSc : 0.0f;
		out.layer = iGroup;
		out.spin = pRigid ? pRigid->getAngularVelocity().length() : 0.0f;
		out.state = pBody->getActivationState();
		out.still = pBody->getDeactivationTime();
		out.sleepSpeed = pRigid ? pRigid->getLinearSleepingThreshold() * gSc : 0.0f;
		out.sleepSpin = pRigid ? pRigid->getAngularSleepingThreshold() : 0.0f;
		out.island = pBody->getIslandTag();
		points[iBest] = 0;
	}
	physicslock.unlock();
	return iCount;
}

void PhysicsQuery_BodiesAtRest(const int* pObjects, int iCount, float fStill, float fFastSpeed, int* pStates)
{
	for (int i = 0; i < iCount; i++) pStates[i] = 0;
	if (!g_dynamicsWorld || iCount <= 0) return;
	std::unordered_map<int, int> wanted;
	for (int i = 0; i < iCount; i++) wanted[pObjects[i]] = i;
	physicslock.lock();
	const btCollisionObjectArray& objects = g_dynamicsWorld->getCollisionObjectArray();
	for (int o = 0; o < objects.size(); o++)
	{
		btRigidBody* pRigid = btRigidBody::upcast(objects[o]);
		const btBroadphaseProxy* pProxy = objects[o]->getBroadphaseHandle();
		if (!pRigid || !pProxy || pRigid->isStaticOrKinematicObject()) continue;
		auto it = wanted.find(PhysicsQuery_ObjectNumber(pRigid, pProxy->m_collisionFilterGroup));
		if (it == wanted.end()) continue;
		// the world runs in units divided by gSc
		const float fSpeed = pRigid->getLinearVelocity().length() * gSc;
		if (!pRigid->isActive() || pRigid->getDeactivationTime() >= fStill) pStates[it->second] = 2;
		else if (fSpeed > fFastSpeed) pStates[it->second] = 3;
		else pStates[it->second] = 1;
	}
	physicslock.unlock();
}

int PhysicsQuery_SleepIsland(int iObject)
{
	if (!g_dynamicsWorld || iObject <= 0) return 0;
	physicslock.lock();
	const btCollisionObjectArray& objects = g_dynamicsWorld->getCollisionObjectArray();

	// the object's moving body
	btRigidBody* pBody = nullptr;
	for (int i = 0; i < objects.size() && !pBody; i++)
	{
		btRigidBody* pRigid = btRigidBody::upcast(objects[i]);
		const btBroadphaseProxy* pProxy = objects[i]->getBroadphaseHandle();
		if (!pRigid || !pProxy || pRigid->isStaticOrKinematicObject()) continue;
		if (PhysicsQuery_ObjectNumber(pRigid, pProxy->m_collisionFilterGroup) == iObject) pBody = pRigid;
	}
	if (!pBody)
	{
		physicslock.unlock();
		return 0;
	}

	// its island's moving bodies (the islands of the last step), checked before any is put to sleep
	int iIsland = pBody->getIslandTag();
	btAlignedObjectArray<btRigidBody*> island;
	bool bCanSleep = true;
	for (int i = 0; i < objects.size() && bCanSleep; i++)
	{
		btRigidBody* pRigid = btRigidBody::upcast(objects[i]);
		if (!pRigid || pRigid->isStaticOrKinematicObject()) continue;
		if (pRigid != pBody && (iIsland < 0 || pRigid->getIslandTag() != iIsland)) continue;
		const btBroadphaseProxy* pProxy = pRigid->getBroadphaseHandle();
		if (pProxy && (pProxy->m_collisionFilterGroup & PHYSICS_LAYER_CHARACTER)) bCanSleep = false;
		if (pRigid->getActivationState() == DISABLE_DEACTIVATION || pRigid->getActivationState() == DISABLE_SIMULATION) bCanSleep = false;
		if (pRigid->getLinearVelocity().length() > pRigid->getLinearSleepingThreshold() * 2) bCanSleep = false;
		if (pRigid->getAngularVelocity().length() > pRigid->getAngularSleepingThreshold() * 2) bCanSleep = false;
		island.push_back(pRigid);
	}
	int iCount = 0;
	if (bCanSleep)
	{
		for (int i = 0; i < island.size(); i++)
		{
			island[i]->setLinearVelocity(btVector3(0, 0, 0));
			island[i]->setAngularVelocity(btVector3(0, 0, 0));
			island[i]->clearForces();
			island[i]->forceActivationState(ISLAND_SLEEPING);
			iCount++;
		}
	}
	physicslock.unlock();
	return iCount;
}
