//#pragma optimize("", off)

//----------------------------------------------------
//--- GAMEGURU - M-Entity
//----------------------------------------------------

#include "stdafx.h"
#include "gameguru.h"

// OPTICK Performance
#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#include "..\Imgui\imgui.h"
#include "..\Imgui\imgui_impl_win32.h"
#include "..\Imgui\imgui_gg_dx11.h"

#include ".\\..\..\\Guru-WickedMAX\\GPUParticles.h"
using namespace GPUParticles;
#include "GGTerrain\GGTerrain.h"
using namespace GGTerrain;

#include "..\..\..\..\Guru-WickedMAX\wickedcalls.h"
#include "WickedEngine.h"
using namespace std;
using namespace wiGraphics;
using namespace wiScene;
using namespace wiECS;

#define MAXREADYDECALS 5
#define MAXUNIQUEDECALS 100
uint32_t ready_decals[MAXUNIQUEDECALS][MAXREADYDECALS] = { 0 };
uint32_t decal_count[MAXUNIQUEDECALS] = { 0 };

// Globals for blacklist string array
LPSTR* g_pBlackList = NULL;
int g_iBlackListMax = 0;
bool g_bBlackListRemovedSomeEntities = false;

int g_iWickedEntityId = -1;
int g_iWickedElementId = 0;
int g_iWickedMeshNumber = 0;
bool g_bUseEditorGrideleprof = false;
int g_iAbortedAsEntityIsGroupFileMode = 0;
int g_iAbortedAsEntityIsGroupFileModeStubOnly = 0;
int g_iAbortedAsEntityIsGroupCreate = 0;
cstr g_sTempGroupForThumbnail = "";

float g_fFlattenMargin = 100.0f;

//. externs
extern std::vector<int> g_ObjectHighlightList;
#define MAXGROUPSLISTS 100 // duplicated in GridEdit.cpp (would replace this when the group list is dynamic)
extern std::vector<sRubberBandType> vEntityGroupList[MAXGROUPSLISTS];
extern cstr sEntityGroupListName[MAXGROUPSLISTS];
extern int GetGroupIndexFromName(cstr sLookFor);

// 
//  ENTITY COMMON CODE (not game specific)
// 

void entity_addtoselection_core ( void )
{
	//  ensure ENT$ does not contain duplicate \ symbols
	t.tnewent_s="";
	for ( t.n = 1 ; t.n<=  Len(t.ent_s.Get()); t.n++ )
	{
		t.tnewent_s=t.tnewent_s+Mid(t.ent_s.Get(),t.n);
		if ( cstr(Mid(t.ent_s.Get(),t.n)) == "\\" && cstr(Mid(t.ent_s.Get(),t.n+1)) == "\\" ) 
		{
			++t.n;
		}
	}
	t.ent_s=t.tnewent_s;

	// Load entity from file
	t.entdir_s="entitybank\\";
	t.ent_s=Right(t.ent_s.Get(),1+(Len(t.ent_s.Get())-(Len(g.rootdir_s.Get())+Len(t.entdir_s.Get()))));

	// Check if filename valid
	t.entnewloaded=0 ; t.entid=0;
	if ( cstr(Right(t.ent_s.Get(),4)) == ".fpe" ) 
	{
		// Check entity exists in bank
		t.tokay=1;
		if ( g.entidmaster>0 ) 
		{
			for ( t.entid = 1 ; t.entid<=  g.entidmaster; t.entid++ )
			{
				if ( t.entitybank_s[t.entid] == t.ent_s ) {  t.tokay = 0  ; t.tfoundid = t.entid ; break; }
			}
		}
		if ( t.tokay == 1 ) 
		{
			// Find Free entity Index
			t.freeentid=-1;
			if ( g.entidmaster>0 ) 
			{
				for ( t.entid = 1 ; t.entid <= g.entidmaster; t.entid++ )
				{
					if ( t.entityprofileheader[t.entid].desc_s == "" ) {  t.freeentid = t.entid  ; break; }
				}
			}

			//  New entity or Free One
			if ( t.freeentid == -1 ) 
			{
				++g.entidmaster ; entity_validatearraysize ( );
				t.entid=g.entidmaster;
				t.entnewloaded=1;
			}
			else
			{
				t.entid=t.freeentid;
			}

			// Load entity
			t.entitybank_s[t.entid]=t.ent_s;
			t.entpath_s=getpath(t.ent_s.Get());
			entity_load ( );
		}
		else
		{
			// already got, assign ID from existing
			t.entid=t.tfoundid;
		}
	}
}

void entity_addtoselection ( void )
{
	//  Load entity from file requester
	SetDir (  g.currententitydir_s.Get() );
	t.ent_s=browseropen_s(9);
	g.currententitydir_s=GetDir();
	SetDir (  g.rootdir_s.Get() );
	entity_addtoselection_core ( );
}

bool entity_copytoremoteifnotthere ( LPSTR pPathToFile )
{
	//PE: Bug fix , never copy in standalone.
	if (t.game.gameisexe == 1) return false;

	// this only works if destination folder already exists, use "mapfile_ensurethisfolderexistsinremoteproject" if want to copy whole folders!
	// if using remote project, first duplicate the entity file to local project
	bool bWeCopiedTheFileOver = false;
	extern StoryboardStruct Storyboard;
	char pPreferredProjectEntityFolder[MAX_PATH];
	strcpy(pPreferredProjectEntityFolder, Storyboard.customprojectfolder);
	strcat(pPreferredProjectEntityFolder, Storyboard.gamename);
	if (strlen(Storyboard.customprojectfolder) > 0)
	{
		// yes, a remote project
		char fullRealPath[MAX_PATH];
		strcpy(fullRealPath, pPathToFile);
		GG_GetRealPath(fullRealPath, 0);
		if (strnicmp (fullRealPath, pPreferredProjectEntityFolder, strlen(pPreferredProjectEntityFolder)) != NULL)
		{
			// file not in local project, copy it over
			strcpy(pPreferredProjectEntityFolder, pPathToFile);
			int ret = GG_GetRealPath(pPreferredProjectEntityFolder, 1);
			if( stricmp(pPreferredProjectEntityFolder, fullRealPath) != NULL )
				CopyFileA(fullRealPath, pPreferredProjectEntityFolder, TRUE);
			else if (ret == 0)
			{
				//PE: Remote project not found, use relative path.
				char Relative[MAX_PATH];
				const char* find = pestrcasestr(fullRealPath, "\\files\\");
				if (find)
				{
					strcpy(Relative, find + 7);
					int ret = GG_GetRealPath(Relative, 1);
					if (stricmp(Relative, fullRealPath) != NULL)
						CopyFileA(fullRealPath, Relative, TRUE);
				}
			}

			bWeCopiedTheFileOver = true;
		}
	}
	return bWeCopiedTheFileOver;
}

void entity_adduniqueentity ( bool bAllowDuplicates )
{
	// Ensure 'entitybank\' is not part of entity filename
	t.entdir_s="entitybank\\";
	if (  cstr(Lower(Left(t.addentityfile_s.Get(),11))) == "entitybank\\" ) 
	{
		t.addentityfile_s=Right(t.addentityfile_s.Get(),Len(t.addentityfile_s.Get())-11);
	}
	if (  cstr(Lower(Left(t.addentityfile_s.Get(),8))) == "ebebank\\" ) 
	{
		t.entdir_s = "";
	}

	//  Check if entity already loaded in
	t.talreadyloaded=0;
	if ( bAllowDuplicates == false )
	{
		for ( t.t = 1 ; t.t<=  g.entidmaster; t.t++ )
		{
			if (  t.entitybank_s[t.t] == t.addentityfile_s ) {  t.talreadyloaded = 1  ; t.entid = t.t; }
		}
	}
	if (t.talreadyloaded == 0)
	{
		//  Allocate one more entity item in array
		if (g.entidmaster > g.entitybankmax - 4)
		{
			Dim (t.tempentitybank_s, g.entitybankmax);
			for (t.t = 0; t.t <= g.entitybankmax; t.t++) t.tempentitybank_s[t.t] = t.entitybank_s[t.t];
			++g.entitybankmax;
			UnDim (t.entitybank_s);
			Dim (t.entitybank_s, g.entitybankmax);
			for (t.t = 0; t.t <= g.entitybankmax - 1; t.t++) t.entitybank_s[t.t] = t.tempentitybank_s[t.t];
		}

		//  Add entity to bank
		++g.entidmaster; entity_validatearraysize ();
		t.entitybank_s[g.entidmaster] = t.addentityfile_s;

		// trigger the creation of a 'group' entity if detected
		if (g_iAbortedAsEntityIsGroupFileMode != 3)
			g_iAbortedAsEntityIsGroupFileMode = 1;

		// if using remote project, first duplicate the entity file to local project
		bool bAlsoCopyOverAllRelatedEntityFiles = false;
		char pThisEntityFile[MAX_PATH];
		strcpy(pThisEntityFile, "entitybank\\");
		strcat(pThisEntityFile, t.addentityfile_s.Get());
		if (entity_copytoremoteifnotthere(pThisEntityFile) == true)
		{
			// and if successfully copied over, copy over all related files
			bAlsoCopyOverAllRelatedEntityFiles = true;
		}

		//  Load extra entity
		t.entid = g.entidmaster;
		t.ent_s = t.entitybank_s[t.entid];
		t.entpath_s = getpath(t.ent_s.Get());
		extern uint32_t SetMasterObject;
		SetMasterObject = g.entitybankoffset + t.entid;
		entity_load ();
		SetMasterObject = 0;

		// copy over all related files if using a remote project
		if (bAlsoCopyOverAllRelatedEntityFiles == true)
		{
			extern bool g_bMakingAStandaloneUsingFileCollectionArray;
			if (g_bMakingAStandaloneUsingFileCollectionArray == false)
			{
				// clear file collection
				g.filecollectionmax = 0;
				Undim (t.filecollection_s);
				Dim (t.filecollection_s, 500);

				// collect all the associated files into filecollection
				extern void mapfile_addallentityrelatedfiles(int, entityeleproftype*);
				entity_fillgrideleproffromprofile();
				mapfile_addallentityrelatedfiles (t.entid, &t.grideleprof);

				// copy all the file collection to the remote project
				extern void mapfile_copyallfilecollectiontopreferredprojectfolder(void);
				mapfile_copyallfilecollectiontopreferredprojectfolder();
			}
		}

		// 090317 - ignore ebebank new structure to avoid empty EBE icons being added to local library left list
		if (stricmp (t.addentityfile_s.Get(), "..\\ebebank\\_builder\\New Site.fpe") == NULL)
			t.talreadyloaded = 1;
	}
}

