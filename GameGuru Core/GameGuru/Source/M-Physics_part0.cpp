//----------------------------------------------------
//--- GAMEGURU - M-Physics
//----------------------------------------------------

#include "stdafx.h"
#include "gameguru.h"
#include "GGPhysicsQuery.h"

#include "..\..\GameGuru\Imgui\imgui.h"
#include "..\..\GameGuru\Imgui\imgui_impl_win32.h"
#include "..\..\GameGuru\Imgui\imgui_gg_dx11.h"

#include "GGTerrain\GGTerrain.h"
#include "GGTerrain\GGTrees.h"
using namespace GGTerrain;
using namespace GGTrees;
bool g_bSpecialPhysicsDebuggingMode = false;
GGVECTOR3 g_vPlayAreaMin = GGVECTOR3(0, 0, 0);
GGVECTOR3 g_vPlayAreaMax = GGVECTOR3(0, 0, 0);
bool g_bModifiedThisTerrainGrid[21][21];
int g_iLastProgressPercentage = -1;
bool g_bMapMatIDToMatIndexAvailable = false;
int g_iMapMatIDToMatIndex[32];
int g_iSuccessfullyBlockedAtTime = 0;
bool g_bSpawningThisOneNow = false;

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

// 
//  Physics Subroutines and Functions
// 

// 
//  PHYSICS CODE
// 

void physics_inittweakables ( void )
{
	//  Editable in Player Start Marker
	t.playercontrol.regenrate=0;
	t.playercontrol.regenspeed=100;
	t.playercontrol.regendelay=3000;
	t.playercontrol.regentime=0;
	t.playercontrol.jumpmax_f=215.0;
	t.playercontrol.gravity_f=900.0;
	t.playercontrol.fallspeed_f=5000.0;
	t.playercontrol.climbangle_f = 66.0f; //LB: messed with ability to climbs stairs, and small pallets on floor... 50.0f; //LB: was 70.0;
	t.playercontrol.footfallpace_f=3.0;
	t.playercontrol.wobblespeed_f=460.0;
	t.playercontrol.wobbleheight_f=1.5;
	t.playercontrol.accel_f=25.0;

	//  third person defaults
	t.playercontrol.thirdperson.enabled=0;
	t.playercontrol.thirdperson.charactere=0;
	t.playercontrol.thirdperson.startmarkere=0;
	t.playercontrol.thirdperson.cameralocked=0;
	t.playercontrol.thirdperson.cameradistance=200.0;
	t.playercontrol.thirdperson.cameraheight=100.0;
	t.playercontrol.thirdperson.camerafocus=5.0;
	t.playercontrol.thirdperson.cameraspeed=50.0;
	t.playercontrol.thirdperson.camerashoulder=6.0;
	t.playercontrol.thirdperson.camerafollow=1;
	t.playercontrol.thirdperson.camerareticle=1;
}

void physics_loadmaterialsoundsintomapmat ( LPSTR pOptionalMaterialSoundsFile )
{
	if (FileExist(pOptionalMaterialSoundsFile) == 1)
	{
		OpenToRead(1, pOptionalMaterialSoundsFile);
		while (FileEnd(1) == 0)
		{
			LPSTR pLine = ReadString(1);
			if (pLine != NULL)
			{
				if (pLine[0] != ';' && strlen(pLine) < 30)
				{
					char pNums[MAX_PATH];
					memset(pNums, 0, sizeof(pNums));
					if (strlen(pLine) > 3)
					{
						strcpy_s(pNums, MAX_PATH, pLine + 3); // skip 'mat'
					}
					LPSTR pEqual = strstr(pNums, "=");
					if (pEqual)
					{
						char pMaterialIndex[32];
						strcpy(pMaterialIndex, pEqual + 1);
						*pEqual = 0;
						char pTerrainMatID[32];
						strcpy(pTerrainMatID, pNums);
						int iMaterialIndex = atoi(pMaterialIndex);
						int iMatID = atoi(pTerrainMatID);
						if (iMatID >= 1 && iMatID <= 32)
						{
							g_iMapMatIDToMatIndex[iMatID - 1] = iMaterialIndex;
						}
					}
				}
			}
		}
		CloseFile(1);
	}
}

void physics_copymatmaptocustommat (void)
{
	extern int g_iCustomTerrainMatSounds[32];
	for (int i = 0; i < 32; i++)
	{
		g_iCustomTerrainMatSounds[i] = g_iMapMatIDToMatIndex[i];
	}
}

void physics_init ( void )
{
	// create material ID to material sound mapping
	if (g_bMapMatIDToMatIndexAvailable == false)
	{
		g_bMapMatIDToMatIndexAvailable = true;
		if (t.visuals.customTexturesFolder.Len() > 0)
		{
			// Custom terrain materials are determined by user, not matsounds.txt
			extern int g_iCustomTerrainMatSounds[32];
			for (int i = 0; i < 32; i++)
			{
				g_iMapMatIDToMatIndex[i] = g_iCustomTerrainMatSounds[i];
			}
		}
		else
		{
			LPSTR pMatConvertTableFile = "terraintextures\\matsounds.txt";
			physics_loadmaterialsoundsintomapmat (pMatConvertTableFile);
		}
	}

	//  Player Control
	t.playercontrol.wobble_f=0.0;
	t.playercontrol.floory_f=0.0;
	t.playercontrol.topspeed_f=0.75;
	t.playercontrol.footfalltype=0;
	t.playercontrol.footfallcount=0;
	if (  t.playercontrol.regenrate>0 ) 
	{
		t.playercontrol.regentime= MAXTimer();
		t.playercontrol.regentick= MAXTimer();
	}
	else
	{
		t.playercontrol.regentime=0;
		t.playercontrol.regentick=0;
	}

	//  Reset jetpack
	t.playercontrol.jetpackmode=0;
	t.playercontrol.jetpackthrust_f=0.0;
	t.playercontrol.jetpackfuel_f=0;

	//  Player misc settings
	t.playercontrol.disablemusicreset=0;

	//  Reset gun collected count
	t.guncollectedcount=0;

	//  Init physics system
	ODEStart (   ); g.gphysicssessionactive=1;

	//  Set starting water Line (  )
	terrain_updatewaterphysics ( );

	//  Create terrain collision
	t.tgenerateterraindirtyregiononly=0;
	timestampactivity(0,"_physics_createterraincollision");
	physics_createterraincollision ( );
	t.tgenerateterraindirtyregiononly=0;

	// MAX also has virtual trees that need physics
	physics_createvirtualtreecylinders();

	//  Player Controller Object
	if (  ObjectExist(t.aisystem.objectstartindex) == 0 ) 
	{
		//  normally created by AI which precedes Physics initialisations
		MakeObjectCube (  t.aisystem.objectstartindex,10 );
	}
	HideObject (  t.aisystem.objectstartindex );

	//  set default player gravity
	t.playercontrol.gravityactive=1;
	t.playercontrol.gravityactivepress=0;
	t.playercontrol.lockatheight=0;
	t.playercontrol.lockatheightpress=0;

	//  Setup entity physics
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		physics_prepareentityforphysics ( );
	}

	//  Introduce all characters and entities to the physics universe
	timestampactivity(0,"Introduce all characters");
	for ( g.charanimindex = 1 ; g.charanimindex <= g.charanimindexmax; g.charanimindex++ )
	{
		// get physics object for this character
		t.tphyobj=t.charanimstates[g.charanimindex].obj;

		// 111115 - but exclude any third person character
		if ( t.playercontrol.thirdperson.enabled == 1 && t.playercontrol.thirdperson.characterindex == g.charanimindex ) 
			t.tphyobj = 0;

		// if object still requires Y adjustment check
		if ( t.tphyobj>0 ) 
		{
			t.e = t.charanimstates[g.charanimindex].e;
			bool bCharacterUsesPhysics = false;
			t.entityelement[t.e].usingphysicsnow = 0;
			if (t.entityprofile[t.entityelement[t.e].bankindex].physics != 0) bCharacterUsesPhysics = true;
			if (bCharacterUsesPhysics == true)
			{
				// get entity index associated with character
				t.tcollisionscaling = t.entityprofile[t.entityelement[t.e].bankindex].collisionscaling;
				t.tcollisionscalingxz = t.entityprofile[t.entityelement[t.e].bankindex].collisionscalingxz;
				physics_setupcharacter ();
			}
		}
	}

	// Ensure the LUA mouse is always reset
	lua_deactivatemouse();

	// player physics setup closer to main loop
	t.freezeplayerposonly = 0;
	physics_setupplayer ( );
}

void physics_finalize ( void )
{
	ODEFinalizeWorld();
}

