void entity_applydamage ( void )
{
	if ( t.entityelement[t.ttte].obj <= 0 ) return;
	if ( ObjectExist ( t.entityelement[t.ttte].obj ) == 0 ) return;

	int iHealthBefore = t.entityelement[t.ttte].health;

	//  if entity being damaged is protagonist
	if (  t.tskiplayerautoreject == 0 ) 
	{
		if (  t.playercontrol.thirdperson.enabled == 1 ) 
		{
			if (  t.ttte == t.playercontrol.thirdperson.charactere ) 
			{
				//  this is the player that was hit, is player damage instead
				t.testore=t.te ; t.te=t.ttte;
				physics_player_takedamage ( );
				t.te=t.testore;
				return;
			}
		}
	}

	//  early exit
	if (  t.entityelement[t.ttte].active == 0  )  return;

	//  no damage if immobile and not a character (collectables)
	if (  t.tallowanykindofdamage == 0 ) 
	{
		// 010216 - if 'isimmobile' was set in FPE, it cannot be reduced to zero health
		t.ttentid=t.entityelement[t.ttte].bankindex;
		if (  t.entityprofile[t.ttentid].isimmobile == 1 && t.entityprofile[t.ttentid].ischaracter == 0  )
		{
			// code at the end will cap any reduction to 1, this keeps COLLECTABLES alive but allows scripts to detect damage on ISIMMOBILE entities
		}
	}

	//  magnify damage if player has superpowers
	if (  t.tdamagesource == 2 ) 
	{
		if (  t.player[t.plrid].powers.level != 100 ) 
		{
			t.tpowerratio_f=(t.player[t.plrid].powers.level+0.0)/100.0;
			t.tdamage=(t.tdamage+0.0)*t.tpowerratio_f;
			t.tdamageforce=(t.tdamageforce+0.0)*t.tpowerratio_f;
		}
	}

	//  if damaging ai in mp, you will take the aggro of the enemy
	if (  t.game.runasmultiplayer == 1 && g.mp.coop  ==  1 && g.mp.ignoreDamageToEntity  ==  0 ) 
	{
		t.ttentid=t.entityelement[t.ttte].bankindex;
		if (  t.ttentid>0 ) 
		{
			if (  (t.entityprofile[t.ttentid].ischaracter  ==  1 || t.entityelement[t.ttte].mp_isLuaChar  ==  1) && t.entityelement[t.ttte].mp_coopControlledByPlayer  !=  g.mp.me && t.entityprofile[t.entid].ismultiplayercharacter  ==  0 ) 
			{
				if (  t.entityelement[t.ttte].mp_coopControlledByPlayer  ==  -1 ) 
				{
					t.tsteamplayeralive = 0;
				}
				else
				{
					t.tsteamplayeralive = SteamGetPlayerAlive(t.entityelement[t.ttte].mp_coopControlledByPlayer);
				}
				if (  MAXTimer() - t.entityelement[t.ttte].mp_coopLastTimeSwitchedTarget > 5000 || t.tsteamplayeralive  ==  0 ) 
				{
					t.entityelement[t.ttte].mp_coopControlledByPlayer = g.mp.me;
					t.entityelement[t.ttte].mp_updateOn = 1;
					mp_sendlua (  MP_LUA_TakenAggro,t.ttte,g.mp.me );
					t.entityelement[t.ttte].mp_coopLastTimeSwitchedTarget = MAXTimer()+5000;
				}
			}
		}
	}

	//  takes ttte, tdamage, tdamageforce, tdamagesource
	//  takes brayx1# to brayz2#
	//  tdamagesource = 0;self 1;bullet 2;explosion
	t.tobj=t.entityelement[t.ttte].obj;
	//  work out force
	if (  t.tdamageforce>0 ) 
	{
		//  force given
		t.tforce_f=t.tdamageforce;
		//  stop force going overboard in multiplayer or if character
		t.tokay=0;
		if (  t.entityprofile[t.entityelement[t.ttte].bankindex].ismultiplayercharacter == 1  )  t.tokay = 1;
		if (  t.game.runasmultiplayer == 1 && g.mp.coop == 1 && t.entityprofile[t.entityelement[t.ttte].bankindex].ismultiplayercharacter == 1  )  t.tokay = 1;
		if (  t.tokay == 1 ) 
		{
			if (  t.tforce_f>150.0  )  t.tforce_f = 150.0;
		}
	}
	else
	{
		//  no force (possibly provided by ODERayTerrain Ex (  HIT elsewhere such as bulletray subroutine) )
		t.tforce_f=0.0f;
	}

	//  find character or non-character
	int istorecharindex = t.tcharanimindex;
	entity_find_charanimindex_fromttte ( );
	t.ttentid=t.entityelement[t.ttte].bankindex;
	if ( t.tcharanimindex == 0 && t.tforce_f>0 && t.entityprofile[t.ttentid].ragdoll == 0 ) 
	{
		//  NON-CHARACTER ENTITY (used for explosion forces)
		t.tdx_f=(t.brayx2_f-t.brayx1_f)*(t.tforce_f/90.0);
		t.tdy_f=(t.brayy2_f-t.brayy1_f)*(t.tforce_f/90.0);
		t.tdz_f=(t.brayz2_f-t.brayz1_f)*(t.tforce_f/90.0);
		ODEAddBodyForce (  t.tobj,t.tdx_f,t.tdy_f,t.tdz_f,0,0,0 );
		ODESetAngularVelocity (  t.tobj,Rnd(600)-300,Rnd(200)-100,Rnd(600)-300 );
	}
	if (t.tcharanimindex > 0)
	{
		// any damage implied on a character instantly pushes it out of dormancy
		t.charanimstates[t.tcharanimindex].dormant = 0;
	}
	t.tcharanimindex = istorecharindex;

	//  work out damage and see if entity gets destroyed
	entity_determinedamagemultiplier ( );
	t.tdamage=t.tdamage*t.tdamagemultiplier_f;

	//  apply damage locally if not multiplayer
	if (  t.game.runasmultiplayer == 0 ) 
	{
		t.entityelement[t.ttte].health=t.entityelement[t.ttte].health-t.tdamage;
	}
	else
	{
		//  Multiplayer, first checks if it is a player, if it is, we send the damage to them to apply
		//  If not, we appply it and inform everyone else
		if (  t.entityprofile[t.entityelement[t.ttte].bankindex].ismultiplayercharacter == 1 ) 
		{
			for ( int tpindex = 0 ; tpindex<=  MP_MAX_NUMBER_OF_PLAYERS-1; tpindex++ )
			{
				if (  t.mp_playerEntityID[tpindex]  ==  t.ttte && tpindex  !=  g.mp.me && SteamGetPlayerAlive(tpindex)  ==  1 ) 
				{
					t.tSteamForce_f = t.tforce_f;
					if (  t.tSteamForce_f  ==  150  )  t.tSteamForce_f  =  300;
					if (  g.mp.ignoreDamageToEntity  ==  0 ) 
					{
						t.tsteamlastdamagesentcounter = t.tsteamlastdamagesentcounter + 1;
						//  13032015 0XX - Team Multiplayer
						if (  g.mp.team  ==  0 || g.mp.friendlyfireoff  ==  0 || t.mp_team[tpindex]  !=  t.mp_team[g.mp.me] ) 
						{
							SteamApplyPlayerDamage (  tpindex,t.tdamage, t.brayx2_f-t.brayx1_f, t.brayy2_f-t.brayy1_f, t.brayz2_f-t.brayz1_f, t.tSteamForce_f, t.bulletraylimbhit );
						}
					}
				}
			}
		}
		else
		{
			//  it is not a player, so we can apply damage to it
			if (  t.entityelement[t.ttte].health > 0 ) 
			{
				t.entityelement[t.ttte].health=t.entityelement[t.ttte].health-t.tdamage;
				if (  g.mp.ignoreDamageToEntity  ==  0 ) 
				{
					if (  t.entityelement[t.ttte].health  <= 0 ) 
					{
						//  for coop, we count ai kills and not player kills
						if (  g.mp.coop  ==  1 ) 
						{
							t.tttentid=t.entityelement[t.ttte].bankindex;
							if (  t.tttentid > 0 ) 
							{
								if (  t.entityprofile[t.tttentid].ischaracter  ==  1 || t.entityelement[t.ttte].mp_isLuaChar  ==  1 ) 
								{
									mp_IKilledAnAI ( );
								}
							}
						}
						++t.tempsteamdestroycount;
						mp_destroyentity ( );
					}
				}
			}
		}
	}
	// a hit that changed the health forces the entity's Lua row to be rewritten next frame at any distance (the value
	// 2); with 1 a far entity's row waited for its turn, one frame in 25-50 past 750 units and never past the freeze
	// distance, so a script saw the drop late or not at all
	// 3, not 2: 2 is the loop's refresh-only turn (the row rewritten, the script NOT run that frame, meant for entities out
	// of logic range), so a hit on a scripted entity in range lost its script for one frame: a vehicle the player was in
	// skipped its camera write and the frame rendered from the player's eye inside the hull (2026-10-21). 3 refreshes the
	// row at any distance and runs the script as every frame
	t.entityelement[t.ttte].lua.flagschanged = (t.entityelement[t.ttte].health != iHealthBefore) ? 3 : 1;

	// 010216 - special case for ISIMMOBILE FPE that are not characters, they cannot get to zero health (COLLECTABLES)
	if (  t.tallowanykindofdamage == 0 ) 
	{
		t.ttentid=t.entityelement[t.ttte].bankindex;
		if (  t.entityprofile[t.ttentid].isimmobile == 1 && t.entityprofile[t.ttentid].ischaracter == 0  )
		{
			// this prevents turrets from being destroyed, so also check if a collectable
			if (t.entityelement[t.ttte].eleprof.iscollectable != 0)
			{
				if (t.entityelement[t.ttte].health <= 0)
					t.entityelement[t.ttte].health = 1;
			}
		}
	}

	// used when switch off auto matic destroy so can play anim before ragdoll, etc (or for damage invulnerability)
	if (t.entityelement[t.ttte].briefimmunity != 0)
		if (t.entityelement[t.ttte].health <= 0)
			t.entityelement[t.ttte].health = 1;

	// the player's hit record: the damage as subtracted and the health after the clamps (only while a player entry is open)
	playerhit_adddamage(t.ttte, t.tdamage, iHealthBefore, t.entityelement[t.ttte].health);

	// when health drops to zero
	if (t.entityelement[t.ttte].health <= 0)
	{
		// counting any entity damage at or below zero a destroy event (for counting XP)
		//PE: Only add this one time or XP just keep increasing. Like "rats" would triggere this constantly even if dead.
		if(iHealthBefore > 0)
			entity_adddestroyevent(t.ttte);

		//  if explodble, have a delayed reaction
		if (t.entityelement[t.ttte].eleprof.explodable != 0)
		{
			if (t.entityelement[t.ttte].explodefusetime == 0)
			{
				if (t.tdamagesource == 2)
				{
					//  explosion is time delayed
					t.entityelement[t.ttte].explodefusetime = MAXTimer() + 350 + ( rand() % 250 );
				}
				else
				{
					//  explosion is instant
					t.entityelement[t.ttte].explodefusetime = MAXTimer();
				}
			}
		}
		else
		{
			//  reset health to zero
			t.entityelement[t.ttte].health = 0;
		}

		// 010616 - May be a third person character, no ragdoll means find death animation and play
		int iThirdPersonCharacter = 0;
		if (t.playercontrol.thirdperson.enabled == 1)
			if (t.playercontrol.thirdperson.charactere == t.ttte)
				iThirdPersonCharacter = t.playercontrol.thirdperson.characterindex;

		//  if character
		int iStoreCharAnimIndex = t.tcharanimindex;
		entity_find_charanimindex_fromttte();
		int iCharacterIndexToUse = t.tcharanimindex;
		t.tcharanimindex = iStoreCharAnimIndex;
		t.tapplyragdollforce = 0;
		if (iThirdPersonCharacter > 0) iCharacterIndexToUse = iThirdPersonCharacter;
		if (t.entityelement[t.ttte].eleprof.explodable != 0) iCharacterIndexToUse = 0;
		if (iCharacterIndexToUse > 0)
		{
			//  CHARACTER
			if (iThirdPersonCharacter == 0)
			{
				int iStoreCharIndex = t.tcharanimindex;
				t.tcharanimindex = iCharacterIndexToUse;
				darkai_killai ();
				t.tcharanimindex = iStoreCharIndex;
			}

			//  if dead, trigger impact death
			t.tdx_f = ObjectPositionX(t.tobj) - t.twhox_f;
			t.tdz_f = ObjectPositionZ(t.tobj) - t.twhoz_f;
			t.tda_f = atan2deg(t.tdx_f, t.tdz_f);
			t.relativeangle_f = WrapValue(ObjectAngleY(t.tobj) - t.tda_f);
			t.impacting = 5;
			extern bool g_bForceNoRagdollJustDestroy;
			if (g_bForceNoRagdollJustDestroy == true)
			{
				// no animation or ragdoll this time
				t.impacting = 0;
			}
			else
			{
				extern bool g_bForceRagdoll;
				extern bool g_bRagdollAllDeaths;
				// SetRagdollAllDeaths(1) gives every death of a ragdoll character (not the third person player) a forced ragdoll
				if (g_bForceRagdoll == true || (g_bRagdollAllDeaths == true && iThirdPersonCharacter == 0 && t.entityprofile[t.ttentid].ragdoll == 1))
				{
					// trigger a forced ragdoll event
					t.impacting = 6;
				}
				else
				{
					if (t.relativeangle_f >= 315 || t.relativeangle_f < 45)  t.impacting = 1;
					if (t.relativeangle_f >= 45 && t.relativeangle_f < 135)  t.impacting = 3;
					if (t.relativeangle_f >= 135 && t.relativeangle_f < 225)  t.impacting = 2;
					if (t.relativeangle_f >= 225 && t.relativeangle_f < 315)  t.impacting = 4;
				}
			}

			//  cannot use state engine - use instant animation for this
			if (t.charanimstates[iCharacterIndexToUse].playcsi != g.csi_limbo)
			{
				if (t.charanimstates[iCharacterIndexToUse].playcsi > 0 && t.charanimstates[iCharacterIndexToUse].playcsi >= t.csi_crouchidle[t.charanimstates[iCharacterIndexToUse].weapstyle] && t.charanimstates[iCharacterIndexToUse].playcsi <= t.csi_crouchgetup[t.charanimstates[iCharacterIndexToUse].weapstyle])
				{
					//  die crouched
					if (t.impacting == 1)  t.charanimstates[iCharacterIndexToUse].playcsi = t.csi_crouchimpactfore[t.charanimstates[iCharacterIndexToUse].weapstyle];
					if (t.impacting == 2)  t.charanimstates[iCharacterIndexToUse].playcsi = t.csi_crouchimpactback[t.charanimstates[iCharacterIndexToUse].weapstyle];
					if (t.impacting == 3)  t.charanimstates[iCharacterIndexToUse].playcsi = t.csi_crouchimpactleft[t.charanimstates[iCharacterIndexToUse].weapstyle];
					if (t.impacting == 4)  t.charanimstates[iCharacterIndexToUse].playcsi = t.csi_crouchimpactright[t.charanimstates[iCharacterIndexToUse].weapstyle];
				}
				else
				{
					//  die stood
					if (t.impacting == 1)  t.charanimstates[iCharacterIndexToUse].playcsi = g.csi_unarmedimpactfore;
					if (t.impacting == 2)  t.charanimstates[iCharacterIndexToUse].playcsi = g.csi_unarmedimpactback;
					if (t.impacting == 3)  t.charanimstates[iCharacterIndexToUse].playcsi = g.csi_unarmedimpactleft;
					if (t.impacting == 4)  t.charanimstates[iCharacterIndexToUse].playcsi = g.csi_unarmedimpactright;
				}
				if (t.impacting == 5)  t.charanimstates[iCharacterIndexToUse].playcsi = g.csi_unarmeddeath;
				t.smoothanim[t.tobj].transition = 0;
			}

			// only for regular characters
			if (iThirdPersonCharacter == 0)
			{
				// wipe out health
				t.entityelement[t.charanimstates[iCharacterIndexToUse].e].health = 0;

				// reset spin e twist and neck twist
				t.charanimstates[iCharacterIndexToUse].spineAiming = 0.0f;
				t.charanimstates[iCharacterIndexToUse].neckAiming = 0.0f;
				int iCharObj = t.charanimstates[iCharacterIndexToUse].obj;
				if (iCharObj > 0)
				{
					int iEntID = t.entityelement[t.charanimstates[iCharacterIndexToUse].e].bankindex;
					int iFrameIndex = t.entityprofile[iEntID].spine2;
					if (iFrameIndex > 0)
					{
						sObject* pCharObject = GetObjectData(iCharObj);
						sFrame* pFrameOfLimb = pCharObject->ppFrameList[iFrameIndex];
						if (pFrameOfLimb)
						{
							WickedCall_RotateLimb(pCharObject, pFrameOfLimb, 0, 0, 0);
						}
					}
				}

				//  setting main to 0 so the main lua won't be called for this object
				t.entityelement[t.ttte].eleprof.aimain = 0;

				//  Prepare character for eventual fade out
				if (t.entityprofile[t.ttentid].ragdoll == 1)
				{
					t.charanimstates[iCharacterIndexToUse].timetofadeout = MAXTimer() + 20000; // from old AICORPSETIME
					t.charanimstates[iCharacterIndexToUse].fadeoutvalue_f = 1.0;
				}
			}

			//  Convert to clone so can operate independent of parent object
			t.tte = t.ttte; entity_converttoclone ();

			//  Ragdoll for characters is now optional
			if (t.entityprofile[t.ttentid].ragdoll == 1)
			{
				//  create ragdoll and stop any further manipulation of the object
				// GG: not for one already a ragdoll (knocked out by SetEntityRagdoll), which would get a death animation and its box
				if (t.entityelement[t.ttte].ragdollplusactivate == 0 && BPhys_RagdollExist(t.entityelement[t.ttte].obj) == 0)
				{
					t.entityelement[t.ttte].ragdollplusactivate = t.impacting;
					// could use: t.bulletraytype; // 1-pierce, 2-shotgun shell
					if (t.tdamageforce > 1000)
						t.entityelement[t.ttte].ragdollplusweapontypeused = 2;
					else
						t.entityelement[t.ttte].ragdollplusweapontypeused = 1;
				}
			}

			// only for regular characters
			if (iThirdPersonCharacter == 0)
			{
				//  Ensure character control ceases at this (instantly for ragdoll / anim death delays this assignment)
				if (t.entityprofile[t.ttentid].ragdoll == 1)
				{
					t.charanimstates[iCharacterIndexToUse].e = 0;
				}
			}
		}
		else
		{
			//  NON-CHARACTER, but can still have ragdoll flagged (like Zombies)
			if (t.entityprofile[t.ttentid].ragdoll == 1)
			{
				// can only ragdoll clones not instances
				t.tte = t.ttte; entity_converttoclone ();

				// create ragdoll and stop any further manipulation of the object
				ragdoll_setcollisionmask (t.entityelement[t.ttte].eleprof.colondeath);
				t.tphye = t.ttte; t.tphyobj = t.entityelement[t.ttte].obj; ragdoll_create ();
				t.tapplyragdollforce = 1;

				// and make attachment object a physics object
				t.tattobj = t.entityelement[t.ttte].attachmentobj;
				if (t.tattobj > 0)
				{
					// and ensure it does not bury into surface by raising it
					if (ODEFind(t.tattobj) == 0)
					{
						ODECreateDynamicBox (t.tattobj, -1, 1);
						// its body goes once it has landed (M-Physics.cpp)
						extern void physics_trackdroppedweapon(int iObject);
						physics_trackdroppedweapon(t.tattobj);
					}
				}

				// and ensure entity is destroyed (active to zero)
				t.entityelement[t.ttte].active = 0;
				t.entityelement[t.ttte].health = 0;
				t.entityelement[t.ttte].lua.flagschanged = 2;
			}
		}

		//  multiplayer undocumented stuff
		if (t.game.runasmultiplayer == 1)
		{
			if (g.mp.ignoreDamageToEntity == 1)
			{
				if (t.tapplyragdollforce == 1)
				{
					t.tapplyragdollforce = 0;
					t.entityelement[t.ttte].ragdollified = 1;
				}
			}
		}

		// and apply bullet directional force (tforce#=from gun settings)
		//bAllowRagdollForceToBeRecorded = true;
	}

	bool bAllowRagdollForceToBeRecorded = false;
	// as bullet hits can be interceded with an animation, and then a tru destroy to create the ragdoll, 
	// for MAX< record the ragdoll force from the original hit so we can apply when we finally become a ragdoll :)
	if (t.tforce_f > 0.0f && t.bulletraylimbhit != -1)
	{
		//if (t.tapplyragdollforce == 1)
		if (t.entityelement[t.ttte].ragdollified == 0 )
		{
			bAllowRagdollForceToBeRecorded = true;
		}
	}
	else
	{
		// but retains last good ragdoll force values if called again but with no force (see soldier attack behavior in MAX)
	}

	// apply bullet directional force if all the elements in the equation are good
	if (bAllowRagdollForceToBeRecorded == true )
	{
		{
			t.entityelement[t.ttte].ragdollified=1;
			t.entityelement[t.ttte].ragdollifiedforcex_f=(t.brayx2_f-t.brayx1_f)*0.8;
			t.entityelement[t.ttte].ragdollifiedforcey_f=(t.brayy2_f-t.brayy1_f)*1.2;
			t.entityelement[t.ttte].ragdollifiedforcez_f=(t.brayz2_f-t.brayz1_f)*0.8;
			if ( t.game.runasmultiplayer == 0 ) 
			{
				t.entityelement[t.ttte].ragdollifiedforcevalue_f=t.tforce_f*8000.0;
			}
			else
			{
				t.tsteamcoopforcemulti_f = 8000.0;
				if ( g.mp.coop == 1 ) 
				{
					if ( t.entityprofile[t.entityelement[t.ttte].bankindex].ismultiplayercharacter == 1 ) 
					{
						if ( t.tforce_f > 300.0  )  t.tforce_f  =  300.0;
						t.tsteamcoopforcemulti_f = 2000.0;
					}
				}
				t.entityelement[t.ttte].ragdollifiedforcevalue_f=t.tforce_f*t.tsteamcoopforcemulti_f;
			}
			t.entityelement[t.ttte].ragdollifiedforcelimb=t.bulletraylimbhit;
			t.bulletraylimbhit=-1;
		}
	}
}

