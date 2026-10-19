// Hired Gun: the Lua functions the game's scripts call that the DX11 branch added. Carried as they are where their
// engine side exists in this build; the rest answer with neutral values (no hit, zero, empty) until that side is ported.
// Registered by addFunctions_hiredgun(), called from addFunctions().

#include "..\GGTerrain\GGTrees.h"

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

int GetDecalLimit(lua_State *L)
{
	// the ripple elements wait for the decal work, so none are reserved yet
	lua_pushinteger(L, g.decalelementmax);
	lua_pushinteger(L, 0);
	return 2;
}

int GetDecalRange(lua_State *L)
{
	lua_pushinteger(L, g.decalrange);
	return 1;
}

// GetDopplerScale(): 0 until the audio doppler (the DX11 branch's wiAudio addition) is in this engine
int GetDopplerScale ( lua_State *L )
{
	lua_pushnumber ( L, 0 );
	return 1;
}

// GetDrawCalls(): the last frame's draw calls in the main pass, in the shadow passes and for transparent objects, and
// the triangles the main pass drew
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

// GetGamePaused(): 1 while SetGamePaused holds the world
int GetGamePaused ( lua_State *L )
{
	lua_pushnumber ( L, 0 ); // SetGamePaused is neutral until its engine side is here, so nothing is ever paused by it
	return 1;
}

// GetGameQuality(): the graphics quality in force, 1 Low, 2 Medium, 3 High (the level's own settings), 4 Ultra
int GetGameQuality(lua_State* L)
{
	lua_pushinteger(L, 3); // High, the level's own settings, until the quality scaling is here
	return 1;
}

int GetGrassDistance(lua_State* L)
{
	lua_pushnumber(L, gggrass_global_params.lod_dist);
	return 1;
}

// GetLightShafts(): 0 until the Lua render settings are here
int GetLightShafts(lua_State* L)
{
	lua_pushnumber(L, 0);
	return 1;
}

// GetObjectMass(obj) - the kilos its physics body has now; 0 for no body or a static one
int GetObjectMass(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	lua_pushnumber(L, 0); // neutral until its engine side is here (kilos for bodies)
	return 1;
}

// GetPhysicsWorldRate() - 60 or 120
int GetPhysicsWorldRate(lua_State* L)
{
	lua_pushinteger(L, 120); // the world steps at 120 until the 60 rate is here
	return 1;
}

int GetRagdollAllDeaths ( lua_State *L )
{
	lua_pushinteger ( L, 0 ); // neutral until its engine side is here
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

int GetWind(lua_State* L)
{
	lua_pushnumber(L, t.gamevisuals.wind_speed);
	lua_pushnumber(L, t.gamevisuals.wind_direction_x);
	lua_pushnumber(L, t.gamevisuals.wind_direction_y);
	lua_pushnumber(L, t.gamevisuals.wind_direction_z);
	lua_pushnumber(L, t.gamevisuals.wind_randomness);
	return 5;
}

int SetEntityAngularVelocity(lua_State *L) { return SetEntityVelocityCore ( L, 1 ); }

int SetEntityVelocity(lua_State *L) { return SetEntityVelocityCore ( L, 0 ); }

// SetEscapeMenu(on): 0 leaves Escape (and a controller's START) to the scripts, for a menu of their own: it no longer
// opens the in-game menu, nor leaves a test level, though shift and Escape still do that. 1 is the usual; back to 1 at
// each level start
int SetEscapeMenu ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	// neutral until its engine side is here: Escape keeps opening the in-game menu
	return 0;
}

// SetGrassDistance(d) - grass draw distance in world units, clamped to the editor slider range (750 to 7000).
// Changing it rebuilds all grass chunks in one frame, so set it on events rather than every frame.
int SetGrassDistance(lua_State* L)
{
	int n = LUA_GETTOP(L);
	if (n < 1) return 0;
	float fDistance = lua_tonumber(L, 1);
	if (fDistance < 750.0f) fDistance = 750.0f;
	if (fDistance > 7000.0f) fDistance = 7000.0f;
	// the restore at a test game's end waits for its engine side
	gggrass_global_params.lod_dist = fDistance;
	return 0;
}

