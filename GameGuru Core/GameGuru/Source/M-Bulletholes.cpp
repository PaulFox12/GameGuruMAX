//----------------------------------------------------
//--- GAMEGURU - M-Bulletholes
//----------------------------------------------------

// includes
#include "stdafx.h"
#include "gameguru.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#include ".\..\..\Guru-WickedMAX\wickedcalls.h"

// globals
#define BULLETHOLESMAX 1000
// holes have no lifetime: with every slot taken a new hole takes the oldest hole's slot that is out of view (outside the
// camera's view or past BULLETHOLESUNSEENDIST, where a hole is under a pixel), else the oldest; one entity carries at most
// BULLETHOLESPEROWNER, its oldest giving way
#define BULLETHOLESPEROWNER 50
#define BULLETHOLESUNSEENDIST 5000.0f
// the holes of an entity that moves go in a small object of their own, in its space, that follows its world matrix, so nothing
// is sent to the renderer while it moves (rebuilding a mesh's render data cost 0.2 to 0.5 ms however small the mesh); they go
// back to the main mesh after BULLETHOLESMOVERHOLD frames still. Objects 3481-3488, free below g.bulletholesobject (Common.cpp)
#define BULLETHOLESMOVERS 8
#define BULLETHOLESMOVERHOLD 120
#define BULLETHOLESMOVEROBJECT 3481
struct sBulletHoleMover
{
	int iOwnerObject; // 0 while free
	int iHoles;
	int iLastMovedFrame;
	bool bMovedThisFrame;
	bool bDirty;
	bool bFullRebuild;
	bool bSlotUsed[BULLETHOLESPEROWNER];
};
sBulletHoleMover g_bulletholemovers[BULLETHOLESMOVERS];
int g_iBulletHoleFrame = 0;
int g_iBulletHoleMoverLogs = 40;

struct sBulletHole
{
	int iVertIndexStart;
	int iMover;
	int iMoverSlot;
	GGVECTOR3 vecQuadUV[4];
	unsigned int uSerial;
	float fBornTime;
	float fRadius;
	GGVECTOR3 vecWorldPos;

	// the entity the hole was made on (0 for terrain), its object, and where that object stood when hit. The hole is removed
	// when the entity hides or goes. On anything but a character it follows the object as it moves: it keeps its quad and
	// normal in the object's own space and is placed again, or carried by a mover, whenever the object's world matrix
	// changes. A character's holes are removed when it moves instead, as a flat quad can't bend with it
	int iOwnerEntity;
	int iOwnerObject;
	GGVECTOR3 vecOwnerPos;
	GGVECTOR3 vecOwnerAngle;
	bool bFollowsOwner;
	GGVECTOR3 vecLocalQuad[4];
	GGVECTOR3 vecLocalNormal;
	GGMATRIX matOwnerWorld;
};
std::vector<sBulletHole> g_bulletholes;

bool g_bulletholeavailable[BULLETHOLESMAX];
GGVECTOR3 g_vecBulletHoleQuad[4];
GGVECTOR3 g_vecBulletHoleQuadUV[4];

// the bullet hole mesh has changed and the renderer's copy must follow; bulletholes_update refreshes it once a frame. The
// first refresh after bulletholes_init rebuilds the whole Wicked object (texture, transparency, render layer), later ones
// copy the vertices into its mesh, as rebuilding the object for every hole cost about 10 ms a hole
bool g_bBulletHolesDirty = false;
bool g_bBulletHolesFullRebuild = true;

// the order holes were made in (the oldest has the lowest), and holes recycled this level for the log, in all and by reason
// (out of view, all in view, an entity's cap)
unsigned int g_uBulletHoleSerial = 0;
int g_iBulletHoleRecycles = 0;
int g_iBulletHoleRecyclesBy[3] = { 0, 0, 0 };

// functions

void bulletholes_changesinglehole (int iVertIndex, float fNX, float fNY, float fNZ);
static void bulletholes_leavemover (sBulletHole* pBulletHole);

// true while a hole could be seen: inside the camera's view and near enough to cover a pixel
static bool bulletholes_inview (sBulletHole* pBulletHole)
{
	const wiScene::CameraComponent& camera = wiScene::GetCamera();
	float fDX = pBulletHole->vecWorldPos.x - camera.Eye.x;
	float fDY = pBulletHole->vecWorldPos.y - camera.Eye.y;
	float fDZ = pBulletHole->vecWorldPos.z - camera.Eye.z;
	if (fDX*fDX + fDY*fDY + fDZ*fDZ > BULLETHOLESUNSEENDIST*BULLETHOLESUNSEENDIST) return false;
	return camera.frustum.CheckSphere(XMFLOAT3(pBulletHole->vecWorldPos.x, pBulletHole->vecWorldPos.y, pBulletHole->vecWorldPos.z), pBulletHole->fRadius * 2.0f);
}

