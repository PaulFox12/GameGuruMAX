bool bFakeReloadActive = false;
void gunmode121_cancel ( void )
{
	gunanimtype realreload;
	if (bFakeReloadActive)
	{
		realreload = t.gstartreload;
		//PE: Perhaps also support ALT
		t.gstartreload.s = g.firemodes[t.gunid][0].action.hide.s;
		t.gstartreload.e = g.firemodes[t.gunid][0].action.hide.e;
		t.gcock.e = g.firemodes[t.gunid][0].action.hide.e;
		t.gendreload = g.firemodes[t.gunid][0].action.show;
		t.greloadloop.s = g.firemodes[t.gunid][0].action.hide.e;
		t.greloadloop.e = g.firemodes[t.gunid][0].action.hide.e;
		t.gshow = t.gidle;
		t.gshow.s = t.gidle.s;
	}

	if (  t.gunmode == 122 ) 
	{
		--t.guninterp;
		if (  t.guninterp <= 0 ) 
		{
			gun_SetObjectInterpolation (  t.currentgunobj,100 );
			gun_SetObjectFrame (  t.currentgunobj,t.gstartreload.s );
			if (  g.firemodes[t.gunid][g.firemode].settings.shotgun == 1 && g.firemodes[t.gunid][g.firemode].settings.isempty == 0 || g.firemodes[t.gunid][g.firemode].settings.isempty == 1 && g.firemodes[t.gunid][g.firemode].settings.emptyshotgun == 1 ) 
			{
				t.gunmode=700;
			}
			else
			{
				if (t.gun[t.gunid].settings.fake_reload && g.firemodes[t.gunid][0].action.hide.s > 0 && g.firemodes[t.gunid][0].action.show.s > 0)
				{
					//PE: Perhaps also support ALT
					realreload = t.gstartreload;
					t.gstartreload.s = g.firemodes[t.gunid][0].action.hide.s;
					t.gstartreload.e = g.firemodes[t.gunid][0].action.hide.e;
					t.gcock.e = g.firemodes[t.gunid][0].action.hide.e;
					bFakeReloadActive = true;
				}
				gun_SetObjectFrame(t.currentgunobj, t.gstartreload.s);
				t.gunmode=123;
			}
		}
	}
	
	//  AIRSLIDE SHOTGUN CODE BEGIN
	if (  t.gunmode == 700 ) 
	{
		gun_SetObjectInterpolation (  t.currentgunobj,100 );
		gun_PlayObject (  t.currentgunobj,t.gstartreload.s,t.gstartreload.e );
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		t.gunmode=701;
	}
	if (  t.gunmode == 701 ) 
	{
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  GetFrame(t.currentgunobj) >= t.gstartreload.e ) 
		{
			t.gunmode=703;
		}
	}
	if (  t.gunmode == 703 ) 
	{
		gun_PlayObject (  t.currentgunobj,t.greloadloop.s,t.greloadloop.e );
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		t.gunmode=7031;
	}
	if (  t.gunmode == 7031 ) 
	{
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  GetFrame(t.currentgunobj) >= t.greloadloop.e ) 
		{
			t.gunmode=702;
		}
	}
	if (  t.gunmode == 702 ) 
	{
		//  actual reload
		g.plrreloading=0;
		t.tneedfromclip=g.firemodes[t.gunid][g.firemode].settings.reloadqty+t.gunchamber-t.weaponammo[g.weaponammoindex+g.ammooffset];
		if (  t.tneedfromclip > 1  )  t.tneedfromclip  =  1;
		t.tpool=g.firemodes[t.gunid][g.firemode].settings.poolindex;
		if (  t.tpool == 0  )  t.ammo = t.weaponclipammo[g.weaponammoindex+g.ammooffset]; else t.ammo = t.ammopool[t.tpool].ammo;
		if (  t.tneedfromclip>t.ammo  )  t.tneedfromclip = t.ammo;
		if (  t.tneedfromclip>0 ) 
		{
			t.weaponammo[g.weaponammoindex+g.ammooffset]=t.weaponammo[g.weaponammoindex+g.ammooffset]+t.tneedfromclip;
			if (  t.tpool == 0  )  t.weaponclipammo[g.weaponammoindex+g.ammooffset] = t.weaponclipammo[g.weaponammoindex+g.ammooffset]-t.tneedfromclip; else t.ammopool[t.tpool].ammo = t.ammopool[t.tpool].ammo-t.tneedfromclip;
			t.gunmode=703;
			if (  g.firemodes[t.gunid][g.firemode].settings.reloadqty+t.gunchamber-t.weaponammo[g.weaponammoindex+g.ammooffset]<1  )  t.gunmode = 7041;
			if (  t.player[t.plrid].state.firingmode == 1  )  t.gunmode = 7041;
			//  end reload if no ammo in clip left
			if (  t.tpool == 0 ) 
			{
				if (  t.weaponclipammo[g.weaponammoindex+g.ammooffset] <= 0 ) 
				{
					t.weaponclipammo[g.weaponammoindex+g.ammooffset]=0 ; t.gunmode=5 ; g.plrreloading=0;
				}
			}
			else
			{
				if (  t.ammopool[t.tpool].ammo <= 0 ) 
				{
					t.ammopool[t.tpool].ammo=0 ; t.gunmode=5 ; g.plrreloading=0;
				}
			}
		}
		else
		{
			t.gunmode=7041;
		}
		if (  t.gunsound[t.gunid][2].soundid1>0 ) 
		{
			if (  SoundExist(t.gunsound[t.gunid][2].soundid1) == 1 ) 
			{
				PositionSound (  t.gunsound[t.gunid][2].soundid1,CameraPositionX()/10.0,CameraPositionY()/3.0,CameraPositionZ()/10.0 );
			}
		}
		gun_updatebulletvisibility ( );
	}
	if (  t.gunmode == 7041 ) 
	{
		if (  t.gunchamber > 0 ) 
		{
			t.gunmode=706;
		}
		else
		{
			t.gunmode=704;
		}
	}
	if (  t.gunmode == 704 ) 
	{
		gun_PlayObject (  t.currentgunobj,t.gendreload.s,t.gcock.e );
		t.gunmode=705;
	}
	if (  t.gunmode == 705 ) 
	{
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  GetFrame(t.currentgunobj) >= t.gcock.e ) { t.gunmode = 5  ; g.plrreloading = 0; }
	}
	if (  t.gunmode == 706 ) 
	{
		gun_PlayObject (  t.currentgunobj,t.gendreload.s,t.gendreload.e );
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		t.gunmode=707;
	}
	if (  t.gunmode == 707 ) 
	{
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  GetFrame(t.currentgunobj) >= t.gendreload.e ) { t.gunmode = 5 ; t.gun[t.gunid].settings.ismelee = 0; }
	}
	//  AIRSLIDE SHOTGUN CODE END
	
	if (  t.gunmode == 123 ) 
	{
		t.gunmode=124;
		//  start reload animation
		if (bFakeReloadActive)
			gun_SetObjectInterpolation(t.currentgunobj, 50);
		else
			gun_SetObjectInterpolation (  t.currentgunobj,100 );

		gun_PlayObject (  t.currentgunobj,t.gstartreload.s,t.gcock.e );
		t.currentgunanimspeed_f = g.firemodes[t.gunid][g.firemode].settings.reloadspeed*t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  t.gunsound[t.gunid][2].soundid1>0 ) 
		{
			if (  SoundExist(t.gunsound[t.gunid][2].soundid1) == 1 ) 
			{
				PositionSound (  t.gunsound[t.gunid][2].soundid1,CameraPositionX()/10.0,CameraPositionY()/3.0,CameraPositionZ()/10.0 );
			}
		}
	}
	if (  t.gunmode == 124 ) 
	{
		//  reload anim with possibility of bullet reset
		t.currentgunanimspeed_f = g.firemodes[t.gunid][g.firemode].settings.reloadspeed*t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  GetFrame(t.currentgunobj) >= t.gun[t.gunid].settings.bulletreset ) 
		{
			//  anticipate new weapon ammo quantity to set bullet visibility
			t.tstoreoldammo=t.weaponammo[g.weaponammoindex+g.ammooffset];
			t.tstoreoldammoclip=t.weaponclipammo[g.weaponammoindex+g.ammooffset];
			t.tpool=g.firemodes[t.gunid][g.firemode].settings.poolindex;
			if (  t.tpool>0  )  t.tstoreoldammopool = t.ammopool[t.tpool].ammo;
			gun_actualreloadcode ( );
			gun_updatebulletvisibility ( );
			t.weaponammo[g.weaponammoindex+g.ammooffset]=t.tstoreoldammo;
			t.weaponclipammo[g.weaponammoindex+g.ammooffset]=t.tstoreoldammoclip;
			if (  t.tpool>0  )  t.ammopool[t.tpool].ammo = t.tstoreoldammopool;
			t.gunmode=125;
		}
		if (  GetFrame(t.currentgunobj) >= t.gcock.e  )  t.gunmode = 126;
	}
	if (  t.gunmode == 125 ) 
	{
		//  reload anim with no possibility of bullet reset
		t.currentgunanimspeed_f = g.firemodes[t.gunid][g.firemode].settings.reloadspeed*t.genericgunanimspeed_f;
		gun_SetObjectSpeed (  t.currentgunobj,t.currentgunanimspeed_f );
		if (  GetFrame(t.currentgunobj) >= t.gcock.e  )  t.gunmode = 126;
	}

	static int pausewhenhidden = 0;
	static int soundtimer = 0;

	if (  t.gunmode == 126 ) 
	{
		//  actual reload
		g.plrreloading=0;
		gun_actualreloadcode ( );
		t.gun[t.gunid].settings.ismelee=0;

		t.gunmode=5;
		if (bFakeReloadActive)
		{
			//PE: Check sound , t.gunsound[t.gunid][2].soundid1
			t.gunmode = 131;
			pausewhenhidden = 10;
			//PE: Start reload sound. Only if pointing to reload frames.
			int usesframe = -1;
			if (t.gunsounditem[t.gunid][0].keyframe > realreload.s && t.gunsounditem[t.gunid][0].keyframe <= realreload.e)
				usesframe = 0;
			else if (t.gunsounditem[t.gunid][2].keyframe > realreload.s && t.gunsounditem[t.gunid][2].keyframe <= realreload.e)
				usesframe = 2;
			else if (t.gunsounditem[t.gunid][1].keyframe > realreload.s && t.gunsounditem[t.gunid][1].keyframe <= realreload.e && t.gunsounditem[t.gunid][usesframe].playsound != 11)
				usesframe = 1;
			else if (t.gunsounditem[t.gunid][3].keyframe > realreload.s && t.gunsounditem[t.gunid][3].keyframe <= realreload.e)
				usesframe = 3;
			else if (t.gunsounditem[t.gunid][9].keyframe > realreload.s && t.gunsounditem[t.gunid][9].keyframe <= realreload.e)
				usesframe = 9;
			if (usesframe >= 0)
			{
				t.sndid = t.gunsound[t.gunid][t.gunsounditem[t.gunid][usesframe].playsound].soundid1;

				if (t.sndid > 0)
				{
					if (SoundExist(t.sndid) == 1)
					{
						PlaySound(t.sndid);
						posinternal3dsound(t.sndid, CameraPositionX(), CameraPositionY(), CameraPositionZ());
						soundtimer = MAXTimer() + 600;
						if (pestrcasestr(t.gun[t.gunid].name_s.Get(), "rpg"))
							soundtimer = MAXTimer() + 1700;
					}
				}
			}
		}
		else
		{
			pausewhenhidden = 0;
			bFakeReloadActive = false;
		}
	}

	//  gun reveal
	if (  t.gunmode == 131 ) 
	{
		if (pausewhenhidden == 0)
		{
			if (bFakeReloadActive)
				gun_SetObjectInterpolation(t.currentgunobj, 10); //PE: Slow to idle.
			else
				gun_SetObjectInterpolation(t.currentgunobj, 100);
			gun_SetObjectFrame(t.currentgunobj, t.gshow.s);
			gun_PlayObject(t.currentgunobj, t.gshow.s, t.gshow.e);
			t.currentgunanimspeed_f = t.genericgunanimspeed_f;
			gun_SetObjectSpeed(t.currentgunobj, t.currentgunanimspeed_f);
			if (t.gunsound[t.gunid][2].soundid1 > 0)
			{
				if (SoundExist(t.gunsound[t.gunid][2].soundid1) == 1)
				{
					PositionSound(t.gunsound[t.gunid][2].soundid1, CameraPositionX() / 10.0, CameraPositionY() / 3.0, CameraPositionZ() / 10.0);
				}
			}
			t.gunmode = 132;
		}
		else
		{
			gun_SetObjectFrame(t.currentgunobj, t.gcock.e);
			if (soundtimer > 0)
			{
				if (MAXTimer() > soundtimer)
				{
					if (t.gunsounditem[t.gunid][3].keyframe > realreload.s && t.gunsounditem[t.gunid][3].keyframe <= realreload.e)
					{
						t.sndid = t.gunsound[t.gunid][t.gunsounditem[t.gunid][3].playsound].soundid1;

						if (t.sndid > 0)
						{
							if (SoundExist(t.sndid) == 1)
							{
								PlaySound(t.sndid);
								posinternal3dsound(t.sndid, CameraPositionX(), CameraPositionY(), CameraPositionZ());
							}
						}
					}
					soundtimer = 0;
				}
			}
			else
				pausewhenhidden--;
		}
	}
	if (  t.gunmode == 132 ) 
	{
		t.currentgunanimspeed_f = t.genericgunanimspeed_f;
		gun_SetObjectSpeed(t.currentgunobj, t.currentgunanimspeed_f);
		if (GetFrame(t.currentgunobj) >= t.gshow.e)
		{
			if (bFakeReloadActive)
				gun_SetObjectInterpolation(t.currentgunobj, 100);
			t.gunmode = 5;
			bFakeReloadActive = false;
		}
		if (GetFrame(t.currentgunobj) < t.gshow.s)
		{
			if (bFakeReloadActive)
				gun_SetObjectInterpolation(t.currentgunobj, 100);
			t.gunmode = 5;
			bFakeReloadActive = false;
		}
	}
}