// SetGunReloadWhileRunning(on) - 1 lets the player reload while running: the gun leaves its run animation for the reload, as
// it does to fire, and goes back to it after; 0 (at first, and when a test game ends) the stock rule, no reload until the
// player stops running
int SetGunReloadWhileRunning(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	// neutral until its engine side is here: the stock rule, no reload while running
	return 0;
}

// SetLightMaxDistance(distance): point and spot lights farther than this from the camera (their range added) are left out
// of the frame, as lights out of view are; 0, the default, for no limit. Back to none at the next level and test end
int SetLightMaxDistance(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	// neutral until its engine side is here: every light stays in the frame
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

// SetPhysicsWorldRate(rate) - 60 steps the physics world 60 times a second (half the work) while the player keeps its own
// 120 steps, as its controller moves a set distance each step; 120 (at first, at a test game's end, and for any other
// value) steps all of it at 120 as before
int SetPhysicsWorldRate(lua_State* L)
{
	if (LUA_GETTOP(L) < 1) return 0;
	// neutral until its engine side is here: the world keeps its 120 steps
	return 0;
}

// SetRagdollAllDeaths(on): 1 gives every death of a character with ragdoll = 1 the forced ragdoll of
// SetEntityHealth(e, -12345), skipping the death animations; SetEntityHealth(e, -12346) still has none. 0 at each level start
int SetRagdollAllDeaths ( lua_State *L )
{
	int n = LUA_GETTOP(L);
	if ( n < 1 ) return 0;
	// neutral until its engine side is here: deaths keep their animations
	return 0;
}

int SetRawSoundDistanceScale ( lua_State *L ) { return SetRawSoundData ( L, 6 ); }

// SetRenderTiming(mode): 1 times the renderer's work every frame (the ranges of Tab Tab's performance data) so GetRenderTime
// can read them, 2 the CPU ranges only (no GPU timestamp queries, so no GPU times), 0 stops. Off again at the next level
int SetRenderTiming ( lua_State *L )
{
	if ( LUA_GETTOP(L) < 1 ) return 0;
	int iMode = (int)lua_tointeger(L, 1);
	// this engine's profiler has no CPU-only mode, so 2 times the GPU as well
	wiProfiler::SetEnabled ( iMode != 0 );
	return 0;
}

// waiting for their engine side

// AddBlastDecal: neutral until its engine side is here; needs WickedCall_AddBlastDecal (wickedcalls.cpp, wickedcalls.h); WickedCall_FindDecalRayTargets (wickedcalls.cpp, wickedcalls.h); WickedCall_SetProjectedDecalOpacity (wickedcalls.cpp, wickedcalls.h)
int AddBlastDecal ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 2;
}

// AddBulletHole: neutral until its engine side is here; needs entity_allowsbulletholes (G-Entity.cpp, G-Entity.h, M-Entity.cpp)
int AddBulletHole ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// AddProjectedDecal: neutral until its engine side is here; needs WickedCall_AddProjectedDecal (wickedcalls.cpp, wickedcalls.h)
int AddProjectedDecal ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// BulletRay: neutral until its engine side is here; needs bHole (G-Entity.cpp, G-Entity.h); entity_scriptbulletray (G-Entity.cpp, G-Entity.h); iMaterial (BulletPhysics.CPP, BulletPhysics.H, G-Entity.cpp)
int BulletRay ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 10;
}

// ClearGrassKillBox: neutral until its engine side is here; needs GGGRASS_MAX_KILLBOXES (GGGrass.cpp, GGGrass.h, GGGrassConstants.hlsli); GGGrass_ClearKillBox (GGGrass.cpp, GGGrass.h, M-Game.cpp)
int ClearGrassKillBox ( lua_State* L )
{
	return 0;
}

// GetBloom: neutral until its engine side is here; needs LuaPostEffects_GetBloom (M-GridEditB.cpp)
int GetBloom ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 5;
}

