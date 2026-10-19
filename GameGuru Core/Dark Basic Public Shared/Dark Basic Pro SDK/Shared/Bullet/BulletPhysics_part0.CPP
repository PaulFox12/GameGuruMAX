// BulletPhysics PHYSICS COMMANDS

//#define FASTBULLETPHYSICS
#define PUTPHYSICSBACKINMAINTHREADSEQUENCE
#pragma warning( disable: 4312 ) // conversion from 'int' to 'void *' of greater size

// Includes
#include "BulletPhysics.h"
#include <stack>
#include <stdio.h>
#include <math.h>
#include <mutex>
#include <unordered_map>
#include <string>

// Bullet specific includes
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h"
#include "BulletDynamics/Character/btKinematicCharacterController.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "../Extras/Serialize/BulletWorldImporter/btBulletWorldImporter.h"
#include "GGTerrainPhysicsShape.h"

// Ragdoll
#include "Ragdoll/DBPro/Include/DBPro.hpp"
#include "Ragdoll/DBProToBullet.h"
#include "Ragdoll/BaseItem.h"
#include "Ragdoll/BaseItemManager.h"
#include "Ragdoll/DBProRagDollBone.h"
#include "Ragdoll/DBProRagDoll.h"
#include "Ragdoll/DBProRagDollManager.h"
#include "Ragdoll/DBProJointManager.h"

//DM
#include "LinearMath/btAlignedObjectArray.h"
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btAabbUtil2.h"
#include "BulletCollision/BroadphaseCollision/btOverlappingPairCache.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"

//PE
#include "BulletCollision\CollisionShapes\btConvexHullShape.h"
#include "BulletCollision\CollisionShapes\btShapeHull.h"
#include "LinearMath/btConvexHull.h"
//End PE

#ifdef WICKEDENGINE
//ZJ
#include "..\..\..\..\Guru-WickedMAX\Wicked-MAX\BulletDebugDrawer.h"
//End ZJ
#endif

//LB
#ifdef WICKEDENGINE
#include "..\Extras\VHACD\public\VHACD.h"
#endif

// access to other LIBs
#include "CObjectsC.h"
#include "CFileC.h"

//#include ".\..\..\..\..\Guru-WickedMAX\wickedcalls.h" DX12 removal

//Dave
void BULLETAnglesFromMatrix ( GGMATRIX* pmatMatrix, GGVECTOR3* pVecAngles );

btCompoundShape*	pImporterCompoundShape;
int					pImporterMainObjectID;
char				g_ProvidedMeshFilenameForBNVHSaving[MAX_PATH];
char				g_ProvidedMeshFilenameForOBJLoading[MAX_PATH];

void DeleteallImporters(); //PE: We need to remove mem used by importers.

// externals to ragdoll system
extern DBProJointManager* jointManager;
extern DBProRagdollManager* ragdollManager;

// Constants
#define BIT(x) (1<<(x))
enum collisiontypes {
    COL_NOTHING = 0, //<Collide with nothing
    COL_TERRAIN = BIT(0), //<Collide with terrain
    COL_OBJECT = BIT(1), //<Collide with static object
	COL_CAPSULECHAR = BIT(2), //<Collide with capsule(character)
	COL_OBJECT_DYNAMIC = BIT(3), //<Collide with dynamic object
};

//PE: Debug stuff.
bool bPlayerShouldbeMoving = false;
int iPlayerStuckErrors = 0;
float fPlayerLastGoodX, fPlayerLastGoodY, fPlayerLastGoodZ;

// World Scaling (turns 1 unit = 1 inch to 1 unit = 1 meter) (75 inches high character becomes 1.8 high in Bullet)
float gSc = 40.0f;// 40.0f;

// Mass Scaling (to produce a mass value from the volume dimensions of the object)
// float gfVolumeReducer = 100000.0f; // not used
const float maxDynamicVolume = 20000.0f;
float gMs = 50.0f;

// Capsule default height
const float g_fCapsuleHeight = 45.0f;

// Global gravity values
float fCharacterGravity = 900.0f;
float fCharacterFallSpeed = 5000.0f;
float fCharacterMaxSlope = 70.0f;

// Water Line handling
#ifdef WICKEDENGINWE
float g_fWaterLineY = 0.0f;
#else
float g_fWaterLineY = 500.0f;
#endif

bool zero_gravity = false;
bool physics_playground = false;

// Globals
extern GlobStruct* g_pGlob;
char BULLETerrStr[256];
//int g_debugraycastvisual = 0;

void HandleError( char* szMessage )
{
	MessageBox( NULL, szMessage, "ODE Error", 0 );
	exit(0);
}

// Bullet globals
btAlignedObjectArray<btBulletWorldImporter*> g_importers;
btDiscreteDynamicsWorld* g_dynamicsWorld;
btAlignedObjectArray<btCollisionShape*> g_collisionShapes;
btAlignedObjectArray<btTriangleIndexVertexArray*> g_indexvertexarrays;
btDefaultCollisionConfiguration* g_collisionConfiguration;
btCollisionDispatcher* g_dispatcher;
btSequentialImpulseConstraintSolver* g_solver;
btPairCachingGhostObject* m_ghostObject = NULL;
btKinematicCharacterController* m_character = NULL;
// GG: SetPhysicsWorldRate(60): the world steps 60 times a second, half the work, while the player's controller, which
// moves a set distance each step, keeps stepping 120 times a second on its own; at 120 all of it steps together as before
bool g_bPhysicsWorldAt60 = false;
static btKinematicCharacterController* g_pCharacterOwnSteps = NULL;
static btScalar g_fCharacterLocalTime = 0;
static float g_fPhysicsFrameTime = 1.0f / 60.0f;

// GG: a one-off push (PushObject, a shot's ray force, a ragdoll hit): at a world rate of 60 an impulse, the force times
// the frame's time, as a frame that takes no world step clears its forces before any step applies them (stepSimulation's
// clearForces) and the push was lost; on average that is the push a force gave at 120, where a frame's steps add up to
// its time. At 120 a force as before
void PhysicsApplyPush(btRigidBody* pBody, const btVector3& force, const btVector3& relPos)
{
	if (!g_bPhysicsWorldAt60)
	{
		pBody->applyForce(force, relPos);
		return;
	}
	// a sleeping body takes no force, so it takes no impulse either
	if (pBody->isActive()) pBody->applyImpulse(force * g_fPhysicsFrameTime, relPos);
}
btBroadphaseInterface* sweepBP = NULL;
btBroadphaseInterface* m_overlappingPairCache = NULL;
#ifdef WICKEDENGINE
BulletDebugDrawer g_debugDrawer;
#endif
int g_CharacterControlObject = 0;

// ray detect globals
int g_hitObjectNumber = 0;
btVector3 g_hitPointWorld;
btVector3 g_hitNormalWorld;

// gore ptr direct from exe stub
extern GlobStruct* g_pGlob;

// Data to control physics system interaction with DBP
struct sObjectList
{
	int iID;
	btRigidBody* body;
	bool bDynamicUpdate;
	btDefaultMotionState* myMotionState;
	bool bCapsuleControl;
	bool bBouyant;
	LPVOID pMem1;
	LPVOID pMem2;
	float fScaled;
	float fRaised;
	btScalar fMass;
	int iResponseMode;
	btVector3 vecUnfrozenLinearFactor; // the factors a freeze (ODESetBodyResponse) replaced, put back on unfreeze
	btVector3 vecUnfrozenAngularFactor;
	bool bCentredOnCollisionBox; // the body sits at the object's collision box centre (box, sphere, cylinder, capsule), not its origin (CreateMesh)
	bool bPivotInBody; // the body was made turned by the object's pivot (an entity's fixnewy), so the drawn transform leaves the pivot out
};
std::vector < sObjectList > g_PhyObjectList;

// the object number of a body, for one made before its user index was set (GGPhysicsQuery's fallback); -1 if none
int ODEFindObjectNumberOfBody(const btCollisionObject* pObject)
{
	for (int j = (int)g_PhyObjectList.size() - 1; j >= 0; j--)
	{
		if (g_PhyObjectList[j].body == pObject) return g_PhyObjectList[j].iID;
	}
	return -1;
}

// Internal functions

void BULLETReceiveCoreDataPtr ( void )
{
}

void ODEAddObject ( int iID, sObject* pObject, btRigidBody* body, int iIsDynamic, btDefaultMotionState* myMotionState, 
	                bool bCapsuleMode = false, bool bBouyant = false, LPVOID pMem1 = NULL, LPVOID pMem2 = NULL,
	                float fScaled = 1.0f, float fRaised = 1.0f, float fFriction = 90.0f, btScalar fMass = -1.0f,
	                bool bCentredOnCollisionBox = true )
{
	// add object to list
	sObjectList object;
	object.iID = iID;
	object.bCentredOnCollisionBox = bCentredOnCollisionBox;
	object.bPivotInBody = false;

	object.body = body;
	if ( iIsDynamic==1 )
		object.bDynamicUpdate = true;
	else
		object.bDynamicUpdate = false;
	object.myMotionState = myMotionState;
	object.bCapsuleControl = bCapsuleMode;
	object.bBouyant = bBouyant;
	object.pMem1 = pMem1;
	object.pMem2 = pMem2;
	object.fScaled = fScaled;
	object.fRaised = fRaised;
	object.fMass = fMass;
	object.iResponseMode = 0;
	g_PhyObjectList.push_back ( object );

	// body settings
	if ( body )
	{
		// default body settings
		float fRealFriction = 0.9f;
		if (fFriction == -1) fFriction = 90.0f;

		// 200516 - set some safety caps for friction
		if (fFriction < 0.0f) fFriction = 0.0f;
		//if (fFriction > 100.0f) fFriction = 100.0f; no upper limt, can make physics objects sticky

		if (zero_gravity) 
		{
			body->setSleepingThresholds(0.10f, 0.0025f);

			// set physics friction
			fRealFriction = fFriction / 200.0f;
			body->setFriction(fRealFriction);
			body->setDamping(0.1f, 0.05f);

			// and mass
			object.fMass = fMass * btScalar(0.25);

			// submitted by Frank Boyle (stab in the dark software)
			if (body->getInvMass() > 0.0)
			{
				btVector3 bbDimensions;
				btScalar diameter;
				bbDimensions = ((btBoxShape*)body->getCollisionShape())->getImplicitShapeDimensions();
				diameter = bbDimensions.getX();
				diameter = bbDimensions.getY() > 0.0 && bbDimensions.getY() < diameter ? bbDimensions.getY() : diameter;
				diameter = bbDimensions.getZ() > 0.0 && bbDimensions.getZ() < diameter ? bbDimensions.getZ() : diameter;
				body->setCcdMotionThreshold(diameter / 8.0f);
				body->setCcdSweptSphereRadius(diameter);
			}
		}
		else 
		{
			body->setSleepingThresholds(1.0f, 0.25f);

			// set physics friction
			fRealFriction = fFriction / 100.0f;
			body->setFriction(fRealFriction);

			// for very small things, increase damping so they do not move forever (small tins)
			if (fMass < 1.0f)
			{
				// more damping on angular velocity, and sleep threshold
				body->setDamping(0.01f, 0.75f);
				body->setSleepingThresholds(1.0f, 0.5f);
			}
			else
			{
				body->setDamping(0.01f, 0.05f);
			}

			// submitted by Frank Boyle (stab in the dark software)
			if (body->getInvMass() > 0.0)
			{
				btVector3 bbDimensions;
				btScalar diameter;
				bbDimensions = ((btBoxShape*)body->getCollisionShape())->getImplicitShapeDimensions();
				diameter = bbDimensions.getX();
				diameter = bbDimensions.getY() > 0.0 && bbDimensions.getY() < diameter ? bbDimensions.getY() : diameter;
				diameter = bbDimensions.getZ() > 0.0 && bbDimensions.getZ() < diameter ? bbDimensions.getZ() : diameter;
				body->setCcdMotionThreshold(diameter / 2.0f);
				body->setCcdSweptSphereRadius(diameter);
			}
		}
	}
}