// takes hole b out of the mesh and the list and frees its slot
static void bulletholes_removeat (int b)
{
	if (g_bulletholes[b].iMover >= 0) bulletholes_leavemover (&g_bulletholes[b]);
	else bulletholes_changesinglehole (g_bulletholes[b].iVertIndexStart, 0, 0, 0);
	g_bulletholeavailable[g_bulletholes[b].iVertIndexStart / 6] = false;
	g_bulletholes.erase(g_bulletholes.begin() + b);
	g_bBulletHolesDirty = true;
}

// removes hole b to make room for a new one (iWhy 0 out of view, 1 all in view, 2 its entity's cap), logged with the totals
// by reason: each of the first 50, after them a line of totals at most every 2 seconds (bulletholes_update)
static void bulletholes_recycle (int b, int iWhy)
{
	const char* pWhy[3] = { "unseen", "all in view", "owner cap" };
	g_iBulletHoleRecycles++;
	g_iBulletHoleRecyclesBy[iWhy]++;
	if (g_iBulletHoleRecycles <= 50)
	{
		char pLog[256];
		sprintf_s(pLog, "BULLETHOLE RECYCLE slot %d (owner %d, age %.0f s, %s); recycled %d: unseen %d, all in view %d, owner cap %d; %d holes",
			g_bulletholes[b].iVertIndexStart / 6, g_bulletholes[b].iOwnerEntity, (float)GetTickCount() / 1000.0f - g_bulletholes[b].fBornTime,
			pWhy[iWhy], g_iBulletHoleRecycles, g_iBulletHoleRecyclesBy[0], g_iBulletHoleRecyclesBy[1], g_iBulletHoleRecyclesBy[2], (int)g_bulletholes.size());
		timestampactivity(0, pLog);
	}
	bulletholes_removeat(b);
}

static void bulletholes_refreshanobject (int iObject, bool* pbFullRebuild)
{
	if (ObjectExist(iObject) == 0) return;
	sObject* pObject = GetObjectData(iObject);
	if (!pObject) return;
	if (*pbFullRebuild == true || pObject->iMeshCount < 1 || !pObject->ppMeshList || !pObject->ppMeshList[0])
	{
		WickedCall_RemoveObject(pObject);
		WickedCall_AddObject(pObject);
		WickedCall_TextureObject(pObject, NULL);
		WickedCall_SetObjectCastShadows(pObject, false);
		WickedCall_SetObjectTransparent(pObject);
		WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_CURSOROBJECT);
		*pbFullRebuild = false;
	}
	else
	{
		// positions, normals and UVs; recreating the render data also refits the mesh bounds to the holes
		WickedCall_UpdateMeshVertexData(pObject->ppMeshList[0], true);
	}
}

void bulletholes_refreshobject (void)
{
	if (g_bBulletHolesDirty == true)
	{
		bulletholes_refreshanobject (g.bulletholesobject, &g_bBulletHolesFullRebuild);
		g_bBulletHolesDirty = false;
	}
	for (int m = 0; m < BULLETHOLESMOVERS; m++)
	{
		if (g_bulletholemovers[m].bDirty == false) continue;
		bulletholes_refreshanobject (BULLETHOLESMOVEROBJECT + m, &g_bulletholemovers[m].bFullRebuild);
		g_bulletholemovers[m].bDirty = false;
	}
}

// makes a hole object with room for iHoles quads of six vertices each
static void bulletholes_makeobject (int iObject, int iHoles)
{
	if (ObjectExist(iObject) == 0)
	{
		// create bullethole memblock mesh
		int iMemblockIndex = 0;
		for (int i = 1; i <= 257; i++)
		{
			if (MemblockExist(i) == 0)
			{
				iMemblockIndex = i;
				break;
			}
		}
		if (iMemblockIndex == 0) return;
		int vertsize = 12+12+8;
		int iVertexCount = iHoles * 6;
		int iSizeBytes = 12; // header size
		iSizeBytes += vertsize * iVertexCount;
		MakeMemblock(iMemblockIndex, iSizeBytes);
		WriteMemblockDWord(iMemblockIndex, 0, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1);
		WriteMemblockDWord(iMemblockIndex, 4, vertsize);
		WriteMemblockDWord(iMemblockIndex, 8, iVertexCount);

		// create bullethole master object
		int iWorkMeshID = g.meshgeneralwork2;
		if (GetMeshExist(iWorkMeshID) == 1) DeleteMesh(iWorkMeshID);
		CreateMeshFromMemblock(iWorkMeshID, iMemblockIndex);
		MakeObject(iObject, iWorkMeshID, 0);
		if (GetMeshExist(iWorkMeshID) == 1) DeleteMesh(iWorkMeshID);
		if (MemblockExist(iMemblockIndex) == 1) DeleteMemblock(iMemblockIndex);

		// set bulletholes object for rendering
		LoadImage("gamecore\\bulletholes\\bulletholes_color.dds", g.bulletholeimage);
		TextureObject(iObject, g.bulletholeimage);
		SetObjectTransparency(iObject, 1);
		SetObjectCull(iObject, 0);
	}
}

