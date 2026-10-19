//----------------------------------------------------
//--- GAMEGURU - M-Game
//----------------------------------------------------

#include "stdafx.h"
#include "gameguru.h"
#include "cOccluderThread.h"
#include "CGfxC.h"
#include "DarkLUA.h"
#include <algorithm>
#include <string>
#include <fstream>
#include <iterator>
#include "master.h"

//PE: GameGuru IMGUI.
#include "..\Imgui\imgui.h"
#include "..\Imgui\imgui_impl_win32.h"
#include "..\Imgui\imgui_gg_dx11.h"

#include ".\\..\..\\Guru-WickedMAX\\GPUParticles.h"
using namespace GPUParticles;
#include "GGTerrain\GGTerrain.h"
#include "GGTerrain\GGTrees.h"
using namespace GGTerrain;
using namespace GGTrees;
#include "GGRecastDetour.h"
GGRecastDetour g_RecastDetour;
bool g_bShowRecastDetourDebugVisuals = false;
int old_render_params2 = 0;

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

extern int g_Storyboard_First_Level_Node;
extern int g_Storyboard_Current_Level;
extern bool g_Storyboard_Starting_New_Level;
extern char g_Storyboard_First_fpm[256];
extern char g_Storyboard_Current_fpm[256];
extern char g_Storyboard_Current_lua[256];
extern char g_Storyboard_Current_Loading_Page[256];
extern StoryboardStruct Storyboard;
// Externs
extern bool g_occluderOn;
extern bool	g_occluderf9Mode;

extern bool g_bInTutorialMode;

// Globals
bool g_bInEditor = true;
int g_iMasterRootState = 0;
int g_iActivelyUsingVRNow = 0;
int g_iInGameMenuState = 0;
extern Master master;
bool g_bResetHasForLevelGeneration = false;

// 
//  Game Module to manage all game flow
// 

extern int iLaunchAfterSync;
extern bool commonexecutable_loop_for_game(void);

void gameexecutable_init(void)
{
	// start game init code
	int iEXEGameIsVR = 0;

	//PE: Load in any imgui media used in standalone, special mode tabtab...
	SetMipmapNum(1); //PE: mipmaps not needed.
	image_setlegacyimageloading(true);
	LoadImage("editors\\uiv3\\ccp-none.png", CCP_NONE);
	LoadImage("editors\\uiv3\\ccp-empty.png", CCP_EMPTY);
	image_setlegacyimageloading(false);
	SetMipmapNum(-1);

	extern bool bSpecialStandalone;
	if (bSpecialStandalone)
	{
		//PE: No VR when running demo games.
		editor_previewmap_initcode(0);
	}
	else
	{
		editor_previewmap_initcode(iEXEGameIsVR);
	}
	iLaunchAfterSync = 201;
}

void gameexecutable_loop(void)
{
	// loop with special modes used when in test game or standalone game
	commonexecutable_loop_for_game();
}

void gameexecutable_finish(void)
{
	// Free before exit app
	mp_free ( );
}

// The occluder thread
cOccluderThread*	g_pOccluderThread = NULL;
float				g_fOccluderCamVelX = 0.0f;
float				g_fOccluderCamVelZ = 0.0f;
float				g_fOccluderLastCamX = 0.0f;
float				g_fOccluderLastCamZ = 0.0f;

void game_scanfornewavatars ( bool bDynamicallyRecreateCharacters )
{
	// add any character creator player avatars in
	if ( t.bTriggerAvatarRescanAndLoad == true )
	{
		for ( t.tcustomAvatarCount = 0 ; t.tcustomAvatarCount <= MP_MAX_NUMBER_OF_PLAYERS-1; t.tcustomAvatarCount++ )
		{
			// check if there is a custom avatar (and not loaded)
			if ( t.mp_playerAvatars_s[t.tcustomAvatarCount] != "" && t.mp_playerAvatarLoaded[t.tcustomAvatarCount] == false ) 
			{
				// there is so lets built a temp fpe file from it
				t.ent_s = g.rootdir_s+"entitybank\\user\\charactercreatorplus\\customAvatar_"+Str(t.tcustomAvatarCount)+".fpe";
				t.avatarFile_s = t.ent_s;
				t.avatarString_s = t.mp_playerAvatars_s[t.tcustomAvatarCount];
				characterkitplus_makeMultiplayerCharacterCreatorAvatar ( );
				entity_addtoselection_core ( );
				characterkitplus_removeMultiplayerCharacterCreatorAvatar ( );
				t.tubindex[t.tcustomAvatarCount+2]=t.entid;
				t.entityprofile[t.tubindex[t.tcustomAvatarCount+2]].ischaracter=0;
				t.entityprofile[t.tubindex[t.tcustomAvatarCount+2]].collisionmode=12;
				// No lua script for player chars
				t.entityprofile[t.tubindex[t.tcustomAvatarCount+2]].aimain_s = "";
				// avatar is now loaded
				t.mp_playerAvatarLoaded[t.tcustomAvatarCount] = true;

				// additionally, when triggered, replace actual objects with new created ones above (for dynamic loading)
				if ( bDynamicallyRecreateCharacters == true )
				{
					t.e = t.mp_playerEntityID[t.tcustomAvatarCount];
					if ( t.e > 0 )
					{
						// update entity element with new character object (dynamically loaded during game)
						t.entityelement[t.e].bankindex = t.entid;

						// update entity object itself
						t.tupdatee = t.e; 
						entity_updateentityobj ( );
					}
				}
			}
		}
		t.bTriggerAvatarRescanAndLoad = false;

		// refreshes object masks of avatar heads
		t.visuals.refreshshaders = 1;
	}
}

