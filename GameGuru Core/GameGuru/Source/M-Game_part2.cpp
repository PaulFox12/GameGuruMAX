void game_masterroot_gameloop_afterloopcode(int iUseVRTest)
{
	// at the point we leave the loop, stop VR mode when leaving the level if required
	if (t.game.gameisexe == 1 && g.vrglobals.GGVREnabled == 2) g_iActivelyUsingVRNow = 0;

	// must stop extra thread right away
	extern void GuruLoopStopExtraThread(void);
	GuruLoopStopExtraThread();

	// a level left while SetGamePaused holds the world resumes it, so the editor and the next level start unpaused
	game_setgamepaused ( false );

	// first save current level stats before reset LUA
	// must now preserve state of level when leave it
	char pLUACustomSaveCall[256];
	strcpy ( pLUACustomSaveCall, "GameLoopSaveStats" );
	LuaSetFunction ( pLUACustomSaveCall, 1, 0 ); 
	LuaPushInt(g_Storyboard_Current_Level);
	LuaCall ();

	// free any lua activity (restore FOV if ingame activity there)
	timestampactivity(0,"finalising LUA system before reset");
	lua_quitting(); 

	// Rest any internal game variables
	game_main_stop ( );

	//PE: Enable collision.
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
						WickedCall_SetDisableCollision(pObject, false);
					}
				}
			}
		}
	}


	//PE: Draw call optimizer
//	if (!g.disable_drawcall_optimizer)
	{
		//PE: restore all states. delete all batched objects.
		for (t.e = 1; t.e <= g.entityelementlist; t.e++)
		{
			t.entid = t.entityelement[t.e].bankindex;
			t.obj = t.entityelement[t.e].obj;

			if (t.obj > 0 && t.e < g.entityelementlist && ObjectExist(t.obj) == 1)
			{
				if (t.entityelement[t.e].draw_call_obj > 0) {

					if (t.entityelement[t.e].draw_call_obj > 0 && ObjectExist(t.entityelement[t.e].draw_call_obj) == 1) {
						DeleteObject(t.entityelement[t.e].draw_call_obj);
						t.entityelement[t.e].draw_call_obj = 0;
						if(ObjectExist(t.obj) == 1)
							ShowObject(t.obj);
					}

					if (t.entityelement[t.e].dc_obj[0] > 0 && ObjectExist(t.entityelement[t.e].dc_obj[0]) == 1)
						ShowObject(t.entityelement[t.e].dc_obj[0]);
					if (t.entityelement[t.e].dc_obj[1] > 0 && ObjectExist(t.entityelement[t.e].dc_obj[1]) == 1)
						ShowObject(t.entityelement[t.e].dc_obj[1]);
					if (t.entityelement[t.e].dc_obj[2] > 0 &&  ObjectExist(t.entityelement[t.e].dc_obj[2]) == 1)
						ShowObject(t.entityelement[t.e].dc_obj[2]);
					if (t.entityelement[t.e].dc_obj[3] > 0 && ObjectExist(t.entityelement[t.e].dc_obj[3]) == 1)
						ShowObject(t.entityelement[t.e].dc_obj[3]);
					if (t.entityelement[t.e].dc_obj[4] > 0 && ObjectExist(t.entityelement[t.e].dc_obj[4]) == 1)
						ShowObject(t.entityelement[t.e].dc_obj[4]);
					if (t.entityelement[t.e].dc_obj[5] > 0 && ObjectExist(t.entityelement[t.e].dc_obj[5]) == 1)
						ShowObject(t.entityelement[t.e].dc_obj[5]);

					t.entityelement[t.e].dc_obj[0] = 0;
					t.entityelement[t.e].dc_obj[1] = 0;
					t.entityelement[t.e].dc_obj[2] = 0;
					t.entityelement[t.e].dc_obj[3] = 0;
					t.entityelement[t.e].dc_obj[4] = 0;
					t.entityelement[t.e].dc_obj[5] = 0;

					t.entityelement[t.e].dc_entid[0] = 0;
					t.entityelement[t.e].dc_entid[1] = 0;
					t.entityelement[t.e].dc_entid[2] = 0;
					t.entityelement[t.e].dc_entid[3] = 0;
					t.entityelement[t.e].dc_entid[4] = 0;
					t.entityelement[t.e].dc_entid[5] = 0;
				}
			}
		}
	}

	bool bFreeLevelAfterLuaScreen = true;

	//PE: Moved here as standalone will delete all objects, so we could not free DCO objects.
	//  Free any level resources
	if (!bFreeLevelAfterLuaScreen)
	{
		timestampactivity(0, "game_freelevel"); //PE: Additional debug to remove
		game_freelevel();
	}
	else
	{
		bulletholes_free();
		// free any HUD screen objects
		if (ObjectExist(g.hudscreen3dobjectoffset) == 1) DeleteObject(g.hudscreen3dobjectoffset);
		hud_free();
		game_stopallsounds();
		lua_freeprompt3d();
		lua_freeallperentity3d();
		lighting_free();
		darkai_free();
		BPhys_ClearDebugDrawData();
	}

	// must reset LUA here for clean end-game-screens
	// ensure LUA is completely reset before loading new ones in
	// the free call is because game options menu init, but not freed back then
	timestampactivity(0,"resetting LUA system");
	titleslua_free ( );
	LuaReset (  );
	extern std::unordered_map<int, sFrame*> lastHitFrame;
	lastHitFrame.clear();

	//PE: restore waterline.
	t.terrain.waterliney_f = g.gdefaultwaterheight;

	reset_env_particles();
	void delete_notused_decal_particles(void);
	//PE: Make sure to delete not used decal particles.

	delete_notused_decal_particles();

	//PE: Clear all wicked particle effects created by lua.
	void CleanUpEmitterEffects(void);
	CleanUpEmitterEffects();

	//PE: restore sun position for editor.
	t.terrain.sundirectionx_f = t.terrain.skysundirectionx_f;
	t.terrain.sundirectiony_f = t.terrain.skysundirectiony_f;
	t.terrain.sundirectionz_f = t.terrain.skysundirectionz_f;

	// 240316 - additional cleanup
	mp_freefadesprite ( );

	// Advance level to 'next one' or 'win game'
	timestampactivity(0,"end of level stage");
	if ( t.game.gameisexe == 1 )
	{
		timestampactivity(0,"game is standalone exe");
		if ( t.game.quitflag == 0 ) 
		{
			timestampactivity(0,"game has not quit");
			bool bUseOldSystem = true;
			if ( strlen(Storyboard.gamename) > 0)
			{
				bUseOldSystem = false;
				if (t.game.lostthegame == 1)
				{
					// Get output link to lose screen from current level node.
					int actiontype = FindNextLevel(g_Storyboard_Current_Level, g_Storyboard_Current_fpm , 1);
					if (actiontype == 2)
					{
						//Found a lose output to a screen.
						std::string script = g_Storyboard_Current_fpm;
						replaceAll(script, ".lua", "");
						char tmp[MAX_PATH];
						sprintf(tmp, "Project LUA script : %s", script.c_str());
						timestampactivity(0, tmp);
						sky_hide();
						if (script != "title")
						{
							titleslua_init();
							titleslua_main((char *)script.c_str());
							//PE: We need a blocking run or screen is not displayed. t.game.levelloop = 0; will start title screen.
							titleslua_blocking_run();
							titleslua_free();
						}
						sky_show();
						t.game.levelloop = 0;
					}
					else
					{
						timestampactivity(0, "Project LUA script : lose");
						sky_hide();
						titleslua_init();
						titleslua_main("lose");
						//PE: We need a blocking run or screen is not displayed. t.game.levelloop = 0; will start title screen.
						titleslua_blocking_run();
						titleslua_free();
						sky_show();
						t.game.levelloop = 0;
					}
				}
				else
				{
					if (strcmp(t.game.pAdvanceWarningOfLevelFilename, "") != NULL)
					{
						//From load game
						t.game.jumplevel_s = t.game.pAdvanceWarningOfLevelFilename;
						strcpy(t.game.pAdvanceWarningOfLevelFilename, "");
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
					else
					{
						//Get next level.
						int actiontype = FindNextLevel(g_Storyboard_Current_Level, g_Storyboard_Current_fpm);
						if (actiontype == 2 || actiontype == 3)
						{
							//We got a screen jump to that screen.
							std::string script = g_Storyboard_Current_fpm;
							replaceAll(script, ".lua", "");
							char tmp[MAX_PATH];
							sprintf(tmp, "Project LUA script : %s", script.c_str());
							timestampactivity(0, tmp);
							if (script != "title")
							{
								//PE: Block for one frame so we do not see old screens.
								extern bool bBlockImGuiUntilNewFrame;
								bBlockImGuiUntilNewFrame = true;
								sky_hide();
								titleslua_init();
								titleslua_main((char *)script.c_str());
								//PE: We need a blocking run or screen is not displayed. t.game.levelloop = 0; will start title screen.
								titleslua_blocking_run();
								titleslua_free();
								sky_show();
							}
							if (actiontype == 3)
							{
								// is the actual game won screen, need to leave level loop afer this!
								t.game.levelloop = 0;
							}
						}
						else
						{
							//We got a new level, use it.
							std::string nextlevel = g_Storyboard_Current_fpm;
							replaceAll(nextlevel, "mapbank\\", "");
							replaceAll(nextlevel, ".fpm", "");
							char tmp[MAX_PATH];
							sprintf(tmp, "Project t.game.jumplevel_s : %s", nextlevel.c_str());
							timestampactivity(0, tmp);
							t.game.jumplevel_s = nextlevel.c_str();

							//PE: Door collision not working on second level, fixed in script.
							//PE: https://github.com/Dark-Basic-Software-Limited/GameGuruRepo/issues/6246
						}
					}
				}
			}
			if (bUseOldSystem)
			{
				//PE: issue https://github.com/TheGameCreators/GameGuruRepo/issues/444
				if (Len(t.game.jumplevel_s.Get()) > 0)
				{
					//  goes around and loads this level name
					timestampactivity(0, "game is loading non-linear level map:");
					timestampactivity(0, "t.game.jumplevel_s.Get()");
				}
				else
				{
					// win, lose or next level pages
					if (t.game.lostthegame == 1)
					{
						//titles_gamelostpage ( );
						timestampactivity(0, "LUA script : lose");
						sky_hide();
						titleslua_init();
						titleslua_main("lose");
						//PE: We need a blocking run or screen is not displayed. t.game.levelloop = 0; will start title screen.
						titleslua_blocking_run();
						titleslua_free();
						sky_show();
						t.game.levelloop = 0;
					}
					else
					{
						t.game.level = t.game.level + 1;
						if (t.game.level > t.game.levelmax)
						{
							timestampactivity(0, "LUA script : win");
							sky_hide();
							titleslua_init();
							titleslua_main("win");
							//PE: We need a blocking run or screen is not displayed. t.game.levelloop = 0; will start title screen.
							titleslua_blocking_run();
							titleslua_free();
							sky_show();
							t.game.levelloop = 0;
						}
						else
						{
							timestampactivity(0, "LUA script : nextlevel");
							sky_hide();
							titleslua_init();
							titleslua_main("nextlevel");
							//PE: We need a blocking run or screen is not displayed.
							titleslua_blocking_run();
							sky_show();
						}
					}
				}
			}
		}
	}

	if (bFreeLevelAfterLuaScreen)
	{
		game_freelevel();
		//PE: Try to avoid the screen between win/loose... and next screen (show blank map).
		extern int iBlockRenderingForFrames;
		extern bool g_bNoSwapchainPresent;
		iBlockRenderingForFrames = 5;
		g_bNoSwapchainPresent = true;
	}


	t.game.quitflag=0;

	//  If was in multiplayer session, no level loop currently
	if (  t.game.runasmultiplayer == 1 ) 
	{
		t.game.levelloop=0;
	}

	// PE: Dump image usage after level.
	if (g.memgeneratedump == 1) 
	{
		timestampactivity(0, "DumpImageList after freeing level data.");
		DumpImageList(); 
	}

	g_RecastDetour.cleanupDebugRender();
	g_bShowRecastDetourDebugVisuals = false;

	// 250619 - very large levels can fragment 32 bit memory after a few levels
	// so this mode will restart the executable, and launch the new level
	// crude solution until 64 bit allows greater memory referencing
	if ( t.game.allowfragmentation == 2 )
		t.game.levelloop = 0;

	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		//PE: Stop any ambient music tracks.
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
		{
			StopSound(iFreeSoundID);
		}
		if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
	}
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
	{
		StopSound(iFreeSoundID);
	}
	if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
	//PE: restore old terrain settings.
	ggterrain_global_render_params2.flags2 = old_render_params2;

	//PE: Always turn back on weapon render.
	extern bool bHideWeapons;
	bHideWeapons = false;
	extern bool bHideWeaponsMuzzle;
	extern bool bHideWeaponsSmoke;
	bHideWeaponsMuzzle = false;
	bHideWeaponsSmoke = false;

}

