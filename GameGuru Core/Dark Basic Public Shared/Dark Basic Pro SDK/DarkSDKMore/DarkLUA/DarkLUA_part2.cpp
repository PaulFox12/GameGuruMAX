//
//
//

#ifdef WICKEDENGINE
int SetEntityAttachmentVisibility (lua_State *L, bool bVisible)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int e = lua_tonumber(L, 1);
	if (e > 0 && e < t.entityelement.size())
	{
		int iGunID = t.entityelement[e].eleprof.hasweapon;
		if (iGunID > 0)
		{
			if (t.gun[iGunID].alwaysshowenemyweapon == 1) bVisible = true;
			int obj = t.entityelement[e].obj;
			if (obj > 0)
			{
				if (ObjectExist(obj) == 1 && GetVisible(obj) == 1)
				{
					int attachobj = t.entityelement[e].attachmentobj;
					if (attachobj > 0)
					{
						if (ObjectExist(attachobj) == 1)
						{
							if (bVisible == true)
								ShowObject(attachobj);
							else
								HideObject(attachobj);
						}
					}
				}
			}
		}
	}
	return 1;
}
int HideEntityAttachment (lua_State *L) { return SetEntityAttachmentVisibility(L, false); }
int ShowEntityAttachment (lua_State *L) { return SetEntityAttachmentVisibility(L, true); }
#endif

//
//
//

#ifdef WICKEDENGINE

int SetDebuggingData (lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tonumber(L, 1);
	int instructionindex = lua_tonumber(L, 2);

	extern int instruction_running_e;
	if (instruction_running_e == e)
	{
		extern int instruction_running_index;
		instruction_running_index = instructionindex;
	}

	return 1;
}

#endif

//
// New RecastDetour(RD) AI Commands
//

#ifdef WICKEDENGINE

int RDFindPath (lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 6) return 0;

	// generate path
	float fStart[3];
	fStart[0] = lua_tonumber(L, 1);
	fStart[1] = lua_tonumber(L, 2);
	fStart[2] = lua_tonumber(L, 3);
	float fEnd[3];
	fEnd[0] = lua_tonumber(L, 4);
	fEnd[1] = lua_tonumber(L, 5);
	fEnd[2] = lua_tonumber(L, 6);
	g_RecastDetour.findPath(fStart, fEnd);

	// done
	return 1;
}

int RD_GetPoint_Core (float* pXYZ,int iCurrentPoint)
{
	int iPointCount = 0;
	float* pfPointData = NULL;
	g_RecastDetour.getPath(&iPointCount, &pfPointData);
	if (iPointCount > 1 && iCurrentPoint <= iPointCount)
	{
		*(pXYZ + 0) = *(pfPointData + (iCurrentPoint * 3) + 0);
		*(pXYZ + 1) = *(pfPointData + (iCurrentPoint * 3) + 1);
		*(pXYZ + 2) = *(pfPointData + (iCurrentPoint * 3) + 2);
		return iPointCount;
	}
	return 0;
}

int RDGetPathPointCount(lua_State *L)
{
	lua2 = L;
	float thisPoint[3] = { 0, 0, 0 };
	int iCount = RD_GetPoint_Core(thisPoint,0);
	lua_pushinteger (L, (lua_Integer)iCount);
	return 1;
}

int RDGetPathPointX(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iPointIndex = lua_tonumber(L, 1);
	float thisPoint[3] = { 0, 0, 0 };
	RD_GetPoint_Core(thisPoint, iPointIndex);
	lua_pushnumber (L, thisPoint[0]);
	return 1;
}

int RDGetPathPointY(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iPointIndex = lua_tonumber(L, 1);
	float thisPoint[3] = { 0, 0, 0 };
	RD_GetPoint_Core(thisPoint, iPointIndex);
	lua_pushnumber (L, thisPoint[1]);
	return 1;
}

int RDGetPathPointZ(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iPointIndex = lua_tonumber(L, 1);
	float thisPoint[3] = { 0, 0, 0 };
	RD_GetPoint_Core(thisPoint, iPointIndex);
	lua_pushnumber (L, thisPoint[2]);
	return 1;
}

int StartMoveAndRotateToXYZ (lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 3 || n > 5) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		// store path on a per character basis
		float thisPoint[3] = { 0, 0, 0 };
		int iPointCount = RD_GetPoint_Core(thisPoint, 0);
		t.charanimstates[t.tcharanimindex].pathPointCount = iPointCount;
		for (int p = 0; p < iPointCount; p++)
		{
			RD_GetPoint_Core(thisPoint, p);
			t.charanimstates[t.tcharanimindex].pointx[p] = thisPoint[0];
			t.charanimstates[t.tcharanimindex].pointy[p] = thisPoint[1];
			t.charanimstates[t.tcharanimindex].pointz[p] = thisPoint[2];
		}

		// start the movement mode
		t.charanimstates[t.tcharanimindex].moveToMode = 1;
		float fMoveSpeed = lua_tonumber(L, 2);
		int movingbackward = 0; if (fMoveSpeed < 0.0f) movingbackward = 1;
		t.charanimstates[t.tcharanimindex].movingbackward = movingbackward;
		t.charanimstates[t.tcharanimindex].movespeed_f = fabs(fMoveSpeed);
		float fTurnSpeed = lua_tonumber(L, 3);
		t.charanimstates[t.tcharanimindex].turnspeed_f = fTurnSpeed;
		int iTiltMode = 0;
		if (n >= 4) iTiltMode = lua_tonumber(L, 4);
		t.charanimstates[t.tcharanimindex].iTiltMode = iTiltMode;
		int iStopFromEnd = 10;
		if (n >= 5) iStopFromEnd = lua_tonumber(L, 5);
		if (iStopFromEnd < 10) iStopFromEnd = 10;
		t.charanimstates[t.tcharanimindex].iStopFromEnd = iStopFromEnd;
	}
	return 1;
}

int MoveAndRotateToXYZ (lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 3 || n > 5) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	int iPointIndex = 0;
	float fDistanceToDest = 0.0f;
	if (t.tcharanimindex != -1)
	{
		if (t.charanimstates[t.tcharanimindex].moveToMode > 0 )
		{
			float fMoveSpeed = lua_tonumber(L, 2);
			t.charanimstates[t.tcharanimindex].movespeed_f = fabs(fMoveSpeed);
			t.charanimstates[t.tcharanimindex].turnspeed_f = lua_tonumber(L, 3);

			int iStopFromEnd = 10;
			if (n == 4) iStopFromEnd = lua_tonumber(L, 4);
			if (iStopFromEnd < 10) iStopFromEnd = 10;
			t.charanimstates[t.tcharanimindex].iStopFromEnd = iStopFromEnd;

			iPointIndex = t.charanimstates[t.tcharanimindex].moveToMode;
			fDistanceToDest = t.charanimstates[t.tcharanimindex].remainingOverallDistanceToDest_f;
		}
	}
	lua_pushinteger (L, (lua_Integer)iPointIndex);
	lua_pushnumber (L, fDistanceToDest);
	return 2;
}

