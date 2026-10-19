void smoothanimupdate (int obj)
{
	if (t.smoothanim[obj].transition > 0)
	{
		t.smoothanim[obj].transition = t.smoothanim[obj].transition - 1;
		if (t.smoothanim[obj].transition == 0)
		{
			// for MAX we operate a smooth transition system
			SetObjectInterpolation (obj, 1.0);
			if (t.smoothanim[obj].playflag == 1)
			{
				if (t.smoothanim[obj].playstarted == 0)
				{
					PlayObject (obj, t.smoothanim[obj].st, t.smoothanim[obj].fn);
					if (t.smoothanim[obj].rev == 0)
					{
						if (t.smoothanim[obj].startat > 0)
						{
							sObject* pObject = GetObjectData(obj);
							WickedCall_SetObjectFrameEx(pObject, t.smoothanim[obj].startat);
							t.smoothanim[obj].startat = 0;
						}
						SetObjectSpeed (obj, abs(GetSpeed(obj)));
					}
					else
					{
						SetObjectSpeed (obj, abs(GetSpeed(obj))*-1);
						SetObjectFrame (obj, t.smoothanim[obj].fn);
					}
					t.smoothanim[obj].playstarted = 1;
				}
			}
			else
			{
				LoopObject ( obj, t.smoothanim[obj].st, t.smoothanim[obj].fn);
				if (t.smoothanim[obj].startat > 0)
				{
					sObject* pObject = GetObjectData(obj);
					WickedCall_SetObjectFrameEx(pObject, t.smoothanim[obj].startat);
					t.smoothanim[obj].startat = 0;
				}
				if (t.smoothanim[obj].rev == 0)
				{
					SetObjectSpeed (obj, abs(GetSpeed(obj)));
				}
				else
				{
					SetObjectSpeed (obj, abs(GetSpeed(obj))*-1);
				}
			}
		}
	}
	else
	{
		if (t.smoothanim[obj].playflag == 1)
		{
			if (t.smoothanim[obj].playstarted == 1)
			{
				sObject* pObject = GetObjectData(obj);
				float fCurrentFrame = WickedCall_GetObjectFrame(pObject);
				if (fCurrentFrame >= t.smoothanim[obj].fn)
				{
					StopObject(obj);
					fCurrentFrame = t.smoothanim[obj].fn;
					WickedCall_SetObjectFrameEx(pObject, fCurrentFrame);
				}
			}
		}
	}
}

int darkai_canshoot (void)
{
	// takes tcharanimindex, takes charanimstate
	int iCanShootNow = 0;

	// if target player, consider
	if (t.charanimstate.entityTarget == 0)
	{
		// recalc PLRVISIBLE to ensure the enemy can truly STILL see PLAYER
		if (t.entityelement[t.charanimstate.e].bPlrVisibleCheckDone == false)
		{
			darkai_calcplrvisible(t.charanimstate);
			t.entityelement[t.charanimstate.e].bPlrVisibleCheckDone = true;
		}
	}

	// if want to shoot, can override firesound in use (otherwise can wait 7 seconds while sound fades)
	t.tpermitanoverride = 0;
	if (t.charanimstate.firesoundindex > 0 && MAXTimer() > (int)t.charanimstate.firesoundstarted + 50)  t.tpermitanoverride = 1;
	bool bProceed = false;
	if (t.charanimstate.entityTarget == 0 && ((t.charanimstate.firesoundindex == 0 || t.tpermitanoverride == 1) && t.entityelement[t.charanimstate.e].plrvisible == 1)) bProceed = true;
	if (t.charanimstate.entityTarget > 0  && ((t.charanimstate.firesoundindex == 0 || t.tpermitanoverride == 1)) ) bProceed = true;

	// reintroduce rate of fire
	if (bProceed == true)
	{
		int iAbleToFire = 0;
		if (t.entityelement[t.charanimstate.e].eleprof.rateoffire > 100)
		{
			int iRandomChance = Rnd((t.entityelement[t.charanimstate.e].eleprof.rateoffire - 100) / 5);
			if (iRandomChance <= 1) iAbleToFire = 1;
		}
		else
		{
			iAbleToFire = 1;
		}
		if (iAbleToFire == 0)
		{
			bProceed = false;
		}
	}

	if (bProceed == true)
	{
		// handle player being shot at
		t.te = t.charanimstate.e;
		if (t.te > 0)
		{
			t.tentid = t.entityelement[t.te].bankindex;
			t.tgunid = t.entityelement[t.te].eleprof.hasweapon;
			t.tcannotfirenow = 0;
			t.tattachedobj = t.entityelement[t.te].attachmentobj;
			if (t.tattachedobj > 0)
			{
			}
			if (t.tgunid > 0 && t.tcannotfirenow == 0)
			{
				// frequenty of fire
				t.ttratecalc_f = (1.0 / (1.0 + g.firemodes[t.tgunid][0].settings.firerate))*g.timeelapsed_f*2.0;
				t.ttratecalc_f *= 30.0f;// 10.0f; // increase rate of fire for MAX
				// also to ensure a rate of fire reducing to zero goes faster
				if (t.entityelement[t.charanimstate.e].eleprof.rateoffire < 50)
				{
					float fFasterPlease = 50.0f - t.entityelement[t.charanimstate.e].eleprof.rateoffire;
					t.ttratecalc_f *= (fFasterPlease / 10.0f);
				}
				t.charanimstate.firerateaccumilator = t.charanimstate.firerateaccumilator - t.ttratecalc_f;
				if (t.charanimstate.firerateaccumilator < 0.0)
				{
					t.charanimstate.firerateaccumilator = 0.5 + (Rnd(100) / 100.0);
					iCanShootNow = 1;
				}
			}
		}
	}

	// can we fire now
	return iCanShootNow;
}