// one way flood fill to reduce terrain polys
struct sLargeQuadListItem
{
	int iX1;
	int iZ1;
	int iX2;
	int iZ2;
};
//int g_iQuadGrid[201][201];
int g_iQuadGrid[401][401];
std::vector<sLargeQuadListItem> g_LargeQuadList;
std::vector<sLargeQuadListItem> g_RefinedLargeQuadList;
void physics_processheightsusingfloodfill (int iX, int iZ, int iGridAtX, int iGridAtZ, float fOneTileSize, int iFullResolutionSize, int iResolutionSIze, int iHeightDataSize, float* pfHeightData)
{
	// stage 0 - we mark this as used right now so no overlapping behaviors occur
	int iUniqueTileID = 1 + g_LargeQuadList.size();
	g_iQuadGrid[iX][iZ] = iUniqueTileID;

	// the right/bottom edge needs full resolution (to join seamlessly with next area)
	int iX1 = iX;
	int iZ1 = iZ;
	int iX2 = iX;
	int iZ2 = iZ;
	if (1)//iX < iResolutionSIze && iZ < iResolutionSIze)
	{
		// stage 1 - create plane from this quad (iX,iZ)
		GGPLANE thisPlane;
		int iRealX = (iGridAtX*iResolutionSIze);
		int iRealZ = (iGridAtZ*iResolutionSIze);
		int iGetHeightX = (iRealX + iX);
		int iGetHeightZ = (iRealZ + iZ);
		float fHeight = 0.0f;
		int iOffset = (iGetHeightZ*iFullResolutionSize) + iGetHeightX;
		if (iOffset < iHeightDataSize) fHeight = pfHeightData[iOffset];
		GGVECTOR3 vecPos0 = GGVECTOR3(iX * fOneTileSize, fHeight, iZ * fOneTileSize);
		iGetHeightX = (iRealX + iX + 1);
		iGetHeightZ = (iRealZ + iZ);
		iOffset = (iGetHeightZ*iFullResolutionSize) + iGetHeightX;
		if (iOffset < iHeightDataSize) fHeight = pfHeightData[iOffset];
		GGVECTOR3 vecPos1 = GGVECTOR3((iX + 1) * fOneTileSize, fHeight, iZ * fOneTileSize);
		iGetHeightX = (iRealX + iX);
		iGetHeightZ = (iRealZ + iZ + 1);
		iOffset = (iGetHeightZ*iFullResolutionSize) + iGetHeightX;
		if (iOffset < iHeightDataSize) fHeight = pfHeightData[iOffset];
		GGVECTOR3 vecPos2 = GGVECTOR3(iX * fOneTileSize, fHeight, (iZ + 1) * fOneTileSize);
		GGPlaneFromPoints(&thisPlane, &vecPos0, &vecPos1, &vecPos2);

		// stage 2 - expand quad in both axis while within threshold
		bool bExpanding = true;
		float fThreshold = 2.0f;
		int iTryX = iX;
		int iTryZ = iZ;
		bool bCanExpandX = true;
		bool bCanExpandZ = true;
		while (bExpanding == true)
		{
			// expand in directions where g_iQuadGrid is free
			if (bCanExpandX == true)
			{
				iTryX++;
				for (int iAllTheZs = iZ1; iAllTheZs <= iZ2; iAllTheZs++)
				{
					if (g_iQuadGrid[iTryX][iAllTheZs] == 0)
					{
						iGetHeightX = (iRealX + iTryX);
						iGetHeightZ = (iRealZ + iAllTheZs);
						fHeight = 0.0f;
						iOffset = (iGetHeightZ*iFullResolutionSize) + iGetHeightX;
						if (iOffset < iHeightDataSize) fHeight = pfHeightData[iOffset];
						GGVECTOR3 vecTryPos = GGVECTOR3(iTryX * fOneTileSize, fHeight, iAllTheZs * fOneTileSize);
						float fDeviationFromPlane = GGPlaneDotCoord(&thisPlane, &vecTryPos);
						if (fabs(fDeviationFromPlane) > fThreshold)
						{
							// this position cannot be added to the current plane/largequad
							bCanExpandX = false;
						}
					}
					else
					{
						// hit a used tile, also cannot expand in this direction any more
						bCanExpandX = false;
					}
				}
				if (bCanExpandX == true)
				{
					// this is fine, keep going in the X direction
					iX2 = iTryX;
					for (int z = iZ1; z <= iZ2; z++)
						for (int x = iX1; x <= iX2; x++)
							g_iQuadGrid[x][z] = iUniqueTileID;
				}
				if (iTryX == iResolutionSIze) bCanExpandX = false;
			}
			if (bCanExpandZ == true)
			{
				iTryZ++;
				for (int iAllTheXs = iX1; iAllTheXs <= iX2; iAllTheXs++)
				{
					if (g_iQuadGrid[iAllTheXs][iTryZ] == 0)
					{
						iGetHeightX = (iRealX + iAllTheXs);
						iGetHeightZ = (iRealZ + iTryZ);
						fHeight = 0.0f;
						iOffset = (iGetHeightZ*iFullResolutionSize) + iGetHeightX;
						if (iOffset < iHeightDataSize) fHeight = pfHeightData[iOffset];
						GGVECTOR3 vecTryPos = GGVECTOR3(iAllTheXs * fOneTileSize, fHeight, iTryZ * fOneTileSize);
						float fDeviationFromPlane = GGPlaneDotCoord(&thisPlane, &vecTryPos);
						if (fabs(fDeviationFromPlane) > fThreshold)
						{
							// this position cannot be added to the current plane/largequad
							bCanExpandZ = false;
						}
					}
					else
					{
						// hit a used tile, also cannot expand in this direction any more
						bCanExpandZ = false;
					}
				}
				if (bCanExpandZ == true)
				{
					// this is fine, keep going in the Z direction
					iZ2 = iTryZ;
					for (int z = iZ1; z <= iZ2; z++)
						for (int x = iX1; x <= iX2; x++)
							g_iQuadGrid[x][z] = iUniqueTileID;
				}
				if (iTryZ == iResolutionSIze) bCanExpandZ = false;
			}

			// if threshold exceeded in both directions, cannot expand any more
			if (bCanExpandX == false && bCanExpandZ == false)
				bExpanding = false;
		}
	}

	// stage 3 - create a 'larger' quad for this expanded area
	sLargeQuadListItem item;
	item.iX1 = iX1;
	item.iZ1 = iZ1;
	item.iX2 = iX2;
	item.iZ2 = iZ2;
	g_LargeQuadList.push_back(item);
}

void physics_createterraincollision ( void )
{
	// we use the 'play area' to reduce 'test level' load time (concentrating only on the area you are using in the level)
	timestampactivity(0, "detect and define play area");
	// calculate bounds of static features
	static float fLastPlayerStartX = -1.0f;
	static float fLastPlayerStartZ = -1.0f;
	float fPlayerStartX = CameraPositionX(0);
	float fPlayerStartZ = CameraPositionZ(0);
	GGVECTOR3 vecMin, vecMax;
	vecMin = GGVECTOR3(fPlayerStartX, 0, fPlayerStartZ);
	vecMax = GGVECTOR3(fPlayerStartX, 0, fPlayerStartZ);
	vecMin -= GGVECTOR3(-500, 0, -500);
	vecMax += GGVECTOR3(500, 0, 500);
	int iObjectCount = 0;
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		if ( 1 ) // must include start marker and all dynamic objects (everything) for full coverage of level
		{
			int iObj = t.entityelement[e].obj;
			if (iObj > 0)
			{
				if (ObjectExist(iObj) == 1)
				{
					if (vecMin.x > ObjectPositionX(iObj)) vecMin.x = ObjectPositionX(iObj);
					if (vecMin.y > ObjectPositionY(iObj)) vecMin.y = ObjectPositionY(iObj);
					if (vecMin.z > ObjectPositionZ(iObj)) vecMin.z = ObjectPositionZ(iObj);
					if (vecMax.x < ObjectPositionX(iObj)) vecMax.x = ObjectPositionX(iObj);
					if (vecMax.y < ObjectPositionY(iObj)) vecMax.y = ObjectPositionY(iObj);
					if (vecMax.z < ObjectPositionZ(iObj)) vecMax.z = ObjectPositionZ(iObj);
					iObjectCount++;
				}
			}
		}
		int entid = t.entityelement[e].bankindex;
		if (entid > 0)
		{
			if (t.entityprofile[entid].ismarker == 1)
			{
				// player start marker
				fPlayerStartX = t.entityelement[e].x;
				fPlayerStartZ = t.entityelement[e].z;
			}
		}
	}
	float fEditableSizeHalved = GGTerrain_GetEditableSize();
	t.terraineditableareasizeminx = -fEditableSizeHalved;
	t.terraineditableareasizeminz = -fEditableSizeHalved;
	t.terraineditableareasizemaxx = fEditableSizeHalved;
	t.terraineditableareasizemaxz = fEditableSizeHalved;
	bool bAnythingHasMoved = false;
	if (fLastPlayerStartX != fPlayerStartX || fLastPlayerStartZ != fPlayerStartZ ) bAnythingHasMoved = true;
	fLastPlayerStartX = fPlayerStartX;
	fLastPlayerStartZ = fPlayerStartZ;
	g_vPlayAreaMin = vecMin - GGVECTOR3(1000, 0, 1000);
	g_vPlayAreaMax = vecMax + GGVECTOR3(1000, 0, 1000);
	if (g_vPlayAreaMin.x < -50000) g_vPlayAreaMin.x = -50000;
	if (g_vPlayAreaMin.x > 50000)  g_vPlayAreaMin.x = 50000;
	if (g_vPlayAreaMin.z < -50000) g_vPlayAreaMin.z = -50000;
	if (g_vPlayAreaMin.z > 50000)  g_vPlayAreaMin.z = 50000;
	if (g_vPlayAreaMax.x < -50000) g_vPlayAreaMax.x = -50000;
	if (g_vPlayAreaMax.x > 50000)  g_vPlayAreaMax.x = 50000;
	if (g_vPlayAreaMax.z < -50000) g_vPlayAreaMax.z = -50000;
	if (g_vPlayAreaMax.z > 50000)  g_vPlayAreaMax.z = 50000;

	// above values are used by navmesh
	if (t.visuals.bEnableEmptyLevelMode == false)
	{
		// only bother creating terrain physics if we are not in 'empty level mode'
		ODECreateGGTerrain();
	}

	// and we are done
	return;
}

struct sVTreeObj
{
	int iID;
	float fX;
	float fY;
	float fZ;
	bool bActive;
	int iType = -1;
};
std::vector<sVTreeObj> g_VTreeObj;

// the tree type of a collision cylinder object from physics_managevirtualtreecylinders, -1 if it isn't one
int physics_virtualtreetype(int iObject)
{
	int vti = iObject - g.virtualtreeobjectstart;
	if (vti < 0 || vti >= (int)g_VTreeObj.size() || !g_VTreeObj[vti].bActive) return -1;
	return g_VTreeObj[vti].iType;
}