void ODERemoveObject ( int iID )
{
	int iDeleteIndex = -1;
	for ( int i = 0; i < (int)g_PhyObjectList.size ( ); i++ )
	{
		if ( g_PhyObjectList [ i ].iID == iID )
		{
			iDeleteIndex = i;
			break;
		}
	}
	if ( iDeleteIndex > -1 )
		g_PhyObjectList.erase ( g_PhyObjectList.begin() + iDeleteIndex );
}

sObjectList* ODEFindID ( int iID )
{
	for ( int i = 0; i < (int)g_PhyObjectList.size ( ); i++ )
	{
		if ( g_PhyObjectList [ i ].iID == iID )
			return &g_PhyObjectList [ i ];
	}
	return NULL;
}

// GG: kilos for an entity's body (SetEntityMass), kept by entity so a body built again (CollisionOn, a wreck swap) takes
// them again. Bullet's units are already kilos, metres and seconds: a Bullet unit is gSc (40) GG units, about a metre,
// gravity is 10, and a ragdoll's 175 at Weight 100 is a heavy man. A body built from its shape keeps the old formula, its
// box in Bullet units cubed over gMs (50), times its Weight over 100 for a box, sphere, cylinder or capsule
static std::unordered_map<int, float> g_EntityMassKg;

// GG: a diagnostic, one object's pushes and its body over the steps after each (SetPhysicsPushLog, GetPhysicsPushLog)
extern std::recursive_mutex physicslock;
extern int g_iPhysicsSubSteps;
static int g_iPushLogObject = 0;
static int g_iPushLogSteps = 8;
static int g_iPushLogStepsLeft = 0;
static std::vector<std::string> g_PushLog;

static void PhysicsPushLogBody(const char* pWhen)
{
	physicslock.lock();
	if (g_PushLog.size() < 256)
	{
		int iEntries = 0;
		for (int i = 0; i < (int)g_PhyObjectList.size(); i++) if (g_PhyObjectList[i].iID == g_iPushLogObject) iEntries++;
		sObjectList* pPhyObject = ODEFindID(g_iPushLogObject);
		char pLine[640];
		if (pPhyObject == NULL || pPhyObject->body == NULL)
		{
			snprintf(pLine, sizeof(pLine), "%s obj %d: no body (%d entries)", pWhen, g_iPushLogObject, iEntries);
		}
		else
		{
			btRigidBody* pBody = pPhyObject->body;
			const btVector3& v = pBody->getLinearVelocity();
			const btVector3& p = pBody->getWorldTransform().getOrigin();
			const btVector3& lf = pBody->getLinearFactor();
			snprintf(pLine, sizeof(pLine), "%s obj %d: body %p entries %d state %d active %d invmass %.5f vel %.1f %.1f %.1f spin %.2f pos %.1f %.1f %.1f linfactor %.2f %.2f %.2f damping %.3f %.3f still %.2f island %d flags %d response %d frame %.4f substeps %d",
				pWhen, g_iPushLogObject, (void*)pBody, iEntries, pBody->getActivationState(), pBody->isActive() ? 1 : 0,
				pBody->getInvMass(), v.getX() * gSc, v.getY() * gSc, v.getZ() * gSc, pBody->getAngularVelocity().length(),
				p.getX() * gSc, p.getY() * gSc, p.getZ() * gSc, lf.getX(), lf.getY(), lf.getZ(), pBody->getLinearDamping(),
				pBody->getAngularDamping(), pBody->getDeactivationTime(), pBody->getIslandTag(), pBody->getCollisionFlags(),
				pPhyObject->iResponseMode, g_fPhysicsFrameTime, g_iPhysicsSubSteps);
		}
		g_PushLog.push_back(pLine);
	}
	physicslock.unlock();
}

// each simulation step (PostTickCallback) after a logged push
static void PhysicsPushLogStep(void)
{
	if (g_iPushLogObject <= 0 || g_iPushLogStepsLeft <= 0) return;
	g_iPushLogStepsLeft--;
	PhysicsPushLogBody("step");
}

int ODEFind (int iID)
{
	if (ODEFindID(iID) != NULL)
		return 1;
	else
		return 0;
}

int ODEFindID( btRigidBody* body)
{
	for (int i = 0; i < (int)g_PhyObjectList.size(); i++)
	{
		if ( g_PhyObjectList[i].body == body )
			return g_PhyObjectList[i].iID;
	}
	return -1;
}

const int maxCollisions = 5;

struct LuaCollisionInfoDEF
{   int ObjectId;
	int numObjCollisions;
	int CollisionObjects[ maxCollisions ];
	float fX[ maxCollisions ], fY[ maxCollisions ], fZ[ maxCollisions ], fF[ maxCollisions ];
	int numTerCollisions;
	int latestTerCollision;
	float ftX[ maxCollisions ], ftY[ maxCollisions ], ftZ[ maxCollisions ];
};

std::vector< LuaCollisionInfoDEF > luaCollisionList;

void AddCollisionToList( const int A, const int B, float fX, float fY, float fZ, float fF )
{
	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while ( loc != luaCollisionList.end() )
	{
		if ( loc->ObjectId == A )
		{
			if ( B > 70000 )    // object on object collision  
				                // ( g.entityviewstartobj buit how do I access it? )
			{
				if ( loc->numObjCollisions >= maxCollisions ) return;
				for ( int i = 0; i < loc->numObjCollisions; i++ )
				{
					// check if we've already got an entry for this pairing
					// (we only record the first contact between the pair)
					if ( loc->CollisionObjects[i] == B ) return;
				}
				loc->CollisionObjects[ loc->numObjCollisions ] = B;
				loc->fX[ loc->numObjCollisions ] = fX;
				loc->fY[ loc->numObjCollisions ] = fY;
				loc->fZ[ loc->numObjCollisions ] = fZ;
				loc->fF[ loc->numObjCollisions ] = fF;
				loc->numObjCollisions++;
				return;
			}
			else if ( B >= 7000 ) // Assume it's a terrain collision
				                  // ( t.tphysicsterrainobjstart but how do I access it ? )
			{
				// just store cyclically and let caller sort out the order
				loc->ftX[ loc->latestTerCollision ] = fX;
				loc->ftY[ loc->latestTerCollision ] = fY;
				loc->ftZ[ loc->latestTerCollision ] = fZ;

				loc->latestTerCollision++;
				if ( loc->latestTerCollision >= maxCollisions )
				{
					loc->latestTerCollision = 0;
					loc->numTerCollisions = maxCollisions;
				}
				else if ( loc->numTerCollisions <  maxCollisions )
				{
					loc->numTerCollisions = loc->latestTerCollision;
				}
				return;
			}
		}
		loc++;
	}
}

void AddObjectCollisionCheck( const int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if ( pPhyObject == NULL ) return;

	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while ( loc != luaCollisionList.end() )
	{
		if ( loc->ObjectId == iObjectNumber )
		{
			// still in the list from previous request so simply set
			// index to start recording again
			loc->numObjCollisions = 0;
			loc->numTerCollisions = 0;
			loc->latestTerCollision = 0;
			return;   // already in list
		}
		loc++;
	}
	LuaCollisionInfoDEF luaCol;
	luaCol.ObjectId = iObjectNumber;
	luaCol.numObjCollisions = 0;
	luaCol.numTerCollisions = 0; 
	luaCol.latestTerCollision = 0;
	luaCollisionList.push_back( luaCol );
}

void RemoveObjectCollisionCheck(const int iObjectNumber)
{
	sObjectList* pPhyObject = ODEFindID(iObjectNumber);
	if (pPhyObject == NULL) return;

	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while (loc != luaCollisionList.end())
	{
		if (loc->ObjectId == iObjectNumber)
		{
			luaCollisionList.erase(loc);
			break;
		}
		loc++;
	}
}

void GetObjectCollisionDetails( const int iObjectNumber, const int iColNumber, int &iColObj, 
	                            float &fX, float &fY, float &fZ, float &fImpulse )
{
	iColObj = 0;
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if (pPhyObject == NULL) return;
	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while ( loc != luaCollisionList.end() )
	{
		if ( loc->ObjectId == iObjectNumber )
		{
			if ( iColNumber > 0 && iColNumber <= loc->numObjCollisions )
			{
				// if last collision requested reset count for next time
				if ( iColNumber == loc->numObjCollisions ) loc->numObjCollisions = 0;
				iColObj = loc->CollisionObjects[ iColNumber - 1 ];
				fX = loc->fX[ iColNumber - 1 ];
				fY = loc->fY[ iColNumber - 1 ];
				fZ = loc->fZ[ iColNumber - 1 ];
				fImpulse = loc->fF[ iColNumber - 1 ];

				return;
			}
			else if ( loc->numObjCollisions > 0 )
			{
				// if specified collision out of range return first 
				// contact in list
				iColObj = loc->CollisionObjects[ 0 ];
				fX = loc->fX[ 0 ];
				fY = loc->fY[ 0 ];
				fZ = loc->fZ[ 0 ];
				fImpulse = loc->fF[ iColNumber - 1 ];

				return;
			}
			else
			{
				return;
			}
		}
		loc++;
	}
}

void GetTerrainCollisionDetails( const int iObjectNumber, const int iColNumber, int &iLatest,
	                             float &fX, float &fY, float &fZ)
{
	iLatest = 0;
	sObjectList* pPhyObject = ODEFindID(iObjectNumber);
	if (pPhyObject == NULL) return;
	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while (loc != luaCollisionList.end())
	{
		if (loc->ObjectId == iObjectNumber)
		{
			if ( iColNumber > 0 && iColNumber <= loc->numTerCollisions )
			{
				iLatest = loc->latestTerCollision;
				// if last collision requested reset count for next time
				if ( iColNumber == loc->numTerCollisions )
				{
					loc->numTerCollisions = 0;
					loc->latestTerCollision = 0;
				}
				fX = loc->ftX[iColNumber - 1];
				fY = loc->ftY[iColNumber - 1];
				fZ = loc->ftZ[iColNumber - 1];

				return;
			}
			else if (loc->numTerCollisions > 0)
			{
				// if specified collision out of range return first 
				// contact in list
				iLatest = 1;
				fX = loc->ftX[ 0 ];
				fY = loc->ftY[ 0 ];
				fZ = loc->ftZ[ 0 ];

				return;
			}
			else
			{
				return;
			}
		}
		loc++;
	}
}

int GetObjectNumCollisions( const int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if ( pPhyObject == NULL ) return 0;
	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while ( loc != luaCollisionList.end() )
	{
		if ( loc->ObjectId == iObjectNumber )
		{
			return loc->numObjCollisions;
		}
		loc++;
	}
	// object not in list
	return 0;
}

int GetTerrainNumCollisions( const int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if ( pPhyObject == NULL ) return 0;
	std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
	while ( loc != luaCollisionList.end() )
	{
		if ( loc->ObjectId == iObjectNumber )
		{
			return loc->numTerCollisions;
		}
		loc++;
	}
	// object not in list
	return 0;
}


