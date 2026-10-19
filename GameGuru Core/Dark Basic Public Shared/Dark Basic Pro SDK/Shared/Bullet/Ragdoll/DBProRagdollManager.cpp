
// BulletPhysicsWrapper for DarkBasic Proffessional
//Stab In The Dark Software 
//Copyright (c) 2002-2014
//http://stabinthedarksoftware.com

///#include "StdAfx.h"
#include "DBProRagdollManager.h"
#include "DBProRagDoll.h"
#include "DBPro.hpp"

#include ".\..\..\Error\CError.h"

DBProRagdollManager* ragdollManager = NULL;// now in constructor new DBProRagdollManager();

//-------------------------------------

DBProRagdollManager::DBProRagdollManager()
{
}

DBProRagdollManager::~DBProRagdollManager()
{
}

void DBProRagdollManager::AddRagdoll(DBProRagDoll* ragdoll)
{
	AddItem(ragdoll);
}

void DBProRagdollManager::DeleteRagdoll(int ragdollID)
{
	DBProRagDoll* ragdoll = GetRagdoll(ragdollID);
	if(ragdoll)
	{
		ragdoll->ResetObjectParametersForAnimation();
		ragdoll->ResetObjectParametersForCulling();
		RemoveItem(ragdollID);
	}
}

DBProRagDoll* DBProRagdollManager::GetRagdoll(int ragdollID)
{
	return (DBProRagDoll*)GetItem(ragdollID);
}

int DBProRagdollManager::GetIDFromBoneObject(int objectID)
{
	for(int i = 0; i < m_data.size(); i++)
	{
		DBProRagDoll* ragdoll = (DBProRagDoll*)m_data[i];
		if(ragdoll->IsBoneObject(objectID))
		{
			return ragdoll->GetID();
		}
	}
	return -1;
}

void DBProRagdollManager::Update()
{
	for(int i = 0; i < m_data.size(); i++)
	{	
		DBProRagDoll* ragdoll = ((DBProRagDoll*)m_data[i]);
		if(ragdoll->IsSleeping()==false)
		{
			ragdoll->Update();
		}
		else
		{
			ragdoll->ResetObjectParametersForCulling();
		}
	}
}

// GG: the time a physics update covers, for each ragdoll's hold to spread its anchor's move over its steps
void DBProRagdollManager::HoldFrame(float frameTime)
{
	for(int i = 0; i < m_data.size(); i++)
	{
		((DBProRagDoll*)m_data[i])->HoldFrame(frameTime);
	}
}

// GG: before each simulation step: held anchors move on, released joints stiffen back
void DBProRagdollManager::HoldStep(float timeStep)
{
	for(int i = 0; i < m_data.size(); i++)
	{
		((DBProRagDoll*)m_data[i])->HoldStep(timeStep);
	}
}

void DBProRagdollManager::AssertRagdollExist(int ragdollID, LPCSTR message, bool bExist /*= true*/) 
{
	if(bExist && ragdollManager->GetRagdoll(ragdollID)==NULL)
	{
		RunTimeError(0,(LPSTR)message);
	}
	else if(!bExist && ragdollManager->GetRagdoll(ragdollID)!=NULL)
	{
		RunTimeError(0,(LPSTR)message);
	}
}



