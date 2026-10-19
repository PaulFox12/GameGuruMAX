void physics_player_gatherkeycontrols ( void )
{
	// Camera in control of player one
	t.plrid=1;

	// Key configuration
	t.plrkeyW=17;
	t.plrkeyA=30;
	t.plrkeyS=31;
	t.plrkeyD=32;
	t.plrkeyQ=16;
	t.plrkeyE=18;
	t.plrkeyF=33;
	t.plrkeyC=46;
	t.plrkeyZ=44;
	t.plrkeyR=19;
	t.plrkeySPACE=57;
	t.plrkeyRETURN=28;
	t.plrkeySHIFT=42;
	t.plrkeySHIFT2=54;
	t.plrkeyF12=88;
	t.plrkeyJ=36;

	// used to override physical key pressed
	static int g_lastplrkeyForceKeystate = -1;
	if (t.plrkeyForceKeystate != g_lastplrkeyForceKeystate)
	{
		g_lastplrkeyForceKeystate = t.plrkeyForceKeystate;
		extern int ForceKeyStateValue(int);
		ForceKeyStateValue(t.plrkeyForceKeystate);
	}

	// from SETUP.INI config keys
	if ( t.listkey[1]>0  )  t.plrkeyW = t.listkey[1];
	if ( t.listkey[2]>0  )  t.plrkeyS = t.listkey[2];
	if ( t.listkey[3]>0  )  t.plrkeyA = t.listkey[3];
	if ( t.listkey[4]>0  )  t.plrkeyD = t.listkey[4];
	if ( t.listkey[5]>0  )  t.plrkeySPACE = t.listkey[5];
	if ( t.listkey[6]>0  )  t.plrkeyC = t.listkey[6];
	if ( t.listkey[7]>0  )  t.plrkeyRETURN = t.listkey[7];
	if ( t.listkey[8]>0  )  t.plrkeyR = t.listkey[8];
	if ( t.listkey[9]>0  )  t.plrkeyQ = t.listkey[9];
	if ( t.listkey[10]>0  )  t.plrkeyE = t.listkey[10];
	if ( t.listkey[11]>0  )  t.plrkeySHIFT = t.listkey[11];

	// Read keys from config, and use in player control actions
	if ( g.walkonkeys == 1 ) 
	{
		if ( KeyState(g.keymap[t.plrkeyW]) ==1 ) { t.plrkeyW=1 ; t.plrkeySLOWMOTION=0 ;} else t.plrkeyW=0;
		if ( KeyState(g.keymap[t.plrkeyA]) ==1 ) { t.plrkeyA=1 ; t.plrkeySLOWMOTION=0 ;} else t.plrkeyA=0;
		if ( KeyState(g.keymap[t.plrkeyS]) ==1 ) { t.plrkeyS=1 ; t.plrkeySLOWMOTION=0 ;} else t.plrkeyS=0;
		if ( KeyState(g.keymap[t.plrkeyD]) ==1 ) { t.plrkeyD=1 ; t.plrkeySLOWMOTION=0 ;} else t.plrkeyD=0;
	}
	if ( g.arrowkeyson == 1 ) 
	{
		t.tplrkeySLOWMOTIONold=t.plrkeySLOWMOTION;
		if ( UpKey() == 1 ) { t.plrkeyW = 1  ; t.plrkeySLOWMOTION = 1; }
		if ( LeftKey() == 1 ) { t.plrkeyA = 1  ; t.plrkeySLOWMOTION = 1; }
		if ( DownKey() == 1 ) { t.plrkeyS = 1  ; t.plrkeySLOWMOTION = 1; }
		if ( RightKey() == 1 ) { t.plrkeyD = 1  ; t.plrkeySLOWMOTION = 1; }
		if ( t.tplrkeySLOWMOTIONold != t.plrkeySLOWMOTION ) 
		{
			t.null=MouseMoveX() ; t.null=MouseMoveY();
			t.cammousemovex_f=0 ; t.cammousemovey_f=0;
			t.tFinalCamX_f=ObjectPositionX(t.aisystem.objectstartindex);
			t.tFinalCamY_f=ObjectPositionY(t.aisystem.objectstartindex);
			t.tFinalCamZ_f=ObjectPositionZ(t.aisystem.objectstartindex);
		}
	}
	if ( KeyState(g.keymap[t.plrkeySHIFT]) == 1 && g.runkeys == 1 && t.jumpaction == 0  )  t.plrkeySHIFT = 1; else t.plrkeySHIFT = 0;
	if ( KeyState(g.keymap[t.plrkeySHIFT2]) == 1 && g.runkeys == 1 && t.jumpaction == 0  )  t.plrkeySHIFT2 = 1; else t.plrkeySHIFT2 = 0;

	// when in vr mode
	if ( g.vrglobals.GGVREnabled > 0 && g.vrglobals.GGVRUsingVRSystem == 1 )
	{
		// lee - 040619 - detect trigger and use as t.plrkeyE
		if ( GGVR_RightController_Trigger() > 0.5 || GGVR_LeftController_Trigger() > 0.5 )
			t.plrkeyE = 1;

		// lee - 040619 - detect grip button and use as run
		if ( GGVR_RightController_Grip() == 1 || GGVR_LeftController_Grip() == 1 )
			t.plrkeySHIFT = 1;

		// Using PGUP/PGDN to control gvrsitteradjust that modifies the VR sitting position for the eye of the player/user
		int iVRSitterAdjustMode = 0;
		if (KeyState(201)) iVRSitterAdjustMode = 1;
		if (KeyState(209)) iVRSitterAdjustMode = 2;
		if (iVRSitterAdjustMode > 0)
		{
			// make thr adjustment
			if (iVRSitterAdjustMode == 1) g.gvrsitteradjust += 1;
			if (iVRSitterAdjustMode == 2) g.gvrsitteradjust -= 1;

			// and save the setting for next time
			FPSC_SaveSETUPVRINI();
		}
	}

	if ( t.conkit.editmodeactive != 0 ) 
	{
		// FPS 3D Editing Mode - keys elsewhere
	}
	else
	{
		// FPS Gaming Mode
		if ( KeyState(g.keymap[t.plrkeySPACE]) == 1 && g.jumponkey == 1  )  t.plrkeySPACE = 1; else t.plrkeySPACE = 0;
		if ( KeyState(g.keymap[t.plrkeyQ]) == 1 && g.peekonkeys == 1  )  t.plrkeyQ = 1; else t.plrkeyQ = 0;
		if ( KeyState(g.keymap[t.plrkeyE]) == 1 && g.peekonkeys == 1  )  t.plrkeyE = 1; else t.plrkeyE = 0;
		if ( KeyState(g.keymap[t.plrkeyF]) == 1  )  t.plrkeyF = 1; else t.plrkeyF = 0;
		if ( KeyState(g.keymap[t.plrkeyC]) == 1 && g.crouchonkey == 1  )  t.plrkeyC = 1; else t.plrkeyC = 0;
		if ( ControlKey() == 1  )  t.plrkeyC = 1;
		if ( KeyState(g.keymap[t.plrkeyZ]) == 1  )  t.plrkeyZ = 1; else t.plrkeyZ = 0;
		if ( KeyState(g.keymap[t.plrkeyR]) == 1  )  t.plrkeyR = 1; else t.plrkeyR = 0;
		if ( KeyState(g.keymap[t.plrkeyRETURN]) == 1  )  t.plrkeyRETURN = 1; else t.plrkeyRETURN = 0;
		if ( KeyState(g.keymap[t.plrkeyJ]) == 1  )  t.plrkeyJ = 1; else t.plrkeyJ = 0;
	}

	// XBOX/Controller Keys
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking XBOX/Controller Keys");
	if ( g.gxbox == 1 ) 
	{
		if ( JoystickFireC() == 1 ) 
		{
			t.plrkeyR = 1;
		}
		if ( JoystickFireD() == 1 )  // also duplicated in LUA.cpp
		{
			t.plrkeyE = 1;
		}
	}
	if ( g.gxbox == 1 ) 
	{
		if ( g.walkonkeys == 1 ) 
		{
			if ( JoystickY()<-850  )  t.plrkeyW = 1;
			if ( JoystickY()>850  )  t.plrkeyS = 1;
			if ( JoystickX()<-850  )  t.plrkeyA = 1;
			if ( JoystickX()>850  )  t.plrkeyD = 1;
		}
		if ( JoystickFireA() == 1 && g.jumponkey == 1   )  t.plrkeySPACE = 1;
		if ( g.gxboxcontrollertype == 0 ) 
		{
			// XBOX360 Controller
			if ( JoystickFireXL(8) == 1 && g.crouchonkey == 1  )  t.plrkeyC = 1;
			if ( JoystickFireXL(9) == 1  )  t.plrkeyZ = 1;
		}
		if ( g.gxboxcontrollertype == 1 ) 
		{
			// Dual Action
			if ( JoystickFireXL(10) == 1 && g.crouchonkey == 1  )  t.plrkeyC = 1;
			if ( JoystickFireXL(11) == 1  )  t.plrkeyZ = 1;
			if ( JoystickFireXL(4) == 1 && g.runkeys == 1  )  t.plrkeySHIFT = 1;
			if ( JoystickFireXL(6) == 1 && g.runkeys == 1  )  t.plrkeySHIFT = 1;
		}
		if ( g.gxboxcontrollertype == 2 ) 
		{
			// Dual Action F310
			if ( JoystickFireXL(10) == 1 && g.crouchonkey == 1  )  t.plrkeyC = 1;
			if ( JoystickFireXL(11) == 1  )  t.plrkeyZ = 1;
			if ( JoystickFireXL(4) == 1 && g.runkeys == 1  )  t.plrkeySHIFT = 1;
			if ( JoystickFireXL(6) == 1 && g.runkeys == 1  )  t.plrkeySHIFT = 1;
		}
	}

	// VR Support - take extra input from VR controllers
	if ( g.vrglobals.GGVREnabled > 0 && g.vrglobals.GGVRUsingVRSystem == 1 )
	{
		// Intialize the player to the start position and rotation and setup the GGVR Player Object
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking VR controllers");
		if (g.vrglobals.GGVRInitialized == 0)
		{
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling GGVR_SetPlayerPosition");
			GGVR_SetPlayerPosition(t.tFinalCamX_f, BT_GetGroundHeight(t.terrain.TerrainID, t.tFinalCamX_f, t.tFinalCamZ_f), t.tFinalCamZ_f);
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling GGVR_SetPlayerRotation");

			// this sets the origin based on the current camera zero (ARG!)
			// should only set based on player angle (minus HMD influence) as HMD added later at right time for smooth headset viewing!
			GGVR_SetPlayerRotation(0, 0, 0);

			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling GGVR_UpdatePlayer(false,terrainID)");
			if ( g.gproducelogfiles == 2 ) 
			{
				char pWhatIsPath[1024];
				GetCurrentDirectoryA ( 1024, pWhatIsPath );
				timestampactivity(0,pWhatIsPath);
			}
			try
			{
				int iBatchStart = g.batchobjectoffset;
				int iBatchEnd = g.batchobjectoffset + g.merged_new_objects + 1;
				GGVR_UpdatePlayer(false,t.terrain.TerrainID,g.lightmappedobjectoffset,g.lightmappedobjectoffsetfinish,g.entityviewstartobj,g.entityviewendobj, iBatchStart, iBatchEnd);
			}
			catch(...)
			{
				timestampactivity(0,"try catch failed when calling GGVR_UpdatePlayer");
			}
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"getting GGVR_GetHMDOffsets");
			g.vrglobals.GGVR_Old_XposOffset = GGVR_GetHMDOffsetX();
			g.vrglobals.GGVR_Old_ZposOffset = GGVR_GetHMDOffsetZ();
			g.vrglobals.GGVR_Old_Yangle = GGVR_GetPlayerAngleY();
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"g.vrglobals.GGVRInitialized");
			g.vrglobals.GGVRInitialized = 1;
		}
		if (g.walkonkeys == 1)
		{
			if (GGVR_LeftController_JoyY() > 0.5) t.plrkeyW = 1;
			if (GGVR_LeftController_JoyY() < -0.5)  t.plrkeyS = 1;
		}
		g.vrglobals.GGVR_XposOffset = GGVR_GetHMDOffsetX();
		g.vrglobals.GGVR_ZposOffset = GGVR_GetHMDOffsetZ();
		g.vrglobals.GGVR_XposOffsetChange = g.vrglobals.GGVR_XposOffset - g.vrglobals.GGVR_Old_XposOffset;
		g.vrglobals.GGVR_ZposOffsetChange = g.vrglobals.GGVR_ZposOffset - g.vrglobals.GGVR_Old_ZposOffset;
		g.vrglobals.GGVR_Old_XposOffset = g.vrglobals.GGVR_XposOffset;
		g.vrglobals.GGVR_Old_ZposOffset = g.vrglobals.GGVR_ZposOffset;
	}

	// GGMAX 2.41: harness trigger-hold. FIRE_WEAPON sets this to N and the trigger is held for N
	// consecutive frames FROM INSIDE this function, which is the only place firingmode is written.
	// Setting g.playeraction from the harness was not enough on its own: the harness runs at a
	// different point in the frame, and one frame of firingmode=1 did not produce a shot (measured
	// -- ammo stayed 30/388 and the barrel survived across 14 captured frames). Applying it here,
	// held across frames, removes both the ordering question and the single-frame question.
	extern int g_ggFireHoldFrames;
	if (g_ggFireHoldFrames > 0)
	{
		t.player[1].state.firingmode = 1;
		g_ggFireHoldFrames--;
	}

	// Automated actions (script control)
	switch ( g.playeraction )
	{
		case 1 : t.player[1].state.firingmode = 1; break ;
		case 2 : t.gunzoommode = 1 ; break ;
		case 3 : t.player[1].state.firingmode = 2 ; break ;
		case 4 : g.forcecrouch = 1 ; break ;
		case 5 : t.plrkeySPACE = 1 ; break ;
		case 6 : t.plrkeyE = 1 ; break ;
		case 7 : t.plrkeyQ = 1 ; break ;
		case 8 : t.plrkeyRETURN = 1 ; break ;
		case 9 : t.tmouseclick = 1 ; break ;
		case 10 : t.tmouseclick = 2 ; break ;
		case 11 : 
		{
			// ensure weapon unjams affect both modes if sharing ammo
			g.firemodes[t.gunid][g.firemode].settings.jammed = 1; 
			if ( t.gun[t.gunid].settings.modessharemags == 1 ) 
			{
				g.firemodes[t.gunid][0].settings.jammed = 1;
				g.firemodes[t.gunid][1].settings.jammed = 1;
			}
		}
		break;
	}

	// Third person disables crouch/zoom/RMB
	t.playercontrol.camrightmousemode=0;
	if ( t.playercontrol.thirdperson.enabled == 1 ) 
	{
		g.forcecrouch=0 ; t.plrkeyC=0;
		t.gunzoommode = 0 ; if ( t.tmouseclick == 2 ) { t.tmouseclick = 0  ; t.playercontrol.camrightmousemode = 1; }
	}

	//  Free weapon jam if reload used (possible relocate these to gun module
	if ( t.player[1].state.firingmode == 2 ) //&& t.gunzoommode == 0 ) 
	{
		// unjam or reload animation to unjam weapon
		g.plrreloading=1;
		g.firemodes[t.gunid][g.firemode].settings.shotsfired=0;
		// play free jam animation if it exists
		if ( g.firemodes[t.gunid][g.firemode].action2.clearjam.s != 0 && g.firemodes[t.gunid][g.firemode].settings.jammed == 1 ) 
		{
			// come out of zoom if in it
			if ( t.gunzoommode >=8 ) t.gunzoommode = 11; // catches all states of a zoomed in state

			// play anim to fix jam
			g.plrreloading=2;
			g.custstart=g.firemodes[t.gunid][g.firemode].action2.clearjam.s;
			g.custend=g.firemodes[t.gunid][g.firemode].action2.clearjam.e;
			t.gunmode=9998;
		}
		g.firemodes[t.gunid][g.firemode].settings.shotsfired=0;

		// ensure weapon unjams affect both modes if sharing ammo
		g.firemodes[t.gunid][g.firemode].settings.jammed = 0;
		if ( t.gun[t.gunid].settings.modessharemags == 1 ) 
		{
			g.firemodes[t.gunid][0].settings.jammed = 0;
			g.firemodes[t.gunid][1].settings.jammed = 0;
		}
	}

	// Forced key controls (script control)
	if ( g.forcemove>0  )  t.plrkeyW = 1;
	if ( g.forcemove<0  )  t.plrkeyS = 1;
	if ( g.forcecrouch == 1 && g.playeraction != 4  )  g.forcecrouch = 0;
	if ( g.playeraction != 4  )  g.playeraction = 0;

	// interrogate IDE to see if we have input focus
	if ( t.game.gameisexe == 0 ) 
	{
		t.plrhasfocus=1;
		if ( t.plrfilemapaccess == 1 ) 
		{
			// if VR, disable this as WMR changes the focus window
			if ( g.vrglobals.GGVREnabled > 0 )
			{
				// window focus can be switched to HMD window
			}
			else
			{
				// normal behavior
				t.plrhasfocus=1;
				#if !defined(ENABLEIMGUI) || defined(USEOLDIDE)
				t.plrhasfocus=GetFileMapDWORD( 11, 148 );
				#endif
			}
		}
	}

	// If player no health (dead), cannot control anything
	if ( t.player[t.plrid].health <= 0 || t.plrhasfocus == 0 ) 
	{
		t.plrkeyW=0;
		t.plrkeyA=0;
		t.plrkeyS=0;
		t.plrkeyD=0;
		t.plrkeyQ=0;
		t.plrkeyE=0;
		t.plrkeyF=0;
		t.plrkeyC=0;
		t.plrkeyZ=0;
		t.plrkeyR=0;
		t.plrkeySPACE=0;
		t.plrkeyRETURN=0;
		t.plrkeySHIFT=0;
		t.plrkeySHIFT2=0;
		t.plrkeyJ=0;
		t.plrkeyForceKeystate = 0;
	}
}

