// LUA command names

// GGMAX 3.40: Lua 5.3/5.4 removed maths functions that the SHIPPED SCRIPTS still call. In
// Scripts/scriptbank, 21 files call math.atan2 (17 of them in the deployed set), and each faults
// the moment it executes:
//     scriptbank/global.lua:419: attempt to call a nil value (field 'atan2')
//
// That error is a MODAL MessageBox, which parks the whole application at zero CPU - it is why
// entering Test Level appeared to freeze. See NIGHT_INVESTIGATIONS 3.39 / 3.40.
//
// Lua ships a compatibility switch for exactly this, but it is NOT enabled: luaconf.h nests
// LUA_COMPAT_MATHLIB inside an #if defined(LUA_COMPAT_5_3), and LUA_COMPAT_5_3 is defined
// nowhere - not in luaconf.h, not in the vcxproj. Rather than edit WickedEngine's vendored Lua
// (an engine pull reverts it) or rewrite 24 content files (the next DX11 script import undoes
// it), alias the removed names once, here, before any script can run.
//
// The aliases are exact, not approximations:
//     5.4 math.atan(y [, x]) computes atan(y/x) using the signs of both -> identical to atan2.
//     math.mod was renamed math.fmod back in 5.1; same function.
// Each is assigned only if absent, so a future Lua that restores them wins. math.atan2 is the only
// GGMAX 3.48: the 3.40 comment here claimed math.atan2 was the only removed name any script
// uses. That was WRONG - it scanned math.* names and missed an entire LIBRARY. Lua 5.2 shipped
// lbitlib.c and DX11 still does; 5.4 removed bit32, and scriptbank/perlin_noise.lua binds
// bit32.band at FILE SCOPE while gameplayercontrol.lua requires it and is added to EVERY level.
// Only an UNTRACKED hand edit in the deploy tree was masking it. mod/pow/log10/unpack are the
// rest of what LUA_COMPAT_MATHLIB would have restored, and cost nothing to carry.
//
// GGMAX 3.124: DX11's own Lua 5.2 (DarkLUA\lua) opened every file by name through GG_fopen -
// luaL_loadfilex (every LoadLua, dofile, loadfile), io.open, io.lines/input/output and require's
// searchpath - so a script that lives only in the writable area (Documents\GameGuruApps\
// GameGuruMAX\Files, where add-on packs such as "Cine Guru MAX" install) was found. The engine's
// Lua 5.4.8 used plain fopen relative to the install Files folder: "cannot open scriptbank\Cine
// Guru MAX\cg_actor.lua: No such file or directory" on Test Level. The engine's Lua now calls
// ggLua_fopenhook when it is set (WICKED_ENGINE_CHANGES 3.124); setting it here, before any script
// runs, restores the DX11 lookup - writable area first, then the install copy, and writes ('w'/'a')
// land in the writable area as they did in DX11.
extern "C" FILE* GG_fopen( const char* filename, const char* mode );
static void GGLua_InstallCompatShim(lua_State* L)
{
	if (!L) return;
	ggLua_fopenhook = GG_fopen;
	// one line: Lua separates statements with whitespace, so no newline escapes are needed
	const char* shim =
		"if math then "
		"  if math.atan2 == nil and math.atan ~= nil then math.atan2 = math.atan end "
		"  if math.mod   == nil and math.fmod ~= nil then math.mod   = math.fmod end "
		"  if math.pow   == nil then math.pow = function(a,b) return a^b end end "
		"  if math.log10 == nil and math.log  ~= nil then math.log10 = function(x) return math.log(x,10) end end "
		"end "
		"if unpack == nil and table and table.unpack then unpack = table.unpack end "
		"if bit32 == nil then "
		"  local M = 0xFFFFFFFF "
		"  local function n32(x) return math.floor(x) & M end "
		"  bit32 = {} "
		"  bit32.band   = function(...) local r = M for _,v in ipairs({...}) do r = r & n32(v) end return r & M end "
		"  bit32.bor    = function(...) local r = 0 for _,v in ipairs({...}) do r = r | n32(v) end return r & M end "
		"  bit32.bxor   = function(...) local r = 0 for _,v in ipairs({...}) do r = r ~ n32(v) end return r & M end "
		"  bit32.bnot   = function(a) return (~n32(a)) & M end "
		"  bit32.lshift = function(a,b) if b < -31 or b > 31 then return 0 end if b >= 0 then return (n32(a) << b) & M else return (n32(a) >> -b) & M end end "
		"  bit32.rshift = function(a,b) if b < -31 or b > 31 then return 0 end if b >= 0 then return (n32(a) >> b) & M else return (n32(a) << -b) & M end end "
		"  bit32.arshift= function(a,b) local x = n32(a) if b <= 0 then return bit32.lshift(x,-b) end if (x & 0x80000000) ~= 0 then if b > 31 then return M end return ((x >> b) | (~(M >> b))) & M end return (x >> b) & M end "
		"  bit32.btest  = function(...) return bit32.band(...) ~= 0 end "
		"end ";
	if (luaL_loadbuffer(L, shim, strlen(shim), "gg_lua54_compat") == 0)
	{
		if (lua_pcall(L, 0, 0, 0) != 0)
		{
			// never fatal - a broken shim must not take the game down with it
			if (lua_isstring(L, -1)) timestampactivity(0, (char*)lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
}

void addFunctions_hiredgun();

void addFunctions()
{
	// add internal commands
	addInternalFunctions_nones();
	addInternalFunctions_integer();
	addInternalFunctions_float();
	addInternalFunctions_string();

	// add extra commands (old method)
	lua_register(lua2, "SendMessage", LuaSendMessage);
	lua_register(lua2, "SendMessageI", LuaSendMessageI);
	lua_register(lua2, "SendMessageF", LuaSendMessageF);
	lua_register(lua2, "SendMessageS", LuaSendMessageS);
	lua_register(lua2, "Include", Include);

	// Direct Calls
	lua_register(lua2, "RestoreGameFromSlot", RestoreGameFromSlot);
	lua_register(lua2, "ResetFade", ResetFade);

	lua_register(lua2, "GetInternalSoundState", GetInternalSoundState);
	lua_register(lua2, "SetInternalSoundState", SetInternalSoundState);
	lua_register(lua2, "SetCheckpoint", SetCheckpoint);

	lua_register(lua2, "UpdateWeaponStats", UpdateWeaponStats);
	lua_register(lua2, "ResetWeaponSystems", ResetWeaponSystems);
	lua_register(lua2, "GetWeaponSlotGot", GetWeaponSlotGot);
	lua_register(lua2, "GetWeaponSlotNoSelect", GetWeaponSlotNoSelect);
	lua_register(lua2, "SetWeaponSlot", SetWeaponSlot);

	lua_register(lua2, "GetWeaponAmmo", GetWeaponAmmo);
	lua_register(lua2, "SetWeaponAmmo", SetWeaponAmmo);
	lua_register(lua2, "GetWeaponClipAmmo", GetWeaponClipAmmo);
	lua_register(lua2, "SetWeaponClipAmmo", SetWeaponClipAmmo);
	lua_register(lua2, "GetWeaponPoolAmmoIndex", GetWeaponPoolAmmoIndex);
	lua_register(lua2, "GetWeaponPoolAmmo", GetWeaponPoolAmmo);
	lua_register(lua2, "SetWeaponPoolAmmo", SetWeaponPoolAmmo);

	lua_register(lua2, "GetWeaponSlot", GetWeaponSlot);
	lua_register(lua2, "GetWeaponSlotPref", GetWeaponSlotPref);
	lua_register(lua2, "GetPlayerWeaponID", GetPlayerWeaponID);
	lua_register(lua2, "GetWeaponID", GetWeaponID);
	lua_register(lua2, "GetEntityWeaponID", GetEntityWeaponID);
	lua_register(lua2, "GetWeaponName", GetWeaponName);
	lua_register(lua2, "SetWeaponDamage", SetWeaponDamage);
	lua_register(lua2, "SetWeaponAccuracy", SetWeaponAccuracy);
	lua_register(lua2, "SetWeaponReloadQuantity", SetWeaponReloadQuantity);
	lua_register(lua2, "SetWeaponFireIterations", SetWeaponFireIterations);
	lua_register(lua2, "SetWeaponRange", SetWeaponRange);
	lua_register(lua2, "SetWeaponDropoff", SetWeaponDropoff);
	lua_register(lua2, "SetWeaponSpotLighting", SetWeaponSpotLighting);
	lua_register(lua2, "GetWeaponDamage", GetWeaponDamage);
	lua_register(lua2, "GetWeaponAccuracy", GetWeaponAccuracy);
	lua_register(lua2, "GetWeaponReloadQuantity", GetWeaponReloadQuantity);
	lua_register(lua2, "GetWeaponFireIterations", GetWeaponFireIterations);
	lua_register(lua2, "GetWeaponRange", GetWeaponRange);
	lua_register(lua2, "GetWeaponDropoff", GetWeaponDropoff);
	lua_register(lua2, "GetWeaponSpotLighting", GetWeaponSpotLighting);
	lua_register(lua2, "SetWeaponFireRate", SetWeaponFireRate);
	lua_register(lua2, "GetWeaponFireRate", GetWeaponFireRate);
	lua_register(lua2, "SetWeaponClipCapacity", SetWeaponClipCapacity);
	lua_register(lua2, "GetWeaponClipCapacity", GetWeaponClipCapacity);

	lua_register(lua2, "WrapAngle", WrapAngle);
	lua_register(lua2, "GetCameraOverride", GetCameraOverride);
	lua_register(lua2, "SetCameraOverride", SetCameraOverride);
	lua_register(lua2, "SetCameraFOV", SetCameraFOV);
	lua_register(lua2, "SetCameraPosition", SetCameraPosition);
	lua_register(lua2, "SetCameraAngle", SetCameraAngle);
	lua_register(lua2, "SetCameraFreeFlight", SetCameraFreeFlight);
	lua_register(lua2, "GetCameraPositionX", GetCameraPositionX);
	lua_register(lua2, "GetCameraPositionY", GetCameraPositionY);
	lua_register(lua2, "GetCameraPositionZ", GetCameraPositionZ);
	lua_register(lua2, "GetCameraAngleX", GetCameraAngleX);
	lua_register(lua2, "GetCameraAngleY", GetCameraAngleY);
	lua_register(lua2, "GetCameraAngleZ", GetCameraAngleZ);
	lua_register(lua2, "GetCameraLookAtX", GetCameraLookAtX);
	lua_register(lua2, "GetCameraLookAtY", GetCameraLookAtY);
	lua_register(lua2, "GetCameraLookAtZ", GetCameraLookAtZ);

	lua_register(lua2, "ForcePlayer", ForcePlayer);

	lua_register(lua2, "SetEntityActive", SetEntityActive);
	lua_register(lua2, "SetEntityActivated", SetEntityActivated);
	lua_register(lua2, "SetEntityObjective", SetEntityObjective);
	lua_register(lua2, "SetEntityCollectable", SetEntityCollectable);
	lua_register(lua2, "SetEntityCollected", SetEntityCollected);
	lua_register(lua2, "SetEntityCollectedForce", SetEntityCollectedForce);
	lua_register(lua2, "SetEntityUsed", SetEntityUsed);
	lua_register(lua2, "GetEntityObjective", GetEntityObjective);
	lua_register(lua2, "GetEntityExplodable", GetEntityExplodable);
	lua_register(lua2, "SetEntityExplodable", SetEntityExplodable);
	lua_register(lua2, "SetExplosionHeight", SetExplosionHeight);
	lua_register(lua2, "SetExplosionDamage", SetExplosionDamage);
	lua_register(lua2, "SetCustomExplosion", SetCustomExplosion);
	
	
	lua_register(lua2, "GetEntityProjectGlobal", GetEntityProjectGlobal);
	lua_register(lua2, "GetEntityCollectable", GetEntityCollectable);
	lua_register(lua2, "GetEntityCollected", GetEntityCollected);
	lua_register(lua2, "GetEntityUsed", GetEntityUsed);
	lua_register(lua2, "SetEntityQuantity", SetEntityQuantity);
	lua_register(lua2, "GetEntityQuantity", GetEntityQuantity);
	lua_register(lua2, "SetEntityHasKey", SetEntityHasKey);
	lua_register(lua2, "GetEntityActive", GetEntityActive);
	lua_register(lua2, "GetEntityWhoActivated", GetEntityWhoActivated);
	lua_register(lua2, "GetEntityVisibility", GetEntityVisibility);
	lua_register(lua2, "GetEntityPositionX", GetEntityPositionX);
	lua_register(lua2, "GetEntityPositionY", GetEntityPositionY);
	lua_register(lua2, "GetEntityPositionZ", GetEntityPositionZ);
	lua_register(lua2, "GetEntityCollBox", GetEntityCollBox);
	lua_register(lua2, "GetEntityColBox", GetEntityColBox);
	lua_register(lua2, "GetEntityPosAng", GetEntityPosAng);
	lua_register(lua2, "GetEntityWeight", GetEntityWeight);
	lua_register(lua2, "GetEntityScales", GetEntityScales);
	lua_register(lua2, "GetEntityName", GetEntityName);
	lua_register(lua2, "GetEntityAngleX", GetEntityAngleX);
	lua_register(lua2, "GetEntityAngleY", GetEntityAngleY);
	lua_register(lua2, "GetEntityAngleZ", GetEntityAngleZ);
	lua_register(lua2, "GetMovementSpeed", GetMovementSpeed);
	lua_register(lua2, "GetAnimationSpeed", GetAnimationSpeed);
	lua_register(lua2, "SetAnimationSpeedModulation", SetAnimationSpeedModulation);
	lua_register(lua2, "GetAnimationSpeedModulation", GetAnimationSpeedModulation);
	lua_register(lua2, "GetMovementDelta", GetMovementDelta);
	lua_register(lua2, "GetMovementDeltaManually", GetMovementDeltaManually);
	lua_register(lua2, "GetEntityMarkerMode", GetEntityMarkerMode);
	lua_register(lua2, "SetEntityAlwaysActive", SetEntityAlwaysActive);
	lua_register(lua2, "GetEntityAlwaysActive", GetEntityAlwaysActive);
	lua_register(lua2, "SetEntityUnderwaterMode", SetEntityUnderwaterMode);
	lua_register(lua2, "GetEntityUnderwaterMode", GetEntityUnderwaterMode);
	lua_register(lua2, "GetEntityParentID", GetEntityParentID);

	lua_register(lua2, "SetEntityIfUsed", SetEntityIfUsed);
	lua_register(lua2, "GetEntityIfUsed", GetEntityIfUsed);

	#ifdef WICKEDENGINE
	lua_register(lua2, "SetEntityAllegiance", SetEntityAllegiance);
	lua_register(lua2, "GetEntityAllegiance", GetEntityAllegiance);
	lua_register(lua2, "GetEntityPatrolMode", GetEntityPatrolMode);
	lua_register(lua2, "GetEntityRelationshipID", GetEntityRelationshipID);
	lua_register(lua2, "GetEntityRelationshipType", GetEntityRelationshipType);
	lua_register(lua2, "GetEntityRelationshipData", GetEntityRelationshipData);
	lua_register(lua2, "GetEntityCanFire", GetEntityCanFire);
	lua_register(lua2, "GetEntityHasWeapon", GetEntityHasWeapon);
	lua_register(lua2, "GetEntityViewAngle", GetEntityViewAngle);
	lua_register(lua2, "GetEntityViewRange", GetEntityViewRange);
	lua_register(lua2, "SetEntityViewRange", SetEntityViewRange);
	lua_register(lua2, "GetEntityMoveSpeed", GetEntityMoveSpeed);
	lua_register(lua2, "GetEntityTurnSpeed", GetEntityTurnSpeed);
	lua_register(lua2, "SetEntityMoveSpeed", SetEntityMoveSpeed);
	lua_register(lua2, "SetEntityTurnSpeed", SetEntityTurnSpeed);
	#endif

	#ifdef WICKEDENGINE
	lua_register(lua2, "GetEntitiesWithinCone", GetEntitiesWithinCone);
	lua_register(lua2, "GetEntityWithinCone", GetEntityWithinCone);
	#endif

	#ifdef WICKEDENGINE
	lua_register(lua2, "GetNearestEntityDestroyed", GetNearestEntityDestroyed);
	lua_register(lua2, "GetNearestSoundDistance", GetNearestSoundDistance);
	lua_register(lua2, "MakeAISound", MakeAISound);
	#endif
	
	#ifdef WICKEDENGINE
	lua_register(lua2, "GetTerrainEditableArea", GetTerrainEditableArea);
	#endif

	lua_register(lua2, "SetEntityString", SetEntityString);
	lua_register(lua2, "GetEntityString", GetEntityString);

	lua_register(lua2, "GetLimbName", GetLimbName);

	lua_register(lua2, "SetEntitySpawnAtStart", SetEntitySpawnAtStart);
	lua_register(lua2, "GetEntitySpawnAtStart", GetEntitySpawnAtStart);
	lua_register(lua2, "GetEntityFilePath", GetEntityFilePath);
	lua_register(lua2, "GetEntityClonedSinceStartValue", GetEntityClonedSinceStartValue);
	lua_register(lua2, "SetPreExitValue", SetPreExitValue);

	lua_register(lua2, "SetEntityAnimation", SetEntityAnimation);
	lua_register(lua2, "GetEntityAnimationStart", GetEntityAnimationStart);
	lua_register(lua2, "GetEntityAnimationFinish", GetEntityAnimationFinish);
	lua_register(lua2, "GetEntityAnimationFound", GetEntityAnimationFound);
	lua_register(lua2, "GetObjectAnimationFinished", GetObjectAnimationFinished);

	lua_register(lua2, "AdjustLookSettingHorizLimit", AdjustLookSettingHorizLimit);
	lua_register(lua2, "AdjustLookSettingHorizOffset", AdjustLookSettingHorizOffset);
	lua_register(lua2, "AdjustLookSettingVertLimit", AdjustLookSettingVertLimit);
	lua_register(lua2, "AdjustLookSettingVertOffset", AdjustLookSettingVertOffset);
	lua_register(lua2, "AdjustAimSettingHorizLimit", AdjustAimSettingHorizLimit);
	lua_register(lua2, "AdjustAimSettingHorizOffset", AdjustAimSettingHorizOffset);
	lua_register(lua2, "AdjustAimSettingVertLimit", AdjustAimSettingVertLimit);
	lua_register(lua2, "AdjustAimSettingVertOffset", AdjustAimSettingVertOffset);

	lua_register(lua2, "GetEntityFootfallMax", GetEntityFootfallMax);
	lua_register(lua2, "GetEntityFootfallKeyframe", GetEntityFootfallKeyframe);
	lua_register(lua2, "GetEntityAnimationNameExist", GetEntityAnimationNameExist);
	lua_register(lua2, "GetEntityAnimationNameExistAndPlaying", GetEntityAnimationNameExistAndPlaying);
	lua_register(lua2, "GetEntityAnimationNameExistAndPlayingSearchAny", GetEntityAnimationNameExistAndPlayingSearchAny);
	lua_register(lua2, "GetEntityAnimationTriggerFrame", GetEntityAnimationTriggerFrame);
	lua_register(lua2, "GetEntityAnimationStartFinish", GetEntityAnimationStartFinish);
	
	// Entity creation/destruction
	lua_register(lua2, "GetOriginalEntityElementMax", GetOriginalEntityElementMax);
	lua_register(lua2, "CreateEntityIfNotPresent", CreateEntityIfNotPresent);
	lua_register(lua2, "SpawnNewEntity", SpawnNewEntity);
	lua_register(lua2, "DeleteNewEntity", DeleteNewEntity);

	lua_register(lua2, "GetAmmoClipMax", GetAmmoClipMax);
	lua_register(lua2, "GetAmmoClip", GetAmmoClip);
	lua_register(lua2, "SetAmmoClip", SetAmmoClip);

	// Entity Physics
	lua_register(lua2, "FreezeEntity", FreezeEntity);
	lua_register(lua2, "UnFreezeEntity", UnFreezeEntity);

	// Terrain
	lua_register(lua2, "GetTerrainHeight", GetTerrainHeight);
	lua_register(lua2, "GetSurfaceHeight", GetSurfaceHeight);
	lua_register(lua2, "SetPlayerSlopeAngle", SetPlayerSlopeAngle);
	lua_register(lua2, "GetTerrainHeightFloat", GetTerrainHeightFloat);

	// DarkAI - Legacy Automatic Mode
	lua_register(lua2, "AIEntityAssignPatrolPath" , AIEntityAssignPatrolPath );
	lua_register(lua2, "AIEntityAddTarget" , AIEntityAddTarget );
	lua_register(lua2, "AIEntityRemoveTarget" , AIEntityRemoveTarget );
	lua_register(lua2, "AIEntityMoveToCover" , AIEntityMoveToCover );
	lua_register(lua2, "AIGetEntityCanFire" , AIGetEntityCanFire );

	// DarkAI - New Manual Mode Control
	lua_register(lua2, "AISetEntityControl", AISetEntityControl);
	lua_register(lua2, "AIEntityStop" , AIEntityStop );
	lua_register(lua2, "AIGetEntityCanSee" , AIGetEntityCanSee );
	lua_register(lua2, "AIGetEntityViewRange" , AIGetEntityViewRange );
	lua_register(lua2, "AIGetEntitySpeed" , AIGetEntitySpeed );
	lua_register(lua2, "AISetEntityMoveBoostPriority" , AISetEntityMoveBoostPriority );
	lua_register(lua2, "AIEntityGoToPosition" , AIEntityGoToPosition );
	lua_register(lua2, "AIGetEntityHeardSound" , AIGetEntityHeardSound );
	lua_register(lua2, "AISetEntityPosition" , AISetEntityPosition );
	lua_register(lua2, "AISetEntityTurnSpeed" , AISetEntityTurnSpeed );
	lua_register(lua2, "AIGetEntityAngleY" , AIGetEntityAngleY );
	lua_register(lua2, "AIGetEntityIsMoving" , AIGetEntityIsMoving );

	lua_register(lua2, "AIGetTotalPaths" , AIGetTotalPaths );
	lua_register(lua2, "AIGetPathCountPoints" , AIGetPathCountPoints );
	lua_register(lua2, "AIPathGetPointX" , AIPathGetPointX );
	lua_register(lua2, "AIPathGetPointY" , AIPathGetPointY );
	lua_register(lua2, "AIPathGetPointZ" , AIPathGetPointZ );

	lua_register(lua2, "AIGetTotalCover" , AIGetTotalCover );
	lua_register(lua2, "AICoverGetPointX" , AICoverGetPointX );
	lua_register(lua2, "AICoverGetPointY" , AICoverGetPointY );
	lua_register(lua2, "AICoverGetPointZ" , AICoverGetPointZ );
	lua_register(lua2, "AICoverGetAngle" , AICoverGetAngle );
	lua_register(lua2, "AICoverGetIfUsed" , AICoverGetIfUsed );
	lua_register(lua2, "AICouldSee", AICouldSee);

	// New Attachment Commands
	#ifdef WICKEDENGINE
	lua_register(lua2, "HideEntityAttachment", HideEntityAttachment);
	lua_register(lua2, "ShowEntityAttachment", ShowEntityAttachment);
	#endif

	// New Debugging Commands
	#ifdef WICKEDENGINE
	lua_register(lua2, "SetDebuggingData", SetDebuggingData);
	#endif
		
	// New RecastDetour (RD) AI Commands
	lua_register(lua2, "RDFindPath", RDFindPath);
	lua_register(lua2, "RDGetPathPointCount", RDGetPathPointCount);
	lua_register(lua2, "RDGetPathPointX", RDGetPathPointX);
	lua_register(lua2, "RDGetPathPointY", RDGetPathPointY);
	lua_register(lua2, "RDGetPathPointZ", RDGetPathPointZ);
	lua_register(lua2, "StartMoveAndRotateToXYZ", StartMoveAndRotateToXYZ);
	lua_register(lua2, "MoveAndRotateToXYZ", MoveAndRotateToXYZ);
	lua_register(lua2, "SetEntityPathRotationMode", SetEntityPathRotationMode);	
	lua_register(lua2, "RDIsWithinMesh", RDIsWithinMesh);
	lua_register(lua2, "RDIsWithinAndOverMesh", RDIsWithinAndOverMesh);
	lua_register(lua2, "RDGetYFromMeshPosition", RDGetYFromMeshPosition);
	lua_register(lua2, "RDBlockNavMesh", RDBlockNavMesh);
	lua_register(lua2, "RDBlockNavMeshWithShape", RDBlockNavMeshWithShape);

	lua_register(lua2, "DoTokenDrop", DoTokenDrop);
	lua_register(lua2, "GetTokenDropCount", GetTokenDropCount);
	lua_register(lua2, "GetTokenDropX", GetTokenDropZ);
	lua_register(lua2, "GetTokenDropY", GetTokenDropY);
	lua_register(lua2, "GetTokenDropZ", GetTokenDropZ);
	lua_register(lua2, "GetTokenDropType", GetTokenDropType);
	lua_register(lua2, "GetTokenDropTimeLeft", GetTokenDropTimeLeft);

	lua_register(lua2, "AdjustPositionToGetLineOfSight", AdjustPositionToGetLineOfSight);
	lua_register(lua2, "SetCharacterMode", SetCharacterMode);
	
	// Visual Attribs
	lua_register(lua2, "GetFogNearest" , GetFogNearest );
	lua_register(lua2, "GetFogDistance" , GetFogDistance );
	lua_register(lua2, "GetFogRed" , GetFogRed );
	lua_register(lua2, "GetFogGreen" , GetFogGreen );
	lua_register(lua2, "GetFogBlue" , GetFogBlue );
	lua_register(lua2, "GetFogIntensity" , GetFogIntensity );
	lua_register(lua2, "GetAmbienceIntensity" , GetAmbienceIntensity );
	lua_register(lua2, "GetAmbienceRed" , GetAmbienceRed );
	lua_register(lua2, "GetAmbienceGreen" , GetAmbienceGreen );
	lua_register(lua2, "GetAmbienceBlue" , GetAmbienceBlue );
	lua_register(lua2, "GetSurfaceRed" , GetSurfaceRed );
	lua_register(lua2, "GetSurfaceGreen" , GetSurfaceGreen );
	lua_register(lua2, "GetSurfaceBlue" , GetSurfaceBlue );
	lua_register(lua2, "GetSurfaceIntensity" , GetSurfaceIntensity );
	lua_register(lua2, "GetPostVignetteRadius" , GetPostVignetteRadius );
	lua_register(lua2, "GetPostVignetteIntensity" , GetPostVignetteIntensity );
	lua_register(lua2, "GetPostMotionDistance" , GetPostMotionDistance );
	lua_register(lua2, "GetPostMotionIntensity" , GetPostMotionIntensity );
	lua_register(lua2, "GetPostDepthOfFieldDistance" , GetPostDepthOfFieldDistance );
	lua_register(lua2, "GetPostDepthOfFieldIntensity" , GetPostDepthOfFieldIntensity );

	lua_register(lua2, "LoadImage" , LoadImage );
	lua_register(lua2, "DeleteImage" , DeleteSpriteImage );
	lua_register(lua2, "GetImageWidth" , GetImageWidth );
	lua_register(lua2, "GetImageHeight" , GetImageHeight );
	lua_register(lua2, "CreateSprite" , CreateSprite );
	lua_register(lua2, "PasteSprite" , PasteSprite );
	lua_register(lua2, "PasteSpritePosition" , PasteSpritePosition );
	lua_register(lua2, "SetSpritePosition", SetSpritePosition);
	lua_register(lua2, "SetSpritePriority" , SetSpritePriorityForLUA);
	lua_register(lua2, "SetSpriteSize" , SetSpriteSize );
	lua_register(lua2, "SetSpriteDepth" , SetSpriteDepth );
	lua_register(lua2, "SetSpriteColor" , SetSpriteColor );	
	lua_register(lua2, "SetSpriteAngle" , SetSpriteAngle );	
	lua_register(lua2, "SetSpriteOffset" , SetSpriteOffset );
	lua_register(lua2, "DeleteSprite" , DeleteSprite );
	lua_register(lua2, "SetSpriteScissor", SetSpriteScissor);
	lua_register(lua2, "SetSpriteImage", SetSpriteImage);
	lua_register(lua2, "DrawSpritesFirst" , DrawSpritesFirstForLUA );
	lua_register(lua2, "DrawSpritesLast" , DrawSpritesLastForLUA );	
	lua_register(lua2, "BackdropOff" , BackdropOffForLUA );	
	lua_register(lua2, "BackdropOn" , BackdropOnForLUA );	

	lua_register(lua2, "LoadGlobalSound" , LoadGlobalSound );	
	lua_register(lua2, "PlayGlobalSound" , PlayGlobalSound );	
	lua_register(lua2, "LoopGlobalSound" , LoopGlobalSound );	
	lua_register(lua2, "StopGlobalSound" , StopGlobalSound );	
	lua_register(lua2, "DeleteGlobalSound" , DeleteGlobalSound );	
	lua_register(lua2, "SetGlobalSoundSpeed" , SetGlobalSoundSpeed );	
	lua_register(lua2, "SetGlobalSoundVolume" , SetGlobalSoundVolume );	
	lua_register(lua2, "GetGlobalSoundExist" , GetGlobalSoundExist );
	lua_register(lua2, "GetGlobalSoundPlaying" , GetGlobalSoundPlaying );
	lua_register(lua2, "GetGlobalSoundLooping" , GetGlobalSoundLooping );
	lua_register(lua2, "GetSoundPlaying", GetSoundPlaying);

	lua_register(lua2, "PlayRawSound" , PlayRawSound );
	lua_register(lua2, "LoopRawSound" , LoopRawSound );
	lua_register(lua2, "StopRawSound" , StopRawSound );
	lua_register(lua2, "SetRawSoundVolume" , SetRawSoundVolume );
	lua_register(lua2, "SetRawSoundSpeed" , SetRawSoundSpeed );
	lua_register(lua2, "RawSoundExist" , RawSoundExist );
	lua_register(lua2, "RawSoundPlaying" , RawSoundPlaying );
	lua_register(lua2, "GetEntityRawSound" , GetEntityRawSound );
	lua_register(lua2, "StartAmbientMusicTrack", StartAmbientMusicTrack);
	lua_register(lua2, "StopAmbientMusicTrack", StopAmbientMusicTrack);
	lua_register(lua2, "SetAmbientMusicTrackVolume", SetAmbientMusicTrackVolume);
	lua_register(lua2, "StartCombatMusicTrack", StartCombatMusicTrack);
	lua_register(lua2, "StopCombatMusicTrack", StopCombatMusicTrack);
	lua_register(lua2, "SetCombatMusicTrackVolume", SetCombatMusicTrackVolume);
	lua_register(lua2, "GetCombatMusicTrackPlaying", GetCombatMusicTrackPlaying);

	lua_register(lua2, "SetSoundMusicMode", SetSoundMusicMode);
	lua_register(lua2, "GetSoundMusicMode", GetSoundMusicMode);

	#ifdef VRTECH
	lua_register(lua2, "GetSpeech" , GetSpeech );
	#endif

	lua_register(lua2, "GetTimeElapsed" , GetTimeElapsed );
	lua_register(lua2, "GetKeyState" , GetKeyState );
	lua_register(lua2, "SetGlobalTimer" , SetGlobalTimer);
	lua_register(lua2, "Timer", GetGlobalTimer);
	lua_register(lua2, "MouseMoveX" , MouseMoveX );
	lua_register(lua2, "MouseMoveY" , MouseMoveY );
	lua_register(lua2, "GetDesktopWidth" , GetDesktopWidth );
	lua_register(lua2, "GetDesktopHeight" , GetDesktopHeight );
	lua_register(lua2, "CurveValue" , CurveValue );
	lua_register(lua2, "CurveAngle" , CurveAngle );
	lua_register(lua2, "PositionMouse" , PositionMouse );
	lua_register(lua2, "GetDynamicCharacterControllerDidJump" , GetDynamicCharacterControllerDidJump );
	lua_register(lua2, "GetCharacterControllerDucking" , GetCharacterControllerDucking );
	lua_register(lua2, "WrapValue" , WrapValue );
	lua_register(lua2, "GetElapsedTime" , GetElapsedTime );
	lua_register(lua2, "GetPlrObjectPositionX" , GetPlrObjectPositionX );
	lua_register(lua2, "GetPlrObjectPositionY" , GetPlrObjectPositionY );
	lua_register(lua2, "GetPlrObjectPositionZ" , GetPlrObjectPositionZ );
	lua_register(lua2, "GetPlrObjectAngleX" , GetPlrObjectAngleX );
	lua_register(lua2, "GetPlrObjectAngleY" , GetPlrObjectAngleY );
	lua_register(lua2, "GetPlrObjectAngleZ" , GetPlrObjectAngleZ );
	lua_register(lua2, "GetGroundHeight" , GetGroundHeight );
	lua_register(lua2, "NewXValue" , NewXValue );
	lua_register(lua2, "NewZValue" , NewZValue );
	lua_register(lua2, "ControlDynamicCharacterController" , ControlDynamicCharacterController );
	lua_register(lua2, "GetCharacterHitFloor" , GetCharacterHitFloor );
	lua_register(lua2, "GetCharacterFallDistance" , GetCharacterFallDistance );
	lua_register(lua2, "RayTerrain" , RayTerrain );
	lua_register(lua2, "GetRayCollisionX" , GetRayCollisionX );
	lua_register(lua2, "GetRayCollisionY" , GetRayCollisionY );
	lua_register(lua2, "GetRayCollisionZ" , GetRayCollisionZ );
	lua_register(lua2, "IntersectAll" , IntersectAll );
	lua_register(lua2, "IntersectStatic", IntersectStatic);
	lua_register(lua2, "IntersectStaticPerformant", IntersectStaticPerformant);
	lua_register(lua2, "IntersectAllIncludeTerrain", IntersectAllIncludeTerrain);
	lua_register(lua2, "GetIntersectCollisionX" , GetIntersectCollisionX );
	lua_register(lua2, "GetIntersectCollisionY" , GetIntersectCollisionY );
	lua_register(lua2, "GetIntersectCollisionZ" , GetIntersectCollisionZ );
	lua_register(lua2, "GetIntersectCollisionNX" , GetIntersectCollisionNX );
	lua_register(lua2, "GetIntersectCollisionNY" , GetIntersectCollisionNY );
	lua_register(lua2, "GetIntersectCollisionNZ" , GetIntersectCollisionNZ );
	lua_register(lua2, "IntersectGetLastHitBone", IntersectGetLastHitBone);
	lua_register(lua2, "IntersectGetLastHitFrame", IntersectGetLastHitFrame);

	lua_register(lua2, "PositionCamera" , PositionCamera );
	lua_register(lua2, "PointCamera" , PointCamera );
	lua_register(lua2, "MoveCamera" , MoveCamera );
	lua_register(lua2, "GetObjectExist" , GetObjectExist );
	lua_register(lua2, "SetObjectFrame", SetObjectFrame);
	lua_register(lua2, "GetObjectFrame" , GetObjectFrame );
	lua_register(lua2, "SetObjectSpeed" , SetObjectSpeed );
	lua_register(lua2, "GetObjectSpeed" , GetObjectSpeed );
	lua_register(lua2, "PositionObject" , PositionObject );
	lua_register(lua2, "RotateObject" , RotateObject );
	lua_register(lua2, "GetObjectAngleX" , GetObjectAngleX );
	lua_register(lua2, "GetObjectAngleY" , GetObjectAngleY );
	lua_register(lua2, "GetObjectAngleZ" , GetObjectAngleZ );
	lua_register(lua2, "GetObjectPosAng",  GetObjectPosAng );
	lua_register(lua2, "GetObjectColBox",  GetObjectColBox );
	lua_register(lua2, "GetObjectCentre", GetObjectCentre);
	lua_register(lua2, "GetObjectColCentre", GetObjectColCentre);
	lua_register(lua2, "GetObjectScales",  GetObjectScales );
	lua_register(lua2, "ScaleObject",   ScaleObjectXYZ );

	// Physics related functions
	lua_register(lua2, "PushObject",              PushObject );
	lua_register(lua2, "ConstrainObjMotion",      ConstrainObjMotion );
	lua_register(lua2, "ConstrainObjRotation",    ConstrainObjRotation );
	lua_register(lua2, "CreateSingleHinge",       CreateSingleHinge );
	lua_register(lua2, "CreateSingleJoint",       CreateSingleJoint );
	lua_register(lua2, "CreateDoubleHinge",       CreateDoubleHinge );
	lua_register(lua2, "CreateDoubleJoint",       CreateDoubleJoint );
	lua_register(lua2, "CreateSliderDouble",      CreateSliderDouble );
	lua_register(lua2, "RemoveObjectConstraints", RemoveObjectConstraints );
	lua_register(lua2, "RemoveConstraint",        RemoveConstraint );
	lua_register(lua2, "PhysicsRayCast",          PhysicsRayCast );
	lua_register(lua2, "SetObjectDamping",        SetObjectDamping );
	lua_register(lua2, "SetHingeLimits",          SetHingeLimits );
	lua_register(lua2, "GetHingeAngle",           GetHingeAngle );
	lua_register(lua2, "SetHingeMotor",           SetHingeMotor );
	lua_register(lua2, "SetSliderMotor",          SetSliderMotor );
	lua_register(lua2, "SetBodyScaling",          SetBodyScaling );
	lua_register(lua2, "SetSliderLimits",         SetSliderLimits );
	lua_register(lua2, "GetSliderPosition",       GetSliderPosition );

	// Collision detection functions 
	lua_register(lua2, "GetObjectNumCollisions",     GetObjectNumCollisions );
	lua_register(lua2, "GetObjectCollisionDetails",  GetObjectCollisionDetails );
	lua_register(lua2, "GetTerrainNumCollisions",    GetTerrainNumCollisions );
	lua_register(lua2, "GetTerrainCollisionDetails", GetTerrainCollisionDetails );
	lua_register(lua2, "AddObjectCollisionCheck",    AddObjectCollisionCheck );
	lua_register(lua2, "RemoveObjectCollisionCheck", RemoveObjectCollisionCheck );

	// quaternion library functions
	lua_register(lua2, "QuatToEuler",  QuatToEuler );
	lua_register(lua2, "EulerToQuat",  EulerToQuat );
	lua_register(lua2, "QuatMultiply", QuatMultiply );
	lua_register(lua2, "QuatSLERP",    QuatSLERP );
	lua_register(lua2, "QuatLERP",     QuatLERP );

	lua_register(lua2, "Convert3DTo2D", LuaConvert3DTo2D); // x,y = Convert3DTo2D(x,y,z)
	lua_register(lua2, "Convert2DTo3D", LuaConvert2DTo3D); // px,py,pz,dx,dy,dz = Convert2DTo3D(x percent,y percent) -- percent
	lua_register(lua2, "ScreenCoordsToPercent", ScreenCoordsToPercent); // percentx,percentx = ScreenCordsToPercent(x,y) -- return percent positions.

	
	// Lua control of dynamic light
	lua_register(lua2, "GetEntityLightNumber", GetEntityLightNumber );
	lua_register(lua2, "GetLightPosition",     GetLightPosition );
	lua_register(lua2, "GetLightAngle",		  GetLightAngle);
	lua_register(lua2, "GetLightEuler",		  GetLightEuler);
	lua_register(lua2, "GetLightRGB",          GetLightRGB );
	lua_register(lua2, "GetLightRange",        GetLightRange );
	lua_register(lua2, "SetLightPosition",     SetLightPosition );
	lua_register(lua2, "SetLightAngle",		  SetLightAngle);
	lua_register(lua2, "SetLightEuler",		  SetLightEuler);
	lua_register(lua2, "SetLightRGB",          SetLightRGB );
	lua_register(lua2, "SetLightRange",        SetLightRange );
	
	lua_register(lua2, "RunCharLoop" , RunCharLoop );
	lua_register(lua2, "TriggerWaterRipple" , TriggerWaterRipple );
	lua_register(lua2, "TriggerWaterRippleSize", TriggerWaterRippleSize);
	lua_register(lua2, "TriggerWaterSplash", TriggerWaterSplash);
	lua_register(lua2, "PlayFootfallSound" , PlayFootfallSound );
	lua_register(lua2, "ResetUnderwaterState" , ResetUnderwaterState );
	lua_register(lua2, "SetUnderwaterOn" , SetUnderwaterOn );
	lua_register(lua2, "SetUnderwaterOff" , SetUnderwaterOff );

	lua_register(lua2, "SetWorldGravity", SetWorldGravity);

	lua_register(lua2, "SetShaderVariable" , SetShaderVariable );
	
	lua_register(lua2, "GetGamePlayerControlJetpackMode" , GetGamePlayerControlJetpackMode );
	lua_register(lua2, "GetGamePlayerControlJetpackFuel" , GetGamePlayerControlJetpackFuel );
	lua_register(lua2, "GetGamePlayerControlJetpackHidden" , GetGamePlayerControlJetpackHidden );
	lua_register(lua2, "GetGamePlayerControlJetpackCollected" , GetGamePlayerControlJetpackCollected );
	lua_register(lua2, "GetGamePlayerControlSoundStartIndex" , GetGamePlayerControlSoundStartIndex );
	lua_register(lua2, "GetGamePlayerControlJetpackParticleEmitterIndex" , GetGamePlayerControlJetpackParticleEmitterIndex );
	lua_register(lua2, "GetGamePlayerControlJetpackThrust" , GetGamePlayerControlJetpackThrust );
	lua_register(lua2, "GetGamePlayerControlStartStrength" , GetGamePlayerControlStartStrength );
	lua_register(lua2, "GetGamePlayerControlIsRunning" , GetGamePlayerControlIsRunning );
	lua_register(lua2, "GetGamePlayerControlFinalCameraAngley" , GetGamePlayerControlFinalCameraAngley );
	lua_register(lua2, "GetGamePlayerControlCx" , GetGamePlayerControlCx );
	lua_register(lua2, "GetGamePlayerControlCy" , GetGamePlayerControlCy );
	lua_register(lua2, "GetGamePlayerControlCz" , GetGamePlayerControlCz );
	lua_register(lua2, "GetGamePlayerControlBasespeed" , GetGamePlayerControlBasespeed );
	lua_register(lua2, "GetGamePlayerControlCanRun" , GetGamePlayerControlCanRun );
	lua_register(lua2, "GetGamePlayerControlMaxspeed" , GetGamePlayerControlMaxspeed );
	lua_register(lua2, "GetGamePlayerControlTopspeed" , GetGamePlayerControlTopspeed );
	lua_register(lua2, "GetGamePlayerControlMovement" , GetGamePlayerControlMovement );
	lua_register(lua2, "GetGamePlayerControlMovey" , GetGamePlayerControlMovey );
	lua_register(lua2, "GetGamePlayerControlLastMovement" , GetGamePlayerControlLastMovement );
	lua_register(lua2, "GetGamePlayerControlFootfallCount" , GetGamePlayerControlFootfallCount );
	lua_register(lua2, "GetGamePlayerControlLastMovement" , GetGamePlayerControlLastMovement );
	lua_register(lua2, "GetGamePlayerControlGravityActive" , GetGamePlayerControlGravityActive );
	lua_register(lua2, "GetGamePlayerControlPlrHitFloorMaterial" , GetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua2, "GetGamePlayerControlUnderwater" , GetGamePlayerControlUnderwater );
	lua_register(lua2, "GetGamePlayerControlJumpMode" , GetGamePlayerControlJumpMode );
	lua_register(lua2, "GetGamePlayerControlJumpModeCanAffectVelocityCountdown" , GetGamePlayerControlJumpModeCanAffectVelocityCountdown );
	lua_register(lua2, "GetGamePlayerControlSpeed" , GetGamePlayerControlSpeed );
	lua_register(lua2, "GetGamePlayerControlAccel" , GetGamePlayerControlAccel );
	lua_register(lua2, "GetGamePlayerControlSpeedRatio" , GetGamePlayerControlSpeedRatio );
	lua_register(lua2, "GetGamePlayerControlWobble" , GetGamePlayerControlWobble );
	lua_register(lua2, "GetGamePlayerControlWobbleSpeed" , GetGamePlayerControlWobbleSpeed );
	lua_register(lua2, "GetGamePlayerControlWobbleHeight" , GetGamePlayerControlWobbleHeight );
	lua_register(lua2, "GetGamePlayerControlJumpmax" , GetGamePlayerControlJumpmax );
	lua_register(lua2, "GetGamePlayerControlPushangle" , GetGamePlayerControlPushangle );
	lua_register(lua2, "GetGamePlayerControlPushforce" , GetGamePlayerControlPushforce );
	lua_register(lua2, "GetGamePlayerControlFootfallPace" , GetGamePlayerControlFootfallPace );
	lua_register(lua2, "GetGamePlayerControlFinalCameraAngley" , GetGamePlayerControlFinalCameraAngley );
	lua_register(lua2, "GetGamePlayerControlLockAtHeight" , GetGamePlayerControlLockAtHeight );
	lua_register(lua2, "GetGamePlayerControlControlHeight" , GetGamePlayerControlControlHeight );
	lua_register(lua2, "GetGamePlayerControlControlHeightCooldown" , GetGamePlayerControlControlHeightCooldown );
	lua_register(lua2, "GetGamePlayerControlStoreMovey" , GetGamePlayerControlStoreMovey );
	lua_register(lua2, "GetGamePlayerControlPlrHitFloorMaterial" , GetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua2, "GetGamePlayerControlHurtFall" , GetGamePlayerControlHurtFall );
	lua_register(lua2, "GetGamePlayerControlLeanoverAngle" , GetGamePlayerControlLeanoverAngle );
	lua_register(lua2, "GetGamePlayerControlLeanover" , GetGamePlayerControlLeanover );
	lua_register(lua2, "GetGamePlayerControlCameraShake" , GetGamePlayerControlCameraShake );
	lua_register(lua2, "GetGamePlayerControlFinalCameraAnglex" , GetGamePlayerControlFinalCameraAnglex );
	lua_register(lua2, "GetGamePlayerControlFinalCameraAngley" , GetGamePlayerControlFinalCameraAngley );
	lua_register(lua2, "GetGamePlayerControlFinalCameraAnglez" , GetGamePlayerControlFinalCameraAnglez );
	lua_register(lua2, "GetGamePlayerControlCamRightMouseMode" , GetGamePlayerControlCamRightMouseMode );
	lua_register(lua2, "GetGamePlayerControlCamCollisionSmooth" , GetGamePlayerControlCamCollisionSmooth );
	lua_register(lua2, "GetGamePlayerControlCamCurrentDistance" , GetGamePlayerControlCamCurrentDistance );
	lua_register(lua2, "GetGamePlayerControlCamDoFullRayCheck" , GetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua2, "GetGamePlayerControlLastGoodcx" , GetGamePlayerControlLastGoodcx );
	lua_register(lua2, "GetGamePlayerControlLastGoodcy" , GetGamePlayerControlLastGoodcy );
	lua_register(lua2, "GetGamePlayerControlLastGoodcz" , GetGamePlayerControlLastGoodcz );
	lua_register(lua2, "GetGamePlayerControlCamDoFullRayCheck" , GetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua2, "GetGamePlayerControlFlinchx" , GetGamePlayerControlFlinchx );
	lua_register(lua2, "GetGamePlayerControlFlinchy" , GetGamePlayerControlFlinchy );
	lua_register(lua2, "GetGamePlayerControlFlinchz" , GetGamePlayerControlFlinchz );
	lua_register(lua2, "GetGamePlayerControlFlinchCurrentx" , GetGamePlayerControlFlinchCurrentx );
	lua_register(lua2, "GetGamePlayerControlFlinchCurrenty" , GetGamePlayerControlFlinchCurrenty );
	lua_register(lua2, "GetGamePlayerControlFlinchCurrentz" , GetGamePlayerControlFlinchCurrentz );
	lua_register(lua2, "GetGamePlayerControlFootfallType" , GetGamePlayerControlFootfallType );
	lua_register(lua2, "GetGamePlayerControlRippleCount" , GetGamePlayerControlRippleCount );
	lua_register(lua2, "GetGamePlayerControlLastFootfallSound" , GetGamePlayerControlLastFootfallSound );
	lua_register(lua2, "GetGamePlayerControlInWaterState" , GetGamePlayerControlInWaterState );
	lua_register(lua2, "GetGamePlayerControlDrownTimestamp" , GetGamePlayerControlDrownTimestamp );
	lua_register(lua2, "GetGamePlayerControlDeadTime" , GetGamePlayerControlDeadTime );
	lua_register(lua2, "GetGamePlayerControlSwimTimestamp" , GetGamePlayerControlSwimTimestamp );
	lua_register(lua2, "GetGamePlayerControlRedDeathFog" , GetGamePlayerControlRedDeathFog );
	lua_register(lua2, "GetGamePlayerControlHeartbeatTimeStamp" , GetGamePlayerControlHeartbeatTimeStamp );
	lua_register(lua2, "GetGamePlayerControlThirdpersonEnabled" , GetGamePlayerControlThirdpersonEnabled );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCharacterIndex" , GetGamePlayerControlThirdpersonCharacterIndex );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraFollow" , GetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraFocus" , GetGamePlayerControlThirdpersonCameraFocus );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCharactere" , GetGamePlayerControlThirdpersonCharactere );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraFollow" , GetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua2, "GetGamePlayerControlThirdpersonShotFired" , GetGamePlayerControlThirdpersonShotFired );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraDistance" , GetGamePlayerControlThirdpersonCameraDistance );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraSpeed" , GetGamePlayerControlThirdpersonCameraSpeed );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraLocked" , GetGamePlayerControlThirdpersonCameraLocked );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraHeight" , GetGamePlayerControlThirdpersonCameraHeight );
	lua_register(lua2, "GetGamePlayerControlThirdpersonCameraShoulder" , GetGamePlayerControlThirdpersonCameraShoulder );
	lua_register(lua2, "GetGamePlayerControlFallDamageModifier", GetGamePlayerControlFallDamageModifier);
	lua_register(lua2, "GetGamePlayerControlSwimSpeed", GetGamePlayerControlSwimSpeed);
	lua_register(lua2, "SetGamePlayerControlJetpackMode" , SetGamePlayerControlJetpackMode );
	lua_register(lua2, "SetGamePlayerControlJetpackFuel" , SetGamePlayerControlJetpackFuel );
	lua_register(lua2, "SetGamePlayerControlJetpackHidden" , SetGamePlayerControlJetpackHidden );
	lua_register(lua2, "SetGamePlayerControlJetpackCollected" , SetGamePlayerControlJetpackCollected );
	lua_register(lua2, "SetGamePlayerControlSoundStartIndex" , SetGamePlayerControlSoundStartIndex );
	lua_register(lua2, "SetGamePlayerControlJetpackParticleEmitterIndex" , SetGamePlayerControlJetpackParticleEmitterIndex );
	lua_register(lua2, "SetGamePlayerControlJetpackThrust" , SetGamePlayerControlJetpackThrust );
	lua_register(lua2, "SetGamePlayerControlStartStrength" , SetGamePlayerControlStartStrength );
	lua_register(lua2, "SetGamePlayerControlIsRunning" , SetGamePlayerControlIsRunning );
	lua_register(lua2, "SetGamePlayerControlFinalCameraAngley" , SetGamePlayerControlFinalCameraAngley );
	lua_register(lua2, "SetGamePlayerControlCx" , SetGamePlayerControlCx );
	lua_register(lua2, "SetGamePlayerControlCy" , SetGamePlayerControlCy );
	lua_register(lua2, "SetGamePlayerControlCz" , SetGamePlayerControlCz );
	lua_register(lua2, "SetGamePlayerControlBasespeed" , SetGamePlayerControlBasespeed );
	lua_register(lua2, "SetGamePlayerControlCanRun" , SetGamePlayerControlCanRun );
	lua_register(lua2, "SetGamePlayerControlMaxspeed" , SetGamePlayerControlMaxspeed );
	lua_register(lua2, "SetGamePlayerControlTopspeed" , SetGamePlayerControlTopspeed );
	lua_register(lua2, "SetGamePlayerControlMovement" , SetGamePlayerControlMovement );
	lua_register(lua2, "SetGamePlayerControlMovey" , SetGamePlayerControlMovey );
	lua_register(lua2, "SetGamePlayerControlLastMovement" , SetGamePlayerControlLastMovement );
	lua_register(lua2, "SetGamePlayerControlFootfallCount" , SetGamePlayerControlFootfallCount );
	lua_register(lua2, "SetGamePlayerControlLastMovement" , SetGamePlayerControlLastMovement );
	lua_register(lua2, "SetGamePlayerControlGravityActive" , SetGamePlayerControlGravityActive );
	lua_register(lua2, "SetGamePlayerControlPlrHitFloorMaterial" , SetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua2, "SetGamePlayerControlUnderwater" , SetGamePlayerControlUnderwater );
	lua_register(lua2, "SetGamePlayerControlJumpMode" , SetGamePlayerControlJumpMode );
	lua_register(lua2, "SetGamePlayerControlJumpModeCanAffectVelocityCountdown" , SetGamePlayerControlJumpModeCanAffectVelocityCountdown );
	lua_register(lua2, "SetGamePlayerControlSpeed" , SetGamePlayerControlSpeed );
	lua_register(lua2, "SetGamePlayerControlAccel" , SetGamePlayerControlAccel );
	lua_register(lua2, "SetGamePlayerControlSpeedRatio" , SetGamePlayerControlSpeedRatio );
	lua_register(lua2, "SetGamePlayerControlWobble" , SetGamePlayerControlWobble );
	lua_register(lua2, "SetGamePlayerControlWobbleSpeed" , SetGamePlayerControlWobbleSpeed );
	lua_register(lua2, "SetGamePlayerControlWobbleHeight" , SetGamePlayerControlWobbleHeight );
	lua_register(lua2, "SetGamePlayerControlJumpmax" , SetGamePlayerControlJumpmax );
	lua_register(lua2, "SetGamePlayerControlPushangle" , SetGamePlayerControlPushangle );
	lua_register(lua2, "SetGamePlayerControlPushforce" , SetGamePlayerControlPushforce );
	lua_register(lua2, "SetGamePlayerControlFootfallPace" , SetGamePlayerControlFootfallPace );
	lua_register(lua2, "SetGamePlayerControlFinalCameraAngley" , SetGamePlayerControlFinalCameraAngley );
	lua_register(lua2, "SetGamePlayerControlLockAtHeight" , SetGamePlayerControlLockAtHeight );
	lua_register(lua2, "SetGamePlayerControlControlHeight" , SetGamePlayerControlControlHeight );
	lua_register(lua2, "SetGamePlayerControlControlHeightCooldown" , SetGamePlayerControlControlHeightCooldown );
	lua_register(lua2, "SetGamePlayerControlStoreMovey" , SetGamePlayerControlStoreMovey );
	lua_register(lua2, "SetGamePlayerControlPlrHitFloorMaterial" , SetGamePlayerControlPlrHitFloorMaterial );
	lua_register(lua2, "SetGamePlayerControlHurtFall" , SetGamePlayerControlHurtFall );
	lua_register(lua2, "SetGamePlayerControlLeanoverAngle" , SetGamePlayerControlLeanoverAngle );
	lua_register(lua2, "SetGamePlayerControlLeanover" , SetGamePlayerControlLeanover );
	lua_register(lua2, "SetGamePlayerControlCameraShake" , SetGamePlayerControlCameraShake );
	lua_register(lua2, "SetGamePlayerControlFinalCameraAnglex" , SetGamePlayerControlFinalCameraAnglex );
	lua_register(lua2, "SetGamePlayerControlFinalCameraAngley" , SetGamePlayerControlFinalCameraAngley );
	lua_register(lua2, "SetGamePlayerControlFinalCameraAnglez" , SetGamePlayerControlFinalCameraAnglez );
	lua_register(lua2, "SetGamePlayerControlCamRightMouseMode" , SetGamePlayerControlCamRightMouseMode );
	lua_register(lua2, "SetGamePlayerControlCamCollisionSmooth" , SetGamePlayerControlCamCollisionSmooth );
	lua_register(lua2, "SetGamePlayerControlCamCurrentDistance" , SetGamePlayerControlCamCurrentDistance );
	lua_register(lua2, "SetGamePlayerControlCamDoFullRayCheck" , SetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua2, "SetGamePlayerControlLastGoodcx" , SetGamePlayerControlLastGoodcx );
	lua_register(lua2, "SetGamePlayerControlLastGoodcy" , SetGamePlayerControlLastGoodcy );
	lua_register(lua2, "SetGamePlayerControlLastGoodcz" , SetGamePlayerControlLastGoodcz );
	lua_register(lua2, "SetGamePlayerControlCamDoFullRayCheck" , SetGamePlayerControlCamDoFullRayCheck );
	lua_register(lua2, "SetGamePlayerControlFlinchx" , SetGamePlayerControlFlinchx );
	lua_register(lua2, "SetGamePlayerControlFlinchy" , SetGamePlayerControlFlinchy );
	lua_register(lua2, "SetGamePlayerControlFlinchz" , SetGamePlayerControlFlinchz );
	lua_register(lua2, "SetGamePlayerControlFlinchCurrentx" , SetGamePlayerControlFlinchCurrentx );
	lua_register(lua2, "SetGamePlayerControlFlinchCurrenty" , SetGamePlayerControlFlinchCurrenty );
	lua_register(lua2, "SetGamePlayerControlFlinchCurrentz" , SetGamePlayerControlFlinchCurrentz );
	lua_register(lua2, "SetGamePlayerControlFootfallType" , SetGamePlayerControlFootfallType );
	lua_register(lua2, "SetGamePlayerControlRippleCount" , SetGamePlayerControlRippleCount );
	lua_register(lua2, "SetGamePlayerControlLastFootfallSound" , SetGamePlayerControlLastFootfallSound );
	lua_register(lua2, "SetGamePlayerControlInWaterState" , SetGamePlayerControlInWaterState );
	lua_register(lua2, "SetGamePlayerControlDrownTimestamp" , SetGamePlayerControlDrownTimestamp );
	lua_register(lua2, "SetGamePlayerControlDeadTime" , SetGamePlayerControlDeadTime );
	lua_register(lua2, "SetGamePlayerControlSwimTimestamp" , SetGamePlayerControlSwimTimestamp );
	lua_register(lua2, "SetGamePlayerControlRedDeathFog" , SetGamePlayerControlRedDeathFog );
	lua_register(lua2, "SetGamePlayerControlHeartbeatTimeStamp" , SetGamePlayerControlHeartbeatTimeStamp );
	lua_register(lua2, "SetGamePlayerControlThirdpersonEnabled" , SetGamePlayerControlThirdpersonEnabled );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCharacterIndex" , SetGamePlayerControlThirdpersonCharacterIndex );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraFollow" , SetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraFocus" , SetGamePlayerControlThirdpersonCameraFocus );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCharactere" , SetGamePlayerControlThirdpersonCharactere );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraFollow" , SetGamePlayerControlThirdpersonCameraFollow );
	lua_register(lua2, "SetGamePlayerControlThirdpersonShotFired" , SetGamePlayerControlThirdpersonShotFired );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraDistance" , SetGamePlayerControlThirdpersonCameraDistance );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraSpeed" , SetGamePlayerControlThirdpersonCameraSpeed );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraLocked" , SetGamePlayerControlThirdpersonCameraLocked );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraHeight" , SetGamePlayerControlThirdpersonCameraHeight );
	lua_register(lua2, "SetGamePlayerControlThirdpersonCameraShoulder" , SetGamePlayerControlThirdpersonCameraShoulder );

	lua_register(lua2, "SetGamePlayerStateGunMode" , SetGamePlayerStateGunMode );
	lua_register(lua2, "GetGamePlayerStateGunMode" , GetGamePlayerStateGunMode );
	lua_register(lua2, "SetGamePlayerStateFiringMode" , SetGamePlayerStateFiringMode );
	lua_register(lua2, "GetGamePlayerStateFiringMode" , GetGamePlayerStateFiringMode );
	lua_register(lua2, "GetGamePlayerStateWeaponAmmoIndex" , GetGamePlayerStateWeaponAmmoIndex );
	lua_register(lua2, "GetGamePlayerStateAmmoOffset" , GetGamePlayerStateAmmoOffset );
	lua_register(lua2, "GetGamePlayerStateGunMeleeKey" , GetGamePlayerStateGunMeleeKey );
	lua_register(lua2, "SetGamePlayerStateBlockingAction", SetGamePlayerStateBlockingAction);
	lua_register(lua2, "GetGamePlayerStateBlockingAction", GetGamePlayerStateBlockingAction);
	lua_register(lua2, "SetGamePlayerStateGunShootNoAmmo" , SetGamePlayerStateGunShootNoAmmo );
	lua_register(lua2, "GetGamePlayerStateGunShootNoAmmo" , GetGamePlayerStateGunShootNoAmmo );
	lua_register(lua2, "SetGamePlayerStateUnderwater" , SetGamePlayerStateUnderwater );
	lua_register(lua2, "GetGamePlayerStateUnderwater" , GetGamePlayerStateUnderwater );
	lua_register(lua2, "SetGamePlayerStateRightMouseHold" , SetGamePlayerStateRightMouseHold );
	lua_register(lua2, "GetGamePlayerStateRightMouseHold" , GetGamePlayerStateRightMouseHold );
	lua_register(lua2, "SetGamePlayerStateXBOX" , SetGamePlayerStateXBOX );
	lua_register(lua2, "GetGamePlayerStateXBOX" , GetGamePlayerStateXBOX );
	lua_register(lua2, "SetGamePlayerStateXBOXControllerType" , SetGamePlayerStateXBOXControllerType );
	lua_register(lua2, "GetGamePlayerStateXBOXControllerType" , GetGamePlayerStateXBOXControllerType );
	lua_register(lua2, "JoystickX" , JoystickY );
	lua_register(lua2, "JoystickY" , JoystickX );
	lua_register(lua2, "JoystickZ" , JoystickZ );
	lua_register(lua2, "SetGamePlayerStateGunZoomMode" , SetGamePlayerStateGunZoomMode );
	lua_register(lua2, "GetGamePlayerStateGunZoomMode" , GetGamePlayerStateGunZoomMode );
	lua_register(lua2, "SetGamePlayerStateGunZoomMag" , SetGamePlayerStateGunZoomMag );
	lua_register(lua2, "GetGamePlayerStateGunZoomMag" , GetGamePlayerStateGunZoomMag );
	lua_register(lua2, "SetGamePlayerStateGunReloadNoAmmo" , SetGamePlayerStateGunReloadNoAmmo );
	lua_register(lua2, "GetGamePlayerStateGunReloadNoAmmo" , GetGamePlayerStateGunReloadNoAmmo );
	lua_register(lua2, "SetGamePlayerStatePlrReloading" , SetGamePlayerStatePlrReloading );
	lua_register(lua2, "GetGamePlayerStatePlrReloading" , GetGamePlayerStatePlrReloading );
	lua_register(lua2, "SetGamePlayerStateGunAltSwapKey1" , SetGamePlayerStateGunAltSwapKey1 );
	lua_register(lua2, "GetGamePlayerStateGunAltSwapKey1" , GetGamePlayerStateGunAltSwapKey1 );
	lua_register(lua2, "SetGamePlayerStateGunAltSwapKey2" , SetGamePlayerStateGunAltSwapKey2 );
	lua_register(lua2, "GetGamePlayerStateGunAltSwapKey2" , GetGamePlayerStateGunAltSwapKey2 );
	lua_register(lua2, "SetGamePlayerStateWeaponKeySelection" , SetGamePlayerStateWeaponKeySelection );
	lua_register(lua2, "GetGamePlayerStateWeaponKeySelection" , GetGamePlayerStateWeaponKeySelection );
	lua_register(lua2, "SetGamePlayerStateWeaponIndex" , SetGamePlayerStateWeaponIndex );
	lua_register(lua2, "GetGamePlayerStateWeaponIndex" , GetGamePlayerStateWeaponIndex );
	lua_register(lua2, "SetGamePlayerStateCommandNewWeapon" , SetGamePlayerStateCommandNewWeapon );
	lua_register(lua2, "GetGamePlayerStateCommandNewWeapon" , GetGamePlayerStateCommandNewWeapon );
	lua_register(lua2, "SetGamePlayerStateGunID" , SetGamePlayerStateGunID );
	lua_register(lua2, "GetGamePlayerStateGunID" , GetGamePlayerStateGunID );
	lua_register(lua2, "SetGamePlayerStateGunSelectionAfterHide" , SetGamePlayerStateGunSelectionAfterHide );
	lua_register(lua2, "GetGamePlayerStateGunSelectionAfterHide" , GetGamePlayerStateGunSelectionAfterHide );
	lua_register(lua2, "SetGamePlayerStateGunBurst" , SetGamePlayerStateGunBurst );
	lua_register(lua2, "GetGamePlayerStateGunBurst", GetGamePlayerStateGunBurst);
	lua_register(lua2, "SetGamePlayerStatePlrKeyForceKeystate", SetGamePlayerStatePlrKeyForceKeystate);
	lua_register(lua2, "JoystickHatAngle" , JoystickHatAngle );
	lua_register(lua2, "JoystickFireXL" , JoystickFireXL );
	lua_register(lua2, "JoystickTwistX" , JoystickTwistX );
	lua_register(lua2, "JoystickTwistY" , JoystickTwistY );
	lua_register(lua2, "JoystickTwistZ" , JoystickTwistZ );
	lua_register(lua2, "SetGamePlayerStatePlrZoomInChange" , SetGamePlayerStatePlrZoomInChange );
	lua_register(lua2, "GetGamePlayerStatePlrZoomInChange" , GetGamePlayerStatePlrZoomInChange );
	lua_register(lua2, "SetGamePlayerStatePlrZoomIn" , SetGamePlayerStatePlrZoomIn );
	lua_register(lua2, "GetGamePlayerStatePlrZoomIn" , GetGamePlayerStatePlrZoomIn );
	lua_register(lua2, "SetGamePlayerStateLuaActiveMouse" , SetGamePlayerStateLuaActiveMouse );
	lua_register(lua2, "GetGamePlayerStateLuaActiveMouse" , GetGamePlayerStateLuaActiveMouse );
	lua_register(lua2, "SetGamePlayerStateRealFov" , SetGamePlayerStateRealFov );
	lua_register(lua2, "GetGamePlayerStateRealFov" , GetGamePlayerStateRealFov );
	lua_register(lua2, "SetGamePlayerStateDisablePeeking" , SetGamePlayerStateDisablePeeking );
	lua_register(lua2, "GetGamePlayerStateDisablePeeking" , GetGamePlayerStateDisablePeeking );
	lua_register(lua2, "SetGamePlayerStatePlrHasFocus" , SetGamePlayerStatePlrHasFocus );
	lua_register(lua2, "GetGamePlayerStatePlrHasFocus" , GetGamePlayerStatePlrHasFocus );
	lua_register(lua2, "SetGamePlayerStateGameRunAsMultiplayer" , SetGamePlayerStateGameRunAsMultiplayer );
	lua_register(lua2, "GetGamePlayerStateGameRunAsMultiplayer" , GetGamePlayerStateGameRunAsMultiplayer );
	lua_register(lua2, "SetGamePlayerStateSteamWorksRespawnLeft" , SetGamePlayerStateSteamWorksRespawnLeft );
	lua_register(lua2, "GetGamePlayerStateSteamWorksRespawnLeft" , GetGamePlayerStateSteamWorksRespawnLeft );
	lua_register(lua2, "SetGamePlayerStateMPRespawnLeft" , SetGamePlayerStateMPRespawnLeft );
	lua_register(lua2, "GetGamePlayerStateMPRespawnLeft" , GetGamePlayerStateMPRespawnLeft );
	lua_register(lua2, "SetGamePlayerStateTabMode" , SetGamePlayerStateTabMode );
	lua_register(lua2, "GetGamePlayerStateTabMode" , GetGamePlayerStateTabMode );
	lua_register(lua2, "SetGamePlayerStateLowFpsWarning" , SetGamePlayerStateLowFpsWarning );
	lua_register(lua2, "GetGamePlayerStateLowFpsWarning" , GetGamePlayerStateLowFpsWarning );
	lua_register(lua2, "SetGamePlayerStateCameraFov" , SetGamePlayerStateCameraFov );
	lua_register(lua2, "GetGamePlayerStateCameraFov" , GetGamePlayerStateCameraFov );

	lua_register(lua2, "GetPlayerFov", GetPlayerFov);
	lua_register(lua2, "GetPlayerAttacking", GetPlayerAttacking);
	lua_register(lua2, "PushPlayer", PushPlayer);
	
	lua_register(lua2, "SetGamePlayerStateCameraFovZoomed" , SetGamePlayerStateCameraFovZoomed );
	lua_register(lua2, "GetGamePlayerStateCameraFovZoomed" , GetGamePlayerStateCameraFovZoomed );
	lua_register(lua2, "SetGamePlayerStateMouseInvert" , SetGamePlayerStateMouseInvert );
	lua_register(lua2, "GetGamePlayerStateMouseInvert" , GetGamePlayerStateMouseInvert );

	lua_register(lua2, "SetGamePlayerStateSlowMotion" , SetGamePlayerStateSlowMotion );
	lua_register(lua2, "GetGamePlayerStateSlowMotion" , GetGamePlayerStateSlowMotion );
	lua_register(lua2, "SetGamePlayerStateSmoothCameraKeys" , SetGamePlayerStateSmoothCameraKeys );
	lua_register(lua2, "GetGamePlayerStateSmoothCameraKeys" , GetGamePlayerStateSmoothCameraKeys );
	lua_register(lua2, "SetGamePlayerStateCamMouseMoveX" , SetGamePlayerStateCamMouseMoveX );
	lua_register(lua2, "GetGamePlayerStateCamMouseMoveX" , GetGamePlayerStateCamMouseMoveX );
	lua_register(lua2, "SetGamePlayerStateCamMouseMoveY" , SetGamePlayerStateCamMouseMoveY );
	lua_register(lua2, "GetGamePlayerStateCamMouseMoveY" , GetGamePlayerStateCamMouseMoveY );
	lua_register(lua2, "SetGamePlayerStateGunRecoilX" , SetGamePlayerStateGunRecoilX );
	lua_register(lua2, "GetGamePlayerStateGunRecoilX" , GetGamePlayerStateGunRecoilX );
	lua_register(lua2, "SetGamePlayerStateGunRecoilY" , SetGamePlayerStateGunRecoilY );
	lua_register(lua2, "GetGamePlayerStateGunRecoilY" , GetGamePlayerStateGunRecoilY );
	lua_register(lua2, "SetGamePlayerStateGunRecoilAngleX" , SetGamePlayerStateGunRecoilAngleX );
	lua_register(lua2, "GetGamePlayerStateGunRecoilAngleX" , GetGamePlayerStateGunRecoilAngleX );
	lua_register(lua2, "SetGamePlayerStateGunRecoilAngleY" , SetGamePlayerStateGunRecoilAngleY );
	lua_register(lua2, "GetGamePlayerStateGunRecoilAngleY" , GetGamePlayerStateGunRecoilAngleY );
	lua_register(lua2, "SetGamePlayerStateGunRecoilCorrectY" , SetGamePlayerStateGunRecoilCorrectY );
	lua_register(lua2, "GetGamePlayerStateGunRecoilCorrectY" , GetGamePlayerStateGunRecoilCorrectY );
	lua_register(lua2, "SetGamePlayerStateGunRecoilCorrectX" , SetGamePlayerStateGunRecoilCorrectX );
	lua_register(lua2, "GetGamePlayerStateGunRecoilCorrectX" , GetGamePlayerStateGunRecoilCorrectX );
	lua_register(lua2, "SetGamePlayerStateGunRecoilCorrectAngleY" , SetGamePlayerStateGunRecoilCorrectAngleY );
	lua_register(lua2, "GetGamePlayerStateGunRecoilCorrectAngleY" , GetGamePlayerStateGunRecoilCorrectAngleY );
	lua_register(lua2, "SetGamePlayerStateGunRecoilCorrectAngleX" , SetGamePlayerStateGunRecoilCorrectAngleX );
	lua_register(lua2, "GetGamePlayerStateGunRecoilCorrectAngleX" , GetGamePlayerStateGunRecoilCorrectAngleX );
	lua_register(lua2, "SetGamePlayerStateCamAngleX" , SetGamePlayerStateCamAngleX );
	lua_register(lua2, "GetGamePlayerStateCamAngleX" , GetGamePlayerStateCamAngleX );
	lua_register(lua2, "SetGamePlayerStateCamAngleY" , SetGamePlayerStateCamAngleY );
	lua_register(lua2, "GetGamePlayerStateCamAngleY" , GetGamePlayerStateCamAngleY );
	lua_register(lua2, "SetGamePlayerStatePlayerDucking" , SetGamePlayerStatePlayerDucking );
	lua_register(lua2, "GetGamePlayerStatePlayerDucking" , GetGamePlayerStatePlayerDucking );
	lua_register(lua2, "SetGamePlayerStateEditModeActive" , SetGamePlayerStateEditModeActive );
	lua_register(lua2, "GetGamePlayerStateEditModeActive" , GetGamePlayerStateEditModeActive );
	lua_register(lua2, "SetGamePlayerStatePlrKeyShift" , SetGamePlayerStatePlrKeyShift );
	lua_register(lua2, "GetGamePlayerStatePlrKeyShift" , GetGamePlayerStatePlrKeyShift );
	lua_register(lua2, "SetGamePlayerStatePlrKeyShift2" , SetGamePlayerStatePlrKeyShift2 );
	lua_register(lua2, "GetGamePlayerStatePlrKeyShift2" , GetGamePlayerStatePlrKeyShift2 );
	lua_register(lua2, "SetGamePlayerStatePlrKeyControl" , SetGamePlayerStatePlrKeyControl );
	lua_register(lua2, "GetGamePlayerStatePlrKeyControl" , GetGamePlayerStatePlrKeyControl );
	lua_register(lua2, "SetGamePlayerStateNoWater" , SetGamePlayerStateNoWater );
	lua_register(lua2, "GetGamePlayerStateNoWater" , GetGamePlayerStateNoWater );
	lua_register(lua2, "SetGamePlayerStateWaterlineY" , SetGamePlayerStateWaterlineY );
	lua_register(lua2, "GetGamePlayerStateWaterlineY" , GetGamePlayerStateWaterlineY );

	lua_register(lua2, "SetGamePlayerStateFlashlightKeyEnabled" , SetGamePlayerStateFlashlightKeyEnabled );
	lua_register(lua2, "GetGamePlayerStateFlashlightKeyEnabled" , GetGamePlayerStateFlashlightKeyEnabled );
	lua_register(lua2, "SetGamePlayerStateFlashlightControl", SetGamePlayerStateFlashlightControl);
	lua_register(lua2, "GetGamePlayerStateFlashlightControl", GetGamePlayerStateFlashlightControl);
	lua_register(lua2, "SetGamePlayerStateFlashlightRange", SetGamePlayerStateFlashlightRange);
	lua_register(lua2, "GetGamePlayerStateFlashlightRange", GetGamePlayerStateFlashlightRange);
	lua_register(lua2, "SetGamePlayerStateFlashlightRadius", SetGamePlayerStateFlashlightRadius);
	lua_register(lua2, "GetGamePlayerStateFlashlightRadius", GetGamePlayerStateFlashlightRadius);
	lua_register(lua2, "SetGamePlayerStateFlashlightColorR", SetGamePlayerStateFlashlightColorR);
	lua_register(lua2, "GetGamePlayerStateFlashlightColorR", GetGamePlayerStateFlashlightColorR);
	lua_register(lua2, "SetGamePlayerStateFlashlightColorG", SetGamePlayerStateFlashlightColorG);
	lua_register(lua2, "GetGamePlayerStateFlashlightColorG", GetGamePlayerStateFlashlightColorG);
	lua_register(lua2, "SetGamePlayerStateFlashlightColorB", SetGamePlayerStateFlashlightColorB);
	lua_register(lua2, "GetGamePlayerStateFlashlightColorB", GetGamePlayerStateFlashlightColorB);
	lua_register(lua2, "SetGamePlayerStateFlashlightCastShadow", SetGamePlayerStateFlashlightCastShadow);
	lua_register(lua2, "GetGamePlayerStateFlashlightCastShadow", GetGamePlayerStateFlashlightCastShadow);

	lua_register(lua2, "SetGamePlayerStateMoving" , SetGamePlayerStateMoving );
	lua_register(lua2, "GetGamePlayerStateMoving" , GetGamePlayerStateMoving );
	lua_register(lua2, "SetGamePlayerStateTerrainHeight" , SetGamePlayerStateTerrainHeight );
	lua_register(lua2, "GetGamePlayerStateTerrainHeight" , GetGamePlayerStateTerrainHeight );
	lua_register(lua2, "SetGamePlayerStateJetpackVerticalMove" , SetGamePlayerStateJetpackVerticalMove );
	lua_register(lua2, "GetGamePlayerStateJetpackVerticalMove" , GetGamePlayerStateJetpackVerticalMove );
	lua_register(lua2, "SetGamePlayerStateTerrainID" , SetGamePlayerStateTerrainID );
	lua_register(lua2, "GetGamePlayerStateTerrainID" , GetGamePlayerStateTerrainID );
	lua_register(lua2, "SetGamePlayerStateEnablePlrSpeedMods" , SetGamePlayerStateEnablePlrSpeedMods );
	lua_register(lua2, "GetGamePlayerStateEnablePlrSpeedMods" , GetGamePlayerStateEnablePlrSpeedMods );
	lua_register(lua2, "SetGamePlayerStateRiftMode" , SetGamePlayerStateRiftMode );
	lua_register(lua2, "GetGamePlayerStateRiftMode" , GetGamePlayerStateRiftMode );
	lua_register(lua2, "SetGamePlayerStatePlayerX" , SetGamePlayerStatePlayerX );
	lua_register(lua2, "GetGamePlayerStatePlayerX" , GetGamePlayerStatePlayerX );
	lua_register(lua2, "SetGamePlayerStatePlayerY" , SetGamePlayerStatePlayerY );
	lua_register(lua2, "GetGamePlayerStatePlayerY" , GetGamePlayerStatePlayerY );
	lua_register(lua2, "SetGamePlayerStatePlayerZ" , SetGamePlayerStatePlayerZ );
	lua_register(lua2, "GetGamePlayerStatePlayerZ" , GetGamePlayerStatePlayerZ );
	lua_register(lua2, "SetGamePlayerStateTerrainPlayerX" , SetGamePlayerStateTerrainPlayerX );
	lua_register(lua2, "GetGamePlayerStateTerrainPlayerX" , GetGamePlayerStateTerrainPlayerX );
	lua_register(lua2, "SetGamePlayerStateTerrainPlayerY" , SetGamePlayerStateTerrainPlayerY );
	lua_register(lua2, "GetGamePlayerStateTerrainPlayerY" , GetGamePlayerStateTerrainPlayerY );
	lua_register(lua2, "SetGamePlayerStateTerrainPlayerZ" , SetGamePlayerStateTerrainPlayerZ );
	lua_register(lua2, "GetGamePlayerStateTerrainPlayerZ" , GetGamePlayerStateTerrainPlayerZ );
	lua_register(lua2, "SetGamePlayerStateTerrainPlayerAX" , SetGamePlayerStateTerrainPlayerAX );
	lua_register(lua2, "GetGamePlayerStateTerrainPlayerAX" , GetGamePlayerStateTerrainPlayerAX );
	lua_register(lua2, "SetGamePlayerStateTerrainPlayerAY" , SetGamePlayerStateTerrainPlayerAY );
	lua_register(lua2, "GetGamePlayerStateTerrainPlayerAY" , GetGamePlayerStateTerrainPlayerAY );
	lua_register(lua2, "SetGamePlayerStateTerrainPlayerAZ" , SetGamePlayerStateTerrainPlayerAZ );
	lua_register(lua2, "GetGamePlayerStateTerrainPlayerAZ" , GetGamePlayerStateTerrainPlayerAZ );
	lua_register(lua2, "SetGamePlayerStateAdjustBasedOnWobbleY" , SetGamePlayerStateAdjustBasedOnWobbleY );
	lua_register(lua2, "GetGamePlayerStateAdjustBasedOnWobbleY" , GetGamePlayerStateAdjustBasedOnWobbleY );
	lua_register(lua2, "SetGamePlayerStateFinalCamX" , SetGamePlayerStateFinalCamX );
	lua_register(lua2, "GetGamePlayerStateFinalCamX" , GetGamePlayerStateFinalCamX );
	lua_register(lua2, "SetGamePlayerStateFinalCamY" , SetGamePlayerStateFinalCamY );
	lua_register(lua2, "GetGamePlayerStateFinalCamY" , GetGamePlayerStateFinalCamY );
	lua_register(lua2, "SetGamePlayerStateFinalCamZ" , SetGamePlayerStateFinalCamZ );
	lua_register(lua2, "GetGamePlayerStateFinalCamZ" , GetGamePlayerStateFinalCamZ );
	lua_register(lua2, "SetGamePlayerStateShakeX" , SetGamePlayerStateShakeX );
	lua_register(lua2, "GetGamePlayerStateShakeX" , GetGamePlayerStateShakeX );
	lua_register(lua2, "SetGamePlayerStateShakeY" , SetGamePlayerStateShakeY );
	lua_register(lua2, "GetGamePlayerStateShakeY" , GetGamePlayerStateShakeY );
	lua_register(lua2, "SetGamePlayerStateShakeZ" , SetGamePlayerStateShakeZ );
	lua_register(lua2, "GetGamePlayerStateShakeZ" , GetGamePlayerStateShakeZ );
	lua_register(lua2, "SetGamePlayerStateImmunity" , SetGamePlayerStateImmunity );
	lua_register(lua2, "GetGamePlayerStateImmunity" , GetGamePlayerStateImmunity );
	lua_register(lua2, "SetGamePlayerStateCharAnimIndex" , SetGamePlayerStateCharAnimIndex );
	lua_register(lua2, "GetGamePlayerStateCharAnimIndex" , GetGamePlayerStateCharAnimIndex );

	lua_register(lua2, "SetGamePlayerStateCounteredAction", SetGamePlayerStateCounteredAction);
	lua_register(lua2, "GetGamePlayerStateCounteredAction", GetGamePlayerStateCounteredAction);

	lua_register(lua2, "GetCharacterForwardX", GetCharacterForwardX);
	lua_register(lua2, "GetCharacterForwardY", GetCharacterForwardY);
	lua_register(lua2, "GetCharacterForwardZ", GetCharacterForwardZ);

	lua_register(lua2, "GetGamePlayerStateMotionController" , GetGamePlayerStateMotionController );
	lua_register(lua2, "GetGamePlayerStateMotionControllerType" , GetGamePlayerStateMotionControllerType );
	lua_register(lua2, "MotionControllerThumbstickX" , MotionControllerThumbstickX );
	lua_register(lua2, "MotionControllerThumbstickY" , MotionControllerThumbstickY );
	lua_register(lua2, "CombatControllerTrigger" , CombatControllerTrigger );
	lua_register(lua2, "CombatControllerGrip", CombatControllerGrip);
	lua_register(lua2, "CombatControllerButtonA", CombatControllerButtonA);
	lua_register(lua2, "CombatControllerButtonB", CombatControllerButtonB);
	lua_register(lua2, "CombatControllerThumbstickX" , CombatControllerThumbstickX );
	lua_register(lua2, "CombatControllerThumbstickY" , CombatControllerThumbstickY );
	lua_register(lua2, "MotionControllerBestX" , MotionControllerBestX );
	lua_register(lua2, "MotionControllerBestY" , MotionControllerBestY );
	lua_register(lua2, "MotionControllerBestZ" , MotionControllerBestZ );
	lua_register(lua2, "MotionControllerBestAngleX" , MotionControllerBestAngleX );
	lua_register(lua2, "MotionControllerBestAngleY" , MotionControllerBestAngleY );
	lua_register(lua2, "MotionControllerBestAngleZ" , MotionControllerBestAngleZ );
	lua_register(lua2, "MotionControllerLaserGuidedEntityObj", MotionControllerLaserGuidedEntityObj);
	lua_register(lua2, "CombatControllerLaserGuidedHit", CombatControllerLaserGuidedHit);

	lua_register(lua2, "SetGamePlayerStateIsMelee" , SetGamePlayerStateIsMelee );
	lua_register(lua2, "GetGamePlayerStateIsMelee" , GetGamePlayerStateIsMelee );
	lua_register(lua2, "SetGamePlayerStateAlternate" , SetGamePlayerStateAlternate );
	lua_register(lua2, "GetGamePlayerStateAlternate" , GetGamePlayerStateAlternate );
	lua_register(lua2, "SetGamePlayerStateModeShareMags" , SetGamePlayerStateModeShareMags );
	lua_register(lua2, "GetGamePlayerStateModeShareMags" , GetGamePlayerStateModeShareMags );
	lua_register(lua2, "SetGamePlayerStateAlternateIsFlak" , SetGamePlayerStateAlternateIsFlak );
	lua_register(lua2, "GetGamePlayerStateAlternateIsFlak" , GetGamePlayerStateAlternateIsFlak );
	lua_register(lua2, "SetGamePlayerStateAlternateIsRay" , SetGamePlayerStateAlternateIsRay );
	lua_register(lua2, "GetGamePlayerStateAlternateIsRay" , GetGamePlayerStateAlternateIsRay );
	lua_register(lua2, "SetFireModeSettingsReloadQty" , SetFireModeSettingsReloadQty );
	lua_register(lua2, "GetFireModeSettingsReloadQty" , GetFireModeSettingsReloadQty );
	lua_register(lua2, "SetFireModeSettingsIsEmpty" , SetFireModeSettingsIsEmpty );
	lua_register(lua2, "GetFireModeSettingsIsEmpty" , GetFireModeSettingsIsEmpty );
	lua_register(lua2, "SetFireModeSettingsJammed" , SetFireModeSettingsJammed );
	lua_register(lua2, "GetFireModeSettingsJammed" , GetFireModeSettingsJammed );
	lua_register(lua2, "SetFireModeSettingsJamChance" , SetFireModeSettingsJamChance );
	lua_register(lua2, "GetFireModeSettingsJamChance" , GetFireModeSettingsJamChance );
	lua_register(lua2, "SetFireModeSettingsMinTimer" , SetFireModeSettingsMinTimer );
	lua_register(lua2, "GetFireModeSettingsMinTimer" , GetFireModeSettingsMinTimer );
	lua_register(lua2, "SetFireModeSettingsAddTimer" , SetFireModeSettingsAddTimer );
	lua_register(lua2, "GetFireModeSettingsAddTimer" , GetFireModeSettingsAddTimer );
	lua_register(lua2, "SetFireModeSettingsShotsFired" , SetFireModeSettingsShotsFired );
	lua_register(lua2, "GetFireModeSettingsShotsFired" , GetFireModeSettingsShotsFired );
	lua_register(lua2, "SetFireModeSettingsCoolTimer" , SetFireModeSettingsCoolTimer );
	lua_register(lua2, "GetFireModeSettingsCoolTimer" , GetFireModeSettingsCoolTimer );
	lua_register(lua2, "SetFireModeSettingsOverheatAfter" , SetFireModeSettingsOverheatAfter );
	lua_register(lua2, "GetFireModeSettingsOverheatAfter" , GetFireModeSettingsOverheatAfter );
	lua_register(lua2, "SetFireModeSettingsJamChanceTime" , SetFireModeSettingsJamChanceTime );
	lua_register(lua2, "GetFireModeSettingsJamChanceTime" , GetFireModeSettingsJamChanceTime );
	lua_register(lua2, "SetFireModeSettingsCoolDown" , SetFireModeSettingsCoolDown );
	lua_register(lua2, "GetFireModeSettingsCoolDown" , GetFireModeSettingsCoolDown );
	lua_register(lua2, "SetFireModeSettingsNoSubmergedFire" , SetFireModeSettingsNoSubmergedFire );
	lua_register(lua2, "GetFireModeSettingsNoSubmergedFire" , GetFireModeSettingsNoSubmergedFire );
	lua_register(lua2, "SetFireModeSettingsSimpleZoom" , SetFireModeSettingsSimpleZoom );
	lua_register(lua2, "GetFireModeSettingsSimpleZoom" , GetFireModeSettingsSimpleZoom );
	lua_register(lua2, "SetFireModeSettingsForceZoomOut" , SetFireModeSettingsForceZoomOut );
	lua_register(lua2, "GetFireModeSettingsForceZoomOut" , GetFireModeSettingsForceZoomOut );
	lua_register(lua2, "SetFireModeSettingsZoomMode" , SetFireModeSettingsZoomMode );
	lua_register(lua2, "GetFireModeSettingsZoomMode" , GetFireModeSettingsZoomMode );
	lua_register(lua2, "SetFireModeSettingsSimpleZoomAnim" , SetFireModeSettingsSimpleZoomAnim );
	lua_register(lua2, "GetFireModeSettingsSimpleZoomAnim" , GetFireModeSettingsSimpleZoomAnim );
	lua_register(lua2, "SetFireModeSettingsPoolIndex" , SetFireModeSettingsPoolIndex );
	lua_register(lua2, "GetFireModeSettingsPoolIndex" , GetFireModeSettingsPoolIndex );
	lua_register(lua2, "SetFireModeSettingsPlrTurnSpeedMod" , SetFireModeSettingsPlrTurnSpeedMod );
	lua_register(lua2, "GetFireModeSettingsPlrTurnSpeedMod" , GetFireModeSettingsPlrTurnSpeedMod );
	lua_register(lua2, "SetFireModeSettingsZoomTurnSpeed" , SetFireModeSettingsZoomTurnSpeed );
	lua_register(lua2, "GetFireModeSettingsZoomTurnSpeed" , GetFireModeSettingsZoomTurnSpeed );
	lua_register(lua2, "SetFireModeSettingsPlrJumpSpeedMod" , SetFireModeSettingsPlrJumpSpeedMod );
	lua_register(lua2, "GetFireModeSettingsPlrJumpSpeedMod" , GetFireModeSettingsPlrJumpSpeedMod );
	lua_register(lua2, "SetFireModeSettingsPlrEmptySpeedMod" , SetFireModeSettingsPlrEmptySpeedMod );
	lua_register(lua2, "GetFireModeSettingsPlrEmptySpeedMod" , GetFireModeSettingsPlrEmptySpeedMod );
	lua_register(lua2, "SetFireModeSettingsPlrMoveSpeedMod" , SetFireModeSettingsPlrMoveSpeedMod );
	lua_register(lua2, "GetFireModeSettingsPlrMoveSpeedMod" , GetFireModeSettingsPlrMoveSpeedMod );
	lua_register(lua2, "SetFireModeSettingsZoomWalkSpeed" , SetFireModeSettingsZoomWalkSpeed );
	lua_register(lua2, "GetFireModeSettingsZoomWalkSpeed" , GetFireModeSettingsZoomWalkSpeed );
	lua_register(lua2, "SetFireModeSettingsPlrReloadSpeedMod" , SetFireModeSettingsPlrReloadSpeedMod );
	lua_register(lua2, "GetFireModeSettingsPlrReloadSpeedMod" , GetFireModeSettingsPlrReloadSpeedMod );
	lua_register(lua2, "SetFireModeSettingsHasEmpty" , SetFireModeSettingsHasEmpty );
	lua_register(lua2, "GetFireModeSettingsHasEmpty" , GetFireModeSettingsHasEmpty );
	lua_register(lua2, "GetFireModeSettingsActionBlockStart", GetFireModeSettingsActionBlockStart);
	lua_register(lua2, "GetFireModeSettingsMeleeWithRightClick", GetFireModeSettingsMeleeWithRightClick);
	lua_register(lua2, "GetFireModeSettingsBlockWithRightClick", GetFireModeSettingsBlockWithRightClick);
		
	lua_register(lua2, "SetGamePlayerStateGunSound" , SetGamePlayerStateGunSound );
	lua_register(lua2, "GetGamePlayerStateGunSound" , GetGamePlayerStateGunSound );
	lua_register(lua2, "SetGamePlayerStateGunAltSound" , SetGamePlayerStateGunAltSound );
	lua_register(lua2, "GetGamePlayerStateGunAltSound" , GetGamePlayerStateGunAltSound );

	lua_register(lua2, "CopyCharAnimState" , CopyCharAnimState );
	lua_register(lua2, "SetCharAnimStatePlayCsi" , SetCharAnimStatePlayCsi );
	lua_register(lua2, "GetCharAnimStatePlayCsi" , GetCharAnimStatePlayCsi );
	lua_register(lua2, "SetCharAnimStateOriginalE" , SetCharAnimStateOriginalE );
	lua_register(lua2, "GetCharAnimStateOriginalE" , GetCharAnimStateOriginalE );
	lua_register(lua2, "SetCharAnimStateObj" , SetCharAnimStateObj );
	lua_register(lua2, "GetCharAnimStateObj" , GetCharAnimStateObj );
	lua_register(lua2, "SetCharAnimStateAnimationSpeed" , SetCharAnimStateAnimationSpeed );
	lua_register(lua2, "GetCharAnimStateAnimationSpeed" , GetCharAnimStateAnimationSpeed );
	lua_register(lua2, "SetCharAnimStateE" , SetCharAnimStateE );
	lua_register(lua2, "GetCharAnimStateE" , GetCharAnimStateE );

	lua_register(lua2, "GetCsiStoodVault" , GetCsiStoodVault );
	lua_register(lua2, "GetCharSeqTrigger" , GetCharSeqTrigger );
	lua_register(lua2, "GetEntityElementBankIndex" , GetEntityElementBankIndex );
	lua_register(lua2, "GetEntityElementObj" , GetEntityElementObj );
	lua_register(lua2, "GetEntityElementRagdollified" , GetEntityElementRagdollified );
	lua_register(lua2, "GetEntityElementSpeedModulator" , GetEntityElementSpeedModulator );
	lua_register(lua2, "GetEntityProfileJumpModifier" , GetEntityProfileJumpModifier );
	lua_register(lua2, "GetEntityProfileStartOfAIAnim" , GetEntityProfileStartOfAIAnim );
	lua_register(lua2, "GetEntityProfileJumpHold" , GetEntityProfileJumpHold );
	lua_register(lua2, "GetEntityProfileJumpResume" , GetEntityProfileJumpResume );

	lua_register(lua2, "SetCharAnimControlsLeaping" , SetCharAnimControlsLeaping );
	lua_register(lua2, "GetCharAnimControlsLeaping" , GetCharAnimControlsLeaping );
	lua_register(lua2, "SetCharAnimControlsMoving" , SetCharAnimControlsMoving );
	lua_register(lua2, "GetCharAnimControlsMoving" , GetCharAnimControlsMoving );
	
	lua_register(lua2, "GetEntityAnimStart" , GetEntityAnimStart );
	lua_register(lua2, "GetEntityAnimFinish" , GetEntityAnimFinish );

	lua_register(lua2, "SetRotationYSlowly" , SetRotationYSlowly );

	// OLD Particle System
	lua_register(lua2, "ParticlesGetFreeEmitter" , ParticlesGetFreeEmitter );
	lua_register(lua2, "ParticlesAddEmitter" ,     ParticlesAddEmitter );
	lua_register(lua2, "ParticlesAddEmitterEx" ,   ParticlesAddEmitterEx );
	lua_register(lua2, "ParticlesDeleteEmitter" ,  ParticlesDeleteEmitter );
	lua_register(lua2, "ParticlesSpawnParticle",   ParticlesSpawnParticle );
	lua_register(lua2, "ParticlesLoadImage",       ParticlesLoadImage );
	lua_register(lua2, "ParticlesLoadEffect",      ParticlesLoadEffect );
	lua_register(lua2, "ParticlesSetFrames",       ParticlesSetFrames );
	lua_register(lua2, "ParticlesSetSpeed",        ParticlesSetSpeed );
	lua_register(lua2, "ParticlesSetGravity",      ParticlesSetGravity );
	lua_register(lua2, "ParticlesSetOffset",       ParticlesSetOffset );
	lua_register(lua2, "ParticlesSetAngle",        ParticlesSetAngle );
	lua_register(lua2, "ParticlesSetRotation",     ParticlesSetRotation );
	lua_register(lua2, "ParticlesSetScale",        ParticlesSetScale );
	lua_register(lua2, "ParticlesSetAlpha",        ParticlesSetAlpha );
	lua_register(lua2, "ParticlesSetLife",         ParticlesSetLife );
	lua_register(lua2, "ParticlesSetWindVector",   ParticlesSetWindVector );
	lua_register(lua2, "ParticlesSetNoWind",       ParticlesSetNoWind );

	// NEW Particle Effects System
	lua_register(lua2, "EffectStart",				EffectStart);
	lua_register(lua2, "EffectStop",					EffectStop);
	lua_register(lua2, "EffectSetLocalPosition",		EffectSetLocalPosition);
	lua_register(lua2, "EffectSetLocalRotation",		EffectSetLocalRotation);
	lua_register(lua2, "EffectSetSpeed",				EffectSetSpeed);
	lua_register(lua2, "EffectSetOpacity",			EffectSetOpacity);
	lua_register(lua2, "EffectSetParticleSize",		EffectSetParticleSize);
	lua_register(lua2, "EffectSetBurstMode",			EffectSetBurstMode);
	lua_register(lua2, "EffectFireBurst",			EffectFireBurst);
	lua_register(lua2, "EffectSetFloorReflection",	EffectSetFloorReflection);
	lua_register(lua2, "EffectSetBounciness",		EffectSetBounciness);
	lua_register(lua2, "EffectSetColor",				EffectSetColor);
	lua_register(lua2, "EffectSetLifespan",			EffectSetLifespan);