int SetEntityPathRotationMode (lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	t.e = lua_tonumber(L, 1);
	entity_lua_findcharanimstate();
	if (t.tcharanimindex != -1)
	{
		int mode = lua_tonumber(L, 2);
		t.charanimstates[t.tcharanimindex].iRotationAlongPathMode = mode;
	}
}

int RDIsWithinMesh(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	int iResult = 0;
	if (g_RecastDetour.isWithinNavMesh(fX, fY, fZ) == true)
		iResult = 1;
	else
		iResult = 0;
	lua_pushinteger (L, (lua_Integer)iResult);
	return 1;
}

int RDIsWithinAndOverMesh(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	int iResult = 0;
	float vecNearestPt[3];
	if (g_RecastDetour.isWithinNavMeshEx(fX, fY, fZ, (float*)&vecNearestPt, true) == true)
		iResult = 1;
	else
		iResult = 0;
	lua_pushinteger (L, (lua_Integer)iResult);
	return 1;
}

int RDGetYFromMeshPosition(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float thisPoint[3] = { 0, 0, 0 };
	float fNewY = g_RecastDetour.getYFromPos(fX, fY, fZ);
	// this could push object under terrain!!
	//fNewY -= 5.0f; // adjust from navmesh Y to real world Y
	// instead use result of RDGetYFromMeshPosition to then cast a proper ray to find real polygon surface!
	lua_pushnumber (L, fNewY);
	return 1;
}

int RDBlockNavMeshCore(lua_State* L,int iWithShape)
{
	// block and unblock navmesh
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (iWithShape == 0 && n < 5) return 0;
	if (iWithShape == 1 && n < 7) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fRadius = lua_tonumber(L, 4);
	int iBlockMode = lua_tonumber(L, 5);
	float fRadius2 = fRadius;
	float fAngle = 0.0f;
	float fAdjMinY = -5.0f;
	float fAdjMaxY = 95.0f;
	if (iWithShape == 1)
	{
		fRadius2 = lua_tonumber(L, 6);
		fAngle = lua_tonumber(L, 7);
		if (n == 9)
		{
			fAdjMinY = lua_tonumber(L, 8);
			fAdjMaxY = lua_tonumber(L, 9);
		}
	}
	float thisPoint[3] = { 0, 0, 0 };
	bool bEnableBlock = false;
	if (iBlockMode != 0) bEnableBlock = true;

	// improved system for more accurate doo and navmesh blocker bounds
	g_RecastDetour.ToggleBlocker(fX, fY, fZ, fRadius, bEnableBlock, fRadius2, fAngle, fAdjMinY, fAdjMaxY);

	// go through ALL characters and request them to recalc their paths, they may no longer be valid
	int storete = t.e;
	int storecharanimindex = t.tcharanimindex;
	for (int e = 1; e <= g.entityelementmax; e++)
	{
		t.e = e; entity_lua_findcharanimstate();
		if (t.tcharanimindex != -1)
		{
			// before we trigger a path update, ensure this AIs current axtive path (if any), goes THROUGH the above blocker
			bool bThisPathWasAffected = false;
			int iPointCount = t.charanimstates[t.tcharanimindex].pathPointCount;
			int iPointIndex = t.charanimstates[t.tcharanimindex].moveToMode;
			if (iPointIndex > 0 && iPointCount > 0)
			{
				float fCurrentX = t.entityelement[e].x;
				float fCurrentZ = t.entityelement[e].z;
				bool bTradingToEndOfPath = true;
				while (bTradingToEndOfPath)
				{
					// point in path
					float thisPoint[3] = { -1, -1, -1 };
					thisPoint[0] = t.charanimstate.pointx[iPointIndex];
					thisPoint[1] = t.charanimstate.pointy[iPointIndex];
					thisPoint[2] = t.charanimstate.pointz[iPointIndex];

					// trace from current to point, does it pass through the blocker?
					float fDistX = thisPoint[0] - fCurrentX;
					float fDistZ = thisPoint[2] - fCurrentZ;
					float fDist = sqrt((fDistX * fDistX) + (fDistZ * fDistZ));
					if (fDist > 0.1f)
					{
						// step through the sliced up path line and see if at each point we entered the blocker area
						for (int iSlice = 0; iSlice < 100; iSlice++)
						{
							float fSliceX = fCurrentX + (fDistX * ((float)iSlice / 100.0f));
							float fSliceZ = fCurrentZ + (fDistZ * ((float)iSlice / 100.0f));
							if (iWithShape == 1)
							{
								// rectangle formed by fRadius as X and fRadius2 as Z and the angle
								float fRelativeToCenterOfBlockerX = fSliceX - fX;
								float fRelativeToCenterOfBlockerZ = fSliceZ - fZ;
								float fRelativeDist = sqrt(fabs(fRelativeToCenterOfBlockerX * fRelativeToCenterOfBlockerX) + fabs(fRelativeToCenterOfBlockerZ * fRelativeToCenterOfBlockerZ));
								float fFinalAngle = GGToDegree(atan2(fRelativeToCenterOfBlockerX, fRelativeToCenterOfBlockerZ)) + fAngle;
								fRelativeToCenterOfBlockerX = NewXValue(0, fFinalAngle, fRelativeDist);
								fRelativeToCenterOfBlockerZ = NewZValue(0, fFinalAngle, fRelativeDist);
								if (fabs(fRelativeToCenterOfBlockerX) < fRadius && fabs(fRelativeToCenterOfBlockerZ) < fRadius2)
								{
									// this point is within the blocker defined above
									if(iBlockMode!=0) bThisPathWasAffected = true;
									break;
								}
							}
							else
							{
								// simple radius check
								if ( fabs(fSliceX - fX) < fRadius && fabs(fSliceZ - fZ) < fRadius )
								{
									// this point is within blocker defined above
									if (iBlockMode != 0) bThisPathWasAffected = true;
									break;
								}
							}
						}
					}

					// move through all points to end of full path
					fCurrentX = thisPoint[0];
					fCurrentZ = thisPoint[2];
					iPointIndex++;
					if (iPointIndex >= iPointCount) bTradingToEndOfPath = false;
				}
			}
			if (bThisPathWasAffected == true)
			{
				// this triggers follow call in script to set new target (old one not valid any more after this)
				t.entityelement[e].lua.interuptpath = 5;// 50; prevent jiggling about looking for new path!
			}
		}
	}
	t.tcharanimindex = storecharanimindex;
	t.e = storete;

	// return successfully
	return 0;
}