void physics_createvirtualtreecylinders (void)
{
	// going to use GGTrees_GetClosest to position kenetic cylinders so player cannot walk through virtual trees
	if (ObjectExist(g.virtualtreeobjectstart) == 0)
	{
		// create once for rest of software lifecycle (Wicked slows when create/delete objects)
		for (int iVT = 0; iVT < PHYSICS_VIRTUALTREE_MAX; iVT++)
		{
			// create cylinder for virtual tree
			int iPhyObjID = g.virtualtreeobjectstart + iVT;
			MakeObjectSphere(iPhyObjID, 9);
			HideObject(iPhyObjID);
			sObject* pObject = GetObjectData(iPhyObjID);
			if (pObject)
			{
				WickedCall_SetObjectCastShadows(pObject, false);
			}
		}
	}

	// start new Vtrees list
	g_VTreeObj.clear();
}

void physics_freevirtualtreecylinders (void)
{
	// just hide, we will need them next time
	for (int iVT = 0; iVT < PHYSICS_VIRTUALTREE_MAX; iVT++)
	{
		// create cylinder for virtual tree
		int iPhyObjID = g.virtualtreeobjectstart + iVT;
		if (ObjectExist(iPhyObjID) == 1)
		{
			ODEDestroyObject(iPhyObjID);
			HideObject(iPhyObjID);
		}
	}
}

void physics_managevirtualtreecylinders (void)
{
	// first start assuming all existing visible trees may not show again
	for (int vti = 0; vti < g_VTreeObj.size(); vti++)
		g_VTreeObj[vti].bActive = false;

	// scan for virtual trees
	if (ggtrees_global_params.draw_enabled == 1)
	{
		float fRadiusOfScan = 200.0f;
		GGTrees::GGTreePoint* pOutPoints = NULL;
		int iTreeCount = GGTrees::GGTrees_GetClosest (CameraPositionX(0), CameraPositionZ(0), fRadiusOfScan, &pOutPoints);
		if (pOutPoints)
		{
			for (int n = 0; n < iTreeCount; n++)
			{
				// this virtual tree pos
				GGVECTOR3 vecTreePos = GGVECTOR3(pOutPoints[n].x, pOutPoints[n].y, pOutPoints[n].z);
				float fTreeThickness = pOutPoints[n].scale;

				// see if tree in the visible list
				int vti = 0;
				for (; vti < g_VTreeObj.size(); vti++)
				{
					if (g_VTreeObj[vti].iID > 0 && g_VTreeObj[vti].fX == vecTreePos.x && g_VTreeObj[vti].fY == vecTreePos.y && g_VTreeObj[vti].fZ == vecTreePos.z)
						break;
				}
				if (vti < g_VTreeObj.size())
				{
					// yes we still know about this tree
					g_VTreeObj[vti].bActive = true;
				}
				else
				{
					// ah, new tree, add to list
					// first see if we have a free slot in exsiting list
					int vti = 0;
					for (; vti < g_VTreeObj.size(); vti++)
					{
						if (g_VTreeObj[vti].bActive == false)
							break;
					}
					if (vti < g_VTreeObj.size())
					{
						// use existing entry in list
					}
					else
					{
						// new list entry
						if (g_VTreeObj.size() < PHYSICS_VIRTUALTREE_MAX)
						{
							sVTreeObj newtree;
							g_VTreeObj.push_back(newtree);
							vti = g_VTreeObj.size() - 1;
						}
						else
						{
							// no room to add to visible list
							vti = -1;
						}
					}
					// entry details
					if (vti != -1)
					{
						int iPhyObjID = g.virtualtreeobjectstart + vti;
						g_VTreeObj[vti].iID = iPhyObjID;
						g_VTreeObj[vti].fX = vecTreePos.x;
						g_VTreeObj[vti].fY = vecTreePos.y;
						g_VTreeObj[vti].fZ = vecTreePos.z;
						g_VTreeObj[vti].bActive = true;
						g_VTreeObj[vti].iType = pOutPoints[n].type;

						// delete any old one
						ODEDestroyObject(iPhyObjID);

						// position dynamically
						PositionObject(iPhyObjID, vecTreePos.x, vecTreePos.y, vecTreePos.z);

						// ShowObject(iPhyObjID); //Debug only.
						HideObject(iPhyObjID);

						// now create our physics object afresh
						SetObjectArbitaryValue (iPhyObjID, 3);// 6); wood, not flesh!

						// must cater for largest thickest trees in the default biome set (scots pine dead)
						//ODECreateStaticCylinder (iPhyObjID, vecTreePos.x, vecTreePos.y, vecTreePos.z, 10, 500, 10, 0, 0, 0);
						ODECreateStaticCylinder (iPhyObjID, vecTreePos.x, vecTreePos.y, vecTreePos.z, fTreeThickness, 500, fTreeThickness, 0, 0, 0);
					}
				}
			}
			delete[] pOutPoints;
		}
		// finally remove any from list which have become inactive
		for (int vti = 0; vti < g_VTreeObj.size(); vti++)
		{
			if (g_VTreeObj[vti].bActive == false)
			{
				if (g_VTreeObj[vti].iID > 0)
				{
					int iPhyObjID = g.virtualtreeobjectstart + vti;
					if (ObjectExist(iPhyObjID) == 1)
					{
						ODEDestroyObject(iPhyObjID);
						HideObject(iPhyObjID);
					}
					g_VTreeObj[vti].iID = 0;
				}
			}
		}
	}
}

void physics_prepareentityforphysics ( void )
{
	//  takes E and ENTID
	t.tphyobj=t.entityelement[t.e].obj;
	if (  t.entid>0 && t.tphyobj>0 ) 
	{
		// Entity has a different collision mode to the parent object in the FPE file...
		int iStoreEntityIndex = t.entid;
		int iStoreOriginalCollisionMode = t.entityprofile[iStoreEntityIndex].collisionmode;
		if (t.e < t.entityelement.size() && t.entityelement[t.e].eleprof.iOverrideCollisionMode != -1)
		{
			t.entityprofile[t.entid].collisionmode = t.entityelement[t.e].eleprof.iOverrideCollisionMode;
		}

		// special hybrid collision mode can hide static limbs of primary object
		if (t.entityprofile[t.entid].collisionmode == 31)
		{
			// create a secondary object to show the static limbs (and hide the non static)
			if (t.entityelement[t.e].attachmentobj == 0)
			{
				int iSecondaryObjID = g.physicssecondariesoffset;
				while (ObjectExist(iSecondaryObjID) == 1 && iSecondaryObjID < g.physicssecondariesoffsetend) iSecondaryObjID++;
				if (ObjectExist(iSecondaryObjID) == 1) DeleteObject (iSecondaryObjID);
				CloneObject(iSecondaryObjID, t.tphyobj);
				PositionObject(iSecondaryObjID, ObjectPositionX(t.tphyobj), ObjectPositionY(t.tphyobj), ObjectPositionZ(t.tphyobj));
				SetObjectToObjectOrientation(iSecondaryObjID, t.tphyobj);
				sObject* pSecondaryObject = GetObjectData(iSecondaryObjID);
				WickedCall_TextureObject(pSecondaryObject, NULL);
				t.entityelement[t.e].attachmentobj = iSecondaryObjID;
				// ensure it does not attract a collision hit during ray cast
				WickedCall_SetObjectRenderLayer(pSecondaryObject, GGRENDERLAYERS_CURSOROBJECT);
				PerformCheckListForLimbs(iSecondaryObjID);
				for (int c = ChecklistQuantity(); c > 1; c += -1)
				{
					t.tname_s = Lower(ChecklistString(c));
					LPSTR pNamePtr = t.tname_s.Get();
					if (strlen(pNamePtr) > 7)
					{
						if (strnicmp(pNamePtr + strlen(pNamePtr) - 7, "_static", 7) != NULL)
						{
							HideLimb(iSecondaryObjID, c - 1);
						}
					}
				}
			}
			
			// hide the static limbs from the primary
			PerformCheckListForLimbs(t.tphyobj);
			for ( int c = ChecklistQuantity(); c >= 1; c += -1)
			{
				t.tname_s = Lower(ChecklistString(c));
				LPSTR pNamePtr = t.tname_s.Get();
				if (strlen(pNamePtr)>7)
				{
					if (strnicmp(pNamePtr+strlen(pNamePtr)-7,"_static",7)==NULL )
					{
						HideLimb(t.tphyobj, c-1);
					}
				}
			}
		}
		SpawnProbe_Sub("physics: limbs");

		// now use collisionindex
		t.tnophysics=0;
		if (  t.entityprofile[t.entid].ismarker != 0  )  t.tnophysics = 1;
		if (  t.entityprofile[t.entid].collisionmode == 11  )  t.tnophysics = 1;
		if (  t.entityprofile[t.entid].collisionmode == 12  )  t.tnophysics = 1;
		if (  t.entityelement[t.e].eleprof.physics == 0  )  t.tnophysics = 1;
		if (  t.entityelement[t.e].eleprof.physics == 2  )  t.tnophysics = 1;
		if (  t.entityprofile[t.entid].isammo == 1  )  t.tnophysics = 1;
		if (t.entityelement[t.e].eleprof.iOverrideCollisionMode == -1)
		{
			// special case where weapon drops can be assigned a physics shapoe when dropped
			if (Len(t.entityprofile[t.entid].isweapon_s.Get()) > 1)  t.tnophysics = 1;
		}
		if (  t.tnophysics == 1 ) 
		{
			//  no physics
		}
		else
		{
			if (  t.entityprofile[t.entid].isebe != 0 ) 
			{
				// EBE structure from cubes
				physics_setupebestructure ( );
				t.entityelement[t.e].usingphysicsnow=1;
			}
			else
			{
				if ( t.entityprofile[t.entid].ischaracter == 1 && t.entityelement[t.e].eleprof.isimmobile == 0 )
				{
					//  physics objects belong to Ghost AI Objects (set outside of this function, i.e. LUA-Entity.cpp)
					// except when you are spawning them in game and want physics sorted
					if (g_bSpawningThisOneNow == true)
					{
						// you shall go to the ball, cinders!
						t.tcollisionscaling = t.entityprofile[t.entid].collisionscaling;
						t.tcollisionscalingxz = t.entityprofile[t.entid].collisionscalingxz;
						physics_setupcharacter ();
					}
				}
				else
				{
					// static or dynamic
					t.tstatic=t.entityelement[t.e].staticflag;
					if (  t.entityelement[t.e].eleprof.isimmobile == 1  )  t.tstatic = 1;

					// physics object is faux-character capsule (zombies, custom characters)
					if ( t.entityprofile[t.entid].collisionmode == 21 || t.entityprofile[t.entid].collisionmode == 22 )
					{
						// create capsule (to be controlled as entity-driven, i.e. MoveForward)
						t.tcollisionscaling = t.entityprofile[t.entid].collisionscaling;
						t.tcollisionscalingxz = t.entityprofile[t.entid].collisionscalingxz;
						physics_setupcharacter ( );
					}
					else
					{
						// create solid entities
						if (t.entityprofile[t.entid].collisionmode == 1)
						{
							t.tshape = 2;
						}
						else if (t.entityprofile[t.entid].collisionmode == 8)
						{
							t.tshape = 8; // new version of '2' but detects for OBJ
						}
						else if (t.entityprofile[t.entid].collisionmode == 9)
						{
							t.tshape = 9;
						}
						else if (t.entityprofile[t.entid].collisionmode == 10)
						{
							t.tshape = 10;
						}
						else if (t.entityprofile[t.entid].collisionmode == 2)
						{
							t.tshape = 6;
						}
						else if (t.entityprofile[t.entid].collisionmode == 3)
						{
							t.tshape = 7;
						}
						else
						{
							if (  t.entityprofile[t.entid].collisionmode >= 1000 ) 
							{
								t.tshape=t.entityprofile[t.entid].collisionmode;
							}
							else
							{
								if (  t.entityprofile[t.entid].collisionmode >= 50 && t.entityprofile[t.entid].collisionmode<60 ) 
								{
									t.tshape=3;
									t.tstatic=1;
								}
								else
								{
									t.tshape=1;
								}
							}
						}
						//  check if it has a list of physics objects from the importer, collisionmode 40
						if (  t.entityprofile[t.entid].physicsobjectcount > 0 && t.entityprofile[t.entid].collisionmode  ==  40 ) 
						{
							t.tshape = 4;
						}
						t.tweight=t.entityelement[t.e].eleprof.phyweight;
						t.tfriction=t.entityelement[t.e].eleprof.phyfriction;
						t.tcollisionscaling=t.entityprofile[t.entid].collisionscaling;
						t.tcollisionscalingxz = t.entityprofile[t.entid].collisionscalingxz;
						physics_setupobject ( );
						// GG: the kilos SetEntityMass gave this entity, or else its .fpe physicsmass, on its new body
						extern bool ODEApplyEntityMassKg(int iEntity, int iObject);
						extern int ODESetObjectMassKg(int iObject, float fKg);
						float fProfileKg = t.entityprofile[t.entityelement[t.e].bankindex].physicsmass;
						if ( !ODEApplyEntityMassKg ( t.e, t.tphyobj ) && fProfileKg > 0 ) ODESetObjectMassKg ( t.tphyobj, fProfileKg );
						t.entityelement[t.e].usingphysicsnow=1;
					}
				}
			}
		}

		// ...and restore parent when done
		t.entityprofile[iStoreEntityIndex].collisionmode = iStoreOriginalCollisionMode;
	}
}