void gun_actualreloadcode ( void )
{
	//  called from several places (above)
	t.tneedfromclip=g.firemodes[t.gunid][g.firemode].settings.reloadqty-t.weaponammo[g.weaponammoindex+g.ammooffset];
	if (  t.gunchamber  ==  1  )  t.tneedfromclip  =  t.tneedfromclip + 1;
	t.tpool=g.firemodes[t.gunid][g.firemode].settings.poolindex;
	if (  t.tpool == 0  )  t.ammo = t.weaponclipammo[g.weaponammoindex+g.ammooffset]; else t.ammo = t.ammopool[t.tpool].ammo;
	if (  t.tneedfromclip>t.ammo  )  t.tneedfromclip = t.ammo;
	if (  t.tneedfromclip>0 ) 
	{
		t.weaponammo[g.weaponammoindex+g.ammooffset]=t.weaponammo[g.weaponammoindex+g.ammooffset]+t.tneedfromclip;
		if (  t.tpool == 0  )  t.weaponclipammo[g.weaponammoindex+g.ammooffset] = t.weaponclipammo[g.weaponammoindex+g.ammooffset]-t.tneedfromclip; else t.ammopool[t.tpool].ammo = t.ammopool[t.tpool].ammo-t.tneedfromclip;
	}
}

void gun_flashbrass_position (float* pfWorldPosX, float* pfWorldPosY, float* pfWorldPosZ, float fModX, float fModY, float fModZ)
{
	// Allow MuzzleFlash to work in Simple Zoom
	GGVECTOR3 vecOffset;
	if (g.firemodes[t.gunid][g.firemode].settings.simplezoom != 0 && t.gunzoommode != 0 && g.firemodes[t.gunid][g.firemode].settings.simplezoomflash == 1)
	{
		vecOffset = GGVECTOR3((g.firemodes[t.gunid][g.firemode].settings.zoommuzzlex_f * fModX) + g.gunlagX_f + g.gunOffsetX_f, (g.firemodes[t.gunid][g.firemode].settings.zoommuzzley_f * fModY) + g.gunlagY_f + g.gunOffsetY_f, (g.firemodes[t.gunid][g.firemode].settings.zoommuzzlez_f * fModZ));
	}
	else
	{
		vecOffset = GGVECTOR3((g.firemodes[t.gunid][g.firemode].settings.muzzlex_f * fModX) + g.gunlagX_f + g.gunOffsetX_f, (g.firemodes[t.gunid][g.firemode].settings.muzzley_f * fModY) + g.gunlagY_f, (g.firemodes[t.gunid][g.firemode].settings.muzzlez_f * fModZ));
	}
	sObject* pWeaponHUDObject = GetObjectData(g.hudbankoffset + 2);
	GGVec3TransformCoord(&vecOffset, &vecOffset, &pWeaponHUDObject->position.matObjectNoTran);
	*pfWorldPosX = ObjectPositionX(g.hudbankoffset + 2);
	*pfWorldPosY = ObjectPositionY(g.hudbankoffset + 2);
	*pfWorldPosZ = ObjectPositionZ(g.hudbankoffset + 2);
	*pfWorldPosX += vecOffset.x;
	*pfWorldPosY += vecOffset.y;
	*pfWorldPosZ += vecOffset.z;
}

void gun_flash ( void )
{
	extern bool bHideWeaponsMuzzle;
	if (bHideWeaponsMuzzle) return;

	if (g.firemodes[t.gunid][g.firemode].settings.flashimg > 0)
	{
		// but only if the gun has flash(muzzleflash) enabled
		// hide muzzle if still visible (quick shot)
		if (t.plrzoomin_f != 0.0 && g.firemodes[t.gunid][g.firemode].settings.simplezoom == 0)
		{
			if (GetVisible(g.hudbankoffset + 5) == 1)  HideObject (g.hudbankoffset + 5);
			if (GetVisible(g.hudbankoffset + 32) == 1)  HideObject (g.hudbankoffset + 32);
			t.gunflash = 0;
		}

		// gun flash position
		float fWorldPosX, fWorldPosY, fWorldPosZ;
		gun_flashbrass_position(&fWorldPosX, &fWorldPosY, &fWorldPosZ, 1, 1, 1);
		PositionObject(g.hudbankoffset + 5, fWorldPosX, fWorldPosY, fWorldPosZ);
		PointObject(g.hudbankoffset + 5, CameraPositionX(), CameraPositionY(), CameraPositionZ());
		sObject* pMuzzleFlashObj = GetObjectData(g.hudbankoffset + 5);
		if (pMuzzleFlashObj && pMuzzleFlashObj->ppFrameList)
		{
			// adjust rotation of frame zero Z to spin the muzzle flash
			pMuzzleFlashObj->ppFrameList[0]->vecRotation.z = Rnd(360);
		}
		if (t.gunflash == 1)
		{
			// fire flash init
			t.gunflash = 2;
			g.gunflashcount = g.firemodes[t.gunid][g.firemode].settings.firerate / 2;
			ShowObject (g.hudbankoffset + 5);
			if (t.gun[t.gunid].settings.flashlimb2 != -1)
			{
				RotateObject (g.hudbankoffset + 32, 0, 0, Rnd(360));
				ShowObject (g.hudbankoffset + 32);
			}

			// light flash init
			if (g.firemodes[t.gunid][g.firemode].settings.usespotlighting != 0)
			{
				RotateCamera(CameraAngleX(), CameraAngleY() - 45.0f, CameraAngleZ());
				MoveCamera(10.0);
				t.playerWeaponFlash.spotflashx_f = CameraPositionX();
				t.playerWeaponFlash.spotflashy_f = CameraPositionY();
				t.playerWeaponFlash.spotflashz_f = CameraPositionZ();
				MoveCamera(-10.0);
				RotateCamera(CameraAngleX(), CameraAngleY() + 45.0f, CameraAngleZ());
				t.playerWeaponFlash.spotlightr_f = g.firemodes[t.gunid][g.firemode].settings.muzzlecolorr / 5;
				t.playerWeaponFlash.spotlightg_f = g.firemodes[t.gunid][g.firemode].settings.muzzlecolorg / 5;
				t.playerWeaponFlash.spotlightb_f = g.firemodes[t.gunid][g.firemode].settings.muzzlecolorb / 5;
				t.playerWeaponFlash.flashlightcontrol_f = 1.0f;
			}
		}

		if (t.gunflash == 2)
		{
			// Timer (  based deduction )
			t.firerate = g.firemodes[t.gunid][g.firemode].settings.firerate / 2;
			if (g.gunflashcount <= (t.firerate) - (g.timeelapsed_f * 2))
			{
				// Just hide all of them early 
				t.playerWeaponFlash.flashlightcontrol_f = 0.0f;
				HideObject (g.hudbankoffset + 5);
				HideObject (g.hudbankoffset + 32);
			}
			g.gunflashcount -= g.timeelapsed_f * 1.25; //PE: A little bit faster.
			if (g.gunflashcount <= 0)
			{
				t.gunflash = 3;
			}
		}
		if (t.gunflash == 3)
		{
			// final hide
			t.gunflash = 0;
			g.gunflashcount = 0;
			t.playerWeaponFlash.flashlightcontrol_f = 0.0f;
			HideObject (g.hudbankoffset + 5);
			HideObject (g.hudbankoffset + 32);
		}
	}
}