int RDBlockNavMeshWithShape(lua_State* L)
{
	return RDBlockNavMeshCore(L,1);
}

int RDBlockNavMesh(lua_State *L)
{
	return RDBlockNavMeshCore(L,0);
}

// TokenDrop functions (include RecastDetour for convenience of navmesh rendering system)

int DoTokenDrop(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 5) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	int iType = lua_tonumber(L, 4);
	float fDuration = lua_tonumber(L, 5);
	g_RecastDetour.DoTokenDrop(fX, fY, fZ, iType, fDuration);
	return 0;
}

int GetTokenDropCount(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n > 0) return 0;
	int iTokenDropCount = g_RecastDetour.GetTokenDropCount();
	lua_pushinteger (L, (lua_Integer)iTokenDropCount);
	return 1;
}
int GetTokenDropX(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropX(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropY(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropY(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropZ(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropZ(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropType(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropType(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}
int GetTokenDropTimeLeft(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iIndex = lua_tonumber(L, 1);
	float fValue = g_RecastDetour.GetTokenDropTimeLeft(iIndex);
	lua_pushnumber (L, fValue);
	return 1;
}


int AdjustPositionToGetLineOfSight (lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 8) return 0;
	int iIgnoreObjNo = lua_tonumber(L, 1);
	float fX = lua_tonumber(L, 2);
	float fY = lua_tonumber(L, 3);
	float fZ = lua_tonumber(L, 4);
	float fTargetPosX = lua_tonumber(L, 5);
	float fTargetPosY = lua_tonumber(L, 6);
	float fTargetPosZ = lua_tonumber(L, 7);
	float fRadiusOfAdjustment = lua_tonumber(L, 8);
	// project line from A to B, and if blocked, try different A positions until we have a line of sight
	// if cannot find one, return the current A position passed in and set iFoundLineOfSight to zero
	int iStaticOnly = 1;
	int iFoundLineOfSight = 0;
	int tthitvalue = 0;
	if (ODERayTerrain(fX, fY, fZ, fTargetPosX, fTargetPosY, fTargetPosZ, true) == 1) tthitvalue = -1;
	if ( tthitvalue == 0 ) tthitvalue = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fTargetPosX, fTargetPosY, fTargetPosZ, iIgnoreObjNo, iStaticOnly, 0, 0, 1, false);
	if ( tthitvalue != 0 )
	{
		// current A position cannot see B, try a few locations using spiral search up to adjustment radius
		float fNewX = fX;
		float fNewZ = fZ;
		float fSpiralA = 0.0f;
		float fSpiralDistance = 0.0f;
		while (fSpiralDistance < fRadiusOfAdjustment)
		{
			fSpiralA += 45.0f;
			fSpiralDistance += (25.0f / 4); //PE: Optimizing Lee test ( was (25.0f / 8) ) set at 4 so it make a circle 2 times with different distance (16 max calls).
			fNewX = fX + (cos(GGToRadian(fSpiralA))*fSpiralDistance);
			fNewZ = fZ + (sin(GGToRadian(fSpiralA))*fSpiralDistance);
			tthitvalue = 0;
			//PE: Optimizing - Snowy Mountain Stroll is getting hit by this in the tunnel. huge fps drop.
			//PE: Optimizing , this is hitting WickedCall_ SentRay3 many times (32 max calls currently) (25.0f / 8).
			if (ODERayTerrain(fNewX, fY, fNewZ, fTargetPosX, fTargetPosY, fTargetPosZ, true) == 1) tthitvalue = -1;
			if (tthitvalue == 0) tthitvalue = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fNewX, fY, fNewZ, fTargetPosX, fTargetPosY, fTargetPosZ, iIgnoreObjNo, iStaticOnly, 0, 0, 1, false);
			if (tthitvalue != 0)
			{
				// still blocked
			}
			else
			{
				// found line of sight if we shift position
				iFoundLineOfSight = 1;
				fX = fNewX;
				fZ = fNewZ;
				break;
			}
		}
	}
	else
	{
		// easy line of sight, success
		iFoundLineOfSight = 1;
	}
	lua_pushinteger (L, (lua_Integer)iFoundLineOfSight);
	lua_pushnumber (L, fX);
	lua_pushnumber (L, fZ);
	return 3;
}

int SetCharacterMode(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	float e = lua_tonumber(L, 1);
	float mode = lua_tonumber(L, 2);
	int iSuccess = 0;
	int entid = t.entityelement[e].bankindex;
	if (t.entityprofile[entid].ischaracter == 1)
	{
		// mode 0 = will disable this character so no longer handles as a character, sets object free for non-character control
		// mode 1 = will restore character as a regular character (acting as a normal ischaracter=1 object)
		t.entityelement[e].eleprof.disableascharacter = 1 - mode;
		iSuccess = 1;
	}
	lua_pushinteger (L, (lua_Integer)iSuccess);
	return 1;
}

#endif

int GetDeviceWidth(lua_State *L)
{
	lua2 = L;

	lua_pushinteger ( L , GetDisplayWidth() );

	return 1;
}

int GetDeviceHeight(lua_State *L)
{
	lua2 = L;

	lua_pushinteger ( L , GetDisplayHeight() );

	return 1;
}

int GetFirstEntitySpawn(lua_State *L)
{
	lua2 = L;

	int id = 0;

	if ( t.delayOneFrameForActivatedLua == 0 )
	{
		if ( t.entitiesActivatedForLua.size() > 0 )
		{
			id = t.entitiesActivatedForLua.back();
			t.entitiesActivatedForLua.pop_back();
		}
	}
	else 
		t.delayOneFrameForActivatedLua = 0;

	lua_pushinteger ( L , id );

	return 1;
}

// VR and Head Tracking

int GetHeadTracker(lua_State *L)
{
	lua2 = L;
	int id = 0;
	//if ( GGVR_IsHmdPresent() > 0 && g.vrglobals.GGVRUsingVRSystem == 1 ) id = 1;
	extern int g_iActivelyUsingVRNow;
	if (GGVR_IsHmdPresent() > 0 && g_iActivelyUsingVRNow == 1) id = 1;
	lua_pushinteger ( L , id );
	return 1;
}
int ResetHeadTracker(lua_State *L)
{
	lua2 = L;
	int id = 0;
	#ifdef VRTECH
	#else
	 SetupResetTracking();
	#endif
	lua_pushinteger ( L , id );
	return 1;
}
int GetHeadTrackerYaw(lua_State *L)
{
	lua2 = L;
	#ifdef VRTECH
	 float fValue = GGVR_GetHMDYaw();// + g_fDriverCompensationYaw;
	#else
	 float fValue = g_fVR920TrackingYaw + g_fDriverCompensationYaw;
	 if ( g_VR920AdapterAvailable == false ) fValue = 0.0f;
	#endif
	lua_pushnumber ( L , fValue );
	return 1;
}
int GetHeadTrackerPitch(lua_State *L)
{
	lua2 = L;
	#ifdef VRTECH
	 float fValue = GGVR_GetHMDPitch();// + g_fDriverCompensationYaw;
	#else
	 float fValue = g_fVR920TrackingPitch + g_fDriverCompensationPitch;
	 if ( g_VR920AdapterAvailable == false ) fValue = 0.0f;
	#endif
	lua_pushnumber ( L , fValue );
	return 1;
}
int GetHeadTrackerRoll(lua_State *L)
{
	lua2 = L;
	#ifdef VRTECH
	 float fValue = GGVR_GetHMDRoll();// + g_fDriverCompensationYaw;
	#else
	 float fValue = g_fVR920TrackingRoll + g_fDriverCompensationRoll;
	 if ( g_VR920AdapterAvailable == false ) fValue = 0.0f;
	#endif
	lua_pushnumber ( L , fValue );
	return 1;
}

int GetHeadTrackerNormalX(lua_State *L)
{
	lua2 = L;
#ifdef WICKEDENGINE
    float fValue = GGVR_GetHMDRNormalX();
#else
	float fValue = 0.0f;
#endif
	lua_pushnumber ( L , fValue );
	return 1;
}

int GetHeadTrackerNormalY(lua_State *L)
{
	lua2 = L;
#ifdef WICKEDENGINE
	float fValue = GGVR_GetHMDRNormalY();
#else
	float fValue = 0.0f;
#endif
	lua_pushnumber ( L , fValue );
	return 1;
}

int GetHeadTrackerNormalZ(lua_State *L)
{
	lua2 = L;
#ifdef WICKEDENGINE
	float fValue = GGVR_GetHMDRNormalZ();
#else
	float fValue = 0.0f;
#endif
	lua_pushnumber ( L , fValue );
	return 1;
}


// PROMPT 3D

int Prompt3D(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	char pTextToRender[1024];
	strcpy ( pTextToRender, lua_tostring(L, 1));
	DWORD dwPrompt3DTime = lua_tonumber(L, 2);
	lua_prompt3d(pTextToRender, MAXTimer() + dwPrompt3DTime , 0 );
	return 1;
}

int PositionPrompt3D(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fZ = lua_tonumber(L, 3);
	float fAY = lua_tonumber(L, 4);
	lua_positionprompt3d(0, fX, fY, fZ, fAY, false );
	return 1;
}


int PromptLocalDuration(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	int storee = t.e;
	cstr stores = t.s_s;

	t.e = lua_tonumber(L, 1);
	const char* pStrPtr = lua_tostring(L, 2);
	if (pStrPtr)
		t.s_s = pStrPtr;
	else
		t.s_s = "";
	int addtime = lua_tonumber(L, 3);
	if (addtime < 100) addtime = 1000;

	void lua_promptlocalcore(int iTrueLocalOrForVR, int addtime = 1000);
	lua_promptlocalcore(0,addtime);

	t.e = storee;
	t.s_s = stores;

	return 0;
}



// AGK IMAGE AND SPRITE COMMANDS

int LoadImage(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, send 0 result back
	if ( n < 1 )
	{
		lua_pushinteger ( L , 0 );
		return 1;
	}

	// required image
	char filename[MAX_PATH];
	strcpy ( filename , cstr(g.fpscrootdir_s+"\\Files\\").Get());
	LPSTR pImgFile = (LPSTR)lua_tostring(L, 1);
	if (pImgFile && strlen(pImgFile) < 256) strcat (filename, pImgFile);

	// attempt a load
	int iID = GetFreeLUAImageID();
	if (iID > 0)
	{
		//LB: ensure scripts that load non existent images do not crash out!
		image_setlegacyimageloading(true);
		if (FileExist (filename) == 0)
		{
			strcpy (filename, cstr(g.fpscrootdir_s + "\\Files\\effectbank\\common\\dot.png").Get());
		}
		if (FileExist (filename) == 1)
		{
			LoadImage (filename, iID);
		}
		else
		{
			iID = 0;
		}
		image_setlegacyimageloading(false);
	}
	lua_pushinteger ( L , (lua_Integer)iID );
	return 1;
}

int GetImageWidth(lua_State *L)
{
	// get LUA param
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushinteger ( L , 0 ); return 1; }

	// get image width
	int iImageID = lua_tointeger(L, 1);
	float fImageWidth = ( ((float)ImageWidth(iImageID)/(float)g_dwScreenWidth) * 100.0f);

	// push return value
	lua_pushnumber ( L , fImageWidth );

	// success
	return 1;
}

int GetImageHeight(lua_State *L)
{
	// get LUA param
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushinteger ( L , 0 ); return 1; }

	// get image width
	int iImageID = lua_tointeger(L, 1);
	float fImageHeight = ( ((float)ImageHeight(iImageID)/(float)g_dwScreenHeight) * 100.0f);

	// push return value
	lua_pushnumber ( L , fImageHeight );

	// success
	return 1;
}

int DeleteSpriteImage(lua_State *L)
{
	// get LUA param
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushinteger ( L , 0 ); return 1; }

	// get image width
	int iImageID = lua_tointeger(L, 1);
	if (iImageID > 0)
	{
		if (ImageExist(iImageID) == 1)
			DeleteImage(iImageID);
	}

	// push return value
	lua_pushinteger ( L , 1 );

	// success
	return 1;
}

int CreateSprite(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, send 0 result back
	if ( n < 1 )
	{
		lua_pushinteger ( L , 0 );
		return 1;
	}

	int iID = 0;
	int iImageID = lua_tointeger(L, 1);
	if (iImageID > 0)
	{
		if (ImageExist(iImageID) == 1)
		{
			iID = GetFreeLUASpriteID();
			if (iID > 0)
			{
				MAXSprite (iID, 0, 0, iImageID);
				SetSpritePriority (iID, 90); // which is 10 in agk
			}
		}
	}
	lua_pushinteger ( L , (lua_Integer)iID );
	return 1;
}

int PasteSprite(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) { lua_pushinteger ( L , 0 ); return 1; }
	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		PasteSprite (iID, SpriteX(iID), SpriteY(iID));
	}
	return 0;
}

