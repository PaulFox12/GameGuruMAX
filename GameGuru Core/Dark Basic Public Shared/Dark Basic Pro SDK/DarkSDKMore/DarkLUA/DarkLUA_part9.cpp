// Hired Gun: the Lua functions the game's scripts call that the DX11 branch added. Carried as they are where their
// engine side exists in this build; the rest answer with neutral values (no hit, zero, empty) until that side is ported.
// Registered by addFunctions_hiredgun(), called from addFunctions().

#include "..\GGTerrain\GGTrees.h"
#include "..\..\..\..\GameGuru\Include\GGPhysicsQuery.h"

// declared as the DX11 branch's DarkLUA.cpp declares them
extern void WickedCall_SetSunColors(float fRed, float fGreen, float fBlue, float fEnergy, float fFov, float fShadowBias);
extern void LuaPostEffects_SetBloom(int iEnabled, float fStrength, float fThreshold, float fRadius, float fCap);
extern void LuaPostEffects_GetBloom(int* piEnabled, float* pfStrength, float* pfThreshold, float* pfRadius, float* pfCap);
extern void LuaPostEffects_SetDepthOfField(int iEnabled, float fStrength, float fFocalLength, float fApertureSize);
extern void LuaPostEffects_GetDepthOfField(int* piEnabled, float* pfStrength, float* pfFocalLength, float* pfApertureSize);
extern void LuaPostEffects_SetLightShafts(int iEnabled);
extern void LuaPostEffects_SetLensFlare(int iEnabled, float fSunHue, float fSunBrightness);
extern void LuaPostEffects_GetLensFlareSun(float* pfSunHue, float* pfSunBrightness);
extern void LuaPostEffects_SetScreenLensFlare(int iEnabled, float fIntensity, float fThreshold, float fSpacing, float fHalo, int iNoSky, float fCap, int iFollowBloom);
extern void LuaPostEffects_GetScreenLensFlare(int* piEnabled, float* pfIntensity, float* pfThreshold, float* pfSpacing, float* pfHalo, int* piNoSky, float* pfCap, int* piFollowBloom);
extern bool GetLightShaftState(void);
extern bool GetLensFlareState(void);
extern void LuaRenderSettings_SetOcclusionCulling(int iOn, int iObjects, int iAnimations, int iTerrain, int iShadows);
extern void LuaRenderSettings_SetLODMultiplier(float fMultiplier);
extern void LuaRenderSettings_SetShadowsLowestLOD(int iOn);
extern void LuaRenderSettings_SetDelayedShadows(int iOn, int iLaptop);
extern void LuaRenderSettings_ResetDelayedShadows(void);
extern void LuaRenderSettings_GetDelayedShadows(int* pOn, int* pLaptop, int* pScript);
extern void LuaRenderSettings_SetShadowRange(float fRange);
extern void LuaRenderSettings_SetSSR(int iOn);
extern void LuaRenderSettings_SetAO(int iOn, float fPower);
extern void LuaRenderSettings_SetSpotShadowCulling(int iOn);
extern void LuaRenderSettings_SetFXAA(int iOn);
extern void LuaRenderSettings_SetReflections(int iOn);
extern void LuaRenderSettings_SetLowestLOD(int iProbes, int iReflections);
extern void LuaRenderSettings_SetAnimations30Fps(int iOn);
extern void LuaRenderSettings_SetMaxApparentSize(float fSize);
extern void LuaRenderSettings_SetShadowResolution(int iSun, int iSpot, int iPoint);
extern void LuaRenderSettings_SetShadowLights(int iSpotMax, int iPointMax);
extern void LuaRenderSettings_SetShadowCascades(int iCascades, const float* pSplits);
extern void LuaRenderSettings_SetTerrainDetail(int iLimit, float fScale, int iReadBack);
extern void LuaRenderSettings_SetGrassSimpleLighting(int iOn);
extern void LuaRenderSettings_ResetGraphics(void);
extern void LuaRenderSettings_SetShadowJobWait(int iOn);
extern void LuaRenderSettings_SetVsync(int iOn);
extern void LuaRenderSettings_SetFrameRateCap(int iFramesPerSecond);
extern void LuaRenderSettings_GetDisplay(int* pVsync, int* pFrameRateCap);
extern void LuaRenderSettings_SetRenderScale(float fScale);
extern float LuaRenderSettings_GetRenderScale(void);
extern void LuaRenderSettings_SetMSAA(int iSamples);
extern int LuaRenderSettings_GetMSAA(void);
extern void LuaRenderSettings_SetGamma(float fGamma);
extern void LuaRenderSettings_SetFSRSharpness(float fSharpness);
extern void LuaRenderSettings_SetRaycastLowestLOD(int iOn);
extern void LuaRenderSettings_SetTransparentShadows(int iOn);
extern void LuaRenderSettings_GetPicture(float* pGamma, float* pFSRSharpness, int* pRaycastLowestLOD, int* pTransparentShadows);
extern void LuaRenderSettings_GetCulling(int* pOcclusion, int* pObjects, int* pAnimations, int* pTerrain, int* pShadows, int* pSpotShadows, int* pShadowsLowestLOD);
extern void LuaRenderSettings_GetLevers(int* pSSR, int* pAO, float* pAOPower, float* pLODMultiplier, float* pShadowRange);
extern void LuaRenderSettings_GetValues(int* pFXAA, int* pReflections, int* pProbesLowestLOD, int* pReflectionsLowestLOD, int* pAnimations30Fps, float* pMaxApparentSize);
extern void LuaRenderSettings_GetShadows(int* pSun, int* pSpot, int* pPoint, int* pSpotMax, int* pPointMax, int* pCascades, float* pSplits);
extern void LuaRenderSettings_GetTerrainDetail(int* pLimit, float* pScale, int* pReadBack, int* pGrassSimpleLighting);

// helpers the carried functions share

// Entity commands index t.entityelement directly, so an id outside it (such as an object number passed by mistake)
// wrote past the end of the array and crashed later somewhere unrelated. iMinID is 1, or 0 / -1 for the few commands
// that treat those as "no entity". Invalid ids are logged once per command and calling script line.
bool LuaEntityIDValid ( lua_State* L, int iEntityID, int iMinID )
{
	if ( iEntityID >= iMinID && iEntityID < (int)t.entityelement.size() ) return true;
	if ( g.gproducelogfiles > 0 )
	{
		static std::unordered_set<std::string> invalidEntityCallsLogged;

		// the call info only names functions called by their global name, so a command called through a local or
		// pcall is looked up in the globals table instead
		std::string name = "?";
		lua_Debug ar;
		if ( lua_getstack ( L, 0, &ar ) && lua_getinfo ( L, "nf", &ar ) )
		{
			if ( ar.name )
			{
				name = ar.name;
			}
			else
			{
				lua_pushglobaltable ( L );
				lua_pushnil ( L );
				while ( lua_next ( L, -2 ) )
				{
					if ( lua_type ( L, -2 ) == LUA_TSTRING && lua_rawequal ( L, -1, -4 ) )
					{
						name = lua_tostring ( L, -2 );
						lua_pop ( L, 2 );
						break;
					}
					lua_pop ( L, 1 );
				}
				lua_pop ( L, 1 );
			}
			lua_pop ( L, 1 );
		}

		// the first two script lines up the stack, skipping C frames such as pcall
		std::string where;
		int iScriptFrames = 0;
		for ( int level = 1; iScriptFrames < 2 && lua_getstack ( L, level, &ar ); level++ )
		{
			if ( lua_getinfo ( L, "Sl", &ar ) && ar.currentline > 0 )
			{
				char pFrame[300];
				snprintf ( pFrame, sizeof(pFrame), "%s%s:%d", iScriptFrames ? " < " : "", ar.short_src, ar.currentline );
				where += pFrame;
				iScriptFrames++;
			}
		}

		if ( invalidEntityCallsLogged.insert(name + where).second )
		{
			char pLog[1024];
			snprintf ( pLog, sizeof(pLog), "Lua %s ignored invalid entity %d (valid %d to %d) at %s", name.c_str(), iEntityID, iMinID, (int)t.entityelement.size()-1, where.c_str() );
			timestampactivity ( 0, pLog );
		}
	}
	return false;
}

// limbs of an entity's object, numbered as GetLimbName and SetLimbIndex number them; the renderer follows each change
static sObject* LuaEntityLimbObject ( lua_State* L, int e, int iLimb )
{
	 if ( !LuaEntityIDValid ( L, e, 1 ) ) return NULL;
	 int iObj = t.entityelement[e].obj;
	 if ( iObj <= 0 || ObjectExist ( iObj ) == 0 ) return NULL;
	 sObject* pObject = GetObjectData ( iObj );
	 if ( !pObject || !pObject->ppFrameList || iLimb < 0 || iLimb >= pObject->iFrameCount || !pObject->ppFrameList[iLimb] ) return NULL;
	 return pObject;
}

// grows a box, in the space of the limb asked about, by a limb's mesh box and with bChildren by those of the limbs under it
// (their rest pose: each child's matOriginal carries it into its parent's space)
static void LuaAddLimbBounds ( sFrame* pFrame, GGMATRIX& matToLimb, bool bChildren, GGVECTOR3& vecMin, GGVECTOR3& vecMax, bool& bAny )
{
	 sMesh* pMesh = pFrame->pMesh;
	 if ( pMesh && pMesh->dwVertexCount > 0 )
	 {
		 for ( int iCorner = 0; iCorner < 8; iCorner++ )
		 {
			 GGVECTOR3 vecCorner = GGVECTOR3 ( (iCorner & 1) ? pMesh->Collision.vecMax.x : pMesh->Collision.vecMin.x,
											   (iCorner & 2) ? pMesh->Collision.vecMax.y : pMesh->Collision.vecMin.y,
											   (iCorner & 4) ? pMesh->Collision.vecMax.z : pMesh->Collision.vecMin.z );
			 GGVec3TransformCoord ( &vecCorner, &vecCorner, &matToLimb );
			 if ( !bAny ) { vecMin = vecCorner; vecMax = vecCorner; bAny = true; }
			 vecMin.x = min ( vecMin.x, vecCorner.x ); vecMin.y = min ( vecMin.y, vecCorner.y ); vecMin.z = min ( vecMin.z, vecCorner.z );
			 vecMax.x = max ( vecMax.x, vecCorner.x ); vecMax.y = max ( vecMax.y, vecCorner.y ); vecMax.z = max ( vecMax.z, vecCorner.z );
		 }
	 }
	 if ( bChildren )
	 {
		 for ( sFrame* pChild = pFrame->pChild; pChild; pChild = pChild->pSibling )
		 {
			 GGMATRIX matChildToLimb = pChild->matOriginal * matToLimb;
			 LuaAddLimbBounds ( pChild, matChildToLimb, true, vecMin, vecMax, bAny );
		 }
	 }
}

// the collision cylinders the physics keeps at the trees round the camera (physics_managevirtualtreecylinders): a hit on
// one is a tree trunk, reported as the map-wide trunks are
static bool LuaPhysicsIsTreeCylinder(int iObject)
{
	return iObject >= g.virtualtreeobjectstart && iObject < g.virtualtreeobjectstart + PHYSICS_VIRTUALTREE_MAX;
}

// the ignore argument of the intersect commands: an object number, or a table of object numbers
static void LuaReadIntersectIgnore(lua_State* L, int iArg, std::vector<int>& ignore)
{
	if (lua_istable(L, iArg))
	{
		for (int i = 1; i <= 256; i++)
		{
			lua_rawgeti(L, iArg, i);
			bool bEnd = lua_isnil(L, -1);
			int iObj = bEnd ? 0 : (int)lua_tonumber(L, -1);
			lua_pop(L, 1);
			if (bEnd) break;
			if (iObj > 0) ignore.push_back(iObj);
		}
	}
	else
	{
		int iObj = lua_tonumber(L, iArg);
		if (iObj > 0) ignore.push_back(iObj);
	}
}

// the object numbers a physics query ignores: one number, or a table of up to PHYSICS_QUERY_MAX_IGNORE (as IntersectRay's)
static int LuaPhysicsIgnoreList(lua_State* L, int iIndex, int* pIgnore)
{
	int iCount = 0;
	if (LUA_GETTOP(L) < iIndex) return 0;
	if (lua_istable(L, iIndex))
	{
		int iLength = (int)lua_rawlen(L, iIndex);
		for (int i = 1; i <= iLength && iCount < PHYSICS_QUERY_MAX_IGNORE; i++)
		{
			lua_rawgeti(L, iIndex, i);
			int iObj = (int)lua_tointeger(L, -1);
			lua_pop(L, 1);
			if (iObj > 0) pIgnore[iCount++] = iObj;
		}
	}
	else
	{
		int iObj = (int)lua_tointeger(L, iIndex);
		if (iObj > 0) pIgnore[iCount++] = iObj;
	}
	return iCount;
}

// hit, x, y, z, nx, ny, nz, obj, fraction, tree, treetype (iTreeType for a map-wide trunk, else the cylinder's), surface
static int LuaPushPhysicsHit(lua_State* L, const PhysicsHit& hit, bool bTree, int iTreeType)
{
	bool bTreeHit = hit.hit && (bTree || LuaPhysicsIsTreeCylinder(hit.object));
	extern int physics_virtualtreetype(int iObject);
	if (bTreeHit && !bTree) iTreeType = physics_virtualtreetype(hit.object);
	lua_pushinteger(L, hit.hit ? 1 : 0);
	for (int i = 0; i < 3; i++) lua_pushnumber(L, hit.hit ? hit.point[i] : 0.0f);
	for (int i = 0; i < 3; i++) lua_pushnumber(L, hit.hit ? hit.normal[i] : 0.0f);
	lua_pushinteger(L, (hit.hit && !bTreeHit) ? hit.object : 0);
	lua_pushnumber(L, hit.hit ? hit.fraction : 1.0f);
	lua_pushinteger(L, bTreeHit ? 1 : 0);
	lua_pushinteger(L, bTreeHit ? iTreeType : -1);
	lua_pushinteger(L, (hit.hit && !bTreeHit) ? hit.surface : PHYSICS_SURFACE_OTHER);
	return 12;
}

float LuaOptionalNumber(lua_State* L, int n, int index)
{
	if (n < index) return -1.0f;
	return lua_tonumber(L, index);
}

// SetEntityVelocity(e, vx, vy, vz): e's physics body moving at that velocity (units a second), woken;
// SetEntityAngularVelocity(e, wx, wy, wz): spinning at that rate (degrees a second) about the world's x, y and z axes,
// woken. A frozen entity (FreezeEntity) or one with no body is left as it is
int SetEntityVelocityCore ( lua_State *L, int iAngular )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( !LuaEntityIDValid ( L, iEntityIndex, 1 ) ) return 0;
	int iObjectNumber = t.entityelement[iEntityIndex].obj;
	if ( iObjectNumber <= 0 ) return 0;
	float fX = lua_tonumber(L, 2);
	float fY = lua_tonumber(L, 3);
	float fZ = lua_tonumber(L, 4);
	if ( iAngular == 1 )
		ODESetAngularVelocityRadians ( iObjectNumber, GGToRadian(fX), GGToRadian(fY), GGToRadian(fZ) );
	else
		ODESetLinearVelocity ( iObjectNumber, fX, fY, fZ );
	return 0;
}

// carried from the DX11 branch

// AddBulletHole(x, y, z, nx, ny, nz [, material [, e]]): a bullet hole where a script's shot hit, as the player's gunfire
// leaves one. material is an entity profile's materialindex (1 stone, 2 metal, 3 wood, 4 glass; default 1; 0 leaves
// none). With e, the hole obeys the same rules as gunfire: only on a static entity or one that allows bullet holes, and
// it goes when that entity moves, hides or is removed. Returns 1 if a hole was requested, 0 if a rule refused it
int AddBulletHole ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	float fNX = lua_tonumber(L, 4), fNY = lua_tonumber(L, 5), fNZ = lua_tonumber(L, 6);
	float fLength = sqrtf ( fNX*fNX + fNY*fNY + fNZ*fNZ );
	int iMaterial = 1;
	if ( n >= 7 ) iMaterial = max ( 0, min ( 15, (int)lua_tointeger(L, 7) ) );
	int e = 0;
	if ( n >= 8 ) e = lua_tointeger(L, 8);
	bool bAllowed = fLength > 0.0001f && iMaterial > 0 && ObjectExist ( g.bulletholesobject ) == 1;
	if ( bAllowed && e != 0 )
	{
		bAllowed = LuaEntityIDValid ( L, e, 1 ) && entity_allowsbulletholes ( e );
	}
	if ( bAllowed ) bulletholes_add ( iMaterial, lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), fNX / fLength, fNY / fLength, fNZ / fLength, e );
	lua_pushinteger(L, bAllowed ? 1 : 0);
	return 1;
}