void entity_validatearraysize ( void )
{
	//  ensure enough space in entity profile arrays
	if (  g.entidmaster+32>g.entidmastermax ) 
	{
		g.entidmastermax=g.entidmaster+32;
		Dim2(  t.entitybodypart,g.entidmastermax, 100   );
		Dim2(  t.entityappendanim,g.entidmastermax, 100  );
		Dim2(  t.entityanim,g.entidmastermax, g.animmax   );
		Dim2(  t.entityfootfall,g.entidmastermax, g.footfallmax  );
		Dim (  t.entityprofileheader,g.entidmastermax   );
		Dim (  t.entityprofile,g.entidmastermax  );
		Dim2(  t.entitydecal_s,g.entidmastermax, 100  );
		Dim2(  t.entitydecal,g.entidmastermax, 100   );
		g.entitybankmax=g.entidmastermax;
		Dim (  t.entitybank_s,g.entidmastermax  );
	}
}


//PE: GenerateD3D9ForMesh - make sure semantic is stored in old D3D9 format.
//PE: Without get fvf offset can fail , and original skin weight is not used but generated , this can give animation problems.
//PE: This is not a problem when using the importer, as it will save everything in the old D3D9 format into the dbo.
void GenerateD3D9ForMesh(sMesh* pMesh, BOOL bNormals, BOOL bTangents, BOOL bBinormals, BOOL bDiffuse, BOOL bBones)
{
	// get FVF details
	sOffsetMap offsetMap;
	GetFVFValueOffsetMap(pMesh->dwFVF, &offsetMap);

	// deactivate bone flag if no bones in source mesh
	if (pMesh->dwBoneCount == 0) bBones = FALSE;

	// valid mesh (no longer using DXMESH)
	if (pMesh->dwFVF > 0)
	{
		// extract vertex size from mesh
		WORD wNumBytesPerVertex = (WORD)pMesh->dwFVFSize;

		// Starting declaration
		int iDeclarationIndex = 0;
		D3D11_INPUT_ELEMENT_DESC pDeclaration[12];

		// check if mesh already has a component (and build declaration)
		BOOL bHasNormals = FALSE;
		BOOL bHasDiffuse = FALSE;
		BOOL bHasTangents = FALSE;
		BOOL bHasBinormals = FALSE;
		BOOL bHasBlendWeights = FALSE;
		BOOL bHasBlendIndices = FALSE;
		BOOL bHasSecondaryUVs = FALSE;
		if (pMesh->dwFVF & GGFVF_XYZ)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "POSITION";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = 0;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
		}
		if (pMesh->dwFVF & GGFVF_NORMAL)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "NORMAL";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			bHasNormals = TRUE;
		}
		if (pMesh->dwFVF & GGFVF_TEX1)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "TEXCOORD";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			bHasDiffuse = TRUE;
		}
		if (pMesh->dwFVF & GGFVF_DIFFUSE)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "COLOR";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			bHasDiffuse = TRUE;
		}
		if (pMesh->dwFVF & offsetMap.dwTU[1] > 0)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "TEXCOORD";
			pDeclaration[iDeclarationIndex].SemanticIndex = 1;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			bHasSecondaryUVs = TRUE;
		}

		if (!bHasNormals && bNormals)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "NORMAL";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			wNumBytesPerVertex += 12;
		}
		if (!bHasDiffuse && bDiffuse)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "COLOR";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			wNumBytesPerVertex += 4;
		}
		if (!bHasTangents && bTangents)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "TANGENT";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			wNumBytesPerVertex += 12;
		}
		if (!bHasBinormals && bBinormals)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "BINORMAL";
			pDeclaration[iDeclarationIndex].SemanticIndex = 0;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			wNumBytesPerVertex += 12;
		}
		DWORD dwOffsetToWeights = wNumBytesPerVertex;
		if (!bHasBlendWeights && bBones)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "TEXCOORD";
			pDeclaration[iDeclarationIndex].SemanticIndex = 1;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			wNumBytesPerVertex += 16;
		}
		DWORD dwOffsetToIndices = wNumBytesPerVertex;
		if (!bHasBlendIndices && bBones)
		{
			pDeclaration[iDeclarationIndex].SemanticName = "TEXCOORD";
			pDeclaration[iDeclarationIndex].SemanticIndex = 2;
			pDeclaration[iDeclarationIndex].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			pDeclaration[iDeclarationIndex].InputSlot = 0;
			pDeclaration[iDeclarationIndex].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			pDeclaration[iDeclarationIndex].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			pDeclaration[iDeclarationIndex].InstanceDataStepRate = 0;
			iDeclarationIndex++;
			wNumBytesPerVertex += 16;
		}

		// copy declaration into old D3D9 format (as DBO relies on this data in the binary!)
		int iDecIndex = 0;
		int iByteOffset = 0;
		for (; iDecIndex < iDeclarationIndex; iDecIndex++)
		{
			int iEntryByteSize = 0;
			if (stricmp(pDeclaration[iDecIndex].SemanticName, "POSITION") == NULL)
			{
				pMesh->pVertexDeclaration[iDecIndex].Usage = GGDECLUSAGE_POSITION;
				pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT3;
				iEntryByteSize = 12;
			}
			if (stricmp(pDeclaration[iDecIndex].SemanticName, "NORMAL") == NULL)
			{
				pMesh->pVertexDeclaration[iDecIndex].Usage = GGDECLUSAGE_NORMAL;
				pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT3;
				iEntryByteSize = 12;
			}
			if (stricmp(pDeclaration[iDecIndex].SemanticName, "COLOR") == NULL)
			{
				pMesh->pVertexDeclaration[iDecIndex].Usage = GGDECLUSAGE_COLOR;
				pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT2;
				iEntryByteSize = 4;
			}
			if (stricmp(pDeclaration[iDecIndex].SemanticName, "TANGENT") == NULL)
			{
				pMesh->pVertexDeclaration[iDecIndex].Usage = GGDECLUSAGE_TANGENT;
				pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT3;
				iEntryByteSize = 12;
			}
			if (stricmp(pDeclaration[iDecIndex].SemanticName, "BINORMAL") == NULL)
			{
				pMesh->pVertexDeclaration[iDecIndex].Usage = GGDECLUSAGE_BINORMAL;
				pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT3;
				iEntryByteSize = 12;
			}
			if (stricmp(pDeclaration[iDecIndex].SemanticName, "TEXCOORD") == NULL)
			{
				pMesh->pVertexDeclaration[iDecIndex].Usage = GGDECLUSAGE_TEXCOORD;
				if (pDeclaration[iDecIndex].Format == DXGI_FORMAT_R32G32B32A32_FLOAT)
				{
					pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT4;
					iEntryByteSize = 16;
				}
				else
				{
					pMesh->pVertexDeclaration[iDecIndex].Type = GGDECLTYPE_FLOAT2;
					iEntryByteSize = 8;
				}
			}
			pMesh->pVertexDeclaration[iDecIndex].Stream = 0;
			pMesh->pVertexDeclaration[iDecIndex].Method = GGDECLMETHOD_DEFAULT;
			pMesh->pVertexDeclaration[iDecIndex].UsageIndex = pDeclaration[iDecIndex].SemanticIndex;
			pMesh->pVertexDeclaration[iDecIndex].Offset = iByteOffset;
			iByteOffset += iEntryByteSize;
		}
		pMesh->pVertexDeclaration[iDecIndex].Stream = 255;
	}
}

