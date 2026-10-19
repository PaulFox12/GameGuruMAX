//
// Wicked Calls Header
//
#pragma once

// Includes
#include <memory>
#include <vector>
#include <string>
#include "..\Dark Basic Public Shared\Dark Basic Pro SDK\Shared\Objects\CObjectDataC.h"

// Helps with CRASH LOG collection
extern thread_local char g_CrashContext[1024];
#define GG_CRASH_CONTEXT(fn, fmt, ...)                          \
    extern int g_iDisableCrashLogSystem;                         \
    if(g_iDisableCrashLogSystem==0)                             \
    {                                                          \
        do {                                                        \
            char _gg_tmp[256];                                     \
            int _n = snprintf(_gg_tmp, sizeof(_gg_tmp),             \
                               " %s: " fmt "\r\n",                    \
                               fn, __VA_ARGS__);                   \
            if (_n > 0)                                             \
            {                                                       \
                size_t cur = strlen(g_CrashContext);               \
                if (cur + _n >= sizeof(g_CrashContext))            \
                {                                                   \
                    memmove(g_CrashContext,                         \
                            g_CrashContext + (cur + _n - sizeof(g_CrashContext)), \
                            sizeof(g_CrashContext));               \
                    cur = strlen(g_CrashContext);                  \
                }                                                   \
                strncat(g_CrashContext, _gg_tmp,                    \
                        sizeof(g_CrashContext) - cur - 1);         \
            }                                                       \
        } while (0);                                                \
    }

#define GG_CRASH_CONTEXT_NO_FLAG(fn, fmt, ...)                          \
    {                                                          \
        do {                                                        \
            char _gg_tmp[256];                                     \
            int _n = snprintf(_gg_tmp, sizeof(_gg_tmp),             \
                               " %s: " fmt "\r\n",                    \
                               fn, __VA_ARGS__);                   \
            if (_n > 0)                                             \
            {                                                       \
                size_t cur = strlen(g_CrashContext);               \
                if (cur + _n >= sizeof(g_CrashContext))            \
                {                                                   \
                    memmove(g_CrashContext,                         \
                            g_CrashContext + (cur + _n - sizeof(g_CrashContext)), \
                            sizeof(g_CrashContext));               \
                    cur = strlen(g_CrashContext);                  \
                }                                                   \
                strncat(g_CrashContext, _gg_tmp,                    \
                        sizeof(g_CrashContext) - cur - 1);         \
            }                                                       \
        } while (0);                                                \
    }

// Custom Layers
#ifndef GGRENDERLAYERSENUM
#define GGRENDERLAYERSENUM
enum GGRENDERLAYERS
{
	GGRENDERLAYERS_VOID = 0,
	GGRENDERLAYERS_NORMAL = 1,
	GGRENDERLAYERS_CURSOROBJECT = 2,
	GGRENDERLAYERS_WIDGETPLANE = 4,
	GGRENDERLAYERS_TERRAIN = 8,
	GGRENDERLAYERS_ALL = GGRENDERLAYERS_NORMAL | GGRENDERLAYERS_CURSOROBJECT | GGRENDERLAYERS_WIDGETPLANE | GGRENDERLAYERS_TERRAIN
};
#endif

// pre-declare Wicked classes
//struct wi::Resource; DX12 duplicate in wi::Resource.h
#undef WICKEDENGINE
#include "../../../WickedEngineDX12/WickedEngine/WickedEngine.h"
//#include "../../../WickedEngineDX12/WickedEngine/wiGraphics.h"
//#include "D:\PROTOTIME\WickedEngineDX12\WickedEngine\wiScene.h"
//#include "D:\PROTOTIME\WickedEngineDX12\WickedEngine\wiECS.h"
//#include "D:\PROTOTIME\WickedEngineDX12\WickedEngine\wiEmittedParticle.h"
//#include "D:\PROTOTIME\WickedEngineDX12\WickedEngine\wiPrimitive.h"
//#include "D:\PROTOTIME\WickedEngineDX12\WickedEngine\wiResourceManager.h"
//namespace wiGraphics = wi::graphics;
//namespace wiScene = wi::scene;
//namespace wiECS = wi::ecs;
//using wiEmittedParticle = wi::EmittedParticleSystem;
//using RAY = wi::primitive::Ray;

