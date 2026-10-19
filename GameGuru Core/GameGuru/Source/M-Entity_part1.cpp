void entity_loaddata ( void )
{
	//  Protectf BIN file if no FPE backup (standalone run)
	t.tprotectBINfile=0;
	if (t.ent_s.Get()[1] == ':') t.entdir_s = "";


	t.tFPEName_s=t.entdir_s+t.ent_s;
	if (  FileExist(t.tFPEName_s.Get()) == 0 ) 
	{
		t.tprotectBINfile=1;
	}

	//  Ensure entity profile still exists
	t.entityprofileheader[t.entid].desc_s="";
	t.tprofile_s=Left(t.tFPEName_s.Get(),Len(t.tFPEName_s.Get())-4); t.tprofile_s += ".bin";
	if (  t.tprotectBINfile == 0 ) 
	{
		//  020715 - to solve BIN issue once and for all, delete them when load entity
		//  and only preserve for final standalone export (actually NO performance difference!)
		if (  FileExist(t.tprofile_s.Get()) == 1  )  DeleteAFile (  t.tprofile_s.Get() );
	}

	t.strwork = t.entdir_s+t.ent_s;
	if (  FileExist(t.strwork.Get()) == 1 || FileExist(t.tprofile_s.Get()) == 1 ) 
	{

	//  Export entity FPE file if flagged
	if (  g.gexportassets == 1 ) 
	{
		t.strwork = t.entdir_s+t.ent_s;
		t.tthumbbmpfile_s = "";	t.tthumbbmpfile_s=t.tthumbbmpfile_s + Left(t.strwork.Get(),(Len(t.entdir_s.Get())+Len(t.ent_s.Get()))-4)+".bmp";
	}

	//  Allowed to loop around if skipBIN flag set
	do {  t.skipBINloadingandtryagain=0;

	//  Check if binary version of entity profile exists (DELETE BIN AT MOMEMENT!)
	//C++ISSUE forcing non binary at the moment due to make memblock from array not implemented for new arrays
	//looks like we are delelting bins anyway, but just incase...
	if ( 1 ) //if (  FileExist(t.tprofile_s.Get()) == 0 ) 
	{
		// 061115 - reset entity anim start value (so can be filled further down)
		for ( int n = 0; n < g.animmax; n++ )
		{
			t.entityanim[t.entid][n].start = 0;
			t.entityanim[t.entid][n].finish = 0;
			t.entityanim[t.entid][n].found = 0;
		}

		//  Must be reset before parse
		t.entityprofile[t.entid].limbmax=0;
		t.entityprofile[t.entid].animmax=0;
		t.entityprofile[t.entid].appendanimmax=0; //PE: sometimes , caused endless loop, was never set anywhere.
		t.entityprofile[t.entid].footfallmax=0;
		t.entityprofile[t.entid].headlimb=-1;
		t.entityprofile[t.entid].firespotlimb=-1;
		t.entityprofile[t.entid].physics=1;
		t.entityprofile[t.entid].phyweight=100;
		t.entityprofile[t.entid].phyfriction=0;
		t.entityprofile[t.entid].hoverfactor=0;
		t.entityprofile[t.entid].phyalways=0;
		t.entityprofile[t.entid].spine=-1;
		t.entityprofile[t.entid].spine2=-1;
		t.entityprofile[t.entid].decaloffsetangle=0;
		t.entityprofile[t.entid].decaloffsetdist=0;
		t.entityprofile[t.entid].decaloffsety=0;
		t.entityprofile[t.entid].ragdoll = 0;
		t.entityprofile[t.entid].nothrowscript=0;
		t.entityprofile[t.entid].canfight=1;
		t.entityprofile[t.entid].rateoffire=85;
		t.entityprofile[t.entid].transparency=0;
		t.entityprofile[t.entid].canseethrough=0;
		t.entityprofile[t.entid].cullmode=0;
		t.entityprofile[t.entid].lod1distance=0;
		t.entityprofile[t.entid].lod2distance=0;
		t.entityprofile[t.entid].characterbasetype = -1;// bitbobon = -1;
		t.entityprofile[t.entid].reservedy2 = 0.0f;
		t.entityprofile[t.entid].reservedy3 = 0.0f;
		t.entityprofile[t.entid].autoflatten=0;
		t.entityprofile[t.entid].headframestart=-1;
		t.entityprofile[t.entid].headframefinish=-1;
		t.entityprofile[t.entid].hairframestart=-1;
		t.entityprofile[t.entid].hairframefinish=-1;
		t.entityprofile[t.entid].hideframestart=-1;
		t.entityprofile[t.entid].hideframefinish=-1;
		t.entityprofile[t.entid].animspeed=100;
		t.entityprofile[t.entid].animstyle=0;
		t.entityprofile[t.entid].collisionscaling = 100;
		t.entityprofile[t.entid].collisionscalingxz = 100;
		t.entityprofile[t.entid].physicsobjectcount = 0;
		t.entityprofile[t.entid].ishudlayer_s="";
		t.entityprofile[t.entid].ishudlayer=0;
		t.entityprofile[t.entid].fatness=50;
		t.entityprofile[t.entid].matrixmode=0;
		t.entityprofile[t.entid].skipfvfconvert=0;
		t.entityprofile[t.entid].resetlimbmatrix=0;
		t.entityprofile[t.entid].onetexture=0;
		t.entityprofile[t.entid].usesweapstyleanims=0;
		t.entityprofile[t.entid].isviolent=1;
		t.entityprofile[t.entid].reverseframes=0;
		t.entityprofile[t.entid].fullbounds=0;
		t.entityprofile[t.entid].cpuanims=0;
		t.entityprofile[t.entid].ignoredefanim=0;
		t.entityprofile[t.entid].explodeheight=0;
		t.entityprofile[t.entid].explodable_decalname = "";
		t.entityprofile[t.entid].scale=100;
		t.entityprofile[t.entid].addhandle_s="";
		t.entityprofile[t.entid].addhandlelimb=0;
		t.entityprofile[t.entid].ischaractercreator=0;
		t.entityprofile[t.entid].charactercreator_s="";
		t.entityprofile[t.entid].fJumpModifier=1.0f;		
		t.entityprofile[t.entid].jumphold=0;
		t.entityprofile[t.entid].jumpresume=0;
		t.entityprofile[t.entid].jumpvaulttrim=1;
		t.entityprofile[t.entid].meleerange=80;
		t.entityprofile[t.entid].meleehitangle=30;
		t.entityprofile[t.entid].meleestrikest=0;
		t.entityprofile[t.entid].meleestrikefn=0;
		t.entityprofile[t.entid].meleedamagest=20;
		t.entityprofile[t.entid].meleedamagefn=30;
		for ( t.q = 0 ; t.q<=  100 ; t.q++ ) { t.entitybodypart[t.entid][t.q]=0 ;   }
		t.entityprofile[t.entid].usespotlighting=0;
		t.entityprofile[t.entid].lodmodifier=0;
		t.entityprofile[t.entid].isocluder=1; // can be adjusted (if notanoccluder set to 1)
		t.entityprofile[t.entid].navmeshbake = 0;
		t.entityprofile[t.entid].noapparentcull = 0;
		t.entityprofile[t.entid].isocludee=1;
		t.entityprofile[t.entid].lootpercentage=100;
		t.entityprofile[t.entid].specular=0;
		t.entityprofile[t.entid].uvscrollu=0;
		t.entityprofile[t.entid].uvscrollv=0;
		t.entityprofile[t.entid].uvscaleu=1.0f;
		t.entityprofile[t.entid].uvscalev=1.0f;
		t.entityprofile[t.entid].invertnormal=0;
		t.entityprofile[t.entid].preservetangents=0;		
		t.entityprofile[t.entid].colondeath=1;
		t.entityprofile[t.entid].parententityindex=0;
		t.entityprofile[t.entid].parentlimbindex=0;
		t.entityprofile[t.entid].quantity=-1; // FPE specifies a value or we use a single weapon clip buy default (below)
		t.entityprofile[t.entid].smoothangle=0;
		t.entityprofile[t.entid].noXZrotation=0;
		t.entityprofile[t.entid].zdepth = 1;
		t.entityprofile[t.entid].isebe = 0;
		t.entityprofile[t.entid].offyoverride = 0;
		t.entityprofile[t.entid].offy = 0; //PE: Was not reset.
		t.entityprofile[t.entid].ismarker = 0;  //PE: Was not reset.
		t.entityprofile[t.entid].ischaracter = 0;
		t.entityprofile[t.entid].bIsDecal = false; //PE: Was not set.
		t.entityprofile[t.entid].entitylight = 0;
		t.entityprofile[t.entid].entitylightx = 0.0f;
		t.entityprofile[t.entid].entitylighty = 0.0f;
		t.entityprofile[t.entid].entitylightz = 0.0f;
		t.entityprofile[t.entid].entitylightshadows = 0;
		t.entityprofile[t.entid].physicsmass = 0.0f;
		t.entityprofile[t.entid].isspinetracker = 1;
		t.entityprofile[t.entid].phyweight=100;
		t.entityprofile[t.entid].phyfriction=100;
		t.entityprofile[t.entid].phyforcedamage=100;
		t.entityprofile[t.entid].rotatethrow=1;
		t.entityprofile[t.entid].explodedamage=100;
		t.entityprofile[t.entid].forcesimpleobstacle=0;
		t.entityprofile[t.entid].forceobstaclepolysize=20.0f;//30.0f; hagia model
		t.entityprofile[t.entid].forceobstaclesliceheight=20.0f;//14.0f; hagia model
		t.entityprofile[t.entid].forceobstaclesliceminsize=4.0f;//5.0f; hagia model 
		t.entityprofile[t.entid].effectprofile=0;
		t.entityprofile[t.entid].ignorecsirefs=0;
		t.entityprofile[t.entid].voiceset_s = ""; // when empty, default to first voice
		t.entityprofile[t.entid].voicerate = 0;
		// For MAX, these HANDFIRESPOT values may be purposed if different characters needed a hand adjust to fit weapon standard (not in EA)
		t.entityprofile[t.entid].handfirespotoffx = 0.0f;
		t.entityprofile[t.entid].handfirespotoffy = 0.0f;
		t.entityprofile[t.entid].handfirespotoffz = 0.0f;
		t.entityprofile[t.entid].handfirespotrotx = 0.0f;
		t.entityprofile[t.entid].handfirespotroty = 0.0f;
		t.entityprofile[t.entid].handfirespotrotz = 0.0f;
		t.entityprofile[t.entid].handfirespotsize = 100.0f;
		t.entityprofile[t.entid].coneangle = 100.0f;
		t.entityprofile[t.entid].conerange = 1000.0f;

		// reset so characters start with NO WEAPON!
		t.entityprofile[t.entid].isammo = 0;
		t.entityprofile[t.entid].hasweapon = 0;
		t.entityprofile[t.entid].hasweapon_s = "";

		// head and spine tracker detail defaults
		t.entityprofile[t.entid].headspinetracker.headhlimit = 60;// 45; CineGuru better talk tracking (can improve down the road with eye and gesture tracking)!
		t.entityprofile[t.entid].headspinetracker.headhoffset = 0;
		t.entityprofile[t.entid].headspinetracker.headvlimit = 45;
		t.entityprofile[t.entid].headspinetracker.headvoffset = 0;
		t.entityprofile[t.entid].headspinetracker.spinehlimit = 45;
		t.entityprofile[t.entid].headspinetracker.spinehoffset = 0;
		t.entityprofile[t.entid].headspinetracker.spinevlimit = 45;
		t.entityprofile[t.entid].headspinetracker.spinevoffset = 0;

		//  Starter animation counts
		t.tnewanimmax=0 ; t.entityprofile[t.entid].animmax=t.tnewanimmax;
		t.tstartofaianim=-1 ; t.entityprofile[t.entid].startofaianim=t.tstartofaianim;

		// other resets
		t.entityappendanim[t.entid][0].filename = "";
		t.entityappendanim[t.entid][0].startframe = 0;

		// wicked custom materials
		t.entityprofile[t.entid].thumbnailbackdrop = "";
		t.entityprofile[t.entid].WEMaterial.MaterialActive = false;

		for (int i = 0; i < MAXMESHMATERIALS; i++)
		{
			// per mesh textures and control values

			t.entityprofile[t.entid].WEMaterial.customShaderID = -1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam1 = 1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam2 = 1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam3 = 1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam4 = 1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam5 = 1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam6 = 1;
			t.entityprofile[t.entid].WEMaterial.customShaderParam7 = 1;
			t.entityprofile[t.entid].WEMaterial.WPEffect = "";

			t.entityprofile[t.entid].WEMaterial.baseColorMapName[i] = "";
			t.entityprofile[t.entid].WEMaterial.normalMapName[i] = "";
			t.entityprofile[t.entid].WEMaterial.emissiveMapName[i] = "";
			t.entityprofile[t.entid].WEMaterial.surfaceMapName[i] = "";
			#ifndef DISABLEOCCLUSIONMAP
			t.entityprofile[t.entid].WEMaterial.occlusionMapName[i] = "";
			#endif
			t.entityprofile[t.entid].WEMaterial.displacementMapName[i] = "";
			t.entityprofile[t.entid].WEMaterial.fAlphaRef[i] = 1.0;
			t.entityprofile[t.entid].WEMaterial.fNormal[i] = 1.0;
			t.entityprofile[t.entid].WEMaterial.fEmissive[i] = 0.0;
			t.entityprofile[t.entid].WEMaterial.fRoughness[i] = 1.0;
			t.entityprofile[t.entid].WEMaterial.fMetallness[i] = 1.0;

			// per mesh colors
			t.entityprofile[t.entid].WEMaterial.dwBaseColor[i] = -1;

			//PE: Default to black, if old .fpe files do not have it set.
			t.entityprofile[t.entid].WEMaterial.dwEmmisiveColor[i] = 0;

			// per mesh settings and flags
			t.entityprofile[t.entid].WEMaterial.bCastShadows[i] = true;
			t.entityprofile[t.entid].WEMaterial.bDoubleSided[i] = false;
			t.entityprofile[t.entid].WEMaterial.fRenderOrderBias[i] = 0;
			t.entityprofile[t.entid].WEMaterial.bPlanerReflection[i] = false;
			t.entityprofile[t.entid].WEMaterial.bTransparency[i] = false;
			t.entityprofile[t.entid].WEMaterial.fReflectance[i] = 0.04f;// 0.002f;
		}

		t.entityprofile[t.entid].thumbnailbackdrop = "";
		t.entityprofile[t.entid].BackBufferZoom = -1.0f;
		t.entityprofile[t.entid].BackBufferCamLeft = -1.0f;
		t.entityprofile[t.entid].BackBufferCamUp = -1.0f;
		t.entityprofile[t.entid].BackBufferRotateX = -1.0f;
		t.entityprofile[t.entid].BackBufferRotateY = -1.0f;
		t.entityprofile[t.entid].iThumbnailAnimset = -1.0f;
		t.entityprofile[t.entid].keywords_s = "";
		
		t.entityprofile[t.entid].effect_s = ""; //bIsDecal , needed reset.

		t.entityprofile[t.entid].collectable.image = "default";
		t.entityprofile[t.entid].collectable.description = t.entityprofileheader[t.entid].desc_s;
		t.entityprofile[t.entid].collectable.cost = 10;
		t.entityprofile[t.entid].collectable.value = 5;
		t.entityprofile[t.entid].collectable.container = "none";
		t.entityprofile[t.entid].collectable.ingredients = "none";
		t.entityprofile[t.entid].collectable.style = "none";

		t.entityprofile[t.entid].blendmode = 0;

		t.entityprofile[t.entid].light.fProbeBrightness = 1.0f;

		//  temp variable to hold which physics object we are on from the importer
		t.tPhysObjCount = 0;

		//  Load entity Data from file
		// ZJ: Users have been reporting their imported models don't load correctly, caused by the FPEs having more than 400 lines.
		Dim(t.data_s, 500); //Dim (  t.data_s,400  );
		t.strwork = t.entdir_s + t.ent_s;
		LoadArray(t.strwork.Get(), t.data_s);
		for (t.l = 0; t.l <= 499; t.l++) //for ( t.l = 0 ; t.l<=  399; t.l++ )*/
		{
			t.line_s=t.data_s[t.l];
			if (  Len(t.line_s.Get())>0 ) 
			{
				if (  t.line_s.Get()[0] != ';' ) 
				{
					// safety check to ensure strings not overloaded (i.e. if trying to load an encrypted carbage FPE)
					if (strlen(t.line_s.Get()) > 255)
					{
						t.value_s = "ERROR";
						continue;
					}

					//  take fieldname and value
					for ( t.c = 0 ; t.c < Len(t.line_s.Get()); t.c++ )
					{
						if ( t.line_s.Get()[t.c] == '=' ) 
						{ 
							t.mid = t.c+1  ; break;
						}
					}
					t.field_s=cstr(Lower(removeedgespaces(Left(t.line_s.Get(),t.mid-1))));
					t.value_s=cstr(removeedgespaces(Right(t.line_s.Get(),Len(t.line_s.Get())-t.mid)));

					//  take value 1 and 2 from value
					for ( t.c = 0 ; t.c < Len(t.value_s.Get()); t.c++ )
					{
						if (  t.value_s.Get()[t.c] == ',' ) 
						{ 
							t.mid = t.c+1 ; break; 
						}
					}
					t.value1=ValF(removeedgespaces(Left(t.value_s.Get(),t.mid-1)));
					t.value1_f=ValF(removeedgespaces(Left(t.value_s.Get(),t.mid-1)));
					t.value2_s=cstr(removeedgespaces(Right(t.value_s.Get(),Len(t.value_s.Get())-t.mid)));
					if (  Len(t.value2_s.Get())>0  )  t.value2 = ValF(t.value2_s.Get()); else t.value2 = -1;

					// string comparison optimization
					alignas(16) char t_field_s[32];
					const char* src = Lower(t.field_s.Get());
					int index = 0;
					while( src[index] && index < 32 )
					{
						t_field_s[ index ] = src[ index ];
						index++;
					}

					int matchlen = index;

					while( index < 32 )
					{
						t_field_s[ index++ ] = 0;
					}

					bool matched = false;

					//  entity header
					cmpStrConst( t_field_s, "desc" );
					if ( matched )  t.entityprofileheader[t.entid].desc_s = t.value_s;
					
					// check if group early, and abort (we use LoadGroup for these type of FPE files)
					cmpStrConst( t_field_s, "isgroupobject" );
					if (matched && t.value1 == 1)
					{
						// if looking for the group flag, mark it as found
						extern int g_iAbortedAsEntityIsGroupFileMode;
						if (g_iAbortedAsEntityIsGroupFileMode == 1)
						{
							// move mode to show we have determined it IS a group file!
							g_iAbortedAsEntityIsGroupFileMode = 2;
						}
					}

					//  entity AI
					cmpStrConst( t_field_s, "aimain" );
					if (matched)
					{
						t.entityprofile[t.entid].aimain_s = Lower(t.value_s.Get());
						if (strnicmp (t.entityprofile[t.entid].aimain_s.Get(), "default.lua", 11) == NULL)
						{
							t.entityprofile[t.entid].aimain_s = "no_behavior_selected.lua";
						}
						else
						{
							if (strlen(t.entityprofile[t.entid].aimain_s.Get()) < 4 ) 
							{
								t.entityprofile[t.entid].aimain_s = "no_behavior_selected.lua";
							}
						}
						if (strnicmp (t.entityprofile[t.entid].aimain_s.Get(), "markers\\togglelight.lua", 23) == NULL)
						{
							// ensure this particular behavior uses case (eventually allow all objects to use case (carefully))
							// doing so right now might introduce new issues!
							t.entityprofile[t.entid].aimain_s = "markers\\ToggleLight.lua";
						}
					}

					// determine CCP base type and store
					cmpStrConst( t_field_s, "ccpassembly" );
					if (matched)
					{
						char pLowCase[MAX_PATH];
						strcpy(pLowCase, t.value_s.Get());
						strlwr(pLowCase);
						t.entityprofile[t.entid].characterbasetype = -1;
						if (strstr (pLowCase, "adult male") != NULL) t.entityprofile[t.entid].characterbasetype = 0;
						if (strstr (pLowCase, "adult female") != NULL) t.entityprofile[t.entid].characterbasetype = 1;
						if (strstr (pLowCase, "zombie male") != NULL) t.entityprofile[t.entid].characterbasetype = 2;
						if (strstr (pLowCase, "zombie female") != NULL) t.entityprofile[t.entid].characterbasetype = 3;
						if (t.entityprofile[t.entid].characterbasetype == -1)
						{
							for (int i = 0; i < g_CharacterType.size(); i++)
							{
								if (strnicmp (pLowCase, g_CharacterType[i].pPartsFolder, strlen(g_CharacterType[i].pPartsFolder)) == NULL)
								{
									t.entityprofile[t.entid].characterbasetype = i;
									break;
								}
							}
						}
					}

					cmpStrConst( t_field_s, "voice" );
					if (  matched  )  t.entityprofile[t.entid].voiceset_s = t.value_s;
					cmpStrConst( t_field_s, "speakrate" );
					if (  matched  )  t.entityprofile[t.entid].voicerate = t.value1;
					cmpStrConst( t_field_s, "soundset" );
					if (  matched  )  t.entityprofile[t.entid].soundset_s = t.value_s;
					cmpStrConst( t_field_s, "soundset1" );
					if (  matched  )  t.entityprofile[t.entid].soundset1_s = t.value_s;
					cmpStrConst( t_field_s, "soundset2" );
					if (  matched  )  t.entityprofile[t.entid].soundset2_s = t.value_s;
					cmpStrConst( t_field_s, "soundset3" );
					if (  matched  )  t.entityprofile[t.entid].soundset3_s = t.value_s;
					cmpStrConst( t_field_s, "soundset4");
					if (  matched  )  t.entityprofile[t.entid].soundset4a_s = t.value_s;
					cmpStrConst( t_field_s, "soundset5");
					if (  matched  )  t.entityprofile[t.entid].soundset5_s = t.value_s;
					cmpStrConst( t_field_s, "soundset6");
					if (  matched  )  t.entityprofile[t.entid].soundset6_s = t.value_s;
					//  entity AI related vars
					cmpStrConst( t_field_s, "usekey" );
					if (  matched  )  t.entityprofile[t.entid].usekey_s = t.value_s;
					cmpStrConst( t_field_s, "ifused" );
					if (  matched  )  t.entityprofile[t.entid].ifused_s = t.value_s;

					//  entity SPAWN
					cmpStrConst( t_field_s, "spawnmax" );
					if (  matched  )  t.entityprofile[t.entid].spawnmax = t.value1;
					cmpStrConst( t_field_s, "spawndelay" );
					if (  matched  )  t.entityprofile[t.entid].spawndelay = t.value1;
					cmpStrConst( t_field_s, "spawnqty" );
					if (  matched  )  t.entityprofile[t.entid].spawnqty = t.value1;
					//  entity orientation
					cmpStrConst( t_field_s, "model" );
					//  if it is a character creator model, ignore this
					if (  t.entityprofile[t.entid].ischaractercreator == 0 ) 
					{
						if (  matched  ) 
							t.entityprofile[t.entid].model_s = t.value_s;
					}

					cmpStrConst( t_field_s, "offsetx" );
					if (  matched  )  t.entityprofile[t.entid].offx = t.value1;
					cmpStrConst( t_field_s, "offsety" );
					if (  matched  )  t.entityprofile[t.entid].offy = t.value1;
					cmpStrConst( t_field_s, "offyoverride" );
					if (  matched  )  
					{
						t.entityprofile[t.entid].offyoverride = 1;
						t.entityprofile[t.entid].offy = t.value1;
					}
					cmpStrConst( t_field_s, "offsetz" );
					if (  matched  )  t.entityprofile[t.entid].offz = t.value1;
					cmpStrConst( t_field_s, "rotx" );
					if (  matched  )  t.entityprofile[t.entid].rotx = t.value1;
					cmpStrConst( t_field_s, "roty" );
					if (  matched  )  t.entityprofile[t.entid].roty = t.value1;
					cmpStrConst( t_field_s, "rotz" );
					if (  matched  )  t.entityprofile[t.entid].rotz = t.value1;
					cmpStrConst( t_field_s, "scale" );
					if (  matched  )  t.entityprofile[t.entid].scale = t.value1;
					cmpStrConst( t_field_s, "fixnewy" );
					if (  matched  )  t.entityprofile[t.entid].fixnewy = t.value1;
					cmpStrConst( t_field_s, "hoverfactor" );
					if (  matched ) 
					{
						//  FPGC - V116 - some FPE characters use a range 0.1-0.9, must be accounted!
						if (  t.value1_f>-1.0 && t.value1_f<1.0  )  t.value1 = 1;
						t.entityprofile[t.entid].hoverfactor=t.value1;
						t.entityprofile[t.entid].hoverfactor=t.value1;
					}
					cmpStrConst( t_field_s, "forwardfacing" );
					if (  matched  )  t.entityprofile[t.entid].forwardfacing = t.value1;
					cmpStrConst( t_field_s, "dontfindfloor" );
					if (  matched  )  t.entityprofile[t.entid].dontfindfloor = t.value1;
					cmpStrConst( t_field_s, "defaultheight" );
					if (  matched  )  t.entityprofile[t.entid].defaultheight = t.value1;
					cmpStrConst( t_field_s, "defaultstatic" );
					if (  matched  )  t.entityprofile[t.entid].defaultstatic = t.value1;

					//  autoflatten
					//  0  ; none (default)
					//  1  ; rectangle area
					//  2  ; circle area
					cmpStrConst( t_field_s, "autoflatten" );
					if (  matched  )  t.entityprofile[t.entid].autoflatten = t.value1;

					//  collisionmode (see GameGuru\Docs\collisionmodevalues.txt)
					//  0  ; box shape (default)
					//  1  ; polygon shape
					//  2  ; sphere shape
					//  3  ; cylinder shape
					//  8  ; polygon shape using OBJ file
					//  9  ; convex hull reduction shape
					//  10 ; hull decomposition - multiple convex hulls
					//  11 ; no physics
					//  12 ; no physics but can still be detected with IntersectAll command
					//  21 ; player repel feature (for characters and other beasts/zombies)
					//  22 ; no repel (for animals that player can pass through)
					//  31 ; hybrid collision (dynamic box shape except meshes ending with "_static" which are separated as non colliding static renders; full doors/windows set) 
					//  40 ; collision boxes (defined in Import Model feature)
					//  41-49 ; reserved (collision polylist, sphere list, cylinder list)
					//  50 ; generate obstacle and cylinder from 1/64th up from base of model
					//  51 ; generate obstacle and cylinder from 1/32th down from base of model
					//  52 ; generate obstacle and cylinder from 8/16th up from base of model
					//  53 ; generate obstacle and cylinder from 7/16th up from base of model
					//  54 ; generate obstacle and cylinder from 6/16th up from base of model
					//  55 ; generate obstacle and cylinder from 5/16th up from base of model
					//  56 ; generate obstacle and cylinder from 4/16th up from base of model
					//  57 ; generate obstacle and cylinder from 3/16th up from base of model
					//  58 ; generate obstacle and cylinder from 2/16th up from base of model
					//  59 ; generate obstacle and cylinder from 1/16th up from base of model
					//  1000-2000 ; only one limb has collision Box Shape (1000=limb zero,1001=limb one,etc)
					//  2000-3000 ; only one limb has collision Polygons Shape (2000=limb zero,2001=limb one,etc)					
					cmpStrConst( t_field_s, "collisionmode" );
					if (  matched  )  t.entityprofile[t.entid].collisionmode = t.value1;
					cmpStrConst( t_field_s, "collisionscaling" );
					if (matched)
					{
						t.entityprofile[t.entid].collisionscaling = t.value1;
						if(t.value2>0) t.entityprofile[t.entid].collisionscalingxz = t.value2;
					}
					cmpStrConst( t_field_s, "collisionoverride" );
					if (  matched  )  t.entityprofile[t.entid].collisionoverride = t.value1;

					// endcollision:
					// 0 - no collision for ragdoll
					// 1 - collision for ragdoll
					cmpStrConst( t_field_s, "endcollision" );
					if (  matched  )  t.entityprofile[t.entid].colondeath = t.value1;

					//  forcesimpleobstacle
					//  -1 ; absolutely no obstacle
					//  0 ; default
					//  1 ; Box (  )
					//  2 ; contour
					//  3 ; full poly scan
					cmpStrConst( t_field_s, "forcesimpleobstacle" );
					if (  matched  )  t.entityprofile[t.entid].forcesimpleobstacle = t.value1;
					cmpStrConst( t_field_s, "forceobstaclepolysize" );
					if (  matched  )  t.entityprofile[t.entid].forceobstaclepolysize = t.value1;
					cmpStrConst( t_field_s, "forceobstaclesliceheight" );
					if (  matched  )  t.entityprofile[t.entid].forceobstaclesliceheight = t.value1;
					cmpStrConst( t_field_s, "forceobstaclesliceminsize" );
					if (  matched  )  t.entityprofile[t.entid].forceobstaclesliceminsize = t.value1;

					cmpStrConst( t_field_s, "navmeshbake" );
					if ( matched ) t.entityprofile[t.entid].navmeshbake = t.value1;

					cmpStrConst( t_field_s, "noapparentcull" );
					if ( matched ) t.entityprofile[t.entid].noapparentcull = t.value1;

					cmpStrConst( t_field_s, "notanoccluder" );
					if (  matched  )  
					{
						t.entityprofile[t.entid].notanoccluder = t.value1;
						if ( t.entityprofile[t.entid].notanoccluder == 1 ) t.entityprofile[t.entid].isocluder = 0;
					}
					cmpStrConst( t_field_s, "notanoccludee" );
					if (  matched  )  
					{
						int notanoccludee = t.value1;
						if ( notanoccludee == 1 ) t.entityprofile[t.entid].isocludee = 0;
					}

					cmpStrConst( t_field_s, "skipfvfconvert" );
					if (  matched  )  t.entityprofile[t.entid].skipfvfconvert = t.value1;
					cmpStrConst( t_field_s, "matrixmode" );
					if (  matched  )  t.entityprofile[t.entid].matrixmode = t.value1;

					// 040116 - when lightmapper scales entities incorrectly, need this flag to correct!
					cmpStrConst( t_field_s, "resetlimbmatrix" );
					if (  matched  )  t.entityprofile[t.entid].resetlimbmatrix = t.value1;

					cmpStrConst( t_field_s, "reverseframes" );
					if (  matched  )  t.entityprofile[t.entid].reverseframes = t.value1;
					cmpStrConst( t_field_s, "fullbounds" );
					if (  matched  )  t.entityprofile[t.entid].fullbounds = t.value1;

					//  cpuanims
					//  0 ; GPU animation
					//  1 ; CPU animation (for wide scope animations that need accurate ray detection)
					//  2 ; Same as [1] but will hide any meshes that do not have animations
					cmpStrConst( t_field_s, "cpuanims" );
					if (  matched  )  t.entityprofile[t.entid].cpuanims = t.value1;
				
					// 131115 - some legacy models hold an OLD nasty frame in matCombined for each frame, and can mess up collision detection if CPUANIMS=1 also
					cmpStrConst( t_field_s, "ignoredefanim" );
					if (  matched  )  t.entityprofile[t.entid].ignoredefanim = t.value1;

					//  materialindex
					// -1    = Can happen when Model Importer batch converts before global material manually set (bug-so repair below)!
					//  0    = - GenericSoft
					//  1    = S Stone
					//  2    = M Metal
					//  3    = W Wood
					//  4    = G Glass
					//  5    = L Liquid Splashy Wet
					//  6    = F Flesh (Bloody Organic)
					//  7    = H Hollow Drum Metal
					//  8    = T Small High Pitch Tin
					//  9    = V Small Low Pitch Tin
					//  10   = I Silly Material
					//  11   = A Marble
					//  12   = C Cobble
					//  13   = R Gravel
					//  14   = E Soft Metal
					//  15   = O Old Stone
					//  16   = D Old Wood
					//  17   = W Shallow Water
					//  18   = U Underwater
					cmpStrConst( t_field_s, "materialindex" );
					if (matched)
					{
						t.entityprofile[t.entid].materialindex = t.value1;
						if (t.entityprofile[t.entid].materialindex == -1) t.entityprofile[t.entid].materialindex = 0; // repair!
					}

					// Allow Bullet Holes until set per placement (-1 when absent: static entities only, see entity_allowsbulletholes)
					cmpStrConst( t_field_s, "allowbulletholes" );
					if (matched) t.entityprofile[t.entid].allowbulletholes = t.value1;

					// Clear Grass and Trees until set per placement (-1 when absent: large static objects only, see entity_clearsvegetation)
					cmpStrConst( t_field_s, "clearvegetation" );
					if (matched) t.entityprofile[t.entid].clearvegetation = t.value1;

					//  LOD and BITBOB system
					cmpStrConst( t_field_s, "disablebatch" );
					if (  matched  )  t.entityprofile[t.entid].disablebatch = t.value1;
					cmpStrConst( t_field_s, "lod1distance" );
					if (  matched  )  t.entityprofile[t.entid].lod1distance = t.value1;
					cmpStrConst( t_field_s, "lod2distance" );
					if (  matched  )  t.entityprofile[t.entid].lod2distance = t.value1;

					//  physics setup
					cmpStrConst( t_field_s, "physics" );
					if (  matched  )  t.entityprofile[t.entid].physics = t.value1;
					cmpStrConst( t_field_s, "phyweight" );
					if (  matched  )  t.entityprofile[t.entid].phyweight = t.value1;
					cmpStrConst( t_field_s, "phyfriction" );
					if (  matched  )  t.entityprofile[t.entid].phyfriction = t.value1;
					cmpStrConst( t_field_s, "explodable" );
					if (  matched  )  t.entityprofile[t.entid].explodable = t.value1;
					cmpStrConst( t_field_s, "explodedamage" );
					if (  matched  )  t.entityprofile[t.entid].explodedamage = t.value1;
					cmpStrConst( t_field_s, "ragdoll" );
					if (matched)
					{
						t.entityprofile[t.entid].ragdoll = t.value1;
					}

					//  FPGC - 160511 - added NOTHROWSCRIPT to entity profile
					cmpStrConst( t_field_s, "nothrowscript" );
					if (  matched  )  t.entityprofile[t.entid].nothrowscript = t.value1;

					//  cone of sight
					cmpStrConst( t_field_s, "coneheight" );
					if (  matched  )  t.entityprofile[t.entid].coneheight = t.value1;
					cmpStrConst( t_field_s, "coneangle" );
					if (  matched  )  t.entityprofile[t.entid].coneangle = t.value1;
					cmpStrConst( t_field_s, "conerange" );
					if (  matched  )  t.entityprofile[t.entid].conerange = t.value1;

					//  visual info
					cmpStrConst( t_field_s, "onetexture" );
					if (  matched  )  t.entityprofile[t.entid].onetexture = t.value1;
					if (t.entityprofile[t.entid].ischaractercreator == 0)
					{
						cmpStrConst( t_field_s, "texturepath" );
						if (matched)  t.entityprofile[t.entid].texpath_s = t.value_s;
						cmpStrConst( t_field_s, "textured" );
						if (matched)  t.entityprofile[t.entid].texd_s = t.value_s;
						cmpStrConst( t_field_s, "texturealtd" );
						if (matched)  t.entityprofile[t.entid].texaltd_s = t.value_s;


						// store thumbnail backdrop image name
						cmpStrConst( t_field_s, "thumbnailbackdrop" ); if (matched) { t.entityprofile[t.entid].thumbnailbackdrop = t.value_s; }
						cmpStrConst( t_field_s, "thumbnailzoom" ); if (matched) { t.entityprofile[t.entid].BackBufferZoom = t.value1_f; }
						cmpStrConst( t_field_s, "thumbnailcamleft" ); if (matched) { t.entityprofile[t.entid].BackBufferCamLeft = t.value1_f; }
						cmpStrConst( t_field_s, "thumbnailcamup" ); if (matched) { t.entityprofile[t.entid].BackBufferCamUp = t.value1_f; }
						cmpStrConst( t_field_s, "thumbnailrotatex" ); if (matched) { t.entityprofile[t.entid].BackBufferRotateX = t.value1_f; }
						cmpStrConst( t_field_s, "thumbnailrotatey" ); if (matched) { t.entityprofile[t.entid].BackBufferRotateY = t.value1_f; }
						cmpStrConst( t_field_s, "thumbnailanimset" ); if (matched) { t.entityprofile[t.entid].iThumbnailAnimset = (int) t.value1_f; }
						cmpStrConst( t_field_s, "keywords" ); if (matched) { t.entityprofile[t.entid].keywords_s = t.value_s; }

						cmpNStrConst( t_field_s, "basecolormap" );
						if ( matched )
						{
							int index = atoi( t_field_s + 12 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.baseColorMapName[index] = t.value_s; 
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst(t_field_s, "customshaderid");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderID = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam1");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam1 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam2");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam2 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam3");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam3 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam4");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam4 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam5");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam5 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam6");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam6 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}
						cmpNStrConst(t_field_s, "customshaderparam7");
						if (matched)
						{
							t.entityprofile[t.entid].WEMaterial.customShaderParam7 = t.value1_f;
							t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
						}


						cmpNStrConst( t_field_s, "alpharef" );
						if ( matched )
						{
							int index = atoi( t_field_s + 8 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fAlphaRef[index] = t.value1_f; 
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "normalmap" );
						if ( matched )
						{
							int index = atoi( t_field_s + 9 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.normalMapName[index] = t.value_s; 
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "normalstrength" );
						if ( matched )
						{
							int index = atoi( t_field_s + 14 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fNormal[index] = t.value1_f; 
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "surfacemap" );
						if ( matched )
						{
							int index = atoi( t_field_s + 10 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.surfaceMapName[index] = t.value_s;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "roughnessstreng" );
						if ( matched )
						{
							int index = atoi( t_field_s + 17 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fRoughness[index] = t.value1_f;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "metalnessstreng" );
						if ( matched )
						{
							int index = atoi( t_field_s + 17 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fMetallness[index] = t.value1_f;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "displacementmap" );
						if ( matched )
						{
							int index = atoi( t_field_s + 15 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.displacementMapName[index] = t.value_s;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "emissivemap" );
						if ( matched )
						{
							int index = atoi( t_field_s + 11 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.emissiveMapName[index] = t.value_s;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "emissivestrengt" ); //PE: "emissivestrength"
						if ( matched )
						{
							int index = atoi( t_field_s + 16 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fEmissive[index] = t.value1_f;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						#ifndef DISABLEOCCLUSIONMAP
						cmpNStrConst( t_field_s, "occlusionmap" );
						if ( matched )
						{
							int index = atoi( t_field_s + 12 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.occlusionMapName[index] = t.value_s;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}
						#endif

						cmpNStrConst( t_field_s, "emissivecolor" );
						if ( matched )
						{
							int index = atoi( t_field_s + 13 );
							if ( index < MAXMESHMATERIALS )
							{
								unsigned long ulValue = 0;
								sscanf(t.value_s.Get(), "%lu", &ulValue);
								t.entityprofile[t.entid].WEMaterial.dwEmmisiveColor[index] = ulValue;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "basecolor" );
						if ( matched && t_field_s[9] != 'm' ) // filter out "basecolormap"
						{
							int index = atoi( t_field_s + 9 );
							if ( index < MAXMESHMATERIALS )
							{
								unsigned long ulValue = 0;
								sscanf(t.value_s.Get(), "%lu", &ulValue);
								t.entityprofile[t.entid].WEMaterial.dwBaseColor[index] = ulValue;
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "transparency" );
						if ( matched )
						{
							int index = atoi( t_field_s + 12 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.bTransparency[index] = t.value1_f;
								if (t.entityprofile[t.entid].WEMaterial.bTransparency[index] < 0)
									t.entityprofile[t.entid].WEMaterial.bTransparency[index] = 0;
							}

							//  transparency modes;
							//  0 - first-phase no alpha
							//  1 - first-phase with alpha masking
							//  2 and 3 - second-phase which overlaps solid geometry
							//  4 - alpha test (only render beyond 0x000000CF alpha values)
							//  5 - water Line (  object (seperates depth sort automatically) )
							//  6 - combination of 3 and 4 (second phase render with alpha blend AND alpha test, used for fading LOD leaves)
							//  7 - very early draw phase no alpha
							if ( t_field_s[12] == 0 )
							{
								if ( t.value1 == 5 ) t.value1 = 6; // 021215 - can only ben one water line
								t.entityprofile[t.entid].transparency = t.value1;
								if (t.entityprofile[t.entid].transparency < 0)
									t.entityprofile[t.entid].transparency = 0;
							}
						}

						cmpNStrConst( t_field_s, "doublesided" );
						if ( matched )
						{
							int index = atoi( t_field_s + 11 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.bDoubleSided[index] = t.value1_f;
							}
						}

						cmpNStrConst( t_field_s, "renderorderbias" );
						if ( matched )
						{
							int index = atoi( t_field_s + 15 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fRenderOrderBias[index] = t.value1_f;
							}
						}

						cmpNStrConst( t_field_s, "castshadow" );
						if ( matched )
						{
							int index = atoi( t_field_s + 10 );
							if ( index < MAXMESHMATERIALS )
							{
								if ( t.value1_f == - 1 )
									t.entityprofile[t.entid].WEMaterial.bCastShadows[index] = false; 
								else
									t.entityprofile[t.entid].WEMaterial.bCastShadows[index] = true; 
							}
						}

						cmpNStrConst( t_field_s, "planerreflectio" );
						if ( matched )
						{
							int index = atoi( t_field_s + 16 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.bPlanerReflection[index] = t.value1_f; 
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}

						cmpNStrConst( t_field_s, "reflectance" );
						if ( matched )
						{
							int index = atoi( t_field_s + 11 );
							if ( index < MAXMESHMATERIALS )
							{
								t.entityprofile[t.entid].WEMaterial.fReflectance[index] = t.value1_f; 
								t.entityprofile[t.entid].WEMaterial.MaterialActive = true;
							}
						}
					}

					cmpStrConst( t_field_s, "effect" );
					if (  matched  )  t.entityprofile[t.entid].effect_s = t.value_s;

					// effectprofile:
					// 0 - default non-PBR
					// 1 - new PBR texture arrangement
					cmpStrConst( t_field_s, "effectprofile" );
					if ( matched ) t.entityprofile[t.entid].effectprofile = t.value1;

					cmpStrConst( t_field_s, "canseethrough" );
					if (  matched  )  t.entityprofile[t.entid].canseethrough = t.value1;

					// specular:
					// 0 - uses _S texture
					// 1 - uses effectbank\\reloaded\\media\\blank_none_S.dds
					// 2 - uses effectbank\\reloaded\\media\\blank_low_S.dds
					// 3 - uses effectbank\\reloaded\\media\\blank_medium_S.dds
					// 4 - uses effectbank\\reloaded\\media\\blank_high_S.dds
					cmpStrConst( t_field_s, "specular" );
					if (  matched  )  t.entityprofile[t.entid].specular = t.value1;

					// can scale the uv data inside the shader
					cmpStrConst( t_field_s, "uvscroll" );
					if (  matched  ) { t.entityprofile[t.entid].uvscrollu = t.value1/100.0f; t.entityprofile[t.entid].uvscrollv = t.value2/100.0f; }
					cmpStrConst( t_field_s, "uvscale" );
					if (  matched  ) { t.entityprofile[t.entid].uvscaleu = t.value1/100.0f; t.entityprofile[t.entid].uvscalev = t.value2/100.0f; }

					// can invert the normal, or set to zero to not invert (not inverted by default)
					cmpStrConst( t_field_s, "invertnormal" );
					if (  matched  )  t.entityprofile[t.entid].invertnormal = t.value1;

					// can choose whether to generate tangent/binormal in the shader
					cmpStrConst( t_field_s, "preservetangents" );
					if (  matched  )  t.entityprofile[t.entid].preservetangents = t.value1;
					
					cmpStrConst( t_field_s, "zdepth" );
					if (  matched  )  t.entityprofile[t.entid].zdepth = t.value1;

					cmpStrConst( t_field_s, "cullmode" );
					if (  matched  )  t.entityprofile[t.entid].cullmode = t.value1;
					cmpStrConst( t_field_s, "reducetexture" );
					if (  matched  )  t.entityprofile[t.entid].reducetexture = t.value1;

					// castshadow:
					//  0 = default shadow caster mode
					// -1 = do not cast shadows or lightmap shadows
					cmpStrConst( t_field_s, "castshadow" );
					if (  matched  )  t.entityprofile[t.entid].castshadow = t.value1;
					cmpStrConst( t_field_s, "smoothangle" );
					if (  matched  )  t.entityprofile[t.entid].smoothangle = t.value1;

					//  entity identity details
					cmpStrConst( t_field_s, "strength" );
					if (  matched  )  t.entityprofile[t.entid].strength = t.value1;
					cmpStrConst( t_field_s, "lives" );
					if (  matched  )  t.entityprofile[t.entid].lives = t.value1;
					cmpStrConst( t_field_s, "speed" );
					if (  matched  )  t.entityprofile[t.entid].speed = t.value1;
					cmpStrConst( t_field_s, "animspeed" );
					if (matched)
					{
						// LB: recent performance impprovements caused characters to move comically fast
						// so detect this specific default value and restore to a more reasonable speed
						// https://github.com/Dark-Basic-Software-Limited/GameGuruRepo/issues/6048
						if (t.entityprofile[t.entid].ischaracter == 1)
						{
							if (t.value1 == 100) t.value1 = 70;
						}

						// and of course any other value other than 100 will be the actual speed value set
						t.entityprofile[t.entid].animspeed = t.value1;
					}
					cmpStrConst( t_field_s, "hurtfall" );
					if (  matched  )  t.entityprofile[t.entid].hurtfall = t.value1;

					cmpStrConst( t_field_s, "isimmobile" );
					if (  matched  )  t.entityprofile[t.entid].isimmobile = t.value1;
					cmpStrConst( t_field_s, "isviolent" );
					if (  matched  )  t.entityprofile[t.entid].isviolent = t.value1;
					cmpStrConst( t_field_s, "isobjective" );
					if (  matched  )  t.entityprofile[t.entid].isobjective = t.value1;
					cmpStrConst( t_field_s, "alwaysactive" );
					if (  matched  )  t.entityprofile[t.entid].phyalways = t.value1;

					cmpStrConst(t_field_s, "iscollectable");
					if (matched)  t.entityprofile[t.entid].iscollectable = t.value1;
					cmpStrConst(t_field_s, "collectimage");
					if (matched)  t.entityprofile[t.entid].collectable.image = t.value_s;
					cmpStrConst(t_field_s, "collectdescription");
					if (matched)  t.entityprofile[t.entid].collectable.description = t.value_s;
					cmpStrConst(t_field_s, "collectcost");
					if (matched)  t.entityprofile[t.entid].collectable.cost = t.value1;
					cmpStrConst(t_field_s, "collectvalue");
					if (matched)  t.entityprofile[t.entid].collectable.value = t.value1;
					cmpStrConst(t_field_s, "collectcontainer");
					if (matched)  t.entityprofile[t.entid].collectable.container = t.value_s;
					cmpStrConst(t_field_s, "collectingredients");
					if (matched)  t.entityprofile[t.entid].collectable.ingredients = t.value_s;
					cmpStrConst(t_field_s, "collectstyle");
					if (matched)  t.entityprofile[t.entid].collectable.style = t.value_s;

					cmpStrConst( t_field_s, "ischaracter" );
					if (  matched  )  t.entityprofile[t.entid].ischaracter = t.value1;
					cmpStrConst( t_field_s, "isspinetracker" );
					if (  matched  )  t.entityprofile[t.entid].isspinetracker = t.value1;
					
					cmpStrConst( t_field_s, "noxzrotation" );
					if (  matched  )  t.entityprofile[t.entid].noXZrotation = t.value1;				
					cmpStrConst( t_field_s, "canfight" );
					if (  matched  )  t.entityprofile[t.entid].canfight = t.value1;

					cmpStrConst( t_field_s, "jumpmodifier" );
					if (  matched  )  t.entityprofile[t.entid].fJumpModifier = (float)t.value1 / 100.0f;
					cmpStrConst( t_field_s, "jumphold" );
					if (  matched  )  t.entityprofile[t.entid].jumphold = t.value1;
					cmpStrConst( t_field_s, "jumpresume" );
					if (  matched  )  t.entityprofile[t.entid].jumpresume = t.value1;
					cmpStrConst( t_field_s, "jumpvaulttrim" );
					if (  matched  )  t.entityprofile[t.entid].jumpvaulttrim = t.value1;

					cmpStrConst( t_field_s, "meleerange" );
					if (  matched  )  t.entityprofile[t.entid].meleerange = t.value1;
					cmpStrConst( t_field_s, "meleehitangle" );
					if (  matched  )  t.entityprofile[t.entid].meleehitangle = t.value1;
					cmpStrConst( t_field_s, "meleestrikest" );
					if (  matched  )  t.entityprofile[t.entid].meleestrikest = t.value1;
					cmpStrConst( t_field_s, "meleestrikefn" );
					if (  matched  )  t.entityprofile[t.entid].meleestrikefn = t.value1;
					cmpStrConst( t_field_s, "meleedamagest" );
					if (  matched  )  t.entityprofile[t.entid].meleedamagest = t.value1;
					cmpStrConst( t_field_s, "meleedamagefn" );
					if (  matched  )  t.entityprofile[t.entid].meleedamagefn = t.value1;

					cmpStrConst( t_field_s, "custombiped" );
					if (  matched  )  t.entityprofile[t.entid].custombiped = t.value1;

					cmpStrConst( t_field_s, "cantakeweapon" );
					if (  matched  )  t.entityprofile[t.entid].cantakeweapon = t.value1;
					cmpStrConst( t_field_s, "isweapon" );
					if (  matched  )  t.entityprofile[t.entid].isweapon_s = t.value_s;
					cmpStrConst( t_field_s, "rateoffire" );
					if (  matched  )  t.entityprofile[t.entid].rateoffire = t.value1;
					cmpStrConst( t_field_s, "ishudlayer" );
					if (  matched  )  t.entityprofile[t.entid].ishudlayer_s = t.value_s;

					//  fpgc - same internals just sanitized  -NEED TO GO IN EDITOR TOO!
					cmpStrConst( t_field_s, "isequipment" );
					if (  matched ) 
					{
						t.entityprofile[t.entid].isweapon_s=t.value_s;
						if (  Len(t.value_s.Get())>2 ) 
						{
							//  FPGC - 280809 - if equipment specified, this entity is ALWAYS ACTIVE (so can pickup AND DROP the item)
							t.entityprofile[t.entid].phyalways=1;
						}
					}

					cmpStrConst( t_field_s, "isammo" );
					if (  matched  )  t.entityprofile[t.entid].isammo = t.value1;
					cmpStrConst( t_field_s, "hasweapon" );
					if (  matched  )  t.entityprofile[t.entid].hasweapon_s = t.value_s;
					cmpStrConst(t_field_s, "ammopool");
					if (matched)  
						t.entityprofile[t.entid].ammopool_s = t.value_s;
					cmpStrConst( t_field_s, "hasequipment" );
					if (  matched  )  t.entityprofile[t.entid].hasweapon_s = t.value_s;
					cmpStrConst( t_field_s, "ishealth" );
					if (  matched  )  t.entityprofile[t.entid].ishealth = t.value1;
					cmpStrConst( t_field_s, "isflak" );
					if (  matched  )  t.entityprofile[t.entid].isflak = t.value1;
					cmpStrConst( t_field_s, "fatness" );
					if (  matched  )  t.entityprofile[t.entid].fatness = t.value1;

					//  marker extras
					//  1=player
					//  2=lights
					//  3=trigger zone
					//  4=decal particle emitter
					//  5=entity lights
					//  6=checkpoint zone
					//  7=multiplayer start
					//  8=floor zone
					//  9=cover zone
					//  10=new particle emitter (particle editor export)
					//  11=flag
					cmpStrConst( t_field_s, "ismarker" );
					if (  matched  )  
					{
						t.entityprofile[t.entid].ismarker = t.value1;
						if ( t.entityprofile[t.entid].ismarker > 0 && t.entityprofile[t.entid].ismarker != 2 )
						{
							// force zone and arrow markers to rise above terrain if planted there
							t.entityprofile[t.entid].offyoverride = 1;
							t.entityprofile[t.entid].offy = 2;
						}
					}
					cmpStrConst( t_field_s, "markerindex" );
					if (  matched  )  t.entityprofile[t.entid].markerindex = t.value1;
					cmpStrConst( t_field_s, "addhandle" );
					if (  matched  )  t.entityprofile[t.entid].addhandle_s = t.value_s;

					// ebe builder extras
					cmpStrConst( t_field_s, "isebe" );
					if (  matched  )  t.entityprofile[t.entid].isebe = t.value1;

					//  light extras
					cmpStrConst( t_field_s, "lightcolor" );
					if (  matched  )  t.entityprofile[t.entid].light.color = t.value1;
					cmpStrConst( t_field_s, "lightrange" );
					if (  matched  )  t.entityprofile[t.entid].light.range = t.value1;
					cmpStrConst( t_field_s, "lightoffsetup" );
					if (  matched  )  t.entityprofile[t.entid].light.offsetup = t.value1;
					cmpStrConst( t_field_s, "lightoffsetz" );
					if (  matched  )  t.entityprofile[t.entid].light.offsetz = t.value1;
					cmpStrConst( t_field_s, "lightprobescale" );
					if (matched)
					{
						// GGMAX 2.90: X/Y/Z keep the .fpe value — they ARE the probe volume.
						// fLightHasProbe is only the "is a probe" flag (tested >= 50), so it is
						// canonicalised to 50 rather than mirroring an arbitrary scale.
						t.entityprofile[t.entid].light.fLightHasProbe = (t.value1 >= 50.0f) ? 50.0f : t.value1;
						t.entityprofile[t.entid].light.fLightHasProbeX = t.value1;
						t.entityprofile[t.entid].light.fLightHasProbeY = t.value1;
						t.entityprofile[t.entid].light.fLightHasProbeZ = t.value1;
					}

					// light type flags
					cmpStrConst( t_field_s, "usespotlighting" );
					if (  matched  )  t.entityprofile[t.entid].usespotlighting = t.value1;

					// GG: a light each placed one carries (lightrange and lightcolor give its range and colour)
					cmpStrConst( t_field_s, "entitylight" );
					if (  matched  )  t.entityprofile[t.entid].entitylight = t.value1;
					cmpStrConst( t_field_s, "entitylightx" );
					if (  matched  )  t.entityprofile[t.entid].entitylightx = t.value1_f;
					cmpStrConst( t_field_s, "entitylighty" );
					if (  matched  )  t.entityprofile[t.entid].entitylighty = t.value1_f;
					cmpStrConst( t_field_s, "entitylightz" );
					if (  matched  )  t.entityprofile[t.entid].entitylightz = t.value1_f;
					cmpStrConst( t_field_s, "entitylightshadows" );
					if (  matched  )  t.entityprofile[t.entid].entitylightshadows = t.value1;
					// GG: its physics body's mass in kilos (SetEntityMass sets one for a single entity)
					cmpStrConst( t_field_s, "physicsmass" );
					if (  matched  )  t.entityprofile[t.entid].physicsmass = t.value1_f;

					//  trigger extras
					cmpStrConst( t_field_s, "stylecolor" );
					if (  matched  )  t.entityprofile[t.entid].trigger.stylecolor = t.value1;

					//  extra decal offset (ideal for placing flames in torches, etc)
					cmpStrConst( t_field_s, "decalangle" );
					if (  matched  )  t.entityprofile[t.entid].decaloffsetangle = t.value1;
					cmpStrConst( t_field_s, "decaldist" );
					if (  matched  )  t.entityprofile[t.entid].decaloffsetdist = t.value1/10.0;
					cmpStrConst( t_field_s, "decaly" );
					if (  matched  )  t.entityprofile[t.entid].decaloffsety = t.value1/10.0;

					//  entity body part list (20/01/11 - refeatured for V118)
					cmpStrConst( t_field_s, "limbmax" );
					if (  matched  )  
					{
						t.entityprofile[t.entid].limbmax = t.value1;
						if (  t.entityprofile[t.entid].limbmax>100  )  t.entityprofile[t.entid].limbmax = 100;
					}

					if (  t.entityprofile[t.entid].limbmax>0 ) 
					{
						cmpNStrConst( t_field_s, "limb" );
						if (  matched  )  
						{
							int limbNum = atoi( t_field_s+4 );
							if ( limbNum < t.entityprofile[t.entid].limbmax )
							{
								if ( limbNum == 0 )
								{
									if ( t_field_s[4] == '0' && t_field_s[5] == 0 ) t.entitybodypart[t.entid][ limbNum ] = t.value1;
								}
								else if ( limbNum < 10 )
								{
									if ( t_field_s[5] == 0 ) t.entitybodypart[t.entid][ limbNum ] = t.value1;
								}
								else if ( limbNum < 100 )
								{
									if ( t_field_s[6] == 0 ) t.entitybodypart[t.entid][ limbNum ] = t.value1;
								}
								else if ( limbNum < 1000 )
								{
									if ( t_field_s[7] == 0 ) t.entitybodypart[t.entid][ limbNum ] = t.value1;
								}
							}
						}
						matched = false; // prevent skipping other strings that start with "limb"
					}

					// read in head and spine tracker settings for this character (if any)
					cmpStrConst( t_field_s, "headhlimit" ); if (matched) { t.entityprofile[t.entid].headspinetracker.headhlimit = t.value1; }
					cmpStrConst( t_field_s, "headhoffset" ); if (matched) { t.entityprofile[t.entid].headspinetracker.headhoffset = t.value1; }
					cmpStrConst( t_field_s, "headvlimit" ); if (matched) { t.entityprofile[t.entid].headspinetracker.headvlimit = t.value1; }
					cmpStrConst( t_field_s, "headvoffset" ); if (matched) { t.entityprofile[t.entid].headspinetracker.headvoffset = t.value1; }
					cmpStrConst( t_field_s, "spinehlimit" ); if (matched) { t.entityprofile[t.entid].headspinetracker.spinehlimit = t.value1; }
					cmpStrConst( t_field_s, "spinehoffset" ); if (matched) { t.entityprofile[t.entid].headspinetracker.spinehoffset = t.value1; }
					cmpStrConst( t_field_s, "spinevlimit" ); if (matched) { t.entityprofile[t.entid].headspinetracker.spinevlimit = t.value1; }
					cmpStrConst( t_field_s, "spinevoffset" ); if (matched) { t.entityprofile[t.entid].headspinetracker.spinevoffset = t.value1; }

					//  determine if entity has a head, and which limbs represent it
					cmpStrConst( t_field_s, "headlimbs" );
					if (matched) { t.entityprofile[t.entid].headframestart = t.value1; t.entityprofile[t.entid].headframefinish = t.value2; }

					//  determine if entity has hair, and which limbs represent it/them
					cmpStrConst( t_field_s, "hairlimbs" );
					if (  matched ) { t.entityprofile[t.entid].hairframestart = t.value1; t.entityprofile[t.entid].hairframefinish = t.value2; }

					//  determine if entity has limbs to hide
					cmpStrConst( t_field_s, "hidelimbs" );
					if (  matched ) { t.entityprofile[t.entid].hideframestart = t.value1; t.entityprofile[t.entid].hideframefinish = t.value2; }

					//  entity decal refs
					cmpStrConst( t_field_s, "decalmax" );
					if (  matched  )
						t.entityprofile[t.entid].decalmax = t.value1;
					if (  t.entityprofile[t.entid].decalmax>0 ) 
					{
						cmpNStrConst( t_field_s, "decal" );
						if (  matched  )  
						{
							const int rootindex = 5;
							bool bValid = false;
							int index = atoi( t_field_s+rootindex );
							if ( index == 0 )
							{
								if ( t_field_s[rootindex] == '0' && t_field_s[rootindex+1] == 0 ) bValid = true;
							}
							else if ( index < 10 )
							{
								if ( t_field_s[rootindex+1] == 0 ) bValid = true;
							}
							else if ( index < 100 )
							{
								if ( t_field_s[rootindex+2] == 0 ) bValid = true;
							}
							else if ( index < 1000 )
							{
								if ( t_field_s[rootindex+3] == 0 ) bValid = true;
							}
							else if ( index < 10000 )
							{
								if ( t_field_s[rootindex+4] == 0 ) bValid = true;
							}

							if ( bValid && index < t.entityprofile[t.entid].decalmax )
							{
								t.entitydecal_s[t.entid][index] = t.value_s;
							}
						}
					}

					// 060718 - entity append anim system
					cmpStrConst( t_field_s, "appendanimfinal" );
					if (  matched )
					{ 
						t.entityappendanim[t.entid][0].filename = t.value_s; 
						t.entityappendanim[t.entid][0].startframe = 0;
					}
					cmpStrConst( t_field_s, "appendanimmax" );
					if ( matched ) 
					{
						t.entityprofile[t.entid].appendanimmax = t.value1; 
						if ( t.entityprofile[t.entid].appendanimmax > 99 ) 
							t.entityprofile[t.entid].appendanimmax = 99;
					}

					//PE: Hanging, in my case: appendanimmax=573444874 value_s=road_straight01.x
					//PE: Hang if you are unlucky and get mem that "appendanimmax" are not already set to zero.
					if ( t.entityprofile[t.entid].appendanimmax > 0 && t.entityprofile[t.entid].appendanimmax <= 99 )
					{
						cmpNStrConst( t_field_s, "appendanimframe" );
						if (  matched  )  
						{
							int index = atoi( t_field_s+15 );
							if ( index != 0 && index < t.entityprofile[t.entid].appendanimmax )
							{
								if ( index < 10 )
								{
									if ( t_field_s[16] == 0 ) t.entityappendanim[t.entid][index].filename = t.value_s;
								}
								else if ( index < 100 )
								{
									if ( t_field_s[17] == 0 ) t.entityappendanim[t.entid][index].filename = t.value_s;
								}
								else if ( index < 1000 )
								{
									if ( t_field_s[18] == 0 ) t.entityappendanim[t.entid][index].filename = t.value_s;
								}
							}
						}
						else
						{
							cmpNStrConst( t_field_s, "appendanim" );
							if (  matched  )  
							{
								int index = atoi( t_field_s+10 );
								if ( index != 0 && index < t.entityprofile[t.entid].appendanimmax )
								{
									if ( index < 10 )
									{
										if ( t_field_s[11] == 0 ) t.entityappendanim[t.entid][index].filename = t.value_s;
									}
									else if ( index < 100 )
									{
										if ( t_field_s[12] == 0 ) t.entityappendanim[t.entid][index].filename = t.value_s;
									}
									else if ( index < 1000 )
									{
										if ( t_field_s[13] == 0 ) t.entityappendanim[t.entid][index].filename = t.value_s;
									}
								}
							}
						}
					}

					cmpStrConst( t_field_s, "drawcalloptimizer" );
					if (matched)  t.entityprofile[t.entid].drawcalloptimizer = t.value1;
					cmpStrConst( t_field_s, "drawcalloptimizeroff" );
					if (matched)  t.entityprofile[t.entid].drawcalloptimizeroff = t.value1;
					cmpStrConst( t_field_s, "drawcallscaleadjust" );
					if (matched)  t.entityprofile[t.entid].drawcallscaleadjust = t.value1;


					//  entity animation sets
					cmpStrConst( t_field_s, "ignorecsirefs" );
					if (  matched  )  t.entityprofile[t.entid].ignorecsirefs = t.value1;
					cmpStrConst( t_field_s, "playanimineditor" );
					if (matched)
					{
						// work out correct anim to play in editor
						int iRealPlayAnimInEditorIndex = t.value1;
						LPSTR pNumString = t.value_s.Get();
						int iIsAPureNumeric = 1;
						for (int n = 0; n < strlen(pNumString); n++)
						{
							if (pNumString[n] >= '0' && pNumString[n] <= '9')
							{
								// can have numbers in a name string
							}
							else
							{
								// found a non number, cannot be a numeric
								iIsAPureNumeric = 0;
								break;
							}
						}
						if (iIsAPureNumeric == 0 && strlen(t.value_s.Get())>0)
						{
							// not a numeric, is an anim name we can search for	
							t.entityprofile[t.entid].playanimineditor_name = t.value_s;
							t.entityprofile[t.entid].playanimineditor = -1;
							t.entityprofile[t.entid].startanimingame = 0;
						}
						else
						{
							// editor uses this to play
							t.entityprofile[t.entid].playanimineditor = iRealPlayAnimInEditorIndex;
							t.entityprofile[t.entid].playanimineditor_name = "";
							// startanimingame is still used in standalone.
							t.entityprofile[t.entid].startanimingame = iRealPlayAnimInEditorIndex;
						}
					}
					cmpStrConst( t_field_s, "animstyle" );
					if (  matched  )  
						t.entityprofile[t.entid].animstyle = t.value1;
					cmpStrConst( t_field_s, "animmax" );
					if (  matched ) 
					{
						t.tnewanimmax=t.value1 ; t.tstartofaianim=t.tnewanimmax;
					}
					if (  t.tnewanimmax>0 ) 
					{
						cmpNStrConst( t_field_s, "anim" );
						if (  matched  )  
						{
							const int rootindex = 4;
							int index = atoi( t_field_s+rootindex );
							bool bValid = false;
							if ( index == 0 )
							{
								if ( t_field_s[rootindex] == '0' && t_field_s[rootindex+1] == 0 ) bValid = true;
							}
							else if ( index < 10 )
							{
								if ( t_field_s[rootindex+1] == 0 ) bValid = true;
							}
							else if ( index < 100 )
							{
								if ( t_field_s[rootindex+2] == 0 ) bValid = true;
							}
							else if ( index < 1000 )
							{
								if ( t_field_s[rootindex+3] == 0 ) bValid = true;
							}
							else if ( index < 10000 )
							{
								if ( t_field_s[rootindex+4] == 0 ) bValid = true;
							}

							if ( bValid && index < t.tstartofaianim )
							{
								t.entityanim[t.entid][index].start = t.value1 ; 
								t.entityanim[t.entid][index].finish = t.value2 ; 
								t.entityanim[t.entid][index].found = 1;
							}
						}
						matched = false; // prevent skipping other strings that start with "anim"
					}

					// get foot fall data (optional)
					cmpStrConst( t_field_s, "footfallmax" );
					if (  matched  )  t.entityprofile[t.entid].footfallmax = t.value1;
					if (  t.entityprofile[t.entid].footfallmax>0 ) 
					{
						cmpNStrConst( t_field_s, "footfall" );
						if (  matched  )  
						{
							const int rootindex = 8;
							int index = atoi( t_field_s+rootindex );
							bool bValid = false;
							if ( index == 0 )
							{
								if ( t_field_s[rootindex] == '0' && t_field_s[rootindex+1] == 0 ) bValid = true;
							}
							else if ( index < 10 )
							{
								if ( t_field_s[rootindex+1] == 0 ) bValid = true;
							}
							else if ( index < 100 )
							{
								if ( t_field_s[rootindex+2] == 0 ) bValid = true;
							}
							else if ( index < 1000 )
							{
								if ( t_field_s[rootindex+3] == 0 ) bValid = true;
							}
							else if ( index < 10000 )
							{
								if ( t_field_s[rootindex+4] == 0 ) bValid = true;
							}

							if ( bValid && index < t.entityprofile[t.entid].footfallmax )
							{
								t.entityfootfall[t.entid][index].leftfootkeyframe = t.value1; 
								t.entityfootfall[t.entid][index].rightfootkeyframe = t.value2;
							}
						}
					}

					//  more data
					cmpStrConst( t_field_s, "quantity" );
					if (  matched  )  t.entityprofile[t.entid].quantity = t.value1;

					//  physics objects from the importer
					cmpStrConst( t_field_s, "physicscount" );
					if (  matched  )  t.entityprofile[t.entid].physicsobjectcount = t.value1;

					if (  cstr(Left(t.field_s.Get(),7)) == "physics" && t.field_s != "physicscount" && t.tPhysObjCount < MAX_ENTITY_PHYSICS_BOXES ) 
					{

						Dim (  t.tArray,10 );

							//  get rid of the quotation marks
							t.tStrip_s = t.value_s;
							t.tStrip_s = Left(t.tStrip_s.Get(), Len(t.tStrip_s.Get())-1);
							t.tStrip_s = Right(t.tStrip_s.Get(), Len(t.tStrip_s.Get())-1);

							t.tArrayMarker = 0;
							t.ttToken_s=FirstToken(t.tStrip_s.Get(),",");
							//PE: Make sure we only run it if we find a token.
							//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/979

							if (t.ttToken_s != "")
							{
								t.tArray[t.tArrayMarker] = t.ttToken_s;
								++t.tArrayMarker;

								do
								{
									t.ttToken_s = NextToken(",");
									if (t.ttToken_s != "")
									{
										t.tArray[t.tArrayMarker] = t.ttToken_s;
										++t.tArrayMarker;
									}
								} while (!(t.ttToken_s == ""));

								//  Format; shapetype, sizex, sizey, sizez, offx, offy, offz, rotx, roty, rotz
								t.tPShapeType = ValF(t.tArray[0].Get());
								//  is it a box?
								if (t.tPShapeType == 0)
								{
									//Dave Crash fix - check we are not going out of bounds
									if (t.entid < MAX_ENTITY_PHYSICS_BOXES * 2)
									{
										t.entityphysicsbox[t.entid][t.tPhysObjCount].SizeX = ValF(t.tArray[1].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].SizeY = ValF(t.tArray[2].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].SizeZ = ValF(t.tArray[3].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].OffX = ValF(t.tArray[4].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].OffY = ValF(t.tArray[5].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].OffZ = ValF(t.tArray[6].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].RotX = ValF(t.tArray[7].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].RotY = ValF(t.tArray[8].Get());
										t.entityphysicsbox[t.entid][t.tPhysObjCount].RotZ = ValF(t.tArray[9].Get());
									}
									++t.tPhysObjCount;
								}

							}

						UnDim (  t.tArray );

					}

				}
			}
		}
		UnDim (  t.data_s );

		for ( int i = 0 ; i <= t.tstartofaianim-1; i++ )
		{
			if ( t.entityanim[t.entid][i].found == 0 ) 
			{ 
				t.entityanim[t.entid][i].start = -1; 
				t.entityanim[t.entid][i].finish = -1; 
			}
		}

		if (t.entityprofile[t.entid].ismarker == 2) 
		{
			//PE: LMFIX - Light , default to 100.
			if (t.entityprofile[t.entid].defaultheight == 155)
				t.entityprofile[t.entid].defaultheight = 80;

			// special code to set this light to black but keep alpha
			if (t.entityprofile[t.entid].light.color == -100)
			{
				t.entityprofile[t.entid].light.color = (255 << 24);
			}
		}

		//  Finish animation quantities
		t.entityprofile[t.entid].animmax=t.tnewanimmax;
		t.entityprofile[t.entid].startofaianim=t.tstartofaianim;

		//  Localisation must change desc to local name
		if (  t.entityprofileheader[t.entid].desc_s != "" ) 
		{
			if (  cstr(Left(t.entityprofileheader[t.entid].desc_s.Get(),1)) != "%" ) 
			{
				// no LOC files in wicked for now
			}
		}

		//  Translate entity references inside entity profile (token translations)
		if (  cstr(Lower(t.entityprofileheader[t.entid].desc_s.Get())) == "%key" ) 
		{
			t.entityprofileheader[t.entid].desc_s=t.strarr_s[472];
		}
		if (  cstr(Lower(t.entityprofileheader[t.entid].desc_s.Get())) == "%light" ) 
		{
			t.entityprofileheader[t.entid].desc_s=t.strarr_s[473];
		}
		if (  cstr(Lower(t.entityprofileheader[t.entid].desc_s.Get())) == "%remote door" ) 
		{
			t.entityprofileheader[t.entid].desc_s=t.strarr_s[474];
		}
		if (  cstr(Lower(t.entityprofileheader[t.entid].desc_s.Get())) == "%teleporter in" ) 
		{
			t.entityprofileheader[t.entid].desc_s=t.strarr_s[615];
		}
		if (  cstr(Lower(t.entityprofileheader[t.entid].desc_s.Get())) == "%teleporter out" ) 
		{
			t.entityprofileheader[t.entid].desc_s=t.strarr_s[616];
		}
		if (  cstr(Lower(t.entityprofileheader[t.entid].desc_s.Get())) == "%lift" ) 
		{
			t.entityprofileheader[t.entid].desc_s=t.strarr_s[617];
		}
		if (  cstr(Lower(t.entityprofile[t.entid].usekey_s.Get())) == "%key" ) 
		{
			t.entityprofile[t.entid].usekey_s=t.strarr_s[472];
		}
		if (  cstr(Lower(t.entityprofile[t.entid].ifused_s.Get())) == "%light" ) 
		{
			t.entityprofile[t.entid].ifused_s=t.strarr_s[473];
		}
		if (  cstr(Lower(t.entityprofile[t.entid].ifused_s.Get())) == "%remote door" ) 
		{
			t.entityprofile[t.entid].ifused_s=t.strarr_s[474];
		}
		if (  cstr(Lower(t.entityprofile[t.entid].ifused_s.Get())) == "%teleporter in" ) 
		{
			t.entityprofile[t.entid].ifused_s=t.strarr_s[615];
		}
		if (  cstr(Lower(t.entityprofile[t.entid].ifused_s.Get())) == "%teleporter out" ) 
		{
			t.entityprofile[t.entid].ifused_s=t.strarr_s[616];
		}
		if (  cstr(Lower(t.entityprofile[t.entid].ifused_s.Get())) == "%lift" ) 
		{
			t.entityprofile[t.entid].ifused_s=t.strarr_s[617];
		}

		//  All profile defaults
		if (  t.entityprofile[t.entid].ismarker != 1 ) 
		{
			if (  t.entityprofile[t.entid].lives<1  )  t.entityprofile[t.entid].lives = 1;
		}
		if (  t.entityprofile[t.entid].speed == 0  )  t.entityprofile[t.entid].speed = 100;
		if (  t.entityprofile[t.entid].hurtfall == 0  )  t.entityprofile[t.entid].hurtfall = 100;

		//  Physics Data Defaults
		if (  t.entityprofile[t.entid].ismarker == 0 ) 
		{
			//  default physics settings (weight and friction done during object load (we need the obj size!)
			//  health packs have no physics by default for A compatibility
			if (  t.entityprofile[t.entid].ishealth != 0 ) 
			{
				t.entityprofile[t.entid].physics=0;
			}
		}
		else
		{
			t.entityprofile[t.entid].physics=0;
		}

		//LB: Additional assumption that objects with no collision should not have physics
		if (t.entityprofile[t.entid].collisionmode >= 11 && t.entityprofile[t.entid].collisionmode <= 12)
		{
			t.entityprofile[t.entid].physics = 0;
		}

		//  LOD System Defaults
		if ( t.entityprofile[t.entid].lod1distance > 0 && t.entityprofile[t.entid].lod2distance == 0 )
		{
			t.entityprofile[t.entid].lod2distance = t.entityprofile[t.entid].lod1distance;
		}

		//  Spawn defaults
		t.entityprofile[t.entid].spawnatstart=1;
		t.entityprofile[t.entid].spawndelayrandom=0;
		t.entityprofile[t.entid].spawnqtyrandom=0;
		t.entityprofile[t.entid].spawnvel=0;
		t.entityprofile[t.entid].spawnvelrandom=0;
		t.entityprofile[t.entid].spawnangle=0;
		t.entityprofile[t.entid].spawnanglerandom=0;
		t.entityprofile[t.entid].spawnlife=0;
		if (  t.entityprofile[t.entid].spawnmax>0 ) 
		{
			t.entityprofile[t.entid].spawnupto=t.entityprofile[t.entid].spawnmax;
			t.entityprofile[t.entid].spawnafterdelay=1;
			if (  t.entityprofile[t.entid].ischaracter == 1 ) 
			{
				t.entityprofile[t.entid].spawnwhendead=1;
			}
			else
			{
				t.entityprofile[t.entid].spawnwhendead=0;
			}
		}
		else
		{
			t.entityprofile[t.entid].spawnupto=0;
			t.entityprofile[t.entid].spawnafterdelay=0;
			t.entityprofile[t.entid].spawnwhendead=0;
		}

		//  Fix scale for FPE
		if (  t.entityprofile[t.entid].scale == 0 ) 
		{
			t.entityprofile[t.entid].scale=100;
		}

		// 010917 - if shader effect is a decal, auto switch zdepth flag (shader no longer does this in DX11)
		LPSTR pEffectMatch = "effectbank\\reloaded\\decal";
		if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pEffectMatch, strlen(pEffectMatch) ) == NULL ) 
		{
			t.entityprofile[t.entid].zdepth = 0;
		}

		// 261117 - intercept and replace any legacy shaders with new PBR ones if game visuals using RealtimePBR (3) mode
		if ( g.gpbroverride == 1 )
		{
			int iReplaceMode = 0;
			LPSTR pTryMatch = "effectbank\\reloaded\\entity_basic.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 1;
			pTryMatch = "effectbank\\reloaded\\character_static.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 1;
			pTryMatch = "effectbank\\reloaded\\entity_anim.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 2;
			pTryMatch = "effectbank\\reloaded\\character_basic.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 5;
			pTryMatch = "effectbank\\reloaded\\character_transparency.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 2;
			pTryMatch = "effectbank\\reloaded\\tree_basic.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 3;			
			pTryMatch = "effectbank\\reloaded\\treea_basic.fx";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 4;	
			if ( strlen ( t.entityprofile[t.entid].effect_s.Get() ) == 0 ) iReplaceMode = 1;
			if ( iReplaceMode > 0 )
			{
				if ( iReplaceMode == 1 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\apbr_basic.fx";
				if ( iReplaceMode == 2 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\apbr_anim.fx";
				if ( iReplaceMode == 3 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\apbr_tree.fx";
				if ( iReplaceMode == 4 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\apbr_treea.fx";
				if ( iReplaceMode == 5 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\apbr_animwithtran.fx";
			}
		}
		else
		{
			// 120418 - conversely, if PBR override not active, and have new PBR asset entities that still 
			// have old DNS textures, switch them back to classic non-PBR (this allows new PBR assets to 
			// replace older legacy assets but still allow backwards compatibility for users who want the
			// old shaders and old textures to remain in effect using PBR override of zero)
			char pEntityItemPath[1024];
			strcpy ( pEntityItemPath, t.ent_s.Get() );
			int n = 0;
			for ( n = strlen(pEntityItemPath)-1; n > 0; n-- )
			{
				if ( pEntityItemPath[n] == '\\' || pEntityItemPath[n] == '/' )
				{
					pEntityItemPath[n+1] = 0;
					break;
				}
			}
			if ( n <= 0 ) strcpy ( pEntityItemPath, "" );
			char pJustTextureName[1024];
			strcpy ( pJustTextureName, t.entityprofile[t.entid].texd_s.Get() );
			if ( strlen ( pJustTextureName ) > 4 )
			{
				pJustTextureName[strlen(pJustTextureName)-4]=0;
				if ( stricmp ( pJustTextureName+strlen(pJustTextureName)-6, "_color" ) == NULL )
				{
					pJustTextureName[strlen(pJustTextureName)-6]=0;
					strcat ( pJustTextureName, "_D" );
				}
				strcat ( pJustTextureName, ".png" );
			}
			char pReplaceWithDNS[1024];
			strcpy ( pReplaceWithDNS, pEntityItemPath );
			strcat ( pReplaceWithDNS, pJustTextureName );
			bool bReplacePBRWithNonPBRDNS = false;
			LPSTR pPBREffectMatch = "effectbank\\reloaded\\apbr";
			if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pPBREffectMatch, strlen(pPBREffectMatch) ) == NULL ) 
			{
				// entity effect specifies PBR, do we have the DNS files available
				if ( strlen ( pJustTextureName ) > 4 )
				{
					cstr pFindDNSFile = t.entdir_s + pReplaceWithDNS;
					if ( FileExist ( pFindDNSFile.Get() ) == 0 )
					{
						pReplaceWithDNS[strlen(pReplaceWithDNS)-4]=0;
						strcat ( pReplaceWithDNS, ".dds" );
						pFindDNSFile = t.entdir_s + pReplaceWithDNS;
						if ( FileExist ( pFindDNSFile.Get() ) == 0 )
						{
							pReplaceWithDNS[strlen(pReplaceWithDNS)-4]=0;
							strcat ( pReplaceWithDNS, ".jpg" );
							pFindDNSFile = t.entdir_s + pReplaceWithDNS;
							if ( FileExist ( pFindDNSFile.Get() ) == 1 )
							{
								bReplacePBRWithNonPBRDNS = true;
							}
						}
						else
						{
							bReplacePBRWithNonPBRDNS = true;
						}
					}
					else
					{
						bReplacePBRWithNonPBRDNS = true;
					}
				}
				else
				{
					// no texture specified, but can still switch to classic shaders (legacy behavior)
					bReplacePBRWithNonPBRDNS = true;
				}
			}
			if ( bReplacePBRWithNonPBRDNS == true )
			{
				// replace the shader used
				int iReplaceMode = 0;
				LPSTR pTryMatch = "effectbank\\reloaded\\apbr_basic.fx";
				if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 1;
				pTryMatch = "effectbank\\reloaded\\apbr_anim.fx";
				if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 2;
				pTryMatch = "effectbank\\reloaded\\apbr_tree.fx";
				if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 3;
				pTryMatch = "effectbank\\reloaded\\apbr_treea.fx";
				if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pTryMatch, strlen(pTryMatch) ) == NULL ) iReplaceMode = 4;
				if ( iReplaceMode > 0 )
				{
					if ( iReplaceMode == 1 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\entity_basic.fx";
					if ( iReplaceMode == 2 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\character_basic.fx";
					if ( iReplaceMode == 3 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\tree_basic.fx";
					if ( iReplaceMode == 4 ) t.entityprofile[t.entid].effect_s = "effectbank\\reloaded\\treea_basic.fx";
				}

				// replace the texture specified (from _color to _D)
				t.entityprofile[t.entid].texd_s = pJustTextureName;
			}
		}

		// if effect shader starts with APBR, auto shift effectprofile from zero to one
		LPSTR pPBREffectMatch = "effectbank\\reloaded\\apbr";
		if ( strnicmp ( t.entityprofile[t.entid].effect_s.Get(), pPBREffectMatch, strlen(pPBREffectMatch) ) == NULL ) 
		{
			if ( t.entityprofile[t.entid].effectprofile == 0 )
				t.entityprofile[t.entid].effectprofile = 1;
		}
	}

	//  Can loop back if skipBIN flag set
	if (  t.skipBINloadingandtryagain == 1 ) 
	{
		timestampactivity(0,cstr(cstr(Str(t.tprotectBINfile))+" Entity BIN File Out Of Date: "+t.tprofile_s).Get());
		if (  t.game.gameisexe == 1 ) 
		{
			t.skipBINloadingandtryagain=0;
		}
		else
		{
			if (  t.tprotectBINfile == 0 ) 
			{
				if (  FileExist(t.tprofile_s.Get()) == 1  )  DeleteAFile (  t.tprofile_s.Get() );
			}
			else
			{
				t.skipBINloadingandtryagain=0;
			}
		}
	}

	} while ( !(  t.skipBINloadingandtryagain == 0 ) );

	//  new field as we now have pure lights and entity lights
	if (  t.entityprofile[t.entid].ismarker == 2 || t.entityprofile[t.entid].ismarker == 5 ) 
	{
		if (  t.entityprofile[t.entid].ismarker == 5  )  t.entityprofile[t.entid].ismarker = 0;
		t.entityprofile[t.entid].islightmarker=1;
		//  FPGC - 300310 - entitylights always active as they may control a dynamic light and possibly decal-particle(mode7)
		t.entityprofile[t.entid].phyalways=1;
	}
	else
	{
		t.entityprofile[t.entid].islightmarker=0;
	}

	//  FPGC - 100610 - all FPGC characters are ALWAYS ACTIVE for full speed logic (more predictable)
	if (  t.entityprofile[t.entid].ischaracter == 1 && g.fpgcgenre == 0 ) 
	{
		t.entityprofile[t.entid].phyalways=1;
		//  FPGC - 110610 - and ALL are invincible
		t.entityprofile[t.entid].strength=0;
	}

	if (t.entityprofile[t.entid].ischaracter == 0)
	{
		// only characters can use isspinetracker mode (now on by default for wicked)
		t.entityprofile[t.entid].isspinetracker = 0;
	}

	//  fileexistelse
	}
	else
	{
		//  File not exist, provide debug information (only if file specified (old entities can be renamed and still hang around inside FPMs)
		if (  Len( cstr(t.entdir_s+t.ent_s).Get() )>Len("entitybank\\") ) 
		{
			debugfilename( cstr(t.entdir_s+t.ent_s).Get(),t.tprofile_s.Get() );
		}
	}

	//  V109 BETA5 - 250408 - flag material is being used
	if (  t.entityprofile[t.entid].materialindex>0 ) 
	{
		//t.mi=t.entityprofile[t.entid].materialindex-1;
		t.mi = t.entityprofile[t.entid].materialindex; //PE: We now use the correct materialindex -1 is from classic.
		if (t.mi < t.material.size())
		{
			t.material[t.mi].usedinlevel = 1;
		}
		else
		{
			// speciried a material index that MAX does not support!
		}
	}

	// if flagged as EBE, attempt to load any EBE cube data
	if ( t.entityprofile[t.entid].isebe != 0 )
	{
		cstr sEBEFile = cstr(Left(t.tFPEName_s.Get(),strlen(t.tFPEName_s.Get())-4)) + cstr(".ebe");
		if ( FileExist ( sEBEFile.Get() ) ) 
		{
			// load EBE data into entityID
			ebe_load_ebefile ( sEBEFile, t.entid );
		}
		else
		{
			// 300817 - EBE has had .ebe file removed to make it regular entity, so remove handle limb
			if ( t.entityprofile[t.entid].isebe == 1 ) 
			{
				t.entityprofile[t.entid].isebe = 2;
			}
		}
	}

	// set transparency for all markers
	if (t.entityprofile[t.entid].ismarker > 0 && t.entityprofile[t.entid].ismarker != 11)
		t.entityprofile[t.entid].transparency = 6;

	#ifndef GGMAXEDU
	// also, objects may reference old scripts, now can look for their new locations via Workshop items
	extern bool workshop_verifyandorreplacescript(int, int);
	if (workshop_verifyandorreplacescript(0, t.entid) == true)
	{
		// we replaced this script with one that exists elsewhere :)
	}
	#endif
}

void entity_loadvideoid ( void )
{
	t.tvideoid=0;
	t.text_s = Lower(Right(t.tvideofile_s.Get(),4));
	if ( t.text_s == ".ogv" || t.text_s == ".mp4" ) 
	{
		t.tvideoid=32;
		for ( t.tt = 1 ; t.tt<=  32; t.tt++ )
		{
			if ( AnimationExist(t.tt) == 0 ) { t.tvideoid = t.tt  ; break; }
		}
		char pFinalVideoFilePath[MAX_PATH];
		strcpy(pFinalVideoFilePath, t.tvideofile_s.Get());
		GG_GetRealPath(pFinalVideoFilePath, 0);
		if ( LoadAnimation(pFinalVideoFilePath, t.tvideoid, g.videoprecacheframes, g.videodelayedload, 1) == false )
		{
			t.tvideoid = -999;
		}
	}
}