void entity_applydecalfordamage ( int ee, float fX, float fY, float fZ)
{
	if (ee > 0 && ee < t.entityelement.size())
	{
		// safety protection to prevent crash if entity element is not valid
	}
	else
		return;
		
	// create either material decal specified in FPE or blood decal
	int entid = t.entityelement[ee].bankindex;
	if (entid > 0 && entid < t.entityprofile.size())
	{
		if (fX == -1 && fX == -1 && fX == -1)
		{
			// and only assume blood if called from a character/entity hurt function
			t.tttriggerdecalimpact = 2;
		}
		if (t.entityprofile[entid].bloodscorch == 0)
		{
			if (entid < t.entitydecal.size())
			{
				if (t.entityprofile[entid].decalmax > 0 && t.entitydecal[entid][0] > 0)
				{
					t.tttriggerdecalimpact = 1;
					t.decalglobal.impactid = t.entitydecal[entid][0];
				}
			}
		}
		float fTorseAreaX = t.entityelement[ee].x;
		float fTorseAreaY = t.entityelement[ee].y + 50;
		float fTorseAreaZ = t.entityelement[ee].z;
		if (fX != -1 || fY != -1 || fZ != -1)
		{
			fTorseAreaX = fX;
			fTorseAreaY = fY;
			fTorseAreaZ = fZ;
		}
		else
		{
			int torselimbindex = t.entityprofile[entid].spine2;
			if (torselimbindex > 0)
			{
				fTorseAreaX = LimbPositionX(t.entityelement[ee].obj, torselimbindex);
				fTorseAreaY = LimbPositionY(t.entityelement[ee].obj, torselimbindex);
				fTorseAreaZ = LimbPositionZ(t.entityelement[ee].obj, torselimbindex);
			}
		}
		entity_triggerdecalatimpact (fTorseAreaX, fTorseAreaY, fTorseAreaZ);
	}
}

void entity_gettruecamera ( void )
{
	//  True camera position
	if (  t.playercontrol.thirdperson.enabled == 1 ) 
	{
		t.tcamerapositionx_f=t.playercontrol.thirdperson.storecamposx;
		t.tcamerapositiony_f=t.playercontrol.thirdperson.storecamposy;
		t.tcamerapositionz_f=t.playercontrol.thirdperson.storecamposz;
	}
	else
	{
		t.tcamerapositionx_f=CameraPositionX(t.terrain.gameplaycamera);
		t.tcamerapositiony_f=CameraPositionY(t.terrain.gameplaycamera);
		t.tcamerapositionz_f=CameraPositionZ(t.terrain.gameplaycamera);
	}
}

void entity_gettrueplayerpos(void)
{
	//  True camera position
	if (t.playercontrol.thirdperson.enabled == 1)
	{
		t.tcamerapositionx_f = ObjectPositionX(t.aisystem.objectstartindex);
		t.tcamerapositiony_f = ObjectPositionY(t.aisystem.objectstartindex);
		t.tcamerapositionz_f = ObjectPositionZ(t.aisystem.objectstartindex);
	}
	else
	{
		t.tcamerapositionx_f = CameraPositionX(t.terrain.gameplaycamera);
		t.tcamerapositiony_f = CameraPositionY(t.terrain.gameplaycamera);
		t.tcamerapositionz_f = CameraPositionZ(t.terrain.gameplaycamera);
	}
}

// GGMAX 2.48: SHOT tracer — the trigger tracer (DUMP_FIRE) proves the trigger worked; this one
// proves what the BULLET RAY did. One row per fired ray, sampled immediately after the hitscan,
// so a "no damage while zoomed" report can be split into: ray hit an entity / hit terrain /
// clean miss / SWALLOWED by an out-of-range object (g_ggLastRayBlockedBy names it — the weapon
// model is the suspect, g_iCurrentGunObj). Ring of 16, always on: it costs a few writes per
// SHOT, not per frame. Dumped by the DUMP_SHOT harness command.
DWORD g_ggLastRayBlockedBy = 0;   // written inside IntersectAllEx's discard branch (CObjectsC_part3.cpp)
extern int g_iCurrentGunObj;
struct GGShotRow
{
	int zoom, gunobj, hit;
	DWORD blockedBy;
	float x1, y1, z1, x2, y2, z2;
	int terrainShortened;
};
static GGShotRow g_ggShotRows[16];
static int g_ggShotCount = 0;
void GGShotTraceDump(char* result, int resultSize)
{
	// GGMAX 3.35i: debug output goes beside the EXE, not into whatever the CWD is now
	// (a file dialog can move it). Defined in Guru-WickedMAX/master_part1.cpp.
	extern FILE* GGDiagFopen(const char* name, const char* mode);
	FILE* f = GGDiagFopen("shottrace.txt", "w");
	int swallowed = 0, hits = 0, terrain = 0, misses = 0;
	DWORD lastBlocker = 0;
	int n = g_ggShotCount < 16 ? g_ggShotCount : 16;
	for (int i = 0; i < n; i++)
	{
		GGShotRow& r = g_ggShotRows[i];
		if (r.hit > 0) hits++;
		else if (r.hit == -1 || r.terrainShortened) terrain++;
		else if (r.blockedBy != 0) { swallowed++; lastBlocker = r.blockedBy; }
		else misses++;
		if (f) fprintf(f, "shot %d: zoom=%d hit=%d blockedBy=%lu gunobj=%d terr=%d ray (%.1f,%.1f,%.1f)->(%.1f,%.1f,%.1f)\n",
			i, r.zoom, r.hit, (unsigned long)r.blockedBy, r.gunobj, r.terrainShortened,
			r.x1, r.y1, r.z1, r.x2, r.y2, r.z2);
	}
	if (f) fclose(f);
	const char* verdict = "no shots recorded — fire first (FIRE_WEAPON / ZOOM_FIRE)";
	if (swallowed > 0) verdict = "RAY SWALLOWED by an out-of-entity-range object — if blockedBy == gunobj, the weapon mesh ate the shot (stale aabb.layerMask)";
	else if (hits > 0) verdict = "ray HIT an entity — hitscan is working";
	else if (terrain > 0) verdict = "ray reached TERRAIN only";
	else if (misses > 0) verdict = "clean miss — ray cast fine, nothing in its path";
	_snprintf(result, resultSize,
		"OK: DUMP_SHOT shots=%d hits=%d terrain=%d swallowed=%d misses=%d lastBlocker=%lu -> Files/shottrace.txt\nVERDICT: %s",
		g_ggShotCount, hits, terrain, swallowed, misses, (unsigned long)lastBlocker, verdict);
	result[resultSize - 1] = 0;
	g_ggShotCount = 0;
}

