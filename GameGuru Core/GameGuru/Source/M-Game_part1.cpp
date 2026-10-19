void game_masterroot_gameloop_initcode(int iUseVRTest)
{
	// also hide rendering of 3D while we set up a new level
	SyncMaskOverride ( 0 );

	extern uint32_t LuaFrameCount;
	LuaFrameCount = 0;

	// Loading page
	timestampactivity(0,"_titles_loadingpageupdate");
	if ( t.game.gameisexe == 1 ) 
	{
		// Ensure no previous flatareadata exists prior to entity creating new ones
		timestampactivity(0, "Remove All Terrain Flat Areas");
		GGTerrain_RemoveAllFlatAreas();
		
		//PE: When getting here everything was faded out.
		t.postprocessings.fadeinvalue_f = 1.0f;
		HideOrShowLUASprites(false);
		EnableAllSprites(); // the disable is called in DarkLUA by ResetFade() black out command when load game position

		//titles_loadingpage ( );
		timestampactivity(0,"LUA script : loading");
		sky_hide();
		t.game.levelloadprogress = 0;
		titleslua_init ( );
		titleslua_main ( "loading" );
		sky_show();
		titleslua_main_loopcode();
		extern bool g_bNoSwapchainPresent;
		//PE: Why was we doing this, this will make a 10 sec blackscreen delay until loading screen is displayed ?????
		//PE: Removed for now TODO check why it was added.
		
		//PE: This was to prevent old loadingscreen is displayed.
		//g_Storyboard_Current_Loading_Page
		extern char g_Storyboard_Current_Loading_Page[256];
		void FindLoadingScreen(void);
		FindLoadingScreen();
		//PE: If using cuatom loading screen delay rendering, TODO: find another way to speed up the 10 sec delay before the screen. (mapfile_loadproject_fpm)
		if( stricmp(g_Storyboard_Current_Loading_Page , "loading.lua") != NULL)
			g_bNoSwapchainPresent = true;
		t.game.levelloadprogress=0  ;
		titles_loadingpageupdate ( );
		g_bNoSwapchainPresent = false;

	}

	// Extract level files from FPM
	if ( t.game.runasmultiplayer == 1 ) 
	{
		// Multiplayer FPM loading
		g.projectfilename_s=g.mysystem.editorsGrideditAbs_s+"__multiplayerlevel__.fpm";//g.fpscrootdir_s+"\\Files\\editors\\gridedit\\__multiplayerlevel__.fpm";
		t.trerfeshvisualsassets=1;
		mapfile_loadproject_fpm ( );
		t.game.jumplevel_s="";
	}
	else
	{
		// Single player
		if ( Len(t.game.jumplevel_s.Get())> 0 ) //PE: issue https://github.com/TheGameCreators/GameGuruRepo/issues/444
		{
			// can override jumplevel with 'advanced warning level filename' when LOAD level from MAIN MENU
			if ( strcmp ( t.game.pAdvanceWarningOfLevelFilename, "" ) != NULL )
			{
				t.game.jumplevel_s = t.game.pAdvanceWarningOfLevelFilename;
				strcpy ( t.game.pAdvanceWarningOfLevelFilename, "" );
			}

			// work out first level from exe name (copied to jumplevel_s)
			g.projectfilename_s = g.mysystem.mapbank_s + t.game.jumplevel_s;
			//PE: In new code ifused can now include mapbank
			if (pestrcasestr(t.game.jumplevel_s.Get(), "mapbank\\"))
				g.projectfilename_s = t.game.jumplevel_s;
			if ( cstr(Lower(Right(g.projectfilename_s.Get(),4))) != ".fpm" )
				g.projectfilename_s=g.projectfilename_s+".fpm";

			// 050316 - if not there, try all subfolders
			if ( FileExist(cstr(g.fpscrootdir_s+"\\Files\\"+g.projectfilename_s).Get()) == 0 ) 
			{
				// go into mapbank folder
				cstr tthisold_s =  "";
				tthisold_s=GetDir();
				SetDir ( g.mysystem.mapbankAbs_s.Get() );

				// scan for ALL files/folders
				ChecklistForFiles (  );
				for ( int c = 1 ; c<=  ChecklistQuantity(); c++ )
				{
					if (  ChecklistValueA(c) != 0 ) 
					{
						// only folders
						cstr tfolder_s = ChecklistString(c);
						if ( tfolder_s != "." && tfolder_s != ".." ) 
						{
							// skip . and .. folders
							cstr newlevellocation = g.mysystem.mapbank_s + tfolder_s + "\\" + t.game.jumplevel_s;
							if ( cstr(Lower(Right(newlevellocation.Get(),4))) != ".fpm" )
								newlevellocation = newlevellocation + ".fpm";

							// does this guessed file location exist
							if ( FileExist(cstr(g.fpscrootdir_s+"\\Files\\"+newlevellocation).Get()) == 1 ) 
							{
								// found the level inside a nested folder
								g.projectfilename_s = newlevellocation; 
								break;
							}
						}
					}
				}
				SetDir ( tthisold_s.Get() );
			}

			//PE: Make sure if using storyboard we set the current level correctly.
			//+t.game.jumplevel_s{ m_pString = 0x000001468321fe60 "level 3.fpm" m_size = 11 m_capacity = 26 }	cStr
			//+ g.projectfilename_s{ m_pString = 0x00000146ae96d490 "mapbank\\level 3.fpm" m_size = 19 m_capacity = 27 }	cStr
			if (strlen(Storyboard.gamename) > 0)
			{
				//PE: Find g_Storyboard_Current_Level from t.game.jumplevel_s.
				for (int i = 0; i < STORYBOARD_MAXNODES; i++)
				{
					if (Storyboard.Nodes[i].used)
					{
						if (pestrcasestr(Storyboard.Nodes[i].level_name, t.game.jumplevel_s.Get()) != 0)
						{
							g_Storyboard_Current_Level = i;
							strcpy(g_Storyboard_Current_fpm, Storyboard.Nodes[i].level_name);
						}
					}
				}
			}

			// finally load the level in
			mapfile_loadproject_fpm ( );
			t.visuals=t.gamevisuals;
			t.game.jumplevel_s = "";

		}
	}

	// reload gunspecs
	if (g.reloadWeaponGunspecs == 1)
	{
		gun_scaninall_dataonly();
	}

	// we first load extra guns into gun array EARLY (ahead of entity data load which assigns gunids to isweapon hasweapon)
	gun_tagmpgunstolist ( );

	// help keep progress bar instant and moving
	char pProgressStr[256];
	sprintf_s(pProgressStr, 256, "PREPARING TEST LEVEL - %d\\100 Complete", 1);
	void printscreenprompt(char*);
	printscreenprompt(pProgressStr);

	// just load the entity data for now (rest in _game_loadinleveldata)
	timestampactivity(0,"_game_loadinentitiesdatainlevel");
	if ( t.game.gameisexe == 1 || t.game.runasmultiplayer == 1 ) 
	{
		//  extra precaution, delete any old entities and LM objects
		if ( t.game.runasmultiplayer == 1 ) 
		{
			entity_delete ( );
		}
		#ifdef WIP_PROLOADLEVELTEXTURES
		//PE: Record preload informations here.
		preload_setup.clear();
		#endif

		game_loadinentitiesdatainlevel ( );

		#ifdef WIP_PROLOADLEVELTEXTURES
		std::string sString = g.projectfilename_s.Get();
		replaceAll(sString, "\\", "_");
		replaceAll(sString, ".", "_");
		replaceAll(sString, ":", "_");

		//PE: Save preload information.
		char szRealFilename[MAX_PATH];
		strcpy_s(szRealFilename, MAX_PATH, "preloadinfo\\");
		strcat(szRealFilename, sString.c_str());
		GG_GetRealPath(szRealFilename, 1);
		std::ofstream output_file(szRealFilename);
		std::ostream_iterator<std::string> output_iterator(output_file, "\n");
		std::copy(preload_setup.begin(), preload_setup.end(), output_iterator);
		preload_setup.clear();
		#endif
	}

	// Load any extra material sounds associated with new entities (i.e. material(m).usedinlevel=1?)
	// NOTE: Level can collect materials (and material depth) and apply here to quicken material loader (2s)
	material_loadsounds ( 0 );

	// and reset 3D listener for consistency each level
	extern void ResetListener (void);
	ResetListener();

	// if multiplayer, detect spawn positions and add extra UBER characters
	if ( t.game.runasmultiplayer == 1 ) 
	{
		// these are the multiplayer start markers
		t.tnumberofstartmarkers = 0;
		g.mp.team = 0;
		g.mp.coop = 0;
		for ( t.tc = 1 ; t.tc<=  MP_MAX_NUMBER_OF_PLAYERS; t.tc++ )
		{
			t.mpmultiplayerstart[t.tc].active=0;
		}
		t.plrindex=1;
		t.tfoundAMultiplayerScript = 0;
		for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
		{
			// reset all updates
			t.entityelement[t.e].mp_updateOn = 0;
			t.entityelement[t.e].mp_isLuaChar = 0;
			t.entityelement[t.e].mp_rotateType = 0;
			t.entid=t.entityelement[t.e].bankindex;
			if ( t.entid>0 ) 
			{
			}
		}

		// Build multiplayer start markers
		t.thaveTeamAMarkers = 0;
		t.thaveTeamBMarkers = 0;
		t.tmpstartindex = 1;
		for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
		{
			t.entid=t.entityelement[t.e].bankindex;
			if ( t.entid>0 ) 
			{
				if ( t.entityprofile[t.entid].ismarker == 7 && t.tmpstartindex <= MP_MAX_NUMBER_OF_PLAYERS ) 
				{
					// add start markers for free for all or team a
					// a spawn GetPoint ( for the multiplayer )
					t.mpmultiplayerstart[t.tmpstartindex].active=1;
					t.mpmultiplayerstart[t.tmpstartindex].x=t.entityelement[t.e].x;
					// added 10 onto the y otherwise the players fall through the ground
					t.mpmultiplayerstart[t.tmpstartindex].y=t.entityelement[t.e].y+50;
					t.mpmultiplayerstart[t.tmpstartindex].z=t.entityelement[t.e].z;
					t.mpmultiplayerstart[t.tmpstartindex].angle=t.entityelement[t.e].ry;
					t.thaveTeamAMarkers = 1;
					++t.tnumberofstartmarkers;
					++t.tmpstartindex;
				}
			}
		}
		// add team b markers if in team mode

		// check for coop mode
		g.mp.coop = 0;

		// perhaps it is a solo game with a start maker only
		bool bHaveRegularStartMarker = false;
		if ( g.mp.coop == 0 && t.tnumberofstartmarkers == 0 ) 
		{
			for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
			{
				t.entid=t.entityelement[t.e].bankindex;
				if ( t.entid>0 ) 
				{
					if ( t.entityprofile[t.entid].ismarker == 1 ) 
					{
						// a spawn GetPoint ( for the multiplayer )
						bHaveRegularStartMarker = true;
						t.mpmultiplayerstart[1].active=1;
						t.mpmultiplayerstart[1].x=t.entityelement[t.e].x;
						// added 10 onto the y otherwise the players fall through the ground
						t.mpmultiplayerstart[1].y=t.entityelement[t.e].y+50;
						t.mpmultiplayerstart[1].z=t.entityelement[t.e].z;
						t.mpmultiplayerstart[1].angle=t.entityelement[t.e].ry;
						t.entityelement[t.e].eleprof.phyalways = 1;

					}
				}
			}
		}

		//  if multiplayer and not coop, disable ai characters
			// Photon retains all characters in map

		// if multiplayer and coop, setup ai for switching who control them, depending on gameplay circumstances

		// if no multiplayer markers, put some at the default height
		if ( t.tnumberofstartmarkers == 0 && bHaveRegularStartMarker == false ) 
		{
			for ( t.tloop = 1; t.tloop <= MP_MAX_NUMBER_OF_PLAYERS; t.tloop++ )
			{
				t.mpmultiplayerstart[t.tloop].active=1;
				t.mpmultiplayerstart[t.tloop].x=GGORIGIN_X;
				//  added 10 onto the y otherwise the players fall through the ground
				t.mpmultiplayerstart[t.tloop].y=BT_GetGroundHeight(t.terrain.TerrainID,GGORIGIN_X,GGORIGIN_Z)+50;
				t.mpmultiplayerstart[t.tloop].z=GGORIGIN_Z;
				t.mpmultiplayerstart[t.tloop].angle=0;
			}
		}

		// reserve max multiplayer characters (all weapon animations included)
		Dim ( t.tubindex,2+MP_MAX_NUMBER_OF_PLAYERS  );
		t.ent_s=g.rootdir_s+"charactercreatorplus\\Uber Character.fpe";
		entity_addtoselection_core ( );
		t.tubindex[0]=t.entid;
		t.entityprofile[t.tubindex[0]].ischaracter=0;
		t.entityprofile[t.tubindex[0]].collisionmode=12;
		t.entityprofile[t.tubindex[0]].aimain_s = "";

			// No teams - no combat!

		// add any character creator player avatars in
		for ( t.tcustomAvatarCount = 0 ; t.tcustomAvatarCount <= MP_MAX_NUMBER_OF_PLAYERS-1; t.tcustomAvatarCount++ )
		{
			t.mp_playerAvatarLoaded[t.tcustomAvatarCount] = false;
		}
		t.bTriggerAvatarRescanAndLoad = true;
		game_scanfornewavatars ( false );

		// store ttiswitch for tti as multiplayer avatars can upset the 0->1 switching!
		t.ttiswitch = 1;
		for ( t.plrindex = 1 ; t.plrindex <= MP_MAX_NUMBER_OF_PLAYERS; t.plrindex++ )
		{
			// Add the max number of players into the level if there are start markers or not
			if ( g.mp.team == 1 && g.mp.coop == 0 ) 
			{
				t.ttiswitch = 1 - t.ttiswitch;
			}
			else
			{
				t.ttiswitch = 0;
			}
			t.tti = t.ttiswitch;

			// check if the player has their own avatar
			if ( t.mp_playerAvatars_s[t.plrindex-1] != "" ) 
			{
				t.tti = t.plrindex-1+2;
			}

			t.ubercharacterindex = t.tubindex[t.tti];
			t.entitymaintype=1 ; t.entitybankindex=t.ubercharacterindex;
			t.gridentityeditorfixed=0;
			t.gridentitystaticmode=0;
			t.gridentityhasparent=0;
			t.tfoundone = 0;
			if ( t.mpmultiplayerstart[t.plrindex].active == 1 ) 
			{
				t.tfoundone = 1;
				t.gridentityposx_f=t.mpmultiplayerstart[t.plrindex].x;
				t.gridentityposy_f=t.mpmultiplayerstart[t.plrindex].y;
				t.gridentityposz_f=t.mpmultiplayerstart[t.plrindex].z;
			}
			else
			{
				t.tonetotry = t.plrindex/2;
				if ( t.tonetotry > 0 ) 
				{
					t.tfoundone = 1;
					if ( t.mpmultiplayerstart[t.tonetotry].active == 1 ) 
					{
						t.gridentityposx_f=t.mpmultiplayerstart[t.tonetotry].x;
						t.gridentityposy_f=t.mpmultiplayerstart[t.tonetotry].y;
						t.gridentityposz_f=t.mpmultiplayerstart[t.tonetotry].z;
					}
				}
			}
			if ( t.tfoundone == 0 ) 
			{
				if ( t.mpmultiplayerstart[1].active == 1 ) 
				{
					t.gridentityposx_f=t.mpmultiplayerstart[1].x;
					t.gridentityposy_f=t.mpmultiplayerstart[1].y;
					t.gridentityposz_f=t.mpmultiplayerstart[1].z;
				}
			}
			t.gridentityrotatex_f=0;
			t.gridentityrotatey_f=t.mpmultiplayerstart[t.plrindex].angle;
			t.gridentityrotatez_f=0;
			t.gridentityrotatequatmode = 0;
			t.gridentityrotatequatx_f = 0;
			t.gridentityrotatequaty_f = 0;
			t.gridentityrotatequatz_f = 0;
			t.gridentityrotatequatw_f = 1;
			t.gridentityscalex_f=100;
			t.gridentityscaley_f=100;
			t.gridentityscalez_f=100;
			entity_fillgrideleproffromprofile ( );
			entity_addentitytomap_core ( );
			t.mpmultiplayerstart[t.plrindex].ghostentityindex=t.e;

			// Grab the entity number for steam to use
			t.mp_playerEntityID[t.plrindex-1] = t.e;
			t.entityprofile[t.ubercharacterindex].ismultiplayercharacter=1;
			t.entityprofile[t.ubercharacterindex].hasweapon_s="";
			t.entityprofile[t.ubercharacterindex].hasweapon=0;
			t.entityprofile[t.ubercharacterindex].aimain_s = "";
		}
	}

	// in standalone, no IDE feeding test level, so load it in
	timestampactivity(0,"_game_loadinleveldata");
	if ( t.game.gameisexe == 1 || t.game.runasmultiplayer == 1 ) 
	{
		game_loadinleveldata ( );
	}

	//  Prepare this level
	t.game.levelplrstatsetup = 1; //PE: Make sure to setup new "player start marker" settings.
	if (t.game.gameisexe == 1) loadingpageprogress(5);

	// help keep progress bar instant and moving
	sprintf_s(pProgressStr, 256, "LOADING LEVEL RESOURCES - %d\\100 Complete", 2);
	void printscreenprompt(char*);
	printscreenprompt(pProgressStr);
	game_preparelevel ( );
	game_preparelevel_forplayer ( );

	//LB: some corruption in older levels, can correct here (level editor also corrects, but not for levels loaded and ran)
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		if (t.entityelement[t.e].obj > 0 && t.e < g.entityelementlist)
		{
			int masterid = t.entityelement[t.e].bankindex;
			if (masterid > 0 && (t.entityprofile[masterid].ragdoll == 1 || t.entityprofile[masterid].ischaracter == 1))
			{
				entity_calculateeuleryfromquat(t.e);
			}
		}
	}

	sprintf_s(pProgressStr, 256, "FINALIZING LEVEL DATA");// -% d\\100 Complete", 15);
	void printscreenprompt(char*);
	printscreenprompt(pProgressStr);
	game_preparelevel_finally ( );

	//PE: Disable collision.
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		int tentid = t.entityelement[t.e].bankindex;
		int tobj = t.entityelement[t.e].obj;
		if (tentid > 0 && tobj > 0)
		{
			if (t.entityprofile[tentid].ischaracter == 0)
			{
				int iShape = t.entityprofile[tentid].collisionmode;
				if (t.entityelement[t.e].eleprof.iOverrideCollisionMode != -1)
						iShape = t.entityelement[t.e].eleprof.iOverrideCollisionMode;
				//PE: no max fpe is using canseethrough. else if (t.entityprofile[tentid].canseethrough == 1) iShape = 11;
				if (iShape == 11)
				{
					sObject* pObject = g_ObjectList[tobj];
					if (pObject)
					{
						// LB: Exception are objects that have is collectable set (need to detect these, even if no collision mode set)
						if (t.entityelement[t.e].eleprof.iscollectable==0)
						{
							WickedCall_SetDisableCollision(pObject, true);
						}
					}
				}
			}
		}
	}

	g.merged_new_objects = 0;
	if ( t.tlmloadsuccess == 0  ) 
	{ 
		//&& !g.disable_drawcall_optimizer
		//###################################################################
		//#### PE: Very simple but effectively draw call optimizer       ####
		//#### Could be made more intelligent when time allow :)         ####
		//#### On a 2000 object level it takes below 2 sec to run.       ####
		//#### setup.ini "drawcalloptimizer=1" will optimize everything  ####
		//#### setup.ini "drawcalloptimizer=0" only fpe settings counts. ####
		//#### .fpe "drawcalloptimizer=1" will optimize this object.     ####
		//#### .fpe "drawcalloptimizer=0" will NOT optimize object.      ####
		//#### .fpe "drawcalloptimizeroff=1" will NOT optimize object.   ####
		//#### (drawcalloptimizeroff is used when you optimize           ####
		//####  everything but have problems with a object )             ####
		//#### .fpe "drawcallscaleadjust" adjust scale.                  ####
		//###################################################################

		timestampactivity(0, "draw call optimizer.");
		#define DC_DISTANCE 1000
		#define MAX_DRAWPRIMITIVES 32765 // max faces.
		#define MAX_DRAWVERTEX 65520 // max vertex 65530.

		// clear old draw call optimizer objects
		for (t.e = 1; t.e <= g.entityelementlist; t.e++)
		{
			if (t.entityelement[t.e].obj > 0 && t.e < g.entityelementlist )
			{
				t.entityelement[t.e].dc_merged = false;
				if (t.entityelement[t.e].draw_call_obj > 0) 
				{
					if (t.entityelement[t.e].draw_call_obj > 0 && ObjectExist(t.entityelement[t.e].draw_call_obj) == 1) 
					{
						DeleteObject(t.entityelement[t.e].draw_call_obj);
						t.entityelement[t.e].draw_call_obj = 0;
					}
				}
			}
		}

		// go through all level objects
		for (t.e = 1; t.e <= g.entityelementlist; t.e++)
		{
			// for each object
			t.entid = t.entityelement[t.e].bankindex;
			t.obj = t.entityelement[t.e].obj;
			if (t.obj > 0 && t.e < g.entityelementlist && t.entityelement[t.e].dc_merged == false && (g.globals.drawcalloptimizer==1 || t.entityprofile[t.entid].drawcalloptimizer == 1) && t.entityprofile[t.entid].drawcalloptimizeroff == 0 && t.entityprofile[t.entid].isimmobile != 1 && t.entityelement[t.e].eleprof.isimmobile != 1 && t.entityelement[t.e].eleprof.spawnatstart == 1)
			{
				struct OrderByObjectDistance
				{
					bool operator()(int pObjectA, int pObjectB)
					{
						if (t.entityelement[pObjectA].dc_distance < t.entityelement[pObjectB].dc_distance) return true;
						if (t.entityelement[pObjectA].dc_distance == t.entityelement[pObjectB].dc_distance) return true;
						return false;
					}
				};

				// Sort a sublist by object, distance to increase hit rate
				int nextObjeid = 0;
				std::vector< int > vObjList;
				if (ObjectExist(t.obj)) 
				{
					for (int i = 1; i <= g.entityelementlist; i++) 
					{
						int testobj = t.entityelement[i].obj;
						int iEntid = t.entityelement[i].bankindex;
						if (testobj > 0 && i != t.e && ObjectExist(testobj) && t.entityelement[i].dc_merged == false && t.entityprofile[iEntid].isimmobile != 1 && t.entityelement[i].eleprof.isimmobile != 1 && t.entityelement[i].staticflag == 1 && t.entityelement[i].eleprof.spawnatstart == 1) 
						{
							sObject* pObject = g_ObjectList[t.obj];
							int instanceonly = 0;
							if (pObject && pObject->pInstanceOfObject) 
							{
								pObject = pObject->pInstanceOfObject;
							}

							sObject* pObjectTest = g_ObjectList[testobj];
							if (pObjectTest && pObjectTest->pInstanceOfObject) 
							{
								pObjectTest = pObjectTest->pInstanceOfObject;
							}

							if (pObject && pObjectTest && pObject == pObjectTest) 
							{
								t.tdx_f = t.entityelement[t.e].x - t.entityelement[i].x;
								t.tdz_f = t.entityelement[t.e].z - t.entityelement[i].z;
								t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
								t.entityelement[i].dc_distance = t.tdd_f;
								vObjList.push_back(i);
							}
						}
					}

					//Sort list
					std::sort(vObjList.begin(), vObjList.end(), OrderByObjectDistance());
				}
				if (vObjList.size() > 0)
					nextObjeid = vObjList[0];

				// Merge objects
				int glueid = t.entityelement[nextObjeid].bankindex;
				int glueobj = t.entityelement[nextObjeid].obj;
				if (vObjList.size() > 0 && glueobj > 0 && ObjectExist(t.obj) && ObjectExist(glueobj))
				{
					bool validshader = false;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_basic.fx") == 0)
						validshader = true;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_tree.fx") == 0)
						validshader = true;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\entity_basic.fx") == 0)
						validshader = true;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_illum.fx") == 0)
						validshader = true;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "") == 0)
						validshader = true;
					if(t.entityprofile[t.entid].animmax == 0)
						validshader = true;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\treea_basic.fx") == 0)
						validshader = false;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_anim.fx") == 0)
						validshader = false;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_animwithtran.fx") == 0)
						validshader = false;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_treea.fx") == 0)
						validshader = false;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_anim8bone.fx") == 0)
						validshader = false;
					if (strcmp(Lower(t.entityprofile[t.entid].effect_s.Get()), "effectbank\\reloaded\\apbr_animwithtran.fx") == 0)
						validshader = false;
					if (t.entityprofile[t.entid].animmax > 0)
						validshader = false;
							
					if (validshader && t.entityprofile[t.entid].ismarker == 0 && t.entityprofile[t.entid].isebe == 0 && t.entityprofile[t.entid].transparency == 0 && t.entityelement[nextObjeid].staticflag == 1 && t.entityprofile[t.entid].isimmobile != 1 && t.entityelement[t.e].eleprof.isimmobile != 1 && t.entityelement[t.e].eleprof.spawnatstart == 1 && t.entityelement[t.e].staticflag == 1)
					{
						//Validate if same master object.
						sObject* pObject = g_ObjectList[t.obj];
						int instanceonly = 0;
						if (pObject && pObject->pInstanceOfObject) 
						{
							pObject = pObject->pInstanceOfObject;
							instanceonly++;
						}
						sObject* pObject2 = g_ObjectList[glueobj];
						if (pObject2 && pObject2->pInstanceOfObject) 
						{
							pObject2 = pObject2->pInstanceOfObject;
							instanceonly++;
						}
						int iMeshWithTexture = -1;
						int iFrameVertex = 0;
						std::vector< int > vUniqueImageIds;
						vUniqueImageIds.clear();
						if (pObject)
						{
							for (int i = 0; i < pObject->iFrameCount; i++)
							{
								if (pObject->ppFrameList[i]->pMesh)
								{
									sMesh* pMesh = pObject->ppFrameList[i]->pMesh;
									if( pMesh->pTextures )
									{
										//PE: Count how many different images is used.
										vUniqueImageIds.push_back(pMesh->pTextures[0].iImageID);
									}
									iMeshWithTexture = i;
									iFrameVertex += pObject->ppFrameList[i]->pMesh->dwVertexCount;
								}
							}
						}

						//Cant merge too many vertex.
						if (iFrameVertex > 0x8000) 
							instanceonly = 0;

						// Allow users to also drawcall optimize multimaterial objects, if set in fpe.
						if (t.entityprofile[t.entid].drawcalloptimizer != 1)
						{
							if (vUniqueImageIds.size() > 1)
							{
								instanceonly = 0;
							}
							if (iMeshWithTexture >= 0 && pObject->ppFrameList[iMeshWithTexture]->pMesh) 
							{
								//PE: Cant do multi material for now.
								if (pObject->ppFrameList[iMeshWithTexture]->pMesh->bUseMultiMaterial)
									instanceonly = 0;
							}
						}

						// Allows you to force even multi-textured object to be batched.
						int iMultiMatCount = GetMultiMaterialCount(t.obj);
						if (iMultiMatCount > 0) 
						{
							if(t.entityprofile[t.entid].drawcalloptimizer != 1)
								instanceonly = 0;
						}

						// Keep objects distance below DC_DISTANCE for best culling.
						if (pObject && pObject2 && instanceonly >= 2  && t.entityelement[nextObjeid].dc_distance < DC_DISTANCE && g.merged_new_objects < 2890 ) 
						{
							if (pObject == pObject2) 
							{
								//Same master glue it.
								if (GetMeshExist(g.meshlightmapwork) == 1)  DeleteMesh(g.meshlightmapwork);

								float gluescalex = ObjectScaleX(glueobj);
								float gluescaley = ObjectScaleY(glueobj);
								float gluescalez = ObjectScaleZ(glueobj);

								float src_scalex = ObjectScaleX(t.obj);
								float src_scaley = ObjectScaleY(t.obj);
								float src_scalez = ObjectScaleZ(t.obj);

								float scaleadjust = t.entityprofile[t.entid].drawcallscaleadjust;

								int tmpobj = (g.merged_new_objects+100) + 87000; //PE: TODO change 85000
								if (tmpobj < g_iObjectListCount && g_ObjectList[tmpobj])
								{
									if (g_ObjectList[tmpobj]->pFrame)
									{
										DeleteObject(tmpobj);
									}
								}

								CloneObject(tmpobj, t.obj);

								// The lod removal could be improved.
								int bestlod = -1;
								PerformCheckListForLimbs(tmpobj);
								for (t.c = ChecklistQuantity(); t.c >= 1; t.c += -1)
								{
									t.tname_s = Lower(ChecklistString(t.c));

									LPSTR pRightFive = "";
									if (strlen(t.tname_s.Get()) > 5)
										pRightFive = t.tname_s.Get() + strlen(t.tname_s.Get()) - 5;

									if ( (t.tname_s == "lod_0" || stricmp(pRightFive,"_lod0") == 0 ) ) bestlod = 0;
									if ( (t.tname_s == "lod_1" || stricmp(pRightFive,"_lod1") == 0 ) && (bestlod < 0 || bestlod > 1) )  bestlod = 1;
									if ( (t.tname_s == "lod_2" || stricmp(pRightFive,"_lod2") == 0 ) && (bestlod < 0) )  bestlod = 2;

									if (t.entityprofile[t.entid].resetlimbmatrix == 1)
									{
										OffsetLimb(tmpobj, t.c - 1, 0, 0, 0, 0);
									}
								}
								if (bestlod >= 0) 
								{
									for (t.c = ChecklistQuantity(); t.c >= 1; t.c += -1)
									{
										t.tname_s = Lower(ChecklistString(t.c));
										LPSTR pRightFive = "";
										if (strlen(t.tname_s.Get()) > 5)
											pRightFive = t.tname_s.Get() + strlen(t.tname_s.Get()) - 5;

										if (bestlod == 0 && ( t.tname_s == "lod_1" || t.tname_s == "lod_2" || t.tname_s == "lod_3") ) 
										{
											RemoveLimb(tmpobj, t.c - 1);
										}
										if (bestlod == 0 && (stricmp(pRightFive, "_lod1") == 0 || stricmp(pRightFive, "_lod2") == 0 || stricmp(pRightFive, "_lod3") == 0 )) 
										{
											RemoveLimb(tmpobj, t.c - 1);
										}
										if (bestlod == 1 && (t.tname_s == "lod_2" || stricmp(pRightFive, "_lod2") == 0) ) 
										{
											RemoveLimb(tmpobj, t.c - 1);
										}
										if (bestlod == 2 && (t.tname_s == "lod_3" || stricmp(pRightFive, "_lod3") == 0 ) ) 
										{
											RemoveLimb(tmpobj, t.c - 1);
										}
									}
								}

								PositionObject(tmpobj, 0, 0, 0); //PE: Need to be at 0,0,0
								ScaleObject(tmpobj, 100, 100, 100); //PE: no scale.

								int iAfterPolygonTotal = 0;
								int iAfterVertex = 0;

								sObject* pAfterObject = g_ObjectList[tmpobj];
								if (pAfterObject)
								{
									if (pAfterObject->iMeshCount>0)
									{
										for (int iM = 0; iM<pAfterObject->iMeshCount; iM++)
										{
											sMesh* pMesh = pAfterObject->ppMeshList[iM];
											if (pMesh)
											{
												iAfterPolygonTotal += pMesh->iDrawPrimitives;
												iAfterVertex += pMesh->dwVertexCount;
											}
										}
									}
								}

								if (pAfterObject && pAfterObject->iMeshCount == 0) 
								{
									if (GetMeshExist(g.meshlightmapwork) == 1)  
										DeleteMesh(g.meshlightmapwork);
								}
								else 
								{
									MakeMeshFromObject(g.meshlightmapwork, tmpobj);
								}

								if( (iAfterPolygonTotal * 2 < MAX_DRAWPRIMITIVES) && (iAfterVertex * 2 < MAX_DRAWVERTEX)  && GetMeshExist(g.meshlightmapwork) == 1)
								{
									int destobj = g.merged_new_objects + 87000; //PE: TODO change 85000 , perhaps reverse from 90000 ?
									if (g_ObjectList[destobj])
									{
										if (g_ObjectList[destobj]->pFrame)
										{
											DeleteObject(destobj);
										}
									}

									t.tmasterx_f = ObjectPositionX(t.obj);
									t.tmastery_f = ObjectPositionY(t.obj);
									t.tmasterz_f = ObjectPositionZ(t.obj);

									// Use mesh to prevent any transforms.
									MakeObject(destobj, g.meshlightmapwork, -1); 

									int testypos = 0;
									PositionObject(destobj, 0, 0, 0); //PE: Need to be at 0,0,0
									ScaleObject(destobj, 100, 100, 100); //PE: no scale.

									float src_angx = ObjectAngleX(t.obj);
									float src_angy = ObjectAngleY(t.obj);
									float src_angz = ObjectAngleZ(t.obj);

									PerformCheckListForLimbs(destobj);
									AddLimb(destobj, ChecklistQuantity(), g.meshlightmapwork);

									t.tox_f = ObjectPositionX(glueobj) - t.tmasterx_f;
									t.toy_f = ObjectPositionY(glueobj) - t.tmastery_f;
									t.toz_f = ObjectPositionZ(glueobj) - t.tmasterz_f;

									OffsetLimb(destobj, ChecklistQuantity(), t.tox_f, t.toy_f, t.toz_f);

									RotateLimb(destobj, ChecklistQuantity(), ObjectAngleX(glueobj), ObjectAngleY(glueobj), ObjectAngleZ(glueobj));
									ScaleLimb(destobj, ChecklistQuantity(), gluescalex + scaleadjust, gluescaley + scaleadjust, gluescalez + scaleadjust);
									for (int i = ChecklistQuantity()-1; i >= 0; i--) 
									{
										RotateLimb(destobj, i, src_angx, src_angy, src_angz);
										ScaleLimb(destobj, i, ObjectScaleX(t.obj) + scaleadjust, ObjectScaleY(t.obj) + scaleadjust, ObjectScaleZ(t.obj) + scaleadjust);
									}

									bool additionaladded2 = false;
									int glueid2 = 0;
									int glueobj2 = 0;
									if ((iAfterPolygonTotal * 3 < MAX_DRAWPRIMITIVES) && (iAfterVertex * 3 < MAX_DRAWVERTEX) && vObjList.size() > 1 ) 
									{
										nextObjeid = vObjList[1];
										glueid2 = t.entityelement[nextObjeid].bankindex;
										glueobj2 = t.entityelement[nextObjeid].obj;
										sObject* pObject3 = g_ObjectList[glueobj2];
										if (pObject3 && pObject3->pInstanceOfObject) 
										{
											pObject3 = pObject3->pInstanceOfObject;
											if (pObject3 == pObject2) 
											{
												t.tdx_f = t.entityelement[t.e].x - t.entityelement[nextObjeid].x;
												t.tdz_f = t.entityelement[t.e].z - t.entityelement[nextObjeid].z;
												t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
												if (t.tdd_f < DC_DISTANCE) 
												{
													//Object ok add.
													float gluescalex2 = ObjectScaleX(glueobj2);
													float gluescaley2 = ObjectScaleY(glueobj2);
													float gluescalez2 = ObjectScaleZ(glueobj2);

													//Its the same master so reuse g.meshlightmapwork
													PerformCheckListForLimbs(destobj);
													AddLimb(destobj, ChecklistQuantity(), g.meshlightmapwork);

													t.tox_f = ObjectPositionX(glueobj2) - t.tmasterx_f;
													t.toy_f = ObjectPositionY(glueobj2) - t.tmastery_f;
													t.toz_f = ObjectPositionZ(glueobj2) - t.tmasterz_f;

													OffsetLimb(destobj, ChecklistQuantity(), t.tox_f, t.toy_f, t.toz_f);

													RotateLimb(destobj, ChecklistQuantity(), ObjectAngleX(glueobj2), ObjectAngleY(glueobj2), ObjectAngleZ(glueobj2));
													ScaleLimb(destobj, ChecklistQuantity(), gluescalex2 + scaleadjust, gluescaley2 + scaleadjust, gluescalez2 + scaleadjust);
													additionaladded2 = true;
												}
											}
										}
									}

									bool additionaladded3 = false;
									int glueid3 = 0;
									int glueobj3 = 0;
									if ((iAfterPolygonTotal * 4 < MAX_DRAWPRIMITIVES) && (iAfterVertex * 4 < MAX_DRAWVERTEX)  && vObjList.size() > 2) 
									{
										nextObjeid = vObjList[2];
										glueid3 = t.entityelement[nextObjeid].bankindex;
										glueobj3 = t.entityelement[nextObjeid].obj;

										sObject* pObject4 = g_ObjectList[glueobj3];
										if (pObject4 && pObject4->pInstanceOfObject) 
										{
											pObject4 = pObject4->pInstanceOfObject;
											if (pObject4 == pObject2) 
											{
												t.tdx_f = t.entityelement[t.e].x - t.entityelement[nextObjeid].x;
												t.tdz_f = t.entityelement[t.e].z - t.entityelement[nextObjeid].z;
												t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
												if (t.tdd_f < DC_DISTANCE) 
												{
													//Object ok add.
													float gluescalex3 = ObjectScaleX(glueobj3);
													float gluescaley3 = ObjectScaleY(glueobj3);
													float gluescalez3 = ObjectScaleZ(glueobj3);
													//Its the same master so reuse g.meshlightmapwork
													PerformCheckListForLimbs(destobj);
													AddLimb(destobj, ChecklistQuantity(), g.meshlightmapwork);

													t.tox_f = ObjectPositionX(glueobj3) - t.tmasterx_f;
													t.toy_f = ObjectPositionY(glueobj3) - t.tmastery_f;
													t.toz_f = ObjectPositionZ(glueobj3) - t.tmasterz_f;

													OffsetLimb(destobj, ChecklistQuantity(), t.tox_f, t.toy_f, t.toz_f);

													RotateLimb(destobj, ChecklistQuantity(), ObjectAngleX(glueobj3), ObjectAngleY(glueobj3), ObjectAngleZ(glueobj3));
													ScaleLimb(destobj, ChecklistQuantity(), gluescalex3 + scaleadjust, gluescaley3 + scaleadjust, gluescalez3 + scaleadjust);
													additionaladded3 = true;
												}
											}
										}
									}

									bool additionaladded4 = false;
									int glueid4 = 0;
									int glueobj4 = 0;
									if ((iAfterPolygonTotal * 5 < MAX_DRAWPRIMITIVES) && (iAfterVertex * 5 < MAX_DRAWVERTEX)  && vObjList.size() > 3) 
									{
										nextObjeid = vObjList[3];
										glueid4 = t.entityelement[nextObjeid].bankindex;
										glueobj4 = t.entityelement[nextObjeid].obj;
										sObject* pObject5 = g_ObjectList[glueobj4];
										if (pObject5 && pObject5->pInstanceOfObject) 
										{
											pObject5 = pObject5->pInstanceOfObject;
											if (pObject5 == pObject2) 
											{
												t.tdx_f = t.entityelement[t.e].x - t.entityelement[nextObjeid].x;
												t.tdz_f = t.entityelement[t.e].z - t.entityelement[nextObjeid].z;
												t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
												if (t.tdd_f < DC_DISTANCE) 
												{
													//Object ok add.
													float gluescalex4 = ObjectScaleX(glueobj4);
													float gluescaley4 = ObjectScaleY(glueobj4);
													float gluescalez4 = ObjectScaleZ(glueobj4);

													//Its the same master so reuse g.meshlightmapwork
													PerformCheckListForLimbs(destobj);
													AddLimb(destobj, ChecklistQuantity(), g.meshlightmapwork);

													t.tox_f = ObjectPositionX(glueobj4) - t.tmasterx_f;
													t.toy_f = ObjectPositionY(glueobj4) - t.tmastery_f;
													t.toz_f = ObjectPositionZ(glueobj4) - t.tmasterz_f;

													OffsetLimb(destobj, ChecklistQuantity(), t.tox_f, t.toy_f, t.toz_f);

													RotateLimb(destobj, ChecklistQuantity(), ObjectAngleX(glueobj4), ObjectAngleY(glueobj4), ObjectAngleZ(glueobj4));
													ScaleLimb(destobj, ChecklistQuantity(), gluescalex4 + scaleadjust, gluescaley4 + scaleadjust, gluescalez4 + scaleadjust);
													additionaladded4 = true;
												}
											}
										}
									}

									bool additionaladded5 = false;
									int glueid5 = 0;
									int glueobj5 = 0;
									if ((iAfterPolygonTotal * 6 < MAX_DRAWPRIMITIVES) && (iAfterVertex * 6 < MAX_DRAWVERTEX)  && vObjList.size() > 4) 
									{
										nextObjeid = vObjList[4];
										glueid5 = t.entityelement[nextObjeid].bankindex;
										glueobj5 = t.entityelement[nextObjeid].obj;

										sObject* pObject6 = g_ObjectList[glueobj5];
										if (pObject6 && pObject6->pInstanceOfObject) 
										{
											pObject6 = pObject6->pInstanceOfObject;
											if (pObject6 == pObject2) 
											{
												t.tdx_f = t.entityelement[t.e].x - t.entityelement[nextObjeid].x;
												t.tdz_f = t.entityelement[t.e].z - t.entityelement[nextObjeid].z;
												t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
												if (t.tdd_f < DC_DISTANCE) 
												{
													//Object ok add.
													float gluescalex5 = ObjectScaleX(glueobj5);
													float gluescaley5 = ObjectScaleY(glueobj5);
													float gluescalez5 = ObjectScaleZ(glueobj5);

													//Its the same master so reuse g.meshlightmapwork
													PerformCheckListForLimbs(destobj);
													AddLimb(destobj, ChecklistQuantity(), g.meshlightmapwork);

													t.tox_f = ObjectPositionX(glueobj5) - t.tmasterx_f;
													t.toy_f = ObjectPositionY(glueobj5) - t.tmastery_f;
													t.toz_f = ObjectPositionZ(glueobj5) - t.tmasterz_f;

													OffsetLimb(destobj, ChecklistQuantity(), t.tox_f, t.toy_f, t.toz_f);

													RotateLimb(destobj, ChecklistQuantity(), ObjectAngleX(glueobj5), ObjectAngleY(glueobj5), ObjectAngleZ(glueobj5));
													ScaleLimb(destobj, ChecklistQuantity(), gluescalex5 + scaleadjust, gluescaley5 + scaleadjust, gluescalez5 + scaleadjust);
													additionaladded5 = true;
												}
											}
										}
									}

									bool additionaladded6 = false;
									int glueid6 = 0;
									int glueobj6 = 0;
									if ((iAfterPolygonTotal * 7 < MAX_DRAWPRIMITIVES) && (iAfterVertex * 7 < MAX_DRAWVERTEX)  && vObjList.size() > 5) 
									{
										nextObjeid = vObjList[5];
										glueid6 = t.entityelement[nextObjeid].bankindex;
										glueobj6 = t.entityelement[nextObjeid].obj;

										sObject* pObject7 = g_ObjectList[glueobj6];
										if (pObject7 && pObject7->pInstanceOfObject) 
										{
											pObject7 = pObject7->pInstanceOfObject;
											if (pObject7 == pObject2) 
											{
												t.tdx_f = t.entityelement[t.e].x - t.entityelement[nextObjeid].x;
												t.tdz_f = t.entityelement[t.e].z - t.entityelement[nextObjeid].z;
												t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
												if (t.tdd_f < DC_DISTANCE) 
												{
													//Object ok add.
													float gluescalex6 = ObjectScaleX(glueobj6);
													float gluescaley6 = ObjectScaleY(glueobj6);
													float gluescalez6 = ObjectScaleZ(glueobj6);

													//Its the same master so reuse g.meshlightmapwork
													PerformCheckListForLimbs(destobj);
													AddLimb(destobj, ChecklistQuantity(), g.meshlightmapwork);

													t.tox_f = ObjectPositionX(glueobj6) - t.tmasterx_f;
													t.toy_f = ObjectPositionY(glueobj6) - t.tmastery_f;
													t.toz_f = ObjectPositionZ(glueobj6) - t.tmasterz_f;

													OffsetLimb(destobj, ChecklistQuantity(), t.tox_f, t.toy_f, t.toz_f);

													RotateLimb(destobj, ChecklistQuantity(), ObjectAngleX(glueobj6), ObjectAngleY(glueobj6), ObjectAngleZ(glueobj6));
													ScaleLimb(destobj, ChecklistQuantity(), gluescalex6 + scaleadjust, gluescaley6 + scaleadjust, gluescalez6 + scaleadjust);
													additionaladded6 = true;
												}
											}
										}
									}

									// Merge everything into a single mesh.
									DeleteMesh(g.meshlightmapwork);
									MakeMeshFromObject(g.meshlightmapwork, destobj);
									DeleteObject(destobj);
									MakeObject(destobj, g.meshlightmapwork, -1);
									PositionObject(destobj, t.tmasterx_f, t.tmastery_f + testypos, t.tmasterz_f);
									if (t.entityprofile[t.entid].canseethrough == 1)
									{
										SetObjectCollisionProperty(destobj, 1);
									}
									if (t.entityprofile[t.entid].ischaracter == 0)
									{
										if (t.entityprofile[t.entid].collisionmode == 11)
										{
											SetObjectCollisionProperty(destobj, 1);
										}
									}
									if (t.entityprofile[t.entid].cullmode >= 0)
									{
										if (t.entityprofile[t.entid].cullmode != 0)
										{
											SetObjectCull(destobj, 0);
										}
										else
										{
											SetObjectCull(destobj, 1);
										}
									}

									if (GetMeshExist(g.meshlightmapwork) == 1)  
										DeleteMesh(g.meshlightmapwork);
												
									CloneObject(destobj, t.obj, 101); //PE: Copy textures only.

									SetObjectStatic(destobj, true); //Mark as static.

									// Disable if any LOD setup from original object.
									if (bestlod >= 0) 
									{
										SetObjectLOD(destobj, 1, 50000);
										SetObjectLOD(destobj, 2, 50000);
									}

									t.entityelement[t.e].draw_call_obj = destobj;
									t.entityelement[t.e].dc_obj[0] = glueobj;
									t.entityelement[t.e].dc_entid[0] = vObjList[0];
									t.entityelement[vObjList[0]].dc_merged = true;
									if (additionaladded2) 
									{
										t.entityelement[t.e].dc_obj[1] = glueobj2;
										t.entityelement[t.e].dc_entid[1] = vObjList[1];
										t.entityelement[vObjList[1]].dc_merged = true;
										HideObject(glueobj2);
									}
									if (additionaladded3) 
									{
										t.entityelement[t.e].dc_obj[2] = glueobj3;
										t.entityelement[t.e].dc_entid[2] = vObjList[2];
										t.entityelement[vObjList[2]].dc_merged = true;
										HideObject(glueobj3);
									}
									if (additionaladded4) 
									{
										t.entityelement[t.e].dc_obj[3] = glueobj4;
										t.entityelement[t.e].dc_entid[3] = vObjList[3];
										t.entityelement[vObjList[3]].dc_merged = true;
										HideObject(glueobj4);
									}
									if (additionaladded5) 
									{
										t.entityelement[t.e].dc_obj[4] = glueobj5;
										t.entityelement[t.e].dc_entid[4] = vObjList[4];
										t.entityelement[vObjList[4]].dc_merged = true;
										HideObject(glueobj5);
									}
									if (additionaladded6) 
									{
										t.entityelement[t.e].dc_obj[5] = glueobj6;
										t.entityelement[t.e].dc_entid[5] = vObjList[5];
										t.entityelement[vObjList[5]].dc_merged = true;
										HideObject(glueobj6);
									}

									//Hide org objects.
									HideObject(t.obj);
									HideObject(glueobj);
									ShowObject(t.entityelement[t.e].draw_call_obj);
									g.merged_new_objects++;

									// NOTE: Does this mean batched objects will not benefit from these important flags (some models have OpenGL normals and the auto-generated tangents shift about)											
									// PE: Ups my bad , actually we would use pObject->draw_call_obj so it would have all the original settings,
									// PE: but not yet, so just added this again, cant remember why i commented it out ? seams to work fine :)
									DWORD dwArtFlags = 0;
									if (t.entityprofile[t.entid].invertnormal == 1) dwArtFlags = 1;
									if (t.entityprofile[t.entid].preservetangents == 1) dwArtFlags |= 1 << 1;
									SetObjectArtFlags(destobj, dwArtFlags, 0.0f);

								}
								DeleteObject(tmpobj);
							}
						}
					}
				}
			}
		}
	}

	// Create nav mesh from entire level geometry
	timestampactivity(0, "Attempt to create nav mesh");
	if (t.game.gameisexe == 1)
	{
		loadingpageprogress(5);
		game_createnavmeshfromlevel ( true );
	}
	else
	{
		game_createnavmeshfromlevel ( false );
	}

	// Setup variables for main game loop
	t.screenprompt_s = "STARTING GAME";
	if (t.game.gameisexe == 0)  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0, t.screenprompt_s.Get());
	game_init ( );

	// Helpful prompt for start of test game
	if ( t.game.gameisexe == 0 && t.game.runasmultiplayer == 0 ) 
	{
		if (g_bInTutorialMode == true)
		{
			t.visuals.generalprompt_s = "PRESS ESCAPE TO RETURN TO TUTORIAL";
		}
		else
		{
			t.visuals.generalprompt_s = "Press TAB to see framerate or ESCAPE to exit test";
		}
		t.visuals.generalpromptstatetimer= MAXTimer()+123;
	}
	else
	{
		if ( t.game.runasmultiplayer == 1 ) 
		{
			t.visuals.generalpromptstatetimer= MAXTimer()+1000;
			t.visuals.generalprompt_s="Welcome to GameGuru MAX Multiplayer";
		}
		else
		{
			t.visuals.generalpromptstatetimer=0;
		}

		//PE: start any animations that use startanimingame > 0. ( standalone ).
		for (t.tte = 1; t.tte <= g.entityelementlist; t.tte++)
		{
			//PE: issue https://github.com/TheGameCreators/GameGuruRepo/issues/341
			// hide EBE markers
			int iIndex = t.entityelement[t.tte].bankindex;
			if (t.entityprofile[iIndex].isebe != 0)
			{
				t.tobj = t.entityelement[t.tte].obj;
				if (t.tobj>0)
				{
					if (ObjectExist(t.tobj) == 1)
					{
						HideLimb(t.tobj, 0);
					}
				}
			}

			t.entid = t.entityelement[t.tte].bankindex;
			t.tttsourceobj = g.entitybankoffset + t.entityelement[t.tte].bankindex;
			t.tobj = t.entityelement[t.tte].obj;
			if (t.tobj > 0)
			{
				if (ObjectExist(t.tobj) == 1)
				{
					//PE: Possible fix for issues:
					//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/206
					//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/273
					//PE: need testing.
					if (t.entityprofile[t.entid].ischaracter == 1) {
						//Char should always have z depth , but somehow its removed somewhere.

						//PE: check t.entityprofile[t.tentid].zdepth == 0
						EnableObjectZDepth(t.tobj);
					}

					//PE: Make sure we reset all animations. mainly for lua controlled objects like doors
					if (t.entityprofile[t.entid].animmax > 0)
					{
						SetObjectFrame(t.tttsourceobj, 0);
						StopObject(t.tttsourceobj);
						SetObjectFrame(t.tobj, 0);
						StopObject(t.tobj);
					}

					if (t.entityprofile[t.entid].startanimingame > 0) {
						if (t.entityprofile[t.entid].animmax > 0) {
							t.q = t.entityprofile[t.entid].startanimingame - 1;
							SetObjectFrame(t.tttsourceobj, 0);
							LoopObject(t.tttsourceobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
							SetObjectFrame(t.tobj, 0);
							LoopObject(t.tobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
						}
					}
				}
			}
		}

	}

	//  setup spin values, this rotates the player 360 degrees at the start to kill initial
	//  stutter issues, during this time we don't want low FPS warnings
	t.postprocessings.spinfill=0 ; t.postprocessings.spinfillStartAng_f=CameraAngleY(0);

	//  apply any settings
	timestampactivity(0,"immediate title settings applied");
	if (  t.game.gameisexe == 1 || t.game.runasmultiplayer == 1 ) 
	{
		titles_immediateupdatesound ( );
		titles_immediateupdategraphics ( );
	}

	//  for multiplayer, check if there is a jetpack in the level and grab the model to place on players back
	if (  t.game.runasmultiplayer == 1 ) 
	{
		for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
		{
			t.entid=t.entityelement[t.e].bankindex;
			if (  t.entid>0 ) 
			{
				if (  cstr(Lower(Left(t.entityprofileheader[t.entid].desc_s.Get(),8)))  ==   "jet pack"  && ObjectExist(g.steamplayermodelsoffset+120)  ==  0 ) 
				{
					CloneObject (  g.steamplayermodelsoffset+120,t.entityelement[t.e].obj );
					YRotateObject (  g.steamplayermodelsoffset+120,180 );
					FixObjectPivot (  g.steamplayermodelsoffset+120 );
				}
			}
		}
	}

	//  Clear screen of any artifacts
	titles_loadingpagefree();
	CLS (  Rgb(0,0,0) );

	// In EXE running, override cameras with no mask until title/loading done
	SyncMaskOverride ( 0xFFFFFFFF );

	// resort texture list to ignore objects set to be ignored
	DoTextureListSort ( );

	//PE: Make sure we do not display hud used in last level.
	t.game.activeStoryboardScreen = -1;
	for (int i = 0; i < STORYBOARD_MAXNODES; i++)
	{
		if (Storyboard.Nodes[i].showAtStart)
		{
			t.game.activeStoryboardScreen = i;
			break;
		}
	}


	// if reloading standalone level, need to restore basic stats from LUA save file
	// must now reload preserved state of level when enter it (g_LevelFilename)
	if (!g_Storyboard_Starting_New_Level)
	{
		char pLUACustomLoadCall[256];
		strcpy(pLUACustomLoadCall, "GameLoopLoadStats");
		LuaSetFunction(pLUACustomLoadCall, 2, 0);
		LuaPushInt(g_Storyboard_Current_Level);
		LuaPushInt(t.game.jumplevelresetstates);
		t.game.jumplevelresetstates = 0;
		LuaCall();
	}
	g_Storyboard_Starting_New_Level = false;

	// one final command to improve static physics performance
	physics_finalize ( );

	// Wipe out mouse deltas
	t.tMousemove_f = MouseMoveX() + MouseMoveY() + MouseZ(); t.tMousemove_f  = 0;

	//  Tab mode LOW FPS Warning
	g.tabmode=0 ; g.lowfpstarttimer= MAXTimer();

	//  Game loop will run while single level is in play
	t.huddamage.immunity=1000;
	t.game.gameloop=1;
	g.timeelapsed_f=0;

	// 260220 - for some reason, Social VR sets view 0,0,1,1, and does not set it back!
	// so we do so here to ensure we see the game
	SetCameraView(0, 0, 0, GetDisplayWidth(), GetDisplayHeight());

	// no more prompts, reset system so next time we can have a 2 seconds grace before any prompts (see printscreenprompt)
	t.screenprompt_s = "";
	printscreenprompt(t.screenprompt_s.Get());
	t.postprocessings.fadeinvalue_f = 0.0; //PE: Fade in level.

	// prompt
	if ( g.gproducelogfiles == 2 )
		timestampactivity(0,"main game loop begins in deep debug trace mode");
	else
		timestampactivity(0,"main game loop begins");

	extern int iTriggerGrassTreeUpdate;
	iTriggerGrassTreeUpdate = 5; //PE: Make sure trees and grass height is set after terrain has finish.

	//Stop any menu background music ...
	int iFreeSoundID = g.temppreviewsoundoffset + 2;
	if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
	{
		// stop currently playing preview
		StopSound(iFreeSoundID);
	}

	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		//PE: start any ambient music tracks.
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
		if (FileExist(t.visuals.sAmbientMusicTrack.Get()) == 1)
		{
			LoadSound(t.visuals.sAmbientMusicTrack.Get(), iFreeSoundID, 0, 1);
			if (SoundExist(iFreeSoundID) == 1)
			{
				LoopSound(iFreeSoundID);
				SetSoundVolume(iFreeSoundID, t.visuals.iAmbientMusicTrackVolume);
			}
		}
	}
	iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
	if (FileExist(t.visuals.sCombatMusicTrack.Get()) == 1)
	{
		LoadSound(t.visuals.sCombatMusicTrack.Get(), iFreeSoundID, 0, 1);
		if (t.gamevisuals.bEnableCombatMusicTrack)
		{
			if (SoundExist(iFreeSoundID) == 1)
			{
				LoopSound(iFreeSoundID);
				SetSoundVolume(iFreeSoundID, 0); //PE: Controlled in lua t.visuals.iCombatMusicTrackVolume);
				extern std::unordered_map<int, float> luavolumes;
				luavolumes.insert_or_assign(iFreeSoundID, 0);
			}
		}
	}

	//PE: Disable all terrain editor tools in testgame.
	old_render_params2 = ggterrain_global_render_params2.flags2;
	ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_MINI_MAP;
	ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_BRUSH_SIZE;

	//PE: Always start with weapon render on.
	extern bool bHideWeapons;
	bHideWeapons = false;
	extern bool bHideWeaponsMuzzle;
	extern bool bHideWeaponsSmoke;
	bHideWeaponsMuzzle = false;
	bHideWeaponsSmoke = false;

	// The map bounds can optionally be shown in testgame.
	extern void TestLevel_ToggleBoundary(bool, bool);
	TestLevel_ToggleBoundary(t.showtestgame2dbounds, t.showtestgame3dbounds);

	// if this was called from standalone, need to update graphics settings to match visuals just loaded
	if (t.game.gameisexe == 1)
	{
		// ensures when game restarted the last graphics settings refresh the level
		visuals_shaderlevels_update();
	}

	// at the point the level actually starts, use VR in standalone
	if (t.game.gameisexe == 1 && g.vrglobals.GGVREnabled == 2) g_iActivelyUsingVRNow = 1;
}

