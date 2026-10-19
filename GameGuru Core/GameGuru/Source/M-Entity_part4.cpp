void entity_saveelementsdata (bool bForCollectionELE)
{
	//  Uses elementfilename$
	if ( t.elementsfilename_s == ""  )  t.elementsfilename_s = g.mysystem.levelBankTestMap_s+"map.ele";

	//  Reduce list size if later elements blank
	int temp = g.entityelementlist;
	while ( temp > 0 ) 
	{
		if ( t.entityelement[temp].maintype == 0  )  --temp; else break;
	}
	g.entityelementlist = temp;

	//  Save entity element list
	t.versionnumbersave = 342; // v342 adds eleprof.soundset4a_s — matches production DX11 so saves round-trip both ways

	EntityWriter writer;

	// write in two passes, first adds up total data size, second pass allocates and writes to the buffer
	for( int pass = 0; pass < 2; pass++ )
	{
		writer.WriteLong ( t.versionnumbersave );
		writer.WriteLong ( g.entityelementlist );
		if ( g.entityelementlist>0 ) 
		{
			for ( int ent = 1 ; ent<=  g.entityelementlist; ent++ )
			{
				if ( t.versionnumbersave >= 101 ) 
				{
					//  Version 1.01 - EA
					writer.WriteLong( t.entityelement[ent].maintype );
					writer.WriteLong( t.entityelement[ent].bankindex );
					writer.WriteLong( t.entityelement[ent].staticflag );
					writer.WriteFloat( t.entityelement[ent].x );
					writer.WriteFloat( t.entityelement[ent].y );
					writer.WriteFloat( t.entityelement[ent].z );
					writer.WriteFloat( t.entityelement[ent].rx );
					writer.WriteFloat( t.entityelement[ent].ry );
					writer.WriteFloat( t.entityelement[ent].rz );
					writer.WriteString( t.entityelement[ent].eleprof.name_s.Get() );
					writer.WriteString ( "" );
					writer.WriteString( t.entityelement[ent].eleprof.aimain_s.Get() );
					writer.WriteString ( "" );
					writer.WriteLong( t.entityelement[ent].eleprof.isobjective );
					writer.WriteString( t.entityelement[ent].eleprof.usekey_s.Get() );
					writer.WriteString( t.entityelement[ent].eleprof.ifused_s.Get() );
					writer.WriteString ( "" );
					writer.WriteLong( t.entityelement[ent].eleprof.uniqueelement );
					writer.WriteString( t.entityelement[ent].eleprof.texd_s.Get() );
					writer.WriteString( t.entityelement[ent].eleprof.texaltd_s.Get() );
					writer.WriteString( t.entityelement[ent].eleprof.effect_s.Get() );
					writer.WriteLong( t.entityelement[ent].eleprof.transparency );
					writer.WriteLong( t.entityelement[ent].editorfixed );
					writer.WriteString( t.entityelement[ent].eleprof.soundset_s.Get() );
					writer.WriteString( t.entityelement[ent].eleprof.soundset1_s.Get() );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnmax );
					writer.WriteLong( t.entityelement[ent].eleprof.spawndelay );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnqty );
					writer.WriteLong( t.entityelement[ent].eleprof.hurtfall );
					writer.WriteLong( t.entityelement[ent].eleprof.castshadow );
					writer.WriteLong( t.entityelement[ent].eleprof.reducetexture );
					writer.WriteLong( t.entityelement[ent].eleprof.speed );
					writer.WriteString ( "" );
					writer.WriteString( t.entityelement[ent].eleprof.hasweapon_s.Get() );
					writer.WriteLong( t.entityelement[ent].eleprof.lives );
					writer.WriteLong( t.entityelement[ent].spawn.max );
					writer.WriteLong( t.entityelement[ent].spawn.delay );
					writer.WriteLong( t.entityelement[ent].spawn.qty );
					writer.WriteFloat( t.entityelement[ent].eleprof.scale );
					writer.WriteFloat( t.entityelement[ent].eleprof.coneheight );
					writer.WriteFloat( t.entityelement[ent].eleprof.coneangle );
					writer.WriteLong( t.entityelement[ent].eleprof.strength );
					writer.WriteLong( t.entityelement[ent].eleprof.isimmobile );
					writer.WriteLong( t.entityelement[ent].eleprof.cantakeweapon );
					writer.WriteLong( t.entityelement[ent].eleprof.quantity );
					writer.WriteLong( t.entityelement[ent].eleprof.markerindex );
					writer.WriteLong( t.entityelement[ent].eleprof.light.color & 0x00FFFFFF );
					writer.WriteLong( t.entityelement[ent].eleprof.light.range );
					writer.WriteLong( t.entityelement[ent].eleprof.trigger.stylecolor );
					writer.WriteLong( t.entityelement[ent].eleprof.trigger.waypointzoneindex );
					writer.WriteString ( "" );
				}
				if ( t.versionnumbersave >= 102 ) 
				{
					writer.WriteLong( t.entityelement[ent].eleprof.rateoffire );
					writer.WriteLong( t.entityelement[ent].eleprof.damage );
					writer.WriteLong( t.entityelement[ent].eleprof.accuracy );
					writer.WriteLong( t.entityelement[ent].eleprof.reloadqty );
					writer.WriteLong( t.entityelement[ent].eleprof.fireiterations );
					writer.WriteLong( t.entityelement[ent].eleprof.lifespan );
					writer.WriteFloat( t.entityelement[ent].eleprof.throwspeed );
					writer.WriteFloat( t.entityelement[ent].eleprof.throwangle );
					writer.WriteLong( t.entityelement[ent].eleprof.bounceqty );
					writer.WriteLong( t.entityelement[ent].eleprof.explodeonhit );
					writer.WriteLong( t.entityelement[ent].eleprof.weaponisammo );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnupto );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnafterdelay );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnwhendead );
					writer.WriteLong( t.entityelement[ent].eleprof.perentityflags );
					writer.WriteLong( t.entityelement[ent].eleprof.perentityflags );
					writer.WriteLong( t.entityelement[ent].eleprof.perentityflags );
					writer.WriteLong( t.entityelement[ent].eleprof.perentityflags );
					writer.WriteLong( t.entityelement[ent].eleprof.perentityflags );
					writer.WriteLong( t.entityelement[ent].eleprof.perentityflags );
				}
				if ( t.versionnumbersave >= 103 ) 
				{
					//  V1 first draft - physics
					writer.WriteLong( t.entityelement[ent].eleprof.physics );
					writer.WriteLong( t.entityelement[ent].eleprof.phyweight );
					writer.WriteLong( t.entityelement[ent].eleprof.phyfriction );
					writer.WriteLong( t.entityelement[ent].eleprof.phyforcedamage );
					writer.WriteLong( t.entityelement[ent].eleprof.rotatethrow );
					writer.WriteLong( t.entityelement[ent].eleprof.explodable );
					writer.WriteLong( t.entityelement[ent].eleprof.explodedamage );
					writer.WriteLong( 0 );
					writer.WriteLong( 0 );
				}
				if ( t.versionnumbersave >= 104 ) 
				{
					//  Addition of new physics field for BETA4
					writer.WriteLong( t.entityelement[ent].eleprof.phyalways );
				}
				if ( t.versionnumbersave >= 105 ) 
				{
					//  Addition of new spawn fields for BETA8
					writer.WriteLong( t.entityelement[ent].eleprof.spawndelayrandom );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnqtyrandom );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnvel );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnvelrandom );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnangle );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnanglerandom );
				}
				if ( t.versionnumbersave >= 106 ) 
				{
					//  Addition of new fields for BETA10
					writer.WriteLong( t.entityelement[ent].eleprof.spawnatstart );
					writer.WriteLong( t.entityelement[ent].eleprof.spawnlife );
				}
				if ( t.versionnumbersave >= 107 ) 
				{
					//  FPSCV104RC8 - forgot to save infinilight index (dynamic lights in final build never worked)
					writer.WriteLong( t.entityelement[ent].eleprof.light.index );
				}
				if ( t.versionnumbersave >= 199 ) 
				{
					//  X10 specific version - any new save data must be higher than 200
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
				}
				if ( t.versionnumbersave >= 200 ) 
				{
					//  X10 specific version - any new save data must be higher than 200
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
					writer.WriteLong ( 0 );
				}
				if ( t.versionnumbersave >= 217 ) 
				{
					//  FPGC - 300710 - save new entity element data
					writer.WriteLong( t.entityelement[ent].eleprof.particleoverride );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.offsety );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.scale );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.randomstartx );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.randomstarty );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.randomstartz );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.linearmotionx );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.linearmotiony );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.linearmotionz );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.randommotionx );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.randommotiony );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.randommotionz );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.mirrormode );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.camerazshift );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.scaleonlyx );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.lifeincrement );
					writer.WriteLong( t.entityelement[ent].eleprof.particle.alphaintensity );
				}
				if ( t.versionnumbersave >= 218 ) 
				{
					//  V118 - 060810 - knxrb - Decal animation setting (Added animation choice setting).
					writer.WriteLong( t.entityelement[ent].eleprof.particle.animated );
				}
				if ( t.versionnumbersave >= 301 ) 
				{
					//  Reloaded ALPHA 1.0045
					writer.WriteString ( "" );
					writer.WriteString( t.entityelement[ent].eleprof.aimainname_s.Get() );
					writer.WriteString ( "" );
					writer.WriteString ( "" );
				}
				if ( t.versionnumbersave >= 302 ) 
				{
					//  Reloaded BETA 1.005
				}
				if ( t.versionnumbersave >= 303 ) 
				{
					//  Reloaded BETA 1.007
					writer.WriteLong( t.entityelement[ent].eleprof.animspeed );
				}
				if ( t.versionnumbersave >= 304 ) 
				{
					//  Reloaded BETA 1.007-200514
					writer.WriteFloat( t.entityelement[ent].eleprof.conerange );
				}
				if ( t.versionnumbersave >= 305 ) 
				{
					//  Reloaded BETA 1.009
					writer.WriteFloat( t.entityelement[ent].scalex );
					writer.WriteFloat( t.entityelement[ent].scaley );
					writer.WriteFloat( t.entityelement[ent].scalez );
					writer.WriteLong( t.entityelement[ent].eleprof.range );
					writer.WriteLong( t.entityelement[ent].eleprof.dropoff );
				}
				if ( t.versionnumbersave >= 306 ) 
				{
					//  Guru 1.00.010
					writer.WriteLong( t.entityelement[ent].eleprof.isviolent );
				}
				if ( t.versionnumbersave >= 307 ) 
				{
					//  Guru 1.00.020
					writer.WriteLong( t.entityelement[ent].eleprof.explodeheight);
				}
				if ( t.versionnumbersave >= 308 ) 
				{
					//  Guru 1.01.001
					writer.WriteLong( t.entityelement[ent].eleprof.usespotlighting );
				}
				if ( t.versionnumbersave >= 309 )
				{
					//  Guru 1.01.002
					writer.WriteLong( t.entityelement[ent].eleprof.lodmodifier );
				}
				if ( t.versionnumbersave >= 310 )
				{
					//  Guru 1.133
					writer.WriteLong( t.entityelement[ent].eleprof.isocluder );
					writer.WriteLong( t.entityelement[ent].eleprof.isocludee );
					writer.WriteLong( t.entityelement[ent].eleprof.colondeath );
					writer.WriteLong( t.entityelement[ent].eleprof.parententityindex );
					writer.WriteLong( t.entityelement[ent].eleprof.parentlimbindex );
					writer.WriteString( t.entityelement[ent].eleprof.soundset2_s.Get() );
					writer.WriteString( t.entityelement[ent].eleprof.soundset3_s.Get() );
					writer.WriteStringInclude0xa( t.entityelement[ent].eleprof.soundset4_s.Get() );
				}
				if ( t.versionnumbersave >= 311 )
				{
					//  Guru 1.133B
					writer.WriteFloat( t.entityelement[ent].eleprof.lootpercentage);
				}
				if ( t.versionnumbersave >= 312 )
				{
					//  Guru 1.14 EBE
					writer.WriteLong( t.entityelement[ent].iHasParentIndex );
				}			
				if ( t.versionnumbersave >= 313 )
				{
					// VRQ V3
					writer.WriteString( t.entityelement[ent].eleprof.voiceset_s.Get() );
					writer.WriteLong( t.entityelement[ent].eleprof.voicerate );
				}
				if (t.versionnumbersave >= 314)
				{
					//PE: We must save custom "Materials" here.

					writer.WriteLong( t.entityelement[ent].eleprof.bCustomWickedMaterialActive );
					writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.MaterialActive );
					writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bCastShadows[0] );
					writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bDoubleSided[0] );
					writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bPlanerReflection[0] );
					writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bTransparency[0] );

					char tas[256];
					sprintf(tas, "%lu", (unsigned long) t.entityelement[ent].eleprof.WEMaterial.dwBaseColor[0] );
					writer.WriteString( tas );
					sprintf(tas, "%lu", (unsigned long)t.entityelement[ent].eleprof.WEMaterial.dwEmmisiveColor[0] );
					writer.WriteString( tas );

					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fReflectance[0] );

					// header 9*4 = 36 bytes for each entry.
					// per mesh 11 * 4 = 44 bytes for a empty material.
					// LB: in previous builds, only one mesh material details where stored, we can retain this for single material objects in slot 0
					writer.WriteLong( 0 );
					int iFrameAndWEMaterialSlotIndex = 0;
					writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.baseColorMapName[iFrameAndWEMaterialSlotIndex].Get() );
					writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.normalMapName[iFrameAndWEMaterialSlotIndex].Get() );
					writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.surfaceMapName[iFrameAndWEMaterialSlotIndex].Get() );
					writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.displacementMapName[iFrameAndWEMaterialSlotIndex].Get() );
					writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.emissiveMapName[iFrameAndWEMaterialSlotIndex].Get() );
					#ifndef DISABLEOCCLUSIONMAP
					writer.WriteString ( t.entityelement[ent].eleprof.WEMaterial.occlusionMapName[iFrameAndWEMaterialSlotIndex].Get() );
					#else
					writer.WriteString ( "" );
					#endif
					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fNormal[iFrameAndWEMaterialSlotIndex] );
					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fRoughness[iFrameAndWEMaterialSlotIndex] );
					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fMetallness[iFrameAndWEMaterialSlotIndex] );
					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fEmissive[iFrameAndWEMaterialSlotIndex] );
					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fAlphaRef[iFrameAndWEMaterialSlotIndex] );
				}
				if (t.versionnumbersave >= 315)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.light.fLightHasProbe );
				}
				if (t.versionnumbersave >= 316)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.iObjectLinkID );
					writer.WriteLong( t.entityelement[ent].eleprof.iCharAlliance );
					writer.WriteLong( t.entityelement[ent].eleprof.iCharFaction );
					writer.WriteLong( t.entityelement[ent].eleprof.iObjectReserved1 );
					writer.WriteLong( t.entityelement[ent].eleprof.iObjectReserved2 );
					writer.WriteLong( t.entityelement[ent].eleprof.iObjectReserved3 );
					writer.WriteLong( t.entityelement[ent].eleprof.iCharPatrolMode );
					writer.WriteFloat( t.entityelement[ent].eleprof.fCharRange[0] );
					writer.WriteFloat( t.entityelement[ent].eleprof.fCharRange[1] );
					for (int i = 0; i < 10; i++)
					{
						writer.WriteFloat( t.entityelement[ent].eleprof.fObjectDataReserved[i] );
						writer.WriteLong( t.entityelement[ent].eleprof.iObjectRelationships[i] );
						writer.WriteLong( t.entityelement[ent].eleprof.iObjectRelationshipsType[i] );
						writer.WriteLong( t.entityelement[ent].eleprof.iObjectRelationshipsData[i] );
					}
				}
				if (t.versionnumbersave >= 317)
				{
					//PE: Save per mesh material settings.
					for (int i = 1; i < MAXMESHMATERIALS; i++)
					{
						writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bCastShadows[i] );
						writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bDoubleSided[i] );
						writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bPlanerReflection[i] );
						writer.WriteLong( t.entityelement[ent].eleprof.WEMaterial.bTransparency[i] );

						char tas[256];
						sprintf(tas, "%lu", (unsigned long)t.entityelement[ent].eleprof.WEMaterial.dwBaseColor[i] );
						writer.WriteString( tas );
						sprintf(tas, "%lu", (unsigned long)t.entityelement[ent].eleprof.WEMaterial.dwEmmisiveColor[i] );
						writer.WriteString( tas );

						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fReflectance[i] );

						int iFrameAndWEMaterialSlotIndex = i;
						writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.baseColorMapName[iFrameAndWEMaterialSlotIndex].Get() );
						writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.normalMapName[iFrameAndWEMaterialSlotIndex].Get() );
						writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.surfaceMapName[iFrameAndWEMaterialSlotIndex].Get() );
						writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.displacementMapName[iFrameAndWEMaterialSlotIndex].Get() );
						writer.WriteString( t.entityelement[ent].eleprof.WEMaterial.emissiveMapName[iFrameAndWEMaterialSlotIndex].Get() );
						#ifndef DISABLEOCCLUSIONMAP
						writer.WriteString ( t.entityelement[ent].eleprof.WEMaterial.occlusionMapName[iFrameAndWEMaterialSlotIndex].Get() );
						#else
						writer.WriteString ( "" );
						#endif
						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fNormal[iFrameAndWEMaterialSlotIndex] );
						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fRoughness[iFrameAndWEMaterialSlotIndex] );
						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fMetallness[iFrameAndWEMaterialSlotIndex] );
						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fEmissive[iFrameAndWEMaterialSlotIndex] );
						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fAlphaRef[iFrameAndWEMaterialSlotIndex] );
					}
				}
				if (t.versionnumbersave >= 318)
				{
					writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fRenderOrderBias[0] );
					for (int i = 1; i < MAXMESHMATERIALS; i++)
					{
						writer.WriteFloat( t.entityelement[ent].eleprof.WEMaterial.fRenderOrderBias[i] );
					}
				}
				if (t.versionnumbersave >= 319)
				{
					//PE: No relation between ent and all groups information, so only store it under ent = 1
					extern int g_iUniqueGroupID;
					if (ent > 1)
					{
						writer.WriteLong( 0 );
						writer.WriteLong( 0 ); //PE: zero groups on other ent entrys.
					}
					else
					{
						writer.WriteLong( g_iUniqueGroupID );
						int iNumberOfGroups = MAXGROUPSLISTS;
						writer.WriteLong( iNumberOfGroups );
						for (int gi = 0; gi < iNumberOfGroups; gi++)
						{
							int iItemsInThisGroup = vEntityGroupList[gi].size( );
							writer.WriteLong( iItemsInThisGroup );
							for (int i = 0; i < iItemsInThisGroup; i++)
							{
								writer.WriteLong( vEntityGroupList[gi][i].iGroupID );
								writer.WriteLong( vEntityGroupList[gi][i].iParentGroupID );
								writer.WriteLong( vEntityGroupList[gi][i].e );
								writer.WriteFloat( vEntityGroupList[gi][i].x );
								writer.WriteFloat( vEntityGroupList[gi][i].y );
								writer.WriteFloat( vEntityGroupList[gi][i].z );
								writer.WriteFloat( vEntityGroupList[gi][i].quatAngle.x );
								writer.WriteFloat( vEntityGroupList[gi][i].quatAngle.y );
								writer.WriteFloat( vEntityGroupList[gi][i].quatAngle.z );
								writer.WriteFloat( vEntityGroupList[gi][i].quatAngle.w );
							}
						}
						// and save out group thumb images, as need these to identify parent groups
						extern int iEntityGroupListImage[MAXGROUPSLISTS];
						for (int gi = 0; gi < iNumberOfGroups; gi++)
						{
							int iImgIndex = iEntityGroupListImage[gi];
							if (bForCollectionELE == false)
							{
								char pGroupImgFilename[MAX_PATH];
								sprintf(pGroupImgFilename, "%sgroupimg%d.png", g.mysystem.levelBankTestMap_s.Get(), gi);
								if (FileExist(pGroupImgFilename) == 1) DeleteAFile(pGroupImgFilename);
								if (iEntityGroupListImage[gi] > 0)
								{
									if (ImageExist(iImgIndex) == 1)
									{
										// img value not important, only as reference to image file creating now for the loader
										SaveImage(pGroupImgFilename, iImgIndex);
									}
								}
							}
							writer.WriteLong(iImgIndex > 0 ? 1 : 0);
						}
					}
				}

				if (t.versionnumbersave >= 320)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.newparticle.bParticle_Preview );
					writer.WriteLong( t.entityelement[ent].eleprof.newparticle.bParticle_Show_At_Start );
					writer.WriteLong( t.entityelement[ent].eleprof.newparticle.bParticle_Looping_Animation );
					writer.WriteLong( t.entityelement[ent].eleprof.newparticle.bParticle_Full_Screen );
					writer.WriteFloat( t.entityelement[ent].eleprof.newparticle.fParticle_Fullscreen_Duration );
					writer.WriteFloat( t.entityelement[ent].eleprof.newparticle.fParticle_Fullscreen_Fadein );
					writer.WriteFloat( t.entityelement[ent].eleprof.newparticle.fParticle_Fullscreen_Fadeout );
					writer.WriteString( t.entityelement[ent].eleprof.newparticle.Particle_Fullscreen_Transition.Get() );
					writer.WriteFloat( t.entityelement[ent].eleprof.newparticle.fParticle_Speed );
					writer.WriteFloat( t.entityelement[ent].eleprof.newparticle.fParticle_Opacity );

				}
				if (t.versionnumbersave >= 321)
				{
					writer.WriteString( t.entityelement[ent].eleprof.newparticle.emittername.Get() );
				}

				if (t.versionnumbersave >= 322)
				{
					writer.WriteFloat( t.entityelement[ent].fDecalSpeed );
					writer.WriteFloat( t.entityelement[ent].fDecalOpacity );
				}
				if (t.versionnumbersave >= 323)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.iOverrideCollisionMode );
				}
				if (t.versionnumbersave >= 324)
				{
					writer.WriteFloat( t.entityelement[ent].eleprof.weapondamagemultiplier );
					writer.WriteFloat( t.entityelement[ent].eleprof.meleedamagemultiplier );
				}
				if (t.versionnumbersave >= 325)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.iAffectedByGravity );
					writer.WriteLong( t.entityelement[ent].eleprof.iMoveSpeed );
					writer.WriteLong( t.entityelement[ent].eleprof.iTurnSpeed );
				}
				if (t.versionnumbersave >= 326)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.light.offsetup );  //Store spot radius.
				}

				if (t.versionnumbersave >= 327)
				{
					writer.WriteString( t.entityelement[ent].eleprof.soundset5_s.Get() );
					writer.WriteString( t.entityelement[ent].eleprof.soundset6_s.Get() );
				}

				if (t.versionnumbersave >= 328)
				{
					writer.WriteLong( t.entityelement[ent].eleprof.iUseSoundVariants );
				}
				if (t.versionnumbersave >= 329)
				{
					writer.WriteFloat(t.entityelement[ent].quatmode);
					writer.WriteFloat(t.entityelement[ent].quatx);
					writer.WriteFloat(t.entityelement[ent].quaty);
					writer.WriteFloat(t.entityelement[ent].quatz);
					writer.WriteFloat(t.entityelement[ent].quatw);
				}
				if (t.versionnumbersave >= 330)
				{
					writer.WriteFloat(t.entityelement[ent].eleprof.bAutoFlatten);
				}
				if (t.versionnumbersave >= 331)
				{
					writer.WriteString(t.entityelement[ent].eleprof.overrideanimset_s.Get());
				}
				if (t.versionnumbersave >= 332)
				{
					writer.WriteLong(t.entityelement[ent].eleprof.iscollectable);
				}
				if (t.versionnumbersave >= 333)
				{
					writer.WriteLong(t.entityelement[ent].eleprof.iSwimSpeed);
				}
				if (t.versionnumbersave >= 334)
				{
					extern cstr sEntityGroupListName[MAXGROUPSLISTS];
					int iNumberOfGroups = MAXGROUPSLISTS;
					writer.WriteLong(iNumberOfGroups);
					for (int gi = 0; gi < iNumberOfGroups; gi++)
					{
						writer.WriteString(sEntityGroupListName[gi].Get());
					}
				}
				if (t.versionnumbersave >= 335)
				{
					writer.WriteLong(t.entityelement[ent].creationOfGroupID);
				}
				if (t.versionnumbersave >= 336)
				{
					writer.WriteLong(t.entityelement[ent].eleprof.light.fLightHasProbeX);
					writer.WriteLong(t.entityelement[ent].eleprof.light.fLightHasProbeY);
					writer.WriteLong(t.entityelement[ent].eleprof.light.fLightHasProbeZ);
				}
				if (t.versionnumbersave >= 337)
				{
					writer.WriteLong(t.entityelement[ent].iCanGoUnderwater);
				}
				if (t.versionnumbersave >= 338)
				{
					writer.WriteLong(t.entityelement[ent].eleprof.clipcapacity);
					writer.WriteLong(t.entityelement[ent].eleprof.weaponpropres1);
					writer.WriteLong(t.entityelement[ent].eleprof.weaponpropres2);
				}
				if (t.versionnumbersave >= 339)
				{
					writer.WriteLong(t.entityelement[ent].eleprof.WEMaterial.customShaderID);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam1);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam2);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam3);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam4);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam5);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam6);
					writer.WriteFloat(t.entityelement[ent].eleprof.WEMaterial.customShaderParam7);
					writer.WriteString(t.entityelement[ent].eleprof.explodable_decalname.Get());
				}
				if (t.versionnumbersave >= 340)
				{
					writer.WriteString(t.entityelement[ent].eleprof.WEMaterial.WPEffect.Get());
					//PE: Fillers.

					writer.WriteFloat(t.entityelement[ent].eleprof.light.fProbeBrightness);
					// GGMAX 3.41: this record is DX11's, byte for byte, for every version up to 342. DX11 is the
					// format every shipping user has, so it is the BASE and must not drift: slot 1 is
					// iAllowBuletHole and slot 2 is iMaterialSoundIndex, both LONG, both taken from DX11.
					// 3.38 wrongly left slot 2 a float filler, which silently dropped Material Type on every save
					// even though the dropdown (M-GridEdit_part1.cpp) and the impact-sound runtime
					// (G-Entity_part2.cpp) were both already present.
					// Anything DX12 wants to store that DX11 does not goes in a NEW version block, never in these
					// reserved slots. The load side in M-Entity_part3.cpp mirrors this exactly.
					writer.WriteLong(t.entityelement[ent].iAllowBuletHole);             // DX11 slot 1
					writer.WriteLong(t.entityelement[ent].eleprof.iMaterialSoundIndex); // DX11 slot 2
					writer.WriteFloat((float)t.entityelement[ent].iClearVegetation); // Clear Grass and Trees tick, 0 (every older level) until set
					writer.WriteFloat(0.0f);
					writer.WriteLong(t.entityelement[ent].eleprof.systemwide_lua);
					writer.WriteLong(t.entityelement[ent].eleprof.isobjective_alwaysactive);
					writer.WriteLong(t.entityelement[ent].eleprof.isProjectGlobal);
					writer.WriteString("");
					writer.WriteString("");
					writer.WriteString("");
				}
				if (t.versionnumbersave >= 341)
				{
					writer.WriteLong(t.entityelement[ent].eleprof.bUseFPESettings);
				}
				if (t.versionnumbersave >= 342)
				{
					writer.WriteString(t.entityelement[ent].eleprof.soundset4a_s.Get());
				}
			}
		} 
		if ( pass == 0 ) writer.AllocateDataForWrite();
	} // pass loop

	// write data buffer to file, EntityWriter will clean itself up
	if ( FileExist(t.elementsfilename_s.Get()) == 1  )  DeleteAFile ( t.elementsfilename_s.Get() );
	OpenToWrite ( 1, t.elementsfilename_s.Get() );
	WriteData( 1, writer.GetData(), writer.GetDataSize() );
	CloseFile ( 1 );
}