void darkai_shoottarget (int targete)
{
	// targete not userd as such, reads from t.charanimstate.entityTarget in darkai_shooteffect
	t.te = t.charanimstate.e;
	t.tentid = t.entityelement[t.te].bankindex;
	if (t.tentid > 0)
	{
		t.tgunid = t.entityelement[t.te].eleprof.hasweapon;
		if (t.tgunid > 0)
		{
			t.ttrr = Rnd(1);
			for (t.tt = t.ttrr + 0; t.tt <= t.ttrr + 1; t.tt++)
			{
				t.ttsnd = t.gunsoundcompanion[t.tgunid][1][t.tt].soundid;
				if (t.ttsnd > 0)
				{
					if (SoundExist(t.ttsnd) == 1)
					{
						if (SoundPlaying(t.ttsnd) == 0 || t.tt == t.ttrr + 1)
						{
							t.toldsndid = t.charanimstate.firesoundindex;
							if (t.toldsndid > 0)
							{
								if (SoundExist(t.toldsndid) == 1)
								{
									StopSound (t.toldsndid);
								}
							}

							t.charanimstate.firesoundindex = t.ttsnd; t.tt = 3;
							t.tfireloopend = g.firemodes[t.tgunid][0].sound.fireloopend;
							t.charanimstate.firesoundstarted = MAXTimer();
							if (t.tfireloopend > 0)
							{
								// sound loops (need to cap it off)
								t.charanimstate.firesoundexpiry = MAXTimer() + 200 + Rnd(200);
							}
							else
							{
								if (t.tfireloopend < 0)
								{
									// when fireloop is negative, we use 'single instance' shots
									// and use negative value as MS time between instance plays
									// need to simulate how player weapon works for EA for now
									t.charanimstate.firesoundexpiry = MAXTimer() + 500;// (fabs(t.tfireloopend));
								}
								else
								{
									// can let sound fade out slowly naturally
									t.charanimstate.firesoundexpiry = MAXTimer() + 5000;
								}
							}
						}
					}
				}
			}
			if (t.charanimstate.firesoundindex > 0)
			{
				// shoot effects
				t.tattachedobj = t.entityelement[t.te].attachmentobj;
				darkai_shooteffect ();
			}
		}
	}
}

void darkai_shootplayer (void)
{
	darkai_shoottarget(0);
}

