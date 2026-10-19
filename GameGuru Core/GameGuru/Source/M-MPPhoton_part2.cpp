void mp_subbedToItem ( void )
{
	for ( t.tloop = 0 ; t.tloop<=  20; t.tloop++ )
	{
		if (  t.mp_subbedItems[t.tloop]  ==  "" ) 
		{
			t.mp_subbedItems[t.tloop] = t.tlobbytring_s;
			break;
		}
	}
}

void mp_checkItemSubbed ( void )
{
	for ( t.tloop = 0 ; t.tloop<=  20; t.tloop++ )
	{
		if (  t.mp_subbedItems[t.tloop]  ==  t.tlobbytring_s && t.tlobbytring_s != "" ) 
		{
			if (  MAXTimer() - t.tsteaminstallingdotstime > 150 ) 
			{
				t.tsteaminstallingdotstime = MAXTimer();
				t.tsteamInstallingDots_s = t.tsteamInstallingDots_s + ".";
				if (  Len(t.tsteamInstallingDots_s.Get()) > 3  )  t.tsteamInstallingDots_s  =  "";
			}
			t.tlobbytring_s = t.tlobbytring_s + " - Installing." + t.tsteamInstallingDots_s;
			break;
		}
	}
}

void mp_resetGameStats ( void )
{
	PhotonResetFile ( );
	mp_nukeTestmap ( );
	cstr mlevel_s = g.mysystem.editorsGrideditAbs_s + "__multiplayerlevel__.fpm";
	if ( FileExist( mlevel_s.Get())  )  DeleteAFile ( mlevel_s.Get() );
	cstr mlevelworkshop_s = g.mysystem.editorsGrideditAbs_s + "__multiplayerworkshopitemid__.dat";
	if ( FileExist( mlevelworkshop_s.Get())  )  DeleteAFile ( mlevelworkshop_s.Get() );

	//  empty messages
	for ( t.tloop = 0 ; t.tloop<=  MP_MAX_CHAT_LINES-1; t.tloop++ )
	{
		t.mp_chat[t.tloop] = "";
	}

	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.entityelement[t.e].mp_networkkill = 0;
	}

	for ( t.tloop = 0 ; t.tloop<=  20; t.tloop++ )
	{
		t.mp_subbedItems[t.tloop] = "";
	}

	 if ( PhotonGetPlayerName() != NULL )
	 {
		g.mp.playerName = PhotonGetPlayerName();
		g.mp.playerID = 123;//PhotonGetPlayerID();
	 }

	g.mp.mode = MP_MODE_MAIN_MENU;
	g.mp.launchServer = 0;
	g.mp.maxHealth = 0;
	g.mp.isLobbyCreated = 0;
	g.mp.isServerCreated = 0;
	g.mp.isGameHost= 0;
	g.mp.voiceChatOn = 0;
	g.mp.lobbycount = 0;
	g.mp.lobbyscrollbarOn = 0;
	g.mp.gameAlreadySpawnedBefore = 0;
	g.mp.killedByPlayer = 0;
	g.mp.previousMessage_s = "";
	g.mp.syncedWithServer = 0;
	g.mp.syncedWithServerMode = 0;
	g.mp.onlySendMapToSpecificPlayer = -1;
	g.mp.oldtime = 0;
	g.mp.me = 0;
	g.mp.playedMyDeathAnim = 0;
	g.mp.fileLoaded = 0;
	g.mp.playGame = 0;
	g.mp.oldSpawnTimeLeft = 0;
	g.mp.respawnLeft = 0;
	g.mp.crouchOn = 0;
	g.mp.meleeOn = 0;
	g.mp.isAnimating = 0;
	g.mp.okayToLoadLevel = 0;
	g.mp.iHaveSaidIAmAlmostReady = 0;
	g.mp.attachmentcount = 0;
	g.mp.gunCount = 0;
	g.mp.gunid = 0;
	g.mp.lastSendTime = 0;
	g.mp.lastSendTimeAppearance = 0;
	g.mp.appearance = 0;
	g.mp.dyingTime = 0;
	g.mp.spawnrnd = -1;
	g.mp.reloading = 0;
	g.mp.syncedWithServer = 0;
	g.mp.sentreadytime = 0;
	g.mp.AttemptedToJoinLobbyTime = 0;
	g.mp.lastSendProjectileTime = 0;
	g.mp.dontApplyDamage = 0;
	g.mp.ragdollon = 0;
	g.mp.spectatorfollowdistance = 200.0;
	g.mp.ignoreDamageToEntity = 0;
	g.mp.endplay = 0;
	g.mp.destroyedObjectCount = 0;
	g.mp.message = "";
	g.mp.messageTime = 0;
	g.mp.oldfootfloortime = 0;
	g.mp.footfloor = 0;
	g.mp.resetcore = 0;
	g.mp.levelContainsCustomContent = 0;
	g.mp.workshopid = "0";
	g.mp.initialSpawnmoveDownCharacterFlag=1;
	g.mp.usersInServersLobbyAtServerCreation = 0;
	g.mp.dontDrawTitles = 0;
	g.mp.haveshowndeath = 0;
	g.mp.checkiflobbiesavailablemode = 0;
	g.mp.noplayermovement = 0;
	g.mp.team = 0;
	g.mp.friendlyfireoff = 0;
	g.mp.nameplatesOff = 0;
	g.mp.damageWasFromAI = 0;
	g.mp.haveSentMyAvatar = 0;
	g.mp.myOriginalSpawnPoint = -1;
	g.mp.realfirsttimespawn = 1;

	for ( t.tc = 0 ; t.tc<=  MP_MAX_NUMBER_OF_PLAYERS-1; t.tc++ )
	{
		t.mp_kills[t.tc] = 0;
		t.mp_deaths[t.tc] = 0;
		t.mp_reload[t.tc] = 0;
		t.mp_playerShooting[t.tc] = 0;
		t.mp_playerAttachmentIndex[t.tc] = 0;
		t.mp_playerIsRagdoll[t.tc] = 0;
		t.mp_playerAttachmentObject[t.tc] = 0;
		t.mp_playerHasSpawned[t.tc] = 0;
		t.mp_oldAppearance[t.tc] = 0;
		t.mp_playingAnimation[t.tc] = 0;
		t.mp_playingRagdoll[t.tc] = 0;
		t.mp_oldplayerx[t.tc] = 0;
		t.mp_oldplayery[t.tc] = 0;
		t.mp_oldplayerz[t.tc] = 0;
		t.mp_meleePlaying[t.tc] = 0;

		t.mp_isDying[t.tc] = 0;
		t.mp_jetpackOn[t.tc] = 0;
		t.mp_lobbies_s[t.tc] = "";
		t.mp_playerEntityID[t.tc] = 0;
		t.mp_forcePosition[t.tc] = 0;
		t.mp_health[t.tc] = 0;
		t.mp_lastIdleReset[t.tc] = 1;
		t.mp_jetpackparticles[t.tc] = -1;
		t.mp_joined[t.tc] = "";
	}

	// until spawn fully implemented, this triggers 1 second appearance of 'alive and existing' players
	for ( t.tc = 0 ; t.tc <= MP_MAX_NUMBER_OF_PLAYERS-1; t.tc++ )
	{
		t.mp_forcePosition[t.tc] = 1;
	}

	t.twhichteam = 1;
	for ( t.tc = 0 ; t.tc<=  MP_MAX_NUMBER_OF_PLAYERS-1; t.tc++ )
	{
		t.twhichteam = 1-t.twhichteam;
		t.mp_team[t.tc] = t.twhichteam;
	}

	for ( t.tc = 0 ; t.tc<=  99; t.tc++ )
	{
		t.mp_attachmentobjects[t.tc] = 0;
		t.mp_gunobj[t.tc] = 0;
		t.mp_gunname[t.tc] = "";
	}

	for ( t.tc = 0 ; t.tc<=  79; t.tc++ )
	{
		t.mp_bullets[t.tc].on = 0;
		t.mp_bullets[t.tc].particles = -1;
		t.mp_bullets[t.tc].sound = 0;
	}

	for ( t.i = 0 ; t.i<=  MP_RESPAWN_TIME_OBJECT_LIST_SIZE; t.i++ )
	{
		t.mp_respawn_timed[t.i].inuse = 0;
	}
	
	t.characterkitcontrol.showmyhead = 0;
}