void entity_savebank ( void )
{
	//  Scan entire entityelement, delete all entitybank entries not used
	if (  g.gcompilestandaloneexe == 0 && g.gpretestsavemode == 0 ) 
	{
		if (  g.entidmaster>0 ) 
		{
			Dim ( t.entitybankused,g.entidmaster  );
			for ( t.tttentid = 1 ; t.tttentid<= g.entidmaster; t.tttentid++ )
			{
				t.entitybankused[t.tttentid]=0;
			}
			for ( t.ttte = 1 ; t.ttte <= g.entityelementlist; t.ttte++ )
			{
				t.tttentid = t.entityelement[t.ttte].bankindex;
				if (  t.tttentid > 0 && t.tttentid <= g.entidmaster ) 
				{
					t.entitybankused[t.tttentid]=1;
				}
			}
			bool bEntErasedDueToGroupCheckIfStillBeingUsed = false;
			for ( t.tttentid = 1 ; t.tttentid <= g.entidmaster; t.tttentid++ )
			{
				if (  t.entitybankused[t.tttentid] == 0 ) 
				{
					// do not remove if a smart object if still being used in level
					if (t.entityprofile[t.tttentid].model_s == "group")
					{
						// what group ID is this smart object?
						cstr tmp = cstr("entitybank\\") + t.entitybank_s[t.tttentid];
						int iSmartObjectGroupIndex = GetGroupIndexFromName(tmp);
						if (iSmartObjectGroupIndex >= 0 && iSmartObjectGroupIndex < MAXGROUPSLISTS)
						{
							// scan to see if this smart object group is being used in the level
							bool bBeingUsed = false;
							if (vEntityGroupList[iSmartObjectGroupIndex].size() > 0)
							{
								int iUniqueGroupID = vEntityGroupList[iSmartObjectGroupIndex][0].iGroupID;
								for (int ee = 1; ee <= g.entityelementlist; ee++)
								{
									if (t.entityelement[ee].bankindex > 0)
									{
										if (t.entityelement[ee].y > -48000.0f) // original smart object elements are buried deep and cloned, they do not count as part of level!
										{
											int thisGroupID = t.entityelement[ee].creationOfGroupID;
											if (thisGroupID > 0 && thisGroupID == iUniqueGroupID)
											{
												bBeingUsed = true;
											}
										}
									}
								}
							}
							if (bBeingUsed == true)
							{
								// we keep the group entity parent in place to access smart object from left panel
								t.entitybankused[t.tttentid] = 1;
							}
							else
							{
								// all instnces of smart object use removed from level, so finally remove the hidden elements as no longer needed
								if (vEntityGroupList[iSmartObjectGroupIndex].size() > 0)
								{
									int iUniqueGroupID = vEntityGroupList[iSmartObjectGroupIndex][0].iGroupID;
									for (int ee = 1; ee <= g.entityelementlist; ee++)
									{
										if (t.entityelement[ee].bankindex > 0)
										{
											if (t.entityelement[ee].y <= -48000.0f)
											{
												int thisGroupID = t.entityelement[ee].creationOfGroupID;
												if (thisGroupID > 0 && thisGroupID == iUniqueGroupID)
												{
													// original smart object elements buried deep, no longer needed
													int entid = t.entityelement[ee].bankindex;
													if (entid > 0)
													{
														t.entitybankused[entid] = 0;
														bEntErasedDueToGroupCheckIfStillBeingUsed = true;
													}
													t.entityelement[ee].bankindex = 0;
													t.entityelement[ee].creationOfGroupID = -1;
												}
											}
										}
									}
								}

								// and finally remove from group list records
								vEntityGroupList[iSmartObjectGroupIndex].clear();
								sEntityGroupListName[iSmartObjectGroupIndex] = "";
							}
						}
					}
					else
					{
						// are they in the collection list (must keep parent even if no element using it right now)
						for (int c = 0; c < g_collectionList.size(); c++)
						{
							if (g_collectionList[c].iEntityID == t.tttentid)
							{
								// being used
								t.entitybankused[t.tttentid] = 1;
							}
						}
					}
				}
			}
			// however, put back if the object was NOT part of a smart object
			if (bEntErasedDueToGroupCheckIfStillBeingUsed == true)
			{
				for (t.tttentid = 1; t.tttentid <= g.entidmaster; t.tttentid++)
				{
					if (t.entitybankused[t.tttentid] == 0)
					{
						// can remove the entity associated with the smart object
						bool bIsASmartObjectChildPart = true;
						for (int eee = 1; eee <= g.entityelementlist; eee++)
						{
							if (t.entityelement[eee].bankindex == t.tttentid && t.entityelement[eee].creationOfGroupID == -1)
							{
								bIsASmartObjectChildPart = false;
								break;
							}
						}
						if (bIsASmartObjectChildPart == false)
						{
							// do not remove, it may be in a smart object but also in the level independently
							t.entitybankused[t.tttentid] = 1;
						}
					}
				}
			}
			for (t.tttentid = 1; t.tttentid <= g.entidmaster; t.tttentid++)
			{
				if (t.entitybankused[t.tttentid] == 0)
				{
					// some debug help
					char debug[MAX_PATH];
					sprintf(debug, "Removing parent object as no longer used: %d - %s", t.tttentid, t.entitybank_s[t.tttentid].Get());
					timestampactivity(0, debug);

					// free RLE data in profile
					ebe_freecubedata (t.tttentid);

					//  remove entity entry if not used when save FPM
					t.entitybank_s[t.tttentid] = "";
				}
			}
			//  shuffle to remove empty entries
			for ( t.tttentid = 1 ; t.tttentid <= g.entidmaster; t.tttentid++ )
			{
				//  not used to record where entities have been moved to
				t.entitybankused[t.tttentid]=0;
			}
			t.treadentid=1 ; t.tlargest=0;
			for ( t.tttentid = 1 ; t.tttentid<=  g.entidmaster; t.tttentid++ )
			{
				if (  t.treadentid <= g.entidmaster ) 
				{
					while (  t.entitybank_s[t.treadentid] == "" ) 
					{
						++t.treadentid ; if (  t.treadentid>g.entidmaster  )  break;
					}
					if (  t.treadentid <= g.entidmaster ) 
					{
						t.entitybank_s[t.tttentid] = t.entitybank_s[t.treadentid];
						t.entityprofileheader[t.tttentid]=t.entityprofileheader[t.treadentid];
						t.entityprofile[t.tttentid]=t.entityprofile[t.treadentid];
						if ( t.entityprofile[t.treadentid].ebe.pRLEData != NULL && t.tttentid < t.treadentid )
						{
							// if we are shifting an EBE entity into place
							// EBE entity parents can be saved shortly after, so ensure object is copied over
							t.sourceobj = g.entitybankoffset + t.tttentid;
							if ( ObjectExist ( t.sourceobj ) == 1 ) DeleteObject ( t.sourceobj );
							int iSourceObjBeingMoved = g.entitybankoffset + t.treadentid;
							if ( ObjectExist ( iSourceObjBeingMoved ) == 1 ) CloneObject ( t.sourceobj, iSourceObjBeingMoved );
							
							// this overrites regular saved EBE entities
							t.entitybank_s[t.tttentid] = cstr("EBE") + cstr(t.tttentid);
						}
						t.entityprofile[t.tttentid].ebe.dwRLESize=t.entityprofile[t.treadentid].ebe.dwRLESize;
						t.entityprofile[t.tttentid].ebe.pRLEData=t.entityprofile[t.treadentid].ebe.pRLEData;
						t.entityprofile[t.tttentid].ebe.dwMatRefCount=t.entityprofile[t.treadentid].ebe.dwMatRefCount;
						t.entityprofile[t.tttentid].ebe.iMatRef=t.entityprofile[t.treadentid].ebe.iMatRef;
						t.entityprofile[t.tttentid].ebe.dwTexRefCount=t.entityprofile[t.treadentid].ebe.dwTexRefCount;
						t.entityprofile[t.tttentid].ebe.pTexRef=t.entityprofile[t.treadentid].ebe.pTexRef;
						for ( t.tt = 0 ; t.tt <=  100 ; t.tt++ ) t.entitybodypart[t.tttentid][t.tt]=t.entitybodypart[t.treadentid][t.tt] ;
						for ( t.tt = 0 ; t.tt <=  g.animmax ; t.tt++ ) t.entityanim[t.tttentid][t.tt]=t.entityanim[t.treadentid][t.tt] ;
						for ( t.tt = 0 ; t.tt <=  g.footfallmax ; t.tt++ ) t.entityfootfall[t.tttentid][t.tt]=t.entityfootfall[t.treadentid][t.tt] ; 
						for ( t.tt = 0 ; t.tt <=  100 ; t.tt++ ) t.entitydecal_s[t.tttentid][t.tt]=t.entitydecal_s[t.treadentid][t.tt] ;
						for ( t.tt = 0 ; t.tt <=  100 ; t.tt++ ) t.entitydecal[t.tttentid][t.tt]=t.entitydecal[t.treadentid][t.tt] ;
						t.entitybankused[t.treadentid]=t.tttentid;
						t.tlargest=t.tttentid;
					}
					else
					{
						// wipe after end of shuffle
						t.entitybank_s[t.tttentid] = "";
						t.entityprofileheader[t.tttentid].desc_s = "";
						t.entityprofile[t.tttentid].ebe.dwRLESize = 0;
						t.entityprofile[t.tttentid].ebe.pRLEData = NULL;
						t.entityprofile[t.tttentid].ebe.dwMatRefCount = 0;
						t.entityprofile[t.tttentid].ebe.iMatRef = NULL;
						t.entityprofile[t.tttentid].ebe.dwTexRefCount = 0;
						t.entityprofile[t.tttentid].ebe.pTexRef = NULL;
					}
				}
				else
				{
					// wipe after end of shuffle
					t.entitybank_s[t.tttentid]="";
					t.entityprofileheader[t.tttentid].desc_s = "";
					t.entityprofile[t.tttentid].ebe.dwRLESize = 0;
					t.entityprofile[t.tttentid].ebe.pRLEData = NULL;
					t.entityprofile[t.tttentid].ebe.dwMatRefCount = 0;
					t.entityprofile[t.tttentid].ebe.iMatRef = NULL;
					t.entityprofile[t.tttentid].ebe.dwTexRefCount = 0;
					t.entityprofile[t.tttentid].ebe.pTexRef = NULL;
				}
				++t.treadentid;
			}
			//  update bank index numbers in entityelements
			for ( t.ttte = 1 ; t.ttte <= g.entityelementlist; t.ttte++ )
			{
				t.tttentid = t.entityelement[t.ttte].bankindex;
				if ( t.tttentid > 0 && t.tttentid <= g.entidmaster ) 
				{
					if (t.entityelement[t.ttte].bankindex != t.entitybankused[t.tttentid])
					{
						// some debug help
						char debug[MAX_PATH];
						int iNewEntID = t.entitybankused[t.tttentid];
						sprintf(debug, "Object element %d reassigned parent object '%s' from %d to %d", t.ttte, t.entitybank_s[iNewEntID].Get(), t.entityelement[t.ttte].bankindex, iNewEntID);
						timestampactivity(0, debug);
					}

					// new entity entry place index
					t.entityelement[t.ttte].bankindex = t.entitybankused[t.tttentid];
				}
			}
			UnDim (  t.entitybankused );

			// new list size
			if ( g.entidmaster != t.tlargest ) 
			{
				// new parent object list (and trigger a load)
				g.entidmaster = t.tlargest;
				t.entityorsegmententrieschanged = 1;

				// when list changes, output a copy of the parent object list for debugging
				char debug[MAX_PATH];
				sprintf(debug, "Parent Object List (%d):", g.entidmaster);
				timestampactivity(0, debug);
				for (int id = 1; id <= g.entidmaster; id++)
				{
					sprintf(debug, "%d - %s", id, t.entitybank_s[id].Get());
					timestampactivity(0, debug);
				}
			}
		}
	}

	//  Save entity bank
	cstr entitybank_s = g.mysystem.levelBankTestMap_s+"map.ent";
	if ( FileExist(entitybank_s.Get()) == 1 ) DeleteAFile ( entitybank_s.Get() );
	OpenToWrite ( 1, entitybank_s.Get() );
	WriteLong ( 1, g.entidmaster );
	if ( g.entidmaster>0 ) 
	{
		for ( t.entid = 1 ; t.entid <= g.entidmaster; t.entid++ )
		{
			WriteString ( 1, t.entitybank_s[t.entid].Get() );
		}
	}
	CloseFile ( 1 );
}

