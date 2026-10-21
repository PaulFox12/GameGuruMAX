void entity_lua_rotatez ( void )
{
	t.v_f=(t.v_f/19.775)*g.timeelapsed_f;
	t.entityelement[t.e].rz=t.entityelement[t.e].rz+t.v_f;
	entity_lua_rotateupdate ( );
}

void entity_lua_setlimbindex ( void )
{
	int iObj = t.entityelement[t.e].obj;
	if ( iObj > 0 )
	{
		if ( ObjectExist(iObj)==1 )
		{
			if ( t.v < GetObjectData(iObj)->iFrameCount )
			{
				t.lualimbindex = t.v;
			}
		}
	}
}

void entity_lua_rotatelimbx ( void )
{
	int iObj = t.entityelement[t.e].obj;
	if ( iObj > 0 )
	{
		if ( ObjectExist(iObj)==1 )
		{
			if ( t.lualimbindex >= 0 && t.lualimbindex < GetObjectData(iObj)->iFrameCount )
			{
				RotateLimb ( iObj, t.lualimbindex, t.v_f, LimbAngleY(iObj, t.lualimbindex), LimbAngleZ(iObj, t.lualimbindex) );
				WickedCall_UpdateLimbFromFrame ( GetObjectData(iObj), t.lualimbindex ); // RotateLimb only writes the DBO frame
			}
		}
	}
}
void entity_lua_rotatelimby ( void )
{
	int iObj = t.entityelement[t.e].obj;
	if ( iObj > 0 )
	{
		if ( ObjectExist(iObj)==1 )
		{
			if ( t.lualimbindex >= 0 && t.lualimbindex < GetObjectData(iObj)->iFrameCount )
			{
				RotateLimb ( iObj, t.lualimbindex, LimbAngleX(iObj, t.lualimbindex), t.v_f, LimbAngleZ(iObj, t.lualimbindex) );
				WickedCall_UpdateLimbFromFrame ( GetObjectData(iObj), t.lualimbindex );
			}
		}
	}
}
void entity_lua_rotatelimbz ( void )
{
	int iObj = t.entityelement[t.e].obj;
	if ( iObj > 0 )
	{
		if ( ObjectExist(iObj)==1 )
		{
			if ( t.lualimbindex >= 0 && t.lualimbindex < GetObjectData(iObj)->iFrameCount )
			{
				RotateLimb ( iObj, t.lualimbindex, LimbAngleX(iObj, t.lualimbindex), LimbAngleY(iObj, t.lualimbindex), t.v_f );
				WickedCall_UpdateLimbFromFrame ( GetObjectData(iObj), t.lualimbindex );
			}
		}
	}
}

void entity_lua_scale ( void )
{
	t.entityelement[t.e].eleprof.scale=t.v_f;
	entity_lua_scaleupdate ( );
}

void entity_lua_setanimation ( void )
{
	t.luaglobal.setanim=t.e;
	if (  t.game.runasmultiplayer  ==  1 && t.tLuaDontSendLua  ==  0 ) 
	{
		mp_sendlua (  MP_LUA_SetAnimation,t.e,0 );
	}
}