void bulletholes_init (void)
{
	bulletholes_makeobject (g.bulletholesobject, BULLETHOLESMAX);
	for (int m = 0; m < BULLETHOLESMOVERS; m++)
	{
		bulletholes_makeobject (BULLETHOLESMOVEROBJECT + m, BULLETHOLESPEROWNER);
		sObject* pObject = GetObjectData(BULLETHOLESMOVEROBJECT + m);
		if (pObject) pObject->position.bCustomWorldMatrix = true;
	}

	//  clear all bulletholes
	bulletholes_clearall();

	// the first change in this level rebuilds the renderer's objects
	g_bBulletHolesFullRebuild = true;
	for (int m = 0; m < BULLETHOLESMOVERS; m++) g_bulletholemovers[m].bFullRebuild = true;
}

void bulletholes_clearall (void)
{
	// wipe out data in object
	if (ObjectExist(g.bulletholesobject) == 1)
	{
		LockVertexDataForLimbCore(g.bulletholesobject, 0, 1);
		for (int iVertIndex = 0; iVertIndex < BULLETHOLESMAX * 6; iVertIndex++)
		{
			SetVertexDataPosition(iVertIndex, 0, 0, 0);
		}
		UnlockVertexData();
	}
	for (int m = 0; m < BULLETHOLESMOVERS; m++)
	{
		sBulletHoleMover* pMover = &g_bulletholemovers[m];
		bool bFullRebuild = pMover->bFullRebuild;
		memset(pMover, 0, sizeof(sBulletHoleMover));
		pMover->bFullRebuild = bFullRebuild;
		pMover->bDirty = true;
		if (ObjectExist(BULLETHOLESMOVEROBJECT + m) == 0) continue;
		LockVertexDataForLimbCore(BULLETHOLESMOVEROBJECT + m, 0, 1);
		for (int iVertIndex = 0; iVertIndex < BULLETHOLESPEROWNER * 6; iVertIndex++)
		{
			SetVertexDataPosition(iVertIndex, 0, 0, 0);
		}
		UnlockVertexData();
	}
	g_iBulletHoleMoverLogs = 40;

	// clear the list
	g_bulletholes.clear();
	memset(g_bulletholeavailable, 0, sizeof(g_bulletholeavailable));
	g_iBulletHoleRecycles = 0;
	g_iBulletHoleRecyclesBy[0] = g_iBulletHoleRecyclesBy[1] = g_iBulletHoleRecyclesBy[2] = 0;
	g_bBulletHolesDirty = true;
}

void bulletholes_changesinglehole (int iVertIndex, float fNX, float fNY, float fNZ)
{
	// needs g_vecBulletHoleQuad and g_vecBulletHoleQuadUV globals
	bool bBulletHoleValid = true; if (fNX == 0 && fNY == 0 & fNZ == 0) bBulletHoleValid = false;
	LockVertexDataForLimbCore(g.bulletholesobject, 0, 1);
	for (int v = 0; v < 6; v++)
	{
		int iWhichCornerOfQuad = 0;
		if (v == 1) iWhichCornerOfQuad = 1;
		if (v == 2) iWhichCornerOfQuad = 2;
		if (v == 3) iWhichCornerOfQuad = 1;
		if (v == 4) iWhichCornerOfQuad = 3;
		if (v == 5) iWhichCornerOfQuad = 2;
		if (bBulletHoleValid == true)
		{
			SetVertexDataPosition(iVertIndex + v, g_vecBulletHoleQuad[iWhichCornerOfQuad].x, g_vecBulletHoleQuad[iWhichCornerOfQuad].y, g_vecBulletHoleQuad[iWhichCornerOfQuad].z);
			SetVertexDataNormals(iVertIndex + v, fNX, fNY, fNZ);
			SetVertexDataUV(iVertIndex + v, g_vecBulletHoleQuadUV[iWhichCornerOfQuad].x, g_vecBulletHoleQuadUV[iWhichCornerOfQuad].y);
		}
		else
		{
			SetVertexDataPosition(iVertIndex + v, 0, 0, 0);
		}
	}
	UnlockVertexData();
}