void entity_savebank_ebe ( void )
{
	// Empty EBEs from testmap folder
	// 190417 - oops, this is deleting perfectly needed textures from testmap folder
	// when it should leave textures alone that may belong to the level EBEs
	// so set a flag to protect textures when save
	cstr pStoreOld = GetDir(); SetDir ( g.mysystem.levelBankTestMap_s.Get() ); //"levelbank\\testmap\\" );
	mapfile_emptyebesfromtestmapfolder(true);
	SetDir ( pStoreOld.Get() );

	// now save all EBE to testmap folder
	for ( t.tttentid = 1 ; t.tttentid <= g.entidmaster; t.tttentid++ )
	{
		if ( strlen(t.entitybank_s[t.tttentid].Get()) > 0 )
		{
			if ( t.entityprofile[t.tttentid].ebe.dwRLESize > 0 )
			{
				// Save EBE to represent this creation in the level
				cStr tSaveFile = g.mysystem.levelBankTestMap_s + cstr("ebe") + cstr(t.tttentid) + cstr(".ebe");
				
				ebe_save_ebefile ( tSaveFile, t.tttentid );
			}
		}
	}
}

void entity_loadbank ( void )
{
	// 050416 - build a black list for parental control (used below)
	 // No blacklist as all content is pre-vetted and clean

	 extern bool bKeepWindowsResponding;
	 void EmptyMessages(void);
	 if (bKeepWindowsResponding)
		 EmptyMessages();

	//  If ent file exists
	t.filename_s=t.levelmapptah_s+"map.ent";
	if (  FileExist(t.filename_s.Get()) == 1 ) 
	{
		//  Destroy old entities
		entity_deletebank ( );

		if (bKeepWindowsResponding)
			EmptyMessages();

		//  Load entity bank
		OpenToRead (  1, cstr(t.levelmapptah_s+"map.ent").Get() );
		g.entidmaster = ReadLong ( 1 );
		if (  g.entidmaster>0 ) 
		{
			entity_validatearraysize ( );
			for ( t.entid = 1 ; t.entid<=  g.entidmaster; t.entid++ )
			{
				t.entitybank_s[t.entid] = ReadString ( 1 );
			}
		}
		CloseFile (  1 );

		// 050416 - blank out any entities which are blacklisted
		g_bBlackListRemovedSomeEntities = false;
		if ( g_pBlackList != NULL )
		{
			char pThisEntityFilename[512];
			for ( t.entid = 1 ; t.entid <= g.entidmaster; t.entid++ )
			{
				strcpy ( pThisEntityFilename, t.entitybank_s[t.entid].Get() );
				strlwr ( pThisEntityFilename );
				for ( int iBlackListIndex=0; iBlackListIndex<g_iBlackListMax; iBlackListIndex++ )
				{
					int iBlacklistEntityNameLength = strlen ( g_pBlackList[iBlackListIndex] );
					int iCompareEnd = strlen ( pThisEntityFilename ) - iBlacklistEntityNameLength;
					if ( strnicmp ( g_pBlackList[iBlackListIndex], pThisEntityFilename + iCompareEnd, iBlacklistEntityNameLength )==NULL )
					{
						// this entity has been banned by parents
						g_bBlackListRemovedSomeEntities = true;
						t.entitybank_s[t.entid] = "";
					}
				}
			}
		}

		// 260215 - Do a pre-scan to determine if any entities are missing
		if ( Len(t.editor.replacefilepresent_s.Get())>1 ) 
		{
			// clear replacement output array
			Dim2( t.replacements_s, 1000, 1 );
			for ( int n=0; n < 1000; n++ )
			{
				t.replacements_s[n][0]="";
				t.replacements_s[n][1]="";
			}

			// load all replacements in a table
			Dim( t.replacementsinput_s, 1000 );
			t.treplacementmax=0;
			if ( FileExist(t.editor.replacefilepresent_s.Get()) == 1 ) 
			{
				LoadArray ( t.editor.replacefilepresent_s.Get(), t.replacementsinput_s );
				for ( t.l = 0 ; t.l <= ArrayCount(t.replacementsinput_s); t.l++ )
				{
					t.tline_s = t.replacementsinput_s[t.l];
					if ( Len(t.tline_s.Get()) > 0 ) 
					{
						for ( t.n = 1 ; t.n<=  Len(t.tline_s.Get()); t.n++ )
						{
							if (  cstr(Mid(t.tline_s.Get(),t.n)) == "=" ) 
							{
								t.told_s=Left(t.tline_s.Get(),t.n-1);
								t.told2_s="";
								for ( t.nn = 1 ; t.nn<=  Len(t.told_s.Get()); t.nn++ )
								{
									t.told2_s=t.told2_s+Mid(t.told_s.Get(),t.nn);
									if ( (cstr(Mid(t.told_s.Get(),t.nn)) == "\\" && cstr(Mid(t.told_s.Get(),t.nn+1)) == "\\") || (cstr(Mid(t.told_s.Get(),t.nn)) == "/" && cstr(Mid(t.told_s.Get(),t.nn+1)) == "/" ) )
									{
										++t.nn;
									}
								}
								t.tnew_s=Right(t.tline_s.Get(),Len(t.tline_s.Get())-t.n);
								++t.treplacementmax;
								t.replacements_s[t.treplacementmax][0]=Lower(t.told2_s.Get());
								t.replacements_s[t.treplacementmax][1]=Lower(t.tnew_s.Get());
							}
						}
					}
				}
			}

			if (bKeepWindowsResponding)
				EmptyMessages();

			//  go through all entities FPM is about to use
			for ( t.entid = 1 ; t.entid <= g.entidmaster; t.entid++ )
			{
				if ( strlen ( t.entitybank_s[t.entid].Get() ) > 0 )
				{
					LPSTR pEntityRef = t.entitybank_s[t.entid].Get();
					if ( pEntityRef[1] == ':')
					{
						// corrects for an ugly bug which exported entity bank references in absolute paths!
						t.tfile_s = t.entitybank_s[t.entid];
						for (t.tt = 1; t.tt <= t.treplacementmax; t.tt++)
						{
							LPSTR pReplaceSearch = t.replacements_s[t.tt][0].Get();
							LPSTR pThisEntFile = t.tfile_s.Get();
							if ( stricmp ( pReplaceSearch, pThisEntFile ) == NULL )
							{
								// found a match with an entry in the .REPLACE file
								// so replace master entry (entity instances will continue to reference by index)
								t.tnewent_s = t.replacements_s[t.tt][1];
								t.entitybank_s[t.entid] = Right(t.tnewent_s.Get(), Len(t.tnewent_s.Get()) - Len("entitybank\\"));
								break;
							}
						}
					}
					else
					{
						t.tfile_s = g.fpscrootdir_s + "\\Files\\entitybank\\" + t.entitybank_s[t.entid];
						if (FileExist(t.tfile_s.Get()) == 0)
						{
							t.tbinfile_s = cstr(Left(t.tfile_s.Get(), Len(t.tfile_s.Get()) - 4)) + cstr(".bin");
							if (FileExist(t.tbinfile_s.Get()) == 0)
							{
								//  cannot find FPE or BIN, so search replace file if we have a substitute
								t.tent_s = cstr("entitybank\\") + t.entitybank_s[t.entid];
								t.ttry2_s = "";
								for (t.nn = 1; t.nn <= Len(t.tent_s.Get()); t.nn++)
								{
									t.ttry2_s = t.ttry2_s + Mid(t.tent_s.Get(), t.nn);
									if ((cstr(Mid(t.tent_s.Get(), t.nn)) == "\\" && cstr(Mid(t.tent_s.Get(), t.nn + 1)) == "\\") || (cstr(Mid(t.tent_s.Get(), t.nn)) == "/" && cstr(Mid(t.tent_s.Get(), t.nn + 1)) == "/"))
									{
										++t.nn;
									}
								}
								t.tent_s = Lower(t.ttry2_s.Get());
								t.tfoundmatch = 0;
								for (t.tt = 1; t.tt <= t.treplacementmax; t.tt++)
								{
									if (t.replacements_s[t.tt][0] == t.tent_s)
									{
										//  found a match with an entry in the .REPLACE file
										//  so replace master entry (entity instances will continue to reference by index)
										t.tnewent_s = t.replacements_s[t.tt][1];
										t.entitybank_s[t.entid] = Right(t.tnewent_s.Get(), Len(t.tnewent_s.Get()) - Len("entitybank\\"));
										t.tfoundmatch = 1;
									}
								}
							}
						}
					}
				}
			}
			//  retain replacements$() for later in loadelementsdata
		}

		if (bKeepWindowsResponding)
			EmptyMessages();

		//  Load in all entity objects and data
		entity_loadentitiesnow ( );
	}
}