int entity_lua_getanimationnamefromobject (sObject* pObject, cstr FindThisName_s, float* fFoundStart, float* fFoundFinish)
{
	int iFoundBest = 0;
	LPSTR pFindThisName = FindThisName_s.Get();
	cStr lowercase_s = FindThisName_s.Lower();
	if (pObject)
	{
		for (int iSearchPatterns = 0; iSearchPatterns < 5 && iFoundBest == 0; iSearchPatterns++)
		{
			cStr lowercase_meaning_s = "";
			if (iSearchPatterns == 4)
			{
				// if all else fails, then check if animset name contains part of the general meaning of the name supplied by the script, case insensitive
				// used in situations where an old model uses something like Attack_01 but newer scripts use improved names such as Attack_Floor, etc
				// so we extract the general meaning of the new script animation name and match against a likely anim from the animset
				int iWritePosition = 0;
				char pLowercaseMeaning[MAX_PATH];
				LPSTR pLowercase = lowercase_s.Get();
				bool bStartGrabbingNow = false;
				for (int n = 0; n < strlen(pLowercase); n++)
				{
					if (pLowercase[n] >= 'a' && pLowercase[n] <= 'z')
					{
						if (bStartGrabbingNow == false) bStartGrabbingNow = true;
						pLowercaseMeaning[iWritePosition] = pLowercase[n];
						iWritePosition++;
					}
					else
					{
						if (bStartGrabbingNow == true)
						{
							// after we started grabbingthing, and the letters ran out, stop constructing string
							break;
						}
					}
				}
				pLowercaseMeaning[iWritePosition] = 0;
				lowercase_meaning_s = pLowercaseMeaning;
			}

			int iAnimSetCount = 1;
			sAnimationSet* pAnimSet = pObject->pAnimationSet;
			while (pAnimSet)
			{
				if (iSearchPatterns == 0)
				{
					// exact match and case match
					if (strcmp(pFindThisName, pAnimSet->szName) == NULL)
					{
						*fFoundStart = pAnimSet->fAnimSetStart;
						*fFoundFinish = pAnimSet->fAnimSetFinish;
						if (pAnimSet == pObject->pAnimationSet) *fFoundFinish = pAnimSet->ulLength;
						iFoundBest = iAnimSetCount;
						break;
					}
				}
				if (iSearchPatterns == 1)
				{
					// exact match and case insensitive
					char pAnimSetNameLower[MAX_PATH];
					strcpy (pAnimSetNameLower, pAnimSet->szName);
					strlwr(pAnimSetNameLower);
					if (strcmp(lowercase_s.Get(), pAnimSetNameLower) == NULL)
					{
						*fFoundStart = pAnimSet->fAnimSetStart;
						*fFoundFinish = pAnimSet->fAnimSetFinish;
						if (pAnimSet == pObject->pAnimationSet) *fFoundFinish = pAnimSet->ulLength;
						iFoundBest = iAnimSetCount;
						break;
					}
				}
				if (iSearchPatterns == 2)
				{
					// animset name contains the name, case insensitive, and have some specific cases we can watch out for
					char pAnimSetNameLower[MAX_PATH];
					strcpy (pAnimSetNameLower, pAnimSet->szName);
					strlwr(pAnimSetNameLower);
					if (strcmp (pAnimSetNameLower, "walk loop") == NULL) strcpy(pAnimSetNameLower, "walk"); // CCP can have "walk start", "walk loop" and walk finish" (need walk loop!)
					if (strcmp (pAnimSetNameLower, lowercase_s.Get()) == NULL)
					{
						*fFoundStart = pAnimSet->fAnimSetStart;
						*fFoundFinish = pAnimSet->fAnimSetFinish;
						if (pAnimSet == pObject->pAnimationSet) *fFoundFinish = pAnimSet->ulLength;
						iFoundBest = iAnimSetCount;
						break;
					}
				}
				if (iSearchPatterns == 3)
				{
					// animset name contains the name 'somewhere', case insensitive
					char pAnimSetNameLower[MAX_PATH];
					strcpy (pAnimSetNameLower, pAnimSet->szName);
					strlwr(pAnimSetNameLower);
					if (strstr(pAnimSetNameLower, lowercase_s.Get()) != NULL)
					{
						*fFoundStart = pAnimSet->fAnimSetStart;
						*fFoundFinish = pAnimSet->fAnimSetFinish;
						if (pAnimSet == pObject->pAnimationSet) *fFoundFinish = pAnimSet->ulLength;
						iFoundBest = iAnimSetCount;
						break;
					}
				}
				if (iSearchPatterns == 4)
				{
					// animset name contains part of the general meaning of the name supplied by the script, case insensitive
					// and see if we can match again 'lowercase_meaning_s'
					if (lowercase_meaning_s.Len() > 0)
					{
						char pAnimSetNameLower[MAX_PATH];
						strcpy (pAnimSetNameLower, pAnimSet->szName);
						strlwr(pAnimSetNameLower);
						if (strstr(pAnimSetNameLower, lowercase_meaning_s.Get()) != NULL)
						{
							*fFoundStart = pAnimSet->fAnimSetStart;
							*fFoundFinish = pAnimSet->fAnimSetFinish;
							if (pAnimSet == pObject->pAnimationSet) *fFoundFinish = pAnimSet->ulLength;
							iFoundBest = iAnimSetCount;
							break;
						}
					}
				}
				pAnimSet = pAnimSet->pNext;
				iAnimSetCount++;
			}
		}
	}
	return iFoundBest;
}

cstr entity_lua_getanimationbyindexviaobject(sObject* pObject, int animsetindextofind)
{
	if (pObject)
	{
		sAnimationSet* pAnimSet = pObject->pAnimationSet;
		if (pAnimSet)
		{
			if (stricmp(pAnimSet->szName, "all") == NULL)
			{
				pAnimSet = pAnimSet->pNext; // skip first one (base zero all anims)
			}
		}
		int iAnimSetCount = 1;
		while (pAnimSet)
		{
			if (stricmp(pAnimSet->szName, "mouthshapes") != NULL)
			{
				if (iAnimSetCount == animsetindextofind)
				{
					return cstr(pAnimSet->szName);
				}
				iAnimSetCount++;
			}
			pAnimSet = pAnimSet->pNext;
		}
	}
	return "";
}

cstr entity_lua_getanimationbyindex(int e, int animsetindextofind)
{
	sObject* pObject = GetObjectData(t.entityelement[e].obj);
	return entity_lua_getanimationbyindexviaobject(pObject, animsetindextofind);
}

int entity_lua_getanimationname (int e, cstr FindThisName_s, float* fFoundStart, float* fFoundFinish)
{
	// change animation to name if passed in numeric
	cstr pUseAnimName = FindThisName_s;
	LPSTR pAnimationName = (LPSTR)pUseAnimName.Get();
	if (strlen(pAnimationName) > 1)
	{
		if (pAnimationName[0] == '=')
		{
			// passed in name as numeric, i.e '=2' which means find animation at slot 2
			int iAnimSetIndexToFind = atoi(pAnimationName + 1);
			extern cstr entity_lua_getanimationbyindex(int, int);
			pUseAnimName = entity_lua_getanimationbyindex (e, iAnimSetIndexToFind);
		}
	}

	// and get the data
	sObject* pObject = GetObjectData(t.entityelement[e].obj);
	return entity_lua_getanimationnamefromobject (pObject, pUseAnimName, fFoundStart, fFoundFinish);
}

void entity_lua_setanimationname ( void )
{
	// find the start and finish frames from object
	float fFoundStart = -1, fFoundFinish = -1;
	entity_lua_getanimationname(t.e, t.s_s, &fFoundStart, &fFoundFinish);

	// set animation ready for object
	t.luaglobal.setanim=-1;
	t.luaglobal.setanimstart=fFoundStart;
	t.luaglobal.setanimfinish=fFoundFinish;
	if ( t.game.runasmultiplayer == 1 && t.tLuaDontSendLua == 0 ) 
	{
		mp_sendlua (  MP_LUA_SetAnimationFrames, fFoundStart, fFoundFinish );
	}
}