void mp_update_all_projectiles ( void )
{
	t.debugHowManyInUse = 0;
	for ( t.tbulletloop = 0 ; t.tbulletloop<=  159; t.tbulletloop++ )
	{
			if (  SteamGetBulletOn(t.tbulletloop)  ==  0 ) 
			{
				//  clean up particles
				if (  t.mp_bullets[t.tbulletloop].particles  !=  -1 ) 
				{
						t.tRaveyParticlesEmitterID=t.mp_bullets[t.tbulletloop].particles;
						ravey_particles_delete_emitter ( );
						t.mp_bullets[t.tbulletloop].particles=-1;
				}
			}
			if (  t.tbulletloop < g.mp.me*20 || t.tbulletloop > (g.mp.me*20)+19 ) 
			{
				t.tSteamSoundID = g.steamsoundoffset+t.tbulletloop;
				if (  SoundExist(t.tSteamSoundID)  ==  1 ) 
				{
					if (  SoundLooping(t.tSteamSoundID)  ==  0 ) 
					{
						DeleteSound (  t.tSteamSoundID );
						t.mp_bullets[t.tbulletloop].sound = 0;
					}
				}
				t.tSteamSoundID = g.steamsoundoffset+200+t.tbulletloop;
				if (  SoundExist(t.tSteamSoundID)  ==  1 ) 
				{
					if (  SoundPlaying(t.tSteamSoundID)  ==  0  )  DeleteSound (  t.tSteamSoundID );
				}
				if (  SteamGetBulletOn(t.tbulletloop)  ==  1 ) 
				{
					++t.debugHowManyInUse;
					t.tsteamBObj = g.steamplayermodelsoffset+200+t.tbulletloop;
					t.mp_bullets[t.tbulletloop].btype = SteamGetBulletType(t.tbulletloop);
					if (  ObjectExist(t.tsteamBObj)  ==  0 ) 
					{
						t.tfindObj = t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].baseObj;
						if (  t.tfindObj  !=  0 ) 
						{
							CloneObject (  t.tsteamBObj, t.tfindObj );
						}
						else
						{
							MakeObjectBox (  t.tsteamBObj,20,20,20 );
						}

						// setup particle emitters for this projectile
						if ( t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].particleType > 0 )
						{
							ravey_particles_get_free_emitter ( );
							if ( t.tResult>0 ) 
							{
								t.tobj = t.tsteamBObj;
								weapon_add_projectile_particles ( );
								t.mp_bullets[t.tbulletloop].particles = t.tResult;
							}
						}

						//  setup sound
						t.mp_bullets[t.tbulletloop].sound = g.steamsoundoffset+t.tbulletloop;
						if (  SoundExist(t.mp_bullets[t.tbulletloop].sound) == 1 ) 
						{
							if (  SoundPlaying(t.mp_bullets[t.tbulletloop].sound)  ==  1  )  StopSound (  t.mp_bullets[t.tbulletloop].sound );
							if (  SoundLooping(t.mp_bullets[t.tbulletloop].sound)  ==  1  )  StopSound (  t.mp_bullets[t.tbulletloop].sound );
							DeleteSound (  t.mp_bullets[t.tbulletloop].sound );
						}
						//  if this projectile has a sound that loops, start it now
						if (  t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].sound  ==  0  )  t.mp_bullets[t.tbulletloop].sound  =  0;
						if (  t.mp_bullets[t.tbulletloop].sound > 0 ) 
						{
							CloneSound (  t.mp_bullets[t.tbulletloop].sound,t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].sound );
							if (  t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].soundLoopFlag  ==  1 ) 
							{
								PositionSound (  t.mp_bullets[t.tbulletloop].sound,SteamGetBulletX(t.tbulletloop), SteamGetBulletY(t.tbulletloop), SteamGetBulletZ(t.tbulletloop) );
								SetSoundSpeed (  t.mp_bullets[t.tbulletloop].sound, t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].soundDopplerBaseSpeed );
								LoopSound (  t.mp_bullets[t.tbulletloop].sound );
								if (  t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].soundDopplerFlag  ==  1 ) 
								{
									t.txDist_f = CameraPositionX(0) - SteamGetBulletX(t.tbulletloop);
									t.tyDist_f = CameraPositionY(0) - SteamGetBulletY(t.tbulletloop);
									t.tzDist_f = CameraPositionZ(0) - SteamGetBulletZ(t.tbulletloop);
									t.mp_bullets[t.tbulletloop].soundDistFromPlayer = Sqrt(t.txDist_f*t.txDist_f + t.tyDist_f*t.tyDist_f + t.tzDist_f*t.tzDist_f);
								}
							}
						}

					}
					if (  ObjectExist(t.tsteamBObj)  ==  1 ) 
					{
						PositionObject (  t.tsteamBObj, SteamGetBulletX(t.tbulletloop), SteamGetBulletY(t.tbulletloop), SteamGetBulletZ(t.tbulletloop) );
						RotateObject (  t.tsteamBObj, SteamGetBulletAngleX(t.tbulletloop), SteamGetBulletAngleY(t.tbulletloop), SteamGetBulletAngleZ(t.tbulletloop) );

						//  do we need to reposition the 3D sound?
						t.tSndID = t.mp_bullets[t.tbulletloop].sound;
						if (  t.tSndID > 0 ) 
						{
							if (  t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].soundLoopFlag  ==  1 ) 
							{
								//  position the sound
								PositionSound (  t.tSndID,SteamGetBulletX(t.tbulletloop), SteamGetBulletY(t.tbulletloop), SteamGetBulletZ(t.tbulletloop) );
								//  calculate and set doppler pitch
								if (  t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].soundDopplerFlag  ==  1 ) 
								{
									t.tOldDist_f = t.mp_bullets[t.tbulletloop].soundDistFromPlayer;
									t.txDist_f = CameraPositionX(0) - SteamGetBulletX(t.tbulletloop);
									t.tyDist_f = CameraPositionY(0) - SteamGetBulletY(t.tbulletloop);
									t.tzDist_f = CameraPositionZ(0) - SteamGetBulletZ(t.tbulletloop);
									t.mp_bullets[t.tbulletloop].soundDistFromPlayer = Sqrt(t.txDist_f*t.txDist_f + t.tyDist_f*t.tyDist_f + t.tzDist_f*t.tzDist_f);
									t.tDistDiff_f = t.tOldDist_f - t.mp_bullets[t.tbulletloop].soundDistFromPlayer;
									t.tSoundMultiplier_f = 1 + (t.tDistDiff_f/t.ElapsedTime_f)*0.00015;
									if (  t.tSoundMultiplier_f < 0.5  )  t.tSoundMultiplier_f  =  0.5;
									if (  t.tSoundMultiplier_f > 2  )  t.tSoundMultiplier_f  =  2;
									t.mp_bullets[t.tbulletloop].btype = SteamGetBulletType(t.tbulletloop);
									SetSoundSpeed (  t.tSndID, t.WeaponProjectileBase[t.mp_bullets[t.tbulletloop].btype].soundDopplerBaseSpeed * t.tSoundMultiplier_f );
								}
							}
						}

					}
				}
				else
				{
					//  projectile ended, show result

					//  clean up particles
					if (  t.mp_bullets[t.tbulletloop].particles  !=  -1 ) 
					{
							t.tRaveyParticlesEmitterID=t.mp_bullets[t.tbulletloop].particles;
							ravey_particles_delete_emitter ( );
							t.mp_bullets[t.tbulletloop].particles=-1;
					}

					if (  ObjectExist(g.steamplayermodelsoffset+200+t.tbulletloop)  ==  1 ) 
					{

						t.tSteamProjectileType = t.mp_bullets[t.tbulletloop].btype;

						//  stop any looping sound
						if (  t.mp_bullets[t.tbulletloop].sound > 0 ) 
						{
							StopSound (  t.mp_bullets[t.tbulletloop].sound );
							t.mp_bullets[t.tbulletloop].sound = 0;
						}

						t.tsteamBObj = g.steamplayermodelsoffset+200+t.tbulletloop;

						t.tDeathSoundSoundID = t.WeaponProjectileBase[t.tSteamProjectileType].soundDeath;
						if (  t.tDeathSoundSoundID > 0 ) 
						{
							t.tSteamSoundID = g.steamsoundoffset+200+t.tbulletloop;
							if (  SoundExist(t.tSteamSoundID)  ==  0 ) 
							{
								CloneSound (  t.tSteamSoundID,t.tDeathSoundSoundID );
							}
							PositionSound (  t.tSteamSoundID,ObjectPositionX(t.tsteamBObj),ObjectPositionY(t.tsteamBObj), ObjectPositionZ(t.tsteamBObj) );
							setsoundspeedtunedfor44k (  t.tSteamSoundID,38000 + Rnd(8000) );
							PlaySound (  t.tSteamSoundID );
						}

						t.texplodex_f=ObjectPositionX(t.tsteamBObj);
						t.texplodey_f=ObjectPositionY(t.tsteamBObj);
						t.texplodez_f=ObjectPositionZ(t.tsteamBObj);
						explosion_rocket(t.texplodex_f,t.texplodey_f,t.texplodez_f);
						DeleteObject (  t.tsteamBObj );
					}
				}
			}
	}
}

void mp_destroyentity ( void )
{
	//  takes ttte
	SteamDeleteObject (  t.ttte );
}

void mp_refresh ( void )
{
	 // handle transfer of host mapfile to joiners (needs to be in main update as host could be loading/waiting to press space)
	 if ( PhotonIsGameRunning() == 1 && g.mp.isGameHost == 1 && g.mp.okayToLoadLevel == 1 )
	 {
		 // only kicks in once server loading or waiting on 'press space' area, rest of time gameloop handles this call!
		 mp_hostalwaysreadytosendplayeramapfile();
	 }
	 // handle background network updates (so don't time out)
	 PhotonLoop (  );
}

int mp_closeconnection ( void )
{
	 return PhotonCloseConnection();
}

void mp_setMessage ( void )
{
	//  takes tmsg$
	g.mp.message = t.tmsg_s;
	g.mp.messageTime = MAXTimer();
}

void mp_setMessageDots ( void )
{
	//  takes tmsg$
	g.mp.messageDots = t.tmsg_s;
	g.mp.messageTimeDots = MAXTimer();
}

void mp_message ( void )
{
	if ( g.mp.message  !=  "" ) 
	{
		mp_text(-1,15,3,g.mp.message.Get());
		if ( MAXTimer() - g.mp.messageTime > MP_MESSAGE_TIMOUT ) 
		{
			g.mp.message = "";
		}
	}
}

void mp_messageDots ( void )
{
	if (  g.mp.messageDots  !=  "" ) 
	{
		mp_textDots(-1,15,3,g.mp.messageDots.Get());
		if (  MAXTimer() - g.mp.messageTimeDots > MP_MESSAGE_TIMOUT ) 
		{
			g.mp.messageDots = "";
		}
	}
return;

}

void mp_update_projectile ( void )
{
	if (  t.tProj > 19  )  return;

	t.tSteamBullet = (g.mp.me*20) + t.tProj;

	t.tTime = MAXTimer();
	if (  t.tTime - t.mp_bullets_send_time[t.tSteamBullet] < MP_PROJECTILE_UPDATE_DELAY && t.tSteamBulletOn  ==  1  )  return;

	t.mp_bullets_send_time[t.tSteamBullet] = t.tTime;
	
	t.mp_bullets[t.tSteamBullet].on = t.tSteamBulletOn;
	SteamSetBullet (  t.tSteamBullet , t.WeaponProjectile[t.tProj].xPos_f, t.WeaponProjectile[t.tProj].yPos_f, t.WeaponProjectile[t.tProj].zPos_f, t.WeaponProjectile[t.tProj].xAng_f, t.WeaponProjectile[t.tProj].yAng_f, t.WeaponProjectile[t.tProj].zAng_f, t.tProjType, t.tSteamBulletOn );

	return;
}

void mp_serverSetLuaGameMode ( void )
{
	SteamSendLua (  MP_LUA_ServerSetLuaGameMode,0,t.v );
return;

}

void mp_setServerTimer ( void )
{
	SteamSendLua (  MP_LUA_SetServerTimer,0,t.v );
return;

}

void mp_serverRespawnAll ( void )
{
}

void mp_restoreEntities ( void )
{
//  `remstart

	for ( t.te = 1 ; t.te<=  g.mp.originalEntitycount; t.te++ )
	{
		t.tentid=t.entityelement[t.te].bankindex;
		if (  t.tentid>0 ) 
		{
			if (  t.entityprofile[t.tentid].ischaracter  ==  1 || t.entityelement[t.te].mp_isLuaChar  ==  1 ) 
			{
				t.entityelement[t.te].x = t.steamStoreentityelement[t.te].x;
				t.entityelement[t.te].y = t.steamStoreentityelement[t.te].y;
				t.entityelement[t.te].z = t.steamStoreentityelement[t.te].z;
				ScaleObject (  t.entityelement[t.te].obj,100,100,100 );
				t.entityelement[t.te].health = 100;
				PositionObject (  t.entityelement[t.te].obj,t.entityelement[t.te].x,t.entityelement[t.te].y,t.entityelement[t.te].z );
				t.entityelement[t.te].mp_coopControlledByPlayer = -1;
				t.entityelement[t.te].mp_updateOn = 0;
				t.entityelement[t.te].active = 1;
				AISetEntityActive (  t.entityelement[t.te].obj,1 );
			}
		}
	}
}

void mp_serverEndPlay ( void )
{
	SteamSendLua (  MP_LUA_ServerEndPlay,0,0 );
	t.playercontrol.jetpackhidden=0;
	t.playercontrol.jetpackmode=0;
	physics_no_gun_zoom ( );
	g.mp.endplay = 1;
	t.aisystem.processplayerlogic = 0;
	g.autoloadgun=0 ; gun_change ( );
return;

}