int PasteSpritePosition(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) { lua_pushinteger ( L , 0 ); return 1; }
	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		//PE: Thanks AmenMoses for this fix :)
		float fX = lua_tonumber(L, 2);
		float fY = lua_tonumber(L, 3);

		fX = (fX * g_dwScreenWidth) / 100.0f;
		fY = (fY * g_dwScreenHeight) / 100.0f;
		PasteSprite (iID, fX, fY);
	}
	return 0;
}

int SetSpriteScissor(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 4) return 0;
	float fX = lua_tonumber(L, 1);
	float fY = lua_tonumber(L, 2);
	float fW = lua_tonumber(L, 3);
	float fH = lua_tonumber(L, 4);
	fX = (fX / 100.0f) * g_dwScreenWidth;
	fY = (fY / 100.0f) * g_dwScreenHeight;
	fW = (fW / 100.0f) * g_dwScreenWidth;
	fH = (fH / 100.0f) * g_dwScreenHeight;
	ScissorSpriteArea (fX, fY, fW, fH);
	return 0;
}

int SetSpriteImage(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int iID = lua_tointeger(L, 1);
	float image = lua_tointeger(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			MAXSprite (iID, SpriteX(iID), SpriteY(iID), image);
		}
	}
	return 0;
}

int SetSpritePosition(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 3 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		float x = lua_tonumber(L, 2);
		float y = lua_tonumber(L, 3);

		x = (x * g_dwScreenWidth) / 100.0f;
		y = (y * g_dwScreenHeight) / 100.0f;

		if (iID > 0)
		{
			if (SpriteExist (iID) == 1)
			{
				MAXSprite (iID, x, y, GetSpriteImage(iID));
			}
		}
	}
	return 0;
}