// allows extra entity to be loaded
int g_iAddEntitiesMode = 0;
int g_iAddEntitiesModeFrom = 0;

void entity_loadentitiesnow ( void )
{
	// Load entities specified by bank
	if ( g.entidmaster>0 ) 
	{
		char debug[MAX_PATH];
		sprintf(debug, "Loading master objects: %ld", g.entidmaster);
		extern int total_mem_from_load;
		total_mem_from_load = 0;
		timestampactivity(0, debug);
		int iFrom = 1;
		if (g_iAddEntitiesMode > 0) iFrom = g_iAddEntitiesModeFrom;
		for ( t.entid = iFrom; t.entid <= g.entidmaster; t.entid++ )
		{
			extern bool bKeepWindowsResponding;
			void EmptyMessages(void);
			if (bKeepWindowsResponding)
				EmptyMessages();

			// set entity name and load it in
			t.entdir_s = "entitybank\\";
			t.ent_s = t.entitybank_s[t.entid];
			t.entpath_s = getpath(t.ent_s.Get());
			t.tonscreenprompt_s = "";
			// if not an FPE, load FPE from EBE source
			if ( strcmp ( Lower(Right(t.ent_s.Get(),4)), ".fpe" ) != NULL )
			{
				// special EBE entity
				ebe_load_ebefile ( g.mysystem.levelBankTestMap_s + cstr("ebe") + cstr(t.entid) + cstr(".ebe"), t.entid );
				t.entityprofileheader[t.entid].desc_s = cstr("EBE") + cstr(t.entid);
				t.entdir_s = g.mysystem.levelBankTestMap_s;
				t.ent_s = cstr("ebe") + cstr(t.entid) + cstr(".fpe");
				t.entpath_s = "";
			}

			// when loading entities, all instances already in place, just need to create a STUB for the smart object
			extern int g_iAbortedAsEntityIsGroupFileMode;
			g_iAbortedAsEntityIsGroupFileMode = 1;

			// stubs will load the group, and that groups entities, but not instantiate the elements of that smart object (need ALL entities profiles to load in first!)
			extern int g_iAbortedAsEntityIsGroupFileModeStubOnly;
			g_iAbortedAsEntityIsGroupFileModeStubOnly = 1;

			extern uint32_t SetMasterObject;
			SetMasterObject = g.entitybankoffset + t.entid;
			// regular FPE entity
			entity_load ( );
			SetMasterObject = 0;

			// only used for when loading entities
			g_iAbortedAsEntityIsGroupFileModeStubOnly = 0;

			if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
			if (  t.desc_s == "" ) 
			{
				// free RLE data in profile
				ebe_freecubedata ( t.entid );
			
				//  where entities have been lost, delete from list
				t.entitybank_s[t.entid]="";
			}

			// keep multiplayer alive
			if ( t.game.runasmultiplayer == 1 ) mp_refresh ( );
		}
		timestampactivity(0, "End Loading master objects!");
	}
}