void CreateVectorListFromCPPAssembly ( LPSTR pCCPAssemblyStringLine, std::vector<std::string> & vecOfStrs )
{
	char* pAssemblyString = pCCPAssemblyStringLine;
	if (pAssemblyString)
	{
		// get past equals and any spaces
		while (*pAssemblyString == '=' || *pAssemblyString == 32) pAssemblyString++;

		// now we have the assembly string; adult male hair 01,adult male head 01,adult male body 03,adult male legs 04e,adult male feet 04
		// delimited by a comma, and indicates which parts we used (to specify the textures to copy over)
		char pCustomPathToFolder[MAX_PATH];
		cstr assemblyString_s = FirstToken(pAssemblyString, ",");
		while (assemblyString_s.Len() > 0)
		{
			// work out texture files from this reference, i.e adult male hair 01
			char pAssemblyReference[1024];
			strcpy(pAssemblyReference, assemblyString_s.Get());
			strlwr(pAssemblyReference);
			if (pAssemblyReference[strlen(pAssemblyReference) - 1] == '\n') pAssemblyReference[strlen(pAssemblyReference) - 1] = 0;
			int iBaseCount = g_CharacterType.size();// 3;
			for (int iBaseIndex = 0; iBaseIndex < iBaseCount; iBaseIndex++)
			{
				LPSTR pBaseName = "";
				if (iBaseIndex == 0) pBaseName = "adult male";
				if (iBaseIndex == 1) pBaseName = "adult female";
				if (iBaseIndex == 2) pBaseName = "zombie male";
				if (iBaseIndex == 3) pBaseName = "zombie female";
				if (iBaseIndex > 3)
				{
					pBaseName = g_CharacterType[iBaseIndex].pPartsFolder;
				}
				if (strstr(pAssemblyReference, pBaseName) != NULL)
				{
					// found category
					cstr pPartFolder = "";
					if (iBaseIndex == 0) pPartFolder = "charactercreatorplus\\parts\\adult male\\";
					if (iBaseIndex == 1) pPartFolder = "charactercreatorplus\\parts\\adult female\\";
					if (iBaseIndex == 2) pPartFolder = "charactercreatorplus\\parts\\zombie male\\";
					if (iBaseIndex == 3) pPartFolder = "charactercreatorplus\\parts\\zombie female\\";
					if (iBaseIndex > 3)
					{
						sprintf(pCustomPathToFolder, "charactercreatorplus\\parts\\%s\\", g_CharacterType[iBaseIndex].pPartsFolder);
						pPartFolder = pCustomPathToFolder;
					}

					// add final texture files
					cstr pTmpFile = pPartFolder + pAssemblyReference;
					char pRemoveTag[MAX_PATH];
					strcpy(pRemoveTag, pTmpFile.Get());
					for (int nnn = 0; nnn < strlen(pRemoveTag); nnn++)
					{
						if (pRemoveTag[nnn] == '[')
						{
							if (pRemoveTag[nnn - 1] == ' ') nnn--;
							pRemoveTag[nnn] = 0;
							break;
						}
					}

					// need to strip out the tag [xxx] part to find texture proper
					std::string str = "";
					str = pRemoveTag; str = str + "_color.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_normal.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_surface.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_mask.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_emissive.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_roughness.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_metalness.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_gloss.dds"; vecOfStrs.push_back(str);
					str = pRemoveTag; str = str + "_ao.dds"; vecOfStrs.push_back(str);
				}
			}
			assemblyString_s = NextToken(",");
		}
	}
}

bool entity_load_thread_prepare(LPSTR pFpeFile)
{
	// preload thread busy, just ignore
	if (object_preload_files_in_progress() || image_preload_files_in_progress())
		return false;

	// vars to do preloading
	cstr sFpeFile = pFpeFile;
	cstr model_s = ""; //Need this from the fpe. t.entityprofile[t.entid].model_s
	int ischaractercreator = 0; //Need this from fpe. t.entityprofile[t.entid].ischaractercreator
	cstr sFile = "", sDboFile = "";
	std::vector<std::string> fpe_file;

	// work out entity bank path
	cstr sEntityBank = "entitybank\\";
	if (cstr(Lower(Left(sFpeFile.Get(), 11))) == "entitybank\\")
	{
		sFpeFile = Right(sFpeFile.Get(), Len(sFpeFile.Get()) - 11);
	}
	if (cstr(Lower(Left(sFpeFile.Get(), 8))) == "ebebank\\")
	{
		sEntityBank = "";
	}

	// already have this loaded into the level
	for (t.t = 1; t.t <= g.entidmaster; t.t++)
	{
		if (t.entitybank_s[t.t] == sFpeFile)
		{
			// fpe already loaded ready to use
			return true; 
		}
	}

	// find the entity file
	cstr epath_s = getpath(sFpeFile.Get());
	sFpeFile = sEntityBank + sFpeFile;
	if (FileExist(sFpeFile.Get()) == 0)
	{
		return(false); //FPE not found.
	}

	if (getVectorFileContent(sFpeFile.Get(), fpe_file, true) == false)
	{
		return(false); //FPE not found.
	}

	// find model file (DBO)
	std::string model = GetLineParameterFromVectorFile("model=", fpe_file, true);
	if (model.length() <= 0) return(false);

	// determine if character creator object
	std::string sIscharacterCreator = GetLineParameterFromVectorFile("charactercreator=", fpe_file, true);
	model_s = model.c_str();
	if (sIscharacterCreator.length() > 0 && atoi(sIscharacterCreator.c_str()) == 1)
		ischaractercreator = 1;

	// check if we can preload this model
	if (ischaractercreator == 0)
		sFile = sEntityBank + epath_s + model_s;
	else
		sFile = model_s;

	// if .X or .FBX file specified, and DBO exists, load DBO as main model file
	int iSrcFormat = 0;
	if (strcmp(Lower(Right(sFile.Get(), 2)), ".x") == 0) iSrcFormat = 1;
	if (strcmp(Lower(Right(sFile.Get(), 4)), ".fbx") == 0) iSrcFormat = 2;
	if (iSrcFormat > 0)
	{
		if (iSrcFormat == 1)
		{
			// X File Format
			sDboFile = Left(sFile.Get(), Len(sFile.Get()) - 2); sDboFile += ".dbo";
			if (sDboFile != "" && FileExist(sDboFile.Get()) == 1) sFile = sDboFile;
		}
		if (iSrcFormat == 2)
		{
			// FBX File Format
			sDboFile = Left(sFile.Get(), Len(sFile.Get()) - 4); sDboFile += ".dbo";
			if (sDboFile != "" && FileExist(sDboFile.Get()) == 1) sFile = sDboFile;
		}
	}
	else
	{
		// if .X or .FBX file NOT specified
		sDboFile = "";
		if (strcmp(Lower(Right(sFile.Get(), 4)), ".dbo") == 0)
		{
			if (FileExist(sFile.Get()) == 0)
			{
				//We can only preload dbo files.
				return(false);
			}
		}
	}

	// get path to original model
	char pModelPath[10248];
	strcpy(pModelPath, "");
	LPSTR pOrigModelFilename = sFile.Get();
	for (int n = strlen(pOrigModelFilename); n > 0; n--)
	{
		if (pOrigModelFilename[n] == '\\' || pOrigModelFilename[n] == '/')
		{
			strcpy(pModelPath, pOrigModelFilename);
			pModelPath[n + 1] = 0;
			break;
		}
	}
	if (FileExist(sFile.Get()) == 0)
	{
		sFile = model_s;
		if (strcmp(Lower(Right(sFile.Get(), 2)), ".x") == 0) { sDboFile = Left(sFile.Get(), Len(sFile.Get()) - 2); sDboFile += ".dbo"; }
		else sDboFile = "";
		if (sDboFile != "" && FileExist(sDboFile.Get()) == 1)  sFile = sDboFile;
	}

	// if DBO model file exists
	if (FileExist(sFile.Get()) == 1)
	{
		if (FileExist(sDboFile.Get()) == 1)
		{
			sFile = sDboFile;
		}
		if (strcmp(Lower(Right(sFile.Get(), 4)), ".dbo") == 0)
		{
			// also scan FPE for all references to textures and preload those
			image_preload_files_start();
			for (int iTexLineScan = 0; iTexLineScan < 100; iTexLineScan++)
			{
				// texture ref field name
				char pTexRefFieldNam[32];
				if ( iTexLineScan > 0 ) 
					sprintf (pTexRefFieldNam, "textureref%d=", iTexLineScan);
				else
					strcpy (pTexRefFieldNam, "textured=");

				// get texture ref data from FPE file
				std::string texturefileref = "";
				texturefileref = GetLineParameterFromVectorFile(pTexRefFieldNam, fpe_file, true);

				// if texture ref valid
				if (texturefileref.length() > 0)
				{
					// certainly preload the one specified
					LPSTR pTextureRef = (LPSTR)texturefileref.c_str();

					// add to image preload
					char pPreloadThisFile[MAX_PATH];
					strcpy (pPreloadThisFile, pModelPath);
					strcat (pPreloadThisFile, pTextureRef);
					image_preload_files_add(pPreloadThisFile);

					// clean tex ref and remove extension if any
					char pCleanTexRef[MAX_PATH];
					strcpy(pCleanTexRef, pTextureRef);
					strlwr(pCleanTexRef);
					LPSTR pExt = NULL;
					pExt = strstr(pCleanTexRef, ".dds"); if (pExt) *pExt = 0;
					pExt = strstr(pCleanTexRef, ".png"); if (pExt) *pExt = 0;
					pExt = strstr(pCleanTexRef, ".tga"); if (pExt) *pExt = 0;
					pExt = strstr(pCleanTexRef, ".jpg"); if (pExt) *pExt = 0;
					pExt = strstr(pCleanTexRef, ".bmp"); if (pExt) *pExt = 0;

					// preload all its PBR set variants
					if ( strstr (pCleanTexRef, "_color") != NULL)
					{
						char pBaseTexRef[MAX_PATH];
						strcpy (pBaseTexRef, pCleanTexRef);
						pBaseTexRef[strlen(pBaseTexRef) - 6] = 0;
						for (int iPBRSet = 1; iPBRSet < 5; iPBRSet++)
						{
							char pThisPBRTex[MAX_PATH];
							strcpy (pThisPBRTex, pBaseTexRef);
							if (iPBRSet == 1) strcat (pThisPBRTex, "_normal.dds");
							if (iPBRSet == 2) strcat (pThisPBRTex, "_surface.dds");
							if (iPBRSet == 3) strcat (pThisPBRTex, "_emissive.dds");
							if (iPBRSet == 4) strcat (pThisPBRTex, "_displacement.dds");
							strcpy (pPreloadThisFile, pModelPath);
							strcat (pPreloadThisFile, pThisPBRTex);
							image_preload_files_add(pPreloadThisFile);
						}
					}
				}
				// also account for character creator assemblies
				std::vector<std::string> ccpAsemblyList;
				std::string ccpAssemblyRef = GetLineParameterFromVectorFile("ccpassembly=", fpe_file, true);
				CreateVectorListFromCPPAssembly((LPSTR)ccpAssemblyRef.c_str(), ccpAsemblyList);
				for ( int i = 0; i < ccpAsemblyList.size(); i++ ) image_preload_files_add((LPSTR)ccpAsemblyList[i].c_str());
				ccpAsemblyList.clear();
			}
			image_preload_files_finish();

			// preload is a go!
			return(true);
		}
	}

	// otherwise not preloading today
	return(false);
}