int SetSpritePriorityForLUA(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int iID = lua_tointeger(L, 1);
	int iPriority = lua_tointeger(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			SetSpritePriority (iID, iPriority);
		}
	}
	return 0;
}

int SetSpriteDepth(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int iID = lua_tointeger(L, 1);
	// need to flip it as agk does the order reversed
	float depth = 100 - lua_tointeger(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			SetSpritePriority (iID, depth);
		}
	}
	return 0;
}

int SetSpriteColor(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 5 )
		return 0;

	int iID = lua_tointeger(L, 1);
	int red = lua_tointeger(L, 2);
	int green = lua_tointeger(L, 3);
	int blue = lua_tointeger(L, 4);
	int alpha = lua_tointeger(L, 5);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			SetSpriteAlpha (iID, alpha);
			SetSpriteDiffuse (iID, red, green, blue);
		}
	}
	return 0;
}

int SetSpriteAngle(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 2 )
		return 0;

	int iID = lua_tointeger(L, 1);
	int angle = lua_tonumber(L, 2);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			RotateSprite (iID, angle);
		}
	}
	return 0;
}

int DeleteSprite(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 1 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		if (SpriteExist (iID) == 1)
		{
			DeleteSprite (iID);
		}
	}
	return 0;
}

int SetSpriteOffset(lua_State *L)
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 3 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		float x = lua_tonumber(L, 2);
		float y = lua_tonumber(L, 3);

		if (x == -1 && y == -1) return 0;

		if (x != -1)
		{
			x = (x * g_dwScreenWidth) / 100.0f;
		}

		if (y != -1)
		{
			y = (y * g_dwScreenHeight) / 100.0f;

			if (x == -1)
			{
				float perc = (y / ImageHeight(GetSpriteImage(iID))) * 100.0f;
				x = ((perc * ImageWidth(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
			}
		}
		else
		{
			float perc = (x / ImageWidth(GetSpriteImage(iID))) * 100.0f;
			y = ((perc * ImageHeight(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
		}

		if (SpriteExist (iID) == 1)
		{
			OffsetSprite (iID, x, y);
		}
	}
	return 0;
}

int SetSpriteSize ( lua_State *L )
{
	lua2 = L;

	// get number of arguments
	int n = LUA_GETTOP(L);

	// Not enough params, return out
	if ( n < 3 )
		return 0;

	int iID = lua_tointeger(L, 1);
	if (iID > 0)
	{
		float sizeX = lua_tonumber(L, 2);
		float sizeY = lua_tonumber(L, 3);

		//PE: vertex data use iXOffset)+iWidth-0.5f, (-0.5f) soo add a bit.
		//PE: mainly visible when using 100 percent
		//PE: https://github.com/TheGameCreators/GameGuruRepo/issues/423
		//PE: So 100 percent do not fill the entire screen.
		//PE: Did not want to change vertex data as it might change how sprites display on other level.
		//PE: So for now just do this to fill the hole screen. (should be changed in vertex at some point)

		if (sizeX == 100) {
			sizeX += 0.1;
		}
		if (sizeY == 100) {
			sizeY += 0.1;
		}

		if (sizeX == -1 && sizeY == -1) return 0;

		if (sizeX != -1)
		{
			sizeX = (sizeX * g_dwScreenWidth) / 100.0f;
		}

		if (sizeY != -1)
		{
			sizeY = (sizeY * g_dwScreenHeight) / 100.0f;

			if (sizeX == -1)
			{
				float perc = (sizeY / ImageHeight(GetSpriteImage(iID))) * 100.0f;
				sizeX = ((perc * ImageWidth(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
			}
		}
		else
		{
			//PE: 11-06-19 issue: https://github.com/TheGameCreators/GameGuruRepo/issues/504
			//PE: I cant test this on my system , but assume we could always use the backbuffer size g_pGlob->iScreenWidth instead of the screenwidth g_dwScreenWidth
			//PE: Can someone with a similar screen setup do this, test the return of these 2 MessageBox.
			//PE: They should always be the same , but issue indicate they are not.
			//PE: Just add rader.lua and enable the below 4 lines to test :)
	//		char tmp[80]; sprintf(tmp, "g_pGlob->iScreenWidth: %d", g_pGlob->iScreenWidth); // 1920
	//		MessageBox(NULL, tmp, "g_pGlob->iScreenWidth", MB_TOPMOST | MB_OK);
	//		sprintf(tmp, "g_dwScreenWidth: %d", g_dwScreenWidth); // 1920
	//		MessageBox(NULL, tmp, "g_dwScreenWidth", MB_TOPMOST | MB_OK);
			float perc = (sizeX / ImageWidth(GetSpriteImage(iID))) * 100.0f;
			sizeY = ((perc * ImageHeight(GetSpriteImage(iID))) / 100.0f) * (g_dwScreenWidth / g_dwScreenHeight);
		}

		if (SpriteExist (iID) == 1)
		{
			SizeSprite (iID, sizeX, sizeY);
		}
	}
	return 0;
}

int DrawSpritesFirstForLUA ( lua_State *L )
{
	DrawSpritesFirst();
	return 0;
}

int DrawSpritesLastForLUA ( lua_State *L )
{
	DrawSpritesLast();
	return 0;
}

int BackdropOffForLUA ( lua_State *L )
{
	BackdropOff();
	return 0;
}

int BackdropOnForLUA ( lua_State *L )
{
	BackdropOn();
	return 0;
}

int LoadGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	const char* pFilename = lua_tostring(L, 1);
	int iID = g.globalsoundoffset + lua_tointeger(L, 2);
	if ( SoundExist(iID)==1 ) DeleteSound(iID);
	LoadSound((LPSTR)pFilename,iID);
	return 0;
}
int PlayGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		PlaySound(iID);
	}
	return 0;
}
int LoopGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		LoopSound(iID);
	}
	return 0;
}
int StopGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		StopSound(iID);
	}
	return 0;
}
int DeleteGlobalSound ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	if ( SoundExist(iID)==1 )
	{
		DeleteSound(iID);
	}
	return 0;
}
int SetGlobalSoundSpeed ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	int iSpeed = lua_tointeger(L, 2);
	if ( SoundExist(iID)==1 )
	{
		SetSoundSpeed(iID,iSpeed);
	}
	return 0;
}
int SetGlobalSoundVolume ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	int iVolume = lua_tointeger(L, 2);
	if ( SoundExist(iID)==1 )
	{
		SetSoundVolume(iID,iVolume);
		extern std::unordered_map<int, float> luavolumes;
		luavolumes.insert_or_assign(iID, iVolume);
	}
	return 0;
}
int GetGlobalSoundExist(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	lua_pushinteger ( L , SoundExist ( iID ) );
	return 1;
}
int GetGlobalSoundPlaying(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	lua_pushinteger ( L , SoundPlaying ( iID ) );
	return 1;
}
int GetGlobalSoundLooping(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iID = g.globalsoundoffset + lua_tointeger(L, 1);
	lua_pushinteger ( L , SoundLooping ( iID ) );
	return 1;
}