void entity_loop_using_negative_playanimineditor(int e, int obj, cstr animname)
{
	float fStartFrame = 0;
	float fFinishFrame = 0;
	sObject* pObject = GetObjectData(obj);
	if (strlen(animname.Get()) > 0 && entity_lua_getanimationnamefromobject (pObject, animname, &fStartFrame, &fFinishFrame) > 0)
	{
		LoopObject(obj, fStartFrame, fFinishFrame);
	}
	else
	{
		int entid = t.entityelement[e].bankindex;
		if (t.entityprofile[entid].ischaracter == 1)
		{
			// makes no sense to animate ALL the anims in a character, looks like a bug (IDLE not guarenteed to be at slot 1)
			LoopObject(obj, 0, 0);
		}
		else
		{
			animname = entity_lua_getanimationbyindexviaobject (pObject, 1);
			if (entity_lua_getanimationnamefromobject (pObject, animname, &fStartFrame, &fFinishFrame) > 0)
			{
				LoopObject(obj, fStartFrame, fFinishFrame);
			}
		}
	}
}

void entity_lua_setanimationframes ( void )
{
	t.luaglobal.setanim=-1;
	t.luaglobal.setanimstart=t.e;
	t.luaglobal.setanimfinish=t.v;
	if (  t.game.runasmultiplayer  ==  1 && t.tLuaDontSendLua  ==  0 ) 
	{
		mp_sendlua (  MP_LUA_SetAnimationFrames,t.e,t.v );
	}
}

void entity_lua_playorloopanimation ( float fStartFromPercentage )
{
	// ensure entity is cloned to allow animation
	t.tte = t.e; entity_converttoclone ( );

	// then do animation
	t.obj = t.entityelement[t.e].obj;
	if ( t.obj>0 ) 
	{
		if ( ObjectExist(t.obj) == 1 ) 
		{
			t.entid = t.entityelement[t.e].bankindex ; t.q=t.luaglobal.setanim;
			if ( t.q == -1 ) 
			{
				// play specific frames
				t.ttstart=t.luaglobal.setanimstart ; t.ttfinish=t.luaglobal.setanimfinish;
			}
			else
			{
				// play from FPE animsets
				t.ttstart=t.entityanim[t.entid][t.q].start ; t.ttfinish=t.entityanim[t.entid][t.q].finish;
			}

			// trigger transition to desired animation
			t.playflag=1-t.luaglobal.loopmode ; t.smoothanim[t.obj].st=-1;
			float fCurrentlyNotUsed = 5.0f;
			smoothanimtriggerrev(t.obj, t.ttstart, t.ttfinish, fCurrentlyNotUsed, 0, t.playflag, fStartFromPercentage);

			// ensure spine tracker is reset when start new animation
			sObject* pObject = GetObjectData(t.obj);
			pObject->bSpineTrackerMoving = false;

			// set as animating
			t.entityelement[t.e].lua.animating=1;
			LuaSetFunction ("UpdateEntityAnimatingFlag", 2, 0);
			LuaPushInt (t.e);
			LuaPushInt (t.entityelement[t.e].lua.animating);
			LuaCall ();
		}
	}
	if ( t.game.runasmultiplayer == 1 ) 
	{
		if ( t.luaglobal.setanim == -1 ) t.luaglobal.setanim = 0;
	}
}

void entity_lua_playanimation ( void )
{
	t.luaglobal.loopmode=0;
	entity_lua_playorloopanimation ( 0 );
	if (  t.game.runasmultiplayer  ==  1 && t.tLuaDontSendLua  ==  0 ) 
	{
		mp_sendlua (  MP_LUA_PlayAnimation,t.e,t.v );
	}
}

void entity_lua_playanimationfrom (void)
{
	t.luaglobal.loopmode = 0;
	entity_lua_playorloopanimation (t.v);
}

void entity_lua_loopanimation ( void )
{
	t.luaglobal.loopmode=1;
	entity_lua_playorloopanimation ( 0 );
}

void entity_lua_loopanimationfrom (void)
{
	t.luaglobal.loopmode = 1;
	entity_lua_playorloopanimation ( t.v );
}

void entity_lua_stopanimation ( void )
{
	t.obj=t.entityelement[t.e].obj;
	if (  t.obj>0 ) 
	{
		if (  ObjectExist(t.obj) == 1 ) 
		{
			StopObject (  t.obj );
		}
	}
}

void entity_lua_movewithanimation ( void )
{
	// is called continually for cycles where the animation should move the character
	entity_lua_findcharanimstate ( );
	if ( t.tcharanimindex != -1 ) 
	{
		int iID = t.entityelement[t.e].obj;
		if ( iID > 0 )
		{
			sObject* pObject = g_ObjectList [ iID ];
			if ( pObject )
			{
				if ( t.v == 1 )
				{
				}
				else
				{
					// keep character still, ignore spine vs base tracking
					if (pObject->bSpineTrackerMoving == true)
					{
						if (pObject->ppFrameList)
						{
							if (pObject->dwSpineCenterLimbIndex > 0)
							{
								sFrame* pFrame = pObject->ppFrameList[pObject->dwSpineCenterLimbIndex];
								if (pFrame)
								{
									WickedCall_SetBip01Position(pObject, pFrame, 0, 0, 0);
								}
							}
						}
						pObject->bSpineTrackerMoving = false;
					}
				}
			}
		}
	}
}

void entity_lua_setanimationframe ( void )
{
	// only for animating entities, which are visible (to prevent ALL animatable objects to clone when reload a saved game position)
	int iID = t.entityelement[t.e].obj;
	if ( iID > 0 )
	{
		sObject* pObject = g_ObjectList [ iID ];
		if ( pObject )
		{
		}
		else
			return;
	}
	else
		return;

	// force a frame in the entity object
	SetObjectFrameEx ( t.entityelement[t.e].obj, t.v_f );
}