// what a new placement's Allow Bullet Holes and Clear Grass and Trees ticks start as when its .fpe doesn't say, set in
// Settings (Developer): 0 unset, following the rules below, 1 yes, 2 no. Only new placements take it
// (entity_addentitytomap_core), so changing it leaves placed objects as they are
int g_iDefaultBulletHoles = 0;
int g_iDefaultClearVegetation = 0;

// whether a bullet hole may be left on entity e: its Allow Bullet Holes tick once set in the editor, 1 yes, 2 no; until
// then (0, also every unticked placement saved before the tick could be cleared) allowbulletholes in its .fpe, and
// without that key static entities only
bool entity_allowsbulletholes(int e)
{
	if (t.entityelement[e].iAllowBuletHole == 1) return true;
	if (t.entityelement[e].iAllowBuletHole == 2) return false;
	int iAllowBulletHoles = t.entityprofile[t.entityelement[e].bankindex].allowbulletholes;
	if (iAllowBulletHoles != -1) return iAllowBulletHoles == 1;
	return t.entityelement[e].staticflag == 1;
}

// the player's hit record, read by Lua (GetPlayerHitSeq / GetPlayerHit): one entry for each ray the player's held weapon
// resolves (gun_shoot_oneray opens it around entity_hasbulletrayhit), and one for each entity a player projectile's blast
// damages (weapon_projectileresult_make). Only damage done while an entry is open is recorded, so other rays and
// explosions stay out. Entries are kept in a ring and reading does not remove them
static const int PLAYERHIT_RINGSIZE = 128;
static sPlayerHit g_PlayerHitRing[PLAYERHIT_RINGSIZE];
static int g_iPlayerHitSeq = 0;
static int g_iPlayerHitOpenMode = 0; // 0 none, 1 a ray (entry g_iPlayerHitOpenSeq), 2 a blast
static int g_iPlayerHitOpenSeq = 0;
static sPlayerHit g_PlayerHitBlast;

void playerhit_clear(void)
{
	memset(g_PlayerHitRing, 0, sizeof(g_PlayerHitRing));
	g_iPlayerHitSeq = 0;
	g_iPlayerHitOpenMode = 0;
	g_iPlayerHitOpenSeq = 0;
}

static sPlayerHit* playerhit_new(void)
{
	g_iPlayerHitSeq++;
	sPlayerHit* pHit = &g_PlayerHitRing[g_iPlayerHitSeq % PLAYERHIT_RINGSIZE];
	memset(pHit, 0, sizeof(sPlayerHit));
	pHit->iSeq = g_iPlayerHitSeq;
	pHit->iLimb = -1;
	return pHit;
}

void playerhit_openray(int iKind, int iShot, int iGunID, float fOX, float fOY, float fOZ)
{
	sPlayerHit* pHit = playerhit_new();
	pHit->iKind = iKind;
	pHit->iShot = iShot;
	pHit->iGunID = iGunID;
	pHit->fOX = fOX; pHit->fOY = fOY; pHit->fOZ = fOZ;
	g_iPlayerHitOpenMode = 1;
	g_iPlayerHitOpenSeq = pHit->iSeq;
}

void playerhit_openblast(int iShot, int iGunID, float fX, float fY, float fZ)
{
	memset(&g_PlayerHitBlast, 0, sizeof(sPlayerHit));
	g_PlayerHitBlast.iHit = 2;
	g_PlayerHitBlast.iLimb = -1;
	g_PlayerHitBlast.iKind = 4;
	g_PlayerHitBlast.iShot = iShot;
	g_PlayerHitBlast.iGunID = iGunID;
	g_PlayerHitBlast.fX = g_PlayerHitBlast.fOX = fX;
	g_PlayerHitBlast.fY = g_PlayerHitBlast.fOY = fY;
	g_PlayerHitBlast.fZ = g_PlayerHitBlast.fOZ = fZ;
	g_iPlayerHitOpenMode = 2;
	g_iPlayerHitOpenSeq = g_iPlayerHitSeq + 1;
}

void playerhit_close(void)
{
	g_iPlayerHitOpenMode = 0;
}

sPlayerHit* playerhit_get(int iSeq)
{
	if (iSeq <= 0 || iSeq > g_iPlayerHitSeq || iSeq <= g_iPlayerHitSeq - PLAYERHIT_RINGSIZE) return NULL;
	sPlayerHit* pHit = &g_PlayerHitRing[iSeq % PLAYERHIT_RINGSIZE];
	if (pHit->iSeq != iSeq) return NULL;
	return pHit;
}

int playerhit_getseq(void)
{
	return g_iPlayerHitSeq;
}

// what the open ray met, written at the end of entity_hasbulletrayhit
static void playerhit_setray(int iHit, int e, float fX, float fY, float fZ, float fNX, float fNY, float fNZ, int iMaterial, int iHole, int iLimb)
{
	if (g_iPlayerHitOpenMode != 1) return;
	sPlayerHit* pHit = playerhit_get(g_iPlayerHitOpenSeq);
	if (!pHit) return;
	pHit->iHit = iHit;
	pHit->e = e;
	if (pHit->e == 0 && pHit->iDamageE > 0) pHit->e = pHit->iDamageE;
	pHit->fX = fX; pHit->fY = fY; pHit->fZ = fZ;
	pHit->fNX = fNX; pHit->fNY = fNY; pHit->fNZ = fNZ;
	pHit->iMaterial = iMaterial;
	pHit->iHole = iHole;
	pHit->iLimb = iLimb;
}

// damage entity_applydamage did while an entry is open: to the open ray's entry, or for a blast one entry per entity
void playerhit_adddamage(int e, int iDamage, int iHealthBefore, int iHealthAfter)
{
	sPlayerHit* pHit = NULL;
	if (g_iPlayerHitOpenMode == 1)
	{
		pHit = playerhit_get(g_iPlayerHitOpenSeq);
	}
	else if (g_iPlayerHitOpenMode == 2)
	{
		for (int iSeq = g_iPlayerHitOpenSeq; iSeq <= g_iPlayerHitSeq && pHit == NULL; iSeq++)
		{
			sPlayerHit* pBlastHit = playerhit_get(iSeq);
			if (pBlastHit && pBlastHit->e == e) pHit = pBlastHit;
		}
		if (pHit == NULL)
		{
			pHit = playerhit_new();
			int iSeq = pHit->iSeq;
			*pHit = g_PlayerHitBlast;
			pHit->iSeq = iSeq;
			pHit->e = e;
		}
	}
	if (!pHit) return;
	if (pHit->iDamageE == 0)
		pHit->iHealthBefore = iHealthBefore;
	else
		pHit->iDamage -= iHealthBefore - pHit->iHealthAfter; // health changed between the hits (a destroyed entity's is reset to 0), so the entry keeps before - damage = after
	pHit->iDamageE = e;
	pHit->iDamage += iDamage;
	pHit->iHealthAfter = iHealthAfter;
	pHit->iKilled = (pHit->iHealthBefore > 0 && iHealthAfter <= 0) ? 1 : 0;
}

// a shot from a script's weapon, detected as entity_hasbulletrayhit detects the player's: terrain first, then the entities with
// full Wicked accuracy, physics shapes for collisionoverride and collisionmode 11 entities, and the material of the entity hit.
// With bLeaveHole it leaves a bullet hole under the same rules. Returns 0 for no hit, 1 for terrain, 2 for an object,
// 5 for a tree
int entity_scriptbulletray ( float fX1, float fY1, float fZ1, float fX2, float fY2, float fZ2, int iIgnoreObj, bool bLeaveHole, int iTerrainMaterial, sScriptBulletRayHit* pHit )
{
	int iHitType = 0;
	int iHitE = 0;
	int iMaterial = 0;
	GGVECTOR3 vecHit = GGVECTOR3(fX2, fY2, fZ2);
	GGVECTOR3 vecNormal = GGVECTOR3(0, 0, 0);

	// terrain first, which shortens the ray (the player's guns treat terrain as material 0, no hole); a tree trunk is
	// class 5 with no material
	if (ODERayTerrain(fX1, fY1, fZ1, fX2, fY2, fZ2, false) == 1)
	{
		vecHit = GGVECTOR3(ODEGetRayCollisionX(), ODEGetRayCollisionY(), ODEGetRayCollisionZ());
		vecNormal = GGVECTOR3(ODEGetRayNormalX(), ODEGetRayNormalY(), ODEGetRayNormalZ());
		extern int g_iODERayTerrainHitTree;
		iHitType = g_iODERayTerrainHitTree ? 5 : 1;
		iMaterial = g_iODERayTerrainHitTree ? 0 : iTerrainMaterial;
	}

	// then the entities, up to the terrain hit
	int iHitObj = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX1, fY1, fZ1, vecHit.x, vecHit.y, vecHit.z, iIgnoreObj, 0, 0, 0, 0, true);
	if (iHitObj > 0 && iHitObj != iIgnoreObj && ObjectExist(iHitObj) == 1)
	{
		int iFoundE = 0;
		for (int e = 1; e <= g.entityelementlist; e++)
		{
			if (t.entityelement[e].obj == iHitObj)
			{
				iFoundE = e; break;
			}
		}
		int iEntID = 0;
		if (iFoundE > 0) iEntID = t.entityelement[iFoundE].bankindex;

		// objects that use physics collision over geometry collision
		int iCollisionWithPhysics = 0;
		if (iFoundE > 0 && (t.entityprofile[iEntID].collisionoverride == 1 || t.entityprofile[iEntID].collisionmode == 11))
		{
			if (t.entityprofile[iEntID].collisionmode == 11)
				iCollisionWithPhysics = 2;
			else if (ODERayTerrainEx(fX1, fY1, fZ1, vecHit.x, vecHit.y, vecHit.z, 2, false) == 1)
				iCollisionWithPhysics = 1;
			else
				iCollisionWithPhysics = 2;
		}

		// 2 = hit the geometry but missed the physics shape (foliage), so the shot passes as it does for the player
		if (iCollisionWithPhysics != 2)
		{
			if (iCollisionWithPhysics == 1)
			{
				vecHit = GGVECTOR3(ODEGetRayCollisionX(), ODEGetRayCollisionY(), ODEGetRayCollisionZ());
				vecNormal = GGVECTOR3(ODEGetRayNormalX(), ODEGetRayNormalY(), ODEGetRayNormalZ());
			}
			else
			{
				vecHit = GGVECTOR3(ChecklistFValueA(6), ChecklistFValueB(6), ChecklistFValueC(6));
				vecNormal = GGVECTOR3(ChecklistFValueA(7), ChecklistFValueB(7), ChecklistFValueC(7));
			}
			iHitType = 2;
			iHitE = iFoundE;
			iMaterial = 0;
			if (iFoundE > 0)
			{
				if (!t.entityelement[iFoundE].eleprof.bUseFPESettings && t.entityelement[iFoundE].eleprof.iMaterialSoundIndex > 0)
					iMaterial = t.entityelement[iFoundE].eleprof.iMaterialSoundIndex;
				else
					iMaterial = t.entityprofile[iEntID].materialindex;
			}
		}
	}

	// bullet hole, under the player's rules: not material 6, only terrain, a static entity or one that allows holes, and a
	// violent character bleeds instead
	bool bHole = false;
	if (bLeaveHole && iHitType > 0 && iMaterial > 0 && iMaterial != 6)
	{
		bool bAllowed = (iHitE == 0 || entity_allowsbulletholes(iHitE));
		if (iHitE > 0 && t.entityprofile[t.entityelement[iHitE].bankindex].ischaracter == 1 && t.entityelement[iHitE].eleprof.isviolent != 0) bAllowed = false;
		if (bAllowed)
		{
			bulletholes_add(iMaterial, vecHit.x, vecHit.y, vecHit.z, vecNormal.x, vecNormal.y, vecNormal.z, iHitE);
			bHole = true;
		}
	}

	if (pHit)
	{
		pHit->e = iHitE;
		pHit->fX = vecHit.x; pHit->fY = vecHit.y; pHit->fZ = vecHit.z;
		pHit->fNX = vecNormal.x; pHit->fNY = vecNormal.y; pHit->fNZ = vecNormal.z;
		pHit->iMaterial = iMaterial;
		pHit->bHole = bHole;
	}
	return iHitType;
}