// BulletRay(x1, y1, z1, x2, y2, z2 [, ignoree [, holes [, terrainmaterial]]]): a script weapon's shot, detected as the player's
// bullets are (entity_scriptbulletray). ignoree is the shooter's own entity (0 for none). holes 1 (default) leaves a bullet hole
// under the player's rules, 0 only detects. terrainmaterial 0 (default) leaves no hole on terrain, as the player's guns; 1 to 15
// uses that material. Returns hit (0 none, 1 terrain, 2 object, 5 tree), e (0 if not an entity), x, y, z, nx, ny, nz, material, hole (1/0)
int BulletRay ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	int iIgnoreObj = 0;
	if ( n >= 7 )
	{
		int iIgnoreE = lua_tointeger(L, 7);
		if ( iIgnoreE > 0 && LuaEntityIDValid ( L, iIgnoreE, 1 ) ) iIgnoreObj = t.entityelement[iIgnoreE].obj;
	}
	bool bLeaveHole = true;
	if ( n >= 8 ) bLeaveHole = lua_tointeger(L, 8) != 0;
	int iTerrainMaterial = 0;
	if ( n >= 9 ) iTerrainMaterial = max ( 0, min ( 15, (int)lua_tointeger(L, 9) ) );
	sScriptBulletRayHit hit;
	int iHit = entity_scriptbulletray ( lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), iIgnoreObj, bLeaveHole, iTerrainMaterial, &hit );
	lua_pushinteger(L, iHit);
	lua_pushinteger(L, hit.e);
	lua_pushnumber(L, hit.fX);
	lua_pushnumber(L, hit.fY);
	lua_pushnumber(L, hit.fZ);
	lua_pushnumber(L, hit.fNX);
	lua_pushnumber(L, hit.fNY);
	lua_pushnumber(L, hit.fNZ);
	lua_pushinteger(L, hit.iMaterial);
	lua_pushinteger(L, hit.bHole ? 1 : 0);
	return 10;
}

int GetBloom(lua_State* L)
{
	int iEnabled = 0;
	float fStrength = 0, fThreshold = 0, fRadius = 0, fCap = 0;
	LuaPostEffects_GetBloom(&iEnabled, &fStrength, &fThreshold, &fRadius, &fCap);
	lua_pushnumber(L, iEnabled);
	lua_pushnumber(L, fStrength);
	lua_pushnumber(L, fThreshold);
	lua_pushnumber(L, fRadius);
	lua_pushnumber(L, fCap);
	return 5;
}

int GetDecalRange(lua_State *L)
{
	lua_pushinteger(L, g.decalrange);
	return 1;
}

// GetDelayedShadows(): on, laptop as in force now, and script 1 while a SetDelayedShadows value holds
int GetDelayedShadows(lua_State* L)
{
	int iOn, iLaptop, iScript;
	LuaRenderSettings_GetDelayedShadows(&iOn, &iLaptop, &iScript);
	lua_pushinteger(L, iOn);
	lua_pushinteger(L, iLaptop);
	lua_pushinteger(L, iScript);
	return 3;
}

int GetDepthOfField(lua_State* L)
{
	int iEnabled = 0;
	float fStrength = 0, fFocalLength = 0, fApertureSize = 0;
	LuaPostEffects_GetDepthOfField(&iEnabled, &fStrength, &fFocalLength, &fApertureSize);
	lua_pushnumber(L, iEnabled);
	lua_pushnumber(L, fStrength);
	lua_pushnumber(L, fFocalLength);
	lua_pushnumber(L, fApertureSize);
	return 4;
}

// GetEntityAxes(e): the world directions of e's model's own forward (z) and up (y) axes as it is drawn, each of unit
// length: fx, fy, fz, ux, uy, uz. A tumbled body's pose with no angle order to follow; nothing if e has no object
int GetEntityAxes ( lua_State* L )
{
	 if ( LUA_GETTOP(L) < 1 ) return 0;
	 int e = lua_tointeger(L, 1);
	 if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;
	 int iObj = t.entityelement[e].obj;
	 if ( iObj <= 0 || ObjectExist ( iObj ) == 0 ) return 0;
	 sObject* pObject = GetObjectData ( iObj );
	 if ( !pObject ) return 0;
	 const GGMATRIX& matWorld = pObject->position.matWorld;
	 GGVECTOR3 vecForward ( matWorld._31, matWorld._32, matWorld._33 );
	 GGVECTOR3 vecUp ( matWorld._21, matWorld._22, matWorld._23 );
	 if ( GGVec3Length ( &vecForward ) < 0.000001f || GGVec3Length ( &vecUp ) < 0.000001f ) return 0;
	 GGVec3Normalize ( &vecForward, &vecForward );
	 GGVec3Normalize ( &vecUp, &vecUp );
	 lua_pushnumber(L, vecForward.x);
	 lua_pushnumber(L, vecForward.y);
	 lua_pushnumber(L, vecForward.z);
	 lua_pushnumber(L, vecUp.x);
	 lua_pushnumber(L, vecUp.y);
	 lua_pushnumber(L, vecUp.z);
	 return 6;
}

// GetEntityImmunity(e): 0 when damage can take the entity's health to 0; otherwise its health stays at 1 or more and
// SetEntityHealth(e, 0) does nothing: -1 held by SetEntityHealth(e, -99999) until SetEntityHealthSilent(e, -1) or (e, 0),
// or the frames left of the brief immunity after a resurrection
int GetEntityImmunity ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	int iImmunity = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) ) iImmunity = t.entityelement[e].briefimmunity;
	lua_pushinteger ( L, iImmunity );
	return 1;
}

int GetEntityInstanceEmissive ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	float fR = 0, fG = 0, fB = 0, fStrength = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		int iObj = t.entityelement[e].obj;
		if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
			WickedCall_GetObjectEmissiveTint ( GetObjectData ( iObj ), &fR, &fG, &fB, &fStrength );
	}
	lua_pushnumber ( L, fR );
	lua_pushnumber ( L, fG );
	lua_pushnumber ( L, fB );
	lua_pushnumber ( L, fStrength );
	return 4;
}

int GetEntityInstanceTint ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	float fR = 1, fG = 1, fB = 1;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		int iObj = t.entityelement[e].obj;
		if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
			WickedCall_GetObjectColorTint ( GetObjectData ( iObj ), &fR, &fG, &fB );
	}
	lua_pushnumber ( L, fR );
	lua_pushnumber ( L, fG );
	lua_pushnumber ( L, fB );
	return 3;
}

// GetEntityLimbAlpha(e, limb): the limb's alpha, 0 to 100, or that of the first limb under it with a mesh; 100 if none
int GetEntityLimbAlpha ( lua_State* L )
{
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 float fAlpha = 1.0f;
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( pObject ) fAlpha = WickedCall_GetLimbAlpha ( pObject, iLimb );
	 lua_pushnumber(L, fAlpha * 100.0f);
	 return 1;
}

// GetEntityLimbBounds(e, limb [, children]): minx, miny, minz, maxx, maxy, maxz of the limb's mesh in its own space; with
// children 1 also the limbs under it (a rotor frame with blade limbs), nothing if there is no mesh
int GetEntityLimbBounds ( lua_State* L )
{
	 int n = LUA_GETTOP(L);
	 if ( n < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 bool bChildren = ( n >= 3 && lua_tointeger(L, 3) != 0 );
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( !pObject ) return 0;
	 GGMATRIX matIdentity;
	 GGMatrixIdentity ( &matIdentity );
	 GGVECTOR3 vecMin = GGVECTOR3(0, 0, 0), vecMax = GGVECTOR3(0, 0, 0);
	 bool bAny = false;
	 LuaAddLimbBounds ( pObject->ppFrameList[iLimb], matIdentity, bChildren, vecMin, vecMax, bAny );
	 if ( !bAny ) return 0;
	 lua_pushnumber(L, vecMin.x);
	 lua_pushnumber(L, vecMin.y);
	 lua_pushnumber(L, vecMin.z);
	 lua_pushnumber(L, vecMax.x);
	 lua_pushnumber(L, vecMax.y);
	 lua_pushnumber(L, vecMax.z);
	 return 6;
}

int GetEntityLimbCount ( lua_State* L )
{
	 if ( LUA_GETTOP(L) < 1 ) return 0;
	 int e = lua_tointeger(L, 1);
	 int iCount = 0;
	 if ( LuaEntityIDValid ( L, e, 1 ) )
	 {
		 int iObj = t.entityelement[e].obj;
		 if ( iObj > 0 && ObjectExist ( iObj ) == 1 ) iCount = GetObjectData ( iObj )->iFrameCount;
	 }
	 lua_pushinteger(L, iCount);
	 return 1;
}

int GetEntityLimbName ( lua_State* L )
{
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 lua_pushstring(L, pObject ? pObject->ppFrameList[iLimb]->szName : "");
	 return 1;
}

// GetEntityLimbParent(e, limb): the limb this one hangs from, -1 for the top of the hierarchy or a bad id
int GetEntityLimbParent ( lua_State* L )
{
	 if ( LUA_GETTOP(L) < 2 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 int iParent = -1;
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( pObject )
	 {
		 sFrame* pParent = pObject->ppFrameList[iLimb]->pParent;
		 for ( int i = 0; pParent && i < pObject->iFrameCount; i++ )
		 {
			 if ( pObject->ppFrameList[i] == pParent ) { iParent = i; break; }
		 }
	 }
	 lua_pushinteger(L, iParent);
	 return 1;
}

// GetEntityMass(e) - kg, source: the kilos SetEntityMass gave it (source 1), else its .fpe's physicsmass (2), else its
// body's mass now (0; 0 kilos for no body)
int GetEntityMass(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	int iEntity = (int)lua_tonumber(L, 1);
	float fKg = 0;
	bool bSet = false;
	int iSource = 0;
	if (iEntity > 0 && iEntity < (int)t.entityelement.size())
	{
		extern bool ODEGetEntityMassKg(int iEntity, float* pfKg);
		extern float ODEGetObjectMassKg(int iObject);
		bSet = ODEGetEntityMassKg(iEntity, &fKg);
		if (bSet) iSource = 1;
		else if (t.entityprofile[t.entityelement[iEntity].bankindex].physicsmass > 0)
		{
			fKg = t.entityprofile[t.entityelement[iEntity].bankindex].physicsmass;
			iSource = 2;
		}
		else fKg = ODEGetObjectMassKg(t.entityelement[iEntity].obj);
	}
	lua_pushnumber(L, fKg);
	lua_pushinteger(L, iSource);
	return 2;
}

// GetEntityRagdollForcePending(e): 1 while a SetEntityRagdollForce on e is still to be pushed (held from the kill until
// its ragdoll exists, then pushed over the next frames as it fades), else 0. A build with it keeps a force set before
// the ragdoll exists
int GetEntityRagdollForcePending ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iEntityIndex = lua_tointeger(L, 1);
	int iPending = 0;
	if ( iEntityIndex > 0 && iEntityIndex < (int)t.entityelement.size() ) iPending = t.entityelement[iEntityIndex].ragdollifiedforcevalue_f > 1.0f ? 1 : 0;
	lua_pushinteger ( L, iPending );
	return 1;
}

// stiffness, damping, waistfold, hipfold, kneefold = GetEntityRagdollHoldStiffness(e); nothing when e is no ragdoll
int GetEntityRagdollHoldStiffness ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	if ( !LuaEntityIDValid ( L, e, 1 ) || t.entityelement[e].obj <= 0 ) return 0;
	float fStiffness, fDamping, fWaistFold, fHipFold, fKneeFold;
	if ( !BPhys_RagDollGetHoldStiffness ( t.entityelement[e].obj, &fStiffness, &fDamping, &fWaistFold, &fHipFold, &fKneeFold ) ) return 0;
	lua_pushnumber ( L, fStiffness );
	lua_pushnumber ( L, fDamping );
	lua_pushnumber ( L, fWaistFold );
	lua_pushnumber ( L, fHipFold );
	lua_pushnumber ( L, fKneeFold );
	return 5;
}

// x, y, z, headangle, faceup, resting, held, knockedout = GetEntityRagdollPose(e): where a ragdoll (knocked out or
// dead) lies: its pelvis, the angle from the pelvis to the head, 1 when lying face up, 1 when no body moves, 1 when
// held, 1 when knocked out (alive). Nothing when the entity is no ragdoll
int GetEntityRagdollPose ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;
	float fX, fY, fZ, fHeadAngle;
	int iFaceUp, iResting, iHeld;
	if ( !ragdoll_getpose ( e, &fX, &fY, &fZ, &fHeadAngle, &iFaceUp, &iResting, &iHeld ) ) return 0;
	lua_pushnumber ( L, fX );
	lua_pushnumber ( L, fY );
	lua_pushnumber ( L, fZ );
	lua_pushnumber ( L, fHeadAngle );
	lua_pushinteger ( L, iFaceUp );
	lua_pushinteger ( L, iResting );
	lua_pushinteger ( L, iHeld );
	lua_pushinteger ( L, ragdoll_isknockedout ( e ) ? 1 : 0 );
	return 8;
}

// GetGamePaused(): 1 while SetGamePaused holds the world
int GetGamePaused ( lua_State *L )
{
	extern bool g_bGamePaused;
	lua_pushnumber ( L, g_bGamePaused == true ? 1 : 0 );
	return 1;
}

// GetGraphicsSettings(): the values in force as a table: fxaa, reflections, probesLowestLOD, reflectionsLowestLOD,
// animations30Fps, maxApparentSize, sunShadow, spotShadow, pointShadow (resolutions), spotShadowLights, pointShadowLights,
// shadowCascades, shadowSplits (a table of four), terrainDetailLimit, terrainDetailScale, terrainReadBack, grassSimpleLighting
int GetGraphicsSettings(lua_State* L)
{
	int iFXAA, iReflections, iProbes, iReflectionsLOD, i30Fps;
	float fApparentSize;
	LuaRenderSettings_GetValues(&iFXAA, &iReflections, &iProbes, &iReflectionsLOD, &i30Fps, &fApparentSize);
	int iSun, iSpot, iPoint, iSpotMax, iPointMax, iCascades;
	float fSplits[4];
	LuaRenderSettings_GetShadows(&iSun, &iSpot, &iPoint, &iSpotMax, &iPointMax, &iCascades, fSplits);
	int iLimit, iReadBack, iGrassSimple;
	float fScale;
	LuaRenderSettings_GetTerrainDetail(&iLimit, &fScale, &iReadBack, &iGrassSimple);
	int iVsync, iFrameRateCap;
	LuaRenderSettings_GetDisplay(&iVsync, &iFrameRateCap);
	lua_newtable(L);
	lua_pushinteger(L, iFXAA); lua_setfield(L, -2, "fxaa");
	lua_pushinteger(L, iReflections); lua_setfield(L, -2, "reflections");
	lua_pushinteger(L, iProbes); lua_setfield(L, -2, "probesLowestLOD");
	lua_pushinteger(L, iReflectionsLOD); lua_setfield(L, -2, "reflectionsLowestLOD");
	lua_pushinteger(L, i30Fps); lua_setfield(L, -2, "animations30Fps");
	lua_pushnumber(L, fApparentSize); lua_setfield(L, -2, "maxApparentSize");
	lua_pushinteger(L, iSun); lua_setfield(L, -2, "sunShadow");
	lua_pushinteger(L, iSpot); lua_setfield(L, -2, "spotShadow");
	lua_pushinteger(L, iPoint); lua_setfield(L, -2, "pointShadow");
	lua_pushinteger(L, iSpotMax); lua_setfield(L, -2, "spotShadowLights");
	lua_pushinteger(L, iPointMax); lua_setfield(L, -2, "pointShadowLights");
	lua_pushinteger(L, iCascades); lua_setfield(L, -2, "shadowCascades");
	lua_newtable(L);
	for (int i = 0; i < 4; i++) { lua_pushnumber(L, fSplits[i]); lua_rawseti(L, -2, i + 1); }
	lua_setfield(L, -2, "shadowSplits");
	lua_pushinteger(L, iLimit); lua_setfield(L, -2, "terrainDetailLimit");
	lua_pushnumber(L, fScale); lua_setfield(L, -2, "terrainDetailScale");
	lua_pushinteger(L, iReadBack); lua_setfield(L, -2, "terrainReadBack");
	lua_pushinteger(L, iGrassSimple); lua_setfield(L, -2, "grassSimpleLighting");
	lua_pushinteger(L, iVsync); lua_setfield(L, -2, "vsync");
	lua_pushinteger(L, iFrameRateCap); lua_setfield(L, -2, "frameRateCap");
	lua_pushnumber(L, LuaRenderSettings_GetRenderScale()); lua_setfield(L, -2, "renderScale");
	lua_pushinteger(L, LuaRenderSettings_GetMSAA()); lua_setfield(L, -2, "msaa");
	int iOcclusion, iObjects, iAnimations, iTerrain, iShadows, iSpotShadows, iShadowsLowestLOD;
	LuaRenderSettings_GetCulling(&iOcclusion, &iObjects, &iAnimations, &iTerrain, &iShadows, &iSpotShadows, &iShadowsLowestLOD);
	lua_pushinteger(L, iOcclusion); lua_setfield(L, -2, "occlusion");
	lua_pushinteger(L, iObjects); lua_setfield(L, -2, "objectCulling");
	lua_pushinteger(L, iAnimations); lua_setfield(L, -2, "animationCulling");
	lua_pushinteger(L, iTerrain); lua_setfield(L, -2, "terrainCulling");
	lua_pushinteger(L, iShadows); lua_setfield(L, -2, "shadowCulling");
	lua_pushinteger(L, iSpotShadows); lua_setfield(L, -2, "spotShadowCulling");
	lua_pushinteger(L, iShadowsLowestLOD); lua_setfield(L, -2, "shadowsLowestLOD");
	float fGamma, fFSRSharpness;
	int iRaycastLowestLOD, iTransparentShadows;
	LuaRenderSettings_GetPicture(&fGamma, &fFSRSharpness, &iRaycastLowestLOD, &iTransparentShadows);
	lua_pushnumber(L, fGamma); lua_setfield(L, -2, "gamma");
	lua_pushnumber(L, fFSRSharpness); lua_setfield(L, -2, "fsrSharpness");
	lua_pushinteger(L, iRaycastLowestLOD); lua_setfield(L, -2, "raycastLowestLOD");
	lua_pushinteger(L, iTransparentShadows); lua_setfield(L, -2, "transparentShadows");
	int iSSR, iAO;
	float fAOPower, fLODMultiplier, fShadowRange;
	LuaRenderSettings_GetLevers(&iSSR, &iAO, &fAOPower, &fLODMultiplier, &fShadowRange);
	lua_pushinteger(L, iSSR); lua_setfield(L, -2, "ssr");
	lua_pushinteger(L, iAO); lua_setfield(L, -2, "ao");
	lua_pushnumber(L, fAOPower); lua_setfield(L, -2, "aoPower");
	lua_pushnumber(L, fLODMultiplier); lua_setfield(L, -2, "lodMultiplier");
	lua_pushnumber(L, fShadowRange); lua_setfield(L, -2, "shadowRange");
	return 1;
}