void game_masterroot_gameloop_afterexitgamemenu(void)
{
	if (t.game.jumplevel_s == "")
	{
		// can override jumplevel with 'advanced warning level filename' when LOAD level from MAIN MENU
		if (strcmp(t.game.pAdvanceWarningOfLevelFilename, "") != NULL)
		{
			t.game.jumplevel_s = t.game.pAdvanceWarningOfLevelFilename;
			if (strlen(Storyboard.gamename) > 0)
			{
				//PE: We use t.game.pAdvanceWarningOfLevelFilename later.
			}
			else
			{
				strcpy(t.game.pAdvanceWarningOfLevelFilename, "");
			}
		}
	}
	if (t.game.jumplevel_s != "")
	{
		//Load level.
		t.game.gameloop = 0;
	}
			
	timestampactivity(0,"leaving options page");
	g.titlesettings.updateshadowsaswell=0;
	//PE: Clear g_iMouseDeltaZ
	MouseMoveZ();
	LuaSetInt("g_MouseWheel", 0); //PE: Reset g_MouseWheel
}

// SetEscapeMenu: false leaves Escape to the scripts' own menu (back to true at each level start)
bool g_bEscapeMenu = true;

// the world (sounds, physics, animation, the player's gun and g_Time) is paused by the in-game menu and by SetGamePaused,
// and resumes only when neither holds it, so the two can overlap
int g_iGameWorldPauses = 0;
bool g_bGamePaused = false;