bool game_masterroot_levelloop_initcode(int iUseVRTest)
{
	// first hide rendering of 3D while we set up
	SyncMaskOverride ( 0 );

	//  Optionally set resolution for game and setup for dependencies
	timestampactivity(0,"_game_setresolution");
	if (  t.game.set.resolution == 1 ) 
	{
		game_setresolution ( );
		game_postresolutionchange ( );
		t.game.set.resolution=0;
	}

	//  One-off splash screen or animation
	if (  t.game.set.initialsplashscreen == 1 ) 
	{
		t.game.set.initialsplashscreen=0;
	}

	//  Setup level progression settings
	t.game.firstlevelinitializesanygameprojectlua = 123;
	t.game.level=1;
	t.game.levelmax=1;
	t.game.levelloop=1;
	t.game.levelendingcycle=0;
	t.game.lostthegame=0;
	t.game.jumplevel_s="";
	strcpy ( t.game.pAdvanceWarningOfLevelFilename, "" );

	//  specify first level to load (same name as executable)
	if (  t.game.gameisexe == 1 ) 
	{
		t.tapp_s=Appname();
		for ( t.n = Len(t.tapp_s.Get()) ; t.n >= 1 ; t.n+= -1 )
		{
			if (  t.tapp_s.Get()[t.n-1] == '\\' || t.tapp_s.Get()[t.n-1] == '/' ) 
			{
				t.tapp_s=Right(t.tapp_s.Get(),Len(t.tapp_s.Get())-t.n);
				break;
			}
		}
		t.game.jumplevel_s=Left(t.tapp_s.Get(),Len(t.tapp_s.Get())-4);


		//PE: Check if we are using a storyboard project
		strcpy(Storyboard.gamename, "");
		load_storyboard(t.game.jumplevel_s.Get());
		if (strlen(Storyboard.gamename) > 0)
		{
			//PE: Got a project. find first level to load.
			FindFirstLevel(g_Storyboard_First_Level_Node, g_Storyboard_First_fpm);
			g_Storyboard_Current_Level = g_Storyboard_First_Level_Node;
			strcpy(g_Storyboard_Current_fpm, g_Storyboard_First_fpm);
			//Clean name.
			
			std::string sLevelTitle = g_Storyboard_First_fpm;
			replaceAll(sLevelTitle, ".fpm", "");
			replaceAll(sLevelTitle, "mapbank\\", "");
			t.game.jumplevel_s = sLevelTitle.c_str();
		}
	}

	//  Title init - If this is just a test game, we only need to set default volumes
	timestampactivity(0,"_titles_init");
	if (  t.game.gameisexe == 1 || t.game.runasmultiplayer == 1 ) 
	{
		titles_init ( );
	}
	else
	{
		t.gamesounds.sounds=100;
		t.gamesounds.music=100;
	}

	// Do title page
	timestampactivity(0,"_titles_titlepage");
	if (t.game.gameisexe == 1 && t.game.ignoretitle == 0)
	{
		sky_hide();
		titles_loadingpageinit();
		titleslua_init();

		// this is a special flag set when quit from game (avoids rogue IMGUI renders that have been deleted in the restart)
		extern bool bBlockImGuiUntilFurtherNotice;
		bBlockImGuiUntilFurtherNotice = false;

		// 250619 - for straight-through level loading, still need to handle title resources
		// (which include sound loading/playing - fixes 3D sound delay issue?!?)
		if (g.iStandaloneIsReloading == 2)
		{
			titleslua_main_inandout("title");
		}
		else
		{
			bool bValid = false;
			int FindFirstSplashNode(void);
			int nodeid = FindFirstSplashNode();
			if (nodeid >= 0)
			{
				int index = 1;

				int iLinkTo = Storyboard.Nodes[nodeid].output_linkto[0];
				int iLinkScreen = -1;
				for (int i = 0; i < STORYBOARD_MAXNODES; i++)
				{
					if (Storyboard.Nodes[i].used)
					{
						for (int l = 0; l < STORYBOARD_MAXOUTPUTS; l++)
						{
							if (iLinkTo > 0 && iLinkTo == Storyboard.Nodes[i].input_id[l])
							{
								iLinkScreen = i;
								break;
							}
						}
					}
				}
				if (iLinkScreen >= 0)
				{
					//PE: Check if we got a video outlink.
					for (int ll = 0; ll < STORYBOARD_MAXWIDGETS; ll++)
					{
						if (Storyboard.Nodes[iLinkScreen].widget_used[ll])
						{
							if (Storyboard.Nodes[iLinkScreen].widget_type[ll] == STORYBOARD_WIDGET_VIDEO)
							{
								if (Storyboard.Nodes[iLinkScreen].widget_action[ll] == STORYBOARD_ACTIONS_GOTOSCREEN)
								{
									bValid = true;
									break;
								}
							}
						}
					}
					if (bValid)
					{
						// screens can have same name (old corruption issue), so new method to identify screen by node
						std::string node_ident_name = ":node:";
						node_ident_name += std::to_string(iLinkScreen);
						t.s_s = node_ident_name.c_str();
						lua_switchpage();
						//bLuaPageClosing = true; //always stop music.
						//iRet = STORYBOARD_ACTIONS_GOTOSCREEN;
					}
				}
			}
			if (bValid)
			{
				titleslua_main(t.s_s.Get());
				strcpy(t.game.pSwitchToLastPage, "title");
			}
			else
			{
				// start title system loop
				titleslua_main("title");
			}
			return true;
		}
	}

	// no title system loop, continue
	game_masterroot_levelloop_initcode_aftertitleloop();
	return false;
}

