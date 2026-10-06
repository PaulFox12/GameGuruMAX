-- LUA Globals Common Header 
-- DESCRIPTION: A global script that controls game & player logic. Do not assign to an entity.

-- Constants
AI_MANUAL    = 0 -- Preferred Mode With Direct Script Control 
AI_AUTOMATIC = 1 -- Discontinued Legacy Mode

-- Globals (built-in)
g_Entity = {}
g_EntityExtra = {}
g_DebugStringPeek = ""

-- New AI Globals
ai_state_startidle, ai_state_idle, ai_state_findpatrolpath, ai_state_startpatrol, ai_state_patrol, ai_state_startmove, ai_state_move, ai_state_avoid, ai_state_hurt, ai_state_punch, ai_state_recoverstart, ai_state_recover, ai_state_startfireonspot, ai_state_fireonspot, ai_state_startreload, ai_state_reload, ai_state_reloadsettle, ai_state_disable, ai_state_duckstart, ai_state_duck, ai_state_unduckstart, ai_state_unduck, ai_state_rollstart, ai_state_roll, ai_state_crouchdashstart, ai_state_crouchdash, ai_state_checkforcover, ai_state_strafeleftstart, ai_state_strafeleft, ai_state_straferightstart, ai_state_straferight, ai_state_preexit = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
ai_state_debug = { "startidle", "idle", "findpatrolpath", "startpatrol", "patrol", "startmove", "move", "avoid", "hurt", "punch", "recoverstart", "recover", "startfireonspot", "fireonspot", "startreload", "reload", "reloadsettle", "disable", "duckstart", "duck", "unduckstart", "unduck",  "rollstart", "roll", "crouchdashstart", "crouchdash", "ai_state_checkforcover", "strafeleftstart", "strafeleft", "straferightstart", "straferight", "preexit" }
ai_combattype_regular, ai_combattype_patrol, ai_combattype_guard, ai_combattype_freezermelee, ai_combattype_bashmelee = 0, 1, 2, 3, 4
ai_movetype_usespeed, ai_movetype_useanim = 0, 1
ai_attacktype_nofire, ai_attacktype_canfire = 0, 1
ai_bot_state = {}
ai_bot_substate = {}
ai_bot_oldhealth = {}
ai_bot_targetx = {}
ai_bot_targety = {}
ai_bot_targetz = {}
ai_bot_closeenoughx = {}
ai_bot_closeenoughy = {}
ai_bot_closeenoughz = {}
ai_bot_pathindex = {}
ai_bot_pointindex = {}
ai_bot_pointdirection = {}
ai_bot_pointmax = {}
ai_bot_pointtime = {}
ai_bot_coverindex = {}
ai_bot_angryhurt = {}
ai_bot_patroltime = {}
ai_bot_hunttime = {}
ai_bot_gofast = {}
ai_bot_sighting = {}
ai_bot_roty = {}
ai_bot_last_sidestep = {}
ai_bot_last_fired = {}
ai_cover_slot = {}

-- Weapon name global
weapon_name = {}

-- Globals (passed-in)
g_ShowObjectDebugVisuals = 0
g_GameStateChange = 0
g_PlayerPosX = 0
g_PlayerPosY = 0
g_PlayerPosZ = 0
g_PlayerAngX = 0
g_PlayerAngY = 0
g_PlayerAngZ = 0
g_PlayerObjNo = 0
g_PlayerHealth = 0 -- handled in gameplayerhealth
g_UserGlobal = {}
g_UserGlobalContainer = ""
g_UserGlobalContainerLast = ""
g_UserGlobalQuestTitleActive = ""
g_UserGlobalQuestTitleActiveObject = ""
g_UserGlobalQuestTitleActiveE = 0
g_UserGlobalQuestTitleShowing = ""
g_UserGlobalQuestTitleShowingObject = ""
g_UserGlobalQuestTitleShowingObject2 = ""
g_UserContainerTotal = 0
g_UserContainerName = {}
g_UserContainerCount = {}
g_UserContainerIndex = {}
g_UserContainerSlot = {}
g_UserContainerQty = {}
g_UserContainerE = {}
g_PlayerLives = 0
g_PlayerFlashlight = 0
g_PlayerGunCount = 0
g_PlayerGunID = 0
g_PlayerGunName = ""
g_PlayerGunMode = 0
g_PlayerGunFired = 0
g_PlayerGunAmmoCount = 0
g_PlayerGunClipCount = 0
g_PlayerGunZoomed = 0
g_PlayerThirdPerson = 0
g_PlayerController = 0
g_PlayerFOV = 34
g_PlayerLastHitTime = 0
g_PlayerDeadTime = 0
g_Time = 0
g_TimeElapsed = 0
g_KeyPressE = 0
g_KeyPressQ = 0
g_KeyPressW = 0
g_KeyPressA = 0
g_KeyPressS = 0
g_KeyPressD = 0
g_KeyPressR = 0
g_KeyPressF = 0
g_KeyPressC = 0
g_KeyPressJ = 0
g_MouseClickControl = 0
g_KeyPressSPACE = 0
g_KeyPressSHIFT = 0
g_Scancode = 0
g_InKey = ""
g_LevelFilename = ""
g_LevelTerrainSize = 0
g_MouseX = 0
g_MouseY = 0
g_MouseWheel = 0
g_MouseClick = 0
g_EntityElementMax = 0
g_PlayerUnderwaterMode = 0
g_PlayerCastTime = {}
g_PlayerCastLast = {}
g_PlayerCastDoneOneForThisCycle = 0
g_PlayerTargetSlot1 = 0
g_PlayerTargetSlot2 = 0
g_PlayerTargetSlot3 = 0
g_PlayerTargetSlot4 = 0

-- Mappable in-game keys
g_PlrKeyW = 0
g_PlrKeyA = 0
g_PlrKeyS = 0
g_PlrKeyD = 0
g_PlrKeyQ = 0
g_PlrKeyE = 0
g_PlrKeyF = 0
g_PlrKeyC = 0
g_PlrKeyZ = 0
g_PlrKeyR = 0
g_PlrKeySPACE = 0
g_PlrKeyRETURN = 0
g_PlrKeySHIFT = 0
g_PlrKeySHIFT2 = 0
g_PlrKeyJ = 0

-- Weapon stats
g_WeaponSlotGot = {}
g_WeaponSlotPref = {}
g_WeaponAmmo = {}
g_WeaponClipAmmo = {}
g_WeaponPoolAmmo = {}

-- Radar globals
g_objectiveCount = 0
radarObjectives = {}

-- Multiplayer
mp_isServer = 0
mp_playerNames = {}
mp_playerKills = {}
mp_playerDeaths = {}
mp_playerConnected = {}
mp_playerTeam = {}
mp_isConnectedToSteam = 0
mp_gameMode = 0
mp_servertimer = 0
mp_showscores = 0
mp_teambased = 0
mp_friendlyfireoff = 0
mp_me = 0;
mp_coop = 0;
mp_enemiesLeftToKill = 100;

-- GameLoop Globals, Init and Loop
g_gameloop_StartHealth = 0
g_gameloop_RegenRate = 0
g_gameloop_RegenSpeed = 0
g_gameloop_RegenDelay = 0
g_gameloop_AnotherInit = 0

-- Globals to track projectile explosion event
g_projectileevent_explosion = 0
g_projectileevent_name = ""
g_projectileevent_x = 0
g_projectileevent_y = 0
g_projectileevent_z = 0
g_projectileevent_radius = 0
g_projectileevent_damage = 0
g_projectileevent_entityhit = 0

function GameLoopInit(invincible,sth,rra,rsp,rde)
 g_gameloop_StartHealth = sth
 g_gameloop_RegenRate = rra
 g_gameloop_RegenSpeed = rsp
 g_gameloop_RegenDelay = rde
 gameloop = require "scriptbank\\gameloop"
 gameloop.init()
 hud0 = require "scriptbank\\hud0"
 hud0.init()
 gameplayerhealth = require "scriptbank\\gameplayerhealth"
 gameplayerhealth.startgame(invincible,sth)
 gameplayerspeed = require "scriptbank\\gameplayerspeed"
 gameplayerspeed.startgame()
 g_PlayerFrozen = 0
 g_gameloop_AnotherInit = 1
end

function GlobalLoop(gameloopflag)
 -- monitor game state changes
 if gameloopflag == 1 then
  if g_GameStateChange > 0 then
   gamedata = require "titlesbank\\gamedata"
   if gamedata.load(g_GameStateChange) == 1 then
    if strLevelFilename ~= g_LevelFilename then
	 -- need to load correct level for save position first
	 JumpToLevel(strLevelFilename)
	else
	 RestoreGameFromSlot(0)
	 g_GameStateChange = 0
	 TriggerFadeIn()
     restoregame = require "titlesbank\\restoregame"
     restoregame.now()
	end
   end
  end
 end
 -- one cycle only flags
 g_PlayerCastDoneOneForThisCycle = 0
 -- call per-game loop script (player health control)
 gameloop = require "scriptbank\\gameloop"
 gameloop.main()
end

function GlobalLoopFinish(gameloopflag)
 -- display in-game huds
 hud0 = require "scriptbank\\hud0"
 hud0.main()
end

function GameLoopQuit()
 gameloop = require "scriptbank\\gameloop"
 gameloop.quit()
 hud0 = require "scriptbank\\hud0"
 hud0.quit()
end

function file_exists(name)
   local f = io.open(name, "r")
   return f ~= nil and io.close(f)
end

function GameLoopSaveStats(storyboardnodeid)
 -- only when dealing with real levels
 if storyboardnodeid >=0 then
	-- reuse save system to preserve state of the level just before we leave
	gamedata = require "titlesbank\\gamedata"
	gamedata.mode(1)
	gamedata.save("0-"..storyboardnodeid,"levelnodeid-"..storyboardnodeid)
	-- and also save state of all player attributes (health, weapon, containers, userglobals)
	gamedata.mode(2)
	gamedata.save("0-globals","playerstate")
	gamedata.mode(0)
 end
end

function GameLoopLoadStats(storyboardnodeid,restorepreviouslevelstate)
 -- only when dealing with real levels
 if storyboardnodeid >=0 then
	-- reload state of the level from last time we left it
	gamedata = require "titlesbank\\gamedata"
	gamedata.mode(0)
	local restoretochanges = 0
	if restorepreviouslevelstate == 1 then
		if gamedata.load("0-"..storyboardnodeid) == 1 then restoretochanges = 1 end
	end
	if gamedata.load("0-globals") == 1 then restoretochanges = 1 end
	-- use above data to restore level to that state
	if restoretochanges == 1 then
		restoregame = require "titlesbank\\restoregame"
		restoregame.mode(1)
		restoregame.now()
		restoregame.mode(0)
	end
 end
end

function PlayerControl()
 gameplayercontrol = require "scriptbank\\gameplayercontrol"
 gameplayercontrol.main()
 gameplayerhealth = require "scriptbank\\gameplayerhealth"
 gameplayerhealth.main()
 gameplayerspeed = require "scriptbank\\gameplayerspeed"
 gameplayerspeed.main()
end
function PlayerHealthSet(health)
 gameplayerhealth = require "scriptbank\\gameplayerhealth"
 gameplayerhealth.set(health)
end
function PlayerHealthAdd(value)
 gameplayerhealth = require "scriptbank\\gameplayerhealth"
 gameplayerhealth.add(value)
end
function PlayerHealthSubtract(value)
 gameplayerhealth = require "scriptbank\\gameplayerhealth"
 gameplayerhealth.subtract(value)
end
function PlayerStaminaDrain(value)
 gameplayerspeed = require "scriptbank\\gameplayerspeed"
 gameplayerspeed.drain(value)
end

-- Common Updater Functions (called by Engine)

function UpdateEntitySMALL(e,object,x,y,z)
 g_Entity[e] = {x=x; y=y; z=z; obj=object;}
end

function UpdateEntity(e,object,x,y,z,rx,ry,rz,ave,act,col,key,zon,ezon,plrvis,ani,hea,frm,pdst,avd,lmb,lmi)
 g_Entity[e] = {x=x; y=y; z=z; anglex=rx; angley=ry; anglez=rz; active=ave; activated=act; animating=ani; collected=col; haskey=key; plrinzone=zon; entityinzone=ezon; plrvisible=plrvis; obj=object; health=hea; strength=hea; frame=frm; timer=0; plrdist=pdst; avoid=avd; limbhit=lmb; limbhitindex=lmi; debuggermode=0;}
 g_EntityExtra[e] = {visible=-1; beingreset=0; collision=1; clonedsincelevelstart=0;}
end

function UpdateEntityRT(e,object,x,y,z,rx,ry,rz,ave,act,col,key,zon,ezon,plrvis,hea,frm,pdst,avd,lmb,lmi)
 if g_Entity[e] ~= nil then
  g_Entity[e]['x'] = x;
  g_Entity[e]['y'] = y;
  g_Entity[e]['z'] = z;
  g_Entity[e]['anglex'] = rx;
  g_Entity[e]['angley'] = ry;
  g_Entity[e]['anglez'] = rz;
  g_Entity[e]['obj'] = object;
  g_Entity[e]['active'] = ave;
  g_Entity[e]['activated'] = act;
  g_Entity[e]['collected'] = col;
  g_Entity[e]['haskey'] = key;
  g_Entity[e]['plrinzone'] = zon;
  g_Entity[e]['entityinzone'] = ezon;
  g_Entity[e]['plrvisible'] = plrvis;
  g_Entity[e]['health'] = hea;
  g_Entity[e]['frame'] = frm;
  g_Entity[e]['plrdist'] = pdst;
  g_Entity[e]['avoid'] = avd;
  g_Entity[e]['limbhit'] = lmb;
  g_Entity[e]['limbhitindex'] = lmi;
 end
end

function UpdateEntityDebugger(e,mode)
 if g_Entity ~= nil then 
  if e >= 1 then
   if g_Entity[e] ~= nil then
    if g_Entity[e]['debuggermode'] ~= nil then
     g_Entity[e]['debuggermode'] = mode
	end
   end
  end
 end
end

function UpdateEntityAnimatingFlag(e,ani)
 if g_Entity[e] ~= nil then
  g_Entity[e]['animating'] = ani
 end
end

function UpdateWeaponStatsItem(mode,index,value)
 if mode==1 then g_WeaponSlotGot[index] = value end
 if mode==2 then g_WeaponSlotPref[index] = value end
 if mode==3 then g_WeaponAmmo[index] = value end
 if mode==4 then g_WeaponClipAmmo[index] = value end
 if mode==5 then g_WeaponPoolAmmo[index] = value end
end

-- Macro Functions
local sqrt = math.sqrt
local abs  = math.abs

function GetPlayerDistance( e )
    local Ent = g_Entity[ e ]
	if Ent ~= nil then
		local PDX, PDY, PDZ = Ent.x - g_PlayerPosX, 
							  Ent.y - g_PlayerPosY,
							  Ent.z - g_PlayerPosZ;
	  
		--if abs( PDY ) > 100 then PDY = PDY * 4 end messes up when player on hill, range not intuitive!
		if abs( PDY ) > 100 then PDY = PDY * 1.25 end
		return sqrt( PDX*PDX + PDY*PDY + PDZ*PDZ )
	else
		return 999999
	end
end

function GetDistanceTo( e, tx, ty, tz )
    local Ent = g_Entity[ e ]
	if Ent ~= nil then
		local PDX, PDY, PDZ = Ent.x - tx, 
							  Ent.y - ty,
							  Ent.z - tz;
	  
		--if abs( PDY ) > 100 then PDY = PDY * 4 end messes up when player on hill, range not intuitive!
		if abs( PDY ) > 100 then PDY = PDY * 1.25 end
		return sqrt( PDX*PDX + PDY*PDY + PDZ*PDZ )
	else
		return 999999
	end
end

function GetPlrLookingAtExThreshold( e, uselineofsight, detectthreshold )
 if e == nil then return end
 if g_Entity == nil then return end
 if g_Entity[e] == nil then return end
 local LookingAt = 0
 if GetHeadTracker() == 1 then
  local pObj = MotionControllerLaserGuidedEntityObj()
  if pObj == g_Entity[e]['obj'] then LookingAt = 1 end
 else
  -- PE: Make sure the angle is correct calculated.
  local FOV_THRESHOLD = 45.0
  local SourceAngle = g_PlayerAngY
  SourceAngle = SourceAngle % 360.0
  if SourceAngle < 0.0 then
      SourceAngle = SourceAngle + 360.0
  end
  local PlayerDX = (g_Entity[e]['x'] - g_PlayerPosX)
  local PlayerDZ = (g_Entity[e]['z'] - g_PlayerPosZ)
  local DestAngle = math.atan2(PlayerDZ, PlayerDX)
  DestAngle = (DestAngle * 57.2957795) - 90.0
  DestAngle = -DestAngle
  DestAngle = DestAngle % 360.0
  if DestAngle < 0.0 then
      DestAngle = DestAngle + 360.0
  end
  local difference = DestAngle - SourceAngle
  if difference > 180.0 then
      difference = difference - 360.0
  elseif difference < -180.0 then
      difference = difference + 360.0
  end
  local Result = math.abs(difference)

  if Result < FOV_THRESHOLD then
    -- additional line of sight test
	if uselineofsight == 1 and g_PlayerCastDoneOneForThisCycle == 0 then
	 if g_PlayerCastTime[e] == nil then g_PlayerCastTime[e] = Timer() end
	 if Timer() > g_PlayerCastTime[e] then
	  g_PlayerCastDoneOneForThisCycle = 1
	  g_PlayerCastTime[e] = Timer() + 250
	  --local minx,miny,minz,maxx,maxy,maxz = GetEntityColBox(e) - did not account for object rotation!
	  --local destx = g_Entity[e]['x'] + (minx+((maxx-minx)/2))
	  --local desty = g_Entity[e]['y'] + (miny+((maxy-miny)/2))
	  --local destz = g_Entity[e]['z'] + (minz+((maxz-minz)/2))
	  local cx,cy,cz = GetObjectColCentre(g_Entity[e]['obj'])
	  local destx = g_Entity[e]['x'] + cx
	  local desty = g_Entity[e]['y'] + cy
	  local destz = g_Entity[e]['z'] + cz
      local dx = GetCameraPositionX(0) - destx
      local dy = GetCameraPositionY(0) - desty
      local dz = GetCameraPositionZ(0) - destz
	  local dist = math.sqrt(math.abs(dx*dx)+math.abs(dy*dy)+math.abs(dz*dz))
	  dx = dx / dist
	  dy = dy / dist
	  dz = dz / dist
	  destx = destx + (dx*detectthreshold)
	  desty = desty + (dy*detectthreshold)
	  destz = destz + (dz*detectthreshold)
      LookingAt = 0
	  if dist < 120 then
	   local tresult = IntersectAll(GetCameraPositionX(0),GetCameraPositionY(0),GetCameraPositionZ(0),destx,desty,destz,0)
	   if tresult == 0 or tresult == g_Entity[e]['obj'] then
	    LookingAt = 1
	   end
	  end
	  g_PlayerCastLast[e] = LookingAt
	 else
	  if g_PlayerCastLast[e] == nil then g_PlayerCastLast[e] = 0 end
	  if g_PlayerCastLast[e] ~= nil then
	   LookingAt = g_PlayerCastLast[e]
	  end
	 end
	else
	 LookingAt = 1
	end
  end
 end
 return LookingAt
end
function GetPlrLookingAtEx( e, uselineofsight )
	return GetPlrLookingAtExThreshold( e, uselineofsight, 2 )
end
function GetPlrLookingAt( e )
 return GetPlrLookingAtEx(e,0) 
end

function SetAnimFromAnimSet(e,iAnimSlot)
 fStartFrame = GetEntityAnimStart(GetEntityElementBankIndex(e),iAnimSlot)
 fFinishFrame = GetEntityAnimFinish(GetEntityElementBankIndex(e),iAnimSlot)
 SetAnimationFrames(fStartFrame,fFinishFrame)
end

-- Common Action Functions (called by LUA)
function Prompt(str)
 SendMessageS_prompt(str)
end
function PromptImage(i)
 SendMessageI_promptimage(i)
end
function PromptDuration(str,v)
 SendMessageS_promptduration(v,str)
end
function PromptTextSize(v)
 SendMessageI_prompttextsize(v)
end
function PromptLocal(e,str)
 SendMessageS_promptlocal(e,str)
end
function PromptLocalForVR(e,str,vrmode)
 SendMessageF_promptlocalforvrmode(vrmode)
 SendMessageS_promptlocalforvr(e,str)
end
function SetFogNearest(v)
 SendMessageF_setfognearest(v)
end
function SetFogDistance(v)
 SendMessageF_setfogdistance(v)
end
function SetFogRed(v)
 SendMessageF_setfogred(v)
end
function SetFogGreen(v)
 SendMessageF_setfoggreen(v)
end
function SetFogBlue(v)
 SendMessageF_setfogblue(v)
end
function SetFogIntensity(v)
 SendMessageF_setfogintensity(v)
end
function SetAmbienceIntensity(v)
 SendMessageF_setambienceintensity(v)
end
function SetAmbienceRed(v)
 SendMessageF_setambiencered(v)
end
function SetAmbienceGreen(v)
 SendMessageF_setambiencegreen(v)
end
function SetAmbienceBlue(v)
 SendMessageF_setambienceblue(v)
end
function SetSurfaceIntensity(v)
 SendMessageF_setsurfaceintensity(v/33.0)
end
function SetSurfaceRed(v)
 SendMessageF_setsurfacered(v)
end
function SetSurfaceGreen(v)
 SendMessageF_setsurfacegreen(v)
end
function SetSurfaceBlue(v)
 SendMessageF_setsurfaceblue(v)
end
function SetSurfaceSunFactor(v)
 SendMessageF_setsurfacesunfactor(v)
end
function SetGlobalSpecular(v)
 SendMessageF_setglobalspecular(v)
end
function SetBrightness(v)
 SendMessageF_setbrightness(v)
end
function SetContrast(v)
 SendMessageF_setcontrast(v)
end
function SetPostBloom(v)
 SendMessageF_setpostbloom(v)
end
function SetPostVignetteRadius(v)
 SendMessageF_setpostvignetteradius(v)
end
function SetPostVignetteIntensity(v)
 SendMessageF_setpostvignetteintensity(v)
end
function SetPostMotionDistance(v)
 SendMessageF_setpostmotiondistance(v)
end
function SetPostMotionIntensity(v)
 SendMessageF_setpostmotionintensity(v)
end
function SetPostDepthOfFieldDistance(v)
 SendMessageF_setpostdepthoffielddistance(v)
end
function SetPostDepthOfFieldIntensity(v)
 SendMessageF_setpostdepthoffieldintensity(v)
end
function SetPostLightRayLength(v)
 SendMessageF_setpostlightraylength(v)
end
function SetPostLightRayQuality(v)
 SendMessageF_setpostlightrayquality(v)
end
function SetPostLightRayDecay(v)
 SendMessageF_setpostlightraydecay(v)
end
function SetPostSAORadius(v)
 SendMessageF_setpostsaoradius(v)
end
function SetPostSAOIntensity(v)
 SendMessageF_setpostsaointensity(v)
end
function SetPostLensFlareIntensity(v)
 SendMessageF_setpostlensflareintensity(v)
end
function SetOptionReflection(v)
 SendMessageF_setoptionreflection(v)
end
function SetOptionShadows(v)
 SendMessageF_setoptionshadows(v)
end
function SetOptionLightRays(v)
 SendMessageF_setoptionlightrays(v)
end
function SetOptionVegetation(v)
 SendMessageF_setoptionvegetation(v)
end
function SetOptionOcclusion(v)
 SendMessageF_setoptionocclusion(v)
end
function SetCameraPanelDistance(v)
 SendMessageF_setcameradistance(v)
end
function SetCameraPanelFOV(v)
 SendMessageF_setcamerafov(v)
end
function SetCameraPanelZoomPercentage(v)
 SendMessageF_setcamerazoompercentage(v)
end
function SetCameraPanelWeaponFOV(v)
 SendMessageF_setcameraweaponfov(v)
end
function SetTerrainLODNear(v)
 SendMessageF_setterrainlodnear(v)
end
function SetTerrainLODMid(v)
 SendMessageF_setterrainlodmid(v)
end
function SetTerrainLODFar(v)
 SendMessageF_setterrainlodfar(v)
end
function SetTerrainSize(v)
 SendMessageF_setterrainsize(v)
end
function SetVegetationQuantity(v)
 SendMessageF_setvegetationquantity(v)
end
function SetVegetationWidth(v)
 SendMessageF_setvegetationwidth(v)
end
function SetVegetationHeight(v)
 SendMessageF_setvegetationheight(v)
end
function JumpToLevelIfUsed(e)
 SendMessageS_jumptolevel(e,"")
end
function JumpToLevelIfUsedEx(e,resetstates)
 SendMessageS_jumptolevel(e,tostring(resetstates))
end
function JumpToLevel(levelname)
 SendMessageS_jumptolevel(0,levelname)
end
function FinishLevel()
 SendMessageI_finishlevel(0)
end
function HideTerrain()
 SendMessageI_hideterrain(0)
end
function ShowTerrain()
 SendMessageI_showterrain(0)
end
function HideWater()
 SendMessageI_hidewater(0)
end
function ShowWater()
 SendMessageI_showwater(0)
end
function HideHuds()
 SendMessageI_hidehuds(0)
end
function ShowHuds()
 SendMessageI_showhuds(0)
end
function FreezeAI()
 SendMessageI_freezeai(0)
end
function UnFreezeAI()
 SendMessageI_unfreezeai(0)
end
function FreezePlayer()
 SendMessageI_freezeplayer(0)
end
function UnFreezePlayer()
 SendMessageI_unfreezeplayer(0)
end
function FreezePlayerPosition()
 SendMessageI_freezeplayer(1)
end
function UnFreezePlayerPosition()
 SendMessageI_unfreezeplayer(1)
end
function SetFreezePosition(x,y,z)
 SendMessageF_setfreezepositionx(x)
 SendMessageF_setfreezepositiony(y)
 SendMessageF_setfreezepositionz(z)
end
function SetFreezeAngle(ax,ay,az)
 SendMessageF_setfreezepositionax(ax)
 SendMessageF_setfreezepositionay(ay)
 SendMessageF_setfreezepositionaz(az)
end
function SetPriorityToTransporter(e,priority)
 SendMessageI_setprioritytotransporter(e,priority)
end
function TransportToFreezePosition()
 SendMessageI_transporttofreezeposition(0)
 if g_gravityhold ~= nil then g_gravityhold = 5 end
end
function TransportToFreezePositionOnly()
 SendMessageI_transporttofreezeposition(1)
 if g_gravityhold ~= nil then g_gravityhold = 5 end
end
function ActivateMouse()
 SendMessageI_activatemouse(0)
end
function DeactivateMouse()
 SendMessageI_deactivatemouse(0)
end
function SetPlayerHealth(v)
 gameplayerhealth.set(v)
end
function SetPlayerHealthCore(v)
 SendMessageI_setplayerhealthcore(v)
end
function SetPlayerLives(v)
 SendMessageI_setplayerlives(v)
end
function SetEntityHealth(e,v)
 SendMessageI_setentityhealth(e,v)
