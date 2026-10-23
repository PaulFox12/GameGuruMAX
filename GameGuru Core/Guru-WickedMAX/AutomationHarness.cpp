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
#include "..\GameGuru\Imgui\imgui.h"
#include "..\GameGuru\Imgui\imgui_internal.h"
#include "..\GameGuru\Imgui\imgui_gg_dx11.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <string>
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
	_snprintf ( result, resultSize, "STATE: %s\nFPS: %.1f\nFRAME_TIME_MS: %.2f\nVRAM_MB: %.1f\nVRAM_BUDGET_MB: %.1f\nVRAM_SHARED_MB: %.1f\nPOLYS: %d\nDRAW_CALLS: %d\n",
		AutoHarness_GetAppState(), fps, fps > 0.0f ? 1000.0f / fps : 0.0f, fUsedMB, fBudgetMB, fSharedUsedMB, (int)wiProfiler::GetPolygons(), (int)wiProfiler::GetDrawCalls() );
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
	else if ( _stricmp ( cmd, "SET_PARTICLESCATTER" ) == 0 )
	{
		// SET_PARTICLESCATTER <strength>: the sun's forward scattering on lit particle layers (as SetParticleScatter), 0 off
		extern void WickedCall_SetParticleScatter(float fStrength);
		extern float WickedCall_GetParticleScatter(void);
		WickedCall_SetParticleScatter((float)atof(arg));
		_snprintf(result, sizeof(result), "OK: SET_PARTICLESCATTER %.3f", WickedCall_GetParticleScatter());
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
	else if ( _stricmp ( cmd, "GET_PERF_DATA" ) == 0 ) Cmd_GetPerfData ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "SCREENSHOT" ) == 0 ) Cmd_Screenshot ( result, sizeof(result) );
	else if ( _stricmp ( cmd, "DUMP_VRAM" ) == 0 ) Cmd_DumpVRAM ( arg, result, sizeof(result) );
	else if ( _stricmp ( cmd, "QUIT" ) == 0 )
	{
		Cmd_Quit ( result, sizeof(result) );
		return;
	}
	else _snprintf ( result, sizeof(result), "ERROR: Unknown command '%s'", cmd );
	result[sizeof(result) - 1] = 0;
	AutoHarness_WriteResult ( result );
}