void UpdateCollisionsForLua()
{
	if ( luaCollisionList.empty()) { return; };

	const float scalefactor = 40.0;

    int numManifolds = g_dynamicsWorld->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++)
	{
		btPersistentManifold* contactManifold = g_dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
		const btCollisionObject* obA = contactManifold->getBody0();
		const btCollisionObject* obB = contactManifold->getBody1();

		const int objIdA = ODEFindID( (btRigidBody*)obA );
		const int objIdB = ODEFindID( (btRigidBody*)obB );

		float fXb = 0.0; float fYb = 0.0; float fZb = 0.0; float fF = 0.0;

		int numContacts = contactManifold->getNumContacts();
		if ( numContacts > 0 )
		for ( int j = 0; j < numContacts; j++ )
		{
			btManifoldPoint& pt = contactManifold->getContactPoint( j );

			if ( pt.getDistance() < 0.f )
			{
				const btVector3& ptB = pt.getPositionWorldOnB() * scalefactor;
				fXb = ptB.getX(); fYb = ptB.getY(); fZb = ptB.getZ();
				fF = -pt.getDistance();
			}
		}

		if ( fXb != 0.0 )  // only add real contacts to list
		{

			std::vector< LuaCollisionInfoDEF >::iterator loc = luaCollisionList.begin();
			while ( loc != luaCollisionList.end() )
			{
				if ( objIdA == loc->ObjectId )
				{
					AddCollisionToList( objIdA, objIdB, fXb, fYb, fZb, fF );
				}
				else if ( objIdB == loc->ObjectId )
				{
					AddCollisionToList( objIdB, objIdA, fXb, fYb, fZb, fF );
				}
				loc++;
			}
		}
	}
}


// GG: projectiles that end on contact (ODEWatchProjectileContact): the first contact of each one's body, found after the
// simulation step it happened in, before the projectile is drawn rebounding from it
struct sProjectileContact
{
	int iObject;
	int iIgnoreObject;
	btRigidBody* body;
	int iTouched; // -1 none yet, else the object number of the body touched (0 the terrain or a body with no object)
	btVector3 vecPoint; // world units
	btVector3 vecNormal; // the touched surface's, facing the projectile
};
static std::vector<sProjectileContact> g_ProjectileContacts;

static void UpdateProjectileContacts()
{
	if ( g_ProjectileContacts.empty() ) return;
	btDispatcher* pDispatcher = g_dynamicsWorld->getDispatcher();
	for ( int m = 0; m < pDispatcher->getNumManifolds(); m++ )
	{
		btPersistentManifold* pManifold = pDispatcher->getManifoldByIndexInternal(m);
		if ( pManifold->getNumContacts() == 0 ) continue;
		for ( sProjectileContact& contact : g_ProjectileContacts )
		{
			if ( contact.iTouched >= 0 ) continue;
			bool bIsBody0 = pManifold->getBody0() == contact.body;
			if ( !bIsBody0 && pManifold->getBody1() != contact.body ) continue;
			const btCollisionObject* pOther = bIsBody0 ? pManifold->getBody1() : pManifold->getBody0();

			// a ghost (the player's capsule) or a body that gives no contact response isn't a hit, nor is the thrower
			if ( !btRigidBody::upcast(pOther) || !pOther->hasContactResponse() ) continue;
			int iOther = pOther->getUserIndex();
			if ( iOther < 0 ) iOther = ODEFindObjectNumberOfBody ( pOther );
			const btBroadphaseProxy* pProxy = pOther->getBroadphaseHandle();
			if ( iOther < 0 || ( pProxy && ( pProxy->m_collisionFilterGroup & COL_TERRAIN ) ) ) iOther = 0;
			if ( iOther > 0 && iOther == contact.iIgnoreObject ) continue;

			// the touching point on the other body (a predicted point ahead of the body doesn't count)
			for ( int p = 0; p < pManifold->getNumContacts(); p++ )
			{
				const btManifoldPoint& point = pManifold->getContactPoint(p);
				if ( point.getDistance() > 0.01f ) continue;
				contact.iTouched = iOther;
				contact.vecPoint = ( bIsBody0 ? point.getPositionWorldOnB() : point.getPositionWorldOnA() ) * gSc;
				// the normal on body 1 points to body 0, so it faces the projectile when the projectile is body 0
				contact.vecNormal = bIsBody0 ? point.m_normalWorldOnB : -point.m_normalWorldOnB;
				break;
			}
		}
	}
}

// GG: before each simulation step: held ragdoll anchors move a step toward their targets, released joints stiffen back
static void PreTickCallback(btDynamicsWorld *world, btScalar timeStep)
{
	if ( ragdollManager ) ragdollManager->HoldStep ( timeStep );
}

void PostTickCallback(btDynamicsWorld *world, btScalar timeStep)
{
	UpdateCollisionsForLua();
	UpdateProjectileContacts();
	PhysicsPushLogStep();

	for (int j=((int)g_PhyObjectList.size())-1; j>=0 ;j--)
	{
		btRigidBody* body = g_PhyObjectList[j].body;
		if ( body==NULL ) continue;
		sObject* pObject = GetObjectData ( g_PhyObjectList[j].iID );
		if ( pObject==NULL ) return;
        if ( body!=NULL && body->isActive() )
        {
            btVector3 velocity = body->getLinearVelocity();
            btScalar speed = velocity.length();
            btScalar maxLinearVelocity = 999999.9f;
            if(speed > maxLinearVelocity)
            {
                velocity *= maxLinearVelocity/speed;
                body->setLinearVelocity(velocity);
            }
        }
    }
}

void ODEStart ( void )
{
	// ragdoll system
	//SAFE_DELETE_ARRAY ( jointManager );
	SAFE_DELETE ( jointManager );
	jointManager = new DBProJointManager();
	//SAFE_DELETE_ARRAY ( ragdollManager );
	SAFE_DELETE ( ragdollManager );
	ragdollManager = new DBProRagdollManager();

	// clear physics system data
	g_PhyObjectList.clear();
	g_ProjectileContacts.clear();

	///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
	g_collisionConfiguration = new btDefaultCollisionConfiguration();

	///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	g_dispatcher = new	btCollisionDispatcher(g_collisionConfiguration);
	// GG: each pair's tests timed while GetPhysicsSlowPairs asks (GGPhysicsQuery_Bullet.cpp), else as the default
	extern void PhysicsQuery_TimedNearCallback(btBroadphasePair& collisionPair, btCollisionDispatcher& dispatcher, const btDispatcherInfo& dispatchInfo);
	g_dispatcher->setNearCallback(PhysicsQuery_TimedNearCallback);
	// GG: a broadphase with no world bounds. The axis sweep's box (x and z -500 to 1800, -20,000 to 72,000 in world units)
	// clamped every body outside it to its edge, so far apart bodies there overlapped on one axis, made pairs and shared
	// one simulation island that never slept; a MAX map is centred on 0 and can be far larger
	sweepBP = new btDbvtBroadphase();
	m_overlappingPairCache = sweepBP;

	///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	g_solver = new btSequentialImpulseConstraintSolver;

	// create dynamics world
	g_dynamicsWorld = new btDiscreteDynamicsWorld(g_dispatcher,m_overlappingPairCache,g_solver,g_collisionConfiguration);
#ifdef WICKEDENGINE
	g_dynamicsWorld->setDebugDrawer(&g_debugDrawer);
	g_debugDrawer.SetWorldScale(gSc);
#endif	
	//Wicked settings check.
//	g_dynamicsWorld->getSolverInfo().m_solverMode |= SOLVER_RANDMIZE_ORDER;
//	g_dynamicsWorld->getDispatchInfo().m_enableSatConvex = true;
//	g_dynamicsWorld->getSolverInfo().m_splitImpulse = true;

	// callback to do some processing after each simulation steo
	g_dynamicsWorld->setInternalTickCallback(&PostTickCallback);
	// GG: and before each, a held ragdoll's anchor moved on (DBProRagDoll::HoldStep)
	g_dynamicsWorld->setInternalTickCallback(&PreTickCallback, 0, true);
	g_dynamicsWorld->getDispatchInfo().m_allowedCcdPenetration=0.0001f;

	// set gravity
	if (zero_gravity) 
	{
		gMs = 1.0;
		g_dynamicsWorld->setGravity(btVector3(0, 0.00f, 0));
	}
	else
	{
		g_dynamicsWorld->setGravity(btVector3(0, -10.0f, 0));
	}

	// clear strings
	strcpy (g_ProvidedMeshFilenameForBNVHSaving, "");
	strcpy (g_ProvidedMeshFilenameForOBJLoading, "");
}

void DestroyThisObject ( sObjectList* pPhyObject )
{
	if ( pPhyObject )
	{
		// delete allocations for this object
		if ( pPhyObject->myMotionState ) delete pPhyObject->myMotionState;

		// remove ridig body from world
		if ( pPhyObject->body )
		{
			// store shape from ragdoll initially
			btCollisionShape* shape = pPhyObject->body->getCollisionShape();

			// remove body from world then delete body
			g_dynamicsWorld->removeRigidBody(pPhyObject->body);
			delete pPhyObject->body;

			// delete stored collision shape from collision object list and memory
			for (int j=0;j<g_collisionShapes.size();j++)
			{
				if ( g_collisionShapes[j]==shape )
				{
					g_collisionShapes[j] = 0;
					if(shape) delete shape;
					g_collisionShapes.removeAtIndex(j);
					break;
				}
			}
		}

		// release any memory created to assign physics object
		if ( pPhyObject->pMem1 ) delete[] pPhyObject->pMem1;
		if ( pPhyObject->pMem2 ) delete[] pPhyObject->pMem2;
		pPhyObject->pMem1 = NULL;
		pPhyObject->pMem2 = NULL;

		// use regular DBPro matrix again
		sObject* pObject = GetObjectData ( pPhyObject->iID );
		if ( pObject ) pObject->position.bCustomWorldMatrix = false;

		// when remove item from middle of list, messes up how objects are referenced here!
		pPhyObject->body = NULL;
		pPhyObject->myMotionState = NULL;
	}
}

