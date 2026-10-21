extern bool g_bEarlyExcludeMode;
int oldSpaceKey = 0;

extern int NumberOfObjects;
extern int NumberOfGroupsShown;
extern int NumberOfObjectsShown;

extern int iEnterGodMode;
float camx, camy, camz, gcamax, gcamay, gcamaz;
float godcamx, godcamy, godcamz, godcamax, godcamay, godcamaz;


void game_main_loop ( void )
{	
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// this trigger informs the in-game extra thread to begin for one frame (extra stuff done at end (when GPU/Wicked doing its thing)
	// SetGamePaused has no AI for it to serve
	extern bool g_bInGameCPUFrameComplete;
	if ( g_bGamePaused == false ) g_bInGameCPUFrameComplete = true;

	// the engine probes' totals (GetEngineProbe) are per game frame
	extern void WickedCall_ProbeFrame(void);
	WickedCall_ProbeFrame();

	//  Timer (  based movement )
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling game_timeelapsed");
	game_timeelapsed ( );

	//  Music processing
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling music_loop");
	music_loop ( );

	//  Character sound update
	//  110315 - 019 - If spawning in, no sound for the player
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling character_sound_update");
	if ( (t.game.runasmultiplayer  ==  0 || g.mp.noplayermovement  ==  0) && g_bGamePaused == false )
	{
		character_sound_update ( );
	}

	//  Force a shader update to ensure correct shadows are used at start
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling visuals_shaderlevels_update");
	if (  t.visuals.refreshcountdown>0 ) 
	{
		visuals_shaderlevels_update();
		--t.visuals.refreshcountdown;
	}

	// debug reason - why is my SocialVR completely black!!
	bool bSocialVRDebugTABTAB = true;

	// Testgame or Standalone
	// 250316 - when level ends, suspend all logic (including more calls to JumpTolevel or in-game last minute AI stuff)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking levelendingcycle");
	if ( t.game.levelendingcycle == 0 )
	{
		if ( (t.game.gameisexe == 0 || g.gprofileinstandalone == 1) && (t.game.runasmultiplayer == 0 || bSocialVRDebugTABTAB == true)  ) 
		{
			// Test Game Mode
			// Tab Mode (only when not mid-fpswarning)
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking tab key handler");
			if ( t.plrkeySHIFT == 0 && t.plrkeySHIFT2 == 0  )  t.tkeystate15 = KeyState(g.keymap[15]); else t.tkeystate15 = 0;
			if ( t.game.runasmultiplayer == 1 && bSocialVRDebugTABTAB == false ) g.tabmode = 0;
			if ( t.conkit.editmodeactive == 1 )  g.tabmode = 0;
			if ( g.lowfpswarning == 0 || g.lowfpswarning == 3 ) 
			{
				if ( t.tkeystate15 == 0 ) t.tabpress = 0;
				if ( g.globals.riftmode>0 ) 
				{
					if (  t.tkeystate15 == 1 && t.tabpress == 0 ) 
					{
						// 101115 - reset hardware menu if press TAB
						g.tabmodeshowfps = 0;

						if (  g.tabmode == 0 ) 
						{
							game_showmouse ( );
							g.tabmode=2;
						}
						else
						{
							game_hidemouse ( );
							g.tabmode=0;
						}
						t.tabpress=1;
					}
				}
				else
				{
					if (  t.tkeystate15 == 1 && t.tabpress == 0 ) 
					{
						// 101115 - reset hardware menu if press TAB
						g.tabmodeshowfps = 0;

						g.tabmode=g.tabmode+1;
						if (  g.tabmode>2 ) 
						{
							g.tabmode=0;
						}
						if (  g.tabmode<2 ) 
						{
							game_hidemouse ( );
						}
						if (  g.tabmode == 2 ) 
						{
							game_showmouse ( );
						}
						t.tabpress=1;
					}
				}
			}
		}
		else
		{
			//  Standalone Mode
			#ifdef FREETRIALVERSION
			 if ( t.game.gameisexe != 0 )
			 {
				// No lightmapping in free trial version
				t.visuals.generalpromptstatetimer= MAXTimer()+123;
				t.visuals.generalprompt_s="Game Created With Free Trial Version Of GameGuru";
			 }
			#endif
		}

		//  Measure Sync (  to loop start )
		t.game.perf.resttosync += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();

		//  Control slider menus (based on tab page)
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling sliders_loop");
		sliders_loop ( );

		// CTRL+H to hide the hud when testing levels. For reason some the key results don't match what they should be from keymap?
		if (t.game.gameisexe == 0)
		{
			static int iKeyRepeatCounter = 0;
			iKeyRepeatCounter++;
			if (iKeyRepeatCounter > 60)
				iKeyRepeatCounter = 60;
			if (KeyState(g.keymap[35]) && KeyState(g.keymap[29]) && iKeyRepeatCounter >= 60)
			{
				g.tabmodehidehuds = !g.tabmodehidehuds;
				iKeyRepeatCounter = 0;
			}
		}

		//  update all projectiles
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling weapon_projectile_loop");
		if ( g_bGamePaused == false ) weapon_projectile_loop ( );

		//  Prompt
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking prompts");
		if (  t.sky.currenthour_f<1.0 || t.sky.currenthour_f >= 13.0 ) 
		{
			t.pm=int(t.sky.currenthour_f);
			if (  t.pm == 0  )  t.pm = 12; else t.pm = t.pm-12;
			t.pm_s = ""; t.pm_s = t.pm_s + Str(t.pm)+"PM";
		}
		else
		{
			t.pm_s = "";t.pm_s = t.pm_s + Str(int(t.sky.currenthour_f))+"AM";
		}
		t.promptextra_s = ""; t.promptextra_s=t.promptextra_s + "FPS:"+Str(GetDisplayFPS())+" TIME:"+t.pm_s;
		t.game.perf.misc += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();

		if (t.playercontrol.thirdperson.enabled != 1)
		{
			if (iEnterGodMode == 1)
			{
				godcamx = camx = CameraPositionX(t.terrain.gameplaycamera);
				godcamy = camy = CameraPositionY(t.terrain.gameplaycamera);
				godcamz = camz = CameraPositionZ(t.terrain.gameplaycamera);
				godcamax = gcamax = CameraAngleX(t.terrain.gameplaycamera);
				godcamay = gcamay = CameraAngleY(t.terrain.gameplaycamera);
				godcamaz = gcamaz = CameraAngleZ(t.terrain.gameplaycamera);
				iEnterGodMode++;
			}
			if (iEnterGodMode == 2)
			{
				if (g.luacameraoverride != 1 && g.luacameraoverride != 3)
				{
					PositionCamera(t.terrain.gameplaycamera, camx, camy, camz);
					RotateCamera(t.terrain.gameplaycamera, gcamax, gcamay, gcamaz);
					t.tobj = t.aisystem.objectstartindex;
					if (ObjectExist(t.tobj))
						PositionObject(t.tobj, camx, camy, camz);
				}
			}
		}


		//PE: Moved of of thread, none of the object functions are 100% thread safe.
		if (BPhys_GetDebugDrawerMode() != 0)
		{
			physics_render_debug_meshes();
		}

		//  loop physics
		if (  t.hardwareinfoglobals.nophysics == 0 && g_bGamePaused == false )
		{
			// Handle physics
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling physics_loop");
			auto range2 = wiProfiler::BeginRangeCPU("Update - Logic - Physics");

			// reinstated physics into main CPU thread for stability over performance
			physics_loop ( );

			// a knocked out character's entity is where his ragdoll's pelvis now is
			ragdoll_knockoutloop ( );

			// special mode for testing
			if (iEnterGodMode != 2)
			{
				physics_player_control (); // has LUA calls inside it
			}

			physics_player_handledeath (); // handles sound, so keep in main thread
			wiProfiler::EndRange(range2);

			// read all slider values for player
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling sliders readall");
			t.slidersmenuindex=t.slidersmenunames.player; sliders_readall ( );

			//  Do weapon attachments AFTER physics moved objects (and if char killed off)
			for ( g.charanimindex = 1 ; g.charanimindex <= g.charanimindexmax; g.charanimindex++ )
			{
				// detect collection of dropped guns
				t.e = t.charanimstates[g.charanimindex].originale;
				if ( t.e > 0 ) entity_monitorattachments ( );
			}

			//  Construction Kit control
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling conkit_loop");
			//conkit_loop ( );
		}
		t.game.perf.physics += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();

		// In-Game Mode (moved from above so LUA is AFTER physics)
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking in-game edit mode");

		if ( t.conkit.editmodeactive == 0 )
		{
			// if third person, trick AI by moving camera to protagonist location
			if ( t.playercontrol.thirdperson.enabled == 1 ) 
			{
				t.playercontrol.thirdperson.storecamposx = CameraPositionX(t.terrain.gameplaycamera);
				t.playercontrol.thirdperson.storecamposy = CameraPositionY(t.terrain.gameplaycamera);
				t.playercontrol.thirdperson.storecamposz = CameraPositionZ(t.terrain.gameplaycamera);
				t.tobj = t.aisystem.objectstartindex;
				if ( g.luacameraoverride != 1 && g.luacameraoverride != 3 )
				{
					PositionCamera ( t.terrain.gameplaycamera, ObjectPositionX(t.tobj), ObjectPositionY(t.tobj), ObjectPositionZ(t.tobj) );
				}
			}

			// All Entity logic
			t.ttempoverallaiperftimerstamp=PerformanceTimer();
			if ( t.hardwareinfoglobals.noai == 0 ) 
			{
				// LUA Logic
				auto range1 = wiProfiler::BeginRangeCPU("Update - Logic - LUA");
				lua_loop ( );
				wiProfiler::EndRange(range1);

				// Entity Logic
				auto range2 = wiProfiler::BeginRangeCPU("Update - Logic - Objects");
				t.game.perf.ai1 += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();
				if ( g_bGamePaused == false )
				{
					entity_loop ( );
					entity_loopanim ( );
				}
				t.game.perf.ai2 += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();
				wiProfiler::EndRange(range2);

				// Update all AI and Characters and VWeaps
				auto range3 = wiProfiler::BeginRangeCPU("Update - Logic - AI");
				if ( t.aisystem.processlogic == 1 && g_bGamePaused == false )
				{
					if ( t.visuals.debugvisualsmode<100 ) 
					{
						darkai_loop ( );
					}

					// handle anmy token drops once per cycle
					g_RecastDetour.ManageTokenDropSystem(g.timeelapsed_f);
				}
				wiProfiler::EndRange(range3);

				// handle any AI stuff related to recastretour
				if ( g_bGamePaused == false )
				{
					auto range4 = wiProfiler::BeginRangeCPU("Update - Logic - Navmesh");
					game_updatenavmeshsystem();
					wiProfiler::EndRange(range4);
				}
			}
			t.game.perf.ai += PerformanceTimer()-t.ttempoverallaiperftimerstamp;
		}

		// if third person, restore camera from protag-cam trick
		if ( t.playercontrol.thirdperson.enabled == 1 ) 
		{
			if ( g.luacameraoverride != 1 && g.luacameraoverride != 3 )
			{
				PositionCamera (  t.terrain.gameplaycamera,t.playercontrol.thirdperson.storecamposx,t.playercontrol.thirdperson.storecamposy,t.playercontrol.thirdperson.storecamposz );
			}
		}
		else
		{
			if (iEnterGodMode == 2)
			{
				if (g.luacameraoverride != 1 && g.luacameraoverride != 3)
				{
					//PE: Move godcam here.

					PositionCamera(t.terrain.gameplaycamera, godcamx, godcamy, godcamz);
					RotateCamera(t.terrain.gameplaycamera, godcamax, godcamay, godcamaz);
					void GodCameraControl(float& x, float& y, float& z, float& ax, float& ay, float& az);
					GodCameraControl(godcamx, godcamy, godcamz, godcamax, godcamay, godcamaz);
					t.tobj = t.aisystem.objectstartindex;
					if (ObjectExist(t.tobj))
						PositionObject(t.tobj, camx, camy, camz);

				}
			}

		}

		//  Gun control
		if ( t.hardwareinfoglobals.noguns == 0 && g_bGamePaused == false )
		{
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling gun_manager");
			gun_manager ( );
			t.slidersmenuindex=t.slidersmenunames.weapon ; sliders_readall ( );
		}
		t.game.perf.gun += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();
	}

	//  update all particles and emitters
	if ( g_bGamePaused == false )
	{
		update_env_particles();
		ravey_particles_update();
	}

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

	//  Decal control
	decalelement_control();

	// bullethole manegement
	bulletholes_update();

	//  Steam call moved here as camera changes need to be BEFORE the shadow update
	if (  t.game.runasmultiplayer == 1 ) 
	{
		// debug tracing
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling mp_gameLoop");

		// run multiplayer logic
		mp_gameLoop ( );

		// mp logic can trigger a new avatar to be loaded and created dynamically
		game_scanfornewavatars ( true );
	}

	// If the camera is spun round quick, redraw shadows immediately
	// 281116 - int tMouseMove = MouseMoveX(); - yep, this killed fluid mousemoveX, thanks for that Lee!
	int tMouseMove = t.cammousemovex_f;
	if (  t.hardwareinfoglobals.noterrain == 0 ) 
	{
		//Dave Performance, calling update on veg and terrain shadow every 4 frames rather than every frame
		//Grass every other frame
		//Gets me 10fps increase on my machine		
		static bool terrainvegdelay = true;
		t.game.perf.terrain1 += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();
		t.game.perf.terrain2 += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();
	}
	if (  t.hardwareinfoglobals.nosky == 0 ) 
	{
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling sky_loop");
		sky_loop ( );
	}
	t.game.perf.terrain3 += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();

	//  Game Debug Prompts
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"assembling debug prompts");
	if (  t.aisystem.showprompts == 1 ) 
	{
		pastebitmapfont("DEBUG PROMPTS",8,20,1,255) ; t.i=1;
		t.strwork = ""; t.strwork = t.strwork + "NUMBER OF CHARACTERS:"+Str(g.charanimindexmax);
		pastebitmapfont( t.strwork.Get() ,8,20+(t.i*25),1,255) ; ++t.i;
		for ( g.charanimindex = 1 ; g.charanimindex<=  g.charanimindexmax; g.charanimindex++ )
		{
			t.tdesc_s = ""; t.tdesc_s = t.tdesc_s + "CHAR:"+Str(g.charanimindex);
			t.tdesc_s=t.tdesc_s+"  AIstate_s="+AIGetEntityState(t.charanimstates[g.charanimindex].obj);
			t.tdesc_s=t.tdesc_s+"  t.moving="+Str(t.charanimcontrols[g.charanimindex].moving);
			t.tdesc_s=t.tdesc_s+"  t.ducking="+Str(t.charanimcontrols[g.charanimindex].ducking);
			pastebitmapfont(t.tdesc_s.Get(),8,20+(t.i*25),1,255) ; ++t.i;
			t.tdesc_s="";
			t.tdesc_s=t.tdesc_s+"  plrvisible="+Str(t.entityelement[t.charanimstates[g.charanimindex].e].plrvisible);
			t.tdesc_s=t.tdesc_s+"  health="+Str(t.entityelement[t.charanimstates[g.charanimindex].e].health);
			t.tdesc_s=t.tdesc_s+"  playcsi="+Str(t.charanimstates[g.charanimindex].playcsi);
			t.tdesc_s=t.tdesc_s+"  t.charseq.mode="+Str(t.charseq[t.charanimstates[g.charanimindex].playcsi].mode);
			pastebitmapfont(t.tdesc_s.Get(),8,20+(t.i*25),1,255) ; ++t.i;
			++t.i;
		}
	}

	// Handle occlusion if active
	if ( g.globals.occlusionmode == 1 ) 
	{
		// VR software cannot use occlusion at the moment
		if ( g.vrqcontrolmode == 0 ) //g.vrglobals.GGVREnabled == 0 )
		{
			// detect velocity of XZ motion of player and advance 'virtual camera' ahead of real camera
			// in order to give occluder time to reveal visible objects in advance of getting there
			if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking occlusionp");
			float plrx = CameraPositionX(0);
			float plrz = CameraPositionZ(0);
			g_fOccluderCamVelX = plrx - g_fOccluderLastCamX;
			g_fOccluderCamVelZ = plrz - g_fOccluderLastCamZ;
			g_fOccluderLastCamX = plrx;
			g_fOccluderLastCamZ = plrz;
			if ( fabs(g_fOccluderCamVelX)>0.01f || fabs(g_fOccluderCamVelZ)>0.01f )
			{
				float fDDMultiplier = 20.0f / sqrt(fabs(g_fOccluderCamVelX*g_fOccluderCamVelX)+fabs(g_fOccluderCamVelZ*g_fOccluderCamVelZ));
				g_fOccluderCamVelX *= fDDMultiplier;
				g_fOccluderCamVelZ *= fDDMultiplier;
				if ( g_fOccluderCamVelX < -20.0f ) g_fOccluderCamVelX = -20.0f;
				if ( g_fOccluderCamVelZ < -20.0f ) g_fOccluderCamVelZ = -20.0f;
				if ( g_fOccluderCamVelX > 20.0f ) g_fOccluderCamVelX = 20.0f;
				if ( g_fOccluderCamVelZ > 20.0f ) g_fOccluderCamVelZ = 20.0f;
			}
			else
			{
				g_fOccluderCamVelX = 0.0f;
				g_fOccluderCamVelZ = 0.0f;
			}
			// show me this
			CPUShiftXZ ( g_fOccluderCamVelX, g_fOccluderCamVelZ );

			CPU3DSetCameraFar ( t.visuals.CameraFAR_f );
			if ( g_pOccluderThread == NULL )
			{
				CPU3DOcclude (  );
				g_hOccluderBegin = CreateEvent ( NULL, FALSE, FALSE, NULL );
				g_hOccluderEnd   = CreateEvent ( NULL, FALSE, FALSE, NULL );
				g_pOccluderThread = new cOccluderThread;
				g_pOccluderThread->Start ( );
			}
			g_occluderOn = true;
		}
	}
	t.game.perf.occlusion += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();

	// Final post processing step

	// Render pre-terrain post process cameras (includes lightray rendering)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling postprocess_preterrain");
	postprocess_preterrain ( );

	//  explosions and fire
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling draw_particles");
	if ( g_bGamePaused == false ) draw_particles();

	//  handle fade out for level progression
	if (  t.game.levelendingcycle > 0 ) 
	{
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling game_end_of_level_check");
		game_end_of_level_check ( );
	}

	//  Post process and visual settings system
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling visuals_loop");
	visuals_loop ( );
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling lighting_loop");
	lighting_loop ( );
	t.game.perf.postprocessing += PerformanceTimer()-g.gameperftimestamp ; g.gameperftimestamp=PerformanceTimer();

	// Check for player guns switched off
	if ( g.noPlayerGuns && g_bGamePaused == false )
	{
		if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling physics_no_gun_zoom");
		physics_no_gun_zoom ( );
		if ( g.autoloadgun != 0 ) { g.autoloadgun=0 ; gun_change ( ); }
	}

	//  Update HUD Layer objects (jetpack)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling hud_updatehudlayerobjects");
	hud_updatehudlayerobjects ( );

	// trigger screen to be grabbed for a HUD image
	bool bGrabSceneToStoreInImage = true;
	if (bGrabSceneToStoreInImage == true)
	{
		extern void GrabBackBufferForAnImage (void);
		GrabBackBufferForAnImage();
	}
}