void mp_setServerKillsToWin ( void )
{
	g.mp.setserverkillstowin = t.v;
return;

}

void mp_networkkill ( void )
{
	//  get damage amount to set it back to 0
	t.tdamage = SteamGetPlayerDamageAmount();
	t.tsteamlastdamageincounter = t.tsteamlastdamageincounter + 1;
	t.tsource = t.entityelement[t.texplodesourceEntity].mp_killedby;
	if ( t.mp_playerEntityID[t.tsource] > 0 )
	{
		t.te = t.mp_playerEntityID[t.tsource];
		g.mp.killedByPlayerFlag = 1;
		g.mp.playerThatKilledMe = t.tsource;
		t.tsteamforce = 500;
		SteamKilledBy (  g.mp.playerThatKilledMe , CameraPositionX(), CameraPositionY(), CameraPositionZ(), t.tsteamforce, 0 );
		g.mp.dyingTime = MAXTimer();
	}
}

void mp_lobbyListBox ( void )
{
	t.tluaTextCenterX = 0;
	if ( g.mp.listboxmode == 0 ) 
	{
		 t.tsize = PhotonGetLobbyListSize();
	}
	if ( g.mp.listboxmode == 1 ) 
	{
		t.tsize = t.tempsteamhowmanyfpmsarethere;
	}
	
	t.tTop_f = 20.0 * (GetDisplayHeight() / 100.0);
	t.tleft_f = 30.0 * (GetDisplayWidth() / 100.0);
	t.tBottom_f = 75.0 * (GetDisplayHeight() / 100.0);
	t.tright_f = 70.0 * (GetDisplayWidth() / 100.0);

	t.tTop = t.tTop_f;
	t.tLeft = t.tleft_f;
	t.tBottom = t.tBottom_f;
	t.tRight = t.tright_f;

	t.tempsteamyminY_f = (GetDisplayHeight() * 0.25) - (GetDisplayHeight() * 0.025);
	t.tempsteamymaxY_f = GetDisplayHeight() * 0.75;
	t.tempsteamyminX_f = GetDisplayWidth() * 0.30;
	t.tempsteamymaxX_f = GetDisplayWidth() * 0.65;
	t.tempsteamselected_f = g.mp.selectedLobby - g.mp.lobbyoffset;
	t.tempmissthisone_f = t.tempsteamselected_f;
	if (  t.tempsteamselected_f  >=  0 && t.tempsteamselected_f  <= 9 ) 
	{
		t.tempsteamselectedY_f = t.tempsteamyminY_f + (t.tempsteamselected_f * (GetDisplayHeight() * 0.05));
		InkEx ( 128, 128, 128 );
		BoxEx ( t.tLeft,t.tempsteamselectedY_f,t.tRight,t.tempsteamselectedY_f+(GetDisplayHeight() * 0.05) );
		if ( t.mc == 1 && t.tempsteamoldmc ==  0 )  g.mp.selectedLobby  =  t.tempsteamselected_f+g.mp.lobbyoffset;
	}

	t.tempsteamyminY_f = (GetDisplayHeight() * 0.25) - (GetDisplayHeight() * 0.025);
	t.tempsteamymaxY_f = GetDisplayHeight() * 0.75;
	t.tempsteamyminX_f = GetDisplayWidth() * 0.30;
	t.tempsteamymaxX_f = GetDisplayWidth() * 0.65;
	if (  t.mx  >=  t.tempsteamyminX_f && t.mx  <=  t.tempsteamymaxX_f ) 
	{
		if (  t.my  >=  t.tempsteamyminY_f && t.my  <=  t.tempsteamymaxY_f ) 
		{
			t.my_f = t.my;
			t.tempsteamselected_f = Floor((t.my_f - t.tempsteamyminY_f) / (GetDisplayHeight() * 0.05));
			if (  t.tempsteamselected_f  >=  0 && t.tempsteamselected_f < 10 && t.tempsteamselected_f  !=  t.tempmissthisone_f ) 
			{
				if (  t.tempsteamselected_f+g.mp.lobbyoffset < t.tsize ) 
				{
					t.tempsteamselectedY_f = t.tempsteamyminY_f + (t.tempsteamselected_f * (GetDisplayHeight() * 0.05));
					InkEx ( 64, 64, 64 );
					BoxEx (  t.tLeft,t.tempsteamselectedY_f,t.tRight,t.tempsteamselectedY_f+(GetDisplayHeight() * 0.05) );
					if (  t.mc  ==  1 && t.tempsteamoldmc  ==  0  )  
					{
						// do not allow selection if game file and too large
						bool bAllowItemToBeHighlighted = true;
						if ( g.mp.listboxmode == 1 ) 
						{
							int iSizeOfFPMFile = atoi(t.tfpmfilesizelist_s[t.tempsteamselected_f+g.mp.lobbyoffset].Get());
							if ( iSizeOfFPMFile < 100 )
							{
								// this is allowed to be selected
							}
							else
							{
								// for now, files 100MB or over are not allowed
								bAllowItemToBeHighlighted = false;
							}
						}

						// select this item from the list
						if ( bAllowItemToBeHighlighted == true )
						{
							g.mp.selectedLobby  =  t.tempsteamselected_f+g.mp.lobbyoffset;
						}
					}
				}
			}
		}
	}

	InkEx ( 255, 255, 255 );
	LineEx (  t.tLeft,t.tTop,t.tRight,t.tTop );
	LineEx (  t.tLeft,t.tTop,t.tLeft,t.tBottom );
	LineEx (  t.tLeft,t.tBottom,t.tRight,t.tBottom );
	LineEx (  t.tRight,t.tTop,t.tRight,t.tBottom );

	t.toffx_f = (1.0 * GetDisplayWidth()) / 100.0;
	t.toffx = t.toffx_f;
	InkEx ( 30, 30, 30 );
	BoxEx (  t.tRight-(t.toffx*2)-8,t.tTop+1,t.tRight-1,t.tBottom-1 );

	t.tTop += 4;
	t.tBottom -= 4;
	t.tRight -= 4;

	t.toffx_f = (1.0 * GetDisplayWidth()) / 100.0;
	t.toffx = t.toffx_f;
	t.toffy_f = (1.0 * GetDisplayHeight()) / 100.0;
	t.toffy = t.toffy_f;

	InkEx ( 255, 255, 255 );
	if (  t.mx > t.tLeft && t.mx < t.tRight ) 
	{
		if (  t.my > t.tTop && t.my < t.tTop+(t.toffy_f*2) ) 
		{
			InkEx ( 128, 128, 128 );
			if (  t.mc  ==  1 ) 
			{
				if (  MAXTimer() - t.tempsteamscrollclicktimer > 100 ) 
				{
					--g.mp.lobbyoffset;
					t.tempsteamscrollclicktimer = MAXTimer();
				}
			}
		}
	}

	LineEx (  t.tRight-t.toffx,t.tTop,t.tRight-(t.toffx*2), t.tTop+(t.toffy*2) );
	LineEx (  t.tRight-t.toffx,t.tTop,t.tRight, t.tTop+(t.toffy*2) );
	LineEx (  t.tRight-(t.toffx*2),t.tTop+(t.toffy*2),t.tRight,t.tTop+(t.toffy*2) );

	InkEx ( 255, 255, 255 );// Rgb (255,255,255),0 ) ;
	if (  t.mx > t.tLeft && t.mx < t.tRight ) 
	{
		if (  t.my > t.tBottom-(t.toffy*2) && t.my < t.tBottom ) 
		{
			InkEx ( 128, 128, 128 );
			if (  t.mc  ==  1 ) 
			{
				if (  MAXTimer() - t.tempsteamscrollclicktimer > 100 ) 
				{
					++g.mp.lobbyoffset;
					t.tempsteamscrollclicktimer = MAXTimer();
				}
			}
		}
	}

	LineEx (  t.tRight-t.toffx,t.tBottom,t.tRight-(t.toffx*2), t.tBottom-(t.toffy*2) );
	LineEx (  t.tRight-t.toffx,t.tBottom,t.tRight, t.tBottom-(t.toffy*2) );
	LineEx (  t.tRight-(t.toffx*2),t.tBottom-(t.toffy*2),t.tRight,t.tBottom-(t.toffy*2) );

	if (  g.mp.lobbyscrollbarOn  ==  0 || t.mc  ==  0 ) 
	{
		g.mp.lobbyscrollbarOn = 0;
		t.tboxsize_f = (10.0 * GetDisplayHeight()) / 100.0;
		t.tboxsize = t.tboxsize_f;
		t.tloboffset_f = g.mp.lobbyoffset;
		t.tsize_f = t.tsize;
		t.tboxoffset_f = (t.tloboffset_f / t.tsize_f) * 100.0;
		if (  t.tboxoffset_f < 0.0  )  t.tboxoffset_f  =  0.0;
		if (  t.tboxoffset_f > 100.0  )  t.tboxoffset_f  =  100.0;
		t.tboxoffset_f = (t.tboxoffset_f * (GetDisplayHeight() * 0.42)) / 100.0;
		t.tboxoffset = t.tboxoffset_f;
		t.tboxtop = t.tTop+(t.toffy*2) + t.tboxoffset + 2;
		InkEx ( 255, 255, 255 );
		if (  t.mx > t.tRight-(t.toffx*2) && t.mx < t.tRight ) 
		{
			if (  t.my > t.tboxtop && t.my < t.tboxtop+t.tboxsize ) 
			{
				InkEx ( 160, 160, 160 );
				if (  t.mc  ==  1 && t.tempsteamoldmc  ==  0 ) 
				{
					g.mp.lobbyscrollbarOn = 1;
					g.mp.lobbyscrollbarOffset = t.tboxtop-t.my;
					g.mp.lobbyscrollbarOldY = t.my;
				}
			}
		}
		BoxEx (  t.tRight-(t.toffx*2),t.tboxtop,t.tRight,t.tboxtop+t.tboxsize );
	}
	else
	{
		g.mp.lobbyscrollbarOldY = t.my;
		t.tboxtop = t.my+g.mp.lobbyscrollbarOffset;
		if (  t.tboxtop < t.tTop+(t.toffy*2)+2  )  t.tboxtop  =  t.tTop+(t.toffy*2)+2;
		if (  t.tboxtop > t.tTop+(t.toffy*2)+2 + ((100.0 * (GetDisplayHeight() * 0.40)) / 100.0)  )  t.tboxtop  =  t.tTop+(t.toffy*2)+2 + ((100.0 * (GetDisplayHeight() * 0.40)) / 100.0);
		t.tboxsize_f = (10.0 * GetDisplayHeight()) / 100.0;
		t.tboxsize = t.tboxsize_f;
		InkEx ( 160, 160, 160 );
		BoxEx (  t.tRight-(t.toffx*2),t.tboxtop,t.tRight,t.tboxtop+t.tboxsize );

		// update the list to reflect where the bar is
		t.tboxtop_f = t.tboxtop - (t.tTop+(t.toffy*2)+2);
		t.tempsteamperc_f = (t.tboxtop_f / (GetDisplayHeight() * 0.42)) * 100.0;
		if (  t.tempsteamperc_f < 0.0  )  t.tempsteamperc_f  =  0.0;
		if (  t.tempsteamperc_f > 100.0  )  t.tempsteamperc_f  =  100.0;
		t.tsize_f = t.tsize;
		t.tempsteamnewoffset_f = (t.tempsteamperc_f * t.tsize_f) / 100.0;
		g.mp.lobbyoffset = t.tempsteamnewoffset_f;

	}

	if (  g.mp.lobbyoffset > t.tsize-10  )  g.mp.lobbyoffset  =  t.tsize-10;
	if (  g.mp.lobbyoffset < 0  )  g.mp.lobbyoffset  =  0;

	InkEx ( 255, 255, 255 );

	t.tempsteamoldmc = t.mc;

	// in Photon, lobbies are actuall rooms (essentially game rooms)
	 LPSTR pLobbyWord = "level";
	 LPSTR pLobbiesWord = "levels";

	if (  g.mp.listboxmode  ==  0 ) 
	{
		if (  t.tsize  ==  1 ) 
		{
			t.tstring_s = cstr("1 ")+pLobbyWord+" found";
		}
		else
		{
			t.tstring_s = cstr(cstr(Str(t.tsize)) + " "+pLobbiesWord+" found");
		}
	}
	if (  g.mp.listboxmode  ==  1 ) 
	{
		t.tstring_s = cstr(cstr(Str(t.tsize)) + " levels found");
	}
	mp_text(-1,15,1,t.tstring_s.Get());

	if (  t.tsize > 0  )  g.mp.lobbycount  =  t.tsize;
	t.teampsteamy = 25;
	t.tlobbycount = 0;
	for ( t.c = 0 ; t.c<=  t.tsize-1; t.c++ )
	{
		if (  t.c  >=  g.mp.lobbyoffset && t.c < g.mp.lobbyoffset+10 ) 
		{
			if (  g.mp.listboxmode  ==  0 ) 
			{
				 t.mp_lobbies_s[t.tlobbycount] = PhotonGetLobbyListName(t.c);
				if ( cstr(Left(t.mp_lobbies_s[t.tlobbycount].Get(),5)) == "Lobby" || Len(t.mp_lobbies_s[t.tlobbycount].Get()) < 8 ) 
				{
					t.mp_lobbies_s[t.tlobbycount] = "Waiting for details...";
				}
				if ( g.mp.selectedLobby ==  t.c )  g.mp.selectedLobbyName = t.mp_lobbies_s[t.tlobbycount];

				t.tempMPLobbyNameFromList_s = t.mp_lobbies_s[t.tlobbycount];
				mp_canIJoinThisLobby ( );
				t.tsteamstring_s = g.mp.lobbyjoinedname;
				if ( t.tsteamcanjoinlobby == 1 ) 
				{
					t.tr = 255;
					t.tg = 255;
					t.tb = 255;
				}
				else
				{
					if ( t.tsteamcanjoinlobby == 2 ) 
					{
						t.tr = 255;
						t.tg = 100;
						t.tb = 100;
					}
					else
					{
						t.tr = 255;
						t.tg = 255;
						t.tb = 50;
					}
				}
			}
			if ( g.mp.listboxmode == 1 ) 
			{
				t.mp_lobbies_s[t.tlobbycount] = t.tfpmfilelist_s[t.c];
				LPSTR pExtra = "";
				int iSizeOfFPMFile = atoi(t.tfpmfilesizelist_s[t.c].Get());
				if ( iSizeOfFPMFile < 100 )
				{
					t.tr = 255;
					t.tg = 255;
					t.tb = 255;
				}
				else
				{
					t.tr = 255;
					t.tg = 255;
					t.tb = 128;
					pExtra = " (too large to host)";
				}
				t.tsteamstring_s = t.mp_lobbies_s[t.tlobbycount] + " (" + t.tfpmfilesizelist_s[t.c] + "MB)" + pExtra;
			}

			t.tlobbytring_s = t.tsteamstring_s;
			if ( t.tsteamcanjoinlobby == 0 ) mp_checkItemSubbed ( );
			mp_textColor(32,t.teampsteamy,1,t.tlobbytring_s.Get(),t.tr,t.tg,t.tb);
			t.teampsteamy += 5;
			++t.tlobbycount;
		}
	}
}

