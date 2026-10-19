//----------------------------------------------------
//--- GAMEGURU - M-GridEdit
//----------------------------------------------------

// moved all the defines into the WICKED DEFINE area () 

// Includes 
#include "stdafx.h"
#include "gameguru.h"
#include "M-WelcomeSystem.h"
#include "M-Widget.h"
#include "GGVR.h"
#include "M-GridEditB.h"
#include "M-RPG.h"
#include "M-Workshop.h"

// OPTICK Performance
#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#include "..\..\GameGuru\Imgui\imnodes.h"
int grideleprof_uniqui_id = 35000;
#define MAXTEXTINPUT 1024
float g_Storyboard_header_height = 150.0f;
char cTmpInput[MAXTEXTINPUT + 1];
int g_Storyboard_First_Level_Node = -1;
int g_Storyboard_Current_Level = -1;
bool g_Storyboard_Starting_New_Level = false;
cstr g_Storyboard_LoaderScreen_Name = "loading";
char g_Storyboard_First_fpm[256];
char g_Storyboard_Current_fpm[256];
char g_Storyboard_Current_lua[256];
char g_Storyboard_Current_Loading_Page[256];
std::vector<std::string> projectbank_list;
std::vector<std::string> projectbank_image;
std::vector<int> projectbank_imageid;
std::vector<int> projectbank_active;
StoryboardStruct Storyboard;
StoryboardStruct checkproject;
StoryboardStruct202 updateproject202;
std::vector< std::pair<ImFont*, std::string>> StoryboardFonts;
// ★ GGMAX 3.38 (DX11 423432b1 "Allow installing/uninstalling of custom fonts"): the stock set,
// held alongside the live list so the Fonts tab can tell a built-in font from an installed one.
std::vector< std::pair<ImFont*, std::string>> DefaultStoryboardFonts;
bool bScreen_Editor_Window = false;
int iScreen_Editor_Node = -1;
int g_iAutoExitScreenEditor = 0;
int iStoryboardExecuteKey = 0;
bool bTriggerSaveAs = false;
bool bTriggerOpenProject = false;
int iDelayTriggerOpenProject = 0;
bool bTriggerSaveAsAfterNewLevel = false;
char SaveProjectAsName[256] = "\0";
char SaveProjectAsError[256] = "\0";
bool bTriggerWhatsNewInStoryboard = false;
bool bAddWhatNewToMenu = false;
bool bOpenProjectsFromWelcome = false;
cstr TriggerLoadGameProject = "";
bool bStoryboardFirstRunSetInitPos = false;
bool bStoryboardInitNodes = false;
bool bJustRederedScreenEditor = false;
int g_iRefreshLibraryFolders = 0;
int g_iRefreshLibraryFoldersAfterDelay = 0;
bool g_bCommonAssetsLoadOnce = true;
char statusbar[512];
#ifdef GGMAXEPIC
// No discounts mentioned in Epic Store listing for now
#else
#define FREETRIALONDISCOUNT
#endif

bool g_bUpdateAppAvailable = false;
bool g_bFreeTrialVersion = false;
int g_iFreeTrialDaysLeft = 0;
bool g_bFreeTrialNowExitsApp = false;

bool g_bAdjustPlaneXZUsingSurfaceXZ = false;
bool g_bResetPlaneAfterXZAdjust = false;
bool g_bHoldGridEntityPosWhenManaged = true;
float g_fHoldGridEntityPosX = 0;
float g_fHoldGridEntityPosY = 0;
float g_fHoldGridEntityPosZ = 0;
float g_fLocalTurnRotationForSmartMode = 0.0f;
int g_iStackToSurfaceMode = 0;
// ★ GGMAX 3.38 (DX11 00bb4197 "Can now disable crash logging to improve performance"): the
// master switch the GG_CRASH_CONTEXT macro tests (wickedcalls.h). 0 = breadcrumbs ON. DX11 also
// exposes it as the setup.cfg knob 'disablecrashlogsystem'; that plumbing lives in Common.cpp and
// is NOT ported yet, so today this is on unless something sets it.
int g_iDisableCrashLogSystem = 0;
int g_iOrientToSurfaceMode = 0;
bool g_bParticleEditorPresent = false;
bool g_bBuildingEditorPresent = false;
DWORD g_dwParticleEditorProcessHandle = NULL;

int g_iIconImageInProperties = 0;
int g_iIconImageInPropertiesLastEntIndex = 0;
cstr g_iconImageInPropertiesLastName_s = "";
bool g_bChangedGameCollectionList = false;

bool g_bUpdateCollectionList = false;
bool g_bSelectedNewObjectToAddToLevel = false;

int g_iSuperTriggerFullGrassReveal = 0;

#include <algorithm>
#include <string>
#include "miniz.h"

int iGenralWindowsFlags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoMove;
bool bBoostIconColors = false;
int iDisplayCircleFrames = 0;

#include "shellapi.h"
#include ".\\..\..\\Guru-WickedMAX\\GPUParticles.h"
using namespace GPUParticles;
#include "GGTerrain/GGTerrain.h"
using namespace GGTerrain;
#include "GGTerrain/GGTrees.h"
using namespace GGTrees;
#include "GGTerrain/GGGrass.h"
using namespace GGGrass;

#include ".\..\..\Guru-WickedMAX\wickedcalls.h"
#include "..\..\Guru-WickedMAX\master.h"
#define USE_ENTITY_TOOL_WINDOW
#ifdef DISPLAYBOUNDINGBOXIN_PROPERTIES
#define XMSTATICCOLOR XMFLOAT4(1.0f, 0.1f, 0.0f, 0.4f)
#define XMDYNAMICCOLOR XMFLOAT4(0.25f, 1.0f, 0.25f, 0.4f)
#else
#define XMSTATICCOLOR XMFLOAT4(1.0f, 0.25f, 0.0f, 0.0f)
#define XMDYNAMICCOLOR XMFLOAT4(0.25f, 1.0f, 0.25f, 0.0f)
#endif

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

extern sObject* g_selected_editor_object;
extern int g_selected_editor_objectID;
extern XMFLOAT4 g_selected_editor_color;
extern float g_fSpecialDragInYAdjustment;

int iEditorGridSizeX = 100;
int iEditorGridSizeZ = 100;
bool bRenderTabTab = false;
bool bRenderNextFrame = false;
bool bNeedImGuiInput = false;
bool bProfilerEnable = false;
int iExtractMode = 0; //0 = find floor, 1 = extracted y value. , 3 = fixed y value.
float fExtractYValue = 0, fExtractFixedYValue = GGORIGIN_Y;
bool bExtractFixPivot = true;
bool g_bStandaloneSinglePlayer = true;
bool g_bStandaloneMultiPlayer = false;
bool g_bStandaloneVRMode = false;
bool g_bPreviewLighting = false;
int iLastOpenHeader = 1;
int iExecuteCTRLkey = 0;
int iExecuteALTkey = 0;
int iIncludeLeftIconSet = 0;
extern uint64_t g_hovered_dot_entity;
extern sObject* g_hovered_dot_pobject;
sObject* g_destination_dot_pobject = NULL;
sObject* g_source_dot_pobject = NULL;
sObject* g_selected_middle_dot_pobject = NULL;
bool bDotMiddleWindow = false;
ImVec2 vDotMiddleWindowPos;
#define MAXDOTMIDDLE 1000
#define DOTOBJECTIDADD 40000
#define DOTCURSOROBJECTID (70001+40000+20000)
#define DOTMIDDLEOBJECTID (70001+40000+20001)
#define MAXDOTARCSOBJECTS 20000
#define DOTARCSOBJECTID (70001+40000+20001+MAXDOTMIDDLE+1000000)
#define RELATIONOBJECTID (70001+40000+20001+MAXDOTMIDDLE)
#define RELATIONOBJECTMAX 1000
int iLargestDotObjectID = 70001;
int iLargestDotCount = 999999;
bool bDotObjectDragging = false;
int iDotMiddleInfoSource[MAXDOTMIDDLE];
int iDotMiddleInfoDestination[MAXDOTMIDDLE];
int iDotMiddleColor[MAXDOTMIDDLE];
int iDotArceColor[MAXDOTARCSOBJECTS];

int iDotMiddleInfoSourceType[MAXDOTMIDDLE];
int iDotMiddleInfoDestinationType[MAXDOTMIDDLE];
extern float fLastHitPosition[4];
int iCursorDotObject = 0;
bool bFactionWindow[16];
float fDrawDotCircleTimer = 0;
bool bDrawDotCircle = false;
float fDrawDotCircleRadius = 0.0;
int fDrawDotCircleFrom = 0;
bool g_bDotsAreVisible = false;

#define BACKBUFFERIMAGE (g.perentitypromptimageoffset+9000)
int BackBufferObjectID = 0;
bool BackBufferSnapShotMode = false;
bool BackBufferGrabGameScreen = false;
bool BackBufferParticlesMode = false;
int iBackBufferParticlesTrigger = 0;
int BackBufferParticleEmitter = -1;
bool bFullScreenBackbuffer = false;
bool bSnapShotModeUseCamera = false;
bool bSnapShotModeUse2D = false;
float fSnapShotModeCameraX = 0.0f, fSnapShotModeCameraY = 0.0f, fSnapShotModeCameraZ = 0.0f;
float fSnapShotModeCameraAngX = 0.0f, fSnapShotModeCameraAngY = 0.0f, fSnapShotModeCameraAngZ = 0.0f;
cstr g_LastGroupSaved_s;
bool g_bBehaviorEditorActive = false;
int bStopBackbufferGrab = 0;

bool library_createbehavior = false;
char library_newbehaviorname[256];

bool BackBufferIsGroup = false;
int BackBufferEntityID = 0;
int BackBufferImageID = 0;
int BackBufferSizeX = 0;
int BackBufferSizeY = 0;
float BackBufferRotateY = 0.0f, RestoreBackBufferRotateY = -1.0f;
float BackBufferRotateX = 0.0f, RestoreBackBufferRotateX = -1.0f;
float BackBufferRotateZ = 0.0f, RestoreBackBufferRotateZ = -1.0f;
float BackBufferZoom = 0.0f, RestoreBackBufferZoom = -1.0f;
float BackBufferCamMove = 0.0f;
float BackBufferCamLeft = 0.0f, RestoreBackBufferCamLeft = -1.0f;
float BackBufferCamUp = 0.0f, RestoreBackBufferCamUp = -1.0f;
bool bBackBufferAnimated = false;
bool bBackBufferRestoreCamera = false;
bool bEditorInFreeFlightMode = false;

bool bLoopBackBuffer = false;
bool bLoopFullFPS = false;
bool bRotateBackBuffer = false;
cstr BackBufferCacheName = "";
cstr ProjectCacheName = "";
cstr BackBufferSaveCacheName = "";
extern std::vector<sImageList> g_imageList;
static std::vector<sImageList> g_TempimageList;
int iRestoreEntidMaster = -1;
int fpe_current_loaded_script = -1;
int fpe_current_loaded_script_image = 0;
int fpe_current_loaded_script_image_count = 0;

bool bReadyToDropEntity = false;
bool bWaitOnMouseRelease = false;
bool bDraggingActive = false;
bool bDraggingActiveInitial = false;
int iDragDropActive = 0;
#define HITPOINTYSTARTPOS GGORIGIN_Y
float fHitPointX = 0.0f, fHitPointY = 0.0f, fHitPointZ = 0.0f;
float fHitOffsetX = 0.0f, fHitOffsetY = 0.0f, fHitOffsetZ = 0.0f;
float fHitRayFrom = 0.0f ,fLastHitY = 0.0f;

extern sObject* g_hovered_pobject;
bool bTriggerVisibleWidget = false;
bool bMouseInputSystemUsed = false;
int iLastHitObjectID = 0;
int iStartMouseX, iStartMouseY;
int iObjectMoveMode = 2; // default to move and find floor mode (smartest = Lees Sneaky Solution)
int iObjectMoveModeDropSystem = 0;
int iObjectMoveModeDropSystemUsing = 0;
bool bObjectAllowOverlapping = 1;