void ODEEnd ( void )
{
	// GG: entity kilos and the push log go with the world
	g_EntityMassKg.clear();
	g_iPushLogObject = 0;

	// ragdoll system termination
	SAFE_DELETE ( jointManager );
	SAFE_DELETE ( ragdollManager );

	///-----cleanup_start-----
	if ( g_dynamicsWorld )
	{
		// GG: the static bodies made from triangles (ODECreateStaticTriangleMeshData), with their shapes and meshes
		extern void ODEDeleteAllStaticTriangleMeshData ( void );
		ODEDeleteAllStaticTriangleMeshData();

		// remove character controller if any
		if (m_character)
		{
			g_dynamicsWorld->removeCollisionObject(m_ghostObject);
			m_ghostObject = NULL;
			delete m_character;
			m_character = NULL;
			g_pCharacterOwnSteps = NULL;
		}

		// reset links to regular DBP objects in case any missed
		for (int j=((int)g_PhyObjectList.size())-1; j>=0 ;j--)
			DestroyThisObject ( &g_PhyObjectList[j] );

		// remove the collision objects from the dynamics world and delete them (any stragglers not deleted by DestroyThisObject)
		for (int i=g_dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
		{
			btCollisionObject* obj = g_dynamicsWorld->getCollisionObjectArray()[i];
			g_dynamicsWorld->removeCollisionObject( obj );
			delete obj;
		}

		//delete collision shapes
		for (int j=0;j<g_collisionShapes.size();j++)
		{
			if ( g_collisionShapes[j]!=0 )
			{
				btCollisionShape* shape = g_collisionShapes[j];
				g_collisionShapes[j] = 0;
				if(shape) delete shape;
			}
		}
		g_collisionShapes.clear();

		// clear physics system data (no mallocs in structure, just references)
		g_PhyObjectList.clear();
		g_ProjectileContacts.clear();

		// delete physics system resources
		delete g_dynamicsWorld;

		// free indexvertex arrays (only after world destroyed)
		for (int j=0;j<g_indexvertexarrays.size();j++)
		{
			btTriangleIndexVertexArray* pIndexVertexArray = g_indexvertexarrays[j];
			SAFE_DELETE ( pIndexVertexArray );
		}
		g_indexvertexarrays.clear();

		delete g_solver;
		delete m_overlappingPairCache;
		delete g_dispatcher;
		delete g_collisionConfiguration;

		// free main var
		g_dynamicsWorld = NULL;
	}
	
	DeleteallImporters();

	//next line is optional: it will be cleared by the destructor when the array goes out of scope
	g_collisionShapes.clear();
}

// ensures the wave always has a fixed center per level start
float g_fStartingCenterPositionX = 0;
float g_fStartingCenterPositionY = 0;
float g_fStartingCenterPositionZ = 0;
int g_iActivationsPerCycle = 0;
bool* g_bBeenActivatedSinceStart = NULL;

void ODEFinalizeWorld ( void )
{
	// 290916 - performance of static geometry - instructs engine to skip updating bounds (they never move)
	if ( g_dynamicsWorld ) g_dynamicsWorld->setForceUpdateAllAabbs( false );

	// deactivate ALL dynamic physics
	for (int j = ((int)g_PhyObjectList.size()) - 1; j >= 0; j--)
	{
		btRigidBody* body = g_PhyObjectList[j].body;
		if (body == NULL) continue;
		sObject* pObject = GetObjectData (g_PhyObjectList[j].iID);
		if (g_PhyObjectList[j].iID == 7000) //PE: Fake terrain.
			continue;
		if (pObject == NULL) return;
		if (body != NULL)
		{
			body->forceActivationState(ISLAND_SLEEPING);
		}
	}

	// mark the starting center position
	g_fStartingCenterPositionX = CameraPositionX(0);
	g_fStartingCenterPositionY = CameraPositionY(0);
	g_fStartingCenterPositionZ = CameraPositionZ(0);
	g_iActivationsPerCycle = 0;
	if (g_bBeenActivatedSinceStart) delete g_bBeenActivatedSinceStart;
	//PE: We get a crash when spawning objects as there is no mem allocated for them.
	uint32_t iReserveForSpawnedObjects = 1000;
	g_bBeenActivatedSinceStart = new bool[(int)g_PhyObjectList.size() + iReserveForSpawnedObjects];
	for (int j = 0; j < (int)g_PhyObjectList.size() + iReserveForSpawnedObjects; j++)
		g_bBeenActivatedSinceStart[j] = false;
}

float ODEProjectActivationWave ( float fDistanceWithinToActivate, float fSlices )
{
	// to ease the initial physics burden, cast an activation wave so dynamic things can fall from sky and settle over the initial seconds
	// after calling this with the desired distance to activate
	if (!g_bBeenActivatedSinceStart)
		return fDistanceWithinToActivate;
	bool bMoveOnWhenAllActivatedInSlice = true;
	for (int j = ((int)g_PhyObjectList.size()) - 1; j >= 0; j--)
	{
		btRigidBody* body = g_PhyObjectList[j].body;
		if (body == NULL) continue;
		sObject* pObject = GetObjectData (g_PhyObjectList[j].iID);
		if (pObject == NULL) continue;
		if (body != NULL)
		{
			float fDX = g_fStartingCenterPositionX - pObject->position.vecPosition.x;
			float fDY = g_fStartingCenterPositionY - pObject->position.vecPosition.y;
			float fDZ = g_fStartingCenterPositionZ - pObject->position.vecPosition.z;
			float fDist = sqrt(fabs(fDX*fDX) + fabs(fDY*fDY) + fabs(fDZ*fDZ));
			if (fDist > fDistanceWithinToActivate && fDist <= fDistanceWithinToActivate + fSlices)
			{
				if (body->isActive() == false && g_bBeenActivatedSinceStart[j] == false)
				{
					bMoveOnWhenAllActivatedInSlice = false;
					if (g_iActivationsPerCycle > 0)
					{
						g_iActivationsPerCycle--;
					}
					else
					{
						body->activate();
						g_bBeenActivatedSinceStart[j] = true;
						g_iActivationsPerCycle = 30;
					}
					break;
				}
			}
		}
	}
	if (bMoveOnWhenAllActivatedInSlice == true)
	{
		fDistanceWithinToActivate += fSlices;
	}
	return fDistanceWithinToActivate;
}

GGQUATERNION BT2DX_QUATERNION(const btQuaternion &q)
{
 return GGQUATERNION(q.x(), q.y(), q.z(), q.w());
}

GGMATRIX BT2DX_MATRIX(const btTransform &ms)
{
 btQuaternion q = ms.getRotation();
 btVector3 p = ms.getOrigin();

 GGMATRIX pos, rot, world;
 GGMatrixTranslation(&pos, p.x(), p.y(), p.z());
 GGMatrixRotationQuaternion(&rot, &BT2DX_QUATERNION(q));
 GGMatrixMultiply(&world, &rot, &pos);

 return world;
}

//Dave
float LinearInterpolate ( float y1, float y2, float mu )
{
   return ( y1 * ( 1 - mu ) + y2 * mu );
}

float BULLETCosineInterpolate ( float y1, float y2, float mu )
{
 float mu2 = ( 1 - cos ( mu * 3.141f ) ) / 2;
 return ( y1 * ( 1 - mu2 ) + y2 * mu2 );
}

//#ifdef PUTPHYSICSBACKINMAINTHREADSEQUENCE
//std::mutex physicslock = {};
std::recursive_mutex physicslock = {};
int g_iPhysicsSubSteps = 0; // the simulation steps the last update took (GGPhysicsQuery's stats)

// GG: SetEntityMass: 1 set on its body now, 0 kept for when it has a body, -1 its body is static (kept for a moving one);
// 0 kilos or less clears it and gives the body its built mass again. It neither wakes the body nor changes its damping,
// sleep thresholds or friction
static int ODEApplyMassKg(sObjectList* pPhyObject, float fKg)
{
	if (pPhyObject == NULL || pPhyObject->body == NULL) return 0;
	btRigidBody* pBody = pPhyObject->body;
	if (pBody->getInvMass() == 0) return -1;
	btVector3 localInertia(0, 0, 0);
	pBody->getCollisionShape()->calculateLocalInertia(fKg, localInertia);
	pBody->setMassProps(fKg, localInertia);
	pBody->updateInertiaTensor();
	return 1;
}

int ODESetEntityMassKg(int iEntity, int iObject, float fKg)
{
	physicslock.lock();
	sObjectList* pPhyObject = iObject > 0 ? ODEFindID(iObject) : NULL;
	int iResult = 0;
	if (fKg > 0)
	{
		g_EntityMassKg[iEntity] = fKg;
		iResult = ODEApplyMassKg(pPhyObject, fKg);
	}
	else
	{
		g_EntityMassKg.erase(iEntity);
		if (pPhyObject && pPhyObject->fMass > 0) iResult = ODEApplyMassKg(pPhyObject, pPhyObject->fMass);
	}
	physicslock.unlock();
	return iResult;
}

// a body built for an entity takes its kilos again (physics_prepareentityforphysics): false if it has none
bool ODEApplyEntityMassKg(int iEntity, int iObject)
{
	physicslock.lock();
	auto it = g_EntityMassKg.find(iEntity);
	bool bSet = (it != g_EntityMassKg.end());
	if (bSet) ODEApplyMassKg(ODEFindID(iObject), it->second);
	physicslock.unlock();
	return bSet;
}

// kilos for an object's body, kept nowhere (an entity's .fpe physicsmass, applied at each build): as ODESetEntityMassKg
int ODESetObjectMassKg(int iObject, float fKg)
{
	physicslock.lock();
	int iResult = ODEApplyMassKg(ODEFindID(iObject), fKg);
	physicslock.unlock();
	return iResult;
}

// the kilos SetEntityMass gave an entity: false if none
bool ODEGetEntityMassKg(int iEntity, float* pfKg)
{
	physicslock.lock();
	auto it = g_EntityMassKg.find(iEntity);
	bool bSet = (it != g_EntityMassKg.end());
	if (bSet) *pfKg = it->second;
	physicslock.unlock();
	return bSet;
}

// an entity deleted: its element can be reused by the next spawn
void ODEClearEntityMassKg(int iEntity)
{
	physicslock.lock();
	g_EntityMassKg.erase(iEntity);
	physicslock.unlock();
}

// the mass an object's body has now (kilos); 0 for none or a static body
float ODEGetObjectMassKg(int iObject)
{
	physicslock.lock();
	float fKg = 0;
	sObjectList* pPhyObject = ODEFindID(iObject);
	if (pPhyObject && pPhyObject->body && pPhyObject->body->getInvMass() > 0) fKg = 1.0f / pPhyObject->body->getInvMass();
	physicslock.unlock();
	return fKg;
}

// SetPhysicsPushLog, GetPhysicsPushLog
void ODESetPushLog(int iObject, int iSteps)
{
	physicslock.lock();
	g_iPushLogObject = iObject;
	g_iPushLogSteps = iSteps > 0 ? iSteps : 8;
	g_iPushLogStepsLeft = 0;
	g_PushLog.clear();
	physicslock.unlock();
}

void ODEGetPushLog(std::vector<std::string>* pLines)
{
	physicslock.lock();
	*pLines = g_PushLog;
	g_PushLog.clear();
	physicslock.unlock();
}
//#else
//class physicslockclass
//{
//public:
//	void lock(void) {};
//	void unlock(void) {};
//};
//physicslockclass physicslock;
//#endif

// the world matrix and angles an object takes from its physics body (trans: the body's world transform); a box body is
// centred on the object's collision box, a mesh body on its origin. Update sets them for a moving body's object, and
// ODEGetBodyPose reads them for any body
static void ODEObjectPoseFromBody ( sObjectList* pEntry, sObject* pObject, const btTransform& trans, GGMATRIX* pWorld, GGVECTOR3* pRotate )
{
	// get body angles
	btQuaternion angles;
	angles = pEntry->body->getOrientation();

	// make the rotation matrix
	GGMATRIX matRotation;
	GGQUATERNION qRotation;
	qRotation.x = angles.getX();
	qRotation.y = angles.getY();
	qRotation.z = angles.getZ();
	qRotation.w = angles.getW();
	GGMatrixRotationQuaternion(&matRotation, &qRotation);

	// LEE, I think the matrix returned is a ZYX order rotation, but we need XYZ
	// so it translates back properly. I could use ZYX in FPSC but it will screw up all maps..
	// (not for a body made turned by the pivot already: ODESetBodyIncludesPivot)
	if (pObject->position.bApplyPivot && !pEntry->bPivotInBody)
		matRotation = pObject->position.matPivot * matRotation;

	// Use physics object position
	GGMATRIX matTranslation;
	GGMatrixTranslation(&matTranslation, trans.getOrigin().getX()*gSc, trans.getOrigin().getY()*gSc, trans.getOrigin().getZ()*gSc);

	// Use physics object position
	GGMATRIX matCenterColOffset;
	float fXSize = (pObject->collision.vecMax.x - pObject->collision.vecMin.x) * pObject->position.vecScale.x;//;
	float fYSize = (pObject->collision.vecMax.y - pObject->collision.vecMin.y) * pObject->position.vecScale.y;//;
	float fZSize = (pObject->collision.vecMax.z - pObject->collision.vecMin.z) * pObject->position.vecScale.z;//;
	float fXOffset = (pObject->collision.vecMin.x * pObject->position.vecScale.x) + (fXSize / 2.0f);
	float fYOffset = (pObject->collision.vecMin.y * pObject->position.vecScale.y) + (fYSize / 2.0f);
	float fZOffset = (pObject->collision.vecMin.z * pObject->position.vecScale.z) + (fZSize / 2.0f);

	// a mesh body (CreateMesh: polygon, convex hull, hull decomposition) is built in object space and placed at
	// the object's origin, not at its collision box centre, so there is no centre to take off; doing so drew it
	// off its physics by the box centre (a wreck with its origin at the ground sank by half its height)
	if (!pEntry->bCentredOnCollisionBox)
	{
		fXOffset = 0.0f;
		fYOffset = 0.0f;
		fZOffset = 0.0f;
	}

	if (pObject->pFrame)
	{
		//PE: This only work for box, dont work for dynamic convexhull shapes ?
		//PE: pFrame->vecOffset  is already in matTranslation, so only offset center of rotation.
		fXOffset += -(pObject->pFrame->vecOffset.x * pObject->position.vecScale.x);
		fYOffset += -(pObject->pFrame->vecOffset.y * pObject->position.vecScale.y);
		fZOffset += -(pObject->pFrame->vecOffset.z * pObject->position.vecScale.z);
		//PE: Add pFrame->vecRotation to matWorld.
		GGMATRIX matRotateFrameX, matRotateFrameY, matRotateFrameZ;
		GGMatrixRotationX(&matRotateFrameX, GGToRadian(pObject->pFrame->vecRotation.x));	// x rotation
		GGMatrixRotationY(&matRotateFrameY, GGToRadian(pObject->pFrame->vecRotation.y));	// y rotation
		GGMatrixRotationZ(&matRotateFrameZ, GGToRadian(pObject->pFrame->vecRotation.z));	// z rotation
		matRotation = matRotateFrameX * matRotateFrameY * matRotateFrameZ * matRotation;
	}

	// All, the trick was to put 'matCenterColOffset' before the rotation step to correct object offset issue :)
	GGMatrixTranslation(&matCenterColOffset, -fXOffset, -fYOffset, -fZOffset);
	GGMATRIX matWorld = pObject->position.matScale * matCenterColOffset * matRotation * matTranslation;

	// GG: the angles from the rotation alone; the world matrix carries the object's scale, which the decode took as part
	// of the turn (a wreck at scale 160 lying at yaw 271 read 144)
	GGVECTOR3 vecRotate = GGVECTOR3(0, 0, 0);
	if (pObject->position.bApplyPivot && pEntry->bPivotInBody)
	{
		// the object's own rotation leaves its pivot out: the regular transform applies the pivot again once the
		// body goes (CollisionOff), and a new body adds it again, so the model neither turns nor turns twice
		float fDet;
		GGMATRIX matPivotInverse;
		GGMatrixInverse(&matPivotInverse, &fDet, &pObject->position.matPivot);
		GGMATRIX matWithoutPivot = matPivotInverse * matRotation;
		BULLETAnglesFromMatrix(&matWithoutPivot, &vecRotate);
	}
	else
	{
		BULLETAnglesFromMatrix(&matRotation, &vecRotate);
	}
	*pWorld = matWorld;
	*pRotate = vecRotate;
}

void Update(float fTimeStep)
{
	// GG: the frame's time for PhysicsApplyPush, at most the 7 steps at 120 a frame could take
	g_fPhysicsFrameTime = fTimeStep < 7.0f / 120.0f ? fTimeStep : 7.0f / 120.0f;

	// step physics simulation
	//PE: 1.0f/120.0f is what is killing the bullet engine (fps drops) , simulation in 1:1 is 1.0f / 60.0f, 1.0f/120.0f run twice as fast.
	//PE: We are calling it at 60 fps , so bullet is always behind and need to call the simulation many times per frame to keep up.
	//PE: Well this change physics demos and how they are coded , bad luck ...
	//PE: We should consider to use this as the default in Max.
	
	//PE: Cant touch physics object while running stepsimulation.
	physicslock.lock();
	// GG: the last update's slow pairs kept for GetPhysicsSlowPairs (GGPhysicsQuery_Bullet.cpp)
	extern void PhysicsQuery_SlowPairsUpdateStart(void);
	PhysicsQuery_SlowPairsUpdateStart();
	// GG: held ragdolls spread their anchors' moves over this update's steps
	if ( ragdollManager ) ragdollManager->HoldFrame ( fTimeStep );
	#ifdef FASTBULLETPHYSICS
		//PE: fixed timestep with interpolation set at 1/60.
		g_iPhysicsSubSteps = g_dynamicsWorld->stepSimulation(fTimeStep, 7, btScalar(1.) / btScalar(60.));
	#else
		// GG: the player's controller is taken out of the world's steps while the world steps at 60, and put back at 120
		btKinematicCharacterController* pCharacterOwnSteps = g_bPhysicsWorldAt60 ? m_character : NULL;
		if (pCharacterOwnSteps != g_pCharacterOwnSteps)
		{
			if (g_pCharacterOwnSteps) g_dynamicsWorld->addAction(g_pCharacterOwnSteps);
			if (pCharacterOwnSteps) g_dynamicsWorld->removeAction(pCharacterOwnSteps);
			g_pCharacterOwnSteps = pCharacterOwnSteps;
			g_fCharacterLocalTime = 0;
		}
		g_iPhysicsSubSteps = g_dynamicsWorld->stepSimulation(fTimeStep, 7, g_bPhysicsWorldAt60 ? btScalar(1.) / btScalar(60.) : 1.0f / 120.0f);
		if (g_pCharacterOwnSteps)
		{
			// the controller's own 1/120 steps after the world's, as many as the world took at 120, at most 7 a frame as before
			const btScalar fCharacterStep = 1.0f / 120.0f;
			g_fCharacterLocalTime += fTimeStep;
			int iCharacterSteps = int(g_fCharacterLocalTime / fCharacterStep);
			g_fCharacterLocalTime -= iCharacterSteps * fCharacterStep;
			if (iCharacterSteps > 7) iCharacterSteps = 7;
			for (int i = 0; i < iCharacterSteps; i++) g_pCharacterOwnSteps->updateAction(g_dynamicsWorld, fCharacterStep);
		}
	#endif

	physicslock.unlock();

	// update ragdoll simulations
	ragdollManager->Update();

	// character controller update
	if (g_CharacterControlObject > 0)
	{
		// position of character
		btTransform characterWorldTrans;
		characterWorldTrans = m_ghostObject->getWorldTransform();
		if (isnan(characterWorldTrans.getOrigin().getX()) == 0)
		{
			// update object position vector
			float fHorizontalSmoothing = 0.35f;
			float fVerticalSmoothing = 0.5f;

			sObject* p1 = GetObjectData(g_CharacterControlObject);
			float fMoveX = (p1->position.vecPosition.x - (characterWorldTrans.getOrigin().getX()*gSc));
			float fMoveY = (p1->position.vecPosition.y - (characterWorldTrans.getOrigin().getY()*gSc));
			float fMoveZ = (p1->position.vecPosition.z - (characterWorldTrans.getOrigin().getZ()*gSc));
			fMoveX = (fMoveX * fHorizontalSmoothing);
			fMoveY = (fMoveY * fVerticalSmoothing);
			fMoveZ = (fMoveZ * fHorizontalSmoothing);
			float newX = p1->position.vecPosition.x - fMoveX;
			float newZ = p1->position.vecPosition.z - fMoveZ;
			float newY = p1->position.vecPosition.y;// -fMoveY; // ensures when 0,0,0 gravity set, character does not slide downward (ladder/zipline)
			if (m_character->getGravity()!=0.0f ) newY -= fMoveY;
			PositionObject(g_CharacterControlObject, newX, newY, newZ);

			// update object rotation which returns status (X=whether can jump)
			float fAngleX = 0.0f;
			if (m_character->wasOnGround()) fAngleX = 1.0f;
			RotateObject(g_CharacterControlObject, fAngleX, 0.0f, 0.0f);
		}
	}

	// print positions of all objects
	physicslock.lock();
	for (int j = ((int)g_PhyObjectList.size()) - 1; j >= 0; j--)
	{
		// get this object
		btRigidBody* body = g_PhyObjectList[j].body;
		if (body == NULL) continue;
		sObject* pObject = GetObjectData(g_PhyObjectList[j].iID);
		if (pObject == NULL) continue;

		// handle response mode to freeze objects
		if (g_PhyObjectList[j].iResponseMode == 1)
		{
			body->setLinearVelocity(btVector3(0, 0, 0));
		}

		// handle physics body visual update
		if (body->getMotionState() && g_PhyObjectList[j].bDynamicUpdate == true)
		{
			// get position from physics body
			btTransform trans;
			body->getMotionState()->getWorldTransform(trans);

			// 210516 - skip NAN values if detected in BODY (massive friction etc)
			if (isnan(trans.getOrigin().getX()) == 1)
			{
				continue;
			}

			// full object control or bCapsuleControl
			if (g_PhyObjectList[j].bCapsuleControl)
			{
				// Update DBP object position so can get coordinate back in DBP
				// handled differently, capsules are secondary and controlled from movement system
				// LB: can set arbvalue here based on material 'arb' being stood on, so the footfall of the
				// capsule (character) is accurate to the material being traversed
				int materialindex = 0;
				g_PhyObjectList[j].body->setUserPointer((void*)(1+materialindex));
			}
			else
			{
				// the object's world matrix and angles from its body
				PositionObject(g_PhyObjectList[j].iID, trans.getOrigin().getX()*gSc, trans.getOrigin().getY()*gSc, trans.getOrigin().getZ()*gSc);
				GGMATRIX matWorld;
				GGVECTOR3 vecRotate;
				ODEObjectPoseFromBody(&g_PhyObjectList[j], pObject, trans, &matWorld, &vecRotate);

				// use ODE matrix, not regular DBPro matrix
				pObject->position.bCustomWorldMatrix = true;
				pObject->position.matWorld = matWorld;

				// communicate custom world matrix to wicked
				//WickedCall_UpdateObject(pObject); DX12

				// update DBPro object position and rotation to match physics
				pObject->position.vecPosition = GGVECTOR3(matWorld._41, matWorld._42, matWorld._43);
				pObject->position.vecRotate = vecRotate;
				pObject->collision.bColCenterUpdated = true;
				body->applyDamping(0.1f);
			}
		}

		// Handle bouyancy
		if (g_PhyObjectList[j].bBouyant == true)
		{
			// water line to control strength of bouyancy
			float fWaterLine = g_fWaterLineY - pObject->position.vecPosition.y;

			// SK: Simplified boyancy for now
			if (fWaterLine > 0.0f)
			{
				float fForce = fWaterLine / g_fWaterLineY;
				if (fForce > 0.03f) fForce = 0.03f;
				btVector3 vOldVel = body->getLinearVelocity();
				float fYVel = vOldVel.getY();
				if (fYVel > 2.0f) fYVel = 2.0f; // Slow ascent
				body->setLinearVelocity(btVector3(vOldVel.getX()*0.75f, fYVel*0.75f, vOldVel.getZ()*0.75f));
				vOldVel = body->getAngularVelocity();
				body->setAngularVelocity(btVector3(vOldVel.getX()*0.99f, fYVel*0.99f, vOldVel.getZ()*0.99f));
				body->applyDamping(0.5f);
			}
		}
	}
	physicslock.unlock();

	// Draw the debug physics objects.
	if (g_debugDrawer.getDebugMode() != 0)
	{
		btRigidBody* rBody = nullptr;
		btTransform transform;

		// Draw the dynamic objects.
		for (int i = 0; i < g_PhyObjectList.size(); i++)
		{
			rBody = g_PhyObjectList[i].body;
			if (rBody == nullptr) continue;
			if (!rBody->isStaticObject())
			{
				rBody->getMotionState()->getWorldTransform(transform);
				// LB: not sure wwhy this was commented out??
				g_dynamicsWorld->debugDrawObject(transform, rBody->getCollisionShape(), btVector3(0, 1, 0));
			}
			rBody = nullptr;
		}

		// Draw the constraints.
		int iConstraintCount = jointManager->GetNumberOfJoints();
		for (int i = 0; i < iConstraintCount; i++)
		{
			btTypedConstraint* constraint = jointManager->GetJoint(i)->GetConstraint();
			if (constraint)
			{
				constraint->setDbgDrawSize(0.5f);
				g_dynamicsWorld->debugDrawConstraint(constraint);
			}
		}
	}
}

void ODEUpdate ( void )
{
	Update ( 1.0f/60.0f );
}

void ODEUpdate ( float fManualStep )
{
	Update ( fManualStep );
}

// Creation commands

//void GetSizePositionRotation( sObject* pObject, float &fXSize, float &fYSize, float &fZSize,
//	                                            float &fXPos,  float &fYPos,  float &fZPos,
//	                                            float &fXRot,  float &fYRot,  float &fZRot,
//												bool bMakeCollisionCenterAdjustment, float fScalingFactor = 1.0f )
//{
//	fXSize = ( pObject->collision.vecMax.x - pObject->collision.vecMin.x) * pObject->position.vecScale.x * fScalingFactor;
//	fYSize = ( pObject->collision.vecMax.y - pObject->collision.vecMin.y) * pObject->position.vecScale.y * fScalingFactor;
//	fZSize = ( pObject->collision.vecMax.z - pObject->collision.vecMin.z) * pObject->position.vecScale.z * fScalingFactor;
//	fXPos = pObject->position.vecPosition.x;//+ pObject->pFrame->vecOffset.x;
//	fYPos = pObject->position.vecPosition.y;//+ pObject->pFrame->vecOffset.y;
//	fZPos = pObject->position.vecPosition.z;//+ pObject->pFrame->vecOffset.z;
//	fXRot = GGToRadian( pObject->position.vecRotate.x + pObject->pFrame->vecRotation.x );
//	fYRot = GGToRadian( pObject->position.vecRotate.y + pObject->pFrame->vecRotation.y );
//	fZRot = GGToRadian( pObject->position.vecRotate.z + pObject->pFrame->vecRotation.z );
//
//	// adjust for center of object as all physics objects coords are at center
//	if ( bMakeCollisionCenterAdjustment == true )
//	{
//		float fXOffset = (pObject->collision.vecMin.x * pObject->position.vecScale.x) + (fXSize / 2.0f);
//		float fYOffset = (pObject->collision.vecMin.y * pObject->position.vecScale.y) + (fYSize / 2.0f);
//		float fZOffset = (pObject->collision.vecMin.z * pObject->position.vecScale.z) + (fZSize / 2.0f);
//		GGVECTOR3 vColOffset = GGVECTOR3( fXOffset, fYOffset, fZOffset );
//		GGMATRIX matRot;
//		GGMatrixRotationX( &matRot, fXRot );  GGVec3TransformCoord( &vColOffset, &vColOffset, &matRot );
//		GGMatrixRotationY( &matRot, fYRot );  GGVec3TransformCoord( &vColOffset, &vColOffset, &matRot );
//		GGMatrixRotationZ( &matRot, fZRot );  GGVec3TransformCoord( &vColOffset, &vColOffset, &matRot );
//		fXPos += vColOffset.x;
//		fYPos += vColOffset.y;
//		fZPos += vColOffset.z;
//	}
//
//}

void GetSizePositionRotation(sObject* pObject, float &fXSize, float &fYSize, float &fZSize,
	float &fXPos, float &fYPos, float &fZPos,
	float &fXRot, float &fYRot, float &fZRot,
	bool bMakeCollisionCenterAdjustment, float fScalingFactor = 1.0f)
{
	fXSize = (pObject->collision.vecMax.x - pObject->collision.vecMin.x) * pObject->position.vecScale.x * fScalingFactor;
	fYSize = (pObject->collision.vecMax.y - pObject->collision.vecMin.y) * pObject->position.vecScale.y * fScalingFactor;
	fZSize = (pObject->collision.vecMax.z - pObject->collision.vecMin.z) * pObject->position.vecScale.z * fScalingFactor;
	fXPos = pObject->position.vecPosition.x;//+ pObject->pFrame->vecOffset.x;
	fYPos = pObject->position.vecPosition.y;//+ pObject->pFrame->vecOffset.y;
	fZPos = pObject->position.vecPosition.z;//+ pObject->pFrame->vecOffset.z;
	fXRot = GGToRadian(pObject->position.vecRotate.x);
	fYRot = GGToRadian(pObject->position.vecRotate.y);
	fZRot = GGToRadian(pObject->position.vecRotate.z);

	// adjust for center of object as all physics objects coords are at center
	if (bMakeCollisionCenterAdjustment == true)
	{
		float fXOffset = (pObject->collision.vecMin.x * pObject->position.vecScale.x) + (fXSize / 2.0f);
		float fYOffset = (pObject->collision.vecMin.y * pObject->position.vecScale.y) + (fYSize / 2.0f);
		float fZOffset = (pObject->collision.vecMin.z * pObject->position.vecScale.z) + (fZSize / 2.0f);
		GGVECTOR3 vObjectOffset = { 0.0,0.0,0.0 };
		GGVECTOR3 vObjectOffsetCenter = { 0.0,0.0,0.0 };

		#ifdef WICKEDENGINE
		if (pObject->pFrame)
		{
			//PE: Offset center of rotation for new pFrame->vecOffset values.
			vObjectOffsetCenter.x = -(pObject->pFrame->vecOffset.x * pObject->position.vecScale.x);
			vObjectOffsetCenter.y = -(pObject->pFrame->vecOffset.y * pObject->position.vecScale.y);
			vObjectOffsetCenter.z = -(pObject->pFrame->vecOffset.z * pObject->position.vecScale.z);

			//PE: Rotation matrix
			GGMATRIX matRotation;
			GGMATRIX matRotateX, matRotateY, matRotateZ;
			GGMatrixRotationX(&matRotateX, GGToRadian(pObject->pFrame->vecRotation.x));	// x rotation
			GGMatrixRotationY(&matRotateY, GGToRadian(pObject->pFrame->vecRotation.y));	// y rotation
			GGMatrixRotationZ(&matRotateZ, GGToRadian(pObject->pFrame->vecRotation.z));	// z rotation
			matRotation = matRotateX * matRotateY * matRotateZ;

			//PE: Translation with matRotation for pFrame->vecRotation
			GGVECTOR3 vecPosOffset = GGVECTOR3(pObject->pFrame->vecOffset.x, pObject->pFrame->vecOffset.y, pObject->pFrame->vecOffset.z);
			GGVec3TransformCoord(&vecPosOffset, &vecPosOffset, &matRotation);
			vObjectOffset.x = vecPosOffset.x;
			vObjectOffset.y = vecPosOffset.y;
			vObjectOffset.z = vecPosOffset.z;
		}
		#endif

		// and now account for the collision center shift
		GGVECTOR3 vColOffset = GGVECTOR3( fXOffset + vObjectOffsetCenter.x, fYOffset + vObjectOffsetCenter.y, fZOffset + vObjectOffsetCenter.z );
		GGMATRIX matRot;
		GGMatrixRotationX( &matRot, fXRot );  GGVec3TransformCoord( &vColOffset, &vColOffset, &matRot );
		GGMatrixRotationY( &matRot, fYRot );  GGVec3TransformCoord( &vColOffset, &vColOffset, &matRot );
		GGMatrixRotationZ( &matRot, fZRot );  GGVec3TransformCoord( &vColOffset, &vColOffset, &matRot );
		fXPos += vColOffset.x + vObjectOffset.x;
		fYPos += vColOffset.y + vObjectOffset.y;
		fZPos += vColOffset.z + vObjectOffset.z;
	}
	else
	{
		#ifdef WICKEDENGINE
		//PE: Make sure only objects are checked , not terrain ...
		if (pObject->dwObjectNumber > 70000 && pObject->pFrame)
		{
			// vars
			bool ConvexHull = false;
			GGVECTOR3 vObjectOffset = { 0.0,0.0,0.0 };
			GGVECTOR3 vObjectOffsetCenter = { 0.0,0.0,0.0 };

			// rotation matrix
			GGMATRIX matRotation;
			GGMATRIX matRotateX, matRotateY, matRotateZ;
			GGMatrixRotationX(&matRotateX, GGToRadian(pObject->pFrame->vecRotation.x));	// x rotation
			GGMatrixRotationY(&matRotateY, GGToRadian(pObject->pFrame->vecRotation.y));	// y rotation
			GGMatrixRotationZ(&matRotateZ, GGToRadian(pObject->pFrame->vecRotation.z));	// z rotation
			matRotation = matRotateX * matRotateY * matRotateZ;

			// translation with matRotation for pFrame->vecRotation
			GGVECTOR3 vecPosOffset = GGVECTOR3(pObject->pFrame->vecOffset.x, pObject->pFrame->vecOffset.y, pObject->pFrame->vecOffset.z);
			GGVec3TransformCoord(&vecPosOffset, &vecPosOffset, &matRotation);
			vObjectOffset.x = vecPosOffset.x;
			vObjectOffset.y = vecPosOffset.y;
			vObjectOffset.z = vecPosOffset.z;

			//PE: if offset in matrix , ignore.
			if (!(floor(pObject->pFrame->matCombined._41) == floor(pObject->pFrame->vecOffset.x) &&
				floor(pObject->pFrame->matCombined._42) == floor(pObject->pFrame->vecOffset.y) &&
				floor(pObject->pFrame->matCombined._43) == floor(pObject->pFrame->vecOffset.z)) )
			{
				vObjectOffset.x = 0;
				vObjectOffset.y = 0;
				vObjectOffset.z = 0;
			}
			// apply offset of root frame , PE: we now check for frame indentity mastrix so also add offset.
			fXPos += vObjectOffset.x;
			fYPos += vObjectOffset.y;
			fZPos += vObjectOffset.z;
		}
		#endif
	}
}

void fillTransform( btTransform &trans, float fXPos, float fYPos, float fZPos, float fXRot, float fYRot, float fZRot )
{
	trans.setIdentity();

	trans.setOrigin( btVector3( fXPos, fYPos, fZPos ) );

	GGQUATERNION QuatAroundX( 1, 0, 0, fXRot );
	GGQUATERNION QuatAroundY( 0, 1, 0, fYRot );
	GGQUATERNION QuatAroundZ( 0, 0, 1, fZRot );

	GGQuaternionRotationAxis( &QuatAroundX, &GGVECTOR3( 1, 0, 0 ), fXRot );
	GGQuaternionRotationAxis( &QuatAroundY, &GGVECTOR3( 0, 1, 0 ), fYRot );
	GGQuaternionRotationAxis( &QuatAroundZ, &GGVECTOR3( 0, 0, 1 ), fZRot );

	GGQUATERNION finalOrientation = QuatAroundX * QuatAroundY * QuatAroundZ;
	trans.setRotation( btQuaternion( finalOrientation.x,
		                             finalOrientation.y,
		                             finalOrientation.z,
	                                 finalOrientation.w ) );
}

void CreateBox ( int iObjectNumber, int isDynamic, int iLimbNumber, int iTipOverMode, float fWeight, float fFriction, float fRestitution,
				 bool blnManual, float fXIN, float fYIN, float fZIN, float fXSizeIN, float fYSizeIN, float fZSizeIN, float fXAngIN, float fYAngIN, float fZAngIN)
{
	// defaults
	if ( fWeight==-1 ) fWeight = 100.0f;
	if ( fFriction==-1 ) fFriction = 90;
	if ( fRestitution==-1 ) fRestitution = 0.0f;

	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;

	if (blnManual)
	{
		fXPos = fXIN;
		fYPos = fYIN;
		fZPos = fZIN;
		fXSize = fXSizeIN;
		fYSize = fYSizeIN;
		fZSize = fZSizeIN;
		fXRot = GGToRadian (fXAngIN);
		fYRot = GGToRadian (fYAngIN);
		fZRot = GGToRadian (fZAngIN);
	}
	else
	{
		GetSizePositionRotation( pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, true);
	}
	
	// real object
	sObject* pRealObject = pObject;
	if ( pObject->pInstanceOfObject ) pRealObject = pObject->pInstanceOfObject;
	
	// if limb number specified
	if ( iLimbNumber!=-1 && iLimbNumber<pRealObject->iFrameCount )
	{
		// change boundbox to that of the limb
		sFrame* pLimbFrame = pRealObject->ppFrameList[iLimbNumber];
		if ( pLimbFrame )
		{
			sMesh* pLimbMesh = pLimbFrame->pMesh;
			if ( pLimbMesh )
			{
				fXSize = ( pLimbMesh->Collision.vecMax.x - pLimbMesh->Collision.vecMin.x );
				fYSize = ( pLimbMesh->Collision.vecMax.y - pLimbMesh->Collision.vecMin.y );
				fZSize = ( pLimbMesh->Collision.vecMax.z - pLimbMesh->Collision.vecMin.z );
				GGVECTOR3 vLimbCenter = GGVECTOR3(pLimbMesh->Collision.vecMin.x+(fXSize/2),pLimbMesh->Collision.vecMin.y+(fYSize/2),pLimbMesh->Collision.vecMin.z+(fZSize/2));
				GGVec3TransformCoord ( &vLimbCenter, &vLimbCenter, &pLimbFrame->matAbsoluteWorld );
				fXPos = vLimbCenter.x;
				fYPos = vLimbCenter.y;
				fZPos = vLimbCenter.z;
				fXSize = fXSize * pObject->position.vecScale.x;
				fYSize = fYSize * pObject->position.vecScale.y;
				fZSize = fZSize * pObject->position.vecScale.z;
			}
		}
	}

	// ensure very small objects are not allowed below this size
	if ( fXSize < 3 ) fXSize = 3;
	if ( fYSize < 4 ) fYSize = 4;
	if ( fZSize < 3 ) fZSize = 3;

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// volume calculation
	float fVolume = 0;
	fVolume = fXSize * fYSize * fZSize;

	// modify with weight modifier
	fVolume *= (fWeight/100.0f);

	// can specify special collision type through iTipOverMode
	bool bDoNotMoveAnyOtherObjects = false;
	bool bLeavePlayerAlone = false;
	bool bDoNotCollideWithCapsuleCharacters = false;
	if ( iTipOverMode == 11 ) { bDoNotMoveAnyOtherObjects=true; iTipOverMode = 0; }
	if ( iTipOverMode == 12 ) { bLeavePlayerAlone = true; iTipOverMode = 0; }
	if ( iTipOverMode == 13 ) { bDoNotCollideWithCapsuleCharacters = true; iTipOverMode = 0; }

	//PE: Weapon drop , must wait for physics thread to end.
	physicslock.lock();

	// create a bullet shape
	btCollisionShape* boxShape = NULL;
	if ( iTipOverMode==0 )
	{
		// regular box
		boxShape = new btBoxShape(btVector3(btScalar(fXSize/2.0f),btScalar(fYSize/2.0f),btScalar(fZSize/2.0f)));
	}
	else
	{
		// create additional shape for a TipOver shape (balancing on gun nozzle)
		btCompoundShape* pCompound = new btCompoundShape();
		btTransform localTrans;
		localTrans.setIdentity();
		btCollisionShape* pShape1 = new btBoxShape(btVector3(btScalar(fXSize/2.0f),btScalar(fYSize/2.0f),btScalar(fZSize/2.0f)));
		float fXSize2 = fXSize * 1.5f;
		float fYSize2 = fYSize * 1.5f;
		float fZSize2 = fZSize * 1.5f;
		if ( iTipOverMode==1 ) fXSize2 = 0.005f/gSc;
		if ( iTipOverMode==2 ) fYSize2 = 0.005f/gSc;
		if ( iTipOverMode==3 ) fZSize2 = 0.005f/gSc;
		btCollisionShape* pShape2 = new btBoxShape(btVector3(btScalar(fXSize2/2.0f),btScalar(fYSize2/2.0f),btScalar(fZSize2/2.0f)));
		pCompound->addChildShape(localTrans, pShape1);
		pCompound->addChildShape(localTrans, pShape2);
		boxShape = (btCollisionShape*)pCompound;
	}

	// add shape to collision list so can release later, they can be "re-used" amongst bodies!!
	g_collisionShapes.push_back(boxShape);

	// mass calculation
	if (isDynamic == 0)
	{
		fVolume = 0;
	}
	else
	{
		// max
		if (fVolume > maxDynamicVolume) fVolume = maxDynamicVolume;

		// also handle min - ensure MASS not SO SMALL THINGS FLY MILLIONS OF MILES!
		if (fVolume < 0.1f) fVolume = 0.1f;
	}
	btScalar mass(fVolume/gMs);
	btVector3 localInertia(0,0,0);
	if ( isDynamic==1 ) boxShape->calculateLocalInertia(mass,localInertia);

	// set position transform for physics object
	btTransform startTransform;
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );

	// motionstate provides interpolation capabilities and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,boxShape,localInertia);
	rbInfo.m_friction = 1.0f;
	rbInfo.m_restitution = fRestitution/100.0f;
	btRigidBody* body = new btRigidBody(rbInfo);

	// pass material index as usrptr
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// also remove all contact response
	if ( bDoNotMoveAnyOtherObjects==true )
	{
		body->setCollisionFlags ( btCollisionObject::CF_NO_CONTACT_RESPONSE );
	}

	// add physics object to system
	short sColliderType = COL_OBJECT;
	if (isDynamic == 1) sColliderType = COL_OBJECT_DYNAMIC;
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	if (bDoNotCollideWithCapsuleCharacters == true)
	{
		// this mode is used for the physics box to project where characters ragdoll plus falls
		sColliderType = COL_CAPSULECHAR;
		sCollidesWith = COL_OBJECT | COL_TERRAIN; // ignore dynamic objects (so box does not sit ON TOP OF THEM
	}

	if (g_dynamicsWorld) 
	{
		if (bLeavePlayerAlone) 
		{
			//PE: Brass is hitting and adding forces to gun and player.
			//PE: Most easy way to fix this is get it setup as COL_CAPSULECHAR
			//PE: So collision with object and terrain still works, but its not part of the COL_OBJECT collision group.
			g_dynamicsWorld->addRigidBody(body, COL_CAPSULECHAR, sCollidesWith);
		}
		else 
		{
			g_dynamicsWorld->addRigidBody(body, sColliderType, sCollidesWith);
		}
	}
	// boyancy mode
	bool bBouyant = false;
	if ( isDynamic==1 ) 
		bBouyant = true; 
	else
		pObject = NULL;

	// add physics object details to pbject list
	ODEAddObject ( iObjectNumber, pObject, body, isDynamic, myMotionState, false, bBouyant, NULL, NULL, 1, 1, fFriction, mass );
	
	physicslock.unlock();
}