void mp_createLobby ( void )
{
	// warning flag if start game on own
	g.mp.haveToldAboutSolo = 0;

	// get players lobby label
	 t.tempsteamhostlobbyname_s = cstr(PhotonGetPlayerName()) + cstr(":");// + cstr("'s Lobby:");

	// get level name
	if ( g.mp.fpmpicked == "Level I just worked on" ) 
	{
		 t.tempsteamlevelname_s = ""; // redundant as done above!
	}
	else
	{
		t.tempsteamlevelname_s = cstr(Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-4)) + cstr(":");
	}

	// get map name
	if ( g.mp.fpmpicked == "Level I just worked on" ) 
	{
		//t.tempsteammaptocheck_s = g.fpscrootdir_s+"\\Files\\mapbank\\worklevel.dat";
		t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s + "worklevel.dat";		
	}
	else
	{
		//t.tempsteammaptocheck_s = g.fpscrootdir_s+"\\Files\\mapbank\\"+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";
		t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";
	}

	// set unique lobbylevel name and create lobby/gameroom
	 PhotonSetLobbyName ( cstr(t.tempsteamhostlobbyname_s+t.tempsteamlevelname_s).Get() );
	 PhotonCreateLobby();

	// mark as host and wait for creation to succeed
	g.mp.isGameHost = 1;
	g.mp.mode = MP_WAITING_FOR_LOBBY_CREATION;
	t.tempsteamlobbycreationtimeout = MAXTimer();
}

void mp_searchForLobbies ( void )
{
	SteamGetLobbyList (  );
	g.mp.mode = MP_MODE_LOBBY;
	g.mp.isGameHost = 0;
}

