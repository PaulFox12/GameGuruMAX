//
// the render settings and post effects a game script sets (SetReflections, SetAO, SetShadowResolution, SetBloom and the
// rest), kept apart from the level's visuals and applied again after every visuals push, from the DX11 branch. On this
// engine the bloom radius and cap, the sun lens flare colour, the screen lens flare, the sun cascade count and splits and
// the shadow job wait have no counterpart: their values are kept and reported, nothing is applied
//
#include "stdafx.h"
#include "gameguru.h"
#include "master.h"
#include "GGTerrain\GGTerrain.h"
#include "GGTerrain\GGGrass.h"
#include "GGTerrain\GGTrees.h"

using namespace GGTerrain;
using namespace GGGrass;

extern MasterRenderer * master_renderer;
void WickedCall_SetShadowRange(float fRange);
void gridedit_setvsync(bool bLevelVSyncEnabled);

// post effects set from Lua for the running game. A value below 0 keeps the level's own setting. They are kept apart
// from t.visuals / t.gamevisuals so nothing reaches the editor or the saved level, and re-applied after every visuals
// push; cleared at each level start and when a test game ends
struct sLuaPostEffects
{
	int iBloom = -1;
	float fBloomStrength = -1;
	float fBloomThreshold = -1;
	float fBloomRadius = -1;
	float fBloomCap = -1;
	int iDOF = -1;
	float fDOFStrength = -1;
	float fDOFFocalLength = -1;
	float fDOFApertureSize = -1;
	int iLightShafts = -1;
	int iLensFlare = -1;
	float fLensFlareSunHue = -1;
	float fLensFlareSunBrightness = -1;
	int iScreenLensFlare = -1;
	float fScreenLensFlareIntensity = -1;
	float fScreenLensFlareThreshold = -1;
	float fScreenLensFlareSpacing = -1;
	float fScreenLensFlareHalo = -1;
	int iScreenLensFlareNoSky = -1;
	float fScreenLensFlareCap = -1;
	int iScreenLensFlareFollowBloom = -1;
};
sLuaPostEffects g_LuaPostEffects;

static float LuaPostEffects_Clamp(float fValue, float fMin, float fMax)
{
	if (fValue < fMin) return fMin;
	if (fValue > fMax) return fMax;
	return fValue;
}

void LuaPostEffects_Apply(void)
{
	if (!master_renderer) return;
	sLuaPostEffects* p = &g_LuaPostEffects;
	if (p->iBloom >= 0) master_renderer->setBloomEnabled(p->iBloom != 0);
	if (p->fBloomThreshold >= 0) master_renderer->setBloomThreshold(p->fBloomThreshold);
	if (p->iDOF >= 0) master_renderer->setDepthOfFieldEnabled(p->iDOF != 0);
	if (p->fDOFStrength >= 0) master_renderer->setDepthOfFieldStrength(p->fDOFStrength);
	if (p->fDOFFocalLength >= 0 || p->fDOFApertureSize >= 0)
	{
		wiScene::CameraComponent& camera = wiScene::GetCamera();
		if (p->fDOFFocalLength >= 0) camera.focal_length = p->fDOFFocalLength;
		if (p->fDOFApertureSize >= 0) camera.aperture_size = p->fDOFApertureSize;
		camera.UpdateCamera();
		camera.SetDirty();
	}
	if (p->iLightShafts >= 0) master_renderer->setLightShaftsEnabled(p->iLightShafts != 0);
	if (p->iLensFlare >= 0) master_renderer->setLensFlareEnabled(p->iLensFlare != 0);
}

// values below 0 keep the current setting; ranges as the editor's sliders
void LuaPostEffects_SetBloom(int iEnabled, float fStrength, float fThreshold, float fRadius, float fCap)
{
	if (iEnabled >= 0) g_LuaPostEffects.iBloom = iEnabled ? 1 : 0;
	if (fStrength >= 0) g_LuaPostEffects.fBloomStrength = LuaPostEffects_Clamp(fStrength, 0.1f, 10.0f);
	if (fThreshold >= 0) g_LuaPostEffects.fBloomThreshold = LuaPostEffects_Clamp(fThreshold, 0.1f, 10.0f);
	if (fRadius >= 0) g_LuaPostEffects.fBloomRadius = LuaPostEffects_Clamp(fRadius, 0.0f, 2.0f);
	if (fCap >= 0) g_LuaPostEffects.fBloomCap = LuaPostEffects_Clamp(fCap, 1.0f, 10000.0f);
	LuaPostEffects_Apply();
}

void LuaPostEffects_GetBloom(int* piEnabled, float* pfStrength, float* pfThreshold, float* pfRadius, float* pfCap)
{
	*pfRadius = g_LuaPostEffects.fBloomRadius >= 0 ? g_LuaPostEffects.fBloomRadius : 0; // this engine's bloom has no radius or cap
	*pfCap = g_LuaPostEffects.fBloomCap >= 0 ? g_LuaPostEffects.fBloomCap : 0;
	*piEnabled = (master_renderer && master_renderer->getBloomEnabled()) ? 1 : 0;
	*pfStrength = g_LuaPostEffects.fBloomStrength >= 0 ? g_LuaPostEffects.fBloomStrength : 1.0f; // this engine's bloom has no strength
	*pfThreshold = master_renderer ? master_renderer->getBloomThreshold() : 0;
}

void LuaPostEffects_SetDepthOfField(int iEnabled, float fStrength, float fFocalLength, float fApertureSize)
{
	if (iEnabled >= 0) g_LuaPostEffects.iDOF = iEnabled ? 1 : 0;
	if (fStrength >= 0) g_LuaPostEffects.fDOFStrength = LuaPostEffects_Clamp(fStrength, 1.0f, 20.0f);
	if (fFocalLength >= 0) g_LuaPostEffects.fDOFFocalLength = LuaPostEffects_Clamp(fFocalLength, 0.001f, 800.0f);
	if (fApertureSize >= 0) g_LuaPostEffects.fDOFApertureSize = LuaPostEffects_Clamp(fApertureSize, 0.0f, 1.0f);
	LuaPostEffects_Apply();
}

void LuaPostEffects_GetDepthOfField(int* piEnabled, float* pfStrength, float* pfFocalLength, float* pfApertureSize)
{
	wiScene::CameraComponent& camera = wiScene::GetCamera();
	*piEnabled = (master_renderer && master_renderer->getDepthOfFieldEnabled()) ? 1 : 0;
	*pfStrength = master_renderer ? master_renderer->getDepthOfFieldStrength() : 0;
	*pfFocalLength = camera.focal_length;
	*pfApertureSize = camera.aperture_size;
}