void game_masterroot_levelloop_initcode_aftertitleloop(void)
{
	// if game executable and not ignoring title system
	if (t.game.gameisexe == 1 && t.game.ignoretitle == 0)
	{
		titleslua_free();
		sky_show();
	}

	// 250619 - can relaunch game executable at a specific level (reduce memory fragmentation)
	if ( g.iStandaloneIsReloading == 2 )
	{
		t.game.jumplevel_s = g.sStandaloneIsReloadingLevel;
		t.luaglobal.gamestatechange = atoi(g.sStandaloneIsReloadingLevelGameStatChange.Get());
		g.sStandaloneIsReloadingLevel = "";
		g.sStandaloneIsReloadingLevelGameStatChange = "";
	}

	// Standaline Multiplayer HOST/JOIN screen
	if ( t.game.runasmultiplayer == 1 ) 
	{
		// Multiplayer init
		mp_fullinit();
		g.mp.mode = MP_MODE_MAIN_MENU;
		timestampactivity(0,"_titles_steampage");
		t.game.cancelmultiplayer=0;
		SetCameraView (  0,0,1,1 );
		titles_steampage ( );
		if ( t.game.cancelmultiplayer == 1 ) 
		{
			// user selected BACK (cancel multiplayer)
			mp_fullclose();
			t.game.levelloop=0;
		}
		else
		{
			// proceed into level loop where multiplayer spawn markers are detected and ghosts loaded
		}
	}

	// Initialise gun system (transcends per-level initialisations)
	t.game.levelplrstatsetup = 1;
	gun_restart ( );
	gun_resetactivateguns ( );
}

bool game_masterroot_levelloop_loopcode(int iUseVRTest)
{
	// Level loop will run while level progression is in progress
	if (t.game.levelloop != 1)
		return true;
	else
		return false;
}

void game_masterroot_levelloop_afterloopcode(int iUseVRTest)
{
	// Free any game resources
	game_freegame ( );

	if ( t.game.runasmultiplayer == 1 ) 
	{
		mp_free_game ( );
		mp_cleanupGame ( );
		if (  g.mp.goBackToEditor  ==  1 ) 
		{
			g.mp.goBackToEditor = 0;
			t.game.masterloop = 0;
		}
	}

	// get rid of debris and particles that may be lingering
	explosion_cleanup ( );

	ravey_particles_hide_all_particles();

	// if ignored title, exit now
	if (  t.game.ignoretitle == 1 && t.game.runasmultiplayer == 0  )  t.game.masterloop = 0;

	// Master loop end
	t.game.allowfragmentation_mainloop = t.game.masterloop;
	if (t.game.allowfragmentation == 0 || t.game.allowfragmentation == 2) t.game.masterloop = 0; //break;
}

