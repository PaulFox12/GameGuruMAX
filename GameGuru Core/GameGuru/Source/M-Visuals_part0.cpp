//----------------------------------------------------
//--- GAMEGURU - M-Visuals
//----------------------------------------------------

// Includes
#include "stdafx.h"
#include <set>       // GGMAX 3.82
#include <string>    // GGMAX 3.82
// GGMAX 3.82: every visuals key the file being parsed STATED EXPLICITLY. An absent key and a
// key set to its default are not the same thing, and the old-file downgrade below has to be
// able to tell them apart.
static std::set<std::string> g_ggVisualsKeysSeen;
#include "gameguru.h"

#include "GGTerrain/GGTerrain.h"
#include "GGTerrain/GGGrass.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

// 
//  VISUALS MODULE
// 

namespace GGGrass
{
	extern struct GGGrassParams gggrass_global_params;
	extern struct GGGrassParams gggrass_save_params;
}

void visuals_init ( void )
{
	//  Default visual settings
	t.visuals.shaderlevels.terrain=1;
	t.visuals.shaderlevels.entities=1;
	t.visuals.shaderlevels.vegetation=1;
	t.visuals.shaderlevels.lighting=2;

	//  Global settings
	t.visuals.mode=1;

	//  Reset to default values
	visuals_resetvalues ( );

	//  Graphic Options
	t.visuals.reflectionmode=99;
	t.visuals.shadowmode=100;
	t.visuals.lightraymode=0;
	t.visuals.vegetationmode=50;
	t.visuals.occlusionvalue=100;
	t.visuals.debugvisualsmode=0;

	//  Post Effects
	t.visuals.bloommode=50;
	t.visuals.VignetteRadius_f=1.0;
	t.visuals.VignetteIntensity_f=1.0;
	t.visuals.MotionDistance_f=0.5;
	t.visuals.MotionIntensity_f=0;
	t.visuals.DepthOfFieldDistance_f=0.5;
	t.visuals.DepthOfFieldIntensity_f=0;
	t.visuals.LightrayLength_f=0.55f;
	t.visuals.LightrayQuality_f=20.0f;
	t.visuals.LightrayDecay_f=0.8f;
	t.visuals.SAORadius_f=0.5f;
	t.visuals.SAOIntensity_f=0.5f;
	t.visuals.SAOQuality_f=1.0f;
	t.visuals.LensFlare_f=0.5f;

	//  Trigger all visuals to update
	t.visuals.refreshvegetation=0;
	t.visuals.refreshshaders=1;
	t.visuals.refreshskysettings=0;
	t.visuals.refreshterraintexture=0;
	t.visuals.refreshvegtexture=0;

	//  Load any previous global settings for in-game
	visuals_load ( );
	t.gamevisuals=t.visuals;

	//  And set editor defaults over the loaded data
	t.editorvisuals=t.visuals ; visuals_editordefaults ( );
	if (t.visuals.reflectionmode == 0)
		t.visuals.reflectionmode = 1;
}

void visuals_calcsunanglefromtimeofday(int iTimeOfday, float* pfSunAngleX, float* pfSunAngleY, float* pfSunAngleZ)
{
	const float time_value[] = { 6.3, 9, 12,15, 18, 20,0 };
	float fCurrentTimeAngleX = 45.0;
	float time = time_value[iTimeOfday];
	if (time >= 12)
	{
		if (time > 15)
		{
			//PE: Sun goes down a little faster.
			fCurrentTimeAngleX = (time - 14) * (180.0 / 12.0);
		}
		else
			fCurrentTimeAngleX = (time - 12) * (180.0 / 12.0);
	}
	else 
	{
		fCurrentTimeAngleX = 180 + (time * (180.0 / 12.0));
	}
	*pfSunAngleX = fCurrentTimeAngleX;
	*pfSunAngleY = 270.0;
	*pfSunAngleZ = 0.0;
}

// GGMAX 3.25: one place that returns every Brutal off-switch to neutral, in the struct AND in
// the live engine flags. Called from both reset paths and therefore from "Reset Visuals" too.
void gg_visuals_reset_brutal_switches()
{
	extern bool gg_terrain_bake, gg_water_bake;
	extern void GGSetNoTreesLevel(int);
	extern void GGSetNoGrassLevel(int);
	extern void GGSetNoAOLevel(int);
	extern void GGSetNoShadowsLevel(int);
	extern void GGSetNoOcclusionLevel(int);
	extern void GGSetParticlePctLevel(int);
	extern void GGSetObjectCullDistLevel(int);
	extern void GGSetTextureDivideLive(int);

	// ⚠ Void any existing baked terrain EXPLICITLY. This function is the pre-parse reset on a
	// level load as well as the Reset Visuals button, and on load the parse can set the switch
	// straight back on inside the same call - so no frame ever observes it off, no edge fires,
	// and without this the PREVIOUS level's baked chunks would stay on screen.
	extern void GGTerrainBake_Clear();
	GGTerrainBake_Clear();
	// ⚠ ONLY CALL A SETTER IF THE LIVE VALUE IS ACTUALLY CHANGING.
	//
	// This function runs on EVERY level load (it is the pre-parse reset as well as the Reset
	// Visuals button), and the first version called every setter unconditionally - so each load
	// poked the tree pool, the grass system, the particle emitters and the object-cull scan even
	// when all of them were already neutral, which nothing did before 3.25. Trees and grass are
	// exactly the subsystems that move a demo's triangle count by millions, and Horseshoe Bend's
	// POLYS went from five consecutive sweeps at an identical 1,583,122 to landing anywhere
	// between 81,302 and 4,038,923 in the two sweeps after this shipped.
	//
	// A reset should be a no-op when there is nothing to reset. Guarding each call restores that
	// and costs one comparison. (Whether it is the whole story is being measured, not assumed -
	// see the note in NIGHT_INVESTIGATIONS.)
	extern bool gg_no_trees, gg_no_grass, gg_no_ao, gg_no_shadows, gg_no_occlusion;
	extern int gg_particle_pct;
	extern float gg_object_cull_dist;

	t.visuals.bTerrainBake      = false;  gg_terrain_bake = false;
	t.visuals.bWaterBake        = false;  gg_water_bake   = false;
	{ extern void GGWaterBake_ClearUnderwaterRequest(); GGWaterBake_ClearUnderwaterRequest(); }   // GGMAX 3.120
	t.visuals.bNoTrees          = false;  if (gg_no_trees)          GGSetNoTreesLevel(0);
	t.visuals.bNoGrass          = false;  if (gg_no_grass)          GGSetNoGrassLevel(0);
	t.visuals.bNoAO             = false;  if (gg_no_ao)             GGSetNoAOLevel(0);
	t.visuals.bNoShadows        = false;  if (gg_no_shadows)        GGSetNoShadowsLevel(0);
	t.visuals.bNoOcclusionCull  = false;  if (gg_no_occlusion)      GGSetNoOcclusionLevel(0);
	t.visuals.iParticlePct      = 100;    if (gg_particle_pct != 100) GGSetParticlePctLevel(100);
	// GGMAX 3.116: Grass Density - an old file has no key, so this IS its value (100% = DX11 density)
	{
		extern int GGGetGrassDensityLevel(); extern void GGSetGrassDensityLevel(int);
		t.visuals.iGrassDensityPct = 100;  if (GGGetGrassDensityLevel() != 100) GGSetGrassDensityLevel(100);
	}
	t.visuals.iObjectCullDist   = 0;      if (gg_object_cull_dist != 0.0f) GGSetObjectCullDistLevel(0);
	t.visuals.iTextureDivide    = 1;
	// GGMAX 3.28: 8 consecutive hidden frames before an object stops being drawn. Reset with
	// the other performance settings so Reset Visuals restores the measured default rather
	// than leaving whatever the previous level chose.
	t.visuals.iOcclusionCullDelay   = 8;
	t.visuals.iShadowResSteps       = 4;
	t.visuals.bSuperQuickObjects    = false;
	t.visuals.iSuperQuickLevel      = 3;   // GGMAX 3.35b: Lit - see the other reset path
	// GGMAX 3.25o: DEFAULT 25, not 1 (Lee's call on the measured curve). At 25 the skip holds
	// 93% of a scene's armatures per frame, against 88% at 10 - and 50 and 100 add about one
	// percent between them while the stutter on mid-distance characters keeps getting worse.
	// ⚠ This only bites while 'Lower Animation & LUA Speed' is ticked, so a level that never
	// used that box is unaffected. A level that DID have it ticked now gets real reduction
	// where it previously got none - that is the point of the change, but it is a behaviour
	// change for existing projects, not just a new default for new ones.
	// ★ Clear the animation-reduction "posed at least once" set. This function is the
	// pre-parse reset on every level load, so it is exactly the right place: an armature
	// carried over from a previous level must not be eligible for holding before it has
	// been posed in THIS one. Without that, culling and LOD read un-posed bounds while the
	// scene streams in and the level settles into a state built on them - the 3.25s defect.
	{ extern void gg_ResetAnimReductionBridge(); gg_ResetAnimReductionBridge(); }
	// DEFAULT 25 (Lee's call). Safe to default on now that a first pose is guaranteed.
	t.visuals.iAnimReductionScale = 25;
	// GGMAX 3.28: 8 consecutive hidden frames before an object stops being drawn. Stock Wicked
	// effectively used 32, which cannot be reached while the camera turns - so occlusion culling
	// did nothing at all during camera motion. Measured on TESTPRO2: 602 draws/frame rotating at
	// 32, 222 at 8, and 51 vs 10 parked.
	t.visuals.iOcclusionCullDelay = 8;
	// GGMAX 3.29: shadow atlas rects are sized from camera DISTANCE, and the shadow cache keys on
	// the rect - so continuous sizing re-renders every local shadow on every frame you walk.
	// Snapping to 4 steps cut the moving cost from 4.21 ms to 1.84 ms on TESTPRO2.
	t.visuals.iShadowResSteps = 4;
	t.visuals.bSuperQuickObjects = false;
	// ★ GGMAX 3.35b: which Super Quick rung. DEFAULT 3 (Lee's call), not 1.
	//
	// 3.34 defaulted to 1 (Flat, no texture fetch at all) reasoning that the box exists to buy
	// back frames and the user could climb back up from there. That is the wrong way round:
	// ticking a performance box and watching the level turn into untextured clay reads as
	// something BROKEN, not as a setting that worked.
	//
	// Rung 3 still drops the normal / ORM / emissive / specular maps, the decals, the ambient
	// occlusion and the reflections - measured at -33% on the opaque pass - and it still looks
	// like the game. Rungs 1 and 2 are one drag away for anyone who needs the rest of it.
	t.visuals.iSuperQuickLevel = 3;
	{
		extern int gg_terrain_bake_res_near;
		t.visuals.iTerrainBakeResNear = 8192;
		// GGMAX 3.91: Water Reflection Size 1024 + Blur 2 is now the DEFAULT, Lee's call after
		// judging the sweep by eye. 3.89/3.90 shipped inert at Auto/Off; this is the behaviour
		// change that turns them on.
		//
		// ⚠⚠ THIS FUNCTION RUNS PRE-PARSE ON EVERY LEVEL LOAD (called from visuals_load
		// below as well as from Reset Visuals), so these values apply to EVERY LEVEL THAT DOES
		// NOT STATE THE KEY - which is all existing content, the 19 shipped demos included.
		// A level saved since 3.89 writes the key and keeps whatever it chose, Auto included.
		//
		// Cost this accepts, per level: ~+26 MiB of reflection targets (1024x534 at 58 B/px
		// against 384x200), and every MIRROR surface softens, not only water - one shared
		// reflection target, so the 3.80 crate-and-puddle case is in scope too.
		// ⚠ Frame-time cost is UNMEASURED on a GPU-bound machine. The RX 9060 XT could not see
		// it (1.8 ms busy in a 4.6 ms frame, notes 3.90b); the 6-year-old AMD card this campaign
		// targets may well be able to. Re-measure there before this default is considered safe.
		t.visuals.iReflectionWidth = 1024;
		t.visuals.iReflectionBlur = 2;
		gg_terrain_bake_res_near = 8192;
	}
	// Texture Detail is deliberately NOT pushed live here. GGSetTextureDivideLive re-creates
	// every texture in the level, which on a reset that was already at Full is a multi-second
	// GPU stall for no change at all. It is pushed on LOAD (below) and by the panel.
}

