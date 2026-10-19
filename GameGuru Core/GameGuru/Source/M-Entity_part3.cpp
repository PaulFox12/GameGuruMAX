#define USEFASTLOADING
#ifdef USEFASTLOADING // continued from part2
int g_iAddEntityElementsMode = 0;

void c_entity_loadelementsdata ( void )
{
	// Free any old elements
	if (g_iAddEntityElementsMode == 0)
	{
		entity_deleteelementsdata ();
		if (t.elementsfilename_s == "")  t.elementsfilename_s = g.mysystem.levelBankTestMap_s + "map.ele";
	}

	// load entity element list
	t.failedtoload=0;
	//t.versionnumbersupported = 338;
	//t.versionnumbersupported = 341;
	t.versionnumbersupported = 342; // v342 adds eleprof.soundset4a_s (Sound4 slot) — matches production DX11

	if ( FileExist(t.elementsfilename_s.Get()) == 1 ) 
	{
		int iElementsInFile = 0;
		c_OpenToRead(1, t.elementsfilename_s.Get());
		t.versionnumberload = c_ReadLong ( 1 );
		if (  t.versionnumberload<100 ) 
		{
			//  Pre-version data - development only
			g.entityelementlist=t.versionnumberload;
			t.versionnumberload=100;
		}
		else
		{
			iElementsInFile = c_ReadLong (1);
			if (g_iAddEntityElementsMode == 0)
			{
				g.entityelementlist = iElementsInFile;
			}
			if (g_iAddEntityElementsMode == 1)
			{
				//g.entityelementlist += iElementsInFile;
			}
		}
		// DX12 PORT (instrument-trust): always record what entity-file version a
		// level actually loads, so A/B sessions can grep the log for it.
		{
			char pEleVersionMsg[MAX_PATH + 128];
			sprintf_s(pEleVersionMsg, "entity elements file: %s (version %d, supported max %d, %d elements)",
				t.elementsfilename_s.Get(), t.versionnumberload, t.versionnumbersupported, iElementsInFile);
			timestampactivity(0, pEleVersionMsg);
		}
		if ( t.versionnumberload <= t.versionnumbersupported )
		{
			if (iElementsInFile > 0)//g.entityelementlist>0 )
			{
				bool bFirstTimeOnlyToGrabGroupDataOops = true;
				if (g_iAddEntityElementsMode == 0)
				{
					UnDim (t.entityelement);
					UnDim2 (t.entityshadervar);
					UnDim (t.entitydebug_s);
					g.entityelementmax = g.entityelementlist;
					Dim (t.entityelement, g.entityelementmax);
					Dim2(t.entityshadervar, g.entityelementmax, g.globalselectedshadermax);
					Dim (t.entitydebug_s, g.entityelementmax);
				}
				if (g_iAddEntityElementsMode == 1 || g_iAddEntityElementsMode == 2)
				{
					// ensure there are enough free slots
					int iCount = 0;
					for (int finde = 1; finde <= g.entityelementlist; finde++)
					{
						if (t.entityelement[finde].bankindex == 0)
						{
							iCount++;
						}
					}
					if (iCount < iElementsInFile)
					{
						// make more space
						Dim (t.storeentityelement, g.entityelementmax);
						for (t.e = 1; t.e <= g.entityelementmax; t.e++)
						{
							t.storeentityelement[t.e] = t.entityelement[t.e];
						}
						UnDim (t.entityelement);
						UnDim (t.entityshadervar);
						int iOldSizeCount = g.entityelementmax;
						g.entityelementmax += iElementsInFile + 10;
						Dim (t.entityelement, g.entityelementmax);
						Dim2(t.entityshadervar, g.entityelementmax, g.globalselectedshadermax);
						for (t.e = 1; t.e <= iOldSizeCount; t.e++)
						{
							t.entityelement[t.e] = t.storeentityelement[t.e];
						}
					}
				}
				for ( int n = 1; n <= iElementsInFile; n++ )
				{
					bool bIncreasedListSize = false;
					if (g_iAddEntityElementsMode == 0)
					{
						t.e = n;
					}
					if (g_iAddEntityElementsMode == 1 || g_iAddEntityElementsMode == 2)
					{
						// find free slot in add mode
						bool bFoundFreeSlot = false;
						for (int finde = 1; finde <= g.entityelementlist; finde++)
						{
							if (t.entityelement[finde].bankindex == 0)
							{
								bFoundFreeSlot = true;
								t.e = finde;
								break;
							}
						}
						if (bFoundFreeSlot == false )
						{
							// increase max element list
							if (g.entityelementlist < g.entityelementmax - 1)
							{
								g.entityelementlist++;
								t.e = g.entityelementlist;
								bIncreasedListSize = true;
							}
							else
							{
								// this should never happen (see code above), but if so, just overwrite last slot
								t.e = g.entityelementmax - 1;
							}
						}

						// special flag so can handle entity with collection list later
						if(g_iAddEntityElementsMode == 1)
							t.entityelement[t.e].specialentityloadflag = 123;
					}
					if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
					//  actual file data
					if (  t.versionnumberload >= 101 ) 
					{
						//  Version 1.01
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].maintype=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].bankindex=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].staticflag=t.a;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].x=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].y=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].z=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].rx=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].ry=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].rz=t.a_f;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.name_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); // t.entityelement[t.e].eleprof.aiinit_s=t.a_s; //PE: Not used anymore.
						t.a_s = c_ReadString (1);
						if (strnicmp (t.a_s.Get(), "default.lua", 11) == NULL)
						{
							t.entityelement[t.e].eleprof.aimain_s = "no_behavior_selected.lua";
						}
						else
						{
							if (strlen(t.a_s.Get()) < 4)
							{
								t.entityelement[t.e].eleprof.aimain_s = "no_behavior_selected.lua";
							}
							else
							{
								t.entityelement[t.e].eleprof.aimain_s = t.a_s;
							}
						}
						t.a_s = c_ReadString ( 1 ); // t.entityelement[t.e].eleprof.aidestroy_s=t.a_s;  //PE: Not used anymore.
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.isobjective=t.a;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.usekey_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.ifused_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); //t.entityelement[t.e].eleprof.ifusednear_s=t.a_s;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.uniqueelement=t.a;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.texd_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.texaltd_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.effect_s=t.a_s;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.transparency=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].editorfixed=t.a;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.soundset_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.soundset1_s=t.a_s;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnmax=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawndelay=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnqty=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.hurtfall=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.castshadow=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.reducetexture=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.speed=t.a;
						t.a_s = c_ReadString ( 1 ); // t.entityelement[t.e].eleprof.aishoot_s=t.a_s;  //PE: Not used anymore.
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.hasweapon_s=t.a_s;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.lives=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].spawn.max=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].spawn.delay=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].spawn.qty=t.a;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.scale=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.coneheight=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.coneangle=t.a_f;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.strength=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.isimmobile=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.cantakeweapon=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.quantity=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.markerindex=t.a;
						t.a = c_ReadLong ( 1 ); t.dw=t.a ; t.dw=t.dw+0xFF000000 ; t.entityelement[t.e].eleprof.light.color=t.dw;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.light.range=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.trigger.stylecolor=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.trigger.waypointzoneindex=t.a;
						t.a_s = c_ReadString ( 1 ); // t.entityelement[t.e].eleprof.basedecal_s=t.a_s;  //PE: Not used anymore.
					}
					if (  t.versionnumberload >= 102 ) 
					{
						//  Version 1.02
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.rateoffire=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.damage=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.accuracy=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.reloadqty=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.fireiterations=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.lifespan=t.a;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.throwspeed=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.throwangle=t.a_f;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.bounceqty=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.explodeonhit=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.weaponisammo=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnupto=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnafterdelay=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnwhendead=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.perentityflags =t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.perentityflags =t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.perentityflags =t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.perentityflags =t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.perentityflags =t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.perentityflags =t.a;
					}
					if (  t.versionnumberload >= 103 ) 
					{
						//  Version 1.03 - V1 draft physics
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.physics=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.phyweight=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.phyfriction=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.phyforcedamage=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.rotatethrow=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.explodable=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.explodedamage=t.a;
						t.a = c_ReadLong ( 1 ); //t.entityelement[t.e].eleprof.phydw4=t.a;
						t.a = c_ReadLong ( 1 ); //t.entityelement[t.e].eleprof.phydw5=t.a;
					}
					if (  t.versionnumberload >= 104 ) 
					{
						//  Version 1.04 - BETA4 extra field
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.phyalways=t.a;
					}
					if (  t.versionnumberload >= 105 ) 
					{
						//  Version 1.05 - BETA8 extra fields
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawndelayrandom=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnqtyrandom=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnvel=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnvelrandom=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnangle=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnanglerandom=t.a;
					}
					if (  t.versionnumberload >= 106 ) 
					{
						//  Version 1.06 - BETA10 extra fields
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnatstart=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.spawnlife=t.a;
					}
					if (  t.versionnumberload >= 107 ) 
					{
						//  FPSCV104RC8 - forgot to save infinilight index (dynamic lights in final build never worked)
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.light.index=t.a;
					}
					if (  t.versionnumberload >= 199 ) 
					{
						//  X10 EXTRAS - Ignored in X9
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
					}
					if (  t.versionnumberload >= 200 ) 
					{
						//  X10 EXTRAS 190707 - Ignored in X9
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
						t.a = c_ReadLong ( 1 );
					}
					if (  t.versionnumberload >= 217 ) 
					{
						//  FPGC - 300710 - save new entity element data
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particleoverride=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.offsety=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.scale=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.randomstartx=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.randomstarty=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.randomstartz=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.linearmotionx=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.linearmotiony=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.linearmotionz=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.randommotionx=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.randommotiony=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.randommotionz=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.mirrormode=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.camerazshift=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.scaleonlyx=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.lifeincrement=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.alphaintensity=t.a;
					}
					if (  t.versionnumberload >= 218 ) 
					{
						//  V118 - 060810 - knxrb - Decal animation setting (Added animation choice setting).
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.particle.animated=t.a;
					}
					if (  t.versionnumberload >= 301 ) 
					{
						//  Reloaded ALPHA 1.0045
						t.a_s = c_ReadString ( 1 ); //t.entityelement[t.e].eleprof.aiinitname_s=t.a_s; //PE: Not used anymore.
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.aimainname_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); //t.entityelement[t.e].eleprof.aidestroyname_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); //t.entityelement[t.e].eleprof.aishootname_s=t.a_s;
					}
					if (  t.versionnumberload >= 302 ) 
					{
						//  Reloaded BETA 1.005
					}
					if (  t.versionnumberload >= 303 ) 
					{
						//  Reloaded BETA 1.007
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.animspeed=t.a;
					}
					if (  t.versionnumberload >= 304 ) 
					{
						//  Reloaded BETA 1.007-200514
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.conerange=t.a_f;
					}
					if (  t.versionnumberload >= 305 ) 
					{
						//  Reloaded BETA 1.0085
						t.a_f = c_ReadFloat ( 1 ); 
						if ( t.a_f > 1e8 ) t.a_f = 0;
						t.entityelement[t.e].scalex=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); 
						if ( t.a_f > 1e8 ) t.a_f = 0;
						t.entityelement[t.e].scaley=t.a_f;
						t.a_f = c_ReadFloat ( 1 ); 
						if ( t.a_f > 1e8 ) t.a_f = 0;
						t.entityelement[t.e].scalez=t.a_f;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.range=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.dropoff=t.a;
					}
					if (  t.versionnumberload >= 306 ) 
					{
						//  GameGuru 1.00.010
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.isviolent=t.a;
					}
					if (  t.versionnumberload >= 307 ) 
					{
						//  GameGuru 1.00.020
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.explodeheight =t.a;
					}
					if (  t.versionnumberload >= 308 ) 
					{
						//  GameGuru 1.01.001
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.usespotlighting=t.a;
					}
					if (  t.versionnumberload >= 309 ) 
					{
						//  GameGuru 1.01.002
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.lodmodifier=t.a;
					}
					if (  t.versionnumberload >= 310 ) 
					{
						//  GameGuru 1.133
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.isocluder=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.isocludee=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.colondeath=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.parententityindex=t.a;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.parentlimbindex=t.a;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.soundset2_s=t.a_s;
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.soundset3_s=t.a_s;
						t.a_s = c_ReadStringIncl0xA( 1 ); t.entityelement[t.e].eleprof.soundset4_s=t.a_s;
					}
					if (  t.versionnumberload >= 311 ) 
					{
						//  GameGuru 1.133B
						t.a_f = c_ReadFloat ( 1 ); t.entityelement[t.e].eleprof.lootpercentage = t.a_f; // was specularperc
					}
					if (  t.versionnumberload >= 312 ) 
					{
						//  GameGuru 1.14 EBE
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].iHasParentIndex = t.a;
					}
					if (  t.versionnumberload >= 313 ) 
					{
						// VRQ V3
						t.a_s = c_ReadString ( 1 ); t.entityelement[t.e].eleprof.voiceset_s=t.a_s;
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.voicerate = t.a;
					}
					if (t.versionnumberload >= 314)
					{
						//PE: we need to copy t.entityprofile[t.ttentid].WEMaterial before customizing.
						int tmaster = t.entityelement[t.e].bankindex;
						if (tmaster < t.entityprofile.size())
						{
							t.entityelement[t.e].eleprof.WEMaterial = t.entityprofile[tmaster].WEMaterial;
						}

						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.bCustomWickedMaterialActive = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.MaterialActive = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bCastShadows[0] = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bDoubleSided[0] = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bPlanerReflection[0] = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bTransparency[0] = t.a;

						unsigned long ulValue = 0;
						t.a_s = c_ReadString(1);
						sscanf(t.a_s.Get(), "%lu", &ulValue);
						t.entityelement[t.e].eleprof.WEMaterial.dwBaseColor[0] = ulValue;

						t.a_s = c_ReadString(1);
						sscanf(t.a_s.Get(), "%lu", &ulValue);
						t.entityelement[t.e].eleprof.WEMaterial.dwEmmisiveColor[0] = ulValue;

						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fReflectance[0] = t.a_f;

						// LB: in previous builds, only one mesh material details where stored, we can retain this for single material objects in slot 0
						t.a = c_ReadLong(1);
						int iFrameAndWEMaterialSlotIndex = 0;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.baseColorMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.normalMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.surfaceMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.displacementMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.emissiveMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
						t.a_s = c_ReadString(1);
						#ifndef DISABLEOCCLUSIONMAP
						t.entityelement[t.e].eleprof.WEMaterial.occlusionMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
						#endif
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fNormal[iFrameAndWEMaterialSlotIndex] = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fRoughness[iFrameAndWEMaterialSlotIndex] = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fMetallness[iFrameAndWEMaterialSlotIndex] = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fEmissive[iFrameAndWEMaterialSlotIndex] = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fAlphaRef[iFrameAndWEMaterialSlotIndex] = t.a_f;
					}
					if (t.versionnumberload >= 315)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.light.fLightHasProbe = t.a;
						// GGMAX 2.90: fLightHasProbe is a FLAG, not a range — every consumer
						// tests it as >= 50, and the old "Probe Range" slider that wrote 50..500
						// here could never do anything (Scene::RunProbeUpdateSystem overwrites
						// probe.range from the transform scale each update). Levels made before
						// 2.90 carry arbitrary values in that band, so canonicalise them to 50
						// on load: identical behaviour, one meaning. Values < 50 mean "not a
						// probe" and are left exactly as they are.
						if (t.entityelement[t.e].eleprof.light.fLightHasProbe >= 50.0f)
							t.entityelement[t.e].eleprof.light.fLightHasProbe = 50.0f;
					}
					if (t.versionnumberload >= 316)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectLinkID = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iCharAlliance = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iCharFaction = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectReserved1 = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectReserved2 = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectReserved3 = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iCharPatrolMode = t.a;
						t.a = c_ReadFloat(1); t.entityelement[t.e].eleprof.fCharRange[0] = t.a;
						t.a = c_ReadFloat(1); t.entityelement[t.e].eleprof.fCharRange[1] = t.a;
						for (int i = 0;i < 10;i++)
						{
							t.a = c_ReadFloat(1); t.entityelement[t.e].eleprof.fObjectDataReserved[i] = t.a;
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectRelationships[i] = t.a;
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectRelationshipsType[i] = t.a;
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iObjectRelationshipsData[i] = t.a;
						}
					}
					if (t.versionnumberload >= 317)
					{
						for (int i = 1; i < MAXMESHMATERIALS; i++)
						{
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bCastShadows[i] = t.a;
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bDoubleSided[i] = t.a;
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bPlanerReflection[i] = t.a;
							t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.bTransparency[i] = t.a;

							unsigned long ulValue = 0;
							t.a_s = c_ReadString(1);
							sscanf(t.a_s.Get(), "%lu", &ulValue);
							t.entityelement[t.e].eleprof.WEMaterial.dwBaseColor[i] = ulValue;

							t.a_s = c_ReadString(1);
							sscanf(t.a_s.Get(), "%lu", &ulValue);
							t.entityelement[t.e].eleprof.WEMaterial.dwEmmisiveColor[i] = ulValue;

							t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fReflectance[i] = t.a_f;

							int iFrameAndWEMaterialSlotIndex = i;
							t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.baseColorMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
							t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.normalMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
							t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.surfaceMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
							t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.displacementMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
							t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.emissiveMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
							t.a_s = c_ReadString(1);
							#ifndef DISABLEOCCLUSIONMAP
							t.entityelement[t.e].eleprof.WEMaterial.occlusionMapName[iFrameAndWEMaterialSlotIndex] = t.a_s;
							#endif
							t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fNormal[iFrameAndWEMaterialSlotIndex] = t.a_f;
							t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fRoughness[iFrameAndWEMaterialSlotIndex] = t.a_f;
							t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fMetallness[iFrameAndWEMaterialSlotIndex] = t.a_f;
							t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fEmissive[iFrameAndWEMaterialSlotIndex] = t.a_f;
							t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fAlphaRef[iFrameAndWEMaterialSlotIndex] = t.a_f;
						}
					}
					if (t.versionnumberload >= 318)
					{
						t.a = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fRenderOrderBias[0] = t.a;
						for (int i = 1; i < MAXMESHMATERIALS; i++)
						{
							t.a = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.fRenderOrderBias[i] = t.a;
						}
					}
					if (t.versionnumberload >= 319)
					{
						//PE: The same group data is stored under ALL t.e and get reset each time.
						//PE: So level with 1000 objects , WILL setup groups and load ALL group images 1000 times ?
						//PE: We cant change old level now, so use this hack.
						//PE: Also this was leaking mem , not sure where. anyway this hack fix it. New maps will only save it under t.e == 1
						extern int g_iUniqueGroupID;
						t.a = c_ReadLong(1); 
						if (t.a > 0) g_iUniqueGroupID = t.a; // strange bug that resets this t.a to zero, the writer dumped in zeros!!
						int iNumberOfGroups = 0;
						t.a = c_ReadLong(1); iNumberOfGroups = t.a;
						for (int gi = 0; gi < iNumberOfGroups; gi++)
						{
							if (bFirstTimeOnlyToGrabGroupDataOops == true) vEntityGroupList[gi].clear();
							int iItemsInThisGroup = 0;
							t.a = c_ReadLong(1); iItemsInThisGroup = t.a;
							for (int i = 0; i < iItemsInThisGroup; i++)
							{
								sRubberBandType item;
								t.a = c_ReadLong(1); item.iGroupID = t.a;
								t.a = c_ReadLong(1); item.iParentGroupID = t.a;
								t.a = c_ReadLong(1); item.e = t.a;
								t.a = c_ReadFloat(1); item.x = t.a;
								t.a = c_ReadFloat(1); item.y = t.a;
								t.a = c_ReadFloat(1); item.z = t.a;
								t.a = c_ReadFloat(1); item.quatAngle.x = t.a;
								t.a = c_ReadFloat(1); item.quatAngle.y = t.a;
								t.a = c_ReadFloat(1); item.quatAngle.z = t.a;
								t.a = c_ReadFloat(1); item.quatAngle.w = t.a;
								if (bFirstTimeOnlyToGrabGroupDataOops == true) vEntityGroupList[gi].push_back(item);
							}
						}
						// and load in group thumb images, and load them into the iEntityGroupListImage image list (so can see them in groups tab)
						extern int iEntityGroupListImage[MAXGROUPSLISTS];
						for (int gi = 0; gi < iNumberOfGroups; gi++)
						{
							if (bFirstTimeOnlyToGrabGroupDataOops==true) iEntityGroupListImage[gi] = 0;
							t.a = c_ReadLong(1); int iHasImage = t.a;
							if (iHasImage == 1 && bFirstTimeOnlyToGrabGroupDataOops == true)
							{
								char pGroupImgFilename[MAX_PATH];
								sprintf(pGroupImgFilename, "%sgroupimg%d.png", g.mysystem.levelBankTestMap_s.Get(), gi);
								if (FileExist(pGroupImgFilename) == 1)
								{
									//Find free image id.
									int iImageID = 0;
									for (int i = 0; i < MAXGROUPSLISTS; i++)
									{
										bool bAlreadyUsed = false;
										int iNewImageID = (110000 + 9000) + i;// (g.perentitypromptimageoffset + 9000) + i;
										for (int l = MAXGROUPSLISTS; l > 0; l--)
										{
											if (iEntityGroupListImage[l] == iNewImageID)
											{
												bAlreadyUsed = true;
												break;
											}
										}
										if (!bAlreadyUsed)
										{
											iImageID = iNewImageID;
											break;
										}
									}
									if (iImageID != 0)
									{
										image_setlegacyimageloading(true);
										LoadImage(pGroupImgFilename, iImageID);
										image_setlegacyimageloading(false);
										iEntityGroupListImage[gi] = iImageID;
									}
								}
							}
						}

						// subsequent entities will just read the data but do nothing with vEntityGroupList!
						bFirstTimeOnlyToGrabGroupDataOops = false;
					}

					if (t.versionnumberload >= 320)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.newparticle.bParticle_Preview = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.newparticle.bParticle_Show_At_Start = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.newparticle.bParticle_Looping_Animation = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.newparticle.bParticle_Full_Screen = t.a;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.newparticle.fParticle_Fullscreen_Duration = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.newparticle.fParticle_Fullscreen_Fadein = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.newparticle.fParticle_Fullscreen_Fadeout = t.a_f;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.newparticle.Particle_Fullscreen_Transition = t.a_s;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.newparticle.fParticle_Speed = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.newparticle.fParticle_Opacity = t.a_f;
					}
					if (t.versionnumberload >= 321)
					{
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.newparticle.emittername= t.a_s;
					}
					if (t.versionnumberload >= 322)
					{
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].fDecalSpeed = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].fDecalOpacity = t.a_f;
					}
					if (t.versionnumberload >= 323)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iOverrideCollisionMode = t.a;
					}
					if (t.versionnumberload >= 324)
					{
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.weapondamagemultiplier = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.meleedamagemultiplier = t.a_f;
					}
					if (t.versionnumberload >= 325)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iAffectedByGravity = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iMoveSpeed = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iTurnSpeed = t.a;
					}

					if (t.versionnumberload >= 326)
					{
						t.a = c_ReadLong ( 1 ); t.entityelement[t.e].eleprof.light.offsetup =t.a; //Store spot radius.
					}
					if (t.versionnumberload >= 327)
					{
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.soundset5_s = t.a_s;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.soundset6_s = t.a_s;
					}
					if (t.versionnumberload >= 328)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iUseSoundVariants = t.a;
					}
					if (t.versionnumberload >= 329)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].quatmode = t.a;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].quatx = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].quaty = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].quatz = t.a_f;
						t.a_f = c_ReadFloat(1); t.entityelement[t.e].quatw = t.a_f;
					}
					if (t.versionnumberload >= 330)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.bAutoFlatten = t.a;
					}
					if (t.versionnumberload >= 331)
					{
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.overrideanimset_s = t.a_s;
					}
					if (t.versionnumberload >= 332)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iscollectable = t.a;
					}
					if (t.versionnumberload >= 333)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iSwimSpeed = t.a;
					}
					if (t.versionnumberload >= 334)
					{
						extern cstr sEntityGroupListName[MAXGROUPSLISTS];
						int iNumberOfGroups = c_ReadLong(1);
						for (int gi = 0; gi < iNumberOfGroups; gi++)
						{
							t.a_s = c_ReadString(1);
							sEntityGroupListName[gi] = t.a_s;
						}
					}
					if (t.versionnumberload >= 335)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].creationOfGroupID = t.a;
					}
					if (t.versionnumberload >= 336)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.light.fLightHasProbeX = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.light.fLightHasProbeY = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.light.fLightHasProbeZ = t.a;
					}
					if (t.versionnumberload >= 337)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].iCanGoUnderwater = t.a;
					}
					if (t.versionnumberload >= 338)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.clipcapacity = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.weaponpropres1 = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.weaponpropres2 = t.a;
					}
					if (t.versionnumberload >= 339)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderID = t.a;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam1 = t.a_f;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam2 = t.a_f;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam3 = t.a_f;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam4 = t.a_f;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam5 = t.a_f;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam6 = t.a_f;
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.WEMaterial.customShaderParam7 = t.a_f;
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.explodable_decalname = t.a_s;
					}
					if (t.versionnumberload >= 340)
					{
						//PE: For next version add: used_old_particle_effect. bindtoMeshID.
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.WEMaterial.WPEffect = t.a_s;
						//PE: Add some fillers we can use later.
						float fFiller;
						int iFiller;
						cstr sFiller;
						
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].eleprof.light.fProbeBrightness = t.a_f;

						// GGMAX 3.41: mirrors the save side in M-Entity_part4.cpp. DX11's record is the BASE for every
						// version up to 342 - slot 1 iAllowBuletHole, slot 2 iMaterialSoundIndex, both LONG. These two
						// sides must change together or every field after this point is read at the wrong offset.
						t.a = c_ReadLong(1); t.entityelement[t.e].iAllowBuletHole = t.a;             // DX11 slot 1
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.iMaterialSoundIndex = t.a; // DX11 slot 2
						t.a = t.a_f = c_ReadFloat(1); t.entityelement[t.e].iClearVegetation = (int)t.a_f;
						t.a = t.a_f = c_ReadFloat(1); fFiller = t.a_f;
						t.a = c_ReadLong(1);
						t.entityelement[t.e].eleprof.systemwide_lua = t.a;
						if (t.entityelement[t.e].eleprof.systemwide_lua > 1)
							t.entityelement[t.e].eleprof.systemwide_lua = 0;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.isobjective_alwaysactive = t.a;
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.isProjectGlobal = t.a;
						t.a_s = c_ReadString(1); sFiller = t.a_s;
						t.a_s = c_ReadString(1); sFiller = t.a_s;
						t.a_s = c_ReadString(1); sFiller = t.a_s;
					}
					if (t.versionnumberload >= 341)
					{
						t.a = c_ReadLong(1); t.entityelement[t.e].eleprof.bUseFPESettings = t.a;
					}
					if (t.versionnumberload >= 342)
					{
						t.a_s = c_ReadString(1); t.entityelement[t.e].eleprof.soundset4a_s = t.a_s;
					}

					// get the index of the entity profile
					t.ttentid=t.entityelement[t.e].bankindex;
					if (t.ttentid >= t.entityprofile.size())
					{
						// somehow, the entity profile bank index was corrupted
						t.ttentid = 0;
						t.entityelement[t.e].bankindex = 0;
						continue;
					}

					// if added element already exists with same parent in list, we do not need to add it
					if (g_iAddEntityElementsMode == 1 && t.entityelement[t.e].bankindex > 0 )
					{
						bool bElementExists = false;
						for (int finde = 1; finde <= g.entityelementlist; finde++)
						{
							if (finde != t.e && t.entityelement[finde].bankindex == t.entityelement[t.e].bankindex)
							{
								bElementExists = true;
								break;
							}
						}
						if (bElementExists == true)
						{
							// we do not keep this element!
							t.ttentid = 0;
							t.entityelement[t.e].bankindex = 0;
							t.entityelement[t.e].specialentityloadflag = 0;
							// and step back so this list size does not run away from us
							if (bIncreasedListSize == true)
							{
								g.entityelementlist--;
							}
							// go to next element in ELE sequence
							continue;
						}
					}

					if (t.entityelement[t.e].eleprof.light.fProbeBrightness == 0)
						t.entityelement[t.e].eleprof.light.fProbeBrightness = 1.0f;

					// fill in the blanks if load older version
					if (  t.versionnumberload<103 ) 
					{
						//  Version 1.03 - V1 draft physics (-1 means calculate at entobj-loadtime)
						t.entityelement[t.e].eleprof.physics=t.entityprofile[t.ttentid].physics;
						t.entityelement[t.e].eleprof.phyweight=t.entityprofile[t.ttentid].phyweight;
						t.entityelement[t.e].eleprof.phyfriction=t.entityprofile[t.ttentid].phyfriction;
						t.entityelement[t.e].eleprof.phyforcedamage=t.entityprofile[t.ttentid].phyforcedamage;
						t.entityelement[t.e].eleprof.rotatethrow=t.entityprofile[t.ttentid].rotatethrow;
						t.entityelement[t.e].eleprof.explodable=t.entityprofile[t.ttentid].explodable;
						//t.entityelement[t.e].eleprof.phydw3=0;
						//t.entityelement[t.e].eleprof.phydw4=0;
						//t.entityelement[t.e].eleprof.phydw5=0;
					}
					if (  t.versionnumberload<104 ) 
					{
						//  Version 1.04 - BETA4 extra field
						t.entityelement[t.e].eleprof.phyalways=t.entityprofile[t.ttentid].phyalways;
					}
					if (  t.versionnumberload<105 ) 
					{
						//  Version 1.05 - BETA8
						t.entityelement[t.e].eleprof.spawndelayrandom=t.entityprofile[t.ttentid].spawndelayrandom;
						t.entityelement[t.e].eleprof.spawnqtyrandom=t.entityprofile[t.ttentid].spawnqtyrandom;
						t.entityelement[t.e].eleprof.spawnvel=t.entityprofile[t.ttentid].spawnvel;
						t.entityelement[t.e].eleprof.spawnvelrandom=t.entityprofile[t.ttentid].spawnvelrandom;
						t.entityelement[t.e].eleprof.spawnangle=t.entityprofile[t.ttentid].spawnangle;
						t.entityelement[t.e].eleprof.spawnanglerandom=t.entityprofile[t.ttentid].spawnanglerandom;
					}
					if (  t.versionnumberload<106 ) 
					{
						//  Version 1.06 - BETA10
						t.entityelement[t.e].eleprof.spawnatstart=t.entityprofile[t.ttentid].spawnatstart;
						t.entityelement[t.e].eleprof.spawnlife=t.entityprofile[t.ttentid].spawnlife;
					}
					if (  t.versionnumberload<217 ) 
					{
						//  FPGC - 300710 - older levels dont use particle override
						t.entityelement[t.e].eleprof.particleoverride=0;
					}
					if (  t.versionnumberload<303 ) 
					{
						//  Reloaded BETA 1.007
						t.entityelement[t.e].eleprof.animspeed=t.entityprofile[t.ttentid].animspeed;
					}
					if (  t.versionnumberload<304 ) 
					{
						//  Reloaded BETA 1.007-200514
						t.entityelement[t.e].eleprof.conerange=t.entityprofile[t.ttentid].conerange;
					}
					if (  t.versionnumberload<306 ) 
					{
						//  GameGuru 1.00.010
						t.entityelement[t.e].eleprof.isviolent=t.entityprofile[t.ttentid].isviolent;
					}
					if (  t.versionnumberload<307 ) 
					{
						//  GameGuru 1.00.020
						t.entityelement[t.e].eleprof.explodeheight =t.entityprofile[t.ttentid].explodeheight;
					}
					if (  t.versionnumberload<310 ) 
					{
						//  GameGuru 1.133
						t.entityelement[t.e].eleprof.isocluder=t.entityprofile[t.ttentid].isocluder;
						t.entityelement[t.e].eleprof.isocludee=t.entityprofile[t.ttentid].isocludee;
						t.entityelement[t.e].eleprof.colondeath=t.entityprofile[t.ttentid].colondeath;
						t.entityelement[t.e].eleprof.parententityindex=t.entityprofile[t.ttentid].parententityindex;
						t.entityelement[t.e].eleprof.parentlimbindex=t.entityprofile[t.ttentid].parentlimbindex;
						t.entityelement[t.e].eleprof.soundset2_s=t.entityprofile[t.ttentid].soundset2_s;
						t.entityelement[t.e].eleprof.soundset3_s=t.entityprofile[t.ttentid].soundset3_s;
						t.entityelement[t.e].eleprof.soundset4_s=t.entityprofile[t.ttentid].soundset4_s;
					}
					if (  t.versionnumberload<311 ) 
					{
						//  GameGuru 1.133B
						t.entityelement[t.e].eleprof.lootpercentage =t.entityprofile[t.ttentid].lootpercentage;
					}
					if (  t.versionnumberload<312 ) 
					{
						//  GameGuru 1.14 EBE
						t.entityelement[t.e].iHasParentIndex = 0;
					}
					if (  t.versionnumberload < 313 ) 
					{
						// VRQ V3
						t.entityelement[t.e].eleprof.voiceset_s=t.entityprofile[t.ttentid].voiceset_s;
						t.entityelement[t.e].eleprof.voicerate=t.entityprofile[t.ttentid].voicerate;
					}
					if (t.versionnumberload < 330)
					{
						t.entityelement[t.e].eleprof.bAutoFlatten = false;
					}
					if (t.versionnumberload < 341)
					{
						//PE: Default to false on old levels , so we don't overwrite users custom settings.
						//PE: if !bCustomWickedMaterialActive we can use bUseFPESettings.
						if (t.entityelement[t.e].eleprof.bCustomWickedMaterialActive)
							t.entityelement[t.e].eleprof.bUseFPESettings = false;
						else
							t.entityelement[t.e].eleprof.bUseFPESettings = true;

						t.entityelement[t.e].eleprof.iMaterialSoundIndex = 0;
					}
					//t.entityelement[t.e].entitydammult_f=1.0; not used any more, reused field for iCanGoUnderwater and renamed entitydammult_f to reserved2
					//t.entityelement[t.e].entityacc=1.0;

					// 131115 - transparency control was removed from GG properties IDE, so ensure
					// it reflects the latest entity profile information (until we allow this value back in)
					t.entityelement[t.e].eleprof.transparency = t.entityprofile[t.ttentid].transparency;
				}
			}
		}
		else
		{
			t.failedtoload=1;
			// DX12 PORT (instrument-trust): make the version-mismatch drop LOUD.
			// Without this the level LOOKS loaded (terrain/trees/grass come
			// through their own files) but every entity is silently missing —
			// invisibly corrupting DX11-vs-DX12 A/B comparisons. Production DX11
			// writes .ele v342; this port reads v341 max until the reader is
			// extended (see SCRATCHPAD.md Tech Debt).
			char pVersionWarning[MAX_PATH + 256];
			sprintf_s(pVersionWarning,
				"ENTITY LOAD SKIPPED: %s is version %d but this build supports max %d - ALL entities in this level are MISSING. Do not trust visual comparisons until the new .ele version is ported.",
				t.elementsfilename_s.Get(), t.versionnumberload, t.versionnumbersupported);
			timestampactivity(0, pVersionWarning);
			extern bool g_bAutomationActive;
			if (!g_bAutomationActive)
			{
				MessageBoxA(NULL, pVersionWarning, "Level Version Mismatch", MB_OK | MB_ICONWARNING);
			}
		}
		c_CloseFile (  1 );

		// can change field values here if updates to engine move vital resources
		// MAY2025 - moved all default animation files to animations\set folder so can centrally
		// add new animations and all existing and new characters/logic can take advantage of new ones
		// DX12 PORT GUARD: this loop must not run if the version check above failed — the
		// allocation block on lines 47-56 is skipped when versionnumberload > versionnumbersupported,
		// so entityelementlist is a bogus value from the newer file and t.entityelement is still
		// sized for the previous level. Loading TESTPROJ1 saved by production DX11 GameGuru MAX
		// (which writes a higher .ele version than the DX12 port supports) crashed here with
		// access violation. Guard is defensive: also bounds against entityelementmax in case
		// any other path leaves the two counters desynced. Root-cause fix (bump
		// versionnumbersupported / handle the newer file layout) is deferred — see
		// SCRATCHPAD.md "Level file version compatibility (DX11 -> DX12)".
		for (t.e = 1; t.failedtoload == 0 && t.e <= g.entityelementlist && t.e <= g.entityelementmax; t.e++)
		{
			if (t.entityelement[t.e].bankindex>0)
			{
				if ( t.entityelement[t.e].eleprof.overrideanimset_s.Len()>0)
				{
					char pFileLocation[MAX_PATH];
					strcpy(pFileLocation, t.entityelement[t.e].eleprof.overrideanimset_s.Get());
					LPSTR pPartLocation = strstr(pFileLocation, "charactercreatorplus\\parts\\");
					if (pPartLocation != NULL)
					{
						pPartLocation+=strlen("charactercreatorplus\\");
						*pPartLocation = '\0';
						char pDefAnimFileLocation[MAX_PATH];
						strcpy(pDefAnimFileLocation, pFileLocation);
						strcat(pDefAnimFileLocation, "animations\\sets\\");
						strcat(pDefAnimFileLocation, pPartLocation+strlen("parts") + 1);
						t.entityelement[t.e].eleprof.overrideanimset_s = pDefAnimFileLocation;
					}
				}
			}
		}

		// If replacement file active, can swap in new SCRIPT and SOUND references
		if(g_iAddEntityElementsMode==0)
		{
			if (Len(t.editor.replacefilepresent_s.Get()) > 1)
			{
				// now go through ELEPROF enrties to update any SCRIPTBANK references and SOUNDSET references
				for (t.e = 1; t.e <= g.entityelementlist; t.e++)
				{
					for (t.tcheck = 1; t.tcheck <= 9; t.tcheck++)
					{
						if (t.tcheck == 1)  t.tcheck_s = t.entityelement[t.e].eleprof.aimain_s;
						if (t.tcheck == 2)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset_s;
						if (t.tcheck == 3)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset1_s;
						if (t.tcheck == 4)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset2_s;
						if (t.tcheck == 5)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset3_s;
						if (t.tcheck == 6)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset4_s;
						if (t.tcheck == 7)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset5_s;
						if (t.tcheck == 8)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset6_s;
						if (t.tcheck == 9)  t.tcheck_s = t.entityelement[t.e].eleprof.soundset4a_s;
						t.ttry_s = "";
						for (t.nn = 1; t.nn <= Len(t.tcheck_s.Get()); t.nn++)
						{
							t.ttry_s = t.ttry_s + Mid(t.tcheck_s.Get(), t.nn);
							if ((cstr(Mid(t.tcheck_s.Get(), t.nn)) == "\\" && cstr(Mid(t.tcheck_s.Get(), t.nn + 1)) == "\\") || (cstr(Mid(t.tcheck_s.Get(), t.nn)) == "/" && cstr(Mid(t.tcheck_s.Get(), t.nn + 1)) == "/"))
							{
								++t.nn;
							}
						}
						t.ttry_s = Lower(t.ttry_s.Get());
						for (t.tt = 1; t.tt <= t.treplacementmax; t.tt++)
						{
							if (t.replacements_s[t.tt][0] == t.ttry_s)
							{
								//  found entry we can replace
								if (t.tcheck == 1) { t.entityelement[t.e].eleprof.aimain_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 2) { t.entityelement[t.e].eleprof.soundset_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 3) { t.entityelement[t.e].eleprof.soundset1_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 4) { t.entityelement[t.e].eleprof.soundset2_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 5) { t.entityelement[t.e].eleprof.soundset3_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 6) { t.entityelement[t.e].eleprof.soundset4_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 7) { t.entityelement[t.e].eleprof.soundset5_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 8) { t.entityelement[t.e].eleprof.soundset6_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
								if (t.tcheck == 9) { t.entityelement[t.e].eleprof.soundset4a_s = t.replacements_s[t.tt][1]; t.tt = t.treplacementmax + 1; }
							}
						}
					}
				}
				//  free usages
				UnDim (t.replacements_s);
			}
		}
	}

	// and erase any elements that DO NOT have a valid profile (file moved/deleted)
	if ( t.failedtoload == 1 ) 
	{
		//  FPGC - 270410 - if entity binary from X10 (or just not supported), ensure NO entities!
		g.entityelementlist=0;
		g.entityelementmax=0;
	}
	else
	{
		// clean up for grouplist corruption
		for (int gi = 0; gi < MAXGROUPSLISTS; gi++)
		{
			if (vEntityGroupList[gi].size() == 0)
			{
				sEntityGroupListName[gi] = "";
			}
		}

		// clean up for entityelement corruption
		if (g_iAddEntityElementsMode == 0)
		{
			for (t.e = 1; t.e <= g.entityelementlist; t.e++)
			{
				t.entid = t.entityelement[t.e].bankindex;
				if (t.entid > 0)
				{
					if (t.entid > ArrayCount(t.entitybank_s))
					{
						t.entityelement[t.e].bankindex = 0;
					}
					else
					{
						if (Len(t.entitybank_s[t.entid].Get()) == 0)
						{
							//  030715 - but only erase if entity not a marker
							if (t.entityprofile[t.entid].ismarker == 0)
							{
								t.entityelement[t.e].bankindex = 0;
							}
						}
					}
				}
			}
		}

		// could have group data corruption, storing entityelements that are not in the level
		int iNumberOfGroups = MAXGROUPSLISTS;
		for (int gi = 0; gi < iNumberOfGroups; gi++)
		{
			int iItemsInThisGroup = vEntityGroupList[gi].size();
			for (int i = 0; i < iItemsInThisGroup; i++)
			{
				int e = vEntityGroupList[gi][i].e;
				if (e > 0 && e <= g.entityelementlist)
				{
					if (t.entityelement[e].bankindex == 0)
					{
						vEntityGroupList[gi].erase(vEntityGroupList[gi].begin() + i);
						iItemsInThisGroup--;
						i--;
					}
				}
			}
		}

		// could have entities in more than one group, remove them from duplicates
		for (int gi = 0; gi < iNumberOfGroups; gi++)
		{
			int iItemsInThisGroup = vEntityGroupList[gi].size();
			for (int i = 0; i < iItemsInThisGroup; i++)
			{
				int e = vEntityGroupList[gi][i].e;
				if (e > 0 && e <= g.entityelementlist)
				{
					for (int gi2 = 0; gi2 < iNumberOfGroups; gi2++)
					{
						if (gi2 != gi)
						{
							for (int i2 = 0; i2 < vEntityGroupList[gi2].size(); i2++)
							{
								if (vEntityGroupList[gi2][i2].e == e)
								{
									vEntityGroupList[gi2].erase(vEntityGroupList[gi2].begin() + i2);
									i2--;
								}
							}
						}
					}
				}
			}
		}
	}
}
#endif