bool entity_load (bool bCalledFromLibrary)
{
	// allowed by default on successful load of the model
	bool bSavingDBOAllowed = true;

	//  Activate auto generation of mipmaps for ALL entities
	SetImageAutoMipMap (  1 );

	//  Entity Object Index
	t.entobj=g.entitybankoffset+t.entid;

	//  debug info
	t.mytimer= MAXTimer();

	//  Load Entity profile data
	entity_loaddata ();

	// special group detection
	bool bWeAreAGroup = false;
	if (g_iAbortedAsEntityIsGroupFileMode == 3)
	{
		// these entities are loaded by the group (store which group this child belongs)
		extern int current_selected_group;
		t.entityprofile[t.entid].ischildofgroup = 1 + current_selected_group;
	}
	cstr LastGroupFilename_s = "";
	if (g_iAbortedAsEntityIsGroupFileMode == 2)
	{
		// okay we are a group!
		int iStoreObj = t.entobj;
		int iStoreEntID = t.entid;
		extern bool LoadGroup(LPSTR pAbsFilename);
		cstr pGroupFilename_s = t.tFPEName_s;
		LastGroupFilename_s = pGroupFilename_s;
		if (bCalledFromLibrary)
		{
			//PE: check if we already got a thumb.
			//PE: Check if a got the original image for this smart object.
			bool CreateBackBufferCacheName(char *file, int width, int height);
			extern cstr BackBufferCacheName;
			CreateBackBufferCacheName(t.addentityfile_s.Get(), 512, 288);
			extern StoryboardStruct Storyboard;
			if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
			{
				GG_SetWritablesToRoot(true);
			}
			if (FileExist(BackBufferCacheName.Get()))
			{
				LoadImage((char *)BackBufferCacheName.Get(), ENTITY_CACHE_ICONS_LARGE + t.entid);
				if (ImageExist(ENTITY_CACHE_ICONS_LARGE + t.entid))
				{
					t.entityprofile[t.entid].iThumbnailLarge = ENTITY_CACHE_ICONS_LARGE + t.entid;
					g_iAbortedAsEntityIsGroupFileMode = 0;
					//PE: No need to read the group and load the files we have the thumb.
					GG_SetWritablesToRoot(false);
					return(false);
				}
			}
			GG_SetWritablesToRoot(false);
		}
		
		g_iAbortedAsEntityIsGroupFileMode = 3;
		LoadGroup(pGroupFilename_s.Get());
		g_iAbortedAsEntityIsGroupFileMode = 0;

		t.entobj = iStoreObj;
		t.entid = iStoreEntID;
		bWeAreAGroup = true;
		// after group created, need to clear reminants from cursor
		g.thumbentityrubberbandlist = g.entityrubberbandlist;
		g.entityrubberbandlist.clear();

		//Make sure any selection are removed
		t.gridentity = 0;
		t.inputsys.constructselection = 0;
		t.inputsys.domodeentity = 1;
		t.grideditselect = 5;
		editor_refresheditmarkers();
	}

	//  Only load characters for entity-local-testing
	t.desc_s=t.entityprofileheader[t.entid].desc_s;
	if (  t.scanforentitiescharactersonly == 1 ) 
	{
		if (  t.entityprofile[t.entid].ischaracter == 0 ) 
		{
			t.desc_s="";
		}
	}

	//  Special mode (from lightmapper) which only loads static entities
	//  which will speed up lightmapping process and reduce hit on system memory
	//  not excluding markers as we need some of them for lighting info
	if (  t.lightmapper.onlyloadstaticentitiesduringlightmapper == 1 ) 
	{
		if (  t.entityprofile[t.entid].ischaracter == 1 ) 
		{
			t.desc_s="";
		}
	}

	// Only if profile data exists
	if (t.desc_s != "")
	{
		//Load the thumbnail.
		#if defined(ENABLEIMGUI) && !defined(USEOLDIDE) 
		t.strwork = t.entdir_s + t.ent_s;
		t.tthumbbmpfile_s = "";	t.tthumbbmpfile_s = t.tthumbbmpfile_s + Left(t.strwork.Get(), (Len(t.entdir_s.Get()) + Len(t.ent_s.Get())) - 4) + ".bmp";

		GG_SetWritablesToRoot(true);
		image_setlegacyimageloading(true);
		LoadImage(t.tthumbbmpfile_s.Get(), ENTITY_CACHE_ICONS + t.entid);
		image_setlegacyimageloading(false);
		GG_SetWritablesToRoot(false);
		t.entityprofile[t.entid].iThumbnailSmall = ENTITY_CACHE_ICONS + t.entid;
		if (!ImageExist(t.entityprofile[t.entid].iThumbnailSmall))
			t.entityprofile[t.entid].iThumbnailSmall = TOOL_ENTITY;
		t.strwork = t.entdir_s + t.ent_s;
		if (ImageExist(t.entityprofile[t.entid].iThumbnailLarge))
		{
			// ensure the latest thumb is used
			DeleteImage(t.entityprofile[t.entid].iThumbnailLarge);
		}
		//t.entityprofile[t.entid].iThumbnailLarge = 0; and do not wipe out necessary index for the eventual reload
		bool CreateBackBufferCacheName(char *file, int width, int height);
		extern cstr BackBufferCacheName;

		SetMipmapNum(1); //PE: mipmaps not needed.
		image_setlegacyimageloading(true);

		bool bWeGotaThumb = false;
		if (bWeAreAGroup)
		{
			//PE: Check if a got the original image for this smart object.
			CreateBackBufferCacheName(LastGroupFilename_s.Get(), 512, 288);
			extern StoryboardStruct Storyboard;
			if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
			{
				GG_SetWritablesToRoot(true);
			}
			if (FileExist(BackBufferCacheName.Get()))
			{
				LoadImage((char *)BackBufferCacheName.Get(), ENTITY_CACHE_ICONS_LARGE + t.entid);
				if (ImageExist(ENTITY_CACHE_ICONS_LARGE + t.entid))
				{
					t.entityprofile[t.entid].iThumbnailLarge = ENTITY_CACHE_ICONS_LARGE + t.entid;
					bWeGotaThumb = true;
				}
			}
			GG_SetWritablesToRoot(false);
		}

		if (!bWeGotaThumb)
		{
			CreateBackBufferCacheName(t.strwork.Get(), 512, 288);
			extern StoryboardStruct Storyboard;
			if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
			{
				GG_SetWritablesToRoot(true);
			}
			if (FileExist(BackBufferCacheName.Get()))
			{
				LoadImage((char *)BackBufferCacheName.Get(), ENTITY_CACHE_ICONS_LARGE + t.entid);
				if (ImageExist(ENTITY_CACHE_ICONS_LARGE + t.entid))
					t.entityprofile[t.entid].iThumbnailLarge = ENTITY_CACHE_ICONS_LARGE + t.entid;
			}
			GG_SetWritablesToRoot(false);
		}
		image_setlegacyimageloading(false);
		SetMipmapNum(-1);
		#endif

		// if a group, make a modified entity that references a group
		if (bWeAreAGroup == true)
		{
			// the group entity
			int iGroupEntID = t.entid;
			int iGroupEntObj = t.entobj;

			// if a 'group' entity, set reference to group
			extern int current_selected_group;
			t.entityprofile[iGroupEntID].model_s = "group";

			// record this group into entity parent profile
			t.entityprofile[iGroupEntID].groupreference = 1; // group indexes can MOVE!

			// test object!
			g_iWickedEntityId = iGroupEntID;
			MakeObjectCube(iGroupEntObj, 0.0f);
			g_iWickedEntityId = -1;

			// this is ther group object entity ID!
			t.entid = iGroupEntID;
			t.entobj = iGroupEntObj;

			//PE: Moved here so we get the groupreference.
			//PE: current_selected_group is used by thumb system , and could crash without it.
			extern int thumb_selected_group;
			thumb_selected_group = current_selected_group;
			current_selected_group = -1; // fixes cellar issue sticky candle and switch escape crash
			// and go no further
			return false;
		}

		//  Load the model
		if (t.entityprofile[t.entid].ischaractercreator == 0)
		{
			t.tfile_s = t.entdir_s + t.entpath_s + t.entityprofile[t.entid].model_s;
		}
		else
		{
			t.tfile_s = t.entityprofile[t.entid].model_s;
		}
		deleteOutOfDateDBO(t.tfile_s.Get());
		// if .X or .FBX file specified, and DBO exists, load DBO as main model file
		int iSrcFormat = 0;
		if (strcmp(Lower(Right(t.tfile_s.Get(), 2)), ".x") == 0) iSrcFormat = 1;
		if (strcmp(Lower(Right(t.tfile_s.Get(), 4)), ".fbx") == 0) iSrcFormat = 2;
		if (iSrcFormat > 0)
		{
			if (iSrcFormat == 1)
			{
				// X File Format
				t.tdbofile_s = Left(t.tfile_s.Get(), Len(t.tfile_s.Get()) - 2); t.tdbofile_s += ".dbo";
				if (t.tdbofile_s != "" && FileExist(t.tdbofile_s.Get()) == 1) t.tfile_s = t.tdbofile_s;
			}
			if (iSrcFormat == 2)
			{
				// FBX File Format
				t.tdbofile_s = Left(t.tfile_s.Get(), Len(t.tfile_s.Get()) - 4); t.tdbofile_s += ".dbo";
				if (t.tdbofile_s != "" && FileExist(t.tdbofile_s.Get()) == 1) t.tfile_s = t.tdbofile_s;
			}
		}
		else
		{
			// if .X or .FBX file NOT specified
			t.tdbofile_s = "";
			if (strcmp(Lower(Right(t.tfile_s.Get(), 4)), ".dbo") == 0)
			{
				// and .DBO specified instead, check if .DBO does not exist
				if (FileExist(t.tfile_s.Get()) == 0)
				{
					// if not exist try .X model file (typical of model import entities that use original X file)
					// but do not copy the DBO file with it
					t.tfile_s = Left(t.tfile_s.Get(), Len(t.tfile_s.Get()) - 4); t.tfile_s += ".x";
				}
			}
		}
		// get path to original model
		char pModelPath[10248];
		strcpy(pModelPath, "");
		LPSTR pOrigModelFilename = t.tfile_s.Get();
		for (int n = strlen(pOrigModelFilename); n > 0; n--)
		{
			if (pOrigModelFilename[n] == '\\' || pOrigModelFilename[n] == '/')
			{
				strcpy(pModelPath, pOrigModelFilename);
				pModelPath[n + 1] = 0;
				break;
			}
		}
		// 070718 - if append final file exists, use that
		bool bUsingAppendAnimFileModel = false;
		cstr pAppendFinalModelFilename = t.entityappendanim[t.entid][0].filename;
		if (strlen(pAppendFinalModelFilename.Get()) > 0)
		{
			pAppendFinalModelFilename = cstr(pModelPath) + pAppendFinalModelFilename;
			if (FileExist(pAppendFinalModelFilename.Get()) == 1)
			{
				bUsingAppendAnimFileModel = true;
				t.tfile_s = pAppendFinalModelFilename;
				pAppendFinalModelFilename = "";
				t.tdbofile_s = "";
			}
		}
		if (FileExist(t.tfile_s.Get()) == 0)
		{
			t.tfile_s = t.entityprofile[t.entid].model_s;
			//  V109 BETA6 - 290408 - allow DBO creation/read if full relative path provides (i.e. meshbank\scifi\etc)
			if (strcmp(Lower(Right(t.tfile_s.Get(), 2)), ".x") == 0) { t.tdbofile_s = Left(t.tfile_s.Get(), Len(t.tfile_s.Get()) - 2); t.tdbofile_s += ".dbo"; }
			else t.tdbofile_s = "";
			if (t.tdbofile_s != "" && FileExist(t.tdbofile_s.Get()) == 1)  t.tfile_s = t.tdbofile_s;
		}
		t.txfile_s = t.tfile_s;
		if (FileExist(t.tfile_s.Get()) == 1)
		{
			//  if DBO version exists, use that (quicker)
			if (FileExist(t.tdbofile_s.Get()) == 1)
			{
				t.tfile_s = t.tdbofile_s;
				t.tdbofile_s = "";
			}
			else
			{
				//  allowed to save DBO (once only)
			}

			bool bNewDecal = false;
			bool bLODLoaded = false;
			//  Load entity (compile does not need the dynamic objects)
			if (t.entobj > 0)
			{
				if (ObjectExist(t.entobj) == 0)
				{
					extern int g_iUseLODObjects;
					extern bool bDisableLODLoad;
					if (g_iUseLODObjects > 0 && !bDisableLODLoad)
					{
						//PE: Try to locate a LOD object.
						std::string lodname = t.tFPEName_s.Get();
						replaceAll(lodname, ".fpe", "_lod.dbo");

						if (FileExist( (char *) lodname.c_str()))
						{
							//PE: Make sure LOD use the correct fpe settings.
							g_iWickedEntityId = t.entid;
							LoadObject((char *) lodname.c_str(), t.entobj);
							char lodlog[1024];
							if (ObjectExist(t.entobj))
							{
								bLODLoaded = true;
								// GG: logged, with each mesh's index counts at LOD0 to LOD3 (0 where a level is missing)
								sprintf(lodlog, "LoadObject LOD ( %s )", lodname.c_str());
								timestampactivity(0, lodlog);
								sObject* pLODObject = GetObjectData(t.entobj);
								for (int iMesh = 0; pLODObject && iMesh < pLODObject->iMeshCount; iMesh++)
								{
									sMesh* pLODMesh = pLODObject->ppMeshList[iMesh];
									if (!pLODMesh) continue;
									sprintf(lodlog, "LOD mesh %d indices: %u %u %u %u", iMesh, (unsigned)pLODMesh->dwIndexCount, (unsigned)pLODMesh->dwIndexCountLOD1, (unsigned)pLODMesh->dwIndexCountLOD2, (unsigned)pLODMesh->dwIndexCountLOD3);
									timestampactivity(0, lodlog);
								}
							}
							else
							{
								sprintf(lodlog, "LoadObject LOD failed ( %s )", lodname.c_str());
								timestampactivity(0, lodlog);
							}
							g_iWickedEntityId = -1;
						}
					}
					// load entity model
					g_iWickedEntityId = t.entid;
					char debug[ 1024 ];
					sprintf(debug, "LoadObject( %s )", t.tfile_s.Get());
					timestampactivity(0, debug);
							
					char* tfile_s = t.tfile_s.Get();
					if (!bLODLoaded)
					{
						if (*tfile_s) LoadObject(t.tfile_s.Get(), t.entobj);
						else MakeObjectBox(t.entobj, 1, 1, 1);
					}
					g_iWickedEntityId = -1;

					if (ObjectExist(t.entobj) == 0)
					{
						// soft error allows failed object loads to continue (message provided in LoadDBO)
						MakeObjectCube(t.entobj, 25);

						// and prevent this temp shape saving as permanent DBO!
						bSavingDBOAllowed = false;
					}

					//LB: default position is not a decal (fixes issue of objects being misaligned in object library auto gen)
					t.entityprofile[t.entid].bIsDecal = false;
					//PE: Old decal support.
					cstr sEffectLower = Lower(t.entityprofile[t.entid].effect_s.Get());
					if (sEffectLower == "effectbank\\reloaded\\decal_animate4.fx")
					{
						t.entityprofile[t.entid].bIsDecal = true;
						t.entityprofile[t.entid].iDecalRows = 4;
						t.entityprofile[t.entid].iDecalColumns = 4;
						t.entityprofile[t.entid].fDecalSpeed = 1.0; // ? later
						bNewDecal = true;
					}
					if (sEffectLower == "effectbank\\reloaded\\decal_animate8.fx")
					{
						t.entityprofile[t.entid].bIsDecal = true;
						t.entityprofile[t.entid].iDecalRows = 8;
						t.entityprofile[t.entid].iDecalColumns = 8;
						t.entityprofile[t.entid].fDecalSpeed = 1.0; // ? later
						bNewDecal = true;
					}
					if (sEffectLower == "effectbank\\reloaded\\decal_animate20.fx")
					{
						t.entityprofile[t.entid].bIsDecal = true;
						t.entityprofile[t.entid].iDecalRows = 20;
						t.entityprofile[t.entid].iDecalColumns = 20;
						t.entityprofile[t.entid].fDecalSpeed = 1.0; // ? later
						bNewDecal = true;
					}
					if (sEffectLower == "effectbank\\reloaded\\decal_animate10.fx" || sEffectLower == "effectbank\\reloaded\\decal_animate10x10.fx")
					{
						t.entityprofile[t.entid].bIsDecal = true;
						t.entityprofile[t.entid].iDecalRows = 10;
						t.entityprofile[t.entid].iDecalColumns = 10;
						t.entityprofile[t.entid].fDecalSpeed = 1.0; // ? later
						bNewDecal = true;
					}
					if (sEffectLower == "effectbank\\reloaded\\decal_animate10x12.fx")
					{
						t.entityprofile[t.entid].bIsDecal = true;
						t.entityprofile[t.entid].iDecalRows = 10;
						t.entityprofile[t.entid].iDecalColumns = 12;
						t.entityprofile[t.entid].fDecalSpeed = 1.0; // ? later
						bNewDecal = true;
					}
					if (bNewDecal)
					{
						float x = ObjectSizeX(t.entobj);
						float y = ObjectSizeY(t.entobj);
						float z = ObjectSizeZ(t.entobj);
						if (ObjectExist(t.entobj)) DeleteObject(t.entobj);
						MakeObjectPlane(t.entobj, x, y);
						PositionObject(t.entobj, 0, y, 0);
						FixObjectPivot(t.entobj);
						SetAlphaMappingOn(t.entobj, 100.0);
						SetObjectTransparency(t.entobj, 6);

						LockVertexDataForLimbCore(t.entobj, 0, 1);
						SetVertexDataNormals(0, 0, 1, 0);
						SetVertexDataNormals(1, 0, 1, 0);
						SetVertexDataNormals(2, 0, 1, 0);
						SetVertexDataNormals(3, 0, 1, 0);
						SetVertexDataNormals(4, 0, 1, 0);
						SetVertexDataNormals(5, 0, 1, 0);
						UnlockVertexData();

						SetObjectUVManually(t.entobj, 0, t.entityprofile[t.entid].iDecalRows , t.entityprofile[t.entid].iDecalColumns );

						SetObjectLight(t.entobj, 0);

						void SetupDecalObject(int obj,int elementID);
						SetupDecalObject(t.entobj,0);

					}

					// 060718 - append animation data from other DBO files
					if (bUsingAppendAnimFileModel == false)
					{
						if (Len(t.tdbofile_s.Get()) == 0)
						{
							if (t.entityprofile[t.entid].appendanimmax > 0)
							{
								for (int aa = 1; aa <= t.entityprofile[t.entid].appendanimmax; aa++)
								{
									cstr pAppendModelFilename = cstr(pModelPath) + t.entityappendanim[t.entid][aa].filename;
									int iStartFrame = t.entityappendanim[t.entid][aa].startframe;
									AppendObject(pAppendModelFilename.Get(), t.entobj, iStartFrame);
								}
							}
						}
					}

					//PE: Only if not already set by a wicked material.
					if (!t.entityprofile[t.entid].WEMaterial.MaterialActive && t.entityprofile[t.entid].WEMaterial.dwBaseColor[0] == -1)
						SetObjectDiffuse(t.entobj, Rgb(255, 255, 255));

					// wipe ANY material emission colors
					if (!t.entityprofile[t.entid].WEMaterial.MaterialActive)
					{
						// only if NOT using an active Wicked material (so multi-mesh object thumbs and previews can have emissives!)
						SetObjectEmissive(t.entobj, 0);
					}

					// prepare properties
					SetFastBoundsCalculation(1);
					SetObjectFilter(t.entobj, 2);
					SetObjectCollisionOff(t.entobj);

					//  if strictly NON-multimaterial, convert now
					if (t.entityprofile[t.entid].onetexture == 1)
					{
						if (GetMeshExist(g.meshgeneralwork) == 1)  DeleteMesh(g.meshgeneralwork);
						MakeMeshFromObject(g.meshgeneralwork, t.entobj);
						DeleteObject(t.entobj);
						MakeObject(t.entobj, g.meshgeneralwork, 0);
					}

					// 011215 - if specified, we can smooth the model before we use it (concrete pipe in TBE level)
					float fSmoothingAngleOrFullGenerate = t.entityprofile[t.entid].smoothangle;
					if (fSmoothingAngleOrFullGenerate > 0 && fSmoothingAngleOrFullGenerate <= 200)
					{
						// 090217 - this only works on orig X files (not subsequent DBO) as they change 
						// the mesh which is then saved out (below)
						if (Len(t.tdbofile_s.Get()) > 1)
						{
							// 090216 - a special mode of over 101 will flip normals for the object (when normals are bad)
							if (fSmoothingAngleOrFullGenerate >= 101.0f)
							{
								SetObjectNormalsEx(t.entobj, 0); // will generate new smooth normals for object
								fSmoothingAngleOrFullGenerate -= 101.0f;
							}

							// and if smoothing factor required, weld some of them together
							if (fSmoothingAngleOrFullGenerate > 0.0f)
							{
								SetObjectSmoothing(t.entobj, fSmoothingAngleOrFullGenerate);
							}
						}
						else
						{
							// some support for direct DBO model smoothing 
							if (fSmoothingAngleOrFullGenerate > 0.0f)
							{
								SetObjectSmoothing(t.entobj, fSmoothingAngleOrFullGenerate);
							}
						}
					}
					else
					{
						// smooth any model, not just X files going to DBO files
						if (fSmoothingAngleOrFullGenerate >= 201)
						{
							SetObjectNormalsEx(t.entobj, 0);
							fSmoothingAngleOrFullGenerate -= 201.0f;
							SetObjectSmoothing(t.entobj, fSmoothingAngleOrFullGenerate);
						}
					}
				}
			}

			//  loaded okay
			if (ObjectExist(t.entobj) == 1)
			{
				// 070718 - if append final model needs to be created, prefer that
				if (strlen(pAppendFinalModelFilename.Get()) > 0)
					if (FileExist(pAppendFinalModelFilename.Get()) == 0)
						t.tdbofile_s = pAppendFinalModelFilename;

				// Save if DBO not exist for entity (for fast loading)
				if (Len(t.tdbofile_s.Get()) > 1 && bSavingDBOAllowed == true && !bLODLoaded )
				{
					// ensure legacy compatibility (avoids new mapedito crashing build process)
					// in wicked, only save if not exist, otherwise existing DBO is not to be touched!
					if (FileExist(t.tdbofile_s.Get()) == 0) SaveObject(t.tdbofile_s.Get(), t.entobj);
					if (FileExist(t.tdbofile_s.Get()) == 1)
					{
						DeleteObject(t.entobj);
						LoadObject(t.tdbofile_s.Get(), t.entobj);
						SetObjectFilter(t.entobj, 2);
						SetObjectCollisionOff(t.entobj);
					}
				}

				// 300817 - if an EBE object with no .EBE file, remove handle from entity
				if (t.entityprofile[t.entid].isebe == 2)
					if (ObjectExist(t.entobj) == 1)
						if (LimbExist(t.entobj, 0) == 1)
							ChangeMesh(t.entobj, 0, 0);

				// Special matrix transform mode for FBX and similar models
				SetObjectRenderMatrixMode(t.entobj, t.entityprofile[t.entid].matrixmode);

				//PE: We are missing skin weight/others in old DX9 DBO setup. needed for some functions.
				//PE: prevent generation of vertex weight that screw up some animations.
				sObject* pObject = g_ObjectList[t.entobj];
				for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
				{
					sMesh* pMesh = pObject->ppMeshList[iMesh];
					GenerateD3D9ForMesh(pMesh, true, true, true, true, true);
				}

				// 131115 - fixes issue of some models not being able to detect with intersectall
				SetObjectDefAnim(t.entobj, t.entityprofile[t.entid].ignoredefanim);

				//  the reverse can be used to allow transparent limbs to be rendered last
				if (t.entityprofile[t.entid].reverseframes == 1)
				{
					ReverseObjectFrames(t.entobj);
				}

				//  main profile object adjustments
				RemoveFixedScale(t.entobj);

				if (t.entityprofile[t.entid].scale != 0)
				{
					ScaleObject(t.entobj, t.entityprofile[t.entid].scale, t.entityprofile[t.entid].scale, t.entityprofile[t.entid].scale);
					//PE: Particle scale bug fix.
					if (t.entityprofile[t.entid].ismarker == 10)
					{
						//PE: Particle use fixed scale.
						SetFixedScale(t.entobj, t.entityprofile[t.entid].scale, t.entityprofile[t.entid].scale, t.entityprofile[t.entid].scale);
					}
				}


				//  Apply texture and effect to entity profile obj
				entity_loadtexturesandeffect();

				//  until static bonemodel scales when not animating, loop
				if (t.entityprofile[t.entid].ischaracter == 1)
				{
					//  refresh using loop stop to get correct character pose
					LoopObject(t.entobj); StopObject(t.entobj);
					//  pivot character to face right way
					RotateObject(t.entobj, 0, 180, 0); FixObjectPivot(t.entobj);
				}

				//if (!bNewDecal)
				{
					//  if entity uses a handle, create and attach it now
					t.entityprofile[t.entid].addhandlelimb = 0;
					if (Len(t.entityprofile[t.entid].addhandle_s.Get()) > 1)
					{
						t.thandlefile_s = t.entdir_s + t.entpath_s + "\\" + t.entityprofile[t.entid].addhandle_s;
						if (FileExist(t.thandlefile_s.Get()) == 1)
						{
							if (ObjectExist(g.entityworkobjectoffset) == 1)  DeleteObject(g.entityworkobjectoffset);
							WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);
							MakeObjectPlane(g.entityworkobjectoffset, 25, 25);
							WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
							RotateObject(g.entityworkobjectoffset, 90, 0, 0);
							ScaleObject(g.entityworkobjectoffset, 50, 50, 50);

							if (GetMeshExist(g.entityworkobjectoffset) == 1)  DeleteMesh(g.entityworkobjectoffset);
							MakeMeshFromObject(g.entityworkobjectoffset, g.entityworkobjectoffset);

							if (bNewDecal)
							{
								//PE: OffsetLimb dont work in wicked , so... (bIsDecal)
								t.tyoffset_f = (t.entityprofile[t.entid].defaultheight / ((t.entityprofile[t.entid].scale + 0.0) / 100.0))*-1;
								LockVertexDataForMesh(g.entityworkobjectoffset);
								for (int i = 0; i < GetVertexDataVertexCount(); i++)
								{
									float vertX = GetVertexDataPositionX(i);
									float vertY = GetVertexDataPositionY(i);
									float vertZ = GetVertexDataPositionZ(i);
									SetVertexDataPosition(i, vertX, vertY + (2 + t.tyoffset_f), vertZ);
								}

								float rc = 4.0;
								SetVertexDataUV(0, 0, 0);
								SetVertexDataUV(1, 1.0 / rc, 0);
								SetVertexDataUV(2, 1.0 / rc, 1.0 / rc);
								SetVertexDataUV(3, 1.0 / rc, 1.0 / rc);
								SetVertexDataUV(4, 0, 1.0 / rc);
								SetVertexDataUV(5, 0, 0);

								UnlockVertexData();
							}

							PerformCheckListForLimbs(g.entityworkobjectoffset);
							if (bNewDecal)
								t.entityprofile[t.entid].addhandlelimb = 1;
							else
								t.entityprofile[t.entid].addhandlelimb = 1 + ChecklistQuantity();

							AddLimb(t.entobj, t.entityprofile[t.entid].addhandlelimb, g.entityworkobjectoffset);
							if (ConfirmObjectAndLimb(t.entobj, t.entityprofile[t.entid].addhandlelimb))
							{
								LinkLimb(t.entobj, 0, t.entityprofile[t.entid].addhandlelimb);
								t.tyoffset_f = (t.entityprofile[t.entid].defaultheight / ((t.entityprofile[t.entid].scale + 0.0) / 100.0))*-1;
								//PE: Offset already done.
								if (!bNewDecal)
									OffsetLimb(t.entobj, t.entityprofile[t.entid].addhandlelimb, 0, 2 + t.tyoffset_f, 0);
								t.texhandleid = loadinternaltexture(t.thandlefile_s.Get());
								TextureLimb(t.entobj, t.entityprofile[t.entid].addhandlelimb, t.texhandleid);
								//SetLimbEffect(t.entobj, t.entityprofile[t.entid].addhandlelimb, t.entityprofile[t.entid].usingeffect);
								if (ObjectExist(g.entityworkobjectoffset) == 1)  DeleteObject(g.entityworkobjectoffset);
								if (GetMeshExist(g.entityworkobjectoffset) == 1)  DeleteMesh(g.entityworkobjectoffset);
								if (t.game.gameisexe == 1)
									HideLimb(t.entobj, t.entityprofile[t.entid].addhandlelimb);
							}
						}
					}
				}

				//  Parent LOD is not enabled, we use clone and instance
				//  settings as we need per-entity element distance control
				if (t.entityprofile[t.entid].isebe == 0)
				{
					// skip if material should come direct from DBO mesh data
					if (t.entityprofile[t.entid].materialindex != 99999)
					{
						// only set material if not EBE, as EBE objects carry per-mesh material values
						SetObjectArbitaryValue(t.entobj, t.entityprofile[t.entid].materialindex);
					}
				}

				//  if entity has decals, find indexes to decals (which are already preloaded)
				t.entityprofile[t.entid].bloodscorch = 0;
				if (t.entityprofile[t.entid].decalmax > 0)
				{
					for (t.tq = 0; t.tq <= t.entityprofile[t.entid].decalmax - 1; t.tq++)
					{
						t.decal_s = t.entitydecal_s[t.entid][t.tq];
						if (strcmp(Lower(t.decal_s.Get()), "blood") == 0)  t.entityprofile[t.entid].bloodscorch = 1;
						decal_find();
						if (t.decalid < 0)  t.decalid = 0;
						t.entitydecal[t.entid][t.tq] = t.decalid;
					}
				}

				//  HideObject (  away )
				// GGMAX 2.33: the park distance is now a knob. DEFAULT 100000 = UNCHANGED.
				// ⚠ WHY THIS EXISTS, and what it does NOT do. A placed clone shares the master's
				// MeshComponent, and `wiScene.cpp:5231-5241` merges `armature->aabb` (WORLD space,
				// resolved from `mesh.armatureID` — a MESH property) into every skinned object's
				// bounds. Measured on spotshadowtest: the weapon mesh has a ONE-BONE armature
				// owned by this parked master, so every placed pickup's AABB becomes the union of
				// itself and this park position — (-700,42,153)-(100001,100001,100001), centre
				// 86,730 units from the camera. That is what overflowed the fp16 batch distance
				// and made pickups invisible (fixed engine-side in 2.32).
				// ★ Lowering this BOUNDS the damage — it does NOT make the bounds correct. The
				// union is still wrong, just level-scale instead of 100 km-scale. The correct fix
				// is engine-side: do not merge an armature box into an instance that does not own
				// that armature's transform. See NIGHT_INVESTIGATIONS_2026-08-12.md §2.32c/§2.33.
				// ⚠ Default deliberately unchanged: masters are hidden but still present, and
				// moving every entity master from 100 km out to inside the play area has real
				// blast radius (ray picks, physics, anything that ignores visibility). Prove it
				// on a full sweep before flipping the default.
				extern int g_masterParkUnits;
				PositionObject(t.entobj, g_masterParkUnits, g_masterParkUnits, g_masterParkUnits);

				//  Set radius of zero allows parent to animate even if outside of frustrum view
				if (GetNumberOfFrames(t.entobj) > 0)
				{
					//  but ONLY for animating objects, do not need to run parent objects if still
					SetSphereRadius(t.entobj, 0);
				}
			}
		}
		else
		{
			//  prevent crash when model name wrong/geometry file missing/etc
			MakeObjectSphere(t.entobj, 1);
			// GGMAX 2.33: same knob as the sibling park above (default 100000 = unchanged).
			extern int g_masterParkUnits;
			PositionObject(t.entobj, g_masterParkUnits, g_masterParkUnits, g_masterParkUnits);
		}

		//  must hide parent objects
		HideObject(t.entobj);

		//  Resolve default weapon gun ids
		if (t.entityprofile[t.entid].isweapon_s != "")
		{
			t.findgun_s = Lower(t.entityprofile[t.entid].isweapon_s.Get()); gun_findweaponindexbyname();
			t.entityprofile[t.entid].isweapon = t.foundgunid;
			if (t.foundgunid > 0)  t.gun[t.foundgunid].activeingame = 1;

			// all weapons are naturally collectable by default
			t.entityprofile[t.entid].iscollectable = 1;
		}
		else
		{
			t.entityprofile[t.entid].isweapon = 0;
		}

		//  Finding hasweapon also in createlemenents (as eleprof may have changed the weapon!)
		if (t.entityprofile[t.entid].hasweapon_s != "")
		{
			t.findgun_s = Lower(t.entityprofile[t.entid].hasweapon_s.Get()); gun_findweaponindexbyname();
			t.entityprofile[t.entid].hasweapon = t.foundgunid;
			if (t.foundgunid > 0 && t.entityprofile[t.entid].isammo == 0)
			{
				// make gun active in game
				t.gun[t.foundgunid].activeingame = 1;

				// 301115 - also populate profile with correct default ammo from clip if default required
				if (t.entityprofile[t.entid].ischaracter == 1)
				{
					if (t.entityprofile[t.entid].quantity == -1)
					{
						t.entityprofile[t.entid].quantity = t.gun[t.foundgunid].settings.reloadqty;
						if (t.entityprofile[t.entid].quantity == 0)
						{
							// discover reload quantity if gun data not loaded in
							t.gunid = t.foundgunid;
							t.gun_s = t.gun[t.gunid].name_s;
							gun_loaddata();
							t.entityprofile[t.entid].quantity = g.firemodes[t.gunid][0].settings.reloadqty;
							t.gunid = 0;
						}
					}
				}
			}
		}
		else
		{
			t.entityprofile[t.entid].hasweapon = 0;
		}

		// see if we can find head automatically
		// 010818 - expanded to find mixamo_xx or bip01_xx or anything_xx
		if (t.entityprofile[t.entid].ischaracter == 1)
		{
			if (t.entityprofile[t.entid].headlimb == -1)
			{
				if (ObjectExist(t.entobj) == 1)
				{
					PerformCheckListForLimbs(t.entobj);
					for (t.tc = 1; t.tc <= ChecklistQuantity(); t.tc++)
					{
						cstr sChecklistFound = Lower(ChecklistString(t.tc));
						LPSTR pChecklistFound = sChecklistFound.Get();
						if (strnicmp(pChecklistFound + strlen(pChecklistFound) - 5, "_head", 5) == NULL)
						{
							t.entityprofile[t.entid].headlimb = t.tc - 1;
							t.tc = ChecklistQuantity() + 1;
						}
					}
				}
			}
			if (t.entityprofile[t.entid].firespotlimb == -1)
			{
				if (ObjectExist(t.entobj) == 1)
				{
					// standard FIRESPOT search
					PerformCheckListForLimbs(t.entobj);
					int iRememberPositionOfRightHand = -1;
					int iLimbCount = ChecklistQuantity();
					for (t.tc = 1; t.tc <= iLimbCount; t.tc++)
					{
						cstr sLimbName = cstr(Lower(ChecklistString(t.tc)));
						if (sLimbName == "firespot")
						{
							t.entityprofile[t.entid].firespotlimb = t.tc - 1;
							t.tc = ChecklistQuantity() + 1;
						}
						else
						{
							if (strstr(sLimbName.Get(),"_r_hand")!=NULL)
							{
								iRememberPositionOfRightHand = t.tc - 1;
							}
						}
					}
					if (t.entityprofile[t.entid].firespotlimb == -1)
					{
						// could not find a FIRESPOT for this character, so create one if found right hand
						if (iRememberPositionOfRightHand != -1)
						{
							// for now, use right hand
							t.entityprofile[t.entid].firespotlimb = iRememberPositionOfRightHand;
						}
					}
				}
			}
			if (t.entityprofile[t.entid].spine == -1)
			{
				if (ObjectExist(t.entobj) == 1)
				{
					PerformCheckListForLimbs(t.entobj);
					for (t.tc = 1; t.tc <= ChecklistQuantity(); t.tc++)
					{
						cstr sChecklistFound = Lower(ChecklistString(t.tc));
						LPSTR pChecklistFound = sChecklistFound.Get();
						// LB: as it turns out, it is Bip01 that has the major say in how the model shuffles forward!
						if ( stricmp(pChecklistFound, "Bip1") == NULL
						||   stricmp(pChecklistFound, "Bip01") == NULL
						||   stricmp(pChecklistFound, "Bip001") == NULL)
						{
							t.entityprofile[t.entid].spine = t.tc - 1;
							break;
						}
					}
				}
			}
			if (t.entityprofile[t.entid].spine2 == -1)
			{
				if (ObjectExist(t.entobj) == 1)
				{
					PerformCheckListForLimbs(t.entobj);
					for (t.tc = 1; t.tc <= ChecklistQuantity(); t.tc++)
					{
						cstr sChecklistFound = Lower(ChecklistString(t.tc));
						LPSTR pChecklistFound = sChecklistFound.Get();
						if (strnicmp(pChecklistFound + strlen(pChecklistFound) - 7, "_spine2", 7) == NULL)
						{
							t.entityprofile[t.entid].spine2 = t.tc - 1;
							break;
						}
					}
				}
			}
		}

		// 090217 - new feature for some characters (Fuse FBX) to have perfect foot planting
		if (t.entityprofile[t.entid].ischaracter == 1 && t.entityprofile[t.entid].isspinetracker == 1 && t.entityprofile[t.entid].spine != -1)
		{
			sObject* pObject = GetObjectData(t.entobj);
			if (pObject)
			{
				pObject->bUseSpineCenterSystem = true;
				pObject->dwSpineCenterLimbIndex = t.entityprofile[t.entid].spine;
				pObject->fSpineCenterTravelDeltaX = 0.0f;
				pObject->fSpineCenterTravelDeltaZ = 0.0f;
			}
		}
		else
		{
			// 051115 - if character, ensure the Y offset is applied to the parent object
			if (t.entityprofile[t.entid].ischaracter == 1 || t.entityprofile[t.entid].offyoverride != 0)
			{
				// calc bounds for static entities that have hierarchy that need shifting up when offyoverride used
				int iCalculateBounds = 0;
				if (t.entityprofile[t.entid].offyoverride != 0) iCalculateBounds = 1;
				OffsetLimb(t.entobj, 0, 0.0f, t.entityprofile[t.entid].offy, 0.0f, iCalculateBounds);
			}
		}

		// 300819 - reenable offset X and Z (for when geometry is shifted from center)
		if (t.entityprofile[t.entid].defaultstatic == 1 && t.entityprofile[t.entid].isimmobile == 1)
		{
			OffsetLimb(t.entobj, 0, t.entityprofile[t.entid].offx, t.entityprofile[t.entid].offy, t.entityprofile[t.entid].offz, 1);
		}

		// 010917 - hide any firespot limb meshes
		if (t.entityprofile[t.entid].firespotlimb > 0)
		{
			ExcludeLimbOn(t.entobj, t.entityprofile[t.entid].firespotlimb);
		}

		//  190115 - wipe out limb control on legacy models, not configured for rotation!
		if (t.entityprofile[t.entid].skipfvfconvert == 1)
		{
			//t.entityprofile[t.entid].firespotlimb=0; but can allow firespot to be retained
			t.entityprofile[t.entid].headlimb = 0;
			t.entityprofile[t.entid].spine = 0;
			t.entityprofile[t.entid].spine2 = 0;
		}

		if (t.entityprofile[t.entid].ismarker == 2)
		{
			// Ensure all lights start as points when first loaded as parent
			entity_updatelightobjtype(t.entobj, 0);
		}

		//PE: Additional settings.
		// For Wicked, cull mode controlled per-mesh with parent default as normal 
		//PE: Prefer WEMaterial over old cullmode
		bool bUseWEMaterial = false;
		if (t.entityprofile[t.entid].WEMaterial.MaterialActive)
		{
			WickedSetEntityId(t.entid);
			WickedSetElementId(0);
			sObject* pObject = g_ObjectList[t.entobj];
			if (pObject)
			{
				bUseWEMaterial = true;
				for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
				{
					sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
					if (pMesh)
					{
						// set properties of mesh
						WickedSetMeshNumber(iMeshIndex);
						bool bDoubleSided = WickedDoubleSided();
						if (bDoubleSided)
						{
							pMesh->bCull = false;
							pMesh->iCullMode = 0;
							WickedCall_SetMeshCullmode(pMesh);
						}
						else
						{
							pMesh->iCullMode = 1;
							pMesh->bCull = true;
							WickedCall_SetMeshCullmode(pMesh);
						}
					}
				}
			}
			WickedSetEntityId(-1);
		}

		if (!bUseWEMaterial)
			SetObjectCull(t.entobj, 1);
		SetObjectLight(t.entobj, 0);
		if (t.entityprofile[t.entid].bIsDecal)
		{
			void SetupDecalObject(int obj, int elementID);
			SetupDecalObject(t.entobj,0);
		}
		SetObjectCollisionOff(t.entobj);

		WickedSetEntityId(t.entid);
		SetAlphaMappingOn(t.entobj, 100);

		SetObjectTransparency(t.entobj, t.entityprofile[t.entid].transparency);
		WickedSetEntityId(-1);

		//  debug info and timestamp list (if logging)
		if (  t.entobj>0 ) 
		{
			if (  ObjectExist(t.entobj) == 1 ) 
			{
				t.strwork = "" ; t.strwork = t.strwork + "Loaded "+Str(t.entid)+":"+t.ent_s + " (" + cstr(GetObjectPolygonCount(t.entobj)) + ")";
				timestampactivity(0, t.strwork.Get() );
			}
		}
	}
	else
	{

		//  debug info and timestamp list (if logging)
		t.strwork = ""; t.strwork = t.strwork + "Skipped "+Str(t.entid)+":"+t.ent_s;
		timestampactivity(0, t.strwork.Get() );
	}

	//  Decactivate auto generation of mipmaps when entity load finished
	SetImageAutoMipMap (  0 );

	// normal entity loaded
	return true;
}

int constexpr conststrlen( const char* str )
{
	return *str ? 1 + conststrlen(str + 1) : 0;
}

#define cmpStrConst( str, cmpVal ) \
{ \
	matched = true; \
	if ( strcmp(str, cmpVal) != 0 ) matched = false; \
}

// max length 16
#define cmpNStrConst( str, cmpVal ) \
{ \
	matched = true; \
	int constexpr len = conststrlen( cmpVal ); \
	if ( strncmp(str, cmpVal, len) != 0 ) matched = false; \
}