// Image Management
#ifndef GGIMAGERESENUM
#define GGIMAGERESENUM
enum eImageResType
{
	IMAGERES_GENERIC,
	IMAGERES_UI,
	IMAGERES_LEVEL
};
struct sImageList
{
	LPSTR pName;
	eImageResType eType;
	wi::Resource image;
	int iMemUsedKB = 0;
	uint32_t MasterObject = 0;
};
#endif

void WickedCall_InitImageManagement(LPSTR pRootFolder);
void WickedCall_FreeImage(sImageList* pImage);
void WickedCall_FreeAllImagesOfType(eImageResType eType);
void WickedCall_GetRelativeAfterRoot(std::string pFilename, LPSTR pFullRelativeLocationFilename);
int WickedCall_FindImageIndexInList(std::string pFilenameToFind, LPSTR pFullRelativeLocationFilename);
void WickedCall_AddImageToList(wi::Resource image, eImageResType eType, std::string pFilenameRef, int iKbused);
// GGMAX texture streaming (2026-08-01): material textures stream by default. Pass
// bAllowStreaming=false for any texture NOT sampled by object/particle shaders
// (sky, lens flares, HUD/2D) — those never write mip feedback, so streaming would
// decay them to minimum resolution (permanent blur).
// NOTE: no default argument here — this header has no include guard and is re-included
// within single translation units; a repeated default argument is C2572. The 2-arg
// overload forwards with bAllowStreaming=true (streaming is the default).
wi::Resource WickedCall_LoadImage(std::string pFilenameToLoad, eImageResType eType, bool bAllowStreaming);
wi::Resource WickedCall_LoadImage(std::string pFilenameToLoad, eImageResType eType);
wi::Resource WickedCall_LoadImage(std::string pFilenameToLoad);
void WickedCall_DeleteImage(std::string pFilenameToLoad);
extern bool g_bTextureStreamingEnabled; // kill-switch (harness SET_TEXSTREAM); affects loads after change