void entity_hasbulletrayhit(void)
{
	// bulletray is x1#,y1#,z1#,x2#,y2#,z2#,bulletrayhit,gunrange#,t.bulletfinalstrengthmod(1),bulletisinfactmeleestrike
	t.brayx1_f = t.x1_f; t.brayy1_f = t.y1_f; t.brayz1_f = t.z1_f;
	t.brayx2_f = t.x2_f; t.brayy2_f = t.y2_f; t.brayz2_f = t.z2_f;
	t.bulletrayhit = 0; t.bulletraylimbhit = -1; t.tttriggerdecalimpact = 0;
	t.tfoundentityindexhit = -1;
	t.tmaterialvalue = -1;

	// first cast a ray at any terrain
	GGVECTOR3 vecRayHitNormal = GGVECTOR3(0, 0, 0);
	int iPlayerHitClass = 0; // for the player's hit record: 0 none, 1 terrain, 2 entity, 3 other object, 4 passed through, 5 tree
	int iPlayerHitE = 0;
	if (ODERayTerrain(t.brayx1_f, t.brayy1_f, t.brayz1_f, t.brayx2_f, t.brayy2_f, t.brayz2_f, false) == 1)
	{
		//  and shorten the ray if we hit terra firma!
		t.brayx2_f = ODEGetRayCollisionX();
		t.brayy2_f = ODEGetRayCollisionY();
		t.brayz2_f = ODEGetRayCollisionZ();
		// get extra collision data, need to know surface normal
		vecRayHitNormal = GGVECTOR3(ODEGetRayNormalX(), ODEGetRayNormalY(), ODEGetRayNormalZ());
		// LEELEE = need to get TERRAIN MATERIAL ID HERE TOO!!
		t.tttriggerdecalimpact = 10;
		extern int g_iODERayTerrainHitTree;
		iPlayerHitClass = g_iODERayTerrainHitTree ? 5 : 1;
	}
	
	// Character creator can override the limb hit, to make the cc head report the head limb of the main character
	t.ccLimbHitOverride = false;

	// if TPP, can ignore entity used as player
	int iIgnoreOneEntityObj = 0;
	if ( t.playercontrol.thirdperson.enabled == 1 )
	{
		// 220217 - cannot shoot self with weapon!
		iIgnoreOneEntityObj = t.entityelement[t.playercontrol.thirdperson.charactere].obj;
	}
	bool bFullWickedAccuracy = true;
	t.thitvalue = IntersectAllEx (g.entityviewstartobj, g.entityviewendobj, t.brayx1_f, t.brayy1_f, t.brayz1_f, t.brayx2_f, t.brayy2_f, t.brayz2_f, 0, 0, 0, 0, 0, bFullWickedAccuracy);
	{
		// GGMAX 2.48 shot tracer sample (see block above entity_hasbulletrayhit)
		GGShotRow& r = g_ggShotRows[g_ggShotCount % 16];
		r.zoom = t.gunzoommode; r.gunobj = g_iCurrentGunObj; r.hit = t.thitvalue;
		r.blockedBy = g_ggLastRayBlockedBy;
		r.x1 = t.brayx1_f; r.y1 = t.brayy1_f; r.z1 = t.brayz1_f;
		r.x2 = t.brayx2_f; r.y2 = t.brayy2_f; r.z2 = t.brayz2_f;
		r.terrainShortened = (t.tttriggerdecalimpact == 10) ? 1 : 0;
		g_ggShotCount++;
	}
	if ( t.thitvalue>0 )
	{
		if (t.thitvalue > 0)
		{
			if (ObjectExist(t.thitvalue) == 1)
			{
				for (t.tte = 1; t.tte <= g.entityelementlist; t.tte++)
				{
					if (t.entityelement[t.tte].obj == t.thitvalue)
					{
						t.tfoundentityindexhit = t.tte; break;
					}
				}
			}
		}
		// record object number we hit
		t.tsteamLastHit=t.thitvalue;
		t.bulletrayhit=t.thitvalue;
		//  first check if object uses 'physics collision' over 'geometry collision'
		t.tcollisionwithphysics=0;
		if (t.tfoundentityindexhit != -1)
		{
			t.tentid = t.entityelement[t.tfoundentityindexhit].bankindex;
		}
		if (t.tfoundentityindexhit != -1)
		{
			if (  t.entityprofile[t.tentid].collisionoverride == 1 || t.entityprofile[t.tentid].collisionmode == 11 )
			{
				if (  t.entityprofile[t.tentid].collisionmode == 11 ) 
				{
					t.tcollisionwithphysics=2;
				}
				else
				{
					if ( ODERayTerrainEx(t.brayx1_f,t.brayy1_f,t.brayz1_f,t.brayx2_f,t.brayy2_f,t.brayz2_f,2,false) == 1 ) 
					{
						t.tcollisionwithphysics=1;
					}
					else
					{
						t.tcollisionwithphysics=2;
					}
				}
			}
		}
		if ( t.tcollisionwithphysics>0 ) 
		{
			if ( t.tcollisionwithphysics == 1 ) 
			{
				// found hit woth physics shapes instead of geometry
				t.brayx2_f=ODEGetRayCollisionX();
				t.brayy2_f=ODEGetRayCollisionY();
				t.brayz2_f=ODEGetRayCollisionZ();
				vecRayHitNormal = GGVECTOR3(ODEGetRayNormalX(), ODEGetRayNormalY(), ODEGetRayNormalZ());
				t.bulletraylimbhit=0;
				if (t.tfoundentityindexhit != -1)
				{
					if (!t.entityelement[t.tfoundentityindexhit].eleprof.bUseFPESettings &&
						t.entityelement[t.tfoundentityindexhit].eleprof.iMaterialSoundIndex > 0 )
					{
						t.tmaterialvalue = t.entityelement[t.tfoundentityindexhit].eleprof.iMaterialSoundIndex;
					}
					else
						t.tmaterialvalue = t.entityprofile[t.tentid].materialindex;
				}
			}
			else
			{
				// hit geometry but missed physics shape, no collision (foliage banana tree)
				t.bulletrayhit=-1;
				t.bulletraylimbhit=-1;
			}
		}
		else
		{
			// shorten ray to reflect hit coordinate
			t.brayx2_f=ChecklistFValueA(6);
			t.brayy2_f=ChecklistFValueB(6);
			t.brayz2_f=ChecklistFValueC(6);
			vecRayHitNormal = GGVECTOR3(ChecklistFValueA(7), ChecklistFValueB(7), ChecklistFValueC(7));

			// get limb we hit (for flinch effect when we hit enemy limb)
			t.tlimbhit=ChecklistValueB(1);

			// return material index and use to trigger decal
			// ChecklistValueA 9 not reliable, get material from entity properties!
			if (t.tfoundentityindexhit != -1)
			{
				if (!t.entityelement[t.tfoundentityindexhit].eleprof.bUseFPESettings &&
					t.entityelement[t.tfoundentityindexhit].eleprof.iMaterialSoundIndex > 0)
				{
					t.tmaterialvalue = t.entityelement[t.tfoundentityindexhit].eleprof.iMaterialSoundIndex;
				}
				else
					t.tmaterialvalue = t.entityprofile[t.tentid].materialindex;
			}
			else
			{
				t.tmaterialvalue = ChecklistValueA(9);
			}

			// check if we hit character creator head and adjust limbhit to the head of the character
			// simpler head shot detection (gun and other things can get in the way)
			if (t.tfoundentityindexhit != -1)
			{
				if (t.entityprofile[t.tentid].ischaracter == 1)
				{
					float fHitY = ChecklistFValueB(6);
					float fPercentageThatIsNotHead = 0.8f;
					if (fHitY > ObjectPositionY(t.thitvalue) + ObjectSizeY(t.thitvalue, 1) * fPercentageThatIsNotHead)
					{
						// head
						t.tlimbhit = getlimbbyname(t.thitvalue, "Bip01_Head");
					}
					else
					{
						fPercentageThatIsNotHead = 0.35f;
						if (fHitY > ObjectPositionY(t.thitvalue) + ObjectSizeY(t.thitvalue, 1) * fPercentageThatIsNotHead)
						{
							// pelvis
							t.tlimbhit = getlimbbyname(t.thitvalue, "Bip01_Pelvis");
						}
						else
						{
							// foot
							t.tlimbhit = getlimbbyname(t.thitvalue, "Bip01_R_Foot");
						}
					}
					if (!t.entityelement[t.tfoundentityindexhit].eleprof.bUseFPESettings &&
						t.entityelement[t.tfoundentityindexhit].eleprof.iMaterialSoundIndex > 0)
					{
						t.tmaterialvalue = t.entityelement[t.tfoundentityindexhit].eleprof.iMaterialSoundIndex;
					}
					else
						t.tmaterialvalue = t.entityprofile[t.tentid].materialindex;
				}
			}

			// reset and assign detectedlimbhit flag for script
			if ( t.tfoundentityindexhit != -1 ) 
			{
				t.entityelement[t.tfoundentityindexhit].detectedlimbhit = 0;
				t.entityelement[t.tfoundentityindexhit].lua.flagschanged = 1;
			}
			if (  t.tlimbhit>0 ) 
			{
				if (  ObjectExist(t.bulletrayhit) == 1 ) 
				{
					if (  LimbExist(t.bulletrayhit,t.tlimbhit) == 1 ) 
					{
						// record which limb we hit
						t.bulletraylimbhit = t.tlimbhit;

						// 201115 - also record limb hit within entity (so LUA can do stuff with the info)
						if ( t.tfoundentityindexhit != -1 ) 
						{
							t.entityelement[t.tfoundentityindexhit].detectedlimbhit = t.tlimbhit;
							t.entityelement[t.tfoundentityindexhit].lua.flagschanged = 1;
						}
					}
				}
			}
		}
		if ( t.tmaterialvalue >= 0 ) t.tttriggerdecalimpact = 10+t.tmaterialvalue;

		// for the player's hit record: an entity, another object, or passed through (collisionmode 11 or the physics shape missed)
		if ( t.tfoundentityindexhit > 0 ) iPlayerHitE = t.tfoundentityindexhit;
		if ( t.tcollisionwithphysics == 2 )
			iPlayerHitClass = 4;
		else
			iPlayerHitClass = ( iPlayerHitE > 0 ) ? 2 : 3;
	}

	// ensure material index never goes negative
	if ( t.tmaterialvalue < 0 ) t.tmaterialvalue = 0;

	//  calculate increment along ray
	t.tbix_f=t.brayx2_f-t.brayx1_f;
	t.tbiy_f=t.brayy2_f-t.brayy1_f;
	t.tbiz_f=t.brayz2_f-t.brayz1_f;
	t.trange_f=Sqrt(abs(t.tbix_f*t.tbix_f)+abs(t.tbiy_f*t.tbiy_f)+abs(t.tbiz_f*t.tbiz_f));
	t.tbix_f=t.tbix_f/t.trange_f;
	t.tbiy_f=t.tbiy_f/t.trange_f;
	t.tbiz_f=t.tbiz_f/t.trange_f;

	//  if bullet ray passed waterlevel, create a splash at intersection
	if (  t.hardwareinfoglobals.nowater == 0 ) 
	{
		if (t.decalglobal.splashdecalrippleid != 0 && ((t.brayy1_f > t.terrain.waterliney_f && t.brayy2_f < t.terrain.waterliney_f) || (t.brayy1_f<t.terrain.waterliney_f && t.brayy2_f>t.terrain.waterliney_f)))
		{
			//  calculate coordate where ray hit water plane
			t.tperc_f=(t.brayy1_f-t.terrain.waterliney_f)/abs(t.tbiy_f);
			t.tbx_f=t.brayx1_f+(t.tbix_f*t.tperc_f);
			t.tby_f=t.brayy1_f+(t.tbiy_f*t.tperc_f);
			t.tbz_f=t.brayz1_f+(t.tbiz_f*t.tperc_f);
			//  check if this coord ABOVE terrain Floor (  )
			t.tgroundheight_f=BT_GetGroundHeight(t.terrain.TerrainID,t.tbx_f,t.tbz_f);
			if (  t.tby_f>t.tgroundheight_f ) 
			{
				//  trigger water splash at coords
				g.decalx=t.tbx_f ; g.decaly=t.tby_f+0.5 ; g.decalz=t.tbz_f; t.tInScale_f = 1;
				decal_triggerwatersplash ( );
				//  play splash sound
				t.tmatindex=17; 
				t.tsoundtrigger = t.material[t.tmatindex].matsound_id[matSound_LandHard][0];
				t.tvol_f = 75;
				t.tspd_f=(t.material[t.tmatindex].freq*1.5)+Rnd(t.material[t.tmatindex].freq)*0.5;
				t.tsx_f=g.decalx ; t.tsy_f=g.decaly ; t.tsz_f=g.decalz;
				material_triggersound ( 0 );
				t.tsoundtrigger=0;
			}
		}
	}

	// check if we hit a character
	if (  t.bulletrayhit>0 ) 
	{
		if (  ObjectExist(t.bulletrayhit) == 1 ) 
		{
			// Find which entity this is
			t.bulletrayhite=-1 ; t.bulletrayhitentid=-1;
			t.tobj=t.bulletrayhit;
			if ( t.tfoundentityindexhit != -1 ) 
			{
				t.tte=t.tfoundentityindexhit ; t.bulletrayhite=t.tte;
				t.bulletrayhitentid=t.entityelement[t.tte].bankindex;
			}
			else
			{
				if ( t.tobj>0 ) 
				{
					if ( ObjectExist(t.tobj) == 1 ) 
					{
						for ( t.tte = 1 ; t.tte <= g.entityelementlist; t.tte++ )
						{
							if ( t.entityelement[t.tte].obj == t.tobj ) 
							{
								t.bulletrayhite=t.tte ; t.bulletrayhitentid=t.entityelement[t.tte].bankindex ; break;
							}
						}
					}
				}
			}

			// Check if this object is a character
			if ( t.bulletrayhitentid != -1 ) 
			{
				t.px_f=t.x1_f ; t.py_f=t.y1_f ; t.pz_f=t.z1_f;
				entity_determinegunforce ( );
				darkai_ischaracterhit ( ); // uses t.bulletfinalstrengthmod,bulletisinfactmeleestrike
				if ( t.darkaifirerayhitcharacter == 0 ) 
				{
					// also make sure it's not a beast (ragdoll)
					t.tokay = 1 ; if ( t.entityprofile[t.bulletrayhitentid].ragdoll == 1 )  t.tokay = 0;
					if ( t.tokay == 1 ) 
					{
						// create a ray of force along bullet tragectory (to disturb non-character objects)
						if ( ODERayForce(t.brayx1_f,t.brayy1_f,t.brayz1_f,t.brayx2_f,t.brayy2_f,t.brayz2_f,t.tforce_f*0.25) == 1 ) 
						{
							// and knock dynamic physics objects if force is applied to them
						}
					}
				}
				if ( t.darkaifirerayhitcharacter == 1 ) 
				{
					// trigger limb flinch system (limbhurt and limbhurta#)
					if ( t.bulletraylimbhit != -1 && t.bulletrayhite != -1 ) 
					{
						t.tte=t.bulletrayhite;
						if (  t.entityelement[t.tte].limbhurt <= 0 ) 
						{
							t.entityelement[t.tte].limbhurt=t.bulletraylimbhit;
							//  determine if entity facing away from plr
							t.tdx_f=ObjectPositionX(t.tobj)-CameraPositionX(0);
							t.tdz_f=ObjectPositionZ(t.tobj)-CameraPositionZ(0);
							t.tangley_f=atan2deg(t.tdx_f,t.tdz_f);
							t.tdiffhurtangle_f=t.tangley_f-ObjectAngleY(t.tobj);
							if ( t.tdiffhurtangle_f<-180  )  t.tdiffhurtangle_f = t.tdiffhurtangle_f+360;
							if ( t.tdiffhurtangle_f>180  )  t.tdiffhurtangle_f = t.tdiffhurtangle_f-360;
							if ( abs(t.tdiffhurtangle_f)<90.0 ) 
							{
								// bend forward
								t.entityelement[t.tte].limbhurta_f=8+Rnd(8);
							}
							else
							{
								// bend back
								t.entityelement[t.tte].limbhurta_f=(8+Rnd(8))*-1;
							}
						}
					}
					// cause blood splat (if violent)
					if ( t.bulletrayhite != -1 ) 
					{
						//PE: But still allow unselect blood effect on some objects :)
						if (t.entityelement[t.bulletrayhite].eleprof.isviolent != 0)
						{
							t.tttriggerdecalimpact = 2;
						}
					}
					t.bulletrayhit=0;
				}
			}
			else
			{
				//  did not hit entity, could be we hit a lightmapped static object
				t.bulletrayhite=0;
			}

			//  determine which entity we hit (if not character which is already handled)
			//  for things such as Zombies and other entities in the level
			if ( t.bulletrayhit > 0 && t.bulletrayhite != -1 ) 
			{
				// apply some damage
				t.tdamagesource = 1;
				t.tdamage = 0;
				if (g.firemodes[t.gunid][g.firemode].settings.damage > 0)
				{
					// ensure DamageMultiplier does not wipe out minimum damage
					t.tdamage = (float)g.firemodes[t.gunid][g.firemode].settings.damage * t.playercontrol.fWeaponDamageMultiplier;
					if (t.playercontrol.fWeaponDamageMultiplier > 0 && t.tdamage < 1) t.tdamage = 1;
				}
				if (t.gun[t.gunid].settings.ismelee == 2 || g.firemodes[t.gunid][g.firemode].settings.usemeleedamageonly > 0)
				{
					if (g.firemodes[t.gunid][0].settings.meleedamage > 0)
					{
						// ensure DamageMultiplier does not wipe out minimum damage
						t.tdamage = (float)g.firemodes[t.gunid][0].settings.meleedamage * t.playercontrol.fMeleeDamageMultiplier;
						if (t.playercontrol.fMeleeDamageMultiplier > 0 && t.tdamage < 1) t.tdamage = 1;
					}
				}
				entity_hitentity ( t.bulletrayhite, t.bulletrayhit );
			}
		}
	}

	// if hitting a material, leave a bullethole (not for melee combat)
	int iPlayerHitHole = 0;
	if (t.gun[t.gunid].settings.ismelee == 0 && g.firemodes[t.gunid][g.firemode].settings.noscorch == 0)
	{
		if (t.tttriggerdecalimpact >= 10 && t.tttriggerdecalimpact != 16 && t.bulletrayhite >= 0 )
		{
			if (t.bulletrayhite == 0 || entity_allowsbulletholes(t.bulletrayhite) )
			{
				int iMaterialIndex = t.tttriggerdecalimpact - 10;
				bulletholes_add(iMaterialIndex, t.brayx2_f, t.brayy2_f, t.brayz2_f, vecRayHitNormal.x, vecRayHitNormal.y, vecRayHitNormal.z, t.bulletrayhite);
				if (iMaterialIndex > 0) iPlayerHitHole = 1;
			}
		}
	}

	// trigger decal at impact coordinate
	//entity_triggerdecalatimpact ( t.brayx2_f, t.brayy2_f, t.brayz2_f );
	t.tfromtheplayer = 1;
	if (t.bulletrayhite > 0)
	{
		// if entity producing decal, ensure the right one being used
		if (t.gun[t.gunid].weapontype >= 51 || t.gun[t.gunid].settings.ismelee != 0)
		{
			//PE: If melee make sure we did hit somethiong, t.bulletrayhite is resused so cant be used.
			if (t.bulletrayhit > 0)
			{
				entity_applydecalfordamage(t.bulletrayhite, t.brayx2_f, t.brayy2_f, t.brayz2_f);
			}
			else
			{
				// default logic
				entity_triggerdecalatimpact(t.brayx2_f, t.brayy2_f, t.brayz2_f);
			}
		}
		else 
		{
			entity_applydecalfordamage(t.bulletrayhite, t.brayx2_f, t.brayy2_f, t.brayz2_f);
		}
	}
	else
	{
		// default logic
		entity_triggerdecalatimpact (t.brayx2_f, t.brayy2_f, t.brayz2_f);
	}
	t.tfromtheplayer = 0;

	// the player's hit record (when gun_shoot_oneray has an entry open): what this ray met
	playerhit_setray(iPlayerHitClass, iPlayerHitE, t.brayx2_f, t.brayy2_f, t.brayz2_f, vecRayHitNormal.x, vecRayHitNormal.y, vecRayHitNormal.z, t.tmaterialvalue, iPlayerHitHole, t.bulletraylimbhit);
}