void physics_no_gun_zoom ( void )
{
	g.realfov_f=t.visuals.CameraFOV_f;
	if ( g.realfov_f < 15 ) g.realfov_f = 15;
	SetCameraFOV ( g.realfov_f );
	SetCameraFOV ( 2, g.realfov_f );
}

void physics_getcorrectjumpframes ( int entid, float* fStartFrame, float* fHoldFrame, float* fResumeFrame, float* fFinishFrame )
{
	// use frames stored in VAULT animation
	t.q = t.entityprofile[entid].startofaianim + t.csi_stoodvault[1];
	*fStartFrame = t.entityanim[entid][t.q].start;
	*fFinishFrame = t.entityanim[entid][t.q].finish;

	// jump hold animation frames overridden in FPE
	if ( t.entityprofile[entid].jumphold > 0 )
		*fHoldFrame = t.entityprofile[entid].jumphold;
	else
		*fHoldFrame = t.entityanim[entid][t.q].finish - 10;

	// jump resume frame to indicate when can resume movement
	if ( t.entityprofile[entid].jumpresume > 0 )
		*fResumeFrame = t.entityprofile[entid].jumpresume;
	else
		*fResumeFrame = t.entityanim[entid][t.q].finish;
}

void physics_player_control_LUA ( void )
{
	// when LUA global ready, call LUA player control function
	if ( t.playercontrol.gameloopinitflag == 0 )
	{
		// F9 Edit Mode Controls internal
		if ( t.conkit.editmodeactive != 0 )
		{
		}
		else
		{
			// Feed in-game mappable keys
			LuaSetInt ( "g_PlrKeyW", t.plrkeyW );
			LuaSetInt ( "g_PlrKeyA", t.plrkeyA );
			LuaSetInt ( "g_PlrKeyS", t.plrkeyS );
			LuaSetInt ( "g_PlrKeyD", t.plrkeyD );
			LuaSetInt ( "g_PlrKeyQ", t.plrkeyQ );
			LuaSetInt ( "g_PlrKeyE", t.plrkeyE ); 
			LuaSetInt ( "g_PlrKeyF", t.plrkeyF );
			LuaSetInt ( "g_PlrKeyC", t.plrkeyC );
			LuaSetInt ( "g_PlrKeyZ", t.plrkeyZ );
			LuaSetInt ( "g_PlrKeyR", t.plrkeyR );
			LuaSetInt ( "g_PlrKeySPACE", t.plrkeySPACE );
			LuaSetInt ( "g_PlrKeyRETURN", t.plrkeyRETURN );
			LuaSetInt ( "g_PlrKeySHIFT", t.plrkeySHIFT );
			LuaSetInt ( "g_PlrKeySHIFT2", t.plrkeySHIFT2 );
			LuaSetInt ( "g_PlrKeyJ", t.plrkeyJ );

			// Call externaliszed script
			LuaSetFunction ( "PlayerControl", 0, 0 );
			LuaCall();
		}
	}
}