float fDebug = 0.0f, fDebug1 = 0.0f, fDebug2 = 0.0f, fDebug3 = 0.0f;

int i_switch_group_tab = 0;
int current_selected_group = -1;
int thumb_selected_group = -1;
bool group_editing_on = false;
bool bCreateNewGroupOnNextDrop = false;
int iLastEntityOnCursor = 0;
float fLastRubberBandX1 = 0.0f;
float fLastRubberBandX2 = 0.0f;
float fLastRubberBandY1 = 0.0f;
float fLastRubberBandY2 = 0.0f;
bool bDetectTerrainOnly = false;
bool bRubberBandCreated = false;
bool bDragCameraActive = false;
bool g_bThumbBankCopyMode = true;
bool g_bRefreshRotationValuesFromObjectOnce = false;
bool g_bRefreshScaleValuesFromObjectOnce = false;
bool g_bLightProbeScaleChanged = false;
bool g_bLightProbeInstantChange = false;
int g_iLightProbeInstantChangeCoolDown = 0;
int iReusePickObjectID = -1;
int iReusePickEntityID = -1;
float fReusePickHitX = 0, fReusePickHitY = 0, fReusePickHitZ = 0;
sObject* pReusePickObject = 0;
std::vector<sLibraryList> g_LibraryFileList;
cStr cLastProjectList = "";


cstr cCurrentBackDropImageFile = "None";
bool bUseBackDropImage = true;
cstr cUseBackbufferCubemap = "";
bool bBackbufferCubemapActive = false;
int iLastSelectedEntityGroup = -1;
int iLastSelectedEntity = 0;
int iSetSettingsFocusTab = 0;
bool bStoryboardWindow = false;
bool bStoryboardWindowOpenLoad = false;
bool bMarketplace_Window = false;
bool bTriggerCloseEntityWindow = false;
bool bMarketplace_Init = false;
bool bFreeTrial_Window = false;
bool bFreeTrial_Init = false;
cstr sDefaultImportPath = "";
bool bResetObjectLibrarySize = false;
bool bWelcomeScreen_Window = false;
bool bWelcomeNoBackButton = false;
bool bWelcomeScreen_Init = false;
std::map<std::string, int> selected_library_fpe;
bool bProceduralLevel = false;
bool bProceduralLevelFromStoryboard = false;
// GGMAX 2.59: set by the Terrain Generator entry recipes BEFORE the flat-level load;
// the terrain bridge suppresses chunk generation while it is true (that whole ring gets
// wiped by the 2.54 entry restart anyway) and clears it at the generator handover, the
// launch resets below, or its own ~30s auto-heal. Owned lifecycle - do NOT reuse
// bProceduralLevelFromStoryboard for this, its resets are not proven on every route.
bool g_ggTerrainGenEntryPending = false;
int iBlackoutForFrames = 0;
int iBlockRenderingForFrames = 0;
int iQuitProceduralLevel = false;
bool bProceduralLevelStartup = false;
int g_iUniqueGroupID = 1000;
cstr sGotoPreviewWithFile = "";
int iGotoPreviewType = 0;
int init_Left_Categories_Column_Width = 3;
int g_iDevToolsOpen = 0;
bool bInvulnerableMode = false;
bool bStartInvulnerableMode = false;
bool bNoSecondAsk = false;
int iWelcomeHeaderType = 0;
int iAboutLogoType = 0;
int active_tools_obj = 0;
int active_tools_entity_index = 0;
int g_iUseLODObjects = 1;
bool bDisableLODLoad = false;
int g_iDisableTerrainSystem = 0;
int g_iDisableJustGrassSystem = 0;
int g_iDisableWParticleSystem = 0;
int g_iEnablePIXMarkers = 0;
bool bSprayMoveWithMouse = false;

bool bTrashcanIconActive = false, bTrashcanIconActive2 = false;
int current_sort_order = 0;
int iWidgetSelection = 0;
bool bRotScaleAlreadyUpdated = false;
int old_iMSAASampleCount = -1;
int old_iFSRMode = -1;
int old_iMSAO = -1;
float old_fMSAOPower = -1.0;
int old_iShadowSpotCascadeResolution = -1;
int old_iShadowSpotResolution = -1;
int old_iShadowPointResolution = -1;
bool bForceRefreshLightCount = false;
int iUpdateOcean = 0;
bool bEditorLight = false;
cStr sNextLevelToLoad;

float fMouseWheelZoomFactor = 3.0;
bool g_bResetCameraToFreeFlightOnNewLevel = false;
float fLocalMax = 1000.0f;

// Defines
#define ENABLETUTORIALVIDEOS

// 
//  GAMEGURU MAP EDITOR EXECUTABLE CODE
// 

//Check if we are in f9 mode
extern bool g_occluderf9Mode;

// extern to global that toggles when load map removed from entities
extern bool g_bBlackListRemovedSomeEntities;
extern bool gbWelcomeSystemActive;
extern int g_iWelcomeLoopPage;
extern int g_trialStampDaysLeft;
int g_tstoreprojectmodifiedstatic = 0;

extern bool g_bCharacterCreatorPlusActivated;
// can prevent app from quitting out while in test game
extern bool g_bDisableQuitFlag;
extern bool bEnableWeather;
char cImGuiDebug[2048] = "\0";
bool bForceKey = false;
int iForceScancode = -1;
cstr csForceKey = "";
bool bForceKey2 = false;
cstr csForceKey2 = "";
bool bForceUndo = false;
bool bForceRedo = false;
int iLaunchAfterSync = 0;
bool bTriggerFovUpdate = false;
bool bKeepWindowsResponding = false;
int iLaunchAfterSyncAction = 0;
bool bLaunchTestGameAfterLoad = false;
bool bLaunchSaveStandalonefterLoad = false;
bool bCloseStoryboardAfterLoad = false;
int iLevelEditorFromStoryboardID = -1;
char pLaunchAfterSyncPreSelectModel[MAX_PATH] = "\0";
char pLaunchAfterSyncLastImportedModel[MAX_PATH] = "\0";
int iOldLaunchAfterSync = 0;
int iSkibFramesBeforeLaunch = 0;
DWORD gWindowSizeXOld = 0;
DWORD gWindowSizeYOld = 0;
DWORD gWindowSizeAddY = 0;
DWORD gWindowSizeAddX = 0;
DWORD gWindowVisibleOld = 0;
DWORD gWindowPosXOld = 0;
DWORD gWindowPosYOld = 0;
DWORD gWindowMaximized = 0;
int xmouseold = 0, ymouseold = 0;

extern bool bImGuiInTestGame;
extern bool bBlockImGuiUntilNewFrame;
extern bool bImGuiRenderWithNoCustomTextures;
extern bool bImGuiFrameState;
extern bool bImGuiReadyToRender;
extern bool bImGuiInitDone;
extern ImVec2 OldrenderTargetSize;
extern ImVec2 OldrenderTargetPos;
extern ImVec2 renderTargetAreaPos;
extern ImVec2 renderTargetAreaSize;
extern bool bImGuiRenderTargetFocus;
extern bool bImGuiGotFocus;
extern bool g_bCascadeQuitFlag;
extern int ImGuiStatusBar_Size;
extern char defaultWriteFolder[260];
bool bEntityGotFocus = false;
char cDirectOpen[260];
bool imgui_is_running = false;
int refresh_gui_docking = 0;
ImGuiID dock_main_tabs, dock_tools_windows;
cstr RedockNextWindow;
ImGuiViewport* viewport;
int toolbar_size;
bool g_bInTutorialMode = false;
int g_iCountdownToAlphaBetaMessage = 0;
ImVec4 drawCol_toogle;
int g_EntityClipboardAnchorEntityIndex = -1;
std::vector<int> g_EntityClipboard;

extern preferences pref;
extern cFolderItem MainEntityList;

bool bExport_Standalone_Window = false;
bool bExport_SaveToGameCloud_Window = false;
bool bExternal_Entities_Window = false;
int iDisplayLibraryType = 0;
int iDisplayLibrarySubType = 0;
int iLastDisplayLibraryType = -1;
cstr sStartLibrarySearchString = "";
cstr sTriggerCategorySelect = "";
int iLibraryStingReturnToID = 0;
int iSelectedLibraryStingReturnID = -1;
cstr sMakeDefaultSelecting = "";
cstr sSelectedLibrarySting = "";
bool bSelectLibraryViewAll = false;
bool bExternal_Entities_Init = false;
bool bEntity_Properties_Window = false;
bool bProperties_Window_Block_Mouse = false;
bool bCheckForClosing = false;
bool bCheckForClosingForce = false;
bool bBuilder_Properties_Window = false;
bool bBuilder_Left_Window = false;
bool bTerrain_Tools_Window = false;
bool bWaypoint_Window = false;
bool bDownloadStore_Window = false;
bool bImporter_Window = false;
bool bHelpVideo_Window = false;
bool bHelp_Window = false;
extern char cForceTutorialName[1024];
bool bHelp_Menu_Image_Window = false;
bool bAbout_Window = false;
bool bCredits_Window = false;
bool bBug_Reporting_Window = false;
bool bBug_RefreshBugList = false;
bool bAbout_Window_First_Run = false;
bool bCredits_Window_First_Run = true;
bool bAbout_Init = false;
bool Entity_Tools_Window = true;
bool bInfo_Window = false;
bool bInfo_Reload = false;
bool bInfo_Window_First_Run = true;
cstr cInfoMessage = "";
cstr cInfoImage = "", cInfoImageLast = "";
int iInfoUniqueId = 0;
extern int g_iActiveMonitors;

bool Visuals_Tools_Window = false;
bool Weather_Tools_Window = false;
bool Game_Settings_Window = false;
bool Logic_Settings_Window = false;
int iRestoreLastWindow = 0;
std::vector<sRubberBandType> vEntityLockedList;
#define MAXGROUPSLISTS 100
cstr sEntityGroupListName[MAXGROUPSLISTS];
std::vector<sRubberBandType> vEntityGroupList[MAXGROUPSLISTS];
int iEntityGroupListImage[MAXGROUPSLISTS];
bool bPreferences_Window = false;
char cPreferencesMessage[MAX_PATH] = { "\x0" };
bool Shooter_Tools_Window = false; // Shooter_Tools_Window not really a window now, just a filter mode for Object Tools
bool Puzzle_Tools_Window = false; //Not yet active. only for toggle state.
bool RPG_Tools_Window = false; //Not yet active. only for toggle state.
char cNextWindowFocus[256];
bool bEditGameSettings = false;
int media_icon_size_leftpanel = 64;
int iColumnsWidth_leftpanel = 110;
int iColumns_leftpanel = 0;
bool bDisplayText_leftpanel = true;
float fFontSize_leftpanel = 1.0;

cFolderItem::sFolderFiles *pDragDropFile = NULL;
int iOldgridentity = -1;
float fPropertiesColoumWidth = 100.0f;
bool bTriggerMessage = false;
bool bTriggerSmallMessage = false;
int iTriggerMessageDelay = 0;
int iTriggerMessageFrames = 0;
int iTriggerMessageY = 0;
char cTriggerMessage[MAX_PATH] = "\0";
char cSmallTriggerMessage[MAX_PATH] = "\0";
int iMessageTimer = 0;
ImVec4 drawCol_back;
ImVec4 drawCol_normal;
ImVec4 drawCol_hover;
ImVec4 drawCol_Down;
ImVec4 drawCol_black = { 0,0,0,0 };

extern ISpObjectToken * CCP_SelectedToken;
extern LPSTR pCCPVoiceSet;
extern char CCP_SpeakText[1024];
extern wchar_t CCP_SpeakText_w[1024];
extern int CCP_Speak_Rate;

std::vector<cstr> tutorial_list; //unsorted.
std::map<std::string, std::string> tutorial_files;
std::map<std::string, std::string> tutorial_videos;
std::map<std::string, std::string> tutorial_description;
std::vector<cstr> about_text; //unsorted.