void entity_hitentity ( int e, int obj )
{
	int iStoreE = t.ttte; t.ttte = e;
	if ( t.entityelement[t.ttte].health > 0 ) 
	{
		//  turn into clone in case we need to animate it (could be in distance)
		if ( GetNumberOfFrames(obj) > 0 ) 
		{
			entity_converttoclone ( );
		}

		// determine and set damage force
		entity_determinegunforce ( );
		t.ttentid=t.entityelement[t.ttte].bankindex;
		if (  t.entityprofile[t.ttentid].ischaracter == 1 || t.entityprofile[t.ttentid].ragdoll == 1 ) 
		{
			t.tdamageforce=t.tforce_f;
		}
		else
		{
			t.tdamageforce=0;
		}

		// apply the damage
		entity_applydamage ( );

		// check if we hit an organic or custom character (animal, zombie, etc)
		t.ttentid = t.entityelement[t.ttte].bankindex;

		// cause blood splat
		if ( t.entityprofile[t.ttentid].materialindex == 6 || ( t.entityprofile[t.ttentid].materialindex == 0 && t.entityprofile[t.ttentid].ischaracter == 1 ) ) 
		{
			if ( t.entityelement[t.ttte].eleprof.isviolent != 0 && g.quickparentalcontrolmode != 2 )
			{
				t.tttriggerdecalimpact=2;
			}
		}

		//  cause blood splat on steam multiplayer char
		if ( t.game.runasmultiplayer == 1 ) 
		{
			if ( t.entityprofile[t.ttentid].ismultiplayercharacter == 1 ) 
			{
				if ( t.entityelement[t.ttte].eleprof.isviolent != 0 && g.quickparentalcontrolmode != 2 )
				{
					t.tttriggerdecalimpact=2;
				}
			}
		}
	}
	t.ttte = iStoreE;
}

void entity_triggerdecalatimpact ( float fX, float fY, float fZ )
{
	// trigger decal at impact coordinate
	// 111215 - and only if ignorematerial flag not set by GUNSPEC (for interactive HUD 'weapons')
	if ( t.tttriggerdecalimpact>0 && (t.gunid==0 || g.firemodes[t.gunid][0].settings.ignorematerial == 0) )
	{
		//  trigger decal animation at coords
		g.decalx=fX; g.decaly=fY+0.5 ; g.decalz=fZ;
		if ( t.tttriggerdecalimpact >= 10 ) decal_triggermaterialdecal ( );
		if ( t.tttriggerdecalimpact == 1 ) decal_triggerimpact ( );
		if ( t.tttriggerdecalimpact == 2 ) 
		{
			if ( t.playercontrol.startviolent != 0 && g.quickparentalcontrolmode != 2 ) 
			{
				t.decalid = t.decalglobal.bloodsplatid; t.decalorient = 0;
				if (t.decal[t.decalid].newparticle.bWPE)
				{
					t.originatore = -1;
					t.decalscalemodx = 40 + Rnd(20); t.decalscalemody = 40 + Rnd(20);
					t.decalforward = 0;
					decalelement_create();
				}
				else
				{
					for (t.iter = 1; t.iter <= 3 + Rnd(1); t.iter++)
					{
						decal_triggerbloodsplat();
					}
				}
			}
		}

		// play material impact sound
		t.tmatindex = 0 ; if (  t.tttriggerdecalimpact >= 10  )  t.tmatindex = t.tttriggerdecalimpact-10;
		t.tsoundtrigger = t.material[t.tmatindex].matsound_id[matSound_LandHard][0];
		t.tspd_f=t.material[t.tmatindex].freq;
		t.tsx_f=g.decalx ; t.tsy_f=g.decaly ; t.tsz_f=g.decalz;
		t.tvol_f = 100.0f ; material_triggersound ( 0 );
		t.tsoundtrigger=0;

		// optionally, if player start marker specified an impact sound, and player made this sound, play it here
		if (t.tfromtheplayer==1 && t.tfromtheplayerentityelementid>0)
		{
			if (t.tttriggerdecalimpact == 2)
			{
				// soft (blood)
				t.tsoundtrigger = t.entityelement[t.tfromtheplayerentityelementid].soundset2;
			}
			else
			{
				// hard (all other surfaces)
				t.tsoundtrigger = t.entityelement[t.tfromtheplayerentityelementid].soundset1;
			}
			material_triggersound (0);
			t.tsoundtrigger = 0;
		}
	}
}

