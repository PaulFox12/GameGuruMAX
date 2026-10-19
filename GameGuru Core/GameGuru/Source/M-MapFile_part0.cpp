//----------------------------------------------------
//--- GAMEGURU - M-MapFile
//----------------------------------------------------

// Includes 
#include "stdafx.h"
#include <direct.h>   // GGMAX 3.81b: _getcwd for the visuals-guard diagnostic
int g_ggVisualsGuardRan = -1;        // GGMAX 3.81b: -1 not reached, 0 guard said no, 1 ran
char g_ggVisualsGuardCwd[512] = {0}; // GGMAX 3.81b: cwd when the guard was evaluated
int g_ggReflTrace[4] = {-1,-1,-1,-1};  // GGMAX 3.81c: bReflectionsEnabled sampled through the load
#include "gameguru.h"

#include "GGTerrain\GGTerrainFile.h"
#include "GGTerrain\GGTerrain.h"
#include "GGTerrain\GGTerrainWicked.h"
#include "GGTerrain\GGTrees.h"
#include "GGTerrain\GGGrass.h"
#include "tracers/TracerManager.h"
using namespace GGTerrain;
using namespace GGTrees;
using namespace GGGrass;


#include "GGThread.h"
using namespace GGThread;

class ExtractZipThread : public GGThread
{
protected:
	static char* pZipFileName;
	static const char* pExtractPath;
	static const char* const* pFileNames;
	static const unsigned long* pFileSizes;
	static volatile unsigned char* pFileDone;
	static volatile uint32_t iNextFile;
	static uint32_t iNumFiles;
	static threadLock lock;
	
	static ExtractZipThread* pThreads;
	static uint32_t iNumThreads;

	uint32_t iFileBlockID;

public:

	static void SetWork( char* zipFile, const char* extractPath, const char* const* files, const unsigned long* sizes, uint32_t numFiles )
	{
		pZipFileName = zipFile;
		pExtractPath = extractPath;
		iNextFile = 0;
		iNumFiles = numFiles;
		pFileNames = files;
		pFileSizes = sizes;

		if ( pFileDone ) delete [] pFileDone;
		pFileDone = new unsigned char[ numFiles ];
		for( int i = 0; i < numFiles; i++ ) pFileDone[ i ] = 0;
	}

	static bool AnyRunning()
	{
		for( uint32_t i = 0; i < iNumThreads; i++ ) 
		{
			if ( pThreads[i].IsRunning() ) return true;
		}
		return false;
	}

	static void WaitForAll()
	{
		for( uint32_t i = 0; i < iNumThreads; i++ ) pThreads[i].Join();
	}

	static void StartThreads()
	{
		for( uint32_t i = 0; i < iNumThreads; i++ ) pThreads[i].Start();
	}

	static void SetThreads( uint32_t numThreads )
	{
		if ( numThreads > 250 ) numThreads = 250; // limited by file block IDs to 256 but play it safe
		if ( numThreads == iNumThreads ) return;
		if ( pThreads ) delete [] pThreads;
		
		pThreads = new ExtractZipThread[ numThreads ];
		iNumThreads = numThreads;
		for( uint32_t i = 0; i < iNumThreads; i++ ) pThreads[ i ].iFileBlockID = i+2; // start at index 2
	}

	static bool IsSetup() { return pThreads != 0; }

	static uint32_t GetProgress()
	{
		// no need for lock here since it is an atomic read
		return (iNextFile * 100) / iNumFiles;
	}

	uint32_t Run( ) 
	{
		OpenFileBlock ( pZipFileName, iFileBlockID, "mypassword" );

		while( 1 )
		{
			if ( bTerminate ) break;
			
			int localIndex = -1;
			while( !lock.Acquire() );

			unsigned long maxSize = 0;
			for( int i = 0; i < iNumFiles; i++ )
			{
				if ( pFileDone[i] == 0 && pFileSizes[i] >= maxSize )
				{
					maxSize = pFileSizes[ i ];
					localIndex = i;
				}
			}

			if ( localIndex >= 0 ) pFileDone[localIndex] = 1;

			lock.Release();

			if ( localIndex < 0 ) break;

			const char* filename = pFileNames[ localIndex ];
			ExtractFileFromBlock( iFileBlockID, filename, pExtractPath );
		}

		CloseFileBlock( iFileBlockID );

		return 0;
	}

	ExtractZipThread() : GGThread() 
	{
		
	}

	~ExtractZipThread() 
	{
		if ( pThreads ) delete [] pThreads;
	}
};

char* ExtractZipThread::pZipFileName = 0;
const char* ExtractZipThread::pExtractPath = 0;
const char* const* ExtractZipThread::pFileNames = 0;
const unsigned long* ExtractZipThread::pFileSizes = 0;
volatile unsigned char* ExtractZipThread::pFileDone = 0;
volatile uint32_t ExtractZipThread::iNextFile = 0;
uint32_t ExtractZipThread::iNumFiles = 0;
threadLock ExtractZipThread::lock;
ExtractZipThread* ExtractZipThread::pThreads = 0;
uint32_t ExtractZipThread::iNumThreads = 0;

#include "..\Imgui\imgui.h"
#include "..\Imgui\imgui_impl_win32.h"
#include "..\Imgui\imgui_gg_dx11.h"
std::vector<cstr> g_sDefaultAssetFiles;

extern StoryboardStruct Storyboard;
extern int g_Storyboard_First_Level_Node;
extern int g_Storyboard_Current_Level;
extern char g_Storyboard_First_fpm[256];
extern char g_Storyboard_Current_fpm[256];
extern char g_Storyboard_Current_lua[256];
extern char g_Storyboard_Current_Loading_Page[256];

void AddWPETextures(char* filename);
// 
//  MAP FILE FORMAT
// 

// MapFile Globals
int g_mapfile_iStage = 0;
int g_mapfile_iNumberOfLevels = 0;
int g_mapfile_iNumberOfEntitiesAcrossAllLevels = 0;
float g_mapfile_fProgress = 0.0f;
float g_mapfile_fProgressSpan = 0.0f;
std::vector<cstr> g_mapfile_fppFoldersToRemoveList;
std::vector<cstr> g_mapfile_fppFilesToRemoveList;
cstr g_mapfile_mapbankpath;
cstr g_mapfile_levelpathfolder;
bool g_bAllowBackwardCompatibleConversion = false;
bool g_bNeedToConvertClassicPositionsToMAX = false;
bool g_bMakingAStandaloneUsingFileCollectionArray = false;

bool restore_old_map = false;