void LuaPostEffects_SetLightShafts(int iEnabled)
{
	if (iEnabled >= 0) g_LuaPostEffects.iLightShafts = iEnabled ? 1 : 0;
	LuaPostEffects_Apply();
}

// GG: the screen-space lens flare: on, strength 0-10, threshold 0.1-10000, the ghosts' gap 0-1, the halo's radius 0-1, the
// sky left out (1) or not (0), the cap 1-10000, the bloom's threshold and cap in place of its own (1) or not (0); values
// below 0 keep the current ones
void LuaPostEffects_SetScreenLensFlare(int iEnabled, float fIntensity, float fThreshold, float fSpacing, float fHalo, int iNoSky, float fCap, int iFollowBloom)
{
	if (iEnabled >= 0) g_LuaPostEffects.iScreenLensFlare = iEnabled ? 1 : 0;
	if (fIntensity >= 0) g_LuaPostEffects.fScreenLensFlareIntensity = LuaPostEffects_Clamp(fIntensity, 0.0f, 10.0f);
	if (fThreshold >= 0) g_LuaPostEffects.fScreenLensFlareThreshold = LuaPostEffects_Clamp(fThreshold, 0.1f, 10000.0f);
	if (fSpacing >= 0) g_LuaPostEffects.fScreenLensFlareSpacing = LuaPostEffects_Clamp(fSpacing, 0.0f, 1.0f);
	if (fHalo >= 0) g_LuaPostEffects.fScreenLensFlareHalo = LuaPostEffects_Clamp(fHalo, 0.0f, 1.0f);
	if (iNoSky >= 0) g_LuaPostEffects.iScreenLensFlareNoSky = iNoSky ? 1 : 0;
	if (fCap >= 0) g_LuaPostEffects.fScreenLensFlareCap = LuaPostEffects_Clamp(fCap, 1.0f, 10000.0f);
	if (iFollowBloom >= 0) g_LuaPostEffects.iScreenLensFlareFollowBloom = iFollowBloom ? 1 : 0;
	LuaPostEffects_Apply();
}

void LuaPostEffects_GetScreenLensFlare(int* piEnabled, float* pfIntensity, float* pfThreshold, float* pfSpacing, float* pfHalo, int* piNoSky, float* pfCap, int* piFollowBloom)
{
	// this engine has no screen-space lens flare: the values a script set, else 0
	const sLuaPostEffects* p = &g_LuaPostEffects;
	*piEnabled = p->iScreenLensFlare > 0 ? 1 : 0;
	*pfIntensity = p->fScreenLensFlareIntensity >= 0 ? p->fScreenLensFlareIntensity : 0;
	*pfThreshold = p->fScreenLensFlareThreshold >= 0 ? p->fScreenLensFlareThreshold : 0;
	*pfSpacing = p->fScreenLensFlareSpacing >= 0 ? p->fScreenLensFlareSpacing : 0;
	*pfHalo = p->fScreenLensFlareHalo >= 0 ? p->fScreenLensFlareHalo : 0;
	*piNoSky = p->iScreenLensFlareNoSky > 0 ? 1 : 0;
	*pfCap = p->fScreenLensFlareCap >= 0 ? p->fScreenLensFlareCap : 0;
	*piFollowBloom = p->iScreenLensFlareFollowBloom > 0 ? 1 : 0;
}

void LuaPostEffects_GetLensFlareSun(float* pfSunHue, float* pfSunBrightness)
{
	// this engine's lens flare takes the light's own colour: the values a script set, else 0
	*pfSunHue = g_LuaPostEffects.fLensFlareSunHue >= 0 ? g_LuaPostEffects.fLensFlareSunHue : 0;
	*pfSunBrightness = g_LuaPostEffects.fLensFlareSunBrightness >= 0 ? g_LuaPostEffects.fLensFlareSunBrightness : 0;
}
void LuaPostEffects_SetLensFlare(int iEnabled, float fSunHue, float fSunBrightness)
{
	if (iEnabled >= 0) g_LuaPostEffects.iLensFlare = iEnabled ? 1 : 0;
	if (fSunHue >= 0) g_LuaPostEffects.fLensFlareSunHue = LuaPostEffects_Clamp(fSunHue, 0.0f, 1.0f);
	if (fSunBrightness >= 0) g_LuaPostEffects.fLensFlareSunBrightness = LuaPostEffects_Clamp(fSunBrightness, 0.0f, 1.0f);
	LuaPostEffects_Apply();
}

// forget the Lua values; with pVisualsToRestore (the editor's own visuals, at the end of a test game) put back the
// effects that Lua changed
void LuaPostEffects_Clear(void* pVisualsToRestore)
{
	sLuaPostEffects old = g_LuaPostEffects;
	g_LuaPostEffects = sLuaPostEffects();
	visualstype* visuals = (visualstype*)pVisualsToRestore;
	if (!visuals || !master_renderer) return;
	if (old.iBloom >= 0 || old.fBloomStrength >= 0 || old.fBloomThreshold >= 0 || old.fBloomRadius >= 0 || old.fBloomCap >= 0)
	{
		master_renderer->setBloomEnabled(visuals->bBloomEnabled);
		master_renderer->setBloomThreshold(visuals->fsetBloomThreshold);
	}
	if (old.iDOF >= 0 || old.fDOFStrength >= 0 || old.fDOFFocalLength >= 0 || old.fDOFApertureSize >= 0)
	{
		wiScene::CameraComponent& camera = wiScene::GetCamera();
		camera.focal_length = visuals->fDOFFocalLength;
		camera.aperture_size = visuals->fDOFApertureSize;
		camera.UpdateCamera();
		camera.SetDirty();
		master_renderer->setDepthOfFieldStrength(visuals->fDOFStrength);
		master_renderer->setDepthOfFieldEnabled(visuals->bDOF);
	}
	if (old.iLightShafts >= 0) master_renderer->setLightShaftsEnabled(visuals->bLightShafts);
	if (old.iLensFlare >= 0) master_renderer->setLensFlareEnabled(visuals->bLensFlare);
	if (old.iScreenLensFlare >= 0 || old.fScreenLensFlareIntensity >= 0 || old.fScreenLensFlareThreshold >= 0 || old.fScreenLensFlareSpacing >= 0 || old.fScreenLensFlareHalo >= 0 || old.iScreenLensFlareNoSky >= 0 || old.fScreenLensFlareCap >= 0 || old.iScreenLensFlareFollowBloom >= 0)
	{
	}
}