void entity_createattachment ( void )
{
	// Single player character must HOLD the weapon before attaching it
	t.tischaracterholdingweapon=0;
	if ( t.entityprofile[t.entid].ischaracter == 1 && t.entityelement[t.e].eleprof.hasweapon>0 ) 
	{
		t.tischaracterholdingweapon=1;
	}

	// Load all VWEAPS for each entity that wants weapon attachments
	t.entid=t.entityelement[t.e].bankindex;
	if ( (t.tischaracterholdingweapon == 1 || t.entityprofile[t.entid].ismultiplayercharacter == 1) && t.entityelement[t.e].obj>0 ) 
	{
		// Make attachment if warranted
		if ( ObjectExist(t.entityelement[t.e].obj) == 1 && t.entityelement[t.e].attachmentobj == 0 ) 
		{
			if ( t.entityprofile[t.entid].firespotlimb>-1 ) 
			{
				// all vweaps (that are active)
				for ( t.tgindex = 1 ; t.tgindex <= g.gunmax; t.tgindex++ )
				{
					if ( t.gun[t.tgindex].activeingame == 1 ) 
					{
						t.tweaponname_s=t.gun[t.tgindex].name_s;
						if ( t.tweaponname_s != "" ) 
						{
							// entity has this gun in their hands
							t.tthasweapon_s = Lower(t.entityelement[t.e].eleprof.hasweapon_s.Get());
							if ( t.tthasweapon_s == t.tweaponname_s.Lower() ) 
							{
								// the gun index
								int iGunID = t.entityelement[t.e].eleprof.hasweapon;

								// go and load this gun (attached to calling entity instance)
								++g.entityattachmentindex;
								t.ttobj=g.entityattachmentsoffset+g.entityattachmentindex;
								if (ObjectExist(t.ttobj) == 1)
								{
									UnGlueObject(t.ttobj);
									DeleteObject (t.ttobj);
								}

								// replaced X file load with optional DBO convert/load
								t.tfile_s="gamecore\\guns\\";
								t.tfile_s += t.tweaponname_s+"\\vweap.x";
								deleteOutOfDateDBO(t.tfile_s.Get());
								if ( cstr(Lower(Right(t.tfile_s.Get(),2))) == ".x"  )  {t.tdbofile_s = Left(t.tfile_s.Get(),Len(t.tfile_s.Get())-2); t.tdbofile_s += ".dbo"; } else t.tdbofile_s = "";
								if (FileExist(t.tfile_s.Get()) == 0 && FileExist(t.tdbofile_s.Get()) == 0)
								{
									// can use new designation for weapons held (for both player and character in new system)
									t.tfile_s = "";
									t.tdbofile_s = "gamecore\\guns\\";
									t.tdbofile_s += t.tweaponname_s + "\\weapon.dbo";
								}
								if (FileExist(t.tfile_s.Get()) == 1 || FileExist(t.tdbofile_s.Get()) == 1)
								{
									if ( FileExist(t.tdbofile_s.Get()) == 1 ) 
									{
										t.tfile_s=t.tdbofile_s;
										t.tdbofile_s="";
									}
									LoadObject ( t.tfile_s.Get(), t.ttobj );

									// Find firespot for this vweap
									t.entityelement[t.e].attachmentobjfirespotlimb = -1; // always use a limb, it in turn uses LimbPosition (which takes reading from glued Wicked object)
									PerformCheckListForLimbs (t.ttobj);
									for (t.tc = 1; t.tc <= ChecklistQuantity(); t.tc++)
									{
										if (cstr(Lower(ChecklistString(t.tc))) == "firespot")
										{
											t.entityelement[t.e].attachmentobjfirespotlimb = t.tc - 1;
											t.tc = ChecklistQuantity() + 1;
										}
									}

									// in ALL events now, there is no FIRESPOT limb, it gets erased below in new system
									if (t.entityelement[t.e].attachmentobjfirespotlimb != -1)
									{
										// can still obtain the offset to place muzzle flasg correctly
										float fMuzzleOffsetX = LimbPositionX(t.ttobj, t.entityelement[t.e].attachmentobjfirespotlimb);
										float fMuzzleOffsetY = LimbPositionY(t.ttobj, t.entityelement[t.e].attachmentobjfirespotlimb);
										float fMuzzleOffsetZ = LimbPositionZ(t.ttobj, t.entityelement[t.e].attachmentobjfirespotlimb);
										t.entityelement[t.e].fFirespotOffsetX = fMuzzleOffsetX;
										t.entityelement[t.e].fFirespotOffsetY = fMuzzleOffsetY;
										t.entityelement[t.e].fFirespotOffsetZ = fMuzzleOffsetZ;
										t.entityelement[t.e].attachmentobjfirespotlimb = -1;
									}
									else
									{
										// else we assume a muzzle flasg ahead of WRIST of weapon (can later add some length to this in gunspec)
										t.entityelement[t.e].fFirespotOffsetX = -t.gun[iGunID].firespotx_f;
										t.entityelement[t.e].fFirespotOffsetY = t.gun[iGunID].firespoty_f;
										t.entityelement[t.e].fFirespotOffsetZ = -t.gun[iGunID].firespotz_f;
									}

									// modify so weapon fits better in hands
									if (t.gun[iGunID].handusesnewweaponsystem == 1)
									{
										// new weapon system, matches weapon pos and rot of player HUD custom hands
										if (LimbExist(t.ttobj, 1) == 1)
											OffsetLimb(t.ttobj, 1, t.gun[iGunID].handposx_f, t.gun[iGunID].handposy_f, t.gun[iGunID].handposz_f);
										else
											OffsetLimb(t.ttobj, 0, t.gun[iGunID].handposx_f, t.gun[iGunID].handposy_f, t.gun[iGunID].handposz_f);

										TurnObjectRight(t.ttobj, t.gun[iGunID].handrotx_f);
										RollObjectRight(t.ttobj, t.gun[iGunID].handroty_f);
										PitchObjectDown(t.ttobj, t.gun[iGunID].handrotz_f);
										MakeMeshFromObject(g.meshgeneralwork2, t.ttobj);
									}
									else
									{
										// old system legacy weapons
										PositionObject (t.ttobj, t.gun[iGunID].handposx_f, t.gun[iGunID].handposy_f, t.gun[iGunID].handposz_f);
										TurnObjectRight(t.ttobj, t.gun[iGunID].handroty_f);
										PitchObjectDown(t.ttobj, t.gun[iGunID].handrotx_f);
										RollObjectRight(t.ttobj, t.gun[iGunID].handrotz_f);
										MakeMeshFromObject(g.meshgeneralwork2, t.ttobj, 11); // account for WorldPos in transforming the mesh
									}

									// remove complex weapon
									DeleteObject(t.ttobj);

									// make simpler single mesh weapon
									MakeObject(t.ttobj, g.meshgeneralwork2, 0);

									// make a firespot debug object
									int iDebugFirespotObj = g.entityattachments2offset + g.entityattachmentindex;
									if (ObjectExist(iDebugFirespotObj) == 1) DeleteObject(iDebugFirespotObj);
									MakeObjectCube(iDebugFirespotObj, 5);
									if (t.gun[iGunID].handusesnewweaponsystem == 1)
									{
										TurnObjectRight(iDebugFirespotObj, t.gun[iGunID].handrotx_f);
										RollObjectRight(iDebugFirespotObj, t.gun[iGunID].handroty_f);
										PitchObjectDown(iDebugFirespotObj, t.gun[iGunID].handrotz_f);
									}
									else
									{
										PositionObject (iDebugFirespotObj, t.gun[iGunID].handposx_f, t.gun[iGunID].handposy_f, t.gun[iGunID].handposz_f);
										TurnObjectRight(iDebugFirespotObj, t.gun[iGunID].handroty_f);
										PitchObjectDown(iDebugFirespotObj, t.gun[iGunID].handrotx_f);
										RollObjectRight(iDebugFirespotObj, t.gun[iGunID].handrotz_f);
									}
									float fFirespotShiftX = t.entityelement[t.e].fFirespotOffsetX;
									float fFirespotShiftY = t.entityelement[t.e].fFirespotOffsetY;
									float fFirespotShiftZ = t.entityelement[t.e].fFirespotOffsetZ;
									MoveObjectRight(iDebugFirespotObj, fFirespotShiftX);
									MoveObjectUp(iDebugFirespotObj, fFirespotShiftY);
									MoveObject(iDebugFirespotObj, fFirespotShiftZ);
									GlueObjectToLimb(iDebugFirespotObj, t.ttobj, 0);
									sObject* pDebugFirespotObj = GetObjectData(iDebugFirespotObj);
									WickedCall_SetObjectRenderLayer(pDebugFirespotObj, GGRENDERLAYERS_CURSOROBJECT);
									HideObject(iDebugFirespotObj);
								}
								else
								{
									MakeObjectTriangle (  t.ttobj,0,0,0,0,0,0,0,0,0 );
								}
								SetObjectDiffuseEx(t.ttobj, 0xFFFFFFFF, 0);
								t.entityelement[t.e].attachmentobj = t.ttobj;

								// VWEAP can choose own texture
								t.tvweaptex_s=t.gun[t.tgindex].vweaptex_s;
								if ( Len(t.tvweaptex_s.Get())<2  )  t.tvweaptex_s = "gun";

								// apply texture to vweap
								if ( g.gdividetexturesize == 0 ) 
								{
									t.texuseid=loadinternaltexture("effectbank\\reloaded\\media\\white_D.dds");
								}
								else
								{
									sprintf ( t.szwork , "gamecore\\guns\\%s\\%s_D.dds" , t.tweaponname_s.Get() , t.tvweaptex_s.Get() );
									t.texuseid=loadinternaltexture(t.szwork);
									if (t.texuseid == 0) 
									{
										sprintf(t.szwork, "gamecore\\guns\\%s\\%s_color.dds", t.tweaponname_s.Get(), t.tvweaptex_s.Get());
										t.texuseid = loadinternaltexture(t.szwork);
										if (t.texuseid == 0)
										{
											// legacy custom weapons could rely on texture loaded with model, no longer the case
											// as the model is recreated from a mesh, so hunt for the gun texture if VWEAP assignment is wrong!
											sprintf(t.szwork, "gamecore\\guns\\%s\\gun_color.dds", t.tweaponname_s.Get());
											t.texuseid = loadinternaltexture(t.szwork);
										}
									}
								}
								TextureObject ( t.ttobj, 0, t.texuseid );

								//PE: fix t.entityelement[t.e].attachmentobj;
								//PE: https://forum.game-guru.com/thread/219491.
								EnableObjectZDepth(t.ttobj);

								// ensure it does not attract a collision hit during ray cast (do before glue)
								sObject* pAttObject = GetObjectData(t.ttobj);
								WickedCall_SetObjectRenderLayer(pAttObject, GGRENDERLAYERS_CURSOROBJECT);

								// scale now from gunspec
								float fHandscale = t.gun[iGunID].handscale_f;
								ScaleObject(t.ttobj, fHandscale, fHandscale, fHandscale);

								// we use wicked to perfectly glue the weapon as a child to the parent
								GlueObjectToLimbEx (t.ttobj, t.entityelement[t.e].obj, t.entityprofile[t.entid].firespotlimb, 4);

								// no need to continue looking thrugh guns
								t.tgindex=g.gunmax; 
								break;
							}
						}
					}
				}
			}
		}
	}

	// pre-spawn weapon drops so to avoid performance hit during the game
	t.entityelement[t.e].precreatedspawnedentityelementindex = 0;
	if (t.playercontrol.thirdperson.enabled == 0)
	{
		t.tobj = t.entityelement[t.e].attachmentobj;
		if (t.tobj > 0)
		{
			if (ObjectExist(t.tobj) == 1)
			{
				// new system spawns weapon object so can be treated like a loot drop
				int iStoree = t.e;
				int iAttachmentObj = t.tobj;
				int iWeaponEntityID = 0;
				t.weaponindex = t.entityelement[t.e].eleprof.hasweapon;
				if (t.weaponindex > 0)
				{
					for (int iWE = 1; iWE <= g.entityelementlist; iWE++)
					{
						int iEntID = t.entityelement[iWE].bankindex;
						if (iEntID > 0)
						{
							if (t.entityprofile[iEntID].isweapon == t.weaponindex)
							{
								iWeaponEntityID = iWE;
								break;
							}
						}
					}
				}
				if (iWeaponEntityID > 0)
				{
					extern int SpawnNewEntityCore(int iEntityIndex);
					float fStoreX = t.entityelement[iWeaponEntityID].x;
					float fStoreY = t.entityelement[iWeaponEntityID].y;
					float fStoreZ = t.entityelement[iWeaponEntityID].z;
					t.entityelement[iWeaponEntityID].x = t.entityelement[t.e].x;
					t.entityelement[iWeaponEntityID].y = t.entityelement[t.e].y;
					t.entityelement[iWeaponEntityID].z = t.entityelement[t.e].z;
					int iNewEntID = SpawnNewEntityCore(iWeaponEntityID);
					t.entityelement[iNewEntID].x = -99999;
					t.entityelement[iNewEntID].y = -99999;
					t.entityelement[iNewEntID].z = -99999;
					int tobj = t.entityelement[iNewEntID].obj;
					PositionObject(tobj, -99999, -99999, 99999);
					HideObject(tobj);
					t.entityelement[t.e].precreatedspawnedentityelementindex = iNewEntID;
				}
			}
		}
	}
}

void entity_freeattachment ( void )
{
	if (  t.entityelement[t.e].attachmentobj>0 ) 
	{
		if (  ObjectExist(t.entityelement[t.e].attachmentobj) == 1 ) 
		{
			HideObject (  t.entityelement[t.e].attachmentobj );
		}
	}
}

void entity_monitorattachments (void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	// handle when player picks up ammo from dead enemies. Assumes ammo pool 1 is the default pool we want to update
	if (t.playercontrol.thirdperson.enabled == 0)
	{
		t.tobj = t.entityelement[t.e].attachmentobj;
		if (t.tobj > 0)
		{
			if (ObjectExist(t.tobj) == 1)
			{
				// weapon drops 
				if (t.entityelement[t.e].health <= 0 && t.entityelement[t.e].eleprof.cantakeweapon != 0)
				{
					if (GetVisible(t.tobj) == 1)
					{
						// new system spawns weapon object so can be treated like a loot drop
						int iStoree = t.e;
						int iAttachmentObj = t.tobj;
						if(t.entityelement[t.e].precreatedspawnedentityelementindex>0)
						{
							// performance hit to spawn in-level, so precreated this weapon drop
							int iNewEntID = t.entityelement[t.e].precreatedspawnedentityelementindex;
							t.entityelement[iNewEntID].x = t.entityelement[t.e].x;
							t.entityelement[iNewEntID].y = t.entityelement[t.e].y;
							t.entityelement[iNewEntID].z = t.entityelement[t.e].z;
							int tobj = t.entityelement[iNewEntID].obj;
							sObject* pNewObject = g_ObjectList[tobj];
							t.entityelement[iNewEntID].y += 1 + fabs(pNewObject->collision.vecCentre.y);
							t.entityelement[iNewEntID].rx = t.entityelement[t.e].rx;
							t.entityelement[iNewEntID].ry = t.entityelement[t.e].ry + Rnd(359);
							t.entityelement[iNewEntID].rz = t.entityelement[t.e].rz;
							t.entityelement[iNewEntID].lua.haskey = 1; // taken over this flag for use with weapon.lua script (auto collect ammo if got weapon)
							t.entityelement[iNewEntID].usingphysicsnow = 0; // collision on below makes the new physics for this drop
							t.entityelement[iNewEntID].eleprof.iOverrideCollisionMode = 0; // and use the box shape for the physics
							t.entityelement[iNewEntID].eleprof.isimmobile = 0; // and free to fall
							t.entityelement[iNewEntID].active = 0;
							t.entityelement[iNewEntID].lua.flagschanged = 123; // cause to call init before normal running
							t.te = iNewEntID; t.tv_f = 0;// t.v_f;
							g.charanimindex = 0;
							entity_updatepos ();
							entity_lua_rotateupdate ();
							t.entityelement[iNewEntID].eleprof.quantity = t.entityelement[t.e].eleprof.quantity;
							t.entityelement[t.e].eleprof.cantakeweapon = 100 + iNewEntID;
							t.e = iNewEntID;
							entity_lua_show();
						}
						t.e = iStoree;
						t.tobj = iAttachmentObj;

						// hide attachment from level
						HideObject (iAttachmentObj);
					}
					else
					{
						// after spawn, sync collectable object with dropped attachment
						if (t.entityelement[t.e].eleprof.cantakeweapon > 100)
						{
							int iNewEntID = t.entityelement[t.e].eleprof.cantakeweapon - 100;
							if (t.entityelement[iNewEntID].collected == 0)
							{
								int tobj = t.entityelement[iNewEntID].obj;
								if (tobj > 0)
								{
									if (ObjectExist(tobj) == 1)
									{
										float fPosX = ObjectPositionX(t.tobj);
										float fPosY = ObjectPositionY(t.tobj);
										float fPosZ = ObjectPositionZ(t.tobj);
										if (fPosX != 0.0f || fPosY != 0.0f || fPosZ != 0.0f)
										{
											t.entityelement[iNewEntID].x = fPosX;
											t.entityelement[iNewEntID].y = fPosY;
											t.entityelement[iNewEntID].z = fPosZ;
											t.entityelement[iNewEntID].rx = ObjectAngleX(t.tobj);
											t.entityelement[iNewEntID].ry = ObjectAngleY(t.tobj);
											t.entityelement[iNewEntID].rz = ObjectAngleZ(t.tobj);
										}
										PositionObject (tobj, t.entityelement[iNewEntID].x, t.entityelement[iNewEntID].y, t.entityelement[iNewEntID].z);
										RotateObject (tobj, t.entityelement[iNewEntID].rx, t.entityelement[iNewEntID].ry, t.entityelement[iNewEntID].rz);
										ShowObject(tobj);
									}
								}
							}
						}
					}
				}
			}
		}

		// loot drops 
		t.tobj = t.entityelement[t.e].obj;
		if (t.tobj > 0)
		{
			if (ObjectExist(t.tobj) == 1)
			{
				int iPercentageChanceOfDroppingAnythingAtAll = t.entityelement[t.e].eleprof.lootpercentage;
				if (t.entityelement[t.e].health <= 0 && iPercentageChanceOfDroppingAnythingAtAll > 0)
				{
					// new system spawns loot objects
					extern int g_iLootListCount;
					extern cstr g_lootList_s[10];
					extern int g_lootListPercentage[10];
					extern void animsystem_createlootlist(cstr);
					animsystem_createlootlist(t.entityelement[t.e].eleprof.ifused_s);
					for (int l = 0; l < g_iLootListCount; l++)
					{
						LPSTR pLootObjName = g_lootList_s[l].Get();
						if (stricmp (pLootObjName, "(Choose Collectible)") != NULL)
						{
							// spawn a clone of the loot object
							int iStoree = t.e;
							int iStoreObj = t.tobj;
							int iLootObjectID = 0;
							for (int iWE = 1; iWE <= g.entityelementlist; iWE++)
							{
								if (t.entityelement[iWE].obj > 0 && t.entityelement[iWE].bankindex > 0)
								{
									if (stricmp (t.entityelement[iWE].eleprof.name_s.Get(), pLootObjName) == NULL)
									{
										// probability of a drop at all
										if ((int)rand() % 100 <= iPercentageChanceOfDroppingAnythingAtAll)
										{
											// probability of THIS item dropping
											if ((int)rand() % 100 <= g_lootListPercentage[l])
											{
												iLootObjectID = iWE;
											}
										}
										break;
									}
								}
							}
							if (iLootObjectID > 0)
							{
								extern int SpawnNewEntityCore(int iEntityIndex);
								float fStoreX = t.entityelement[iLootObjectID].x;
								float fStoreY = t.entityelement[iLootObjectID].y;
								float fStoreZ = t.entityelement[iLootObjectID].z;
								t.entityelement[iLootObjectID].x = ObjectPositionX(t.tobj);
								t.entityelement[iLootObjectID].y = ObjectPositionY(t.tobj);
								t.entityelement[iLootObjectID].z = ObjectPositionZ(t.tobj);
								int iNewEntID = SpawnNewEntityCore(iLootObjectID);
								t.entityelement[iLootObjectID].x = fStoreX;
								t.entityelement[iLootObjectID].y = fStoreY;
								t.entityelement[iLootObjectID].z = fStoreZ;
								int tobj = t.entityelement[iNewEntID].obj;
								sObject* pNewObject = g_ObjectList[tobj];
								if (pNewObject)
								{
									t.entityelement[iNewEntID].y += 1 + fabs(pNewObject->collision.vecCentre.y) + 10.0f;
									t.entityelement[iNewEntID].rx = t.entityelement[t.e].rx;
									t.entityelement[iNewEntID].ry = t.entityelement[t.e].ry + Rnd(359);
									t.entityelement[iNewEntID].rz = t.entityelement[t.e].rz;
									t.entityelement[iNewEntID].eleprof.lootpercentage = 1000;
									PositionObject (tobj, t.entityelement[iNewEntID].x, t.entityelement[iNewEntID].y, t.entityelement[iNewEntID].z);
								}
								t.e = iNewEntID;
								entity_lua_collisionon();
							}
							t.e = iStoree;
							t.tobj = iStoreObj;
						}
					}

					// once chance to droo when no health, then exit loot drop system
					t.entityelement[t.e].eleprof.lootpercentage = 0;
				}
			}
		}
	}
}

