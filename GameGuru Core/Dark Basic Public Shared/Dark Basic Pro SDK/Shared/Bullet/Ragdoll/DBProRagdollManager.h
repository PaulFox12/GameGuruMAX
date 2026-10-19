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
	static void AssertRagdollExist(int ragdollID, LPCSTR message, bool bExist = true); 

private:

};

extern DBProRagdollManager* ragdollManager;