bool bTutorial_Init = false;
int current_tutorial = -1;
int selected_tutorial = 0;
bool bVideoPlayerMaximized = false;
bool bSmallVideoPlayerMaximized = false;
bool bLastSmallVideoPlayerMaximized = false;

bool bVideoResumePossible = false;
bool bVideoPerccentStart = false;
int iVideoFindFirstFrame = 0;
int iVideoDelayExecute = 0;
bool bTutorialCheckAction = false;
int bDelayedTutorialCheckAction = -1;
int iDelayedCameraRestore = 0;
char cForceTutorialName[1024] = "\0";
char cTutorialName[TUTORIALMAXTEXT] = "\0";
cstr cVideoDescription = "";
ActiveTutorial tut;
bool bTutorialRendered = false;
bool bSmallVideoFrameStart = true;
bool bSetTutorialSectionLeft = false;

//Tooltip object code.
int iLastTooltipSelection = -1;
int iTooltipTimer = 0;
int iTooltipHoveredTimer = 0;
int iTooltipLastObjectId = 0;
bool iTooltipAlreadyLoaded = true;
bool iTooltipObjectReady = false;
float lastKeyTime = 0;
char cHelpMenuImage[MAX_PATH];
bool bLostFocus = false;
bool bRenderTargetModalMode = false;
int iStartupTime = 0;
cstr CurrentWinTitle = "";
int speech_ids[5];

extern bool bWaypointDrawmode;
extern float custom_back_color[4];
extern bool bUpdateVeg;
extern int iLastUpdateVeg;


float fEmptyLevelFloorY = 0;
bool bEmptyLevelGrid = false;

// moved here so Classic would compile
bool Shooter_Tools_Window_Active = false;
void DeleteWaypointsAddedToCurrentCursor(void);
void Add_Grid_Snap_To_Position(bool bFromWidgetMode);
float ImGuiGetMouseX(void);
float ImGuiGetMouseY(void);
void RotateAndMoveRubberBand(int iActiveObj, float fMovedActiveObjectX, float fMovedActiveObjectY, float fMovedActiveObjectZ, GGQUATERNION quatRotationEvent); //float fMovedActiveObjectRX, float fMovedActiveObjectRY, float fMovedActiveObjectRZ);
void SetStartPositionsForRubberBand(int iActiveObj);
void EmptyMessages(void);

	void HandleObjectDeletion();
	void ControlAdvancedSetting(int&, const char*, bool* = nullptr);
	void TestLevel_ToggleBoundary(bool _2d, bool _3d);
	void TestLevel_ToggleTreeVegWater(bool tree, bool veg, bool water);

void set_inputsys_mclick(int value)
{
	t.inputsys.mclick = value;
}

// GLOBAL to know when in welcome area
int iTriggerWelcomeSystemStuff = 0;
int iCountDownToShowQuickStartDialog = 0;

// Declared here because this unity-build part file has no include path to
// GGTerrainWicked.h. Definition: Guru-WickedMAX/GGTerrain/GGTerrainWicked.cpp.
namespace GGTerrain { bool GGTerrainWicked_IsRevealHeld(); }

