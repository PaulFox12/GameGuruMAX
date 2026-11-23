//
// Automation harness: a command read from auto_command.txt beside the exe, its answer written to auto_result.txt, both
// appended to auto_log.txt. The file protocol, the verbs and their answers are those of the DX12 build's harness, so one
// script can drive either build the same way (load a level, place the camera, read the frame rate, take a screenshot)
//

#include "stdafx.h"
#include "AutomationHarness.h"
#include "gameguru.h"
#include "globstruct.h"
#include "master.h"
namespace Tracers { void DebugLastAdded(char* out, int osize); }
#include "..\GameGuru\Imgui\imgui.h"
#include "..\GameGuru\Imgui\imgui_internal.h"
#include "..\GameGuru\Imgui\imgui_gg_dx11.h"
#include <stdio.h>
#include <string.h>

namespace GGTerrain { int GGTerrain_GetHeight( float x, float z, float* outHeight, int accurateButSlow, int includeFlatAreas ); }
namespace GGTerrain { void GGTerrain_AddEnvProbeList( float x, float y, float z, float range, float quatx, float quaty, float quatz, float quatw, float sx, float sy, float sz, float brightness ); }
namespace GGTrees { extern int gg_tree_local_shadows, gg_tree_local_shadow_lod; extern unsigned int gg_tree_local_shadow_counts[ 3 ]; }
extern int g_iGunSmokePuffs, g_iGunSmokeNoKeyOn; // GUNSMOKE_STATE, SET_GUNSMOKENOKEY (G-Gun.cpp, M-Gun.cpp)
extern int g_iLuaQuietSkip; extern int g_iLuaFarRows; extern int g_iLuaWalkSplit; void lua_loopstats_text ( char* pOut, int iSize, bool bReset ); // SET_LUAQUIET, LUA_LOOP_STATS (M-LUA.cpp)
void lua_row_text ( int e, char* pOut, int iSize ); // LUA_ROW (M-LUA.cpp)
#include <time.h>
#include <windows.h>
#include <string>
#include <map>
#include <vector>

extern bool bWelcomeScreen_Window;
extern bool bStoryboardWindow;
extern bool bImGuiInTestGame;
extern bool g_bNoGGUntilGameGuruMainCalled;
extern bool g_bDisableQuitFlag;
extern StoryboardStruct Storyboard;
extern char cDirectOpen[260];
extern int iLaunchAfterSync;
extern int iSkibFramesBeforeLaunch;
extern bool bCloseStoryboardAfterLoad;
extern int iLevelEditorFromStoryboardID;
extern std::vector<std::string> projectbank_list;
extern cstr TriggerLoadGameProject;
extern void GetVramUsageAndBudget ( float* pfUsedMB, float* pfBudgetMB, float* pfSharedUsedMB, float* pfSharedBudgetMB );

static bool s_initialized = false;
static DWORD s_startTick = 0;
static char s_cmdPath[MAX_PATH];
static char s_rspPath[MAX_PATH];
static char s_logPath[MAX_PATH];
static char s_shotPath[MAX_PATH];

static void AutoHarness_InitPaths ( void )
{
	char exePath[MAX_PATH];
	GetModuleFileNameA ( NULL, exePath, MAX_PATH );
	char* lastSlash = strrchr ( exePath, '\\' );
	if ( !lastSlash ) lastSlash = strrchr ( exePath, '/' );
	if ( lastSlash ) lastSlash[1] = 0;
	else strcpy ( exePath, ".\\" );
	_snprintf ( s_cmdPath, MAX_PATH, "%sauto_command.txt", exePath );
	_snprintf ( s_rspPath, MAX_PATH, "%sauto_result.txt", exePath );
	_snprintf ( s_logPath, MAX_PATH, "%sauto_log.txt", exePath );
	_snprintf ( s_shotPath, MAX_PATH, "%sauto_screenshot.png", exePath );
	s_cmdPath[MAX_PATH - 1] = 0;
	s_rspPath[MAX_PATH - 1] = 0;
	s_logPath[MAX_PATH - 1] = 0;
	s_shotPath[MAX_PATH - 1] = 0;
}