int GetGrassDistance(lua_State* L)
{
	lua_pushnumber(L, gggrass_global_params.lod_dist);
	return 1;
}

int GetLensFlare(lua_State* L)
{
	lua_pushnumber(L, GetLensFlareState() ? 1 : 0);
	// GG: and how far the sun's flare takes its colour and follows its intensity
	float fSunHue = 0, fSunBrightness = 0;
	LuaPostEffects_GetLensFlareSun(&fSunHue, &fSunBrightness);
	lua_pushnumber(L, fSunHue);
	lua_pushnumber(L, fSunBrightness);
	return 3;
}

// GetLuaEntityCosts(): the entities' turns in the engine's Lua loop over the last second (lua_getentitycosts), nil
// until the first second is over
int GetLuaEntityCosts(lua_State* L)
{
	sLuaEntityCosts costs;
	if (!lua_getentitycosts(&costs))
	{
		lua_pushnil(L);
		return 1;
	}
	lua_createtable(L, 0, 14);
	lua_pushnumber(L, costs.dWindowMs); lua_setfield(L, -2, "window");
	lua_pushinteger(L, costs.iFrames); lua_setfield(L, -2, "frames");
	lua_pushnumber(L, costs.dTotal); lua_setfield(L, -2, "total");
	lua_pushnumber(L, costs.dRow); lua_setfield(L, -2, "row");
	lua_pushnumber(L, costs.dMain); lua_setfield(L, -2, "main");
	lua_pushnumber(L, costs.dKey); lua_setfield(L, -2, "key");
	lua_pushinteger(L, costs.iTurns); lua_setfield(L, -2, "turns");
	lua_pushinteger(L, costs.iRows); lua_setfield(L, -2, "rows");
	lua_pushinteger(L, costs.iMains); lua_setfield(L, -2, "mains");
	lua_pushinteger(L, costs.iKeyScans); lua_setfield(L, -2, "keyscans");
	lua_pushinteger(L, costs.iKeyScannersMax); lua_setfield(L, -2, "keymax");
	lua_pushinteger(L, costs.iGC); lua_setfield(L, -2, "gc");
	lua_createtable(L, costs.iTop, 0);
	for (int i = 0; i < costs.iTop; i++)
	{
		const sLuaEntityCostTop& entity = costs.top[i];
		const bool bValid = entity.e > 0 && entity.e < (int)t.entityelement.size();
		lua_createtable(L, 0, 9);
		lua_pushinteger(L, entity.e); lua_setfield(L, -2, "e");
		lua_pushnumber(L, entity.dTotal); lua_setfield(L, -2, "ms");
		lua_pushnumber(L, entity.dRow); lua_setfield(L, -2, "row");
		lua_pushnumber(L, entity.dMain); lua_setfield(L, -2, "main");
		lua_pushnumber(L, entity.dKey); lua_setfield(L, -2, "key");
		lua_pushinteger(L, entity.iGC); lua_setfield(L, -2, "gc");
		lua_pushstring(L, bValid ? t.entityelement[entity.e].eleprof.aimainname_s.Get() : ""); lua_setfield(L, -2, "script");
		lua_pushinteger(L, bValid ? t.entityelement[entity.e].eleprof.phyalways : 0); lua_setfield(L, -2, "phyalways");
		lua_pushnumber(L, bValid ? t.entityelement[entity.e].plrdist : 0); lua_setfield(L, -2, "plrdist");
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "top");
	return 1;
}

// GetObjectMass(obj) - the kilos its physics body has now; 0 for no body or a static one
int GetObjectMass(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	extern float ODEGetObjectMassKg(int iObject);
	lua_pushnumber(L, ODEGetObjectMassKg((int)lua_tonumber(L, 1)));
	return 1;
}

int GetPhysicsAwakeBodies(lua_State* L)
{
	int iMax = 16;
	if (LUA_GETTOP(L) >= 1) iMax = (int)lua_tointeger(L, 1);
	if (iMax < 1) iMax = 1;
	if (iMax > 64) iMax = 64;
	PhysicsAwakeBody bodies[64];
	int iTotal = 0, iTouchingNothing = 0;
	int iCount = PhysicsQuery_AwakeBodies(bodies, iMax, &iTotal, &iTouchingNothing);
	lua_createtable(L, iCount, 0);
	for (int i = 0; i < iCount; i++)
	{
		const PhysicsAwakeBody& body = bodies[i];
		int iEntity = -1;
		if (body.object > 0)
		{
			for (int e = 1; e <= g.entityelementlist; e++)
			{
				if (t.entityelement[e].obj == body.object) { iEntity = e; break; }
			}
		}
		const char* pState = "active";
		if (body.state == 2) pState = "asleep";
		else if (body.state == 3) pState = "ready";
		else if (body.state == 4) pState = "always";
		else if (body.state == 5) pState = "off";
		lua_createtable(L, 0, 19);
		lua_pushinteger(L, body.object); lua_setfield(L, -2, "obj");
		lua_pushinteger(L, iEntity); lua_setfield(L, -2, "e");
		lua_pushboolean(L, (body.object > 0 && ObjectExist(body.object) == 1) ? 1 : 0); lua_setfield(L, -2, "exists");
		lua_pushboolean(L, body.ragdoll); lua_setfield(L, -2, "ragdoll");
		lua_pushboolean(L, body.listed); lua_setfield(L, -2, "listed");
		lua_pushstring(L, body.shape ? body.shape : ""); lua_setfield(L, -2, "shape");
		lua_pushnumber(L, body.mass); lua_setfield(L, -2, "mass");
		lua_pushnumber(L, body.pos[0]); lua_setfield(L, -2, "x");
		lua_pushnumber(L, body.pos[1]); lua_setfield(L, -2, "y");
		lua_pushnumber(L, body.pos[2]); lua_setfield(L, -2, "z");
		lua_pushnumber(L, body.speed); lua_setfield(L, -2, "speed");
		lua_pushnumber(L, body.spin); lua_setfield(L, -2, "spin");
		lua_pushstring(L, pState); lua_setfield(L, -2, "state");
		lua_pushnumber(L, body.still); lua_setfield(L, -2, "still");
		lua_pushinteger(L, body.island); lua_setfield(L, -2, "island");
		lua_pushinteger(L, body.points); lua_setfield(L, -2, "points");
		lua_pushinteger(L, body.layer); lua_setfield(L, -2, "layer");
		lua_pushinteger(L, body.mask); lua_setfield(L, -2, "mask");
		lua_rawseti(L, -2, i + 1);
	}
	lua_pushinteger(L, iTotal);
	lua_pushinteger(L, iTouchingNothing);
	return 3;
}

// GetPhysicsBodyPose(obj): where an object's physics body is, as the pose it would give the object: x, y, z and angles
// x, y, z in degrees (as GetEntityAngleX/Y/Z), then bodies, how many bodies the physics holds for the object (more than
// 1 is one left behind; the pose is the first's, the one CollisionOff removes). A static body never moves its object, so
// this shows the collider against the drawn object. With no body: nil for the pose, and 0
int GetPhysicsBodyPose(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	float fPos[3], fAng[3];
	int iBodies = ODEGetBodyPose((int)lua_tointeger(L, 1), fPos, fAng);
	for (int i = 0; i < 3; i++) { if (iBodies > 0) lua_pushnumber(L, fPos[i]); else lua_pushnil(L); }
	for (int i = 0; i < 3; i++) { if (iBodies > 0) lua_pushnumber(L, fAng[i]); else lua_pushnil(L); }
	lua_pushinteger(L, iBodies);
	return 7;
}

int GetPhysicsPushLog(lua_State* L)
{
	extern void ODEGetPushLog(std::vector<std::string>* pLines);
	std::vector<std::string> lines;
	ODEGetPushLog(&lines);
	lua_createtable(L, (int)lines.size(), 0);
	for (int i = 0; i < (int)lines.size(); i++)
	{
		lua_pushstring(L, lines[i].c_str());
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

// GetPhysicsSlowPairs([n]): a diagnostic, the body pairs whose collision tests took longest in the physics' last update,
// slowest first, up to n (5 by default, at most 16): a table of { ms (their tests' time over the update's steps), calls
// (how many tests), obj1, e1, shape1, obj2, e2, shape2 } (obj 0 the ground, -1 unknown; e the entity holding the object,
// -1 none; shape with its size, e.g. "compound 80 children, 1408 hull points", "mesh 12000 triangles", "terrain"). The
// tests are timed for 10 s after each call, so the first call after a while gives an empty table; only tests over about
// 0.01 ms are kept
int GetPhysicsSlowPairs(lua_State* L)
{
	int iMax = 5;
	if (LUA_GETTOP(L) >= 1) iMax = (int)lua_tointeger(L, 1);
	if (iMax < 1) iMax = 1;
	if (iMax > 16) iMax = 16;
	PhysicsSlowPair pairs[16];
	int iCount = PhysicsQuery_SlowPairs(pairs, iMax);
	lua_createtable(L, iCount, 0);
	for (int i = 0; i < iCount; i++)
	{
		const PhysicsSlowPair& pair = pairs[i];
		lua_createtable(L, 0, 8);
		lua_pushnumber(L, pair.ms); lua_setfield(L, -2, "ms");
		lua_pushinteger(L, pair.calls); lua_setfield(L, -2, "calls");
		for (int b = 0; b < 2; b++)
		{
			int iEntity = -1;
			if (pair.object[b] > 0)
			{
				for (int e = 1; e <= g.entityelementlist; e++)
				{
					if (t.entityelement[e].obj == pair.object[b]) { iEntity = e; break; }
				}
			}
			lua_pushinteger(L, pair.object[b]); lua_setfield(L, -2, b == 0 ? "obj1" : "obj2");
			lua_pushinteger(L, iEntity); lua_setfield(L, -2, b == 0 ? "e1" : "e2");
			lua_pushstring(L, pair.shape[b]); lua_setfield(L, -2, b == 0 ? "shape1" : "shape2");
		}
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

// GetPhysicsStats(): what the physics did in its last update, to find what a costly physics frame is working on. Returns
// subSteps (simulation steps taken, at most 7), awake (bodies moving or settling), manifolds (body pairs in contact),
// points (their contact points), then busiestObject and busiestPoints: the object with the most contact points, the
// ground and character capsules left out (-1 none), and its points
int GetPhysicsStats(lua_State* L)
{
	PhysicsStats stats;
	PhysicsQuery_Stats(&stats);
	lua_pushinteger(L, stats.subSteps);
	lua_pushinteger(L, stats.awakeBodies);
	lua_pushinteger(L, stats.manifolds);
	lua_pushinteger(L, stats.contactPoints);
	lua_pushinteger(L, stats.busiestObject);
	lua_pushinteger(L, stats.busiestPoints);
	return 6;
}

int GetPhysicsStatsTop(lua_State* L)
{
	int iMax = 5;
	if (LUA_GETTOP(L) >= 1) iMax = (int)lua_tointeger(L, 1);
	if (iMax < 1) iMax = 1;
	if (iMax > 32) iMax = 32;
	PhysicsStatsBody bodies[32];
	int iCount = PhysicsQuery_StatsTop(bodies, iMax);
	lua_createtable(L, iCount, 0);
	for (int i = 0; i < iCount; i++)
	{
		const PhysicsStatsBody& body = bodies[i];
		int iEntity = -1;
		if (body.object > 0)
		{
			for (int e = 1; e <= g.entityelementlist; e++)
			{
				if (t.entityelement[e].obj == body.object) { iEntity = e; break; }
			}
		}
		const char* pKind = "other";
		if (LuaPhysicsIsTreeCylinder(body.object)) pKind = "tree";
		else if (body.layer & PHYSICS_LAYER_DYNAMIC) pKind = "dynamic";
		else if (body.layer & PHYSICS_LAYER_STATIC) pKind = "static";
		const char* pState = "active";
		if (body.state == 2) pState = "asleep";
		else if (body.state == 3) pState = "ready";
		else if (body.state == 4) pState = "always";
		else if (body.state == 5) pState = "off";
		lua_createtable(L, 0, 12);
		lua_pushinteger(L, iEntity); lua_setfield(L, -2, "e");
		lua_pushinteger(L, body.object); lua_setfield(L, -2, "obj");
		lua_pushinteger(L, body.points); lua_setfield(L, -2, "points");
		lua_pushboolean(L, body.awake ? 1 : 0); lua_setfield(L, -2, "awake");
		lua_pushnumber(L, body.speed); lua_setfield(L, -2, "speed");
		lua_pushstring(L, pKind); lua_setfield(L, -2, "kind");
		lua_pushnumber(L, body.spin); lua_setfield(L, -2, "spin");
		lua_pushstring(L, pState); lua_setfield(L, -2, "state");
		lua_pushnumber(L, body.still); lua_setfield(L, -2, "still");
		lua_pushnumber(L, body.sleepSpeed); lua_setfield(L, -2, "sleepspeed");
		lua_pushnumber(L, body.sleepSpin); lua_setfield(L, -2, "sleepspin");
		lua_pushinteger(L, body.island); lua_setfield(L, -2, "island");
		lua_rawseti(L, -2, i + 1);
	}
	return 1;
}

// GetPhysicsWorldRate() - 60 or 120
int GetPhysicsWorldRate(lua_State* L)
{
	extern bool g_bPhysicsWorldAt60;
	lua_pushinteger(L, g_bPhysicsWorldAt60 ? 60 : 120);
	return 1;
}

// GetPlayerHit(seq): one entry of the player's hit record (G-Entity.cpp playerhit_*), nothing if not written yet or
// older than the 128 kept. Returns hit (0 none, 1 terrain, 2 entity, 3 other object, 4 passed through, 5 tree), e, x, y, z,
// nx, ny, nz, material, hole (the same first ten as BulletRay), then limb, damage, health before, health after,
// killed, kind (1 round, 2 a later pellet of the pull, 3 melee, 4 a player projectile's hit or blast), shot, gunid,
// and ox, oy, oz where the ray started. Reading does not remove the entry
int GetPlayerHit ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	sPlayerHit* pHit = playerhit_get ( (int)lua_tointeger(L, 1) );
	if ( !pHit ) return 0;
	lua_pushinteger(L, pHit->iHit);
	lua_pushinteger(L, pHit->e);
	lua_pushnumber(L, pHit->fX);
	lua_pushnumber(L, pHit->fY);
	lua_pushnumber(L, pHit->fZ);
	lua_pushnumber(L, pHit->fNX);
	lua_pushnumber(L, pHit->fNY);
	lua_pushnumber(L, pHit->fNZ);
	lua_pushinteger(L, pHit->iMaterial);
	lua_pushinteger(L, pHit->iHole);
	lua_pushinteger(L, pHit->iLimb);
	lua_pushinteger(L, pHit->iDamage);
	lua_pushinteger(L, pHit->iHealthBefore);
	lua_pushinteger(L, pHit->iHealthAfter);
	lua_pushinteger(L, pHit->iKilled);
	lua_pushinteger(L, pHit->iKind);
	lua_pushinteger(L, pHit->iShot);
	lua_pushinteger(L, pHit->iGunID);
	lua_pushnumber(L, pHit->fOX);
	lua_pushnumber(L, pHit->fOY);
	lua_pushnumber(L, pHit->fOZ);
	return 21;
}

// GetPlayerHitSeq(): the newest entry in the player's hit record, 0 if none this level. Entries never repeat a number
int GetPlayerHitSeq ( lua_State *L )
{
	lua_pushinteger(L, playerhit_getseq());
	return 1;
}

int GetRagdollAllDeaths ( lua_State *L )
{
	extern bool g_bRagdollAllDeaths;
	lua_pushinteger ( L, g_bRagdollAllDeaths ? 1 : 0 );
	return 1;
}

// GetRayNormalX/Y/Z(): the surface normal where the last RayTerrain hit, the ground or a tree trunk
int GetRayNormalX ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayNormalX() );
	return 1;
}

int GetRayNormalY ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayNormalY() );
	return 1;
}