void game_pauseworld ( void )
{
	++g_iGameWorldPauses;
	if ( g_iGameWorldPauses > 1 ) return;
	t.tremembertimer= MAXTimer();
	game_main_snapshotsound ( );
	physics_pausephysics ( );
	entity_pauseanimations ( );
	extern void gun_SetObjectSpeed(int, float);
	if ( t.currentgunobj > 0 ) { if ( ObjectExist(t.currentgunobj)==1 ) { gun_SetObjectSpeed (  t.currentgunobj,0) ; } }
	if ( t.playercontrol.jetobjtouse>0 )
	{
		if ( ObjectExist(t.playercontrol.jetobjtouse) == 1  )  SetObjectSpeed (  t.playercontrol.jetobjtouse,0 );
	}
}

void game_resumeworld ( void )
{
	if ( g_iGameWorldPauses == 0 ) return;
	--g_iGameWorldPauses;
	if ( g_iGameWorldPauses > 0 ) return;
	extern void gun_SetObjectSpeed(int, float);
	if ( t.currentgunobj>0 ) { if ( ObjectExist(t.currentgunobj) == 1 ) { gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f); } }
	physics_resumephysics ( );
	entity_resumeanimations ( );
	t.aisystem.cumilativepauses= MAXTimer()-t.tremembertimer;
	game_main_snapshotsoundresume ( );
}