void entity_lua_changeanimationframe (void)
{
	// only for animating entities, which are visible (to prevent ALL animatable objects to clone when reload a saved game position)
	int iID = t.entityelement[t.e].obj; if (iID <=0 ) return;
	sObject* pObject = g_ObjectList[iID]; if (!pObject) return;

	// force a frame in the entity object WITHOUT stopping the animation
	ChangeObjectFrame (t.entityelement[t.e].obj, t.v_f);
}

void entity_lua_setanimationspeed ( void )
{
	//  animspeed is modulated by timeelapsed in entity_loopanim
	t.entityelement[t.e].eleprof.animspeed = t.v_f * 100.0f; // 101115 - character scripts all refer to 1.0 as a speed of 100

	// and characters need to know this change immediately
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
		t.charanimstates[t.tcharanimindex].animationspeed_f=(65.0/100.0)*t.entityelement[t.e].eleprof.animspeed;
}

// g_bForceRagdoll when calling 'entity_applydamage'
bool g_bForceRagdoll = false;
bool g_bForceNoRagdollJustDestroy = false;
// SetRagdollAllDeaths: every death of a ragdoll character as if by SetEntityHealth(e,-12345), until the level ends
bool g_bRagdollAllDeaths = false;

void entity_lua_setentityhealth_core ( int iSilentOrDamage )
{
	// iSilentOrDamage  0 : none, 1 : silent, 2 : damage
	if (t.v == -12345 || t.v == -12346)
	{
		// set health to zero
		if (t.v == -12345) g_bForceRagdoll = true; // but also force ragdoll over preferred death anim
		if (t.v == -12346) g_bForceNoRagdollJustDestroy = true; // but also DO NOT force ragdoll or any preferred death anim!
		t.v = 0;
	}
	
	// cannot set any health/damage if currently in ragdoll phase
	if (t.entityelement[t.e].ragdollplusactivate != 0)
	{
		return;
	}
	// if new health is zero, apply damage to entity directly
	if ( t.v <= 0 && iSilentOrDamage == 0 )
	{
		//  set an entities health
		if ( t.entityelement[t.e].briefimmunity == 0 )
		{
			t.ttte = t.e;
			t.tdamage = t.entityelement[t.e].health;
			//PE: https://github.com/TheGameCreators/GameGuruRepo/commit/1863aaa879b05925670badf777a43825b1069702
			//PE: 150619 - this also produce this: https://github.com/TheGameCreators/GameGuruRepo/issues/466
			//PE: Changes so SetEntityHealth(e,-1) is needed to produce headshot effect.
			if (t.v == -1) 
			{
				t.tdamageforce = 700.0f; // 210918 - so headshots have better ragdoll reaction
				t.brayy1_f=t.entityelement[t.e].y-500.0;
				t.v = 0;
			}
			else 
			{
				t.tdamageforce = 0.0f; //PE: 150619
				t.brayy1_f = t.entityelement[t.e].y - 20.0; //PE:
			}
			t.tdamagesource = 0;
			t.brayx1_f=t.entityelement[t.e].x;
			t.brayx2_f=t.entityelement[t.e].x;
			t.brayy2_f=t.entityelement[t.e].y;
			t.brayz1_f=t.entityelement[t.e].z;
			t.brayz2_f=t.entityelement[t.e].z;
			t.tallowanykindofdamage=1;
			entity_applydamage ( );
			t.tallowanykindofdamage=0;
		}
	}
	else
	{
		if (t.v == -99999)
		{
			t.entityelement[t.e].briefimmunity = -1;
		}
		else
		{
			if (iSilentOrDamage == 2)
			{
				t.ttte = t.e;
				// check if immunity effectively blocked this damage
				if (t.entityelement[t.e].health > 99000)
				{
					// the entity t.e blocked this damage, allow them to
					// counter attack having deflected the potential damage avoided
				}
				else
				{
					t.tdamage = t.entityelement[t.e].health - t.v;
					t.tdamageforce = 0.0f;
					t.tdamagesource = 0;
					t.brayx1_f = t.entityelement[t.e].x;
					t.brayx2_f = t.entityelement[t.e].x;
					t.brayy1_f = t.entityelement[t.e].y;
					t.brayy2_f = t.entityelement[t.e].y;
					t.brayz1_f = t.entityelement[t.e].z;
					t.brayz2_f = t.entityelement[t.e].z;
					t.tallowanykindofdamage = 1;
					entity_applydamage ();
					entity_applydecalfordamage(t.e, -1, -1, -1);
					t.tallowanykindofdamage = 0;
				}
			}
			else
			{
				if (iSilentOrDamage == 1 && t.v == -1)
				{
					// allow non-zero damage again (reverse of Prevent Zero Health)
					t.entityelement[t.e].briefimmunity = 0;
				}
				else
				{
					if (iSilentOrDamage == 1 && t.v == 0) t.entityelement[t.e].briefimmunity = 0;
					t.entityelement[t.e].health = t.v;
				}
			}
		}
	}
	// and restore before leave
	g_bForceRagdoll = false;
	g_bForceNoRagdollJustDestroy = false;
}
void entity_lua_setentityhealth ( )
{
	entity_lua_setentityhealth_core ( 0 );
}
void entity_lua_setentityhealthsilent ( )
{
	entity_lua_setentityhealth_core ( 1 );
}
void entity_lua_setentityhealthwithdamage ()
{
	entity_lua_setentityhealth_core (2);
}