int GetRayNormalZ ( lua_State *L )
{
	lua_pushnumber ( L, ODEGetRayNormalZ() );
	return 1;
}

int GetTextHeight ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iSize = lua_tonumber(L, 1);
	float fHeight = 0.0f;
	if ( GetDisplayHeight() > 0 ) fHeight = (getbitmapfontheight(iSize) * 100.0f) / GetDisplayHeight();
	lua_pushnumber ( L, fHeight );
	return 1;
}

// GetTextWidth(size,text) and GetTextHeight(size) measure Text() output in the same screen percent units Text() is positioned in
int GetTextWidth ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int iSize = lua_tonumber(L, 1);
	const char* pText = lua_tostring(L, 2);
	float fWidth = 0.0f;
	if ( pText && GetDisplayWidth() > 0 ) fWidth = (getbitmapfontwidth((char*)pText, iSize) * 100.0f) / GetDisplayWidth();
	lua_pushnumber ( L, fWidth );
	return 1;
}

int GetTreeDistance(lua_State* L)
{
	lua_pushnumber(L, GGTrees::ggtrees_global_params.lod_dist);
	lua_pushnumber(L, GGTrees::ggtrees_global_params.lod_dist_shadow);
	return 2;
}

int GetTreeSubsurface(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.tree_sss);
	return 1;
}

// GetTreeTypeName(type): the name of a tree type (GetTreesNear's type, PhysicsRay's treetype), such as "jungletree3a"; ""
// for none
int GetTreeTypeName(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	int iType = (int)lua_tointeger(L, 1);
	char pName[64] = "";
	if (iType >= 0) GGTrees::GGTrees_GetTypeName((uint32_t)iType, pName, sizeof(pName));
	lua_pushstring(L, pName);
	return 1;
}

// GetTreesNear(x, z, radius [, max]): the tree trunks within radius of x, z across (at any height), nearest first, as a
// table of { x, y, z, radius, type, name, top, crown, scale }: each trunk's base and its radius, as the trees' collision has it
// (by species and scale), its tree type and that type's name ("jungletree3a"), the tree's top, crown (where its crown starts:
// the lowest branch or leaf of its species' full detail mesh at its scale, the top for a species with none; below it only the
// bare trunk) and its own scale.
// At most max (64 unless set). Trees not drawn are left out: hidden under a building, removed by a blast, or all of them
// when trees are off. For a script's own collision test, such as a car driven by Lua
int GetTreesNear(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 3) return 0;
	float fX = lua_tonumber(L, 1);
	float fZ = lua_tonumber(L, 2);
	float fRadius = lua_tonumber(L, 3);
	int iMax = (n >= 4) ? lua_tointeger(L, 4) : 64;
	GGTrees::GGTreePoint* pPoints = NULL;
	int iCount = 0;
	if (GGTrees::ggtrees_global_params.draw_enabled && fRadius > 0) iCount = GGTrees::GGTrees_GetClosest(fX, fZ, fRadius, &pPoints);

	// the nearest first
	std::vector<std::pair<float, int>> nearest;
	for (int i = 0; i < iCount; i++)
	{
		float fDX = pPoints[i].x - fX;
		float fDZ = pPoints[i].z - fZ;
		std::pair<float, int> tree(fDX * fDX + fDZ * fDZ, i);
		size_t k = nearest.size();
		nearest.push_back(tree);
		while (k > 0 && nearest[k - 1].first > tree.first) { nearest[k] = nearest[k - 1]; k--; }
		nearest[k] = tree;
	}

	int iTake = (int)nearest.size() < iMax ? (int)nearest.size() : iMax;
	if (iTake < 0) iTake = 0;
	lua_createtable(L, iTake, 0);
	for (int k = 0; k < iTake; k++)
	{
		GGTrees::GGTreePoint& tree = pPoints[nearest[k].second];
		lua_createtable(L, 0, 9);
		lua_pushnumber(L, tree.x); lua_setfield(L, -2, "x");
		lua_pushnumber(L, tree.y); lua_setfield(L, -2, "y");
		lua_pushnumber(L, tree.z); lua_setfield(L, -2, "z");
		lua_pushnumber(L, tree.scale * 0.5f); lua_setfield(L, -2, "radius");
		lua_pushinteger(L, tree.type); lua_setfield(L, -2, "type");
		char pName[64];
		GGTrees::GGTrees_GetTypeName(tree.type, pName, sizeof(pName));
		lua_pushstring(L, pName); lua_setfield(L, -2, "name");
		lua_pushnumber(L, tree.top); lua_setfield(L, -2, "top");
		lua_pushnumber(L, tree.crown); lua_setfield(L, -2, "crown");
		lua_pushnumber(L, tree.instanceScale); lua_setfield(L, -2, "scale");
		lua_rawseti(L, -2, k + 1);
	}
	if (pPoints) delete[] pPoints;
	return 1;
}

int GetWind(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.wind_speed);
	lua_pushnumber(L, t.gamevisuals.wind_direction_x);
	lua_pushnumber(L, t.gamevisuals.wind_direction_y);
	lua_pushnumber(L, t.gamevisuals.wind_direction_z);
	lua_pushnumber(L, t.gamevisuals.wind_randomness);
	return 5;
}

// IntersectRay(x1, y1, z1, x2, y2, z2 [, ignore [, flags]]): one full-accuracy pick with all its results. ignore is an
// object number or a table of them. flags add up: 1 also hits terrain and tree trunks, 2 passes through
// entities whose collisionmode is 11, as the player's bullets do. Returns obj (0 none, -1 terrain or other geometry),
// e (0 if not an entity), x, y, z, nx, ny, nz (the end of the ray and 0,0,0 when nothing was hit), limb (-1 none)
int IntersectRay ( lua_State* L )
{
	int n = LUA_GETTOP(L);
	if ( n < 6 ) return 0;
	float fX = lua_tonumber(L, 1), fY = lua_tonumber(L, 2), fZ = lua_tonumber(L, 3);
	float fEndX = lua_tonumber(L, 4), fEndY = lua_tonumber(L, 5), fEndZ = lua_tonumber(L, 6);
	std::vector<int> ignore;
	if ( n >= 7 ) LuaReadIntersectIgnore(L, 7, ignore);
	int iFlags = 0;
	if ( n >= 8 ) iFlags = lua_tointeger(L, 8);

	int iHitObj = 0, iHitE = 0, iLimb = -1;
	float fHitX = fEndX, fHitY = fEndY, fHitZ = fEndZ, fNX = 0, fNY = 0, fNZ = 0;

	// the ground first, which shortens the ray
	if ( (iFlags & 1) && ODERayTerrain(fX, fY, fZ, fEndX, fEndY, fEndZ, false) == 1 )
	{
		fEndX = fHitX = ODEGetRayCollisionX(); fEndY = fHitY = ODEGetRayCollisionY(); fEndZ = fHitZ = ODEGetRayCollisionZ();
		fNX = ODEGetRayNormalX(); fNY = ODEGetRayNormalY(); fNZ = ODEGetRayNormalZ();
		iHitObj = -1;
	}

	// then the objects up to it, passing collisionmode 11 entities if asked (a few at most)
	extern std::vector<int> g_IntersectExtraIgnoreObjects;
	for ( int iTry = 0; iTry < 8; iTry++ )
	{
		g_IntersectExtraIgnoreObjects.clear();
		for (int i = 1; i < (int)ignore.size(); i++) g_IntersectExtraIgnoreObjects.push_back(ignore[i]);
		int iIgnoreObjNo = ignore.size() > 0 ? ignore[0] : 0;
		int iObj = IntersectAllEx(g.entityviewstartobj, g.entityviewendobj, fX, fY, fZ, fEndX, fEndY, fEndZ, iIgnoreObjNo, 0, 0, 0, 0, true);
		g_IntersectExtraIgnoreObjects.clear();
		if ( iObj == 0 ) break;
		int iObjE = 0;
		if ( iObj > 0 )
		{
			for ( int e = 1; e <= g.entityelementlist; e++ )
			{
				if ( t.entityelement[e].obj == iObj ) { iObjE = e; break; }
			}
			if ( (iFlags & 2) && iObjE > 0 && t.entityprofile[t.entityelement[iObjE].bankindex].collisionmode == 11 )
			{
				ignore.push_back(iObj);
				continue;
			}
		}
		iHitObj = iObj;
		iHitE = iObjE;
		fHitX = ChecklistFValueA(6); fHitY = ChecklistFValueB(6); fHitZ = ChecklistFValueC(6);
		fNX = ChecklistFValueA(7); fNY = ChecklistFValueB(7); fNZ = ChecklistFValueC(7);
		iLimb = ( iObj > 0 ) ? ChecklistValueB(1) : -1;
		break;
	}

	lua_pushinteger(L, iHitObj);
	lua_pushinteger(L, iHitE);
	lua_pushnumber(L, fHitX);
	lua_pushnumber(L, fHitY);
	lua_pushnumber(L, fHitZ);
	lua_pushnumber(L, fNX);
	lua_pushnumber(L, fNY);
	lua_pushnumber(L, fNZ);
	lua_pushinteger(L, iLimb);
	return 9;
}

// PhysicsOverlapBox(cx, cy, cz, hx, hy, hz, yaw [, layers [, ignore [, trees]]]): what a box (as PhysicsSweepBox's) touches
// now, such as a car wedged against something. Returns count, trees, then the object numbers of the count bodies it touches
// (up to 32; 0 the ground, -1 a character capsule), trees being how many tree trunks it touches (with trees 1, the default;
// with trees 2 only their bare parts below the crowns)
int PhysicsOverlapBox(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 7) return 0;
	float fCentre[3] = { (float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3) };
	float fHalf[3] = { (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6) };
	float fYaw = (float)lua_tonumber(L, 7);
	int iLayers = (n >= 8) ? (int)lua_tointeger(L, 8) : PHYSICS_LAYER_DEFAULT;
	int pIgnore[PHYSICS_QUERY_MAX_IGNORE];
	int iIgnoreCount = LuaPhysicsIgnoreList(L, 9, pIgnore);
	int iTreesMode = (n < 10) ? 1 : (int)lua_tointeger(L, 10);
	int pObjects[32];
	int iFound = PhysicsQuery_OverlapBox(fCentre, fHalf, fYaw, iLayers, pIgnore, iIgnoreCount, pObjects, 32);

	// the collision cylinders at the trees round the camera are trunks, counted with the map-wide ones
	int iCount = 0;
	for (int i = 0; i < iFound; i++)
	{
		if (!LuaPhysicsIsTreeCylinder(pObjects[i])) pObjects[iCount++] = pObjects[i];
	}
	int iTrees = (iTreesMode != 0) ? GGTrees::GGTrees_OverlapBoxTrunks(fCentre, fHalf, fYaw, iTreesMode == 2) : 0;
	lua_pushinteger(L, iCount);
	lua_pushinteger(L, iTrees);
	for (int i = 0; i < iCount; i++) lua_pushinteger(L, pObjects[i]);
	return 2 + iCount;
}

// PhysicsRay(x1, y1, z1, x2, y2, z2 [, layers [, ignore]]): the nearest body the segment meets in the physics world, with no
// force applied (PhysicsRayCast pushes what it hits and returns moving bodies only). layers add up: 1 the ground (and
// entities built as terrain colliders), 2 static entities, 4 characters, 8 moving bodies (11 unless set); ignore is an
// object number or a table of up to 8. Returns hit, x, y, z, nx, ny, nz, obj (0 the ground or a tree, -1 a character
// capsule), fraction, tree (1 for a trunk), treetype (the trunk's tree type, GetTreeTypeName, -1 if none) and surface (1 a
// surface built from a spline, a road's pavement, which is ground with obj 0; else 0). The physics world has the ground only as detailed as the terrain round the
// camera, no body for collisionmode 11 or 12, and trunks only round the camera (RayTrees has them everywhere)
int PhysicsRay(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 6) return 0;
	float fFrom[3] = { (float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3) };
	float fTo[3] = { (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6) };
	int iLayers = (n >= 7) ? (int)lua_tointeger(L, 7) : PHYSICS_LAYER_DEFAULT;
	int pIgnore[PHYSICS_QUERY_MAX_IGNORE];
	int iIgnoreCount = LuaPhysicsIgnoreList(L, 8, pIgnore);
	PhysicsHit hit;
	PhysicsQuery_Ray(fFrom, fTo, iLayers, pIgnore, iIgnoreCount, &hit);
	return LuaPushPhysicsHit(L, hit, false, -1);
}

// PhysicsSweepBox(cx, cy, cz, hx, hy, hz, yaw, mx, my, mz [, layers [, ignore [, trees]]]): the first thing a box meets as it
// moves by mx, my, mz: the box centred on cx, cy, cz, reaching hx, hy, hz each way along its own axes and turned by yaw
// degrees about Y as an object is (GetObjectColBox gives a hull's). It meets the physics world as PhysicsRay does, and
// with trees 1 (the default) every drawn tree trunk on the map too, base to top; with trees 2 only each trunk's bare part,
// from its base up to its crown (GetTreesNear's crown), so a pass through a canopy is no hit. No gap between samples and no tunnelling, and the
// normal is the real contact's. Returns what PhysicsRay does, fraction being how far along the motion; 0 means the box
// already touched something, so lift a car's box clear of the ground or leave the ground out of layers
int PhysicsSweepBox(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 10) return 0;
	float fCentre[3] = { (float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3) };
	float fHalf[3] = { (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5), (float)lua_tonumber(L, 6) };
	float fYaw = (float)lua_tonumber(L, 7);
	float fMotion[3] = { (float)lua_tonumber(L, 8), (float)lua_tonumber(L, 9), (float)lua_tonumber(L, 10) };
	int iLayers = (n >= 11) ? (int)lua_tointeger(L, 11) : PHYSICS_LAYER_DEFAULT;
	int pIgnore[PHYSICS_QUERY_MAX_IGNORE];
	int iIgnoreCount = LuaPhysicsIgnoreList(L, 12, pIgnore);
	int iTrees = (n < 13) ? 1 : (int)lua_tointeger(L, 13);
	bool bBareTrunk = (iTrees == 2);
	PhysicsHit hit;
	PhysicsQuery_SweepBox(fCentre, fHalf, fYaw, fMotion, iLayers, pIgnore, iIgnoreCount, &hit);

	// the collision cylinders at the trees round the camera stand the tree's full height, so bare trunks sweep on past them
	// and take those trees from the map-wide trunks below
	while (bBareTrunk && hit.hit && LuaPhysicsIsTreeCylinder(hit.object) && iIgnoreCount < PHYSICS_QUERY_MAX_IGNORE)
	{
		pIgnore[iIgnoreCount++] = hit.object;
		PhysicsQuery_SweepBox(fCentre, fHalf, fYaw, fMotion, iLayers, pIgnore, iIgnoreCount, &hit);
	}
	if (bBareTrunk && hit.hit && LuaPhysicsIsTreeCylinder(hit.object)) hit.hit = false;

	// the map-wide trunks, where nearer
	bool bTree = false;
	float fTrunk[7];
	int iTreeType = -1;
	if (iTrees != 0 && GGTrees::GGTrees_SweepBoxTrunks(fCentre, fHalf, fYaw, fMotion, fTrunk, &iTreeType, bBareTrunk) && (!hit.hit || fTrunk[0] < hit.fraction))
	{
		hit.hit = true;
		hit.fraction = fTrunk[0];
		for (int i = 0; i < 3; i++) { hit.point[i] = fTrunk[1 + i]; hit.normal[i] = fTrunk[4 + i]; }
		hit.object = 0;
		bTree = true;
	}
	return LuaPushPhysicsHit(L, hit, bTree, iTreeType);
}

