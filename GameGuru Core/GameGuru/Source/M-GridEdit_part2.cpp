	// 191015 - Trigger quick start dialog when editor flowing
	if ( iCountDownToShowQuickStartDialog > 0 )
	{
		iCountDownToShowQuickStartDialog--;
		if ( iCountDownToShowQuickStartDialog == 0 )
		{
			// insert parental control prompt if not used before
			if ( g.quickparentalcontrolmode == 0 )
			{
				// 050416 - ensure once time entrance to parental control from boot-up
				g.quickparentalcontrolmode = 1;
				editor_showparentalcontrolpage ();
			}
		}
	}

	//  Do not get start memory until processed one loop (below)
	if (  t.gamememactuallyusedstarttriggercount>0 ) 
	{
		--t.gamememactuallyusedstarttriggercount;
		if (  t.gamememactuallyusedstarttriggercount == 0 ) 
		{
			g.gamememactuallyusedstart=SMEMAvailable(1);
		}
	}

	//  Machine Independent Speed
	game_timeelapsed ( );
	t.ts_f=(MAXTimer()-t.tsl_f)/50.0 ; t.tsl_f= MAXTimer();

	//  Send SteamID to the editor if needed
	mp_sendSteamIDToEditor ( );

	//  User input calls
	static bool imgui_onetime_init = false;
	if (g.globals.ideinputmode == 1) 
	{
		imgui_input_getcontrols();
	}
	else 
	{
		input_getcontrols();
	}
	if (!imgui_onetime_init) 
	{
		imgui_onetime_init = true;
	}

	bool bTitleSystemActive = true;
	if ((bTriggerWhatsNewInStoryboard && g_iWelcomeLoopPage == WELCOME_ANNOUNCEMENTS)) bTitleSystemActive = false;
	if (bTitleSystemActive)
	{
		// could not launch Welcome system before IMGUI inits, so flagged to happen here
		if (iTriggerWelcomeSystemStuff > 0 && iTriggerWelcomeSystemStuff < 6) iTriggerWelcomeSystemStuff++;
		if (iTriggerWelcomeSystemStuff > 5)
		{
			// Init or Cycle
			if (iTriggerWelcomeSystemStuff == 6)
			{
				// only show front dialogs if not resuming from previous session
				if (g.grestoreeditorsettings == 0)
				{
					// Welcome quick start page
					g.quickstartmenumode = 0;
					if (g.iFreeVersionModeActive != 0)
					{
						editor_showquickstart(0);
						iTriggerWelcomeSystemStuff = 99;
					}
					else
					{
						if (g.gshowonstartup == 1 || g.iTriggerSoftwareToQuit != 0)
						{
							if(!bAddWhatNewToMenu) //Already in menu.
								editor_showquickstart(0);
							iTriggerWelcomeSystemStuff = 99;
						}
						else
						{
							welcome_free();
							iTriggerWelcomeSystemStuff = 7;
						}
					}
				}
				else
				{
					// always need to close down loading splash
					welcome_free();
					iTriggerWelcomeSystemStuff = 7;
				}
			}
			else if (iTriggerWelcomeSystemStuff == 7)
			{
				//Exit wait for mouse release before closing welcome screen.
				if (t.inputsys.mclick == 0)
					iTriggerWelcomeSystemStuff = 0;
			}
			else
			{
				// Cycle - handle welcome loop
				if (welcome_cycle() == true)
				{
					welcome_free();
					iTriggerWelcomeSystemStuff = 7;
				}
			}

			//Make sure we dont sent input to rendertarget when in welcome.
			t.inputsys.xmouse = 500000;
			t.inputsys.ymouse = 0;
			t.inputsys.xmousemove = 0;
			t.inputsys.ymousemove = 0;
			set_inputsys_mclick(0);// t.inputsys.mclick = 0;
			t.inputsys.zmouse = 0;
			t.inputsys.wheelmousemove = 0;
			t.inputsys.activemouse = 0;
			t.syncthreetimes = 1;
			t.inputsys.k_s = "";
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
		}
	}

	// calc local cursor
	bool bPickActive = true;
	iReusePickObjectID = -1;
	pReusePickObject = 0;
	if (pref.iDragCameraMovement && t.ebe.on == 0 && bDragCameraActive) bPickActive = false;
	if (bPickActive)
	{
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-calclocalcursor"); input_calculatelocalcursor (); wi::profiler::EndRange(rP2); }
	}

	// Character Creator Plus
	if ( g_bCharacterCreatorPlusActivated == true )
	{
		// character creator plus character edited in situ
		charactercreatorplus_loop();
	}

	//  Importer or Main Editor
	if ( t.importer.loaded != 0 || (t.interactive.active == 1 && (t.interactive.pageindex<21 || t.interactive.pageindex>90)) )
	{
		//  Importer control or Interactive Mode
		if (  t.importer.loaded != 0 ) 
		{
			importer_loop ( );
			importer_draw ( );
		}
	}
	else
	{
		// Editor Controls
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-constructsel"); editor_constructionselection(); wi::profiler::EndRange(rP2); }

		if (t.grideditselect != 0)
		{
			//PE: Make sure to hide "cubes" when not editing.
			WickedCall_DisplayCubes(false);
		}

		if ( t.grideditselect == 3 || t.grideditselect == 4 ) 
		{
			// Entity controls
			editor_viewfunctionality ( );
		}
		else
		{
			{ auto rP2=wi::profiler::BeginRangeCPU("P2-mainfunc"); editor_mainfunctionality ( ); wi::profiler::EndRange(rP2); }
			if ( t.grideditselect == 0 ) 
			{
				// Terrain controls
				t.terrain.camx_f=t.cx_f ; t.terrain.camz_f=t.cy_f;
				t.terrain.zoom_f=t.gridzoom_f*0.12;
				{ auto rP2=wi::profiler::BeginRangeCPU("P2-terrain_editctl"); terrain_editcontrol ( ); wi::profiler::EndRange(rP2); }
				if (BackBufferImageID <= 0)
				{
					WickedCall_DisplayCubes(true);
				}

				//PE: Make sure clicks inside terrain tools also record a change, so level is saved.
				if (bImGuiRenderTargetFocus)
				{
					//  Any click inside 3D area constitues some sort of edit
					if (t.inputsys.mclick != 0)
					{
						g.projectmodified = 1;
						gridedit_changemodifiedflag();
						// effect on g.projectmodifiedstatic
					}
				}
			}
			else
			{
				if ( t.ebe.on == 1 )
				{
					// Easy Building Editor
					ebe_loop();
				}
				else
				{
					//  Non-terrain controls
					{ auto rP2=wi::profiler::BeginRangeCPU("P2-mapediting"); gridedit_mapediting ( ); wi::profiler::EndRange(rP2); }
				}
			}
		}
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-overallfunc"); editor_overallfunctionality ( ); wi::profiler::EndRange(rP2); }

		//  Handle visual components
		editor_detect_invalid_screen ( );
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-editor_visuals"); editor_visuals ( ); wi::profiler::EndRange(rP2); }
		
		//  Ensure entity animations speeds are controlled
		//  PERF 2026-07-25: in the EDITOR this maintenance pass (anim speed sync, decal frames,
		//  loop windows over every entity element, ~0.6ms) is time-based (g.timeelapsed_f), so
		//  running it every OTHER frame advances identically — halve the cadence in editor mode
		//  only (test game keeps every-frame for exact gameplay timing).
		if(!bExport_Standalone_Window)
		{
			extern bool bImGuiInTestGame;
			static uint32_t s_loopanimParity = 0;
			s_loopanimParity++;
			if (bImGuiInTestGame || t.game.gameisexe != 0 || (s_loopanimParity & 1) == 0)
			{
				auto rP2=wi::profiler::BeginRangeCPU("P2-entity_loopanim"); entity_loopanim ( ); wi::profiler::EndRange(rP2);
			}
		}

		//  Widget control
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-widget_loop"); widget_loop ( ); wi::profiler::EndRange(rP2); }

		//  Ensure lighting is updated as lighting is edited and moved
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-lighting_loop"); lighting_loop ( ); wi::profiler::EndRange(rP2); }

		//  Only show terrain cursor if in terrain edit mode
		if (  t.grideditselect == 0 && t.inputsys.mclick != 2 && t.inputsys.mclick != 4 && t.interactive.insidepanel == 0 ) 
		{
			terrain_cursor ( );
		}
		else
		{
			terrain_cursor_off ( );
		}

		//  Render terrain elements (shadowupdatepacer as shadow calc is expensive, time slice it)
		t.terrain.gameplaycamera=0;
		{ auto rP2=wi::profiler::BeginRangeCPU("P2-waterineditor"); terrain_waterineditor ( ); wi::profiler::EndRange(rP2); }

		// 111115 - keep track of memory between sessions with simpler SYSMEM minus STARTMEM calculation
		g.gamememactuallyused = SMEMAvailable(1) - g.gamememactuallyusedstart;

		// 111115 - and introduce sliding effect to hide flicker due to reading direct system memory value
		t.tmempercdest_f = (g.gamememactuallyused+0.0f) / (g.gamememactualmaxrightnow+0.0f);
		if ( t.tmemperc_f < t.tmempercdest_f-0.01f )
		{
			t.tmemperc_f = t.tmemperc_f + 0.01f;
		}
		else
		{
			if ( t.tmemperc_f > t.tmempercdest_f+0.01f )
			{
				t.tmemperc_f = t.tmemperc_f - 0.01f;
			}
		}

		if (  t.tmemperc_f > 1.0f  )  t.tmemperc_f = 1.0;
		if (  g.ghidememorygauge == 0 ) 
		{
			// (Dave) - check the image exists
			if ( ImageExist ( g.editorimagesoffset+2 ) == 1 )
			{
				Ink (  Rgb(0,0,0),0  ); Box (  2,2,102,18 );
				Ink (  Rgb(0,255,0),0  ); Box (  2,2,3+(99*t.tmemperc_f),18 );
				PasteImage (  g.editorimagesoffset+2,2,2,1 );
			}
		}
		//  End of Character Creator branch
	}

	//  Constantly checking if VIDMEM invalidated
	editor_detect_invalid_screen ( );

	// 191015 - test level click prompt
	if ( g.showtestlevelclickprompt > 0 )
	{
		if ( timeGetTime() > g.showtestlevelclickprompt )
		{
			g.showtestlevelclickprompt = 0;
		}
		int iXPos = 630;
		int iYPos = abs ( cos( timeGetTime()/500.0f )*35.0f );
		PasteImage ( g.editorimagesoffset+61, iXPos - (ImageWidth(g.editorimagesoffset+61)/2), 50+iYPos );
	}

	//  Update screen (if mouse in 3D are)
	if (  t.recoverdonotuseany3dreferences == 0 ) 
	{
		//  editor super chuggy
		if (  t.inputsys.activemouse == 1 ) 
		{
			//  constant update
			SyncRate ( 0 ); SyncMask ( 1 ); Sync ( ); SleepNow ( 5 );
		}
		else
		{
			//  check for PAINT message
			OpenFileMap (  3, "FPSEXCHANGE" );
			SetEventAndWait (  3 );
			if (  GetFileMapDWORD( 3, 60 ) == 1 ) 
			{
				SetFileMapDWORD (  3,60,0  ); t.syncthreetimes=3;
				SetEventAndWait (  3 );
			}
			SyncRate (  0 );
			if (  t.syncthreetimes>0 ) {  --t.syncthreetimes; Sync ( ); }
			SleepNow ( 10 );
		}

		//  Detect if resolution changed (windows)
		editor_detect_invalid_screen ( );
	}

	if (g_bCascadeQuitFlag) 
	{
		int iRet = AskSaveBeforeNewAction();
		if (iRet == 2)
		{
			g_bCascadeQuitFlag = false;
		}
		else 
		{
			PostQuitMessage(0);
		}
	}
}