// SetGamePaused: pause the world as the in-game menu does, and its logic too (game_main_loop skips physics, the player,
// entities, AI, weapons and particles), while LUA, the HUD, music and rendering carry on
void game_setgamepaused ( bool bPause )
{
	if ( bPause == g_bGamePaused ) return;
	g_bGamePaused = bPause;
	extern void WickedCall_PauseEmitters(bool bPause);
	WickedCall_PauseEmitters(bPause);
	if ( bPause == true )
		game_pauseworld ( );
	else
		game_resumeworld ( );
}

void game_masterroot_gameloop_afterescapepressed(void)
{
	game_resumeworld ( );
	t.strwork = ""; t.strwork = t.strwork + "resuming game loop with flag "+Str(t.game.gameloop);
	timestampactivity(0, t.strwork.Get() );
	// Wipe out mouse deltas
	t.tMousemove_f = MouseMoveX() + MouseMoveY() + MouseZ(); t.tMousemove_f  = 0;

	//PE: Restart any ambient music tracks.
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		//PE: start any ambient music tracks.
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
		if (FileExist(t.visuals.sAmbientMusicTrack.Get()) == 1)
		{
			LoadSound(t.visuals.sAmbientMusicTrack.Get(), iFreeSoundID, 0, 1);
			if (SoundExist(iFreeSoundID) == 1)
			{
				LoopSound(iFreeSoundID);
				SetSoundVolume(iFreeSoundID, t.visuals.iAmbientMusicTrackVolume);
			}
		}
	}
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
	if (FileExist(t.visuals.sCombatMusicTrack.Get()) == 1)
	{
		LoadSound(t.visuals.sCombatMusicTrack.Get(), iFreeSoundID, 0, 1);
		if (t.gamevisuals.bEnableCombatMusicTrack)
		{
			if (SoundExist(iFreeSoundID) == 1)
			{
				LoopSound(iFreeSoundID);
				SetSoundVolume(iFreeSoundID, t.visuals.iCombatMusicTrackVolume);
			}
		}
	}

	// at the point we leave the in-game menu, resume VR mode while if required
	if (t.game.gameisexe == 1 && g.vrglobals.GGVREnabled == 2) g_iActivelyUsingVRNow = 1;
}