#ifdef WICKEDPARTICLESYSTEM
	//PE: Wicked particle system.
	lua_register(lua2, "WParticleEffectLoad", WParticleEffectLoad);
	lua_register(lua2, "WParticleEffectPosition", WParticleEffectPosition);
	lua_register(lua2, "WParticleEffectVisible", WParticleEffectVisible);
	lua_register(lua2, "WParticleEffectAction", WParticleEffectAction);
#endif

	//PE: Other missing commands.
	lua_register(lua2, "PositionSound", entity_lua_positionsound);
	lua_register(lua2, "AddTracer", AddTracer);
	lua_register(lua2, "LoadTracerImage", LoadTracerImage);


	lua_register(lua2, "GetBulletHit",             GetBulletHit);
	lua_register(lua2, "SetFlashLight" , SetFlashLight );
	lua_register(lua2, "SetFlashLightPosition", SetFlashLightPosition);
	lua_register(lua2, "GetFlashLightPosition", GetFlashLightPosition);

	lua_register(lua2, "SetAttachmentVisible" , SetAttachmentVisible );
	lua_register(lua2, "SetOcclusion" , SetOcclusion );
	lua_register(lua2, "SetPlayerWeapons", SetPlayerWeapons);
	lua_register(lua2, "FirePlayerWeapon", FirePlayerWeapon);
	lua_register(lua2, "SetFlashLightKeyEnabled" , SetFlashLightKeyEnabled );
	lua_register(lua2, "SetPlayerRun" , SetPlayerRun );
	lua_register(lua2, "SetFont" , SetFont );
	lua_register(lua2, "GetDeviceWidth" , GetDeviceWidth );
	lua_register(lua2, "GetDeviceHeight" , GetDeviceHeight );
	lua_register(lua2, "GetFirstEntitySpawn" , GetFirstEntitySpawn );
	lua_register(lua2, "GetNextEntitySpawn" , GetFirstEntitySpawn );	

	// VR and head tracking
	lua_register(lua2, "GetHeadTracker" , GetHeadTracker );	
	lua_register(lua2, "ResetHeadTracker" , ResetHeadTracker );	
	lua_register(lua2, "GetHeadTrackerYaw" , GetHeadTrackerYaw );	
	lua_register(lua2, "GetHeadTrackerPitch" , GetHeadTrackerPitch );	
	lua_register(lua2, "GetHeadTrackerRoll" , GetHeadTrackerRoll );	
	lua_register(lua2, "GetHeadTrackerNormalX" , GetHeadTrackerNormalX );	
	lua_register(lua2, "GetHeadTrackerNormalY" , GetHeadTrackerNormalY );	
	lua_register(lua2, "GetHeadTrackerNormalZ" , GetHeadTrackerNormalZ );		

	// Prompt3D
	lua_register(lua2, "Prompt3D" , Prompt3D );	
	lua_register(lua2, "PositionPrompt3D" , PositionPrompt3D );	
	lua_register(lua2, "PromptLocalDuration", PromptLocalDuration);

	// utility
	lua_register(lua2, "MsgBox" , MsgBox );
	lua_register(lua2, "IsTestGame", GetIsTestgame);

	//Cloud Shader
	lua_register(lua2, "GetCloudDensity", GetCloudDensity);
	lua_register(lua2, "GetCloudCoverage", GetCloudCoverage);
	lua_register(lua2, "GetCloudHeight", GetCloudHeight);
	lua_register(lua2, "GetCloudThickness", GetCloudThickness);
	lua_register(lua2, "GetCloudSpeed", GetCloudSpeed);
	lua_register(lua2, "SetCloudDensity", SetCloudDensity);
	lua_register(lua2, "SetCloudCoverage", SetCloudCoverage);
	lua_register(lua2, "SetCloudHeight", SetCloudHeight);
	lua_register(lua2, "SetCloudThickness", SetCloudThickness);
	lua_register(lua2, "SetCloudSpeed", SetCloudSpeed);

	//PE: Other Shader
	lua_register(lua2, "SetTreeWind", SetTreeWind);
	lua_register(lua2, "GetTreeWind", GetTreeWind);

	//Water Shader
	//setter
	lua_register(lua2, "SetWaterHeight", SetWaterHeight);
	lua_register(lua2, "SetWaterColor", SetWaterShaderColor);
	lua_register(lua2, "SetWaterWaveIntensity", SetWaterWaveIntensity);
	lua_register(lua2, "SetWaterTransparancy", SetWaterTransparancy);
	lua_register(lua2, "SetWaterReflection", SetWaterReflection);
	lua_register(lua2, "SetWaterReflectionSparkleIntensity", SetWaterReflectionSparkleIntensity);
	lua_register(lua2, "SetWaterFlowDirection", SetWaterFlowDirection);
	lua_register(lua2, "SetWaterDistortionWaves", SetWaterDistortionWaves);
	lua_register(lua2, "SetRippleWaterSpeed", SetRippleWaterSpeed);
	//getter
	lua_register(lua2, "GetWaterHeight", GetWaterHeight);
	lua_register(lua2, "GetWaterWaveIntensity", GetWaterWaveIntensity);
	lua_register(lua2, "GetWaterShaderColorRed", GetWaterShaderColorRed);
	lua_register(lua2, "GetWaterShaderColorGreen", GetWaterShaderColorGreen);
	lua_register(lua2, "GetWaterShaderColorBlue", GetWaterShaderColorBlue);
	lua_register(lua2, "GetWaterTransparancy", GetWaterTransparancy);
	lua_register(lua2, "GetWaterReflection", GetWaterReflection);
	lua_register(lua2, "GetWaterReflectionSparkleIntensity", GetWaterReflectionSparkleIntensity);
	lua_register(lua2, "GetWaterFlowDirectionX", GetWaterFlowDirectionX);
	lua_register(lua2, "GetWaterFlowDirectionY", GetWaterFlowDirectionY);
	lua_register(lua2, "GetWaterFlowSpeed", GetWaterFlowSpeed);
	lua_register(lua2, "GetWaterDistortionWaves", GetWaterDistortionWaves);
	lua_register(lua2, "GetRippleWaterSpeed", GetRippleWaterSpeed); 
	lua_register(lua2, "GetWaterEnabled", GetWaterEnabled);

	#ifdef STORYBOARD
	//Storyboard
	lua_register(lua2, "SetScreenHUDGlobalScale", SetScreenHUDGlobalScale);
	lua_register(lua2, "InitScreen", InitScreen);
	lua_register(lua2, "DisplayScreen", DisplayScreen);
	lua_register(lua2, "DisplayCurrentScreen", DisplayCurrentScreen);
	lua_register(lua2, "GetCurrentScreen", GetCurrentScreen);
	lua_register(lua2, "GetCurrentScreenName", GetCurrentScreenName);
	lua_register(lua2, "CheckScreenToggles", CheckScreenToggles);
	lua_register(lua2, "DisableGunFireInHUD", DisableGunFireInHUD);
	lua_register(lua2, "EnableGunFireInHUD", EnableGunFireInHUD);
	lua_register(lua2, "DisableBoundHudKeys", DisableBoundHudKeys);
	lua_register(lua2, "EnableBoundHudKeys", EnableBoundHudKeys);
	
	lua_register(lua2, "ScreenToggle", ScreenToggle);
	lua_register(lua2, "ScreenToggleByKey", ScreenToggleByKey);
	lua_register(lua2, "GetIfUsingTABScreen", GetIfUsingTABScreen);
	lua_register(lua2, "GetStoryboardActive", GetStoryboardActive);
	lua_register(lua2, "GetScreenWidgetValue", GetScreenWidgetValue);
	lua_register(lua2, "SetScreenWidgetValue", SetScreenWidgetValue);
	lua_register(lua2, "SetScreenWidgetSelection", SetScreenWidgetSelection);
	lua_register(lua2, "GetScreenElementsType", GetScreenElementsType);
	lua_register(lua2, "GetScreenElementTypeID", GetScreenElementTypeID);
	lua_register(lua2, "GetScreenElements", GetScreenElements);
	lua_register(lua2, "GetScreenElementID", GetScreenElementID);
	lua_register(lua2, "GetScreenElementImage", GetScreenElementImage);
	lua_register(lua2, "GetScreenElementArea", GetScreenElementArea);
	lua_register(lua2, "GetScreenElementDetails", GetScreenElementDetails);
	lua_register(lua2, "GetScreenElementName", GetScreenElementName);
	lua_register(lua2, "SetScreenElementVisibility", SetScreenElementVisibility);
	lua_register(lua2, "SetScreenElementPosition", SetScreenElementPosition);
	lua_register(lua2, "SetScreenElementText", SetScreenElementText);
	lua_register(lua2, "SetScreenElementColor", SetScreenElementColor);
	lua_register(lua2, "GetCollectionAttributeQuantity", GetCollectionAttributeQuantity);
	lua_register(lua2, "GetCollectionAttributeLabel", GetCollectionAttributeLabel);
	lua_register(lua2, "GetCollectionItemQuantity", GetCollectionItemQuantity);
	lua_register(lua2, "GetCollectionItemAttribute", GetCollectionItemAttribute);	
	lua_register(lua2, "GetCollectionQuestAttributeQuantity", GetCollectionQuestAttributeQuantity);
	lua_register(lua2, "GetCollectionQuestAttributeLabel", GetCollectionQuestAttributeLabel);
	lua_register(lua2, "GetCollectionQuestQuantity", GetCollectionQuestQuantity);
	lua_register(lua2, "GetCollectionQuestAttribute", GetCollectionQuestAttribute);
	lua_register(lua2, "MakeInventoryContainer", MakeInventoryContainer);
	lua_register(lua2, "GetInventoryTotal", GetInventoryTotal);
	lua_register(lua2, "GetInventoryName", GetInventoryName);
	lua_register(lua2, "GetInventoryExist", GetInventoryExist);
	lua_register(lua2, "GetInventoryQuantity", GetInventoryQuantity);
	lua_register(lua2, "GetInventoryItem", GetInventoryItem);
	lua_register(lua2, "GetInventoryItemID", GetInventoryItemID);
	lua_register(lua2, "GetInventoryItemSlot", GetInventoryItemSlot);
	lua_register(lua2, "SetInventoryItemSlot", SetInventoryItemSlot);
	lua_register(lua2, "MoveInventoryItem", MoveInventoryItem);
	lua_register(lua2, "DeleteAllInventoryContainers", DeleteAllInventoryContainers);
	lua_register(lua2, "AddInventoryItem", AddInventoryItem);
	#endif

	lua_register(lua2, "SetCharacterDirectionOverride", SetCharacterDirectionOverride);
	lua_register(lua2, "LimitSwimmingVerticalMovement", LimitSwimmingVerticalMovement);

	// material commands
	lua_register(lua2, "SetEntityBaseColor", SetEntityBaseColor);
	lua_register(lua2, "GetEntityBaseColor", GetEntityBaseColor);
	lua_register(lua2, "SetEntityBaseAlpha", SetEntityBaseAlpha);
	lua_register(lua2, "GetEntityBaseAlpha", GetEntityBaseAlpha);
	lua_register(lua2, "SetEntityAlphaClipping", SetEntityAlphaClipping);
	lua_register(lua2, "GetEntityAlphaClipping", GetEntityAlphaClipping);
	lua_register(lua2, "SetEntityNormalStrength", SetEntityNormalStrength);
	lua_register(lua2, "GetEntityNormalStrength", GetEntityNormalStrength);
	lua_register(lua2, "SetEntityRoughnessStrength", SetEntityRoughnessStrength);
	lua_register(lua2, "GetEntityRoughnessStrength", GetEntityRoughnessStrength);
	lua_register(lua2, "SetEntityMetalnessStrength", SetEntityMetalnessStrength);
	lua_register(lua2, "GetEntityMetalnessStrength", GetEntityMetalnessStrength);
	lua_register(lua2, "SetEntityEmissiveColor", SetEntityEmissiveColor);
	lua_register(lua2, "GetEntityEmissiveColor", GetEntityEmissiveColor);
	lua_register(lua2, "SetEntityEmissiveStrength", SetEntityEmissiveStrength);
	lua_register(lua2, "GetEntityEmissiveStrength", GetEntityEmissiveStrength);
	lua_register(lua2, "SetEntityReflectance", SetEntityReflectance);
	lua_register(lua2, "GetEntityReflectance", GetEntityReflectance);
	lua_register(lua2, "SetEntityRenderBias", SetEntityRenderBias);
	lua_register(lua2, "GetEntityRenderBias", GetEntityRenderBias);
	lua_register(lua2, "SetEntityTransparency", SetEntityTransparency);
	lua_register(lua2, "GetEntityTransparency", GetEntityTransparency);
	lua_register(lua2, "SetEntityDoubleSided", SetEntityDoubleSided);
	lua_register(lua2, "GetEntityDoubleSided", GetEntityDoubleSided);
	lua_register(lua2, "SetEntityPlanarReflection", SetEntityPlanarReflection);
	lua_register(lua2, "GetEntityPlanarReflection", GetEntityPlanarReflection);
	lua_register(lua2, "SetEntityCastShadows", SetEntityCastShadows);
	lua_register(lua2, "GetEntityCastShadows", GetEntityCastShadows);
	lua_register(lua2, "SetEntityZDepthMode", SetEntityZDepthMode);
	lua_register(lua2, "GetEntityZDepthMode", GetEntityZDepthMode);
	lua_register(lua2, "SetEntityOutline", SetEntityOutline);
	lua_register(lua2, "GetEntityOutline", GetEntityOutline);

	// texture commands
	lua_register(lua2, "SetEntityTexture", SetEntityTexture);
	lua_register(lua2, "SetEntityTextureScale", SetEntityTextureScale);
	lua_register(lua2, "SetEntityTextureOffset", SetEntityTextureOffset);

	lua_register(lua2, "SetWeaponArmsVisible", SetWeaponArmsVisible);
	lua_register(lua2, "GetWeaponArmsVisible", GetWeaponArmsVisible);

	lua_register(lua2, "GetEntityInZoneWithFilter", GetEntityInZoneWithFilter);
	lua_register(lua2, "IsPointWithinZone", IsPointWithinZone);

	// In-game HUD
	lua_register(lua2, "IsPlayerInGame", IsPlayerInGame);
	lua_register(lua2, "SetLevelFadeoutEnabled", SetLevelFadeoutEnabled);

	// Sun
	lua_register(lua2, "SetSunDirection", SetSunDirection);
	lua_register(lua2, "SetSunLightingColor", SetSunLightingColor);
	lua_register(lua2, "SetSunIntensity", SetSunIntensity);
	lua_register(lua2, "GetSunIntensity", GetSunIntensity);
	// ★ GGMAX 3.38: the DX11 parity set (see DarkLUA_part6.cpp / _part4.cpp).
	lua_register(lua2, "GetSunColorRed", GetSunColorRed);
	lua_register(lua2, "GetSunColorGreen", GetSunColorGreen);
	lua_register(lua2, "GetSunColorBlue", GetSunColorBlue);
	lua_register(lua2, "ForceMouseXYClick", ForceMouseXYClick);

	// Lighting
	lua_register(lua2, "SetExposure", SetExposure);
	lua_register(lua2, "GetExposure", GetExposure);

	lua_register(lua2, "SetLutTo", lua_set_lut);
	lua_register(lua2, "GetLut", lua_get_lut);
	lua_register(lua2, "PromptLocalOffset", PromptLocalOffset);
	lua_register(lua2, "PromptGuruMeditation", PromptGuruMeditation);

	//Other effects.
	lua_register(lua2, "SetGrassScale", SetGrassScale);
	lua_register(lua2, "GetGrassScale", GetGrassScale);
	lua_register(lua2, "GunAnimationSetFrame", GunAnimationSetFrame);
	lua_register(lua2, "LoopGunAnimation", LoopGunAnimation);
	lua_register(lua2, "StopGunAnimation", StopGunAnimation);
	lua_register(lua2, "PlayGunAnimation", PlayGunAnimation);
	lua_register(lua2, "GunAnimationPlaying", GunAnimationPlaying);
	lua_register(lua2, "GetGunAnimationFramesFromName", GetGunAnimationFramesFromName);
	lua_register(lua2, "SetGunAnimationSpeed", SetGunAnimationSpeed);
	lua_register(lua2, "ForceGunUnderWater", ForceGunUnderWater);
	lua_register(lua2, "GetGunEmissiveStrength", GetGunEmissiveStrength);
	lua_register(lua2, "SetGunEmissiveStrength", SetGunEmissiveStrength);

	addFunctions_hiredgun();
}

 /*
 struct luaMessage
{
	char msgDesc[256];
	int msgInt;
	float msgFloat;
	char msgString[256];
};

luaMessage currentMessage;

int luaMessageCount = 0;
int maxLuaMessages = 0;
luaMessage** ppLuaMessages = NULL;
 */