// sets a hole's six vertices in a hole object: its corners, normal and texture coordinates
static void bulletholes_placesinglehole (int iObject, int iVertIndex, GGVECTOR3* pQuad, GGVECTOR3 vecNormal, GGVECTOR3* pUV)
{
	const int iWhichCornerOfQuad[6] = { 0, 1, 2, 1, 3, 2 };
	LockVertexDataForLimbCore(iObject, 0, 1);
	for (int v = 0; v < 6; v++)
	{
		GGVECTOR3* pCorner = &pQuad[iWhichCornerOfQuad[v]];
		GGVECTOR3* pCornerUV = &pUV[iWhichCornerOfQuad[v]];
		SetVertexDataPosition(iVertIndex + v, pCorner->x, pCorner->y, pCorner->z);
		SetVertexDataNormals(iVertIndex + v, vecNormal.x, vecNormal.y, vecNormal.z);
		SetVertexDataUV(iVertIndex + v, pCornerUV->x, pCornerUV->y);
	}
	UnlockVertexData();
}

// the mover carrying an owner object's holes, -1 if none
static int bulletholes_findmover (int iOwnerObject)
{
	for (int m = 0; m < BULLETHOLESMOVERS; m++)
		if (g_bulletholemovers[m].iOwnerObject == iOwnerObject)
			return m;
	return -1;
}

// puts a hole in its owner's mover, taking a free one if it has none; false with none free, and the hole stays in the main mesh
static bool bulletholes_joinmover (sBulletHole* pBulletHole)
{
	int iMover = bulletholes_findmover(pBulletHole->iOwnerObject);
	if (iMover < 0)
	{
		iMover = bulletholes_findmover(0);
		if (iMover < 0) return false;
		g_bulletholemovers[iMover].iOwnerObject = pBulletHole->iOwnerObject;
		g_bulletholemovers[iMover].bMovedThisFrame = true;
		if (g_iBulletHoleMoverLogs > 0)
		{
			g_iBulletHoleMoverLogs--;
			char pLog[128];
			sprintf_s(pLog, "BULLETHOLE MOVER %d carries the holes of object %d", iMover, pBulletHole->iOwnerObject);
			timestampactivity(0, pLog);
		}
	}
	sBulletHoleMover* pMover = &g_bulletholemovers[iMover];
	for (int iSlot = 0; iSlot < BULLETHOLESPEROWNER; iSlot++)
	{
		if (pMover->bSlotUsed[iSlot] == true) continue;
		GGVECTOR3 vecNormal;
		GGVec3Normalize(&vecNormal, &pBulletHole->vecLocalNormal);
		bulletholes_placesinglehole(BULLETHOLESMOVEROBJECT + iMover, iSlot * 6, pBulletHole->vecLocalQuad, vecNormal, pBulletHole->vecQuadUV);
		pMover->bSlotUsed[iSlot] = true;
		pMover->iHoles++;
		pMover->iLastMovedFrame = g_iBulletHoleFrame;
		pMover->bDirty = true;
		pBulletHole->iMover = iMover;
		pBulletHole->iMoverSlot = iSlot;
		return true;
	}
	return false;
}

// takes a hole out of its mover, which is free again once it carries none
static void bulletholes_leavemover (sBulletHole* pBulletHole)
{
	sBulletHoleMover* pMover = &g_bulletholemovers[pBulletHole->iMover];
	LockVertexDataForLimbCore(BULLETHOLESMOVEROBJECT + pBulletHole->iMover, 0, 1);
	for (int v = 0; v < 6; v++) SetVertexDataPosition((pBulletHole->iMoverSlot * 6) + v, 0, 0, 0);
	UnlockVertexData();
	pMover->bSlotUsed[pBulletHole->iMoverSlot] = false;
	pMover->bDirty = true;
	pMover->iHoles--;
	if (pMover->iHoles <= 0)
	{
		if (g_iBulletHoleMoverLogs > 0)
		{
			g_iBulletHoleMoverLogs--;
			char pLog[128];
			sprintf_s(pLog, "BULLETHOLE MOVER %d free again (object %d)", pBulletHole->iMover, pMover->iOwnerObject);
			timestampactivity(0, pLog);
		}
		pMover->iOwnerObject = 0;
		pMover->bMovedThisFrame = false;
	}
	pBulletHole->iMover = -1;
	pBulletHole->iMoverSlot = -1;
}