// GetDecalStats: neutral until its engine side is here; needs g_iDecalDroppedFull (M-Decal.cpp)
int GetDecalStats ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 3;
}

// GetDelayedShadows: neutral until its engine side is here; needs LuaRenderSettings_GetDelayedShadows (M-GridEditB.cpp)
int GetDelayedShadows ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 3;
}

// GetDepthOfField: neutral until its engine side is here; needs LuaPostEffects_GetDepthOfField (M-GridEditB.cpp)
int GetDepthOfField ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 4;
}

// GetEngineProbe: neutral until its engine side is here; needs WickedCall_ProbeGet (wickedcalls.cpp, wickedcalls.h)
int GetEngineProbe ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 3;
}

// GetEngineProbeDetail: neutral until its engine side is here; needs WickedCall_ProbeDetail (wickedcalls.cpp, wickedcalls.h)
int GetEngineProbeDetail ( lua_State* L )
{
	lua_pushstring ( L, "" );
	return 1;
}

// GetEntityAxes: neutral until its engine side is here; needs vecForward (BulletPhysics.CPP)
int GetEntityAxes ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 6;
}

// GetEntityInstanceEmissive: neutral until its engine side is here; needs WickedCall_GetObjectEmissiveTint (wickedcalls.cpp, wickedcalls.h)
int GetEntityInstanceEmissive ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 4;
}

// GetEntityInstanceTint: neutral until its engine side is here; needs WickedCall_GetObjectColorTint (wickedcalls.cpp, wickedcalls.h)
int GetEntityInstanceTint ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 3;
}

// GetEntityLimbAlpha: neutral until its engine side is here; needs WickedCall_GetLimbAlpha (wickedcalls.cpp, wickedcalls.h)
int GetEntityLimbAlpha ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	return 1;
}

// GetEntityMass: neutral until its engine side is here; needs physicsmass (BulletPhysics.CPP, M-Entity.cpp, M-Physics.cpp)
int GetEntityMass ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 2;
}

// GetEntityRagdollHoldStiffness: neutral until its engine side is here; needs BPhys_RagDollGetHoldStiffness (BulletPhysics.CPP, BulletPhysics.H)
int GetEntityRagdollHoldStiffness ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 5;
}

// GetEntityRagdollPose: neutral until its engine side is here; needs ragdoll_getpose (M-Ragdoll.cpp, M-Ragdoll.h); ragdoll_isknockedout (M-Ragdoll.cpp, M-Ragdoll.h)
int GetEntityRagdollPose ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 8;
}

// GetFrameCosts: neutral until its engine side is here; needs WickedCallFrameCosts (wickedcalls.cpp, wickedcalls.h); WickedCall_GetFrameCosts (wickedcalls.cpp, wickedcalls.h); bVSync (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h); dExecute (wickedcalls.cpp, wickedcalls.h); dExecuteMax (wickedcalls.cpp, wickedcalls.h); dPresent (wickedcalls.cpp, wickedcalls.h); dPresentMax (wickedcalls.cpp, wickedcalls.h); dSubmit (wickedcalls.cpp, wickedcalls.h); dSubmitMax (wickedcalls.cpp, wickedcalls.h); dWindowMs (M-LUA.cpp, M-LUA.h, wickedcalls.cpp); iFrames (M-LUA.cpp, M-LUA.h, wickedcalls.cpp); iSubmits (wickedcalls.cpp, wickedcalls.h)
int GetFrameCosts ( lua_State* L )
{
	lua_pushnil ( L );
	return 1;
}

// GetGpuFrameCosts: neutral until its engine side is here; needs WickedCallGpuCosts (wickedcalls.cpp, wickedcalls.h); WickedCall_GetGpuFrameCosts (wickedcalls.cpp, wickedcalls.h); dGpu (wickedcalls.cpp, wickedcalls.h); dGpuMax (wickedcalls.cpp, wickedcalls.h); iFrames (M-LUA.cpp, M-LUA.h, wickedcalls.cpp); iMissed (wickedcalls.cpp, wickedcalls.h)
int GetGpuFrameCosts ( lua_State* L )
{
	lua_pushnil ( L );
	return 1;
}