void entity_deletebank ( void )
{
	//  Destroy old entities
	if ( g.entidmastermax>0 ) 
	{
		for ( t.entid = 1 ; t.entid<=  g.entidmastermax; t.entid++ )
		{
			// delete parent entity object
			t.entobj = g.entitybankoffset+t.entid;
			if ( ObjectExist(t.entobj) == 1  ) DeleteObject (  t.entobj );

			//PE: Delete all textures used by master object here.
			//PE: TODO Perhaps use the .lst file from newly loaded level and do not delete those if they are used in the new level.
			void WickedCall_FreeImage_By_MasterID(uint32_t masterid);
			WickedCall_FreeImage_By_MasterID(t.entobj);

			// free RLE data in profile
			ebe_freecubedata ( t.entid );

			// wipe from table
			t.entitybank_s[t.entid]="";
		}
	}

	//  reset character creator bankoffset
	t.characterkitcontrol.bankOffset = 0;
	t.characterkitcontrol.count = 0; // 150216 - Dave needs to learn how to clean up after himself!

	//  Destroy profile data
	UnDim (  t.entityprofile );
	#ifdef DEFAULTMASTERENTITY
	Dim(t.entityprofile, DEFAULTMASTERENTITY);
	g.entidmastermax = DEFAULTMASTERENTITY;
	#else
	Dim(t.entityprofile, 100);
	g.entidmastermax = 100;
	#endif
	g.entidmaster=0;
}

void entity_deleteelementsdata ( void )
{
	//  Free any old elements
	entity_deleteelements ( );

	//  Clear counter for new load
	g.entityelementlist=0;
	g.entityelementmax=0;
}

void entity_deleteelements ( void )
{
	//  Quick deletes
	if ( g.entityelementlist > 0 ) 
	{
		DeleteObjects (  g.entityviewstartobj+1, g.entityviewstartobj+g.entityelementlist );
	}
	if ( g.entityattachmentindex > 0 ) 
	{
		DeleteObjects (g.entityattachmentsoffset, g.entityattachmentsoffset + g.entityattachmentindex);
		DeleteObjects (g.entityattachments2offset, g.entityattachments2offset + g.entityattachmentindex);
	}

	//  set character creator offset back to 0 (which is a nice indicator that it is not in use also)
	t.characterkitcontrol.offset = 0;
}

void entity_assignentityparticletodecalelement ( void )
{
	if (  t.originatore>0 ) 
	{
		if (  t.entityelement[t.originatore].eleprof.particleoverride == 1 ) 
		{
			t.decalelement[t.d].particle=t.entityelement[t.originatore].eleprof.particle;
		}
	}
}