bool bMainLoopRunning = false;

bool game_masterroot_gameloop_loopcode(int iUseVRTest)
{
	// within gameloop, can have a in-game menu loop, so handle this if used
	if (g_iInGameMenuState == 1)
	{
		if (titleslua_main_loopcode() == true)
		{
			game_masterroot_gameloop_afterexitgamemenu();
			game_masterroot_gameloop_afterescapepressed();
			g_iInGameMenuState = 0;
		}
		else
		{
			return false;
		}
	}

	//  Game cycle loop
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"winddown mp_closeconnection");
	if ( t.game.gameloopwinddown == 1 )
	{
		if ( mp_closeconnection() == 1 )
		{
			t.game.gameloopwinddown = 0;
			t.game.gameloop = 0;
		}
	}

	// detect if standalone is a foreground window
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"obtain plrhasfocus");
	if ( t.game.gameisexe == 1 )
	{
		if ( g.gvrmode == 3 )
		{
			t.plrhasfocus = 1;
		}
		else
		{
			HWND hForeWnd = GetForegroundWindow();
			if ( GetWindowHandle() != hForeWnd ) 
				t.plrhasfocus = 0;
			else
				t.plrhasfocus = 1;
		}
	}

	// if controller active, also detect for START button press (same as ESCAPE)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"controller start button check");
	bool bControllerEscape = false;
	if ( g.gxbox > 0 && JoystickFireXL(9) == 1 ) bControllerEscape = true;

	// VR support can escape to in-game menu with button A
	if (g.vrglobals.GGVREnabled > 0 && g_iActivelyUsingVRNow == 1)
	{
		if (GGVR_RightController_Button1() == 1) bControllerEscape == true;
	}

	//  trigger options page or exit test level (SetEscapeMenu(0) leaves Escape and the START button to the scripts, though
	//  shift and Escape still leave a test level)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"escape button check");
	bool bEscapePressed = false;
	if ( g_bEscapeMenu == true )
	{
		if ( EscapeKey() == 1 || bControllerEscape == true ) bEscapePressed = true;
	}
	else
	{
		if ( t.game.gameisexe == 0 && (KeyState(42) == 1 || KeyState(54) == 1) && EscapeKey() == 1 ) bEscapePressed = true;
	}
	if ( bEscapePressed == true )
	{
		// at the point we enter the in-game menu, stop VR mode if required
		if (t.game.gameisexe == 1 && g.vrglobals.GGVREnabled == 2) g_iActivelyUsingVRNow = 0;

		// can perform some extra debug snapshots when enter in-game menu - useful!
		if (g.gproducelogfiles == 2) GGTerrain::GGTerrain_DebugOutputFlattenedAreas();

		game_pauseworld ( );
		while ( EscapeKey() != 0 ) {}
		if ( t.game.gameisexe == 0 ) // no menu in multiplayer test mode && t.game.runasmultiplayer == 0 ) 
		{
			if ( t.game.runasmultiplayer == 1 )
			{
				// wait until connection closed, then exit game loop
				t.game.gameloopwinddown = 1; 
			}
			else
			{
				// leave right away
				t.game.gameloop=0; 
				t.game.levelloop=0; 
				t.game.masterloop=0;
			}
			if ( t.conkit.editmodeactive == 1 ) 
			{
				//conkitedit_switchoff ( );
			}
		}
		else
		{
			g.titlesettings.updateshadowsaswell = 1;
			timestampactivity(0, "entering options page");
			titleslua_init();
			g_iInGameMenuState = 1;
			titleslua_main("gamemenu");
			if (titleslua_main_loopcode() == true)
			{
				// title systen left right away, continue
				game_masterroot_gameloop_afterexitgamemenu();
			}
			else
			{
				// cycle
				return false;
			}
		}
		game_masterroot_gameloop_afterescapepressed();
		g_iInGameMenuState = 0;
	}

	// Fade in gamescreen (using post process shader)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"fade game screen logic");
	if ( t.postprocessings.fadeinvalue_f<1.0 ) 
	{
		// Hide Lua Sprites
		HideOrShowLUASprites ( true );
		if ( t.postprocessings.fadeinvalue_f <= 0 )
		{
			// only if in test game mode, standalone already set volume values (in title.lua)						
			if ( t.game.gameisexe == 0 )
			{
				// set music and sound global volumes
				audio_volume_init ( );
			}
			else if (!FileExist("savegames\\sounds.dat")) 
			{
				//PE: This is not always in standalone ? , so:
				audio_volume_init();
			}

			// and update internal volume values so music update can use volumes!
			audio_volume_update ( );
			//PE: LUA first calls has already been made, so restore the settings after this reset.
			void restore_last_lua_volumes_settings(void);
			restore_last_lua_volumes_settings();
		}
		//PE: Wicked seams a littler faster then classic , strange ?
		t.postprocessings.fadeinvalue_f = t.postprocessings.fadeinvalue_f + (g.timeelapsed_f*0.08f);
		if (  t.postprocessings.fadeinvalue_f >= 1.0f ) 
		{
			//PE: Disable 3D hiding. we are all done and ready to play.
			extern int iKeepBackgroundForFrames;
			iKeepBackgroundForFrames = 0;
			t.postprocessings.fadeinvalue_f=1.0f;
			//g.globals.hidelowfpswarning = 0; // this overrides the SETUP.INI setting
			HideOrShowLUASprites ( false );
			EnableAllSprites(); // the disable is called in DarkLUA by ResetFade() black out command when load game position
		}
		extern bool bFakeStandaloneTest;
		if ( (t.game.gameisexe == 1 || bFakeStandaloneTest ) && t.postprocessings.fadeinvalue_f < 1.0) //PE: Only standalone and , not on last frame.
		{
			extern bool bImGuiInTestGame;
			extern bool bRenderTabTab;
			extern bool bBlockImGuiUntilNewFrame;
			extern bool bImGuiRenderWithNoCustomTextures;
			extern bool g_bNoGGUntilGameGuruMainCalled;
			extern bool bImGuiFrameState;

			if ((bImGuiInTestGame) && !bRenderTabTab && !bImGuiFrameState)
			{
				//We need a new frame.
				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
				bRenderTabTab = true;
				bBlockImGuiUntilNewFrame = false;
				bImGuiRenderWithNoCustomTextures = false;
				extern bool bSpriteWinVisible;
				bSpriteWinVisible = false;
			}

			ImGuiViewport* mainviewport = ImGui::GetMainViewport();
			if (mainviewport)
			{
				ImDrawList* drawlist = ImGui::GetForegroundDrawList(mainviewport);
				if (drawlist)
				{
					ImVec4 monitor_col = ImVec4(0.0, 0.0, 0.0, 1.0 - t.postprocessings.fadeinvalue_f); //Fade in.
					drawlist->AddRectFilled(ImVec2(-1, -1), ImGui::GetMainViewport()->Size, ImGui::GetColorU32(monitor_col));
				}
			}
		}
		t.postprocessings.fadeinvalueupdate=1;
	}

	// handle fading
	bMainLoopRunning = true;

	//  Immunity when respawn
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"handle player immunity");
	if (  t.huddamage.immunity>0 && g_bGamePaused == false )
	{
		t.huddamage.immunity=t.huddamage.immunity-(10*g.timeelapsed_f);
		if (  t.huddamage.immunity<0  )  t.huddamage.immunity = 0;
	}

	//  Run all game subroutines		
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling game_main_loop");
	game_main_loop ( );

	//  Update screen
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling game_sync");
	game_sync ( );

	// determine if end of game loop
	if (t.game.gameloop != 1)
		return true;
	else
		return false;
}