// performance levers set from Lua (SetOcclusionCulling, SetLODMultiplier, SetShadowsLowestLOD, SetDelayedShadows,
// SetShadowRange, SetSSR, SetAO): a visuals push and a graphics quality change set them from the pushed visuals, so the
// ones a script has set are applied again after both (Wicked_Update_Visuals, visuals_shaderlevels_update_core) until the
// level ends. Below 0 is unset
struct sLuaRenderSettings
{
	int iOcclusion = -1;
	int iObjectCulling = -1;
	int iAnimationCulling = -1;
	int iTerrainCulling = -1;
	int iShadowCulling = -1;
	float fLODMultiplier = -1;
	int iShadowsLowestLOD = -1;
	int iDelayedShadows = -1;
	int iDelayedShadowsLaptop = -1;
	float fShadowRange = -1;
	int iSSR = -1;
	int iAO = -1;
	float fAOPower = -1;
	int iSpotShadowCulling = -1; // spot light shadows apart from point light shadows
	int iFXAA = -1;
	int iReflections = -1;
	int iProbesLowestLOD = -1;
	int iReflectionsLowestLOD = -1;
	int iAnimations30Fps = -1;
	float fMaxApparentSize = -1;
	int iSunShadowResolution = -1;
	int iSpotShadowResolution = -1;
	int iPointShadowResolution = -1;
	int iSpotShadowMax = -1;
	int iPointShadowMax = -1;
	int iShadowCascades = -1;
	float fShadowSplits[4] = { -1, -1, -1, -1 };
	// the terrain's detail and the grass's lighting, and what they were before a script set them (the graphics quality
	// sets them from the level's own, with nothing to put back from)
	int iTerrainDetailLimit = -1;
	float fTerrainDetailScale = -1;
	int iTerrainReadBackReduction = -1;
	int iGrassSimpleLighting = -1;
	int iShadowJobWait = -1;
	int iVsync = -1;
	int iFrameRateCap = -1;
	float fRenderScale = -1;
	int iMSAA = -1;
	float fGamma = -1;
	float fFSRSharpness = -1;
	int iRaycastLowestLOD = -1;
	int iTransparentShadows = -1;
	int iTAA = -1; // this engine's temporal anti-aliasing (SetTAA)
	int iUpscaler = 0; // what SetRenderScale upscales with: 0 FSR 1, 1 FSR 2 (SetUpscaler)
	int iTerrainDetailLimitBefore = 0;
	float fTerrainDetailScaleBefore = 1.0f;
	int iTerrainReadBackReductionBefore = 4;
	int iGrassSimpleLightingBefore = 0;
};
sLuaRenderSettings g_LuaRenderSettings;

extern bool bEnableObjectCulling;
extern bool bEnableAnimationCulling;
extern bool bEnableTerrainChunkCulling;
extern bool bEnablePointShadowCulling;
extern bool bEnableSpotShadowCulling;
extern float fLODMultiplier;
extern bool bShadowsLowestLOD;
extern bool g_bDelayedShadows;
extern bool g_bDelayedShadowsLaptop;
extern bool bEnableDelayPointShadow;
extern float pointShadowScaler;
extern bool bProbesLowestLOD;
extern bool bReflectionsLowestLOD;
extern bool bEnable30FpsAnimations;
extern float maxApparentSize;
extern int g_iFrameRateCap;
extern bool bRaycastLowestLOD;
extern float g_fGlobalGammaFadeIn;
extern float g_fGlobalGammaFadeInDest;
void Wicked_Update_Shadows(void *voidvisual);

// the delayed shadow refresh as Wicked_Update_Visuals sets it: point shadows follow it, refreshed less on a laptop
static void LuaRenderSettings_SetDelayedShadowGlobals(bool bDelayed, bool bLaptop)
{
	g_bDelayedShadows = bDelayed;
	g_bDelayedShadowsLaptop = bLaptop;
	bEnableDelayPointShadow = bDelayed;
	pointShadowScaler = (bDelayed && bLaptop) ? 0.6f : 1.0f;
}

// the gamma a script or the level wants: the level start's fade-in heads for it, and outside the fade it is set straight
static void LuaRenderSettings_SetGammaTarget(float fGamma)
{
	bool bFading = g_fGlobalGammaFadeIn < g_fGlobalGammaFadeInDest;
	g_fGlobalGammaFadeInDest = fGamma;
	if (!bFading || g_fGlobalGammaFadeIn >= fGamma)
	{
		g_fGlobalGammaFadeIn = fGamma;
		if (master_renderer) master_renderer->setBrightness((fGamma - 2.2f) * 0.15f); // as the editor's Gamma slider
	}
}

bool LuaRenderSettings_TransparentShadows(bool bVisuals)
{
	return g_LuaRenderSettings.iTransparentShadows >= 0 ? g_LuaRenderSettings.iTransparentShadows != 0 : bVisuals;
}

// FSR 1 upscaling from a 3D resolution of 1 / fUpScale of the screen's (1 for none), as the level's FSR setting does it in
// Wicked_Update_Visuals (the render targets are made again, a hitch)
// the 3D drawn at 1 / fUpScale of the screen and upscaled: by FSR 1 (spatial, as the DX11 branch and this engine's own panel
// do it), or by FSR 2 (this engine's temporal upscaler, which anti-aliases as well) when SetUpscaler(1) asked for it. FSR 2
// smears and speckles until GameGuru's own terrain, tree and grass passes give it the motion it needs. 1 draws at the screen's
// size
static void LuaRenderSettings_SetFSRScale(float fUpScale)
{
	if (!master_renderer) return;
	float fScale = fUpScale > 0 ? 1.0f / fUpScale : 1.0f;
	bool bUpscale = fUpScale != 1.0f;
	bool bFSR2 = bUpscale && g_LuaRenderSettings.iUpscaler == 1;
	bool bFSR1 = bUpscale && !bFSR2;
	if (master_renderer->resolutionScale == fScale && master_renderer->getFSREnabled() == bFSR1 && master_renderer->getFSR2Enabled() == bFSR2) return;
	master_renderer->resolutionScale = fScale;
	master_renderer->setFSREnabled(bFSR1);
	master_renderer->setFSR2Enabled(bFSR2);
	if (bFSR1) master_renderer->setFSRSharpness(t.visuals.fFSRSharpness);
	if (bFSR2) master_renderer->setFSR2Sharpness(t.visuals.fFSRSharpness);
	// the texture LOD bias that goes with a temporal upscale, as the engine's FSR 2 presets set it (log2 of the scale, less one)
	wi::graphics::SamplerDesc desc = wi::renderer::GetSampler(wi::enums::SAMPLER_OBJECTSHADER)->GetDesc();
	desc.mip_lod_bias = bFSR2 ? log2f(fScale) - 1.0f : 0.0f;
	wi::renderer::ModifyObjectSampler(desc);
	master_renderer->ResizeBuffers();
}