void bulletholes_add (int iMaterialIndex, float fX, float fY, float fZ, float fNX, float fNY, float fNZ, int iOwnerEntity)
{
	// no holes for silent materials
	if (iMaterialIndex <= 0)
		return;

	// map materialindex to atlas (could make this a settings file to update via text file easily in the future)
	float fBulletHoleRadius = 1.25f;
	int iAtlasIndex = iMaterialIndex;
	switch (iMaterialIndex)
	{
		case 0: iAtlasIndex = 1; fBulletHoleRadius = 0.50f; break; // generic
		case 1: iAtlasIndex = 0; fBulletHoleRadius = 1.00f; break; // stone
		case 2: iAtlasIndex = 3; fBulletHoleRadius = 0.35f; break; // metal
		case 3: iAtlasIndex = 4; fBulletHoleRadius = 0.75f; break; // wood
		case 4: iAtlasIndex = 2; fBulletHoleRadius = 0.75f; break; // glass
	}

	// instant reject if near an existing bullethole (avoid zclash); squared distances, as this runs over every hole
	GGVECTOR3 vecPointInWorldSpace = GGVECTOR3(fX, fY, fZ);
	for (int n = 0; n < (int)g_bulletholes.size(); n++)
	{
		float fDX = g_bulletholes[n].vecWorldPos.x - vecPointInWorldSpace.x;
		float fDY = g_bulletholes[n].vecWorldPos.y - vecPointInWorldSpace.y;
		float fDZ = g_bulletholes[n].vecWorldPos.z - vecPointInWorldSpace.z;
		float fNear = g_bulletholes[n].fRadius + fBulletHoleRadius;
		if (fDX*fDX + fDY*fDY + fDZ*fDZ < fNear*fNear)
		{
			// bullethole too near an existing one
			return;
		}
	}

	// an entity at its cap gives up its oldest hole
	if (iOwnerEntity > 0)
	{
		int iOwnerCount = 0;
		int iOwnerOldest = -1;
		for (int n = 0; n < (int)g_bulletholes.size(); n++)
		{
			if (g_bulletholes[n].iOwnerEntity != iOwnerEntity) continue;
			iOwnerCount++;
			if (iOwnerOldest < 0 || g_bulletholes[n].uSerial < g_bulletholes[iOwnerOldest].uSerial) iOwnerOldest = n;
		}
		if (iOwnerCount >= BULLETHOLESPEROWNER && iOwnerOldest >= 0) bulletholes_recycle(iOwnerOldest, 2);
	}

	// find free vertindex to place bullethole
	int iBulletSlot;
	for (iBulletSlot = 0; iBulletSlot < BULLETHOLESMAX; iBulletSlot++)
		if (g_bulletholeavailable[iBulletSlot] == false)
			break;

	// every slot taken: the oldest hole out of view gives up its slot, else the oldest
	if (iBulletSlot >= BULLETHOLESMAX && g_bulletholes.size() > 0)
	{
		int iOldestUnseen = -1;
		int iOldest = -1;
		for (int n = 0; n < (int)g_bulletholes.size(); n++)
		{
			if (iOldest < 0 || g_bulletholes[n].uSerial < g_bulletholes[iOldest].uSerial) iOldest = n;
			if (iOldestUnseen >= 0 && g_bulletholes[n].uSerial >= g_bulletholes[iOldestUnseen].uSerial) continue;
			if (bulletholes_inview(&g_bulletholes[n]) == false) iOldestUnseen = n;
		}
		int iVictim = iOldestUnseen >= 0 ? iOldestUnseen : iOldest;
		iBulletSlot = g_bulletholes[iVictim].iVertIndexStart / 6;
		bulletholes_recycle(iVictim, iOldestUnseen >= 0 ? 0 : 1);
	}

	// add bullethole if found a free space
	if (iBulletSlot < BULLETHOLESMAX)
	{
		// make coordinates for the bullethole quad
		g_vecBulletHoleQuad[0] = GGVECTOR3(-2, -2, 0);
		g_vecBulletHoleQuad[1] = GGVECTOR3( 2, -2, 0);
		g_vecBulletHoleQuad[2] = GGVECTOR3(-2,  2, 0);
		g_vecBulletHoleQuad[3] = GGVECTOR3( 2,  2, 0);
		GGMATRIX matFinalOrientation;
		float fRandomSpin = rand() % 360; // spin hole around 360 degrees randomly
		GGMatrixRotationZ(&matFinalOrientation, GGToRadian(fRandomSpin)); 
		float fAngleX, fAngleY, fAngleZ;
		GetAngleFromPoint (0, 0, 0, fNX, fNY, fNZ, &fAngleX, &fAngleY, &fAngleZ);
		GGMATRIX matRotX; GGMatrixRotationX(&matRotX, GGToRadian(fAngleX));
		GGMATRIX matRotY; GGMatrixRotationY(&matRotY, GGToRadian(fAngleY));
		GGMATRIX matRotZ; GGMatrixRotationZ(&matRotZ, GGToRadian(fAngleZ));
		GGMATRIX matNormalDirectionRot;
		GGMatrixIdentity(&matNormalDirectionRot);
		GGMatrixMultiply(&matNormalDirectionRot, &matNormalDirectionRot, &matRotX);
		GGMatrixMultiply(&matNormalDirectionRot, &matNormalDirectionRot, &matRotY);
		GGMatrixMultiply(&matNormalDirectionRot, &matNormalDirectionRot, &matRotZ);
		GGMatrixMultiply(&matFinalOrientation, &matFinalOrientation, &matNormalDirectionRot);
		GGVECTOR3 vecPointInWorldSpace = GGVECTOR3(fX, fY, fZ);
		GGVECTOR3 vecNormalizedDir = GGVECTOR3(fNX, fNY, fNZ);
		GGVec3Normalize(&vecNormalizedDir, &vecNormalizedDir);
		// bring actual point slightly out of surface so as not to clash with it
		// LB: not happy with this for terrain, the physics geometry does not match the visual one, so bulletholes float, or sink under the terrain floor!
		// every so slightly for regular things like walls, flat static objects, etc (again if the physics shape is very different, float or sink again)
		GGVECTOR3 vecShiftedPointInWorldSpace = vecPointInWorldSpace + (vecNormalizedDir * 0.1f);
		for (int p = 0; p < 4; p++)
		{
			GGVec3TransformCoord(&g_vecBulletHoleQuad[p], &g_vecBulletHoleQuad[p], &matFinalOrientation);
			g_vecBulletHoleQuad[p] += vecShiftedPointInWorldSpace;
		}

		// work out UV based on material index
		float U_f = 0.0f;
		float V_f = 0.0f;
		float USize_f = 1.0f / 4.0f;
		float VSize_f = 1.0f / 4.0f;
		if (iAtlasIndex != 0)
		{
			int across = int(iAtlasIndex / 4.0f);
			V_f = VSize_f * across;
			U_f = iAtlasIndex * USize_f ;
		}

		//MD: Bullet holes had white atrefacts due to float inaccuracy, shaving off the edges fixed this
		g_vecBulletHoleQuadUV[0] = GGVECTOR3(U_f + 0.01f, V_f + 0.01f, 0);
		g_vecBulletHoleQuadUV[1] = GGVECTOR3(U_f + USize_f - 0.01f, V_f + 0.01f, 0);
		g_vecBulletHoleQuadUV[2] = GGVECTOR3(U_f + 0.01f, V_f + VSize_f - 0.01f, 0);
		g_vecBulletHoleQuadUV[3] = GGVECTOR3(U_f + USize_f - 0.01f, V_f + VSize_f - 0.01f, 0);

		// add bullet hole to list
		sBulletHole bullethole;
		bullethole.iVertIndexStart = iBulletSlot * 6;
		bullethole.iMover = -1;
		bullethole.iMoverSlot = -1;
		for (int p = 0; p < 4; p++) bullethole.vecQuadUV[p] = g_vecBulletHoleQuadUV[p];
		bullethole.uSerial = g_uBulletHoleSerial++;
		bullethole.fBornTime = (float)GetTickCount() / 1000.0f;
		bullethole.fRadius = fBulletHoleRadius;
		bullethole.vecWorldPos = vecPointInWorldSpace;
		bullethole.iOwnerEntity = 0;
		bullethole.iOwnerObject = 0;
		bullethole.vecOwnerPos = GGVECTOR3(0, 0, 0);
		bullethole.vecOwnerAngle = GGVECTOR3(0, 0, 0);
		bullethole.bFollowsOwner = false;
		if (iOwnerEntity > 0 && iOwnerEntity < (int)t.entityelement.size())
		{
			int iObj = t.entityelement[iOwnerEntity].obj;
			if (iObj > 0 && ObjectExist(iObj) == 1)
			{
				bullethole.iOwnerEntity = iOwnerEntity;
				bullethole.iOwnerObject = iObj;
				bullethole.vecOwnerPos = GGVECTOR3(ObjectPositionX(iObj), ObjectPositionY(iObj), ObjectPositionZ(iObj));
				bullethole.vecOwnerAngle = GGVECTOR3(ObjectAngleX(iObj), ObjectAngleY(iObj), ObjectAngleZ(iObj));

				// on anything but a character, the hole keeps its place on the object as it moves
				sObject* pOwnerObject = GetObjectData(iObj);
				if (pOwnerObject && t.entityprofile[t.entityelement[iOwnerEntity].bankindex].ischaracter == 0)
				{
					GGMATRIX matInverse;
					float fDeterminant = 0.0f;
					GGMatrixInverse(&matInverse, &fDeterminant, &pOwnerObject->position.matWorld);
					if (fabs(fDeterminant) > 0.000001f)
					{
						for (int p = 0; p < 4; p++) GGVec3TransformCoord(&bullethole.vecLocalQuad[p], &g_vecBulletHoleQuad[p], &matInverse);
						GGVec3TransformNormal(&bullethole.vecLocalNormal, &vecNormalizedDir, &matInverse);
						bullethole.matOwnerWorld = pOwnerObject->position.matWorld;
						bullethole.bFollowsOwner = true;
					}
				}
			}
		}
		g_bulletholes.push_back(bullethole);
		g_bulletholeavailable[iBulletSlot] = true;

		// add vert data to represent this bullet hole, seen after the next bulletholes_update: on an entity whose holes
		// have a mover, straight into it
		sBulletHole* pNewHole = &g_bulletholes.back();
		if (pNewHole->bFollowsOwner == false || bulletholes_findmover(pNewHole->iOwnerObject) < 0 || bulletholes_joinmover(pNewHole) == false)
		{
			bulletholes_changesinglehole (bullethole.iVertIndexStart, fNX, fNY, fNZ);
			g_bBulletHolesDirty = true;
		}
	}
}