// Functions
void WickedCall_RefreshObjectAnimations(sObject* pObject, void* pstate);
void WickedCall_SanitizeSkeletons(void);
void WickedCall_BuildStaticMeshBVHs(void);
void WickedCall_AddObject( sObject* pObject );
void WickedCall_SetObjectSpeed(sObject* pObject, float fSpeed);
void WickedCall_CheckAnimationDone(sObject* pObject);
bool WickedCall_GetAnimationPlayingState (sObject* pObject);
void WickedCall_SetAnimationLerpFactor (sObject* pObject);
void WickedCall_PlayObject(sObject* pObject, float fStart, float fEnd, bool bLooped);
void WickedCall_InstantObjectFrameUpdate(sObject* pObject);
void WickedCall_GetObjectAnimationData(sObject* pObject, float* pStart, float* pFinish);
void WickedCall_StopObject(sObject* pObject);
void WickedCall_SetObjectFrame(sObject* pObject, float fFrame);
void WickedCall_SetObjectFrameEx(sObject* pObject, float fFrame);
float WickedCall_GetObjectFrame(sObject* pObject);
float WickedCall_GetObjectRealFrame(sObject* pObject);
bool WickedCall_GetObjectPlaying(sObject* pObject);
void WickedCall_RemoveObject( sObject* pObject );
void WickedCall_SetTexturePath ( LPSTR pPath );
void WickedCall_TextureMesh ( sMesh* pMesh );
void WickedCall_SetReflectance (sMesh* pMesh, float fReflectance);
void WickedCall_SetObjectReflectance(sObject* pObject, float fReflectance);
float WickedCall_GetObjectReflectance(sObject* pObject);
void WickedCall_SetMeshMaterial(sMesh* pMesh, bool bForce);
void WickedCall_TextureObjectAsEmissive(sObject* pObject);
void WickedCall_SetAsEmissiveMaterial(sMesh* pMesh);
void WickedCall_TextureObject ( sObject* pObject, sMesh* pJustThisMesh );
void WickedCall_SetObjectBaseColor(sObject* pObject, int r, int g, int b);
void WickedCall_GetObjectBaseColor(sObject* pObject, int* r, int* g, int* b);
void WickedCall_SetObjectEmissiveColor(sObject* pObject, int r, int g, int b);
void WickedCall_CopyMaterialColorsToMeshes(sObject* pObject);
void WickedCall_SetMeshBaseColor(sMesh* pMesh, int r, int g, int b);
void WickedCall_GetMeshBaseColor(sMesh* pMesh, int* r, int* g, int* b);
void WickedCall_SetMeshEmissiveColor(sMesh* pMesh, int r, int g, int b);
void WickedCall_GetMeshEmissiveColor(sMesh* pMesh, int* r, int* g, int* b);
void WickedCall_GetObjectEmissiveColor(sObject* pObject, int* r, int* g, int* b);
void WickedCall_SetObjectNormalness(sObject* pObject, float normalness);
float WickedCall_GetObjectNormalness(sObject* pObject);
void WickedCall_SetObjectRoughness(sObject* pObject, float roughness);
float WickedCall_GetObjectRoughness(sObject* pObject);
void WickedCall_SetObjectMetalness(sObject* pObject, float metalness);
float WickedCall_GetObjectMetalness(sObject* pObject);
void WickedCall_SetObjectEmissiveStrength(sObject* pObject, float strength);
float WickedCall_GetObjectEmissiveStrength(sObject* pObject);
void WickedCall_SetObjectTransparentDirect(sObject* pObject, bool bTransparent);
bool WickedCall_GetObjectTransparentDirect(sObject* pObject);
void WickedCall_TextureMeshWithImagePtr(sMesh* pMesh, int iPutInEmissivemode);
void WickedCall_TextureObjectWithImagePtr ( sObject* pObject, int iPutInEmissivemode );
void WickedCall_UpdateObject(sObject* pObject);
void WickedCall_UpdateLimbsOfObject( sObject* pObject );
void WickedCall_UpdateSceneForPick(void);
void WickedCall_SetRenderOrderBias(sMesh* pMesh, float fDistanceToAdd);
float WickedCall_GetRenderOrderBias(sMesh* pMesh);
void WickedCall_SetObjectRenderOrderBias(sObject* pObject, float fRenderOrderBias);
float WickedCall_GetObjectRenderOrderBias(sObject* pObject);
void WickedCall_SetObjectPlanerReflection(sObject* pObject, bool bPlanerReflection);
bool WickedCall_GetObjectPlanerReflection(sObject* pObject);
void WickedCall_SetLimbVisible(sFrame* pFrame, bool bVisible);
void WickedCall_SetObjectPreventAnyApparentOcclusion (sObject* pObject, bool bPreventAnyApparentOcclusion);
void WickedCall_SetDisableCollision(sObject* pObject, bool collision);
void WickedCall_SetObjectVisible ( sObject* pObject, bool bVisible );
void WickedCall_GlueObjectToObject ( sObject* pObjectToGlue, sObject* pParentObject, int iLimb, int iObjIDToSyncAnimTo, int iWorldToLocal);
void WickedCall_UnGlueObjectToObject ( sObject* pObjectToUnGlue);
void WickedCall_PresetObjectRenderLayer(int iLayerMask);
void WickedCall_PresetObjectLimbRenderLayer(int iLayerMask, int iLimb);
void WickedCall_PresetObjectUVScale(float fUScale, float fVScale);
void WickedCall_PresetObjectCreateOnDemand(bool bCreateOnlyWhenUsed);
void WickedCall_PresetObjectIgnoreTextures(bool bIgnoreTextureInfo);
void WickedCall_PresetObjectTextureFromImagePtr(bool bUseImagePtrInsteadOfTexFile, int iPutInEmissiveMode);
void WickedCall_PresetObjectPutInEmissive(int iPutInEmissiveMode);
void WickedCall_RotateLimb(sObject* pObject, sFrame* pFrame, float fAX, float fAY, float fAZ);
void WickedCall_CalculateWorldQuat(GGVECTOR4* pQuatA, GGVECTOR4* pQuatB, GGVECTOR4* pQuatResult);
void WickedCall_CalculateQuatDiff(GGVECTOR4* pQuatA, GGVECTOR4* pQuatB, GGVECTOR4* pQuatDiff);
void WickedCall_CalculateQuatFromCombined(sObject* pObject, sFrame* pFrame, GGVECTOR4* pQuat);
bool WickedCall_ObjectRay(sObject* pObject, const float* pFrom, const float* pDir, float fLength, float* pDistance, float* pNormal, int* pLimb);
void WickedCall_OverrideLimbWithCombined(sObject* pObject, sFrame* pFrame, bool bIncludeTranslation);
void WickedCall_OverrideLimbOff(sObject* pObject, sFrame* pFrame);
void WickedCall_SetBip01Position(sObject* pObject, sFrame* pFrame, int iUseMode, float fX, float fZ);
void WickedCall_SetBip01Rotation(sObject* pObject, sFrame* pFrame, int iUseMode, float fY);
void WickedCall_SetObjectPreFrames(sObject* pObject, LPSTR pParentFrameName, float fFrameToUse, float fSmoothSlerpToNextShape, int iPreFrameMode);
void WickedCall_SetObjectRenderLayer ( sObject* pObject, int iLayerMask );
bool Convert2Dto3D(long x, long y, float* pOutX, float* pOutY, float* pOutZ, float* pDirX, float* pDirY, float* pDirZ);
bool WickedCall_GetPick2(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask);
bool WickedCall_GetPick2_Thread(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask);
bool WickedCall_GetPick2_BVH(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask);
bool WickedCall_GetPick2_OLD(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask);
bool WickedCall_GetPick ( float* pOutX, float* pOutY, float* pOutZ, float* pNormalX, float* pNormalY, float* pNormalZ, uint64_t* pObject, int iLayerMask );
// GGMAX 2.64: does a pick-hit wicked entity belong to the given DBO object number? The scene
// pick's layer mask cannot EXCLUDE terrain chunks (entities without a LayerComponent default
// to ALL layer bits, so they pass every mask) — callers needing "did I hit THIS object" must
// test the hit entity, not just hit success (Terrain Generator marker grab).
bool WickedCall_IsEntityOfObject(uint64_t iEntityID, int iObjectNumber);
bool WickedCall_SentRay(float originx, float originy, float originz, float directionx, float directiony, float directionz, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask);
bool WickedCall_SentRay2(float originx, float originy, float originz, float directionx, float directiony, float directionz, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask);
bool WickedCall_SentRay3(float originx, float originy, float originz, float directionx, float directiony, float directionz, float fDistanceOfRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit);
bool WickedCall_SentRay4(float originx, float originy, float originz, float directionx, float directiony, float directionz, float fDistanceOfRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit, bool bOpaqueOnly);
// pIgnore1 and pIgnore2 (or NULL) are left out of the pick without changing their layers, as a thread must not
bool WickedCall_SentRay4_ThreadSafe(float originx, float originy, float originz, float directionx, float directiony, float directionz, float fDistanceOfRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit, bool bOpaqueOnly, sObject* pIgnore1 = NULL, sObject* pIgnore2 = NULL);
void WickedCall_GetMouseDeltas(float* pfX, float* pfY);
uint32_t WickedCall_GetTextureWidth(void* ptex);
uint32_t WickedCall_GetTextureHeight(void* ptex);
void WickedCall_SetObjectOutline(sObject* pObject, float fHighlight);
bool WickedCall_GetObjectOutline(sObject* pObject);
void WickedCall_SetObjectHighlightColor(sObject* pObject, bool bHighlight, int highlightColorType);
void WickedCall_SetObjectEmissiveTint(sObject* pObject, float fRed, float fGreen, float fBlue, float fStrength);
bool WickedCall_GetObjectEmissiveTint(sObject* pObject, float* pfRed, float* pfGreen, float* pfBlue, float* pfStrength);
void WickedCall_SetObjectColorTint(sObject* pObject, float fRed, float fGreen, float fBlue);
bool WickedCall_GetObjectColorTint(sObject* pObject, float* pfRed, float* pfGreen, float* pfBlue);
void WickedCall_SetLimbAlpha(sObject* pObject, int iLimb, float fAlpha, bool bChildren);
float WickedCall_GetLimbAlpha(sObject* pObject, int iLimb);
void WickedCall_SetObjectHighlightRed(sObject* pObject, bool bHighlight);
void WickedCall_SetObjectHighlightBlue(sObject* pObject, bool bHighlight);
void WickedCall_RenderEditorFunctions(void);
void WickedCall_SetSelectedObject(sObject* pObject);
uint64_t WickedCall_GetFirstRootEntityID(sObject* pObject);
int WickedCall_GetNumberOfRootEntityIDs(sObject* pObject);
void WickedCall_SetMeshTransparent(sMesh* pMesh);
void WickedCall_SetMeshCullmode(sMesh* pMesh);
void WickedCall_SetObjectCullmode(sObject* pObject);
void WickedCall_SetObjectDoubleSided(sObject* pObject, bool bDoubleSided);
void WickedCall_SetObjectHighlight(sObject* pObject, bool bHighlight);
bool WickedCall_GetObjectDoubleSided(sObject* pObject);
void WickedCall_SetObjectTransparent(sObject* pObject);
void WickedCall_SetObjectDisableDepth(sObject* pObject, bool bDisable);
std::string WickedCall_GetAllTexturesUsed(sObject* pObject);
void WickedCall_SetMeshAlpha(sMesh* pMesh, float fPercentage);
LPSTR WickedCall_GetMeshMaterialName(sMesh* pMesh);
void WickedCall_GetFrameWorldPos(sFrame* pFrame, float* pfX, float* pfY, float* pfZ);
void WickedCall_GetGluedLimbWorldPos(sObject* pObject, int iLimbID, float* pfX, float* pfY, float* pfZ);
void WickedCall_GetLimbDataEx(sObject* pObject, int iLimbID, bool bAdjustLimb, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, float* pX, float* pY, float* pZ, float* pQAX, float* pQAY, float* pQAZ, float* pQAW);
void WickedCall_GetLimbLocalPosAndRot(sObject* pObject, int iLimbID, float* pX, float* pY, float* pZ, float* pQAX, float* pQAY, float* pQAZ, float* pQAW);
void WickedCall_GetLimbLocalAndWorld(sObject* pObject, int iLimbID, float* pLocal, float* pWorld);
bool WickedCall_GetLimbWorldsAtFrame(sObject* pObject, float fFrame, const float* pMadeLocals, const float* pMadeWorlds, float* pWorlds);
void WickedCall_GetLimbData(sObject* pObject, int iLimbID, float* pX, float* pY, float* pZ, float* pQAX, float* pQAY, float* pQAZ, float* pQAW);
void WickedCall_UpdateMeshVertexData(sMesh* pMesh);
void WickedCall_SetObjectAlpha(sObject* pObject, float fPercentage);
float WickedCall_GetObjectAlpha(sObject* pObject);
void WickedCall_SetObjectCastShadows(sObject* pObject, bool bCastShadow);
bool WickedCall_GetObjectCastShadows(sObject* pObject);
void WickedCall_SetObjectTextureUV(sObject* pObject, float x, float y, float z, float w);
void WickedCall_GetObjectTextureUV(sObject* pObject, float* x, float* y, float* z, float* w);
void WickedCall_SetObjectLightToUnlit(sObject* pObject, int shaderType);
uint64_t WickedCall_AddLight(int iLightType);
void WickedCall_DeleteLight(uint64_t wickedlightindex);
void WickedCall_UpdateLight(uint64_t wickedlightindex, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, float fRange, float fSpotRadius, int iColR, int iColG, int iColB, bool bCastShadow);
void WickedCall_UpdateProbes(void);
int WickedCall_Get2DShadowLights(void);
int WickedCall_GetCubeShadowLights(bool bDebug);
int WickedCall_GetSpotShadowLights(bool bDebug);
int WickedCall_GetFrustumVisibleObjects(void); // # of scene objects surviving main-camera frustum cull this frame
void WickedCall_SetEditorCameraLight(bool bSwitchOn);
void WickedCall_SetSpriteBoundBox(bool bShow,float fX1, float fY1,float fX2, float fY2);
void WickedCall_SetSunDirection(float fAx, float fAy, float fAz);
void WickedCall_SetSunColors(float fRed, float fGreen, float fBlue, float fEnergy, float fFov, float fShadowBias);
void WickedCall_SetVisualizerEnabled(bool bVisualizer);
void WickedCall_SunSetVolumetricsEnabled(bool bVolumetrics);
void WickedCall_SunSetSetStatic(bool bSetStatic);
void WickedCall_SunSetRange(float fRange);
void WickedCall_SetTextureName(int obj, char *texturename);
void WickedCall_DrawImguiNow(void);
void WickedCall_SetCameraFOV(float fFOV);
int WickedCall_GetSkinable(void);
int WickedCall_GetSkinableVisible(void);