// the upscale of the level's FSR setting (None, Ultra Quality, Quality, Balanced, Performance)
static float LuaRenderSettings_LevelFSRScale(visualstype* visuals)
{
	switch (visuals->iFSRMode)
	{
		case 1: return 1.3f;
		case 2: return 1.5f;
		case 3: return 1.7f;
		case 4: return 2.0f;
	}
	return 1.0f;
}

void LuaRenderSettings_Apply(void)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (p->iObjectCulling >= 0) bEnableObjectCulling = p->iObjectCulling != 0;
	if (p->iAnimationCulling >= 0) bEnableAnimationCulling = p->iAnimationCulling != 0;
	if (p->iTerrainCulling >= 0) bEnableTerrainChunkCulling = p->iTerrainCulling != 0;
	if (p->iShadowCulling >= 0) bEnablePointShadowCulling = bEnableSpotShadowCulling = p->iShadowCulling != 0;
	if (p->iSpotShadowCulling >= 0) bEnableSpotShadowCulling = p->iSpotShadowCulling != 0;
	if (p->iProbesLowestLOD >= 0) bProbesLowestLOD = p->iProbesLowestLOD != 0;
	if (p->iReflectionsLowestLOD >= 0) bReflectionsLowestLOD = p->iReflectionsLowestLOD != 0;
	if (p->iAnimations30Fps >= 0) bEnable30FpsAnimations = p->iAnimations30Fps != 0;
	if (p->fMaxApparentSize >= 0) maxApparentSize = p->fMaxApparentSize;
	if (p->iTerrainDetailLimit >= 0) ggterrain_global_render_params2.detailLimit = (uint32_t)p->iTerrainDetailLimit;
	if (p->fTerrainDetailScale >= 0) ggterrain_global_render_params2.detailScale = p->fTerrainDetailScale;
	if (p->iTerrainReadBackReduction >= 0) ggterrain_global_render_params2.readBackTextureReduction = (uint32_t)p->iTerrainReadBackReduction;
	if (p->iGrassSimpleLighting >= 0) gggrass_global_params.simplePBR = p->iGrassSimpleLighting;
	if (p->iRaycastLowestLOD >= 0) bRaycastLowestLOD = p->iRaycastLowestLOD != 0;
	if (p->iTAA >= 0) wiRenderer::SetTemporalAAEnabled(p->iTAA != 0);
	if (p->fGamma >= 0) LuaRenderSettings_SetGammaTarget(p->fGamma);
	if (p->iVsync >= 0) gridedit_setvsync(p->iVsync != 0);
	if (p->iFrameRateCap >= 0) g_iFrameRateCap = p->iFrameRateCap;
	if (p->iSunShadowResolution >= 0 || p->iSpotShadowResolution >= 0 || p->iPointShadowResolution >= 0 || p->iSpotShadowMax >= 0 || p->iPointShadowMax >= 0 || p->iTransparentShadows >= 0)
	{
		Wicked_Update_Shadows(&t.visuals);
	}
	if (p->iOcclusion >= 0) wiRenderer::SetOcclusionCullingEnabled(p->iOcclusion != 0);
	if (p->fLODMultiplier >= 0) fLODMultiplier = p->fLODMultiplier;
	if (p->iShadowsLowestLOD >= 0) bShadowsLowestLOD = p->iShadowsLowestLOD != 0;
	if (p->iDelayedShadows >= 0 || p->iDelayedShadowsLaptop >= 0)
	{
		bool bDelayed = p->iDelayedShadows >= 0 ? p->iDelayedShadows != 0 : g_bDelayedShadows;
		bool bLaptop = p->iDelayedShadowsLaptop >= 0 ? p->iDelayedShadowsLaptop != 0 : g_bDelayedShadowsLaptop;
		LuaRenderSettings_SetDelayedShadowGlobals(bDelayed, bLaptop);
	}
	if (p->fShadowRange >= 0) WickedCall_SetShadowRange(p->fShadowRange);
	if (master_renderer)
	{
		if (p->iSSR >= 0) master_renderer->setSSREnabled(p->iSSR != 0);
		if (p->iAO >= 0) master_renderer->setAO(p->iAO ? RenderPath3D::AO_MSAO : RenderPath3D::AO_DISABLED);
		if (p->iFXAA >= 0) master_renderer->setFXAAEnabled(p->iFXAA != 0);
		if (p->iReflections >= 0) master_renderer->setReflectionsEnabled(p->iReflections != 0);
		if (p->fAOPower >= 0) master_renderer->setAOPower(p->fAOPower);
		if (p->iMSAA >= 0 && master_renderer->getMSAASampleCount() != (uint32_t)p->iMSAA) master_renderer->setMSAASampleCount(p->iMSAA);
	}
	if (p->fRenderScale >= 0)
	{
		LuaRenderSettings_SetFSRScale(1.0f / p->fRenderScale);
		// FSR needs FXAA (Wicked_Update_Visuals)
		if (p->fRenderScale < 1.0f && master_renderer) master_renderer->setFXAAEnabled(true);
	}
	if (p->fFSRSharpness >= 0 && master_renderer) { master_renderer->setFSRSharpness(p->fFSRSharpness); master_renderer->setFSR2Sharpness(p->fFSRSharpness); }
}