void gridedit_triggermessagehandler (bool bForceMessageNoFade)
{
	// Level reveal hold: opaque cover over the 3D render target while the
	// terrain the camera faces finishes building after a level load. Real
	// frames run underneath (legacy height readback + wicked chunk generation);
	// the hold releases from GGTerrainWicked_Update when the visible chunk set
	// exists, or via its internal ~5s deadline.
	if (GGTerrain::GGTerrainWicked_IsRevealHeld())
	{
		ImGuiViewport* holdviewport = ImGui::GetMainViewport();
		if (holdviewport)
		{
			ImDrawList* holddl = ImGui::GetForegroundDrawList(holdviewport);
			if (holddl)
			{
				// Plain black cover, no text — the storyboard click already showed
				// "Loading Level ...". +2px at the bottom: the render target
				// extends slightly below OldrenderTargetSize and leaves a line of
				// scene pixels visible otherwise (verified pixel-level).
				ImVec2 rtMin = ImVec2(OldrenderTargetPos.x, OldrenderTargetPos.y);
				ImVec2 rtMax = ImVec2(OldrenderTargetPos.x + OldrenderTargetSize.x, OldrenderTargetPos.y + OldrenderTargetSize.y + 2.0f);
				holddl->AddRectFilled(rtMin, rtMax, IM_COL32(0, 0, 0, 255));
			}
		}
	}

	if (!bTriggerMessage && bTriggerSmallMessage && iTriggerMessageFrames > 0)
	{
		ImGuiViewport* mainviewport = ImGui::GetMainViewport();
		if (mainviewport)
		{
			ImDrawList* dl = ImGui::GetForegroundDrawList(mainviewport);
			if (dl)
			{
				ImGuiContext& g = *GImGui;
				float fontscale = 1.25;
				ImVec2 textsize = ImGui::CalcTextSize(cSmallTriggerMessage)  * fontscale;
				float vCenterTextX = (OldrenderTargetSize.x * 0.5) - (textsize.x * 0.5);
				ImVec4 background = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
				background.w = 0.7;

				dl->AddRectFilled(ImVec2(OldrenderTargetPos.x + vCenterTextX - 2.0, OldrenderTargetPos.y + 50.0 - 2.0), ImVec2(OldrenderTargetPos.x + vCenterTextX + textsize.x + 2.0, OldrenderTargetPos.y + 50.0 + textsize.y + 2.0), ImGui::GetColorU32(background), 2.0, ImDrawCornerFlags_All);
				dl->AddText(g.Font, g.FontSize * fontscale, ImVec2(OldrenderTargetPos.x + vCenterTextX, OldrenderTargetPos.y + 50.0 ), ImGui::GetColorU32(ImGuiCol_Text), cSmallTriggerMessage);
			}
		}
		iTriggerMessageFrames--;
		if (iTriggerMessageFrames == 0)
		{
			cSmallTriggerMessage[0] = 0;
			bTriggerSmallMessage = false;
		}
	}
	if (bTriggerMessage)
	{
		if (iTriggerMessageDelay > 0)
		{
			iTriggerMessageDelay--;
			return;
		}

		if (iTriggerMessageY == 1)
		{
			if (iMessageTimer == 0 || MAXTimer() - iMessageTimer > 8100)
				iMessageTimer = MAXTimer();
		}
		else
		{
			if (iMessageTimer == 0 || MAXTimer() - iMessageTimer > 4100)
				iMessageTimer = MAXTimer();
		}
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowViewport(viewport->ID);
		if (iTriggerMessageY > 0)
		{
			ImVec2 viewPortPos = ImGui::GetMainViewport()->Pos;
			ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
			ImGui::SetNextWindowPos(viewPortPos + ImVec2(50, 24+iTriggerMessageY), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(viewPortSize.x - 100, 0), ImGuiCond_Always);
		}
		else
		{
			//PE: Now always center on viewport instead of rendertarget. as we now are inside storyboard.
			ImVec2 viewPortPos = ImGui::GetMainViewport()->Pos;
			ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
			ImGui::SetNextWindowPos(viewPortPos + ImVec2(350, 130), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(viewPortSize.x - 700, 0), ImGuiCond_Always);
		}
		bool winopen = true;

		ImVec4* style_colors = ImGui::GetStyle().Colors;
		ImVec4 oldBgColor = style_colors[ImGuiCol_WindowBg];
		ImVec4 oldTextColor = style_colors[ImGuiCol_Text];

		float fader = 1.0f;
		if (bForceMessageNoFade == false)
		{
			if (iTriggerMessageY == 1)
				fader = ((float)MAXTimer() - (float)iMessageTimer) / 1500.0f;
			else
				fader = ((float)MAXTimer() - (float)iMessageTimer) / 1000.0f;

			fader -= 1.0;
			if (fader < 0) {
				fader = 0.0001;
			}
			fader /= 3.0;
			fader = 1.0 - fader;
			if (fader < 0.1)
			{
				bTriggerMessage = false;
				bTriggerSmallMessage = false;
				iTriggerMessageY = 0;
				iMessageTimer = 0;
			}
		}

		style_colors[ImGuiCol_WindowBg].x = 0.0;
		style_colors[ImGuiCol_WindowBg].y = 0.0;
		style_colors[ImGuiCol_WindowBg].z = 0.0;
		if (iTriggerMessageY == 1)
			style_colors[ImGuiCol_WindowBg].w *= (fader*0.85);
		else
			style_colors[ImGuiCol_WindowBg].w *= (fader*0.5);

		style_colors[ImGuiCol_Text].x = 1.0;
		style_colors[ImGuiCol_Text].y = 1.0;
		style_colors[ImGuiCol_Text].z = 1.0;
		style_colors[ImGuiCol_Text].w *= fader;

		ImGui::Begin("##Messageinfo", &winopen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
		ImGui::SetWindowFontScale(2.0);
		ImGui::Text(" ");
		float fTextSize = ImGui::CalcTextSize(cTriggerMessage).x;
		ImGui::SetCursorPos(ImVec2((ImGui::GetWindowSize().x*0.5) - (fTextSize*0.5), ImGui::GetCursorPos().y));

		ImGui::Text(cTriggerMessage);
		ImGui::Text(" ");
		ImGui::SetWindowFontScale(1.0);
		ImGui::End();
		style_colors[ImGuiCol_WindowBg] = oldBgColor;
		style_colors[ImGuiCol_Text] = oldTextColor;
	}
}

void vr_init (void)
{
	// VR Handling
	bool bSkipVRForNow = false;
	if (g.gvrmodefordevelopers == 1)
		bSkipVRForNow = false; // for users who wish to play with half-baked VR 
	else
		bSkipVRForNow = true; // saves 1750ms from init below!!!

	// No VR or RIFTMODE in Editor Mode
	g.globals.riftmode = 0;
	g.vrglobals.GGVREnabled = 0;
	if (bSkipVRForNow == true)
	{
		timestampactivity(0, "VR System disabled to improve launch speed (temporary)");
		g.vrglobals.GGVRUsingVRSystem = 1;
	}
	else
	{
		g.vrglobals.GGVRUsingVRSystem = 1;
		if (g.gvrmode == 2) g.vrglobals.GGVREnabled = 1; // OpenVR (Steam)
		if (g.gvrmode == 3) g.vrglobals.GGVREnabled = 2; // Windows Mixed Reality (Microsoft)
		char pVRSystemString[1024];
		sprintf(pVRSystemString, "choose VR system with mode %d", g.vrglobals.GGVREnabled);
		timestampactivity(0, pVRSystemString);
		int iErrorCode = GGVR_ChooseVRSystem(g.vrglobals.GGVREnabled, g.gproducelogfiles, "");// cstr(g.fpscrootdir_s + "\\GGWMR.dll").Get() );
		if (iErrorCode > 0)
		{
			// if VR headset is not present, switch VR off to speed up non-VR rendering (especially for debug)
			char pErrorStr[1024];
			sprintf(pErrorStr, "Error Choosing VR System : Code %d", iErrorCode);
			timestampactivity(0, pErrorStr);
			timestampactivity(0, "switching VR off, headset not detected");
			g.vrglobals.GGVREnabled = 0;
		}
		else
		{
			//PE: Only if we use vr.
			if (g.gvrmode > 0)
			{
				// Give portal enough time to start its launch, then get rid of GameWindow until we need it!
				Sleep(10);
				CloseWindow(g_pGlob->hOriginalhWnd);
				Sleep(10);
				g_pGlob->hOriginalhWnd = NULL;
				SetWindowPos(g_pGlob->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
		}
	}
}

// mapeditor inits and loop call
int gguishadereffectindex = 0;
void mapeditorexecutable_init ( void )
{
	//  Means we are in the editor (1) or in standalone game (0)
	timestampactivity(0,"ide input mode");
	g.globals.ideinputmode = 1;

	// VR Support
	vr_init();

	//  Set device to get multisampling AA active in editor
	t.multisamplingfactor=0;
	t.multimonitormode=0;

	//  Init app
	timestampactivity(0,"sync states");
	SyncOn (   ); SyncRate (  0 );
	t.strwork = "" ; t.strwork = t.strwork + t.strarr_s[475]+" - [Editor]";
	timestampactivity(0,"window states");
	ShowWindow ( ); WindowToFront (  t.strwork.Get() );
	AlwaysActiveOff ( );

	// start thread loader for for generic (startup) files (multi-threaded loading)
	generic_preloadfiles();

	// moved auth check to void Master::Update(float dt) - as early as possible!

	// So entirely replace fixed function rendering, use this shader effect
	g.guishadereffectindex = loadinternaleffect("effectbank\\reloaded\\gui_basic.fx");
	gguishadereffectindex = g.guishadereffectindex;
	g.guidiffuseshadereffectindex = loadinternaleffect("effectbank\\reloaded\\gui_diffuse.fx");
	g.guiwireframeshadereffectindex = loadinternaleffect("effectbank\\reloaded\\gui_wireframe.fx");
	g.guidepthshadereffectindex = loadinternaleffect("effectbank\\reloaded\\gui_showdepth.fx");

	//  Camera aspect ratio adjustment for desktop resolution
	timestampactivity(0,"camera states");
	t.aspect_f=GetDesktopWidth() ; t.aspect_f=t.aspect_f/GetDesktopHeight();
	SetCameraAspect ( t.aspect_f );

	// 111115 - base start memory for GameGuru (overwritten if g.grestoreeditorsettings==0)
	timestampactivity(0,"memory states");
	g.gamememactuallyusedstart=SMEMAvailable(1);

	// Reset texture/profile in EBE folder
	ebe_restoreebedefaulttextures();

	//  Early editor only inits
	timestampactivity(0,"pre widget init state");
	t.tsplashstatusprogress_s="WIDGET INIT";
	timestampactivity(0,t.tsplashstatusprogress_s.Get());
	timestampactivity(0,"widget status update");
	version_splashtext_statusupdate ( );
	widget_init ( );

	t.tsplashstatusprogress_s="SLIDERS INIT";
	timestampactivity(0,t.tsplashstatusprogress_s.Get());
	version_splashtext_statusupdate ( );
	sliders_init ( );

	// Generic asset loading common to editor and game
	t.tresetforstartofeditor=1;
	t.tsplashstatusprogress_s="LOAD COMMON ASSETS";
	timestampactivity(0,t.tsplashstatusprogress_s.Get());
	version_splashtext_statusupdate ( );
	common_loadcommonassets ( 0 );

	//  Initialise meshes and editor resources
	t.tsplashstatusprogress_s="INIT EDITOR RESOURCES";
	timestampactivity(0,t.tsplashstatusprogress_s.Get());
	version_splashtext_statusupdate ( );
	t.lastgrideditselect=-1;
	g.gmapeditmode = 1;
	editor_init ( );

	//  Load resource file which has test game memory usage data contained
	t.tsplashstatusprogress_s="LOAD MAIN RESOURCES";
	timestampactivity(0,t.tsplashstatusprogress_s.Get());
	version_splashtext_statusupdate ( );
	loadresource();

	//  Call visuals loop once to set shader constants
	t.tsplashstatusprogress_s="UPDATE VISUAL SETTINGS";
	timestampactivity(0,t.tsplashstatusprogress_s.Get());
	version_splashtext_statusupdate ( );
	t.visuals=t.editorvisuals;
	t.visuals.refreshshaders=1;
	visuals_loop ( );
	bool bUpdateEngineToo = false;
	extern void visuals_shaderlevels_update_core (bool);
	visuals_shaderlevels_update_core (bUpdateEngineToo);
	//visuals_shaderlevels_update ( );

	//PE: FOV has changed here if on widescreen that adjust fov depending on aspect ratio.
	//PE: We must refesh the windows to account for the new fov.
	float gpw = master.masterrenderer.GetPhysicalWidth();
	float gph = master.masterrenderer.GetPhysicalHeight();
	if (((float)gpw / (float)gph) > 2.1 && gpw > 1920)
	{
		bTriggerFovUpdate = true; //PE: Set FOV.
	}

	// Load map editior settings
	t.bTriggerNewMapAtStart = true;
	t.bIgnoreFirstCallToNewLevel = false;
	if ( g.grestoreeditorsettings == 1 ) 
	{
		t.tsplashstatusprogress_s="RESTORE LAST PROJECT";
		timestampactivity(0,t.tsplashstatusprogress_s.Get());
		version_splashtext_statusupdate ( );
		//popup_text_close();
		t.tfile_s = g.mysystem.editorsGridedit_s+"cfg.cfg";//"editors\\gridedit\\cfg.cfg";
		if ( FileExist(t.tfile_s.Get()) == 1 ) 
		{
			//  Load last Editor CFG Settings
			t.skipfpmloading=1;
			editor_loadcfg ( );

			//  load project specified in CFG (worklevel.fpm?)
			mapfile_loadproject_fpm ( );

			//  Now wipe config (in case we fail to load in restart, we avoid an infinite loop)
			if (  FileExist(t.tfile_s.Get()) == 1  ) DeleteAFile ( t.tfile_s.Get() );

			//  load in current files in LEVELBANK\TESTMAP (not from FPM)
			gridedit_load_map ( );
			t.terrain.grassregionx1 = t.terrain.grassregionx2;
			extern int g_iSuperTriggerFullGrassReveal; // hmm, shoved in to get the damn grass showing on initial load!
			g_iSuperTriggerFullGrassReveal = 10;
			t.skipfpmloading=0;
		}
	}
	else
	{
		// Start Splash (only one which does not wait for Sync ( -as interface not avail. in debug) )
		t.tsplashstatusprogress_s="";
		timestampactivity(0,t.tsplashstatusprogress_s.Get());
		version_splashtext_statusupdate ( );
	}

	//  trigger zoom to aquire camera range for editor
	t.updatezoom=1;

	//  Start resource bar must accurately reflect ALL data loaded by editor
	if ( g.grestoreeditorsettings==0 )
	{
		t.gamememactuallyusedstarttriggercount = 5; // 111115 - one trigger to get STARTMEM at beginning of GameGuru session execution (honest value)
	}

	//  Var to control machine independent speed
	game_timeelapsed_init ( );
	t.tsl_f= MAXTimer();

	// IDE announcement system (note VR Quest has this option)
	// (note VR Quest has this option)
	iTriggerWelcomeSystemStuff = 1;
	if (g.gshowannouncements == 1)
	{
		bTriggerWhatsNewInStoryboard = true;
	}
	// only show front dialogs if not resuming from previous session
	int iCountDownToShowQuickStartDialog = 0;
	if ( g.grestoreeditorsettings == 0 ) 
	{
		// Welcome quick start page
		g.quickstartmenumode = 0;
		if ( g.iFreeVersionModeActive != 0 )
		{
			editor_showquickstart ( 0 );
			welcome_free();
		}
		else
		{
			if (g.gshowonstartup == 1 || g.iTriggerSoftwareToQuit != 0) 
			{
				editor_showquickstart(0);
				welcome_free(); //PE: We must always close it for level auto load to work.
			}
			else
			{
				welcome_free();
			}
		}
	}
	else
	{
		// always need to close down loading splash
		welcome_free();
	}

	// After 5 minutes, trigger another trial reminder
	DWORD dwStartOfEditingSession = timeGetTime() + (1000*60*5);
	DWORD dwSecondReminder = 0;

	//Load needed images.
	image_preload_files_reset(); //PE: At this point we have no more thread loaded images to use.
	SetMipmapNum(1); //PE: mipmaps not needed.
	image_setlegacyimageloading(true);

	LoadImage("editors\\uiv3\\shape.png", TOOL_SHAPE);
	LoadImage("editors\\uiv3\\level.png", TOOL_LEVELMODE);
	LoadImage("editors\\uiv3\\storedlevel.png", TOOL_STOREDLEVEL);
	LoadImage("editors\\uiv3\\blendmode.png", TOOL_BLENDMODE);
	LoadImage("editors\\uiv3\\rampmode.png", TOOL_RAMPMODE);
	LoadImage("editors\\uiv3\\painttexture.png", TOOL_PAINTTEXTURE);
	LoadImage("editors\\uiv3\\paintgrass.png", TOOL_PAINTGRASS);
	LoadImage("editors\\uiv3\\entity.png", TOOL_ENTITY);
	LoadImage("editors\\uiv3\\markers.png", TOOL_MARKERS);
	LoadImage("editors\\uiv3\\waypoints.png", TOOL_WAYPOINTS);
	LoadImage("editors\\uiv3\\newwaypoints.png", TOOL_NEWWAYPOINTS);
	LoadImage("editors\\uiv3\\toolbar\\position.png", TOOLBAR_POSITION);
	LoadImage("editors\\uiv3\\toolbar\\scale.png", TOOLBAR_SCALE);
	LoadImage("editors\\uiv3\\toolbar\\rotate.png", TOOLBAR_ROTATE);
	LoadImage("editors\\uiv3\\toolbar\\grid.png", TOOLBAR_GRID);
	LoadImage("editors\\uiv3\\toolbar\\gridsettings.png", TOOLBAR_GRIDSETTINGS);	
	LoadImage("editors\\uiv3\\toolbar\\snap.png", TOOLBAR_SNAP);

	LoadImage("editors\\uiv3\\toolbar\\surface.png", TOOLBAR_SURFACE);
	LoadImage("editors\\uiv3\\toolbar\\vert.png", TOOLBAR_VERT);
	LoadImage("editors\\uiv3\\toolbar\\horizontal.png", TOOLBAR_HORI);

	if (FileExist("editors\\uiv3\\playbut-icon.png"))
	{
		LoadImage("editors\\uiv3\\playbut-icon.png", TOOL_TESTGAME);
	}
	else if (FileExist("editors\\uiv3\\play-icon.png"))
	{
		LoadImage("editors\\uiv3\\play-icon.png", TOOL_TESTGAME);
	}
	else
		LoadImage("editors\\uiv3\\testgame.png", TOOL_TESTGAME);
	LoadImage("editors\\uiv3\\vrmode.png", TOOL_VRMODE);
	LoadImage("editors\\uiv3\\savestandalone.png", TOOL_SOCIALVR);
	LoadImage("editors\\uiv3\\newlevel.png", TOOL_NEWLEVEL);
	LoadImage("editors\\uiv3\\loadlevel.png", TOOL_LOADLEVEL);
	LoadImage("editors\\uiv3\\savelevel.png", TOOL_SAVELEVEL);
	LoadImage("editors\\uiv3\\rounding_overlay_style0-h.png", ROUNDING_OVERLAY);
	LoadImage("editors\\uiv3\\ebe-block.png", EBE_BLOCK);
	LoadImage("editors\\uiv3\\ebe-column.png", EBE_COLUMN);
	LoadImage("editors\\uiv3\\ebe-cube.png", EBE_CUBE);
	LoadImage("editors\\uiv3\\ebe-floor.png", EBE_FLOOR);
	LoadImage("editors\\uiv3\\ebe-new.png", EBE_NEW);
	LoadImage("editors\\uiv3\\ebe-row.png", EBE_ROW);
	LoadImage("editors\\uiv3\\ebe-stairs.png", EBE_STAIRS);
	LoadImage("editors\\uiv3\\ebe-wall.png", EBE_WALL);
	LoadImage("editors\\uiv3\\builder.png", TOOL_BUILDER);
	LoadImage("editors\\uiv3\\ccp.png", TOOL_CCP);
	LoadImage("editors\\uiv3\\import.png", TOOL_IMPORT);
	LoadImage("editors\\uiv3\\media-play.png", MEDIA_PLAY);
	LoadImage("editors\\uiv3\\media-pause.png", MEDIA_PAUSE);
	LoadImage("editors\\uiv3\\media-refresh.png", MEDIA_REFRESH);
	LoadImage("editors\\uiv3\\media-record.png", MEDIA_RECORD);
	LoadImage("editors\\uiv3\\media-recording.png", MEDIA_RECORDING);
	LoadImage("editors\\uiv3\\media-recordprocessing.png", MEDIA_RECORDPROCESSING);
	LoadImage("editors\\uiv3\\pointer2.png", TUTORIAL_POINTER);
	LoadImage("editors\\uiv3\\pointer3.png", TUTORIAL_POINTERUP);	

	LoadImage("editors\\uiv3\\gameguru-max-logo.png", ABOUT_LOGO);
	iAboutLogoType = 0;
	LoadImage("editors\\uiv3\\ABOUT-TGC.png", ABOUT_TGC);
	LoadImage("editors\\uiv3\\ABOUT-Country.png", ABOUT_HB);
	LoadImage("editors\\uiv3\\ebe-control1.png", EBE_CONTROL1);
	LoadImage("editors\\uiv3\\ebe-control2.png", EBE_CONTROL2);
	LoadImage("editors\\uiv3\\shape-up.png", TOOL_SHAPE_UP);
	LoadImage("editors\\uiv3\\shape-down.png", TOOL_SHAPE_DOWN);
	LoadImage("editors\\uiv3\\drawwaypoints.png", TOOL_DRAWWAYPOINTS);
	LoadImage("editors\\uiv3\\dotcircle.png", TOOL_DOTCIRCLE);
	LoadImage("editors\\uiv3\\dotcircles.png", TOOL_DOTCIRCLE_S);
	LoadImage("editors\\uiv3\\dotcirclem.png", TOOL_DOTCIRCLE_M);

	LoadImage("editors\\uiv3\\ent-properties.png", TOOL_ENT_EDIT);
	LoadImage("editors\\uiv3\\ent-extract.png", TOOL_ENT_EXTRACT);
	LoadImage("editors\\uiv3\\ent-duplicate.png", TOOL_ENT_DUPLICATE);
	LoadImage("editors\\uiv3\\ent-lock.png", TOOL_ENT_LOCK);
	LoadImage("editors\\uiv3\\ent-findfloor.png", TOOL_ENT_FINDFLOOR);
	LoadImage("editors\\uiv3\\ent-delete.png", TOOL_ENT_DELETE);
	LoadImage("editors\\uiv3\\ent-search.png", TOOL_ENT_SEARCH);

	LoadImage("editors\\uiv3\\circle.png", TOOL_CIRCLE);
	LoadImage("editors\\uiv3\\circles.png", TOOL_CIRCLE_S);
	LoadImage("editors\\uiv3\\circlem.png", TOOL_CIRCLE_M);

	LoadImage("editors\\uiv3\\environment.png", TOOL_VISUALS);
	LoadImage("editors\\uiv3\\camera.png", TOOL_CAMERA);
	LoadImage("editors\\uiv3\\light.png", TOOL_CAMERALIGHT);
	LoadImage("editors\\uiv3\\goback.png", TOOL_GOBACK);
	LoadImage("editors\\uiv3\\goexit.png", TOOL_GOEXIT);
	LoadImage("editors\\uiv3\\media-maximize.png", MEDIA_MAXIMIZE);
	LoadImage("editors\\uiv3\\media-minimize.png", MEDIA_MINIMIZE);

	LoadImage("editors\\uiv3\\weather-sun.png", ENV_SUN);
	LoadImage("editors\\uiv3\\weather-rain.png", ENV_RAIN);
	LoadImage("editors\\uiv3\\weather-snow.png", ENV_SNOW);
	LoadImage("editors\\uiv3\\weather.png", ENV_WEATHER);
	LoadImage("editors\\uiv3\\tool-gamesettings.png", TOOL_GAME_SETTINGS);

	LoadImage("editors\\uiv3\\logic.png", TOOL_LOGIC);// shooter.png", TOOL_SHOOTER);
	
	LoadImage("entitybank\\_markers\\Trigger Zone.bmp", TOOL_TRIGGERZONE);
	LoadImage("entitybank\\_markers\\flag.bmp", TOOL_FLAG);

	LoadImage("editors\\uiv3\\pencil-small.png", TOOL_PENCIL);

	LoadImage("editors\\uiv3\\logichighlight.png", UI3D_DOTOBJECTS);	
	LoadImage("editors\\uiv3\\brain_logic_marker.dds", UI3D_DOTMIDDLEOBJECTS);

	LoadImage("editors\\uiv3\\shape-circle.png", SHAPE_CIRCLE);
	LoadImage("editors\\uiv3\\shape-square.png", SHAPE_SQUARE);


	LoadImage("editors\\uiv3\\key-alt.png", KEY_ALT);
	LoadImage("editors\\uiv3\\key-backspace.png", KEY_BACKSPACE);
	LoadImage("editors\\uiv3\\keyboard.png", KEY_KEYBOARD);
	LoadImage("editors\\uiv3\\key-control.png", KEY_CONTROL);
	LoadImage("editors\\uiv3\\key-minus.png", KEY_MINUS);
	LoadImage("editors\\uiv3\\key-plus.png", KEY_PLUS);
	LoadImage("editors\\uiv3\\key-shift.png", KEY_SHIFT);
	LoadImage("editors\\uiv3\\key-tab.png", KEY_TAB);
	LoadImage("editors\\uiv3\\left-mouse-button.png", MOUSE_LMB);
	LoadImage("editors\\uiv3\\right-mouse-button.png", MOUSE_RMB);

	LoadImage("editors\\uiv3\\key-r.png", KEY_R);
	LoadImage("editors\\uiv3\\key-delete.png", KEY_DELETE);
	LoadImage("editors\\uiv3\\key-y.png", KEY_Y);
	LoadImage("editors\\uiv3\\key-return.png", KEY_RETURN);
	LoadImage("editors\\uiv3\\key-pgup.png", KEY_PGUP);
	LoadImage("editors\\uiv3\\key-pgdn.png", KEY_PGDN);
	LoadImage("editors\\uiv3\\key-f.png", KEY_F);
	LoadImage("editors\\uiv3\\key-g.png", KEY_G);
	LoadImage("editors\\uiv3\\key-z.png", KEY_Z);
	LoadImage("editors\\uiv3\\key-i.png", KEY_I);

	LoadImage("editors\\uiv3\\key-n.png", KEY_N);
	LoadImage("editors\\uiv3\\key-l.png", KEY_L);
	LoadImage("editors\\uiv3\\key-e.png", KEY_E);
	LoadImage("editors\\uiv3\\key-space.png", KEY_SPACE);
	LoadImage("editors\\uiv3\\key-t.png", KEY_T);
	LoadImage("editors\\uiv3\\key-o.png", KEY_O);
	LoadImage("editors\\uiv3\\key-q.png", KEY_Q);


	LoadImage("editors\\uiv3\\key-separator.png", KEY_SEPARATOR);
	LoadImage("editors\\uiv3\\key-separator-small.png", KEY_SEPARATOR_SMALL);

	LoadImage("editors\\uiv3\\favoritesmall.png", MEDIA_FAVORITE);

	LoadImage("editors\\uiv3\\group-edit.png", TOOL_GROUPEDIT);
	LoadImage("editors\\uiv3\\ungroup.png", TOOL_UNGROUP);
	LoadImage("editors\\uiv3\\group.png", TOOL_GROUP);
	LoadImage("editors\\uiv3\\group-save.png", TOOL_GROUPSAVE);

	LoadImage("editors\\uiv3\\trashcan.png", TOOL_TRASHCAN);
	LoadImage("editors\\uiv3\\unlock-tools.png", TOOL_UNLOCK);
	LoadImage("editors\\uiv3\\lock-tools.png", TOOL_LOCK);
	LoadImage("editors\\uiv3\\smart-object.png", TOOL_SMARTOBJECT);
	
	LoadImage("editors\\uiv3\\favoritesmall-dis.png", MEDIA_FAVORITE_DIS);
	LoadImage("editors\\uiv3\\key-maximize.png", KEY_MAXIMIZE);

	LoadImage("editors\\uiv3\\middle-mouse-button.png", MOUSE_MMB);

	LoadImage("editors\\uiv3\\object-horizontal.png", OBJECT_MOVE_XZ);
	LoadImage("editors\\uiv3\\object-vert.png", OBJECT_MOVE_Y);
	LoadImage("editors\\uiv3\\object-surface.png", OBJECT_MOVE_SURFACESCAN);
	LoadImage("editors\\uiv3\\object-findfloor.png", OBJECT_MOVE_FINDFLOOR);
	LoadImage("editors\\uiv3\\object-orientation.png", OBJECT_MOVE_ORIENTATION);
	LoadImage("editors\\uiv3\\object-lock.png", OBJECT_MOVE_LOCK);
	LoadImage("editors\\uiv3\\object-unlock.png", OBJECT_MOVE_UNLOCK);

	LoadImage("editors\\uiv3\\key-control-shift.png", KEY_CONTROL_SHIFT);

	LoadImage("editors\\uiv3\\i-info.png", ICON_INFO);
	LoadImage("editors\\uiv3\\temp_infinity.png", IMPORTER_ALL_MESH);

	LoadImage("editors\\uiv3\\ent-filter.png", TOOL_ENT_FILTER);

	LoadImage("editors\\marketplace\\ggmax.png", MARKETPLACE_GGMAX);
	LoadImage("editors\\marketplace\\gc-store.png", MARKETPLACE_GCSTORE);
	LoadImage("editors\\marketplace\\sketchfab.png", MARKETPLACE_SKETCHFAB);
	LoadImage("editors\\marketplace\\filler.png", MARKETPLACE_FILLER);
	LoadImage("editors\\marketplace\\marketplace.png", MARKETPLACE_HEADER);
	LoadImage("editors\\marketplace\\shockwave-sound.png", MARKETPLACE_SHOCKWAVESOUND);
	LoadImage("editors\\marketplace\\community.png", MARKETPLACE_COMMUNITY);
	
	#ifdef FREETRIALONDISCOUNT
	LoadImage("editors\\freetrial\\header-sale.png", FREETRIAL_HEADER);
	LoadImage("editors\\freetrial\\body-sale.png", FREETRIAL_BODY);
	#else
	LoadImage("editors\\freetrial\\header.png", FREETRIAL_HEADER);
	LoadImage("editors\\freetrial\\body.png", FREETRIAL_BODY);
	#endif
	LoadImage("editors\\freetrial\\notavailable.png", FREETRIAL_NOTAVAILABLE);
	LoadImage("editors\\freetrial\\oldschooldigit-base-50px.png", FREETRIAL_COUNTER_BASE);
	LoadImage("editors\\freetrial\\header-sale-oneday.png", FREETRIAL_COUNTER_ONEDAY);			
	LoadImage("editors\\uiv3\\filler-rounded.png", WELCOME_FILLERROUNDED);

	LPSTR pWelcomeHeaderHUB = "editors\\uiv3\\welcome-header.png";
	if (g_bFreeTrialVersion == true)
	{
		pWelcomeHeaderHUB = "editors\\freetrial\\welcome-header.png";
	}
	if (FileExist(pWelcomeHeaderHUB))
	{
		LoadImage(pWelcomeHeaderHUB, WELCOME_HEADER);
		iWelcomeHeaderType = 3;
	}

	LoadImage("editors\\uiv3\\filetype-ogg.png", FILETYPE_OGG);
	LoadImage("editors\\uiv3\\filetype-wav.png", FILETYPE_WAV);
	LoadImage("editors\\uiv3\\filetype-mp3.png", FILETYPE_MP3);

	LoadImage("editors\\uiv3\\filetype-video.png", FILETYPE_VIDEO);

	LoadImage("editors\\uiv3\\player-start.png", PLAYER_START);
	LoadImage("editors\\uiv3\\player-start2.png", PLAYER_START2);
	LoadImage("editors\\uiv3\\filetype-particle.png", FILETYPE_PARTICLE);

	SetIconSet(true);
	SetMipmapNum(1); //PE: mipmaps not needed.
	image_setlegacyimageloading(true);

	LoadImage("editors\\uiv3\\light-point.png", LIGHT_POINT);
	LoadImage("editors\\uiv3\\light-spot.png", LIGHT_SPOT);

	LoadImage("editors\\uiv3\\checkbox-character-off.png", FILTER_CHAR_OFF);
	LoadImage("editors\\uiv3\\checkbox-character-on.png", FILTER_CHAR_ON);
	LoadImage("editors\\uiv3\\checkbox-scenary-off.png", FILTER_SCENARY_OFF);
	LoadImage("editors\\uiv3\\checkbox-scenary-on.png", FILTER_SCENARY_ON);
	LoadImage("editors\\uiv3\\checkbox-favorite-off.png", FILTER_FAVORITE_OFF);
	LoadImage("editors\\uiv3\\checkbox-favorite-on.png", FILTER_FAVORITE_ON);
	LoadImage("editors\\uiv3\\checkbox-hud-off.png", FILTER_HUD_OFF);
	LoadImage("editors\\uiv3\\checkbox-hud-on.png", FILTER_HUD_ON);
	LoadImage("editors\\uiv3\\checkbox-elements-off.png", FILTER_ELEMENTS_OFF);
	LoadImage("editors\\uiv3\\checkbox-elements-on.png", FILTER_ELEMENTS_ON);
	LoadImage("editors\\uiv3\\checkbox-user-off.png", FILTER_USER_OFF);
	LoadImage("editors\\uiv3\\checkbox-user-on.png", FILTER_USER_ON);
	LoadImage("editors\\uiv3\\checkbox-dlua-off.png", FILTER_DLUA_OFF);
	LoadImage("editors\\uiv3\\checkbox-dlua-on.png", FILTER_DLUA_ON);

	LoadImage("editors\\uiv3\\pin.png", MEDIA_PIN);
	LoadImage("editors\\uiv3\\unpin.png", MEDIA_UNPIN);
	LoadImage("editors\\uiv3\\tool-mountain.png", TOOL_TERRAIN_TOOLBAR);

	LoadImage("editors\\uiv3\\ccp-none.png", CCP_NONE);
	LoadImage("editors\\uiv3\\ccp-empty.png", CCP_EMPTY);

	LoadImage("editors\\uiv3\\terrain-random.png", TERRAIN_RANDOM);
	LoadImage("editors\\uiv3\\terrain-pick.png", TERRAIN_PICK);
	LoadImage("editors\\uiv3\\terrain-write.png", TERRAIN_WRITE);
	LoadImage("editors\\uiv3\\terrain-restore.png", TERRAIN_RESTORE);
	if (FileExist("editors\\uiv3\\storyboard-header6.png"))
	{
		LoadImage("editors\\uiv3\\storyboard-header6.png", STORYBOARD_HEADER);
		g_Storyboard_header_height = 94.0f; //PE: More storyboard area. and same size as hud header.
	}
	else
		if (FileExist("editors\\uiv3\\storyboard-header5.png"))
	{
		LoadImage("editors\\uiv3\\storyboard-header5.png", STORYBOARD_HEADER);
		g_Storyboard_header_height = 114.0f; //PE: Way better on ultra wide monitors.
	}

	LoadImage("editors\\uiv3\\entity_image2.png", STORYBOARD_BACKDROP);
	LoadImage("editors\\uiv3\\entity_music2.png", STORYBOARD_MUSIC);
	LoadImage("editors\\uiv3\\entity_checkpoint2.png", STORYBOARD_PREVIEW);
	LoadImage("editors\\templates\\backdrops\\transparent-backdrop.png", STORYBOARD_TRANSPARET);

	LoadImage("tutorialbank\\welcome-video.jpg", WELCOME_VIDEO);

	LoadImage("editors\\uiv3\\tree_tool.png", TOOL_PAINTTREE); //PE: Need another one for this, no +
	LoadImage("editors\\uiv3\\tree_add.png", TOOL_TREE_ADD);
	LoadImage("editors\\uiv3\\tree_delete.png", TOOL_TREE_DELETE);
	LoadImage("editors\\uiv3\\tree_move.png", TOOL_TREE_MOVE);
	LoadImage("editors\\uiv3\\trees_add.png", TOOL_TREES_ADD);
	LoadImage("editors\\uiv3\\trees_delete_2.png", TOOL_TREES_DELETE);

	LoadImage("editors\\uiv3\\click-here-box.png", BOX_CLICK_HERE);

	LoadImage("editors\\uiv3\\brain-icon.png", BRAIN_ICON);
	LoadImage("editors\\uiv3\\icon-question.png", QUESTION_ICON);

	LoadImage("editors\\uiv3\\terrain mover.dds", UI3D_TERRAINMOVER);//dotmiddleobject.png", UI3D_DOTMIDDLEOBJECTS);

	LoadImage("editors\\uiv3\\icon_bush.png", TOOL_PAINTBUSH); //PE: Need another one for this, no +
	LoadImage("editors\\uiv3\\add_bush.png", TOOL_BUSH_ADD);
	LoadImage("editors\\uiv3\\delete_bush.png", TOOL_BUSH_DELETE);
	LoadImage("editors\\uiv3\\move_bush.png", TOOL_BUSH_MOVE);
	LoadImage("editors\\uiv3\\paint_bushes.png", TOOL_BUSHES_ADD);
	LoadImage("editors\\uiv3\\delete_bushes.png", TOOL_BUSHES_DELETE);
	LoadImage("editors\\uiv3\\scale_bush.png", TOOL_BUSH_SCALE);
	LoadImage("editors\\uiv3\\scale_tree.png", TOOL_TREE_SCALE);

	LoadImage("editors\\uiv3\\hub-livebroadcasts.png", HUB_LIVEBROADCAST);
	LoadImage("editors\\uiv3\\hub-discordwide.png", HUB_DISCORD);
	LoadImage("editors\\uiv3\\hub-facebook.png", HUB_FACEBOOK);
	LoadImage("editors\\uiv3\\hub-forum.png", HUB_FORUM);
	LoadImage("editors\\uiv3\\hub-workshopitem.png", HUB_WORKSHOPITEM);
	LoadImage("editors\\uiv3\\hub-tiktok.png", HUB_TIKTOK);
	LoadImage("editors\\uiv3\\hub-twitter.png", HUB_TWITTER);
	LoadImage("editors\\uiv3\\hub-userguide.png", HUB_USERGUIDE);
	LoadImage("editors\\uiv3\\hub-website.png", HUB_WEBSITE);

	LoadImage("editors\\uiv3\\hub-commtut-0-placeholder.png", HUB_COMMTUT0);
	LoadImage("editors\\uiv3\\hub-commtut-1-bmi.png", HUB_COMMTUT1);
	LoadImage("editors\\uiv3\\hub-commtut-2-plemsoft.png", HUB_COMMTUT2);
	LoadImage("editors\\uiv3\\hub-commtut-3-extreme.png", HUB_COMMTUT3);

	LoadImage("editors\\uiv3\\image-icon.png", SCREENEDITOR_IMAGE);
	LoadImage("editors\\uiv3\\text-icon.png", SCREENEDITOR_TEXT);
	LoadImage("editors\\uiv3\\button-icon.png", SCREENEDITOR_BUTTON);
	LoadImage("editors\\uiv3\\radiobutton-icon.png", SCREENEDITOR_RADIOBUTTON);
	LoadImage("editors\\uiv3\\tick-box-icon.png", SCREENEDITOR_TICKBOX);
	LoadImage("editors\\uiv3\\slider-icon.png", SCREENEDITOR_SLIDER);
	LoadImage("editors\\uiv3\\progressbar-icon.png", SCREENEDITOR_PROGRESSBAR);
	LoadImage("editors\\uiv3\\textarea-icon.png", SCREENEDITOR_TEXTAREA);
	LoadImage("editors\\uiv3\\video-icon.png", SCREENEDITOR_VIDEO);

	ImNodes::CreateContext();

	gridedit_makelighthybrid();

	image_setlegacyimageloading(false);
	SetMipmapNum(-1);

	ChangeGGFont("editors\\uiv3\\Roboto-Medium.ttf",15);

	extern char launchLoadOnStartup[260];
	if (strlen(launchLoadOnStartup) > 0 ) 
	{
		strcpy(cDirectOpen, launchLoadOnStartup);
		if (strlen(cDirectOpen) > 0) {

			t.returnstring_s = cDirectOpen;
			if (t.returnstring_s != "")
			{
				if (cstr(Lower(Right(t.returnstring_s.Get(), 4))) == ".fpm")
				{
					t.gridentity = 0;
					t.inputsys.constructselection = 0;
					t.inputsys.domodeentity = 1;
					t.grideditselect = 5;
					editor_refresheditmarkers();

					g.projectfilename_s = t.returnstring_s;
					gridedit_load_map();
					g_EntityClipboard.clear(); //PE: Clear any old copy/paste.
					undosys_clearall(); //PE: Clear undo redo system.
					t.terrain.grassregionx1 = t.terrain.grassregionx2;
					bUpdateVeg = true;

					//Locate player start marker.
					for (t.e = 1; t.e <= g.entityelementlist; t.e++)
					{
						if (t.entityelement[t.e].bankindex > 0)
						{
							if (t.entityprofile[t.entityelement[t.e].bankindex].ismarker == 1 && t.entityprofile[t.entityelement[t.e].bankindex].lives != -1)
							{
								//Point camera.
								t.obj = t.entityelement[t.e].obj;
								if (t.obj > 0) {
									float offsetx = ((float)GetDesktopWidth() - renderTargetAreaSize.x) * 0.25f;
									t.cx_f = ObjectPositionX(t.obj) + offsetx; //t.editorfreeflight.c.x_f;
									t.cy_f = ObjectPositionZ(t.obj); //t.editorfreeflight.c.z_f;
								}
								break;
							}
						}
					}

				}
			}
		}

		iLastUpdateVeg = 0;
		bUpdateVeg = true;
	}
	else 
	{
	}

	//PE: redirect all to image.
	SetCameraToImage(0, g.postprocessimageoffset, GetDisplayWidth(), GetDisplayHeight(), 2);
	imgui_is_running = true;

	//Make sure we have envmap.
	visuals_justshaderupdate();
	t.visuals.refreshskysettingsfromlua = true;
	cubemap_generateglobalenvmap();
	t.visuals.refreshskysettingsfromlua = false;

	t.visuals.VegQuantity_f = t.gamevisuals.VegQuantity_f;
	t.visuals.VegWidth_f = t.gamevisuals.VegWidth_f;
	t.visuals.VegHeight_f = t.gamevisuals.VegHeight_f;

	t.terrain.grassupdateafterterrain = 1;
	t.terrain.grassupdateafterterrain = 0;
	ShowVegetationGrid();
	visuals_justshaderupdate();

	// Moved last so we can load levels before main loop.
	// start thread loader for Character Creator texture files (multi-threaded loading) (saves 2s if started CCP)
	timestampactivity(0, "preload CCP textures early");
	charactercreatorplus_preloadinitialcharacter();

	// build character types list early as needed for FPE parsing
	extern void charactercreatorplus_populatechartypes(void);
	timestampactivity(0, "preload FPE character types");
	charactercreatorplus_populatechartypes();

	//  Main loop
	iStartupTime = MAXTimer();
	timestampactivity(0, "Guru Map Editor Loop Starts");

	//Default to OBJECT TOOL panel (so can view tutorials right away)
	bForceKey = true;
	csForceKey = "o";
	t.gridentitymarkersmodeonly = 0; 
	t.grideditselect = 0;
	
	// trigger an alha/beta prompt
	g_iCountdownToAlphaBetaMessage = 20;

	//Trigger welcome screen.
	bWelcomeScreen_Window = false;

	extern bool bSpecialEditorFromStandalone;
	extern bool bEnsureIntroVideoIsNotRun;
	extern bool bReturnToWelcome;
	if (pref.iDisplayWelcomeScreen == 1)
	{
		if (bSpecialEditorFromStandalone && bReturnToWelcome)
		{
			bWelcomeNoBackButton = true;
			bWelcomeScreen_Window = true;
		}
		else if (!bSpecialEditorFromStandalone)
		{
			bWelcomeNoBackButton = true;
			bWelcomeScreen_Window = true;
		}
	}

	//LB: ensure special return from standalone flag reset if final destination was welcome HUB (to prevent storyboard being forced to switch)
	if (bReturnToWelcome == true)
	{
		bSpecialEditorFromStandalone = false;
		bEnsureIntroVideoIsNotRun = true;
		bReturnToWelcome = false;
	}

	t.gridentitygridlock = pref.iGridMode;
}

void mapeditorexecutable_loop_leavetestgame(void)
{
	bBlockImGuiUntilNewFrame = true;
	bRenderNextFrame = false;
	SetCameraToImage(0, g.postprocessimageoffset, GetDisplayWidth(), GetDisplayHeight(), 2); //switch back to render target.
	iLaunchAfterSync = 0;
	bImGuiInTestGame = false;
	fpe_current_loaded_script = -1; //Refresh dlua after testgame.
	sky_show(); //Restore skybox.
	// nor post effects and performance levers set from Lua: put back the editor's own
	extern void LuaPostEffects_Clear(void* pVisualsToRestore);
	LuaPostEffects_Clear(&t.visuals);
	extern void LuaRenderSettings_Clear(void);
	LuaRenderSettings_Clear();
	iLastUpdateVeg = 0; //Veg: update any changes from F9
	bUpdateVeg = true;

	// set vsync back on when we return to the editor so we don't 100% the GPU
	if (g.iEditorVSync == 0)
		wiEvent::SetVSync(false); // see if this improves performance in the level editor, was ( true );
}

ImVec2 back_renderTargetAreaPos;
ImVec2 back_renderTargetAreaSize;
int backup_pickedObject = -1;
int backup_gridentity = -1;
int backup_gridentityobj = -1;
int back_iLastResolutionWidth = 0;
int back_iLastResolutionHeight = 0;
bool bFakeStandaloneTest = false;
int iTriggerGrassTreeUpdate = 0;
int iMaxZeroHeight = 0;
bool commonexecutable_loop_for_game(void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// cannot proceed until we have visual data
	if (t.visuals.CameraNEAR_f == 0 && t.visuals.CameraFAR_f == 0) return false;

	//PE: Support delayed terrain update in standalone.
	extern int iTriggerInvalidateAfterFrames;

	if (iBlockRenderingForFrames > 0)
	{
		extern bool g_bNoSwapchainPresent;
		iBlockRenderingForFrames--;
		if (iBlockRenderingForFrames <= 0)
			g_bNoSwapchainPresent = false;
		else
			g_bNoSwapchainPresent = true;
	}

	if (iTriggerGrassTreeUpdate > 0)
	{
		if (iTriggerGrassTreeUpdate == 5)
		{
			float height = 0;
			if (t.visuals.bEnableEmptyLevelMode)
			{
				//PE: Just give it 5 frame on empty levels.
				if (iMaxZeroHeight++ < 6)
				{
					iTriggerGrassTreeUpdate++;
				}
				else
				{
					iMaxZeroHeight = 0;
				}
			}
			else
			{
				if (!GGTerrain_GetHeight(CameraPositionX(), CameraPositionZ(), &height, 0, 0))
				{
					//Height not available in this are ?.
					iTriggerGrassTreeUpdate++;
				}
			}
		}
		if (iTriggerGrassTreeUpdate == 1)
		{
			ggterrain_extra_params.iUpdateGrass = 1;
			ggterrain_extra_params.iUpdateTrees = 1;
			//PE: Also update probe after terrain is done.
			extern bool g_bLightProbeScaleChanged;
			g_bLightProbeScaleChanged = true;
		}
		iTriggerGrassTreeUpdate--;
	}
	if (iTriggerInvalidateAfterFrames > 0)
	{
		if (iTriggerInvalidateAfterFrames == 1)
		{
			//PE: Height should be availble now.
			//PE: Also update grass and tree, i have some levels where grass is floating in air.
			iTriggerGrassTreeUpdate = 20;
		}
		if (iTriggerInvalidateAfterFrames == 10)
		{
			//PE: Need to do this delayed so terrain update have been updated with new data.
			//PE: Invalidate everything so custom sculpt data is updated with textures ...
			//PE: @Paul not sure if there is a better way to do this, but this works :)
			// While the level-load reveal cover is still up, this refresh is for the
			// LEGACY terrain only — the Wicked chunks generate gated on heights-ready
			// and would just be rebuilt twice (doubling the covered build). Mid-session
			// full invalidations (terrain parameter changes) still reach the bridge.
			uint32_t invalFlags = GGTERRAIN_INVALIDATE_ALL;
			if (GGTerrain::GGTerrainWicked_IsRevealHeld()) invalFlags |= GGTERRAIN_INVALIDATE_NO_WICKED;
			GGTerrain::GGTerrain_InvalidateRegion(-1000000.0, -1000000.0, 1000000.0, 1000000.0, invalFlags);
		}
		iTriggerInvalidateAfterFrames--;
	}

	// called from both mapeditor(test game) and standalone game
	if (bTriggerFovUpdate)
	{
		float fUsedFOV = t.visuals.CameraFOV_f;
		if (bImGuiInTestGame==false) fUsedFOV = 45;
		bTriggerFovUpdate = false;
		float fCameraFov = XM_PI / ((fUsedFOV) / 15.0f); //Fit GG settings.
		if (bImGuiInTestGame == true)
		{
			fCameraFov = GGToRadian(fUsedFOV); // Oops - backwards logic, lower FOV needs lower angle passed in
		}
		wiScene::GetCamera().CreatePerspective((float)master.masterrenderer.GetLogicalWidth(), (float)master.masterrenderer.GetLogicalHeight(), t.visuals.CameraNEAR_f, t.visuals.CameraFAR_f, fCameraFov);
		wiScene::GetCamera().SetDirty(true);
	}

	void RenderPreviewEmitter(void);
	RenderPreviewEmitter();

	if (iLaunchAfterSync == 201)
	{
		//As we set 201 launch testgame after we have a imgui frame, we need to set Scissors here.
		bImGuiInTestGame = true;
		g_bDisableQuitFlag = true;

		//PE: Use the actual screen resolution, not the windows size.
		extern ImVec2 fImGuiScissorTopLeft;
		extern ImVec2 fImGuiScissorBottomRight;
		fImGuiScissorTopLeft = { 0, 0 };
		fImGuiScissorBottomRight = { (float)GetSystemMetrics(SM_CXSCREEN),  (float)GetSystemMetrics(SM_CYSCREEN) };

		//PE: Used by LUA.
		extern DWORD g_dwScreenWidth;
		extern DWORD g_dwScreenHeight;
		g_dwScreenWidth = fImGuiScissorBottomRight.x;
		g_dwScreenHeight = fImGuiScissorBottomRight.y;
		g_pGlob->iScreenWidth = fImGuiScissorBottomRight.x;
		g_pGlob->iScreenHeight = fImGuiScissorBottomRight.y;

		//PE: Change resolution in wicked.
		if (wiGraphics::GetDevice() != nullptr)
		{
			int width = fImGuiScissorBottomRight.x;
			int height = fImGuiScissorBottomRight.y;
			float fNearDistance = DEFAULT_NEAR_PLANE;
			float fFarDistance = DEFAULT_FAR_PLANE;
		}

		g_bDisableQuitFlag = false;
		t.postprocessings.fadeinvalue_f = 0.0f; //PE: Make sure we trigger default settings like music / volume ...

		WickedCall_DisplayCubes(false); //PE: Hide terrain tool cubes.
		wiProfiler::SetEnabled(false); //PE: Clear stat for a fresh testgame or standalone.

		//LB: need to hide shooter genre debug here as UI still shows them even AFTER the preview_init has been called!
		// hide visual logic dots and arcs
		extern bool g_bDotsAreVisible;
		if (g_bDotsAreVisible)
		{
			//DrawCharacterDots(false);
			DrawLogicNodes(false);
			g_bDotsAreVisible = false;
		}

		if (bStartInvulnerableMode)
		{
			bStartInvulnerableMode = false;
			bInvulnerableMode = true;
		}

		iLaunchAfterSync = 202;
		return true;
	}
	extern bool bSpecialStandalone;
	if (bSpecialStandalone || t.game.gameisexe == 1)
	{
		if (iLaunchAfterSync == 799 || iLaunchAfterSync == 699)
		{
			iLaunchAfterSync = 202;
			void AddRemoteProjectFonts(void);
			AddRemoteProjectFonts();
		}
	}
	if (iLaunchAfterSync == 202)
	{
		int iGridObj = g.ebeobjectbankoffset + 1000;
		if (ObjectExist(iGridObj))
			DeleteObject(iGridObj);
		extern uint32_t PreviewWPERoot;
		if (PreviewWPERoot != 0)
		{
			//PE: Delete effects.
			void DeleteEmitterEffects(uint32_t root);
			DeleteEmitterEffects(PreviewWPERoot);
			PreviewWPERoot = 0;
		}

		bImGuiInTestGame = true;
		g_bDisableQuitFlag = true;
		extern bool	g_bDrawSpritesFirst;
		extern bool bMainLoopRunning;
		if (g_bDrawSpritesFirst)
		{
			UpdateSprites();
		}
		bMainLoopRunning = false;
		if (editor_previewmap_loopcode(0) == true)
		{
			// when loop ends, run code after loop
			bImGuiInTestGame = false;
			bBlockImGuiUntilNewFrame = true;
			bFakeStandaloneTest = false;
			bRenderNextFrame = false;

			wiProfiler::SetEnabled(false); //PE: Clear stat.
			if (bProfilerEnable)
			{
				wiProfiler::SetEnabled(true);
			}
			editor_previewmap_afterloopcode(0);
			mapeditorexecutable_loop_leavetestgame();

			// mapeditor or standalone game
			if (t.game.gameisexe == 1)
			{
				// trigger exit from game
				iLaunchAfterSync = 0;
				PostQuitMessage(0);
			}
			else
			{
				// map editor restore
				//Restore resolution and scissor
				float fNearDistance = DEFAULT_NEAR_PLANE;
				float fFarDistance = DEFAULT_FAR_PLANE; //PE: Default editor camera range.
				extern ImVec2 fImGuiScissorTopLeft;
				extern ImVec2 fImGuiScissorBottomRight;
				fImGuiScissorTopLeft = back_renderTargetAreaPos;
				fImGuiScissorBottomRight = back_renderTargetAreaPos + back_renderTargetAreaSize;
				g_pGlob->iScreenWidth = back_iLastResolutionWidth;
				g_pGlob->iScreenHeight = back_iLastResolutionHeight;
				if(backup_pickedObject != -1)
					t.widget.pickedObject = backup_pickedObject;
				if(backup_gridentity != -1)
					backup_gridentity = t.gridentity;
				if (backup_gridentityobj != -1)
					backup_gridentityobj = t.gridentityobj;

				if(!t.showeditorelements) editor_toggle_element_vis((bool)t.showeditorelements);

				// continue
				iSkibFramesBeforeLaunch = 2;
				iLaunchAfterSync = 203;
			}

			//PE: Reset if we have any hanging keys from test game.
			ImGuiIO& io = ImGui::GetIO();
			io.KeySuper = false;
			io.KeyCtrl = false;
			io.KeyAlt = false;
			io.KeyShift = false;
			for (int iTemp = 0; iTemp < 256; iTemp++)
			{
				io.KeysDown[iTemp] = 0;
			}
			io.MouseDown[0] = 0; //PE: Mouse (release) is also loast inside blocking dialogs. Reset!
			io.MouseDown[1] = 0;
			io.MouseDown[2] = 0;
			io.MouseDown[3] = 0;

		}
		g_bDisableQuitFlag = false;
		return true;
	}

	// allow continue on to editor if appropriate
	return false;
}

void gridedit_setsmartobjectvisibilityinrubberband(bool bVisible)
{
	if (g.entityrubberbandlist.size() > 0)
	{
		for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
		{
			int e = g.entityrubberbandlist[i].e;
			if (t.entityprofile[t.entityelement[e].bankindex].ischildofgroup != 0)
			{
				if (t.entityprofile[t.entityelement[e].bankindex].ismarker != 0)
				{
					if (bVisible)
						ShowObject(t.entityelement[e].obj);
					else
						HideObject(t.entityelement[e].obj);
				}
			}
		}
	}
}

void SetGlobalGraphicsSettings( int level ) // 0=lowest, 1=medium, 2=high, 3=ultra, default to 2 (high)
{
	GGTerrain::GGTerrain_SetPerformanceMode( level );
	GGTrees::GGTrees_SetPerformanceMode( level );
	GGGrass::GGGrass_SetPerformanceMode( level );

	switch( level )
	{
		case 0: // low
		{
			t.visuals.bSSREnabled = false;
			t.visuals.bFXAAEnabled = false;
			t.visuals.bLightShafts = false;
			t.visuals.bLensFlare = false;
			t.visuals.bReflectionsEnabled = false;
			// GGMAX 2.39: t.visuals.iShadowSpotCascadeResolution = 512;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointMax = 2;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointResolution = 256;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotMax = 1;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotResolution = 256;   <-- preset no longer stamps shadow settings
		} break;

		case 1: // medium
		{
			t.visuals.bSSREnabled = false;
			t.visuals.bFXAAEnabled = true;
			t.visuals.bLightShafts = true;
			t.visuals.bLensFlare = true;
			t.visuals.bReflectionsEnabled = true;
			// GGMAX 2.39: t.visuals.iShadowSpotCascadeResolution = 1024;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointMax = 4;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointResolution = 512;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotMax = 4;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotResolution = 512;   <-- preset no longer stamps shadow settings
		} break;

		case 2: // high
		{
			t.visuals.bSSREnabled = false;
			t.visuals.bFXAAEnabled = true;
			t.visuals.bLightShafts = true;
			t.visuals.bLensFlare = true;
			t.visuals.bReflectionsEnabled = true;
			// GGMAX 2.39: t.visuals.iShadowSpotCascadeResolution = 2048;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointMax = 12;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointResolution = 512;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotMax = 8;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotResolution = 512;   <-- preset no longer stamps shadow settings
		} break;

		case 3: // ultra
		{
			t.visuals.bSSREnabled = false;
			t.visuals.bFXAAEnabled = true;
			t.visuals.bLightShafts = true;
			t.visuals.bLensFlare = true;
			t.visuals.bReflectionsEnabled = true;
			// GGMAX 2.39: t.visuals.iShadowSpotCascadeResolution = 2048;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointMax = 16;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowPointResolution = 512;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotMax = 8;   <-- preset no longer stamps shadow settings
			// GGMAX 2.39: t.visuals.iShadowSpotResolution = 512;   <-- preset no longer stamps shadow settings
		} break;
	}
	// Tree shadow LOD distance + cascade range are deliberately NOT preset-stamped: they
	// are level-authored (saved in the FPM visuals, tuned in the Shadows panel), and the
	// Wicked_Update_Visuals below re-asserts t.visuals over whatever
	// GGTrees_SetPerformanceMode just dialed — authored values survive any quality preset.
		// ============================================================================================
	// GGMAX 2.39: the quality presets NO LONGER STAMP ANY SHADOW SETTING.
	//
	// Every t.visuals.iShadow* write in the four cases below is commented out above. They were
	// overwriting the level's authored Shadows panel the moment a test game started, because
	// M-GridEdit_part2.cpp:1045 runs this whenever pref.iTestGameGraphicsQuality != 2 (High).
	// Measured with DUMP_SHADOWQTY on Snowy Mountain Stroll: editor 8 spot / 16 point, test game
	// 1 / 2 -- the LOW preset's values -- while gamevisuals still held the authored 8 / 16.
	//
	// The panel then displayed those as "0/0" because its combos match only {0,4,8,12,16}, and 1
	// and 2 are in neither list. (Seeding from the nearest listed value does NOT rescue that: the
	// nearest entry to 1 IS 0, so it still reads zero. The override itself had to go.)
	//
	// This follows the rule already established for the two settings directly below: tree shadow
	// LOD distance and cascade range are level-authored and deliberately exempt from presets.
	// Shadow quantity and resolution are authored in the same panel and now behave the same way.
	// The non-shadow preset writes (SSR, FXAA, light shafts, lens flare, reflections) are
	// untouched -- they are a different panel and were not part of the report.
	// ============================================================================================
Wicked_Update_Visuals( &t.visuals );
}

void mapeditorexecutable_full_folder_refresh(void)
{
	// work out the project folder path for third location of assets
	static char cFullProjectWritePath[MAX_PATH];
	static char cFullWritePath[MAX_PATH];

	// only update folders and files if flagged
	if (!bExternal_Entities_Init)
	{
		// do entities init once when flagged
		bExternal_Entities_Init = true;

		// eventually ensure that the tree view in process_entity_library_v2 will be updated so the user can see any new folders they created.
		extern bool bTreeViewInitInNextFrame;
		bTreeViewInitInNextFrame = true;

		// Go through all media folders
		cstr pOld = GetDir();
		for (int iMediaFolderType = 0; iMediaFolderType <= 6; iMediaFolderType++)
		{
			// folders to check
			LPSTR pMediaFolderPattern = "";
			if (iMediaFolderType == 0) pMediaFolderPattern = "entitybank";
			if (iMediaFolderType == 1) pMediaFolderPattern = "audiobank";
			if (iMediaFolderType == 2) pMediaFolderPattern = "imagebank";
			if (iMediaFolderType == 3) pMediaFolderPattern = "videobank";
			if (iMediaFolderType == 4) pMediaFolderPattern = "scriptbank";
			if (iMediaFolderType == 5) pMediaFolderPattern = "particlesbank";
			if (iMediaFolderType == 6) pMediaFolderPattern = "charactercreatorplus\\animations";
			
			// use GetMainEntityList to add root, writables and project folder files
			strcpy(cFullWritePath, pMediaFolderPattern);
			GG_GetRealPath(cFullWritePath, 1);
			cStr CurrentPath = pOld + cStr("\\") + cStr(pMediaFolderPattern);
			if (strnicmp(CurrentPath.Get(), cFullWritePath, CurrentPath.Len()) == 0)
			{
				// same folder means no separate writables area, i.e. GG_GetRealPath create mode failed
				// only root folder for non-writable systems
				GetMainEntityList(pMediaFolderPattern, "", NULL, "", true, iMediaFolderType);
			}
			else
			{
				// tracks folder creations
				cFolderItem* pLastFolder = &MainEntityList;
				cFolderItem* pFirstOfTheLastFolder = NULL;

				// writables folder
				GetMainEntityList(cFullWritePath, "", pFirstOfTheLastFolder, "w:", true, iMediaFolderType);
				pLastFolder = &MainEntityList;
				while (pLastFolder->m_pNext)
				{
					pLastFolder = pLastFolder->m_pNext;
				}

				// root folder
				SetDir(pOld.Get());
				GetMainEntityList(pMediaFolderPattern, "", pLastFolder, "", false, iMediaFolderType);

				extern char szBeforeChangeWriteDir[MAX_PATH];
				extern bool bIncludeDocumentFolderInRemoteProject;
				//PE: Normal writefolder if remote project.
				if(bIncludeDocumentFolderInRemoteProject && strlen(szBeforeChangeWriteDir) > 0)
				{
					char newpath[MAX_PATH];
					strcpy(newpath, szBeforeChangeWriteDir);
					strcat(newpath, "Files");
					SetDir(newpath);
					GetMainEntityList(pMediaFolderPattern, "", pLastFolder, "", false, iMediaFolderType);
				}

				SetDir(pOld.Get());

			}
		}

		//Sort folder entrys.
		SetDir(pOld.Get());
		cFolderItem *pNewFolder = (cFolderItem *)&MainEntityList;
		cFolderItem *m_pfirstFolder = NULL;
		int mc = 0;
		while (pNewFolder->m_pNext)
		{
			if (!m_pfirstFolder) m_pfirstFolder = pNewFolder->m_pNext;
			pNewFolder = pNewFolder->m_pNext;
			mc++;
		}
		if (mc > 1)
		{
			//#### SORT ####
			char ** cptr = new char *[mc + 1];
			cFolderItem *m_pSortFolder = m_pfirstFolder->m_pNext;
			int mc2 = 0;
			for (int a = 0; a < mc; a++)
			{
				if (m_pSortFolder)
				{
					cptr[a] = (char *)m_pSortFolder;
					m_pSortFolder = m_pSortFolder->m_pNext;
					mc2++;
				}
			}
			qsort(cptr, mc2, sizeof(cptr[0]), cstring_cmp_folder);
			m_pSortFolder = m_pfirstFolder->m_pNext;
			m_pfirstFolder->m_pNext = (cFolderItem *)cptr[0];
			for (int a = 0; a < mc2; a++)
			{
				m_pSortFolder = (cFolderItem *)cptr[a];
				if (m_pSortFolder)
				{
					if (a + 1 < mc2) m_pSortFolder->m_pNext = (cFolderItem *)cptr[a + 1];
				}
			}
			delete[] cptr;
			if (m_pSortFolder) m_pSortFolder->m_pNext = NULL;
		}
	}

	static bool bScan_Files_List = true;
	static int bScan_Files_Start = 50; //Wait some frames before we start , so terrain ... can get a head start.

	extern int g_iScannedFiles;
	extern std::vector<cFolderItem::sFolderFiles *> g_ScanFpeFiles;
	extern bool g_bFpeScanning;

	if (bExternal_Entities_Init && bScan_Files_List && bScan_Files_Start-- <= 0)
	{
		cFolderItem *pSearchFolder = &MainEntityList;
		pSearchFolder = pSearchFolder->m_pNext;
		while (pSearchFolder) {
			if (pSearchFolder->m_pFirstFile) {
				cFolderItem::sFolderFiles * searchfiles = pSearchFolder->m_pFirstFile->m_pNext;
				while (searchfiles) {
					//PE: For now only scan fpe, we might need additional info about other file types but ...
					if (searchfiles->iType == 0)
					{
						g_ScanFpeFiles.push_back(searchfiles);
					}
					searchfiles = searchfiles->m_pNext;
				}
			}
			pSearchFolder = pSearchFolder->m_pNext;
		}
		fpe_thread_start();
		bScan_Files_List = false;
	}
}

void launchOrShowEditorCore ( char* pFolderName, char* pWindowNameA, char* pWindowNameB, char* pAppName )
{
	LPSTR pEditorWindowTitle = pWindowNameA;
	if (WindowExist(pEditorWindowTitle) == 0)
	{
		pEditorWindowTitle = pWindowNameB;
		if (WindowExist(pEditorWindowTitle) == 0)
		{
			// not here, launch it!
			char pOldDir[MAX_PATH];
			strcpy(pOldDir, GetDir());
			SetDir("..");
			SetDir("Tools\\");
			SetDir(pFolderName);
			ExecuteFile(pAppName, "", "");
			SetDir(pOldDir);
		}
		else
		{
			// found you!
			WindowToFront(pEditorWindowTitle);
		}
	}
	else
	{
		// found you!
		WindowToFront(pEditorWindowTitle);
	}
}

void launchOrShowParticleEditor(void)
{
	launchOrShowEditorCore("Particle Editor","Particle Editor 0.7b","Particle Editor","particle_editor.exe");
}

void launchOrShowBuildingEditor(void)
{
	launchOrShowEditorCore("Building Editor", "GameGuru Building Editor", "GameGuru Building Editor*", "GameGuru Building Editor.exe");
}