void WickedCall_SetShadowRange(float ShadowFar);
void WickedCall_UpdateCharacterShadows(void); // GGMAX 2026-07-30: per-character dedicated sun shadow slots (nearest-3)
void WickedCall_SetMeshAlphaRef(sMesh* pMesh, float fAlphaRef);
void WickedCall_SetObjectAlphaRef(sObject* pObject, float fAlphaRef);
float WickedCall_GetObjectAlphaRef(sObject* pObject);
void WickedCall_EnableCameraLight(bool On);
void WickedCall_EnableThumbLight(bool On);
void WickedCall_SetLDSSkinningEnabled(bool enabled);
void WickedCall_DisplayCubes(bool Visible);

void WickedCall_CreateReflectionProbe(float x, float y, float z, char *name, float size);
void WickedCall_DeleteReflectionProbe(char *name);
void WickedCall_MoveReflectionProbe(float x, float y, float z, char *name, float size);

void WickedCall_SetLightShaftParameters(float density, float weight, float decay, float exposure);
void WickedCall_GetLightShaftParameters(float& density, float& weight, float& decay, float& exposure);
void WickedCall_SetRenderTargetMouseFocus(bool focus);

void WickedCall_UpdateWaterHeight(float height);
// per-frame totals for calls made many times a frame, where a profiler range keeps only the first call (GetEngineProbe);
// main thread only. WickedCall_ProbeFrame closes a frame; a probe of -1 is not counted
enum eWickedCallProbe { WICKEDCALL_PROBE_PICK, WICKEDCALL_PROBE_PICK_LAYERS, WICKEDCALL_PROBE_PICK_WICKED, WICKEDCALL_PROBE_PICK_LOOKUP,
	WICKEDCALL_PROBE_DECAL_CREATE, WICKEDCALL_PROBE_DECAL_FADE, WICKEDCALL_PROBE_COUNT };