void StartStaticObject ( int iObjectNumber )
{
	pImporterCompoundShape = new btCompoundShape;
	pImporterMainObjectID = iObjectNumber;
}

void EndStaticObject ( int iObjectNumber, int iTerrainCollider )
{
	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fXSize = (pObject->collision.vecMax.x - pObject->collision.vecMin.x) * pObject->position.vecScale.x;
	float fYSize = (pObject->collision.vecMax.y - pObject->collision.vecMin.y) * pObject->position.vecScale.y;
	float fZSize = (pObject->collision.vecMax.z - pObject->collision.vecMin.z) * pObject->position.vecScale.z;
	float fXPos = pObject->position.vecPosition.x;
	float fYPos = pObject->position.vecPosition.y;
	float fZPos = pObject->position.vecPosition.z;
	float fXRot = GGToRadian(pObject->position.vecRotate.x);
	float fYRot = GGToRadian(pObject->position.vecRotate.y);
	float fZRot = GGToRadian(pObject->position.vecRotate.z);

	// adjust for center of object as all physics objects coords are at center
	GGVECTOR3 vColOffset = GGVECTOR3(0, 0, 0);
	GGMATRIX matRot;
	GGMatrixRotationX(&matRot, fXRot);	GGVec3TransformCoord(&vColOffset, &vColOffset, &matRot);
	GGMatrixRotationY(&matRot, fYRot);	GGVec3TransformCoord(&vColOffset, &vColOffset, &matRot);
	GGMatrixRotationZ(&matRot, fZRot); GGVec3TransformCoord(&vColOffset, &vColOffset, &matRot);
	fXPos += vColOffset.x;
	fYPos += vColOffset.y;
	fZPos += vColOffset.z;

	// real object
	sObject* pRealObject = pObject;
	if ( pObject->pInstanceOfObject ) pRealObject = pObject->pInstanceOfObject;
	
	// ensure very small objects are not allowed below this size
	if ( fXSize < 6 ) fXSize = 6;
	if ( fYSize < 6 ) fYSize = 6;
	if ( fZSize < 6 ) fZSize = 6;

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// add shape to collision list so can release later, they can be "re-used" amongst bodies!!
	g_collisionShapes.push_back(pImporterCompoundShape);

	// mass/volume calculation
	btScalar mass(0);
	btVector3 localInertia(0,0,0);

	// set position transform for physics object
	btTransform startTransform;
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );

	// motionstate provides interpolation capabilities and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,pImporterCompoundShape,localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	// pass material index as usrptr
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add physics object to system
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	if ( iTerrainCollider == 1 )
		g_dynamicsWorld->addRigidBody ( body, COL_TERRAIN, sCollidesWith);
	else
		g_dynamicsWorld->addRigidBody ( body, COL_OBJECT, sCollidesWith);

	// boyancy mode
	bool bBouyant = false;
	pObject = NULL;

	// add physics object details to pbject list
	ODEAddObject ( iObjectNumber, pObject, body, 0, myMotionState, false, bBouyant, NULL, NULL, 1, 1, 100.0f, mass );
}