void gun_brass ( void )
{
	extern bool bHideWeapons;
	if (bHideWeapons) return;
	//  If gun has no brass, skip this creation moment
	if (  g.firemodes[t.gunid][g.firemode].settings.brass == 0  )  return;

	//  Twin gun second brass feature
	t.gunbrass2 = 0; if ( t.gunbrass == 1 && t.gun[t.gunid].settings.flashlimb2 != -1 ) t.gunbrass2 = 1;
	
	// count available brass, and if two are not available, free the oldest
	if ( t.gunbrass > 0 ||  t.gunbrass2 > 0 )
	{
		int iCountBrass = 0;
		int iCountBrassVisible = 0;
		for ( t.o = 6 ; t.o <= 20; t.o++ )
		{
			iCountBrass++;
			t.obj=g.hudbankoffset+t.o;
			if ( GetVisible(t.obj) == 1 ) 
				iCountBrassVisible++;
		}
		int iBrassNeeded = iCountBrassVisible - (iCountBrass-2);
		if ( iBrassNeeded > 0 )
		{
			// need to free up oldest (i.e lowest) brass
			while ( iBrassNeeded > 0 )
			{
				int iLowestObj = 0;
				float fLowestY = 999999.0f;
				for ( t.o = 6 ; t.o <= 20; t.o++ )
				{
					t.obj = g.hudbankoffset+t.o;
					float fThisY = ObjectPositionY(t.obj);
					if ( fThisY < fLowestY )
					{
						iLowestObj = t.obj;
						fLowestY = fThisY;
					}
				}
				if ( iLowestObj > 0 ) 
				{
					ODEDestroyObject ( iLowestObj );
					HideObject ( iLowestObj );
				}
				iBrassNeeded--;
			}
		}
	}

	//  find free shell and expel
	for ( t.o = 6 ; t.o <= 20; t.o++ )
	{
		t.obj=g.hudbankoffset+t.o;
		if ( (GetVisible(t.obj) == 0 || t.o == 20) && t.gunbrass == 1 ) 
		{
			if (t.gun[t.gunid].settings.brasslimb>0)
			{
				t.lx_f = LimbPositionX(t.currentgunobj, t.gun[t.gunid].settings.brasslimb) + 1.0 - (Rnd(20) / 10.0);
				t.ly_f = LimbPositionY(t.currentgunobj, t.gun[t.gunid].settings.brasslimb);
				t.lz_f = LimbPositionZ(t.currentgunobj, t.gun[t.gunid].settings.brasslimb) + 1.0 - (Rnd(20) / 10.0);
			}
			else
			{
				float fWorldPosX, fWorldPosY, fWorldPosZ;
				gun_flashbrass_position(&fWorldPosX, &fWorldPosY, &fWorldPosZ, 0.6f, 0.4f, 0.6f);
				t.lx_f = fWorldPosX + 1.0 - (Rnd(20) / 10.0);
				t.ly_f = fWorldPosY;
				t.lz_f = fWorldPosZ + 1.0 - (Rnd(20) / 10.0);
			}
			PositionObject (  t.obj,t.lx_f,t.ly_f,t.lz_f );
			RotateObject (  t.obj,0,CameraAngleY(0),0 );
			t.brassfallcount_f[t.o]=g.firemodes[t.gunid][g.firemode].settings.brasslife;
			ShowObject (  t.obj );
			ODECreateDynamicBox(t.obj, -1, 12); //PE: Make sure not to move player.
			t.tbrassang_f=g.firemodes[t.gunid][g.firemode].settings.brassangle+Rnd(g.firemodes[t.gunid][g.firemode].settings.brassanglerand);
			t.tbrassspeed_f=g.firemodes[t.gunid][g.firemode].settings.brassspeed+Rnd(g.firemodes[t.gunid][g.firemode].settings.brassspeedrand);
			t.tvelx_f=NewXValue(0,CameraAngleY(0)+t.tbrassang_f,t.tbrassspeed_f);
			t.tvelz_f=NewZValue(0,CameraAngleY(0)+t.tbrassang_f,t.tbrassspeed_f);
			t.tbrassupward_f=g.firemodes[t.gunid][g.firemode].settings.brassupward+Rnd(g.firemodes[t.gunid][g.firemode].settings.brassupwardrand);
			t.tvelx_f /= 10.0f;
			t.tvelz_f /= 10.0f;
			t.tbrassupward_f /= 10.0f;
			ODEAddBodyForce (  t.obj,t.tvelx_f,t.tbrassupward_f,t.tvelz_f,0,0,0 );
			t.tbrassrotx_f=g.firemodes[t.gunid][g.firemode].settings.brassrotx+Rnd(g.firemodes[t.gunid][g.firemode].settings.brassrotxrand);
			t.tbrassroty_f=g.firemodes[t.gunid][g.firemode].settings.brassroty+Rnd(g.firemodes[t.gunid][g.firemode].settings.brassrotyrand);
			t.tbrassrotz_f=g.firemodes[t.gunid][g.firemode].settings.brassrotz+Rnd(g.firemodes[t.gunid][g.firemode].settings.brassrotzrand);
			ODESetAngularVelocity (  t.obj,t.tbrassrotx_f,t.tbrassroty_f,t.tbrassrotz_f );
			t.gunbrass=0;
		}
		if ((GetVisible(t.obj) == 0 || t.o == 20) && t.gunbrass == 0 && t.gunbrass2 == 1)
		{
			if (t.gun[t.gunid].settings.brasslimb2 > 0)
			{
				t.lx_f = LimbPositionX(t.currentgunobj, t.gun[t.gunid].settings.brasslimb2) + 1.0 - (Rnd(20) / 10.0);
				t.ly_f = LimbPositionY(t.currentgunobj, t.gun[t.gunid].settings.brasslimb2);
				t.lz_f = LimbPositionZ(t.currentgunobj, t.gun[t.gunid].settings.brasslimb2) + 1.0 - (Rnd(20) / 10.0);
				PositionObject(t.obj, t.lx_f, t.ly_f, t.lz_f);
				RotateObject(t.obj, 0, CameraAngleY(), 0);
				t.brassfallcount_f[t.o] = 25.0;
				ShowObject(t.obj);
				ODECreateDynamicBox(t.obj, -1, 12); //PE: Make sure not to move player.
			}
			//  apply forces (above) here
			t.gunbrass2 = 0;
		}
	}
}

void gun_brass_indi ( void )
{
	//  new system uses physics, and life fade to remove brass
	for ( t.o = 6 ; t.o<=  20; t.o++ )
	{
		t.obj=g.hudbankoffset+t.o;
		if (  GetVisible(t.obj) == 1 ) 
		{
			t.brassfallcount_f[t.o]=t.brassfallcount_f[t.o]-g.timeelapsed_f;
			if (  t.brassfallcount_f[t.o]<0 ) 
			{
				ODEDestroyObject (  t.obj );
				HideObject (  t.obj );
			}
		}
	}
}

void gun_smoke ( void )
{
	extern bool bHideWeaponsSmoke;
	if (bHideWeaponsSmoke) return;

	//  FPSCV104RC5-twingun
	t.gunsmoke2 = 0 ; if (  t.gunsmoke == 1 && t.gun[t.gunid].settings.flashlimb2 != -1  )  t.gunsmoke2 = 1;

	//  find free smoke and puff
	if (g.firemodes[t.gunid][0].settings.smokeimg > 0)
	{
		// but only if the gun has smoke enabled
		for (t.o = 21; t.o <= 30; t.o++)
		{
			t.obj = g.hudbankoffset + t.o;
			if (GetVisible(t.obj) == 0 && t.gunsmoke == 1)
			{
				t.ttsmokelimb = t.gun[t.gunid].settings.smokelimb;
				if (t.ttsmokelimb <= 0)  t.ttsmokelimb = t.gun[t.gunid].settings.brasslimb;
				if (t.ttsmokelimb > 0)
				{
					// position from smoke or brass limb
					t.lx_f = LimbPositionX(t.currentgunobj, t.ttsmokelimb) + 1.0 - (Rnd(20) / 10.0);
					t.ly_f = LimbPositionY(t.currentgunobj, t.ttsmokelimb) + 1.0 - (Rnd(20) / 10.0);
					t.lz_f = LimbPositionZ(t.currentgunobj, t.ttsmokelimb) + 1.0 - (Rnd(20) / 10.0);
				}
				else
				{
					// gun flash position
					float fWorldPosX, fWorldPosY, fWorldPosZ;
					gun_flashbrass_position(&fWorldPosX, &fWorldPosY, &fWorldPosZ, 1, 1, 1);
					t.lx_f = fWorldPosX + 1.0 - (Rnd(20) / 10.0);
					t.ly_f = fWorldPosY + 1.0 - (Rnd(20) / 10.0);
					t.lz_f = fWorldPosZ + 1.0 - (Rnd(20) / 10.0);
				}
				// no fixpivot logic
				RotateObject (t.obj, 0, 0, 0);
				PositionObject (t.obj, t.lx_f, t.ly_f, t.lz_f);
				ShowObject (t.obj);
				t.smokeframe_f[t.o] = 0.0;
				t.gunsmoke = 0; t.smokeframe = 0;
			}
			if (GetVisible(t.obj) == 0 && t.gunsmoke == 0 && t.gunsmoke2 == 1)
			{
				t.ttsmokelimb = t.gun[t.gunid].settings.smokelimb2;
				if (t.ttsmokelimb <= 0) t.ttsmokelimb = t.gun[t.gunid].settings.brasslimb;
				if (t.ttsmokelimb > 0)
				{
					t.lx_f = LimbPositionX(t.currentgunobj, t.ttsmokelimb) + 1.0 - (Rnd(20) / 10.0);
					t.ly_f = LimbPositionY(t.currentgunobj, t.ttsmokelimb) + 1.0 - (Rnd(20) / 10.0);
					t.lz_f = LimbPositionZ(t.currentgunobj, t.ttsmokelimb) + 1.0 - (Rnd(20) / 10.0);
				}
				else
				{
					// gun flash position
					float fWorldPosX, fWorldPosY, fWorldPosZ;
					gun_flashbrass_position(&fWorldPosX, &fWorldPosY, &fWorldPosZ, 1, 1, 1);
					t.lx_f = fWorldPosX + 1.0 - (Rnd(20) / 10.0);
					t.ly_f = fWorldPosY + 1.0 - (Rnd(20) / 10.0);
					t.lz_f = fWorldPosZ + 1.0 - (Rnd(20) / 10.0);
				}
				// no fixpivot logic
				RotateObject (t.obj, 0, 0, 0);
				PositionObject (t.obj, t.lx_f, t.ly_f, t.lz_f);
				ShowObject (t.obj);
				t.smokeframe_f[t.o] = 0.0;
				t.gunsmoke2 = 0; t.smokeframe = 0;
			}
			if (GetVisible(t.obj) == 1)
			{
				PointObject (t.obj, CameraPositionX(), CameraPositionY(), CameraPositionZ());
				t.smokerisespeed_f = g.firemodes[t.gunid][g.firemode].settings.smokespeed / 100.0;
				PositionObject (t.obj, ObjectPositionX(t.obj), ObjectPositionY(t.obj) + t.smokerisespeed_f, ObjectPositionZ(t.obj));
				float fSmokeSize = g.firemodes[t.gunid][g.firemode].settings.smokesize;
				ScaleObject (t.obj, fSmokeSize, fSmokeSize, fSmokeSize);
				t.smokeframe_f[t.o] = t.smokeframe_f[t.o] + (2.0 * g.timeelapsed_f); t.smokeframe = t.smokeframe_f[t.o];
				if (GetInScreen(t.obj) == 1 && t.smokeframe <= 15)
				{
					SetObjectUVManually(t.obj, t.smokeframe, 4, 4);
				}
				else
				{
					HideObject (t.obj);
				}
			}
		}
	}
}