double WickedCall_ProbeNow(void);
void WickedCall_ProbeAdd(int iProbe, double dMilliseconds);
void WickedCall_ProbeFrame(void);
bool WickedCall_ProbeGet(const char* pName, float* pMilliseconds, float* pCalls);
struct WickedCallProbeScope
{
	int iProbe;
	double dStart;
	WickedCallProbeScope(int iProbeIndex) : iProbe(iProbeIndex), dStart(WickedCall_ProbeNow()) {}
	~WickedCallProbeScope() { WickedCall_ProbeAdd(iProbe, WickedCall_ProbeNow() - dStart); }
};
void WickedCall_UpdateWaterColor(float red, float green, float blue);
void WickedCall_PutWaterRipple(float fX, float fY, float fZ, float fSize);
void WickedCall_UpdateTreeWind(float wind);

void WickedCall_RemoveObjectTextures(sObject* pObject);

void WickedCall_SetExposure(float exposure);

void WickedCall_CreateDecal(sObject* pObject);

uint32_t WickedCall_LoadWiScene(char* filename, bool attached, char* changename, char* changenameto);

void WickedCall_PerformEmitterAction(int iAction, uint32_t emitter_root);
void WickedCall_UpdateEmitters(void);
uint32_t WickedCall_LoadWPE(char* filename);
// GGMAX 3.59: WPE-driven weather (rain/snow). See wickedcalls_part4.cpp.
float    GGWind_GustSizeToWaveSize(float gustSize);   // GGMAX 3.62
void     GGWeather_Set(int mode);
void     GGWeather_Update(float intensityPercent, bool bIndoors);
void     GGWeather_Clear(void);
uint32_t GGWeather_GetRoot(void);
uint32_t WickedCall_CreateEmitter(std::string& name, float posX, float posY, float posZ, uint32_t proot);
void WickedCall_SetShaderParameter(int obj, int parameter, float value);
bool WickedCall_CaptureBackbufferRegionToJPG(int x, int y, int w, int h, const char* outputPath);
// GGMAX 3.103: same, box-downscaled to fit maxOutW x maxOutH. Writes by extension (.jpg/.png). No default
// arguments: this header has no include guard and is included twice in one project (C2572).
bool WickedCall_CaptureBackbufferRegionToJPGScaled(int x, int y, int w, int h, const char* outputPath, int maxOutW, int maxOutH);
// GGMAX 3.111: crop a tightly packed RGBA8 image, force it opaque, optionally box-scale it down to fit
// maxOutW x maxOutH (0 = no scaling), and write it - format from the extension (.jpg/.png/...).
bool WickedCall_SaveRGBA8Region(const unsigned char* rgba, int srcW, int srcH, int x, int y, int w, int h, const char* outputPath, int maxOutW, int maxOutH);