void mp_searchForFpms ( void )
{
	g.mp.mode = MP_SERVER_CHOOSING_FPM_TO_USE;
	t.told_s=GetDir();
	//SetDir (  cstr(g.fpscrootdir_s + "\\Files\\mapbank").Get() );
	SetDir ( g.mysystem.mapbankAbs_s.Get() );
	ChecklistForFiles (  );
	Dim ( t.tfpmfilelist_s, ChecklistQuantity( ) );
	Dim ( t.tfpmfilesizelist_s, ChecklistQuantity( ) );
	t.tempsteamhowmanyfpmsarethere = 0;
	for ( t.c = 1 ; t.c<=  ChecklistQuantity(); t.c++ )
	{
		if (  ChecklistValueA(t.c) == 0 ) 
		{
			t.tfile_s=ChecklistString(t.c);
			if (  t.tfile_s != "." && t.tfile_s != ".." ) 
			{
				if (  cstr(Lower(Right(t.tfile_s.Get(),4)))  ==  ".fpm" ) 
				{
					t.tfpmfilelist_s[t.tempsteamhowmanyfpmsarethere] = t.tfile_s;
					DWORD filesize = 0;
					HANDLE hfile = GG_CreateFile(t.tfile_s.Get(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
					if ( hfile != INVALID_HANDLE_VALUE )
					{
						filesize = GetFileSize(hfile, NULL);	
						CloseHandle(hfile);
					}
					int iMBSize = (int)(filesize/1024/1024); if ( iMBSize < 1 ) iMBSize = 1;
					t.tfpmfilesizelist_s[t.tempsteamhowmanyfpmsarethere] = cstr(Str(iMBSize));
					++t.tempsteamhowmanyfpmsarethere;
				}
			}
		}
	}
	SetDir (  t.told_s.Get() );
}

void mp_launchGame ( void )
{
	g.mp.launchServer = 1;
}

void mp_restartMultiplayerSystem ( void )
{
	// after 4 seconds, get out of this infinite loop of no none-connection!
	DWORD dwTimer = timeGetTime();
	while ( mp_closeconnection() == 0 && timeGetTime() < dwTimer+4000 )
	{
		 PhotonLoop();
	}
	mp_fullinit();
}

void mp_backToStart ( void )
{
	mp_resetGameStats ( );
}

void mp_selectedALevel ( void )
{
	cstr mlevel_s = g.mysystem.editorsGrideditAbs_s + "__multiplayerlevel__.fpm";
	if ( FileExist( mlevel_s.Get())  ) DeleteAFile ( mlevel_s.Get() );
	g.mp.fpmpicked = t.tfpmfilelist_s[g.mp.selectedLobby];
	mp_checkIfLevelHasCustomContent ( );
	if (  g.mp.fpmpicked  ==  "Level I just worked on" ) 
	{
		cstr worklevel_s = g.mysystem.editorsGrideditAbs_s + "worklevel.fpm";
		CopyAFile ( worklevel_s.Get(),mlevel_s.Get() );
	}
	else
	{
		CopyAFile ( cstr(g.mysystem.mapbankAbs_s+g.mp.fpmpicked).Get(), cstr(g.fpscrootdir_s+"\\Files\\editors\\gridedit\\__multiplayerlevel__.fpm").Get() );
	}
	if (  g.mp.levelContainsCustomContent  ==  1 ) 
	{
		//  first we check if the changed flag is set (they have saved since hosting) if not, we dont need to upload to steam
		if (  g.mp.fpmpicked  ==  "Level I just worked on" ) 
		{
			t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s + "worklevel.dat";
		}
		else
		{
			t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";
		}

		t.tmphopitemtocheckifchangedandversion_s = t.tempsteammaptocheck_s;
		mp_grabWorkshopChangedFlagAndVersion ( );
		g.mp.workshopItemChangedFlag = t.tMPshopHasItemChangedFlag;
		if (  t.tMPshopHasItemChangedFlag  ==  1 ) 
		{
			g.mp.mode = MP_SERVER_CHOOSING_TO_MAKE_FPS_WORKSHOP;
		}
		else
		{
			g.mp.workshopid = t.tMPshopTheIDNumber_s;
		}
	}
}

void mp_checkIfLevelHasCustomContent ( void )
{

	if (  g.mp.fpmpicked  ==  "Level I just worked on" ) 
	{
		t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+"worklevel.dat";
	}
	else
	{
		t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";
	}

	if (  FileExist(t.tempsteammaptocheck_s.Get()) ) 
	{
		g.mp.levelContainsCustomContent = 1;
	}
	else
	{
		g.mp.levelContainsCustomContent = 0;
	}

return;

}

void mp_buildWorkShopItem ( void )
{
	g.mp.mode = MP_CREATING_WORKSHOP_ITEM;
	switch (  g.mp.buildingWorkshopItemMode ) 
	{
		case 0:
			if (  PathExist( cstr(g.fpscrootdir_s+"\\Files\\editors\\workshopItem").Get() )  ==  0 ) 
			{
				MakeDirectory ( cstr(g.fpscrootdir_s+"\\Files\\editors\\workshopItem").Get() );
			}
			else
			{
				t.told_s=GetDir();
				SetDir (  cstr(g.fpscrootdir_s + "\\Files\\editors\\workshopItem").Get() );
				ChecklistForFiles (  );

				for ( t.c = 1 ; t.c<=  ChecklistQuantity(); t.c++ )
				{
					if (  ChecklistValueA(t.c) == 0 ) 
					{
						t.tfile_s=ChecklistString(t.c);
						if (  t.tfile_s != "." && t.tfile_s != ".." ) 
						{
							DeleteAFile (  t.tfile_s.Get() );
						}
					}
				}
				SetDir (  t.told_s.Get() );
			}

			if (  FileOpen(1)  ==  1  )  CloseFile (  1 );

			if (  g.mp.fpmpicked  ==  "Level I just worked on" ) 
			{
				t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+"worklevel.dat";//g.fpscrootdir_s+"\\Files\\mapbank\\worklevel.dat";
				t.tempsteamleveltocopy_s = g.mysystem.mapbankAbs_s+"worklevel.fpm";//g.fpscrootdir_s+"\\Files\\mapbank\\worklevel.fpm";
			}
			else
			{
				t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";//g.fpscrootdir_s+"\\Files\\mapbank\\"+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";
				t.tempsteamleveltocopy_s = g.mysystem.mapbankAbs_s+g.mp.fpmpicked;//g.fpscrootdir_s+"\\Files\\mapbank\\" + g.mp.fpmpicked;
			}

			t.tempsteamdestworkshopfolder_s = g.fpscrootdir_s + "\\Files\\editors\\workshopItem\\";
			CopyAFile (  t.tempsteamleveltocopy_s.Get(), cstr(t.tempsteamdestworkshopfolder_s + "editors_gridedit___multiplayerlevel__.fpm").Get() );
			//  grab clanged flag and version number (we dont need the changed flag here tho)
			t.tmphopitemtocheckifchangedandversion_s = t.tempsteammaptocheck_s;
			mp_grabWorkshopChangedFlagAndVersion ( );

			OpenToRead (  1,t.tempsteammaptocheck_s.Get() );
			g.mp.buildingWorkshopItemMode = 1;
			//  skip over the warning, change flag, version number and workshop id
			t.tempsteamstring_s = ReadString ( 1 );
			t.tempsteamstring_s = ReadString ( 1 );
			t.tempsteamstring_s = ReadString ( 1 );
			t.tempsteamstring_s = ReadString ( 1 );
			//  skip over the fpm in the file
			t.tempsteamstring_s = ReadString ( 1 );

		break;
//   ``````````````````````````````````

		case 1:
			t.tempsteamdestworkshopfolder_s = g.fpscrootdir_s + "\\Files\\editors\\workshopItem\\";
			//  Write out version number
			if (  FileOpen(2)  )  CloseFile (  2 );
			OpenToWrite (  2, cstr(t.tempsteamdestworkshopfolder_s+"version.dat").Get() );
			WriteString (  2,Str(t.tMPshopTheVersionNumber) );
			CloseFile (  2 );

			t.tempsteamstring_s = ReadString ( 1 );
			t.tempMPshopfilename_s = "";
			for ( t.c = 1 ; t.c<=  Len(t.tempsteamstring_s.Get()); t.c++ )
			{
				if (  cstr(Mid(t.tempsteamstring_s.Get(),t.c))  ==  "\\" || cstr(Mid(t.tempsteamstring_s.Get(),t.c))  ==  "/" ) 
				{
					t.tempMPshopfilename_s=t.tempMPshopfilename_s+"_";
				}
				else
				{
					if (  cstr(Mid(t.tempsteamstring_s.Get(),t.c))  ==  "_" ) 
					{
						t.tempMPshopfilename_s=t.tempMPshopfilename_s+"@";
					}
					else
					{
						t.tempMPshopfilename_s=t.tempMPshopfilename_s+Mid(t.tempsteamstring_s.Get(),t.c);
					}
				}
			}

			if (  t.tempsteamstring_s  !=  "" ) 
			{
				CopyAFile (  cstr(g.fpscrootdir_s+"\\Files\\"+t.tempsteamstring_s).Get() , cstr(t.tempsteamdestworkshopfolder_s + t.tempMPshopfilename_s).Get() );
				t.tsteamyesencrypt = 0;
				//  models
				if (  cstr(Right(t.tempMPshopfilename_s.Get(),4))  ==  ".dbo"  )  t.tsteamyesencrypt  =  1;
				if (  cstr(Right(t.tempMPshopfilename_s.Get(),2))  ==  ".x"  )  t.tsteamyesencrypt  =  1;
				//  images
				if (  cstr(Right(t.tempMPshopfilename_s.Get(),4))  ==  ".png"  )  t.tsteamyesencrypt  =  1;
				if ( cstr( Right(t.tempMPshopfilename_s.Get(),4))  ==  ".jpg"  )  t.tsteamyesencrypt  =  1;
				if (  cstr(Right(t.tempMPshopfilename_s.Get(),4))  ==  ".dds"  )  t.tsteamyesencrypt  =  1;
				if (  cstr(Right(t.tempMPshopfilename_s.Get(),4))  ==  ".bmp"  )  t.tsteamyesencrypt  =  1;
				//  sounds and music
				if ( cstr( Right(t.tempMPshopfilename_s.Get(),4))  ==  ".wav"  )  t.tsteamyesencrypt  =  1;
				if ( cstr( Right(t.tempMPshopfilename_s.Get(),4))  ==  ".mp3"  )  t.tsteamyesencrypt  =  1;
				if ( cstr( Right(t.tempMPshopfilename_s.Get(),4) ) ==  ".ogg"  )  t.tsteamyesencrypt  =  1;
				//  scripts
				if (  cstr(Right(t.tempMPshopfilename_s.Get(),4))  ==  ".fpe"  )  t.tsteamyesencrypt  =  1;

				if (  t.tsteamyesencrypt  ==  1 ) 
				{
					EncryptWorkshopDBPro ( cstr(t.tempsteamdestworkshopfolder_s + t.tempMPshopfilename_s).Get() );
					DeleteAFile (  cstr(t.tempsteamdestworkshopfolder_s + t.tempMPshopfilename_s).Get() );
				}
			}
			if (  FileEnd(1) ) 
			{
				g.mp.buildingWorkshopItemMode = 2;
			}
		break;
//   ``````````````````````````````````

		case 2:

			if (  g.mp.fpmpicked  ==  "Level I just worked on" ) 
			{
				t.tempMPshopname_s = "My Level";
			}
			else
			{
				t.tempMPshopname_s = Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-4);
			}

			SteamSetRoot( cstr( g.fpscrootdir_s+"\\Files\\" ).Get() );
			SteamCreateWorkshopItem (  t.tempMPshopname_s.Get() );

			g.mp.buildingWorkshopItemMode = 3;
		break;
//   ``````````````````````````````````

		case 3:
			if (  SteamIsWorkshopItemUploaded()  ==  1 ) 
			{
				//  set changed flag to 0
				if (  g.mp.fpmpicked  ==  "Level I just worked on" ) 
				{
					t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+"worklevel.dat";
				}
				else
				{
					t.tempsteammaptocheck_s = g.mysystem.mapbankAbs_s+Left(g.mp.fpmpicked.Get(),Len(g.mp.fpmpicked.Get())-3)+"dat";
				}
				if (  FileOpen(1)  ==  1  )  CloseFile (  1 );
				//  Work out how many lines there are so we can Dim (  the right amount )
				t.thowmanylines = 0;
				OpenToRead (  1,t.tempsteammaptocheck_s.Get() );
					while (  FileEnd(1) == 0 ) 
					{
						t.tthrowawaystring_s = ReadString ( 1 );
						++t.thowmanylines;
					}
				CloseFile (  1 );
				////  now read in the whole thing
				Dim (  t.templines,t.thowmanylines );
				t.thowmanylines = 0;
				OpenToRead (  1,t.tempsteammaptocheck_s.Get() );
					while (  FileEnd(1) == 0 ) 
					{
						t.templines[t.thowmanylines] = ReadString ( 1 );
						++t.thowmanylines;
					}
				CloseFile (  1 );
				DeleteAFile (  t.tempsteammaptocheck_s.Get() );
				OpenToWrite (  1,t.tempsteammaptocheck_s.Get() );
				g.mp.workshopid = SteamGetWorkshopID();

				t.templines[1] = "0";
				t.templines[3] = g.mp.workshopid;
				for ( t.tloop = 0 ; t.tloop<=  t.thowmanylines-1; t.tloop++ )
				{
					WriteString (  1,t.templines[t.tloop].Get() );
				}
				CloseFile (  1 );

				//  get rid of the array
				UnDim (  t.templines );

				g.mp.buildingWorkshopItemMode = 99;
			}

			if (  SteamIsWorkshopItemUploaded()  ==  -1  )  g.mp.buildingWorkshopItemMode  =  98;
		break;
	}//~ return
return;

}

void mp_buildingWorkshopItemFailed ( void )
{
	t.tsteamlostconnectioncustommessage_s = "Could not build item (Error MP015)";
	mp_lostConnection ( );
	g.mp.mode = MP_SERVER_CHOOSING_FPM_TO_USE;
}

void mp_joinALobby ( void )
{
	t.a = g.mp.selectedLobby;
	if ( g.mp.selectedLobbyName != "Getting Lobby details..." ) 
	{
		g.mp.lobbyjoinedname = g.mp.selectedLobbyName;
		 // No workshop in Photon - just join!
		 PhotonJoinLobby(g.mp.lobbyjoinedname.Get());

		g.mp.mode = MP_JOINING_LOBBY;
		g.mp.oldtime = MAXTimer();
		t.tsteamwaitedforlobbytimer = MAXTimer();
		g.mp.AttemptedToJoinLobbyTime = MAXTimer();
		g.mp.lobbycount = 0;
	}
}

void mp_canIJoinThisLobby ( void )
{
	if ( g.mp.selectedLobbyName != "Getting Lobby details..." ) 
	{
		g.mp.lobbyjoinedname = t.tempMPLobbyNameFromList_s;
		 // Sitename
		 cstr tempstringsitename_s = "";
		// t.tempsteamgotto = 0;
		 for ( t.tc = 1 ; t.tc <= Len(g.mp.lobbyjoinedname.Get()); t.tc++ )
		 {
			//++t.tempsteamgotto;
			if ( cstr(Mid(g.mp.lobbyjoinedname.Get(),t.tc) ) ==  ":" ) { break; } //t.tempsteamgotto += 2 ; break; }
			tempstringsitename_s = tempstringsitename_s + Mid(g.mp.lobbyjoinedname.Get(),t.tc);
		 }
		 // Lobby User Name
		 t.tempsteamstringlobbyname_s = "";
		 t.tempsteamfoundone = 0;
		 for ( t.tc = 1 ; t.tc <= Len(g.mp.lobbyjoinedname.Get()); t.tc++ )
		 {
			if ( cstr(Mid(g.mp.lobbyjoinedname.Get(),t.tc)) == ":" )
			{
				++t.tempsteamfoundone;
			}
			else
			{
				if ( t.tempsteamfoundone == 1 ) 
				{
					t.tempsteamstringlobbyname_s = t.tempsteamstringlobbyname_s + Mid(g.mp.lobbyjoinedname.Get(),t.tc);
				}
			}
		 }
		 // Unique Lobby User ID
		 cstr userUniqueID_s = "";
		 t.tempsteamfoundone = 0;
		 for ( t.tc = 1 ; t.tc <= Len(g.mp.lobbyjoinedname.Get()); t.tc++ )
		 {
			if ( cstr(Mid(g.mp.lobbyjoinedname.Get(),t.tc)) == ":" )
			{
				++t.tempsteamfoundone;
			}
			else
			{
				if ( t.tempsteamfoundone == 2 ) 
				{
					userUniqueID_s = userUniqueID_s + Mid(g.mp.lobbyjoinedname.Get(),t.tc);
				}
			}
		 }
		 // level name
		 g.mp.levelnametojoin = "";
		 t.tempsteamfoundone = 0;
		 for ( t.tc = 1 ; t.tc <= Len(g.mp.lobbyjoinedname.Get()); t.tc++ )
		 {
			if ( cstr(Mid(g.mp.lobbyjoinedname.Get(),t.tc)) == ":" )
			{
				++t.tempsteamfoundone;
			}
			else
			{
				if ( t.tempsteamfoundone == 3 ) 
				{
					g.mp.levelnametojoin = g.mp.levelnametojoin + Mid(g.mp.lobbyjoinedname.Get(),t.tc);
				}
			}
		 }
		 // Assemble name for display
		 g.mp.lobbyjoinedname = t.tempsteamstringlobbyname_s + "'s ";
		 g.mp.lobbyjoinedname = g.mp.lobbyjoinedname + g.mp.levelnametojoin + " Level";

		g.mp.workshopidtojoin = "";
		 // No workshop or versioning in Photon
		 t.tsteamcanjoinlobby = 1;
	}
	else
	{
		t.tsteamcanjoinlobby = 0;
	}
}

void mp_leaveALobby ( void )
{
	 PhotonLeaveLobby (  );
	mp_resetGameStats ( );
}

void mp_SubscribeToWorkShopItem ( void )
{
	t.tlobbytring_s = g.mp.lobbyjoinedname;
	mp_subbedToItem ( );
	g.mp.mode = MP_ASKING_IF_SUBSCRIBE_TO_WORKSHOP_ITEM_WAITING_FOR_RESULTS;
}

void mp_save_workshop_files_needed ( void )
{
	cstr toriginalMasterLevelFile_s = "";
	cstr toriginalprojectname_s = "";

	toriginalMasterLevelFile_s = t.tmasterlevelfile_s;
	toriginalprojectname_s = g.projectfilename_s;
	//  If there is no baseList.dat file we cant proceed
	if (  FileExist("editors\\baseList.dat")  ==  0  )  return;

	//  Work out how many lines there are so we can Dim (  the right amount )
	t.thowmanyfpefiles = 0;
	OpenToRead (  1,"editors\\baseList.dat" );
	while (  FileEnd(1) == 0 ) 
	{
		t.tthrowawaystring_s = ReadString ( 1 );
		++t.thowmanyfpefiles;
	}
	CloseFile (  1 );

	//  Store the count in our global steamworks type
	g.mp.howmanyfpefiles = t.thowmanyfpefiles;

	Dim (  t.tallfpefiles_s,t.thowmanyfpefiles  );
	t.thowmanyfpefiles = 0;
	OpenToRead (  1,"editors\\baseList.dat" );
	while (  FileEnd(1) == 0 ) 
	{
		t.tallfpefiles_s[t.thowmanyfpefiles] = ReadString ( 1 );
		++t.thowmanyfpefiles;
	}
	CloseFile (  1 );

	t.exename_s=t.tsteamsavefilename_s;
	if (  cstr(Lower(Right(t.exename_s.Get(),4))) == ".fpm" ) 
	{
		t.exename_s=Left(t.exename_s.Get(),Len(t.exename_s.Get())-4);
	}
	for ( t.n = Len(t.exename_s.Get()) ; t.n >= 1 ; t.n+= -1 )
	{
		if (  cstr(Mid(t.exename_s.Get(),t.n)) == "\\" || cstr(Mid(t.exename_s.Get(),t.n)) == "/" ) 
		{
			t.exename_s=Right(t.exename_s.Get(),Len(t.exename_s.Get())-t.n);
			break;
		}
	}
	if (  Len(t.exename_s.Get())<1  )  t.exename_s = "sample";

	//  the level
	t.tmasterlevelfile_s=cstr("mapbank\\")+t.exename_s+".fpm";
	t.strwork = "" ; t.strwork = t.strwork + "Saving required files list for "+ t.tmasterlevelfile_s;
	timestampactivity(0, t.strwork.Get() );

	//  Get absolute My Games folder
	g.exedir_s="?";
	t.told_s=GetDir();
	t.tworkshoplistfile_s = t.told_s+"\\mapbank\\" + t.exename_s + ".dat";
	t.tMPshopTheVersionNumber = 1;
	if (  FileExist(t.tworkshoplistfile_s.Get())  ==  1 ) 
	{
		t.tmphopitemtocheckifchangedandversion_s = t.tworkshoplistfile_s;
		mp_grabWorkshopChangedFlagAndVersion ( );
		++t.tMPshopTheVersionNumber;
		DeleteAFile (  t.tworkshoplistfile_s.Get() );
	}
	OpenToWrite (  1,t.tworkshoplistfile_s.Get() );

	//  set the changed flag since we are saving, this way we dont rely on workshop info to know if a file is new or not
	WriteString (  1,"DO NOT MANUALY EDIT THIS FILE" );
	WriteString (  1,"1" );
	WriteString (  1,Str(t.tMPshopTheVersionNumber) );
	WriteString (  1,"0" );

	//  Collect ALL files in string array list
	g.filecollectionmax = 0;
	Dim (  t.filecollection_s,500  );

	//  include original FPM
	addtocollection(t.tmasterlevelfile_s.Get());

	//  Stage 2 - collect all files
	t.tlevelfile_s="";
	g.projectfilename_s=t.tmasterlevelfile_s;

	//  load in level FPM
	if (  Len(t.tlevelfile_s.Get())>1 ) 
	{
		g.projectfilename_s=t.tlevelfile_s;
	}

	//  chosen sky, terrain and veg
	addfoldertocollection( cstr(cstr("skybank\\")+t.skybank_s[g.skyindex]).Get() );
	addfoldertocollection("skybank\\night");
	// reduce large custom files needed for FPM and Level Cloud (faster down/loading)
	#ifdef ENABLECUSTOMTERRAIN
	 if ( stricmp ( g.terrainstyle_s.Get(), "CUSTOM" ) != NULL )
	 {
		addfoldertocollection( cstr(cstr("terrainbank\\")+g.terrainstyle_s).Get() );
	 }
	#endif

	//  choose all entities and associated files
	for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
	{
		t.entid=t.entityelement[t.e].bankindex;
		if (  t.entid>0 ) 
		{
			//  check for lua scripts
			if (  t.entityelement[t.e].eleprof.aimain_s  !=  "" ) 
			{
				if (  mp_check_if_entity_is_from_install(t.entityelement[t.e].eleprof.aimain_s.Get())  ==  0 ) 
				{
					addtocollection( cstr(cstr("scriptbank\\")+t.entityelement[t.e].eleprof.aimain_s).Get() );
				}
			}
			//  entity profile file
			t.tentityname1_s=cstr("entitybank\\")+t.entitybank_s[t.entid];
			t.tentityname2_s=cstr(Left(t.tentityname1_s.Get(),Len(t.tentityname1_s.Get())-4))+".fpe";
			if (  FileExist( cstr(g.fpscrootdir_s+"\\Files\\"+t.tentityname2_s).Get() ) == 1 ) 
			{
				t.tentityname_s=t.tentityname2_s;
			}
			else
			{
				t.tentityname_s=t.tentityname1_s;
			}
			//  Check to see if the entity is part of the base install
			//  If it is, we can skip checking any further with it
			if (  mp_check_if_entity_is_from_install(t.tentityname_s.Get())  ==  0 ) 
			{

				addtocollection(t.tentityname_s.Get());
				//  entity files in folder
				t.tentityfolder_s=t.tentityname_s;
				for ( t.n = Len(t.tentityname_s.Get()) ; t.n >=  1 ; t.n+= -1 )
				{
					if (  cstr(Mid(t.tentityname_s.Get(),t.n)) == "\\" || cstr(Mid(t.tentityname_s.Get(),t.n)) == "/" ) 
					{
						t.tentityfolder_s=Left(t.tentityfolder_s.Get(),t.n);
						break;
					}
				}
				//  model file
				t.tlocaltofpe=1;
				for ( t.n = 1 ; t.n<=  Len(t.entityprofile[t.entid].model_s.Get()); t.n++ )
				{
					if (  cstr(Mid(t.entityprofile[t.entid].model_s.Get(),t.n)) == "\\" || cstr(Mid(t.entityprofile[t.entid].model_s.Get(),t.n)) == "/" ) 
					{
						t.tlocaltofpe=0 ; break;
					}
				}
				if (  t.tlocaltofpe == 1 ) 
				{
					t.tfile1_s=t.tentityfolder_s+t.entityprofile[t.entid].model_s;
				}
				else
				{
					t.tfile1_s=t.entityprofile[t.entid].model_s;
				}
				t.tfile2_s=cstr(Left(t.tfile1_s.Get(),Len(t.tfile1_s.Get())-2))+".dbo";
				if (  FileExist( cstr(g.fpscrootdir_s+"\\Files\\"+t.tfile2_s).Get() ) == 1 ) 
				{
					t.tfile_s=t.tfile2_s;
				}
				else
				{
					t.tfile_s=t.tfile1_s;
				}
				t.tmodelfile_s=t.tfile_s;
				addtocollection(t.tmodelfile_s.Get());
				//  entity characterpose file (if any)
				t.tfile3_s=cstr(Left(t.tfile1_s.Get(),Len(t.tfile1_s.Get())-2))+".dat";
				if (  FileExist(cstr(g.fpscrootdir_s+"\\Files\\"+t.tfile3_s).Get()) == 1 ) 
				{
					addtocollection(t.tfile3_s.Get());
				}

				//  texture files
				t.tlocaltofpe=1;
				for ( t.n = 1 ; t.n<=  Len(t.entityelement[t.e].eleprof.texd_s.Get()); t.n++ )
				{
					if (  cstr(Mid(t.entityelement[t.e].eleprof.texd_s.Get(),t.n)) == "\\" || cstr(Mid(t.entityelement[t.e].eleprof.texd_s.Get(),t.n)) == "/" ) 
					{
						t.tlocaltofpe=0 ; break;
					}
				}
				if (  t.tlocaltofpe == 1 ) 
				{
					t.tfile_s=t.tentityfolder_s+t.entityelement[t.e].eleprof.texd_s;
				}
				else
				{
					t.tfile_s=t.entityelement[t.e].eleprof.texd_s;
				}
				addtocollection(t.tfile_s.Get());
				timestampactivity(0, cstr(cstr("Exporting ")+t.entitybank_s[t.entid]+" texd:"+t.tfile_s).Get() );
				if (  cstr(Left(Lower(Right(t.tfile_s.Get(),6)),2)) == "_d" ) 
				{
					t.tfileext_s=Right(t.tfile_s.Get(),3);
					t.tfile_s=cstr(Left(t.tfile_s.Get(),Len(t.tfile_s.Get())-6))+"_n."+t.tfileext_s ; addtocollection(t.tfile_s.Get());
					t.tfile_s=cstr(Left(t.tfile_s.Get(),Len(t.tfile_s.Get())-6))+"_s."+t.tfileext_s ; addtocollection(t.tfile_s.Get());
					t.tfile_s=cstr(Left(t.tfile_s.Get(),Len(t.tfile_s.Get())-6))+"_i."+t.tfileext_s ; addtocollection(t.tfile_s.Get());
					t.tfile_s=cstr(Left(t.tfile_s.Get(),Len(t.tfile_s.Get())-6))+"_o."+t.tfileext_s ; addtocollection(t.tfile_s.Get());
				}
				if (  t.tlocaltofpe == 1 ) 
				{
					t.tfile_s=t.tentityfolder_s+t.entityelement[t.e].eleprof.texaltd_s;
				}
				else
				{
					t.tfile_s=t.entityelement[t.e].eleprof.texaltd_s;
				}
				addtocollection(t.tfile_s.Get());
				//  if entity did not specify texture it is multi-texture, so interogate model file
				findalltexturesinmodelfile(t.tmodelfile_s.Get(),t.tentityfolder_s.Get(),t.entityprofile[t.entityelement[t.e].bankindex].texpath_s.Get());
				//  shader file
				t.tfile_s=t.entityelement[t.e].eleprof.effect_s ; addtocollection(t.tfile_s.Get());
				//  script files
				t.tfile_s=cstr("scriptbank\\")+t.entityelement[t.e].eleprof.aimain_s ; addtocollection(t.tfile_s.Get());
				//  sound files
				t.tfile_s = t.entityelement[t.e].eleprof.soundset_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.soundset1_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.soundset2_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.soundset3_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.soundset5_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.soundset6_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.soundset4a_s; addtocollection(t.tfile_s.Get());
				t.tfile_s = t.entityelement[t.e].eleprof.overrideanimset_s; addtocollection(t.tfile_s.Get());

				// lipsync files associated with soundset references
				cstr tmpFile_s = t.entityelement[t.e].eleprof.soundset_s; tmpFile_s = cstr(Left( tmpFile_s.Get(), strlen(tmpFile_s.Get())-4)) + ".lip"; addtocollection(tmpFile_s.Get());
				tmpFile_s = t.entityelement[t.e].eleprof.soundset1_s; tmpFile_s = cstr(Left( tmpFile_s.Get(), strlen(tmpFile_s.Get())-4)) + ".lip"; addtocollection(tmpFile_s.Get());
				tmpFile_s = t.entityelement[t.e].eleprof.soundset2_s; tmpFile_s = cstr(Left( tmpFile_s.Get(), strlen(tmpFile_s.Get())-4)) + ".lip"; addtocollection(tmpFile_s.Get());
				tmpFile_s = t.entityelement[t.e].eleprof.soundset3_s; tmpFile_s = cstr(Left( tmpFile_s.Get(), strlen(tmpFile_s.Get())-4)) + ".lip"; addtocollection(tmpFile_s.Get());

				//  collectable guns
				if (  Len(t.entityprofile[t.entid].isweapon_s.Get())>1 ) 
				{
					t.tfile_s=cstr("gamecore\\guns\\")+t.entityprofile[t.entid].isweapon_s ; addfoldertocollection(t.tfile_s.Get());
					t.foundgunid=t.entityprofile[t.entid].isweapon;
					if (  t.foundgunid>0 ) 
					{
						for ( t.x = 0 ; t.x<=  1; t.x++ )
						{
							t.tpoolindex=g.firemodes[t.foundgunid][t.x].settings.poolindex;
							if (  t.tpoolindex>0 ) 
							{
								t.tfile_s=cstr("gamecore\\ammo\\")+t.ammopool[t.tpoolindex].name_s ; addfoldertocollection(t.tfile_s.Get());
							}
						}
					}
				}
				//  associated guns and ammo
				if (  Len(t.entityelement[t.e].eleprof.hasweapon_s.Get())>1 ) 
				{
					t.tfile_s=cstr("gamecore\\guns\\")+t.entityelement[t.e].eleprof.hasweapon_s ; addfoldertocollection(t.tfile_s.Get());
					t.foundgunid=t.entityelement[t.e].eleprof.hasweapon;
					if (  t.foundgunid>0 ) 
					{
						for ( t.x = 0 ; t.x<=  1; t.x++ )
						{
							t.tpoolindex=g.firemodes[t.foundgunid][t.x].settings.poolindex;
							if (  t.tpoolindex>0 ) 
							{
								t.tfile_s=cstr("gamecore\\ammo\\")+t.ammopool[t.tpoolindex].name_s ; addfoldertocollection(t.tfile_s.Get());
							}
						}
					}
				}
				//  zone marker can reference other levels to jump to
				if (  t.entityprofile[t.entid].ismarker == 3 ) 
				{
					t.tlevelfile_s=t.entityelement[t.e].eleprof.ifused_s;
					if (  Len(t.tlevelfile_s.Get())>1 ) 
					{
						t.tlevelfile_s=cstr("mapbank\\")+t.tlevelfile_s+".fpm";
						addtocollection(t.tlevelfile_s.Get());
					}
				}
			}

		}
	}

	// fill in the .dat file
	SetDir ( cstr(g.fpscrootdir_s+"\\Files\\").Get() );
	t.thowmanyadded = 0;
	t.filesmax = g.filecollectionmax;
	for ( t.fileindex = 1 ; t.fileindex <= t.filesmax; t.fileindex++ )
	{
		t.name_s=t.filecollection_s[t.fileindex];
		if (  cstr(Left(t.name_s.Get(),12))  ==  "entitybank\\\\"  )  t.name_s  =  cstr("entitybank\\") + Right(t.name_s.Get(), Len(t.name_s.Get())-12);
		if (  cstr(Left(t.name_s.Get(),12))  ==  "scriptbank\\\\"  )  t.name_s  =  cstr("scriptbank\\") + Right(t.name_s.Get(), Len(t.name_s.Get())-12);
		if (  FileExist(t.name_s.Get()) == 1 ) 
		{
			if (  mp_check_if_entity_is_from_install(t.name_s.Get())  ==  0 ) 
			{
				WriteString (  1,t.name_s.Get() );
				++t.thowmanyadded;

				//  09032015 - 017 - If its a gun, grab the muzzleflash, decals and include them
				if (  cstr(Right(t.name_s.Get(),11))  ==  "gunspec.txt" ) 
				{
					if (  FileOpen(3)  )  CloseFile (  3 );
					t.tfoundflash = 0;
					OpenToRead (  3,t.name_s.Get() );
					t.tfoundflash = 0;
					while (  FileEnd(3)  ==  0 && t.tfoundflash  ==  0 ) 
					{
						t.tisthisflash_s = ReadString ( 3 );
						if (  cstr(Left(t.tisthisflash_s.Get(),11))  ==  "muzzleflash" ) 
						{
							t.tlocationofequals = FindLastChar(t.tisthisflash_s.Get(),"=");
							if (  t.tlocationofequals > 1 ) 
							{
								if (  cstr(Mid(t.tisthisflash_s.Get(),t.tlocationofequals+1))  ==  " " ) 
								{
									t.tflash_s = Right(t.tisthisflash_s.Get(),Len(t.tisthisflash_s.Get())-(t.tlocationofequals+1));
								}
								else
								{
									t.tflash_s = Right(t.tisthisflash_s.Get(),Len(t.tisthisflash_s.Get())-(t.tlocationofequals));
								}
								t.tfext_s = "";
								if (  FileExist( cstr(cstr("gamecore\\muzzleflash\\flash")+t.tflash_s+".png").Get() )  ==  1  )  t.tfext_s  =  ".png";
								if (  FileExist( cstr(cstr("gamecore\\muzzleflash\\flash")+t.tflash_s+".dds").Get() )  ==  1  )  t.tfext_s  =  ".dds";
								if (  t.tfext_s  !=  "" ) 
								{
									WriteString (  1,cstr(cstr("gamecore\\muzzleflash\\flash")+t.tflash_s+t.tfext_s).Get() );
									++t.thowmanyadded;
								}
								t.tfext_s = "";
								if (  FileExist(cstr(cstr("gamecore\\decals\\muzzleflash")+t.tflash_s+"\\decal.png").Get() )  ==  1  )  t.tfext_s  =  ".png";
								if (  FileExist(cstr(cstr("gamecore\\decals\\muzzleflash")+t.tflash_s+"\\decal.dds").Get() )  ==  1  )  t.tfext_s  =  ".dds";
								if (  t.tfext_s  !=  "" ) 
								{
									WriteString (  1,cstr(cstr("gamecore\\decals\\muzzleflash")+t.tflash_s+"\\decal"+t.tfext_s).Get() );
									++t.thowmanyadded;
								}
								t.tfext_s = "";
								if (  FileExist(cstr(cstr("gamecore\\decals\\muzzleflash")+t.tflash_s+"\\decalspec.txt").Get() )  ==  1  )  t.tfext_s  =  ".txt";
								if (  t.tfext_s  !=  "" ) 
								{
									WriteString (  1,cstr(cstr("gamecore\\decals\\muzzleflash")+t.tflash_s+"\\decalspec"+t.tfext_s).Get() );
									++t.thowmanyadded;
								}
								t.tfoundflash = 1;
							}
						}
					}
					CloseFile (  3 );
				}
			}
		}
	}
	CloseFile (  1 );

	//  if (  it is just the fpm  )  there are is no custom media with this level
	if (  t.thowmanyadded  <=  1  )  DeleteAFile (  t.tworkshoplistfile_s.Get() );

	//  cleanup file array
	UnDim (  t.filecollection_s );

	//  Restore directory
	SetDir (  t.told_s.Get() );

	UnDim (  t.tallfpefiles );

	t.tmasterlevelfile_s = toriginalMasterLevelFile_s;
	g.projectfilename_s = toriginalprojectname_s;

}

void mp_grabWorkshopChangedFlagAndVersion ( void )
{
	if (  FileExist(t.tmphopitemtocheckifchangedandversion_s.Get())  ==  1 ) 
	{
		if (  FileOpen(1)  )  CloseFile (  1 );
		OpenToRead (  1,t.tmphopitemtocheckifchangedandversion_s.Get() );
		//  skip the warning message
		t.tnothing_s = ReadString ( 1 );
		//  read in flag changed
		t.tnothing_s = ReadString ( 1 );
		t.tMPshopHasItemChangedFlag = ValF(t.tnothing_s.Get());
		//  read in version number
		t.tnothing_s = ReadString ( 1 );
		t.tMPshopTheVersionNumber = ValF(t.tnothing_s.Get());
		//  read in workshop id
		t.tnothing_s = ReadString ( 1 );
		t.tMPshopTheIDNumber_s = t.tnothing_s;
		CloseFile (  1 );
	}
return;

//  Check to see if this file is part of the base install
}

int mp_check_if_entity_is_from_install ( char* name_s )
{
	int ttemploop = 0;
	int ttresult = 0;
	ttresult = 0;
	if (  cstr(Left(name_s,12))  ==  "entitybank\\\\"  )  strcpy ( name_s  , cstr(cstr("entitybank\\") + Right(name_s, Len(name_s)-12)).Get() );
	if (  cstr(Right(name_s,3))  ==  "bin" ) 
	{
		strcpy ( name_s , cstr(cstr(Lower(Left(name_s,Len(name_s)-3))) + cstr("fpe")).Get() );
	}
	else
	{
		name_s = Lower(name_s);
	}
	cstr nameCheck = cstr(name_s);
	for ( ttemploop = 0 ; ttemploop<=  g.mp.howmanyfpefiles-1; ttemploop++ )
	{
		cstr listFile = cstr( _strlwr(t.tallfpefiles_s[ttemploop].Get()) );
		if (  nameCheck  ==  listFile ) 
		{
			ttresult = 1;
			break;
		}
	}
	return ttresult;
}

void mp_resetSteam ( void )
{
	mp_free ( );
	mp_init ( );
	mp_resetGameStats ( );
	g.mp.needToResetOnStartup = 0;
}

void mp_shoot ( void )
{
	if (  t.weaponammo[g.weaponammoindex+g.ammooffset]>0 ) 
	{
		SteamShoot (  );
	}
}

void mp_chat ( void )
{

	//  check for chat
	t.tchat_s = SteamGetChat();
	if (  t.tchat_s  !=  "" ) 
	{
		mp_chatNew ( );
	}

	t.tscancode = ScanCode();
	if (  KeyState(g.keymap[28])  ==  1 && t.oldchatscancode  !=  28 ) 
	{
		g.mp.chaton = 1-g.mp.chaton;
		if (  g.mp.chaton  ==  1 ) 
		{
			//  start a new chat message
			ClearEntryBuffer (  );
			g.mp.chatstring = "";
		}
		else
		{
			//  send the chat message
			//  local send
			if (  Len(g.mp.chatstring.Get())  ==  1 ) 
			{
				if (  Asc(Mid(g.mp.chatstring.Get(),1))  <=  31  )  g.mp.chatstring  =  "";
			}
			else
			{
				if (  Asc(Mid(g.mp.chatstring.Get(),1))  <=  31  )  g.mp.chatstring  =  Right(g.mp.chatstring.Get(),Len(g.mp.chatstring.Get())-1);
			}
			if (  g.mp.chatstring !=  "" ) 
			{
				t.tchat_s = cstr(cstr(Str(g.mp.me)) + Left( cstr(cstr("<") + SteamGetPlayerName() + "> " + g.mp.chatstring).Get(),80)).Get();
				mp_chatNew ( );
				g.mp.chatstring = "";
				if (  t.tchatLobbyMode  ==  0 ) 
				{
					SteamSendChat (  t.tchat_s.Get() );
				}
				else
				{
					//  030315 - 013 - Lobby chat
					SteamSendLobbyChat (  t.tchat_s.Get() );
				}
			}
		}
	}
	if (  g.mp.chaton  ==  1 ) 
	{
		if (  MAXTimer() - g.mp.lastSpawnedTime > 1000 ) 
		{
			t.aisystem.processplayerlogic=0;
		}
		else
		{
			t.aisystem.processplayerlogic=1;
		}
		g.mp.chattimer = MAXTimer();

		g.mp.chatstring = Entry(1);
		//  show the Text (  we have typed )
		if (  MAXTimer() - t.chatcursortime > 250 ) 
		{
			t.chatcursortime = MAXTimer();
			g.mp.cursoron = 1-g.mp.cursoron;
		}
		if (  g.mp.cursoron  ==  0 ) 
		{
			t.tstringtoshow_s = cstr(Left(cstr(cstr("<") + SteamGetPlayerName() + "> " + g.mp.chatstring).Get(),80));
		}
		else
		{
			t.tstringtoshow_s = cstr(Left(cstr(cstr("<") + SteamGetPlayerName() + "> " + g.mp.chatstring).Get(),80)) + cstr("|");
		}
	}
	else
	{
		t.aisystem.processplayerlogic=1;
	}
	t.oldchatscancode = t.tscancode;

	if (  MAXTimer() - g.mp.chattimer  <=  MP_CHAT_DELAY+2550 ) 
	{
		t.ttimegone = MAXTimer()-t.toldchattime;
		if (  t.ttimegone > 50 ) 
		{
			t.toldchattime = MAXTimer();
			t.ttimegone = 16;
		}
		t.toldchattime = MAXTimer();
		if (  MAXTimer() - g.mp.chattimer  >=  MP_CHAT_DELAY ) 
		{
			t.tsteamalpha = t.tsteamalpha - t.ttimegone;
		}
		else
		{
			t.tsteamalpha = t.tsteamalpha + t.ttimegone;
		}
		if (  t.tsteamalpha < 0  )  t.tsteamalpha  =  0;
		if (  t.tsteamalpha > 255  )  t.tsteamalpha  =  255;
		
		if (  t.tsteamalpha > 0 ) 
		{
			InkEx ( 20, 20, 20 );//  Rgb(20,20,20),Rgb(20,20,20) );
			BoxEx (  5,5,(40*10)+5,((MP_MAX_CHAT_LINES+1)*15)+10 );
		}

		t.tsteamy = 10;
		for ( t.tloop = 0 ; t.tloop<=  MP_MAX_CHAT_LINES-1; t.tloop++ )
		{
			if (  t.mp_chat[t.tloop] != "" ) 
			{
				if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "0" ) { t.r  =  255 ; t.g  =  255 ; t.b  =  50; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "1" ) { t.r  =  255  ; t.g  =  100 ; t.b  =  100; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "2" ) { t.r  =  100  ; t.g  =  255 ; t.b  =  100; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "3" ) { t.r  =  100  ; t.g  =  100 ; t.b  =  255; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "4" ) { t.r  =  255  ; t.g  =  255 ; t.b  =  100; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "5" ) { t.r  =  255  ; t.g  =  100 ; t.b  =  255; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "6" ) { t.r  =  100  ; t.g  =  255 ; t.b  =  255; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "7" ) { t.r  =  200  ; t.g  =  255 ; t.b  =  200; }
				else if (  cstr(Left(t.mp_chat[t.tloop].Get(),1))  ==  "s" ) { t.r  =  255  ; t.g  =  255 ; t.b  =  255; }
				t.tluarealcoords = 1;
				t.tluatextalpha = t.tsteamalpha;
				mp_textColor(10,t.tsteamy,2,Right(t.mp_chat[t.tloop].Get(),Len(t.mp_chat[t.tloop].Get())-1),t.r,t.g,t.b);
			}
			t.tsteamy += 14;
		}

		if (  g.mp.chaton  ==  1 ) 
		{
			t.tluarealcoords = 1;
			t.tluatextalpha = t.tsteamalpha;
			t.tsteamy = 10+((MP_MAX_CHAT_LINES)*15);
			mp_textColor(10,t.tsteamy,2,t.tstringtoshow_s.Get(),255,255,255);
		}
	}
	InkEx ( 255, 255, 255 );// Rgb(255,255,255),Rgb(0,0,0) );
}