void mapfile_saveproject_fpm ( void )
{
	cstr pOldDir = GetDir();

	//  use default or special worklevel stored
	if (  t.goverridefpmdestination_s != "" ) 
	{
		t.ttempprojfilename_s=t.goverridefpmdestination_s;
	}
	else
	{
		t.ttempprojfilename_s=g.projectfilename_s;
	}
	//PE: Default folder could be d:\max\ , and write folder could be f:\docwrite\
	//PE: So then moving zip, it takes from d:\... and move to f:\... , d:\\ dont exists so .fpm file is deleted.
	//PE: To solve always use full path to .fpm
	char destination[MAX_PATH];
	strcpy(destination, t.ttempprojfilename_s.Get());
	GG_GetRealPath(destination, 1);
	t.ttempprojfilename_s = destination;
	if (g.editorsavebak == 1) 
	{
		//PE: Make a backup before overwriting a fpm level.
		char backupname[1024];
		strcpy(backupname, t.ttempprojfilename_s.Get());
		backupname[strlen(backupname) - 1] = 'k';
		backupname[strlen(backupname) - 2] = 'a';
		backupname[strlen(backupname) - 3] = 'b';
		DeleteAFile(backupname);
		CopyAFile(t.ttempprojfilename_s.Get(), backupname);
	}

	//  log prompts
	timestampactivity(0, cstr(cstr("Saving FPM level file: ")+t.ttempprojfilename_s).Get() );

	//PE: Save heightmap.
	uint32_t iHeightmapSize = 0, iHeightmapWidth = 0, iHeightmapHeight = 0;
	iHeightmapSize = GGTerrain::GGTerrain_GetHeightmapDataSize(iHeightmapWidth, iHeightmapHeight);
	t.visuals.iHeightmapWidth = t.gamevisuals.iHeightmapWidth = iHeightmapWidth;
	t.visuals.iHeightmapHeight = t.gamevisuals.iHeightmapHeight = iHeightmapHeight;

	// ensure when export visual, always start with HIGHEST mode to reflect settings chosen when editing level (i.e. grass distance)
	//PE: Respect custom settings, some levels need to disable some optimizing settings...
	if (t.gamevisuals.shaderlevels.entities != 2)
	{
		t.gamevisuals.shaderlevels.terrain = 1;
		t.gamevisuals.shaderlevels.entities = 1;
		t.gamevisuals.shaderlevels.vegetation = 1;
		t.gamevisuals.shaderlevels.lighting = 1;
	}
	//  Switch visuals to gamevisuals as this is what we want to save
	t.editorvisuals=t.visuals ; t.visuals=t.gamevisuals  ; visuals_save ( );

	//  Copy visuals.ini into levelfile folder
	t.tincludevisualsfile=0;
	char pRealVisFile[MAX_PATH];
	strcpy(pRealVisFile, g.fpscrootdir_s.Get());
	strcat(pRealVisFile, "\\visuals.ini");
	GG_GetRealPath(pRealVisFile, 1);
	if (FileExist(pRealVisFile) == 1)
	{
		t.tvisfile_s = g.mysystem.levelBankTestMap_s + "visuals.ini";
		if (FileExist(t.tvisfile_s.Get()) == 1)  DeleteAFile(t.tvisfile_s.Get());
		CopyAFile(pRealVisFile, t.tvisfile_s.Get());
		t.tincludevisualsfile = 1;
	}

	//  And switch back for benefit to editor visuals
	t.visuals=t.editorvisuals; // messes up when click test game again, old: gosub _visuals_save

	//  Delete any old file
	if (  FileExist(t.ttempprojfilename_s.Get()) == 1  )  DeleteAFile (  t.ttempprojfilename_s.Get() );

	//  Copy CFG to testgame area for saving with other files
	t.tttfile_s="cfg.cfg";
	cstr cfgfile_s = g.mysystem.editorsGridedit_s + t.tttfile_s;
	cstr cfginlevelbank_s = g.mysystem.levelBankTestMap_s + t.tttfile_s;
	if (  FileExist( cfgfile_s.Get() ) == 1 )
	{
		if ( FileExist( cfginlevelbank_s.Get() ) == 1 ) DeleteAFile ( cfginlevelbank_s.Get() );
		//PE: We need full path from write folder in wicked.
		char destination[MAX_PATH];
		strcpy(destination, cfgfile_s.Get());
		GG_GetRealPath(destination, 1);
		cfgfile_s = destination;
		CopyAFile ( cfgfile_s.Get(), cfginlevelbank_s.Get() );
	}

	//PE: For some reason cfg.cfg is missing from the fpm ?
	if (FileExist(cfginlevelbank_s.Get()) == 0)
	{
		//PE: Create a new directly inside testmap.
		editor_savecfg(cfginlevelbank_s.Get());
		timestampactivity(0, cstr(cstr("Creating cfg.cfg file: ") + cfginlevelbank_s).Get());
	}

	extern std::vector<sRubberBandType> vEntityLockedList;
	cfginlevelbank_s = g.mysystem.levelBankTestMap_s + "locked.cfg";
	//PE: Save a copy of locked objects.
	if (FileExist(cfginlevelbank_s.Get()) == 1)  DeleteAFile(cfginlevelbank_s.Get());
	OpenToWrite(1, cfginlevelbank_s.Get());
	WriteLong(1, vEntityLockedList.size());
	for (int i = 0; i < vEntityLockedList.size(); i++)
	{
		WriteLong(1, vEntityLockedList[i].e);
	}
	CloseFile(1);

	// Create a FPM (zipfile)
	CreateFileBlock (  1, t.ttempprojfilename_s.Get() );
	SetFileBlockKey (  1, "mypassword" );
	SetDir ( g.mysystem.levelBankTestMap_s.Get() ); // "levelbank\\testmap\\" );
	AddFileToBlock (  1, "header.dat" );
	AddFileToBlock (  1, "playerconfig.dat" );
	AddFileToBlock(1, "locked.cfg");
	AddFileToBlock (  1, "cfg.cfg" );
	// entity and waypoint files
	AddFileToBlock (  1, "map.ele" );
	AddFileToBlock (  1, "map.ent" );
	AddFileToBlock (  1, "map.way" );
	// darkai obstacle data (container zero)
	AddFileToBlock (  1, "map.obs" );
	// terrain files
	// save all node folders (containing terrain geometry and virtual textures)
	ChecklistForFiles (  );
	std::vector<std::string> terrainNodeFolders;
	terrainNodeFolders.clear();
	for ( t.c = 1 ; t.c <= ChecklistQuantity(); t.c++ )
	{
		t.tfile_s = ChecklistString(t.c);
		if ( t.tfile_s != "." && t.tfile_s != ".." && t.tfile_s != "ttsfiles" ) 
		{
			if (ChecklistValueA(t.c) == 1)
			{
				if (cstr(Lower(Left(t.tfile_s.Get(), 2))) == "tt")
				{
					// found terrain node folder
					terrainNodeFolders.push_back(t.tfile_s.Get());
				}
			}
		}
	}
	for (int tt = 0; tt < terrainNodeFolders.size(); tt++)
	{
		LPSTR pTerrainNodeFolder = (LPSTR)terrainNodeFolders[tt].c_str();
		SetDir(pTerrainNodeFolder);
		ChecklistForFiles();
		SetDir("..");
		for (t.c = 1; t.c <= ChecklistQuantity(); t.c++)
		{
			t.tfile_s = ChecklistString(t.c);
			if (t.tfile_s != "." && t.tfile_s != "..")
			{
				cstr pFullRelPathToTerrainFile = cstr(pTerrainNodeFolder) + "\\" + t.tfile_s.Get();
				AddFileToBlock ( 1, pFullRelPathToTerrainFile.Get() );
			}
		}
	}

	#define MAXGROUPSLISTS 100 // duplicated in GridEdit.cpp
	for (int gi = 0; gi < MAXGROUPSLISTS; gi++)
	{
		char pGroupImgFilename[MAX_PATH];
		sprintf(pGroupImgFilename, "groupimg%d.png", gi);
		if (FileExist(pGroupImgFilename) == 1)
		{
			AddFileToBlock (1, pGroupImgFilename);
		}
	}

	// new multi-grass system stores grass choices in testmap folder
	AddFileToBlock ( 1, "grass_coloronly.dds" );

	// new terrain system saves its data settings
	cstr TerrainDataFile_s = g.mysystem.levelBankTestMap_s + "ggterrain.dat";
	GGTerrainFile_SaveTerrainData(TerrainDataFile_s.Get(), g.gdefaultwaterheight);
	AddFileToBlock (1, "ggterrain.dat");
	uint32_t terrain_sculpt_size = GGTerrain::GGTerrain_GetSculptDataSize();
	char *data = new char[terrain_sculpt_size];
	if (data)
	{
		cstr sculpt_data_name = cstr((int)terrain_sculpt_size) + cstr(".dat");
		TerrainDataFile_s = g.mysystem.levelBankTestMap_s + sculpt_data_name;
		GGTerrain::GGTerrain_GetSculptData((uint8_t*)data);
		if (FileExist(TerrainDataFile_s.Get()) == 1) DeleteAFile(TerrainDataFile_s.Get());
		HANDLE hwritefile = GG_CreateFile(TerrainDataFile_s.Get(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hwritefile != INVALID_HANDLE_VALUE)
		{
			DWORD byteswritten;
			WriteFile(hwritefile, data, terrain_sculpt_size, &byteswritten, NULL);
			CloseHandle(hwritefile);
			AddFileToBlock(1, sculpt_data_name.Get());
		}
		delete(data);
	}

	//PE: Save Paint Data.
	uint32_t terrain_paint_size = GGTerrain::GGTerrain_GetPaintDataSize();
	data = new char[terrain_paint_size];
	if (data)
	{
		cstr paint_data_name = cstr((int)terrain_paint_size) + cstr(".ptd");

		TerrainDataFile_s = g.mysystem.levelBankTestMap_s + paint_data_name;

		GGTerrain::GGTerrain_GetPaintData((uint8_t*)data);

		if (FileExist(TerrainDataFile_s.Get()) == 1) DeleteAFile(TerrainDataFile_s.Get());

		HANDLE hwritefile = GG_CreateFile(TerrainDataFile_s.Get(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hwritefile != INVALID_HANDLE_VALUE)
		{
			DWORD byteswritten;
			WriteFile(hwritefile, data, terrain_paint_size, &byteswritten, NULL);
			CloseHandle(hwritefile);
			AddFileToBlock(1, paint_data_name.Get());
		}
		delete(data);
	}

	//PE: Save Tree Data.
	uint32_t terrain_tree_size = GGTrees::GGTrees_GetDataSize() * 4;
	data = new char[terrain_tree_size];
	if (data)
	{
		cstr tree_data_name = cstr((int)terrain_tree_size) + cstr(".tre");

		TerrainDataFile_s = g.mysystem.levelBankTestMap_s + tree_data_name;

		GGTrees::GGTrees_GetData((float*)data);

		if (FileExist(TerrainDataFile_s.Get()) == 1) DeleteAFile(TerrainDataFile_s.Get());

		HANDLE hwritefile = GG_CreateFile(TerrainDataFile_s.Get(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hwritefile != INVALID_HANDLE_VALUE)
		{
			DWORD byteswritten;
			WriteFile(hwritefile, data, terrain_tree_size, &byteswritten, NULL);
			CloseHandle(hwritefile);
			AddFileToBlock(1, tree_data_name.Get());
		}
		delete(data);
	}

	//PE: Save grass Data.
	uint32_t terrain_grass_size = GGGrass::GGGrass_GetDataSize();
	data = new char[terrain_grass_size];
	if (data)
	{
		cstr grass_data_name = cstr((int)terrain_grass_size) + cstr(".gra");

		TerrainDataFile_s = g.mysystem.levelBankTestMap_s + grass_data_name;

		GGGrass::GGGrass_GetData((uint8_t*)data);

		if (FileExist(TerrainDataFile_s.Get()) == 1) DeleteAFile(TerrainDataFile_s.Get());

		HANDLE hwritefile = GG_CreateFile(TerrainDataFile_s.Get(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hwritefile != INVALID_HANDLE_VALUE)
		{
			DWORD byteswritten;
			WriteFile(hwritefile, data, terrain_grass_size, &byteswritten, NULL);
			CloseHandle(hwritefile);
			AddFileToBlock(1, grass_data_name.Get());
		}
		delete(data);
	}

	//PE: Save heightmap data if any.
	if (iHeightmapSize > 0 && iHeightmapWidth > 0 && iHeightmapHeight > 0)
	{
		data = new char[iHeightmapSize];
		if (data)
		{
			cstr heightmap_data_name = "heightmapdata.raw";

			TerrainDataFile_s = g.mysystem.levelBankTestMap_s + heightmap_data_name;

			GGTerrain::GGTerrain_GetHeightmapData((uint16_t*)data);

			if (FileExist(TerrainDataFile_s.Get()) == 1) DeleteAFile(TerrainDataFile_s.Get());

			HANDLE hwritefile = GG_CreateFile(TerrainDataFile_s.Get(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hwritefile != INVALID_HANDLE_VALUE)
			{
				DWORD byteswritten;
				WriteFile(hwritefile, data, iHeightmapSize, &byteswritten, NULL);
				CloseHandle(hwritefile);
				AddFileToBlock(1, heightmap_data_name.Get());
			}
			delete(data);
		}

	}

	// lightmap files
	if ( PathExist("lightmaps") == 1 ) 
	{
		AddFileToBlock (  1, "lightmaps\\objectlist.dat" );
		AddFileToBlock (  1, "lightmaps\\objectnummax.dat" );
		t.tnummaxfile_s=t.lightmapper.lmpath_s+"objectnummax.dat";
		if (  FileExist(t.tnummaxfile_s.Get()) == 1 ) 
		{
			OpenToRead (  1,t.tnummaxfile_s.Get() );
			t.temaxinfolder = ReadLong ( 1 );
			CloseFile (  1 );
		}
		else
		{
			t.temaxinfolder=4999;
		}
		for ( t.e = 0 ; t.e<=  (t.temaxinfolder*2)+100; t.e++ )
		{
			t.tname_s=t.lightmapper.lmpath_s+Str(t.e)+".dds";
			if (  FileExist(t.tname_s.Get()) == 1 ) 
			{
				AddFileToBlock (  1, cstr(cstr("lightmaps\\")+Str(t.e)+".dds").Get() );
			}
		}
		t.tfurthestobjnumber=g.lightmappedobjectoffset;
		SetDir (  "lightmaps" );
		ChecklistForFiles (  );
		for ( t.c = 1 ; t.c<=  ChecklistQuantity(); t.c++ )
		{
			t.tfile_s=ChecklistString(t.c);
			if (  t.tfile_s != "." && t.tfile_s != ".." ) 
			{
				if (  cstr(Lower(Right(t.tfile_s.Get(),4))) == ".dbo" ) 
				{
					t.tfile_s=Right(t.tfile_s.Get(),Len(t.tfile_s.Get())-Len("object"));
					t.tfile_s=Left(t.tfile_s.Get(),Len(t.tfile_s.Get())-4);
					t.tfilevalue = ValF(t.tfile_s.Get()) ; if (  t.tfilevalue>t.tfurthestobjnumber  )  t.tfurthestobjnumber = t.tfilevalue;
				}
			}
		}
		SetDir (  ".." );
		for ( t.tobj = g.lightmappedobjectoffset; t.tobj <= t.tfurthestobjnumber; t.tobj++ )
		{
			t.tname_s = ""; t.tname_s = t.tname_s + "lightmaps\\object"+Str(t.tobj)+".dbo";
			if (  FileExist(t.tname_s.Get()) == 1  )  AddFileToBlock (  1, cstr(cstr("lightmaps\\object")+Str(t.tobj)+".dbo").Get() );
		}
	}

	// ttsfiles files
	if ( PathExist("ttsfiles") == 1 ) 
	{
		SetDir ( "ttsfiles" );
		ChecklistForFiles (  );
		std::vector<cstr> ttsfileslist;
		ttsfileslist.clear();
		for ( t.c = 1 ; t.c <= ChecklistQuantity(); t.c++ )
		{
			t.tfile_s = ChecklistString(t.c);
			if ( t.tfile_s != "." && t.tfile_s != ".." ) 
			{
				if ( cstr(Lower(Right(t.tfile_s.Get(),4))) == ".txt" || cstr(Lower(Right(t.tfile_s.Get(),4))) == ".wav" || cstr(Lower(Right(t.tfile_s.Get(),4))) == ".lip") 
				{
					ttsfileslist.push_back(t.tfile_s);
				}
			}
		}
		SetDir ( ".." );
		for ( t.c = 0; t.c < ttsfileslist.size(); t.c++ )
		{
			AddFileToBlock ( 1, cstr(cstr("ttsfiles\\")+ttsfileslist[t.c]).Get() );
		}
	}

	//  visual settings
	if (  t.tincludevisualsfile == 1  )  AddFileToBlock (  1, "visuals.ini" );

	//  conkit data
	if (  FileExist("conkit.dat")  )  AddFileToBlock (  1,"conkit.dat" );

	//  ebe files
	ChecklistForFiles (  );
	for ( t.c = 1 ; t.c <= ChecklistQuantity(); t.c++ )
	{
		t.tfile_s = ChecklistString(t.c);
		if ( t.tfile_s != "." && t.tfile_s != ".." ) 
		{
			cstr strEnt = cstr(Lower(Right(t.tfile_s.Get(),4)));
			if ( strcmp ( strEnt.Get(), ".ebe" ) == NULL )
			{
				AddFileToBlock ( 1, t.tfile_s.Get() );
				cstr tNameOnly = Left(t.tfile_s.Get(),strlen(t.tfile_s.Get())-4);
				cstr tThisFile = tNameOnly + cstr(".fpe");
				if ( FileExist(tThisFile.Get()) ) AddFileToBlock ( 1, tThisFile.Get() );
				// wicked saves DBOs
				tThisFile = tNameOnly + cstr(".dbo");
				if ( FileExist(tThisFile.Get()) ) AddFileToBlock ( 1, tThisFile.Get() );
			}
		}
	}

	// cannot just discover EBE textures, so go through all entity parents for all EBEs and save their textures
	for ( int iEntID = 1; iEntID <= g.entidmaster; iEntID++ )
	{
		if (strlen(t.entitybank_s[iEntID].Get()) > 0)
		{
			if (t.entityprofile[iEntID].ebe.dwRLESize > 0)
			{	
				char pJustNameOfTex[MAX_PATH];
				strcpy(pJustNameOfTex, t.entityprofile[iEntID].texd_s.Get());
				pJustNameOfTex[strlen(pJustNameOfTex) - strlen("_color.dds")] = 0;
				cstr tThisFile = cstr(pJustNameOfTex) + "_color.dds";
				if ( FileExist(tThisFile.Get()) ) AddFileToBlock ( 1, tThisFile.Get() );
				tThisFile = cstr(pJustNameOfTex) + "_normal.dds";
				if ( FileExist(tThisFile.Get()) ) AddFileToBlock ( 1, tThisFile.Get() );
				tThisFile = cstr(pJustNameOfTex) + "_surface.dds";
				if ( FileExist(tThisFile.Get()) ) AddFileToBlock ( 1, tThisFile.Get() );
			}
		}
	}

	cstr terrainMaterialFile = g.mysystem.levelBankTestMap_s + "custommaterials.dat";
	SaveTerrainTextureFolder(terrainMaterialFile.Get());
	AddFileToBlock(1, "custommaterials.dat");

	SetDir ( pOldDir.Get() );
	SaveFileBlock ( 1 );

	// New automated backup system for FPM files (stored in destination var)
	#define AUTOSAVEBACKUP
	#ifdef AUTOSAVEBACKUP
	timestampactivity(0, "Auto-save a copy of every save to the mapbank\_automatedbackups folder");
	char automatedcopyfpm[MAX_PATH];
	char automatedcopyname[MAX_PATH];
	strcpy(automatedcopyfpm, "");
	strcpy(automatedcopyname, destination);
	for (int i = strlen(automatedcopyname)-8; i > 0; i--)
	{
		if (strnicmp(automatedcopyname + i, "mapbank\\", 8)==NULL || strnicmp(automatedcopyname + i, "mapbank/", 8) == NULL)
		{
			strcpy(automatedcopyfpm, automatedcopyname+i+8);
			automatedcopyname[i+8] = 0;
			break;
		}
	}
	if (strlen(automatedcopyfpm) > 0)
	{
		// replace \\ and chop ext
		for (int i = 0; i < strlen(automatedcopyfpm); i++)
		{
			if (automatedcopyfpm[i] == '\\' || automatedcopyfpm[i] == '/') automatedcopyfpm[i] = '_';
		}
		LPSTR pExtDot = strrchr(automatedcopyfpm, '.');
		if(pExtDot) *pExtDot = 0;

		// create folder
		strcpy(automatedcopyname, automatedcopyname);
		strcat(automatedcopyname, "_automatedbackups\\");
		CreateDirectoryA(automatedcopyname, NULL);

		// version num tracker file
		int iVersionNumber = 1;
		char pTrackerFile[MAX_PATH];
		strcpy(pTrackerFile, automatedcopyname);
		strcat(pTrackerFile, automatedcopyfpm);
		strcat(pTrackerFile, ".dat");
		if (FileExist(pTrackerFile) == 1)
		{
			OpenToRead(1, pTrackerFile);
			iVersionNumber = atoi(ReadString(1));
			CloseFile(1);
		}

		// create auto backup filename
		char pCheckFile[MAX_PATH];
		strcpy(pCheckFile, automatedcopyname);
		strcat(pCheckFile, automatedcopyfpm);
		strcat(pCheckFile, "_1");
		char* pNum = strrchr(pCheckFile, '_');
		if (pNum)
		{
			sprintf(pNum, "_%d", iVersionNumber);
			strcat(pCheckFile, ".fpm");
		}

		// find next available version number
		while(FileExist(pCheckFile) == 1)
		{
			char *pDot = strrchr(pCheckFile, '.');
			if (pDot)
			{
				char *pNum = strrchr(pCheckFile, '_');
				if (pNum && pNum < pDot)
				{
					int iNum = atoi(pNum + 1);
					iNum++;
					*pNum = 0;
					sprintf(pNum, "_%d", iNum);
					strcat(pCheckFile, ".fpm");
					iVersionNumber = iNum;
				}
			}
		}
		DeleteAFile(pCheckFile);
		CopyAFile(destination, pCheckFile);

		// save version tracker file for next time
		if (FileExist(pTrackerFile) == 1) DeleteFileA(pTrackerFile);
		{
			OpenToWrite(1, pTrackerFile);
			char pVersionNum[256];
			sprintf(pVersionNum, "%d", iVersionNumber);
			WriteString(1, pVersionNum);
			CloseFile(1);
		}

		// and remove older files so storage does not become an issue
		if(iVersionNumber > 9)
		{
			for (int i = 1; i < iVersionNumber - 9; i++)
			{
				strcpy(pCheckFile, automatedcopyname);
				strcat(pCheckFile, automatedcopyfpm);
				strcat(pCheckFile, "_");
				strcat(pCheckFile, Str(i));
				strcat(pCheckFile, ".fpm");
				DeleteAFile(pCheckFile);
			}
		}
	}
	#endif

	// collect ALL entity profile files
	g.filecollectionmax = 0;
	Undim (t.filecollection_s);
	Dim (t.filecollection_s, 500);
	void addthisentityprofilesfilestocollection (entityeleproftype * pEleProf);
	for (int entid = 1; entid <= g.entidmaster; entid++)
	{
		if (strlen(t.entitybank_s[entid].Get()) > 0)
		{
			t.e = 0; t.entid = entid;
			addthisentityprofilesfilestocollection (NULL);
		}
	}

	// create an itinery LST file (so auto updater can find and download dependent files)
	cstr LSTFile_s = cstr(Left(g.projectfilename_s.Get(), Len(g.projectfilename_s.Get()) - 4)) + ".lst";
	if (FileExist(LSTFile_s.Get()) == 1) DeleteAFile (LSTFile_s.Get());
	std::vector <cstr> lstlist_s;
	Dim (lstlist_s, g.filecollectionmax);
	int iListIndex = 0;
	for (int i = 0; i <= g.filecollectionmax; i++)
	{
		LPSTR pThisFile = t.filecollection_s[i].Get();
		int iThisSize = strlen (pThisFile);
		if (iThisSize  > 0)
		{
			// must have a file specified
			if (pThisFile[iThisSize - 1] == '\\' || pThisFile[iThisSize - 1] == '/')
			{
				// ignore folders
			}
			else
			{
				if (FileExist(pThisFile) == 1)
				{
					lstlist_s[iListIndex++] = pThisFile;
				}
			}
		}
	}
	SaveArray (LSTFile_s.Get(), lstlist_s);
	UnDim (lstlist_s);

	// save any changes to game collection list and ELE file
	extern preferences pref;
	save_rpg_system(pref.cLastUsedStoryboardProject, true);

	//  does crazy cool stuff
	t.tsteamsavefilename_s = t.ttempprojfilename_s;

	//  log prompts
	timestampactivity(0,"Saving FPM level file complete");
}

void mapfile_emptyebesfromtestmapfolder(bool bIgnoreValidTextureFiles)
{
	ChecklistForFiles();
	for (t.c = 1; t.c <= ChecklistQuantity(); t.c++)
	{
		t.tfile_s = ChecklistString(t.c);
		if (t.tfile_s != "." && t.tfile_s != "..")
		{
			// only if not a CUSTOM content piece
			if (strnicmp(t.tfile_s.Get(), "CUSTOM_", 7) != NULL)
			{
				cstr strEnt = cstr(Lower(Right(t.tfile_s.Get(), 4)));
				if (stricmp(strEnt.Get(), ".ebe") == NULL || stricmp(strEnt.Get(), ".fpe") == NULL)
				{
					DeleteAFile(t.tfile_s.Get());
					cstr tNameOnly = Left(t.tfile_s.Get(), strlen(t.tfile_s.Get()) - 4);
					cstr tThisFile = tNameOnly + cstr(".fpe");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());
					tThisFile = tNameOnly + cstr(".dbo");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());

					tThisFile = tNameOnly + cstr(".x");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());

					tThisFile = tNameOnly + cstr(".bmp");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());
					tThisFile = tNameOnly + cstr("_D.dds");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());
					tThisFile = tNameOnly + cstr("_N.dds");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());
					tThisFile = tNameOnly + cstr("_S.dds");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());

					tThisFile = tNameOnly + cstr("_D.jpg");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());
					tThisFile = tNameOnly + cstr("_N.jpg");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());
					tThisFile = tNameOnly + cstr("_S.jpg");
					if (FileExist(tThisFile.Get()) == 1) DeleteAFile(tThisFile.Get());

				}
				strEnt = cstr(Lower(Right(t.tfile_s.Get(), 6)));
				if (bIgnoreValidTextureFiles == false)
				{
					if (strcmp(strEnt.Get(), "_d.dds") == NULL || strcmp(strEnt.Get(), "_n.dds") == NULL || strcmp(strEnt.Get(), "_s.dds") == NULL
						|| strcmp(strEnt.Get(), "_d.jpg") == NULL || strcmp(strEnt.Get(), "_n.jpg") == NULL || strcmp(strEnt.Get(), "_s.jpg") == NULL)
					{
						DeleteAFile(t.tfile_s.Get());
					}
				}
			}
		}
	}
}