void physics_player_control ( void )
{
	// No player controls when in editing mode
	if ( t.plrhasfocus == 0 ) return;

	// Gather input data
	t.k_s=Lower(Inkey() );

	// Invincibe Mode - God Mode
	if ( g.ggodmodestate == 1 && t.k_s == "i" ) t.player[1].health = 99999;

	// Get MouseClick (except when in TAB TAB mode)
	// Mode 1 = ignore A, C, D buttons of controller
	if ( g.tabmode < 2 )
		t.tmouseclick = control_mouseclick_mode(1);
	else
		t.tmouseclick=0;

	// GGMAX 2.48: harness zoom-fire. ZOOM_FIRE holds the RIGHT MOUSE bit for N frames so the
	// Lua zoom state machine (gameplayercontrol.lua:361-410 reads g_MouseClickControl bit 2,
	// advances gunzoommode +1/frame to zoomed) runs its SHIPPED path — not a forced state.
	// Mid-hold it presses the LEFT MOUSE bit for a few frames, which fires through the SHIPPED
	// path too (gameplayercontrol.lua:199 bit 1 -> SetGamePlayerStateFiringMode(1) -> DarkLUA
	// sets gunmode 101). ⚠ Do NOT swap this for the 2.41 firingmode hold — the 2.42 tracer
	// proved that path alone does not fire; the real trigger is the Lua one.
	// Must live HERE: after t.tmouseclick is assigned for this frame, before Lua reads it.
	{
		extern int g_ggZoomHoldFrames, g_ggZoomFireAtFrame, g_ggZoomFireMouseFrames;
		if (g_ggZoomHoldFrames > 0)
		{
			t.tmouseclick |= 2;
			g_ggZoomHoldFrames--;
			if (g_ggZoomFireAtFrame > 0)
			{
				g_ggZoomFireAtFrame--;
				if (g_ggZoomFireAtFrame == 0) g_ggZoomFireMouseFrames = 4;
			}
		}
		if (g_ggZoomFireMouseFrames > 0) { t.tmouseclick |= 1; g_ggZoomFireMouseFrames--; }
	}

	// Set input data for LUA call
	LuaSetInt ( "g_KeyPressJ", t.plrkeyJ );
	LuaSetInt ( "g_MouseClickControl", t.tmouseclick );
	physics_player_control_LUA();

	// Apply colour to shader
	SetVector4 ( g.terrainvectorindex, t.playercontrol.redDeathFog_f, 0, 0, 0 );
	t.tColorVector = g.terrainvectorindex;
	postprocess_setscreencolor ( );
}

void physics_player_handledeath ( void )
{
	// handle player death
	if (  t.game.runasmultiplayer == 0 ) 
	{
		//  Handle player death - only for single player
		if (  t.playercontrol.deadtime>0 ) 
		{
			//  control sequence
			if (  t.playercontrol.thirdperson.enabled == 0 ) 
			{
				if (  CameraAngleZ(0)<45 ) 
				{
					if ( g.luacameraoverride != 2 && g.luacameraoverride != 3 )
					{
						ZRotateCamera (  0,CameraAngleZ(0)+5.0 );
					}
				}
			}
			//  when death pause over
			if (  t.aisystem.processplayerlogic == 1 ) 
			{
				if (  MAXTimer()>t.playercontrol.deadtime ) 
				{
					if (  t.playercontrol.startlives>0 && t.player[t.plrid].lives == 0 && t.game.gameisexe == 1 ) 
					{
						//  280415 - GAME OVER flag!
						t.game.gameloop=0 ; t.game.lostthegame=1;
					}
					else
					{
						//  move player to start
						physics_player_gotolastcheckpoint ( );
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
									SetSoundVolume(iFreeSoundID, 0); //PE: Controlled in lua t.visuals.iCombatMusicTrackVolume);
									extern std::unordered_map<int, float> luavolumes;
									luavolumes.insert_or_assign(iFreeSoundID, 0);
								}
							}
						}
					}
				}
			}
		}
	}
}

void physics_play_thump_sound (float fX, float fY, float fZ, float fStartFreq, float fFreqRange)
{
	int iThumpID = 1 + (rand() % 6);
	#define SIXTHUMPSOUNDSFORMELEE 6
	for (int iTryAll = 0; iTryAll < SIXTHUMPSOUNDSFORMELEE; iTryAll++)
	{
		if (SoundPlaying(g.meleethumpsoundoffset + iThumpID) == 0)
			break;
		iThumpID++;
		if (iThumpID > SIXTHUMPSOUNDSFORMELEE)
			iThumpID = 1;
	}
	int iThumpSound = g.meleethumpsoundoffset + iThumpID;
	if (SoundPlaying(iThumpSound) == 0)
	{
		if (iThumpSound > 0 && SoundExist(iThumpSound) == 1)
		{
			PositionSound (iThumpSound, fX, fY, fZ);
			setsoundspeedtunedfor44k (iThumpSound, fStartFreq + Rnd(fFreqRange));
			PlaySound (iThumpSound);
		}
	}
}

void physics_player_reset_underwaterstate ( void )
{
	visuals_underwater_off ( );
	t.playercontrol.inwaterstate = 0;
	t.playercontrol.drowntimestamp = 0;
}

void physics_player_listener ( void )
{
	ScaleListener (  5.0  ); RotateListener (  0,CameraAngleY(),0 );
	PositionListener ( CameraPositionX(), CameraPositionY(), CameraPositionZ() );
}