// GetGraphicsSettings: neutral until its engine side is here; needs LuaRenderSettings_GetCulling (M-GridEditB.cpp); LuaRenderSettings_GetDisplay (M-GridEditB.cpp); LuaRenderSettings_GetLevers (M-GridEditB.cpp); LuaRenderSettings_GetMSAA (M-GridEditB.cpp); LuaRenderSettings_GetPicture (M-GridEditB.cpp); LuaRenderSettings_GetRenderScale (M-GridEditB.cpp); LuaRenderSettings_GetShadows (M-GridEditB.cpp); LuaRenderSettings_GetTerrainDetail (M-GridEditB.cpp); LuaRenderSettings_GetValues (M-GridEditB.cpp)
int GetGraphicsSettings ( lua_State* L )
{
	lua_newtable ( L );
	return 1;
}

// GetLensFlare: neutral until its engine side is here; needs LuaPostEffects_GetLensFlareSun (M-GridEditB.cpp)
int GetLensFlare ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 3;
}

// GetLuaEntityCosts: neutral until its engine side is here; needs dKey (M-LUA.cpp, M-LUA.h); dMain (M-LUA.cpp, M-LUA.h); dRow (M-LUA.cpp, M-LUA.h); dTotal (M-LUA.cpp, M-LUA.h); dWindowMs (M-LUA.cpp, M-LUA.h, wickedcalls.cpp); iFrames (M-LUA.cpp, M-LUA.h, wickedcalls.cpp); iGC (M-LUA.cpp, M-LUA.h); iKeyScannersMax (M-LUA.cpp, M-LUA.h); iKeyScans (M-LUA.cpp, M-LUA.h); iMains (M-LUA.cpp, M-LUA.h); iRows (M-LUA.cpp, M-LUA.h); iTurns (M-LUA.cpp, M-LUA.h); lua_getentitycosts (M-LUA.cpp, M-LUA.h)
int GetLuaEntityCosts ( lua_State* L )
{
	lua_pushnil ( L );
	return 1;
}

// GetPhysicsAwakeBodies: neutral until its engine side is here; needs PhysicsQuery_AwakeBodies (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int GetPhysicsAwakeBodies ( lua_State* L )
{
	lua_newtable ( L );
	lua_newtable ( L );
	lua_pushinteger ( L, 0 );
	return 3;
}

// GetPhysicsBodyPose: neutral until its engine side is here; needs ODEGetBodyPose (BulletPhysics.CPP, BulletPhysics.H)
int GetPhysicsBodyPose ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnil ( L );
	lua_pushnumber ( L, 0 );
	lua_pushnil ( L );
	lua_pushinteger ( L, 0 );
	lua_pushnil ( L );
	lua_pushnil ( L );
	return 7;
}

// GetPhysicsPushLog: neutral until its engine side is here; needs ODEGetPushLog (BulletPhysics.CPP)
int GetPhysicsPushLog ( lua_State* L )
{
	lua_newtable ( L );
	return 1;
}

// GetPhysicsSlowPairs: neutral until its engine side is here; needs PhysicsQuery_SlowPairs (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int GetPhysicsSlowPairs ( lua_State* L )
{
	lua_newtable ( L );
	return 1;
}

// GetPhysicsStats: neutral until its engine side is here; needs PhysicsQuery_Stats (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp); awakeBodies (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp); busiestObject (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp); busiestPoints (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp); subSteps (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int GetPhysicsStats ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 6;
}

// GetPhysicsStatsTop: neutral until its engine side is here; needs PHYSICS_LAYER_DYNAMIC (GGPhysicsQuery.h); PHYSICS_LAYER_STATIC (GGPhysicsQuery.h); PhysicsQuery_StatsTop (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp); sleepSpeed (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp); sleepSpin (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int GetPhysicsStatsTop ( lua_State* L )
{
	lua_newtable ( L );
	return 1;
}

