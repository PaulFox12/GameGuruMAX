int LimitSwimmingVerticalMovement(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1)
		return 0;

	int value = lua_tonumber(L, 1);
	ODESwimmingLimitVertical(value);
}


int GetCharacterHitFloor ( lua_State *L )
{
	lua_pushinteger ( L, (lua_Integer)ODEGetCharacterHitFloor() );
	return 1;
}
int GetCharacterFallDistance ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetCharacterFallDistance() );
	return 1;
}
int RayTerrain ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fToX = lua_tonumber(L, 4);
	float fToY = lua_tonumber(L, 5);
	float fToZ = lua_tonumber(L, 6);
	lua_pushinteger ( L, (lua_Integer)ODERayTerrain(fX, fY, fZ, fToX, fToY, fToZ, true) );
	return 1;
}
int GetRayCollisionX ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayCollisionX() );
	return 1;
}
int GetRayCollisionY ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayCollisionY() );
	return 1;
}
int GetRayCollisionZ ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayCollisionZ() );
	return 1;
}

// GGMAX diag: LUA Intersect* call counters (scripts run on the main thread only; read
// via harness GET_PERF_DATA "RAYS:" line — running totals, diff two dumps for rates).
unsigned long long gg_dbg_lua_isect_calls = 0;
unsigned long long gg_dbg_lua_isect_us = 0;
unsigned long long gg_dbg_lua_isect_mode[4] = { 0, 0, 0, 0 };
static unsigned long long gg_dbg_lua_qpc_us(void)
{
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&c);
	return (unsigned long long)((c.QuadPart * 1000000.0) / (double)f.QuadPart);
}

int IntersectCore (lua_State* L, int iMode)
{
	#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
	#endif
	// iMode : 0=dynamic, 1=staticonly, 2-performant, 3-dynamic and use terrain hit to adjust ray to detect objects only
	int n = LUA_GETTOP(L);
	if (iMode == 2)
	{
		if (n < 9) return 0;
	}
	else
	{
		if (n < 7) return 0;
	}
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fNewX = lua_tonumber(L, 4);
	float fNewY = lua_tonumber(L, 5);
	float fNewZ = lua_tonumber(L, 6);
	int iIgnoreObjNo = lua_tonumber(L, 7);

	// use a database to store recent results, and pull from that before redoing a real intersect test
	int iIndexInIntersectDatabase = 0;
	int iLifeInMilliseconds = 0;
	int iIgnorePlayerCapsule = 0;
	int iIgnoreTerrain = 0;
	if (iMode == 0 && n == 7)
	{
		//PE: Now that terrain ray is working all scripts that use IntersectAll now hits terrain. Ignore terrain so it works like before.
		//PE: IntersectAll can now be set to include terrain parameter 11 , for new scripts.
		iIgnoreTerrain = 1;
	}
	if (iMode == 2)
	{
		iIndexInIntersectDatabase = lua_tonumber(L, 8);
		iLifeInMilliseconds = lua_tonumber(L, 9);
		if (n < 10)
			iIgnorePlayerCapsule = 1;
		else
			iIgnorePlayerCapsule = lua_tonumber(L, 10);
		if (n < 11)
			iIgnoreTerrain = 0;
		else
			iIgnoreTerrain = lua_tonumber(L, 11);
	}

	// catch silly tests
	if ((fX == 0 && fY == 0 && fZ == 0 && fNewX == 0 && fNewY == 0 && fNewZ == 0) || (fX == fNewX && fY == fNewY && fZ == fNewZ))
	{
		if (iMode == 2 && iLifeInMilliseconds == -1)
		{
			// resetting the database is not a silly test, it is a signal!
		}
		else
		{
			lua_pushinteger (L, 0);
			return 1;
		}
	}

	// do the expensive ray cast
	unsigned long long ggT0 = gg_dbg_lua_qpc_us();
	gg_dbg_lua_isect_calls++;
	if (iMode >= 0 && iMode <= 3) gg_dbg_lua_isect_mode[iMode]++;
	int tthitvalue = 0;
	if ( iIgnoreTerrain == 0 && iLifeInMilliseconds != -1 && ODERayTerrain(fX, fY, fZ, fNewX, fNewY, fNewZ, true) == 1)
	{
		// dynamic and use terrain hit to adjust ray to detect objects only
		if (iMode == 3)
		{
			fNewX = ODEGetRayCollisionX();
			fNewY = ODEGetRayCollisionY();
			fNewZ = ODEGetRayCollisionZ();
		}
		else
		{
			tthitvalue = -1;
		}
	}
	bool bFullWickedAccuracy = true;
	if (iMode == 2) bFullWickedAccuracy = false;
	if (tthitvalue == 0 )
	{
		tthitvalue = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fNewX, fNewY, fNewZ, iIgnoreObjNo, iMode, iIndexInIntersectDatabase, iLifeInMilliseconds, iIgnorePlayerCapsule, bFullWickedAccuracy);
		if (tthitvalue == -1)
		{
			// the Wicked terrain's chunks answer the scene pick, so -1 comes without a terrain ray here (the old terrain gave
			// it only through ODERayTerrain); scripts read GetRayCollisionX/Y/Z for -1, which held the previous terrain
			// ray's point: a weapon's crosshair ray at bare ground converged on the craft's own downward probe
			extern void ODESetRayCollision ( float fX, float fY, float fZ, float fNX, float fNY, float fNZ );
			ODESetRayCollision ( ChecklistFValueA(6), ChecklistFValueB(6), ChecklistFValueC(6), ChecklistFValueA(7), ChecklistFValueB(7), ChecklistFValueC(7) );
		}
	}
	gg_dbg_lua_isect_us += gg_dbg_lua_qpc_us() - ggT0;
	lua_pushinteger ( L, (lua_Integer)tthitvalue );
	return 1;
}

int IntersectGetLastHitBone(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1)
	{
		lua_pushstring(L, "");
		return 1;
	}
	int iObjectID = lua_tonumber(L,1);
	extern std::unordered_map<int, sFrame*> lastHitFrame;
	if (lastHitFrame.count(iObjectID) > 0)
	{

		if (ObjectExist(iObjectID))
		{
			sObject* pObject = g_ObjectList[iObjectID];
			sFrame* pFrame = lastHitFrame[iObjectID];
			//PE: Is frame still valid ?
			for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
			{
				if (pFrame == pObject->ppFrameList[iFrameIndex])
				{
					if (pFrame->pMesh && pFrame->pMesh->pBones)
					{
						if (strlen(pFrame->pMesh->pBones->szName) > 0)
						{
							//PE: Return name of first bone in list.
							lua_pushstring(L, pFrame->pMesh->pBones->szName);
							return 1;
						}
					}
					break;
				}
			}
		}
	}

	lua_pushstring(L, "");
	return 1;
}