static void AutoHarness_Log ( const char* pLabel, const char* pText )
{
	FILE* f = fopen ( s_logPath, "a" );
	if ( !f ) return;
	time_t now = time ( NULL );
	struct tm* lt = localtime ( &now );
	char timebuf[64];
	strftime ( timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", lt );
	if ( strcmp ( pLabel, "COMMAND" ) == 0 )
		fprintf ( f, "[%s] COMMAND: %s\n", timebuf, pText );
	else
		fprintf ( f, "[%s] RESULT:\n%s\n---\n", timebuf, pText );
	fclose ( f );
}

static void AutoHarness_WriteResult ( const char* pResult )
{
	FILE* f = fopen ( s_rspPath, "w" );
	if ( f )
	{
		fputs ( pResult, f );
		fclose ( f );
	}
	AutoHarness_Log ( "RESULT", pResult );
}

static const char* AutoHarness_GetAppState ( void )
{
	if ( !g_bNoGGUntilGameGuruMainCalled ) return "initializing";
	if ( t.game.gameisexe == 1 )
	{
		if ( t.game.titleloop == 1 ) return "standalone_title";
		if ( t.game.gameloop == 1 || t.game.levelloop == 1 ) return "standalone_playing";
		return "standalone_loading";
	}
	if ( bImGuiInTestGame ) return "game";
	if ( bWelcomeScreen_Window ) return "hub";
	if ( bStoryboardWindow ) return "storyboard";
	if ( iLaunchAfterSync >= 200 && iLaunchAfterSync <= 210 ) return "loading";
	return "editor";
}

static const char* AutoHarness_NodeTypeName ( int type )
{
	switch ( type )
	{
		case STORYBOARD_TYPE_NONE: return "none";
		case STORYBOARD_TYPE_SPLASH: return "splash";
		case STORYBOARD_TYPE_SCREEN: return "screen";
		case STORYBOARD_TYPE_LEVEL: return "level";
		case STORYBOARD_TYPE_HUD: return "hud";
	}
	return "unknown";
}

static void Cmd_GetState ( char* result, int resultSize )
{
	int written = _snprintf ( result, resultSize, "STATE: %s\nTAB: none\nERRORS: none\nUPTIME: %lu\n", AutoHarness_GetAppState(), (GetTickCount() - s_startTick) / 1000 );
	if ( written < 0 ) written = 0;
	if ( bStoryboardWindow )
	{
		int count = 0;
		for ( int i = 0; i < STORYBOARD_MAXNODES; i++ ) if ( Storyboard.Nodes[i].used ) count++;
		written += _snprintf ( result + written, resultSize - written, "PROJECT: %s\nNODES(%d):\n", Storyboard.gamename, count );
		for ( int i = 0; i < STORYBOARD_MAXNODES && written < resultSize - 600; i++ )
		{
			if ( !Storyboard.Nodes[i].used ) continue;
			written += _snprintf ( result + written, resultSize - written, "  [%d] type=%s title=\"%s\" level=\"%s\"\n", i, AutoHarness_NodeTypeName ( Storyboard.Nodes[i].type ), Storyboard.Nodes[i].title, Storyboard.Nodes[i].level_name );
		}
	}
	result[resultSize - 1] = 0;
}

static void Cmd_Navigate ( const char* target, char* result, int resultSize )
{
	if ( _stricmp ( target, "hub" ) == 0 || _strnicmp ( target, "hub.", 4 ) == 0 )
	{
		bWelcomeScreen_Window = true;
		bStoryboardWindow = false;
		_snprintf ( result, resultSize, "OK: Navigated to hub" );
	}
	else if ( _stricmp ( target, "storyboard" ) == 0 )
	{
		bStoryboardWindow = true;
		bWelcomeScreen_Window = false;
		_snprintf ( result, resultSize, "OK: Navigated to storyboard" );
	}
	else
		_snprintf ( result, resultSize, "ERROR: Unknown navigation target '%s'", target );
	result[resultSize - 1] = 0;
}

static void Cmd_ListProjects ( char* result, int resultSize )
{
	int written = _snprintf ( result, resultSize, "OK: PROJECTS(%d):\n", (int)projectbank_list.size() );
	for ( size_t i = 0; i < projectbank_list.size() && written < resultSize - 300; i++ )
		written += _snprintf ( result + written, resultSize - written, "  %s\n", projectbank_list[i].c_str() );
	result[resultSize - 1] = 0;
}

static void Cmd_OpenProject ( const char* projectName, char* result, int resultSize )
{
	if ( !bWelcomeScreen_Window )
	{
		_snprintf ( result, resultSize, "ERROR: OPEN_PROJECT must be run from hub (welcome screen not visible)" );
		result[resultSize - 1] = 0;
		return;
	}
	if ( projectbank_list.empty() )
	{
		_snprintf ( result, resultSize, "ERROR: projectbank_list is empty; the hub fills it on its first frame, try GET_STATE once then retry" );
		result[resultSize - 1] = 0;
		return;
	}
	for ( size_t i = 0; i < projectbank_list.size(); i++ )
	{
		if ( _stricmp ( projectbank_list[i].c_str(), projectName ) != 0 ) continue;
		static char s_openProjectName[260];
		strncpy ( s_openProjectName, projectbank_list[i].c_str(), sizeof(s_openProjectName) - 1 );
		s_openProjectName[sizeof(s_openProjectName) - 1] = 0;
		TriggerLoadGameProject = s_openProjectName;
		bWelcomeScreen_Window = false;
		bStoryboardWindow = true;
		_snprintf ( result, resultSize, "OK: Opening project '%s' (index %d of %d)", s_openProjectName, (int)i, (int)projectbank_list.size() );
		result[resultSize - 1] = 0;
		return;
	}
	_snprintf ( result, resultSize, "ERROR: project '%s' not found (%d available)", projectName, (int)projectbank_list.size() );
	result[resultSize - 1] = 0;
}

// loads a level node into the editor as a click on it does; a title of "" takes the first node that has a level file
static void Cmd_ClickNode ( const char* nodeTitle, char* result, int resultSize )
{
	if ( !bStoryboardWindow )
	{
		_snprintf ( result, resultSize, "ERROR: Not in storyboard view, cannot click node" );
		result[resultSize - 1] = 0;
		return;
	}
	for ( int i = 0; i < STORYBOARD_MAXNODES; i++ )
	{
		const StoryboardNodesStruct& n = Storyboard.Nodes[i];
		if ( !n.used || n.type != STORYBOARD_TYPE_LEVEL || strlen ( n.level_name ) == 0 ) continue;
		if ( nodeTitle[0] && _stricmp ( n.title, nodeTitle ) != 0 ) continue;
		strcpy ( cDirectOpen, n.level_name );
		iLaunchAfterSync = 7;
		iSkibFramesBeforeLaunch = 5;
		bCloseStoryboardAfterLoad = true;
		iLevelEditorFromStoryboardID = i;
		_snprintf ( result, resultSize, "OK: Loading level node '%s' (file: %s, index: %d)", n.title, n.level_name, i );
		result[resultSize - 1] = 0;
		return;
	}
	if ( nodeTitle[0] )
		_snprintf ( result, resultSize, "ERROR: Node '%s' not found in storyboard, or it has no level file", nodeTitle );
	else
		_snprintf ( result, resultSize, "ERROR: No level node with an assigned level file found in storyboard" );
	result[resultSize - 1] = 0;
}

// CLICK load_level presses the storyboard's Add Existing Level (its file dialog follows); CLICK test_level runs the level
static void Cmd_Click ( const char* element, char* result, int resultSize )
{
	if ( _stricmp ( element, "load_level" ) == 0 )
	{
		extern int iStoryboardExecuteKey;
		if ( bStoryboardWindow )
		{
			iStoryboardExecuteKey = 'L';
			_snprintf ( result, resultSize, "OK: Triggered Load Level" );
		}
		else
			_snprintf ( result, resultSize, "ERROR: Not in storyboard view" );
	}
	else if ( _stricmp ( element, "test_level" ) == 0 )
	{
		if ( strcmp ( AutoHarness_GetAppState(), "editor" ) == 0 )
		{
			iLaunchAfterSync = 1;
			_snprintf ( result, resultSize, "OK: Triggered Test Level from editor" );
		}
		else
			_snprintf ( result, resultSize, "ERROR: CLICK test_level only works in the level editor (state: %s)", AutoHarness_GetAppState() );
	}
	else
		_snprintf ( result, resultSize, "ERROR: Unknown click element '%s'", element );
	result[resultSize - 1] = 0;
}

static void Cmd_GetCamera ( char* result, int resultSize )
{
	_snprintf ( result, resultSize, "OK: pos=(%.2f,%.2f,%.2f) ang=(%.2f,%.2f) freeflight=%d", t.editorfreeflight.c.x_f, t.editorfreeflight.c.y_f, t.editorfreeflight.c.z_f, t.editorfreeflight.c.angx_f, t.editorfreeflight.c.angy_f, t.editorfreeflight.mode );
	result[resultSize - 1] = 0;
}

// SET_CAMERA x y z [angx angy]: the editor's free flight camera, as its own travel-to writes it
static void Cmd_SetCamera ( const char* arg, char* result, int resultSize )
{
	float x = 0, y = 0, z = 0, ax = 0, ay = 0;
	int got = sscanf_s ( arg, "%f %f %f %f %f", &x, &y, &z, &ax, &ay );
	if ( strcmp ( AutoHarness_GetAppState(), "editor" ) != 0 )
		_snprintf ( result, resultSize, "ERROR: SET_CAMERA only works in the level editor (state: %s)", AutoHarness_GetAppState() );
	else if ( got < 3 )
		_snprintf ( result, resultSize, "ERROR: SET_CAMERA needs <x> <y> <z> [angx angy]" );
	else
	{
		t.editorfreeflight.mode = 1;
		t.editorfreeflight.c.x_f = x;
		t.editorfreeflight.c.y_f = y;
		t.editorfreeflight.c.z_f = z;
		if ( got >= 5 )
		{
			t.editorfreeflight.c.angx_f = ax;
			t.editorfreeflight.c.angy_f = ay;
		}
		_snprintf ( result, resultSize, "OK: SET_CAMERA pos=(%.1f,%.1f,%.1f) ang=(%.1f,%.1f)", x, y, z, t.editorfreeflight.c.angx_f, t.editorfreeflight.c.angy_f );
	}
	result[resultSize - 1] = 0;
}

// WPE_PREVIEW <.pe path relative to Files> [x y z]: what the editor's Preview checkbox does (LoadWPE, then the emitter
// actions 1 burst, 4 restart and 5 visible, handed to RenderPreviewEmitter), at x y z or 250 units ahead of the free
// flight camera, so a screenshot A/B can see it; as the DX12 build's verb. Answers with the sun's direction too
uint32_t WickedCall_LoadWPE ( char* filename );
void WickedCall_PerformEmitterAction ( int iAction, uint32_t emitter_root );
bool WickedCall_ParticleEffectPosition ( uint32_t root, float fX, float fY, float fZ );
static void Cmd_WPEPreview ( const char* arg, char* result, int resultSize )
{
	char pefile[MAX_PATH];
	strncpy ( pefile, arg ? arg : "", sizeof(pefile) - 1 );
	pefile[sizeof(pefile) - 1] = 0;
	// the last three words, when they are numbers, are the position
	float pos[3] = { 0, 0, 0 };
	int got = 0;
	for ( ; got < 3; got++ )
	{
		char* pSpace = strrchr ( pefile, ' ' );
		if ( !pSpace ) break;
		char* pEnd = NULL;
		float v = (float)strtod ( pSpace + 1, &pEnd );
		if ( pEnd == pSpace + 1 || *pEnd != 0 ) break;
		pos[2 - got] = v;
		*pSpace = 0;
	}
	if ( got != 3 )
	{
		float fAngX = GGToRadian ( t.editorfreeflight.c.angx_f ), fAngY = GGToRadian ( t.editorfreeflight.c.angy_f );
		pos[0] = t.editorfreeflight.c.x_f + sinf ( fAngY ) * cosf ( fAngX ) * 250.0f;
		pos[1] = t.editorfreeflight.c.y_f - sinf ( fAngX ) * 250.0f;
		pos[2] = t.editorfreeflight.c.z_f + cosf ( fAngY ) * cosf ( fAngX ) * 250.0f;
	}
	uint32_t root = WickedCall_LoadWPE ( pefile );
	if ( root == 0 )
	{
		_snprintf ( result, resultSize, "FAIL: LoadWPE returned 0 for '%s'", pefile );
	}
	else
	{
		WickedCall_PerformEmitterAction ( 1, root );
		WickedCall_PerformEmitterAction ( 4, root );
		WickedCall_PerformEmitterAction ( 5, root );
		extern uint32_t PreviewWPERoot;
		extern bool bPreviewWPE;
		PreviewWPERoot = root;
		bPreviewWPE = true;
		WickedCall_ParticleEffectPosition ( root, pos[0], pos[1], pos[2] );
		// and the sun's direction (towards it), so a check can look through the effect into the sun
		XMFLOAT3 sun = XMFLOAT3 ( 0, 0, 0 );
		wiScene::Scene& scene = wiScene::GetScene();
		for ( size_t i = 0; i < scene.lights.GetCount(); i++ )
		{
			if ( scene.lights[i].GetType() == wiScene::LightComponent::DIRECTIONAL ) { sun = scene.lights[i].direction; break; }
		}
		_snprintf ( result, resultSize, "OK: root=%u handed to RenderPreviewEmitter, placed at (%.1f,%.1f,%.1f) sun (%.3f,%.3f,%.3f)", root, pos[0], pos[1], pos[2], sun.x, sun.y, sun.z );
	}
	result[resultSize - 1] = 0;
}

namespace wiGraphics { namespace DX11_Internal { void GG_DumpVRAMCensus ( const char* path ); } }
static void Cmd_DumpVRAM ( const char* arg, char* result, int resultSize )
{
	// DUMP_VRAM [tag]: every live texture and buffer with its estimated bytes, dimensions, format and flags and its
	// debug name, largest first, with the driver's usage and budget; written to Files\vram_census[_tag].txt, the
	// same file the DX12 engine's DUMP_VRAM writes, for the two builds to be compared line for line
	char vcPath[MAX_PATH];
	if ( arg && arg[0] )
	{
		char vcTag[64]; _snprintf ( vcTag, sizeof(vcTag), "%s", arg ); vcTag[sizeof(vcTag) - 1] = 0;
		for ( char* p = vcTag; *p; ++p ) { if ( *p == ' ' || *p == '\\' || *p == '/' || *p == ':' ) *p = '_'; }
		_snprintf ( vcPath, sizeof(vcPath), "vram_census_%s.txt", vcTag );
	}
	else
	{
		_snprintf ( vcPath, sizeof(vcPath), "vram_census.txt" );
	}
	vcPath[sizeof(vcPath) - 1] = 0;
	wiGraphics::DX11_Internal::GG_DumpVRAMCensus ( vcPath );
	_snprintf ( result, resultSize, "OK: DUMP_VRAM written to %s (game CWD = Files dir)", vcPath );
	result[resultSize - 1] = 0;
}

static void Cmd_GetPerfData ( char* result, int resultSize )
{
	float fps = ImGui::GetCurrentContext() ? ImGui::GetIO().Framerate : 0.0f;
	float fUsedMB = 0, fBudgetMB = 0, fSharedUsedMB = 0, fSharedBudgetMB = 0;
	GetVramUsageAndBudget ( &fUsedMB, &fBudgetMB, &fSharedUsedMB, &fSharedBudgetMB );
	int written = _snprintf ( result, resultSize, "STATE: %s\nFPS: %.1f\nFRAME_TIME_MS: %.2f\nVRAM_MB: %.1f\nVRAM_BUDGET_MB: %.1f\nVRAM_SHARED_MB: %.1f\nPOLYS: %d\nDRAW_CALLS: %d\n",
		AutoHarness_GetAppState(), fps, fps > 0.0f ? 1000.0f / fps : 0.0f, fUsedMB, fBudgetMB, fSharedUsedMB, (int)wiProfiler::GetPolygons(), (int)wiProfiler::GetDrawCalls() );
	// GG: the scene's component counts, as the DX12 build's (clone_memory_check.py reads them)
	wiScene::Scene& scene = wiScene::GetScene();
	if ( written > 0 && written < resultSize - 256 )
		_snprintf ( result + written, resultSize - written, "SCENE_OBJECTS: %d\nSCENE_MESHES: %d\nSCENE_MATERIALS: %d\nSCENE_ARMATURES: %d\nSCENE_ANIMATIONS: %d\n"
			"SCENE_TRANSFORMS: %d\nSCENE_HIERARCHY: %d\nSCENE_EMITTERS: %d\nSCENE_SOUNDS: %d\n",
			(int)scene.objects.GetCount(), (int)scene.meshes.GetCount(), (int)scene.materials.GetCount(), (int)scene.armatures.GetCount(), (int)scene.animations.GetCount(),
			(int)scene.transforms.GetCount(), (int)scene.hierarchy.GetCount(), (int)scene.emitters.GetCount(), (int)scene.sounds.GetCount() );
	result[resultSize - 1] = 0;
}

static void Cmd_Screenshot ( char* result, int resultSize )
{
	wiHelper::screenshot ( master.swapChain, s_shotPath );
	_snprintf ( result, resultSize, "OK: Screenshot saved to %s", s_shotPath );
	result[resultSize - 1] = 0;
}

static void Cmd_Quit ( char* result, int resultSize )
{
	_snprintf ( result, resultSize, "OK: Quitting application" );
	result[resultSize - 1] = 0;
	AutoHarness_WriteResult ( result );
	if ( g_pGlob && g_pGlob->hWnd )
	{
		g_bDisableQuitFlag = false;
		PostMessage ( g_pGlob->hWnd, WM_CLOSE, 0, 0 );
	}
}

void AutoHarness_CheckForCommand ( void )
{
	if ( !s_initialized )
	{
		AutoHarness_InitPaths();
		s_startTick = GetTickCount();
		s_initialized = true;
	}
	if ( GetFileAttributesA ( s_cmdPath ) == INVALID_FILE_ATTRIBUTES ) return;
	char cmdBuf[1024];
	cmdBuf[0] = 0;
	FILE* f = fopen ( s_cmdPath, "r" );
	if ( !f ) return;
	if ( !fgets ( cmdBuf, sizeof(cmdBuf), f ) ) cmdBuf[0] = 0;
	fclose ( f );
	int len = (int)strlen ( cmdBuf );
	while ( len > 0 && (cmdBuf[len - 1] == '\n' || cmdBuf[len - 1] == '\r') ) cmdBuf[--len] = 0;
	if ( len == 0 ) return; // the writer may still be flushing
	DeleteFileA ( s_cmdPath );
	AutoHarness_Log ( "COMMAND", cmdBuf );

	char cmd[256];
	char arg[768];
	cmd[0] = 0;
	arg[0] = 0;
	char* space = strchr ( cmdBuf, ' ' );
	if ( space )
	{
		int cmdLen = (int)(space - cmdBuf);
		if ( cmdLen >= (int)sizeof(cmd) ) cmdLen = (int)sizeof(cmd) - 1;
		memcpy ( cmd, cmdBuf, cmdLen );
		cmd[cmdLen] = 0;
		const char* a = space + 1;
		while ( *a == ' ' ) a++;
		strncpy ( arg, a, sizeof(arg) - 1 );
		arg[sizeof(arg) - 1] = 0;
	}
	else
	{
		strncpy ( cmd, cmdBuf, sizeof(cmd) - 1 );
		cmd[sizeof(cmd) - 1] = 0;
	}

	static char result[32768];
	const int resultSize = (int)sizeof(result); // GG: the size the verbs ported from the DX12 build's harness write to
	result[0] = 0;
	if ( _stricmp ( cmd, "GET_STATE" ) == 0 ) Cmd_GetState ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "NAVIGATE" ) == 0 ) Cmd_Navigate ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "LIST_PROJECTS" ) == 0 ) Cmd_ListProjects ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "OPEN_PROJECT" ) == 0 ) Cmd_OpenProject ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "CLICK_NODE" ) == 0 ) Cmd_ClickNode ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "CLICK_ONLY_LEVEL" ) == 0 ) Cmd_ClickNode ( "", result, sizeof(result) );
	else if ( _stricmp ( cmd, "CLICK" ) == 0 ) Cmd_Click ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "GET_CAMERA" ) == 0 ) Cmd_GetCamera ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "SET_CAMERA" ) == 0 ) Cmd_SetCamera ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "WPE_PREVIEW" ) == 0 ) Cmd_WPEPreview ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "RUN_LUA" ) == 0 )
	{
		// RUN_LUA <chunk>: run in the game's Lua state, "return ..." to read values back (as the DX12 build's verb)
		extern int RunLuaString ( char* pCode, char* pResultBuf, int iResultSize );
		static char luaResult[8192];
		int rc = RunLuaString ( arg, luaResult, sizeof(luaResult) );
		_snprintf ( result, sizeof(result), "%s: %s", ( rc == 0 ) ? "OK" : "FAIL", luaResult[0] ? luaResult : "(no result)" );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "TEST_ENVPROBE" ) == 0 )
	{
		// TEST_ENVPROBE x y z size: a reflection probe placed as a probe light marker places it (a box of half size each way),
		// for testing the probes in a level with none; it lasts until the probe list is made again
		float x = 0, y = 0, z = 0, s = 500;
		int n = sscanf ( arg, "%f %f %f %f", &x, &y, &z, &s );
		if ( n >= 3 ) GGTerrain::GGTerrain_AddEnvProbeList ( x, y, z, s, 0, 0, 0, 1, s, s, s, 1.0f );
		_snprintf ( result, sizeof(result), n >= 3 ? "OK: TEST_ENVPROBE %.0f %.0f %.0f %.0f" : "ERROR: TEST_ENVPROBE x y z [size]", x, y, z, s );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_PARTICLESCATTER" ) == 0 )
	{
		// SET_PARTICLESCATTER <strength>: the sun's forward scattering on lit particle layers (as SetParticleScatter), 0 off
		extern void WickedCall_SetParticleScatter(float fStrength);
		extern float WickedCall_GetParticleScatter(void);
		WickedCall_SetParticleScatter((float)atof(arg));
		_snprintf(result, sizeof(result), "OK: SET_PARTICLESCATTER %.3f", WickedCall_GetParticleScatter());
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "DUMP_DISTANTLIGHTS" ) == 0 )
	{
		// DUMP_DISTANTLIGHTS: the distant lights: lights listed and drawn, tags tied to an object and their lights drawn, where the first were placed
		extern void DistantLights_GetDebug(char* pOut, size_t size);
		char text[1024];
		DistantLights_GetDebug(text, sizeof(text));
		_snprintf(result, sizeof(result), "OK: DUMP_DISTANTLIGHTS %s", text);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_SUN" ) == 0 )
	{
		// SET_SUN r g b intensity: the sun light's colour (0-255, as the visuals) and intensity set directly, the level's
		// visuals untouched (the next visuals push puts them back); for the sun flare's colour on a coloured sun (as the
		// DX12 build's verb)
		extern void WickedCall_SetSunColors(float fRed, float fGreen, float fBlue, float fEnergy, float fFov, float fShadowBias);
		float r = 255, g = 255, b = 255, e = t.visuals.SunIntensity_f;
		int got = sscanf_s(arg, "%f %f %f %f", &r, &g, &b, &e);
		if ( got >= 3 ) WickedCall_SetSunColors(r / 255.0f, g / 255.0f, b / 255.0f, e, 1.0f, t.visuals.fSunShadowBias);
		_snprintf(result, sizeof(result), "OK: SET_SUN rgb=(%.0f,%.0f,%.0f) intensity=%.3f (level %.0f,%.0f,%.0f at %.3f)", r, g, b, e,
			t.visuals.SunRed_f, t.visuals.SunGreen_f, t.visuals.SunBlue_f, t.visuals.SunIntensity_f);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_EDITORDOTS" ) == 0 )
	{
		// SET_EDITORDOTS [0|1 | keep 0|1]: the editor's entity dots (a hidden sphere per dynamic entity, its object number less
		// 1,000,000) deleted (0, their numbers kept) or made again as the editor makes them (1), for a cost A/B in one game;
		// keep 1 makes and leaves them in a test game as before, keep 0 (the default) doesn't; no argument answers how many
		// exist, how many are kept to remake, and the keep setting. As the DX12 build's verb
		static std::vector<int> s_dotsDeleted;
		extern void CreateDotObject(int obj);
		extern void PositionDotObject(int obj);
		extern int g_iEditorDotsInGame;
		extern int editor_deleteentitydots(void);
		int iOn = -1, iKeep = -1;
		if ( arg && _strnicmp(arg, "keep", 4) == 0 ) sscanf_s(arg + 4, "%d", &iKeep);
		else if ( arg ) sscanf_s(arg, "%d", &iOn);
		if ( iKeep == 0 || iKeep == 1 ) g_iEditorDotsInGame = iKeep;
		int done = 0;
		LARGE_INTEGER liFreq, liStart, liEnd;
		QueryPerformanceFrequency(&liFreq);
		QueryPerformanceCounter(&liStart);
		if ( iOn == 0 )
		{
			for ( int o = ENTITYDOTOBJECTFIRST; o <= ENTITYDOTOBJECTLAST; o++ )
				if ( ObjectExist(o) == 1 ) s_dotsDeleted.push_back(o);
			done = editor_deleteentitydots();
		}
		else if ( iOn == 1 )
		{
			for ( int o : s_dotsDeleted )
			{
				if ( ObjectExist(o) == 0 && ObjectExist(o - (ENTITYDOTOBJECTFIRST - ENTITYOBJECTBASE)) == 1 )
				{
					CreateDotObject(o); PositionDotObject(o); HideObject(o); done++;
				}
			}
			s_dotsDeleted.clear();
		}
		QueryPerformanceCounter(&liEnd);
		double ms = (liEnd.QuadPart - liStart.QuadPart) * 1000.0 / (double)liFreq.QuadPart;
		int count = 0;
		for ( int o = ENTITYDOTOBJECTFIRST; o <= ENTITYDOTOBJECTLAST; o++ ) if ( ObjectExist(o) == 1 ) count++;
		_snprintf(result, sizeof(result), "OK: SET_EDITORDOTS done=%d ms=%.0f dots=%d kept=%d keep=%d objects=%d", done, ms, count,
			(int)s_dotsDeleted.size(), g_iEditorDotsInGame, (int)wiScene::GetScene().objects.GetCount());
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_LIGHTSHAFTS" ) == 0 )
	{
		// SET_LIGHTSHAFTS [strength length]: as SetLightShaftStrength (a value below 0 keeps it, no clamp); no argument answers,
		// with whether the shafts are on. As the DX12 build's verb
		extern float gg_lightshaft_gain, gg_lightshaft_length;
		extern bool GetLightShaftState(void);
		float v[2] = { -1, -1 };
		if ( arg ) sscanf_s(arg, "%f %f", &v[0], &v[1]);
		if ( v[0] >= 0 ) gg_lightshaft_gain = v[0];
		if ( v[1] >= 0 ) gg_lightshaft_length = v[1];
		_snprintf(result, sizeof(result), "OK: SET_LIGHTSHAFTS strength=%.2f length=%.2f on=%d exposure=%.3f", gg_lightshaft_gain,
			gg_lightshaft_length, GetLightShaftState() ? 1 : 0, 0.2f);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SUN_DISC" ) == 0 )
	{
		// SUN_DISC [gain [r g b]]: the sun's disc alone (Tab Tab's Sun Disc, Lua SetSunDisc); with values set as the slider
		// sets them (the level's, in t.visuals and t.gamevisuals, saved with it); answers the level's, the game's copy, what
		// the renderer draws, and the sun's direction (towards it, to aim a camera at the disc). As the DX12 build's verb
		extern void WickedCall_SetSunDisc(float fGain, float fRed, float fGreen, float fBlue);
		float gain = -1, red = -1, green = -1, blue = -1;
		int got = sscanf_s(arg, "%f %f %f %f", &gain, &red, &green, &blue);
		if ( got >= 1 && gain >= 0 )
		{
			t.gamevisuals.fSunDiscGain = t.visuals.fSunDiscGain = gain;
			if ( got >= 4 )
			{
				t.gamevisuals.fSunDiscRed = t.visuals.fSunDiscRed = red;
				t.gamevisuals.fSunDiscGreen = t.visuals.fSunDiscGreen = green;
				t.gamevisuals.fSunDiscBlue = t.visuals.fSunDiscBlue = blue;
			}
			WickedCall_SetSunDisc(t.visuals.fSunDiscGain, t.visuals.fSunDiscRed / 255.0f, t.visuals.fSunDiscGreen / 255.0f, t.visuals.fSunDiscBlue / 255.0f);
			g.projectmodified = 1;
		}
		float dg = 0, dr = 0, dgr = 0, db = 0;
		wiRenderer::GetSunDisc(dg, dr, dgr, db);
		const XMFLOAT3 sun = wiScene::GetScene().weather.sunDirection;
		_snprintf(result, sizeof(result), "OK: SUN_DISC level=%.3f,%.0f,%.0f,%.0f game=%.3f,%.0f,%.0f,%.0f drawn=%.3f,%.3f,%.3f,%.3f sundir=%.4f,%.4f,%.4f",
			t.visuals.fSunDiscGain, t.visuals.fSunDiscRed, t.visuals.fSunDiscGreen, t.visuals.fSunDiscBlue,
			t.gamevisuals.fSunDiscGain, t.gamevisuals.fSunDiscRed, t.gamevisuals.fSunDiscGreen, t.gamevisuals.fSunDiscBlue,
			dg, dr, dgr, db, sun.x, sun.y, sun.z);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_SCREENFLARE" ) == 0 || _stricmp ( cmd, "SET_SUNFLARE" ) == 0 )
	{
		// SET_SCREENFLARE [enabled intensity threshold spacing halo nosky cap followbloom] and SET_SUNFLARE [enabled hue
		// brightness]: the screen-space lens flare and the sun's flare set on the renderer directly, the level's visuals
		// untouched (the next visuals push puts them back); a value below 0 keeps the current one, no arguments reads them
		// (as the DX12 build's verbs)
		extern MasterRenderer* master_renderer;
		if ( !master_renderer )
		{
			_snprintf(result, sizeof(result), "ERROR: no master_renderer");
		}
		else if ( _stricmp ( cmd, "SET_SCREENFLARE" ) == 0 )
		{
			float v[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
			sscanf_s(arg, "%f %f %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
			if (v[0] >= 0) master_renderer->setScreenLensFlareEnabled(v[0] != 0);
			if (v[1] >= 0) master_renderer->setScreenLensFlareIntensity(v[1]);
			if (v[2] >= 0) master_renderer->setScreenLensFlareThreshold(v[2]);
			if (v[3] >= 0) master_renderer->setScreenLensFlareSpacing(v[3]);
			if (v[4] >= 0) master_renderer->setScreenLensFlareHalo(v[4]);
			if (v[5] >= 0) master_renderer->setScreenLensFlareNoSky(v[5] != 0);
			if (v[6] >= 0) master_renderer->setScreenLensFlareCap(v[6]);
			if (v[7] >= 0) master_renderer->setScreenLensFlareFollowBloom(v[7] != 0);
			_snprintf(result, sizeof(result), "OK: SET_SCREENFLARE enabled=%d intensity=%.3f threshold=%.3f spacing=%.3f halo=%.3f nosky=%d cap=%.1f followbloom=%d",
				master_renderer->getScreenLensFlareEnabled() ? 1 : 0, master_renderer->getScreenLensFlareIntensity(), master_renderer->getScreenLensFlareThreshold(),
				master_renderer->getScreenLensFlareSpacing(), master_renderer->getScreenLensFlareHalo(), master_renderer->getScreenLensFlareNoSky() ? 1 : 0,
				master_renderer->getScreenLensFlareCap(), master_renderer->getScreenLensFlareFollowBloom() ? 1 : 0);
		}
		else
		{
			float v[3] = { -1, -1, -1 };
			sscanf_s(arg, "%f %f %f", &v[0], &v[1], &v[2]);
			if (v[0] >= 0) master_renderer->setLensFlareEnabled(v[0] != 0);
			if (v[1] >= 0 || v[2] >= 0)
				wiRenderer::SetLensFlareLightColour(v[1] >= 0 ? v[1] : wiRenderer::GetLensFlareLightHue(), v[2] >= 0 ? v[2] : wiRenderer::GetLensFlareLightBrightness());
			_snprintf(result, sizeof(result), "OK: SET_SUNFLARE enabled=%d hue=%.3f brightness=%.3f",
				master_renderer->getLensFlareEnabled() ? 1 : 0, wiRenderer::GetLensFlareLightHue(), wiRenderer::GetLensFlareLightBrightness());
		}
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_BLOOM" ) == 0 )
	{
		// SET_BLOOM [enabled strength threshold radius cap]: the bloom set on the renderer directly, the level's visuals
		// untouched (the next visuals push puts them back); a value below 0 keeps the current one, no arguments reads
		// them (as the DX12 build's verb, whose sixth value, the bloom before the exposure, this build always does)
		extern MasterRenderer* master_renderer;
		if ( !master_renderer )
		{
			_snprintf(result, sizeof(result), "ERROR: no master_renderer");
		}
		else
		{
			float v[5] = { -1, -1, -1, -1, -1 };
			sscanf_s(arg, "%f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4]);
			if (v[0] >= 0) master_renderer->setBloomEnabled(v[0] != 0);
			if (v[1] >= 0) master_renderer->setBloomStrength(v[1]);
			if (v[2] >= 0) master_renderer->setBloomThreshold(v[2]);
			if (v[3] >= 0) master_renderer->setBloomRadius(v[3]);
			if (v[4] >= 0) master_renderer->setBloomCap(v[4]);
			_snprintf(result, sizeof(result), "OK: SET_BLOOM enabled=%d strength=%.3f threshold=%.3f radius=%.3f cap=%.1f beforeexposure=1 exposure=%.4f eyeadaption=%d",
				master_renderer->getBloomEnabled() ? 1 : 0, master_renderer->getBloomStrength(), master_renderer->getBloomThreshold(),
				master_renderer->getBloomRadius(), master_renderer->getBloomCap(), master_renderer->getExposure(),
				master_renderer->getEyeAdaptionEnabled() ? 1 : 0);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_SHADOWMINTEXELS" ) == 0 )
	{
		// SET_SHADOWMINTEXELS [n]: sun shadow casters smaller than n of a cascade's shadow map texels left out of it (2, the
		// default; 0 keeps every caster, as stock); answers with the object-cascade pairs left out last frame
		extern float gg_shadow_min_texels;
		extern unsigned int gg_shadow_texel_skipped;
		if ( arg && *arg ) gg_shadow_min_texels = (float)atof(arg);
		_snprintf(result, sizeof(result), "OK: SET_SHADOWMINTEXELS %.2f - left out last frame %u", gg_shadow_min_texels, gg_shadow_texel_skipped);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_LUAQUIET" ) == 0 )
	{
		// SET_LUAQUIET [0|1]: lua_loop_allentities skips quiet turns (1, the default: statics with no script, and entities with
		// no script whose Lua row already shows everything) or walks every entity as before (0); answers with the walk's stats
		if ( arg && *arg ) g_iLuaQuietSkip = atoi(arg) ? 1 : 0;
		lua_loopstats_text(result, sizeof(result), false);
	}
	else if ( _stricmp ( cmd, "SET_LUAFARROWS" ) == 0 )
	{
		// SET_LUAFARROWS [0|1]: an entity beyond its logic range is given its Lua row the frame it moves or its state
		// changes (1, the default) or only on a hit or a kill, as before (0); answers with the walk stats
		if ( arg && *arg ) g_iLuaFarRows = atoi(arg) ? 1 : 0;
		lua_loopstats_text(result, sizeof(result), false);
	}
	else if ( _stricmp ( cmd, "LUA_ROW" ) == 0 )
	{
		// LUA_ROW <e>: an entity's object position, element position, health, active, static and pending flags, beside what
		// its Lua row was last given (UpdateEntity / UpdateEntityRT); for checking a skipped entity's row is current
		lua_row_text(arg ? atoi(arg) : 0, result, sizeof(result));
	}
	else if ( _stricmp ( cmd, "LUA_LOOP_STATS" ) == 0 )
	{
		// LUA_LOOP_STATS [reset]: the entity walk per frame since the last reset: its time (mean, max), turns taken, statics and
		// quiet entities skipped, rows given (UpdateEntityRT) and main calls; reset starts a new window. LUA_LOOP_STATS split 0|1:
		// each element's visit timed by how it ended (left at the gate, a static skipped, a quiet turn skipped, a turn taken),
		// off by default
		if ( arg && _strnicmp ( arg, "split", 5 ) == 0 ) g_iLuaWalkSplit = atoi ( arg + 5 ) ? 1 : 0;
		lua_loopstats_text(result, sizeof(result), arg && _stricmp(arg, "reset") == 0);
	}
	else if ( _stricmp ( cmd, "SET_LUAPARK" ) == 0 || _stricmp ( cmd, "LUA_PARK" ) == 0 )
	{
		// GG: SET_LUAPARK [0|1|2]: the Lua walk parks nothing (0), the statics (1) or the statics and the quiet entities (2, the
		// default), passing them by until woken; answers with LUA_LOOP_STATS. LUA_PARK <e>: an element's parked state and last wake
		extern int g_iLuaWalkPark; void lua_park_text ( int e, char* pOut, int iSize );
		if ( _stricmp ( cmd, "LUA_PARK" ) == 0 ) lua_park_text ( arg ? atoi ( arg ) : 0, result, sizeof(result) );
		else
		{
			if ( arg && *arg ) g_iLuaWalkPark = atoi ( arg ) < 0 ? 0 : ( atoi ( arg ) > 2 ? 2 : atoi ( arg ) );
			lua_loopstats_text ( result, sizeof(result), false );
		}
	}
	else if ( _stricmp ( cmd, "SET_GUNSMOKENOKEY" ) == 0 )
	{
		// SET_GUNSMOKENOKEY [0|1]: a gunspec naming a smoke decal with no smoke key has smoke (1, the default) or none (0, as
		// before, GameGuruRepo #6445); read when a gun loads, so set it before Test Level
		if ( arg && *arg ) g_iGunSmokeNoKeyOn = atoi(arg) ? 1 : 0;
		_snprintf(result, sizeof(result), "OK: SET_GUNSMOKENOKEY %d", g_iGunSmokeNoKeyOn);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "GUNSMOKE_STATE" ) == 0 )
	{
		// GUNSMOKE_STATE: every loaded gun's smoke key, smoke decal and smoke image for both fire modes (smokeimg 0: no puffs),
		// the current gun, and the puffs shown since the start
		int w = _snprintf(result, sizeof(result), "OK: puffs=%d current=%d nokey=%d", g_iGunSmokePuffs, t.gunid, g_iGunSmokeNoKeyOn);
		for ( int gi = 1; gi <= g.gunmax && w < (int)sizeof(result) - 200; gi++ )
		{
			if ( strlen(t.gun[gi].name_s.Get()) < 1 ) continue;
			w += _snprintf(result + w, sizeof(result) - w, "\n%d %s", gi, t.gun[gi].name_s.Get());
			for ( int fm = 0; fm <= 1; fm++ )
				w += _snprintf(result + w, sizeof(result) - w, " | m%d smoke=%d decal=%s img=%d", fm, g.firemodes[gi][fm].settings.smoke,
					g.firemodes[gi][fm].settings.smokedecal_s.Get(), g.firemodes[gi][fm].settings.smokeimg);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_TREELOCALSHADOWS" ) == 0 )
	{
		// SET_TREELOCALSHADOWS [0|1 [lod]]: the full detail trees cast into the spot and point lights' shadows (1, the default)
		// or the sun's only (0, as before), from mesh LOD lod (0 the full mesh, 1 or 2 a type's lighter shadow mesh); no
		// argument reads it. Answers with the light views drawn into, draws and trees since the start
		if ( arg && *arg )
		{
			int on = -1, lod = -1;
			sscanf_s(arg, "%d %d", &on, &lod);
			if ( on >= 0 ) GGTrees::gg_tree_local_shadows = on ? 1 : 0;
			if ( lod >= 0 ) GGTrees::gg_tree_local_shadow_lod = lod > 2 ? 2 : lod;
		}
		_snprintf(result, sizeof(result), "OK: SET_TREELOCALSHADOWS %d lod=%d views=%u draws=%u trees=%u",
			GGTrees::gg_tree_local_shadows, GGTrees::gg_tree_local_shadow_lod, GGTrees::gg_tree_local_shadow_counts[0],
			GGTrees::gg_tree_local_shadow_counts[1], GGTrees::gg_tree_local_shadow_counts[2]);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "TEXSCROLL_STEP" ) == 0 )
	{
		// TEXSCROLL_STEP <seconds>: the textures scripts scroll (SetEntityTextureScroll, SetEntityLimbTextureScroll) moved on
		// by that many seconds now, as a game frame moves them; a paused game (a frozen test view) moves none of them
		float fSeconds = ( arg && *arg ) ? (float)atof(arg) : 0.0f;
		extern void WickedCall_UpdateLuaTextureScroll(float fSeconds);
		WickedCall_UpdateLuaTextureScroll(fSeconds);
		_snprintf(result, sizeof(result), "OK: TEXSCROLL_STEP %.3f", fSeconds);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "DUMP_METALS" ) == 0 )
	{
		// DUMP_METALS: every material in the scene, its metalness, roughness, whether its surface map loaded and the colour and
		// surface maps' names, to metals_dump.txt beside the exe; answers how many are metal with no surface map to scale it
		char pPath[MAX_PATH];
		GetModuleFileNameA ( NULL, pPath, MAX_PATH );
		char* pSlash = strrchr ( pPath, '\\' );
		if ( pSlash ) strcpy ( pSlash + 1, "metals_dump.txt" );
		FILE* f = fopen ( pPath, "w" );
		wiScene::Scene& scene = wiScene::GetScene();
		int iMetalNoMap = 0;
		for ( size_t i = 0; i < scene.materials.GetCount(); i++ )
		{
			wiScene::MaterialComponent& m = scene.materials[i];
			bool bMap = ( m.textures[wiScene::MaterialComponent::SURFACEMAP].resource != nullptr );
			if ( m.metalness > 0.5f && !bMap ) iMetalNoMap++;
			if ( f ) fprintf ( f, "%.2f %.2f %d | %s | %s\n", m.metalness, m.roughness, bMap ? 1 : 0, m.textures[wiScene::MaterialComponent::BASECOLORMAP].name.c_str(), m.textures[wiScene::MaterialComponent::SURFACEMAP].name.c_str() );
		}
		if ( f ) fclose ( f );
		_snprintf ( result, sizeof(result), "OK: DUMP_METALS %d materials, %d metal with no surface map", (int)scene.materials.GetCount(), iMetalNoMap );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "TRACER_TEST" ) == 0 )
	{
		// TRACER_TEST [mps [active]]: with arguments every loaded gun's tracermps (and traceractive) as the gunspec keys set them;
		// answers the held gun's settings, the last ray and the last streak added
		if ( arg && *arg )
		{
			float fMps = 0; int iActive = -1;
			sscanf ( arg, "%f %d", &fMps, &iActive );
			for ( int iGun = 1; iGun <= g.gunmax; iGun++ )
			{
				t.gun[iGun].settings.tracer_mps = fMps;
				if ( iActive >= 0 ) t.gun[iGun].settings.tracer_active = ( iActive != 0 );
			}
		}
		char pLast[256]; pLast[0] = 0;
		Tracers::DebugLastAdded ( pLast, sizeof(pLast) );
		_snprintf ( result, sizeof(result), "OK: TRACER_TEST gun=%d '%s' active=%d mps=%.0f lifetime=%.2f maxlength=%.1f zoomheld=%d ray=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f) %s",
			t.gunid, t.gun_s.Get(), t.gun[t.gunid].settings.tracer_active ? 1 : 0, t.gun[t.gunid].settings.tracer_mps, t.gun[t.gunid].settings.tracer_lifetime, t.gun[t.gunid].settings.tracer_maxlength,
			g.gdisablerightmousehold, t.brayx1_f, t.brayy1_f, t.brayz1_f, t.brayx2_f, t.brayy2_f, t.brayz2_f, pLast );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "TEST_DECAL" ) == 0 )
	{
		// TEST_DECAL kind x z size [image]: a projected decal (kind 0, size wide, onto the ground) or a blast decal (kind 1,
		// radius size, its centre a quarter of it above the ground, the world's axes, no setup rays) on the ground at x, z,
		// through the engine calls AddProjectedDecal and AddBlastDecal use, so the editor's camera can look at one;
		// TEST_DECAL clear removes them all (as the DX12 build's verb)
		extern void WickedCall_RemoveProjectedDecal ( int iID );
		extern int WickedCall_AddProjectedDecal ( LPSTR pImage, float fX, float fY, float fZ, float fSize, float fNX, float fNY, float fNZ, float fSpinDegrees, float fDepth, float fLife );
		extern int WickedCall_AddBlastDecal ( LPSTR pImage, float fX, float fY, float fZ, float fRadius, const float* pAxes, uint32_t iOcclusion, int iHitDirection, float fHitClip, float fLife, float fGroundX, float fGroundY, float fGroundZ );
		if ( arg && _strnicmp ( arg, "clear", 5 ) == 0 )
		{
			WickedCall_RemoveProjectedDecal ( -1 );
			_snprintf ( result, sizeof(result), "OK: TEST_DECAL cleared" );
		}
		else
		{
			int kind = 0; float x = 0, z = 0, size = 200; char image[MAX_PATH] = "entitybank\\User\\Mercs2\\decals\\explosion_decal_1.dds";
			int got = sscanf ( arg ? arg : "", "%d %f %f %f %259s", &kind, &x, &z, &size, image );
			if ( got < 4 )
			{
				_snprintf ( result, sizeof(result), "FAIL: TEST_DECAL kind x z size [image] | clear" );
			}
			else
			{
				float y = 0;
				GGTerrain::GGTerrain_GetHeight ( x, z, &y, 1, 1 );
				int id = 0;
				if ( kind == 0 )
				{
					id = WickedCall_AddProjectedDecal ( image, x, y, z, size, 0, 1, 0, 0, 0, -1 );
				}
				else
				{
					const float axes[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
					id = WickedCall_AddBlastDecal ( image, x, y + size * 0.25f, z, size, axes, ~0u, -1, 0, -1, x, y, z );
				}
				_snprintf ( result, sizeof(result), "OK: TEST_DECAL id=%d kind=%d at (%.0f,%.0f,%.0f) size %.0f", id, kind, x, y, z, size );
			}
		}
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "TEST_DECALEXPLODE" ) == 0 )
	{
		// TEST_DECALEXPLODE <decal name>[|distance[|standon]]: a decal's effect made as a projectile's custom explosion makes it
		// (weapon_projectileresult_make: decal_find, decalelement_create at range 10000, then decalelement_standon when the
		// projectile's file has wpeexplosionnormal), distance ahead of the camera (400), stood on an upward normal with
		// standon 1; reports the decal's state (as the DX12 build's verb)
		extern void decal_find ( void );
		extern void decalelement_create ( void );
		extern void decalelement_standon ( int d, float fNX, float fNY, float fNZ );
		char buf[512];
		strncpy ( buf, arg ? arg : "", sizeof(buf) - 1 ); buf[sizeof(buf) - 1] = 0;
		char* context = NULL;
		char* pName = strtok_s ( buf, "|", &context );
		char* pDist = strtok_s ( NULL, "|", &context );
		char* pStand = strtok_s ( NULL, "|", &context );
		const float fDist = pDist ? (float)atof ( pDist ) : 400.0f;
		const int iStand = pStand ? atoi ( pStand ) : 0;
		t.decal_s = pName ? pName : "";
		decal_find ( );
		const int id = t.decalid;
		if ( id <= 0 || id > g.decalmax )
		{
			_snprintf ( result, sizeof(result), "ERROR: TEST_DECALEXPLODE: no decal '%s' (%d decals)", pName ? pName : "", (int)g.decalmax );
		}
		else
		{
			const wiScene::CameraComponent& cam = wiScene::GetCamera ( );
			const int iOldRange = g.decalrange;
			g.decalrange = 10000;
			g.decalx = cam.Eye.x + cam.At.x * fDist;
			g.decaly = cam.Eye.y + cam.At.y * fDist;
			g.decalz = cam.Eye.z + cam.At.z * fDist;
			t.decalorient = 0;
			t.decalid = id;
			t.currentdecald = 0;
			decalelement_create ( );
			g.decalrange = iOldRange;
			const int iElement = t.currentdecald;
			if ( iStand && iElement > 0 ) decalelement_standon ( iElement, 0, 1, 0 );
			_snprintf ( result, sizeof(result),
				"OK: TEST_DECALEXPLODE '%s' id=%d active=%d bWPE=%d emitterid=%d element=%d at (%.0f,%.0f,%.0f) standon=%d",
				t.decal[id].name_s.Get ( ), id, t.decal[id].active, t.decal[id].newparticle.bWPE ? 1 : 0,
				(int)t.decal[id].newparticle.emitterid, iElement, g.decalx, g.decaly, g.decalz, iStand );
		}
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "GET_PERF_DATA" ) == 0 ) Cmd_GetPerfData ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "SCREENSHOT" ) == 0 ) Cmd_Screenshot ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "DUMP_VRAM" ) == 0 ) Cmd_DumpVRAM ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "QUIT" ) == 0 )
	{
		Cmd_Quit ( result, sizeof(result) );
		return;
	}
	else if ( _stricmp ( cmd, "MOUSE_AT" ) == 0 || _stricmp ( cmd, "MOUSE_DOWN" ) == 0 || _stricmp ( cmd, "MOUSE_UP" ) == 0 )
	{
		// MOUSE_AT x y (client pixels, a SCREENSHOT's), MOUSE_DOWN, MOUSE_UP: the OS cursor moved and the left button pressed or let
		// go, as the DX12 build's verbs (hold the button across a frame: MOUSE_AT, MOUSE_DOWN, a pause, MOUSE_UP)
		int mx = -1, my = -1;
		const int got = arg[0] ? sscanf_s ( arg, "%d %d", &mx, &my ) : 0;
		const bool bAt = _stricmp ( cmd, "MOUSE_AT" ) == 0;
		if ( !g_pGlob || !g_pGlob->hWnd ) _snprintf ( result, sizeof(result), "ERROR: no window handle" );
		else if ( bAt && got != 2 ) _snprintf ( result, sizeof(result), "ERROR: MOUSE_AT <x> <y>" );
		else
		{
			if ( GetForegroundWindow ( ) != g_pGlob->hWnd ) SetForegroundWindow ( g_pGlob->hWnd );
			POINT pt = { mx, my };
			if ( got == 2 ) { ClientToScreen ( g_pGlob->hWnd, &pt ); SetCursorPos ( pt.x, pt.y ); }
			if ( !bAt )
			{
				INPUT in = {};
				in.type = INPUT_MOUSE;
				in.mi.dwFlags = _stricmp ( cmd, "MOUSE_DOWN" ) == 0 ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
				SendInput ( 1, &in, sizeof(INPUT) );
			}
			_snprintf ( result, sizeof(result), "OK: %s client=(%d,%d)", cmd, mx, my );
		}
	}
	else if ( _stricmp ( cmd, "SAVE_LEVEL" ) == 0 )
	{
		// SAVE_LEVEL: the editor's File > Save (gridedit_save_map) of the level open, as the DX12 build's verb; tests save
		// copies only
		extern void gridedit_save_map ( void );
		if ( strcmp ( AutoHarness_GetAppState ( ), "editor" ) != 0 ) _snprintf ( result, sizeof(result), "ERROR: SAVE_LEVEL only works in the editor (state: %s)", AutoHarness_GetAppState ( ) );
		else
		{
			gridedit_save_map ( );
			_snprintf ( result, sizeof(result), "OK: SAVE_LEVEL completed without crashing" );
		}
	}
	else if ( _stricmp ( cmd, "SPAWN_QUIET" ) == 0 )
	{
		// SPAWN_QUIET [0|1]: Lua's SpawnNewEntity without its log line per spawn (a stress test's thousands of spawns)
		extern bool g_bSpawnQuiet;
		int iOn = -1;
		if (arg) sscanf(arg, "%d", &iOn);
		if (iOn == 0 || iOn == 1) g_bSpawnQuiet = iOn == 1;
		_snprintf(result, resultSize, "OK: SPAWN_QUIET on=%d", g_bSpawnQuiet ? 1 : 0);
		result[resultSize - 1] = 0;
	}
	else if ( _stricmp ( cmd, "ENTITY_BANDS" ) == 0 )
	{
		// ENTITY_BANDS: where the entities' objects lie against the fixed object number ranges (entity headroom): live
		// entity objects, the lowest and highest, the entity range's start and end, how many sit in each fixed range, and the
		// object list's size and live count
		struct sBand { const char* name; int from; int to; int n; };
		sBand bands[] = {
			{ "batch85000", 85000, 89999, 0 }, { "weapons90000", 90000, 90999, 0 }, { "widget91000", 91000, 91999, 0 },
			{ "lightmapped92000", 92000, 110000, 0 }, { "dots110001", 110001, 130000, 0 }, { "dotcursor130001", 130001, 150000, 0 },
			{ "postprocess150001", 150001, 170000, 0 }, { "explosions170001", 170001, 180000, 0 }, { "ravey180001", 180001, 189900, 0 },
			{ "ebe189901", 189901, 190000, 0 }, { "occlusion190001", 190001, 199999, 0 }, { "models200000", 200000, 209999, 0 },
			{ "prompts210000", 210000, 219999, 0 }, { "secondaries220000", 220000, 299999, 0 }, { "debugdraw300000", 300000, 999999, 0 },
			{ "dotarcs1131002", 1131002, 1999999, 0 } };
		int nBands = sizeof(bands) / sizeof(bands[0]);
		int iLive = 0, iLow = 0x7fffffff, iHigh = 0, iInBands = 0;
		for (int e = 1; e <= g.entityelementlist && e < (int)t.entityelement.size(); e++)
		{
			int o = t.entityelement[e].obj;
			if (o <= 0 || ObjectExist(o) == 0) continue;
			iLive++;
			if (o < iLow) iLow = o;
			if (o > iHigh) iHigh = o;
			for (int b = 0; b < nBands; b++) if (o >= bands[b].from && o <= bands[b].to) { bands[b].n++; iInBands++; break; }
		}
		extern int g_iObjectListCount, g_iObjectListRefCount;
		// the editor's entity dots that exist: in their range from 1,000,001, and in the old one (110001 to 130000)
		extern int* g_ObjectListRef;
		int iDots = 0, iOldDots = 0;
		for (int s = 0; s < g_iObjectListRefCount; s++)
		{
			int o = g_ObjectListRef[s];
			if (o >= 1000001 && o <= 1999999) iDots++;
			else if (o >= 110001 && o <= 130000) iOldDots++;
		}
		int n = _snprintf(result, resultSize, "OK: ENTITY_BANDS live=%d low=%d high=%d start=%d end=%d inbands=%d list=%d listlive=%d elements=%d dots=%d olddots=%d",
			iLive, iLive ? iLow : 0, iHigh, g.entityviewstartobj, g.entityviewendobj, iInBands, g_iObjectListCount, g_iObjectListRefCount, g.entityelementlist, iDots, iOldDots);
		for (int b = 0; b < nBands && n > 0 && n < (int)resultSize - 64; b++)
			if (bands[b].n) n += _snprintf(result + n, resultSize - n, " %s=%d", bands[b].name, bands[b].n);
		result[resultSize - 1] = 0;
	}
	else if ( _stricmp ( cmd, "RAGDOLL_LIFT" ) == 0 )
	{
		// RAGDOLL_LIFT [0|1 [reset]]: the lift of a ragdoll out of the terrain it is made in (BPhys_RagDollEnd) off or on, and
		// its counts: ragdolls checked, lifted, the last lift and the largest (world units); `reset` zeroes the counts
		extern bool g_bRagdollLift;
		extern int g_iRagdollLiftChecked, g_iRagdollLifted;
		extern float g_fRagdollLiftLast, g_fRagdollLiftMax;
		int iOn = -1; char reset[16] = "";
		if (arg) sscanf(arg, "%d %15s", &iOn, reset);
		if (iOn == 0 || iOn == 1) g_bRagdollLift = iOn == 1;
		if (_stricmp(reset, "reset") == 0) { g_iRagdollLiftChecked = 0; g_iRagdollLifted = 0; g_fRagdollLiftLast = 0; g_fRagdollLiftMax = 0; }
		_snprintf(result, resultSize, "OK: RAGDOLL_LIFT on=%d checked=%d lifted=%d last=%.2f max=%.2f", g_bRagdollLift ? 1 : 0,
			g_iRagdollLiftChecked, g_iRagdollLifted, g_fRagdollLiftLast, g_fRagdollLiftMax);
		result[resultSize - 1] = 0;
	}
	else if ( _stricmp ( cmd, "RAGDOLL_COUNTS" ) == 0 || _stricmp ( cmd, "RAGDOLL_SETTLE" ) == 0 )
	{
		// RAGDOLL_COUNTS / RAGDOLL_SETTLE [0|1 [distance [seconds]]]: as the DX12 build's verbs
		extern void BPhys_RagdollCounts(int* pTotal, int* pStatic, int* pAwakeSlow, int* pBodiesAwake, float* pMaxLinear, float* pMaxAngular);
		extern int g_iRagdollSettleRule, g_iRagdollSettledByPlace, g_iRagdollSettledBySpeed;
		extern float g_fRagdollSettleDistance, g_fRagdollSettleSeconds;
		if ( _stricmp ( cmd, "RAGDOLL_SETTLE" ) == 0 && arg && arg[0] )
		{
			int on = 0; float distance = g_fRagdollSettleDistance, seconds = g_fRagdollSettleSeconds;
			sscanf ( arg, "%d %f %f", &on, &distance, &seconds );
			g_iRagdollSettleRule = on != 0 ? 1 : 0;
			if ( distance > 0.01f ) g_fRagdollSettleDistance = distance;
			if ( seconds > 0.05f ) g_fRagdollSettleSeconds = seconds;
		}
		int total = 0, stat = 0, slow = 0, bodies = 0; float lin = 0, ang = 0;
		BPhys_RagdollCounts ( &total, &stat, &slow, &bodies, &lin, &ang );
		_snprintf ( result, sizeof(result), "OK: %s ragdolls=%d static=%d awake=%d awake_slow=%d bodies_awake=%d fastest=%.1f units/s %.2f rad/s | rule=%d distance=%.2f seconds=%.2f settled_by_place=%d settled_by_speed=%d",
			cmd, total, stat, total - stat, slow, bodies, lin, ang, g_iRagdollSettleRule, g_fRagdollSettleDistance, g_fRagdollSettleSeconds, g_iRagdollSettledByPlace, g_iRagdollSettledBySpeed );
	}
	else if ( _stricmp ( cmd, "TEST_PROJECTILE" ) == 0 )
	{
		// TEST_PROJECTILE <gun> <source> <x> <y> <z> <pitch> <yaw>: a test launch of a gun's projectile as its firer launches
		// it (weapon_projectile_make): source 0 the player, else the firing entity, from a point at an angle (pitch down
		// positive); for shooter_credit_check.py
		char gun[256] = ""; int source = 0; float x = 0, y = 0, z = 0, pitch = 0, yaw = 0;
		if ( !arg || sscanf ( arg, "%255s %d %f %f %f %f %f", gun, &source, &x, &y, &z, &pitch, &yaw ) < 7 )
		{
			_snprintf ( result, sizeof(result), "ERROR: TEST_PROJECTILE <gun> <source> <x> <y> <z> <pitch> <yaw>" );
		}
		else
		{
			int gunid = 0;
			for ( int g2 = 1; g2 < (int)t.gun.size(); g2++ ) if ( _stricmp ( t.gun[g2].name_s.Get(), gun ) == 0 ) { gunid = g2; break; }
			if ( _strnicmp ( gun, "proj:", 5 ) == 0 )
			{
				// a projectile type by its own name (as a thrown beacon's), with no gun
				extern void weapon_getprojectileid ( void );
				extern void weapon_projectile_make ( bool bUsingVRForAngle, bool bDoNotAdvanceToAvoidPenetration );
				t.tProjectileType_s = Lower ( gun + 5 ); weapon_getprojectileid ( );
				int type = t.tProjectileType;
				if ( type > 0 )
				{
					t.tSourceEntity = source; t.tTracerFlag = 0;
					t.tStartX_f = x; t.tStartY_f = y; t.tStartZ_f = z;
					t.tAngX_f = pitch; t.tAngY_f = yaw; t.tAngZ_f = 0;
					weapon_projectile_make ( false, false );
				}
				_snprintf ( result, sizeof(result), "OK: TEST_PROJECTILE projectile=%s type=%d source=%d", gun + 5, type, source );
			}
			else if ( gunid == 0 )
			{
				_snprintf ( result, sizeof(result), "ERROR: TEST_PROJECTILE no gun %s", gun );
			}
			else
			{
				extern void weapon_getprojectileid ( void );
				extern void weapon_projectile_make ( bool bUsingVRForAngle, bool bDoNotAdvanceToAvoidPenetration );
				int iStoreGunID = t.gunid;
				t.gunid = gunid;
				t.tProjectileType_s = t.gun[gunid].projectile_s; weapon_getprojectileid ( );
				int type = t.tProjectileType;
				if ( type > 0 )
				{
					t.tSourceEntity = source; t.tTracerFlag = 0;
					t.tStartX_f = x; t.tStartY_f = y; t.tStartZ_f = z;
					t.tAngX_f = pitch; t.tAngY_f = yaw; t.tAngZ_f = 0;
					weapon_projectile_make ( false, false );
				}
				t.gunid = iStoreGunID;
				_snprintf ( result, sizeof(result), "OK: TEST_PROJECTILE gun=%d projectile=%s type=%d source=%d", gunid, t.gun[gunid].projectile_s.Get(), type, source );
			}
		}
	}
	else if ( _stricmp ( cmd, "PHYS_WAKES" ) == 0 )
	{
		// PHYS_WAKES [reset | 0 | 1 | list]: the wakes of what rests on a static body Lua's CollisionOff removes
		// (physics_wakeaboveobject): on or off (0 removes the body as before), calls, their time (total, largest, last), and
		// the last one's bodies, ragdolls and dropped guns woken and the broadphase's candidates in its box; list adds the
		// dropped guns watched and settled, with where each lies
		extern int g_iPhysWakeOn, g_iPhysWakeCalls, g_iPhysWakeLast[4];
		extern double g_dPhysWakeMsTotal, g_dPhysWakeMsMax, g_dPhysWakeMsLast;
		extern void physics_describedroppedweapons ( char* pOut, int iMax );
		if ( arg && _stricmp ( arg, "reset" ) == 0 )
		{
			g_iPhysWakeCalls = 0; g_dPhysWakeMsTotal = 0; g_dPhysWakeMsMax = 0; g_dPhysWakeMsLast = 0;
			for ( int i = 0; i < 4; i++ ) g_iPhysWakeLast[i] = 0;
		}
		else if ( arg && ( strcmp ( arg, "0" ) == 0 || strcmp ( arg, "1" ) == 0 ) ) g_iPhysWakeOn = atoi ( arg );
		char weapons[2048] = "";
		if ( arg && _stricmp ( arg, "list" ) == 0 ) physics_describedroppedweapons ( weapons, sizeof(weapons) );
		_snprintf ( result, sizeof(result), "OK: PHYS_WAKES on=%d calls=%d ms_total=%.3f ms_max=%.3f ms_last=%.3f last_bodies=%d last_ragdolls=%d last_weapons=%d last_candidates=%d %s",
			g_iPhysWakeOn, g_iPhysWakeCalls, g_dPhysWakeMsTotal, g_dPhysWakeMsMax, g_dPhysWakeMsLast, g_iPhysWakeLast[0], g_iPhysWakeLast[1], g_iPhysWakeLast[2], g_iPhysWakeLast[3], weapons );
	}
	else if ( _stricmp ( cmd, "NPCHOLE_LOG" ) == 0 )
	{
		// NPCHOLE_LOG [0|1]: 1 logs each NPC round's bullet hole ("NPC HOLE", 300 a session); 0, the default, doesn't
		extern int g_iNpcHoleLog;
		if ( arg && arg[0] ) g_iNpcHoleLog = atoi ( arg ) != 0 ? 1 : 0;
		_snprintf ( result, sizeof(result), "OK: NPCHOLE_LOG %d", g_iNpcHoleLog );
	}
	else if ( _stricmp ( cmd, "NAV_BOXCHECK" ) == 0 )
	{
		// NAV_BOXCHECK: the last navmesh path (Lua RDFindPath) as returned, each line checked against the blocking boxes (closed
		// doors, parked vehicles; the first line excepted when it steps out of the box the start stood in): lines crossing one,
		// the box the start and the goal were moved out of, lines taken round a box, rounded corners left sharp, the end point;
		// as the DX12 build's verb
		extern void GG_NavBoxCheck ( char* pOut, int size );
		char check[400];
		GG_NavBoxCheck ( check, sizeof(check) );
		_snprintf ( result, sizeof(result), "OK: NAV_BOXCHECK %s", check );
	}
	else if ( _stricmp ( cmd, "NAV_BOXES" ) == 0 )
	{
		// NAV_BOXES: the navmesh's blocker boxes: count, then per box its index, closed (1), centre x z, half sizes in its own
		// frame, turn in degrees, height span
		extern void GG_NavBoxes ( char* pOut, int size );
		char boxes[3000];
		GG_NavBoxes ( boxes, sizeof(boxes) );
		_snprintf ( result, sizeof(result), "OK: NAV_BOXES %s", boxes );
	}
	else if ( _stricmp ( cmd, "NAV_PATHBATCH" ) == 0 )
	{
		// NAV_PATHBATCH <in file>|<out file>[|reps]: path searches as Lua's RDFindPath makes them, one a line of the in file (start
		// x y z, goal x y z), each run reps times (1) and timed; a line each in the out file: index, ms a search, NAV_BOXCHECK's
		// answer, the points
		extern int GG_NavPathBatch ( const char* pIn, const char* pOut, int iReps );
		char a[2048];
		strncpy_s ( a, sizeof(a), arg ? arg : "", _TRUNCATE );
		char* pOutFile = strchr ( a, '|' );
		int iReps = 1;
		int iDone = -1;
		if ( pOutFile )
		{
			*pOutFile++ = 0;
			char* pReps = strchr ( pOutFile, '|' );
			if ( pReps ) { *pReps++ = 0; iReps = atoi ( pReps ); }
			iDone = GG_NavPathBatch ( a, pOutFile, iReps );
		}
		_snprintf ( result, sizeof(result), "%s: NAV_PATHBATCH %d searches, %d each", iDone >= 0 ? "OK" : "ERROR", iDone, iReps );
	}
	else if ( _stricmp ( cmd, "SET_NAVBOXFIX" ) == 0 )
	{
		// SET_NAVBOXFIX [0|1]: 1 (as built) a path search from or to a point inside a blocking box runs from (to) just outside it,
		// and a straight line across a box goes round it; 0 the old rule (no path from or to a box, a path crossing one thrown out)
		extern bool g_bGGNavBoxExits;
		if ( arg && arg[0] ) g_bGGNavBoxExits = atoi ( arg ) != 0;
		_snprintf ( result, sizeof(result), "OK: SET_NAVBOXFIX %d", g_bGGNavBoxExits ? 1 : 0 );
	}
	else if ( _stricmp ( cmd, "SET_LUASCREEN" ) == 0 )
	{
		// SET_LUASCREEN [w h]: the screen size Lua's sprite calls turn percentages into pixels by, set for a test of another
		// screen's shape (2560 1080 for 21:9); 0 0 puts back what it was; as the DX12 build's verb
		extern DWORD g_dwScreenWidth, g_dwScreenHeight;
		static DWORD savedW = 0, savedH = 0;
		int w = -1, h = -1;
		if ( arg && sscanf ( arg, "%d %d", &w, &h ) == 2 )
		{
			if ( w > 0 && h > 0 )
			{
				if ( savedW == 0 ) { savedW = g_dwScreenWidth; savedH = g_dwScreenHeight; }
				g_dwScreenWidth = (DWORD)w; g_dwScreenHeight = (DWORD)h;
			}
			else if ( savedW != 0 )
			{
				g_dwScreenWidth = savedW; g_dwScreenHeight = savedH; savedW = savedH = 0;
			}
		}
		_snprintf ( result, sizeof(result), "OK: SET_LUASCREEN %lu %lu", (unsigned long)g_dwScreenWidth, (unsigned long)g_dwScreenHeight );
	}
	else if ( _stricmp ( cmd, "SPRITE_SIZE" ) == 0 && arg && arg[0] )
	{
		// SPRITE_SIZE <sprite>: a sprite's size and offset in pixels and its image's size, as the DX12 build's verb
		extern int SpriteExist(int iID); extern int GetSpriteImage(int iID); extern int SpriteWidth(int iID); extern int SpriteHeight(int iID);
		extern int SpriteOffsetX(int iID); extern int SpriteOffsetY(int iID); extern int ImageWidth(int iID); extern int ImageHeight(int iID);
		const int id = atoi ( arg );
		if ( SpriteExist ( id ) != 1 ) _snprintf ( result, sizeof(result), "ERROR: SPRITE_SIZE no sprite %d", id );
		else
		{
			const int img = GetSpriteImage ( id );
			_snprintf ( result, sizeof(result), "OK: SPRITE_SIZE %d w=%d h=%d offx=%d offy=%d image=%d imgw=%d imgh=%d", id, SpriteWidth ( id ), SpriteHeight ( id ),
				SpriteOffsetX ( id ), SpriteOffsetY ( id ), img, img > 0 ? ImageWidth ( img ) : 0, img > 0 ? ImageHeight ( img ) : 0 );
		}
	}
	else if ( _stricmp ( cmd, "PLRVIS_LOCK" ) == 0 )
	{
		// PLRVIS_LOCK [0|1]: 1 holds the visibility list's lock through the extra thread's rays as before 2026-11-12
		extern int g_iPlrVisHoldLock;
		if ( arg && arg[0] ) g_iPlrVisHoldLock = atoi ( arg ) != 0 ? 1 : 0;
		_snprintf ( result, sizeof(result), "OK: PLRVIS_LOCK %d", g_iPlrVisHoldLock );
	}
	else if ( _stricmp ( cmd, "OBJECT_NAMES" ) == 0 )
	{
		// OBJECT_NAMES: the scene's objects counted by name (digits folded), "count name" a line in object_names.txt in the
		// game's current folder (Files), as the DX12 build's verb
		wiScene::Scene& scene = wiScene::GetScene();
		std::map<std::string, int> counts;
		for ( size_t i = 0; i < scene.objects.GetCount(); i++ )
		{
			const wiECS::Entity e = scene.objects.GetEntity ( i );
			const wiScene::NameComponent* nm = scene.names.GetComponent ( e );
			std::string name = nm ? nm->name : std::string ( "(unnamed)" );
			for ( char& c : name ) if ( c >= '0' && c <= '9' ) c = '#';
			const wiScene::MeshComponent* mesh = scene.meshes.GetComponent ( scene.objects[i].meshID );
			if ( mesh && mesh->armatureID != wiECS::INVALID_ENTITY ) name += " [skinned]";
			counts[name]++;
		}
		FILE* f = fopen ( "object_names.txt", "w" );
		if ( f )
		{
			for ( auto& kv : counts ) fprintf ( f, "%d %s\n", kv.second, kv.first.c_str() );
			fclose ( f );
		}
		_snprintf ( result, sizeof(result), "OK: OBJECT_NAMES objects=%d names=%d written=%d", (int)scene.objects.GetCount(), (int)counts.size(), f ? 1 : 0 );
	}
	else if ( _stricmp ( cmd, "SET_PHYSSLEEPSYNC" ) == 0 )
	{
		// SET_PHYSSLEEPSYNC [0|1]: the physics pose sync leaves a sleeping body already written alone and Bullet's motion
		// prediction leaves sleeping bodies out (1, the default), or both as before (0); answers with the last update's poses
		// written and skipped
		extern int g_iPhysicsSleepSync, g_iPhysicsPosesWritten, g_iPhysicsPosesSkipped;
		if ( arg && *arg ) g_iPhysicsSleepSync = atoi(arg) ? 1 : 0;
		extern int gGGPredictSkipSleeping; gGGPredictSkipSleeping = g_iPhysicsSleepSync;
		_snprintf ( result, sizeof(result), "OK: SET_PHYSSLEEPSYNC %d written=%d skipped=%d", g_iPhysicsSleepSync,
			g_iPhysicsPosesWritten, g_iPhysicsPosesSkipped );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "PUSH_TEST" ) == 0 )
	{
		// PUSH_TEST <e> <shot|melee> <impulse> [raise]: element e pushed as a shot's or a swing's push (the gunspec pushforce or
		// melee pushforce, newton-seconds, with SetPropPushLimits' cap) from the camera, the ray to the centre of its box raised
		// by raise units; answers the impulse given and the body's mass (-1 no such element or object)
		int e = 0; char kind[16] = { 0 }; float fImpulse = 0.0f, fRaise = 0.0f;
		if (arg) sscanf(arg, "%d %15s %f %f", &e, kind, &fImpulse, &fRaise);
		extern float entity_pushproptest(int e, bool bMelee, float fImpulse, float fRaise, float* pMassKg);
		float fMass = 0.0f;
		const bool bMelee = _stricmp(kind, "melee") == 0;
		const float fGiven = entity_pushproptest(e, bMelee, fImpulse, fRaise, &fMass);
		_snprintf(result, sizeof(result), "OK: PUSH_TEST e=%d %s asked=%.3f given=%.3f mass=%.2f", e, bMelee ? "melee" : "shot", fImpulse, fGiven, fMass);
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "SET_WALKLISTS" ) == 0 )
	{
		// SET_WALKLISTS [0|1]: the per-frame entity walks (the Lua loop, entity_loop, its explodables' walk, entity_loopanim)
		// step lists of the elements in use and the dynamic ones (1, the default since 2026-11-17) or every element as before
		// (0). Answers the lists' sizes, the element count, how many times the lists have been made (for the element count or a
		// game's start, and on the half second) and the elements merged in between
		extern int g_iWalkListsOn;
		extern std::vector<int> g_WalkInUse, g_WalkDynamic;
		extern uint32_t g_uWalkListsRebuilds[3], g_uWalkListsMerged;
		if ( arg && *arg ) g_iWalkListsOn = atoi(arg) ? 1 : 0;
		_snprintf ( result, sizeof(result), "OK: SET_WALKLISTS %d inuse=%d dynamic=%d elements=%d rebuilds_count=%u rebuilds_time=%u merged=%u",
			g_iWalkListsOn, (int)g_WalkInUse.size(), (int)g_WalkDynamic.size(), g.entityelementlist, g_uWalkListsRebuilds[0],
			g_uWalkListsRebuilds[1], g_uWalkListsMerged );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "DRAWN_POS" ) == 0 )
	{
		// DRAWN_POS <obj>: where the renderer draws an object (its Wicked root transform's world position) beside the
		// object's own position, for checking a body's pose reached what is drawn
		extern bool WickedCall_DrawnPosition ( sObject* pObject, float* pXYZ );
		int iObj = arg ? atoi(arg) : 0;
		sObject* pObject = (iObj > 0 && ObjectExist(iObj) == 1) ? GetObjectData(iObj) : NULL;
		float fDrawn[3] = { 0, 0, 0 };
		if ( pObject == NULL || !WickedCall_DrawnPosition(pObject, fDrawn) )
			_snprintf ( result, sizeof(result), "ERROR: DRAWN_POS no drawn object %d", iObj );
		else
			_snprintf ( result, sizeof(result), "OK: DRAWN_POS %d drawn=%.2f,%.2f,%.2f object=%.2f,%.2f,%.2f custom=%d", iObj,
				fDrawn[0], fDrawn[1], fDrawn[2], pObject->position.vecPosition.x, pObject->position.vecPosition.y,
				pObject->position.vecPosition.z, pObject->position.bCustomWorldMatrix ? 1 : 0 );
		result[sizeof(result) - 1] = 0;
	}
	else if ( _stricmp ( cmd, "PHYS_TIMES" ) == 0 )
	{
		// PHYS_TIMES [reset]: the physics range's parts per frame since the last reset: tree cylinders, the player, the world's
		// update (its step and its pose sync within it), dropped weapons, knocked out ragdolls, the player's control and death
		extern double g_dPhysPartMs[10]; extern uint32_t g_uPhysPartFrames;
		const double f = g_uPhysPartFrames > 0 ? (double)g_uPhysPartFrames : 1.0;
		_snprintf(result, sizeof(result), "OK: PHYS_TIMES frames=%u treecyl=%.4f player=%.4f world=%.4f step=%.4f sync=%.4f weapons=%.4f knockout=%.4f control=%.4f death=%.4f",
			g_uPhysPartFrames, g_dPhysPartMs[0] / f, g_dPhysPartMs[1] / f, g_dPhysPartMs[2] / f, g_dPhysPartMs[7] / f, g_dPhysPartMs[8] / f,
			g_dPhysPartMs[3] / f, g_dPhysPartMs[4] / f, g_dPhysPartMs[5] / f, g_dPhysPartMs[6] / f);
		result[sizeof(result) - 1] = 0;
		// PHYS_TIMES zones on|off: Bullet's own zones timed too, listed after the parts (zone=ms a frame/calls a frame)
		extern int g_iPhysZonesWant; extern void PhysZones_Text ( char* pOut, int iSize, double dFrames ); extern void PhysZones_Reset ( void );
		if (arg && _strnicmp(arg, "zones", 5) == 0) g_iPhysZonesWant = strstr(arg, "off") ? -1 : 1;
		{
			int iLen = (int)strlen(result);
			if (iLen < (int)sizeof(result) - 64) PhysZones_Text(result + iLen, (int)sizeof(result) - iLen, f);
		}
		if (arg && _stricmp(arg, "reset") == 0) { for (int i = 0; i < 10; i++) g_dPhysPartMs[i] = 0; g_uPhysPartFrames = 0; PhysZones_Reset(); }
	}
	else if ( _stricmp ( cmd, "GET_PHYSRATE" ) == 0 )
	{
		// GET_PHYSRATE: the physics world's rate in force (what Tab Tab's Physics World At 60Hz box shows) and the level's
		extern bool g_bPhysicsWorldAt60;
		_snprintf ( result, sizeof(result), "OK: GET_PHYSRATE in_force=%d level=%d game_level=%d", g_bPhysicsWorldAt60 ? 60 : 120,
			t.visuals.bPhysicsWorldAt60 ? 60 : 120, t.gamevisuals.bPhysicsWorldAt60 ? 60 : 120 );
	}
	else if ( _stricmp ( cmd, "ANIM_SHARING" ) == 0 )
	{
		// ANIM_SHARING: clones that took their template's built animation, key data held by more than one, the scene's
		// animations and key data, the builds that didn't share by reason, the providers (as the DX12 build's verb)
		extern void WickedCall_AnimSharingInfo ( char* pOut, int iSize );
		WickedCall_AnimSharingInfo ( result, sizeof(result) );
	}
	else if ( _stricmp ( cmd, "ANIM_POSE" ) == 0 )
	{
		// ANIM_POSE e: entity e's animation and its pose as a sum of its bones' animated rotations (as the DX12 build's verb)
		int e = atoi ( arg );
		if ( e < 1 || e > g.entityelementlist || t.entityelement[e].obj <= 0 || !ObjectExist ( t.entityelement[e].obj ) )
			_snprintf ( result, sizeof(result), "ERROR: ANIM_POSE entity %d has no object", e );
		else
		{
			extern void WickedCall_AnimPoseInfo ( sObject* pObj, int e, char* pOut, int iSize );
			WickedCall_AnimPoseInfo ( GetObjectData ( t.entityelement[e].obj ), e, result, sizeof(result) );
		}
	}
	else if ( _stricmp ( cmd, "PRESS_ESCAPE" ) == 0 )
	{
		// PRESS_ESCAPE: leave a test game as its Escape key does (the game's loops ended), as the DX12 build's verb
		if ( strcmp ( AutoHarness_GetAppState ( ), "game" ) == 0 )
		{
			t.game.gameloop = 0;
			t.game.levelloop = 0;
			t.game.masterloop = 0;
			_snprintf ( result, sizeof(result), "OK: Triggered escape from test game" );
		}
		else _snprintf ( result, sizeof(result), "ERROR: PRESS_ESCAPE only works in game state (current state: %s)", AutoHarness_GetAppState ( ) );
	}
	else if ( _stricmp ( cmd, "UNDO_TOP" ) == 0 )
	{
		// UNDO_TOP: the editor's undo and redo lists, each's depth and its top step's kind (0 objects, 1 terrain, 2 splines),
		// and the selection, as the DX12 build's verb
		extern std::stack<sUndoMasterStackItem> g_UndoSysMasterStack[2];
		const std::stack<sUndoMasterStackItem>& u = g_UndoSysMasterStack[eUndoSys_UndoList];
		const std::stack<sUndoMasterStackItem>& r = g_UndoSysMasterStack[eUndoSys_RedoList];
		const int resultSize = (int)sizeof(result);
		_snprintf ( result, sizeof(result), "OK: UNDO_TOP undo depth %d top type %d count %d | redo depth %d top type %d",
			(int)u.size ( ), u.empty ( ) ? -1 : (int)u.top ( ).type, u.empty ( ) ? 0 : u.top ( ).count,
			(int)r.size ( ), r.empty ( ) ? -1 : (int)r.top ( ).type );
		int sl = (int)strlen(result);
		sl += _snprintf(result + sl, resultSize - sl, " | selected %d:", (int)g.entityrubberbandlist.size());
		for (size_t i = 0; i < g.entityrubberbandlist.size() && i < 6 && sl > 0 && sl < (int)resultSize - 80; i++)
		{
			const int e = g.entityrubberbandlist[i].e;
			sl += _snprintf(result + sl, resultSize - sl, " %d %s;", e, (e > 0 && e < (int)t.entityelement.size()) ? t.entityelement[e].eleprof.name_s.Get() : "?");
		}
	}
	else if ( _stricmp ( cmd, "ENTITY_FIND" ) == 0 )
	{
		// ENTITY_FIND <static|explodable> <x> <z> <radius>: the nearest live entities (up to 8) with that flag within the radius
		// of a point, nearest first: index, distance, name (static: staticflag set; explodable: eleprof.explodable set and
		// health above 0); for shooter_credit_check.py
		char kind[32] = ""; float x = 0, z = 0, radius = 0;
		if (!arg || sscanf(arg, "%31s %f %f %f", kind, &x, &z, &radius) < 4) _snprintf(result, resultSize, "ERROR: ENTITY_FIND <static|explodable> <x> <z> <radius>");
		else
		{
			std::vector<std::pair<float, int>> found;
			bool bStatic = _stricmp(kind, "static") == 0;
			for (int e = 1; e <= g.entityelementlist; e++)
			{
				if (t.entityelement[e].bankindex <= 0 || t.entityelement[e].obj <= 0) continue;
				if (bStatic ? t.entityelement[e].staticflag != 1 : (t.entityelement[e].eleprof.explodable == 0 || t.entityelement[e].health <= 0)) continue;
				float dx = t.entityelement[e].x - x, dz = t.entityelement[e].z - z;
				float d = sqrtf(dx * dx + dz * dz);
				if (d <= radius) found.push_back(std::make_pair(d, e));
			}
			std::sort(found.begin(), found.end());
			int n = _snprintf(result, resultSize, "OK: ENTITY_FIND %s found=%d", kind, (int)found.size());
			for (int i = 0; i < (int)found.size() && i < 8 && n > 0 && n < (int)resultSize - 160; i++)
				n += _snprintf(result + n, resultSize - n, " | %d %.0f %.0f %.0f %s", found[i].second, found[i].first,
					t.entityelement[found[i].second].x, t.entityelement[found[i].second].z, t.entityelement[found[i].second].eleprof.name_s.Get());
		}
		result[resultSize - 1] = 0;
	}
	else if ( _stricmp ( cmd, "TEST_DELETE_UNDO" ) == 0 )
	{
		// TEST_DELETE_UNDO <e>: entity e renamed, deleted as the Delete key deletes it (DeleteEntityFromLists, then
		// gridedit_deleteentityfrommap and its undo event), then undone, redone and undone again, its name and script read
		// after each; its own name put back at the end; for undo_name_check.py
		extern bool DeleteEntityFromLists ( int e );
		extern void editor_undo ( void );
		extern void editor_redo ( void );
		extern bool g_UndoSysObjectIsBeingMoved;
		int e = arg ? atoi ( arg ) : 0;
		if ( strcmp ( AutoHarness_GetAppState(), "editor" ) != 0 ) _snprintf ( result, sizeof(result), "ERROR: TEST_DELETE_UNDO only works in the level editor" );
		else if ( e <= 0 || e > g.entityelementlist || t.entityelement[e].bankindex <= 0 ) _snprintf ( result, sizeof(result), "ERROR: TEST_DELETE_UNDO no entity %d", e );
		else
		{
			cstr old_s = t.entityelement[e].eleprof.name_s;
			cstr script_s = t.entityelement[e].eleprof.aimain_s;
			t.entityelement[e].eleprof.name_s = "UndoNameTest";
			DeleteEntityFromLists ( e );
			t.tentitytoselect = e;
			g_UndoSysObjectIsBeingMoved = false;
			gridedit_deleteentityfrommap ( );
			int bankDeleted = t.entityelement[e].bankindex;
			editor_undo ( );
			cstr undo1_s = t.entityelement[e].eleprof.name_s;
			cstr script1_s = t.entityelement[e].eleprof.aimain_s;
			int bankUndo1 = t.entityelement[e].bankindex;
			editor_redo ( );
			int bankRedo = t.entityelement[e].bankindex;
			editor_undo ( );
			cstr undo2_s = t.entityelement[e].eleprof.name_s;
			int bankUndo2 = t.entityelement[e].bankindex;
			if ( t.entityelement[e].bankindex > 0 ) t.entityelement[e].eleprof.name_s = old_s;
			_snprintf ( result, sizeof(result), "OK: TEST_DELETE_UNDO e=%d was=%s script=%s | deleted bank=%d | undo bank=%d name=%s script=%s | redo bank=%d | undo again bank=%d name=%s",
				e, old_s.Get(), script_s.Get(), bankDeleted, bankUndo1, undo1_s.Get(), script1_s.Get(), bankRedo, bankUndo2, undo2_s.Get() );
		}
	}
	else if ( _stricmp ( cmd, "UNDO" ) == 0 || _stricmp ( cmd, "REDO" ) == 0 )
	{
		// UNDO / REDO: the editor's own Ctrl+Z / Ctrl+Y (editor_undo, editor_redo), in the level editor only, as the DX12 build's verbs
		extern void editor_undo ( void );
		extern void editor_redo ( void );
		if ( strcmp ( AutoHarness_GetAppState(), "editor" ) != 0 ) _snprintf ( result, sizeof(result), "ERROR: %s only works in the level editor", cmd );
		else
		{
			if ( _stricmp ( cmd, "UNDO" ) == 0 ) editor_undo ( ); else editor_redo ( );
			_snprintf ( result, sizeof(result), "OK: %s performed", cmd );
		}
	}
	else if ( spline_harness ( cmd, arg, result, (int)sizeof(result) ) ) { } // SPLINE_INFO, SPLINE_LIST, SPLINE_EDIT, SPLINE_UNBAKE, SPLINE_REBAKE, SPLINE_DUMPMAPS, SPLINE_GROUND, SPLINE_ADDRIVER (M-Splines.cpp)
	else _snprintf ( result, sizeof(result), "ERROR: Unknown command '%s'", cmd );
	result[sizeof(result) - 1] = 0;
	AutoHarness_WriteResult ( result );
}