void physics_player_takedamage ( void )
{
	// Receives; tdamage, te, tDrownDamageFlag
	// Uses tDrownDamageFlag to avoid blood splats and other non drowning damage effects.
	// This is set to 0 after takedamage is called, so doesn't need to be unset elsewhere
	// before calling this sub
	float fOriginalDamage = t.tdamage;

	// Apply player shake, even if immune to damage
	if ((t.tdamage > 0 && t.player[t.plrid].health > 0) || t.tdamage > 65000)
	{
		float fMoreShakeIfMeleeBased = 1.0f;
		if (t.te > 0)
		{
			int gunid = t.entityelement[t.te].eleprof.hasweapon;
			if (gunid > 0)
			{
				if (t.gun[gunid].weapontype == 51) // melee weapon type is 51
				{
					fMoreShakeIfMeleeBased = 3.0f;
				}
			}
		}
		else
		{
			// if no entity inflicting, assume external or generic damage, so more shake!
			fMoreShakeIfMeleeBased = 2.5f;
		}
		t.playercontrol.camerashake_f = (t.tdamage / 100.0f) * 25.0f * fMoreShakeIfMeleeBased;
	}

	// a successful block with deflect any further damage event
	bool bSuccessfullyBlockingNow = false;
	if (t.player[1].state.blockingaction == 2)
	{
		float protectedstart = g.firemodes[t.gunid][g.firemode].blockaction.idle.s;
		float protectedend = g.firemodes[t.gunid][g.firemode].blockaction.idle.e;
		if (protectedstart == 0 && protectedstart == 0)
		{
			// when block loop (idle) missing, use entire block sequence as more generous block
			protectedstart = g.firemodes[t.gunid][g.firemode].blockaction.start.s;
			protectedend = g.firemodes[t.gunid][g.firemode].blockaction.finish.e;
		}
		else
		{
			// be a 'little' lenient when using BLOCK LOOP phase, allowing for the block frame to rest
			protectedstart -= 2; protectedend += 5;
			if (protectedend > g.firemodes[t.gunid][g.firemode].blockaction.finish.e)
			{
				// however do not allow it to go beyond end of block finish frame
				protectedend = g.firemodes[t.gunid][g.firemode].blockaction.finish.e;
			}
		}
		float fFrameNow = GetFrame(t.currentgunobj);
		if (fFrameNow >= protectedstart && fFrameNow <= protectedend)
		{
			// block has succeeded, player hurt within block range
			bSuccessfullyBlockingNow = true;

			// to give us an extra game cycle of bullet time!
			SetObjectSpeed (t.currentgunobj, 1);

			// if this entity hitteer was blocked, then it has opened itself up to being counter attacked :)
			if (t.te > 0)
			{
				extern int g_iCounterAttackTargetForPlayer;
				if (g_iCounterAttackTargetForPlayer == 0)
				{
					g_iCounterAttackTargetForPlayer = t.te;
				}
			}
		}
		else
		{
			// block has failed, player hurt outside of block range
			// this can only trigger if BLOCK LOOP is specified, ie the window of ACTUAL block!
			t.player[1].state.blockingaction = 4;
		}
	}

	// player cannot be damaged when immune!
	if ( t.huddamage.immunity>0  )  return;

	// quite early if in F9 editing mode
	if ( t.conkit.editmodeactive != 0  )  return;

	//  Apply player health damage
	if (  (t.tdamage>0 && t.player[t.plrid].health>0) || t.tdamage>65000 ) 
	{
		// aloow all player hit code to proceed (block/etc) but just deal no damage
		bool bImmunityFromActualDamage = false;
		if (t.player[t.plrid].health >= 99999)
		{
			bImmunityFromActualDamage = true;
			t.tdamage = 0;
		}

		//  Flag player damage in health regen code
		if (  t.playercontrol.regentime>0  )  t.playercontrol.regentime = MAXTimer();

		//  Deduct health from player
		if (t.playercontrol.startstrength > 0 && bSuccessfullyBlockingNow==false)
		{	
			// deduct player health
			LuaSetFunction ("PlayerHealthSubtract", 1, 0);
			LuaPushInt(t.tdamage);
			LuaCall();
			t.player[t.plrid].health = LuaGetInt("g_PlayerHealth");
		}

		// if NOT drowning, do usual damage stuff
		if ( t.tDrownDamageFlag == 0 ) 
		{
			// Instruct HUD about player damage
			if ( t.playercontrol.startviolent != 0 && g.quickparentalcontrolmode != 2 && bSuccessfullyBlockingNow==false )
			{
				if ( t.playercontrol.thirdperson.enabled == 1 ) 
				{
					// third person character produces blood
					if ( t.te != -1 ) 
					{
						t.ttobj1=t.entityelement[t.playercontrol.thirdperson.charactere].obj;
						t.decalx1=ObjectPositionX(t.ttobj1);
						t.decaly1=ObjectPositionY(t.ttobj1)+(ObjectSizeY(t.ttobj1)/2);
						t.decalz1=ObjectPositionZ(t.ttobj1);
						t.ttobj2=t.entityelement[t.te].obj;
						t.decalx2=ObjectPositionX(t.ttobj2);
						t.decaly2=ObjectPositionY(t.ttobj2)+(ObjectSizeY(t.ttobj2)/2);
						t.decalz2=ObjectPositionZ(t.ttobj2);
						t.ttdx_f=t.decalx1-t.decalx2;
						t.ttdy_f=t.decaly1-t.decaly2;
						t.ttdz_f=t.decalz1-t.decalz2;
						t.ttdd_f=Sqrt(abs(t.ttdx_f*t.ttdx_f)+abs(t.ttdy_f*t.ttdy_f)+abs(t.ttdz_f*t.ttdz_f));
						t.ttdx_f=t.ttdx_f/t.ttdd_f;
						t.ttdy_f=t.ttdy_f/t.ttdd_f;
						t.ttdz_f=t.ttdz_f/t.ttdd_f;
						g.decalx=t.decalx1-(t.ttdx_f*5.0);
						g.decaly=t.decaly1-(t.ttdy_f*5.0);
						g.decalz=t.decalz1-(t.ttdz_f*5.0);
						g.decalx=(g.decalx-10)+Rnd(20);
						g.decaly=(g.decaly-20)+Rnd(40);
						g.decalz=(g.decalz-10)+Rnd(20);
						for ( t.iter = 1 ; t.iter<=  1+Rnd(1); t.iter++ )
						{
							decal_triggerbloodsplat ( );
						}
					}
				}
				else
				{
					if (bImmunityFromActualDamage == false)
					{
						if (t.te != -1)
						{
							// only if entity caused damage
							new_damage_marker(t.te, ObjectPositionX(t.entityelement[t.te].obj), ObjectPositionY(t.entityelement[t.te].obj), ObjectPositionZ(t.entityelement[t.te].obj), t.tdamage);
						}
						else
						{
							// hurt from non-entity source
							for (t.iter = 0; t.iter <= 9; t.iter++)
							{
								placeblood(50, 0, 0, 0, 0);
							}
						}
					}
				}
			}

			// if melee damage, mark with a thump!
			//PE: Only if melee. it would play whenever hurtplayer was called from lua.
			if (t.te > 0)
			{
				int gunid = t.entityelement[t.te].eleprof.hasweapon;
				if (gunid > 0)
				{
					if (t.gun[gunid].weapontype == 51) // melee weapon type is 51
					{
						physics_play_thump_sound(CameraPositionX(), CameraPositionY(), CameraPositionZ(), 38000, Rnd(8000));
					}
				}
			}


			// Trigger player grunt noise or block sound
			if (bSuccessfullyBlockingNow == false)
			{
				if (bImmunityFromActualDamage == false)
				{
					if (t.playercontrol.startviolent != 0 && g.quickparentalcontrolmode != 2)
					{
						if ((DWORD)(MAXTimer() + 250) > t.playercontrol.timesincelastgrunt)
						{
							// only ever one in three or if been a while since we grunted
							t.playercontrol.timesincelastgrunt = MAXTimer();
							int iLastOne = t.tplrhurt;
							bool bHaveUniqueSound = false;
							while (bHaveUniqueSound == false)
							{
								int iRandomHurt = Rnd(12);
								switch (iRandomHurt)
								{
									case 0: t.tplrhurt = 1; break;
									case 1: t.tplrhurt = 2; break;
									case 2: t.tplrhurt = 3; break;
									case 3: t.tplrhurt = 4; break;
									case 4: t.tplrhurt = 8; break;
									case 5: t.tplrhurt = 9; break;
									case 6: t.tplrhurt = 10; break;
									case 7: t.tplrhurt = 16; break;
									case 8: t.tplrhurt = 21; break;
									case 9: t.tplrhurt = 22; break;
									case 10: t.tplrhurt = 23; break;
									case 11: t.tplrhurt = 24; break;
									case 12: t.tplrhurt = 25; break;
								}
								if (iLastOne != t.tplrhurt) bHaveUniqueSound = true;
							}
							t.tsnd = t.playercontrol.soundstartindex + t.tplrhurt;
							playinternalsound(t.tsnd);
						}
					}
				}
			}
			else
			{
				// if player is blocking, no damage
				
				// and play shield style sound with subtle camera shake to show we took it well!
				int iRandomBlockSnd = Rnd(3);
				switch (iRandomBlockSnd)
				{
					case 0: t.tplrhurt = 26; break;
					case 1: t.tplrhurt = 27; break;
					case 2: t.tplrhurt = 28; break;
				}
				t.tsnd = t.playercontrol.soundstartindex + t.tplrhurt;
				playinternalsound(t.tsnd);
				int iMinDamage = t.tdamage;
				if (t.tdamage < 50) iMinDamage = 50;
				t.playercontrol.camerashake_f = (iMinDamage / 100.0f) * 100.0f;

				// aloow all player hit code to proceed (block/etc) but just deal no damage
				if (t.player[t.plrid].health >= 99999) t.tdamage = fOriginalDamage;
				float fhalfdamagebacktoenemy = t.tdamage / 2.0f;

				// but repel their damage back to the attacker
				if (t.te > 0 && t.te < t.entityelement.size())
				{
					t.ttte = t.te;
					t.tdamage = fhalfdamagebacktoenemy;
					t.tdamageforce = 0.0f;
					t.brayx1_f = CameraPositionX();
					t.brayy1_f = CameraPositionY();
					t.brayz1_f = CameraPositionZ();
					t.brayx2_f = t.entityelement[t.te].x;
					t.brayy2_f = t.entityelement[t.te].y;
					t.brayz2_f = t.entityelement[t.te].z;
					t.tallowanykindofdamage = 0;
					t.twhox_f = t.brayx1_f;
					t.twhoz_f = t.brayz1_f;
					t.tdamagesource = 1;
					entity_applydamage ();
				}

				// and mark an actual player block
				if (g_iSuccessfullyBlockedAtTime == 0)
					g_iSuccessfullyBlockedAtTime = 1;

				// no further player damage code
				return;
			}
		}
		
		// Check if player health at zero
		if ( t.player[t.plrid].health <= 0 ) 
		{
			t.player[t.plrid].health = 0;
			LuaSetFunction ("PlayerHealthSet", 1, 0);
			LuaPushInt(0);
			LuaCall();
			if (  t.game.runasmultiplayer  ==  1 )
			{
				if (  t.tsteamwasnetworkdamage  ==  1 ) 
				{
					if (  t.entityelement[t.texplodesourceEntity].mp_networkkill  ==  1 ) 
					{
						//  inform of network kill
						mp_networkkill ( );
					}
				}
			}
			//  player looses a life
			if (  t.playercontrol.startlives>0 ) 
			{
				//  only reduce lives if using lives
				if (  t.game.runasmultiplayer == 0 ) 
				{
					t.player[t.plrid].lives=t.player[t.plrid].lives-1;
					if (  t.player[t.plrid].lives <= 0 ) 
					{
						t.player[t.plrid].lives=0;
					}
				}
			}
			if ( t.playercontrol.startviolent != 0 && g.quickparentalcontrolmode != 2 )
			{
				if ( t.tDrownDamageFlag == 0 ) 
				{
					// player grunts in deadness if this isn't death by drowning
					playinternalsound(t.playercontrol.soundstartindex+1);
				}
			}
			// if camera was overriden, take it back
			g.luacameraoverride = 0;
			// if was frozen, unfreeze for the restore
			t.aisystem.processplayerlogic = 1;
			// restore player zoom
			t.plrzoominchange=1 ; t.plrzoomin_f=0.0;
			gun_playerdead ( );
			// start death sequence for player
			t.playercontrol.deadtime= MAXTimer()+2000;
			// make sure all music is stopped
			if (  t.playercontrol.disablemusicreset == 0 ) 
			{
				music_resetall ( );
			}
			//  if third person, also create ragdoll of protagonist
			if (  t.playercontrol.thirdperson.enabled == 1 ) 
			{
				t.ttte=t.playercontrol.thirdperson.charactere;
				t.tdamageforce=0;
				t.entityelement[t.ttte].health=1 ; t.tdamage=1;
				t.entityelement[t.ttte].ry=ObjectAngleY(t.entityelement[t.ttte].obj);
				t.tskiplayerautoreject=1;
				entity_applydamage ( );
				t.tskiplayerautoreject=0;
			}
		}
	}
}