void mapeditorexecutable_finish(void)
{
	// End map editor program (moved above chdir and pref writes)
	common_justbeforeend();

	// Come out of Files folder
	SetCurrentDirectoryA(g.fpscrootdir_s.Get());

	if (t.game.set.ismapeditormode == 1) {

		cstr prefile = defaultWriteFolder;
		prefile += "gamegurumax.pref";
		FILE* preffile = GG_fopen(prefile.Get(), "wb+");
		if (preffile) 
		{
			fwrite(&pref, 1, sizeof(pref), preffile);
			fclose(preffile);
		}

		if (pref.save_layout) {
			char cmLayoutFile[MAX_PATH];
			sprintf(cmLayoutFile, "%suimax.layout", defaultWriteFolder);
			ImGui::SaveIniSettingsToDisk(cmLayoutFile);
		}
	}
}

void mapeditorexecutable(void)
{
	mapeditorexecutable_init();

	// main loop
	while (!g_bCascadeQuitFlag)
	{
		mapeditorexecutable_loop();
	}
	mapeditorexecutable_finish();
}

int AskSaveBeforeNewAction(void)
{
	int iAction = 0;
	if (g.projectmodified == 1)
	{
		iAction = askBoxCancel("Do you wish to save first?", "Confirmation"); //1==Yes 2=Cancel 0=No

		if (iAction == 1)
		{
			//  yes save first
			if (g.projectfilename_s == "")
			{
				t.returnstring_s = "";
				cStr tOldDir = GetDir();
				char * cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_SAVE, "fpm\0*.fpm\0", g.mysystem.mapbankAbs_s.Get(), NULL, true);
				SetDir(tOldDir.Get());
				if (cFileSelected && strlen(cFileSelected) > 0) {
					t.returnstring_s = cFileSelected;
				}
				if (t.returnstring_s != "")
				{
					if (cstr(Lower(Right(t.returnstring_s.Get(), 4))) != ".fpm")  t.returnstring_s = t.returnstring_s + ".fpm";
					g.projectfilename_s = t.returnstring_s;
					bool oksave = true;
					if (FileExist(g.projectfilename_s.Get())) {
						oksave = overWriteFileBox(g.projectfilename_s.Get());
					}
					if (oksave) {
						gridedit_save_map();
					}
				}
			}
			else
			{
				gridedit_save_map();
			}

			g.projectmodified = 0; gridedit_changemodifiedflag();
			g.projectmodifiedstatic = 0;
		}
	}
	return iAction;

}

void editor_detect_invalid_screen ( void )
{
}

void editor_showhelppage ( int iHelpType )
{
	return;
}

void editor_showparentalcontrolpage ( void )
{
	// allow parental control to be activated and deactivated
	 OpenFileMap (  1, "FPSEXCHANGE" );
	 SetEventAndWait (  1 );
	do
	{
		set_inputsys_mclick(MouseClick());// t.inputsys.mclick = MouseClick();
		FastSync (  );
	} 
	while ( !(  t.inputsys.mclick == 0 ) );
	t.inputsys.kscancode=0;
	t.asx_f=1.0;
	t.asy_f=1.0;
	t.imgx_f=ImageWidth(g.editorimagesoffset+8)*t.asx_f;
	t.imgy_f=ImageHeight(g.editorimagesoffset+8)*t.asy_f;
	MAXSprite (  123,-10000,-10000,g.editorimagesoffset+8 );
	SizeSprite (  123,t.imgx_f,t.imgy_f );
	t.lastmousex=MouseX() ; t.lastmousey=MouseY();
	t.tpressf1toleave=0;
	int iStayInParentalControlDialog = 1;
	cstr ParentFourDigitCode = "";
	int digitcode[5];
	digitcode[0] = 0;
	digitcode[1] = 0;
	digitcode[2] = 0;
	digitcode[3] = 0;
	int digitcodeindex = 0;
	int tpressedbefore = 0;
	bool bParentalToggleForcesQuit = false;
	while ( iStayInParentalControlDialog == 1 && t.inputsys.kscancode != 27 ) 
	{
		 t.inputsys.kscancode=GetFileMapDWORD( 1, 100 );
		 if (  GetFileMapDWORD( 1, 908 ) == 1  )  break;
		 if (  GetFileMapDWORD( 1, 516 )>0  )  break;
		int tnewkeycode = 0; // numpad detection
		if ( t.inputsys.kscancode == 45 ) tnewkeycode = 48;
		if ( t.inputsys.kscancode == 35 ) tnewkeycode = 49;
		if ( t.inputsys.kscancode == 40 ) tnewkeycode = 50;
		if ( t.inputsys.kscancode == 34 ) tnewkeycode = 51;
		if ( t.inputsys.kscancode == 37 ) tnewkeycode = 52;
		if ( t.inputsys.kscancode == 12 ) tnewkeycode = 53;
		if ( t.inputsys.kscancode == 39 ) tnewkeycode = 54;
		if ( t.inputsys.kscancode == 36 ) tnewkeycode = 55;
		if ( t.inputsys.kscancode == 38 ) tnewkeycode = 56;
		if ( t.inputsys.kscancode == 33 ) tnewkeycode = 57;
		if ( tnewkeycode > 0 )
		{
			t.inputsys.kscancode = tnewkeycode;
		}
		if ( t.inputsys.kscancode >= 48 && t.inputsys.kscancode <= 57 )
		{
			if ( tpressedbefore == 0 && digitcodeindex < 4 )
			{
				if ( g.quickparentalcontrolmode == 1 )
				{
					// enable lock - entering password
					digitcode[digitcodeindex] = t.inputsys.kscancode;
					digitcodeindex++;
					ParentFourDigitCode = ParentFourDigitCode + "*";
					tpressedbefore = 1;
					if ( digitcodeindex == 4 )
					{
						g.quickparentalcontrolmode = 2;
						g.quickparentalcontrolmodepassword[0] = digitcode[0];
						g.quickparentalcontrolmodepassword[1] = digitcode[1];
						g.quickparentalcontrolmodepassword[2] = digitcode[2];
						g.quickparentalcontrolmodepassword[3] = digitcode[3];
						iStayInParentalControlDialog = 0;
						bParentalToggleForcesQuit = true;
						break;
					}
				}
				if ( g.quickparentalcontrolmode == 2 )
				{
					// disable lock - confirming password
					digitcode[digitcodeindex] = t.inputsys.kscancode;
					digitcodeindex++;
					ParentFourDigitCode = ParentFourDigitCode + "*";
					tpressedbefore = 1;
					if ( digitcodeindex == 4 )
					{
						// real password or secret backdoor
						bool bPasswordOkay = false;
						if (digitcode[0]==g.quickparentalcontrolmodepassword[0]
						&&	digitcode[1]==g.quickparentalcontrolmodepassword[1]
						&&	digitcode[2]==g.quickparentalcontrolmodepassword[2]
						&&	digitcode[3]==g.quickparentalcontrolmodepassword[3] ) bPasswordOkay = true;
						if (digitcode[0]==57 // 9119
						&&	digitcode[1]==49
						&&	digitcode[2]==49
						&&	digitcode[3]==57 ) bPasswordOkay = true;
						if ( bPasswordOkay == true )
						{
							g.quickparentalcontrolmode = 1;
							iStayInParentalControlDialog = 0;
							bParentalToggleForcesQuit = true;
							break;
						}
						else
						{
							// try again
							ParentFourDigitCode = "";
							digitcodeindex = 0;
						}
					}
				}

			}
		}
		else
		{
			tpressedbefore = 0;
		}

		t.terrain.gameplaycamera=0;
		int iDialogTop = (GetChildWindowHeight(0)-t.imgy_f)/2;
		PasteSprite ( 123, (GetChildWindowWidth(0)-t.imgx_f)/2, iDialogTop );
		LPSTR pRCMTitle = "RESTRICTED CONTENT MODE : OFF";
		if ( g.quickparentalcontrolmode == 2 ) pRCMTitle = "RESTRICTED CONTENT MODE : ON";
		pastebitmapfontcenter ( pRCMTitle, GetChildWindowWidth(0)/2, iDialogTop + 20, 3, 255 );
		pastebitmapfontcenter ( "This feature will control visibility of restricted content such as", GetChildWindowWidth(0)/2, iDialogTop + 70, 1, 255 );
		pastebitmapfontcenter ( "blood, violence and gore which may be offensive to some users.", GetChildWindowWidth(0)/2, iDialogTop + 95, 1, 255 );
		pastebitmapfontcenter ( "If you do not want this, press escape now.", GetChildWindowWidth(0)/2, iDialogTop + 120, 1, 255 );
		pastebitmapfontcenter ( ParentFourDigitCode.Get(), GetChildWindowWidth(0)/2, iDialogTop + (t.imgy_f/2), 4, 255 );
		if ( g.quickparentalcontrolmode == 2 )
		{
			pastebitmapfontcenter ( "ENTER YOUR FOUR DIGIT PASSWORD TO DEACTIVATE CONTENT LOCK", GetChildWindowWidth(0)/2, iDialogTop + t.imgy_f - 70, 2, 255 );
			pastebitmapfontcenter ( "OR PRESS [ESCAPE] TO CANCEL", GetChildWindowWidth(0)/2, iDialogTop + t.imgy_f - 50, 2, 255 );
		}
		else
		{
			pastebitmapfontcenter ( "ENTER FOUR DIGIT PASSWORD TO ACTIVATE CONTENT LOCK", GetChildWindowWidth(0)/2, iDialogTop + t.imgy_f - 70, 2, 255 );
			pastebitmapfontcenter ( "OR PRESS [ESCAPE] TO ENTER REGULAR MODE", GetChildWindowWidth(0)/2, iDialogTop + t.imgy_f - 50, 2, 255 );
		}
		pastebitmapfontcenter ( "YOU CAN ACCESS THIS OPTION AGAIN FROM THE HELP MENU", GetChildWindowWidth(0)/2, iDialogTop + t.imgy_f - 30, 2, 255 );
		Sync ( );
	}
	do
	{
		t.inputsys.kscancode=GetFileMapDWORD( 1, 100 );
		FastSync (  );
	} 
	while ( (  t.inputsys.kscancode > 3 ) ); //PE: We can keep getting virtual keys <= 3.
	//PE: Make sure we dont sent mouse input to whatever is below page.
	do
	{
		set_inputsys_mclick(MouseClick());// t.inputsys.mclick = MouseClick();
		FastSync();
	} while (!(t.inputsys.mclick == 0));

	// only a mode of 2 carries the digit code for activated
	if ( g.quickparentalcontrolmode != 2 )
	{
		digitcode[0] = 0;
		digitcode[1] = 0;
		digitcode[2] = 0;
		digitcode[3] = 0;
	}

	// 050416 - flag file to control parental control mode
	t.tfile_s=g.fpscrootdir_s+"\\parentalcontrolmode.ini";
	DeleteAFile (  t.tfile_s.Get() );
	if (  FileOpen(1)  ==  1  )  CloseFile (  1 );
	OpenToWrite (  1,t.tfile_s.Get() );
	WriteString (  1, cstr(g.quickparentalcontrolmode).Get() );
	WriteByte (  1, digitcode[0] );
	WriteByte (  1, digitcode[1] );
	WriteByte (  1, digitcode[2] );
	WriteByte (  1, digitcode[3] );
	CloseFile (  1 );
	t.tfile_s = g.fpscrootdir_s+"\\parentalcontrolactive.ini";
	if ( g.quickparentalcontrolmode == 2 )
	{
		// ensure file exists for IDE benefit
		OpenToWrite ( 1,t.tfile_s.Get() );
		WriteString ( 1, "123" );
		CloseFile ( 1 );
	}
	else
	{
		// delete this file to show IDE no parental control in effect
		DeleteAFile (  t.tfile_s.Get() );
	}

	// force the product to quit if change parental control setting
	if ( bParentalToggleForcesQuit == true )
	{
		MessageBoxA ( GetForegroundWindow(), "In order for the restricted content mode chosen to take effect, you must exit GameGuru and restart", "GameGuru Restart", MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST );
	}
}