// RayTrees(x1, y1, z1, x2, y2, z2): the nearest tree trunk the segment meets, anywhere on the map (the physics sees only
// those round the camera). Returns 1 and the point and the trunk's normal there, or 0 and zeros. A trunk is as thick as
// the trees' physics collision (by species and scale) and reaches from the ground to the tree's top; canopies don't
// count, nor trees hidden under a building or removed by a blast. Only the tree chunks along the segment are walked
int RayTrees(lua_State* L)
{
	if (LUA_GETTOP(L) < 6) return 0;
	float fOut[6] = { 0, 0, 0, 0, 0, 0 };
	int iHit = GGTrees::GGTrees_RayCastTrunks(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), fOut);
	lua_pushinteger(L, iHit);
	for (int i = 0; i < 6; i++) lua_pushnumber(L, fOut[i]);
	return 7;
}

// ResetDelayedShadows(): forget what SetDelayedShadows set and go back to the level's or the graphics quality's choice
int ResetDelayedShadows(lua_State* L)
{
	LuaRenderSettings_ResetDelayedShadows();
	return 0;
}

// ResetEntityMaterial(e): puts back the entity's materials as the level load gave them, mesh by mesh (its .fpe's, or its
// element's own): textures, base and emissive colour, emissive strength, roughness, metalness, reflectance, normal
// strength, alpha and transparency. Like the material setters it changes every entity of the model, as they share their
// materials; the per-instance values (SetEntityInstanceEmissive, the tint, limb alpha) are left alone
int ResetEntityMaterial (lua_State *L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	int iEntityID = lua_tointeger(L, 1);
	if (!LuaEntityIDValid(L, iEntityID, 1)) return 0;
	entity_resetmaterial(iEntityID);
	return 0;
}

// ResetGraphicsSettings(): forget every value the graphics setters above set and go back to the level's and the graphics
// quality's, so a quality change moves them again (a menu's Default)
int ResetGraphicsSettings(lua_State* L)
{
	LuaRenderSettings_ResetGraphics();
	return 0;
}

// SetAO(on [, power]): ambient occlusion (corners and gaps take less ambient light) on (1) or off (0), and its power, 0
// to 5 (1 is the editor's default)
int SetAO(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetAO((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2));
	return 0;
}

// SetAnimations30Fps(on): far characters animate at 30 frames a second
int SetAnimations30Fps(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetAnimations30Fps((int)lua_tonumber(L, 1));
	return 0;
}

int SetBloom(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetBloom((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2), LuaOptionalNumber(L, n, 3), LuaOptionalNumber(L, n, 4), LuaOptionalNumber(L, n, 5));
	return 0;
}

// SetDelayedShadows(on [, laptop]): the sun's shadow cascades 1 to 4 are redrawn every 2, 3, 4 and 9 frames instead of
// every frame, and point light shadows less often too; laptop also redraws cascade 0 every other frame and the rest every
// 3, 4, 5 and 9. On unless the level turned it off
int SetDelayedShadows(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetDelayedShadows((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2));
	return 0;
}

int SetDepthOfField(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetDepthOfField((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2), LuaOptionalNumber(L, n, 3), LuaOptionalNumber(L, n, 4));
	return 0;
}

int SetEntityAngularVelocity(lua_State *L) { return SetEntityVelocityCore ( L, 1 ); }

// SetEntityInstanceEmissive(e, r, g, b [, strength]): per instance multiplier, 0 to 1, on the entity's emissive colour
// and strength as authored (1,1,1,1 = unchanged, strength 0 = no glow). Unlike the material setters it changes this
// instance only, not every instance of the same object
int SetEntityInstanceEmissive ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int e = lua_tointeger(L, 1);
	if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;
	float fStrength = 1.0f;
	if ( n >= 5 ) fStrength = lua_tonumber(L, 5);
	int iObj = t.entityelement[e].obj;
	if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
		WickedCall_SetObjectEmissiveTint ( GetObjectData ( iObj ), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), fStrength );
	return 0;
}

// SetEntityInstanceTint(e, r, g, b): per instance multiplier, 0 to 1, on the entity's base colour (1,1,1 = unchanged), e.g.
// to darken a wreck or colour one vehicle. Unlike the material setters it changes this instance only
int SetEntityInstanceTint ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 4 ) return 0;
	int e = lua_tointeger(L, 1);
	if ( !LuaEntityIDValid ( L, e, 1 ) ) return 0;
	int iObj = t.entityelement[e].obj;
	if ( iObj > 0 && ObjectExist ( iObj ) == 1 )
		WickedCall_SetObjectColorTint ( GetObjectData ( iObj ), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4) );
	return 0;
}

// SetEntityLimbAlpha(e, limb, percent [, children]): fades the limb, 0 to 100 (100 = as made), with children 1 also the limbs
// under it (a rotor frame with blade limbs). The renderer dithers the fade rather than blending it, and a faded limb keeps
// its shadow, so hide a limb that has faded out (HideLimb). A skinned mesh fades as its own limb, not through its bones
int SetEntityLimbAlpha ( lua_State* L )
{
	 int n = LUA_GETTOP(L);
	 if ( n < 3 ) return 0;
	 int iLimb = lua_tointeger(L, 2);
	 bool bChildren = ( n >= 4 && lua_tointeger(L, 4) != 0 );
	 sObject* pObject = LuaEntityLimbObject ( L, lua_tointeger(L, 1), iLimb );
	 if ( !pObject ) return 0;
	 WickedCall_SetLimbAlpha ( pObject, iLimb, lua_tonumber(L, 3) / 100.0f, bChildren );
	 return 0;
}

// SetEntityRagdoll(e, 1 [, dx, dy, dz, force]): a living character (ragdoll = 1 in its .fpe) becomes a ragdoll but keeps its
// health and script, pushed along dx,dy,dz by force (as SetEntityRagdollForce's); its entity position follows the pelvis.
// SetEntityRagdoll(e, 0 [, angle]): he stands where his body lies, facing angle or the way he gets up (face up, toward
// his feet; face down, toward his head), a character again; play his get-up animation then. Returns 1 when done
int SetEntityRagdoll ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int e = lua_tointeger(L, 1);
	int iDone = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		if ( lua_tointeger(L, 2) != 0 )
		{
			float fPushX = n >= 6 ? lua_tonumber(L, 3) : 0.0f;
			float fPushY = n >= 6 ? lua_tonumber(L, 4) : 0.0f;
			float fPushZ = n >= 6 ? lua_tonumber(L, 5) : 0.0f;
			float fPushForce = n >= 6 ? lua_tonumber(L, 6) : 0.0f;
			iDone = ragdoll_knockout ( e, fPushX, fPushY, fPushZ, fPushForce );
		}
		else
		{
			iDone = ragdoll_wake ( e, n >= 3, n >= 3 ? lua_tonumber(L, 3) : 0.0f );
		}
	}
	lua_pushinteger ( L, iDone );
	return 1;
}

// SetEntityRagdollHold(e, x, y, z, ax, ay, az [, limbname]): holds a ragdoll's pelvis (or the body moving limbname) at
// x,y,z, turned as the character would be: 0,a,0 as standing facing a; 90,a,0 face down, head toward a; -90,a,0 face
// up, feet toward a. The rest hangs from it. Call each frame while held; it is brought there over 0.25 s.
// SetEntityRagdollHold(e) lets go. Returns 1 while held
int SetEntityRagdollHold ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	int iHeld = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		if ( n >= 7 )
		{
			const char* pLimbName = n >= 8 ? lua_tostring(L, 8) : NULL;
			iHeld = ragdoll_hold ( e, lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4), lua_tonumber(L, 5), lua_tonumber(L, 6), lua_tonumber(L, 7), pLimbName );
		}
		else
		{
			ragdoll_release ( e );
		}
	}
	lua_pushinteger ( L, iHeld );
	return 1;
}

// SetEntityRagdollHoldReference(e [, clip [, frame]]): e's ragdoll while held is draped from the pose of e's own
// animation clip (a name as SetAnimationName takes) at frame (0 its first frame, 1 its last; 0), each body as its limbs
// are there, not the pose it was made in; the stiffness's folds, knee fold (-1 keeps the reference's knees) and the
// joint limits then apply as before. With no clip, the pose it was made in again. Set again after each
// SetEntityRagdoll(e, 1). 1 when set, 0 when e is no ragdoll or has no such clip
int SetEntityRagdollHoldReference ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int e = lua_tointeger(L, 1);
	int iDone = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) )
	{
		const char* pClip = ( n >= 2 && lua_isstring(L, 2) ) ? lua_tostring(L, 2) : "";
		float fFraction = ( n >= 3 ) ? lua_tonumber(L, 3) : 0.0f;
		iDone = ragdoll_setholdreference ( e, pClip, fFraction );
	}
	lua_pushinteger ( L, iDone );
	return 1;
}

// SetEntityRagdollHoldStiffness(e, stiffness [, damping [, waistfold [, hipfold [, kneefold]]]]): for e's ragdoll while
// held, how hard each body is pulled to a drape (0 hangs free, 1 rigid; 0.85 when made), its swing's damping (0 to 1;
// 0.6), and the drape's folds in degrees at the held body's joints: the part above toward his chest (60) and the parts
// below (60); and the bend of each knee beyond, from the thigh toward the back of it (0 straight, 90 a right angle,
// kept within the knee's limits), the same however the ragdoll was made; negative leaves the knees as made (-1). Left
// out keeps the value. Set again after each SetEntityRagdoll(e, 1), as a new ragdoll starts with these. 1 when set
int SetEntityRagdollHoldStiffness ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 2 ) return 0;
	int e = lua_tointeger(L, 1);
	int iDone = 0;
	if ( LuaEntityIDValid ( L, e, 1 ) && t.entityelement[e].obj > 0 )
	{
		int iObj = t.entityelement[e].obj;
		float fStiffness, fDamping, fWaistFold, fHipFold, fKneeFold;
		if ( BPhys_RagDollGetHoldStiffness ( iObj, &fStiffness, &fDamping, &fWaistFold, &fHipFold, &fKneeFold ) )
		{
			fStiffness = lua_tonumber(L, 2);
			if ( n >= 3 ) fDamping = lua_tonumber(L, 3);
			if ( n >= 4 ) fWaistFold = lua_tonumber(L, 4);
			if ( n >= 5 ) fHipFold = lua_tonumber(L, 5);
			if ( n >= 6 ) fKneeFold = lua_tonumber(L, 6);
			iDone = BPhys_RagDollSetHoldStiffness ( iObj, fStiffness, fDamping, fWaistFold, fHipFold, fKneeFold ) ? 1 : 0;
		}
	}
	lua_pushinteger ( L, iDone );
	return 1;
}

int SetEntityVelocity(lua_State *L) { return SetEntityVelocityCore ( L, 0 ); }

// SetEscapeMenu(on): 0 leaves Escape (and a controller's START) to the scripts, for a menu of their own: it no longer
// opens the in-game menu, nor leaves a test level, though shift and Escape still do that. 1 is the usual; back to 1 at
// each level start
int SetEscapeMenu ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern bool g_bEscapeMenu;
	g_bEscapeMenu = lua_tonumber(L, 1) != 0;
	return 0;
}

// SetFSRSharpness(s): the sharpening of the FSR upscale (the level's FSR setting or SetRenderScale), 0 to 2
int SetFSRSharpness(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetFSRSharpness(lua_tonumber(L, 1));
	return 0;
}

// the rest of what the graphics quality sets, for a graphics menu with its own options (kept through visuals pushes and
// quality changes, the level's own again at the next level and when a test game ends; a negative value keeps the current)
// SetFXAA(on), SetReflections(on): the FXAA pass and the planar reflections
int SetFXAA(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetFXAA((int)lua_tonumber(L, 1));
	return 0;
}

// SetFrameRateCap(fps): the most frames a second, 10 to 1000; 0 for no cap
int SetFrameRateCap(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetFrameRateCap((int)lua_tonumber(L, 1));
	return 0;
}

// SetGamePaused(on): 1 pauses the game world for a menu as the in-game menu does (physics, the player, entities and their
// animation, AI, weapons, projectiles, particles and the game's sounds) while every script, the HUD, sprites, music and
// rendering carry on, and new sounds play; g_Time holds still and g_TimeElapsed and GetTimeElapsed() read 0 (Timer() and
// GetElapsedTime() are the real clock). 0 resumes. A level that ends resumes it
int SetGamePaused ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern void game_setgamepaused ( bool bPause );
	game_setgamepaused ( lua_tonumber(L, 1) != 0 );
	return 0;
}

// SetGamma(g): the gamma, 0.1 to 10, which the level start's fade-in heads for; 0 goes back to the level's
int SetGamma(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetGamma(lua_tonumber(L, 1));
	return 0;
}

// SetGrassSimpleLighting(on): the grass lit by the simpler, cheaper PBR (Low and Medium quality)
int SetGrassSimpleLighting(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetGrassSimpleLighting((int)lua_tonumber(L, 1));
	return 0;
}

// SetLODMultiplier(v): models with LOD levels switch to LOD 1, 2 and 3 past 400, 600 and 800 units times v (0 to 15; the
// graphics quality sets 1 to 3)
int SetLODMultiplier(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetLODMultiplier(lua_tonumber(L, 1));
	return 0;
}

int SetLensFlare(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetLensFlare((int)lua_tonumber(L, 1), LuaOptionalNumber(L, n, 2), LuaOptionalNumber(L, n, 3));
	return 0;
}

// SetLightShadow(light, on): whether the light casts shadows (1 or 0), for the same light numbers as SetLightRGB. A
// shadow is the costliest part of a point or spot light, so turn it off on lights that don't need one
int SetLightShadow(lua_State *L)
{
	if (LUA_GETTOP(L) < 2) return 0;
	int i = lua_tointeger(L, 1);
	if (i > 0 && i <= g.infinilightmax && t.infinilight[i].used == 1)
	{
		t.infinilight[i].bCanShadow = lua_tointeger(L, 2) != 0;
	}
	return 0;
}

int SetLightShafts(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaPostEffects_SetLightShafts((int)lua_tonumber(L, 1));
	return 0;
}

// SetMSAA(samples): multisample anti-aliasing, 1 (off), 2, 4 or 8 samples, with or without FXAA; under a render scale it
// samples the smaller 3D before the upscale. The render targets are made again at the next update (a hitch), so set it on
// a choice, not every frame. 0 goes back to the level's
int SetMSAA(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetMSAA((int)lua_tonumber(L, 1));
	return 0;
}

// SetOcclusionCulling(on [, objects [, animations [, terrain [, shadows [, spotShadows]]]]]): the GPU occlusion culling,
// which skips what stands wholly behind something (a frame late, and nothing within 1500 units), and what it is used for:
// objects, the animation of an occluded character (it pauses), terrain chunks, and point and spot light shadows (shadows
// both, or point only when spotShadows is given). 1 or 0 each
int SetOcclusionCulling(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetOcclusionCulling((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2), (int)LuaOptionalNumber(L, n, 3), (int)LuaOptionalNumber(L, n, 4), (int)LuaOptionalNumber(L, n, 5));
	if (n >= 6) LuaRenderSettings_SetSpotShadowCulling((int)lua_tonumber(L, 6));
	return 0;
}

// SetPhysicsPushLog(obj [, steps]) - a diagnostic: each PushObject on obj logs its body before and after, then over the
// next steps (8 at first) simulation steps; obj 0 stops it. GetPhysicsPushLog() returns the lines since, as a table
int SetPhysicsPushLog(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	int iSteps = (LUA_GETTOP(L) >= 2) ? (int)lua_tonumber(L, 2) : 8;
	extern void ODESetPushLog(int iObject, int iSteps);
	ODESetPushLog((int)lua_tonumber(L, 1), iSteps);
	return 0;
}