void entity_addentitytomap_core ( void )
{
	// called from _entity_addentitytomap and also _game_masterroot
	bool bDroppedBackInSameElement = false;
	if ( t.gridentityoverwritemode == 0 )
	{
		// First see if we have a prfeference (when click object to cursor, ideally want it dropped back in same t.e
		t.tokay = 0;
		if (t.gridentitypreferelementindex > 0)
		{
			if (t.entityelement[t.gridentitypreferelementindex].maintype == 0)
			{
				t.tokay = t.gridentitypreferelementindex;
				bDroppedBackInSameElement = true;
			}
			t.gridentitypreferelementindex = 0;
		}

		// Create new or use free entity element
		if (t.tokay == 0 && g.entityelementlist > 0)
		{
			for (t.e = 1; t.e <= g.entityelementlist; t.e++)
			{
				if (t.entityelement[t.e].maintype == 0)
				{
					t.tokay = t.e;
					//PE: If old object was part of group remove it, or if will get deleted on next load.
					t.entityelement[t.e].creationOfGroupID = -1;
					break;
				}
			}
		}

		if ( t.tokay == 0 ) 
		{
			++g.entityelementlist;
			t.e=g.entityelementlist;
			if ( g.entityelementlist>g.entityelementmax ) 
			{
				// grown by half its size (at least 10), not by 10: every growth copies the whole array out and back
				int iGrowBy = g.entityelementmax / 2;
				if ( iGrowBy < 10 ) iGrowBy = 10;
				entity_growelementarray ( g.entityelementmax + iGrowBy );
				t.e = g.entityelementlist;
			}
		}
		else
		{
			t.e=t.tokay;
		}
	}
	else
	{
		// can force new entity into a specific slot (when undo entiy group delete)
		t.e = t.gridentityoverwritemode;
	}

	//PE: Any newly added object should be unlocked.
	if (t.gridentityoverwritemode == 0)
	{
		extern std::vector<sRubberBandType> vEntityLockedList;
		t.entityelement[t.e].editorlock = 0;
		for (int i = 0; i < vEntityLockedList.size(); i++)
		{
			if (vEntityLockedList[i].e == t.e)
			{
				vEntityLockedList.erase(vEntityLockedList.begin() + i);
				sObject* pObject;
				if (t.entityelement[t.e].obj > 0)
				{
					if (t.entityelement[t.e].obj < g_iObjectListCount)
					{
						pObject = g_ObjectList[t.entityelement[t.e].obj];
						if (pObject)
						{
							WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
							WickedCall_SetObjectHighlightRed(pObject, false);
						}
					}
				}

				break;
			}
		}
	}

	// noticed newly created can have old garbage in them, clear to be safe
	t.entityelement[t.e].collected = 0;

	//  Fill entity element details
	t.entityelement[t.e].editorfixed=t.gridentityeditorfixed;
	t.entityelement[t.e].maintype=t.entitymaintype;
	t.entityelement[t.e].bankindex=t.entitybankindex;
	t.entityelement[t.e].staticflag=t.gridentitystaticmode;
	if (t.gridentityoverwritemode == 0 && bDroppedBackInSameElement == false)
	{
		// a new placement starts with Allow Bullet Holes unset, which follows its .fpe (entity_allowsbulletholes), rather
		// than the tick of a deleted object that used this element; one dropped back into its own element, or restored
		// by undo, keeps its setting; Clear Grass and Trees likewise
		t.entityelement[t.e].iAllowBuletHole = 0;
		t.entityelement[t.e].iClearVegetation = 0;

		// where its .fpe doesn't say, the Settings choices (Developer) go into the new placement's own ticks, so changing
		// them later leaves placed objects as they are. On leaves characters without holes, and markers, characters and
		// decals clearing nothing, as the rules do
		int iBank = t.entitybankindex;
		bool bCharacter = (t.entityprofile[iBank].ischaracter != 0);
		if (t.entityprofile[iBank].allowbulletholes == -1)
		{
			if (g_iDefaultBulletHoles == 1 && !bCharacter) t.entityelement[t.e].iAllowBuletHole = 1;
			if (g_iDefaultBulletHoles == 2) t.entityelement[t.e].iAllowBuletHole = 2;
		}
		if (t.entityprofile[iBank].clearvegetation == -1)
		{
			bool bNeverClears = (t.entityprofile[iBank].ismarker != 0 || bCharacter || t.entityprofile[iBank].bIsDecal);
			if (g_iDefaultClearVegetation == 1 && !bNeverClears) t.entityelement[t.e].iClearVegetation = 1;
			if (g_iDefaultClearVegetation == 2) t.entityelement[t.e].iClearVegetation = 2;
		}
	}
	t.entityelement[t.e].iHasParentIndex=t.gridentityhasparent;
	t.entityelement[t.e].x=t.gridentityposx_f;
	t.entityelement[t.e].z=t.gridentityposz_f;
	t.entityelement[t.e].y=t.gridentityposy_f;
	t.entityelement[t.e].rx=t.gridentityrotatex_f;
	t.entityelement[t.e].ry=t.gridentityrotatey_f;
	t.entityelement[t.e].rz=t.gridentityrotatez_f;
	t.entityelement[t.e].quatmode = t.gridentityrotatequatmode;
	t.entityelement[t.e].quatx = t.gridentityrotatequatx_f;
	t.entityelement[t.e].quaty = t.gridentityrotatequaty_f;
	t.entityelement[t.e].quatz = t.gridentityrotatequatz_f;
	t.entityelement[t.e].quatw = t.gridentityrotatequatw_f;
	t.entityelement[t.e].scalex=t.gridentityscalex_f-100;
	t.entityelement[t.e].scaley=t.gridentityscaley_f-100;
	t.entityelement[t.e].scalez=t.gridentityscalez_f-100;
	t.entityelement[t.e].eleprof=t.grideleprof;
	t.entityelement[t.e].eleprof.light.index=0;
	t.entityelement[t.e].soundset = 0;
	t.entityelement[t.e].soundset1 = 0;
	t.entityelement[t.e].soundset2 = 0;
	t.entityelement[t.e].soundset3 = 0;
	t.entityelement[t.e].soundset4 = 0;
	t.entityelement[t.e].soundset5 = 0;
	t.entityelement[t.e].soundset6 = 0;
	t.entityelement[t.e].underground=0;
	t.entityelement[t.e].beenmoved=1;

	//PE: Always false by default.
	t.entityelement[t.e].eleprof.systemwide_lua = false;
	
	// auto flatten system
	t.entityelement[t.e].eleprof.iFlattenID = -1; // cannot carry this ID over
	if (!g_bEnableAutoFlattenSystem) //PE: If disabled always disable autoflatten.
		t.entityelement[t.e].eleprof.bAutoFlatten = false;
	extern bool g_bCreatingHiddenGroupInstance;
	if (g_bCreatingHiddenGroupInstance == true)
	{
		t.entityelement[t.e].iIsSmarkobjectDummyObj = 1;
	}

	t.entityelement[t.e].lua.outofrangefreeze = 0;
}

// the entity element array (and its shader values) made big enough for iNewMax elements, the elements kept. Each growth
// copies every element out and back, 60-150 ms with ~1100 elements, so callers grow it by a lot at once
void entity_growelementarray ( int iNewMax )
{
	if ( iNewMax <= g.entityelementmax ) return;
	int iOldMax = g.entityelementmax;
	Dim ( t.storeentityelement, iOldMax );
	for ( int e = 1; e <= iOldMax; e++ )
	{
		t.storeentityelement[e] = t.entityelement[e];
	}
	UnDim ( t.entityelement );
	UnDim ( t.entityshadervar );
	g.entityelementmax = iNewMax;
	Dim ( t.entityelement, g.entityelementmax );
	Dim2 ( t.entityshadervar, g.entityelementmax, g.globalselectedshadermax );
	for ( int e = 1; e <= iOldMax; e++ )
	{
		t.entityelement[e] = t.storeentityelement[e];
	}
}