void physics_setupplayernoreset ( void )
{
	//  create character controller for player
	PositionObject (  t.aisystem.objectstartindex,t.terrain.playerx_f,t.terrain.playery_f,t.terrain.playerz_f );
	if ( t.freezeplayerposonly==0 ) RotateObject (  t.aisystem.objectstartindex,t.terrain.playerax_f,t.terrain.playeray_f,t.terrain.playeraz_f );
	SetObjectArbitaryValue (  t.aisystem.objectstartindex, 6 ); // 6-flesh

	// allow SetWorldGravity to decide players gravity value (and prevents slow drop when holding onto ladder and constantly calling this function)
	ODECreateDynamicCharacterController (  t.aisystem.objectstartindex,t.playercontrol.gravity_f,t.playercontrol.fallspeed_f,t.playercontrol.climbangle_f );
}

void physics_setupplayer ( void )
{
	physics_setupplayernoreset ( );
	if ( g.luacameraoverride != 2 && g.luacameraoverride != 3 )
	{
		if ( t.freezeplayerposonly==0 ) RotateCamera (  0,t.terrain.playerax_f,t.terrain.playeray_f,t.terrain.playeraz_f );
	}
}

void physics_disableplayer ( void )
{
	ODEDestroyObject (  t.aisystem.objectstartindex );
}

void physics_setupcharacter ( void )
{
	// only if physics is active for this character
	if (t.entityprofile[t.entityelement[t.e].bankindex].physics != 0)
	{
		// create physics for this character/faux-character object
		SetObjectArbitaryValue (t.tphyobj, 6); // 6-flesh
		if (t.entityelement[t.e].eleprof.isimmobile == 0)
		{
			// 190718 - remove t.terrain.adjaboveground_f from enemy terrain relative positioning
			// ensure CHARACTER do not spawn UNDER the terrain
			t.tgroundheight_f = BT_GetGroundHeight(t.terrain.TerrainID, ObjectPositionX(t.tphyobj), ObjectPositionZ(t.tphyobj));
			t.tgroundheight_f = t.tgroundheight_f + 2.5;
			// 291116 - account for object vecCenter (so characters with Y=0=Floor are not unjustly raised)
			// NOW DONE EARLIER SO NO NEED TO ADJUST FOR COL CENTER TWICE (see calling function)
			//float fAccountForVecCenter = GetObjectCollisionCenterY ( t.tphyobj );
			if (ObjectPositionY(t.tphyobj) <= t.tgroundheight_f)
			{
				PositionObject (t.tphyobj, ObjectPositionX(t.tphyobj), t.tgroundheight_f, ObjectPositionZ(t.tphyobj));
			}
			else
			{
				PositionObject (t.tphyobj, ObjectPositionX(t.tphyobj), ObjectPositionY(t.tphyobj), ObjectPositionZ(t.tphyobj));
			}
			t.tfinalscale_f = g.gcharactercapsulescale_f * ((t.entityprofile[t.entityelement[t.e].bankindex].scale + 0.0f) / 100.0f);
			float fWeight = t.entityelement[t.e].eleprof.phyweight;
			float fFriction = t.entityelement[t.e].eleprof.phyfriction;
			ODECreateDynamicCapsule (t.tphyobj, t.tfinalscale_f, 0.0, fWeight, fFriction, (float)t.tcollisionscaling / 100.0f, (float)t.tcollisionscalingxz / 100.0f);
		}
		else
		{
			// 290515 - fixes scifi DLC characters floating (ISIMMOBILE=1) PositionObject ( t.tphyobj,ObjectPositionX(t.tphyobj),ObjectPositionY(t.tphyobj)+(ObjectSizeY(t.tphyobj,1)/2),ObjectPositionZ(t.tphyobj) );
			PositionObject (t.tphyobj, ObjectPositionX(t.tphyobj), ObjectPositionY(t.tphyobj), ObjectPositionZ(t.tphyobj));
			ODECreateStaticCapsule (t.tphyobj, (float)t.tcollisionscaling / 100.0f, (float)t.tcollisionscalingxz / 100.0f);
		}
		t.entityelement[t.e].usingphysicsnow = 1;
	}
	else
	{
		t.entityelement[t.e].usingphysicsnow = 0;
	}
}

void physics_setupebestructure ( void )
{
	//  create EBE physics for this object. Takes tphyobj and entid and e
	if ( t.tphyobj>0 ) 
	{
		if ( ObjectExist(t.tphyobj) == 1 ) 
		{
			ebe_physics_setupebestructure ( t.tphyobj, t.e );
		}
	}
}