void mapfile_emptyterrainfilesfromtestmapfolder ( void )
{
	// also makes sense to empty terrain files too as we are clearing this folder for new activity
	// move to terrain node folder location
	cstr pOldDir = GetDir();
	char pRealWritableArea[MAX_PATH];
	strcpy(pRealWritableArea, pOldDir.Get());
	strcat(pRealWritableArea, "\\levelbank\\testmap\\");
	GG_GetRealPath(pRealWritableArea, 1);
	SetDir(pRealWritableArea);
	// all node folders (containing terrain geometry and virtual textures)
	ChecklistForFiles (  );
	std::vector<std::string> terrainNodeFolders;
	terrainNodeFolders.clear();
	for ( t.c = 1 ; t.c <= ChecklistQuantity(); t.c++ )
	{
		t.tfile_s = ChecklistString(t.c);
		if ( t.tfile_s != "." && t.tfile_s != ".." && t.tfile_s != "ttsfiles" ) 
		{
			if (ChecklistValueA(t.c) == 1)
			{
				if (cstr(Lower(Left(t.tfile_s.Get(), 2))) == "tt")
				{
					// found terrain node folder
					terrainNodeFolders.push_back(t.tfile_s.Get());
				}
			}
		}
	}
	for (int tt = 0; tt < terrainNodeFolders.size(); tt++)
	{
		LPSTR pTerrainNodeFolder = (LPSTR)terrainNodeFolders[tt].c_str();
		SetDir(pTerrainNodeFolder);
		ChecklistForFiles();
		for (t.c = 1; t.c <= ChecklistQuantity(); t.c++)
		{
			t.tfile_s = ChecklistString(t.c);
			if (t.tfile_s != "." && t.tfile_s != "..")
			{
				DeleteAFile ( t.tfile_s.Get() );
			}
		}
		SetDir("..");
		RemoveDirectoryA(pTerrainNodeFolder);
	}
	SetDir(pOldDir.Get());
}