int IntersectGetLastHitFrame(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1)
	{
		lua_pushstring(L, "");
		return 1;
	}
	int iObjectID = lua_tonumber(L, 1);
	extern std::unordered_map<int, sFrame*> lastHitFrame;
	if (lastHitFrame.count(iObjectID) > 0)
	{

		if (ObjectExist(iObjectID))
		{
			sObject* pObject = g_ObjectList[iObjectID];
			sFrame* pFrame = lastHitFrame[iObjectID];
			//PE: Is frame still valid ?
			for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
			{
				if (pFrame == pObject->ppFrameList[iFrameIndex])
				{
					if (pFrame->szName)
					{
						if (strlen(pFrame->szName) > 0)
						{
							//PE: Return bone name.
							lua_pushstring(L, pFrame->szName);
							return 1;
						}
					}
					break;
				}
			}
		}
	}

	lua_pushstring(L, "");
	return 1;
}

int IntersectAll ( lua_State *L )
{
	return IntersectCore ( L, 3 );
}
int IntersectStatic ( lua_State *L )
{
	return IntersectCore ( L, 1 );
}
int IntersectStaticPerformant (lua_State *L)
{
	return IntersectCore (L, 2);
}
int IntersectAllIncludeTerrain (lua_State* L)
{
	return IntersectCore (L, 0);
}
int GetIntersectCollisionX ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueA(6) );
	return 1;
}
int GetIntersectCollisionY ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueB(6) );
	return 1;
}
int GetIntersectCollisionZ ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueC(6) );
	return 1;
}
int GetIntersectCollisionNX ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueA(7) );
	return 1;
}
int GetIntersectCollisionNY ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueB(7) );
	return 1;
}
int GetIntersectCollisionNZ ( lua_State *L )
{
	lua_pushnumber ( L, ChecklistFValueC(7) );
	return 1;
}
int PositionCamera ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	PositionCamera ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int PointCamera ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	PointCamera ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int MoveCamera ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	MoveCamera ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
	return 0;
}
int GetObjectExist ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushinteger ( L, (lua_Integer)ObjectExist(lua_tonumber(L, 1)) );
	return 1;
}

void gun_PlayObject(int iObjID, float fStart, float fEnd);
void gun_StopObject(int iObjID);
void gun_LoopObject(int iObjID, float fStart, float fEnd);
void gun_SetObjectFrame(int iObjID, float fValue);
void gun_SetObjectSpeed(int iObjID, float fValue);

float iGunAnimStart = 0;
float iGunAnimEnd = 0;
float fOldGunSpeed = 0;
int iGunAnimMode = 2; // 0 = Play , 1 = Loop, 2 = stop
extern bool bCustomGunAnimationRunning;
bool bForceGunUnderWater = false;

void GunInitAnimationSettings(void)
{
	iGunAnimStart = 0;
	iGunAnimEnd = 0;
	iGunAnimMode = 2;
	bCustomGunAnimationRunning = false;
	bForceGunUnderWater = false;
}
int ForceGunUnderWater(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	bForceGunUnderWater = lua_tonumber(L, 1);
	return 0;
}

int GetGunEmissiveStrength(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	lua_pushnumber(L, t.gun[t.gunid].settings.fEmissiveStrength);
	return 1;
}
int SetGunEmissiveStrength(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float emi = lua_tonumber(L, 1);
	sObject* pGunObject = GetObjectData(t.currentgunobj);
	if (pGunObject)
		WickedCall_SetObjectEmissiveStrength(pGunObject, emi);
	return 0;
}

int GetGunAnimationFramesFromName(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	char AnimName[512];
	float fFoundStart = 0, fFoundFinish = 0;

	const char* luastring = lua_tostring(L, 1);
	if(luastring)
		strcpy(AnimName, luastring);

	sObject* pObject = GetObjectData(t.currentgunobj);
	extern int entity_lua_getanimationnamefromobject(sObject * pObject, cstr FindThisName_s, float* fFoundStart, float* fFoundFinish);
	if (pObject && luastring && entity_lua_getanimationnamefromobject(pObject, AnimName, &fFoundStart, &fFoundFinish) > 0)
	{
		lua_pushnumber(L, fFoundStart);
		lua_pushnumber(L, fFoundFinish);
	}
	else
	{
		//PE: Try to get it from the current weapon.
		if (stricmp(AnimName, "reload") == NULL)
		{
			if (g.firemodes[t.gunid][0].action.startreload.s > 0 && g.firemodes[t.gunid][0].action.startreload.e >= g.firemodes[t.gunid][0].action.startreload.s)
			{
				lua_pushinteger(L, (lua_Integer)(g.firemodes[t.gunid][0].action.startreload.s));
				lua_pushinteger(L, (lua_Integer)(g.firemodes[t.gunid][0].action.startreload.e));
				return 2;
			}
		}
		if (stricmp(AnimName, "idle") == NULL)
		{
			if (g.firemodes[t.gunid][0].action.idle.s > 0 && g.firemodes[t.gunid][0].action.idle.e >= g.firemodes[t.gunid][0].action.idle.s)
			{
				lua_pushinteger(L, (lua_Integer)(g.firemodes[t.gunid][0].action.idle.s));
				lua_pushinteger(L, (lua_Integer)(g.firemodes[t.gunid][0].action.idle.e));
				return 2;
			}
		}
		if (stricmp(AnimName, "fire") == NULL)
		{
			if (g.firemodes[t.gunid][0].action.start.s > 0 && g.firemodes[t.gunid][0].action.start.e >= g.firemodes[t.gunid][0].action.start.s)
			{
				lua_pushinteger(L, (lua_Integer)(g.firemodes[t.gunid][0].action.start.s));
				lua_pushinteger(L, (lua_Integer)(g.firemodes[t.gunid][0].action.start.e));
				return 2;
			}
		}

		lua_pushinteger(L, 0);
		lua_pushinteger(L, 0);
	}
	return 2;
}
int GunAnimationSetFrame(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float start = lua_tonumber(L, 1);
	gun_SetObjectFrame(t.currentgunobj, start);
	return 0;
}
int GunAnimationPlaying(lua_State* L)
{
	int n = LUA_GETTOP(L);
	//if (n < 1) return 0;
	float frame = GetFrame(t.currentgunobj);
	if (iGunAnimMode == 0)
	{
		//PE: Stop animation when done.

		bool playing = false;
		if (t.currentgunobj > 0)
		{
			sObject* pObject = g_ObjectList[t.currentgunobj];
			playing = WickedCall_GetAnimationPlayingState(pObject);
		}

		if (frame >= iGunAnimEnd || !playing)
		{
			gun_StopObject(t.currentgunobj);
			gun_SetObjectFrame(t.currentgunobj, iGunAnimEnd);
			if (fOldGunSpeed > 1)
				gun_SetObjectSpeed(t.currentgunobj, fOldGunSpeed);
			fOldGunSpeed = 0;
			lua_pushinteger(L, 0);
			bCustomGunAnimationRunning = false;
			t.gunmode = 9; //PE: switch to idle.
			iGunAnimMode = 3;
		}
		else
		{
			lua_pushinteger(L, 1);
			bCustomGunAnimationRunning = true;
		}

	}
	else if(iGunAnimMode == 1)
	{
		//PE: Looping always playing.
		lua_pushinteger(L, 1);
		bCustomGunAnimationRunning = true;
	}
	else
	{
		//PE: Stopped.
		lua_pushinteger(L, 0);
		bCustomGunAnimationRunning = false;
	}
	return 1;

}
int SetGunAnimationSpeed(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float speed = lua_tonumber(L, 1);
	if(fOldGunSpeed == 0)
		fOldGunSpeed = GetSpeed(t.currentgunobj);
	gun_SetObjectSpeed(t.currentgunobj, speed);
	return 0;
}
int PlayGunAnimation(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float start = lua_tonumber(L, 1);
	float end = lua_tonumber(L, 2);
	if (start > end)
	{
		float tmp = start;
		start = end;
		end = tmp;
	}
	gun_PlayObject(t.currentgunobj, start, end);
	bCustomGunAnimationRunning = true;
	iGunAnimStart = start;
	iGunAnimEnd = end;
	iGunAnimMode = 0;
	return 0;
}
int StopGunAnimation(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	gun_StopObject(t.currentgunobj);
	if (fOldGunSpeed > 1)
		gun_SetObjectSpeed(t.currentgunobj, fOldGunSpeed);
	fOldGunSpeed = 0;
	iGunAnimMode = 2;
	bCustomGunAnimationRunning = false;
	t.gunmode = 9; //PE: switch to idle.
	return 0;
}
int LoopGunAnimation(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float start = lua_tonumber(L, 1);
	float end = lua_tonumber(L, 2);
	if (start > end)
	{
		float tmp = start;
		start = end;
		end = tmp;
	}
	gun_LoopObject(t.currentgunobj, start, end);
	bCustomGunAnimationRunning = true;
	iGunAnimStart = start;
	iGunAnimEnd = end;
	iGunAnimMode = 1;
	return 0;
}