// values below 0 keep the current setting
void LuaRenderSettings_SetOcclusionCulling(int iOn, int iObjects, int iAnimations, int iTerrain, int iShadows)
{
	if (iOn >= 0) g_LuaRenderSettings.iOcclusion = iOn ? 1 : 0;
	if (iObjects >= 0) g_LuaRenderSettings.iObjectCulling = iObjects ? 1 : 0;
	if (iAnimations >= 0) g_LuaRenderSettings.iAnimationCulling = iAnimations ? 1 : 0;
	if (iTerrain >= 0) g_LuaRenderSettings.iTerrainCulling = iTerrain ? 1 : 0;
	if (iShadows >= 0) g_LuaRenderSettings.iShadowCulling = iShadows ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetLODMultiplier(float fMultiplier)
{
	if (fMultiplier >= 0) g_LuaRenderSettings.fLODMultiplier = fMultiplier > 15.0f ? 15.0f : fMultiplier;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetShadowsLowestLOD(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iShadowsLowestLOD = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetDelayedShadows(int iOn, int iLaptop)
{
	if (iOn >= 0) g_LuaRenderSettings.iDelayedShadows = iOn ? 1 : 0;
	if (iLaptop >= 0) g_LuaRenderSettings.iDelayedShadowsLaptop = iLaptop ? 1 : 0;
	LuaRenderSettings_Apply();
}

// forget the script's delayed shadow values and put back those of t.visuals (the level's, or the graphics quality's)
void LuaRenderSettings_ResetDelayedShadows(void)
{
	if (g_LuaRenderSettings.iDelayedShadows < 0 && g_LuaRenderSettings.iDelayedShadowsLaptop < 0) return;
	g_LuaRenderSettings.iDelayedShadows = -1;
	g_LuaRenderSettings.iDelayedShadowsLaptop = -1;
	LuaRenderSettings_SetDelayedShadowGlobals(t.visuals.g_bDelayedShadows, t.visuals.g_bDelayedShadowsLaptop);
}

// the delayed shadow refresh in force, and whether a script has set it
void LuaRenderSettings_GetDelayedShadows(int* pOn, int* pLaptop, int* pScript)
{
	*pOn = g_bDelayedShadows ? 1 : 0;
	*pLaptop = g_bDelayedShadowsLaptop ? 1 : 0;
	*pScript = (g_LuaRenderSettings.iDelayedShadows >= 0 || g_LuaRenderSettings.iDelayedShadowsLaptop >= 0) ? 1 : 0;
}

// how far the sun's last shadow cascade reaches; kept past the fourth cascade's 30000 units and within the stock 500000.
// 0 or less goes back to the pushed visuals' range
void LuaRenderSettings_SetShadowRange(float fRange)
{
	if (fRange <= 0)
	{
		if (g_LuaRenderSettings.fShadowRange >= 0) WickedCall_SetShadowRange(t.visuals.fShadowFarPlane);
		g_LuaRenderSettings.fShadowRange = -1;
		return;
	}
	if (fRange < 31000.0f) fRange = 31000.0f;
	if (fRange > 500000.0f) fRange = 500000.0f;
	g_LuaRenderSettings.fShadowRange = fRange;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetSSR(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iSSR = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

// the power as the editor's slider, kept to 0 to 5 here (below 0 is unset)
void LuaRenderSettings_SetAO(int iOn, float fPower)
{
	if (iOn >= 0) g_LuaRenderSettings.iAO = iOn ? 1 : 0;
	if (fPower >= 0) g_LuaRenderSettings.fAOPower = fPower > 5.0f ? 5.0f : fPower;
	LuaRenderSettings_Apply();
}

// values below 0 keep the current setting. spotShadows split from shadows (SetOcclusionCulling's sixth value)
void LuaRenderSettings_SetSpotShadowCulling(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iSpotShadowCulling = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetFXAA(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iFXAA = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetReflections(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iReflections = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetLowestLOD(int iProbes, int iReflections)
{
	if (iProbes >= 0) g_LuaRenderSettings.iProbesLowestLOD = iProbes ? 1 : 0;
	if (iReflections >= 0) g_LuaRenderSettings.iReflectionsLowestLOD = iReflections ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetAnimations30Fps(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iAnimations30Fps = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

// as the visuals push keeps it: 0.000001 to 0.2
void LuaRenderSettings_SetMaxApparentSize(float fSize)
{
	if (fSize >= 0) g_LuaRenderSettings.fMaxApparentSize = fSize < 0.000001f ? 0.000001f : (fSize > 0.2f ? 0.2f : fSize);
	LuaRenderSettings_Apply();
}

// resolutions 0 (none) to 2048 as the shadow push caps them; the counts 0 to 16. A change recreates the shadow maps, a hitch
void LuaRenderSettings_SetShadowResolution(int iSun, int iSpot, int iPoint)
{
	if (iSun >= 0) g_LuaRenderSettings.iSunShadowResolution = iSun > 2048 ? 2048 : iSun;
	if (iSpot >= 0) g_LuaRenderSettings.iSpotShadowResolution = iSpot > 2048 ? 2048 : iSpot;
	if (iPoint >= 0) g_LuaRenderSettings.iPointShadowResolution = iPoint > 2048 ? 2048 : iPoint;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetShadowLights(int iSpotMax, int iPointMax)
{
	if (iSpotMax >= 0) g_LuaRenderSettings.iSpotShadowMax = iSpotMax > 16 ? 16 : iSpotMax;
	if (iPointMax >= 0) g_LuaRenderSettings.iPointShadowMax = iPointMax > 16 ? 16 : iPointMax;
	LuaRenderSettings_Apply();
}

// the pushed visuals' shadow sizes and counts, replaced by any a script set (read by Wicked_Update_Shadows)
void LuaRenderSettings_ShadowValues(int* pSun, int* pSpot, int* pPoint, int* pSpotMax, int* pPointMax)
{
	const sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (p->iSunShadowResolution >= 0) *pSun = p->iSunShadowResolution;
	if (p->iSpotShadowResolution >= 0) *pSpot = p->iSpotShadowResolution;
	if (p->iPointShadowResolution >= 0) *pPoint = p->iPointShadowResolution;
	if (p->iSpotShadowMax >= 0) *pSpotMax = p->iSpotShadowMax;
	if (p->iPointShadowMax >= 0) *pPointMax = p->iPointShadowMax;
}

// cascades 1 to 5; the four splits only if all are above 0 and rising
void LuaRenderSettings_SetShadowCascades(int iCascades, const float* pSplits)
{
	if (iCascades >= 0) g_LuaRenderSettings.iShadowCascades = iCascades < 1 ? 1 : (iCascades > 5 ? 5 : iCascades);
	if (pSplits && pSplits[0] > 0 && pSplits[1] > pSplits[0] && pSplits[2] > pSplits[1] && pSplits[3] > pSplits[2])
	{
		for (int i = 0; i < 4; i++) g_LuaRenderSettings.fShadowSplits[i] = pSplits[i];
	}
	LuaRenderSettings_Apply();
}

// detail limit 0 to 4, scale 0.25 to 2, read back reduction 4 to 6 (#113: higher leaves fallback levels unrequested)
void LuaRenderSettings_SetTerrainDetail(int iLimit, float fScale, int iReadBack)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (iLimit >= 0)
	{
		if (p->iTerrainDetailLimit < 0) p->iTerrainDetailLimitBefore = (int)ggterrain_global_render_params2.detailLimit;
		p->iTerrainDetailLimit = iLimit > 4 ? 4 : iLimit;
	}
	if (fScale >= 0)
	{
		if (p->fTerrainDetailScale < 0) p->fTerrainDetailScaleBefore = ggterrain_global_render_params2.detailScale;
		p->fTerrainDetailScale = fScale < 0.25f ? 0.25f : (fScale > 2.0f ? 2.0f : fScale);
	}
	if (iReadBack >= 0)
	{
		if (p->iTerrainReadBackReduction < 0) p->iTerrainReadBackReductionBefore = (int)ggterrain_global_render_params2.readBackTextureReduction;
		p->iTerrainReadBackReduction = iReadBack < 4 ? 4 : (iReadBack > 6 ? 6 : iReadBack);
	}
	LuaRenderSettings_Apply();
}

// the shadow job waits for the frame set up and prepass jobs even with delayed shadows off, as it does with them on
void LuaRenderSettings_SetShadowJobWait(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iShadowJobWait = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

// vsync on or off, outranking the level's and SETUP.INI's (the swap chain is made again, a blip), and the most frames a
// second, 10 to 1000 (0 for none)
void LuaRenderSettings_SetVsync(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iVsync = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetFrameRateCap(int iFramesPerSecond)
{
	if (iFramesPerSecond >= 0) g_LuaRenderSettings.iFrameRateCap = iFramesPerSecond == 0 ? 0 : (iFramesPerSecond < 10 ? 10 : (iFramesPerSecond > 1000 ? 1000 : iFramesPerSecond));
	LuaRenderSettings_Apply();
}

// the 3D drawn at this fraction of the screen's resolution, 0.5 to 1, and upscaled with FSR 1, the 2D drawn at the screen's
// (FXAA stays on below 1, which FSR needs); 0 goes back to the level's FSR setting
void LuaRenderSettings_SetRenderScale(float fScale)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (fScale == 0)
	{
		if (p->fRenderScale < 0) return;
		p->fRenderScale = -1;
		LuaRenderSettings_SetFSRScale(LuaRenderSettings_LevelFSRScale(&t.visuals));
		if (master_renderer) master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
		LuaRenderSettings_Apply();
		return;
	}
	if (fScale > 0) p->fRenderScale = fScale < 0.5f ? 0.5f : (fScale > 1.0f ? 1.0f : fScale);
	LuaRenderSettings_Apply();
}

// MSAA samples, 1, 2, 4 or 8 (the render targets are made again at the next update, a hitch); 0 goes back to the level's
void LuaRenderSettings_SetMSAA(int iSamples)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (iSamples == 0)
	{
		if (p->iMSAA < 0) return;
		p->iMSAA = -1;
		if (master_renderer && master_renderer->getMSAASampleCount() != (uint32_t)t.visuals.iMSAASampleCount) master_renderer->setMSAASampleCount(t.visuals.iMSAASampleCount);
		return;
	}
	if (iSamples > 0) p->iMSAA = iSamples >= 8 ? 8 : (iSamples >= 4 ? 4 : (iSamples >= 2 ? 2 : 1));
	LuaRenderSettings_Apply();
}

// the gamma, 0.1 to 10 (0 goes back to the level's); FSR's sharpening, 0 to 2; picks against models' lowest LOD; shadows
// through transparent surfaces (a change makes the shadow maps again, a hitch). A negative value keeps the current
void LuaRenderSettings_SetGamma(float fGamma)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (fGamma == 0)
	{
		if (p->fGamma < 0) return;
		p->fGamma = -1;
		LuaRenderSettings_SetGammaTarget(t.visuals.fGamma);
		return;
	}
	if (fGamma > 0) p->fGamma = fGamma < 0.1f ? 0.1f : (fGamma > 10.0f ? 10.0f : fGamma);
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetFSRSharpness(float fSharpness)
{
	if (fSharpness >= 0) g_LuaRenderSettings.fFSRSharpness = fSharpness > 2.0f ? 2.0f : fSharpness;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_SetRaycastLowestLOD(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iRaycastLowestLOD = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

// the engine's temporal anti-aliasing (not the DX11 branch's: this engine has it; FSR 2 upscaling anti-aliases on its own)
void LuaRenderSettings_SetTAA(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iTAA = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}
int LuaRenderSettings_GetTAA(void)
{
	return wiRenderer::GetTemporalAAEnabled() ? 1 : 0;
}
// what SetRenderScale upscales with: 0 FSR 1, 1 FSR 2; a running upscale is redone with the new one
void LuaRenderSettings_SetUpscaler(int iMode)
{
	int iNew = iMode == 1 ? 1 : 0;
	if (g_LuaRenderSettings.iUpscaler == iNew) return;
	g_LuaRenderSettings.iUpscaler = iNew;
	if (g_LuaRenderSettings.fRenderScale > 0 && g_LuaRenderSettings.fRenderScale < 1.0f) LuaRenderSettings_SetFSRScale(1.0f / g_LuaRenderSettings.fRenderScale);
}
int LuaRenderSettings_GetUpscaler(void)
{
	return g_LuaRenderSettings.iUpscaler;
}
void LuaRenderSettings_SetTransparentShadows(int iOn)
{
	if (iOn >= 0) g_LuaRenderSettings.iTransparentShadows = iOn ? 1 : 0;
	LuaRenderSettings_Apply();
}

void LuaRenderSettings_GetPicture(float* pGamma, float* pFSRSharpness, int* pRaycastLowestLOD, int* pTransparentShadows)
{
	*pGamma = g_fGlobalGammaFadeInDest;
	*pFSRSharpness = g_LuaRenderSettings.fFSRSharpness >= 0 ? g_LuaRenderSettings.fFSRSharpness : t.visuals.fFSRSharpness;
	*pRaycastLowestLOD = bRaycastLowestLOD ? 1 : 0;
	*pTransparentShadows = LuaRenderSettings_TransparentShadows(t.visuals.bTransparentShadows) ? 1 : 0;
}

// the culling switches in force (SetOcclusionCulling) and the lowest-LOD shadows (SetShadowsLowestLOD)
void LuaRenderSettings_GetCulling(int* pOcclusion, int* pObjects, int* pAnimations, int* pTerrain, int* pShadows, int* pSpotShadows, int* pShadowsLowestLOD)
{
	*pOcclusion = wiRenderer::GetOcclusionCullingEnabled() ? 1 : 0;
	*pObjects = bEnableObjectCulling ? 1 : 0;
	*pAnimations = bEnableAnimationCulling ? 1 : 0;
	*pTerrain = bEnableTerrainChunkCulling ? 1 : 0;
	*pShadows = bEnablePointShadowCulling ? 1 : 0;
	*pSpotShadows = bEnableSpotShadowCulling ? 1 : 0;
	*pShadowsLowestLOD = bShadowsLowestLOD ? 1 : 0;
}

int LuaRenderSettings_GetMSAA(void)
{
	return master_renderer ? (int)master_renderer->getMSAASampleCount() : 1;
}

float LuaRenderSettings_GetRenderScale(void)
{
	return master_renderer ? master_renderer->resolutionScale : 1.0f;
}

void LuaRenderSettings_GetDisplay(int* pVsync, int* pFrameRateCap)
{
	*pVsync = master.bVsyncEnabled ? 1 : 0;
	*pFrameRateCap = g_iFrameRateCap;
}

// the values in force of the levers batch 12 added (SetSSR, SetAO, SetLODMultiplier, SetShadowRange)
void LuaRenderSettings_GetLevers(int* pSSR, int* pAO, float* pAOPower, float* pLODMultiplier, float* pShadowRange)
{
	*pSSR = master_renderer ? (master_renderer->getSSREnabled() ? 1 : 0) : 0;
	*pAO = master_renderer ? (master_renderer->getAO() != RenderPath3D::AO_DISABLED ? 1 : 0) : 0;
	*pAOPower = master_renderer ? master_renderer->getAOPower() : 0;
	*pLODMultiplier = fLODMultiplier;
	extern float fWickedCallShadowFarPlane;
	*pShadowRange = fWickedCallShadowFarPlane;
}

void LuaRenderSettings_SetGrassSimpleLighting(int iOn)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (iOn >= 0)
	{
		if (p->iGrassSimpleLighting < 0) p->iGrassSimpleLightingBefore = gggrass_global_params.simplePBR;
		p->iGrassSimpleLighting = iOn ? 1 : 0;
	}
	LuaRenderSettings_Apply();
}

// the values in force, for a graphics menu (GetGraphicsSettings)
void LuaRenderSettings_GetValues(int* pFXAA, int* pReflections, int* pProbesLowestLOD, int* pReflectionsLowestLOD, int* pAnimations30Fps, float* pMaxApparentSize)
{
	*pFXAA = master_renderer ? (master_renderer->getFXAAEnabled() ? 1 : 0) : 0;
	*pReflections = master_renderer ? (master_renderer->getReflectionsEnabled() ? 1 : 0) : 0;
	*pProbesLowestLOD = bProbesLowestLOD ? 1 : 0;
	*pReflectionsLowestLOD = bReflectionsLowestLOD ? 1 : 0;
	*pAnimations30Fps = bEnable30FpsAnimations ? 1 : 0;
	*pMaxApparentSize = maxApparentSize;
}

void LuaRenderSettings_GetShadows(int* pSun, int* pSpot, int* pPoint, int* pSpotMax, int* pPointMax, int* pCascades, float* pSplits)
{
	*pSun = t.visuals.iShadowSpotCascadeResolution;
	*pSpot = t.visuals.iShadowSpotResolution;
	*pPoint = t.visuals.iShadowPointResolution;
	*pSpotMax = t.visuals.iShadowSpotMax;
	*pPointMax = t.visuals.iShadowPointMax;
	LuaRenderSettings_ShadowValues(pSun, pSpot, pPoint, pSpotMax, pPointMax);
	if (*pSun > 2048) *pSun = 2048;
	if (*pSpot > 2048) *pSpot = 2048;
	if (*pPoint > 2048) *pPoint = 2048;
}

void LuaRenderSettings_GetTerrainDetail(int* pLimit, float* pScale, int* pReadBack, int* pGrassSimpleLighting)
{
	*pLimit = (int)ggterrain_global_render_params2.detailLimit;
	*pScale = ggterrain_global_render_params2.detailScale;
	*pReadBack = (int)ggterrain_global_render_params2.readBackTextureReduction;
	*pGrassSimpleLighting = gggrass_global_params.simplePBR;
}

// the graphics values a script had set (old) put back to those of the visuals (the level's and the graphics quality's), and
// the terrain and grass to what they were before the script set them
static void LuaRenderSettings_RestoreGraphics(const sLuaRenderSettings& old, visualstype* visuals)
{
	if (old.iObjectCulling >= 0) bEnableObjectCulling = visuals->bEnableObjectCulling;
	if (old.iAnimationCulling >= 0) bEnableAnimationCulling = visuals->bEnableAnimationCulling;
	if (old.iTerrainCulling >= 0) bEnableTerrainChunkCulling = visuals->bEnableTerrainChunkCulling;
	if (old.iShadowCulling >= 0 || old.iSpotShadowCulling >= 0)
	{
		bEnablePointShadowCulling = visuals->bEnablePointShadowCulling;
		bEnableSpotShadowCulling = visuals->bEnableSpotShadowCulling;
	}
	if (old.iOcclusion >= 0) wiRenderer::SetOcclusionCullingEnabled(visuals->bOcclusionCulling);
	if (old.iShadowsLowestLOD >= 0) bShadowsLowestLOD = visuals->bShadowsLowestLOD;
	if (old.iProbesLowestLOD >= 0) bProbesLowestLOD = visuals->bProbesLowestLOD;
	if (old.iReflectionsLowestLOD >= 0) bReflectionsLowestLOD = visuals->bReflectionsLowestLOD;
	if (old.iAnimations30Fps >= 0) bEnable30FpsAnimations = visuals->bEnable30FpsAnimations;
	if (old.fMaxApparentSize >= 0) maxApparentSize = visuals->ApparentSize;
	if (old.iShadowCascades >= 0 || old.fShadowSplits[0] >= 0)
	{
		const float fSplits[4] = { 380.0f, 950.0f, 7500.0f, 30000.0f };
	}
	if (old.iTerrainDetailLimit >= 0) ggterrain_global_render_params2.detailLimit = (uint32_t)old.iTerrainDetailLimitBefore;
	if (old.fTerrainDetailScale >= 0) ggterrain_global_render_params2.detailScale = old.fTerrainDetailScaleBefore;
	if (old.iTerrainReadBackReduction >= 0) ggterrain_global_render_params2.readBackTextureReduction = (uint32_t)old.iTerrainReadBackReductionBefore;
	if (old.iGrassSimpleLighting >= 0) gggrass_global_params.simplePBR = old.iGrassSimpleLightingBefore;
	if (old.iSunShadowResolution >= 0 || old.iSpotShadowResolution >= 0 || old.iPointShadowResolution >= 0 || old.iSpotShadowMax >= 0 || old.iPointShadowMax >= 0 || old.iTransparentShadows >= 0)
	{
		Wicked_Update_Shadows(visuals);
	}
	if (old.fGamma >= 0) LuaRenderSettings_SetGammaTarget(visuals->fGamma);
	if (old.fFSRSharpness >= 0 && master_renderer) { master_renderer->setFSRSharpness(visuals->fFSRSharpness); master_renderer->setFSR2Sharpness(visuals->fFSRSharpness); }
	if (old.iRaycastLowestLOD >= 0) bRaycastLowestLOD = visuals->bRaycastLowestLOD;
	if (old.iTAA >= 0) wiRenderer::SetTemporalAAEnabled(false); // the stock engine runs without it
	if (old.iFXAA >= 0 && master_renderer) master_renderer->setFXAAEnabled(visuals->bFXAAEnabled);
	if (old.iReflections >= 0 && master_renderer) master_renderer->setReflectionsEnabled(visuals->bReflectionsEnabled);
	if (old.iVsync >= 0) gridedit_setvsync(visuals->bLevelVSyncEnabled && g.gvsync != 0);
	if (old.iFrameRateCap >= 0) g_iFrameRateCap = 0;
	if (old.fRenderScale >= 0)
	{
		LuaRenderSettings_SetFSRScale(LuaRenderSettings_LevelFSRScale(visuals));
		if (master_renderer) master_renderer->setFXAAEnabled(visuals->bFXAAEnabled);
	}
	if (old.iMSAA >= 0 && master_renderer && master_renderer->getMSAASampleCount() != (uint32_t)visuals->iMSAASampleCount) master_renderer->setMSAASampleCount(visuals->iMSAASampleCount);
	if (old.fLODMultiplier >= 0) fLODMultiplier = visuals->fLODMultiplier;
	if (old.fShadowRange >= 0) WickedCall_SetShadowRange(visuals->fShadowFarPlane);
	if (old.iSSR >= 0 && master_renderer) master_renderer->setSSREnabled(visuals->bSSREnabled);
	if ((old.iAO >= 0 || old.fAOPower >= 0) && master_renderer)
	{
		// as the last visuals push left it
		master_renderer->setAO(master.iAOSetting > 0 ? RenderPath3D::AO_MSAO : RenderPath3D::AO_DISABLED);
		if (master.iAOSetting > 0) master_renderer->setAOPower(master.fAOPower);
	}
}

// forget a script's graphics values (SetFXAA, SetReflections, SetProbesLowestLOD, SetReflectionsLowestLOD,
// SetAnimations30Fps, SetMaxApparentSize, SetShadowResolution, SetShadowLights, SetShadowCascades, SetTerrainDetail,
// SetGrassSimpleLighting, SetVsync, SetFrameRateCap, SetRenderScale, SetMSAA, SetSSR, SetAO, SetLODMultiplier, SetShadowRange,
// SetTreeDistance, SetTreeTransition, SetGrassDistance, SetOcclusionCulling, SetShadowsLowestLOD, SetGamma, SetFSRSharpness,
// SetRaycastLowestLOD, SetTransparentShadows) and put back the level's and the graphics quality's, so a quality change
// moves them again (ResetGraphicsSettings); the other levers stay as set
void LuaRenderSettings_ResetGraphics(void)
{
	sLuaRenderSettings old = g_LuaRenderSettings;
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	p->iSpotShadowCulling = -1;
	p->iFXAA = -1;
	p->iReflections = -1;
	p->iProbesLowestLOD = -1;
	p->iReflectionsLowestLOD = -1;
	p->iAnimations30Fps = -1;
	p->fMaxApparentSize = -1;
	p->iSunShadowResolution = -1;
	p->iSpotShadowResolution = -1;
	p->iPointShadowResolution = -1;
	p->iSpotShadowMax = -1;
	p->iPointShadowMax = -1;
	p->iShadowCascades = -1;
	for (int i = 0; i < 4; i++) p->fShadowSplits[i] = -1;
	p->iTerrainDetailLimit = -1;
	p->fTerrainDetailScale = -1;
	p->iTerrainReadBackReduction = -1;
	p->iGrassSimpleLighting = -1;
	p->iVsync = -1;
	p->iFrameRateCap = -1;
	p->fRenderScale = -1;
	p->iMSAA = -1;
	p->iOcclusion = -1;
	p->iObjectCulling = -1;
	p->iAnimationCulling = -1;
	p->iTerrainCulling = -1;
	p->iShadowCulling = -1;
	p->iShadowsLowestLOD = -1;
	p->fGamma = -1;
	p->fFSRSharpness = -1;
	p->iRaycastLowestLOD = -1;
	p->iTransparentShadows = -1;
	p->iTAA = -1;
	p->iUpscaler = 0;
	p->iSSR = -1;
	p->iAO = -1;
	p->fAOPower = -1;
	p->fLODMultiplier = -1;
	p->fShadowRange = -1;
	LuaRenderSettings_RestoreGraphics(old, &t.visuals);

	LuaRenderSettings_Apply();
}

// after the graphics quality has set the terrain and grass from the level's own, those are what a script's values give back
void LuaRenderSettings_TakeQualityValues(void)
{
	sLuaRenderSettings* p = &g_LuaRenderSettings;
	if (p->iTerrainDetailLimit >= 0) p->iTerrainDetailLimitBefore = (int)ggterrain_global_render_params2.detailLimit;
	if (p->fTerrainDetailScale >= 0) p->fTerrainDetailScaleBefore = ggterrain_global_render_params2.detailScale;
	if (p->iTerrainReadBackReduction >= 0) p->iTerrainReadBackReductionBefore = (int)ggterrain_global_render_params2.readBackTextureReduction;
	if (p->iGrassSimpleLighting >= 0) p->iGrassSimpleLightingBefore = gggrass_global_params.simplePBR;
}

// forget the Lua values and put back those of t.visuals (the level's, or the editor's when a test game ends)
void LuaRenderSettings_Clear(void)
{
	sLuaRenderSettings old = g_LuaRenderSettings;
	g_LuaRenderSettings = sLuaRenderSettings();
	visualstype* visuals = &t.visuals;
	if (old.iDelayedShadows >= 0 || old.iDelayedShadowsLaptop >= 0) LuaRenderSettings_SetDelayedShadowGlobals(visuals->g_bDelayedShadows, visuals->g_bDelayedShadowsLaptop);
	LuaRenderSettings_RestoreGraphics(old, visuals);
}