void entity_lua_setforcex ( void )
{
	t.brayx2_f = t.v; t.brayx1_f = 0.0f;
}
void entity_lua_setforcey ( void )
{
	t.brayy2_f = t.v; t.brayy1_f = 0.0f;
}
void entity_lua_setforcez ( void )
{
	t.brayz2_f = t.v; t.brayz1_f = 0.0f;
}
void entity_lua_setforcelimb ( void )
{
	t.entityelement[t.e].ragdollifiedforcelimb = t.v;
}
void entity_lua_ragdollforce ( void )
{
	//  set an entities ragdoll force value (t.e to t.v)
	t.entityelement[t.e].ragdollifiedforcex_f = (t.brayx2_f-t.brayx1_f)*0.8;
	t.entityelement[t.e].ragdollifiedforcey_f = (t.brayy2_f-t.brayy1_f)*1.2;
	t.entityelement[t.e].ragdollifiedforcez_f = (t.brayz2_f-t.brayz1_f)*0.8;
	t.entityelement[t.e].ragdollifiedforcevalue_f = t.v * 8000.0;
}

void entity_lua_charactercontrolmanual ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		//  check we are not already in that state before sending out lua, so we dont flood
		if (  t.charanimstates[t.tcharanimindex].playcsi != g.csi_limbo ) 
			if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 && g.mp.endplay  ==  0 ) 
				mp_sendlua (  MP_LUA_CharacterControlManual,t.e,t.v );

		//  disable all character control influence 
		t.charanimstate.playcsi=g.csi_limbo;
		t.charanimstate.limbomanualmode = 1; // (AND ENTER NEW AI FULL MANUAL MODE)
		t.charanimstates[t.tcharanimindex]=t.charanimstate;
	}
	if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 ) 
		mp_sendlua (  MP_LUA_CharacterControlManual,t.e,t.v );
}

void entity_lua_charactercontrollimbo ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		//  check we are not already in that state before sending out lua, so we dont flood
		if (  t.charanimstates[t.tcharanimindex].playcsi  !=  g.csi_limbo ) 
		{
			if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 && g.mp.endplay  ==  0 ) 
			{
				mp_sendlua (  MP_LUA_CharacterControlLimbo,t.e,t.v );
			}
		}
		//  disable all character control influence
		t.charanimstate.playcsi=g.csi_limbo;
		t.charanimstate.limbomanualmode = 0; // REGULAR LIMBO MODE WITH SOME CONTROL OVER AI OBJECT
		t.charanimstates[t.tcharanimindex]=t.charanimstate;
	}
	if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 ) 
	{
		mp_sendlua (  MP_LUA_CharacterControlLimbo,t.e,t.v );
	}
}

void entity_lua_charactercontrolunarmed ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		//  check we are not already in that state before sending out lua, so we dont flood
		if ( t.charanimstates[t.tcharanimindex].playcsi != g.csi_unarmed ) 
		{
			if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 && g.mp.endplay  ==  0 ) 
			{
				mp_sendlua (  MP_LUA_CharacterControlUnarmed,t.e,t.v );
			}
		}
		//  restful no weapon out
		if ( t.charanimstates[t.tcharanimindex].playcsi == g.csi_limbo  )  t.charanimstates[t.tcharanimindex].playcsi = g.csi_unarmed;
		t.charanimcontrols[t.tcharanimindex].alerted=0;
	}
}

void entity_lua_charactercontrolarmed ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 && t.csi_stood.size() > 0)
	{
		// 200316 - replaced UNARMED state with STOOD (as this is the armed CSI states!)
		// check we are not already in that state before sending out lua, so we dont flood
		if ( t.charanimstates[t.tcharanimindex].playcsi != t.csi_stood[t.charanimstates[t.tcharanimindex].weapstyle] ) 
		{
			if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 && g.mp.endplay  ==  0 ) 
			{
				mp_sendlua (  MP_LUA_CharacterControlArmed,t.e,t.v );
			}
		}
		// get weapon out (oh er)
		if (  t.charanimstates[t.tcharanimindex].playcsi == g.csi_limbo ) 
		{
			t.charanimstates[t.tcharanimindex].playcsi = t.csi_stood[t.charanimstates[t.tcharanimindex].weapstyle];
			if ( t.charanimstates[t.tcharanimindex].playcsi == 0 )
			{
				// 110416 - no weapon on this character, defer to unarmed state (legacy behaviour)
				t.charanimstates[t.tcharanimindex].playcsi = g.csi_unarmed;
			}
		}
		t.charanimcontrols[t.tcharanimindex].alerted=1;
	}
}

void entity_lua_charactercontrolfidget ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		//  cause character to fidget - do we use charactercontrol array for everything?!?
		//  want to avoid 'unnecessary' state machines hard-coded into engine
		if (  t.charanimstates[t.tcharanimindex].playcsi == g.csi_limbo  )  t.charanimstates[t.tcharanimindex].playcsi = g.csi_unarmed;
	}
}

void entity_lua_charactercontrolducked ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		//  ducked mode
		if (  t.charanimstates[t.tcharanimindex].playcsi == g.csi_limbo  )  t.charanimstates[t.tcharanimindex].playcsi = g.csi_unarmed;
		t.charanimcontrols[t.tcharanimindex].ducking = 1;
	}
return;

}

void entity_lua_charactercontrolstand ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		//  stand up mode
		if (  t.charanimstates[t.tcharanimindex].playcsi == g.csi_limbo ) 
		{
			t.charanimstates[t.tcharanimindex].playcsi=g.csi_unarmed;
		}
		t.charanimcontrols[t.tcharanimindex].ducking = 2;
	}
}

void entity_lua_setcharactertowalkrun ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		t.charanimstate.runmode=t.v;
		t.charanimstate.strafemode=0;
		t.charanimstates[t.tcharanimindex]=t.charanimstate;
	}
	if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 ) 
	{
		mp_sendlua (  MP_LUA_setcharactertowalkrun,t.e,t.v );
	}
}