int GetSoundPlaying(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	int e = lua_tointeger(L, 1);
	int v = lua_tointeger(L, 2);
	int tsnd = 0;
	if (v == 0) tsnd = t.entityelement[e].soundset;
	if (v == 1) tsnd = t.entityelement[e].soundset1;
	if (v == 2) tsnd = t.entityelement[e].soundset2;
	if (v == 3) tsnd = t.entityelement[e].soundset3;
	if (v == 4) tsnd = t.entityelement[e].soundset4;
	if (v == 5) tsnd = t.entityelement[e].soundset5;
	if (v == 6) tsnd = t.entityelement[e].soundset6;
	int iPlaying = 0;
	if (tsnd > 0)
	{
		if (SoundExist(tsnd) == 1 && SoundPlaying(tsnd) == 1)
		{
			iPlaying = tsnd;
		}
	}
	lua_pushinteger (L, iPlaying);
	return 1;
}

int SetRawSoundData ( lua_State *L, int iDataMode )
{
	lua2 = L;
	int iParamNum = 0;
	switch ( iDataMode )
	{
		case 1 : iParamNum = 1;	break;
		case 2 : iParamNum = 1;	break;
		case 3 : iParamNum = 1;	break;
		case 4 : iParamNum = 2;	break;
		case 6 : iParamNum = 2;	break;
	}
	int n = LUA_GETTOP(L);
	if ( n < iParamNum ) return 0;
	int iSoundID = lua_tonumber(L, 1);
	if (iSoundID > 0 && SoundExist(iSoundID) == 1)
	{
		switch (iDataMode)
		{
		case 1: PlaySound(lua_tonumber(L, 1)); break;
		case 2: LoopSound(lua_tonumber(L, 1)); break;
		case 3: StopSound(lua_tonumber(L, 1)); break;
		case 4:
		{
			int sndid = lua_tonumber(L, 1);
			float volume = soundtruevolume(lua_tonumber(L, 2));
			SetSoundVolume(sndid, volume);
			extern std::unordered_map<int, float> luavolumes;
			luavolumes.insert_or_assign(sndid, volume);
			break;
		}
		case 5: SetSoundSpeed(lua_tonumber(L, 1), lua_tonumber(L, 2)); break;
		case 6: SetSoundDistanceScale(lua_tonumber(L, 1), lua_tonumber(L, 2)); break; // GG: the sound's own 3D distance scale (SetRawSoundDistanceScale)
		}
	}
	return 0;
}
int GetRawSoundData ( lua_State *L, int iDataMode )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	switch ( iDataMode )
	{
		case 1 : lua_pushinteger ( L , (lua_Integer)SoundExist ( lua_tonumber(L, 1) ) ); break;
		case 2 : lua_pushinteger ( L , (lua_Integer)SoundPlaying ( lua_tonumber(L, 1) ) ); break;
		case 3 : lua_pushinteger ( L , (lua_Integer)(t.entityelement[lua_tonumber(L, 1)].soundset) ); break;
		case 4 : lua_pushnumber ( L , GetSoundDistanceScale ( lua_tonumber(L, 1) ) ); break; // GG: the sound's 3D distance scale, its own or the global one
	}
	return 1;
}
int PlayRawSound ( lua_State *L ) { return SetRawSoundData ( L, 1 ); }
int LoopRawSound ( lua_State *L ) { return SetRawSoundData ( L, 2 ); }
int StopRawSound ( lua_State *L ) { return SetRawSoundData ( L, 3 ); }
int SetRawSoundVolume ( lua_State *L ) { return SetRawSoundData ( L, 4 ); }
int SetRawSoundSpeed ( lua_State *L ) { return SetRawSoundData ( L, 5 ); }
int RawSoundExist ( lua_State *L ) { return GetRawSoundData ( L, 1 ); }
int RawSoundPlaying ( lua_State *L ) { return GetRawSoundData ( L, 2 ); }

int GetEntityRawSound(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iE = lua_tonumber(L, 1);
	int iSoundSlot = lua_tonumber(L, 2);
	int iRawSoundIndex = 0;
	if (iSoundSlot == 0) iRawSoundIndex = t.entityelement[iE].soundset;
	if (iSoundSlot == 1) iRawSoundIndex = t.entityelement[iE].soundset1;
	if (iSoundSlot == 2) iRawSoundIndex = t.entityelement[iE].soundset2;
	if (iSoundSlot == 3) iRawSoundIndex = t.entityelement[iE].soundset3;
	if (iSoundSlot == 4) iRawSoundIndex = t.entityelement[iE].soundset4;
	if (iSoundSlot == 5) iRawSoundIndex = t.entityelement[iE].soundset5;
	if (iSoundSlot == 6) iRawSoundIndex = t.entityelement[iE].soundset6;
	lua_pushinteger ( L , (lua_Integer)iRawSoundIndex );
	return 1;
}