int SetObjectFrame (lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	SetObjectFrame (lua_tonumber(L, 1), lua_tonumber(L, 2));
	return 0;
}
int GetObjectFrame ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, GetFrame(lua_tonumber(L, 1)) );
	return 1;
}
int SetObjectSpeed ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	SetObjectSpeed ( lua_tonumber(L, 1), lua_tonumber(L, 2) );
	return 0;
}
int GetObjectSpeed ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, GetSpeed(lua_tonumber(L, 1)) );
	return 1;
}
int PositionObject ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	PositionObject ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int ScaleObjectXYZ(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	ScaleObject(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	return 0;
}
// Add Fast Quaternion functions
int QuatMultiply(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 8) return 0;

	GGQUATERNION q1( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	GGQUATERNION q2( lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8) );
	
	float A = ( q1.w + q1.x ) * ( q2.w + q2.x );
	float B = ( q1.z - q1.y ) * ( q2.y - q2.z );
	float C = ( q1.w - q1.x ) * ( q2.y + q2.z );
	float D = ( q1.y + q1.z ) * ( q2.w - q2.x );
	float E = ( q1.x + q1.z ) * ( q2.x + q2.y );
	float F = ( q1.x - q1.z ) * ( q2.x - q2.y );
	float G = ( q1.w + q1.y ) * ( q2.w - q2.z );
	float H = ( q1.w - q1.y ) * ( q2.w + q2.z );

	q1.w = B + (-E - F + G + H ) / 2;
	q1.x = A - ( E + F + G + H ) / 2;
	q1.y = C + ( E - F + G - H ) / 2;
	q1.z = D + ( E - F - G + H ) / 2;

	lua_pushnumber( L, q1.x );
	lua_pushnumber( L, q1.y );
	lua_pushnumber( L, q1.z );
	lua_pushnumber( L, q1.w );
	return 4;
}
int QuatToEuler(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;

	GGQUATERNION q( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );

	float sqw = q.w * q.w;
	float sqx = q.x * q.x;
	float sqy = q.y * q.y;
	float sqz = q.z * q.z;

	float h = -2.0 * ( q.x * q.z - q.y * q.w );

	float x, y, z;

	x = -atan2( 2.0 * ( q.y * q.z + q.x * q.w ), ( -sqx - sqy + sqz + sqw ) );
	z = -atan2( 2.0 * ( q.x * q.y + q.z * q.w ), (  sqx - sqy - sqz + sqw ) );

	if ( abs( h ) < 0.99999 )
	{		
		y =  asin( -2.0 * ( q.x * q.z - q.y * q.w ) );
	}
	else
	{
		y = ( MAXPI / 2 ) * h;
	}

	lua_pushnumber( L, x );
	lua_pushnumber( L, y );
	lua_pushnumber( L, z );
	return 3;
}
int EulerToQuat(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float pitch = lua_tonumber( L, 1 );
	float yaw   = lua_tonumber( L, 2 );
	float roll  = lua_tonumber( L, 3 );

	float sr = sin( roll  / 2.0 );
	float sp = sin( pitch / 2.0 );
	float sy = sin( yaw   / 2.0 );
	float cr = cos( roll  / 2.0 );
	float cp = cos( pitch / 2.0 );
	float cy = cos( yaw   / 2.0 );

	float cycp = cy * cp;
	float sysp = sy * sp;
	float sycp = sy * cp;
	float cysp = cy * sp;

	lua_pushnumber( L, ( sr * sycp ) - ( cr * cysp ) );  // q.x
	lua_pushnumber( L, ( sr * cysp ) + ( cr * sycp ) );  // q.y
	lua_pushnumber( L, ( cr * sysp ) - ( sr * cycp ) );  // q.z
	lua_pushnumber( L, ( sr * sysp ) + ( cr * cycp ) );  // q.w
	return 4;
}
int QuatSLERP(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 9) return 0;
	const GGQUATERNION qa(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	const GGQUATERNION qb(lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8));

	GGQUATERNION qOut;

	QuaternionSlerp( &qOut, &qa, &qb, lua_tonumber(L, 9) );

	lua_pushnumber(L, qOut.x);
	lua_pushnumber(L, qOut.y);
	lua_pushnumber(L, qOut.z);
	lua_pushnumber(L, qOut.w);
	return 4;
}
int QuatLERP(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 9) return 0;

	const GGQUATERNION qa(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	const GGQUATERNION qb(lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8));
	float t = lua_tonumber(L, 9);

	float at = 1.0 - t;
	float bt = t;
	if (qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w < 0) 
	{
		bt = -t;
	}

	lua_pushnumber(L, qa.x * at + qb.x * bt );
	lua_pushnumber(L, qa.y * at + qb.y * bt );
	lua_pushnumber(L, qa.z * at + qb.z * bt );
	lua_pushnumber(L, qa.w * at + qb.w * bt );
	return 4;
}