void game_masterroot_initcode(int iUseVRTest)
{
	// prevent any VR if VRtest is off
	if (t.game.gameisexe == 1)
	{
		// in standalonme mode, GGVRUsingVRSystem set 1 elsehwere as needed
	}
	else
	{
		// in test level mode, can toggle this
		if (iUseVRTest == 0) g.vrglobals.GGVRUsingVRSystem = 0;
	}
	g_iActivelyUsingVRNow = iUseVRTest;

	// GGMAX 3.124: Lua's io.open goes through GG_fopen again (DarkLUA_part7.cpp), which creates savegames\ in
	// the writable area on the first write, as DX11 did. This install-folder copy is only the fallback for when
	// that creation fails (a UNC custom write folder, Controlled Folder Access): GG_fopen then opens the install path
	if ( PathExist("savegames") == 0 ) MakeDirectory("savegames");

	//  Load all one-off non-graphics assets
	timestampactivity(0,"_game_oneoff_nongraphics");
	game_oneoff_nongraphics ( );

	// Pick the HUD screen that should be shown at the start of the level
	t.game.activeStoryboardScreen = -1;
	for (int i = 0; i < STORYBOARD_MAXNODES; i++)
	{
		if (Storyboard.Nodes[i].showAtStart)
		{
			t.game.activeStoryboardScreen = i;
			break;
		}
	}

	//  Master loop will run until whole game terminated
	t.game.masterloop=1;
}

bool game_masterroot_loopcode(int iUseVRTest)
{
	// state engine to handle nested loops (master / level / gameloop)
	if (g_iMasterRootState == 0)
	{
		if (game_masterroot_levelloop_initcode(iUseVRTest) == true)
		{
			// go to title loop first (below)
			g_iMasterRootState = 51;
		}
		else
		{
			// title system not used, continue
			g_iMasterRootState = 1;
		}
	}
	if (g_iMasterRootState == 1)
	{
		if (game_masterroot_levelloop_loopcode(iUseVRTest) == true)
		{
			g_iMasterRootState = 6;
		}
		else
		{
			g_iMasterRootState = 2;
		}
	}
	if (g_iMasterRootState == 2)
	{
		game_masterroot_gameloop_initcode(iUseVRTest);
		g_iMasterRootState = 3;
	}
	if (g_iMasterRootState == 3)
	{
		if (game_masterroot_gameloop_loopcode(iUseVRTest) == true)
		{
			g_iMasterRootState = 4;
		}
	}
	if (g_iMasterRootState == 4)
	{
		game_masterroot_gameloop_afterloopcode(iUseVRTest);
		g_iMasterRootState = 5;
	}
	if (g_iMasterRootState == 5)
	{
		if (t.game.levelloop == 1 && Len(t.game.jumplevel_s.Get()) > 0 )
		{
			g_iMasterRootState = 1;
		}
		else
		{
			g_iMasterRootState = 6;
		}
	}
	if (g_iMasterRootState == 6)
	{
		game_masterroot_levelloop_afterloopcode(iUseVRTest);
		g_iMasterRootState = 0;
	}

	// title system state control
	if (g_iMasterRootState == 51)
	{
		if ( titleslua_main_loopcode() == true )
		{
			game_masterroot_levelloop_initcode_aftertitleloop();
			g_iMasterRootState = 2;
		}
	}

	// determine if end of master loop
	if (t.game.masterloop != 1)
		return true;
	else
		return false;
}

void game_masterroot_afterloopcode(int iUseVRTest)
{
	// End splash if EXE is advertising
	if ( t.game.set.endsplash == 1 ) 
	{
		t.game.set.endsplash=0;
	}

	// restore VR activity (vrtest flag has done its job) 
	g.vrglobals.GGVRUsingVRSystem = 1;
	g_iActivelyUsingVRNow = 0;
	master.StopVR();
	// restore normal rendering activity when finish game run
	SyncMaskOverride ( 0xFFFFFFFF );
	// cannot rely on postprocess to restore, so do so here when return
	SetCameraView ( 0, 0, 0, GetDisplayWidth(), GetDisplayHeight() );
}

void game_masterroot(int iUseVRTest)
{
	bool bRunLoop = true;
	game_masterroot_initcode(iUseVRTest);
	g_iMasterRootState = 0;
	while (bRunLoop == true)
	{
		if (game_masterroot_loopcode(iUseVRTest) == true) bRunLoop = false;
	}
	game_masterroot_afterloopcode(iUseVRTest);
}

void game_setresolution ( void )
{
	//  set game resolution here
	t.multisamplingfactor=0;
	t.multimonitormode=0;
	SetDisplayMode (  GetDesktopWidth(),GetDesktopHeight(),32,g.gvsync,t.multisamplingfactor,t.multimonitormode );
	SyncOn (   ); SyncRate (  0  ); Sync (   ); SetAutoCamOff (  );
	DisableEscapeKey (  );
}

void game_postresolutionchange ( void )
{
}

void game_oneoff_nongraphics ( void )
{
	// Trigger a sound stops initial slow-down?
	if (g.silentsoundoffset > 0)
	{
		if (SoundExist(g.silentsoundoffset) == 1)
		{
			PlaySound(g.silentsoundoffset);
			PositionSound(g.silentsoundoffset, 0, 0, 0);
		}
	}

	//  Force all weapons into weapon slots (initial default start)
	gun_gatherslotorder ( );
}

void game_loadinentitiesdatainlevel ( void )
{
	// Load player settings
	timestampactivity(0,"Load player config");
	mapfile_loadplayerconfig ( );

	//PE: Free master on load.
	bool bRetainMasterObjects = false;
	if (t.game.gameisexe == 1)
	{
		static cstr old_level_name = "";
		if (g.projectfilename_s == old_level_name)
		{
			bRetainMasterObjects = true;
		}
		old_level_name = g.projectfilename_s;
	}

	if (!bRetainMasterObjects)
	{
		// Load entity bank
		t.screenprompt_s = "LOADING ENTITY BANK";
		if (t.game.gameisexe == 0) printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
		timestampactivity(0, t.screenprompt_s.Get());
		entity_loadbank();
	}

	// Load entity elements
	t.screenprompt_s="LOADING ENTITY ELEMENTS";
	if ( t.game.gameisexe == 0 ) printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	timestampactivity(0, "s2:entity_loadelementsdata()");
	entity_loadelementsdata ( );
	timestampactivity(0, "e2:entity_loadelementsdata()");

	// LB: quick sanity check to screen out corrupt entityparent ID references
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		t.entid = t.entityelement[t.e].bankindex;
		if (t.entid > 0 && t.entid >= t.entityprofile.size())
		{
			// ensures can never reference an out of bounds entityparent ID
			t.entityelement[t.e].bankindex = 0;
		}
	}
}

void game_loadinleveldata ( void )
{
	//  Load waypoints
	t.screenprompt_s="LOADING WAYPOINTS DATA";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	waypoint_loaddata ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
	waypoint_recreateobjs ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  Load terrain
	t.screenprompt_s="LOADING TERRAIN DATA";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	terrain_loaddata ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  Recreate all entities in level
	t.screenprompt_s="CREATE ENTITY OBJECTS";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());

	char debug[MAX_PATH];
	sprintf(debug, "Setup objects: %ld", g.entityelementlist);
	timestampactivity(0, debug);
	extern bool bNoHierarchySorting;
	bNoHierarchySorting = true;
	extern int iInstancedTotal;
	iInstancedTotal = 0;

	for ( t.tupdatee = 1 ; t.tupdatee<=  g.entityelementlist; t.tupdatee++ )
	{
		entity_updateentityobj ( );
		if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
	}

	sprintf(debug, "Instanced objects: %ld", iInstancedTotal);
	timestampactivity(0, debug);
	bNoHierarchySorting = false;

	t.terrain.terrainpainteroneshot=0;

	//  default start position is edit-camera XZ (Y done in physics init call)
	t.terrain.playerx_f=25000;
	t.terrain.playery_f=0;
	t.terrain.playerz_f=25000;
	t.terrain.playerax_f=0.0;
	t.terrain.playeray_f=0.0;
	t.terrain.playeraz_f=0.0;
	t.camangy_f=0;

	//  hide all markers
	t.screenprompt_s="GAME OBJECT CLEANUP";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.obj=t.entityelement[t.e].obj;
		if (  t.obj>0 ) 
		{
			if (  ObjectExist(t.obj) == 1 ) 
			{
				t.entid=t.entityelement[t.e].bankindex;
				if (  t.entityprofile[t.entid].ismarker != 0 ) 
				{
					//  all markers must be hidden
					HideObject (  t.obj );
				}
				if (  t.entityprofile[t.entid].addhandlelimb>0 ) 
				{
					//  hide decal handles
					HideLimb (  t.obj,t.entityprofile[t.entid].addhandlelimb );

				}
			}
		}
	}
	waypoint_hideall ( );
}