void physics_setupobject ( void )
{
	// default is no special handling of OBJ collision meshes
	ODESetOBJLoadingFilename("");

	// create physics for this object. Takes tphyobj and entid and e
	if ( t.tphyobj>0 ) 
	{
		if ( ObjectExist(t.tphyobj) == 1 ) 
		{
			// hull decomp take time - offer ability to save and reload
			if (t.tshape == 10)
			{
				char pNoFPE[MAX_PATH];
				strcpy(pNoFPE, t.entitybank_s[t.entid].Get());
				pNoFPE[strlen(pNoFPE) - 4] = 0;
				char pObjectFilename[MAX_PATH];
				sprintf(pObjectFilename, "%s\\Files\\entitybank\\%s.bullet", g.fpscrootdir_s.Get(), pNoFPE);
				ODESetMeshFilename(pObjectFilename);
				GG_GetRealPath(pObjectFilename, 0);
				if (FileExist(pObjectFilename) == 0)
				{
					// use only the name
					char pJustName[MAX_PATH];
					strcpy(pJustName, "");
					for (int n = strlen(pNoFPE) - 1; n > 0; n--)
					{
						if (pNoFPE[n] == '/' || pNoFPE[n] == '\\')
						{
							sprintf(pJustName, " - %s", pNoFPE + n + 1);
							break;
						}
					}
					char pHullDecompWait[256];
					sprintf(pHullDecompWait, "HULL DECOMPOSITION%s", pJustName);
					t.screenprompt_s = pHullDecompWait;
					extern DWORD g_SensibleMessageTimer;
					g_SensibleMessageTimer = 1;
					printscreenprompt(t.screenprompt_s.Get());
				}
			}

			extern bool physics_playground;
			// skip if material should come direct from DBO mesh data
			if (t.entityprofile[t.entid].materialindex != 99999)
			{
				SetObjectArbitaryValue ( t.tphyobj, t.entityprofile[t.entid].materialindex );
			}
			if ( t.tstatic == 1 ) // now allow physics entities in multiplayer || t.game.runasmultiplayer == 1 ) 
			{
				// if static, need to ensure FIXNEWY pivot is respected
				if ( t.tstatic == 1 ) 
				{
					t.tstaticfixnewystore_f=ObjectAngleY(t.tphyobj);
					RotateObject (  t.tphyobj,ObjectAngleX(t.tphyobj),ObjectAngleY(t.tphyobj)+t.entityprofile[t.entid].fixnewy,ObjectAngleZ(t.tphyobj) );
				}

				// if special polygon collision mode (that uses OBJ) handle now)
				if (t.tshape == 8)
				{
					// set the FPE filename so the .OBJ or _COL.obj can be checked and used if present
					char pFullFPEFilename[MAX_PATH];
					sprintf(pFullFPEFilename, "%s\\Files\\entitybank\\%s", g.fpscrootdir_s.Get(), t.entitybank_s[t.entid].Get());
					ODESetOBJLoadingFilename(pFullFPEFilename);

					// and now treat as regular polygon collision mesh
					t.tshape = 2; 
				}

				//  create the physics now
				if (physics_playground)
				{
					t.entityelement[t.e].staticflag = 0;
					ODECreateDynamicTriangleMesh(t.tphyobj, t.tweight, t.tfriction, -1, 1); //Turn everything into convex hull.
				}
				else if (t.tshape >= 1000 && t.tshape < 2000)
				{
					ODECreateStaticBox (  t.tphyobj,t.tshape-1000 );
				}
				else if ( t.tshape >= 2000 && t.tshape < 3000 ) 
				{
					ODECreateStaticTriangleMesh (  t.tphyobj,t.tshape-2000 );
				}
				else if ( t.tshape == 1 ) 
				{
					ODECreateStaticBox ( t.tphyobj );
				}
				else if (t.tshape == 6)
				{
					ODECreateStaticSphere ( t.tphyobj );
				}
				else if (t.tshape == 7)
				{
					ODECreateStaticCylinder ( t.tphyobj );
				}
				else if ( t.tshape == 2 || t.tshape == 9 || t.tshape == 10)
				{
					if (  t.tshape == 2 ) 
					{
						if (  t.tcollisionscaling != 100 ) 
						{
							ODECreateStaticTriangleMesh (  t.tphyobj,-1,t.tcollisionscaling );
						}
						else
						{
							ODECreateStaticTriangleMesh (  t.tphyobj );
						}
					}
					else
					{
						if (t.tshape == 10)
						{
							ODECreateStaticTriangleMesh (t.tphyobj, -1, t.tcollisionscaling, 2);
						}
						else
						{
							ODECreateStaticTriangleMesh (t.tphyobj, -1, t.tcollisionscaling, 1);
						}
					}
				}
				else if ( t.tshape == 3 ) 
				{
					physics_setuptreecylinder ( );
				}
				// tshape 4 is a list of physics objects from the importer
				else if ( t.tshape == 4 ) 
				{
					physics_setupimportershapes ( );
				}
				// if static, restore object before leaving
				if ( t.tstatic == 1 ) 
				{
					RotateObject (  t.tphyobj,ObjectAngleX(t.tphyobj),t.tstaticfixnewystore_f,ObjectAngleZ(t.tphyobj) );
				}
			}
			else
			{
				// objects will fall through Floor (  if they are perfectly sitting on it )
				PositionObject ( t.tphyobj, ObjectPositionX(t.tphyobj), ObjectPositionY(t.tphyobj)+0.1, ObjectPositionZ(t.tphyobj) );

				if(physics_playground)
				{
					ODECreateDynamicTriangleMesh(t.tphyobj, t.tweight, t.tfriction, -1, 1);
				}
				else
				if ( t.tshape == 6 )     
				{
					// Sphere
					ODECreateDynamicSphere(t.tphyobj, t.tweight, t.tfriction, 0.01f);
				}
				else if ( t.tshape == 7 ) 
				{
					// Cylinder
					ODECreateDynamicCylinder(t.tphyobj, t.tweight, t.tfriction, 0.01f);
				}
				else if (t.tshape == 9)
				{
					// Dynamic convex hull
					ODECreateDynamicTriangleMesh(t.tphyobj, t.tweight, t.tfriction, -1, 1);
				}
				else if (t.tshape == 10)
				{
					// Dynamic hull decomp
					ODECreateDynamicTriangleMesh(t.tphyobj, t.tweight, t.tfriction, -1, 2);
				}
				else
				{
					// box
					ODECreateDynamicBox(t.tphyobj, -1, 0, t.tweight, t.tfriction, -1);
				}

				// apply zero gravity if ticked
				if (t.entityelement[t.e].eleprof.iAffectedByGravity == 0)
				{
					ODESetNoGravity(t.tphyobj, 0);
				}
			}
		}
	}
	ODESetMeshFilename("");
	ODESetOBJLoadingFilename("");
}

int physics_findfreegamerealtimeobj ( void )
{
	int iFreeObj = g.gamerealtimeobjoffset;
	while ( ObjectExist ( iFreeObj ) == 1 && iFreeObj < g.gamerealtimeobjoffsetmax )
	{
		iFreeObj++;
	}
	if ( iFreeObj == g.gamerealtimeobjoffsetmax )
	{
		if ( ObjectExist ( iFreeObj ) == 1 ) DeleteObject ( iFreeObj );
	}
	return iFreeObj;
}

void physics_freeallgamerealtimeobjs ( void )
{
	for ( int iObj = g.gamerealtimeobjoffset; iObj <= g.gamerealtimeobjoffsetmax; iObj++ )
		if ( ObjectExist ( iObj ) == 1 ) 
			DeleteObject ( iObj );
}

void physics_setupimportershapes ( void )
{
	// flag to control if debug collision boxes should be left
	bool bLeaveDebugCollisionBoxes = false;
	if ( g.globals.showdebugcollisonboxes == 1 ) bLeaveDebugCollisionBoxes = true;

	// get collision boxes data from entity to make importer collision shapes
	if ( t.entid > MAX_ENTITY_PHYSICS_BOXES*2  ) 
	{
		Dim2 ( t.entityphysicsbox , t.entid , MAX_ENTITY_PHYSICS_BOXES   );
	}
	ODEStartStaticObject (  t.tphyobj );
	float fMoveToObjectWorldX = ObjectPositionX ( t.tphyobj );
	float fMoveToObjectWorldY = ObjectPositionY ( t.tphyobj );
	float fMoveToObjectWorldZ = ObjectPositionZ ( t.tphyobj );
	for ( t.tcount = 0 ; t.tcount <= t.entityprofile[t.entid].physicsobjectcount-1; t.tcount++ )
	{
		if (  ObjectExist(g.tempimporterlistobject)  )  DeleteObject (  g.tempimporterlistobject );
		int iObjectToUse = g.tempimporterlistobject;
		if ( bLeaveDebugCollisionBoxes == true ) iObjectToUse = physics_findfreegamerealtimeobj();
		t.tescale=t.entityprofile[t.entid].scale;
		if (  t.tescale>0 ) 
		{
			t.tnewscalex_f=t.tescale+t.entityelement[t.e].scalex;
			t.tnewscaley_f=t.tescale+t.entityelement[t.e].scaley;
			t.tnewscalez_f=t.tescale+t.entityelement[t.e].scalez;
		}
		else
		{
			t.tnewscalex_f=100+t.entityelement[t.e].scalex;
			t.tnewscaley_f=100+t.entityelement[t.e].scaley;
			t.tnewscalez_f=100+t.entityelement[t.e].scalez;
		}
		t.tnewsizex_f=(t.tnewscalex_f/100.0)*(t.entityphysicsbox[t.entid][t.tcount].SizeX);
		t.tnewsizey_f=(t.tnewscaley_f/100.0)*(t.entityphysicsbox[t.entid][t.tcount].SizeY);
		t.tnewsizez_f=(t.tnewscalez_f/100.0)*(t.entityphysicsbox[t.entid][t.tcount].SizeZ);
		MakeObjectBox ( iObjectToUse, t.tnewsizex_f, t.tnewsizey_f, t.tnewsizez_f );
		float tNewOffX = (t.tnewscalex_f/100.0) * t.entityphysicsbox[t.entid][t.tcount].OffX;
		float tNewOffY = (t.tnewscaley_f/100.0) * t.entityphysicsbox[t.entid][t.tcount].OffY;
		float tNewOffZ = (t.tnewscalez_f/100.0) * t.entityphysicsbox[t.entid][t.tcount].OffZ;
		t.tocy_f=ObjectSizeY(t.tphyobj,1)/2.0;
		PositionObject ( iObjectToUse, tNewOffX, t.tocy_f + tNewOffY, tNewOffZ );
		RotateObject ( iObjectToUse, t.entityphysicsbox[t.entid][t.tcount].RotX , t.entityphysicsbox[t.entid][t.tcount].RotY , t.entityphysicsbox[t.entid][t.tcount].RotZ );
		ODEAddStaticObjectBox ( t.tphyobj, iObjectToUse, t.entityprofile[t.entid].materialindex );
		if ( bLeaveDebugCollisionBoxes == true ) 
		{
			FixObjectPivot ( iObjectToUse );
			RotateObject ( iObjectToUse, t.entityelement[t.e].rx, t.entityelement[t.e].ry, t.entityelement[t.e].rz );
			GGVECTOR3 vecOffset = GGVECTOR3 ( tNewOffX, t.tocy_f + tNewOffY, tNewOffZ );
			sObject* pObjectPtr = GetObjectData ( t.tphyobj );
			GGVec3TransformCoord ( &vecOffset, &vecOffset, &pObjectPtr->position.matRotation );
			PositionObject ( iObjectToUse, fMoveToObjectWorldX+vecOffset.x, fMoveToObjectWorldY+vecOffset.y, fMoveToObjectWorldZ+vecOffset.z );
		}
	}
	if ( ObjectExist(g.tempimporterlistobject)  )  DeleteObject (  g.tempimporterlistobject );
	ODEEndStaticObject (  t.tphyobj, 0 );
}