void entity_monitorloot (void)
{
	if (t.entityelement[t.e].eleprof.lootpercentage == 1000)
	{
		if (t.entityelement[t.e].collected == 0)
		{
			int iNewEntID = t.e;
			int tobj = t.entityelement[iNewEntID].obj;
			if (tobj > 0)
			{
				if (ObjectExist(tobj) == 1)
				{
					t.entityelement[iNewEntID].x = ObjectPositionX(t.tobj);
					t.entityelement[iNewEntID].y = ObjectPositionY(t.tobj);
					t.entityelement[iNewEntID].z = ObjectPositionZ(t.tobj);
					t.entityelement[iNewEntID].rx = ObjectAngleX(t.tobj);
					t.entityelement[iNewEntID].ry = ObjectAngleY(t.tobj);
					t.entityelement[iNewEntID].rz = ObjectAngleZ(t.tobj);
					ShowObject(tobj);
				}
			}
		}
	}	
}

void entity_converttoclone ( void )
{
	// wicked handles instances inside engine, no need to have clone/instance here
}

void entity_converttoclonetransparent ( void )
{
	// wicked handles instances inside engine, no need to have clone/instance here
}

bool entity_isuniquespecularoruv ( int ee )
{
	return false;
}

void entity_converttoinstance ( void )
{
	// takes tte
	// really needed this when I had ragdoll objects, call it if object has been ragdollified
	if (t.entityelement[t.tte].ragdollified == 1)
	{
		t.tobj=t.entityelement[t.tte].obj;
		if ( t.tobj>0 ) 
		{
			// 101216 - if entity is given unique specular, must be a clone to take effect
			// 020217 - quit early if cannot make this an instance
			bool bUniqueSpecularOrUV = entity_isuniquespecularoruv ( t.tte );
			if ( ObjectExist(t.tobj) == 1 && bUniqueSpecularOrUV == false ) 
			{
				//  first remove any ragdoll
				entity_freeragdoll ();

				//  then delete clone and recreate as instance
				t.tstorevis=GetVisible(t.tobj);
				DeleteObject (  t.tobj );
				t.ttsourceobj=g.entitybankoffset+t.entityelement[t.tte].bankindex;

				WickedSetElementId(t.tte);
				WickedSetEntityId(t.entityelement[t.tte].bankindex);

				// always clone for now (instance work during performance opt)
				CloneObject (t.tobj, t.ttsourceobj, 1);

				WickedSetElementId(0);
				WickedSetEntityId(-1);

				// restore any radius settings the original object might have had
				SetSphereRadius (  t.tobj,-1 );
				t.entityelement[t.tte].isclone=0;
				t.tentid = t.entityelement[t.tte].bankindex;
				entity_prepareobj ( );
				entity_positionandscale ( );
				if (  t.tstorevis == 0  )  HideObject (  t.tobj );
				if ( t.entityprofile[t.tentid].addhandlelimb == 0 )
				{
					// 301115 - override parent LOD distance with LODModifier
					entity_calculateentityLODdistances ( t.tentid, t.tobj, t.entityelement[t.tte].eleprof.lodmodifier );
				}
			}
		}
	}
}

// a clone's steps timed from SpawnProbe_Start to SpawnProbe_Stop (SpawnNewEntityCore); marks outside a spawn do nothing
static bool g_bSpawnProbeActive = false;
static DWORD g_dwSpawnProbeThread = 0;
static LARGE_INTEGER g_SpawnProbeStart, g_SpawnProbeLast;
static double g_dSpawnProbeMs[SPAWNSTEP_COUNT];

// the named slices (SpawnProbe_Sub), each given to the step whose mark follows it, and the counted calls
#define SPAWNPROBE_SLICES 32
#define SPAWNPROBE_CALLS 16
struct sSpawnProbeNamed { const char* pName; int iStep; int iCount; double dMs; };
static sSpawnProbeNamed g_SpawnProbeSlice[SPAWNPROBE_SLICES];
static int g_iSpawnProbeSlices = 0;
static sSpawnProbeNamed g_SpawnProbeCall[SPAWNPROBE_CALLS];
static int g_iSpawnProbeCalls = 0;
static int g_iSpawnProbeCallDepth = 0;

static bool SpawnProbe_Here ( void )
{
	return g_bSpawnProbeActive && GetCurrentThreadId() == g_dwSpawnProbeThread;
}

static double SpawnProbe_MsSince ( LONGLONG llFrom, LARGE_INTEGER* pNow )
{
	LARGE_INTEGER now, freq;
	QueryPerformanceCounter(&now);
	QueryPerformanceFrequency(&freq);
	if (pNow) *pNow = now;
	return (double)(now.QuadPart - llFrom) * 1000.0 / (double)freq.QuadPart;
}

void SpawnProbe_Start ( void )
{
	QueryPerformanceCounter(&g_SpawnProbeStart);
	g_SpawnProbeLast = g_SpawnProbeStart;
	for (int i = 0; i < SPAWNSTEP_COUNT; i++) g_dSpawnProbeMs[i] = 0;
	g_iSpawnProbeSlices = 0;
	g_iSpawnProbeCalls = 0;
	g_iSpawnProbeCallDepth = 0;
	g_dwSpawnProbeThread = GetCurrentThreadId();
	g_bSpawnProbeActive = true;
}

void SpawnProbe_Mark ( int iStep )
{
	if (!SpawnProbe_Here() || iStep < 0 || iStep >= SPAWNSTEP_COUNT) return;
	g_dSpawnProbeMs[iStep] += SpawnProbe_MsSince(g_SpawnProbeLast.QuadPart, &g_SpawnProbeLast);
	// the slices since the last mark are part of this step
	for (int i = 0; i < g_iSpawnProbeSlices; i++)
	{
		if (g_SpawnProbeSlice[i].iStep >= 0) continue;
		g_SpawnProbeSlice[i].iStep = iStep;
		g_dSpawnProbeMs[iStep] += g_SpawnProbeSlice[i].dMs;
	}
}

void SpawnProbe_Sub ( const char* pName )
{
	if (!SpawnProbe_Here() || !pName) return;
	int i = 0;
	while (i < g_iSpawnProbeSlices && (g_SpawnProbeSlice[i].iStep >= 0 || strcmp(g_SpawnProbeSlice[i].pName, pName) != 0)) i++;
	if (i == g_iSpawnProbeSlices)
	{
		if (i == SPAWNPROBE_SLICES) return; // no room: the time stays with the step
		g_SpawnProbeSlice[i].pName = pName;
		g_SpawnProbeSlice[i].iStep = -1;
		g_SpawnProbeSlice[i].iCount = 0;
		g_SpawnProbeSlice[i].dMs = 0;
		g_iSpawnProbeSlices++;
	}
	g_SpawnProbeSlice[i].dMs += SpawnProbe_MsSince(g_SpawnProbeLast.QuadPart, &g_SpawnProbeLast);
	g_SpawnProbeSlice[i].iCount++;
}

// a counted call inside another (an image load's own file checks) counts as part of the outer one
long long SpawnProbe_CallStart ( void )
{
	if (!SpawnProbe_Here()) return 0;
	if (g_iSpawnProbeCallDepth++ > 0) return 0;
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return now.QuadPart;
}

void SpawnProbe_CallEnd ( const char* pName, long long llStart )
{
	if (!SpawnProbe_Here() || g_iSpawnProbeCallDepth <= 0) return;
	g_iSpawnProbeCallDepth--;
	if (llStart == 0 || !pName) return;
	int i = 0;
	while (i < g_iSpawnProbeCalls && strcmp(g_SpawnProbeCall[i].pName, pName) != 0) i++;
	if (i == g_iSpawnProbeCalls)
	{
		if (i == SPAWNPROBE_CALLS) return;
		g_SpawnProbeCall[i].pName = pName;
		g_SpawnProbeCall[i].iStep = -1;
		g_SpawnProbeCall[i].iCount = 0;
		g_SpawnProbeCall[i].dMs = 0;
		g_iSpawnProbeCalls++;
	}
	g_SpawnProbeCall[i].dMs += SpawnProbe_MsSince(llStart, NULL);
	g_SpawnProbeCall[i].iCount++;
}

// a counted call whose time was measured elsewhere (CreateRenderData's sections)
void SpawnProbe_AddCall ( const char* pName, double dMs )
{
	if (!SpawnProbe_Here() || !pName) return;
	int i = 0;
	while (i < g_iSpawnProbeCalls && strcmp(g_SpawnProbeCall[i].pName, pName) != 0) i++;
	if (i == g_iSpawnProbeCalls)
	{
		if (i == SPAWNPROBE_CALLS) return;
		g_SpawnProbeCall[i].pName = pName;
		g_SpawnProbeCall[i].iStep = -1;
		g_SpawnProbeCall[i].iCount = 0;
		g_SpawnProbeCall[i].dMs = 0;
		g_iSpawnProbeCalls++;
	}
	g_SpawnProbeCall[i].dMs += dMs;
	g_SpawnProbeCall[i].iCount++;
}

// "N ms: profile a, add core b [slice c, slice x2 d], ...; file checks n in e ms" for the steps and slices of 0.05 ms or
// more, in order, then the counted calls
void SpawnProbe_Stop ( char* pText, int iSize )
{
	static const char* pNames[SPAWNSTEP_COUNT] = { "profile", "add core", "before the clone", "clone", "animations and shadows",
		"prepare (materials)", "depth and LOD", "position", "lights", "emitter", "flatten", "add rest", "sounds", "physics" };
	g_bSpawnProbeActive = false;
	int iLen = sprintf_s(pText, iSize, "%.1f ms:", SpawnProbe_MsSince(g_SpawnProbeStart.QuadPart, NULL));
	for (int i = 0; i < SPAWNSTEP_COUNT && iLen > 0 && iLen < iSize - 40; i++)
	{
		if (g_dSpawnProbeMs[i] < 0.05) continue;
		iLen += sprintf_s(pText + iLen, iSize - iLen, " %s %.1f", pNames[i], g_dSpawnProbeMs[i]);
		const char* pOpen = " [";
		for (int s = 0; s < g_iSpawnProbeSlices && iLen > 0 && iLen < iSize - 80; s++)
		{
			const sSpawnProbeNamed& slice = g_SpawnProbeSlice[s];
			if (slice.iStep != i || slice.dMs < 0.05) continue;
			if (slice.iCount > 1) iLen += sprintf_s(pText + iLen, iSize - iLen, "%s%s x%d %.1f", pOpen, slice.pName, slice.iCount, slice.dMs);
			else iLen += sprintf_s(pText + iLen, iSize - iLen, "%s%s %.1f", pOpen, slice.pName, slice.dMs);
			pOpen = ", ";
		}
		if (pOpen[0] == ',' && iLen > 0) iLen += sprintf_s(pText + iLen, iSize - iLen, "]");
		if (iLen > 0) iLen += sprintf_s(pText + iLen, iSize - iLen, ",");
	}
	if (iLen > 0 && pText[iLen - 1] == ',') pText[--iLen] = 0;
	const char* pSep = "; ";
	for (int c = 0; c < g_iSpawnProbeCalls && iLen > 0 && iLen < iSize - 80; c++)
	{
		iLen += sprintf_s(pText + iLen, iSize - iLen, "%s%s %d in %.1f ms", pSep, g_SpawnProbeCall[c].pName, g_SpawnProbeCall[c].iCount, g_SpawnProbeCall[c].dMs);
		pSep = ", ";
	}
}