// GetPlayerHit: neutral until its engine side is here; needs fOX (G-Entity.cpp, G-Entity.h); fOY (G-Entity.cpp, G-Entity.h); fOZ (G-Entity.cpp, G-Entity.h); iDamage (G-Entity.cpp, G-Entity.h); iHealthAfter (G-Entity.cpp, G-Entity.h); iHole (G-Entity.cpp, G-Entity.h); iKilled (G-Entity.cpp, G-Entity.h); iKind (G-Entity.cpp, G-Entity.h); iMaterial (BulletPhysics.CPP, BulletPhysics.H, G-Entity.cpp); iShot (G-Entity.cpp, G-Entity.h); playerhit_get (G-Entity.cpp, G-Entity.h)
int GetPlayerHit ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 21;
}

// GetPlayerHitSeq: neutral until its engine side is here; needs playerhit_getseq (G-Entity.cpp, G-Entity.h)
int GetPlayerHitSeq ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
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

// GetTreeTypeName: neutral until its engine side is here; needs GGTrees_GetTypeName (GGTrees.cpp, GGTrees.h)
int GetTreeTypeName ( lua_State* L )
{
	lua_pushstring ( L, "" );
	return 1;
}

// GetTreeVariety: neutral until its engine side is here; needs variety_mirror (GGTrees.cpp, GGTrees.h); variety_tint (GGTrees.cpp, GGTrees.h)
int GetTreeVariety ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 2;
}

// GetTreesNear: neutral until its engine side is here; needs GGTrees_GetTypeName (GGTrees.cpp, GGTrees.h); instanceScale (GGTrees.cpp, GGTrees.h)
int GetTreesNear ( lua_State* L )
{
	lua_newtable ( L );
	return 1;
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

// IntersectRay: neutral until its engine side is here; needs g_IntersectExtraIgnoreObjects (CObjectsC.cpp)
int IntersectRay ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushinteger ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushinteger ( L, 0 );
	return 9;
}

// PhysicsOverlapBox: neutral until its engine side is here; needs GGTrees_OverlapBoxTrunks (GGTrees.cpp, GGTrees.h); PHYSICS_LAYER_DEFAULT (GGPhysicsQuery.h); PHYSICS_QUERY_MAX_IGNORE (GGPhysicsQuery.h); PhysicsQuery_OverlapBox (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int PhysicsOverlapBox ( lua_State* L )
{
	return 0;
}

// PhysicsRay: neutral until its engine side is here; needs PHYSICS_LAYER_DEFAULT (GGPhysicsQuery.h); PHYSICS_QUERY_MAX_IGNORE (GGPhysicsQuery.h); PhysicsQuery_Ray (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int PhysicsRay ( lua_State* L )
{
	return 0;
}

// PhysicsSweepBox: neutral until its engine side is here; needs GGTrees_SweepBoxTrunks (GGTrees.cpp, GGTrees.h); PHYSICS_LAYER_DEFAULT (GGPhysicsQuery.h); PHYSICS_QUERY_MAX_IGNORE (GGPhysicsQuery.h); PhysicsQuery_SweepBox (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int PhysicsSweepBox ( lua_State* L )
{
	return 0;
}

// RayTrees: neutral until its engine side is here; needs GGTrees_RayCastTrunks (GGTrees.cpp, GGTrees.h, M-Physics.cpp)
int RayTrees ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnil ( L );
	lua_pushnil ( L );
	lua_pushnil ( L );
	lua_pushnil ( L );
	lua_pushnil ( L );
	return 7;
}

// RemoveProjectedDecal: neutral until its engine side is here; needs WickedCall_RemoveProjectedDecal (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int RemoveProjectedDecal ( lua_State* L )
{
	return 0;
}

// ResetDelayedShadows: neutral until its engine side is here; needs LuaRenderSettings_ResetDelayedShadows (M-GridEditB.cpp)
int ResetDelayedShadows ( lua_State* L )
{
	return 0;
}