extern int howManyOccluders;
extern int howManyOccludersDrawn;
extern int howManyOccludees;
extern int howManyOccludeesHidden;
extern int howManyMarkers;
extern float trackingSize;

void game_dynamicRes()
{
	// If dynamic res is disabled via setup ini, return out
	if ( t.DisableDynamicRes ) return;

	if ( g_pGlob->dwNumberOfPrimCalls > 500 && GetDisplayFPS() < 60 )
		t.bHiResMode = false;
	else if ( g_pGlob->dwNumberOfPrimCalls < 200 && GetDisplayFPS() > 60 )
		t.bHiResMode = true;

	if ( t.visuals.debugvisualsmode == 20 ) t.bHiResMode = false;
	if ( t.visuals.debugvisualsmode == 21 ) t.bHiResMode = true;

	if ( t.bHiResMode != t.bOldHiResMode )
	{
		SetCameraHiRes ( t.bHiResMode );
		t.bOldHiResMode = t.bHiResMode;
	}
}

extern float smallDistanceMulti;

void game_sync ( void )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	//  Work out overall time spent per cycle
	t.game.perf.overall += PerformanceTimer()-g.gameperfoveralltimestamp ; g.gameperfoveralltimestamp=PerformanceTimer();

	//  HUD Damage Display
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling controlblood");
	controlblood();
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling controldamagemarker");
	controldamagemarker();

	//  Slider menus rendered last
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling sliders_draw");
	sliders_draw ( );

	//  Detect if FPS drops (only for single player - never for MP games)
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking show hide mouse");
	if (  t.game.runasmultiplayer == 0 && g.globals.hidelowfpswarning == 0 && g.tabmode == 0 && g.ghardwareinfomode == 0 && t.visuals.generalpromptstatetimer == 0 ) 
	{
		if ( t.conkit.cooldown>0 )  
		{
			// cooldown ensures this FPS warning only happens when cooled off
			--t.conkit.cooldown;
			g.lowfpstarttimer = MAXTimer();
		}
		if (  t.conkit.editmodeactive == 0 && t.conkit.cooldown == 0 ) 
		{
			if (  g.lowfpswarning == 0 ) 
			{
				if ( (unsigned long)MAXTimer()>g.lowfpstarttimer+2000 ) 
				{
					if ( GetDisplayFPS()<20 ) 
					{
						g.lowfpswarning=1;
						game_showmouse ( );
					}
				}
			}
			else
			{
				if (  g.lowfpswarning == 2 ) 
				{
					game_hidemouse ( );
					while ( MouseClick() != 0 ) { }
					g.lowfpswarning=3;
				}
			}
		}
	}

	//  Only render main and postprocess camera (not paint camera, reflection or lightray cameras)
	//  for globals.riftmode, left and right eyes are rendered in the _postprocess_preterrain step
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"applying sync mask");
	t.tmastersyncmask=0;
	SyncMask (  t.tmastersyncmask+(1<<3)+(1) );

	//  Update RealSense if any
	///realsense_loop ( );

	//  Update screen
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling sync");
	Sync (  );
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling game_dynamicRes");
	game_dynamicRes();

	//Dave Performance - let the occluder thread know it is okay to begin
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling CPU3DOcclude");
	CPU3DOcclude (  );
	if ( g_hOccluderBegin ) SetEvent ( g_hOccluderBegin );

	t.game.perf.synctime += (PerformanceTimer()-g.gameperftimestamp) ; g.gameperftimestamp=PerformanceTimer();

	//  collect main Sync (  statistics )
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"checking statistics");
	t.mainstatistic1=GetStatistic(1);
	t.mainstatistic5=GetStatistic(5);

	//  detect and slow down action
	if (  t.visuals.debugvisualsmode == 4 ) 
	{
		if (  ReturnKey() == 1 ) 
		{
			t.player[1].health=50000;
			physics_pausephysics ( );
			SleepNow (  200 );
			physics_resumephysics ( );
		}
	}

	//  Work out performance metrics
	if ( g.gproducelogfiles == 2 ) timestampactivity(0,"calling sliders readall");
	t.slidersmenuindex=t.slidersmenunames.performance  ; sliders_readall ( );
}