void physics_setuptreecylinder ( void )
{
	//  takes; tphyobj and entid and sets up a tree cylinder
	if (  t.tphyobj < 1  )  return;
	if (  ObjectExist(t.tphyobj)  ==  0  )  return;
	if (  t.entid < 1  )  return;

	//  Tree height (adjusted for scale)
	t.tSizeY_f = ObjectSizeY(t.tphyobj,1);

	//  if have ABS position from AI OBSTACLE calc, use that instead
	if (  t.entityelement[t.e].abscolx_f != -1 ) 
	{
		t.tFinalX_f = t.entityelement[t.e].abscolx_f;
		t.tFinalZ_f = t.entityelement[t.e].abscolz_f;
	}
	else
	{
		t.tFinalX_f = ObjectPositionX(t.tphyobj);
		t.tFinalZ_f = ObjectPositionZ(t.tphyobj);
	}
	t.tFinalY_f = ObjectPositionY(t.tphyobj) + (t.tSizeY_f/2.0);

	//  if have ABS radius from AI OBSTACLE calc, use that instead
	if (  t.entityelement[t.e].abscolradius_f != -1 ) 
	{
		t.tSizeX_f = t.entityelement[t.e].abscolradius_f;
		t.tSizeZ_f = t.entityelement[t.e].abscolradius_f;
	}
	else
	{
		t.tSizeX_f = 20;
		t.tSizeZ_f = 20;
	}

	//  increase size by 25%
	t.tSizeX_f=t.tSizeX_f*1.25;
	t.tSizeZ_f=t.tSizeZ_f*1.25;

	// now create our physics object
	SetObjectArbitaryValue (t.tphyobj, 3);// 3-wood not 6 flesh!
	ODECreateStaticCylinder ( t.tphyobj,t.tFinalX_f,t.tFinalY_f,t.tFinalZ_f,t.tSizeX_f,t.tSizeY_f,t.tSizeZ_f,0,0,0 );
}

void physics_disableobject ( void )
{
	ODEDestroyObject (  t.tphyobj );
}

void physics_beginsimulation ( void )
{
	t.machineindependentphysicsupdate = timeGetSecond();
}

void physics_pausephysics ( void )
{
	t.ptimer1 = timeGetSecond();
}

void physics_resumephysics ( void )
{
	//t.ptimer2=PerformanceTimer() ; t.ptimer2=t.ptimer2/t.pfreq1;
	t.ptimer2 = timeGetSecond();
	t.machineindependentphysicsupdate=t.machineindependentphysicsupdate+(t.ptimer2-t.ptimer1);
}

#include "wiProfiler.h"

// GG: a weapon a dead character drops becomes a moving box (G-Entity.cpp) that nothing removes: wedged in a wreck it
// never stops, and keeps the wreck's whole island awake (the Lua session's runs 288 and 292: a Cygnus wreck at 53-56
// contact points beside two dropped guns, 8-20 ms of physics a frame). Each is checked twice a second and loses its
// body once it has been still for half a second or is asleep, or after 15 s (60 s while still falling fast); it stays
// drawn where it lies
struct sDroppedWeapon { int iObject; float fDropped; };
static std::vector<sDroppedWeapon> g_DroppedWeapons;
static float g_fDroppedWeaponsChecked = 0;

void physics_trackdroppedweapon ( int iObject )
{
	if ( iObject <= 0 ) return;
	for ( const sDroppedWeapon& weapon : g_DroppedWeapons ) if ( weapon.iObject == iObject ) return;
	g_DroppedWeapons.push_back ( { iObject, timeGetSecond() } );
}

static void physics_settledroppedweapons ( void )
{
	if ( g_DroppedWeapons.empty() ) return;
	const float fNow = timeGetSecond();
	if ( fNow - g_fDroppedWeaponsChecked < 0.5f ) return;
	g_fDroppedWeaponsChecked = fNow;
	std::vector<int> objects, states ( g_DroppedWeapons.size() );
	for ( const sDroppedWeapon& weapon : g_DroppedWeapons ) objects.push_back ( weapon.iObject );
	PhysicsQuery_BodiesAtRest ( objects.data(), (int)objects.size(), 0.5f, 200.0f, states.data() );
	for ( int i = (int)g_DroppedWeapons.size() - 1; i >= 0; i-- )
	{
		const float fAge = fNow - g_DroppedWeapons[i].fDropped;
		bool bDone = states[i] == 0;
		if ( !bDone && fAge >= 1.0f && states[i] == 2 ) bDone = true;
		if ( !bDone && ( ( fAge >= 15.0f && states[i] != 3 ) || fAge >= 60.0f ) ) bDone = true;
		if ( !bDone ) continue;
		if ( states[i] != 0 ) ODEDestroyObject ( g_DroppedWeapons[i].iObject );
		g_DroppedWeapons.erase ( g_DroppedWeapons.begin() + i );
	}
}

void physics_loop ( void )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	// shuffle virtual trees about as the player needs
	physics_managevirtualtreecylinders();

	// Player control
	extern int iEnterGodMode;
	if (iEnterGodMode != 2)
	{
		if (g.gproducelogfiles == 2) timestampactivity(0, "calling physics_player");
		physics_player();
	}
	//  Update physics system
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling timeGetSecond");
	t.tphysicsadvance_f = timeGetSecond() - t.machineindependentphysicsupdate;
	if (  t.tphysicsadvance_f >= (1.0/120.0) ) 
	{
		//  only process physics once we reach the minimum substep constant
		if ( t.tphysicsadvance_f>0.05f ) t.tphysicsadvance_f = 0.05f;
		t.machineindependentphysicsupdate = timeGetSecond();
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling ODEUpdate");
		ODEUpdate ( t.tphysicsadvance_f );
	}
	physics_settledroppedweapons ( );
}

void physics_free ( void )
{
	g_DroppedWeapons.clear();
	// special hybrid collision mode can hide static limbs, so reshow them when physics done
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		t.obj = t.entityelement[t.e].obj;
		if (t.obj > 0)
		{
			if (ObjectExist(t.obj) == 1)
			{
				t.entid = t.entityelement[t.e].bankindex;
				if (t.entityprofile[t.entid].collisionmode == 31)
				{
					// reshow previously hidden static limbs
					PerformCheckListForLimbs(t.obj);
					for (int c = ChecklistQuantity(); c >= 1; c += -1)
					{
						t.tname_s = Lower(ChecklistString(c));
						LPSTR pNamePtr = t.tname_s.Get();
						if (strlen(pNamePtr) > 7)
						{
							if (strnicmp(pNamePtr + strlen(pNamePtr) - 7, "_static", 7) == NULL)
							{
								ShowLimb(t.obj, c - 1);
							}
						}
					}

					// and delete the secondary object created to show the static limbs detached from the primary
					if (t.entityelement[t.e].attachmentobj > 0)
					{
						int iSecondaryObjID = t.entityelement[t.e].attachmentobj;
						if (ObjectExist(iSecondaryObjID) == 1) DeleteObject (iSecondaryObjID);
						t.entityelement[t.e].attachmentobj = 0;
					}
				}
			}
		}
	}

	// just hide the virtual tree cylinders until we use them again
	physics_freevirtualtreecylinders();

	// remove any game realtime objects (used for debugging collision boxes, possible LUA spawned 3D objects, etc)
	physics_freeallgamerealtimeobjs();

	// free terrain physics object
	if ( t.terrain.superflat == 0 ) 
	{
		for ( t.tobj = t.terrain.TerrainLODOBJStart ; t.tobj <= t.terrain.TerrainLODOBJFinish; t.tobj++ )
		{
			if (  ObjectExist(t.tobj) == 1 ) 
			{
				ODEDestroyObject (  t.tobj );
			}
		}
	}
	else
	{
		//PE: Need to free all terrain phy objects, 7000+ , remember we keep the pMem1,pMem2 memory with data that need to be freed.
		if (ObjectExist(t.tphysicsterrainobjstart) == 1)
		{
			if (t.tphysicsterrainobjend > t.tphysicsterrainobjstart)
			{
				for (int obj = t.tphysicsterrainobjstart; obj <= t.tphysicsterrainobjend; obj++)
				{
					ODEDestroyObject(obj);
					DeleteObject(obj);
				}
			}
			else
			{
				ODEDestroyObject(t.tphysicsterrainobjstart);
				DeleteObject(t.tphysicsterrainobjstart);
			}
		}
	}

	//  detatch entity from physics
	for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
	{
		t.obj=t.entityelement[t.e].obj;
		if ( t.obj>0 ) 
		{
			if ( ObjectExist(t.obj) == 1 ) 
			{
				if ( t.entityelement[t.e].usingphysicsnow == 1 ) 
				{
					t.tphyobj=t.obj  ; physics_disableobject ( );
					t.entityelement[t.e].usingphysicsnow=0;
				}
			}
		}
	}

	// Clean-up physics system
	extern int g_iDevToolsOpen;
	if (g_iDevToolsOpen != 0)
	{
		physics_set_debug_draw(0);
	}
	ODEEnd(); 
	g.gphysicssessionactive=0;
}