// ResetEntityMaterial: neutral until its engine side is here; needs entity_resetmaterial (G-Entity.cpp, G-Entity.h)
int ResetEntityMaterial ( lua_State* L )
{
	return 0;
}

// ResetGraphicsSettings: neutral until its engine side is here; needs LuaRenderSettings_ResetGraphics (M-GridEditB.cpp)
int ResetGraphicsSettings ( lua_State* L )
{
	return 0;
}

// SetAO: neutral until its engine side is here; needs LuaRenderSettings_SetAO (M-GridEditB.cpp)
int SetAO ( lua_State* L )
{
	return 0;
}

// SetAnimations30Fps: neutral until its engine side is here; needs LuaRenderSettings_SetAnimations30Fps (M-GridEditB.cpp)
int SetAnimations30Fps ( lua_State* L )
{
	return 0;
}

// SetBloom: neutral until its engine side is here; needs LuaPostEffects_SetBloom (M-GridEditB.cpp)
int SetBloom ( lua_State* L )
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

// SetDelayedShadows: neutral until its engine side is here; needs LuaRenderSettings_SetDelayedShadows (M-GridEditB.cpp)
int SetDelayedShadows ( lua_State* L )
{
	return 0;
}

// SetDepthOfField: neutral until its engine side is here; needs LuaPostEffects_SetDepthOfField (M-GridEditB.cpp)
int SetDepthOfField ( lua_State* L )
{
	return 0;
}

// SetEntityInstanceEmissive: neutral until its engine side is here; needs WickedCall_SetObjectEmissiveTint (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int SetEntityInstanceEmissive ( lua_State* L )
{
	return 0;
}

// SetEntityInstanceTint: neutral until its engine side is here; needs WickedCall_SetObjectColorTint (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int SetEntityInstanceTint ( lua_State* L )
{
	return 0;
}

// SetEntityLimbAlpha: neutral until its engine side is here; needs WickedCall_SetLimbAlpha (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int SetEntityLimbAlpha ( lua_State* L )
{
	return 0;
}

// SetEntityRagdoll: neutral until its engine side is here; needs ragdoll_knockout (M-Ragdoll.cpp, M-Ragdoll.h); ragdoll_wake (M-Ragdoll.cpp, M-Ragdoll.h)
int SetEntityRagdoll ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// SetEntityRagdollHold: neutral until its engine side is here; needs ragdoll_hold (M-Ragdoll.cpp, M-Ragdoll.h); ragdoll_release (M-Ragdoll.cpp, M-Ragdoll.h)
int SetEntityRagdollHold ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// SetEntityRagdollHoldReference: neutral until its engine side is here; needs ragdoll_setholdreference (M-Ragdoll.cpp, M-Ragdoll.h)
int SetEntityRagdollHoldReference ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// SetEntityRagdollHoldStiffness: neutral until its engine side is here; needs BPhys_RagDollGetHoldStiffness (BulletPhysics.CPP, BulletPhysics.H); BPhys_RagDollSetHoldStiffness (BulletPhysics.CPP, BulletPhysics.H)
int SetEntityRagdollHoldStiffness ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// SetEntityReceivesDecals: neutral until its engine side is here; needs WickedCall_SetObjectReceivesDecals (G-Gun.cpp, M-HUD.cpp, wickedcalls.cpp)
int SetEntityReceivesDecals ( lua_State* L )
{
	return 0;
}

// SetFSRSharpness: neutral until its engine side is here; needs LuaRenderSettings_SetFSRSharpness (M-GridEditB.cpp)
int SetFSRSharpness ( lua_State* L )
{
	return 0;
}

// SetFXAA: neutral until its engine side is here; needs LuaRenderSettings_SetFXAA (M-GridEditB.cpp)
int SetFXAA ( lua_State* L )
{
	return 0;
}

// SetFrameRateCap: neutral until its engine side is here; needs LuaRenderSettings_SetFrameRateCap (M-GridEditB.cpp)
int SetFrameRateCap ( lua_State* L )
{
	return 0;
}