end
function SetEntityHealthSilent(e,v)
 SendMessageI_setentityhealthsilent(e,v)
end
function SetEntityHealthWithDamage(e,v)
 SendMessageI_setentityhealthwithdamage(e,v)
end
function SetEntityRagdollForce(e,limb,x,y,z,v)
 SendMessageI_setforcex(x)
 SendMessageI_setforcey(y)
 SendMessageI_setforcez(z);
 SendMessageI_setforcelimb(e,limb)
 SendMessageI_ragdollforce(e,v)
end
function StartTimer(e)
 g_Entity[e]['timer'] = g_Time
end
function GetTimer(e)
 return g_Time - g_Entity[e]['timer'];
end
function Destroy(e)
 SendMessageI_destroy(e)
end
function CollisionOn(e)
 SendMessageI_collisionon(e)
 if g_EntityExtra[e] == nil then g_EntityExtra[e] = {} end
 g_EntityExtra[e]['collision'] = 1
end
function CollisionOff(e)
 SendMessageI_collisionoff(e)
 if g_EntityExtra[e] == nil then g_EntityExtra[e] = {} end
 g_EntityExtra[e]['collision'] = 0
end
function GetEntityPlayerVisibility(e)
 SendMessageI_getentityplrvisible(e)
end
function GetEntityInZone(e)
 SendMessageI_getentityinzone(e)
end
function Hide(e)
 SendMessageI_hide(e)
 if g_EntityExtra[e] == nil then g_EntityExtra[e] = {} end
 g_EntityExtra[e]['visible'] = 0
end
function Show(e)
 SendMessageI_show(e)
 if g_EntityExtra[e] == nil then g_EntityExtra[e] = {} end
 g_EntityExtra[e]['visible'] = 1
end
function HideLimb(e,limb)
 SendMessageI_hidelimbs(e,limb)
end
function ShowLimb(e,limb)
 SendMessageI_showlimbs(e,limb)
end
function HideLimbsExcept(e,limb)
 SendMessageI_hidelimbs(e,limb*-1)
end
function ShowLimbsExcept(e,limb)
 SendMessageI_showlimbs(e,limb*-1)
end
function Spawn(e)
 SendMessageI_spawn(e)
end
function SetActivated(e,v)
 g_Entity[e]['activated'] = v
 SendMessageI_setactivated(e,v)
end
function SetActivatedWithMP(e,v)
 g_Entity[e]['activated'] = v
 SendMessageI_setactivatedformp(e,v)
end
function ResetLimbHit(e)
 g_Entity[e]['limbhit'] = -1
 SendMessageI_resetlimbhit(e,-1)
end
function ActivateIfUsed(e)
 SendMessageI_activateifused(e)
end
function PerformLogicConnections(e)
 SendMessageI_performlogicconnections(e)
end
function PerformLogicConnectionNumber(e,v)
 SendMessageI_performlogicconnectionnumber(e,v)
end
function PerformLogicConnectionsAsKey(e)
 SendMessageI_performlogicconnectionsaskey(e)
end
function SpawnIfUsed(e)
 SendMessageI_spawnifused(e)
end
function TransportToIfUsed(e)
 SendMessageI_transporttoifused(e)
end
function RefreshEntity(e)
 SendMessageI_refreshentity(e)
end
function RefreshEntityFromParent(e,parente)
 SendMessageI_refreshentity(e,parente)
end
function Collected(e)
 SendMessageI_collected(e,1)
end
function MoveUp(e,v)
 SendMessageF_moveup(e,v)
end
function MoveDown( e, v )
 MoveUp( e, -v )
end
function MoveForward(e,v)
 SendMessageF_moveforward(e,v)
end
function MoveBackward(e,v)
 SendMessageF_movebackward(e,v)
end
function SetHoverFactor(e,v)
 SendMessageF_sethoverfactor(e,v)
end
function SetPosition(e,x,y,z)
 --interferes with game reload/level restore and scripts that use SetPosition
 --if g_EntityExtra[e] == nil then g_EntityExtra[e] = {} end
 --if g_EntityExtra[e]['beingreset'] > 0 then 
 --  g_EntityExtra[e]['beingreset']=g_EntityExtra[e]['beingreset']-1
 --else
  SendMessageF_setpositionx(e,x)
  SendMessageF_setpositiony(e,y)
  SendMessageF_setpositionz(e,z)
 --end
end
function ResetPosition(e,x,y,z)
 --if g_EntityExtra[e] == nil then g_EntityExtra[e] = {} end
 --if g_EntityExtra[e] ~= nil then g_EntityExtra[e]['beingreset'] = 2 end
 SendMessageF_resetpositionx(e,x)
 SendMessageF_resetpositiony(e,y)
 SendMessageF_resetpositionz(e,z)
end
function SetRotation(e,x,y,z)
 SendMessageF_setrotationx(e,x)
 SendMessageF_setrotationy(e,y)
 SendMessageF_setrotationz(e,z)
end
function ResetRotation(e,x,y,z)
 SendMessageF_resetrotationx(e,x)
 SendMessageF_resetrotationy(e,y)
 SendMessageF_resetrotationz(e,z)
end
function ModulateSpeed(e,v)
 SendMessageF_modulatespeed(e,v)
end
function RotateX(e,v)
 SendMessageF_rotatex(e,v)
end
function RotateY(e,v)
 SendMessageF_rotatey(e,v)
end
function RotateZ(e,v)
 SendMessageF_rotatez(e,v)
end
function SetLimbIndex(e,v)
 SendMessageI_setlimbindex(e,v)
end
function RotateLimbX(e,v)
 SendMessageF_rotatelimbx(e,v)
end
function RotateLimbY(e,v)
 SendMessageF_rotatelimby(e,v)
end
function RotateLimbZ(e,v)
 SendMessageF_rotatelimbz(e,v)
end
function Scale(e,v)
 SendMessageF_scale(e,v)
end
function SetAnimation(e)
 SendMessageI_setanimation(e)
end
function SetAnimationName(e,str)
 SendMessageS_setanimationname(e,str)
end
function SetAnimationFrames(e,v)
 SendMessageI_setanimationframes(e,v)
end
function PlayAnimation(e)
 SendMessageI_playanimation(e)
end
function PlayAnimationFrom(e,v)
 SendMessageI_playanimationfrom(e,v)
end
function LoopAnimation(e)
 SendMessageI_loopanimation(e)
end
function LoopAnimationFrom(e,v)
 SendMessageI_loopanimationfrom(e,v)
end
function StopAnimation(e)
 SendMessageI_stopanimation(e)
end
function MoveWithAnimation(e,v)
 SendMessageI_movewithanimation(e,v)
end
function SetAnimationSpeed(e,v)
 SendMessageF_setanimationspeed(e,v)
end
function SetAnimationFrame(e,v)
 SendMessageF_setanimationframe(e,v)
end
function ChangeAnimationFrame(e,v)
 SendMessageF_changeanimationframe(e,v)
end
function GetAnimationFrame(e)
 return g_Entity[e]['frame']
end
function GravityOff(e)
 SendMessageI_setnogravity(e,1)
end
function GravityOn(e)
 SendMessageI_setnogravity(e,0)
end
function LookAtPlayer(e)
 SendMessageF_lookatplayer(e,100)
end
function LookAtPlayer(e,v)
 SendMessageF_lookattargete(e,0)
 SendMessageF_lookattargetyoffset(e,0)
 SendMessageF_lookatplayer(e,v)
end
function LookAtTarget(e,v,othere)
 SendMessageF_lookattargete(e,othere)
 SendMessageF_lookattargetyoffset(e,0)
 SendMessageF_lookattarget(e,v)
end
function LookAtTargetWithOffset(e,v,othere,yoffset)
 SendMessageF_lookattargete(e,othere)
 SendMessageF_lookattargetyoffset(e,yoffset)
 SendMessageF_lookattarget(e,v)
end
function AimAtPlayer(e,v)
 SendMessageF_lookattargete(e,0)
 SendMessageF_lookattargetyoffset(e,0)
 SendMessageF_aimsmoothmode(e,v)
end
function AimAtTarget(e,v,othere)
 SendMessageF_lookattargete(e,othere)
 SendMessageF_lookattargetyoffset(e,0)
 SendMessageF_aimsmoothmode(e,v)
end
function LookForward(e,v)
 SendMessageF_lookforward(e,v)
end
function LookAtAngle(e,v)
 SendMessageF_lookatangle(e,v)
end
function RotateToPlayerWithOffset(e,angleoffset)
 SendMessageF_rotatetoplayerwithoffset(e,angleoffset)
end
function RotateToPlayer(e)
 SendMessageI_rotatetoplayer(e,100)
end
function RotateToCamera(e)
 SendMessageI_rotatetocamera(e,100)
end
function RotateToPlayerSlowly(e,v)
 SendMessageI_rotatetoplayer(e,v)
end
function AddPlayerWeapon(e)
 SendMessageI_addplayerweapon(e,0);
end
function AddPlayerWeaponSuggestSlot(e,suggestedslot)
 SendMessageI_addplayerweapon(e,suggestedslot)
end
function ChangePlayerWeapon(str)
 SendMessageS_changeplayerweapon(str)
end
function ChangePlayerWeaponID(id)
 SendMessageI_changeplayerweaponid(id)
end
function ReplacePlayerWeapon(e)
 SendMessageI_replaceplayerweapon(e)
end
function RemovePlayerWeapon(slot)
 SendMessageI_removeplayerweapon(slot)
end
function RemovePlayerWeapons(e)
 SendMessageI_removeplayerweapons(e)
end
function AddPlayerAmmo(e)
 SendMessageI_addplayerammo(e)
end
function AddPlayerHealth(e)
 SendMessageI_addplayerhealth(e)
end
function SetPlayerPower(e,v)
 SendMessageI_setplayerpower(e,v)
end
function AddPlayerPower(e,v)
 SendMessageI_addplayerpower(e,v)
end
function AddPlayerJetPack(e,fuel)
 SendMessageI_addplayerjetpack(e,fuel)
end
function Checkpoint(e)
 SendMessageI_checkpoint(e)
end
function GetPlayerInZone(e)
 return g_Entity[e]['plrinzone'];
end
function SetSound(e,v)
 SendMessageI_setsound(e,v)
end
function PlaySound(e,v)
 SendMessageI_playsound(e,v)
end
function PlaySoundIfSilent(e,v)
 SendMessageI_playsoundifsilent(e,v)
end
function PlayNon3DSound(e,v)
 SendMessageI_playnon3dsound(e,v)
end
function LoopNon3DSound(e,v)
 SendMessageI_loopnon3dsound(e,v)
end
function LoopSound(e,v)
 SendMessageI_loopsound(e,v)
end
function StopSound(e,v)
 SendMessageI_stopsound(e,v)
end
function SetSoundSpeed(freq)
 SendMessageI_setsoundspeed(freq)
end
function SetSoundVolume(vol)
 SendMessageI_setsoundvolume(vol)
end
function PlaySpeech(e,v)
 SendMessageI_playsound(e,v)
 SendMessageI_playspeech(e,v)
end
function StopSpeech(e,v)
 SendMessageI_stopsound(e,v)
 SendMessageI_stopspeech(e,v)
end
function PlayVideo(e,v)
 SendMessageI_playvideo(e,v)
end
function PlayVideoNoSkip(e,v)
 SendMessageI_playvideonoskip(e,v)
end
function PromptVideo(e,v)
 SendMessageI_promptvideo(e,v)
end
function PromptVideoNoSkip(e,v)
 SendMessageI_promptvideonoskip(e,v)
end
function StopVideo(e,v)
 SendMessageI_stopvideo(e,v)
end
function FireWeapon(e)
 SendMessageI_fireweapon(e)
end
function FireWeaponInstant(e)
 SendMessageI_fireweaponinstant(e)
end
function HurtPlayer(e,v)
 SendMessageI_hurtplayer(e,v)
end
function DrownPlayer(e,v)
 SendMessageI_drownplayer(e,v)
end
function SwitchScript(e,str)
 SendMessageS_switchscript(e,str)
end
function SetCharacterSoundSet(e)
 SendMessageI_setcharactersoundset(e)
end
function SetCharacterSound(e,str)
 SendMessageS_setcharactersound(e,str)
end
function PlayCharacterSound(e,str)
 SendMessageS_playcharactersound(e,str)
end

-- reinstated custom music commands (see below)
--function PlayCombatMusic(playTime,fadeTime)
-- music_play_timecue(2,playTime,fadeTime);
--end
--function PlayFinalAssaultMusic(fadeTime)
-- music_play_cue(3,fadeTime);
--end

function DisableMusicReset(v)
 SendMessageI_disablemusicreset(v)
end
function HideLight(e)
 SendMessageI_setlightvisible(e,0);
end
function ShowLight(e)
 SendMessageI_setlightvisible(e,1)
end
function LoadImages(str,v)
 SendMessageS_loadimages(v,str)
end
function SetImagePosition(x,y)
 SendMessageF_setimagepositionx(x)
 SendMessageF_setimagepositiony(y)
end
function ShowImage(i)
 SendMessageI_showimage(i)
end
function HideImage(i)
 SendMessageI_hideimage(i)
end
function SetImageAlignment(i)
 SendMessageI_setimagealignment(i)
end
function Text(x,y,size,txt)
 SendMessageF_textx(x)
 SendMessageF_texty(y)
 SendMessageI_textsize(size)
 SendMessageS_texttxt(txt)
end
function TextCenterOnX(x,y,size,txt)
 SendMessageF_textx(x)
 SendMessageF_texty(y)
 SendMessageI_textsize(size)
 SendMessageI_textcenterx(0)
 SendMessageS_texttxt(txt)
end
function TextColor(x,y,size,txt,r,g,b)
 SendMessageF_textx(x)
 SendMessageF_texty(y)
 SendMessageI_textsize(size)
 SendMessageI_textred(r)
 SendMessageI_textgreen(g)
 SendMessageI_textblue(b)
 SendMessageS_texttxt(txt)
end
function TextCenterOnXColor(x,y,size,txt,r,g,b)
 SendMessageF_textx(x)
 SendMessageF_texty(y)
 SendMessageI_textsize(size)
 SendMessageI_textcenterx(0)
 SendMessageI_textred(r)
 SendMessageI_textgreen(g)
 SendMessageI_textblue(b)
 SendMessageS_texttxt(txt)
end
function Panel(x,y,x2,y2)
 SendMessageF_panelx(x)
 SendMessageF_panely(y)
 SendMessageF_panelx2(x2)
 SendMessageF_panely2(y2)
end
function SetSkyTo(str)
 SendMessageS_setskyto(str)
end
function GetInKey()
	return g_InKey;
end
function GetScancode()
	return g_Scancode;
end
function StartGame()
 SendMessage_startgame()
end
function LoadGame()
 SendMessage_loadgame()
end
function SaveGame()
 SendMessage_savegame()
end
function QuitGame()
 SendMessage_quitgame()
end
function LeaveGame()
 SendMessage_leavegame()
end
function ResumeGame()
 SendMessage_resumegame()
end
function SwitchPage(strPageName)
 SendMessageS_switchpage(strPageName)
end
function SwitchPageBack()
 SendMessage_switchpageback()
end
function LevelFilenameToLoad(strLevelName)
 SendMessageS_levelfilenametoload(strLevelName)
end
function TriggerFadeIn()
 SendMessage_triggerfadein()
end
function WinGame()
 SendMessage_wingame()
end
function LoseGame()
 SendMessage_losegame()
end
function SetGameQuality(v)
 SendMessageI_setgamequality(v)
end
function SetPlayerFOV(v)
 g_PlayerFOV = v
 SendMessageI_setplayerfov(v)
end
function SetGameSoundVolume(v)
 SendMessageI_setgamesoundvolume(v)
end
function SetGameMusicVolume(v)
 SendMessageI_setgamemusicvolume(v)
end
function SetLoadingResource(i,v)
 SendMessageI_setloadingresource(i,v)
end

-- Old Common Multiplayer
function MP_IsServer()
 -- 1 = is the server, 0 = is not the server
 return mp_isServer;
end
function SetMultiplayerGameMode(mode)
 SendMessageI_mpgamemode(mode)
 mp_gameMode = mode;
end
function GetMultiplayerGameMode(mode)
 return mp_gameMode;
end
function SetServerTimer(t)
	SendMessageI_setservertimer(t)
	mp_servertimer = t;
end
function GetServerTimer(t)
	return mp_servertimer;
end
function GetServerTimerPassed()
	return os.time()-mp_servertimer;
end
function ServerRespawnAll()
	SendMessageI_serverrespawnall(0)
end
function ServerEndPlay()
	SendMessageI_serverendplay(0)
end
function GetShowScores()
	return mp_showscores;
end
function SetServerKillsToWin(v)
	SendMessageI_setserverkillstowin(v)
end
function GetMultiplayerTeamBased()
	return mp_teambased;
end
function SetMultiplayerGameFriendlyFireOff()
	mp_friendlyfireoff = 1;
end
function SetNameplatesOff()
	SendMessageI_nameplatesoff(v)
end
function GetCoOpMode()
	return mp_coop
end
function GetCoOpEnemiesAlive()
	return mp_enemiesLeftToKill
end

-- Ancient LUA commands (superseded by removing hard coded systems)
function CharacterControlManual(e)
 SendMessageI_charactercontrolmanual(e)
end
function CharacterControlLimbo(e)
 SendMessageI_charactercontrollimbo(e)
end
function CharacterControlUnarmed(e)
 SendMessageI_charactercontrolunarmed(e)
end
function CharacterControlArmed(e)
 SendMessageI_charactercontrolarmed(e)
end
function CharacterControlFidget(e)
 SendMessageI_charactercontrolfidget(e)
end
function CharacterControlDucked(e)
 SendMessageI_charactercontrolducked(e)
end
function CharacterControlStand(e)
 SendMessageI_charactercontrolstand(e)
end
function SetCharacterToWalk(e)
 SendMessageI_setcharactertowalkrun(e,0);
end
function SetCharacterToRun(e)
 SendMessageI_setcharactertowalkrun(e,1)
end
function SetCharacterToStrafeLeft(e)
 SendMessageI_setcharactertostrafe(e,0);
end
function SetCharacterToStrafeRight(e)
 SendMessageI_setcharactertostrafe(e,1)
end
function SetCharacterVisionDelay(e,v)
 SendMessageI_setcharactervisiondelay(e,v)
end
function LockCharacterPosition(e,v)
 SendMessageI_setlockcharacter(e,1)
end
function UnlockCharacterPosition(e,v)
 SendMessageI_setlockcharacter(e,0);
end

-- Custom Music Commands
function music_load(id,str,interval,length)
	-- load in and setup a piece of music
	if str ~= "" then
		SendMessageS_musicload(id,str)
		SendMessageI_musicsetinterval(id,interval)
		SendMessageI_musicsetlength(id,length)
	end
end
function music_play(m,fadeTime)
	-- plays m fading in over fadetime, stopping any other music over fadetime * 3
	music_set_fadetime(fadeTime)
	SendMessageI_musicplayfade(m)
end
function music_play_instant(m,fadeTime)
	-- plays m fading in over fadetime, stopping any other music instantly
	music_set_fadetime(fadeTime)
	SendMessageI_musicplayinstant(m)
end
function music_play_cue(m,fadeTime)
	-- plays m at the next interval of the current music, starting at full volume and fading out the current music by fadeTime
	music_set_fadetime(fadeTime)
	SendMessageI_musicplaycue(m)
end
function music_play_time(m,playTime,fadeTime)
	-- as music_play, but plays m for time playTime before returning to play the default track
	music_set_fadetime(fadeTime)
	SendMessageI_musicplaytime(m,playTime)
end
function music_play_timecue(m,playTime,fadeTime)
	-- as music_play_cue, but plays m for time playTime before returning to play the default track, using timing intervals
	music_set_fadetime(fadeTime)
	SendMessageI_musicplaytimecue(m,playTime)
end
function music_stop(fadeTime)
	-- stops the music playing in time fadeTime
	music_set_fadetime(fadeTime)
	SendMessage_musicstop()
end
function music_set_volume(v,fadeTime)
	-- sets the global music volume and fades the playing track to this volume over fadeTime
	music_set_fadetime(fadeTime)
	SendMessageI_musicsetvolume(v)
end
function music_set_default(m)
	-- sets the default music track m (the track returned to after another track is played for a finite period of time)
	SendMessageI_musicsetdefault(m)
end
function music_set_fadetime(f)
	-- sets the global fadetime for all subsequent commands that use fade
	SendMessageI_musicsetfadetime(f)
end

-- OLD Particle System (the old legacy commands, the new 'effect' commands are directly added to LUA in Engine for speed)
function StartParticleEmitter(e)
 SendMessageF_startparticleemitter(e,0);
end
function StopParticleEmitter(e)
 SendMessageF_stopparticleemitter(e,0);
end

--Ancient AI Globals (old legacy globals no longer supported with newer scripts)
AI_CLOSEST_TO_PLAYER = 50
ai_old_health = {}
ai_soldier_state = {}
ai_soldier_pathindex = {}
ai_combat_mode = {}
ai_combat_state_delay = {}
ai_combat_old_time = {}
ai_combat_turn_delay = 1
ai_combat_cover_delay = 4
ai_combat_delay_after_finding = 2
ai_next_aggro_delay = 0
ai_start_x = {}
ai_start_z = {}
ai_dest_x = {}
ai_dest_z = {}
ai_ran_to_cover = {}
ai_alerted_mode = {}
ai_alerted_state_delay = {}
ai_alerted_old_time = {}
ai_alerted_spoken = {}
ai_returning_home = {}
ai_alert_x = -10000
ai_alert_z = -10000
ai_alert_counter = 0
ai_alert_entity = 0
ai_path_point_index = {}
ai_path_point_direction = {}
ai_path_point_max = {}
ai_starting_heath = {}
ai_patrol_x = {}
ai_patrol_z = {}
ai_aggro_x = nil
ai_aggro_z = nil
ai_aggro_entity = nil
ai_aggro_range = 400
PlayerDist = 0