void entity_lua_setlockcharacter ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		t.charanimstate.runmode=0;
		t.charanimstate.strafemode=0;
		t.charanimstate.freezeallmovement=t.v;
		t.charanimstates[t.tcharanimindex]=t.charanimstate;
	}
}

void entity_lua_setcharactertostrafe ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		t.charanimstate.runmode=0;
		t.charanimstate.strafemode=1+t.v;
		t.charanimstates[t.tcharanimindex]=t.charanimstate;
	}
return;

}

void entity_lua_setcharactervisiondelay ( void )
{
	entity_lua_findcharanimstate ( );
	if (  t.tcharanimindex != -1 ) 
	{
		t.charanimstate.visiondelay=t.v;
		t.charanimstates[t.tcharanimindex]=t.charanimstate;
	}
}

void entity_lua_lookatplayer ( void )
{
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		// for smoothness, move smooth track so always called until reach final position (for head tracking)
		t.charanimstates[t.tcharanimindex].entityTarget = 0;
		t.charanimstates[t.tcharanimindex].neckAiming = t.v_f;
	}
}

int g_presetTargetE = 0;
float g_presetTargetYOffset_f = 0.0f;

void entity_lua_lookattargetyoffset (void)
{
	g_presetTargetYOffset_f = t.v_f;
}

void entity_lua_lookattargete (void)
{
	g_presetTargetE = (int)t.v_f;
	if (g_presetTargetE < 0) g_presetTargetE = 0;
}

void entity_lua_lookattarget (void)
{
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		t.charanimstates[t.tcharanimindex].entityTarget = g_presetTargetE;
		t.charanimstates[t.tcharanimindex].entityTargetYOffset_f = g_presetTargetYOffset_f;
		t.charanimstates[t.tcharanimindex].neckAiming = t.v_f;
	}
}

void entity_lua_aimsmoothmode (void)
{
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		// for smoothness, move smooth track so always called until reach final position (for spine)
		t.charanimstates[t.tcharanimindex].entityTarget = g_presetTargetE;
		t.charanimstates[t.tcharanimindex].entityTargetYOffset_f = g_presetTargetYOffset_f;
		t.charanimstates[t.tcharanimindex].spineAiming = t.v_f;
	}
}

void entity_lua_lookforward ( void )
{
	entity_lua_findcharanimstate();
	if ( t.tcharanimindex != -1 ) 
	{
		t.charanimstates[t.tcharanimindex].neckAiming = 0.0f;
	}
}
void entity_lua_lookatangle (void)
{
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		t.charanimstates[t.tcharanimindex].neckAiming = (t.v_f - 10000); // special code to store forced angle
	}
}

void entity_lua_rotatetoanglecore ( float fDestAngle, float fAngleOffset )
{
	entity_lua_findcharanimstate ( );
	if ( t.v == 100 ) 
	{
		t.tsmooth_f=1.0;
	}
	else
	{
		t.tsmooth_f=(100.0/(t.v+0.0))/g.timeelapsed_f;
	}
	if ( t.tcharanimindex == -1 || t.entityelement[t.e].eleprof.disableascharacter == 1 )
	{
		//  regular entity
		t.tnewangley_f=CurveAngle(fDestAngle,t.entityelement[t.e].ry,t.tsmooth_f);
		t.entityelement[t.e].ry=t.tnewangley_f;
		entity_lua_rotateupdate ( );
	}
	else
	{
		// MAX has no AI subsystem - but for characters we will set the destination angle and smoothly rotate within char_loop or similar
		t.charanimstate.currentangle_f = fDestAngle;
		float fModulateRotSpeed = t.charanimstate.iRotationAlongPathMode / 100.0f;
		t.charanimstate.currentangleslowlyspeed_f = (t.v * fModulateRotSpeed);
		t.charanimstate.moveToMode = 0; // face target will override goto target
		t.charanimstates[t.tcharanimindex] = t.charanimstate;
	}
	if (  t.game.runasmultiplayer  ==  1 && g.mp.coop  ==  1 && t.tLuaDontSendLua  ==  0 ) 
	{
		if (  MAXTimer() - t.entityelement[t.e].mp_rotateTimer > 1000 ) 
		{
			mp_sendlua (  MP_LUA_RotateToPlayer,t.e,t.tnewangley_f );
			t.entityelement[t.e].mp_rotateTimer = MAXTimer();
		}
	}
}

void entity_lua_rotatetocore(float fAngleOffset)
{
	t.tdx_f=t.tcamerapositionx_f-t.entityelement[t.e].x;
	t.tdz_f=t.tcamerapositionz_f-t.entityelement[t.e].z;
	float fDestAngle = atan2deg(t.tdx_f,t.tdz_f);
	entity_lua_rotatetoanglecore ( fDestAngle, fAngleOffset );
}

void entity_lua_rotatetoplayer ( void )
{
	//  third person moves camera to player position
	t.tcamerapositionx_f=CameraPositionX(t.terrain.gameplaycamera);
	t.tcamerapositiony_f=CameraPositionY(t.terrain.gameplaycamera);
	t.tcamerapositionz_f=CameraPositionZ(t.terrain.gameplaycamera);
	entity_lua_rotatetocore ( 0.0f );
}

void entity_lua_rotatetocamera ( void )
{
	//  for when we want the REAL camera (for decals)
	entity_gettruecamera ( );
	entity_lua_rotatetocore ( 0.0f );
}

void entity_lua_rotatetoplayerwithoffset ( void )
{
	t.tcamerapositionx_f=CameraPositionX(t.terrain.gameplaycamera);
	t.tcamerapositiony_f=CameraPositionY(t.terrain.gameplaycamera);
	t.tcamerapositionz_f=CameraPositionZ(t.terrain.gameplaycamera);
	entity_lua_rotatetocore ( t.v_f );
}