// SetGamePaused: neutral until its engine side is here; needs game_setgamepaused (M-Game.cpp)
int SetGamePaused ( lua_State* L )
{
	return 0;
}

// SetGamma: neutral until its engine side is here; needs LuaRenderSettings_SetGamma (M-GridEditB.cpp)
int SetGamma ( lua_State* L )
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

// SetGrassSimpleLighting: neutral until its engine side is here; needs LuaRenderSettings_SetGrassSimpleLighting (M-GridEditB.cpp)
int SetGrassSimpleLighting ( lua_State* L )
{
	return 0;
}

// SetGrassTightBounds: neutral until its engine side is here; needs GGGrass_SetTightBounds (GGGrass.cpp, GGGrass.h, M-GridEditB.cpp)
int SetGrassTightBounds ( lua_State* L )
{
	return 0;
}

// SetLODMultiplier: neutral until its engine side is here; needs LuaRenderSettings_SetLODMultiplier (M-GridEditB.cpp)
int SetLODMultiplier ( lua_State* L )
{
	return 0;
}

// SetLensFlare: neutral until its engine side is here; needs LuaPostEffects_SetLensFlare (M-GridEditB.cpp)
int SetLensFlare ( lua_State* L )
{
	return 0;
}

// SetLightShafts: neutral until its engine side is here; needs LuaPostEffects_SetLightShafts (M-GridEditB.cpp)
int SetLightShafts ( lua_State* L )
{
	return 0;
}

// SetMSAA: neutral until its engine side is here; needs LuaRenderSettings_SetMSAA (M-GridEditB.cpp)
int SetMSAA ( lua_State* L )
{
	return 0;
}

// SetOcclusionCulling: neutral until its engine side is here; needs LuaRenderSettings_SetOcclusionCulling (M-GridEditB.cpp); LuaRenderSettings_SetSpotShadowCulling (M-GridEditB.cpp)
int SetOcclusionCulling ( lua_State* L )
{
	return 0;
}

// SetParticleHalfResolution: neutral until its engine side is here; needs WickedCall_SetParticleHalfResolution (wickedcalls.cpp, wickedcalls.h)
int SetParticleHalfResolution ( lua_State* L )
{
	return 0;
}

// SetPhysicsPushLog: neutral until its engine side is here; needs ODESetPushLog (BulletPhysics.CPP)
int SetPhysicsPushLog ( lua_State* L )
{
	return 0;
}