// SetPhysicsWorldRate(rate) - 60 steps the physics world 60 times a second (half the work) while the player keeps its own
// 120 steps, as its controller moves a set distance each step; any other value steps all of it at 120 as before. At first
// and at a test game's end the level's Performance setting (Physics World At 60Hz, visuals.PhysicsWorldAt60) applies
int SetPhysicsWorldRate(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	extern bool g_bPhysicsWorldAt60;
	g_bPhysicsWorldAt60 = (lua_tonumber(L, 1) == 60);
	return 0;
}

// SetProbesLowestLOD(on), SetReflectionsLowestLOD(on): environment probes and reflections drawn from each model's lowest LOD
int SetProbesLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetLowestLOD((int)lua_tonumber(L, 1), -1);
	return 0;
}

// SetRagdollAllDeaths(on): 1 gives every death of a character with ragdoll = 1 the forced ragdoll of
// SetEntityHealth(e, -12345), skipping the death animations; SetEntityHealth(e, -12346) still has none. 0 at each level start
int SetRagdollAllDeaths ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	extern bool g_bRagdollAllDeaths;
	g_bRagdollAllDeaths = lua_tointeger(L, 1) != 0;
	return 0;
}

int SetRawSoundDistanceScale ( lua_State *L ) { return SetRawSoundData ( L, 6 ); }
int GetRawSoundDistanceScale ( lua_State *L ) { return GetRawSoundData ( L, 4 ); }

// SetRaycastLowestLOD(on): rays and picks test models with LOD levels against their lowest LOD, which is cheaper and less exact
int SetRaycastLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetRaycastLowestLOD((int)lua_tonumber(L, 1));
	return 0;
}

int SetReflections(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetReflections((int)lua_tonumber(L, 1));
	return 0;
}

int SetReflectionsLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetLowestLOD(-1, (int)lua_tonumber(L, 1));
	return 0;
}

// SetRenderScale(f): the 3D drawn at f of the screen's resolution, 0.5 to 1, and upscaled with FSR 1, the HUD and sprites
// at the screen's; FXAA stays on below 1. It stands in for the level's FSR setting; 0 goes back to it. The render targets
// are made again (a hitch), so set it when the player lets go of a slider, not every step
int SetRenderScale(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetRenderScale(lua_tonumber(L, 1));
	return 0;
}

// SetSSR(on): screen space reflections on (1) or off (0)
int SetSSR(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetSSR((int)lua_tonumber(L, 1));
	return 0;
}

// SetShadowCascades(n [, split1, split2, split3, split4]): the sun's shadow cascades in use, 1 to 5 (past the last, no sun
// shadow), and the view depths the first four end at (380, 950, 7500, 30000 unless set; used only if all four are given,
// above 0 and rising). Moving a split changes that cascade's texel size, so its shadows sharpen or blur; each cascade's depth
// bias was tuned to the stock splits, so acne or light leaks may show
int SetShadowCascades(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fSplits[4];
	bool bSplits = n >= 5;
	for (int i = 0; i < 4 && bSplits; i++) fSplits[i] = (float)lua_tonumber(L, 2 + i);
	LuaRenderSettings_SetShadowCascades((int)lua_tonumber(L, 1), bSplits ? fSplits : NULL);
	return 0;
}

// SetShadowLights(spotMax [, pointMax]): how many spot and point lights cast shadows at once, 0 to 16
int SetShadowLights(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetShadowLights((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2));
	return 0;
}

// SetShadowRange(d): how far the sun's last shadow cascade reaches, 31000 to 500000 units (500000 unless set); nothing
// beyond it casts a sun shadow, and nearer makes the far shadows sharper. 0 goes back to the level's
int SetShadowRange(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetShadowRange(lua_tonumber(L, 1));
	return 0;
}

// SetShadowResolution(sun [, spot [, point]]): shadow map sizes, 0 (none) to 2048; a change recreates the maps (a hitch)
int SetShadowResolution(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetShadowResolution((int)lua_tonumber(L, 1), (int)LuaOptionalNumber(L, n, 2), (int)LuaOptionalNumber(L, n, 3));
	return 0;
}

// SetShadowsLowestLOD(on): models cast their shadows from their lowest LOD, which is cheaper
int SetShadowsLowestLOD(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetShadowsLowestLOD((int)lua_tonumber(L, 1));
	return 0;
}

// SetTerrainDetail(limit [, scale [, readBack]]): the terrain texture's detail as the graphics quality sets it: the detail
// limit 0 (finest) to 4, the detail scale 0.25 to 2, the read back reduction 4 to 6 (higher reads back less, coarser pages)
int SetTerrainDetail(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	LuaRenderSettings_SetTerrainDetail((int)lua_tonumber(L, 1), (float)LuaOptionalNumber(L, n, 2), (int)LuaOptionalNumber(L, n, 3));
	return 0;
}

// SetTransparentShadows(on): shadows cast through transparent surfaces, tinted by them; a change makes the shadow maps
// again (a hitch)
int SetTransparentShadows(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetTransparentShadows((int)lua_tonumber(L, 1));
	return 0;
}

// SetVsync(on): vsync on (1) or off (0), outranking the level's and SETUP.INI's; the swap chain is made again (a blip)
int SetVsync(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	LuaRenderSettings_SetVsync((int)lua_tonumber(L, 1));
	return 0;
}

// SleepEntity(e): puts a physics entity's body to sleep with every moving body touching it, directly or through others
// (its island), their velocities zeroed, as a body settled at rest sleeps; they wake as any sleeping body does (a blast,
// a hit, a velocity set, an awake body touching them). Returns how many bodies slept: 0 when e has no moving body, or its
// island holds a character, a body kept awake or a body still moving (try again later)
int SleepEntity(lua_State *L)
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	int iEntityIndex = lua_tonumber(L, 1);
	if ( !LuaEntityIDValid ( L, iEntityIndex, 1 ) ) return 0;
	int iSlept = 0;
	int iObjectNumber = t.entityelement[iEntityIndex].obj;
	int iForce = ( n >= 2 ) ? (int)lua_tointeger(L, 2) : 0;
	if ( iObjectNumber > 0 ) iSlept = PhysicsQuery_SleepIsland ( iObjectNumber, iForce );
	lua_pushinteger ( L, iSlept );
	return 1;
}

// waiting for their engine side, and this branch's own

// AddBlastDecal: neutral until its engine side is here; needs WickedCall_AddBlastDecal (wickedcalls.cpp, wickedcalls.h); WickedCall_FindDecalRayTargets (wickedcalls.cpp, wickedcalls.h); WickedCall_SetProjectedDecalOpacity (wickedcalls.cpp, wickedcalls.h)
int AddBlastDecal ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 2;
}

// AddProjectedDecal: neutral until its engine side is here; needs WickedCall_AddProjectedDecal (wickedcalls.cpp, wickedcalls.h)
int AddProjectedDecal ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// ClearGrassKillBox: neutral until its engine side is here; needs GGGRASS_MAX_KILLBOXES (GGGrass.cpp, GGGrass.h, GGGrassConstants.hlsli); GGGrass_ClearKillBox (GGGrass.cpp, GGGrass.h, M-Game.cpp)
int ClearGrassKillBox ( lua_State* L )
{
	return 0;
}

// GetDecalLimit(): the element count, and the ripple elements, none reserved until the decal work is here
int GetDecalLimit(lua_State *L)
{
	lua_pushinteger(L, g.decalelementmax);
	lua_pushinteger(L, 0);
	return 2;
}

// GetDecalStats: neutral until its engine side is here; needs g_iDecalDroppedFull (M-Decal.cpp)
int GetDecalStats ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 3;
}

// GetDopplerScale(): 0 until the audio doppler (the DX11 branch's wiAudio addition) is in this engine
int GetDopplerScale ( lua_State *L )
{
	lua_pushnumber ( L, 0 );
	return 1;
}

// The draw call counts wait for the profiler counters; the polygons come from this build's own main-camera count
int GetDrawCalls ( lua_State *L )
{
	extern uint64_t GGPerf_GetPolyCount();
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, (lua_Integer)GGPerf_GetPolyCount() );
	return 4;
}

// GetEngineProbe(name): the per-frame time and calls of the named probe (this branch has the first probe set, d0ed3926; the
// later probes with the longest single call are not here, so the third value is 0)
int GetEngineProbe(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	const char* pName = lua_tostring(L, 1);
	float fMilliseconds, fCalls;
	if (!pName || !WickedCall_ProbeGet(pName, &fMilliseconds, &fCalls)) return 0;
	lua_pushnumber(L, fMilliseconds);
	lua_pushnumber(L, fCalls);
	lua_pushnumber(L, 0);
	return 3;
}

// GetEngineProbeDetail: neutral until its engine side is here; needs WickedCall_ProbeDetail (wickedcalls.cpp, wickedcalls.h)
int GetEngineProbeDetail ( lua_State* L )
{
	lua_pushstring ( L, "" );
	return 1;
}

// GetFrameCosts: neutral until its engine side is here; needs WickedCallFrameCosts (wickedcalls.cpp, wickedcalls.h); WickedCall_GetFrameCosts (wickedcalls.cpp, wickedcalls.h); bVSync (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h); dExecute (wickedcalls.cpp, wickedcalls.h); dExecuteMax (wickedcalls.cpp, wickedcalls.h); dPresent (wickedcalls.cpp, wickedcalls.h); dPresentMax (wickedcalls.cpp, wickedcalls.h); dSubmit (wickedcalls.cpp, wickedcalls.h); dSubmitMax (wickedcalls.cpp, wickedcalls.h); iSubmits (wickedcalls.cpp, wickedcalls.h)
int GetFrameCosts ( lua_State* L )
{
	lua_pushnil ( L );
	return 1;
}

// GetGameQuality(): 3, High, the level's own settings, until the quality scaling is here
int GetGameQuality(lua_State* L)
{
	lua_pushinteger(L, 3);
	return 1;
}

// GetGpuFrameCosts: neutral until its engine side is here; needs WickedCallGpuCosts (wickedcalls.cpp, wickedcalls.h); WickedCall_GetGpuFrameCosts (wickedcalls.cpp, wickedcalls.h); dGpu (wickedcalls.cpp, wickedcalls.h); dGpuMax (wickedcalls.cpp, wickedcalls.h); iMissed (wickedcalls.cpp, wickedcalls.h)
int GetGpuFrameCosts ( lua_State* L )
{
	lua_pushnil ( L );
	return 1;
}

// GetLightShafts(): 0 until the Lua render settings are here
int GetLightShafts(lua_State* L)
{
	lua_pushnumber(L, 0);
	return 1;
}

// GetRenderTime: neutral until its engine side is here; needs GetRangeTime ()
int GetRenderTime ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 2;
}

// GetRippleBatching: neutral until its engine side is here; needs decal_getripplebatching (M-Decal.cpp)
int GetRippleBatching ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 4;
}

// GetTerrainMaterialAt: neutral until its engine side is here; needs GGTerrain_GetPaintedMaterial (GGTerrain.cpp, GGTerrain.h)
int GetTerrainMaterialAt ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// GetTerrainPageStats: neutral until its engine side is here; needs GGTerrain_GetPageCounts (GGTerrain.cpp, GGTerrain.h)
int GetTerrainPageStats ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 4;
}

// GetTreeCounts: neutral until its engine side is here; needs GGTrees_GetNumHighDetailShadow (GGTrees.cpp, GGTrees.h)
int GetTreeCounts ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 2;
}

// GetTreeCrownShade: neutral until its engine side is here; needs crown_mode (GGTrees.cpp, GGTrees.h); crown_shade (GGTrees.cpp, GGTrees.h)
int GetTreeCrownShade ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 2;
}

// GetTreeShade: neutral until its engine side is here; needs shade_bark (GGTrees.cpp, GGTrees.h); shade_leaves (GGTrees.cpp, GGTrees.h)
int GetTreeShade ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 2;
}

// GetTreeVariety: neutral until its engine side is here; needs variety_mirror (GGTrees.cpp, GGTrees.h); variety_tint (GGTrees.cpp, GGTrees.h)
int GetTreeVariety ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 2;
}

// GetVideoMemoryUsed: neutral until its engine side is here; needs GetVramUsageAndBudget (AutomationHarness.cpp, DBDLLCore.cpp)
int GetVideoMemoryUsed ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 4;
}

// GetWaterHeightAt: neutral until its engine side is here; needs spline_waterheightat (M-Splines.cpp, M-Splines.h)
int GetWaterHeightAt ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 2;
}

// RemoveProjectedDecal: neutral until its engine side is here; needs WickedCall_RemoveProjectedDecal (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int RemoveProjectedDecal ( lua_State* L )
{
	return 0;
}

// SetDecalLimit: neutral until its engine side is here; needs decal_setlimit (M-Decal.cpp, M-Decal.h)
int SetDecalLimit ( lua_State* L )
{
	return 0;
}

// SetDecalRange: neutral until its engine side is here; needs decal_setrange (M-Decal.cpp, M-Decal.h)
int SetDecalRange ( lua_State* L )
{
	return 0;
}

// SetEntityReceivesDecals: neutral until its engine side is here; needs WickedCall_SetObjectReceivesDecals (G-Gun.cpp, M-HUD.cpp, wickedcalls.cpp)
int SetEntityReceivesDecals ( lua_State* L )
{
	return 0;
}

// SetGpuTiming: neutral until its engine side is here; needs WickedCall_GetGpuTiming (wickedcalls.cpp, wickedcalls.h); WickedCall_SetGpuTiming (wickedcalls.cpp, wickedcalls.h)
int SetGpuTiming ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// SetGrassBudget: neutral until its engine side is here; needs GGGrass_SetBudget (GGGrass.cpp, GGGrass.h, M-GridEditB.cpp)
int SetGrassBudget ( lua_State* L )
{
	return 0;
}

// SetGrassDistance(d): the grass draw distance, clamped to the editor slider range (750 to 7000); the restore at a test
// game's end waits for its engine side
int SetGrassDistance(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fDistance = lua_tonumber(L, 1);
	if (fDistance < 750.0f) fDistance = 750.0f;
	if (fDistance > 7000.0f) fDistance = 7000.0f;
	gggrass_global_params.lod_dist = fDistance;
	return 0;
}

// SetGrassEqualDepth: neutral until its engine side is here; needs GGGrass_SetEqualDepth (GGGrass.cpp, GGGrass.h, M-GridEditB.cpp)
int SetGrassEqualDepth ( lua_State* L )
{
	return 0;
}

// SetGrassFadeBand: neutral until its engine side is here; needs GGGrass_SetFadeBand (GGGrass.cpp, GGGrass.h, M-GridEditB.cpp)
int SetGrassFadeBand ( lua_State* L )
{
	return 0;
}

// SetGrassKillBox: neutral until its engine side is here; needs GGGRASS_MAX_KILLBOXES (GGGrass.cpp, GGGrass.h, GGGrassConstants.hlsli); GGGrass_SetKillBox (GGGrass.cpp, GGGrass.h, GGTrees.h); GGTrees_KillBox (GGTrees.cpp, GGTrees.h)
int SetGrassKillBox ( lua_State* L )
{
	return 0;
}

// SetGrassShade: neutral until its engine side is here; needs GGGrass_SetShade (GGGrass.cpp, GGGrass.h, M-GridEditB.cpp)
int SetGrassShade ( lua_State* L )
{
	return 0;
}

// SetGrassTightBounds: neutral until its engine side is here; needs GGGrass_SetTightBounds (GGGrass.cpp, GGGrass.h, M-GridEditB.cpp)
int SetGrassTightBounds ( lua_State* L )
{
	return 0;
}

// SetGunReloadWhileRunning(on): neutral until its engine side is here; the stock rule, no reload while running
int SetGunReloadWhileRunning(lua_State* L)
{
	return 0;
}

// SetLightMaxDistance(distance): point and spot lights farther than this from the camera (their range added) are left out
// of the frame, as lights out of view are; 0, the default, for no limit. Back to none at the next level and test end
namespace wi::renderer { extern float gg_light_max_distance; }
// CameraLogMark(text): a line in the engine's camera call log (harness SET_CAMERALOG 1) and its dump two frames on, for a
// script to mark the frame of a hit; nothing when the log is off
extern bool g_bCameraLog; extern int g_iCameraLogDumpIn; extern void CameraLogLine(const char* pText);
int CameraLogMark(lua_State* L)
{
	if (!g_bCameraLog) return 0;
	char t[200];
	_snprintf(t, sizeof(t), "LUA MARK %s", LUA_GETTOP(L) >= 1 ? lua_tostring(L, 1) : "");
	t[sizeof(t) - 1] = 0;
	CameraLogLine(t);
	if (g_iCameraLogDumpIn == 0) g_iCameraLogDumpIn = 3;
	return 0;
}