#ifdef WIP_PROLOADLEVELTEXTURES
std::vector<std::string> preload_setup;
#endif


uint32_t GetObjectNavMeshVertexCount( int iID )
{
	if ( !ConfirmObject ( iID ) ) return 0;

	sObject* pObject = g_ObjectList [ iID ];

	uint32_t numTotalVertices = 0;
	
	for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
	{
		sFrame* pFrame = pObject->ppFrameList[iFrameIndex];
		if (pFrame)
		{
			sMesh* pMesh = pFrame->pMesh;
			if (pMesh)
			{
				if (pMesh->dwIndexCount == 0)
				{
					numTotalVertices += pMesh->dwVertexCount;
				}
				else
				{
					// has index data
					numTotalVertices += pMesh->dwIndexCount;
				}
			}
		}
	}

	return numTotalVertices;
}

void GetObjectNavMeshVertices( int iID, float* pVertices )
{
	if ( !ConfirmObject ( iID ) ) return;

	sObject* pObject = g_ObjectList [ iID ];

	uint32_t numTotalVertices = 0;

	// faces
	for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
	{
		sFrame* pFrame = pObject->ppFrameList[iFrameIndex];
		if (pFrame)
		{
			sMesh* pMesh = pFrame->pMesh;
			if (pMesh)
			{			
				GGMATRIX matThisFrame;
				GGMatrixTranslation	(&matThisFrame, pFrame->vecOffset.x, pFrame->vecOffset.y, pFrame->vecOffset.z);
				
				float* pVertData = (float*) pMesh->pVertexData;
				uint32_t stride = pMesh->dwFVFSize / 4;

				if (pMesh->dwIndexCount == 0)
				{
					// has no indice data
					
					for (DWORD dwV = 0; dwV < pMesh->dwVertexCount; dwV ++)
					{
						GGVECTOR3 vecXYZ = GGVECTOR3( pVertData[0], pVertData[1], pVertData[2] );
						GGVec3TransformCoord(&vecXYZ, &vecXYZ, &matThisFrame);

						uint32_t index = (numTotalVertices + dwV) * 3;

						pVertices[ index + 0 ] = vecXYZ.x;
						pVertices[ index + 1 ] = vecXYZ.y;
						pVertices[ index + 2 ] = vecXYZ.z;

						pVertData += stride;
					}

					numTotalVertices += pMesh->dwVertexCount;
				}
				else
				{
					// has indice data
					for (DWORD dwI = 0; dwI < pMesh->dwIndexCount; dwI++ )
					{
						DWORD dwV = pMesh->pIndices[ dwI ];
						
						uint32_t vIndex = dwV * stride;
						GGVECTOR3 vecXYZ = GGVECTOR3( pVertData[vIndex], pVertData[vIndex + 1], pVertData[vIndex + 2] );
						GGVec3TransformCoord(&vecXYZ, &vecXYZ, &matThisFrame);

						uint32_t index = (numTotalVertices + dwI) * 3;

						pVertices[ index + 0 ] = vecXYZ.x;
						pVertices[ index + 1 ] = vecXYZ.y;
						pVertices[ index + 2 ] = vecXYZ.z;
					}

					numTotalVertices += pMesh->dwIndexCount;
				}
			}
		}
	}
}

//PE: save raw vertices.
bool saveVertices(const std::string& filename, float* pVertices, int numVertices)
{

	std::ofstream outFile(filename, std::ios::binary);

	if (!outFile)
	{
		return false;
	}

	outFile.write(reinterpret_cast<const char*>(&numVertices), sizeof(int));

	outFile.write(reinterpret_cast<const char*>(pVertices), sizeof(float) * numVertices * 3);

	outFile.close();

	return true;
}