--[[

Direct call LUA Commands:

UpdateWeaponStats: UpdateWeaponStats() -- Call this to instantly update all g_Weapon* data
ResetWeaponSystems: ResetWeaponSystems ( ) -- resets any projectiles currently active in game

SetWeaponSlot: SetWeaponSlot ( index, got flag, preference flag ) -- Sets the weapon data directly, index is 1 through 10. The got flag value is the weapon ID of the weapon you have in that slot. By setting it, you can effectively grant the player that weapon without them having to pick it up, but you should ensure that weapon is placed somewhere in the level so it can load it the particulars.  The preference flag value also takes a Weapon ID and is used when you want to make sure when a weapon is collected, it will go to that slot, so  SetWeaponSlot ( 9, 0, 12 ) will make sure that when you collect Weapon ID 12 it will automatically be assigned to slot 9.

GetWeaponAmmo: quantity = GetWeaponAmmo ( index ) -- Sets the weapon data directly, index is 1 through 10
SetWeaponAmmo: SetWeaponAmmo ( index, ammo quantity ) -- Sets the weapon data directly, index is 1 through 10
GetWeaponClipAmmo: quantity = GetWeaponClipAmmo ( index ) -- Sets the weapon data directly, index is 1 through 10
SetWeaponClipAmmo: SetWeaponClipAmmo ( index, clip quantity ) -- Sets the weapon data directly, index is 1 through 10
GetWeaponPoolAmmoIndex: poolindex = GetWeaponPoolAmmoIndex ( index ) -- Gets the weapons pool index, index is 1 through 10
GetWeaponPoolAmmo: quantity = GetWeaponPoolAmmo ( poolindex ) -- Sets the weapon data directly, index is poolindex returned by GetWeaponPoolAmmoIndex(slot)
SetWeaponPoolAmmo: SetWeaponPoolAmmo ( poolindex, pool ammo quantity ) -- Sets the weapon data directly, index is poolindex returned by GetWeaponPoolAmmoIndex(slot)
GetWeaponSlot: GetWeaponSlot ( index ) -- Gets WeaponID from slot, index is 1 through 10
GetWeaponSlotPref: GetWeaponSlotPref ( index ) -- Gets preferred WeaponID from slot, index is 1 through 10
GetPlayerWeaponID: WeaponID = GetPlayerWeaponID ( ) -- Returns the WeaponID the player is currently carrying
GetWeaponID: GetWeaponID ( GunNameString ) -- Gets WeaponID associated with current GunNameString
GetEntityWeaponID: weaponid = GetEntityWeaponID ( e ) -- Gets the WeaponID associated with the entity specified
GetWeaponName : name = GetWeaponName(weaponid) -- Gets the weapon name from the WeaponID
SetWeaponDamage: SetWeaponDamage ( WeaponID, FireModeIndex, Value ) -- Sets damage value of weapon
SetWeaponAccuracy: SetWeaponAccuracy ( WeaponID, FireModeIndex, Value ) -- Sets accuracy value of weapon
SetWeaponReloadQuantity: SetWeaponReloadQuantity ( WeaponID, FireModeIndex, Value ) -- Sets reload quantity value of weapon
SetWeaponFireIterations: SetWeaponFireIterations ( WeaponID, FireModeIndex, Value ) -- Sets shot iterations value of weapon
SetWeaponRange: SetWeaponRange ( WeaponID, FireModeIndex, Value ) -- Sets range value of weapon
SetWeaponDropoff: SetWeaponDropoff ( WeaponID, FireModeIndex, Value ) -- Sets dropoff value of weapon
SetWeaponSpotLighting: SetWeaponSpotLighting ( WeaponID, FireModeIndex, Value ) -- Sets whether weapon uses spot light effect
GetWeaponDamage: Value = GetWeaponDamage ( WeaponID, FireModeIndex ) -- Get damage value of weapon
GetWeaponAccuracy: Value = GetWeaponAccuracy ( WeaponID, FireModeIndex ) -- Get accuracy value of weapon
GetWeaponReloadQuantity: Value = GetWeaponReloadQuantity ( WeaponID, FireModeIndex ) -- Get reload quantity value of weapon
GetWeaponFireIterations: Value = GetWeaponFireIterations ( WeaponID, FireModeIndex ) -- Get shot iterations value of weapon
GetWeaponRange: Value = GetWeaponRange ( WeaponID, FireModeIndex ) -- Get range value of weapon
GetWeaponDropoff: Value = GetWeaponDropoff ( WeaponID, FireModeIndex ) -- Get dropoff value of weapon
GetWeaponSpotLighting: Value = GetWeaponSpotLighting ( WeaponID, FireModeIndex ) -- Get whether weapon uses spot light effect
SetWeaponFireRate: SetWeaponFireRate ( WeaponID, FireModeIndex, Value ) -- Set the assigned firerate of the weapon
GetWeaponFireRate: Value = GetWeaponFireRate ( WeaponID, FireModeIndex ) -- Get the assigned firerate of the weapon (if any)
SetWeaponClipCapacity: SetWeaponClipCapacity ( WeaponID, FireModeIndex, Value ) -- Set the assigned clip capacity of the weapon
GetWeaponClipCapacity: Value = GetWeaponClipCapacity ( WeaponID, FireModeIndex ) -- Get the assigned clip capacity of the weapon

WrapAngle : V = WrapAngle ( Angle, Dest, Smoothing ) will smoothly change Angle to Dest, returning in V. Smoothing 0 to 1.
SetCameraOverride : SetCameraOverride ( i ) where i is 0-off, 1-position only, 2-angle only, 3-position and angle
SetCameraPosition : SetCameraPosition ( c, x, y, z ) where c should be zero and XYZ are 3D coordinates
SetCameraAngle : SetCameraAngle ( c, x, y, z ) where c should be zero and XYZ are euler angles
GetCameraPositionX : V = GetCameraPositionX ( c ) where V is the X coordinate of the specified camera
GetCameraPositionY : V = GetCameraPositionY ( c ) where V is the Y coordinate of the specified camera
GetCameraPositionZ : V = GetCameraPositionZ ( c ) where V is the Z coordinate of the specified camera
GetCameraAngleX : V = GetCameraAngleX ( c ) where V is the X angle of the specified camera
GetCameraAngleY : V = GetCameraAngleY ( c ) where V is the Y angle of the specified camera
GetCameraAngleZ : V = GetCameraAngleZ ( c ) where V is the Z angle of the specified camera

ForcePlayer : ForcePlayer ( angle, velocity ) where angle is the angle and physics velocity to push the player

GetOriginalEntityElementMax: count = GetOriginalEntityElementMax() -- get original count of entities in level at fresh start
SetEntityActive: SetEntityActive ( e, active flag ) -- Sets the entity data directly, e is entity index
SetEntityActivated: SetEntityActivated ( e, activated flag ) -- Sets the entity data directly, e is entity index
SetEntityHasKey : SetEntityHasKey ( e, haskey flag ) -- Sets the entity data directly, e is entity index

SetEntityObjective: SetEntityObjective ( e, anobjective ) -- Will mark entity as an objective if set to 1
SetEntityCollectable: SetEntityCollectable ( e, cancollect ) -- Can collect this if cancollect is set to 1
SetEntityCollected: SetEntityCollected ( e, collectedflag ) -- Sets that the entity has been collected
GetEntityObjective: isobjective = GetEntityObjective ( e ) -- Gets whether the entity is an objective
GetEntityCollectable: cancollect = GetEntityCollectable ( e ) -- Gets whether the entity can be collected
GetEntityCollected: collectedflag = GetEntityCollected ( e ) -- Gets whether the entity has been collected
SetEntityUsed: SetEntityUsed ( e, usedflag ) -- Sets that the entity has been used
GetEntityUsed: usedflag = GetEntityUsed ( e ) -- Gets whether the entity has been used
SetEntityQuantity: SetEntityQuantity ( e, qty ) -- Sets the entity quantity value
GetEntityQuantity: qty = GetEntityQuantity ( e ) -- Get the entity quantity value

GetEntityVisibility : vis = GetEntityVisibility ( e ) -- where e is the entity number and return vis (0-hidden,1-shown)
GetEntityActive : flag = GetEntityActive ( e ) -- returns 1 if the entity is active and not in process of dying
GetEntityPositionX : x = GetEntityPositionX ( e ) -- where e is the entity number and returns the X position
GetEntityPositionY : y = GetEntityPositionY ( e ) -- where e is the entity number and returns the Y position
GetEntityPositionZ : z = GetEntityPositionZ ( e ) -- where e is the entity number and returns the Z position
GetEntityAngleX : x = GetEntityAngleX ( e ) -- where e is the entity number and returns the X angle
GetEntityAngleY : y = GetEntityAngleY ( e ) -- where e is the entity number and returns the Y angle
GetEntityAngleZ : z = GetEntityAngleZ ( e ) -- where e is the entity number and returns the Z angle
GetMovementSpeed : speed = GetMovementSpeed ( e ) -- where e is the entity number and speed is the SPEED of the entity
GetAnimationSpeed : speed = GetAnimationSpeed ( e ) -- where e is the entity number and speed is the ANIMSPEED of the entity
SetAnimationSpeedModulation : SetAnimationSpeedModulation ( e, speed ) -- where e is the entity number and speed is animation speed modulator
GetAnimationSpeedModulation : speed = GetAnimationSpeedModulation ( e ) -- where e is the entity number and speed is the animation speed modulator
GetMovementDelta : delta = GetMovementDelta ( e ) -- where e is the entity number and delta is the movement distance since the last cycle
GetEntityCollBox : GetEntityCollBox ( e ) -- useful command see below (though typo from legacy issue must retain for backward compat.)
GetEntityColBox : minx,miny,minz,maxx,maxy,maxz = GetEntityColBox ( e ) -- returns the unrotated boundbox for the specified entity
GetEntityScales : sx,sy,sz = GetEntityScales ( e ) -- returns the XYZ scale of the specified entity
GetEntityName : string = GetEntityName ( e ) -- will return the name of the entity specified by e
GetEntityParentID : value = GetEntityParentID ( e ) -- will return the parent ID used by this entity
GetMovementDeltaManually : GetMovementDeltaManually ( e ) -- useful command

SetEntityUnderwaterMode: SetEntityUnderwaterMode ( e, mode ) -- Set mode to 1 to allow character entity to survive underwater
GetEntityUnderwaterMode: mode = GetEntityUnderwaterMode ( e ) -- Get the underwater mode, a value of 1 means character entity can survive underwater

SetEntityIfUsed: SetEntityIfUsed(e, string) -- Overwrites the IFUSED string of the entity (useful for changing teleport destination marker)
GetEntityIfUsed: string = GetEntityIfUsed(e) -- Get the IFUSED string of the specified entity

AdjustLookSettingHorizLimit : AdjustLookSettingHorizLimit ( e, hlimit ) -- where e is the entity number and param sets the horizontal limit
AdjustLookSettingHorizOffset : AdjustLookSettingHorizOffset ( e, hoffset ) -- where e is the entity number and param sets the horizontal offset
AdjustLookSettingVertLimit : AdjustLookSettingVertLimit ( e, vlimit ) -- where e is the entity number and param sets the vertical limit
AdjustLookSettingVertOffset : AdjustLookSettingVertOffset ( e, voffset ) -- where e is the entity number and param sets the vertical offset
AdjustAimSettingHorizLimit : AdjustAimSettingHorizLimit ( e, hlimit ) -- where e is the entity number and param sets the horizontal limit
AdjustAimSettingHorizOffset : AdjustAimSettingHorizOffset ( e, hoffset ) -- where e is the entity number and param sets the horizontal offset
AdjustAimSettingVertLimit : AdjustAimSettingVertLimit ( e, vlimit ) -- where e is the entity number and param sets the vertical limit
AdjustAimSettingVertOffset : AdjustAimSettingVertOffset ( e, voffset ) -- where e is the entity number and param sets the vertical offset

GetEntityFootfallMax : count = GetEntityFootfallMax(e) -- returns the footfall max value stored in the specified entity
GetEntityFootfallKeyframe : count = GetEntityFootfallKeyframe(e,index,leftorright) -- returns the animation frame stored in the entity at the index, left or right
GetEntityAnimationNameExist : count = GetEntityAnimationNameExist(e,name) -- return 1 if the animation name exists in the object
GetEntityAnimationNameExistAndPlaying : result = GetEntityAnimationNameExistAndPlaying(e,name) -- return 1 if the animation name exists and is playing in the object
GetEntityAnimationTriggerFrame : frame = GetEntityAnimationTriggerFrame(e,triggerindex) -- return the frame value of the trigger stored in the object anim by index (1-3)
GetEntityAnimationStartFinish : start, finish = GetEntityAnimationStartFinish(e,name) -- return the start and finish frame of the specified animation in the object
 
GetEntitiesWithinCone : count = GetEntitiesWithinCone(x,y,z,ax,ay,az,cone angle,cone distance) -- creates a list of all entities within the specified cone
GetEntityWithinCone : e = GetEntityWithinCone(index) -- returns the entity index stored in the above created list using the specified index (zero indexed)

SetEntityString : SetEntityString ( e, slot, string, loadSound ) -- where e is the entity number and slot (0-4) to write the string into
                                                                 -- set loadSound to 1 to also relplace the sound sample in the specified
																 -- slot with the one pointed to by 'string'
GetEntityString : GetEntityString ( e, slot ) -- where e is the entity number and slot (0-6) is the sound slot index (soundslot 4 is not used)

GetEntitySpawnAtStart : state = GetEntitySpawnAtStart ( e ) -- returns the state of the spawn (0-dont spawn at start, 1-spawn at start, 2-spawned during game)
GetEntityFilePath : string = GetEntityFilePath ( e ) -- returns the entity file path to be used for helping inventory image systems
GetEntityAnimationStart : frame = GetEntityAnimationStart ( e, animsetindex ) -- returns frame (Y) stored in FPE under animX = Y,Z where X is animsetindex
GetEntityAnimationFinish : frame = GetEntityAnimationFinish ( e, animsetindex ) -- returns frame (Z) stored in FPE under animX = Y,Z where X is animsetindex
GetEntityAnimationFound : flag = GetEntityAnimationFound ( e, animsetindex ) -- returns a 1 if this value was set in FPE anim system (not CSI)
GetObjectAnimationFinished : flag = GetObjectAnimationFinished ( e ) -- returns a 1 if the specified entity has finished playing the specified animation
GetObjectAnimationFinished : flag = GetObjectAnimationFinished ( e, shaveendframes ) -- as above but shaves specified number of frames off the end of the animation

Entity Creation and Destruction
-------------------------------
CreateEntityIfNotPresent : CreateEntityIfNotPresent ( e ) -- ensures this entity element index is available by the level
SpawnNewEntity : newe = SpawnNewEntity ( currente [, nobody] ) -- will create a new entity by copying the entity specified by currente; with nobody 1 the copy gets no physics body until its first CollisionOn (for one whose collision is turned off at once; the stock exe ignores nobody and makes the body)
DeleteNewEntity : DeleteNewEntity ( e ) -- will delete any entity specified by e newly created during the level, but no original ones

Other Things
------------
GetAmmoClip : ammoclip = GetAmmoClip ( e ) -- returns the current ammo in clip for the held weapon of the entity
SetAmmoClip : SetAmmoClip ( e, ammoquantity ) -- sets the ammo clip quantity for the held weapon of the entity
	
FreezeEntity : FreezeEntity ( e [, mode] ) -- where e is the entity index and mode is reserved. Holds a physics entity still (gravity and hits no longer move it) until UnFreezeEntity
UnFreezeEntity : UnFreezeEntity ( e ) -- where e is the entity index to be unfrozen from a previous call to FreezeEntity
SleepEntity : bodies = SleepEntity ( e [, force] ) -- puts a physics entity's body to sleep with every moving body touching it, directly or through others (its island; one put to sleep alone is woken at the next step while its island is awake), velocities zeroed. They wake as any sleeping body does: a blast, a hit, a velocity set, an awake body touching them. Returns how many bodies slept: 0 when e has no moving body, or its island holds a character, a body kept awake or a body still moving (faster than twice its sleep speeds: try again later); with force 1 the island's other bodies may move up to ten times their sleep speeds (one rocking where it is wedged, such as a dropped gun in a wreck), e's own body still within twice

GetTerrainHeight : height = GetTerrainHeight(x,z) -- where X and Z are the coordinates on the terrain you want the height from
GetSurfaceHeight : height, hit = GetSurfaceHeight(x,y,z) -- where X, Y and Z are the coordinates above/on a surface, find the height of the surface below; hit is 1 if a surface was found, 0 (and height 0) on a miss

GetFogNearest : value = GetFogNearest ( ) -- gets the setting value currently used in the game
GetFogDistance : value = GetFogDistance ( ) -- gets the setting value currently used in the game
GetFogRed : value = GetFogRed ( ) -- gets the setting value currently used in the game
GetFogGreen : value = GetFogGreen ( ) -- gets the setting value currently used in the game
GetFogBlue : value = GetFogBlue ( ) -- gets the setting value currently used in the game
GetFogIntensity : value = GetFogIntensity ( ) -- gets the setting value currently used in the game
GetAmbienceIntensity : value = GetAmbienceIntensity ( ) -- gets the setting value currently used in the game
GetAmbienceRed : value = GetAmbienceRed ( ) -- gets the setting value currently used in the game
GetAmbienceGreen : value = GetAmbienceGreen ( ) -- gets the setting value currently used in the game
GetAmbienceBlue : value = GetAmbienceBlue ( ) -- gets the setting value currently used in the game
GetSurfaceRed : value = GetSurfaceRed ( ) -- gets the setting value currently used in the game
GetSurfaceGreen : value = GetSurfaceGreen ( ) -- gets the setting value currently used in the game
GetSurfaceBlue : value = GetSurfaceBlue ( ) -- gets the setting value currently used in the game
GetSurfaceIntensity : value = GetSurfaceIntensity ( ) -- gets the setting value currently used in the game
GetPostVignetteRadius : value = GetPostVignetteRadius ( ) -- gets the setting value currently used in the game
GetPostVignetteIntensity : value = GetPostVignetteIntensity ( ) -- gets the setting value currently used in the game
GetPostMotionDistance : value = GetPostMotionDistance ( ) -- gets the setting value currently used in the game
GetPostMotionIntensity : value = GetPostMotionIntensity ( ) -- gets the setting value currently used in the game
GetPostDepthOfFieldDistance : value = GetPostDepthOfFieldDistance ( ) -- gets the setting value currently used in the game
GetPostDepthOfFieldIntensity : value = GetPostDepthOfFieldIntensity ( ) -- gets the setting value currently used in the game

LoadImage: myImage = LoadImage ( "myFolder\\myImage.png" ) -- Anywhere inside GG Files folder (however it is best to put into scriptbank images folder for making standalones)
GetImageWidth: myWidth = GetImageWidth ( 1 ) -- get percentage width of image
GetImageHeight: myHeight = GetImageHeight ( 1 ) -- get percentage height of image
CreateSprite: mySprite = CreateSprite ( myImage )
PasteSprite: PasteSprite ( mySprite ) -- pastes the sprite at its current location (good for pasting sprite BEHIND text)
PasteSpritePosition: PasteSpritePosition ( mySprite , x , y ) -- as above but pasted at a specified XY coordinate
DeleteSprite: DeleteSprite ( mySprite )
SetSpriteScissor: SetSpriteScissor ( x , y, width, height ) -- sets an area to clip future sprite pastes into. Use 0,0,0,0 to remove scissor
SetSpritePosition: SetSpritePosition ( mySprite , x , y )
SetSpritePriority: SetSpritePriority ( mySprite , value ) -- where value can be -1 to force pasted sprites on top of everything else, even HUDs
SetSpriteSize: SetSpriteSize ( mySprite , sizeX , sizeZ ) -- passing -1 as one of the params ensure the sprite retains its aspect ratio
SetSpriteDepth: SetSpriteDepth ( mySprite , 10 ) (0 is front, 100 is back)
SetSpriteColor: SetSpriteColor ( mySprite , red , green, blue, alpha )
SetSpriteAngle: SetSpriteAngle ( mySprite , 180 )
SetSpriteOffset: SetSpriteOffset ( mySprite , 5 , -1 ) -- would be the centre for a 10% width image assigned to a sprite, passing -1 as one of the params ensure the sprite retains its aspect ratio
SetSpriteImage: SetSpriteImage ( mySprite , myImage )

LoadGlobalSound: LoadGlobalSound ( filename, iID ) -- where iID is an index greater than zero and filename points to a WAV file in your game installation	
DeleteGlobalSound: DeleteGlobalSound ( iID ) -- where iID is the index of the sound loaded in the load command
PlayGlobalSound: PlayGlobalSound ( iID ) -- where iID is the index of the sound to be played
LoopGlobalSound: LoopGlobalSound ( iID ) -- where iID is the index of the sound to be looped
StopGlobalSound: StopGlobalSound ( iID ) -- where iID is the index of the sound to be stopped
SetGlobalSoundSpeed: SetGlobalSoundSpeed ( iID, speed ) -- where iID is the index of the sound to change the speed of
SetGlobalSoundVolume: SetGlobalSoundVolume ( iID, volume ) -- where iID is the index of the sound to change the volume of
GetGlobalSoundExist: GetGlobalSoundExist ( iID ) -- where iID is the index of the sound to check existence of
GetGlobalSoundPlaying: GetGlobalSoundPlaying ( iID ) -- where iID is the index of the sound to check if playing
GetGlobalSoundLooping: GetGlobalSoundLooping ( iID ) -- where iID is the index of the sound to check if looping
GetSoundPlaying: GetSoundPlaying ( e, v ) -- where e is the entity element index and v is the sound slot (0-5)

PlayRawSound: PlayRawSound ( iID ) -- where iID is the raw engine sound index
LoopRawSound: LoopRawSound ( iID ) -- where iID is the raw engine sound index
StopRawSound: StopRawSound ( iID ) -- where iID is the raw engine sound index
SetRawSoundVolume: SetRawSoundVolume ( iID, iVolume ) -- where iID is the raw engine sound index and iVolume from 0 to 100
RawSoundExist: iFlag = RawSoundExist ( iID ) -- where iID is the raw engine sound index
RawSoundPlaying: iFlag = RawSoundPlaying ( iID ) -- where iID is the raw engine sound index
SetRawSoundSpeed: SetRawSoundSpeed ( iID, speed ) -- where iID is the raw engine sound index and speed is speciried
GetEntityRawSound: iID = GetEntityRawSound ( e ) -- where e is the entity index
StartAmbientMusicTrack: StartAmbientMusicTrack ( ) -- starts any specified ambient music track loop
StopAmbientMusicTrack: StopAmbientMusicTrack ( ) -- stops any specified ambient music track loop
SetAmbientMusicTrackVolume : SetAmbientMusicTrackVolume ( volume ) -- where volume is a percentage betweem 0 and 100
StartCombatMusicTrack: StartCombatMusicTrack ( ) -- starts any specified combat music track loop
StopCombatMusicTrack: StopCombatMusicTrack ( ) -- stops any specified combat music track loop
SetCombatMusicTrackVolume : SetCombatMusicTrackVolume ( volume ) -- where volume is a percentage betweem 0 and 100
GetCombatMusicTrackPlaying: iID = GetCombatMusicTrackPlaying ( ) -- returns 1 if the combat music is playing

GetTimeElapsed: delta = GetTimeElapsed() -- returns the float time slice of the current game cycle

SetGamePlayerControlJetpackMode: SetGamePlayerControlJetpackMode ( iValue ) -- sets the player control data to specified value
GetGamePlayerControlJetpackMode: iValue = GetGamePlayerControlJetpackMode() -- gets the specified player control data value
SetGamePlayerControlJetpackFuel: SetGamePlayerControlJetpackFuel ( iValue ) -- sets the player control data to specified value
GetGamePlayerControlJetpackFuel: iValue = GetGamePlayerControlJetpackFuel() -- gets the specified player control data value
SetGamePlayerControlJetpackHidden: SetGamePlayerControlJetpackHidden ( iValue ) -- sets the player control data to specified value
GetGamePlayerControlJetpackHidden: iValue = GetGamePlayerControlJetpackHidden() -- gets the specified player control data value
SetGamePlayerControlJetpackCollected: SetGamePlayerControlJetpackCollected ( iValue ) -- sets the player control data to specified value
GetGamePlayerControlJetpackCollected: iValue = GetGamePlayerControlJetpackCollected() -- gets the specified player control data value
SetGamePlayerControlSoundStartIndex: SetGamePlayerControlSoundStartIndex ( iValue ) -- sets the player control data to specified value
GetGamePlayerControlSoundStartIndex: iValue = GetGamePlayerControlSoundStartIndex() -- gets the specified player control data value
SetGamePlayerControlJetpackParticleEmitterIndex: SetGamePlayerControlJetpackParticleEmitterIndex ( iValue ) -- uses the OLD particle system
GetGamePlayerControlJetpackParticleEmitterIndex: iValue = GetGamePlayerControlJetpackParticleEmitterIndex() -- uses the OLD particle system
SetGamePlayerControlJetpackThrust: SetGamePlayerControlJetpackThrust ( iValue ) -- sets the player control data to specified value
GetGamePlayerControlJetpackThrust: iValue = GetGamePlayerControlJetpackThrust() -- gets the specified player control data value

SetPlayerRun: SetPlayerRun(1) -- 1: enable or 0: disable player's ability to run. 
SetPlayerWeapons: SetPlayerWeapons(0) disabled player weapons, SetPlayerWeapons(1) restores them
SetAttachmentVisible: SetAttachmentVisible(e,1). 1 sets the entities attachment to be visible (such as their weapon), 0 switches it off
SetFlashLight: SetFlashLight(1), 1 switches the flash light on, 0 off
SetFlashLightKeyEnabled: SetFlashLightKeyEnabled(1) - 1 for on, 0 for off. Disables the flash light key from being used
SetOcclusion: SetOcclusion(100), control the occluder from script! set the occlude from 0 (off) to 100 (max). 10 and under for minimal popping

SetFont: SetFont ( "myFont : 1) To change in game font. Fonts 1-3 are the default ones used.
NOTE: You can place your texture atlas bitmap font file in the Files\fontbank\ folder with the name FPSCR-Font-XX.png where XX is the unique name for your font. When you want to use it, simply call the command SetFont ( "XX", YY ) where XX is the unique name above and YY is the index you want to 'overwrite'. Remember to include the 'FPSCR-Font-XX-Subimages.fnt' file which describes the coordinates within the texture atlas image for the specific bitmap fonts contained therein.
GetTextWidth: w = GetTextWidth ( size, text ) -- width of Text() output in percent of screen width (same units as the Text x position)
GetTextHeight: h = GetTextHeight ( size ) -- height of a line of Text() output in percent of screen height

Include(file) -- to include a LUA script file from scriptbank.

GetFirstEntitySpawn: GetFirstEntitySpawn() -- useful for scripts (such as the radar) that need to deal with entities that can come into existence at any time (returns 0 if no new entity)
GetNextEntitySpawn: GetNextEntitySpawn() -- returns the next new spawn in the list, 0 if the end of the list is reached

GetDeviceWidth: GetDeviceWidth() -- returns the display width in pixels
GetDeviceHeight: GetDeviceHeight() -- returns the current display height in pixels

GetTimeElapsed: GetTimeElapsed() -- returns the time elapsed since the last cycle
GetKeyState: x=GetKeyState(y) -- returns 1 in x when keycode specified in y is pressed
SetGlobalTimer: SetGlobalTimer(milliseconds) -- adjusts the time returned by Timer() and g_Time to a new system time
Timer: x=Timer() -- returns the global time in milliseconds since the system started
MouseMoveX: x=MouseMoveX() -- returns the mouse delta since the last time it was called
MouseMoveY: y=MouseMoveY() -- returns the mouse delta since the last time it was called
GetDesktopWidth: GetDesktopWidth() -- returns the current desktop width
GetDesktopHeight: GetDesktopHeight() -- returns the current desktop height
CurveValue: x=CurveValue(dest,current,smooth) -- returns the smoothed value based on the smooth factor
CurveAngle: a=CurveAngle(dest,current,smooth) -- as CurveValue but handles angles from 0-360 degrees
PositionMouse: PositionMouse(x,y) -- repositions the hardware mouse pointer in real-time (screen size coords)
GetDynamicCharacterControllerDidJump: x=GetDynamicCharacterControllerDidJump() -- returns 1 if controller jumped
GetCharacterControllerDucking: x=GetCharacterControllerDucking() -- returns 1 if the player has been forced to duck
WrapValue: x=WrapValue(y) -- takes the value y and wraps it to an angle between 0-360 degrees
GetElapsedTime: x=GetElapsedTime() -- returns the elapsed delta time since the last game cycle
GetPlrObjectPositionX: x=GetPlrObjectPositionX() -- returns the raw x position of the visible player object
GetPlrObjectPositionY: x=GetPlrObjectPositionY() -- returns the raw y position of the visible player object
GetPlrObjectPositionZ: x=GetPlrObjectPositionZ() -- returns the raw z position of the visible player object
GetPlrObjectAngleX: x=GetPlrObjectAngleX() -- returns the raw x angle of the visible player object
GetPlrObjectAngleY: x=GetPlrObjectAngleY() -- returns the raw y angle of the visible player object
GetPlrObjectAngleZ: x=GetPlrObjectAngleZ() -- returns the raw z angle of the visible player object
GetGroundHeight: y=GetGroundHeight(x,z) -- returns the terrain height of the coordinates specified by x and z
NewXValue: x=NewXValue(current,angle,distance) -- projects a new x position from the specified angle and distance 
NewZValue: z=NewZValue(current,angle,distance) -- projects a new z position from the specified angle and distance
ControlDynamicCharacterController: ControlDynamicCharacterController(iObj, -- controls the physics capsule of the player
 fAngleY
 fAngleX
 fSpeed
 fJump
 fDucking
 fPushAngle
 fPushForce
 fThrustUpwards )
GetCharacterHitFloor: x=GetCharacterHitFloor() -- returns a value if the player hits the floor
GetCharacterFallDistance: x=GetCharacterFallDistance() -- returns the distance the player fell when it hit the floor
RayTerrain: r=RayTerrain(x,y,z,x2,y2,z2) -- returns 1 if the ray cast hits the terrain geometry (only reliable when ray faces directly down)
GetRayCollisionX: x=GetRayCollisionX() -- returns the X position of the terrain hit position
GetRayCollisionY: y=GetRayCollisionY() -- returns the Y position of the terrain hit position
GetRayCollisionZ: z=GetRayCollisionZ() -- returns the Z position of the terrain hit position
IntersectAll: x=IntersectAll(x1,y1,z1,x2,y2,z2,IgnoreObj) -- returns object ID if the ray cast hits ANY entity or lightmapped geometry, and adjusts ray to account for terrain and does not report a terrain hit. IgnoreObj may be a table of object numbers
IntersectAllIncludeTerrain: x=IntersectAllIncludeTerrain(x1,y1,z1,x2,y2,z2,IgnoreObj) -- returns 1 if the ray cast hits ANY entity or lightmapped geometry
IntersectAllIncludeTerrain: x=IntersectAllIncludeTerrain(x1,y1,z1,x2,y2,z2,IgnoreObj,index,life,ignoreplayer,ignoreterrain) -- as above but maintains internal database for faster results, index to identify and life is milliseconds to hold on database, and extra ignore flags
IntersectStatic: x=IntersectStatic(x1,y1,z1,x2,y2,z2,IgnoreObj) -- returns 1 if the ray cast hits non-animating entity or lightmapped geometry. IgnoreObj may be a table of object numbers
IntersectStaticPerformant: x=IntersectStaticPerformant(x1,y1,z1,x2,y2,z2,IgnoreObj,index,life) -- as above but maintains internal database for faster results, index to identify and life is milliseconds to hold on database, and uses physics raycast instead of Wicked geometry raycast
IntersectAll: x=IntersectAll(x1,y1,z1,x2,y2,z2,IgnoreObj) -- returns object ID if the ray cast hits ANY entity or lightmapped geometry. IgnoreObj may be a table of object numbers
GetIntersectCollisionX: x=GetIntersectCollisionX() -- returns the X position of the entity hit position
GetIntersectCollisionY: y=GetIntersectCollisionY() -- returns the Y position of the entity hit position
GetIntersectCollisionZ: z=GetIntersectCollisionZ() -- returns the Z position of the entity hit position
GetIntersectCollisionNX: x=GetIntersectCollisionNX() -- returns the X normal of the entity hit surface
GetIntersectCollisionNY: y=GetIntersectCollisionNY() -- returns the Y normal of the entity hit surface
GetIntersectCollisionNZ: z=GetIntersectCollisionNZ() -- returns the Z normal of the entity hit surface
PositionCamera: PositionCamera(cam,x,y,z) -- positions the specified camera at the xyz position
PointCamera: PointCamera(cam,x,y,z) -- angles the specified camera towards the xyz position
MoveCamera: MoveCamera(cam,step) -- moves the specified camera at its current angle by the step amount
GetObjectExist: x=GetObjectExist(obj) -- returns if the specified object exists
SetObjectFrame: SetObjectFrame(obj,x) -- sets the animation frame of the specified object 
GetObjectFrame: x=GetObjectFrame(obj) -- returns the animation frame of the specified object 
SetObjectSpeed: SetObjectSpeed(obj,x) -- sets the animation speed of the specified object 
GetObjectSpeed: x=GetObjectSpeed(obj) -- returns the animation speed of the specified object 
PositionObject: PositionObject(obj,x,y,z) -- sets the position of the specified object
RotateObject: RotateObject(obj,x,y,z) -- sets the angle of the specified object
GetObjectAngleX: x=GetObjectAngleX(obj) -- returns the X position of the specified object
GetObjectAngleY: y=GetObjectAngleY(obj) -- returns the Y position of the specified object
GetObjectAngleZ: z=GetObjectAngleZ(obj) -- returns the Z position of the specified object
RunCharLoop: RunCharLoop() -- runs the legacy animation system to control character index GetGamePlayerStateCharAnimIndex()
TriggerWaterRipple: TriggerWaterRipple(x,y,z) -- triggers a ripple decal animation at the xyz position 
TriggerWaterRippleSize : TriggerWaterRippleSize ( x, y, z, sizex, sizey [, nx, ny, nz] ) -- a ripple ring of that size; with a surface normal it lies on that surface (a slope) rather than flat. Nothing checks for water, so it can mark wet ground. Not made past GetDecalRange() from the camera
PlayFootfallSound: snd=PlayFootfallSound(type,x,y,z,lastsnd) -- triggers footfall sound and returns raw sound index used
PlayFootfallSound: snd=PlayFootfallSound(type,x,y,z,lastsnd,leftorright,walkorrun) -- additional parameters for greater footfall variety
ResetUnderwaterState: ResetUnderwaterState() -- resets the underwater sub-system when player emerges from water
SetUnderwaterOn: SetUnderwaterOn() -- use this when the player goes underwater for visual changes
SetUnderwaterOff: SetUnderwaterOff() -- use this when the player goes above water for visual changes

------- NEW Particle Effects System --------- 

EffectStart: EffectStart(e) -- starts the animation of a particle effect of the specified Entity
EffectStop: EffectStop(e) -- stops the animation of a particle effect of the specified Entity
EffectSetLocalPosition: EffectSetLocalPosition(e,x,y,z) - sets the local position of the emitter of this particle effect
EffectSetLocalRotation: EffectSetLocalRotation(e,x,y,z) - sets the local rotation of the emitter of this particle effect
EffectSetSpeed: EffectSetSpeed(e,speed) -- sets the overall speed of the specified Entity particle effect, default is 100
EffectSetOpacity: EffectSetOpacity(e,opacity) -- sets the overall opacity of the specified Entity particle effect, default is 100
EffectSetParticleSize: EffectSetParticleSize(e,size) -- sets the overall size of the particle associated with particle effect, default is 100
EffectSetBurstMode: EffectSetBurstMode(e,mode) -- set mode to 0 to have the particle effect repeat, 1 for a single burst when fired
EffectFireBurst: EffectFireBurst(e) -- trigger a particle effect set to automode of zero to burst a particle effect one time
EffectSetFloorReflection: EffectSetFloorReflection(e,active,height) -- set the collision reflection floor for this particle effect
EffectSetBounciness: EffectSetBounciness(e,bounciness) -- set the bounciness of the particle, the default is 100 and ranges from 0 to 500
EffectSetColor: EffectSetColor(e,red,green,blue) -- sets the overall color of the specified Entity particle effect, component range is 0 to 255
EffectSetLifespan: EffectSetLifespan(e,lifespan) -- sets the overall lifespan of a single particle in the particle effect, default is 100

---------------------------------------------


------- OLD Particle system commands --------- 
ParticlesGetFreeEmitter: emitterId = ParticlesGetFreeEmitter() -- where emitterId is the index of the particle emitter
ParticlesAddEmitter: 
	ParticlesAddEmitter(emitterId,               -- create a particle emitter with the following parameters..
						animationSpeed,          -- 
						startsOffRandomAngle,    -- 0 no, 1 yes
						offsetMinX,              -- All these look really daunting if you aren't aware of how particles systems work
						offsetMinY,              -- but basically they are all ranges of values between which the created particles 
						offsetMinZ,              -- will spawn with or end with, so for example if you specify a minimum x offset of 
						offsetMaxX,              -- -100 and a maximum x offset of +100 then all particles generated will be given a
						offsetMaxY,              -- random starting position within that range of either the player position or an 
						offsetMaxZ,              -- entity position (if using the ParticlesAddEmitterEx variant, see below)
						scaleStartMin,           --
						scaleStartMax,           --
						scaleEndMin,             --
						scaleEndMax,             --
						movementSpeedMinX,       --
						movementSpeedMinY,       --
						movementSpeedMinZ,       --
						movementSpeedMaxX,       --
						movementSpeedMaxY,       --
						movementSpeedMaxZ,       --
						rotateSpeedMinZ,         --
						rotateSpeedMaxZ,         --
						lifeMin,                 --
						lifeMax,                 --
						alphaStartMin,           -- Alpha range is 0 .. 100, i.e. 0 = invisible ... 100 = fully opaque
						alphaStartMax,           --
						alphaEndMin,             --
						alphaEndMax,             --
						frequency                -- frequency with which particles are spawned in milliseconds
					   )
ParticlesAddEmitterEx:  Same as ParticlesAddEmitter with 4 extra parameters (i.e. following the frequency parameter)
						entityid,				 -- particles will be spawned at the location of the given entity ( or -1 )
						limbindex,			     -- particles spawned at limb if applicable ( or 0 )
						particleimage,			 -- specified imageFile value will be used ( see ParticlesLoadImage )
						imageframe               -- for custom images specifies number of frames ( 64, 16 or 4 )					
ParticlesDeleteEmitter: ParticlesDeleteEmitter(emitterId) -- where emitterId is the index of the particle emitter
ParticlesLoadImage:  Example imageFile = ParticlesLoadImage( "effectbank\\particles\\flowerpuff.dds" )
					Allows dynamic loading of custom image files which can then be passed into ParticlesAddEmitterEx
					Note: sprite sheets have to be square, e.g. 2x2 or 6x6 (4 frames and 36 frames respectively), maximum
					size is 64x64 (4096 frames)				
ParticlesLoadEffect: Example effectId = ParticlesLoadEffect( "effectbank\\reloaded\\decal_basic_additive.fx", emitterId )
                    Changes the shader effect used for a given particle emitter					
-- the following commands allow on-the-fly altering of specific particle creation parameters
ParticlesSpawnParticle( emitterId, xPos, yPos, zPos ) -- forces the emitter to spawn a particle at the specified position
ParticlesSetFrames( emitterId, animationSpeed, startFrame, endFrame )
ParticlesSetSpeed( emitterId, movementSpeedMinX, movementSpeedMinY, movementSpeedMinZ,
		                      movementSpeedMaxX, movementSpeedMaxY, movementSpeedMaxZ )
ParticlesSetOffset( emitterId,  offsetMinX, offsetMinY, offsetMinZ,
                                offsetMaxX, offsetMaxY, offsetMaxZ ) 
ParticlesSetScale( emitterId, scaleStartMin, scaleStartMax, scaleEndMin, scaleEndMax )  
ParticlesSetAlpha( emitterId, alphaStartMin, alphaStartMax, alphaEndMin, alphaEndMax )
ParticlesSetAngle( emitterId, xAngle, yAngle, zAngle )  - Euler angles 
-- the '0' parameters below are not yet fully implemented in the engine, basically they are placeholders for the future
ParticlesSetRotation:   ParticlesSetRotation( emitterId, 0, 0, rotateSpeedMinZ, 0, 0, rotateSpeedMaxZ )
ParticlesSetLife:		ParticlesSetLife( emitterId, lifeMin, lifeMax, maxParticles, 0, maxPerFrame ) 
-- maxParticles default is 100, maxPerFrame default is 50
-- This command mimics a basic wind effect, all particles will be effected by this ..
ParticlesSetWindVector: ParticlesSetWindVector( windX, windZ ) -- values are X & Z units per frame
-- unless specifically omitted sith this command
ParticlesSetNoWind( emitterId )  -- All particles created by the specified emitter will not be affected by wind.
-- This command mimics a basic gravity component
ParticlesSetGravity( emitterId, startG, endG ) -- values are Y units per frame
----------------------------------------------

GetGamePlayerControlJetpackMode: GetGamePlayerControlJetpackMode() -- command used by the default player control mechanism
GetGamePlayerControlJetpackFuel: GetGamePlayerControlJetpackFuel() -- command used by the default player control mechanism
GetGamePlayerControlJetpackHidden: GetGamePlayerControlJetpackHidden() -- command used by the default player control mechanism
GetGamePlayerControlJetpackCollected: GetGamePlayerControlJetpackCollected() -- command used by the default player control mechanism
GetGamePlayerControlSoundStartIndex: GetGamePlayerControlSoundStartIndex() -- command used by the default player control mechanism
GetGamePlayerControlJetpackParticleEmitterIndex: GetGamePlayerControlJetpackParticleEmitterIndex() -- uses the OLD particle system
GetGamePlayerControlJetpackThrust: GetGamePlayerControlJetpackThrust() -- command used by the default player control mechanism
GetGamePlayerControlStartStrength: GetGamePlayerControlStartStrength() -- command used by the default player control mechanism
GetGamePlayerControlIsRunning: GetGamePlayerControlIsRunning() -- command used by the default player control mechanism
GetGamePlayerControlFinalCameraAngley: GetGamePlayerControlFinalCameraAngley() -- command used by the default player control mechanism
GetGamePlayerControlCx: GetGamePlayerControlCx() -- command used by the default player control mechanism
GetGamePlayerControlCy: GetGamePlayerControlCy() -- command used by the default player control mechanism
GetGamePlayerControlCz: GetGamePlayerControlCz() -- command used by the default player control mechanism
GetGamePlayerControlBasespeed: GetGamePlayerControlBasespeed() -- command used by the default player control mechanism
GetGamePlayerControlCanRun: GetGamePlayerControlCanRun() -- command used by the default player control mechanism
GetGamePlayerControlMaxspeed: GetGamePlayerControlMaxspeed() -- command used by the default player control mechanism
GetGamePlayerControlTopspeed: GetGamePlayerControlTopspeed() -- command used by the default player control mechanism
GetGamePlayerControlMovement: GetGamePlayerControlMovement() -- command used by the default player control mechanism
GetGamePlayerControlMovey: GetGamePlayerControlMovey() -- command used by the default player control mechanism
GetGamePlayerControlLastMovement: GetGamePlayerControlLastMovement() -- command used by the default player control mechanism
GetGamePlayerControlFootfallCount: GetGamePlayerControlFootfallCount() -- command used by the default player control mechanism
GetGamePlayerControlLastMovement: GetGamePlayerControlLastMovement() -- command used by the default player control mechanism
GetGamePlayerControlGravityActive: GetGamePlayerControlGravityActive() -- command used by the default player control mechanism
GetGamePlayerControlPlrHitFloorMaterial: GetGamePlayerControlPlrHitFloorMaterial() -- command used by the default player control mechanism
GetGamePlayerControlUnderwater: GetGamePlayerControlUnderwater() -- command used by the default player control mechanism
GetGamePlayerControlJumpMode: GetGamePlayerControlJumpMode() -- command used by the default player control mechanism
GetGamePlayerControlJumpModeCanAffectVelocityCountdown: GetGamePlayerControlJumpModeCanAffectVelocityCountdown() -- command used by the default player control mechanism
GetGamePlayerControlSpeed: GetGamePlayerControlSpeed() -- command used by the default player control mechanism
GetGamePlayerControlAccel: GetGamePlayerControlAccel() -- command used by the default player control mechanism
GetGamePlayerControlSpeedRatio: GetGamePlayerControlSpeedRatio() -- command used by the default player control mechanism
GetGamePlayerControlWobble: GetGamePlayerControlWobble() -- command used by the default player control mechanism
GetGamePlayerControlWobbleSpeed: GetGamePlayerControlWobbleSpeed() -- command used by the default player control mechanism
GetGamePlayerControlWobbleHeight: GetGamePlayerControlWobbleHeight() -- command used by the default player control mechanism
GetGamePlayerControlJumpmax: GetGamePlayerControlJumpmax() -- command used by the default player control mechanism
GetGamePlayerControlPushangle: GetGamePlayerControlPushangle() -- command used by the default player control mechanism
GetGamePlayerControlPushforce: GetGamePlayerControlPushforce() -- command used by the default player control mechanism
GetGamePlayerControlFootfallPace : GetGamePlayerControlFootfallPace() -- command used by the default player control mechanism
GetGamePlayerControlFinalCameraAngley: GetGamePlayerControlFinalCameraAngley() -- command used by the default player control mechanism
GetGamePlayerControlLockAtHeight: GetGamePlayerControlLockAtHeight() -- command used by the default player control mechanism
GetGamePlayerControlControlHeight: GetGamePlayerControlControlHeight() -- command used by the default player control mechanism
GetGamePlayerControlControlHeightCooldown: GetGamePlayerControlControlHeightCooldown() -- command used by the default player control mechanism
GetGamePlayerControlStoreMovey: GetGamePlayerControlStoreMovey() -- command used by the default player control mechanism
GetGamePlayerControlPlrHitFloorMaterial: GetGamePlayerControlPlrHitFloorMaterial() -- command used by the default player control mechanism
GetGamePlayerControlHurtFall: GetGamePlayerControlHurtFall() -- command used by the default player control mechanism
GetGamePlayerControlLeanoverAngle: GetGamePlayerControlLeanoverAngle() -- command used by the default player control mechanism
GetGamePlayerControlLeanover: GetGamePlayerControlLeanover() -- command used by the default player control mechanism
GetGamePlayerControlCameraShake: GetGamePlayerControlCameraShake() -- command used by the default player control mechanism
GetGamePlayerControlFinalCameraAnglex: GetGamePlayerControlFinalCameraAnglex() -- command used by the default player control mechanism
GetGamePlayerControlFinalCameraAngley: GetGamePlayerControlFinalCameraAngley() -- command used by the default player control mechanism
GetGamePlayerControlFinalCameraAnglez: GetGamePlayerControlFinalCameraAnglez() -- command used by the default player control mechanism
GetGamePlayerControlCamRightMouseMode: GetGamePlayerControlCamRightMouseMode() -- command used by the default player control mechanism
GetGamePlayerControlCamCollisionSmooth: GetGamePlayerControlCamCollisionSmooth() -- command used by the default player control mechanism
GetGamePlayerControlCamCurrentDistance: GetGamePlayerControlCamCurrentDistance() -- command used by the default player control mechanism
GetGamePlayerControlCamDoFullRayCheck: GetGamePlayerControlCamDoFullRayCheck() -- command used by the default player control mechanism
GetGamePlayerControlLastGoodcx: GetGamePlayerControlLastGoodcx() -- command used by the default player control mechanism
GetGamePlayerControlLastGoodcy: GetGamePlayerControlLastGoodcy() -- command used by the default player control mechanism
GetGamePlayerControlLastGoodcz: GetGamePlayerControlLastGoodcz() -- command used by the default player control mechanism
GetGamePlayerControlCamDoFullRayCheck: GetGamePlayerControlCamDoFullRayCheck() -- command used by the default player control mechanism
GetGamePlayerControlFlinchx: GetGamePlayerControlFlinchx() -- command used by the default player control mechanism
GetGamePlayerControlFlinchy: GetGamePlayerControlFlinchy() -- command used by the default player control mechanism
GetGamePlayerControlFlinchz: GetGamePlayerControlFlinchz() -- command used by the default player control mechanism
GetGamePlayerControlFlinchCurrentx: GetGamePlayerControlFlinchCurrentx() -- command used by the default player control mechanism
GetGamePlayerControlFlinchCurrenty: GetGamePlayerControlFlinchCurrenty() -- command used by the default player control mechanism
GetGamePlayerControlFlinchCurrentz: GetGamePlayerControlFlinchCurrentz() -- command used by the default player control mechanism
GetGamePlayerControlFootfallType: GetGamePlayerControlFootfallType() -- command used by the default player control mechanism
GetGamePlayerControlRippleCount: GetGamePlayerControlRippleCount() -- command used by the default player control mechanism
GetGamePlayerControlLastFootfallSound: GetGamePlayerControlLastFootfallSound() -- command used by the default player control mechanism
GetGamePlayerControlInWaterState: GetGamePlayerControlInWaterState() -- command used by the default player control mechanism
GetGamePlayerControlDrownTimestamp: GetGamePlayerControlDrownTimestamp() -- command used by the default player control mechanism
GetGamePlayerControlDeadTime: GetGamePlayerControlDeadTime() -- command used by the default player control mechanism
GetGamePlayerControlSwimTimestamp: GetGamePlayerControlSwimTimestamp() -- command used by the default player control mechanism
GetGamePlayerControlRedDeathFog: GetGamePlayerControlRedDeathFog() -- command used by the default player control mechanism
GetGamePlayerControlHeartbeatTimeStamp: GetGamePlayerControlHeartbeatTimeStamp() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonEnabled: GetGamePlayerControlThirdpersonEnabled() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCharacterIndex: GetGamePlayerControlThirdpersonCharacterIndex() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraFollow: GetGamePlayerControlThirdpersonCameraFollow() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraFocus: GetGamePlayerControlThirdpersonCameraFocus() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCharactere: GetGamePlayerControlThirdpersonCharactere() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraFollow: GetGamePlayerControlThirdpersonCameraFollow() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonShotFired: GetGamePlayerControlThirdpersonShotFired() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraDistance: GetGamePlayerControlThirdpersonCameraDistance() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraSpeed: GetGamePlayerControlThirdpersonCameraSpeed() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraLocked: GetGamePlayerControlThirdpersonCameraLocked() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraHeight: GetGamePlayerControlThirdpersonCameraHeight() -- command used by the default player control mechanism
GetGamePlayerControlThirdpersonCameraShoulder: GetGamePlayerControlThirdpersonCameraShoulder() -- command used by the default player control mechanism
SetGamePlayerControlJetpackMode: SetGamePlayerControlJetpackMode() -- command used by the default player control mechanism
SetGamePlayerControlJetpackFuel: SetGamePlayerControlJetpackFuel() -- command used by the default player control mechanism
SetGamePlayerControlJetpackHidden: SetGamePlayerControlJetpackHidden() -- command used by the default player control mechanism
SetGamePlayerControlJetpackCollected: SetGamePlayerControlJetpackCollected() -- command used by the default player control mechanism
SetGamePlayerControlSoundStartIndex: SetGamePlayerControlSoundStartIndex() -- command used by the default player control mechanism
SetGamePlayerControlJetpackParticleEmitterIndex: SetGamePlayerControlJetpackParticleEmitterIndex() -- uses the OLD particle system
SetGamePlayerControlJetpackThrust: SetGamePlayerControlJetpackThrust() -- command used by the default player control mechanism
SetGamePlayerControlStartStrength: SetGamePlayerControlStartStrength() -- command used by the default player control mechanism
SetGamePlayerControlIsRunning: SetGamePlayerControlIsRunning() -- command used by the default player control mechanism
SetGamePlayerControlFinalCameraAngley: SetGamePlayerControlFinalCameraAngley() -- command used by the default player control mechanism
SetGamePlayerControlCx: SetGamePlayerControlCx() -- command used by the default player control mechanism
SetGamePlayerControlCy: SetGamePlayerControlCy() -- command used by the default player control mechanism
SetGamePlayerControlCz: SetGamePlayerControlCz() -- command used by the default player control mechanism
SetGamePlayerControlBasespeed: SetGamePlayerControlBasespeed() -- command used by the default player control mechanism
SetGamePlayerControlCanRun: SetGamePlayerControlCanRun() -- command used by the default player control mechanism
SetGamePlayerControlMaxspeed: SetGamePlayerControlMaxspeed() -- command used by the default player control mechanism
SetGamePlayerControlTopspeed: SetGamePlayerControlTopspeed() -- command used by the default player control mechanism
SetGamePlayerControlMovement: SetGamePlayerControlMovement() -- command used by the default player control mechanism
SetGamePlayerControlMovey: SetGamePlayerControlMovey() -- command used by the default player control mechanism
SetGamePlayerControlLastMovement: SetGamePlayerControlLastMovement() -- command used by the default player control mechanism
SetGamePlayerControlFootfallCount: SetGamePlayerControlFootfallCount() -- command used by the default player control mechanism
SetGamePlayerControlLastMovement: SetGamePlayerControlLastMovement() -- command used by the default player control mechanism
SetGamePlayerControlGravityActive: SetGamePlayerControlGravityActive() -- command used by the default player control mechanism
SetGamePlayerControlPlrHitFloorMaterial: SetGamePlayerControlPlrHitFloorMaterial() -- command used by the default player control mechanism
SetGamePlayerControlUnderwater: SetGamePlayerControlUnderwater() -- command used by the default player control mechanism
SetGamePlayerControlJumpMode: SetGamePlayerControlJumpMode() -- command used by the default player control mechanism
SetGamePlayerControlJumpModeCanAffectVelocityCountdown: SetGamePlayerControlJumpModeCanAffectVelocityCountdown() -- command used by the default player control mechanism
SetGamePlayerControlSpeed: SetGamePlayerControlSpeed() -- command used by the default player control mechanism
SetGamePlayerControlAccel: SetGamePlayerControlAccel() -- command used by the default player control mechanism
SetGamePlayerControlSpeedRatio: SetGamePlayerControlSpeedRatio() -- command used by the default player control mechanism
SetGamePlayerControlWobble: SetGamePlayerControlWobble() -- command used by the default player control mechanism
SetGamePlayerControlWobbleSpeed: SetGamePlayerControlWobbleSpeed() -- command used by the default player control mechanism
SetGamePlayerControlWobbleHeight: SetGamePlayerControlWobbleHeight() -- command used by the default player control mechanism
SetGamePlayerControlJumpmax: SetGamePlayerControlJumpmax() -- command used by the default player control mechanism
SetGamePlayerControlPushangle: SetGamePlayerControlPushangle() -- command used by the default player control mechanism
SetGamePlayerControlPushforce: SetGamePlayerControlPushforce() -- command used by the default player control mechanism
SetGamePlayerControlFootfallPace: SetGamePlayerControlFootfallPace() -- command used by the default player control mechanism
SetGamePlayerControlFinalCameraAngley: SetGamePlayerControlFinalCameraAngley() -- command used by the default player control mechanism
SetGamePlayerControlLockAtHeight: SetGamePlayerControlLockAtHeight() -- command used by the default player control mechanism
SetGamePlayerControlControlHeight: SetGamePlayerControlControlHeight() -- command used by the default player control mechanism
SetGamePlayerControlControlHeightCooldown: SetGamePlayerControlControlHeightCooldown() -- command used by the default player control mechanism
SetGamePlayerControlStoreMovey: SetGamePlayerControlStoreMovey() -- command used by the default player control mechanism
SetGamePlayerControlPlrHitFloorMaterial: SetGamePlayerControlPlrHitFloorMaterial() -- command used by the default player control mechanism
SetGamePlayerControlHurtFall: SetGamePlayerControlHurtFall() -- command used by the default player control mechanism
SetGamePlayerControlLeanoverAngle: SetGamePlayerControlLeanoverAngle() -- command used by the default player control mechanism
SetGamePlayerControlLeanover: SetGamePlayerControlLeanover() -- command used by the default player control mechanism
SetGamePlayerControlCameraShake: SetGamePlayerControlCameraShake() -- command used by the default player control mechanism
SetGamePlayerControlFinalCameraAnglex: SetGamePlayerControlFinalCameraAnglex() -- command used by the default player control mechanism
SetGamePlayerControlFinalCameraAngley: SetGamePlayerControlFinalCameraAngley() -- command used by the default player control mechanism
SetGamePlayerControlFinalCameraAnglez: SetGamePlayerControlFinalCameraAnglez() -- command used by the default player control mechanism
SetGamePlayerControlCamRightMouseMode: SetGamePlayerControlCamRightMouseMode() -- command used by the default player control mechanism
SetGamePlayerControlCamCollisionSmooth: SetGamePlayerControlCamCollisionSmooth() -- command used by the default player control mechanism
SetGamePlayerControlCamCurrentDistance: SetGamePlayerControlCamCurrentDistance() -- command used by the default player control mechanism
SetGamePlayerControlCamDoFullRayCheck: SetGamePlayerControlCamDoFullRayCheck() -- command used by the default player control mechanism
SetGamePlayerControlLastGoodcx: SetGamePlayerControlLastGoodcx() -- command used by the default player control mechanism
SetGamePlayerControlLastGoodcy: SetGamePlayerControlLastGoodcy() -- command used by the default player control mechanism
SetGamePlayerControlLastGoodcz: SetGamePlayerControlLastGoodcz() -- command used by the default player control mechanism
SetGamePlayerControlCamDoFullRayCheck: SetGamePlayerControlCamDoFullRayCheck() -- command used by the default player control mechanism
SetGamePlayerControlFlinchx: SetGamePlayerControlFlinchx() -- command used by the default player control mechanism
SetGamePlayerControlFlinchy: SetGamePlayerControlFlinchy() -- command used by the default player control mechanism
SetGamePlayerControlFlinchz: SetGamePlayerControlFlinchz() -- command used by the default player control mechanism
SetGamePlayerControlFlinchCurrentx: SetGamePlayerControlFlinchCurrentx() -- command used by the default player control mechanism
SetGamePlayerControlFlinchCurrenty: SetGamePlayerControlFlinchCurrenty() -- command used by the default player control mechanism
SetGamePlayerControlFlinchCurrentz: SetGamePlayerControlFlinchCurrentz() -- command used by the default player control mechanism
SetGamePlayerControlFootfallType: SetGamePlayerControlFootfallType() -- command used by the default player control mechanism
SetGamePlayerControlRippleCount: SetGamePlayerControlRippleCount() -- command used by the default player control mechanism
SetGamePlayerControlLastFootfallSound: SetGamePlayerControlLastFootfallSound() -- command used by the default player control mechanism
SetGamePlayerControlInWaterState: SetGamePlayerControlInWaterState() -- command used by the default player control mechanism
SetGamePlayerControlDrownTimestamp: SetGamePlayerControlDrownTimestamp() -- command used by the default player control mechanism
SetGamePlayerControlDeadTime: SetGamePlayerControlDeadTime() -- command used by the default player control mechanism
SetGamePlayerControlSwimTimestamp: SetGamePlayerControlSwimTimestamp() -- command used by the default player control mechanism
SetGamePlayerControlRedDeathFog: SetGamePlayerControlRedDeathFog() -- command used by the default player control mechanism
SetGamePlayerControlHeartbeatTimeStamp: SetGamePlayerControlHeartbeatTimeStamp() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonEnabled: SetGamePlayerControlThirdpersonEnabled() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCharacterIndex: SetGamePlayerControlThirdpersonCharacterIndex() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraFollow: SetGamePlayerControlThirdpersonCameraFollow() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraFocus: SetGamePlayerControlThirdpersonCameraFocus() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCharactere: SetGamePlayerControlThirdpersonCharactere() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraFollow: SetGamePlayerControlThirdpersonCameraFollow() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonShotFired: SetGamePlayerControlThirdpersonShotFired() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraDistance: SetGamePlayerControlThirdpersonCameraDistance() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraSpeed: SetGamePlayerControlThirdpersonCameraSpeed() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraLocked: SetGamePlayerControlThirdpersonCameraLocked() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraHeight: SetGamePlayerControlThirdpersonCameraHeight() -- command used by the default player control mechanism
SetGamePlayerControlThirdpersonCameraShoulder: SetGamePlayerControlThirdpersonCameraShoulder() -- command used by the default player control mechanism
SetGamePlayerStateGunMode: SetGamePlayerStateGunMode() -- command used by the default player control mechanism
GetGamePlayerStateGunMode: GetGamePlayerStateGunMode() -- command used by the default player control mechanism
SetGamePlayerStateFiringMode: SetGamePlayerStateFiringMode() -- command used by the default player control mechanism
GetGamePlayerStateFiringMode: GetGamePlayerStateFiringMode() -- returns a value of 1 when the LMB is pressed, even if the player has not got any weapons
GetGamePlayerStateWeaponAmmoIndex: GetGamePlayerStateWeaponAmmoIndex() -- command used by the default player control mechanism
GetGamePlayerStateAmmoOffset: GetGamePlayerStateAmmoOffset() -- command used by the default player control mechanism
GetGamePlayerStateGunMeleeKey: GetGamePlayerStateGunMeleeKey() -- command used by the default player control mechanism
GetGamePlayerStateBlockingAction: GetGamePlayerStateBlockingAction() -- command used by the default player control mechanism
SetGamePlayerStateGunShootNoAmmo: SetGamePlayerStateGunShootNoAmmo() -- command used by the default player control mechanism
GetGamePlayerStateGunShootNoAmmo: GetGamePlayerStateGunShootNoAmmo() -- command used by the default player control mechanism
SetGamePlayerStateUnderwater: SetGamePlayerStateUnderwater() -- command used by the default player control mechanism
GetGamePlayerStateUnderwater: GetGamePlayerStateUnderwater() -- command used by the default player control mechanism
SetGamePlayerStateRightMouseHold: SetGamePlayerStateRightMouseHold() -- command used by the default player control mechanism
GetGamePlayerStateRightMouseHold: GetGamePlayerStateRightMouseHold() -- command used by the default player control mechanism
SetGamePlayerStateXBOX: SetGamePlayerStateXBOX() -- command used by the default player control mechanism
GetGamePlayerStateXBOX: GetGamePlayerStateXBOX() -- command used by the default player control mechanism
JoystickX: JoystickX() -- returns a value between -1000 and +1000 representing X axis of controller
JoystickY: JoystickY() -- returns a value between -1000 and +1000 representing Y axis of controller
JoystickZ: JoystickZ() -- returns a value between -1000 and +1000 representing Trigger of controller
SetGamePlayerStateGunZoomMode: SetGamePlayerStateGunZoomMode() -- command used by the default player control mechanism
GetGamePlayerStateGunZoomMode: GetGamePlayerStateGunZoomMode() -- command used by the default player control mechanism
SetGamePlayerStateGunZoomMag: SetGamePlayerStateGunZoomMag() -- command used by the default player control mechanism
GetGamePlayerStateGunZoomMag: GetGamePlayerStateGunZoomMag() -- command used by the default player control mechanism
SetGamePlayerStateGunReloadNoAmmo: SetGamePlayerStateGunReloadNoAmmo() -- command used by the default player control mechanism
GetGamePlayerStateGunReloadNoAmmo: GetGamePlayerStateGunReloadNoAmmo() -- command used by the default player control mechanism
SetGamePlayerStatePlrReloading: SetGamePlayerStatePlrReloading() -- command used by the default player control mechanism
GetGamePlayerStatePlrReloading: GetGamePlayerStatePlrReloading() -- command used by the default player control mechanism
SetGamePlayerStateGunAltSwapKey1: SetGamePlayerStateGunAltSwapKey1() -- command used by the default player control mechanism
GetGamePlayerStateGunAltSwapKey1: GetGamePlayerStateGunAltSwapKey1() -- command used by the default player control mechanism
SetGamePlayerStateGunAltSwapKey2: SetGamePlayerStateGunAltSwapKey2() -- command used by the default player control mechanism
GetGamePlayerStateGunAltSwapKey2: GetGamePlayerStateGunAltSwapKey2() -- command used by the default player control mechanism
SetGamePlayerStateWeaponKeySelection: SetGamePlayerStateWeaponKeySelection() -- command used by the default player control mechanism
GetGamePlayerStateWeaponKeySelection: GetGamePlayerStateWeaponKeySelection() -- command used by the default player control mechanism
SetGamePlayerStateWeaponIndex: SetGamePlayerStateWeaponIndex() -- command used by the default player control mechanism
GetGamePlayerStateWeaponIndex: GetGamePlayerStateWeaponIndex() -- command used by the default player control mechanism
SetGamePlayerStateCommandNewWeapon: SetGamePlayerStateCommandNewWeapon() -- command used by the default player control mechanism
GetGamePlayerStateCommandNewWeapon: GetGamePlayerStateCommandNewWeapon() -- command used by the default player control mechanism
SetGamePlayerStateGunID: SetGamePlayerStateGunID() -- command used by the default player control mechanism
GetGamePlayerStateGunID: GetGamePlayerStateGunID() -- command used by the default player control mechanism
SetGamePlayerStateGunSelectionAfterHide: SetGamePlayerStateGunSelectionAfterHide() -- command used by the default player control mechanism
GetGamePlayerStateGunSelectionAfterHide: GetGamePlayerStateGunSelectionAfterHide() -- command used by the default player control mechanism
SetGamePlayerStateGunBurst: SetGamePlayerStateGunBurst() -- command used by the default player control mechanism
GetGamePlayerStateGunBurst: GetGamePlayerStateGunBurst() -- command used by the default player control mechanism
SetGamePlayerStatePlrKeyForceKeystate: SetGamePlayerStatePlrKeyForceKeystate(keystate) -- inject a keystate value into system as though user pressed physical key
JoystickHatAngle: JoystickHatAngle() -- command used by the default player control mechanism
JoystickFireXL: JoystickFireXL() -- command used by the default player control mechanism
JoystickTwistX: JoystickTwistX() -- command used by the default player control mechanism
JoystickTwistY: JoystickTwistY() -- command used by the default player control mechanism
JoystickTwistZ: JoystickTwistZ() -- command used by the default player control mechanism
SetGamePlayerStatePlrZoomInChange: SetGamePlayerStatePlrZoomInChange() -- command used by the default player control mechanism
GetGamePlayerStatePlrZoomInChange: GetGamePlayerStatePlrZoomInChange() -- command used by the default player control mechanism
SetGamePlayerStatePlrZoomIn: SetGamePlayerStatePlrZoomIn() -- command used by the default player control mechanism
GetGamePlayerStatePlrZoomIn: GetGamePlayerStatePlrZoomIn() -- command used by the default player control mechanism
SetGamePlayerStateLuaActiveMouse: SetGamePlayerStateLuaActiveMouse() -- command used by the default player control mechanism
GetGamePlayerStateLuaActiveMouse: GetGamePlayerStateLuaActiveMouse() -- command used by the default player control mechanism
SetGamePlayerStateRealFov: SetGamePlayerStateRealFov() -- command used by the default player control mechanism
GetGamePlayerStateRealFov: GetGamePlayerStateRealFov() -- command used by the default player control mechanism
GetPlayerFOV: fov = GetPlayerFOV() -- command used to retrieve the latest player FOV (previously set by SetPlayerFOV)
SetGamePlayerStateDisablePeeking: SetGamePlayerStateDisablePeeking() -- command used by the default player control mechanism
GetGamePlayerStateDisablePeeking: GetGamePlayerStateDisablePeeking() -- command used by the default player control mechanism
SetGamePlayerStatePlrHasFocus: SetGamePlayerStatePlrHasFocus() -- command used by the default player control mechanism
GetGamePlayerStatePlrHasFocus: GetGamePlayerStatePlrHasFocus() -- command used by the default player control mechanism
SetGamePlayerStateGameRunAsMultiplayer: SetGamePlayerStateGameRunAsMultiplayer() -- command used by the default player control mechanism
GetGamePlayerStateGameRunAsMultiplayer: GetGamePlayerStateGameRunAsMultiplayer() -- command used by the default player control mechanism
SetGamePlayerStateSteamWorksRespawnLeft: SetGamePlayerStateSteamWorksRespawnLeft() -- command used by the default player control mechanism
GetGamePlayerStateSteamWorksRespawnLeft: GetGamePlayerStateSteamWorksRespawnLeft() -- command used by the default player control mechanism
SetGamePlayerStateTabMode: SetGamePlayerStateTabMode() -- command used by the default player control mechanism
GetGamePlayerStateTabMode: GetGamePlayerStateTabMode() -- command used by the default player control mechanism
SetGamePlayerStateLowFpsWarning: SetGamePlayerStateLowFpsWarning() -- command used by the default player control mechanism
GetGamePlayerStateLowFpsWarning: GetGamePlayerStateLowFpsWarning() -- command used by the default player control mechanism
SetGamePlayerStateCameraFov: SetGamePlayerStateCameraFov() -- command used by the default player control mechanism
GetGamePlayerStateCameraFov: GetGamePlayerStateCameraFov() -- command used by the default player control mechanism
SetGamePlayerStateCameraFovZoomed: SetGamePlayerStateCameraFovZoomed() -- command used by the default player control mechanism
GetGamePlayerStateCameraFovZoomed: GetGamePlayerStateCameraFovZoomed() -- command used by the default player control mechanism
SetGamePlayerStateMouseInvert: SetGamePlayerStateMouseInvert() -- command used by the default player control mechanism
GetGamePlayerStateMouseInvert: GetGamePlayerStateMouseInvert() -- command used by the default player control mechanism
SetGamePlayerStateSlowMotion: SetGamePlayerStateSlowMotion() -- command used by the default player control mechanism
GetGamePlayerStateSlowMotion: GetGamePlayerStateSlowMotion() -- command used by the default player control mechanism
SetGamePlayerStateSmoothCameraKeys: SetGamePlayerStateSmoothCameraKeys() -- command used by the default player control mechanism
GetGamePlayerStateSmoothCameraKeys: GetGamePlayerStateSmoothCameraKeys() -- command used by the default player control mechanism
SetGamePlayerStateCamMouseMoveX: SetGamePlayerStateCamMouseMoveX() -- command used by the default player control mechanism
GetGamePlayerStateCamMouseMoveX: GetGamePlayerStateCamMouseMoveX() -- command used by the default player control mechanism
SetGamePlayerStateCamMouseMoveY: SetGamePlayerStateCamMouseMoveY() -- command used by the default player control mechanism
GetGamePlayerStateCamMouseMoveY: GetGamePlayerStateCamMouseMoveY() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilX: SetGamePlayerStateGunRecoilX() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilX: GetGamePlayerStateGunRecoilX() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilY: SetGamePlayerStateGunRecoilY() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilY: GetGamePlayerStateGunRecoilY() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilAngleX: SetGamePlayerStateGunRecoilAngleX() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilAngleX: GetGamePlayerStateGunRecoilAngleX() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilAngleY: SetGamePlayerStateGunRecoilAngleY() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilAngleY: GetGamePlayerStateGunRecoilAngleY() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilCorrectY: SetGamePlayerStateGunRecoilCorrectY() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilCorrectY: GetGamePlayerStateGunRecoilCorrectY() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilCorrectX: SetGamePlayerStateGunRecoilCorrectX() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilCorrectX: GetGamePlayerStateGunRecoilCorrectX() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilCorrectAngleY: SetGamePlayerStateGunRecoilCorrectAngleY() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilCorrectAngleY: GetGamePlayerStateGunRecoilCorrectAngleY() -- command used by the default player control mechanism
SetGamePlayerStateGunRecoilCorrectAngleX: SetGamePlayerStateGunRecoilCorrectAngleX() -- command used by the default player control mechanism
GetGamePlayerStateGunRecoilCorrectAngleX: GetGamePlayerStateGunRecoilCorrectAngleX() -- command used by the default player control mechanism
SetGamePlayerStateCamAngleX: SetGamePlayerStateCamAngleX() -- command used by the default player control mechanism
GetGamePlayerStateCamAngleX: GetGamePlayerStateCamAngleX() -- command used by the default player control mechanism
SetGamePlayerStateCamAngleY: SetGamePlayerStateCamAngleY() -- command used by the default player control mechanism
GetGamePlayerStateCamAngleY: GetGamePlayerStateCamAngleY() -- command used by the default player control mechanism
SetGamePlayerStatePlayerDucking: SetGamePlayerStatePlayerDucking() -- command used by the default player control mechanism
GetGamePlayerStatePlayerDucking: GetGamePlayerStatePlayerDucking() -- command used by the default player control mechanism
SetGamePlayerStateEditModeActive: SetGamePlayerStateEditModeActive() -- command used by the default player control mechanism
GetGamePlayerStateEditModeActive: GetGamePlayerStateEditModeActive() -- command used by the default player control mechanism
SetGamePlayerStatePlrKeyShift: SetGamePlayerStatePlrKeyShift() -- command used by the default player control mechanism
GetGamePlayerStatePlrKeyShift: GetGamePlayerStatePlrKeyShift() -- command used by the default player control mechanism
SetGamePlayerStatePlrKeyShift2: SetGamePlayerStatePlrKeyShift2() -- command used by the default player control mechanism
GetGamePlayerStatePlrKeyShift2: GetGamePlayerStatePlrKeyShift2() -- command used by the default player control mechanism
SetGamePlayerStatePlrKeyControl: SetGamePlayerStatePlrKeyControl() -- command used by the default player control mechanism
GetGamePlayerStatePlrKeyControl: GetGamePlayerStatePlrKeyControl() -- command used by the default player control mechanism
SetGamePlayerStateNoWater: SetGamePlayerStateNoWater() -- command used by the default player control mechanism
GetGamePlayerStateNoWater: GetGamePlayerStateNoWater() -- command used by the default player control mechanism
SetGamePlayerStateWaterlineY: SetGamePlayerStateWaterlineY() -- command used by the default player control mechanism
GetGamePlayerStateWaterlineY: GetGamePlayerStateWaterlineY() -- command used by the default player control mechanism

SetGamePlayerStateFlashlightKeyEnabled: SetGamePlayerStateFlashlightKeyEnabled() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightKeyEnabled: GetGamePlayerStateFlashlightKeyEnabled() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightControl: SetGamePlayerStateFlashlightControl() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightControl: GetGamePlayerStateFlashlightControl() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightRange: SetGamePlayerStateFlashlightRange() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightRange: GetGamePlayerStateFlashlightRange() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightRadius: SetGamePlayerStateFlashlightRadius() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightRadius: GetGamePlayerStateFlashlightRadius() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightColorR: SetGamePlayerStateFlashlightColorR() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightColorR: GetGamePlayerStateFlashlightColorR() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightColorG: SetGamePlayerStateFlashlightColorG() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightColorG: GetGamePlayerStateFlashlightColorG() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightColorB: SetGamePlayerStateFlashlightColorB() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightColorB: GetGamePlayerStateFlashlightColorB() -- command used by the default player control mechanism
SetGamePlayerStateFlashlightCastShadow: SetGamePlayerStateFlashlightCastShadow() -- command used by the default player control mechanism
GetGamePlayerStateFlashlightCastShadow: GetGamePlayerStateFlashlightCastShadow() -- command used by the default player control mechanism

SetGamePlayerStateMoving: SetGamePlayerStateMoving() -- command used by the default player control mechanism
GetGamePlayerStateMoving: GetGamePlayerStateMoving() -- command used by the default player control mechanism
SetGamePlayerStateTerrainHeight: SetGamePlayerStateTerrainHeight() -- command used by the default player control mechanism
GetGamePlayerStateTerrainHeight: GetGamePlayerStateTerrainHeight() -- command used by the default player control mechanism
SetGamePlayerStateJetpackVerticalMove: SetGamePlayerStateJetpackVerticalMove() -- command used by the default player control mechanism
GetGamePlayerStateJetpackVerticalMove: GetGamePlayerStateJetpackVerticalMove() -- command used by the default player control mechanism
SetGamePlayerStateTerrainID: SetGamePlayerStateTerrainID() -- command used by the default player control mechanism
GetGamePlayerStateTerrainID: GetGamePlayerStateTerrainID() -- command used by the default player control mechanism
SetGamePlayerStateEnablePlrSpeedMods: SetGamePlayerStateEnablePlrSpeedMods() -- command used by the default player control mechanism
GetGamePlayerStateEnablePlrSpeedMods: GetGamePlayerStateEnablePlrSpeedMods() -- command used by the default player control mechanism
SetGamePlayerStateRiftMode: SetGamePlayerStateRiftMode() -- command used by the default player control mechanism
GetGamePlayerStateRiftMode: GetGamePlayerStateRiftMode() -- command used by the default player control mechanism
SetGamePlayerStatePlayerX: SetGamePlayerStatePlayerX() -- command used by the default player control mechanism
GetGamePlayerStatePlayerX: GetGamePlayerStatePlayerX() -- command used by the default player control mechanism
SetGamePlayerStatePlayerY: SetGamePlayerStatePlayerY() -- command used by the default player control mechanism
GetGamePlayerStatePlayerY: GetGamePlayerStatePlayerY() -- command used by the default player control mechanism
SetGamePlayerStatePlayerZ: SetGamePlayerStatePlayerZ() -- command used by the default player control mechanism
GetGamePlayerStatePlayerZ: GetGamePlayerStatePlayerZ() -- command used by the default player control mechanism
SetGamePlayerStateTerrainPlayerX: SetGamePlayerStateTerrainPlayerX() -- command used by the default player control mechanism
GetGamePlayerStateTerrainPlayerX: GetGamePlayerStateTerrainPlayerX() -- command used by the default player control mechanism
SetGamePlayerStateTerrainPlayerY: SetGamePlayerStateTerrainPlayerY() -- command used by the default player control mechanism
GetGamePlayerStateTerrainPlayerY: GetGamePlayerStateTerrainPlayerY() -- command used by the default player control mechanism
SetGamePlayerStateTerrainPlayerZ: SetGamePlayerStateTerrainPlayerZ() -- command used by the default player control mechanism
GetGamePlayerStateTerrainPlayerZ: GetGamePlayerStateTerrainPlayerZ() -- command used by the default player control mechanism
SetGamePlayerStateTerrainPlayerAX: SetGamePlayerStateTerrainPlayerAX() -- command used by the default player control mechanism
GetGamePlayerStateTerrainPlayerAX: GetGamePlayerStateTerrainPlayerAX() -- command used by the default player control mechanism
SetGamePlayerStateTerrainPlayerAY: SetGamePlayerStateTerrainPlayerAY() -- command used by the default player control mechanism
GetGamePlayerStateTerrainPlayerAY: GetGamePlayerStateTerrainPlayerAY() -- command used by the default player control mechanism
SetGamePlayerStateTerrainPlayerAZ: SetGamePlayerStateTerrainPlayerAZ() -- command used by the default player control mechanism
GetGamePlayerStateTerrainPlayerAZ: GetGamePlayerStateTerrainPlayerAZ() -- command used by the default player control mechanism
SetGamePlayerStateAdjustBasedOnWobbleY: SetGamePlayerStateAdjustBasedOnWobbleY() -- command used by the default player control mechanism
GetGamePlayerStateAdjustBasedOnWobbleY: GetGamePlayerStateAdjustBasedOnWobbleY() -- command used by the default player control mechanism
SetGamePlayerStateFinalCamX: SetGamePlayerStateFinalCamX() -- command used by the default player control mechanism
GetGamePlayerStateFinalCamX: GetGamePlayerStateFinalCamX() -- command used by the default player control mechanism
SetGamePlayerStateFinalCamY: SetGamePlayerStateFinalCamY() -- command used by the default player control mechanism
GetGamePlayerStateFinalCamY: GetGamePlayerStateFinalCamY() -- command used by the default player control mechanism
SetGamePlayerStateFinalCamZ: SetGamePlayerStateFinalCamZ() -- command used by the default player control mechanism
GetGamePlayerStateFinalCamZ: GetGamePlayerStateFinalCamZ() -- command used by the default player control mechanism
SetGamePlayerStateShakeX: SetGamePlayerStateShakeX() -- command used by the default player control mechanism
GetGamePlayerStateShakeX: GetGamePlayerStateShakeX() -- command used by the default player control mechanism
SetGamePlayerStateShakeY: SetGamePlayerStateShakeY() -- command used by the default player control mechanism
GetGamePlayerStateShakeY: GetGamePlayerStateShakeY() -- command used by the default player control mechanism
SetGamePlayerStateShakeZ: SetGamePlayerStateShakeZ() -- command used by the default player control mechanism
GetGamePlayerStateShakeZ: GetGamePlayerStateShakeZ() -- command used by the default player control mechanism
SetGamePlayerStateImmunity: SetGamePlayerStateImmunity() -- command used by the default player control mechanism
GetGamePlayerStateImmunity: GetGamePlayerStateImmunity() -- command used by the default player control mechanism
SetGamePlayerStateCharAnimIndex: SetGamePlayerStateCharAnimIndex() -- command used by the default player control mechanism
GetGamePlayerStateCharAnimIndex: GetGamePlayerStateCharAnimIndex() -- command used by the default player control mechanism

SetGamePlayerStateIsMelee: SetGamePlayerStateIsMelee() -- by default sets current weapon, can specify optional gunid and firemode
GetGamePlayerStateIsMelee: GetGamePlayerStateIsMelee() -- command used by the default player control mechanism
SetGamePlayerStateAlternate: SetGamePlayerStateAlternate() -- by default sets current weapon, can specify optional gunid and firemode
GetGamePlayerStateAlternate: GetGamePlayerStateAlternate() -- command used by the default player control mechanism
SetGamePlayerStateModeShareMags: SetGamePlayerStateModeShareMags() -- by default sets current weapon, can specify optional gunid and firemode
GetGamePlayerStateModeShareMags: GetGamePlayerStateModeShareMags() -- command used by the default player control mechanism
SetGamePlayerStateAlternateIsFlak: SetGamePlayerStateAlternateIsFlak() -- by default sets current weapon, can specify optional gunid and firemode
GetGamePlayerStateAlternateIsFlak: GetGamePlayerStateAlternateIsFlak() -- command used by the default player control mechanism
SetGamePlayerStateAlternateIsRay: SetGamePlayerStateAlternateIsRay() -- by default sets current weapon, can specify optional gunid and firemode
GetGamePlayerStateAlternateIsRay: GetGamePlayerStateAlternateIsRay() -- command used by the default player control mechanism
SetFireModeSettingsReloadQty: SetFireModeSettingsReloadQty() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsReloadQty: GetFireModeSettingsReloadQty() -- command used by the default player control mechanism
SetFireModeSettingsIsEmpty: SetFireModeSettingsIsEmpty() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsIsEmpty: GetFireModeSettingsIsEmpty() -- command used by the default player control mechanism
SetFireModeSettingsJammed: SetFireModeSettingsJammed() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsJammed: GetFireModeSettingsJammed() -- command used by the default player control mechanism
SetFireModeSettingsJamChance: SetFireModeSettingsJamChance() -- by default sets current weapon, can specify optional gunid and firemodem
GetFireModeSettingsJamChance: GetFireModeSettingsJamChance() -- command used by the default player control mechanism
SetFireModeSettingsMinTimer: SetFireModeSettingsMinTimer() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsMinTimer: GetFireModeSettingsMinTimer() -- command used by the default player control mechanism
SetFireModeSettingsAddTimer: SetFireModeSettingsAddTimer() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsAddTimer: GetFireModeSettingsAddTimer() -- command used by the default player control mechanism
SetFireModeSettingsShotsFired: SetFireModeSettingsShotsFired() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsShotsFired: GetFireModeSettingsShotsFired() -- command used by the default player control mechanism
SetFireModeSettingsCoolTimer: SetFireModeSettingsCoolTimer() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsCoolTimer: GetFireModeSettingsCoolTimer() -- command used by the default player control mechanism
SetFireModeSettingsOverheatAfter: SetFireModeSettingsOverheatAfter() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsOverheatAfter: GetFireModeSettingsOverheatAfter() -- command used by the default player control mechanism
SetFireModeSettingsJamChanceTime: SetFireModeSettingsJamChanceTime() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsJamChanceTime: GetFireModeSettingsJamChanceTime() -- command used by the default player control mechanism
SetFireModeSettingsCoolDown: SetFireModeSettingsCoolDown() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsCoolDown: GetFireModeSettingsCoolDown() -- command used by the default player control mechanism
SetFireModeSettingsNoSubmergedFire: SetFireModeSettingsNoSubmergedFire() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsNoSubmergedFire: GetFireModeSettingsNoSubmergedFire() -- command used by the default player control mechanism
SetFireModeSettingsSimpleZoom: SetFireModeSettingsSimpleZoom() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsSimpleZoom: GetFireModeSettingsSimpleZoom() -- command used by the default player control mechanism
SetFireModeSettingsForceZoomOut: SetFireModeSettingsForceZoomOut() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsForceZoomOut: GetFireModeSettingsForceZoomOut() -- command used by the default player control mechanism
SetFireModeSettingsZoomMode: SetFireModeSettingsZoomMode() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsZoomMode: GetFireModeSettingsZoomMode() -- command used by the default player control mechanism
SetFireModeSettingsSimpleZoomAnim: SetFireModeSettingsSimpleZoomAnim() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsSimpleZoomAnim: GetFireModeSettingsSimpleZoomAnim() -- command used by the default player control mechanism
SetFireModeSettingsPoolIndex: SetFireModeSettingsPoolIndex() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsPoolIndex: GetFireModeSettingsPoolIndex() -- command used by the default player control mechanism
SetFireModeSettingsPlrTurnSpeedMod: SetFireModeSettingsPlrTurnSpeedMod() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsPlrTurnSpeedMod: GetFireModeSettingsPlrTurnSpeedMod() -- command used by the default player control mechanism
SetFireModeSettingsZoomTurnSpeed: SetFireModeSettingsZoomTurnSpeed() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsZoomTurnSpeed: GetFireModeSettingsZoomTurnSpeed() -- command used by the default player control mechanism
SetFireModeSettingsPlrJumpSpeedMod: SetFireModeSettingsPlrJumpSpeedMod() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsPlrJumpSpeedMod: GetFireModeSettingsPlrJumpSpeedMod() -- command used by the default player control mechanism
SetFireModeSettingsPlrEmptySpeedMod: SetFireModeSettingsPlrEmptySpeedMod() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsPlrEmptySpeedMod: GetFireModeSettingsPlrEmptySpeedMod() -- command used by the default player control mechanism
SetFireModeSettingsPlrMoveSpeedMod: SetFireModeSettingsPlrMoveSpeedMod() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsPlrMoveSpeedMod: GetFireModeSettingsPlrMoveSpeedMod() -- command used by the default player control mechanism
SetFireModeSettingsZoomWalkSpeed: SetFireModeSettingsZoomWalkSpeed() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsZoomWalkSpeed: GetFireModeSettingsZoomWalkSpeed() -- command used by the default player control mechanism
SetFireModeSettingsPlrReloadSpeedMod: SetFireModeSettingsPlrReloadSpeedMod() -- default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsPlrReloadSpeedMod: GetFireModeSettingsPlrReloadSpeedMod() -- command used by the default player control mechanism
SetFireModeSettingsHasEmpty: SetFireModeSettingsHasEmpty() -- by default sets current weapon, can specify optional gunid and firemode
GetFireModeSettingsHasEmpty: GetFireModeSettingsHasEmpty() -- command used by the default player control mechanism
GetFireModeSettingsActionBlockStart: GetFireModeSettingsActionBlockStart() -- command used by the default player control mechanism
GetFireModeSettingsMeleeWithRightClick: GetFireModeSettingsMeleeWithRightClick() -- get the meleewithrightclick value

SetGamePlayerStateGunSound: SetGamePlayerStateGunSound() -- command used by the default player control mechanism
GetGamePlayerStateGunSound: GetGamePlayerStateGunSound() -- command used by the default player control mechanism
SetGamePlayerStateGunAltSound: SetGamePlayerStateGunAltSound() -- command used by the default player control mechanism
GetGamePlayerStateGunAltSound: GetGamePlayerStateGunAltSound() -- command used by the default player control mechanism
CopyCharAnimState: CopyCharAnimState() -- command used by the default player control mechanism
SetCharAnimStatePlayCsi: SetCharAnimStatePlayCsi() -- command used by the default player control mechanism
GetCharAnimStatePlayCsi: GetCharAnimStatePlayCsi() -- command used by the default player control mechanism
SetCharAnimStateOriginalE: SetCharAnimStateOriginalE() -- command used by the default player control mechanism
GetCharAnimStateOriginalE: GetCharAnimStateOriginalE() -- command used by the default player control mechanism
SetCharAnimStateObj: SetCharAnimStateObj() -- command used by the default player control mechanism
GetCharAnimStateObj: GetCharAnimStateObj() -- command used by the default player control mechanism
SetCharAnimStateAnimationSpeed: SetCharAnimStateAnimationSpeed() -- command used by the default player control mechanism
GetCharAnimStateAnimationSpeed: GetCharAnimStateAnimationSpeed() -- command used by the default player control mechanism
SetCharAnimStateE: SetCharAnimStateE() -- command used by the default player control mechanism
GetCharAnimStateE: GetCharAnimStateE() -- command used by the default player control mechanism
GetCsiStoodVault: GetCsiStoodVault() -- command used by the default player control mechanism
GetCharSeqTrigger: GetCharSeqTrigger() -- command used by the default player control mechanism
GetEntityElementBankIndex: GetEntityElementBankIndex() -- command used by the default player control mechanism
GetEntityElementObj: GetEntityElementObj() -- command used by the default player control mechanism
GetEntityElementRagdollified: GetEntityElementRagdollified() -- command used by the default player control mechanism
GetEntityElementSpeedModulator: GetEntityElementSpeedModulator() -- command used by the default player control mechanism
GetEntityProfileJumpModifier: GetEntityProfileJumpModifier() -- command used by the default player control mechanism
GetEntityProfileStartOfAIAnim: GetEntityProfileStartOfAIAnim() -- command used by the default player control mechanism
GetEntityProfileJumpHold: GetEntityProfileJumpHold() -- command used by the default player control mechanism
GetEntityProfileJumpResume: GetEntityProfileJumpResume() -- command used by the default player control mechanism
SetCharAnimControlsLeaping: SetCharAnimControlsLeaping() -- command used by the default player control mechanism
GetCharAnimControlsLeaping: GetCharAnimControlsLeaping() -- command used by the default player control mechanism
SetCharAnimControlsMoving: SetCharAnimControlsMoving() -- command used by the default player control mechanism
GetCharAnimControlsMoving: GetCharAnimControlsMoving() -- command used by the default player control mechanism
GetEntityAnimStart: GetEntityAnimStart() -- command used by the default player control mechanism
GetEntityAnimFinish: GetEntityAnimFinish() -- command used by the default player control mechanism

VR:MotionControllers
--------------------
GetGamePlayerStateMotionController: GetGamePlayerStateMotionController() -- returns 1 if VR HMD present
GetGamePlayerStateMotionControllerType: GetGamePlayerStateMotionControllerType() -- returns 2 if WMR present
MotionControllerThumbnstickX: MotionControllerThumbnstickX() -- returns X axis of thumbstick on motion controller
MotionControllerThumbnstickY: MotionControllerThumbnstickY() -- returns Y axis of thumbstick on motion controller
CombatControllerTrigger: CombatControllerTrigger() -- detects when the trigger is pressed
CombatControllerGrip: CombatControllerGrip -- detects when the grip button is pressed
CombatControllerButtonA: CombatControllerButtonA -- detects when the A button is pressed
CombatControllerButtonB: CombatControllerButtonB -- detects when the B button is pressed
CombatControllerThumbstickX -- returns X axis of thumbstick on motion controller
CombatControllerThumbstickY -- returns Y axis of thumbstick on motion controller
MotionControllerBestX -- returns X position of currently active motion controller
MotionControllerBestY -- returns Y position of currently active motion controller
MotionControllerBestZ -- returns Z position of currently active motion controller
MotionControllerBestAngleX -- returns X angle of currently active motion controller
MotionControllerBestAngleY -- returns Y angle of currently active motion controller
MotionControllerBestAngleZ -- returns Z angle of currently active motion controller
MotionControllerLaserGuidedEntityObj -- returns the object number of the entity the laser is correctly intersecting
CombatControllerLaserGuidedHit -- returns 1 if the specified object was hit, and also returns X Y Z values of the coordinate of the hit

Entity Material Commands
------------------------
SetEntityBaseColor: SetEntityBaseColor(e,r,g,b) -- this command changes the entity object material property
GetEntityBaseColor: r, g, b = GetEntityBaseColor(e) -- this command gets the entity object material property
SetEntityBaseAlpha: SetEntityBaseAlpha(e,value) -- this command changes the entity object material property
GetEntityBaseAlpha: value = GetEntityBaseAlpha(e) -- this command gets the entity object material property
SetEntityAlphaClipping: SetEntityAlphaClipping(e,value) -- this command changes the entity object material property
GetEntityAlphaClipping: value = GetEntityAlphaClipping(e) -- this command gets the entity object material property
SetEntityNormalStrength: SetEntityNormalStrength(e,value) -- this command changes the entity object material property
GetEntityNormalStrength: value = GetEntityNormalStrength(e) -- this command gets the entity object material property
SetEntityRoughnessStrength: SetEntityRoughnessStrength(e,value) -- this command changes the entity object material property
GetEntityRoughnessStrength: value = GetEntityRoughnessStrength(e) -- this command gets the entity object material property
SetEntityMetalnessStrength: SetEntityMetalnessStrength(e,value) -- this command changes the entity object material property
GetEntityMetalnessStrength: value = GetEntityMetalnessStrength(e) -- this command gets the entity object material property
SetEntityEmissiveColor: SetEntityEmissiveColor(e,r,g,b) -- this command changes the entity object material property
GetEntityEmissiveColor: r, g, b = GetEntityEmissiveColor(e) -- this command gets the entity object material property
ResetEntityMaterial : ResetEntityMaterial ( e ) -- puts back the entity's materials as the level load gave them, slot by slot (its .fpe's, or its element's own): textures, base and emissive colour, emissive strength, roughness, metalness, reflectance, normal strength, alpha and transparency. Model-wide, as the material setters are; per-instance values (SetEntityInstanceEmissive, the tint, limb alpha) are left alone
GetEntityMeshCount : count = GetEntityMeshCount ( e ) -- how many meshes (material slots) the entity's object has, numbered from 0
SetEntityMeshBaseColor : SetEntityMeshBaseColor ( e, slot, r, g, b ) -- one material slot's base colour (0 to 255), where SetEntityBaseColor sets every slot's; model-wide
GetEntityMeshBaseColor : r, g, b = GetEntityMeshBaseColor ( e, slot ) -- one material slot's base colour, where GetEntityBaseColor reads only slot 0's
SetEntityMeshEmissiveColor : SetEntityMeshEmissiveColor ( e, slot, r, g, b ) -- one material slot's emissive colour (0 to 255), where SetEntityEmissiveColor sets every slot's; model-wide
GetEntityMeshEmissiveColor : r, g, b = GetEntityMeshEmissiveColor ( e, slot ) -- one material slot's emissive colour, where GetEntityEmissiveColor reads only slot 0's
SetEntityEmissiveStrength: SetEntityEmissiveStrength(e,value) -- this command changes the entity object material property
GetEntityEmissiveStrength: value = GetEntityEmissiveStrength(e) -- this command gets the entity object material property
SetEntityReflectance: SetEntityReflectance(e,value) -- this command changes the entity object material property
GetEntityReflectance: value = GetEntityReflectance(e) -- this command gets the entity object material property
SetEntityRenderBias: SetEntityRenderBias(e,value) -- this command changes the entity object material property
GetEntityRenderBias: value = GetEntityRenderBias(e) -- this command gets the entity object material property
SetEntityTransparency: SetEntityTransparency(e,value) -- this command changes the entity object material property
GetEntityTransparency: value = GetEntityTransparency(e) -- this command gets the entity object material property
SetEntityDoubleSided: SetEntityDoubleSided(e,value) -- this command changes the entity object material property
GetEntityDoubleSided: value = GetEntityDoubleSided(e) -- this command gets the entity object material property
SetEntityPlanarReflection: SetEntityPlanarReflection(e,value) -- this command changes the entity object material property
GetEntityPlanarReflection: value = GetEntityPlanarReflection(e) -- this command gets the entity object material property
SetEntityCastShadows: SetEntityCastShadows(e,value) -- this command changes the entity object material property
GetEntityCastShadows: value = GetEntityCastShadows(e) -- this command gets the entity object material property
SetEntityZDepthMode: SetEntityZDepthMode(e,value) -- a value of 0 switches off culling, 1 is normal CCW culling and 2 is ignore zdepth in scene
GetEntityZDepthMode: value = GetEntityZDepthMode(e) -- this command gets the entity object material property
SetEntityTexture: SetEntityTexture(e,imageID) -- change the texture of an entity to that specified by imageID
SetEntityTextureScale: SetEntityTextureScale(e,u,v) -- change the texture UV map of an entity to scale it (1.0=no change)
SetEntityTextureOffset: SetEntityTextureOffset(e,u,v) -- change the texture UV map of an entity to offset all coordinates (0.0=no change)
SetEntityOutline: SetEntityOutline(e,value) -- this command changes the entity object outline property.
GetEntityOutline: GetEntityOutline(e) -- this command gets the entity object outline property.

IsPlayerInGame: IsPlayerInGame() -- return 1 if the player is in the game
SetLevelFadeoutEnabled: SetLevelFadeoutEnabled(mode) -- sets the fade out system mode to zero or one

SetSunDirection: SetSunDirection(x,y,z) -- rotate the sun directional light in X, Y and Z degrees (roll, pitch and yaw)
SetSunLightingColor: SetSunLightingColor(r,g,b) -- set the R,G,B color of the sun color
SetSunIntensity: SetSunIntensity(intensity) -- set the intensity value of the sun color
SetExposure: SetExposure(value) -- set the exposure value of the post processing system
GetExposure: value = GetExposure() -- get the exposure value of the post processing system

Understanding old PREEXIT system
--------------------------------
SetPreExitValue: SetPreExitValue(e,value) -- where e is the entity and a value of 2 will exit preexit function
function ai_melee_animmove_init(e)
 SetPreExitValue(e,0) -- set this to zero to clear out this value for the entity
end
function ai_melee_animmove_preexit(e)
 SetPreExitValue(e,1) -- setting this to one will ensure we keep calling preexit over and over
 if module_combatmelee.preexit(e,ai_movetype_useanim) == 1 then
  SetPreExitValue(e,2) -- when we set this to two, we instruct the engine to stop calling preexit and go to _exit
 end
end
function ai_melee_animmove_exit(e)
 module_combatmelee.exit(e)
end

These AI commands represent a subset of the DarkAI system (further help referenced at Docs\DarkAI Documentation):

NEW MANUAL MODE COMMANDS
AIEntityGoToPosition: AIEntityGoToPosition(obj,x,z) -- where obj is the object number of the entity.Use this for legacy AI bot movement
AIEntityGoToPosition: AIEntityGoToPosition(obj,x,z,containerindex) -- where obj is the object number of the entity. New decoupled AI bot manual control
AISetEntityControl: AISetEntityControl(obj,controlmode) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIEntityStop: AIEntityStop(obj) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIGetEntityCanSee: AIGetEntityCanSee(obj,x,z,groundflag) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIGetEntityViewRange: AIGetEntityViewRange(obj) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIGetEntityHeardSound: AIGetEntityHeardSound(obj) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AICouldSee: AICouldSee(obj,x,y,z) -- where obj is the object number of the entity. XYZ of the point in space tested line of sight by the AI
AIGetEntityIsMoving: AIGetEntityIsMoving(obj) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AISetEntityPosition : AISetEntityPosition(obj,x,y,z) -- where obj is the object number of the entity and XYZ are the new coordinates
AISetEntityTurnSpeed : AISetEntityTurnSpeed(obj,speed) -- where obj is the object number and speed is the turning speed for navigating entities
AIGetEntitySpeed: speed = AIGetEntitySpeed(obj) -- where obj is the object number of the entity and returns the movement speed of the entity
AIGetTotalPaths: AIGetTotalPaths() -- returns the number of created paths
AIGetPathCountPoints: AIGetPathCountPoints(pathnumber) -- where pathnumber is the internal number of the path
AIPathGetPointX: AIPathGetPointX(pathnumber,pointindex) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIPathGetPointY: AIPathGetPointY(pathnumber,pointindex) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIPathGetPointZ: AIPathGetPointZ(pathnumber,pointindex) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIGetTotalCover: AIGetTotalCover() -- returns the number of created cover zones
AICoverGetPointX: AICoverGetPointX(coverindex) -- where coverindex is the index of the cover marker and returns the X position
AICoverGetPointY: AICoverGetPointY(coverindex) -- where coverindex is the index of the cover marker and returns the Y position
AICoverGetPointZ: AICoverGetPointZ(coverindex) -- where coverindex is the index of the cover marker and returns the Z position
AICoverGetAngle: AICoverGetAngle(coverindex) -- where coverindex is the index of the cover marker and returns the Y angle position
AICoverGetIfUsed : AICoverGetIfUsed(coverindex) -- where coverindex is the index of the cover marker and returns string from IFUSED field

SetRotationYSlowly : SetRotationYSlowly(e,destangle,smoothvalue) -- where e is the entity and a smooth value of 100 is immediate, where 50 takes twice as long to rotate to destangle
GetSpeech : istalking = GetSpeech(e) -- returns 1 while mouth data is playing through character associated with E

NEW MAX AI COMMANDS
RDFindPath : RDFindPath(x1,y1,z1,x2,y2,z2) -- asks navigation system to work out a path between two coordinates in the level
RDGetPathPointCount : count = RDGetPathPointCount() -- returns the number of points along the path calculated with RDFindPath
RDGetPathPointX : x = RDGetPathPointX(pointindex) -- returns the X coordinate of the specified point
RDGetPathPointY : y = RDGetPathPointY(pointindex) -- returns the Y coordinate of the specified point
RDGetPathPointZ : z = RDGetPathPointZ(pointindex) -- returns the Z coordinate of the specified point
StartMoveAndRotateToXYZ : StartMoveAndRotateToXYZ(e,movespeed,turnspeed,tiltmode,stopatdist) -- starts the specified entity along a path previously calculated with RDFindPath, and optionally allows to stop before very end by 'stopatdist'
MoveAndRotateToXYZ : pointindex, remainingdistance = MoveAndRotateToXYZ(e,movespeed,turnspeed,stopatdist) -- continually moves entity along path at move and turn speed specified, and optionally allows to stop before very end by 'stopatdist'
SetEntityPathRotationMode : SetEntityPathRotationMode(e,mode) -- set to 0 to disable object rotation when it follows a path (defaults to 1 to rotate the object to follow path direction)
AdjustPositionToGetLineOfSight : success, x, z = AdjustPositionToGetLineOfSight(ignoreobj,x,y,z,targetx,targety,targetz,adjustmentradius)
RDIsWithinMesh : result = RDIsWithinMesh(x,y,z) -- returns 1 if the XYZ coordinate is on the nav mesh
RDIsWithinAndOverMesh : result = RDIsWithinAndOverMesh(x,y,z) -- returns 1 if the XYZ coordinate is on the nav mesh and over it
RDGetYFromMeshPosition : y = RDGetYFromMeshPosition(x,y,z) -- returns the correcy Y coordinate of the nearest polygon surface on the nav mesh at specified position
RDBlockNavMesh : RDBlockNavMesh(x,y,z,radius,blockmode) -- blocks an area on the nav mesh at specified position and radius, use blockmode of 1 to block
RDBlockNavMeshWithShape : RDBlockNavMeshWithShape(x,y,z,sizex,blockmode,sizez,angle) -- blocks an area on the nav mesh at specified position and radius, use blockmode of 1 to block
RDBlockNavMeshWithShape : RDBlockNavMeshWithShape(x,y,z,sizex,blockmode,sizez,angle,adjminy,adjmaxy) -- blocks an area on the nav mesh at specified position and radius, use blockmode of 1 to block
SetCharacterMode : success = SetCharacterMode (e,mode) -- returns 1 if a character objects 'mode' was successfully changed (mode 1=character, 0=non-character)

NEW MAX ZONE COMMANDS
GetEntityInZoneWithFilter : result = GetEntityInZoneWithFilter(e) -- returns 1 if any entity is in this zone
GetEntityInZoneWithFilter : result = GetEntityInZoneWithFilter(e,mode) -- returns 1 if specific entity is in this zone (1 to detect only active objects, 2 to only detect characters that are active, 3 to only detect non-characters that are active, 4 to only detect non static objects, 5 to only detect static objects)
IsPointWithinZone : result = IsPointWithinZone(e,x,y,z) -- returns 1 if the XYZ coordinate is within specified entity zone

NEW MAX TOKENDROP COMMANDS
DoTokenDrop : DoTokenDrop(x,y,z,type,duration) -- creates a drop token at X, Y, Z position, with custom type and milliseconds life duration
GetTokenDropCount : count = GetTokenDropCount() -- returns the total number of target drops
GetTokenDropX : x = GetTokenDropX(index) -- returns the X coordinate of the specified target drop
GetTokenDropY : y = GetTokenDropY(index) -- returns the Y coordinate of the specified target drop
GetTokenDropZ : z = GetTokenDropZ(index) -- returns the Z coordinate of the specified target drop
GetTokenDropType : type = GetTokenDropType(index) -- returns the Type coordinate of the specified target drop
GetTokenDropTimeLeft : timeleft = GetTokenDropTimeLeft(index) -- returns the TimeLeft in milliseconds of the specified target drop

OLD LEGACY AUTOMATIC COMMANDS
AIEntityAssignPatrolPath AIEntityAssignPatrolPath(obj,pathid) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIEntityAddTarget: AIEntityAddTarget(obj,targetid) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIEntityRemoveTarget: AIEntityRemoveTarget(obj,targetid) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIEntityMoveToCover: AIEntityMoveToCover(obj,x,z) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions
AIGetEntityCanFire: AIGetEntityCanFire(obj) -- where obj is the object number of the entity. See DarkAI docs for parameter descriptions

GetHeadTracker : GetHeadTracker() -- returns a value of one if the head tracker is connected
ResetHeadTracker : ResetHeadTracker() -- resets the tracker to signal device is facing forward
GetHeadTrackerYaw : GetHeadTrackerYaw() - returns the yaw of the head tracker if attached
GetHeadTrackerPitch : GetHeadTrackerPitch() - returns the pitch of the head tracker if attached
GetHeadTrackerRoll : GetHeadTrackerRoll() - returns the roll of the head tracker if attached

Prompt3D : Prompt3D(text,duration) -- renders a 3D text panel in front of camera showing 'text'
PositionPrompt3D : PositionPrompt3D(x,y,z,angle) -- repositions 3D text panel to any XYZ world coordinate

ScaleObject : ScaleObject( obj, x, y, z ) -- Scales object in all axis (Note: uses object id not entity!)

SetSkyTo : SetSkyTo ( str ) -- where str is the folder name of the sky you want to change to

SetGrassDistance : SetGrassDistance ( d ) -- grass draw distance in world units (750 to 7000), kept until the next level loads. Rebuilds all grass, so call on events not every frame
GetGrassDistance : d = GetGrassDistance() -- current grass draw distance in world units

SetTreeDistance : SetTreeDistance ( d [, shadowD] ) -- distance where trees switch from full detail to billboards, and the same for their shadows (750 to 20000). Overrides the graphics quality presets; 0 or omitted keeps the current value
GetTreeDistance : d, shadowD = GetTreeDistance()
SetTreeTransition : SetTreeTransition ( w [, shadowW] ) -- width of the band where billboards and full detail trees cross fade (100 to 4000, default 500)
GetTreeTransition : w, shadowW = GetTreeTransition()
SetTreeShadowCascades : SetTreeShadowCascades ( billboard [, fullDetail] ) -- how many sun shadow cascades (0 to 5, nearest first) draw billboard and full detail tree shadows; negative or omitted keeps the current value; a cascade with billboard but not full detail shadows draws billboard shadows for the nearer trees too
SetTreeShadowMeshLOD : SetTreeShadowMeshLOD ( cascade1 [, further] ) -- the mesh that casts the full detail tree shadows of sun shadow cascade 1 and of cascades 2 and on: 0 the full mesh, 1 LOD1, 2 LOD2 (lighter, used only where it has fewer triangles); negative or omitted keeps the current value; by default 0 and 2, cleared at the next level. Cascades 2 and on also take the leaf alpha one mip coarser
GetTreeShadowMeshLOD : cascade1, further = GetTreeShadowMeshLOD ( ) -- the two values SetTreeShadowMeshLOD sets, as in force
GetTreeShadowCascades : billboard, fullDetail = GetTreeShadowCascades()
PhysicsRay : hit, x, y, z, nx, ny, nz, obj, fraction, tree, treetype = PhysicsRay ( x1, y1, z1, x2, y2, z2 [, layers [, ignore] ] ) -- the nearest body the segment meets in the physics world, fast (about 0.005 ms, against 1-5 ms for a Wicked pick) and with no force applied (PhysicsRayCast pushes what it hits and returns moving bodies only). layers add up: 1 the ground (and entities built as terrain colliders), 2 static entities, 4 characters, 8 moving bodies (11 when left out); ignore: an object number or a table of up to 8. obj: 0 the ground or a tree, -1 a character capsule, a ragdoll's bone its character's object; fraction: how far along; tree 1 for a trunk. The physics world has the ground only as detailed as the terrain round the camera, no body for collisionmode 11 or 12, and trunks only round the camera (RayTrees has them everywhere) treetype is a trunk's tree type (GetTreeTypeName names it), -1 when the hit isn't a trunk.
PhysicsSweepBox : hit, x, y, z, nx, ny, nz, obj, fraction, tree, treetype = PhysicsSweepBox ( cx, cy, cz, hx, hy, hz, yaw, mx, my, mz [, layers [, ignore [, trees] ] ] ) -- the first thing a box meets as it moves by mx, my, mz: centred on cx, cy, cz, reaching hx, hy, hz each way along its own axes, turned by yaw degrees about Y as an object is (GetObjectColBox gives a hull's). Meets the physics world as PhysicsRay does, and with trees 1 (the default) every drawn tree trunk on the map, base to top; with trees 2 only each trunk's bare part, from its base to its crown (GetTreesNear's crown), so a pass through a canopy is no hit. No gaps between samples, no tunnelling, and the normal of the real contact. fraction 0: the box already touched something (lift a car's box clear of the ground, or leave the ground out of layers) treetype is a trunk's tree type (GetTreeTypeName names it), -1 when the hit isn't a trunk.
PhysicsOverlapBox : count, trees, obj1, obj2, ... = PhysicsOverlapBox ( cx, cy, cz, hx, hy, hz, yaw [, layers [, ignore [, trees] ] ] ) -- what a box (as PhysicsSweepBox's) touches now, e.g. a wedged car: count bodies (their object numbers follow, up to 32; 0 the ground, -1 a character capsule, a ragdoll's bone its character's object) and how many tree trunks (with trees 1, the default; with trees 2 only their bare parts below the crowns)
GetPhysicsStats : subSteps, awake, manifolds, points, busiestObject, busiestPoints = GetPhysicsStats ( ) -- what the physics did in its last update, to see what a costly physics frame is working on: subSteps the simulation steps it took (1/120 s each, at most 7, so a slow frame takes more), awake the bodies moving or settling, manifolds the body pairs in contact and points their contact points, then busiestObject the object with the most contact points and busiestPoints its points (the ground and character capsules left out, so a capsule pushing on a vehicle names the vehicle; -1 none; a tree's collision cylinder near the camera can be named)
GetPhysicsStatsTop : list = GetPhysicsStatsTop ( [n] ) -- the bodies with the most contact points in the physics' last update, most first, up to n (5 by default, at most 32), the ground and character capsules left out: a table of { e (entity, -1 none), obj (-1 unknown), points, awake, speed (units a second; before 2026-10-02 in the physics' own units, 40 times smaller), kind ("dynamic", "static", "tree" or "other"), spin (radians a second), state ("active", "asleep", "ready": still enough to sleep but its island isn't, "always": kept awake by the physics, never sleeping, "off"), still (seconds under its sleep speeds; it sleeps after 2), sleepspeed, sleepspin (those speeds), island (bodies touching, directly or through others, share it and sleep together; -1 none) }
GetLuaEntityCosts : costs = GetLuaEntityCosts ( ) -- the entities' turns in the engine's Lua loop over the last second, nil until a second has passed (not in the stock exe: guard it): { window (ms), frames, total, row, main, key (ms in all: every turn, the UpdateEntityRT row refreshes, the scripts' main calls, the scans for an uncollected USE KEY), turns, rows, mains, keyscans (counts over the window), keymax (most entities scanning for a USE KEY in one frame), gc (turns in which the Lua heap shrank by 1 MB or more: a collection step ran inside them), top = { { e, ms, row, main, key, gc, script, phyalways, plrdist }, ... } (the ten dearest entities over the window, dearest first) }
GetFrameCosts : costs = GetFrameCosts ( ) -- the frames since the last call, nil if none (not in the stock exe: guard it): { window (ms), frames, submits, submit, present, execute (ms summed: each frame's submit of its command lists, the presents in it, waiting for vsync or for the GPU when it is behind, and the rest), submitmax, presentmax, executemax (the most in one frame), vsync (1 on), steps = { name = ms } (each main thread step's own time summed, a range's without the ranges inside it; CPU Frame its time outside them all, outside ranges the time between frames) }
SetGpuTiming : wason = SetGpuTiming ( on ) -- the engine's own GPU timing on (1) or off (0, as at the start), from the next frame; returns 1 if it was on (not in the stock exe: guard it)
GetGpuFrameCosts : costs = GetGpuFrameCosts ( ) -- the frames read back since the last call, nil if none (timing off or none yet; not in the stock exe: guard it): { frames, missed (left out: not finished eight frames later, or across a GPU clock change), gpu (ms summed, each frame's first command list's start to its last one's end), gpumax (the most in one frame), lists = { name = ms } (each command list's GPU time summed, e.g. DrawShadowmaps, OpaqueZPrepass, PostprocessRTReflection the opaque scene, RenderLightShafts the transparents and post, Particles the update's GPU work, Swapchain the final compose), parts = { name = ms } (named spans inside them, which nest: Opaque - Terrain, Opaque - Trees, Opaque - Grass, Transparent, Ocean, Post and others) }
SetGrassFadeBand : SetGrassFadeBand ( units ) -- the width of the dithered band past the grass distance where the cards thin out, 0 to 5000 (2500 at first); the cards are spread over the distance plus the band, so a narrower band packs them closer; every chunk is made again (not in the stock exe: guard it)
SetGrassEqualDepth : SetGrassEqualDepth ( on ) -- 1: the grass colour pass draws only where its prepass left a card's own depth, so its lit shader runs once a pixel; 0 (at first) as before (not in the stock exe: guard it)
SetGrassTightBounds : SetGrassTightBounds ( on ) -- 1: the grass chunks' bounds fitted to the cards, so chunks off screen are culled; 0 (at first) padded as before (not in the stock exe: guard it)
SetGrassGrid : SetGrassGrid ( n ) -- the grid the 400,000 grass cards are spread over, 8 (at first) or 16 chunks across: 16 stands the same cards about 36% closer; every chunk is made again (not in the stock exe: guard it)
SetGrassBudget : SetGrassBudget ( cards ) -- the grass cards spread over the grid, 50,000 to 800,000 (400,000 at first): twice the cards over the same distance plus band is twice as dense; room for the most is made at the start, so this makes no buffer; every chunk is made again, a few a frame (not in the stock exe: guard it)
GetGrassTuning : tuning = GetGrassTuning ( ) -- { fadeband, equaldepth, tightbounds, grid, cards } as set (not in the stock exe: guard it)
SetTerrainMeasure : SetTerrainMeasure ( mode ) -- 0 none (at first); 1 the terrain prepass depth only (its pages stop following the camera: a measuring run only); 2 both terrain passes into a 1 x 1 viewport, their geometry's GPU cost alone (not in the stock exe: guard it)
SetTerrainNearFirst : SetTerrainNearFirst ( on ) -- 1 (at first) the terrain prepass draws its chunks nearest first, the same picture; 0 in the old order (not in the stock exe: guard it)
GetTerrainDrawStats : stats = GetTerrainDrawStats ( ) -- the last terrain prepass: { chunks (drawn), flat (of them, heights within 0.01 units), triangles } (not in the stock exe: guard it)
SetTerrainDepthOnlyPrepass : SetTerrainDepthOnlyPrepass ( on ) -- 1: the terrain prepass draws depth only and the main view's colour pass writes the virtual texture's page requests instead (the same requests); 0 (at first) as before; not in VR (not in the stock exe: guard it)
SetTerrainSeabedSkip : SetTerrainSeabedSkip ( on ) -- 1: seabed the ocean's water fog leaves less than 1/20,000 of is shaded flat, as nothing of it shows (camera above the water only); 0 (at first) as before (not in the stock exe: guard it)
GetPhysicsBodyPose : x, y, z, ax, ay, az, bodies = GetPhysicsBodyPose ( obj ) -- where an object's physics body is, as the pose it would give the object (position, and angles in degrees as GetEntityAngleX/Y/Z), then how many bodies the physics holds for that object (more than 1 is one left behind; the pose is the first's, the one CollisionOff removes). A static body never moves its object, so this shows the collider against the drawn object. No body: nil for the pose, and 0
GetTreesNear : trees = GetTreesNear ( x, z, radius [, max] ) -- the tree trunks within radius of x, z across (any height), nearest first, as a table of { x = , y = , z = , radius = , type = , name = , top = , crown = , scale = }: each trunk's base and radius as the trees' collision has it (by species and scale), the tree's top, crown (where its crown starts: the lowest branch or leaf of its species' full detail mesh at its scale, the top for a species with none; below it only the bare trunk) and its own scale; at most max (64 when left out). Trees not drawn are left out (hidden under a building, removed by a blast, or all when trees are off). For a script's own collision test, e.g. a car driven by Lua type is the tree type and name its name ("jungletree3a").
GetTreeTypeName : name = GetTreeTypeName ( type ) -- the name of a tree type (GetTreesNear's type, PhysicsRay's and PhysicsSweepBox's treetype) as the tree bank's billboard files give it, such as "jungletree3a"; "" for none
GetTreeCounts : high, shadow = GetTreeCounts ( ) -- the trees drawn at full detail this frame, and the trees in the full detail shadow list; both grow with the tree distance and transition (SetTreeDistance, SetTreeTransition)
GetTerrainPageStats : waiting, visible, readbacks, made = GetTerrainPageStats ( ) -- the terrain's virtual texture: pages wanted on screen but not made yet (a backlog that grows while the ground shows lower detail), pages wanted on screen in all, then since the level started the read backs taken and the pages made (their change over a second gives rates)
GetGameQuality : quality = GetGameQuality ( ) -- the graphics quality in force: 1 Low, 2 Medium, 3 High (the level's own settings, and what a game has until the player changes quality), 4 Ultra
RayTrees : hit, x, y, z, nx, ny, nz = RayTrees ( x1, y1, z1, x2, y2, z2 ) -- the nearest tree trunk the segment meets, anywhere on the map (the physics and IntersectAll see only trunks near the camera, or none): hit 1 with the point and the trunk's normal, else 0 and zeros. A trunk is as thick as the trees' collision (by species and scale), from the ground to the tree's top; canopies don't count, nor trees hidden under a building or removed by a blast. Walks only the tree chunks along the segment, so long rays (a laser designator's) are cheap

SetBloom : SetBloom ( on [, strength [, threshold] ] ) -- bloom on (1) or off (0), strength 0.1 to 3, threshold 0.1 to 10. For this level only; omitted or negative values keep the current ones
GetBloom : on, strength, threshold = GetBloom()
SetDepthOfField : SetDepthOfField ( on [, strength [, focalLength [, aperture] ] ] ) -- depth of field on or off, strength 1 to 20, focal length 0.001 to 800, aperture 0 to 1. For this level only
GetDepthOfField : on, strength, focalLength, aperture = GetDepthOfField()
SetLightShafts : SetLightShafts ( on ) -- sun light shafts on or off, for this level only
GetLightShafts : on = GetLightShafts()
SetLensFlare : SetLensFlare ( on ) -- sun lens flare on or off, for this level only
GetLensFlare : on = GetLensFlare()
SetOcclusionCulling : SetOcclusionCulling ( on [, objects [, animations [, terrain [, shadows] ] ] ] ) -- the GPU occlusion culling (skips what stands wholly behind something, a frame late, nothing within 1500 units) and what uses it: objects, the animation of an occluded character (it pauses), terrain chunks, point and spot light shadows; 1 or 0 each, an omitted or negative value keeps the current one. Kept through graphics quality changes until the level ends (not SetOcclusion, the old CPU occluder) (a sixth value, spotShadows, sets spot light shadow culling apart from point light shadows)
SetLODMultiplier : SetLODMultiplier ( v ) -- models with LOD levels (a _lod.dbo) switch to LOD 1, 2, 3 past 400, 600, 800 units times v (0 to 15; the graphics quality sets 1 to 3); until the level ends
SetShadowsLowestLOD : SetShadowsLowestLOD ( on ) -- models cast shadows from their lowest LOD (cheaper); until the level ends
SetDelayedShadows : SetDelayedShadows ( on [, laptop] ) -- the sun's shadow cascades 1 to 4 redrawn every 2, 3, 4, 9 frames instead of every frame, point light shadows less often too; laptop also redraws cascade 0 every other frame and the rest every 3, 4, 5, 9 (on unless the level turned it off). An omitted or negative value keeps the current one; until the level ends
ResetDelayedShadows : ResetDelayedShadows ( ) -- forget what SetDelayedShadows set and go back to the level's or the graphics quality's delayed shadows (e.g. on after a heli flight that turned them off)
SetLightMaxDistance : SetLightMaxDistance ( distance ) -- point and spot lights farther than this from the camera (their range added) are left out of the frame as lights out of view are, so they take no light slot; 0, the default, for no limit; back to none at the next level and test end
SetShadowJobWait : SetShadowJobWait ( on ) -- the render's shadow job waits for the frame set up and prepass jobs even with delayed shadows off, as it does with them on (off by default; a test for the stalls in Wicked's Render with delayed shadows off); off again at the next level and test end
GetDelayedShadows : on, laptop, script = GetDelayedShadows ( ) -- the delayed shadow refresh in force now (1 or 0 each), and script 1 while a SetDelayedShadows value holds
SetFXAA : SetFXAA ( on ) -- the FXAA pass (1 or 0); like every graphics setting below it holds through visuals pushes and quality changes and goes back to the level's at the next level and when a test game ends
SetReflections : SetReflections ( on ) -- the planar reflections (1 or 0)
SetProbesLowestLOD : SetProbesLowestLOD ( on ) -- environment probes drawn from each model's lowest LOD
SetReflectionsLowestLOD : SetReflectionsLowestLOD ( on ) -- reflections drawn from each model's lowest LOD
SetAnimations30Fps : SetAnimations30Fps ( on ) -- far characters animate at 30 frames a second
SetMaxApparentSize : SetMaxApparentSize ( v ) -- objects smaller on screen than v (0.000001 to 0.2, the editor's value / 10000) aren't drawn
SetShadowResolution : SetShadowResolution ( sun [, spot [, point] ] ) -- shadow map sizes, 0 (none) to 2048; a change recreates the maps (a hitch), so set it from a menu, not every frame
SetShadowLights : SetShadowLights ( spotMax [, pointMax] ) -- how many spot and point lights cast shadows at once, 0 to 16
SetShadowCascades : SetShadowCascades ( n [, split1, split2, split3, split4] ) -- the sun's shadow cascades in use, 1 to 5 (past the last, no sun shadow), and the view depths the first four end at (380, 950, 7500, 30000 unless set; used only if all four are given, above 0 and rising); a moved split sharpens or blurs that cascade, and the depth bias was tuned to the stock splits, so acne or light leaks may show
SetTerrainDetail : SetTerrainDetail ( limit [, scale [, readBack] ] ) -- the terrain texture detail as the graphics quality sets it: limit 0 (finest) to 4, scale 0.25 to 2, read back reduction 4 to 6 (higher: coarser pages, less read back)
SetGrassSimpleLighting : SetGrassSimpleLighting ( on ) -- the grass lit by the simpler, cheaper PBR (Low and Medium quality)
SetVsync : SetVsync ( on ) -- vsync on (1) or off (0), outranking the level's and SETUP.INI's; live, with a blip as the swap chain is made again
SetFrameRateCap : SetFrameRateCap ( fps ) -- the most frames a second, 10 to 1000; 0 for no cap; live
SetRenderScale : SetRenderScale ( f ) -- the 3D drawn at f of the screen's resolution, 0.5 to 1, upscaled with FSR 1 (the HUD and sprites stay at the screen's; FXAA stays on below 1); stands in for the level's FSR setting, 0 goes back to it; a hitch as the render targets are made again, so set it on a slider's release
GetRenderScale : f = GetRenderScale() -- the fraction of the screen's resolution the 3D is drawn at
SetMSAA : SetMSAA ( samples ) -- multisample anti-aliasing, 1 (off), 2, 4 or 8, with or without FXAA (under a render scale it samples the smaller 3D); a hitch as the render targets are made again; 0 goes back to the level's
SetGamma : SetGamma ( g ) -- the gamma (a brightness slider), 0.1 to 10; the level start's fade-in heads for it; 0 goes back to the level's
SetFSRSharpness : SetFSRSharpness ( s ) -- the sharpening of the FSR upscale (the level's FSR setting or SetRenderScale), 0 to 2
SetRaycastLowestLOD : SetRaycastLowestLOD ( on ) -- rays and picks test models that have LOD levels against their lowest LOD: cheaper, less exact
SetTransparentShadows : SetTransparentShadows ( on ) -- shadows cast through transparent surfaces; a hitch as the shadow maps are made again
GetGraphicsSettings : settings = GetGraphicsSettings ( ) -- the values in force as a table: fxaa, reflections, probesLowestLOD, reflectionsLowestLOD, animations30Fps, maxApparentSize, sunShadow, spotShadow, pointShadow, spotShadowLights, pointShadowLights, shadowCascades, shadowSplits (four), terrainDetailLimit, terrainDetailScale, terrainReadBack, grassSimpleLighting, vsync, frameRateCap, renderScale, msaa, ssr, ao, aoPower, lodMultiplier, shadowRange, occlusion, objectCulling, animationCulling, terrainCulling, shadowCulling (point lights), spotShadowCulling, shadowsLowestLOD, gamma, fsrSharpness, raycastLowestLOD, transparentShadows
ResetGraphicsSettings : ResetGraphicsSettings ( ) -- forget every value the graphics setters above set (and SetOcclusionCulling, SetShadowsLowestLOD, SetGamma, SetFSRSharpness, SetRaycastLowestLOD, SetTransparentShadows, SetSSR, SetAO, SetLODMultiplier, SetShadowRange, SetMSAA, SetTreeDistance, SetTreeTransition and SetGrassDistance) and go back to the level's and the graphics quality's, so a quality change moves them again (a menu's Default)
SetSSR : SetSSR ( on ) -- screen space reflections on (1) or off (0); until the level ends
SetAO : SetAO ( on [, power] ) -- ambient occlusion (corners and gaps take less ambient light) on (1) or off (0), and its power 0 to 5 (1 is the editor's default; omitted keeps the current one); until the level ends
SetShadowRange : SetShadowRange ( d ) -- how far the sun's last shadow cascade reaches, 31000 to 500000 units (500000 unless set): nothing beyond casts a sun shadow, nearer makes far shadows sharper. 0 goes back to the level's; until the level ends
SetWind : SetWind ( speed [, dirX, dirY, dirZ [, randomness] ] ) -- weather wind (rain and snow drift): speed 0 to 5, direction -20 to 20 per axis, randomness 0 to 2. Tree sway is SetTreeWind
GetWind : speed, dirX, dirY, dirZ, randomness = GetWind()
SetWaterFog : SetWaterFog ( minDist, maxDist [, minAmount] ) -- how clear the water is: nearer than minDist the water fog is at minAmount (0 to 1, lower is clearer), beyond maxDist the water is opaque (0 to 100000 units). SetWaterTransparancy does nothing in MAX
GetWaterFog : minDist, maxDist, minAmount = GetWaterFog()
SetRawSoundDistanceScale : SetRawSoundDistanceScale ( id, units ) -- 3D distance for one sound: full volume within about this distance, fading beyond; 0 = the global setup.ini curvedistancescaler (default 250). id from GetEntityRawSound( e, slot )
GetRawSoundDistanceScale : units = GetRawSoundDistanceScale ( id ) -- the distance scale this sound plays with
SetDopplerScale : SetDopplerScale ( scale ) -- how strongly 3D sounds rise in pitch as they come closer and fall as they go away, from how fast each sound and the camera move: 1 as in the real world (the default), 0 none, more to exaggerate; back to 1 at the next level
GetDopplerScale : scale = GetDopplerScale() -- the doppler strength
SetDecalRange : SetDecalRange ( units ) -- decals (water splashes and ripples, impacts, blood) further than this from the camera are not made; 800 (about 20 m) unless set, back to that at the next level
GetDecalRange : units = GetDecalRange() -- the decal range
SetDecalLimit : SetDecalLimit ( total [, ripples] ) -- how many decals can show at once (100 unless raised, up to 349; made at once and kept) and how many of those only water ripples may use (0, the default, shares them all; back to 0 at the next level)
GetDecalLimit : total, ripples = GetDecalLimit() -- the decal limit and the elements kept for ripples
GetDecalStats : showing, outofrange, nofree = GetDecalStats() -- decals showing now, and since the level started those not made for being out of range and for finding no free element
AnglesToQuat : x, y, z, w = AnglesToQuat ( ax, ay, az ) -- the engine's angles (degrees, as SetRotation and GetEntityAngleX/Y/Z) to a quaternion (as QuatMultiply and QuatSLERP take); QuatToEuler and EulerToQuat use radians and mirror pitch and roll
QuatToAngles : ax, ay, az = QuatToAngles ( x, y, z, w ) -- a quaternion back to the engine's angles, in degrees
AddProjectedDecal : id = AddProjectedDecal ( image, x, y, z, size [, nx, ny, nz [, spin [, depth [, life] ] ] ] ) -- a texture projected onto the terrain and objects round x, y, z, following slopes, steps and corners (a scorch mark). size: width across the surface; nx, ny, nz: the surface normal it projects along (0, 1, 0 by default); spin: degrees about it; depth: how far along the normal it reaches (half its size by default); life: seconds before it fades out and goes (0 = never). image: a texture path as models use them (a .dds with mipmaps, or a .png). 0 if the texture did not load; none stay past the level
RemoveProjectedDecal : RemoveProjectedDecal ( id ) -- removes it at once; -1 removes them all
SetProjectedDecalOpacity : SetProjectedDecalOpacity ( id, percent ) -- 0 to 100, for a fade of your own
SetProjectedDecalFacing : SetProjectedDecalFacing ( id, cutoff [, fade] ) -- only surfaces facing the decal's normal (a blast decal's centre) by more than cutoff, a cosine, take it: 0.2 unless set (0.1 for a blast decal), so it doesn't reach the inside face of a thin wall or streak a face along its box; -1 lets every surface in its box take it. fade: how far past the cutoff it takes to reach full strength, smoothly (0.01 to 1, 0.1 when left out); wider fades it out round a curve instead of ending in a hard line
AddBlastDecal : id, rayscast = AddBlastDecal ( image, x, y, z, radius [, life [, opacity [, nx, ny, nz [, rays [, spin] ] ] ] ] ) -- rayscast: how many rays it cast; they test the ground and only the objects within their reach (1.125 x radius round the centre) that take decals (not those SetEntityReceivesDecals refused, such as vehicles and soldiers), and none are cast when there are no such objects, as the ground alone is left to the facing test, so a blast on open ground costs no rays. spin: degrees about the up axis, used only when no wall squared the decal (open ground, or rays 0 on a floor hit), so ground scorches don't all face one way (0 when left out). a scorch round a blast that lands on every surface within radius facing it, the floor and both walls of a corner alike, each unstretched (triplanar); it fades out towards the radius. nx, ny, nz: the normal of the surface hit (up by default), which lifts the centre a quarter of the radius off it. opacity: 0 to 100. rays 0 skips the short rays (34 at most, once) that turn it square to the nearest wall and keep it off surfaces behind the first one along each of its axes (a floor under a slab, a wall behind a wall). A projected decal: same ids, limit and clearing; RemoveProjectedDecal, SetProjectedDecalOpacity, SetProjectedDecalGrass, SetProjectedDecalFacing and SetProjectedDecalBlend work on it
SetProjectedDecalBlend : SetProjectedDecalBlend ( id, power ) -- how sharply a blast decal's surfaces take the texture from the direction they face most, 1 to 64 (8 unless set): higher leaves fewer surfaces blending two or three directions (cheaper, no ghosted double image on slopes); too high shows a seam where the direction changes
SetProjectedDecalGrass : SetProjectedDecalGrass ( id, radius [, trees] ) -- no grass is drawn within radius of the decal's centre (0 = none); those whose edge is nearest the camera (the one it stands in first) use the grass kill shapes SetGrassKillBox's 8 leave free, 32 in all; one too far to reach the drawn grass takes none. trees 1 (0 when left out) also removes every tree whose trunk reaches within radius of the centre (as far above and below), for the rest of the level: they stay gone when the decal goes, and come back only at the next level start or test game end
SetProjectedDecalLimit : SetProjectedDecalLimit ( count ) -- how many projected decals there can be, 48 unless set, up to 96; the oldest go first past it; back to 48 at the next level
GetProjectedDecalLimit : count = GetProjectedDecalLimit() -- the projected decal limit
GetProjectedDecalCount : count = GetProjectedDecalCount() -- the projected decals there are now
SetEntityLOD : SetEntityLOD ( e [, lod] ) -- the entity draws at LOD lod, 0 (full detail) to 3, whatever its distance; -1 or left out chooses by distance again. Only a model with LOD levels (its _lod.dbo) has any to force; past its last it draws its last. Until the level ends
GetEntityLOD : lod, levels = GetEntityLOD ( e ) -- the LOD the entity draws at now, 0 (full detail) to 3, and how many LOD levels its model has past full detail (0 without a _lod.dbo, and then lod is always 0); chosen each frame by distance (SetLODMultiplier) unless SetEntityLOD forced one
SetEntityShaderParam : SetEntityShaderParam ( e, n, value ) -- a custom shader's parameter n (1 to 7, as the .fpe's shader parameters); only materials with a custom shader have them. The model's materials are shared, so every entity using the model changes; back to the model's own at the next level or test game end
SetEntityTextureScroll : SetEntityTextureScroll ( e, du, dv ) -- the entity's textures scroll by du, dv texture widths a second, the same at any frame rate (water in a channel, a conveyor belt); 0, 0 stops them where they are. Every entity using the model scrolls; they go back to where they started at the next level or test game end
SetEntityReceivesDecals : SetEntityReceivesDecals ( e, on ) -- 0: the entity's model takes no projected decals (characters, vehicles passing over a scorch); every entity using that model changes with it
SetEntityInstanceEmissive : SetEntityInstanceEmissive ( e, r, g, b [, strength] ) -- glow of this one instance, multiplying the entity's authored emissive colour and strength (each 0 to 1; 1,1,1,1 = as authored, strength 0 = off). Other instances of the same object keep theirs
GetEntityInstanceEmissive : r, g, b, strength = GetEntityInstanceEmissive ( e )

SetTreeWind : SetTreeWind ( amount [, speed] ) -- how far trees and grass sway in the wind, and how fast (speed 0 or omitted ties the speed to the amount)
GetTreeWind : amount, speed = GetTreeWind()
SetTreeSubsurface : SetTreeSubsurface ( v ) -- how brightly the sun shines through the leaves of trees seen against it (0 to 1, the editor's Subsurface Scattering slider), full-detail trees and billboards; kept through visuals pushes, the level's own again at the next level and when a test game ends
GetTreeSubsurface : v = GetTreeSubsurface ( )

GetEntityLimbCount : n = GetEntityLimbCount ( e ) -- number of limbs of the entity's object; limbs are numbered 0 to n-1 as GetLimbName numbers them
GetEntityLimbName : name = GetEntityLimbName ( e, limb ) -- "" if out of range
SetEntityLimbRotation : SetEntityLimbRotation ( e, limb, ax, ay, az ) -- turns one limb (degrees, in its own space) on top of its pose in the model: rotors, wheels, turrets. A playing animation that drives the limb overwrites it; a mesh skinned to bones does not move (turn its bone, see GetEntityLimbBone)
GetEntityLimbRotation : ax, ay, az = GetEntityLimbRotation ( e, limb )
SetEntityLimbOffset : SetEntityLimbOffset ( e, limb, x, y, z ) -- moves one limb in its own space (suspension, recoil)
GetEntityLimbOffset : x, y, z = GetEntityLimbOffset ( e, limb )
SetEntityLimbPivot : SetEntityLimbPivot ( e, limb, x, y, z ) -- the point in the limb's own space that its rotation turns about (default 0,0,0, the limb's origin)
GetEntityLimbPivot : x, y, z = GetEntityLimbPivot ( e, limb )
GetEntityLimbBounds : minx, miny, minz, maxx, maxy, maxz = GetEntityLimbBounds ( e, limb [, children] ) -- the box of the limb's mesh in its own space; children 1 also takes in its child limbs' meshes. Nothing for a limb without a mesh
GetEntityLimbParent : limb = GetEntityLimbParent ( e, limb ) -- the limb this one hangs from, -1 at the top
GetEntityLimbBoneCount : n = GetEntityLimbBoneCount ( e, limb ) -- how many bones the limb's mesh is skinned to, 0 if it is not skinned
GetEntityLimbBone : limb, name = GetEntityLimbBone ( e, limb, n ) -- the limb that is bone n (0 to count-1) of the limb's skinned mesh, and its name; -1, "" if none
SetEntityLimbAlpha : SetEntityLimbAlpha ( e, limb, percent [, children] ) -- fades one limb, 0 to 100 (100 = as made); children 1 also the limbs under it (a rotor frame with blade limbs). The renderer dithers it rather than blending, and a faded limb keeps its shadow, so hide it once faded out with HideLimb(e, limb + 1) (HideLimb counts limbs from 1, these calls from 0). Back to 100 at each level start
GetEntityLimbAlpha : percent = GetEntityLimbAlpha ( e, limb ) -- the limb's alpha, or that of the first limb under it with a mesh; 100 if none

IntersectRay : obj, e, x, y, z, nx, ny, nz, limb = IntersectRay ( x1, y1, z1, x2, y2, z2 [, ignore [, flags] ] ) -- one full-accuracy pick with all its results: obj 0 none, -1 terrain or other geometry; e 0 if not an entity; limb -1 none. ignore is an object number or a table of them; flags 1 hits the ground first, 2 passes through entities with collisionmode 11
RayRegionBegin : count = RayRegionBegin ( x, y, z, radius ) -- lists the objects whose bounds reach the sphere, for RayRegion until the end of this frame; returns how many. For many short rays near one place (rings, a car's probes)
RayRegion : obj, e, x, y, z, nx, ny, nz, limb = RayRegion ( x1, y1, z1, x2, y2, z2 [, ignore] ) -- a full-accuracy pick against RayRegionBegin's objects only, with IntersectRay's returns; no terrain. Hits nothing outside the frame of RayRegionBegin. ignore is an object number or a table of them
RayRegionEnd : RayRegionEnd ( ) -- drops RayRegionBegin's list early (it lapses at the end of the frame anyway)
GetIntersectCollisionLimb : limb = GetIntersectCollisionLimb() -- the limb the last IntersectAll / IntersectStatic hit, -1 for none or the ground
GetRayNormalX : nx = GetRayNormalX() -- the surface normal where the last RayTerrain hit (the ground or a tree trunk)
GetRayNormalY : ny = GetRayNormalY()
GetRayNormalZ : nz = GetRayNormalZ()

AddBulletHole : ok = AddBulletHole ( x, y, z, nx, ny, nz [, material [, e] ] ) -- leaves a bullet hole where a script's shot hit, as gunfire does: nx, ny, nz the surface normal, material the materialindex (1 stone by default, 0 none), e the entity hit (only static ones, or ones with Allow Bullet Holes). Returns 1 if added
BulletRay : hit, e, x, y, z, nx, ny, nz, material, hole = BulletRay ( x1, y1, z1, x2, y2, z2 [, ignoreobj [, holes [, terrainmaterial] ] ] ) -- one round from a script weapon, detected as the player's rounds are. hit 0 nothing, 1 terrain, 2 entity, 3 other object, 4 passed through, 5 tree. holes 1 leaves the hole the player's gun would
GetPlayerHitSeq : seq = GetPlayerHitSeq() -- the newest entry in the player's hit record, 0 at level start
GetPlayerHit : hit, e, x, y, z, nx, ny, nz, material, hole, limb, damage, healthbefore, healthafter, killed, kind, shot, gunid, ox, oy, oz = GetPlayerHit ( seq ) -- one of the player's last 128 rounds (kind 1), pellets (2), melee strikes (3) or blast hits (4); nothing once it has left the record
g_PlayerGunShotThisFrame -- the rounds the player's gun fired since the last frame (dry clicks and melee strikes are not rounds)

WParticleEffectSetKillBox : WParticleEffectSetKillBox ( id, slot, minx, miny, minz, maxx, maxy, maxz ) -- slot 1 to 8: the effect's particles inside the world box are removed (rain indoors); ( id, slot ) alone clears the slot
WParticleEffectClearKillBoxes : WParticleEffectClearKillBoxes ( id )
WParticleEffectSetOpacity : WParticleEffectSetOpacity ( id, percent ) -- 100 = as made
WParticleEffectGetOpacity : percent = WParticleEffectGetOpacity ( id )
WParticleEffectSetSize : WParticleEffectSetSize ( id, percent ) -- particle size, 100 = as made
WParticleEffectGetSize : percent = WParticleEffectGetSize ( id )
WParticleEffectSetColor : WParticleEffectSetColor ( id, r, g, b ) -- 0 to 255 each, multiplying the effect's own colour (255,255,255 = as made)
WParticleEffectGetColor : r, g, b = WParticleEffectGetColor ( id )
WParticleEffectGetBounds : minx, miny, minz, maxx, maxy, maxz = WParticleEffectGetBounds ( id ) -- the world box the effect's particles are born in, as of the last frame
EffectGetOpacity : percent = EffectGetOpacity ( e ) -- a placed particle entity's opacity, as EffectSetOpacity set it

SetEntityInstanceTint : SetEntityInstanceTint ( e, r, g, b ) -- colour multiplier for this one instance, 0 to 1 each (1,1,1 = as made). Other instances of the same object keep theirs
GetEntityInstanceTint : r, g, b = GetEntityInstanceTint ( e )
GetEntityImmunity : n = GetEntityImmunity ( e ) -- 0 normally; -1 while SetEntityHealth(e,-99999) holds its health up, or the frames left of the brief immunity after a resurrection. While it is not 0, SetEntityHealth(e,0) does nothing

SetGrassKillBox : SetGrassKillBox ( slot, x, y, z, halfx, halfy, halfz [, yaw [, trees] ] ) -- slot 1 to 8: no grass is drawn with its root inside the box (turned by yaw degrees about Y, as an object's angle Y), e.g. under a vehicle; set it every frame to follow one. Cleared at level start. trees 1 (a blast, not a vehicle; 0 when left out) also removes every tree whose trunk reaches into the box, for the rest of the level: they stay gone when the box moves or is cleared
SetGrassShade : SetGrassShade ( root [, tip] ) -- a grass blade's light at its root and at its tip (0 to 2), lerped up the blade and multiplied into its lit colour; tip left out is root. 0.45 both is how the grass has always been drawn (the engine's height term was always 0), 0.45, 1.05 the ramp it was written with; back to 0.45 at level start and test game end
GetGrassShade : root, tip = GetGrassShade ( )
GetTextureSource : path, changed = GetTextureSource ( kind, index [, part] ) -- the file a vegetation or terrain texture was last uploaded from this session, as the engine resolved it (a Documents copy ahead of the install's). kind "grass" (index the grass slice, 0 to 45), "tree" (index the tree type as GetTreesNear gives it; part "billboard", the default, "billboardnormal", "trunk" or "leaves") or "terrain" (index the material slot, 0 to 31; part "color", the default, "normal" or "surface"). path is "" when nothing was loaded there; changed: 0 the same file would load now, unchanged; 1 it was written since; 2 another file would load now (a Documents copy added or removed); 3 it is gone. Anything but 0 means a restart would load something else. Nothing is returned for a bad kind, index or part
ClearGrassKillBox : ClearGrassKillBox ( [slot] ) -- without a slot clears them all

GetVideoMemoryUsed : usedMB, budgetMB, sharedUsedMB, sharedBudgetMB = GetVideoMemoryUsed() -- the dedicated video memory the game uses, and the budget Windows gives it; then the shared system memory the card uses and its budget, which grows when the dedicated memory nears its budget and Windows moves resources out (slow: the early warning of running out)
SetGamePaused : SetGamePaused ( on ) -- 1 pauses the game world for a menu as the in-game menu does (physics, the player, entities and their animation, AI, weapons, projectiles, particles and the game's sounds) while every script, the HUD, sprites, music and rendering carry on and new sounds play; g_Time holds still, g_TimeElapsed and GetTimeElapsed() read 0, Timer() and GetElapsedTime() are the real clock. 0 resumes; a level that ends resumes it
GetGamePaused : paused = GetGamePaused() -- 1 while SetGamePaused holds the world
SetEscapeMenu : SetEscapeMenu ( on ) -- 0 leaves Escape and a controller's START to the scripts, for a pause menu of their own (read it with GetKeyState(1)): it no longer opens the in-game menu or leaves a test level, though shift and Escape still leave a test level. 1 is the usual; back to 1 at each level start
QuitGame : QuitGame() -- exit to Windows; called from a level it ends the level first, and a test level goes back to the editor
LeaveGame : LeaveGame() -- leave the level for the title page (the in-game menu's Main Menu); a test level goes back to the editor
GetDrawCalls : main, shadow, transparent, triangles = GetDrawCalls() -- the last frame's draw calls and the main pass's triangles
SetRenderTiming : SetRenderTiming ( mode ) -- 1 times the renderer's work every frame so GetRenderTime can read it, 2 the CPU ranges only (no GPU timestamp queries, no GPU times), 0 stops; off again at the next level
GetRenderTime : ms, stale = GetRenderTime ( [name] ) -- milliseconds, averaged over 20 frames, of a timed range: "GPU Frame" (the default), "CPU Frame" or another name from Tab Tab's performance data; while timing is off, the main thread's CPU ranges ("CPU Frame", "Update - Logic - LUA", ...) are still answered, from the frame phase timing, and GPU ranges are -1; -1 also for the first frames after it is turned on; stale: how many frames the time has gone without a new GPU result (0 fresh), as results arrive late when the GPU runs far behind
GetEngineProbe : ms, calls, longest = GetEngineProbe ( name ) -- the engine time of calls made many times a frame: milliseconds and calls a frame averaged over the last 20 frames, and the longest single call among them. Names: "pick", "pick layers", "pick wicked", "pick lookup", "decal create", "decal fade" (main thread), and "thread pick" (the extra logic thread's picks), "thread frame" (its whole cycle), "texture load", "object add", "gpu create" (a GPU buffer or texture made), "gpu map" (a map, unmap or buffer update), "present" (any thread), "profiler queries" (the profiler's GPU queries a frame, as the value), "profiler lock" (waits for the profiler's lock), "profiler hold" (how long each holder kept it), "gpu shader" (a shader, pipeline state or sampler made), "frame phase" (the main thread's steps, timing on or off: each named range's own time, so the longest is the slowest step), "job stall" (each engine job that waited in the queue or ran 20 ms or more, on any thread: its run time)
GetEngineProbeDetail : text = GetEngineProbeDetail ( name [, ms] ) -- for "gpu create", "gpu map", "present", "gpu shader" or "profiler hold", the slowest call in the last 20 frames that took ms or more (50 by default, at least 10): what it was, how long, the thread (main, extra or worker) and its callers by name; for "frame phase" without ms the slowest step over 50 ms, "<step> N ms, in <parents>" (parents innermost first), and with ms the slowest frame of ms or more with its three longest steps, "frame N ms: <step> A ms, in <parents>; <step> B ms; ...", then its three dearest entity turns in the Lua loop from 0.5 ms, "[lua: e=N <script> T ms, ...]" ("(gc)" on a turn in which a Lua collection step ran); for "job stall" the slowest job of ms or more, "<function> lambda_N ran A ms (queued B ms), <main, extra or worker> thread[, in a wait on its own context or another], N jobs, core C to D" (each also goes in the engine log as a "Job stall:" line); "" if none
SetPlayerHealth : SetPlayerHealth ( v ) -- sets the player's health; taking it from above 0 to 0 or below starts the death sequence and the respawn, as damage does

***** The following five functions return multiple values, if you do not need them all just replace 
***** the ones you don't need with '_' for example : _, _, _, Ax, Ay, Az = GetEntityPosAng( e ) would
***** just give you last three of the 6 values returned
GetObjectPosAng : x, y, z, Ax, Ay, Az = GetObjectPosAng( obj ) -- returns position and Euler angles of object
GetEntityPosAng : x, y, z, Ax, Ay, Az = GetEntityPosAng( e )   -- returns position and Euler angles of entity
GetObjectScales : xs, ys, zs = GetObjectScales( obj ) -- returns scale values of object in all axis (Note: uses object id not entity!)
GetEntityWeight : weight = GetEntityWeight( e ) -- returns the Physics weight value of the entity

SetEntityAlwaysActive : SetEntityAlwaysActive( e, flag ) -- sets the entity always active flag
GetEntityAlwaysActive : flag = GetEntityAlwaysActive( e ) -- returns 1 if the entity is set to always active

***** Collision box is defined by coordinates of two opposing corners, from these it is easy to calculate the size of the object
GetObjectColBox : xmin, ymin, zmin, xmax, ymax, zmax = GetObjectColBox( obj ) -- returns collision cube of object

***** Object center commands, from both the regular bounds and the actual model collision center that ignores any offsetting 
GetObjectCentre : x, y, z = GetObjectCentre( obj ) -- returns actual center of object
GetObjectColCentre : x, y, z = GetObjectColCentre( obj ) -- returns real collision center of object (middle of the visible model mesh)

***** Lua control of dynamic light, you get the light number using entity e number then use that in the other light functions
***** for example; lightNum = GetEntityLightNumber( e )  then  x, y, z = GetLightPosition( lightNum )
GetEntityLightNumber : lightNum = GetEntityLightNumber( e ) -- returns the internal light number held by the entity (a light marker's, or the light an entity carries when its .fpe has entitylight = 1: the engine places that one with the entity every frame, so don't set its position; 0 if none)
GetLightPosition : x, y, z = GetLightPosition( lightNum ) -- returns the XYZ position of the dynamic light specified
GetLightAngle : xv, yv, zv = GetLightAngle( LightNum ) -- returns the angle vector of the dynamic light specified
GetLightRGB : r, g, b = GetLightRGB( lightNum ) -- returns the RGB color of the dynamic light specified
GetLightRange : range = GetLightRange ( lightNum ) -- returns the range value of the dynamic light specified
SetLightPosition : SetLightPosition ( lightNum, x, y, z ) -- sets the new position of the specified dynamic light
SetLightAngle : SetLightAngle( lightNum, xv, yv, zv ) -- set a vector (-1 to 1) to each angle which is then converted to 0-360
SetLightRGB : SetLightRGB ( lightNum, r, g, b ) -- sets the new color of the specified dynamic light
SetLightShadow : SetLightShadow ( lightNum, on ) -- whether the specified light casts shadows (1 or 0); shadows are the costliest part of a point or spot light, so turn them off on lights that don't need one
SetLightRange : SetLightRange ( lightNum, range ) -- sets the new range (1 to 10000) of the specified light
GetLightEuler : x, y, z = GetLightEuler ( lightNum ) -- get the euler angles from the light direction
SetLightEuler : SetLightEuler( lightNum, x, y, z ) -- set euler angles for the light direction

***Water Shader Settings*** Look into the shader for informations about these values(open effectbank/reloaded/water_basic.fx with i.e. notepad)
SetWaterHeight(value) -- sets water setting attributes
SetWaterColor(red,green,blue) -- sets water setting attributes
SetWaterWaveIntensity(value) -- sets water setting attributes
SetWaterTransparency(value) -- sets water setting attributes
SetWaterReflection(value) -- sets water setting attributes
SetWaterReflectionSparkleIntensity(value) -- sets water setting attributes
SetWaterFlowDirection(x,y,speed) small tip: -1 =east/north; 1=west/south - speed is a multiplier of the flowdirection(higher => fast flow)
SetWaterDistortionWaves(value) -- sets water setting attributes
SetRippleWaterSpeed(value) -- sets water setting attributes
GetWaterHeight() -- gets water setting attributes
GetWaterHeightAt : height, isRiver = GetWaterHeightAt ( x, z ) -- the water surface's height at a point: a river's (the editor's Roads and Rivers splines) where one flows over it, else the level's water line; isRiver 1 for a river, 0 for the sea
GetRiverTurbulenceAt : turbulence = GetRiverTurbulenceAt ( x, z ) -- how rough a river's water is at a point (the editor's Roads and Rivers splines): 0 smooth, or no river above the sea there, to 1 rapids where its slope changes sharply
GetWaterFlowAt : flowX, flowZ, speed = GetWaterFlowAt ( x, z ) -- a river's current at a point (the editor's Roads and Rivers splines): its x and z in units a second downstream, and its speed; 0, 0, 0 where no river flows above the sea
GetWaterWaveIntensity() -- gets water setting attributes
GetWaterShaderColorRed() -- gets water setting attributes
GetWaterShaderColorGreen() -- gets water setting attributes
GetWaterShaderColorBlue() -- gets water setting attributes
GetWaterTransparency() -- gets water setting attributes
GetWaterReflection() -- gets water setting attributes
GetWaterReflectionSparkleIntensity() -- gets water setting attributes
GetWaterFlowDirectionX() -- gets water setting attributes
GetWaterFlowDirectionY() -- gets water setting attributes
GetWaterFlowSpeed() -- gets water setting attributes
GetWaterDistortionWaves() -- gets water setting attributes
GetRippleWaterSpeed() -- gets water setting attributes

** Object Relationship Commands
GetEntityMarkerMode(e) -- get the marker mode value
SetEntityAllegiance(e) -- set the allegiance value for this object ("0=Enemy", "1=Ally", "2=Neutral")
GetEntityAllegiance(e) -- get the allegiance value for this object ("-1=None", "0=Enemy", "1=Ally", "2=Neutral")
GetEntityPatrolMode(e) -- get the patrol mode for this object
GetEntityRelationshipID(e,i) -- get one of ten other connected entID
GetEntityRelationshipType(e,i) -- get the type of connection for an entID
GetEntityRelationshipData(e,i) -- get the data specified in the UI for the connection for an entID
GetEntityCanFire(e) -- can the character fire a weapon right now
GetEntityHasWeapon(e) -- returns the WeaponID of any weapon assigned to this object
GetEntityViewAngle(e) -- get the cone angle of the object
GetEntityViewRange(e) -- get the cone range of the object
GetEntityMoveSpeed(e) -- get the move speed of the object
GetEntityTurnSpeed(e) -- get the turn speed of the object
SetEntityMoveSpeed(e) -- set the move speed of the object
SetEntityTurnSpeed(e) -- set the turn speed of the object

GetNearestEntityDestroyed : e = GetNearestEntityDestroyed(mode) -- returns the closest entity that was destroyed. Mode reserved.

** New AI SOund Commands
GetNearestSoundDistance : distance, whoe = GetNearestSoundDistance(x,y,z,category) -- returns the closest distance greater than zero if a sound heard made by category of game architype
MakeAISound : MakeAISound(x,y,z,radius,category,e) -- make a sound with a specified category of game architype (0-neutral, 1-enemy, 2-player)

** New Terrain Commands
GetTerrainEditableArea(dimension) : size = GetTerrainEditableArea() -- returns the editable size of the terrain. Dimesion 0,1,2,3 correspond to minx,minz,maxx,maxz

** AmenMoses Notes On Additional Advanced Commands
GetLimbName( objectId, limbId )
	-- returns a string containing the limb name for the specified limbId or nil if no limb found.

***** Physics functions (Use physlib.lua functions to simplify use of most of these functions!)
***** These descriptions are for those who are familiar with the Bullet Physics Engine
PushObject( objectId, vx, vy, vz, rx, ry, rz )
	-- Applies a force to an object, vx, vy, vz is a vector defining the direction and amplitude
	--                               rx, ry, rz is the relative offset on the object where the force is applied

ConstrainObjMotion( objectId, cx, cy, cz )
	-- Constrains an objects motion, cx, cy, cz are the constraint values on each world axis
	-- the simplest use of this is with values of 0 or 1, i.e a value of 1 is free motion on that axis
	-- a value of 0 doesn't allow any motion on that axis 

ConstrainObjRotation( objectId, rx, ry, rz )
	-- Constrains an objects rotation, rx, ry, rz are the constraint values on each world axis
	-- the simplest use of this is with values of 0 or 1, i.e a value of 1 is free rotation on that axis
	-- a value of 0 doesn't allow any rotation on that axis 

SetObjectDamping( objectId, mv, rv )
	-- Applies a damping amount to an object (drag iow), mv is motion damping amount, rv is rotation damping amount
	-- a value of 0 is no damping, 1 is maximum damping
	
SetBodyScaling( objectId, sx, sy, sz ) 
	-- Scales an objects collision shape, sx, sy, sz are the scaling factors in each axis.
	-- for example values of 0.95 would make the objects collision shape 95% of its visible shape.

PhysicsRayCast( fromX, fromY, fromZ, toX, toY, toZ, force )
	-- Works the same way as the normal ray casting except only applies to dynamic physics objects
	-- Returns 4 values: objectId of ray hit and x, y, z location of the collision point
	-- if the 'force' value is not 0 then the obnject hit will also get 'pushed' in the direction of the ray


***** Physics constraint creation functions
CreateSingleHinge( objectId, x, y, z, hingeType, minAng, maxAng )
	-- Creates a physics 'hinge' constraint between the object and the world
	-- returns an identifier of the constraint (constId) or -1 if constraint creation fails

CreateSingleJoint( objectId, x, y, z )
	-- Creates a physics 'joint' constraint between the object and the world
	-- returns an identifier of the constraint (constId) or -1 if constraint creation fails

CreateDoubleHinge( objectIdA, objectIdB, xa, ya, za, xb, yb, zb, hingeTypeA, hingeTypeB, noCols )
	-- Creates a physics 'hinge' constraint between two objects
	-- returns an identifier of the constraint (constId) or -1 if constraint creation fails

CreateDoubleJoint( objectIdA, objectIdB, xa, ya, za, xb, yb, zb, noCols )
	-- Creates a physics 'joint' constraint between two objects
	-- returns an identifier of the constraint (constId) or -1 if constraint creation fails

CreateSliderDouble( objectIdA, objectIdB, qXa, qYa, qZa, qWa, xa, ya, za,
                                          qXb, qYb, qZb, qWb, xb, yb, zb, 
										  useLinearReferenceFrameA )
	-- Creates a physics 'slider' constraint between two objects
	-- returns an identifier of the constraint (constId) or -1 if constraint creation fails

RemoveObjectConstraints( objectId )
	-- Removes all physics constraints from an object

RemoveConstraint( constId )
	-- Remove a specified constraint

***** Physics constraint control functions
SetHingeLimits( constId, minAng, maxAng, softness, bias, relaxation )
	-- Changes the values for a specified hinge constraint

GetHingeAngle( constId )
	-- returns the current angle value of a hinge constraint in radians

SetHingeMotor( constId, speed, angle, force )
	-- Set a motor for a specified hinge constraint

SetSliderMotor( constId, OnOff, force, velocity )
	-- Set a motor for a specified slider constraint

GetSliderPosition( constId )
	-- returns the current position value of a slider constraint

SetSliderLimits( constId, lowerLimit, upperLimit, lowerAngle, upperAngle  )
	-- Set the limits on a slider constraint

***** Physics Collision detection functions (use the physlib functions to access these as they are more useful) 
AddObjectCollisionCheck( objectId )

RemoveObjectCollisionCheck( objectId ) 

GetObjectNumCollisions( objectId )
	-- returns the number of colisions with other objects that were detected for an object since the last call
	
GetObjectCollisionDetails( objectId, num )
	-- returns the specified collision details or the first one if num not specified:
	-- objectId - the  object it hit
	-- x, y, z  - world position of the collision 
	-- force    - how hard the two objects collided (this is a very small number usually)

GetTerrainNumCollisions( objectId )
	-- returns the number of colisions with the terrain were detected for an object since the last call

GetTerrainCollisionDetails( objectId, num )
	-- returns the specified collision details or the first one if num not specified:
	-- latest   - flag indicating if this was the most recent collision
	-- x, y, z  - world position of the collision 

--]]

-- New Wicked Particle commands
-- EffectID = WParticleEffectLoad("FileName") -- Load a particle effect and return EffectID
-- WParticleEffectPosition(EffectID,x,y,z) -- Position the particle effect.
-- WParticleEffectVisible(EffectID,0) -- Hide or show the particle effect (0 = Hide , 1 = Show)
-- WParticleEffectAction(EffectID,Action) -- Perform actions on the particle effect (1 = Burst all. 2 = Pause. 3 = Resume. 4 = Restart)
-- DisableBoundHudKeys() -- Disable HUD keys
-- EnableBoundHudKeys() -- Enable HUD keys
-- px,py,pz,dx,dy,dz = Convert2DTo3D(ScreenPercentX,ScreenPercentY) -- px,py,pz = 3D Position. dx,dy,dz = 3D Direction for use with IntersectAll or similar calls.
-- ScreenPosX,ScreenPosY = Convert3DTo2D(x,y,z) -- Convert a 3D positon into 2D screen positions.
-- ScreenPercentX,ScreenPercentX = ScreenCoordsToPercent(ScreenPosX,ScreenPosY) -- Convert 2D screen positions into screen percentage position.
-- LoadTracerImage("FileName",ImageID) -- Load a .dds tracer image into ImageID (0-99) for AddTracer. Load it before use: a tracer whose ImageID has no image draws nothing.
-- AddTracer(x,y,z,tx,ty,tz,lifetime,r,g,b,glow,scrollv,scalev,width,maxlength,ImageID) -- Draw a bullet tracer from x,y,z to tx,ty,tz (all 16 values are needed). lifetime in seconds, r,g,b 0-255, glow brightens it, scrollv and scalev scroll and scale the image along it, width in world units, maxlength the length of the streak that travels from start to end over the lifetime (0 = the whole line at once, fading), ImageID from LoadTracerImage. The stock guns use 0.12, 255,128,25, 10, 0, 1.0, 1.2, 100.

-- Storyboard commands
-- SetScreenHUDGlobalScale : SetScreenHUDGlobalScale
-- InitScreen : InitScreen
-- DisplayScreen : DisplayScreen
-- DisplayCurrentScreen : buttonid = DisplayCurrentScreen() -- returns the ID of any HUD button pressed that has the 'return button ID to LUA' set
-- GetCurrentScreen : GetCurrentScreen
-- GetCurrentScreenName : GetCurrentScreenName
-- CheckScreenToggles : CheckScreenToggles
-- ScreenToggle : ScreenToggle ( HUD Screen Name ) -- switch to the specified HUD screen
-- ScreenToggleByKey : ScreenToggleByKey ( HUD Screen Toggle Key ) -- switch to the specified HUD screen
-- GetStoryboardActive : GetStoryboardActive
-- GetScreenWidgetValue : GetScreenWidgetValue
-- SetScreenWidgetValue : SetScreenWidgetValue
-- SetScreenWidgetSelection : SetScreenWidgetSelection
-- GetScreenElementsType : GetScreenElementsType
-- GetScreenElementTypeID : GetScreenElementTypeID
-- GetScreenElements : GetScreenElements
-- GetScreenElementID : GetScreenElementID
-- GetScreenElementArea : GetScreenElementArea
-- GetScreenElementDetails : GetScreenElementDetails
-- GetScreenElementName : GetScreenElementName
-- SetScreenElementVisibility : SetScreenElementVisibility
-- SetScreenElementPosition : SetScreenElementPosition
-- SetScreenElementText : SetScreenElementText
-- SetScreenElementColor : SetScreenElementColor ( elementID, r, g, b, a )
-- GetCollectionAttributeQuantity : GetCollectionAttributeQuantity
-- GetCollectionAttributeLabel :, GetCollectionAttributeLabel
-- GetCollectionItemQuantity : GetCollectionItemQuantity
-- GetCollectionItemAttribute : GetCollectionItemAttribute
-- GetCollectionQuestAttributeQuantity : totalattribs = GetCollectionQuestAttributeQuantity() -- returns the total number of quest attribute labels used
-- GetCollectionQuestAttributeLabel : value = GetCollectionQuestAttributeLabel(attribindex) -- returns the label string stored at this position in the attribute list
-- GetCollectionQuestQuantity : totalquests = GetCollectionQuestQuantity() -- returns total number of quests in the game
-- GetCollectionQuestAttribute : value = GetCollectionQuestAttribute(questindex,"attrib") -- returns the value stored in the attrib label of the specified quest

-- MakeInventoryContainer : containerindex = MakeInventoryContainer(inventory container name) -- creates a new inventory container
-- GetInventoryTotal : quantity = GetInventoryTotal() -- returns to total number of inventory slots available
-- GetInventoryName : string = GetInventoryName(index) -- returns the name of the inventory container at specified index
-- GetInventoryExist : value = GetInventoryExist(index) -- returns one if the inventory container exists at specified index (HUD "HUD Screen 2" neds to exist)
-- GetInventoryQuantity : quantity = GetInventoryQuantity(inventory container name) -- returns the number of items inside the specified inventory container)
-- GetInventoryItem : collectionItemID = GetInventoryItem(inventory container name,index) -- returns the ID of the item at the specified index of the container
-- GetInventoryItemID : entityelementID = GetInventoryItemID(inventory container name,index) -- returns the Entity ID of the item at the specified index of the container
-- GetInventoryItemSlot : slotindex = GetInventoryItemSlot(inventory container name,index) -- returns the Slot Index of the item at the specified index of the container
-- SetInventoryItemSlot : SetInventoryItemSlot(inventory container name,index,newslot) -- sets a new slot index for the item at the specified index of the container
-- MoveInventoryItem : MoveInventoryItem ( container name from, container name to, collectionID, entityelementID, slot index ) -- moves an item across containers
-- DeleteAllInventoryContainers : DeleteAllInventoryContainers() -- deletes ALL inventory containers - destructive!
-- AddInventoryItem : AddInventoryItem ( container name, collectionID, newe, slot index ) -- adds a new item of collection and entityelement value to specified slot in the container
