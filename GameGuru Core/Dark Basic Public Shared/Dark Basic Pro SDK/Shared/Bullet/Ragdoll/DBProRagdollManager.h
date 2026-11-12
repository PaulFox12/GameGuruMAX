#pragma once

#include "Windows.h"
#include "DBProRagDoll.h"
#include "BaseItemManager.h"

class DBProRagdollManager : public BaseItemManager
{
public:
	DBProRagdollManager();
	~DBProRagdollManager();

	void AddRagdoll(DBProRagDoll* ragdoll);
	void DeleteRagdoll(int ragdollID);
	DBProRagDoll* GetRagdoll(int ragdollID);
	void Update(); 
	// GG: a held ragdoll's anchor moved through a physics update's steps (BulletPhysics.CPP's pre-tick callback)
	void HoldFrame(float frameTime);
	void HoldStep(float timeStep);
	int GetIDFromBoneObject(int objectID);
	// GG: how many ragdolls there are, how many are static, how many awake ones are all but still (every body under 4
	// world units a second and 1 radian a second), the bodies awake, and the fastest awake body's speeds
	void Counts(int* pTotal, int* pStatic, int* pAwakeSlow, int* pBodiesAwake, float* pMaxLinear, float* pMaxAngular);
	static void AssertRagdollExist(int ragdollID, LPCSTR message, bool bExist = true); 

private:

};

extern DBProRagdollManager* ragdollManager;