void gun_updatebulletvisibility ( void )
{
	if (  g.firemode == 0 ) 
	{
		if (  t.gun[t.gunid].settings.bulletmod == 1 ) 
		{
			if (  t.weaponammo[g.weaponammoindex]<t.gun[t.gunid].settings.bulletamount ) 
			{
				for ( t.p = t.gun[t.gunid].settings.bulletlimbstart ; t.p<=  t.gun[t.gunid].settings.bulletlimbend; t.p++ )
				{
					if (  t.p <= ArrayCount(t.bulletlimbs) ) 
					{
						t.limbnumber=t.bulletlimbs[t.p];
						if (  t.limbnumber<t.bulletlimbs[t.gun[t.gunid].settings.bulletlimbstart+t.weaponammo[g.weaponammoindex]] ) 
						{
							ScaleLimb (  t.gun[t.gunid].obj,t.limbnumber,100,100,100 );
						}
						else
						{
							ScaleLimb (  t.gun[t.gunid].obj,t.limbnumber,1,1,1 );
						}
					}
				}
			}
			else
			{
				for ( t.p = t.gun[t.gunid].settings.bulletlimbstart ; t.p<=  t.gun[t.gunid].settings.bulletlimbend; t.p++ )
				{
					if (  t.p <= ArrayCount(t.bulletlimbs) ) 
					{
						t.limbnumber=t.bulletlimbs[t.p];
						ScaleLimb (  t.gun[t.gunid].obj,t.limbnumber,100,100,100 );
					}
				}
			}
		}
	}
}

// every round (or throw) the player's gun has fired; Lua gets the number fired each frame as g_PlayerGunShotThisFrame
int g_iPlayerGunShotCount = 0;

// the trigger pull the rays of gun_shoot_oneray belong to, for the player's hit record (playerhit_openray)
int g_iPlayerHitPullShot = -1;
int g_iPlayerHitPullGunID = 0;
int g_iPlayerHitPullRays = 0;

void gun_shoot ( void )
{
	//  When fire Line (  active )
	if ( t.gunshoot == 1 ) 
	{
		g_iPlayerGunShotCount++;

		//  170315 - 020 - stop invincible if you shoot
		g.mp.invincibleTimer = 0;

		//  if third person, trigger shot flag
		if ( t.playercontrol.thirdperson.enabled == 1 ) 
		{
			int iWeaponIndex = t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.hasweapon;
			if ( iWeaponIndex > 0 )
			{
				// this triggers the TPP to rotate to face shot direction
				t.playercontrol.thirdperson.shotfired = 1;
			}
		}

		//  Recoil normal and zoom variants
		if (  t.gunzoommode != 0 ) 
		{
			t.xprect_f=g.firemodes[t.gunid][g.firemode].settings.zoomrecoilxcorrect_f/100;
			t.yprect_f=g.firemodes[t.gunid][g.firemode].settings.zoomrecoilycorrect_f/100;
		}
		else
		{
			t.xprect_f=g.firemodes[t.gunid][g.firemode].settings.recoilxcorrect_f/100;
			t.yprect_f=g.firemodes[t.gunid][g.firemode].settings.recoilycorrect_f/100;
		}
		t.xprect_f -= 0.25f;
		t.yprect_f -= 0.25f;

		//  Zoom vs Normal recoil motion
		if ( t.gunzoommode != 0 ) 
		{
			g.gunRecoilY_f = g.gunRecoilY_f + (g.firemodes[t.gunid][g.firemode].settings.zoomrecoily_f/5);
			t.gunRecoilCorrectY_f = t.gunRecoilCorrectY_f + ((g.firemodes[t.gunid][g.firemode].settings.zoomrecoily_f/5)*(t.yprect_f));
			if (  Rnd(1)  ==  1 ) 
			{
				g.gunRecoilX_f = g.gunRecoilX_f + (g.firemodes[t.gunid][g.firemode].settings.zoomrecoilx_f/5) ; g.gunRecoilCorrectX_f = g.gunRecoilCorrectX_f + ((g.firemodes[t.gunid][g.firemode].settings.zoomrecoilx_f/5)*(t.xprect_f));
			}
			else
			{
				g.gunRecoilX_f = g.gunRecoilX_f - (g.firemodes[t.gunid][g.firemode].settings.zoomrecoilx_f/5) ; g.gunRecoilCorrectX_f = g.gunRecoilCorrectX_f - ((g.firemodes[t.gunid][g.firemode].settings.zoomrecoilx_f/5)*(t.xprect_f));
			}
		}
		else
		{
			g.gunRecoilY_f = g.gunRecoilY_f + (g.firemodes[t.gunid][g.firemode].settings.recoily_f/5);
			t.gunRecoilCorrectY_f = t.gunRecoilCorrectY_f + ((g.firemodes[t.gunid][g.firemode].settings.recoily_f/5)*(t.yprect_f));
			if (  Rnd(1)  ==  1 ) 
			{
				g.gunRecoilX_f = g.gunRecoilX_f + (g.firemodes[t.gunid][g.firemode].settings.recoilx_f/5) ; g.gunRecoilCorrectX_f = g.gunRecoilCorrectX_f + ((g.firemodes[t.gunid][g.firemode].settings.recoilx_f/5)*(t.xprect_f));
			}
			else
			{
				g.gunRecoilX_f = g.gunRecoilX_f - (g.firemodes[t.gunid][g.firemode].settings.recoilx_f/5) ; g.gunRecoilCorrectX_f = g.gunRecoilCorrectX_f - ((g.firemodes[t.gunid][g.firemode].settings.recoilx_f/5)*(t.xprect_f));
			}
		}

		//  Line (  Modified for Alternate Fire )
		t.tokay=0;
		if ( g.firemodes[t.gunid][g.firemode].settings.flakindex == 0  )  t.tokay = 1;
		if ( t.gun[t.gunid].settings.alternateisflak == 1 && t.gun[t.gunid].settings.alternate == 0  )  t.tokay = 1;
		if ( t.gun[t.gunid].settings.alternateisray == 1 && t.gun[t.gunid].settings.alternate == 1  )  t.tokay = 1;
		if ( t.gun[t.gunid].settings.ismelee == 2 )  t.tokay = 1;
		if ( t.tokay == 1 ) 
		{
			//  BULLET
			//  gun data controls iterations and accuracy
			t.trayiter=1+g.firemodes[t.gunid][g.firemode].settings.iterate;
			//  Make gun more accurate so bullets don't fly to mars - Additionally use simple zoom accuracy if selected
			if (  t.gunzoommode != 0 && g.firemodes[t.gunid][g.firemode].settings.simplezoom != 0 ) 
			{
				t.trayaccuracy_f=g.firemodes[t.gunid][g.firemode].settings.simplezoomacc;
			}
			else
			{
				//  Temporary sniper rifle accuracy until weapon system overhauled. zoomaccuracy not used and can't figure out how
				//  all the zoommode shananigans work, so unable to use simple zoom. RMB (iron sight) for now
				if (  strcmp ( Lower(t.gun[t.gunid].name_s.Get())  ,  "modern\\sniperm700" ) == 0 && (MouseClick() & 2)  ==  0 ) 
				{
					t.trayaccuracy_f=g.firemodes[t.gunid][g.firemode].settings.accuracy;
					if (  t.trayaccuracy_f>15  )  t.trayaccuracy_f = 15;
				}
				else
				{
					if (  (t.plrkeySHIFT) == 1 && t.playercontrol.movement != 0 && g.firemodes[t.gunid][g.firemode].settings.runaccuracy != -1 ) 
					{
						t.trayaccuracy_f=g.firemodes[t.gunid][g.firemode].settings.runaccuracy;
					}
					else
					{
						t.trayaccuracy_f=g.firemodes[t.gunid][g.firemode].settings.accuracy;
					}
				}
			}

			//  Bullet Limbs code
			gun_updatebulletvisibility ( );

			// set iteration for ray shots
			if ( t.gun[t.gunid].settings.ismelee == 2 )
			{
				// melee attacks only ever have one iteration
				t.gunshootspread=1;
			}
			else
			{
				if (  t.trayiter>1 ) 
				{
					t.gunshootspread=t.trayiter;
				}
				else
				{
					t.gunshootspread=1;
				}
			}

			// the player's hit record: these rays are one trigger pull, of this gun (a melee strike is not a counted round)
			g_iPlayerHitPullShot = g_iPlayerGunShotCount;
			if ( t.gun[t.gunid].weapontype >= 51 || t.gun[t.gunid].settings.ismelee != 0 ) g_iPlayerHitPullShot = -1;
			g_iPlayerHitPullGunID = t.gunid;
			g_iPlayerHitPullRays = 0;

			// store camera position and angle so all rays originate from same location
			t.gunshootspreadposx = CameraPositionX(0);
			t.gunshootspreadposy = CameraPositionY(0);
			t.gunshootspreadposz = CameraPositionZ(0);
			t.gunshootspreadanglex = CameraAngleX(0);
			t.gunshootspreadangley = CameraAngleY(0);
			t.gunshootspreadanglez = CameraAngleZ(0);

			//  shot over
			t.gunshoot=0;
		}
		else
		{
			// flak is the projectile from the players weapon
			bool bNormalOrVRMode = false;
			bool bDoNotAdvanceToAvoidPenetration = false;
			t.flakid=g.firemodes[t.gunid][g.firemode].settings.flakindex;
			if (  t.flakid>0 ) 
			{
				//  find starting GetPoint (  for projectile )
				if (  t.playercontrol.thirdperson.enabled == 1 ) 
				{
					//  third person flak (fireball)
					t.flakangle_f=CameraAngleY(t.terrain.gameplaycamera);
					t.flakpitch_f=0;
					t.flakx_f=ObjectPositionX(t.aisystem.objectstartindex);
					t.flaky_f=ObjectPositionY(t.aisystem.objectstartindex);
					t.flakz_f=ObjectPositionZ(t.aisystem.objectstartindex);
					t.tattobj=t.entityelement[t.playercontrol.thirdperson.charactere].attachmentobj;
					if (  t.tattobj>0 ) 
					{
						if (  ObjectExist(t.tattobj) == 1 ) 
						{
							t.tattlimb=t.entityelement[t.playercontrol.thirdperson.charactere].attachmentobjfirespotlimb;
							t.flakx_f=LimbPositionX(t.tattobj,t.tattlimb);
							t.flaky_f=LimbPositionY(t.tattobj,t.tattlimb);
							t.flakz_f=LimbPositionZ(t.tattobj,t.tattlimb);
						}
					}
				}
				else
				{
					// first person flak
					t.flakangle_f=CameraAngleY();
					t.flakpitch_f=CameraAngleX();

					// can intercept calculated ray with real ray from VR controller (if available)
					extern int g_iActivelyUsingVRNow;
					if (g.vrglobals.GGVREnabled > 0 && g_iActivelyUsingVRNow == 1) bNormalOrVRMode = true;
					if (bNormalOrVRMode == true)
					{
						int iLaserGuideObj = GGVR_GetLaserGuideObject();
						if (iLaserGuideObj > 0 && ObjectExist(iLaserGuideObj) == 1)
						{
							t.flakangle_f=ObjectAngleY(iLaserGuideObj);
							t.flakpitch_f=ObjectAngleX(iLaserGuideObj);
						}
					}

					// check if clearance in front of player, so can place grenade perfectly
					t.flakx_f = CameraPositionX();
					t.flaky_f = CameraPositionY();
					t.flakz_f = CameraPositionZ();
					float fGrenadePosX = CameraPositionX() + NewXValue(0, t.flakangle_f + 45, 40);
					float fGrenadePosY = CameraPositionY();
					float fGrenadePosZ = CameraPositionZ() + NewZValue(0, t.flakangle_f + 45, 40);
					if (t.gun[t.gunid].settings.flashlimb != -1)
					{
						if (LimbExist(t.currentgunobj, t.gun[t.gunid].settings.flashlimb) == 1)
						{
							fGrenadePosX = LimbPositionX(t.currentgunobj, t.gun[t.gunid].settings.flashlimb);
							fGrenadePosY = LimbPositionY(t.currentgunobj, t.gun[t.gunid].settings.flashlimb);
							fGrenadePosZ = LimbPositionZ(t.currentgunobj, t.gun[t.gunid].settings.flashlimb);
						}
					}
					int iIgnoreObj = t.currentgunobj;
					float fAndTheAdvanceX = fGrenadePosX - t.flakx_f;
					float fAndTheAdvanceY = fGrenadePosY - t.flaky_f;
					float fAndTheAdvanceZ = fGrenadePosZ - t.flakz_f;
					float fAndTheAdvance = sqrt(fabs(fAndTheAdvanceX * fAndTheAdvanceX) + fabs(fAndTheAdvanceY * fAndTheAdvanceY) + fabs(fAndTheAdvanceZ * fAndTheAdvanceZ));
					fAndTheAdvanceX /= fAndTheAdvance;
					fAndTheAdvanceY /= fAndTheAdvance;
					fAndTheAdvanceZ /= fAndTheAdvance;
					fAndTheAdvanceX *= 60.0f;
					fAndTheAdvanceY *= 60.0f;
					fAndTheAdvanceZ *= 60.0f;
					int iHitTerrain = ODERayTerrain(t.flakx_f, t.flaky_f, t.flakz_f, fGrenadePosX+ fAndTheAdvanceX, fGrenadePosY+ fAndTheAdvanceY, fGrenadePosZ+ fAndTheAdvanceZ, false);
					int iHitSomething = 0;
					if (iHitTerrain == 0 ) iHitSomething = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, t.flakx_f, t.flaky_f, t.flakz_f, fGrenadePosX, fGrenadePosY, fGrenadePosZ, iIgnoreObj, 0, 0, 0, 0, false);
					if (iHitSomething != 0 || iHitTerrain != 0 )
					{
						// obstruction, place at player no advance shifting - leave at camera pos
						bDoNotAdvanceToAvoidPenetration = true;
					}
					else
					{
						t.flakx_f = fGrenadePosX;
						t.flaky_f = fGrenadePosY;
						t.flakz_f = fGrenadePosZ;
					}
				}
				//  create and launch projectile
				t.tProjectileType_s=t.gun[t.gunid].projectile_s  ; weapon_getprojectileid ( );
				if (  t.tProjectileType>0 ) 
				{
					t.tSourceEntity=0 ; t.tTracerFlag=0;
					t.tStartX_f=t.flakx_f ; t.tStartY_f=t.flaky_f ; t.tStartZ_f=t.flakz_f;
					t.tAngX_f=t.flakpitch_f ; t.tAngY_f=t.flakangle_f ; t.tAngZ_f=0;
					weapon_projectile_make ( bNormalOrVRMode, bDoNotAdvanceToAvoidPenetration);
					//  hide projectile which is part of HUD
					if (  g.firemodes[t.gunid][g.firemode].settings.flaklimb != -1 ) 
					{
						HideLimb (  t.currentgunobj,g.firemodes[t.gunid][g.firemode].settings.flaklimb );
					}
				}
			}

			//  shot over
			t.gunshoot=0;
		}

		// 200918 - trigger ai sound so enemies can pick up the shot
		t.tradius_f=2000;
		g.aidetectnearbymode = 1;
		g.aidetectnearbycount = 60*4;
		g.aidetectnearbymodeX_f = CameraPositionX();
		g.aidetectnearbymodeZ_f = CameraPositionZ();
	}

	// And can iterate more gunshoot rays if required
	// instead of many interations in one call, the iterations are
	// spread to one per cycle to reduce a 'freeze' effect
	if ( t.gunshootspread>0 ) 
	{
		// trigger another ray in iteration sequence (using stored camera values at time of shot)
		gun_shoot_oneray ( );
		--t.gunshootspread;
	}
}