void editor_freezeanimations ( void )
{
	// go through all objects and freeze their animations
	if ( t.fStoreObjAnimSpeeds==NULL )
	{
		t.fStoreObjAnimSpeeds = new float[210000];
		for ( int iObj = 1; iObj < 210000; iObj++ )
		{
			if ( ObjectExist ( iObj )==1 )
			{
				sObject* pObject = GetObjectData ( iObj );
				t.fStoreObjAnimSpeeds [ iObj ] = pObject->fAnimSpeed;
				pObject->fAnimSpeed = 0.0f;
			}
		}
	}
}

void editor_unfreezeanimations ( void )
{
	// go through all objects and restore all animation speeds from freeze step above
	if ( t.fStoreObjAnimSpeeds )
	{
		for ( int iObj = 1; iObj < 210000; iObj++ )
		{
			if ( ObjectExist ( iObj )==1 )
			{
				sObject* pObject = GetObjectData ( iObj );
				pObject->fAnimSpeed = t.fStoreObjAnimSpeeds [ iObj ];
			}
		}
		delete t.fStoreObjAnimSpeeds;
		t.fStoreObjAnimSpeeds = NULL;
	}
}

void editor_showquickstart ( int iForceMainOpen )
{
	// open welcome system
	editor_freezeanimations();
	if ( gbWelcomeSystemActive == false )
	{
		welcome_init(1);
		welcome_staticbackdrop();
		welcome_init(2);
	}
	welcome_init(0);

	// if first time run for VRQ
	if ( g.vrqTriggerSerialCodeEntrySystem == 1 )
	{
		 PostQuitMessage(0);
	}
	else
	{
		// Welcome system not syncronous any more
		if (g.iFreeVersionModeActive == 1)
		{
			welcome_show(WELCOME_FREEINTROAPP);
		}
		if (g.iFreeVersionModeActive == 2)
		{
			welcome_show(WELCOME_FREETRIALINTROAPP);
		}

		// if welcome not deactivated
		if (g.gshowonstartup != 0 || iForceMainOpen == 1)
		{
			// if no announcement wanting to share news
			if (g_iWelcomeLoopPage != WELCOME_ANNOUNCEMENTS)
			{
				// MAX only uses old welcome system for announcements right now
				welcome_show(WELCOME_ANNOUNCEMENTS);
				if (g_iWelcomeLoopPage != WELCOME_ANNOUNCEMENTS)
					bTriggerWhatsNewInStoryboard = false; //PE: No need to trigger nothing to show.
			}
			if (strlen(t.tlevelautoload_s.Get()) > 0)
			{
				//Trigger load level.
				welcome_free();
				t.tlevelautoload_s = "";
			}
		}
	}

	//  reset before leave
	t.inputsys.kscancode=0;
	set_inputsys_mclick(0);// t.inputsys.mclick = 0;
	t.inputsys.xmouse=0;
	t.inputsys.ymouse=0;
}