void darkai_shooteffect (void)
{
	// needs tgunid, example; tgunid=entityprofile(tentid).hasweapon
	// needs tattachedobj, example; tattachedobj=entityelement(te).attachmentobj
	// needs te (entityelement index), example; te = e
	// charanimstate.firesoundindex needs to be set, examle; charanimstate.firesoundindex=ttsnd
	// t.charanimstate.entityTarget also used

	// target position
	bool bMuzzleFlashIfPlrTarget = true;
	int ee = t.charanimstate.entityTarget;
	if (ee  > 0)
	{
		t.tplayerx_f = t.entityelement[ee].x;
		t.tplayery_f = t.entityelement[ee].y;
		t.tplayerz_f = t.entityelement[ee].z;

		// added this as it seemed to be missing (ie enemy weapon pointing test was from the characters feet)
		if (t.charanimstate.entityTargetYOffset_f != 0.0f)
			t.tplayery_f += t.charanimstate.entityTargetYOffset_f;
		else
			t.tplayery_f += 65.0f;
	}
	else
	{
		t.tplayerx_f = ObjectPositionX(t.aisystem.objectstartindex);
		t.tplayery_f = ObjectPositionY(t.aisystem.objectstartindex);
		t.tplayerz_f = ObjectPositionZ(t.aisystem.objectstartindex);
	}

	// emit spot flash
	if (t.tattachedobj > 0)
	{
		// best coordinate is firespot on attached gun
		t.tokay = 0;
		t.tattachmentobjfirespotlimb = t.entityelement[t.te].attachmentobjfirespotlimb;
		if (t.tgunid > 0 && t.tattachmentobjfirespotlimb != 0)
		{
			if (t.tattachmentobjfirespotlimb == -1)
			{
				int tentityattachmentindex = t.tattachedobj - g.entityattachmentsoffset;
				int iDebugFirespotObj = g.entityattachments2offset + tentityattachmentindex;
				if (ObjectExist(iDebugFirespotObj) == 1)
				{
					t.tx_f = LimbPositionX(iDebugFirespotObj, 0);
					t.ty_f = LimbPositionY(iDebugFirespotObj, 0);
					t.tz_f = LimbPositionZ(iDebugFirespotObj, 0);
					t.tokay = 1;
				}
			}
			else
			{
				t.tx_f = LimbPositionX(t.tattachedobj, t.tattachmentobjfirespotlimb);
				t.ty_f = LimbPositionY(t.tattachedobj, t.tattachmentobjfirespotlimb);
				t.tz_f = LimbPositionZ(t.tattachedobj, t.tattachmentobjfirespotlimb);
				t.tokay = 1;
			}
		}
		if (t.tokay == 0)
		{
			//  actual gun position is better source coordinate
			t.tx_f = ObjectPositionX(t.tattachedobj);
			t.ty_f = ObjectPositionY(t.tattachedobj);
			t.tz_f = ObjectPositionZ(t.tattachedobj);
		}
	}
	else
	{
		// fallback is entity center
		t.tobj = t.entityelement[t.te].obj;
		t.tx_f = ObjectPositionX(t.tobj);
		t.ty_f = ObjectPositionY(t.tobj) + 50.0;
		t.tz_f = ObjectPositionZ(t.tobj);
	}
	t.tcolr = g.firemodes[t.entityelement[t.te].eleprof.hasweapon][0].settings.muzzlecolorr / 5;
	t.tcolg = g.firemodes[t.entityelement[t.te].eleprof.hasweapon][0].settings.muzzlecolorg / 5;
	t.tcolb = g.firemodes[t.entityelement[t.te].eleprof.hasweapon][0].settings.muzzlecolorb / 5;
	lighting_spotflash_forenemies ();

	// initiate decal
	if (bMuzzleFlashIfPlrTarget == true)
	{
		t.decalid = g.firemodes[t.tgunid][0].decalid;
		g.decalx = t.tx_f; g.decaly = t.ty_f; g.decalz = t.tz_f;
		t.decalscalemodx = 0; t.decalorient = 11;
		t.originatore = -1;
		t.originatorobj = t.tattachedobj;
		// if gunspec does not specify decal forward, apply some so we can see the muzzle flash for characters!
		t.decalforward = g.firemodes[t.tgunid][0].settings.decalforward;
		if (t.decalforward == 0) t.decalforward = 100.0f;
		t.decalforward = t.decalforward * 2.0f;
		if (g.firemodes[t.tgunid][0].action.automatic.s > 0)
		{
			// special instruction for decal to loop X times
			t.decalburstloop = 0;
		}
		else
		{
			t.decalburstloop = 0;
		}
		decalelement_create ();
		t.decalburstloop = 0;
	}

	// emit sound
	// a better system is to create the event between shooter and target, bringing alert position closer to combat
	t.tsx_f = t.entityelement[t.te].x;
	t.tsy_f = t.entityelement[t.te].y;
	t.tsz_f = t.entityelement[t.te].z;
	float fDX = t.tplayerx_f - t.tsx_f;
	float fDY = t.tplayery_f - t.tsy_f;
	float fDZ = t.tplayerz_f - t.tsz_f;
	float fDIst = sqrt(fabs(fDX*fDX) + fabs(fDY*fDY) + fabs(fDZ*fDZ));
	fDX /= 2;
	fDY /= 2;
	fDZ /= 2;
	t.tsx_f += fDX;
	t.tsy_f += fDY;
	t.tsz_f += fDZ;
	t.tradius_f = fDIst;
	if (t.tradius_f < 500) t.tradius_f = 500;
	darkai_makesound ();
	t.ttsnd = t.charanimstate.firesoundindex;
	if (t.ttsnd > 0)
	{
		if (SoundExist(t.ttsnd) == 1)
		{
			t.tfireloopend = 0;

			if (t.tfireloopend > 0)
			{
				PlaySoundOffset (t.ttsnd, t.tfireloopend); 
				LoopSound (t.ttsnd, 0, t.tfireloopend);
			}
			else
			{
				if (t.tfireloopend < 0)
				{
					// need to simulate how player rifle works for EA for now
					LoopSound (t.ttsnd, 0, 5000);
				}
				else
				{
					PlaySound (t.ttsnd);
				}
			}
			PositionSound (t.ttsnd, t.entityelement[t.te].x, t.entityelement[t.te].y, t.entityelement[t.te].z);
			t.tvolume_f = soundtruevolume(95.0);
			SetSoundVolume (t.ttsnd, t.tvolume_f);
			setsoundspeedtunedfor44k (t.ttsnd, 43000 + Rnd(2000));
		}
	}

	// is bullet or flak
	t.tflakid = g.firemodes[t.tgunid][0].settings.flakindex;
	if (t.tflakid == 0)
	{
		// BULLET - determine if bullet hit based on distance (ttdistanceaccuracy# lower is better)
		t.ttdx_f = t.tplayerx_f - t.tx_f;
		t.ttdy_f = t.tplayery_f - t.ty_f;
		t.ttdz_f = t.tplayerz_f - t.tz_f;
		t.ttdd_f = Sqrt(abs(t.ttdx_f*t.ttdx_f) + abs(t.ttdy_f*t.ttdy_f) + abs(t.ttdz_f*t.ttdz_f));
		t.ttdistanceaccuracy_f = t.ttdd_f / 800.0;
		if (t.ttentid > 0)  t.tisnotmpchar = t.entityprofile[t.ttentid].ismultiplayercharacter; else t.tisnotmpchar = 0;
		if (t.aisystem.playerducking == 1)
		{
			if (t.playercontrol.movement == 0)
			{
				t.tchancetohit_f = 4.0;
			}
			else
			{
				t.tchancetohit_f = 12.0;
			}
		}
		else
		{
			if (t.playercontrol.movement == 0)
			{
				t.tchancetohit_f = 2.0;
			}
			else
			{
				t.tchancetohit_f = 6.0;
			}
		}
		// amount of damage to player
		t.tdamage = g.firemodes[t.tgunid][0].settings.damage;
		if (ee > 0)
		{
			if (t.entityelement[ee].obj > 0 && ObjectExist(t.entityelement[ee].obj))
			{
				if (t.gun[t.tgunid].settings.tracer_active)
				{
					XMFLOAT3 tracer_from, tracer_hit;

					tracer_from.x = t.tx_f;
					tracer_from.y = t.ty_f;
					tracer_from.z = t.tz_f;

					int entid = t.entityelement[ee].bankindex;

					float fTorseAreaX = t.entityelement[ee].x;
					float fTorseAreaY = t.entityelement[ee].y + 50;
					float fTorseAreaZ = t.entityelement[ee].z;
					int torselimbindex = t.entityprofile[entid].spine2;
					if (torselimbindex > 0)
					{
						fTorseAreaX = LimbPositionX(t.entityelement[ee].obj, torselimbindex);
						fTorseAreaY = LimbPositionY(t.entityelement[ee].obj, torselimbindex);
						fTorseAreaZ = LimbPositionZ(t.entityelement[ee].obj, torselimbindex);
					}

					tracer_hit.x = fTorseAreaX;
					tracer_hit.y = fTorseAreaY;
					tracer_hit.z = fTorseAreaZ;

					Tracers::AddTracer(
						tracer_from,
						tracer_hit,
						t.gun[t.tgunid].settings.tracer_lifetime, // Lifetime
						XMFLOAT4(t.gun[t.tgunid].settings.tracer_colorR, t.gun[t.tgunid].settings.tracer_colorG, t.gun[t.tgunid].settings.tracer_colorB, 1), // Color
						t.gun[t.tgunid].settings.tracer_glow, // 5.0f, // Glow
						t.gun[t.tgunid].settings.tracer_scrollV, // Scroll
						t.gun[t.tgunid].settings.tracer_scaleV, // scaleV
						t.gun[t.tgunid].settings.tracer_width, // width
						t.gun[t.tgunid].settings.tracer_maxlength, // max length
						t.tgunid // TextureID
					);
				}
			}

			// another character is target
			t.ttte = ee;
			t.tdamageforce = 0;
			t.tdamagesource = 1;
			entity_applydamage ();

			// create either material decal specified in FPE or blood decal
			entity_applydecalfordamage(ee,-1,-1,-1);
		}
		else
		{
			float addheight = 25;
			if (t.aisystem.playerducking > 0)
				addheight -= 20;

			if (t.ttdistanceaccuracy_f < 0.3 || Rnd(t.tchancetohit_f*t.ttdistanceaccuracy_f) == 0)
			{
				if (t.gun[t.tgunid].settings.tracer_active)
				{
					//PE: Hit t.tplayerx_f
					XMFLOAT3 tracer_from, tracer_hit;

					tracer_from.x = t.tx_f;
					tracer_from.y = t.ty_f;
					tracer_from.z = t.tz_f;

					tracer_hit.x = t.tplayerx_f + (-1 + Rnd(2));
					tracer_hit.y = t.tplayery_f + addheight + (-1 + Rnd(2));;
					tracer_hit.z = t.tplayerz_f + (-1 + Rnd(2));

					if (t.gun[t.tgunid].settings.tracer_maxlength > 0)
					{
						//PE: Extent range so player see the tracer near by.
						XMVECTOR start = XMLoadFloat3(&tracer_from);
						XMVECTOR end = XMLoadFloat3(&tracer_hit);
						XMVECTOR dir = XMVectorSubtract(end, start);
						//float length = XMVectorGetX(XMVector3Length(dir)); //Hit weapon ? *0.97;
						dir = XMVector3Normalize(dir);
						XMStoreFloat3(&tracer_hit , end + (dir * t.gun[t.tgunid].settings.tracer_maxlength));
					}

					Tracers::AddTracer(
						tracer_from,
						tracer_hit,
						t.gun[t.tgunid].settings.tracer_lifetime, // Lifetime
						XMFLOAT4(t.gun[t.tgunid].settings.tracer_colorR, t.gun[t.tgunid].settings.tracer_colorG, t.gun[t.tgunid].settings.tracer_colorB, 1), // Color
						t.gun[t.tgunid].settings.tracer_glow, // 5.0f, // Glow
						t.gun[t.tgunid].settings.tracer_scrollV, // Scroll
						t.gun[t.tgunid].settings.tracer_scaleV, // scaleV
						t.gun[t.tgunid].settings.tracer_width, // width
						t.gun[t.tgunid].settings.tracer_maxlength, // max length
						t.tgunid // TextureID
					);
				}
				// player is target
				physics_player_takedamage ();
			}
			else
			{
				if (t.gun[t.tgunid].settings.tracer_active)
				{
					//PE: Miss t.tplayerx_f
					XMFLOAT3 tracer_from, tracer_hit;

					tracer_from.x = t.tx_f;
					tracer_from.y = t.ty_f;
					tracer_from.z = t.tz_f;

					tracer_hit.x = t.tplayerx_f + (-5 + Rnd(10));
					tracer_hit.y = t.tplayery_f + addheight + (-10 + Rnd(20));
					tracer_hit.z = t.tplayerz_f + (-5 + Rnd(10));

					if (t.gun[t.tgunid].settings.tracer_maxlength > 0)
					{
						//PE: Extent range so player see the tracer near by.
						XMVECTOR start = XMLoadFloat3(&tracer_from);
						XMVECTOR end = XMLoadFloat3(&tracer_hit);
						XMVECTOR dir = XMVectorSubtract(end, start);
						dir = XMVector3Normalize(dir);
						XMStoreFloat3(&tracer_hit, end + (dir * t.gun[t.tgunid].settings.tracer_maxlength));
					}

					Tracers::AddTracer(
						tracer_from,
						tracer_hit,
						t.gun[t.tgunid].settings.tracer_lifetime, // Lifetime
						XMFLOAT4(t.gun[t.tgunid].settings.tracer_colorR, t.gun[t.tgunid].settings.tracer_colorG, t.gun[t.tgunid].settings.tracer_colorB, 1), // Color
						t.gun[t.tgunid].settings.tracer_glow, // 5.0f, // Glow
						t.gun[t.tgunid].settings.tracer_scrollV, // Scroll
						t.gun[t.tgunid].settings.tracer_scaleV, // scaleV
						t.gun[t.tgunid].settings.tracer_width, // width
						t.gun[t.tgunid].settings.tracer_maxlength, // max length
						t.tgunid // TextureID
					);
				}
			}
		}
	}
	else
	{
		// FLAK (projectile) - find starting GetPoint ( for projectile )
		t.tobj = t.entityelement[t.te].attachmentobj;
		if (t.tobj > 0)
		{
			if(t.entityelement[t.te].attachmentobjfirespotlimb>-1)
			{
				t.flakx_f = LimbPositionX(t.tobj, t.entityelement[t.te].attachmentobjfirespotlimb);
				t.flaky_f = LimbPositionY(t.tobj, t.entityelement[t.te].attachmentobjfirespotlimb);
				t.flakz_f = LimbPositionZ(t.tobj, t.entityelement[t.te].attachmentobjfirespotlimb);
			}
			else
			{
				t.flakx_f = t.tx_f;// t.entityelement[t.te].fFirespotOffsetX;
				t.flaky_f = t.ty_f;// t.entityelement[t.te].fFirespotOffsetY;
				t.flakz_f = t.tz_f;// t.entityelement[t.te].fFirespotOffsetZ;
			}
			t.tdx_f = t.tplayerx_f - t.flakx_f;
			t.tdy_f = t.tplayery_f - t.flaky_f;
			t.tdz_f = t.tplayerz_f - t.flakz_f;
			t.tdd_f = Sqrt(abs(t.tdx_f*t.tdx_f) + abs(t.tdz_f*t.tdz_f));
			t.flakangle_f = atan2deg(t.tdx_f, t.tdz_f) + (Rnd(4) - 2);
			t.flakpitch_f = ((t.tdy_f / t.tdd_f)*-35.0) + Rnd(4) - 2;
			t.ttzentid = t.entityelement[t.te].bankindex;

			// create and launch projectile
			int iStoreGunID = t.gunid;
			t.gunid = t.tgunid;
			t.tProjectileType_s = t.gun[t.gunid].projectile_s; weapon_getprojectileid ();
			t.tSourceEntity = t.te; t.tTracerFlag = 0;
			t.tStartX_f = t.flakx_f; t.tStartY_f = t.flaky_f; t.tStartZ_f = t.flakz_f;
			t.tAngX_f = t.flakpitch_f; t.tAngY_f = t.flakangle_f; t.tAngZ_f = 0;
			weapon_projectile_make (false, false);
			t.gunid = iStoreGunID;
		}
		t.tolde = t.e;
		t.e = t.te;
		entity_lua_findcharanimstate ();
		t.e = t.tolde;

		// deduct one unit of ammo (only if npc oes NOT ignore need to reload)
		if (g.firemodes[t.tgunid][0].settings.npcignorereload == 0)
		{
			t.charanimstate.ammoinclip = t.charanimstate.ammoinclip - 1;
			if (t.charanimstate.ammoinclip < 0)  t.charanimstate.ammoinclip = 0;
		}
	}
}

