//----------------------------------------------------
//--- GAMEGURU - M-Bulletholes
//----------------------------------------------------

// includes
#include "stdafx.h"
#include "gameguru.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

// globals
#define BULLETHOLESMAX 1000
struct sBulletHole
{
	int iVertIndexStart;
	float fLifeCounter;
	float fRadius;
	GGVECTOR3 vecWorldPos;

	// the entity the hole was made on (0 for terrain), its object, and where that object stood when hit. The hole is removed
	// when the entity hides or goes. On anything but a character it follows the object as it moves: it keeps its quad and
	// normal in the object's own space and is placed again, in the one world-space mesh, whenever the object's world matrix
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

// functions

void bulletholes_refreshobject (void)
{
	if (ObjectExist(g.bulletholesobject) == 0) return;
	sObject* pObject = GetObjectData(g.bulletholesobject);
	if (!pObject) return;
	if (g_bBulletHolesFullRebuild == true || pObject->iMeshCount < 1 || !pObject->ppMeshList || !pObject->ppMeshList[0])
	{
		WickedCall_RemoveObject(pObject);
		WickedCall_AddObject(pObject);
		WickedCall_TextureObject(pObject, NULL);
		WickedCall_SetObjectCastShadows(pObject, false);
		WickedCall_SetObjectTransparent(pObject);
		WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_CURSOROBJECT);
		g_bBulletHolesFullRebuild = false;
	}
	else
	{
		// positions, normals and UVs; recreating the render data also refits the mesh bounds to the holes
		WickedCall_UpdateMeshVertexData(pObject->ppMeshList[0], true);
	}
	g_bBulletHolesDirty = false;
}

void bulletholes_init (void)
{
	if (ObjectExist(g.bulletholesobject) == 0)
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
		int iPolygonCount = BULLETHOLESMAX*2;
		int vertsize = 12+12+8;
		int iVertexCount = iPolygonCount * 6;
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
		MakeObject(g.bulletholesobject, iWorkMeshID, 0);
		if (GetMeshExist(iWorkMeshID) == 1) DeleteMesh(iWorkMeshID);
		if (MemblockExist(iMemblockIndex) == 1) DeleteMemblock(iMemblockIndex);

		// set bulletholes object for rendering
		LoadImage("gamecore\\bulletholes\\bulletholes_color.dds", g.bulletholeimage);
		TextureObject(g.bulletholesobject, g.bulletholeimage);
		SetObjectTransparency(g.bulletholesobject, 1);
		SetObjectCull(g.bulletholesobject, 0);
	}

	//  clear all bulletholes
	bulletholes_clearall();

	// the first change in this level rebuilds the renderer's object
	g_bBulletHolesFullRebuild = true;
}

void bulletholes_clearall (void)
{
	// wipe out data in object
	if (ObjectExist(g.bulletholesobject) == 1)
	{
		int iPolygonCount = BULLETHOLESMAX * 2;
		int iVertexCount = iPolygonCount * 6;
		LockVertexDataForLimbCore(g.bulletholesobject, 0, 1);
		for (int iVertIndex = 0; iVertIndex < iVertexCount; iVertIndex++)
		{
			SetVertexDataPosition(iVertIndex, 0, 0, 0);
		}
		UnlockVertexData();
	}

	// clear the list
	g_bulletholes.clear();
	memset(g_bulletholeavailable, 0, sizeof(g_bulletholeavailable));
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

	// instant reject if near an existing bullethole (avoid zclash)
	GGVECTOR3 vecPointInWorldSpace = GGVECTOR3(fX, fY, fZ);
	for (int n = 0; n < g_bulletholes.size(); n++)
	{
		float fDX = g_bulletholes[n].vecWorldPos.x - vecPointInWorldSpace.x;
		float fDY = g_bulletholes[n].vecWorldPos.y - vecPointInWorldSpace.y;
		float fDZ = g_bulletholes[n].vecWorldPos.z - vecPointInWorldSpace.z;
		float fDist = sqrt(fabs(fDX*fDX) + fabs(fDY*fDY) + fabs(fDZ*fDZ));
		if (fDist < g_bulletholes[n].fRadius+fBulletHoleRadius)
		{
			// bullethole too near an existing one
			return;
		}
	}

	// find free vertindex to place bullethole
	int iBulletSlot;
	for (iBulletSlot = 0; iBulletSlot < BULLETHOLESMAX; iBulletSlot++)
		if (g_bulletholeavailable[iBulletSlot] == false)
			break;

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
		bullethole.fLifeCounter = 2000.0f;
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

		// add vert data to represent this bullet hole, seen after the next bulletholes_update
		bulletholes_changesinglehole (bullethole.iVertIndexStart, fNX, fNY, fNZ);
		g_bBulletHolesDirty = true;
	}
}