void editor_preparewindow (int iUseVRTest)
{
	//PE: Test game mode.
	extern DWORD gWindowVisible;

	RECT rect = { NULL };
	GetWindowRect(g_pGlob->hWnd, &rect);

	// correct size to restore (usingt "sizer" tool)
	gWindowSizeXOld = rect.right - rect.left;
	gWindowSizeYOld = rect.bottom - rect.top;

	gWindowPosXOld = rect.left;
	gWindowPosYOld = rect.top;

	gWindowVisibleOld = gWindowVisible; //SW_MAXIMIZE
	if (IsZoomed(g_pGlob->hWnd))
		gWindowMaximized = 1;
	else
		gWindowMaximized = 0;

	HMONITOR monitor = MonitorFromWindow(g_pGlob->hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	int monitor_width = info.rcMonitor.right - info.rcMonitor.left;
	int monitor_height = info.rcMonitor.bottom - info.rcMonitor.top;
	g_pGlob->dwWindowX = info.rcMonitor.left;
	g_pGlob->dwWindowY = info.rcMonitor.top;

	//  First call will toggle keyboard/mouse back to BACKGROUND (to capture all direct data)
	SetWindowModeOn ();

	//PE: Test game mode.
	SetWindowSettings(0, 0, 0);
	SetWindowPos(g_pGlob->hWnd, HWND_TOP, g_pGlob->dwWindowX, g_pGlob->dwWindowY, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetForegroundWindow(g_pGlob->hWnd);
	SetWindowSize(monitor_width, monitor_height);
	ShowWindow(); MaximiseWindow();
}

void editor_previewmapormultiplayer_initcode ( int iUseVRTest )
{
	//  store if project modified
	t.storeprojectmodified=g.projectmodified;
	g_tstoreprojectmodifiedstatic = g.projectmodifiedstatic; 

	//  flag that we clicked TEST GAME
	t.interactive.testgameused=1;

	g.tabmodehidehuds = 0; //Enable HUD if lua disabled it in prev session.

	//  Before launch test game, check if enough contiguous
	checkmemoryforgracefulexit();

	// called here for non-standalone
	if (t.game.gameisexe == 0)
	{
		editor_preparewindow(iUseVRTest);
	}
	else
	{
		// and called earlier before splash screen so no flicker!
	}
	 
	//Hide any windows outside main viewport.
	ImGui::HideAllViewPortWindows();
	LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	//  center mouse pointer in editor (and hide it)
	game_hidemouse ( );

	//  hide widget if was highlighted when tested game
	widget_hide ( );

	// hide EBE if active when click test game button
	ebe_hide();

	// hide texture terrain painter panels
	terrain_paintselector_hide();

	//  switch off any IDE entity highlighting
	t.geditorhighlightingtentityobj = 0;
	t.geditorhighlightingtentityID = 0;
	editor_restoreentityhighlightobj ( );

	// switch off any rubber band entity highlighting
	gridedit_clearentityrubberbandlist();

	// 210917 - refresh HLSL shaders (flagged as doing shader work)
	if ( g.gforceloadtestgameshaders == 1 )
	{
		// go through all loaded effects and reload them
		for ( t.t = -5 ; t.t <= g.effectbankmax; t.t++ )
		{
			char pEffectFilename[1024];
			strcpy ( pEffectFilename, "effectbank\\reloaded\\" );
			if ( t.t == -5 ) 
			{ 
				t.tteffectid = t.terrain.effectstartindex+0; 
				if ( g.gpbroverride == 1 )
					strcat ( pEffectFilename, "apbr_terrain.fx"); 
				else
					strcat ( pEffectFilename, "terrain_basic.fx"); 
			}
			if ( t.t == -4 ) 
			{ 
				t.tteffectid = t.terrain.effectstartindex+2; 
				if ( g.gpbroverride == 1 )
					strcat ( pEffectFilename, "apbr_veg.fx"); 
				else
					strcat ( pEffectFilename, "vegetation_basic.fx"); 
			}
			if ( t.t == -3 ) 
			{ 
				t.tteffectid = g.thirdpersonentityeffect; 
				if ( g.gpbroverride == 1 )
					strcat ( pEffectFilename, "apbr_basic.fx"); 
				else
					strcat ( pEffectFilename, "entity_basic.fx"); 
			}
			if ( t.t == -2 ) 
			{ 
				t.tteffectid = g.thirdpersoncharactereffect; 
				if ( g.gpbroverride == 1 )
					strcat ( pEffectFilename, "apbr_animwithtran.fx"); 
				else
					strcat ( pEffectFilename, "character_basic.fx"); 
			}
			if ( t.t == -1 ) { t.tteffectid = g.staticlightmapeffectoffset; strcat ( pEffectFilename, "static_basic.fx"); }
			if ( t.t == 0 ) { t.tteffectid = g.staticshadowlightmapeffectoffset; strcat ( pEffectFilename, "shadow_basic.fx"); }
			if ( t.t > 0 ) { t.tteffectid = g.effectbankoffset+t.t; strcpy ( pEffectFilename, t.effectbank_s[t.t].Get()); }
			if ( GetEffectExist ( t.tteffectid ) == 1 ) 
			{
				// gather all objects that use this effect
				int iObjListMax = 0;
				DWORD** pObjList = new DWORD* [ g_iObjectListCount ];
				memset ( pObjList, 0, sizeof(DWORD*)*g_iObjectListCount );
				for ( DWORD dwObject = 0; dwObject < (DWORD)g_iObjectListCount; dwObject++ )
				{
					sObject* pObject = g_ObjectList [ dwObject ];
					if ( pObject )
					{ 
						bool bAnyMeshUsingEffect = false;
						for ( DWORD dwMesh = 0; dwMesh < (DWORD)pObject->iMeshCount; dwMesh++ )
						{
							if ( pObject->ppMeshList [ dwMesh ]->pVertexShaderEffect == m_EffectList [ t.tteffectid ]->pEffectObj )
							{
								bAnyMeshUsingEffect = true;
							}
						}
						if ( bAnyMeshUsingEffect == true )
						{
							DWORD* pPerObjData = new DWORD[1+pObject->iMeshCount];
							memset ( pPerObjData, 0, sizeof(DWORD)*(1+pObject->iMeshCount) );
							pObjList[iObjListMax] = pPerObjData;
							*(pPerObjData+0) = dwObject;
							DWORD dwObjDataIndex = 1;
							for ( DWORD dwFrameIndex = 0; dwFrameIndex < (DWORD)pObject->iFrameCount; dwFrameIndex++ )
							{
								if ( pObject->ppFrameList [ dwFrameIndex ]->pMesh )
								{
									if ( pObject->ppFrameList [ dwFrameIndex ]->pMesh->pVertexShaderEffect == m_EffectList [ t.tteffectid ]->pEffectObj )
									{
										*(pPerObjData+dwObjDataIndex) = 1+dwFrameIndex;
										dwObjDataIndex++;
									}
								}
							}
							iObjListMax++;
						}
					}
				}

				// delete the old effect and load a new one
				DeleteEffect ( t.tteffectid );
				LoadEffect ( pEffectFilename, t.tteffectid, 0 );
				filleffectparamarray ( t.tteffectid );

				// set the new effects to each object in the list
				if ( iObjListMax > 0 )
				{
					for ( int iObjListIndex = 0; iObjListIndex < iObjListMax; iObjListIndex++ )
					{
						DWORD* pPerObjData = pObjList[iObjListIndex];
						DWORD dwObject = *(pPerObjData+0);
						sObject* pObject = g_ObjectList [ dwObject ];
						if ( pObject )
						{
							for ( DWORD dwObjDataIndex = 0; dwObjDataIndex < (DWORD)pObject->iMeshCount; dwObjDataIndex++ )
							{
								DWORD dwFrameIndex = *(pPerObjData+1+dwObjDataIndex);
								if ( dwFrameIndex > 0 )
								{
									dwFrameIndex--;
									SetLimbEffect ( dwObject, dwFrameIndex, t.tteffectid );
								}
							}
						}
						SAFE_DELETE(pPerObjData);
					}
				}
				SAFE_DELETE(pObjList);

				// by default, set to first technique
				SetEffectTechnique ( t.tteffectid, NULL );
			}
		}

		// re-assign params for reloaded terrain and veg
		terrain_applyshader();
	}
	
	//  set-up test game screen prompt assets
	if ( t.game.runasmultiplayer == 1 ) 
	{
		loadscreenpromptassets(2);
		 printscreenprompt("ENTERING MULTIPLAYER MODE");
	}
	else
	{
		loadscreenpromptassets(iUseVRTest);
		printscreenprompt("LAUNCHING TEST LEVEL");
	}

	//  Save editor configuration
	timestampactivity(0,"PREVIEWMAP: Save config");
	editor_savecfg ( );

	// level saving takes 25% of overall 'click test level on large level'
	// removed for now in favour of user choosing when they should save/backup their creations
	// can restore this if we can get save to sub-3 seconds.
	g.gpretestsavemode = 0;

	// Now saves all part-files into temp FPM file (which multiplayer can pick up later)
	if ( t.game.runasmultiplayer == 1 ) 
	{
		//  save temp copy of current level
		g.projectfilename_s=g.mysystem.editorsGrideditAbs_s+"worklevel.fpm";//g.fpscrootdir_s+"\\Files\\editors\\gridedit\\worklevel.fpm";
		editor_savecfg ( );
		mapfile_saveproject_fpm ( );
	}

	// GCStore could have assed assets since the last 'test game' so refresh internal lists
	sky_init ( );
	terrain_initstyles ( );

	// Re-acquire indices now the lists have changed
	// takes visuals.sky$ visuals.terrain$ visuals.vegetation$
	visuals_updateskyterrainvegindex ( );

	// Ensure game visuals settings used
	t.gamevisuals.skyindex=t.visuals.skyindex;
	t.gamevisuals.sky_s=t.visuals.sky_s;
	t.gamevisuals.terrainindex=t.visuals.terrainindex;
	t.gamevisuals.terrain_s=t.visuals.terrain_s;
	t.gamevisuals.vegetationindex=t.visuals.vegetationindex;
	t.gamevisuals.vegetation_s=t.visuals.vegetation_s;
	t.gamevisuals.iEnvironmentWeather = t.visuals.iEnvironmentWeather;

	// the visuals vs gamevisuals could do with some work, I noticed our ambience is being overwritten
	// when it really needed to be transferred to the gamevisuals (is this done elsewhere?)
	t.gamevisuals.AmbienceRed_f = t.visuals.AmbienceRed_f;
	t.gamevisuals.AmbienceGreen_f = t.visuals.AmbienceGreen_f;
	t.gamevisuals.AmbienceBlue_f = t.visuals.AmbienceBlue_f;
	t.gamevisuals.SunAngleX = t.visuals.SunAngleX;
	t.gamevisuals.SunAngleY = t.visuals.SunAngleY;
	t.gamevisuals.SunAngleZ = t.visuals.SunAngleZ;
	t.gamevisuals.bSSREnabled = t.visuals.bSSREnabled;
	t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled;

	t.gamevisuals.bDOF = t.visuals.bDOF;
	t.gamevisuals.fDOFStrength = t.visuals.fDOFStrength;
	t.gamevisuals.fDOFApertureSize = t.visuals.fDOFApertureSize;
	t.gamevisuals.fDOFFocalLength = t.visuals.fDOFFocalLength;

	t.gamevisuals.bLightShafts = t.visuals.bLightShafts;
	t.gamevisuals.bLensFlare = t.visuals.bLensFlare;
	t.gamevisuals.fsetBloomRadius = t.visuals.fsetBloomRadius;
	t.gamevisuals.fsetBloomCap = t.visuals.fsetBloomCap;
	t.gamevisuals.bReflectionsEnabled = t.visuals.bReflectionsEnabled;
	t.gamevisuals.iShadowSpotCascadeResolution = t.visuals.iShadowSpotCascadeResolution;
	t.gamevisuals.iShadowPointMax = t.visuals.iShadowPointMax;
	t.gamevisuals.iShadowPointResolution = t.visuals.iShadowPointResolution;
	t.gamevisuals.iShadowSpotMax = t.visuals.iShadowSpotMax;
	t.gamevisuals.iShadowSpotResolution = t.visuals.iShadowSpotResolution;
	t.gamevisuals.iEnvProbeResolution = t.visuals.iEnvProbeResolution;
	t.gamevisuals.newperformancepresets = t.visuals.newperformancepresets;

	// ensure all optimization states are transferred to the game
	t.gamevisuals.shaderlevels.entities = t.visuals.shaderlevels.entities;
	t.gamevisuals.shaderlevels.lighting = t.visuals.shaderlevels.lighting;
	t.gamevisuals.shaderlevels.terrain = t.visuals.shaderlevels.terrain;
	t.gamevisuals.shaderlevels.vegetation = t.visuals.shaderlevels.vegetation;
	t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling;
	t.gamevisuals.bEnableObjectCulling = t.visuals.bEnableObjectCulling;
	t.gamevisuals.bEnableTerrainChunkCulling = t.visuals.bEnableTerrainChunkCulling;
	t.gamevisuals.bEnablePointShadowCulling = t.visuals.bEnablePointShadowCulling;
	t.gamevisuals.bEnableSpotShadowCulling = t.visuals.bEnableSpotShadowCulling;
	t.gamevisuals.bEnableAnimationCulling = t.visuals.bEnableAnimationCulling;
	t.gamevisuals.fLODMultiplier = t.visuals.fLODMultiplier;
	t.gamevisuals.bDisableSkybox = t.visuals.bDisableSkybox;
	t.gamevisuals.bEnable30FpsAnimations = t.visuals.bEnable30FpsAnimations;

	t.gamevisuals.bShadowsLowestLOD = t.visuals.bShadowsLowestLOD;
	t.gamevisuals.bProbesLowestLOD = t.visuals.bProbesLowestLOD;
	t.gamevisuals.bRaycastLowestLOD = t.visuals.bRaycastLowestLOD;
	t.gamevisuals.bPhysicsLowestLOD = t.visuals.bPhysicsLowestLOD;
	t.gamevisuals.bThreadedPhysics = t.visuals.bThreadedPhysics;
	t.gamevisuals.bPhysicsWorldAt60 = t.visuals.bPhysicsWorldAt60;
	t.gamevisuals.bReflectionsLowestLOD = t.visuals.bReflectionsLowestLOD;

	t.gamevisuals.g_bDelayedShadows = t.visuals.g_bDelayedShadows;
	t.gamevisuals.g_bDelayedShadowsLaptop = t.visuals.g_bDelayedShadowsLaptop;
	t.gamevisuals.ApparentSize = t.visuals.ApparentSize;
	t.gamevisuals.bReflectionsEnabled = t.visuals.bReflectionsEnabled;
	t.gamevisuals.bLevelVSyncEnabled = t.visuals.bLevelVSyncEnabled;

	t.gamevisuals.ColorGradingLUT = t.visuals.ColorGradingLUT;
	t.gamevisuals.bColorGrading = t.visuals.bColorGrading;

	// copy game visuals to visuals for use in level play
	t.visuals = t.gamevisuals;

	gggrass_save_params = gggrass_global_params;
	// ★★★ GGMAX 3.96: SNAPSHOT THE TERRAIN AND TREE PARAMS TOO - THE PRESET BELOW STAMPS
	// THEM AND THEY WERE NEVER PUT BACK, AND THEY ARE SAVED INTO THE LEVEL FILE.
	//
	// GRASS was snapshotted here from the start and restored on the way out. Terrain and trees
	// were not, and SetGlobalGraphicsSettings dials all three (M-GridEdit_part0.cpp:1775-1777).
	//
	// ⚠⚠ THIS IS NOT COSMETIC - IT REACHES DISK. Measured end to end on Switch Escape:
	// load the shipped level, one Test Level round trip, File>Save, and five of six terrain
	// fields in the level's ggterrain.dat had changed to the LOW preset's values:
	//     segments_per_chunk 64 -> 16     segment_size 8.0 -> 16.0
	//     detailScale 0.586464 -> 0.5     detailLimit 0 -> 2     tilingPower 0.56 -> 0.68
	// The chain is SetGlobalGraphicsSettings -> GGTerrain_SetPerformanceMode writes the GLOBAL
	// params -> CheckParams copies global to local (GGTerrain_part0.cpp:3539/3577/3612) ->
	// GGTerrain_SaveSettings serialises the LOCALS (:5769). A tester who tests their level and
	// saves bakes the downgrade in permanently, and nothing tells them.
	//
	// ★ It had already happened to Lee's own testpro2level, which carried the LOW signature on
	// all six fields while NOT ONE of the 21 shipped demos did. That mismatch is what turned a
	// suspicion into a measurement.
	{
		ggterrain_save_params = GGTerrain::ggterrain_global_params;
		ggterrain_save_render_params = GGTerrain::ggterrain_global_render_params;
		ggterrain_save_render_params2 = GGTerrain::ggterrain_global_render_params2;
		ggtrees_save_params = GGTrees::ggtrees_global_params;
	}

	// GGMAX 3.87: THE TEST GAME QUALITY PRESET MUST NOT DESTROY THE LEVEL'S AUTHORED POST PROCESSING.
	//
	// Lee, on Switch Escape: the editor's Post Processing panel shows Reflections, FXAA, Light
	// Shafts and Lens Flare all ON, and the moment he presses Test Level all four are OFF. Those
	// four plus SSR are EXACTLY the five fields SetGlobalGraphicsSettings writes
	// (M-GridEdit_part0.cpp:1782-1786) - its LOW case sets all five false.
	//
	// The copy that was supposed to carry the author's choice into the game is already correct and
	// is NOT the bug: lines 990-1000 above copy all five from t.visuals into t.gamevisuals, and
	// line 1041 copies them back. They arrive intact and are then stamped over, two lines later.
	//
	// FIXED HERE, AT THE CALL SITE, AND NOT IN SetGlobalGraphicsSettings, because that function has
	// a SECOND CALLER WITH THE OPPOSITE INTENT: M-LUA-General.cpp:1625 runs it from the shipped
	// game's own graphics options menu, where stamping these five IS the player's choice. Gutting
	// the preset the way GGMAX 2.39 gutted its shadow writes would have silently disabled that
	// menu. Only this path is destroying authored level data.
	//
	// What the preset still does here is untouched: GGTerrain/GGTrees/GGGrass performance modes,
	// which is where a low-spec test game actually gets its frame time. Only the five level-authored
	// post-processing flags are put back.
	//
	// Third defect at this one call site - 2.39 stopped it stamping the level's authored SHADOW
	// settings for the same reason. An apply with no restore. See NIGHT_INVESTIGATIONS 3.86/3.87.
	if(pref.iTestGameGraphicsQuality != 2)
	{
		const bool ggAuthoredSSR         = t.visuals.bSSREnabled;
		const bool ggAuthoredFXAA        = t.visuals.bFXAAEnabled;
		const bool ggAuthoredLightShafts = t.visuals.bLightShafts;
		const bool ggAuthoredLensFlare   = t.visuals.bLensFlare;
		const bool ggAuthoredReflections = t.visuals.bReflectionsEnabled;

		SetGlobalGraphicsSettings( pref.iTestGameGraphicsQuality );

		t.visuals.bSSREnabled         = ggAuthoredSSR;
		t.visuals.bFXAAEnabled        = ggAuthoredFXAA;
		t.visuals.bLightShafts        = ggAuthoredLightShafts;
		t.visuals.bLensFlare          = ggAuthoredLensFlare;
		t.visuals.bReflectionsEnabled = ggAuthoredReflections;
		t.gamevisuals.bSSREnabled         = ggAuthoredSSR;
		t.gamevisuals.bFXAAEnabled        = ggAuthoredFXAA;
		t.gamevisuals.bLightShafts        = ggAuthoredLightShafts;
		t.gamevisuals.bLensFlare          = ggAuthoredLensFlare;
		t.gamevisuals.bReflectionsEnabled = ggAuthoredReflections;
	}

	t.visuals.refreshshaders=1;
	t.visuals.refreshvegtexture=1;

	// Hide camera while prepare test map
	t.storecx_f=CameraPositionX();
	t.storecy_f=CameraPositionY();
	t.storecz_f=CameraPositionZ();

	// default start position is edit-camera XZ
	t.terrain.playerx_f = CameraPositionX(0);
	t.terrain.playerz_f = CameraPositionZ(0);
	t.terrain.playery_f = BT_GetGroundHeight(t.terrain.TerrainID, t.terrain.playerx_f, t.terrain.playerz_f) + 150.0;
	t.terrain.playerax_f = 0.0;
	t.terrain.playeray_f = 0.0;
	t.terrain.playeraz_f = 0.0;

	// store all editor entity positions and rotations
	t.storedentityelementlist=g.entityelementlist;
	t.storedentityviewcurrentobj=g.entityviewcurrentobj;
	Dim (  t.storedentityelement,g.entityelementlist );
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.storedentityelement[t.e]=t.entityelement[t.e];
	}

	// hide all markers
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		t.entid = t.entityelement[t.e].bankindex;
		t.obj = t.entityelement[t.e].obj;
		if (t.obj > 0)
		{
			if (ObjectExist(t.obj) == 1)
			{
				if (t.entityprofile[t.entid].ismarker != 0)
				{
					//  all markers must be hidden
					HideObject(t.obj);
				}
				if (t.entityprofile[t.entid].addhandlelimb > 0)
				{
					//  hide decal handles
					HideLimb(t.obj, t.entityprofile[t.entid].addhandlelimb);
				}
			}
		}
	}
	
	// ensure all locked entity transparency resolves
	for ( t.tte = 1 ; t.tte<=  g.entityelementlist; t.tte++ )
	{
		if ( t.entityelement[t.tte].editorlock == 1 || t.entityelement[t.tte].underground == 1 ) 
		{
			t.tobj=t.entityelement[t.tte].obj;
			if ( t.tobj>0 ) 
			{
				if ( ObjectExist(t.tobj) == 1 ) 
				{
					if ( t.entityelement[t.tte].underground == 1  )  t.entityelement[t.tte].isclone = 1;
					entity_converttoinstance ( );
				}
			}
		}
	}

	//PE: start any animations that are not in editor mode.
	for (t.tte = 1; t.tte <= g.entityelementlist; t.tte++)
	{
		// hide EBE markers
		int iIndex = t.entityelement[t.tte].bankindex;
		if (t.entityprofile[iIndex].isebe != 0)
		{
			t.tobj = t.entityelement[t.tte].obj;
			if (t.tobj > 0)
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

				//FULLBOUNDS
				if (t.entityprofile[t.entid].startanimingame > 0) 
				{
					if (t.entityprofile[t.entid].animmax > 0) 
					{
						t.q = t.entityprofile[t.entid].startanimingame - 1;
						SetObjectFrame(t.tttsourceobj, 0);
						LoopObject(t.tttsourceobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
						SetObjectFrame(t.tobj, 0);
						LoopObject(t.tobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
					}
				}
				else 
				{
					//PE: Restore any non char animations.
					if (t.tobj > 0 && t.entityprofile[t.entid].ischaracter == 0 && GetNumberOfFrames(t.tobj) > 0)
					{
						SetObjectFrame(t.tobj, 0);
						if (t.entityprofile[t.entid].animmax > 0 && t.entityprofile[t.entid].playanimineditor > 0 && t.entityprofile[t.entid].ischaractercreator == 0)
						{
							t.q = t.entityprofile[t.entid].playanimineditor - 1;
							LoopObject(t.tobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
						}
						else if (t.entityprofile[t.entid].playanimineditor < 0)
						{
							// uses name instead of index, the negative is the ordinal into the animset
							extern void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname);
							entity_loop_using_negative_playanimineditor(t.tte, t.tobj, t.entityprofile[t.entid].playanimineditor_name);
						}
						else
						{
							LoopObject(t.tobj); StopObject(t.tobj);
						}
					}

				}
			}
		}
	}

	editor_toggle_element_vis(t.showtestgameelements);

	// hide editor objects too
	for ( t.obj = t.editor.objectstartindex+1; t.obj <= t.editor.objectstartindex+1+10 ;  t.obj++ ) //?//t.editor.objectstartindex+10;
	{
		if ( ObjectExist(t.obj) == 1 ) 
		{
			HideObject (  t.obj );
		}
	}

	// hide any EBE site markers (limb zeros)
	for ( t.tte = 1; t.tte <= g.entityelementlist; t.tte++ )
	{
		int iIndex = t.entityelement[t.tte].bankindex;
		if ( t.entityprofile[iIndex].isebe != 0 ) 
		{
			t.tobj = t.entityelement[t.tte].obj;
			if ( t.tobj>0 ) 
			{
				if ( ObjectExist(t.tobj) == 1 ) 
				{
					// when EBE entity is loaded first time, no editing, the handle object at limb zero is not used
					sObject* pObject = GetObjectData(t.tobj);
					if ( pObject->iFrameCount > 1 )
						HideLimb ( t.tobj, 0 );
				}
			}
		}
	}

	// ensure no collision from legacy engine
	AutomaticCameraCollision (  0,0,0 );
	SetGlobalCollisionOff (  );

	// Setup game view camera
	SetCameraFOV ( 75 );
	g.grav_f=-5.0;

	// store original terrain heights
	if ( t.terrain.TerrainID>0 ) 
	{
		for ( t.z = 0 ; t.z<=  1024; t.z++ )
		{
			for ( t.x = 0 ; t.x<=  1024; t.x++ )
			{
				t.h_f=BT_GetGroundHeight(t.terrain.TerrainID,t.x*50.0,t.z*50.0,1);
				t.terrainmatrix[t.x][t.z]=t.h_f;
			}
		}
	}

	// Create heightmap from this terrain (for quad reduction)
	if ( t.terrain.TerrainID>0 ) 
	{
		t.terrain.terrainregionupdate=0;
		terrain_refreshterrainmatrix ( );
		t.theightfile_s=g.mysystem.levelBankTestMap_s+"heightmap.dds"; //"levelbank\\testmap\\heightmap.dds";
		terrain_createheightmapfromheightdata ( );
	}

	// full speed
	SyncRate ( 0 );

	// Work out the amount of memory used for the TEST GAME session
	t.tmemorybeforetestgame=SMEMAvailable(1);

	//
	// launch game root with IDE 'test at cursor position' settings
	//
	t.game.set.resolution=0;
	t.game.set.initialsplashscreen=0;
	t.game.set.ismapeditormode=0;
	extern int tgamesetismapeditormode;
	tgamesetismapeditormode = 0;
	WickedCall_SetEditorCameraLight(false);

	// can now edit behavior logic 'live'
	extern void gridedit_restartanybehaviorediting(void);
	gridedit_restartanybehaviorediting();

	//PE: Clear any test mode / standalone highlights.
	extern std::vector<int> g_StandaloneObjectHighlightList;
	g_StandaloneObjectHighlightList.clear(); //PE: They clear on all frames so this is the only thing needed.
	extern bool bActivateStandaloneOutline;
	bActivateStandaloneOutline = false;
	// game loop init code
	game_masterroot_initcode ( iUseVRTest );
}

bool editor_previewmapormultiplayer_loopcode ( int iUseVRTest )
{
	bool bEndThisLoop = false;
	g_bDisableQuitFlag = true;
	bEndThisLoop = game_masterroot_loopcode ( iUseVRTest );
	g_bDisableQuitFlag = false;
	return bEndThisLoop;
}

namespace wi::renderer { extern float gg_light_max_distance; void SetParticleScatter(float value); } // GGMAX (wiRenderer.cpp)
void editor_previewmapormultiplayer_afterloopcode ( int iUseVRTest )
{
	// game after loop code
	game_masterroot_afterloopcode ( iUseVRTest );

	t.terrain.skysundirectionx_f = t.terrain.sundirectionx_f;
	t.terrain.skysundirectiony_f = t.terrain.sundirectiony_f;
	t.terrain.skysundirectionz_f = t.terrain.sundirectionz_f;

	t.game.set.ismapeditormode=1;
	extern int tgamesetismapeditormode;
	tgamesetismapeditormode = 1;

	//PE: This will make sure everything spawed is deleted , like physics / sound / waypoints / attachments ...
	void CleanUpSpawedObject(void);
	CleanUpSpawedObject();

	//PE: Clear all wicked particle effects created by lua.
	void CleanUpEmitterEffects(void);
	CleanUpEmitterEffects();

	WickedCall_SetEditorCameraLight(true);

	//PE: Hide any hit decals.
	decal_hide();

	// restore any EBE site markers (limb zeros)
	for ( t.tte = 1; t.tte <= g.entityelementlist; t.tte++ )
	{
		int iIndex = t.entityelement[t.tte].bankindex;
		if ( t.entityprofile[iIndex].isebe != 0 ) 
		{
			t.tobj = t.entityelement[t.tte].obj;
			if ( t.tobj>0 ) 
			{
				if ( ObjectExist(t.tobj) == 1 ) 
				{
					ShowLimb ( t.tobj, 0 );
				}
			}
		}
	}

	// Revert mode to only render NEAR technique
	visuals_restoreterrainshaderforeditor ( );

	// editor speed max
	SyncMask ( 1 );
	SyncRate ( 0 );

	//PE: release mouse so all monitors can be used.
	ClipCursor(NULL);

	// restore mouse pos and visbility
	game_showmouse ( );

	// prompt informing user we are saving the level changes
	if ( t.conkit.modified == 1 ) 
	{
		popup_text("Saving level changes");
	}

	//  show all waypoints and zones
	waypoint_restore ( );

	// 101115 - restore all characters to use regular character shader
	game_setup_character_shader_entities ( false );

	// if additional entities added, remove and restore orig count
	if ( g.entityelementlist>t.storedentityelementlist ) 
	{
		for ( t.e = t.storedentityelementlist+1 ; t.e<= g.entityelementlist ; t.e++ )
		{
			t.obj=t.entityelement[t.e].obj;
			if ( t.obj>0 ) 
			{
				if ( ObjectExist(t.obj) == 1 ) 
				{
					DeleteObject ( t.obj );
				}
			}
			t.entityelement[t.e].obj=0;
			t.entityelement[t.e].bankindex=0;
		}
		g.entityelementlist=t.storedentityelementlist;
		g.entityviewcurrentobj=t.storedentityviewcurrentobj;
	}

	// in addition, remove any that where spawned inside the original list
	bool bClearAllInGameSpawns = true;
	if ( bClearAllInGameSpawns == true )
	{
		for (t.e = 1; t.e <= g.entityelementlist; t.e++)
		{
			if (t.entityelement[t.e].iWasSpawnedInGame > 0)
			{
				t.obj = t.entityelement[t.e].obj;
				if (t.obj > 0)
				{
					if (ObjectExist(t.obj) == 1)
					{
						DeleteObject (t.obj);
					}
				}
				t.entityelement[t.e].obj = 0;
				t.entityelement[t.e].bankindex = 0;
				t.entityelement[t.e].iWasSpawnedInGame = 0;
			}
		}
	}

	// restore all editor entity positions and rotations
	for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
	{
		t.obj=t.entityelement[t.e].obj;
		if ( t.obj>0 ) 
		{
			if ( ObjectExist(t.obj) == 1 ) 
			{
				// ensure particles can be restored (realtime changes)
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_Speed_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_Speed_Original;
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_Opacity_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_Opacity_Original;
				t.storedentityelement[t.e].eleprof.newparticle.bParticle_Size_Original = t.entityelement[t.e].eleprof.newparticle.bParticle_Size_Original;
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_Floor_Height_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_Floor_Height_Original;
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_Bounciness_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_Bounciness_Original;
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_R_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_R_Original;
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_G_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_G_Original;
				t.storedentityelement[t.e].eleprof.newparticle.fParticle_B_Original = t.entityelement[t.e].eleprof.newparticle.fParticle_B_Original;

				// only if still exists - could have been deleted
				t.entityelement[t.e] = t.storedentityelement[t.e];

				// and if particle, restore the real-time changes (emitter rot and pos)
				entity_updateparticleemitter(t.e);

				// and then wipe those stored settings
				t.entityelement[t.e].eleprof.newparticle.fParticle_Speed_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.fParticle_Opacity_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.bParticle_Size_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.fParticle_Floor_Height_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.fParticle_Bounciness_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.fParticle_R_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.fParticle_G_Original = -123.0f;
				t.entityelement[t.e].eleprof.newparticle.fParticle_B_Original = -123.0f;
			}
		}
	}
	UnDim ( t.storedentityelement );

	// restore entity positions and rotations
	for ( t.e = 1 ; t.e <= g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		t.obj=t.entityelement[t.e].obj;
		if ( t.obj>0 ) 
		{
			if ( ObjectExist(t.obj) == 1 ) 
			{
				//PE: Also restore moved particles.
				if ( t.entityprofile[t.entid].ismarker == 0  || t.entityprofile[t.entid].ismarker == 10 )
				{
					// reset entity
					PositionObject (  t.obj,t.entityelement[t.e].x,t.entityelement[t.e].y,t.entityelement[t.e].z );
					RotateObject (  t.obj,t.entityelement[t.e].rx,t.entityelement[t.e].ry,t.entityelement[t.e].rz );
					ScaleObject(t.obj, 100 + t.entityelement[t.e].scalex, 100 + t.entityelement[t.e].scaley, 100 + t.entityelement[t.e].scalez);
					ShowObject (  t.obj );

					//PE: Sometimes NPC's face the wrong way after they have been ragdoll.
					if (t.entityprofile[t.entid].ischaracter == 1)
					{
						sObject* pObject = GetObjectData(t.obj);
						void entity_resetlimbtwists(sObject * pObject, int e);
						entity_resetlimbtwists(pObject, t.e);
						RotateLimb(t.obj, 0, 0, 0, 0);

						entity_calculateeuleryfromquat(t.e); //PE: Always get correct rotation.
						RotateObject(t.obj, t.entityelement[t.e].rx, t.entityelement[t.e].ry, t.entityelement[t.e].rz);

						//PE: Ragdoll can remove pivot , so reset if not set. (NPCs would face the wrong way)
						if (!pObject->position.bApplyPivot)
						{
							RotateObject(t.obj, 0, 180, 0);
							FixObjectPivot(t.obj);
							//PE: Need to rotate again for pivot to kick in.
							RotateObject(t.obj, t.entityelement[t.e].rx, t.entityelement[t.e].ry, t.entityelement[t.e].rz);
						}
					}
					
					// restore zdepth mode of this entity
					entity_preparedepth(t.entid, t.obj);
				}
				if ( t.entityprofile[t.entid].addhandlelimb>0 ) 
				{
					ShowLimb ( t.obj,t.entityprofile[t.entid].addhandlelimb );
				}
			}
		}
	}

	// restore new particles that may have been deleted
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		int iParticleEmitter = t.entityelement[t.e].eleprof.newparticle.emitterid;
		if (iParticleEmitter != -1)
		{
			if (t.entityelement[t.e].eleprof.newparticle.bParticle_Preview == true)
				gpup_emitterActive(iParticleEmitter, 1);
			else
				gpup_emitterActive(iParticleEmitter, 0);
		}
	}

	// show all markers
	t.gridentityhidemarkers=0;
	editor_updatemarkervisibility ( );

	// ensure all locked entity transparency resolves
	for ( t.tte = 1 ; t.tte<=  g.entityelementlist; t.tte++ )
	{
		if ( t.entityelement[t.tte].editorlock == 1 || t.entityelement[t.tte].underground == 1 ) 
		{
			t.tobj=t.entityelement[t.tte].obj;
			if ( t.tobj>0 ) 
			{
				if ( ObjectExist(t.tobj) == 1 ) 
				{
					//PE: Re-enable transparent on locked entities.
					if(t.entityelement[t.tte].editorlock == 1)
					{
						t.entityelement[t.tte].isclone = 0;
						entity_converttoclonetransparent();
					}
					else 
					{
						if (t.entityelement[t.tte].underground == 1) t.entityelement[t.tte].isclone = 1;
						entity_converttoinstance();
					}
				}
			}
		}
	}

	//PE: disable any animations that should not be in editor.
	for (t.tte = 1; t.tte <= g.entityelementlist; t.tte++)
	{
		// hide EBE markers
		int iIndex = t.entityelement[t.tte].bankindex;
		if (t.entityprofile[iIndex].isebe != 0)
		{
			t.tobj = t.entityelement[t.tte].obj;
			if (t.tobj > 0)
			{
				if (ObjectExist(t.tobj) == 1)
				{
					ShowLimb(t.tobj, 0);
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
				//PE: Make sure we reset all animations. mainly for lua controlled objects like doors
				if (t.entityprofile[t.entid].animmax > 0)
				{
					SetObjectFrame(t.tttsourceobj, 0);
					StopObject(t.tttsourceobj);
					SetObjectFrame(t.tobj, 0);
					StopObject(t.tobj);

					//LB: also need to trigger Wicked to update the frame at least once
					sObject* pParentObj = GetObjectData(t.tttsourceobj);
					sObject* pInstanceObj = GetObjectData(t.tobj);
					WickedCall_SetObjectFrame(pParentObj, 0);
					WickedCall_SetObjectFrame(pInstanceObj, 0);
				}
				if (t.entityprofile[t.entid].startanimingame > 0) 
				{
					if (t.entityprofile[t.entid].animmax > 0) 
					{
						t.q = 0;
						SetObjectFrame(t.tttsourceobj, 0);
						StopObject(t.tttsourceobj);
						SetObjectFrame(t.tobj, 0);
						StopObject(t.tobj);
					}
				}
			}
		}

		//PE: pframe is lost on clone objects, recreate.
		if (t.entityprofile[t.entid].ismarker == 0 && t.entityprofile[t.entid].isebe == 0)
		{
			if (t.entityelement[t.tte].isclone == 1 && t.entityelement[t.tte].underground == 0)
			{
				if (t.entityelement[t.tte].editorlock == 0)
				{
					entity_converttoinstance();
				}
			}
		}

		if (t.tobj > 0 && t.entityprofile[t.entid].ischaracter == 1) 
		{
			//Restore any character animations for editor.
			if (GetNumberOfFrames(t.tobj) > 0)
			{
				SetObjectFrame(t.tobj, 0);
				if (t.entityprofile[t.entid].animmax > 0 && t.entityprofile[t.entid].playanimineditor > 0 && t.entityprofile[t.entid].ischaractercreator == 0)
				{
					t.q = t.entityprofile[t.entid].playanimineditor - 1;
					LoopObject(t.tobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
				}
				else if (t.entityprofile[t.entid].playanimineditor < 0)
				{
					// uses name instead of index, the negative is the ordinal into the animset
					extern void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname);
					entity_loop_using_negative_playanimineditor(t.tte, t.tobj, t.entityprofile[t.entid].playanimineditor_name);
				}
				else
				{
					LoopObject(t.tobj); StopObject(t.tobj);
				}
			}
		}
		else 
		{
			//PE: Restore any non char animations.
			if (t.tobj > 0 && GetNumberOfFrames(t.tobj) > 0)
			{
				SetObjectFrame(t.tobj, 0);
				if (t.entityprofile[t.entid].animmax > 0 && t.entityprofile[t.entid].playanimineditor > 0 && t.entityprofile[t.entid].ischaractercreator == 0)
				{
					t.q = t.entityprofile[t.entid].playanimineditor - 1;
					LoopObject(t.tobj, t.entityanim[t.entid][t.q].start, t.entityanim[t.entid][t.q].finish);
				}
				else if (t.entityprofile[t.entid].playanimineditor < 0)
				{
					// uses name instead of index, the negative is the ordinal into the animset
					extern void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname);
					entity_loop_using_negative_playanimineditor(t.tte, t.tobj, t.entityprofile[t.entid].playanimineditor_name);
				}
				else
				{
					LoopObject(t.tobj); StopObject(t.tobj);
				}
			}
		}

		// ensure any pivot influences are also restored
		if (t.tobj > 0)
		{
			sObject* pObject = g_ObjectList[t.tobj];
			if (pObject)
			{
				if (t.entityprofile[t.entid].fixnewy != 0)
				{
					RotateObject (t.tobj, 0, t.entityprofile[t.entid].fixnewy, 0);
					FixObjectPivot (t.tobj);
					CalculateObjectWorld (pObject, NULL);
					WickedCall_UpdateObject(pObject);
				}
			}
		}
	}

	// signal that we have finished Test Level, restore mapeditor windows
	 OpenFileMap (  1, "FPSEXCHANGE" );
	 SetFileMapDWORD (  1, 970, 1 );
	 SetEventAndWait (  1 );

	// Restore camera
	editor_restoreeditcamera ( );
	t.updatezoom=1;

	// restore object visibilities
	editor_refresheditmarkers ( );

	// 130320 - ensure water height change is not saved out to root (messes up when reload software; underwater)
	float fStoreWaterLevel = g.gdefaultwaterheight;
	g.gdefaultwaterheight = -500.0f; //GGORIGIN_Y;

	// LUA may have changed fog, restore it
	t.visuals.FogNearest_f = t.gamevisuals.FogNearest_f;
	t.visuals.FogDistance_f = t.gamevisuals.FogDistance_f;
	t.visuals.FogR_f = t.gamevisuals.FogR_f;
	t.visuals.FogG_f = t.gamevisuals.FogG_f;
	t.visuals.FogB_f = t.gamevisuals.FogB_f;
	t.visuals.FogA_f = t.gamevisuals.FogA_f;

	// and sun, ambience and exposure: SetSunIntensity, SetSunLightingColor, SetSunDirection, SetExposure and the
	// ambience commands write only t.visuals, while the editor and Tab Tab controls write both, so a day/night script's
	// last values are dropped here and the user's own changes stay
	t.visuals.SunIntensity_f = t.gamevisuals.SunIntensity_f;
	t.visuals.SunRed_f = t.gamevisuals.SunRed_f;
	t.visuals.SunGreen_f = t.gamevisuals.SunGreen_f;
	t.visuals.SunBlue_f = t.gamevisuals.SunBlue_f;
	t.visuals.SunAngleX = t.gamevisuals.SunAngleX;
	t.visuals.SunAngleY = t.gamevisuals.SunAngleY;
	t.visuals.SunAngleZ = t.gamevisuals.SunAngleZ;
	t.visuals.AmbienceIntensity_f = t.gamevisuals.AmbienceIntensity_f;
	t.visuals.AmbienceRed_f = t.gamevisuals.AmbienceRed_f;
	t.visuals.AmbienceGreen_f = t.gamevisuals.AmbienceGreen_f;
	t.visuals.AmbienceBlue_f = t.gamevisuals.AmbienceBlue_f;
	t.visuals.fExposure = t.gamevisuals.fExposure;

	// and the sky: SetSkyTo writes only t.visuals, which the copy below would carry into the editor and its next save, so
	// put the editor's back unless a sky was picked in Tab Tab after the script's
	extern int g_iLuaSkyEditorIndex, g_iLuaSkySetIndex;
	extern cstr g_sLuaSkyEditorName;
	bool bRestoredEditorSky = false;
	if ( g_iLuaSkyEditorIndex != -1 )
	{
		if ( t.visuals.skyindex == g_iLuaSkySetIndex )
		{
			t.visuals.skyindex = g_iLuaSkyEditorIndex;
			t.visuals.sky_s = g_sLuaSkyEditorName;
			bRestoredEditorSky = true;
		}
		g_iLuaSkyEditorIndex = -1;
		g_iLuaSkySetIndex = -1;
	}

	// remember game states for next time
	visuals_save ( );

	t.gamevisuals=t.visuals;

	// and restore as would otherwise interfere with ?
	g.gdefaultwaterheight = fStoreWaterLevel;

	// restore shader constants with editor visuals (and bring back some settings we want to retain)
	t.visuals=t.editorvisuals;
	t.visuals.skyindex=t.gamevisuals.skyindex;
	t.visuals.sky_s=t.gamevisuals.sky_s;
	t.visuals.terrainindex=t.gamevisuals.terrainindex;
	t.visuals.terrain_s=t.gamevisuals.terrain_s;
	t.visuals.vegetationindex=t.gamevisuals.vegetationindex;
	t.visuals.vegetation_s=t.gamevisuals.vegetation_s;
	t.visuals.iEnvironmentWeather = t.gamevisuals.iEnvironmentWeather;

	t.visuals.AmbienceRed_f = t.gamevisuals.AmbienceRed_f;
	t.visuals.AmbienceGreen_f = t.gamevisuals.AmbienceGreen_f;
	t.visuals.AmbienceBlue_f = t.gamevisuals.AmbienceBlue_f;

	t.visuals.FogR_f = t.gamevisuals.FogR_f;
	t.visuals.FogG_f = t.gamevisuals.FogG_f;
	t.visuals.FogB_f = t.gamevisuals.FogB_f;
	t.visuals.FogA_f = t.gamevisuals.FogA_f;
	t.visuals.FogNearest_f = t.gamevisuals.FogNearest_f;
	t.visuals.FogDistance_f = t.gamevisuals.FogDistance_f;

	t.visuals.SkyIntensity_f = t.gamevisuals.SkyIntensity_f;
	t.visuals.SunIntensity_f = t.gamevisuals.SunIntensity_f;
	t.visuals.SunRed_f = t.gamevisuals.SunRed_f;
	t.visuals.SunGreen_f = t.gamevisuals.SunGreen_f;
	t.visuals.SunBlue_f = t.gamevisuals.SunBlue_f;
	t.visuals.ZenithRed_f = t.gamevisuals.ZenithRed_f;
	t.visuals.ZenithGreen_f = t.gamevisuals.ZenithGreen_f;
	t.visuals.ZenithBlue_f = t.gamevisuals.ZenithBlue_f;
	t.visuals.fSunShadowBias = t.gamevisuals.fSunShadowBias;
	t.visuals.bColorGrading = t.gamevisuals.bColorGrading;
	t.visuals.ColorGradingLUT = t.gamevisuals.ColorGradingLUT;
	t.visuals.bBloomEnabled = t.gamevisuals.bBloomEnabled;
	t.visuals.bLevelVSyncEnabled = t.gamevisuals.bLevelVSyncEnabled;
	t.visuals.bOcclusionCulling = t.gamevisuals.bOcclusionCulling;

	t.visuals.fEnvProbeBrightness = t.gamevisuals.fEnvProbeBrightness;

	t.visuals.bEnableTerrainChunkCulling = t.gamevisuals.bEnableTerrainChunkCulling;
	t.visuals.bEnablePointShadowCulling = t.gamevisuals.bEnablePointShadowCulling;
	t.visuals.bEnableSpotShadowCulling = t.gamevisuals.bEnableSpotShadowCulling;
	t.visuals.bEnableObjectCulling = t.gamevisuals.bEnableObjectCulling;
	t.visuals.bEnableAnimationCulling = t.gamevisuals.bEnableAnimationCulling;
	t.visuals.fLODMultiplier = t.gamevisuals.fLODMultiplier;

	t.visuals.bEnable30FpsAnimations = t.gamevisuals.bEnable30FpsAnimations;

	t.visuals.bShadowsLowestLOD = t.gamevisuals.bShadowsLowestLOD;
	t.visuals.bProbesLowestLOD = t.gamevisuals.bProbesLowestLOD;
	t.visuals.bRaycastLowestLOD = t.gamevisuals.bRaycastLowestLOD;
	t.visuals.bPhysicsLowestLOD = t.gamevisuals.bPhysicsLowestLOD;
	t.visuals.bThreadedPhysics = t.gamevisuals.bThreadedPhysics;
	t.visuals.bPhysicsWorldAt60 = t.gamevisuals.bPhysicsWorldAt60;
	// and the physics world's rate back from the level's setting, after a script's SetPhysicsWorldRate
	extern bool g_bPhysicsWorldAt60; g_bPhysicsWorldAt60 = t.visuals.bPhysicsWorldAt60;
	// and a script's light reach and particle forward scattering back to off (SetLightMaxDistance, SetParticleScatter); not in
	// Wicked_Update_Visibles, which a visuals push and HideTerrain/ShowTerrain also run in a game
	wi::renderer::gg_light_max_distance = 0.0f;
	wi::renderer::SetParticleScatter(0.0f);
	t.visuals.bReflectionsLowestLOD = t.gamevisuals.bReflectionsLowestLOD;

	
	t.visuals.g_bDelayedShadows = t.gamevisuals.g_bDelayedShadows;
	t.visuals.g_bDelayedShadowsLaptop = t.gamevisuals.g_bDelayedShadowsLaptop;


	t.visuals.fsetBloomThreshold = t.gamevisuals.fsetBloomThreshold;
	t.visuals.ApparentSize = t.gamevisuals.ApparentSize;
	t.visuals.bSSREnabled = t.gamevisuals.bSSREnabled;
	t.visuals.bReflectionsEnabled = t.gamevisuals.bReflectionsEnabled;
	t.visuals.bFXAAEnabled = t.gamevisuals.bFXAAEnabled;

	t.visuals.bDOF = t.gamevisuals.bDOF;
	t.visuals.fDOFStrength = t.gamevisuals.fDOFStrength;
	t.visuals.fDOFApertureSize = t.gamevisuals.fDOFApertureSize;
	t.visuals.fDOFFocalLength = t.gamevisuals.fDOFFocalLength;


	t.visuals.bTessellation = t.gamevisuals.bTessellation;
	t.visuals.bLightShafts = t.gamevisuals.bLightShafts;
	t.visuals.bLensFlare = t.gamevisuals.bLensFlare;
	t.visuals.fsetBloomRadius = t.gamevisuals.fsetBloomRadius;
	t.visuals.fsetBloomCap = t.gamevisuals.fsetBloomCap;
	t.visuals.bAutoExposure = t.gamevisuals.bAutoExposure;
	t.visuals.fAutoExposureRate = t.gamevisuals.fAutoExposureRate;
	t.visuals.fAutoExposureKey = t.gamevisuals.fAutoExposureKey;
	t.visuals.fExposure = t.gamevisuals.fExposure;
	t.visuals.fGamma = t.gamevisuals.fGamma;
	t.visuals.fDeSaturate = t.gamevisuals.fDeSaturate;

	t.visuals.SkyCloudiness = t.gamevisuals.SkyCloudiness;
	t.visuals.SkyCloudCoverage = t.gamevisuals.SkyCloudCoverage;
	t.visuals.SkyCloudHeight = t.gamevisuals.SkyCloudHeight;
	t.visuals.SkyCloudThickness = t.gamevisuals.SkyCloudThickness;
	t.visuals.SkyCloudSpeed = t.gamevisuals.SkyCloudSpeed;

	t.visuals.iMSAASampleCount = t.gamevisuals.iMSAASampleCount;
	t.visuals.iFSRMode = t.gamevisuals.iFSRMode;
	t.visuals.fFSRSharpness = t.gamevisuals.fFSRSharpness;

	t.visuals.iMSAO = t.gamevisuals.iMSAO;
	t.visuals.fMSAOPower = t.gamevisuals.fMSAOPower;

	//PE: restore SetGlobalGraphicsSettings here. unless changed with tab tab, then they are changed in gamevisuals
	//PE: Postprocess already restored.
	t.visuals.iShadowSpotCascadeResolution = t.gamevisuals.iShadowSpotCascadeResolution;
	t.visuals.iShadowSpotResolution = t.gamevisuals.iShadowSpotResolution;
	t.visuals.iShadowPointResolution = t.gamevisuals.iShadowPointResolution;
	t.visuals.iShadowPointMax = t.gamevisuals.iShadowPointMax;
	t.visuals.iShadowSpotMax = t.gamevisuals.iShadowSpotMax;
	t.visuals.bTransparentShadows = t.gamevisuals.bTransparentShadows;

	t.visuals.fTreeShadowLODDist = t.gamevisuals.fTreeShadowLODDist;
	t.visuals.iTreeShadowRange = t.gamevisuals.iTreeShadowRange;
	GGTrees::ggtrees_global_params.lod_dist_shadow = t.visuals.fTreeShadowLODDist;
	GGTrees::ggtrees_global_params.tree_shadow_range = t.visuals.iTreeShadowRange;

	t.visuals.iEnvProbeResolution = t.gamevisuals.iEnvProbeResolution;
	t.visuals.newperformancepresets = t.gamevisuals.newperformancepresets;

	t.visuals.fShadowFarPlane = t.gamevisuals.fShadowFarPlane;

	t.visuals.bWaterEnable = t.gamevisuals.bWaterEnable;
	t.visuals.fWaterWaveAmplitude = t.gamevisuals.fWaterWaveAmplitude;
	t.visuals.fWaterPatchLength = t.gamevisuals.fWaterPatchLength;
	t.visuals.fWaterCausticSize = t.gamevisuals.fWaterCausticSize;
	t.visuals.fWaterChoppyScale = t.gamevisuals.fWaterChoppyScale;
	t.visuals.fWaterWindDependency = t.gamevisuals.fWaterWindDependency;

	t.visuals.WaterFogMinDist = t.gamevisuals.WaterFogMinDist;
	t.visuals.WaterFogMaxDist = t.gamevisuals.WaterFogMaxDist;
	t.visuals.WaterFogMinAmount = t.gamevisuals.WaterFogMinAmount;
	
	//PE: Water color was missing.
	t.visuals.WaterRed_f = t.gamevisuals.WaterRed_f;
	t.visuals.WaterGreen_f = t.gamevisuals.WaterGreen_f;
	t.visuals.WaterBlue_f = t.gamevisuals.WaterBlue_f;
	t.visuals.WaterAlpha_f = t.gamevisuals.WaterAlpha_f;
	t.visuals.fUnderwaterColorR = t.gamevisuals.fUnderwaterColorR;
	t.visuals.fUnderwaterColorG = t.gamevisuals.fUnderwaterColorG;
	t.visuals.fUnderwaterColorB = t.gamevisuals.fUnderwaterColorB;
	t.visuals.fUnderwaterFog = t.gamevisuals.fUnderwaterFog;

	t.visuals.iTimeOfday = t.gamevisuals.iTimeOfday;
	t.visuals.SunAngleX = t.gamevisuals.SunAngleX;
	t.visuals.SunAngleY = t.gamevisuals.SunAngleY;
	t.visuals.SunAngleZ = t.gamevisuals.SunAngleZ;

	t.visuals.bSimulate24Hours = t.gamevisuals.bSimulate24Hours;
	t.visuals.fTimeSpeed = t.gamevisuals.fTimeSpeed;

	t.visuals.fWeatherIntensity = t.gamevisuals.fWeatherIntensity;
	t.visuals.fWeatherLighting = t.gamevisuals.fWeatherLighting;
	t.visuals.fWeatherThunder = t.gamevisuals.fWeatherThunder;
	t.visuals.fWeatherWind = t.gamevisuals.fWeatherWind;
	
	t.visuals.bPPSnow = t.gamevisuals.bPPSnow;
	t.visuals.voxel_steps = t.gamevisuals.voxel_steps;
	t.visuals.pp_size = t.gamevisuals.pp_size;
	t.visuals.pp_alpha = t.gamevisuals.pp_alpha;
	t.visuals.wind_direction_x = t.gamevisuals.wind_direction_x;
	t.visuals.wind_direction_y = t.gamevisuals.wind_direction_y;
	t.visuals.wind_direction_z = t.gamevisuals.wind_direction_z;
	t.visuals.wind_speed = t.gamevisuals.wind_speed;
	t.visuals.wind_randomness = t.gamevisuals.wind_randomness;
	t.visuals.tree_wind = t.gamevisuals.tree_wind;
	t.visuals.tree_sss = t.gamevisuals.tree_sss;


	t.visuals.fLevelDifficulty = t.gamevisuals.fLevelDifficulty;
	
	gggrass_global_params = gggrass_save_params;
	// GGMAX 3.96: and put terrain and trees back, so the editor returns to the level's OWN
	// settings and a later File>Save writes what the author had rather than the Test Game preset.
	// This is also what restores ggtrees lod_dist 1000 -> 3000, i.e. the 35% of triangles that
	// went missing from every level loaded after a test game (notes 3.86).
	{
		GGTerrain::ggterrain_global_params = ggterrain_save_params;
		GGTerrain::ggterrain_global_render_params = ggterrain_save_render_params;
		GGTerrain::ggterrain_global_render_params2 = ggterrain_save_render_params2;
		GGTrees::ggtrees_global_params = ggtrees_save_params;
	}

	for (int iL = 0; iL < 32; iL++) 
	{
		t.visuals.sTerrainTextures[iL] = t.gamevisuals.sTerrainTextures[iL];
		t.visuals.sTerrainTexturesName[iL] = t.gamevisuals.sTerrainTexturesName[iL];
	}
	
	for (int iL = 0; iL < 128; iL++) 
	{
		t.visuals.sGrassTextures[iL] = t.gamevisuals.sGrassTextures[iL];
		t.visuals.sGrassTexturesName[iL] = t.gamevisuals.sGrassTexturesName[iL];
		t.visuals.sFactionName[iL] = t.gamevisuals.sFactionName[iL];
	}

	t.visuals.bEndableAmbientMusicTrack = t.gamevisuals.bEndableAmbientMusicTrack;
	t.visuals.sAmbientMusicTrack = t.gamevisuals.sAmbientMusicTrack;
	t.visuals.iAmbientMusicTrackVolume = t.gamevisuals.iAmbientMusicTrackVolume;
	t.visuals.bEnableCombatMusicTrack = t.gamevisuals.bEnableCombatMusicTrack;
	t.visuals.sCombatMusicTrack = t.gamevisuals.sCombatMusicTrack;
	t.visuals.iCombatMusicTrackVolume = t.gamevisuals.iCombatMusicTrackVolume;

	t.visuals.bEndableTreeDrawing = t.gamevisuals.bEndableTreeDrawing;
	t.visuals.bEndableGrassDrawing = t.gamevisuals.bEndableGrassDrawing;
	t.visuals.bEndableTerrainDrawing = t.gamevisuals.bEndableTerrainDrawing;
	t.visuals.bEnableEmptyLevelMode = t.gamevisuals.bEnableEmptyLevelMode;
	t.visuals.bEnableZeroNavMeshMode = t.gamevisuals.bEnableZeroNavMeshMode;

	t.visuals.iHeightmapWidth = t.gamevisuals.iHeightmapWidth;
	t.visuals.iHeightmapHeight = t.gamevisuals.iHeightmapHeight;

	t.visuals.bDisableSkybox = t.gamevisuals.bDisableSkybox;

	t.visuals.bRainEnabled = t.gamevisuals.bRainEnabled;
	t.visuals.fRainSpeedX = t.gamevisuals.fRainSpeedX;
	t.visuals.fRainSpeedY = t.gamevisuals.fRainSpeedY;
	t.visuals.fRainOpacity = t.gamevisuals.fRainOpacity;
	t.visuals.fRainScaleX = t.gamevisuals.fRainScaleX;
	t.visuals.fRainScaleY = t.gamevisuals.fRainScaleY;
	t.visuals.fRainRefreactionScale = t.gamevisuals.fRainRefreactionScale;


	t.visuals.bSnowEnabled = t.gamevisuals.bSnowEnabled;
	t.visuals.fSnowLayers = t.gamevisuals.fSnowLayers;
	t.visuals.fSnowDepth = t.gamevisuals.fSnowDepth;
	t.visuals.fSnowWind = t.gamevisuals.fSnowWind;
	t.visuals.fSnowSpeed = t.gamevisuals.fSnowSpeed;
	t.visuals.fSnowOpacity = t.gamevisuals.fSnowOpacity;
	t.visuals.fSnowOffset = t.gamevisuals.fSnowOffset;

	// the editor's sky came back from a SetSkyTo: load its images, keeping the editor's sun and other visuals
	if ( bRestoredEditorSky )
	{
		g.skyindex = t.visuals.skyindex;
		t.terrainskyspecinitmode = 0;
		sky_skyspec_init ( false );
	}

	// and refresh assets based on restore
	t.visuals.refreshshaders=1;
	visuals_loop ( );
	visuals_shaderlevels_update ( );

	// use infinilights to show dynamic lighting in editor
	lighting_init ( );

	// Second call will toggle keyboard/mouse back to FOREGROUND
	SetWindowModeOn ( );

	//PE: Need to restore original settings.
	//PE: Setup the window here. pos size. Docking ?
	SetWindowSettings(5, 1, 1);
	SetForegroundWindow(g_pGlob->hWnd);
	SetWindowSize(gWindowSizeXOld+ gWindowSizeAddX, gWindowSizeYOld+ gWindowSizeAddY); //PE: test
	SetWindowPosition(gWindowPosXOld, gWindowPosYOld);
	ShowWindow();
	if (gWindowMaximized == 1 )
		MaximiseWindow();
	else
		RestoreWindow();

	//PE: enable outside windows again.
	ImGui::ShowAllViewPortWindows();

	// Close popup message
	if ( t.conkit.modified == 1 ) 
	{
		SleepNow ( 1000 );
		popup_text_close();
		t.conkit.modified=0;
	}

	// Ensure no terrain/entity editing carried back
	t.terrain.terrainpainteroneshot=0;

	// Set editor to use a true 1;1 pixel mapping for Text ( , Steam GUI and other overlay images )
	SetChildWindowTruePixel ( 1 );

	// restore if project modified
	t.tignoreinvalidateobstacles=1;
	g.projectmodified = t.storeprojectmodified;
	t.tignoreinvalidateobstacles=0;

	// Something is clipping objects when returning to editor
	editor_loadcfg();
	editor_refreshcamerarange();
}