int ScreenCoordsToPercent(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fPercentX = ( fX / (float) g_dwScreenWidth) * 100.0f;
	float fPercentY = (fY / (float)g_dwScreenHeight) * 100.0f;
	lua_pushnumber(L, fPercentX);
	lua_pushnumber(L, fPercentY);
	return 2;

}

int LuaConvert2DTo3D(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	
	float x = (fX * g_dwScreenWidth) / 100.0f;
	float y = (fY * g_dwScreenHeight) / 100.0f;

	//PE: Wicked GetPickRay use dpi so must add it here.
	const float dpiscaling = (float)GetDpiForWindow(g_pGlob->hWnd) / 96.0f;
	x /= dpiscaling;
	y /= dpiscaling;

	float fOutX = 0, fOutY = 0, fOutZ = 0;
	float fDirX = 0, fDirY = 0, fDirZ = 0;
	Convert2Dto3D(x, y, &fOutX, &fOutY, &fOutZ, &fDirX, &fDirY, &fDirZ);

	lua_pushnumber(L, fOutX);
	lua_pushnumber(L, fOutY);
	lua_pushnumber(L, fOutZ);
	lua_pushnumber(L, fDirX);
	lua_pushnumber(L, fDirY);
	lua_pushnumber(L, fDirZ);
	return 6;
}


int LuaConvert3DTo2D(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	ImVec2 Convert3DTo2D(float x, float y, float z);
	ImVec2 v2DPos = Convert3DTo2D(fX, fY, fZ);
	lua_pushnumber(L, v2DPos.x);
	lua_pushnumber(L, v2DPos.y);
	return 2;
}

// end of Fast Quaternion functions
int RotateObject ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	RotateObject ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int GetObjectAngleX ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectAngleX(lua_tonumber(L, 1)) );
	return 1;
}
int GetObjectAngleY ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectAngleY(lua_tonumber(L, 1)) );
	return 1;
}
int GetObjectAngleZ ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	lua_pushnumber ( L, ObjectAngleZ(lua_tonumber(L, 1)) );
	return 1;
}
int GetObjectPosAng( lua_State *L )
{
	int n = lua_gettop( L );
	if (n < 1) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance(iID) )
	{
		// seems can be called in LUA when object not exist, so return zeros
		lua_pushinteger ( L, 0 );
		lua_pushinteger ( L, 0 );
		lua_pushinteger ( L, 0 );
		lua_pushinteger ( L, 0 );
		lua_pushinteger ( L, 0 );
		lua_pushinteger ( L, 0 );
		return 6;
	}
	else
	{
		// object information
		sObject* pObject = g_ObjectList[iID];
		lua_pushnumber ( L, pObject->position.vecPosition.x );
		lua_pushnumber ( L, pObject->position.vecPosition.y );
		lua_pushnumber ( L, pObject->position.vecPosition.z );
		lua_pushnumber ( L, pObject->position.vecRotate.x );
		lua_pushnumber ( L, pObject->position.vecRotate.y );
		lua_pushnumber ( L, pObject->position.vecRotate.z );
	}
	return 6;
}
int GetObjectColBox( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber( L, 1 );
	if (!ConfirmObjectInstance( iID ) )
		return 0;
	// object information
	sObject* pObject = g_ObjectList[iID];

	lua_pushnumber( L, pObject->collision.vecMin.x );
	lua_pushnumber( L, pObject->collision.vecMin.y );
	lua_pushnumber( L, pObject->collision.vecMin.z );
	lua_pushnumber( L, pObject->collision.vecMax.x );
	lua_pushnumber( L, pObject->collision.vecMax.y );
	lua_pushnumber( L, pObject->collision.vecMax.z );
	return 6;
}
int GetObjectCentre( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	// object information
	sObject* pObject = g_ObjectList[iID];

	lua_pushnumber(L, pObject->collision.vecCentre.x);
	lua_pushnumber(L, pObject->collision.vecCentre.y);
	lua_pushnumber(L, pObject->collision.vecCentre.z);
	return 3;
}
int GetObjectColCentre(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID)) return 0;
	sObject* pObject = g_ObjectList[iID];
	UpdateColCenter(pObject);
	lua_pushnumber(L, pObject->collision.vecColCenter.x);
	lua_pushnumber(L, pObject->collision.vecColCenter.y);
	lua_pushnumber(L, pObject->collision.vecColCenter.z);
	return 3;
}
int GetObjectScales( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	// object information
	sObject* pObject = g_ObjectList[iID];

	lua_pushnumber(L, pObject->position.vecScale.x);
	lua_pushnumber(L, pObject->position.vecScale.y);
	lua_pushnumber(L, pObject->position.vecScale.z);
	return 3;
}
int PushObject( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	if (n == 7)
	{
		ODEAddBodyForce( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
						 	  lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7));
	}
	else if (n == 4)
	{
		ODEAddBodyForce( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), 0, 0, 0 );
	}
	return 0;
}
int ConstrainObjMotion( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0; 
	ODEConstrainBodyMotion( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int ConstrainObjRotation( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance(iID) )
		return 0;
	ODEConstrainBodyRotation( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int CreateSingleHinge( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 7 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance(iID) )
		return 0;
	int iC = ODECreateHingeSingle( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
		                                lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7) );
	lua_pushinteger( L, (lua_Integer)iC );
	return 1;
}
int CreateDoubleHinge( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 11 ) return 0;
	int iIDa = lua_tonumber(L, 1);
	int iIDb = lua_tonumber(L, 2);
	if (!ConfirmObjectInstance(iIDa) || !ConfirmObjectInstance(iIDb))
		return 0;

	int iC = ODECreateHingeDouble( iIDa, iIDb, lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5),
		                                       lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8),
		                                       lua_tonumber(L, 9), lua_tonumber(L, 10), lua_tonumber(L, 11) );
	lua_pushinteger( L, (lua_Integer)iC );
	return 1;
}
int CreateSingleJoint( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 4 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;

	int iC = ODECreateJointSingle( iID, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	lua_pushinteger( L, (lua_Integer)iC );
	return 1;
}
int CreateDoubleJoint( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 9 ) return 0;
	int iIDa = lua_tonumber( L, 1 );
	int iIDb = lua_tonumber( L, 2 );
	if ( !ConfirmObjectInstance( iIDa ) || !ConfirmObjectInstance( iIDb ) )
		return 0;

	int iC = ODECreateJointDouble( iIDa, iIDb, lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5),
		                                       lua_tonumber(L, 6), lua_tonumber(L, 7), lua_tonumber(L, 8),
		                                       lua_tonumber(L, 9) );
	lua_pushinteger( L, (lua_Integer)iC );
	return 1;
}
int CreateSliderDouble( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 17 ) return 0;
	int iIDa = lua_tonumber( L, 1 );
	int iIDb = lua_tonumber( L, 2 );
	if ( !ConfirmObjectInstance( iIDa ) || !ConfirmObjectInstance( iIDb ) )
		return 0;

	int iC = ODECreateSliderDouble( iIDa, iIDb, lua_tonumber(L,  3), lua_tonumber(L,  4), lua_tonumber(L,  5),
		                                        lua_tonumber(L,  6), lua_tonumber(L,  7), lua_tonumber(L,  8),
		                                        lua_tonumber(L,  9), lua_tonumber(L, 10), lua_tonumber(L, 11),
		                                        lua_tonumber(L, 12), lua_tonumber(L, 13), lua_tonumber(L, 14),
		                                        lua_tonumber(L, 15), lua_tonumber(L, 16), lua_tonumber(L, 17) == 1 );
	lua_pushinteger( L, (lua_Integer)iC );
	return 1;
}
int SetSliderLimits(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetSliderLimits( iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5) );
	return 0;
}