void mapfile_emptylightmapandttsfilesfolder_wicked( void )
{
	//PE: lightmaps
	cstr pOldDir = GetDir();
	char pRealWritableArea[MAX_PATH];
	strcpy(pRealWritableArea, pOldDir.Get());
	if( pestrcasestr(pOldDir.Get(), "\\testmap"))
		strcat(pRealWritableArea, "\\lightmaps");
	else
		strcat(pRealWritableArea, "\\levelbank\\testmap\\lightmaps");
	GG_GetRealPath(pRealWritableArea, 1);
	if (PathExist(pRealWritableArea))
	{
		SetDir(pRealWritableArea);
		ChecklistForFiles();
		for (t.c = 1; t.c <= ChecklistQuantity(); t.c++)
		{
			t.tfile_s = ChecklistString(t.c);
			if (t.tfile_s != "." && t.tfile_s != "..")
			{
				if (FileExist(t.tfile_s.Get()) == 1)  DeleteAFile(t.tfile_s.Get());
			}
		}
		SetDir(pOldDir.Get());
		RemoveDirectoryA(pRealWritableArea);
	}
	//PE: ttsfiles
	strcpy(pRealWritableArea, pOldDir.Get());
	if (pestrcasestr(pOldDir.Get(), "\\testmap"))
		strcat(pRealWritableArea, "\\ttsfiles");
	else
		strcat(pRealWritableArea, "\\levelbank\\testmap\\ttsfiles");
	GG_GetRealPath(pRealWritableArea, 1);
	if (PathExist(pRealWritableArea))
	{
		SetDir(pRealWritableArea);
		ChecklistForFiles();
		for (t.c = 1; t.c <= ChecklistQuantity(); t.c++)
		{
			t.tfile_s = ChecklistString(t.c);
			if (t.tfile_s != "." && t.tfile_s != "..")
			{
				if (FileExist(t.tfile_s.Get()) == 1)  DeleteAFile(t.tfile_s.Get());
			}
		}
		SetDir(pOldDir.Get());
		RemoveDirectoryA(pRealWritableArea);
	}
	//PE: Somehow it was not reverted, added here.
	SetDir(pOldDir.Get());
}