void editor_previewmapormultiplayer(int iUseVRTest)
{
	// for non-MAX scenarios (single function call)
	bool bRunLoop = true;
	editor_previewmapormultiplayer_initcode(iUseVRTest);
	while ( bRunLoop == true)
	{
		if (editor_previewmapormultiplayer_loopcode(iUseVRTest) == true) bRunLoop = false;
	}
	editor_previewmapormultiplayer_afterloopcode(iUseVRTest);
	t.postprocessings.fadeinvalue_f = 1.0f;
}

void editor_multiplayermode ( void )
{
	// check we are not in the importer or character creator
	editor_checkIfInSubApp ( );

	//  Record last edited project
	t.storeprojectfilename_s=g.projectfilename_s;

	//  Set multiplayer flags here
	t.game.runasmultiplayer=1;
	editor_previewmapormultiplayer ( 1 );

	// PE: I cant restore editor after multiplayer mode ? SO:
	// call a new map editor
	OpenFileMap(2, "FPSEXCHANGE");
	SetFileMapString(2, 1000, "Guru-MapEditor.exe");
	SetFileMapString(2, 1256, "-r");
	SetFileMapDWORD(2, 994, 0);
	SetFileMapDWORD(2, 924, 1);
	SetEventAndWait(2);
	// Terminate fragmented EXE
	common_justbeforeend();
	ExitProcess(0);

	// As multiplayer can load OTHER things, restore level to state before we clicked MM button
	t.tfile_s=g.mysystem.editorsGridedit_s+"cfg.cfg";//"editors\\gridedit\\cfg.cfg";
	if ( FileExist(t.tfile_s.Get()) == 1 ) 
	{
		timestampactivity(0,"reloading your level after MM button");
		t.skipfpmloading=0;
		g.projectfilename_s=g.mysystem.editorsGrideditAbs_s+"worklevel.fpm";//g.fpscrootdir_s+"\\Files\\editors\\gridedit\\worklevel.fpm";
		editor_loadcfg ( );
		gridedit_load_map ( );
		bUpdateVeg = true;

		//  added to solve fog issue when go in and out of MP menu
		visuals_editordefaults ( );
		t.visuals.refreshshaders=1;
	}

	//  restore last edited project
	g.projectfilename_s=t.storeprojectfilename_s;
	gridedit_updateprojectname ( );

	//editor_restoreeditcamera();
	//SyncMaskOverride(0xFFFFFFFF);

}