int RemoveObjectConstraints( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;

	ODERemoveBodyConstraints( iID );
	return 0;
}
int RemoveConstraint( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iC = lua_tonumber( L, 1 );

	ODERemoveConstraint( iC );
	return 0;
}
int SetObjectDamping( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;

	ODESetBodyDamping( iID, lua_tonumber( L, 2 ), lua_tonumber( L, 3 ) );

	return 0;
}
int SetHingeLimits(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int iC = lua_tonumber(L, 1);

	if (n < 4)
	{
		// Set angle min/max
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), 0.9f, 0.3f, 1.0f);
	}
	else if (n < 5)
	{
		// also set softness
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), 0.3f, 1.0f);
	}
	else if (n < 6)
	{
		// also set bias
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
			lua_tonumber(L, 5), 1.0f);
	}
	else
	{
		// also set relaxation
		ODESetHingeLimits(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4),
			lua_tonumber(L, 5), lua_tonumber(L, 6));
	}
	return 0;
}
int SetHingeMotor( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetHingeMotor(iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
	return 0;
}
int SetSliderMotor(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetSliderMotor( iC, lua_tonumber(L, 2) == 1, lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}
int GetHingeAngle(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iC = lua_tonumber(L, 1);

	lua_pushnumber(L, ODEGetHingeAngle(iC));
	return 1;
}
int GetSliderPosition(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iC = lua_tonumber(L, 1);

	lua_pushnumber( L, ODEGetSliderPosition( iC ) );
	return 1;
}
int SetBodyScaling(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int iC = lua_tonumber(L, 1);

	ODESetBodyScaling( iC, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}

int PhysicsRayCast( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 7 ) return 0;
	float fFromX = lua_tonumber( L, 1 );
	float fFromY = lua_tonumber( L, 2 );
	float fFromZ = lua_tonumber( L, 3 );
	float fToX   = lua_tonumber( L, 4 );
	float fToY   = lua_tonumber( L, 5 );
	float fToZ   = lua_tonumber( L, 6 );
	float fForce = lua_tonumber( L, 7 );
	if ( ODERayForce( fFromX, fFromY, fFromZ, fToX, fToY, fToZ, fForce ) == 1 )
	{
		int iObjHit = ODEGetRayObjectHit();
		// only return dynamic objects
		if ( ODEGetBodyIsDynamic( iObjHit ) )
		{
			lua_pushinteger( L, (lua_Integer)iObjHit );
			lua_pushnumber( L, ODEGetRayCollisionX() );
			lua_pushnumber( L, ODEGetRayCollisionY() );
			lua_pushnumber( L, ODEGetRayCollisionZ() );
			return 4;
		}
	}
	lua_pushinteger( L, 0 );
	return 1;
}
int GetObjectNumCollisions(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	lua_pushinteger(L, (lua_Integer)ODEGetBodyNumCollisions(iID));
	return 1;
}
int GetObjectCollisionDetails( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	int colNum = 0;
	int iColObj = 0;
	float fX, fY, fZ, fF;
	if (n == 2) colNum = lua_tonumber(L, 2);
	ODEGetBodyCollisionDetails( iID, colNum, iColObj, fX, fY, fZ, fF );
	lua_pushinteger( L, (lua_Integer)iColObj );
	lua_pushnumber( L, fX );
	lua_pushnumber( L, fY );
	lua_pushnumber( L, fZ );
	lua_pushnumber( L, fF );

	return 5;
}
int GetTerrainNumCollisions( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	lua_pushinteger( L, (lua_Integer)ODEGetTerrainNumCollisions( iID ) );
	return 1;
}
int GetTerrainCollisionDetails( lua_State *L )
{
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	int colNum = 0;
	int iLatest = 0;
	float fX, fY, fZ;
	if (n == 2) colNum = lua_tonumber( L, 2 );
	ODEGetTerrainCollisionDetails( iID, colNum, iLatest, fX, fY, fZ );
	lua_pushinteger( L, (lua_Integer)iLatest );
	lua_pushnumber( L, fX );
	lua_pushnumber( L, fY );
	lua_pushnumber( L, fZ );

	return 4;
}
int AddObjectCollisionCheck( lua_State *L )
{
	int n = lua_gettop( L );
	if (n < 1) return 0;
	int iID = lua_tonumber( L, 1 );
	if ( !ConfirmObjectInstance( iID ) )
		return 0;
	ODEAddBodyCollisionCheck( iID );
	return 0;
}
int RemoveObjectCollisionCheck(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iID = lua_tonumber(L, 1);
	if (!ConfirmObjectInstance(iID))
		return 0;
	ODERemoveBodyCollisionCheck(iID);
	return 0;
}

// Lua control of dynamic light
// get the light number using entity e number 
// then use that in the other light functions
int GetEntityLightNumber( lua_State *L )
{
	lua2 = L;
	int n = lua_gettop( L );
	if ( n < 1 ) return 0;

	// get lightentity e number
	int iID = lua_tonumber( L, 1 );
	if ( iID <= 0 ) return 0;

	for ( int i = 1; i <= g.infinilightmax; i++)
	{
		if ( t.infinilight[ i ].used == 1 && t.infinilight[ i ].e == iID )
		{
			lua_pushinteger( L, i );
			return 1;
		}
	}
	return 0;
}
int GetLightPosition( lua_State *L )
{
	lua2 = L;
	int n = lua_gettop( L );
	if ( n < 1 )
		return 0;

	// get light number
	int i = lua_tointeger( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		lua_pushnumber( L, t.infinilight[i].x );
		lua_pushnumber( L, t.infinilight[i].y );
		lua_pushnumber( L, t.infinilight[i].z );
		return 3;
	}
	return 0;
}

//PE: Needed function to make light rotate, now that we have strange Z int eular angles.
//RotateGlobalAngleY(ax,ay,az,NewAngleY)
int RotateGlobalAngleY(lua_State *L)
{
	lua2 = L;
	// get number of arguments
	int n = LUA_GETTOP(L);
	// Not enough params, return out
	if (n < 4)
		return 0;

	float fAngleX = lua_tonumber(L, 1);
	float fAngleY = lua_tonumber(L, 2);
	float fAngleZ = lua_tonumber(L, 3);
	
	//Matrix
	float fNewAngleY = lua_tonumber(L, 4);

	//PE: Rotate Matrix.

	//New eular.
	lua_pushnumber(L, fAngleX);
	lua_pushnumber(L, fAngleY);
	lua_pushnumber(L, fAngleZ);
	return 3;
}

int GetLightAngle(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1)
		return 0;

	// get light number
	int i = lua_tointeger(L, 1);

	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		float fAngleX = t.infinilight[i].f_angle_x;
		float fAngleY = t.infinilight[i].f_angle_y;
		float fAngleZ = t.infinilight[i].f_angle_z;

		void FixEulerZInverted(float &ax, float &ay, float &az);
		FixEulerZInverted(fAngleX, fAngleY, fAngleZ);

		#ifdef WICKEDENGINE
		fAngleX = (fAngleX / 180.0f) - 1.0f;
		fAngleY = (fAngleY / 180.0f) - 1.0f;
		fAngleZ = (fAngleZ / 180.0f) - 1.0f;
		#endif

		lua_pushnumber(L, fAngleX);
		lua_pushnumber(L, fAngleY);
		lua_pushnumber(L, fAngleZ);
		return 3;
	}
	return 0;
}
int GetLightEuler(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int i = lua_tointeger(L, 1);
	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		float fAngleX = t.infinilight[i].f_angle_x;
		float fAngleY = t.infinilight[i].f_angle_y;
		float fAngleZ = t.infinilight[i].f_angle_z;
		void FixEulerZInverted(float& ax, float& ay, float& az);
		FixEulerZInverted(fAngleX, fAngleY, fAngleZ);
		lua_pushnumber(L, fAngleX);
		lua_pushnumber(L, fAngleY);
		lua_pushnumber(L, fAngleZ);
		return 3;
	}
	return 0;
}
int GetLightRGB( lua_State *L )
{
	lua2 = L;
	int n = lua_gettop( L );
	if ( n < 1 )
		return 0;

	// get light number
	int i = lua_tointeger( L, 1 );

	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		lua_pushinteger( L, (lua_Integer)(t.infinilight[i].colrgb.r) );
		lua_pushinteger( L, (lua_Integer)(t.infinilight[i].colrgb.g) );
		lua_pushinteger( L, (lua_Integer)(t.infinilight[i].colrgb.b) );
		return 3;
	}
	return 0;
}
int GetLightRange(lua_State *L)
{
	lua2 = L;
	int n = lua_gettop( L );
	if ( n < 1 )
		return 0;

	// get light number
	int i = lua_tointeger( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		lua_pushnumber( L, t.infinilight[ i ].range );
		return 1;
	}
	return 0;
}
// uses light number from above
int SetLightPosition( lua_State *L )
{
	lua2 = L;
	// get number of arguments
	int n = lua_gettop( L );
	// Not enough params, return out
	if ( n < 4 )
		return 0;

	// get light number
	int i = lua_tonumber( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		t.infinilight[ i ].x = lua_tonumber( L, 2 );
		t.infinilight[ i ].y = lua_tonumber( L, 3 );
		t.infinilight[ i ].z = lua_tonumber( L, 4 );
	}
	return 0;
}