void game_main_stop ( void )
{
	// Rest any ingame variables
	if ( t.conkit.entityeditmode != 0 || t.conkit.editmodeactive == 1 ) 
	{
		//conkitedit_switchoff ( );
	}
}

void game_jump_to_level_from_lua (int iResetStates)
{
	if (  t.game.gameisexe == 1 ) 
	{
		if (  t.game.gameloop == 1 && t.game.levelendingcycle  ==  0 ) 
		{
			t.game.jumplevelresetstates = iResetStates;
			t.game.jumplevel_s = t.tleveltojump_s;
			t.game.levelendingcycle = 4000;
		}
	}
	else
	{
		t.s_s = "" ; t.s_s = t.s_s+"Jump To Level : "+t.tleveltojump_s ; lua_prompt ( );
	}
}

void game_finish_level_from_lua ( void )
{
	if (  t.game.gameisexe == 1 ) 
	{
		if (  t.game.gameloop == 1 && t.game.levelendingcycle  ==  0 ) 
		{
			t.game.levelendingcycle = 4000;
		}
	}
	else
	{
		t.s_s="Game Completed"  ; lua_prompt ( );
	}
}

void game_end_of_level_check ( void )
{
	// end of level fade out
	t.game.levelendingcycle = t.game.levelendingcycle - (g.timeelapsed_f * 400.0);// 200.0); slicker!
	t.huddamage.immunity=1000;
	if (  t.game.levelendingcycle  <=  0 ) 
	{
		t.game.gameloop=0;
		t.game.levelendingcycle = 0;
		t.postprocessings.fadeinenabled = 1;
		//PE: Delay screen update , some frames so we only see the "Game Over Screen".
		extern int iBlockRenderingForFrames;
		iBlockRenderingForFrames = 10;
		extern bool g_bNoSwapchainPresent;
		g_bNoSwapchainPresent = true; 
	}

	// control fade out of screen
	if (t.postprocessings.fadeinenabled)
	{
		t.postprocessings.fadeinvalue_f = t.game.levelendingcycle / 4500.0; //PE: fadeinvalue_f is a bit delayed , so just count down faster.
		t.postprocessings.fadeinvalueupdate = 1;
	}

	// fade audio (can be slow, so only do at end)
	if (t.game.levelendingcycle <= 100)
	{
		if (t.audioVolume.music > t.postprocessings.fadeinvalue_f * 100.0)  t.audioVolume.music = t.postprocessings.fadeinvalue_f * 100.0;
		if (t.audioVolume.sound > t.postprocessings.fadeinvalue_f * 100.0)  t.audioVolume.sound = t.postprocessings.fadeinvalue_f * 100.0;
		audio_volume_update ();
	}
}