void physics_player_gotolastcheckpoint ( void )
{
	//  move player to last checkpoint (or start marker if no checkpoint)
	t.terrain.playerx_f=t.playercheckpoint.x;
	t.terrain.playery_f=t.playercheckpoint.y;
	t.terrain.playerz_f=t.playercheckpoint.z;
	t.terrain.playerax_f=0;
	t.terrain.playeray_f=t.playercheckpoint.a;
	t.camangy_f=t.terrain.playeray_f;
	t.terrain.playeraz_f=0;
	t.playercontrol.finalcameraangley_f=t.terrain.playeray_f;
	physics_resetplayer_core ( );

	// resume all soundloops from when passed through checkpoint
	bool bPauseAndResumeFromGameMenu = false;
	game_main_snapshotsoundresumecheckpoint(bPauseAndResumeFromGameMenu);
}

void physics_resetplayer_core ( void )
{
	//  Cannot restart under water!
	if (  t.hardwareinfoglobals.nowater == 0 ) 
	{
		if (  t.terrain.playery_f<t.terrain.waterliney_f ) 
		{
			t.terrain.playery_f=t.terrain.waterliney_f+t.terrain.adjaboveground_f;
		}
	}

	//  if the player was previous underwater, set them above water and switch off underwater effects
	physics_player_reset_underwaterstate ( );

	//  disable and setup player
	physics_disableplayer ( );
	physics_setupplayer ( );

	//  restore health
	t.player[t.plrid].health=t.playercontrol.startstrength;

	//  ressurection cease fire allows player to escape shooters when respawn
	t.playercontrol.ressurectionceasefire= MAXTimer()+3000;

	//  reset vegetation
	t.completelyfillvegarea=1;

	//  fade in game screen again
	t.postprocessings.fadeinvalue_f=0.0;

	//  player is immune for a while
	t.huddamage.immunity=1000;

	//  reset death state
	t.playercontrol.deadtime=0;
	if ( g.luacameraoverride != 2 && g.luacameraoverride != 3 )
	{
		ZRotateCamera (  0,0 );
	}

	//  red screen effect finish
	t.playercontrol.redDeathFog_f = 0;
	SetVector4 (  g.terrainvectorindex,0,0,0,0 );
	t.tColorVector = g.terrainvectorindex ; postprocess_setscreencolor ( );

	//  Stop any blood HUD
	resetblood();
	resetdamagemarker();

	//  Deal with sounds if not disabled via script
	if (  t.playercontrol.disablemusicreset == 0 ) 
	{
		//  Stop any incidental music
		game_stopallsounds();
		//  Stop any looping projectile sounds
		weapon_projectile_reset ( );
		//  play default music
		music_playdefault ( );

		//  Restore any sounds from last checkpoint/start marker
		for ( t.s = g.soundbankoffset ; t.s<=  g.soundbankoffsetfinish; t.s++ )
		{
			if (  t.soundloopcheckpoint[t.s] == 1 ) 
			{
				if (  SoundExist(t.s) == 1 ) 
				{
					LoopSound (  t.s );
				}
			}
		}
	}

	//  ensure all markers and waypoints remain hidden
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		if (  t.entityprofile[t.entid].ismarker != 0 ) 
		{
			t.obj=t.entityelement[t.e].obj;
			if (  t.obj>0 ) 
			{
				if (  ObjectExist(t.obj) == 1 ) 
				{
					HideObject (  t.obj );
				}
			}
		}
	}
	waypoint_hideall ( );

	// if third person, resurrect ragdoll protagonist
	if ( t.playercontrol.thirdperson.enabled == 1 ) 
	{
		// ensure character protagonist is respawned
		t.e=t.playercontrol.thirdperson.charactere;
		entity_lua_spawn_core ( );
		physics_player_thirdpersonreset ( );
		//PE: Something is disable Z depth buffer when 3rd die ?, just reenable.
		//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/330
		EnableObjectZWrite(t.entityelement[t.e].obj);
		EnableObjectZDepth(t.entityelement[t.e].obj);
	}
}

void physics_player_thirdpersonreset ( void )
{
	//  convert to clone so can animate it (ragdoll makes it an instance)
	t.tentityconverttoclonenotshared=1;
	t.tte=t.e ; entity_converttoclone ( );
	t.tentityconverttoclonenotshared=0;
	t.charanimstate.e=t.e ; t.obj=t.entityelement[t.e].obj;
	entity_setupcharobjsettings ( );
	//  and reset any transitional states in animation system (avoid freezing it)
	t.smoothanim[t.charanimstate.obj].transition=0;
	t.smoothanim[t.charanimstate.obj].playstarted=0;
	//  camera has smoothing in third person, so reset this
	if ( g.luacameraoverride != 1 && g.luacameraoverride != 3 )
	{
		PositionCamera (  t.terrain.gameplaycamera,t.terrain.playerx_f,t.terrain.playery_f,t.terrain.playerz_f );
	}

	// ensure depth is not written so no motion blur
	// apply special character shader so can uniquely change shader constants
	// without affecting other NPCs and trees, etc
	if ( t.obj>0 ) 
	{
		if ( ObjectExist(t.obj) == 1 ) 
		{
			int tttentid = t.entityelement[t.e].bankindex;
			int ttsourceobj = g.entitybankoffset + tttentid;
			if ( ttsourceobj > 0 )
			{
				if ( ObjectExist ( ttsourceobj ) == 1 )
				{
					if ( GetNumberOfFrames ( ttsourceobj ) > 0 )
					{
						// third person is animating
						SetObjectEffect ( t.obj, g.thirdpersoncharactereffect );
						SetEffectConstantF (  g.thirdpersoncharactereffect,"DepthWriteMode",0.0f );			
					}
					else
					{
						// third person is non-animating
						SetObjectEffect ( t.obj, g.thirdpersonentityeffect );
						SetEffectConstantF ( g.thirdpersonentityeffect, "DepthWriteMode", 0.0f );
					}
				}
			}
		}
	}
	//  and also treat the vweap attachment too
	t.tattachmentobj=t.entityelement[t.e].attachmentobj;
	if (  t.tattachmentobj>0 ) 
	{
		if (  ObjectExist(t.tattachmentobj) == 1 ) 
		{
			SetObjectEffect (  t.tattachmentobj,g.thirdpersonentityeffect );
			SetEffectConstantF (  g.thirdpersonentityeffect,"DepthWriteMode",0.0f );
		}
	}
}