float QuickEulerWrapAngle(float Angle)
{
	float NewAngle = fmod(Angle, 360.0f);
	if (NewAngle < 0.0f)
		NewAngle += 360.0f;
	return NewAngle;
}

int SetLightAngle(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if (n < 4)
		return 0;

	// get light number
	int i = lua_tonumber(L, 1);

	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		t.infinilight[i].f_angle_x = lua_tonumber(L, 2);
		t.infinilight[i].f_angle_y = lua_tonumber(L, 3);
		t.infinilight[i].f_angle_z = lua_tonumber(L, 4);

		#ifdef WICKEDENGINE
		//PE: Range -1 to 1.0 , convert to 0-360.
		//PE: This makes it rotate like in Classic.
		t.infinilight[i].f_angle_x = (t.infinilight[i].f_angle_x + 1.0) * 180.0;
		t.infinilight[i].f_angle_y = (t.infinilight[i].f_angle_y + 1.0) * 180.0;
		t.infinilight[i].f_angle_z = (t.infinilight[i].f_angle_z + 1.0) * 180.0;
		#endif
	}
	return 0;
}
int SetLightEuler(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	int i = lua_tonumber(L, 1);
	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		t.infinilight[i].f_angle_x = lua_tonumber(L, 2);
		t.infinilight[i].f_angle_y = lua_tonumber(L, 3);
		t.infinilight[i].f_angle_z = lua_tonumber(L, 4);
	}
	return 0;
}
int SetLightRGB( lua_State *L ) 
{
	lua2 = L;
	// get number of arguments
	int n = lua_gettop( L );
	// Not enough params, return out
	if ( n < 4 )
		return 0;

	// get light number
	int i = lua_tonumber( L, 1 );

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[ i ].used == 1 )
	{
		t.infinilight[ i ].colrgb.r = lua_tonumber( L, 2 );
		t.infinilight[ i ].colrgb.g = lua_tonumber( L, 3 );
		t.infinilight[ i ].colrgb.b = lua_tonumber( L, 4 );
	}
	return 0;
}

int SetLightRange( lua_State *L )
{
	lua2 = L;
	// get number of arguments
	int n = LUA_GETTOP(L);
	// Not enough params, return out
	if (n < 2)
		return 0;

	// get light number
	int i = lua_tointeger(L, 1);

	if ( i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1 )
	{
		float rng = lua_tonumber(L, 2);
		if ( rng < 1.0f )
		{
			rng = 1.0f;
		}
		else if ( rng > 10000.0f )
		{
			rng = 10000.0f;
		}
		t.infinilight[ i ].range = rng;
	}
	return 0;
}

int RunCharLoop ( lua_State *L )
{
	// run character animation system
	char_loop ( );
	return 0;
}
int TriggerWaterRipple ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	g.decalx=lua_tonumber(L, 1);
	g.decaly=lua_tonumber(L, 2);
	g.decalz=lua_tonumber(L, 3);
	#ifdef WICKEDENGINE
	// DX12: engine-native water ripple replaces the old additive ripple decal
	// (footprint USER-TUNED 2026-07-28: 20 units, half the initial 40)
	extern void WickedCall_PutWaterRipple(float fX, float fY, float fZ, float fSize);
	WickedCall_PutWaterRipple(g.decalx, g.decaly, g.decalz, 20.0f);
	#else
	decal_triggerwaterripple ( );
	#endif
	return 0;
}