float GetWAVtoLIPProgress(void);

void game_preparelevel ( void )
{
	// need the latest refreshed gunlist for each new level
	extern bool g_bGunListNeedsRefreshing;
	g_bGunListNeedsRefreshing = true;

	//  Init music system first to make sure nothing is playing during the load sequence
	// 271115 - has to be here as LUA triggers Play Music during its INIT but if MUSIC_INIT was
	// called last it would stop the default music from playing, but setting volume to zero 
	// will achieve the same result of keeping music silent until ready
	music_init ( );
	t.audioVolume.musicFloat = 0;
	t.audioVolume.soundFloat = 0;

	//  Load all assets required to perform level
	timestampactivity(0,"_game_preparelevel:");

	//  Switch on post process if it was switched off (init called later in _finally subroutine)
	postprocess_on ( );

	//  (re)load any player sounds (player style specified in player start marker)
	material_loadplayersounds ( );

	//  init character sound
	character_sound_init ( );

	//  particles
	ravey_particles_init ( );
	reset_env_particles ( );

	// bulletholes return at last
	bulletholes_init();

	//  Allow Steam to refresh (so does not stall)
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  HUD graphics
	t.screenprompt_s = "LOADING HUD GRAPHICS";
	timestampactivity(0,t.screenprompt_s.Get());
	hud_init ( );

	//  Load sky spec (for any shaders later that require sun-pos)
	t.screenprompt_s="LOADING SKY";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	t.terrainskyspecinitmode=0;

	//PE: Remember sun angle.
	float oSx = t.visuals.SunAngleX;
	float oSy = t.visuals.SunAngleY;
	float oSz = t.visuals.SunAngleZ;

	sky_skyspec_init( false );

	//PE: In wicked we want to restore the sun angle from the map and not use skyspec.ini settings. (only when loading a old level).
	if (t.visuals.skyindex == 0 || t.visuals.bDisableSkybox)
	{
		//PE: Only if we re not using a simple skybox.
		t.terrain.sunrotationx_f = t.visuals.SunAngleX = oSx;
		t.terrain.sunrotationy_f = t.visuals.SunAngleY = oSy;
		t.terrain.sunrotationz_f = t.visuals.SunAngleZ = oSz;
	}

	//  Load in HUD Layer assets
	t.screenprompt_s = "LOADING HUD LAYERS";
	timestampactivity(0,t.screenprompt_s.Get());
	hud_scanforhudlayers ( );

	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  setup terrain for in-game
	t.screenprompt_s = "LOADING WATER SYSTEM";
	timestampactivity(0,t.screenprompt_s.Get());
	terrain_start_play ( );
	terrain_water_init ( );

	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  setup terrain for in-game
	t.screenprompt_s="LOADING A.I SYSTEM";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	darkai_init ( );
	t.aisystem.containerpathmax=0;
	t.screenprompt_s="PREPARING A.I SYSTEM";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());

	//  Reset waypoint for game activity
	if (t.showtestgameelements == 0)
	{
		t.screenprompt_s = "RESETTING WAYPOINTS A.I";
		timestampactivity(0, t.screenprompt_s.Get());
		waypoint_reset();
	}

	//  setup entities
	t.screenprompt_s="CREATING ENTITY A.I";
	timestampactivity(0,t.screenprompt_s.Get());
	entity_init ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	// Load weapon system
	t.screenprompt_s = "LOADING NEW WEAPONS";
	if (t.game.gameisexe == 0)  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0, t.screenprompt_s.Get());
	gun_activategunsfromentities ();
	gun_setup ();
	gun_loadonlypresent ();
	entity_init_nowcreateattachments();
	entity_init_overwritefireratesettings();

	//  create A.I entities for all characters
	t.screenprompt_s="SETTING UP CHARACTERS";
	timestampactivity(0,t.screenprompt_s.Get());
	darkai_setup_characters ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  When all static entities added, complete obstacle map
	if (  g.gskipobstaclecreation == 0 ) 
	{
		t.screenprompt_s="CREATING A.I OBSTACLES";
		timestampactivity(0,t.screenprompt_s.Get());
	}

	//  setup infinilights
	t.screenprompt_s="PREPARING DYNAMIC LIGHTS";
	timestampactivity(0,t.screenprompt_s.Get());
	lighting_init ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  particles/flak/debris required
	decal_activatedecalsfromentities ( );
	material_activatedecals ( );
	void weapon_projectile_activatedecals(void);
	weapon_projectile_activatedecals();
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	// load in decals (and new particle decals)
	t.screenprompt_s = "LOADING DECAL EFFECTS";
	timestampactivity(0, t.screenprompt_s.Get());
	decal_loadonlyactivedecals ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	// load required scripts
	void GunInitAnimationSettings(void);
	GunInitAnimationSettings();
	lua_init ( );
	lua_scanandloadactivescripts ( );

	// if still generating LIP file, wait here
	t.screenprompt_s="GENERATING LIP SYNC DATA";
	//if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5); looks wrong, most levels dont have any lips..
	timestampactivity(0,t.screenprompt_s.Get());
	float fProgressOfGeneration = GetWAVtoLIPProgress();
	while ( fProgressOfGeneration > 0.0f && fProgressOfGeneration < 1.0f )
	{
		fProgressOfGeneration = GetWAVtoLIPProgress();
		if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
		Sleep(50);
	}

	// load entity sounds and video
	entity_loadactivesoundsandvideo ( );
}

void game_preparelevel_forplayer ( void )
{
	//  Player settings
	physics_player_init ( );

	//  player start position must come from level setuo info
	t.terrain.gameplaycamera=0;
	SetCurrentCamera (  t.terrain.gameplaycamera );
	PositionCamera (  t.terrain.gameplaycamera,t.terrain.playerx_f,t.terrain.playery_f,t.terrain.playerz_f );
	PointCamera (  t.terrain.gameplaycamera,0,100,0 );
	RotateCamera (  t.terrain.gameplaycamera,t.terrain.playerax_f,t.terrain.playeray_f,t.terrain.playeraz_f );

	//  should be done in visual update call now!!
	SetCameraRange (  t.terrain.gameplaycamera, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE );
	SetCameraAspect (  t.terrain.gameplaycamera,1.325f );
	SetCameraFOV (  t.terrain.gameplaycamera,75 );
}

//Dave Performance - setup character entities for shader switching
void game_setup_character_shader_entities ( bool bMode )
{
}