int SetLightMaxDistance(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	const float fDistance = (float)lua_tonumber(L, 1);
	wi::renderer::gg_light_max_distance = fDistance > 0.0f ? fDistance : 0.0f;
	return 0;
}

// SetParticleHalfResolution(on): the particle layers marked for half resolution (their option bit 8) drawn at it, a quarter of
// the pixels laid over the scene (1, the default), or at full resolution as the rest (0)
namespace wi::renderer { void SetParticleHalfResolutionEnabled(bool value); bool GetParticleHalfResolutionEnabled(); }
int SetParticleHalfResolution ( lua_State* L )
{
	if (LUA_GETTOP(L) < 1) return 0;
	wi::renderer::SetParticleHalfResolutionEnabled(lua_tonumber(L, 1) != 0);
	return 0;
}
// GetParticleHalfResolution(): 1 when the marked particle layers are drawn at half resolution, else 0
int GetParticleHalfResolution ( lua_State* L )
{
	lua_pushnumber(L, wi::renderer::GetParticleHalfResolutionEnabled() ? 1 : 0);
	return 1;
}

// SetProjectedDecalBlend: neutral until its engine side is here; needs WickedCall_SetProjectedDecalBlend (wickedcalls.cpp, wickedcalls.h)
int SetProjectedDecalBlend ( lua_State* L )
{
	return 0;
}

// SetProjectedDecalFacing: neutral until its engine side is here; needs WickedCall_SetProjectedDecalFacing (wickedcalls.cpp, wickedcalls.h)
int SetProjectedDecalFacing ( lua_State* L )
{
	return 0;
}

// SetProjectedDecalGrass: neutral until its engine side is here; needs WickedCall_SetProjectedDecalGrass (wickedcalls.cpp, wickedcalls.h)
int SetProjectedDecalGrass ( lua_State* L )
{
	return 0;
}

// SetProjectedDecalLimit: neutral until its engine side is here; needs WickedCall_SetProjectedDecalLimit (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int SetProjectedDecalLimit ( lua_State* L )
{
	return 0;
}

// SetProjectedDecalOpacity: neutral until its engine side is here; needs WickedCall_SetProjectedDecalOpacity (wickedcalls.cpp, wickedcalls.h)
int SetProjectedDecalOpacity ( lua_State* L )
{
	return 0;
}

// SetRenderTiming(mode): 1 times the renderer's work every frame so GetRenderTime can read them, 0 stops. This engine's
// profiler has no CPU-only mode, so 2 times the GPU as well
int SetRenderTiming ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	int iMode = (int)lua_tointeger(L, 1);
	wiProfiler::SetEnabled ( iMode != 0 );
	return 0;
}

// SetRippleBatching: neutral until its engine side is here; needs decal_setripplebatching (M-Decal.cpp, M-GridEditB.cpp)
int SetRippleBatching ( lua_State* L )
{
	return 0;
}

// SetSunShadowDirection: neutral until its engine side is here; needs WickedCall_SetSunShadowDirection (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int SetSunShadowDirection ( lua_State* L )
{
	return 0;
}

// SetTerrainDryBox: neutral until its engine side is here; needs GGTerrain_SetDryBox (GGTerrain.cpp, GGTerrain.h)
int SetTerrainDryBox ( lua_State* L )
{
	return 0;
}

// SetTerrainMaterialWetness: neutral until its engine side is here; needs GGTerrain_SetMaterialWetness (GGTerrain.cpp, GGTerrain.h)
int SetTerrainMaterialWetness ( lua_State* L )
{
	return 0;
}

// SetTerrainSeabedSkip: neutral until its engine side is here; needs GGTerrain_SetSeabedSkip (GGTerrain.cpp, GGTerrain.h, M-GridEditB.cpp)
int SetTerrainSeabedSkip ( lua_State* L )
{
	return 0;
}

// SetTerrainWetness: neutral until its engine side is here; needs GGTerrain_SetWetness (GGTerrain.cpp, GGTerrain.h)
int SetTerrainWetness ( lua_State* L )
{
	return 0;
}

// SetTreeCrownShade: neutral until its engine side is here; needs GGTrees_SetCrownShade (GGTrees.cpp, GGTrees.h)
int SetTreeCrownShade ( lua_State* L )
{
	return 0;
}

// SetTreeDistance: neutral until its engine side is here; needs GGTrees_SetLuaDistances (GGTrees.cpp, GGTrees.h)
int SetTreeDistance ( lua_State* L )
{
	return 0;
}

// SetTreeShade: neutral until its engine side is here; needs GGTrees_SetShade (GGTrees.cpp, GGTrees.h)
int SetTreeShade ( lua_State* L )
{
	return 0;
}

// SetTreeSubsurface: neutral until its engine side is here; needs WickedCall_UpdateTreeSubsurface (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h); g_bLuaSetTreeSubsurface (M-GridEditB.cpp)
int SetTreeSubsurface ( lua_State* L )
{
	return 0;
}

// SetTreeVariety: neutral until its engine side is here; needs GGTrees_SetVariety (GGTrees.cpp, GGTrees.h)
int SetTreeVariety ( lua_State* L )
{
	return 0;
}

// GG: the Wicked particle effect controls the DX11 branch had, on this engine's scene: an effect is the root entity
// WParticleEffectLoad gave back, its layers the emitters parented to it (a layer named as its emitter)
static bool WParticleEffectLayerMatches(wi::scene::Scene& scene, wi::ecs::Entity emitter, const char* pName)
{
	if (pName == nullptr || pName[0] == 0 || (pName[0] == '*' && pName[1] == 0)) return true;
	const wi::scene::NameComponent* name = scene.names.GetComponent(emitter);
	return name && _stricmp(name->name.c_str(), pName) == 0;
}
#define GG_FOR_EFFECT_LAYERS(root) \
	wi::scene::Scene& scene = wiScene::GetScene(); \
	for (size_t i = 0; i < scene.emitters.GetCount(); i++) \
	{ \
		wi::ecs::Entity emitter = scene.emitters.GetEntity(i); \
		wi::scene::HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter); \
		if (!hier || hier->parentID != (root)) continue;

// WParticleEffectSetSize(EffectID, percent): the particle size of every layer, as a percentage (100 as authored)
int WParticleEffectSetSize ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 2) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	float fSize = max(0.0f, (float)lua_tonumber(L, 2) / 100.0f);
	GG_FOR_EFFECT_LAYERS(root)
		scene.emitters[i].size_scale = fSize;
	}
	return 0;
}
int WParticleEffectGetSize ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	GG_FOR_EFFECT_LAYERS(root)
		lua_pushnumber(L, scene.emitters[i].size_scale * 100.0f);
		return 1;
	}
	return 0;
}
// WParticleEffectSetColor(EffectID, r, g, b): a colour tint on every layer (255, 255, 255 as authored)
int WParticleEffectSetColor ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 4) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	XMFLOAT3 tint = XMFLOAT3(max(0.0f, (float)lua_tonumber(L, 2) / 255.0f), max(0.0f, (float)lua_tonumber(L, 3) / 255.0f), max(0.0f, (float)lua_tonumber(L, 4) / 255.0f));
	GG_FOR_EFFECT_LAYERS(root)
		scene.emitters[i].color_tint = tint;
	}
	return 0;
}
int WParticleEffectGetColor ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	GG_FOR_EFFECT_LAYERS(root)
		lua_pushnumber(L, scene.emitters[i].color_tint.x * 255.0f);
		lua_pushnumber(L, scene.emitters[i].color_tint.y * 255.0f);
		lua_pushnumber(L, scene.emitters[i].color_tint.z * 255.0f);
		return 3;
	}
	return 0;
}
// WParticleEffectSetOpacity(EffectID, percent): the opacity of every layer, as a percentage (100 as authored)
int WParticleEffectSetOpacity ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 2) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	float fOpacity = max(0.0f, (float)lua_tonumber(L, 2) / 100.0f);
	GG_FOR_EFFECT_LAYERS(root)
		scene.emitters[i].opacity_scale = fOpacity;
	}
	return 0;
}
int WParticleEffectGetOpacity ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	GG_FOR_EFFECT_LAYERS(root)
		lua_pushnumber(L, scene.emitters[i].opacity_scale * 100.0f);
		return 1;
	}
	return 0;
}
// WParticleEffectSetKillBox(EffectID, slot [, x1, y1, z1, x2, y2, z2]): a box (1 to 8) a particle of the effect dies in;
// without the corners the slot is cleared
int WParticleEffectSetKillBox ( lua_State* L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	int iSlot = (int)lua_tonumber(L, 2) - 1;
	if (iSlot < 0 || iSlot >= 8) return 0;
	XMFLOAT4 vMin = XMFLOAT4(FLT_MAX, FLT_MAX, FLT_MAX, 0);
	XMFLOAT4 vMax = XMFLOAT4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0);
	if (n >= 8)
	{
		float fX1 = (float)lua_tonumber(L, 3), fY1 = (float)lua_tonumber(L, 4), fZ1 = (float)lua_tonumber(L, 5);
		float fX2 = (float)lua_tonumber(L, 6), fY2 = (float)lua_tonumber(L, 7), fZ2 = (float)lua_tonumber(L, 8);
		vMin = XMFLOAT4(min(fX1, fX2), min(fY1, fY2), min(fZ1, fZ2), 0);
		vMax = XMFLOAT4(max(fX1, fX2), max(fY1, fY2), max(fZ1, fZ2), 0);
	}
	GG_FOR_EFFECT_LAYERS(root)
		wi::EmittedParticleSystem& ec = scene.emitters[i];
		ec.killbox_min[iSlot] = vMin;
		ec.killbox_max[iSlot] = vMax;
		if (ec.killbox_count < (uint32_t)iSlot + 1) ec.killbox_count = (uint32_t)iSlot + 1;
	}
	return 0;
}
int WParticleEffectClearKillBoxes ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	GG_FOR_EFFECT_LAYERS(root)
		scene.emitters[i].killbox_count = 0;
	}
	return 0;
}
// WParticleEffectGetBounds(EffectID): the box the effect's layers spawn in (their meshes or volumes, the orbital offset
// and the random position added), as minx, miny, minz, maxx, maxy, maxz; nothing for an effect with no layers
int WParticleEffectGetBounds ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	wi::primitive::AABB bounds;
	bool bFound = false;
	GG_FOR_EFFECT_LAYERS(root)
		wi::scene::TransformComponent* transform = scene.transforms.GetComponent(emitter);
		if (!transform) continue;
		const wi::EmittedParticleSystem& ec = scene.emitters[i];
		const wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(ec.meshID);
		wi::primitive::AABB area;
		if (mesh)
		{
			area = mesh->aabb.transform(transform->world);
		}
		else if (ec.IsVolumeEnabled())
		{
			area = wi::primitive::AABB(XMFLOAT3(-1, -1, -1), XMFLOAT3(1, 1, 1)).transform(transform->world);
		}
		else
		{
			XMFLOAT3 pos = transform->GetPosition();
			float fHalf = fabsf(ec.random_position_scale) * 0.5f;
			area = wi::primitive::AABB(XMFLOAT3(pos.x - fHalf, pos.y, pos.z - fHalf), XMFLOAT3(pos.x + fHalf, pos.y, pos.z + fHalf));
		}
		area._min.x -= fabsf(ec.startpos.x); area._max.x += fabsf(ec.startpos.x);
		area._min.y -= fabsf(ec.startpos.y) * 0.5f; area._max.y += fabsf(ec.startpos.y) * 0.5f;
		area._min.z -= fabsf(ec.startpos.z); area._max.z += fabsf(ec.startpos.z);
		bounds = bFound ? wi::primitive::AABB::Merge(bounds, area) : area;
		bFound = true;
	}
	if (!bFound) return 0;
	lua_pushnumber(L, bounds._min.x); lua_pushnumber(L, bounds._min.y); lua_pushnumber(L, bounds._min.z);
	lua_pushnumber(L, bounds._max.x); lua_pushnumber(L, bounds._max.y); lua_pushnumber(L, bounds._max.z);
	return 6;
}
// WParticleEffectSetLayer(EffectID, Name [, Size [, Emissive [, Count [, Speed [, Spread [, Life]]]]]]): multiplies one layer of
// the effect (its emitter named Name, or every layer for "" or "*"); a value below 0 or left out keeps the layer's; the
// count of layers changed
int WParticleEffectSetLayer ( lua_State* L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 2) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	const char* pName = lua_tostring(L, 2);
	float fValues[6];
	for (int v = 0; v < 6; v++) fValues[v] = (n >= 3 + v) ? (float)lua_tonumber(L, 3 + v) : -1.0f;
	int iLayers = 0;
	GG_FOR_EFFECT_LAYERS(root)
		if (!WParticleEffectLayerMatches(scene, emitter, pName)) continue;
		wi::EmittedParticleSystem& layer = scene.emitters[i];
		if (fValues[0] >= 0) layer.layer_size_scale = fValues[0];
		if (fValues[1] >= 0) layer.emissive_scale = fValues[1];
		if (fValues[2] >= 0) layer.count_scale = fValues[2];
		if (fValues[3] >= 0) layer.speed_scale = fValues[3];
		if (fValues[4] >= 0) layer.spread_scale = fValues[4];
		if (fValues[5] >= 0) layer.life_scale = fValues[5];
		iLayers++;
	}
	lua_pushinteger(L, iLayers);
	return 1;
}
// WParticleEffectGetLayer(EffectID, Name): Size, Emissive, Count, Speed, Spread, Life of the effect's first layer named Name
int WParticleEffectGetLayer ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 2) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	const char* pName = lua_tostring(L, 2);
	GG_FOR_EFFECT_LAYERS(root)
		if (!WParticleEffectLayerMatches(scene, emitter, pName)) continue;
		const wi::EmittedParticleSystem& layer = scene.emitters[i];
		lua_pushnumber(L, layer.layer_size_scale);
		lua_pushnumber(L, layer.emissive_scale);
		lua_pushnumber(L, layer.count_scale);
		lua_pushnumber(L, layer.speed_scale);
		lua_pushnumber(L, layer.spread_scale);
		lua_pushnumber(L, layer.life_scale);
		return 6;
	}
	return 0;
}
// WParticleEffectGetLayers(EffectID): a table of the effect's layer names
int WParticleEffectGetLayers ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	lua_newtable(L);
	int iLayers = 0;
	GG_FOR_EFFECT_LAYERS(root)
		const wi::scene::NameComponent* name = scene.names.GetComponent(emitter);
		lua_pushstring(L, name ? name->name.c_str() : "");
		lua_rawseti(L, -2, ++iLayers);
	}
	return 1;
}
// WParticleEffectSetLayerWind(EffectID, Name, x, y, z): the air's velocity a layer's drag works against, so its particles
// drift with it; the count of layers changed
int WParticleEffectSetLayerWind ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 5) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	const char* pName = lua_tostring(L, 2);
	XMFLOAT3 wind = XMFLOAT3((float)lua_tonumber(L, 3), (float)lua_tonumber(L, 4), (float)lua_tonumber(L, 5));
	int iLayers = 0;
	GG_FOR_EFFECT_LAYERS(root)
		if (!WParticleEffectLayerMatches(scene, emitter, pName)) continue;
		scene.emitters[i].layer_wind = wind;
		iLayers++;
	}
	lua_pushinteger(L, iLayers);
	return 1;
}
int WParticleEffectGetLayerWind ( lua_State* L )
{
	lua2 = L;
	if (LUA_GETTOP(L) < 2) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	const char* pName = lua_tostring(L, 2);
	GG_FOR_EFFECT_LAYERS(root)
		if (!WParticleEffectLayerMatches(scene, emitter, pName)) continue;
		const XMFLOAT3& wind = scene.emitters[i].layer_wind;
		lua_pushnumber(L, wind.x); lua_pushnumber(L, wind.y); lua_pushnumber(L, wind.z);
		return 3;
	}
	return 0;
}
// WParticleEffectSetWrapMotion(EffectID [, x, y, z]): the motion an effect's wrapping-volume layers draw their streaks
// against (the emitter's own motion when left out); the count of layers changed
int WParticleEffectSetWrapMotion ( lua_State* L )
{
	lua2 = L;
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	wi::ecs::Entity root = (wi::ecs::Entity)lua_tonumber(L, 1);
	const bool bSet = n >= 4;
	XMFLOAT3 motion = bSet ? XMFLOAT3((float)lua_tonumber(L, 2), (float)lua_tonumber(L, 3), (float)lua_tonumber(L, 4)) : XMFLOAT3(0, 0, 0);
	int iLayers = 0;
	GG_FOR_EFFECT_LAYERS(root)
		scene.emitters[i].wrap_motion = motion;
		scene.emitters[i].bWrapMotion = bSet;
		iLayers++;
	}
	lua_pushinteger(L, iLayers);
	return 1;
}
#undef GG_FOR_EFFECT_LAYERS