void darkai_ischaracterhit (void)
{
	// takes; px#,py#,pz#,tobj,t.bulletfinalstrengthmod
	t.darkaifirerayhitcharacter = 0;
	for (g.charanimindex = 1; g.charanimindex <= g.charanimindexmax; g.charanimindex++)
	{
		if (t.entityelement[t.charanimstates[g.charanimindex].e].health > 0 && t.entityelement[t.charanimstates[g.charanimindex].e].eleprof.disableascharacter == 0 )
		{
			if (t.tobj == t.charanimstates[g.charanimindex].obj)
			{
				// if melee attack on character, half force for better organic response
				if (t.gun[t.gunid].settings.ismelee == 2)  t.tforce_f = t.tforce_f / 2.0;
				t.twhox_f = t.px_f; 
				t.twhoy_f = t.py_f; 
				t.twhoz_f = t.pz_f;
				darkai_shootcharacter ();
				t.darkaifirerayhitcharacter = 1;
			}
		}
	}
}

void darkai_shootcharacter (void)
{
	// receives charanimindex tobj tdamage twhox# twhoy# twhoz#
	if (t.entityelement[t.charanimstates[g.charanimindex].e].health > 0)
	{
		// handle shooting of character
		t.ttte = t.charanimstates[g.charanimindex].e;
		t.tdamage = 0; t.tdamageforce = t.tforce_f;
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
				t.tdamage = g.firemodes[t.gunid][0].settings.meleedamage * t.playercontrol.fMeleeDamageMultiplier;
				if (t.playercontrol.fWeaponDamageMultiplier > 0 && t.tdamage < 1) t.tdamage = 1;
			}
		}
		t.tdamage = t.tdamage * t.bulletfinalstrengthmod;
		t.tdamagesource = 1;
		entity_applydamage ();

		// and for extra viceral effect, play some thumps
		if (t.bulletisinfactmeleestrike > 0)
		{
			extern void physics_play_thump_sound (float fX, float fY, float fZ, float fFreqStart, float fFreqRange);
			physics_play_thump_sound(CameraPositionX(), CameraPositionY(), CameraPositionZ(), 44000, Rnd(6000));
		}
	}
}