void gun_shoot_oneray ( void )
{
	// get weapon/melee range
	t.range_f = g.firemodes[t.gunid][g.firemode].settings.range ; if (  t.range_f == 0  )  t.range_f = 3000;

	// 011215 - use firemode zero for melee range and damage (some store weapons only specify primary!)
	if ( t.gun[t.gunid].settings.ismelee == 2 ) t.range_f = g.firemodes[t.gunid][0].settings.meleerange;

	// work out weapon inaccuracies
	t.tca_f=Rnd(360000.0)/1000.0;
	t.tcx_f=Cos(t.tca_f) ; t.tcy_f=Sin(t.tca_f);
	t.tcm_f=Rnd(t.trayaccuracy_f*1000.0)/100000.0;
	if ( t.gunshootspread>1 && t.tcm_f<2.0 ) t.tcm_f = 2.0;
	t.tcx_f=t.tcx_f*t.tcm_f ; t.tcy_f=t.tcy_f*t.tcm_f;

	// project gun-Line for shot
	if ( t.playercontrol.thirdperson.enabled == 1 )
	{
		// if camera lock, always facing forward when firing
		float fRangeOfRay = 7000.0f;
		if ( t.playercontrol.thirdperson.cameralocked == 1 )
		{
			int iCharE = t.playercontrol.thirdperson.charactere;
			float fPlrAngle = ObjectAngleY(t.entityelement[iCharE].obj);
			t.x1_f = ObjectPositionX(t.entityelement[iCharE].obj);
			t.y1_f = ObjectPositionY(t.entityelement[iCharE].obj) + 50.0f;
			t.z1_f = ObjectPositionZ(t.entityelement[iCharE].obj);
			t.x2_f = NewXValue(t.x1_f, fPlrAngle, 100.0f);
			t.y2_f = t.y1_f;
			t.z2_f = NewZValue(t.z1_f, fPlrAngle, 100.0f);

			// reduce range and scatter distortion
			fRangeOfRay = 500.0f;
			t.tcx_f *= (500.0f/7000.0f);
			t.tcy_f *= (500.0f/7000.0f);
		}
		else
		{
			// special TPP shooting (from camera for accurate impact coordinate)
			t.x1_f = t.gunshootspreadposx;
			t.y1_f = t.gunshootspreadposy;
			t.z1_f = t.gunshootspreadposz;

			// work out cross-hair position on screen to distant 3D coordinate
			int iDisplayWidth = GetDisplayWidth();
			int iDisplayHeight = GetDisplayHeight();
			PickScreen2D23D(iDisplayWidth * 0.5f, iDisplayHeight * 0.25f, 7000.0f);
			t.x2_f = t.gunshootspreadposx + GetPickVectorX();
			t.y2_f = t.gunshootspreadposy + GetPickVectorY();
			t.z2_f = t.gunshootspreadposz + GetPickVectorZ();
		}

		// adjust destination vector with bullet inaccuacies
		PositionObject ( g.hudbankoffset+3, t.x1_f, t.y1_f, t.z1_f );
		PointObject ( g.hudbankoffset+3, t.x2_f, t.y2_f, t.z2_f );
		RotateObject ( g.hudbankoffset+3, ObjectAngleX(g.hudbankoffset+3)+t.tcy_f,ObjectAngleY(g.hudbankoffset+3)+t.tcx_f,ObjectAngleZ(g.hudbankoffset+3) );
		MoveObject ( g.hudbankoffset+3, fRangeOfRay );
	}
	else
	{
		// regular FPS shooting from camera center
		t.x1_f=t.gunshootspreadposx;
		t.y1_f=t.gunshootspreadposy;
		t.z1_f=t.gunshootspreadposz;
		PositionObject (  g.hudbankoffset+3,t.x1_f,t.y1_f,t.z1_f );
		t.tca_f=Rnd(360000.0)/1000.0;
		t.tcx_f=Cos(t.tca_f) ; t.tcy_f=Sin(t.tca_f);
		t.tcm_f=Rnd(t.trayaccuracy_f*1000.0)/100000.0;
		if (  t.gunshootspread>1 && t.tcm_f<2.0  )  t.tcm_f = 2.0;
		t.tcx_f=t.tcx_f*t.tcm_f ; t.tcy_f=t.tcy_f*t.tcm_f;
		RotateObject (  g.hudbankoffset+3, t.gunshootspreadanglex+t.tcy_f,t.gunshootspreadangley+t.tcx_f,t.gunshootspreadanglez );
		MoveObject (  g.hudbankoffset+3,t.range_f );
	}

	// final destination of bullet at temp obj (hub+3)
	t.tdropoff_f=((g.firemodes[t.gunid][g.firemode].settings.dropoff+0.0)*12.0*8.0)/100.0;
	t.tdropoff_f=(((8.0/100.0)*t.range_f)/100.0)*t.tdropoff_f;
	PositionObject ( g.hudbankoffset+3,ObjectPositionX(g.hudbankoffset+3),ObjectPositionY(g.hudbankoffset+3)-t.tdropoff_f,ObjectPositionZ(g.hudbankoffset+3) );
	DisableObjectZDepth ( g.hudbankoffset+3 );
	t.x2_f=ObjectPositionX ( g.hudbankoffset+3 );
	t.y2_f=ObjectPositionY ( g.hudbankoffset+3 );
	t.z2_f=ObjectPositionZ ( g.hudbankoffset+3 );

	// can intercept calculated ray with real ray from VR controller (if available)
	bool bNormalOrVRMode = false;
	//if (g.vrglobals.GGVREnabled > 0 && g.vrglobals.GGVRUsingVRSystem == 1) bNormalOrVRMode = true;
	extern int g_iActivelyUsingVRNow;
	if (g.vrglobals.GGVREnabled > 0 && g_iActivelyUsingVRNow == 1) bNormalOrVRMode = true;
	if (bNormalOrVRMode == true)
	{
		int iLaserGuideObj = GGVR_GetLaserGuideObject();
		if (iLaserGuideObj > 0 && ObjectExist(iLaserGuideObj) == 1)
		{
			// work out ray cast from laser object
			MoveObject(iLaserGuideObj, -100.0f);
			t.x1_f = ObjectPositionX(iLaserGuideObj);
			t.y1_f = ObjectPositionY(iLaserGuideObj);
			t.z1_f = ObjectPositionZ(iLaserGuideObj);
			MoveObject(iLaserGuideObj, 1000.0f);
			t.x2_f = ObjectPositionX(iLaserGuideObj);
			t.y2_f = ObjectPositionY(iLaserGuideObj);
			t.z2_f = ObjectPositionZ(iLaserGuideObj);
			MoveObject(iLaserGuideObj, -1000.0f);
		}
	}

	// any shot may be heard out there!
	if (g.firemodes[t.gunid][g.firemode].settings.noscorch == 0)
	{
		// if a scorchy shooty bullety thing
		t.tsx_f = CameraPositionX(0);
		t.tsy_f = CameraPositionY(0);
		t.tsz_f = CameraPositionZ(0);
		t.tradius_f = 1500;
		darkai_makesound_byplayer ();
	}

	// reset bullethit vars
	t.bullethit=0 ; t.bullethitstatic=0;
	t.tbullethitmaterial=0 ; t.tbullethitflesh=0;
	t.bulletraytype=g.firemodes[t.gunid][g.firemode].settings.damagetype;
	t.bulletfinalstrengthmod = 1.0f;
	t.bulletisinfactmeleestrike = 0;
	t.gunrange_f=t.range_f;

	//PE: Do not make a tracer if melee.
	//weapontype ; 0-grenade, 1-pistol, 2-rocket, 3-shotgun, 4-uzi, 5-assault, 51-melee(noammo)
	if( t.gun[t.gunid].weapontype < 50 && t.gun[t.gunid].settings.ismelee == 0)
	{
		if (t.gun[t.gunid].settings.tracer_active)
		{
			XMFLOAT3 tracer_from, tracer_hit;
			bool bFireTracer = true;
			//PE: If zoom mode ? you can't see it.
			if (g.gdisablerightmousehold > 0)
			{
				bFireTracer = false;
				//PE: Weapon always zoom.
				extern int iTracerPosition;
				if (iTracerPosition == 1 || iTracerPosition == 2)
				{
					//PE: From below.
					float move = -30;
					if( iTracerPosition == 2 )
						move = 30;
					MoveCameraDown(0, move);
					MoveCamera(0, 20);
					tracer_from.x = CameraPositionX();
					tracer_from.y = CameraPositionY();
					tracer_from.z = CameraPositionZ();
					MoveCamera(0, -20);
					move = 30;
					if (iTracerPosition == 2)
						move = -30;
					MoveCameraDown(0, move);

					tracer_hit.x = t.x2_f; // t.brayx2_f;
					tracer_hit.y = t.y2_f; // t.brayy2_f;
					tracer_hit.z = t.z2_f; // t.brayz2_f;

					XMVECTOR start = { tracer_from.x , tracer_from.y, tracer_from.z };// XMLoadFloat3(&tracer.startPos);
					XMVECTOR end = { tracer_hit.x , tracer_hit.y, tracer_hit.z };// XMLoadFloat3(&tracer.endPos);
					XMVECTOR dir = XMVectorSubtract(end, start);
					float length = XMVectorGetX(XMVector3Length(dir)); //Hit weapon ? *0.97;
					dir = XMVector3Normalize(dir);
					if (length > 500)
					{
						end = start + (dir * 500.0f);
						tracer_hit.x = XMVectorGetX(end);
						tracer_hit.y = XMVectorGetY(end);
						tracer_hit.z = XMVectorGetZ(end);
					}

					Tracers::AddTracer(
						tracer_from,
						tracer_hit,
						t.gun[t.gunid].settings.tracer_lifetime, // Lifetime
						XMFLOAT4(t.gun[t.gunid].settings.tracer_colorR, t.gun[t.gunid].settings.tracer_colorG, t.gun[t.gunid].settings.tracer_colorB, 1), // Color
						t.gun[t.gunid].settings.tracer_glow, // 5.0f, // Glow
						t.gun[t.gunid].settings.tracer_scrollV, // Scroll
						t.gun[t.gunid].settings.tracer_scaleV, // scaleV
						t.gun[t.gunid].settings.tracer_width, // width
						t.gun[t.gunid].settings.tracer_maxlength, // max length
						t.gunid //Image to use.
					);

				}
			}
			else
			{
				t.flakangle_f = CameraAngleY();
				t.flakpitch_f = CameraAngleX();

				extern int g_iActivelyUsingVRNow;
				if (g.vrglobals.GGVREnabled > 0 && g_iActivelyUsingVRNow == 1) bNormalOrVRMode = true;
				if (bNormalOrVRMode == true)
				{
					int iLaserGuideObj = GGVR_GetLaserGuideObject();
					if (iLaserGuideObj > 0 && ObjectExist(iLaserGuideObj) == 1)
					{
						t.flakangle_f = ObjectAngleY(iLaserGuideObj);
						t.flakpitch_f = ObjectAngleX(iLaserGuideObj);
					}
				}

				// check if clearance in front of player, so can place grenade perfectly
				t.flakx_f = CameraPositionX();
				t.flaky_f = CameraPositionY();
				t.flakz_f = CameraPositionZ();
				float fTracerPosX = CameraPositionX() + NewXValue(0, t.flakangle_f + 45, 40);
				float fTracerPosY = CameraPositionY();
				float fTracerPosZ = CameraPositionZ() + NewZValue(0, t.flakangle_f + 45, 40);
				//r_finger01
				int iSmokeLimb = t.gun[t.gunid].settings.smokelimb;
				if (iSmokeLimb <= 0)  iSmokeLimb = t.gun[t.gunid].settings.brasslimb;

				if (t.gun[t.gunid].settings.flashlimb != -1)
				{
					if (LimbExist(t.currentgunobj, t.gun[t.gunid].settings.flashlimb) == 1)
					{
						fTracerPosX = LimbPositionX(t.currentgunobj, t.gun[t.gunid].settings.flashlimb);
						fTracerPosY = LimbPositionY(t.currentgunobj, t.gun[t.gunid].settings.flashlimb);
						fTracerPosZ = LimbPositionZ(t.currentgunobj, t.gun[t.gunid].settings.flashlimb);
					}
				}
				else if (iSmokeLimb > 0)
				{
					// position from smoke or brass limb
					fTracerPosX = LimbPositionX(t.currentgunobj, iSmokeLimb);
					fTracerPosY = LimbPositionY(t.currentgunobj, iSmokeLimb);
					fTracerPosZ = LimbPositionZ(t.currentgunobj, iSmokeLimb);
				}
				else
				{
					gun_flashbrass_position(&fTracerPosX, &fTracerPosY, &fTracerPosZ, 1, 1, 1);
				}

				tracer_from.x = fTracerPosX;
				tracer_from.y = fTracerPosY;
				tracer_from.z = fTracerPosZ;

				tracer_hit.x = t.x2_f; // t.brayx2_f;
				tracer_hit.y = t.y2_f; // t.brayy2_f;
				tracer_hit.z = t.z2_f; // t.brayz2_f;

				XMVECTOR start = { tracer_from.x , tracer_from.y, tracer_from.z };// XMLoadFloat3(&tracer.startPos);
				XMVECTOR end = { tracer_hit.x , tracer_hit.y, tracer_hit.z };// XMLoadFloat3(&tracer.endPos);
				XMVECTOR dir = XMVectorSubtract(end, start);
				float length = XMVectorGetX(XMVector3Length(dir)); //Hit weapon ? *0.97;
				dir = XMVector3Normalize(dir);
				if (length > 500)
				{
					end = start + (dir * 500.0f);
					tracer_hit.x = XMVectorGetX(end);
					tracer_hit.y = XMVectorGetY(end);
					tracer_hit.z = XMVectorGetZ(end);
				}

				Tracers::AddTracer(
					tracer_from,
					tracer_hit,
					t.gun[t.gunid].settings.tracer_lifetime, // Lifetime
					XMFLOAT4(t.gun[t.gunid].settings.tracer_colorR, t.gun[t.gunid].settings.tracer_colorG, t.gun[t.gunid].settings.tracer_colorB, 1), // Color
					t.gun[t.gunid].settings.tracer_glow, // 5.0f, // Glow
					t.gun[t.gunid].settings.tracer_scrollV, // Scroll
					t.gun[t.gunid].settings.tracer_scaleV, // scaleV
					t.gun[t.gunid].settings.tracer_width, // width
					t.gun[t.gunid].settings.tracer_maxlength, // max length
					t.gunid //Image to use.
				);
			}
		}
	}

	// if this was a 'melee weapon' (51 or above)
	if (t.gun[t.gunid].weapontype >= 51 || t.gun[t.gunid].settings.ismelee != 0)
	{
		// mark this as a melee weapon strike (will add a thump sound later on)
		t.bulletisinfactmeleestrike = 1;

		// and the ray did not quite find a surface to strike
		bool bFullWickedAccuracy = true;
		int tquickrayhitchecke = -1;
		t.thitvalue = IntersectAllEx (g.entityviewstartobj, g.entityviewendobj, t.x1_f, t.y1_f, t.z1_f, t.x2_f, t.y2_f, t.z2_f, 0, 0, 0, 0, 0, bFullWickedAccuracy);
		if ( t.thitvalue == 0 )
		{
			// now lets find someone standing RIGHT in front of our sledgehammer :)
			int tfoundalikelyvictim = -1;
			float fStraightOnDamageRatio = 1.0;
			float fClosest = 9999;
			float fMostInFront = 180;
			for (t.tte = 1; t.tte <= g.entityelementlist; t.tte++)
			{
				if (t.entityelement[t.tte].obj > 0)
				{
					int entid = t.entityelement[t.tte].bankindex;
					if (entid > 0)
					{
						if (t.entityprofile[entid].ischaracter != 0)
						{
							if (t.entityelement[t.tte].health > 0)
							{
								float fDX = t.entityelement[t.tte].x - t.x1_f;
								float fDZ = t.entityelement[t.tte].z - t.z1_f;
								float fDD = sqrt(fabs(fDX * fDX) + fabs(fDZ * fDZ));
								if (fDD < t.gunrange_f)
								{
									float fDA = atan2(fDX, fDZ) * 180.0f / 3.14159265358979323846f;
									fDA = WrapValue(fDA);
									float fDiffA = fDA - CameraAngleY();
									if (fDiffA < 0) fDiffA += 360;
									if (fDiffA > 180) fDiffA -= 360;
									float fWindowOfOp = 35.0f;
									fDiffA = fabs(fDiffA);
									if (fDiffA < fWindowOfOp)
									{
										// stronger damage is head on
										fStraightOnDamageRatio = 1.0f - (fDiffA / fWindowOfOp);
										
										// this victim is a character, alive, within range and less than 45 degrees in front of player
										if (fDD < fClosest)
										{
											fClosest = fDD;
											if (fDiffA < fMostInFront)
											{
												fMostInFront = fDiffA;
												tfoundalikelyvictim = t.tte;
											}
										}
									}
								}
							}
						}
					}
				}
			}
			if (tfoundalikelyvictim > 0)
			{
				//if we have a character in front of us, find the chest area	
				int iObj = t.entityelement[tfoundalikelyvictim].obj;
				t.headlimbofcharacter = t.entityprofile[t.entityelement[tfoundalikelyvictim].bankindex].headlimb;
				t.spine2limbofcharacter = t.entityprofile[t.entityelement[tfoundalikelyvictim].bankindex].spine2;
				int iChosenLimb = 0;
				if (t.headlimbofcharacter > 0 && LimbExist(iObj, t.headlimbofcharacter) == 1) iChosenLimb = t.headlimbofcharacter;
				if (t.spine2limbofcharacter > 0 && LimbExist(iObj, t.spine2limbofcharacter) == 1) iChosenLimb = t.spine2limbofcharacter;
				float fCharacterChestAreaX, fCharacterChestAreaY, fCharacterChestAreaZ;
				if (LimbExist(iObj, iChosenLimb) == 1)
				{
					fCharacterChestAreaX = LimbPositionX(iObj, iChosenLimb);
					fCharacterChestAreaY = LimbPositionY(iObj, iChosenLimb);
					fCharacterChestAreaZ = LimbPositionZ(iObj, iChosenLimb);
				}
				else
				{
					// this does not work if character flat on floor!
					fCharacterChestAreaX = t.entityelement[tfoundalikelyvictim].x;
					fCharacterChestAreaY = t.entityelement[tfoundalikelyvictim].y + 50.0f;
					fCharacterChestAreaZ = t.entityelement[tfoundalikelyvictim].z;
				}

				// and create an artificial hit on them to improve the fast-paced melee combat when up close
				// (i.e. swinging a massive sledgehammer at a character, and the ray targets below the armpit will stil hurt like heck)
				PositionObject (g.hudbankoffset + 3, fCharacterChestAreaX, fCharacterChestAreaY, fCharacterChestAreaZ);
				DisableObjectZDepth (g.hudbankoffset + 3);
				t.x2_f = ObjectPositionX (g.hudbankoffset + 3);
				t.y2_f = ObjectPositionY (g.hudbankoffset + 3);
				t.z2_f = ObjectPositionZ (g.hudbankoffset + 3);

				// weaken final damage if player not quite lined up with character during the almost miss strike
				t.bulletfinalstrengthmod = fStraightOnDamageRatio;
			}
		}
	}

	// raycast to entity, recorded in the player's hit record: kind 1 a round, 2 a later ray (pellet) of the same pull, 3 melee
	int iPlayerHitKind = 1;
	if ( g_iPlayerHitPullRays > 0 ) iPlayerHitKind = 2;
	if ( t.bulletisinfactmeleestrike == 1 ) iPlayerHitKind = 3;
	g_iPlayerHitPullRays++;
	playerhit_openray ( iPlayerHitKind, g_iPlayerHitPullShot, g_iPlayerHitPullGunID, t.x1_f, t.y1_f, t.z1_f );
	entity_hasbulletrayhit ( );
	playerhit_close ( );
}