int TriggerWaterRippleSize(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	g.decalx = lua_tonumber(L, 1);
	g.decaly = lua_tonumber(L, 2);
	g.decalz = lua_tonumber(L, 3);
	t.decalscalemodx = lua_tonumber(L, 4);
	t.decalscalemody = lua_tonumber(L, 5);
	#ifdef WICKEDENGINE
	// DX12: engine-native water ripple replaces the old additive ripple decal;
	// the script's x-size argument is reused as the world-unit footprint, halved
	// (USER-TUNED 2026-07-28) since scripts carry DX11-era sizes
	extern void WickedCall_PutWaterRipple(float fX, float fY, float fZ, float fSize);
	WickedCall_PutWaterRipple(g.decalx, g.decaly, g.decalz, (float)t.decalscalemodx * 0.5f);
	#else
	decal_triggerwaterripplesize();
	#endif
	return 0;
}
int TriggerWaterSplash(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	g.decalx = lua_tonumber(L, 1);
	g.decaly = lua_tonumber(L, 2);
	g.decalz = lua_tonumber(L, 3);
	t.tInScale_f = lua_tonumber(L, 4);
	#ifdef WICKEDENGINE
	extern int g_iBlendMode;
	int storage = g_iBlendMode;
	g_iBlendMode = 5; // Additive.
	#endif
	decal_triggerwatersplash();
	#ifdef WICKEDENGINE
	g_iBlendMode = storage;
	#endif
}
int PlayFootfallSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 5 || n > 7 ) return 0;
	int footfalltype = lua_tonumber(L, 1);
	float fX = lua_tonumber(L, 2);
	float fY = lua_tonumber(L, 3);
	float fZ = lua_tonumber(L, 4);
	int lastfootfallsound = lua_tonumber(L, 5);
	bool bProceedToPlaySound = false;
	#ifdef WICKEDENGINE
	int iLeftOrRight = lua_tonumber(L, 6);
	int iWalkOrRun = lua_tonumber(L, 7);
	extern void sound_footfallsound_core(int, int, float, float, float, int, int, int*);
	sound_footfallsound_core(0, footfalltype, fX, fY, fZ, iLeftOrRight, iWalkOrRun, &lastfootfallsound);
	#else
	t.trndsnd = Rnd(3);
	if (t.trndsnd == 0) t.tsnd = t.material[footfalltype].tred0id;
	if (t.trndsnd == 1) t.tsnd = t.material[footfalltype].tred1id;
	if (t.trndsnd == 2) t.tsnd = t.material[footfalltype].tred2id;
	if (t.trndsnd == 3) t.tsnd = t.material[footfalltype].tred3id;
	if (t.tsnd > 0)
	{
		if (t.trndsnd == lastfootfallsound)
		{
			t.trndsnd = t.trndsnd + 1; if (t.trndsnd > 3) t.trndsnd = 0;
			if (t.trndsnd == 0) t.tsnd = t.material[footfalltype].tred0id;
			if (t.trndsnd == 1) t.tsnd = t.material[footfalltype].tred1id;
			if (t.trndsnd == 2) t.tsnd = t.material[footfalltype].tred2id;
			if (t.trndsnd == 3) t.tsnd = t.material[footfalltype].tred3id;
		}
		lastfootfallsound = t.trndsnd;
		// play this material sound (will play tsnd+0 through tsnd+4)
		t.tsoundtrigger = t.tsnd; t.tvol_f = 90;
		t.tspd_f = t.material[footfalltype].freq;
		t.tsx_f = fX;
		t.tsy_f = fY;
		t.tsz_f = fZ;
		material_triggersound (1);
	}
	#endif
	lua_pushinteger ( L, (lua_Integer)lastfootfallsound );
	return 1;
}
int ResetUnderwaterState ( lua_State *L )
{
	physics_player_reset_underwaterstate();
	return 0;
}
int SetUnderwaterOn ( lua_State *L )
{
	visuals_underwater_on();
	return 0;
}
int SetUnderwaterOff ( lua_State *L )
{
	visuals_underwater_off();
	return 0;
}
int SetWorldGravity(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 4)
		return 0;

	float x = lua_tonumber(L, 1);
	float y = lua_tonumber(L, 2);
	float z = lua_tonumber(L, 3);
	float fall = lua_tonumber(L, 4);

	ODESetWorldGravity(x, y, z, fall);
}

// Set Shader Values

int SetShaderVariable ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	int iShaderIndex = lua_tonumber(L, 1);
	char pConstantName[512];
	strcpy ( pConstantName, lua_tostring(L, 2) );
	float fValue1 = lua_tonumber(L, 3);
	float fValue2 = lua_tonumber(L, 4);
	float fValue3 = lua_tonumber(L, 5);
	float fValue4 = lua_tonumber(L, 6);
	int iShaderIndexStart = 1;
	int iShaderIndexFinish = 2000;
	if ( iShaderIndex > 0 ) { iShaderIndexStart = iShaderIndex; iShaderIndexFinish = iShaderIndex; }
	SetVector4 ( g.terrainvectorindex1, fValue1, fValue2, fValue3, fValue4 );
	for ( int iSI = iShaderIndexStart; iSI <= iShaderIndexFinish; iSI++ )
	{
		if ( GetEffectExist ( iSI ) == 1 ) 
		{
			DWORD pConstantPtr = GetEffectParameterIndex ( iSI, pConstantName );
			if ( pConstantPtr ) 
			{
				SetEffectConstantVEx( iSI, pConstantPtr, g.terrainvectorindex1 );
			}
		}
	}
	return 0;
}

//PE: Control cloud shader
void Wicked_Update_Cloud(void* visual);
int GetCloudDensity(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudiness);
	return 1;
}
int SetCloudDensity(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudiness = lua_tonumber(L, 1);
	Wicked_Update_Cloud((void*) &t.gamevisuals);
	return 0;
}
int GetCloudCoverage(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudCoverage);
	return 1;
}
int SetCloudCoverage(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudCoverage = lua_tonumber(L, 1);
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}
int GetCloudHeight(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudHeight);
	return 1;
}
int SetCloudHeight(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudHeight = lua_tonumber(L, 1);
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}
int GetCloudThickness(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudThickness);
	return 1;
}
int SetCloudThickness(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudThickness = lua_tonumber(L, 1);
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}
int GetCloudSpeed(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.SkyCloudSpeed);
	return 1;
}
int SetCloudSpeed(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.SkyCloudSpeed = lua_tonumber(L, 1);
	Wicked_Update_Cloud((void*)&t.gamevisuals);
	return 0;
}

//PE: Other Shaders
int SetTreeWind(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	t.gamevisuals.tree_wind = lua_tonumber(L, 1);
	//void WickedCall_UpdateTreeWind(float wind)
	WickedCall_UpdateTreeWind(t.gamevisuals.tree_wind);
	return 0;
}
int GetTreeWind(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.tree_wind);
	return 1;
}