#ifdef WICKEDENGINE
int StartAmbientMusicTrack(lua_State* L)
{
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (FileExist(t.visuals.sAmbientMusicTrack.Get()) == 1)
		{
			if (SoundExist(iFreeSoundID) == 1)
			{
				LoopSound(iFreeSoundID);
				SetSoundVolume(iFreeSoundID, t.visuals.iAmbientMusicTrackVolume);
				extern std::unordered_map<int, float> luavolumes;
				luavolumes.insert_or_assign(iFreeSoundID, t.visuals.iAmbientMusicTrackVolume);
			}
		}
	}
	return 0;
}
int StopAmbientMusicTrack(lua_State* L)
{
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
		{
			StopSound(iFreeSoundID);
		}
	}
	return 0;
}
int SetAmbientMusicTrackVolume(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	if (t.gamevisuals.bEndableAmbientMusicTrack)
	{
		int iFreeSoundID = g.temppreviewsoundoffset + 3;
		if (SoundExist(iFreeSoundID) == 1)
		{
			float fVolumePercentage = lua_tonumber(L, 1) / 100.0f;
			float fFinalVolume = t.visuals.iAmbientMusicTrackVolume * fVolumePercentage;
			SetSoundVolume(iFreeSoundID, fFinalVolume);
			extern std::unordered_map<int, float> luavolumes;
			luavolumes.insert_or_assign(iFreeSoundID, fFinalVolume);
		}
	}
	return 0;
}
int StartCombatMusicTrack(lua_State* L)
{
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (FileExist(t.visuals.sCombatMusicTrack.Get()) == 1)
	{
		if (SoundExist(iFreeSoundID) == 1)
		{
			LoopSound(iFreeSoundID);
			SetSoundVolume(iFreeSoundID, t.visuals.iCombatMusicTrackVolume);
			extern std::unordered_map<int, float> luavolumes;
			luavolumes.insert_or_assign(iFreeSoundID, t.visuals.iCombatMusicTrackVolume);
		}
	}
	return 0;
}
int StopCombatMusicTrack(lua_State* L)
{
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
	{
		StopSound(iFreeSoundID);
	}
	return 0;
}
int SetCombatMusicTrackVolume(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1)
	{
		float fVolumePercentage = lua_tonumber(L, 1) / 100.0f;
		float fFinalVolume = t.visuals.iCombatMusicTrackVolume * fVolumePercentage;
		SetSoundVolume(iFreeSoundID, fFinalVolume);
		extern std::unordered_map<int, float> luavolumes;
		luavolumes.insert_or_assign(iFreeSoundID, fFinalVolume);
	}
	return 0;
}
int GetCombatMusicTrackPlaying(lua_State *L)
{
	lua2 = L;
	int iPlaying = 0;
	int iFreeSoundID = g.temppreviewsoundoffset + 5;
	if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
	{
		iPlaying = 1;
	}
	lua_pushinteger (L, (lua_Integer)iPlaying);
	return 1;
}

int SetSoundMusicMode(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	extern bool g_bSoundIsMusic[65536];
	int iSoundIndex = lua_tonumber(L, 1);
	g_bSoundIsMusic[iSoundIndex] = lua_tonumber(L, 2);
	//audio_volume_update(); this is a MASSIVE PERF HIT FOR LEVELS THAT USE LOTS OF SOUNDS
	return 1;
}
int GetSoundMusicMode(lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	extern bool g_bSoundIsMusic[65536];
	int iSoundIndex = lua_tonumber(L, 1);
	lua_pushinteger (L, (lua_Integer)(g_bSoundIsMusic[iSoundIndex]));
	return 1;
}

#endif

// Voice

#ifdef VRTECH
int GetSpeech(lua_State *L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iRunning = 0;
	int iE = lua_tonumber(L, 1);
	int iCharAnimIndex = -1;
	for ( int tcharanimindex = 1 ; tcharanimindex <= g.charanimindexmax; tcharanimindex++ )
	{
		if (  t.charanimstates[tcharanimindex].e == iE ) { iCharAnimIndex = tcharanimindex  ; break; }
	}
	if ( iCharAnimIndex != -1 )
	{
		if ( t.charanimstates[iCharAnimIndex].ccpo.speak.fMouthTimeStamp > 0.0f ) iRunning = 1;
	}
	lua_pushinteger ( L, iRunning );
	return 1;
}
#endif

// Generic

int GetTimeElapsed ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, g.timeelapsed_f );
	return 1;
}

int GetKeyState ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iKeyValue = lua_tonumber(L, 1);
	lua_pushinteger ( L, (lua_Integer)KeyState(g.keymap[iKeyValue]) );
	return 1;
}

int SetGlobalTimer (lua_State* L)
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iRestoreToTime = lua_tonumber(L, 1);
	extern DWORD g_dwAppLocalTimeStart;
	extern void SetLocalTimerReset(void);
	SetLocalTimerReset();
	g_dwAppLocalTimeStart -= iRestoreToTime;
	return 1;
}

int GetGlobalTimer ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)MAXTimer() );
	return 1;
}

int MouseMoveX ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)MouseMoveX() );
	return 1;
}
int MouseMoveY ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)MouseMoveY() );
	return 1;
}
int GetDesktopWidth ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)GetDesktopWidth() );
	return 1;
}
int GetDesktopHeight ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)GetDesktopHeight() );
	return 1;
}
int CurveValue ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, CurveValue(a, b, c) );
	return 1;
}
int CurveAngle ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, CurveAngle(a, b, c) );
	return 1;
}
int PositionMouse ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	float fScreenX = lua_tonumber(L, 1);
	float fScreenY = lua_tonumber(L, 2);
	PositionMouse ( fScreenX, fScreenY );
	g.LUAMouseX = fScreenX;
	g.LUAMouseY = fScreenY;
	return 0;
}

int GetDynamicCharacterControllerDidJump ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)ODEGetDynamicCharacterControllerDidJump() );
	return 1;
}
int GetCharacterControllerDucking ( lua_State *L )
{
	lua2 = L;
	lua_pushinteger ( L, (lua_Integer)ODEGetCharacterControllerDucking(t.aisystem.objectstartindex) );
	return 1;
}