// SetProbesLowestLOD: neutral until its engine side is here; needs LuaRenderSettings_SetLowestLOD (M-GridEditB.cpp)
int SetProbesLowestLOD ( lua_State* L )
{
	return 0;
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

// SetRaycastLowestLOD: neutral until its engine side is here; needs LuaRenderSettings_SetRaycastLowestLOD (M-GridEditB.cpp)
int SetRaycastLowestLOD ( lua_State* L )
{
	return 0;
}

// SetReflections: neutral until its engine side is here; needs LuaRenderSettings_SetReflections (M-GridEditB.cpp)
int SetReflections ( lua_State* L )
{
	return 0;
}

// SetReflectionsLowestLOD: neutral until its engine side is here; needs LuaRenderSettings_SetLowestLOD (M-GridEditB.cpp)
int SetReflectionsLowestLOD ( lua_State* L )
{
	return 0;
}

// SetRenderScale: neutral until its engine side is here; needs LuaRenderSettings_SetRenderScale (M-GridEditB.cpp)
int SetRenderScale ( lua_State* L )
{
	return 0;
}

// SetRippleBatching: neutral until its engine side is here; needs decal_setripplebatching (M-Decal.cpp, M-GridEditB.cpp)
int SetRippleBatching ( lua_State* L )
{
	return 0;
}

// SetSSR: neutral until its engine side is here; needs LuaRenderSettings_SetSSR (M-GridEditB.cpp)
int SetSSR ( lua_State* L )
{
	return 0;
}

// SetShadowCascades: neutral until its engine side is here; needs LuaRenderSettings_SetShadowCascades (M-GridEditB.cpp)
int SetShadowCascades ( lua_State* L )
{
	return 0;
}

// SetShadowLights: neutral until its engine side is here; needs LuaRenderSettings_SetShadowLights (M-GridEditB.cpp)
int SetShadowLights ( lua_State* L )
{
	return 0;
}

// SetShadowRange: neutral until its engine side is here; needs LuaRenderSettings_SetShadowRange (M-GridEditB.cpp)
int SetShadowRange ( lua_State* L )
{
	return 0;
}

// SetShadowResolution: neutral until its engine side is here; needs LuaRenderSettings_SetShadowResolution (M-GridEditB.cpp)
int SetShadowResolution ( lua_State* L )
{
	return 0;
}

// SetShadowsLowestLOD: neutral until its engine side is here; needs LuaRenderSettings_SetShadowsLowestLOD (M-GridEditB.cpp)
int SetShadowsLowestLOD ( lua_State* L )
{
	return 0;
}

// SetSunShadowDirection: neutral until its engine side is here; needs WickedCall_SetSunShadowDirection (M-GridEditB.cpp, wickedcalls.cpp, wickedcalls.h)
int SetSunShadowDirection ( lua_State* L )
{
	return 0;
}

// SetTerrainDetail: neutral until its engine side is here; needs LuaRenderSettings_SetTerrainDetail (M-GridEditB.cpp)
int SetTerrainDetail ( lua_State* L )
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

// SetTransparentShadows: neutral until its engine side is here; needs LuaRenderSettings_SetTransparentShadows (M-GridEditB.cpp)
int SetTransparentShadows ( lua_State* L )
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

// SetVsync: neutral until its engine side is here; needs LuaRenderSettings_SetVsync (M-GridEditB.cpp)
int SetVsync ( lua_State* L )
{
	return 0;
}

// SleepEntity: neutral until its engine side is here; needs PhysicsQuery_SleepIsland (GGPhysicsQuery.h, GGPhysicsQuery_Bullet.cpp)
int SleepEntity ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// WParticleEffectClearKillBoxes: neutral until its engine side is here; needs killbox_count ()
int WParticleEffectClearKillBoxes ( lua_State* L )
{
	return 0;
}

// WParticleEffectGetBounds: neutral until its engine side is here; needs EMITTER_VOLUME ()
int WParticleEffectGetBounds ( lua_State* L )
{
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	lua_pushnumber ( L, 0 );
	return 6;
}

// WParticleEffectSetKillBox: neutral until its engine side is here; needs EMITTER_KILLBOX_COUNT (ShaderInterop_EmittedParticle.h); killbox_count (); killbox_max (); killbox_min ()
int WParticleEffectSetKillBox ( lua_State* L )
{
	return 0;
}

// WParticleEffectSetLayer: neutral until its engine side is here; needs count_scale (); emissive_scale (); layer_size_scale (); life_scale (); speed_scale (); spread_scale ()
int WParticleEffectSetLayer ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// WParticleEffectSetLayerWind: neutral until its engine side is here; needs layer_wind ()
int WParticleEffectSetLayerWind ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
	return 1;
}

// WParticleEffectSetOpacity: neutral until its engine side is here; needs opacity_scale ()
int WParticleEffectSetOpacity ( lua_State* L )
{
	return 0;
}

// WParticleEffectSetSize: neutral until its engine side is here; needs size_scale ()
int WParticleEffectSetSize ( lua_State* L )
{
	return 0;
}

// WParticleEffectSetWrapMotion: neutral until its engine side is here; needs bWrapMotion (); wrap_motion ()
int WParticleEffectSetWrapMotion ( lua_State* L )
{
	lua_pushinteger ( L, 0 );
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
	lua_register(lua2, "SetLightShadow", SetLightShadow);
	lua_register(lua2, "SetLightShafts", SetLightShafts);
	lua_register(lua2, "SetMSAA", SetMSAA);
	lua_register(lua2, "SetOcclusionCulling", SetOcclusionCulling);
	lua_register(lua2, "SetParticleHalfResolution", SetParticleHalfResolution);
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
}