void entity_lua_set_gravity ( void )
{
	t.entityelement[t.e].nogravity=t.v;
	ODESetNoGravity(t.entityelement[t.e].obj, 1 - t.entityelement[t.e].nogravity);
}

void entity_lua_fireweapon ( bool instant )
{
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		// at this point, weapons already animating a fire so move this to canfire code
		darkai_shoottarget(t.charanimstates[t.tcharanimindex].entityTarget);
		t.charanimstates[t.tcharanimindex] = t.charanimstate;
	}
}

void entity_lua_hurtplayer ( void )
{
	t.tdamage=t.v ; t.te=t.e ; t.tDrownDamageFlag=0;
	physics_player_takedamage ( );
}

void entity_lua_drownplayer ( void )
{
	t.tdamage=t.v ; t.te=t.e ; t.tDrownDamageFlag=1;
	physics_player_takedamage ( );
	t.tDrownDamageFlag=0;
}

void entity_lua_switchscript ( void )
{
	bool bAllowScriptToSwitch = true;
	if (strlen(t.s_s.Get()) > 0)
	{
		// only record original behavior
		if (t.entityelement[t.e].lua.returningaimainstored == 0)
		{
			t.entityelement[t.e].lua.returningaimain_s = t.entityelement[t.e].eleprof.aimain_s;
			t.entityelement[t.e].lua.returningaimainstored = 1;
		}
	}
	else
	{
		// when want to return, pass in ""
		if (t.entityelement[t.e].lua.returningaimainstored == 1)
		{
			t.s_s = t.entityelement[t.e].lua.returningaimain_s;
			t.entityelement[t.e].lua.returningaimainstored = 0;
		}
		else
		{
			// if call a return, but already at master, do nothing
			bAllowScriptToSwitch = false;
		}
	}
	if (bAllowScriptToSwitch == true)
	{
		t.entityelement[t.e].eleprof.aimain_s = t.s_s;
		lua_loadscriptin();

		// first try initialising with a name string
		t.strwork = ""; t.strwork = t.strwork + t.entityelement[t.e].eleprof.aimainname_s.Get() + "_init_name";
		LuaSetFunction (t.strwork.Get(), 2, 0);
		t.tentityname_s = t.entityelement[t.e].eleprof.name_s;
		LuaPushInt (t.e); LuaPushString (t.tentityname_s.Get());
		LuaCallSilent ();

		// then try passing in location of this script (needed for relative discovery of BYC file!)
		t.strwork = ""; t.strwork = t.strwork + t.entityelement[t.e].eleprof.aimainname_s.Get() + "_init_file";
		LuaSetFunction (t.strwork.Get(), 2, 0);
		char pRemoveLUAEXT[MAX_PATH];
		strcpy(pRemoveLUAEXT, t.entityelement[t.e].eleprof.aimain_s.Get());
		if (strlen(pRemoveLUAEXT) > 4)
		{
			pRemoveLUAEXT[strlen(pRemoveLUAEXT) - 4] = 0;
		}
		LuaPushInt (t.e);
		LuaPushString(pRemoveLUAEXT);
		LuaCallSilent ();

		// then the actual _init call
		t.strwork = cstr(cstr(Lower(t.entityelement[t.e].eleprof.aimainname_s.Get())) + "_init");
		LuaSetFunction (t.strwork.Get(), 1, 0);
		LuaPushInt (t.e); LuaCallSilent ();

		//Check if we use properties variables.
		char tmp[MAX_PATH];
		strcpy(tmp, t.entityelement[t.e].eleprof.aimainname_s.Get());
		char* pFindSlash = strrchr(tmp, '\\');
		if (pFindSlash) strcpy(tmp, pFindSlash + 1);
		strcat(tmp, "_properties(");
		if (pestrcasestr(t.entityelement[t.e].eleprof.soundset4_s.Get(), tmp))
		{
			//Found one , parse and sent variables to script.
			lua_execute_properties_variable(t.entityelement[t.e].eleprof.soundset4_s.Get());
		}
	}
}

int g_iSuggestedSlot = 0;

void entity_lua_addplayerweapon(void)
{
	if (t.e > t.entityelement.size()) return; //PE: Got Crash Here.
	// collect this weapon
	t.tentid = t.entityelement[t.e].bankindex;
	t.weaponindex = t.entityprofile[t.tentid].isweapon;
	if (t.weaponindex != 0)
	{
		t.tqty = t.entityelement[t.e].eleprof.quantity;
		g_iSuggestedSlot = t.v;
		if (physics_player_addweapon() == true)
		{
			t.entityelement[t.e].eleprof.quantity = 0;// and keep the ammo elsewhere :)
		}
		g_iSuggestedSlot = 0;
	}
}

void entity_lua_changeplayerweapon(void)
{
	// what weapon
	t.findgun_s = t.s_s;
	gun_findweaponindexbyname();

	// before force this weapon, ensure if it does NOT exist, to create it first so can have ammo too
	int iHaveThis = 0;
	for (t.ws = 1; t.ws < 12; t.ws++)
	{
		if (t.weaponslot[t.ws].got == t.foundgunid)
		{
			iHaveThis = t.ws;
			break;
		}
	}
	if (iHaveThis == 0)
	{
		t.tqty = g.firemodes[t.foundgunid][0].settings.reloadqty;
		t.weaponindex = t.foundgunid;
		if (physics_player_addweapon() == true)
		{
			// weapon was added, with some ammo
		}
	}

	// force this weapon NAME to be selected
	g.autoloadgun = t.foundgunid; 
	gun_change();
}