// SetTAA(on): the engine's temporal anti-aliasing, 1 on, 0 off (this engine's; FSR 2 upscaling anti-aliases on its own).
// Back to off at each level start and a test game's end
int SetTAA ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern void LuaRenderSettings_SetTAA(int iOn);
	LuaRenderSettings_SetTAA ( lua_tointeger(L, 1) != 0 ? 1 : 0 );
	return 0;
}

// GetTAA(): 1 while the temporal anti-aliasing is on
int GetTAA ( lua_State *L )
{
	extern int LuaRenderSettings_GetTAA(void);
	lua_pushinteger ( L, LuaRenderSettings_GetTAA() );
	return 1;
}

// SetUpscaler(mode): what SetRenderScale upscales with on this engine: 0 FSR 1 (spatial, the DX11 branch's), 1 FSR 2
// (temporal; it anti-aliases too, but GameGuru's own terrain, tree and grass passes do not yet give it the motion it
// needs, so it smears and speckles until that is done). FSR 1 at each level start
int SetUpscaler ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	extern void LuaRenderSettings_SetUpscaler(int iMode);
	LuaRenderSettings_SetUpscaler ( (int)lua_tointeger(L, 1) );
	return 0;
}

// GetUpscaler(): 0 FSR 1, 1 FSR 2
int GetUpscaler ( lua_State *L )
{
	extern int LuaRenderSettings_GetUpscaler(void);
	lua_pushinteger ( L, LuaRenderSettings_GetUpscaler() );
	return 1;
}

void addFunctions_hiredgun()
{
	lua_register(lua2, "AddBlastDecal", AddBlastDecal);
	lua_register(lua2, "AddBulletHole", AddBulletHole);
	lua_register(lua2, "AddProjectedDecal", AddProjectedDecal);
	lua_register(lua2, "BulletRay", BulletRay);
	lua_register(lua2, "ClearGrassKillBox", ClearGrassKillBox);
	lua_register(lua2, "GetBloom", GetBloom);
	lua_register(lua2, "GetDecalLimit", GetDecalLimit);
	lua_register(lua2, "GetDecalRange", GetDecalRange);
	lua_register(lua2, "GetDecalStats", GetDecalStats);
	lua_register(lua2, "GetDelayedShadows", GetDelayedShadows);
	lua_register(lua2, "GetDepthOfField", GetDepthOfField);
	lua_register(lua2, "GetDopplerScale", GetDopplerScale);
	lua_register(lua2, "GetDrawCalls", GetDrawCalls);
	lua_register(lua2, "GetEngineProbe", GetEngineProbe);
	lua_register(lua2, "GetEngineProbeDetail", GetEngineProbeDetail);
	lua_register(lua2, "GetEntityAxes", GetEntityAxes);
	lua_register(lua2, "GetEntityImmunity", GetEntityImmunity);
	lua_register(lua2, "GetEntityInstanceEmissive", GetEntityInstanceEmissive);
	lua_register(lua2, "GetEntityInstanceTint", GetEntityInstanceTint);
	lua_register(lua2, "GetEntityLimbAlpha", GetEntityLimbAlpha);
	lua_register(lua2, "GetEntityLimbBounds", GetEntityLimbBounds);
	lua_register(lua2, "GetEntityLimbCount", GetEntityLimbCount);
	lua_register(lua2, "GetEntityLimbName", GetEntityLimbName);
	lua_register(lua2, "GetEntityLimbParent", GetEntityLimbParent);
	lua_register(lua2, "GetEntityMass", GetEntityMass);
	lua_register(lua2, "GetEntityRagdollForcePending", GetEntityRagdollForcePending);
	lua_register(lua2, "GetEntityRagdollHoldStiffness", GetEntityRagdollHoldStiffness);
	lua_register(lua2, "GetEntityRagdollPose", GetEntityRagdollPose);
	lua_register(lua2, "GetFrameCosts", GetFrameCosts);
	lua_register(lua2, "GetGamePaused", GetGamePaused);
	lua_register(lua2, "GetGameQuality", GetGameQuality);
	lua_register(lua2, "GetGpuFrameCosts", GetGpuFrameCosts);
	lua_register(lua2, "GetGraphicsSettings", GetGraphicsSettings);
	lua_register(lua2, "GetGrassDistance", GetGrassDistance);
	lua_register(lua2, "GetLensFlare", GetLensFlare);
	lua_register(lua2, "GetLightShafts", GetLightShafts);
	lua_register(lua2, "GetLuaEntityCosts", GetLuaEntityCosts);
	lua_register(lua2, "GetObjectMass", GetObjectMass);
	lua_register(lua2, "GetPhysicsAwakeBodies", GetPhysicsAwakeBodies);
	lua_register(lua2, "GetPhysicsBodyPose", GetPhysicsBodyPose);
	lua_register(lua2, "GetPhysicsPushLog", GetPhysicsPushLog);
	lua_register(lua2, "GetPhysicsSlowPairs", GetPhysicsSlowPairs);
	lua_register(lua2, "GetPhysicsStats", GetPhysicsStats);
	lua_register(lua2, "GetPhysicsStatsTop", GetPhysicsStatsTop);
	lua_register(lua2, "GetPhysicsWorldRate", GetPhysicsWorldRate);
	lua_register(lua2, "GetPlayerHit", GetPlayerHit);
	lua_register(lua2, "GetPlayerHitSeq", GetPlayerHitSeq);
	lua_register(lua2, "GetRagdollAllDeaths", GetRagdollAllDeaths);
	lua_register(lua2, "GetRayNormalX", GetRayNormalX);
	lua_register(lua2, "GetRayNormalY", GetRayNormalY);
	lua_register(lua2, "GetRayNormalZ", GetRayNormalZ);
	lua_register(lua2, "GetRenderTime", GetRenderTime);
	lua_register(lua2, "GetRippleBatching", GetRippleBatching);
	lua_register(lua2, "GetTerrainMaterialAt", GetTerrainMaterialAt);
	lua_register(lua2, "GetTerrainPageStats", GetTerrainPageStats);
	lua_register(lua2, "GetTextHeight", GetTextHeight);
	lua_register(lua2, "GetTextWidth", GetTextWidth);
	lua_register(lua2, "GetTreeCounts", GetTreeCounts);
	lua_register(lua2, "GetTreeCrownShade", GetTreeCrownShade);
	lua_register(lua2, "GetTreeDistance", GetTreeDistance);
	lua_register(lua2, "GetTreeShade", GetTreeShade);
	lua_register(lua2, "GetTreeSubsurface", GetTreeSubsurface);
	lua_register(lua2, "GetTreeTypeName", GetTreeTypeName);
	lua_register(lua2, "GetTreeVariety", GetTreeVariety);
	lua_register(lua2, "GetTreesNear", GetTreesNear);
	lua_register(lua2, "GetVideoMemoryUsed", GetVideoMemoryUsed);
	lua_register(lua2, "GetWaterHeightAt", GetWaterHeightAt);
	lua_register(lua2, "GetWind", GetWind);
	lua_register(lua2, "IntersectRay", IntersectRay);
	lua_register(lua2, "PhysicsOverlapBox", PhysicsOverlapBox);
	lua_register(lua2, "PhysicsRay", PhysicsRay);
	lua_register(lua2, "PhysicsSweepBox", PhysicsSweepBox);
	lua_register(lua2, "RayTrees", RayTrees);
	lua_register(lua2, "RemoveProjectedDecal", RemoveProjectedDecal);
	lua_register(lua2, "ResetDelayedShadows", ResetDelayedShadows);
	lua_register(lua2, "ResetEntityMaterial", ResetEntityMaterial);
	lua_register(lua2, "ResetGraphicsSettings", ResetGraphicsSettings);
	lua_register(lua2, "SetAO", SetAO);
	lua_register(lua2, "SetAnimations30Fps", SetAnimations30Fps);
	lua_register(lua2, "SetBloom", SetBloom);
	lua_register(lua2, "SetDecalLimit", SetDecalLimit);
	lua_register(lua2, "SetDecalRange", SetDecalRange);
	lua_register(lua2, "SetDelayedShadows", SetDelayedShadows);
	lua_register(lua2, "SetDepthOfField", SetDepthOfField);
	lua_register(lua2, "SetEntityAngularVelocity", SetEntityAngularVelocity);
	lua_register(lua2, "SetEntityInstanceEmissive", SetEntityInstanceEmissive);
	lua_register(lua2, "SetEntityInstanceTint", SetEntityInstanceTint);
	lua_register(lua2, "SetEntityLimbAlpha", SetEntityLimbAlpha);
	lua_register(lua2, "SetEntityRagdoll", SetEntityRagdoll);
	lua_register(lua2, "SetEntityRagdollHold", SetEntityRagdollHold);
	lua_register(lua2, "SetEntityRagdollHoldReference", SetEntityRagdollHoldReference);
	lua_register(lua2, "SetEntityRagdollHoldStiffness", SetEntityRagdollHoldStiffness);
	lua_register(lua2, "SetEntityReceivesDecals", SetEntityReceivesDecals);
	lua_register(lua2, "SetEntityVelocity", SetEntityVelocity);
	lua_register(lua2, "SetEscapeMenu", SetEscapeMenu);
	lua_register(lua2, "SetFSRSharpness", SetFSRSharpness);
	lua_register(lua2, "SetFXAA", SetFXAA);
	lua_register(lua2, "SetFrameRateCap", SetFrameRateCap);
	lua_register(lua2, "SetGamePaused", SetGamePaused);
	lua_register(lua2, "SetGamma", SetGamma);
	lua_register(lua2, "SetGpuTiming", SetGpuTiming);
	lua_register(lua2, "SetGrassBudget", SetGrassBudget);
	lua_register(lua2, "SetGrassDistance", SetGrassDistance);
	lua_register(lua2, "SetGrassEqualDepth", SetGrassEqualDepth);
	lua_register(lua2, "SetGrassFadeBand", SetGrassFadeBand);
	lua_register(lua2, "SetGrassKillBox", SetGrassKillBox);
	lua_register(lua2, "SetGrassShade", SetGrassShade);
	lua_register(lua2, "SetGrassSimpleLighting", SetGrassSimpleLighting);
	lua_register(lua2, "SetGrassTightBounds", SetGrassTightBounds);
	lua_register(lua2, "SetGunReloadWhileRunning", SetGunReloadWhileRunning);
	lua_register(lua2, "SetLODMultiplier", SetLODMultiplier);
	lua_register(lua2, "SetLensFlare", SetLensFlare);
	lua_register(lua2, "SetLightMaxDistance", SetLightMaxDistance);
	lua_register(lua2, "CameraLogMark", CameraLogMark);
	lua_register(lua2, "SetLightShadow", SetLightShadow);
	lua_register(lua2, "SetLightShafts", SetLightShafts);
	lua_register(lua2, "SetMSAA", SetMSAA);
	lua_register(lua2, "SetOcclusionCulling", SetOcclusionCulling);
	lua_register(lua2, "SetParticleHalfResolution", SetParticleHalfResolution);
	lua_register(lua2, "GetParticleHalfResolution", GetParticleHalfResolution);
	lua_register(lua2, "SetPhysicsPushLog", SetPhysicsPushLog);
	lua_register(lua2, "SetPhysicsWorldRate", SetPhysicsWorldRate);
	lua_register(lua2, "SetProbesLowestLOD", SetProbesLowestLOD);
	lua_register(lua2, "SetProjectedDecalBlend", SetProjectedDecalBlend);
	lua_register(lua2, "SetProjectedDecalFacing", SetProjectedDecalFacing);
	lua_register(lua2, "SetProjectedDecalGrass", SetProjectedDecalGrass);
	lua_register(lua2, "SetProjectedDecalLimit", SetProjectedDecalLimit);
	lua_register(lua2, "SetProjectedDecalOpacity", SetProjectedDecalOpacity);
	lua_register(lua2, "SetRagdollAllDeaths", SetRagdollAllDeaths);
	lua_register(lua2, "SetRawSoundDistanceScale", SetRawSoundDistanceScale);
	lua_register(lua2, "GetRawSoundDistanceScale", GetRawSoundDistanceScale);
	lua_register(lua2, "SetRaycastLowestLOD", SetRaycastLowestLOD);
	lua_register(lua2, "SetReflections", SetReflections);
	lua_register(lua2, "SetReflectionsLowestLOD", SetReflectionsLowestLOD);
	lua_register(lua2, "SetRenderScale", SetRenderScale);
	lua_register(lua2, "SetRenderTiming", SetRenderTiming);
	lua_register(lua2, "SetRippleBatching", SetRippleBatching);
	lua_register(lua2, "SetSSR", SetSSR);
	lua_register(lua2, "SetShadowCascades", SetShadowCascades);
	lua_register(lua2, "SetShadowLights", SetShadowLights);
	lua_register(lua2, "SetShadowRange", SetShadowRange);
	lua_register(lua2, "SetShadowResolution", SetShadowResolution);
	lua_register(lua2, "SetShadowsLowestLOD", SetShadowsLowestLOD);
	lua_register(lua2, "SetSunShadowDirection", SetSunShadowDirection);
	lua_register(lua2, "SetTerrainDetail", SetTerrainDetail);
	lua_register(lua2, "SetTerrainDryBox", SetTerrainDryBox);
	lua_register(lua2, "SetTerrainMaterialWetness", SetTerrainMaterialWetness);
	lua_register(lua2, "SetTerrainSeabedSkip", SetTerrainSeabedSkip);
	lua_register(lua2, "SetTerrainWetness", SetTerrainWetness);
	lua_register(lua2, "SetTransparentShadows", SetTransparentShadows);
	lua_register(lua2, "SetTreeCrownShade", SetTreeCrownShade);
	lua_register(lua2, "SetTreeDistance", SetTreeDistance);
	lua_register(lua2, "SetTreeShade", SetTreeShade);
	lua_register(lua2, "SetTreeSubsurface", SetTreeSubsurface);
	lua_register(lua2, "SetTreeVariety", SetTreeVariety);
	lua_register(lua2, "SetVsync", SetVsync);
	lua_register(lua2, "SleepEntity", SleepEntity);
	lua_register(lua2, "WParticleEffectClearKillBoxes", WParticleEffectClearKillBoxes);
	lua_register(lua2, "WParticleEffectGetBounds", WParticleEffectGetBounds);
	lua_register(lua2, "WParticleEffectSetKillBox", WParticleEffectSetKillBox);
	lua_register(lua2, "WParticleEffectSetLayer", WParticleEffectSetLayer);
	lua_register(lua2, "WParticleEffectSetLayerWind", WParticleEffectSetLayerWind);
	lua_register(lua2, "WParticleEffectSetOpacity", WParticleEffectSetOpacity);
	lua_register(lua2, "WParticleEffectSetSize", WParticleEffectSetSize);
	lua_register(lua2, "WParticleEffectSetWrapMotion", WParticleEffectSetWrapMotion);
	lua_register(lua2, "WParticleEffectGetSize", WParticleEffectGetSize);
	lua_register(lua2, "WParticleEffectSetColor", WParticleEffectSetColor);
	lua_register(lua2, "WParticleEffectGetColor", WParticleEffectGetColor);
	lua_register(lua2, "WParticleEffectGetOpacity", WParticleEffectGetOpacity);
	lua_register(lua2, "WParticleEffectGetLayer", WParticleEffectGetLayer);
	lua_register(lua2, "WParticleEffectGetLayers", WParticleEffectGetLayers);
	lua_register(lua2, "WParticleEffectGetLayerWind", WParticleEffectGetLayerWind);
	lua_register(lua2, "SetTAA", SetTAA);
	lua_register(lua2, "GetTAA", GetTAA);
	lua_register(lua2, "SetUpscaler", SetUpscaler);
	lua_register(lua2, "GetUpscaler", GetUpscaler);
}

// the main Lua state's heap in KB (lua_loop_allentities' per-entity costs, M-LUA.cpp); this branch's state is lua2
int LuaHeapKB(void)
{
	extern lua_State* lua2;
	return lua2 ? lua_gc(lua2, LUA_GCCOUNT, 0) : 0;
}