void entity_addentitytomap ( void )
{
	// Entity To Add
	t.entitymaintype=1;
	t.entitybankindex=t.gridentity;
	entity_addentitytomap_core ( );
	SpawnProbe_Mark(SPAWNSTEP_ADDCORE);

	// transfer waypoint zone index to entityelement
	if (t.grideleprof.trigger.waypointzoneindex > 0)
	{
		if (t.grideleprof.trigger.waypointzoneindex < t.waypoint.size())
		{
			t.waypointindex = t.grideleprof.trigger.waypointzoneindex;
			t.entityelement[t.e].eleprof.trigger.waypointzoneindex = t.waypointindex;
			t.waypoint[t.waypointindex].linkedtoentityindex = t.e;
			waypoint_fixcorruptduplicate(t.e); // moved from below to here as we have valid t.waypointindex
		}
		t.grideleprof.trigger.waypointzoneindex = 0;
	}

	//  as create entity, apply any texture change required
	t.stentid=t.entid ; t.entid=t.entitybankindex;
	t.entdir_s="entitybank\\" ; t.ent_s=t.entitybank_s[t.entid] ; t.entpath_s=getpath(t.ent_s.Get());

	//  GRIDELEPROF might contain GUN+FLAK Data
	t.entid=t.entityelement[t.e].bankindex;
	t.tgunid_s=t.entityprofile[t.entid].isweapon_s;
	entity_getgunidandflakid ( );
	if (  t.tgunid>0 ) 
	{
		// populate the actual gun and flak settings (for further weapon entity creations)
		int firemode = 0; // 110718 - entity properties should only edit first primary gun settings (so we dont mess up enhanced weapons)
		g.firemodes[t.tgunid][firemode].settings.damage=t.grideleprof.damage;
		g.firemodes[t.tgunid][firemode].settings.accuracy=t.grideleprof.accuracy;
		g.firemodes[t.tgunid][firemode].settings.reloadqty=t.grideleprof.reloadqty;
		g.firemodes[t.tgunid][firemode].settings.iterate=t.grideleprof.fireiterations;
		g.firemodes[t.tgunid][firemode].settings.range=t.grideleprof.range;
		g.firemodes[t.tgunid][firemode].settings.dropoff=t.grideleprof.dropoff;
		g.firemodes[t.tgunid][firemode].settings.usespotlighting=t.grideleprof.usespotlighting;
		g.firemodes[t.tgunid][firemode].settings.clipcapacity = t.grideleprof.clipcapacity;
		g.firemodes[t.tgunid][firemode].settings.weaponpropres1 = t.grideleprof.weaponpropres1;
		g.firemodes[t.tgunid][firemode].settings.weaponpropres2 = t.grideleprof.weaponpropres2;

		//  which must also populate ALL other entities of same weapon
		t.tgunidchanged=t.tgunid;
		for ( t.te = 1 ; t.te<=  g.entityelementlist; t.te++ )
		{
			t.tentid=t.entityelement[t.te].bankindex;
			t.tgunid_s=t.entityprofile[t.tentid].isweapon_s;
			entity_getgunidandflakid ( );
			if (  t.tgunid == t.tgunidchanged ) 
			{
				t.entityelement[t.te].eleprof.damage=t.grideleprof.damage;
				t.entityelement[t.te].eleprof.accuracy=t.grideleprof.accuracy;
				t.entityelement[t.te].eleprof.reloadqty=t.grideleprof.reloadqty;
				t.entityelement[t.te].eleprof.fireiterations=t.grideleprof.fireiterations;
				t.entityelement[t.te].eleprof.range=t.grideleprof.range;
				t.entityelement[t.te].eleprof.dropoff=t.grideleprof.dropoff;
				t.entityelement[t.te].eleprof.usespotlighting=t.grideleprof.usespotlighting;
				t.entityelement[t.te].eleprof.clipcapacity = t.grideleprof.clipcapacity;
				t.entityelement[t.te].eleprof.weaponpropres1 = t.grideleprof.weaponpropres1;
				t.entityelement[t.te].eleprof.weaponpropres2 = t.grideleprof.weaponpropres2;
				t.entityelement[t.te].eleprof.lifespan=t.grideleprof.lifespan;
				t.entityelement[t.te].eleprof.throwspeed=t.grideleprof.throwspeed;
				t.entityelement[t.te].eleprof.throwangle=t.grideleprof.throwangle;
				t.entityelement[t.te].eleprof.bounceqty=t.grideleprof.bounceqty;
				t.entityelement[t.te].eleprof.explodeonhit=t.grideleprof.explodeonhit;
			}
		}
	}

	//  If multiplayer start marker, must propogate any script change to all others
	if (  t.entityprofile[t.entid].ismarker == 7 ) 
	{
		for ( t.te = 1 ; t.te<=  g.entityelementlist; t.te++ )
		{
			if (  t.te != t.e ) 
			{
				t.tentid=t.entityelement[t.te].bankindex;
				if (  t.entityprofile[t.tentid].ismarker == 7 ) 
				{
					t.entityelement[t.te].eleprof.aimain_s=t.entityelement[t.e].eleprof.aimain_s;
					t.entityelement[t.te].eleprof.soundset_s = t.entityelement[t.e].eleprof.soundset_s;
					t.entityelement[t.te].eleprof.soundset1_s = t.entityelement[t.e].eleprof.soundset1_s;
					t.entityelement[t.te].eleprof.soundset2_s = t.entityelement[t.e].eleprof.soundset2_s;
					t.entityelement[t.te].eleprof.soundset3_s = t.entityelement[t.e].eleprof.soundset3_s;
					t.entityelement[t.te].eleprof.soundset4_s = t.entityelement[t.e].eleprof.soundset4_s;
					t.entityelement[t.te].eleprof.soundset5_s = t.entityelement[t.e].eleprof.soundset5_s;
					t.entityelement[t.te].eleprof.soundset6_s = t.entityelement[t.e].eleprof.soundset6_s;
					t.entityelement[t.te].eleprof.soundset4a_s = t.entityelement[t.e].eleprof.soundset4a_s;
					t.entityelement[t.te].eleprof.overrideanimset_s = t.entityelement[t.e].eleprof.overrideanimset_s;
					t.entityelement[t.te].eleprof.strength=t.entityelement[t.e].eleprof.strength;
					t.entityelement[t.te].eleprof.speed=t.entityelement[t.e].eleprof.speed;
					t.entityelement[t.te].eleprof.animspeed=t.entityelement[t.e].eleprof.animspeed;
				}
			}
		}
	}

	//  Add entity reference into map
	t.tupdatee=t.e; entity_updateentityobj ( );
	SpawnProbe_Mark(SPAWNSTEP_POSITION);

	// mark as static if it was
	if ( t.entityelement[t.tupdatee].staticflag == 1 ) g.projectmodifiedstatic = 1;

	// 160616 - just added EBE Builder New Site Entity 
	if ( t.entityprofile[t.gridentity].isebe > 0 )
	{
		if ( stricmp ( t.entitybank_s[t.gridentity].Get(), "..\\ebebank\\_builder\\New Site.fpe" ) == NULL )
		{
			// NEW FPE and EBE SITE
			t.entitybankindex = t.gridentity;

			// add new entity to entity library
			t.entityprofileheader[t.entitybankindex].desc_s = cstr("EBE") + cstr(t.entitybankindex);

			// update entityelement with new parent entity details
			t.entityelement[t.e].bankindex = t.entitybankindex;

			// change to site name after above creation from orig FPE template
			ebe_newsite ( t.e );

			// update entity FPE parent object from above newsite entity element obj
			t.entitybank_s[t.entitybankindex] = t.entityprofileheader[t.entitybankindex].desc_s;
			t.entityelement[t.e].bankindex = t.entitybankindex;
			t.entityelement[t.e].eleprof.name_s = t.entityprofileheader[t.entitybankindex].desc_s;
			ebe_updateparent ( t.e );

			// ensure mouse is released before painting on grid
			t.ebe.bReleaseMouseFirst = true;
		}
		else
		{
			// Selected an existing EBE entity from library
		}
	}

	//PE: Moved here as we use the object direction vector for spot lights.
	// update infinilight list with addition
	if (t.entityprofile[t.entitybankindex].ismarker == 2 || t.entityprofile[t.entitybankindex].ismarker == 5 || t.entityelement[t.e].eleprof.usespotlighting || t.entityprofile[t.entitybankindex].entitylight)
	{
		//PE: Some weapons have usespotlighting , this ruin all the lua light states.
		// GG: an entity that carries a light gets it however it was added
		if (!(t.bSpawnCalledFromLua && t.entityprofile[t.entitybankindex].ismarker == 0 ) || t.entityprofile[t.entitybankindex].entitylight)
		{
			lighting_refreshorhold();
			entity_updatelightobj(t.e, t.entityelement[t.e].obj);
		}
	}
	SpawnProbe_Mark(SPAWNSTEP_LIGHTS);

	// if new particle emitter, update it when created (to start the particle emission)
	entity_updateparticleemitter(t.tupdatee);
	SpawnProbe_Mark(SPAWNSTEP_EMITTER);

	// when add an entity to the scene, auto flatten if flagged
	if (t.entityelement[t.e].eleprof.iFlattenID == -1)
		entity_autoFlattenWhenAdded(t.e); //MD: Wrapped this section into a function to use when loading in levels to auto flatten areas
	else
		entity_updateautoflatten(t.e); //LB: Fix in case already added (in prepareobj for example)
	SpawnProbe_Mark(SPAWNSTEP_FLATTEN);

	// ensure collection list up to date with new entity additions (such as weapons and other implied collectables)
	extern bool g_bSelectedNewObjectToAddToLevel;
	if (g_bSelectedNewObjectToAddToLevel == true)
	{
		g_bSelectedNewObjectToAddToLevel = false;
		extern bool g_bUpdateCollectionList;
		g_bUpdateCollectionList = true;
	}
}
bool bUpdateObjectList = false;
void entity_deleteentityfrommap ( void )
{
	// GGMAX 2.35: a deleted entity must refresh the CACHED local shadow atlas.
	// The cache's change detector only spots casters that MOVED (world matrix vs last frame's);
	// a deleted object leaves the objects array entirely, so nothing remains to compare and the
	// cache silently keeps its shadow — user repro: delete the ammo in the editor, its shadow
	// stays on the table. Every editor delete route reaches this one function (7 call sites), so
	// this is the single place that closes it.
	extern void GGInvalidateLocalShadows();
	GGInvalidateLocalShadows();

	bUpdateObjectList = true;
	//  Entity Type To Delete
	t.entitymaintype=1;

	//  Use entity coord to find tile
	t.tupdatee=t.tentitytoselect;

	// remember entity bank index for later light refresh
	int iWasEntID = t.entityelement[t.tupdatee].bankindex;

	// mark as static if it was
	if ( t.entityelement[t.tupdatee].staticflag == 1 ) g.projectmodifiedstatic = 1;

	//  blank from entity element list
	t.entityelement[t.tupdatee].bankindex=0;
	t.entityelement[t.tupdatee].maintype=0;
	t.entityelement[t.tupdatee].iHasParentIndex = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset) ; t.entityelement[t.tupdatee].soundset = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset1) ; t.entityelement[t.tupdatee].soundset1 = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset2) ; t.entityelement[t.tupdatee].soundset2 = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset3); t.entityelement[t.tupdatee].soundset3 = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset4); t.entityelement[t.tupdatee].soundset4 = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset5); t.entityelement[t.tupdatee].soundset5 = 0;
	deleteinternalsound(t.entityelement[t.tupdatee].soundset6); t.entityelement[t.tupdatee].soundset6 = 0;

	//  Delete any associated waypoint/trigger zone
	t.waypointindex=t.entityelement[t.tupdatee].eleprof.trigger.waypointzoneindex;
	if (  t.waypointindex > 0 ) 
	{
		if (  t.grideleprof.trigger.waypointzoneindex != t.waypointindex && t.tDontDeleteWPFlag  ==  0 ) 
		{
			t.w=t.waypoint[t.waypointindex].start;
			waypoint_delete ( );
		}
		t.entityelement[t.tupdatee].eleprof.trigger.waypointzoneindex=0;
	}
	t.tDontDeleteWPFlag = 0;

	// update infinilight list with removal
	if ( t.entityprofile[iWasEntID].ismarker == 2 || t.entityprofile[iWasEntID].ismarker == 5 || t.entityprofile[iWasEntID].entitylight )
	{
		//  refresh existing lights
		lighting_refreshorhold ( );
	}
	//Delete any particle effects.
	if (g_UndoSysObjectIsBeingMoved != true)
	{
		int iParticleEmitter = t.entityelement[t.tupdatee].eleprof.newparticle.emitterid;
		if (iParticleEmitter != -1)
		{
			gpup_deleteEffect(iParticleEmitter);
			t.entityelement[t.tupdatee].eleprof.newparticle.emitterid = -1;
		}
	}


	t.entityelement[t.tupdatee].eleprof.blendmode = 0;

	// remove flatten if any
	if (t.entityelement[t.tupdatee].eleprof.iFlattenID != -1)
	{
		GGTerrain_RemoveFlatArea(t.entityelement[t.tupdatee].eleprof.iFlattenID);
		t.entityelement[t.tupdatee].eleprof.iFlattenID = -1;
	}

	// If the object is actually being deleted (and not added to cursor) then we need to remove any references to this object from OTHER object visual logic connections
	if (g_UndoSysObjectIsBeingMoved != true)
	{
		void GetRelationshipObject(int iFindLinkID, int* piEntityID, int* piObj);
		constexpr int maxLogicConnections = 10;
		int deletedObjectLinkID = t.entityelement[t.tupdatee].eleprof.iObjectLinkID;
		for (int i = 0; i < maxLogicConnections; i++)
		{
			int otherObj = 0, otherEntID = 0;
			GetRelationshipObject(t.entityelement[t.tupdatee].eleprof.iObjectRelationships[i], &otherEntID, &otherObj);
			if (otherEntID > 0)
			{
				entityeleproftype& otherEnt = t.entityelement[otherEntID].eleprof;
				// Find the logic connection that the deleted entity has to another entity
				for (int j = 0; j < maxLogicConnections; j++)
				{
					// Delete the logic connection that the other object has to the object pending deletion
					if (otherEnt.iObjectRelationships[j] == deletedObjectLinkID)
					{
						otherEnt.iObjectRelationships[j] = 0;
						otherEnt.iObjectRelationshipsData[j] = 0;
						otherEnt.iObjectRelationshipsType[j] = 0;
						break;
					}
				}
			}
		}
	}

	// deleting and readding objects, MUST wipe out relational data!
	for (int i = 0; i < 10; i++)
	{
		t.entityelement[t.tupdatee].eleprof.iObjectLinkID = 0;
		t.entityelement[t.tupdatee].eleprof.iObjectRelationships[i] = 0;
		t.entityelement[t.tupdatee].eleprof.iObjectRelationshipsData[i] = 0;
		t.entityelement[t.tupdatee].eleprof.iObjectRelationshipsType[i] = 0;
	}

	// update real ent obj (.obj=0 inside)
	entity_updateentityobj ( );
}