// true while the entity a hole was made on still has the object it was hit on, shown
static bool bulletholes_ownerpresent (sBulletHole* pBulletHole)
{
	int e = pBulletHole->iOwnerEntity;
	if (e >= (int)t.entityelement.size()) return false;
	int iObj = t.entityelement[e].obj;
	return iObj == pBulletHole->iOwnerObject && ObjectExist(iObj) == 1 && GetVisible(iObj) == 1;
}

// true unless two world matrices differ by more than a hair
static bool bulletholes_samematrix (const GGMATRIX* pA, const GGMATRIX* pB)
{
	const float* pFloatA = (const float*)pA;
	const float* pFloatB = (const float*)pB;
	for (int i = 0; i < 16; i++) if (fabs(pFloatA[i] - pFloatB[i]) > 0.0001f) return false;
	return true;
}

// true while the entity a hole was made on is still there, shown, and where it was when hit (within a few units
// and degrees, so a settling physics body does not count as moving)
static bool bulletholes_ownerunchanged (sBulletHole* pBulletHole)
{
	if (bulletholes_ownerpresent(pBulletHole) == false) return false;
	int iObj = pBulletHole->iOwnerObject;
	float fDX = ObjectPositionX(iObj) - pBulletHole->vecOwnerPos.x;
	float fDY = ObjectPositionY(iObj) - pBulletHole->vecOwnerPos.y;
	float fDZ = ObjectPositionZ(iObj) - pBulletHole->vecOwnerPos.z;
	if (fDX*fDX + fDY*fDY + fDZ*fDZ > 4.0f*4.0f) return false;
	float fAngleDiff[3] = { ObjectAngleX(iObj) - pBulletHole->vecOwnerAngle.x, ObjectAngleY(iObj) - pBulletHole->vecOwnerAngle.y, ObjectAngleZ(iObj) - pBulletHole->vecOwnerAngle.z };
	for (int a = 0; a < 3; a++)
	{
		float fDiff = fmod(fabs(fAngleDiff[a]), 360.0f);
		if (fDiff > 180.0f) fDiff = 360.0f - fDiff;
		if (fDiff > 2.0f) return false;
	}
	return true;
}