void physics_explodesphere ( void )
{
	//  takes texplodex#,texplodey#,texplodez#,texploderadius#,t.texplodesourceEntity
	t.tstrengthofexplosion_f = t.tDamage_f;

	//  detect if player within radius and apply damage
	t.tdx_f=ObjectPositionX(t.aisystem.objectstartindex)-t.texplodex_f;
	t.tdy_f=ObjectPositionY(t.aisystem.objectstartindex)-t.texplodey_f;
	t.tdz_f=ObjectPositionZ(t.aisystem.objectstartindex)-t.texplodez_f;
	t.tdd_f=Sqrt(abs(t.tdx_f*t.tdx_f)+abs(t.tdy_f*t.tdy_f)+abs(t.tdz_f*t.tdz_f));
	if (  t.tdd_f<t.texploderadius_f ) 
	{
		//  apply camera shake for nearby explosion
		t.playercontrol.camerashake_f=(((t.texploderadius_f*2)-(t.tdd_f/(t.texploderadius_f*2)))*t.tstrengthofexplosion_f)/150.0/20.0;
	}
	if (  t.tdd_f<t.texploderadius_f ) 
	{
		// apply damage
		t.te=-1;
		t.tdamage = (1.0f - (t.tdd_f / t.texploderadius_f)) * t.tstrengthofexplosion_f;
		if ( t.tdamage > t.tstrengthofexplosion_f ) t.tdamage = t.tstrengthofexplosion_f;
		if ( t.game.runasmultiplayer == 1 ) 
		{
			t.tsteamwasnetworkdamage = 0;
			if ( t.entityelement[t.texplodesourceEntity].mp_networkkill == 1 ) 
			{
				// 13032015 0XX - Team Multiplayer
				if ( g.mp.team  ==  0 || g.mp.friendlyfireoff  ==  0 || t.mp_team[t.entityelement[t.texplodesourceEntity].mp_killedby]  !=  t.mp_team[g.mp.me] ) 
				{
					t.tsteamwasnetworkdamage = 1;
				}
			}
		}
		if ( t.game.runasmultiplayer  ==  1 ) 
		{
			// 13032015 0XX - Team Multiplayer
			// Can't kill yourself if friendly fire is off
			if ( t.tsteamwasnetworkdamage  ==  1 || g.mp.friendlyfireoff  ==  0 || g.mp.damageWasFromAI  ==  1 ) 
			{
				physics_player_takedamage ( );
			}
		}
		else
		{
			physics_player_takedamage ( );
		}
		if ( t.game.runasmultiplayer == 1 ) t.tsteamwasnetworkdamage = 0;

		//  apply force to push player
		t.playercontrol.pushangle_f = atan2deg(t.tdx_f,t.tdz_f);
		float fForceRelativeToDamage = t.tstrengthofexplosion_f / 30.0f;
		t.playercontrol.pushforce_f = (1.0f-(t.tdd_f/t.texploderadius_f))*fForceRelativeToDamage;
	}
	//  if the explosion was caused by another player, we let them handle it rather than us
	if (  t.game.runasmultiplayer  ==  1 ) 
	{
		if (  t.entityelement[t.texplodesourceEntity].mp_networkkill  ==  1 && g.mp.damageWasFromAI  ==  0 ) 
		{
			return;
		}
	}
	//  create a sphere of force at this location
	for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		if (  g.mp.damageWasFromAI  ==  0 ) 
		{
			if (  t.texplodesourceEntity > 0 && t.e == t.texplodesourceEntity  )  t.entid = 0;
		}
		if ( t.entid > 0 && t.entityelement[t.e].obj > 0 ) 
		{
			// early rejects
			if (t.entityprofile[t.entid].ismarker != 0) continue;
			if (t.entityelement[t.e].active == 0) continue;
			if (t.entityelement[t.e].staticflag == 1) continue;
			if (t.entityelement[t.e].y <= -899999) continue;

			// 220618 - use center of object, not coordinate of entity XYZ
			float fCenterOfEntityX = ObjectPositionX(t.entityelement[t.e].obj) + GetObjectCollisionCenterX(t.entityelement[t.e].obj);
			float fCenterOfEntityY = ObjectPositionY(t.entityelement[t.e].obj) + GetObjectCollisionCenterY(t.entityelement[t.e].obj);
			float fCenterOfEntityZ = ObjectPositionZ(t.entityelement[t.e].obj) + GetObjectCollisionCenterZ(t.entityelement[t.e].obj);
			t.tdx_f = fCenterOfEntityX - t.texplodex_f;
			t.tdy_f = fCenterOfEntityY - t.texplodey_f;
			t.tdz_f = fCenterOfEntityZ - t.texplodez_f;
			t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f)+abs(t.tdy_f*t.tdy_f)+abs(t.tdz_f*t.tdz_f));
			if (t.tdd_f < t.texploderadius_f)
			{
				// 220618 - before apply actual entity damage/effect, ensure a line of sight exists (could be behind wall/door)
				float fRayDestFromExplosionX = fCenterOfEntityX - t.texplodex_f;
				float fRayDestFromExplosionY = fCenterOfEntityY - t.texplodey_f;
				float fRayDestFromExplosionZ = fCenterOfEntityZ - t.texplodez_f;
				fRayDestFromExplosionX += t.texplodex_f;
				fRayDestFromExplosionY += t.texplodey_f;
				fRayDestFromExplosionZ += t.texplodez_f;

				// refer to previously collected information on anything that explodes (performance boost and anit-freeze system)
				t.tintersectvalue = -1;
				int iExplodingE = t.texplodesourceEntity;
				if (iExplodingE > 0)
				{
					if (t.entityelement[iExplodingE].iPreScannedVisible.size() > 0)
					{
						for (int i = 0; i < t.entityelement[iExplodingE].iPreScannedVisible.size(); i++)
						{
							if (t.e == t.entityelement[iExplodingE].iPreScannedVisible[i])
							{
								t.tintersectvalue = 0;
								break;
							}
						}
					}
				}

				// if still no block, must check all (the slower way)
				if (t.tintersectvalue == -1)
				{
					// NOTE: May speed this up by setting all above prescan objects to cursor objects for this one test..
					// must perform direct test as dont have prescannedvis list to compare if we have a direct ray line
					t.tintersectvalue = IntersectAll(g.entityviewstartobj, g.entityviewendobj,
										t.texplodex_f, t.texplodey_f, t.texplodez_f, 
										fRayDestFromExplosionX, fRayDestFromExplosionY, fRayDestFromExplosionZ, 
										t.entityelement[t.e].obj );
				}

				if ( t.tintersectvalue == 0 || t.tintersectvalue == t.entityelement[ t.texplodesourceEntity ].obj )
				{
					t.tdamage = (1.0f - (t.tdd_f / t.texploderadius_f)) * t.tstrengthofexplosion_f;
					t.tdamageforce = (1.0f - (t.tdd_f / t.texploderadius_f)) * (t.tstrengthofexplosion_f);
					t.brayx1_f = t.texplodex_f;
					t.brayy1_f = t.texplodey_f;
					t.brayz1_f = t.texplodez_f;
					t.brayx2_f = fCenterOfEntityX;
					t.brayy2_f = fCenterOfEntityY;
					t.brayz2_f = fCenterOfEntityZ;
					t.braydx_f = t.brayx2_f-t.brayx1_f;
					t.braydz_f = t.brayz2_f-t.brayz1_f;
					t.braydist_f = Sqrt(abs(t.braydx_f*t.braydx_f)+abs(t.braydz_f*t.braydz_f));
					if ( t.braydist_f < 75 ) t.brayy2_f = t.texplodey_f + 100.0;
					if ( t.tdamageforce > 150 ) t.tdamageforce = 150;
					// explosion (time delayed explosion), except when a grenade/missile vs exploder, instead make it instant :)
					t.tdamagesource = 2;
					if (t.texplodesourceEntity == 0) t.tdamagesource = 3;
					t.ttte = t.e ; entity_applydamage( ); t.e = t.ttte;

					//  inform darkAI of the explosion
					t.tsx_f = t.entityelement[t.e].x; 
					t.tsz_f = t.entityelement[t.e].z;
					darkai_makeexplosionsound ( );
				}
			}
		}
	}

	//  Reset flag for ai damage
	g.mp.damageWasFromAI = 0;
}