// AI Sound Events

struct sSoundEvent
{
	float fX;
	float fY;
	float fZ;
	float fRadius;
	int iCategory;
	int iWhoE;
	DWORD dwTimeToFinish;
};
std::vector<sSoundEvent> g_soundEvent;

float getClosestSoundWithinRange (float fX, float fY, float fZ, int iCategory, int* piWhoE)
{
	int iWhoE = 0;
	int iResult = -1;
	float fBestDist = 99999.9f;
	for (int i = 0; i < g_soundEvent.size(); i++)
	{
		sSoundEvent* pSoundEvent = &g_soundEvent[i];
		float fDX = fX - pSoundEvent->fX;
		float fDY = fY - pSoundEvent->fY;
		float fDZ = fZ - pSoundEvent->fZ;
		float fDist = sqrt(fabs(fDX*fDX) + fabs(fDY*fDY) + fabs(fDZ*fDZ));
		if (fDist < pSoundEvent->fRadius && fDist < fBestDist && ( iCategory==0 || iCategory == pSoundEvent->iCategory) )
		{
			iWhoE = pSoundEvent->iWhoE;
			fBestDist = fDist;
			iResult = i;
		}
	}
	*piWhoE = iWhoE;
	if (iResult != -1)
	{
		return fBestDist;
	}
	else
	{
		return 0;
	}
}