bool physics_player_addweapon ( void )
{
	extern int g_iSuggestedSlot;
	//  takes weaponindex
	t.tweaponisnew=0;
	//  check all weapon slots
	t.gotweapon=0;
	for ( t.ws = 1 ; t.ws < 10; t.ws++ )
	{
		if (  t.weaponslot[t.ws].got == t.weaponindex  )  t.gotweapon = t.ws;
	}
	if ( t.gotweapon == 0 ) 
	{
		// check if we have a slot preference
		t.tweaponisnew=1;
		t.gotweaponpref=0;
		for ( t.ws = 1 ; t.ws < 12; t.ws++ )
		{
			if (  t.weaponslot[t.ws].pref == t.weaponindex )  
				t.gotweaponpref = t.ws;
		}
		// check if we are forcing a suggested slot: g_iSuggestedSlot
		if (t.gotweaponpref == 0)
		{
			// is there a suggested slot?
			if (g_iSuggestedSlot > 0)
			{
				if (t.weaponslot[g_iSuggestedSlot].got == 0)
				{
					// check if suggested slot is available (not blocked)
					bool bThisSlotBlocked = false;
					int prefGunID = t.weaponslot[g_iSuggestedSlot].pref;
					if (prefGunID > 0)
					{
						if (stricmp(t.gun[prefGunID].name_s.Get(), "Slot Not Used") == NULL)
						{
							bThisSlotBlocked = true;
						}
					}
					if (bThisSlotBlocked == false )
					{
						// suggested slot is available, use it
						t.gotweaponpref = g_iSuggestedSlot;
					}
				}
			}
		}

		// add weapon
		if (t.gotweaponpref == 0)
		{
			// find free slot
			for ( t.ws = 1 ; t.ws < 10; t.ws++ )
			{
				if (t.weaponslot[t.ws].got == 0 && t.weaponslot[t.ws].pref == 0)
					break;
			}

			// force a slot
			if ( g.forcedslot != 0 )
			{ 
				t.ws = g.forcedslot; 
				t.gotweaponpref = t.ws; 
				g.forcedslot = 0; 
			}

			// LB: superceded with more pref control, just need 't.ws <= 10' to know we have a slot available at this point!
			// count weapons for maximum slots. If exceeded, prevent pick up.
			if ( t.ws < 10 ) 
			{
				// add weapon into free slot and create pref for it
				t.weaponslot[t.ws].pref=t.weaponindex;
				t.weaponhud[t.ws]=t.gun[t.weaponindex].hudimage;

				// mark weapon with 'possible' entity that held this weapon (for equipment activation)
				g.firemodes[t.weaponindex][0].settings.equipmententityelementindex=t.autoentityusedtoholdweapon;
			}
			else
			{
				// no room for weapon in available slots
				t.ws = 0;

				// and can leave right now
				return false;
			}
		}
		else
		{
			t.ws = t.gotweaponpref;
		}

		// switch to collected weapon
		if ( g.autoswap == 1 && t.ws>0 ) 
		{
			// insert as slot weapon
			t.weaponslot[t.ws].got=t.weaponindex;
			t.weaponslot[t.ws].invpos=t.weaponinvposition;
			g.autoloadgun=t.weaponindex;
			t.weaponkeyselection=t.ws;
			t.gotweapon=t.ws;
		}

		//  place details of weapon in slot
		if (  t.ws>0 ) 
		{
			//  insert as slot weapon
			t.weaponslot[t.ws].got=t.weaponindex;
			t.weaponslot[t.ws].invpos=t.weaponinvposition;
			t.gotweapon=t.ws;
			if (  t.gunid == 0 ) 
			{
				//  if no gun held, auto select collected
				g.autoloadgun=t.weaponindex;
				t.weaponkeyselection=t.ws;
			}

			// when slot suggested (editing hot keys, no need for pref init position)
			if (g_iSuggestedSlot > 0 ) t.weaponslot[t.ws].pref = 0; 
		}
	}

	// weapons start with some ammo
	if ( t.gotweapon>0 ) 
	{
		// ammo for weapon
		t.tgunid = t.weaponslot[t.gotweapon].got;
		if ( t.gun[t.tgunid].settings.weaponisammo == 0 )
		{
			// no ammo should be possible
			if (t.tqty < 0) t.tqty = 0; 

			// when new weapon starts, get ammo from store (in case was moved to container and is brought back)
			if (t.tweaponisnew == 1)
			{
				if (t.gotweapon < 10)
				{
					t.weaponammo[t.gotweapon] = t.gun[t.tgunid].storeammo;
					t.weaponclipammo[t.gotweapon] = t.gun[t.tgunid].storeclipammo;
				}
			}

			t.taltqty = 0;
			if (t.gotweapon < 10 && t.weaponammo[t.gotweapon] == 0 && t.tweaponisnew == 1)
			{
				// provide some alternative ammo (weaponammo+10)
				if ( t.gun[t.tgunid].settings.modessharemags == 0 ) 
				{
					//  080415 - only if not sharing ammo
					t.taltqty = t.tqty;
					if (  t.taltqty>g.firemodes[t.tgunid][1].settings.reloadqty )
					{
						t.altpool=g.firemodes[t.tgunid][1].settings.poolindex;
						t.weaponammo[t.gotweapon+10]=g.firemodes[t.tgunid][1].settings.reloadqty;
						if (t.altpool > 0)
						{
							t.ammopool[t.altpool].ammo = t.ammopool[t.altpool].ammo + (t.taltqty - g.firemodes[t.tgunid][1].settings.reloadqty);
							int iMaxClipCapacity = g.firemodes[t.tgunid][1].settings.clipcapacity * g.firemodes[t.tgunid][1].settings.reloadqty;
							if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
							if (t.ammopool[t.altpool].ammo > iMaxClipCapacity) t.ammopool[t.altpool].ammo = iMaxClipCapacity;
						}
						else
						{
							t.weaponclipammo[t.gotweapon + 10] = t.taltqty - g.firemodes[t.tgunid][1].settings.reloadqty;
						}
					}
					else
					{
						if (  t.gun[t.tgunid].settings.addtospare == 0 ) 
						{
							t.weaponammo[t.gotweapon+10]=t.taltaty;
						}
						else
						{
							if (  t.gun[t.tgunid].settings.canaddtospare == 1 ) 
							{
								t.altpool=g.firemodes[t.tgunid][1].settings.poolindex;
								int iMaxClipCapacity = g.firemodes[t.tgunid][1].settings.clipcapacity * g.firemodes[t.tgunid][1].settings.reloadqty;
								if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
								if (t.altpool == 0)
								{
									t.weaponclipammo[t.gotweapon + 10] = t.weaponclipammo[t.gotweapon + 10] + t.taltqty;
									if (t.weaponclipammo[t.gotweapon + 10] > iMaxClipCapacity) t.weaponclipammo[t.gotweapon + 10] = iMaxClipCapacity;
								}
								else
								{
									t.ammopool[t.altpool].ammo = t.ammopool[t.altpool].ammo + t.taltqty;
									if (t.ammopool[t.altpool].ammo > iMaxClipCapacity) t.ammopool[t.altpool].ammo = iMaxClipCapacity;
								}
							}
							if (  t.gun[t.tgunid].settings.canaddtospare == 0  )  t.weaponammo[t.gotweapon+10] = t.taltqty;
						}
					}
				}
				//  provide some primary ammo
				if (  t.tqty>g.firemodes[t.tgunid][0].settings.reloadqty ) 
				{
					//  gun has MAX slots of ammo, cannot exceed this!
					t.tpool=g.firemodes[t.tgunid][0].settings.poolindex;
					t.weaponammo[t.gotweapon]=g.firemodes[t.tgunid][0].settings.reloadqty;
					if (  t.tpool>0 ) 
					{
						t.ammopool[t.tpool].ammo=t.ammopool[t.tpool].ammo+(t.tqty-g.firemodes[t.tgunid][0].settings.reloadqty);
					}
					else
					{
						t.weaponclipammo[t.gotweapon]=t.tqty-g.firemodes[t.tgunid][0].settings.reloadqty;
					}
				}
				else
				{
					if (  t.gun[t.tgunid].settings.addtospare == 0 ) 
					{
						t.weaponammo[t.gotweapon]=t.tqty;
					}
					else
					{
						//  new gunspec addition "addtospare" this will allow it so picking up ammo
						//  with an empty weapon won't add the ammo directly into the clip
						if (  t.gun[t.tgunid].settings.canaddtospare == 1 ) 
						{
							t.tpool=g.firemodes[t.tgunid][0].settings.poolindex;
							int iMaxClipCapacity = g.firemodes[t.tgunid][0].settings.clipcapacity * g.firemodes[t.tgunid][0].settings.reloadqty;
							if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
							if (  t.tpool == 0 )
							{
								t.weaponclipammo[t.gotweapon] = t.weaponclipammo[t.gotweapon] + t.tqty;
								if (t.weaponclipammo[t.gotweapon] > iMaxClipCapacity) t.weaponclipammo[t.gotweapon] = iMaxClipCapacity;
							}
							else
							{
								t.ammopool[t.tpool].ammo=t.ammopool[t.tpool].ammo+t.tqty;
								if (t.ammopool[t.tpool].ammo > iMaxClipCapacity) t.ammopool[t.tpool].ammo = iMaxClipCapacity;
							}
						}
						if (  t.gun[t.tgunid].settings.canaddtospare == 0 ) 
						{
							t.gun[t.tgunid].settings.canaddtospare=1;
							t.weaponammo[t.gotweapon]=t.tqty;
						}
					}
				}
			}
			else
			{
				if (t.gotweapon < 10)
				{
					t.tpool = g.firemodes[t.tgunid][0].settings.poolindex;
					t.altpool = g.firemodes[t.tgunid][1].settings.poolindex;
					int iMaxClipCapacity = g.firemodes[t.tgunid][0].settings.clipcapacity * g.firemodes[t.tgunid][0].settings.reloadqty;
					if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
					if (t.tpool == 0)
					{
						t.weaponclipammo[t.gotweapon] = t.weaponclipammo[t.gotweapon] + t.tqty;
						if (t.weaponclipammo[t.gotweapon] > iMaxClipCapacity) t.weaponclipammo[t.gotweapon] = iMaxClipCapacity;
					}
					else
					{
						t.ammopool[t.tpool].ammo = t.ammopool[t.tpool].ammo + t.tqty;
						if (t.ammopool[t.tpool].ammo > iMaxClipCapacity) t.ammopool[t.tpool].ammo = iMaxClipCapacity;
					}
					iMaxClipCapacity = g.firemodes[t.tgunid][1].settings.clipcapacity * g.firemodes[t.tgunid][1].settings.reloadqty;
					if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
					if (t.altpool == 0)
					{
						t.weaponclipammo[t.gotweapon + 10] = t.weaponclipammo[t.gotweapon + 10] + t.taltqty;
						if (t.weaponclipammo[t.gotweapon + 10] > iMaxClipCapacity) t.weaponclipammo[t.gotweapon + 10] = iMaxClipCapacity;
					}
					else
					{
						t.ammopool[t.altpool].ammo = t.ammopool[t.altpool].ammo + t.taltqty;
						if (t.ammopool[t.altpool].ammo > iMaxClipCapacity) t.ammopool[t.altpool].ammo = iMaxClipCapacity;
					}
				}
			}
		}
	}

	//  refresh gun count
	physics_player_refreshcount ( );

	//  if collected weapon, and is empty, trigger reload if gun anim able
	if (  t.gotweapon>0 && t.gotweapon < 10)
	{
		t.tgunid=t.weaponslot[t.gotweapon].pref;
		if (  t.weaponammo[t.gotweapon] == 0 ) 
		{
			if (  t.gunmode >= 5 && t.gunmode<31 ) 
			{
				t.gunmode=121;
			}
		}
	}

	// success
	return true;
}

void physics_player_removeweapon ( void )
{
	// check all weapon slots
	for ( t.ws = 1 ; t.ws < 12; t.ws++ )
	{
		if ( t.weaponslot[t.ws].got == t.weaponindex  )  break;
	}
	if (  t.ws < 12 ) 
	{
		// Ensure gun is removed (if applicable)
		if ( t.gunid>0 && t.weaponslot[t.ws].got == t.gunid ) 
		{
			g.autoloadgun=0;
		}
		// drop weapon from slot
		t.weaponslot[t.ws].got=0;
		t.weaponslot[t.ws].invpos=0;
	}

	//  refresh gun count
	physics_player_refreshcount ( );
}

void physics_player_resetWeaponSlots( void )
{
	for (t.ws = 1; t.ws < 12; t.ws++)
	{
		t.weaponslot[t.ws].got = 0;
		t.weaponslot[t.ws].invpos = 0;
	}
}

void physics_player_refreshcount ( void )
{
	//  refresh gun count
	t.guncollectedcount=0;
	for ( t.ws = 1 ; t.ws < 10; t.ws++ )
	{
		if (  t.weaponslot[t.ws].got>0  )  ++t.guncollectedcount;
	}

	// Trigger zoom out so that the player doesn't stay zoomed in when picking up a new weapon
	if (t.gunzoommode == 9 || t.gunzoommode == 10)
	{
		t.gunzoommode = 11;
	}

	return;
}

void physics_clear_debug_draw(void)
{
	for (int i = 0; i < t.iPhysicsDebugObjects.size(); i++)
	{
		if (ObjectExist(t.iPhysicsDebugObjects[i])) DeleteObject(t.iPhysicsDebugObjects[i]);
	}
	t.iPhysicsDebugObjects.clear();
	t.iPhysicsDebugObjectsToUpdate.clear();
	t.iPhysicsCreatedDynamicMesh = 0;
	t.iPhysicsCreatedStaticMesh = 0;
	t.iPhysicsDebugMaxOffset = 0;
	t.physicsDebugDrawData = nullptr;
	t.iPhysicsDebugDynamicOffsets.clear();
	BPhys_ClearDebugDrawData();
}

