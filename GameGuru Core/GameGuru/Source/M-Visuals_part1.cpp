void visuals_updateobjectmasks ( void )
{
	// actually used relfection value
	int iUseThisReflectionValue = t.visuals.reflectionmode;
	if ( g.gvrmode != 0 && iUseThisReflectionValue > 10 ) 
		iUseThisReflectionValue = 10;

	// can be called from _loop and also from terrain (reflection update when not looking at water pixels)
	if ( 1 ) 
	{
		if (g.globals.riftmode > 0 || (g.vrglobals.GGVREnabled > 0 && g.vrglobals.GGVRUsingVRSystem == 1))
		{
			if (g.vrglobals.GGVREnabled > 0)
			{
				// VIVE = left and right eye cameras, and camera zero for editor
				t.tmaskforcamerasraw = 00001 + (1 << t.glefteyecameraid) + (1 << t.grighteyecameraid);
			}
			else
			{
				// RIFT = left and right eye cameras too (but not camera zero)
				// left and right eye cameras too (but not camera zero)
				t.tmaskforcamerasraw = 00000 + (1 << t.glefteyecameraid) + (1 << t.grighteyecameraid);
			}
		}
		else
		{
			t.tmaskforcamerasraw=00001;
		}
		t.tmaskforcamerasnoreflectionlightrayshadowsflag=t.tmaskforcamerasraw;
		t.tmaskforcamerasnoshad=t.tmaskforcamerasraw;
		if (  t.visuals.shadowmode>0 || t.game.onceonlyshadow == 1 ) 
		{
			t.tmaskforcameras=t.tmaskforcamerasraw+(1<<31);
		}
		else
		{
			t.tmaskforcameras=t.tmaskforcamerasraw;
		}
		t.game.onceonlyshadow=0;
		t.tmaskforcamerasnoreflectionlightrayflag=t.tmaskforcameras;
		if (  t.visuals.lightraymode>0 ) 
		{
			t.tmaskforcameras=t.tmaskforcameras+16;
			t.tmaskforcamerasnoshad=t.tmaskforcamerasnoshad+16;
		}
		if ( iUseThisReflectionValue > 0 ) // t.visuals.reflectionmodemodified>0 ) 
		{
			t.tmaskforcameras=t.tmaskforcameras+4;
			t.tmaskforcamerasnoshad=t.tmaskforcamerasnoshad+4;
		}

		//  All game objects
		for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
		{
			visuals_updatespecificobjectmasks ( t.e, t.entityelement[t.e].obj );
		}

		//  grass can be excluded with specific reflection mode
		if ( iUseThisReflectionValue <= 99 ) 
		{
			//  ensure grass does not render in reflection render
			t.tmask=t.tmaskforcamerasnoreflectionlightrayshadowsflag;
			if ( iUseThisReflectionValue == 0 ) 
			{
				//  no water at all
				if( g.globals.forcenowaterreflection == 0 )
					t.hardwareinfoglobals.nowater=2;
				//  set all entities to never reflect
				for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
				{
					t.entityelement[t.e].donotreflect=1;
				}
			}
			else
			{
				//  restore water feature
				if (  t.hardwareinfoglobals.nowater == 2  )  t.hardwareinfoglobals.nowater = 0;
				//  work out which entities should reflect in reflection render
				t.thideatthissize=(100-iUseThisReflectionValue)*20;
				for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
				{
					t.entid=t.entityelement[t.e].bankindex;
					if (  t.entityprofile[t.entid].ismarker == 0 ) 
					{
						t.entityelement[t.e].donotreflect=1;
						t.obj=t.entityelement[t.e].obj;
						if (  t.obj>0 ) 
						{
							if (  ObjectExist(t.obj) == 1 ) 
							{
								t.tsizex=ObjectSizeX(t.obj,1);
								t.tsizey=ObjectSizeY(t.obj,1);
								t.tsizez=ObjectSizeZ(t.obj,1);
								if (  t.tsizex>t.thideatthissize || t.tsizey>t.thideatthissize || t.tsizez>t.thideatthissize ) 
								{
									t.entityelement[t.e].donotreflect=0;
								}
							}
						}
					}
				}
				//  and also go through any LM objects with same check as above
				if (  t.visuals.shaderlevels.lighting == 1 && t.game.set.ismapeditormode == 0 ) 
				{
					for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
					{
						if (  t.e <= ArrayCount(t.lmsceneobj) ) 
						{
							if (  t.lmsceneobj[t.e].startobj>0 ) 
							{
								for ( t.tobj = t.lmsceneobj[t.e].startobj; t.tobj<= t.lmsceneobj[t.e].finishobj; t.tobj++ )
								{
									if (  ObjectExist(t.tobj) == 1 ) 
									{
										t.tsizex=ObjectSizeX(t.tobj,1);
										t.tsizey=ObjectSizeY(t.tobj,1);
										t.tsizez=ObjectSizeZ(t.tobj,1);
										if (  t.tsizex>t.thideatthissize || t.tsizey>t.thideatthissize || t.tsizez>t.thideatthissize ) 
										{
											t.entityelement[t.e].donotreflect=0;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{
			//  restore water feature
			if (  t.hardwareinfoglobals.nowater == 2  )  t.hardwareinfoglobals.nowater = 0;
			//  grass in reflection render
			t.tmask=t.tmaskforcamerasnoreflectionlightrayshadowsflag+4;
			//  set all entities to allow full reflection
			for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
			{
				t.entityelement[t.e].donotreflect=0;
			}
		}
		SetTerrainMask (  t.tmask );

		//  hide LM objects from shadow render if pre-bake(lighting=1)
		if (  t.visuals.shaderlevels.lighting == 1 && t.game.set.ismapeditormode == 0 ) 
		{
			for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
			{
				if (  t.e <= ArrayCount(t.lmsceneobj) ) 
				{
					if (  t.lmsceneobj[t.e].startobj>0 ) 
					{
						for ( t.tobj = t.lmsceneobj[t.e].startobj; t.tobj <= t.lmsceneobj[t.e].finishobj; t.tobj++ )
						{
							if (  ObjectExist(t.tobj) == 1 ) 
							{
								t.tsize=ObjectSize(t.tobj,1);
								t.tsmallmask=t.tmaskforcamerasnoreflectionlightrayshadowsflag;
								if (  t.tsize>25 ) 
								{
									if (  t.visuals.lightraymode>0 ) 
									{
										t.tsmallmask=t.tsmallmask+16;
									}
								}
								if (  t.entityelement[t.e].donotreflect == 1 ) 
								{
									if (  t.visuals.lightraymode>0 ) 
									{
										t.tlargemask=t.tmaskforcamerasnoreflectionlightrayshadowsflag+16;
									}
									else
									{
										t.tlargemask=t.tmaskforcamerasnoreflectionlightrayshadowsflag;
									}
								}
								else
								{
									t.tlargemask=t.tmaskforcamerasnoshad;
								}
								if (  t.tsize<100 ) 
								{
									SetObjectMask (  t.tobj,t.tsmallmask,0,0,0 );
								}
								else
								{
									SetObjectMask (  t.tobj,t.tlargemask,0,0,0 );
								}
							}
						}
					}
				}
			}
			if (  g.lightmappedterrainoffset != -1 ) 
			{
				for ( t.tlmobj2 = g.lightmappedterrainoffset ; t.tlmobj2<=  g.lightmappedterrainoffsetfinish; t.tlmobj2++ )
				{
					if (  ObjectExist(t.tlmobj2) == 1 ) 
					{
						if ( g.iLightmappingExcludeTerrain == 0 )
						{
							if (  ObjectSize(t.tlmobj2,1)<100 ) 
							{
								SetObjectMask (  t.tlmobj2,t.tmaskforcamerasnoreflectionlightrayshadowsflag,0,0,0 );
							}
							else
							{
								SetObjectMask (  t.tlmobj2,t.tmaskforcamerasnoshad,0,0,0 );
							}
						}
						else
						{
							SetObjectMask ( t.tlmobj2,0,0,0,0 );
						}
					}
				}
			}
		}
	}
}

// Moved this code into its own function so lua can call it too rather than refreshing all visuals
void visuals_CheckSetGlobalDepthSkipSystem ( void )
{
	if ( t.visuals.DepthOfFieldIntensity_f==0.0f && t.visuals.MotionIntensity_f==0.0f && t.visuals.SAOIntensity_f==0.0f )
		SetGlobalDepthSkipSystem ( true );
	else
		SetGlobalDepthSkipSystem ( false );
}

void visuals_loop ( void )
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	// Shortcut-keys to adjust visual settings
	if ( t.conkit.editmodeactive == 0 ) 
	{
		if ( ScanCode() == 0  )  t.visuals.pressed = 0;
		if ( t.visuals.pressed == 0 ) 
		{
			if ( t.visuals.pressed == 1 ) 
			{
				if ( t.visuals.promptstatetimer == 0 ) 
				{
					t.visuals.showpromptssavestate=t.aisystem.showprompts;
				}
				t.visuals.promptstatetimer= MAXTimer()+3000;
			}
			if ( KeyState(g.keymap[87]) == 1 ) 
			{ 
				// when F11 pressed, scroll through FPS show modes (and hardware info panel)
				t.visuals.pressed = 1; 
				g.tabmodeshowfps = g.tabmodeshowfps + 1;
				if ( g.tabmodeshowfps > 2 ) g.tabmodeshowfps = 0;
			}
		}
	}
	if ( t.visuals.promptstatetimer > 0 ) 
	{
		t.tvisualprompt_s="";
		t.tvisualprompt_s=t.tvisualprompt_s+" : "+StrEx(t.visuals.value_f,2);
		if ( MAXTimer()>t.visuals.promptstatetimer ) 
		{
			t.visuals.promptstatetimer=0;
			t.aisystem.showprompts=t.visuals.showpromptssavestate;
		}
		t.tvisualalpha=255;
		if ( MAXTimer()>t.visuals.promptstatetimer-255  )  t.tvisualalpha = (t.visuals.promptstatetimer-MAXTimer());
		if ( t.tvisualalpha<0  )  t.tvisualalpha = 0;
		if ( t.tvisualalpha>255  )  t.tvisualalpha = 255;
		pastebitmapfont(t.tvisualprompt_s.Get(),20,(GetDisplayHeight()-50),1,t.tvisualalpha);
	}

	// General prompt
	if ( t.visuals.generalpromptstatetimer>0 ) 
	{
		if ( t.postprocessings.fadeinvalue_f == 1.0 ) 
		{
			t.tvisualprompt_s=t.visuals.generalprompt_s;
			if ( MAXTimer()>t.visuals.generalpromptstatetimer ) 
			{
				t.visuals.generalpromptstatetimer=0;
				t.visuals.generalpromptalignment=0;
			}
			t.tvisualalpha=255;
			if ( MAXTimer()>t.visuals.generalpromptstatetimer-255  )  t.tvisualalpha = (t.visuals.generalpromptstatetimer-MAXTimer());
			if ( t.tvisualalpha<0  )  t.tvisualalpha = 0;
			if ( t.tvisualalpha>255  )  t.tvisualalpha = 255;
			t.txwidth=getbitmapfontwidth(t.tvisualprompt_s.Get(),1);
			if ( t.visuals.generalpromptalignment == 1  )  t.tprmpty = GetDisplayHeight()-50; else t.tprmpty = 50;
			pastebitmapfont(t.tvisualprompt_s.Get(),(GetDisplayWidth()-t.txwidth)/2,t.tprmpty,1,t.tvisualalpha);
		}
		else
		{
			t.visuals.generalpromptstatetimer= MAXTimer()+3000;
		}
	}

	//  Apply visual settings when change detected
	if ( t.visuals.pressed == 1 || t.visuals.refreshshaders == 1 ) 
	{
		// One refresh per request
		t.visuals.refreshshaders=0;

		// 091115 - new system to SKIP the depth render of all qualifying shader (using a pass called "RenderDepthPixelsPass")
		visuals_CheckSetGlobalDepthSkipSystem();

		// Code to update ALL OBJECT MASKS to decide which cameras will get them
		// Done on a refresh as this process is expensive (20% of empty level)
		if ( 1 ) 
		{
			// Set the object masks based on graphic options (uses reflectionmodemodified)
			visuals_updateobjectmasks ( );

			// reflection settings can hide/show plane of water
			terrain_updatewaterphysics ( );
			if ( t.hardwareinfoglobals.nowater != 0 ) 
			{
				if ( ObjectExist(t.terrain.objectstartindex+5) == 1  )  HideObject (  t.terrain.objectstartindex+5 );
			}
			else
			{
				if ( ObjectExist(t.terrain.objectstartindex+5) == 1  )  ShowObject (  t.terrain.objectstartindex+5 );
			}

			// However, ensure reflection sky remains (even if game objects culled away)
			t.tskymaskforcamerasnoshadow = t.tmaskforcameras & 1073741823;
			if ( ObjectExist(t.terrain.objectstartindex+4) == 1 ) 
			{
				SetObjectMask (  t.terrain.objectstartindex+4,(t.tskymaskforcamerasnoshadow),0,0,0 );
			}
			if ( ObjectExist(t.terrain.objectstartindex+8) == 1 ) 
			{
				SetObjectMask (  t.terrain.objectstartindex+8,(t.tskymaskforcamerasnoshadow),0,0,0 );
			}
			if ( ObjectExist(t.terrain.objectstartindex+9) == 1 ) 
			{
				SetObjectMask (  t.terrain.objectstartindex+9,1);//(t.tskymaskforcamerasnoshadow),0,0,0 ); no relfection of scroll
			}

			// And, ensure terrain physics objects (used for occlusion) are never rendered
			if ( t.terrain.TerrainLODOBJStart>0 ) 
				for ( t.o = t.terrain.TerrainLODOBJStart ; t.o <= t.terrain.TerrainLODOBJFinish; t.o++ )
					if ( ObjectExist(t.o) == 1 ) 
						SetObjectMask ( t.o,0,0,0,0 );

			// Update in-game objects that only appear in main camera
			if ( g.globals.riftmode>0 || (g.vrglobals.GGVREnabled > 0 && g.vrglobals.GGVRUsingVRSystem == 1) )  
			{
				if ( g.vrglobals.GGVREnabled > 0 )
					t.tmaskmaincamera=1+(1<<t.glefteyecameraid)+(1<<t.grighteyecameraid);
				else
					t.tmaskmaincamera=0+(1<<t.glefteyecameraid)+(1<<t.grighteyecameraid);
			}
			else
			{
				t.tmaskmaincamera=1;
			}

			// guns only for main camera (HUD objects for camera zero only)
			for ( t.o = g.gunbankoffset ; t.o <= g.editorwaypointoffset-1; t.o++ )
			{
				if ( ObjectExist(t.o) == 1 ) 
				{
					//SetObjectMask ( t.o, t.tmaskmaincamera, 0, 0, 0 ); //until have a VR friendly way to show HUD objects
					SetObjectMask ( t.o, 1, 0, 0, 0 );
				}
			}

			// Water plane only for main camera
			if ( ObjectExist(t.terrain.objectstartindex+5)>0 ) 
			{
				SetObjectMask ( t.terrain.objectstartindex+5, t.tmaskmaincamera );//1 ); need to see water in VR renders
			}

			// go through all entities to ensure they render to VR scenes
			for ( t.e = 1; t.e <= g.entityelementlist; t.e++ )
			{
				// LB: visuals_loop can be called when project FPM loads, but strangely BEFORE the entity parents and elements are loaded, so this might be an issue (resolved for now with t.entid < t.entityprofile.size())
				t.entid = t.entityelement[t.e].bankindex;
				if ( t.entid > 0 && t.entid < t.entityprofile.size())
				{
					if ( t.entityprofile[t.entid].ischaractercreator == 1 ) 
					{
						for ( int iParts = 0; iParts <= 2; iParts++ )
						{
							t.tccobj = (g.charactercreatorrmodelsoffset+((t.e*3)-t.characterkitcontrol.offset))+iParts;
							if ( ObjectExist(t.tccobj) == 1 ) 
							{
								SetObjectMask ( t.tccobj, t.tmaskmaincamera+(1<<31) );
							}
						}
					}
				}
			}
		}

		// Shader updates
		visuals_justshaderupdate ( );

		// Calculate 'reasonable' camera FOV (void zero and 100)
		g.greasonableWeaponFOV_f=t.visuals.WeaponFOV_f;

		// Set selected object FOV's
		for ( t.tgunid = 1 ; t.tgunid<=  g.gunmax; t.tgunid++ )
		{
			t.tgunobj=t.gun[t.tgunid].obj;
			if ( t.tgunobj>0 ) 
			{
				if ( ObjectExist(t.tgunobj) == 1 ) 
				{
					SetObjectFOV ( t.tgunobj,g.greasonableWeaponFOV_f );
				}
			}
		}

		// Muzzle Flash(es)
		for ( t.t = 0 ; t.t<=  1; t.t++ )
		{
			if ( t.t == 0  )  t.tobj = g.hudbankoffset+5;
			if ( t.t == 1  )  t.tobj = g.hudbankoffset+32;
			if ( ObjectExist(t.tobj) == 1  )  SetObjectFOV (  t.tobj,g.greasonableWeaponFOV_f );
		}

		// Brass
		for ( t.t = 6 ; t.t<=  20; t.t++ )
		{
			t.tobj=g.hudbankoffset+t.t;
			if ( ObjectExist(t.tobj) == 1  )  SetObjectFOV (  t.tobj,g.greasonableWeaponFOV_f );
		}

		// Smoke
		for ( t.t = 21 ; t.t<=  30; t.t++ )
		{
			t.tobj=g.hudbankoffset+t.t;
			if ( ObjectExist(t.tobj) == 1  )  SetObjectFOV (  t.tobj,g.greasonableWeaponFOV_f );
		}

		// Adjust resolution of reflection based on slider value
		if ( t.visuals.reflectionmode>0 ) 
		{
			if ( ImageExist(t.terrain.imagestartindex+6) == 1 ) 
			{
				if ( t.terrain.reflsizer != g.greflectionrendersize ) 
				{
					t.terrain.reflsizer=g.greflectionrendersize;
					SetCameraToImage ( 2,-1,0,0 );
					SetCameraToImage ( 2,t.terrain.imagestartindex+6,t.terrain.reflsizer,t.terrain.reflsizer );
					TextureObject ( t.terrain.objectstartindex+5,2,t.terrain.imagestartindex+6 );
				}
			}
		}

		// Ensures LOW FPS detector not fooled by setting changes
		g.lowfpstarttimer= MAXTimer();

		// and trigger camera refresh (but flag can be triggered elsewhere, like LUA command to change camera)
		t.visuals.refreshmaincameras = 1;
		void Wicked_Update_Visuals(void *voidvisual);
		Wicked_Update_Visuals( (void *) &t.visuals );
	}

	// 070918 - have separate control of camera refresh
	if ( t.visuals.refreshmaincameras == 1 )
	{
		// Set camera settings
		g.greasonableCameraFOV_f=t.visuals.CameraFOV_f;
		for ( t.tcamid = 0 ; t.tcamid <= 4; t.tcamid++ )
		{
			if ( CameraExist(t.tcamid) == 1 && t.tcamid != 3 ) 
			{
				// 311017 - solve Z clash issues by adjusting near depth based on far depth
				//PE: removes flickering on "old bridge" in TBE.
				//PE: 8+ seams wo work without near geo disappering.
				//PE: 14 seams to be the largest possible when directly up to a flat wall.
				//PE: Default range 8-14 . use setup.ini lowestnearcamera to go lower then 8.
				float fFinalNearDepth = g.lowestnearcamera + t.visuals.CameraNEAR_f + ((t.visuals.CameraFAR_f/DEFAULT_FAR_PLANE)*6.0f); // PE: range 8-14
				SetCameraRange ( t.tcamid, fFinalNearDepth, t.visuals.CameraFAR_f );
				SetCameraAspect ( t.tcamid,t.visuals.CameraASPECT_f );
				SetCameraFOV ( t.tcamid,g.greasonableCameraFOV_f );
			}
		}
		t.visuals.refreshmaincameras = 0;
	}

	// Update vegetation when required. Wait for mouse release because it takes a long time when updating during a drag
	if ( t.visuals.refreshvegetation == 1 ) 
	{
		if (  MouseClick() == 0 ) 
		{
			// Only update the vegetation grid if the view distance is far enough, otherwise just clear it
			if ( t.terrain.vegetationgridsize>1 ) 
			{
				SetEffectConstantF (  t.terrain.terrainshaderindex,"GrassFadeDistance",t.tGrassFadeDistance );
				if ( t.terrain.vegetationshaderindex>0 ) 
				{
					if ( GetEffectExist(t.terrain.vegetationshaderindex) == 1 ) 
					{
						SetEffectConstantF (  t.terrain.vegetationshaderindex,"GrassFadeDistance",t.tGrassFadeDistance );
					}
				}
			}
			else
			{
				DeleteVegetationGrid (  );
			}
			t.visuals.refreshvegetation=0;
			g.lowfpstarttimer= MAXTimer();
		}
		else
		{
			t.s_s="generating new grass geometry" ; lua_prompt ( );
		}
	}

	// refreshterrain super texture (when switch terrain bank)
	if ( t.visuals.refreshterrainsupertexture>0 ) 
	{
		if ( t.visuals.refreshterrainsupertexture == 2 ) 
		{
			//  generate super texture from above textures
			t.visuals.refreshterrainsupertexture=0;
		}
		else
		{
			t.s_s="generating new terrain super texture" ; lua_prompt ( );
			t.visuals.refreshterrainsupertexture=2;
		}
	}

	// Update world settings (sky, terrain, veg texture)
	if ( t.visuals.refreshskysettings == 1 ) 
	{
		// change day sky
		g.skyindex = t.visuals.skyindex ; if (  g.skyindex>g.skymax  )  g.skyindex = g.skymax;
		t.visuals.sky_s=t.skybank_s[g.skyindex];
		t.terrainskyspecinitmode=0 ; sky_skyspec_init ( );
		t.sky.currenthour_f=8.0;
		t.sky.daynightprogress=0;
		t.sky.changingsky=1;
		t.visuals.refreshskysettings=0;
		g.lowfpstarttimer= MAXTimer();

		// if change sky, regenerate env map
		cubemap_generateglobalenvmap();
	}

	// Update terrain textures
	if ( t.visuals.refreshterraintexture > 0 ) 
	{
		// 080320 - this prevented terrain from being changed in test game
		//if ( t.game.set.ismapeditormode == 1 ) t.visuals.refreshterraintexture = 2;
		t.visuals.refreshterraintexture = 2;
		if ( t.visuals.refreshterraintexture == 2 ) 
		{
			// first check if CUSTOM available (texture_d.dds present)
			bool bCustomTextureExists = false;
			if ( FileExist ( cstr(g.mysystem.levelBankTestMapAbs_s+"Texture_D.dds").Get() ) == 1 ) bCustomTextureExists = true;
			if ( FileExist ( cstr(g.mysystem.levelBankTestMapAbs_s+"Texture_D.jpg").Get() ) == 1 ) bCustomTextureExists = true;
			#ifdef ENABLECUSTOMTERRAIN
			if ( t.visuals.terrainindex > 1 || (t.visuals.terrainindex==1 && bCustomTextureExists == true) )
			#else
			if ( t.visuals.terrainindex >= 1 )
			#endif
			{
				// change terrain textures
				g.terrainstyleindex=t.visuals.terrainindex;
				if ( g.terrainstyleindex>g.terrainstylemax  )  g.terrainstyleindex = g.terrainstylemax;
				t.visuals.terrain_s=t.terrainstylebank_s[g.terrainstyleindex];
				terrain_changestyle ( );
			}
			else
			{
				// revert to last one, custom texture not present
				if ( t.visuals.terrainindex == 1 )
				{
					t.visuals.terrainindex = g.terrainstyleindex;
					t.slidersmenuvalue[t.slidersmenunames.worldpanel][2].value_s = t.terrainstylebank_s[g.terrainstyleindex];
					t.slidersmenuvalue[t.slidersmenunames.worldpanel][2].value = g.terrainstyleindex;
				}
			}
			t.visuals.refreshterraintexture=0;
			g.lowfpstarttimer= MAXTimer();
			t.visuals.refreshshaders=1;

			// if change terrain, regenerate env map
			cubemap_generateglobalenvmap();
		}
	}

	// Update veg texture
	if ( t.visuals.refreshvegtexture == 1 ) 
	{
		// change vegetation textures
		g.vegstyleindex=t.visuals.vegetationindex;
		if (  g.vegstyleindex>g.vegstylemax  )  g.vegstyleindex = g.vegstylemax;
		t.visuals.vegetation_s=t.vegstylebank_s[g.vegstyleindex];
		t.visuals.refreshvegtexture=0;
		g.lowfpstarttimer= MAXTimer();
	}
}

void visuals_shaderlevels_update_core (bool bUpdateEngine)
{
	// HIGHEST
	// t.visuals.shaderlevels.entities = 1;
	// t.visuals.shaderlevels.terrain = 1;
	// t.visuals.shaderlevels.vegetation = 1;
	// MEDIUM
	// t.visuals.shaderlevels.entities = 2;
	// t.visuals.shaderlevels.terrain = 3;
	// t.visuals.shaderlevels.vegetation = 3;
	// LOWEST
	// t.visuals.shaderlevels.entities = 3;
	// t.visuals.shaderlevels.terrain = 4;
	// t.visuals.shaderlevels.vegetation = 4;

	// Settings for Editor and Game
	extern bool bEnable30FpsAnimations;
	extern bool bShadowsLowestLOD;
	extern bool bProbesLowestLOD;
	extern bool bRaycastLowestLOD;
	extern bool bPhysicsLowestLOD;
	extern bool bReflectionsLowestLOD;

	extern bool g_bDelayedShadows;
	extern bool g_bDelayedShadowsLaptop;
	extern bool bEnableTerrainChunkCulling;
	extern bool bEnablePointShadowCulling;
	extern bool bEnableSpotShadowCulling;
	extern bool bEnableObjectCulling;
	extern bool bEnableAnimationCulling;
	extern float maxApparentSize;
	extern float fLODMultiplier;
	extern bool bThreadedPhysics;
	bool bPerformAnUpdate = false;
	int iChangeSkyType = -1;
	if (t.visuals.shaderlevels.entities == 2) // CUSTOM (MEDIUM)
	{
		// Custom settings - do not override
	}
	else
	{
		if (t.visuals.shaderlevels.entities == 1) // HIGHEST
		{
			bShadowsLowestLOD = false;
			bProbesLowestLOD = false;
			bRaycastLowestLOD = true;
			bReflectionsLowestLOD = false;

			bEnable30FpsAnimations = false;
			bPhysicsLowestLOD = false;
			g_bDelayedShadows = false;
			g_bDelayedShadowsLaptop = false;
			float fASize = 0.08f;
			maxApparentSize = fASize / 10000.0f;
			t.visuals.bLevelVSyncEnabled = true;
			t.visuals.bReflectionsEnabled = true;
			//PE: Only change if user has not selected a custom skybox and use bDisableSkybox
			if (t.visuals.skyindex == 0)
			{
				//PE: Dont change if user have selected "none"
				//LB: toggling LOW then HIGHEST messed this logic - lets assume standalone players can OVERRIDE sky settings if they want simulated sky
				if (t.visuals.bDisableSkybox==true)
				{
					t.visuals.bDisableSkybox = false;
					iChangeSkyType = 0;
				}
			}
			fLODMultiplier = 3.0f;
		}
		if (t.visuals.shaderlevels.entities == 3) // LOW
		{
			bShadowsLowestLOD = true;
			bProbesLowestLOD = true;
			bRaycastLowestLOD = true;
			bPhysicsLowestLOD = true;
			bReflectionsLowestLOD = true;

			bEnable30FpsAnimations = true;

			g_bDelayedShadows = true;
			g_bDelayedShadowsLaptop = true;
			float fASize = 1.0f;
			maxApparentSize = fASize / 10000.0f;
			t.visuals.bLevelVSyncEnabled = false;
			t.visuals.bReflectionsEnabled = false;
			//PE: Only change if user has not selected a custom skybox and use bDisableSkybox
			if (t.visuals.skyindex == 0)
			{
				//PE: Dont change if user have selected "none"
				if (t.visuals.bDisableSkybox == false)
				{
					t.visuals.bDisableSkybox = true;
					iChangeSkyType = 1;
				}
			}
			fLODMultiplier = 1.0f;

		}
		t.visuals.bOcclusionCulling = true;
		bEnableObjectCulling = true;
		bEnableTerrainChunkCulling = true;
		bEnablePointShadowCulling = true;
		bEnableSpotShadowCulling = true;
		bEnableAnimationCulling = true;
		t.gamevisuals.bDisableSkybox = t.visuals.bDisableSkybox;
		bPerformAnUpdate = true;
	}
	if ( bPerformAnUpdate == true )
	{
		t.gamevisuals.bEnable30FpsAnimations = t.visuals.bEnable30FpsAnimations = bEnable30FpsAnimations;

		t.gamevisuals.bPhysicsLowestLOD = t.visuals.bPhysicsLowestLOD = bPhysicsLowestLOD;
		t.gamevisuals.bShadowsLowestLOD = t.visuals.bShadowsLowestLOD = bShadowsLowestLOD;
		t.gamevisuals.bProbesLowestLOD = t.visuals.bProbesLowestLOD = bProbesLowestLOD;
		t.gamevisuals.bRaycastLowestLOD = t.visuals.bRaycastLowestLOD = bRaycastLowestLOD;
		t.gamevisuals.bReflectionsLowestLOD = t.visuals.bReflectionsLowestLOD = bReflectionsLowestLOD;

		t.gamevisuals.g_bDelayedShadows = t.visuals.g_bDelayedShadows = g_bDelayedShadows;
		t.gamevisuals.g_bDelayedShadowsLaptop = t.visuals.g_bDelayedShadowsLaptop = g_bDelayedShadowsLaptop;
		t.gamevisuals.bEnableObjectCulling = t.visuals.bEnableObjectCulling = bEnableObjectCulling;
		t.gamevisuals.bEnableTerrainChunkCulling = t.visuals.bEnableTerrainChunkCulling = bEnableTerrainChunkCulling;
		t.gamevisuals.bEnablePointShadowCulling = t.visuals.bEnablePointShadowCulling = bEnablePointShadowCulling;
		t.gamevisuals.bEnableSpotShadowCulling = t.visuals.bEnableSpotShadowCulling = bEnableSpotShadowCulling;
		t.gamevisuals.bEnableObjectCulling = t.visuals.bEnableObjectCulling = bEnableObjectCulling;
		t.gamevisuals.bEnableAnimationCulling = t.visuals.bEnableAnimationCulling = bEnableAnimationCulling;
		t.gamevisuals.fLODMultiplier = t.visuals.fLODMultiplier = fLODMultiplier;

		t.gamevisuals.bThreadedPhysics = t.visuals.bThreadedPhysics = bThreadedPhysics;
		
		t.gamevisuals.ApparentSize = t.visuals.ApparentSize = maxApparentSize;
		t.gamevisuals.bReflectionsEnabled = t.visuals.bReflectionsEnabled;
		if (bEnableTerrainChunkCulling && !t.visuals.bOcclusionCulling)
		{
			t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
		}
		if (bEnablePointShadowCulling && !t.visuals.bOcclusionCulling)
		{
			t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
		}
		if (bEnableSpotShadowCulling && !t.visuals.bOcclusionCulling)
		{
			t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
		}
		if (bEnableObjectCulling && !t.visuals.bOcclusionCulling)
		{
			t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
		}
		if (bEnableAnimationCulling && !t.visuals.bOcclusionCulling)
		{
			t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
		}
		t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling;
		t.gamevisuals.bLevelVSyncEnabled = t.visuals.bLevelVSyncEnabled;
		if (bUpdateEngine == true)
		{
			wiRenderer::SetOcclusionCullingEnabled(t.visuals.bOcclusionCulling);
			// DX12: the engine's staggered directional cascade refresh (delta 1.11) must follow the
			// project's Delayed Shadows setting. At HIGHEST quality g_bDelayedShadows=false, so this
			// disables staggering -> every-frame cascades -> stable shadows (fixes the cliff-shadow
			// flicker under camera movement). Previously the engine hard-forced staggering on at sun
			// creation, so the "Delayed Shadows" UI checkbox (point-shadow only) could never turn it off.
			wiRenderer::SetDelayedShadowCascadesEnabled(g_bDelayedShadows);
			// Also gate the shadow LOD override (Wicked default ON) — it renders terrain into shadows
			// at a threshold-oscillating per-cascade LOD independent of the visible chunk, so the cast
			// shadow flips between two terrain shapes ("shadow mismatch", the author's own warning).
			// OFF at HIGHEST quality = shadows follow the stable main-view LOD -> no shape flicker.
			wiRenderer::SetShadowLODOverrideEnabled(g_bDelayedShadows);
			extern void gridedit_setvsync (bool);
			gridedit_setvsync(t.visuals.bLevelVSyncEnabled);
			extern void gridedit_setreflection (bool);
			gridedit_setreflection(t.visuals.bReflectionsEnabled);
			extern void gridedit_setsky (int);
			if(iChangeSkyType >= 0)
				gridedit_setsky(iChangeSkyType);
			g.projectmodified = 1;
		}
	}

	// Only do this in standalone as it will otherwise mess up editor settings!
	if (bUpdateEngine == true && t.game.gameisexe == 1)
	{
		// Wicked controls some early performance levers:
		static bool bInitGraphicsSettingsValues = true;
		static float fInitialShadowPointResolution = 0;
		static float fInitialShadowSpotResolution = 0;
		static float fInitialGrassDrawDistanceValue = 0;
		static float fInitialCameraFar = t.visuals.CameraFAR_f;
		if (bInitGraphicsSettingsValues == true)
		{
			if (t.visuals.shaderlevels.entities == 1)
			{
				fInitialShadowPointResolution = t.visuals.iShadowPointResolution;
				fInitialShadowSpotResolution = t.visuals.iShadowSpotResolution;
			}
			if (t.visuals.shaderlevels.entities == 2)
			{
				fInitialShadowPointResolution = t.visuals.iShadowPointResolution * 2;
				fInitialShadowSpotResolution = t.visuals.iShadowSpotResolution * 2;
			}
			if (t.visuals.shaderlevels.entities == 3)
			{
				fInitialShadowPointResolution = t.visuals.iShadowPointResolution * 4;
				fInitialShadowSpotResolution = t.visuals.iShadowSpotResolution * 4;
			}
			if (t.visuals.shaderlevels.vegetation == 1) fInitialGrassDrawDistanceValue = GGGrass::gggrass_global_params.lod_dist;
			if (t.visuals.shaderlevels.vegetation == 2 || t.visuals.shaderlevels.vegetation == 3) fInitialGrassDrawDistanceValue = GGGrass::gggrass_global_params.lod_dist / 2;
			if (t.visuals.shaderlevels.vegetation == 4) fInitialGrassDrawDistanceValue = GGGrass::gggrass_global_params.lod_dist / 3;

			// all initial values set for possible changes below
			bInitGraphicsSettingsValues = false;
		}

		// "entities" controls shadow work
		if (fInitialShadowPointResolution > 0)
		{
			t.visuals.iShadowPointResolution = fInitialShadowPointResolution;
			if (t.visuals.shaderlevels.entities == 2) t.visuals.iShadowPointResolution = fInitialShadowPointResolution / 2;
			if (t.visuals.shaderlevels.entities == 3) t.visuals.iShadowPointResolution = fInitialShadowPointResolution / 4;
		}
		if (fInitialShadowSpotResolution > 0)
		{
			t.visuals.iShadowSpotResolution = fInitialShadowSpotResolution;
			if (t.visuals.shaderlevels.entities == 2) t.visuals.iShadowSpotResolution = fInitialShadowSpotResolution / 2;
			if (t.visuals.shaderlevels.entities == 3) t.visuals.iShadowSpotResolution = fInitialShadowSpotResolution / 4;
		}
		void Wicked_Update_Visuals(void* voidvisual);
		Wicked_Update_Visuals((void*)&t.visuals);

		// "vegetation" controls draw distance
		extern GGGrass::GGGrassParams gggrass_global_params;
		GGGrass::gggrass_global_params.lod_dist = fInitialGrassDrawDistanceValue;
		if (t.visuals.shaderlevels.vegetation >= 2) GGGrass::gggrass_global_params.lod_dist = fInitialGrassDrawDistanceValue * 2;
		if (t.visuals.shaderlevels.vegetation == 4) GGGrass::gggrass_global_params.lod_dist = fInitialGrassDrawDistanceValue * 3;

		extern CCameraManager m_CameraManager;
		tagCameraData* m_ptr = m_CameraManager.GetData(0);
		WickedCall_SetCameraFOV(m_ptr->fFOV);
	}

	// performance levers a game script set (SetOcclusionCulling and the rest) survive this refresh; in a test game it sets
	// them with no visuals push after it
	extern void LuaRenderSettings_Apply(void);
	LuaRenderSettings_Apply();
}

void visuals_shaderlevels_setlevel (int iActionTypeInternalByName, bool bUpdateEngine)
{
	if (iActionTypeInternalByName == 1)
	{
		t.visuals.shaderlevels.entities = 1;
		t.visuals.shaderlevels.terrain = 1;
		t.visuals.shaderlevels.vegetation = 1;
	}
	if (iActionTypeInternalByName == 3)
	{
		t.visuals.shaderlevels.entities = 2;
		t.visuals.shaderlevels.terrain = 3;
		t.visuals.shaderlevels.vegetation = 3;
	}
	if (iActionTypeInternalByName == 4)
	{
		t.visuals.shaderlevels.entities = 3;
		t.visuals.shaderlevels.terrain = 4;
		t.visuals.shaderlevels.vegetation = 4;
	}
	visuals_shaderlevels_update_core(bUpdateEngine);
}

void visuals_shaderlevels_update (void)
{
	bool bUpdateEngineToo = true;
	visuals_shaderlevels_update_core (bUpdateEngineToo);
}

void visuals_underwater_on ( void )
{
	// save all our shader settings then set the underwater fog action
	if ( t.visuals.underwatermode  ==  0 ) 
	{
		t.tDrowning_OldReflectionMode = t.visuals.reflectionmode;
		t.tDrowning_OldFogFar_f = t.visuals.FogDistance_f;
		t.tDrowning_OldFogNear_f = t.visuals.FogNearest_f;
		t.tDrowning_OldFogR_f = t.visuals.FogR_f;
		t.tDrowning_OldFogG_f = t.visuals.FogG_f;
		t.tDrowning_OldFogB_f = t.visuals.FogB_f;
		t.tDrowning_OldFogA_f = t.visuals.FogA_f;
		t.tDrowning_OldWobbleHeight = t.playercontrol.wobbleheight_f;
		//PE: Terrain reflection from underwater looks strange , so render reflections without terrain.
		t.visuals.reflectionmode = 1;
		// don't change fog underwater, handled by the shader
		//PE: SSAO looks wrong underwater so perhaps disable it. or increase fog distance.
		//PE: remove head bobbing
		t.playercontrol.wobbleheight_f = 0.0;

		if (g.underwatermode == 1) 
		{
			//PE: You can control how the post process make waves to make it look like we are underwater.
			SetVector4(g.terrainvectorindex1, 1.0, 40.0, 0.0135, 0.17);  //PE: Active=1,Speed,Distortion,Scale
			SetEffectConstantV(g.postprocesseffectoffset + 0, "UnderWaterSettings", g.terrainvectorindex1); //PE: post bloom.
			SetEffectConstantV(g.postprocesseffectoffset + 4, "UnderWaterSettings", g.terrainvectorindex1); //PE: also post sao.
		}

		// water gravity control
		if (t.playercontrol.gravityactive == 1)
			ODESetWorldGravity(0, -0.5f, 0, 150.0);
		else
			ODESetWorldGravity(0, 0, 0);

		t.tFogR_f = t.visuals.FogR_f; t.tFogG_f = t.visuals.FogG_f; t.tFogB_f = t.visuals.FogB_f ; t.tFogA_f = t.visuals.FogA_f;
		t.tFogNear_f = t.visuals.FogNearest_f; t.tFogFar_f = t.visuals.FogDistance_f;
		terrain_setfog ( );
		terrain_water_setfog ( );
		t.visuals.underwatermode = 1;

		//PE: objects underwater also need fog.
		visuals_justshaderupdate(); 
	}
}

void visuals_underwater_off ( void )
{
	// reset all our shaders back
	if ( t.visuals.underwatermode  ==  1 ) 
	{
		t.visuals.reflectionmode = t.tDrowning_OldReflectionMode;
		t.visuals.FogDistance_f = t.tDrowning_OldFogFar_f;
		t.visuals.FogNearest_f = t.tDrowning_OldFogNear_f;
		t.visuals.FogR_f = t.tDrowning_OldFogR_f;
		t.visuals.FogG_f = t.tDrowning_OldFogG_f;
		t.visuals.FogB_f = t.tDrowning_OldFogB_f;
		t.visuals.FogA_f = t.tDrowning_OldFogA_f;
		t.playercontrol.wobbleheight_f = t.tDrowning_OldWobbleHeight; //PE: restore head bobbing

		t.tFogR_f = t.visuals.FogR_f; t.tFogG_f = t.visuals.FogG_f ; t.tFogB_f = t.visuals.FogB_f ; t.tFogA_f = t.visuals.FogA_f;
		t.tFogNear_f = t.visuals.FogNearest_f; t.tFogFar_f = t.visuals.FogDistance_f;
		terrain_setfog ( );
		terrain_water_setfog ( );
		t.visuals.underwatermode = 0;

		// restore to ground gravity
		if (t.playercontrol.gravityactive == 1)
			ODESetWorldGravity(0, -20, 0 , 0);
		else
			ODESetWorldGravity(0, 0, 0);

		//PE: Restore normal fog and disable screen wave effect.
		visuals_justshaderupdate();
		if (g.underwatermode == 1) 
		{
			SetVector4(g.terrainvectorindex, 0.0, 155.0, 0.0095, 0.0);
			SetEffectConstantV(g.postprocesseffectoffset + 0, "UnderWaterSettings", g.terrainvectorindex); //PE: post bloom.
			SetEffectConstantV(g.postprocesseffectoffset + 4, "UnderWaterSettings", g.terrainvectorindex); //PE: also post sao.
		}
	}
}