void entity_lua_changeplayerweaponid(void)
{
	extern bool bForceGunUnderWater;
	if (bForceGunUnderWater)
		return;
	// if no gun, this will load in gun if it is not already in memory (normally all pre-loaded but standalonelevelreload mode can load a level with missing guns)
	// so this will be called from the GameLoopLoadStats global function for all weapons currently held by player at that point in game when start the fresh level
	int iWeaponID = t.v;

	// force this weapon ID to be selected
	g.autoloadgun = iWeaponID;
	gun_change();
}

void entity_lua_replaceplayerweapon ( void )
{
	//  replace this weapon with one currently held
	//  remove weapon from slot
	t.tswapslot=0;
	if (  t.gunid>0 ) 
	{
		//  find swap slot for old weapon (gunid)
		for ( t.ws = 1 ; t.ws < 12; t.ws++ )
		{
			if (  t.weaponslot[t.ws].pref == t.gunid  )  t.tswapslot = t.ws;
		}
		//  remove old weapon
		t.weaponindex=t.gunid ; physics_player_removeweapon ( );
	}
	//  what is new weapon
	t.tentid=t.entityelement[t.e].bankindex;
	t.weaponindex=t.entityprofile[t.tentid].isweapon;
	//  assign preference for new weapon
	if (  t.tswapslot>0 ) 
	{
		t.weaponslot[t.tswapslot].pref = t.weaponindex;
		if(t.tswapslot < 11)
			t.weaponammo[t.tswapslot] = 0; // reset so new weapon can work out its new ammo
	}
	//  now collect weapon (will find freed up slot from above)
	t.tqty=t.entityelement[t.e].eleprof.quantity;
	physics_player_addweapon ( );
}

void entity_lua_addplayerammo ( void )
{
	//  Collect Ammo for player
	t.tentid=t.entityelement[t.e].bankindex;
	t.tqty=t.entityelement[t.e].eleprof.quantity;
	t.tgunid=t.entityprofile[t.tentid].hasweapon;

	t.tfiremode=0;
	t.tpool=g.firemodes[t.tgunid][t.tfiremode].settings.poolindex;
	if (  t.tpool == 0 ) 
	{
		// the ammo is for a weapon that is missing from files, check to see if the ammo can be used for any other weapons.
		if (t.entityprofile[t.tentid].ammopool_s.Len() > 0)
		{
			for (int i = 0; i < t.ammopool.size(); i++)
			{
				if (stricmp(t.ammopool[i].name_s.Get(), t.entityprofile[t.tentid].ammopool_s.Get()) == 0)
				{
					g.firemodes[t.tgunid][t.tfiremode].settings.poolindex = i;
					t.tpool = i;
					break;
				}
			}
		}
	}
	if(t.tpool != 0)
	{
		// increase ammo pool by ammo quantity
		t.ammopool[t.tpool].ammo = t.ammopool[t.tpool].ammo + t.tqty;
		int iMaxClipCapacity = g.firemodes[t.tgunid][t.tfiremode].settings.clipcapacity * g.firemodes[t.tgunid][t.tfiremode].settings.reloadqty;
		if (iMaxClipCapacity == 0) iMaxClipCapacity = 99999;
		if (t.ammopool[t.tpool].ammo > iMaxClipCapacity) t.ammopool[t.tpool].ammo = iMaxClipCapacity;
	}
}

void entity_lua_addplayerhealth ( void )
{
	// collect health
	t.tqty = t.entityelement[t.e].eleprof.strength;

	//LB: new player health intercept
	LuaSetFunction ("PlayerHealthAdd", 1, 0);
	LuaPushInt(t.tqty);
	LuaCall();
	t.player[t.plrid].health = LuaGetInt("g_PlayerHealth");
}

void entity_lua_setplayerpower ( void )
{
	//  increase power of player (levelup/magic)
	t.player[t.plrid].powers.level=t.v;
return;

}

void entity_lua_addplayerpower ( void )
{
	//  increase power of player (levelup/magic)
	t.player[t.plrid].powers.level=t.player[t.plrid].powers.level+t.v;
return;

}

void entity_lua_addplayerjetpack ( void )
{
	// collect jet pack
	t.playercontrol.jetpackcollected = 1;
	if (  t.playercontrol.jetpackmode == 0  )  t.playercontrol.jetpackmode = 1;
	t.playercontrol.jetpackfuel_f=t.playercontrol.jetpackfuel_f+t.v;
	if (  t.playercontrol.jetobjtouse>0 ) 
	{
		if (  ObjectExist(t.playercontrol.jetobjtouse) == 1  )  HideObject (  t.playercontrol.jetobjtouse );
	}
	t.thudlayeritemindex=t.entityprofile[t.entityelement[t.e].bankindex].ishudlayer;
	t.playercontrol.jetobjtouse=t.hudlayerlist[t.thudlayeritemindex].obj;
	t.playercontrol.jetpackhidden=t.hudlayerlist[t.thudlayeritemindex].hidden;
	if (  t.playercontrol.thirdperson.enabled == 1 ) 
	{
		t.playercontrol.jetpackhidden=1;
	}
	if (  t.playercontrol.jetobjtouse>0 ) 
	{
		if (  ObjectExist(t.playercontrol.jetobjtouse) == 1  )  ShowObject (  t.playercontrol.jetobjtouse );
	}
}

void entity_lua_set_light_visible ( void )
{
	//  receives e and v
	t.entityelement[t.e].eleprof.light.islit=t.v;
	for ( t.l = 1 ; t.l<=  g.infinilightmax; t.l++ )
	{
		if (  t.infinilight[t.l].used == 1 ) 
		{
			if (  t.infinilight[t.l].e == t.e ) 
			{
				t.infinilight[t.l].islit=t.v;
			}
		}
	}
}