int CreateHingeSingle( int iObjectNumber, float fX, float fY, float fZ, int iType, float fAngMin, float fAngMax )
{
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if ( pPhyObject == NULL ) return -1; 
	if ( pPhyObject->body == NULL ) return -1;
	if ( iType <= 0 || iType > 3 ) return -1;
	btVector3 vType;
	if (iType == 1) vType = btVector3(1, 0, 0);  // hinge on objects X axis
	if (iType == 2) vType = btVector3(0, 1, 0);  // hinge on objects Y axis
	if (iType == 3) vType = btVector3(0, 0, 1);  // hinge on objects Z axis

	btHingeConstraint *hingeC = new btHingeConstraint( *(pPhyObject->body),
		                                               btVector3(fX / gSc, fY / gSc, fZ / gSc),
		                                               vType,
		                                               true );
	if ( fAngMin != 0.0 || fAngMax != 0.0 )
	{
		hingeC->setLimit( btScalar( fAngMin ), btScalar( fAngMax ) );
	}
	g_dynamicsWorld->addConstraint( hingeC );

	// need to find constraint index and return it to caller (this is really naff!)
	for (int i = 0; i < g_dynamicsWorld->getNumConstraints(); i++)
	{
		btTypedConstraint *tC = g_dynamicsWorld->getConstraint(i);
		if ( tC == hingeC ) return i;
	}
	return -1;
}