void mp_chatNew ( void )
{
	for ( t.tloop = 0 ; t.tloop<=  MP_MAX_CHAT_LINES-2; t.tloop++ )
	{
		t.mp_chat[t.tloop] = t.mp_chat[t.tloop+1];
	}
	if (  Len(t.tchat_s.Get()) > 80  )  t.tchat_s  =  Left(t.tchat_s.Get(),80);
	if (  cstr(Left(t.tchat_s.Get(),1))  !=  "s" ) 
	{
		t.mp_chat[MP_MAX_CHAT_LINES-1] = t.tchat_s;
		g.mp.chattimer = MAXTimer();
	}
	//  200315 - 021 - pick up users joining the game from the server message sent
	if (  cstr(Left(t.tchat_s.Get(),1))  ==  "s" ) 
	{
		t.tnametocheckforjoining_s = Right(t.tchat_s.Get(),Len(t.tchat_s.Get())-1);
		for ( t.tn = 0 ; t.tn<=  MP_MAX_NUMBER_OF_PLAYERS-1; t.tn++ )
		{
			if (  cstr(Left(t.tnametocheckforjoining_s.Get(),Len(t.mp_joined[t.tn].Get())))  ==  t.mp_joined[t.tn] && t.mp_joined[t.tn]  !=  "" && cstr(Right(t.mp_joined[t.tn].Get(),6))  !=  "Joined" ) 
			{
				t.mp_joined[t.tn] = t.mp_joined[t.tn] + " - Joined";
			}
		}
	}
}