void physics_player_init ( void )
{
	// One Player In Single Player Game 
	t.plrid=1;
	t.tnostartmarker=1;
	g.flashLightKeyEnabled = true;

	//  Initialise player settings
	if ( t.game.levelplrstatsetup == 1 )
	{
		// starting stats
		t.playercontrol.startlives=0;
		t.playercontrol.startstrength=100;
		t.playercontrol.startviolent=1;
		t.playercontrol.starthasweapon=0;
		t.playercontrol.starthasweaponqty=0;
		t.playercontrol.speedratio_f=1.0;
		t.playercontrol.hurtfall=100;
		t.playercontrol.canrun=1;
		t.player[t.plrid].lives=t.playercontrol.startlives;
		t.player[t.plrid].powers.level = 100;

		//LB: new player health intercept
		t.player[t.plrid].health=t.playercontrol.startstrength; // overwritten later when LUA init and active!

		// act on start marker
		t.playercontrol.hurtfall=0;
		t.playercontrol.speedratio_f=1.0;
	}
	for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		if ( t.entityprofile[t.entid].ismarker == 1 ) 
		{
			//  Player Start Marker Settings
			t.terrain.playerx_f=t.entityelement[t.e].x;
			t.terrain.playery_f=t.entityelement[t.e].y;
			t.terrain.playerz_f=t.entityelement[t.e].z;

			void FixEulerZInverted(float &ax, float &ay, float &az);
			float ax, ay, az;
			ax = t.entityelement[t.e].rx;
			ay = t.entityelement[t.e].ry;
			az = t.entityelement[t.e].rz;
			FixEulerZInverted(ax,ay,az);

			t.terrain.playerax_f=0;
			t.terrain.playeray_f = ay;
			t.camangy_f=t.terrain.playeray_f;
			t.terrain.playeraz_f=0;
			t.playercontrol.finalcameraangley_f=t.terrain.playeray_f;

			//  Player Global Settings for this level
			if ( t.game.levelplrstatsetup == 1 )
			{
				t.playercontrol.startlives = t.entityelement[t.e].eleprof.lives;
				t.playercontrol.startstrength=t.entityelement[t.e].eleprof.strength;
				if (  t.playercontrol.thirdperson.enabled == 1 ) 
				{
					t.tprotagoniste=t.playercontrol.thirdperson.charactere;
					t.playercontrol.starthasweapon=t.entityelement[t.tprotagoniste].eleprof.hasweapon;
				}
				else
				{
					t.playercontrol.starthasweapon=t.entityelement[t.e].eleprof.hasweapon;
				}
				t.playercontrol.starthasweaponqty=t.entityelement[t.e].eleprof.quantity;
				t.playercontrol.startviolent=t.entityelement[t.e].eleprof.isviolent;
				t.playercontrol.speedratio_f=t.entityelement[t.e].eleprof.speed/100.0;
				t.playercontrol.hurtfall=t.entityelement[t.e].eleprof.hurtfall;

				t.playercontrol.iPlayHeartBeatSoundOff = t.entityelement[t.e].eleprof.perentityflags & 1;
				t.playercontrol.iShowScreenBloodOff = (t.entityelement[t.e].eleprof.perentityflags & (1 << 1)) >> 1;
				t.playercontrol.fWeaponDamageMultiplier = t.entityelement[t.e].eleprof.weapondamagemultiplier;
				t.playercontrol.fMeleeDamageMultiplier = t.entityelement[t.e].eleprof.meleedamagemultiplier;
				t.playercontrol.fSwimSpeed = t.entityelement[t.e].eleprof.iSwimSpeed;

				// new property of player start marker to disable flashlight
				if (t.entityelement[t.e].eleprof.usespotlighting == 1 )
					g.flashLightKeyEnabled = false;
				else
					g.flashLightKeyEnabled = true;

				// 050416 - if in parental mode, ensure no weapon at start
				if ( g.quickparentalcontrolmode == 2 )
				{
					// only ban modern day weapons, not fireball
					int iPlrGunID = t.weaponindex=t.playercontrol.starthasweapon;
					if ( iPlrGunID > 0 )
					{
						if ( strnicmp ( t.gun[iPlrGunID].name_s.Get(), "modernday", 9 ) == NULL )
						{
							t.playercontrol.starthasweapon = 0;
							t.playercontrol.starthasweaponqty = 0;
							t.playercontrol.startviolent = 0;
						}
					}
				}

				//  Populate lives and health with default player
				t.player[t.plrid].lives=t.playercontrol.startlives;

				//LB: new player health intercept
				t.player[t.plrid].health=t.playercontrol.startstrength; // overwritten later when LUA init and active!

				//  Start Marker present
				t.tnostartmarker=0;
			}
			else
			{
				// level 2 and above do not control start stats or weaponry
			}
		}
	}

	float fEditableSizeHalved = GGTerrain_GetEditableSize();
	t.terraineditableareasizeminx = -fEditableSizeHalved;
	t.terraineditableareasizeminz = -fEditableSizeHalved;
	t.terraineditableareasizemaxx = fEditableSizeHalved;
	t.terraineditableareasizemaxz = fEditableSizeHalved;
	if (t.terrain.playerx_f < t.terraineditableareasizeminx + 100.0f) t.terrain.playerx_f = t.terraineditableareasizeminx + 100.0f;
	if (t.terrain.playerx_f > t.terraineditableareasizemaxx - 100.0f) t.terrain.playerx_f = t.terraineditableareasizemaxx - 100.0f;
	if (t.terrain.playerz_f < t.terraineditableareasizeminz + 100.0f) t.terrain.playerz_f = t.terraineditableareasizeminz + 100.0f;
	if (t.terrain.playerz_f > t.terraineditableareasizemaxz - 100.0f) t.terrain.playerz_f = t.terraineditableareasizemaxz - 100.0f;

	//  If no player start marker, reset player physics tweakables
	if ( t.game.levelplrstatsetup == 1 )
	{
		if ( t.tnostartmarker == 1 ) physics_inittweakables ( );
	}

	// if multiplayer mode, change start position to the multiplayer start marker default
	if ( t.game.runasmultiplayer == 1 ) 
	{
		// store good one
		float fGoodX = t.terrain.playerx_f;
		float fGoodY = t.terrain.playery_f;
		float fGoodZ = t.terrain.playerz_f;
		float fGoodA = t.terrain.playeray_f;

		// chose a multiplayer start position at random
		int iChoose = 1;
		if ( t.tmpstartindex > 1 ) iChoose = 1 + (rand() % t.tmpstartindex);
		t.terrain.playerx_f=t.mpmultiplayerstart[iChoose].x;
		t.terrain.playery_f=t.mpmultiplayerstart[iChoose].y;
		t.terrain.playerz_f=t.mpmultiplayerstart[iChoose].z;
		t.terrain.playeray_f=t.mpmultiplayerstart[iChoose].angle;
		t.camangy_f=t.terrain.playeray_f;
		if ( t.terrain.playerx_f < 100 )
		{
			// no start position, revert to regular start marker
			t.terrain.playerx_f = fGoodX;
			t.terrain.playery_f = fGoodY;
			t.terrain.playerz_f = fGoodZ;
			t.terrain.playeray_f = fGoodA;
			t.camangy_f=t.terrain.playeray_f;
		}
		t.playercontrol.finalcameraangley_f=t.terrain.playeray_f;
	}

	//  Player start height (marker or no)
	float fRawTerrainH = BT_GetGroundHeight(t.terrain.TerrainID, t.terrain.playerx_f, t.terrain.playerz_f);
	t.tbestterrainplayery_f = fRawTerrainH + t.terrain.adjaboveground_f;

	// DEBUG: Log player start height calculation
	{
		char dbg[512];
		sprintf(dbg, "PLAYER_START: entityY=%.1f terrainH=%.1f adjAbove=%.1f bestY=%.1f pos=(%.1f,%.1f,%.1f)",
			t.terrain.playery_f, fRawTerrainH, t.terrain.adjaboveground_f, t.tbestterrainplayery_f,
			t.terrain.playerx_f, t.terrain.playery_f, t.terrain.playerz_f);
		timestampactivity(0, dbg);
	}

	// GGMAX 3.44 (DX11 1d49f004): DX11 commented this clamp out - a Start Marker placed
	// underwater was being teleported to waterline+20, making an underwater-opening level
	// impossible to build. DX11's own note: "new request wants to start underwater if the
	// marker is underwater".
	//  also ensure ABOVE water Line (  )
	//if (  t.tbestterrainplayery_f<t.terrain.waterliney_f+20+t.terrain.adjaboveground_f )
	//{
	//	t.tbestterrainplayery_f=t.terrain.waterliney_f+20+t.terrain.adjaboveground_f;
	//}
	if (  t.terrain.playery_f == 0 )
	{
		t.terrain.playery_f=t.tbestterrainplayery_f;
	}
	else
	{
		t.terrain.playery_f=t.terrain.playery_f+t.terrain.adjaboveground_f;
		if (  t.terrain.playery_f<t.tbestterrainplayery_f  )  t.terrain.playery_f = t.tbestterrainplayery_f;
	}
	// DEBUG: Log final player Y
	{
		char dbg[256];
		sprintf(dbg, "PLAYER_START_FINAL: playery=%.1f", t.terrain.playery_f);
		timestampactivity(0, dbg);
	}

	// Select weapon if start marker specifies it
	if ( t.game.levelplrstatsetup == 1 )
	{
		// sometimes the player would spawn with a "phantom weapon" where the weapon slot would be occupied and obstruct collecting/using weapons
		// this makes sure that the player start the level with no weapons and unoccupied weapon slots
		physics_player_resetWeaponSlots();

		// if starting with a weapon, grant it here
		if (  t.playercontrol.starthasweapon>0 ) 
		{
			// weapon added to weapon slot 1
			t.weaponindex=t.playercontrol.starthasweapon;
			t.tqty=t.playercontrol.starthasweaponqty;
			physics_player_addweapon ( );

			// as this was never a level-object weapon, we force it into the slot 
			// and ensure it cannot be removed or dropped, there is no object associated with it
			inventoryContainerType item;
			item.e = 0;
			cstr thisWeaponTitle = gun_names_tonormal(t.gun[t.weaponindex].name_s.Get());
			item.collectionID = find_rpg_collectionindex(thisWeaponTitle.Get());
			if (item.collectionID == 0)
			{
				thisWeaponTitle = t.gun[t.weaponindex].name_s.Get();
				item.collectionID = find_rpg_collectionindex(thisWeaponTitle.Get());
			}
			item.slot = 0;
			for (int n = 1; n <= 10; n++) { if (t.weaponslot[n].pref == t.weaponindex) { item.slot = n - 1; break; } }
			t.inventoryContainer[1].push_back(item);
		}
	}
	else
	{
		// if have weapon from previous level session, activate it
		if ( t.lastgunid > 0 ) 
		{
			g.autoloadgun = t.lastgunid;
			t.lastgunid = 0;
		}
	}

	// and only use this flag once per game
	t.game.levelplrstatsetup = 0;

	//  OpenFileMap (  for IDE access )
	if (  t.plrfilemapaccess == 0 && t.game.gameisexe == 0 ) 
	{
		OpenFileMap (  11, "FPSEXCHANGE" );
		SetEventAndWait (  11 );
		t.plrfilemapaccess=1;
	}
}

void physics_player_free ( void )
{
	if (  t.plrfilemapaccess == 1 ) 
	{
		t.plrfilemapaccess=0;
	}
}

//Dave Performance
int physics_player_listener_delay = 0;
void physics_player ( void )
{
	if ( t.game.runasmultiplayer == 0 || g.mp.noplayermovement == 0 ) 
	{
		if ( t.aisystem.processplayerlogic == 1 ) 
		{
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling physics_player_gatherkeycontrols");
			physics_player_gatherkeycontrols ( );
			gun_update_hud ( );
			if ( ++physics_player_listener_delay > 3 )
			{
				physics_player_listener_delay = 0;
				if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling physics_player_listener");
				physics_player_listener ( );
			}
		}
		else
		{
			// prevent player physics movement
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling ODEControlDynamicCharacterController");
			ODEControlDynamicCharacterController ( t.aisystem.objectstartindex, 0, 0, 0, 0, t.aisystem.playerducking, 0, 0, 0 );

			//PE: Make sure to rotate set camera to make shadows work in freezemode.
			if (t.freezeplayerposonly == 0)
				RotateObject(t.aisystem.objectstartindex, t.terrain.playerax_f, t.terrain.playeray_f, t.terrain.playeraz_f);
			if (g.luacameraoverride != 2 && g.luacameraoverride != 3)
			{
				// lee - 140820 - when this flag is set with Q&A script, terrain suddenly gets shadow!
				if (t.freezeplayerposonly == 0) 
					RotateCamera(0, t.terrain.playerax_f, t.terrain.playeray_f, t.terrain.playeraz_f);
				else
					RotateCamera(0, t.freezeplayerax, t.freezeplayeray, t.freezeplayeraz);
			}

		}
	}
}