void bulletholes_update (void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	g_iBulletHoleFrame++;

	// holes on an entity that has hidden (a wreck swap hides it) or gone since the hit are removed; those on one that has
	// moved or turned follow it, or are removed from a character, all of them in this one update
	for (int b = (int)g_bulletholes.size() - 1; b >= 0; b--)
	{
		sBulletHole* pBulletHole = &g_bulletholes[b];
		if (pBulletHole->iOwnerEntity == 0) continue;
		if (pBulletHole->bFollowsOwner == true && bulletholes_ownerpresent(pBulletHole) == true)
		{
			sObject* pOwnerObject = GetObjectData(pBulletHole->iOwnerObject);
			if (!pOwnerObject) continue;
			bool bStill = bulletholes_samematrix(&pOwnerObject->position.matWorld, &pBulletHole->matOwnerWorld);
			if (bStill == true && (pBulletHole->iMover < 0 || g_iBulletHoleFrame - g_bulletholemovers[pBulletHole->iMover].iLastMovedFrame < BULLETHOLESMOVERHOLD)) continue;
			if (bStill == false)
			{
				pBulletHole->matOwnerWorld = pOwnerObject->position.matWorld;
				if (pBulletHole->iMover < 0 && bulletholes_joinmover(pBulletHole) == true)
				{
					bulletholes_changesinglehole(pBulletHole->iVertIndexStart, 0, 0, 0);
					g_bBulletHolesDirty = true;
				}
				if (pBulletHole->iMover >= 0)
				{
					// its mover carries it: only its place in the world for the checks in bulletholes_add
					GGVECTOR3 vecCentre = (pBulletHole->vecLocalQuad[0] + pBulletHole->vecLocalQuad[3]) * 0.5f;
					GGVec3TransformCoord(&pBulletHole->vecWorldPos, &vecCentre, &pBulletHole->matOwnerWorld);
					g_bulletholemovers[pBulletHole->iMover].iLastMovedFrame = g_iBulletHoleFrame;
					g_bulletholemovers[pBulletHole->iMover].bMovedThisFrame = true;
					continue;
				}
			}
			else
			{
				// still long enough: back to the main mesh
				bulletholes_leavemover(pBulletHole);
			}
			GGVECTOR3 vecQuad[4];
			for (int p = 0; p < 4; p++) GGVec3TransformCoord(&vecQuad[p], &pBulletHole->vecLocalQuad[p], &pBulletHole->matOwnerWorld);
			GGVECTOR3 vecNormal;
			GGVec3TransformNormal(&vecNormal, &pBulletHole->vecLocalNormal, &pBulletHole->matOwnerWorld);
			GGVec3Normalize(&vecNormal, &vecNormal);
			pBulletHole->vecWorldPos = (vecQuad[0] + vecQuad[3]) * 0.5f;
			bulletholes_placesinglehole(g.bulletholesobject, pBulletHole->iVertIndexStart, vecQuad, vecNormal, pBulletHole->vecQuadUV);
			g_bBulletHolesDirty = true;
			continue;
		}
		if (pBulletHole->bFollowsOwner == false && bulletholes_ownerunchanged(pBulletHole) == true) continue;
		bulletholes_removeat(b);
	}

	// holes no longer expire: one stays until its slot is needed (bulletholes_add) or its entity hides, goes or moves

	// past the first 50 recycles, their totals at most every 2 seconds
	static int iRecyclesLogged = 0;
	static DWORD dwRecyclesLoggedAt = 0;
	if (g_iBulletHoleRecycles < iRecyclesLogged) iRecyclesLogged = 0;
	if (g_iBulletHoleRecycles > 50 && g_iBulletHoleRecycles != iRecyclesLogged && GetTickCount() - dwRecyclesLoggedAt > 2000)
	{
		char pLog[256];
		sprintf_s(pLog, "BULLETHOLE RECYCLE totals: recycled %d: unseen %d, all in view %d, owner cap %d; %d holes",
			g_iBulletHoleRecycles, g_iBulletHoleRecyclesBy[0], g_iBulletHoleRecyclesBy[1], g_iBulletHoleRecyclesBy[2], (int)g_bulletholes.size());
		timestampactivity(0, pLog);
		iRecyclesLogged = g_iBulletHoleRecycles;
		dwRecyclesLoggedAt = GetTickCount();
	}

	// show this frame's new, expired and removed holes in one refresh
	bulletholes_refreshobject();

	// a mover whose owner moved takes its world matrix, and that is all the renderer needs
	for (int m = 0; m < BULLETHOLESMOVERS; m++)
	{
		sBulletHoleMover* pMover = &g_bulletholemovers[m];
		if (pMover->bMovedThisFrame == false) continue;
		pMover->bMovedThisFrame = false;
		sObject* pObject = GetObjectData(BULLETHOLESMOVEROBJECT + m);
		sObject* pOwnerObject = GetObjectData(pMover->iOwnerObject);
		if (!pObject || !pOwnerObject) continue;
		pObject->position.matWorld = pOwnerObject->position.matWorld;
		WickedCall_UpdateObject(pObject);
	}
}

void bulletholes_free (void)
{
	// blank out all verts in the object
	bulletholes_clearall();

	// hide bullethole master object
	if (ObjectExist(g.bulletholesobject) == 1)
	{
		// hide the actual object
		HideObject(g.bulletholesobject);
	}
	for (int m = 0; m < BULLETHOLESMOVERS; m++)
		if (ObjectExist(BULLETHOLESMOVEROBJECT + m) == 1)
			HideObject(BULLETHOLESMOVEROBJECT + m);
}