void GodCameraControl(float &x, float &y, float &z, float& ax, float& ay, float& az)
{
	ImGuiIO& io = ImGui::GetIO();
	
	if (1)
	{
		if (ImGui::IsMouseDown(1))
		{
			float speed = 6.0f;
			float xdiff = ImGui::GetIO().MouseDelta.x / speed;
			float ydiff = ImGui::GetIO().MouseDelta.y / speed;
			ax += ydiff;
			ay += xdiff;
			if (ax > 180.0f)  ax = ax - 360.0f;
			if (ax < -89.999f)  ax = -89.999f;
			if (ax > 89.999f)  ax = 89.999f;
			RotateCamera(ax, ay, 0);
		}

	}

	if(1)
	{
		static float fAccelerationTimer = 0.0f;
		if (t.inputsys.keyup == 1)  t.plrkeyW = 1; else t.plrkeyW = 0;
		if (t.inputsys.keyleft == 1)  t.plrkeyA = 1; else t.plrkeyA = 0;
		if (t.inputsys.keydown == 1)  t.plrkeyS = 1; else t.plrkeyS = 0;
		if (t.inputsys.keyright == 1)  t.plrkeyD = 1; else t.plrkeyD = 0;

		//  mouse wheel mimmics W and S when no CONTROL key pressed (170616 - but not when in EBE mode as its used for grid layer control)
		int usingWheel = 0;
		t.traise_f = 0.0;
		if (t.inputsys.keyshift == 1 || io.KeyShift)
		{

			fAccelerationTimer += g.timeelapsed_f * 0.005f;
			if (fAccelerationTimer > 1.0f) fAccelerationTimer = 1.0f;
			t.tffcspeed_f = 10.0 * g.timeelapsed_f;
		}
		else
		{
			fAccelerationTimer = 0.0f;
			if (t.inputsys.keycontrol == 1 || io.KeyCtrl)
			{
				// reduce this until we sort out scale!
				t.tffcspeed_f = 1.0 * g.timeelapsed_f;
			}
			else
			{
				// reduce this until we sort out scale!
				t.tffcspeed_f = 5.0 * g.timeelapsed_f;
			}
		}

		
		if(1)
		{
			float fHeightAtThisPartOfTerrain = BT_GetGroundHeight(t.terrain.TerrainID, x, z);
			float height = y - fHeightAtThisPartOfTerrain;
			if (height < 0) height = 0;
			float modifier = height * height * 0.00001f + 2 + 50 * fAccelerationTimer;
			if (modifier > 50) modifier = 50;
			if (modifier < 2) modifier = 2;
			t.tffcspeed_f *= modifier;
		}

		if (t.inputsys.k_s == "e" || ImGui::IsKeyDown(69))  t.traise_f = -90;
		if (t.inputsys.k_s == "q" || ImGui::IsKeyDown(81))  t.traise_f = 90;

		PositionCamera(x, y, z);

		if (t.plrkeyW == 1 || ImGui::IsKeyDown(87) || ImGui::IsKeyDown(38))
			MoveCamera(t.tffcspeed_f);
		if (t.plrkeyS == 1 || ImGui::IsKeyDown(83) || ImGui::IsKeyDown(40))
			MoveCamera(t.tffcspeed_f * -1);

		if (t.plrkeyA == 1 || ImGui::IsKeyDown(65) || ImGui::IsKeyDown(37)) { RotateCamera(0, ay - 90, 0); MoveCamera(t.tffcspeed_f); }
		if (t.plrkeyD == 1 || ImGui::IsKeyDown(68) || ImGui::IsKeyDown(39)) { RotateCamera(0, ay + 90, 0); MoveCamera(t.tffcspeed_f); }

		if (t.traise_f != 0) { RotateCamera(t.traise_f, 0, 0); MoveCamera(t.tffcspeed_f); }
		if (MouseClick() == 4)
		{
			//  new middle mouse panning
			RotateCamera(0, ay, 0);
			MoveCamera(t.cammousemovey_f * -2);
			if (t.cammousemovex_f < 0) { RotateCamera(0, ay - 90, 0); MoveCamera(abs(t.cammousemovex_f * 2)); }
			if (t.cammousemovex_f > 0) { RotateCamera(0, ay + 90, 0); MoveCamera(t.cammousemovex_f * 2); }
		}
		x = CameraPositionX();
		y = CameraPositionY();
		z = CameraPositionZ();
	}

	//  ensure camera NEVER goes into Floor (  )
	if (0)
	{
		t.tcurrenth_f = BT_GetGroundHeight(t.terrain.TerrainID, x, z) + 10.0;
		if (y < t.tcurrenth_f)
		{
			y = t.tcurrenth_f;
		}

		if (t.editorfreeflight.s.y_f < t.tcurrenth_f)
		{
			t.editorfreeflight.s.y_f = t.tcurrenth_f;
		}
	}

	PositionCamera(x, y, z);
	RotateCamera(ax, ay, 0);

}