extern int howManyMarkers;

void game_preparelevel_finally ( void )
{
	// Tell shadow maps to restore previous vis list rather than sort it
	g_bInEditor = false;
	t.performanceCameraDrawDistance = 0;
	t.haveSetupShaderSwitching = false;

	// each level's deaths take the death animations until a script sets SetRagdollAllDeaths
	extern bool g_bRagdollAllDeaths;
	g_bRagdollAllDeaths = false;
	// and starts at most 3 deaths a frame (SetRagdollCreatesPerFrame)
	extern int g_iRagdollCreatesPerFrame;
	g_iRagdollCreatesPerFrame = 3;
	// and no character knocked out (SetEntityRagdoll)
	extern void ragdoll_knockoutclear ( void );
	ragdoll_knockoutclear ( );

	g.isGameBeingPlayed = true;

	// This is used to record when we have switched lighting modes so we don't do it constantly
	g.inGameLightingMode = 0;
	if (  t.game.runasmultiplayer == 1 ) 
	{
		mp_load_guns ( );
	}

	//Free up spawns sent to lua
	t.entitiesActivatedForLua.clear();

	// Don't switch off guns!
	g.noPlayerGuns = false;
	g.remembergunid = 0;

	//  Generate mega texture of terrain paint for VERY LOW shaders
	if (  t.terrain.generatedsupertexture == 0 ) 
	{
		t.screenprompt_s="GENERATING TERRAIN SUPER TEXTURE";
		timestampactivity(0,t.screenprompt_s.Get());
		t.terrain.generatedsupertexture = 1;
	}

	//  Initiate post process system (or reactivate it)
	t.screenprompt_s="INITIALIZING POSTPROCESS";
	timestampactivity(0,t.screenprompt_s.Get());
	postprocess_init ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
	timestampactivity(0,"postprocessing initialized");

	//  Ensure correct shaders in play
	visuals_shaderlevels_update ( );

	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  Initialise Construction Kit
	//conkit_init ( );

	//  Init physics
	t.screenprompt_s="INITIALIZING PHYSICS";
	if (  t.game.gameisexe == 0  )  printscreenprompt(t.screenprompt_s.Get()); else loadingpageprogress(5);
	timestampactivity(0,t.screenprompt_s.Get());
	physics_init ( );

	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  Activate Occlusion System
	timestampactivity(0,"Activate Occlusion System");
	if (  g.globals.occlusionmode == 1 ) 
	{
		//  Once all assets in place, create occlusion database
		// 110416 - commented out again, turns out when occluder in THREAD is flagged to end, it clears the occluder list
		//CPU3DClear(); // 260316 - dont know why the clear was commented out, it is ESSENTIAL to ensure levels dont mess each other up
		CPU3DSetCameraIndex (  0 );
		//  Occlusion poly list can have a variable size to help performance
		CPU3DSetPolyCount ( t.visuals.occlusionvalue );
		//  Set occludees for all entities in level
		t.toccobj=g.occlusionboxobjectoffset;
		for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
		{
			if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
			t.entid=t.entityelement[t.e].bankindex;
			if (  t.entityprofile[t.entid].ismarker == 0 ) 
			{
				t.obj=t.entityelement[t.e].obj;
				if (  t.obj>0 ) 
				{
					if (  ObjectExist(t.obj) == 1 ) 
					{
						//Dave Performance, if the object is static, make it static for the game
						if ( t.entityelement[t.e].staticflag == 1 )
							SetObjectStatic(t.obj , true );
						else
							SetObjectStatic(t.obj , false );		

						//  reject any objects too small as they won't make good occluders
						if (  t.entityelement[t.e].staticflag == 1 && t.entityprofile[t.entid].notanoccluder == 0 && ((ObjectSizeX(t.obj,1)>MINOCCLUDERSIZE && ObjectSizeY(t.obj,1)>MINOCCLUDERSIZE ) || (ObjectSizeZ(t.obj,1)>MINOCCLUDERSIZE && ObjectSizeY(t.obj,1)>MINOCCLUDERSIZE )) ) 
						{
							//  OCLUDER AND OCLUDEE
							//  OPTIMIZE; we can make this even faster by using a simple math-QUAD in the render part
							//  instead of storing the verts of a 12 polygon box!!
							if ( t.entityelement[t.e].eleprof.isocluder == 1 )
							{
								if (  t.entityprofile[t.entid].physicsobjectcount>0 && t.entityprofile[t.entid].collisionmode == 40 ) 
								{
									t.tocy_f=ObjectSizeY(t.obj)/2.0;
									for ( t.tcount = 0 ; t.tcount<=  t.entityprofile[t.entid].physicsobjectcount-1; t.tcount++ )
									{
										if (  ObjectExist(t.toccobj) == 1  )  DeleteObject (  t.toccobj );
										MakeObjectBox (  t.toccobj, t.entityphysicsbox[t.entid][t.tcount].SizeX * (t.entityprofile[t.entid].scale * 0.01 ), t.entityphysicsbox[t.entid][t.tcount].SizeY * (t.entityprofile[t.entid].scale * 0.01 ), t.entityphysicsbox[t.entid][t.tcount].SizeZ * (t.entityprofile[t.entid].scale * 0.01 ) );
										OffsetLimb (  t.toccobj, 0, t.entityphysicsbox[t.entid][t.tcount].OffX * (t.entityprofile[t.entid].scale * 0.01 ) , t.entityphysicsbox[t.entid][t.tcount].OffY * (t.entityprofile[t.entid].scale * 0.01 ) , t.entityphysicsbox[t.entid][t.tcount].OffZ * (t.entityprofile[t.entid].scale * 0.01 ) );
										RotateLimb (  t.toccobj, 0, t.entityphysicsbox[t.entid][t.tcount].RotX , t.entityphysicsbox[t.entid][t.tcount].RotY , t.entityphysicsbox[t.entid][t.tcount].RotZ );
										MakeMeshFromObject (  g.meshgeneralwork,t.toccobj );
										DeleteObject (  t.toccobj );
										MakeObject (  t.toccobj,g.meshgeneralwork,0 );
										DeleteMesh (  g.meshgeneralwork );
										PositionObject (  t.toccobj,ObjectPositionX(t.obj),ObjectPositionY(t.obj)+t.tocy_f,ObjectPositionZ(t.obj) );
										RotateObject (  t.toccobj,ObjectAngleX(t.obj),ObjectAngleY(t.obj),ObjectAngleZ(t.obj) );
										SetObjectMask (  t.toccobj, 0 );
										SetObjectCollisionProperty (  t.toccobj,1 );
										CPU3DAddOccluder (  t.toccobj );
										++t.toccobj;
									}
								}
								else
								{
									//  polygon occluders TOO EXPENSIVE
									//  NOTE; Suggest a new set of polygons inside each model marked 'occluder'
									//  which when detected are submitted here via the OBJ
									// Dave Performance - adding everything in, even poly stuff
									// Don't add in collisionmode 50-59 (trees) as they make poor occluders
									if ( t.entityprofile[t.entid].collisionmode < 50 || t.entityprofile[t.entid].collisionmode > 59 ) 
										CPU3DAddOccluder (  t.obj );
								}
							}
						}
					}
				}

				// Add as an occludee
				if ( t.entityelement[t.e].eleprof.isocludee == 1 )
				{
					// Also let the occluder know if it is a character or not as characters are shown for longer
					// compared to other objects
					if ( t.entityprofile[t.entid].ischaracter == 1 ) 
					{
						// Also add character creator parts, if this is a cc character
						CPU3DAddOccludee ( t.obj , true );
					}
					else
					{
						CPU3DAddOccludee ( t.obj , false );
					}
				}
			}
			else
				howManyMarkers++;
		}
		while (  t.toccobj<g.occlusionboxobjectoffsetfinish ) 
		{
			if (  ObjectExist(t.toccobj) == 1  )  DeleteObject (  t.toccobj );
			++t.toccobj;
		}

		//  also occlude any weapons carried by characters
		if (  g.entityattachmentindex>0 ) 
		{
			for ( t.obj = g.entityattachmentsoffset+1 ; t.obj<=  g.entityattachmentsoffset+g.entityattachmentindex; t.obj++ )
			{
				if (  t.obj>0 ) 
				{
					if (  ObjectExist(t.obj) == 1 ) 
					{
						CPU3DAddOccludee (  t.obj , false );
					}
				}
			}
		}
	}

	//  Final states of entities and call ALL entity script INIT functions
	timestampactivity(0,"Entity Initiations");
	entity_initafterphysics ( );
	// each level starts with its own post effects and performance levers until a script sets them
	extern void LuaPostEffects_Clear(void* pVisualsToRestore);
	LuaPostEffects_Clear(NULL);
	extern void LuaRenderSettings_Clear(void);
	LuaRenderSettings_Clear();
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
	lua_launchallinitscripts ( );
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  Once player start known, fill veg area instantly
	timestampactivity(0,"Fill Veg Areas");
	t.completelyfillvegarea=1;
	if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );

	//  Force a shader update to ensure correct shadows are used at start
	t.visuals.refreshcountdown=5;

	// Let steam know we have finished loading
	if ( t.game.runasmultiplayer == 1 ) 
	{
		g.mp.finishedLoadingMap = 1;
	}

	// The start marker may have given the play an initial gun, so lets call physics_player_refreshcount just incase it has
	physics_player_refreshcount();
	// the player's hit record starts empty on every level
	playerhit_clear();
}