void SetHingeLimits(int iConstraint, float fAngMin, float fAngMax, float fSoftness, float fBias, float fRelaxation)
{
	btHingeConstraint *hingeC = (btHingeConstraint*)g_dynamicsWorld->getConstraint(iConstraint);
	if (hingeC == NULL) return;

	hingeC->setLimit(btScalar(fAngMin), btScalar(fAngMax), btScalar(fSoftness), btScalar(fBias), btScalar(fRelaxation));
}

void SetHingeMotor( int iConstraint, float fSpeed, float fAngle, float fForce )
{
	btHingeConstraint *hingeC = (btHingeConstraint*)g_dynamicsWorld->getConstraint(iConstraint);
	if (hingeC == NULL) return;

	if ( fAngle != 0.0f )
	{
		hingeC->enableMotor( fSpeed != 0.0f  );
		hingeC->setMaxMotorImpulse( btScalar( fForce ) );
		hingeC->setMotorTarget( btScalar( fAngle ), btScalar( 0.1f ) );
	}
	else
	{
		hingeC->enableAngularMotor( fSpeed != 0.0f, btScalar( fSpeed ), btScalar( fForce ) );
	}
}

void SetSliderMotor( int iConstraint, bool OnOff, float fForce, float fVelocity )
{
	btSliderConstraint *sliderC = (btSliderConstraint*)g_dynamicsWorld->getConstraint(iConstraint);
	if (sliderC == NULL) return;

	sliderC->setPoweredLinMotor( OnOff );
	sliderC->setMaxLinMotorForce( btScalar( fForce ) );
	sliderC->setTargetLinMotorVelocity( btScalar( fVelocity ) );
}