// moves a hole's quad (the six vertices of its two triangles) to new corners and a new normal, keeping its texture
// coordinates
static void bulletholes_placesinglehole (int iVertIndex, GGVECTOR3* pQuad, GGVECTOR3 vecNormal)
{
	const int iWhichCornerOfQuad[6] = { 0, 1, 2, 1, 3, 2 };
	LockVertexDataForLimbCore(g.bulletholesobject, 0, 1);
	for (int v = 0; v < 6; v++)
	{
		GGVECTOR3* pCorner = &pQuad[iWhichCornerOfQuad[v]];
		SetVertexDataPosition(iVertIndex + v, pCorner->x, pCorner->y, pCorner->z);
		SetVertexDataNormals(iVertIndex + v, vecNormal.x, vecNormal.y, vecNormal.z);
	}
	UnlockVertexData();
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
	bool bUpdateTheObject = false;

	// holes on an entity that has hidden (a wreck swap hides it) or gone since the hit are removed; those on one that has
	// moved or turned follow it, or are removed from a character, all of them in this one update
	for (int b = (int)g_bulletholes.size() - 1; b >= 0; b--)
	{
		sBulletHole* pBulletHole = &g_bulletholes[b];
		if (pBulletHole->iOwnerEntity == 0) continue;
		if (pBulletHole->bFollowsOwner == true && bulletholes_ownerpresent(pBulletHole) == true)
		{
			sObject* pOwnerObject = GetObjectData(pBulletHole->iOwnerObject);
			if (!pOwnerObject || bulletholes_samematrix(&pOwnerObject->position.matWorld, &pBulletHole->matOwnerWorld) == true) continue;
			pBulletHole->matOwnerWorld = pOwnerObject->position.matWorld;
			GGVECTOR3 vecQuad[4];
			for (int p = 0; p < 4; p++) GGVec3TransformCoord(&vecQuad[p], &pBulletHole->vecLocalQuad[p], &pBulletHole->matOwnerWorld);
			GGVECTOR3 vecNormal;
			GGVec3TransformNormal(&vecNormal, &pBulletHole->vecLocalNormal, &pBulletHole->matOwnerWorld);
			GGVec3Normalize(&vecNormal, &vecNormal);
			pBulletHole->vecWorldPos = (vecQuad[0] + vecQuad[3]) * 0.5f;
			bulletholes_placesinglehole(pBulletHole->iVertIndexStart, vecQuad, vecNormal);
			bUpdateTheObject = true;
			continue;
		}
		if (pBulletHole->bFollowsOwner == false && bulletholes_ownerunchanged(pBulletHole) == true) continue;
		bulletholes_changesinglehole (pBulletHole->iVertIndexStart, 0, 0, 0);
		g_bulletholeavailable[pBulletHole->iVertIndexStart / 6] = false;
		g_bulletholes.erase(g_bulletholes.begin() + b);
		bUpdateTheObject = true;
	}

	// go through all bulletholes in list and remove those that have expired
	for (int b = 0; b < g_bulletholes.size(); b++)
	{
		sBulletHole* pBulletHole = &g_bulletholes[b];
		pBulletHole->fLifeCounter -= g.timeelapsed_f;
		if ( pBulletHole->fLifeCounter < 0.0f ) //LB: could also condition this based on camera distance/view so user NEVER sees the hole disappear
		{
			// remove bullethole from object
			bulletholes_changesinglehole (pBulletHole->iVertIndexStart, 0, 0, 0);

			// remove bullet hoel from list
			int iBulletSlot = pBulletHole->iVertIndexStart / 6;
			g_bulletholeavailable[iBulletSlot] = false;
			g_bulletholes.erase(g_bulletholes.begin() + b);

			// see the change
			bUpdateTheObject = true;

			// break from loop, only delete one from list per cycle
			break;
		}
	}
	if (bUpdateTheObject == true) g_bBulletHolesDirty = true;

	// show this frame's new, expired and removed holes in one refresh
	if (g_bBulletHolesDirty == true) bulletholes_refreshobject();
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
}