// new improved system using master and event stacks
void entity_performtheundoaction (eUndoEventType eventtype, void* pEventData)
{
	// if event data
	if (pEventData)
	{
		// perform editor action to undo these events
		switch (eventtype)
		{
		case eUndoSys_Object_Add:
		{
			sUndoSysEventObjectAdd* pEvent = (sUndoSysEventObjectAdd*)pEventData;
			t.tentitytoselect = pEvent->iEntityElementIndex;
			entity_deleteentityfrommap();
			break;
		}
		case eUndoSys_Object_Delete:
		{
			sUndoSysEventObjectDelete* pEvent = (sUndoSysEventObjectDelete*)pEventData;
			int store = t.e;
			t.e = pEvent->e;

			//t.entid = t.gridentity;
			t.entid = pEvent->entitybankindex;
			t.gridentity = t.entid;

			//PE: Before adding to map , make sure to fillout all informations from master to t.grideleprof.
			entity_fillgrideleproffromprofile();

			t.grideleprof.trigger.waypointzoneindex = pEvent->grideleprof_trigger_waypointzoneindex;
			t.entitybankindex = t.entid;
			t.gridentityeditorfixed = pEvent->gridentityeditorfixed;
			t.entitymaintype = pEvent->entitymaintype;
			t.entitybankindex = pEvent->entitybankindex;
			t.gridentitystaticmode = pEvent->gridentitystaticmode;
			t.gridentityhasparent = pEvent->gridentityhasparent;
			t.gridentityposx_f = pEvent->gridentityposx_f;
			t.gridentityposy_f = pEvent->gridentityposy_f;
			t.gridentityposz_f = pEvent->gridentityposz_f;
			t.gridentityrotatex_f = pEvent->gridentityrotatex_f;
			t.gridentityrotatey_f = pEvent->gridentityrotatey_f;
			t.gridentityrotatez_f = pEvent->gridentityrotatez_f;
			t.gridentityrotatequatmode = pEvent->gridentityrotatequatmode;
			t.gridentityrotatequatx_f = pEvent->gridentityrotatequatx_f;
			t.gridentityrotatequaty_f = pEvent->gridentityrotatequaty_f;
			t.gridentityrotatequatz_f = pEvent->gridentityrotatequatz_f;
			t.gridentityrotatequatw_f = pEvent->gridentityrotatequatw_f;
			t.gridentityscalex_f = pEvent->gridentityscalex_f + 100;
			t.gridentityscaley_f = pEvent->gridentityscaley_f + 100;
			t.gridentityscalez_f = pEvent->gridentityscalez_f + 100;
			t.gridentityoverwritemode = t.e;
			entity_addentitytomap ();
			t.gridentityoverwritemode = 0;
			t.gridentity = 0;
			t.e = store;
			break;
		}
		case eUndoSys_Object_ChangePosRotScl:
		{
			sUndoSysEventObjectChangePosRotScl* pEvent = (sUndoSysEventObjectChangePosRotScl*)pEventData;
			int te = pEvent->e;
			t.entityelement[te].x = pEvent->posx_f;
			t.entityelement[te].y = pEvent->posy_f;
			t.entityelement[te].z = pEvent->posz_f;
			t.entityelement[te].rx = pEvent->rotatex_f;
			t.entityelement[te].ry = pEvent->rotatey_f;
			t.entityelement[te].rz = pEvent->rotatez_f;		
			t.entityelement[te].quatmode = pEvent->rotatequatmode;
			t.entityelement[te].quatx = pEvent->rotatequatx_f;
			t.entityelement[te].quaty = pEvent->rotatequaty_f;
			t.entityelement[te].quatz = pEvent->rotatequatz_f;
			t.entityelement[te].quatw = pEvent->rotatequatw_f;
			t.entityelement[te].scalex = pEvent->scalex_f;
			t.entityelement[te].scaley = pEvent->scaley_f;
			t.entityelement[te].scalez = pEvent->scalez_f;
			t.tobj = t.entityelement[te].obj;
			t.tte = te;
			entity_positionandscale();
			break;
		}
		case eUndoSys_Object_DeleteWaypoint:
		{
			sUndoSysEventObjectDeleteWaypoint* pEvent = (sUndoSysEventObjectDeleteWaypoint*)pEventData;
			waypoint_undoredo_add(pEvent->waypoint, pEvent->waypointcoords);
			break;
		}
		case eUndoSys_Object_Group:
		{
			sUndoSysEventObjectGroup* pEvent = (sUndoSysEventObjectGroup*)pEventData;

			extern void UnGroupUndoSys(int index);
			UnGroupUndoSys(pEvent->groupindex);

			break;
		}
		case eUndoSys_Object_UnGroup:
		{
			sUndoSysEventObjectUnGroup* pEvent = (sUndoSysEventObjectUnGroup*)pEventData;

			extern void GroupUndoSys(int index, std::vector<sRubberBandType> groupData);
			GroupUndoSys(pEvent->groupindex, pEvent->groupData);
			
			break;
		}
		}

		// and delete the event data (the action consumes the memory created to store the event data)
		delete pEventData;
		pEventData = NULL;
	}
}
void entity_createundoaction (int eventtype, int te, bool bUserAction)
{
	// if user caused this action manually, must clear redo stack which 
	// contains events from previous alternative future
	if (bUserAction == true) undosys_clearredostack();

	// add specific event to stacks
	switch (eventtype)
	{
	case eUndoSys_Object_Add:
	{
		undosys_object_add(te);
		break;
	}
	case eUndoSys_Object_Delete:
	{
		undosys_object_delete (te, t.entityelement[te].eleprof.trigger.waypointzoneindex,
			t.entityelement[te].editorfixed,
			t.entityelement[te].maintype,
			t.entityelement[te].bankindex,
			t.entityelement[te].staticflag,
			t.entityelement[te].iHasParentIndex,
			t.entityelement[te].x,
			t.entityelement[te].y,
			t.entityelement[te].z,
			t.entityelement[te].rx,
			t.entityelement[te].ry,
			t.entityelement[te].rz,
			t.entityelement[te].quatmode,
			t.entityelement[te].quatx,
			t.entityelement[te].quaty,
			t.entityelement[te].quatz,
			t.entityelement[te].quatw,
			t.entityelement[te].scalex,
			t.entityelement[te].scaley,
			t.entityelement[te].scalez);
		break;
	}
	case eUndoSys_Object_ChangePosRotScl:
	{
		undosys_object_changeposrotscl (te, t.entityelement[te].x,
			t.entityelement[te].y,
			t.entityelement[te].z,
			t.entityelement[te].rx,
			t.entityelement[te].ry,
			t.entityelement[te].rz,
			t.entityelement[te].quatmode,
			t.entityelement[te].quatx,
			t.entityelement[te].quaty,
			t.entityelement[te].quatz,
			t.entityelement[te].quatw,
			t.entityelement[te].scalex,
			t.entityelement[te].scaley,
			t.entityelement[te].scalez);
		break;
	}
	case eUndoSys_Object_DeleteWaypoint:
	{
		undosys_object_deletewaypoint(t.entityelement[te].eleprof.trigger.waypointzoneindex, te);
		break;
	}
	case eUndoSys_Object_Group:
	{
		
		undosys_object_group(te);
		break;
	}
	case eUndoSys_Object_UnGroup:
	{
		extern std::vector<sRubberBandType> vEntityGroupList[MAXGROUPSLISTS];
		undosys_object_ungroup(te, vEntityGroupList[te]);
		break;
	}
	}
}
void entity_createtheredoaction (eUndoEventType eventtype, void* pEventData)
{
	switch (eventtype)
	{
	case eUndoSys_Object_Add: 
	{
		// about to undo this add (the _addundo will delete the object), so we create a delete event in the redo stack
		sUndoSysEventObjectAdd* pEvent = (sUndoSysEventObjectAdd*)pEventData;
		int te = pEvent->iEntityElementIndex;
		if (t.entityelement[te].eleprof.trigger.waypointzoneindex > 0)
		{
			undosys_multiplevents_start();
			entity_createundoaction(eUndoSys_Object_Delete, te, false);
			entity_createundoaction(eUndoSys_Object_DeleteWaypoint, te, false);
			undosys_multiplevents_finish();
		}
		else
		{
			entity_createundoaction(eUndoSys_Object_Delete, te, false);
		}
		break;
	}
	case eUndoSys_Object_Delete: 
	{
		// about to undo this delete (which the _deleteundo will add the object back in), so we create an add event in the redo stack
		sUndoSysEventObjectDelete* pEvent = (sUndoSysEventObjectDelete*)pEventData;
		int te = pEvent->e;
		entity_createundoaction(eUndoSys_Object_Add, te, false);
		break;
	}
	case eUndoSys_Object_ChangePosRotScl: 
	{
		// about to undo this move event (which will repos/rot/scl back to what was stored), so we create a move event in the redo stack to put it back the other way
		sUndoSysEventObjectChangePosRotScl* pEvent = (sUndoSysEventObjectChangePosRotScl*)pEventData;
		int te = pEvent->e;
		entity_createundoaction(eUndoSys_Object_ChangePosRotScl, te, false);
		break;
	}
	case eUndoSys_Object_DeleteWaypoint:
	{
		// About to undo a delete, so create an add event in the redo stack.
		sUndoSysEventObjectDeleteWaypoint* pEvent = (sUndoSysEventObjectDeleteWaypoint*)pEventData;
		int te = pEvent->e;
		entity_createundoaction(eUndoSys_Object_Add, te, false);
		break;
	}
	case eUndoSys_Object_Group:
	{
		sUndoSysEventObjectGroup* pEvent = (sUndoSysEventObjectGroup*)pEventData;
		int index = pEvent->groupindex;
		entity_createundoaction(eUndoSys_Object_UnGroup, index, false);
		break;
	}
	case eUndoSys_Object_UnGroup:
	{
		sUndoSysEventObjectUnGroup* pEvent = (sUndoSysEventObjectUnGroup*)pEventData;
		entity_createundoaction(eUndoSys_Object_Group, pEvent->groupindex, false);
		break;
	}
	}
}
void entity_undo (void)
{
	// undo system to control undo stack
	undosys_undoevent();
}
void entity_redo (void)
{
	// undo system to control redo stack
	undosys_redoevent();
}

std::vector<int> delete_decal_particles;
void delete_notused_decal_particles( void )
{
	//PE: Cleanup all decals.
	for (int a = 0; a < MAXUNIQUEDECALS; a++)
	{
		for (int i = 0; i < MAXREADYDECALS; i++)
		{
			if (ready_decals[a][i] > 0)
			{
				void DeleteEmitterEffects(uint32_t root);
				DeleteEmitterEffects(ready_decals[a][i]);
				ready_decals[a][i] = 0;
				decal_count[a] = 0;
			}
		}
	}
	for (int i = 1; i <= g.decalelementmax; i++)
	{
		if (t.decalelement[i].active == 1)
		{
			int did = t.decalelement[i].decalid;

			if (did >= t.decal.size())
				continue;

			if (t.decal[did].newparticle.bWPE)
			{
				if (t.decalelement[i].newparticle.emitterid > 0)
				{
					int iParticleEffect = t.decalelement[i].newparticle.emitterid;
					delete_decal_particles.erase(std::remove(delete_decal_particles.begin(), delete_decal_particles.end(), iParticleEffect), delete_decal_particles.end());
					void DeleteEmitterEffects(uint32_t root);
					DeleteEmitterEffects(iParticleEffect);
					t.decalelement[i].newparticle.emitterid = -1;
				}
			}
		}
	}
	for (int i = 0; i < delete_decal_particles.size(); i++)
	{
		gpup_deleteEffect(delete_decal_particles[i]);
	}
	//PE: Clear all deleted particles.
	delete_decal_particles.clear();
}