float GetHingeAngle(int iConstraint)
{
	btHingeConstraint *hingeC = (btHingeConstraint*)g_dynamicsWorld->getConstraint(iConstraint);
	if (hingeC == NULL) return 0.0f;

	return (float)hingeC->getHingeAngle();
}

int CreateHingeDouble( int iObjectNumA, int iObjectNumB, float fXa, float fYa, float fZa,
	                                                     float fXb, float fYb, float fZb, 
	                                                     int iTypeA, int iTypeB, int noCollision )
{
	sObjectList* pPhyObjectA = ODEFindID( iObjectNumA );
	sObjectList* pPhyObjectB = ODEFindID( iObjectNumB );
	if ( pPhyObjectA == NULL || pPhyObjectB == NULL ) return -1;
	if ( pPhyObjectA->body == NULL || pPhyObjectB->body == NULL ) return -1;

	if ( iTypeA <= 0 || iTypeA > 3 ) return -1;
	if ( iTypeB <= 0 || iTypeB > 3 ) return -1;

	btVector3 vTypeA, vTypeB;
	if ( iTypeA == 1 ) vTypeA = btVector3( 1, 0, 0 );  // hinge on X axis
	if ( iTypeA == 2 ) vTypeA = btVector3( 0, 1, 0 );  // hinge on Y axis
	if ( iTypeA == 3 ) vTypeA = btVector3( 0, 0, 1 );  // hinge on Z axis
	if ( iTypeB == 1 ) vTypeB = btVector3( 1, 0, 0 );  // hinge on X axis
	if ( iTypeB == 2 ) vTypeB = btVector3( 0, 1, 0 );  // hinge on Y axis
	if ( iTypeB == 3 ) vTypeB = btVector3( 0, 0, 1 );  // hinge on Z axis

	btHingeConstraint *hingeC = new btHingeConstraint( *( pPhyObjectA->body ), 
		                                               *( pPhyObjectB->body ),
		                                               btVector3( fXa / gSc, fYa / gSc, fZa / gSc ),
		                                               btVector3( fXb / gSc, fYb / gSc, fZb / gSc ),
		                                               vTypeA, vTypeB,
		                                               true );
	g_dynamicsWorld->addConstraint( hingeC, noCollision == 1 );

	// need to find constraint index and return it to caller
	for (int i = 0; i < g_dynamicsWorld->getNumConstraints(); i++)
	{
		btTypedConstraint *tC = g_dynamicsWorld->getConstraint(i);
		if ( tC == hingeC ) return i;
	}
	return -1;
}

int CreateJointSingle( int iObjectNumber, float fX, float fY, float fZ )
{
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if ( pPhyObject == NULL ) return -1;
	if ( pPhyObject->body == NULL ) return -1;
	btPoint2PointConstraint *jointC = new btPoint2PointConstraint( *( pPhyObject->body ),
		                                                           btVector3( fX / gSc, fY / gSc, fZ / gSc ));
	g_dynamicsWorld->addConstraint( jointC );

	// need to find constraint index and return it to caller
	for (int i = 0; i < g_dynamicsWorld->getNumConstraints(); i++)
	{
		btTypedConstraint *tC = g_dynamicsWorld->getConstraint(i);
		if (tC == jointC) return i;
	}
	return -1;
}

int CreateJointDouble( int iObjectNumA, int iObjectNumB, float fXa, float fYa, float fZa, float fXb, float fYb, float fZb, int noCollision )
{
	sObjectList* pPhyObjectA = ODEFindID( iObjectNumA );
	sObjectList* pPhyObjectB = ODEFindID( iObjectNumB );
	if ( pPhyObjectA == NULL || pPhyObjectB == NULL ) return -1;
	if ( pPhyObjectA->body == NULL || pPhyObjectB->body == NULL ) return -1;

	btPoint2PointConstraint *jointC = new btPoint2PointConstraint( *( pPhyObjectA->body ),
		                                                           *( pPhyObjectB->body ),
		                                                           btVector3( fXa / gSc, fYa / gSc, fZa / gSc ),
		                                                           btVector3( fXb / gSc, fYb / gSc, fZb / gSc ) );
	g_dynamicsWorld->addConstraint( jointC, noCollision == 1 );

	// need to find constraint index and return it to caller
	for (int i = 0; i < g_dynamicsWorld->getNumConstraints(); i++)
	{
		btTypedConstraint *tC = g_dynamicsWorld->getConstraint(i);
		if ( tC == jointC ) return i;
	}
	return -1;
}

int CreateSliderDouble( int iObjectNumA, int iObjectNumB, float qXa, float qYa, float qZa, float qWa, float fXa, float fYa, float fZa,
	                                                      float qXb, float qYb, float qZb, float qWb, float fXb, float fYb, float fZb,
	                                                      bool useLinearReferenceFrameA )
{
	sObjectList* pPhyObjectA = ODEFindID(iObjectNumA);
	sObjectList* pPhyObjectB = ODEFindID(iObjectNumB);
	if (pPhyObjectA == NULL || pPhyObjectB == NULL) return -1;
	if (pPhyObjectA->body == NULL || pPhyObjectB->body == NULL) return -1;

	const btTransform& frameInA = btTransform( btQuaternion( qXa, qYa, qZa, qWa ), { fXa, fYa, fZa } );
	const btTransform& frameInB = btTransform( btQuaternion( qXb, qYb, qZb, qWb ), { fXb, fYb, fZb } );

	btSliderConstraint *sliderC = new btSliderConstraint( *(pPhyObjectA->body),
		                                                  *(pPhyObjectB->body), 
		                                                  frameInA, frameInB, 
		                                                  useLinearReferenceFrameA );

	g_dynamicsWorld->addConstraint( sliderC );

	// need to find constraint index and return it to caller
	for (int i = 0; i < g_dynamicsWorld->getNumConstraints(); i++)
	{
		btTypedConstraint *tC = g_dynamicsWorld->getConstraint(i);
		if ( tC == sliderC ) return i;
	}
	return -1;
}

void SetSliderLimits( int iConstraint, float fLowerLin, float fUpperLin, float fLowerAng, float fUpperAng )
{
	btSliderConstraint *sliderC = (btSliderConstraint*)g_dynamicsWorld->getConstraint(iConstraint);
	if (sliderC == NULL) return;

	sliderC->setLowerLinLimit( btScalar( fLowerLin ) );
	sliderC->setUpperLinLimit( btScalar( fUpperLin ) );
	sliderC->setLowerAngLimit( btScalar( fLowerAng ) );
	sliderC->setUpperAngLimit( btScalar( fUpperAng ) );
}

float GetSliderPosition( int iConstraint )
{
	btSliderConstraint *sliderC = (btSliderConstraint*)g_dynamicsWorld->getConstraint(iConstraint);
	if (sliderC == NULL) return 0.0f;

	return (float)sliderC->getLinearPos();
}

void AddStaticObjectBox ( int iObjectNumber , int iStaticBoxObject, int iMaterialRefIndex )
{
	// get reference object
	sObject* pObject = GetObjectData ( iStaticBoxObject );
	if ( pObject==NULL ) return;

	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	GetSizePositionRotation(pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, true);
	
	// real object
	sObject* pRealObject = pObject;
	if ( pObject->pInstanceOfObject ) pRealObject = pObject->pInstanceOfObject;
	
	// ensure very small objects are not allowed below this size
	if ( fXSize < 6 ) fXSize = 6;
	if ( fYSize < 6 ) fYSize = 6;
	if ( fZSize < 6 ) fZSize = 6;

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// create a bullet shape
	btCollisionShape* boxShape = new btBoxShape(btVector3(btScalar(fXSize/2.0f),btScalar(fYSize/2.0f),btScalar(fZSize/2.0f)));

	// assign material ref index to boxShape (for later use when detecting physics material)
	boxShape->setUserPointer ( (void*)iMaterialRefIndex );

	// add shape to collision list so can release later, they can be "re-used" amongst bodies!!
	g_collisionShapes.push_back(boxShape);

	// set position transform for physics object
	btTransform startTransform;
	
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );

	pImporterCompoundShape->addChildShape( startTransform, boxShape );
}

void CreateSphere ( int iObjectNumber, int isDynamic, float fWeight, float fFriction, float fRestitution )
{
	// defaults
	if ( fWeight==-1 ) fWeight = 100.0f;
	if ( fFriction==-1 ) fFriction = 90.0f;
	if ( fRestitution==-1 ) fRestitution = 0.0f;

	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fRadius = pObject->collision.fRadius * pObject->position.vecScale.x;

	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	GetSizePositionRotation(pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, true);
	
	// just before shape creation, reduce scale of size and position
	fRadius /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// volume calculation
	float fRadiusCubed = fRadius*fRadius*fRadius;
	float fVolume = ((4.0f/3.0f) * fRadiusCubed) * 3.14159f;

	// modify with weight modifier
	fVolume *= (fWeight/100.0f);

	// create a dynamic rigidbody
	btCollisionShape* colShape = new btSphereShape(btScalar(fRadius));
	g_collisionShapes.push_back(colShape);

	// mass/volume calculation
	if ( isDynamic==0 )
	{ 
		fVolume = 0;
	}
	else
	{
		// max
		if (fVolume > maxDynamicVolume) fVolume = maxDynamicVolume;

		// also handle min - ensure MASS not SO SMALL THINGS FLY MILLIONS OF MILES!
		if (fVolume < 0.1f) fVolume = 0.1f;
	}
	btScalar mass(fVolume/gMs);
	btVector3 localInertia(0,0,0);
	if ( isDynamic==1 ) colShape->calculateLocalInertia(mass,localInertia);

	// physics object pos and rotation
	btTransform startTransform;
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );
	
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,colShape,localInertia);
	rbInfo.m_friction = 1.0f;
	rbInfo.m_restitution = fRestitution/100.0f;
	btRigidBody* body = new btRigidBody(rbInfo);

	// pass material index as usrptr
	sObject* pRealObject = pObject;
	if ( pRealObject->pInstanceOfObject ) pRealObject = pRealObject->pInstanceOfObject;
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add physics object to system
	short sColliderType = COL_OBJECT;
	if (isDynamic == 1) sColliderType = COL_OBJECT_DYNAMIC;
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	g_dynamicsWorld->addRigidBody ( body, sColliderType, sCollidesWith);

	// boyancy mode
	bool bBouyant = false;
	if ( isDynamic==1 ) 
		bBouyant = true; 
	else
		pObject = NULL;

	// add physics object details to pbject list
	ODEAddObject ( iObjectNumber, pObject, body, isDynamic, myMotionState, false, bBouyant, NULL, NULL, 1, 1, fFriction, mass );
}