void visuals_resetvalues (bool bNewLevel)
{
	// Visual Settings
	t.visuals.FogR_f=140;
	t.visuals.FogG_f=158;
	t.visuals.FogB_f=173;
	t.visuals.FogA_f=128;
	t.visuals.AmbienceIntensity_f=128.0f;

	t.visuals.FogNearest_f = 40000;
	t.visuals.FogDistance_f = 700000;
	t.visuals.AmbienceRed_f = 70;
	t.visuals.AmbienceGreen_f = 70;
	t.visuals.AmbienceBlue_f = 70;

	t.visuals.iPhysicsDebugDraw = 0;

	t.visuals.SurfaceIntensity_f=1.333;
	t.visuals.SurfaceRed_f=255;
	t.visuals.SurfaceGreen_f=255;
	t.visuals.SurfaceBlue_f=255;
	t.visuals.SurfaceSunFactor_f=1.0;
	t.visuals.Specular_f=1.0;
	t.visuals.DistanceTransitionStart_f=4000;
	t.visuals.DistanceTransitionRange_f=400;
	t.visuals.DistanceTransitionMultiplier_f=1;
	t.visuals.PostBrightness_f=0.0f;
	t.visuals.PostContrast_f=1.1666f;
	if (t.visuals.reflectionmode == 0)
		t.visuals.reflectionmode = 1;

	// defaults for occlusion, lightrays and water mode
	t.visuals.occlusionvalue = 100;
	t.visuals.LightrayLength_f=0.55f;
	t.visuals.LightrayQuality_f=20.0f;
	t.visuals.LightrayDecay_f=0.8f;
	t.visuals.SAORadius_f=0.5f;
	t.visuals.SAOIntensity_f=0.5f;
	t.visuals.SAOQuality_f=1.0f;
	t.visuals.LensFlare_f=0.5f;

	//  Camera settings
	t.visuals.CameraNEAR_f=DEFAULT_NEAR_PLANE;
	t.visuals.CameraFAR_f=DEFAULT_FAR_PLANE;
	t.visuals.CameraASPECT_f = 1920.0 / 1080.0; //PE: Fixed in wicked, or fov will be wrong on widescreen.
	t.visuals.CameraFOV_f = 60; //LB:Default now 90 degree (XM_PI/(fov/15.0f) was 45.0; //PE: Fixed in wicked.
	t.visuals.WeaponFOV_f = 60; //LB:Default now 90 degree (XM_PI/(fov/15.0f) was 45.0;
	t.visuals.CameraFOVZoomed_f=1.0;

	//  Quality Settings
	t.visuals.TerrainLOD1_f=4000.0;
	t.visuals.TerrainLOD2_f=5000.0;
	t.visuals.TerrainLOD3_f=5500.0;
	t.visuals.TerrainSize_f=100.0;
	t.visuals.VegQuantity_f=100.0;
	t.visuals.VegWidth_f=50.0;
	t.visuals.VegHeight_f=50.0;

	//  World Settings (reset to default selections)
	terrain_initstyles_reset ( );
	t.visuals.sky_s="";
	t.visuals.terrain_s=g.terrainstyle_s;
	t.visuals.vegetation_s=g.vegstyle_s;
	visuals_updateskyterrainvegindex ( );
	if (t.visuals.skyindex >= t.skybank_s.size()) t.visuals.skyindex = t.skybank_s.size() - 1;
	if (t.visuals.terrainindex >= t.terrainstylebank_s.size()) t.visuals.terrainindex = t.terrainstylebank_s.size() - 1;
	if (t.visuals.vegetationindex >= t.vegstylebank_s.size()) t.visuals.vegetationindex = t.vegstylebank_s.size() - 1;
	t.visuals.sky_s=t.skybank_s[t.visuals.skyindex];
	t.visuals.terrain_s=t.terrainstylebank_s[t.visuals.terrainindex];
	t.visuals.vegetation_s=t.vegstylebank_s[t.visuals.vegetationindex];
	t.strwork = ""; t.strwork = t.strwork +"visuals.sky$="+t.visuals.sky_s;
	timestampactivity(0,t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork +"visuals.terrain$="+t.visuals.terrain_s;
	timestampactivity(0,t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork +"visuals.vegetation$="+t.visuals.vegetation_s;
	timestampactivity(0,t.strwork.Get());

	float fWaterLine = t.terrain.waterliney_f;

	visuals_water_reset();

	//PE: Dont change water line as each biome is different.
	if (!bNewLevel)
		g.gdefaultwaterheight = t.terrain.waterliney_f = fWaterLine;

	t.visuals.CameraFOV_f = 60; //LB:Default now 90 degree (XM_PI/(fov/15.0f) was 45.0;
	t.visuals.WeaponFOV_f = 60; //LB:Default now 90 degree (XM_PI/(fov/15.0f) was 45.0;

	t.visuals.FogA_f = 0.0f; //PE: We use FogA as height in wicked.
	t.visuals.SkyIntensity_f = 1.0f;
	t.visuals.SunIntensity_f = 9.0f;
	t.visuals.SunRed_f = 255.0f;
	t.visuals.SunGreen_f = 255.0f;
	t.visuals.SunBlue_f = 255.0f;
	t.visuals.ZenithRed_f = 104.0f;
	t.visuals.ZenithGreen_f = 104.0f;
	t.visuals.ZenithBlue_f = 104.0f;
	t.visuals.fSunShadowBias = 0.00005f;
	t.visuals.bColorGrading = false;
	t.visuals.ColorGradingLUT = "";
	t.visuals.bBloomEnabled = true;
	t.visuals.bLevelVSyncEnabled = true;
	t.visuals.bOcclusionCulling = false;
	// GGMAX: reset the per-level Low VRAM flag so a level WITHOUT the field doesn't inherit
	// the previous level's state (the FPM parse below only fires when the field exists).
	t.visuals.bLowVRAM = false;
	{ extern void GGSetLowVRAMLevel(int); GGSetLowVRAMLevel(0); }
	// GGMAX 3.25: the Brutal off-switches, per-level. Reset to their NEUTRAL values, and push
	// each one live - this function is what the "Reset Visuals" button calls, so writing the
	// struct without driving the switches would leave the panel ticked and the engine off.
	gg_visuals_reset_brutal_switches();

	t.visuals.bEnableTerrainChunkCulling = false;
	t.visuals.bEnablePointShadowCulling = false;
	t.visuals.bEnableSpotShadowCulling = false;
	t.visuals.bEnableObjectCulling = false;
	t.visuals.bEnableAnimationCulling = false;
	t.visuals.fLODMultiplier = 1.0f;

	t.visuals.bEnable30FpsAnimations = false;

	t.visuals.bShadowsLowestLOD = false;
	t.visuals.bProbesLowestLOD = false;
	t.visuals.bRaycastLowestLOD = false;
	t.visuals.bPhysicsLowestLOD = false;
	t.visuals.bThreadedPhysics = false;
	t.visuals.bPhysicsWorldAt60 = false;
	t.visuals.bReflectionsLowestLOD = false;

	t.visuals.g_bDelayedShadows = true;
	t.visuals.g_bDelayedShadowsLaptop = false;
	t.visuals.fEnvProbeBrightness = 1.0f;

	t.visuals.fsetBloomThreshold = 2.0f;
	t.visuals.ApparentSize = 0.000008f;
	t.visuals.fsetBloomStrength = 1.0f;
	t.visuals.bSSREnabled = false;
	t.visuals.bReflectionsEnabled = true;
	t.visuals.bFXAAEnabled = false;

	t.visuals.bDOF = false;
	t.visuals.fDOFStrength = 10.0f;
	t.visuals.fDOFApertureSize = 0.2f;
	t.visuals.fDOFFocalLength = 300.0f;

	t.visuals.bTessellation = true;
	t.visuals.bLightShafts = true;
	t.visuals.lightShaftDensity = 0.8f;
	t.visuals.lightShaftWeight = 0.3f;
	t.visuals.lightShaftDecay = 0.945f;
	t.visuals.lightShaftExposure = 0.7f;
	t.visuals.bLensFlare = true;
	t.visuals.bAutoExposure = false;
	t.visuals.fAutoExposureRate = 0.5f;
	t.visuals.fAutoExposureKey = 0.15f;
	t.visuals.fExposure = 1.0f;
	t.visuals.fGamma = 2.2f;
	t.visuals.fDeSaturate = 1.0f;
	
	t.visuals.iMSAASampleCount = 1;
	t.visuals.iFSRMode = 0;
	t.visuals.fFSRSharpness = 0.2f;

	t.visuals.iMSAO = 1;
	t.visuals.fMSAOPower = 1.0;

	t.visuals.iShadowSpotCascadeResolution = 1024;
	t.visuals.iShadowSpotResolution = 512;
	t.visuals.iShadowPointResolution = 512;
	t.visuals.iShadowPointMax = 16;
	t.visuals.iShadowSpotMax = 8;
	t.visuals.bTransparentShadows = false;

	t.visuals.fTreeShadowLODDist = 2000.0f;   // GGMAX 3.108c: Lee 2026-10-12 (was 4000)
	t.visuals.iTreeShadowRange = 5;

	t.visuals.fShadowFarPlane = DEFAULT_FAR_PLANE;

	t.visuals.SkyCloudiness = 0.68f;
	t.visuals.SkyCloudCoverage = 1.38f;
	t.visuals.SkyCloudHeight = 60000.0f;
	t.visuals.SkyCloudThickness = 98000.0f;
	t.visuals.SkyCloudSpeed = 3.0f;

	t.visuals.bWaterEnable = true;
	t.showeditorwater = true;

	t.visuals.fWaterWaveAmplitude = 20.0f;
	t.visuals.fWaterPatchLength = 40.0f;
	t.visuals.fWaterCausticSize = 3.0f;
	t.visuals.fUnderwaterColorR = 5.0f;
	t.visuals.fUnderwaterColorG = 40.0f;
	t.visuals.fUnderwaterColorB = 60.0f;
	t.visuals.fUnderwaterFog = 20.0f;
	t.visuals.fWaterChoppyScale = 0.0f;
	t.visuals.fWaterWindDependency = 0.0f;
	t.visuals.WaterSpeed1 = 0.06f;
	t.visuals.WaterFogMaxDist = 11500.0f;
	t.visuals.WaterFogMinDist = 0.0f;
	t.visuals.WaterFogMinAmount = 0.25f;

	t.visuals.iTimeOfday = 1;
	visuals_calcsunanglefromtimeofday(t.visuals.iTimeOfday,&t.visuals.SunAngleX,&t.visuals.SunAngleY,&t.visuals.SunAngleZ);

	t.visuals.bSimulate24Hours = false;
	t.visuals.fTimeSpeed = 1.0;

	t.visuals.fWeatherIntensity = 50.0;
	t.visuals.fWeatherLighting = 0.0;
	t.visuals.fWeatherThunder = 0.0;
	t.visuals.fWeatherWind = 50.0;

	t.visuals.bPPSnow = false;
	t.visuals.voxel_steps = 16.0f;
	t.visuals.pp_size = 1.0f;
	t.visuals.pp_alpha = 1.0f;
	t.visuals.wind_direction_x = 1.0f;
	t.visuals.wind_direction_y = -7.0f;
	t.visuals.wind_direction_z = 1.0f;
	t.visuals.wind_speed = 1.0f;
	t.visuals.wind_randomness = 1.0f;
	t.visuals.bpp_disable_indoor = true;
	t.visuals.tree_wind = 0.1f;   // GGMAX 3.58 (Lee): was 0.0 - see Types.h
	t.visuals.tree_sss = 0.0f;

	t.visuals.fLevelDifficulty = 50.0;

	for (int iL = 0; iL < 32; iL++) 
	{
		t.visuals.sTerrainTextures[iL] = "";
		t.visuals.sTerrainTexturesName[iL] = "";
	}
	for (int iL = 0; iL < 128; iL++) 
	{
		t.visuals.sGrassTextures[iL] = "";
		t.visuals.sGrassTexturesName[iL] = "";
		extern bool iDeleteAllGrassTextures;
		iDeleteAllGrassTextures = true;
		t.visuals.sFactionName[iL] = "";
	}

	t.visuals.bEndableAmbientMusicTrack = false;
	t.visuals.sAmbientMusicTrack = "";
	t.visuals.iAmbientMusicTrackVolume = 100;
	t.visuals.bEnableCombatMusicTrack = false;
	t.visuals.sCombatMusicTrack = "";
	t.visuals.iCombatMusicTrackVolume = 100;

	if (bNewLevel)
	{
		t.visuals.bEndableTreeDrawing = false;
		t.visuals.bEndableGrassDrawing = false;
		t.visuals.bEndableTerrainDrawing = true;
		t.visuals.bEnableEmptyLevelMode = false;
		t.visuals.bEnableZeroNavMeshMode = false;

		t.showeditortrees = false; //Editor also follow this on new level.
		t.showeditorveg = false;
		t.showeditorterrain = true;
		t.visuals.iHeightmapWidth = 0;
		t.visuals.iHeightmapHeight = 0;
	}

	t.visuals.bDisableSkybox = false;

	t.visuals.bRainEnabled = false;
	t.visuals.fRainSpeedX = -1.5;
	t.visuals.fRainSpeedY = -18.0;
	t.visuals.fRainOpacity = 0.0;
	t.visuals.fRainScaleX = 2.0;
	t.visuals.fRainScaleY = 2.0;
	t.visuals.fRainRefreactionScale = 0.005;

	t.visuals.bSnowEnabled = false;
	t.visuals.fSnowLayers = 15.0;
	t.visuals.fSnowDepth = 0.5;
	t.visuals.fSnowWind = 0.5;
	t.visuals.fSnowSpeed = 2.0;
	t.visuals.fSnowOpacity = 1.0;
	t.visuals.fSnowOffset = 0.0;

	t.visuals.iEnvProbeResolution = 128;

	t.visuals.newperformancepresets = 0;

	if (t.visuals.skyindex == 0)
	{
		extern wiECS::Entity g_weatherEntityID;
		wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
		if (weather)
		{
			//PE: Must delete old skymap.
			if (weather->skyMap.IsValid() && weather->skyMapName.length() > 0)
			{
				//PE: Make sure to free any old resources.
				WickedCall_DeleteImage(weather->skyMapName);
			}
			weather->skyMap = {};
			weather->skyMapName = "";
		}
	}

	//PE: MEM - 1472 : Wicked_Update_Visuals((void *)&t.visuals);               S:0MB   V : (3620, 0)
	void Wicked_Update_Visuals(void *voidvisual);
	Wicked_Update_Visuals((void *)&t.visuals);
	//PE: MEM - 1480 : Done Wicked_Update_Visuals((void *)&t.visuals);          S:889MB V : (4510, 0)
}

void visuals_editordefaults ( void )
{
	//  Editor requires some different values
	t.visuals=t.editorvisuals;
	t.visuals.shaderlevels.terrain=3;
	t.visuals.shaderlevels.entities=2;
	t.visuals.shaderlevels.vegetation=3;
	t.visuals.shaderlevels.lighting=2;

	t.visuals.TerrainLOD1_f=8000.0;
	t.visuals.TerrainLOD2_f=15000.0;
	t.visuals.TerrainLOD3_f=20000.0;
	t.visuals.TerrainSize_f=100.0f;
	t.visuals.AmbienceIntensity_f=89.25f;
	//PE: We should always have light in editor, addition to issue: https://github.com/TheGameCreators/GameGuruRepo/issues/352#event-2404317522
	//PE: fix for commit: https://github.com/TheGameCreators/GameGuruRepo/commit/4aff257545b19a969e4676c7b057a42d8e93fdf1
	t.visuals.SurfaceSunFactor_f = 1.0;
	t.visuals.SurfaceIntensity_f=0.8f;
	t.visuals.SurfaceRed_f=255;
	t.visuals.SurfaceGreen_f=255;
	t.visuals.SurfaceBlue_f=255;
	t.visuals.PostBrightness_f=0.0;
	t.visuals.PostContrast_f=1.1666f;

	t.visuals.CameraASPECT_f = 1920.0 / 1080.0; //PE: Fixed in wicked, or fov will be wrong on widescreen.
	t.visuals.CameraFOV_f = 60; //LB:Default now 90 degree (XM_PI/(fov/15.0f) was 45.0; //PE: Fixed in wicked.
	t.visuals.WeaponFOV_f = 60; //LB:Default now 90 degree (XM_PI/(fov/15.0f) was 45.0;

	//PE: Only have camera settings in test game.
	t.visuals.CameraNEAR_f = DEFAULT_NEAR_PLANE;
	t.visuals.CameraFAR_f = DEFAULT_FAR_PLANE;
	t.visuals.CameraFOVZoomed_f=1.0f;
	t.visuals.shadowmode=100;

	if (t.visuals.FogA_f > 2.0)
		t.visuals.FogA_f = 0;
	
	t.editorvisuals=t.visuals;
}

void visuals_water_reset(void)
{
	//Water Shader Settings
	g.gdefaultwaterheight = -500.0f;// GGORIGIN_Y; properly out the picture (stops it showing on grey grid level)
	t.terrain.waterliney_f = g.gdefaultwaterheight;
	t.visuals.WaterRed_f = 9;
	t.visuals.WaterGreen_f = 21;
	t.visuals.WaterBlue_f = 43;
	t.visuals.WaterAlpha_f = 0.0f;
	t.visuals.WaterWaveIntensity_f = 155.0f;
	t.visuals.WaterTransparancy_f = 0.75f;
	t.visuals.WaterReflection_f = 0.50f;
	t.visuals.WaterReflectionSparkleIntensity = 1.90f;
	t.visuals.WaterFlowDirectionX = 1;
	t.visuals.WaterFlowDirectionY = 1;
	t.visuals.WaterDistortionWaves = 0.030f;
	t.visuals.WaterSpeed1 = 0.06f;
	t.visuals.WaterFogMaxDist = 11500.0f;
	t.visuals.WaterFogMinDist = 0.0f;
	t.visuals.WaterFogMinAmount = 0.25f;
	t.visuals.WaterFlowSpeed = 1.0f;
}

void visuals_newlevel ( void )
{
	// 310117 - Game gets its visuals from the visuals.ini file
	visuals_load ( );

	//  New level resets these values
	visuals_resetvalues ( );

	//  Trigger all visuals to update
	t.visuals.refreshvegetation=0;
	t.visuals.refreshshaders=1;
	t.visuals.refreshskysettings=0;
	t.visuals.refreshterraintexture=0;
	t.visuals.refreshvegtexture=0;
	t.editorvisuals=t.visuals;

	// 090517 - actually we WANT to reset some of the game visual settings (reset fog/terrain/etc) for NEW LEVEL!
	t.gamevisuals=t.visuals;

	//  Apply editor defaults
	visuals_editordefaults ( );

	// force a fade in of the gamma (hides construction artefacts and looks nice)
	extern float g_fGlobalGammaFadeIn;
	g_fGlobalGammaFadeIn = -2;
}

void visuals_free ( void )
{
	//  clear values when end session
	t.nearestlightindex[1]=0;
	t.nearestlightindex[2]=0;
	t.nearestlightindex[3]=0;
	t.nearestlightindex[4]=0;

	//  reset underwater effects when session ends
	physics_player_reset_underwaterstate ( );
}

void visuals_updateskyterrainvegindex ( void )
{
	//  Find sky (saved EXEs have smaller sky banks)
	g.skyindex=1;
	for ( t.s = 1 ; t.s<=  g.skymax; t.s++ )
	{
		if (  t.skybank_s[t.s] == "clear"  )  g.skyindex = t.s;
	}
	//PE: Support "None"
	g.skyindex = 0;// for MAX, Non is the default (Dynamic Sky!)
	for (t.s = 0; t.s <= g.skymax; t.s++)
	{
		if (  t.skybank_s[t.s] == t.visuals.sky_s ) 
		{
			g.skyindex=t.s;
		}
	}
	t.visuals.skyindex=g.skyindex;

	//  Find terrain (saved EXEs have smaller sky banks)
	g.terrainstyleindex=1;
	for ( t.t = 1 ; t.t<=  g.terrainstylemax; t.t++ )
	{
		if (  t.terrainstylebank_s[t.t] == t.visuals.terrain_s ) 
		{
			g.terrainstyleindex=t.t;
		}
	}
	t.visuals.terrainindex=g.terrainstyleindex;

	//  Find veg (saved EXEs have smaller sky banks)
	g.vegstyleindex=1;
	for ( t.v = 1 ; t.v<=  g.vegstylemax; t.v++ )
	{
		if (  t.vegstylebank_s[t.v] == t.visuals.vegetation_s ) 
		{
			g.vegstyleindex=t.v;
		}
	}
	t.visuals.vegetationindex=g.vegstyleindex;
}

void visuals_save ( void )
{
	// save all visual settings
	t.visfile_s=g.fpscrootdir_s+"\\visuals.ini";
	char pRealVisFile[MAX_PATH] = "";
	strcpy(pRealVisFile, t.visfile_s.Get());
	GG_GetRealPath(pRealVisFile, 1);
	t.visfile_s = pRealVisFile;
	if ( FileExist(t.visfile_s.Get()) == 1 ) DeleteAFile ( t.visfile_s.Get() );
	OpenToWrite (  1,t.visfile_s.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.shaderlevels.terrain="+Str(t.visuals.shaderlevels.terrain);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.shaderlevels.entities="+Str(t.visuals.shaderlevels.entities);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.shaderlevels.vegetation="+Str(t.visuals.shaderlevels.vegetation);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.shaderlevels.lighting="+Str(t.visuals.shaderlevels.lighting);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.mode="+Str(t.visuals.mode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.PostContrast#="+Str(t.visuals.PostContrast_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.PostBrightness#="+Str(t.visuals.PostBrightness_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.FogNearest#="+Str(t.visuals.FogNearest_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.FogDistance#="+Str(t.visuals.FogDistance_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.FogR#="+Str(t.visuals.FogR_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.FogG#="+Str(t.visuals.FogG_f) ;
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.FogB#="+Str(t.visuals.FogB_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.FogA#="+Str(t.visuals.FogA_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.AmbienceIntensity#="+Str(t.visuals.AmbienceIntensity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.AmbienceRed#="+Str(t.visuals.AmbienceRed_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.AmbienceGeen#="+Str(t.visuals.AmbienceGreen_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.AmbienceBlue#="+Str(t.visuals.AmbienceBlue_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SurfaceIntensity#="+Str(t.visuals.SurfaceIntensity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SurfaceRed#="+Str(t.visuals.SurfaceRed_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SurfaceGreen#="+Str(t.visuals.SurfaceGreen_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SurfaceBlue#="+Str(t.visuals.SurfaceBlue_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.Specular#="+Str(t.visuals.Specular_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SurfaceSunFactor#="+Str(t.visuals.SurfaceSunFactor_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.DistanceTransitionStart#="+Str(t.visuals.DistanceTransitionStart_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.DistanceTransitionRange#="+Str(t.visuals.DistanceTransitionRange_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.CameraNEAR#="+Str(t.visuals.CameraNEAR_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.CameraFAR#="+Str(t.visuals.CameraFAR_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.CameraFOV#="+Str(t.visuals.CameraFOV_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.WeaponFOV#="+Str(t.visuals.WeaponFOV_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.CameraFOVZoomed#="+Str(t.visuals.CameraFOVZoomed_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.TerrainLOD1#="+Str(t.visuals.TerrainLOD1_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.TerrainLOD2#="+Str(t.visuals.TerrainLOD2_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.TerrainLOD3#="+Str(t.visuals.TerrainLOD3_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.TerrainSize#="+Str(t.visuals.TerrainSize_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.VegQuantity#="+Str(t.visuals.VegQuantity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.VegWidth#="+Str(t.visuals.VegWidth_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.VegHeight#="+Str(t.visuals.VegHeight_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.skyindex="+Str(t.visuals.skyindex);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.sky$="+t.visuals.sky_s;
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.terrainindex="+Str(t.visuals.terrainindex);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.terrain$="+t.visuals.terrain_s;
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.vegetationindex="+Str(t.visuals.vegetationindex);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.vegetation$="+t.visuals.vegetation_s;
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.reflectionmode="+Str(t.visuals.reflectionmode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.shadowmode="+Str(t.visuals.shadowmode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.bloommode="+Str(t.visuals.bloommode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.lightraymode="+Str(t.visuals.lightraymode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.vegetationmode="+Str(t.visuals.vegetationmode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.occlusionvalue="+Str(t.visuals.occlusionvalue);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.debugvisualsmode="+Str(t.visuals.debugvisualsmode);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.VignetteRadius#="+Str(t.visuals.VignetteRadius_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.VignetteIntensity#="+Str(t.visuals.VignetteIntensity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.MotionDistance#="+Str(t.visuals.MotionDistance_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.MotionIntensity#="+Str(t.visuals.MotionIntensity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.DepthOfFieldDistance#="+Str(t.visuals.DepthOfFieldDistance_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.DepthOfFieldIntensity#="+Str(t.visuals.DepthOfFieldIntensity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.LightrayLength="+Str(t.visuals.LightrayLength_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.LightrayQuality="+Str(t.visuals.LightrayQuality_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.LightrayDecay="+Str(t.visuals.LightrayDecay_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SAORadius="+Str(t.visuals.SAORadius_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SAOIntensity="+Str(t.visuals.SAOIntensity_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork +"visuals.SAOQuality="+Str(t.visuals.SAOQuality_f);
	WriteString (  1, t.strwork.Get() );
	t.strwork = ""; t.strwork = t.strwork + "visuals.LensFlare=" + Str(t.visuals.LensFlare_f);
	WriteString (1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Exposure=" + Str(t.visuals.fExposure);
	WriteString (1, t.strwork.Get());
	//New Water Settings
	t.strwork = ""; t.strwork = t.strwork + "visuals.Waterheight=" + Str(g.gdefaultwaterheight);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Waterred=" + Str(t.visuals.WaterRed_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Waterblue=" + Str(t.visuals.WaterBlue_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Watergreen=" + Str(t.visuals.WaterGreen_f);
	WriteString(1, t.strwork.Get());
	// GGMAX 3.25e: the A of the Water Base Color picker. Saved for the first time here - it
	// was write-only before, reset to 0 on every load, so an author could set the surface
	// transparency and never see it again. Default stays 0 so the real ocean is unchanged.
	t.strwork = ""; t.strwork = t.strwork + "visuals.Wateralpha=" + Str(t.visuals.WaterAlpha_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterWaveIntensity=" + Str(t.visuals.WaterWaveIntensity_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterTransparancy=" + Str(t.visuals.WaterTransparancy_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterSparkleCol=" + Str(t.visuals.WaterReflectionSparkleIntensity);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterReflection=" + Str(t.visuals.WaterReflection_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterFlowDirectionX=" + Str(t.visuals.WaterFlowDirectionX);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterFlowDirectionY=" + Str(t.visuals.WaterFlowDirectionY);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterDistortionWaves=" + Str(t.visuals.WaterDistortionWaves);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterSpeed1=" + Str(t.visuals.WaterSpeed1);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterFlowSpeed=" + Str(t.visuals.WaterFlowSpeed);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Weather=" + Str(t.visuals.iEnvironmentWeather);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.SunIntensity=" + Str(t.visuals.SunIntensity_f);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.SunRed=" + Str(t.visuals.SunRed_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SunGreen=" + Str(t.visuals.SunGreen_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SunBlue=" + Str(t.visuals.SunBlue_f);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.ZenithRed=" + Str(t.visuals.ZenithRed_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ZenithGreen=" + Str(t.visuals.ZenithGreen_f);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ZenithBlue=" + Str(t.visuals.ZenithBlue_f);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.SunShadowBias=" + Str(t.visuals.fSunShadowBias);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ColorGrading=" + Str(t.visuals.bColorGrading);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.ColorGradingLUT=" + t.visuals.ColorGradingLUT;
	WriteString(1, t.strwork.Get());
	
	t.strwork = ""; t.strwork = t.strwork + "visuals.LevelVSyncEnabled=" + Str(t.visuals.bLevelVSyncEnabled);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.OcclusionCulling=" + Str(t.visuals.bOcclusionCulling);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.LowVRAM=" + Str(t.visuals.bLowVRAM);
	WriteString(1, t.strwork.Get());

	// GGMAX 3.25: Brutal off-switches, per-level.
	t.strwork = ""; t.strwork = t.strwork + "visuals.TerrainBake=" + Str(t.visuals.bTerrainBake);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SuperQuickObjects=" + Str(t.visuals.bSuperQuickObjects);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterBake=" + Str(t.visuals.bWaterBake);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.NoTrees=" + Str(t.visuals.bNoTrees);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.NoGrass=" + Str(t.visuals.bNoGrass);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.NoAO=" + Str(t.visuals.bNoAO);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.NoShadows=" + Str(t.visuals.bNoShadows);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.NoOcclusionCull=" + Str(t.visuals.bNoOcclusionCull);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ParticlePct=" + Str(t.visuals.iParticlePct);
	WriteString(1, t.strwork.Get());
	// GGMAX 3.116: written from the LIVE level value, not t.visuals - a Test Game round trip reverts
	// t.visuals to editorvisuals, and the engine value is the one the panel shows and the grass uses.
	{
		extern int GGGetGrassDensityLevel();
		t.visuals.iGrassDensityPct = GGGetGrassDensityLevel();
		t.strwork = ""; t.strwork = t.strwork + "visuals.GrassDensity=" + Str(t.visuals.iGrassDensityPct);
		WriteString(1, t.strwork.Get());
	}
	t.strwork = ""; t.strwork = t.strwork + "visuals.ObjectCullDist=" + Str(t.visuals.iObjectCullDist);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.TextureDivide=" + Str(t.visuals.iTextureDivide);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.AnimReductionScale=" + Str(t.visuals.iAnimReductionScale);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.OcclusionCullDelay=" + Str(t.visuals.iOcclusionCullDelay);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ShadowResSteps=" + Str(t.visuals.iShadowResSteps);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SuperQuickLevel=" + Str(t.visuals.iSuperQuickLevel);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.TerrainBakeResNear=" + Str(t.visuals.iTerrainBakeResNear);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ReflectionWidth=" + Str(t.visuals.iReflectionWidth);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ReflectionBlur=" + Str(t.visuals.iReflectionBlur);
	WriteString(1, t.strwork.Get());
	
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableTerrainChunkCulling=" + Str(t.visuals.bEnableTerrainChunkCulling);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnablePointShadowCulling=" + Str(t.visuals.bEnablePointShadowCulling);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableSpotShadowCulling=" + Str(t.visuals.bEnableSpotShadowCulling);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableObjectCulling=" + Str(t.visuals.bEnableObjectCulling);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableAnimationCulling=" + Str(t.visuals.bEnableAnimationCulling);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.LODMultiplier=" + Str(t.visuals.fLODMultiplier);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.ThreadedPhysics=" + Str(t.visuals.bThreadedPhysics);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.PhysicsWorldAt60=" + Str(t.visuals.bPhysicsWorldAt60);
	WriteString(1, t.strwork.Get());
	

	t.strwork = ""; t.strwork = t.strwork + "visuals.Enable30FpsAnimations=" + Str(t.visuals.bEnable30FpsAnimations);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.ShadowsLowestLOD=" + Str(t.visuals.bShadowsLowestLOD);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ProbesLowestLOD=" + Str(t.visuals.bProbesLowestLOD);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RaycastLowestLOD=" + Str(t.visuals.bRaycastLowestLOD);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.PhysicsLowestLOD=" + Str(t.visuals.bPhysicsLowestLOD);
	WriteString(1, t.strwork.Get());
	
	t.strwork = ""; t.strwork = t.strwork + "visuals.ReflectionsLowestLOD=" + Str(t.visuals.bReflectionsLowestLOD);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.DelayedShadows=" + Str(t.visuals.g_bDelayedShadows);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.DelayedShadowsLaptop=" + Str(t.visuals.g_bDelayedShadowsLaptop);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.EnvProbeBrightness=" + Str(t.visuals.fEnvProbeBrightness);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.BloomEnabled=" + Str(t.visuals.bBloomEnabled);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.BloomThreshold=" + Str(t.visuals.fsetBloomThreshold);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.ApparentSize=" + Str(t.visuals.ApparentSize);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.BloomStrength=" + Str(t.visuals.fsetBloomStrength);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SSREnabled=" + Str(t.visuals.bSSREnabled);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ReflectionsEnabled=" + Str(t.visuals.bReflectionsEnabled);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.FXAAEnabled=" + Str(t.visuals.bFXAAEnabled);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.DOF=" + Str(t.visuals.bDOF);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.DOFStrength=" + Str(t.visuals.fDOFStrength);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.DOFApertureSize=" + Str(t.visuals.fDOFApertureSize);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.DOFFocalLength=" + Str(t.visuals.fDOFFocalLength);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.Tessellation=" + Str(t.visuals.bTessellation);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.LightShafts=" + Str(t.visuals.bLightShafts);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.LensFlare=" + Str(t.visuals.bLensFlare);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Exposure=" + Str(t.visuals.fExposure);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.AutoExposure=" + Str(t.visuals.bAutoExposure);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.fAutoExposureRate=" + Str(t.visuals.fAutoExposureRate);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.fAutoExposureKey=" + Str(t.visuals.fAutoExposureKey);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Gamma=" + Str(t.visuals.fGamma);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.DeSaturate=" + Str(t.visuals.fDeSaturate);
	WriteString(1, t.strwork.Get());
	
	t.strwork = ""; t.strwork = t.strwork + "visuals.MSAASampleCount=" + Str(t.visuals.iMSAASampleCount);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.FSRMode=" + Str(t.visuals.iFSRMode);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.FSRSharpness=" + Str(t.visuals.fFSRSharpness);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.MSAO=" + Str(t.visuals.iMSAO);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.MSAOPower=" + Str(t.visuals.fMSAOPower);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.SkyCloudiness=" + Str(t.visuals.SkyCloudiness);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SkyCloudScale=" + Str(t.visuals.SkyCloudHeight);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SkyCloudSpeed=" + Str(t.visuals.SkyCloudSpeed);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SkyCloudCoverage=" + Str(t.visuals.SkyCloudCoverage);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SkyCloudHeight=" + Str(t.visuals.SkyCloudHeight);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SkyCloudThickness=" + Str(t.visuals.SkyCloudThickness);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.ShadowSpotCascadeResolution=" + Str(t.visuals.iShadowSpotCascadeResolution);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ShadowSpotResolution=" + Str(t.visuals.iShadowSpotResolution);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.ShadowPointResolution=" + Str(t.visuals.iShadowPointResolution);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.iShadowPointMax=" + Str(t.visuals.iShadowPointMax);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.iShadowSpotMax=" + Str(t.visuals.iShadowSpotMax);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.bTransparentShadows=" + Str(t.visuals.bTransparentShadows);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.TreeShadowLODDist=" + Str(t.visuals.fTreeShadowLODDist);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.TreeShadowRange=" + Str(t.visuals.iTreeShadowRange);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.SunAngleX=" + Str(t.visuals.SunAngleX);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SunAngleY=" + Str(t.visuals.SunAngleY);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SunAngleZ=" + Str(t.visuals.SunAngleZ);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.ShadowFarPlane=" + Str(t.visuals.fShadowFarPlane);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterEnable=" + Str(t.visuals.bWaterEnable);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterWaveAmplitude=" + Str(t.visuals.fWaterWaveAmplitude);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterPatchLength=" + Str(t.visuals.fWaterPatchLength);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterCausticSize=" + Str(t.visuals.fWaterCausticSize);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.UnderwaterColorR=" + Str(t.visuals.fUnderwaterColorR);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.UnderwaterColorG=" + Str(t.visuals.fUnderwaterColorG);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.UnderwaterColorB=" + Str(t.visuals.fUnderwaterColorB);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.UnderwaterFog=" + Str(t.visuals.fUnderwaterFog);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterChoppyScale=" + Str(t.visuals.fWaterChoppyScale);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterWindDependency=" + Str(t.visuals.fWaterWindDependency);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterFogMinDist=" + Str(t.visuals.WaterFogMinDist);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterFogMaxDist=" + Str(t.visuals.WaterFogMaxDist);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WaterFogMinAmount=" + Str(t.visuals.WaterFogMinAmount);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.TimeOfday=" + Str(t.visuals.iTimeOfday);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.Simulate24Hours=" + Str(t.visuals.bSimulate24Hours);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.TimeSpeed=" + Str(t.visuals.fTimeSpeed);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.WeatherIntensity=" + Str(t.visuals.fWeatherIntensity);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WeatherLighting=" + Str(t.visuals.fWeatherLighting);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WeatherThunder=" + Str(t.visuals.fWeatherThunder);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.WeatherWind=" + Str(t.visuals.fWeatherWind);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.bPPSnow=" + Str(t.visuals.bPPSnow);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.bPPSnow=" + Str(t.visuals.bPPSnow);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.voxel_steps=" + Str(t.visuals.voxel_steps);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.pp_size=" + Str(t.visuals.pp_size);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.pp_alpha=" + Str(t.visuals.pp_alpha);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.wind_direction_x=" + Str(t.visuals.wind_direction_x);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.wind_direction_x=" + Str(t.visuals.wind_direction_x);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.wind_direction_y=" + Str(t.visuals.wind_direction_y);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.wind_direction_z=" + Str(t.visuals.wind_direction_z);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.wind_speed=" + Str(t.visuals.wind_speed);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.wind_randomness=" + Str(t.visuals.wind_randomness);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.bpp_disable_indoor=" + Str(t.visuals.bpp_disable_indoor);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.tree_wind=" + Str(t.visuals.tree_wind);
	WriteString(1, t.strwork.Get());
	
	t.strwork = ""; t.strwork = t.strwork + "visuals.tree_sss=" + Str(t.visuals.tree_sss);
	WriteString(1, t.strwork.Get());


	t.strwork = ""; t.strwork = t.strwork + "visuals.LevelDifficulty=" + Str(t.visuals.fLevelDifficulty);
	WriteString(1, t.strwork.Get());
	
	for (int iL = 0; iL < 32; iL++) 
	{
		if (t.visuals.sTerrainTextures[iL] != "")
		{
			t.strwork = ""; t.strwork = t.strwork + "visuals.TerrainTextures" + cStr(iL) + "=" + cStr(t.visuals.sTerrainTextures[iL]);
			WriteString(1, t.strwork.Get());
			t.strwork = ""; t.strwork = t.strwork + "visuals.TerrainTexturesName" + cStr(iL) + "=" + cStr(t.visuals.sTerrainTexturesName[iL]);
			WriteString(1, t.strwork.Get());
		}
	}
	for (int iL = 0; iL < 128; iL++) 
	{
		if (t.visuals.sGrassTextures[iL] != "")
		{
			t.strwork = ""; t.strwork = t.strwork + "visuals.GrassTextures" + cStr(iL) + "=" + cStr(t.visuals.sGrassTextures[iL]);
			WriteString(1, t.strwork.Get());
			t.strwork = ""; t.strwork = t.strwork + "visuals.GrassTexturesName" + cStr(iL) + "=" + cStr(t.visuals.sGrassTexturesName[iL]);
			WriteString(1, t.strwork.Get());
		}
		if (t.visuals.sFactionName[iL] != "")
		{
			t.strwork = ""; t.strwork = t.strwork + "visuals.FactionName" + cStr(iL) + "=" + cStr(t.visuals.sFactionName[iL]);
			WriteString(1, t.strwork.Get());
		}
		
	}

	t.strwork = ""; t.strwork = t.strwork + "visuals.EndableAmbientMusicTrack=" + Str(t.visuals.bEndableAmbientMusicTrack);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.AmbientMusicTrack=" + cStr(t.visuals.sAmbientMusicTrack);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.AmbientMusicTrackVolume=" + Str(t.visuals.iAmbientMusicTrackVolume);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableCombatMusicTrack=" + Str(t.visuals.bEnableCombatMusicTrack);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.CombatMusicTrack=" + cStr(t.visuals.sCombatMusicTrack);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.CombatMusicTrackVolume=" + Str(t.visuals.iCombatMusicTrackVolume);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.RainEnabled=" + Str(t.visuals.bRainEnabled);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RainSpeedX=" + Str(t.visuals.fRainSpeedX);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RainSpeedY=" + Str(t.visuals.fRainSpeedY);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RainOpacity=" + Str(t.visuals.fRainOpacity);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RainScaleX=" + Str(t.visuals.fRainScaleX);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RainScaleY=" + Str(t.visuals.fRainScaleY);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.RainRefreactionScale=" + Str(t.visuals.fRainRefreactionScale);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.SnowEnabled=" + Str(t.visuals.bSnowEnabled);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SnowLayers=" + Str(t.visuals.fSnowLayers);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SnowDepth=" + Str(t.visuals.fSnowDepth);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SnowWind=" + Str(t.visuals.fSnowWind);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SnowSpeed=" + Str(t.visuals.fSnowSpeed);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.SnowOpacity=" + Str(t.visuals.fSnowOpacity);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.EndableTreeDrawing=" + Str(t.visuals.bEndableTreeDrawing);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EndableGrassDrawing=" + Str(t.visuals.bEndableGrassDrawing);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EndableTerrainDrawing=" + Str(t.visuals.bEndableTerrainDrawing);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableEmptyLevelMode=" + Str(t.visuals.bEnableEmptyLevelMode);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.EnableZeroNavMeshMode=" + Str(t.visuals.bEnableZeroNavMeshMode);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.HeightmapWidth=" + Str(t.visuals.iHeightmapWidth);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.HeightmapHeight=" + Str(t.visuals.iHeightmapHeight);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.DisableSkybox=" + Str(t.visuals.bDisableSkybox);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.GrassMinHeight=" + Str(GGGrass::gggrass_global_params.min_height);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.GrassMaxHeight=" + Str(GGGrass::gggrass_global_params.max_height);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.GrassStartHeightUnderwater=" + Str(GGGrass::gggrass_global_params.min_height_underwater);
	WriteString(1, t.strwork.Get());
	t.strwork = ""; t.strwork = t.strwork + "visuals.GrassEndHeightUnderwater=" + Str(GGGrass::gggrass_global_params.max_height_underwater);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.EnvProbeResolution=" + Str(t.visuals.iEnvProbeResolution);
	WriteString(1, t.strwork.Get());

	t.strwork = ""; t.strwork = t.strwork + "visuals.NewPerformancePresets=" + Str(t.visuals.newperformancepresets);
	WriteString(1, t.strwork.Get());

	WriteString (  1, "" );

	CloseFile (  1 );
	return;
}

void visuals_load ( void )
{
	//  Record previous visuals settings (see below)
	t.defaultvisuals = t.visuals;

	//  Reset newer visual values that may not exist in older visuals.ini files
	t.visuals.VignetteRadius_f=0.5;
	t.visuals.VignetteIntensity_f=0;
	t.visuals.MotionDistance_f=0.5;
	t.visuals.MotionIntensity_f=0;
	t.visuals.DepthOfFieldDistance_f=0.5;
	t.visuals.DepthOfFieldIntensity_f=0;
	t.visuals.LightrayLength_f=0.55f;
	t.visuals.LightrayQuality_f=20.0f;
	t.visuals.LightrayDecay_f=0.8f;
	t.visuals.SAORadius_f=0.5f;
	t.visuals.SAOIntensity_f=0.5f;
	t.visuals.SAOQuality_f=1.0f;
	t.visuals.LensFlare_f=0.5f;
	//water settings
	visuals_water_reset();

	//Wicked defaults.
	t.visuals.SkyIntensity_f = 1.0f;
	t.visuals.SunIntensity_f = 9.0f;
	t.visuals.SunRed_f = 255.0f;
	t.visuals.SunGreen_f = 255.0f;
	t.visuals.SunBlue_f = 255.0f;
	t.visuals.ZenithRed_f = 104.0f;
	t.visuals.ZenithGreen_f = 104.0f;
	t.visuals.ZenithBlue_f = 104.0f;
	t.visuals.fSunShadowBias = 0.00005f;
	t.visuals.bColorGrading = false;
	t.visuals.ColorGradingLUT = "";
	t.visuals.bBloomEnabled = true;
	t.visuals.bLevelVSyncEnabled = true;
	t.visuals.bOcclusionCulling = false;
	// GGMAX: reset the per-level Low VRAM flag so a level WITHOUT the field doesn't inherit
	// the previous level's state (the FPM parse below only fires when the field exists).
	t.visuals.bLowVRAM = false;
	{ extern void GGSetLowVRAMLevel(int); GGSetLowVRAMLevel(0); }
	// GGMAX 3.25: the Brutal off-switches, per-level. Reset to their NEUTRAL values, and push
	// each one live - this function is what the "Reset Visuals" button calls, so writing the
	// struct without driving the switches would leave the panel ticked and the engine off.
	gg_visuals_reset_brutal_switches();
	t.visuals.bEnableTerrainChunkCulling = false;
	t.visuals.bEnablePointShadowCulling = false;
	t.visuals.bEnableSpotShadowCulling = false;
	t.visuals.bEnableObjectCulling = false;
	t.visuals.bEnableAnimationCulling = false;
	t.visuals.fLODMultiplier = 1.0f;

	t.visuals.bEnable30FpsAnimations = false;

	t.visuals.bPhysicsLowestLOD = false;
	t.visuals.bShadowsLowestLOD = false;
	t.visuals.bProbesLowestLOD = false;
	t.visuals.bRaycastLowestLOD = false;
	t.visuals.bThreadedPhysics = false;
	t.visuals.bPhysicsWorldAt60 = false;
	t.visuals.bReflectionsLowestLOD = false;

	t.visuals.g_bDelayedShadows = true;
	t.visuals.g_bDelayedShadowsLaptop = false;

	t.visuals.fEnvProbeBrightness = 1.0f;

	t.visuals.fsetBloomThreshold = 2.0f;
	t.visuals.ApparentSize = 0.000008f;
	t.visuals.fsetBloomStrength = 1.0f;
	t.visuals.bSSREnabled = false;
	t.visuals.bReflectionsEnabled = true;
	t.visuals.bFXAAEnabled = false;

	t.visuals.bDOF = false;
	t.visuals.fDOFStrength = 10.0f;
	t.visuals.fDOFApertureSize = 0.2f;
	t.visuals.fDOFFocalLength = 300.0f;

	t.visuals.bTessellation = true;
	t.visuals.bLightShafts = true;
	t.visuals.bLensFlare = true;
	t.visuals.bAutoExposure = true;
	t.visuals.fAutoExposureRate = 0.5f;
	t.visuals.fAutoExposureKey = 0.15f;
	t.visuals.fExposure = 1.0f;
	t.visuals.fGamma = 2.2f;
	t.visuals.fDeSaturate = 1.0f;
	
	t.visuals.iMSAASampleCount = 1;
	t.visuals.iFSRMode = 0;
	t.visuals.fFSRSharpness = 0.2f;

	t.visuals.iMSAO = 1;
	t.visuals.fMSAOPower = 1.0;

	t.visuals.SkyCloudiness = 0.68f;
	t.visuals.SkyCloudCoverage = 1.38f;
	t.visuals.SkyCloudHeight = 60000.0f;
	t.visuals.SkyCloudThickness = 98000.0f;
	t.visuals.SkyCloudSpeed = 3.0f;

	t.visuals.iShadowSpotCascadeResolution = 1024;
	t.visuals.iShadowSpotResolution = 512;
	t.visuals.iShadowPointResolution = 512;
	t.visuals.iShadowPointMax = 16;
	t.visuals.iShadowSpotMax = 8;
	t.visuals.bTransparentShadows = false;

	t.visuals.fTreeShadowLODDist = 2000.0f;   // GGMAX 3.108c: Lee 2026-10-12 (was 4000)
	t.visuals.iTreeShadowRange = 5;

	t.visuals.fShadowFarPlane = DEFAULT_FAR_PLANE;
	t.visuals.bWaterEnable = true;
	t.visuals.fWaterWaveAmplitude = 20.0f;
	t.visuals.fWaterPatchLength = 40.0f;
	t.visuals.fWaterCausticSize = 3.0f;
	t.visuals.fUnderwaterColorR = 5.0f;
	t.visuals.fUnderwaterColorG = 40.0f;
	t.visuals.fUnderwaterColorB = 60.0f;
	t.visuals.fUnderwaterFog = 20.0f;
	t.visuals.fWaterChoppyScale = 0.0f;
	t.visuals.fWaterWindDependency = 0.0f;
	t.visuals.WaterSpeed1 = 0.06f;

	t.visuals.WaterFogMaxDist = 11500.0f;
	t.visuals.WaterFogMinDist = 0.0f;
	t.visuals.WaterFogMinAmount = 0.25f;

	t.visuals.iTimeOfday = 1;
	visuals_calcsunanglefromtimeofday(t.visuals.iTimeOfday, &t.visuals.SunAngleX, &t.visuals.SunAngleY, &t.visuals.SunAngleZ);

	t.visuals.bSimulate24Hours = false;
	t.visuals.fTimeSpeed = 1.0;

	t.visuals.fWeatherIntensity = 50.0;
	t.visuals.fWeatherLighting = 0.0;
	t.visuals.fWeatherThunder = 0.0;
	t.visuals.fWeatherWind = 50.0;

	t.visuals.bPPSnow = false;
	t.visuals.voxel_steps = 16.0f;
	t.visuals.pp_size = 1.0f;
	t.visuals.pp_alpha = 1.0f;
	t.visuals.wind_direction_x = 1.0f;
	t.visuals.wind_direction_y = -7.0f;
	t.visuals.wind_direction_z = 1.0f;
	t.visuals.wind_speed = 1.0f;
	t.visuals.wind_randomness = 1.0f;
	t.visuals.bpp_disable_indoor = true;
	t.visuals.tree_wind = 0.1f;   // GGMAX 3.58 (Lee): was 0.0 - see Types.h
	t.visuals.tree_sss = 0.0f;
	

	t.visuals.fLevelDifficulty = 50.0;

	for (int iL = 0; iL < 128; iL++) 
	{
		t.visuals.sGrassTextures[iL] = "";
		t.visuals.sGrassTexturesName[iL] = "";
		extern bool iDeleteAllGrassTextures;
		iDeleteAllGrassTextures = true;
		t.visuals.sFactionName[iL] = "";
	}

	t.visuals.bEndableAmbientMusicTrack = false;
	t.visuals.sAmbientMusicTrack = "";
	t.visuals.iAmbientMusicTrackVolume = 100;
	t.visuals.bEnableCombatMusicTrack = false;
	t.visuals.sCombatMusicTrack = "";
	t.visuals.iCombatMusicTrackVolume = 100;

	t.visuals.bEndableTreeDrawing = false;
	t.visuals.bEndableGrassDrawing = false;
	t.visuals.bEndableTerrainDrawing = true;
	t.visuals.bEnableEmptyLevelMode = false;
	t.visuals.bEnableZeroNavMeshMode = false;

	t.visuals.iHeightmapWidth = 0;
	t.visuals.iHeightmapHeight = 0;

	t.visuals.bDisableSkybox = false;

	t.visuals.bRainEnabled = false;
	t.visuals.fRainSpeedX = -1.5;
	t.visuals.fRainSpeedY = -18.0;
	t.visuals.fRainOpacity = 0.0;
	t.visuals.fRainScaleX = 2.0;
	t.visuals.fRainScaleY = 2.0;
	t.visuals.fRainRefreactionScale = 0.005;

	t.visuals.bSnowEnabled = false;
	t.visuals.fSnowLayers = 15.0;
	t.visuals.fSnowDepth = 0.5;
	t.visuals.fSnowWind = 0.5;
	t.visuals.fSnowSpeed = 2.0;
	t.visuals.fSnowOpacity = 1.0;
	t.visuals.fSnowOffset = 0.0;

	// special flag to know when new performance defaults applied
	t.visuals.newperformancepresets = 0;

	// load all visual settings
	bool bVisualFileFromLevel = false;
	if ( g.fpscrootdir_s == "" ) 
	{
		t.visfile_s="visuals.ini";
		bVisualFileFromLevel = true;
	}
	else
	{
		char cFullWritePath[MAX_PATH] = "";
		GG_GetRealPath(cFullWritePath, 0);
		t.visfile_s = cstr(cFullWritePath) + "visuals.ini";
		if (FileExist(t.visfile_s.Get()) == 0)
		{
			//PE: No file yet in write folder , take the original.
			t.visfile_s = g.fpscrootdir_s + "\\visuals.ini";
		}
	}
	if ( FileExist(t.visfile_s.Get()) == 1 ) 
	{
		g_ggVisualsKeysSeen.clear();   // GGMAX 3.82
		OpenToRead (  1,t.visfile_s.Get() );
		do
		{
			t.tline_s = ReadString ( 1 );
			for ( t.n = 1 ; t.n<=  Len(t.tline_s.Get()); t.n++ )
			{
				if (  cstr(Mid(t.tline_s.Get(),t.n)) == "=" ) 
				{
					t.tfield_s=Left(t.tline_s.Get(),t.n-1);
					t.tvalue_s=Right(t.tline_s.Get(),Len(t.tline_s.Get())-Len(t.tfield_s.Get())-1);
				}
			}
			// GGMAX 3.82: one record point for all ~190 keys - the only place tfield_s is set.
			if (t.tfield_s.Get() != NULL && t.tfield_s.Get()[0] != 0)
				g_ggVisualsKeysSeen.insert(std::string(t.tfield_s.Get()));
			t.try_s = "visuals.shaderlevels.terrain" ; if (  t.tfield_s == t.try_s  )  t.visuals.shaderlevels.terrain = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.shaderlevels.entities" ;
			if (  t.tfield_s == t.try_s  )
				t.visuals.shaderlevels.entities = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.shaderlevels.vegetation" ; if (  t.tfield_s == t.try_s  )  t.visuals.shaderlevels.vegetation = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.shaderlevels.lighting" ; if (  t.tfield_s == t.try_s  )  t.visuals.shaderlevels.lighting = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.mode" ; if (  t.tfield_s == t.try_s  )  t.visuals.mode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.PostContrast#" ; if (  t.tfield_s == t.try_s  )  t.visuals.PostContrast_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.PostBrightness#" ; if (  t.tfield_s == t.try_s  )  t.visuals.PostBrightness_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.FogNearest#" ;
			if (t.tfield_s == t.try_s)
			{
				//PE: @Paul we need this so users can change the settings.
				//PE: You can change the defaults in "visual.ini" this is what the editor use :)
				//PE: Removed t.visuals.FogNearest_f = 360000 , t.visuals.FogDistance_f = 1000000.0f.
				t.visuals.FogNearest_f = ValF(t.tvalue_s.Get());
			}
			t.try_s = "visuals.FogDistance#" ; if (  t.tfield_s == t.try_s  )  t.visuals.FogDistance_f = ValF(t.tvalue_s.Get()); //PE: Needed ?
			t.try_s = "visuals.FogR#" ; if (  t.tfield_s == t.try_s  )  t.visuals.FogR_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.FogG#" ; if (  t.tfield_s == t.try_s  )  t.visuals.FogG_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.FogB#" ; if (  t.tfield_s == t.try_s  )  t.visuals.FogB_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.FogA#" ;
			if (t.tfield_s == t.try_s)
			{
				t.visuals.FogA_f = ValF(t.tvalue_s.Get());
				if (t.visuals.FogA_f > 2.0) //PE: Wicked used for height.
					t.visuals.FogA_f = 0;
			}
			t.try_s = "visuals.AmbienceIntensity#" ; if (  t.tfield_s == t.try_s  )  t.visuals.AmbienceIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.AmbienceRed#" ; if (  t.tfield_s == t.try_s  )
				t.visuals.AmbienceRed_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.AmbienceGeen#" ; if (  t.tfield_s == t.try_s  )
				t.visuals.AmbienceGreen_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.AmbienceBlue#" ; if (  t.tfield_s == t.try_s  )
				t.visuals.AmbienceBlue_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SurfaceIntensity#" ; if (  t.tfield_s == t.try_s  )  t.visuals.SurfaceIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SurfaceRed#" ; if (  t.tfield_s == t.try_s  )  t.visuals.SurfaceRed_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SurfaceGreen#" ; if (  t.tfield_s == t.try_s  )  t.visuals.SurfaceGreen_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SurfaceBlue#" ; if (  t.tfield_s == t.try_s  )  t.visuals.SurfaceBlue_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SurfaceSunFactor#" ; if (  t.tfield_s == t.try_s  )  t.visuals.SurfaceSunFactor_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Specular#" ; if (  t.tfield_s == t.try_s  )  t.visuals.Specular_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DistanceTransitionStart#" ; if (  t.tfield_s == t.try_s  )  t.visuals.DistanceTransitionStart_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DistanceTransitionRange#" ; if (  t.tfield_s == t.try_s  )  t.visuals.DistanceTransitionRange_f = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.CameraFAR#" ; if (  t.tfield_s == t.try_s  )  t.visuals.CameraFAR_f = ValF(t.tvalue_s.Get());
			//  20032015 = 021 - Don't use FOV for multiplayer
			if (  t.game.runasmultiplayer  ==  0 ) 
			{
				t.try_s = "visuals.CameraFOV#" ; if (  t.tfield_s == t.try_s  )
				t.visuals.CameraFOV_f = ValF(t.tvalue_s.Get());
				t.try_s = "visuals.WeaponFOV#" ; if (  t.tfield_s == t.try_s  )  t.visuals.WeaponFOV_f = ValF(t.tvalue_s.Get());
				t.try_s = "visuals.CameraFOVZoomed#" ; if (  t.tfield_s == t.try_s  )  t.visuals.CameraFOVZoomed_f = ValF(t.tvalue_s.Get());
			}
			t.try_s = "visuals.TerrainLOD1#" ; if (  t.tfield_s == t.try_s  )  t.visuals.TerrainLOD1_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.TerrainLOD2#" ; if (  t.tfield_s == t.try_s  )  t.visuals.TerrainLOD2_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.TerrainLOD3#" ; if (  t.tfield_s == t.try_s  )  t.visuals.TerrainLOD3_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.TerrainSize#" ; if (  t.tfield_s == t.try_s  )  t.visuals.TerrainSize_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.VegQuantity#" ; if (  t.tfield_s == t.try_s  )  t.visuals.VegQuantity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.VegWidth#" ; if (  t.tfield_s == t.try_s  )  t.visuals.VegWidth_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.VegHeight#" ; if (  t.tfield_s == t.try_s  )  t.visuals.VegHeight_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.skyindex" ; if (  t.tfield_s == t.try_s  )  t.visuals.skyindex = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.sky$" ; if (  t.tfield_s == t.try_s  )  t.visuals.sky_s = t.tvalue_s;
			t.try_s = "visuals.terrainindex" ; if (  t.tfield_s == t.try_s  )  t.visuals.terrainindex = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.terrain$" ; if (  t.tfield_s == t.try_s  )  t.visuals.terrain_s = t.tvalue_s;
			t.try_s = "visuals.vegetationindex" ; if (  t.tfield_s == t.try_s  )  t.visuals.vegetationindex = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.vegetation$" ; if (  t.tfield_s == t.try_s  )  t.visuals.vegetation_s = t.tvalue_s;
			t.try_s = "visuals.reflectionmode" ; if (  t.tfield_s == t.try_s  )  t.visuals.reflectionmode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.shadowmode" ; if (  t.tfield_s == t.try_s  )  t.visuals.shadowmode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.bloommode" ; if (  t.tfield_s == t.try_s  )  t.visuals.bloommode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.lightraymode" ; if (  t.tfield_s == t.try_s  )  t.visuals.lightraymode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.vegetationmode" ; if (  t.tfield_s == t.try_s  )
				t.visuals.vegetationmode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.occlusionvalue" ; if (  t.tfield_s == t.try_s  )  t.visuals.occlusionvalue = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.debugvisualsmode" ; if (  t.tfield_s == t.try_s  )  t.visuals.debugvisualsmode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.VignetteRadius#" ; if (  t.tfield_s == t.try_s  )  t.visuals.VignetteRadius_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.VignetteIntensity#" ; if (  t.tfield_s == t.try_s  )  t.visuals.VignetteIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.MotionDistance#" ; if (  t.tfield_s == t.try_s  )  t.visuals.MotionDistance_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.MotionIntensity#" ; if (  t.tfield_s == t.try_s  )  t.visuals.MotionIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DepthOfFieldDistance#" ; if (  t.tfield_s == t.try_s  )  t.visuals.DepthOfFieldDistance_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DepthOfFieldIntensity#" ; if (  t.tfield_s == t.try_s  )  t.visuals.DepthOfFieldIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LightrayLength" ; if (  t.tfield_s == t.try_s  )  t.visuals.LightrayLength_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LightrayQuality" ; if (  t.tfield_s == t.try_s  )  t.visuals.LightrayQuality_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LightrayDecay" ; if (  t.tfield_s == t.try_s  )  t.visuals.LightrayDecay_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SAORadius" ; if (  t.tfield_s == t.try_s  )  t.visuals.SAORadius_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SAOIntensity" ; if (  t.tfield_s == t.try_s  )  t.visuals.SAOIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SAOQuality" ; if (  t.tfield_s == t.try_s  )  t.visuals.SAOQuality_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LensFlare"; if (t.tfield_s == t.try_s)  t.visuals.LensFlare_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Exposure"; if (t.tfield_s == t.try_s)  t.visuals.fExposure = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.fAutoExposureRate"; if (t.tfield_s == t.try_s)  t.visuals.fAutoExposureRate = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.fAutoExposureKey"; if (t.tfield_s == t.try_s)  t.visuals.fAutoExposureKey = ValF(t.tvalue_s.Get());
			//new water settings
			t.try_s = "visuals.Waterheight"; if (t.tfield_s == t.try_s)  g.gdefaultwaterheight = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Waterred"; if (t.tfield_s == t.try_s)  t.visuals.WaterRed_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Waterblue"; if (t.tfield_s == t.try_s)  t.visuals.WaterBlue_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Watergreen"; if (t.tfield_s == t.try_s)  t.visuals.WaterGreen_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Wateralpha"; if (t.tfield_s == t.try_s)  t.visuals.WaterAlpha_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterWaveIntensity"; if (t.tfield_s == t.try_s)  t.visuals.WaterWaveIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterTransparancy"; if (t.tfield_s == t.try_s)  t.visuals.WaterTransparancy_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterSparkleCol"; if (t.tfield_s == t.try_s)  t.visuals.WaterReflectionSparkleIntensity = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterReflection"; if (t.tfield_s == t.try_s)  t.visuals.WaterReflection_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterFlowDirectionX"; if (t.tfield_s == t.try_s)  t.visuals.WaterFlowDirectionX = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterFlowDirectionY"; if (t.tfield_s == t.try_s)  t.visuals.WaterFlowDirectionY = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterDistortionWaves"; if (t.tfield_s == t.try_s)  t.visuals.WaterDistortionWaves = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterSpeed1"; if (t.tfield_s == t.try_s)  t.visuals.WaterSpeed1 = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterFlowSpeed"; if (t.tfield_s == t.try_s)  t.visuals.WaterFlowSpeed = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Weather"; if (t.tfield_s == t.try_s)  t.visuals.iEnvironmentWeather = ValF(t.tvalue_s.Get());

			//PE: Why was SunIntensity disabled (commented out) ? , re enabled it so it can be saved/loaded with levels.
			t.try_s = "visuals.SunIntensity"; if (t.tfield_s == t.try_s)  t.visuals.SunIntensity_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SunRed"; if (t.tfield_s == t.try_s)  t.visuals.SunRed_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SunGreen"; if (t.tfield_s == t.try_s)  t.visuals.SunGreen_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SunBlue"; if (t.tfield_s == t.try_s)  t.visuals.SunBlue_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ZenithRed"; if (t.tfield_s == t.try_s)  t.visuals.ZenithRed_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ZenithGreen"; if (t.tfield_s == t.try_s)  t.visuals.ZenithGreen_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ZenithBlue"; if (t.tfield_s == t.try_s)  t.visuals.ZenithBlue_f = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SunShadowBias"; if (t.tfield_s == t.try_s)  t.visuals.fSunShadowBias = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ColorGrading"; if (t.tfield_s == t.try_s)  t.visuals.bColorGrading = ValF(t.tvalue_s.Get());
		
			t.try_s = "visuals.ColorGradingLUT"; if (t.tfield_s == t.try_s)
			{
				//PE: Change saved lut locations.
				std::string sString = t.tvalue_s.Get();
				replaceAll(sString, "imagebank\\", "editors\\");
				t.visuals.ColorGradingLUT = sString.c_str();
			}

			t.try_s = "visuals.LevelVSyncEnabled"; if (t.tfield_s == t.try_s)  t.visuals.bLevelVSyncEnabled = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.OcclusionCulling"; if (t.tfield_s == t.try_s)  t.visuals.bOcclusionCulling = ValF(t.tvalue_s.Get());
			// GGMAX: apply the per-level Low VRAM flag AT PARSE TIME — grass systems build at
			// chunk-spawn during level load, so waiting for the visuals-apply pass would miss them.
			t.try_s = "visuals.LowVRAM"; if (t.tfield_s == t.try_s) { t.visuals.bLowVRAM = ValF(t.tvalue_s.Get()) != 0; extern void GGSetLowVRAMLevel(int); GGSetLowVRAMLevel(t.visuals.bLowVRAM ? 1 : 0); }

			// GGMAX 3.25: Brutal off-switches, per-level. Applied AT PARSE TIME like LowVRAM -
			// several of these are read while the level is still building (grass at chunk spawn,
			// texture detail at texture load), so waiting for the visuals-apply pass would miss
			// them for one whole level. A level that predates these fields matches nothing here
			// and keeps the neutral values the pre-parse reset already wrote.
			{
				extern bool gg_terrain_bake, gg_water_bake;
				extern void GGSetNoTreesLevel(int);
				extern void GGSetNoGrassLevel(int);
				extern void GGSetNoAOLevel(int);
				extern void GGSetNoShadowsLevel(int);
				extern void GGSetNoOcclusionLevel(int);
				extern void GGSetParticlePctLevel(int);
				extern void GGSetObjectCullDistLevel(int);
				extern void GGSetTextureDivideLive(int);
				t.try_s = "visuals.TerrainBake";     if (t.tfield_s == t.try_s) { t.visuals.bTerrainBake     = ValF(t.tvalue_s.Get()) != 0; gg_terrain_bake = t.visuals.bTerrainBake; }
				t.try_s = "visuals.WaterBake";       if (t.tfield_s == t.try_s) { t.visuals.bWaterBake       = ValF(t.tvalue_s.Get()) != 0; gg_water_bake   = t.visuals.bWaterBake; }
				t.try_s = "visuals.NoTrees";         if (t.tfield_s == t.try_s) { t.visuals.bNoTrees         = ValF(t.tvalue_s.Get()) != 0; GGSetNoTreesLevel(t.visuals.bNoTrees ? 1 : 0); }
				t.try_s = "visuals.NoGrass";         if (t.tfield_s == t.try_s) { t.visuals.bNoGrass         = ValF(t.tvalue_s.Get()) != 0; GGSetNoGrassLevel(t.visuals.bNoGrass ? 1 : 0); }
				t.try_s = "visuals.NoAO";            if (t.tfield_s == t.try_s) { t.visuals.bNoAO            = ValF(t.tvalue_s.Get()) != 0; GGSetNoAOLevel(t.visuals.bNoAO ? 1 : 0); }
				t.try_s = "visuals.NoShadows";       if (t.tfield_s == t.try_s) { t.visuals.bNoShadows       = ValF(t.tvalue_s.Get()) != 0; GGSetNoShadowsLevel(t.visuals.bNoShadows ? 1 : 0); }
				t.try_s = "visuals.NoOcclusionCull"; if (t.tfield_s == t.try_s) { t.visuals.bNoOcclusionCull = ValF(t.tvalue_s.Get()) != 0; GGSetNoOcclusionLevel(t.visuals.bNoOcclusionCull ? 1 : 0); }
				t.try_s = "visuals.ParticlePct";     if (t.tfield_s == t.try_s) { t.visuals.iParticlePct     = (int)ValF(t.tvalue_s.Get()); GGSetParticlePctLevel(t.visuals.iParticlePct); }
				t.try_s = "visuals.GrassDensity";    if (t.tfield_s == t.try_s) { extern void GGSetGrassDensityLevel(int); extern int GGGetGrassDensityLevel(); t.visuals.iGrassDensityPct = (int)ValF(t.tvalue_s.Get()); GGSetGrassDensityLevel(t.visuals.iGrassDensityPct); t.visuals.iGrassDensityPct = GGGetGrassDensityLevel(); }
				t.try_s = "visuals.ObjectCullDist";  if (t.tfield_s == t.try_s) { t.visuals.iObjectCullDist  = (int)ValF(t.tvalue_s.Get()); GGSetObjectCullDistLevel(t.visuals.iObjectCullDist); }
				t.try_s = "visuals.TextureDivide";   if (t.tfield_s == t.try_s) { t.visuals.iTextureDivide   = (int)ValF(t.tvalue_s.Get()); if (t.visuals.iTextureDivide != 1) GGSetTextureDivideLive(t.visuals.iTextureDivide); }
				t.try_s = "visuals.AnimReductionScale"; if (t.tfield_s == t.try_s) { t.visuals.iAnimReductionScale = (int)ValF(t.tvalue_s.Get()); }
				t.try_s = "visuals.OcclusionCullDelay"; if (t.tfield_s == t.try_s) { t.visuals.iOcclusionCullDelay = (int)ValF(t.tvalue_s.Get()); }
				t.try_s = "visuals.ShadowResSteps"; if (t.tfield_s == t.try_s) { t.visuals.iShadowResSteps = (int)ValF(t.tvalue_s.Get()); }
				t.try_s = "visuals.SuperQuickLevel"; if (t.tfield_s == t.try_s) { t.visuals.iSuperQuickLevel = (int)ValF(t.tvalue_s.Get()); }
				t.try_s = "visuals.SuperQuickObjects"; if (t.tfield_s == t.try_s) { t.visuals.bSuperQuickObjects = ValF(t.tvalue_s.Get()) != 0; }
				t.try_s = "visuals.TerrainBakeResNear"; if (t.tfield_s == t.try_s) { extern int gg_terrain_bake_res_near; t.visuals.iTerrainBakeResNear = (int)ValF(t.tvalue_s.Get()); if (t.visuals.iTerrainBakeResNear >= 256 && t.visuals.iTerrainBakeResNear <= 8192) gg_terrain_bake_res_near = t.visuals.iTerrainBakeResNear; }
				// GGMAX 3.89: Water Reflection Size. ONE clamp, used identically by the panel, the
				// harness verb and the engine push. Anything out of range reads as Auto.
				t.try_s = "visuals.ReflectionWidth"; if (t.tfield_s == t.try_s) { t.visuals.iReflectionWidth = (int)ValF(t.tvalue_s.Get()); if (t.visuals.iReflectionWidth != 0 && (t.visuals.iReflectionWidth < 128 || t.visuals.iReflectionWidth > 2048)) t.visuals.iReflectionWidth = 0; }
				t.try_s = "visuals.ReflectionBlur"; if (t.tfield_s == t.try_s) { t.visuals.iReflectionBlur = (int)ValF(t.tvalue_s.Get()); if (t.visuals.iReflectionBlur < 0 || t.visuals.iReflectionBlur > 3) t.visuals.iReflectionBlur = 0; }
			}

			t.try_s = "visuals.EnableTerrainChunkCulling"; if (t.tfield_s == t.try_s)  t.visuals.bEnableTerrainChunkCulling = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnablePointShadowCulling"; if (t.tfield_s == t.try_s)  t.visuals.bEnablePointShadowCulling = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnableSpotShadowCulling"; if (t.tfield_s == t.try_s)  t.visuals.bEnableSpotShadowCulling = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnableObjectCulling";
			if (t.tfield_s == t.try_s)
				t.visuals.bEnableObjectCulling = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnableAnimationCulling"; if (t.tfield_s == t.try_s)  t.visuals.bEnableAnimationCulling = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LODMultiplier"; if (t.tfield_s == t.try_s)  t.visuals.fLODMultiplier = ValF(t.tvalue_s.Get());
			
			t.try_s = "visuals.ThreadedPhysics"; if (t.tfield_s == t.try_s)  t.visuals.bThreadedPhysics = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.PhysicsWorldAt60"; if (t.tfield_s == t.try_s)  t.visuals.bPhysicsWorldAt60 = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.Enable30FpsAnimations"; if (t.tfield_s == t.try_s)  t.visuals.bEnable30FpsAnimations = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.PhysicsLowestLOD"; if (t.tfield_s == t.try_s)  t.visuals.bPhysicsLowestLOD = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ShadowsLowestLOD"; if (t.tfield_s == t.try_s)  t.visuals.bShadowsLowestLOD = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ProbesLowestLOD"; if (t.tfield_s == t.try_s)  t.visuals.bProbesLowestLOD = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RaycastLowestLOD"; if (t.tfield_s == t.try_s)  t.visuals.bRaycastLowestLOD = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ReflectionsLowestLOD"; if (t.tfield_s == t.try_s)  t.visuals.bReflectionsLowestLOD = ValF(t.tvalue_s.Get());


			t.try_s = "visuals.DelayedShadows"; if (t.tfield_s == t.try_s)  t.visuals.g_bDelayedShadows = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DelayedShadowsLaptop"; if (t.tfield_s == t.try_s)  t.visuals.g_bDelayedShadowsLaptop = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.EnvProbeBrightness"; if (t.tfield_s == t.try_s)  { t.visuals.fEnvProbeBrightness = ValF(t.tvalue_s.Get()); if (t.visuals.fEnvProbeBrightness < 0.01f || t.visuals.fEnvProbeBrightness > 10.0f) t.visuals.fEnvProbeBrightness = 1.0f; }

			t.try_s = "visuals.BloomEnabled"; if (t.tfield_s == t.try_s)  t.visuals.bBloomEnabled = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.BloomThreshold"; if (t.tfield_s == t.try_s)  t.visuals.fsetBloomThreshold = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ApparentSize"; if (t.tfield_s == t.try_s)  t.visuals.ApparentSize = ValF(t.tvalue_s.Get());
			
			t.try_s = "visuals.BloomStrength"; if (t.tfield_s == t.try_s)  { t.visuals.fsetBloomStrength = ValF(t.tvalue_s.Get()); if (t.visuals.fsetBloomStrength < 0.1f || t.visuals.fsetBloomStrength > 3.0f) t.visuals.fsetBloomStrength = 1.0f; }

			t.try_s = "visuals.SSREnabled"; if (t.tfield_s == t.try_s)	t.visuals.bSSREnabled = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ReflectionsEnabled"; if (t.tfield_s == t.try_s)  t.visuals.bReflectionsEnabled = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.FXAAEnabled"; if (t.tfield_s == t.try_s)  t.visuals.bFXAAEnabled = ValF(t.tvalue_s.Get());


			t.try_s = "visuals.DOF"; if (t.tfield_s == t.try_s)  t.visuals.bDOF = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DOFStrength"; if (t.tfield_s == t.try_s)  t.visuals.fDOFStrength = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DOFApertureSize"; if (t.tfield_s == t.try_s)  t.visuals.fDOFApertureSize = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.DOFFocalLength"; if (t.tfield_s == t.try_s)  t.visuals.fDOFFocalLength = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.Tessellation"; if (t.tfield_s == t.try_s)  t.visuals.bTessellation = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LightShafts"; if (t.tfield_s == t.try_s)  t.visuals.bLightShafts = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.LensFlare"; if (t.tfield_s == t.try_s)  t.visuals.bLensFlare = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Exposure"; if (t.tfield_s == t.try_s)  t.visuals.fExposure = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.AutoExposure"; if (t.tfield_s == t.try_s)  t.visuals.bAutoExposure = ValF(t.tvalue_s.Get());
			
			//PE: Who removed gamme ? we need it when restoring saved levels ? it was commented out ?
			// PORT TRANSLATE (2026-07-28): Gamma/DeSaturate/BloomStrength/EnvProbeBrightness were
			// DEAD sliders for the whole DX12 port until engine deltas 1.54/1.55 re-wired them, so
			// level files can carry way-out values fiddled against a dead control (the TESTPRO1
			// island shipped Gamma~15 = washed-white scene). Tame implausible values to neutral on
			// load; sane ranges pass through untouched. Live sliders remain unclamped for tuning.
			t.try_s = "visuals.Gamma"; if (t.tfield_s == t.try_s)  { t.visuals.fGamma = ValF(t.tvalue_s.Get()); if (t.visuals.fGamma < 0.5f || t.visuals.fGamma > 5.0f) t.visuals.fGamma = 2.2f; }
			t.try_s = "visuals.DeSaturate"; if (t.tfield_s == t.try_s)  { t.visuals.fDeSaturate = ValF(t.tvalue_s.Get()); if (t.visuals.fDeSaturate < 0.0f || t.visuals.fDeSaturate > 1.0f) t.visuals.fDeSaturate = 1.0f; }
			
			t.try_s = "visuals.MSAASampleCount"; if (t.tfield_s == t.try_s)  t.visuals.iMSAASampleCount = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.FSRMode"; if (t.tfield_s == t.try_s)  t.visuals.iFSRMode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.FSRSharpness"; if (t.tfield_s == t.try_s)  t.visuals.fFSRSharpness = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.MSAO"; if (t.tfield_s == t.try_s)  t.visuals.iMSAO = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.MSAOPower"; if (t.tfield_s == t.try_s)  t.visuals.fMSAOPower = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.SkyCloudiness"; if (t.tfield_s == t.try_s)  t.visuals.SkyCloudiness = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SkyCloudScale"; 
			if (t.tfield_s == t.try_s)
			{
				// LB: older levels had incredible small scale value
				float fTheValue = ValF(t.tvalue_s.Get());
				if (fTheValue >= 1)
				{
					// only accept the newer much larger cloud height values, ignore the 0.0003 stuff from earlier!
					t.visuals.SkyCloudHeight = fTheValue;
				}
			}
			t.try_s = "visuals.SkyCloudSpeed"; if (t.tfield_s == t.try_s)  t.visuals.SkyCloudSpeed = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SkyCloudCoverage"; if (t.tfield_s == t.try_s)  t.visuals.SkyCloudCoverage = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SkyCloudThickness"; if (t.tfield_s == t.try_s)  t.visuals.SkyCloudThickness = ValF(t.tvalue_s.Get());
		
			t.try_s = "visuals.ShadowSpotCascadeResolution"; if (t.tfield_s == t.try_s)  t.visuals.iShadowSpotCascadeResolution = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ShadowSpotResolution"; if (t.tfield_s == t.try_s)  t.visuals.iShadowSpotResolution = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.ShadowPointResolution"; if (t.tfield_s == t.try_s)  t.visuals.iShadowPointResolution = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.iShadowPointMax"; if (t.tfield_s == t.try_s)  t.visuals.iShadowPointMax = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.iShadowSpotMax"; if (t.tfield_s == t.try_s)  t.visuals.iShadowSpotMax = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.bTransparentShadows"; if (t.tfield_s == t.try_s)  t.visuals.bTransparentShadows = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.TreeShadowLODDist"; if (t.tfield_s == t.try_s)  { t.visuals.fTreeShadowLODDist = ValF(t.tvalue_s.Get()); if (t.visuals.fTreeShadowLODDist < 750.0f) t.visuals.fTreeShadowLODDist = 2000.0f; if (t.visuals.fTreeShadowLODDist > 2000.0f) t.visuals.fTreeShadowLODDist = 2000.0f; }   // GGMAX 3.108d: range 750..2000 (Lee 2026-10-12); older, larger values become 2000
			t.try_s = "visuals.TreeShadowRange"; if (t.tfield_s == t.try_s)  { t.visuals.iTreeShadowRange = (int)ValF(t.tvalue_s.Get()); if (t.visuals.iTreeShadowRange < 0 || t.visuals.iTreeShadowRange > 5) t.visuals.iTreeShadowRange = 5; }

			t.try_s = "visuals.SunAngleX"; if (t.tfield_s == t.try_s)  t.visuals.SunAngleX = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SunAngleY"; if (t.tfield_s == t.try_s)  t.visuals.SunAngleY = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SunAngleZ"; if (t.tfield_s == t.try_s)  t.visuals.SunAngleZ = ValF(t.tvalue_s.Get());

			//t.try_s = "visuals.ShadowFarPlane"; if (t.tfield_s == t.try_s)  t.visuals.fShadowFarPlane = ValF(t.tvalue_s.Get());


			t.try_s = "visuals.WaterEnable"; if (t.tfield_s == t.try_s)
				t.visuals.bWaterEnable = ValF(t.tvalue_s.Get());
			
			t.try_s = "visuals.WaterWaveAmplitude"; if (t.tfield_s == t.try_s)  t.visuals.fWaterWaveAmplitude = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterPatchLength"; if (t.tfield_s == t.try_s)  t.visuals.fWaterPatchLength = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterCausticSize"; if (t.tfield_s == t.try_s)  { t.visuals.fWaterCausticSize = ValF(t.tvalue_s.Get()); if (t.visuals.fWaterCausticSize < 1.0f || t.visuals.fWaterCausticSize > 10.0f) t.visuals.fWaterCausticSize = 3.0f; }
			t.try_s = "visuals.UnderwaterColorR"; if (t.tfield_s == t.try_s)  t.visuals.fUnderwaterColorR = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.UnderwaterColorG"; if (t.tfield_s == t.try_s)  t.visuals.fUnderwaterColorG = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.UnderwaterColorB"; if (t.tfield_s == t.try_s)  t.visuals.fUnderwaterColorB = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.UnderwaterFog"; if (t.tfield_s == t.try_s)  { t.visuals.fUnderwaterFog = ValF(t.tvalue_s.Get()); if (t.visuals.fUnderwaterFog < 0.0f || t.visuals.fUnderwaterFog > 100.0f) t.visuals.fUnderwaterFog = 20.0f; }
			t.try_s = "visuals.WaterChoppyScale"; if (t.tfield_s == t.try_s)  t.visuals.fWaterChoppyScale = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterWindDependency"; if (t.tfield_s == t.try_s)  t.visuals.fWaterWindDependency = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.WaterFogMinDist"; if (t.tfield_s == t.try_s)  t.visuals.WaterFogMinDist = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterFogMaxDist"; if (t.tfield_s == t.try_s)  t.visuals.WaterFogMaxDist = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WaterFogMinAmount"; if (t.tfield_s == t.try_s)  t.visuals.WaterFogMinAmount = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.TimeOfday"; if (t.tfield_s == t.try_s)  t.visuals.iTimeOfday = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.Simulate24Hours"; if (t.tfield_s == t.try_s)  t.visuals.bSimulate24Hours = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.TimeSpeed"; if (t.tfield_s == t.try_s)  t.visuals.fTimeSpeed = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.WeatherIntensity"; if (t.tfield_s == t.try_s)  t.visuals.fWeatherIntensity = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WeatherLighting"; if (t.tfield_s == t.try_s)  t.visuals.fWeatherLighting = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WeatherThunder"; if (t.tfield_s == t.try_s)  t.visuals.fWeatherThunder = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.WeatherWind"; if (t.tfield_s == t.try_s)  t.visuals.fWeatherWind = ValF(t.tvalue_s.Get());


			t.try_s = "visuals.bPPSnow"; if (t.tfield_s == t.try_s)  t.visuals.bPPSnow = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.voxel_steps"; if (t.tfield_s == t.try_s)  t.visuals.voxel_steps = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.pp_size"; if (t.tfield_s == t.try_s)  t.visuals.pp_size = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.pp_alpha"; if (t.tfield_s == t.try_s)  t.visuals.pp_alpha = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.wind_direction_x"; if (t.tfield_s == t.try_s)  t.visuals.wind_direction_x = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.wind_direction_y"; if (t.tfield_s == t.try_s)  t.visuals.wind_direction_y = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.wind_direction_z"; if (t.tfield_s == t.try_s)  t.visuals.wind_direction_z = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.wind_speed"; if (t.tfield_s == t.try_s)  t.visuals.wind_speed = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.wind_randomness"; if (t.tfield_s == t.try_s)  t.visuals.wind_randomness = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.bpp_disable_indoor"; if (t.tfield_s == t.try_s)  t.visuals.bpp_disable_indoor = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.tree_wind"; if (t.tfield_s == t.try_s)  t.visuals.tree_wind = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.tree_sss"; if (t.tfield_s == t.try_s)  t.visuals.tree_sss = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.LevelDifficulty"; if (t.tfield_s == t.try_s)  t.visuals.fLevelDifficulty = ValF(t.tvalue_s.Get());

			for (int iL = 0; iL < 128; iL++) 
			{
				t.try_s = cStr("visuals.TerrainTextures") + cStr(iL);
				if (t.tfield_s == t.try_s)
					t.visuals.sTerrainTextures[iL] = t.tvalue_s;
				t.try_s = cStr("visuals.TerrainTexturesName") + cStr(iL);
				if (t.tfield_s == t.try_s)
					t.visuals.sTerrainTexturesName[iL] = t.tvalue_s;

				t.try_s = cStr("visuals.GrassTextures") + cStr(iL);
				if (t.tfield_s == t.try_s)
					t.visuals.sGrassTextures[iL] = t.tvalue_s;
				t.try_s = cStr("visuals.GrassTexturesName") + cStr(iL);
				if (t.tfield_s == t.try_s)
					t.visuals.sGrassTexturesName[iL] = t.tvalue_s;

				t.try_s = cStr("visuals.FactionName") + cStr(iL);
				if (t.tfield_s == t.try_s)
					t.visuals.sFactionName[iL] = t.tvalue_s;
			}

			t.try_s = "visuals.EndableAmbientMusicTrack"; if (t.tfield_s == t.try_s)  t.visuals.bEndableAmbientMusicTrack = ValF(t.tvalue_s.Get());
			t.try_s = cStr("visuals.AmbientMusicTrack"); if (t.tfield_s == t.try_s) t.visuals.sAmbientMusicTrack = t.tvalue_s;
			t.try_s = "visuals.AmbientMusicTrackVolume"; if (t.tfield_s == t.try_s)  t.visuals.iAmbientMusicTrackVolume = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnableCombatMusicTrack"; if (t.tfield_s == t.try_s)  t.visuals.bEnableCombatMusicTrack = ValF(t.tvalue_s.Get());
			t.try_s = cStr("visuals.CombatMusicTrack"); if (t.tfield_s == t.try_s) t.visuals.sCombatMusicTrack = t.tvalue_s;
			t.try_s = "visuals.CombatMusicTrackVolume"; if (t.tfield_s == t.try_s)  t.visuals.iCombatMusicTrackVolume = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.RainEnabled"; if (t.tfield_s == t.try_s)  t.visuals.bRainEnabled = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RainSpeedX"; if (t.tfield_s == t.try_s)  t.visuals.fRainSpeedX = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RainSpeedY"; if (t.tfield_s == t.try_s)  t.visuals.fRainSpeedY = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RainOpacity"; if (t.tfield_s == t.try_s)  t.visuals.fRainOpacity = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RainScaleX"; if (t.tfield_s == t.try_s)  t.visuals.fRainScaleX = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RainScaleY"; if (t.tfield_s == t.try_s)  t.visuals.fRainScaleY = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.RainRefreactionScale"; if (t.tfield_s == t.try_s)  t.visuals.fRainRefreactionScale = ValF(t.tvalue_s.Get());


			t.try_s = "visuals.SnowEnabled"; if (t.tfield_s == t.try_s)  t.visuals.bSnowEnabled = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SnowLayers"; if (t.tfield_s == t.try_s)  t.visuals.fSnowLayers = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SnowDepth"; if (t.tfield_s == t.try_s)  t.visuals.fSnowDepth = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SnowWind"; if (t.tfield_s == t.try_s)  t.visuals.fSnowWind = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SnowSpeed"; if (t.tfield_s == t.try_s)  t.visuals.fSnowSpeed = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.SnowOpacity"; if (t.tfield_s == t.try_s)  t.visuals.fSnowOpacity = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.EndableTreeDrawing"; if (t.tfield_s == t.try_s)  t.visuals.bEndableTreeDrawing = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EndableGrassDrawing"; if (t.tfield_s == t.try_s)  t.visuals.bEndableGrassDrawing = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EndableTerrainDrawing"; if (t.tfield_s == t.try_s)  t.visuals.bEndableTerrainDrawing = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnableEmptyLevelMode"; if (t.tfield_s == t.try_s)  t.visuals.bEnableEmptyLevelMode = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.EnableZeroNavMeshMode"; if (t.tfield_s == t.try_s)  t.visuals.bEnableZeroNavMeshMode = ValF(t.tvalue_s.Get());	
			
			t.try_s = "visuals.HeightmapWidth"; if (t.tfield_s == t.try_s)  t.visuals.iHeightmapWidth = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.HeightmapHeight"; if (t.tfield_s == t.try_s)  t.visuals.iHeightmapHeight = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.DisableSkybox"; if (t.tfield_s == t.try_s)  t.visuals.bDisableSkybox = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.GrassMinHeight"; if (t.tfield_s == t.try_s)  GGGrass::gggrass_global_params.min_height = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.GrassMaxHeight"; if (t.tfield_s == t.try_s)  GGGrass::gggrass_global_params.max_height = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.GrassStartHeightUnderwater"; if (t.tfield_s == t.try_s)  GGGrass::gggrass_global_params.min_height_underwater = ValF(t.tvalue_s.Get());
			t.try_s = "visuals.GrassEndHeightUnderwater"; if (t.tfield_s == t.try_s)  GGGrass::gggrass_global_params.max_height_underwater = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.EnvProbeResolution"; if (t.tfield_s == t.try_s)  t.visuals.iEnvProbeResolution = ValF(t.tvalue_s.Get());

			t.try_s = "visuals.NewPerformancePresets"; if (t.tfield_s == t.try_s)  t.visuals.newperformancepresets = ValF(t.tvalue_s.Get());

		} while ( !(  Len(t.tline_s.Get())<2 ) );

		CloseFile (  1 );

		// LB: Detect old MAX level values, and correct here
		if (t.visuals.WaterSpeed1 > 1.0f)
		{
			// a recent change reduced this value from the likes oif 35 down to 0.00xx, which coincided with reducing water level to zero
			t.visuals.WaterSpeed1 = 0.06f;
			g.gdefaultwaterheight = -500.0f;
			t.visuals.fWaterPatchLength = 10.0f;
			t.visuals.bWaterEnable = false;
		}
		t.terrain.waterliney_f = g.gdefaultwaterheight;

		if (!t.visuals.bWaterEnable)
		{
			t.terrain.waterliney_f = g.gdefaultwaterheight = -10000.0f;
		}

		// if older visuals file and loaded into new performance aware engine, add best defaults for PERFORMANCE! No-one remarks on QUALITY in Steam Reviews, so favor PERFORMANCE.
		if (bVisualFileFromLevel == true)
		{
			if (t.visuals.newperformancepresets == 0)
			{
				// ★★★ GGMAX 3.82: the downgrade may not overwrite a setting the author STATED.
				//
				// Every level carries its own visuals.ini inside the .fpm, and one written before
				// `NewPerformancePresets` existed lands here - so the whole level is stamped LOW,
				// which among other things clears bReflectionsEnabled (M-Visuals_part1.cpp, the
				// `shaderlevels.entities == 3` branch). Lee hit exactly that: Switch Escape's 2023
				// file says `visuals.ReflectionsEnabled=1` and the checkbox still loaded OFF, because
				// the value was overwritten seconds after being read.
				//
				// ★ The downgrade is still right for everything the file DOES NOT mention - an old
				// level that never expressed a preference should favour performance. What it cannot
				// do is tell "unstated" from "stated and old". g_ggVisualsKeysSeen is that
				// distinction: snapshot, downgrade, then put back only the fields whose key was
				// physically present in the file.
				//
				// ⚠ Restores ONLY what the downgrade actually stamps - shaderlevels.* via
				// visuals_shaderlevels_setlevel, and the three flags its update_core writes.
				// Extending the downgrade means extending this list; they are a pair.
				const visualstype vStated = t.visuals;
				bool bUpdateEngine = false;
				visuals_shaderlevels_setlevel(4, bUpdateEngine); // LOW - best for performance :)
				t.visuals.newperformancepresets = 1;

				#define GG_KEY_STATED(k) (g_ggVisualsKeysSeen.find(k) != g_ggVisualsKeysSeen.end())
				if (GG_KEY_STATED("visuals.shaderlevels.terrain"))    t.visuals.shaderlevels.terrain    = vStated.shaderlevels.terrain;
				if (GG_KEY_STATED("visuals.shaderlevels.entities"))   t.visuals.shaderlevels.entities   = vStated.shaderlevels.entities;
				if (GG_KEY_STATED("visuals.shaderlevels.vegetation")) t.visuals.shaderlevels.vegetation = vStated.shaderlevels.vegetation;
				if (GG_KEY_STATED("visuals.shaderlevels.lighting"))   t.visuals.shaderlevels.lighting   = vStated.shaderlevels.lighting;
				if (GG_KEY_STATED("visuals.ReflectionsEnabled"))      t.visuals.bReflectionsEnabled     = vStated.bReflectionsEnabled;
				if (GG_KEY_STATED("visuals.LevelVSyncEnabled"))       t.visuals.bLevelVSyncEnabled      = vStated.bLevelVSyncEnabled;
				if (GG_KEY_STATED("visuals.DisableSkybox"))           t.visuals.bDisableSkybox          = vStated.bDisableSkybox;
				#undef GG_KEY_STATED
			}
		}
	}

	//  Restore any settings we want when IDE first starts (that might have been changed by load/save code)
	if (  t.tresetforstartofeditor == 1 ) 
	{
		//  settings that do not change!
		t.visuals.AmbienceIntensity_f=t.defaultvisuals.AmbienceIntensity_f;
		t.visuals.AmbienceRed_f=t.defaultvisuals.AmbienceRed_f;
		t.visuals.AmbienceGreen_f=t.defaultvisuals.AmbienceGreen_f;
		t.visuals.AmbienceBlue_f=t.defaultvisuals.AmbienceBlue_f;
		t.visuals.SurfaceRed_f=t.defaultvisuals.SurfaceRed_f;
		t.visuals.SurfaceGreen_f=t.defaultvisuals.SurfaceGreen_f;
		t.visuals.SurfaceBlue_f=t.defaultvisuals.SurfaceBlue_f;
		t.visuals.skyindex=t.defaultvisuals.skyindex;
		t.visuals.sky_s=t.defaultvisuals.sky_s;
		t.visuals.terrainindex=t.defaultvisuals.terrainindex;
		t.visuals.terrain_s=t.defaultvisuals.terrain_s;
		t.visuals.vegetationindex=t.defaultvisuals.vegetationindex;
		t.visuals.vegetation_s=t.defaultvisuals.vegetation_s;
		t.visuals.TerrainSize_f=100.0;
		t.visuals.iEnvironmentWeather = 0;
		t.tresetforstartofeditor=0;
		//  save out to establish these defaults (if later used to save a level)
		visuals_save ( );
	}

	// Set initial states for the Editor View Options - based on the saved visuals for the level.
	t.showeditorwater = t.visuals.bWaterEnable;
	t.showeditortrees = t.visuals.bEndableTreeDrawing;
	t.showeditorveg = t.visuals.bEndableGrassDrawing;
	t.showeditorterrain = t.visuals.bEndableTerrainDrawing;
}

void visuals_justshaderupdate ( void )
{
}

void visuals_restoreterrainshaderforeditor ( void )
{
	//  must set this distance value to FAR distance so overhead view in editor mode DOES NOT USE super texture (not exist)
	if (  t.terrain.terrainshaderindex>0 ) 
	{
		if (  GetEffectExist(t.terrain.terrainshaderindex) == 1 ) 
		{
			SetVector4 (  g.terrainvectorindex,9999999.0,9999999.0,0.0,0.0 );
			SetEffectConstantV (  t.terrain.terrainshaderindex,"DistanceTransition",g.terrainvectorindex );
		}
	}
}

void visuals_updatespecificobjectmasks ( int e, int obj )
{
	if ( obj>0 ) 
	{
		if ( ObjectExist(obj) == 1 ) 
		{
			int entid = t.entityelement[e].bankindex;
			if ( t.entityprofile[entid].ismarker != 0 ) 
			{
				SetObjectMask ( obj,1,0,0,0 );
			}
			else
			{
				if ( t.entityelement[e].hasbeenbatched == 1 || t.entityprofile[entid].castshadow == -1 ) 
				{
					if (  t.entityprofile[entid].castshadow == -1 ) 
					{
						SetObjectMask ( obj,t.tmaskforcamerasraw,0,0,0 );
						//PE: This removes per mesh cast shadow in test game.
						sObject* pObject = g_ObjectList[obj];
						if (pObject)
							WickedCall_SetObjectCastShadows(pObject, false);

						bool bUseWEMaterial = false;
						if (t.entityprofile[entid].WEMaterial.MaterialActive)
						{
							WickedSetEntityId(entid);
							if (e > 0)
								WickedSetElementId(e);
							else
								WickedSetElementId(0);

							if (pObject)
							{
								bUseWEMaterial = true;
								for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
								{
									sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
									if (pMesh)
									{
										// set properties of mesh
										WickedSetMeshNumber(iMeshIndex);
										bool bCastShadows = WickedGetCastShadows();

										// get wicked meshID
										wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
										if (mesh)
										{
											// get material from mesh
											uint64_t materialEntity = mesh->subsets[0].materialID;
											wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
											if (pObjectMaterial)
											{
												if (bCastShadows)
													pObjectMaterial->SetCastShadow(true);
												else
													pObjectMaterial->SetCastShadow(false);
											}
										}
									}
								}
							}
							WickedSetEntityId(-1);
							WickedSetElementId(0);
						}

					}
					else
					{
						SetObjectMask ( obj,0,0,0,0 );
					}
				}
				else
				{
					t.tsize=ObjectSize(obj,1);
					t.tsmallmask=t.tmaskforcamerasnoreflectionlightrayflag;
					if ( t.tsize>10 )  // 131215 - 25 ) 
					{
						// 131215 - if larger than 10, allow entity to be reflected (before it was only over 100!)
						t.tsmallmask = t.tmaskforcamerasnoreflectionlightrayflag + 4;
						if ( t.visuals.lightraymode>0 )
						{
							t.tsmallmask=t.tsmallmask+16;
						}
					}
					if ( t.entityelement[e].donotreflect == 1 ) 
					{
						if ( t.visuals.lightraymode>0 ) 
						{
							t.tlargemask=t.tmaskforcamerasnoreflectionlightrayflag+16;
						}
						else
						{
							t.tlargemask=t.tmaskforcamerasnoreflectionlightrayflag;
						}
					}
					else
					{
						t.tlargemask=t.tmaskforcameras;
					}
					if ( t.tsize<100 ) 
					{
						SetObjectMask ( obj,t.tsmallmask,0,0,0 );
					}
					else
					{
						SetObjectMask ( obj,t.tlargemask,0,0,0 );
					}
				}
			}
		}
	}
}