void mp_quitGame ( void )
{
	//  if the server, let everyone know instantly the server is dropping
	//  020315 - 012 - Send an end game message when the host decides to leave
	if (  g.mp.isGameHost  ==  1 ) 
	{
		SteamEndGame (  );
	}

	t.game.gameloop=0;
	t.game.levelloop=0;
	t.game.titleloop=0;
	t.game.quitflag=1;
}

void mp_freefadesprite ( void )
{
	// 240316 - v1.13b1 - free sprite now finished with fade
	if ( t.tspritetouse > 0 )
	{
		if ( SpriteExist(t.tspritetouse) == 1 ) DeleteSprite ( t.tspritetouse );
		t.tspritetouse = 0;
	}
}

void mp_backToEditor ( void )
{
	t.game.gameloop=0;
	t.game.levelloop=0;
	t.game.titleloop=0;
	t.game.quitflag=1;
	g.mp.goBackToEditor = 1;
}

void mp_cleanupGame ( void )
{
	// default start position is edit-camera XZ
	t.terrain.playerx_f=t.cx_f;
	t.terrain.playerz_f=t.cy_f;
	if (  t.terrain.TerrainID>0 ) 
	{
		t.terrain.playery_f=BT_GetGroundHeight(t.terrain.TerrainID,t.terrain.playerx_f,t.terrain.playerz_f)+150.0;
	}
	else
	{
		t.terrain.playery_f=g.gdefaultterrainheight+150.0;
	}
	t.terrain.playerax_f=0.0;
	t.terrain.playeray_f=0.0;
	t.terrain.playeraz_f=0.0;
	t.camangy_f=0.0;

	// remove all entities
	if ( g.entityelementlist>0 ) 
	{
		for ( t.e = 1 ; t.e<=  g.entityelementlist; t.e++ )
		{
			t.obj=t.entityelement[t.e].obj;
			if (  t.obj>0 ) 
			{
				if (  ObjectExist(t.obj) == 1 ) 
				{
					DeleteObject (  t.obj );
				}
			}
			t.entityelement[t.e].obj=0;
			t.entityelement[t.e].bankindex=0;
		}
		g.entityelementlist=0;
	}
}

void mp_sendSteamIDToEditor ( void )
{
}