//Control Water Shader
//setter
int SetWaterHeight(lua_State *L) 
{
	t.terrain.waterliney_f = lua_tonumber(L, 1);
	terrain_updatewaterphysics();
	extern void WickedCall_UpdateWaterHeight(float);
	WickedCall_UpdateWaterHeight(t.terrain.waterliney_f);
	return 0;
}
int SetWaterShaderColor(lua_State *L) 
{
#ifdef WICKEDENGINE
	t.gamevisuals.WaterRed_f = lua_tonumber(L, 1);
	t.gamevisuals.WaterGreen_f = lua_tonumber(L, 2);
	t.gamevisuals.WaterBlue_f = lua_tonumber(L, 3);
	WickedCall_UpdateWaterColor(t.gamevisuals.WaterRed_f, t.gamevisuals.WaterGreen_f, t.gamevisuals.WaterBlue_f);
	return 0;
#else
	t.visuals.WaterRed_f = lua_tonumber(L, 1);
	t.visuals.WaterGreen_f = lua_tonumber(L, 2);
	t.visuals.WaterBlue_f = lua_tonumber(L, 3);
	SetVector4(g.terrainvectorindex, t.visuals.WaterRed_f / 256, t.visuals.WaterGreen_f / 256, t.visuals.WaterBlue_f / 256, 0);
	SetEffectConstantV(t.terrain.effectstartindex + 1, "WaterCol", g.terrainvectorindex);
	return 0;
#endif
}
int SetWaterWaveIntensity(lua_State *L)
{
	t.visuals.WaterWaveIntensity_f = lua_tonumber(L, 1);
	SetVector4(g.terrainvectorindex, t.visuals.WaterWaveIntensity_f, t.visuals.WaterWaveIntensity_f, 0, 0);
	SetEffectConstantV(t.terrain.effectstartindex + 1, "nWaterScale", g.terrainvectorindex);
	return 0;
}
int SetWaterTransparancy(lua_State *L)
{
	t.visuals.WaterTransparancy_f = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "WaterTransparancy", t.visuals.WaterTransparancy_f);
	return 0;
}
int SetWaterReflection(lua_State *L)
{
	t.visuals.WaterReflection_f = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "WaterReflection", t.visuals.WaterReflection_f);
	return 0;
}
int SetWaterReflectionSparkleIntensity(lua_State *L)
{
	t.visuals.WaterReflectionSparkleIntensity = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "reflectionSparkleIntensity", t.visuals.WaterReflectionSparkleIntensity);
	return 0;
}
int SetWaterFlowDirection(lua_State *L)
{
	t.visuals.WaterFlowDirectionX = lua_tonumber(L, 1);
	t.visuals.WaterFlowDirectionY = lua_tonumber(L, 2);
	t.visuals.WaterFlowSpeed = lua_tonumber(L, 3);
	SetVector4(g.terrainvectorindex, t.visuals.WaterFlowDirectionX*t.visuals.WaterFlowSpeed, t.visuals.WaterFlowDirectionY*t.visuals.WaterFlowSpeed, 0, 0);
	SetEffectConstantV(t.terrain.effectstartindex + 1, "flowdirection", g.terrainvectorindex);
	return 0;
}
int SetWaterDistortionWaves(lua_State *L)
{
	t.visuals.WaterDistortionWaves = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "distortion2", t.visuals.WaterDistortionWaves);
	return 0;
}
int SetRippleWaterSpeed(lua_State *L)
{
	t.visuals.WaterSpeed1 = lua_tonumber(L, 1);
	SetEffectConstantF(t.terrain.effectstartindex + 1, "WaterSpeed1", t.visuals.WaterSpeed1);
	return 0;
}
//getter
int GetWaterHeight(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.terrain.waterliney_f);
	return 1;
}
int GetWaterWaveIntensity(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterWaveIntensity_f);
	return 1;
}
int GetWaterShaderColorRed(lua_State *L)
{
	lua2 = L;
#ifdef WICKEDENGINE
	lua_pushnumber(L, t.gamevisuals.WaterRed_f);
#else
	lua_pushnumber(L, t.visuals.WaterRed_f);
#endif
	return 1;
}
int GetWaterShaderColorGreen(lua_State *L)
{
	lua2 = L;
#ifdef WICKEDENGINE
	lua_pushnumber(L, t.gamevisuals.WaterGreen_f);
#else
	lua_pushnumber(L, t.visuals.WaterGreen_f);
#endif
	return 1;
}
int GetWaterShaderColorBlue(lua_State *L)
{
	lua2 = L;
#ifdef WICKEDENGINE
	lua_pushnumber(L, t.gamevisuals.WaterBlue_f);
#else
	lua_pushnumber(L, t.visuals.WaterBlue_f);
#endif
	return 1;
}
int GetWaterTransparancy(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterTransparancy_f);
	return 1;
}
int GetWaterReflection(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterReflection_f);
	return 1;
}
int GetWaterReflectionSparkleIntensity(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterReflectionSparkleIntensity);
	return 1;
}
int GetWaterFlowDirectionX(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterFlowDirectionX);
	return 1;
}
int GetWaterFlowDirectionY(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterFlowDirectionY);
	return 1;
}
int GetWaterFlowSpeed(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterFlowSpeed);
	return 1;
}
int GetWaterDistortionWaves(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterDistortionWaves);
	return 1;
}
int GetRippleWaterSpeed(lua_State *L)
{
	lua2 = L;
	lua_pushnumber(L, t.visuals.WaterSpeed1);
	return 1;
}
#ifdef WICKEDENGINE
int GetWaterEnabled(lua_State* L)
{
	lua2 = L;
	lua_pushinteger(L, (lua_Integer)(t.visuals.bWaterEnable));
	return 1;
}
#endif

int GetIsTestgame(lua_State *L) {
	lua2 = L;
	if ((t.game.gameisexe == 0 || g.gprofileinstandalone == 1) && t.game.runasmultiplayer == 0) 
	{
		lua_pushinteger(L, 1);
	}
	else 
	{
		lua_pushinteger(L, 0);
	}
	return 1;
}

//Dynamic sun.

int SetSunDirection(lua_State *L) 
{
	float fAx = lua_tonumber(L, 1);
	float fAy = lua_tonumber(L, 2);
	float fAz = lua_tonumber(L, 3);
	t.visuals.SunAngleX = fAx;
	t.visuals.SunAngleY = fAy;
	t.visuals.SunAngleZ = fAz;
	WickedCall_SetSunDirection(t.visuals.SunAngleX, t.visuals.SunAngleY, t.visuals.SunAngleZ);
	return 0;
}

#ifdef STORYBOARD

//Storyboaard
int FindLuaScreenNode(char* name);
int FindLuaScreenTitleNode(char* name);
int FindLuaScreenTitleNodeByKey(char* key);

int GetStoryboardActive(lua_State *L)
{
	lua2 = L;

	char pScreenName[512];
	strcpy(pScreenName, lua_tostring(L, 1));

	int iNode = FindLuaScreenNode(pScreenName);
	if (iNode >= 0 && strlen(Storyboard.gamename) > 0)
	{
		lua_pushinteger(L, 1);
	}
	else
	{
		lua_pushinteger(L, 0);
	}
	return 1;
}

int iSpecialLuaReturn = -1;
int SetScreenHUDGlobalScale(lua_State* L)
{
	float fGlobalScaleMod = lua_tonumber(L, 1);
	extern void screen_editor_setscalemod(float);
	screen_editor_setscalemod (fGlobalScaleMod);
	return 0;
}

#endif // STORYBOARD - continued in part4