int WrapValue ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	float a = lua_tonumber(L, 1);
	lua_pushnumber ( L, WrapValue(a) );
	return 1;
}
int GetElapsedTime ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, t.ElapsedTime_f );
	return 1;
}
int GetPlrObjectPositionX ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, ObjectPositionX(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectPositionY ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, ObjectPositionY(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectPositionZ ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, ObjectPositionZ(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectAngleX ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, ObjectAngleX(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectAngleY ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, ObjectAngleY(t.aisystem.objectstartindex) );
	return 1;
}
int GetPlrObjectAngleZ ( lua_State *L )
{
	lua2 = L;
	lua_pushnumber ( L, ObjectAngleZ(t.aisystem.objectstartindex) );
	return 1;
}
int GetGroundHeight ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	float x = lua_tonumber(L, 1);
	float z = lua_tonumber(L, 2);
	lua_pushnumber ( L, BT_GetGroundHeight(t.terrain.TerrainID,x,z) );
	return 1;
}
int NewXValue ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, NewXValue(a, b, c) );
	return 1;
}
int NewZValue ( lua_State *L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if ( n < 3 ) return 0;
	float a = lua_tonumber(L, 1);
	float b = lua_tonumber(L, 2);
	float c = lua_tonumber(L, 3);
	lua_pushnumber ( L, NewZValue(a, b, c) );
	return 1;
}

int ControlDynamicCharacterController ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 8 ) return 0;
	float fAngleY = lua_tonumber(L, 1);
	float fAngleX = lua_tonumber(L, 2);
	float fSpeed = lua_tonumber(L, 3);
	float fJump = lua_tonumber(L, 4);
	float fDucking = lua_tonumber(L, 5);
	float fPushAngle = lua_tonumber(L, 6);
	float fPushForce = lua_tonumber(L, 7);
	float fThrustUpwards = lua_tonumber(L, 8);

	#ifdef VRTECH
	if ( g.vrglobals.GGVREnabled == 0 )
	{
		// no VR control
		ODESetDynamicCharacterController (t.aisystem.objectstartindex, t.terrain.waterliney_f, 0, 0, 0, 0, 0, 0);
		ODEControlDynamicCharacterController ( t.aisystem.objectstartindex, fAngleY, fAngleX, fSpeed, fJump, fDucking, fPushAngle, fPushForce, fThrustUpwards );
	}
	else
	{
		// VR Control (standing or seated)
		double Modified_fAngleY = 0.0;
		double norm = 0.0;
		if ( g.vrglobals.GGVRStandingMode == 1 )
		{
			// VR Controlled player capsule
			double norm_XOffset = 0.0;
			double norm_ZOffset = 0.0;

			// Rotate the offset around the Yaw of the HMD to make it relative to the HMD facing
			double radian = 0.0174532988888;
			double modifiedX = 0.0;
			double modifiedZ = 0.0;
			double HMDYaw = GGVR_GetHMDYaw();
			double camL = t.playercontrol.finalcameraangley_f;

			if (HMDYaw < 0.0)
			{
				HMDYaw = 360.0 + HMDYaw;
			}
			if (HMDYaw > 360.0)
			{
				HMDYaw = HMDYaw - 360.0;
			}
			if (camL < 0.0)
			{
				camL = 360.0 + camL;
			}
			if (camL > 360.0)
			{
				camL = camL - 360.0;
			}

			double yl = (camL - HMDYaw);

			if (yl < 0.0f)
			{
				yl = 360.0f + yl;
			}
			if (yl > 360.0f)
			{
				yl = yl - 360.0f;
			}
		
			yl = yl *  radian;
			double cosYL = cos(yl);  double sinYL = sin(yl); double nsinYL = -sin(yl);

			// move player along X and Z if in standing mode
			modifiedX = (sin(fAngleY*radian)*fSpeed) + ((g.vrglobals.GGVR_XposOffsetChange*cosYL) + (g.vrglobals.GGVR_ZposOffsetChange*sinYL));
			modifiedZ = (cos(fAngleY*radian)*fSpeed) + ((g.vrglobals.GGVR_XposOffsetChange*nsinYL) + (g.vrglobals.GGVR_ZposOffsetChange*cosYL));
		
			norm_XOffset = 0.0;
			norm_ZOffset = 0.0;
			Modified_fAngleY = 0.0;
		
			//Work out the motion angle of the HMD in the play area
			norm = sqrt((modifiedX*modifiedX) + (modifiedZ*modifiedZ));
			if (norm != 0.0)
			{
				double XOffset = modifiedX / norm;
				double ZOffset = modifiedZ / norm;
				double MovementAngle = 0.0f;

				if (XOffset == 0.0)
				{
					if (ZOffset > 0.0f)
					{
						MovementAngle = 0.0f;
					}
					else
					{
						MovementAngle = 180.0f;
					}
				}
				if (XOffset > 0.0)
				{
					if (ZOffset >= 0.0f)
					{
						MovementAngle = Asin(XOffset);
					}
					else
					{
						MovementAngle = 180.0f - Asin(XOffset);
					}
				}
				if (XOffset < 0.0)
				{
					if (ZOffset >= 0.0)
					{
						MovementAngle = 360.0f + Asin(XOffset);
					}
					else
					{
						MovementAngle = 180.0f - Asin(XOffset);
					}
				}

				Modified_fAngleY = MovementAngle;

			}
			else
			{
				norm_XOffset = 0.0;
				norm_ZOffset = 0.0;
				norm = 0.0;
				Modified_fAngleY = fAngleY;
			}
		}
		else
		{
			norm = fSpeed;
			Modified_fAngleY = fAngleY;
		}
		ODESetDynamicCharacterController (t.aisystem.objectstartindex, t.terrain.waterliney_f, 0, 0, 0, 0, 0, 0);
		ODEControlDynamicCharacterController(t.aisystem.objectstartindex, Modified_fAngleY, fAngleX, norm, fJump, fDucking, fPushAngle, fPushForce, fThrustUpwards);
	}
	#else
		// no VR control
		ODESetDynamicCharacterController (t.aisystem.objectstartindex, t.terrain.waterliney_f, 0, 0, 0, 0, 0, 0);
		ODEControlDynamicCharacterController ( t.aisystem.objectstartindex, fAngleY, fAngleX, fSpeed, fJump, fDucking, fPushAngle, fPushForce, fThrustUpwards );
	#endif
	return 0;
}

int SetCharacterDirectionOverride(lua_State* L)
{
	// Check for the correct parameter count.
	int n = LUA_GETTOP(L);
	if (n < 7) 
		return 0;

	// Extract parameter values.
	float fAngleX = lua_tonumber(L, 1);
	float fAngleY = lua_tonumber(L, 2);
	float fAngleZ = lua_tonumber(L, 3);
	float fAxisX = lua_tonumber(L, 4);
	float fAxisY = lua_tonumber(L, 5);
	float fAxisZ = lua_tonumber(L, 6);
	float fMagnitude = lua_tonumber(L, 7);
	
	ODESetCharacterDirectionOverride(fAngleX, fAngleY, fAngleZ, fAxisX, fAxisY, fAxisZ, fMagnitude);

	return 1;
}