void entity_createobj ( void )
{
	//  takes OBJ, TUPDATEE, TENDIT
	t.sourceobj=g.entitybankoffset+t.tentid;
	if (  ObjectExist(t.sourceobj) == 1 ) 
	{
		if (  t.tupdatee != -1  )  t.entityelement[t.tupdatee].profileobj = t.sourceobj;

		// wicked handles instances inside engine, so always clone
		bool bCreateAsClone = true;

		if (t.tupdatee != -1 && t.entityelement[t.tupdatee].eleprof.bUseFPESettings)
		{
			//PE: Make sure to copy over master object to entity material.
			//PE: This also insures that bUseInstancing is used for all object that have this flag.
			sObject* pMasterObject = g_ObjectList[t.sourceobj];
			int iMasterID = t.entityelement[t.tupdatee].bankindex;
			if (pMasterObject && iMasterID > 0 && iMasterID < t.entityprofile.size())
			{
				Wicked_Copy_Material_To_Grideleprof((void*)pMasterObject, 0, &t.entityelement[t.tupdatee].eleprof);
			}
		}
		extern bool bUseInstancing;
		extern int iUseMasterObjectID;
		extern bool bNextObjectMustBeClone;
		//PE: InstanceObject - If this object has a custom "Materials" always create as clone.
		if (t.tupdatee != -1)
		{
			WickedSetElementId(t.tupdatee);
			WickedSetEntityId(t.entityelement[t.tupdatee].bankindex);
			iUseMasterObjectID = t.sourceobj;

			bUseInstancing = true; //PE: Need to exclude animated/cpp/ profile wematerial!= entityele wematerial.
			if (bNextObjectMustBeClone) bUseInstancing = false;
			//if (t.entityprofile[t.tentid].ismarker != 0 || t.entityprofile[t.tentid].cpuanims != 0 || t.entityprofile[t.gridentity].isebe != 0 ) bUseInstancing = false;
			if (t.entityprofile[t.tentid].ismarker != 0 || t.entityprofile[t.tentid].cpuanims != 0 || t.entityprofile[t.tentid].isebe != 0) bUseInstancing = false;
			if (t.entityprofile[t.tentid].ischaractercreator == 1) bUseInstancing = false;
			if (t.entityprofile[t.tentid].animmax > 0)  bUseInstancing = false;
			if (t.entityprofile[t.tentid].bIsDecal)  bUseInstancing = false;

			extern int active_tools_obj;
			extern int active_tools_entity_index;
			extern sObject* g_selected_editor_object;
			extern int g_selected_editor_objectID;

			//PE: Cursor always real clone , if changing materials ...
			if (t.widget.pickedEntityIndex == t.tupdatee || t.obj == 70000 || t.gridentityobj == t.obj || active_tools_obj == t.obj || active_tools_entity_index == t.tupdatee || t.tentitytoselect == t.tupdatee)
			{
				bUseInstancing = false;
			}
			else if(bUseInstancing)
			{
				if (t.entityelement[t.tupdatee].eleprof.WEMaterial.MaterialActive)
				{
					sObject* pObject = g_ObjectList[t.sourceobj];
					if (pObject && pObject->iMeshCount < MAXMESHMATERIALS) //PE: Max 100 custom materials available.
					{
						entityeleproftype mastereleprof;
						//PE: Create a mastereleprof from master object, just like it would look in entityelement[t.tupdatee].eleprof.
						//PE: We need to do this as entity_prepareobj is called after the cloneobj is actually created.
						//PE: This will make that many more object is included in instancing.
						Wicked_Copy_Material_To_Grideleprof((void*)pObject, 0, &mastereleprof);
						WickedMaterial *Master_WEMaterial = NULL;
						Master_WEMaterial = &mastereleprof.WEMaterial;
						DWORD dwMeshCount = pObject->iMeshCount;
						for (int iMesh = 0; iMesh < (int)dwMeshCount; iMesh++)
						{
							//PE: Surface is different , and could match entityprofile only ?
							if (Master_WEMaterial->surfaceMapName[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.surfaceMapName[iMesh] &&
								t.entityprofile[t.tentid].WEMaterial.surfaceMapName[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.surfaceMapName[iMesh])
							{
								bUseInstancing = false;
								break;
							}

							if (Master_WEMaterial->baseColorMapName[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.baseColorMapName[iMesh] ||
								Master_WEMaterial->normalMapName[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.normalMapName[iMesh])
							{
								bUseInstancing = false;
								break;
							}
							if (Master_WEMaterial->displacementMapName[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.displacementMapName[iMesh] ||
								Master_WEMaterial->emissiveMapName[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.emissiveMapName[iMesh])
							{
								bUseInstancing = false;
								break;
							}

							//PE: bDoubleSided different first set later. so check both.
							if (Master_WEMaterial->bDoubleSided[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.bDoubleSided[iMesh] &&
								t.entityprofile[t.tentid].WEMaterial.bDoubleSided[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.bDoubleSided[iMesh] )
							{
								bUseInstancing = false;
								break;
							}

							if (Master_WEMaterial->bTransparency[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.bTransparency[iMesh] ||
								Master_WEMaterial->fRenderOrderBias[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fRenderOrderBias[iMesh] ||
								#ifdef CUSTOMSHADERS
								Master_WEMaterial->customShaderID != t.entityelement[t.tupdatee].eleprof.WEMaterial.customShaderID ||
								#endif
								Master_WEMaterial->bPlanerReflection[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.bPlanerReflection[iMesh] ||
								Master_WEMaterial->bCastShadows[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.bCastShadows[iMesh])
							{
								bUseInstancing = false;
								break;
							}
							//PE: Set later so make double check.
							if (Master_WEMaterial->fReflectance[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fReflectance[iMesh] &&
								t.entityprofile[t.tentid].WEMaterial.bDoubleSided[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fReflectance[iMesh])
							{
								bUseInstancing = false;
								break;
							}
							//PE: fMetallness different first set later. so check both.
							if (Master_WEMaterial->fMetallness[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fMetallness[iMesh] &&
								t.entityprofile[t.tentid].WEMaterial.fMetallness[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fMetallness[iMesh])
							{
								bUseInstancing = false;
								break;
							}
							//PE: fRoughness different first set later. so check both.
							if (Master_WEMaterial->fRoughness[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fRoughness[iMesh] &&
								t.entityprofile[t.tentid].WEMaterial.fRoughness[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fRoughness[iMesh])
							{
								bUseInstancing = false;
								break;
							}

							if (
								Master_WEMaterial->dwBaseColor[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.dwBaseColor[iMesh] ||
								Master_WEMaterial->dwEmmisiveColor[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.dwEmmisiveColor[iMesh] ||
								Master_WEMaterial->fNormal[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fNormal[iMesh] ||
								Master_WEMaterial->fAlphaRef[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fAlphaRef[iMesh] ||
								Master_WEMaterial->fEmissive[iMesh] != t.entityelement[t.tupdatee].eleprof.WEMaterial.fEmissive[iMesh] )
							{
								bUseInstancing = false;
								break;
							}

						}
					}
					else
					{
						bUseInstancing = false;
					}
				}
			}
		}


		SpawnProbe_Mark(SPAWNSTEP_BEFORECLONE);
		if ( bCreateAsClone == true )
		{
			CloneObject (  t.obj,t.sourceobj,1 );
			if (  t.tupdatee != -1  )  t.entityelement[t.tupdatee].isclone = 1;
		}
		else
		{
			InstanceObject (  t.obj,t.sourceobj );
			if (  t.tupdatee != -1  )  t.entityelement[t.tupdatee].isclone = 0;
		}
		SpawnProbe_Mark(SPAWNSTEP_CLONE);
		iUseMasterObjectID = 0;
		bUseInstancing = false;
		WickedSetEntityId(-1);
		WickedSetElementId(0);

		//LB: incorporate overrideanimset into object creation step (during editing/loading/etc)
		if (t.obj > 0)
		{
			LPSTR pOverrideAnimSet = t.entityelement[t.tupdatee].eleprof.overrideanimset_s.Get();
			if (strlen(pOverrideAnimSet) > 1) // "" = default to weapon type, "-" = default to object anim
			{
				// replace actual object animations
				if (FileExist(pOverrideAnimSet) == 1)
				{
					sObject* pObject = GetObjectData(t.obj);
					AppendObject(pOverrideAnimSet, t.obj, 0);
					WickedCall_RefreshObjectAnimations(pObject, pObject->wickedloaderstateptr);
				}
			}
		}

		if (t.entityprofile[t.tentid].islightmarker == 1)
		{
			sObject* pObject = g_ObjectList[t.obj];
			if (pObject)
				WickedCall_SetObjectCastShadows(pObject, false);
			t.entityprofile[t.tentid].castshadow = -1;
		}
		//  restore any radius settings the original object might have had
		SetSphereRadius (  t.obj,-1 );

		//  ensure new object ONLY interacts with main camera and shadow camera
		//  (until postprocess masks kick in)
		if (  t.entityprofile[t.tentid].ismarker != 0 ) 
		{
			SetObjectMask (  t.obj,1 );
		}
		else
		{
			SetObjectMask (  t.obj,1+(1<<31) );
		}

		//  initially prep any objects with animation
		if (  GetNumberOfFrames(t.obj)>0 ) 
		{
			SetObjectFrame (  t.obj,0  ); LoopObject (  t.obj  ); StopObject (  t.obj );
		}

		//  allow first animation
		if (t.entityprofile[t.tentid].startanimingame > 0 && t.entityprofile[t.tentid].animmax>0 ) { //PE:

			t.q = t.entityprofile[t.tentid].startanimingame - 1;
			LoopObject(t.sourceobj, t.entityanim[t.tentid][t.q].start, t.entityanim[t.tentid][t.q].finish);
			if (GetNumberOfFrames(t.obj) > 0)
			{
				LoopObject(t.obj, t.entityanim[t.tentid][t.q].start, t.entityanim[t.tentid][t.q].finish);
				StopObject(t.obj);
			}
			StopObject(t.sourceobj);
		}
		else if (  t.entityprofile[t.tentid].animmax>0 && t.entityprofile[t.tentid].playanimineditor>0 ) 
		{
			// animation chosen
			t.q=t.entityprofile[t.tentid].playanimineditor-1;

			// play through "parent object" (ONE OFF)
			LoopObject ( t.sourceobj, t.entityanim[t.tentid][t.q].start,t.entityanim[t.tentid][t.q].finish );

			// 060217 - and clone object if so
			if ( GetNumberOfFrames(t.obj) > 0 ) 
			{
				LoopObject ( t.obj, t.entityanim[t.tentid][t.q].start, t.entityanim[t.tentid][t.q].finish );
			}
		}
		else if (t.entityprofile[t.tentid].playanimineditor < 0)
		{
			// uses name instead of index, the negative is the ordinal into the animset
			extern void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname);
			entity_loop_using_negative_playanimineditor(t.tupdatee, t.obj, t.entityprofile[t.tentid].playanimineditor_name);
		}

		SpawnProbe_Mark(SPAWNSTEP_ANIMATIONS);
		//  SetObject (  properties )
		t.tobj=t.obj ; t.tte=t.tupdatee ; entity_prepareobj ( );
		SpawnProbe_Mark(SPAWNSTEP_PREPARE);

		// prepare correct depth mode
		entity_preparedepth(t.tentid, t.tobj);

		if (  t.tupdatee != -1  ) 
		{
			if ( t.entityprofile[t.tentid].addhandlelimb == 0 )
			{
				// 301115 - override parent LOD distance with LODModifier
				entity_calculateentityLODdistances ( t.tentid, t.tobj, t.entityelement[t.tupdatee].eleprof.lodmodifier );
			}
		}
		SpawnProbe_Mark(SPAWNSTEP_DEPTHLOD);
	}
	else
	{
		//  debug sphere when object not found
		MakeObjectCube (  t.obj,25 );
		SetObjectCollisionOff (  t.obj );
		SetAlphaMappingOn (  t.obj,100 );
	}
}

void entity_updatelightobjtype (int obj, int spotlighting)
{
	if (obj > 0)
	{
		if (LimbExist(obj, 3) == 1)
		{
			if (spotlighting == 0)
			{
				// point light
				ShowLimb(obj, 0);
				ShowLimb(obj, 1);
				ShowLimb(obj, 2);
				HideLimb(obj, 3);
			}
			else
			{
				// spot light
				HideLimb(obj, 0);
				HideLimb(obj, 1);
				HideLimb(obj, 2);
				ShowLimb(obj, 3);
			}
		}
	}
}

void entity_updatelightobj ( int e, int obj )
{
	// adjusts object to show correct shape and color for this light
	if (e > 0 && obj > 0)
	{
		int entid = t.entityelement[e].bankindex;
		//PE: While we have it as a cursor object, entid = 0 so use t.gridentity.
		if (entid == 0 && t.refreshgrideditcursor == 1 && e < t.entityelement.size() && t.gridentity > 0)
			entid = t.gridentity;

		if (entid > 0 && t.entityprofile[entid].ismarker == 2)
		{
			// change color based on color
			SetObjectDiffuse(obj, t.entityelement[e].eleprof.light.color);
			SetObjectEmissive(obj, Rgb(0, 0, 0));

			// change shape based on type
			int spotlighting = t.entityelement[e].eleprof.usespotlighting;
			entity_updatelightobjtype(obj, spotlighting);
		}
	}
}

void entity_preparedepth( int entid, int obj)
{
	if (t.entityprofile[entid].zdepth == 0)
	{
		// simply clears zdepth but keeps layer
		DisableObjectZDepthEx (obj, 1);
	}
	else
	{
		if (t.entityprofile[entid].zdepth == 2)
		{
			// new zdepth mode which DOES move the render order (same render order as weapons so cannot penetrate geometry)
			DisableObjectZDepthEx (obj, 0);
		}
		else
		{
			// normal zdepth handling
			EnableObjectZDepth (obj);
		}
	}
}