void game_stopallsounds ( void )
{
	// stop ALL sounds
	for ( t.s = 1; t.s <= 65535; t.s++ )
	{
		if ( SoundExist(t.s) == 1 ) 
		{
			if ( t.s >= g.musicsoundoffset && t.s <= g.musicsoundoffsetend ) 
			{
				// if from game menu, do not stop dynamic music sound progress
				StopSound (  t.s );
			}
			else
			{
				StopSound ( t.s );
			}
		}
	}
}

void game_freelevel ( void )
{
	// remove any bulletholes
	bulletholes_free();

	// free any HUD screen objects
	if (ObjectExist(g.hudscreen3dobjectoffset) == 1) DeleteObject (g.hudscreen3dobjectoffset);

	//Reset loading bar percentage
	extern int g_iLastProgressPercentage;
	g_iLastProgressPercentage = 0;
	g.isGameBeingPlayed = false;

	//  hide any jetpacks, etc
	hud_free ( );

	//  stop ALL sounds
	game_stopallsounds();

	// Delete occluder thread and close event handles (moved from free game which is too late for multi-level games)
	if ( g_pOccluderThread )
	{
		//Let the occluder know its time to finish
		g_occluderOn = false;

		//Dave Performance - let the occluder thread know it is okay to begin
		if ( g_hOccluderBegin ) SetEvent ( g_hOccluderBegin );

		//Dave Performance - wait for occluder to finish first
		if ( g_hOccluderEnd ) WaitForSingleObject ( g_hOccluderEnd, INFINITE );
	}
	
	// remove bits created by LUA scripts
	lua_freeprompt3d();
	lua_freeallperentity3d();

	//  close down game entities
	entity_free ( );

	//  Delete any infinilights
	lighting_free ( );

	//  AI finish
	darkai_free ( );

	//  free physics
	physics_free ( );

	// Clear the static physics data so we can get the dynamic data.
	BPhys_ClearDebugDrawData();

	//  deselect current gun and hide all gun objects
	gun_free ( );
	gun_freeguns ( );
	gun_removempgunsfromlist ( );

	//  remove water and sky effects
	if( t.game.gameisexe >= 1) //Keep sky for editor.
	sky_free ( );

	terrain_water_free ( );

	//  restore terrain from in-game
	terrain_stop_play ( );

	//  free Construction Kit
	//conkit_free ( );

	//  free any visual leftovers
	visuals_free ( );

	//  free character sound
	character_sound_free ( );

	//  close script system
	lua_free ( );

	//  free projectiles
	weapon_projectile_free ( );

	// finally delete entity element objs (only if standalone)
	if ( t.game.gameisexe == 1 )
	{
		//PE: Need to delete all particle emtters.
		gpup_deleteAllEffects();

		// only for standalone as test game needs entities for editor :)
		entity_delete();
		ClearAnyLightMapInternalTextures();

		//PE: Delete all entitybank textures used.
		if (g.standalonefreememorybetweenlevels == 1)
			ClearAnyEntitybankInternalTextures();
	}
}

void game_init ( void )
{
	//  Machine independent speed
	game_timeelapsed_init ( );

	//  Load slider menu resources
	sliders_init ( );

	//  Trigger all visuals to update
	t.visuals.refreshshaders=1;

	//  HideMouse (  and clear deltas )
	game_hidemouse ( );

	//  Last thing before main game loop
	physics_beginsimulation ( );

	//  Reset game checkpoint
	t.playercheckpoint.stored=1;
	t.playercheckpoint.x=CameraPositionX(0);
	t.playercheckpoint.y=CameraPositionY(0);
	t.playercheckpoint.z=CameraPositionZ(0);
	t.playercheckpoint.a=CameraAngleY(0);

	//  Reset hardware flags with each new level map
	t.hardwareinfoglobals.noterrain=0;
	t.hardwareinfoglobals.nowater=0;
	t.hardwareinfoglobals.noguns=0;
	t.hardwareinfoglobals.nolmos=0;
	t.hardwareinfoglobals.nosky=0;
	t.hardwareinfoglobals.nophysics=0;
	t.hardwareinfoglobals.noai=0;
	t.hardwareinfoglobals.nograss=0;
	t.hardwareinfoglobals.noentities=0;

	//  initialise panel resources
	panel_init ( );

	//  construction kit f9 mode cursor
	t.characterkitcontrol.oldF9CursorEntid = 0;
}