char szLuaReturnString[1024];

 DARKLUA_API LPSTR LuaMessageDesc ( void )
 {
  	// Return string pointer	
	const char *s = currentMessage.msgDesc;

	// If input string valid
	if(s)
	{
		strcpy(szLuaReturnString, s);
	}
	else
	{
		strcpy(szLuaReturnString, "");
	}

	return GetReturnStringFromTEXTWorkString( szLuaReturnString );
 }

 DARKLUA_API int LuaMessageIndex ()
 {
	return currentMessage.msgIndex;
 }

 DARKLUA_API float LuaMessageFloat ()
 {
	return currentMessage.msgFloat;
 }

  DARKLUA_API int LuaMessageInt ()
 {
	return currentMessage.msgInt;
 }

  char szLuaMessageString[1024];

 DARKLUA_API LPSTR LuaMessageString ( void )
 {
  	// Return string pointer
	LPSTR pReturnString=NULL;
	const char *s = currentMessage.msgString;

	// If input string valid
	if(s)
	{
		strcpy(szLuaMessageString, s);
	}
	else
	{
		strcpy(szLuaMessageString, "");
	}

	return GetReturnStringFromTEXTWorkString( szLuaMessageString );
 }

 DARKLUA_API int LuaNext()
 {
	if ( luaMessageCount == 0 ) 
	{
		strcpy ( currentMessage.msgDesc, "" );
		currentMessage.msgFloat = 0.0f;
		currentMessage.msgInt = 0;
		currentMessage.msgIndex = 0;
		strcpy ( currentMessage.msgString, "" );
		return 0;
	}
	else
	{
		if (ppLuaMessages && ppLuaMessages[0]->msgDesc)
		{
			strcpy (currentMessage.msgDesc, ppLuaMessages[0]->msgDesc);
			currentMessage.msgFloat = ppLuaMessages[0]->msgFloat;
			currentMessage.msgInt = ppLuaMessages[0]->msgInt;
			currentMessage.msgIndex = ppLuaMessages[0]->msgIndex;
			strcpy (currentMessage.msgString, ppLuaMessages[0]->msgString);

			delete ppLuaMessages[0];
			ppLuaMessages[0] = NULL;

			for (int c = 1; c < luaMessageCount; c++)
				ppLuaMessages[c - 1] = ppLuaMessages[c];

			ppLuaMessages[luaMessageCount - 1] = NULL;

			luaMessageCount--;
		}
		else
		{
			// something erased all/part LUA messages array
			return 0;
		}
	}
	return 1;
 }

 DARKLUA_API void SetLuaState ( int id )
 {
	 defaultState = id;
 }

 bool checkScriptAlreadyLoaded ( int id , LPSTR pString )
 {

	 for ( int c = 0 ; c < ScriptsLoaded.size() ; c++ )
	 {
		 if ( strcmp ( pString , ScriptsLoaded[c].fileName ) == 0 )
		 {
			 if ( id == ScriptsLoaded[c].stateID )
			 {
				return true;
			 }
		 }
	 }

	
	 StringList tempStringList;
	 strcpy ( tempStringList.fileName , pString );
	 tempStringList.stateID = id;

	 ScriptsLoaded.push_back(tempStringList);

	 return false;

 }

 DARKLUA_API int LoadLua( LPSTR pString , int id )
 {

	 if ( checkScriptAlreadyLoaded ( id , pString ) ) return 0;

	if ( id <= 0 )
	{
		//MessageBox(NULL, "invalid Lua ID, must be 1 or above", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( maxLuaStates == 0 )
	{
		maxLuaStates = 100;
		ppLuaStates = new luaState*[maxLuaStates+1];

		for ( int c = 0 ; c < maxLuaStates+1 ; c++ )
			ppLuaStates[c] = NULL;
	}

	if ( id > maxLuaStates )
	{
		luaState** ppBigger = NULL;
		ppBigger = new luaState*[maxLuaStates+101];

		for ( int c = 0; c < maxLuaStates+1; c++ )
		 ppBigger [ c ] = ppLuaStates [ c ];

		delete [ ] ppLuaStates;

		ppLuaStates = ppBigger;

		for ( int c = maxLuaStates+1; c < maxLuaStates+101; c++ )
			ppLuaStates[c] = NULL;

		maxLuaStates += 100;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua State ID already in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		//return 0;

		// create new Lua state   
		lua2 = luaL_newstate();

		// load Lua libraries
		static const luaL_Reg lualibs[] =
		{
			{ "base", luaopen_base },
			{ NULL, NULL}
		};

		addFunctions();

		/*const luaL_Reg *lib = lualibs;
		for(; lib->func != NULL; lib++)
		{
			lib->func(lua2);
			lua_settop(lua2, 0);
		}*/

		luaL_openlibs(lua2);
		GGLua_InstallCompatShim(lua2);   // GGMAX 3.40: before ANY script runs
	}

    // run the Lua script
	int result = 0;

	char VirtualFilename[MAX_PATH];
	strcpy ( VirtualFilename , pString );
	LuaCheckForWorkshopFile ( VirtualFilename );

	result =  luaL_dofile(lua2, VirtualFilename);

	if (result == 1 )
	{
		while(ShowCursor(TRUE) <= 0);
		SetCursorPos ( g_dwScreenWidth / 2 , g_dwScreenHeight / 2 );
		MessageBox( g_pGlob->hWnd , lua_tostring(lua2, -1), "LUA ERROR!" , MB_OK | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
		lua_pop(lua2, 1);
	}
	else
	{
		if ( ppLuaStates[id] == NULL )
			ppLuaStates[id] = new luaState;

		ppLuaStates[id]->state = lua2;
	}

	return result;
 }

 DARKLUA_API int LoadLua( LPSTR pString )
 {
	int id = defaultState;


	if ( checkScriptAlreadyLoaded ( id , pString ) ) return 0;

	if ( id <= 0 )
	{
		//MessageBox(NULL, "invalid Lua ID, must be 1 or above", "LUA ERROR", MB_TOPMOST | MB_OK);
		return 0;
	}

	if ( maxLuaStates == 0 )
	{
		maxLuaStates = 100;
		ppLuaStates = new luaState*[maxLuaStates+1];

		for ( int c = 0 ; c < maxLuaStates+1 ; c++ )
			ppLuaStates[c] = NULL;
	}

	if ( id > maxLuaStates )
	{
		luaState** ppBigger = NULL;
		ppBigger = new luaState*[maxLuaStates+101];

		for ( int c = 0; c < maxLuaStates+1; c++ )
		 ppBigger [ c ] = ppLuaStates [ c ];

		delete [ ] ppLuaStates;

		ppLuaStates = ppBigger;

		for ( int c = maxLuaStates+1; c < maxLuaStates+101; c++ )
			ppLuaStates[c] = NULL;

		maxLuaStates += 100;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua State ID already in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		//return 0;

		// create new Lua state   
		lua2 = luaL_newstate();

		// load Lua libraries
		static const luaL_Reg lualibs[] =
		{
			{ "base", luaopen_base },
			{ NULL, NULL}
		};

		addFunctions();

		/*const luaL_Reg *lib = lualibs;
		for(; lib->func != NULL; lib++)
		{
			lib->func(lua2);
			lua_settop(lua2, 0);
		}*/

		luaL_openlibs(lua2);
		GGLua_InstallCompatShim(lua2);   // GGMAX 3.40: before ANY script runs

	}

	/*lua_getglobal(lua2, "package");
	lua_getfield(lua2, -1, "path"); // get field "path" from table at top of stack (-1)
	std::string cur_path = lua_tostring(lua2, -1); // grab path string from top of stack
	cur_path.append (";"); // do your path magic here
	cur_path.append("F:/TGCSHARED/fpsc-reloaded/FPS Creator Files/Files/?");
	lua_pop(lua2, 1); // get rid of the string on the stack we just pushed on line 5
	lua_pushstring(lua2, cur_path.c_str()); // push the new one
	lua_setfield(lua2, -2, "path"); // set the field "path" in table at -2 with value at top of stack
	lua_pop(lua2, 1); // get rid of package table from top of stack*/

    // run the Lua script
	int result = 0;

	char VirtualFilename[MAX_PATH];
	strcpy ( VirtualFilename , pString );
	LuaCheckForWorkshopFile ( VirtualFilename );

	result = luaL_dofile(lua2, VirtualFilename);

	if (result == 1 )
	{
		while(ShowCursor(TRUE) <= 0);
		SetCursorPos ( g_dwScreenWidth / 2 , g_dwScreenHeight / 2 );
		MessageBox( g_pGlob->hWnd , lua_tostring(lua2, -1), "LUA ERROR!" , MB_OK | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
		//MessageBox(NULL, lua_tostring(lua2, -1), "LUA ERROR", MB_TOPMOST | MB_OK);
		lua_pop(lua2, 1);
	}
	else
	{
		if ( ppLuaStates[id] == NULL )
			ppLuaStates[id] = new luaState;

		ppLuaStates[id]->state = lua2;
	}

	return result;
 }

DARKLUA_API void LuaSetFunction( LPSTR pString , int id, int params, int results )
{

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	lua2 = ppLuaStates[id]->state;
	
	char* pLastOccurance =  strrchr ( pString , '\\' );
	if ( pLastOccurance )
		strcpy ( functionName, pLastOccurance+1 );
	else
		strcpy ( functionName , pString );
	functionParams = params;
	functionResults = results;
	functionStateID = id;

	// the function name 
	lua_getglobal(lua2, functionName );
}

DARKLUA_API void LuaSetFunction( LPSTR pString , int params, int results )
{
	int id = defaultState;

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaSetFunction" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "pString" , pString );
	WriteToDebugLog ( "params" , params );
	WriteToDebugLog ( "results" , results );
	WriteToDebugLog ( "===========" , true );
#endif

	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	lua2 = ppLuaStates[id]->state;

	char* pLastOccurance =  strrchr ( pString , '\\' );
	if ( pLastOccurance )
		strcpy ( functionName, pLastOccurance+1 );
	else
		strcpy ( functionName , pString );
	functionParams = params;
	functionResults = results;
	functionStateID = id;

	// the function name 
	lua_getglobal(lua2, functionName );
}

DARKLUA_API int LuaValidateEntityTable ( int iEntityIndex )
{
	int iValid = 0;
	int id = defaultState;
	if ( id > maxLuaStates+1 ) return 0;
	if ( ppLuaStates==NULL ) return 0;
	if ( ppLuaStates[id] == NULL ) return 0;
	lua2 = ppLuaStates[id]->state;
	int stacktopindex = lua_gettop (lua2);
	lua_getglobal(lua2, "g_Entity");
	lua_pushinteger(lua2, (lua_Integer)iEntityIndex); 
	lua_gettable(lua2, -2); // g_Entity[e] 
	lua_pushstring(lua2, "x");
	if ( lua_istable(lua2,-2) )
	{
		lua_gettable(lua2, -2);  // g_Entity[e]["x"]
		if ( lua_isnumber ( lua2, -1 ) || lua_isstring ( lua2, -1 ) )
		{
			// table exists and the element within the table also exists as a number/string so its valid
			iValid = 1;
		}
	}
	lua_settop (lua2, stacktopindex);
	return iValid;
}


// GGMAX 3.24: O(1) replacements for the linear FunctionsWithErrors scan (DarkLUA_part0.cpp)
bool gg_FunctionHasError ( LPSTR pName );
void gg_MarkFunctionError ( LPSTR pName, int stateID );
void gg_ClearFunctionErrors ( void );
DARKLUA_API void LuaCall()
{
	// GGMAX 3.24: was a linear strcmp scan over every recorded error, on every lua call.
	if ( gg_FunctionHasError ( functionName ) )
	{
		lua_pop(lua2,functionParams+1);
		return;
	}

	int id = functionStateID;

	int failedResults = 0;

	if ( id > maxLuaStates+1 )
	{

		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		lua_pop(lua2,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{

		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		lua_pop(lua2,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaCall" , true );
	WriteToDebugLog ( "ID" , functionStateID );
	WriteToDebugLog ( "functionParams" , functionParams );
	WriteToDebugLog ( "functionResults" , functionResults );
	WriteToDebugLog ( "===========" , true );
#endif
	GG_CRASH_CONTEXT("LuaCall", "functionName=%s functionParams=%d", functionName, functionParams);

	lua2 = ppLuaStates[id]->state;

	//if ( functionParams == 1 )
	//	int dave = 1;

	// call the function with x arguments, return y results
	if ( lua_isfunction(lua2, -(1+functionParams)) == 1 )
	{
		//lua_call(lua2, functionParams, functionResults);
      if (lua_pcall(lua2, functionParams, functionResults, 0) != 0)
	  {
		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		char s[256];
		sprintf ( s , "error running function: %s", lua_tostring(lua2, -1));
		//MessageBox(NULL, s, "LUA ERROR", MB_TOPMOST | MB_OK);
		while(ShowCursor(TRUE) <= 0);
		SetCursorPos ( g_dwScreenWidth / 2 , g_dwScreenHeight / 2 );
		MessageBox( g_pGlob->hWnd , lua_tostring(lua2, -1), "LUA ERROR" , MB_OK | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND );
		lua_pop(lua2, 1);
		failedResults = 1;
	  }
	}
	else
	{

		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		// remove params from the stack
		lua_pop(lua2,functionParams);
		failedResults = 1;

		char s[256];
		//sprintf ( s , "No function called %s" , functionName );
		//MessageBox(NULL, s, "LUA ERROR", MB_TOPMOST | MB_OK);
	}

	functionStateID = 0;
	if ( failedResults > 0 )
		lua_pop(lua2, failedResults);
}

DARKLUA_API void LuaCallSilent()
{
	// GGMAX 3.24: was a linear strcmp scan over every recorded error, on every lua call.
	if ( gg_FunctionHasError ( functionName ) )
	{
		lua_pop(lua2,functionParams+1);
		return;
	}

	int id = functionStateID;

	int failedResults = 0;

	if ( id > maxLuaStates+1 )
	{

		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		lua_pop(lua2,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

	if ( ppLuaStates==NULL ) return;
	if ( ppLuaStates[id] == NULL )
	{

		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		lua_pop(lua2,functionParams+1);

		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return ;
	}

#ifdef LUA_DO_DEBUG
	WriteToDebugLog ( "-->LuaCallSilent" , true );
	WriteToDebugLog ( "ID" , id );
	WriteToDebugLog ( "functionParams" , functionParams );
	WriteToDebugLog ( "functionResults" , functionResults );
	WriteToDebugLog ( "===========" , true );
#endif
	GG_CRASH_CONTEXT("LuaCall", "functionName=%s functionParams=%d", functionName, functionParams);

	lua2 = ppLuaStates[id]->state;

	// call the function with x arguments, return y results
	if ( lua_isfunction(lua2, -(1+functionParams)) == 1 )
	{
		//lua_call(lua2, functionParams, functionResults);
      if (lua_pcall(lua2, functionParams, functionResults, 0) != 0)
	  {
		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		char s[256];
		//sprintf ( s , "error running function: %s", lua_tostring(lua2, -1));
		//MessageBox(NULL, s, "LUA ERROR", MB_TOPMOST | MB_OK);
		failedResults = 1;
	  }
	}
	else
	{

		//add to error list
		gg_MarkFunctionError ( functionName, id );   // GGMAX 3.24: also indexes it

		// remove params from the stack
		lua_pop(lua2,functionParams);
		failedResults = 1;
	}

	functionStateID = 0;
	if ( failedResults > 0 )
		lua_pop(lua2, failedResults);

}

 DARKLUA_API void CloseLua( int id )
 {
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates[id] != NULL )
	{
		// close the Lua state
		lua_close(ppLuaStates[id]->state);
		delete ppLuaStates[id];
		ppLuaStates[id] = NULL;
	}
	//else
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);

 }

  DARKLUA_API void CloseLua()
 {
	int id = defaultState;
	
	if ( id > maxLuaStates+1 )
	{
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);
		return;
	}

	if ( ppLuaStates[id] != NULL )
	{
		// close the Lua state
		lua_close(ppLuaStates[id]->state);
		delete ppLuaStates[id];
		ppLuaStates[id] = NULL;
	}
	//else
		//MessageBox(NULL, "Lua ID not in use", "LUA ERROR", MB_TOPMOST | MB_OK);

 }

  DARKLUA_API void CloseLuaSilent( int id )
 {
	if ( id > maxLuaStates+1 )
	{
		return;
	}

	if ( ppLuaStates[id] != NULL )
	{
		// close the Lua state
		lua_close(ppLuaStates[id]->state);
		delete ppLuaStates[id];
		ppLuaStates[id] = NULL;
	}

 }

 // GGMAX diag (harness RUN_LUA): execute a lua chunk in the CURRENT game lua state on the
 // main thread; returns 0 on success and writes tostring() of any results to pResultBuf.
 DARKLUA_API int RunLuaString ( LPSTR pCode, LPSTR pResultBuf, int iResultSize )
 {
	if (pResultBuf && iResultSize > 0) pResultBuf[0] = 0;
	if (lua2 == NULL)
	{
		if (pResultBuf) _snprintf(pResultBuf, iResultSize - 1, "ERROR: no lua state");
		return -1;
	}
	int top = lua_gettop(lua2);
	int status = luaL_loadstring(lua2, pCode);
	if (status == 0) status = lua_pcall(lua2, 0, LUA_MULTRET, 0);
	if (status != 0)
	{
		const char* err = lua_tostring(lua2, -1);
		if (pResultBuf) _snprintf(pResultBuf, iResultSize - 1, "LUA ERROR: %s", err ? err : "(no message)");
		lua_settop(lua2, top);
		return -1;
	}
	int nres = lua_gettop(lua2) - top;
	int written = 0;
	for (int i = 1; i <= nres && pResultBuf; i++)
	{
		const char* s = lua_tostring(lua2, top + i); // NULL for tables/functions/nil
		written += _snprintf(pResultBuf + written, iResultSize - 1 - written, "%s%s", (i > 1) ? " | " : "", s ? s : luaL_typename(lua2, top + i));
		if (written >= iResultSize - 2) break;
	}
	lua_settop(lua2, top);
	return 0;
 }