void mapfile_loadproject_fpm ( void )
{
	// GGMAX 3.55: drop any bullet tracers still in flight from the OUTGOING level. Tracers
	// are aged out by Tracers::Update() which only runs from the draw hook, so without this
	// a streak spawned moments before a level swap survives and draws into the new level.
	Tracers::ClearLevel();

	//PE: Deselect any objects, if we load a level with less entityties then t.widget.pickedEntityIndex it can crash.
	t.widget.pickedEntityIndex = 0;
	t.gridentity = 0;

	// can do extra steps when load in FPM
	bool bThisIsTheNewTerrainSystem = false;

	//  Ensure FPM exists
	t.trerfeshvisualsassets=0;
	timestampactivity(0, cstr(cstr("_mapfile_loadproject_fpm: ")+g.projectfilename_s+" "+GetDir()).Get() );
	if ( FileExist(g.projectfilename_s.Get()) == 1 ) 
	{
		//  Empty the lightmap folder
		timestampactivity(0,"LOADMAP: mapfile_emptylightmapandttsfilesfolder_wicked");
		mapfile_emptylightmapandttsfilesfolder_wicked();

		// empty any terrain node files
		mapfile_emptyterrainfilesfromtestmapfolder();

		//  Store and switch folders
		t.tdirst_s=GetDir() ; SetDir ( g.mysystem.levelBankTestMap_s.Get() ); // "levelbank\\testmap\\" );

		// Delete key testmap file (if any)
		if ( FileExist("header.dat") == 1 ) DeleteAFile ( "header.dat" );
		if ( FileExist("playerconfig.dat") == 1 ) DeleteAFile ( "playerconfig.dat" );
		if ( FileExist("watermask.dds") == 1 ) DeleteAFile ( "watermask.dds" );
		if ( FileExist("watermask.png") == 1 ) DeleteAFile ( "watermask.png" );
		if ( FileExist("vegmask.png") == 1) DeleteAFile( "vegmask.png"); //PE: If we switch from a new fpm to a old only with .dds, old need to be removed.
		if ( FileExist("visuals.ini") == 1 ) DeleteAFile ( "visuals.ini" );
		if ( FileExist("conkit.dat") == 1 ) DeleteAFile ( "conkit.dat" );
		if ( FileExist("map.obs") == 1 ) DeleteAFile ( "map.obs" );
		if ( FileExist("locked.cfg") == 1) DeleteAFile("locked.cfg");

		if (FileExist("cfg.cfg") == 1) DeleteAFile("cfg.cfg");

		// new terrain system loads its data settings in this file (see below)
		if (FileExist("ggterrain.dat") == 1) DeleteAFile("ggterrain.dat");
		uint32_t terrain_sculpt_size = GGTerrain::GGTerrain_GetSculptDataSize();
		cstr sculpt_data_name = cstr( (int) terrain_sculpt_size) + cstr(".dat");
		if (FileExist(sculpt_data_name.Get()) == 1) DeleteAFile(sculpt_data_name.Get());

		uint32_t terrain_paint_size = GGTerrain::GGTerrain_GetPaintDataSize();
		cstr paint_data_name = cstr((int)terrain_paint_size) + cstr(".ptd");
		if (FileExist(paint_data_name.Get()) == 1) DeleteAFile(paint_data_name.Get());

		uint32_t tree_data_size = GGTrees::GGTrees_GetDataSize() * 4;
		cstr tree_data_name = cstr((int)tree_data_size) + cstr(".tre");
		if (FileExist(tree_data_name.Get()) == 1) DeleteAFile(tree_data_name.Get());

		cstr old_tree_data_name = "4800000.tre"; //PE: Tree format 1.0. Make sure it deleted.
		if (FileExist(old_tree_data_name.Get()) == 1) DeleteAFile(old_tree_data_name.Get());

		cstr old_size_tree_data_name = "4800004.tre"; // 400,000 tree slots
		if (FileExist(old_size_tree_data_name.Get()) == 1) DeleteAFile(old_size_tree_data_name.Get());

		uint32_t grass_data_size = GGGrass::GGGrass_GetDataSize();
		cstr grass_data_name = cstr((int)grass_data_size) + cstr(".gra");
		if (FileExist(grass_data_name.Get()) == 1) DeleteAFile(grass_data_name.Get());

		if (FileExist("heightmapdata.raw") == 1) DeleteAFile("heightmapdata.raw");

		//  Delete env map for PBR (if any)
		if ( FileExist("globalenvmap.dds") == 1 ) DeleteAFile ( "globalenvmap.dds" );

		if (FileExist("custommaterials.dat") == 1) DeleteAFile("custommaterials.dat");

		LPSTR pTerrainPreference = "TerrainPreference.tmp";
		if (FileExist(pTerrainPreference) == 1) DeleteFileA(pTerrainPreference);

		// empty any ebe files
		mapfile_emptyebesfromtestmapfolder(false);

		//  Restore folder to Files (for extraction)
		SetDir ( t.tdirst_s.Get() );

		//  Read FPM into testmap area
		timestampactivity(0,"LOADMAP: read FPM block");
		t.tpath_s=g.mysystem.levelBankTestMap_s.Get(); //"levelbank\\testmap\\";

		if ( !ExtractZipThread::IsSetup() )
		{
			SYSTEM_INFO sysinfo;
			GetSystemInfo( &sysinfo );
			uint32_t numThreads = sysinfo.dwNumberOfProcessors;
			if ( numThreads > 3 ) numThreads--;
			ExtractZipThread::SetThreads( numThreads );
		}

		OpenFileBlock (  g.projectfilename_s.Get(),1,"mypassword" );
		uint32_t numFiles = GetFileBlockNumFiles( 1 );
		const char* const* pAllFiles = GetFileBlockAllFileNames( 1 );
		const unsigned long* pAllSizes = GetFileBlockAllFileSizes( 1 );
		ExtractZipThread::SetWork( g.projectfilename_s.Get(), t.tpath_s.Get(), pAllFiles, pAllSizes, numFiles );
		ExtractZipThread::StartThreads();
		ExtractZipThread::WaitForAll();
		CloseFileBlock( 1 ); // must remain open until all work completed for pAllFiles to remain valid

		SetDir ( g.mysystem.levelBankTestMapAbs_s.Get() );

		//  If file still not present, extraction failed
		if (  FileExist("header.dat") == 0 ) 
		{
			//  inform user the FPM could not be loaded (corrupt file)
			t.tloadsuccessfully=0;
		}

		// If file still not present, extraction failed
		SetDir ( g.mysystem.levelBankTestMapAbs_s.Get() );		
		if ( g.memskipwatermask == 0 && (FileExist("watermask.dds") == 0 || FileExist("watermask.png") == 0) )
		{
			//  Only Reloaded Formats have this texture file, so fail load if not there (Classic FPM)
			t.tloadsuccessfully=2;
		}

		// new terrain system loads its data settings
		//PE: We are already inside "levelbank\\testmap\\" ?
		//PE: In editor g.mysystem.levelBankTestMap_s have "c:" and works, in standalone we dont have that so:
		cstr TerrainDataFile_s = "ggterrain.dat";
		g_bNeedToConvertClassicPositionsToMAX = false;
		if (FileExist(TerrainDataFile_s.Get()) == 0) g_bNeedToConvertClassicPositionsToMAX = true;
		GGTerrainFile_LoadTerrainData(TerrainDataFile_s.Get(),false);
		extern bool bTreeGlobalInit;
		bTreeGlobalInit = false;

		//PE: Restore Sculpt Data.
		char* data;
		extern int g_iDisableTerrainSystem;
		if (g_iDisableTerrainSystem == 0)
		{
			terrain_sculpt_size = GGTerrain::GGTerrain_GetSculptDataSize();
			data = new char[terrain_sculpt_size];
			if (data)
			{
				cstr sculpt_data_name = cstr((int)terrain_sculpt_size) + cstr(".dat");
				if (FileExist(sculpt_data_name.Get()) == 1)
				{
					char FilenameString[_MAX_PATH];
					strcpy(FilenameString, sculpt_data_name.Get());
					if (DB_FileExist(FilenameString))
					{
						// Open file to be read
						HANDLE hreadfile = GG_CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (hreadfile != INVALID_HANDLE_VALUE)
						{
							// Read file into memory
							DWORD bytesread;
							ReadFile(hreadfile, data, terrain_sculpt_size, &bytesread, NULL);
							CloseHandle(hreadfile);
							GGTerrain::GGTerrain_SetSculptData(terrain_sculpt_size, (uint8_t*)data);
						}
					}
				}
				delete(data);
			}

			void check_new_terrain_parameters(void);
			check_new_terrain_parameters();

			void reset_terrain_paint_date(void);
			reset_terrain_paint_date(); //PE: Reset old paint textures, after this. try loading from saved fpm.

			if (FileExist("custommaterials.dat") == 1)
			{
				LoadTerrainTextureFolder("custommaterials.dat");
			}

			// GGMAX 3.68: drop the grass material cache for the OUTGOING level before this one's
			// paint data is applied. It is lazy and rebuilds on first sighting of each type, so the
			// incoming level reloads exactly the DDS set it paints - instead of inheriting the union
			// of every level visited this session (4 resident at boot, 40 by load 20, ~47 MB).
			GGTerrain::GGTerrainWicked_ReleaseGrassMaterials();

			//PE: Restore Paint Texture Data.
			terrain_paint_size = GGTerrain::GGTerrain_GetPaintDataSize();
			data = new char[terrain_paint_size];
			if (data)
			{
				// GGMAX 3.65 VRAM LEAK (root cause): pMaterialMap is a PERSISTENT 4096x4096 buffer
				// that lives for the process, and the restore below only writes it when this level
				// ships a .ptd. A level with no painted terrain therefore INHERITED the previous
				// level's entire paint map - and SetupWickedTerrainMaterials scans that map to
				// decide which materials to instantiate, so the new level created material entities,
				// and loaded the Color/Normal/Surface DDS set, for every material the PREVIOUS level
				// had painted. Measured with the VRAM census: Operation Amazon loads 11 terrain
				// materials cold, but 20 after one visit to RPG Template - the extra nine are
				// mat14-mat28, which it does not use, at +126 MB. Over a session it converges on the
				// union of every level visited (32 materials, ~430 MB).
				//
				// This was never only a memory bug: the stale paint also feeds the blend/slot
				// mapping, so an unpainted level was being set up against another level's paint.
				bool bAppliedPaintData = false;
				cstr paint_data_name = cstr((int)terrain_paint_size) + cstr(".ptd");
				if (FileExist(paint_data_name.Get()) == 1)
				{
					char FilenameString[_MAX_PATH];
					strcpy(FilenameString, paint_data_name.Get());
					if (DB_FileExist(FilenameString))
					{
						// Open file to be read
						HANDLE hreadfile = GG_CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (hreadfile != INVALID_HANDLE_VALUE)
						{
							// Read file into memory
							DWORD bytesread;
							ReadFile(hreadfile, data, terrain_paint_size, &bytesread, NULL);
							CloseHandle(hreadfile);
							GGTerrain::GGTerrain_SetPaintData(terrain_paint_size, (uint8_t*)data);
							bAppliedPaintData = true;
							//PE: We already made a delay invalidate region so all fine...
						}
					}
				}
				if (!bAppliedPaintData)
				{
					// No paint data for this level: the map must be EMPTY, not whatever the last
					// level left behind. ResetPaintData memsets + re-uploads + invalidates textures,
					// the same tail SetPaintData runs; the notify is what SetPaintData additionally
					// does and is what makes the Wicked terrain re-scan and drop the stale slots.
					GGTerrain::GGTerrain_ResetPaintData();
					GGTerrain::GGTerrainWicked_OnPaintDataChanged();
				}
				delete(data);
			}

			//PE: Default hide all tree's.
			GGTrees::GGTrees_HideAll();

			//PE: Restore Tree Data.
			tree_data_size = GGTrees::GGTrees_GetDataSize() * 4;
			data = new char[tree_data_size];
			if (data)
			{
				cstr tree_data_name = cstr((int)tree_data_size) + cstr(".tre");
				cstr old_tree_data_name = "4800000.tre"; //PE: Tree format 1.0

				// a level saved with 400,000 tree slots loads into the first slots, the rest stay hidden
				GGTrees::GGTrees_GetEmptyData((float*)data);
				cstr old_size_tree_data_name = "4800004.tre";
				if (FileExist(tree_data_name.Get()) == 0 && FileExist(old_size_tree_data_name.Get()) == 1)
				{
					tree_data_name = old_size_tree_data_name;
				}

				bool bConvertOldFormat = false;
				if (FileExist(old_tree_data_name.Get()) == 1)
				{
					bConvertOldFormat = true;
				}
				if (FileExist(tree_data_name.Get()) == 1 || bConvertOldFormat)
				{
					char FilenameString[_MAX_PATH];
					strcpy(FilenameString, tree_data_name.Get());
					if (bConvertOldFormat)
					{
						strcpy(FilenameString, old_tree_data_name.Get());
						uint32_t* dataInt = (uint32_t*)data;
						dataInt[0] = 2; //PE: New version
						data += 4;
						tree_data_size -= 4;
					}
					if (DB_FileExist(FilenameString))
					{
						// Open file to be read
						HANDLE hreadfile = GG_CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (hreadfile != INVALID_HANDLE_VALUE)
						{
							// Read file into memory
							DWORD bytesread;
							ReadFile(hreadfile, data, tree_data_size, &bytesread, NULL);
							CloseHandle(hreadfile);
							if (!bConvertOldFormat)
							{
								//PE: Skip conversion from 2.0 to 3.0 as the 2.0 using 3.0 data have been out for some time now. and people would already have saved 3.0 trees with 2.0 version.
								//PE: Mainly so people will not have to correct the trees again. No way to see if 3.0 data has been saved as 2.0 now.
								uint32_t* dataInt = (uint32_t*)data;
								if (dataInt[0] == 2) dataInt[0] = 3;
							}
							if (bConvertOldFormat)
							{
								int size = GGTrees::GGTrees_GetDataSize();
								size--; //PE: Remove version.
								uint32_t* dataInt = (uint32_t*)data;
								int scale = 85; //PE: 85=1.0 hlsl: GetTreeScale( uint data ) { return ((data >> 16) & 0xFF) / 170.0 + 0.5; }

								for (int i = 0; i < size; i += 3)
								{
									//PE: Convert old tree data to new 2.0 format.
									uint32_t data = dataInt[2 + i];
									uint32_t type = (data & 0xF);
									uint32_t visible = (data & 0x100);
									uint32_t id = (data >> 10);
									uint32_t scaleindex = (data >> 4) & 0x7; //hlsl: uint index = (IN.data >> 4) & 0x7;
									float randScale = (scaleindex / 7.0) * 0.25 + 0.75; //hlsl. old scale.
									int newscale = (int)((float)(57 + (scaleindex * 4.0)) * randScale);
									type &= 0x1F;
									uint32_t varIndex = id & 0x7;
									data = (type << 11) | (varIndex << 8);
									if (visible) data |= 0x1;
									else data &= ~0x1;
									dataInt[2 + i] = data;
									dataInt[2 + i] = (dataInt[2 + i] & 0xFF00FFFF) | (newscale << 16);
								}
								data -= 4;
							}
							GGTrees::GGTrees_SetData((float*)data);
						}
						else if (bConvertOldFormat)
						{
							data -= 4;
						}
					}
					else if (bConvertOldFormat)
					{
						data -= 4;
					}
				}
				delete(data);
			}


			//PE: Restore grass Data.
			grass_data_size = GGGrass::GGGrass_GetDataSize();
			data = new char[grass_data_size];
			if (data)
			{
				cstr grass_data_name = cstr((int)grass_data_size) + cstr(".gra");
				if (FileExist(grass_data_name.Get()) == 1)
				{
					char FilenameString[_MAX_PATH];
					strcpy(FilenameString, grass_data_name.Get());
					if (DB_FileExist(FilenameString))
					{
						// Open file to be read
						HANDLE hreadfile = GG_CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (hreadfile != INVALID_HANDLE_VALUE)
						{
							// Read file into memory
							DWORD bytesread;
							ReadFile(hreadfile, data, grass_data_size, &bytesread, NULL);
							CloseHandle(hreadfile);
							GGGrass::GGGrass_SetData(grass_data_size, (uint8_t*)data);
						}
					}
				}
				delete(data);
			}
			//PE: Make sure to trigger a delayed update, so everything is placed correctly, grass/trees need height.
			extern int iTriggerInvalidateAfterFrames;
			iTriggerInvalidateAfterFrames = 60; //PE: Height data need to be set first, then we can adjust grass/trees/virtual texture.

		}

		//  load in visuals from loaded file
		timestampactivity(0,"LOADMAP: load in visuals");
		// GGMAX 3.81b DIAGNOSTIC: this guard tests a RELATIVE path while the visuals_load()
		// it guards reads an ABSOLUTE one (cFullWritePath + "visuals.ini"). Record what the
		// guard decided and where it was standing when it decided it.
		extern int g_ggVisualsGuardRan; extern char g_ggVisualsGuardCwd[512];
		g_ggVisualsGuardRan = (FileExist("visuals.ini") == 1) ? 1 : 0;
		{ char* cwdp = _getcwd(g_ggVisualsGuardCwd, 511); (void)cwdp; }
		if (  FileExist("visuals.ini") == 1 ) 
		{
			t.tstorefpscrootdir_s=g.fpscrootdir_s;
			g.fpscrootdir_s="" ; visuals_load ( );
			g_ggReflTrace[0] = t.visuals.bReflectionsEnabled ? 1 : 0;   // GGMAX 3.81c
			g.fpscrootdir_s=t.tstorefpscrootdir_s;
			t.trerfeshvisualsassets=1;

			//  Ensure visuals settings are copied to gamevisuals (the true destination)
			t.gamevisuals=t.visuals;
			t.editorvisuals=t.visuals;

			//  And ensure editor visuals mimic required settings from loaded data
			g_ggReflTrace[1] = t.visuals.bReflectionsEnabled ? 1 : 0;   // GGMAX 3.81c
			visuals_editordefaults ( );
			t.visuals=t.editorvisuals;
			g_ggReflTrace[2] = t.visuals.bReflectionsEnabled ? 1 : 0;   // GGMAX 3.81c
			t.visuals.skyindex=t.gamevisuals.skyindex;
			t.visuals.sky_s=t.gamevisuals.sky_s;
			t.visuals.terrainindex=t.gamevisuals.terrainindex;
			t.visuals.terrain_s=t.gamevisuals.terrain_s;
			t.visuals.vegetationindex=t.gamevisuals.vegetationindex;
			t.visuals.vegetation_s=t.gamevisuals.vegetation_s;

			//  Re-acquire indices now the lists have changed
			//  takes visuals.sky$ visuals.terrain$ visuals.vegetation$
			visuals_updateskyterrainvegindex ( );
			t.gamevisuals.skyindex=t.visuals.skyindex;
			t.gamevisuals.terrainindex=t.visuals.terrainindex;
			t.gamevisuals.vegetationindex=t.visuals.vegetationindex;
		}


		uint32_t iHeightmapSize = 0, iHeightmapWidth = 0, iHeightmapHeight = 0;
		iHeightmapWidth = t.gamevisuals.iHeightmapWidth;
		iHeightmapHeight = t.gamevisuals.iHeightmapHeight;
		iHeightmapSize = iHeightmapWidth * iHeightmapHeight * sizeof(uint16_t);

		//PE: Load heightmap data if any.
		if (iHeightmapSize > 0 && iHeightmapWidth > 0 && iHeightmapHeight > 0)
		{
			data = new char[iHeightmapSize];
			if (data)
			{
				cstr heightmap_data_name = "heightmapdata.raw";
				if (FileExist(heightmap_data_name.Get()) == 1)
				{
					char FilenameString[_MAX_PATH];
					strcpy(FilenameString, heightmap_data_name.Get());
					if (DB_FileExist(FilenameString))
					{
						// Open file to be read
						HANDLE hreadfile = GG_CreateFile(FilenameString, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (hreadfile != INVALID_HANDLE_VALUE)
						{
							// Read file into memory
							DWORD bytesread;
							ReadFile(hreadfile, data, iHeightmapSize, &bytesread, NULL);
							CloseHandle(hreadfile);
							GGTerrain::GGTerrain_SetHeightmapData((uint16_t*) data, iHeightmapWidth, iHeightmapHeight);
						}
					}
				}
				delete(data);
			}
		}

		// must trigger the terrain textures to update after visuals.ini load in terrain texture preferences
		if (strlen(t.visuals.sTerrainTextures[0].Get()) > 0)
		{
			extern std::vector<std::string> g_DeferTextureUpdate;
			g_DeferTextureUpdate.clear();
			g_DeferTextureUpdate.reserve(32);
			for (int i = 0; i < 32; i++)
			{
				std::string texture = t.visuals.sTerrainTextures[i].Get();
				g_DeferTextureUpdate.push_back(texture);
			}
			extern int g_iDeferTextureUpdateToNow;
			g_iDeferTextureUpdateToNow = 2;
		}


		//  if MAP OBS exists, we are not generating OBS data this time
		t.aisystem.generateobs=1;
		t.tobsfile_s="map.obs";
		if (  FileExist(t.tobsfile_s.Get()) == 1 ) 
		{
			t.aisystem.generateobs=0;
		}

		//  if CFG file present, copy to editor folder for later use (stores FPG for us)
		if (  t.game.runasmultiplayer == 0 ) 
		{
			//  single player/editor only - not needed when loading multiplayer map
			if (  t.tloadsuccessfully == 1 ) 
			{
				t.tttfile_s="cfg.cfg";
				if (  FileExist(t.tttfile_s.Get()) == 1 ) 
				{
					cstr cfgfile_s = g.mysystem.editorsGrideditAbs_s + t.tttfile_s;
					if ( FileExist( cfgfile_s.Get() ) == 1  )  DeleteAFile ( cfgfile_s.Get() );
					CopyAFile ( t.tttfile_s.Get(), cfgfile_s.Get() );
				}

				//PE: Load locked objects.
				extern std::vector<sRubberBandType> vEntityLockedList;
				vEntityLockedList.clear();
				if (FileExist("locked.cfg") == 1)
				{
					OpenToRead(1, "locked.cfg");
					int iAll = ReadLong(1);
					for (int i = 0; i < iAll; i++)
					{
						int e = ReadLong(1);
						sRubberBandType vEntityLockedItem;
						vEntityLockedItem.e = e;
						vEntityLockedList.push_back(vEntityLockedItem);
					}
					CloseFile(1);
				}
				// LB: The above list MAY carry around corrupt references, might be an idea
				// to scan and sanitise this agains the known entityelement and object list to ensure
				// bad refernces will not cause out of bounds errors. This will happen for older levels
				// that exploited a bug that caused objects to be changed to unlocked, but stayed in
				// the locked list and not removed, later being adopted by new objects added to level.
				// done later when editlock flags being set and know final size of entityelement array
				// search for gridedit_load_map with comment: "Restore locked state. from locked.cfg"
			}
		}

		// Retore and switch folders
		SetDir ( t.tdirst_s.Get() );

		//  if visuals file present, apply it
		g_ggReflTrace[3] = t.visuals.bReflectionsEnabled ? 1 : 0;   // GGMAX 3.81c
		timestampactivity(0,"LOADMAP: apply visuals");
		if (  t.trerfeshvisualsassets == 1 ) 
		{
			//  if loading from game (level load), ensure it's the game visuals we use
			if (  t.game.gameisexe == 1 || t.game.runasmultiplayer == 1  )  t.visuals = t.gamevisuals;
			//  and refresh assets based on restore
			t.visuals.refreshshaders=1;
			t.visuals.refreshterraintexture=1;
			t.visuals.refreshvegtexture=1;

			//PE: Remember sun angle.
			float oSx = t.visuals.SunAngleX;
			float oSy = t.visuals.SunAngleY;
			float oSz = t.visuals.SunAngleZ;

			//PE: Special wicked setup so retain visuals.
			t.visuals.refreshskysettings = 0;
			g.skyindex = t.visuals.skyindex; if (g.skyindex > g.skymax)  g.skyindex = g.skymax;
			t.visuals.sky_s = t.skybank_s[g.skyindex];
			t.terrainskyspecinitmode = 0;
			sky_skyspec_init( false );
			t.sky.currenthour_f = 8.0;
			t.sky.daynightprogress = 0;
			t.sky.changingsky = 1;

			// if change sky, regenerate env map
			timestampactivity(0, "LOADMAP: cubemap_generateglobalenvmap");
			cubemap_generateglobalenvmap();
			timestampactivity(0, "LOADMAP: visuals_loop");
			visuals_loop();

			//PE: In wicked we want to restore the sun angle from the map and not use skyspec.ini settings. (only when loading a old level).
			timestampactivity(0, "LOADMAP: Wicked_Update_Visuals");
			if (t.visuals.skyindex == 0 || t.visuals.bDisableSkybox)
			{
				//PE: Only if we re not using a simple skybox.
				t.visuals.SunAngleX = oSx;
				t.visuals.SunAngleY = oSy;
				t.visuals.SunAngleZ = oSz;
			}
			extern void Wicked_Update_Visuals(void *voidvisual);
			Wicked_Update_Visuals((void*) &t.visuals );
		}
	}
	else
	{
		t.tloadsuccessfully=0;
	}

	// fire up light probe update when get new level loaded
	extern bool g_bLightProbeScaleChanged;
	g_bLightProbeScaleChanged = true;

	// when finished loading an OLD LEVEL FROM CLASSIC, need to copy and convert assets to make it a MAX level
	if ( t.tloadsuccessfully != 0 && g_bAllowBackwardCompatibleConversion == true)
	{
		// this ensures when level files acted on, they can be used in MAX
		timestampactivity(0, "LOADMAP: mapfile_convertCLASSICtoMAX");
		mapfile_convertCLASSICtoMAX(g.projectfilename_s.Get());
	}
}

void mapfile_newmap ( void )
{
	//  Defaults
	t.layermax=20 ; t.maxx=500 ; t.maxy=500;
	t.olaylistmax=100;

	// when new map called, empty all terrain files 
	mapfile_emptyterrainfilesfromtestmapfolder();
}

void mapfile_loadmap ( void )
{
	// Load header data (need main mapdata for visdata)
	t.filename_s=t.levelmapptah_s+"header.dat";
	if (  FileExist(t.filename_s.Get()) == 1 ) 
	{
		// Header - version number
		OpenToRead (  1,t.filename_s.Get() );
		t.versionmajor = ReadLong ( 1 );
		t.versionminor = ReadLong ( 1 );
		CloseFile (  1 );
	}

	// 080917 - if old header, delete map.obj as it contains corrupt waypoint data
	if ( t.versionmajor < 1 )
	{
		//LPSTR pOldDir = GetDir();
		//LPSTR pObstacleWaypointData = "levelbank\\testmap\\map.obs";
		//if ( FileExist ( pObstacleWaypointData ) == 1 ) DeleteFile ( pObstacleWaypointData );
		cstr obstacleWaypointData_s = g.mysystem.levelBankTestMap_s+"map.obs";
		if ( FileExist ( obstacleWaypointData_s.Get() ) == 1 ) DeleteFileA ( obstacleWaypointData_s.Get() );
	}
}

void mapfile_savemap ( void )
{
	// Store old folder
	t.old_s=GetDir();

	// Enter folder
	SetDir ( g.mysystem.levelBankTestMap_s.Get() ); //"levelbank\\testmap\\" );

	// Clear old files out (TEMP)
	if (  FileExist("header.dat") == 1  )  DeleteAFile (  "header.dat" );

	// Create header file
	OpenToWrite (  1,"header.dat" );

	// Version 0.0 = Reloaded
	// Version 1.0 = GameGuru DX11 (new obstacle data save fixes)
	t.versionmajor = 1; WriteLong ( 1, t.versionmajor );
	t.versionminor = 0; WriteLong ( 1, t.versionminor );

	// end of header
	CloseFile (  1 );

	// Restore
	SetDir (  t.old_s.Get() );
}

void mapfile_loadplayerconfig ( void )
{
	//  Load player settings
	t.filename_s=t.levelmapptah_s+"playerconfig.dat";
	if (  FileExist(t.filename_s.Get()) == 1 ) 
	{
		//  Reloaded Header
		OpenToRead (  1,t.filename_s.Get() );

		//  verion header
		t.tmp_s = ReadString ( 1 );
		t.tversion = ReadLong ( 1 );

		//  player settings
		if (  t.tversion >= 201006 ) 
		{
			t.a_f = ReadFloat ( 1 ); t.playercontrol.jumpmax_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.gravity_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.fallspeed_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.climbangle_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.footfallpace_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.wobblespeed_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.wobbleheight_f=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.accel_f=t.a_f;
		}

		//  extra player settings
		if (  t.tversion >= 20100651 ) 
		{
			t.a_f = ReadFloat ( 1 ); t.playercontrol.regenrate=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.regenspeed=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.regendelay=t.a_f;
		}

		//  extra player settings for third person (V1.01.002)
		if (  t.tversion >= 20100652 ) 
		{
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.enabled=t.a;
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.startmarkere=t.a;
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.charactere=t.a;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.thirdperson.cameradistance=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.thirdperson.cameraheight=t.a_f;
			t.a_f = ReadFloat ( 1 ); t.playercontrol.thirdperson.cameraspeed=t.a_f;
		}
		if (  t.tversion >= 20100653 ) 
		{
			t.a_f = ReadFloat ( 1 ); t.playercontrol.thirdperson.camerafocus=t.a_f;
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.cameralocked=t.a;
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.camerashoulder=t.a;
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.camerafollow=t.a;
			t.a = ReadLong ( 1 ); t.playercontrol.thirdperson.camerareticle=t.a;
		}

		CloseFile (  1 );
	}

	// No third person mode in MAX
	t.playercontrol.thirdperson.enabled = 0;
}

void mapfile_saveplayerconfig ( void )
{
	//  Store old folder
	t.old_s=GetDir();

	//  Enter folder
	SetDir ( g.mysystem.levelBankTestMap_s.Get() ); // "levelbank\\testmap\\" );

	//  Version for player config has minor value (between betas)
	t.gtweakversion=20100653;

	//  WriteLong (  out )
	if (  FileExist("playerconfig.dat") == 1  )  DeleteAFile (  "playerconfig.dat" );
	OpenToWrite (  1,"playerconfig.dat" );

	//  verion header
	WriteString (  1,"version" );
	WriteLong (  1,t.gtweakversion );

	//  player settings
	WriteFloat (  1,t.playercontrol.jumpmax_f );
	WriteFloat (  1,t.playercontrol.gravity_f );
	WriteFloat (  1,t.playercontrol.fallspeed_f );
	WriteFloat (  1,t.playercontrol.climbangle_f );
	WriteFloat (  1,t.playercontrol.footfallpace_f );
	WriteFloat (  1,t.playercontrol.wobblespeed_f );
	WriteFloat (  1,t.playercontrol.wobbleheight_f );
	WriteFloat (  1,t.playercontrol.accel_f );

	//  extra settings from V1.0065 (20100651)
	WriteFloat (  1,t.playercontrol.regenrate );
	WriteFloat (  1,t.playercontrol.regenspeed );
	WriteFloat (  1,t.playercontrol.regendelay );

	//  extra settings from V1.01.030 (20100652)
	WriteLong (  1,t.playercontrol.thirdperson.enabled );
	WriteLong (  1,t.playercontrol.thirdperson.startmarkere );
	WriteLong (  1,t.playercontrol.thirdperson.charactere );
	WriteFloat (  1,t.playercontrol.thirdperson.cameradistance );
	WriteFloat (  1,t.playercontrol.thirdperson.cameraheight );
	WriteFloat (  1,t.playercontrol.thirdperson.cameraspeed );

	//  20100653 additions
	WriteFloat (  1,t.playercontrol.thirdperson.camerafocus );
	WriteLong (  1,t.playercontrol.thirdperson.cameralocked );
	WriteLong (  1,t.playercontrol.thirdperson.camerashoulder );
	WriteLong (  1,t.playercontrol.thirdperson.camerafollow );
	WriteLong (  1,t.playercontrol.thirdperson.camerareticle );

	CloseFile (  1 );

	//  Restore
	SetDir (  t.old_s.Get() );
}