void gun_soundcontrol ( void )
{
	// PlaySound ( frames when GetFrame ( matches ) )
	if (  t.gun[t.gunid].sound.soundframes>0 ) 
	{
		for ( t.p = 0 ; t.p <= t.gun[t.gunid].sound.soundframes; t.p++ )
		{
			if ( t.p < 100) // ensure cannot access sounditem items out of bounds!
			{
				t.sndid = t.gunsound[t.gunid][t.gunsounditem[t.gunid][t.p].playsound].soundid1;
				if ((t.gun[t.gunid].action.automatic.s > 0 && t.gun[t.gunid].settings.alternate == 0) && t.p == 0 || t.gun[t.gunid].altaction.automatic.s > 0 && t.gun[t.gunid].settings.alternate == 1 && t.gun[t.gunid].settings.alternateisray == 1 && t.p == 0)  t.sndid = 0;
				if (t.sndid > 0)
				{
					if (int(t.gunsounditem[t.gunid][t.p].keyframe) == int(GetFrame(t.currentgunobj)))
					{
						if (SoundExist(t.sndid) == 1)
						{
							if (t.gunsounditem[t.gunid][t.p].lastplay == 0)
							{
								t.gunsounditem[t.gunid][t.p].lastplay = 1;
								posinternal3dsound(t.sndid, CameraPositionX(), CameraPositionY(), CameraPositionZ());
								if (SoundExist(t.sndid) == 1)
								{
									PlaySound(t.sndid);
								}
							}
						}
					}
					else
					{
						t.gunsounditem[t.gunid][t.p].lastplay = 0;
					}
				}
			}
		}
	}
}