void darkai_makesound_ex ( int iCategory, int iWhoE )
{
	// t.tsx_f, t.tsz_f, t.tradius_f
	// iCategory : 1-player, 2-nonplayer, 0-generic(explosion)
	sSoundEvent soundEvent;
	soundEvent.fX = t.tsx_f;
	soundEvent.fY = t.tsy_f;
	soundEvent.fZ = t.tsz_f;
	soundEvent.fRadius = t.tradius_f;
	soundEvent.iCategory = iCategory;
	soundEvent.iWhoE = iWhoE;
	soundEvent.dwTimeToFinish = timeGetTime() + 1000;
	g_soundEvent.push_back(soundEvent);
}

void darkai_makesound_byplayer (void)
{
	darkai_makesound_ex ( 1, -1 );
}

void darkai_makesound (void)
{
	darkai_makesound_ex ( 2, t.te );
}

void darkai_makeexplosionsound (void)
{
	t.tradius_f = 1000.0f;
	darkai_makesound_ex ( 0, 0 );
}

void darkai_managesound (void)
{
	// kill sound events after X time after creation
	DWORD dwCurrentTime = timeGetTime();
	for (int i = 0; i < g_soundEvent.size(); i++)
	{
		sSoundEvent* pSoundEvent = &g_soundEvent[i];
		if (dwCurrentTime > pSoundEvent->dwTimeToFinish)
		{
			g_soundEvent.erase(g_soundEvent.begin() + i);
			i = -1;
		}
	}
}