void entity_loadelementsdata(void)
{
	// fast loading of ELE file
	char pLoadEntityData[MAX_PATH];
	sprintf(pLoadEntityData, "c_entity_loadelementsdata: %s", t.elementsfilename_s.Get());
	timestampactivity(0, pLoadEntityData);
	char pLoadEntityDataBefore[MAX_PATH];
	sprintf(pLoadEntityDataBefore, "c_entity_loadelementsdata before: %d", g.entityelementmax);
	timestampactivity(0, pLoadEntityDataBefore);
	c_entity_loadelementsdata();

	extern bool bKeepWindowsResponding;
	void EmptyMessages(void);
	if (bKeepWindowsResponding)
		EmptyMessages();

	char pLoadEntityDataAfter[MAX_PATH];
	sprintf(pLoadEntityDataAfter, "c_entity_loadelementsdata after: %d", g.entityelementmax);
	timestampactivity(0, pLoadEntityDataAfter);
	
	//PE: Add any systemwidelua.ele to end of current elements
	extern StoryboardStruct Storyboard;
	if (strlen(Storyboard.gamename) > 0)
	{
		timestampactivity(0, "loading in systemwidelua.ele");
		cstr storeoldELEfile = t.elementsfilename_s;
		char collectionELEfilename[MAX_PATH];
		strcpy(collectionELEfilename, "projectbank\\");
		strcat(collectionELEfilename, Storyboard.gamename);
		strcat(collectionELEfilename, "\\systemwidelua.ele");
		t.elementsfilename_s = collectionELEfilename;
		extern int g_iAddEntityElementsMode;
		g_iAddEntityElementsMode = 2;
		c_entity_loadelementsdata();

		if (bKeepWindowsResponding)
			EmptyMessages();

		t.elementsfilename_s = storeoldELEfile;
		g_iAddEntityElementsMode = 0;

		//PE: Need to load masterobject if not there.
		for (int i = 1; i <= g.entityelementlist; i++)
		{
			if (t.entityelement[i].eleprof.systemwide_lua)
			{
				int tentid = t.entityelement[i].bankindex;
				if (tentid > 0)
				{
					if (tentid == 0 || tentid > t.entityprofile.size() || t.entityprofile[tentid].ismarker != 12)
					{
						//PE: Need to reload and remap.
						extern int g_iAddEntitiesModeFrom;
						g_iAddEntitiesModeFrom = g.entidmaster + 1;
						cstr entProfileToAdd_s = "_markers\\BehaviorHidden.fpe";

						int iFoundMatchEntID = 0;
						for (int entid = 1; entid <= g.entidmaster; entid++)
						{
							if (stricmp(t.entitybank_s[entid].Get(), entProfileToAdd_s.Get()) == NULL)
							{
								iFoundMatchEntID = entid;
								break;
							}
						}
						if (iFoundMatchEntID == 0)
						{
							g.entidmaster++;
							entity_validatearraysize();
							t.entitybank_s[g.entidmaster] = entProfileToAdd_s;
							iFoundMatchEntID = g.entidmaster;
							extern int g_iAddEntitiesMode;
							g_iAddEntitiesMode = 1;
							entity_loadentitiesnow();

							if (bKeepWindowsResponding)
								EmptyMessages();

							g_iAddEntitiesMode = 0;
						}
						t.entityelement[i].bankindex = iFoundMatchEntID;
					}
				}
			}
		}
	}
	sprintf(pLoadEntityDataAfter, "c_entity_loadelementsdata after systemwidelua.ele: %d", g.entityelementmax);
	timestampactivity(0, pLoadEntityDataAfter);

	// now entitybank and entityelement and vEntityGroupList are loaded, we need to refresh any smart objects (they may have changed externally, i.e BE)
	bool bForceSmartObjectRefresh = true;
	if (bForceSmartObjectRefresh == true)
	{
		for (int iGroupIndex = 0; iGroupIndex < MAXGROUPSLISTS; iGroupIndex++)
		{
			if (vEntityGroupList[iGroupIndex].size() > 0)
			{
				int iUniqueGroupID = vEntityGroupList[iGroupIndex][0].iGroupID;
				if (iUniqueGroupID > 0)
				{
					// load a fresh copy of this smart object (will ultimately call LoadGroup to populate with needed elements for below)
					int iFoundEntID = 0;
					for (int entIndex = 1; entIndex <= g.entidmaster; entIndex++)
					{
						cstr tmp = cstr("entitybank\\") + t.entitybank_s[entIndex];
						int iSmartObjectGroupIndex = GetGroupIndexFromName(tmp);
						if (iSmartObjectGroupIndex >= 0 && iSmartObjectGroupIndex < MAXGROUPSLISTS)
						{
							if (vEntityGroupList[iSmartObjectGroupIndex].size() > 0)
							{
								if (vEntityGroupList[iSmartObjectGroupIndex][0].iGroupID == iUniqueGroupID)
								{
									iFoundEntID = entIndex;
									break;
								}
							}
						}
					}

					if (bKeepWindowsResponding)
						EmptyMessages();

					extern void ReloadEntityIDInSitu(int);
					ReloadEntityIDInSitu (iFoundEntID);
				}
			}
		}
	}

	// after all entity profiles and elements in, can refresh collection list that references entities
	if(g_collectionLabels.size()>0)
	{
		// before
		char pLogCollectionCountBefore[MAX_PATH];
		sprintf(pLogCollectionCountBefore, "Collection list size before: %d", g_collectionList.size());
		timestampactivity(0, pLogCollectionCountBefore);
		bool bLoadingLevel = true;

		if (bKeepWindowsResponding)
			EmptyMessages();

		if (refresh_collection_from_entities(bLoadingLevel) == true)
		{
			// after
			char pLogCollectionCount[MAX_PATH];
			sprintf(pLogCollectionCount, "Collection list size after: %d", g_collectionList.size());
			timestampactivity(0, pLogCollectionCount);

			// refresh detected some entity profile/elements are missing
			// these will be needed for multi-level consistency and carrying items around the whole game
			timestampactivity(0, "Loading additional entities for collection item list");

			// build list of required entity profiles
			timestampactivity(0, "entity bank additions");
			std::vector<int> g_entityBankAdditionsCollectionIndex;
			std::vector<cstr> g_entityBankAdditions;
			for (int n = 0; n < g_collectionList.size(); n++)
			{
				if (g_collectionList[n].collectionFields.size() > 1)
				{
					LPSTR pCollectionItemTitle = g_collectionList[n].collectionFields[0].Get();
					LPSTR pCollectionItemProfile = g_collectionList[n].collectionFields[1].Get();
					if (strlen(pCollectionItemTitle) > 0)
					{
						if (g_collectionList[n].iEntityID == 0)
						{
							bool bFoundIt = false;
							if (stricmp(g_collectionList[n].collectionFields[1].Get(), "default") == NULL)
							{
								// try desc as a clue to finding it
								for (int entid = 1; entid <= g.entidmastermax; entid++)
								{
									if (stricmp (t.entityprofileheader[entid].desc_s.Get(), pCollectionItemTitle) == NULL)
									{
										g_collectionList[n].collectionFields[1] = t.entitybank_s[entid];
										g_collectionList[n].iEntityID = entid;
										bFoundIt = true;
										break;
									}
								}
								pCollectionItemProfile = "";
							}
							if(bFoundIt==false && stricmp(g_collectionList[n].collectionFields[1].Get(), "none") == NULL)
							{
								// None could be a weapon
								if (g_collectionList[n].collectionFields.size() > 8)
								{
									if (strnicmp(g_collectionList[n].collectionFields[8].Get(), "weapon=", 7) == NULL)
									{
										LPSTR pWeaponName = g_collectionList[n].collectionFields[8].Get() + 7;
										for (int entid = 1; entid <= g.entidmastermax; entid++)
										{
											if (stricmp (t.entityprofile[entid].isweapon_s.Get(), pWeaponName) == NULL)
											{
												g_collectionList[n].collectionFields[1] = t.entitybank_s[entid];
												g_collectionList[n].iEntityID = entid;
												bFoundIt = true;
												break;
											}
										}
										if (bFoundIt == false)
										{
											// okay, so no weapon of this type in the parent objects, need to point to the
											// entity using the gun collection database
											for (int gunid = 1; gunid <= g.gunmax; gunid++)
											{
												if (stricmp (t.gun[gunid].name_s.Get(), pWeaponName) == NULL)
												{
													if (t.gun[gunid].pathtostockentity_s.Len() > 0)
													{
														g_collectionList[n].collectionFields[1] = t.gun[gunid].pathtostockentity_s;
													}
												}
											}
										}
									}
								}
							}
							if(bFoundIt==false)
							{
								// do a direct search for it
								LPSTR pCollectionItemProfile = g_collectionList[n].collectionFields[1].Get();
								if (strlen(pCollectionItemProfile) > 0)
								{
									for (int entid = 1; entid <= g.entidmastermax; entid++)
									{
										if (stricmp (t.entitybank_s[entid].Get(), pCollectionItemProfile) == NULL)
										{
											g_collectionList[n].iEntityID = entid;
											bFoundIt = true;
											break;
										}
									}
								}
							}
							if (bFoundIt == false && strlen(pCollectionItemProfile) > 0)
							{
								//PE: "none" will end up as a ebe missing file.
								if( strlen(pCollectionItemProfile) > 1 && !(stricmp(pCollectionItemProfile, "none") == NULL) )
								{
									// add entity to additions
									g_entityBankAdditions.push_back(pCollectionItemProfile);
									g_entityBankAdditionsCollectionIndex.push_back(n);

									// and log it
									char pLogCollectionAdded[MAX_PATH];
									sprintf(pLogCollectionAdded, "entity Bank Addition: %s", pCollectionItemProfile);
									timestampactivity(0, pLogCollectionAdded);
								}
							}
						}
					}
				}
			}

			// merge load entity profiles
			if (g_entityBankAdditions.size() > 0)
			{
				extern int g_iAddEntitiesModeFrom;
				g_iAddEntitiesModeFrom = g.entidmaster + 1;
				for (int i = 0; i < g_entityBankAdditions.size(); i++)
				{
					// Look for this
					cstr entProfileToAdd_s = g_entityBankAdditions[i];

					// add if not exist in bank
					int iFoundMatchEntID = 0;
					for (int entid = 1; entid <= g.entidmaster - 1; entid++)
					{
						if (stricmp(t.entitybank_s[entid].Get(), entProfileToAdd_s.Get()) == NULL)
						{
							iFoundMatchEntID = entid;
							break;
						}
					}
					if (iFoundMatchEntID == 0)
					{
						g.entidmaster++;
						entity_validatearraysize ();
						t.entitybank_s[g.entidmaster] = entProfileToAdd_s;
						iFoundMatchEntID = g.entidmaster;
					}
					g_collectionList[g_entityBankAdditionsCollectionIndex[i]].iEntityID = iFoundMatchEntID;
				}
				extern int g_iAddEntitiesMode;
				g_iAddEntitiesMode = 1;
				entity_loadentitiesnow();
				if (bKeepWindowsResponding)
					EmptyMessages();
				g_iAddEntitiesMode = 0;
			}

			// load in game project elements file to end of current elements
			timestampactivity(0, "loading in collection - items.ele");
			cstr storeoldELEfile = t.elementsfilename_s;
			char collectionELEfilename[MAX_PATH];
			strcpy(collectionELEfilename, "projectbank\\");
			extern StoryboardStruct Storyboard;
			strcat(collectionELEfilename, Storyboard.gamename);
			strcat(collectionELEfilename, "\\collection - items.ele");
			t.elementsfilename_s = collectionELEfilename;
			extern int g_iAddEntityElementsMode;
			g_iAddEntityElementsMode = 1;
			c_entity_loadelementsdata();

			if (bKeepWindowsResponding)
				EmptyMessages();

			t.elementsfilename_s = storeoldELEfile;
			g_iAddEntityElementsMode = 0;

			// associate new entity elements with collection entry
			for (int e = 1; e <= g.entityelementlist; e++)
			{
				if (t.entityelement[e].specialentityloadflag == 123)
				{
					// if already exists, do NOT add, just wipe out
					t.entityelement[e].specialentityloadflag = 0;
					bool bAlreadyExistsSoSkip = false;
					for (int eee = 1; eee <= g.entityelementmax; eee++)
					{
						if (eee != e)
						{
							if (t.entityelement[eee].bankindex > 0)
							{
								LPSTR pEntityElementName1 = t.entityelement[e].eleprof.name_s.Get();
								LPSTR pEntityElementName2 = t.entityelement[eee].eleprof.name_s.Get();
								if (stricmp(pEntityElementName1, pEntityElementName2) == NULL)
								{
									t.entityelement[e].maintype = 0;
									t.entityelement[e].bankindex = 0;
									bAlreadyExistsSoSkip = true;
									break;
								}
							}
						}
					}
					int iCollectionIndexFound = -1;
					if (bAlreadyExistsSoSkip == false)
					{
						LPSTR pEntityElementName = t.entityelement[e].eleprof.name_s.Get();
						for (int n = 0; n < g_collectionList.size(); n++)
						{
							if (g_collectionList[n].collectionFields.size() > 1)
							{
								LPSTR pCollectionItemTitle = g_collectionList[n].collectionFields[0].Get();
								if (stricmp(pCollectionItemTitle, pEntityElementName) == NULL)
								{
									// the newly loaded element will be used for this collection item
									g_collectionList[n].iEntityElementE = e;

									// now hide it away in the level
									t.entityelement[e].bankindex = g_collectionList[n].iEntityID;
									t.entityelement[e].x = -99999;
									t.entityelement[e].y = -99999;
									t.entityelement[e].z = -99999;
									t.entityelement[e].eleprof.spawnatstart = 1;

									// found, so can quit
									iCollectionIndexFound = n;
									break;
								}
							}
						}
					}

					// now scan all entities in common with this collection item entity just loaded from the ELE file
					// and clone all details to them (there should only be one collectale entity element/eleprof identity)
					int iDeletingThisElementSoUseFoundE = 0;
					bool bDeleteThisNewElementNotNeeded = false;
					LPSTR pMasterEntityName = t.entityelement[e].eleprof.name_s.Get();
					for (int ee = 1; ee <= g.entityelementmax; ee++)
					{
						if (ee != e)
						{
							int masterid = t.entityelement[ee].bankindex;
							if (masterid > 0)
							{
								if (stricmp (t.entityelement[ee].eleprof.name_s.Get(), pMasterEntityName) == NULL)
								{
									t.entityelement[ee].eleprof = t.entityelement[e].eleprof;
									iDeletingThisElementSoUseFoundE = ee;
									bDeleteThisNewElementNotNeeded = true;
								}
							}
						}
					}
					if (bDeleteThisNewElementNotNeeded == true)
					{
						// delete element, not needed for this level
						t.entityelement[e].bankindex = 0;

						// and reassign collection item E to the existing one
						if (iCollectionIndexFound != -1)
						{
							g_collectionList[iCollectionIndexFound].iEntityElementE = iDeletingThisElementSoUseFoundE;
						}
					}
				}
			}

			// and now if any 'iEntityElementE' are zero, we need to create an element off-level 
			// as an instance that can be cloned down the road
			bool bSaveUpdatedELEFile = false;
			for (int n = 0; n < g_collectionList.size(); n++)
			{
				if (g_collectionList[n].iEntityID > 0 && g_collectionList[n].iEntityElementE == 0)
				{
					// before make an instance, check if it already exists as a valid entity
					int entid = g_collectionList[n].iEntityID;
					for (int ee = 1; ee <= g.entityelementmax; ee++)
					{
						int masterid = t.entityelement[ee].bankindex;
						if (masterid > 0)
						{
							if (masterid == entid)
							{
								g_collectionList[n].iEntityElementE = ee;
								break;
							}
						}
					}
					if (g_collectionList[n].iEntityElementE == 0)
					{
						// make a hidden instance of this entity
						t.gridentity = entid;
						t.gridentityeditorfixed = 0;
						t.entitymaintype = 1;
						t.entitybankindex = t.entid;
						t.gridentitystaticmode = 0;
						t.gridentityhasparent = 0;
						t.gridentityposx_f = 0;
						t.gridentityposz_f = 0;
						t.gridentityposy_f = 0;
						t.gridentityrotatex_f = 0;
						t.gridentityrotatey_f = 0;
						t.gridentityrotatez_f = 0;
						t.gridentityrotatequatmode = 1;
						t.gridentityrotatequatx_f = 0;
						t.gridentityrotatequaty_f = 0;
						t.gridentityrotatequatz_f = 0;
						t.gridentityrotatequatw_f = 1;
						t.gridentityscalex_f = 100;
						t.gridentityscaley_f = 100;
						t.gridentityscalez_f = 100;
						t.entid = entid; entity_fillgrideleproffromprofile();
						entity_addentitytomap ();

						if (bKeepWindowsResponding)
							EmptyMessages();

						t.e = t.tupdatee;
						t.entityelement[t.e].x = -99999;
						t.entityelement[t.e].y = -99999;
						t.entityelement[t.e].z = -99999;
						t.entityelement[t.e].eleprof.spawnatstart = 0;
						t.entityelement[t.e].lua.firsttime = 0;
						t.entityelement[t.e].active = 0;
						g_collectionList[n].iEntityElementE = t.e;
						t.gridentity = 0;
					}
					bSaveUpdatedELEFile = true;
				}
			}
			// also if iEntityID is zero at this point, find it and update
			for (int n = 0; n < g_collectionList.size(); n++)
			{
				if (g_collectionList[n].iEntityID == 0)
				{
					if (g_collectionList[n].collectionFields.size() > 1)
					{
						LPSTR pCollectionItemTitle = g_collectionList[n].collectionFields[0].Get();
						for (int eee = 1; eee <= g.entityelementmax; eee++)
						{
							if (t.entityelement[eee].bankindex > 0)
							{
								LPSTR pEntityElementName = t.entityelement[eee].eleprof.name_s.Get();
								if (stricmp(pCollectionItemTitle, pEntityElementName) == NULL)
								{
									// the newly loaded element will be used for this collection item
									g_collectionList[n].iEntityElementE = eee;
									bSaveUpdatedELEFile = true;
									break;
								}
							}
						}
					}
				}
			}
			if (bSaveUpdatedELEFile == true)
			{
				// ensure collection list and ELE file up to date
				extern preferences pref;
				save_rpg_system(pref.cLastUsedStoryboardProject, true);
				if (bKeepWindowsResponding)
					EmptyMessages();

			}

			// also to fix older saves, remove any off-level duplicates
			for (int e = 1; e <= g.entityelementlist; e++)
			{
				if (t.entityelement[e].bankindex > 0)
				{
					bool bFoundDuplicate = false;
					for (int eee = 1; eee <= g.entityelementmax; eee++)
					{
						if (eee != e)
						{
							if (t.entityelement[e].bankindex > 0)
							{
								if (t.entityelement[e].bankindex == t.entityelement[eee].bankindex)
								{
									if (t.entityelement[e].x == -99999 && t.entityelement[e].y == -99999 && t.entityelement[e].z == -99999)
									{
										if (t.entityelement[eee].x == -99999 && t.entityelement[eee].y == -99999 && t.entityelement[eee].z == -99999)
										{
											bFoundDuplicate = true;
											int iPreferredElementE = 0;
											for (int n = 0; n < g_collectionList.size(); n++)
											{
												if (g_collectionList[n].iEntityID == t.entityelement[e].bankindex)
												{
													if (g_collectionList[n].iEntityElementE > 0)
													{
														iPreferredElementE = g_collectionList[n].iEntityElementE;
														break;
													}
												}
											}
											if (iPreferredElementE > 0)
											{
												if (eee != iPreferredElementE)
												{
													// if not preferred element, remove duplicate from element list
													t.entityelement[eee].maintype = 0;
													t.entityelement[eee].bankindex = 0;
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

			// and a full pass to convert any parent objects into collectables if the collection list has them
			refresh_rpg_parents_of_items();
			if (bKeepWindowsResponding)
				EmptyMessages();

		}
	}

	// so it seems vEntityGroupList can be saved in elements data, but reference entities that no longer exist
	// suggesting too a deeper issue relating to group data becoming corrupt, but cannot allow group datra to proceed
	// that points to entities that do not exist, so must delete those rogue groups
	bool bCleanUpRedundantGroupData = true;
	if (bCleanUpRedundantGroupData == true)
	{
		for (int iGroupIndex = 0; iGroupIndex < MAXGROUPSLISTS; iGroupIndex++)
		{
			if (vEntityGroupList[iGroupIndex].size() > 0)
			{
				for (int i = 0; i < vEntityGroupList[iGroupIndex].size(); i++)
				{
					int e = vEntityGroupList[iGroupIndex][i].e;
					if (e == 0 || e >= t.entityelement.size())
					{
						// this entity is zero or does not exist in entity element data
						// so remove entire group as redundant/corrupt
						vEntityGroupList[iGroupIndex].clear();
						break;
					}
				}
			}
		}
	}
}

// class to write in two passes, first adds up total size, second creates and fills the data buffer
class EntityWriter
{
protected:

	unsigned char* pData = 0;
	unsigned int iDataSize = 0;
	unsigned int iMaxDataSize = 0;
	unsigned int doWrite = 0; // first pass = 0, second pass = 1

public:

	EntityWriter() {}
	~EntityWriter() { if ( pData ) delete [] pData; }

	unsigned char* GetData() { return pData; }
	unsigned int GetDataSize() { return iDataSize; }

	// call between passes
	void AllocateDataForWrite()
	{
		assert( !pData ); 
		if ( pData ) delete [] pData; // should't be called more than once, but prevent memory leak if it is
		pData = new unsigned char[ iDataSize ];
		iMaxDataSize = iDataSize;
		iDataSize = 0;
		doWrite = 1;
	}

	void WriteLong( int num ) 
	{
		const unsigned int elementSize = sizeof(int);
		if ( !doWrite ) iDataSize += elementSize;
		else
		{
			assert( (iDataSize + elementSize) <= iMaxDataSize );
			memcpy( pData + iDataSize, &num, elementSize );
			iDataSize += elementSize;
		}
	}
	
	void WriteFloat( float num ) 
	{
		const unsigned int elementSize = sizeof(float);
		if ( !doWrite ) iDataSize += elementSize;
		else
		{
			assert( (iDataSize + elementSize) <= iMaxDataSize );
			memcpy( pData + iDataSize, &num, elementSize );
			iDataSize += elementSize;
		}
	}

	void WriteString( char* str ) 
	{
		for (int i = 0; i < strlen(str); i++)
		{
			//PE: Make sure we dont break the .ele file , seen some corrupt strings with \n\r.
			if (*(str + i) == '\n' || *(str + i) == '\r')
			{
				*(str + i) = ' ';
			}
		}
		unsigned int elementSize = strlen(str);
		if ( !doWrite ) iDataSize += elementSize + 2;
		else
		{
			if ( elementSize )
			{
				assert( (iDataSize + elementSize) <= iMaxDataSize );
				memcpy( pData + iDataSize, str, elementSize );
				iDataSize += elementSize;
			}

			pData[ iDataSize ] = 13;
			pData[ iDataSize + 1 ] = 10;
			iDataSize += 2;
		}
	}
	void WriteStringInclude0xa(char* str)
	{
		for (int i = 0; i < strlen(str); i++)
		{
			//PE: Make sure we dont break the .ele file , seen some corrupt strings with \n\r.
			if ( *(str + i) == '\r')
			{
				*(str + i) = ' ';
			}
		}
		unsigned int elementSize = strlen(str);
		if (!doWrite) iDataSize += elementSize + 2;
		else
		{
			if (elementSize)
			{
				assert((iDataSize + elementSize) <= iMaxDataSize);
				memcpy(pData + iDataSize, str, elementSize);
				iDataSize += elementSize;
			}

			pData[iDataSize] = 13;
			pData[iDataSize + 1] = 10;
			iDataSize += 2;
		}
	}

};