void gun_create_hud ( void )
{
	// Only create if not already exist
	if ( ObjectExist(g.hudbankoffset+2) == 0 ) 
	{
		// Setup HUD Center Marker
		MakeObjectBox (  g.hudbankoffset+2,30,100,30 );
		SetObjectCollisionOff (  g.hudbankoffset+2 );
		SetObjectMask(g.hudbankoffset + 2, 1);
		HideObject (  g.hudbankoffset+2 );

		// Setup HUD Gun-Line (  Marker (shows impact coord) )
		MakeObjectCube (  g.hudbankoffset+3,5 );
		SetObjectCollisionOff (  g.hudbankoffset+3 );
		SetObjectMask(g.hudbankoffset + 3, 1);
		HideObject (  g.hudbankoffset+3 );

		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);

		// Muzzle Flash(es)
		for ( t.t = 0; t.t <= 1; t.t++ )
		{
			if (  t.t == 0  )  t.tobj = g.hudbankoffset+5;
			if (  t.t == 1  )  t.tobj = g.hudbankoffset+32;
			MakeObjectPlane (t.tobj, 25, 25);
			SetObjectCollisionOff ( t.tobj );
			// no special Z treatment
			//DisableObjectZWrite ( t.tobj );
			SetObjectTransparency ( t.tobj, 6 );
			SetObjectAmbient (  t.tobj,0 );
			SetObjectLight (  t.tobj,0 );
			SetObjectFOV (  t.tobj,37 );
			SetObjectMask(t.tobj, 1); //PE: Dont interfere with shadow camera.
			HideObject (  t.tobj );
			SetObjectEffect ( t.tobj, g.decaleffectoffset );
			sObject* pObject = GetObjectData(t.tobj);
			if (pObject) 
			{
				WickedCall_SetObjectCastShadows(pObject, false);
				WickedCall_SetObjectLightToUnlit(pObject, (int)wiScene::MaterialComponent::SHADERTYPE_UNLIT);
			}

		}

		// Brass
		for ( t.o = 6 ; t.o <= 20; t.o++ )
		{
			t.obj=g.hudbankoffset+t.o;
			MakeObjectCube ( t.obj,10 );
			SetObjectCollisionOff ( t.obj );
			DisableObjectZDepthEx ( t.obj, 1 );
			SetObjectMask(t.tobj, 1); //PE: Dont interfere with shadow camera.
			HideObject ( t.obj );

			// apply all textures applied to this weapon
			sObject* pObject = GetObjectData(t.obj);
			if (pObject) 
			{
				WickedCall_SetObjectCastShadows(pObject, false);
			}
		}

		// Smoke
		for ( t.o = 21 ; t.o<=  30; t.o++ )
		{
			t.obj=g.hudbankoffset+t.o;
			MakeObjectPlane (t.obj, 50, 50);
			SetObjectCollisionOff (  t.obj );
			SetObjectTransparency ( t.obj, 6 );
			// no special Z treatment
			SetObjectAmbient (  t.obj,0 );
			SetObjectLight (  t.obj,0 );
			SetObjectFOV (  t.obj,37 );
			SetObjectMask(t.obj, 1); //PE: Dont interfere with shadow camera.
			HideObject (  t.obj );
			SetObjectEffect(t.obj, g.decaleffectoffset);

			sObject* pObject = GetObjectData(t.obj);
			if (pObject) 
			{
				WickedCall_SetObjectCastShadows(pObject, false);
				WickedCall_SetObjectLightToUnlit(pObject, (int)wiScene::MaterialComponent::SHADERTYPE_UNLIT);
			}
		}

		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
	}
}

void gun_setup ( void )
{
	//  Create common resources for gun
	gun_create_hud ( );
}

void gun_gatherslotorder_load(void)
{
	// on the off chance we need the latest refreshed gunlist
	extern bool g_bGunListNeedsRefreshing;
	g_bGunListNeedsRefreshing = true;

	t.tslotmax = 0;
	g.gunslotmax = 0;
	LPSTR pWeaponSlotFile = "editors\\keymap\\weaponslots.dat";
	if (FileExist(pWeaponSlotFile) == 1)
	{
		Dim (t.data_s, 100);
		LoadArray (pWeaponSlotFile, t.data_s);
		for (t.l = 0; t.l <= 99; t.l++)
		{
			t.line_s = t.data_s[t.l];
			if (Len(t.line_s.Get()) > 0)
			{
				if (strcmp (Left(t.line_s.Get(), 1), ";") != 0)
				{
					// get field and value strings
					for (t.c = 0; t.c < Len(t.line_s.Get()); t.c++)
					{
						if (t.line_s.Get()[t.c] == '=') { t.mid = t.c + 1; break; }
					}
					t.field_s = Lower(removeedgespaces(Left(t.line_s.Get(), t.mid - 1)));
					t.value_s = removeedgespaces(Right(t.line_s.Get(), Len(t.line_s.Get()) - t.mid));

					// gather gun type from slot
					for (t.tww = 1; t.tww <= 9; t.tww++)
					{
						t.tryfield_s = "slot";
						t.tryfield_s += Str(t.tww);
						if (t.field_s == t.tryfield_s)
						{
							// find gun id from name
							t.findgun_s = t.value_s;
							gun_findweaponindexbyname ();
							t.weaponslot[t.tww].pref = t.foundgunid;
							t.weaponSlotPreferrenceSettings[t.tww-1] = t.foundgunid;
							if (t.foundgunid > 0)  g.gunslotmax = t.tww;
						}
					}
				}
			}
		}
		UnDim (t.data_s);
	}
}

void gun_gatherslotorder_save(void)
{
	LPSTR pWeaponSlotFile = "editors\\keymap\\weaponslots.dat";
	Dim (t.data_s, 100);
	t.data_s[0] = ";Weapon Slots Configuration for Player";
	for (t.tww = 1; t.tww <= 9; t.tww++)
	{
		t.data_s[t.tww] = cstr("slot") + cstr(t.tww) + "=";
		//t.foundgunid = t.weaponslot[t.tww].pref;
		t.foundgunid = t.weaponSlotPreferrenceSettings[t.tww-1];
		if (t.foundgunid > 0)
		{
			t.data_s[t.tww] = t.data_s[t.tww] + cstr(Lower(t.gun[t.foundgunid].name_s.Get()));
		}
	}
	if (FileExist(pWeaponSlotFile) == 1) DeleteFileA(pWeaponSlotFile);
	SaveArray (pWeaponSlotFile, t.data_s);
	UnDim (t.data_s);
}

void gun_gatherslotorder (void)
{
	gun_gatherslotorder_load();
}