// Set the drawing mode of the physics debug drawer.
void physics_set_debug_draw(int iDraw)
{
	// can force a cleanup if statics flag toggled
	if (iDraw == 1) physics_clear_debug_draw();

	// set debug draw flags
	BPhys_SetDebugDrawerMode(iDraw, t.visuals.iPhysicsDebugDrawStatics, t.visuals.iPhysicsDebugDrawConstraints);

	// Clean up.
	if (!iDraw) physics_clear_debug_draw();
}

void physics_create_debug_mesh(float* data, int count, bool bStatic, int offset)
{
	if (!data) return;

	// Find a free memblock.
	int iFound = 0;
	for (int i = 1; i <= 257; i++)
	{
		if (MemblockExist(i) == 0)
		{
			iFound = i;
			break;
		}
	}
	if (iFound == 0) return;

	// Find a free object slot.
	int obj = -1;
	for (int i = g.physicsdebugdraweroffset; i < g.physicsdebugdraweroffset + 200; i++)
	{
		if (ObjectExist(i) == 0)
		{
			obj = i;
			break;
		}
	}
	if (obj < 0) return;

	int vertsize = 32;
	int iSizeBytes = 0;
	int vertexCount = count / 3;
	vertexCount *= 9;
	iSizeBytes = vertsize * vertexCount;
	iSizeBytes += 12; // Add header bytes.

	// if memblock creation fails, try smaller size so we see something!
	if (iSizeBytes > 0)
	{
		for (int iTries = 0; iTries < 6; iTries++)
		{
			MakeMemblock(iFound, iSizeBytes);
			if (MemblockExist(iFound) == 0)
			{
				// try half that
				count /= 2;
				vertexCount = count / 3;
				vertexCount *= 9;
				iSizeBytes = vertsize * vertexCount;
				iSizeBytes += 12;
			}
			else
			{
				// memblock creation successful
				break;
			}
		}
	}

	// Write the memblock header.
	if (MemblockExist(iFound) == 1)
	{
		// FVF format.
		WriteMemblockDWord(iFound, 0, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1);
		// Size of single vertex - 3 x float: position, 3 x float: normal,1 x DWORD: diffuse, 2 x float: tex coords = 36 bytes.
		WriteMemblockDWord(iFound, 4, 12 + 12 + 8);
		// Number of vertices in the mesh.
		WriteMemblockDWord(iFound, 8, vertexCount);

		float x0, x1, x2, x3, x4, x5;
		float y0, y1, y2, y3, y4, y5;
		float z0, z1, z2, z3, z4, z5;
		int v = 0;
		float p0[3];
		float p1[3];
		float points[18];

		// Every 6 elements of data contain two points on the physics object.
		// data can contain the vertices of a previously created mesh, so start at offset (count at the last mesh creation).
		for (int i = offset; i <= count - 6; i += 6)
		{
			p0[0] = data[i]; p0[1] = data[i + 1]; p0[2] = data[i + 2];
			p1[0] = data[i + 3]; p1[1] = data[i + 4]; p1[2] = data[i + 5];

			physics_debug_make_prism_between_points(p0, p1, points);

			// Corners of the prism.
			x0 = points[0]; y0 = points[1]; z0 = points[2];
			x1 = points[3]; y1 = points[4]; z1 = points[5];
			x2 = points[6]; y2 = points[7]; z2 = points[8];
			x3 = points[9]; y3 = points[10]; z3 = points[11];
			x4 = points[12]; y4 = points[13]; z4 = points[14];
			x5 = points[15]; y5 = points[16]; z5 = points[17];

			// Form the faces of the prism from the corners.
			physics_add_vert_to_debug_mesh(x0, y0, z0, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x2, y2, z2, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x3, y3, z3, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x2, y2, z2, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x4, y4, z4, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x3, y3, z3, v, iFound);
			v++;

			physics_add_vert_to_debug_mesh(x1, y1, z1, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x2, y2, z2, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x4, y4, z4, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x2, y2, z2, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x5, y5, z5, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x4, y4, z4, v, iFound);
			v++;

			physics_add_vert_to_debug_mesh(x0, y0, z0, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x1, y1, z1, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x5, y5, z5, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x0, y0, z0, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x5, y5, z5, v, iFound);
			v++;
			physics_add_vert_to_debug_mesh(x3, y3, z3, v, iFound);
			v++;
		}

		// Keep track of the newly created object so we can delete it later.
		t.iPhysicsDebugObjects.push_back(obj);

		// Dynamic objects will need their vertices updated to prevent recreating the mesh each frame.
		if (!bStatic)
		{
			t.iPhysicsDebugObjectsToUpdate.push_back(obj);
			t.iPhysicsDebugDynamicOffsets.push_back(offset);
		}

		WickedCall_PresetObjectCreateOnDemand(true);
		CreateMeshFromMemblock(obj, iFound);
		MakeObject(obj, obj, 0);
		WickedCall_PresetObjectCreateOnDemand(false);

		sObject* pObject = GetObjectData(obj);
		int verts = pObject->ppMeshList[0]->dwVertexCount;

		SetObject(obj, 0, 0, 0, 0, 0, 0, 0);
		SetObjectCollisionOff(obj);
		DisableObjectZWrite(obj);
		SetObjectLight(obj, 0);
		SetObjectMask(obj, 1);

		// 150817 - GUI shader with DIFFUSE element included
		SetObjectEffect(obj, g.guidiffuseshadereffectindex);

		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);
		WickedCall_RemoveObject(pObject);
		WickedCall_AddObject(pObject);
		WickedCall_TextureObject(pObject, NULL);
		WickedCall_SetObjectCastShadows(pObject, false);
		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
		// wicked object does not use per-vertex diffuse color, so find first color and apply to whole object
		DWORD diffuse = Rgb(0, 255, 0);
		SetObjectDiffuse(obj, Rgb(0, 255, 0));

		// set alpha and transparency of this object
		SetObjectTransparency(obj, 2);
		SetAlphaMappingOn(obj, 75);// 25 );

		DeleteMemblock(iFound);
	}
}

void physics_add_vert_to_debug_mesh(float x, float y, float z, int v, int memblock)
{
	//  Position of vertex in memblock
	int pos = 12 + (v * 32);// 12);

	//  Set vertex position
	WriteMemblockFloat(memblock, pos + 0, x);
	WriteMemblockFloat(memblock, pos + 4, y);
	WriteMemblockFloat(memblock, pos + 8, z);
}

void physics_update_debug_mesh(float* data, int count, int objectID, int offsetLower, int offsetUpper)
{
	float x0, x1, x2, x3, x4, x5;
	float y0, y1, y2, y3, y4, y5;
	float z0, z1, z2, z3, z4, z5;
	int v = 0;
	float p0[3];
	float p1[3];
	float points[18];

	if (!data) return;

	sObject* pObject = GetObjectData(objectID);
	if (!pObject)
	{
		return;
	}
	
	sMesh* pMesh = pObject->ppMeshList[0];
	if (!pMesh)
	{
		return;
	}
	
	LockVertexDataForLimbCore(objectID, 0, 1);
	for (int i = offsetLower; i <= offsetUpper - 6; i += 6)
	{
		// Every 6 elements of data contain two points on the physics object.
		p0[0] = data[i]; p0[1] = data[i + 1]; p0[2] = data[i + 2];
		p1[0] = data[i + 3]; p1[1] = data[i + 4]; p1[2] = data[i + 5];

		physics_debug_make_prism_between_points(p0, p1, points);

		// Corners of the prism.
		x0 = points[0]; y0 = points[1]; z0 = points[2];
		x1 = points[3]; y1 = points[4]; z1 = points[5];
		x2 = points[6]; y2 = points[7]; z2 = points[8];
		x3 = points[9]; y3 = points[10]; z3 = points[11];
		x4 = points[12]; y4 = points[13]; z4 = points[14];
		x5 = points[15]; y5 = points[16]; z5 = points[17];

		// Make each face of the prism (two triangles per face).
		SetVertexDataPosition(v++, x0, y0, z0);
		SetVertexDataPosition(v++, x2, y2, z2);
		SetVertexDataPosition(v++, x3, y3, z3);
		SetVertexDataPosition(v++, x2, y2, z2);
		SetVertexDataPosition(v++, x4, y4, z4);
		SetVertexDataPosition(v++, x3, y3, z3);
							 
		SetVertexDataPosition(v++, x1, y1, z1);
		SetVertexDataPosition(v++, x2, y2, z2);
		SetVertexDataPosition(v++, x4, y4, z4);
		SetVertexDataPosition(v++, x2, y2, z2);
		SetVertexDataPosition(v++, x5, y5, z5);
		SetVertexDataPosition(v++, x4, y4, z4);
							 
		SetVertexDataPosition(v++, x0, y0, z0);
		SetVertexDataPosition(v++, x1, y1, z1);
		SetVertexDataPosition(v++, x5, y5, z5);
		SetVertexDataPosition(v++, x0, y0, z0);
		SetVertexDataPosition(v++, x5, y5, z5);
		SetVertexDataPosition(v++, x3, y3, z3);
	}

	UnlockVertexData();

	WickedCall_UpdateMeshVertexData(pMesh);
}

