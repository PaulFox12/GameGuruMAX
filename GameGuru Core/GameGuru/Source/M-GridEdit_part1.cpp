// Reload hardening (2026-07-26): resume texture streaming after a level load completes.
// Definition: Guru-WickedMAX/wickedcalls_part2.cpp (pairs with WickedCall_ReloadQuiesceGPU
// called inside gridedit_load_map).
extern "C" void WickedCall_ReloadQuiesceEnd(void);

void mapeditorexecutable_loop(void)
{
	// the moment storyboard is used, we can load the rest of the common assets needed for editor and game
	if (bStoryboardWindow == true || pref.iDisplayWelcomeScreen == 0)
	{
		// Generic asset loading common to editor and game that can be deferred until user opens a project for first time
		if (g_bCommonAssetsLoadOnce == true)
		{
			t.tsplashstatusprogress_s = "LOAD DELAYED COMMON ASSETS";
			timestampactivity(0, t.tsplashstatusprogress_s.Get());
			version_splashtext_statusupdate ();
			::SetCursor(::LoadCursor(NULL, IDC_WAIT));
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
			bKeepWindowsResponding = true;

			common_loadcommonassets_delayed (0);

			bKeepWindowsResponding = false;
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
			::SetCursor(::LoadCursor(NULL, IDC_ARROW));

			g_bCommonAssetsLoadOnce = false;
		}
	}

	if (g_iCountdownToAlphaBetaMessage > 0)
	{
		g_iCountdownToAlphaBetaMessage--;
		if (g_iCountdownToAlphaBetaMessage == 0)
		{
			// great time to check for any new updates!
			common_autoupdatecheck();
		}
	}

	if (iUpdateOcean > 0)
	{
		iUpdateOcean--;
		if ( iUpdateOcean == 0 )
		{
			extern wiECS::Entity g_weatherEntityID;
			wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
			wiScene::GetScene().OceanRegenerate();
		}
	}

	static int iCheckForMinimized = 0;
	static bool bInMinimizeMode = false;
	//WS_MINIMIZE
	if (bInMinimizeMode)
	{
		Sleep(250); //Set Speed to 4 fps, so we have some reactions in other windows outside the main window.
		LONG lWinStyle = GetWindowLong(g_pGlob->hWnd, GWL_STYLE);
		if (!(lWinStyle & WS_MINIMIZE))
			bInMinimizeMode = false;
	}
	if (iCheckForMinimized++ >= 180) //PE: Evry 3 sec.
	{
		LONG lWinStyle = GetWindowLong(g_pGlob->hWnd,GWL_STYLE);
		if (lWinStyle & WS_MINIMIZE)
			bInMinimizeMode = true;
		iCheckForMinimized = 0;
	}

	//PE: Change icon set here where we have no imgui images on screen.
	SetIconSetCheck(false);

	bSmallVideoFrameStart = true;
	// special modes used when in test game or standalone game
	// GGMAX 2026-08-05 PERF: this call was the one gap in the "Logic - common_loop" breakdown.
	// The CL-* ranges start at CL-PreBlock BELOW this line and run to the end of the function,
	// so everything above was unattributed - which is why the CL-* rows summed to ~0.85 ms of a
	// 14.54 ms parent and the panel looked like it was lying. Instrument it.
	auto clGame = wi::profiler::BeginRangeCPU("CL-GameLoop");
	const bool bGameLoopTookOver = (commonexecutable_loop_for_game() == true);
	wi::profiler::EndRange(clGame);
	if (bGameLoopTookOver) return;

	// PERF P.3: pre-ImGui housekeeping range (weather particles, IO/style setup, launch state machine).
	auto clPre = wi::profiler::BeginRangeCPU("CL-PreBlock");

	//PE: Some function require we have a empty imgui , so launch here.
	extern bool g_bNoSwapchainPresent;

	bLastSmallVideoPlayerMaximized = bSmallVideoPlayerMaximized; //We need to status one frame behind.
	bSmallVideoPlayerMaximized = false;

	// can update collection list with flag
	if (g_bUpdateCollectionList == true)
	{
		//PE: Problem , open a level outside this project , select a weapon. and it get saved in the current project.
		//PE: https://youtu.be/LXyva4_786Y
		//PE: Must validate that this level belong to the active project.

		bool bPartOfStoreBoard = true; //PE: Running without a project.
		if (strlen(Storyboard.gamename) > 0)
		{
			bPartOfStoreBoard = false; //PE: Running using a project.
			char currentlevel[MAX_PATH];
			strcpy(currentlevel, g.projectfilename_s.Get());
			char* find = (char *) pestrcasestr(&currentlevel[0], "mapbank\\");
			if (find)
			{
				strcpy(currentlevel, find + 8);
			}
			for (int i = 0; i < STORYBOARD_MAXNODES; i++)
			{
				if (Storyboard.Nodes[i].used && Storyboard.Nodes[i].type == STORYBOARD_TYPE_LEVEL)
				{
					// get level name
					if (strlen(Storyboard.Nodes[i].level_name) > 0)
					{
						char levelname[MAX_PATH];
						strcpy(levelname, Storyboard.Nodes[i].level_name);
						char* find = (char*)pestrcasestr(&levelname[0], "mapbank\\");
						if (find)
						{
							strcpy(levelname, find + 8);
						}
						if (pestrcasestr(currentlevel, levelname))
						{
							bPartOfStoreBoard = true;
							break;
						}
					}
				}
			}
		}
		if (bPartOfStoreBoard)
		{
			bool bLoadingLevel = false;
			refresh_collection_from_entities(bLoadingLevel);
		}
		g_bUpdateCollectionList = false;
	}

	//PE: Moved load here. so no imgui objects on screen.
	//PE: Some imgui used ShaderResourceViews seams to change while loading a new level in wicked.
	if (iSkibFramesBeforeLaunch == 0) 
	{
		switch (iLaunchAfterSync)
		{
			case 699:
			{
				iLaunchAfterSync = 0;
				void AddRemoteProjectFonts(void);
				AddRemoteProjectFonts();
				break;
			}
			case 698:
			{
				//PE: Reload all fonts.
				iLaunchAfterSync = 0;
				void AddRemoteProjectFonts(void);
				ChangeGGFont("editors\\uiv3\\Roboto-Medium.ttf", 15);
				AddRemoteProjectFonts();
				break;
			}
			case 203: //PE: trigger a WM_SIZE so resolution,scissor,targetarea all match.
			{
				iLaunchAfterSync = 0;
				if (gWindowMaximized == 1) 
				{
					ShowWindow(g_pGlob->hWnd, SW_MAXIMIZE);
				}
				else 
				{
					SetWindowPos(g_pGlob->hWnd, HWND_TOP, gWindowPosXOld, gWindowPosYOld, gWindowSizeXOld + gWindowSizeAddX, gWindowSizeYOld + gWindowSizeAddY, SWP_SHOWWINDOW);
					ShowWindow(g_pGlob->hWnd, SW_SHOWNORMAL);
				}
				bUpdateVeg = true;
				bTriggerFovUpdate = true;
				break;
			}
			case 502: //Do the actual level load.
			{
					::SetCursor(::LoadCursor(NULL, IDC_WAIT));
				ImGuiIO& io = ImGui::GetIO();
				io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

				bKeepWindowsResponding = true;

				g.projectfilename_s = sNextLevelToLoad;

				extern bool g_bAllowBackwardCompatibleConversion;
				g_bAllowBackwardCompatibleConversion = true;
				gridedit_load_map();
				g_bAllowBackwardCompatibleConversion = false;
				WickedCall_ReloadQuiesceEnd(); // reload hardening: resume texture streaming

				g_EntityClipboard.clear(); //PE: Clear any old copy/paste.
				// GGMAX 3.44 (DX11 b807cfd7): the OTHER load path cleared undo and this one did not, so
				// Ctrl+Z after a storyboard/next-level load replayed edits from the PREVIOUS level.
				undosys_clearall(); //PE: Clear undo redo system.


				if(!bCloseStoryboardAfterLoad)
					iLevelEditorFromStoryboardID = -1; //If loaded from here, we cant update storyboard.

				t.terrain.grassregionx1 = t.terrain.grassregionx2;
				bUpdateVeg = true;

				iLaunchAfterSync = 80; //Update env
				iSkibFramesBeforeLaunch = 5;

				int firstempty = -1;
				int i = 0;
				for (; i < REMEMBERLASTFILES; i++) {
					if (firstempty == -1 && strlen(pref.last_open_files[i]) <= 0)
						firstempty = i;

					if (strlen(pref.last_open_files[i]) > 0 && pestrcasestr(g.projectfilename_s.Get(), pref.last_open_files[i])) { //already there
						break;
					}
				}
				if (i >= REMEMBERLASTFILES) {
					if (firstempty == -1) {
						//No empty slots , rotate.
						for (int ii = 0; ii < REMEMBERLASTFILES - 1; ii++) {
							strcpy(pref.last_open_files[ii], pref.last_open_files[ii + 1]);
						}
						strcpy(pref.last_open_files[REMEMBERLASTFILES - 1], g.projectfilename_s.Get());
					}
					else
						strcpy(pref.last_open_files[firstempty], g.projectfilename_s.Get());
				}

				//Locate player start marker.
				for (t.e = 1; t.e <= g.entityelementlist; t.e++)
				{
					if (t.entityelement[t.e].bankindex > 0)
					{
						if (t.entityprofile[t.entityelement[t.e].bankindex].ismarker == 1 && t.entityprofile[t.entityelement[t.e].bankindex].lives != -1)
						{
							//Point camera.
							t.obj = t.entityelement[t.e].obj;
							if (t.obj > 0) {
								float offsetx = ((float)GetDesktopWidth() - renderTargetAreaSize.x) * 0.25f;
								t.cx_f = ObjectPositionX(t.obj) + offsetx; //t.editorfreeflight.c.x_f;
								t.cy_f = ObjectPositionZ(t.obj); //t.editorfreeflight.c.z_f;
							}
							break;
						}
					}
				}

				iLastUpdateVeg = 0;
				bUpdateVeg = true;
				extern int g_iSuperTriggerFullGrassReveal; // hmm, shoved in to get the damn grass showing on initial load!
				g_iSuperTriggerFullGrassReveal = 10;

				//PE: Always start in object mode.
				bForceKey = true;
				csForceKey = "o";
				bTerrain_Tools_Window = false;
				Entity_Tools_Window = true;

				bKeepWindowsResponding = false;
				io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
				::SetCursor(::LoadCursor(NULL, IDC_ARROW));

				break;
			}
			case 2: //Open
				GGTerrain_CancelRamp();
				iLaunchAfterSync = 0;
				int iRet;
				iRet = AskSaveBeforeNewAction();
				if (iRet != 2)
				{
					//PE: filedialogs change dir so.
					cStr tOldDir = GetDir();

					// we know we need to focus on the mapbank associated with the current storyboard
					cstr correctFPMLocation_s = Storyboard.customprojectfolder;
					if (correctFPMLocation_s.Len() > 0)
					{
						correctFPMLocation_s += Storyboard.gamename;
						correctFPMLocation_s += "\\Files\\mapbank";
					}
					else
					{
						correctFPMLocation_s = g.mysystem.mapbankAbs_s.Get();
					}

					//cFileSelected = (char*)noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "fpm\0*.fpm\0", g.mysystem.mapbankAbs_s.Get(), NULL, true);
					char* cFileSelected = (char*)noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "fpm\0*.fpm\0", correctFPMLocation_s.Get(), NULL, true);
					SetDir(tOldDir.Get());
					if (cFileSelected && strlen(cFileSelected) > 0)
					{
						t.returnstring_s = cFileSelected;
						if (t.returnstring_s != "")
						{
							if (cstr(Lower(Right(t.returnstring_s.Get(), 4))) == ".fpm")
							{
								t.gridentity = 0;
								t.inputsys.constructselection = 0;
								editor_refresheditmarkers();
								g.projectfilename_s = t.returnstring_s;
							
								//Load actual level.
								sNextLevelToLoad = t.returnstring_s;
								iLaunchAfterSync = 502; 
								iSkibFramesBeforeLaunch = 3;
								strcpy(cTriggerMessage, "Loading Level ...");
								bTriggerMessage = true;
							}
						}
					}
				}
				// hmm, shoved in to get the damn grass showing on initial load!
				extern int g_iSuperTriggerFullGrassReveal; 
				g_iSuperTriggerFullGrassReveal = 10;
				iLastUpdateVeg = 0;
				break;

			case 7: // Direct Open
			{
				GGTerrain_CancelRamp();
				iLaunchAfterSync = 0;
				int iRet;
				iRet = AskSaveBeforeNewAction();
				if (iRet != 2)
				{
					if (strlen(cDirectOpen) > 0) {

						t.returnstring_s = cDirectOpen;
						if (t.returnstring_s != "")
						{
							if (cstr(Lower(Right(t.returnstring_s.Get(), 4))) == ".fpm")
							{
								t.gridentity = 0;
								t.inputsys.constructselection = 0;
								editor_refresheditmarkers();
								g.projectfilename_s = t.returnstring_s;

								//Load actual level.
								sNextLevelToLoad = t.returnstring_s;
								iLaunchAfterSync = 502; 
								iSkibFramesBeforeLaunch = 3;
								strcpy(cTriggerMessage, "Loading Level ...");
								bTriggerMessage = true;
							}
						}
					}
				}
				// hmm, shoved in to get the damn grass showing on initial load!
				extern int g_iSuperTriggerFullGrassReveal; 
				g_iSuperTriggerFullGrassReveal = 10;
				iLastUpdateVeg = 0;
				break;
			}
		}
	}

	switch(iLaunchAfterSync)
	{
		case 1:  // Test Game
		case 20: // VR Test Game

			if (t.game.gameisexe == 0)
			{
				//PE: Backup so we can restore last selected object after testgame.
				backup_pickedObject = t.widget.pickedObject;
				backup_gridentity = t.gridentity;
				backup_gridentityobj = t.gridentityobj;
			}
			else
			{
				backup_pickedObject = -1;
				backup_gridentity = -1;
				backup_gridentityobj = -1;
			}

			GGTerrain_CancelRamp();
			bInvulnerableMode = false;
			if (bStartInvulnerableMode)
			{
				bStartInvulnerableMode = false;
				bInvulnerableMode = true;
			}

			if (iLaunchAfterSync == 20 && g.gvrmode == 0)
			{
			}
			else
			{
				int iGridObj = g.ebeobjectbankoffset + 1000;
				if (ObjectExist(iGridObj))
					DeleteObject(iGridObj);
				extern uint32_t PreviewWPERoot;
				if (PreviewWPERoot != 0)
				{
					//PE: Delete effects.
					void DeleteEmitterEffects(uint32_t root);
					DeleteEmitterEffects(PreviewWPERoot);
					PreviewWPERoot = 0;
				}

				bImGuiInTestGame = true;
				bool bTestInVRMode = false;
				if (iLaunchAfterSync == 20) bTestInVRMode = true;
				extern int iLastResolutionWidth;
				extern int iLastResolutionHeight;
				back_iLastResolutionWidth = iLastResolutionWidth;
				back_iLastResolutionHeight = iLastResolutionHeight;
				back_renderTargetAreaPos = renderTargetAreaPos;
				back_renderTargetAreaSize = renderTargetAreaSize;
				RunCode(0); //switch to backbuffer 
				// g.projectmodified = 1; if just testing, do not assume a modification!
				// ensure threads loading resources are silent before test game
				image_preload_files_wait();
				object_preload_files_wait();
				image_preload_files_reset();
				object_preload_files_reset();
				if ( bTestInVRMode == false )
				{
					editor_previewmap_initcode(0);
					iLaunchAfterSync = 201;
				}
				else
				{
					editor_previewmap_initcode(1);
					iLaunchAfterSync = 201;
				}
				break;
			}
			break;

		case 30: //Create thumbnail.
		{
			if (iTooltipLastObjectId > 0) 
			{
				int drawobj = g.entitybankoffset + iTooltipLastObjectId;
				g_bNoSwapchainPresent = true; //dont present backbuffer to HWND.
			}
			iLaunchAfterSync = 31;
			break;
		}
		case 31: //Switch back to presenting the swapchain.
		{
			g_bNoSwapchainPresent = false;
			iLaunchAfterSync = 0;
			break;
		}

		case 21: //Social VR.
		{
			extern uint32_t PreviewWPERoot;
			if (PreviewWPERoot != 0)
			{
				//PE: Delete effects.
				void DeleteEmitterEffects(uint32_t root);
				DeleteEmitterEffects(PreviewWPERoot);
				PreviewWPERoot = 0;
			}
			bImGuiInTestGame = true;
			RunCode(0); //switch to backbuffer
			editor_multiplayermode();
			bBlockImGuiUntilNewFrame = true;
			bRenderNextFrame = false;
			SetCameraToImage(0, g.postprocessimageoffset, GetDisplayWidth(), GetDisplayHeight(), 2); //switch back to render target.
			bImGuiInTestGame = false;
			iLaunchAfterSync = 0;
			sky_show(); //Restore skybox.
			iLastUpdateVeg = 0; //Veg: update any changes from F9
			bUpdateVeg = true;
			break;
		}

		default:
			break;
	}

	//Display Weather.
	extern bool bEnableWeather;
	if (bEnableWeather) 
	{
		update_env_particles();
		ravey_particles_update();

		if (t.visuals.bPPSnow && t.visuals.bpp_disable_indoor)
		{
			//Disable weather indoor.
			float xPos = CameraPositionX();
			float yPos = CameraPositionY();
			float zPos = CameraPositionZ();
			static int iDelayedRayCast;
			if (iDelayedRayCast++ % 15 == 0)
			{
				extern wiECS::Entity g_weatherEntityID;
				wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
				int iHitObj = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, xPos, yPos, zPos, xPos, yPos + 2000.0f, zPos, 0, 0, 0, 0, 1, true);
				//weather->SetPPSnowEnabled(...); // removed in new WickedEngine API - no equivalent
				(void)iHitObj;
			}
		}

	}

	//PE: Imgui variables.
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	//PE: To solve problem with ALT-TAB away and key "release" are sent to another app.
	HWND tmpHwnd = GetFocus();
	if (bLostFocus == false && tmpHwnd != g_pGlob->hWnd) 
	{
		bLostFocus = true;
		io.KeySuper = false;
		io.KeyCtrl = false;
		io.KeyAlt = false;
		io.KeyShift = false;

		t.inputsys.keyreturn = 0;
		t.inputsys.keyshift = 0;
		t.inputsys.keytab = 0;
		t.inputsys.keyleft = 0;
		t.inputsys.keyright = 0;
		t.inputsys.keyup = 0;
		t.inputsys.keydown = 0;
		t.inputsys.keycontrol = 0;
		t.inputsys.keyspace = 0;
		t.inputsys.kscancode = 0;

		for (int iTemp = 0; iTemp < 273; iTemp++)
			io.KeysDown[iTemp] = 0;
	}
	if (tmpHwnd == g_pGlob->hWnd) 
	{
		if (bLostFocus) 
		{
			//We got focus again.
			//Looks like its fine to reset keys on lost only.
			//If there is a problem, they can also be reset here.
		}
		bLostFocus = false;
	}

	// set when showonstartup.ini does not exist and is created (first run sorts out UI panels)
	// and it seems, fixes the issue of 'bImGuiGotFocus' being true on some laptops on first run!?
	if (g.gfirsttimerun == 1)
	{
		g.gfirsttimerun = 0;
		refresh_gui_docking = 0;
		pref.vStartResolution = { 1280,800 };
		pref.iMaximized = 1;
	}

	//PE: ImGuiWindowFlags_NoNav added to prevent cursor keys to navigate imgui.
	if(pref.iAllowUndocking)
		iGenralWindowsFlags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoNav;
	else
		iGenralWindowsFlags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav;

	ImVec4 style_back = ImGui::GetStyle().Colors[ImGuiCol_Text];
	ImVec4 style_winback = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
	style_back = ImVec4(1.0, 1.0, 1.0, 1.0);

	bBoostIconColors = false;

	if (pref.current_style == 3) 
	{
		drawCol_back = ImColor(255, 255, 255, 128)*style_back;
		drawCol_normal = ImColor(255, 255, 255, 255);
		drawCol_hover = ImColor(180, 180, 180, 230);
	}
	else if (pref.current_style == 25) 
	{
		drawCol_back = ImColor(255, 255, 255, 128)*style_back;
		drawCol_normal = ImColor(255, 255, 255, 255);
		drawCol_hover = ImColor(180, 180, 180, 230);
		bBoostIconColors = true;
	}
	else 
	{
		drawCol_back = ImColor(255, 255, 255, 128)*style_back;
		drawCol_normal = ImColor(220, 220, 220, 230)*style_back;
		drawCol_hover = ImColor(255, 255, 255, 255)*style_back;
	}
	drawCol_Down = ImColor(255, 255, 255, 255)*style_back;
	ImVec4 drawCol_active = ImColor(120, 220, 120, 220)*style_back;
	ImVec4 drawCol_tmp = ImColor(220, 220, 220, 220)*style_back;
	ImVec4 drawCol_header = ImColor(255, 255, 255, 255)*style_back;

	bool toolbar_gradiant = false;
	#ifdef USETOOLBARGRADIENT
	toolbar_gradiant = true;
	#endif

	int adder = 6;
	ImVec4 drawCol_back_test = ImColor(255, 255, 255, adder)*style_back; adder += 6;
	ImVec4 drawCol_back_tools = ImColor(255, 255, 255, adder)*style_back; adder += 6;
	ImVec4 drawCol_back_waypoint = ImColor(255, 255, 255, adder)*style_back; adder += 6;
	ImVec4 drawCol_back_entities = ImColor(255, 255, 255, adder)*style_back; adder += 6;
	ImVec4 drawCol_back_terrain_tools = ImColor(255, 255, 255, adder)*style_back; adder += 6;
	ImVec4 drawCol_back_terrain = ImColor(255, 255, 255, adder)*style_back; adder += 6;
	ImVec4 drawCol_back_gg = ImColor(255, 255, 255, adder)*style_back;
	drawCol_toogle = drawCol_back_gg;
	if (pref.current_style == 25) {
		drawCol_back_test = ImColor(255, 255, 255,0);
		drawCol_back_tools = ImColor(255, 255, 255, 0);
		drawCol_back_waypoint = ImColor(255, 255, 255, 0);
		drawCol_back_entities = ImColor(255, 255, 255, 0);
		drawCol_back_terrain_tools = ImColor(255, 255, 255, 0);
		drawCol_back_terrain = ImColor(255, 255, 255, 0);
		drawCol_back_gg = ImColor(255, 255, 255, 0);
		drawCol_toogle = ImColor(255, 255, 255, 50);
	}

	ImVec4 drawCol_back_active = ImColor(255, 255, 255, 160); //*style_back;
	if (pref.current_style == 25) {
		drawCol_back_active = ImColor(128, 128, 128, 128); //*style_back;
	}


	static bool bLastImGuiGotFocus;
	bLastImGuiGotFocus = bImGuiGotFocus;

	bImGuiGotFocus = false; //PE: Set this if any of the imgui windows got focus.
	bImGuiReadyToRender = false;

	// Start the Dear ImGui frame
	if (!bImGuiFrameState && !bRenderTabTab) // lee added !bRenderTabTab to prevent double newframe
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		extern bool bSpriteWinVisible;
		bSpriteWinVisible = false;
		bImGuiFrameState = true;
		bTutorialRendered = false;
		bBlockImGuiUntilNewFrame = false;
		bImGuiRenderWithNoCustomTextures = false;
	}

	float fontSize = 0.0f;
	ImVec2 mCharAdvance = ImVec2(0, 0);
	int iOldRounding = 0;
	if (bImGuiFrameState) 
	{
		fontSize = ImGui::CalcTextSize("#").x;
		mCharAdvance = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing());
	}

	//PE: Additional resets, not done in direct input.
	t.inputsys.doartresize = 0;
	t.inputsys.dosave = 0; t.inputsys.doopen = 0; t.inputsys.donew = 0; t.inputsys.donewflat = 0; t.inputsys.dosaveas = 0;
	wi::profiler::EndRange(clPre); // close CL-PreBlock
	if (bImGuiFrameState)
	{
		// PERF P.3: sub-range instrumentation partitioning the ~5.9ms editor-UI "Logic - common_loop".
		// All boundaries are at this (2-tab) top level of the bImGuiFrameState block; there are no
		// returns in this file after line 71, so these sequential ranges are always balanced.
		auto clSeg = wi::profiler::BeginRangeCPU("CL-Head");
		// LEELEE disable interaction with main editor if Welcome system is active!
		if (iTriggerWelcomeSystemStuff != 0)
		{
			// LEELEE disable input to IMGUI menu/toolbar/panels while Welcome System is active
			//PE: Displaying it as Modal will disable everything but the welcome system.
			bRenderTargetModalMode = true;
		}
		else
		{
			bRenderTargetModalMode = false;
		}
		int icon_size = 50;
		ImVec2 iToolbarIconSize = { (float)icon_size, (float)icon_size };
		static bool dockingopen = true;
		float fsy = ImGui::CalcTextSize("#").y;
		toolbar_size = icon_size + (fsy*2.0) + 2;
		ImVec2 viewPortPos = ImGui::GetMainViewport()->Pos;
		ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
		ImGuiStatusBar_Size = fsy * 1.4;
		//PE: Render toolbar.

		iOldRounding = ImGui::GetStyle().WindowRounding;
		ImGui::GetStyle().WindowRounding = 0.0f;
		
		if (bStopBackbufferGrab > 0)
		{
			bStopBackbufferGrab--;
			if (bStopBackbufferGrab == 0)
			{
				if (BitmapExist(99))
				{
					DeleteBitmapEx(99);
				}
				bFullScreenBackbuffer = false;
			}
		}

		//#################
		//#### Toolbar ####
		//#################

		static float toolbar_offset_center = 0;
		int current_mode = 0;

		if (t.grideditselect == 6) 
		{
			extern bool bWaypointDrawmode;
			if(bWaypointDrawmode)
				current_mode = TOOL_DRAWWAYPOINTS;
			else
				current_mode = TOOL_WAYPOINTS;
		}
		else if (t.grideditselect == 5) 
		{
			if (t.gridentitymarkersmodeonly == 0)
				current_mode = TOOL_ENTITY;
			else
				current_mode = TOOL_MARKERS;
		}
		else 
		{
			if (t.terrain.terrainpaintermode >= 1 && t.terrain.terrainpaintermode <= 5)
			{
				if (t.terrain.terrainpaintermode == 1)  current_mode = TOOL_SHAPE;
				if (t.terrain.terrainpaintermode == 2)  current_mode = TOOL_LEVELMODE;
				if (t.terrain.terrainpaintermode == 3)  current_mode = TOOL_STOREDLEVEL;
				if (t.terrain.terrainpaintermode == 4)  current_mode = TOOL_BLENDMODE;
				if (t.terrain.terrainpaintermode == 5)  current_mode = TOOL_RAMPMODE;
			}
			else
			{
				if (t.terrain.terrainpaintermode == 6)  current_mode = TOOL_PAINTTEXTURE;
				if (t.terrain.terrainpaintermode == 7)  current_mode = TOOL_PAINTTEXTURE;
				if (t.terrain.terrainpaintermode == 8)  current_mode = TOOL_PAINTTEXTURE;
				if (t.terrain.terrainpaintermode == 9)  current_mode = TOOL_PAINTTEXTURE;
				if (t.terrain.terrainpaintermode == 10) current_mode = TOOL_PAINTGRASS;
			}
		}

		if(g_bCharacterCreatorPlusActivated)
			current_mode = TOOL_CCP;
		if(bBuilder_Properties_Window || t.ebe.on == 1)
			current_mode = TOOL_BUILDER;
		if (bImporter_Window && t.importer.importerActive == 1)
			current_mode = TOOL_IMPORT;

		if ( t.gridentity > 0 && t.entityprofile[t.gridentity].isebe != 0) 
		{
			current_mode = TOOL_BUILDER;
		}

		bool bOldWelcomeScreen_Window = bWelcomeScreen_Window;

		//PE: Make sure we dont place the toolbar in its own viewport.
		ImGui::SetNextWindowPos(ImVec2(0, 0) + viewPortPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, toolbar_size));

		if (pref.current_style == 25)
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.26f, 0.35f, 1.00f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.11f, 0.16f, 0.22f, 1.00f)); //org ImVec4(0.58f, 0.58f, 0.58f, 1.00f); // ImGui::PopStyleColor();
		}

		int toolbar_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		if (bOldWelcomeScreen_Window)
		{
			toolbar_flags |= ImGuiWindowFlags_NoChangeZOrder;
		}
		toolbar_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		ImGui::Begin("Toolbar", NULL , toolbar_flags);

		if (bOldWelcomeScreen_Window)
		{
			//Disable
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		}

		ImVec4 drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		float fDiv = 1.0f / 255.0f;
		ImVec4 drawCol_Divider_Selected = ImVec4(fDiv * 177.0f, fDiv * 206.0f, fDiv * 225.0f, 1.0f);
		if (pref.current_style == 25) 
		{
			drawCol_hover = ImVec4(fDiv * 142.0f, fDiv * 184.0f, fDiv * 212.0f, 1.0f);
		}

		ImGui::GetStyle().WindowRounding = iOldRounding;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

		float cursorpos = ImGui::GetCursorPos().x;

		//PE: New "Back to Game Project" ? 
		drawCol_tmp = drawCol_back_test; //LB: same background as toogle buttons for consistency
		//if (ImGui::ImgBtn(TOOL_GOBACK, iToolbarIconSize, drawCol_back_gg, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
		if (ImGui::ImgBtn(TOOL_GOBACK, iToolbarIconSize, drawCol_tmp, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
		{
			CloseAllOpenToolsThatNeedSave();
	
			if (g.projectmodified == 1 && g.projectfilename_s != "")
			{
				int iRet = askBoxCancel("Do you wish to save first?", "Confirmation"); //1==Yes 2=Cancel 0=No

				if (iRet == 1)
				{
					//Yes
					iLaunchAfterSync = 504; //Do the actualy save here.
					iSkibFramesBeforeLaunch = 3;
					strcpy(cTriggerMessage, "Saving Level ...");
					bTriggerMessage = true;
					iLaunchAfterSyncAction = 1; //Go to storyboard.
				}
				if (iRet == 0)
				{
					//PE: NO = Restore original fpm. on next load in storyboard.
					g.projectfilename_s = "";
					g.projectmodified = 0; gridedit_changemodifiedflag();
					g.projectmodifiedstatic = 0;
					GGTerrain_CancelRamp();
					bStoryboardWindow = true;
				}
			}
			else
			{
				int iRet = AskSaveBeforeNewAction();
				if (iRet != 2)
				{
					GGTerrain_CancelRamp();
					bStoryboardWindow = true;
				}
			}

			// Object library does not disappear if open when going to storyboard.
			if (bExternal_Entities_Window)
				bTriggerCloseEntityWindow = true;
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Back to Game Project Storyboard");
	
		ImGui::SameLine();
		drawCol_tmp = drawCol_back_test; //LB: same background as toogle buttons for consistency
		//if (ImGui::ImgBtn(TOOL_SAVELEVEL, iToolbarIconSize, drawCol_back_gg, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
		if (ImGui::ImgBtn(TOOL_SAVELEVEL, iToolbarIconSize, drawCol_tmp, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
		{
			CloseAllOpenToolsThatNeedSave();
			if (bTutorialCheckAction) TutorialNextAction();
			iLaunchAfterSync = 3; //Save
			iSkibFramesBeforeLaunch = 2;
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Save Level");
		ImGui::SameLine();

		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 2.0f, ImGui::GetCursorPos().y));
	
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImVec4 tool_selected_col = ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram];
		if(pref.current_style == 3 )
			tool_selected_col = ImGui::GetStyle().Colors[ImGuiCol_Button];

		ImVec2 tool_selected_padding = { 1.0, 1.0 };

		ImGui::SameLine();
		drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 2.0f, ImGui::GetCursorPos().y));
		float precise_icon_width = ImGui::GetCursorPos().x;

		//---------------------------------------------------------------------------------
		drawCol_tmp = drawCol_back_test; //LB: same background as toogle buttons for consistency
		//drawCol_tmp = drawCol_back_entities;
		//if (current_mode == TOOL_ENTITY || current_mode == TOOL_LOGIC) drawCol_tmp = drawCol_back_entities * drawCol_back_active; else drawCol_tmp = drawCol_back_entities;

		CheckTutorialAction("TOOL_TESTGAME", -10.0f); //Tutorial: check if we are waiting for this action
		if (ImGui::ImgBtn(TOOL_TESTGAME, iToolbarIconSize, drawCol_tmp, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down,0, 0, 0, 0, false, toolbar_gradiant,false,false,false, bBoostIconColors))
		{
			CloseAllOpenTools(false);
			if (bTutorialCheckAction) TutorialNextAction();
			iLaunchAfterSync = 1;
		}

		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Test Level");

		ImGui::SameLine();

		precise_icon_width = ImGui::GetCursorPos().x - precise_icon_width;
		
		// VR Mode
		if (g.gvrmode > 0 && g.gvrmodefordevelopers == 1)
		{
			if (ImGui::ImgBtn(TOOL_VRMODE, iToolbarIconSize, drawCol_tmp, drawCol_normal*drawCol_Selection, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
			{
				CloseAllOpenTools();
				if (bTutorialCheckAction) TutorialNextAction();
				iLaunchAfterSync = 20; //Test game VR.
			}
			if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0)
			{
				ImGui::SetTooltip("%s", "Test Level in VR (Developer Mode Feature)");
			}
			ImGui::SameLine();
		}


		toolbar_offset_center = ImGui::GetCursorPos().x - cursorpos;


		float rightx = ImGui::GetContentRegionMax().x;
		float right_border = 2.0f;

		//########################
		//#### Toggle Buttons ####
		//########################

		ImVec4 toggle_color;

		//------------------------------------------------------------------

		CheckTutorialAction("TOOL_SHAPE", -10.0f); //Tutorial: check if we are waiting for this action
		
		ImGui::SetCursorPos(ImVec2(rightx - right_border - (precise_icon_width * 7), ImGui::GetCursorPos().y));
		if (t.grideditselect == 0 && t.terrain.terrainpaintermode >= 1 && t.terrain.terrainpaintermode <= 12)
		{
			//PE: Keep selection in all sculpt modes.
			drawCol_tmp = drawCol_back_terrain * drawCol_back_active;
			if (pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;
			window->DrawList->AddRect((window->DC.CursorPos - tool_selected_padding), window->DC.CursorPos + tool_selected_padding + iToolbarIconSize, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
		}
		else
		{
			drawCol_tmp = drawCol_back_terrain;
		}
		drawCol_tmp = drawCol_back_test; //PE: Test. same background as toogle buttons.
		if (t.visuals.bEnableEmptyLevelMode == false)
		{
			if (ImGui::ImgBtn(TOOL_TERRAIN_TOOLBAR, iToolbarIconSize, drawCol_tmp, drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
			{
				CloseAllOpenTools();
				if (bTutorialCheckAction) TutorialNextAction();
				if (!pref.iEnableSingleRightPanelAdvanced)
				{
					Logic_Settings_Window = false;
					Game_Settings_Window = false;
					Weather_Tools_Window = false;
					Visuals_Tools_Window = false;
					//LB: Shooter now a filter mode Shooter_Tools_Window = false;
					iRestoreLastWindow = 0;
				}
				bForceKey = true;
				csForceKey = "t";
				bForceKey2 = true;
				csForceKey2 = "1";
			}
			if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Terrain, Painting, Trees and Vegetation (T)"); //"Terrain Tools"
		}
		ImGui::SameLine();

		//------------------------------------------------------------------

		ImGui::SetCursorPos(ImVec2(rightx - right_border - (precise_icon_width * 6), ImGui::GetCursorPos().y));

		drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		CheckTutorialAction("TOOL_ENTITY", -10.0f); //Tutorial: check if we are waiting for this action
		if (current_mode == TOOL_ENTITY || current_mode == TOOL_LOGIC) drawCol_tmp = drawCol_back_entities * drawCol_back_active; else drawCol_tmp = drawCol_back_entities;
		if ((current_mode == TOOL_ENTITY || current_mode == TOOL_LOGIC) && pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;
		if ((current_mode == TOOL_ENTITY || current_mode == TOOL_LOGIC) && pref.current_style >= 0) window->DrawList->AddRect((window->DC.CursorPos - tool_selected_padding), window->DC.CursorPos + tool_selected_padding + iToolbarIconSize, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
		drawCol_tmp = drawCol_back_test; //PE: Test. same background as toogle buttons.
		if (ImGui::ImgBtn(TOOL_ENTITY, iToolbarIconSize, drawCol_tmp, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors)) {

			CloseAllOpenTools();
			if (bTutorialCheckAction) TutorialNextAction();
			if (!pref.iEnableSingleRightPanelAdvanced)
			{
				Logic_Settings_Window = false;
				Game_Settings_Window = false;
				Weather_Tools_Window = false;
				Visuals_Tools_Window = false;
				//LB: Shooter now a filter mode Shooter_Tools_Window = false;
				iRestoreLastWindow = 0;
			}

			bForceKey = true;
			csForceKey = "o";
			Entity_Tools_Window = true;
			GGTerrain::GGTerrain_CancelRamp();
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Object Tools (O)"); //Entity Mode

		ImGui::SameLine();
		
		//------------------------------------------------------------------

		// Logic Toolbar - was Shooter Start
		ImGui::SetCursorPos(ImVec2(rightx - right_border - (precise_icon_width * 5), ImGui::GetCursorPos().y));

		toggle_color = drawCol_toogle;
		if (pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;
		if (!Shooter_Tools_Window) {
			drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			toggle_color = drawCol_back_test;
		}

		if (ImGui::ImgBtn(TOOL_LOGIC, iToolbarIconSize, toggle_color, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
		{
			if (iRestoreLastWindow == 0 && !pref.iEnableSingleRightPanelAdvanced)
			{
				if (bTerrain_Tools_Window)
					iRestoreLastWindow = 1;
				else
					iRestoreLastWindow = 2;
			}
			if (Shooter_Tools_Window)
			{
				Shooter_Tools_Window = false;
				if (iRestoreLastWindow >= 0 && !pref.iEnableSingleRightPanelAdvanced)
				{
					if (iRestoreLastWindow == 1)
					{
						bTerrain_Tools_Window = true;
						t.grideditselect = 0;
					}
					else
					{
						bForceKey = true;
						csForceKey = "o";
						Entity_Tools_Window = true;
					}
					iRestoreLastWindow = 0;
				}
			}
			else
			{
				if (bTerrain_Tools_Window)
				{
					//PE: Switch to object mode, so we dont make terrain changes when in logic.
					bTerrain_Tools_Window = false;
					t.grideditselect = 5;
				}

				if (!pref.iEnableSingleRightPanelAdvanced)
					CloseAllOpenTools();

				Shooter_Tools_Window = true;

				if (!pref.iEnableSingleRightPanelAdvanced)
				{
					Visuals_Tools_Window = false;
					Game_Settings_Window = false;
					if (Weather_Tools_Window)
						Weather_Tools_Window = false;
					Entity_Tools_Window = false;
				}
			}
			Logic_Settings_Window = Shooter_Tools_Window;
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Visual Logic Connections");
		ImGui::SameLine();
		//Shooter End


		ImGui::SetCursorPos(ImVec2(rightx - right_border - (precise_icon_width * 4), ImGui::GetCursorPos().y));
		toggle_color = drawCol_toogle;
		if (pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;
		if (!Visuals_Tools_Window) {
			drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			toggle_color = drawCol_back_test;
		}
		
		if (ImGui::ImgBtn(TOOL_VISUALS, iToolbarIconSize, toggle_color, drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors)) {
			//Display weather window.
			if (iRestoreLastWindow == 0 && !pref.iEnableSingleRightPanelAdvanced)
			{
				if (bTerrain_Tools_Window)
					iRestoreLastWindow = 1;
				else
					iRestoreLastWindow = 2;
			}
			if (!pref.iEnableSingleRightPanelAdvanced)
				CloseAllOpenTools();

			if (Visuals_Tools_Window) {

				Visuals_Tools_Window = false;
				if (iRestoreLastWindow >= 0 && !pref.iEnableSingleRightPanelAdvanced)
				{
					if (iRestoreLastWindow == 1)
					{
						bTerrain_Tools_Window = true;
						t.grideditselect = 0;
					}
					else
					{
						bForceKey = true;
						csForceKey = "o";
						Entity_Tools_Window = true;
					}
					iRestoreLastWindow = 0;
				}
			}
			else {
				Visuals_Tools_Window = true;
				if (!pref.iEnableSingleRightPanelAdvanced)
				{
					Logic_Settings_Window = false;
					Game_Settings_Window = false;
					if (Weather_Tools_Window)
						Weather_Tools_Window = false;
					//LB: Shooter now a filter mode if (Shooter_Tools_Window)
					//	Shooter_Tools_Window = false;
					Entity_Tools_Window = false;
				}
			}
		}

		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Environment Effects");
		ImGui::SameLine();

		ImGui::SetCursorPos(ImVec2(rightx - right_border -(precise_icon_width*3) , ImGui::GetCursorPos().y));
		toggle_color = drawCol_toogle;
		if (pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;


		//PE: New Game Settings icon.
		if (!Game_Settings_Window)
		{
			drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			toggle_color = drawCol_back_test;
		}

		if (ImGui::ImgBtn(TOOL_GAME_SETTINGS, iToolbarIconSize, toggle_color, drawCol_normal/**drawCol_Selection*/, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant, false, false, false, bBoostIconColors))
		{
			//Display game seetings window.
			if (iRestoreLastWindow == 0 && !pref.iEnableSingleRightPanelAdvanced)
			{
				if (bTerrain_Tools_Window)
					iRestoreLastWindow = 1;
				else
					iRestoreLastWindow = 2;
			}
			if (!pref.iEnableSingleRightPanelAdvanced)
				CloseAllOpenTools();
			if (Game_Settings_Window)
			{
				Game_Settings_Window = false;
				if (iRestoreLastWindow >= 0 && !pref.iEnableSingleRightPanelAdvanced)
				{
					if (iRestoreLastWindow == 1)
					{
						bTerrain_Tools_Window = true;
						t.grideditselect = 0;
					}
					else
					{
						bForceKey = true;
						csForceKey = "o";
						Entity_Tools_Window = true;
					}
					iRestoreLastWindow = 0;
				}
			}
			else
			{
				Game_Settings_Window = true;
				if (!pref.iEnableSingleRightPanelAdvanced)
				{
					if (Visuals_Tools_Window)
						Visuals_Tools_Window = false;
					Entity_Tools_Window = false;
				}
			}
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Game Settings");
		ImGui::SameLine();

		ImGui::SetCursorPos(ImVec2(rightx - right_border - (precise_icon_width * 2), ImGui::GetCursorPos().y));
		toggle_color = drawCol_toogle;
		if (pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;
		if (!bEditorLight)
		{
			drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			toggle_color = drawCol_back_test;
		}

		if (ImGui::ImgBtn(TOOL_CAMERALIGHT, iToolbarIconSize, toggle_color, drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant,false,false,false, bBoostIconColors)) 
		{
			//Toggle camera mode.
			if (bEditorLight) bEditorLight = false;
			else bEditorLight = true;
			WickedCall_EnableCameraLight(bEditorLight);
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Editor Light");
		ImGui::SameLine();
		ImGui::SetCursorPos(ImVec2(rightx - right_border - (precise_icon_width * 1), ImGui::GetCursorPos().y));

		bool bIsTopDownStatus = !(bool)t.editorfreeflight.mode;
		toggle_color = drawCol_toogle;
		if (pref.current_style == 25) drawCol_Selection = drawCol_Divider_Selected;
		drawCol_Selection = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		toggle_color = drawCol_back_test;

		if (ImGui::ImgBtn(TOOL_CAMERA, iToolbarIconSize, toggle_color, drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, toolbar_gradiant,false,false,false, bBoostIconColors))
		{
			//Toggle camera mode.
			bForceKey = true;
			if (bIsTopDownStatus)
				csForceKey = "f";
			else
			{
				csForceKey = "f";
			}
		}
		if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Camera View");

		ImGui::PopStyleVar();
		ImGui::PopStyleVar();


		if (bOldWelcomeScreen_Window)
		{
			ImGui::PopItemFlag(); //PE: Enable this tab.
		}

		if (pref.current_style == 25) {
			ImGui::PopStyleColor(2);
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.43f, 0.57f, 1.00f)); //org ImVec4(0.58f, 0.58f, 0.58f, 1.00f); // ImGui::PopStyleColor();
		}

		#define HIDE_MOVED_MENU_TO_STORYBOARD

		if (bOldWelcomeScreen_Window)
		{
			//Disable
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		}

		if (ImGui::BeginMenuBar() && bExternal_Entities_Window==false && bPreferences_Window==false) // cannot have MENU when viewing XX Library
		{
			if (ImGui::BeginMenu("File") )
			{
				//PE: YES new/load/save as is back :)

				// recommend an actual dialog to control how terrain/sky/veg/trees/stuff are generated
				// to start the user off with something more interesting than bumpy terrain!
				if (pref.iEnableLevelEditorOpenAndNew)
				{
					if (ImGui::MenuItem("New Level", "CTRL+N"))
					{
						int iRet = AskSaveBeforeNewAction();
						if (iRet == 0) //No save
						{
							g.projectmodified = 0; gridedit_changemodifiedflag();
							g.projectmodifiedstatic = 0;
						}
						if (iRet != 2)
						{
							bNoSecondAsk = true;
							CloseAllOpenTools();
						}
						if (iRet != 2)
						{
							//PE: Default to terrain tools , like when we launch Max.
							bForceKey = true;
							csForceKey = "t";
							bForceKey2 = true;
							csForceKey2 = "6";
							t.inputsys.domodeterrain = 1; t.inputsys.dowaypointview = 0;
							t.gridentitymarkersmodeonly = 0; t.grideditselect = 0;
							t.terrain.terrainpaintermode = 6;
							bTerrain_Tools_Window = true;
							// must reset any manual editing
							GGTerrain_ResetSculpting();
							void reset_terrain_paint_date(void);
							reset_terrain_paint_date();
							bProceduralLevelFromStoryboard = false;
							{ extern bool g_ggTerrainGenEntryPending; g_ggTerrainGenEntryPending = false; } // GGMAX 2.59
							iLaunchAfterSync = 5;
							iSkibFramesBeforeLaunch = 5;
						}
					}
				}

				if (pref.iEnableLevelEditorOpenAndNew)
				{
					if (ImGui::MenuItem("Open Level", "CTRL+O"))
					{
						CloseAllOpenToolsThatNeedSave();
						iLaunchAfterSync = 2;
						iSkibFramesBeforeLaunch = 5;
					}
				}

				if (ImGui::MenuItem("Save Level", "CTRL+B"))
				{
					//Save
					CloseAllOpenToolsThatNeedSave();
					iLaunchAfterSync = 3; 
					iSkibFramesBeforeLaunch = 5;
				}

				if (ImGui::MenuItem("Save Level As...", ""))//CTRL+R" ) )//F12") )
				{
					//Save As
					CloseAllOpenToolsThatNeedSave();
					iLaunchAfterSync = 4; 
					iSkibFramesBeforeLaunch = 5;
				}

				ImGui::Separator();

				if (pref.iEnableLevelEditorOpenAndNew)
				{
					//for (int ii = 0; ii < REMEMBERLASTFILES; ii++) { //reverse
					for (int ii = REMEMBERLASTFILES - 1; ii >= 0; ii--) 
					{
						if (strlen(pref.last_open_files[ii]) > 0) 
						{
							char tmp[260];
							strcpy(tmp, pref.last_open_files[ii]);
							int pos = strlen(tmp);
							while (pos > 0 && tmp[pos] != '\\') pos--;

							//std::string s_tmp = std::to_string(1+ii); //Reverse
							std::string s_tmp = std::to_string(REMEMBERLASTFILES - ii);
							s_tmp += ": ";
							s_tmp += &tmp[pos + 1];

							if (ImGui::MenuItem(s_tmp.c_str())) 
							{
								if (bWaypointDrawmode || bWaypoint_Window) { bWaypointDrawmode = false; bWaypoint_Window = false; }
								if (bImporter_Window) { importer_quit(); bImporter_Window = false; }
								if (g_bCharacterCreatorPlusActivated) g_bCharacterCreatorPlusActivated = false;
								if (bEntity_Properties_Window) bEntity_Properties_Window = false;
								if (t.ebe.on == 1) ebe_hide();
								strcpy(cDirectOpen, pref.last_open_files[ii]);
								iLaunchAfterSync = 7; //Direct open.
								iSkibFramesBeforeLaunch = 5;
							}
						}
					}
					ImGui::Separator();
				}


				if (ImGui::MenuItem("Back to Storyboard Editor"))
				{
					CloseAllOpenToolsThatNeedSave();
					//Storyboard can change levels ... so make sure we ask to save first.
					// UI AUDIT 2026-07-28: an unconditional bStoryboardWindow=true after this
					// if-block made Cancel in the save prompt not cancel — removed.
					int iRet = AskSaveBeforeNewAction();
					if (iRet != 2)
					{
						bStoryboardWindow = true;
						GGTerrain_CancelRamp();
					}
				}

				ImGui::EndMenu();
			}
			else
			{
				if(pref.bAutoOpenMenuItems)
					if (ImGui::IsItemHovered()) 
						ImGui::OpenPopup("File");
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo","CTRL+Z")) 
				{
					//t.inputsys.doundo = 1;
					bForceUndo = true;
				}
				if (ImGui::MenuItem("Redo", "CTRL+Y")) 
				{
					t.inputsys.doredo = 1;
					bForceRedo = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Cut", "CTRL+X")) {
					iExecuteCTRLkey = 'X';
				}
				if (ImGui::MenuItem("Copy", "CTRL+C")) {
					iExecuteCTRLkey = ImGuiKey_C;
				}
				if (ImGui::MenuItem("Paste", "CTRL+V")) {
					iExecuteCTRLkey = ImGuiKey_V;
				}
				if (ImGui::MenuItem("Import", "CTRL+I")) {
					iExecuteCTRLkey = 'I';
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete", "DEL")) {
					iExecuteCTRLkey = ImGuiKey_Delete;
				}



				ImGui::Separator();




				if (ImGui::MenuItem("Settings", "")) 
				{
					strcpy(cPreferencesMessage,"");
					bPreferences_Window = true;
				}

				ImGui::EndMenu();
			}
			else
			{
				if (pref.bAutoOpenMenuItems)
					if (ImGui::IsItemHovered())
						ImGui::OpenPopup("Edit");
			}

			if (ImGui::BeginMenu("Tools"))
			{
				if (ImGui::MenuItem("Quest Editor"))
				{
					CloseAllOpenTools();
					extern bool bQuestEditor_Window;
					bQuestEditor_Window = true;
				}

				if (ImGui::MenuItem("Character Creator"))
				{
					CloseAllOpenTools();
					iLaunchAfterSync = 82; //Start Character Creator
					iSkibFramesBeforeLaunch = 2;
					strcpy(cTriggerMessage, "Loading Character Creator");
					bTriggerMessage = true;
				}

				// Tooling
				if (g_bParticleEditorPresent == true)
				{
					ImGui::Separator();
					if (ImGui::MenuItem("Particle Editor"))
					{
						extern void launchOrShowParticleEditor(void);
						launchOrShowParticleEditor();
					}
				}
				if (g_bBuildingEditorPresent == true)
				{
					if(!g_bParticleEditorPresent == true)
						ImGui::Separator();

					if (ImGui::MenuItem("Building Editor"))
					{
						extern void launchOrShowBuildingEditor(void);
						launchOrShowBuildingEditor();
					}
				}

				ImGui::EndMenu();
			}
			else
			{
				if (pref.bAutoOpenMenuItems)
					if (ImGui::IsItemHovered())
						ImGui::OpenPopup("Tools");
			}


#ifndef GGMAXEDU
			static bool bMarketHovered = false;
			window = ImGui::GetCurrentWindow();
			ImRect text_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(100, ImGui::GetFontSize()) );
			bool bEnabled = true;
			if (ImGui::MenuItem2("Marketplace",nullptr, bMarketHovered, bEnabled))
			{
				CloseAllOpenTools();
				DeleteWaypointsAddedToCurrentCursor();
				CloseDownEditorProperties();
				bMarketplace_Window = true;
			}
			ImVec2 startend = ImGui::GetCursorPos();
			text_bb.Max.x = window->DC.CursorPos.x;
			if (ImGui::IsMouseHoveringRect(text_bb.Min, text_bb.Max))
			{
				bMarketHovered = true;
			}
			else
				bMarketHovered = false;

#endif



			if (ImGui::BeginMenu("Help"))
			{

				image_setlegacyimageloading(true);


				if (ImGui::MenuItem("Test Level Controls")) {

					strcpy(cHelpMenuImage, "languagebank\\english\\artwork\\testgamelayout.png");
					LoadImage(cHelpMenuImage, HELPMENU_IMAGE);
					bHelp_Menu_Image_Window = true;
				}

				// no VR for now
				if (ImGui::MenuItem("Read User Manual")) 
				{
					//ExecuteFile("https://gameguru-max.document360.io/docs", "", "", 0);
					// User guide has been moved to offline only
					ExecuteFile("..\\Guides\\User Manual\\GameGuru MAX - User Guide.pdf", "", "", 0);
				}
				if (ImGui::MenuItem("Guides Folder"))
				{
					ExecuteFile("..\\Guides\\", "", "", 0);
				}
				char pAbsEXE[MAX_PATH];
				sprintf(pAbsEXE, "%s\\Guides\\MaxLua\\MaxLua.exe", g.fpscrootdir_s.Get());
				if (FileExist(pAbsEXE))
				{
					if (ImGui::MenuItem("Scripting Command List"))
					{
						char pAbsPATH[MAX_PATH];
						sprintf(pAbsPATH, "%s\\Guides\\MaxLua\\", g.fpscrootdir_s.Get());
						ExecuteFile(pAbsEXE, "", pAbsPATH, 0);
					}
				}
				if (g_bParticleEditorPresent == true)
				{
					if (ImGui::MenuItem("Particle Editor User Guide"))
					{
						char pOldDir[MAX_PATH];
						strcpy(pOldDir, GetDir());
						SetDir("..");
						SetDir("Tools\\Particle Editor\\media\\docs\\");
						ExecuteFile("Particle Editor User Guide.pdf", "", "", 0);
						SetDir(pOldDir);
					}
				}
				if (g_bBuildingEditorPresent == true)
				{
					if (ImGui::MenuItem("Building Editor Getting Started"))
					{
						ExecuteFile("https://www.youtube.com/watch?v=5o865epIhqY", "", "", 0);
					}
				}
				#ifndef DISABLETUTORIALS
				if (ImGui::MenuItem("Getting Started Tutorial"))
				{
					bHelpVideo_Window = true;
					bHelp_Window = true;
					bSetTutorialSectionLeft = false;
					strcpy(cForceTutorialName, "01 - Getting started");
				}
				#endif
				if (ImGui::MenuItem("Report an Issue (GitHub)"))
				{
					ExecuteFile("https://github.com/TheGameCreators/GameGuruRepo/issues/new", "", "", 0);

				}
				if (g_bFreeTrialVersion == true)
				{
					if (ImGui::MenuItem("Buy GameGuru MAX"))
					{
						CloseAllOpenTools();
						DeleteWaypointsAddedToCurrentCursor();
						CloseDownEditorProperties();
						bFreeTrial_Window = true;
					}
				}

				if (ImGui::MenuItem("About")) {
					bAbout_Window = true;
					bAbout_Window_First_Run = true;
				}
				image_setlegacyimageloading(false);

				ImGui::EndMenu();
			}
			else
			{
				if (pref.bAutoOpenMenuItems)
					if (ImGui::IsItemHovered()) 
						ImGui::OpenPopup("Help");
			}
			ImGui::EndMenuBar();
		}

		if (bOldWelcomeScreen_Window)
		{
			ImGui::PopItemFlag(); //PE: Enable this tab.
		}

		if (pref.current_style == 25)
			ImGui::PopStyleColor(); 

		//Process systemwide shortcut keys.

		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::GetTime() - lastKeyTime >= 0.125) 
		{ 
			//small delay between key input.
			auto ctrl = io.KeyCtrl;
			auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
			auto shift = io.KeyShift;

			//PE: No repeat on these keys.
			static bool bWaitOnGRelease = false;
			if (bWaitOnGRelease && ImGui::IsKeyReleased(71))
				bWaitOnGRelease = false;

			//LB: ensure CTRL+Z can release Z and repress so all does not happen at once
			static bool bWaitOnZRelease = false;
			if (bWaitOnZRelease && !ImGui::IsKeyDown(90))
			{
				t.inputsys.undokeypress = 0;
				bWaitOnZRelease = false;
			}

			if (ctrl && !shift && !alt && ImGui::IsKeyPressed(89) || iExecuteCTRLkey == 'Y' ) //Y
			{ 
				//CTRL Y - redo
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				bForceRedo = true;
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(78) || iExecuteCTRLkey == 'N') //N
			{
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				if (bStoryboardWindow && iExecuteCTRLkey != 'N' && !bProceduralLevel) iStoryboardExecuteKey = 'N';
				if (!bStoryboardWindow)
				{
					if (pref.iEnableLevelEditorOpenAndNew)
					{
						CloseAllOpenTools();
						iLaunchAfterSync = 5;
						bProceduralLevelFromStoryboard = false;
							{ extern bool g_ggTerrainGenEntryPending; g_ggTerrainGenEntryPending = false; } // GGMAX 2.59
						iSkibFramesBeforeLaunch = 5;
					}
				}
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(76)) //CTRL+L, Add level in storyboard, lock/unlock selected object(s) in editor
			{
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				if (bStoryboardWindow && !bProceduralLevel)
					iStoryboardExecuteKey = 'L';
				else
				{
					bool bLock = true;
					// Determine if the selected object should be locked/unlocked.
					int iObjectLockedIndex = -1;
					if (vEntityLockedList.size() > 0)
					{
						for (int i = 0; i < vEntityLockedList.size(); i++)
						{
							int e = vEntityLockedList[i].e;
							if (e < 0 || e >= t.entityelement.size()) continue;

							if (e == t.widget.pickedEntityIndex)
							{
								iObjectLockedIndex = i;
								bLock = false;
								break;
							}
						}
					}

					LockSelectedObject(bLock, iObjectLockedIndex);
				}
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(69)) //E
			{
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				if (bStoryboardWindow && !bProceduralLevel) iStoryboardExecuteKey = 'E';
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(32)) //SPACE
			{
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				if (bStoryboardWindow && !bProceduralLevel) iStoryboardExecuteKey = ' ';
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(73) || iExecuteCTRLkey == 'I') //I - Importer
			{
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				DeleteWaypointsAddedToCurrentCursor();
				CloseDownEditorProperties();
				CloseAllOpenTools();
				iLaunchAfterSync = 8; //Import model
				iSkibFramesBeforeLaunch = 5;
				bMarketplace_Window = false;
				if (bExternal_Entities_Window)
				{
					bTriggerCloseEntityWindow = true;
					bCheckForClosingForce = true; //Force window to close.
				}
				bEnableWeather = false;
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(79) || iExecuteCTRLkey == 'O' ) //O
			{
				if (pref.iEnableLevelEditorOpenAndNew)
				{
					lastKeyTime = (float)ImGui::GetTime();
					iExecuteCTRLkey = 0;
					CloseAllOpenTools();
					iLaunchAfterSync = 2;
					iSkibFramesBeforeLaunch = 5;
				}
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(66) || iExecuteCTRLkey == 'B') //B
			{
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				CloseAllOpenToolsThatNeedSave();
				iLaunchAfterSync = 3; //Save
				iSkibFramesBeforeLaunch = 5;
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(88) || iExecuteCTRLkey == 'X') //X , "CUT"
			{
				if (!bLastImGuiGotFocus || iExecuteCTRLkey == 'X')
				{
					lastKeyTime = (float)ImGui::GetTime();
					iExecuteCTRLkey = 0;
					t.widget.deletebuttonselected = 1;
					widget_show_widget();
				}
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(90) && bWaitOnZRelease==false) 
			{ 
				//CTRL Z - undo
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
				bForceUndo = true;
				bWaitOnZRelease = true;
				t.inputsys.undokeypress = 1;
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(71)) //CTRL+G
			{
				//CTRL G - Group.
				if (!bWaitOnGRelease)
					CreateNewGroup(-1);
				bWaitOnGRelease = true;
			}
			else if (ctrl && shift && !alt && ImGui::IsKeyPressed(71)) //CTRL+SHIFT+G
			{
				//CTRL+SHIFT G - UnGroup.
				if(!bWaitOnGRelease)
					UnGroupSelected();
				bWaitOnGRelease = true;
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C)) || iExecuteCTRLkey == ImGuiKey_C)
			{
				//PE: Mouse need to be in Level Editor for copy paste objects to work.
				if (!bLastImGuiGotFocus || iExecuteCTRLkey == ImGuiKey_C)
				{
					lastKeyTime = (float)ImGui::GetTime();
					iExecuteCTRLkey = 0;

					//CTRL C - copy rubber band or single to clipboard
					if (t.widget.pickedEntityIndex > 0)
					{
						g_EntityClipboard.clear();
						if (g.entityrubberbandlist.size() > 0)
						{
							for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
							{
								int e = g.entityrubberbandlist[i].e;
								int entid = t.entityelement[e].bankindex;
								if (entid > 0 && t.entityprofile[entid].ismarker != 1)
								{
									g_EntityClipboard.push_back(e);
								}
							}
						}
						else
						{
							int entid = t.entityelement[t.widget.pickedEntityIndex].bankindex;
							if (entid > 0 && t.entityprofile[entid].ismarker != 1 )
							{
								g_EntityClipboard.push_back(t.widget.pickedEntityIndex);
							}
						}
						g_EntityClipboardAnchorEntityIndex = t.widget.pickedEntityIndex;
					}
				}
			}
			else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V)) || iExecuteCTRLkey == ImGuiKey_V )
			{
				//PE: Mouse need to be in Level Editor for copy paste objects to work.
				if (!bLastImGuiGotFocus || iExecuteCTRLkey == ImGuiKey_V)
				{

					lastKeyTime = (float)ImGui::GetTime();
					iExecuteCTRLkey = 0;

					//CTRL V - duplicate from clipboard
					if (g_EntityClipboard.size() > 0)
					{
						// batch the paste into a single event
						undosys_multiplevents_start();

						// a small offset so user can see new pasted entity
						float fShiftOffsetForPasteX = 25.0f + rand() % 50;
						float fShiftOffsetForPasteZ = 25.0f + rand() % 50;
						if (rand() % 2 == 0) fShiftOffsetForPasteX = -fShiftOffsetForPasteX;
						if (rand() % 2 == 0) fShiftOffsetForPasteZ = -fShiftOffsetForPasteZ;

						// determine anchor entity from original entity group
						int iAnchorEntityIndex = -1;

						// we are also going to move rubber band selection to new pasted group
						g.entityrubberbandlist.clear();

						// for each entity, create a duplicate and offset slightly so we can see it
						for (int i = 0; i < (int)g_EntityClipboard.size(); i++)
						{
							// duplicate new entity as clone of relevant original clipboard entity
							int e = g_EntityClipboard[i];
							//PE: Crash from paste from another level.
							if (e < t.entityelement.size())
							{
								t.gridentity = t.entityelement[e].bankindex;
								//PE: all t.gridentity... need to be set for this to work correctly.
								t.entid = t.gridentity;
								entity_fillgrideleproffromprofile();  // t.entid
								t.gridentityposx_f = t.entityelement[e].x;
								t.gridentityposy_f = t.entityelement[e].y;
								t.gridentityposz_f = t.entityelement[e].z;
								t.gridentityrotatex_f = t.entityelement[e].rx;
								t.gridentityrotatey_f = t.entityelement[e].ry;
								t.gridentityrotatez_f = t.entityelement[e].rz;
								t.gridentityrotatequatmode = t.entityelement[e].quatmode;
								t.gridentityrotatequatx_f = t.entityelement[e].quatx;
								t.gridentityrotatequaty_f = t.entityelement[e].quaty;
								t.gridentityrotatequatz_f = t.entityelement[e].quatz;
								t.gridentityrotatequatw_f = t.entityelement[e].quatw;
								if (t.entityprofile[t.gridentity].ismarker == 10)
								{
									t.gridentityscalex_f = 100.0f + t.entityelement[e].scalex;
									t.gridentityscaley_f = 100.0f + t.entityelement[e].scaley;
									t.gridentityscalez_f = 100.0f + t.entityelement[e].scalez;
								}
								else
								{
									t.gridentityscalex_f = ObjectScaleX(t.entityelement[e].obj);
									t.gridentityscaley_f = ObjectScaleY(t.entityelement[e].obj);
									t.gridentityscalez_f = ObjectScaleZ(t.entityelement[e].obj);
								}
								t.grideleprof = t.entityelement[e].eleprof;
								entity_cleargrideleprofrelationshipdata();
								t.grideleprof.newparticle.emitterid = -1; //PE: Must always get a new emitter ID.

								//PE: InstanceObject - Cursor,Object Tools - objects must always be real clones.
								extern bool bNextObjectMustBeClone;
								bNextObjectMustBeClone = true;

								gridedit_addentitytomap();

								bNextObjectMustBeClone = false;

								if (e == g_EntityClipboardAnchorEntityIndex) iAnchorEntityIndex = t.e;
								t.entityelement[t.e].x = t.entityelement[e].x + fShiftOffsetForPasteX;
								t.entityelement[t.e].y = t.entityelement[e].y;
								t.entityelement[t.e].z = t.entityelement[e].z + fShiftOffsetForPasteZ;
								t.entityelement[t.e].rx = t.entityelement[e].rx;
								t.entityelement[t.e].ry = t.entityelement[e].ry;
								t.entityelement[t.e].rz = t.entityelement[e].rz;
								t.entityelement[t.e].quatmode = t.entityelement[e].quatmode;
								t.entityelement[t.e].quatx = t.entityelement[e].quatx;
								t.entityelement[t.e].quaty = t.entityelement[e].quaty;
								t.entityelement[t.e].quatz = t.entityelement[e].quatz;
								t.entityelement[t.e].quatw = t.entityelement[e].quatw;
								t.entityelement[t.e].editorfixed = t.entityelement[e].editorfixed;
								t.entityelement[t.e].staticflag = t.entityelement[e].staticflag;
								t.entityelement[t.e].scalex = t.entityelement[e].scalex;
								t.entityelement[t.e].scaley = t.entityelement[e].scaley;
								t.entityelement[t.e].scalez = t.entityelement[e].scalez;
								t.entityelement[t.e].soundset = t.entityelement[e].soundset;
								t.entityelement[t.e].soundset1 = t.entityelement[e].soundset1;
								t.entityelement[t.e].soundset2 = t.entityelement[e].soundset2;
								t.entityelement[t.e].soundset3 = t.entityelement[e].soundset3;
								t.entityelement[t.e].soundset4 = t.entityelement[e].soundset4;
								t.entityelement[t.e].soundset5 = t.entityelement[e].soundset5;
								t.entityelement[t.e].soundset6 = t.entityelement[e].soundset6;
								//PE: We have a new particle id here, so cant just copy.
								newparticletype backup_newparticle = t.entityelement[t.e].eleprof.newparticle;
								t.entityelement[t.e].eleprof = t.entityelement[e].eleprof;
								t.entityelement[t.e].eleprof.newparticle = backup_newparticle;
								PositionObject(t.entityelement[t.e].obj, t.entityelement[t.e].x, t.entityelement[t.e].y, t.entityelement[t.e].z);
								RotateObject(t.entityelement[t.e].obj, t.entityelement[t.e].rx, t.entityelement[t.e].ry, t.entityelement[t.e].rz);

								// Can't copy object relations so ensure previous are cleared
								t.entityelement[t.e].eleprof.iObjectLinkID = 0;
								//PE: This caused a crash iObjectRelationshipsData[j] > 10 (should have been j) made memory overwrite inside eleprof
								for (int j = 0; j < 10; j++)
								{
									t.entityelement[t.e].eleprof.iObjectRelationships[j] = 0;
									t.entityelement[t.e].eleprof.iObjectRelationshipsType[j] = 0;
									t.entityelement[t.e].eleprof.iObjectRelationshipsData[j] = 0;
								}

								// and add to new rubber band group
								sRubberBandType rubberbandItem;
								rubberbandItem.e = t.e;
								rubberbandItem.x = t.entityelement[t.e].x;
								rubberbandItem.y = t.entityelement[t.e].y;
								rubberbandItem.z = t.entityelement[t.e].z;
								rubberbandItem.px = t.entityelement[t.e].x;
								rubberbandItem.py = t.entityelement[t.e].y;
								rubberbandItem.pz = t.entityelement[t.e].z;
								rubberbandItem.rx = t.entityelement[t.e].rx;
								rubberbandItem.ry = t.entityelement[t.e].ry;
								rubberbandItem.rz = t.entityelement[t.e].rz;
								rubberbandItem.quatmode = t.entityelement[t.e].quatmode;
								rubberbandItem.quatx = t.entityelement[t.e].quatx;
								rubberbandItem.quaty = t.entityelement[t.e].quaty;
								rubberbandItem.quatz = t.entityelement[t.e].quatz;
								rubberbandItem.quatw = t.entityelement[t.e].quatw;
								rubberbandItem.scalex = t.entityelement[t.e].scalex;
								rubberbandItem.scaley = t.entityelement[t.e].scaley;
								rubberbandItem.scalez = t.entityelement[t.e].scalez;
								g.entityrubberbandlist.push_back(rubberbandItem);
							}
						}

						// switch widget to newly pasted entity so can instantly widget it about
						if (iAnchorEntityIndex != -1)
						{
							t.widget.pickedEntityIndex = iAnchorEntityIndex;
							t.widget.pickedObject = t.entityelement[iAnchorEntityIndex].obj;
						}

						// batch the paste into a single event finish here
						undosys_multiplevents_finish();

						// ensure gridentity cleared after duplication
						t.gridentity = 0;

						bDraggingActive = false;
						t.onetimeentitypickup = 0;
						iLastSelectedEntityGroup = -1;
						iLastSelectedEntity = -1;
					}
				}
			}
			else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)) || iExecuteCTRLkey == ImGuiKey_Delete)
			{
				HandleObjectDeletion();
				lastKeyTime = (float)ImGui::GetTime();
				iExecuteCTRLkey = 0;
			}

			

		}

		ImGui::End();

		//####################
		//#### Status bar ####
		//####################

		bool bStatusbarActive = true;
		if (gbWelcomeSystemActive == true || bWelcomeScreen_Window) bStatusbarActive = false;
		if (bStatusbarActive)
		{
			int iOldWindowBorderSize = ImGui::GetStyle().WindowBorderSize;
			ImGui::GetStyle().WindowRounding = 0.0f;
			ImGui::GetStyle().WindowBorderSize = 1.0f;

			float paddingy = ImGui::GetStyle().WindowPadding.y;
			float startposy = viewPortSize.y - (ImGuiStatusBar_Size + 2);
			ImGui::SetNextWindowPos(viewPortPos + ImVec2(0.0f, startposy), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, ImGuiStatusBar_Size));

			if (pref.current_style == 25)
			{
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.26f, 0.35f, 1.00f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.11f, 0.16f, 0.22f, 1.00f)); //org ImVec4(0.58f, 0.58f, 0.58f, 1.00f); // ImGui::PopStyleColor();
			}
			if (pref.current_style == 1)
			{
				//PE: VS2022 style
				const float r = pref.status_bar_color.x; // (1.0f / 255.0f) * 14;
				const float g = pref.status_bar_color.y; // (1.0f / 255.0f) * 99;
				const float b = pref.status_bar_color.z; // (1.0f / 255.0f) * 156;
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(r, g, b, 1.00f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(r, g, b, 1.00f));
			}

			ImGui::Begin("Statusbar", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
			ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 10.0f, ImGui::GetCursorPos().y ));
			ImGui::Text("%s", t.laststatusbar_s.Get());
			ImGui::SameLine();
			//Align right.
			int align_checkbox = 96;
			int align_light_checkbox = 106;
			int align_combo_size = 120;
			#ifndef ADDCONTROLSTOSTAUSBAR
			align_light_checkbox = 0;
			align_checkbox = 0;
			#endif
			if (fpe_thread_in_progress())
			{
				extern int g_iScannedFiles;
				cstr title = cStr("Scanning FPE Files: ") + cStr(g_iScannedFiles) + cStr("  ");
				float fTextSize = ImGui::CalcTextSize(title.Get()).x * 1.05;
				ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - fTextSize, ImGui::GetCursorPos().y ));
				ImGui::Text(title.Get());
			}
			else
			{
				//PE: Display status, grid mode ...
				//statusbar
				float fTextSize = ImGui::CalcTextSize(statusbar).x * 1.05; // t.statusbar_s.Get()).x * 1.05;
				ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - fTextSize - 10.0f, ImGui::GetCursorPos().y ));
				ImGui::Text(statusbar); // t.statusbar_s.Get());
			}
			// UI AUDIT 2026-07-28: DEAD CODE — ADDCONTROLSTOSTAUSBAR is never defined; the
			// Editor Light / Top Down / alignment controls live in the top toolbar instead.
			// Ready for deletion for hygiene.
			#ifdef ADDCONTROLSTOSTAUSBAR

			float fTextSize = ImGui::CalcTextSize(t.statusbar_s.Get()).x * 1.05;
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - fTextSize - align_combo_size - align_checkbox - align_light_checkbox, ImGui::GetCursorPos().y));
			ImGui::Text(t.statusbar_s.Get());

			ImGui::SameLine();
			ImVec2 vPos = ImGui::GetCursorPos();

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - align_combo_size - align_checkbox - align_light_checkbox, ImGui::GetCursorPos().y - 5));

			if (ImGui::Checkbox(" Editor Light", &bEditorLight))
			{
				WickedCall_EnableCameraLight(bEditorLight);
			}
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, 4.0f));


			ImGui::SameLine();
			//		ImVec2 vPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(vPos);
			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - align_combo_size - align_checkbox, ImGui::GetCursorPos().y - 5));
			bool bTopDownStatus = !(bool)t.editorfreeflight.mode;
			if (ImGui::Checkbox(" Top Down", &bTopDownStatus)) {
				bForceKey = true;
				if (!bTopDownStatus)
					csForceKey = "f";
				else
					csForceKey = "g";
			}
			ImGui::SameLine();

			ImGui::SetCursorPos(vPos);

			const char* items_align[] = { "NORMAL", "SNAP", "GRID" };
			int item_current_type_selection = 0;
			item_current_type_selection = t.gridentitygridlock;

			ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - align_combo_size, ImGui::GetCursorPos().y - 5.0));
			ImGui::PushItemWidth(align_combo_size - 10);
			if (ImGui::Combo("##BehavioursSimpleInput", &item_current_type_selection, items_align, IM_ARRAYSIZE(items_align))) {
				t.gridentitygridlock = item_current_type_selection;
			}
			ImGui::PopItemWidth();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Alignment");
			//			if (t.gridentitygridlock == 0)  t.statusbar_s = t.statusbar_s + "NORMAL";
			//			if (t.gridentitygridlock == 1)  t.statusbar_s = t.statusbar_s + "SNAP";
			//			if (t.gridentitygridlock == 2)  t.statusbar_s = t.statusbar_s + "GRID";

			#endif

			ImGui::End();
			ImGui::GetStyle().WindowRounding = iOldRounding;
			ImGui::GetStyle().WindowBorderSize = iOldWindowBorderSize;

			if (pref.current_style == 25 || pref.current_style == 1)
			{
				ImGui::PopStyleColor(2);
			}

		}
		else
		{
			if (bWelcomeScreen_Window)
			{
				//PE: We need to display it empty, so we can fill it out with background color on welcome screen.
				float startposy = viewPortSize.y - 32 - 2.0;
				ImGui::SetNextWindowPos(viewPortPos + ImVec2(0.0f, startposy), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(ImGui::GetMainViewport()->Size.x, ImGuiStatusBar_Size));
				ImGui::Begin("Statusbar", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
				ImGui::End();
			}
			else
			{
				//PE: We need to display it empty, or the z order will be wrong later.
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::SetNextWindowPos(viewPortPos + ImVec2(0.0f, viewPortSize.y + 40), ImGuiCond_Always); //Out of screen.
				ImGui::Begin("Statusbar", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
				ImGui::End();
			}
		}


		//Docking.
		ImVec4 OldImGuiColWindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		ImVec4 OldImGuiColChildBg = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];
		ImGui::GetStyle().Colors[ImGuiCol_ChildBg] = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

		//######################################################################
		//#### Default dockspace setup, how is our windows split on screen. ####
		//######################################################################

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking; //ImGuiWindowFlags_MenuBar
		viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos + ImVec2(0, toolbar_size));
		ImGui::SetNextWindowSize(viewport->Size - ImVec2(0, toolbar_size + ImGuiStatusBar_Size));
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGuiWindowFlags oldwindow_flags = window_flags;
		window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpaceAGK", &dockingopen, window_flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar(2);

		window_flags = oldwindow_flags;

		static ImGuiID dock_id_bottom;
		
		//We cant make all windows dock if all windows is NOT undocked first (.ini setup problem ), so refresh_gui_docking == 2
		if (ImGui::DockBuilderGetNode(ImGui::GetID("MyDockspace")) == NULL || refresh_gui_docking == 2)
		{
			//Default docking setup.
			ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
			ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
			ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace); // Add empty node
			ImGui::DockBuilderSetNodePos(dockspace_id, viewport->Pos + ImVec2(0, toolbar_size));
			ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size - ImVec2(0, toolbar_size + ImGuiStatusBar_Size));

			ImGuiID dock_main_id = dockspace_id;
			ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.12f, NULL, &dock_main_id); //Toolbar


			ImGuiID dock_id_right;
			if (viewport->Size.x > 1300)
				dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.14f, NULL, &dock_main_id); //0.20f
			else if (viewport->Size.x < 1100)
				dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.23f, NULL, &dock_main_id); //0.20f
			else
				dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.21f, NULL, &dock_main_id); //0.20f


			ImGuiID dock_id_right2out;
			ImGuiID dock_id_right2 = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.2f, &dock_id_right2out, &dock_main_id); //0.20f

			// create dock ID for above Tutorial Help Window (for Tutorial Video area)
			ImGuiID dock_id_right2below = ImGui::DockBuilderSplitNode(dock_id_right2out, ImGuiDir_Down, 0.26f, NULL, NULL); //0.18

			// leelee, technically not allowed by IMGUI but I fudged the IMGUI code to allow it - seems to work fine!
			ImGuiID dock_id_right3below = ImGui::DockBuilderSplitNode(dock_id_right2out, ImGuiDir_Down, 0.60f, NULL, NULL); //(0.65) PE: Video area must have room for a normal mp4.


			ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.15f, NULL, &dock_main_id); //0.15f
			ImGuiID dock_id_left_down = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.15f, NULL, &dock_id_left); //0.15f
			ImGuiID dock_id_left_down_large = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.30f, NULL, &dock_id_left); //0.15f

			ImGuiID dock_id_left2out;
			ImGuiID dock_id_left2 = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.30f, &dock_id_left2out, &dock_main_id); //0.20f
			ImGuiID dock_id_left2below = ImGui::DockBuilderSplitNode(dock_id_left2out, ImGuiDir_Down, 0.26f, NULL, NULL); //0.18
			ImGuiID dock_id_left3below = ImGui::DockBuilderSplitNode(dock_id_left2out, ImGuiDir_Down, 0.60f, NULL, NULL); //(0.65) PE: Video area must have room for a normal mp4.

			// Disable tab bar for custom toolbar and statusbar
			ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_top);
			node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

			ImGui::DockBuilderDockWindow(TABEDITORNAME, dock_main_id);
			ImGui::DockBuilderDockWindow(TABENTITYNAME, dock_id_left);

			ImGui::DockBuilderDockWindow("Tutorial Video##HelpVideoWindow", dock_id_right2below);
			ImGui::DockBuilderDockWindow("Tutorial Steps##HelpWindow", dock_id_right3below);

			ImGui::DockBuilderDockWindow("Tutorial Video##LeftHelpVideoWindow", dock_id_left2below);
			ImGui::DockBuilderDockWindow("Tutorial Steps##LeftHelpWindow", dock_id_left3below);

			ImGui::DockBuilderDockWindow("Entity Properties##PropertiesWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Character Creator##PropertiesWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Structure Properties##BuilderPropertiesWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Importer##ImporterWindow", dock_id_right);

			ImGui::DockBuilderDockWindow("Terrain Tools##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Sculpt Terrain##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Paint Terrain##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Add Vegetation##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Terrain Tools##Sculpt Terrain##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Terrain Tools##Paint Terrain##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Terrain Tools##Add Vegetation##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Terrain Tools##Add Trees##TerrainToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Terrain Tools##Add Bushes##TerrainToolsWindow", dock_id_right);		

			ImGui::DockBuilderDockWindow("Waypoints##WaypointsToolsWindow", dock_id_right);

			ImGui::DockBuilderDockWindow("Object Tools##EntityToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Environment Effects##VisualsToolsWindow", dock_id_right);
			ImGui::DockBuilderDockWindow("Game Settings##GameSettings", dock_id_right);
			ImGui::DockBuilderDockWindow("Logic Settings##LogicSettings", dock_id_right);
			ImGui::DockBuilderDockWindow("Shooter Genre##GameLogicTools", dock_id_right);
			ImGui::DockBuilderDockWindow("Current Objects##AdditionalIconsWindow", dock_id_left_down_large);
			ImGui::DockBuilderDockWindow("Bug Reporting System##BugReportingWindow", dock_id_right);

			// Disable tab bar.
			//PE: This will not work if you are able to undock windows.
			if (!pref.iAllowUndocking)
			{
				ImGuiDockNode* newnode = ImGui::DockBuilderGetNode(dock_id_left_down_large);
				if (newnode)
					newnode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
			}
			//ImGuiDockNodeFlags_AutoHideTabBar
			dock_main_tabs = dock_main_id;
			dock_tools_windows = dock_id_right;
			ImGui::DockBuilderFinish(dockspace_id);
		}
		
		ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 vOldWindowMinSize = style.WindowMinSize;
		style.WindowMinSize.x = 150.0f;

		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		style.WindowMinSize = vOldWindowMinSize;

		ImGui::End();
		
		// Restore normal window backdrop color, but leave child backdrop color alone (as copied from window backdrop color)
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = OldImGuiColWindowBg;

		if (dock_main_tabs == 0)
			dock_main_tabs = dockspace_id;
		
		grideleprof_uniqui_id = 55000; //If using the new properties widgets outside properties window.

		wi::profiler::EndRange(clSeg); clSeg = wi::profiler::BeginRangeCPU("CL-Tutorial");
		//#######################
		//#### Tutorial Help ####
		//#######################
		if (bHelp_Window && bHelpVideo_Window == false) bHelp_Window = false;
		//PE: Always read in all tutorials.
		if (!bTutorial_Init) 
		{
			//Reset everything.
			tut.bActive = false;
			tut.iSteps = 0;
			strcpy(tut.cStartText, "");
			strcpy(tut.cVideoPath, "");
			tut.bVideoReady = false;
			tut.bVideoInit = false;
			bVideoResumePossible = false;
			for (int il = 0; il < TUTORIALMAXSTEPS; il++) { //Reset
				strcpy(tut.cStepHeader[il], "");
				strcpy(tut.cStepText[il], "");
				strcpy(tut.cStepAction[il], "");
				tut.vOffsetPointer[il] = ImVec2(0, 0);
			}

			get_tutorials();

			//use first entry.
			std::map<std::string, std::string>::iterator it = tutorial_files.begin();
				
			if (it->first.length() > 0) {
				strcpy(cTutorialName, it->first.c_str());
			}

			bTutorial_Init = true;
		}
			
		if (bHelp_Window && current_tutorial != selected_tutorial) 
		{
			current_tutorial = selected_tutorial;

			//Reset everything.
			tut.bActive = false;
			tut.iCurrent_Step = 0;
			tut.iSteps = 0;
			strcpy(tut.cStartText, "");
			strcpy(tut.cVideoPath, "");
			tut.bVideoReady = false;
			tut.bVideoInit = false;
			bVideoResumePossible = false;

			for (int il = 0; il < TUTORIALMAXSTEPS; il++) { //Reset
				strcpy(tut.cStepHeader[il], "");
				strcpy(tut.cStepText[il], "");
				strcpy(tut.cStepAction[il], "");
				tut.vOffsetPointer[il] = ImVec2(0, 0);
			}

			//Read in selected tutorial.
			int count_tut = 0;

			//Find filename to use:
			cstr tut_filename = "";// editors\\uiv3\\tutorial.txt";
			if (tutorial_files.size() > 0) 
			{
				for (std::map<std::string, std::string>::iterator it = tutorial_files.begin(); it != tutorial_files.end(); ++it) 
				{
					if (it->first.length() > 0) 
					{
						if (count_tut++ >= selected_tutorial) 
						{
							tut_filename = it->second.c_str();
							break;
						}
					}
				}
			}

			//Reset active tutorial.
			FILE* fTut = GG_fopen(tut_filename.Get(), "r");
			if (fTut)
			{
				char ctmp[TUTORIALMAXTEXT];
				bool bStart = false;
				while (!feof(fTut))
				{
					fgets(ctmp, TUTORIALMAXTEXT-1, fTut);
					if (strlen(ctmp) > 0 && ctmp[strlen(ctmp) - 1] == '\n')
						ctmp[strlen(ctmp) - 1] = 0;

					if (strncmp(ctmp, "TUT:", 4) == 0)
					{
						if( bStart )
							break; // new section exit.
						bStart = true;
					}
					if (bStart) {
						//Add to active tutorial.
						bool bFound = false;

						if (strncmp(ctmp, "VIDEO:", 6) == 0)
						{
							strcpy(tut.cVideoPath, &ctmp[7]);

							char resolved[MAX_PATH];
							int retval = GetFullPathNameA(tut.cVideoPath, MAX_PATH, resolved, NULL);
							if (retval > 0) {
								strcpy(tut.cVideoPath, resolved);
							}
							bFound = true;
						}
						if (strncmp(ctmp, "START:", 6) == 0)
						{
							strcpy(tut.cStartText, &ctmp[7]);
							bFound = true;
						}

						if (!bFound) {
							for (int il = 1; il < TUTORIALMAXSTEPS; il++) {

								cstr cmp = "STEP"; cmp = cmp + Str(il); cmp = cmp + "-HEADER:";
								if (strncmp(ctmp, cmp.Get(), cmp.Len()) == 0) {
									strcpy(tut.cStepHeader[il - 1], &ctmp[cmp.Len() + 1]);
									bFound = true;
								}
								cmp = "STEP"; cmp = cmp + Str(il); cmp = cmp + "-TEXT:";
								if (strncmp(ctmp, cmp.Get(), cmp.Len()) == 0) {
									strcpy(tut.cStepText[il - 1], &ctmp[cmp.Len() + 1]);
									bFound = true;
								}
								cmp = "STEP"; cmp = cmp + Str(il); cmp = cmp + "-ACTION:";
								if (strncmp(ctmp, cmp.Get(), cmp.Len()) == 0) {
									strcpy(tut.cStepAction[il - 1], &ctmp[cmp.Len() + 1]);
									bFound = true;
								}
								cmp = "STEP"; cmp = cmp + Str(il); cmp = cmp + "-OFFSETX:";
								if (strncmp(ctmp, cmp.Get(), cmp.Len()) == 0) {
									tut.vOffsetPointer[il - 1].x = atof(&ctmp[cmp.Len() + 1]);
									bFound = true;
								}
								cmp = "STEP"; cmp = cmp + Str(il); cmp = cmp + "-OFFSETY:";
								if (strncmp(ctmp, cmp.Get(), cmp.Len()) == 0) {
									tut.vOffsetPointer[il - 1].y = atof(&ctmp[cmp.Len() + 1]);
									bFound = true;
								}
								if (bFound)
								{
									if (tut.iSteps < il)
										tut.iSteps = il;
									break;
								}
							}
						}
					}
				}
				fclose(fTut);
				if (tut.iSteps > 0) 
				{
					//Add Tutorial Complete
					strcpy(tut.cStepHeader[tut.iSteps], "Final Tutorial Step");
					strcpy(tut.cStepText[tut.iSteps], "");
					strcpy(tut.cStepAction[tut.iSteps], "-=DONE=-");
					tut.iSteps++;
				}
			}
		}

		if (refresh_gui_docking == 0)
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Tutorial Video##HelpVideoWindow", &bHelpVideo_Window, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Tutorial Steps##HelpWindow", &bHelp_Window, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Tutorial Video##LeftHelpVideoWindow", &bHelpVideo_Window, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Tutorial Steps##LeftHelpWindow", &bHelp_Window, iGenralWindowsFlags);
			ImGui::End();
		}
		else if (bHelp_Window && tutorial_files.size() > 0)
		{
			char cTutWindowVideoName[256];
			char cTutWindowStepsName[256];

			if (bSetTutorialSectionLeft)
			{
				strcpy(cTutWindowVideoName, "Tutorial Video##LeftHelpVideoWindow");
				strcpy(cTutWindowStepsName, "Tutorial Steps##LeftHelpWindow");
			}
			else
			{
				strcpy(cTutWindowVideoName, "Tutorial Video##HelpVideoWindow");
				strcpy(cTutWindowStepsName, "Tutorial Steps##HelpWindow");
			}

			if( iVideoFindFirstFrame > 0) {
				if (iVideoFindFirstFrame == 1) {
					PauseAnim(tut.bVideoID);
					bVideoResumePossible = false;
				}
				iVideoFindFirstFrame--;
			}

			switch (iVideoDelayExecute) {
				case 1: //Play restart
				{
					iVideoDelayExecute = 0;
					StopAnimation(tut.bVideoID);
					PlayAnimation(tut.bVideoID);
					SetRenderAnimToImage(tut.bVideoID, true);
					UpdateAllAnimation();
					Sleep(50); //Sleep so we get a video texture in the next call.
					UpdateAllAnimation();
					SetVideoVolume(100.0);
					bVideoResumePossible = false;
					break;
				}
				case 2: //Resume
				{
					iVideoDelayExecute = 0;
					ResumeAnim(tut.bVideoID);
					break;
				}
				case 3: //Pause
				{
					iVideoDelayExecute = 0;
					PauseAnim(tut.bVideoID);
					bVideoResumePossible = true;
					break;
				}
				default:
					break;
			}

			if (bVideoPlayerMaximized) 
			{
				ImGui::SetNextWindowSize(ImVec2(48 * ImGui::GetFontSize(), 46 * ImGui::GetFontSize()), ImGuiCond_Once);
				ImGui::SetNextWindowPosCenter(ImGuiCond_Once);
				ImGui::Begin("Tutorial Video##Videos2MaxSize", &bVideoPlayerMaximized, 0);
			}
			else 
			{
				ImGui::Begin(cTutWindowVideoName, &bHelpVideo_Window, iGenralWindowsFlags);
			}
			ImGui::Indent(10);
			ImGui::PushItemWidth(-10);

			if (ImGui::BeginCombo("##SelectYourTutorial", cTutorialName) ) // The second parameter is the label previewed before opening the combo.
			{
				int vloop = 0;
				for (std::map<std::string, std::string>::iterator it = tutorial_files.begin(); it != tutorial_files.end(); ++it)
				{
					if (it->first.length() > 0)
					{
						bool is_selected = false;
						if (strcmp(it->first.c_str(), cTutorialName) == 0)
							is_selected = true;
						if (ImGui::Selectable(it->first.c_str(), is_selected))
						{
							//Change Tutorial.
							strcpy(cTutorialName, it->first.c_str());
							selected_tutorial = vloop;
						}
						if (is_selected)
							ImGui::SetItemDefaultFocus();
						vloop++;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			// and a force tutorial mode
			bool bForceASelection = false;
			if (strlen(cForceTutorialName) > 0)
			{
				strcpy(cTutorialName, cForceTutorialName);
				strcpy(cForceTutorialName, "");
				int vloop = 0;
				for (std::map<std::string, std::string>::iterator it = tutorial_files.begin(); it != tutorial_files.end(); ++it)
				{
					if (it->first.length() > 0)
					{
						if (strcmp(it->first.c_str(), cTutorialName) == 0)
						{
							strcpy(cTutorialName, it->first.c_str());
							selected_tutorial = vloop;
						}
						vloop++;
					}
				}
				bForceASelection = true;
			}

			// use video panel (wait until videos GOOD and other Rick-Requests)
			ImVec4 oldImGuiCol_ChildWindowBg = ImGui::GetStyle().Colors[ImGuiCol_ChildWindowBg];
			#ifdef ENABLETUTORIALVIDEOS
			{
				if (!tut.bVideoInit)
				{
					if (tut.bVideoID > 0) {
						if (AnimationExist(tut.bVideoID)) {
							if (AnimationPlaying(tut.bVideoID))
								StopAnimation(tut.bVideoID);

							DeleteAnimation(tut.bVideoID);
							tut.bVideoID = 0;
						}
					}

					t.tvideofile_s = tut.cVideoPath;
					tut.bVideoID = 0;
					t.text_s = Lower(Right(t.tvideofile_s.Get(), 4));
					if (t.text_s == ".ogv" || t.text_s == ".mp4")
					{
						tut.bVideoID = 32;
						for (int itl = 1; itl <= 32; itl++)
						{
							if (AnimationExist(itl) == 0) { tut.bVideoID = itl; break; }
						}
						char pFinalVideoFilePath[MAX_PATH];
						strcpy(pFinalVideoFilePath, t.tvideofile_s.Get());
						GG_GetRealPath(pFinalVideoFilePath, 0);
						if (LoadAnimation(pFinalVideoFilePath, tut.bVideoID, g.videoprecacheframes, g.videodelayedload, 1) == false)
						{
							tut.bVideoID = -999;
						}
					}
					if (tut.bVideoID > 0) 
					{
						PlaceAnimation(tut.bVideoID, -1, -1, -1, -1);
						SetRenderAnimToImage(tut.bVideoID, true);
						//Try to get first frame.
						StopAnimation(tut.bVideoID);
						PlayAnimation(tut.bVideoID);
						SetRenderAnimToImage(tut.bVideoID, true);
						iVideoFindFirstFrame = 4;
						UpdateAllAnimation();
						bVideoResumePossible = false;
						bVideoPerccentStart = false;
					}
					tut.bVideoInit = true;
				}

				float fRatio = 1.0f / ((float)GetDesktopWidth() / (float)GetDesktopHeight());

				ID3D11ShaderResourceView* lpVideoTexture = GetAnimPointerView(tut.bVideoID);
				float fVideoW = GetAnimWidth(tut.bVideoID);
				float fVideoH = GetAnimHeight(tut.bVideoID);
				if (tut.bVideoInit && tut.bVideoID > 0 && lpVideoTexture) {
					fRatio = 1.0f / (fVideoW / fVideoH);
				}

				float videoboxheight = (ImGui::GetContentRegionAvail().x - 10.0) * fRatio;

				oldImGuiCol_ChildWindowBg = ImGui::GetStyle().Colors[ImGuiCol_ChildWindowBg];
				ImGui::GetStyle().Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
				ImGui::BeginChild("Video##TutorialVideo", ImVec2(ImGui::GetContentRegionAvail().x - 10.0, videoboxheight), true, iGenralWindowsFlags);
				window = ImGui::GetCurrentWindow();
				ImRect image_bb(window->DC.CursorPos, window->DC.CursorPos + ImGui::GetContentRegionAvail());
				if (lpVideoTexture) {
					SetRenderAnimToImage(tut.bVideoID, true);
					float animU = GetAnimU(tut.bVideoID);
					float animV = GetAnimV(tut.bVideoID);
					ImVec2 uv0 = ImVec2(0, 0);
					ImVec2 uv1 = ImVec2(animU, animV);
					window->DrawList->AddImage((ImTextureID)lpVideoTexture, image_bb.Min, image_bb.Max, uv0, uv1, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
				}

				if( !(tut.bVideoID > 0 && AnimationExist(tut.bVideoID) && AnimationPlaying(tut.bVideoID) ))
				{
					//Display a play button.
					ImVec2 vOldPos = ImGui::GetCursorPos();
					float fPlayButSize = ImGui::GetContentRegionAvail().x * 0.15;
					float fCenterX = (ImGui::GetContentRegionAvail().x*0.5) - (fPlayButSize*0.5);
					float fCenterY = (videoboxheight*0.5) - (fPlayButSize*0.5);
					ImGui::SetCursorPos(ImVec2(fCenterX, fCenterY));
					ImVec4 vColorFade = { 1.0,1.0,1.0,0.5 };
					if (ImGui::ImgBtn(MEDIA_PLAY, ImVec2(fPlayButSize, fPlayButSize), ImColor(255, 255, 255, 0), drawCol_normal*vColorFade, drawCol_hover*vColorFade, drawCol_Down*vColorFade, -1, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
					{
						bVideoPerccentStart = true;
						if (bVideoResumePossible) {
							iVideoDelayExecute = 2; //resume
						}
						else {
							iVideoDelayExecute = 1; //play - restart.
						}
					}
					if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Play");

					ImGui::SetCursorPos(vOldPos);
				}

				if (ImGui::IsMouseHoveringRect(image_bb.Min, image_bb.Max)) 
				{
					if (!bStoryboardWindow && !bProceduralLevel)
					{
						if (ImGui::IsMouseDoubleClicked(0))
						{
							bVideoPlayerMaximized = 1 - bVideoPlayerMaximized;
						}
					}
				}

				ImGui::EndChild();
				ImGui::GetStyle().Colors[ImGuiCol_ChildWindowBg] = oldImGuiCol_ChildWindowBg;

				if (tut.bVideoID > 0) {
					if (AnimationExist(tut.bVideoID)) {

						//ImGui::SameLine();
						float fdone = GetAnimPercentDone(tut.bVideoID) / 100.0f;
						if (!bVideoPerccentStart) fdone = 0.0f;

						ImGui::ProgressBar(fdone, ImVec2(ImGui::GetContentRegionAvail().x - 10, 6), "");

#define MEDIAICONSIZE 20

						if (ImGui::ImgBtn(MEDIA_PLAY, ImVec2(MEDIAICONSIZE, MEDIAICONSIZE), ImColor(255, 255, 255, 0), drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
						{
							bVideoPerccentStart = true;
							if (bVideoResumePossible) {
								iVideoDelayExecute = 2; //resume
							}
							else {
								iVideoDelayExecute = 1; //play - restart.
							}
						}
						if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Play");
						ImGui::SameLine();
						if (ImGui::ImgBtn(MEDIA_PAUSE, ImVec2(MEDIAICONSIZE, MEDIAICONSIZE), ImColor(255, 255, 255, 0), drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
						{
							iVideoDelayExecute = 3; // pause
						}
						if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Pause");
						ImGui::SameLine();
						if (ImGui::ImgBtn(MEDIA_REFRESH, ImVec2(MEDIAICONSIZE, MEDIAICONSIZE), ImColor(255, 255, 255, 0), drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
						{
							bVideoPerccentStart = true;
							iVideoDelayExecute = 1; //play - restart.
						}
						if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Restart");



						if (!bVideoPlayerMaximized)
						{
							ImGui::SameLine();
							if (ImGui::ImgBtn(MEDIA_MAXIMIZE, ImVec2(MEDIAICONSIZE, MEDIAICONSIZE), ImColor(255, 255, 255, 0), drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, true,false,false,false,false, bBoostIconColors))
							{
								bVideoPlayerMaximized = true;
							}
							if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Maximize");
						}
						else
						{
							ImGui::SameLine();
							if (ImGui::ImgBtn(MEDIA_MINIMIZE, ImVec2(MEDIAICONSIZE, MEDIAICONSIZE), ImColor(255, 255, 255, 0), drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, true,false,false,false,false, bBoostIconColors))
							{
								bVideoPlayerMaximized = false;
							}
							if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Minimize");
						}



					}
				}
			}

			if (bVideoPlayerMaximized) {
				std::map<std::string, std::string>::iterator it = tutorial_description.find(cTutorialName);
				if (it != tutorial_description.end()) {
					cVideoDescription = it->second.c_str();
					ImGui::Separator();
					ImGui::Text("Description");
					ImGui::TextWrapped(cVideoDescription.Get());
				}
				bImGuiGotFocus = true;
			}

			if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0) {
				//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
				ImGui::Text("");
				ImGui::Text("");
			}

			#endif

			ImGui::End();

			///

			ImGui::Begin(cTutWindowStepsName, &bHelp_Window, iGenralWindowsFlags);

			ImGui::PushItemWidth(-10);
			if (ImGui::StyleButton(tut.cStartText, ImVec2(ImGui::GetContentRegionAvail().x - 10.0f, 0.0f))) {
				tut.bActive = true;
				tut.iCurrent_Step = 0;
				//PE: Minimize video player.
				bVideoPlayerMaximized = false;
			}
			ImGui::PopItemWidth();

			for (int il = 0; il < tut.iSteps; il++) {

				int additional_lines = 0;
				char line_split[TUTORIALMAXTEXT], *line_found = NULL, *line_start = NULL;
				strcpy(line_split, tut.cStepText[il]);
				line_start = line_found = &line_split[0];
				while ((line_found = (char *)pestrcasestr(line_found, "\\n"))) {
					line_found++;
					line_found++;
					additional_lines++;
				}
				float stepboxheight = mCharAdvance.y * (3 + additional_lines);

				cstr uniqueid = "##STEP";
				uniqueid += Str(il);

				float fOldChildRounding = ImGui::GetStyle().ChildRounding;
				ImGui::GetStyle().ChildRounding = 10.0f;

				if (tut.bActive && tut.iCurrent_Step == il) { //Set current step color.
					ImGui::GetStyle().Colors[ImGuiCol_ChildWindowBg] = ImGui::GetStyle().Colors[ImGuiCol_Button];
					static int last_scroll_set = -1;
					if (tut.iCurrent_Step != last_scroll_set) {
						last_scroll_set = tut.iCurrent_Step;
						ImGui::SetScrollHereY();
					}
				}

				ImGui::BeginChild(uniqueid.Get(), ImVec2(ImGui::GetContentRegionAvail().x-10.0, stepboxheight) , true, iGenralWindowsFlags);
					
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, mCharAdvance.y*0.5 ));

				ImGui::SetWindowFontScale(1.15);
				ImGui::TextCenter(tut.cStepHeader[il]);
				ImGui::SetWindowFontScale(1.0);

				//Update Score:
				tut.fScore = (float) tut.iCurrent_Step / (float) (tut.iSteps-1) * 100.0f;
				if (tut.fScore <= 0.99f) tut.fScore = 0.0f;
				if (tut.fScore >= 99.9f) {
					tut.fScore = 100.0f;
					strcpy(tut.cStepText[tut.iSteps - 1], "COMPLETE - Well Done!");
				}
				else {
					strcpy(tut.cStepText[tut.iSteps - 1], "INCOMPLETE");
				}
									
				strcpy(line_split, tut.cStepText[il]);
				line_start = line_found = &line_split[0];

				while ((line_found = (char *)pestrcasestr(line_found, "\\n"))) {
					*line_found = 0;
					ImGui::TextCenter(line_start);
					line_found++;
					line_found++;
					line_start = line_found;
				}
				ImGui::TextCenter(line_start);

				ImGui::EndChild();

				ImGui::GetStyle().ChildRounding = fOldChildRounding;
				ImGui::GetStyle().Colors[ImGuiCol_ChildWindowBg] = oldImGuiCol_ChildWindowBg;

				ImGui::Spacing();
			}

			ImGui::Indent(-10);

			if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0) 
			{
				//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
				ImGui::Text("");
				ImGui::Text("");
			}

			ImRect bbwin(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize());
			if (ImGui::IsMouseHoveringRect(bbwin.Min, bbwin.Max))
			{
				bImGuiGotFocus = true;
			}
			if (ImGui::IsAnyItemFocused()) {
				bImGuiGotFocus = true;
			}
			//Tutorial really small min, as we have multiply dock to the same side.
			CheckMinimumDockSpaceSize(20.0f);

			ImGui::End();

		}
		else 
		{
			//Help window closed , check if we ned to free any videos.
			if (tut.bVideoID > 0) {
				current_tutorial = -1; //make sure to reopen when window visible again.
				if (AnimationExist(tut.bVideoID)) {
					if (AnimationPlaying(tut.bVideoID))
						StopAnimation(tut.bVideoID);
					DeleteAnimation(tut.bVideoID);
					tut.bVideoID = 0;
					bVideoResumePossible = false;
				}
			}
		}

		//###############################
		//#### Welcome Screen Window ####
		//###############################
		if( gbWelcomeSystemActive == false )
		{
			// only show Welcome Screen if the 'old' welcome/announcement is not in effect
			Welcome_Screen();
		}
		else
		{
			Welcome_Screen(); //Also dislay welcome screen behind in new design. to hide 3D editor.
		}

		wi::profiler::EndRange(clSeg); clSeg = wi::profiler::BeginRangeCPU("CL-Market+Misc");
		//#############################
		//#### Market place Window ####
		//#############################


		#define STOREPROMOICONS 8

		float fMarketplacePanelHeight = 48.0f;// 55.0f; fits 4:3 aspect!

			if (g_bFreeTrialVersion == true)
			{
				if (bMarketplace_Window == true)
				{
					bFreeTrial_Window = true;
					bMarketplace_Window = false;
				}
			}
			if (refresh_gui_docking == 0)
			{
				ImGui::SetNextWindowSize(ImVec2(68 * ImGui::GetFontSize(), fMarketplacePanelHeight * ImGui::GetFontSize()), ImGuiCond_Always);
				ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
				bool bTmp = true;
				ImGui::Begin("Marketplace##MarketplaceWindow", &bTmp, 0);
				ImGui::End();
			}
			else if (bMarketplace_Window)
			{
				int StorePromoItems = STOREPROMOICONS;
				static int gg_max_dlc[STOREPROMOICONS];
				static cstr gg_max_link[STOREPROMOICONS];
				static int sketchfab_dlc[STOREPROMOICONS];
				static cstr sketchfab_link[STOREPROMOICONS];
				static int shockwavesound_dlc[STOREPROMOICONS];
				static cstr shockwavesound_link[STOREPROMOICONS];
				static int community_dlc[STOREPROMOICONS];
				static cstr community_link[STOREPROMOICONS];
				static int gcstore_dlc[STOREPROMOICONS];
				static cstr gcstore_imageurl[STOREPROMOICONS];
				static cstr gcstore_link[STOREPROMOICONS];
				static int import_image[STOREPROMOICONS];
				static cstr import_fpe[STOREPROMOICONS];
				if (!bMarketplace_Init)
				{
					for (int i = 0; i < STOREPROMOICONS; i++)
					{
						image_setlegacyimageloading(true);
						if(gg_max_dlc[i] > 0 && ImageExist(gg_max_dlc[i])) DeleteImage(gg_max_dlc[i]);
						if (sketchfab_dlc[i] > 0 && ImageExist(sketchfab_dlc[i])) DeleteImage(sketchfab_dlc[i]);
						if (shockwavesound_dlc[i] > 0 && ImageExist(shockwavesound_dlc[i])) DeleteImage(shockwavesound_dlc[i]);
						if (community_dlc[i] > 0 && ImageExist(community_dlc[i])) DeleteImage(community_dlc[i]);
						if (gcstore_dlc[i] > 0 && ImageExist(gcstore_dlc[i])) DeleteImage(gcstore_dlc[i]);
						if (import_image[i] > 0 && ImageExist(import_image[i])) DeleteImage(import_image[i]);
						image_setlegacyimageloading(false);
						gg_max_dlc[i] = 0;
						gg_max_link[i] = "";
						sketchfab_dlc[i] = 0;
						sketchfab_link[i] = "";
						shockwavesound_dlc[i] = 0;
						shockwavesound_link[i] = "";
						community_dlc[i] = 0;
						community_link[i] = "";
						gcstore_dlc[i] = 0;
						gcstore_imageurl[i] = "";
						gcstore_link[i] = "";	
						import_image[i] = 0;
						import_fpe[i] = "";
					}
					SetMipmapNum(1);
					image_setlegacyimageloading(true);
					
					loadMarketplaceData(gg_max_dlc, gg_max_link, sketchfab_dlc, sketchfab_link, shockwavesound_dlc, shockwavesound_link, 
										community_dlc, community_link, gcstore_dlc, gcstore_imageurl, gcstore_link);

					image_setlegacyimageloading(false);
					SetMipmapNum(-1);
					bMarketplace_Init = true;

					//##################################
					//### Load last imported models. ###
					//##################################

					int iImportPromoIcon = 0;
					for (int i = 0; i < 10; i++)
					{
						if (strlen(pref.last_import_files[i]) > 0)
						{
							//PE: Check if we got a cached thumb in correct format.
							char cTmp[MAX_PATH];
							strcpy(cTmp, "entitybank\\");
							strcat(cTmp, pref.last_import_files[i]);
							CreateBackBufferCacheName(pref.last_import_files[i], 512, 288);
							extern StoryboardStruct Storyboard;
							if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
							{
								GG_SetWritablesToRoot(true);
							}
							if (FileExist(BackBufferCacheName.Get()) && FileExist(cTmp) )
							{
								SetMipmapNum(1);
								image_setlegacyimageloading(true);
								import_image[iImportPromoIcon] = MARKETPLACE_ICONS + (STOREPROMOICONS * 3) + iImportPromoIcon;

								LoadImage((char *)BackBufferCacheName.Get(), import_image[iImportPromoIcon]);
								if (ImageExist(import_image[iImportPromoIcon]))
								{
									import_fpe[iImportPromoIcon] = pref.last_import_files[i];
									iImportPromoIcon++;
								}
								image_setlegacyimageloading(false);
								SetMipmapNum(-1);
							}
							GG_SetWritablesToRoot(false);
						}
						if (iImportPromoIcon >= STOREPROMOICONS)
							break;
					}
				}

				// TGC only sell objects right now!
				bool bHideGGMaxMarketplace = false;
				if (iDisplayLibraryType != 0 || iDisplayLibrarySubType !=0)
					bHideGGMaxMarketplace = true;

				if (bHideGGMaxMarketplace)
					ImGui::SetNextWindowSize(ImVec2(((90.0f / 4.0f) * 2) * ImGui::GetFontSize(), 33 * ImGui::GetFontSize()), ImGuiCond_Always);// ImGuiCond_Once);
				else
					ImGui::SetNextWindowSize(ImVec2(((90.0f / 4.0f) * 3) * ImGui::GetFontSize(), fMarketplacePanelHeight * ImGui::GetFontSize()), ImGuiCond_Always);//ImGuiCond_Once);

				ImGui::SetNextWindowPosCenter(ImGuiCond_Always);// ImGuiCond_Once);
				
				ImGui::Begin("Marketplace##MarketplaceWindow", &bMarketplace_Window, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings);
				ImGui::Indent(10);

				ImGui::Text("");
				ImVec2 vCurPos = ImGui::GetCursorPos();
				float fFontSize = ImGui::GetFontSize();
				int icon_size = ImGui::GetFontSize()*3.0;
				ImVec2 VIconSize = { (float)icon_size, (float)icon_size };
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, -15));
				if (ImGui::ImgBtn(TOOL_GOBACK, VIconSize, ImVec4(0, 0, 0, 0), drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
				{
					bMarketplace_Window = false;
				}
				if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Exit Marketplace");
				ImGui::SameLine();

				ImVec2 VHeaderSize;
				if (bHideGGMaxMarketplace)
				{
					ImGui::SetWindowFontScale(1.0);
					ImGui::Text("");
					VHeaderSize = { 365, 39 };
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(154, -58));
					ImGui::ImgBtn(MARKETPLACE_HEADER, VHeaderSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), 0, 0, 0, 0, false, false, false, false, false, false);
					ImGui::Text("");
				}
				else
				{
					VHeaderSize = { 730, 78 };
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(78, -10));
					ImGui::ImgBtn(MARKETPLACE_HEADER, VHeaderSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), 0, 0, 0, 0, false, false, false, false, false, false);
				}

				float fButWidth = 150.0f;
				
				if (bHideGGMaxMarketplace)
					ImGui::Columns(1, "Marketplacecolumns4", false);  //false no border
				else
					ImGui::Columns(2, "Marketplacecolumns4", false);  //false no border

				float fContentWidth = ImGui::GetContentRegionAvailWidth();
				float fLogoWidth = fContentWidth;
				float fImageWidth = 460;
				float fImageHeight = 215;

				//Use same size on all logos.
				if (ImageExist(MARKETPLACE_GGMAX))
				{
					fImageWidth = ImageWidth(MARKETPLACE_GGMAX);
					fImageHeight = ImageHeight(MARKETPLACE_GGMAX);
				}
				float fScale = fLogoWidth / fImageWidth;
				float fRatio = fImageHeight / fImageWidth;
				ImVec2 vLogoSize = { fLogoWidth , fImageHeight * fScale };
				ImVec2 vPromoSize = { fLogoWidth , fImageHeight * fScale };

				ImVec2 vSizeOfScrollablePanels = ImVec2(vPromoSize.x, ((vPromoSize.y-15.0f) * 3));

				vPromoSize *= 0.5;
				vPromoSize = vPromoSize - ImVec2(15.0f, 15.0f); //ImVec2(5.0f, 5.0f);

				ImVec2 vYOffsetToButtons = ImVec2(0, (((475 / 68.6f) * vPromoSize.y)));
				if (bHideGGMaxMarketplace) vYOffsetToButtons = ImVec2(0, (((150 / 68.6f)*vPromoSize.y)));

				//MAX x=154.25 y=69.432

				int TextureID = MARKETPLACE_FILLER;
				float fPromoHeight = ImGui::GetCursorPosY();
				if (!bHideGGMaxMarketplace)
				{
					if (ImageExist(MARKETPLACE_GGMAX)) TextureID = MARKETPLACE_GGMAX;
					if (ImGui::ImgBtn(TextureID, vLogoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false, false, false, false, false))
					{
						ExecuteFile("https://store.steampowered.com/app/1247290/GameGuru_MAX/", "", "", 0);
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", "Click to view the GameGuru MAX Steam Store Page");
					}

					//#### Promo icon section. ####
					vCurPos = ImGui::GetCursorPos();
					fPromoHeight = ImGui::GetCursorPosY();
					for (int i = 0; i < 2; i++)
					{
						TextureID = MARKETPLACE_FILLER;
						ImVec4 vFadeIcons = { 1.0,1.0,1.0,0.2 };
						if (ImageExist(gg_max_dlc[i]))
						{
							TextureID = gg_max_dlc[i];
							vFadeIcons = { 1.0,1.0,1.0,1.0 };
						}
						ImGui::SetCursorPos(ImVec2(vCurPos.x+10.0f, ImGui::GetCursorPos().y));
						if (ImGui::ImgBtn(TextureID, vPromoSize*2, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)*vFadeIcons, drawCol_hover*vFadeIcons, drawCol_Down*vFadeIcons, -1, 0, 0, 0, false, false, false, false, false, false))
						{
							// already aware of steam and non-steam, assigned directly from "MarketplaceData.json"
							if (gg_max_link[i].Len() > 0)
							{
								ExecuteFile(gg_max_link[i].Get(), "", "", 0);
							}
						}
					}
					fPromoHeight = ImGui::GetCursorPosY() - fPromoHeight;

					ImGui::SetWindowFontScale(1.4f);		
					ImGui::SetCursorPos(vCurPos + vYOffsetToButtons);
					{
						if (ImGui::StyleButton("Get More DLC", ImVec2(vLogoSize.x, fFontSize*2.0)))
						{
							DeleteWaypointsAddedToCurrentCursor();
							CloseDownEditorProperties();
							ExecuteFile("https://store.steampowered.com/dlc/1247290/GameGuru_MAX/", "", "", 0);
							bTriggerCloseEntityWindow = true;
						}
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click to view the GameGuru MAX Steam DLC Page");
					}
					ImGui::SetWindowFontScale(1.0);

					ImGui::NextColumn();
				}

				TextureID = MARKETPLACE_FILLER;
				if (ImageExist(MARKETPLACE_GCSTORE))
					TextureID = MARKETPLACE_GCSTORE;

				if (ImGui::ImgBtn(TextureID, vLogoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false, false, false, false, false))
				{
					ExecuteFile("https://gamecreator.store/max?r=tgc", "", "", 0);
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click to view the Game Creator Store Website");

				//#### Promo icon section. ####
				vCurPos = ImGui::GetCursorPos();
				if ( iDisplayLibraryType == 0 )
				{
					fPromoHeight = ImGui::GetCursorPosY();
					ImGui::BeginChild("##gamecreatorstorescrollable", vSizeOfScrollablePanels, false, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings);
					for (int i = 0; i < 7; i++)
					{
						TextureID = MARKETPLACE_FILLER;
						ImVec4 vFadeIcons = { 1.0,1.0,1.0,0.2 };
						if (ImageExist(gcstore_dlc[i]))
						{
							TextureID = gcstore_dlc[i];
							vFadeIcons = { 1.0,1.0,1.0,1.0 };
						}
						else
						{
							// slowly load in preview thumbs over time so as not to stall the UI
							if (strlen(gcstore_imageurl[i].Get()) > 0)
							{
								static DWORD g_dwLoadGameCreatorStorePreviewsTimer = 0;
								if (timeGetTime() > g_dwLoadGameCreatorStorePreviewsTimer + 100)
								{
									// attempt a download
									char cUrl[256];
									char pDataReturned[132000];
									memset(pDataReturned, 0, sizeof(pDataReturned));
									DWORD dwDataReturnedSize = 0;
									char pImageFile[256];
									sprintf(pImageFile, "downloads\\gcStore%d.png", 1 + i);
									if (FileExist(pImageFile) == 1) DeleteAFile(pImageFile);
									std::string url = gcstore_imageurl[i].Get();
									replaceAll(url, "\\/", "/");
									replaceAll(url, "https://gcs-product-media.fra1.cdn.digitaloceanspaces.com", "");
									replaceAll(url, "http://gcs-product-media.fra1.cdn.digitaloceanspaces.com", "");
									strcpy(cUrl, url.c_str());
									LPSTR pPassInURL = cUrl;
									int iError = StoreOpenURLForDataOrFile("gcs-product-media.fra1.cdn.digitaloceanspaces.com", pDataReturned, &dwDataReturnedSize, "", "GET", pPassInURL, pImageFile);
									if (iError > 0)
									{
										// error - no image today!
										strcpy (pImageFile, "editors\\marketplace\\gcStore0.png");
									}

									// and load the preview
									image_setlegacyimageloading(true);
									LoadImage(pImageFile, gcstore_dlc[i]);
									image_setlegacyimageloading(false);

									// delay next attempt for a time
									g_dwLoadGameCreatorStorePreviewsTimer = timeGetTime();
								}
							}
						}
						if (ImGui::ImgBtn(TextureID, vPromoSize * 2, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)*vFadeIcons, drawCol_hover*vFadeIcons, drawCol_Down*vFadeIcons, -1, 0, 0, 0, false, false, false, false, false, false))
						{
							if (gcstore_link[i].Len() > 0)
							{
								ExecuteFile(gcstore_link[i].Get(), "", "", 0);
							}
						}
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click to view this object on the Game Creator Store Website");
					}
					ImGui::EndChild();
					ImGui::Text("");
					fPromoHeight = ImGui::GetCursorPosY() - fPromoHeight;
				}
				else
				{
					ImGui::Text("");
					if (iDisplayLibraryType == 1)
					{
						float fStoreAudioTextHeight = fPromoHeight * 0.5;
						ImGui::SetCursorPos(vCurPos + ImVec2(0, fStoreAudioTextHeight - (fFontSize*2.5)));
						ImGui::TextCenter("GameGuru MAX supports");
						ImGui::TextCenter("WAV,MP3,OGG files.");
					}
				}

				ImGui::SetWindowFontScale(1.4);
				ImGui::SetCursorPos(vCurPos + vYOffsetToButtons);
				if (ImGui::StyleButton("Access Your Store Items", ImVec2(vLogoSize.x, fFontSize*2.0)))
				{
					CloseAllOpenTools();
					extern int iDownloadStoreProgress;
					extern bool bDownloadStoreError;
					extern char cDownloadStoreError[4096];
					iDownloadStoreProgress = 0;
					bDownloadStoreError = false;
					strcpy(cDownloadStoreError, "");
					bDownloadStore_Window = true;
					bPreferences_Window = false;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Log-in and download items purchased from the store");
				ImGui::SetWindowFontScale(1.0f);
				ImGui::NextColumn();

				if (iDisplayLibraryType == 2 || iDisplayLibraryType == 3 || iDisplayLibraryType == 4 || iDisplayLibraryType == 5)
				{
					TextureID = MARKETPLACE_FILLER;
					if (ImageExist(MARKETPLACE_COMMUNITY))
						TextureID = MARKETPLACE_COMMUNITY;

					if (ImGui::ImgBtn(TextureID, vLogoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false, false, false, false, false))
					{
						ExecuteFile("https://forum.game-guru.com/board/1", "", "", 0);
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "GameGuru MAX Community");

					//#### Promo icon section. ####
					vCurPos = ImGui::GetCursorPos();
					fPromoHeight = ImGui::GetCursorPosY();
					for (int i = 0; i < StorePromoItems; i++)
					{
						TextureID = MARKETPLACE_FILLER;
						ImVec4 vFadeIcons = { 1.0,1.0,1.0,0.2 };

						if (ImageExist(community_dlc[i]))
						{
							TextureID = community_dlc[i];
							vFadeIcons = { 1.0,1.0,1.0,1.0 };
						}
						if (ImGui::ImgBtn(TextureID, vPromoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)*vFadeIcons, drawCol_hover*vFadeIcons, drawCol_Down*vFadeIcons, 0, 0, 0, 0, false, false, false, false, false, false))
						{
							if (community_link[i].Len() > 0)
							{
								ExecuteFile(community_link[i].Get(), "", "", 0);
							}
						}
						if (i % 2 == 0) ImGui::SameLine();
					}
					ImGui::Text("");
					fPromoHeight = ImGui::GetCursorPosY() - fPromoHeight;

					ImGui::SetWindowFontScale(1.4);
					ImGui::SetCursorPos(vCurPos + vYOffsetToButtons);
					if (ImGui::StyleButton("Visit GameGuru MAX Community", ImVec2(vLogoSize.x, fFontSize*2.0)))
					{
						DeleteWaypointsAddedToCurrentCursor();
						CloseDownEditorProperties();
						bTriggerCloseEntityWindow = true;
						//bMarketplace_Window = false;
						ExecuteFile("https://forum.game-guru.com/board/1", "", "", 0);
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Visit GameGuru MAX Community");

					ImGui::SetWindowFontScale(1.0);
				}
				else
				{
					if (iDisplayLibraryType == 1)
					{
						TextureID = MARKETPLACE_FILLER;
						if (ImageExist(MARKETPLACE_SHOCKWAVESOUND))
							TextureID = MARKETPLACE_SHOCKWAVESOUND;

						if (ImGui::ImgBtn(TextureID, vLogoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false, false, false, false, false))
						{
							ExecuteFile("https://www.shockwave-sound.com/a/e43bd272af", "", "", 0);
						}
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click to view the Shockwave Sound Website");

						//#### Promo icon section. ####
						vCurPos = ImGui::GetCursorPos();

						ImGui::Text("");
						float fShockwaveTextHeight = fPromoHeight * 0.5;
						ImGui::SetCursorPos(vCurPos + ImVec2(0, fShockwaveTextHeight - (fFontSize*2.5)));
						ImGui::TextCenter("GameGuru MAX supports");
						ImGui::TextCenter("WAV,MP3,OGG files.");

						ImGui::SetWindowFontScale(1.4);
						ImGui::SetCursorPos(vCurPos + vYOffsetToButtons);
						if (ImGui::StyleButton("Visit Shockwave Sound Store", ImVec2(vLogoSize.x, fFontSize*2.0)))
						{
							DeleteWaypointsAddedToCurrentCursor();
							CloseDownEditorProperties();
							bTriggerCloseEntityWindow = true;
							ExecuteFile("https://www.shockwave-sound.com/a/e43bd272af", "", "", 0);
						}
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click to view the Shockwave Sound Website");

						ImGui::SetWindowFontScale(1.0);
					}
					else
					{
						TextureID = MARKETPLACE_FILLER;
						ImGui::SetWindowFontScale(1.0);
					}
				}

				ImGui::SetWindowFontScale(1.0);

				// completed marketplace
				ImGui::Columns(1);
				ImGui::Indent(-10);
				bImGuiGotFocus = true;
				ImGui::End();
			}


		//###########################
		//#### Free Trial Window ####
		//###########################

		static int iCountingFreeDialogClicks = 0;
		if (refresh_gui_docking == 0)
		{
			ImGui::SetNextWindowSize(ImVec2(68 * ImGui::GetFontSize(), (fMarketplacePanelHeight+2.0f) * ImGui::GetFontSize()), ImGuiCond_Always);
			ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
			bool bTmp = true;
			ImGui::Begin("FreeTrial##FreeTrialWindow", &bTmp, 0);
			ImGui::End();
		}
		else if (bFreeTrial_Window)
		{
			if (!bFreeTrial_Init)
			{
				bFreeTrial_Init = true;
			}
			ImGui::SetNextWindowSize(ImVec2(((90.0f / 4.0f) * 3) * ImGui::GetFontSize(), (fMarketplacePanelHeight + 2.0f) * ImGui::GetFontSize()), ImGuiCond_Always);
			ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
			ImGui::Begin("FreeTrial##FreeTrialWindow", &bFreeTrial_Window, ImGuiWindowFlags_None | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings);
			ImGui::Indent(10);
			ImGui::Text("");
			float fFontSize = ImGui::GetFontSize();
			int icon_size = ImGui::GetFontSize()*3.0;
			ImVec2 VIconSize = { (float)icon_size, (float)icon_size };
			bool bClickAlreadyHandled = false;
			if (ImGui::ImgBtn(TOOL_GOBACK, VIconSize, ImVec4(0, 0, 0, 0), drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
			{
				bClickAlreadyHandled = true;
				if (g_bFreeTrialNowExitsApp == true)
					PostQuitMessage(0);
				else
					bFreeTrial_Window = false;
			}
			if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0)
			{
				if (g_bFreeTrialNowExitsApp == true)
					ImGui::SetTooltip("%s", "Exit GameGuru MAX Free Trial Version");
				else
					ImGui::SetTooltip("%s", "Exit Free Trial Window");
			}
			ImGui::SameLine();

			ImVec2 VHeaderSize;
			VHeaderSize = { 730, 128 };
			ImVec2 pOldPos = ImGui::GetCursorPos();
			ImGui::SetCursorPos(pOldPos + ImVec2(78 - 18, -35));
			int iHeaderImgID = FREETRIAL_HEADER;
			if (g_iFreeTrialDaysLeft == 1) iHeaderImgID = FREETRIAL_COUNTER_ONEDAY;
			ImGui::ImgBtn(iHeaderImgID, VHeaderSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), 0, 0, 0, 0, false, false, false, false, false, false);
			ImVec2 pAfterPos = ImGui::GetCursorPos();

			// demo countdown numeric 
			ImGui::SetCursorPos(pOldPos + ImVec2(78+568, -4));
			ImGui::ImgBtn(FREETRIAL_COUNTER_BASE, ImVec2(50,64), ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), 0, 0, 0, 0, false, false, false, false, false, false);
			ImGui::SetCursorPos(pOldPos + ImVec2(78+338, -9));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
			ImGui::SetWindowFontScale(5.0f);
			// days left in free trial
			char pDays[32];
			sprintf(pDays, "%d", g_iFreeTrialDaysLeft);
			ImGui::TextCenter(pDays);
			ImGui::PopStyleColor();
			ImGui::SetCursorPos(pAfterPos);

			float fContentWidth = ImGui::GetContentRegionAvailWidth();
			float fLogoWidth = fContentWidth - 10;
			float fImageWidth = 460;
			float fImageHeight = 215;
			if (ImageExist(FREETRIAL_BODY))
			{
				fImageWidth = ImageWidth(FREETRIAL_BODY);
				fImageHeight = ImageHeight(FREETRIAL_BODY);
			}
			float fScale = fLogoWidth / fImageWidth;
			ImVec2 vLogoSize = { fLogoWidth , fImageHeight * fScale };
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, -20));
			#ifdef GGMAXEPIC
			if (ImGui::ImgBtn(FREETRIAL_BODY, vLogoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), 0, 0, 0, 0, false, false, false, false, false, false))
			{
				bClickAlreadyHandled = true;
				ExecuteFile("https://store.epicgames.com/", "", "", 0);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click here to go to the Epic Store to buy GameGuru MAX");
			#else
			if (ImGui::ImgBtn(FREETRIAL_BODY, vLogoSize, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1), 0, 0, 0, 0, false, false, false, false, false, false))
			{
				bClickAlreadyHandled = true;
				ExecuteFile("https://store.steampowered.com/app/1247290/GameGuru_MAX/", "", "", 0);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click here to go to the Steam page to buy GameGuru MAX");
			#endif

			ImGui::SetWindowFontScale(0.5f);
			ImGui::Text("");
			ImGui::SetWindowFontScale(2.8f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.00f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.5f, 0.0f, 1.00f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.1f, 1.00f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.5f, 0.0f, 1.00f));
			if (ImGui::StyleButton("Buy GameGuru MAX", ImVec2(vLogoSize.x, fFontSize*4.0)))
			{
				bClickAlreadyHandled = true;
				#ifdef GGMAXEPIC
				ExecuteFile("https://store.epicgames.com/", "", "", 0);
				#else
				#ifdef FREETRIALONDISCOUNT
				ExecuteFile("https://store.steampowered.com/bundle/25504/GameGuru_Twin_Pack/", "", "", 0);
				#else
				ExecuteFile("https://store.steampowered.com/app/1247290/GameGuru_MAX/", "", "", 0);
				#endif
				#endif
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();
			#ifdef GGMAXEPIC
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click here to go to the Epic Store to buy GameGuru MAX");
			#else
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Click here to go to the Steam page to buy GameGuru MAX");
			#endif
			ImGui::SetWindowFontScale(1.0);

			// completed free trial window
			ImGui::Indent(-10);
			bImGuiGotFocus = true;
			ImGui::End();

			// if any click not uised above, close dialog on general principal (clicke doutside of dialog most likely)
			ImGuiIO& io = ImGui::GetIO();
			if (io.MouseReleased[0] > 0 && bClickAlreadyHandled == false)
			{
				iCountingFreeDialogClicks++;
				if (iCountingFreeDialogClicks >= 2)
				{				
					bFreeTrial_Window = false;
				}
			}
		}
		if (bFreeTrial_Window == false)
		{
			iCountingFreeDialogClicks = 0; 
		}

		//#####################
		//#### Info Window ####
		//#####################
		
		iInfoUniqueId = 500001;
		if (refresh_gui_docking == 0) 
		{
			ImGui::SetNextWindowSize(ImVec2(46 * ImGui::GetFontSize(), 32 * ImGui::GetFontSize()), ImGuiCond_Once); //ImGuiCond_FirstUseEver
			ImGui::SetNextWindowPosCenter(ImGuiCond_Once);
			bool bTmp = true;
			ImGui::Begin("Information##InformationWindow", &bTmp, ImGuiWindowFlags_NoDocking);
			ImGui::End();
		}
		else if (bInfo_Window) {
			if (bInfo_Window_First_Run)
			{
				ImGui::SetNextWindowSize(ImVec2(46 * ImGui::GetFontSize(), 32 * ImGui::GetFontSize()), ImGuiCond_Once); //ImGuiCond_FirstUseEver
				ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
				bInfo_Window_First_Run = false;
			}
			if (bInfo_Reload || cInfoImageLast != cInfoImage )
			{
				//Load new image.
				//cInfoImage
				image_setlegacyimageloading(true);
				//  Load editor images
				SetMipmapNum(1); //PE: mipmaps not needed.
				if (GetImageExistEx(INFOIMAGE))
					DeleteImage(INFOIMAGE);
				LoadImage(cInfoImage.Get(), INFOIMAGE);
				if (!GetImageExistEx(INFOIMAGE))
				{
					//Get default information image.
					LoadImage("tutorialbank\\information-default.jpg", INFOIMAGE);
				}
				SetMipmapNum(-1);
				image_setlegacyimageloading(false);
				cInfoImageLast = cInfoImage;
			}
			ImGui::Begin("Information##InformationWindow", &bInfo_Window, ImGuiWindowFlags_NoDocking);

			if (GetImageExistEx(INFOIMAGE))
			{
				float fRegionWidth = ImGui::GetContentRegionAvailWidth();
				float img_w = ImageWidth(INFOIMAGE);
				float img_h = ImageHeight(INFOIMAGE);
				float fRatio = img_h / img_w;
#ifndef REMOVED_EARLYACCESS
				ImGui::ImgBtn(INFOIMAGE, ImVec2(fRegionWidth, fRegionWidth*fRatio), ImVec4(0.0, 0.0, 0.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(1.0, 1.0, 1.0, 1.0), 0, 0, 0, 0, false);
			}
			ImGui::TextWrapped(cInfoMessage.Get());
#else
			}

			char newLine[MAX_PATH];
			char textToDisplay[MAX_PATH];
			strcpy(textToDisplay, cInfoMessage.Get());

			ImGui::SetWindowFontScale(1.75f);
			ImVec2 windowSize = ImGui::GetWindowSize();
			ImVec2 textSize = ImGui::CalcTextSize(cInfoMessage.Get());

			int iTotalLinesAllowed = windowSize.y / textSize.y;
			iTotalLinesAllowed++;

			// Reduce window size in calculations for margins.
			windowSize.x *= 0.8f;
			
			int iNumberOfLines = textSize.x / windowSize.x;
			// Add another line to accomodate for anything after the decimal place.
			iNumberOfLines++;

			// Work out where cursor should be placed to have the text block centered.
 			ImGui::SetCursorPosY((iTotalLinesAllowed - iNumberOfLines) * 0.5f * textSize.y);

			int iOffset = 0;
			int iLength = cInfoMessage.Len();

			// Work out the target number of characters per line.
			int iIncrement = iLength / iNumberOfLines;
			iIncrement++;

			int iPreviousOffset = 0;
			
			// Split cInfoMessage to display centered text over multiple lines.
			for (int i = 0; i < iNumberOfLines; i++)
			{
				strcpy(newLine, cInfoMessage.Get() + iOffset);
				iPreviousOffset = iOffset;
				iOffset += iIncrement;

				// Find a suitable place to end the line.
				for (int j = iOffset; j < iLength; j++)
				{
					if (textToDisplay[j] == ' ')
					{
						if (textToDisplay[j + 1] == '.' || textToDisplay[j + 1] == ',')
							iOffset++;
						break;
					}
					else if (textToDisplay[j] == '.' || textToDisplay[j] == ',')
						break;
					else
						iOffset++;
				}
				newLine[iOffset - iPreviousOffset ] = 0;
				ImGui::TextCenter(newLine);
				
			}
		
			ImGui::SetWindowFontScale(1.0f);
#endif
			ImGui::End();
		}
		else
		{
			bInfo_Reload = true; //Reload on new run.
		}

		//###############
		//#### About ####
		//###############

			About_Screen();

		//##################
		//#### Importer ####
		//##################

		if (refresh_gui_docking == 0 && !bImporter_Window) 
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Importer##ImporterWindow", &bImporter_Window, iGenralWindowsFlags);
			ImGui::End();
		}

		imgui_importer_loop();

		//#########################
		//#### Help Menu Image ####
		//#########################
		static bool bReadyToProcessMouse = false;
		if(bHelp_Menu_Image_Window) {
				
			if (GetImageExistEx(HELPMENU_IMAGE)) {
				ImGui::OpenPopup("Help##HelpMenuImage");

				float img_w = ImageWidth(HELPMENU_IMAGE);
				float img_h = ImageHeight(HELPMENU_IMAGE);

				ImGui::SetNextWindowPosCenter(ImGuiCond_Always);

				if (ImGui::BeginPopupModal("Help##HelpMenuImage", &bHelp_Menu_Image_Window, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings )) { //ImGuiWindowFlags_AlwaysAutoResize
					//@Lee if you only want 1:1 pixel remove the below (but it could go outside windows).
					//@Lee if 1:1 is possible it will do it.
					if (img_w > viewPortSize.x || img_h > viewPortSize.y) {
						float fRatio = 1.0f / (img_w / img_h);
						img_w = viewPortSize.x;
						img_h = viewPortSize.x * fRatio;
						if (img_h > viewPortSize.y) {
							float fRatio = 1.0f / (img_h / img_w);
							img_h = viewPortSize.y;
							img_w = viewPortSize.y * fRatio;
						}
					}
					ImGui::ImgBtn(HELPMENU_IMAGE, ImVec2(img_w, img_h), ImVec4(0.0, 0.0, 0.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(1.0, 1.0, 1.0, 1.0), 0, 0, 0, 0, false);
					bImGuiGotFocus = true;
					ImGui::EndPopup();
				}

			}
			else {
				bHelp_Menu_Image_Window = false;
			}

			//Close no matter where is clicked.
			ImGuiIO& io = ImGui::GetIO();
			if (ImGui::IsKeyPressed(27)) {
				bHelp_Menu_Image_Window = false;
			}
			if (bReadyToProcessMouse && ImGui::IsMouseReleased(0) ) {
				bHelp_Menu_Image_Window = false;
			}
			if (io.MouseClicked[0] > 0) {
				bReadyToProcessMouse = true; //next frame
			}
		}
		else {
			bReadyToProcessMouse = false;
		}

		//###########################
		//#### Export Standalone ####
		//###########################
		static bool bModalInformation = false;
		if (g_bFreeTrialVersion == true)
		{
			if (bExport_Standalone_Window == true)
			{
				bFreeTrial_Window = true;
				bExport_Standalone_Window = false;
			}
		}
		if (bExport_Standalone_Window) 
		{
			static char cStandalonePath[MAX_PATH] = "\0";
			static int iStandaloneCycle = 0;
			if (cStandalonePath[0] == 0) 
			{
				g.exedir_s = g.myownrootdir_s;
				if(cstr(Right(g.myownrootdir_s.Get(), 1)) == "\\" )
					g.exedir_s += "My Games\\";
				else
					g.exedir_s += "\\My Games\\";
				strcpy(cStandalonePath, g.exedir_s.Get());

				if (strlen(pref.cDefaultStandalonePath) > 0)
				{
					strcpy(cStandalonePath,pref.cDefaultStandalonePath);
				}
			}

			ImGui::OpenPopup("Export: Save Standalone Game##SaveStandaloneWindow");
			ImGui::SetNextWindowSize(ImVec2(43 * ImGui::GetFontSize(), 32 * ImGui::GetFontSize()), ImGuiCond_Once);
			ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);// ImGuiCond_Once);
			if (ImGui::BeginPopupModal("Export: Save Standalone Game##SaveStandaloneWindow", &bExport_Standalone_Window, 0)) 
			{
				ImGui::Indent(10);
				float col_start = 80.0f;
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));

				ImGui::SetWindowFontScale(1.2);
				ImGui::TextWrapped("You can export your game project as a self-contained standalone game that can then be shared and played without the need for GameGuru MAX.");

				ImGui::TextWrapped("When your game has been saved as a standalone game, Windows File Explorer can be used to open your standalone game folder and you can then run your game from that location. You can also zip up the whole folder if you wish to share or sell your game.");
				ImGui::Text("");

				// VR mode
				if (g.gvrmode != 0)
				{
					if (ImGui::Checkbox("Save with Experimental Virtual Reality Mode##setVRModeEnabled", &g_bStandaloneVRMode))
					{
						// VR Mode changed
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", "Setting your standalone to VR mode will attempt to launch the game on any supported OpenXR device");
					}
				}

				// Option to open folder automatically after saving standalone
				static bool bOpenFolder = false;
				ImGui::Checkbox("Open folder after game has been saved and close GameGuru MAX", &bOpenFolder);
				ImGui::Text("");
				ImGui::SetWindowFontScale(1.0);

				// Save or Cancel button
				ImGui::Indent(-10);
				float save_gadget_size = ImGui::GetFontSize()*16.0;
				float w = ImGui::GetWindowContentRegionWidth();
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (save_gadget_size*0.5), 0.0f));
				ImGui::SetWindowFontScale(1.4);
				if (iStandaloneCycle == 0) 
				{
					extern float g_mapfile_fProgress;
					g_mapfile_fProgress = 0.0f;
					if (ImGui::StyleButton("SAVE STANDALONE", ImVec2(save_gadget_size, 0))) {
						g.exedir_s = cStandalonePath;
						iStandaloneCycle = 1;
					}
				}
				else 
				{
					if (ImGui::StyleButton("CANCEL", ImVec2(save_gadget_size, 0))) {
						iStandaloneCycle = 5;
					}
				}
				ImGui::SetWindowFontScale(1.0);

				ImGui::Text("");

				ImGui::PushID(iInfoUniqueId++);
				if (ImGui::ImgBtn(ICON_INFO, ImVec2(20, 20), ImColor(0, 0, 0, 0), ImColor(220, 220, 220, 220), ImColor(255, 255, 255, 255), ImColor(180, 180, 160, 255), -1, 0, 0, 0, false, false, false, false, false)) //, bBoostIconColors
				{
					//PE: We are modal here, so need the special modal information window.
					cInfoMessage = "Games made with GameGuru MAX are designed for PCs with this minimum specification:\n\nWindows 10\nIntel Dual-Core 2GHz or AMD Dual-Core 2GHz CPU\n8 GB Ram\nNVIDIA GeForce GTX960 or similar\nDirectX 11\n30 GB device storage\nDirectX Compatabible Sound Card";
					bModalInformation = true;
					cInfoImage = "";
				}
				ImGui::PopID();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", "Click For More Information");
				}
				ImGui::SameLine();
				ImGui::TextWrapped("Ensure that anyone you share your standalone game with has a PC system that meets the minimum requirements of GameGuru MAX.");
				ImGui::Text("");

				ImGui::Indent(10);

				//New settings here!
				if (iStandaloneCycle == 0)
				{
					ImGui::SetWindowFontScale(1.2);
					ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
					ImGui::Text("Choose where you would like your standalone to be saved:");
					ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
					ImGui::Text("Path");
					ImGui::SameLine();
					ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
					ImGui::SetCursorPos(ImVec2(col_start, ImGui::GetCursorPosY()));

					float path_gadget_size = ImGui::GetFontSize()*2.0;

					ImGui::PushItemWidth(-10 - path_gadget_size);
					ImGui::InputText("##InputPathCCP", &cStandalonePath[0], 250, ImGuiInputTextFlags_ReadOnly);
					if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;
					ImGui::PopItemWidth();

					ImGui::SameLine();
					ImGui::PushItemWidth(path_gadget_size);
					if (ImGui::StyleButton("...##ccppath")) 
					{
						cStr tOldDir = GetDir();
						char * cFileSelected;
						cstr fulldir = cStandalonePath;

						cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_DIR, "All\0*.*\0", fulldir.Get(), NULL);

						SetDir(tOldDir.Get());

						if (cFileSelected && strlen(cFileSelected) > 0) 
						{
							strcpy(cStandalonePath, cFileSelected);
							if (cStandalonePath[strlen(cStandalonePath) - 1] != '\\')
								strcat(cStandalonePath, "\\");
							strcpy(pref.cDefaultStandalonePath, cStandalonePath);
						}
					}
					ImGui::PopItemWidth();
					ImGui::SetWindowFontScale(1.0);
				}

				float fdone = (float)mapfile_savestandalone_getprogress() / 100.0f;
				if (iStandaloneCycle == 1)
				{
					// check if destination export path exists
					if (PathExist(cStandalonePath) == false)
					{
						iStandaloneCycle = 0;
						strcpy(cTriggerMessage, "Save Standalone Aborted - Destination Path Not Exist");
						bTriggerMessage = true;
						bExport_Standalone_Window = false; //Close window.
					}
					else
					{
						// can proceed
						fdone = 0.01f;
					}
				}

				if (fdone > 0.0f) 
				{
					ImGui::SetWindowFontScale(1.2);
					char tmp[32];
					sprintf(tmp, "Progress: %.0f%%", fdone*100.0f);
					ImGui::ProgressBar(fdone, ImVec2(ImGui::GetContentRegionAvail().x - 10, 28), tmp);
					ImGui::SetWindowFontScale(1.0);

					// log save standalone progress
					char pSaveStandaloneLog[256];
					sprintf(pSaveStandaloneLog, "Save Standalone %d : %s", iStandaloneCycle, tmp);
					timestampactivity(0, pSaveStandaloneLog);
				}

				if (iStandaloneCycle == 2)
				{
					// start save standalone creation
					mapfile_savestandalone_start();
					iStandaloneCycle = 3;
				}
				if (iStandaloneCycle == 3)
				{
					// run standalone creation calls
					if (mapfile_savestandalone_continue() == 1)
					{
						// complete standalone creation
						iStandaloneCycle = 4;
					}
				}
				if (iStandaloneCycle == 4)
				{
					// complete standalone creation
					mapfile_savestandalone_finish();
					iStandaloneCycle = 0;
					strcpy(cTriggerMessage, "Save Standalone Done");
					bTriggerMessage = true;
					bExport_Standalone_Window = false; //Close window.				

					void InjectIconToExe(char *icon, char *exe, int intresourcenumber);
					char projectico[MAX_PATH];
					char projectfinal_ico[MAX_PATH];
					strcpy(projectico, "projectbank\\");
					strcat(projectico, Storyboard.gamename);

					strcpy(projectico, projectico);
					strcat(projectico, "\\project256.ico");
					GG_GetRealPath(projectico, 1);
					if (FileExist(projectico))
					{
						t.dest_s = t.exepath_s + t.exename_s + "\\" + t.exename_s + ".exe";
						InjectIconToExe(projectico, t.dest_s.Get(), 1);
						HINSTANCE hinstance = ShellExecuteA(NULL, "open", "ie4uinit.exe", "-show", "", SW_SHOWDEFAULT);
						Sleep(100); //PE: Let it update
					}
					if (bOpenFolder)
					{
						cstr open_folder = cStandalonePath;
						if (g.bUseStoryBoardSetup)
						{
							//Use project name as exename
							if (strlen(Storyboard.gamename) > 0)
							{
								open_folder = open_folder + cstr(Storyboard.gamename) + "\\";
							}
						}
						HINSTANCE hinstance = ShellExecuteA(NULL, "open", open_folder.Get(), "", "", SW_SHOWDEFAULT);
						g_bCascadeQuitFlag = true;
					}
				}
				if (iStandaloneCycle == 5)
				{
					// cancel standalone creation
					mapfile_savestandalone_restoreandclose();
					iStandaloneCycle = 0;
					strcpy(cTriggerMessage, "Save Standalone Cancelled");
					bTriggerMessage = true;
					bExport_Standalone_Window = false; //Close window.
				}
				if (iStandaloneCycle == 1) iStandaloneCycle = 2;

				ImGui::Indent(-10);

				bImGuiGotFocus = true;

				//#######################################################
				//#### Modal Popup Information , must do it this way ####
				//#######################################################

				if (bModalInformation)
				{
					ImGui::OpenPopup("Information##modalinformationwindow");

					ImGui::SetNextWindowSize(ImVec2(46 * ImGui::GetFontSize(), 28 * ImGui::GetFontSize()), ImGuiCond_Once);
					ImGui::SetNextWindowPosCenter(ImGuiCond_Appearing);// ImGuiCond_Once);
					if (ImGui::BeginPopupModal("Information##modalinformationwindow", &bModalInformation, 0))
					{
						ImGui::Indent(10);
						ImGui::SetWindowFontScale(1.75f);
						ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, 4));
						ImGui::TextWrapped(cInfoMessage.Get());
						ImGui::SetWindowFontScale(1.0f);
						ImGui::Indent(-10);
						ImGui::EndPopup();
					}
				}

				ImGui::EndPopup();
			}
		}

		wi::profiler::EndRange(clSeg); clSeg = wi::profiler::BeginRangeCPU("CL-ObjTools+Settings");
		//######################
		//#### Object Tools ####
		//######################
		bool bIsLightProbe = false;
		g_selected_editor_object = NULL;
		g_selected_editor_objectID = 0;
		if (t.widget.pickedObject > 0) 
		{
			if (t.widget.pickedObject < g_iObjectListCount)
			{
				if (g_ObjectList[t.widget.pickedObject])
				{
					if (t.widget.pickedEntityIndex > 0) {

						if (t.entityelement[t.widget.pickedEntityIndex].staticflag)
							g_selected_editor_color = XMSTATICCOLOR;
						else
							g_selected_editor_color = XMDYNAMICCOLOR;
					}
					g_selected_editor_object = g_ObjectList[t.widget.pickedObject];
					g_selected_editor_objectID = t.widget.pickedObject;
				}
			}
		}

		static sObject* g_last_selected_editor_object = NULL;
		if (g_selected_editor_object && g_last_selected_editor_object != g_selected_editor_object)
		{
			g_last_selected_editor_object = g_selected_editor_object;
			ImGui::SetWindowFocus("Object Tools##EntityToolsWindow");

			// Check if we no longer have a group selected and then switch back to the " Current Objects" tab if so
			if (current_selected_group >= 0)
			{
				bool bIsNewObjectInGroup = false;

				for (int j = 0; j < MAXGROUPSLISTS; j++)
				{
					for (int i = 0; i < vEntityGroupList[j].size(); i++)
					{
						if (t.widget.pickedObject == vEntityGroupList[j].at(i).e)
						{
							bIsNewObjectInGroup = true;
							break;
						}
					}
				}
				if (!bIsNewObjectInGroup)
					i_switch_group_tab = 1;
				else
					i_switch_group_tab = 2;
			}
			else
			{
				i_switch_group_tab = 1;
			}
		}

		if (refresh_gui_docking == 0 && !Entity_Tools_Window) 
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Object Tools##EntityToolsWindow", &Entity_Tools_Window, iGenralWindowsFlags);
			ImGui::End();
		}
		else 
		{
			int iEntityIndex = t.widget.pickedEntityIndex;
			int iActiveObj = t.widget.activeObject;
			bool bUpdateGrideleprof = false;
			if (t.gridentityextractedindex > 0)
			{
				iEntityIndex = t.gridentityextractedindex;
				bUpdateGrideleprof = true;
				if (t.gridentityobj > 0)
					iActiveObj = t.gridentityobj;
			}
			else
			{
				iEntityIndex = t.widget.pickedEntityIndex;
				if (t.widget.activeObject == 0 && t.widget.pickedEntityIndex < t.entityelement.size() )
				{
					if(t.widget.pickedEntityIndex > 0)
						iActiveObj = t.tentityobj = t.entityelement[t.widget.pickedEntityIndex].obj;
					else
					{
						if (1==2 && t.tentitytoselect > 0)
						{
							//Temp enable widget , reset is set in next run of gridedit_mapediting();
							iEntityIndex = t.tentitytoselect;
							t.widget.activeObject = t.entityelement[t.tentitytoselect].obj;
							if (!ObjectExist(t.widget.activeObject))
							{
								t.widget.activeObject = 0;
							}
						}
						iActiveObj = t.widget.activeObject;
					}
				}
			}
			active_tools_obj = iActiveObj;
			active_tools_entity_index = iEntityIndex;
			//PE: Make sure the object we edit are a clone , fix many of the problems where material changes was also going to the master object.
			if (iEntityIndex > 0 && iEntityIndex == t.widget.pickedEntityIndex && iActiveObj > 0)
			{
				sObject* pObject = g_ObjectList[iActiveObj];
				if (pObject)
				{
					for (int iMesh = 0; iMesh < (int)pObject->iMeshCount; iMesh++)
					{
						sMesh* pMesh = pObject->ppMeshList[iMesh];
						if (pMesh)
						{
							if (pMesh->master_wickedmeshindex > 0)
							{
								//PE: Make sure selected object is a clone.
								t.tupdatee = iEntityIndex;
								entity_updateentityobj();
							}
							//PE: only checking first mesh needed.
							break;
						}
					}
				}
			}

			static int iLastActiveEntityIndex = -1, iLastActiveObj = -1;

			if (Entity_Tools_Window && ( current_mode == TOOL_ENTITY || current_mode == TOOL_MARKERS || (t.gridentity > 0 && t.entityprofile[t.gridentity].isebe != 0)  )) 
			{
				if (!g_selected_editor_object && iActiveObj > 0)
				{
					if (g_ObjectList[iActiveObj])
					{
						if (iEntityIndex > 0) {

							if (t.entityelement[iEntityIndex].staticflag)
								g_selected_editor_color = XMSTATICCOLOR;
							else
								g_selected_editor_color = XMDYNAMICCOLOR;
						}
						g_selected_editor_object = g_ObjectList[iActiveObj];
						g_selected_editor_objectID = iActiveObj;
					}
				}

				int iMasterID = t.entityelement[iEntityIndex].bankindex;
				if (bDraggingActive && t.widget.pickedEntityIndex > 0 && t.gridentity > 0)
				{
					//PE: Keep displaying old info, while dragging a gridentity around.
					iMasterID = t.gridentity;
				}

				// determine if a provbe or not
				if(iEntityIndex>0)
					if (t.entityelement[iEntityIndex].eleprof.light.fLightHasProbe >= 50.0f)
						bIsLightProbe = true;

				// detect ANY change inside entityelement (inc eleprof) so can trigger instance cloing of collectables
				bool bSnappedEntityElementCopy = false;
				static entityeleproftype snapshotentityelement;
				if (iEntityIndex > 0)
				{
					memcpy(&snapshotentityelement, &t.entityelement[iEntityIndex].eleprof, sizeof(entityeleproftype));
					bSnappedEntityElementCopy = true;
				}

				bool bWithNoScrollbar = false;
				if (iEntityIndex > 0 && iMasterID > 0 && iActiveObj > 0 && ObjectExist(iActiveObj) && pref.iEnableIdentityProperties )
				{
					ImGui::Begin("Object Tools##EntityToolsWindow", &Entity_Tools_Window, iGenralWindowsFlags | ImGuiWindowFlags_NoScrollbar);
					bWithNoScrollbar = true;
				}
				else
				{
					ImGui::Begin("Object Tools##EntityToolsWindow", &Entity_Tools_Window, iGenralWindowsFlags);
				}

				bool bRunExtractDuplicate = false;
				bool bDuplicate = false;
				bool bChildWindowOpen = false;
				bool bClickedTheLockUnlockButton = false;

				if (iActiveObj > 0)
				{
					bool bIsEBEWidget = false;
					int iEntID = 0;
					if (iEntityIndex > 0)
					{
						iEntID = t.entityelement[iEntityIndex].bankindex;
						if (iEntID > 0)
							if (t.entityprofile[iEntID].isebe != 0)
								bIsEBEWidget = true;
					}
					// rubber band or selected parent
					bool bRealRubberBand = false;
					int iEntityInGroupList = -1;
					if (g.entityrubberbandlist.size() > 0)
					{
						bRealRubberBand = true;
						if (iEntityIndex > 0)
						{
							bool bPartOfParentChildGroup = false;
							editor_rec_checkifindexinparentchain(iEntityIndex, &bPartOfParentChildGroup);
							if (bPartOfParentChildGroup == true)
								bRealRubberBand = false;
						}
					}
					else
					{
						iEntityInGroupList = isEntityInGroupList(iEntityIndex);
					}
					
					bool bToolPosition = false; // widgetPOSObj;
					bool bToolRotation = false; // widgetROTObj;
					bool bToolScale = false; // widgetSCLObj;
					bool bToolProperties = false; // widgetPRPObj;
					bool bToolExtract = false; // widgetDUPObj;
					bool bToolDelete = false; // widgetDELObj;
					bool bToolLock = false; // widgetLCKObj;
					bool bToolEdit = false;
					bool bToolSave = false;
					bool bToolDublicate = false; // 
					bool bToolFindFloor = true; //always on.,

					// show all or just POS
					if (bRealRubberBand == true)
					{
						// Rubber band select POS, DELETE and LOCK only
						bToolPosition = true;
						bToolRotation = true;
						bToolScale = true;
						bToolDelete = true;
						bToolLock = true;

						bToolFindFloor = false;
					}
					else
					{
						// POS, ROT, SCALE, etc
						bToolPosition = true;
						bToolRotation = true;
						bToolScale = true;
						bToolProperties = true;
						bToolExtract = true;
						bToolDelete = true;
						bToolLock = true;

						bToolDublicate = true;

						// hide if EBE widget
						if (bIsEBEWidget == true)
						{
							bToolProperties = false;
							bToolScale = false;
							bToolEdit = true;
						}
					}

					//  hide any buttons and widgets if entity is a 'waypoint zone type'
					t.ttte = iEntityIndex;
					if (t.ttte > 0)
					{
						t.tttwi = t.entityelement[t.ttte].eleprof.trigger.waypointzoneindex;
						if (t.tttwi > 0)
						{
							bToolRotation = false;
							bToolScale = false;
							bToolDublicate = false;
						}
						else if (iEntID > 0)
						{
							if (t.entityprofile[iEntID].islightmarker == 1 || t.entityprofile[iEntID].ischaracter == 1)
							{
								// regular light or character
								bToolScale = false;

								// allow probes to have rotation
								if (bIsLightProbe==true)//t.entityprofile[iEntID].ismarker == 2 && t.entityelement[t.ttte].eleprof.light.fLightHasProbe >= 50.0f)
								{
									bToolRotation = true;
								}
							}
							else if (t.entityprofile[iEntID].ismarker > 0)
							{
								// but allow Particles to have rotation and scale control
								if (t.entityprofile[iEntID].ismarker != 10 )
								{
									bToolRotation = false;
									bToolScale = false;

									// allow the player start marker to be rotated.
									if (t.entityprofile[iEntID].ismarker == 1)
										bToolRotation = true;
								}
							}
						}
					}

					//#############################
					//#### PE: New properties. ####
					//#############################

					int iObject = t.entityelement[iEntityIndex].obj;
					int media_icon_size = 64;
					sObject* pObject = g_ObjectList[iActiveObj];
					int iCollectionQuestIndex = -1;

					if (iEntityIndex > 0 && iMasterID > 0 && ObjectExist(iActiveObj))
					{
						grideleprof_uniqui_id = 35000;

						if (iLastActiveEntityIndex != iEntityIndex || iLastActiveObj != iActiveObj)
						{
							if (iLastActiveObj != 70000 && iLastActiveEntityIndex > 0)
							{
								if (iLastActiveEntityIndex < t.entityelement.size())
								{
									if (t.entityelement[iLastActiveEntityIndex].obj == iLastActiveObj)
									{
										//PE: InstanceObject - convert old one that we edited to a instance id possible.
										t.tupdatee = iLastActiveEntityIndex;
										entity_updateentityobj();
									}
								}
							}
							fpe_current_loaded_script = -1; //Make sure dlua is loaded in next call to DisplayFPEBehavior.
							iLastActiveEntityIndex = iEntityIndex;
							iLastActiveObj = iActiveObj;
							// Ensure that the rotation values are updated when switching objects
							g_bRefreshRotationValuesFromObjectOnce = true;
							g_bRefreshScaleValuesFromObjectOnce = true;
						}
						imgui_set_openproperty_flags(iMasterID);
						if (pref.iEnableIdentityProperties)
						{
							bChildWindowOpen = true;

							if (ImGui::StyleCollapsingHeader("Identity##2", ImGuiTreeNodeFlags_DefaultOpen)) //ImGuiTreeNodeFlags_None
							{
								//Display icon.
								if (pref.iObjectEnableAdvanced == 2)
								{
									// no icon when in compact mode
								}
								else
								{
									if (t.entityprofile[iMasterID].iThumbnailSmall > 0)
									{
										float w = ImGui::GetContentRegionAvailWidth();
										float fRatio = (float)ImageWidth(t.entityprofile[iMasterID].iThumbnailLarge) / (float)ImageHeight(t.entityprofile[iMasterID].iThumbnailLarge);
										if (ImageExist(t.entityprofile[iMasterID].iThumbnailLarge) && fRatio > 1.0)
										{
											float fwidth = media_icon_size * fRatio;
											ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (fwidth*0.5), 0.0f));
											ImGui::ImgBtn(t.entityprofile[iMasterID].iThumbnailLarge, ImVec2(fwidth, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, false);
										}
										else
										{
											ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
											ImGui::ImgBtn(t.entityprofile[iMasterID].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
										}
									}
								}

								// special color change when object is a collectable
								bool bObjectIsACollectableAndReadOnlyName = false;
								LPSTR pDescTooltip = t.strarr_s[204].Get();
								bool isProjectGlobal = t.entityelement[iEntityIndex].eleprof.isProjectGlobal;
								if (t.entityelement[iEntityIndex].eleprof.iscollectable != 0 || iCollectionQuestIndex > 0 || isProjectGlobal)
								{
									bObjectIsACollectableAndReadOnlyName = true;
									ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
									if (isProjectGlobal)
										pDescTooltip = "This object has been set as a project global object";
									else if(iCollectionQuestIndex>0)
										pDescTooltip = "This object has been set as a quest giver";
									else
										pDescTooltip = "This object has been set as an item collectable";
								}
								else
								{
									ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
								}

								if (bIsLightProbe == false)
								{
									ImGui::Indent(10);
									t.entityelement[iEntityIndex].eleprof.name_s = imgui_setpropertystring2_v2(t.group, t.entityelement[iEntityIndex].eleprof.name_s.Get(), "", pDescTooltip, bObjectIsACollectableAndReadOnlyName);
									ImGui::Indent(-10);
								}
								ImGui::PopStyleColor();
								ImGui::Separator();
							}
						}
					}

					//PE: Add a child here!
					if (bWithNoScrollbar)
					{
						ImGui::BeginChild("##objectpropertieswithnoscrollbar", ImVec2(0, 0), false, iGenralWindowsFlags);
					}

					static int iLastEntityIndex = -1;
					if (iEntityIndex != iLastEntityIndex)
					{
						iLastEntityIndex = iEntityIndex;
						ImGui::SetScrollY(0);
					}


					//##########################
					//#### Entity Transform ####
					//##########################

					bool bReadOnlyMode = false;
					if (t.entityelement[iEntityIndex].editorlock == 1)
						bReadOnlyMode = true;

					if (bToolPosition || bToolRotation || bToolScale)
					{
						// Object tools headers range from 14-20.
						// Default to the positioning header when selecting an object for the first time.
						if (g_selected_editor_object && g_last_selected_editor_object != g_selected_editor_object && (iLastOpenHeader < 13 || (iLastOpenHeader > 18 && iLastOpenHeader != 28)))
							iLastOpenHeader = 14;

						if (pref.bAutoClosePropertySections && iLastOpenHeader != 14)
							ImGui::SetNextItemOpen(false, ImGuiCond_Always);

						if (pref.iObjectEnableAdvanced == 2)
							ImGui::SetNextItemOpen(true, ImGuiCond_Always);

						if (ImGui::StyleCollapsingHeader("Positioning, Rotating and Scaling", ImGuiTreeNodeFlags_DefaultOpen)) //ImGuiTreeNodeFlags_DefaultOpen
						{
							// header prep
							if (pref.iObjectEnableAdvanced == 2)
							{
								// compact mode allows another component to be regarded as last opened header - keeps posrotscl open
							}
							else
							{
								iLastOpenHeader = 14;
							}
							float w = ImGui::GetWindowContentRegionWidth() - 30.0f;

							// if change these, update the object
							bool bUpdatePosition = false;
							bool bUpdateRoataion = false;
							bool bUpdateScale = false;
							float fPos[3], fScale[3];
							static float fAngle[3];
							static float fOldAngle[3];
							static float fScaleOrg[3];
							static int iLastPickedEntID = -1;
							static float fScaleMul = 100.0f;
							float fOldActiveObjectSX,fOldActiveObjectSY,fOldActiveObjectSZ;

							// Need these buttons for switching the widget type (when its on).
							ImGui::Indent(10);
							if (!pref.iEnableDragDropEntityMode || pref.iEnableDragDropWidgetSelect)
							{
								// widget mode 
								ImGui::TextCenter("Widget Mode");
								float fFontSize = ImGui::GetFontSize();
								int iLockButton = 0; if (pref.iObjectEnableAdvanced == 2) iLockButton = 1;
								float fButtonSize = (w-10) / ((int)bToolPosition + (int)bToolRotation + (int)bToolScale + iLockButton);
								if (bToolPosition)
								{
									ImGui::PushItemWidth(fButtonSize);
									bool bSelected = (t.widget.mode == 0);
									if (bSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);											
									if (ImGui::StyleButton("Position", ImVec2(fButtonSize, 0)))
									{
										t.widget.mode = 0;
										widget_show_widget();
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Position (F2)");							
									ImGui::PopItemWidth();
									if (bSelected) ImGui::PopStyleColor();
								}
								if (bToolRotation)
								{
									ImGui::SameLine();
									ImGui::PushItemWidth(fButtonSize);							
									bool bSelected = (t.widget.mode == 1);
									if (bSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);							
									if (ImGui::StyleButton("Rotation", ImVec2(fButtonSize, 0)))
									{
										t.widget.mode = 1;
										widget_show_widget();
										g_bRefreshRotationValuesFromObjectOnce = true;
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Rotation (F3)");
									ImGui::PopItemWidth();							
									if (bSelected) ImGui::PopStyleColor();
								}
								if (bToolScale)
								{
									ImGui::SameLine();
									ImGui::PushItemWidth(fButtonSize);							
									bool bSelected = (t.widget.mode == 2);
									if (bSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);						
									if (ImGui::StyleButton("Scale", ImVec2(fButtonSize, 0)))
									{
										t.widget.mode = 2;
										widget_show_widget();
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Scale (F4)");						
									ImGui::PopItemWidth();
									if (bSelected) ImGui::PopStyleColor();
								}
								if (iLockButton)
								{
									ImGui::SameLine();
									ImGui::PushItemWidth(fButtonSize);
									bool bSelected = false;
									if (bSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
									LPSTR pLockButtonText = "Lock";
									if (g.entityrubberbandlist.size() == 0 && iEntityIndex > 0 && t.entityelement[iEntityIndex].editorlock == 1) pLockButtonText = "Unlock";
									if (ImGui::StyleButton(pLockButtonText, ImVec2(fButtonSize, 0)))
									{
										bClickedTheLockUnlockButton = true;
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Lock/Unlock Object");
									ImGui::PopItemWidth();
									if (bSelected) ImGui::PopStyleColor();
								}
							}

							if (iEntityIndex != iLastPickedEntID)
							{
								iLastPickedEntID = iEntityIndex;
								fScaleMul = 100.0f;
								fScaleOrg[0] = ObjectScaleX(iActiveObj);
								fScaleOrg[1] = ObjectScaleY(iActiveObj);
								fScaleOrg[2] = ObjectScaleZ(iActiveObj);
							}

							fPos[0] = ObjectPositionX(iActiveObj);
							fPos[1] = ObjectPositionY(iActiveObj);
							fPos[2] = ObjectPositionZ(iActiveObj);
							if (g_bRefreshRotationValuesFromObjectOnce == true)
							{
								// should only snapshot eulers once as otherwise they
								// mess up when pulling from newer quaternion method
								g_bRefreshRotationValuesFromObjectOnce = false;
								if (t.entityprofile[iMasterID].ischaracter == 1)
								{
									// quats are the true rotations of objects, but refresh euler for characters to ONLY use the Y axis
									entity_calculateeuleryfromquat(iEntityIndex);

									// characters are simpler, just the Y angle from the entity properties
									fOldAngle[0] = 0;
									fOldAngle[1] = t.entityelement[iEntityIndex].ry;
									fOldAngle[2] = 0;
									fAngle[0] = 0;
									fAngle[1] = t.entityelement[iEntityIndex].ry;
									fAngle[2] = 0;
								}
								else
								{
									// non characters
									fOldAngle[0] = ObjectAngleX(iActiveObj);
									fOldAngle[1] = ObjectAngleY(iActiveObj);
									fOldAngle[2] = ObjectAngleZ(iActiveObj);
									fAngle[0] = ObjectAngleX(iActiveObj);
									fAngle[1] = ObjectAngleY(iActiveObj);
									fAngle[2] = ObjectAngleZ(iActiveObj);

									// rotations (such as seccam) can be widget rotated JUST on the Y, and we can detect this and use Y-only euler!
									// quats are the true rotations of objects, but refresh euler for characters to ONLY use the Y axis
									if ((fAngle[0] == 180 && fAngle[2] == 180) || (fAngle[0] == -180 && fAngle[2] == -180))
									{
										// so, 100 degrees is 80,180,180 can be converted to 100,0,0
										entity_calculateeuleryfromquat(iEntityIndex);
										fOldAngle[0] = 0;
										fOldAngle[1] = t.entityelement[iEntityIndex].ry;
										fOldAngle[2] = 0;
										fAngle[0] = 0;
										fAngle[1] = t.entityelement[iEntityIndex].ry;
										fAngle[2] = 0;
									}
								}
							}
							ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImVec4(0, 0, 0, 0));
							float fPushItemWidth = -2.0;
							ImGui::Indent(-10);

							// store for rubber band position
							float fOldActiveObjectRX = ObjectAngleX(iActiveObj);
							float fOldActiveObjectRY = ObjectAngleY(iActiveObj);
							float fOldActiveObjectRZ = ObjectAngleZ(iActiveObj);

							// LB: only need to show smart buttons if not showing widget
							if (pref.iEnableDragDropWidgetSelect == 0)
							{
								// title for three position modes 
								if (pref.iEnableDragDropEntityMode && pref.iObjectEnableAdvanced != 2)
								{
									LPSTR pEditPositionTitle = "Smart Mode";
									if (iObjectMoveMode == 0) pEditPositionTitle = "Smart Mode - Horizontal Only";
									if (iObjectMoveMode == 1) pEditPositionTitle = "Smart Mode - Vertical Only";
									if (iObjectMoveMode == 2) pEditPositionTitle = "Smart Mode";
									ImGui::TextCenter(pEditPositionTitle);
								}

								// LB: Is not a HOLD action, it must be a single press toggle
								static bool bReadyToChange = true;
								bool bPressTAB = t.inputsys.keytab == 1;
								if (!bPressTAB) bReadyToChange = true;
								if (bReadyToChange && bPressTAB)
								{
									if (t.inputsys.keyshift == 1)
									{
										iObjectMoveMode--;
										if (iObjectMoveMode < 0 || iObjectMoveMode > 2)
											iObjectMoveMode = 2;
									}
									else
									{
										iObjectMoveMode++;
										if (iObjectMoveMode > 2 || iObjectMoveMode < 0)
											iObjectMoveMode = 0;
									}
									bReadyToChange = false; //toggle, wait until tab is released again.
								}

								// center buttons
								ImVec2 padding = { 3.0, 3.0 };
								w = ImGui::GetContentRegionAvail().x - 10.0f;
								float icon_spacer = 10.0f;
								int max_icon_size = 56;
								int control_image_size = 26; //lowest possible icon size.
								float center_icons_numbers = 3.0f;
								if (pref.iObjectEnableAdvanced == 2)
								{
									// compact mode for positioning buttons
									center_icons_numbers = 6.0f;
									icon_spacer = 5.0f;
									max_icon_size = 28.0f;
									control_image_size = 13.0f;
								}
								float control_width = (control_image_size + 3.0) * center_icons_numbers + 6.0;
								int indent = 10;
								if (w > control_width)
								{
									//PE: fit perfectly with window width.
									control_image_size = (w - 20.0) / center_icons_numbers;
									control_image_size -= 4.0; //Padding.
									if (control_image_size > max_icon_size) control_image_size = max_icon_size;
									control_width = (control_image_size + 3.0) * center_icons_numbers + 6.0;
									if (control_image_size == max_icon_size)
									{
										indent = (w*0.5) - (control_width*0.5);
										if (indent < 10)
											indent = 10;
									}
								}
								else
								{
									indent = (w*0.5) - (control_width*0.5);
									if (indent < 10)
										indent = 10;
								}
								if (pref.iObjectEnableAdvanced == 2)
								{
									// smaller buttons dont need too much indent
									indent -= 10;
								}

								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, 4.0f));

								if (pref.iEnableDragDropEntityMode)
								{
									ImGui::Indent(indent);

									if (iObjectMoveMode == 2)
									{
										const ImRect image_bb((ImGui::GetCurrentWindow()->DC.CursorPos - padding), ImGui::GetCurrentWindow()->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
										ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
									}
									if (ImGui::ImgBtn(OBJECT_MOVE_SURFACESCAN, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors)) {
										iObjectMoveMode = 2;
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move object and optionally find surface and orientation");

									ImGui::SameLine();
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(icon_spacer, 0.0f));

									if (iObjectMoveMode == 0)
									{
										const ImRect image_bb((ImGui::GetCurrentWindow()->DC.CursorPos - padding), ImGui::GetCurrentWindow()->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
										ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
									}
									if (ImGui::ImgBtn(OBJECT_MOVE_XZ, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
									{
										iObjectMoveMode = 0;
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move object horizontally, keeping current Y coordinate");

									ImGui::SameLine();
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(icon_spacer, 0.0f));

									if (iObjectMoveMode == 1)
									{
										const ImRect image_bb((ImGui::GetCurrentWindow()->DC.CursorPos - padding), ImGui::GetCurrentWindow()->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
										ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
									}
									if (ImGui::ImgBtn(OBJECT_MOVE_Y, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors)) {
										iObjectMoveMode = 1;
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move object vertically, keeping current X and Z coordinates");

									// title for stack and lock buttons
									LPSTR pEditPositionTitle = "Stack, Orientation, Lock";
									if(pref.iObjectEnableAdvanced != 2) ImGui::TextCenter(pEditPositionTitle);

									// find center of buttons for second row (repeat code from above - urg)
									if (pref.iObjectEnableAdvanced == 2)
									{
										// tag second row ofr icons to end of first for compact choices
										ImGui::SameLine();
									}
									else
									{
										ImGui::Indent(-(indent));
										center_icons_numbers = 3.0f;
										control_width = (control_image_size + 3.0) * center_icons_numbers + 6.0;
										if (w > control_width)
										{
											control_image_size = (w - 20.0) / center_icons_numbers;
											control_image_size -= 4.0;
											if (control_image_size > max_icon_size) control_image_size = max_icon_size;
											control_width = (control_image_size + 3.0) * center_icons_numbers + 6.0;
											if (control_image_size == max_icon_size)
											{
												indent = (w*0.5) - (control_width*0.5);
												if (indent < 10)
													indent = 10;
											}
										}
										else
										{
											indent = (w*0.5) - (control_width*0.5);
											if (indent < 10)
												indent = 10;
										}
										ImGui::Indent(indent);
									}

									// decide button colors
									ImVec4 buttonColor = ImVec4(1.0, 1.0, 1.0, 1.0);
									ImVec4 hoverColor = ImVec4(0.8, 0.8, 0.8, 0.8);
									if (iObjectMoveMode == 2)
									{
										if (g_iStackToSurfaceMode == 1)
										{
											const ImRect image_bb((ImGui::GetCurrentWindow()->DC.CursorPos - padding), ImGui::GetCurrentWindow()->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
											ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
										}
									}
									else
									{
										buttonColor = ImVec4(0.5, 0.5, 0.5, 0.5);
										hoverColor = ImVec4(0.5, 0.5, 0.5, 0.5);
									}

									if (ImGui::ImgBtn(OBJECT_MOVE_FINDFLOOR, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), buttonColor, hoverColor, ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
									{
										if (iObjectMoveMode == 2)
										{
											g_iStackToSurfaceMode = 1 - g_iStackToSurfaceMode;
										}
									}
									if (ImGui::IsItemHovered())
									{
										ImGui::SetTooltip("Find a position beneath or above the object to stack it");
									}

									ImGui::SameLine();
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(icon_spacer, 0.0f));

									buttonColor = ImVec4(1.0, 1.0, 1.0, 1.0);
									hoverColor = ImVec4(0.8, 0.8, 0.8, 0.8);
									if (iObjectMoveMode == 2 && g_iStackToSurfaceMode == 1)
									{
										if (g_iOrientToSurfaceMode == 1)
										{
											const ImRect image_bb((ImGui::GetCurrentWindow()->DC.CursorPos - padding), ImGui::GetCurrentWindow()->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
											ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
										}
									}
									else
									{
										buttonColor = ImVec4(0.5, 0.5, 0.5, 0.5);
										hoverColor = ImVec4(0.5, 0.5, 0.5, 0.5);
									}
									if (ImGui::ImgBtn(OBJECT_MOVE_ORIENTATION, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), buttonColor, hoverColor, ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
									{
										if (iObjectMoveMode == 2 && g_iStackToSurfaceMode == 1)
										{
											// toggle orientation mode
											g_iOrientToSurfaceMode = 1 - g_iOrientToSurfaceMode;
										}
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("Orient object inline with angle of any surface we stack onto");

									ImGui::SameLine();
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(icon_spacer, 0.0f));

									//OBJECT_MOVE_LOCK
									int iIcon = OBJECT_MOVE_LOCK;
									if (g.entityrubberbandlist.size() == 0 && iEntityIndex > 0 && t.entityelement[iEntityIndex].editorlock == 1) iIcon = OBJECT_MOVE_UNLOCK;
									if (iIcon == OBJECT_MOVE_UNLOCK)
									{
										const ImRect image_bb((ImGui::GetCurrentWindow()->DC.CursorPos - padding), ImGui::GetCurrentWindow()->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
										ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
									}
									if (g.entityrubberbandlist.size() > 0)
									{
										bool bAllLocked = true;
										for (int i = 0; i < g.entityrubberbandlist.size(); i++)
										{
											int e = g.entityrubberbandlist[i].e;
											if (e > 0)
											{
												if (t.entityelement[e].editorlock == 0)
												{
													bAllLocked = false;
													break;
												}
											}
										}
										if (iEntityIndex > 0 && t.entityelement[iEntityIndex].editorlock == 0)
											bAllLocked = false;
										if (bAllLocked)
											iIcon = OBJECT_MOVE_UNLOCK;
									}
									if (ImGui::ImgBtn(iIcon, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
									{
										// entity lock/unlock
										bClickedTheLockUnlockButton = true;
									}
									if (ImGui::IsItemHovered())
									{
										if (iIcon == OBJECT_MOVE_UNLOCK)
										{
											if (g.entityrubberbandlist.size() == 0)
												ImGui::SetTooltip("Unlock object to allow it to be moved again");
											else
												ImGui::SetTooltip("Unlock all objects to allow them to be moved again");
										}
										else
										{
											if (g.entityrubberbandlist.size() == 0)
												ImGui::SetTooltip("Lock object to prevent it from being moved");
											else
												ImGui::SetTooltip("Lock all objects to prevent them from being moved");
										}
									}

									ImGui::Indent(-(indent));
								}
								bObjectAllowOverlapping = 1;
							}
							else
							{
								// lock/unlock code repeated above too for non-widget version
								if (pref.iEnableDragDropEntityMode && pref.iObjectEnableAdvanced != 2)
								{
									// is locked or unlocked
									int iIcon = OBJECT_MOVE_LOCK;
									if (g.entityrubberbandlist.size() == 0 && iEntityIndex > 0 && t.entityelement[iEntityIndex].editorlock == 1) iIcon = OBJECT_MOVE_UNLOCK;
									if (g.entityrubberbandlist.size() > 0)
									{
										bool bAllLocked = true;
										for (int i = 0; i < g.entityrubberbandlist.size(); i++)
										{
											int e = g.entityrubberbandlist[i].e;
											if (e > 0)
											{
												if (t.entityelement[e].editorlock == 0)
												{
													bAllLocked = false;
													break;
												}
											}
										}
										if (iEntityIndex > 0 && t.entityelement[iEntityIndex].editorlock == 0) bAllLocked = false;
										if (bAllLocked) iIcon = OBJECT_MOVE_UNLOCK;
									}

									// show lock and unlock button
									ImGui::Indent(10);
									float fFontSize = ImGui::GetFontSize();
									float fButtonSize = w / 3;
									ImGui::PushItemWidth(fButtonSize);
									LPSTR pLockUnlockTitle = "Lock Object";
									if (g.entityrubberbandlist.size() > 0)
									{
										pLockUnlockTitle = "Lock Objects";
										if (iIcon == OBJECT_MOVE_UNLOCK) pLockUnlockTitle = "Unlock Objects";
									}
									else
									{
										if (iIcon == OBJECT_MOVE_UNLOCK) pLockUnlockTitle = "Unlock Object";
									}

									int control_image_size = 56;
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (control_image_size * 0.5) + 7.0f , 4.0f));
									if (ImGui::ImgBtn(iIcon, ImVec2(control_image_size, control_image_size), ImVec4(1.0, 1.0, 1.0, 0.0), ImVec4(1.0, 1.0, 1.0, 1.0), ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
									{
										// entity lock/unlock
										bClickedTheLockUnlockButton = true;
									}
									if (ImGui::IsItemHovered())
									{
										if (iIcon == OBJECT_MOVE_UNLOCK)
										{
											if (g.entityrubberbandlist.size() == 0)
												ImGui::SetTooltip("Unlock object to allow it to be moved again");
											else
												ImGui::SetTooltip("Unlock all objects to allow them to be moved again");
										}
										else
										{
											if (g.entityrubberbandlist.size() == 0)
												ImGui::SetTooltip("Lock object to prevent it from being moved");
											else
												ImGui::SetTooltip("Lock all objects to prevent them from being moved");
										}
									}
									ImGui::PopItemWidth();
									ImGui::Indent(-10);
								}
							}

							if (bReadOnlyMode)
							{
								//PE: Disable ALL gadgets and moving/rotation/scaling.
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							}

							// input field size
							float inputsize = w / 3.0f;
							inputsize -= 10.0f; //For text.
							inputsize -= 5.0f; //For padding.

							ImVec2 vStorePos = ImGui::GetCursorPos();

							if (pref.iObjectEnableAdvanced || !pref.iEnableDragDropEntityMode)
							{
								if(pref.iObjectEnableAdvanced != 2) ImGui::TextCenter("Position");

								// X Y Z layout
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(8.0f, 3.0f));
								ImGui::Text("PX");
								ImGui::SameLine();
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -3.0f));
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (ImGui::InputFloat("##XYZpositionX", &fPos[0], 0.0f, 0.0f, "%.1f")) 	bUpdatePosition = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Position X");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								ImGui::Text("PY");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (ImGui::InputFloat("##XYZpositionY", &fPos[1], 0.0f, 0.0f, "%.1f")) 	bUpdatePosition = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Position Y");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								ImGui::Text("PZ");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (ImGui::InputFloat("##XYZpositionZ", &fPos[2], 0.0f, 0.0f, "%.1f")) 	bUpdatePosition = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Position Z");
								ImGui::PopItemWidth();
							}

							if (bReadOnlyMode)
							{
								ImGui::PopItemFlag();
								ImGui::PopStyleVar();
							}

							if (bReadOnlyMode)
							{
								//PE: Disable ALL gadgets and moving/rotation/scaling.
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							}

							// rotation stuff
							if (fAngle[0] > 360.0) fAngle[0] -= 360.0f;
							if (fAngle[0] < 0.0) fAngle[0] += 360.0f;
							if (fAngle[1] > 360.0) fAngle[1] -= 360.0f;
							if (fAngle[1] < 0.0) fAngle[1] += 360.0f;
							if (fAngle[2] > 360.0) fAngle[2] -= 360.0f;
							if (fAngle[2] < 0.0) fAngle[2] += 360.0f;

							bool bIsStartMarker = false;
							// Player start marker should still allow Y-axis rotations
							if (t.entityprofile[t.entityelement[t.ttte].bankindex].ismarker == 1)
								bIsStartMarker = true;

							if (!bToolRotation && !bIsStartMarker)
							{
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);		
							}
							if (pref.iObjectEnableAdvanced == 2)
							{
								// compact rotation layout
								bool bChangeYAngleOnly = false;
								if (g.entityrubberbandlist.size() > 0 || iEntityInGroupList >= 0) bChangeYAngleOnly = true;
								if (t.entityprofile[t.entityelement[t.ttte].bankindex].ischaracter || bIsStartMarker) bChangeYAngleOnly = true;

								// X Y Z layout
								if (bChangeYAngleOnly == true)
								{
									ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
									ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
								}
								ImGui::SetCursorPos(ImVec2(vStorePos.x, ImGui::GetCursorPos().y) + ImVec2(8.0f, 3.0f));
								ImGui::Text("RX");
								ImGui::SameLine();
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -3.0f));
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (bChangeYAngleOnly == true)
								{
									float fZero = 0.0f;
									ImGui::InputFloat("##Xrotation", &fZero, 0.0f, 0.0f, "%.1f");
								}
								else
								{
									if (ImGui::InputFloat("##Xrotation", &fAngle[0], 0.0f, 0.0f, "%.1f")) 	bUpdateRoataion = true;
								}
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Rotation X");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								if (bChangeYAngleOnly == true)
								{
									ImGui::PopItemFlag();
									ImGui::PopStyleVar();
								}
								ImGui::Text("RY");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (ImGui::InputFloat("##Yrotation", &fAngle[1], 0.0f, 0.0f, "%.1f")) 	bUpdateRoataion = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Rotation Y");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								if (bChangeYAngleOnly == true)
								{
									ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
									ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
								}
								ImGui::Text("RZ");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (bChangeYAngleOnly == true)
								{
									float fZero = 0.0f;
									ImGui::InputFloat("##Zrotation", &fZero, 0.0f, 0.0f, "%.1f");
								}
								else
								{
									if (ImGui::InputFloat("##Zrotation", &fAngle[2], 0.0f, 0.0f, "%.1f")) 	bUpdateRoataion = true;
								}
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Rotation Z");
								ImGui::PopItemWidth();
								if (bChangeYAngleOnly == true)
								{
									ImGui::PopItemFlag();
									ImGui::PopStyleVar();
								}
							}
							else
							{
								ImGui::Indent(10);
								if (g.entityrubberbandlist.size() > 0 || iEntityInGroupList >= 0)
								{
									ImGui::TextCenter("Rotation Angle Y");
									if (ImGui::MaxSliderInputFloat("##Yrotation", &fAngle[1], 0.0f, 359.0f, "Adjust Object Rotation Y", 0, 359))
									{
										if (fAngle[1] > 359.0f) fAngle[1] -= 360.0f;
										bUpdateRoataion = true;
									}
								}
								else
								{
									ImGui::TextCenter("Rotation Angle Y");
									if (ImGui::MaxSliderInputFloat("##Yrotation", &fAngle[1], 0.0f, 359.0f, "Adjust Object Rotation Y", 0, 359))
									{
										if (fAngle[1] > 359.0f) fAngle[1] -= 360.0f;
										bUpdateRoataion = true;
									}
									if (t.entityprofile[t.entityelement[t.ttte].bankindex].ischaracter || bIsStartMarker)
									{
										ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
										ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
									}
									if (pref.iObjectEnableAdvanced)
									{
										ImGui::TextCenter("Rotation Angle X");
										if (ImGui::MaxSliderInputFloat("##Xrotation", &fAngle[0], 0.0f, 359.0f, "Adjust Object Rotation X", 0, 359))
										{
											if (fAngle[0] > 359.0f) fAngle[0] -= 360.0f;
											bUpdateRoataion = true;
										}
										bool bIsLight = false;
										if (!bIsLight)
										{
											ImGui::TextCenter("Rotation Angle Z");
											if (ImGui::MaxSliderInputFloat("##Zrotation", &fAngle[2], 0.0f, 359.0f, "Adjust Object Rotation Z", 0, 359))
											{
												if (fAngle[2] > 359.0f) fAngle[2] -= 360.0f;
												bUpdateRoataion = true;
											}
										}
									}
									if (t.entityprofile[t.entityelement[t.ttte].bankindex].ischaracter || bIsStartMarker)
									{
										ImGui::PopItemFlag();
										ImGui::PopStyleVar();
									}
								}
							}
							if (!bToolRotation && !bIsStartMarker)
							{
								ImGui::PopItemFlag();
								ImGui::PopStyleVar();
							}

							static std::vector<std::array<float, 3>> startRotations;
							static std::vector<std::array<int, 1>> startQuatRotationMode;
							static std::vector<std::array<float, 4>> startQuatRotations;
							static std::vector <std::array<float, 3>> startPositions;
							static bool bStartedRotationUpdate = false;

							if (bUpdateRoataion && ImGui::GetIO().MouseClicked[0])
							{
								// Store initial rotations before any have been applied.
								bStartedRotationUpdate = true;
								if (g.entityrubberbandlist.size() == 0)
								{
									std::array<float, 3> prevRotation = { t.entityelement[t.ttte].rx, t.entityelement[t.ttte].ry, t.entityelement[t.ttte].rz };
									startRotations.push_back(prevRotation);
									std::array<float, 4> prevQuatRotation = { t.entityelement[t.ttte].quatx, t.entityelement[t.ttte].quaty, t.entityelement[t.ttte].quatz, t.entityelement[t.ttte].quatw };
									startQuatRotations.push_back(prevQuatRotation);
									std::array<int, 1> prevQuatRotationMode = { t.entityelement[t.ttte].quatmode };
									startQuatRotationMode.push_back(prevQuatRotationMode);
								}
								else
								{
									for (int i = 0; i < g.entityrubberbandlist.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										std::array<float, 3> prevRotation = { t.entityelement[e].rx, t.entityelement[e].ry, t.entityelement[e].rz };
										startRotations.push_back(prevRotation);
										std::array<float, 4> prevQuatRotation = { t.entityelement[e].quatx, t.entityelement[e].quaty, t.entityelement[e].quatz, t.entityelement[e].quatw };
										startQuatRotations.push_back(prevQuatRotation);
										std::array<int, 1> prevQuatRotationMode = { t.entityelement[e].quatmode };
										startQuatRotationMode.push_back(prevQuatRotationMode);
										// Need to store positions for rubberband, since they rotate about a point. 
										std::array<float, 3> prevPosition = { t.entityelement[e].x, t.entityelement[e].y, t.entityelement[e].z };
										startPositions.push_back(prevPosition);
									}
								}				
							}

							if (bStartedRotationUpdate && ImGui::GetIO().MouseReleased[0])
							{
								// Pass the initial rotations to the undo system.
								if (g.entityrubberbandlist.size() == 0)
								{
									undosys_object_changeposrotscl(t.ttte, t.entityelement[t.ttte].x, t.entityelement[t.ttte].y, t.entityelement[t.ttte].z, 
										startRotations[0][0], startRotations[0][1], startRotations[0][2], 
										startQuatRotationMode[0][0], startQuatRotations[0][0], startQuatRotations[0][1], startQuatRotations[0][2], startQuatRotations[0][3],
										t.entityelement[t.ttte].scalex,	t.entityelement[t.ttte].scaley, t.entityelement[t.ttte].scalez);
								}
								else
								{
									undosys_multiplevents_start();
									for (int i = 0; i < startPositions.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										undosys_object_changeposrotscl(e, startPositions[i][0], startPositions[i][1], startPositions[i][2], 
											startRotations[i][0], startRotations[i][1], startRotations[i][2],
											startQuatRotationMode[i][0], startQuatRotations[i][0], startQuatRotations[i][1], startQuatRotations[i][2], startQuatRotations[i][3],
											t.entityelement[e].scalex, t.entityelement[e].scaley, t.entityelement[e].scalez);
									}
									undosys_multiplevents_finish();
								}

								bStartedRotationUpdate = false;

								startRotations.clear();
								startPositions.clear();
							}

							if (bReadOnlyMode)
							{
								ImGui::PopItemFlag();
								ImGui::PopStyleVar();
							}

							if (bReadOnlyMode)
							{
								//PE: Disable ALL gadgets and moving/rotation/scaling.
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							}

							ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, 4.0f));

							// scale stuff
							float fImporterScaleMultiply = 1.0f;
							fScale[0] = ObjectScaleX(iActiveObj);
							fScale[1] = ObjectScaleY(iActiveObj);
							fScale[2] = ObjectScaleZ(iActiveObj);

							// store old active object scales for rubber band
							if (g_bRefreshScaleValuesFromObjectOnce)
							{
								bUpdateScale = true;
								g_bRefreshScaleValuesFromObjectOnce = false;
							}
							fOldActiveObjectSX = ObjectScaleX(iActiveObj);
							fOldActiveObjectSY = ObjectScaleY(iActiveObj);
							fOldActiveObjectSZ = ObjectScaleZ(iActiveObj);

							if (!bToolScale)
							{
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							}

							// Disable advanced scaling options for particles
							bool enableSettings = true;
							if (t.ttte > 0)
							{
								int entid = t.entityelement[t.ttte].bankindex;
								if (entid > 0)
								{
									if (t.entityprofile[entid].ismarker == 10)
									{
										enableSettings = false; 
									}
								}
							}

							if (pref.iObjectEnableAdvanced == 2)
							{
								// compact scale layout
								// X Y Z layout
								ImGui::SetCursorPos(ImVec2(vStorePos.x, ImGui::GetCursorPos().y) + ImVec2(8.0f, 3.0f));
								ImGui::Text("SX");
								ImGui::SameLine();
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -3.0f));
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								bool bChangedAScaleValue = false;
								if (ImGui::InputFloat("##Xscale", &fScale[0], 0.0f, 0.0f, "%.1f")) 	bChangedAScaleValue = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Scale X");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								ImGui::Text("SY");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (ImGui::InputFloat("##Yscale", &fScale[1], 0.0f, 0.0f, "%.1f")) 	bChangedAScaleValue = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Scale Y");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								ImGui::Text("SZ");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								if (ImGui::InputFloat("##Zscale", &fScale[2], 0.0f, 0.0f, "%.1f")) 	bChangedAScaleValue = true;
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Object Scale Z");
								ImGui::PopItemWidth();
								if (bChangedAScaleValue == true)
								{
									ScaleObject(iActiveObj, fScale[0], fScale[1], fScale[2]);
									fScaleOrg[0] = ObjectScaleX(iActiveObj);
									fScaleOrg[1] = ObjectScaleY(iActiveObj);
									fScaleOrg[2] = ObjectScaleZ(iActiveObj);
									fScaleMul = 0.0f;
									if (t.ttte > 0)
									{
										bUpdateScale = true;
									}
								}

								ImGui::Indent(10);
							}
							else
							{
								ImGui::PushItemWidth(fPushItemWidth);
								ImGui::TextCenter("Scale Multiplier Percentage");
								if (ImGui::MaxSliderInputFloat("##EntityScaleAll", &fScaleMul, 0.0f, 1000.0f, "Use this slider to multiply or divide the scale on all three axis", 0, 1000))
								{
									float fSm = fScaleMul / 100.0f;
									// In certain cases, original scale values can be wiped out to 0, causing object to 'disappear' when this slider is used
									if (fScaleOrg[0] == 0 && fScaleOrg[1] == 0 && fScaleOrg[2] == 0)
									{
										fScaleOrg[0] = ObjectScaleX(iActiveObj);
										fScaleOrg[1] = ObjectScaleY(iActiveObj);
										fScaleOrg[2] = ObjectScaleZ(iActiveObj);
									}
									ScaleObject(iActiveObj, fScaleOrg[0] * fSm, fScaleOrg[1] * fSm, fScaleOrg[2] * fSm);
									if (t.ttte > 0)
									{
										bUpdateScale = true;
									}
								}
								if (pref.iObjectEnableAdvanced)
								{
									if (enableSettings)
									{
										ImGui::TextCenter("Scale X");
										if (ImGui::MaxSliderInputFloat("##XScaleOnly", &fScale[0], 0.0f, 1000.0f, "Scale X axis only", 0, 1000))
										{
											ScaleObject(iActiveObj, fScale[0], fScale[1], fScale[2]);
											//Need to update org.
											fScaleOrg[0] = ObjectScaleX(iActiveObj);
											fScaleOrg[1] = ObjectScaleY(iActiveObj);
											fScaleOrg[2] = ObjectScaleZ(iActiveObj);
											fScaleMul = 0.0f;
											if (t.ttte > 0)
											{
												bUpdateScale = true;
											}
										}
										ImGui::TextCenter("Scale Y");
										if (ImGui::MaxSliderInputFloat("##YScaleOnly", &fScale[1], 0.0f, 1000.0f, "Scale Y axis only", 0, 1000))
										{
											ScaleObject(iActiveObj, fScale[0], fScale[1], fScale[2]);
											//Need to update org.
											fScaleOrg[0] = ObjectScaleX(iActiveObj);
											fScaleOrg[1] = ObjectScaleY(iActiveObj);
											fScaleOrg[2] = ObjectScaleZ(iActiveObj);
											fScaleMul = 0.0f;
											if (t.ttte > 0)
											{
												bUpdateScale = true;
											}
										}
										ImGui::TextCenter("Scale Z");
										if (ImGui::MaxSliderInputFloat("##ZScaleOnly", &fScale[2], 0.0f, 1000.0f, "Scale Z axis only", 0, 1000))
										{
											ScaleObject(iActiveObj, fScale[0], fScale[1], fScale[2]);
											//Need to update org.
											fScaleOrg[0] = ObjectScaleX(iActiveObj);
											fScaleOrg[1] = ObjectScaleY(iActiveObj);
											fScaleOrg[2] = ObjectScaleZ(iActiveObj);
											fScaleMul = 0.0f;
											if (t.ttte > 0)
											{
												bUpdateScale = true;
											}
										}
									}
								}
								ImGui::PopItemWidth();
							}
							if (!bToolScale)
							{
								ImGui::PopItemFlag();
								ImGui::PopStyleVar();
							}

							if (bReadOnlyMode)
							{
								ImGui::PopItemFlag();
								ImGui::PopStyleVar();
							}

							if (bIsLightProbe == false)
							{
								// random spray object mode
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, 4.0f));
								ImGui::PushItemWidth(fPushItemWidth);
								bool bSpray = t.gridedit.entityspraymode;
								if (ImGui::Checkbox("Randomly Spray Objects", &bSpray))
								{
								}
								t.gridedit.entityspraymode = bSpray;
								if (t.gridedit.entityspraymode)
								{
									ImGui::TextCenter("Spray Radius");
									if (ImGui::MaxSliderInputInt("##SetSpray Radius", &t.gridedit.entitysprayrange, 30, 1000, "Set Spray Radius"))
									{
										iDisplayCircleFrames = 20;
									}
									if (ImGui::Checkbox("Move Spray Center With Mouse", &bSprayMoveWithMouse))
									{
									}

								}
								ImGui::PopItemWidth();
								static bool bOldSprayMode = t.gridedit.entityspraymode;
								static uint32_t oldflag = 0;
								static float oldbrushSize = 0;
								if (t.gridedit.entityspraymode)
								{
									if (bOldSprayMode != t.gridedit.entityspraymode)
									{
										oldflag = ggterrain_global_render_params2.flags2;
										oldbrushSize = ggterrain_global_render_params2.brushSize;
										bOldSprayMode = t.gridedit.entityspraymode;
									}
									void set_terrain_sculpt_mode(int mode);
									void set_terrain_edit_mode(int mode);
									set_terrain_sculpt_mode(0);
									set_terrain_edit_mode(0);
									ggterrain_global_render_params2.flags2 |= GGTERRAIN_SHADER_FLAG2_SHOW_BRUSH_SIZE;
									ggterrain_global_render_params2.brushSize = (float)t.gridedit.entitysprayrange * 1.5f; //PE: A bit larger.
								}
								else
								{
									if (bOldSprayMode != t.gridedit.entityspraymode)
									{
										if (oldflag != 0) ggterrain_global_render_params2.flags2 = oldflag;
										if (oldbrushSize != 0) ggterrain_global_render_params2.brushSize = oldbrushSize;
										bOldSprayMode = t.gridedit.entityspraymode;
									}
								}
							}

							// advanced toggle
							if (pref.iObjectEnableAdvanced != 2)
							{
								ControlAdvancedSetting(pref.iObjectEnableAdvanced, "Advanced Object Tools");
							}

							ImGui::PopStyleColor();
							ImGui::Indent(-10);

							// update probe itself if changed
							if (bIsLightProbe == true)
							{
								if (!bReadOnlyMode && bUpdatePosition == true) g_bLightProbeScaleChanged = true;
								if (!bReadOnlyMode && bUpdateRoataion == true) g_bLightProbeScaleChanged = true;
							}

							// update any position, rotation or scale changes
							if (!bReadOnlyMode && bUpdatePosition == true)
							{
								// work out difference for rubber band positioning
								float fOldActiveObjectX = ObjectPositionX(iActiveObj);
								float fOldActiveObjectY = ObjectPositionY(iActiveObj);
								float fOldActiveObjectZ = ObjectPositionZ(iActiveObj);
								if (iEntityInGroupList >= 0)
								{
									//Add all groups with entity to rubberband.
									CheckGroupListForRubberbandSelections(t.ttte);
								}
								else if (g.entityrubberbandlist.size() > 0)
								{
									//Make sure all groups is selected from within rubberband selecting.
									for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										CheckGroupListForRubberbandSelections(e);
									}
								}

								// move the target entity
								PositionObject(iActiveObj, fPos[0], fPos[1], fPos[2]);
								t.entityelement[t.ttte].x = ObjectPositionX(iActiveObj);
								t.entityelement[t.ttte].y = ObjectPositionY(iActiveObj);
								t.entityelement[t.ttte].z = ObjectPositionZ(iActiveObj);

								// if we need to also move rubber band highlighted objects, do so now
								if (g.entityrubberbandlist.size() > 0)
								{
									float fMovedActiveObjectX = ObjectPositionX(iActiveObj) - fOldActiveObjectX;
									float fMovedActiveObjectY = ObjectPositionY(iActiveObj) - fOldActiveObjectY;
									float fMovedActiveObjectZ = ObjectPositionZ(iActiveObj) - fOldActiveObjectZ;
									for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										int tobj = t.entityelement[e].obj;
										if (tobj > 0 && t.entityelement[e].editorlock == 0)
										{
											if (ObjectExist(tobj) == 1)
											{
												if (tobj != iActiveObj)
												{
													// reposition this entity
													PositionObject(tobj, ObjectPositionX(tobj) + fMovedActiveObjectX, ObjectPositionY(tobj) + fMovedActiveObjectY, ObjectPositionZ(tobj) + fMovedActiveObjectZ);
													t.entityelement[e].x = ObjectPositionX(tobj);
													t.entityelement[e].y = ObjectPositionY(tobj);
													t.entityelement[e].z = ObjectPositionZ(tobj);
													if (t.entityelement[e].staticflag == 1) g.projectmodifiedstatic = 1;
													widget_movezonesandlights(e);
												}
											}
										}
									}
								}
							}
							if (!bReadOnlyMode && bUpdateRoataion)
							{
								if (iEntityInGroupList >= 0)
								{
									//Add all groups with entity to rubberband.
									CheckGroupListForRubberbandSelections(t.ttte);
								}
								else if (g.entityrubberbandlist.size() > 0)
								{
									//Make sure all groups is selected from within rubberband selecting.
									for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										CheckGroupListForRubberbandSelections(e);
									}
								}

								// rotate the target entity now (one way euler values)
								int iObj = iActiveObj;
								if (ObjectExist(iObj) == 1)
								{
									// rotation event
									float fMoveAngleX = fAngle[0] - fOldAngle[0];
									float fMoveAngleY = fAngle[1] - fOldAngle[1];
									float fMoveAngleZ = fAngle[2] - fOldAngle[2];
									fOldAngle[0] = fAngle[0];
									fOldAngle[1] = fAngle[1];
									fOldAngle[2] = fAngle[2];
									GGQUATERNION quatRotationEvent, QuatAroundX, QuatAroundY, QuatAroundZ;
									GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(fMoveAngleX));
									GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(fMoveAngleY));
									GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(fMoveAngleZ));
									quatRotationEvent = QuatAroundX * QuatAroundY * QuatAroundZ;

									// current orientation from eulers
									// fundamentally, sliders and values for X Y Z and EULER, so can only set them this way!
									if (g.entityrubberbandlist.size() > 0)
									{
										// when object part of selected, use quat and only use "Y" value as a movement delta via rotation event
										GGQUATERNION QuatAroundX, QuatAroundY, QuatAroundZ;
										GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(ObjectAngleX(iObj)));
										GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(ObjectAngleY(iObj)));
										GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(ObjectAngleZ(iObj)));
										GGQUATERNION quatCurrentOrientation;
										quatCurrentOrientation = QuatAroundX * QuatAroundY * QuatAroundZ;
										GGQUATERNION quatNewOrientation;
										GGQuaternionMultiply(&quatNewOrientation, &quatCurrentOrientation, &quatRotationEvent);
										RotateObjectQuat(iObj, quatNewOrientation.x, quatNewOrientation.y, quatNewOrientation.z, quatNewOrientation.w);
										// and also rotate the selected!
										SetStartPositionsForRubberBand(iObj);
										RotateAndMoveRubberBand(iObj, 0, 0, 0, quatRotationEvent);

										// ensure the root object (active object) also gets its rotation!
										if (t.ttte > 0)
										{
											t.entityelement[t.ttte].rx = ObjectAngleX(iActiveObj);
											t.entityelement[t.ttte].ry = ObjectAngleY(iActiveObj);
											t.entityelement[t.ttte].rz = ObjectAngleZ(iActiveObj);
										}
									}
									else
									{
										// when single object, treat as euler X Y Z 
										RotateObject(iObj, fAngle[0], fAngle[1], fAngle[2]);
										if (t.ttte > 0)
										{
											t.entityelement[t.ttte].rx = ObjectAngleX(iActiveObj);
											t.entityelement[t.ttte].ry = ObjectAngleY(iActiveObj);
											t.entityelement[t.ttte].rz = ObjectAngleZ(iActiveObj);
										}
									}

									// update entity quat as the preferred source rotation
									entity_updatequatfromeuler(t.ttte);
								}
							}
							if (!bReadOnlyMode && bUpdateScale == true)
							{
								// scale the target entity
								t.entityelement[t.ttte].scalex = ObjectScaleX(iActiveObj) - 100.0;
								t.entityelement[t.ttte].scaley = ObjectScaleY(iActiveObj) - 100.0;
								t.entityelement[t.ttte].scalez = ObjectScaleZ(iActiveObj) - 100.0;

								if (iEntityInGroupList >= 0)
								{
									//Add all groups with entity to rubberband.
									CheckGroupListForRubberbandSelections(t.ttte);
								}
								else if (g.entityrubberbandlist.size() > 0)
								{
									//Make sure all groups is selected from within rubberband selecting.
									for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										CheckGroupListForRubberbandSelections(e);
									}
								}

								// if we need to also scale rubber band highlighted objects, do so now
								if (g.entityrubberbandlist.size() > 0)
								{
									float fMovedActiveObjectSX = ObjectScaleX(iActiveObj) - fOldActiveObjectSX;
									float fMovedActiveObjectSY = ObjectScaleY(iActiveObj) - fOldActiveObjectSY;
									float fMovedActiveObjectSZ = ObjectScaleZ(iActiveObj) - fOldActiveObjectSZ;
									for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
									{
										int e = g.entityrubberbandlist[i].e;
										int tobj = t.entityelement[e].obj;
										if (tobj > 0 && t.entityelement[e].editorlock == 0)
										{
											if (ObjectExist(tobj) == 1)
											{
												if (tobj != iActiveObj)
												{
													//LB: oops! if (t.entityprofile[e].ischaracter == 0 && t.entityprofile[e].ismarker == 0)
													int entid = t.entityelement[e].bankindex;
													if (entid > 0)
													{
														if (t.entityprofile[entid].ischaracter == 0 && t.entityprofile[entid].ismarker == 0)
														{
															ScaleObject(tobj, ObjectScaleX(tobj) + fMovedActiveObjectSX, ObjectScaleY(tobj) + fMovedActiveObjectSY, ObjectScaleZ(tobj) + fMovedActiveObjectSZ);
															t.entityelement[e].scalex = ObjectScaleX(tobj) - 100;
															t.entityelement[e].scaley = ObjectScaleY(tobj) - 100;
															t.entityelement[e].scalez = ObjectScaleZ(tobj) - 100;
															if (t.entityelement[e].staticflag == 1) g.projectmodifiedstatic = 1;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}

					//#############################
					//#### PE: New properties. ####
					//#############################

					if (iEntityIndex > 0 && iMasterID > 0 && ObjectExist(iActiveObj))
					{
						bool bGeneralActive = false;
						if ((t.entityprofile[iMasterID].ismarker == 0 /*|| t.entityprofile[iMasterID].islightmarker == 1 */ || t.tflagspawn == 1))
							bGeneralActive = true;
						if (t.tflagchar == 0 && t.tflagvis == 1 && t.tflagsimpler == 0)
							bGeneralActive = true;

						if (bGeneralActive)
						{
							char title[24] = "General##2";
							if (t.entityprofile[iMasterID].ischaracter == 1)
								strcpy(title, "Character Settings##2");

							if (pref.bAutoClosePropertySections && iLastOpenHeader != 17)
								ImGui::SetNextItemOpen(false, ImGuiCond_Always);

							if (ImGui::StyleCollapsingHeader(title, ImGuiTreeNodeFlags_None)) //ImGuiTreeNodeFlags_DefaultOpen
							{
								iLastOpenHeader = 17;
								DisplayFPEGeneral(false, iMasterID, &t.entityelement[iEntityIndex].eleprof, iEntityIndex);
							}
						}

						bool bMaterialsUsed = true;
						cStr HeaderName = "Behavior##2";
						if (t.entityprofile[iMasterID].ismarker == 1)
						{
							HeaderName = "Customize##2";
						}
						if (t.tflaglight == 1 || t.entityprofile[iMasterID].ismarker == 2)
						{
							if(bIsLightProbe==true)
							{
								HeaderName = "Probe Settings##2";
							}
							else
							{
								HeaderName = "Color Palette##2";
							}
						}
						if (t.entityprofile[iMasterID].ismarker == 10)
						{
							HeaderName = "Particles##2";
							bMaterialsUsed = false;
						}
						if (t.entityprofile[iMasterID].bIsDecal)
						{
							HeaderName = "Decal##99";
						}

						bool bAllowBehaviorChange = true;
						bool bIsThisAnEBE = false;
						if (t.entityprofile[iMasterID].isebe != 0 && t.widget.pickedEntityIndex > 0)
						{
							HeaderName = "Structure Editor Object##99";
							bIsThisAnEBE = true;
						}
						else
						{
							if (t.entityprofile[iMasterID].ismarker == 0)
							{
								if (t.entityelement[iEntityIndex].staticflag != 0)
								{
									// cannot change behavior of a static object!
									bAllowBehaviorChange = false;
								}
							}
						}

						// rubber band awareness
						bool bRubberbandActive = false;
						if (g.entityrubberbandlist.size() > 0) bRubberbandActive = true;
						if (g.entityrubberbandlist.size() == 1 && g.entityrubberbandlist[0].e == iEntityIndex) bRubberbandActive = false;
						if (!bRubberbandActive )
						{
							if (pref.bAutoClosePropertySections && iLastOpenHeader != 16)
								ImGui::SetNextItemOpen(false, ImGuiCond_Always);

							if (ImGui::StyleCollapsingHeader(HeaderName.Get(), ImGuiTreeNodeFlags_DefaultOpen))//ImGuiTreeNodeFlags_None))
							{
								iLastOpenHeader = 16;

								ImGui::Indent(10);
								if (bIsThisAnEBE == true)
								{
									// Edit Structure Editor Object
									float edit_gadget_size = ImGui::GetFontSize()*10.0;
									float w = ImGui::GetWindowContentRegionWidth();
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (edit_gadget_size*0.5), 0.0f));
									if (ImGui::StyleButton("Edit Structure Object", ImVec2(edit_gadget_size, 0)))
									{
										t.widget.propertybuttonselected = 1;
										t.ebe.bReleaseMouseFirst = true;
									}
								}
								else
								{
									if (bAllowBehaviorChange == true)
									{
										DisplayFPEBehavior(false, iMasterID, &t.entityelement[iEntityIndex].eleprof, iEntityIndex, false);
									}
									else
									{
										// inform user cannot use behaviors for static objects
										ImGui::TextCenter("No Behavior For Static Objects");
									}
								}
								ImGui::Indent(-10);
							}
						}

						// Collectbale/Quest Settings if required
						int iCollectableSettingsMode = 0;
						LPSTR pCollectySettingsLabel = "Additional Settings##1";
						if (t.entityelement[iEntityIndex].eleprof.iscollectable != 0) { iCollectableSettingsMode = 1; pCollectySettingsLabel = "Collectable Settings##1"; }
						if (iCollectionQuestIndex > 0)
						{
							pCollectySettingsLabel = "Quest Settings##1";
							iCollectableSettingsMode = 2;
						}
						if (iCollectableSettingsMode > 0)
						{
							if (pref.bAutoClosePropertySections && iLastOpenHeader != 28) ImGui::SetNextItemOpen(false, ImGuiCond_Always);
							if (ImGui::StyleCollapsingHeader(pCollectySettingsLabel, ImGuiTreeNodeFlags_None))
							{
								iLastOpenHeader = 28;
								int iCollectionItemIndex = -1;
								if (iCollectableSettingsMode == 1)
								{
									if (t.entityprofile[t.entityelement[iEntityIndex].bankindex].isweapon_s.Len() > 0)
									{
										// weapon style
										cstr pSearchForWeapon = cstr("weapon=") + cstr(t.entityprofile[t.entityelement[iEntityIndex].bankindex].isweapon_s);
										for (int ci = 0; ci < g_collectionList.size(); ci++)
										{
											if (stricmp (g_collectionList[ci].collectionFields[8].Get(), pSearchForWeapon.Get()) == NULL)
											{
												iCollectionItemIndex = ci;
												break;
											}
										}
										if (iCollectionItemIndex == -1)
										{
											// okay, no modern reference, fall back on title to match name of weapon!
											for (int ci = 0; ci < g_collectionList.size(); ci++)
											{
												if (stricmp (g_collectionList[ci].collectionFields[0].Get(), t.entityelement[iEntityIndex].eleprof.name_s.Get()) == NULL)
												{
													iCollectionItemIndex = ci;
													break;
												}
											}
										}
									}
									else
									{
										// regular item
										for (int ci = 0; ci < g_collectionList.size(); ci++)
										{
											if (stricmp (g_collectionList[ci].collectionFields[0].Get(), t.entityelement[iEntityIndex].eleprof.name_s.Get()) == NULL)
											{
												iCollectionItemIndex = ci;
												break;
											}
										}
									}
								}
								if (iCollectableSettingsMode == 2)
								{
									if (iCollectionQuestIndex > 0)
									{
										iCollectionItemIndex = iCollectionQuestIndex - 1;
									}
								}
								if (iCollectionItemIndex == -1)
								{
									// No Master Collection List entry, enable user to create an entry ( will be saved below ) 
									float w = ImGui::GetWindowContentRegionWidth();
									float but_gadget_size = ImGui::GetFontSize() * 15.0;
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f));
									LPSTR pCreateButtonLabel = "Create Collection Item";
									if (ImGui::StyleButton(pCreateButtonLabel, ImVec2(but_gadget_size, 0)))
									{
										if (iCollectableSettingsMode == 1)
										{
											// create an item entry
											collectionItemType item;
											fill_rpg_item_defaults(&item, iMasterID, iEntityIndex);
											// first, if a weapon, check DESC for matching 'raw weapon path'
											// and if found, overwrite with better item entry created here
											if ( t.entityprofile[iMasterID].isweapon_s.Len()>0)
											{
												for (int n = 0; n < g_collectionList.size(); n++)
												{
													if (item.collectionFields.size() > 3)
													{
														if (g_collectionList[n].collectionFields.size() > 3)
														{
															if (stricmp (g_collectionList[n].collectionFields[3].Get(), t.entityprofile[iMasterID].isweapon_s.Get()) == NULL)
															{
																// overwrite, save and leave
																g_collectionList[n].collectionFields = item.collectionFields;
																g_bChangedGameCollectionList = true;
																break;
															}
														}
													}
												}
											}
											// check if unique
											bool bNewItemIsUnqiue = true;
											for (int n = 0; n < g_collectionList.size(); n++)
											{
												if (item.collectionFields.size() > 0)
												{
													if (g_collectionList[n].collectionFields.size() > 0)
													{
														if (g_collectionList[n].collectionFields[0] == item.collectionFields[0])
														{
															bNewItemIsUnqiue = false;
															break;
														}
													}
												}
											}
											if (bNewItemIsUnqiue == true)
											{
												g_collectionList.push_back(item);
												g_bChangedGameCollectionList = true;
											}
										}
									}
								}
								else
								{
									// show collectable details
									bool bQuestTypeIsCollect = false;
									ImGui::Indent(10);
									int iCount = g_collectionLabels.size();
									if (iCollectableSettingsMode == 2) iCount = g_collectionQuestLabels.size();
									for (int l = 0; l < iCount; l++)
									{
										int iKnownLabel = -1;
										LPSTR pLabel = "";
										if (iCollectableSettingsMode == 1)
										{
											pLabel = g_collectionLabels[l].Get();
										}
										if (iCollectableSettingsMode == 2)
										{
											pLabel = g_collectionQuestLabels[l].Get();
										}
										if (stricmp(pLabel, "title") == NULL) iKnownLabel = 0;
										if (stricmp(pLabel, "image") == NULL) iKnownLabel = 2;
										if (iCollectableSettingsMode == 1)
										{
											if (stricmp(pLabel, "description") == NULL) iKnownLabel = 3;
											if (stricmp(pLabel, "cost") == NULL) iKnownLabel = 4;
											if (stricmp(pLabel, "value") == NULL) iKnownLabel = 5;
											if (stricmp(pLabel, "container") == NULL) iKnownLabel = 6;
											if (stricmp(pLabel, "ingredients") == NULL) iKnownLabel = 7;
											if (stricmp(pLabel, "style") == NULL) iKnownLabel = 8;
										}
										if (iCollectableSettingsMode == 2)
										{
											if (stricmp(pLabel, "type") == NULL) iKnownLabel = 51;
											if (stricmp(pLabel, "desc1") == NULL) iKnownLabel = 52;
											if (stricmp(pLabel, "desc2") == NULL) iKnownLabel = 53;
											if (stricmp(pLabel, "desc3") == NULL) iKnownLabel = 54;
											if (stricmp(pLabel, "object") == NULL) iKnownLabel = 55;
											if (stricmp(pLabel, "receiver") == NULL) iKnownLabel = 56;
											if (stricmp(pLabel, "level") == NULL) iKnownLabel = 57;
											if (stricmp(pLabel, "points") == NULL) iKnownLabel = 58;
											if (stricmp(pLabel, "value") == NULL) iKnownLabel = 59;
											if (stricmp(pLabel, "status") == NULL) iKnownLabel = 60;
											if (stricmp(pLabel, "activate") == NULL) iKnownLabel = 61;
											if (stricmp(pLabel, "quantity") == NULL) iKnownLabel = 62;
											if (stricmp(pLabel, "endmap") == NULL) iKnownLabel = 63;
										}
										// show the EXTRA columns entered into the collection TSV file so UI can edit them
										//if (iKnownLabel >= 0)
										if (iKnownLabel >= 0 || iCollectableSettingsMode == 1) // but only for COLLECTION items, not quests (iKnownLabel can be -1)
										{
											// Any tip
											LPSTR pShowTop = "";
											if (iCollectableSettingsMode == 1)
											{
												pShowTop = "Enter a value for this item";
												if (iKnownLabel == 2) pShowTop = "Select an image that will be used to represent this object in your HUD screens";
												if (iKnownLabel == 3) pShowTop = "Enter a description for this item that may appear in your HUD screens";
												if (iKnownLabel == 4) pShowTop = "Enter a cost for this item that may appear in your HUD screens";
												if (iKnownLabel == 5) pShowTop = "Enter a value for this item that may appear in your HUD screens";
												if (iKnownLabel == 6) pShowTop = "Enter a container for this item that determines where the item will start in the game";
												if (iKnownLabel == 7) pShowTop = "Enter ingredients separated my comma to be used by a recipe item";
												if (iKnownLabel == 8) pShowTop = "Enter a style for this item such as recipe or spell";
											}
											if (iCollectableSettingsMode == 2)
											{
												pShowTop = "Enter a value for this quest";
												if (iKnownLabel == 2) pShowTop = "Select an image that will be used to represent this quest in your HUD screens";
												if (iKnownLabel == 51) pShowTop = "Enter a quest type for the quest task";
												if (iKnownLabel == 52) pShowTop = "Enter a description for the quest task";
												if (iKnownLabel == 53) pShowTop = "Enter a description for the quest task";
												if (iKnownLabel == 54) pShowTop = "Enter a description for the quest task";
												if (iKnownLabel == 55) pShowTop = "Enter the name of an object that will represent the quest object";
												if (iKnownLabel == 56) pShowTop = "Enter the name of an object that will represent the quest receiver";
												if (iKnownLabel == 57) pShowTop = "Enter the player level required to activate this quest";
												if (iKnownLabel == 58) pShowTop = "Enter the number of XP points awarded when this quest is completed";
												if (iKnownLabel == 59) pShowTop = "Enter the money earned by completing this quest";
												if (iKnownLabel == 60) pShowTop = "Enter the initial status of this quest when the game starts";
												if (iKnownLabel == 61) pShowTop = "Enter the object to activate when this quest is completed";
												if (iKnownLabel == 62) pShowTop = "Enter a quantity associated with this quest";
												if (iKnownLabel == 63) pShowTop = "Enter the level name that this quest is active on";
											}

											// Attrib Label
											if (iKnownLabel == 2)
											{
												// can change image
												LPSTR pImageLabel = "";
												if (iCollectableSettingsMode == 1)
												{
													pImageLabel = "Item Icon Image";
												}
												if (iCollectableSettingsMode == 2)
												{
													pImageLabel = "Quest Icon Image";
												}
												ImGui::TextCenter(pImageLabel);
												float w = ImGui::GetContentRegionAvailWidth();
												cstr UniqueCollectionItemImage = "##UniqueCollectionItemImage";
												if (iSelectedLibraryStingReturnID == window->GetID(UniqueCollectionItemImage.Get()))
												{
													if (iCollectableSettingsMode == 1)
													{
														g_collectionList[iCollectionItemIndex].collectionFields[l] = sSelectedLibrarySting.Get();
														g_collectionList[iCollectionItemIndex].iEntityID = iMasterID;
														g_collectionList[iCollectionItemIndex].iEntityElementE = iEntityIndex;
													}
													if (iCollectableSettingsMode == 2)
													{
														g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = sSelectedLibrarySting.Get();
													}
													g_bChangedGameCollectionList = true;
													sSelectedLibrarySting = "";
													iSelectedLibraryStingReturnID = -1; //disable.
													g_iIconImageInPropertiesLastEntIndex = 0;// trigger reload
												}
												int entid = 0;
												if (iEntityIndex > 0) entid = t.entityelement[iEntityIndex].bankindex;
												if (entid > 0)
												{
													if (g_iIconImageInPropertiesLastEntIndex != iEntityIndex)
													{
														g_iIconImageInPropertiesLastEntIndex = iEntityIndex;
														g_iconImageInPropertiesLastName_s = "";
														if (entid > 0) g_iconImageInPropertiesLastName_s = t.entitybank_s[entid];
														g_iIconImageInProperties = 0;
													}
													else
													{
														// even if sale element index, can delete and quickly create another collectable in same index slot, need to be aware of this
														if (strcmp (g_iconImageInPropertiesLastName_s.Get(), t.entitybank_s[entid].Get()) != NULL)
														{
															g_iconImageInPropertiesLastName_s = t.entitybank_s[entid];
															g_iIconImageInProperties = 0;
														}
													}
												}
												LPSTR pIconImageInProperties = "";
												if (iCollectableSettingsMode == 1)
												{
													pIconImageInProperties = g_collectionList[iCollectionItemIndex].collectionFields[l].Get();
												}
												if (iCollectableSettingsMode == 2)
												{
													pIconImageInProperties = g_collectionQuestList[iCollectionItemIndex].collectionFields[l].Get();
												}
												if (g_iIconImageInProperties == 0)
												{
													if (FileExist(pIconImageInProperties) == 0)
													{
														// image specified does not exist, original file could have moved/deleted, so revert to default
														pIconImageInProperties = "default";
													}
													cstr actualImgFile_s = "";
													if (stricmp(pIconImageInProperties, "default") == NULL)
													{
														// replace with actual img file if viewing property
														cstr entityfile = t.entitybank_s[iMasterID];
														actualImgFile_s = get_rpg_imagefinalfile(entityfile);
														pIconImageInProperties = actualImgFile_s.Get();
														if (iCollectableSettingsMode == 1)
														{
															g_collectionList[iCollectionItemIndex].collectionFields[l] = pIconImageInProperties;
															g_collectionList[iCollectionItemIndex].iEntityID = iMasterID;
															g_collectionList[iCollectionItemIndex].iEntityElementE = iEntityIndex;
														}
														if (iCollectableSettingsMode == 2)
														{
															g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = pIconImageInProperties;
														}
														g_bChangedGameCollectionList = true;
													}
													g_iIconImageInProperties = g.iconimagebankoffset;
													if (GetImageExistEx(g_iIconImageInProperties) == 1) DeleteImage(g_iIconImageInProperties);
													image_setlegacyimageloading(true);
													if (FileExist(pIconImageInProperties) == 1)
													{
														// actual icon image
														LoadImage(pIconImageInProperties, g_iIconImageInProperties);
													}
													else
													{
														// specified image not found, use placeholder
														pIconImageInProperties = "imagebank\\HUD Library\\MAX\\object.png";
														LoadImage(pIconImageInProperties, g_iIconImageInProperties);
													}
													image_setlegacyimageloading(false);
												}
												int iTextureID = g_iIconImageInProperties;
												ImVec2 ImageSize = ImVec2(w - ImGui::GetCurrentWindow()->ScrollbarSizes.x, ImGui::GetFontSize());
												ID3D11ShaderResourceView* lpTexture = GetImagePointerView(iTextureID);
												if (lpTexture)
												{
													float img_w = ImageWidth(iTextureID);
													float img_h = ImageHeight(iTextureID);
													ImageSize.y = img_h * (ImageSize.x / img_w);
												}
												ImVec2 vImagePos = ImGui::GetCursorPos();
												ImGui::Dummy(ImageSize);
												ImVec4 color = ImVec4(1.0, 1.0, 1.0, 1.0);
												ImVec4 back_color = ImVec4(0.2, 0.2, 0.2, 0.75);
												if (ImGui::IsItemHovered())
												{
													color.w = 0.75;
													if (ImGui::IsMouseReleased(0))
													{
														sStartLibrarySearchString = "Icon";
														bExternal_Entities_Window = true;
														iDisplayLibraryType = 2; //Image
														iLibraryStingReturnToID = window->GetID(UniqueCollectionItemImage.Get());
													}
												}
												ImVec2 img_pos = ImGui::GetWindowPos() + vImagePos;
												img_pos.y -= ImGui::GetScrollY();
												window->DrawList->AddRectFilled(img_pos, img_pos + ImageSize, ImGui::GetColorU32(back_color));
												if (lpTexture)
												{
													window->DrawList->AddImage((ImTextureID)lpTexture, img_pos, img_pos + ImageSize, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(color));
												}
												else
												{
													window->DrawList->AddRectFilled(img_pos, img_pos + ImageSize, ImGui::GetColorU32(color));
												}
												lpTexture = GetImagePointerView(TOOL_PENCIL); //Add pencil
												if (lpTexture)
												{
													ImVec2 vDrawPos = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - ImageSize.y - 3.0f };
													window->DrawList->AddImage((ImTextureID)lpTexture, vDrawPos, vDrawPos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
												}
												if (ImGui::IsItemHovered() && pShowTop) ImGui::SetTooltip(pShowTop);
											}
											else
											{
												if (iCollectableSettingsMode == 2 && iKnownLabel == 51)
												{
													// drop down to make life easier
													char cTmpInput[MAX_PATH];
													strcpy(cTmpInput, g_collectionQuestList[iCollectionItemIndex].collectionFields[l].Get());
													const char* items[] = { "Collect", "Destroy", "Deliver", "Activate" };
													int item_current = 0;
													if (stricmp(cTmpInput, "collect") == NULL) item_current = 0;
													if (stricmp(cTmpInput, "destroy") == NULL) item_current = 1;
													if (stricmp(cTmpInput, "deliver") == NULL) item_current = 2;
													if (stricmp(cTmpInput, "activate") == NULL) item_current = 3;
													ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
													ImGui::Text("Quest Type");
													ImGui::SameLine();
													ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
													ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
													ImGui::PushItemWidth(-10);
													if (ImGui::Combo("##combostaticQuestType2", &item_current, items, IM_ARRAYSIZE(items)))
													{
														if (item_current == 0) g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = "collect";
														if (item_current == 1) g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = "destroy";
														if (item_current == 2) g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = "deliver";
														if (item_current == 3) g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = "activate";
													}
													ImGui::PopItemWidth();

													// optional quantity property only applicable to some quest types
													if (item_current == 0)
													{
														// later in loop we use this flag to allow quantity to show
														bQuestTypeIsCollect = true;
													}
												}
												else
												{
													// good old typing out your entry
													bool bAllowEditing = true;
													if (iKnownLabel == 0 || iKnownLabel == 1) bAllowEditing = false;
													if (t.entityprofile[iMasterID].isweapon > 0 && iKnownLabel >= 7) bAllowEditing = false;
													if (iCollectableSettingsMode == 2 && iKnownLabel == 62 && bQuestTypeIsCollect == false) bAllowEditing = false;
													if (bAllowEditing == true)
													{
														char pNameOfAttrib[MAX_PATH];
														if (iCollectableSettingsMode == 1)
														{
															strcpy(pNameOfAttrib, "Item ");
														}
														if (iCollectableSettingsMode == 2)
														{
															strcpy(pNameOfAttrib, "Quest ");
														}
														char pCap[2];
														pCap[0] = pLabel[0];
														pCap[1] = 0;
														strupr(pCap);
														strcat(pNameOfAttrib, pCap);
														strcat(pNameOfAttrib, pLabel + 1);
														ImGui::TextCenter(pNameOfAttrib);
														ImGui::PushItemWidth(-10);
														char cTmpInput[MAX_PATH];
														if (iCollectableSettingsMode == 1)
														{
															strcpy(cTmpInput, g_collectionList[iCollectionItemIndex].collectionFields[l].Get());
														}
														if (iCollectableSettingsMode == 2)
														{
															strcpy(cTmpInput, g_collectionQuestList[iCollectionItemIndex].collectionFields[l].Get());
														}
														int inputFlags = 0;
														char pNameOfAttribUnique[MAX_PATH];
														strcpy(pNameOfAttribUnique, "##CollectableItem");
														strcat(pNameOfAttribUnique, pLabel);
														if (ImGui::InputText(pNameOfAttribUnique, &cTmpInput[0], MAXTEXTINPUT, inputFlags))
														{
															if (iCollectableSettingsMode == 1)
															{
																g_collectionList[iCollectionItemIndex].collectionFields[l] = cTmpInput;
																g_collectionList[iCollectionItemIndex].iEntityID = iMasterID;
																g_collectionList[iCollectionItemIndex].iEntityElementE = iEntityIndex;
															}
															if (iCollectableSettingsMode == 2)
															{
																g_collectionQuestList[iCollectionItemIndex].collectionFields[l] = cTmpInput;
															}
															bImGuiGotFocus = true;
															g_bChangedGameCollectionList = true;
														}
														if (ImGui::IsItemHovered() && pShowTop) ImGui::SetTooltip(pShowTop);
														if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;
														ImGui::PopItemWidth();
													}
												}
											}
										}
									}
									ImGui::Indent(-10);

									// option to delete a collectable from the global list
									if (iCollectableSettingsMode == 1)
									{
										float but_gadget_size = ImGui::GetFontSize() * 12.0;
										float w = ImGui::GetWindowContentRegionWidth() - 10.0;
										ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f));
										LPSTR pCreateButtonLabel = "Delete From Collection List";
										if (ImGui::StyleButton(pCreateButtonLabel, ImVec2(but_gadget_size, 0)))
										{
											// create new quest list without the one deleted
											std::vector<collectionItemType> newCollectionList;
											for (int ci = 0; ci < g_collectionList.size(); ci++)
											{
												if (ci != iCollectionItemIndex)
												{
													newCollectionList.push_back(g_collectionList[ci]);
												}
											}
											g_collectionList = newCollectionList;
											g_bChangedGameCollectionList = true;
										}
										if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete this collection item from the main collection list of the game project");
									}
								}
							}
						}

						// save any changes to game collection list 
						if (g_bChangedGameCollectionList == true)
						{
							// go through all object parents to ensure
							refresh_rpg_parents_of_items();
							
							// save collection item list out
							save_rpg_system(pref.cLastUsedStoryboardProject, true);
							g_bChangedGameCollectionList = false;
						}

						// Custom Materials now an advanced feature
						if ( bMaterialsUsed == true && pref.iObjectEnableAdvanced && t.entityprofile[iMasterID].ismarker == 0 && bIsThisAnEBE == false)
						{
							// multi-selection allows SOME material changes
							if (!bRubberbandActive)
							{
								// single object editing
								bool bNeedMaterialUpdate = false;

								// detect if object changed while showing materials, ensure the switch happens in the UI
								static void* pLastObjectPtr;
								if ((void*)pObject != pLastObjectPtr)
								{
									//PE: reset mesh names, as we got a new object.
									t.importer.bModelMeshNamesSet = false;
									t.importer.cModelMeshNames.clear();
									bNeedMaterialUpdate = true;
									pLastObjectPtr = (void*)pObject;
								}

								if (pref.bAutoClosePropertySections && iLastOpenHeader != 13)
									ImGui::SetNextItemOpen(false, ImGuiCond_Always);

								if (ImGui::StyleCollapsingHeader("Materials##2", ImGuiTreeNodeFlags_None))
								{
									iLastOpenHeader = 13;

									ImGui::Indent(10);

									{
										if(!t.entityelement[iEntityIndex].eleprof.bUseFPESettings)
										{
											if (t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex >= 5)
												t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex = 0;

											char material_sound_selection[256] = "\0";
											if (t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex <= 0)
											{
												strcpy(material_sound_selection, "None");
											}
											if (t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex == 1)
											{
												strcpy(material_sound_selection, "Silent");
											}
											else if (t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex == 2)
											{
												strcpy(material_sound_selection, "Stone");
											}
											else if (t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex == 3)
											{
												strcpy(material_sound_selection, "Metal");
											}
											else if (t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex == 4)
											{
												strcpy(material_sound_selection, "Wood");
											}
											char* cMaterialTypes[4] = { "Silent", "Stone", "Metal", "Wood" };
											ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 15));
											ImGui::Text("Material Type");
											ImGui::SameLine();
											ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
											ImGui::PushItemWidth(-10);
											if (ImGui::BeginCombo("##ImporterMaterialType", &material_sound_selection[0], ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLarge))
											{
												for (int i = 0; i < 4; i++)
												{
													bool is_selected = false;
													if (strcmp(material_sound_selection, cMaterialTypes[i]) == NULL)
													{
														is_selected = true;
													}
													if (ImGui::Selectable(cMaterialTypes[i], is_selected))
													{
														strcpy(material_sound_selection, cMaterialTypes[i]);
														t.entityelement[iEntityIndex].eleprof.iMaterialSoundIndex = i + 1;
														//t.slidersmenuvalue[t.importer.properties1Index][10].value = i + 1;
													}
													if (is_selected) ImGui::SetItemDefaultFocus();
												}
												ImGui::EndCombo();
											}
											if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select the material index for this object");
											ImGui::PopItemWidth();
											ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 15));
										}

										//PE: display material settings
										WickedSetEntityId(iMasterID);
										WickedSetElementId(iEntityIndex);
										Wicked_Change_Object_Material((void*)pObject, 0, &t.entityelement[iEntityIndex].eleprof,true, t.entityelement[iEntityIndex].eleprof.bUseFPESettings);

										if (ImGui::Checkbox("Always use Original Object Settings##2", &t.entityelement[iEntityIndex].eleprof.bUseFPESettings))
										{
											bNeedMaterialUpdate = true;
											t.importer.bModelMeshNamesSet = false;
											t.importer.cModelMeshNames.clear();
										}
										WickedSetEntityId(-1);
										WickedSetElementId(0);
									}

									ImGui::Indent(-10);
								}
								if (bNeedMaterialUpdate)
								{
									if (t.entityelement[iEntityIndex].eleprof.bUseFPESettings || !t.entityelement[iEntityIndex].eleprof.bCustomWickedMaterialActive)
									{
										// Set material settings from master object.
										sObject* pMasterObject = g_ObjectList[g.entitybankoffset + iMasterID];
										if (pMasterObject && iMasterID > 0 && iMasterID < t.entityprofile.size())
										{
											Wicked_Copy_Material_To_Grideleprof((void*)pMasterObject, 0, &t.entityelement[iEntityIndex].eleprof);
											Wicked_Set_Material_From_grideleprof((void*)pObject, 0, &t.entityelement[iEntityIndex].eleprof);
										}
									}
									else
									{
										// Set custom material settings.
										Wicked_Copy_Material_To_Grideleprof((void*)pObject, 0, &t.entityelement[iEntityIndex].eleprof);
										Wicked_Set_Material_From_grideleprof((void*)pObject, 0, &t.entityelement[iEntityIndex].eleprof);
										t.entityelement[iEntityIndex].eleprof.WEMaterial.MaterialActive = true;
										t.grideleprof.WEMaterial.MaterialActive = true;
									}
								}
							}
							else
							{
								// multi selection editing
								bool bNeedMaterialUpdate = false;
								if (pref.bAutoClosePropertySections && iLastOpenHeader != 13) ImGui::SetNextItemOpen(false, ImGuiCond_Always);
								if (ImGui::StyleCollapsingHeader("All Materials##3", ImGuiTreeNodeFlags_None))
								{
									// UI for all materials
									iLastOpenHeader = 13;
									ImGui::Indent(10);

									// any custom flags sets ALL of them (or none of them)
									bool bAnyCustomMaterials = false;
									bool bAnyUseFPESettings = false;
									for (int ii = 0; ii < g.entityrubberbandlist.size(); ii++)
									{
										int ee = g.entityrubberbandlist[ii].e;
										if (t.entityelement[ee].eleprof.bCustomWickedMaterialActive == true)
										{
											bAnyCustomMaterials = true;
											//break;
										}
										if (t.entityelement[ee].eleprof.bUseFPESettings)
										{
											bAnyUseFPESettings = true;
										}
									}
									{
										// display custom material settings for ALL objects
										if (g.entityrubberbandlist.size() > 0)
										{
											int iEntityIndex = g.entityrubberbandlist[0].e;
											WickedSetEntityId(t.entityelement[iEntityIndex].bankindex);
											WickedSetElementId(iEntityIndex);
											Wicked_Change_Object_Material((void*)pObject, 6, &t.entityelement[iEntityIndex].eleprof,true, bAnyUseFPESettings);

											if (ImGui::Checkbox("Always use Original Object Settings##3", &bAnyUseFPESettings))
											{
												for (int ii = 0; ii < g.entityrubberbandlist.size(); ii++)
												{
													int ee = g.entityrubberbandlist[ii].e;
													t.entityelement[ee].eleprof.bUseFPESettings = bAnyUseFPESettings;
													if (bAnyUseFPESettings)
													{
														//PE: Copy material from master object.
														int iMasterID = t.entityelement[ee].bankindex;
														if (iMasterID > 0 && iMasterID < t.entityprofile.size())
														{
															sObject* pMasterObject = g_ObjectList[g.entitybankoffset + iMasterID];
															if (pMasterObject)
															{
																Wicked_Copy_Material_To_Grideleprof((void*)pMasterObject, 0, &t.entityelement[ee].eleprof);
																if (t.entityprofile[iMasterID].WEMaterial.dwBaseColor[0] == -1)
																	SetObjectDiffuse(iActiveObj, Rgb(255, 255, 255));
																Wicked_Set_Material_From_grideleprof((void*)pObject, 0, &t.entityelement[ee].eleprof);
																t.entityelement[ee].eleprof.WEMaterial.MaterialActive = false;
															}
														}
													}
												}
											}

											WickedSetEntityId(-1);
											WickedSetElementId(0);
										}
									}
									ImGui::Indent(-10);
								}
							}
						}

						// Moved into its own settings.
						if (pref.iEnableDeveloperObjectTools)
						{
							char title[24] = "Developer Settings##2";

							if (pref.bAutoClosePropertySections && iLastOpenHeader != 18)
								ImGui::SetNextItemOpen(false, ImGuiCond_Always);

							//Need pref. default closed.
							if (ImGui::StyleCollapsingHeader(title, ImGuiTreeNodeFlags_None)) //ImGuiTreeNodeFlags_DefaultOpen
							{
								iLastOpenHeader = 18;
								DisplayFPEAdvanced(false, iMasterID, &t.entityelement[iEntityIndex].eleprof, iEntityIndex);
							}
						}
					}
				}

				// entity lock/unlock
				if ( bClickedTheLockUnlockButton == true )
				{
					if (iEntityIndex > 0)
					{
						int iLoopMax = 1;
						if (g.entityrubberbandlist.size() > 0) iLoopMax = g.entityrubberbandlist.size();
						for (int i = 0; i < iLoopMax; i++)
						{
							// get entity index
							int e = iEntityIndex;
							if (g.entityrubberbandlist.size() > 0)
								e = g.entityrubberbandlist[i].e;

							// toggle lock flag
							t.entityelement[e].editorlock = 1 - t.entityelement[e].editorlock;

							sObject* pObject;
							if (t.entityelement[e].obj > 0)
							{
								pObject = g_ObjectList[t.entityelement[e].obj];
								if (pObject)
								{
									if (t.entityelement[e].editorlock)
									{
										#ifndef ALLOWSELECTINGLOCKEDOBJECTS
										WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_CURSOROBJECT);
										#endif
										sRubberBandType vEntityLockedItem;
										vEntityLockedItem.e = e;
										vEntityLockedList.push_back(vEntityLockedItem);
									}
									else 
									{
										//Delete from list.
										for (int i = 0; i < vEntityLockedList.size(); i++)
										{
											if (vEntityLockedList[i].e == e) 
											{
												vEntityLockedList.erase(vEntityLockedList.begin() + i);
												break;
											}
										}
										WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
									}
								}
							}
						}
						iLastActiveEntityIndex = -1;
						iLastActiveObj = -1;

						// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
						t.widget.pickedObject = 0;
					}
				}
				//##############################
				//#### Grid/Editor Settings ####
				//##############################
				// grid and alignment moved here from above (no longer need a host object)
				if (pref.iSmallToolbar == 0)
				{
					// older system (never had Y axis), so force this scenario to restore legacy behavior if using old grid settings method!
					pref.fEditorGridOffsetY = 0;
					pref.fEditorGridSizeY = 0;

					if (pref.bAutoClosePropertySections && iLastOpenHeader != 15)
						ImGui::SetNextItemOpen(false, ImGuiCond_Always);

					if (ImGui::StyleCollapsingHeader("Grid and Alignment Settings", ImGuiTreeNodeFlags_None))
					{
						iLastOpenHeader = 15;
						ImGui::Indent(10);

						//static bool bEditorGridFitObjectSize = false;
						static int iGridOffsetMode = 0;
						if (t.gridentitygridlock > 0)
						{
							if (t.gridentitygridlock == 2)
								iGridOffsetMode = 0;
							else
								iGridOffsetMode = 1;
						}
						else
						{
							iGridOffsetMode = 0;
						}
						bool bGridEnabled = pref.iGridEnabled;
						if (ImGui::Checkbox("Enable Grid Mode", &bGridEnabled))
						{
							pref.iGridEnabled = bGridEnabled;
						}
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select to snap the object to an aligned position");
						if (pref.iGridEnabled == false)
						{
							t.gridentitygridlock = 0;
							pref.iGridMode = t.gridentitygridlock;
						}
						if (pref.iGridEnabled == true)
						{
							ImGui::RadioButton("Use Grid Positions", &iGridOffsetMode, 0);
							if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snaps the selected object to the chosen grid positions");
							ImGui::RadioButton("Snap Mode", &iGridOffsetMode, 1);
							if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap the object to the nearest object bound box");
							t.gridentitygridlock = 2 - iGridOffsetMode;
							pref.iGridMode = t.gridentitygridlock;
						}

						// grid size only avaiulable in advanced mode
						ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
						if (pref.iObjectEnableAdvanced)
						{
							if (t.gridentitygridlock == 2)
							{
								ImGui::TextCenter("Grid Offset");
								float w = ImGui::GetContentRegionAvail().x - 10.0f;
								float inputsize = w / 2.0f;
								inputsize -= 10.0f; //For text.
								inputsize -= 5.0f; //For padding.
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(18.0f, 3.0f));
								ImGui::Text("X");
								ImGui::SameLine();
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -3.0f));
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								ImGui::InputFloat("##XYZgridoffsetX", &pref.fEditorGridOffsetX, 0.0f, 0.0f, "%.1f");
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Grid Offset X");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								ImGui::Text("Z");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								ImGui::InputFloat("##XYZgridoffsetZ", &pref.fEditorGridOffsetZ, 0.0f, 0.0f, "%.1f");
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Grid Offset Z");
								ImGui::PopItemWidth();
								ImGui::TextCenter("Grid Size");
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(18.0f, 3.0f));
								ImGui::Text("X");
								ImGui::SameLine();
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -3.0f));
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								ImGui::InputFloat("##XYZgridsizeX", &pref.fEditorGridSizeX, 0.0f, 0.0f, "%.1f");
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Grid Size X");
								ImGui::PopItemWidth();
								ImGui::SameLine();
								ImGui::Text("Z");
								ImGui::SameLine();
								ImGui::PushItemWidth(inputsize - ImGui::GetFontSize());
								ImGui::InputFloat("##XYZgridsizeZ", &pref.fEditorGridSizeZ, 0.0f, 0.0f, "%.1f");
								if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("Change Grid Size Z");
								ImGui::PopItemWidth();

								// clever button to align grid to object (for older levels with arbitary alignments mixed together)
								if (iEntityIndex > 0 && g.entityrubberbandlist.size() == 0)
								{
									float w = ImGui::GetWindowContentRegionWidth();
									float but_gadget_size = ImGui::GetFontSize()*15.0;
									ImGui::Text("");
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
									if (ImGui::StyleButton("Align Grid Offset To Object", ImVec2(but_gadget_size, 0)))
									{
										float x = t.entityelement[iEntityIndex].x;
										float z = t.entityelement[iEntityIndex].z;
										int iSizeRoundedX = int(x / pref.fEditorGridSizeX)*pref.fEditorGridSizeX;
										pref.fEditorGridOffsetX = x - iSizeRoundedX;
										int iSizeRoundedZ = int(z / pref.fEditorGridSizeZ)*pref.fEditorGridSizeZ;
										pref.fEditorGridOffsetZ = z - iSizeRoundedZ;
									}
									ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
									if (ImGui::StyleButton("Align Grid Size To Object", ImVec2(but_gadget_size, 0)))
									{
										float sx = ObjectSizeX(t.entityelement[iEntityIndex].obj, 1);
										float sz = ObjectSizeZ(t.entityelement[iEntityIndex].obj, 1);
										pref.fEditorGridSizeX = sx;
										pref.fEditorGridSizeZ = sz;
									}
								}

								// can never have a grid size below one
								if (pref.fEditorGridSizeX <= 0.1f) pref.fEditorGridSizeX = 0.1f;
								if (pref.fEditorGridSizeZ <= 0.1f) pref.fEditorGridSizeZ = 0.1f;
							}
						}

						// button to unlock any objects in locked list
						if (vEntityLockedList.size() > 0)
						{
							float w = ImGui::GetWindowContentRegionWidth();
							float but_gadget_size = ImGui::GetFontSize()*10.0;
							ImGui::Text("");
							ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
							cStr unlockstr = cStr("Unlock ") + cStr((int)vEntityLockedList.size()) + cStr(" Objects");
							if (ImGui::StyleButton(unlockstr.Get(), ImVec2(but_gadget_size, 0))) 
							{
								for (int i = 0; i < vEntityLockedList.size(); i++)
								{
									int e = vEntityLockedList[i].e;
									if (e < 0 || e >= t.entityelement.size()) continue;

									t.entityelement[e].editorlock = 0;
									sObject* pObject;
									if (t.entityelement[e].obj > 0) 
									{
										pObject = g_ObjectList[t.entityelement[e].obj];
										if (pObject) 
										{
											WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
										}
									}
								}
								vEntityLockedList.clear();

								// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
								t.widget.pickedObject = 0;
							}
						}
						ImGui::Indent(-10);
						ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
					}
				}

				// detect ANY change inside entityelement (inc eleprof) so can trigger instance cloing of collectables
				if (iEntityIndex > 0 && bSnappedEntityElementCopy == true)
				{
					bool bEntityElementChanged = false;
					char* pMemDataOld = (char*)&snapshotentityelement;
					char* pMemDataNew = (char*)&t.entityelement[iEntityIndex].eleprof;
					int iWhereWeDetectedChange = 0;
					for (int n = 0; n < sizeof(entityeleproftype); n++)
					{
						if (*pMemDataOld != *pMemDataNew)
						{
							// change in entity element or eleprof detected
							iWhereWeDetectedChange = n;
							bEntityElementChanged = true;
							break;
						}
						pMemDataOld++;
						pMemDataNew++;
					}
					if (bEntityElementChanged == true)
					{
						// now scan all entities in common with this entity, and clone all details
						// to them (there should only be one collectale entity element/eleprof identity)
						if (t.entityelement[iEntityIndex].eleprof.iscollectable != 0)
						{
							int thismasterid = t.entityelement[iEntityIndex].bankindex;
							LPSTR pMasterEntityName = t.entityelement[iEntityIndex].eleprof.name_s.Get();
							for (int ee = 1; ee <= g.entityelementmax; ee++)
							{
								if (ee != iEntityIndex)
								{
									int masterid = t.entityelement[ee].bankindex;
									if (masterid > 0)
									{
										// and essentially, ONLY copy into the SAME PARENT ID object (not ANYTHING named so!)
										if (thismasterid == masterid)
										{
											if (stricmp (t.entityelement[ee].eleprof.name_s.Get(), pMasterEntityName) == NULL)
											{
												// clone currently edited entity
												t.entityelement[ee].eleprof = t.entityelement[iEntityIndex].eleprof;
												sObject* pObject = GetObjectData(t.entityelement[ee].obj);
												if (pObject)
												{
													WickedSetEntityId(masterid);
													WickedSetElementId(ee);
													for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
													{
														Wicked_Set_Material_From_grideleprof_ThisMesh (pObject, 0, &t.entityelement[ee].eleprof, iMesh);
													}
													WickedSetEntityId(-1);
													WickedSetElementId(0);
												}
											}
										}
									}
								}
							}
						}

						// any entity change should invalidate project change flag so can save even small changes
						g.projectmodified = 1;
					}
				}

				// tutorial component
				if (!pref.bHideTutorials)
				{
					// Default to tutorial panel if no object is selected.
					// if (Entity_Tools_Window && !g_selected_editor_object && !Visuals_Tools_Window && iLastOpenHeader != 20)
					//LB: can keep tutorial closed now even if no object selected 
					if (Entity_Tools_Window && !g_selected_editor_object && !Visuals_Tools_Window && iLastOpenHeader != 15 && iLastOpenHeader != 16 && iLastOpenHeader != 8 && iLastOpenHeader != 9 && iLastOpenHeader != 20 && sGotoPreviewWithFile.Len() == 0) // 20 is keyboard shortcxuts, 15 is grid component
						iLastOpenHeader = 19;

					if (pref.bAutoClosePropertySections && iLastOpenHeader != 19)
						ImGui::SetNextItemOpen(false, ImGuiCond_Always);
					
					if (ImGui::StyleCollapsingHeader("Tutorial", ImGuiTreeNodeFlags_DefaultOpen))
					{
						iLastOpenHeader = 19;

						ImGui::Indent(10);
						cstr cShowTutorial = "0201 - Level Editing";
						char* tutorial_combo_items[] = { "0201 - Level Editing", "0301 - Object Library", "0401 - Object Grouping", "0601 - Terrain Editing", "0202 - Particle Editor", "0203 - The Animation Library", "0801 - Character Creator", "0901 - Behaviour AI" };
						SmallTutorialVideo(cShowTutorial.Get(), tutorial_combo_items, ARRAYSIZE(tutorial_combo_items), SECTION_ENTITY_TOOLS, true );
						float but_gadget_size = ImGui::GetFontSize()*12.0;
						float w = ImGui::GetWindowContentRegionWidth() - 10.0;
						ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
						#ifdef INCLUDESTEPBYSTEP
						if (ImGui::StyleButton("View Step by Step Tutorial", ImVec2(but_gadget_size, 0)))
						{
							// pre-select tutorial 03
							bHelp_Window = true;
							bHelpVideo_Window = true;
							bSetTutorialSectionLeft = false;
							strcpy(cForceTutorialName, cShowTutorial.Get());
						}
						if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start Step by Step Tutorial");
						#endif
						ImGui::Indent(-10);
					}
				}

				// insert a keyboard shortcut component into panel
				UniversalKeyboardShortcut(eKST_ObjectMode);

				if (bRunExtractDuplicate) 
				{
					t.tentitytoselect = iEntityIndex;
					t.widget.duplicatebuttonselected = 0;
					t.gridentityautofind = 7;

					t.widget.pickedObject = 0;
					widget_updatewidgetobject();

					t.onetimeentitypickup = 1;

					//  extract entity from the map
					if (t.tentitytoselect > 0)
					{
						if (t.entityelement[t.tentitytoselect].editorfixed == 0)
						{
							fExtractYValue = t.entityelement[t.tentitytoselect].y;

							t.gridentityeditorfixed = t.entityelement[t.tentitytoselect].editorfixed;
							t.gridentity = t.entityelement[t.tentitytoselect].bankindex;
							t.ttrygridentitystaticmode = t.entityelement[t.tentitytoselect].staticflag;
							t.ttrygridentity = t.gridentity; editor_validatestaticmode();
							t.gridedit.autoflatten = t.entityprofile[t.gridentity].autoflatten;
							t.gridedit.entityspraymode = 0;
							if (t.gridentityautofind == 7)
							{
								//  widget extracts without forcing entity to Floor
								t.gridentityautofind = 0;
								t.gridentityposoffground = 1;
								t.gridentityusingsoftauto = 0;
							}
							else
							{
								t.gridentityposoffground = 0;
								t.gridentityusingsoftauto = 1;
								// MAX handles its own positioning system
								{
									t.gridentityautofind = 0;
								}
							}
							t.gridentitysurfacesnap = 0; // surfacesnap off as messes up extract offset for entity
							t.gridentityextractedindex = t.tentitytoselect;
							t.gridentityhasparent = 0;//t.entityelement[t.tentitytoselect].iHasParentIndex; 210317 - break association when extract so can place free of parent
							t.gridentityposx_f = t.entityelement[t.tentitytoselect].x;
							t.gridentityposy_f = t.entityelement[t.tentitytoselect].y;
							t.gridentityposz_f = t.entityelement[t.tentitytoselect].z;
							t.gridentityrotatex_f = t.entityelement[t.tentitytoselect].rx;
							t.gridentityrotatey_f = t.entityelement[t.tentitytoselect].ry;
							t.gridentityrotatez_f = t.entityelement[t.tentitytoselect].rz;
							t.gridentityrotatequatmode = t.entityelement[t.tentitytoselect].quatmode;
							t.gridentityrotatequatx_f = t.entityelement[t.tentitytoselect].quatx;
							t.gridentityrotatequaty_f = t.entityelement[t.tentitytoselect].quaty;
							t.gridentityrotatequatz_f = t.entityelement[t.tentitytoselect].quatz;
							t.gridentityrotatequatw_f = t.entityelement[t.tentitytoselect].quatw;
							if (t.entityprofile[t.gridentity].ismarker == 10)
							{
								t.gridentityscalex_f = 100.0f + t.entityelement[t.tentitytoselect].scalex;
								t.gridentityscaley_f = 100.0f + t.entityelement[t.tentitytoselect].scaley;
								t.gridentityscalez_f = 100.0f + t.entityelement[t.tentitytoselect].scalez;
							}
							else
							{
								t.gridentityscalex_f = ObjectScaleX(t.entityelement[t.tentitytoselect].obj);
								t.gridentityscaley_f = ObjectScaleY(t.entityelement[t.tentitytoselect].obj);
								t.gridentityscalez_f = ObjectScaleZ(t.entityelement[t.tentitytoselect].obj);
							}
							t.grideleprof = t.entityelement[t.tentitytoselect].eleprof;
							t.grideleproflastname_s = t.grideleprof.name_s;

							//  Transfer any waypoint association
							t.waypointindex = t.entityelement[t.tentitytoselect].eleprof.trigger.waypointzoneindex;
							t.grideleprof.trigger.waypointzoneindex = t.waypointindex;
							t.waypoint[t.waypointindex].linkedtoentityindex = 0;
							
							if (!bDuplicate) 
							{
								iLastEntityOnCursor = t.tentitytoselect;
								gridedit_deleteentityfrommap();
							}
							else
							{
								iLastEntityOnCursor = 0;
							}

							t.refreshgrideditcursor = 1;

							// remove entity index from rubber band selection
							for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
								if (g.entityrubberbandlist[i].e == t.tentitytoselect)
									g.entityrubberbandlist[i].e = 0;

							//Just place under cursor.
							t.inputsys.dragoffsetx_f = 0;
							t.inputsys.dragoffsety_f = 0;
						}
					}
				}

				CheckMinimumDockSpaceSize(250.0f);

				//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
				if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0)
				{
					ImGui::Text("");
					ImGui::Text("");
				}

				//PE: End child here if active
				if (bWithNoScrollbar)
				{
					ImGui::EndChild();
				}

				ImGui::End();
			}
		}

		//############################
		//#### Save To Level Cloud ###
		//############################

		//########################
		//#### Download Store ####
		//########################

		//Waypoints##WaypointsToolsWindow
		if (refresh_gui_docking == 0 && !bDownloadStore_Window) 
		{
			//Make sure window is setup in docking space.
			ImGui::SetNextWindowSize(ImVec2(40 * ImGui::GetFontSize(), 30 * ImGui::GetFontSize()), ImGuiCond_Once);
			ImGui::SetNextWindowPosCenter(ImGuiCond_Once);

			ImGui::Begin("Game Creator Store Downloader##DownloadStoreWindow", &bDownloadStore_Window, 0);
			ImGui::End();
		}
		imgui_download_store();

		//#######################
		//#### Shooter Tools ####
		//#######################
		if (Shooter_Tools_Window )
		{
			//PE: Need own docking window.
			//LB: Shooter now a filter mode 
			//ImGui::Begin("Shooter Genre##GameLogicTools", &Shooter_Tools_Window, 0);
			//imgui_shooter_tools();
			//ImGui::End();
			iIncludeLeftIconSet = 1;
			g_bDotsAreVisible = true;
			if (t.showeditorelements == 1)
			{
				g_bDotsAreVisible = true;
				DrawLogicNodes(true);
			}
		}
		else 
		{
			if(iIncludeLeftIconSet == 1) iIncludeLeftIconSet = 0;
			Shooter_Tools_Window_Active = false;
			if (g_bDotsAreVisible)
			{
				DrawLogicNodes(false);
				g_bDotsAreVisible = false;
			}
		}

		//########################
		//#### Weather Window ####
		//########################

		//PE: Game Settings window.
		if (refresh_gui_docking == 0 && !Game_Settings_Window)
		{
			//Make sure window is setup in docking space.
			bool bTrue = true;
			ImGui::Begin("Game Settings##GameSettings", &bTrue, iGenralWindowsFlags);
			ImGui::End();
		}
		else
		{
			if (Game_Settings_Window)
			{
				ImGui::Begin("Game Settings##GameSettings", &Game_Settings_Window, 0);
				imgui_Customize_Game_Settings(3);
				ImGui::End();

			}
		}

		if (refresh_gui_docking == 0 && !Logic_Settings_Window)
		{
			//Make sure window is setup in docking space.
			bool bTrue = true;
			ImGui::Begin("Logic Settings##LogicSettings", &bTrue, iGenralWindowsFlags);
			ImGui::End();
		}
		else
		{
			if (Logic_Settings_Window)
			{
				ImGui::Begin("Logic Settings##LogicSettings", &Logic_Settings_Window, 0);

				// LB: inserted shooter properties at top of Object Tools if filter mode active
				if (Shooter_Tools_Window)
				{
					//PE: Looks like pref.iEnableRelationPopupWindow is not available anymore , but just in case :)
					imgui_shooter_tools();
				}

				imgui_Customize_Logic_Settings(3);
				ImGui::End();

			}
		}

		void ProcessQuestEditor(void);
		ProcessQuestEditor();


		//#####################
		//#### Preferences ####
		//#####################

		ProcessPreferences();

		//########################
		//#### Waypoint Tools ####
		//########################

		//Waypoints##WaypointsToolsWindow
		if (refresh_gui_docking == 0 && !bWaypoint_Window) 
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Waypoints##WaypointsToolsWindow", &bWaypoint_Window, iGenralWindowsFlags);
			ImGui::End();
		}
		waypoint_imgui_loop();

		//#######################
		//#### Terrain Tools ####
		//#######################
		static bool bTerrainToolsDocked = false;

		if (!bTerrainToolsDocked || (refresh_gui_docking == 0 && !bTerrain_Tools_Window ))
		{
			//Make sure window is setup in docking space.
			bool bTrue = true;
			ImGui::Begin("Terrain Tools##TerrainToolsWindow", &bTerrain_Tools_Window, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Sculpt Terrain##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Paint Terrain##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Add Vegetation##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Terrain Tools##Sculpt Terrain##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Terrain Tools##Paint Terrain##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Terrain Tools##Add Vegetation##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Terrain Tools##Add Trees##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Terrain Tools##Add Bushes##TerrainToolsWindow", &bTrue, iGenralWindowsFlags);
			ImGui::End();
		
			bTerrainToolsDocked = true;
		}
		else
		{
			imgui_terrain_loop_v3(); //PE: New design for Paul's new terrain system :)
		}

		//############################
		//#### Builder Properties ####
		//############################

		if (refresh_gui_docking == 0 && !bBuilder_Properties_Window) 
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Structure Properties##BuilderPropertiesWindow", &bBuilder_Properties_Window, iGenralWindowsFlags);
			ImGui::End();
		}
		else {
			if(!bBuilder_Properties_Window)
				if (t.ebe.on == 1) ebe_hide();
		}
		imgui_ebe_loop();


		//###########################
		//#### Character Creator ####
		//###########################

		if (refresh_gui_docking == 0 && !g_bCharacterCreatorPlusActivated) 
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Character Creator##PropertiesWindow", &g_bCharacterCreatorPlusActivated, iGenralWindowsFlags);
			ImGui::End();
		}
		charactercreatorplus_imgui_v3();

		//################################
		//#### Building Editor - 2022 ####
		//################################


		wi::profiler::EndRange(clSeg); clSeg = wi::profiler::BeginRangeCPU("CL-EntityProps");
		//###########################
		//#### Entity Properties ####
		//###########################
			
		static int iOldPickedEntityIndex = -1;

		if (refresh_gui_docking == 0) ImGui::SetNextWindowPos(viewPortPos + ImVec2(400, 140), ImGuiCond_Always); //ImGuiCond_FirstUseEver,ImGuiCond_Once
		if (refresh_gui_docking == 0) ImGui::SetNextWindowSize(ImVec2(30 * ImGui::GetFontSize(), 40 * ImGui::GetFontSize()), ImGuiCond_Always); //ImGuiCond_FirstUseEver

		if (refresh_gui_docking == 0) 
		{
			//Need to be here while first time docking.
			ImGui::Begin("Entity Properties##PropertiesWindow", &bEntity_Properties_Window, iGenralWindowsFlags);
			ImGui::End();
		}
		else if (bEntity_Properties_Window) {
				

			if (t.widget.pickedEntityIndex > 0 && t.cameraviewmode == 2) {

				//We are in properties mode.
				grideleprof_uniqui_id = 35000;

				static int current_loaded_script = -1;
				static int current_selected_script = 0;
				static bool current_loaded_script_has_dlua = false;

				int iParentEntid = t.ttrygridentity;
					
				if (iOldPickedEntityIndex != t.widget.pickedEntityIndex) 
				{
					//PE: Setup "Materials" defaults.
					//PE: NOTE: we also need a way to get this info from t.entityelement[t.e].eleprof.WEMaterial
					//PE: should also be placed inside t.grideleprof
					int picked_object = t.gridentityobj;
					if (t.gridentityobj > 0 && t.gridentityobj < g_iObjectListCount ) 
					{
						if (g_ObjectList[t.gridentityobj])
						{
							if(t.grideleprof.WEMaterial.MaterialActive)
								Wicked_Set_Material_From_grideleprof(g_ObjectList[t.gridentityobj], 0);
							else
								Wicked_Set_Material_Defaults(g_ObjectList[t.gridentityobj], 0);
						}
					}

					iOldPickedEntityIndex = t.widget.pickedEntityIndex;

					//t.gridentity can be changed when keys like "r" is used , so make a backup.
					iOldgridentity = t.gridentity;

					// get voices sets
					if (g_voiceList_s.size() == 0) 
					{
						if (CreateListOfVoices() > 0) 
						{
							pCCPVoiceSet = g_voiceList_s[0].Get();
							CCP_SelectedToken = g_voicetoken[0];
						}
					}

					// entity may have voice preferences set to check that
					pCCPVoiceSet = t.grideleprof.voiceset_s.Get();
					CCP_Speak_Rate = t.grideleprof.voicerate;
					if (strlen(pCCPVoiceSet) > 0)
					{
						// find token for this voiceset
						for (int n = 0; n < g_voiceList_s.size(); n++)
						{
							if (stricmp(g_voiceList_s[n].Get(), pCCPVoiceSet) == NULL)
							{
								CCP_SelectedToken = g_voicetoken[n];
								break;
							}
						}
					}
					else
					{
						// default if blank
						if ( g_voiceList_s.size() > 0 ) 
							pCCPVoiceSet = g_voiceList_s[0].Get();
						else
							pCCPVoiceSet = "";
						if (g_voicetoken.size() > 0)
							CCP_SelectedToken = g_voicetoken[0];
						else
							CCP_SelectedToken = NULL;
						CCP_Speak_Rate = 0;
					}

					//Make sure to read DLUA.
					current_loaded_script = -1;
				}

				g_selected_editor_object = NULL;
				g_selected_editor_objectID = 0;
				if (iOldgridentity > 0) 
				{
					int picked_object = t.gridentityobj;
					if (picked_object > 0) 
					{
						if (picked_object < g_iObjectListCount)
						{
							if (g_ObjectList[picked_object])
							{
								if(t.gridentitystaticmode)
									g_selected_editor_color = XMSTATICCOLOR;
								else
									g_selected_editor_color = XMDYNAMICCOLOR;
								g_selected_editor_object = g_ObjectList[picked_object];
								g_selected_editor_objectID = picked_object;
							}
						}
					}
				}

				ImGui::Begin("Entity Properties##PropertiesWindow", &bEntity_Properties_Window, iGenralWindowsFlags);

				int media_icon_size = 64;
				ImGui::BeginChild("##cEntitiesPropertiesHeader", ImVec2(0, 0),false, iGenralWindowsFlags);
					
				ImGui::SetWindowFontScale(0.90);
				ImGui::PushItemWidth(ImGui::GetFontSize()*10.0);

				if (bEntity_Properties_Window)
				{
					t.gridentity = iOldgridentity;

					//Collect the flags to use.
					imgui_set_openproperty_flags(t.gridentity);
					int tflagtext = 0, tflagimage=0;

					LPSTR pAIRoot = "scriptbank\\";
					if (t.tflagai == 1)
					{
						if (g.quickparentalcontrolmode == 2)
						{
							if (t.entityprofile[t.gridentity].ismarker == 0)
							{
								if (t.tflagchar == 1)
									pAIRoot = "scriptbank\\people\\";
								else
									pAIRoot = "scriptbank\\objects\\";
							}
							else
							{
								pAIRoot = "scriptbank\\markers\\";
							}
						}
					}
						

					if (t.tsimplecharview == 1)
					{
						//  Wizard (simplified) property editing
						t.group = 0;
						if (ImGui::StyleCollapsingHeader("Character Info", ImGuiTreeNodeFlags_DefaultOpen)) {

							t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), t.strarr_s[413].Get(), "Choose a unique name for this character");
							t.grideleprof.aimain_s = imgui_setpropertylist2(t.group, t.controlindex, t.grideleprof.aimain_s.Get(), "Behaviour", "Select a behaviour for this character", 11);
							t.grideleprof.soundset1_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset1_s.Get(), "Voiceover", "Select t.a WAV or OGG file this character will use during their behavior", "audiobank\\");
							t.grideleprof.ifused_s = imgui_setpropertystring2(t.group, t.grideleprof.ifused_s.Get(), "If Used", "Sometimes used to specify the name of an entity to be activated");
						}
					}
					else
					{

						bool bUnfoldAdvanced = false;
						ImGui::SetWindowFontScale(1.0);
						float fRegionWidth = ImGui::GetWindowContentRegionWidth();
						float textwidth;
						fPropertiesColoumWidth = 90.0f;
						int adv_flasgs = ImGuiTreeNodeFlags_DefaultOpen;
						if (g.vrqcontrolmode > 0) 
						{
							//Simple version.
							adv_flasgs = ImGuiTreeNodeFlags_None;
								
							//##################################################################
							//#### Simple , perhaps based on current .lua script.           ####
							//#### We need unique id here so add ##SimpleInput to all items ####
							//##################################################################
								
							if (t.entityprofile[t.gridentity].ischaracter > 0) {
								//Chars.

								if (ImGui::StyleCollapsingHeader("Character Properties", ImGuiTreeNodeFlags_DefaultOpen)) {

									//Display icon.
									if (t.entityprofile[iParentEntid].iThumbnailSmall > 0) {
										float w = ImGui::GetWindowContentRegionWidth();
										ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
										ImGui::ImgBtn(t.entityprofile[iParentEntid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
									}

									ImGui::Indent(10);

									t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());
									//bSoundSet , Male/Female
									//PE: Type removed.
									//t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), "Type", t.strarr_s[255].Get(), "audiobank\\voices\\");

									ImGui::Indent(-10);
								}

								int speech_entries = 0;
								bool bUpdateMainString = false;

								for (int speech_loop = 0; speech_loop < 5; speech_loop++)
									speech_ids[speech_loop] = -1;
								//behavior
								if (ImGui::StyleCollapsingHeader("Character Behavior", ImGuiTreeNodeFlags_DefaultOpen)) {

									ImGui::Indent(10);

									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text("Behaviors");
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));

									ImGui::PushItemWidth(-10);

									// scan PEOPLE folder for complete list of script
									std::vector<cstr> scriptList_s; scriptList_s.clear();
									std::vector<cstr> scriptListTitle_s; scriptListTitle_s.clear();
									cstr oldDir_s = GetDir();
									SetDir(g.fpscrootdir_s.Get());
									SetDir("Files\\scriptbank\\people");
									ChecklistForFiles();
									for ( int f = 1; f <= ChecklistQuantity(); f++)
									{
										cstr tfile_s = ChecklistString(f);
										LPSTR pFilename = tfile_s.Get();
										if (tfile_s != "." && tfile_s != "..")
										{
											if (strnicmp(pFilename + strlen(pFilename) - 4, ".lua", 4) == NULL)
											{
												// create a readable title from file
												char pTitleName[256];
												strcpy(pTitleName, pFilename);
												pTitleName[strlen(pTitleName) - 4] = 0;
												for (int n = 0; n < strlen(pTitleName); n++)
												{
													if (n == 0)
													{
														if (pTitleName[n] >= 'a' && pTitleName[n] <= 'z' )
															pTitleName[n] -= ('a' - 'A');
													}
													else
													{
														if (pTitleName[n] >= 'A' && pTitleName[n] <= 'Z' )
															pTitleName[n] += ('a' - 'A');
													}
													if (pTitleName[n] == '_') pTitleName[n] = ' ';
												}

												// add script and title to list
												scriptList_s.push_back(cstr("people\\")+tfile_s);
												scriptListTitle_s.push_back(cstr(pTitleName));
											}
										}
									}
									scriptList_s.push_back(cstr(""));
									scriptListTitle_s.push_back(cstr("Custom"));
									SetDir(oldDir_s.Get());

									// and create items list
									static int g_scriptpeople_item_count = 0;
									static char** g_scriptpeople_items = NULL;
									if (g_scriptpeople_item_count != scriptList_s.size())
									{
										if (g_scriptpeople_items)
										{
											for (int i = 0; i < g_scriptpeople_item_count; i++) SAFE_DELETE(g_scriptpeople_items[i]);
											SAFE_DELETE(g_scriptpeople_items);
										}
										g_scriptpeople_item_count = scriptList_s.size();
										g_scriptpeople_items = new char*[g_scriptpeople_item_count];
										for (int i = 0; i < g_scriptpeople_item_count; i++)
										{
											g_scriptpeople_items[i] = new char[256];
											strcpy(g_scriptpeople_items[i], scriptListTitle_s[i].Get());
										}
									}

									int item_current_type_selection = g_scriptpeople_item_count - 1; //Default Custom.
									for (int i = 0; i < g_scriptpeople_item_count - 1; i++) 
									{
										if (pestrcasestr(t.grideleprof.aimain_s.Get(), scriptList_s[i].Get())) 
										{
											item_current_type_selection = i;
											break;
										}
									}

									if (current_loaded_script != item_current_type_selection) 
									{
										//Load in lua and check for custom properties.
										cstr script_name_append = "";
										if (item_current_type_selection < g_scriptpeople_item_count - 1)
											script_name_append += (char *) scriptList_s[item_current_type_selection].Get();
										else
											script_name_append += t.grideleprof.aimain_s;

										cstr script_name = "";
										script_name = "scriptbank\\";
										script_name += script_name_append;

										//Try to parse script.
										ParseLuaScript(&t.grideleprof,script_name.Get());
										current_loaded_script = item_current_type_selection;

										if (t.grideleprof.PropertiesVariableActive == 1) 
										{
											bUpdateMainString = true;
											current_loaded_script_has_dlua = true;
										}
										else 
										{
											if (current_loaded_script_has_dlua) 
											{
												//Reset t.grideleprof.soundset4_s that contain the dlua calls.
												t.grideleprof.soundset4_s = "";
												current_loaded_script_has_dlua = false;
											}
										}
									}

									if (ImGui::Combo("##BehavioursSimpleInput", &item_current_type_selection, g_scriptpeople_items, g_scriptpeople_item_count, 20)) 
									{
										if (item_current_type_selection < g_scriptpeople_item_count - 1) 
										{
											t.grideleprof.aimain_s = scriptList_s[item_current_type_selection].Get();
										}
										else 
										{
											t.grideleprof.aimain_s = "";
										}
									}
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Character Behavior");

									ImGui::PopItemWidth();

									if (item_current_type_selection == g_scriptpeople_item_count - 1) 
									{
										//Custom script , display directly.
										std::string ms = t.strarr_s[417].Get();
										ms = "Script";
										cstr aim = t.grideleprof.aimain_s;
										t.grideleprof.aimain_s = imgui_setpropertyfile2(t.group, t.grideleprof.aimain_s.Get(), (char *)ms.c_str(), t.strarr_s[207].Get(), pAIRoot);
										if (aim != t.grideleprof.aimain_s)
											current_loaded_script = -1;
									}

									if (t.grideleprof.PropertiesVariableActive == 1) {
										speech_entries = DisplayLuaDescription(&t.grideleprof);
									}
									else {
										if (t.grideleprof.PropertiesVariable.VariableDescription.Len() > 0) {
											DisplayLuaDescriptionOnly(&t.grideleprof);
										}
									}

									ImGui::Indent(-10);
								}

								if(speech_entries > 0)
								{
									//@Lee all SPEECH control is moved to this function.
									SpeechControls(speech_entries, bUpdateMainString);
								}

								if (ImGui::StyleCollapsingHeader("Customize", ImGuiTreeNodeFlags_DefaultOpen)) {

									ImGui::Indent(10);

									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text("Move Speed");
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
									ImGui::PushItemWidth(-10);
									ImGui::MaxSliderInputInt("##Movement SpeedSimpleInput", &t.grideleprof.speed, 1, 500, "Set Movement Speed");

									if (t.playercontrol.thirdperson.enabled == 1) t.tanimspeed_f = t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.animspeed;
									else t.tanimspeed_f = t.grideleprof.animspeed;

									ImGui::PopItemWidth();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text("Anim Speed");
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
									ImGui::PushItemWidth(-10);
									int tmpint = t.tanimspeed_f;
									ImGui::MaxSliderInputInt("##Animation Speed Simple", &tmpint, 1, 500, "Set Animation Speed");

									t.tanimspeed_f = tmpint;
									ImGui::PopItemWidth();
									if (t.playercontrol.thirdperson.enabled == 1) t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.animspeed = t.tanimspeed_f;
									else t.grideleprof.animspeed = t.tanimspeed_f;

									ImGui::Indent(-10);
								}
							}
							else if (t.tflaglight == 1)
							{
								if (ImGui::StyleCollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) 
								{
									//Display icon.
									if (t.entityprofile[iParentEntid].iThumbnailSmall > 0) 
									{
										float w = ImGui::GetWindowContentRegionWidth();
										ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
										ImGui::ImgBtn(t.entityprofile[iParentEntid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
									}
									ImGui::Indent(10);
									//Name and color setup only.
									ImGui::Text("");
									t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());
									ImGui::Indent(-10);
								}

								if (ImGui::StyleCollapsingHeader("Customize", ImGuiTreeNodeFlags_DefaultOpen)) {

									ImGui::Indent(10);

									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text("Light Distance");
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
									ImGui::PushItemWidth(-10);

									ImGui::SliderInt("##Light RangeSimpleInput", &t.grideleprof.light.range, 1, 3000);
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.strarr_s[250].Get() );
									ImGui::PopItemWidth();

									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text("Light Color");
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
									ImGui::PushItemWidth(-10);
									ImGui::Text(""); //place it below text, so it get larger.

									float colors[5];
									colors[3] = ((t.grideleprof.light.color & 0xff000000) >> 24) / 255.0f;
									colors[0] = ((t.grideleprof.light.color & 0x00ff0000) >> 16) / 255.0f;
									colors[1] = ((t.grideleprof.light.color & 0x0000ff00) >> 8) / 255.0f;
									colors[2] = (t.grideleprof.light.color & 0x000000ff) / 255.0f;
									ImGui::ColorPicker3("##Light ColorSimpleInput", colors, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel);
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.strarr_s[251].Get() );

									colors[0] *= 255.0f;
									colors[1] *= 255.0f;
									colors[2] *= 255.0f;
									colors[3] *= 255.0f;
									t.grideleprof.light.color = 0xff000000 + ((unsigned int)colors[0] << 16) + ((unsigned int)colors[1] << 8) + +((unsigned int)colors[2]);
									ImGui::PopItemWidth();

									// update the light live
									if (t.gridentitywickedlightindex == 0)
									{
										int iLightType = 1;
										if (t.grideleprof.usespotlighting) iLightType = 2;
										t.gridentitywickedlightindex = WickedCall_AddLight(iLightType);
									}
									if (t.gridentitywickedlightindex > 0)
									{
										float lightx = t.gridentityposx_f;
										float lighty = t.gridentityposy_f;
										float lightz = t.gridentityposz_f;
										float lightax = t.gridentityrotatex_f;
										float lightay = t.gridentityrotatey_f;
										float lightaz = t.gridentityrotatez_f;
										float lightrange = t.grideleprof.light.range;
										float lightspotradius = t.grideleprof.light.offsetup;
										int colr = colors[0];
										int colg = colors[1];
										int colb = colors[2];
										bool bCastShadow = true;
										if (t.grideleprof.castshadow == 1) bCastShadow = false;
										WickedCall_UpdateLight(t.gridentitywickedlightindex, lightx, lighty, lightz, lightax, lightay, lightaz, lightrange, lightspotradius, colr, colg, colb, bCastShadow);
									}

									ImGui::Indent(-10);
								}
							}
							else if (t.entityprofile[t.gridentity].ismarker == 1) 
{
								//Start Marker.
								if (ImGui::StyleCollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
								{
									//Display icon.
									if (t.entityprofile[iParentEntid].iThumbnailSmall > 0) {
										float w = ImGui::GetWindowContentRegionWidth();
										ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
										ImGui::ImgBtn(t.entityprofile[iParentEntid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
									}

									ImGui::Indent(10);

									//Name only.
									ImGui::Text("");
									t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());

									ImGui::Indent(-10);

								}

								// DLUA support added here.
								int speech_entries = 0;
								bool bUpdateMainString = false;
								for (int speech_loop = 0; speech_loop < 5; speech_loop++)
									speech_ids[speech_loop] = -1;
								if (current_loaded_script != current_selected_script) 
								{
									//Load in lua and check for custom properties.
									cstr script_name = "scriptbank\\";
									script_name += t.grideleprof.aimain_s;
									//Try to parse script.
									ParseLuaScript(&t.grideleprof, script_name.Get());
									current_loaded_script = current_selected_script;

									if (t.grideleprof.PropertiesVariableActive == 1) {
										bUpdateMainString = true;
										current_loaded_script_has_dlua = true;
									}
									else {
										if (current_loaded_script_has_dlua) {
											//Reset t.grideleprof.soundset4_s that contain the dlua calls.
											t.grideleprof.soundset4_s = "";
											current_loaded_script_has_dlua = false;
										}
									}

								}

								// Markers behaviours
								if (t.grideleprof.PropertiesVariableActive == 1 || t.grideleprof.PropertiesVariable.VariableDescription.Len() > 0)
								{
									if (ImGui::StyleCollapsingHeader("Behaviors", ImGuiTreeNodeFlags_DefaultOpen)) {

										ImGui::Indent(10);

										if (t.grideleprof.PropertiesVariableActive == 1) {
											speech_entries = DisplayLuaDescription(&t.grideleprof);
										}
										else {
											if (t.grideleprof.PropertiesVariable.VariableDescription.Len() > 0) {
												DisplayLuaDescriptionOnly(&t.grideleprof);
											}
										}

										ImGui::Indent(-10);
									}
								}

								if (speech_entries > 0)
								{
									// all SPEECH control is moved to this function.
									SpeechControls(speech_entries, bUpdateMainString);
								}


								if (ImGui::StyleCollapsingHeader("Customize", ImGuiTreeNodeFlags_DefaultOpen)) {

									ImGui::Indent(10);

									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text("Player Speed");
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));

									ImGui::PushItemWidth(-10);

									//ImGui::SliderInt("##Movement SpeedSimpleInput", &t.grideleprof.speed, 1, 500);
									ImGui::MaxSliderInputInt("##Movement SpeedSimpleInput", &t.grideleprof.speed, 1, 500, "Set Player Speed");
									//if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Set Player Speed");

									if (t.playercontrol.thirdperson.enabled == 1) t.tanimspeed_f = t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.animspeed;
									else t.tanimspeed_f = t.grideleprof.animspeed;

									ImGui::PopItemWidth();

									// Object Has Weapon
									if (t.tflaghasweapon == 1 && t.playercontrol.thirdperson.enabled == 0 && g.quickparentalcontrolmode != 2)
									{
										t.grideleprof.hasweapon_s = imgui_setpropertylist2c(t.group, t.controlindex, t.grideleprof.hasweapon_s.Get(), "Attachment", t.strarr_s[209].Get(), 1);
									}

									ImGui::Indent(-10);
								}
							}
							else 
							{
								//#################
								//#### Objects ####
								//################# 

								t.tokay = 1;
								if (ObjectExist(g.entitybankoffset + t.gridentity) == 1)
								{
									if (GetNumberOfFrames(g.entitybankoffset + t.gridentity) > 0)
									{
										t.tokay = 0;
									}
								}

								int speech_entries = 0;
								bool bUpdateMainString = false;

								for (int speech_loop = 0; speech_loop < 5; speech_loop++)
									speech_ids[speech_loop] = -1;

								//health.lua
								cstr aimain = t.grideleprof.aimain_s.Lower();
								//new: trigger anyting not a marker.
								if (t.entityprofile[t.gridentity].ismarker == 0 )// || aimain == "key.lua" || aimain == "objects\\key.lua" || aimain == "door.lua" || aimain == "default.lua" || aimain == "health.lua" || aimain == "pickuppable.lua" ) ) 
								{
									if (ImGui::StyleCollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
									{
										//Display icon.
										if (t.entityprofile[iParentEntid].iThumbnailSmall > 0) {
											float w = ImGui::GetWindowContentRegionWidth();
											ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
											ImGui::ImgBtn(t.entityprofile[iParentEntid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
										}

										ImGui::Indent(10);

										t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());

										ImGui::Indent(-10);
									}
										
									// Object behaviours
									if (ImGui::StyleCollapsingHeader("Behaviors", ImGuiTreeNodeFlags_DefaultOpen))
									{
										ImGui::Indent(10);

										// scan OBJECTS folder for complete list of script
										std::vector<cstr> scriptList_s; scriptList_s.clear();
										std::vector<cstr> scriptListTitle_s; scriptListTitle_s.clear();
										cstr oldDir_s = GetDir();
										SetDir(g.fpscrootdir_s.Get());
										SetDir("Files\\scriptbank\\objects");
										ChecklistForFiles();
										for ( int f = 1; f <= ChecklistQuantity(); f++)
										{
											cstr tfile_s = ChecklistString(f);
											LPSTR pFilename = tfile_s.Get();
											if (tfile_s != "." && tfile_s != "..")
											{
												if (strnicmp(pFilename + strlen(pFilename) - 4, ".lua", 4) == NULL)
												{
													// create a readable title from file
													char pTitleName[256];
													strcpy(pTitleName, pFilename);
													pTitleName[strlen(pTitleName) - 4] = 0;
													for (int n = 0; n < strlen(pTitleName); n++)
													{
														if (n == 0)
														{
															if (pTitleName[n] >= 'a' && pTitleName[n] <= 'z' )
																pTitleName[n] -= ('a' - 'A');
														}
														else
														{
															if (pTitleName[n] >= 'A' && pTitleName[n] <= 'Z' )
																pTitleName[n] += ('a' - 'A');
														}
														if (pTitleName[n] == '_') pTitleName[n] = ' ';
													}

													// add script and title to list
													scriptList_s.push_back(cstr("objects\\")+tfile_s);
													scriptListTitle_s.push_back(cstr(pTitleName));
												}
											}
										}
										scriptList_s.push_back(cstr(""));
										scriptListTitle_s.push_back(cstr("Custom"));
										SetDir(oldDir_s.Get());

										// and create items list
										static int g_scriptobjects_item_count = 0;
										static char** g_scriptobjects_items = NULL;
										if (g_scriptobjects_item_count != scriptList_s.size())
										{
											if (g_scriptobjects_items)
											{
												for (int i = 0; i < g_scriptobjects_item_count; i++) SAFE_DELETE(g_scriptobjects_items[i]);
												SAFE_DELETE(g_scriptobjects_items);
											}
											g_scriptobjects_item_count = scriptList_s.size();
											g_scriptobjects_items = new char*[g_scriptobjects_item_count];
											for (int i = 0; i < g_scriptobjects_item_count; i++)
											{
												g_scriptobjects_items[i] = new char[256];
												strcpy(g_scriptobjects_items[i], scriptListTitle_s[i].Get());
											}
										}

										// find selection
										int item_current_type_selection = g_scriptobjects_item_count - 1; //Default Custom.
										for (int i = 0; i < g_scriptobjects_item_count - 1 ; i++) 
										{
											if (pestrcasestr(t.grideleprof.aimain_s.Get(), scriptList_s[i].Get())) 
											{
												item_current_type_selection = i;
												break;
											}
										}
												
										ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
										ImGui::Text("Behaviors");
										ImGui::SameLine();
										ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
										ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
										ImGui::PushItemWidth(-10);

										if (ImGui::Combo("##Behaviours2SimpleInput", &item_current_type_selection, g_scriptobjects_items, g_scriptobjects_item_count, 20 ))
										{
											if (item_current_type_selection >= 0) 
												t.grideleprof.aimain_s = scriptList_s[item_current_type_selection];
											else 
												t.grideleprof.aimain_s = "default.lua";
											aimain = t.grideleprof.aimain_s.Lower();
										}
										if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Object Behavior");
										ImGui::PopItemWidth();

										if (current_loaded_script != item_current_type_selection) 
										{
											//Load in lua and check for custom properties.
											cstr script_name = "scriptbank\\";
											if (item_current_type_selection < g_scriptobjects_item_count - 1) //PE: Need to check for custom
												script_name += scriptList_s[item_current_type_selection];
											else
												script_name += t.grideleprof.aimain_s;
											//Try to parse script.
											ParseLuaScript(&t.grideleprof, script_name.Get());
											current_loaded_script = item_current_type_selection;

											if (t.grideleprof.PropertiesVariableActive == 1) 
											{
												bUpdateMainString = true;
												current_loaded_script_has_dlua = true;
											}
											else 
											{
												if (current_loaded_script_has_dlua) 
												{
													//Reset t.grideleprof.soundset4_s that contain the dlua calls.
													t.grideleprof.soundset4_s = "";
													current_loaded_script_has_dlua = false;
												}
											}
										}

										if (item_current_type_selection == g_scriptobjects_item_count - 1)
										{
											//Custom Behaviours , display directly.
											std::string ms = t.strarr_s[417].Get();
											ms = "Script";
											cstr aim = t.grideleprof.aimain_s;
											t.grideleprof.aimain_s = imgui_setpropertyfile2(t.group, t.grideleprof.aimain_s.Get(), (char *)ms.c_str(), t.strarr_s[207].Get(), pAIRoot);
											aimain = t.grideleprof.aimain_s.Lower();
											if (aim != t.grideleprof.aimain_s)
												current_loaded_script = -1;
										}

										if (t.grideleprof.PropertiesVariableActive == 1) 
										{
											speech_entries = DisplayLuaDescription(&t.grideleprof);
										}
										else 
										{
											if (t.grideleprof.PropertiesVariable.VariableDescription.Len() > 0) 
											{
												DisplayLuaDescriptionOnly(&t.grideleprof);
											}
										}

										ImGui::Indent(-10);
									}

									if (speech_entries > 0)
									{
										// all SPEECH control is moved to this function.
										SpeechControls(speech_entries, bUpdateMainString);
									}
								}
								else
									bUnfoldAdvanced = true;
							}
						}

						ImGui::SetWindowFontScale(1.0); //0.90

						fPropertiesColoumWidth = 120.0f; //Advanced coloum need to be larger as we have large fields.

						// All objects need 'certain fields' as pretty commmon  to have it for the script
						if (t.entityprofile[t.gridentity].ismarker == 0) // so as not to interfere with markers
						{
							bool bSound0Mentioned = false;
							bool bSound1Mentioned = false;
							bool bSound2Mentioned = false;
							bool bSound3Mentioned = false;
							bool bSound4Mentioned = false;
							bool bSound5Mentioned = false;
							bool bSound6Mentioned = false;
							bool bVideoSlotMentioned = false;
							bool bIfUsedMentioned = false;
							bool bUseKeyMentioned = false;
							bool bShootingWeaponMentioned = false;
							bool bMeleeWeaponMentioned = false;
							bool bUnarmedMentioned = false;
							int iAnimationSetMentioned = 0;
							char pCaptureAnyScriptDesc[10240 + (80 * 300) + (80 * 300)];
							strcpy(pCaptureAnyScriptDesc, t.grideleprof.PropertiesVariable.VariableDescription.Get());
							for (int i = 0; i < t.grideleprof.PropertiesVariable.iVariables; i++)
							{
								//strcat(pCaptureAnyScriptDesc, t.grideleprof.PropertiesVariable.VariableSectionDescription[i]);
								strcat(pCaptureAnyScriptDesc, t.grideleprof.PropertiesVariable.VariableSectionDescription[i].Get());
							}
							for (int i = 0; i < t.grideleprof.PropertiesVariable.iVariables; i++)
							{
								//strcat(pCaptureAnyScriptDesc, t.grideleprof.PropertiesVariable.VariableSectionEndDescription[i]);
								strcat(pCaptureAnyScriptDesc, t.grideleprof.PropertiesVariable.VariableSectionEndDescription[i].Get());
							}
							if (strstr(pCaptureAnyScriptDesc, "<Sound0>") != 0) bSound0Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Sound1>") != 0) bSound1Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Sound2>") != 0) bSound2Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Sound3>") != 0) bSound3Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Sound4>") != 0) bSound4Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Sound5>") != 0) bSound5Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Sound6>") != 0) bSound6Mentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Video Slot>") != 0) bVideoSlotMentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<If Used>") != 0) bIfUsedMentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Shooting Weapon>") != 0) bShootingWeaponMentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Melee Weapon>") != 0) bMeleeWeaponMentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Any Weapon>") != 0) { bShootingWeaponMentioned = true; bMeleeWeaponMentioned = true; }
							if (strstr(pCaptureAnyScriptDesc, "<Unarmed>") != 0) bUnarmedMentioned = true;
							if (strstr(pCaptureAnyScriptDesc, "<Soldier Animations>") != 0) iAnimationSetMentioned = 1;
							if (strstr(pCaptureAnyScriptDesc, "<Melee Animations>") != 0) iAnimationSetMentioned = 2;
							if (strstr(pCaptureAnyScriptDesc, "<Zombie Animations>") != 0) iAnimationSetMentioned = 3;
							if (strstr(pCaptureAnyScriptDesc, "<Default Animations>") != 0) iAnimationSetMentioned = 4;

							if (bVideoSlotMentioned == true) t.grideleprof.soundset1_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset1_s.Get(), "Video Slot", t.strarr_s[601].Get(), "videobank\\");
							if (bSound0Mentioned == true) t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), "Sound0", t.strarr_s[254].Get(), "audiobank\\");
							if (bSound1Mentioned == true) t.grideleprof.soundset1_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset1_s.Get(), "Sound1", t.strarr_s[254].Get(), "audiobank\\");
							if (bSound2Mentioned == true) t.grideleprof.soundset2_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset2_s.Get(), "Sound2", t.strarr_s[254].Get(), "audiobank\\");
							if (bSound3Mentioned == true) t.grideleprof.soundset3_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset3_s.Get(), "Sound3", t.strarr_s[254].Get(), "audiobank\\");
							// LB: corrected bad decision to assign sound4 to 4, 5 to 6, etc (now 4,5,6 align with the introduction of a new 4a string)
							if (bSound4Mentioned == true) t.grideleprof.soundset4a_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset4a_s.Get(), "Sound4", t.strarr_s[254].Get(), "audiobank\\");
							if (bSound5Mentioned == true) t.grideleprof.soundset5_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset5_s.Get(), "Sound5", t.strarr_s[254].Get(), "audiobank\\");
							if (bSound6Mentioned == true) t.grideleprof.soundset6_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset6_s.Get(), "Sound6", t.strarr_s[254].Get(), "audiobank\\");
							if (bIfUsedMentioned == true) t.grideleprof.ifused_s = imgui_setpropertystring2(t.group, t.grideleprof.ifused_s.Get(), t.strarr_s[437].Get(), t.strarr_s[226].Get());
							if (bUseKeyMentioned == true) t.grideleprof.usekey_s = imgui_setpropertystring2(t.group, t.grideleprof.usekey_s.Get(), t.strarr_s[436].Get(), t.strarr_s[225].Get());
							bool readonly = false;
							bool bFromCharacterCreator = false;
							if (bShootingWeaponMentioned == true || bMeleeWeaponMentioned == true)
							{
								extern void animsystem_weaponproperty (int, bool, entityeleproftype*, bool, bool);
								animsystem_weaponproperty(t.entityprofile[t.gridentity].characterbasetype, bFromCharacterCreator, &t.grideleprof, bShootingWeaponMentioned, bMeleeWeaponMentioned);
							}
							else if (bUnarmedMentioned)
							{
								extern void animsystem_weaponproperty(int, bool, entityeleproftype*, bool, bool);
								animsystem_weaponproperty(t.entityprofile[t.gridentity].characterbasetype, bFromCharacterCreator, &t.grideleprof, false, false);
							}
							if (iAnimationSetMentioned > 0)
							{
								extern void animsystem_animationsetproperty (int, bool, entityeleproftype*, int, int);
								animsystem_animationsetproperty(t.entityprofile[t.gridentity].characterbasetype, bFromCharacterCreator, &t.grideleprof, iAnimationSetMentioned, -1);
							}
						}

						sObject* pObject = NULL;
						if( t.gridentityobj > 0) pObject = g_ObjectList[t.gridentityobj];
						if (ObjectExist(g.entitybankoffset + t.gridentity) == 1 && pObject )
						{
							if (ImGui::StyleCollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
							{
								ImGui::Indent(10);
								{
									Wicked_Change_Object_Material((void*)pObject, 0, NULL , true , t.grideleprof.bUseFPESettings);

									if (ImGui::Checkbox("Always use Original Object Settings##2", &t.grideleprof.bUseFPESettings))
									{
										if (t.grideleprof.bUseFPESettings)
										{
											t.grideleprof.WEMaterial.MaterialActive = false;
										}
										else
										{
											Wicked_Copy_Material_To_Grideleprof((void*)pObject, 0);
											t.grideleprof.WEMaterial.MaterialActive = true;
										}
									}

								}
								ImGui::Indent(-10);
							}
						}

						// And finally the ADVANCED section
						bool bAdvencedOpen = false;
						if (g.vrqcontrolmode > 0) 
						{
							if(bUnfoldAdvanced)
								bAdvencedOpen = true;
							else if (ImGui::StyleCollapsingHeader("Advanced", adv_flasgs)) { //ImGuiTreeNodeFlags_None //ImGuiTreeNodeFlags_DefaultOpen
								bAdvencedOpen = true;
							}
						}
						else 
						{
							bAdvencedOpen = true;
						}
						if (bAdvencedOpen) 
						{
							t.group = 0;
							if (ImGui::StyleCollapsingHeader(t.strarr_s[412].Get(), ImGuiTreeNodeFlags_DefaultOpen)) 
							{
								if (bUnfoldAdvanced) 
								{
									//Display icon.
									if (t.entityprofile[iParentEntid].iThumbnailSmall > 0) 
									{
										float w = ImGui::GetWindowContentRegionWidth();
										ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
										ImGui::ImgBtn(t.entityprofile[iParentEntid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
									}
								}
								if (bUnfoldAdvanced)
								{
									if (t.entityprofile[t.gridentity].ischaracter > 0)
									{
										t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());
									}
									else
									{
										if (t.entityprofile[t.gridentity].ismarker > 0)
										{
											if (t.entityprofile[t.gridentity].islightmarker > 0) {
												t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());
											}
											else {
												t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());
											}
										}
										else {
											t.grideleprof.name_s = imgui_setpropertystring2(t.group, t.grideleprof.name_s.Get(), "Name", t.strarr_s[204].Get());
										}
									}
								}

								if (t.entityprofile[t.gridentity].ismarker == 0 || t.entityprofile[t.gridentity].islightmarker == 1)
								{
									if (g.gentitytogglingoff == 0)
									{
										t.tokay = 1;
										if (ObjectExist(g.entitybankoffset + t.gridentity) == 1)
										{
											if (GetNumberOfFrames(g.entitybankoffset + t.gridentity) > 0)
											{
												t.tokay = 0;
											}
										}
										if (t.tokay == 1)
										{
											t.gridentitystaticmode = imgui_setpropertylist2(t.group, t.controlindex, Str(t.gridentitystaticmode), t.strarr_s[414].Get(), t.strarr_s[205].Get(), 0);
										}
									}
								}

								// 101016 - Additional General Parameters
								if (t.tflagchar == 0 && t.tflagvis == 1)
								{
									if (t.tflagsimpler == 0)
									{
										t.grideleprof.isocluder = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.isocluder), "Occluder", "Set to YES makes this object an occluder", 0); ++t.controlindex;
										t.grideleprof.isocludee = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.isocludee), "Occludee", "Set to YES makes this object an occludee", 0); ++t.controlindex;
									}
								}

								if (ImGui::IsAnyItemFocused()) 
								{
									bImGuiGotFocus = true;
								}
							}

							int speech_entries = 0;
							//Add DLUA here id
							if (bUnfoldAdvanced && t.entityprofile[t.gridentity].ismarker > 1) {
								//ismarker = 1 has its own function.
								//DLUA support added here.
								bool bUpdateMainString = false;
								for (int speech_loop = 0; speech_loop < 5; speech_loop++)
									speech_ids[speech_loop] = -1;
								if (current_loaded_script != current_selected_script) {
									//Load in lua and check for custom properties.
									cstr script_name = "scriptbank\\";
									script_name += t.grideleprof.aimain_s;
									//Try to parse script.
									ParseLuaScript(&t.grideleprof, script_name.Get());
									current_loaded_script = current_selected_script;

									if (t.grideleprof.PropertiesVariableActive == 1) {
										bUpdateMainString = true;
										current_loaded_script_has_dlua = true;
									}
									else {
										if (current_loaded_script_has_dlua) {
											//Reset t.grideleprof.soundset4_s that contain the dlua calls.
											t.grideleprof.soundset4_s = "";
											current_loaded_script_has_dlua = false;
										}
									}

								}

								if (t.grideleprof.PropertiesVariableActive == 1 || t.grideleprof.PropertiesVariable.VariableDescription.Len() > 0)
								{
									if (ImGui::StyleCollapsingHeader("Behavior", ImGuiTreeNodeFlags_DefaultOpen)) {

										ImGui::Indent(10);

										if (t.grideleprof.PropertiesVariableActive == 1) {
											speech_entries = DisplayLuaDescription(&t.grideleprof);
										}
										else {
											if (t.grideleprof.PropertiesVariable.VariableDescription.Len() > 0) {
												DisplayLuaDescriptionOnly(&t.grideleprof);
											}
										}

										ImGui::Indent(-10);
									}
								}

								if (speech_entries > 0)
								{
									// all SPEECH control is moved to this function.
									SpeechControls(speech_entries, bUpdateMainString);
								}

							}

							t.group = 1;
							if (ImGui::StyleCollapsingHeader(t.strarr_s[415].Get(), ImGuiTreeNodeFlags_DefaultOpen))
							{
								//  Basic AI
								if (t.tflagai == 1)
								{
									// can redirect to better folders if in g.quickparentalcontrolmode
									LPSTR pAIRoot = "scriptbank\\";
									if (g.quickparentalcontrolmode == 2)
									{
										if (t.entityprofile[t.gridentity].ismarker == 0)
										{
											if (t.tflagchar == 1)
												pAIRoot = "scriptbank\\people\\";
											else
												pAIRoot = "scriptbank\\objects\\";
										}
										else
										{
											pAIRoot = "scriptbank\\markers\\";
										}
									}
									cstr tmpvalue;
									tmpvalue = imgui_setpropertyfile2(t.group, t.grideleprof.aimain_s.Get(), t.strarr_s[417].Get(), t.strarr_s[207].Get(), pAIRoot);
									if (t.grideleprof.aimain_s != tmpvalue) 
									{
										t.grideleprof.aimain_s = tmpvalue;
										current_loaded_script = -1;
									}
								}


								//  Has Weapon
								if (t.tflaghasweapon == 1 && t.playercontrol.thirdperson.enabled == 0 && g.quickparentalcontrolmode != 2)
								{
									t.grideleprof.hasweapon_s = imgui_setpropertylist2c(t.group, t.controlindex, t.grideleprof.hasweapon_s.Get(), t.strarr_s[419].Get(), t.strarr_s[209].Get(), 1);
								}

								//  Is Weapon (FPGC - 280809 - filtered fpgcgenre=1 is shooter genre)
								if (t.tflagweap == 1 && g.fpgcgenre == 1)
								{
									t.grideleprof.damage = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.damage), t.strarr_s[420].Get(), t.strarr_s[210].Get()));
									t.grideleprof.accuracy = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.accuracy), t.strarr_s[421].Get(), "Increases the inaccuracy of conical distribution by 1/100th of t.a degree"));
									if (t.grideleprof.weaponisammo == 0)
									{
										t.grideleprof.reloadqty = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.reloadqty), t.strarr_s[422].Get(), t.strarr_s[212].Get()));
										t.grideleprof.fireiterations = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.fireiterations), t.strarr_s[423].Get(), t.strarr_s[213].Get()));
										t.grideleprof.range = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.range), "Range", "Maximum range of bullet travel"));
										t.grideleprof.dropoff = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.dropoff), "Dropoff", "Amount in inches of vertical dropoff per 100 feet of bullet travel"));
										t.grideleprof.clipcapacity = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.clipcapacity), "Clip Capacity", "The total maximum number of clips the player can carry for this weapon"));
									}
									else
									{
										t.grideleprof.lifespan = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.lifespan), t.strarr_s[424].Get(), t.strarr_s[214].Get()));
										t.grideleprof.throwspeed = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.throwspeed), t.strarr_s[425].Get(), t.strarr_s[215].Get()));
										t.grideleprof.throwangle = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.throwangle), t.strarr_s[426].Get(), t.strarr_s[216].Get()));
										t.grideleprof.bounceqty = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.bounceqty), t.strarr_s[427].Get(), t.strarr_s[217].Get()));
										t.grideleprof.explodeonhit = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.explodeonhit), t.strarr_s[428].Get(), t.strarr_s[218].Get(), 0);
									}
									if (t.tflagsimpler == 0)
									{
										t.grideleprof.usespotlighting = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.usespotlighting), "Spot Lighting", "Set whether emits dynamic spot lighting", 0);
									}
								}

								//  Is Character
								if (t.tflagchar == 1)
								{
									if (t.tflagsimpler == 0)
									{

										// 020316 - special check to avoid offering can take weapon if no HUD.X
										t.tfile_s = cstr("gamecore\\guns\\") + t.grideleprof.hasweapon_s + cstr("\\HUD.X");
										if (FileExist(t.tfile_s.Get()) == 1)
										{
											t.grideleprof.cantakeweapon = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.cantakeweapon), t.strarr_s[429].Get(), t.strarr_s[219].Get(), 0);
											//Take Weapon's Ammo
											cstr fieldname = t.strarr_s[430];
											if (fieldname == "Take Weapon's Ammo") fieldname = "Take Weapon Ammo"; //Need to be shorter.
											t.grideleprof.quantity = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.quantity), fieldname.Get() , t.strarr_s[220].Get()));
										}
										t.grideleprof.rateoffire = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.rateoffire), t.strarr_s[431].Get(), t.strarr_s[221].Get()));
									}
								}
								if (t.tflagquantity == 1 && g.quickparentalcontrolmode != 2)
								{
									t.grideleprof.quantity = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.quantity), t.strarr_s[432].Get(), t.strarr_s[222].Get()));
								}

								//  AI Extra
								if (t.tflagvis == 1 && t.tflagai == 1)
								{
									if (t.tflagchar == 1)
									{
										t.grideleprof.coneangle = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.coneangle), t.strarr_s[434].Get(), t.strarr_s[224].Get()));
										t.grideleprof.conerange = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.conerange), "View Range", "The range within which the AI may see the player. Zero triggers the characters default range."));
										t.grideleprof.ifused_s = imgui_setpropertystring2(t.group, t.grideleprof.ifused_s.Get(), t.strarr_s[437].Get(), t.strarr_s[226].Get());
										if (g.quickparentalcontrolmode != 2)
										{
											t.grideleprof.isviolent = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.isviolent), "Blood Effects", "Sets whether blood and screams should be used", 0);
										}
										if (t.tflagsimpler == 0)
										{
											t.grideleprof.colondeath = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.colondeath), "End Collision", "Set to NO switches off collision when die", 0);
										}
									}
									else
									{
										if (t.tflagweap == 0 && t.tflagammo == 0)
										{
											t.grideleprof.usekey_s = imgui_setpropertystring2(t.group, t.grideleprof.usekey_s.Get(), t.strarr_s[436].Get(), t.strarr_s[225].Get());
											if (t.tflagsimpler != 0 & t.entityprofile[t.gridentity].ismarker == 3 && t.entityprofile[t.gridentity].trigger.stylecolor == 1)
											{
												// only one level - no winzone chain option
											}
											else
											{
												t.grideleprof.ifused_s = imgui_setpropertystring2(t.group, t.grideleprof.ifused_s.Get(), t.strarr_s[437].Get(), t.strarr_s[226].Get());
											}
										}
									}
								}
								if (t.tflagifused == 1)
								{
									if (t.tflagusekey == 1)
									{
										t.grideleprof.usekey_s = imgui_setpropertystring2(t.group, t.grideleprof.usekey_s.Get(), t.strarr_s[436].Get(), t.strarr_s[225].Get());
									}
									if (t.tflagsimpler != 0 & t.entityprofile[t.gridentity].ismarker == 3 && t.entityprofile[t.gridentity].trigger.stylecolor == 1)
									{
										// only one level - no winzone chain option
									}
									else
									{
										t.grideleprof.ifused_s = imgui_setpropertystring2(t.group, t.grideleprof.ifused_s.Get(), t.strarr_s[438].Get(), t.strarr_s[227].Get());
									}
								}

							}

							if (t.tflagspawn == 1)
							{
								t.group = 1;
								if (ImGui::StyleCollapsingHeader(t.strarr_s[439].Get(), ImGuiTreeNodeFlags_DefaultOpen)) {

									t.grideleprof.spawnatstart = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.spawnatstart), t.strarr_s[562].Get(), t.strarr_s[563].Get(), 0);
								}
							}


							//  Statistics
							if ((t.tflagvis == 1 || t.tflagobjective == 1 || t.tflaglives == 1 || t.tflagstats == 1) && t.tflagweap == 0 && t.tflagammo == 0)
							{
								t.group = 1;
								if (ImGui::StyleCollapsingHeader(t.strarr_s[451].Get(), ImGuiTreeNodeFlags_DefaultOpen)) 
								{
									if (t.tflagplayersettings == 1)
									{
										t.playercontrol.jumpmax_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.jumpmax_f), "Jump Speed", "Sets the jump speed of the player which controls overall jump height"));
										t.playercontrol.gravity_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.gravity_f), "Gravity", "Sets the modified force percentage of the players own gravity"));
										t.playercontrol.fallspeed_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.fallspeed_f), "Fall Speed", "Sets the maximum speed percentage at which the player will fall"));
										t.playercontrol.climbangle_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.climbangle_f), "Climb Angle", "Sets the maximum angle permitted for the player to ascend a slope"));
										if (t.playercontrol.thirdperson.enabled == 0)
										{
											t.playercontrol.wobblespeed_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.wobblespeed_f), "Wobble Speed", "Sets the rate of motion applied to the camera when moving"));
											t.playercontrol.wobbleheight_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.wobbleheight_f * 100), "Wobble Height", "Sets the degree of motion applied to the camera when moving")) / 100.0f;
											t.playercontrol.footfallpace_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.footfallpace_f * 100), "Footfall Pace", "Sets the rate at which the footfall sound is played when moving")) / 100.0f;
										}
										t.playercontrol.accel_f = atof(imgui_setpropertystring2(t.group, Str(t.playercontrol.accel_f * 100), "Acceleration", "Sets the acceleration curve used when t.moving from t.a stood position")) / 100.0f;
									}
									if (t.tflagmobile == 1) 
									{
										t.grideleprof.isimmobile = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.isimmobile), t.strarr_s[457].Get(), t.strarr_s[247].Get(), 0);
									}
									if (t.tflagmobile == 1)
									{
										if (t.tflagsimpler == 0)
										{
											t.grideleprof.lodmodifier = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.lodmodifier), "LOD Modifier", "Modify when the LOD transition takes effect. The default value is 0, increase this to a percentage reduce the LOD effect."));
										}
									}
								}
							}

							//  Physics Data (non-multiplayer)
							if (t.entityprofile[t.gridentity].ismarker == 0 && t.entityprofile[t.gridentity].islightmarker == 0)
							{
								t.group = 1;
								if (ImGui::StyleCollapsingHeader(t.strarr_s[596].Get(), ImGuiTreeNodeFlags_DefaultOpen)) {

									if (t.grideleprof.physics != 1)  t.grideleprof.physics = 0;
									t.grideleprof.physics = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.physics), t.strarr_s[580].Get(), t.strarr_s[581].Get(), 0);
									t.grideleprof.phyalways = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.phyalways), t.strarr_s[582].Get(), t.strarr_s[583].Get(), 0);
									t.grideleprof.phyweight = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.phyweight), t.strarr_s[584].Get(), t.strarr_s[585].Get()));
									t.grideleprof.phyfriction = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.phyfriction), t.strarr_s[586].Get(), t.strarr_s[587].Get()));
									if (t.tflagsimpler == 0)
									{
										t.grideleprof.explodable = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.explodable), t.strarr_s[592].Get(), t.strarr_s[593].Get(), 0);
										t.grideleprof.explodedamage = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.explodedamage), t.strarr_s[594].Get(), t.strarr_s[595].Get()));
									}
								}
							}

							//  Ammo data (FPGC - 280809 - filtered fpgcgenre=1 is shooter genre
							if (g.fpgcgenre == 1)
							{
								if (t.tflagammo == 1 || t.tflagammoclip == 1)
								{
									if (ImGui::StyleCollapsingHeader(t.strarr_s[459].Get(), ImGuiTreeNodeFlags_DefaultOpen)) {
										t.grideleprof.quantity = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.quantity), t.strarr_s[460].Get(), t.strarr_s[249].Get()));
									}

								}
							}

							//  Light data
							if (t.tflaglight == 1)
							{
								if (ImGui::StyleCollapsingHeader(t.strarr_s[461].Get(), ImGuiTreeNodeFlags_DefaultOpen)) {
									t.grideleprof.light.range = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.light.range), t.strarr_s[462].Get(), t.strarr_s[250].Get())); //PE: 462=Light Range
										
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
									ImGui::Text(t.strarr_s[463].Get());
									ImGui::SameLine();
									ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
									ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
									ImGui::PushItemWidth(-10);

									float colors[5];
									colors[3] = ((t.grideleprof.light.color & 0xff000000) >> 24) / 255.0f;
									colors[0] = ((t.grideleprof.light.color & 0x00ff0000) >> 16) / 255.0f;
									colors[1] = ((t.grideleprof.light.color & 0x0000ff00) >> 8) / 255.0f;
									colors[2] = (t.grideleprof.light.color & 0x000000ff) / 255.0f;
									ImGui::ColorEdit3("##LightColorSetupField", colors, 0);
									if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.strarr_s[251].Get());
									colors[0] *= 255.0f;
									colors[1] *= 255.0f;
									colors[2] *= 255.0f;
									colors[3] *= 255.0f;
									t.grideleprof.light.color = 0xff000000 + ((unsigned int)colors[0] << 16) + ((unsigned int)colors[1] << 8) + +((unsigned int)colors[2]);

									ImGui::PopItemWidth();

									if (t.tflagsimpler == 0)
									{
										t.grideleprof.usespotlighting = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.usespotlighting), "Spot Lighting", "Change dynamic light to spot lighting", 0);
									}
								}
							}

							//  Decal data
							if (t.tflagtdecal == 1)
							{
								t.propfield[t.group] = t.controlindex;
								//  Decal Particle data
								if (t.tflagdecalparticle == 1)
								{
									//++t.group; startgroup("Decal Particle"); t.controlindex = 0;
									if (ImGui::StyleCollapsingHeader("Decal Particle", ImGuiTreeNodeFlags_DefaultOpen)) {

										t.grideleprof.particleoverride = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.particleoverride), "Custom Settings", "Whether you wish to override default settings", 0);
										t.grideleprof.particle.offsety = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.offsety), "OffsetY", "Vertical adjustment of start position"));
										t.grideleprof.particle.scale = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.scale), "Scale", "A value from 0 to 100, denoting size of particle"));
										t.grideleprof.particle.randomstartx = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.randomstartx), "Random Start X", "Random start area"));
										t.grideleprof.particle.randomstarty = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.randomstarty), "Random Start Y", "Random start area"));
										t.grideleprof.particle.randomstartz = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.randomstartz), "Random Start Z", "Random start area"));
										t.grideleprof.particle.linearmotionx = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.linearmotionx), "Linear Motion X", "Constant motion direction"));
										t.grideleprof.particle.linearmotiony = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.linearmotiony), "Linear Motion Y", "Constant motion direction"));
										t.grideleprof.particle.linearmotionz = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.linearmotionz), "Linear Motion Z", "Constant motion direction"));
										t.grideleprof.particle.randommotionx = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.randommotionx), "Random Motion X", "Random motion direction"));
										t.grideleprof.particle.randommotiony = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.randommotiony), "Random Motion Y", "Random motion direction"));
										t.grideleprof.particle.randommotionz = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.randommotionz), "Random Motion Z", "Random motion direction"));
										t.grideleprof.particle.mirrormode = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.mirrormode), "Mirror Mode", "Set to one to reverse the particle"));
										t.grideleprof.particle.camerazshift = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.camerazshift), "Camera Z Shift", "Shift t.particle towards camera"));
										t.grideleprof.particle.scaleonlyx = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.scaleonlyx), "Scale Only X", "Percentage X over Y scale"));
										t.grideleprof.particle.lifeincrement = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.lifeincrement), "Life Increment", "Control lifespan of particle"));
										t.grideleprof.particle.alphaintensity = atol(imgui_setpropertystring2(t.group, Str(t.grideleprof.particle.alphaintensity), "Alpha Intensity", "Control alpha percentage of particle"));
										//  V118 - 060810 - knxrb - Decal animation setting (Added animation choice setting).
										t.grideleprof.particle.animated = imgui_setpropertylist2(t.group, t.controlindex, Str(t.grideleprof.particle.animated), "Animated Particle", "Sets whether the t.particle t.decal Texture is animated or static.", 0);
									}
								}
							}

							// moved Particle to main

							// Sound
							if (t.tflagsound == 1 || t.tflagsoundset == 1 || tflagtext == 1 || tflagimage == 1)
							{
								cstr group_text;
								if (tflagtext == 1 || tflagimage == 1)
								{
									if (tflagtext == 1) group_text = "Text";
									if (tflagimage == 1) group_text = "Image";
								}
								else
								{
									group_text = "Media";
								}
									
								if (speech_entries > 0)
								{
								}

								if (ImGui::StyleCollapsingHeader(group_text.Get(), ImGuiTreeNodeFlags_DefaultOpen))
								{
									if (g.fpgcgenre == 1)
									{
										if (t.entityprofile[t.gridentity].ischaracter > 0) 
										{
											t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), "Sound0", t.strarr_s[254].Get(), "audiobank\\");
										}
										else 
										{
											if (g.vrqcontrolmode != 0)
											{
												if (t.tflagsound == 1 && t.tflagsoundset != 1) 
												{
													//PE: changed from 469 to 467 , should be sound0
													t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), t.strarr_s[467].Get(), t.strarr_s[253].Get(), "audiobank\\");
												}
											}
											else
											{
												if (t.tflagsound == 1 && t.tflagsoundset != 1) 
												{
													t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), t.strarr_s[467].Get(), t.strarr_s[253].Get(), "audiobank\\");
												}
											}
											if (t.tflagsoundset == 1) 
											{
												t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), t.strarr_s[469].Get(), t.strarr_s[255].Get(), "audiobank\\voices\\");
											}
											if (tflagtext == 1) 
											{
												t.grideleprof.soundset_s = imgui_setpropertystring2(t.group, t.grideleprof.soundset_s.Get(), "Text to Appear", "Enter text to appear in-game");
											}
											if (tflagimage == 1) 
											{
												t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), "Image File", "Select image to appear in-game", "scriptbank\\images\\imagesinzone\\");
											}
										}

										if (t.tflagnosecond == 0)
										{
											if (t.tflagsound == 1 || t.tflagsoundset == 1)
											{
												//We got some missing translations.
												if (t.strarr_s[468] == "") t.strarr_s[468] = "Sound1";
												if (t.strarr_s[480] == "") t.strarr_s[480] = "Sound2";
												if (t.strarr_s[481] == "") t.strarr_s[481] = "Sound3";
												t.grideleprof.soundset1_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset1_s.Get(), t.strarr_s[468].Get(), t.strarr_s[254].Get(), "audiobank\\");
												t.grideleprof.soundset2_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset2_s.Get(), t.strarr_s[480].Get(), t.strarr_s[254].Get(), "audiobank\\");
												t.grideleprof.soundset3_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset3_s.Get(), t.strarr_s[481].Get(), t.strarr_s[254].Get(), "audiobank\\");
												//ImGui::TextCenter("Sound4");
												//ImGui::TextCenter("(repurposed)");
												t.grideleprof.soundset4a_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset4a_s.Get(), "Sound4", t.strarr_s[254].Get(), "audiobank\\");
												t.grideleprof.soundset5_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset5_s.Get(), "Sound5", t.strarr_s[254].Get(), "audiobank\\");
												t.grideleprof.soundset6_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset6_s.Get(), "Sound6", t.strarr_s[254].Get(), "audiobank\\");
											}
										}
									}
									else
									{
										if (t.tflagsoundset == 1)
										{
											t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), t.strarr_s[469].Get(), t.strarr_s[255].Get(), "audiobank\\voices\\");
										}
										else
										{
											t.grideleprof.soundset_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset_s.Get(), t.strarr_s[467].Get(), t.strarr_s[253].Get(), "audiobank\\"); ++t.controlindex;
										}
										t.grideleprof.soundset1_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset1_s.Get(), t.strarr_s[468].Get(), t.strarr_s[254].Get(), "audiobank\\"); ++t.controlindex;
									}
								}
							}

							// Video
							if (t.tflagvideo == 1)
							{
								if (ImGui::StyleCollapsingHeader(t.strarr_s[597].Get(), ImGuiTreeNodeFlags_DefaultOpen)) {

									t.grideleprof.soundset1_s = imgui_setpropertyfile2(t.group, t.grideleprof.soundset1_s.Get(), "Video Slot", t.strarr_s[601].Get(), "videobank\\");
								}
							}

							//  Third person settings
							if (t.tflagplayersettings == 1 && t.playercontrol.thirdperson.enabled == 1)
							{
								if (ImGui::StyleCollapsingHeader("Third Person", ImGuiTreeNodeFlags_DefaultOpen)) {

									t.livegroupforthirdperson = t.group;
									t.playercontrol.thirdperson.cameralocked = imgui_setpropertylist2(t.group, t.controlindex, Str(t.playercontrol.thirdperson.cameralocked), "Camera Locked", "Fixes camera height and angle for third person view", 0);
									t.playercontrol.thirdperson.cameradistance = atol(imgui_setpropertystring2(t.group, Str(t.playercontrol.thirdperson.cameradistance), "Camera Distance", "Sets the distance of the third person camera"));
									t.playercontrol.thirdperson.camerashoulder = atol(imgui_setpropertystring2(t.group, Str(t.playercontrol.thirdperson.camerashoulder), "Camera X Offset", "Sets the distance to shift the camera over shoulder"));
									t.playercontrol.thirdperson.cameraheight = atol(imgui_setpropertystring2(t.group, Str(t.playercontrol.thirdperson.cameraheight), "Camera Y Offset", "Sets the vertical height of the third person camera. If more than twice the camera distance, camera collision disables"));
									t.playercontrol.thirdperson.camerafocus = atol(imgui_setpropertystring2(t.group, Str(t.playercontrol.thirdperson.camerafocus), "Camera Focus", "Sets the camera X angle offset to align focus of the third person camera"));
									t.playercontrol.thirdperson.cameraspeed = atol(imgui_setpropertystring2(t.group, Str(t.playercontrol.thirdperson.cameraspeed), "Camera Speed", "Sets the retraction speed percentage of the third person camera"));
									t.playercontrol.thirdperson.camerafollow = imgui_setpropertylist2(t.group, t.controlindex, Str(t.playercontrol.thirdperson.camerafollow), "Run Mode", "If set to yes, protagonist uses WASD t.movement mode", 0);
									t.playercontrol.thirdperson.camerareticle = imgui_setpropertylist2(t.group, t.controlindex, Str(t.playercontrol.thirdperson.camerareticle), "Show Reticle", "Show the third person 'crosshair' reticle Dot ( ", 0);
								}
							}

						}
					} //Advenced open


					if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0) 
					{
						//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
						ImGui::Text("");
						ImGui::Text("");
					}


					ImRect bbwin(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize());
					if (ImGui::IsMouseHoveringRect(bbwin.Min, bbwin.Max))
					{
						bImGuiGotFocus = true;
					}
					if (ImGui::IsAnyItemFocused()) {
						bImGuiGotFocus = true;
					}
				}
				ImGui::PopItemWidth();
				ImGui::SetWindowFontScale(1.0);

				ImGui::EndChild();

				CheckMinimumDockSpaceSize(250.0f);

				ImGui::End();

				if(!bEntity_Properties_Window) //Window closed.
					iOldPickedEntityIndex = -1;

				if (t.inputsys.mclick == 1 && ImGui::IsWindowHovered()) 
				{
					//Click start , block until mouse is release.
					bProperties_Window_Block_Mouse = true;
				}

			}
			else {
				iOldPickedEntityIndex = -1;
			}
		}
		else 
		{
			//PRoperties closed , check if we need to exit zoommode.
			if (t.gridentityinzoomview > 0) 
			{
				t.tpressedtoleavezoommode = 2; //Exit zoom and save.
				if (t.grideditselect < 3 || t.grideditselect > 4) 
				{
					//Make sure to exit fast.
					int igridentity = t.gridentity;

					if( iOldgridentity != t.gridentity && iOldgridentity > -1)
						t.gridentity = iOldgridentity;

					int olges = t.grideditselect;
					t.grideditselect = 4;
					editor_viewfunctionality();
					t.grideditselect = olges;
					t.gridentity = igridentity;
				}
			}

			//PE: Bug if open properties, close and delete object, then add object and properties, failed and use old id.
			iOldPickedEntityIndex = -1;
		}


		//####################################
		//#### Procedural Level Generator ####
		//####################################

		//No resetting needed fixed.
		void procedural_new_level(void);
		procedural_new_level();

		//###########################
		//#### External Entities ####
		//###########################

		static std::map<std::string, std::int32_t> entity_folders;

		//PE: Exactly fit for 9 normal object, and 6 that include dlua description.
		if (refresh_gui_docking == 0 || bResetObjectLibrarySize) ImGui::SetNextWindowSize(ImVec2(66 * ImGui::GetFontSize(), (43 * ImGui::GetFontSize()) + 19.0), ImGuiCond_Always); //ImGuiCond_FirstUseEver
		if (refresh_gui_docking == 0 || bResetObjectLibrarySize) ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
		if (refresh_gui_docking == 0) init_Left_Categories_Column_Width = 3;
		ImGuiWindowFlags ex_window_flags = 0;
		if (refresh_gui_docking == 0 && !bExternal_Entities_Window)
		{
			ImGui::Begin("##Object Library ExternalWindow", &bExternal_Entities_Window, ex_window_flags);
			ImGui::End();
		}
		if (refresh_gui_docking <= 2 )
			pref.iSetColumnsEntityLib = 3;

		#ifndef GGMAXEDU
		// ensure workshop system always called to handle callbacks and ensure latest items are available when editing
		extern void workshop_update(bool);
		workshop_update(false);
		#endif

		// librray system
		process_entity_library_v2();

		// collect an entire list of all relevant filders (entitybank, scriptbank, images, particles, etc)
		mapeditorexecutable_full_folder_refresh();

		wi::profiler::EndRange(clSeg); clSeg = wi::profiler::BeginRangeCPU("CL-ObjLists");
		//########################
		//#### Level Entities ####
		//########################

		if (iDragDropActive > 0)
			iDragDropActive--;

		//PE: V2SEARCHBAR will display the new search layout.
		#define V2SEARCHBAR

		if (refresh_gui_docking == 0) 
		{
			//ImGuiWindowFlags_NoDocking
			bool bOpen = true;
			ImGui::Begin(TABENTITYNAME, &bOpen, iGenralWindowsFlags);
			ImGui::End();
			ImGui::Begin("Current Objects##AdditionalIconsWindow", &bOpen, iGenralWindowsFlags);
			ImGui::End();
		}
		if (refresh_gui_docking > 0) 
		{
			//PE: Not sure if these should be there so ...
			#define REMOVE_NEWEST_AND_OLDEST
			#define SORT_LEVELITEMS 0 
			#define SORT_PROJECTSITEMS 1
			#define SORT_DETAILEDITEMS 2
			#define SORT_OLDESTITEMS 7
			#define SORT_NEWESTITEMS 6
			#define SORT_GROUPITEMS 3
			#define SORT_INSTANCEITEMS 4
			#define SORT_BEHAVIORITEMS 5
			static bool bProjectItems = false;

			bool bToolTipActive = true;
			if (pref.iEnableDragDropEntityMode && bDraggingActive)
			{
				if (t.gridentity != 0 || t.gridentityobj != 0)
				{
					bToolTipActive = false;
				}
			}
			if (bTrashcanIconActive || bTrashcanIconActive2)
				bToolTipActive = false;

			int iWinFlags = 0;
			bool bAlwaysOpen = true;
			ImGui::Begin(TABENTITYNAME, &bAlwaysOpen, iGenralWindowsFlags | ImGuiWindowFlags_NoScrollbar); //, &bAlwaysOpen, iWinFlags);
			static char cSearchEntities[1024] = "\0";

			ImGui::BeginChild("##ChirlEntitiesLeftPanel", ImVec2(ImGui::GetWindowSize().x - 2.0f, fsy*2.0), false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs); //ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar

			ImGui::PushItemWidth(-1);

			CheckTutorialAction("+##+", 8.0f); //Tutorial: check if we are waiting for this action

			if (ImGui::StyleButton("Add##+", ImVec2(ImGui::GetWindowSize().x *0.19, fsy*1.5)))
			{
				if (bTutorialCheckAction) TutorialNextAction(); //Clicked get next tutorial action.
				//Open Add item page.
				cFolderItem *pSearchFolder = &MainEntityList;
				pSearchFolder = pSearchFolder->m_pNext;
				while (pSearchFolder)
				{
					if (pSearchFolder->m_pFirstFile) 
					{
						cFolderItem::sFolderFiles * searchfiles = pSearchFolder->m_pFirstFile->m_pNext;
						while (searchfiles) 
						{
							searchfiles->iFlags = 0;
							searchfiles = searchfiles->m_pNext;
						}
					}
					pSearchFolder = pSearchFolder->m_pNext;
				}

				bExternal_Entities_Window = true;
				iDisplayLibraryType = 0;
				iDisplayLibrarySubType = 0;
			}
			if (bToolTipActive && ImGui::IsItemHovered()) ImGui::SetTooltip("Click here to add a new object to the game level");

			// when click ADD, have the option to refresh library (activated in developer mode for users who import often as it is a performance hit)
			if (g_iRefreshLibraryFoldersAfterDelay > 0)
			{
				g_iRefreshLibraryFoldersAfterDelay--;
				if (g_iRefreshLibraryFoldersAfterDelay == 2)
				{
					strcpy(cTriggerMessage, "Refreshing Library Lists ...");
					bTriggerMessage = true;
				}
				if (g_iRefreshLibraryFoldersAfterDelay == 0)
				{
					g_iRefreshLibraryFolders = 2;
				}
			}
			if (g_iRefreshLibraryFolders != 0 )
			{
				timestampactivity(0, "REFRESHING LIBRARY FOLDERS");
				extern void RefreshPurchasedFolder (void);
				RefreshPurchasedFolder();
				// force the purchased cateogry to show up (and also cause needed refresh)
				extern void process_gotopurchaedandrefreshtopurchases (bool bForceSearch);
				process_gotopurchaedandrefreshtopurchases(false);
				// trigger folder tree on left of library to recalculate in case of new folders (audiobank\xx)
				extern bool bTreeViewInitInNextFrame;
				bTreeViewInitInNextFrame = true;
				// also update the gun list, we might have new weapons
				timestampactivity(0, "RESCANNING G-LIST");
				extern bool g_bGunListNeedsRefreshing;
				g_bGunListNeedsRefreshing = true;
				gun_scaninall_findnewlyaddedgun();
				decal_scaninall_findnewlyaddedgun();
				if (g_iRefreshLibraryFolders == 1)
				{
					// View All (instead of Purchased View)
					bSelectLibraryViewAll = true;
				}
				g_iRefreshLibraryFolders = 0;
			}
		
			ImGui::SameLine();
			ImGui::PopItemWidth();

			static bool bToggleThumbViews = true;
			int iToggleIcon;
			if (bToggleThumbViews)
				iToggleIcon = ENTITY_EYE_ON;
			else
				iToggleIcon = ENTITY_EYE_OFF;

			float iconoffsetx = -7.0f;
			float iconoffsety = -4.0f;
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(iconoffsetx, iconoffsety));
			if (ImGui::ImgBtn(iToggleIcon, ImVec2(23, 23), ImColor(0, 0, 0, 0), ImColor(220, 220, 220, 220), ImColor(255, 255, 255, 255),
				ImColor(180, 180, 160, 255), -1, 0, 0, 0, false, false, false, false, false))
			{
				bToggleThumbViews = 1 - bToggleThumbViews;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle on off thumbnails view.");


			ImGui::SameLine();
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(iconoffsetx-1,abs(iconoffsety)));

			float fDropDownWidth = 0.0f;
			fDropDownWidth = 34.0f;
			ImGui::PushItemWidth(-6 - fDropDownWidth);
			window = ImGui::GetCurrentWindow();

			ImVec2 current_pos = ImGui::GetCursorPos();
			float current_item_width = ImGui::GetItemRectSize().x;

			#ifdef V2SEARCHBAR
			float fOldSpacing = style.FramePadding.x;
			style.FramePadding.x = 22.0; //Make room for search icon.
			ImVec2 vSearchPos = ImGui::GetCursorPos();
			#endif

			ImVec2 search_icon_pos = window->DC.CursorPos + ImVec2(ImGui::GetContentRegionAvailWidth() - 10.0 - 16.0 - fDropDownWidth,2.0f);
			ImGui::SetItemAllowOverlap();
			static bool bSetKeyboardFocus = false;
			// Force the keyboard focus to the input text field when the user presses the clear search button (set below).
			if (bSetKeyboardFocus)
			{
				ImGui::SetKeyboardFocusHere(0);
				bSetKeyboardFocus = false;
			}
			// only for object based lists, not groups/behaviors/etc
			LPSTR pToolTipForSearch = "Cannot search this list!";

			bool bIsSearchAble = false;
			if (current_sort_order == SORT_LEVELITEMS || current_sort_order == SORT_NEWESTITEMS || current_sort_order == SORT_OLDESTITEMS || current_sort_order == SORT_DETAILEDITEMS)
				bIsSearchAble = true;

			if (bIsSearchAble)
			{
				pToolTipForSearch = "Type here to search for an object in your level";
			}
			if (ImGui::InputText(" ##cSearchEntities", &cSearchEntities[0], MAX_PATH, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (bIsSearchAble)
				{
					if (strlen(cSearchEntities) > 1)
					{
						bool already_there = false;
						for (int l = 0; l < MAXSEARCHHISTORY; l++) {
							if (strcmp(cSearchEntities, pref.small_search_history[l]) == 0) {
								already_there = true;
								break;
							}
						}
						if (!already_there) {
							bool foundspot = false;
							for (int l = 0; l < MAXSEARCHHISTORY; l++) {
								if (strlen(pref.small_search_history[l]) <= 0) {
									strcpy(pref.small_search_history[l], cSearchEntities);
									foundspot = true;
									break;
								}
							}
							if (!foundspot) {
								//Move entry list.
								for (int l = 0; l < MAXSEARCHHISTORY; l++) {
									strcpy(pref.small_search_history[l], pref.small_search_history[l + 1]);
								}
								strcpy(pref.small_search_history[MAXSEARCHHISTORY - 1], cSearchEntities);
							}
						}
					}
				}
			}
			if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered() && bToolTipActive) ImGui::SetTooltip("%s", pToolTipForSearch);
			if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;
			ImGui::PopItemWidth();
			ImGui::SameLine();

			ImVec2 restore_pos = ImGui::GetCursorPos();
			current_pos.y = ImGui::GetCursorPosY();

			#ifdef V2SEARCHBAR
			style.FramePadding.x = fOldSpacing;
			//Only display closebut if we have room.
			if (restore_pos.x > 110 ) // ? not sure if we only activate after search begin ? && strlen(cSearchEntities) > 0
			{
				ImGui::SetItemAllowOverlap();
				if (ImGui::CloseButton(ImGui::GetCurrentWindow()->GetID("#ClearSearchv2"), ImGui::GetWindowPos() + ImGui::GetCursorPos() + ImVec2(-38, 0)))
				{
					strcpy(cSearchEntities, "");
					bSetKeyboardFocus = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear search");
				ImGui::SameLine();
			}
			#endif

			static bool bSearchWinToggle = false;
			bool search_img_hovered = false, search_img_held = false;
			ID3D11ShaderResourceView* lpTexture = GetImagePointerView(TOOL_ENT_SEARCH);
			if (lpTexture)
			{
				ImGui::SetItemAllowOverlap();
				#ifdef V2SEARCHBAR
				ImVec2 search_icon_pos = ImGui::GetWindowPos() + vSearchPos + ImVec2(3.0, 3.0);
				#endif
				ImRect bb(search_icon_pos, search_icon_pos + ImVec2(16, 16));
				ImGui::PushID(TOOL_ENT_SEARCH);
				const ImGuiID id = window->GetID("#image");
				ImGui::PopID();
				ImGui::ItemSize(bb);
				if (ImGui::ItemAdd(bb, id)) {
					bool pressed = ImGui::ButtonBehavior(bb, id, &search_img_hovered, &search_img_held);
					if (pressed) {
						bSearchWinToggle = 1 - bSearchWinToggle;
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
					}
					ImGuiWindow* window = ImGui::GetCurrentWindow();
					window->DrawList->AddImage((ImTextureID)lpTexture, search_icon_pos, search_icon_pos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0)));
				}
			}

			if (bSearchWinToggle)
			{
				//Do we have a search history.
				bool display_history = true; //false;
				if (display_history)
				{
					ImGui::SameLine();
					
					ImGui::SetNextWindowPos(ImGui::GetWindowPos() + current_pos);
					if (ImGui::Begin("##searchselectpopup", &bSearchWinToggle, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_Tooltip )) //| ImGuiWindowFlags_Tooltip
					{
						#ifndef V2SEARCHBAR
						if( ImGui::StyleButton("Clear Search Field",ImVec2(ImGui::GetContentRegionAvailWidth(),0.0f) ) )
						{
							strcpy(cSearchEntities, "");
							bSearchWinToggle = false;
						}
						ImGui::Separator();
						#endif

						//ImGui::Indent(10);
						for (int l = 0; l < MAXSEARCHHISTORY; l++) {
							if (strlen(pref.small_search_history[l]) > 0) {
								bool is_selected = false;
								if (ImGui::Selectable(pref.small_search_history[l], is_selected)) {
									strcpy(cSearchEntities, pref.small_search_history[l]);
									bSearchWinToggle = false;
								}
							}
						}
					}
					ImGui::End();
				}
			}

			ImGui::SetCursorPos(current_pos);

			if (!search_img_hovered)
			{
				if (ImGui::IsMouseReleased(0)) //ImGui::IsAnyMouseDown())
				{
					bSearchWinToggle = false;
				}
			}

			ImGui::SetCursorPos(ImVec2(restore_pos.x-8.0f, restore_pos.y));

			#ifdef REMOVE_NEWEST_AND_OLDEST
			const char* sortby_modes[] = { "Level Items", "Project Items", "Detailed Object List", "Group List", "Instance List", "Behavior List"};
			#else
			const char* sortby_modes[] = { "Level Items", "Project Items", "Detailed Object List", "Group List", "Instance List", "Behavior List", "Newest" , "Oldest" };
			#endif
			int isortbySize = IM_ARRAYSIZE(sortby_modes);
			if (!pref.iEnableAdvancedEntityList)
				isortbySize--;

			int comboflags = ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLarge;
			ImGui::PushItemWidth(-24);
			bool bUpdateProjectFiles = false;

			if (bProjectItems && current_sort_order == SORT_LEVELITEMS)
			{
				current_sort_order = SORT_PROJECTSITEMS;
			}

			if (ImGui::BeginCombo("##combosortbymodesentotyleft", sortby_modes[current_sort_order], comboflags))
			{
				for (int n = 0; n < isortbySize; n++)
				{
					bool is_selected = (current_sort_order == n);
					if (ImGui::Selectable(sortby_modes[n], is_selected)) 
					{
						current_sort_order = n;
						if (n == SORT_PROJECTSITEMS)
						{
							//PE: Use same list , just add all.
							bProjectItems = true;
							bUpdateProjectFiles = true;
							current_sort_order = SORT_LEVELITEMS;
						}
						else if( n == SORT_LEVELITEMS)
						{
							bProjectItems = false;
							bUpdateProjectFiles = true;
						}
						
					}
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::EndChild();

			if (bProjectItems && current_sort_order == SORT_PROJECTSITEMS)
			{
				current_sort_order = SORT_LEVELITEMS;
			}

			ImVec2 content_avail = ImVec2(0.0, 0.0);
			content_avail = ImGui::GetContentRegionAvail();
			content_avail.y -= fsy * 3.0;
			if (content_avail.y < fsy) content_avail.y = fsy;
			
			// Size of icons.
			int entity_icons = 12; if (pref.iObjectEnableAdvanced)	entity_icons = 13;
			int entity_icons_columns = 6;
			float entity_w = ImGui::GetContentRegionAvailWidth() - 10.0f;
			float fSpacer = 0.0f;
			float entity_image_size = entity_w / (float)entity_icons_columns;
			entity_image_size -= ((1.125f) * entity_icons_columns);
			if (entity_w > 680)
			{
				//Switch to 15 per row.
				entity_icons = 15;
				entity_icons_columns = entity_icons;// 12;
				entity_image_size = entity_w / (float)entity_icons_columns;
				entity_image_size -= 7.5f;
			}
			else if (entity_w > 360)
			{
				//Switch to 12 per row.
				entity_icons_columns = entity_icons;// 12;
				entity_image_size = entity_w / (float)entity_icons_columns;
				entity_image_size -= 7.5f;
			}		

			int iIconRows = (entity_icons + (entity_icons_columns - 1)) / entity_icons_columns;
			if (content_avail.x <= 1)
			{
				content_avail = ImGui::GetContentRegionAvail();
				content_avail.y -= (fsy*0.5);
			}
			content_avail.y -= 3.0f;
			content_avail.y -= ((entity_w / entity_icons_columns) * iIconRows);
			content_avail.y -= 10.0f;
			if (entity_icons_columns > 10)
				content_avail.y -= entity_image_size;

			static bool bViewOptionsOpen = false;
			if(bViewOptionsOpen)
				ImGui::BeginChild("##MainEntitiesLeftPanel", content_avail - ImVec2(0.0f, 205.0f), false, iGenralWindowsFlags); //, false, ImGuiWindowFlags_HorizontalScrollbar);
			else
				ImGui::BeginChild("##MainEntitiesLeftPanel", content_avail, false, iGenralWindowsFlags); //, false, ImGuiWindowFlags_HorizontalScrollbar);
			
			if(bProjectItems && current_sort_order == SORT_LEVELITEMS)
				ImGui::TextCenter("Project Items");
			else
				ImGui::TextCenter(sortby_modes[current_sort_order]);


			static std::vector<std::pair<std::string,int>> sorted_entity_files;

			static int last_entidmaster = 0;
			static int last_include_icon_set = -1;

			int iMasterEntid = g.entidmaster;
			if (iRestoreEntidMaster >= 0 && bExternal_Entities_Window)
			{
				iMasterEntid = iRestoreEntidMaster;
			}

			if (current_sort_order == SORT_LEVELITEMS && !bProjectItems)
			{
				static int last_entityelementlist = 0;
				if (last_entityelementlist != g.entityelementlist)
				{
					last_entityelementlist = g.entityelementlist;
					bUpdateProjectFiles = true;
				}
				extern bool bUpdateObjectList;
				if (bUpdateObjectList)
				{
					bUpdateObjectList = false;
					bUpdateProjectFiles = true;
				}
			}
			

			static int last_sortby = -1;
			if (bUpdateProjectFiles || last_entidmaster != iMasterEntid || last_include_icon_set != iIncludeLeftIconSet || current_sort_order != last_sortby )
			{
				//Sort new list.
				sorted_entity_files.clear();
				if (iMasterEntid >= 1)
				{
					if (current_sort_order == SORT_GROUPITEMS)
					{
						// Sort group list
						for (int gi = 0; gi < MAXGROUPSLISTS; gi++)
						{
							if (vEntityGroupList[gi].size() > 0)
							{
								std::string stmp = "Group " + std::to_string(gi);
								sorted_entity_files.push_back(std::make_pair(stmp, gi));
							}
						}
					}
					else
					{
						if (current_sort_order == SORT_INSTANCEITEMS || current_sort_order == SORT_BEHAVIORITEMS)
						{
							if (current_sort_order == SORT_INSTANCEITEMS)
							{
								// Sort by instance ID order
								for (t.e = 1; t.e <= g.entityelementlist; t.e++)
								{
									cstr scriptname = t.entityelement[t.e].eleprof.name_s;
									std::string stmp = Lower(scriptname.Get());
									if (t.entityelement[t.e].bankindex > 0)
									{
										sorted_entity_files.push_back(std::make_pair(stmp, t.e));
									}
								}
							}
							if (current_sort_order == SORT_BEHAVIORITEMS)
							{
								// Sort by behavior list
								for (t.e = 1; t.e <= g.entityelementlist; t.e++)
								{
									cstr scriptname = t.entityelement[t.e].eleprof.aimain_s;
									std::string stmp = Lower(scriptname.Get());
									bool bFoundThisScriptInList = false;
									for (int i = 0; i < sorted_entity_files.size(); i++)
									{
										if (sorted_entity_files[i].first == stmp)
										{
											// only add a unique one (ie list of behaviours)
											bFoundThisScriptInList = true;
											break;
										}
									}
									if (bFoundThisScriptInList == false)
									{
										sorted_entity_files.push_back(std::make_pair(stmp, t.e));
									}
								}
							}
						}
						else
						{
							// Sort entity parent list.
							for (t.entid = 1; t.entid <= iMasterEntid; t.entid++)
							{
								std::string stmp = Lower(t.entityprofileheader[t.entid].desc_s.Get());
								if (current_sort_order == SORT_NEWESTITEMS || current_sort_order == SORT_OLDESTITEMS)
								{
									//Convert to sortable by string.
									if (t.entid < 10)
										stmp = "000" + std::to_string(t.entid);
									else if (t.entid < 100)
										stmp = "00" + std::to_string(t.entid);
									else if (t.entid < 1000)
										stmp = "0" + std::to_string(t.entid);
									else
										stmp = std::to_string(t.entid);
								}
								stmp += "###"; //We need it to be unique so add this.
								stmp += t.entityprofile[t.entid].model_s.Get();
								stmp += "###";
								stmp += std::to_string(t.entid);
								int itmp = t.entid;

								bool bValid = true;
								if (current_sort_order == SORT_LEVELITEMS && !bProjectItems)
								{
									//static int lastentityelementlist = -1;
									bValid = false;
									//PE: Exclude hidden and auto-gen objects.
									for (int i = 1; i <= g.entityelementlist; i++)
									{
										if (t.entityelement[i].bankindex == t.entid)
										{
											if (!(t.entityelement[i].y == -99999 || t.entityelement[i].y == -999999))
											{
												bValid = true;
												break;
											}
										}
										if (t.gridentity == t.entid ) //t.widget.pickedEntityIndex == t.entid)
										{
											bValid = true;
											break;
										}
									}
								}
								if(bValid)
									sorted_entity_files.push_back(std::make_pair(stmp, itmp));
							}
							std::sort(sorted_entity_files.begin(), sorted_entity_files.end());
							if (current_sort_order == SORT_NEWESTITEMS)
									std::reverse(sorted_entity_files.begin(), sorted_entity_files.end());
						}
					}
				}
				last_entidmaster = iMasterEntid;
				last_include_icon_set = iIncludeLeftIconSet;
				last_sortby = current_sort_order;
			}

			int uniqueId = 15000;
			int preview_count = 0;
			media_icon_size_leftpanel = 64;
			iColumnsWidth_leftpanel = 110;
			iColumns_leftpanel = 0;
			bDisplayText_leftpanel = true;
			fFontSize_leftpanel = SMALLFONTSIZE;
			ImGui::SetWindowFontScale(fFontSize_leftpanel);
			float fWinWidth = ImGui::GetWindowSize().x - 10.0; // Flicker - ImGui::GetCurrentWindow()->ScrollbarSizes.x;
			if (iColumnsWidth_leftpanel >= fWinWidth && fWinWidth > media_icon_size_leftpanel) 
			{
				iColumnsWidth_leftpanel = fWinWidth;
				fFontSize_leftpanel = SMALLESTFONTSIZE;
				ImGui::SetWindowFontScale(fFontSize_leftpanel);
			}
			if (fWinWidth <= media_icon_size_leftpanel + 10) 
			{
				iColumnsWidth_leftpanel = media_icon_size_leftpanel;
				fFontSize_leftpanel = SMALLESTFONTSIZE;
				ImGui::SetWindowFontScale(fFontSize_leftpanel);
			}
			if (fWinWidth <= 42) 
			{
				media_icon_size_leftpanel = 32;
				iColumnsWidth_leftpanel = media_icon_size_leftpanel + 16;
				bDisplayText_leftpanel = false;
			}
			iColumns_leftpanel = (int)(ImGui::GetWindowSize().x / (iColumnsWidth_leftpanel));
			if (iColumns_leftpanel <= 1)
				iColumns_leftpanel = 1;

			#ifdef ADD_DETAIL_LEFT_PANEL_ENTITY_LIST
			if (current_sort_order == SORT_BEHAVIORITEMS || current_sort_order == SORT_INSTANCEITEMS || current_sort_order == SORT_DETAILEDITEMS || current_sort_order == SORT_PROJECTSITEMS || current_sort_order == SORT_GROUPITEMS) //PE: Detailed display in one column.
			{
				iColumns_leftpanel = 1;
			}
			if (!bToggleThumbViews && (current_sort_order == SORT_LEVELITEMS || current_sort_order == SORT_OLDESTITEMS || current_sort_order == SORT_NEWESTITEMS))
			{
				iColumns_leftpanel = 1;
			}
			#endif

			if (!sorted_entity_files.empty())
			{
				ImGui::Columns(iColumns_leftpanel, "mycolumns4entities", false);  //false no border

				bool bHoveredUsed = false;
				for (int iloop = 0; iloop < 2; iloop++)
				{
					for (std::vector< std::pair<std::string, std::int32_t>>::iterator it = sorted_entity_files.begin(); it != sorted_entity_files.end(); ++it)
					{
						if (it->second == 999999)
						{
							if (iloop == 1)
							{
								//Seperator.
								if (iColumns_leftpanel == 1)
								{
									ImGui::Separator();
									preview_count++;
									ImGui::NextColumn();
								}
								else
								{
									if (iColumns_leftpanel == 1)
									{
										ImGui::Separator();
										preview_count++;
										ImGui::NextColumn();
									}
									else
									{
										for (int i = preview_count % iColumns_leftpanel; i < iColumns_leftpanel; i++)
										{
											preview_count++;
											ImGui::NextColumn();
										}
										ImGui::Separator();
									}
								}
							}
						}
						else if (it->second == 999998)
						{
							if (iloop == 1)
							{
								std::string sString = it->first;
								replaceAll(sString, "ZZZZ-", "");
								ImGui::Text(sString.c_str());
								preview_count++;
								ImGui::NextColumn();
							}
						}
						else if (it->second > 0)
						{
							if (iloop == 0 && current_sort_order == SORT_PROJECTSITEMS)
							{
								// Collection Items List (5)
								ImGui::SetWindowFontScale(1.0);

								// should we show this item
								int entid = it->second;
								char cName[512];
								strcpy(cName, t.entityprofileheader[entid].desc_s.Get());
								bool DisplayEntry = false;
								int iCollectionItemIndex = -1;
								for (int c = 0; c < g_collectionList.size(); c++)
								{
									if (g_collectionList[c].iEntityID == entid)
									{
										iCollectionItemIndex = c;
										break;
									}
								}
								if ( iCollectionItemIndex != -1 )
								{
									DisplayEntry = true;
									if (strlen(cSearchEntities) > 0)
									{
										if (!pestrcasestr(cName, cSearchEntities))
											DisplayEntry = false;
									}
									if (t.entityprofile[entid].groupreference != -1)
									{
										DisplayEntry = false;
									}
								}
								if (DisplayEntry == true)
								{
									ImGui::PushID(uniqueId++);
									float fFramePadding = (iColumnsWidth_leftpanel - media_icon_size_leftpanel) * 0.5;
									float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
									ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };
									float fRatio = 288.0f / 512.0f;
									float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
									vIconSize = { fImageWidth ,fImageWidth * fRatio };
									char* cFind = strstr(cName, "###");
									if (cFind) cFind[0] = '\0';
									ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow; //Got sub selections.
									bool bSelected = false;
									ImGui::PushItemWidth(-20.0);
									std::string treename = cName;
									bool TreeNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(entid + 99000), node_flags, treename.c_str());
									bool bHovered = ImGui::IsItemHovered();
									ImGui::PopItemWidth();
									if (TreeNodeOpen)
									{
										ImGui::Indent(-5);
										bool bMoveCameraToObjectPosition = true;// false;
										DoTreeNodeEntity(entid, bMoveCameraToObjectPosition);
										ImGui::Indent(5);
										ImGui::TreePop();
									}
									ImGui::PopID();
									preview_count++;
									ImGui::NextColumn();
								}
							}
							else if (iloop == 0 && current_sort_order == SORT_GROUPITEMS)
							{
								// Group List (6)
								ImGui::SetWindowFontScale(1.0);

								// should we show this item
								int groupindex = it->second;
								char cName[512];
								strcpy(cName, sEntityGroupListName[groupindex].Get());
								if(strlen(cName)>4) cName[strlen(cName) - 4] = 0;
								if (strlen(cName) == 0) sprintf(cName, "Group %d", 1+groupindex);
								bool DisplayEntry = false;
								if (vEntityGroupList[groupindex].size() > 0)
								{
									DisplayEntry = true;
								}
								if (DisplayEntry == true)
								{
									ImGui::PushID(uniqueId++);
									float fFramePadding = (iColumnsWidth_leftpanel - media_icon_size_leftpanel) * 0.5;
									float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
									ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };
									float fRatio = 288.0f / 512.0f;
									float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
									vIconSize = { fImageWidth ,fImageWidth * fRatio };
									char* cFind = strstr(cName, "###");
									if (cFind) cFind[0] = '\0';
									ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow; //Got sub selections.
									bool bSelected = false;
									ImGui::PushItemWidth(-20.0);
									std::string treename = cName;
									bool TreeNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(groupindex + 99000), node_flags, treename.c_str());
									bool bHovered = ImGui::IsItemHovered();
									ImGui::PopItemWidth();
									if (TreeNodeOpen)
									{
										ImGui::Indent(-5);
										bool bMoveCameraToObjectPosition = true;
										DoTreeNodeGroup(groupindex, bMoveCameraToObjectPosition);
										ImGui::Indent(5);
										ImGui::TreePop();
									}
									ImGui::PopID();
									preview_count++;
									ImGui::NextColumn();
								}
							}
							else if (iloop == 0 && current_sort_order == SORT_INSTANCEITEMS)
							{
								// Instance List (7)
								ImGui::SetWindowFontScale(1.0);
								int e = it->second;
								bool DisplayEntry = true;
								if (DisplayEntry == true)
								{
									ImGui::PushID(uniqueId++);
									float fFramePadding = (iColumnsWidth_leftpanel - media_icon_size_leftpanel) * 0.5;
									float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
									ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };
									float fRatio = 288.0f / 512.0f;
									float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
									vIconSize = { fImageWidth ,fImageWidth * fRatio };
									char cName[512];
									strcpy(cName, t.entityelement[e].eleprof.name_s.Get());
									ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf;
									std::string treename = "#" + std::to_string(e);
									treename = treename + " " + cName;
									bool bAutoGenObject = false;
									if (t.entityelement[e].x == -99999 && t.entityelement[e].y == -99999 && t.entityelement[e].z == -99999)
									{
										treename = treename + " (Auto-Gen) ";
										bAutoGenObject = true;
									}
									if (t.entityelement[e].y == -999999)
									{
										treename = treename + " (Hidden) ";
										bAutoGenObject = true;
									}
									bool TreeNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(e + 99000), node_flags, treename.c_str());
									if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0))
									{
										// DUPLICATED ESEWHERE (LOTS) SO EVENTUALLY MAKE ONE FUNCTION FOR DIS.
										if (t.entityelement[e].obj > 0)
										{
											t.widget.pickedEntityIndex = e;
											t.widget.pickedObject = t.entityelement[t.widget.pickedEntityIndex].obj;
											g.entityrubberbandlist.clear();
											bEditorInFreeFlightMode = true;
											t.editorfreeflight.mode = 1;
											int group = isEntityInGroupList(t.widget.pickedEntityIndex);
											if (group >= 0)
											{
												//PE: Add all groups with entity to rubberband.
												CheckGroupListForRubberbandSelections(t.widget.pickedEntityIndex);
											}
											if (bAutoGenObject == false)
											{
												float zoom = ObjectSize(t.entityelement[e].obj, 1) * 2.0;
												if (zoom < 30.0f) zoom = 30.0f;
												float realcamy = ObjectSizeY(t.entityelement[e].obj, 1) * 0.75;
												float camy = realcamy;
												if (camy < 30.0f) camy = 30.0f;
												if (t.entityprofile[t.entityelement[e].bankindex].ismarker > 0)
												{
													zoom = 100.0;
													camy = 50.0;
												}
												PositionCamera(t.entityelement[e].x, t.entityelement[e].y, t.entityelement[e].z);
												PointCamera(t.entityelement[e].x, t.entityelement[e].y, t.entityelement[e].z);
												MoveCamera(0, -zoom);
												PositionCamera(CameraPositionX(0), t.entityelement[e].y + camy, CameraPositionZ(0));
												PointCamera(t.entityelement[e].x, t.entityelement[e].y + (realcamy * 0.5), t.entityelement[e].z);
												t.editorfreeflight.c.x_f = CameraPositionX();
												t.editorfreeflight.c.y_f = CameraPositionY();
												t.editorfreeflight.c.z_f = CameraPositionZ();
												t.editorfreeflight.c.angx_f = CameraAngleX();
												t.editorfreeflight.c.angy_f = CameraAngleY();
												t.cx_f = t.editorfreeflight.c.x_f;
												t.cy_f = t.editorfreeflight.c.z_f;
											}
										}
									}
									if (TreeNodeOpen) ImGui::TreePop();
									ImGui::PopID();
									preview_count++;
									ImGui::NextColumn();
								}
							}
							else if (iloop == 0 && current_sort_order == SORT_BEHAVIORITEMS)
							{
								// Behavior List (8)
								ImGui::SetWindowFontScale(1.0);
								int e = it->second;
								bool DisplayEntry = true;
								if (DisplayEntry == true)
								{
									ImGui::PushID(uniqueId++);
									float fFramePadding = (iColumnsWidth_leftpanel - media_icon_size_leftpanel) * 0.5;
									float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
									ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };
									float fRatio = 288.0f / 512.0f;
									float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
									vIconSize = { fImageWidth ,fImageWidth * fRatio };
									char cName[512];
									strcpy(cName, it->first.c_str());
									ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
									bool bSelected = false;
									ImGui::PushItemWidth(-20.0);
									std::string treename = cName;
									bool TreeNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(e + 99000), node_flags, treename.c_str());
									ImGui::PopItemWidth();
									if (TreeNodeOpen)
									{
										ImGui::Indent(-5);
										DoTreeNodeBehavior(cName, true);
										ImGui::Indent(5);
										ImGui::TreePop();
									}
									ImGui::PopID();
									preview_count++;
									ImGui::NextColumn();
								}
							}
							else if (iloop == 0 && (current_sort_order == SORT_DETAILEDITEMS || !bToggleThumbViews ))
							{
								// Detailed Object List (4)
								ImGui::SetWindowFontScale(1.0);

								bool DisplayEntry = true;
								char cName[512];
								strcpy(cName, t.entityprofileheader[it->second].desc_s.Get());

								if (strlen(cSearchEntities) > 0)
								{
									//PE: This will search the desc. and the object name.
									if (!pestrcasestr(cName, cSearchEntities))
										DisplayEntry = false;
								}
								if (current_sort_order != SORT_DETAILEDITEMS)
								{
									if (!bToggleThumbViews && (t.entityprofile[it->second].ismarker != 0 || t.entityprofile[it->second].ischildofgroup != 0))
									{
										DisplayEntry = false;
									}
								}

								bool bUseWideThumb = false;
								int iTextureID = t.entityprofile[it->second].iThumbnailSmall;
								if (t.entityprofile[it->second].iThumbnailLarge > 0)
								{
									bUseWideThumb = true;
									iTextureID = t.entityprofile[it->second].iThumbnailLarge;
								}

								if (DisplayEntry && iTextureID > 0)
								{
									ImGui::PushID(uniqueId++);
									float fFramePadding = (iColumnsWidth_leftpanel - media_icon_size_leftpanel)*0.5;
									float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
									ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };

									if (bUseWideThumb)
									{
										//512x288
										float fRatio = 288.0f / 512.0f;
										float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
										vIconSize = { fImageWidth ,fImageWidth*fRatio };
									}

									char *cFind = strstr(cName, "###");
									if (cFind)
										cFind[0] = '\0';

									if (t.entityprofile[it->second].groupreference != -1)
									{
										strcat(cName, " (Smart Object)");
									}
									ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow; //Got sub selections.

									bool bSelected = false;

									ImGui::PushItemWidth(-20.0); //PE: Room for a icon.
									std::string treename = cName;
									bool TreeNodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(it->second + 99000), node_flags, treename.c_str());
									bool bHovered = ImGui::IsItemHovered();
									if (bHovered == true)
									{
										if (ImGui::IsMouseDoubleClicked(0))
										{
											LPSTR pParentName = t.entityprofileheader[it->second].desc_s.Get();
											int iAnchorEntityIndex = -1;
											g.entityrubberbandlist.clear();
											for (int e = 1; e <= g.entityelementlist; e++)
											{
												int entid = t.entityelement[e].bankindex;
												if (entid > 0)
												{
													LPSTR pThisName = t.entityprofileheader[entid].desc_s.Get();
													if (stricmp(pParentName, pThisName) == NULL)
													{
														iAnchorEntityIndex = e;
														sRubberBandType rubberbandItem;
														rubberbandItem.e = e;
														rubberbandItem.x = t.entityelement[e].x;
														rubberbandItem.y = t.entityelement[e].y;
														rubberbandItem.z = t.entityelement[e].z;
														rubberbandItem.px = t.entityelement[e].x;
														rubberbandItem.py = t.entityelement[e].y;
														rubberbandItem.pz = t.entityelement[e].z;
														rubberbandItem.rx = t.entityelement[e].rx;
														rubberbandItem.ry = t.entityelement[e].ry;
														rubberbandItem.rz = t.entityelement[e].rz;
														rubberbandItem.quatmode = t.entityelement[e].quatmode;
														rubberbandItem.quatx = t.entityelement[e].quatx;
														rubberbandItem.quaty = t.entityelement[e].quaty;
														rubberbandItem.quatz = t.entityelement[e].quatz;
														rubberbandItem.quatw = t.entityelement[e].quatw;
														rubberbandItem.scalex = t.entityelement[e].scalex;
														rubberbandItem.scaley = t.entityelement[e].scaley;
														rubberbandItem.scalez = t.entityelement[e].scalez;
														g.entityrubberbandlist.push_back (rubberbandItem);
													}
												}
											}
											if (iAnchorEntityIndex != -1)
											{
												t.widget.pickedEntityIndex = iAnchorEntityIndex;
												t.widget.pickedObject = t.entityelement[iAnchorEntityIndex].obj;
												t.gridentity = 0;
											}
										}
									}
									ImGui::PopItemWidth();

									cstr find = t.entitybank_s[it->second];
									BeginDragDropFPE(find.Get(), iTextureID, bToolTipActive, vIconSize);
									if (!bHoveredUsed && bHovered && bToolTipActive && !bDraggingActive)
									{
										bHoveredUsed = true;
										ImGui::BeginTooltip();
										ImGui::ImgBtn(iTextureID, vIconSize, drawCol_back, drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, true, false, false, false, true, false);
										ImGui::EndTooltip();
									}

									bool bTreeNodeSelected = false;

									if (TreeNodeOpen) 
									{
										//Display any sub nodes
										ImGui::Indent(-5);
										DoTreeNodeEntity(it->second,true);
										ImGui::Indent(5);
										ImGui::TreePop();
									}

									if (bTreeNodeSelected)
									{
										//Only if we are not dragging in a trashcan.
										if (bToolTipActive)
										{
											if (bWaypointDrawmode || bWaypoint_Window) { bWaypointDrawmode = false; bWaypoint_Window = false; }
											if (g_bCharacterCreatorPlusActivated) g_bCharacterCreatorPlusActivated = false;
											if (bImporter_Window) { importer_quit(); bImporter_Window = false; }

											FreeTempImageList();
											DeleteWaypointsAddedToCurrentCursor();
											CloseDownEditorProperties();
											//Make sure we are in entity mode.
											bForceKey = true;
											csForceKey = "o";
											iExtractMode = 0; //PE: Always start in find floor mode.
											t.gridentity = it->second;
											t.inputsys.constructselection = it->second;
											t.inputsys.domodeentity = 1;
											t.grideditselect = 5;
											//Make sure we use a fresh t.grideleprof
											t.entid = t.gridentity;
											entity_fillgrideleproffromprofile();
											t.grideleprof.bUseFPESettings = true; //PE: New added always use bUseFPESettings.
											t.grideleprof.isProjectGlobal = false;

											t.inputsys.dragoffsetx_f = 0;
											t.inputsys.dragoffsety_f = 0;
											fHitPointX = 0;
											fHitPointY = HITPOINTYSTARTPOS;
											fHitPointZ = 0;
											fHitOffsetX = 0;
											fHitOffsetY = 0;
											fHitOffsetZ = 0;
											g_fSpecialDragInYAdjustment = 0.0f;
											bDraggingActive = false;
											g_bHoldGridEntityPosWhenManaged = true;
											g_fHoldGridEntityPosX = t.gridentityposx_f;
											g_fHoldGridEntityPosY = t.gridentityposy_f;
											g_fHoldGridEntityPosZ = t.gridentityposz_f;
											editor_refresheditmarkers();
											g_bSelectedNewObjectToAddToLevel = true;
										}
									}

									ImGui::PopID();
									preview_count++;
									ImGui::NextColumn();
								}
							}
							else
							{
								// no longer list markers in left entity panel, we have the game elements buttons now
								if ((iloop == 0 && t.entityprofile[it->second].ismarker == 0 && t.entityprofile[it->second].ischildofgroup == 0))
								{
									bool DisplayEntry = true;
									char cName[512];
									strcpy(cName, t.entityprofileheader[it->second].desc_s.Get());

									if (strlen(cSearchEntities) > 0)
									{
										//PE: This will search the desc. and the object name.
										if (!pestrcasestr(cName, cSearchEntities))
											DisplayEntry = false;
									}

									bool bUseWideThumb = false;
									int iTextureID = t.entityprofile[it->second].iThumbnailSmall;
									if (t.entityprofile[it->second].iThumbnailLarge > 0)
									{
										bUseWideThumb = true;
										iTextureID = t.entityprofile[it->second].iThumbnailLarge;
									}
									if (DisplayEntry && iTextureID > 0)
									{
										// get ready to overlay a smart object icon
										ImVec2 vSmartObjectIconPos = ImGui::GetCursorPos();

										ImGui::PushID(uniqueId++);
										float fFramePadding = (iColumnsWidth_leftpanel - media_icon_size_leftpanel)*0.5;
										float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
										ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };

										if (!bUseWideThumb)
										{
											ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (fCenterX - (media_icon_size_leftpanel*0.5)), ImGui::GetCursorPosY()));
										}
										else
										{
											//512x288
											float fRatio = 288.0f / 512.0f;
											float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
											vIconSize = { fImageWidth ,fImageWidth*fRatio };
										}

										// Entity Left Panel.
										if (ImGui::ImgBtn(iTextureID, vIconSize, drawCol_back, drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, false, false, false, false, true, false))
										{
											//Only if we are not dragging in a trashcan.
											if (bToolTipActive)
											{
												if (bWaypointDrawmode || bWaypoint_Window) { bWaypointDrawmode = false; bWaypoint_Window = false; }
												if (g_bCharacterCreatorPlusActivated) g_bCharacterCreatorPlusActivated = false;
												if (bImporter_Window) { importer_quit(); bImporter_Window = false; }

												FreeTempImageList();
												DeleteWaypointsAddedToCurrentCursor();
												CloseDownEditorProperties();
												//Make sure we are in entity mode.
												bForceKey = true;
												iExtractMode = 0; //PE: Always start in find floor mode.
												csForceKey = "o";
												t.gridentity = it->second;
												t.inputsys.constructselection = it->second;
												t.inputsys.domodeentity = 1;
												t.grideditselect = 5;
												//Make sure we use a fresh t.grideleprof
												t.entid = t.gridentity;
												entity_fillgrideleproffromprofile();
												t.grideleprof.bUseFPESettings = true; //PE: New added always use bUseFPESettings.
												t.grideleprof.isProjectGlobal = false;

												t.inputsys.dragoffsetx_f = 0;
												t.inputsys.dragoffsety_f = 0;
												fHitPointX = 0;
												fHitPointY = HITPOINTYSTARTPOS;
												fHitPointZ = 0;
												fHitOffsetX = 0;
												fHitOffsetY = 0;
												fHitOffsetZ = 0;
												g_fSpecialDragInYAdjustment = 0.0f;
												bDraggingActive = false;
												g_bHoldGridEntityPosWhenManaged = true;
												g_fHoldGridEntityPosX = t.gridentityposx_f;
												g_fHoldGridEntityPosY = t.gridentityposy_f;
												g_fHoldGridEntityPosZ = t.gridentityposz_f;
												editor_refresheditmarkers();
												g_bSelectedNewObjectToAddToLevel = true;
											}
										}

										cstr find = t.entitybank_s[it->second];
										BeginDragDropFPE(find.Get(), iTextureID, bToolTipActive, vIconSize);

										char *cFind = strstr(cName, "###");
										if (cFind)
											cFind[0] = '\0';
										if (ImGui::IsItemHovered() && bToolTipActive) ImGui::SetTooltip("%s", cName);

										if (bDisplayText_leftpanel)
										{
											ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -5.0f));
											ImGui::TextCenter("%s", cName); //no wrap.
										}

										// show when object is a smart object
										if (t.entityprofile[it->second].groupreference != -1)
										{
											int iImageSize = 20;
											ImVec2 opos = ImGui::GetCursorPos();
											ImGui::SetCursorPos(ImVec2(vSmartObjectIconPos.x + vIconSize.x - 17.0f, vSmartObjectIconPos.y - 19.0f + vIconSize.y));
											ImGui::SetItemAllowOverlap();
											if (ImGui::ImgBtn(TOOL_SMARTOBJECT, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
											{
												// clicking does nothing
											}
											ImGui::SetCursorPos(opos);
											if (ImGui::IsItemHovered()) ImGui::SetTooltip("This is a Smart Object");
										}

										ImGui::PopID();
										preview_count++;
										ImGui::NextColumn();
									}
								}
							}
						}
					}
				}
				ImGui::Columns(1);
			}

			ImGui::SetWindowFontScale(1.00);
			if (ImGui::IsWindowHovered() || ImGui::IsAnyItemHovered())
				bImGuiGotFocus = true;

			//PE: Doubble click to move closer to terrain. disabled for objects as we might end up into walls ...
			extern float fLastTerrainHitX, fLastTerrainHitY, fLastTerrainHitZ;
			if (!bImGuiGotFocus && bImGuiRenderTargetFocus && g_hovered_pobject == 0 && t.thighlighterobj == 0)
			{
				if (fLastTerrainHitY > g.gdefaultwaterheight && t.grideditselect == 5)
				{
					static float traveltox = 0, traveltoy = 0, traveltoz = 0;
					if(ImGui::IsMouseDoubleClicked(0))
					{
						//PE: Get a fresh.
						wiInput::MouseState mouseState = wiInput::GetMouseState();
						RAY pickRay = wiRenderer::GetPickRay((long)mouseState.position.x, (long)mouseState.position.y, master.masterrenderer);
						if (!GGTerrain::GGTerrain_RayCast(pickRay, &fLastTerrainHitX, &fLastTerrainHitY, &fLastTerrainHitZ, 0, 0, 0, 0))
						{
							fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;
						}
						else
						{
							if (fLastTerrainHitY > g.gdefaultwaterheight)
							{
								float composx, composy, composz, comangx, comangy, comangz;

								composx = CameraPositionX(0);
								composy = CameraPositionY(0);
								composz = CameraPositionZ(0);
								comangx = CameraAngleX(0);
								comangy = CameraAngleY(0);
								comangz = CameraAngleZ(0);

								float step = 10; //PE: This will move us 10% from the total distance to the target.
								float dx = (composx-fLastTerrainHitX) / step;
								float dy = (composy-fLastTerrainHitY) / step;
								float dz = (composz-fLastTerrainHitZ) / step;

								traveltox = fLastTerrainHitX + dx;
								traveltoy = fLastTerrainHitY + dy;
								traveltoz = fLastTerrainHitZ + dz;

								t.editorfreeflight.mode = 3;
								t.editorfreeflight.s.x_f = traveltox;
								t.editorfreeflight.s.y_f = traveltoy;
								t.editorfreeflight.s.z_f = traveltoz;
								PositionCamera(traveltox, traveltoy, traveltoz);
								PointCamera(fLastTerrainHitX, fLastTerrainHitY, fLastTerrainHitZ);
								t.editorfreeflight.s.angx_f = CameraAngleX(0);
								t.editorfreeflight.s.angy_f = CameraAngleY(0);
								t.editorfreeflight.c = t.editorfreeflight.s;
								PositionCamera(composx, composy, composz);
								RotateCamera(comangx, comangy, comangz);
							}
						}
					}
				}
			}

			if (bProfilerEnable)
			{
				ImGui::Separator();
				wiScene::Scene* pScene = &wiScene::GetScene();

				float cpuFrameMs = wi::profiler::GetCPUFrameTime();
				float gpuFrameMs = wi::profiler::GetGPUFrameTime();

				ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
				ImGui::Text("CPU Frame: %.2f ms", cpuFrameMs);
				ImGui::Text("GPU Frame: %.2f ms", gpuFrameMs);
				ImGui::Text("Scene Meshes: %d", (int)pScene->meshes.GetCount());
				ImGui::Text("Scene Materials: %d", (int)pScene->materials.GetCount());
				ImGui::Text("Scene Transforms: %d", (int)pScene->transforms.GetCount());
				ImGui::Text("Scene Hierarchy: %d", (int)pScene->hierarchy.GetCount());

				ImGui::Separator();
				// GGMAX 2.94: read the CACHED snapshot, not a second live GetTextData().
				// GetTextData() only accumulates ranges whose in_use is still set, and it
				// PRINTS-AND-ZEROES the persistent row cache as it goes - so a second caller
				// in the same frame races the first and one of them renders every row as
				// 0.00 ms while the headline GPU Frame/Busy lines (which are read directly,
				// not through that cache) stay correct. That is exactly the symptom seen on
				// the Performance panel. Same fix already applied to the other panel in
				// M-GridEditB_part2.cpp:1074.
				extern std::string GGPerf_GetCachedProfilerText();
				std::string profiler_data = GGPerf_GetCachedProfilerText();
				if (!profiler_data.empty())
					ImGui::TextUnformatted(profiler_data.c_str());

			}

			if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0) 
			{
				//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
				ImGui::Text("");
				ImGui::Text("");
				ImGui::Text("");
			}

			ImGui::EndChild();

			// number of game element buttson shown
			entity_icons = 12;
			if (pref.iObjectEnableAdvanced)
				entity_icons = 16;

			int entity_images[] = { ENTITY_START, ENTITY_CHECKPOINT, ENTITY_FLAG, ENTITY_TRIGGERZONE, ENTITY_WIN, ENTITY_LIGHT,ENTITY_VIDEO,ENTITY_MUSIC,ENTITY_SOUND,ENTITY_PARTICLE,ENTITY_IMAGE, ENTITY_TEXT, ENTITY_PROBE, ENTITY_COVER, ENTITY_BEHAVIOR, ENTITY_NEW_PARTICLE };
			static cstr entity_scripts[] = {
				"_markers\\Player Start.fpe",
				"_markers\\Player Checkpoint.fpe",
				"_markers\\flag.fpe" ,
				"_markers\\Trigger Zone.fpe",
				"_markers\\Win Zone.fpe",
				"_markers\\White Light.fpe",
				"_markers\\Video Zone.fpe",
				"_markers\\Ambience Zone.fpe",
				"_markers\\Audio Zone.fpe",
				"_markers\\Particles.fpe",
				"_markers\\Image Zone.fpe",
				"_markers\\Text Zone.fpe",
				"_markers\\Probe.fpe",
				"_markers\\Cover Zone.fpe",
				"_markers\\Behavior.fpe", //global Behaviors
				"_markers\\NewParticles.fpe"
			};
			static cstr entity_tooltip[] = {
				"Add Player Start Position",
				"Add Player Checkpoint",
				"Add Flag",
				"Add Trigger Zone",
				"Add Win Zone",
				"Add Light",
				"Add Video Zone",
				"Add Music Zone",
				"Add Audio Zone",
				"Add New Particle",
				"Add Image Zone",
				"Add Text Zone",
				"Add Environment Probe",
				"Add Cover Zone",
				"Add Global Behavior",
				"Add WPE Zone"
			};

			int offset = 0;
			if (bViewOptionsOpen)
				offset = 225;// 205;// 115;
			if (entity_icons_columns > 10 && entity_icons_columns < 15)
				offset += entity_image_size;

			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, ImGui::GetContentRegionAvail().y - offset
				- ((entity_w / entity_icons_columns) * iIconRows) - ImGui::GetFontSize() * 4.0f + 10.0f));
			ImGui::TextCenter("Game Elements");

			ImVec4 IconColor = ImVec4(1.0, 1.0, 1.0, 1.0);
			ImGui::Indent(4);
			for (int i = 0; i < entity_icons; i++)
			{
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(fSpacer, 0.0f));

				if (ImGui::ImgBtn(entity_images[i], ImVec2(entity_image_size, entity_image_size), ImVec4(0.0, 0.0, 0.0, 0.0), IconColor, ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, false))
				{
					FreeTempImageList(); //PE: Whenever g.entidmaster can change we must make sure to free any "temp" objects loaded.

					t.addentityfile_s = entity_scripts[i];
					if (t.addentityfile_s != "")
					{
						entity_adduniqueentity(false);
						t.tasset = t.entid;
						if (t.talreadyloaded == 0)
						{
							editor_filllibrary();
						}
					}

					iExtractMode = 0; //PE: Always start in find floor mode.
					t.inputsys.constructselection = t.tasset;
					t.gridentity = t.entid;
					t.inputsys.constructselection = t.entid;
					t.inputsys.domodeentity = 1;
					t.grideditselect = 5;
					Entity_Tools_Window = true;
					//Make sure we use a fresh t.grideleprof
					entity_fillgrideleproffromprofile();
					t.grideleprof.bUseFPESettings = true; //PE: New added always use bUseFPESettings.
					t.grideleprof.isProjectGlobal = false;

					editor_refresheditmarkers();

					// Show elements when placing a new one down, prevents half being hidden and half not.
					t.showeditorelements = 1;
					editor_toggle_element_vis(t.showeditorelements);

				}
				if (ImGui::IsItemHovered() && bToolTipActive) ImGui::SetTooltip(entity_tooltip[i].Get());
				BeginDragDropFPE(entity_scripts[i].Get(), entity_images[i], bToolTipActive, ImVec2(entity_image_size, entity_image_size));

				ImVec2 restore_cursorpos = ImGui::GetCursorPos();
				if ((i + 1) % entity_icons_columns != 0 && i != entity_icons - 1)
					ImGui::SameLine();
			}

			content_avail = ImGui::GetContentRegionAvail();
			content_avail.y -= 6.0;

			bViewOptionsOpen = false;

			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, 3.0f));
			ImGui::Indent(-4);

			if (ImGui::StyleCollapsingHeader("View Options##viewoptions"))
			{
				bViewOptionsOpen = true;
				ImGui::Columns(3);

				ImGuiWindow* win = ImGui::GetCurrentWindow();
				win->DC.CurrentColumns->Flags |= ImGuiColumnsFlags_NoResize;

				ImGui::SetColumnWidth(0, content_avail.x * 0.625f);
				ImGui::TextCenter("");

				float fFontSize = ImGui::GetFontSize();

				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize*0.25f));
				ImGui::Text("Game Elements");
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize * 0.5f));
				ImGui::Text("Editable Area 3D Edge");
				if (t.visuals.bEnableEmptyLevelMode == false)
				{
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize * 0.5f));
					ImGui::Text("Editable Area 2D Edge");
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize * 0.5f));
					ImGui::Text("Trees");
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize * 0.5f));
					ImGui::Text("Vegetation");
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize * 0.5f));
					ImGui::Text("Water");
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, fFontSize * 0.5f));
					ImGui::Text("Terrain");
				}
				ImGui::NextColumn();
				ImGui::SetColumnWidth(1, content_avail.x * 0.2f);
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(-1.0f, 0.0f));
				ImGui::Text("Editor");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Changes to the Editor View settings are temporary and will only effect visuals whilst editing your levels");
				
				// Editor game elements.
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
				bool bShow = t.showeditorelements;
				if (ImGui::Checkbox("##EditorElements", &bShow))
				{
					t.showeditorelements = bShow;
					editor_toggle_element_vis(bShow);
				}
				// Editor 3D boundary.
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
				bShow = (ggterrain_global_render_params2.flags2 & GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE_3D) != 0;
				if (ImGui::Checkbox("##Editor3DBounds", &bShow))
				{
					if (bShow) ggterrain_global_render_params2.flags2 |= GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE_3D;
					else ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE_3D;
				}
				// Regular mode
				if (t.visuals.bEnableEmptyLevelMode == false)
				{
					// Editor 2D boundary.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
					bShow = (ggterrain_global_render_params2.flags2 & GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE) ? 1 : 0;
					if (ImGui::Checkbox("##Editor2DBounds", &bShow))
					{
						if (bShow) ggterrain_global_render_params2.flags2 |= GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE;
						else ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE;
					}
					// Editor Trees.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
					if (t.showeditortrees < 0)
						t.showeditortrees = t.visuals.bEndableTreeDrawing;
					bShow = t.showeditortrees;
					if (ImGui::Checkbox("##EditorTrees", &bShow))
					{
						ggtrees_global_params.draw_enabled = bShow;
						t.showeditortrees = bShow;
					}

					// Editor Vegetation.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
					if (t.showeditorveg < 0)
						t.showeditorveg = t.visuals.bEndableGrassDrawing;
					bShow = t.showeditorveg;
					if (ImGui::Checkbox("##EditorVeg", &bShow))
					{
						gggrass_global_params.draw_enabled = bShow;
						t.showeditorveg = bShow;
						// UI AUDIT 2026-07-28: apply immediately (sweeps the live Wicked
						// grass entities) — same pattern as the Editor Water checkbox.
						Wicked_Update_Visuals((void*)&t.visuals);
					}

					// Editor Water.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
					if (t.showeditorwater < 0)
						t.showeditorwater = t.visuals.bWaterEnable;
					bShow = t.showeditorwater;
					if (ImGui::Checkbox("##EditorWater", &bShow))
					{
						t.showeditorwater = bShow;
						Wicked_Update_Visuals((void*)&t.visuals);
					}

					// Editor Terrain.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(4.0f, 0.0f));
					if (t.showeditorterrain < 0)
						t.showeditorterrain = t.visuals.bEndableTerrainDrawing;
					bShow = t.showeditorterrain;
					if (ImGui::Checkbox("##EditorTerrain", &bShow))
					{
						t.showeditorterrain = bShow;
						Wicked_Update_Visuals((void*)&t.visuals);
					}
				}
				ImGui::NextColumn();
								
				ImGui::SetColumnWidth(2, content_avail.x * 0.18f);
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(1.0f, 0.0f));
				ImGui::Text("Level");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Changes to the Level View settings will effect the visuals seen in-game");
				
				// Test level game elements.
				bShow = t.showtestgameelements;
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
				if (ImGui::Checkbox("##LevelElements", &bShow))	t.showtestgameelements = bShow;
				// Test level 3D boundary.
				bShow = t.showtestgame3dbounds;
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
				if (ImGui::Checkbox("##Level3DBounds", &bShow))	t.showtestgame3dbounds = bShow;
				// Regular mode
				if (t.visuals.bEnableEmptyLevelMode == false)
				{
					// Test level 2D boundary.
					bShow = t.showtestgame2dbounds;
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
					if (ImGui::Checkbox("##Level2DBounds", &bShow))	t.showtestgame2dbounds = bShow;
					// Test level Trees.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
					if (ImGui::Checkbox("##LevelTrees", &t.visuals.bEndableTreeDrawing))
						t.gamevisuals.bEndableTreeDrawing = t.visuals.bEndableTreeDrawing;

					// Test level Vegetation.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
					if (ImGui::Checkbox("##LevelVeg", &t.visuals.bEndableGrassDrawing))
						t.gamevisuals.bEndableGrassDrawing = t.visuals.bEndableGrassDrawing;

					// Test level Water.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
					if (ImGui::Checkbox("##LevelWater", &t.visuals.bWaterEnable))
						t.gamevisuals.bWaterEnable = t.visuals.bWaterEnable;

					// Test level Terrain.
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(5.0f, 0.0f));
					if (ImGui::Checkbox("##LevelTerrain", &t.visuals.bEndableTerrainDrawing))
						t.gamevisuals.bEndableTerrainDrawing = t.visuals.bEndableTerrainDrawing;
				}
				ImGui::Columns(1);
			}

			int iGridObj = g.ebeobjectbankoffset + 1000;
			if (!bImGuiInTestGame && bEmptyLevelGrid)
			{
				if (!ObjectExist(iGridObj))
				{
					//PE: TODO need to exclude this mesh from select outline.
					WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);
					MakeObjectPlane(iGridObj, 6000, 6000);
					WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
					XRotateObject(iGridObj, 90);
					DisableObjectZDepth(iGridObj);
					sObject* pObject = GetObjectData(iGridObj);
					if (pObject)
					{
						WickedCall_SetObjectCastShadows(pObject, false);
						WickedCall_SetObjectLightToUnlit(pObject, (int)wiScene::MaterialComponent::SHADERTYPE_UNLIT);
						WickedCall_SetObjectDisableDepth(pObject, true);
					}

					float ShaderParam1 = 2.0; //THICKNESS_FACTOR
					float ShaderParam2 = 3000; //FADE_DISTANCE
					float ShaderParam3 = 0.4f; //POWER_EXPONENT
					float ShaderParam4 = 0.4f; //BASE ALPHA
					void importer_set_all_material_shader_id(int obj, int shaderID, float p1, float p2, float p3, float p4, float p5, float p6, float p7);
					importer_set_all_material_shader_id(iGridObj, 4, ShaderParam1, ShaderParam2, ShaderParam3, ShaderParam4, 0, 0, 0);
				}
				float camx = CameraPositionX();
				float camy = CameraPositionY();
				float camz = CameraPositionZ();
				PositionObject(iGridObj, camx, fEmptyLevelFloorY, camz);

			}
			else if (!bEmptyLevelGrid)
			{
				if (ObjectExist(iGridObj))
				{
					DeleteObject(iGridObj);
				}
			}

			//Drag/Drop to remove objects.
			ImRect bb = { ImGui::GetWindowContentRegionMin()+ImGui::GetWindowPos(),ImGui::GetWindowContentRegionMax() + ImGui::GetWindowPos() };

			if (bTrashcanIconActive)
				bTrashcanIconActive = false;
			DragDrop_CheckTrashcanDrop(bb);

			if (ImGui::BeginDragDropTargetCustom(bb, 12345))
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_MODEL_DROP_TARGET", 0)) // ImGuiDragDropFlags_AcceptNoDrawDefaultRect
				{
					AddPayLoad((ImGuiPayload*)payload, false);
				}
			}

			// note to show we are in completely empty level mode
			if (t.visuals.bEnableEmptyLevelMode == true)
			{
				// this mode is activated when the terrain is first generated
				ImGui::TextCenter("");
				ImGui::TextCenter("Completely Empty Level Mode");
				ImGui::TextCenter("");
				ImGui::TextCenter("Terrain, water and other default");
				ImGui::TextCenter("elements removed from this level");
			}

			ImGui::End();

			//#########################
			//#### Current objects ####
			//#########################
			
			ImGui::Begin("Current Objects##AdditionalIconsWindow", &bAlwaysOpen, iGenralWindowsFlags | ImGuiWindowFlags_NoTitleBar);

			static ImVec2 vBelowContentSize = { 0.0,40.0 };
			bool bSelectionAvail = false;
			int control_image_size = 42; //32;

			//PE: Make room for tool icons.
			content_avail = ImGui::GetContentRegionAvail();
			if (vBelowContentSize.y != 40.0f) {
				if (vBelowContentSize.y > 84) //2 lines of icons max. otherwise scrollbar.
					vBelowContentSize.y = 84;
				content_avail.y -= (vBelowContentSize.y + 8.0f);
			}
			else
				content_avail.y -= fsy * 3.0;
			if (content_avail.y < fsy) content_avail.y = fsy;

			ImGui::BeginChild("##CurrentObjectsLeftPanel", content_avail, false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavInputs);

			extern sObject* g_highlight_pobject;
			extern std::vector<sRubberBandType> entityselectionlist;
			g_highlight_pobject = NULL;
			entityselectionlist.clear();
			int iUniqueTreeId = 67671;

			#define GROUPV2


			int iInsideTab = 0;
			if (ImGui::BeginTabBar("currentandgrouptabbar"))
			{
				int tabflags = 0;
				if (i_switch_group_tab == 1)
				{
					i_switch_group_tab = 0;
					tabflags = ImGuiTabItemFlags_SetSelected;
				}
				if (ImGui::BeginTabItem(" Current Objects ", NULL, tabflags))
				{
					iInsideTab = 1;
					tabflags = 0;

					ImGui::SetWindowFontScale(fFontSize_leftpanel);
					//PE: To stop flicker of icons in list. it can take some frames before t.widget.pickedObject is set.
					static int iLastWidgetPickedObject[4] = { -1,-1,-1,-1 };
					if (t.gridentity <= 0)
					{
						iLastWidgetPickedObject[0] = iLastWidgetPickedObject[1];
						iLastWidgetPickedObject[1] = iLastWidgetPickedObject[2];
						iLastWidgetPickedObject[2] = t.widget.pickedObject;
					}

					int iFirstIcon = -1;
					int iFirstEntityId = -1;
					if (t.gridentity > 0)
					{
						iFirstIcon = t.gridentity;
						iLastWidgetPickedObject[2] = -1;
					}
					else if (t.widget.pickedEntityIndex > 0) // t.widget.pickedObject > 0 && 
					{
						//Ignore picked if rubberband.
						if (!g.entityrubberbandlist.size() > 0)
						{
							int bankindex = t.entityelement[t.widget.pickedEntityIndex].bankindex;
							iFirstIcon = bankindex;
							iFirstEntityId = t.widget.pickedEntityIndex;
						}
					}

					ImGui::Columns(iColumns_leftpanel, "CurrentObjectsAdditional", false);  //false no border

					if (iFirstIcon > 0 && t.gridentityinzoomview == 0)
					{

						bool bUseWideThumb = false;
						int iTextureID = t.entityprofile[iFirstIcon].iThumbnailSmall;
						#ifdef USEWIDEICONSEVERYWHERE
						if (t.entityprofile[iFirstIcon].iThumbnailLarge > 0)
						{
							bUseWideThumb = true;
							iTextureID = t.entityprofile[iFirstIcon].iThumbnailLarge;
						}
						#endif

						if (iTextureID > 0)
						{

							bSelectionAvail = true;
							bool isThumbHovered = false;
							ImGui::PushID(uniqueId++);
							float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
							ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };

							if (!bUseWideThumb)
							{
								ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (fCenterX - (media_icon_size_leftpanel*0.5)), ImGui::GetCursorPosY()));
							}
							else
							{
								//512x288
								float fRatio = 288.0f / 512.0f;
								float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
								vIconSize = { fImageWidth ,fImageWidth*fRatio };
							}

							ImVec2 vToolsPos = ImGui::GetCursorPos();
							
							if (ImGui::ImgBtn(iTextureID, vIconSize, drawCol_back, drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, true))
							{
								//Add to cursor.
								if (t.widget.pickedObject > 0 && t.widget.pickedEntityIndex > 0)
								{
									AddEntityToCursor(t.widget.pickedEntityIndex);
									// AddEntityToCursor used to both pick up an object, but in this case
									// we are duplicating, so we do need to wipe out certain real-time per-instance data
									t.grideleprof.newparticle.emitterid = -1; //LB: Must always get a new emitter ID.
									entity_cleargrideleprofrelationshipdata();
									bDraggingActive = false;
								}
							}
							if (ImGui::IsItemHovered())
								isThumbHovered = true;

							if (bWaitOnMouseRelease)
							{
								if (!ImGui::IsMouseDown(0))
									bWaitOnMouseRelease = false;
							}

							if (pref.iEnableDragDropEntityMode && !bWaitOnMouseRelease && t.gridentity == 0 && t.gridentityobj == 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
							{
								if (t.widget.pickedObject > 0 && t.widget.pickedEntityIndex > 0)
								{
									StartDragDropFromEntityID(t.widget.pickedEntityIndex);
								}
							}

							//#### Locked Objects ####
							bool isObjectInLocedList = false;
							int iObjectLockedIndix = -1;
							if (vEntityLockedList.size() > 0)
							{
								for (int i = 0; i < vEntityLockedList.size(); i++)
								{
									int e = vEntityLockedList[i].e;
									if (e < 0 || e >= t.entityelement.size()) continue;

									if (e == t.widget.pickedEntityIndex)
									{
										isObjectInLocedList = true;
										iObjectLockedIndix = i;
										break;
									}
								}
							}
							if (isObjectInLocedList) 
							{
								int iImageSize = 20;
								ImVec2 opos = ImGui::GetCursorPos();
								ImGui::SetCursorPos(ImVec2(vToolsPos.x + vIconSize.x - 17.0f, vToolsPos.y - 19.0f + vIconSize.y));
								ImGui::SetItemAllowOverlap();
								if (ImGui::ImgBtn(TOOL_LOCK, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
								{
									LockSelectedObject(false, iObjectLockedIndix);
								}
								ImGui::SetCursorPos(opos);
								if (ImGui::IsItemHovered() && bToolTipActive) 
								{
									isThumbHovered = false;
									ImGui::SetTooltip("UnLock Object");
								}
							}
							else
							{
								int iImageSize = 20;
								ImVec2 opos = ImGui::GetCursorPos();
								ImGui::SetCursorPos(ImVec2(vToolsPos.x + vIconSize.x - 17.0f, vToolsPos.y - 19.0f + vIconSize.y));
								ImGui::SetItemAllowOverlap();
								if (ImGui::ImgBtn(TOOL_UNLOCK, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
								{
									LockSelectedObject(true, iObjectLockedIndix);
								}
								ImGui::SetCursorPos(opos);
								if (ImGui::IsItemHovered() && bToolTipActive) 
								{
									isThumbHovered = false;
									ImGui::SetTooltip("Lock Object");
								}
							}

							if (!bToolTipActive)
								isThumbHovered = false;

							static int ContextSel = -1;
							if (isThumbHovered || ContextSel == iFirstIcon)
							{
								if (ImGui::BeginPopupContextWindow())
								{
									ContextSel = iFirstIcon;
									ListGroupContextMenu(true);
									ImGui::EndPopup();
								}
							}

							if (isThumbHovered) {
								ImGui::SetTooltip("%s", t.entityprofileheader[iFirstIcon].desc_s.Get());
								if (iFirstEntityId > 0 && t.entityelement[iFirstEntityId].obj > 0) {
									if (g_ObjectList[t.entityelement[iFirstEntityId].obj]) {
										g_highlight_pobject = g_ObjectList[t.entityelement[iFirstEntityId].obj];
									}
								}

							}

							if (bDisplayText_leftpanel) {
								#ifdef USEWIDEICONSEVERYWHERE
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -5.0f));
								#endif
								if (iFirstEntityId > 0)
									ImGui::TextCenter("%s", t.entityelement[iFirstEntityId].eleprof.name_s.Get()); //no wrap.
								else
									ImGui::TextCenter("%s", t.entityprofileheader[iFirstIcon].desc_s.Get()); //no wrap.
							}
							ImGui::PopID();
							preview_count++;
							ImGui::NextColumn();
						}
					}

					if (g.entityrubberbandlist.size() > 0 && t.gridentityinzoomview == 0)
					{
						bSelectionAvail = true;
						for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
						{
							bool bValid = true;
							int e = g.entityrubberbandlist[i].e;
							int tobj = t.entityelement[e].obj;
							int bankindex = t.entityelement[e].bankindex;

							//PE: Make sure we dont display the one attached to the cursor, already displayed and is not valid.
							if (bankindex == 0) bValid = false;

							if (bValid)
							{
								bool bUseWideThumb = false;
								int iTextureID = t.entityprofile[bankindex].iThumbnailSmall;

#ifdef USEWIDEICONSEVERYWHERE
								if (t.entityprofile[bankindex].iThumbnailLarge > 0)
								{
									bUseWideThumb = true;
									iTextureID = t.entityprofile[bankindex].iThumbnailLarge;
								}
#endif

								if (iTextureID > 0)
								{
									bool isThumbHovered = false;
									ImGui::PushID(uniqueId++);
									float fCenterX = ImGui::GetContentRegionAvail().x * 0.5;
									ImVec2 vIconSize = { (float)media_icon_size_leftpanel , (float)media_icon_size_leftpanel };

									if (!bUseWideThumb)
									{
										ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (fCenterX - (media_icon_size_leftpanel*0.5)), ImGui::GetCursorPosY()));
									}
									else
									{
										//512x288
										float fRatio = 288.0f / 512.0f;
										float fImageWidth = ImGui::GetContentRegionAvail().x - 4.0f;
										vIconSize = { fImageWidth ,fImageWidth*fRatio };
									}

									ImVec2 vToolsPos = ImGui::GetCursorPos();
									if (ImGui::ImgBtn(iTextureID, vIconSize, drawCol_back, drawCol_normal, drawCol_hover, drawCol_Down, -1, 0, 0, 0, true))
									{
										//Copy to cursor.
										AddEntityToCursor(e);
										bDraggingActive = false;
									}
									if (ImGui::IsItemHovered())
										isThumbHovered = true;

									if (bWaitOnMouseRelease)
									{
										if (!ImGui::IsMouseDown(0))
											bWaitOnMouseRelease = false;
									}

									if (pref.iEnableDragDropEntityMode && !bWaitOnMouseRelease && t.gridentity == 0 && t.gridentityobj == 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
									{
										StartDragDropFromEntityID(e);
									}

									//#### Locked Objects ####
									bool isObjectInLocedList = false;
									int iObjectLockedIndix = -1;
									if (vEntityLockedList.size() > 0)
									{
										for (int i = 0; i < vEntityLockedList.size(); i++)
										{
											if (e == vEntityLockedList[i].e)
											{
												isObjectInLocedList = true;
												iObjectLockedIndix = i;
												break;
											}
										}
									}
									if (isObjectInLocedList) 
									{
										int iImageSize = 20;
										ImVec2 opos = ImGui::GetCursorPos();
										ImGui::SetCursorPos(ImVec2(vToolsPos.x + vIconSize.x - 17.0f, vToolsPos.y - 19.0f + vIconSize.y));
										ImGui::SetItemAllowOverlap();
										if (ImGui::ImgBtn(TOOL_LOCK, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
										{
											if (iObjectLockedIndix >= 0) 
											{
												t.entityelement[e].editorlock = 0;
												sObject* pObject;
												if (t.entityelement[e].obj > 0) 
												{
													pObject = g_ObjectList[t.entityelement[e].obj];
													if (pObject) 
													{
														WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
													}
												}
												vEntityLockedList.erase(vEntityLockedList.begin() + iObjectLockedIndix);
											}
											// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
											t.widget.pickedObject = 0;
										}
										ImGui::SetCursorPos(opos);
										if (ImGui::IsItemHovered() && bToolTipActive) 
										{
											isThumbHovered = false;
											ImGui::SetTooltip("UnLock Object");
										}
									}
									else
									{
										int iImageSize = 20;
										ImVec2 opos = ImGui::GetCursorPos();
										ImGui::SetCursorPos(ImVec2(vToolsPos.x + vIconSize.x - 17.0f, vToolsPos.y - 19.0f + vIconSize.y));
										ImGui::SetItemAllowOverlap();
										if (ImGui::ImgBtn(TOOL_UNLOCK, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
										{
											t.entityelement[e].editorlock = 1 - t.entityelement[e].editorlock;
											sObject* pObject;
											if (t.entityelement[e].obj > 0) 
											{
												pObject = g_ObjectList[t.entityelement[e].obj];
												if (pObject) {
													if (t.entityelement[e].editorlock)
													{
														#ifndef ALLOWSELECTINGLOCKEDOBJECTS
														WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_CURSOROBJECT);
														#endif
														sRubberBandType vEntityLockedItem;
														vEntityLockedItem.e = e;
														vEntityLockedList.push_back(vEntityLockedItem);
													}
													else 
													{
														//Delete from list.
														for (int i = 0; i < vEntityLockedList.size(); i++)
														{
															if (vEntityLockedList[i].e == e) 
															{
																vEntityLockedList.erase(vEntityLockedList.begin() + i);
																break;
															}
														}
														WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
													}
												}
											}
											// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
											t.widget.pickedObject = 0;
										}
										ImGui::SetCursorPos(opos);
										if (ImGui::IsItemHovered() && bToolTipActive) 
										{
											isThumbHovered = false;
											ImGui::SetTooltip("Lock Object");
										}
									}

									if (!bToolTipActive)
										isThumbHovered = false;

									static int ContextSel = -1;
									if (isThumbHovered || ContextSel == e)
									{
										if (ImGui::BeginPopupContextWindow())
										{
											ContextSel = e;
											ListGroupContextMenu(true, e);
											ImGui::EndPopup();
										}
									}

									if (isThumbHovered) {
										ImGui::SetTooltip("%s", t.entityprofileheader[bankindex].desc_s.Get());
										if (e > 0 && t.entityelement[e].obj > 0) {
											if (g_ObjectList[t.entityelement[e].obj]) {
												g_highlight_pobject = g_ObjectList[t.entityelement[e].obj];
											}
										}
									}

									if (bDisplayText_leftpanel) {
#ifdef USEWIDEICONSEVERYWHERE
										ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, -5.0f));
#endif
										if(e > 0)
											ImGui::TextCenter("%s", t.entityelement[e].eleprof.name_s.Get()); //no wrap.
										else
											ImGui::TextCenter("%s", t.entityprofileheader[bankindex].desc_s.Get()); //no wrap.
									}
									ImGui::PopID();
									preview_count++;
									ImGui::NextColumn();
								}
							} //BValid
						}
					}

					ImGui::Columns(1);
					ImGui::SetWindowFontScale(1.00);

					if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0) {
						//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
						ImGui::Text("");
						ImGui::Text("");
						ImGui::Text("");
					}

					ImGui::EndTabItem();
				}

				tabflags = 0;
				if (i_switch_group_tab == 2)
				{
					i_switch_group_tab = 0;
					tabflags = ImGuiTabItemFlags_SetSelected;
				}
				if (ImGui::BeginTabItem(" Groups ", NULL, tabflags))
				{
					if (ImGui::IsItemHovered() && bToolTipActive) ImGui::SetTooltip("Object Groups");

					//Use Columns and fixed size.
					ImGui::Columns(iColumns_leftpanel, "CurrentObjectsAdditional", false);  //false no border
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
					iInsideTab = 2;
					
					//#### Group Lists ####
					for (int l = 0; l < MAXGROUPSLISTS; l++)
					{
						if (vEntityGroupList[l].size() > 0)
						{
							//LB: quickly reject groups that have no image (i.e are child groups)
							if (iEntityGroupListImage[l] == 0)
								continue;

							//LB: reject groups that have names (which means they are groups for Smart Objects, not user created)
							if (sEntityGroupListName[l].Len() > 0)
								continue;

							cstr sGroupString = cstr("Group") + cstr(l + 1) + cstr(":") + cstr("Objects (") + cstr((int)vEntityGroupList[l].size()) + cstr(")");
							#ifdef GROUPV2
							float w = ImGui::GetContentRegionAvailWidth();
							float fPreviewImgSize = w - 10.0f;
							if (fPreviewImgSize > 200.0f) fPreviewImgSize = 200.0f;
							float ImgY = ImGui::GetFontSize();
							bool isClicked = false;
							ImVec2 vToolsPos = ImGui::GetCursorPos();
							if (iEntityGroupListImage[l] > 0 && ImageExist(iEntityGroupListImage[l]))
							{
								float ImgX = ImageWidth(iEntityGroupListImage[l]);
								ImgY = ImageHeight(iEntityGroupListImage[l]);
								float Ratio = fPreviewImgSize / ImgX;
								ImgY *= Ratio;

								if (current_selected_group == l)
								{
									window = ImGui::GetCurrentWindow();
									ImVec2 padding = { 3.0, 3.0 };
									const ImRect image_bb((window->DC.CursorPos - padding), window->DC.CursorPos + padding + ImVec2(fPreviewImgSize, ImgY));
									window->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
								}
								isClicked = ImGui::ImgBtn(iEntityGroupListImage[l], ImVec2(fPreviewImgSize, ImgY), drawCol_back, drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false, false,false,false,false);
							}
							else
								isClicked = ImGui::Selectable(sGroupString.Get()); //PE: Missing ?

							if (ImGui::IsItemHovered())
							{
								//Highlight group.
								entityselectionlist = vEntityGroupList[l];
								if (iEntityGroupListImage[l] > 0 && ImageExist(iEntityGroupListImage[l]))
								{
									ImGui::BeginTooltip();
									float fRatio = (float)ImageHeight(iEntityGroupListImage[l]) / (float)ImageWidth(iEntityGroupListImage[l]);
									float imagew = 400.0f;
									float imageh = imagew * fRatio;
									ImGui::ImgBtn(iEntityGroupListImage[l], ImVec2(imagew, imageh), drawCol_back, drawCol_normal, drawCol_hover, drawCol_Down, 0, 0, 0, 0, false);
									ImGui::EndTooltip();
								}
							}

							if(isClicked)
							{
								current_selected_group = l;
								g.entityrubberbandlist = vEntityGroupList[l];
								//Set widget on first item in list.
								int e = g.entityrubberbandlist[0].e;
								if (e > 0)
								{
									if (t.entityelement[e].editorlock == 0)
									{
										t.widget.pickedEntityIndex = e;
										t.widget.pickedObject = t.entityelement[e].obj;
									}
								}
								//PE: Make sure next selection , delete rubberband if ctrl not used.
								iLastSelectedEntityGroup = -1;
								iLastSelectedEntity = -1;
							}

							if (bWaitOnMouseRelease)
							{
								if (!ImGui::IsMouseDown(0))
									bWaitOnMouseRelease = false;
							}

							if (pref.iEnableDragDropEntityMode && !bWaitOnMouseRelease && t.gridentity == 0 && t.gridentityobj == 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
							{
								//Just use first entry.
								int e = vEntityGroupList[l][0].e;
								if (iEntityGroupListImage[l] > 0 && ImageExist(iEntityGroupListImage[l]))
									StartDragDropFromEntityID(e, l , iEntityGroupListImage[l]);
								else
									StartDragDropFromEntityID(e,l);
							}

							//Check if any item in group is locked.
							bool isObjectInLocedList = false;
							int iObjectLockedIndix = -1;
							if (vEntityGroupList[l].size() > 0)
							{
								for (int i = 0; i < vEntityGroupList[l].size(); i++)
								{
									int e = vEntityGroupList[l][i].e;
									if (e > 0 && t.entityelement[e].editorlock)
									{
										isObjectInLocedList = true;
										iObjectLockedIndix = i;
										break;
									}
								}
							}

							int iImageSize = 20;
							ImVec2 opos = ImGui::GetCursorPos();
							ImGui::SetCursorPos(ImVec2(vToolsPos.x + fPreviewImgSize - 22.0f, vToolsPos.y - 22.0f + ImgY));
							ImGui::SetItemAllowOverlap();
							ImGui::PushID( 223344 + l); //Need unique ids.
							if (isObjectInLocedList)
							{
								if (ImGui::ImgBtn(TOOL_LOCK, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
								{
									if (vEntityGroupList[l].size() > 0)
									{
										for (int i = 0; i < vEntityGroupList[l].size(); i++)
										{
											int e = vEntityGroupList[l][i].e;
											if (e > 0) {
												t.entityelement[e].editorlock = 0;
												sObject* pObject;
												if (t.entityelement[e].obj > 0) {
													pObject = g_ObjectList[t.entityelement[e].obj];
													if (pObject) {
														WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
													}
												}
												//Delete from list.
												for (int il = 0; il < vEntityLockedList.size(); il++)
												{
													if (vEntityLockedList[il].e == e) 
													{
														vEntityLockedList.erase(vEntityLockedList.begin() + il);
														break;
													}
												}
											}
										}

										// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
										t.widget.pickedObject = 0;
									}
								}
								if (ImGui::IsItemHovered() && bToolTipActive) 
								{
									ImGui::SetTooltip("UnLock Group");
								}
							}
							else
							{
								if (ImGui::ImgBtn(TOOL_UNLOCK, ImVec2(iImageSize, iImageSize), ImColor(0, 0, 0, 0), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), ImColor(255, 255, 255, 180), 0, 0, 0, 0, false,false,false,false,false, bBoostIconColors))
								{
									if (vEntityGroupList[l].size() > 0)
									{
										for (int i = 0; i < vEntityGroupList[l].size(); i++)
										{
											int e = vEntityGroupList[l][i].e;
											if (e > 0)
											{
												t.entityelement[e].editorlock = 1;
												sObject* pObject;
												if (t.entityelement[e].obj > 0)
												{
													pObject = g_ObjectList[t.entityelement[e].obj];
													if (pObject) 
													{
														#ifndef ALLOWSELECTINGLOCKEDOBJECTS
														WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_CURSOROBJECT);
														#endif
													}
												}
												sRubberBandType vEntityLockedItem;
												vEntityLockedItem.e = e;
												vEntityLockedList.push_back(vEntityLockedItem);
											}
										}

										// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
										t.widget.pickedObject = 0;
									}
								}
								if (ImGui::IsItemHovered() && bToolTipActive) {
									ImGui::SetTooltip("Lock Group");
								}
							}
							ImGui::SetCursorPos(opos);
							ImGui::PopID();
							#endif
							ImGui::NextColumn();
						}
					}
					ImGui::PopStyleVar();
					ImGui::Columns(1);
					if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0) 
					{
						// Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
						ImGui::Text("");
						ImGui::Text("");
					}
					ImGui::EndTabItem();
				}
				
				tabflags = 0;
				if (i_switch_group_tab == 3)
				{
					i_switch_group_tab = 0;
					tabflags = ImGuiTabItemFlags_SetSelected;
				}
				ImGui::EndTabBar(); //PE:Fix Assert error , stacksize.
			}

			ImGui::EndChild();

			vBelowContentSize = ImGui::GetCursorPos();

			ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));


			float w = ImGui::GetWindowContentRegionWidth();
			int iTotalIcons = 3;
			if (pref.iObjectEnableAdvanced) iTotalIcons = 4;
			int icon_spacer = 10;
			float control_width = (control_image_size + 4.0) * iTotalIcons + 12.0;
			control_width += (icon_spacer*(iTotalIcons - 1));
			int indent = (w*0.5) - (control_width*0.5);
			if (indent < 2) indent = 2;
			ImGui::Indent(indent);

			ImVec2 restore_cursorpos = ImGui::GetCursorPos();

			window = ImGui::GetCurrentWindow();

			// Can create smart object from a group, so ask here
			static bool bGetNewSmartObjectName = false;
			static char pSmartObjectName[256];
			if (bGetNewSmartObjectName == true)
			{
				ImGui::SetNextWindowSize(ImVec2(26 * ImGui::GetFontSize(), 11 * ImGui::GetFontSize()), ImGuiCond_Once);
				ImGui::SetNextWindowPosCenter(ImGuiCond_Once);
				cstr sUniqueWinName = cstr("Enter A Name for your Smart Object##Smart Object Name Window");
				bool bSmartObjectNameWindow = true; //PE: The window should always be open here.
				ImGui::Begin(sUniqueWinName.Get(), &bSmartObjectNameWindow, 0);
				ImGui::Indent(10);
				cstr sUniqueInputName = cstr("##Smart Object Name") + cstr(1);
				ImGui::PushItemWidth(-10);
				ImGui::Text("");
				ImGui::Text("Type a name for your Smart Object and press ENTER:");
				if (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) ImGui::SetKeyboardFocusHere(0);
				if (ImGui::InputText(sUniqueInputName.Get(), pSmartObjectName, 250, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					// save the group
					char pObjectSavedFilename[MAX_PATH];
					strcpy(pObjectSavedFilename, pSmartObjectName);
					if (SaveGroup(current_selected_group, pObjectSavedFilename) == true)
					{
						// success, go to preview to set thumbnail
						g_LastGroupSaved_s = pObjectSavedFilename;

						//PE: If we already have the original image use that no need for large preview. just goto the folder where it was created.
						CreateBackBufferCacheNameEx(g_LastGroupSaved_s.Get(), 512, 288, true);
						if (!FileExist(BackBufferCacheName.Get()))
						{
							extern cstr sGotoPreviewWithFile;
							extern int iGotoPreviewType;
							sGotoPreviewWithFile = pObjectSavedFilename;
							iGotoPreviewType = 2;
						}
						else
						{
							sStartLibrarySearchString = "user";
							iLastDisplayLibraryType = -1;
							bExternal_Entities_Window = true;
							iDisplayLibraryType = 0;
							iDisplayLibrarySubType = 0;
						}
					}

					// finished here
					bGetNewSmartObjectName = false;
				}
				if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;

				ImGui::PopItemWidth();
				ImGui::Text("");
				int iCancelSize = 100;
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((ImGui::GetWindowContentRegionWidth()*0.5) - (iCancelSize*0.5), 0.0f));
				if ( !bSmartObjectNameWindow || ImGui::StyleButton("Cancel", ImVec2(iCancelSize, 0)))
				{
					//Window closed or cancel selected, just exit.
					bGetNewSmartObjectName = false;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel");
				ImGui::Indent(-10);
				bImGuiGotFocus = true;
				ImGui::End();
			}
			for (int b = 0; b < iTotalIcons; b++)
			{
				//TOOL_UNLOCK
				ImVec4 IconActive = ImVec4(1.0, 1.0, 1.0, 1.0);
				ImVec4 IconInActive = ImVec4(0.7, 0.7, 0.7, 0.7);
				ImVec4 IconColor;
				bool bIconActive = false;
				int iIconID = 0;
				LPSTR pLabelToolTip = "";
				if (b == 0 ) { iIconID = TOOL_GROUP; pLabelToolTip = "Group Objects"; }
				if (b == 1 ) { iIconID = TOOL_UNGROUP; pLabelToolTip = "Ungroup Objects"; }
				if (b == 2) { iIconID = TOOL_GROUPEDIT; pLabelToolTip = "Edit Group"; }
				if (b == 3) { iIconID = TOOL_GROUPSAVE; pLabelToolTip = "Save Group"; }
				//PE: && bSelectionAvail Always display looks better.
				bool bValidSelectionForGroup = false;
				// if selection a smart object
				bool bIsASmartObject = false;
				if (current_selected_group != -1)
				{
					if (vEntityGroupList[current_selected_group].size() > 0)
					{
						int iParentGroupID = vEntityGroupList[current_selected_group][0].iParentGroupID;
						for (int i = 0; i < MAXGROUPSLISTS; i++)
						{
							if (vEntityGroupList[i].size() > 0)
							{
								if (vEntityGroupList[i][0].iGroupID == iParentGroupID)
								{
									if (sEntityGroupListName[i].Len() > 0)
									{
										// parent is a smart object
										bIsASmartObject = true;
									}
								}
							}
						}
					}
				}
				if (bIsASmartObject == true )
				{
					// yes, so change labels to reflect Smart Object editing
					if (iInsideTab == 1)
					{
						// but only when in current object view
						if (b == 0) { pLabelToolTip = "This selection is a Smart Object"; }
						if (b == 1) { pLabelToolTip = "Ungroup Smart Object"; }
						if (b == 2) { pLabelToolTip = "Edit Smart Object"; }
						if (b == 3) { pLabelToolTip = "Save As Smart Object"; }
						if (iIconID == TOOL_UNGROUP || iIconID == TOOL_GROUPEDIT || iIconID == TOOL_GROUPSAVE)
						{
							IconColor = IconActive;
							bIconActive = true;
						}
						else
						{
							IconColor = IconInActive;
						}
					}
					else
					{
						// smart objects have no controls in the group tab
						IconColor = IconInActive;
					}
				}
				else
				{
					if (bSelectionAvail)
					{
						// regular group must not be a smart object - need at least two objects to make a NEW group
						if (g.entityrubberbandlist.size() > 1)
						{
							bValidSelectionForGroup = true;
						}
					}
					if (bValidSelectionForGroup == true && !(iIconID == TOOL_UNGROUP || iIconID == TOOL_GROUPEDIT || iIconID == TOOL_GROUPSAVE))
					{
						IconColor = IconActive;
						bIconActive = true;
					}
					else
					{
						IconColor = IconInActive;
					}
					if ((iIconID == TOOL_UNGROUP || iIconID == TOOL_GROUPEDIT || iIconID == TOOL_GROUPSAVE) && current_selected_group >= 0 && iInsideTab == 2)
					{
						IconColor = IconActive;
						bIconActive = true;
					}
				}

				if (iIconID > 0)
				{
					//Highlight icon.
					if (iIconID == TOOL_GROUPEDIT && bIconActive)
					{
						if (current_selected_group >= 0 && group_editing_on)
						{
							ImVec2 padding = { 3.0, 3.0 };
							const ImRect image_bb((window->DC.CursorPos - padding), window->DC.CursorPos + padding + ImVec2(control_image_size, control_image_size));
							window->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
						}
					}

					if (ImGui::ImgBtn(iIconID, ImVec2(control_image_size, control_image_size), ImVec4(0.0, 0.0, 0.0, 0.0), IconColor, ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false,false, bBoostIconColors))
					{
						if ( 1 )
						{
							// clicked mode button
							if (iIconID == TOOL_GROUP)
							{
								CreateNewGroup(-1);
							}
							if (iIconID == TOOL_UNGROUP && bIconActive)
							{
								// show all game elements when editing a group
								gridedit_setsmartobjectvisibilityinrubberband(true);

								// then ungroup them so visible and ready to manage
								UnGroupSelected();
							}
							if (iIconID == TOOL_GROUPEDIT && bIconActive)
							{
								if (group_editing_on && current_selected_group >= 0)
								{
									//Turning off , refresh group thumb.
									g.entityrubberbandlist = vEntityGroupList[current_selected_group];
									GetRubberbandLowHighValues();
									//PE: Generate new thumbnail of group.
									BackBufferIsGroup = false;
									BackBufferEntityID = 0;
									BackBufferObjectID = 0;
									BackBufferImageID = iEntityGroupListImage[current_selected_group];
									BackBufferSizeX = 512 * 2.0f;
									BackBufferSizeY = 288 * 2.0f;
									BackBufferZoom = 1.0f;
									BackBufferCamLeft = 0.0f;
									BackBufferCamUp = 0.0f;
									bRotateBackBuffer = false;
									bBackBufferAnimated = false;
									bLoopBackBuffer = false;
									RevertBackbufferCubemap();
									BackBufferSnapShotMode = true;

									//PE: Fullscreen
									if (BitmapExist(99))
									{
										DeleteBitmapEx(99);
									}
									bFullScreenBackbuffer = true;
									bStopBackbufferGrab = 1;

									//if BackBufferSnapShotMode
									if (t.widget.pickedEntityIndex > 0 && t.widget.activeObject > 0)
									{
										widget_hide();
									}

									// smart object game elements hide their game elements when not editing them
									gridedit_setsmartobjectvisibilityinrubberband(false);
								}
								else
								{
									// show all game elements when editing a group
									gridedit_setsmartobjectvisibilityinrubberband(true);
								}
								group_editing_on = 1 - group_editing_on; //toggle

							}
							if (iIconID == TOOL_GROUPSAVE)
							{
								// save group as object
								if (current_selected_group >= 0)
								{
									bGetNewSmartObjectName = true;
									strcpy(pSmartObjectName, "");
								}
							}
						}
					}
					if (ImGui::IsItemHovered() && bToolTipActive) ImGui::SetTooltip(pLabelToolTip);

					if (iIconID == TOOL_GROUP)
					{
						if (ImGui::BeginPopupContextWindow())
						{
							ListGroupContextMenu();
							ImGui::EndPopup();
						}
					}


					restore_cursorpos = ImGui::GetCursorPos();
					if (b < iTotalIcons)
					{
						ImGui::SameLine();
						ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(icon_spacer, 0));
					}
					//PE: Support wrapping of icons.
					float caw = (ImGui::GetContentRegionAvailWidth() + (control_image_size*0.5));
					if (caw < control_image_size) {
						ImGui::SetCursorPos(restore_cursorpos);
					}
				}
			}
			ImGui::Indent(-indent);
			ImGui::SetCursorPos(restore_cursorpos); //Restore cursor.

			vBelowContentSize = ImGui::GetCursorPos() - vBelowContentSize;

			//Support DragDrop to remove objects.
			bb = { ImGui::GetWindowContentRegionMin() + ImGui::GetWindowPos(),ImGui::GetWindowContentRegionMax() + ImGui::GetWindowPos() };

			if (bTrashcanIconActive2)
				bTrashcanIconActive2 = false;
			bool bTmp = bTrashcanIconActive;
			DragDrop_CheckTrashcanDrop(bb);
			bTrashcanIconActive2 = bTrashcanIconActive;
			bTrashcanIconActive = bTmp;

			ImGui::End();

		}

		wi::profiler::EndRange(clSeg); clSeg = wi::profiler::BeginRangeCPU("CL-Tail");
		//################################
		//#### EBE BUILDER LEFT PANEL ####
		//################################


		//##############################
		//#### Bug Reporting system ####
		//##############################

		ProcessBugReporting();


		//####################
		//#### Storyboard ####
		//####################

		//PE: Delayed startup moved to process_storeboard so we can hide 3D editor.
		static bool bInitStoryboardStartup = true;
		if (bInitStoryboardStartup)
		{
			extern bool bSpecialEditorFromStandalone;
			extern bool bReturnToWelcome;
			if (bSpecialEditorFromStandalone && !bReturnToWelcome)
			{
				bStoryboardWindow = true;
				GGTerrain_CancelRamp();
			}
			else if (!pref.iDisplayWelcomeScreen)
			{
				// LB: If we are not displaying the welcome screen, we should display the storyboard window.
				// as the iLastInStoryboard flag to take the user direct to the last level edited does not work
				// but taking the user to the last used storyboard allows the user to select the level they 
				// wish to work on and avoids ever being in a level that has no associated FPM level.
				//if (pref.iLastInStoryboard)
				bool bAlwaysTakeUserWhoSkipsHubToLastStoryboard = true;
				if(bAlwaysTakeUserWhoSkipsHubToLastStoryboard==true)
				{
					bStoryboardWindow = true;
					GGTerrain_CancelRamp();
				}
			}
			bInitStoryboardStartup = false;
		}

		process_storeboard();

		//###################
		//#### RPG GAMES ####
		//###################
		#ifdef RPG_GAMES
		ProcessRPGSetupWindow();
		#endif

		//###########################
		//#### VISULS LEFT PANEL ####
		//###########################

		if (refresh_gui_docking == 0 && !Visuals_Tools_Window)
		{
			//Make sure window is setup in docking space.
			ImGui::Begin("Environment Effects##VisualsToolsWindow", &Visuals_Tools_Window, iGenralWindowsFlags);
			ImGui::End();
		}
		else 
		{
			//PE: Make sure we switch to the correct name , this can change in test game.
			extern cStr sWindowName;
			sWindowName = "Environment Effects##VisualsToolsWindow";
			tab_tab_visuals(1, 0);
		}

		bImGuiReadyToRender = true;

		if (refresh_gui_docking < 4) 
		{
			refresh_gui_docking++;
		}
		else 
		{
			if (!bImGuiInitDone)
			{
				WickedCall_EnableCameraLight(bEditorLight);
			}
			bImGuiInitDone = true;
			static bool bLeftPanelSelectedAsDefault = false;
			if (!bLeftPanelSelectedAsDefault) {
				ImGui::SetWindowFocus("Terrain Tools##Paint Terrain##TerrainToolsWindow");
				bLeftPanelSelectedAsDefault = true;
			}

		}

		//Some need launch after we have bImGuiReadyToRender , so prompt will work.
		if (iSkibFramesBeforeLaunch == 0) 
		{
			switch (iLaunchAfterSync)
			{
				case 503: //Save the actual map here!
				{
					::SetCursor(::LoadCursor(NULL, IDC_WAIT));
					ImGuiIO& io = ImGui::GetIO();
					io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
					bKeepWindowsResponding = true;

					iLaunchAfterSync = 0;
					if (iLaunchAfterSyncAction == 11)
					{
						strcpy(cTriggerMessage, "A start marker has been added to your new level. This is where you will start in this level when you press Test Level.");
						bTriggerMessage = true;
						iTriggerMessageDelay = 30;
						iTriggerMessageY = 1;
						iMessageTimer = 0;
						iLaunchAfterSyncAction = 0;

						//MD: Ensures the saved camera position is close to the player start marker. Otherwise, terrain generator camera position would be saved.
						t.editorfreeflight.c.x_f = t.gridentityposx_f + 10.0f;
						t.editorfreeflight.c.y_f = t.gridentityposy_f + 200.0f;
						t.editorfreeflight.c.z_f = t.gridentityposz_f + 10.0f;
						t.editorfreeflight.c.angx_f = 70.0f;
						t.editorfreeflight.c.angy_f = -70.0f;
					}
					gridedit_save_map();
					g.projectmodified = 0; gridedit_changemodifiedflag();
					g.projectmodifiedstatic = 0;

					bKeepWindowsResponding = false;
					io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
					::SetCursor(::LoadCursor(NULL, IDC_ARROW));

					break;
				}
				case 504: //Save the actual map here!
				{

					::SetCursor(::LoadCursor(NULL, IDC_WAIT));
					ImGuiIO& io = ImGui::GetIO();
					io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
					bKeepWindowsResponding = true;

					iLaunchAfterSync = 0;
					gridedit_save_map();
					g.projectmodified = 0; gridedit_changemodifiedflag();
					g.projectmodifiedstatic = 0;
					if (iLaunchAfterSyncAction == 1)
					{
						bStoryboardWindow = true;
						GGTerrain_CancelRamp();
					}
					if (iLaunchAfterSyncAction == 2)
					{
						iLaunchAfterSync = 2;
						iSkibFramesBeforeLaunch = 5;
					}

					if (iLaunchAfterSyncAction == 3)
					{
						//PE: Default to terrain tools , like when we launch Max.
						bForceKey = true;
						csForceKey = "t";
						bForceKey2 = true;
						csForceKey2 = "6";
						t.inputsys.domodeterrain = 1; t.inputsys.dowaypointview = 0;
						t.gridentitymarkersmodeonly = 0; t.grideditselect = 0;
						t.terrain.terrainpaintermode = 6;
						bTerrain_Tools_Window = true;
						// must reset any manual editing
						GGTerrain_ResetSculpting();
						void reset_terrain_paint_date(void);
						reset_terrain_paint_date();
						iLaunchAfterSync = 5;
						iSkibFramesBeforeLaunch = 5;
					}

					iLaunchAfterSyncAction = 0;

					bKeepWindowsResponding = false;
					io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
					::SetCursor(::LoadCursor(NULL, IDC_ARROW));

					break;
				}

				case 3: //Save

					GGTerrain_CancelRamp();

					iLaunchAfterSync = 0;
					if (g.projectmodified == 1)
					{
						// Make sure we have last ebe changes.
						if (t.ebe.on == 1)
						{
							ebe_hide();
						}

						// yes save first
						if (g.projectfilename_s == "")
						{
							t.returnstring_s = "";
							cStr tOldDir = GetDir();
							char * cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "fpm\0*.fpm\0", g.mysystem.mapbankAbs_s.Get(), NULL,true);
							SetDir(tOldDir.Get());
							if (cFileSelected && strlen(cFileSelected) > 0) 
							{
								t.returnstring_s = cFileSelected;
							}
							if (t.returnstring_s != "")
							{
								if (cstr(Lower(Right(t.returnstring_s.Get(), 4))) != ".fpm")  t.returnstring_s = t.returnstring_s + ".fpm";
								g.projectfilename_s = t.returnstring_s;
								bool oksave = true;
								if (FileExist(g.projectfilename_s.Get())) 
								{
									oksave = overWriteFileBox(g.projectfilename_s.Get());
								}
								if (oksave) 
								{
									//Do the actualy save here.
									iLaunchAfterSync = 503; 
									iSkibFramesBeforeLaunch = 3;

									//Add newly saved fpm level to recent list.
									int firstempty = -1;
									int i = 0;
									for (; i < REMEMBERLASTFILES; i++) 
									{
										if (firstempty == -1 && strlen(pref.last_open_files[i]) <= 0)
											firstempty = i;

										if (strlen(pref.last_open_files[i]) > 0 && pestrcasestr(g.projectfilename_s.Get(), pref.last_open_files[i])) 
										{ 
											//already there
											break;
										}
									}
									if (i >= REMEMBERLASTFILES) 
									{
										if (firstempty == -1) 
										{
											//No empty slots , rotate.
											for (int ii = 0; ii < REMEMBERLASTFILES - 1; ii++) 
											{
												strcpy(pref.last_open_files[ii], pref.last_open_files[ii + 1]);
											}
											strcpy(pref.last_open_files[REMEMBERLASTFILES - 1], g.projectfilename_s.Get());
										}
										else
											strcpy(pref.last_open_files[firstempty], g.projectfilename_s.Get());
									}
									strcpy(cTriggerMessage, "Saving Level ...");
									bTriggerMessage = true;
								}
							}
						}
						else
						{
							//Do the actualy save here.
							iLaunchAfterSync = 503; 
							iSkibFramesBeforeLaunch = 3;
							strcpy(cTriggerMessage, "Saving Level ...");
							bTriggerMessage = true;
						}
					}
					break;

				case 4: //Save As
				{
					GGTerrain_CancelRamp();

					cstr oldprojectfilename_s = g.projectfilename_s;
					iLaunchAfterSync = 0;
					if (t.ebe.on == 1)
						ebe_hide(); //Make sure we have last ebe changes.

					t.returnstring_s = "";
					cStr tOldDir = GetDir();

					// we know we need to focus on the mapbank associated with the current storyboard
					cstr correctFPMLocation_s = Storyboard.customprojectfolder;
					if (correctFPMLocation_s.Len() > 0)
					{
						correctFPMLocation_s += Storyboard.gamename;
						correctFPMLocation_s += "\\Files\\mapbank";
					}
					else
					{
						correctFPMLocation_s = g.mysystem.mapbankAbs_s.Get();
					}

					char* cFileSelected = (char*)noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "fpm\0*.fpm\0", correctFPMLocation_s.Get(), NULL, true);

					SetDir(tOldDir.Get());
					if (cFileSelected && strlen(cFileSelected) > 0) 
					{
						t.returnstring_s = cFileSelected;
					}
					if (t.returnstring_s != "")
					{
						if (cstr(Lower(Right(t.returnstring_s.Get(), 4))) != ".fpm")  t.returnstring_s = t.returnstring_s + ".fpm";
						g.projectfilename_s = t.returnstring_s;

						bool oksave = true;
						if (FileExist(g.projectfilename_s.Get())) 
						{
							oksave = overWriteFileBox(g.projectfilename_s.Get());
						}
						if (oksave) 
						{
							//Add newly saved fpm level to recent list.
							int firstempty = -1;
							int i = 0;
							for (; i < REMEMBERLASTFILES; i++) 
							{
								if (firstempty == -1 && strlen(pref.last_open_files[i]) <= 0)
									firstempty = i;
								if (strlen(pref.last_open_files[i]) > 0 && pestrcasestr(g.projectfilename_s.Get(), pref.last_open_files[i])) 
								{
									//already there
									break;
								}
							}
							if (i >= REMEMBERLASTFILES) 
							{
								if (firstempty == -1) 
								{
									//No empty slots , rotate.
									for (int ii = 0; ii < REMEMBERLASTFILES - 1; ii++) 
									{
										strcpy(pref.last_open_files[ii], pref.last_open_files[ii + 1]);
									}
									strcpy(pref.last_open_files[REMEMBERLASTFILES - 1], g.projectfilename_s.Get());
								}
								else
									strcpy(pref.last_open_files[firstempty], g.projectfilename_s.Get());
							}

							iLaunchAfterSync = 503; //Do the actualy save here.
							iSkibFramesBeforeLaunch = 3;

							//PE: Change storyboard node to new save as project name.
							if (iLevelEditorFromStoryboardID >= 0)
							{
								//Validate we have the correct one.
								if (iLevelEditorFromStoryboardID < STORYBOARD_MAXNODES)
								{
									if (stricmp(Storyboard.Nodes[iLevelEditorFromStoryboardID].level_name, oldprojectfilename_s.Get()) == NULL)
									{
										//Valid , Update node.

										//Only relative.
										char tmp[MAX_PATH];
										strcpy(tmp, t.returnstring_s.Get());
										char *find = (char *)pestrcasestr(tmp, "mapbank\\");
										if (find && find != &tmp[0]) strcpy(&tmp[0], find);

										std::string sLevelPath = &tmp[0];

										int iPos;
										for (iPos = strlen(tmp); iPos >= 0; iPos--)
											if (tmp[iPos] == '\\') break;
										if (iPos > 0) iPos++;
										std::string sLevelTitle = &tmp[iPos];
										replaceAll(sLevelTitle, ".fpm", "");

										strcpy(Storyboard.Nodes[iLevelEditorFromStoryboardID].level_name, sLevelPath.c_str());
										strcpy(Storyboard.Nodes[iLevelEditorFromStoryboardID].title, sLevelTitle.c_str());
									}
								}
							}
							strcpy(cTriggerMessage, "Saving Level ...");
							bTriggerMessage = true;
						}
					}
					break;
				}

				case 5: // New flatten level
				{
					GGTerrain_CancelRamp();

					iLaunchAfterSync = 0;
					int iRet = 0;
					if (!bNoSecondAsk)
					{
						iRet = AskSaveBeforeNewAction();
					}
					else
					{
						bNoSecondAsk = false;
						iRet = 0;
					}
					if (iRet != 2)
					{
						// need to trigger the new level code
						t.inputsys.donewflat = 1;
						t.inputsys.donew == 1;//?
						gridedit_new_map();
						t.inputsys.donewflat = 0;
						t.inputsys.donew = 0;
						iLaunchAfterSync = 80;
						iSkibFramesBeforeLaunch = 5;
						// go to procedural terrain generator
						bProceduralLevel = true;
						bProceduralLevelStartup = true; // will trigger a random theme to be selected (overriding EMPTY init above)
						GGTerrain_RemoveHeightMap();
						iLevelEditorFromStoryboardID = -1; //We cant update storyboard.

					}

					break;
				}

				case 6: // New level
				{
					GGTerrain_CancelRamp();

					iLaunchAfterSync = 0;
					int iRet = AskSaveBeforeNewAction();
					if (iRet != 2)
					{
						t.inputsys.donewflat = 0;
						t.inputsys.donew == 1;
						gridedit_new_map();
						t.inputsys.donewflat = 0;
						t.inputsys.donew = 0;

						iLaunchAfterSync = 80; //Update env
						iSkibFramesBeforeLaunch = 5;

						strcpy(cTriggerMessage, "New level created");
						bTriggerMessage = true;
					}
					iLastUpdateVeg = 0;
					bUpdateVeg = true;
					break;
				}


				case 8: //Import model.
				{
					iLaunchAfterSync = 0;

					// if free trial, no import
					if (g_bFreeTrialVersion == true)
					{
						bFreeTrial_Window = true;
						break;
					}

					GGTerrain_CancelRamp();

					if (sDefaultImportPath == "")
						sDefaultImportPath = g.fpscrootdir_s;
					cStr tOldDir = GetDir();
					char * cFileSelected;

					// if batch converting, keep going around until no files left in batch list
					extern bool bBatchConverting;
					if (bBatchConverting == true)
					{
						extern std::vector<cstr> batchFileList;
						int iBatchFileCount = batchFileList.size();
						if (iBatchFileCount > 0)
						{
							extern char cImportPath[MAX_PATH];
							strcpy(pLaunchAfterSyncPreSelectModel, cImportPath);
							strcat(pLaunchAfterSyncPreSelectModel, "\\");
							strcat(pLaunchAfterSyncPreSelectModel, batchFileList[iBatchFileCount-1].Get());
							batchFileList.pop_back();
						}
					}

					if (strlen(pLaunchAfterSyncPreSelectModel) > 0)
					{
						// can trigger the importer with a preselected model filename (used by scaling mode changes)
						cFileSelected = pLaunchAfterSyncPreSelectModel;
					}
					else
					{
						// otherwise by default it requests a model file
						cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "All\0*.*\0X\0*.x\0DBO\0*.dbo\0OBJ\0*.obj\0FBX\0*.fbx\0GLTF\0*.gltf\0GLB\0*.glb\0\0\0", sDefaultImportPath.Get(), NULL, true, "Import Model");

						// store last manually selected model file (for reload for scaling mode)
						if ( cFileSelected ) 
							strcpy (pLaunchAfterSyncLastImportedModel, cFileSelected);
						else
							strcpy (pLaunchAfterSyncLastImportedModel, "");
					}
					SetDir(tOldDir.Get());
					if (cFileSelected && strlen(cFileSelected) > 0) 
					{
						char szModelPath[ MAX_PATH ];
						strcpy( szModelPath, cFileSelected );
						const char* pExtension = strrchr( szModelPath, '.' );
						if ( !pExtension )
						{
							strcpy(cTriggerMessage, "File extension not found");
							bTriggerMessage = true;
						}
						else
						{
							bool bFoundModel = true;
							// extract zip file and set szModelPath to first model found
							if ( _stricmp(pExtension, ".zip") == 0 )
							{
								char szSubZipPath[ MAX_PATH ];
								bool bFoundSubZip = false;

								bFoundModel = false;
								char rootFolder[ MAX_PATH ];
								strcpy( rootFolder, GG_GetWritePath() );
								strcat( rootFolder, "imported_models\\" );

								// add zip file name as a folder to extract to
								char* pSlash = strrchr( szModelPath, '/' );
								char* pBSlash = strrchr( szModelPath, '\\' );
								if ( pBSlash > pSlash ) pSlash = pBSlash;
								if ( pSlash ) strcat( rootFolder, pSlash+1 );
								
								// remove extension
								pSlash = strrchr( rootFolder, '.' );
								if ( pSlash ) *pSlash = 0;
								strcat( rootFolder, "\\" );

								char finalPath[ MAX_PATH ];
								
								// extract zip file
								mz_zip_archive zip_archive;
								memset(&zip_archive, 0, sizeof(zip_archive));
								if ( !mz_zip_reader_init_file( &zip_archive, szModelPath, 0 ) )
								{
									mz_zip_reader_end( &zip_archive );
									strcpy(cTriggerMessage, "Failed to open zip file for reading");
									bTriggerMessage = true;
								}
								int numFiles = mz_zip_reader_get_num_files( &zip_archive );
								char filename[ 1024 ];
								for( int i = 0; i < numFiles; i++ )
								{
									mz_zip_reader_get_filename( &zip_archive, i, filename, 1024 );

									if ( mz_zip_reader_is_file_a_directory( &zip_archive, i ) ) 
									{
										// skip empty folders
										continue;
									}
            
									if ( strlen(rootFolder) + strlen(filename) + 3 > MAX_PATH ) continue;

									strcpy( finalPath, rootFolder );
									strcat( finalPath, filename );
                        
									// look for model file
									if ( !bFoundModel )
									{
										pExtension = strrchr( filename, '.' );
										if ( pExtension )
										{
											if ( !stricmp(pExtension, ".x") 
											  || !stricmp(pExtension, ".dbo")
											  || !stricmp(pExtension, ".obj")
											  || !stricmp(pExtension, ".fbx")
											  || !stricmp(pExtension, ".gltf")
											  || !stricmp(pExtension, ".glb") )
											{
												bFoundModel = true;
												strcpy( szModelPath, finalPath );
											}
											if ( !stricmp(pExtension, ".zip") )
											{
												bFoundSubZip = true;
												strcpy( szSubZipPath, finalPath );
											}
										}
									}
            
									GG_CreatePath( finalPath );
            
									mz_zip_reader_extract_to_file( &zip_archive, i, finalPath, 0 );
								}
        
								mz_zip_reader_end( &zip_archive );

								if ( !bFoundModel && bFoundSubZip )
								{
									char* pSlash = strrchr( szSubZipPath, '/' );
									char* pBSlash = strrchr( szSubZipPath, '\\' );
									if ( pBSlash > pSlash ) pSlash = pBSlash;
									if ( pSlash ) strcat( rootFolder, pSlash+1 );
								
									// remove extension
									pSlash = strrchr( rootFolder, '.' );
									if ( pSlash ) *pSlash = 0;
									strcat( rootFolder, "\\" );

									char finalPath[ MAX_PATH ];
								
									// extract zip file
									mz_zip_archive zip_archive;
									memset(&zip_archive, 0, sizeof(zip_archive));
									if ( !mz_zip_reader_init_file( &zip_archive, szSubZipPath, 0 ) )
									{
										mz_zip_reader_end( &zip_archive );
										strcpy(cTriggerMessage, "Failed to open zip file for reading");
										bTriggerMessage = true;
									}
									int numFiles = mz_zip_reader_get_num_files( &zip_archive );
									char filename[ 1024 ];
									for( int i = 0; i < numFiles; i++ )
									{
										mz_zip_reader_get_filename( &zip_archive, i, filename, 1024 );

										if ( mz_zip_reader_is_file_a_directory( &zip_archive, i ) ) 
										{
											// skip empty folders
											continue;
										}
            
										if ( strlen(rootFolder) + strlen(filename) + 3 > MAX_PATH ) continue;

										strcpy( finalPath, rootFolder );
										strcat( finalPath, filename );
                        
										// look for model file
										if ( !bFoundModel )
										{
											pExtension = strrchr( filename, '.' );
											if ( pExtension )
											{
												if ( !stricmp(pExtension, ".x") 
												  || !stricmp(pExtension, ".dbo")
												  || !stricmp(pExtension, ".obj")
												  || !stricmp(pExtension, ".fbx")
												  || !stricmp(pExtension, ".gltf")
												  || !stricmp(pExtension, ".glb") )
												{
													bFoundModel = true;
													strcpy( szModelPath, finalPath );
												}
											}
										}
            
										GG_CreatePath( finalPath );
            
										mz_zip_reader_extract_to_file( &zip_archive, i, finalPath, 0 );
									}
        
									mz_zip_reader_end( &zip_archive );
								}
							}

							if ( !bFoundModel )
							{
								strcpy(cTriggerMessage, "Zip file does not contain a recognised model format");
								bTriggerMessage = true;
							}
							else
							{
								t.returnstring_s = szModelPath;
								const char* pExtension = strrchr( szModelPath, '.' );
								bool bPermittedFormat = false;
								if (stricmp(pExtension, ".x") == NULL) bPermittedFormat = true;
								if (stricmp(pExtension, ".dbo") == NULL) bPermittedFormat = true;
								if (stricmp(pExtension, ".obj") == NULL) bPermittedFormat = true;
								if (stricmp(pExtension, ".fbx") == NULL) bPermittedFormat = true;
								if (stricmp(pExtension, ".gltf") == NULL) bPermittedFormat = true;
								if (stricmp(pExtension, ".glb") == NULL) bPermittedFormat = true;
								if ( bPermittedFormat == true )
								{
									// load the model
									sDefaultImportPath = t.returnstring_s; //Remember last import path.
									t.timporterfile_s = t.returnstring_s;
									importer_loadmodel();
								}
								else 
								{
									strcpy(cTriggerMessage, "This is not a supported model file.");
									bTriggerMessage = true;
								}
							}
						}
					}
					if (bDelayedTutorialCheckAction == TOOL_IMPORT) 
					{
						bDelayedTutorialCheckAction = -1;
						TutorialNextAction();
					}
					// clear 'pLaunchAfterSyncPreSelectModel' as this is a one time use until set again
					if (strlen(pLaunchAfterSyncPreSelectModel) > 0)
					{
						strcpy (pLaunchAfterSyncPreSelectModel, "");
					}
					break;
				}

				case 80: //Update envmap
				{
					iLaunchAfterSync = 0;
					//Make sure we have envmap.
					visuals_justshaderupdate();
					t.visuals.refreshskysettingsfromlua = true;
					cubemap_generateglobalenvmap();
					t.visuals.refreshskysettingsfromlua = false;
					//extern bool bFullVegUpdate;
					//bFullVegUpdate = true;
					bUpdateVeg = true;

					//PE: We need to recreate probes after all objects is placed, so we can find the probe boundingbox.
					GGGrass::GGGrass_DeferInstanceUpdates(1); // the grass is placed again once, not for each flat area
					for (int te = 1; te <= g.entityelementlist; te++)
					{
						int entid = t.entityelement[te].bankindex;
						if (entid > 0)
						{
							entity_autoFlattenWhenAdded(te);
						}
					}
					GGGrass::GGGrass_DeferInstanceUpdates(0);
					if (bLaunchTestGameAfterLoad)
					{
						bLaunchTestGameAfterLoad = false;
						iLaunchAfterSync = 1;
					}
					if (bLaunchSaveStandalonefterLoad)
					{
						bExport_Standalone_Window = true;
						bLaunchSaveStandalonefterLoad = false;
					}
					if (bCloseStoryboardAfterLoad)
					{
						bStoryboardWindow = false;
						bCloseStoryboardAfterLoad = false;
					}

					break;
				}

				case 81: //Delayed window focus.
				{
					iLaunchAfterSync = 0;
					//Make sure we have envmap.
					ImGui::SetWindowFocus(cNextWindowFocus);
					strcpy(cNextWindowFocus, "");
					break;
				}

				case 82: //Delayed window focus.
				{
					iLaunchAfterSync = 0;
					g_bCharacterCreatorPlusActivated = true;
					break;
				}

				default:
					break;
			}
		}
		else
		{
			iSkibFramesBeforeLaunch--;
		}

		//###########################
		//#### Trigger A Message ####
		//###########################
		bool bForceMessageNoFade = false;
		gridedit_triggermessagehandler(bForceMessageNoFade);

		//PE: Hide transition from storyboard to terrain generator.
		if (iBlackoutForFrames > 0)
		{
			if (iBlackoutForFrames != 5)
			{
				//PE: newlevel: bBlockImGuiUntilNewFrame = true; Triggered , what to do ?
				if (bBlockImGuiUntilNewFrame)
					g_bNoSwapchainPresent = true;

				//PE: Hide everything in the background (wicked 3D stuff).
				ImGuiViewport* mainviewport = ImGui::GetMainViewport();
				if (0) //PE: Switched to normal message.
				{
					if (mainviewport)
					{
						ImDrawList* drawlist = ImGui::GetForegroundDrawList(mainviewport);
						if (drawlist)
						{
							ImVec4 monitor_col = ImVec4(0, 0, 0, 1.0); //Black for now.
							drawlist->AddRectFilled(ImVec2(-1, -1), ImGui::GetMainViewport()->Size + ImVec2(40, 40), ImGui::GetColorU32(monitor_col));

							ImGuiContext& g = *GImGui;
							ImGui::SetWindowFontScale(2.0);
							ImVec2 tsize = ImGui::CalcTextSize("Preparing the Terrain Generator.Please wait...");
							ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
							ImVec2 tpos = (viewPortSize * 0.5) - (tsize * 0.5);
							drawlist->AddText(g.Font, g.FontSize, tpos, ImGui::GetColorU32(ImGuiCol_Text), "Preparing the Terrain Generator.Please wait...");
							ImGui::SetWindowFontScale(1.0);

						}
					}
				}
			}
			iBlackoutForFrames--;
			if(iBlackoutForFrames == 0)
				g_bNoSwapchainPresent = false;

		}
		wi::profiler::EndRange(clSeg); // close CL-Tail
	}