void game_freegame ( void )
{
	// Ensure we are switched back to full res
	t.bHiResMode = true;
	t.bOldHiResMode = true;
	SetCameraHiRes ( t.bHiResMode );

	//Free up spawns sent to lua
	t.entitiesActivatedForLua.clear();

	// Ensure Steam chat sprite has gone
	if (  SpriteExist(g.steamchatpanelsprite)  )  DeleteSprite (  g.steamchatpanelsprite );

	// Restore fonts
	loadallfonts();

	//Switch back to using an additional sort visibility list in the editor to avoid flickering when adding new entities
	g_bInEditor = true;
	//  Free file map access game uses
	physics_player_free ( );

	//  Free slider resources, not needed for title
	sliders_free ( );

	// Free LUA Sprites and Images
	FreeLUASpritesAndImages ();

	//  Free resources not specific to a single level before returning to title page
	postprocess_free ( );

	if (  t.game.runasmultiplayer == 1 ) 
	{
		mp_free_game ( );
	}

	panel_free ( );

	// free temp bitmap used to redirect 2D drawing to an image
	panel_Free2DDrawing();

	//Dave Performance - switch entities and profile object back to animating
	for ( int c = 0 ; c < (int)t.characterBasicEntityList.size() ; c++ )
	{		
		int tobj = t.entityelement[t.characterBasicEntityList[c]].obj;
		if ( tobj > 0 )
		{
			if ( ObjectExist ( tobj ) == 1 )
			{
				if ( !t.characterBasicEntityListIsSetToCharacter[c] )
				{
					SetObjectEffect( tobj , t.characterBasicShaderID );
					SetObjectEffect( g.entitybankoffset+t.entityelement[t.characterBasicEntityList[c]].bankindex , t.characterBasicShaderID );				
				}
			}
		}
	}

	t.characterBasicEntityList.clear();
	t.characterBasicEntityListIsSetToCharacter.clear();

	// switch all objects back to dynamic before heading back to the editor
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		if (  t.entityprofile[t.entid].ismarker == 0 ) 
		{
			t.obj=t.entityelement[t.e].obj;

			if (  t.obj>0 ) 
			{
				SetObjectStatic(t.obj , false );
			}
		}
	}

	//Dave Performance - restore any ignored objects and get the texture list sortid again to include them
	ClearIgnoredObjects ( );
	DoTextureListSort ( );
}


void game_hidemouse(void)
{
	if (g.mouseishidden == 0) {
		g.mouseishidden = 1;
		HideMouse();
		t.null = MouseMoveX() + MouseMoveY();
		return;
	}
}

void game_showmouse(void)
{
	if (g.mouseishidden == 1) {
		g.mouseishidden = 0;
		ShowMouse();
		//Tab Tab bug ?
		t.null = MouseMoveX() + MouseMoveY();
		return;
	}
}

void game_timeelapsed_init ( void )
{
	//  Machine indie speed
	t.TimerFrequency_f=PerformanceFrequency();
	t.StartFrameTime=PerformanceTimer();
	t.ElapsedTime_f=0.0;
	t.LastTimeStamp_f=timeGetSecond();
	g.timeelapsed_f=0;
}

void game_timeelapsed ( void )
{
	// Calculate time between cycles
	// room for the scripts' spawns: the array is copied out and back whenever it grows, so it grows by 1024 when the next
	// 256 slots are gone, not by one at every spawn (asking for the count plus 512 every frame did that: every clone
	// raised the count by one and the frame after it copied the whole array, 600 ms on the DX12 build)
	if ( g.entityelementlist + 256 > g.entityelementmax ) entity_growelementarray ( g.entityelementmax + 1024 );
	float fThisTimeCount = timeGetSecond();
	t.ElapsedTime_f = fThisTimeCount - t.LastTimeStamp_f;
	g.timeelapsed_f = t.ElapsedTime_f * 20.0;
	t.LastTimeStamp_f = fThisTimeCount;

	//  Cap to around 25fps so that leaps in movement/speed not to severe!
	if (  g.timeelapsed_f>0.75f  )  g.timeelapsed_f = 0.75f;
	if (  g.timeelapsed_f<0.00833f  )  g.timeelapsed_f = 0.00833f;

	void update_per_frame_effects(void);
	update_per_frame_effects();
}

void game_main_snapshotsoundloopcheckpoint ( bool bPauseAndResumeFromGameMenu )
{
	// remember any looping sounds but exclude weapon and rocket sounds
	if (bPauseAndResumeFromGameMenu==true || (t.playercontrol.disablemusicreset == 0 && bPauseAndResumeFromGameMenu == false) )
	{
		for ( t.s = g.soundbankoffset ; t.s<= g.soundbankoffsetfinish; t.s++ )
		{
			if (  t.soundloopcheckpoint[t.s] != 2 ) 
			{
				t.soundloopcheckpoint[t.s] = 0;
				if (  SoundExist(t.s) == 1 )
				{
					if (SoundPlaying(t.s) == 1)
					{
						t.soundloopcheckpoint[t.s] = 1;
						if (SoundLooping(t.s) == 1)
						{
							t.soundloopcheckpoint[t.s] = 3;
						}
					}
					if (bPauseAndResumeFromGameMenu == true)
					{
						PauseSound(t.s);
					}
					else
					{
						// record state only
					}
				}
			}
		}
	}
}

void game_main_snapshotsoundresumecheckpoint (bool bPauseAndResumeFromGameMenu)
{
	// knocked out characters are let go; the entities' reset puts their objects back
	ragdoll_knockoutclear ( );
	if (bPauseAndResumeFromGameMenu == true || (t.playercontrol.disablemusicreset == 0 && bPauseAndResumeFromGameMenu == false))
	{
		for ( t.s = g.soundbankoffset ; t.s <= g.soundbankoffsetfinish; t.s++ )
		{
			if ( t.soundloopcheckpoint[t.s] != 2 ) 
			{
				if (SoundExist(t.s) == 1)
				{
					if ( t.soundloopcheckpoint[t.s] != 0 )
					{
						if (bPauseAndResumeFromGameMenu == true)
						{
							// simply resume for game menu (and reset state flag)
							ResumeSound(t.s);
							t.soundloopcheckpoint[t.s] = 0;
						}
						else
						{
							// must recreate loop or play sounds (do not reset state flag as may restart several times)
							if (t.soundloopcheckpoint[t.s] == 3) LoopSound(t.s);
							if (t.soundloopcheckpoint[t.s] == 1) PlaySound(t.s);
						}
					}
					else
					{
						if (bPauseAndResumeFromGameMenu == true)
						{
							// game menu pause resume does not need to stop sounds
						}
						else
						{
							// stop any rogue sounds still chiming when restart at checkpoint
							StopSound(t.s);
						}
					}
				}
			}
		}
	}
}

void game_main_snapshotsound()
{
	// preserve any checkpoint state
	for (int i = g.soundbankoffset; i <= g.soundbankoffsetfinish; i++)
	{
		t.soundloopstore[i] = t.soundloopcheckpoint[i];
	}
	// grab current state into checkpoint and pause sounds
	bool bPauseAndResumeFromGameMenu = true;
	game_main_snapshotsoundloopcheckpoint(bPauseAndResumeFromGameMenu);
	// store these into game menu for when resume later
	for (int i = g.soundbankoffset; i <= g.soundbankoffsetfinish; i++)
	{
		t.soundloopgamemenu[i] = t.soundloopcheckpoint[i];
	}
}

void game_main_snapshotsoundresume()
{
	// copy game menu states into checkpoint
	for (int i = g.soundbankoffset; i <= g.soundbankoffsetfinish; i++)
	{
		t.soundloopcheckpoint[i] = t.soundloopgamemenu[i];
	}
	// resume all that was paused
	bool bPauseAndResumeFromGameMenu = true;
	game_main_snapshotsoundresumecheckpoint(bPauseAndResumeFromGameMenu);
	// restore storec checkpoint to resume game
	for (int i = g.soundbankoffset; i <= g.soundbankoffsetfinish; i++)
	{
		t.soundloopcheckpoint[i] = t.soundloopstore[i];
	}
}