void physics_debug_make_prism_between_points(float* p0, float* p1, float* results, float thickness)
{
	XMFLOAT3 a;
	XMFLOAT3 b;
	XMVECTOR positionA;
	XMVECTOR direction;
	XMVECTOR normal;
	XMFLOAT3 worldUp;
	XMVECTOR temp;
	XMVECTOR dot;
	XMVECTOR right;
	XMVECTOR up;

	// for terrain physics debug!
	XMVECTOR points[6];

	// Calculate direction between the two points.
	a = XMFLOAT3(p0[0], p0[1], p0[2]);
	b = XMFLOAT3(p1[0], p1[1], p1[2]);
	direction = XMLoadFloat3(&b) - XMLoadFloat3(&a);
	normal = XMVector3Normalize(direction);

	// Calculate a vector that allows us to get another vector perpendicular to the direction vector.
	worldUp = XMFLOAT3(0.0f, 1.0f, 0.0f);
	temp = XMLoadFloat3(&worldUp);
	dot = XMVector3Dot(normal, temp);
	positionA = XMLoadFloat3(&a);
	if (XMVectorGetX(dot) >= 0.98f || XMVectorGetX(dot) <= -0.98f)
	{
		// Normal and temp have the same direction, so change temp.
		worldUp = XMFLOAT3(1.0f, 0.0f, 0.0f);
		temp = XMLoadFloat3(&worldUp);
	}

	// Calculate the other two basis vectors, giving 3 orthogonal vectors.
	right = XMVector3Cross(normal, temp);
	right = XMVector3Normalize(right);

	up = XMVector3Cross(right, normal);
	up = XMVector3Normalize(up);

	// Calculate 3 points about the normal vector (that pass through the right and up vectors)
	for (int j = 0; j < 3; j++)
	{
		// 2.0944 radians = 120 degrees for 3 points around the positionA.
		points[j] = positionA + (thickness * cosf(2.0944f * j) * right) + (thickness * sinf(2.0944f * j) * up);

		// Calculate the corresponding point on the other triangle by moving the first point towards the second.
		points[j + 3] = points[j] + direction;
	}

	for (int j = 0; j < 6; j++)
	{
		results[3 * j + 0] = XMVectorGetX(points[j]);
		results[3 * j + 1] = XMVectorGetY(points[j]);
		results[3 * j + 2] = XMVectorGetZ(points[j]);
	}
}

// For editor only.
void physics_debug_add_object(int objectID)
{
	if(BPhys_GetDebugObjectCount() == 0) ODEStart();
	BPhys_AddDebugSingleObject(objectID);
	t.tphyobj = objectID;
	t.entid = objectID;
	physics_importer_create_temp();
}

// For editor only.
void physics_debug_remove_object(int objectID)
{
	BPhys_RemoveDebugSingleObject(objectID);
	if (BPhys_GetDebugObjectCount() == 0) ODEEnd();
}

// For editor only.
void physics_debug_draw()
{
	BPhys_DrawDebugObjects();
	physics_render_debug_meshes();
}

// For test game and editor.
void physics_render_debug_meshes()
{
	if (BPhys_GetDebugDrawerMode() != 0)
	{
		int elementCount = 0;

		if (t.iPhysicsCreatedStaticMesh == 0)
		{
			// Get all of the points in the static physics geometry.
			float* data = BPhys_GetStaticDebugDrawData(elementCount);
			if (elementCount > 0)
			{
				// Create the static physics mesh (if any)
				physics_create_debug_mesh(data, elementCount, true, 0);
			}

			// Flag set to ensure we only get the static geometry once.
			t.iPhysicsCreatedStaticMesh = 1;

			// Clear the static physics data so we can get the dynamic data.
			BPhys_ClearDebugDrawData();
		}
		else
		{
			// Get the updated debug data.
			t.physicsDebugDrawData = BPhys_GetDynamicDebugDrawData(elementCount);

			if (t.iPhysicsCreatedDynamicMesh == 0)
			{
				// Create the debug mesh if it hasn't already been created.
				physics_create_debug_mesh(t.physicsDebugDrawData, elementCount, false, t.iPhysicsDebugMaxOffset);
				t.iPhysicsDebugMaxOffset = elementCount;
				t.iPhysicsCreatedDynamicMesh = 1;
			}
			else
			{
				// Update all of the debug meshes.
				for (int i = 0; i < t.iPhysicsDebugObjectsToUpdate.size(); i++)
				{
					int iOffsetUpper = elementCount;
					if (i < t.iPhysicsDebugObjectsToUpdate.size() - 1) iOffsetUpper = t.iPhysicsDebugDynamicOffsets[i + 1];

					physics_update_debug_mesh(t.physicsDebugDrawData, elementCount, t.iPhysicsDebugObjectsToUpdate[i],
						t.iPhysicsDebugDynamicOffsets[i], iOffsetUpper);
				}

				if (elementCount > t.iPhysicsDebugMaxOffset)
				{
					// Object has been added to the physics world, so a new physics mesh must be created.
					t.iPhysicsCreatedDynamicMesh = 0;
				}
			}
		}
	}
}

void physics_importer_create_temp()
{
	if (t.importer.collisionshape == 0) t.tshape = 0;   // box
	if (t.importer.collisionshape == 1) t.tshape = 1;   // polygon
	if (t.importer.collisionshape == 2) t.tshape = 2;   // sphere
	if (t.importer.collisionshape == 3) t.tshape = 3;   // cylinder
	if (t.importer.collisionshape == 4) t.tshape = 9;   // hull
	if (t.importer.collisionshape == 5) t.tshape = 21;  // character collission
	if (t.importer.collisionshape == 6) t.tshape = 50;  // tree collision
	if (t.importer.collisionshape == 7) t.tshape = 11;  // no collision

	if (t.tstatic == 1) // now allow physics entities in multiplayer || t.game.runasmultiplayer == 1 ) 
	{
		//  create the physics now
		if (t.tshape >= 1000 && t.tshape < 2000)
		{
			ODECreateStaticBox(t.tphyobj, t.tshape - 1000);
		}
		else if (t.tshape >= 2000 && t.tshape < 3000)
		{
			ODECreateStaticTriangleMesh(t.tphyobj, t.tshape - 2000);
		}
		else if (t.tshape == 1)
		{
			ODECreateStaticBox(t.tphyobj);
		}
		else if (t.tshape == 6)
		{
			ODECreateStaticSphere(t.tphyobj);
		}
		else if (t.tshape == 7)
		{
			ODECreateStaticCylinder(t.tphyobj);
		}
		else if (t.tshape == 2 || t.tshape == 9 || t.tshape == 10)
		{
			if (t.tshape == 2)
			{
				if (t.tcollisionscaling != 100)
				{
					ODECreateStaticTriangleMesh(t.tphyobj, -1, t.tcollisionscaling);
				}
				else
				{
					ODECreateStaticTriangleMesh(t.tphyobj);
				}
			}
			else
			{
				if (t.tshape == 10)
				{
					ODECreateStaticTriangleMesh(t.tphyobj, -1, t.tcollisionscaling, 2);
				}
				else
				{
					ODECreateStaticTriangleMesh(t.tphyobj, -1, t.tcollisionscaling, 1);
				}
			}
		}
		else if (t.tshape == 3)
		{
			physics_setuptreecylinder();
		}
		// tshape 4 is a list of physics objects from the importer
		else if (t.tshape == 4)
		{
			physics_setupimportershapes();
		}
		// if static, restore object before leaving
		if (t.tstatic == 1)
		{
			RotateObject(t.tphyobj, ObjectAngleX(t.tphyobj), t.tstaticfixnewystore_f, ObjectAngleZ(t.tphyobj));
		}
	}
	else
	{
		// objects will fall through Floor (  if they are perfectly sitting on it )
		PositionObject(t.tphyobj, ObjectPositionX(t.tphyobj), ObjectPositionY(t.tphyobj) + 0.1, ObjectPositionZ(t.tphyobj));

		if (t.tshape == 6)
		{
			// Sphere
			ODECreateDynamicSphere(t.tphyobj, t.tweight, t.tfriction, 0.01f);
		}
		else if (t.tshape == 7)
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
			// Dynamic hull composition
			ODECreateDynamicTriangleMesh(t.tphyobj, t.tweight, t.tfriction, -1, 2);
		}
		else
		{
			// box
			ODECreateDynamicBox(t.tphyobj, -1, 0, t.tweight, t.tfriction, -1);
		}
	}
}

int physics_getmaterialindex (float fX, float fZ)
{
	int iMatID = GGTerrain_GetMaterialIndex(fX, fZ) & 0xFF;
	int iMaterialIndex = 0;
	if (iMatID >= 0 && iMatID < 32) iMaterialIndex = g_iMapMatIDToMatIndex[iMatID];
	return iMaterialIndex;
}

// the point and the trunk's normal where the last terrain ray met a tree (d52f3870), read by ODERayTerrain
float g_fTreeRayHitX = 0, g_fTreeRayHitY = 0, g_fTreeRayHitZ = 0;
float g_fTreeRayHitNX = 0, g_fTreeRayHitNY = 1, g_fTreeRayHitNZ = 0;

// where a ray first meets a tree trunk, anywhere on the map (GGTrees_RayCastTrunks: the drawn trees, each trunk as thick as
// its collision cylinder and as tall as the tree); the cylinders round the camera (physics_managevirtualtreecylinders) are
// for the player's movement only. Keeps the entry point and the trunk's normal there for ODERayTerrain
int physics_rayintersecttree (float fX, float fY, float fZ, float fToX, float fToY, float fToZ)
{
	float fHit[6];
	if (GGTrees::GGTrees_RayCastTrunks(fX, fY, fZ, fToX, fToY, fToZ, fHit) == 0) return 0;
	g_fTreeRayHitX = fHit[0]; g_fTreeRayHitY = fHit[1]; g_fTreeRayHitZ = fHit[2];
	g_fTreeRayHitNX = fHit[3]; g_fTreeRayHitNY = fHit[4]; g_fTreeRayHitNZ = fHit[5];
	return 1;
}

// GGMAX 2.41: frames remaining for the harness FIRE_WEAPON trigger-hold (see the consumer at the
// top of physics_player_gatherkeycontrols, the only writer of state.firingmode).
int g_ggFireHoldFrames = 0;

// GGMAX 2.48: harness ZOOM_FIRE — right-mouse hold frames, when (inside that hold) to fire,
// and how many frames the left-mouse press lasts once triggered.
int g_ggZoomHoldFrames = 0;
int g_ggZoomFireAtFrame = 0;
int g_ggZoomFireMouseFrames = 0;