//PE: Load raw vertices.
float* loadVertices(const std::string& filename, int& outNumVertices)
{
	std::ifstream inFile(filename, std::ios::binary);

	if (!inFile)
	{
		return nullptr;
	}

	inFile.read(reinterpret_cast<char*>(&outNumVertices), sizeof(int));

	float* pVertices = new float[outNumVertices * 3];

	inFile.read(reinterpret_cast<char*>(pVertices), sizeof(float) * outNumVertices * 3);

	inFile.close();

	return pVertices;
}

void game_createnavmeshfromlevel ( bool bForceGeneration )
{
	bool bStandalone = bForceGeneration;

	if (bStandalone)
	{
		//PE: Check if we got the raw pVertices size numVertices,  under fpm name.
		cstr importer_getfilenameonly(LPSTR pFileAndPossiblePath);
		cstr filenameonly = importer_getfilenameonly(g.projectfilename_s.Get());
		std::string newfilename = filenameonly.Get();
		replaceAll(newfilename, ".fpm", ".raw");
		DARKSDK LPSTR GetDir(void);
		//PE: Save into standalone folder.
		std::string fullname = (char *) GetDir();
		fullname = fullname + "\\navbank\\";
		CreateDirectoryA(fullname.c_str(), NULL);
		fullname = fullname + newfilename;

		int loadedCount = 0;
		float* pLoadedVertices = loadVertices(fullname, loadedCount);

		if (pLoadedVertices && loadedCount > 0)
		{
			timestampactivity(0, "Using cached vertices to build the nav mesh");
			g_RecastDetour.buildall(pLoadedVertices, loadedCount);
			if (pLoadedVertices) delete[] pLoadedVertices;
			return;
		}
	}
	// area around any entities on map
	GGVECTOR3 vecMinArea = GGVECTOR3( 999999,  999999,  999999);
	GGVECTOR3 vecMaxArea = GGVECTOR3(-999999, -999999, -999999);
	float editableSize = GGTerrain_GetEditableSize();

	// find any NAVMESH LIMIT flags
	bool bUsingNavMeshLimitCustomArea = false;
	GGVECTOR3 vecCustomPlayAreaMin = GGVECTOR3(999999, 999999, 999999);
	GGVECTOR3 vecCustomPlayAreaMax = GGVECTOR3(-999999, -999999, -999999);
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		int entid = t.entityelement[e].bankindex;
		if (t.entityprofile[entid].ismarker == 11 )
		{
			if (stricmp(t.entityelement[e].eleprof.name_s.Get(), "navmesh limit") == NULL)
			{
				if (t.entityelement[e].x > vecCustomPlayAreaMax.x) vecCustomPlayAreaMax.x = t.entityelement[e].x;
				if (t.entityelement[e].z > vecCustomPlayAreaMax.z) vecCustomPlayAreaMax.z = t.entityelement[e].z;
				if (t.entityelement[e].x < vecCustomPlayAreaMin.x) vecCustomPlayAreaMin.x = t.entityelement[e].x;
				if (t.entityelement[e].z < vecCustomPlayAreaMin.z) vecCustomPlayAreaMin.z = t.entityelement[e].z;
				bUsingNavMeshLimitCustomArea = true;
			}
		}
	}

	// work out hash for all "static" objects
	double dSuperHash = 0;
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		int entid = t.entityelement[e].bankindex;

		// build the bounding box using both static objects and characters, in case there are no static objects near a character
		bool bNeedThisToHaveNavMesh = false;
		if ( t.entityelement[e].staticflag == 1) bNeedThisToHaveNavMesh = true;
		if ( entid > 0 && t.entityprofile[entid].navmeshbake == 1) bNeedThisToHaveNavMesh = true;
		if ( t.entityprofile[entid].ischaracter == 1) bNeedThisToHaveNavMesh = true;
		if ( t.entityprofile[entid].ismarker == 11) bNeedThisToHaveNavMesh = true;
		if ( bNeedThisToHaveNavMesh == true)
		{
			int iObj = t.entityelement[e].obj;
			if (iObj > 0)
			{
				if (ObjectExist(iObj) == 1)
				{
					// establish bounds of static objects
					GGVECTOR3 vecPos = GGVECTOR3(ObjectPositionX(iObj), ObjectPositionY(iObj), ObjectPositionZ(iObj));
					if ( vecPos.x > editableSize || vecPos.x < -editableSize || vecPos.z > editableSize || vecPos.z < -editableSize ) continue;

					// extra feature called NAVMESH LIMIT (assign this name to a flag and place)
					// which enables the nav mesh area to be customized, allowing objects outside to be placed for scenery
					if (bUsingNavMeshLimitCustomArea == true)
					{
						if( vecPos.x < vecCustomPlayAreaMin.x ||
							vecPos.x > vecCustomPlayAreaMax.x ||
							vecPos.z < vecCustomPlayAreaMin.z ||
							vecPos.z > vecCustomPlayAreaMax.z )
						{
							// this object outside of custom navmesh limit area, can ignore (done again below)
							continue;
						}
					}

					if (vecPos.x > vecMaxArea.x) vecMaxArea.x = vecPos.x;
					if (vecPos.z > vecMaxArea.z) vecMaxArea.z = vecPos.z;
					if (vecPos.x < vecMinArea.x) vecMinArea.x = vecPos.x;
					if (vecPos.z < vecMinArea.z) vecMinArea.z = vecPos.z;

					// calculating superhash
					dSuperHash += iObj;
					dSuperHash += vecPos.x;
					dSuperHash += vecPos.y;
					dSuperHash += vecPos.z;
					dSuperHash += ObjectAngleX(iObj);
					dSuperHash += ObjectAngleY(iObj);
					dSuperHash += ObjectAngleZ(iObj);
					dSuperHash += ObjectScaleX(iObj);
					dSuperHash += ObjectScaleY(iObj);
					dSuperHash += ObjectScaleZ(iObj);
				}
			}
		}
	}
		
	if (dSuperHash == 0)
	{
		// failing all else, just use camera position
		vecMinArea.x = vecMaxArea.x = CameraPositionX(0);
		vecMinArea.z = vecMaxArea.z = CameraPositionZ(0);
	}

	// toggling trees means recalcing for tree obstacles
	dSuperHash += ggtrees_global_params.draw_enabled;

	// toggling trees means recalcing for tree obstacles
	dSuperHash += (int)t.terrain.waterliney_f;

	// must still reset nav mesh system for new run (blocker list)
	g_RecastDetour.ResetBlockerSystem();
	g_RecastDetour.ResetTokenDropSystem();
	g_RecastDetour.SetWaterTableY(t.terrain.waterliney_f);

	// exit early if no change detected in static arrangement
	static double dLastSuperHash = -1;
	if(g_bResetHasForLevelGeneration == true)
	{
		dLastSuperHash = -1;
		bForceGeneration = true;
		g_bResetHasForLevelGeneration = false;
	}
	if (bForceGeneration == false)
	{
		if (dLastSuperHash != -1 && dSuperHash == dLastSuperHash) return;
	}
	dLastSuperHash = dSuperHash;

	// create monster object representing level geometry
	int iBuildAllLevelMesh = g.meshgeneralwork;
	int iBuildAllLevelObj = g.tempobjectoffset + 0;
	if (ObjectExist(iBuildAllLevelObj) == 1) DeleteObject(iBuildAllLevelObj);

	// always expand bounding box in case characters move around
	vecMinArea.x -= 1000;
	vecMaxArea.x += 1000;
	vecMinArea.z -= 1000;
	vecMaxArea.z += 1000;

	// reuse navmeshlimiter to create a tiny navmesh nothing could possibly use (but keep navmesh calls working)
	bool bIgnoreStaticStuff = false;
	if (t.visuals.bEnableZeroNavMeshMode == true)
	{
		vecMinArea.x = -51050;
		vecMaxArea.x = -51000;
		vecMinArea.z = -51050;
		vecMaxArea.z = -51000;
		vecCustomPlayAreaMin.x = -51050;
		vecCustomPlayAreaMax.x = -51000;
		vecCustomPlayAreaMin.z = -51050;
		vecCustomPlayAreaMax.z = -51000;
		bIgnoreStaticStuff = true;
	}

	// terrain geometry
	int iLimbIndex = 1;
	int iTerrainObj = iBuildAllLevelObj;
	bool bCreateTerrainMesh = true;
	if (bCreateTerrainMesh == true )
	{
		// generate polygons for required area
		int iFirstLOD = 2;
		GGVECTOR3* pvecVerts = NULL;
		int iTerrainFloorVertexCount = GGTerrain_GetTriangleList(&pvecVerts, vecMinArea.x, vecMinArea.z, vecMaxArea.x, vecMaxArea.z, iFirstLOD);
		if (iTerrainFloorVertexCount > 0 )
		{
			// divide up into 16-bit size meshes
			int iVertStart =  0;
			int iPiecesCount = iTerrainFloorVertexCount / 65535;
			for (int iPieces = 0; iPieces <= iPiecesCount; iPieces++)
			{
				// create a mesh from polygons
				int newobj = g.tempobjectoffset + 1;
				int iTerrainNavMeshObj = newobj;
				if (iPieces == 0) iTerrainNavMeshObj = iBuildAllLevelObj;
				if (ObjectExist(iTerrainNavMeshObj) == 1) DeleteObject (iTerrainNavMeshObj);
				MakeObjectPlane (iTerrainNavMeshObj, 1, 1);
				sObject* pObject = GetObjectData(iTerrainNavMeshObj);
				delete pObject->ppMeshList[0];
				sMesh* pMesh = new sMesh();

				// full piece size or remaining
				DWORD dwVertexCount = iTerrainFloorVertexCount - iVertStart;
				if (dwVertexCount > 65535) dwVertexCount = 65535;

				// resize object for this
				DWORD dwIndexCount = dwVertexCount;
				SetupMeshFVFData (pMesh, GGFVF_XYZ, dwVertexCount, dwIndexCount, false);
				pMesh->iPrimitiveType = GGPT_TRIANGLELIST;
				pMesh->iDrawVertexCount = pMesh->dwVertexCount;
				pMesh->iDrawPrimitives = dwIndexCount / 3;
				pObject->ppMeshList[0] = pMesh;
				pObject->pFrame->pMesh = pMesh;
				float* pVertPtr = (float*)pMesh->pVertexData;
				for (int v = 0; v < dwVertexCount; v++)
				{
					*(pVertPtr + 0) = pvecVerts[iVertStart + v].x;
					*(pVertPtr + 1) = pvecVerts[iVertStart + v].y;
					*(pVertPtr + 2) = pvecVerts[iVertStart + v].z;
					pVertPtr += 3;
				}
				WORD* pIndicePtr = (WORD*)pMesh->pIndices;
				for (int i = 0; i < dwIndexCount; i++)
				{
					*(pIndicePtr) = i;
					pIndicePtr++;
				}

				// for next piece (if any)
				iVertStart += 65535;

				// add this terrain piece as mesh to main OBJ
				if (iPieces > 0)
				{
					// after iniutial creation, add new meshes
					MakeMeshFromObject(iBuildAllLevelMesh, iTerrainNavMeshObj);
					AddLimb(iBuildAllLevelObj, iLimbIndex, iBuildAllLevelMesh);
					iLimbIndex++;

					// remove tempo mesh/obj
					if (ObjectExist(iTerrainNavMeshObj) == 1) DeleteObject (iTerrainNavMeshObj);
				}
			}

			// ensure main OIBJ is hidden during process
			HideObject(iBuildAllLevelObj);

			// free when complete
			if (pvecVerts)
			{
				delete pvecVerts;
				pvecVerts = NULL;
			}
		}
		else
		{
			// inexplicavble that any part of the terrain measuring 1000x1000 produces NO polys!!
			MakeObjectPlane (iBuildAllLevelObj, 1, 1);
			HideObject(iBuildAllLevelObj);
		}
	}
	else
	{
		MakeObjectPlane (iBuildAllLevelObj, 1, 1);
		HideObject(iBuildAllLevelObj);
	}

	// all static objects in level
	if (bIgnoreStaticStuff == false)
	{
		for (int e = 1; e <= g.entityelementlist; e++)
		{
			if (t.entityelement[e].staticflag == 1)
			{
				int iObj = t.entityelement[e].obj;
				int iBankindex = t.entityelement[e].bankindex;
				bool bValid = true;
				if (t.entityprofile[iBankindex].ismarker != 0) bValid = false;
				if (t.entityprofile[iBankindex].collisionmode == 11) bValid = false;
				if (t.entityprofile[iBankindex].collisionmode == 12) bValid = false;
				if (t.entityprofile[iBankindex].isammo == 1) bValid = false;
				if (t.entityprofile[iBankindex].isweapon_s.Len() > 1) bValid = false;

				if (bValid && iObj > 0 && ObjectExist(iObj) == 1)
				{
					// get position of static obstruction
					GGVECTOR3 vecPos = GGVECTOR3(ObjectPositionX(iObj), ObjectPositionY(iObj), ObjectPositionZ(iObj));
					if (vecPos.x > editableSize || vecPos.x < -editableSize || vecPos.z > editableSize || vecPos.z < -editableSize) continue;

					// also reject if object is moved to a non-visible position
					if (vecPos.y <= -50000) continue;

					// extra feature called NAVMESH LIMIT
					if (bUsingNavMeshLimitCustomArea == true)
					{
						if (vecPos.x < vecCustomPlayAreaMin.x ||
							vecPos.x > vecCustomPlayAreaMax.x ||
							vecPos.z < vecCustomPlayAreaMin.z ||
							vecPos.z > vecCustomPlayAreaMax.z)
						{
							// this object outside of custom navmesh limit area, can ignore
							continue;
						}
					}

					//PE: Add physics shapes here.
					if (GetMeshExist(iBuildAllLevelMesh) == 1) DeleteMesh(iBuildAllLevelMesh);
					if (iBankindex > 0 && t.entityprofile[iBankindex].collisionmode >= 50 && t.entityprofile[iBankindex].collisionmode < 60)
					{
						int newobj = g.tempobjectoffset + 1;
						if (ObjectExist(newobj)) DeleteObject(newobj);
						MakeObjectCylinder(newobj, 1);

						t.tSizeY_f = ObjectSizeY(iObj, 1);

						//  if have ABS position from AI OBSTACLE calc, use that instead
						if (t.entityelement[e].abscolx_f != -1)
						{
							t.tFinalX_f = t.entityelement[e].abscolx_f;
							t.tFinalZ_f = t.entityelement[e].abscolz_f;
						}
						else
						{
							t.tFinalX_f = ObjectPositionX(iObj);
							t.tFinalZ_f = ObjectPositionZ(iObj);
						}
						t.tFinalY_f = ObjectPositionY(iObj) + (t.tSizeY_f / 2.0);

						//  if have ABS radius from AI OBSTACLE calc, use that instead
						if (t.entityelement[e].abscolradius_f != -1)
						{
							t.tSizeX_f = t.entityelement[e].abscolradius_f;
							t.tSizeZ_f = t.entityelement[e].abscolradius_f;
						}
						else
						{
							t.tSizeX_f = 20;
							t.tSizeZ_f = 20;
						}

						//  increase size by 25%
						t.tSizeX_f = t.tSizeX_f * 1.25;
						t.tSizeZ_f = t.tSizeZ_f * 1.25;

						ScaleObject(newobj, t.tSizeX_f * 100, t.tSizeY_f * 100, t.tSizeZ_f * 100);
						MakeMeshFromObject(iBuildAllLevelMesh, newobj);
						AddLimb(iBuildAllLevelObj, iLimbIndex, iBuildAllLevelMesh);
						OffsetLimb(iBuildAllLevelObj, iLimbIndex, ObjectPositionX(iObj), ObjectPositionY(iObj) + (t.tSizeY_f * 0.5), ObjectPositionZ(iObj));
						iLimbIndex++;
					}
					else
					{
						// to make a cleaner NAVMESH, stairs are better as ramps, so use OBJ if present for this purpose
						// if object uses convex hull, see if there is an OBJ we can swap inplace of the objects full mesh
						int iObjToUseForNavMesh = iObj;
						bool bHeavyPOlyShapesShouldCheckForOBJ = false;
						if (t.entityprofile[iBankindex].collisionmode == 1) bHeavyPOlyShapesShouldCheckForOBJ = true; // polygon
						if (t.entityprofile[iBankindex].collisionmode == 8) bHeavyPOlyShapesShouldCheckForOBJ = true; // polygon with OBJ
						if (t.entityprofile[iBankindex].collisionmode == 9) bHeavyPOlyShapesShouldCheckForOBJ = true; // convex hull
						if (t.entityprofile[iBankindex].collisionmode == 10) bHeavyPOlyShapesShouldCheckForOBJ = true; // hull decomp
						if (bHeavyPOlyShapesShouldCheckForOBJ == true)
						{
							char pNoFPE[MAX_PATH];
							strcpy(pNoFPE, t.entitybank_s[iBankindex].Get());
							pNoFPE[strlen(pNoFPE) - 4] = 0;
							char pOBJCollisionMesh[MAX_PATH];
							sprintf(pOBJCollisionMesh, "%s\\Files\\entitybank\\%s.obj", g.fpscrootdir_s.Get(), pNoFPE);
							GG_GetRealPath(pOBJCollisionMesh, 0);
							if (FileExist(pOBJCollisionMesh) == 0)
							{
								sprintf(pOBJCollisionMesh, "%s\\Files\\entitybank\\%s_COL.obj", g.fpscrootdir_s.Get(), pNoFPE);
								GG_GetRealPath(pOBJCollisionMesh, 0);
							}
							if (FileExist(pOBJCollisionMesh) == 1)
							{
								// can optimize this by keeping the low poly OBJ, perhaps add to DBO as a LOD??
								if (ObjectExist(g.temp2objectoffset) == 1) DeleteObject(g.temp2objectoffset);
								LoadObject (pOBJCollisionMesh, g.temp2objectoffset);
								iObjToUseForNavMesh = g.temp2objectoffset;
								RotateObject(iObjToUseForNavMesh, ObjectAngleX(iObj), ObjectAngleY(iObj), ObjectAngleZ(iObj));
								ScaleObject(iObjToUseForNavMesh, ObjectScaleX(iObj), ObjectScaleY(iObj), ObjectScaleZ(iObj));
							}
						}
						// regular mesh from object
						MakeMeshFromObject(iBuildAllLevelMesh, iObjToUseForNavMesh);
						AddLimb(iBuildAllLevelObj, iLimbIndex, iBuildAllLevelMesh);
						OffsetLimb(iBuildAllLevelObj, iLimbIndex, ObjectPositionX(iObj), ObjectPositionY(iObj), ObjectPositionZ(iObj));
						iLimbIndex++;
					}
				}
			}
		}
	}

	// simple obstacle object/mesh to punch into navmesh
	if (ObjectExist(g.temp2objectoffset)) DeleteObject(g.temp2objectoffset); //PE: Got 7007 if any tree cylinder are created.
	MakeObjectBox (g.temp2objectoffset, 10, 200, 10);
	MakeMeshFromObject (g.meshgeneralwork2, g.temp2objectoffset);
	DeleteObject(g.temp2objectoffset);

	// solve issue of large delay when nothing in scene (hack for now, need to find real reason)
	if (iLimbIndex == 1)
	{
		AddLimb(iBuildAllLevelObj, iLimbIndex, g.meshgeneralwork2);
		OffsetLimb(iBuildAllLevelObj, iLimbIndex, 0 - 800, 0 - 5, 0 - 800);
		iLimbIndex++;
	}

	// add virtual trees into the navmesh (quick solution to code, very slow to execute!!)
	bool bOldMethodOfAddingTreesToNavMesh = false;
	if (bOldMethodOfAddingTreesToNavMesh == true)
	{
		if (ggtrees_global_params.draw_enabled == 1)
		{
			timestampactivity(0, "Adding all trees to temporary nav mesh super object");
			float fPlayAreaRadiusX = (vecMaxArea.x - vecMinArea.x) / 2;
			float fPlayAreaRadiusZ = (vecMaxArea.z - vecMinArea.z) / 2;
			float fPlayAreaCenterX = vecMinArea.x + fPlayAreaRadiusX;
			float fPlayAreaCenterZ = vecMinArea.z + fPlayAreaRadiusZ;
			float fPlayAreaRadius = fPlayAreaRadiusX;
			if (fPlayAreaRadiusZ > fPlayAreaRadius) fPlayAreaRadius = fPlayAreaRadiusZ;
			GGTrees::GGTreePoint* pOutPoints = NULL;
			int iTreeCount = GGTrees::GGTrees_GetClosest (fPlayAreaCenterX, fPlayAreaCenterZ, fPlayAreaRadius, &pOutPoints);
			if (pOutPoints)
			{
				for (int n = 0; n < iTreeCount; n++)
				{
					GGVECTOR3 vecTreePos = GGVECTOR3(pOutPoints[n].x, pOutPoints[n].y, pOutPoints[n].z);
					AddLimb(iBuildAllLevelObj, iLimbIndex, g.meshgeneralwork2);
					OffsetLimb(iBuildAllLevelObj, iLimbIndex, vecTreePos.x, vecTreePos.y, vecTreePos.z);
					iLimbIndex++;
				}
				delete pOutPoints;
			}
		}
	}

	// Generating raw nav mesh vertices for build
	timestampactivity(0, "Generating raw nav mesh vertices for build");
	float* pRawVertices = 0;
	uint32_t numRawVertices = 0;
	const bool saveObject = false;
	if ( saveObject )
	{
		// save level geometry object as OBJ
		char pProgressStr[256];
		sprintf_s(pProgressStr, 256, "SAVING NAV MESH TOPOGRAPHY - %d\\100 Complete", 11);
		void printscreenprompt(char*);
		printscreenprompt(pProgressStr);
		LPSTR pAllLevelGeometryFilename = "levelbank\\testmap\\rawlevelgeometry.obj";
		if (FileExist(pAllLevelGeometryFilename)) DeleteFileA(pAllLevelGeometryFilename);
		SaveObjectEx(pAllLevelGeometryFilename, iBuildAllLevelObj, true); //PE: Use 8% of the time optimize it.
	}
	else
	{
		// get raw vertex soup from super object
		char pProgressStr[256];
		sprintf_s(pProgressStr, 256, "GENERATING NAV MESH VERTICES - %d\\100 Complete", 11);
		void printscreenprompt(char*);
		printscreenprompt(pProgressStr);
		numRawVertices = GetObjectNavMeshVertexCount( iBuildAllLevelObj );
		pRawVertices = new float[numRawVertices *3 ];
		GetObjectNavMeshVertices( iBuildAllLevelObj, pRawVertices);
	}

	// new method of adding trees, direct to the vertex soup
	float* pVertices = pRawVertices;
	uint32_t numVertices = numRawVertices;
	if (bIgnoreStaticStuff == false)
	{
		if (bOldMethodOfAddingTreesToNavMesh == false)
		{
			if (ggtrees_global_params.draw_enabled == 1)
			{
				timestampactivity(0, "Adding all trees to post raw vertex soup (quicker)");
				float fPlayAreaRadiusX = (vecMaxArea.x - vecMinArea.x) / 2;
				float fPlayAreaRadiusZ = (vecMaxArea.z - vecMinArea.z) / 2;
				float fPlayAreaCenterX = vecMinArea.x + fPlayAreaRadiusX;
				float fPlayAreaCenterZ = vecMinArea.z + fPlayAreaRadiusZ;
				float fPlayAreaRadius = fPlayAreaRadiusX;
				if (fPlayAreaRadiusZ > fPlayAreaRadius) fPlayAreaRadius = fPlayAreaRadiusZ;
				GGTrees::GGTreePoint* pOutPoints = NULL;
				int iTreeCount = GGTrees::GGTrees_GetClosest (fPlayAreaCenterX, fPlayAreaCenterZ, fPlayAreaRadius, &pOutPoints);
				if (pOutPoints)
				{
					// reserve space for larger vertex soup
					numVertices = numRawVertices + (iTreeCount * 6 * 6);
					pVertices = new float[numVertices * 3];
					memcpy (pVertices, pRawVertices, sizeof(float) * numRawVertices * 3);

					// for each tree, add a cube to vertex data
					uint32_t numCurrentVertices = numRawVertices;
					for (int n = 0; n < iTreeCount; n++)
					{
						GGVECTOR3 vecTreePos = GGVECTOR3(pOutPoints[n].x, pOutPoints[n].y, pOutPoints[n].z);
						sMesh* pTreeCubeShape = g_RawMeshList[g.meshgeneralwork2];
						if (pTreeCubeShape)
						{
							float* pVertData = (float*)pTreeCubeShape->pVertexData;
							uint32_t stride = pTreeCubeShape->dwFVFSize / 4;
							for (DWORD dwI = 0; dwI < pTreeCubeShape->dwIndexCount; dwI++)
							{
								DWORD dwV = pTreeCubeShape->pIndices[dwI];
								uint32_t vIndex = dwV * stride;
								GGVECTOR3 vecXYZ = GGVECTOR3(pVertData[vIndex], pVertData[vIndex + 1], pVertData[vIndex + 2]);
								uint32_t index = (numCurrentVertices + dwI) * 3;
								pVertices[index + 0] = vecTreePos.x + vecXYZ.x;
								pVertices[index + 1] = vecTreePos.y + vecXYZ.y;
								pVertices[index + 2] = vecTreePos.z + vecXYZ.z;
							}
							numCurrentVertices += pTreeCubeShape->dwIndexCount;
						}
					}
					delete pOutPoints;

					// remove old vert soup in favor of new one
					if (pRawVertices) delete[] pRawVertices;
				}
			}
		}

		// final step to send all Y coords under waterline to depths to avoid paths under water (and character walking into drowning)
		for (DWORD dwV = 0; dwV < numVertices; dwV++)
		{
			uint32_t index = dwV * 3;
			float fY = pVertices[index + 1];
			if (fY < t.terrain.waterliney_f)
			{
				// sink this vertex to the depths if underwater, cannot make path from this!
				pVertices[index + 1] = -9999;
			}
		}
	}

	// can now delete massive object
	timestampactivity(0, "Delete temporary nav mesh super object");
	DeleteObject(iBuildAllLevelObj);

	// generate nav mesh using recast on OBJ
	timestampactivity(0, "Using recast to build the nav mesh");

	if (bStandalone)
	{
		//PE: Save raw pVertices size numVertices for standalone, save under fpm name.
		cstr importer_getfilenameonly(LPSTR pFileAndPossiblePath);
		cstr filenameonly = importer_getfilenameonly(g.projectfilename_s.Get());
		std::string newfilename = filenameonly.Get();
		replaceAll(newfilename, ".fpm", ".raw");
		DARKSDK LPSTR GetDir(void);
		//PE: Save into standalone folder.
		std::string fullname = (char*)GetDir();
		fullname = fullname + "\\navbank\\";
		CreateDirectoryA(fullname.c_str(), NULL);
		fullname = fullname + newfilename;
		if (saveVertices(fullname, pVertices, numVertices))
		{
			//PE: Success.
		}
	}

	g_RecastDetour.buildall( pVertices, numVertices );

	// remove vert soup
	if ( pVertices ) delete [] pVertices;
}

void game_updatenavmeshsystem(void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	// render any debug objects (such as nav mesh)
	if (g_bShowRecastDetourDebugVisuals == true)
		g_RecastDetour.handleDebugRender();
	else
		g_RecastDetour.cleanupDebugRender();
}