void gun_selectandorload ( void )
{
	// Load gun if not selected
	// GGMAX 3.122: ...or if it is in memory but was built for a DIFFERENT Player Start arm set than the
	// one this game uses (g_guns_loadedArms, G-Gun_part0.cpp). gun_load then rebuilds only what the arm set
	// changes - its own refresh test decides - and returns at once for a gun whose arms cannot change.
	bool bArmsOutOfDate = false;
	if ( t.gun[t.gunid].obj != 0 )
	{
		std::map<int, std::string>::const_iterator itArms = g_guns_loadedArms.find(t.gunid);
		if ( itArms != g_guns_loadedArms.end() && stricmp(itArms->second.c_str(), g_guns_customArms_s.Get()) != 0 )
			bArmsOutOfDate = true;
	}
	if ( t.gun[t.gunid].obj == 0 || bArmsOutOfDate )
	{
		gun_load ( );
	}

	//  Associate gun with player
	t.currentgunobj=t.gun[t.gunid].obj;

	WickedCall_PresetObjectPutInEmissive(1);

	//  Setup gun with muzzle flash image
	if (  t.gun[t.gunid].settings.flashlimb != -1 ) 
	{
		TextureObject ( g.hudbankoffset+5, 0, g.firemodes[t.gunid][0].settings.flashimg );
		PositionObject(g.hudbankoffset + 5, 0, 0, 0);
		t.size_f = g.firemodes[t.gunid][0].settings.muzzlesize_f ; if (  t.size_f == 0.0  )  t.size_f = 100.0;
		ScaleObject (  g.hudbankoffset+5,t.size_f,t.size_f,t.size_f );
	}
	if (  t.gun[t.gunid].settings.flashlimb2 != -1 ) 
	{
		TextureObject (  g.hudbankoffset+32,0,g.firemodes[t.gunid][0].settings.flashimg );
		PositionObject(g.hudbankoffset + 32, 0, 0, 0);
		t.size_f = g.firemodes[t.gunid][0].settings.muzzlesize_f ; if (  t.size_f == 0.0  )  t.size_f = 100.0;
		ScaleObject (  g.hudbankoffset+32,t.size_f,t.size_f,t.size_f );
	}
	else
	{
		ScaleObject (  g.hudbankoffset+32,0,0,0 );
	}

	//  Setup gun with smoke images
	if (  t.gun[t.gunid].settings.smokelimb != -1 ) 
	{
		for ( t.o = 21 ; t.o<=  30; t.o++ )
		{
			t.obj=g.hudbankoffset+t.o;
			if (g.firemodes[t.gunid][g.firemode].settings.smokeimg == 0)
			{
				ScaleObject (t.obj,0,0,0);
				SetObjectMask (t.obj, 0);
			}
			else
			{
				TextureObject (t.obj, g.firemodes[t.gunid][g.firemode].settings.smokeimg);
				SetObjectEffect (t.obj, g.decaleffectoffset);
				SetObjectMask (t.obj, 1);
			}
			HideObject (  t.obj );
			SetObjectCull ( t.obj, 0 );
			SetObjectTransparency ( t.obj, 6 );
			DisableObjectZWrite ( t.obj );
		}
	}

	WickedCall_PresetObjectPutInEmissive(0);

	// Setup gun with brass models
	if ( t.gun[t.gunid].settings.brasslimb != -1 ) 
	{
		extern bool bBlockSceneUpdate;
		bBlockSceneUpdate = true;

		for ( t.o = 6 ; t.o<=  20; t.o++ )
		{
			t.obj=g.hudbankoffset+t.o;
			extern bool bNoHierarchySorting;
			bNoHierarchySorting = true;

			if ( ObjectExist(t.obj) == 1 )  
			{
				ODEDestroyObject ( t.obj );
				DeleteObject ( t.obj ); //PE: Slow as wicked update everything on each call.
			}
			if (  g.firemodes[t.gunid][g.firemode].settings.brassobjmaster == 0 ) 
			{
				MakeObjectCube ( t.obj, 0 );
			}
			else
			{
				InstanceObject ( t.obj, g.firemodes[t.gunid][g.firemode].settings.brassobjmaster );
				//LB: strangely brass has no textures, needed to call it here
				sObject* pObject = GetObjectData(t.obj);
				WickedCall_TextureObject(pObject, NULL);
			}
			bNoHierarchySorting = false;
			SetObjectCollisionOff (  t.obj );
			SetObjectFOV (  t.obj,37 );
			HideObject (  t.obj );
		}

		bBlockSceneUpdate = false;
		//PE: When all done, update scene , it takes some time but we dont have to do it 20 times.
		WickedCall_UpdateSceneForPick();

	}
}


// GGMAX 3.122: free every sound id one gun holds - main and variations, companions, alt - and zero them,
// as gun_releaseresources does (gated on the id, not the name, which gun_loaddata may have re-read).
// Used when gun_load (G-Gun_part3.cpp, same unity build) rebuilds a gun already in memory, before it
// reloads them under new ids.
static void gun_freegunsounds(int gunid)
{
	if (gunid <= 0 || gunid >= (int)t.gunsound.size()) return;
	for (int i = 0; i <= 15; i++)
	{
		int* pIds[5] = { &t.gunsound[gunid][i].soundid1, &t.gunsound[gunid][i].soundid2,
			&t.gunsound[gunid][i].soundid3, &t.gunsound[gunid][i].soundid4, &t.gunsound[gunid][i].altsoundid };
		for (int k = 0; k < 5; k++)
		{
			if (k == 4 && i == 2) continue;	// slot 2 has no alt sound (never loaded or released for it)
			if (*pIds[k] > 0)
			{
				if (SoundExist(*pIds[k]) == 1) DeleteSound(*pIds[k]);
				*pIds[k] = 0;
			}
		}
		if (i <= 3)
		{
			for (int c = 0; c < 3; c++)
			{
				int& id = t.gunsoundcompanion[gunid][i][c].soundid;
				if (id > 0)
				{
					if (SoundExist(id) == 1) DeleteSound(id);
					id = 0;
				}
			}
		}
	}
}

// GGMAX 3.122b: the custom-arms ("legacy arm replacement trick") pieces of gun_load (G-Gun_part3.cpp, same
// unity build), shared by a gun's FIRST load and by the ARMS-ONLY refresh of a gun already in memory whose
// Player Start arm set changed - so the two can never drift apart. The arm set changes nothing else about an
// arm-swap gun: re-running all of gun_load for it (3.122) re-read gunspec (reverting Lua / entity weapon
// changes), reloaded its sounds and brass, and re-applied its rotation pivot to the reused object.
//
// Build the secondary arms object for gun object iGunObj: delete the old one, then (a set chosen and the gun
// has HUD-ORIG.dbo) load hands\<set>\arms.dbo textured with the set's arms_color.dds and give it the
// Legacy<anim>.dbo animation. Returns true when the trick is in use. customArms_s may come back as the
// fallback set. Uses t.gunid / t.gun_s - the gun being loaded, as in gun_load.
static bool gun_loadlegacyarms(int iGunObj, cstr& customArms_s, LPSTR pLegacyGunFile)
{
	if (iGunObj <= 0) return false;	// (the inline code could LoadObject into object 0 here)
	const int iGunSecondaryObj = g.gunbankextraobjoffset + (iGunObj - g.gunbankoffset);
	if (ObjectExist(iGunSecondaryObj) == 1) DeleteObject(iGunSecondaryObj);
	bool bUsingLegacyArmReplacementTrick = false;
	if (customArms_s.Len() > 0)
	{
		if (FileExist(pLegacyGunFile) == 1)
		{
			// load secondary hands 
			cstr pHands_s = g.fpscrootdir_s + cstr("\\Files\\gamecore\\hands\\") + customArms_s + cstr("\\arms.dbo");
			if(FileExist(pHands_s.Get())==0)
			{
				// player start marker specified hands not exist on this install, so default to legacy ones
				customArms_s = "Combat Gloves Light";
				pHands_s = g.fpscrootdir_s + cstr("\\Files\\gamecore\\hands\\") + customArms_s + cstr("\\arms.dbo");
			}
			if (FileExist(pHands_s.Get()) == 1)
			{
				// load the hands
				LoadObject(pHands_s.Get(), iGunSecondaryObj);

				// texture these seoncdary arms/hands
				cstr pCustomArms_s = cstr("gamecore\\hands\\") + customArms_s + cstr("\\arms_color.dds");
				if (ImageExist(g.weaponstempimageoffset) == 1) DeleteImage(g.weaponstempimageoffset);
				LoadImage(pCustomArms_s.Get(), g.weaponstempimageoffset);
				TextureObject(iGunSecondaryObj, g.weaponstempimageoffset);

				// apply correct legacy animations
				cstr pAbsPathToAnim = g.fpscrootdir_s + cstr("\\Files\\gamecore\\hands\\Animations\\Legacy");
				char pNoSpacesInGunName[MAX_PATH];
				strcpy(pNoSpacesInGunName, "");
				int n2 = 0;
				LPSTR pGunPathAndName = t.gun_s.Get();
				for (int n = 0; n < strlen(pGunPathAndName); n++)
				{
					if (pGunPathAndName[n] == ' ' || pGunPathAndName[n] == '\\' || pGunPathAndName[n] == '/')
					{
					}
					else
					{
						pNoSpacesInGunName[n2++] = pGunPathAndName[n];
					}
				}
				pNoSpacesInGunName[n2] = 0;

				if (t.gun[t.gunid].legacy_animation_s.Len() > 0)
				{
					cstr path = pAbsPathToAnim;
					path += t.gun[t.gunid].legacy_animation_s;
					path += ".dbo";
					if (!FileExist(path.Get()))
					{
						pAbsPathToAnim += pNoSpacesInGunName;// "EnhancedAK";
						pAbsPathToAnim += ".dbo";
					}
					else
					{
						pAbsPathToAnim = path;
					}
				}
				else
				{
					pAbsPathToAnim += pNoSpacesInGunName;// "EnhancedAK";
					pAbsPathToAnim += ".dbo";
				}

				sObject* pSecondaryObject = GetObjectData(iGunSecondaryObj);
				if (pSecondaryObject && FileExist(pAbsPathToAnim.Get()))
				{
					if (AppendAnimationFromFile(pSecondaryObject, pAbsPathToAnim.Get(), 0) == true)
					{
						WickedCall_RefreshObjectAnimations(pSecondaryObject, pSecondaryObject->wickedloaderstateptr);
					}
					bUsingLegacyArmReplacementTrick = true;
				}
				else
				{
					//PE: Anim not there , cant replace arms.
					if (ObjectExist(iGunSecondaryObj) == 1)
						DeleteObject(iGunSecondaryObj);
					bUsingLegacyArmReplacementTrick = false;
				}
				if (ObjectExist(iGunSecondaryObj) == 1)
					HideObject(iGunSecondaryObj);
			}
		}
	}
	return bUsingLegacyArmReplacementTrick;
}

// The gun's own LEGACYARMS limb: hidden while the trick supplies the arms, otherwise shown again. A refresh to
// No Preference reuses the gun object an earlier build hid it on. The mesh flag is what ShowObject honours
// (WickedCall_SetObjectVisible); DX12 ShowLimb would make the limb renderable even on a hidden gun, so the
// Wicked limb is only made visible directly when the gun itself is showing.
static void gun_applylegacyarmslimb(int iGunObj, bool bUsingLegacyArmReplacementTrick)
{
	extern void WickedCall_SetLimbVisible(sFrame* pFrame, bool bVisible);
	if (iGunObj <= 0 || ObjectExist(iGunObj) != 1) return;
	sObject* pObj = GetObjectData(iGunObj);
	if (!pObj) return;
	for (int f = 0; f < pObj->iFrameCount; f++)
	{
		sFrame* pFrame = pObj->ppFrameList[f];
		if (!pFrame || stricmp(pFrame->szName, "LEGACYARMS") != 0) continue;
		if (bUsingLegacyArmReplacementTrick == true)
		{
			HideLimb(iGunObj, f);
		}
		else if (pFrame->pMesh && pFrame->pMesh->bVisible == false)
		{
			pFrame->pMesh->bVisible = true;
			if (GetVisible(iGunObj) == 1) WickedCall_SetLimbVisible(pFrame, true);
		}
	}
}

// Attach the secondary arms object the way gun_load attaches the gun: glued to the HUD gun marker, the gun's
// transparency mode, no Z depth.
static void gun_attachlegacyarms(int iGunObj)
{
	if (iGunObj <= 0) return;
	const int iGunSecondaryObj = g.gunbankextraobjoffset + (iGunObj - g.gunbankoffset);
	if (ObjectExist(iGunSecondaryObj) != 1) return;
	// anim sync did not work as n-core anim system use different read/write values per frame - alas
	//int iMode = 3; // mode 3 syncs the secondary object to the primary object anim timer with no lag (pass in the animating objID (can be different from obj being glued to)
	GlueObjectToLimbEx (iGunSecondaryObj, g.hudbankoffset + 2, 0, 0);// t.currentgunobj);
	if (t.gun[t.gunid].transparency > 2)
		SetObjectTransparency (iGunSecondaryObj, t.gun[t.gunid].transparency);
	else
		SetObjectTransparency (iGunSecondaryObj, 2);
	DisableObjectZDepth (iGunSecondaryObj);
}
