//
// AutomationHarness.cpp - File-based command/response automation for Claude Code testing
//

#include "stdafx.h"
#include "AutomationHarness.h"
#include "GameGuruMain.h"
#include "gameguru.h"
#include "master.h"
#include "tracers/TracerManager.h"   // GGMAX 3.55: DUMP_TRACERS
#include "..\GameGuru\Imgui\imgui.h"
#include "..\GameGuru\Imgui\imgui_internal.h"
#include <stdio.h>
#include <dbghelp.h>          // GGMAX 1.81: symbolize Scene::Update callers (DUMP_SCENEUPDATE)
#pragma comment(lib, "dbghelp.lib")
#include <string.h>
#include <time.h>
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm> // std::sort (FIND_OBJECT distance ranking)
#include <map>       // GGMAX 3.122: DUMP_GUNARMS reads g_guns_loadedArms

// Terrain debug accessor (extern "C" from GGTerrain_part0.cpp)
extern "C" int GGTerrain_GetDrawDebugInfo(int* drawCount, int* exitReason, int* initFlag, int* drawEn, int* updateEn);

// Tree params for the SET_TREES live-tuning command
#include "GGTerrain/GGTrees.h"
#include "GGTerrain/GGGrass.h" // GGMAX 1.97: SET_GRASSPARAM writes gggrass_global_params

// Terrain params + undo enums for the SCULPT_TEST / PAINT_TEST commands
#include "GGTerrain/GGTerrain.h"
#include "M-UndoSys.h"

// Wicked-terrain blend/bridge diagnostics defined in GGTerrainWicked.cpp (GET_PERF_DATA).
// Note: they sit at file scope BEFORE that file's namespace GGTerrain opens — global namespace.
extern uint64_t g_dbgBridgeCalls, g_dbgBridgeChunksMarked, g_dbgBridgeKeysErased,
	g_dbgAutoBlendChunks, g_dbgPaintBlendChunks;
extern size_t g_dbgInvalidatedCensus, g_dbgMergePendingCensus;
extern bool g_terrainIdleGate;
extern uint64_t g_dbgIdleGateSkips;
extern uint32_t g_dbgIdleCalmFrames;
extern uint64_t g_dbgAutoSkipNoChunk, g_dbgAutoSkipNoLayers, g_dbgAutoSkipInvalid,
	g_dbgAutoSkipMergePend, g_dbgAutoPassRuns;
extern size_t g_dbgAutoLastPending;
// Grass-hair lifecycle diagnostics (shadow-flicker investigation) — global scope in GGTerrainWicked.cpp.
extern uint64_t g_dbgGrassRecycles, g_dbgGrassFullResets, g_dbgGrassRecreates;
extern size_t g_dbgGrassDeadMeshNow;
extern uint64_t g_dbgGrassExternalKills; // 2026-08-05 sculpt-grass fix: dead-record repairs

// GGMAX diag (FPS-plummet hunt): Scene::Intersects breakdown counters (engine wiScene.cpp)
#include <atomic>
namespace wi { namespace scene {
	extern std::atomic<unsigned long long> gg_dbg_isect_calls, gg_dbg_isect_objects,
		gg_dbg_isect_aabbpass, gg_dbg_isect_tris, gg_dbg_isect_skintris;
} }
// GGMAX diag (VT FreeSort hunt): free-list rebuild forensics (engine wiTerrain.cpp)
namespace wi { namespace terrain {
	extern std::atomic<unsigned long long> gg_dbg_vt_rebuilds, gg_dbg_vt_scan_us, gg_dbg_vt_sort_us,
		gg_dbg_vt_free, gg_dbg_vt_requests, gg_dbg_vt_reason, gg_dbg_vt_tiles;
} }
// GGMAX 1.87: merged-grass type-resolution freeze (flicker probe). Declared at file scope with
// its real namespace — a block-scope `extern bool` inside the command handler would resolve to
// the GLOBAL namespace and fail to link, a trap this file has hit before.
namespace wi { extern bool gg_grass_freeze_type; extern int gg_grass_freeze_mode; }
// GGMAX wall-gap tracer (engine wiProfiler.cpp): frame gaps >100ms dumped to gap_trace.txt
namespace wi { namespace profiler {
	extern std::atomic<unsigned long long> gg_trace_gap_count, gg_trace_gap_last_ms;
} }
// GGMAX 1.82: worst SINGLE lazy PSO compile since launch (engine wiGraphicsDevice_DX12.cpp,
// global namespace). Declared at file scope, not inside the function that reads it — a
// block-scope extern picks up the enclosing namespace and mangles wrongly, which has cost
// a build cycle before.
extern std::atomic<unsigned long long> gg_dbg_pso_compile_max_us;
// GGMAX 2.61: CPU-blocking copy-queue waits in CopyAllocator::submit (engine
// wiGraphicsDevice_DX12.cpp, global namespace — file scope for the same mangling reason).
extern std::atomic<unsigned long long> gg_dbg_copywait_us, gg_dbg_copywait_events;
// GGMAX 2.62: Terrain Generator biome click injection + selection mirror (M-TerrainNew_part5.cpp)
extern int g_ggHarnessBiomeClick;
extern int g_ggBiomeSelectedMirror;
// GGMAX 2.62/2.63: biome-reaction chain counters (GGTerrainWicked.cpp; this file includes
// GGTerrain.h but not GGTerrainWicked.h, so declare them here at file scope)
namespace GGTerrain { extern uint32_t gg_dbg_checkparams_runs, gg_dbg_checkparams_resets, gg_dbg_params_notifies, gg_dbg_params_wipes, gg_dbg_material_notifies; }
#include "wiProfiler.h"       // GGMAX 1.82: gg_hitch_reset / gg_hitch_get / GG_HITCH_BUCKETS

// WickedEngine helpers for screenshot and scene interrogation
#include "wiHelper.h"
#include "wiResourceManager.h" // REUPLOAD_TEXTURE probe (corruption hunt)
namespace wi { namespace resourcemanager { extern bool gg_streaming_paused; } } // SET_STREAMING probe
// GGMAX 1.69: texture-streaming observability counters (engine wiResourceManager.cpp)
namespace wi { namespace resourcemanager {
	extern std::atomic<uint32_t> gg_dbg_stream_enrolled, gg_dbg_stream_replaced, gg_dbg_stream_mem_permille;
	extern std::atomic<unsigned long long> gg_dbg_stream_resident_bytes, gg_dbg_stream_full_bytes;
	extern std::atomic<unsigned long long> gg_dbg_stream_job_starts, gg_dbg_stream_job_ends, gg_dbg_stream_busy_skips;
	extern std::atomic<uint32_t> gg_dbg_stream_dec_req0, gg_dbg_stream_dec_reqlow, gg_dbg_stream_dec_nomips,
		gg_dbg_stream_dec_cancel, gg_dbg_stream_dec_in, gg_dbg_stream_dec_out, gg_dbg_stream_max_req;
	// GGMAX 1.73: streaming bounds-guard rejections. Non-zero means the job refused an upload
	// that would have read past the end of its buffer — details in stream_guard.txt.
	extern std::atomic<uint32_t> gg_dbg_stream_guard_rejects;
	// GGMAX 1.73: per-load breadcrumb trace arm switch (harness SET_TEXSTREAMTRACE)
	extern bool gg_stream_load_trace;
} }
// GGMAX 1.73: upload footprint dump arm switch (engine wiGraphicsDevice_DX12.cpp, global scope)
extern bool gg_upload_trace;
// GGMAX 1.69: streaming feedback-chain probes (copies -> fb -> req = each link of GPU feedback)
namespace wi { extern std::atomic<unsigned long long> gg_dbg_stream_req_calls; }
namespace wi { namespace scene { extern std::atomic<unsigned long long> gg_dbg_stream_fb_hits; } }
namespace wi { namespace renderer { extern std::atomic<unsigned long long> gg_dbg_stream_copies; } }
// GGMAX: transparent shadow atlas write counter (wiRenderer.cpp). Must be declared at NAMESPACE
// scope — a block-scope `extern` binds to the global namespace instead and fails to link.
namespace wi { namespace renderer { extern std::atomic<unsigned long long> gg_dbg_transparent_shadow_batches; } }
namespace wi { namespace renderer { extern bool gg_probe_capture; } } // SET_PROBECAPTURE: environment probe captures on or held
namespace wi { namespace renderer { extern bool gg_transparent_shadows; } } // A/B: is the 512 MB atlas visually load-bearing?
extern bool g_bTextureStreamingEnabled; // game-side enrollment kill-switch (wickedcalls_part0.cpp), harness SET_TEXSTREAM
#include "wiGraphicsDevice.h"
#include "wiApplication.h"
#include "wiScene.h"
#include "wiRenderer.h"

// Direct access to profiler internals for diagnostics
// GGMAX 2.94: brutal off-switch effective flags (GGTerrain/GGTerrainWicked.cpp). Global
// namespace, but declared at FILE scope so every handler below can read them.
extern bool gg_no_terrain;
extern bool gg_no_trees;
extern bool gg_no_grass;
extern bool gg_no_water;
// GGMAX 3.25: the two panel bake switches are plain bools with no machine||level setter pair
// (they are per-LEVEL only), so the switch table needs a function-pointer shaped wrapper.
extern bool gg_terrain_bake;
extern bool gg_water_bake;
static void GGHarness_SetTerrainBake(int on) { gg_terrain_bake = (on != 0); }
static void GGHarness_SetWaterBake  (int on) { gg_water_bake   = (on != 0); }

namespace wi::profiler {
	extern bool ENABLED;
	extern bool ENABLED_REQUEST;
	extern float gg_gpu_busy_time;   // GGMAX 2.91 - union of the child GPU ranges (wiProfiler.cpp:85)
	extern float gg_gpu_idle_time;   // GGMAX 2.91 - frame span minus that union
}

// GGMAX 1.33: incremental terrain-VT bookkeeping master switch (wiTerrain.cpp)
namespace wi::terrain {
	extern bool gg_vt_incremental;
	extern uint32_t gg_bake_resolution;   // GGMAX 3.02
	extern int  gg_vt_writeback_interval;   // GGMAX 2.94d
}

// GGMAX 1.36/1.41: hierarchy + material-cache master switches (wiScene.cpp)
namespace wi::scene {
	extern bool gg_hierarchy_levelorder;
	extern bool gg_material_cache;
}

// GGMAX 1.53b/c: terrain VT tiling cap + hold live re-tune (GGTerrainWicked.cpp)
namespace GGTerrain { void GGTerrainWicked_SetTileShare(int k, int hold); }
namespace GGTerrain { void GGTerrainWicked_GetGrassDebug(int* pWickedEnabled, int* pHairCount, int* pVisibleCount, unsigned long long* pStrandSum); } // GGMAX 2.67
namespace GGTerrain { bool GGTerrainWicked_IsTerrainHidden(); bool GGTerrainWicked_IsRingComplete(); } // GGMAX 2.68h

// GGMAX 1.37: hair/grass sim static-skip master switch (wiRenderer.cpp)
namespace wi::renderer { extern uint32_t gg_lights_visible; extern uint32_t gg_lights_packed; } // GGMAX: the light pool's counts
namespace wi::renderer {
	extern bool gg_hair_sim_static_skip;
	extern uint32_t gg_hair_sim_wind_interval;
	extern bool gg_grass_wetmap; // 1.50: true = stock Wicked ocean/rain wetting on GG grass (dark-on-reveal bug demo)
	extern float gg_shadow_receiver_bias; // 1.57: sun-cascade receiver depth bias (animated self-shadow flicker fix)
	extern int gg_debugvis; // 1.62: tangent-frame visualization mode (SET_TANGENTVIS 0-5)
}

// GGMAX 1.49: grass strand LOD knobs (wiHairParticle.cpp)
namespace wi {
	extern bool gg_grass_lod;
	extern float gg_grass_lod_step2_frac, gg_grass_lod_step4_frac, gg_grass_lod_width_boost;
}

// GGMAX 1.39/1.40: underwater skip + command-list merge switches (wiRenderPath3D.cpp)
namespace wi {
	extern bool gg_skip_underwater_above_water;
	extern bool gg_underwater_ceiling_exact;   // GGMAX 3.120: SET_UWCEILING
	extern bool gg_render_merge_lists;
	extern float gg_app_submit_present_ms; // 1.32c: submit+present wall time (wiApplication.cpp)
}

// GGMAX 1.48a/b/c: submit-tail phase attribution + queue-routing switches (wiGraphicsDevice_DX12.cpp)
namespace wi::graphics {
	extern float gg_submit_ms_close, gg_submit_ms_fences, gg_submit_ms_present, gg_submit_ms_sync, gg_submit_ms_stall;
	extern uint32_t gg_submit_lists, gg_submit_batches, gg_submit_deps;
	// GGMAX 3.26a: command lists DROPPED because Close() failed, cumulative for the session.
	// Surfaced because Max/Files/log.txt is LOST under taskkill //F - a harness run can prove the
	// app survived and still tell you nothing about what it logged. This survives in the reply.
	extern std::atomic<uint32_t> gg_close_failures;
	extern bool gg_single_queue;
	extern bool gg_lean_async;
	// GGMAX 2.16: rolling stall window — the per-frame values above are last-frame snapshots
	// and `stall` is variable (0.00 in three Switch Escape captures, 0.38 in a fourth).
	extern double   gg_stall_sum_ms;
	extern float    gg_stall_max_ms;
	extern uint32_t gg_stall_frames, gg_stall_nonzero;
	void GG_ResetSubmitStats();
}

// The global Master instance
extern Master master;

// State flags from the engine
extern bool bWelcomeScreen_Window;
extern bool bStoryboardWindow;
extern bool bImGuiInTestGame;
extern bool g_bNoGGUntilGameGuruMainCalled;
extern int g_iInitializationSequence;
extern int iLaunchAfterSync;
extern int iStoryboardExecuteKey;
extern cstr TriggerLoadGameProject;
extern bool g_bDisableQuitFlag;
extern bool bProceduralLevel;

// Screen editor (from M-GridEdit_part0.cpp)
extern bool bScreen_Editor_Window;
extern int iScreen_Editor_Node;
extern int g_iAutoExitScreenEditor;

// Toolbar toggle states (from M-GridEdit_part0.cpp)
extern bool bEditorLight;
extern bool bTerrain_Tools_Window;
extern bool Entity_Tools_Window;
extern bool Visuals_Tools_Window;
extern bool Shooter_Tools_Window;
extern bool Game_Settings_Window;
extern bool Weather_Tools_Window;

// Level loading from storyboard
extern char cDirectOpen[260];
extern int iSkibFramesBeforeLaunch;
extern bool bCloseStoryboardAfterLoad;
extern int iLevelEditorFromStoryboardID;

// Global struct for window handle
#include "globstruct.h"
extern GlobStruct* g_pGlob;

// Memory/VRAM helpers
DARKSDK int SMEMAvailable(int iMode);
float GetTotalVramUsage(void);

// Demo game library list and edit trigger
#include "M-GridEditB.h"
extern std::vector<sLibraryList> g_LibraryFileList;
extern bool bTriggerEditDemoGame;

// My Games projects list (populated by GetProjectList("projectbank\\", ...) whenever the
// hub renders; see M-GridEditB_part16.cpp:627). One string per user project folder.
#include <vector>
#include <string>

// GGMAX 3.35i: diagnostic files go beside the EXE, not into whatever the CWD currently is
// (a file dialog can move it). Defined in Guru-WickedMAX/master_part1.cpp.
extern FILE* GGDiagFopen(const char* name, const char* mode);
extern std::vector<std::string> projectbank_list;

// Storyboard data (StoryboardStruct defined in imgui_gg_dx11.h, included via M-GridEditB.h)
extern StoryboardStruct Storyboard;

// Tab force variable
int g_iAutoForceWelcomeTab = -1;

// Demo selection variable (set by SELECT_DEMO, consumed by Welcome_Screen)
char g_sAutoSelectDemo[260] = {0};

// Command/response file paths (absolute, computed at init from exe directory)
static char s_cmdPath[MAX_PATH] = {0};
static char s_rspPath[MAX_PATH] = {0};
static char s_logPath[MAX_PATH] = {0};

// Uptime tracking
static DWORD s_startTick = 0;
static bool s_initialized = false;

// Set to true once the harness processes its first command.
// Keeps app running even when window loses focus, so automation works while alt-tabbed.
bool g_bAutomationActive = false;

// BURST_FRAMES: capture N CONSECUTIVE rendered frames (one screenshot per frame) to
// Files/screenshots/frame_###.png. The per-frame readback stalls the GPU (frame time
// balloons) but preserves frame-to-frame TEMPORAL structure — the instrument for
// separating per-frame alternation artifacts (A/B/A/B states) from pose-driven shading
// changes, which 1-second-apart SCREENSHOT sampling fundamentally cannot distinguish.
static int g_burstFramesRemaining = 0;
static int g_burstFrameIndex = 0;

// Deferred key-up: hold key down for 2 frames so per-frame sampling detects the press
static int s_pendingKeyUpVK = 0;     // virtual key code waiting for key-up
static int s_pendingKeyUpFrames = 0; // frames remaining before sending WM_KEYUP

// Injected key press for terrain key system — consumed by GGTerrainWicked_Update()
int g_autoHarnessInjectedKey = 0; // VK code of key to inject, 0 = none

static void AutoHarness_InitPaths(void)
{
	// Get the directory containing the exe
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	// Strip the exe filename to get directory
	char* lastSlash = strrchr(exePath, '\\');
	if (!lastSlash) lastSlash = strrchr(exePath, '/');
	if (lastSlash) lastSlash[1] = 0; // keep trailing slash
	else strcpy(exePath, ".\\");

	_snprintf(s_cmdPath, MAX_PATH, "%sauto_command.txt", exePath);
	_snprintf(s_rspPath, MAX_PATH, "%sauto_result.txt", exePath);
	_snprintf(s_logPath, MAX_PATH, "%sauto_log.txt", exePath);
	s_cmdPath[MAX_PATH - 1] = 0;
	s_rspPath[MAX_PATH - 1] = 0;
	s_logPath[MAX_PATH - 1] = 0;
}

// ---- Internal helpers ----

static void AutoHarness_WriteResult(const char* result)
{
	// Write result file (overwrite)
	FILE* f = fopen(s_rspPath, "w");
	if (f)
	{
		fputs(result, f);
		fclose(f);
	}

	// Append to log with timestamp
	f = fopen(s_logPath, "a");
	if (f)
	{
		time_t now = time(NULL);
		struct tm* lt = localtime(&now);
		char timebuf[64];
		strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", lt);
		fprintf(f, "[%s] RESULT:\n%s\n---\n", timebuf, result);
		fclose(f);
	}
}

static void AutoHarness_LogCommand(const char* cmd)
{
	FILE* f = fopen(s_logPath, "a");
	if (f)
	{
		time_t now = time(NULL);
		struct tm* lt = localtime(&now);
		char timebuf[64];
		strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", lt);
		fprintf(f, "[%s] COMMAND: %s\n", timebuf, cmd);
		fclose(f);
	}
}

// Get current app state as a string
static const char* AutoHarness_GetAppState(void)
{
	if (!g_bNoGGUntilGameGuruMainCalled)
		return "initializing";
	// 2026-08-05: standalone game mode (hub PLAY GAME relaunches the exe with project=2,
	// app identity Guru-Game) — report the title menu / level phases distinctly
	if (t.game.gameisexe == 1)
	{
		if (t.game.titleloop == 1)
			return "standalone_title";
		if (t.game.gameloop == 1 || t.game.levelloop == 1)
			return "standalone_playing";
		return "standalone_loading";
	}
	if (bImGuiInTestGame)
		return "game";
	if (bWelcomeScreen_Window)
		return "hub";
	if (bStoryboardWindow)
		return "storyboard";
	if (iLaunchAfterSync >= 200 && iLaunchAfterSync <= 210)
		return "loading";
	return "editor";
}

// Get current tab name
static const char* AutoHarness_GetTabName(int tabId)
{
	switch (tabId)
	{
		case 0: return "demo_games";
		case 1: return "my_games";
		case 3: return "tutorials";
		case 4: return "user_guide";
		case 5: return "live_changelog";
		case 6: return "workshop_uploader";
		case 7: return "workshop";
		case 42: return "community_tutorials";
		default: return "unknown";
	}
}

// Get visible ImGui windows
static int AutoHarness_GetVisibleWindows(char* outBuf, int bufSize)
{
	outBuf[0] = 0;
	int written = 0;
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (!ctx) return 0;

	bool first = true;
	for (int i = 0; i < ctx->Windows.Size; i++)
	{
		ImGuiWindow* win = ctx->Windows[i];
		if (win && win->Active && !win->Hidden && win->Name && win->Name[0] != '#')
		{
			int needed = (int)strlen(win->Name) + 2; // comma + space
			if (written + needed >= bufSize - 1) break;
			if (!first)
			{
				outBuf[written++] = ',';
				outBuf[written++] = ' ';
			}
			int len = (int)strlen(win->Name);
			memcpy(outBuf + written, win->Name, len);
			written += len;
			first = false;
		}
	}
	outBuf[written] = 0;
	return written;
}

// ---- Command handlers ----

// We need to read iCurrentOpenTab from part16 - it's static local, so we track it via
// a global that the Welcome_Screen can write to
static int s_lastKnownTab = 0;
// This will be set by Welcome_Screen when it runs
int g_iAutoCurrentTab = 0;

static const char* AutoHarness_NodeTypeName(int type)
{
	switch (type)
	{
		case STORYBOARD_TYPE_NONE: return "none";
		case STORYBOARD_TYPE_SPLASH: return "splash";
		case STORYBOARD_TYPE_SCREEN: return "screen";
		case STORYBOARD_TYPE_LEVEL: return "level";
		case STORYBOARD_TYPE_HUD: return "hud";
		default: return "unknown";
	}
}

// Build a summary of storyboard nodes for GET_STATE
static int AutoHarness_GetStoryboardSummary(char* outBuf, int bufSize)
{
	int written = 0;
	if (!bStoryboardWindow) return 0;

	// Game name
	written += _snprintf(outBuf + written, bufSize - written,
		"PROJECT: %s\n", Storyboard.gamename[0] ? Storyboard.gamename : "(none)");

	// Count and list used nodes
	int nodeCount = 0;
	for (int i = 0; i < STORYBOARD_MAXNODES; i++)
		if (Storyboard.Nodes[i].used) nodeCount++;

	written += _snprintf(outBuf + written, bufSize - written,
		"NODES(%d):\n", nodeCount);

	for (int i = 0; i < STORYBOARD_MAXNODES && written < bufSize - 256; i++)
	{
		if (!Storyboard.Nodes[i].used) continue;
		const StoryboardNodesStruct& n = Storyboard.Nodes[i];
		written += _snprintf(outBuf + written, bufSize - written,
			"  [%d] type=%s title=\"%s\" level=\"%s\"",
			i, AutoHarness_NodeTypeName(n.type),
			n.title[0] ? n.title : "",
			n.level_name[0] ? n.level_name : "");

		// Show output connections
		for (int o = 0; o < STORYBOARD_MAXOUTPUTS; o++)
		{
			if (n.output_title[o][0] && written < bufSize - 128)
			{
				written += _snprintf(outBuf + written, bufSize - written,
					" out%d=\"%s\"->%d", o, n.output_title[o], n.output_linkto[o]);
			}
		}
		if (written < bufSize - 2)
			outBuf[written++] = '\n';
	}

	outBuf[written] = 0;
	return written;
}

static void Cmd_GetState(char* result, int resultSize)
{
	const char* state = AutoHarness_GetAppState();

	const char* tabName = "none";
	if (bWelcomeScreen_Window)
		tabName = AutoHarness_GetTabName(g_iAutoCurrentTab);

	char panels[4096];
	AutoHarness_GetVisibleWindows(panels, sizeof(panels));
	if (panels[0] == 0)
		strcpy(panels, "none");

	DWORD uptime = (GetTickCount() - s_startTick) / 1000;

	int written = _snprintf(result, resultSize,
		"STATE: %s\n"
		"TAB: %s\n"
		"VISIBLE_PANELS: %s\n"
		"ERRORS: none\n"
		"UPTIME: %lu\n",
		state, tabName, panels, uptime);
	if (written < 0) written = 0;

	// 2026-08-05: raw phase flags for standalone-game soak scripting
	if (t.game.gameisexe == 1 && written < resultSize - 1)
	{
		int w2 = _snprintf(result + written, resultSize - written,
			"STANDALONE: titleloop=%d gameloop=%d levelloop=%d masterloop=%d\n",
			(int)t.game.titleloop, (int)t.game.gameloop, (int)t.game.levelloop, (int)t.game.masterloop);
		if (w2 > 0) written += w2;
	}

	// Append storyboard details when in storyboard state
	if (bStoryboardWindow && written < resultSize - 1)
	{
		AutoHarness_GetStoryboardSummary(result + written, resultSize - written);
	}

	result[resultSize - 1] = 0;
}

static void Cmd_Navigate(const char* target, char* result, int resultSize)
{
	if (!target || !target[0])
	{
		_snprintf(result, resultSize, "ERROR: NAVIGATE requires a target argument");
		result[resultSize - 1] = 0;
		return;
	}

	// Navigate to hub
	if (_stricmp(target, "hub") == 0)
	{
		bWelcomeScreen_Window = true;
		bStoryboardWindow = false;
		_snprintf(result, resultSize, "OK: Navigated to hub");
		result[resultSize - 1] = 0;
		return;
	}

	// Navigate to hub tabs
	if (_strnicmp(target, "hub.", 4) == 0)
	{
		const char* tabName = target + 4;
		int tabId = -1;

		if (_stricmp(tabName, "demo_games") == 0) tabId = 0;
		else if (_stricmp(tabName, "my_games") == 0) tabId = 1;
		else if (_stricmp(tabName, "tutorials") == 0) tabId = 3;
		else if (_stricmp(tabName, "user_guide") == 0) tabId = 4;
		else if (_stricmp(tabName, "live_changelog") == 0) tabId = 5;
		else if (_stricmp(tabName, "community_tutorials") == 0) tabId = 42;
		else if (_stricmp(tabName, "workshop_uploader") == 0) tabId = 6;
		else if (_stricmp(tabName, "workshop") == 0) tabId = 7;

		if (tabId < 0)
		{
			_snprintf(result, resultSize, "ERROR: Unknown hub tab '%s'", tabName);
			result[resultSize - 1] = 0;
			return;
		}

		bWelcomeScreen_Window = true;
		bStoryboardWindow = false;
		g_iAutoForceWelcomeTab = tabId;

		_snprintf(result, resultSize, "OK: Navigated to hub.%s (tab %d)", tabName, tabId);
		result[resultSize - 1] = 0;
		return;
	}

	// Navigate to storyboard
	if (_stricmp(target, "storyboard") == 0)
	{
		bStoryboardWindow = true;
		bWelcomeScreen_Window = false;
		_snprintf(result, resultSize, "OK: Navigated to storyboard");
		result[resultSize - 1] = 0;
		return;
	}

	_snprintf(result, resultSize, "ERROR: Unknown navigation target '%s'", target);
	result[resultSize - 1] = 0;
}

static void Cmd_Click(const char* element, char* result, int resultSize)
{
	if (!element || !element[0])
	{
		_snprintf(result, resultSize, "ERROR: CLICK requires an element argument");
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "play_game") == 0)
	{
		// Trigger play game from storyboard
		if (bStoryboardWindow)
		{
			iStoryboardExecuteKey = ' ';
			_snprintf(result, resultSize, "OK: Triggered Play Game from storyboard");
		}
		else if (bWelcomeScreen_Window)
		{
			// 2026-08-05: hub PLAY GAME (the button next to EDIT GAME on a selected demo
			// card). For project-backed demos this RELAUNCHES the exe as a standalone game
			// (project=2) and THIS process exits — expect the harness to go silent and a
			// fresh GameGuruMAX.exe (app identity Guru-Game) to appear.
			extern bool bTriggerPlayDemoGame;
			bTriggerPlayDemoGame = true;
			_snprintf(result, resultSize, "OK: Triggered Play Game from hub (demo games); process may relaunch as standalone");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: Not in storyboard or hub view, cannot Play Game");
		}
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "edit_game") == 0)
	{
		if (bWelcomeScreen_Window)
		{
			// Trigger edit from Demo Games tab (same as double-click)
			bTriggerEditDemoGame = true;
			_snprintf(result, resultSize, "OK: Triggered Edit Game from hub (demo games)");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: edit_game only works from hub. Use CLICK_NODE <title> to open a level from the storyboard");
		}
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "test_level") == 0)
	{
		const char* state = AutoHarness_GetAppState();
		if (strcmp(state, "editor") == 0)
		{
			iLaunchAfterSync = 1;
			_snprintf(result, resultSize, "OK: Triggered Test Level from editor");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: test_level only works from editor (current state: %s)", state);
		}
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "new_project") == 0)
	{
		// New project - only from hub
		_snprintf(result, resultSize, "ERROR: new_project not yet implemented via automation");
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "add_level") == 0)
	{
		if (bStoryboardWindow)
		{
			iStoryboardExecuteKey = 'N';
			_snprintf(result, resultSize, "OK: Triggered Add New Level");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: Not in storyboard view");
		}
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "load_level") == 0)
	{
		if (bStoryboardWindow)
		{
			iStoryboardExecuteKey = 'L';
			_snprintf(result, resultSize, "OK: Triggered Load Level");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: Not in storyboard view");
		}
		result[resultSize - 1] = 0;
		return;
	}

	if (_stricmp(element, "exit_screen_editor") == 0)
	{
		if (bScreen_Editor_Window)
		{
			g_iAutoExitScreenEditor = 1;
			_snprintf(result, resultSize, "OK: Triggered Exit to Storyboard from screen editor");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: Screen editor is not open");
		}
		result[resultSize - 1] = 0;
		return;
	}

	_snprintf(result, resultSize, "ERROR: Unknown click element '%s'", element);
	result[resultSize - 1] = 0;
}

static void Cmd_ListDemos(char* result, int resultSize)
{
	if (g_LibraryFileList.size() == 0)
	{
		_snprintf(result, resultSize, "OK: DEMOS(0):\n(none loaded)");
		result[resultSize - 1] = 0;
		return;
	}

	int written = _snprintf(result, resultSize, "OK: DEMOS(%d):\n", (int)g_LibraryFileList.size());
	if (written < 0) written = 0;

	for (int i = 0; i < (int)g_LibraryFileList.size(); i++)
	{
		const char* name = g_LibraryFileList[i].cName.Get();
		// Strip .png extension for display
		char displayName[260];
		strncpy(displayName, name, sizeof(displayName) - 1);
		displayName[sizeof(displayName) - 1] = 0;
		char* dot = strrchr(displayName, '.');
		if (dot) *dot = 0;

		int needed = (int)strlen(displayName) + 4; // "  N\n"
		if (written + needed >= resultSize - 1) break;
		written += _snprintf(result + written, resultSize - written, "  %s\n", displayName);
	}
	result[resultSize - 1] = 0;
}

static void Cmd_SelectDemo(const char* demoName, char* result, int resultSize)
{
	if (!demoName || !demoName[0])
	{
		_snprintf(result, resultSize, "ERROR: SELECT_DEMO requires a demo name argument");
		result[resultSize - 1] = 0;
		return;
	}

	if (!bWelcomeScreen_Window)
	{
		_snprintf(result, resultSize, "ERROR: Not on hub screen, cannot select demo");
		result[resultSize - 1] = 0;
		return;
	}

	// Search g_LibraryFileList for a case-insensitive match (by display name, without extension)
	for (int i = 0; i < (int)g_LibraryFileList.size(); i++)
	{
		const char* name = g_LibraryFileList[i].cName.Get();
		char displayName[260];
		strncpy(displayName, name, sizeof(displayName) - 1);
		displayName[sizeof(displayName) - 1] = 0;
		char* dot = strrchr(displayName, '.');
		if (dot) *dot = 0;

		if (_stricmp(displayName, demoName) == 0)
		{
			// Found it - set the global that Welcome_Screen will consume
			strncpy(g_sAutoSelectDemo, name, sizeof(g_sAutoSelectDemo) - 1);
			g_sAutoSelectDemo[sizeof(g_sAutoSelectDemo) - 1] = 0;

			// Also force to Demo Games tab
			g_iAutoForceWelcomeTab = 0;

			_snprintf(result, resultSize, "OK: Selected demo '%s' (file: %s)", displayName, name);
			result[resultSize - 1] = 0;
			return;
		}
	}

	_snprintf(result, resultSize, "ERROR: Demo '%s' not found in library (%d demos loaded)", demoName, (int)g_LibraryFileList.size());
	result[resultSize - 1] = 0;
}

// ---------------------------------------------------------------------------
// My Games project handlers
// ---------------------------------------------------------------------------
// The My Games tab drives a different code path than demo_games. When the user
// double-clicks a project card at M-GridEditB_part16.cpp:1044, the handler sets
// TriggerLoadGameProject + toggles bWelcomeScreen_Window / bStoryboardWindow.
// process_storeboard() then picks up TriggerLoadGameProject at
// M-GridEditB_part19.cpp:202 and calls load_storyboard() on the next frame.
// We replicate that here — see project memory "harness-open-my-games".

static void Cmd_ListProjects(char* result, int resultSize)
{
	int written = _snprintf(result, resultSize, "PROJECT_COUNT: %d\n", (int)projectbank_list.size());
	if (projectbank_list.empty())
	{
		written += _snprintf(result + written, resultSize - written,
			"NOTE: list not yet populated — call GET_STATE after the hub has rendered at least once\n");
	}
	for (int i = 0; i < (int)projectbank_list.size() && written < resultSize - 128; i++)
	{
		written += _snprintf(result + written, resultSize - written,
			"  [%d] %s\n", i, projectbank_list[i].c_str());
	}
	result[resultSize - 1] = 0;
}

static void Cmd_OpenProject(const char* projectName, char* result, int resultSize)
{
	if (!projectName || !projectName[0])
	{
		_snprintf(result, resultSize, "ERROR: OPEN_PROJECT requires a project name argument");
		result[resultSize - 1] = 0;
		return;
	}

	if (!bWelcomeScreen_Window)
	{
		_snprintf(result, resultSize, "ERROR: OPEN_PROJECT must be run from hub (welcome screen not visible)");
		result[resultSize - 1] = 0;
		return;
	}

	if (projectbank_list.empty())
	{
		_snprintf(result, resultSize,
			"ERROR: projectbank_list is empty — the hub renderer populates it on first frame; try GET_STATE once then retry");
		result[resultSize - 1] = 0;
		return;
	}

	// Case-insensitive name match against the projectbank folder list.
	int foundIndex = -1;
	for (int i = 0; i < (int)projectbank_list.size(); i++)
	{
		if (_stricmp(projectbank_list[i].c_str(), projectName) == 0)
		{
			foundIndex = i;
			break;
		}
	}

	if (foundIndex < 0)
	{
		int written = _snprintf(result, resultSize,
			"ERROR: project '%s' not found (%d available):", projectName, (int)projectbank_list.size());
		for (int i = 0; i < (int)projectbank_list.size() && written < resultSize - 64; i++)
		{
			written += _snprintf(result + written, resultSize - written, " '%s'", projectbank_list[i].c_str());
		}
		result[resultSize - 1] = 0;
		return;
	}

	// Copy the resolved name into a static buffer so TriggerLoadGameProject (cstr) has a
	// stable const char* to consume even if projectbank_list re-sorts before the next
	// frame's process_storeboard() picks up the trigger.
	static char s_openProjectName[260];
	strncpy(s_openProjectName, projectbank_list[foundIndex].c_str(), sizeof(s_openProjectName) - 1);
	s_openProjectName[sizeof(s_openProjectName) - 1] = 0;

	// Replicate the double-click handler at M-GridEditB_part16.cpp:1121:
	//   TriggerLoadGameProject = <name>;
	//   bWelcomeScreen_Window = false;
	//   bStoryboardWindow = true;
	TriggerLoadGameProject = s_openProjectName;
	bWelcomeScreen_Window = false;
	bStoryboardWindow = true;

	_snprintf(result, resultSize,
		"OK: Opening project '%s' (index %d of %d) — storyboard will render on next frame",
		s_openProjectName, foundIndex, (int)projectbank_list.size());
	result[resultSize - 1] = 0;
}

// Convenience: click the FIRST storyboard node whose type=level and level_name
// is non-empty. TESTPROJ1 style — one level, don't want to require the caller
// to know its exact title.
static void Cmd_ClickOnlyLevel(char* result, int resultSize)
{
	if (!bStoryboardWindow)
	{
		_snprintf(result, resultSize, "ERROR: CLICK_ONLY_LEVEL must be run from storyboard view");
		result[resultSize - 1] = 0;
		return;
	}

	for (int i = 0; i < STORYBOARD_MAXNODES; i++)
	{
		if (!Storyboard.Nodes[i].used) continue;
		const StoryboardNodesStruct& n = Storyboard.Nodes[i];
		if (n.type != STORYBOARD_TYPE_LEVEL) continue;
		if (strlen(n.level_name) == 0) continue;

		// Mirror Cmd_ClickNode's level-load path.
		strcpy(cDirectOpen, n.level_name);
		iLaunchAfterSync = 7;
		iSkibFramesBeforeLaunch = 5;
		bCloseStoryboardAfterLoad = true;
		iLevelEditorFromStoryboardID = i;

		_snprintf(result, resultSize,
			"OK: Loading first level node '%s' (file: %s, index: %d)",
			n.title, n.level_name, i);
		result[resultSize - 1] = 0;
		return;
	}

	_snprintf(result, resultSize, "ERROR: No level node with an assigned level file found in storyboard");
	result[resultSize - 1] = 0;
}

static void Cmd_Wait(const char* msStr, char* result, int resultSize)
{
	int ms = 0;
	if (msStr && msStr[0])
		ms = atoi(msStr);
	if (ms > 0 && ms <= 30000)
		Sleep((DWORD)ms);

	// Return state after wait
	Cmd_GetState(result, resultSize);
}

static void Cmd_Screenshot(char* result, int resultSize)
{
	// Use WickedEngine screenshot API
	std::string outPath = wi::helper::screenshot(master.swapChain, "auto_screenshot");
	if (outPath.empty())
	{
		_snprintf(result, resultSize, "ERROR: Screenshot failed");
	}
	else
	{
		_snprintf(result, resultSize, "OK: Screenshot saved to %s", outPath.c_str());
	}
	result[resultSize - 1] = 0;
}

static void Cmd_GetScreenText(char* result, int resultSize)
{
	int written = 0;
	const char* state = AutoHarness_GetAppState();
	written += _snprintf(result + written, resultSize - written, "STATE: %s\n", state);

	if (bStoryboardWindow)
	{
		written += _snprintf(result + written, resultSize - written,
			"PROJECT: %s\n"
			"DESCRIPTION: %.200s\n"
			"READONLY: %d\n",
			Storyboard.gamename[0] ? Storyboard.gamename : "(none)",
			Storyboard.game_description[0] ? Storyboard.game_description : "",
			Storyboard.project_readonly);

		// Detailed node dump
		for (int i = 0; i < STORYBOARD_MAXNODES && written < resultSize - 512; i++)
		{
			if (!Storyboard.Nodes[i].used) continue;
			const StoryboardNodesStruct& n = Storyboard.Nodes[i];
			written += _snprintf(result + written, resultSize - written,
				"NODE[%d]: type=%s title=\"%s\" level=\"%s\" levelnumber=\"%s\" editable=%d\n",
				i, AutoHarness_NodeTypeName(n.type),
				n.title[0] ? n.title : "",
				n.level_name[0] ? n.level_name : "",
				n.levelnumber[0] ? n.levelnumber : "",
				n.iEditEnable);

			// Screen-specific info
			if (n.type == STORYBOARD_TYPE_SCREEN || n.type == STORYBOARD_TYPE_SPLASH)
			{
				if (n.screen_title[0] && written < resultSize - 256)
					written += _snprintf(result + written, resultSize - written,
						"  screen_title=\"%s\"\n", n.screen_title);
			}

			// Output pins
			for (int o = 0; o < STORYBOARD_MAXOUTPUTS && written < resultSize - 256; o++)
			{
				if (n.output_title[o][0])
					written += _snprintf(result + written, resultSize - written,
						"  out[%d]: \"%s\" action=\"%s\" linkto=%d\n",
						o, n.output_title[o], n.output_action[o], n.output_linkto[o]);
			}

			// Input pins
			for (int inp = 0; inp < STORYBOARD_MAXOUTPUTS && written < resultSize - 256; inp++)
			{
				if (n.input_title[inp][0])
					written += _snprintf(result + written, resultSize - written,
						"  in[%d]: \"%s\"\n", inp, n.input_title[inp]);
			}

			// Widgets with labels
			for (int w = 0; w < STORYBOARD_MAXWIDGETS && written < resultSize - 256; w++)
			{
				if (n.widget_used[w] && n.widget_label[w][0])
					written += _snprintf(result + written, resultSize - written,
						"  widget[%d]: label=\"%s\" type=%d\n",
						w, n.widget_label[w], n.widget_type[w]);
			}
		}
	}
	else if (bWelcomeScreen_Window)
	{
		written += _snprintf(result + written, resultSize - written,
			"TAB: %s\n", AutoHarness_GetTabName(g_iAutoCurrentTab));

		// List demos if on demo tab
		if (g_iAutoCurrentTab == 0 && g_LibraryFileList.size() > 0)
		{
			written += _snprintf(result + written, resultSize - written, "DEMOS:\n");
			for (int i = 0; i < (int)g_LibraryFileList.size() && written < resultSize - 256; i++)
			{
				const char* name = g_LibraryFileList[i].cName.Get();
				written += _snprintf(result + written, resultSize - written, "  %s\n", name);
			}
		}
	}
	else
	{
		// Editor state - list visible panels
		char panels[4096];
		AutoHarness_GetVisibleWindows(panels, sizeof(panels));
		written += _snprintf(result + written, resultSize - written,
			"PANELS: %s\n", panels[0] ? panels : "none");

		// Enumerate toolbar buttons with their tooltip labels and toggle states
		ImGuiWindow* toolbarWin = ImGui::FindWindowByName("Toolbar");
		if (toolbarWin && toolbarWin->Active)
		{
			written += _snprintf(result + written, resultSize - written, "TOOLBAR_BUTTONS:\n");
			written += _snprintf(result + written, resultSize - written, "  Back to Game Project Storyboard\n");
			written += _snprintf(result + written, resultSize - written, "  Save Level\n");
			written += _snprintf(result + written, resultSize - written, "  Test Level\n");
			if (g.gvrmode > 0 && g.gvrmodefordevelopers == 1)
				written += _snprintf(result + written, resultSize - written, "  Test Level in VR\n");
			if (t.visuals.bEnableEmptyLevelMode == false)
				written += _snprintf(result + written, resultSize - written, "  Terrain, Painting, Trees and Vegetation [active=%d]\n", bTerrain_Tools_Window ? 1 : 0);
			written += _snprintf(result + written, resultSize - written, "  Object Tools [active=%d]\n", Entity_Tools_Window ? 1 : 0);
			written += _snprintf(result + written, resultSize - written, "  Visual Logic Connections [active=%d]\n", Shooter_Tools_Window ? 1 : 0);
			written += _snprintf(result + written, resultSize - written, "  Environment Effects [active=%d]\n", Visuals_Tools_Window ? 1 : 0);
			written += _snprintf(result + written, resultSize - written, "  Game Settings [active=%d]\n", Game_Settings_Window ? 1 : 0);
			written += _snprintf(result + written, resultSize - written, "  Editor Light [active=%d]\n", bEditorLight ? 1 : 0);
			written += _snprintf(result + written, resultSize - written, "  Camera View\n");
		}

		// Menu bar items
		if (toolbarWin && toolbarWin->Active)
		{
			written += _snprintf(result + written, resultSize - written, "MENU_BAR: File, Edit, Tools, Help\n");
		}
	}

	result[resultSize - 1] = 0;
}

static void Cmd_ClickNode(const char* nodeTitle, char* result, int resultSize)
{
	if (!nodeTitle || !nodeTitle[0])
	{
		_snprintf(result, resultSize, "ERROR: CLICK_NODE requires a node title argument");
		result[resultSize - 1] = 0;
		return;
	}

	if (!bStoryboardWindow)
	{
		_snprintf(result, resultSize, "ERROR: Not in storyboard view, cannot click node");
		result[resultSize - 1] = 0;
		return;
	}

	// Find the node by title (case-insensitive)
	for (int i = 0; i < STORYBOARD_MAXNODES; i++)
	{
		if (!Storyboard.Nodes[i].used) continue;
		if (_stricmp(Storyboard.Nodes[i].title, nodeTitle) != 0) continue;

		const StoryboardNodesStruct& n = Storyboard.Nodes[i];

		if (n.type == STORYBOARD_TYPE_LEVEL && strlen(n.level_name) > 0)
		{
			// Load level and exit to editor (mirrors storyboard click logic)
			strcpy(cDirectOpen, n.level_name);
			iLaunchAfterSync = 7; // Direct load
			iSkibFramesBeforeLaunch = 5;
			bCloseStoryboardAfterLoad = true;
			iLevelEditorFromStoryboardID = i;
			_snprintf(result, resultSize, "OK: Loading level node '%s' (file: %s)", n.title, n.level_name);
		}
		else if (n.type == STORYBOARD_TYPE_LEVEL)
		{
			// GGMAX 2.49: an empty level node opens the TERRAIN GENERATOR in the real UI — mirror
			// the storyboard's own recipe (M-GridEditB_part19.cpp:2629-2659) instead of erroring,
			// so the generator flow (no-preview + Generate-crash bugs) is drivable headlessly.
			extern bool bForceKey; extern cstr csForceKey;
			extern bool bTerrain_Tools_Window; extern bool Entity_Tools_Window;
			extern bool bProceduralLevelFromStoryboard;
			extern int iBlackoutForFrames, iWaitForNewLevel, iNewLevelNode;
			extern char cTriggerMessage[]; extern bool bTriggerMessage;
			extern void imgui_populatecustombiomes(void);
			bForceKey = true; csForceKey = "o";
			bTerrain_Tools_Window = false; Entity_Tools_Window = true;
			bProceduralLevelFromStoryboard = true;
			{ extern bool g_ggTerrainGenEntryPending; g_ggTerrainGenEntryPending = true; } // GGMAX 2.59: keep in sync with the part19 recipe
			iLaunchAfterSync = 5;
			iBlackoutForFrames = 5;
			iSkibFramesBeforeLaunch = 2;
			iWaitForNewLevel = 10;
			iNewLevelNode = i;
			GG_SetWritablesToRoot(true);
			if (FileExist("thumbbank\\lastnewlevel.jpg")) DeleteAFile("thumbbank\\lastnewlevel.jpg");
			GG_SetWritablesToRoot(false);
			strcpy(cTriggerMessage, "Preparing the Terrain Generator. Please wait...");
			bTriggerMessage = true;
			imgui_populatecustombiomes();
			_snprintf(result, resultSize, "OK: node '%s' has no level file — opening the Terrain Generator (storyboard recipe)", n.title);
		}
		else if (n.type == STORYBOARD_TYPE_SCREEN || n.type == STORYBOARD_TYPE_SPLASH)
		{
			// Open the screen editor for this node
			bScreen_Editor_Window = true;
			iScreen_Editor_Node = i;
			_snprintf(result, resultSize, "OK: Opened screen editor for node '%s' (type=%s, index=%d)",
				n.title, AutoHarness_NodeTypeName(n.type), i);
		}
		else
		{
			_snprintf(result, resultSize, "OK: Clicked node '%s' (type=%s) - note: only level/screen nodes trigger actions",
				n.title, AutoHarness_NodeTypeName(n.type));
		}
		result[resultSize - 1] = 0;
		return;
	}

	// GGMAX 2.63: list the LEVEL nodes that DO exist — a probe hunting a renamed or
	// level-assigned node gets the answer in one round trip instead of guessing titles
	// (cost a blind evening round when the user's own session mutated the project).
	{
		int written3 = _snprintf(result, resultSize, "ERROR: Node '%s' not found in storyboard. Level nodes:", nodeTitle);
		for (int i = 0; i < STORYBOARD_MAXNODES && written3 > 0 && written3 < resultSize - 96; i++)
		{
			if (!Storyboard.Nodes[i].used) continue;
			if (Storyboard.Nodes[i].type != STORYBOARD_TYPE_LEVEL) continue;
			written3 += _snprintf(result + written3, resultSize - written3, " '%s'%s",
				Storyboard.Nodes[i].title,
				strlen(Storyboard.Nodes[i].level_name) > 0 ? "(has-level)" : "(EMPTY)");
		}
	}
	result[resultSize - 1] = 0;
}

// GGMAX 2.20: SU-Hierarchy load-balance instrument (see wiScene.cpp).
namespace wi::scene         { extern std::atomic<uint32_t> gg_hier_max_subtree; }
namespace wi::scene         { extern std::atomic<uint32_t> gg_hier_visited; }
extern uint32_t gg_elanim_total, gg_elanim_skip_noent, gg_elanim_skip_static, gg_elanim_work;   // GGMAX 3.16
extern uint32_t gg_elanim_ff_entities, gg_elanim_ff_sets;   // GGMAX 3.16
extern uint32_t gg_elanim_ff_rebuilds, gg_elanim_ff_hits;   // GGMAX 3.18
namespace wi::scene         { extern uint32_t gg_hier_rebuilds; extern uint32_t gg_hier_skips; extern int gg_hier_cache_snapshot; }   // GGMAX 3.14
namespace wi::scene         { extern uint32_t gg_hier_root_count; }

static void Cmd_GetPerfData(char* result, int resultSize)
{
	int written = 0;
	const char* state = AutoHarness_GetAppState();

	// FPS and frame time
	// FIX 2026-07-25: guard the ImGui context — a GET_PERF_DATA arriving during startup or
	// shutdown crashed here (GetIO() derefs the null global context; recurring 0xc0000005 at
	// this line across several days of Guru-Crash.log entries — the "silent" harness deaths).
	float fps = 0.0f;
	if (ImGui::GetCurrentContext() != nullptr)
	{
		fps = ImGui::GetIO().Framerate;
	}
	float frameTimeMs = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;
	written += _snprintf(result + written, resultSize - written,
		"STATE: %s\n"
		"FPS: %.1f\n"
		"FRAME_TIME_MS: %.2f\n",
		state, fps, frameTimeMs);

	// the lights in view this frame and how many the entity array took (the light pool: 1024 slots, nearest first)
	written += _snprintf(result + written, resultSize - written,
		"LIGHTS_VISIBLE: %u\n"
		"LIGHTS_PACKED: %u\n",
		wi::renderer::gg_lights_visible, wi::renderer::gg_lights_packed);

	// System memory (MB)
	int memMB = SMEMAvailable(1);
	float memGB = (float)memMB / 1024.0f;
	written += _snprintf(result + written, resultSize - written,
		"SYSTEM_MEM_MB: %d\n"
		"SYSTEM_MEM_GB: %.2f\n",
		memMB, memGB);

	// VRAM (MB)
	float vramMB = GetTotalVramUsage();
	written += _snprintf(result + written, resultSize - written,
		"VRAM_MB: %.1f\n"
		"VRAM_GB: %.2f\n",
		vramMB, vramMB / 1024.0f);

	// GPU adapter name
	auto* device = wi::graphics::GetDevice();
	if (device)
	{
		written += _snprintf(result + written, resultSize - written,
			"GPU_ADAPTER: %s\n", device->GetAdapterName().c_str());
	}

	// Scene component counts
	wi::scene::Scene* pScene = master.masterrenderer.scene;
	if (pScene && written < resultSize - 512)
	{
		written += _snprintf(result + written, resultSize - written,
			"SCENE_OBJECTS: %d\n"
			"SCENE_MESHES: %d\n"
			"SCENE_MATERIALS: %d\n"
			"SCENE_LIGHTS: %d\n"
			"SCENE_TRANSFORMS: %d\n"
			"SCENE_CAMERAS: %d\n"
			"SCENE_EMITTERS: %d\n"
			"SCENE_HAIRS: %d\n"
			"SCENE_ANIMATIONS: %d\n"
			"SCENE_ARMATURES: %d\n"
			"SCENE_DECALS: %d\n"
			"SCENE_PROBES: %d\n"
			"SCENE_SOUNDS: %d\n"
			"SCENE_COLLIDERS: %d\n"
			"SCENE_RIGIDBODIES: %d\n"
			"SCENE_SOFTBODIES: %d\n"
			"SCENE_SCRIPTS: %d\n"
			"SCENE_HIERARCHY: %d\n"
			"SCENE_WEATHERS: %d\n",
			(int)pScene->objects.GetCount(),
			(int)pScene->meshes.GetCount(),
			(int)pScene->materials.GetCount(),
			(int)pScene->lights.GetCount(),
			(int)pScene->transforms.GetCount(),
			(int)pScene->cameras.GetCount(),
			(int)pScene->emitters.GetCount(),
			(int)pScene->hairs.GetCount(),
			(int)pScene->animations.GetCount(),
			(int)pScene->armatures.GetCount(),
			(int)pScene->decals.GetCount(),
			(int)pScene->probes.GetCount(),
			(int)pScene->sounds.GetCount(),
			(int)pScene->colliders.GetCount(),
			(int)pScene->rigidbodies.GetCount(),
			(int)pScene->softbodies.GetCount(),
			(int)pScene->scripts.GetCount(),
			(int)pScene->hierarchy.GetCount(),
			(int)pScene->weathers.GetCount());
	}

	// GGMAX 2.20: SU-Hierarchy load balance. The fast path Dispatches one job per subtree ROOT,
	// so the system's wall clock is the LARGEST SINGLE SUBTREE, not the total. imbalance = the
	// biggest job's share of all nodes walked: near 1.0 means one subtree IS the system and the
	// other workers idle; near roots^-1 means the work is evenly spread and the shape is right.
	// This is what decides whether SU-Hierarchy's remaining ~0.6 ms is a scheduling problem or
	// genuine aggregate per-entity cost. See SWITCHESCAPE_PERF.md §13.
	if (written < resultSize - 256)
	{
		const uint32_t hroots = wi::scene::gg_hier_root_count;
		const uint32_t hmax   = wi::scene::gg_hier_max_subtree.load(std::memory_order_relaxed);
		const uint32_t hvis   = wi::scene::gg_hier_visited.load(std::memory_order_relaxed);
		written += _snprintf(result + written, resultSize - written,
			"HIER: roots=%u maxSubtree=%u visited=%u imbalance=%.3f rebuilds=%u skips=%u\n",
			hroots, hmax, hvis, hvis ? (double)hmax / (double)hvis : 0.0,
			wi::scene::gg_hier_rebuilds, wi::scene::gg_hier_skips);
	}

	// GGMAX 3.16: entity_loopanim shape - is the cost the WORK, or just walking every element?
	if (written < resultSize - 160)
	{
		written += _snprintf(result + written, resultSize - written,
			"ELANIM: total=%u skipNoEnt=%u skipStatic=%u work=%u ffEnt=%u ffSets=%u ffRebuild=%u ffHit=%u\n",
			gg_elanim_total, gg_elanim_skip_noent, gg_elanim_skip_static, gg_elanim_work, gg_elanim_ff_entities, gg_elanim_ff_sets, gg_elanim_ff_rebuilds, gg_elanim_ff_hits);
	}

	// GGMAX 2.25: wi::terrain chunk ring size — the DX12 entity floor.
	//
	// (2*gen+1)^2 chunk entities, each a mesh + material + transform + hierarchy node walked by
	// Scene::Update every frame; DX11 has none of them (no wiTerrain at all — §16).
	//
	// ★ WHAT `gen` ACTUALLY IS: a VIEW DISTANCE, not map coverage. GG sets
	// SetCenterToCamEnabled(true) (GGTerrainWicked.cpp), and the engine recomputes center_chunk
	// from camera.Eye every frame (wiTerrain.cpp:776-780) — so the ring FOLLOWS THE CAMERA and
	// terrain always extends viewM in every direction from wherever the player is.
	// ⚠⚠ It therefore does NOT matter how big the editable map is: the map can never outrun the
	// ring. An earlier draft of this line printed `needGen = ceil(mapHalf/chunkU)` and flagged the
	// two 5 km demos as "cropped" — that was WRONG, it silently assumed an origin-centred ring.
	// Flying to x=90000 on A Grand Canyon Adventure (2286 m out, past any origin-centred reach)
	// shows solid ground. Do not reintroduce a coverage metric here.
	// ⚠ 1 unit = 1 inch; editable_size is the HALF-size. mapHalfM is context only.
	// ⚠ `chunks` can exceed ringMax after the camera moves: removal lags creation by
	// removal_threshold = gen + 2 + gg_removal_margin rings, so old chunks linger.
	if (written < resultSize - 256 && pScene != nullptr && pScene->terrains.GetCount() > 0)
	{
		const wi::terrain::Terrain& tr = pScene->terrains[0];
		const float chunkU = (float)(wi::terrain::chunk_width - 1) * tr.chunk_scale;
		const float mapHalfU = GGTerrain::ggterrain_global_render_params2.editable_size;
		const float viewU = (float)tr.generation * chunkU;
		const int   ringMax = (2 * tr.generation + 1) * (2 * tr.generation + 1);
		// GGMAX 2.53: ovr=1 means the ring is pinned to the Terrain Generator's editable-area
		// marker (world XZ shown) instead of the camera; must read 0 in every other mode.
		written += _snprintf(result + written, resultSize - written,
			"TERRAIN_RING: gen=%d chunks=%d ringMax=%d chunkU=%.0f viewU=%.0f viewM=%.0f "
			"centreToCam=%d mapHalfM=%.0f ovr=%d ovrX=%.0f ovrZ=%.0f pend=%d skipbvh=%d procLvl=%d "
			"emptyV=%d emptyG=%d emptyE=%d hidden=%d\n",
			tr.generation, (int)tr.chunks.size(), ringMax, chunkU,
			viewU, viewU * 0.0254f, tr.IsCenterToCamEnabled() ? 1 : 0, mapHalfU * 0.0254f,
			wi::terrain::gg_generation_center_override_enabled ? 1 : 0,
			wi::terrain::gg_generation_center_override_x,
			wi::terrain::gg_generation_center_override_z,
			[]{ extern bool g_ggTerrainGenEntryPending; return g_ggTerrainGenEntryPending ? 1 : 0; }(),
			wi::terrain::gg_generation_skip_bvh ? 1 : 0,
			[]{ extern bool bProceduralLevel; return bProceduralLevel ? 1 : 0; }(),
			// GGMAX 2.68h: the ssss10 storyboard-switch hunt — which COPY of the empty flag
			// is live after a level switch, and is the bridge actually hiding
			t.visuals.bEnableEmptyLevelMode ? 1 : 0,
			t.gamevisuals.bEnableEmptyLevelMode ? 1 : 0,
			t.editorvisuals.bEnableEmptyLevelMode ? 1 : 0,
			GGTerrain::GGTerrainWicked_IsTerrainHidden() ? 1 : 0);
	}

	// GGMAX 2.27: decal element pool — prewarm + grow (SWITCHESCAPE_PERF.md §23).
	// `built` is how many pool quads actually exist as ECS entities (the number that costs
	// per-frame Scene::Update work); `max` is the ceiling it may grow to. ★ READ THIS BEFORE
	// TRUSTING ANY decalprewarm A/B — it is the proof the setup.ini key reached the pool, the
	// same check that exposed the SET_TREES pool no-op (§2).
	if (written < resultSize - 256)
	{
		extern int g_decalBuilt, g_decalPrewarm, g_decalGrowDeferred;
		written += _snprintf(result + written, resultSize - written,
			"DECALPOOL: built=%d prewarm=%d max=%d deferred=%d\n",
			g_decalBuilt, g_decalPrewarm, (int)g.decalelementmax, g_decalGrowDeferred);
	}

	// GGMAX 2.28: directional shadow-caster count + the extrusion that produced it.
	// ★ This pair is the EXECUTED-CHECK for SET_SHADOWEXTRUDE. `casters` MUST move when the
	// extrusion changes; if it does not, the knob never reached the cull and any timing A/B
	// beside it is measuring nothing (the §24.2 trap, which cost three gate runs to learn).
	// ⚠ VISIBLE_OBJECTS is blind to this — it counts CAMERA visibility, not caster selection.
	if (written < resultSize - 256)
	{
		written += _snprintf(result + written, resultSize - written,
			"SHADOW_CASTERS: %u extrude=%.0f\n",
			wi::renderer::gg_dbg_shadow_casters.load(std::memory_order_relaxed),
			wi::renderer::gg_shadow_caster_extrude);
	}

	// Visibility counts from the main render pass
	if (written < resultSize - 256)
	{
		written += _snprintf(result + written, resultSize - written,
			"VISIBLE_OBJECTS: %d\n"
			"VISIBLE_LIGHTS: %d\n"
			"VISIBLE_DECALS: %d\n"
			"VISIBLE_ENVPROBES: %d\n"
			"VISIBLE_EMITTERS: %d\n"
			"VISIBLE_HAIRS: %d\n",
			(int)master.masterrenderer.visibility_main.visibleObjects.size(),
			(int)master.masterrenderer.visibility_main.visibleLights.size(),
			(int)master.masterrenderer.visibility_main.visibleDecals.size(),
			(int)master.masterrenderer.visibility_main.visibleEnvProbes.size(),
			(int)master.masterrenderer.visibility_main.visibleEmitters.size(),
			(int)master.masterrenderer.visibility_main.visibleHairs.size());
	}

	// Delayed (staggered) directional shadow cascades — should follow the project's Delayed Shadows
	// setting (OFF at HIGHEST quality). ON = staggered refresh (can lag/flicker shadows under camera
	// movement); OFF = every-frame cascades (stable).
	written += _snprintf(result + written, resultSize - written,
		"DELAYED_SHADOWS_ENGINE: %s\n",
		wi::renderer::GetDelayedShadowCascadesEnabled() ? "ON (staggered)" : "OFF (every-frame)");
	// Shadow LOD override: ON = terrain casts shadows at a per-cascade LOD that can oscillate vs the
	// visible chunk (the "two terrain shapes" flicker); OFF = shadows use the stable main-view LOD.
	written += _snprintf(result + written, resultSize - written,
		"SHADOW_LOD_OVERRIDE_ENGINE: %s\n",
		wi::renderer::IsShadowLODOverrideEnabled() ? "ON (per-cascade LOD, can flicker)" : "OFF (view LOD)");
	// Far-cascade caster cull (DX11 parity): ON = objects only shadow into the near cascades (far
	// cascades render terrain only) - trims the every-other-frame staggered-cascade CPU spike.
	written += _snprintf(result + written, resultSize - written,
		"SHADOW_FARCULL_ENGINE: %s\n",
		wi::renderer::GetShadowFarCascadeCull() ? "ON (near cascades only)" : "OFF (all cascades)");
	// GG Phase 1 point-light shadow budget (revived iShadowPointMax). GRANTED = local casters that won
	// an atlas shadow slot this frame; CAPPED = casters denied a slot (rendered fully lit, DX11-style);
	// RENDERED = local shadows actually re-drawn this frame. Budget -1 => uncapped (stock, all render).
	{
		int shGranted = 0, shCapped = 0, shRendered = 0;
		wi::renderer::GetLocalShadowStats(shGranted, shCapped, shRendered);
		written += _snprintf(result + written, resultSize - written,
			"SHADOW_LOCAL_GRANTED: %d\n"
			"SHADOW_LOCAL_CAPPED: %d\n"
			"SHADOW_LOCAL_RENDERED: %d\n"
			"SHADOW_LOCAL_CACHE_ENGINE: %s\n",
			shGranted, shCapped, shRendered,
			wi::renderer::GetLocalShadowCachingEnabled() ? "ON (static-cache)" : "OFF");
	}

	// Camera position and orientation (for diagnosing camera issues)
	{
		auto& cam = wiScene::GetCamera();
		written += _snprintf(result + written, resultSize - written,
			"CAMERA_EYE: %.1f, %.1f, %.1f\n"
			"CAMERA_AT: %.3f, %.3f, %.3f\n"
			"CAMERA_UP: %.3f, %.3f, %.3f\n",
			cam.Eye.x, cam.Eye.y, cam.Eye.z,
			cam.At.x, cam.At.y, cam.At.z,
			cam.Up.x, cam.Up.y, cam.Up.z);
	}

	// Caustic debug: the actual value being uploaded to GGCustomFrameCB (b4) each frame.
	// 0 => Wicked_Update_Visuals never populated it; ~0.00067 => default Caustic Size 3.0.
	{
		written += _snprintf(result + written, resultSize - written,
			"CAUSTIC_SCALE: %.4f (ShaderOcean.caustic_scale -> engine lightingHF seabed caustics)\n"
			"CAUSTIC_PATCHLEN: %.2f (drives waves; caustic size is now independent of it)\n"
			"CAUSTIC_SIZE_FROM_SLIDER: %.2f (what the visuals/slider path delivered; -1 = never ran)\n",
			wiScene::GetScene().ocean.params.caustic_scale,
			wiScene::GetScene().ocean.params.patch_length,
			[]() -> float { extern float g_dbgCausticSizeFromVisuals; return g_dbgCausticSizeFromVisuals; }());
	}

	// Water-height trace: the GG water line from source (g.gdefaultwaterheight) through the
	// weather component and the per-frame blended scene.weather singleton, to the value actually
	// uploaded to GGCustomFrameCB (b4 -> g_xFrame_WaterHeight in the terrain/grass/tree shaders).
	// If WH_GGCB diverges from WH_SCENE_WEATHER the b4 upload path is broken (it was, in Wicked
	// mode, until GGCustomFrame_Update was hooked before the terrain early-return).
	{
		extern wi::ecs::Entity g_weatherEntityID;
		wiScene::WeatherComponent* whComp = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
		written += _snprintf(result + written, resultSize - written,
			"WH_GDEFAULT: %.2f (g.gdefaultwaterheight - GG source of truth)\n"
			"WH_COMPONENT: %.2f (weathers[g_weatherEntityID].oceanParameters.waterHeight - written by Wicked_Update_Visuals)\n"
			"WH_SCENE_WEATHER: %.2f (scene.weather.oceanParameters.waterHeight - read by GGCustomFrame_Update)\n"
			"WH_GGCB: %.2f (actual value uploaded to GGCustomFrameCB b4)\n",
			g.gdefaultwaterheight,
			whComp ? whComp->oceanParameters.waterHeight : -99999.0f,
			wiScene::GetScene().weather.oceanParameters.waterHeight,
			GGTerrain::GGCustomFrame_GetWaterHeight());
	}

	// Surface water colour diagnostics: what the ocean SURFACE (oceanSurfacePS) receives. The rgb
	// is the base/albedo colour, a is the surface opacity (drives refraction fog); extinction is the
	// per-channel absorption tint. If WATER_ALPHA is ~0 the base colour barely shows from above.
	{
		auto& op = wiScene::GetScene().weather.oceanParameters; // what oceanSurfacePS is marshalled from
		written += _snprintf(result + written, resultSize - written,
			"WATER_COLOR: %.3f, %.3f, %.3f\n"
			"WATER_ALPHA: %.3f (surface opacity; ~0 = fully transparent, base colour won't show from above)\n"
			"WATER_EXTINCTION: %.3f, %.3f, %.3f (absorption tint)\n",
			op.waterColor.x, op.waterColor.y, op.waterColor.z, op.waterColor.w,
			op.extinctionColor.x, op.extinctionColor.y, op.extinctionColor.z);
	}

	// Hair/grass simulation diagnostics — the hair simulate compute pass dispatches one thread per
	// STRAND per hair system every frame (regardless of how many are drawn), so total strand count
	// is the GPU cost driver. Report totals + the per-system params that scale per-strand cost.
	{
		auto& sc = wiScene::GetScene();
		uint64_t totalStrands = 0, maxStrands = 0;
		uint32_t seg = 0, bb = 0; float vd = 0, len = 0;
		for (size_t i = 0; i < sc.hairs.GetCount(); i++)
		{
			auto& h = sc.hairs[i];
			totalStrands += h.strandCount;
			if (h.strandCount > maxStrands) maxStrands = h.strandCount;
			if (i == 0) { seg = h.segmentCount; bb = h.billboardCount; vd = h.viewDistance; len = h.length; }
		}
		written += _snprintf(result + written, resultSize - written,
			"HAIR_SYSTEMS: %d\n"
			"HAIR_TOTAL_STRANDS: %llu (one sim thread each, every frame)\n"
			"HAIR_MAX_STRANDS_PER_SYS: %llu\n"
			"HAIR_SEG: %u  HAIR_BILLBOARD: %u  HAIR_VIEWDIST: %.0f  HAIR_LEN: %.1f\n",
			(int)sc.hairs.GetCount(), (unsigned long long)totalStrands, (unsigned long long)maxStrands,
			seg, bb, vd, len);
		{
			// GGMAX 3.116: so sweeps log the grass density they ran at
			extern int gg_grass_density_pct; extern bool gg_grass_density_dx11;
			extern float GGGrass_NearDensityPerM2(); extern float GGGrass_DX11DensityPerM2();
			written += _snprintf(result + written, resultSize - written, "GRASS_DENSITY: mode=%s pct=%d near_per_m2=%.2f dx11_per_m2=%.2f\n",
				gg_grass_density_dx11 ? "dx11" : "const", gg_grass_density_pct, GGGrass_NearDensityPerM2(), GGGrass_DX11DensityPerM2());
		}

		// GGMAX: grass systems per CHUNK. One hair system exists per (chunk x painted type),
		// so this histogram is exactly the payoff available from merging a chunk's types into
		// one system: chunks with 1 type save nothing, chunks with N types save (N-1)/N.
		// Measure this before assuming the merge is worth building.
		{
			extern void GGGrass_GetChunkTypeHistogram(unsigned int* histOut, unsigned int histLen,
				unsigned int* chunksOut, unsigned int* systemsOut);
			unsigned int hist[9] = {}; unsigned int chunks = 0, systems = 0;
			GGGrass_GetChunkTypeHistogram(hist, 9, &chunks, &systems);
			written += _snprintf(result + written, resultSize - written,
				"GRASS_CHUNKS: chunks=%u systems=%u  types_per_chunk 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u 7:%u 8+:%u  merged_would_be=%u\n",
				chunks, systems, hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7], hist[8], chunks);
			// GGMAX 1.84: which exit ProcessGrassChunkMerged takes, plus the LIVE flag value.
			// `grassmerge=1` produced zero hair systems and source reading did not explain it.
			// The flag echo is deliberate — an inert knob and a broken feature look identical
			// from the effect alone, which is exactly how mablockmb wasted a build cycle.
			{
				extern bool gg_grass_merge;
				extern unsigned int gg_dbg_merge_calls, gg_dbg_merge_notypes, gg_dbg_merge_reused,
					gg_dbg_merge_nomat, gg_dbg_merge_created;
				written += _snprintf(result + written, resultSize - written,
					"GRASS_MERGE: flag=%d calls=%u notypes=%u reused=%u nomat=%u created=%u"
					" | fullResets=%llu recycles=%llu recreates=%llu deadMeshNow=%llu\n",
					gg_grass_merge ? 1 : 0, gg_dbg_merge_calls, gg_dbg_merge_notypes,
					gg_dbg_merge_reused, gg_dbg_merge_nomat, gg_dbg_merge_created,
					(unsigned long long)g_dbgGrassFullResets, (unsigned long long)g_dbgGrassRecycles,
					(unsigned long long)g_dbgGrassRecreates, (unsigned long long)g_dbgGrassDeadMeshNow);
			}
		}

		// GGMAX 1.37 diagnostics: the hair-sim static-skip gate inputs
		written += _snprintf(result + written, resultSize - written,
			"WEATHER_WIND: dir=(%.4f, %.4f, %.4f) speed=%.3f randomness=%.3f wavesize=%.3f\n"
			"HAIRSKIP: %s\n"
			"APP_SUBMIT_PRESENT_MS: %.2f (queue submits + Present + swapchain pacing, outside CPU-frame span)\n",
			sc.weather.windDirection.x, sc.weather.windDirection.y, sc.weather.windDirection.z,
			sc.weather.windSpeed, sc.weather.windRandomness, sc.weather.windWaveSize,
			wi::renderer::gg_hair_sim_static_skip ? "enabled" : "disabled",
			wi::gg_app_submit_present_ms);

		// GGMAX 1.48a: submit-tail phase breakdown
		written += _snprintf(result + written, resultSize - written,
			"SUBMIT_STALL_WINDOW: mean=%.3f max=%.2f stalled=%u/%u frames (%.1f%%) — rolling since SET_SUBMITSTATS 1; the single-frame stall below is a SNAPSHOT and varies, judge from this line\n"
			"SUBMIT_PHASES_MS: close=%.2f fences=%.2f present=%.2f sync=%.2f stall=%.2f (lists=%u batches=%u deps=%u)\n",
			wi::graphics::gg_stall_frames ? (wi::graphics::gg_stall_sum_ms / wi::graphics::gg_stall_frames) : 0.0,
			wi::graphics::gg_stall_max_ms,
			wi::graphics::gg_stall_nonzero, wi::graphics::gg_stall_frames,
			wi::graphics::gg_stall_frames ? (100.0 * wi::graphics::gg_stall_nonzero / wi::graphics::gg_stall_frames) : 0.0,
			wi::graphics::gg_submit_ms_close, wi::graphics::gg_submit_ms_fences,
			wi::graphics::gg_submit_ms_present, wi::graphics::gg_submit_ms_sync,
			wi::graphics::gg_submit_ms_stall,
			wi::graphics::gg_submit_lists, wi::graphics::gg_submit_batches, wi::graphics::gg_submit_deps);
	}

	// Terrain debug info
	{
		int drawCount=0, exitReason=0, initFlag=0, drawEn=0, updateEn=0;
		GGTerrain_GetDrawDebugInfo(&drawCount, &exitReason, &initFlag, &drawEn, &updateEn);
		written += _snprintf(result + written, resultSize - written,
			"TERRAIN_DRAW_COUNT: %d\n"
			"TERRAIN_DRAW_EXIT: %d\n"
			"TERRAIN_INIT: %d\n"
			"TERRAIN_DRAW_EN: %d\n"
			"TERRAIN_UPDATE_EN: %d\n",
			drawCount, exitReason, initFlag, drawEn, updateEn);
	}

	// PERF P.3: per-frame editor entity-pick reuse cache effectiveness (cumulative since launch)
	{
		extern int g_pickRealRuns, g_pickCacheHits, g_pickMissMask, g_pickMissRay;
		written += _snprintf(result + written, resultSize - written,
			"PICK_REAL_RUNS: %d (scene raycasts actually executed)\n"
			"PICK_CACHE_HITS: %d (reused - ray unchanged)\n"
			"PICK_MISS_MASK: %d (miss: layer/output pattern differed)\n"
			"PICK_MISS_RAY: %d (miss: cursor/camera differed)\n",
			g_pickRealRuns, g_pickCacheHits, g_pickMissMask, g_pickMissRay);
	}

	// PERF P.3: editor placement state (tells us which gridedit_mapediting path runs at idle)
	written += _snprintf(result + written, resultSize - written,
		"EDIT_STATE: gridentity=%d gridentityobj=%d pickedSection=%d mclick=%d selstage=%d grideditselect=%d\n",
		t.gridentity, t.gridentityobj, t.widget.pickedSection, t.inputsys.mclick,
		t.selstage, t.grideditselect);

	// Wicked-terrain blend/bridge diagnostics (cumulative counters + last-frame censuses)
	{
		using namespace GGTerrain;
		written += _snprintf(result + written, resultSize - written,
			"TERRAINW_BRIDGE_CALLS: %llu\n"
			"TERRAINW_BRIDGE_MARKED: %llu\n"
			"TERRAINW_BRIDGE_KEYS_ERASED: %llu\n"
			"TERRAINW_AUTOBLEND_CHUNKS: %llu\n"
			"TERRAINW_PAINTBLEND_CHUNKS: %llu\n"
			"TERRAINW_INVALIDATED_NOW: %llu\n"
			"TERRAINW_MERGEPENDING_NOW: %llu\n"
			"TERRAINW_AUTO_RUNS: %llu\n"
			"TERRAINW_AUTO_LASTPENDING: %llu\n"
			"TERRAINW_AUTO_SKIP_NOCHUNK: %llu\n"
			"TERRAINW_AUTO_SKIP_NOLAYERS: %llu\n"
			"TERRAINW_AUTO_SKIP_INVALID: %llu\n"
			"TERRAINW_AUTO_SKIP_MERGEPEND: %llu\n"
			"GRASS_RECYCLES: %llu\n"
			"GRASS_FULLRESETS: %llu\n"
			"GRASS_RECREATES: %llu\n"
			"GRASS_DEADMESH_NOW: %llu\n"
			"GRASS_EXTKILLS: %llu\n"
			"TERRAINW_IDLE: gate=%d calm=%u skips=%llu\n",
			(unsigned long long)g_dbgBridgeCalls,
			(unsigned long long)g_dbgBridgeChunksMarked,
			(unsigned long long)g_dbgBridgeKeysErased,
			(unsigned long long)g_dbgAutoBlendChunks,
			(unsigned long long)g_dbgPaintBlendChunks,
			(unsigned long long)g_dbgInvalidatedCensus,
			(unsigned long long)g_dbgMergePendingCensus,
			(unsigned long long)g_dbgAutoPassRuns,
			(unsigned long long)g_dbgAutoLastPending,
			(unsigned long long)g_dbgAutoSkipNoChunk,
			(unsigned long long)g_dbgAutoSkipNoLayers,
			(unsigned long long)g_dbgAutoSkipInvalid,
			(unsigned long long)g_dbgAutoSkipMergePend,
			(unsigned long long)g_dbgGrassRecycles,
			(unsigned long long)g_dbgGrassFullResets,
			(unsigned long long)g_dbgGrassRecreates,
			(unsigned long long)g_dbgGrassDeadMeshNow,
			(unsigned long long)g_dbgGrassExternalKills,
			g_terrainIdleGate ? 1 : 0, g_dbgIdleCalmFrames,
			(unsigned long long)g_dbgIdleGateSkips);
	}

	// Tab mode (profiler panel state)
	written += _snprintf(result + written, resultSize - written,
		"TAB_MODE: %d\n", g.tabmode);

	// GGMAX 1.67: main-camera poly count (always counted, no profiler needed)
	{
		extern uint64_t GGPerf_GetPolyCount();
		written += _snprintf(result + written, resultSize - written,
			"POLYS: %llu\n", (unsigned long long)GGPerf_GetPolyCount());
	}

	// GGMAX 3.26a: dropped command lists. ALWAYS printed, including the zero, so that a sweep
	// transcript records the healthy case too - "the line is missing" and "the line says 0" are
	// not the same evidence, and only the second one is worth anything later.
	written += _snprintf(result + written, resultSize - written,
		"CLOSEFAIL: %u\n",
		wi::graphics::gg_close_failures.load(std::memory_order_relaxed));

	// GGMAX diag: cumulative ray-primitive counters for the FPS-plummet hunt.
	// Values are running totals — diff two dumps (and their t= stamps) for rates.
	{
		extern unsigned long long gg_dbg_lua_isect_calls, gg_dbg_lua_isect_us, gg_dbg_lua_isect_mode[4];
		extern unsigned long long gg_dbg_sentray4_calls, gg_dbg_sentray4_us;
		written += _snprintf(result + written, resultSize - written,
			"RAYS: luaIsect=%llu luaIsect_ms=%.1f modes(t/s/p/d)=%llu/%llu/%llu/%llu sentray4=%llu sentray4_ms=%.1f t=%u\n",
			gg_dbg_lua_isect_calls, gg_dbg_lua_isect_us / 1000.0,
			gg_dbg_lua_isect_mode[0], gg_dbg_lua_isect_mode[1], gg_dbg_lua_isect_mode[2], gg_dbg_lua_isect_mode[3],
			gg_dbg_sentray4_calls, gg_dbg_sentray4_us / 1000.0,
			(unsigned int)timeGetTime());
		written += _snprintf(result + written, resultSize - written,
			"RAYS2: isectCalls=%llu objIter=%llu aabbPass=%llu tris=%llu skinTris=%llu\n",
			wi::scene::gg_dbg_isect_calls.load(), wi::scene::gg_dbg_isect_objects.load(),
			wi::scene::gg_dbg_isect_aabbpass.load(), wi::scene::gg_dbg_isect_tris.load(),
			wi::scene::gg_dbg_isect_skintris.load());
		written += _snprintf(result + written, resultSize - written,
			"VT: rebuilds=%llu scan_ms=%.1f sort_ms=%.1f free=%llu tiles=%llu reqTotal=%llu reasonBits=%llu\n",
			wi::terrain::gg_dbg_vt_rebuilds.load(), wi::terrain::gg_dbg_vt_scan_us.load() / 1000.0,
			wi::terrain::gg_dbg_vt_sort_us.load() / 1000.0, wi::terrain::gg_dbg_vt_free.load(),
			wi::terrain::gg_dbg_vt_tiles.load(), wi::terrain::gg_dbg_vt_requests.load(),
			wi::terrain::gg_dbg_vt_reason.load());
		written += _snprintf(result + written, resultSize - written,
			"GAPS: count=%llu last_ms=%llu (frame gaps >100ms; ledger in gap_trace.txt)\n",
			wi::profiler::gg_trace_gap_count.load(), wi::profiler::gg_trace_gap_last_ms.load());
		// GGMAX 1.82: the hitch histogram (see HITCH_RESET). Every frame is bucketed, so this
		// catches the 1-30 ms first-use PSO compiles that GAPS (>100ms) and any FPS average both
		// miss. psoC/psoMs are deltas since the last reset; psoMax is the worst SINGLE compile
		// since launch, which is the number that decides whether lazy PSOs are felt or not.
		{
			unsigned long long hFrames = 0, hOver[GG_HITCH_BUCKETS] = {}, hMaxUs = 0, hTotalUs = 0, hPso = 0, hPsoUs = 0;
			wi::profiler::gg_hitch_get(&hFrames, hOver, &hMaxUs, &hTotalUs, &hPso, &hPsoUs);
			written += _snprintf(result + written, resultSize - written,
				"HITCH: frames=%llu mean_ms=%.2f worst_ms=%.1f over(16.7/25/33/50/100)=%llu/%llu/%llu/%llu/%llu psoC=%llu psoMs=%.1f psoMax_ms=%.1f\n",
				hFrames, hFrames ? (hTotalUs / 1000.0 / hFrames) : 0.0, hMaxUs / 1000.0,
				hOver[0], hOver[1], hOver[2], hOver[3], hOver[4],
				hPso, hPsoUs / 1000.0, gg_dbg_pso_compile_max_us.load() / 1000.0);
		}
		written += _snprintf(result + written, resultSize - written,
			"STREAM: on=%d enrolled=%u replaced=%u resident_mb=%.1f full_mb=%.1f gpumem_pct=%.1f copies=%llu fb=%llu req=%llu guard_rejects=%u\n",
			g_bTextureStreamingEnabled ? 1 : 0,
			wi::resourcemanager::gg_dbg_stream_enrolled.load(),
			wi::resourcemanager::gg_dbg_stream_replaced.load(),
			wi::resourcemanager::gg_dbg_stream_resident_bytes.load() / (1024.0 * 1024.0),
			wi::resourcemanager::gg_dbg_stream_full_bytes.load() / (1024.0 * 1024.0),
			wi::resourcemanager::gg_dbg_stream_mem_permille.load() / 10.0,
			wi::renderer::gg_dbg_stream_copies.load(),
			wi::scene::gg_dbg_stream_fb_hits.load(),
			wi::gg_dbg_stream_req_calls.load(),
			wi::resourcemanager::gg_dbg_stream_guard_rejects.load());
		// GGMAX: transparent shadow atlas occupancy. batches = objects that reached the
		// TRANSPARENT|WATER shadow queue since launch. The atlas is RGBA16F sized by the shadow
		// packer (512 MB on four hub demos); its alpha carries a depth value used by
		// TRANSPARENT_SHADOWMAP_SECONDARY_DEPTH_CHECK, so the format can only be reduced if this
		// stays 0. Measure before touching it.
		written += _snprintf(result + written, resultSize - written,
			"SHADOWT: transparent_shadow_batches=%llu\n",
			wi::renderer::gg_dbg_transparent_shadow_batches.load());
		{
			// GGMAX 1.70: VRAM census aggregates (full line items via DUMP_VRAM)
			extern void GG_GetVRAMTotals(unsigned long long*, unsigned long long*, unsigned long long*, unsigned long long*, unsigned int*);
			unsigned long long vcAll = 0, vcDef = 0, vcUse = 0, vcBud = 0; unsigned int vcCount = 0;
			GG_GetVRAMTotals(&vcAll, &vcDef, &vcUse, &vcBud, &vcCount);
			written += _snprintf(result + written, resultSize - written,
				"VRAM: census_mb=%.1f defaultheap_mb=%.1f resources=%u driver_usage_mb=%.1f driver_budget_mb=%.1f\n",
				vcAll / (1024.0 * 1024.0), vcDef / (1024.0 * 1024.0), vcCount,
				vcUse / (1024.0 * 1024.0), vcBud / (1024.0 * 1024.0));
			// Mesh-data suballocator occupancy: blocks are freed only when fully empty, so
			// free_mb here is either genuine headroom or fragmentation holding blocks alive.
			unsigned int sbBlocks = 0; unsigned long long sbTotal = 0, sbFree = 0;
			wi::renderer::GG_GetSuballocatorStats(&sbBlocks, &sbTotal, &sbFree);
			written += _snprintf(result + written, resultSize - written,
				"SUBALLOC: blocks=%u total_mb=%.1f free_mb=%.1f used_mb=%.1f\n",
				sbBlocks, sbTotal / (1024.0 * 1024.0), sbFree / (1024.0 * 1024.0),
				(sbTotal - sbFree) / (1024.0 * 1024.0));
		}
		written += _snprintf(result + written, resultSize - written,
			"STREAM2: jobStart=%llu jobEnd=%llu busySkip=%llu\n",
			wi::resourcemanager::gg_dbg_stream_job_starts.load(),
			wi::resourcemanager::gg_dbg_stream_job_ends.load(),
			wi::resourcemanager::gg_dbg_stream_busy_skips.load());
		written += _snprintf(result + written, resultSize - written,
			"STREAM3: req0=%u reqLow=%u noMips=%u cancel=%u in=%u out=%u reqMask=0x%X (per-pass census; reqMask=all request magnitudes seen)\n",
			wi::resourcemanager::gg_dbg_stream_dec_req0.load(),
			wi::resourcemanager::gg_dbg_stream_dec_reqlow.load(),
			wi::resourcemanager::gg_dbg_stream_dec_nomips.load(),
			wi::resourcemanager::gg_dbg_stream_dec_cancel.load(),
			wi::resourcemanager::gg_dbg_stream_dec_in.load(),
			wi::resourcemanager::gg_dbg_stream_dec_out.load(),
			wi::resourcemanager::gg_dbg_stream_max_req.load());
	}

	// Fog / atmosphere debug (level visuals + live weather component values)
	{
		extern wi::ecs::Entity g_weatherEntityID;
		wi::scene::WeatherComponent* weather = wi::scene::GetScene().weathers.GetComponent(g_weatherEntityID);
		written += _snprintf(result + written, resultSize - written,
			"FOG_VIS: near=%.1f far=%.1f a=%.3f rgb=(%.0f,%.0f,%.0f)\n"
			"FOG_WEATHER: start=%.1f density=%.8f overrideCol=%d horizon=(%.2f,%.2f,%.2f)\n",
			t.visuals.FogNearest_f, t.visuals.FogDistance_f, t.visuals.FogA_f,
			t.visuals.FogR_f, t.visuals.FogG_f, t.visuals.FogB_f,
			weather ? weather->fogStart : -1.0f,
			weather ? weather->fogDensity : -1.0f,
			weather ? (weather->IsOverrideFogColor() ? 1 : 0) : -1,
			weather ? weather->horizon.x : -1.0f,
			weather ? weather->horizon.y : -1.0f,
			weather ? weather->horizon.z : -1.0f);
	}

	// Profiler timing data (if profiler is enabled)
	if (wi::profiler::IsEnabled() && written < resultSize - 256)
	{
		float cpuMs = wi::profiler::GetCPUFrameTime();
		float gpuMs = wi::profiler::GetGPUFrameTime();
		written += _snprintf(result + written, resultSize - written,
			"CPU_FRAME_MS: %.2f\n"
			"GPU_FRAME_MS: %.2f\n",
			cpuMs, gpuMs);

		// Use cached text from Compose() which captures all GPU sub-ranges.
		// GetTextData() called here during Update would miss GPU ranges
		// because BeginFrame() clears in_use before Render creates them.
		extern std::string GGPerf_GetCachedProfilerText();
		std::string profText = GGPerf_GetCachedProfilerText();
		if (!profText.empty() && written + (int)profText.size() < resultSize - 32)
		{
			written += _snprintf(result + written, resultSize - written,
				"PROFILER_DATA:\n%s", profText.c_str());
		}
	}

	result[resultSize - 1] = 0;
}

static void Cmd_ToggleProfiler(char* result, int resultSize)
{
	const char* state = AutoHarness_GetAppState();
	if (strcmp(state, "game") != 0)
	{
		_snprintf(result, resultSize, "ERROR: TOGGLE_PROFILER only works in game state (current state: %s)", state);
		result[resultSize - 1] = 0;
		return;
	}

	// Cycle g.tabmode exactly like the TAB key does (M-Game_part3.cpp line 93)
	g.tabmode = g.tabmode + 1;
	if (g.tabmode > 2)
		g.tabmode = 0;

	const char* modeName = "game (normal)";
	if (g.tabmode == 1) modeName = "visuals panel";
	if (g.tabmode == 2) modeName = "performance panel";

	_snprintf(result, resultSize, "OK: Toggled to tabmode=%d (%s)", g.tabmode, modeName);
	result[resultSize - 1] = 0;
}

static void Cmd_PressEscape(char* result, int resultSize)
{
	const char* state = AutoHarness_GetAppState();
	if (strcmp(state, "game") == 0)
	{
		// Same exit path as ESC key in M-Game_part1.cpp line 1523
		t.game.gameloop = 0;
		t.game.levelloop = 0;
		t.game.masterloop = 0;
		_snprintf(result, resultSize, "OK: Triggered escape from test game (gameloop=0, levelloop=0, masterloop=0)");
	}
	else
	{
		_snprintf(result, resultSize, "ERROR: PRESS_ESCAPE only works in game state (current state: %s)", state);
	}
	result[resultSize - 1] = 0;
}

static int AutoHarness_ParseKeyName(const char* name)
{
	if (!name || !name[0]) return -1;

	// Single character: A-Z, 0-9
	if (name[1] == 0)
	{
		char c = name[0];
		if (c >= 'a' && c <= 'z') return 0x41 + (c - 'a');
		if (c >= 'A' && c <= 'Z') return 0x41 + (c - 'A');
		if (c >= '0' && c <= '9') return 0x30 + (c - '0');
		return -1;
	}

	// Named keys (case-insensitive)
	if (_stricmp(name, "ESCAPE") == 0) return 0x1B;
	if (_stricmp(name, "ESC") == 0) return 0x1B;
	if (_stricmp(name, "ENTER") == 0) return 0x0D;
	if (_stricmp(name, "RETURN") == 0) return 0x0D;
	if (_stricmp(name, "SPACE") == 0) return 0x20;
	if (_stricmp(name, "TAB") == 0) return 0x09;
	if (_stricmp(name, "BACKSPACE") == 0) return 0x08;
	if (_stricmp(name, "DELETE") == 0) return 0x2E;
	if (_stricmp(name, "INSERT") == 0) return 0x2D;
	if (_stricmp(name, "HOME") == 0) return 0x24;
	if (_stricmp(name, "END") == 0) return 0x23;
	if (_stricmp(name, "PAGEUP") == 0) return 0x21;
	if (_stricmp(name, "PAGEDOWN") == 0) return 0x22;
	if (_stricmp(name, "LEFT") == 0) return 0x25;
	if (_stricmp(name, "UP") == 0) return 0x26;
	if (_stricmp(name, "RIGHT") == 0) return 0x27;
	if (_stricmp(name, "DOWN") == 0) return 0x28;
	if (_stricmp(name, "SHIFT") == 0) return 0x10;
	if (_stricmp(name, "CONTROL") == 0) return 0x11;
	if (_stricmp(name, "CTRL") == 0) return 0x11;
	if (_stricmp(name, "ALT") == 0) return 0x12;

	// Function keys F1-F12
	if ((name[0] == 'F' || name[0] == 'f') && name[1] >= '1' && name[1] <= '9')
	{
		int fnum = atoi(name + 1);
		if (fnum >= 1 && fnum <= 12) return 0x70 + (fnum - 1);
	}

	return -1;
}

static void Cmd_PressKey(const char* arg, char* result, int resultSize)
{
	if (!arg || !arg[0])
	{
		_snprintf(result, resultSize, "ERROR: PRESS_KEY requires a key name (e.g. PRESS_KEY Y, PRESS_KEY F1, PRESS_KEY ESCAPE)");
		result[resultSize - 1] = 0;
		return;
	}

	if (!g_pGlob || !g_pGlob->hWnd)
	{
		_snprintf(result, resultSize, "ERROR: No window handle available");
		result[resultSize - 1] = 0;
		return;
	}

	int vk = AutoHarness_ParseKeyName(arg);
	if (vk < 0)
	{
		_snprintf(result, resultSize, "ERROR: Unknown key name '%s'. Use A-Z, 0-9, F1-F12, ESCAPE, ENTER, SPACE, TAB, SHIFT, CONTROL, ALT, arrows, etc.", arg);
		result[resultSize - 1] = 0;
		return;
	}

	// Post WM_KEYDOWN to the main window for the game's own input handling
	UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
	LPARAM lParamDown = (1 /*repeat=1*/) | (scanCode << 16);
	PostMessage(g_pGlob->hWnd, WM_KEYDOWN, (WPARAM)vk, lParamDown);

	// Also set ImGui key state directly — during test game mode, the ImGui WndProc
	// handler is bypassed so WM_KEYDOWN never reaches io.KeysDown[]. This ensures
	// terrain key sampling (GGTerrain_CheckKeys) detects the press.
	if (vk < 256 && ImGui::GetCurrentContext() != nullptr) // FIX 2026-07-25: null-context guard
		ImGui::GetIO().KeysDown[vk] = true;

	// Set injected key for terrain key system — in editor mode, the timing
	// between harness execution and GGTerrain_CheckKeys() can cause the rising
	// edge detection to miss the press from io.KeysDown[] alone.
	// GGTerrainWicked_Update() consumes this and clears it.
	extern int g_autoHarnessInjectedKey;
	g_autoHarnessInjectedKey = vk;

	s_pendingKeyUpVK = vk;
	s_pendingKeyUpFrames = 2;

	_snprintf(result, resultSize, "OK: Pressed key '%s' (VK=0x%02X)", arg, vk);
	result[resultSize - 1] = 0;
}

static void Cmd_Quit(char* result, int resultSize)
{
	_snprintf(result, resultSize, "OK: Quitting application");
	result[resultSize - 1] = 0;
	AutoHarness_WriteResult(result);

	// Post WM_CLOSE to the main window
	if (g_pGlob && g_pGlob->hWnd)
	{
		g_bDisableQuitFlag = false;
		PostMessage(g_pGlob->hWnd, WM_CLOSE, 0, 0);
	}
}

// ---- Scene interrogation commands ----

static void Cmd_ListEntities(const char* filter, char* result, int resultSize)
{
	int written = 0;
	const char* state = AutoHarness_GetAppState();

	bool filterLights = (filter && _stricmp(filter, "lights") == 0);

	written += _snprintf(result + written, resultSize - written,
		"STATE: %s\nTOTAL_ENTITY_SLOTS: %d\n", state, g.entityelementlist);

	int count = 0;
	int lightCount = 0;
	for (int e = 1; e <= g.entityelementlist && written < resultSize - 512; e++)
	{
		int bankindex = t.entityelement[e].bankindex;
		if (bankindex <= 0) continue;
		if (!t.entityelement[e].active) continue;

		bool isLight = (t.entityprofile[bankindex].ismarker == 2);
		bool isLightMarker = (t.entityprofile[bankindex].islightmarker == 1);
		if (isLight || isLightMarker) lightCount++;

		if (filterLights && !isLight && !isLightMarker) continue;

		count++;

		const char* entityName = t.entityelement[e].eleprof.name_s.Get();
		if (!entityName || !entityName[0]) entityName = "";

		written += _snprintf(result + written, resultSize - written,
			"  [%d] name=\"%.40s\" bank=%d pos=(%.1f,%.1f,%.1f) marker=%d",
			e, entityName, bankindex,
			t.entityelement[e].x, t.entityelement[e].y, t.entityelement[e].z,
			t.entityprofile[bankindex].ismarker);

		if (isLight || isLightMarker)
		{
			DWORD col = t.entityelement[e].eleprof.light.color;
			written += _snprintf(result + written, resultSize - written,
				" LIGHT(range=%d rgb=(%d,%d,%d) islit=%d spot=%d lidx=%d)",
				t.entityelement[e].eleprof.light.range,
				RgbR(col), RgbG(col), RgbB(col),
				t.entityelement[e].eleprof.light.islit,
				t.entityelement[e].eleprof.usespotlighting,
				t.entityelement[e].eleprof.light.index);
		}

		if (written < resultSize - 2)
			result[written++] = '\n';
	}

	if (written < resultSize - 128)
		written += _snprintf(result + written, resultSize - written,
			"LISTED: %d entities (LIGHT_ENTITIES: %d)\n", count, lightCount);

	result[resultSize - 1] = 0;
}

static void Cmd_GetEntity(const char* indexStr, char* result, int resultSize)
{
	if (!indexStr || !indexStr[0])
	{
		_snprintf(result, resultSize, "ERROR: GET_ENTITY requires an entity index argument");
		result[resultSize - 1] = 0;
		return;
	}

	int e = atoi(indexStr);
	if (e < 1 || e > g.entityelementlist)
	{
		_snprintf(result, resultSize, "ERROR: Entity index %d out of range (1-%d)", e, g.entityelementlist);
		result[resultSize - 1] = 0;
		return;
	}

	int written = 0;
	int bankindex = t.entityelement[e].bankindex;

	const char* entityName = t.entityelement[e].eleprof.name_s.Get();
	if (!entityName || !entityName[0]) entityName = "";

	written += _snprintf(result + written, resultSize - written,
		"ENTITY[%d]:\n"
		"  name=\"%s\"\n"
		"  active=%d\n"
		"  bankindex=%d\n"
		"  obj=%d\n"
		"  pos=(%.2f, %.2f, %.2f)\n"
		"  rot=(%.2f, %.2f, %.2f)\n"
		"  scale=(%.2f, %.2f, %.2f)\n"
		"  staticflag=%d\n",
		e, entityName,
		t.entityelement[e].active,
		bankindex,
		t.entityelement[e].obj,
		t.entityelement[e].x, t.entityelement[e].y, t.entityelement[e].z,
		t.entityelement[e].rx, t.entityelement[e].ry, t.entityelement[e].rz,
		t.entityelement[e].scalex, t.entityelement[e].scaley, t.entityelement[e].scalez,
		t.entityelement[e].staticflag);

	if (bankindex > 0 && written < resultSize - 512)
	{
		written += _snprintf(result + written, resultSize - written,
			"  PROFILE:\n"
			"    ismarker=%d\n"
			"    islightmarker=%d\n"
			"    usespotlighting=%d\n"
			"    castshadow=%d\n",
			t.entityprofile[bankindex].ismarker,
			t.entityprofile[bankindex].islightmarker,
			t.entityprofile[bankindex].usespotlighting,
			t.entityprofile[bankindex].castshadow);
	}

	// Light data
	if (written < resultSize - 512)
	{
		DWORD col = t.entityelement[e].eleprof.light.color;
		written += _snprintf(result + written, resultSize - written,
			"  LIGHT_DATA:\n"
			"    index=%d\n"
			"    islit=%d\n"
			"    range=%d\n"
			"    color=0x%08X rgb=(%d,%d,%d)\n"
			"    offsetup=%d\n"
			"    offsetz=%d\n"
			"    usespotlighting=%d\n"
			"    fLightHasProbe=%.1f\n"
			"    fProbeBrightness=%.2f\n",
			t.entityelement[e].eleprof.light.index,
			t.entityelement[e].eleprof.light.islit,
			t.entityelement[e].eleprof.light.range,
			t.entityelement[e].eleprof.light.color,
			RgbR(col), RgbG(col), RgbB(col),
			t.entityelement[e].eleprof.light.offsetup,
			t.entityelement[e].eleprof.light.offsetz,
			t.entityelement[e].eleprof.usespotlighting,
			t.entityelement[e].eleprof.light.fLightHasProbe,
			t.entityelement[e].eleprof.light.fProbeBrightness);
	}

	// If this entity has an infinilight, show it
	int lightIdx = t.entityelement[e].eleprof.light.index;
	if (lightIdx > 0 && lightIdx <= g.infinilightmax && written < resultSize - 512)
	{
		infinilighttype* pLight = &t.infinilight[lightIdx];
		written += _snprintf(result + written, resultSize - written,
			"  INFINILIGHT[%d]:\n"
			"    used=%d islit=%d e=%d\n"
			"    pos=(%.1f,%.1f,%.1f) range=%.0f\n"
			"    rgb=(%d,%d,%d)\n"
			"    spot=%d shadow=%d\n"
			"    wickedlightindex=%llu\n",
			lightIdx,
			pLight->used, pLight->islit, pLight->e,
			pLight->x, pLight->y, pLight->z, pLight->range,
			pLight->colrgb.r, pLight->colrgb.g, pLight->colrgb.b,
			pLight->is_spot_light ? 1 : 0, pLight->bCanShadow ? 1 : 0,
			(unsigned long long)pLight->wickedlightindex);

		// WickedEngine cross-reference
		wi::scene::Scene* pScene = master.masterrenderer.scene;
		if (pScene && pLight->wickedlightindex > 0)
		{
			wi::scene::LightComponent* lightComp = pScene->lights.GetComponent(pLight->wickedlightindex);
			if (lightComp)
			{
				const char* typeName = "UNKNOWN";
				switch (lightComp->GetType())
				{
					case wi::scene::LightComponent::DIRECTIONAL: typeName = "DIRECTIONAL"; break;
					case wi::scene::LightComponent::POINT: typeName = "POINT"; break;
					case wi::scene::LightComponent::SPOT: typeName = "SPOT"; break;
					case wi::scene::LightComponent::RECTANGLE: typeName = "RECTANGLE"; break;
				}
				written += _snprintf(result + written, resultSize - written,
					"  WICKED_LIGHT:\n"
					"    type=%s\n"
					"    color=(%.3f,%.3f,%.3f)\n"
					"    intensity=%.2f\n"
					"    range=%.1f\n"
					"    castShadow=%d\n"
					"    inactive=%d\n"
					"    position=(%.1f,%.1f,%.1f)\n",
					typeName,
					lightComp->color.x, lightComp->color.y, lightComp->color.z,
					lightComp->intensity,
					lightComp->range,
					lightComp->IsCastingShadow() ? 1 : 0,
					lightComp->IsInactive() ? 1 : 0,
					lightComp->position.x, lightComp->position.y, lightComp->position.z);
			}
			else
			{
				written += _snprintf(result + written, resultSize - written,
					"  WICKED_LIGHT: NOT_FOUND (entity %llu missing from scene)\n",
					(unsigned long long)pLight->wickedlightindex);
			}
		}
	}

	result[resultSize - 1] = 0;
}

static void Cmd_ListLights(char* result, int resultSize)
{
	int written = 0;
	const char* state = AutoHarness_GetAppState();

	wi::scene::Scene* pScene = master.masterrenderer.scene;
	int wickedLightCount = pScene ? (int)pScene->lights.GetCount() : 0;

	written += _snprintf(result + written, resultSize - written,
		"STATE: %s\n"
		"INFINILIGHT_MAX: %d\n"
		"WICKED_SCENE_LIGHTS: %d\n"
		"VISIBLE_LIGHTS: %d\n",
		state, g.infinilightmax, wickedLightCount,
		(int)master.masterrenderer.visibility_main.visibleLights.size());

	for (int l = 1; l <= g.infinilightmax && written < resultSize - 512; l++)
	{
		infinilighttype* pLight = &t.infinilight[l];

		written += _snprintf(result + written, resultSize - written,
			"LIGHT[%d]: e=%d used=%d islit=%d pos=(%.1f,%.1f,%.1f) range=%.0f "
			"rgb=(%d,%d,%d) spot=%d shadow=%d wkID=%llu",
			l, pLight->e, pLight->used, pLight->islit,
			pLight->x, pLight->y, pLight->z,
			pLight->range,
			pLight->colrgb.r, pLight->colrgb.g, pLight->colrgb.b,
			pLight->is_spot_light ? 1 : 0,
			pLight->bCanShadow ? 1 : 0,
			(unsigned long long)pLight->wickedlightindex);

		// Cross-reference with WickedEngine
		if (pScene && pLight->wickedlightindex > 0)
		{
			wi::scene::LightComponent* lightComp = pScene->lights.GetComponent(pLight->wickedlightindex);
			if (lightComp)
			{
				const char* typeName = "?";
				switch (lightComp->GetType())
				{
					case wi::scene::LightComponent::DIRECTIONAL: typeName = "DIR"; break;
					case wi::scene::LightComponent::POINT: typeName = "PT"; break;
					case wi::scene::LightComponent::SPOT: typeName = "SP"; break;
					case wi::scene::LightComponent::RECTANGLE: typeName = "RC"; break;
				}
				written += _snprintf(result + written, resultSize - written,
					" W(%s int=%.1f rng=%.1f rgb=(%.2f,%.2f,%.2f) shd=%d%s)",
					typeName,
					lightComp->intensity,
					lightComp->range,
					lightComp->color.x, lightComp->color.y, lightComp->color.z,
					lightComp->IsCastingShadow() ? 1 : 0,
					lightComp->IsInactive() ? " INACTIVE" : "");
			}
			else
			{
				written += _snprintf(result + written, resultSize - written, " W(MISSING!)");
			}
		}
		else if (pLight->wickedlightindex == 0)
		{
			written += _snprintf(result + written, resultSize - written, " W(NONE)");
		}

		if (written < resultSize - 2)
			result[written++] = '\n';
	}

	result[resultSize - 1] = 0;
}

// DUMP_SKIN: full armature/skinning/animation report written to auto_skin.txt (no size cap)
// to diagnose skinned-mesh corruption (the intermittent "exploded parrot" on Island Showdown).
// Order: skinned objects first (SUSPECT flag when AABB is huge), armature summaries,
// per-bone detail for suspect/filter-matched armatures, then one line per animation.
// Optional arg = case-insensitive name filter on skinned-object names (adds those
// armatures to the detail pass even when not suspect). Summary goes to auto_result.txt.
static void Cmd_DumpSkin(const char* arg, char* result, int resultSize)
{
	wi::scene::Scene* pScene = master.masterrenderer.scene;
	if (!pScene)
	{
		_snprintf(result, resultSize, "ERROR: no scene");
		result[resultSize - 1] = 0;
		return;
	}

	// report file lives next to auto_result.txt
	char skinPath[MAX_PATH];
	strncpy(skinPath, s_rspPath, MAX_PATH);
	skinPath[MAX_PATH - 1] = 0;
	char* slash = strrchr(skinPath, '\\');
	if (!slash) slash = strrchr(skinPath, '/');
	if (slash) slash[1] = 0; else skinPath[0] = 0;
	strncat(skinPath, "auto_skin.txt", MAX_PATH - strlen(skinPath) - 1);

	FILE* f = fopen(skinPath, "w");
	if (!f)
	{
		_snprintf(result, resultSize, "ERROR: cannot write %s", skinPath);
		result[resultSize - 1] = 0;
		return;
	}

	// --- Pass 1: skinned objects (mesh has armatureID), collect suspects for detail pass ---
	const float SUSPECT_HALFWIDTH = 5000.0f;
	std::vector<wi::ecs::Entity> detailArmatures;
	int skinnedCount = 0, suspectCount = 0;
	char suspectNames[512] = { 0 };

	for (size_t o = 0; o < pScene->objects.GetCount(); ++o)
	{
		wi::scene::ObjectComponent& obj = pScene->objects[o];
		if (obj.meshID == wi::ecs::INVALID_ENTITY) continue;
		wi::scene::MeshComponent* mesh = pScene->meshes.GetComponent(obj.meshID);
		if (!mesh || mesh->armatureID == wi::ecs::INVALID_ENTITY) continue;
		skinnedCount++;

		wi::ecs::Entity objEntity = pScene->objects.GetEntity(o);
		wi::scene::NameComponent* name = pScene->names.GetComponent(objEntity);
		const char* objName = name ? name->name.c_str() : "?";

		bool armaExists = pScene->armatures.GetComponent(mesh->armatureID) != nullptr;
		const wi::primitive::AABB& aabb = pScene->aabb_objects[o];
		XMFLOAT3 c = aabb.getCenter(), h = aabb.getHalfWidth();

		bool suspect = (h.x > SUSPECT_HALFWIDTH || h.y > SUSPECT_HALFWIDTH || h.z > SUSPECT_HALFWIDTH) && obj.IsRenderable();
		bool filtered = (arg && arg[0] && pestrcasestr(objName, arg));
		if (suspect)
		{
			suspectCount++;
			if (strlen(suspectNames) < sizeof(suspectNames) - 64)
			{
				strncat(suspectNames, objName, 48);
				strncat(suspectNames, " ", 2);
			}
		}
		if ((suspect || filtered) && detailArmatures.size() < 16)
		{
			bool have = false;
			for (auto e : detailArmatures) if (e == mesh->armatureID) have = true;
			if (!have) detailArmatures.push_back(mesh->armatureID);
		}

		fprintf(f, "SKINOBJ ent=%llu name='%s' mesh=%llu arma=%llu%s renderable=%d "
			"aabbC=(%.0f,%.0f,%.0f) aabbH=(%.0f,%.0f,%.0f)%s\n",
			(unsigned long long)objEntity, objName,
			(unsigned long long)obj.meshID, (unsigned long long)mesh->armatureID,
			armaExists ? "" : " ARMA_MISSING!",
			obj.IsRenderable() ? 1 : 0,
			c.x, c.y, c.z, h.x, h.y, h.z,
			suspect ? "  <<< SUSPECT" : "");
	}

	// --- Pass 2: armature summaries ---
	fprintf(f, "ARMATURES: %d\n", (int)pScene->armatures.GetCount());
	for (size_t a = 0; a < pScene->armatures.GetCount(); ++a)
	{
		wi::scene::ArmatureComponent& arma = pScene->armatures[a];
		wi::ecs::Entity armaEntity = pScene->armatures.GetEntity(a);

		int missing = 0, invalid = 0, nans = 0;
		float minT = FLT_MAX, maxT = -FLT_MAX;
		for (wi::ecs::Entity boneEntity : arma.boneCollection)
		{
			if (boneEntity == wi::ecs::INVALID_ENTITY) { invalid++; continue; }
			wi::scene::TransformComponent* bone = pScene->transforms.GetComponent(boneEntity);
			if (!bone) { missing++; continue; }
			float tx = bone->world._41, ty = bone->world._42, tz = bone->world._43;
			if (isnan(tx) || isnan(ty) || isnan(tz)) { nans++; continue; }
			float mag = sqrtf(tx * tx + ty * ty + tz * tz);
			if (mag < minT) minT = mag;
			if (mag > maxT) maxT = mag;
		}

		// boneData = final skin matrices (armature-local); huge translations here = exploded skin
		float minB = FLT_MAX, maxB = -FLT_MAX;
		int nanB = 0;
		for (const auto& bd : arma.boneData)
		{
			float tx = bd.mat0.w, ty = bd.mat1.w, tz = bd.mat2.w;
			if (isnan(tx) || isnan(ty) || isnan(tz)) { nanB++; continue; }
			float mag = sqrtf(tx * tx + ty * ty + tz * tz);
			if (mag < minB) minB = mag;
			if (mag > maxB) maxB = mag;
		}

		wi::scene::TransformComponent* armaT = pScene->transforms.GetComponent(armaEntity);
		fprintf(f, "ARM[%d] ent=%llu bones=%d invBind=%d boneData=%d miss=%d inval=%d nan=%d "
			"|boneW|=%.1f..%.1f |skinT|=%.1f..%.1f nanSkin=%d armaPos=(%.1f,%.1f,%.1f)\n",
			(int)a, (unsigned long long)armaEntity,
			(int)arma.boneCollection.size(), (int)arma.inverseBindMatrices.size(), (int)arma.boneData.size(),
			missing, invalid, nans,
			minT == FLT_MAX ? 0.0f : minT, maxT == -FLT_MAX ? 0.0f : maxT,
			minB == FLT_MAX ? 0.0f : minB, maxB == -FLT_MAX ? 0.0f : maxB, nanB,
			armaT ? armaT->world._41 : 0.0f, armaT ? armaT->world._42 : 0.0f, armaT ? armaT->world._43 : 0.0f);
	}

	// --- Pass 3: per-bone detail for suspect/filtered armatures ---
	for (wi::ecs::Entity armaEntity : detailArmatures)
	{
		wi::scene::ArmatureComponent* arma = pScene->armatures.GetComponent(armaEntity);
		if (!arma) { fprintf(f, "DETAIL arma=%llu MISSING\n", (unsigned long long)armaEntity); continue; }

		wi::scene::TransformComponent* armaT = pScene->transforms.GetComponent(armaEntity);
		wi::scene::HierarchyComponent* armaH = pScene->hierarchy.GetComponent(armaEntity);
		fprintf(f, "DETAIL arma=%llu parent=%llu world=(%.1f,%.1f,%.1f) bones=%d\n",
			(unsigned long long)armaEntity,
			(unsigned long long)(armaH ? armaH->parentID : 0),
			armaT ? armaT->world._41 : 0.0f, armaT ? armaT->world._42 : 0.0f, armaT ? armaT->world._43 : 0.0f,
			(int)arma->boneCollection.size());

		for (size_t b = 0; b < arma->boneCollection.size(); ++b)
		{
			wi::ecs::Entity boneEntity = arma->boneCollection[b];
			wi::scene::TransformComponent* bone = pScene->transforms.GetComponent(boneEntity);
			wi::scene::NameComponent* bname = boneEntity ? pScene->names.GetComponent(boneEntity) : nullptr;
			wi::scene::HierarchyComponent* bh = boneEntity ? pScene->hierarchy.GetComponent(boneEntity) : nullptr;

			float ibx = 0, iby = 0, ibz = 0;
			if (b < arma->inverseBindMatrices.size())
			{
				ibx = arma->inverseBindMatrices[b]._41; iby = arma->inverseBindMatrices[b]._42; ibz = arma->inverseBindMatrices[b]._43;
			}
			float sx = 0, sy = 0, sz = 0;
			if (b < arma->boneData.size())
			{
				sx = arma->boneData[b].mat0.w; sy = arma->boneData[b].mat1.w; sz = arma->boneData[b].mat2.w;
			}

			if (bone)
			{
				fprintf(f, "  B[%d] ent=%llu '%s' parent=%llu W=(%.1f,%.1f,%.1f) L=(%.1f,%.1f,%.1f) "
					"LR=(%.2f,%.2f,%.2f,%.2f) LS=(%.2f,%.2f,%.2f) IB=(%.1f,%.1f,%.1f) SK=(%.1f,%.1f,%.1f)\n",
					(int)b, (unsigned long long)boneEntity, bname ? bname->name.c_str() : "?",
					(unsigned long long)(bh ? bh->parentID : 0),
					bone->world._41, bone->world._42, bone->world._43,
					bone->translation_local.x, bone->translation_local.y, bone->translation_local.z,
					bone->rotation_local.x, bone->rotation_local.y, bone->rotation_local.z, bone->rotation_local.w,
					bone->scale_local.x, bone->scale_local.y, bone->scale_local.z,
					ibx, iby, ibz, sx, sy, sz);
			}
			else
			{
				fprintf(f, "  B[%d] ent=%llu '%s' NO_TRANSFORM IB=(%.1f,%.1f,%.1f) SK=(%.1f,%.1f,%.1f)\n",
					(int)b, (unsigned long long)boneEntity, bname ? bname->name.c_str() : "?",
					ibx, iby, ibz, sx, sy, sz);
			}
		}
	}

	// --- Pass 4: animations, compact line each ---
	fprintf(f, "ANIMATIONS: %d\n", (int)pScene->animations.GetCount());
	for (size_t i = 0; i < pScene->animations.GetCount(); ++i)
	{
		wi::scene::AnimationComponent& anim = pScene->animations[i];
		wi::ecs::Entity animEntity = pScene->animations.GetEntity(i);
		wi::scene::NameComponent* name = pScene->names.GetComponent(animEntity);

		int chBadTarget = 0, chNoData = 0;
		for (const auto& ch : anim.channels)
		{
			if (ch.target == wi::ecs::INVALID_ENTITY || !pScene->transforms.GetComponent(ch.target)) chBadTarget++;
			if (ch.samplerIndex < (int)anim.samplers.size())
			{
				if (!pScene->animation_datas.GetComponent(anim.samplers[ch.samplerIndex].data)) chNoData++;
			}
		}

		// first valid channel target tells us which hierarchy this anim drives
		wi::ecs::Entity firstTarget = anim.channels.empty() ? 0 : anim.channels[0].target;

		wi::scene::ObjectComponent* cullObj = pScene->objects.GetComponent(anim.objectIndex);
		fprintf(f, "ANIM[%d] ent=%llu '%s' play=%d loop=%d t=%.2f s=%.1f e=%.1f amt=%.2f spd=%.1f "
			"cullObj=%llu(%s) ch=%d badTgt=%d noData=%d tgt0=%llu\n",
			(int)i, (unsigned long long)animEntity, name ? name->name.c_str() : "?",
			anim.IsPlaying() ? 1 : 0, anim.IsLooped() ? 1 : 0,
			anim.timer, anim.start, anim.end, anim.amount, anim.speed,
			(unsigned long long)anim.objectIndex,
			cullObj ? (cullObj->IsRenderable() ? "vis" : "HIDDEN") : "none",
			(int)anim.channels.size(), chBadTarget, chNoData,
			(unsigned long long)firstTarget);
	}

	// --- Pass 5: animation keyframe data garbage scan ---
	// If garbage lives HERE the corruption is baked at load (keyframe build);
	// if all data is clean yet bone locals are garbage, something writes transforms directly.
	int dataBad = 0;
	fprintf(f, "ANIMDATA: %d\n", (int)pScene->animation_datas.GetCount());
	for (size_t i = 0; i < pScene->animation_datas.GetCount(); ++i)
	{
		auto& ad = pScene->animation_datas[i];
		float maxAbs = 0.0f;
		int bad = 0, firstBad = -1;
		for (size_t k = 0; k < ad.keyframe_data.size(); ++k)
		{
			float v = ad.keyframe_data[k];
			if (isnan(v) || fabsf(v) > 1.0e6f)
			{
				bad++;
				if (firstBad < 0) firstBad = (int)k;
			}
			else if (fabsf(v) > maxAbs) maxAbs = v < 0 ? -v : v;
		}
		float maxTime = ad.keyframe_times.empty() ? 0.0f : ad.keyframe_times.back();
		if (bad > 0)
		{
			dataBad++;
			fprintf(f, "ANIMDATA[%d] ent=%llu keys=%d data=%d maxT=%.0f BAD=%d firstBad=%d sample=(%g,%g,%g,%g)\n",
				(int)i, (unsigned long long)pScene->animation_datas.GetEntity(i),
				(int)ad.keyframe_times.size(), (int)ad.keyframe_data.size(), maxTime, bad, firstBad,
				firstBad >= 0 && firstBad + 3 < (int)ad.keyframe_data.size() ? ad.keyframe_data[firstBad] : 0.0f,
				firstBad >= 0 && firstBad + 3 < (int)ad.keyframe_data.size() ? ad.keyframe_data[firstBad + 1] : 0.0f,
				firstBad >= 0 && firstBad + 3 < (int)ad.keyframe_data.size() ? ad.keyframe_data[firstBad + 2] : 0.0f,
				firstBad >= 0 && firstBad + 3 < (int)ad.keyframe_data.size() ? ad.keyframe_data[firstBad + 3] : 0.0f);
		}
	}
	fprintf(f, "ANIMDATA_BAD_TOTAL: %d\n", dataBad);

	fclose(f);

	_snprintf(result, resultSize,
		"OK: DUMP_SKIN wrote %s\nSKINNED_OBJECTS: %d  ARMATURES: %d  ANIMATIONS: %d  BAD_ANIMDATA: %d\nSUSPECTS: %d %s",
		skinPath, skinnedCount, (int)pScene->armatures.GetCount(), (int)pScene->animations.GetCount(),
		dataBad, suspectCount, suspectNames);
	result[resultSize - 1] = 0;
}

// ---- SKIN_WATCH: per-frame scan for garbage bone rotations (parrot corruption hunt) ----
// Enabled via the SKIN_WATCH command BEFORE loading a level. Each frame, scans every
// TransformComponent for a garbage rotation_local (|component| > 1e3 or NaN) and logs
// the FIRST detection per entity with a frame counter and scene component counts, so the
// load phase where the write lands can be identified. Output: auto_skinwatch.txt.
static bool s_bSkinWatch = false;
static uint32_t s_skinWatchFrame = 0;
static FILE* s_skinWatchFile = nullptr;
static std::vector<uint64_t> s_skinWatchSeen;

static void AutoHarness_SkinWatchTick(void)
{
	if (!s_bSkinWatch) return;
	s_skinWatchFrame++;
	wi::scene::Scene* pScene = master.masterrenderer.scene;
	if (!pScene) return;

	if (!s_skinWatchFile)
	{
		char path[MAX_PATH];
		strncpy(path, s_rspPath, MAX_PATH);
		path[MAX_PATH - 1] = 0;
		char* slash = strrchr(path, '\\');
		if (!slash) slash = strrchr(path, '/');
		if (slash) slash[1] = 0; else path[0] = 0;
		strncat(path, "auto_skinwatch.txt", MAX_PATH - strlen(path) - 1);
		s_skinWatchFile = fopen(path, "w");
		if (!s_skinWatchFile) { s_bSkinWatch = false; return; }
	}

	int newHits = 0;
	for (size_t i = 0; i < pScene->transforms.GetCount(); ++i)
	{
		wi::scene::TransformComponent& tr = pScene->transforms[i];
		float rx = tr.rotation_local.x, ry = tr.rotation_local.y, rz = tr.rotation_local.z, rw = tr.rotation_local.w;
		bool bad = isnan(rx) || isnan(ry) || isnan(rz) || isnan(rw) ||
			fabsf(rx) > 1000.0f || fabsf(ry) > 1000.0f || fabsf(rz) > 1000.0f || fabsf(rw) > 1000.0f;
		if (!bad) continue;

		uint64_t entity = (uint64_t)pScene->transforms.GetEntity(i);
		bool seen = false;
		for (uint64_t e : s_skinWatchSeen) if (e == entity) { seen = true; break; }
		if (seen) continue;
		s_skinWatchSeen.push_back(entity);
		newHits++;

		wi::scene::NameComponent* name = pScene->names.GetComponent((wi::ecs::Entity)entity);
		fprintf(s_skinWatchFile, "HIT frame=%u ent=%llu '%s' LR=(%g,%g,%g,%g) LS=(%g,%g,%g) LT=(%g,%g,%g) "
			"[transforms=%d objects=%d armatures=%d anims=%d animdatas=%d]\n",
			s_skinWatchFrame, (unsigned long long)entity, name ? name->name.c_str() : "?",
			rx, ry, rz, rw,
			tr.scale_local.x, tr.scale_local.y, tr.scale_local.z,
			tr.translation_local.x, tr.translation_local.y, tr.translation_local.z,
			(int)pScene->transforms.GetCount(), (int)pScene->objects.GetCount(),
			(int)pScene->armatures.GetCount(), (int)pScene->animations.GetCount(),
			(int)pScene->animation_datas.GetCount());
	}

	if (newHits > 0 || (s_skinWatchFrame % 300) == 0)
	{
		fprintf(s_skinWatchFile, "TICK frame=%u hits=%d totalSeen=%d transforms=%d objects=%d armatures=%d anims=%d\n",
			s_skinWatchFrame, newHits, (int)s_skinWatchSeen.size(),
			(int)pScene->transforms.GetCount(), (int)pScene->objects.GetCount(),
			(int)pScene->armatures.GetCount(), (int)pScene->animations.GetCount());
		fflush(s_skinWatchFile);
	}
}

// ---- SCULPT_TEST / PAINT_TEST: synthetic terrain-edit strokes ----
// Drives the same GGTerrain_Update_Sculpting/Painting apply functions the editor
// dispatch calls, so the edit -> InvalidateRegion -> Wicked chunk regen/repaint
// chain is exercised without real mouse input. One brush tick per frame.
namespace GGTerrain {
	extern GGTerrainInternalParams ggterrain_internal_params;
	extern int ggterrain_initialised;
	void GGTerrain_Update_Sculpting(float pickX, float pickY, float pickZ);
	void GGTerrain_Update_Painting(float pickX, float pickY, float pickZ);
	void GGTerrain_TriggerPaintTextureLoad(void);
}
void GGTerrain_CreateUndoRedoAction(int type, int eList, bool bUserAction = true, void* pEventData = nullptr);
extern int g_iCalculatingChangeBounds;

// terrain tests only make sense in the level editor with the terrain system live
static bool AutoHarness_TerrainEditAllowed(void)
{
	if (bWelcomeScreen_Window || bStoryboardWindow || bImGuiInTestGame) return false;
	if (!GGTerrain::ggterrain_initialised) return false;
	return true;
}

static int   s_terrainTestFrames = 0;
static int   s_terrainTestPaint = 0;       // 0 = sculpt run, 1 = paint run
static int   s_terrainTestSculptMode = 0;  // GGTERRAIN_SCULPT_* for sculpt runs
static float s_terrainTestX = 0.0f, s_terrainTestZ = 0.0f;
static int   s_terrainTestMaterial = 0;
static int   s_terrainTestSavedEditMode = 0;
static int   s_terrainTestSavedSculptMode = 0;
static int   s_terrainTestSavedPaintMat = 0;

static void AutoHarness_TerrainEditTick(void)
{
	using namespace GGTerrain;
	if (s_terrainTestFrames <= 0) return;
	if (!AutoHarness_TerrainEditAllowed())
	{
		// state changed under us (test game started, level closed) — cut the run short;
		// fall through with no more apply calls so the end-of-run block below still
		// finalizes any armed undo and hands the editor its modes back
		s_terrainTestFrames = 0;
	}
	else
	{
	s_terrainTestFrames--;

	float y = 0.0f;
	GGTerrain_GetHeight(s_terrainTestX, s_terrainTestZ, &y);

	if (s_terrainTestPaint == 0)
	{
		ggterrain_extra_params.edit_mode = GGTERRAIN_EDIT_SCULPT;
		ggterrain_extra_params.sculpt_mode = s_terrainTestSculptMode;
		ggterrain_internal_params.mouseLeftState = 1;   // consumed inside the apply call
		GGTerrain_Update_Sculpting(s_terrainTestX, y, s_terrainTestZ);
	}
	else
	{
		ggterrain_extra_params.edit_mode = GGTERRAIN_EDIT_PAINT;
		ggterrain_extra_params.paint_material = s_terrainTestMaterial;
		ggterrain_internal_params.mouseLeftState = 1;
		// run the same first-use texture-load trigger as the real editor dispatch —
		// skipping it is how the first-stroke texture-load glitch escaped testing
		GGTerrain_TriggerPaintTextureLoad();
		GGTerrain_Update_Painting(s_terrainTestX, y, s_terrainTestZ);
	}
	}

	if (s_terrainTestFrames <= 0)
	{
		// stroke over: finalize the undo action the way the editor release path does,
		// then hand the editor its modes back (skip the undo if the terrain went away
		// under an abandoned run — the snapshot buffers may be gone with it)
		if (g_iCalculatingChangeBounds && ggterrain_initialised)
		{
			g_iCalculatingChangeBounds = 0;
			GGTerrain_CreateUndoRedoAction(s_terrainTestPaint == 0 ? eUndoSys_Terrain_Sculpt : eUndoSys_Terrain_Paint, eUndoSys_UndoList);
		}
		ggterrain_internal_params.mouseLeftState = 0;
		ggterrain_extra_params.edit_mode = s_terrainTestSavedEditMode;
		ggterrain_extra_params.sculpt_mode = s_terrainTestSavedSculptMode;
		ggterrain_extra_params.paint_material = s_terrainTestSavedPaintMat;
	}
}

// GGMAX 3.118: GRASS_PAINT_TEST - a synthetic Grass tool SPRAY stroke through the shipped
// GGGrass_Update_Painting (the sibling of PAINT_TEST). Edit mode, paint type/material/mode and the stroke
// mouse state are set ONLY around our own call and restored straight after, so the editor's real grass
// dispatch (driven by the real mouse) never sees the synthetic state. The last frame is the release
// frame: Update_Painting itself finalizes the undo action and publishes the stroke rect then.
extern int gggrass_initialised;   // GGGrass.cpp (global, not in the header)
static int      s_grassTestFrames = 0, s_grassTestFrame = 0, s_grassTestMatch = 1;
static uint64_t s_grassTestType = 1;
static float    s_grassTestX = 0.0f, s_grassTestZ = 0.0f;
static void AutoHarness_GrassPaintTick(void)
{
	using namespace GGTerrain;
	if (s_grassTestFrames <= 0) return;
	if (!AutoHarness_TerrainEditAllowed())
	{
		// abandoned mid-stroke (Test Game, storyboard, level closed - review 3.118): the first frame armed the
		// SHARED terrain/grass undo slot (g_iCalculatingChangeBounds + g_pTerrainSnapshot). Run the release
		// frame so Update_Painting clears it, uploads and publishes; with the grass system gone just disarm.
		if (s_grassTestFrame == 0) { s_grassTestFrames = 0; return; }
		if (!::gggrass_initialised || !ggterrain_initialised)
		{
			g_iCalculatingChangeBounds = 0;
			s_grassTestFrames = 0;
			return;
		}
		s_grassTestFrames = 1;   // this call becomes the release frame
	}
	const int savedEdit = ggterrain_extra_params.edit_mode;
	const int savedMat = GGGrass::gggrass_global_params.paint_material;
	const uint64_t savedType = GGGrass::gggrass_global_params.paint_type;
	const int savedMode = GGGrass::gggrass_global_params.paint_mode;
	ggterrain_extra_params.edit_mode = GGTERRAIN_EDIT_GRASS;
	GGGrass::gggrass_global_params.paint_material = s_grassTestMatch ? 0 : 1;
	GGGrass::gggrass_global_params.paint_type = s_grassTestType;
	GGGrass::gggrass_global_params.paint_mode = GGGRASS_PAINT_SPRAY;
	const bool last = (s_grassTestFrames == 1);
	GGGrass::gggrass_internal_params.mouseLeftPressed = (s_grassTestFrame == 0) ? 1 : 0;
	GGGrass::gggrass_internal_params.mouseLeftState = last ? 0 : 1;
	GGGrass::gggrass_internal_params.mouseLeftReleased = last ? 1 : 0;
	RAY ray(XMVectorSet(s_grassTestX, 50000.0f, s_grassTestZ, 0.0f), XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
	GGGrass::GGGrass_Update_Painting(ray);
	GGGrass::gggrass_internal_params.mouseLeftPressed = 0;
	GGGrass::gggrass_internal_params.mouseLeftState = 0;
	GGGrass::gggrass_internal_params.mouseLeftReleased = 0;
	ggterrain_extra_params.edit_mode = savedEdit;
	GGGrass::gggrass_global_params.paint_material = savedMat;
	GGGrass::gggrass_global_params.paint_type = savedType;
	GGGrass::gggrass_global_params.paint_mode = savedMode;
	s_grassTestFrame++;
	s_grassTestFrames--;
}

// GGMAX 3.123: TREE_PAINT_TEST - a synthetic Trees tool SPRAY / SPRAY REMOVE stroke through the shipped
// GGTrees_Update_Painting (sibling of GRASS_PAINT_TEST), for the tree undo path: eUndoSys_Terrain_PaintTree
// restores all 400000 records with one raw memcpy, the only way any bit pattern can reach pAllTrees.
// [inject N] then writes an invalid type (45) into the first N VISIBLE records of the stroke's undo snapshot
// right after the stroke takes it - what a corrupt snapshot would carry - so the UNDO must strip them
// (hidden, type cleared): GET_TREE_LOADSTATS saved_badtype reads 0 after it, saved_visible drops by N.
// Edit mode, paint mode and the stroke mouse state are set ONLY around our own call. In memory only.
namespace GGTrees { extern GGTreesInternalParams ggtrees_internal_params; }
extern int ggtrees_initialised;      // GGTrees_part0.cpp (global, not in the header)
extern int g_iActiveTreeEvent;       // GGTrees_part0.cpp: 1 while a spray stroke's snapshot is held
extern uint8_t* g_pTerrainSnapshot;  // M-UndoSys-Terrain.cpp: the snapshot buffer terrain, grass and trees share
static int   s_treeTestFrames = 0, s_treeTestFrame = 0, s_treeTestMode = GGTREES_PAINT_SPRAY, s_treeTestInject = 0;
static int   s_treeTestInjected = -1;   // -1 = no injection asked, else how many records were rewritten
static float s_treeTestX = 0.0f, s_treeTestZ = 0.0f;
static void AutoHarness_TreePaintTick(void)
{
	using namespace GGTerrain;
	if (s_treeTestFrames <= 0) return;
	if (!AutoHarness_TerrainEditAllowed())
	{
		// abandoned mid-stroke: run the release frame so the stroke's undo action is made and
		// g_iActiveTreeEvent clears (it holds the shared snapshot); with the trees gone just disarm
		if (s_treeTestFrame == 0) { s_treeTestFrames = 0; return; }
		if (!::ggtrees_initialised) { ::g_iActiveTreeEvent = 0; s_treeTestFrames = 0; return; }
		s_treeTestFrames = 1;   // this call becomes the release frame
	}
	const int savedEdit = ggterrain_extra_params.edit_mode;
	const int savedMode = GGTrees::ggtrees_global_params.paint_mode;
	ggterrain_extra_params.edit_mode = GGTERRAIN_EDIT_TREES;
	GGTrees::ggtrees_global_params.paint_mode = s_treeTestMode;
	const bool last = (s_treeTestFrames == 1);
	GGTrees::ggtrees_internal_params.mouseLeftPressed = (s_treeTestFrame == 0) ? 1 : 0;
	GGTrees::ggtrees_internal_params.mouseLeftState = last ? 0 : 1;
	GGTrees::ggtrees_internal_params.mouseLeftReleased = last ? 1 : 0;
	RAY ray(XMVectorSet(s_treeTestX, 50000.0f, s_treeTestZ, 0.0f), XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));
	GGTrees::GGTrees_Update_Painting(ray);
	if (s_treeTestFrame == 0 && s_treeTestInject > 0)
	{
		// the first held frame took the snapshot (GGTrees_GetSnapshot: InstanceTree x,y,z,data,id)
		struct SnapTree { float x, y, z; uint32_t data; uint32_t id; };
		static_assert(sizeof(SnapTree) == 20, "InstanceTree layout");
		s_treeTestInjected = 0;
		if (::g_iActiveTreeEvent && ::g_pTerrainSnapshot)
		{
			SnapTree* pSnap = (SnapTree*)::g_pTerrainSnapshot;
			const uint32_t n = (GGTrees::GGTrees_GetDataSize() - 1) / 3;
			for (uint32_t i = 0; i < n && s_treeTestInjected < s_treeTestInject; i++)
			{
				if ((pSnap[i].data & 1) == 0) continue;
				pSnap[i].data = (pSnap[i].data & ~(0x3Fu << 11)) | (45u << 11);
				s_treeTestInjected++;
			}
		}
	}
	GGTrees::ggtrees_internal_params.mouseLeftPressed = 0;
	GGTrees::ggtrees_internal_params.mouseLeftState = 0;
	GGTrees::ggtrees_internal_params.mouseLeftReleased = 0;
	ggterrain_extra_params.edit_mode = savedEdit;
	GGTrees::ggtrees_global_params.paint_mode = savedMode;
	s_treeTestFrame++;
	s_treeTestFrames--;
}

// ---------------------------------------------------------------------------
// VERIFY_MESH / VERIFY_WATCH — GPU mesh-buffer content oracle (reload-corruption hunt).
//
// All mesh data lives suballocated inside a shared global GPU buffer (see
// wiScene_Components.cpp CreateRenderData -> SuballocateGPUBuffer), so a suballocator
// lifecycle race can stomp one mesh's live range with another's upload while every
// CPU-side record stays correct. These commands detect that without eyes:
//
//   VERIFY_MESH <name-substr>  snapshot each matching mesh's generalBuffer bytes,
//                              CreateRenderData() (re-upload from the intact CPU
//                              arrays), snapshot again, byte-compare region by region.
//                              Mismatch = the GPU copy was NOT what the CPU data
//                              produces = content corruption (now healed as a side
//                              effect). Meshes with freed CPU arrays are SKIPPED.
//   VERIFY_WATCH <name-substr> two snapshots ~60 frames apart with NO re-upload —
//                              any diff means an ACTIVE writer stomped the buffer
//                              while the scene sat still. Zero side effects.
//
// Readback runs through the frame's own command lists (BeginCommandList on this
// thread joins the current frame submit); data is read 3 frames later when the
// frame fence guarantees completion. Corrupt meshes only are detailed in
// Files/verify_mesh.txt; the summary is (re)written to auto_result.txt at the end.
// ---------------------------------------------------------------------------

struct VerifyRegion
{
	const char* name;
	uint64_t offA;      // offset within generalBuffer at snapshot A
	uint64_t offB;      // offset at snapshot B (verify mode rebuild may relocate; ~0 = same as A)
	uint64_t size;
};

struct VerifyTarget
{
	wi::ecs::Entity meshID = wi::ecs::INVALID_ENTITY;
	std::string label;                  // object/mesh name for the report
	XMFLOAT3 pos = XMFLOAT3(0, 0, 0);   // one instance's world position (editor cross-ref)
	uint64_t bufSizeA = 0;
	uint64_t bufSizeB = 0;
	wi::vector<VerifyRegion> regions;
	wi::graphics::GPUBuffer rbA;
	wi::graphics::GPUBuffer rbB;
	std::vector<uint8_t> bytesA;
	bool skipped = false;
	const char* note = "";
};

static std::vector<VerifyTarget> s_verifyTargets;
static int      s_verifyPhase = 0;          // 0 idle, 1 record-A, 2 wait-A, 3 record-B, 4 wait-B+compare
static int      s_verifyMode = 0;           // 0 = VERIFY (re-upload between snapshots), 1 = WATCH
static uint64_t s_verifyFrameA = 0;
static uint64_t s_verifyFrameB = 0;
static uint64_t s_verifyWatchUntil = 0;     // WATCH: frame to wait for before snapshot B
static size_t   s_verifyWaveStart = 0;      // current wave = [start, end) into s_verifyTargets
static size_t   s_verifyWaveEnd = 0;
static int      s_verifyCorrupt = 0;        // corrupt meshes so far
static int      s_verifySkipped = 0;
static int      s_verifyChecked = 0;
static FILE*    s_verifyFile = nullptr;
static size_t   s_verifyRebuildCursor = (size_t)-1; // phase-2 re-upload budget cursor
static DWORD    s_verifyPhaseTick = 0;              // watchdog: last phase-change time
static bool     s_verifyBurst = false;              // "burst" arg: unbudgeted rebuild storm (allocator-race repro)

static void VerifyCaptureRegions(const wi::scene::MeshComponent* mesh, wi::vector<VerifyRegion>& out, bool asB)
{
	auto add = [&](const char* nm, const wi::scene::MeshComponent::BufferView& v)
	{
		if (!v.IsValid() || v.size == 0) return;
		if (asB)
		{
			for (auto& r : out) { if (strcmp(r.name, nm) == 0) { r.offB = v.offset; return; } }
		}
		else
		{
			VerifyRegion r; r.name = nm; r.offA = v.offset; r.offB = ~0ull; r.size = v.size;
			out.push_back(r);
		}
	};
	add("ib", mesh->ib);
	add("vb_pos_wind", mesh->vb_pos_wind);
	add("vb_nor", mesh->vb_nor);
	add("vb_tan", mesh->vb_tan);
	add("vb_uvs", mesh->vb_uvs);
	add("vb_atl", mesh->vb_atl);
	add("vb_col", mesh->vb_col);
	add("vb_bon", mesh->vb_bon);
}

// Record whole-generalBuffer copies into readback buffers for wave targets. rbField selects A or B.
static bool VerifyRecordSnapshot(bool snapB)
{
	using namespace wi::graphics;
	GraphicsDevice* device = GetDevice();
	wi::scene::Scene& scene = wi::scene::GetScene();
	CommandList cmd;
	bool began = false;
	for (size_t i = s_verifyWaveStart; i < s_verifyWaveEnd; i++)
	{
		VerifyTarget& t = s_verifyTargets[i];
		if (t.skipped) continue;
		wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(t.meshID);
		if (mesh == nullptr || !mesh->generalBuffer.IsValid())
		{
			t.skipped = true; t.note = "mesh gone mid-verify"; continue;
		}
		uint64_t bufSize = mesh->generalBuffer.desc.size;
		if (snapB)
		{
			t.bufSizeB = bufSize;
			VerifyCaptureRegions(mesh, t.regions, true);
		}
		else
		{
			t.bufSizeA = bufSize;
			VerifyCaptureRegions(mesh, t.regions, false);
		}
		GPUBufferDesc rbd;
		rbd.size = bufSize;
		rbd.usage = Usage::READBACK;
		GPUBuffer& rb = snapB ? t.rbB : t.rbA;
		if (!device->CreateBuffer(&rbd, nullptr, &rb) || rb.mapped_data == nullptr)
		{
			t.skipped = true; t.note = "readback alloc failed"; continue;
		}
		if (!began) { cmd = device->BeginCommandList(); began = true; }
		GPUBarrier pre = GPUBarrier::Buffer(&mesh->generalBuffer, ResourceState::SHADER_RESOURCE, ResourceState::COPY_SRC);
		device->Barrier(&pre, 1, cmd);
		device->CopyBuffer(&rb, 0, &mesh->generalBuffer, 0, bufSize, cmd);
		GPUBarrier post = GPUBarrier::Buffer(&mesh->generalBuffer, ResourceState::COPY_SRC, ResourceState::SHADER_RESOURCE);
		device->Barrier(&post, 1, cmd);
	}
	return began;
}

static void AutoHarness_VerifyTick(void)
{
	using namespace wi::graphics;
	if (s_verifyPhase == 0) return;
	GraphicsDevice* device = GetDevice();
	if (device == nullptr) { s_verifyPhase = 0; return; }
	uint64_t frame = device->GetFrameCount();
	wi::scene::Scene& scene = wi::scene::GetScene();

	// Watchdog: a phase stuck >30s (device removed, frame counter frozen) aborts cleanly
	// instead of leaving the driver polling a result that will never come.
	static int s_watchLastPhase = 0;
	static size_t s_watchLastCursor = 0;
	if (s_verifyPhase != s_watchLastPhase || s_verifyRebuildCursor != s_watchLastCursor)
	{
		s_watchLastPhase = s_verifyPhase;
		s_watchLastCursor = s_verifyRebuildCursor;
		s_verifyPhaseTick = GetTickCount();
	}
	else if (GetTickCount() - s_verifyPhaseTick > 30000)
	{
		char aborted[256];
		_snprintf(aborted, sizeof(aborted), "ERROR: verify ABORTED by watchdog (phase %d stuck 30s, wave %d..%d) — device may be lost",
			s_verifyPhase, (int)s_verifyWaveStart, (int)s_verifyWaveEnd);
		aborted[sizeof(aborted) - 1] = 0;
		if (s_verifyFile) { fprintf(s_verifyFile, "%s\n", aborted); fclose(s_verifyFile); s_verifyFile = nullptr; }
		AutoHarness_WriteResult(aborted);
		s_verifyTargets.clear();
		s_verifyPhase = 0;
		s_verifyRebuildCursor = (size_t)-1;
		s_watchLastPhase = 0;
		return;
	}

	if (s_verifyPhase == 1)
	{
		// Open a new wave: batch targets up to a memory cap so snapshots stay bounded.
		const uint64_t WAVE_BYTE_CAP = 96ull * 1024ull * 1024ull;
		uint64_t waveBytes = 0;
		s_verifyWaveEnd = s_verifyWaveStart;
		while (s_verifyWaveEnd < s_verifyTargets.size())
		{
			VerifyTarget& t = s_verifyTargets[s_verifyWaveEnd];
			if (!t.skipped)
			{
				wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(t.meshID);
				uint64_t sz = (mesh != nullptr && mesh->generalBuffer.IsValid()) ? mesh->generalBuffer.desc.size : 0;
				if (waveBytes > 0 && waveBytes + sz > WAVE_BYTE_CAP) break;
				waveBytes += sz;
			}
			s_verifyWaveEnd++;
		}
		VerifyRecordSnapshot(false);
		s_verifyFrameA = frame;
		s_verifyPhase = 2;
	}
	else if (s_verifyPhase == 2)
	{
		if (frame < s_verifyFrameA + 3) return; // frame fence: snapshot A complete when N-2 finished
		if (s_verifyRebuildCursor == (size_t)-1)
		{
			for (size_t i = s_verifyWaveStart; i < s_verifyWaveEnd; i++)
			{
				VerifyTarget& t = s_verifyTargets[i];
				if (t.skipped) continue;
				t.bytesA.resize((size_t)t.bufSizeA);
				memcpy(t.bytesA.data(), t.rbA.mapped_data, (size_t)t.bufSizeA);
				t.rbA = {};
			}
			s_verifyRebuildCursor = s_verifyWaveStart;
		}
		if (s_verifyMode == 0)
		{
			// Budget the re-uploads: a single-frame storm of hundreds of CreateRenderData calls
			// (mass suballocator free+alloc) is exactly the churn that detonates the allocator —
			// spread them so the tool doesn't out-abuse the bug it hunts.
			int rebuilt = 0;
			const int rebuildBudget = s_verifyBurst ? 1000000 : 48; // burst = deliberate one-frame allocator storm (race repro)
			while (s_verifyRebuildCursor < s_verifyWaveEnd && rebuilt < rebuildBudget)
			{
				VerifyTarget& t = s_verifyTargets[s_verifyRebuildCursor];
				if (!t.skipped)
				{
					wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(t.meshID);
					if (mesh != nullptr) { mesh->CreateRenderData(); rebuilt++; } // re-upload from intact CPU arrays
				}
				s_verifyRebuildCursor++;
			}
			if (s_verifyRebuildCursor < s_verifyWaveEnd) return; // continue next tick
		}
		s_verifyRebuildCursor = (size_t)-1;
		s_verifyWatchUntil = s_verifyFrameA + 60;
		s_verifyPhase = 3;
	}
	else if (s_verifyPhase == 3)
	{
		if (s_verifyMode == 1 && frame < s_verifyWatchUntil) return; // WATCH: give a writer time to strike
		VerifyRecordSnapshot(true);
		s_verifyFrameB = frame;
		s_verifyPhase = 4;
	}
	else if (s_verifyPhase == 4)
	{
		if (frame < s_verifyFrameB + 3) return;
		for (size_t i = s_verifyWaveStart; i < s_verifyWaveEnd; i++)
		{
			VerifyTarget& t = s_verifyTargets[i];
			if (t.skipped)
			{
				s_verifySkipped++;
				if (s_verifyFile) fprintf(s_verifyFile, "SKIP mesh=%u label=\"%s\" pos=(%.0f,%.0f,%.0f) note=%s\n",
					(unsigned)t.meshID, t.label.c_str(), t.pos.x, t.pos.y, t.pos.z, t.note);
				continue;
			}
			s_verifyChecked++;
			const uint8_t* bytesB = (const uint8_t*)t.rbB.mapped_data;
			int corruptRegions = 0;
			char regionReport[1024];
			regionReport[0] = 0;
			for (auto& r : t.regions)
			{
				uint64_t offB = (r.offB == ~0ull) ? r.offA : r.offB;
				if (offB + r.size > t.bufSizeB || r.offA + r.size > t.bytesA.size())
				{
					corruptRegions++;
					_snprintf(regionReport + strlen(regionReport), sizeof(regionReport) - strlen(regionReport) - 1,
						"  region %s LAYOUT-CHANGED (A off=%llu B off=%llu size=%llu bufB=%llu)\n",
						r.name, (unsigned long long)r.offA, (unsigned long long)offB,
						(unsigned long long)r.size, (unsigned long long)t.bufSizeB);
					continue;
				}
				const uint32_t* wa = (const uint32_t*)(t.bytesA.data() + r.offA);
				const uint32_t* wb = (const uint32_t*)(bytesB + offB);
				uint64_t words = r.size / 4;
				uint64_t firstBad = ~0ull, badCount = 0;
				for (uint64_t w = 0; w < words; w++)
				{
					if (wa[w] != wb[w])
					{
						if (firstBad == ~0ull) firstBad = w * 4;
						badCount++;
					}
				}
				if (badCount > 0)
				{
					corruptRegions++;
					uint64_t fb = firstBad / 4;
					uint32_t a0 = wa[fb], b0 = wb[fb];
					uint32_t a1 = (fb + 1 < words) ? wa[fb + 1] : 0, b1 = (fb + 1 < words) ? wb[fb + 1] : 0;
					_snprintf(regionReport + strlen(regionReport), sizeof(regionReport) - strlen(regionReport) - 1,
						"  region %s CORRUPT first@+0x%llX badwords=%llu/%llu A=%08X %08X B=%08X %08X\n",
						r.name, (unsigned long long)firstBad, (unsigned long long)badCount, (unsigned long long)words,
						a0, a1, b0, b1);
				}
			}
			if (corruptRegions > 0)
			{
				s_verifyCorrupt++;
				if (s_verifyFile)
				{
					fprintf(s_verifyFile, "MESH %u label=\"%s\" pos=(%.0f,%.0f,%.0f) size=%llu VERDICT=%s\n%s",
						(unsigned)t.meshID, t.label.c_str(), t.pos.x, t.pos.y, t.pos.z,
						(unsigned long long)t.bufSizeA,
						(s_verifyMode == 1) ? "ACTIVE-WRITER" : "CORRUPT-HEALED", regionReport);
					fflush(s_verifyFile); // progress visible to the external driver mid-run
				}
			}
			t.rbB = {};
			t.bytesA.clear();
			t.bytesA.shrink_to_fit();
		}
		s_verifyWaveStart = s_verifyWaveEnd;
		if (s_verifyWaveStart < s_verifyTargets.size())
		{
			s_verifyPhase = 1; // next wave
		}
		else
		{
			char summary[512];
			_snprintf(summary, sizeof(summary),
				"OK: %s complete — checked=%d corrupt=%d skipped=%d (of %d targets) -> verify_mesh.txt",
				(s_verifyMode == 1) ? "VERIFY_WATCH" : "VERIFY_MESH",
				s_verifyChecked, s_verifyCorrupt, s_verifySkipped, (int)s_verifyTargets.size());
			summary[sizeof(summary) - 1] = 0;
			if (s_verifyFile)
			{
				fprintf(s_verifyFile, "%s\n", summary);
				fclose(s_verifyFile);
				s_verifyFile = nullptr;
			}
			AutoHarness_WriteResult(summary); // rewrite auto_result.txt so the driver sees completion
			s_verifyTargets.clear();
			s_verifyPhase = 0;
		}
	}
}

// ---- Main entry point (called once per tick) ----

// SET_TANGENTVIS CYCLE state: auto-advance gg_debugvis 0..20, 3s dwell each.
// Mode 0 (normal shading, flicker visible) marks the start of each loop.
bool g_tvCycleActive = false;

// DUMP_SOTAN two-frame streamout snapshot state (tangent-w coin-flip byte forensics)
static std::vector<uint8_t> g_sotanSnapA;
static std::vector<uint8_t> g_sotanSrcA;
static wi::ecs::Entity g_sotanMesh = wi::ecs::INVALID_ENTITY;
static int g_sotanPhase = 0;
static char g_sotanLabel[160] = { 0 };

static void Sotan_ReadbackBuffer(const wi::graphics::GPUBuffer& buf, std::vector<uint8_t>& out)
{
	using namespace wi::graphics;
	GraphicsDevice* device = GetDevice();
	GPUBufferDesc rb;
	rb.size = buf.GetDesc().size;
	rb.usage = Usage::READBACK;
	GPUBuffer staging;
	if (!device->CreateBuffer(&rb, nullptr, &staging)) { out.clear(); return; }
	CommandList cmd = device->BeginCommandList();
	device->CopyResource(&staging, &buf, cmd);
	device->SubmitCommandLists();
	device->WaitForGPU();
	out.resize((size_t)rb.size);
	memcpy(out.data(), staging.mapped_data, (size_t)rb.size);
}

static void Sotan_Tick(void)
{
	if (g_sotanPhase <= 0) return;
	wi::scene::Scene& scn = wi::scene::GetScene();
	const wi::scene::MeshComponent* mesh = scn.meshes.GetComponent(g_sotanMesh);
	if (mesh == nullptr || !mesh->streamoutBuffer.IsValid() || !mesh->so_tan.IsValid()) { g_sotanPhase = 0; return; }
	if (g_sotanPhase == 1)
	{
		Sotan_ReadbackBuffer(mesh->streamoutBuffer, g_sotanSnapA);
		Sotan_ReadbackBuffer(mesh->generalBuffer, g_sotanSrcA);
		g_sotanPhase = g_sotanSnapA.empty() ? 0 : 2;
		return;
	}
	// phase 2: snapshot B one frame later, compare, report
	std::vector<uint8_t> B, srcB;
	Sotan_ReadbackBuffer(mesh->streamoutBuffer, B);
	Sotan_ReadbackBuffer(mesh->generalBuffer, srcB);
	g_sotanPhase = 0;
	if (B.empty() || B.size() != g_sotanSnapA.size()) return;
	FILE* f = GGDiagFopen("sotan_dump.txt", "w");
	if (f == nullptr) return;
	const uint64_t tanOfs = mesh->so_tan.offset;
	const int nverts = (int)(mesh->so_tan.size / 8ull); // R16G16B16A16_FLOAT
	const uint8_t* A = g_sotanSnapA.data();
	const uint8_t* Bp = B.data();
	int wflips = 0, xyzchanged = 0, wgarbage = 0, listed = 0;
	fprintf(f, "SOTAN %s  buffer=%llu bytes  so_tan ofs=%llu size=%llu verts=%d\n",
		g_sotanLabel, (unsigned long long)B.size(), (unsigned long long)tanOfs, (unsigned long long)mesh->so_tan.size, nverts);
	fprintf(f, "views: so_pos ofs=%llu so_pre ofs=%llu so_nor ofs=%llu\n",
		(unsigned long long)mesh->so_pos.offset, (unsigned long long)mesh->so_pre.offset, (unsigned long long)mesh->so_nor.offset);
	for (int v = 0; v < nverts; v++)
	{
		const uint16_t* ta = (const uint16_t*)(A + tanOfs + (uint64_t)v * 8ull);
		const uint16_t* tb = (const uint16_t*)(Bp + tanOfs + (uint64_t)v * 8ull);
		bool wf = ((ta[3] ^ tb[3]) & 0x8000) != 0;
		bool xyz = (ta[0] != tb[0]) || (ta[1] != tb[1]) || (ta[2] != tb[2]);
		if (wf) wflips++;
		if (xyz) xyzchanged++;
		// legit w halves: +-1.0 = 0x3C00/0xBC00 (allow denorm-ish garbage detection)
		uint16_t mag = tb[3] & 0x7FFF;
		if (mag != 0x3C00 && mag != 0x0000) wgarbage++;
		if (wf && listed < 12)
		{
			fprintf(f, "  v%d A(xyzw)=%04x %04x %04x %04x  B=%04x %04x %04x %04x\n",
				v, ta[0], ta[1], ta[2], ta[3], tb[0], tb[1], tb[2], tb[3]);
			listed++;
		}
	}
	// contiguity fingerprint of flips: count runs
	int runs = 0; bool inrun = false;
	for (int v = 0; v < nverts; v++)
	{
		const uint16_t* ta = (const uint16_t*)(A + tanOfs + (uint64_t)v * 8ull);
		const uint16_t* tb = (const uint16_t*)(Bp + tanOfs + (uint64_t)v * 8ull);
		bool wf = ((ta[3] ^ tb[3]) & 0x8000) != 0;
		if (wf && !inrun) { runs++; inrun = true; }
		else if (!wf) inrun = false;
	}
	// w value census (snapshot B): quantization noise vs garbage discriminator
	{
		auto halfToFloat = [](uint16_t h) -> float {
			uint32_t sgn = (h & 0x8000u) << 16; uint32_t exp = (h >> 10) & 0x1F; uint32_t man = h & 0x3FFu;
			uint32_t f;
			if (exp == 0) { if (man == 0) f = sgn; else { exp = 127 - 15 + 1; while ((man & 0x400u) == 0) { man <<= 1; exp--; } man &= 0x3FFu; f = sgn | (exp << 23) | (man << 13); } }
			else if (exp == 31) f = sgn | 0x7F800000u | (man << 13);
			else f = sgn | ((exp - 15 + 127) << 23) | (man << 13);
			float out; memcpy(&out, &f, 4); return out;
		};
		int histNeg1 = 0, histNearNeg = 0, histNearZero = 0, histNearPos = 0, histPos1 = 0, histWild = 0, printed = 0;
		fprintf(f, "w census (B): ");
		for (int v = 0; v < nverts; v++)
		{
			const uint16_t* tb = (const uint16_t*)(Bp + tanOfs + (uint64_t)v * 8ull);
			float wv = halfToFloat(tb[3]);
			if (wv <= -0.999f && wv >= -1.001f) histNeg1++;
			else if (wv < -0.5f) histNearNeg++;
			else if (wv < 0.5f) histNearZero++;
			else if (wv < 0.999f) histNearPos++;
			else if (wv <= 1.001f) histPos1++;
			else histWild++;
			if (printed < 12 && !(wv == 1.0f || wv == -1.0f || wv == 0.0f))
			{
				fprintf(f, "v%d=%.4f(%04x) ", v, wv, tb[3]);
				printed++;
			}
		}
		fprintf(f, "\nw histogram: [-1]=%d (-1,-0.5)=%d (-0.5,0.5)=%d (0.5,1)=%d [+1]=%d wild=%d\n",
			histNeg1, histNearNeg, histNearZero, histNearPos, histPos1, histWild);
	}
	fprintf(f, "SUMMARY verts=%d w-sign-flips=%d (%.1f%%) in %d contiguous runs; xyz-changed=%d (%.1f%%); w-not-(+-1|0)=%d\n",
		nverts, wflips, 100.0f * wflips / (nverts > 0 ? nverts : 1), runs, xyzchanged, 100.0f * xyzchanged / (nverts > 0 ? nverts : 1), wgarbage);
	// RLE fingerprint of the B-snapshot w lane: block boundaries + values identify the foreign
	// writer (4096-aligned = heap/page aliasing; 512/64-element groups = dispatch-shaped;
	// subset-offset-aligned = mesh-structure writer).
	{
		fprintf(f, "w RLE (B): ");
		uint16_t curv = 0xFFFF; int runlen = 0; int printedRuns = 0; int startV = 0;
		for (int v = 0; v <= nverts && printedRuns < 48; v++)
		{
			uint16_t wv = 0xFFFF;
			if (v < nverts) wv = ((const uint16_t*)(Bp + tanOfs + (uint64_t)v * 8ull))[3];
			if (wv != curv)
			{
				if (runlen > 0)
				{
					uint64_t byteofs = tanOfs + (uint64_t)startV * 8ull;
					fprintf(f, "[v%d+%d w=%04x ofs%%4096=%llu] ", startV, runlen, curv, (unsigned long long)(byteofs % 4096ull));
					printedRuns++;
				}
				curv = wv; runlen = 1; startV = v;
			}
			else runlen++;
		}
		fprintf(f, "\n");
	}
	// so_nor + so_pos w-lane footprint: the CS writes float4(nor,0) and float4(pos,0) — any
	// nonzero w there = the foreign writer's footprint extends beyond the tan region.
	{
		const uint64_t norOfs = mesh->so_nor.offset;
		const int norVerts = (int)(mesh->so_nor.size / 8ull);
		int norNonzero = 0;
		for (int v = 0; v < norVerts; v++)
			if (((const uint16_t*)(Bp + norOfs + (uint64_t)v * 8ull))[3] != 0) norNonzero++;
		const uint64_t posOfs = mesh->so_pos.offset;
		const int posVerts = (int)(mesh->so_pos.size / 16ull);
		int posNonzero = 0;
		for (int v = 0; v < posVerts; v++)
			if (((const uint32_t*)(Bp + posOfs + (uint64_t)v * 16ull))[3] != 0) posNonzero++;
		fprintf(f, "FOOTPRINT: so_nor w!=0 on %d/%d verts; so_pos w!=0 on %d/%d verts\n",
			norNonzero, norVerts, posNonzero, posVerts);
	}
	// SOURCE side: the generalBuffer vb_tan region (R8G8B8A8_SNORM, 4 B/vertex; canonical w byte
	// = 0x7F/+1, 0x81/-1 (or 0x80), 0x00). Junk here = source poisoned; A!=B = live stomping.
	if (mesh->vb_tan.IsValid() && !g_sotanSrcA.empty() && srcB.size() == g_sotanSrcA.size())
	{
		const uint64_t srcOfs = mesh->vb_tan.offset;
		const int srcVerts = (int)(mesh->vb_tan.size / 4ull);
		int srcCanon = 0, srcChanged = 0, srcListed = 0;
		fprintf(f, "SOURCE vb_tan ofs=%llu size=%llu verts=%d samples: ", (unsigned long long)srcOfs, (unsigned long long)mesh->vb_tan.size, srcVerts);
		for (int v = 0; v < srcVerts; v++)
		{
			const uint8_t* sa = g_sotanSrcA.data() + srcOfs + (uint64_t)v * 4ull;
			const uint8_t* sb = srcB.data() + srcOfs + (uint64_t)v * 4ull;
			uint8_t wb = sb[3];
			if (wb == 0x7F || wb == 0x7E || wb == 0x81 || wb == 0x80 || wb == 0x82 || wb == 0x00) srcCanon++;
			if (memcmp(sa, sb, 4) != 0) srcChanged++;
			if (srcListed < 10 && !(wb == 0x7F || wb == 0x7E || wb == 0x81 || wb == 0x80 || wb == 0x82 || wb == 0x00))
			{
				fprintf(f, "v%d=%02x%02x%02x%02x ", v, sb[0], sb[1], sb[2], sb[3]);
				srcListed++;
			}
		}
		fprintf(f, "\nSOURCE SUMMARY: canonical-w=%d/%d (%.1f%%) changed-A-to-B=%d\n",
			srcCanon, srcVerts, 100.0f * srcCanon / (srcVerts > 0 ? srcVerts : 1), srcChanged);
	}
	fclose(f);
}

// GGMAX 2026-08-05: standalone-game (PLAY GAME / Guru-Game identity) commands, hoisted
// helper per the C1061 pattern. Returns true if cmd was handled.
extern "C" int GGAuto_MapScreenActionName(const char* name); // M-GridEditB_part22.cpp
namespace wi { namespace renderer {
	uint32_t GG_GetShadowRects(uint32_t* entities, int* widths, int* heights, int* types, uint32_t maxn, float* scale, int* slices); // engine 2.06, slices 3.68
} }
// GGMAX 2026-08-06: shadow-budget commands hoisted out of the main dispatch chain —
// adding SET_SHADOW_MAX_SPOT/_POINT as chain links re-hit MSVC C1061 (every else-if
// link nests one block deeper). Returns true if cmd was handled.
// GGMAX 3.55: bullet-tracer status, hoisted per the C1061 pattern (the main ladder is at the
// MSVC nesting limit - see the note above the dispatch function). Answers in one line WHICH
// link of the chain is broken when no streaks appear:
//   ready=0           -> shaders/PSO never came up
//   spawned_total=0   -> gameplay never called AddTracer (wrong gun, traceractive=0, not in game)
//   drawpasses=0      -> customDraw_Transparent is not reaching tracer_draw (the 7-month bug)
//   texslots_valid=0  -> the DDS loader found no tracer.dds for the equipped gun
// GGMAX 3.57: tree sway. SET_TREEWIND <0-100> drives visuals.tree_wind as an INT PERCENT
// (int passthrough - the underlying visual is a 0..1 float, the verb is not bool-ized), and
// GET_TREEWIND reads back both the visual and the two engine scalars it produces, so a
// "trees are not moving" result can name its own cause: wind=0 -> nobody set it; amp=0 ->
// the conversion did not run; dirlen=0 -> weather windDirection is zero and sample_wind
// returns nothing whatever the amplitude.
static bool AutoHarness_TreeWindCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	extern wiECS::Entity g_weatherEntityID;
	extern void GGTrees_SetSwayFromVisuals( float treeWind, wi::scene::WeatherComponent* weather );
	if (_stricmp(cmd, "SET_TREEWIND") == 0)
	{
		int pct = atoi(arg);
		// Diagnostics may exceed 100% deliberately: an absurd amplitude is the cheapest way to
		// tell "the sway path is dead" from "the sway path works and is tuned too subtle".
		if (pct < 0) pct = 0; if (pct > 10000) pct = 10000;
		t.visuals.tree_wind = (float)pct / 100.0f;
		t.gamevisuals.tree_wind = t.visuals.tree_wind;
		wiScene::WeatherComponent* w = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
		GGTrees_SetSwayFromVisuals(t.visuals.tree_wind, w);
		_snprintf(result, resultSize, "OK: SET_TREEWIND %d%% (tree_wind=%.2f, weather=%s)",
			pct, t.visuals.tree_wind, w ? "yes" : "NULL");
		result[resultSize - 1] = 0;
		return true;
	}
	// GGMAX 3.58b: SET_ENTITYWIND <0-1000> forces the per-material ENTITY tree-sway path onto every
	// material that is not already wind-enabled, as an int percent of DX11's "Object Wind".
	// Needed because NO stock media assigns the Tree Animate shader (0 of the shipped entitybank),
	// so without this there is no content that exercises the new path and a green build would prove
	// nothing. Materials already using wind are skipped, which is exactly the tree-pool vegetation,
	// so this also leaves the 3.57 path alone and the two can be told apart on screen.
	// GGMAX 3.59: SET_WEATHER <0-5> drives t.visuals.iEnvironmentWeather, the same int the three
	// Weather buttons write. INT PASSTHROUGH - 0 none, 1/2 rain, 3/4 snow, 5 none (the old
	// unlabelled test mode). The dispatcher is edge-triggered, so writing the value is all that is
	// needed; update_env_particles picks it up on the next frame.
	if (_stricmp(cmd, "SET_WEATHER") == 0)
	{
		int w = atoi(arg);
		if (w < 0) w = 0; if (w > 5) w = 5;
		t.visuals.iEnvironmentWeather = w;
		t.gamevisuals.iEnvironmentWeather = w;
		// The EDITOR only runs update_env_particles when "Display Weather in Editor" is ticked
		// (bEnableWeather, default FALSE at M-TerrainNew_part0.cpp:196). Tick it here so the verb
		// does what a user does; Test Game is ungated and needs no equivalent.
		extern bool bEnableWeather;
		if (w > 0) bEnableWeather = true;
		uint32_t GGWeather_GetRoot(void);
		_snprintf(result, resultSize, "OK: SET_WEATHER %d (intensity=%.0f, wpe_root_before=%u)",
			w, t.visuals.fWeatherIntensity, GGWeather_GetRoot());
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GET_WEATHER") == 0)
	{
		uint32_t GGWeather_GetRoot(void);
		extern bool bEnableWeather; extern int g_ggWeatherUpdateCalls; extern int environment_weather;
		extern char g_ggWeatherLastPath[]; extern uint32_t g_ggWeatherLastResult; extern int g_ggWeatherSetCalls;
		extern uint64_t gg_wpe_lastVer, gg_wpe_lastStride, gg_wpe_lastKnown; extern const char* gg_wpe_lastError;
		extern uint64_t gg_wpe_emStart, gg_wpe_afterRecs, gg_wpe_afterIds, gg_wpe_fileSize;
		_snprintf(result, resultSize,
			"OK: GET_WEATHER mode=%d latch=%d intensity=%.0f wpe_root=%u editorWeather=%d updateCalls=%d setCalls=%d lastResult=%u"
			" peVer=%llu peStride=%llu peKnown=%llu emStart=%llu afterRecs=%llu afterIds=%llu size=%llu peErr=\"%s\" lastPath=\"%s\"",
			t.visuals.iEnvironmentWeather, environment_weather, t.visuals.fWeatherIntensity,
			GGWeather_GetRoot(), bEnableWeather ? 1 : 0, g_ggWeatherUpdateCalls,
			g_ggWeatherSetCalls, g_ggWeatherLastResult,
			(unsigned long long)gg_wpe_lastVer, (unsigned long long)gg_wpe_lastStride,
			(unsigned long long)gg_wpe_lastKnown,
			(unsigned long long)gg_wpe_emStart, (unsigned long long)gg_wpe_afterRecs,
			(unsigned long long)gg_wpe_afterIds, (unsigned long long)gg_wpe_fileSize,
			gg_wpe_lastError, g_ggWeatherLastPath);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WEATHERINTENSITY") == 0)
	{
		int p = atoi(arg); if (p < 0) p = 0; if (p > 100) p = 100;
		t.visuals.fWeatherIntensity = (float)p;
		t.gamevisuals.fWeatherIntensity = (float)p;
		_snprintf(result, resultSize, "OK: SET_WEATHERINTENSITY %d", p);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_ENTITYWIND") == 0)
	{
		int pct = atoi(arg);
		if (pct < 0) pct = 0; if (pct > 1000) pct = 1000;
		const float v = (float)pct / 100.0f;
		wi::scene::Scene& sc = wi::scene::GetScene();
		int touched = 0, skipped = 0;
		for (size_t i = 0; i < sc.materials.GetCount(); ++i)
		{
			wi::scene::MaterialComponent& m = sc.materials[i];
			if (m.IsUsingWind() && m.userdata.x == 0) { skipped++; continue; }   // tree-pool vegetation
			if (v <= 0.0f) { m.userdata.x = 0; m.SetUseWind(false); }
			else { m.userdata.x = *reinterpret_cast<const uint32_t*>(&v); m.SetUseWind(true); }
			m.SetDirty();
			touched++;
		}
		_snprintf(result, resultSize, "OK: SET_ENTITYWIND %d%% (objwind=%.2f) touched=%d skipped_vegetation=%d",
			pct, v, touched, skipped);
		result[resultSize - 1] = 0;
		return true;
	}
	// GGMAX 3.61: SET_TREESWAYSPACE <world units> - the size of one wind-noise cell for the tree
	// path. One texel of the 32^3 volume spans SPACE/32 units, so this is really "how big is a
	// gust". Too small and a canopy straddles many cells and shimmers per-branch instead of
	// bending as one (that was the 1000 default). Live, no rebuild, so it can be judged by eye.
	if (_stricmp(cmd, "SET_TREESWAYSPACE") == 0)
	{
		extern float g_ggTreeSwaySpace;
		float v = (float)atof(arg);
		if (v < 32.0f) v = 32.0f;               // below this a texel is under a world unit
		if (v > 4000000.0f) v = 4000000.0f;
		g_ggTreeSwaySpace = v;
		// re-apply immediately so the change is visible without touching the slider
		extern wiECS::Entity g_weatherEntityID;
		wiScene::WeatherComponent* w = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
		extern void GGTrees_SetSwayFromVisuals( float treeWind, wi::scene::WeatherComponent* weather );
		GGTrees_SetSwayFromVisuals(t.visuals.tree_wind, w);
		_snprintf(result, resultSize, "OK: SET_TREESWAYSPACE %.0f (one noise cell = %.0f world units, ~%.1f m)",
			v, v / 32.0f, (v / 32.0f) / 39.37f);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GET_TREEWIND") == 0)
	{
		wiScene::WeatherComponent* w = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
		float dx = w ? w->windDirection.x : 0.0f, dy = w ? w->windDirection.y : 0.0f, dz = w ? w->windDirection.z : 0.0f;
		_snprintf(result, resultSize,
			"OK: GET_TREEWIND visuals=%.3f amp=%.3f spacercp=%.6f cell=%.0f dir=(%.2f,%.2f,%.2f) dirlen=%.3f speed=%.2f randomness=%.2f gustsize=%.2f",
			t.visuals.tree_wind,
			w ? w->gg_objectWindAmplitude : -1.0f,
			w ? w->gg_objectWindSpaceRcp : -1.0f,
			w ? (1.0f / (w->gg_objectWindSpaceRcp > 0 ? w->gg_objectWindSpaceRcp : 1.0f)) / 32.0f : -1.0f,
			dx, dy, dz, sqrtf(dx*dx + dy*dy + dz*dz), w ? w->windSpeed : -1.0f,
			w ? w->windRandomness : -1.0f, w ? w->windWaveSize : -1.0f);
		result[resultSize - 1] = 0;
		return true;
	}
	return false;
}

static bool AutoHarness_TracerCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "DUMP_TRACERS") != 0) return false;
	char trstat[512]; trstat[0] = 0;
	Tracers::DebugStatus(trstat, sizeof(trstat));
	_snprintf(result, resultSize, "OK: DUMP_TRACERS %s", trstat);
	result[resultSize - 1] = 0;
	return true;
}

// GGMAX 3.116: Grass Density, hoisted per the C1061 pattern (shares the existing helper arm).
//   SET_GRASSDENSITY <pct>          the LEVEL value (as the Graphics and Performance slider), 10..200
//   SET_GRASSDENSITY_MACHINE <pct>  the machine value (as setup.ini grassdensity)
//   SET_GRASSDENSITYMODE <0|1>      1 = DX11 density from the draw distance (default), 0 = pre-3.116 constant
//   GET_GRASSDENSITY                everything the density is made of, the latch, and what is built
extern void GGSetGrassDensity(int); extern void GGSetGrassDensityLevel(int); extern void GGSetGrassDensityDX11(int);
extern int GGGetGrassDensityLevel(); extern int GGGetGrassDensityMachine();
extern float GGGrass_DX11DensityPerM2(); extern float GGGrass_NearDensityPerM2(); extern float GGGrass_LodDistEffective();
extern void GGGrass_GetDensityState(float*, float*, unsigned int*, unsigned int*, unsigned int*, unsigned int*, unsigned int*);
static bool AutoHarness_GrassDensityCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	extern int gg_grass_density_pct; extern bool gg_grass_density_dx11; extern bool gg_lowvram; extern int gg_grass_far_mode;
	if (_stricmp(cmd, "SET_GRASSDENSITY") == 0 || _stricmp(cmd, "SET_GRASSDENSITY_MACHINE") == 0 || _stricmp(cmd, "SET_GRASSDENSITYMODE") == 0)
	{
		if (arg && arg[0])
		{
			const int v = atoi(arg);
			if (_stricmp(cmd, "SET_GRASSDENSITY") == 0) { GGSetGrassDensityLevel(v); t.gamevisuals.iGrassDensityPct = t.visuals.iGrassDensityPct = GGGetGrassDensityLevel(); }
			else if (_stricmp(cmd, "SET_GRASSDENSITY_MACHINE") == 0) GGSetGrassDensity(v);
			else GGSetGrassDensityDX11(v);
		}
		_snprintf(result, resultSize, "OK: %s - level %d%%, machine %d%%, effective %d%%, mode %s. Grass rebuilds once the inputs hold still (15 frames), nearest chunks first - GET_GRASSDENSITY shows the latch.",
			cmd, GGGetGrassDensityLevel(), GGGetGrassDensityMachine(), gg_grass_density_pct, gg_grass_density_dx11 ? "DX11 (draw distance)" : "constant 100k/chunk (pre-3.116)");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_GRASSFARMODE") == 0)
	{
		// GGMAX 3.117: 1 = DX11 far field (default), 0 = 1.49b strand LOD, 2 = DX11 rings with no dissolve (baseline)
		extern int gg_grass_far_mode;
		if (arg && arg[0]) { const int v = atoi(arg); if (v >= 0 && v <= 2) gg_grass_far_mode = v; }
		_snprintf(result, resultSize, "OK: SET_GRASSFARMODE %d (1 = DX11 far field: full size to the draw distance, then the dissolve; 0 = strand LOD with widened survivors; 2 = DX11 rings, no dissolve)", gg_grass_far_mode);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_TERRAINBRUSH") == 0)
	{
		// GGMAX 3.118: SET_TERRAINBRUSH <size> - the terrain/grass brush radius (world units), as the editor slider
		if (arg && arg[0]) { const float v = (float)atof(arg); if (v > 0.0f) GGTerrain::ggterrain_global_render_params2.brushSize = v; }
		_snprintf(result, resultSize, "OK: SET_TERRAINBRUSH %.0f", GGTerrain::ggterrain_global_render_params2.brushSize);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GRASS_POPULATE") == 0)
	{
		// GGMAX 3.118: GRASS_POPULATE <match 0|1> - the Grass tool's "Populate Vegetation Everywhere" button minus
		// its confirmation box: 1 = Match Terrain Color (every cell auto, resolved by the ground under it), 0 =
		// the selected grass. In memory only - the harness never marks the project modified.
		const int match = (arg && arg[0]) ? atoi(arg) : 1;
		const int savedMat = GGGrass::gggrass_global_params.paint_material;
		GGGrass::gggrass_global_params.paint_material = match ? 0 : 1;
		if (GGGrass::gggrass_global_params.paint_type == 0) GGGrass::gggrass_global_params.paint_type = 1;
		GGGrass::GGGrass_AddAll();
		GGGrass::gggrass_global_params.paint_material = savedMat;
		_snprintf(result, resultSize, "OK: GRASS_POPULATE %s (paint density %d%%)", match ? "Match Terrain Color (auto)" : "selected grass", GGGrass::gggrass_global_params.paint_density);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GRASS_PAINT_TEST") == 0)
	{
		// GGMAX 3.118: GRASS_PAINT_TEST <worldX> <worldZ> <frames> [match 0|1 (default 1)] [paint_type mask (default 1 =
		// Course Grass)] - a synthetic Grass tool spray stroke at the brush size (SET_TERRAINBRUSH). In memory only.
		float gx = 0, gz = 0; int gf = 0, gm = 1; unsigned long long gt = 1;
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%f %f %d %d %llu", &gx, &gz, &gf, &gm, &gt) : 0;
		if (!AutoHarness_TerrainEditAllowed()) _snprintf(result, resultSize, "ERROR: GRASS_PAINT_TEST needs the level editor with terrain initialised");
		else if (s_grassTestFrames > 0) _snprintf(result, resultSize, "ERROR: a grass test stroke is already running (%d frames left)", s_grassTestFrames);
		else if (s_terrainTestFrames > 0) _snprintf(result, resultSize, "ERROR: a terrain test is running (%d frames left)", s_terrainTestFrames);
		else if (g_iCalculatingChangeBounds != 0 || s_treeTestFrames > 0 || ::g_iActiveTreeEvent != 0) _snprintf(result, resultSize, "ERROR: a terrain/grass/tree stroke is in progress - release the mouse first");   // 3.123: a tree stroke holds the shared snapshot too
		else if (got >= 3 && gf > 0)
		{
			s_grassTestX = gx; s_grassTestZ = gz; s_grassTestMatch = gm ? 1 : 0; s_grassTestType = gt ? (uint64_t)gt : 1;
			s_grassTestFrame = 0; s_grassTestFrames = gf + 1;   // + the release frame
			_snprintf(result, resultSize, "OK: GRASS_PAINT_TEST at (%.0f, %.0f) for %d frames, Match Terrain Color %s, types 0x%llx (brushSize=%.0f)",
				gx, gz, gf, s_grassTestMatch ? "ON" : "OFF", (unsigned long long)s_grassTestType, GGTerrain::ggterrain_global_render_params2.brushSize);
		}
		else _snprintf(result, resultSize, "ERROR: GRASS_PAINT_TEST needs <worldX> <worldZ> <frames> [match] [typemask]");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GRASS_CLEAR_BARE") == 0)
	{
		// GGMAX 3.118: GRASS_CLEAR_BARE [1 = do it; default 0 = dry run] - GGGrass_RemoveCourseGrassFromBareGround.
		// Clears Course Grass (byte 2) a pre-3.118 DX12 build wrote onto sand/rock. In-memory only: the harness
		// never marks the project modified, so nothing is saved unless the level is saved by hand.
		const bool doIt = (arg && atoi(arg) == 1);
		const int n = GGGrass::GGGrass_RemoveCourseGrassFromBareGround(!doIt);
		if (n < 0) _snprintf(result, resultSize, "ERROR: GRASS_CLEAR_BARE - terrain or grass map not ready");
		else _snprintf(result, resultSize, "OK: GRASS_CLEAR_BARE %s %d Course Grass cell(s) on bare ground (of 16,777,216)", doIt ? "cleared" : "would clear (dry run; GRASS_CLEAR_BARE 1 to do it)", n);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GET_GRASSMAP_AUDIT") == 0)
	{
		// GGMAX 3.118: the grass map against the ground under each cell (GGGrass_AuditMap). [step] samples
		// every step-th cell each way (default 4 = 1M of the 16.7M cells).
		int step = 4;
		if (arg && arg[0]) step = atoi(arg);
		GGGrass::GGGrass_AuditMap(step, result, resultSize);
		return true;
	}
	if (_stricmp(cmd, "GET_GROUNDDIST_GRID") == 0)
	{
		// GGMAX 3.117: the TRUE camera->terrain distance through a grid of screen points (NDC cell centres over
		// the camera's FULL projection = the whole client window / SCREENSHOT, row-major from its top-left; in the
		// editor the 3D panel is only a CROP of it - map a screenshot pixel by the full image size, as
		// grass_falloff_measure.py does), by marching each ray against the terrain heights. A
		// flat-ground row->distance mapping read grass PAST the cull radius on a hilly level - this is the
		// instrument for anything that measures a distance-dependent effect off a screenshot.
		// usage: GET_GROUNDDIST_GRID [cols rows [kind]] (default 32 20, max 64 40). kind 0 = 3D distance (default),
		// 1 = HORIZONTAL (xz) distance, 2 = the hit point's ground Y. -1 = no hit within 40000.
		int cols = 32, rows = 20, kind = 0;
		if (arg && arg[0]) sscanf(arg, "%d %d %d", &cols, &rows, &kind);
		cols = std::max(2, std::min(cols, 64)); rows = std::max(2, std::min(rows, 40));
		const wi::scene::CameraComponent& cam = wi::scene::GetCamera();
		const XMMATRIX invVP = cam.GetInvViewProjection();
		const XMVECTOR eye = XMLoadFloat3(&cam.Eye);
		int len = _snprintf(result, resultSize, "OK: GROUNDDIST %d %d eye %.1f %.1f %.1f kind %d\n", cols, rows, cam.Eye.x, cam.Eye.y, cam.Eye.z, kind);
		for (int r = 0; r < rows && len > 0 && len < (int)resultSize - 16; r++)
		{
			for (int c = 0; c < cols; c++)
			{
				const float nx = -1.0f + 2.0f * (c + 0.5f) / cols;
				const float ny = 1.0f - 2.0f * (r + 0.5f) / rows;
				const XMVECTOR onRay = XMVector3TransformCoord(XMVectorSet(nx, ny, 0.5f, 1.0f), invVP);
				XMFLOAT3 d; XMStoreFloat3(&d, XMVector3Normalize(XMVectorSubtract(onRay, eye)));
				float hitT = -1.0f, prevT = 0.0f;
				for (float tt = 25.0f; tt <= 40000.0f; tt += 25.0f)
				{
					float gy = 0.0f;
					if (GGTerrain::GGTerrain_GetHeight(cam.Eye.x + d.x * tt, cam.Eye.z + d.z * tt, &gy, 1) && cam.Eye.y + d.y * tt <= gy)
					{
						float lo = prevT, hi = tt;   // bisect the crossing to well under a unit
						for (int it = 0; it < 14; it++)
						{
							const float mid = 0.5f * (lo + hi);
							float my = 0.0f;
							GGTerrain::GGTerrain_GetHeight(cam.Eye.x + d.x * mid, cam.Eye.z + d.z * mid, &my, 1);
							if (cam.Eye.y + d.y * mid <= my) hi = mid; else lo = mid;
						}
						hitT = hi;
						break;
					}
					prevT = tt;
				}
				float outV = hitT;
				if (hitT > 0.0f && kind == 1) outV = hitT * sqrtf(d.x * d.x + d.z * d.z);
				else if (hitT > 0.0f && kind == 2) outV = cam.Eye.y + d.y * hitT;
				len += _snprintf(result + len, resultSize - len, (c + 1 < cols) ? "%.0f " : "%.0f\n", outV);
				if (len <= 0 || len >= (int)resultSize - 16) break;
			}
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GET_GRASSDENSITY") != 0) return false;
	float nearS = 0, lowMul = 1; unsigned int t3 = 0, t2 = 0, t1 = 0, stale = 0, latches = 0;
	GGGrass_GetDensityState(&nearS, &lowMul, &t3, &t2, &t1, &stale, &latches);
	unsigned long long total = 0; unsigned int systems = 0, minS = 0xFFFFFFFFu, maxS = 0;
	{
		wi::scene::Scene& sc = wi::scene::GetScene();
		for (size_t i = 0; i < sc.hairs.GetCount(); i++)
		{
			const wi::HairParticleSystem& h = sc.hairs[i];
			if (h.grass_type == 0) continue;   // not GG grass
			systems++; total += h.strandCount;
			if (h.strandCount < minS) minS = h.strandCount;
			if (h.strandCount > maxS) maxS = h.strandCount;
		}
	}
	if (systems == 0) minS = 0;
	// GGMAX 3.117: the camera and the ground under it, so a screenshot row maps to a ground distance
	const XMFLOAT3 camEye = wi::scene::GetCamera().Eye;
	float groundY = 0.0f;
	GGTerrain::GGTerrain_GetHeight(camEye.x, camEye.z, &groundY, 1);
	const float nearPM2 = GGGrass_NearDensityPerM2(), dx11PM2 = GGGrass_DX11DensityPerM2();   // (near is a windef.h macro)
	_snprintf(result, resultSize,
		"GRASS DENSITY (GGMAX 3.116)\n"
		"  mode              : %s; far field %s\n"
		"  percent           : level %d%%, machine %d%%, effective %d%%\n"
		"  draw distance     : raw %d, effective %.0f (lowvram %d, density x%.2f)\n"
		"  latched           : %.0f strands per chunk at full density; per system: tier3 %u, tier2 %u, tier1 %u (latch moved %u times)\n"
		"  near density      : %.2f per m2 now | classic (DX11) at this draw distance %.2f | ratio %.3f\n"
		"  built             : %u systems, %llu strands (min %u, max %u per system); %u chunks still at an old density\n"
		"  camera            : eye %.1f %.1f %.1f, ground under it %.1f (height above ground %.1f)\n",
		gg_grass_density_dx11 ? "DX11 - density from the Grass Draw Distance (default)" : "constant 100,000 per chunk (pre-3.116)",
		(gg_grass_far_mode == 1) ? "DX11 (full size to the draw distance, dissolve to +2500)" : ((gg_grass_far_mode == 2) ? "DX11 rings, NO dissolve (baseline)" : "strand LOD (pre-3.117)"),
		GGGetGrassDensityLevel(), GGGetGrassDensityMachine(), gg_grass_density_pct,
		(int)GGGrass::gggrass_global_params.lod_dist, GGGrass_LodDistEffective(), gg_lowvram ? 1 : 0, lowMul,
		nearS, t3, t2, t1, latches,
		nearPM2, dx11PM2, (dx11PM2 > 0) ? nearPM2 / dx11PM2 : 0.0f,
		systems, total, minS, maxS, stale,
		camEye.x, camEye.y, camEye.z, groundY, camEye.y - groundY);
	result[resultSize - 1] = 0;
	return true;
}


// GGMAX 3.87: DUMP_POSTFX - the five level-authored post-processing flags in BOTH stores at once,
// plus the tree LOD distance the quality preset dials.
//
// Built because the defect it verifies is INVISIBLE to every instrument that existed: the editor
// and the test game each render their own panel correctly, and only a side-by-side of t.visuals
// against t.gamevisuals, taken in BOTH states, shows one being stamped over. Lee could only report
// it with two screenshots; this makes it one command.
//
// lod_dist rides along because it is the SAME preset call (GGTrees_SetPerformanceMode) and it is
// the one field that is still NOT restored on the way back to the editor - see notes 3.86.
static bool AutoHarness_PostFXCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "DUMP_POSTFX") != 0) return false;
	extern preferences pref;   // defined in imgui_gg_dx11_part0.cpp; not declared in any header
	_snprintf(result, resultSize,
		"OK: DUMP_POSTFX visuals[ssr=%d refl=%d fxaa=%d shafts=%d flare=%d bloom=%d vsync=%d] "
		"gamevisuals[ssr=%d refl=%d fxaa=%d shafts=%d flare=%d bloom=%d vsync=%d] "
		"testgamequality=%d treelod=%.0f state=%s",
		t.visuals.bSSREnabled ? 1 : 0, t.visuals.bReflectionsEnabled ? 1 : 0,
		t.visuals.bFXAAEnabled ? 1 : 0, t.visuals.bLightShafts ? 1 : 0,
		t.visuals.bLensFlare ? 1 : 0, t.visuals.bBloomEnabled ? 1 : 0,
		t.visuals.bLevelVSyncEnabled ? 1 : 0,
		t.gamevisuals.bSSREnabled ? 1 : 0, t.gamevisuals.bReflectionsEnabled ? 1 : 0,
		t.gamevisuals.bFXAAEnabled ? 1 : 0, t.gamevisuals.bLightShafts ? 1 : 0,
		t.gamevisuals.bLensFlare ? 1 : 0, t.gamevisuals.bBloomEnabled ? 1 : 0,
		t.gamevisuals.bLevelVSyncEnabled ? 1 : 0,
		pref.iTestGameGraphicsQuality,
		GGTrees::ggtrees_global_params.lod_dist,
		AutoHarness_GetAppState());
	result[resultSize - 1] = 0;
	return true;
}

// GGMAX 3.83: real-cursor + object-preview verbs.
//
// Everything hover-driven in the editor was untestable until now: CLICK is a table of named
// app actions and PRESS_KEY posts WM_KEYDOWN, but ImGui::IsItemHovered() reads io.MousePos,
// which the Win32 backend takes from the OS cursor. So a hover needs the OS cursor moved.
//
// Coordinates are CLIENT pixels of the MAX window - the same space PICK_AT uses and the same
// space a SCREENSHOT is measured in, so a coordinate read off a screenshot can be used here
// directly.
static bool AutoHarness_MouseCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	const bool bAt = (_stricmp(cmd, "MOUSE_AT") == 0);
	const bool bClick = (_stricmp(cmd, "MOUSE_CLICK") == 0);
	// MOUSE_DOWN / MOUSE_UP exist because a down+up pair inside one SendInput is drained by
	// ONE message pump, and imgui_impl_win32 sets io.MouseDown[0] true on WM_LBUTTONDOWN and
	// false on WM_LBUTTONUP - so the pair nets to false and ImGui never sees a click. Hover
	// works (it only needs the position), the click silently does not. Hold the button
	// across at least one frame: MOUSE_AT, MOUSE_DOWN, sleep, MOUSE_UP.
	const bool bDown = (_stricmp(cmd, "MOUSE_DOWN") == 0);
	const bool bUp = (_stricmp(cmd, "MOUSE_UP") == 0);
	// GGMAX 3.106: SET_PALETTEBITS [tree|grass <hex>] - set (or, with no arguments, just read) the 64-bit
	// paint palette bitfields. For SAVE_LEVEL -> reload round trips of the settings loader: the grass
	// field uses all 64 bits (stock + custom palette slots), which the old 32-bit load dropped.
	if (_stricmp(cmd, "SET_PALETTEBITS") == 0)
	{
		char pbWhich[16] = { 0 };
		char pbHex[40] = { 0 };
		const int pbGot = (arg && arg[0]) ? sscanf_s(arg, "%15s %39s", pbWhich, (unsigned)sizeof(pbWhich), pbHex, (unsigned)sizeof(pbHex)) : 0;
		if (pbGot >= 2)
		{
			const uint64_t pbVal = _strtoui64(pbHex, NULL, 16);
			if (_stricmp(pbWhich, "tree") == 0) GGTrees::ggtrees_global_params.paint_tree_bitfield = pbVal;
			else if (_stricmp(pbWhich, "grass") == 0) GGGrass::gggrass_global_params.paint_type = pbVal;
		}
		_snprintf(result, resultSize, "OK: SET_PALETTEBITS tree=0x%llx grass=0x%llx",
			(unsigned long long)GGTrees::ggtrees_global_params.paint_tree_bitfield,
			(unsigned long long)GGGrass::gggrass_global_params.paint_type);
		result[resultSize - 1] = 0;
		return true;
	}
	// GGMAX 3.106: SET_GRASSCUSTOMSLOT <slot 22..63> [relative dds path] - fill (or, with no path, empty)
	// a B.9 custom grass palette slot exactly as the palette's "Add New Grass" does, minus the file
	// dialog. Lets a test select a custom slot, save, reload and check the selection survives.
	if (_stricmp(cmd, "SET_GRASSCUSTOMSLOT") == 0)
	{
		extern bool bUpdateGrassMaterials;
		const int csSlot = (arg && arg[0]) ? atoi(arg) : -1;
		const char* csPath = (arg && arg[0]) ? strchr(arg, ' ') : nullptr;
		if (csPath) csPath++;
		else csPath = "";
		if (csSlot < GGGRASS_CUSTOM_SLOT_BASE || csSlot >= GGGRASS_MAX_PALETTE_SLOTS)
		{
			_snprintf(result, resultSize, "ERROR: SET_GRASSCUSTOMSLOT <slot %d..%d> [relative dds path]", GGGRASS_CUSTOM_SLOT_BASE, GGGRASS_MAX_PALETTE_SLOTS - 1);
		}
		else
		{
			t.visuals.sGrassTextures[csSlot] = (char*)csPath;
			t.gamevisuals.sGrassTextures[csSlot] = (char*)csPath;
			t.visuals.sGrassTexturesName[csSlot] = (char*)csPath;
			t.gamevisuals.sGrassTexturesName[csSlot] = (char*)csPath;
			GGGrass::GGGrass_SetCustomSlotFilename(csSlot, csPath);
			bUpdateGrassMaterials = true;
			_snprintf(result, resultSize, "OK: SET_GRASSCUSTOMSLOT %d = '%s'", csSlot, csPath);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	// GGMAX 3.104: TYPE_TEXT <text> - posts one WM_CHAR per character to the MAX window, which the
	// Win32 ImGui backend turns into io.AddInputCharacter, so the FOCUSED ImGui InputText receives it.
	// PRESS_KEY cannot type into a text field (WM_KEYDOWN / the terrain key system only).
	if (_stricmp(cmd, "TYPE_TEXT") == 0)
	{
		if (!g_pGlob || !g_pGlob->hWnd)
		{
			_snprintf(result, resultSize, "ERROR: no window handle");
			result[resultSize - 1] = 0;
			return true;
		}
		int nTyped = 0;
		for (const char* c = arg; c && *c; ++c, ++nTyped)
			PostMessageA(g_pGlob->hWnd, WM_CHAR, (WPARAM)(unsigned char)*c, 0);
		_snprintf(result, resultSize, "OK: TYPE_TEXT posted %d characters (lands in the focused ImGui field)", nTyped);
		result[resultSize - 1] = 0;
		return true;
	}
	if (bAt || bClick || bDown || bUp)
	{
		if (!g_pGlob || !g_pGlob->hWnd)
		{
			_snprintf(result, resultSize, "ERROR: no window handle");
			result[resultSize - 1] = 0;
			return true;
		}
		int mx = -1, my = -1;
		int got = (arg && arg[0]) ? sscanf_s(arg, "%d %d", &mx, &my) : 0;
		if (bAt && got != 2)
		{
			_snprintf(result, resultSize, "ERROR: MOUSE_AT requires <x> <y> in client pixels");
			result[resultSize - 1] = 0;
			return true;
		}
		// The Win32 ImGui backend only samples the cursor when MAX owns the foreground, so an
		// unfocused window would take the move and register no hover at all - silently.
		if (GetForegroundWindow() != g_pGlob->hWnd)
		{
			SetForegroundWindow(g_pGlob->hWnd);
		}
		POINT pt = { 0, 0 };
		if (got == 2)
		{
			pt.x = mx; pt.y = my;
			ClientToScreen(g_pGlob->hWnd, &pt);
			SetCursorPos(pt.x, pt.y);
		}
		else
		{
			GetCursorPos(&pt);
		}
		if (bClick || bDown || bUp)
		{
			INPUT in[2] = {};
			UINT n = 0;
			if (bClick || bDown) { in[n].type = INPUT_MOUSE; in[n].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; n++; }
			if (bClick || bUp)   { in[n].type = INPUT_MOUSE; in[n].mi.dwFlags = MOUSEEVENTF_LEFTUP;   n++; }
			SendInput(n, in, sizeof(INPUT));
		}
		_snprintf(result, resultSize, "OK: %s client=(%d,%d) screen=(%ld,%ld)",
			bClick ? "MOUSE_CLICK" : bDown ? "MOUSE_DOWN" : bUp ? "MOUSE_UP" : "MOUSE_AT",
		mx, my, pt.x, pt.y);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "PERTURB_SCENE") == 0)
	{
		// GGMAX 3.121: PERTURB_SCENE <1|0> - push every LEVEL input the Object/Particle Library live preview
		// must ignore to an extreme (1), or put the saved values back exactly (0). Live-only, nothing saved:
		// ambient, fog density/start/opacity/colour (horizon), zenith, wind (direction, speed, gust size,
		// randomness), the sun light's colour and intensity, Object Detail Distance (8000) and Particle Density
		// (0%). Tool preview_isolation_probe.sh: the preview tile must not move between 1 and 0.
		// Here, not in the else-if chain below: one more link there is C1061 (blocks nested too deeply).
		static bool s_saved = false;
		static XMFLOAT3 s_amb, s_hor, s_zen, s_wind, s_sunCol;
		static float s_fogD = 0, s_fogS = 0, s_fogO = 0, s_sunI = 0, s_wSpd = 0, s_wWave = 0, s_wRand = 0, s_cull = 0;
		static int s_pct = 100;
		extern float gg_object_cull_dist;
		extern int gg_particle_pct;
		extern wi::ecs::Entity g_weatherEntityID;
		extern wi::ecs::Entity g_entitySunLight;
		wi::scene::WeatherComponent* weather = wi::scene::GetScene().weathers.GetComponent(g_weatherEntityID);
		wi::scene::LightComponent* sun = wi::scene::GetScene().lights.GetComponent(g_entitySunLight);
		const int on = (arg && arg[0]) ? atoi(arg) : 0;
		if (!weather)
		{
			_snprintf(result, resultSize, "ERROR: PERTURB_SCENE no weather component");
		}
		else if (on)
		{
			if (!s_saved)
			{
				s_amb = weather->ambient; s_hor = weather->horizon; s_zen = weather->zenith; s_wind = weather->windDirection;
				s_fogD = weather->fogDensity; s_fogS = weather->fogStart; s_fogO = weather->gg_fog_opacity;
				if (sun) { s_sunCol = sun->color; s_sunI = sun->intensity; }
				s_wSpd = weather->windSpeed; s_wWave = weather->windWaveSize; s_wRand = weather->windRandomness;
				s_cull = gg_object_cull_dist; s_pct = gg_particle_pct;
				s_saved = true;
			}
			weather->ambient = XMFLOAT3(1.5f, 0.1f, 0.1f);		// strong red
			weather->horizon = XMFLOAT3(1.0f, 0.0f, 1.0f);		// magenta fog colour
			weather->zenith = XMFLOAT3(0.0f, 1.0f, 0.0f);
			weather->fogDensity = 0.002f;
			weather->fogStart = 0.0f;
			weather->gg_fog_opacity = 1.0f;
			weather->windDirection = XMFLOAT3(60.0f, 0.0f, 60.0f);
			weather->windSpeed = 8.0f; weather->windWaveSize = 4.0f; weather->windRandomness = 20.0f;
			gg_object_cull_dist = 8000.0f;	// below the backdrop's 21200 - radius: culls it outright unless exempt
			gg_particle_pct = 0;
			if (sun) { sun->color = XMFLOAT3(0.0f, 1.0f, 0.0f); sun->intensity = (s_sunI > 0.0f ? s_sunI : 1.0f) * 4.0f; }
			_snprintf(result, resultSize, "OK: PERTURB_SCENE 1 (ambient red, fog magenta 0.002 from 0, wind 60 speed 8, sun green x4, object detail 8000, particles 0%%%s)",
				sun ? "" : " - NO SUN LIGHT");
		}
		else
		{
			if (s_saved)
			{
				weather->ambient = s_amb; weather->horizon = s_hor; weather->zenith = s_zen; weather->windDirection = s_wind;
				weather->fogDensity = s_fogD; weather->fogStart = s_fogS; weather->gg_fog_opacity = s_fogO;
				if (sun) { sun->color = s_sunCol; sun->intensity = s_sunI; }
				weather->windSpeed = s_wSpd; weather->windWaveSize = s_wWave; weather->windRandomness = s_wRand;
				gg_object_cull_dist = s_cull; gg_particle_pct = s_pct;
				s_saved = false;
				_snprintf(result, resultSize, "OK: PERTURB_SCENE 0 (restored)");
			}
			else _snprintf(result, resultSize, "OK: PERTURB_SCENE 0 (nothing to restore)");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_OBJPREVIEW") == 0)
	{
		// Stage the live preview so a device hang names WHICH of the three steps did it:
		// 0 off, 1 engine render_to_texture only, 2 + tonemap, 3 + ImGui bind (default).
		// SET_OBJPREVIEW <mode> [exposure]  - the exposure is optional and does NOT restart the
		// pass, so it can be swept against one live hover instead of one rebuild per guess.
		extern int gg_objpreview_mode;
		extern float gg_objpreview_exposure;
		extern int gg_objpreview_backdrop;
		int m = -1; float ex = -1.0f; int bd = -1;
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%d %f %d", &m, &ex, &bd) : 0;
		if (got < 1 || m < 0 || m > 3)
		{
			_snprintf(result, resultSize, "ERROR: SET_OBJPREVIEW <0..3> [exposure] (now mode %d exposure %.3f)",
				gg_objpreview_mode, gg_objpreview_exposure);
		}
		else
		{
			extern void GGObjectPreview_Stop(void);
			const bool bModeChanged = (gg_objpreview_mode != m);
			gg_objpreview_mode = m;
			if (got >= 2) gg_objpreview_exposure = ex;
			if (got >= 3 && bd >= 0 && bd <= 2) { gg_objpreview_backdrop = bd; GGObjectPreview_Stop(); }
			if (bModeChanged) GGObjectPreview_Stop();
			_snprintf(result, resultSize, "OK: SET_OBJPREVIEW mode %d exposure %.3f backdrop %d",
				m, gg_objpreview_exposure, gg_objpreview_backdrop);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_OBJPREVIEW_LIGHT") == 0)
	{
		// SET_OBJPREVIEW_LIGHT <k> [absIntensity] [fovDeg] - k is the distance-scaled light
		// preview is up. Does not restart the pass, so intensity can be swept against ONE live
		// hover. 0 = leave whatever WickedCall_EnableThumbLight created (8 cd / 2900).
		// 3.97: [studio 0|1] [ambient] [envscale] appended - the preview's own environment. The
		// light model is now a FIXED energy: pass k=0 and the energy as absIntensity.
		extern float gg_objpreview_lightk;
		extern float gg_objpreview_lightint;
		extern float gg_objpreview_fovdeg;
		extern int gg_objpreview_studio;
		extern float gg_objpreview_ambient;
		extern float gg_objpreview_envscale;
		extern float gg_objpreview_modulate;
		extern int gg_objpreview_freeze;
		extern float gg_objpreview_particlelight;
		// 3.97b: [modulate] [freeze 0|1] appended. 3.121: [particle light] appended (lit WPE particles' total white light).
		float lk = -1.0f, li = -1.0f, fv = -1.0f, am = -1.0f, es = -1.0f, md = -1.0f, pl = -1.0f; int st2 = -1, fz = -1;
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%f %f %f %d %f %f %f %d %f", &lk, &li, &fv, &st2, &am, &es, &md, &fz, &pl) : 0;
		if (got < 1)
		{
			_snprintf(result, resultSize,
				"ERROR: SET_OBJPREVIEW_LIGHT <k> [absIntensity] [fovDeg] (now k=%.2f abs=%.1f fov=%.1f)",
				gg_objpreview_lightk, gg_objpreview_lightint, gg_objpreview_fovdeg);
		}
		else
		{
			gg_objpreview_lightk = lk;
			if (got >= 2) gg_objpreview_lightint = li;
			if (got >= 3 && fv > 0.0f) gg_objpreview_fovdeg = fv;
			if (got >= 4 && st2 >= 0) gg_objpreview_studio = st2;
			if (got >= 5 && am >= 0.0f) gg_objpreview_ambient = am;
			if (got >= 6 && es >= 0.0f) gg_objpreview_envscale = es;
			if (got >= 7 && md >= 0.0f) gg_objpreview_modulate = md;
			if (got >= 8 && fz >= 0) gg_objpreview_freeze = fz;
			if (got >= 9 && pl >= 0.0f) gg_objpreview_particlelight = pl;
			_snprintf(result, resultSize,
				"OK: SET_OBJPREVIEW_LIGHT k=%.5f abs=%.2f fov=%.1f studio=%d ambient=%.3f envscale=%.3f modulate=%.3f freeze=%d plight=%.2f",
				gg_objpreview_lightk, gg_objpreview_lightint, gg_objpreview_fovdeg, gg_objpreview_studio, gg_objpreview_ambient, gg_objpreview_envscale, gg_objpreview_modulate, gg_objpreview_freeze, gg_objpreview_particlelight);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_OBJPREVIEW_MAT") == 0)
	{
		// SET_OBJPREVIEW_MAT <metalness> [roughness] [bounce] - the first two override the PREVIEW
		// OBJECT's subsets only (a temporary bank object, never a placed instance); -1 leaves them
		// as loaded. bounce is the third light's strength as a fraction of the key, 0 disables it.
		// (a temporary bank object, never a placed instance). -1 leaves the material as loaded.
		extern float gg_objpreview_metal, gg_objpreview_rough;
		extern float gg_objpreview_bounce;
		extern int gg_objpreview_unlit;
		float mv = -2.0f, rv = -2.0f, bv = -2.0f; int uv2 = -1;
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%f %f %f %d", &mv, &rv, &bv, &uv2) : 0;
		if (got < 1)
		{
			_snprintf(result, resultSize, "ERROR: SET_OBJPREVIEW_MAT <metalness> [roughness] (now %.2f / %.2f)",
				gg_objpreview_metal, gg_objpreview_rough);
		}
		else
		{
			gg_objpreview_metal = mv;
			if (got >= 2) gg_objpreview_rough = rv;
			if (got >= 3) { gg_objpreview_bounce = bv; extern float gg_objpreview_stage[4][3]; gg_objpreview_stage[3][2] = bv; }	// 3.98: IS the stage bounce
			if (got >= 4) gg_objpreview_unlit = uv2;
			_snprintf(result, resultSize, "OK: SET_OBJPREVIEW_MAT metalness %.2f roughness %.2f bounce %.2f unlit %d",
				gg_objpreview_metal, gg_objpreview_rough, gg_objpreview_bounce, gg_objpreview_unlit);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_OBJPREVIEW_STAGE") == 0)
	{
		// 3.98 SET_OBJPREVIEW_STAGE <0..3> <azimuthDeg> <elevationDeg> <strength>  (0 key 1 fill 2 rim 3 bounce)
		//      SET_OBJPREVIEW_STAGE 4 <softbox>   - studio-sky softbox strength (0 = plain gradient)
		extern float gg_objpreview_stage[4][3];
		extern float gg_objpreview_softbox;
		int li = -1; float a0 = 0, a1 = 0, a2 = 0;
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%d %f %f %f", &li, &a0, &a1, &a2) : 0;
		if (got >= 4 && li >= 0 && li <= 3) { gg_objpreview_stage[li][0] = a0; gg_objpreview_stage[li][1] = a1; gg_objpreview_stage[li][2] = a2; }
		else if (got >= 2 && li == 4) { gg_objpreview_softbox = a0; }
		else if (got >= 2 && li == 5) { extern int gg_objpreview_meshfix; gg_objpreview_meshfix = (int)a0; }	// 5 <1|2|3> one-shot mesh repair
		_snprintf(result, resultSize, "%s: SET_OBJPREVIEW_STAGE key(%.0f,%.0f,%.2f) fill(%.0f,%.0f,%.2f) rim(%.0f,%.0f,%.2f) bounce(%.0f,%.0f,%.2f) softbox %.2f",
			(got >= 2) ? "OK" : "USAGE <0..3> az el strength | 4 softbox",
			gg_objpreview_stage[0][0], gg_objpreview_stage[0][1], gg_objpreview_stage[0][2],
			gg_objpreview_stage[1][0], gg_objpreview_stage[1][1], gg_objpreview_stage[1][2],
			gg_objpreview_stage[2][0], gg_objpreview_stage[2][1], gg_objpreview_stage[2][2],
			gg_objpreview_stage[3][0], gg_objpreview_stage[3][1], gg_objpreview_stage[3][2], gg_objpreview_softbox);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_OBJPREVIEW_MATS") == 0)
	{
		extern void GGObjectPreview_DumpMaterials(char*, int);
		GGObjectPreview_DumpMaterials(result, (int)resultSize);
		return true;
	}
	if (_stricmp(cmd, "DUMP_OBJPREVIEW") == 0)
	{
		// Names every link in the live-preview chain in one line, so a blank thumbnail can be
		// told apart from a preview that was never requested, never rendered, or never bound.
		extern int BackBufferObjectID;
		extern int BackBufferImageID;
		extern bool bLoopBackBuffer;
		extern bool bRotateBackBuffer;
		extern float BackBufferRotateY;
		extern bool GGObjectPreview_IsActive(void);
		extern void GGObjectPreview_DebugStatus(char* buf, int bufsize);
		char st[2048]; st[0] = 0;   // 3.97: 512 truncated the studio fields; 3.110: 1024 the particle fields
		GGObjectPreview_DebugStatus(st, sizeof(st));
		extern bool ImGui_DX12_IsInitialized();
		_snprintf(result, resultSize,
			"OK: DUMP_OBJPREVIEW dx12=%d obj=%d imageid=%d loop=%d rotate=%d rotY=%.1f active=%d | %s",
			ImGui_DX12_IsInitialized() ? 1 : 0,
			BackBufferObjectID, BackBufferImageID, bLoopBackBuffer ? 1 : 0, bRotateBackBuffer ? 1 : 0,
			BackBufferRotateY, GGObjectPreview_IsActive() ? 1 : 0, st);
		result[resultSize - 1] = 0;
		return true;
	}
	return false;
}
static bool AutoHarness_ShadowBudgetCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "SET_SHADOW_MAX") == 0)
	{
		// Drive BOTH local shadow caps (spot+point) for testing the Phase 1/2 budget+cache without
		// the UI (GGMAX 2.07 split the engine budget per type; this stays the global kill switch).
		// The game's shadow-props recompute only runs on a visuals-apply, so force the engine directly.
		int n = atoi(arg);
		if (n < 0) n = 0;
		t.visuals.iShadowSpotMax = n;
		t.gamevisuals.iShadowSpotMax = n;
		t.visuals.iShadowPointMax = n;
		t.gamevisuals.iShadowPointMax = n;
		wi::renderer::SetLocalShadowBudget(n, n); // immediate: GRANTED = min(visible casters, n) per type
		_snprintf(result, resultSize, "OK: SET_SHADOW_MAX spot+point budgets forced to %d", n);
	}
	else if (_stricmp(cmd, "SET_SHADOW_MAX_SPOT") == 0)
	{
		// GGMAX 2.07: force only the SPOT/RECT shadow cap (point cap untouched).
		int n = atoi(arg);
		if (n < 0) n = 0;
		t.visuals.iShadowSpotMax = n;
		t.gamevisuals.iShadowSpotMax = n;
		wi::renderer::SetLocalShadowBudget(n, t.visuals.iShadowPointMax);
		_snprintf(result, resultSize, "OK: SET_SHADOW_MAX_SPOT budget forced to %d (point stays %d)", n, t.visuals.iShadowPointMax);
	}
	else if (_stricmp(cmd, "SET_SHADOW_MAX_POINT") == 0)
	{
		// GGMAX 2.07: force only the POINT shadow cap (spot cap untouched).
		int n = atoi(arg);
		if (n < 0) n = 0;
		t.visuals.iShadowPointMax = n;
		t.gamevisuals.iShadowPointMax = n;
		wi::renderer::SetLocalShadowBudget(t.visuals.iShadowSpotMax, n);
		_snprintf(result, resultSize, "OK: SET_SHADOW_MAX_POINT budget forced to %d (spot stays %d)", n, t.visuals.iShadowSpotMax);
	}
	else if (_stricmp(cmd, "LIST_LIGHTS") == 0)
	{
		// Ground-truth dump of every live Wicked LightComponent (cone-edge artifact hunt):
		// entity, type, pos, dir, range, outer/inner cone (deg), intensity, cast flag.
		wi::scene::Scene* pScene = master.masterrenderer.scene;
		int off = _snprintf(result, resultSize, "OK: %d lights\n", (int)pScene->lights.GetCount());
		for (size_t i = 0; i < pScene->lights.GetCount() && off > 0 && off < (int)resultSize - 160; ++i)
		{
			const wi::scene::LightComponent& L = pScene->lights[i];
			off += _snprintf(result + off, resultSize - off,
				"  ent=%u t=%d pos=(%.0f,%.0f,%.0f) dir=(%.2f,%.2f,%.2f) rng=%.0f outer=%.1f inner=%.1f int=%.2f cast=%d\n",
				(uint32_t)pScene->lights.GetEntity(i), (int)L.GetType(),
				L.position.x, L.position.y, L.position.z,
				L.direction.x, L.direction.y, L.direction.z,
				L.GetRange(), L.outerConeAngle * 57.2958f, L.innerConeAngle * 57.2958f,
				L.intensity, L.IsCastingShadow() ? 1 : 0);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	else if (_stricmp(cmd, "SET_AO") == 0)
	{
		// Live AO toggle (same as the VK_2 perf hotkey): 0 = AO_DISABLED, 1 = AO_MSAO.
		// Added for the cone-edge tile-artifact hunt: MSAO's deinterleaved blocks were the
		// last tile-quantized suspect after culling/buckets were exonerated empirically.
		bool on = (arg[0] != '0');
		master.masterrenderer.setAO(on ? wi::RenderPath3D::AO_MSAO : wi::RenderPath3D::AO_DISABLED);
		_snprintf(result, resultSize, "OK: SET_AO %s", on ? "MSAO" : "DISABLED");
	}
	else if (_stricmp(cmd, "TILE_DEBUG") == 0)
	{
		// Per-tile light-culling heatmap overlay (engine debug shader variant). Re-added for
		// the 2026-08-06 cone-edge tile-dropout hunt: shows each 16px tile's culled entity
		// count as color — artifact tiles with a different count than neighbors = culling.
		bool on = (arg[0] != '0');
		wi::renderer::SetDebugLightCulling(on);
		_snprintf(result, resultSize, "OK: TILE_DEBUG %s", on ? "ON (per-tile entity heatmap)" : "OFF");
	}
	else if (_stricmp(cmd, "SET_SHADOW_RES") == 0)
	{
		// 2026-08-06 sun-off coupling hunt: drive one shadow RESOLUTION exactly as the UI
		// dropdowns do (visuals field + the engine setter), no visuals-apply needed.
		char which[32] = { 0 };
		int n = 0;
		if (sscanf(arg, "%31s %d", which, &n) == 2 && n >= 0)
		{
			if (n > 2048) n = 2048;
			if (_stricmp(which, "sun") == 0)
			{
				t.visuals.iShadowSpotCascadeResolution = n;
				t.gamevisuals.iShadowSpotCascadeResolution = n;
				wi::renderer::SetShadowProps2D(n);
				_snprintf(result, resultSize, "OK: SET_SHADOW_RES sun(cascade) = %d", n);
			}
			else if (_stricmp(which, "spot") == 0)
			{
				t.visuals.iShadowSpotResolution = n;
				t.gamevisuals.iShadowSpotResolution = n;
				wi::renderer::SetShadowPropsSpot(n);
				_snprintf(result, resultSize, "OK: SET_SHADOW_RES spot = %d", n);
			}
			else if (_stricmp(which, "point") == 0)
			{
				t.visuals.iShadowPointResolution = n;
				t.gamevisuals.iShadowPointResolution = n;
				wi::renderer::SetShadowPropsCube(n);
				_snprintf(result, resultSize, "OK: SET_SHADOW_RES point = %d", n);
			}
			else
			{
				_snprintf(result, resultSize, "ERROR: SET_SHADOW_RES needs sun|spot|point <res>");
			}
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: SET_SHADOW_RES needs sun|spot|point <res>");
		}
	}
	else
	{
		return false;
	}
	result[resultSize - 1] = 0;
	return true;
}

// GGMAX 2.26: DUMP_MESHES rides this helper rather than taking its own link in the main
// dispatch chain. Adding one more `else if` there re-hit MSVC C1061 immediately (every chain
// link nests a block deeper, and that chain is already at the limit — same trap as the
// shadow-budget and outline helpers). Delegating from inside an EXISTING link costs no nesting.
static bool AutoHarness_CensusCommands(const char* cmd, const char* arg, char* result, size_t resultSize);

static bool AutoHarness_StandaloneCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (AutoHarness_CensusCommands(cmd, arg, result, resultSize)) return true;

	if (_stricmp(cmd, "TITLE_CLICK") == 0)
	{
		// Press a storyboard screen widget by ACTION name (start/exit/continue/back/
		// resume/leave) exactly as a mouse click would — the flag is consumed inside
		// screen_editor's widget loop, so sounds/actions run the user's code path.
		extern int g_iAutoTriggerScreenAction;
		if (!arg || !arg[0])
		{
			_snprintf(result, resultSize, "ERROR: TITLE_CLICK requires an action name (start/exit/continue/back/resume/leave)");
		}
		else
		{
			int iAction = GGAuto_MapScreenActionName(arg);
			if (iAction == 0)
			{
				_snprintf(result, resultSize, "ERROR: unknown TITLE_CLICK action '%s'", arg);
			}
			else
			{
				g_iAutoTriggerScreenAction = iAction;
				_snprintf(result, resultSize, "OK: queued screen action '%s' (%d) for next widget pass", arg, iAction);
			}
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_LIGHT_LIT") == 0)
	{
		// SET_LIGHT_LIT <index> <0|1> — toggle a GG infinilight (flicker hunt: isolate the
		// light that owns a lit pattern). Writes islit AND zero/restores the Wicked
		// component intensity so it takes effect regardless of the sync cadence.
		int li = 0, lit = 1;
		if (sscanf_s(arg, "%d %d", &li, &lit) == 2 && li >= 1 && li <= g.infinilightmax)
		{
			infinilighttype* pL = &t.infinilight[li];
			pL->islit = lit;
			wi::scene::Scene* pScene = master.masterrenderer.scene;
			wi::scene::LightComponent* lc = (pScene && pL->wickedlightindex > 0) ? pScene->lights.GetComponent((wi::ecs::Entity)pL->wickedlightindex) : nullptr;
			static float s_savedIntensity[1024] = {};
			if (lc && li < 1024)
			{
				if (!lit)
				{
					if (s_savedIntensity[li] == 0.0f) s_savedIntensity[li] = lc->intensity;
					lc->intensity = 0.0f;
				}
				else if (s_savedIntensity[li] > 0.0f)
				{
					lc->intensity = s_savedIntensity[li];
					s_savedIntensity[li] = 0.0f;
				}
			}
			_snprintf(result, resultSize, "OK: light[%d] islit=%d wicked=%s", li, lit, lc ? "updated" : "none");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: SET_LIGHT_LIT <index 1..%d> <0|1>", g.infinilightmax);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_LIGHT_RADIUS") == 0)
	{
		// SET_LIGHT_RADIUS <index> <fullConeDeg> — write the light-list spotlightradius exactly
		// as the Spotlight Radius slider does; lighting_loop pushes it to Wicked next frame.
		// Built to repro/verify 2.07c (shadow cache went stale on cone edits: pos/range unchanged).
		int li = 0; float deg = 0.0f;
		if (sscanf_s(arg, "%d %f", &li, &deg) == 2 && li >= 1 && li <= g.infinilightmax)
		{
			t.infinilight[li].spotlightradius = deg;
			_snprintf(result, resultSize, "OK: light[%d] spotlightradius=%.1f (lighting_loop re-pushes the cone)", li, deg);
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: SET_LIGHT_RADIUS <index 1..%d> <degrees>", g.infinilightmax);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GET_CAMERA") == 0)
	{
		_snprintf(result, resultSize, "OK: pos=(%.2f,%.2f,%.2f) ang=(%.2f,%.2f) freeflight=%d",
			t.editorfreeflight.c.x_f, t.editorfreeflight.c.y_f, t.editorfreeflight.c.z_f,
			t.editorfreeflight.c.angx_f, t.editorfreeflight.c.angy_f, t.editorfreeflight.mode);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_SHADOWRECTS") == 0)
	{
		// Spot-shadow flicker hunt: the packer's final scale + every packed light rect.
		// A light's rect size changing with camera POSE = the flicker mechanism.
		// GGMAX 3.68: ws/hs are now the PACKED rect (what the packer's containing size is computed
		// from); sl is the slice multiplier, so the per-cascade/per-face size is w/slices.
		uint32_t ents[64]; int ws[64], hs[64], tys[64], sl[64]; float scale = 0.0f;
		uint32_t n = wi::renderer::GG_GetShadowRects(ents, ws, hs, tys, 64, &scale, sl);
		int packw = 0, packh = 0;
		for (uint32_t i = 0; i < n; ++i) { if (ws[i] > packw) packw = ws[i]; if (hs[i] > packh) packh = hs[i]; }
		int off = _snprintf(result, resultSize, "OK: pack_scale=%.4f rects=%u maxrect=%dx%d\n", scale, n, packw, packh);
		for (uint32_t i = 0; i < n && off > 0 && off < (int)resultSize - 64; ++i)
		{
			off += _snprintf(result + off, resultSize - off, "  ent=%u type=%d packed=%dx%d slices=%d per=%dx%d\n",
				ents[i], tys[i], ws[i], hs[i], sl[i], sl[i] > 0 ? ws[i] / sl[i] : ws[i], hs[i]);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "KILL_EMITTERS") == 0)
	{
		// DEVICE_HUNG discriminator: remove every EmittedParticleSystem so no particle
		// simulate dispatches run. If the standalone-play GPU hang stops with emitters
		// gone, the particle path (simulate CS reads texture_depth_history) is convicted.
		auto& scene = wi::scene::GetScene();
		size_t n = scene.emitters.GetCount();
		while (scene.emitters.GetCount() > 0)
		{
			scene.emitters.Remove(scene.emitters.GetEntity(0));
		}
		_snprintf(result, resultSize, "OK: removed %u emitters", (unsigned)n);
		result[resultSize - 1] = 0;
		return true;
	}
	return false;
}

// GGMAX 2.08 (hair rendering parity): transparency diagnostics + the live knob for the
// DX11 double-sided-transparent no-depth-write rule. Hoisted for the same C1061 reason as
// the helpers above. Returns true if cmd was handled.
// GGMAX 2026-08-07 (tasks #120/#121): gpup forensics entry points — implemented inside
// GPUParticles_part0.cpp because the emitter/settings structs are private to that TU.
// GGMAX 2.15 perf knobs. Declared here rather than pulling wiXInput.h / adding a wiScene.h
// export into this TU; both definitions live in the engine and these match them exactly.
namespace wi::input::xinput { extern uint32_t gg_xinput_rescan_frames; }
namespace wi::scene         { extern int      gg_scene_serial_profile; }
namespace wi::scene         { extern bool     gg_instinit_parallel; }
namespace wi::scene         { extern float    gg_envprobe_brightness; } // GGMAX 1.55 knob (wiScene.cpp:46), SET_ENVPROBE_BRIGHTNESS
namespace wi::scene         { extern int      gg_probeparallax; }       // GGMAX 2.89 knob (wiScene.cpp:47), SET_PROBEPARALLAX
namespace wi::scene         { extern int      gg_probeonlyglobal; }     // GGMAX 2.89 knob (wiScene.cpp:51), SET_PROBEONLYGLOBAL
namespace wi::scene         { extern float    gg_lod_step; }            // the LOD distance step, DUMP_LODSTATS

namespace GPUParticles {
	int gpup_debug_dump(char* summary, int summarySize);
	void gpup_debug_show(int on);
	void gpup_debug_force_arm(int mode);
	void gpup_debug_set_sn(float v);
	void gpup_debug_set_clocks(float sn, float rotsn, float agk_seconds);
	void gpup_debug_regen_textures();
	void gpup_debug_canary(int mode);
	int gpup_debug_track(int enr);
	void gpup_debug_solo(int enr);
	void gpup_debug_drawlog(int on);
	void gpup_debug_rebind(int on);
	int gpup_debug_ages(int enr);
}

// GGMAX 2.73 (task #155, "circle image on each cube side"): the base/global environment
// reflection every shiny surface falls back to is probes[0] = GGTerrain's globalEnvProbe,
// a 128px capture taken at world (0, terrain-height, 0) — the MAP CORNER. DUMP_ENVPROBE
// saves every probe cube + the raw sky cube as .dds (decode offline with texconv) and
// censuses what geometry sits inside the capture radius of probes[0], so the thing being
// photographed into the base env map can be NAMED instead of guessed at.
extern bool g_bLightProbeScaleChanged; // M-GridEdit_part0.cpp — the editor's full probe-refresh path
int gg_debugprobes_force = 0; // GGMAX 2.74b: SET_DEBUGPROBES sticky override, consumed by lighting_loop (G-Lighting.cpp)
static bool AutoHarness_EnvProbeCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "DUMP_ENVPROBE") == 0)
	{
		// DUMP_ENVPROBE [radius=3000] — dump all env probe cubes + sky cube + census of
		// objects near probes[0] (the global/base env probe).
		float epRadius = 3000.0f;
		if (arg && arg[0]) sscanf_s(arg, "%f", &epRadius);
		wi::scene::Scene& epScene = wi::scene::GetScene();
		FILE* epF = GGDiagFopen("envprobe_dump.txt", "w");
		if (epF == nullptr)
		{
			_snprintf(result, resultSize, "ERROR: DUMP_ENVPROBE could not open envprobe_dump.txt");
			result[resultSize - 1] = 0;
			return true;
		}
		int epSaved = 0;
		float epGX = 0, epGY = 0, epGZ = 0;
		fprintf(epF, "probes=%d (probes[0] is the global/base env reflection when no local probe covers a pixel)\n",
			(int)epScene.probes.GetCount());
		for (size_t epi = 0; epi < epScene.probes.GetCount(); ++epi)
		{
			wi::ecs::Entity epe = epScene.probes.GetEntity(epi);
			const wi::scene::EnvironmentProbeComponent& epp = epScene.probes[epi];
			const wi::scene::NameComponent* epn = epScene.names.GetComponent(epe);
			const wi::scene::TransformComponent* ept = epScene.transforms.GetComponent(epe);
			float px = ept ? ept->world._41 : 0, py = ept ? ept->world._42 : 0, pz = ept ? ept->world._43 : 0;
			if (epi == 0) { epGX = px; epGY = py; epGZ = pz; }
			fprintf(epF, "probe[%d] entity=%u name=\"%s\" pos=(%.0f,%.0f,%.0f) range=%.0f res=%u mips=%u dirty=%d render_dirty=%d realtime=%d texfile=\"%s\" texvalid=%d\n",
				(int)epi, (unsigned)epe, epn ? epn->name.c_str() : "?", px, py, pz,
				epp.range, epp.texture.IsValid() ? epp.texture.desc.width : 0,
				epp.texture.IsValid() ? epp.texture.desc.mip_levels : 0,
				epp.IsDirty() ? 1 : 0, epp.render_dirty ? 1 : 0, epp.IsRealTime() ? 1 : 0,
				epp.textureName.c_str(), epp.texture.IsValid() ? 1 : 0);
			if (epp.texture.IsValid() && epSaved < 16)
			{
				char epFile[128];
				_snprintf(epFile, sizeof(epFile), "envprobe_%d_%s.dds", (int)epi, epn ? epn->name.c_str() : "unnamed");
				epFile[sizeof(epFile) - 1] = 0;
				for (char* c = epFile; *c; ++c) if (*c == ' ' || *c == '\\' || *c == '/' || *c == ':') *c = '_';
				bool epOk = wi::helper::saveTextureToFile(epp.texture, epFile);
				fprintf(epF, "  -> %s %s\n", epFile, epOk ? "SAVED" : "SAVE-FAILED");
				if (epOk) epSaved++;
			}
		}
		// the raw sky cube (shaderscene.globalenvmap) for comparison — sky backdrop + probe
		// capture backdrop, sampled only at mip 0 by shaders
		bool epSkySaved = false;
		if (epScene.weathers.GetCount() > 0)
		{
			const wi::scene::WeatherComponent& epw = epScene.weathers[0];
			fprintf(epF, "weather.skyMapName=\"%s\" valid=%d\n", epw.skyMapName.c_str(), epw.skyMap.IsValid() ? 1 : 0);
			if (epw.skyMap.IsValid())
			{
				epSkySaved = wi::helper::saveTextureToFile(epw.skyMap.GetTexture(), "envprobe_sky.dds");
				fprintf(epF, "  -> envprobe_sky.dds %s (desc %ux%u mips=%u fmt=%d)\n",
					epSkySaved ? "SAVED" : "SAVE-FAILED",
					epw.skyMap.GetTexture().desc.width, epw.skyMap.GetTexture().desc.height,
					epw.skyMap.GetTexture().desc.mip_levels, (int)epw.skyMap.GetTexture().desc.format);
			}
		}
		// census: what does probes[0] photograph? Everything within radius of its position.
		// NOTE: probe rendering culls via the CACHED aabb layerMask (2.48 rule), so print that.
		struct EpHit { float dist; wi::ecs::Entity entity; const char* name; uint32_t layerMask; uint32_t filterMask; int renderable; };
		std::vector<EpHit> epHits;
		const size_t epObjCount = std::min(epScene.objects.GetCount(), epScene.aabb_objects.size());
		for (size_t eoi = 0; eoi < epObjCount; ++eoi)
		{
			wi::ecs::Entity eoe = epScene.objects.GetEntity(eoi);
			const wi::scene::TransformComponent* eot = epScene.transforms.GetComponent(eoe);
			if (eot == nullptr) continue;
			float dx = eot->world._41 - epGX, dy = eot->world._42 - epGY, dz = eot->world._43 - epGZ;
			float d = sqrtf(dx * dx + dy * dy + dz * dz);
			if (d > epRadius) continue;
			const wi::scene::NameComponent* eon = epScene.names.GetComponent(eoe);
			const wi::scene::ObjectComponent& eoo = epScene.objects[eoi];
			epHits.push_back({ d, eoe, eon ? eon->name.c_str() : "?",
				epScene.aabb_objects[eoi].layerMask, eoo.GetFilterMask(), eoo.IsRenderable() ? 1 : 0 });
		}
		std::sort(epHits.begin(), epHits.end(), [](const EpHit& a, const EpHit& b) { return a.dist < b.dist; });
		fprintf(epF, "census: %d objects within %.0f of probes[0] pos (%.0f,%.0f,%.0f) — sorted by distance:\n",
			(int)epHits.size(), epRadius, epGX, epGY, epGZ);
		int epWritten = 0;
		for (const EpHit& h : epHits)
		{
			if (epWritten++ >= 300) break;
			fprintf(epF, "  dist=%.0f entity=%u renderable=%d layerMask=0x%08x filterMask=0x%02x name=\"%s\"\n",
				h.dist, (unsigned)h.entity, h.renderable, h.layerMask, h.filterMask, h.name);
		}
		fclose(epF);
		_snprintf(result, resultSize, "OK: DUMP_ENVPROBE probes=%d cubes_saved=%d sky_saved=%d census_hits=%d r=%.0f -> envprobe_dump.txt + envprobe_*.dds",
			(int)epScene.probes.GetCount(), epSaved, epSkySaved ? 1 : 0, (int)epHits.size(), epRadius);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_TERRAINSURF") == 0)
	{
		// DUMP_TERRAINSURF [x z] — GGMAX 2.74 (#155 pipeline hunt): dump the live SVT atlases
		// (basecolor + surface) to DDS and the nearest terrain CHUNK material's texture-slot
		// state. Built because a chrome (roughness 0) terrain Surface.dds provably renders
		// IDENTICALLY to roughness 0.69 — this instrument splits bake-side from shading-side.
		float tsx = 0, tsz = 0;
		if (arg && arg[0]) sscanf_s(arg, "%f %f", &tsx, &tsz);
		wi::scene::Scene& tsScene = wi::scene::GetScene();
		if (tsScene.terrains.GetCount() == 0)
		{
			_snprintf(result, resultSize, "ERROR: DUMP_TERRAINSURF no wicked terrain");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::terrain::Terrain& tsTerr = tsScene.terrains[0];
		FILE* tsF = GGDiagFopen("terrainsurf_dump.txt", "w");
		int tsSaved = 0;
		using MC = wi::scene::MaterialComponent;
		const int tsMaps[2] = { MC::BASECOLORMAP, MC::SURFACEMAP };
		const char* tsNames[2] = { "terrain_atlas_basecolor.dds", "terrain_atlas_surface.dds" };
		for (int mi = 0; mi < 2; ++mi)
		{
			if (tsTerr.atlas.maps[tsMaps[mi]].texture.IsValid())
			{
				bool ok = wi::helper::saveTextureToFile(tsTerr.atlas.maps[tsMaps[mi]].texture, tsNames[mi]);
				if (tsF) fprintf(tsF, "atlas map %d -> %s %s (%ux%u fmt=%d)\n", tsMaps[mi], tsNames[mi],
					ok ? "SAVED" : "SAVE-FAILED",
					tsTerr.atlas.maps[tsMaps[mi]].texture.desc.width, tsTerr.atlas.maps[tsMaps[mi]].texture.desc.height,
					(int)tsTerr.atlas.maps[tsMaps[mi]].texture.desc.format);
				if (ok) tsSaved++;
			}
			else if (tsF) fprintf(tsF, "atlas map %d INVALID\n", tsMaps[mi]);
		}
		// nearest terrain chunk material: objects carrying FILTER_TERRAIN
		float tsBest = 1e30f; wi::ecs::Entity tsBestEnt = wi::ecs::INVALID_ENTITY;
		for (size_t oi = 0; oi < tsScene.objects.GetCount(); ++oi)
		{
			const wi::scene::ObjectComponent& oo = tsScene.objects[oi];
			if ((oo.filterMask & wi::enums::FILTER_TERRAIN) == 0) continue;
			wi::ecs::Entity oe = tsScene.objects.GetEntity(oi);
			const wi::scene::TransformComponent* ot = tsScene.transforms.GetComponent(oe);
			if (!ot) continue;
			float dx = ot->world._41 - tsx, dz = ot->world._43 - tsz;
			float d = dx * dx + dz * dz;
			if (d < tsBest) { tsBest = d; tsBestEnt = oe; }
		}
		if (tsBestEnt != wi::ecs::INVALID_ENTITY && tsF)
		{
			const wi::scene::MaterialComponent* cm = tsScene.materials.GetComponent(tsBestEnt);
			fprintf(tsF, "nearest chunk entity=%u dist=%.0f material=%s\n", (unsigned)tsBestEnt, sqrtf(tsBest), cm ? "YES" : "NO");
			if (cm)
			{
				wi::graphics::GraphicsDevice* dev = wi::graphics::GetDevice();
				fprintf(tsF, "  roughness=%.3f metalness=%.3f reflectance=%.3f\n", cm->roughness, cm->metalness, cm->reflectance);
				for (int t = 0; t < MC::TEXTURESLOT_COUNT; ++t)
				{
					if (!cm->textures[t].resource.IsValid() && cm->textures[t].name.empty()) continue;
					int desc = cm->textures[t].resource.IsValid()
						? dev->GetDescriptorIndex(&cm->textures[t].resource.GetTexture(), wi::graphics::SubresourceType::SRV) : -1;
					fprintf(tsF, "  slot %d: valid=%d desc=%d residency=%d feedback=%d lodclamp=%.1f name=\"%s\"\n",
						t, cm->textures[t].resource.IsValid() ? 1 : 0, desc,
						cm->textures[t].sparse_residencymap_descriptor, cm->textures[t].sparse_feedbackmap_descriptor,
						cm->textures[t].lod_clamp, cm->textures[t].name.c_str());
				}
			}
		}
		if (tsF) fclose(tsF);
		_snprintf(result, resultSize, "OK: DUMP_TERRAINSURF atlases_saved=%d chunk=%u -> terrainsurf_dump.txt", tsSaved, (unsigned)tsBestEnt);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_ENVPROBE_BRIGHTNESS") == 0)
	{
		// SET_ENVPROBE_BRIGHTNESS <f> — live scale on EnvironmentReflection_Global (engine
		// GGMAX 1.55 knob, default 1.0). 0.5 reproduces DX11's dielectric damping
		// (envColor *= 0.5*metalness+0.5 == 0.5 on metalness-0 surfaces like sand/terrain),
		// so 1.0 vs 0.5 is the DX12-vs-DX11 reflection-energy A/B on the beach.
		float ebv = 1.0f;
		if (arg == nullptr || sscanf_s(arg, "%f", &ebv) != 1)
		{
			_snprintf(result, resultSize, "ERROR: SET_ENVPROBE_BRIGHTNESS needs <float>");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::scene::gg_envprobe_brightness = ebv;
		_snprintf(result, resultSize, "OK: SET_ENVPROBE_BRIGHTNESS %.3f (1.0=DX12 stock, 0.5=DX11 dielectric damping)", ebv);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PROBEMARKERBRIGHTNESS") == 0)
	{
		// SET_PROBEMARKERBRIGHTNESS <f> — (2.90) drive the %probe marker's "Probe Brightness"
		// panel slider from the harness, on EVERY probe marker in the level. This is the
		// PER-PROBE knob (eleprof.light.fProbeBrightness -> g_envProbeList[].brightness ->
		// EnvironmentProbeComponent::filterBrightness, baked into the cube during BRDF mip
		// filtering) — NOT the global SET_ENVPROBE_BRIGHTNESS shader knob above. Setting it
		// raises g_bLightProbeScaleChanged, which is exactly what the panel does, so the
		// probe list rebuilds, SetBrightness self-dirties and the cube re-bakes.
		float pmb = 1.0f;
		if (arg == nullptr || sscanf_s(arg, "%f", &pmb) != 1)
		{
			_snprintf(result, resultSize, "ERROR: SET_PROBEMARKERBRIGHTNESS needs <float> (panel range 0.01-10)");
			result[resultSize - 1] = 0;
			return true;
		}
		int pmbCount = 0;
		for (int ee = 1; ee <= g.entityelementlist; ee++)
		{
			int pmbEnt = t.entityelement[ee].bankindex;
			if (pmbEnt > 0 && pmbEnt < (int)t.entityprofile.size() && t.entityprofile[pmbEnt].ismarker == 2)
			{
				if (t.entityelement[ee].eleprof.light.fLightHasProbe >= 50.0f)
				{
					t.entityelement[ee].eleprof.light.fProbeBrightness = pmb;
					pmbCount++;
				}
			}
		}
		g_bLightProbeScaleChanged = true;
		_snprintf(result, resultSize, "OK: SET_PROBEMARKERBRIGHTNESS %.3f applied to %d probe marker(s); probe list will rebuild and re-bake (1.0 = stock)", pmb, pmbCount);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PROBE_TEST") == 0)
	{
		// SET_PROBE_TEST <x> <y> <z> [range=383] [sx sy sz=100] — (2.75b, #155 round 3) drive
		// the REAL user-probe path without a placed marker: append to g_envProbeList exactly
		// like a probe marker does (G-Lighting.cpp walk), which activates the tracking system
		// and captures a pool probe AT the given position. Built to reproduce Lee's corrupt
		// ground-level (y=4.7) probe capture. Pair with DUMP_ENVPROBE to extract the cube.
		float ptx = 0, pty = 0, ptz = 0, ptr = 383.0f, psx = 100.0f, psy = 100.0f, psz = 100.0f, pyaw = 0.0f;
		int got = arg ? sscanf_s(arg, "%f %f %f %f %f %f %f %f", &ptx, &pty, &ptz, &ptr, &psx, &psy, &psz, &pyaw) : 0;
		if (got < 3)
		{
			_snprintf(result, resultSize, "ERROR: SET_PROBE_TEST needs <x> <y> <z> [range] [sx sy sz] [yawdeg]");
			result[resultSize - 1] = 0;
			return true;
		}
		const float pyawrad = pyaw * 3.14159265f / 180.0f;
		const float pqy = sinf(pyawrad * 0.5f), pqw = cosf(pyawrad * 0.5f);
		GGTerrain::GGTerrain_AddEnvProbeList(ptx, pty, ptz, ptr, 0, pqy, 0, pqw, psx, psy, psz, 1.0f);
		_snprintf(result, resultSize, "OK: SET_PROBE_TEST probe listed at (%.0f,%.0f,%.0f) range=%.0f size=(%.0f,%.0f,%.0f) yaw=%.0f — tracking assigns+captures a pool slot over the next frames",
			ptx, pty, ptz, ptr, psx, psy, psz, pyaw);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_DEBUGPROBES") == 0)
	{
		// SET_DEBUGPROBES <0|1> — (2.74b, #155 ball-visualizer hunt) force the engine's debug
		// env-probe spheres on/off without needing a probe marker picked in the editor
		// (G-Lighting.cpp:260 flips this on pick; it also re-clears it every frame while no
		// probe is picked, so pair this with a same-frame screenshot or pick a probe first).
		int dpOn = 0; float dpScale = 0.0f;
		if (arg) sscanf_s(arg, "%d %f", &dpOn, &dpScale);
		extern int gg_debugprobes_force; // consulted by lighting_loop (G-Lighting.cpp), which otherwise clears the flag every frame
		gg_debugprobes_force = dpOn;
		if (dpScale > 0.0f) wiRenderer::SetDebugEnvProbeSphereScale(dpScale); // 2.75 preview-size knob
		wiRenderer::SetToDrawDebugEnvProbes(dpOn != 0);
		_snprintf(result, resultSize, "OK: SET_DEBUGPROBES %d scale=%.0f", dpOn, dpScale);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PROBEVIEW") == 0)
	{
		// SET_PROBEVIEW <mode> [mip] [scale] — GGMAX 2.89 (#157) PROBE INSPECTION MODE.
		//   mode 0 = off (stock)
		//   mode 1 = the inspection sphere mirrors the GLOBAL cube (probes[0] = what every
		//            shader reads as GetScene().globalprobe)
		//   mode 2 = the same sphere, same pose, mirroring the LOCAL cube instead
		//   mip    = the level sampled (0 = the raw capture; >0 walks the filtered chain)
		//   scale  = sphere radius in world units when no probe marker is picked (default 60,
		//            about a %probe marker ball; a picked marker overrides this every frame)
		// The sphere is a RAW MIRROR (cubeMapPS: no Fresnel, no roughness, no parallax, no
		// brightness) so what it shows IS the cube, not the marker's PBR interpretation of it.
		int pvMode = 0; float pvMip = 0.0f, pvScale = 0.0f;
		if (arg) sscanf_s(arg, "%d %f %f", &pvMode, &pvMip, &pvScale);
		wiRenderer::SetProbeView(pvMode, pvMip);
		if (pvScale > 0.0f) wiRenderer::SetDebugEnvProbeSphereScale(pvScale);
		else if (pvMode > 0) wiRenderer::SetDebugEnvProbeSphereScale(60.0f);
		// Receipt: name the cube the mode will actually put on the ball, so a picture is never
		// trusted without knowing which texture produced it.
		wi::scene::Scene& pvScene = wi::scene::GetScene();
		char pvWho[192]; pvWho[0] = 0;
		if (pvScene.probes.GetCount() > 0)
		{
			wi::ecs::Entity pve = pvScene.probes.GetEntity(0);
			const wi::scene::NameComponent* pvn = pvScene.names.GetComponent(pve);
			const wi::scene::EnvironmentProbeComponent& pvp = pvScene.probes[0];
			_snprintf(pvWho, sizeof(pvWho), " global=probes[0] \"%s\" pos=(%.0f,%.0f,%.0f) res=%u mips=%u valid=%d",
				pvn ? pvn->name.c_str() : "?", pvp.position.x, pvp.position.y, pvp.position.z,
				pvp.texture.IsValid() ? pvp.texture.desc.width : 0,
				pvp.texture.IsValid() ? pvp.texture.desc.mip_levels : 0,
				pvp.texture.IsValid() ? 1 : 0);
			pvWho[sizeof(pvWho) - 1] = 0;
		}
		_snprintf(result, resultSize, "OK: SET_PROBEVIEW mode=%d (%s) mip=%.1f scale=%.0f probes=%d%s",
			pvMode,
			pvMode == 0 ? "off" : (pvMode == 1 ? "GLOBAL cube" : "LOCAL cube"),
			pvMip, pvScale, (int)pvScene.probes.GetCount(), pvWho);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PROBEPARALLAX") == 0)
	{
		// SET_PROBEPARALLAX <0|1|2|3> — GGMAX 2.89 (#157) env-probe parallax precision/policy.
		//   0 = stock half (min16float) math — REPRODUCES the circles defect
		//   1 = float math, parallax kept (the precision-only fix)
		//   2 = float math + MAGENTA wherever the stock half math would overflow fp16
		//   3 = float math + skip parallax entirely for level-sized boxes (DX11-style raw
		//       reflection vector for the global probe) — **the shipping DEFAULT**
		// Why it matters: the parallax ray-exit distance is in WORLD units. fp16 tops out at
		// 65504, and GG's globalEnvProbe OBB is 50,000 units, so the stock math returns +INF
		// for every direction outside six ~40 degree caps around the axes. Local probe boxes
		// are tiny, which is exactly why local reflections always looked clean.
		int ppMode = 3; // matches the shipping default
		if (arg) sscanf_s(arg, "%d", &ppMode);
		wi::scene::gg_probeparallax = ppMode; // engine wiScene.cpp:47
		// Report the geometry this predicts, so the picture can be checked against the maths.
		float ppHalfExtent = 0.0f;
		wi::scene::Scene& ppScene = wi::scene::GetScene();
		if (ppScene.probes.GetCount() > 0)
		{
			wi::ecs::Entity ppe = ppScene.probes.GetEntity(0);
			const wi::scene::TransformComponent* ppt = ppScene.transforms.GetComponent(ppe);
			if (ppt)
			{
				// the OBB half-extent is the transform's scale (the probe box is a unit cube)
				XMFLOAT3 ppScl = ppt->GetScale();
				ppHalfExtent = std::max(ppScl.x, std::max(ppScl.y, ppScl.z));
			}
		}
		const float ppWorstDist = ppHalfExtent * 1.7320508f; // corner-most exit distance
		_snprintf(result, resultSize, "OK: SET_PROBEPARALLAX %d (%s) probes[0] box half-extent=%.0f worst-exit=%.0f fp16max=65504 -> %s",
			ppMode,
			ppMode == 0 ? "stock half - defect ON" : (ppMode == 1 ? "float, parallax kept" : (ppMode == 2 ? "float + magenta overflow map" : "float + no parallax on level-sized boxes - DEFAULT")),
			ppHalfExtent, ppWorstDist,
			ppWorstDist > 65504.0f ? "STOCK MATH OVERFLOWS (circles expected at mode 0)" : "stock math stays in range (no circles even at mode 0)");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PROBEONLYGLOBAL") == 0)
	{
		// SET_PROBEONLYGLOBAL <0|1> — GGMAX 2.89 (#157). 1 = the shader ignores every LOCAL
		// probe so all surfaces read the GLOBAL cube. This is how the marker ball's circles are
		// reproduced on demand: the ball normally sits inside its own local probe box (clean),
		// and only falls through to the global cube while a drag releases the pool slot — which
		// is exactly the difference between Lee's shot 1 and shot 3. Done shader-side so
		// GGTerrain's per-frame pool tracking cannot undo it.
		int ogOn = 0;
		if (arg) sscanf_s(arg, "%d", &ogOn);
		wi::scene::gg_probeonlyglobal = ogOn; // engine wiScene.cpp
		_snprintf(result, resultSize, "OK: SET_PROBEONLYGLOBAL %d (%s)", ogOn,
			ogOn ? "local probes IGNORED - everything reads the global cube" : "stock local+global blending");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_GLOBALPROBEBOX") == 0)
	{
		// SET_GLOBALPROBEBOX <halfextent> — GGMAX 2.89 (#157) threshold experiment. Resize the
		// GLOBAL probe's parallax OBB live. The fp16 prediction is sharp: with stock half math
		// (SET_PROBEPARALLAX 0) the circles must vanish below 65504/sqrt(3) = 37820 units and
		// reappear above it. Nothing else in the pipeline has a threshold at that number, so a
		// clean switch across it is the proof. Does NOT re-capture the cube — box only.
		float gbExtent = 0.0f;
		if (arg) sscanf_s(arg, "%f", &gbExtent);
		if (gbExtent <= 0.0f)
		{
			_snprintf(result, resultSize, "ERROR: SET_GLOBALPROBEBOX needs a positive half-extent (stock global is 50000)");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::scene::Scene& gbScene = wi::scene::GetScene();
		if (gbScene.probes.GetCount() == 0)
		{
			_snprintf(result, resultSize, "ERROR: SET_GLOBALPROBEBOX no probes in scene");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::ecs::Entity gbe = gbScene.probes.GetEntity(0);
		wi::scene::TransformComponent* gbt = gbScene.transforms.GetComponent(gbe);
		const wi::scene::EnvironmentProbeComponent& gbp = gbScene.probes[0];
		if (gbt == nullptr)
		{
			_snprintf(result, resultSize, "ERROR: SET_GLOBALPROBEBOX probes[0] has no transform");
			result[resultSize - 1] = 0;
			return true;
		}
		gbt->ClearTransform();
		gbt->Translate(gbp.position);
		gbt->Scale(XMFLOAT3(gbExtent, gbExtent, gbExtent));
		gbt->UpdateTransform();
		gbt->SetDirty();
		_snprintf(result, resultSize, "OK: SET_GLOBALPROBEBOX half-extent=%.0f at (%.0f,%.0f,%.0f) worst-exit=%.0f -> stock half math %s",
			gbExtent, gbp.position.x, gbp.position.y, gbp.position.z, gbExtent * 1.7320508f,
			gbExtent * 1.7320508f > 65504.0f ? "OVERFLOWS (circles)" : "in range (clean)");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "REFRESH_ENVPROBE") == 0)
	{
		// Trigger the editor-identical full refresh: lighting_loop -> GGTerrain_ClearEnvProbeList
		// (which zeroes globalEnvProbePos.y, forcing the GLOBAL probe re-render) -> marker re-add.
		// Also fire WickedCall_UpdateProbes so the 2.73 pool re-bake path runs (same as a sky
		// change / level load does). Dump before AND after this to separate "stale capture"
		// from "reproducibly corrupt capture".
		g_bLightProbeScaleChanged = true;
		extern void WickedCall_UpdateProbes(void);
		WickedCall_UpdateProbes();
		_snprintf(result, resultSize, "OK: REFRESH_ENVPROBE g_bLightProbeScaleChanged + bUpdateProbes set (global + local probes will re-capture over the next frames)");
		result[resultSize - 1] = 0;
		return true;
	}
	return false;
}

// GGMAX 3.80: DUMP_REFLECTION - the planar reflection's two transforms, side by side.
//
// Lee: the crate's base does not line up with its reflection in the Puddle, and "alignment issues
// like this are the result of the transforms used to render the main scene being different from
// the transforms used to render the reflection scene". This prints both so the comparison is a
// subtraction rather than a judgement about a dark screenshot.
//
// What to read:
//   plane          the mirror plane (xyz = normal, w = -distance). Its height must equal the
//                  RENDERED water surface, not the object's bounding-box centre.
//   main / refl    Eye/At/Up, fov, width/height (these drive the PROJECTION aspect), near/far,
//                  and jitter. ★ If width/height differ between the two, their projections differ
//                  and the reflection cannot line up across the whole screen.
//   aspect         computed from width/height. The sampling in objectHF.hlsli reprojects world
//                  positions through reflection_view_projection, so any aspect or FOV difference
//                  between the matrix that RENDERED the reflection and the one that SAMPLES it
//                  shows up as a horizontal slide that grows toward the screen edges.
//   VP rows        the actual matrices. If the two differ only by the mirror, alignment is a
//                  plane problem; if they differ in scale terms, it is a projection problem.
static bool AutoHarness_ReflectionCommands(const char* cmd, const char* arg, char* result, int resultSize)
{
	// GGMAX 3.89: SET_REFLECTIONSIZE <0|128..2048> - Water Reflection Size, for sweeping the
	// planar reflection resolution without reaching for the panel. 0 = Auto (stock internal/4).
	// ⚠ C1061: the main dispatch ladder is AT the MSVC nesting limit, so this verb SHARES
	// this helper by widening its early-out. Never add an else-if arm for it.
	// GGMAX 3.90: SET_REFLECTIONBLUR <0..3>. Added because the 3.90 smoke test could not reach
	// the blur at all without it - it printed blur=0 twice and proved nothing while the UAV
	// store on an MSAA resolve target, the one part of the change with no in-tree precedent,
	// stayed unexecuted. A knob with no way to exercise it is a knob with no way to test it.
	// ⚠ C1061: shares this helper, never a new else-if arm.
	if (_stricmp(cmd, "SET_REFLECTIONBLUR") == 0)
	{
		const int v = atoi(arg);
		if (v < 0 || v > 3)
		{
			_snprintf(result, resultSize, "ERROR: SET_REFLECTIONBLUR %d out of range (0 = off, 1..3 = passes of the 9-tap separable Gaussian)", v);
			result[resultSize - 1] = 0;
			return true;
		}
		// ⚠ BOTH copies, as with the size verb - the .fpm saves from gamevisuals.
		t.gamevisuals.iReflectionBlur = t.visuals.iReflectionBlur = v;
		_snprintf(result, resultSize,
			"OK: SET_REFLECTIONBLUR %d. Applied by the push in MasterRenderer::Update, so it takes "
			"one frame - run DUMP_REFLECTION and read the REFLBLUR line to confirm.", v);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_REFLECTIONSIZE") == 0)
	{
		const int v = atoi(arg);
		if (v != 0 && (v < 128 || v > 2048))
		{
			_snprintf(result, resultSize, "ERROR: SET_REFLECTIONSIZE %d out of range (0 = Auto/quarter of screen, otherwise 128..2048 WIDE - the height follows the main camera aspect)", v);
			result[resultSize - 1] = 0;
			return true;
		}
		// ⚠ BOTH copies. The per-frame push in MasterRenderer::Update reads t.visuals and
		// the Test Game return path restores from editorvisuals.
		t.gamevisuals.iReflectionWidth = t.visuals.iReflectionWidth = v;
		_snprintf(result, resultSize,
			"OK: SET_REFLECTIONSIZE %d (0 = Auto = internal resolution / 4). The four reflection "
			"targets are re-created by the push in MasterRenderer::Update, so this takes one frame "
			"- run DUMP_REFLECTION and read the REFLSIZE line to confirm.", v);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_REFLECTION") != 0) return false;

	extern MasterRenderer* master_renderer;
	if (master_renderer == nullptr)
	{
		_snprintf(result, resultSize, "ERROR: DUMP_REFLECTION no master_renderer");
		result[resultSize - 1] = 0;
		return true;
	}

	const wi::scene::CameraComponent& mc = *(master_renderer->camera);
	const wi::scene::CameraComponent& rc = master_renderer->camera_reflection;
	const wi::renderer::Visibility& vis = master_renderer->visibility_main;

	// ⚠⚠ GGMAX 3.90: THESE ARE TARGET SIZES AND THEY DO NOT FEED THE PROJECTION.
	//
	// This line reported refl aspect 1.920000 against main 1.917603 and I spent an hour on it
	// as a suspected sampling misalignment. It is an artefact of THIS dump. camera_reflection
	// is copied from the main camera and Reflect() runs UpdateCamera(), which builds P from
	// 1536/801; the 384x200 is written to the width/height FIELDS 381 lines later and reaches
	// only internal_resolution and the scissor. Nothing rebuilds the matrix after.
	//
	// The honest check is below: mainVP and reflVP _11 must match in magnitude, and they do.
	// Read THAT, not these two numbers.
	const float mAspect = (mc.height  != 0.0f) ? (mc.width  / mc.height)  : 0.0f;  // target, not projection
	const float rAspect = (rc.height  != 0.0f) ? (rc.width  / rc.height)  : 0.0f;  // target, not projection

	int w = 0;
	w += _snprintf(result + w, resultSize - w,
		"REFLECTION planar_visible=%d\n"
		"  plane      n=(%.5f,%.5f,%.5f) w=%.4f   (mirror height = %.4f when n=(0,1,0))\n",
		vis.planar_reflection_visible ? 1 : 0,
		vis.reflectionPlane.x, vis.reflectionPlane.y, vis.reflectionPlane.z, vis.reflectionPlane.w,
		-vis.reflectionPlane.w);

	w += _snprintf(result + w, resultSize - w,
		"  main  eye=(%.2f,%.2f,%.2f) at=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f)\n"
		"        fov=%.5f w=%.1f h=%.1f aspect=%.6f near=%.3f far=%.1f jitter=(%.5f,%.5f)\n",
		mc.Eye.x, mc.Eye.y, mc.Eye.z, mc.At.x, mc.At.y, mc.At.z, mc.Up.x, mc.Up.y, mc.Up.z,
		mc.fov, mc.width, mc.height, mAspect, mc.zNearP, mc.zFarP, mc.jitter.x, mc.jitter.y);

	w += _snprintf(result + w, resultSize - w,
		"  refl  eye=(%.2f,%.2f,%.2f) at=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f)\n"
		"        fov=%.5f w=%.1f h=%.1f aspect=%.6f near=%.3f far=%.1f jitter=(%.5f,%.5f)\n",
		rc.Eye.x, rc.Eye.y, rc.Eye.z, rc.At.x, rc.At.y, rc.At.z, rc.Up.x, rc.Up.y, rc.Up.z,
		rc.fov, rc.width, rc.height, rAspect, rc.zNearP, rc.zFarP, rc.jitter.x, rc.jitter.y);

	// ★ the three differences that would each produce a visible misalignment
	w += _snprintf(result + w, resultSize - w,
		"  DELTA aspect=%.6f  fov=%.6f  near=%.4f  far=%.2f  mainJitter=%d\n",
		rAspect - mAspect, rc.fov - mc.fov, rc.zNearP - mc.zNearP, rc.zFarP - mc.zFarP,
		(mc.jitter.x != 0.0f || mc.jitter.y != 0.0f) ? 1 : 0);

	w += _snprintf(result + w, resultSize - w,
		"  clipPlane  =(%.5f,%.5f,%.5f,%.4f)\n"
		"  clipPlaneOrig=(%.5f,%.5f,%.5f,%.4f)\n",
		rc.clipPlane.x, rc.clipPlane.y, rc.clipPlane.z, rc.clipPlane.w,
		rc.clipPlaneOriginal.x, rc.clipPlaneOriginal.y, rc.clipPlaneOriginal.z, rc.clipPlaneOriginal.w);

	// the matrices themselves - if these disagree by more than the mirror, it is a projection bug
	w += _snprintf(result + w, resultSize - w,
		"  mainVP r0=(%.5f,%.5f,%.5f,%.5f) r1=(%.5f,%.5f,%.5f,%.5f)\n"
		"  reflVP r0=(%.5f,%.5f,%.5f,%.5f) r1=(%.5f,%.5f,%.5f,%.5f)\n",
		mc.VP._11, mc.VP._12, mc.VP._13, mc.VP._14, mc.VP._21, mc.VP._22, mc.VP._23, mc.VP._24,
		rc.VP._11, rc.VP._12, rc.VP._13, rc.VP._14, rc.VP._21, rc.VP._22, rc.VP._23, rc.VP._24);

	// ★ GGMAX 3.81: the whole Reflections enable chain in one line. Lee sees the tick ON in
	// DX11 and OFF in DX12 for the same level, and each link can break independently:
	//   visuals          what the level/visuals.ini loaded and the checkbox binds to
	//   gamevisuals      the play-time copy; the editor copies FROM it on some load paths
	//   shaderlevels     entities==3 (LOW) force-clears the flag in M-Visuals_part1.cpp:688
	//                    - DX11 has the identical branch, so a DIFFERENT value here is the tell
	//   renderer         what the engine is actually doing, past all of the above
	{
		extern MasterRenderer* master_renderer;
		w += _snprintf(result + w, resultSize - w,
			"  REFLSTATE visuals=%d gamevisuals=%d shaderlevels.entities=%d renderer=%d\n",
			t.visuals.bReflectionsEnabled ? 1 : 0,
			t.gamevisuals.bReflectionsEnabled ? 1 : 0,
			(int)t.visuals.shaderlevels.entities,
			(master_renderer && master_renderer->getReflectionsEnabled()) ? 1 : 0);
		// GGMAX 3.89: what size the reflection is actually being rendered at, and whether the
		// two stores and the engine agree. visuals != engine means the push in
		// MasterRenderer::Update has not run yet (it takes one frame) or reflections are off.
		{
			int rtw = 0, rth = 0, rts = 0, rtv = 0;
			if (master_renderer && master_renderer->rtReflection.IsValid())
			{
				const wi::graphics::TextureDesc& rd = master_renderer->rtReflection.GetDesc();
				rtw = (int)rd.width; rth = (int)rd.height; rts = (int)rd.sample_count; rtv = 1;
			}
			w += _snprintf(result + w, resultSize - w,
				"  REFLSIZE visuals=%d gamevisuals=%d engine=%d (0 = Auto = internal/4)  rt=%dx%d samples=%d valid=%d\n",
				t.visuals.iReflectionWidth, t.gamevisuals.iReflectionWidth,
				wi::renderer::gg_reflection_width, rtw, rth, rts, rtv);
			w += _snprintf(result + w, resultSize - w,
				"  REFLBLUR visuals=%d gamevisuals=%d engine=%d (0 = off, else 9-tap Gaussian passes)\n",
				t.visuals.iReflectionBlur, t.gamevisuals.iReflectionBlur,
				wi::renderer::gg_reflection_blur);
		}
		{
			extern int g_ggVisualsGuardRan; extern char g_ggVisualsGuardCwd[512];
			w += _snprintf(result + w, resultSize - w,
				"  VISGUARD ran=%d (-1=never reached, 0=FileExist(\"visuals.ini\") said NO) cwd=\"%s\"\n",
				g_ggVisualsGuardRan, g_ggVisualsGuardCwd);
			extern int g_ggReflTrace[4];
			w += _snprintf(result + w, resultSize - w,
				"  VISTRACE afterParse=%d beforeEditorDefaults=%d afterEditorDefaults=%d atApplyVisuals=%d\n",
				g_ggReflTrace[0], g_ggReflTrace[1], g_ggReflTrace[2], g_ggReflTrace[3]);
		}
	}

	// ★ Who asked for the reflection, and is the plane at their SURFACE or their CENTRE?
	// wiRenderer.cpp:4787 builds the plane from object.center - the centre of the bounding box.
	// For a flat puddle centre == surface and that is fine; for anything with thickness the mirror
	// sits below the visible water and every reflection is offset by twice the difference.
	{
		wi::scene::Scene& sc = wi::scene::GetScene();
		int listed = 0;
		w += _snprintf(result + w, resultSize - w, "  REQUESTERS (IsRequestPlanarReflection):\n");
		for (size_t i = 0; i < sc.objects.GetCount() && w < resultSize - 300; ++i)
		{
			const wi::scene::ObjectComponent& o = sc.objects[i];
			if (!o.IsRequestPlanarReflection()) continue;
			wi::ecs::Entity e = sc.objects.GetEntity(i);
			const wi::scene::NameComponent* nm = sc.names.GetComponent(e);
			const wi::primitive::AABB* bb = (i < sc.aabb_objects.size()) ? &sc.aabb_objects[i] : nullptr;
			listed++;
			if (bb != nullptr)
			{
				const float topGap = bb->_max.y - o.center.y;
				w += _snprintf(result + w, resultSize - w,
					"    \"%s\" center=(%.3f,%.4f,%.3f) aabbY=[%.4f..%.4f] thickness=%.4f centre-to-top=%.4f\n",
					nm ? nm->name.c_str() : "(unnamed)",
					o.center.x, o.center.y, o.center.z,
					bb->_min.y, bb->_max.y, bb->_max.y - bb->_min.y, topGap);
			}
			else
			{
				w += _snprintf(result + w, resultSize - w,
					"    \"%s\" center=(%.3f,%.4f,%.3f) (no aabb)\n",
					nm ? nm->name.c_str() : "(unnamed)", o.center.x, o.center.y, o.center.z);
			}
		}
		if (listed == 0) w += _snprintf(result + w, resultSize - w, "    (none - the plane came from the OCEAN path)\n");
	}

	result[resultSize - 1] = 0;
	return true;
}

static bool AutoHarness_TransparencyCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "DUMP_TRANSPARENTS") == 0)
	{
		// DUMP_TRANSPARENTS [name-substr] — name the render state of every subset that the
		// transparent pass will touch. The three fields that decide whether the hair/leaf
		// parity rule fires are dsided (mesh OR material double-sided), blend (the EFFECTIVE
		// GetBlendMode, not userBlendMode — a material can be pulled transparent by its
		// filter mask alone) and filt&TRANSP. A hair material that reports dsided=0 would mean
		// the depth-write theory is aimed at the wrong axis.
		wi::scene::Scene& trScene = wi::scene::GetScene();
		auto trContains = [](const std::string& hay, const char* needle) -> bool {
			if (needle == nullptr || needle[0] == 0) return true;
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		FILE* trF = GGDiagFopen("transparents_dump.txt", "w");
		if (trF == nullptr)
		{
			_snprintf(result, resultSize, "ERROR: DUMP_TRANSPARENTS could not open transparents_dump.txt");
			result[resultSize - 1] = 0;
			return true;
		}
		int trObjects = 0, trSubsets = 0, trDoubleSidedTransparent = 0, trForceDepth = 0;
		for (size_t toi = 0; toi < trScene.objects.GetCount(); ++toi)
		{
			wi::ecs::Entity toe = trScene.objects.GetEntity(toi);
			const wi::scene::NameComponent* ton = trScene.names.GetComponent(toe);
			if (!trContains(ton ? ton->name : std::string(), arg)) continue;
			const wi::scene::ObjectComponent& too = trScene.objects[toi];
			const wi::scene::MeshComponent* tom = trScene.meshes.GetComponent(too.meshID);
			if (tom == nullptr) continue;
			bool trHeaderWritten = false;
			for (const auto& tos : tom->subsets)
			{
				const wi::scene::MaterialComponent* tmat = trScene.materials.GetComponent(tos.materialID);
				if (tmat == nullptr) continue;
				const uint32_t trFilter = tmat->GetFilterMask();
				const bool trTransparent = (trFilter & wi::enums::FILTER_TRANSPARENT) != 0;
				const bool trDouble = tom->IsDoubleSided() || tmat->IsDoubleSided();
				// only the rows that can matter: anything the transparent pass draws, or
				// anything double-sided (a leaf/hair card that is NOT transparent is worth
				// seeing too, because it explains a material that the rule will skip).
				if (!trTransparent && !trDouble) continue;
				if (!trHeaderWritten)
				{
					const wi::scene::TransformComponent* tot = trScene.transforms.GetComponent(toe);
					fprintf(trF, "OBJ entity=%llu name=\"%s\" pos=(%.0f,%.0f,%.0f) subsets=%d\n",
						(unsigned long long)toe, ton ? ton->name.c_str() : "?",
						tot ? tot->world._41 : 0.0f, tot ? tot->world._42 : 0.0f, tot ? tot->world._43 : 0.0f,
						(int)tom->subsets.size());
					trHeaderWritten = true;
					trObjects++;
				}
				trSubsets++;
				if (trTransparent && trDouble) trDoubleSidedTransparent++;
				if (tmat->IsForceDepth()) trForceDepth++;
				// GGMAX 2.09: forcedepth wins over the hair rule when both would match, so label it
				// that way — the two are mutually exclusive in RenderMeshes.
				const bool trCarve = tmat->IsForceDepth();
				fprintf(trF,
					"  mat=%llu blend=%d userblend=%d dsided=%d(mesh=%d,mat=%d) alphaRef=%.3f alphatest=%d "
					"filt=0x%X%s opacity=%.2f shader=%d forcedepth=%d base=\"%s\"%s\n",
					(unsigned long long)tos.materialID,
					(int)tmat->GetBlendMode(), (int)tmat->userBlendMode,
					trDouble ? 1 : 0, tom->IsDoubleSided() ? 1 : 0, tmat->IsDoubleSided() ? 1 : 0,
					tmat->alphaRef, tmat->IsAlphaTestEnabled() ? 1 : 0,
					trFilter, trTransparent ? "(TRANSP)" : "",
					tmat->baseColor.w, (int)tmat->shaderType, trCarve ? 1 : 0,
					tmat->textures[wi::scene::MaterialComponent::BASECOLORMAP].name.c_str(),
					(trCarve && trTransparent) ? "   <== WEAPON depth-carve applies"
						: ((trTransparent && trDouble) ? "   <== hair/leaf parity rule applies" : ""));
			}
		}
		fprintf(trF, "OBJECTS %d SUBSETS %d DOUBLESIDED_TRANSPARENT %d\n",
			trObjects, trSubsets, trDoubleSidedTransparent);
		fclose(trF);
		_snprintf(result, resultSize,
			"OK: DUMP_TRANSPARENTS \"%s\" objects=%d subsets=%d doublesided_transparent=%d "
			"hairknob=%d nodepthwrite_draws=%llu | weaponknob=%d forcedepth_mats=%d forcedepth_draws=%llu "
			"-> transparents_dump.txt",
			arg ? arg : "", trObjects, trSubsets, trDoubleSidedTransparent,
			wi::renderer::gg_transparent_doublesided_nodepthwrite ? 1 : 0,
			(unsigned long long)wi::renderer::GG_GetNoDepthWriteDrawCount(),
			wi::renderer::gg_weapon_forcedepth ? 1 : 0, trForceDepth,
			(unsigned long long)wi::renderer::GG_GetForceDepthDrawCount());
	}
	else if (_stricmp(cmd, "SET_HAIRDEPTH") == 0)
	{
		// SET_HAIRDEPTH <0|1> — live A/B for the hair/leaf parity rule. 1 = DX11 behaviour
		// (double-sided transparent draws with depth WRITE off, back faces then front faces),
		// 0 = upstream behaviour (depth write on, back faces then both faces). Both pipeline
		// permutations are built at LoadShaders, so this needs no shader reload — unlike
		// lazypso, which has to be set before the engine starts.
		wi::renderer::gg_transparent_doublesided_nodepthwrite = (atoi(arg) != 0);
		_snprintf(result, resultSize, "OK: SET_HAIRDEPTH nodepthwrite=%d (draws so far=%llu)",
			wi::renderer::gg_transparent_doublesided_nodepthwrite ? 1 : 0,
			(unsigned long long)wi::renderer::GG_GetNoDepthWriteDrawCount());
	}
	else if (_stricmp(cmd, "SET_WEAPONDEPTH") == 0)
	{
		// SET_WEAPONDEPTH <0|1> — live A/B for the first-person weapon depth carve.
		// 1 = DX11 behaviour (back faces stamp the weapon's volume with compare ALWAYS, then the
		// front faces draw over it, so world geometry cannot clip the weapon), 0 = the DX12 port's
		// behaviour (ordinary depth test, so a wall the player stands against cuts into the gun).
		// Selection-time like SET_HAIRDEPTH: both permutations are built at LoadShaders, no reload.
		// A zero forcedepth_draws with a weapon on screen means the GG_FORCEDEPTH material flag
		// never reached the renderer — that would be a game-side fault, not a pipeline one.
		wi::renderer::gg_weapon_forcedepth = (atoi(arg) != 0);
		_snprintf(result, resultSize, "OK: SET_WEAPONDEPTH forcedepth=%d (carve draws so far=%llu)",
			wi::renderer::gg_weapon_forcedepth ? 1 : 0,
			(unsigned long long)wi::renderer::GG_GetForceDepthDrawCount());
	}
	else if (_stricmp(cmd, "SET_WEAPONSHADOW") == 0)
	{
		// SET_WEAPONSHADOW <0|1> — live A/B for the first-person weapon SHADOW pull (GGMAX 2.14,
		// DX11 SHADERTYPE_WEAPON / WEAPON_SHADOW parity; the second half of the 2.09 weapon work).
		// 1 = DX11 behaviour (each light's shadow lookup uses a position pulled to 1/3 of the camera
		// distance, so a gun clipping into a wall is not shadowed BY that wall), 0 = the DX12 port's
		// behaviour (the gun goes dark whenever the player presses against geometry).
		// Unlike SET_WEAPONDEPTH this is a TRUE revert as well as an A/B: the material bit only
		// marks "this is a weapon" and the knob drives a per-frame FrameCB bit the shader tests
		// alongside it, so 0 restores the pre-2.14 look within a frame with no reload.
		// ⚠ The symptom only appears with the player pressed into geometry, and the harness cannot
		// drive the player (see the 2.09 notes) — flip this and ask for one walk into a wall.
		wi::renderer::gg_weapon_shadow = (atoi(arg) != 0);
		_snprintf(result, resultSize, "OK: SET_WEAPONSHADOW weaponshadow=%d (forcedepth=%d, carve draws=%llu)",
			wi::renderer::gg_weapon_shadow ? 1 : 0,
			wi::renderer::gg_weapon_forcedepth ? 1 : 0,
			(unsigned long long)wi::renderer::GG_GetForceDepthDrawCount());
	}
	else if (_stricmp(cmd, "SET_XINPUT") == 0)
	{
		// SET_XINPUT <frames> — GGMAX 2.15 gamepad-poll throttle. XInputGetState on an EMPTY
		// controller slot is a driver round-trip, and stock Wicked polled all four slots every
		// frame: 0.29 ms of a 4.62 ms editor CPU frame on Switch Escape with no pad attached.
		// <frames> = how often a known-empty slot is re-probed (staggered, max one per frame).
		// 0 = stock every-frame polling, for A/B. CONNECTED pads are always polled every frame
		// at any setting, so this can never add controller latency — the only thing it delays
		// is noticing a hotplug (by up to <frames> frames).
		wi::input::xinput::gg_xinput_rescan_frames = (uint32_t)atoi(arg);
		_snprintf(result, resultSize, "OK: SET_XINPUT rescan_frames=%u (0=stock every-frame poll of all 4 slots)",
			wi::input::xinput::gg_xinput_rescan_frames);
	}
	else if (_stricmp(cmd, "SET_SUBMITSTATS") == 0)
	{
		// SET_SUBMITSTATS 1 — GGMAX 2.16, reset the rolling submit-stall window so the next
		// GET_PERF_DATA's SUBMIT_STALL_WINDOW covers only the arm you are measuring.
		// Exists because SUBMIT_PHASES_MS is a LAST-FRAME snapshot: on Switch Escape `stall`
		// read 0.00 three times and 0.38 once, and the 0.00 was used to wrongly exonerate the
		// GPU fence as the absorber of CPU slack. Call this at the top of every A/B arm.
		wi::graphics::GG_ResetSubmitStats();
		_snprintf(result, resultSize, "OK: SET_SUBMITSTATS reset (SUBMIT_STALL_WINDOW now accumulating from this frame)");
	}
	else if (_stricmp(cmd, "SET_SCENESERIAL") == 0)
	{
		// SET_SCENESERIAL <0|1> — GGMAX 2.15 DIAGNOSTIC. Scene::Update's systems only DISPATCH
		// jobs, so their whole cost lands in the one jobsystem::Wait that closes each stage —
		// which is why "Scene-S1 0.91 ms" shows only 0.03 ms of named children and no ordinary
		// profiler range can attribute it. With this on, each instrumented system gets its own
		// Wait + "SU-<name>" range.
		// ⚠ Serialising REMOVES cross-system overlap, so the TOTAL frame inflates — compare the
		// SU-* shares against each other, NEVER the total against a normal frame. Never ship on.
		wi::scene::gg_scene_serial_profile = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize, "OK: SET_SCENESERIAL %d (diagnostic: serialises Scene::Update systems; totals inflate, shares are real)",
			wi::scene::gg_scene_serial_profile);
	}
	else if (_stricmp(cmd, "SET_INSTINIT") == 0)
	{
		// SET_INSTINIT <0|1> — GGMAX 2.18. A/B the parallel instance-array blank pass in
		// Scene::Update. 0 = stock Wicked (ONE worker memcpys 256 B x instanceArraySize into
		// write-combined UPLOAD memory every frame — ~1.87 MB on Switch Escape); 1 = the same
		// writes Dispatch'd across the worker pool. Behaviour-neutral by construction, so the
		// only thing to watch is the Scene-S1 range: S1 is 1.02 ms there but its named systems
		// total just 0.07 ms, and this pass is the biggest unnamed occupant of that gap.
		wi::scene::gg_instinit_parallel = (atoi(arg) != 0);
		_snprintf(result, resultSize, "OK: SET_INSTINIT %d (%s instance-array blank pass)",
			wi::scene::gg_instinit_parallel ? 1 : 0,
			wi::scene::gg_instinit_parallel ? "parallel" : "stock single-worker");
	}
	else if (_stricmp(cmd, "SET_LIGHTFALLOFF") == 0)
	{
		// SET_LIGHTFALLOFF <0|1> — live A/B for the GGMAX 2.10 light power parity. 1 = DX11
		// behaviour (energy 30 × (1-d²/r²)², no inverse-square — broad even flood reaching the
		// authored range), 0 = the DX12 port's old look (windowed 1/d² with the range²×π/4
		// intensity heuristic — hot pool at the source). FULLY live in one knob: the shader
		// branches on a per-frame FrameCB bit AND lighting_loop re-pushes every light's
		// intensity through WickedCall_UpdateLight each frame, which reads the same bool — so
		// both halves flip together within a frame. No reload, no early-parse trap.
		wi::renderer::gg_dx11_light_falloff = (atoi(arg) != 0);
		_snprintf(result, resultSize, "OK: SET_LIGHTFALLOFF dx11curve=%d",
			wi::renderer::gg_dx11_light_falloff ? 1 : 0);
	}
	else if (_stricmp(cmd, "GPUP_DUMP") == 0)
	{
		// GPUP_DUMP — legacy gpup particle forensics (tasks #120/#121). Writes gpup_dump.txt
		// (settings, cadence counters, per-emitter parsed fields + the GPU-visible constants)
		// next to the exe; the result line carries the cadence summary. Two dumps N seconds
		// apart give the effective sim rate: d(time_sum)/N ≈ 66.6/s means 1.0× real time.
		// max_time is the worst single-step warp — after the dt-cap fix it must stay ≤ 2.23;
		// larger values mean an uncapped hitch reached the sim (the "particles reset" bug).
		char sum[512] = {0};
		GPUParticles::gpup_debug_dump(sum, sizeof(sum));
		_snprintf(result, resultSize, "OK: GPUP_DUMP %s -> gpup_dump.txt", sum);
	}
	else if (_stricmp(cmd, "GPUP_SHOW") == 0)
	{
		// GPUP_SHOW <0|1> — attribution lever: 0 skips ALL legacy gpup drawing (sim keeps
		// running). Separates "the steam is causing X" from everything else in one flip.
		GPUParticles::gpup_debug_show(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_SHOW draw=%d", atoi(arg) != 0 ? 1 : 0);
	}
	else if (_stricmp(cmd, "GPUP_SET_SN") == 0)
	{
		// GPUP_SET_SN <value> — fast-forward the gpup sn clock (hash seed source). The
		// fp32 hash-degradation bug needed ~50 min to appear; this reaches any magnitude
		// instantly. Post-fix invariant: the steam look must NOT change with this value.
		GPUParticles::gpup_debug_set_sn((float)atof(arg));
		_snprintf(result, resultSize, "OK: GPUP_SET_SN sn=%s", arg ? arg : "0");
	}
	else if (_stricmp(cmd, "GPUP_SET_CLOCKS") == 0)
	{
		// GPUP_SET_CLOCKS <sn> <rotsn> <agk_seconds> — inject all three gpup global clocks
		// at once (the ~55-min white-out bisect: age one clock at a time on a fresh level).
		float fSn = 0, fRot = 0, fAgk = 0;
		if (arg) sscanf_s(arg, "%f %f %f", &fSn, &fRot, &fAgk);
		GPUParticles::gpup_debug_set_clocks(fSn, fRot, fAgk);
		_snprintf(result, resultSize, "OK: GPUP_SET_CLOCKS sn=%.1f rotsn=%.1f agk=%.1f", fSn, fRot, fAgk);
	}
	else if (_stricmp(cmd, "SET_LENSFLARE") == 0)
	{
		// SET_LENSFLARE <0|1> — lens flares draw AFTER the gpup hook in the transparent
		// pass (task #120: the white-out is a fullscreen ~88%-opacity white overlay; a
		// flare element stretched fullscreen, its draw poisoned by leftover gpup state,
		// fits every observation). Toggling this in a live white-out convicts/exonerates.
		extern MasterRenderer * master_renderer;
		if (master_renderer)
		{
			master_renderer->setLensFlareEnabled(atoi(arg) != 0);
			_snprintf(result, resultSize, "OK: SET_LENSFLARE %s", atoi(arg) ? "ON" : "OFF");
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: no master_renderer");
		}
	}
	else if (_stricmp(cmd, "DUMP_SUN") == 0)
	{
		// DUMP_SUN — per-stage readback stats of the light-shaft chain (task #120: the steam
		// white-out IS the shafts contribution; this names the stage that goes white:
		// sun0 = DrawSun mask, sun2 = downsample, sun1 = radial blur output). Lives in this
		// helper chain because the main dispatch chain is at the MSVC C1061 nesting limit.
		extern MasterRenderer * master_renderer;
		if (master_renderer)
		{
			master_renderer->GG_DumpSunChain(result, (int)resultSize);
		}
		else
		{
			_snprintf(result, resultSize, "ERROR: no master_renderer");
		}
	}
	else if (_stricmp(cmd, "GPUP_DRAWLOG") == 0)
	{
		// GPUP_DRAWLOG <0|1> — per-frame draw flight recorder for emitter 2 (task #122):
		// one CSV row per rendered frame (clocks, currImage, spawn window, warp, rota,
		// blend, opacity) to Files/gpup_drawlog.csv. Correlate with a 30fps window capture.
		GPUParticles::gpup_debug_drawlog(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_DRAWLOG %d", atoi(arg));
	}
	else if (_stricmp(cmd, "GPUP_SOLO") == 0)
	{
		// GPUP_SOLO <n|-1> — draw ONLY emitter n (-1 = all). Round-5 attribution: the ping
		// (one-frame persistent plume vanish with a static pool) follows one emitter's draw.
		GPUParticles::gpup_debug_solo(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_SOLO %d", atoi(arg));
	}
	else if (_stricmp(cmd, "GPUP_AGES") == 0)
	{
		// GPUP_AGES <enr> — full age-plane snapshot (all P*P slots) appended to gpup_ages.csv
		// (task #122 round 7): polled across a mass-respawn event, the birth mask exposes the
		// over-spawn geometry the 120-slot tracker cannot resolve.
		int n = GPUParticles::gpup_debug_ages(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_AGES e%d slots=%d -> Files/gpup_ages.csv", atoi(arg), n);
	}
	else if (_stricmp(cmd, "GPUP_REBIND") == 0)
	{
		// GPUP_REBIND <0|1> — force full descriptor re-dirty per gpup draw via decoy binds
		// (task #122 round 7): defeats the engine BindResource/BindSampler compare-skip so
		// every draw flushes a freshly copied descriptor table. Pings dying under 1 convicts
		// the stale-descriptor-table class named by the round-6 bisect.
		GPUParticles::gpup_debug_rebind(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_REBIND %d", atoi(arg));
	}
	else if (_stricmp(cmd, "GPUP_TRACK") == 0)
	{
		// GPUP_TRACK <emitter> — per-particle journey telemetry (task #122 round 4): decode
		// 120 fixed particle slots (age/pos/speed) from the sim textures into
		// Files/gpup_track.csv. Called at ~1Hz, rows chain into per-slot trajectories:
		// mass mid-life jumps, mass age resets, and age stalls are directly visible.
		int n = GPUParticles::gpup_debug_track(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_TRACK e%d rows=%d -> Files/gpup_track.csv", atoi(arg), n);
	}
	else if (_stricmp(cmd, "GPUP_CANARY") == 0)
	{
		// GPUP_CANARY <0|1|2> — shader debug mode (task #120): 1 = red fixed-size dots at
		// pool positions (bypasses size/alpha/color/billboard math), 2 = also bypass the
		// pool samples (grid positions). In a white-out: canary1 normal = poison in the
		// bypassed path; canary1 broken + canary2 normal = pool SAMPLING is the victim.
		GPUParticles::gpup_debug_canary(atoi(arg));
		_snprintf(result, resultSize, "OK: GPUP_CANARY mode=%d", atoi(arg));
	}
	else if (_stricmp(cmd, "GPUP_REGEN") == 0)
	{
		// GPUP_REGEN — re-create texNoiseOrig/texDist2 from their PNGs on a LIVE instance
		// (task #120). These are the only gpup GPU resources with process lifetime; on a
		// white-out catch, REGEN clearing the screen CONFIRMS input-texture poisoning (and
		// the durable fix: re-create at level load). No effect on healthy scenes.
		GPUParticles::gpup_debug_regen_textures();
		_snprintf(result, resultSize, "OK: GPUP_REGEN noiseOrig+dist2+5 static buffers recreated from source");
	}
	else if (_stricmp(cmd, "SET_GPUPARM") == 0)
	{
		// SET_GPUPARM <0|1|2> — pin the gpup update arm: 0 = stock FPS-dependent branch,
		// 1 = force the whole-batch arm, 2 = force the split arm (needs 2+ emitters). The
		// stock threshold (7.143ms = 140 FPS) sits inside the editor's frame-time band, so
		// the arm can flip with view FPS; this lever gives a same-FPS look A/B of the arms.
		GPUParticles::gpup_debug_force_arm(atoi(arg));
		_snprintf(result, resultSize, "OK: SET_GPUPARM force_arm=%d", atoi(arg));
	}
	else
	{
		return false;
	}
	result[resultSize - 1] = 0;
	return true;
}

// GGMAX 2026-08-05: outline-pipeline diagnostic commands, hoisted out of the main
// dispatch chain - the else-if chain hit MSVC C1061 (blocks nested too deeply),
// since every chain link nests one block deeper. Returns true if cmd was handled.
static bool AutoHarness_OutlineCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
#define sizeof_result_compat resultSize
	if (_stricmp(cmd, "DUMP_OUTLINE") == 0)
	{
		// DUMP_OUTLINE - name which stage of the selection-outline pipeline is dead.
		// Stages: FEED (editor globals set from selection) -> STENCIL (objects carrying a
		// user stencil ref) -> MASK (RenderOutlineHighlighers passes) -> COMPOSITE
		// (Wicked_Render_Opaque_Scene Postprocess_Outline). Plus every gate the chain
		// tests: grideditselect==5, prefs checkbox/thickness, depth-stencil presence.
		extern sObject* g_selected_pobject; extern sObject* g_highlight_pobject;
		extern sObject* g_selected_editor_object; extern int g_selected_editor_objectID;
		extern std::vector<int> g_ObjectHighlightList;
		extern uint64_t g_dbgOutlineCompositeRuns, g_dbgOutlineMaskRuns, g_dbgOutlineSkippedFrames;
		extern int g_iOutlineIdleGate;
		int iGetgrideditselect(void);
		bool bUseEditorOutlineSelection(void);
		float fGetHighlightThickness(void);

		// count scene objects currently carrying a nonzero user stencil ref (the proof
		// the STENCIL stage ran - this is what the mask pass keys on), with per-object
		// detail: ref value, renderable, has-mesh - the write side only matters for
		// objects that actually draw
		int stencilObjs = 0;
		char stencilDetail[640] = {};
		{
			auto& sc = wiScene::GetScene();
			int w = 0;
			wi::ecs::Entity refMesh = wi::ecs::INVALID_ENTITY;
			for (size_t i = 0; i < sc.objects.GetCount(); i++)
			{
				if (sc.objects[i].userStencilRef != 0)
				{
					stencilObjs++;
					wi::ecs::Entity e = sc.objects.GetEntity(i);
					if (sc.objects[i].meshID != wi::ecs::INVALID_ENTITY) refMesh = sc.objects[i].meshID;
					if (w < (int)sizeof(stencilDetail) - 96)
					{
						wi::scene::TransformComponent* tr = sc.transforms.GetComponent(e);
						XMFLOAT3 p = tr ? tr->GetPosition() : XMFLOAT3(0, 0, 0);
						w += _snprintf(stencilDetail + w, sizeof(stencilDetail) - w,
							"[e=%u ref=%u rnd=%d mesh=%d pos=(%.0f,%.0f,%.0f)] ",
							(unsigned)e, (unsigned)sc.objects[i].userStencilRef,
							sc.objects[i].IsRenderable() ? 1 : 0,
							sc.objects[i].meshID != wi::ecs::INVALID_ENTITY ? 1 : 0, p.x, p.y, p.z);
					}
				}
			}
			// TWIN DETECTOR: other renderable objects sharing the ref'd object's MESH -
			// a co-located unref'd twin wins the depth race and the ref'd copy's stencil
			// never lands (depth_fail = KEEP)
			if (refMesh != wi::ecs::INVALID_ENTITY)
			{
				w += _snprintf(stencilDetail + w, sizeof(stencilDetail) - w, "| sameMesh: ");
				for (size_t i = 0; i < sc.objects.GetCount() && w < (int)sizeof(stencilDetail) - 96; i++)
				{
					if (sc.objects[i].meshID == refMesh)
					{
						wi::ecs::Entity e = sc.objects.GetEntity(i);
						wi::scene::TransformComponent* tr = sc.transforms.GetComponent(e);
						XMFLOAT3 p = tr ? tr->GetPosition() : XMFLOAT3(0, 0, 0);
						w += _snprintf(stencilDetail + w, sizeof(stencilDetail) - w,
							"[e=%u ref=%u rnd=%d (%.0f,%.0f,%.0f)] ",
							(unsigned)e, (unsigned)sc.objects[i].userStencilRef,
							sc.objects[i].IsRenderable() ? 1 : 0, p.x, p.y, p.z);
					}
				}
			}
		}
		// draws that actually BOUND a nonzero user stencil ref, per render pass (engine
		// counter in RenderMeshes) - splits "component carries ref" from "draw wrote it"
		char passdetail[320] = {};
		{
			using namespace wi::enums;
			extern std::atomic<uint64_t>* GG_GetUserStencilDrawCounters(void);
			extern std::atomic<uint64_t>* GG_GetTotalDrawCounters(void);
			std::atomic<uint64_t>* ctr = GG_GetUserStencilDrawCounters();
			std::atomic<uint64_t>* tot = GG_GetTotalDrawCounters();
			static const char* names[RENDERPASS_COUNT] = { "main","prepass","prepassDO","envmap","shadow","voxelize","rainblk" };
			int w = 0;
			for (int rp = 0; rp < RENDERPASS_COUNT && w < (int)sizeof(passdetail) - 48; rp++)
				w += _snprintf(passdetail + w, sizeof(passdetail) - w, "%s=%llu/%llu ",
					names[rp], (unsigned long long)ctr[rp].load(std::memory_order_relaxed),
					(unsigned long long)tot[rp].load(std::memory_order_relaxed));
		}
		_snprintf(result, resultSize,
			"OUTLINE: grideditselect=%d (need 5)  outlineEnabled=%d thickness=%.2f\n"
			"FEED: sel_pobject=%d sel_editor=%d (id=%d) highlight=%d listsize=%d\n"
			"STENCIL: objects_with_userStencilRef=%d %s\n"
			"STENCIL_DRAWS (ref/total): %s\n"
			"MASK: runs=%llu   COMPOSITE: runs=%llu   GATE: idle=%d skipped=%llu\n"
			"activeObject=%d gridentity=%d testgame=%d",
			iGetgrideditselect(), bUseEditorOutlineSelection() ? 1 : 0, fGetHighlightThickness(),
			g_selected_pobject ? 1 : 0, g_selected_editor_object ? 1 : 0, g_selected_editor_objectID,
			g_highlight_pobject ? 1 : 0, (int)g_ObjectHighlightList.size(),
			stencilObjs, stencilDetail,
			passdetail,
			(unsigned long long)g_dbgOutlineMaskRuns, (unsigned long long)g_dbgOutlineCompositeRuns,
			g_iOutlineIdleGate, (unsigned long long)g_dbgOutlineSkippedFrames,
			t.widget.activeObject, t.gridentity, bImGuiInTestGame ? 1 : 0);
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_OUTLINE_RT") == 0)
	{
		// DUMP_OUTLINE_RT - save the outline mask target to a PNG. Splits the remaining
		// search space in one artifact: black = the stencil/mask stage produces nothing
		// (stencil bits not landing or the image stencil-test not matching); a white
		// silhouette of the selected object = mask fine, composite/Postprocess_Outline
		// side is the problem.
		extern wiGraphics::Texture rt_Outline, rt_Outline_Red, rt_Outline_Blue;
		if (!rt_Outline.IsValid())
		{
			_snprintf(result, resultSize, "FAIL: rt_Outline not valid");
		}
		else
		{
			bool ok = wi::helper::saveTextureToFile(rt_Outline, "outline_rt.png");
			bool okR = rt_Outline_Red.IsValid() && wi::helper::saveTextureToFile(rt_Outline_Red, "outline_rt_red.png");
			bool okB = rt_Outline_Blue.IsValid() && wi::helper::saveTextureToFile(rt_Outline_Blue, "outline_rt_blue.png");
			_snprintf(result, resultSize, "%s: rt_Outline %ux%u -> outline_rt(.png/_red/_blue) red=%d blue=%d",
				ok ? "OK" : "FAIL", rt_Outline.GetDesc().width, rt_Outline.GetDesc().height, okR ? 1 : 0, okB ? 1 : 0);
		}
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "OUTLINE_MASKTEST") == 0)
	{
		// OUTLINE_MASKTEST <0|1|2> - 1 = draw UNCONDITIONALLY (stencil compare disabled):
		// white dump proves the pass/PSO/RT plumbing. 2 = ENGINE-nibble compare: every
		// drawn object writes engine ref 1, so silhouettes prove stencil WRITES land at
		// all. 0 = normal USER-nibble compare.
		extern int g_iOutlineMaskTest;
		g_iOutlineMaskTest = atoi(arg);
		_snprintf(result, resultSize, "OK: outline mask test=%d", g_iOutlineMaskTest);
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "OUTLINE_GATE") == 0)
	{
		// OUTLINE_GATE <0|1> - 1 (default) skips the outline mask + composite passes on
		// frames where nothing is highlighted; 0 restores the pre-2026-08-06 behaviour of
		// running all six full-screen passes every frame. Exists so the saving can be
		// A/B'd in one session: the gate is worth ~1.0-1.7 ms/frame in the editor, which
		// is 12-17% on the light hub demos.
		extern int g_iOutlineIdleGate;
		g_iOutlineIdleGate = atoi(arg);
		_snprintf(result, resultSize, "OK: outline idle gate=%d (%s)", g_iOutlineIdleGate,
			g_iOutlineIdleGate ? "skip when nothing highlighted" : "always run");
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRIDEDITSELECT") == 0)
	{
		// SET_GRIDEDITSELECT <n> - force the grid edit select mode (5 = entity selection,
		// what clicking an entity tool does). Diagnostic for the outline pipeline, whose
		// feed stage early-outs unless the editor is in mode 5.
		t.grideditselect = atoi(arg);
		_snprintf(result, resultSize, "OK: grideditselect=%d", t.grideditselect);
		result[resultSize - 1] = 0;
	}
	else
	{
		return false;
	}
	return true;
#undef sizeof_result_compat
}

// GGMAX 2.26: mesh census — NAME the empty-level floor instead of deriving it by subtraction.
//
// SWITCHESCAPE_PERF.md §16 attributed the DX12 entity floor by accounting: sum the known
// creators, subtract from the measured total, call the difference "unexplained" (~75 meshes).
// That method can say a remainder EXISTS but never what it IS — and a subtraction-derived
// number is precisely the shape that produced the retracted §22.7 finding. This enumerates
// every live MeshComponent instead, so the remainder has to identify itself.
//
// Two groupings, because floor meshes are mostly UNNAMED:
//   SIGNATURE (verts/indices/subsets) — generator families cluster exactly regardless of naming.
//     A wi::terrain chunk is always chunk_width^2 = 67*67 = 4489 vertices, so every chunk
//     collapses into one row whatever it is called.
//   NAME — the mesh's own NameComponent, else the referencing object's, else its parent's.
// ORPHAN = a MeshComponent that NO ObjectComponent references. Those draw nothing and are the
// leak §16 suspected but could not test ("Wicked's Entity_Remove on an object does not
// necessarily destroy the MeshComponent it referenced").
static bool AutoHarness_CensusCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	// DECAL_BURST <n> — place n decals through the REAL allocation path (decalelement_create),
	// so the 2.27 prewarm+grow pool can be hitch-tested without a player pulling a trigger.
	// ⚠ This exists because the harness provably cannot drive the player (see the 2.14 walk-test
	// note), and "does lazy allocation stall a firefight" is exactly the question that needs a
	// controlled burst rather than an eyeball.
	// Pair with HITCH_RESET before and the HITCH: line of GET_PERF_DATA after; judge the
	// histogram TAIL (worst_ms / over-buckets), never mean FPS — a 1-30 ms stall is invisible
	// in an average, which is the whole reason the hitch instrument exists (engine 1.82).
	if (_stricmp(cmd, "DECAL_BURST") == 0)
	{
		extern void decalelement_create(void);
		extern int g_decalBuilt;
		int n = atoi(arg);
		if (n < 1) n = 25;
		if (n > 500) n = 500;
		const int before = g_decalBuilt;
		// The range gate gets the position vs the gameplay camera; open it wide so the burst
		// cannot be silently dropped by distance and read as "no hitch".
		const float savedRange = (float)g.decalrange;
		g.decalrange = 1000000.0f;
		const int savedDecalId = t.decalid;
		if (t.decalid <= 0) t.decalid = 1;
		for (int i = 0; i < n; i++)
		{
			g.decalx = (float)((i % 10) * 12);
			g.decaly = (float)((i / 10) * 12);
			g.decalz = (float)((i % 7) * 9);
			decalelement_create();
		}
		g.decalrange = savedRange;
		t.decalid = savedDecalId;
		_snprintf(result, resultSize,
			"OK: DECAL_BURST n=%d pool built %d -> %d (max %d) — read HITCH: for the tail",
			n, before, g_decalBuilt, (int)g.decalelementmax);
		result[resultSize - 1] = 0;
		return true;
	}

	// SET_SHADOWEXTRUDE <units> — GGMAX 2.28, the directional shadow-CASTER culling extrusion.
	// This is the fix for "the shadow pops away when its caster leaves the camera frustum": stock
	// Wicked's floor is `min(2000, farPlane) * 0.5` = 1000 units, a metres-scale constant in an
	// inch-scale world, i.e. only 25 m of caster allowance. 0 restores stock exactly.
	// ★ Fully live — CreateDirLightShadowCams reads it every frame — so this is a true A/B lever:
	// stand where a shadow pops, raise it, and watch whether the shadow comes back. That is the
	// empirical confirmation the diagnosis still needs (NIGHT_INVESTIGATIONS_2026-08-12.md C).
	// ⚠ Judge the COST on the shadow pass, not FPS: a wider cull adds casters to the NEAR
	// cascades, which are the expensive ones.
	if (_stricmp(cmd, "SET_SHADOWEXTRUDE") == 0)
	{
		const float v = (float)atof(arg);
		wi::renderer::gg_shadow_caster_extrude = (v < 0.0f) ? 0.0f : v;
		_snprintf(result, resultSize,
			"OK: SET_SHADOWEXTRUDE %.0f units (%.0f m) — 0 = stock 1000u/25m floor",
			wi::renderer::gg_shadow_caster_extrude,
			wi::renderer::gg_shadow_caster_extrude * 0.0254f);
		result[resultSize - 1] = 0;
		return true;
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────
	// GGMAX 2.29: RAGDOLL instruments. Ragdoll death still T-posed after 2.28 restored the
	// writeback CALL, so the next move had to be an instrument that NAMES the dead stage rather
	// than a fifth code-reading guess. DUMP_RAGDOLL walks the whole chain end to end:
	//   CREATE (M-Ragdoll.cpp) -> UPDATE (DBProRagDoll.cpp) -> WRITEBACK (wickedcalls_part2.cpp)
	// Every counter is raw; the VERDICT line is derived and is printed LAST so it can never
	// stand in for the numbers above it.
	// ─────────────────────────────────────────────────────────────────────────────────────────
	if (_stricmp(cmd, "DUMP_RAGDOLL") == 0)
	{
		extern int g_rdCreateCalls, g_rdCreateObj, g_rdCreateFrames, g_rdCreatePelvis,
			g_rdCreateSpine, g_rdCreateExists, g_rdCreateProduced, g_rdCreateLimbs;
		extern char g_rdCreatePrefix[16];
		extern int g_rdUpdCalls, g_rdUpdObj, g_rdUpdBones, g_rdUpdFramesMoved,
			g_rdUpdWriteback, g_rdUpdAnimateFrom, g_rdUpdStillMoving;
		extern int g_rdWBEntered, g_rdWBAnimRef, g_rdWBAnimSet, g_rdWBAnimComp,
			g_rdWBChannel, g_rdWBApplied, g_rdWBSplitTgt, g_ragdollWriteback;
		extern uint64_t g_rdWBLastTarget;
		extern XMFLOAT3 g_rdWBLastTrans;
		extern XMFLOAT4 g_rdWBLastRot;
		extern int g_rdWBClobbered, g_rdWBSurvived, g_rdWBTrackedWritten;

		// ★ THE CAMERA-FREE PROOF. Read the bone the writeback last wrote straight back out of
		// the scene: if `live` still matches `wrote`, the pose survived to the point the armature
		// samples it; if it has drifted back, something (the animation system re-evaluating a
		// still-playing clip) is overwriting the ragdoll every frame. A screenshot cannot answer
		// this — the first 2.29 run photographed an empty room because the body was off-camera.
		char bone[192];
		{
			wi::scene::Scene& sc = wi::scene::GetScene();
			const wi::scene::TransformComponent* bt = (g_rdWBLastTarget != 0)
				? sc.transforms.GetComponent((wi::ecs::Entity)g_rdWBLastTarget) : nullptr;
			if (bt == nullptr)
			{
				_snprintf(bone, sizeof(bone), "RAGDOLL_BONE: target=%llu (no transform - nothing written yet)",
					(unsigned long long)g_rdWBLastTarget);
			}
			else
			{
				const float dt2 =
					fabsf(bt->translation_local.x - g_rdWBLastTrans.x) +
					fabsf(bt->translation_local.y - g_rdWBLastTrans.y) +
					fabsf(bt->translation_local.z - g_rdWBLastTrans.z);
				const float dr2 =
					fabsf(bt->rotation_local.x - g_rdWBLastRot.x) +
					fabsf(bt->rotation_local.y - g_rdWBLastRot.y) +
					fabsf(bt->rotation_local.z - g_rdWBLastRot.z) +
					fabsf(bt->rotation_local.w - g_rdWBLastRot.w);
				_snprintf(bone, sizeof(bone),
					"RAGDOLL_BONE: target=%llu wroteT=(%.2f,%.2f,%.2f) liveT=(%.2f,%.2f,%.2f) dT=%.4f dR=%.4f %s",
					(unsigned long long)g_rdWBLastTarget,
					g_rdWBLastTrans.x, g_rdWBLastTrans.y, g_rdWBLastTrans.z,
					bt->translation_local.x, bt->translation_local.y, bt->translation_local.z,
					dt2, dr2, (dt2 + dr2 < 0.001f) ? "HELD" : "OVERWRITTEN");
			}
			bone[sizeof(bone) - 1] = 0;
		}

		// registered ragdolls, straight out of the game's own table
		char reg[256]; reg[0] = 0; int regCount = 0;
		for (int i = 0; i <= g.RagdollsMax; i++)
		{
			if (t.Ragdolls[i].obj == 0) continue;
			regCount++;
			if (strlen(reg) < sizeof(reg) - 16)
			{
				char one[24]; _snprintf(one, sizeof(one), "%s%d", reg[0] ? "," : "", t.Ragdolls[i].obj);
				strncat(reg, one, sizeof(reg) - strlen(reg) - 1);
			}
		}

		// Derived — the first stage that is starved. Order matters: each stage can only be
		// judged once the one feeding it is known to have run.
		const char* verdict;
		if (g_rdCreateCalls == 0)
			verdict = "CREATE NEVER CALLED - the death path never selected ragdoll (check the death option, not the ragdoll code)";
		else if (g_rdCreateProduced == 0)
			verdict = "CREATE GATE CLOSED - no pelvis/spine limb match, or a ragdoll already existed";
		else if (g_rdCreateLimbs == 0)
			verdict = "NO LIMBS TAGGED - bones exist but no visible limb is bound to them";
		else if (g_rdUpdCalls == 0)
			verdict = "BULLET NOT DRIVING - ragdollManager::Update never reached this ragdoll (asleep or unregistered)";
		else if (g_rdUpdFramesMoved == 0)
			verdict = "BONES CLAIM NO FRAMES - Update runs but flags no frame as bone-driven";
		else if (g_rdWBEntered == 0)
			verdict = "WRITEBACK NEVER CALLED - the loop is running but not reaching the call";
		else if (g_rdWBAnimRef == 0)  verdict = "WRITEBACK BAILS AT pAnimRef";
		else if (g_rdWBAnimSet == 0)  verdict = "WRITEBACK BAILS AT pAnimationSet";
		else if (g_rdWBAnimComp == 0) verdict = "WRITEBACK BAILS AT AnimationComponent lookup";
		else if (g_rdWBChannel == 0)  verdict = "WRITEBACK BAILS AT channel resolve";
		else if (g_rdWBApplied == 0)  verdict = "WRITEBACK IS A NO-OP - reaches the apply point and applies nothing (writeback knob off?)";
		// ⚠ Judge survival by `clobbered` FIRST. The RAGDOLL_BONE read-back can say HELD purely
		// because the harness samples in the same game-logic phase the write happens in;
		// `clobbered` is measured at the next writeback and does not care about phase.
		else if (g_rdWBClobbered > 0 && g_rdWBClobbered >= g_rdWBSurvived)
			verdict = "POSE CLOBBERED BETWEEN WRITEBACKS - the bone is written every frame and something restores it (a still-playing animation clip is the prime suspect)";
		else if (strstr(bone, "OVERWRITTEN") != NULL)
			verdict = "POSE OVERWRITTEN by the time of this dump - see clobbered/survived for which side wins";
		else verdict = "CHAIN COMPLETE - every stage fired and the bone HELD its ragdoll pose; if it still looks wrong the fault is in the VALUES, not the plumbing";

		_snprintf(result, resultSize,
			"RAGDOLL_CREATE: calls=%d obj=%d frames=%d prefix=%s pelvis=%d spine=%d existed=%d produced=%d limbsTagged=%d\n"
			"RAGDOLL_UPDATE: calls=%d obj=%d bones=%d framesMoved=%d animateFrom=%d stillMoving=%d writebackCalls=%d\n"
			"RAGDOLL_WB: entered=%d animRef=%d animSet=%d animComp=%d channel=%d applied=%d splitTarget=%d knob=%d clobbered=%d survived=%d\n"
			"%s\n"
			"RAGDOLL_REG: count=%d objs=[%s] max=%d\n"
			"RAGDOLL_VERDICT (derived): %s",
			g_rdCreateCalls, g_rdCreateObj, g_rdCreateFrames, g_rdCreatePrefix[0] ? g_rdCreatePrefix : "-",
			g_rdCreatePelvis, g_rdCreateSpine, g_rdCreateExists, g_rdCreateProduced, g_rdCreateLimbs,
			g_rdUpdCalls, g_rdUpdObj, g_rdUpdBones, g_rdUpdFramesMoved, g_rdUpdAnimateFrom,
			g_rdUpdStillMoving, g_rdUpdWriteback,
			g_rdWBEntered, g_rdWBAnimRef, g_rdWBAnimSet, g_rdWBAnimComp, g_rdWBChannel,
			g_rdWBApplied, g_rdWBSplitTgt, g_ragdollWriteback, g_rdWBClobbered, g_rdWBSurvived,
			bone,
			regCount, reg, (int)g.RagdollsMax,
			verdict);
		result[resultSize - 1] = 0;

		if (_stricmp(arg, "reset") == 0)
		{
			g_rdCreateCalls = g_rdCreateProduced = g_rdCreateLimbs = 0;
			g_rdCreatePelvis = g_rdCreateSpine = -2; g_rdCreateExists = -1; g_rdCreatePrefix[0] = 0;
			g_rdUpdCalls = g_rdUpdFramesMoved = g_rdUpdWriteback = 0;
			g_rdWBEntered = g_rdWBAnimRef = g_rdWBAnimSet = g_rdWBAnimComp = 0;
			g_rdWBChannel = g_rdWBApplied = g_rdWBSplitTgt = 0;
			g_rdWBClobbered = g_rdWBSurvived = g_rdWBTrackedWritten = 0;
			g_rdWBLastTarget = 0;
		}
		return true;
	}

	// SET_RAGDOLLWRITEBACK <0|1> — 1 (default) applies ragdoll bone poses to the visible mesh
	// through the GGAnimBridge preframe path; 0 restores the 2.28 behaviour where the writeback
	// call runs and does nothing. This is the A/B for "is the writeback what was missing".
	if (_stricmp(cmd, "SET_RAGDOLLWRITEBACK") == 0)
	{
		extern int g_ragdollWriteback;
		g_ragdollWriteback = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize, "OK: SET_RAGDOLLWRITEBACK %d (0 = 2.28 no-op behaviour)", g_ragdollWriteback);
		result[resultSize - 1] = 0;
		return true;
	}

	// RAGDOLL_TEST [entityIndex] — ragdollify a character WITHOUT needing the player to shoot it.
	// ⚠ The harness provably cannot drive the player (2.14 walk-test note), and the death path
	// runs deep inside AI state; this reproduces exactly what G-Entity_part1.cpp:722-732 does at
	// the moment it chooses ragdoll. With no argument it picks the first live character entity.
	if (_stricmp(cmd, "RAGDOLL_TEST") == 0)
	{
		int want = atoi(arg);
		int picked = 0;
		if (want > 0 && want <= g.entityelementmax && t.entityelement[want].active > 0)
		{
			picked = want;
		}
		else
		{
			for (int e = 1; e <= g.entityelementmax; e++)
			{
				if (t.entityelement[e].active == 0) continue;
				if (t.entityelement[e].obj <= 0) continue;
					// Skip anyone already ragdolled, so consecutive calls walk to the NEXT character.
				// Without this a second RAGDOLL_TEST re-picks the same body, BPhys_RagdollExist
				// closes the gate, and the second arm of an A/B measures nothing — which is
				// exactly what the first 2.29 run did (existed=1, every counter frozen).
				if (t.entityelement[e].ragdollified == 1) continue;
				// character-ness lives on the PROFILE (bank) record, not the element — the same
				// t.entityprofile[bankindex] lookup G-Entity uses everywhere.
				const int bank = t.entityelement[e].bankindex;
				if (bank <= 0 || t.entityprofile[bank].ischaracter != 1) continue;
				if (getlimbbyname(t.entityelement[e].obj, (char*)"Bip01_Pelvis") < 0) continue;
				picked = e; break;
			}
		}
		if (picked == 0)
		{
			_snprintf(result, resultSize, "FAIL: RAGDOLL_TEST found no live character entity with a Bip01_Pelvis limb");
			result[resultSize - 1] = 0;
			return true;
		}
		const int obj = t.entityelement[picked].obj;
		t.ttte = picked;
		ragdoll_setcollisionmask(t.entityelement[picked].eleprof.colondeath);
		t.tphye = picked;
		t.tphyobj = obj;
		// ⚠ `t.tobj` TOO, and it is not optional. ragdoll_create's pose-extraction loop reads
		// limb data out of t.tphyobj but writes it back with RotateLimbQuat(t.tobj, ...)
		// (M-Ragdoll.cpp:195) — an ambient global the real death paths happen to have already
		// set to the same entity (G-Entity_part1.cpp:30, G-Entity_part2.cpp:79, both checked).
		// Omitting it here crashed MAX outright in AnglesFromMatrix on the first 2.29 run:
		// RotateLimbQuat was handed whatever object was last selected.
		// ★ Not a live bug — but it IS a landmine for any future caller of ragdoll_create.
		t.tobj = obj;
		ragdoll_create();
		t.entityelement[picked].ragdollified = 1;
		t.entityelement[picked].ragdollifiedforcex_f = 0.8f;
		t.entityelement[picked].ragdollifiedforcey_f = 0.0f;
		t.entityelement[picked].ragdollifiedforcez_f = 0.8f;
		t.entityelement[picked].ragdollifiedforcevalue_f = 0.0f;
		t.entityelement[picked].ragdollifiedforcelimb = 0;
		_snprintf(result, resultSize,
			"OK: RAGDOLL_TEST entity=%d obj=%d — now read DUMP_RAGDOLL (and look at the character)",
			picked, obj);
		result[resultSize - 1] = 0;
		return true;
	}

	// SET_OCCLUSION <0|1> — GGMAX 2.30. GPU occlusion culling on/off, live.
	// ⚠ Writes BOTH the engine flag AND t.visuals.bOcclusionCulling, and it must. The Graphics
	// panel RECONCILES the engine to the visuals value whenever it is open
	// (M-GridEditB_part24.cpp:134) and the visuals-apply path pushes it again
	// (M-GridEditB_part3.cpp:1815) — setting only the engine flag would be silently reverted,
	// which is exactly the "prove the knob REACHES the thing" trap that wasted a session on
	// SET_TREES. The readback prints both so a divergence is visible immediately.
	if (_stricmp(cmd, "SET_OCCLUSION") == 0)
	{
		const bool on = (atoi(arg) != 0);
		wi::renderer::SetOcclusionCullingEnabled(on);
		t.visuals.bOcclusionCulling = on;
		_snprintf(result, resultSize,
			"OK: SET_OCCLUSION %d — engine=%d visuals=%d (both written; they must agree)",
			on ? 1 : 0,
			wi::renderer::GetOcclusionCullingEnabled() ? 1 : 0,
			t.visuals.bOcclusionCulling ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}

	// ─────────────────────────────────────────────────────────────────────────────────────────
	// DUMP_FIRE — GGMAX 2.42. Read the trigger tracer armed by FIRE_WEAPON and name the stage that
	// stopped the shot. Each verdict branch is a DIFFERENT fault; from outside they all look
	// identical ("the gun did not fire"), which is exactly why two code-reading guesses failed.
	if (_stricmp(cmd, "DUMP_FIRE") == 0)
	{
		extern void GGFireTraceDump(char* result, int resultSize);
		GGFireTraceDump(result, resultSize);
		return true;
	}

	// VIDEO_TEST — GGMAX 2.50. Drive the tutorial-video pipeline headlessly: load an .mp4 into a
	// high anim slot and play it. This exercises the exact chain a thumbnail click uses
	// (LoadAnimation -> PlayAnimation -> WMF decode -> YUY2 convert -> DX12 bridge upload);
	// VIDEO_STATUS then reports whether decoded frames are reaching a GPU texture.
	if (_stricmp(cmd, "VIDEO_TEST") == 0)
	{
		if (arg == nullptr || arg[0] == 0)
		{
			_snprintf(result, resultSize, "ERROR: VIDEO_TEST needs a video path (relative to Files/)");
			result[resultSize - 1] = 0;
			return true;
		}
		// The 5-arg overload from Dark Basic Public Shared/Include/CAnimation.h:73 is the one the
		// tutorial widgets use (M-GridEditB_part4.cpp:352); the 2-arg variant is not linked.
		extern bool LoadAnimation(LPSTR pFilename, int iIndex, int precacheframes, int videodelayedload, int iSilentMode);
		extern void PlayAnimation(int animindex, int x1, int y1, int x2, int y2);
		extern void gg_videotrace(const char* msg);
		gg_videotrace("VIDEO_TEST: calling LoadAnimation");
		// slot 30: ANIMATIONMAX is only 33 (CAnimation_part0.cpp:44) - slot 60 was silently out of range
		bool bLoaded = LoadAnimation((LPSTR)arg, 30, 0, 0, 1);   // no delayed load: exercise the full chain now
		gg_videotrace(bLoaded ? "VIDEO_TEST: LoadAnimation OK, calling PlayAnimation" : "VIDEO_TEST: LoadAnimation FAILED - NOT playing (a dead slot raises a MODAL RunTimeError)");
		if (bLoaded)
		{
			PlayAnimation(30, 0, 0, 320, 180);
			gg_videotrace("VIDEO_TEST: PlayAnimation returned");
		}
		_snprintf(result, resultSize, "OK: VIDEO_TEST load=%d playing '%s' in anim slot 30 — poll VIDEO_STATUS", bLoaded ? 1 : 0, arg);
		result[resultSize - 1] = 0;
		return true;
	}

	// VIDEO_STATUS — GGMAX 2.50. The three separable stages: pMediaClip (decode session up),
	// view handle (a decoded frame reached a GPU texture), percent (the clock advances).
	if (_stricmp(cmd, "VIDEO_STATUS") == 0)
	{
		extern ID3D11ShaderResourceView* GetAnimPointerView(int AnimIndex);
		extern float GetAnimPercentDone(int AnimIndex);
		extern int GetVideoPlaying(void);
		void* view = (void*)GetAnimPointerView(30);
		_snprintf(result, resultSize, "OK: VIDEO_STATUS view=%p percent=%.1f playing=%d -> %s",
			view, GetAnimPercentDone(30), GetVideoPlaying(),
			view ? "FRAMES ARE REACHING THE GPU" : "no frame yet (or load failed)");
		result[resultSize - 1] = 0;
		return true;
	}

	// TERRAINGEN_GENERATE — GGMAX 2.49. Press the Terrain Generator's "Generate Terrain and Open
	// the Level Editor" button headlessly: sets the same two states the button's click block ends
	// with (M-TerrainNew_part5.cpp:3504-3505, iQuitProceduralLevel countdown + tree reinit).
	// ⚠ Deliberately SKIPS the button's entity re-banking preamble — this command exists to drive
	// the crash-fix verification (the crash was in the countdown's screenshot block), not to be a
	// full substitute for the button.
	if (_stricmp(cmd, "TERRAINGEN_GENERATE") == 0)
	{
		extern bool bProceduralLevel;
		extern int iQuitProceduralLevel;
		extern bool bTreeGlobalInit;
		if (!bProceduralLevel)
		{
			_snprintf(result, resultSize, "ERROR: TERRAINGEN_GENERATE — not in the Terrain Generator (bProceduralLevel false)");
			result[resultSize - 1] = 0;
			return true;
		}
		iQuitProceduralLevel = 5;
		bTreeGlobalInit = false;
		_snprintf(result, resultSize, "OK: TERRAINGEN_GENERATE — countdown armed (5 frames to the screenshot block)");
		result[resultSize - 1] = 0;
		return true;
	}

	// TERRAINGEN_STATE — GGMAX 2.49. Report the generator's state for gating scripts.
	if (_stricmp(cmd, "TERRAINGEN_STATE") == 0)
	{
		extern bool bProceduralLevel;
		extern int iQuitProceduralLevel;
		extern bool bTriggerTerrainSaveAsWindow;
		_snprintf(result, resultSize, "OK: TERRAINGEN_STATE procedural=%d quitCountdown=%d saveAsOpen=%d",
			bProceduralLevel ? 1 : 0, iQuitProceduralLevel, bTriggerTerrainSaveAsWindow ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}

	// TERRAINGEN_BACK — GGMAX 2.54. Exit the Terrain Generator back to the storyboard: the
	// same single variable the back arrow's confirmed storyboard route sets
	// (M-TerrainNew_part5.cpp TOOL_GOBACK handler), minus its askBoxCancel — that is a MODAL
	// MessageBox a headless run can never answer (the modal-legacy-path rule). Exists to
	// drive the 2.54 enter→exit→re-enter wipe verification.
	if (_stricmp(cmd, "TERRAINGEN_BACK") == 0)
	{
		extern bool bProceduralLevel;
		if (!bProceduralLevel)
		{
			_snprintf(result, resultSize, "ERROR: TERRAINGEN_BACK — not in the Terrain Generator (bProceduralLevel false)");
			result[resultSize - 1] = 0;
			return true;
		}
		bProceduralLevel = false;
		_snprintf(result, resultSize, "OK: TERRAINGEN_BACK — bProceduralLevel=false (returning to storyboard)");
		result[resultSize - 1] = 0;
		return true;
	}

	// SET_TERRAIN_GEN <n> — GGMAX 2.54a DIAGNOSTIC. Set the terrain chunk ring radius
	// (Terrain::generation) live: ring span = (2n+1)^2 chunks, reach = n x 134 m from the
	// centre. Built to MEASURE the cost of covering a 5 km editable area (gen 19 = 1521
	// chunks) against the shipping gen 12 (625). The ring refills/shrinks progressively on
	// the next Generation_Updates; removal threshold adapts (generation + 2 + margin).
	// ⚠ Diagnostic only — never ship a session with this changed; POLYS/VRAM comparisons
	// against other runs are invalid while it differs from the default.
	if (_stricmp(cmd, "SET_TERRAIN_GEN") == 0)
	{
		int n = atoi(arg);
		if (n < 4 || n > 24)
		{
			_snprintf(result, resultSize, "ERROR: SET_TERRAIN_GEN needs 4..24 (got '%s'; default 12, 5km coverage = 19)", arg);
			result[resultSize - 1] = 0;
			return true;
		}
		wi::scene::Scene& ggSc = wi::scene::GetScene();
		if (ggSc.terrains.GetCount() == 0)
		{
			_snprintf(result, resultSize, "ERROR: SET_TERRAIN_GEN — no terrain in scene");
			result[resultSize - 1] = 0;
			return true;
		}
		ggSc.terrains[0].generation = n;
		_snprintf(result, resultSize, "OK: SET_TERRAIN_GEN %d — ring target %dx%d = %d chunks (watch TERRAIN_RING chunks refill)",
			n, 2 * n + 1, 2 * n + 1, (2 * n + 1) * (2 * n + 1));
		result[resultSize - 1] = 0;
		return true;
	}

	// MERGE_PROF — GGMAX 2.60. MergeFastInternal cost attribution (the terrain generator's
	// output merges through it inside Scene-S1 — the ~490ms mega-frame suspect). Cumulative
	// since launch; per-manager totals name WHICH ComponentManager::Merge carries the cost.
	if (_stricmp(cmd, "MERGE_PROF") == 0)
	{
		using namespace wi::scene;
		int written2 = _snprintf(result, resultSize,
			"OK: MERGE_PROF calls=%llu total=%.1fms max=%.1fms |",
			(unsigned long long)gg_mergeprof_calls,
			gg_mergeprof_total_us / 1000.0,
			gg_mergeprof_max_us / 1000.0);
		for (int i = 0; i < gg_mergeprof_entry_count && written2 < resultSize - 80; ++i)
		{
			if (gg_mergeprof_entries[i].us < 1000) continue; // only managers with >=1ms cumulative
			written2 += _snprintf(result + written2, resultSize - written2, " %s=%.1fms",
				gg_mergeprof_entries[i].name, gg_mergeprof_entries[i].us / 1000.0);
		}
		result[resultSize - 1] = 0;
		return true;
	}

	// TERRAIN_GENPROF [RESET] — GGMAX 2.58. Per-phase chunk-generation cost breakdown
	// (cumulative engine accumulators; averages are per generated chunk). Answers "what
	// takes the most time when generating a terrain chunk". ⚠ renderdata is timed inside
	// its async job and OVERLAPS the physics phase — rank consumers by phase, but only
	// `total` is wall time per chunk on the generator thread.
	if (_stricmp(cmd, "TERRAIN_GENPROF") == 0)
	{
		using namespace wi::terrain;
		if (arg[0] != 0 && _stricmp(arg, "RESET") == 0)
		{
			gg_genprof_heights_us = 0; gg_genprof_vertex_us = 0; gg_genprof_renderdata_us = 0;
			gg_genprof_grass_us = 0; gg_genprof_blendcb_us = 0; gg_genprof_regiontex_us = 0; gg_genprof_bvh_us = 0;
			gg_genprof_bvh_events = 0; // GGMAX 2.61: was missing — N read cumulative-since-launch after a reset (cost a false alarm)
			gg_genprof_physics_us = 0; gg_genprof_total_us = 0; gg_genprof_chunks = 0;
			_snprintf(result, resultSize, "OK: TERRAIN_GENPROF reset");
			result[resultSize - 1] = 0;
			return true;
		}
		const uint64_t n = gg_genprof_chunks.load();
		if (n == 0)
		{
			_snprintf(result, resultSize, "OK: TERRAIN_GENPROF chunks=0 (nothing generated since launch/reset)");
			result[resultSize - 1] = 0;
			return true;
		}
		const double d = (double)n * 1000.0; // us -> avg ms per chunk
		_snprintf(result, resultSize,
			"OK: TERRAIN_GENPROF chunks=%llu avg ms/chunk: total=%.2f | heights=%.2f vertex=%.2f renderdata=%.2f(async) bvh=%.2f(async,N=%llu) grass=%.2f blendcb=%.2f regiontex=%.2f physics=%.2f",
			(unsigned long long)n,
			gg_genprof_total_us.load() / d,
			gg_genprof_heights_us.load() / d,
			gg_genprof_vertex_us.load() / d,
			gg_genprof_renderdata_us.load() / d,
			gg_genprof_bvh_us.load() / d,
			(unsigned long long)gg_genprof_bvh_events.load(),
			gg_genprof_grass_us.load() / d,
			gg_genprof_blendcb_us.load() / d,
			gg_genprof_regiontex_us.load() / d,
			gg_genprof_physics_us.load() / d);
		result[resultSize - 1] = 0;
		return true;
	}

	// VT_PROF [RESET] — GGMAX 2.61. Main-thread VT/texture allocation attribution (the generator
	// fill wall). updcpu = whole UpdateVirtualTexturesCPU body per call (wait0 = its entry wait on
	// last frame's async VT job); vtinit = VirtualTexture::init (INCLUDES nested resinit); resinit
	// = Residency::init pool misses (~10 device creates, 4 blocking uploads each); regionmain =
	// main-thread "last minute" CreateChunkRegionTexture actually creating; copywait = device-wide
	// CPU-blocking copy-queue waits (EVERY with-initdata create pays one, any thread).
	if (_stricmp(cmd, "VT_PROF") == 0)
	{
		using namespace wi::terrain;
		if (arg[0] != 0 && _stricmp(arg, "RESET") == 0)
		{
			gg_vtprof_updatecpu_us = 0; gg_vtprof_updatecpu_calls = 0; gg_vtprof_updatecpu_max_us = 0;
			gg_vtprof_wait0_us = 0; gg_vtprof_vtinit_us = 0; gg_vtprof_vtinit_events = 0;
			gg_vtprof_resinit_us = 0; gg_vtprof_resinit_events = 0;
			gg_vtprof_regionmain_us = 0; gg_vtprof_regionmain_events = 0;
			gg_dbg_copywait_us = 0; gg_dbg_copywait_events = 0;
			_snprintf(result, resultSize, "OK: VT_PROF reset");
			result[resultSize - 1] = 0;
			return true;
		}
		_snprintf(result, resultSize,
			"OK: VT_PROF updcpu calls=%llu tot=%.1fms max=%.1fms wait0=%.1fms | vtinit N=%llu %.1fms | resinit N=%llu %.1fms | regionmain N=%llu %.1fms | copywait N=%llu %.1fms",
			(unsigned long long)gg_vtprof_updatecpu_calls.load(),
			gg_vtprof_updatecpu_us.load() / 1000.0,
			gg_vtprof_updatecpu_max_us.load() / 1000.0,
			gg_vtprof_wait0_us.load() / 1000.0,
			(unsigned long long)gg_vtprof_vtinit_events.load(),
			gg_vtprof_vtinit_us.load() / 1000.0,
			(unsigned long long)gg_vtprof_resinit_events.load(),
			gg_vtprof_resinit_us.load() / 1000.0,
			(unsigned long long)gg_vtprof_regionmain_events.load(),
			gg_vtprof_regionmain_us.load() / 1000.0,
			(unsigned long long)::gg_dbg_copywait_events.load(),
			::gg_dbg_copywait_us.load() / 1000.0);
		result[resultSize - 1] = 0;
		return true;
	}

	// TERRAINGEN_BIOME [1-7|plains|desert|forest|snow|canyon|mountain|rainforest] — GGMAX 2.62.
	// No arg: report the generator's biome state (selection mirror + every param a biome click
	// mutates that the terrain derives from). With arg: inject a click through the SHIPPED
	// button branch (M-TerrainNew_part5 iRandomThemeChoice lane) — only works while the
	// Terrain Generator panel is drawing. Built for the "DESERT does nothing on DX12" hunt.
	if (_stricmp(cmd, "TERRAINGEN_BIOME") == 0)
	{
		using namespace GGTerrain;
		if (arg[0] != 0)
		{
			int choice = 0;
			if (arg[0] >= '0' && arg[0] <= '9') choice = atoi(arg);
			else if (_stricmp(arg, "plains") == 0) choice = 1;
			else if (_stricmp(arg, "desert") == 0) choice = 2;
			else if (_stricmp(arg, "forest") == 0) choice = 3;
			else if (_stricmp(arg, "snow") == 0) choice = 4;
			else if (_stricmp(arg, "canyon") == 0) choice = 5;
			else if (_stricmp(arg, "mountain") == 0) choice = 6;
			else if (_stricmp(arg, "rainforest") == 0) choice = 7;
			if (choice < 1 || choice > 7)
			{
				_snprintf(result, resultSize, "ERROR: TERRAINGEN_BIOME wants 1-7 or plains/desert/forest/snow/canyon/mountain/rainforest (got '%s')", arg);
			}
			else
			{
				g_ggHarnessBiomeClick = choice;
				_snprintf(result, resultSize, "OK: TERRAINGEN_BIOME click %d queued (consumed on the next generator panel frame)", choice);
			}
			result[resultSize - 1] = 0;
			return true;
		}
		int wtb = _snprintf(result, resultSize,
			"OK: TERRAINGEN_BIOME sel=%d ptype=%d seed=%u amp=%.2f offx=%.1f offz=%.1f editsize=%.0f slopemat0=%d treebits=0x%llx | chain cpRuns=%u cpResets=%u notifies=%u matNotifies=%u wipes=%u",
			g_ggBiomeSelectedMirror,
			ggterrain_extra_params.iProceduralTerrainType,
			(unsigned int)ggterrain_global_params.seed,
			ggterrain_global_params.fractal_initial_amplitude,
			ggterrain_global_params.offset_x,
			ggterrain_global_params.offset_z,
			GGTerrain::ggterrain_global_render_params2.editable_size,
			(int)(GGTerrain::ggterrain_global_render_params.slopeMatIndex[0] & 0xff),
			(unsigned long long)GGTrees::ggtrees_global_params.paint_tree_bitfield,   // GGMAX 3.102: all 38 bits
			GGTerrain::gg_dbg_checkparams_runs,
			GGTerrain::gg_dbg_checkparams_resets,
			GGTerrain::gg_dbg_params_notifies,
			GGTerrain::gg_dbg_material_notifies,
			GGTerrain::gg_dbg_params_wipes);
		// GGMAX 2.65: the marker rests wherever the last drag dropped it (release no
		// longer recentres), so its world position is now state worth reporting — the
		// Generate-button offset fold is offx/offz += MetersToOffset(UnitsToMeters(-marker)).
		if (ObjectExist(17998) && wtb > 0 && wtb < resultSize - 96)
		{
			sObject* pTGM = GetObjectData(17998);
			// GGMAX 2.68c: markerScr = the marker's PROJECTED screen position (backbuffer
			// coords, published by the generator's footprint block) — real-cursor probes
			// must grab THERE (+the title-bar Y offset), never at hardcoded coordinates:
			// the screen Y depends on the ground height at the marker, which varies by seed.
			extern float g_ggDbgMarkerScreenX, g_ggDbgMarkerScreenY;
			if (pTGM)
				wtb += _snprintf(result + wtb, resultSize - wtb, " | marker=(%.0f,%.0f) markerScr=(%.0f,%.0f)",
					pTGM->position.vecPosition.x, pTGM->position.vecPosition.z,
					g_ggDbgMarkerScreenX, g_ggDbgMarkerScreenY);
		}
		// GGMAX 2.67: wicked grass state — names the dead stage when "Vegetation looks wrong"
		// (drawEn = legacy flag the checkbox writes; wOn = the real wicked gate; hairs/vis =
		// hair systems present/unhidden; strands = total strand count).
		if (wtb > 0 && wtb < resultSize - 96)
		{
			int gOn = 0, gN = 0, gVis = 0; unsigned long long gStrands = 0;
			GGTerrain::GGTerrainWicked_GetGrassDebug(&gOn, &gN, &gVis, &gStrands);
			_snprintf(result + wtb, resultSize - wtb, " | grass drawEn=%d wOn=%d hairs=%d vis=%d strands=%llu",
				GGGrass::gggrass_global_params.draw_enabled, gOn, gN, gVis, gStrands);
		}
		result[resultSize - 1] = 0;
		return true;
	}

	// TERRAINGEN_MATS — GGMAX 2.63c diagnostic. What each wicked terrain material slot points
	// at RIGHT NOW (matN folder from its basecolor texture name) + a histogram of the GG
	// painted-material map. Built for the stale-centre biome repro: separates STALE TEXTURES
	// (a slot still resolves to the old biome's matN) from STALE PAINT (pMaterialMap bytes
	// left over from the old biome — painted indices are ABSOLUTE catalogue slots, so they
	// resurrect the old look regardless of the new biome's slot materials).
	if (_stricmp(cmd, "TERRAINGEN_MATS") == 0)
	{
		using namespace GGTerrain;
		auto& scn = wi::scene::GetScene();
		int w4 = _snprintf(result, resultSize, "OK: TERRAINGEN_MATS slots:");
		if (scn.terrains.GetCount() > 0)
		{
			wi::terrain::Terrain& terr = scn.terrains[0];
			for (size_t s = 0; s < terr.materialEntities.size() && w4 > 0 && w4 < resultSize - 64; ++s)
			{
				wi::scene::MaterialComponent* m = scn.materials.GetComponent(terr.materialEntities[s]);
				char tail[64] = "-";
				if (m != nullptr && !m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name.empty())
				{
					const std::string& n = m->textures[wi::scene::MaterialComponent::BASECOLORMAP].name;
					size_t p = n.find("terraintextures/");
					std::string t = (p != std::string::npos) ? n.substr(p + 16) : n;
					size_t q = t.find("/Color");
					if (q != std::string::npos) t = t.substr(0, q);
					strncpy(tail, t.c_str(), sizeof(tail) - 1); tail[sizeof(tail) - 1] = 0;
				}
				w4 += _snprintf(result + w4, resultSize - w4, " s%d=%s", (int)s, tail);
			}
		}
		else
		{
			w4 += _snprintf(result + w4, resultSize - w4, " (no wicked terrain)");
		}
		{
			// painted-material map histogram via the public snapshot API (the raw pointer +
			// size macro live behind the implementation side of GGTerrain.h)
			const uint32_t total = GGTerrain_GetPaintDataSize();
			uint8_t* buf = (total > 0) ? new uint8_t[total] : nullptr;
			if (buf != nullptr && GGTerrain_GetPaintData(buf) == 1 && w4 > 0 && w4 < resultSize - 128)
			{
				unsigned int counts[256]; memset(counts, 0, sizeof(counts));
				unsigned int nz = 0;
				for (uint32_t i = 0; i < total; ++i)
				{
					if (buf[i] > 0) { nz++; counts[buf[i]]++; }
				}
				w4 += _snprintf(result + w4, resultSize - w4, " | paintmap nz=%u/%u top:", nz, total);
				for (int k = 0; k < 3 && w4 > 0 && w4 < resultSize - 32; ++k)
				{
					unsigned int best = 0, bi = 0;
					for (int v = 1; v < 256; ++v) { if (counts[v] > best) { best = counts[v]; bi = (unsigned int)v; } }
					if (best == 0) break;
					w4 += _snprintf(result + w4, resultSize - w4, " mat%u=%u", bi, best);
					counts[bi] = 0;
				}
			}
			else if (w4 > 0 && w4 < resultSize - 32)
			{
				w4 += _snprintf(result + w4, resultSize - w4, " | paintmap unavailable");
			}
			delete[] buf;
		}
		// GGMAX 2.63c: centre-chunk blend forensics — WHICH blendmap layer carries the land
		// weight (stale-mapping bakes park weight in a layer with no backing material).
		if (scn.terrains.GetCount() > 0 && w4 > 0 && w4 < resultSize - 160)
		{
			wi::terrain::Terrain& terr = scn.terrains[0];
			wi::terrain::Chunk cc; cc.x = 0; cc.z = 0;
			auto itc = terr.chunks.find(cc);
			if (itc != terr.chunks.end())
			{
				wi::terrain::ChunkData& cd = itc->second;
				w4 += _snprintf(result + w4, resultSize - w4, " | chunk(0,0) born=%d layers=%d w:",
					cd.gg_blendmap_generated ? 1 : 0, (int)cd.blendmap_layers.size());
				for (size_t li = 0; li < cd.blendmap_layers.size() && li < 8 && w4 > 0 && w4 < resultSize - 24; ++li)
				{
					uint64_t sum = 0;
					for (uint8_t px : cd.blendmap_layers[li].pixels) sum += px;
					w4 += _snprintf(result + w4, resultSize - w4, " L%d=%llu", (int)li, (unsigned long long)(sum / 1000));
				}
			}
			else
			{
				w4 += _snprintf(result + w4, resultSize - w4, " | chunk(0,0) MISSING");
			}
		}
		result[resultSize - 1] = 0;
		return true;
	}

	// IMGUI_PROBE — GGMAX 2.65 diagnostic. What ImGui itself sees RIGHT NOW: io.MousePos,
	// button state, hovered/nav window names, display + main-viewport rects, and (when it
	// exists) the Save-As modal's rect. Built because a real-cursor click that lands fine
	// on panel buttons produced NO hover on the Save New Level As modal — this names the
	// coordinate or routing mismatch instead of guessing at it.
	if (_stricmp(cmd, "IMGUI_PROBE") == 0)
	{
		ImGuiContext* gctx = GImGui;
		if (!gctx)
		{
			_snprintf(result, resultSize, "ERROR: no ImGui context");
			result[resultSize - 1] = 0;
			return true;
		}
		ImGuiIO& io = ImGui::GetIO();
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGuiWindow* saveas = ImGui::FindWindowByName("Save New Level As##Storyboard");
		int w6 = _snprintf(result, resultSize,
			"OK: IMGUI_PROBE mouse=(%.0f,%.0f) down=%d disp=(%.0f,%.0f) vpPos=(%.0f,%.0f) vpSize=(%.0f,%.0f) hovered='%s' nav='%s'",
			io.MousePos.x, io.MousePos.y, io.MouseDown[0] ? 1 : 0,
			io.DisplaySize.x, io.DisplaySize.y,
			vp ? vp->Pos.x : -1.0f, vp ? vp->Pos.y : -1.0f,
			vp ? vp->Size.x : -1.0f, vp ? vp->Size.y : -1.0f,
			gctx->HoveredWindow ? gctx->HoveredWindow->Name : "none",
			gctx->NavWindow ? gctx->NavWindow->Name : "none");
		if (saveas && w6 > 0 && w6 < resultSize - 128)
			_snprintf(result + w6, resultSize - w6, " | saveas pos=(%.0f,%.0f) size=(%.0f,%.0f) active=%d",
				saveas->Pos.x, saveas->Pos.y, saveas->Size.x, saveas->Size.y, saveas->Active ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}

	// PICK_AT <x> <y> [layerMask] — GGMAX 2.64 diagnostic. Runs the scene pick at the given
	// client coordinates (same space as SCREENSHOT pixels) and reports hit/position/entity +
	// whether the entity maps to the Terrain Generator marker object (17998). Built for the
	// marker-hover hunt: names whether a pick over the marker returns the marker's entity,
	// a terrain chunk, or nothing.
	if (_stricmp(cmd, "PICK_AT") == 0)
	{
		float px = 0, py = 0; int pmask = GGRENDERLAYERS_CURSOROBJECT;
		if (sscanf(arg, "%f %f %d", &px, &py, &pmask) < 2)
		{
			_snprintf(result, resultSize, "ERROR: PICK_AT wants <x> <y> [layerMask]");
			result[resultSize - 1] = 0;
			return true;
		}
		float ox = 0, oy = 0, oz = 0;
		uint64_t ent = 0;
		// GetPick2 early-outs when ImGui owns the mouse — the harness runs at a frame point
		// where that flag is up (the generator's own hover path clears it during its draw).
		// Bypass for the measurement, restore after.
		extern bool bImGuiGotFocus;
		const bool savedFocus = bImGuiGotFocus;
		bImGuiGotFocus = false;
		bool hit = WickedCall_GetPick2(px, py, &ox, &oy, &oz, NULL, NULL, NULL, &ent, pmask);
		bImGuiGotFocus = savedFocus;
		int w5 = _snprintf(result, resultSize,
			"OK: PICK_AT (%.0f,%.0f) mask=%d hit=%d pos=(%.1f,%.1f,%.1f) entity=%llu isMarker17998=%d",
			px, py, pmask, hit ? 1 : 0, ox, oy, oz,
			(unsigned long long)ent,
			WickedCall_IsEntityOfObject(ent, 17998) ? 1 : 0);
		// also dump the marker object's own frame entities + visibility so a miss can be
		// classified: mapping bug (hit entity IS in this list) vs unpickable marker (it isn't)
		if (ObjectExist(17998) && w5 > 0 && w5 < resultSize - 96)
		{
			sObject* pM = GetObjectData(17998);
			w5 += _snprintf(result + w5, resultSize - w5, " | marker frames=%d ents:", pM ? pM->iFrameCount : -1);
			for (int f = 0; pM && f < pM->iFrameCount && f < 4 && w5 > 0 && w5 < resultSize - 32; f++)
				w5 += _snprintf(result + w5, resultSize - w5, " %llu",
					(unsigned long long)(pM->ppFrameList[f] ? pM->ppFrameList[f]->wickedobjindex : 0));
			if (pM) w5 += _snprintf(result + w5, resultSize - w5, " vis=%d", pM->bVisible ? 1 : 0);
		}
		result[resultSize - 1] = 0;
		return true;
	}

	// ZOOM_FIRE — GGMAX 2.48. Hold right mouse to zoom, fire mid-hold, keep zoom held after.
	// Everything goes through the SHIPPED input path (see M-Physics_part1.cpp consumer), so the
	// shot is fired in a genuine zoomed state — built to reproduce "zoomed firing does no damage".
	// Args: [zoomframes] [fireat] — defaults 120 and 60 (fire after 60 frames of zoom, hold
	// remains ~60 more so the shot lands fully zoomed).
	if (_stricmp(cmd, "ZOOM_FIRE") == 0)
	{
		const char* st = AutoHarness_GetAppState();
		if (strcmp(st, "game") != 0)
		{
			_snprintf(result, resultSize, "ERROR: ZOOM_FIRE only works in test game (state: %s)", st);
			result[resultSize - 1] = 0;
			return true;
		}
		int zoomFrames = 120, fireAt = 60;
		if (arg != nullptr && arg[0] != 0)
		{
			int a = 0, b = 0;
			if (sscanf(arg, "%d %d", &a, &b) >= 1 && a > 0 && a <= 600) zoomFrames = a;
			if (b > 0 && b < zoomFrames) fireAt = b;
		}
		if (fireAt >= zoomFrames) fireAt = zoomFrames / 2;
		extern int g_ggZoomHoldFrames, g_ggZoomFireAtFrame;
		g_ggZoomHoldFrames = zoomFrames;
		g_ggZoomFireAtFrame = fireAt;
		_snprintf(result, resultSize, "OK: ZOOM_FIRE — RMB held %d frames, LMB fires at frame %d (shipped Lua input path)",
			zoomFrames, fireAt);
		result[resultSize - 1] = 0;
		return true;
	}

	// DUMP_SHOT — GGMAX 2.48. Read the bullet-ray tracer (one row per fired ray, sampled inside
	// entity_hasbulletrayhit) and print a verdict: hit entity / terrain / clean miss / SWALLOWED
	// by an out-of-entity-range object (blockedBy names it; == gunobj means the weapon mesh ate
	// the shot via the stale aabb.layerMask). Companion to DUMP_FIRE, one stage further down.
	if (_stricmp(cmd, "DUMP_SHOT") == 0)
	{
		extern void GGShotTraceDump(char* result, int resultSize);
		GGShotTraceDump(result, resultSize);
		return true;
	}

	// FIRE_WEAPON — GGMAX 2.41. Pull the trigger exactly as a left mouse button press would.
	//
	// Uses the engine's OWN script-control hook rather than faking input: M-Physics_part1.cpp:218
	// switches on g.playeraction and `case 1` sets t.player[1].state.firingmode = 1, which is the
	// same state a real LMB produces. g.playeraction self-clears at :284 every frame, so one
	// command is one trigger pull — issue it again for another shot.
	// Built to study the barrel explosion without a human holding the mouse.
	if (_stricmp(cmd, "FIRE_WEAPON") == 0)
	{
		const char* st = AutoHarness_GetAppState();
		if (strcmp(st, "game") != 0)
		{
			_snprintf(result, resultSize, "ERROR: FIRE_WEAPON only works in test game (state: %s)", st);
			result[resultSize - 1] = 0;
			return true;
		}
		int holdFrames = 6;
		if (arg != nullptr && arg[0] != 0) { const int n = atoi(arg); if (n > 0 && n <= 240) holdFrames = n; }
		// ★ GGMAX 2.42c: THE trigger is t.gunmode = 101, not firingmode.
		// The tracer proved firingmode never arrives at the gun as 1 even though the hold ran
		// (holdLeft drained to 0), and DarkLUA_part5.cpp:1599 explains why — the original devs hit
		// this in 2015 and left the note "seems when in game, this gets ignored so no gunshoot
		// happens..". Their own Lua FirePlayerWeapon(1) therefore does NOT set firingmode; it does
		// exactly this, and only firingmode>=2 (zoom) still writes the field.
		if (t.gunmode < 101) t.gunmode = 101;
		extern int g_ggFireHoldFrames;
		g_ggFireHoldFrames = holdFrames;   // retained: harmless, and keeps holdLeft meaningful
		g.playeraction = 1;
		// GGMAX 2.42: arm the trigger tracer over a window LONGER than the hold, so the rows show
		// what happens after the trigger releases too (the edge latch only resolves then).
		extern void GGFireTraceReset(int frames);
		GGFireTraceReset(holdFrames + 24);
		_snprintf(result, resultSize, "OK: FIRE_WEAPON — trigger held for %d frames, tracer armed for %d",
			holdFrames, holdFrames + 24);
		result[resultSize - 1] = 0;
		return true;
	}

	// DUMP_SHADOWQTY — GGMAX 2.38. Print the shadow quantity/resolution from ALL THREE visuals
	// structs at once, plus the app state.
	//
	// The bug is "the editor shows 8 spot / 16 point, test game shows 0 / 0". Reading the call
	// sites did not settle it: visuals <-> gamevisuals are copied BOTH ways, and visuals_save /
	// visuals_load both handle the fields, so no value is simply missing — something overwrites
	// one copy later. Which copy, and when, is a question about live state, so print all three
	// side by side and sample it in the editor and again in test game. The one that differs names
	// the stage; guessing between three structs and a load sequence does not.
	if (_stricmp(cmd, "DUMP_SHADOWQTY") == 0)
	{
		_snprintf(result, resultSize,
			"OK: DUMP_SHADOWQTY state=%s\n"
			"  visuals      spotMax=%d pointMax=%d  res sun=%d spot=%d point=%d\n"
			"  gamevisuals  spotMax=%d pointMax=%d  res sun=%d spot=%d point=%d   <-- what the GAME uses\n"
			"  editorvisuals spotMax=%d pointMax=%d res sun=%d spot=%d point=%d\n"
			"  engine: localShadowCaching=%d",
			AutoHarness_GetAppState(),
			t.visuals.iShadowSpotMax, t.visuals.iShadowPointMax,
			t.visuals.iShadowSpotCascadeResolution, t.visuals.iShadowSpotResolution, t.visuals.iShadowPointResolution,
			t.gamevisuals.iShadowSpotMax, t.gamevisuals.iShadowPointMax,
			t.gamevisuals.iShadowSpotCascadeResolution, t.gamevisuals.iShadowSpotResolution, t.gamevisuals.iShadowPointResolution,
			t.editorvisuals.iShadowSpotMax, t.editorvisuals.iShadowPointMax,
			t.editorvisuals.iShadowSpotCascadeResolution, t.editorvisuals.iShadowSpotResolution, t.editorvisuals.iShadowPointResolution,
			wi::renderer::GetLocalShadowCachingEnabled() ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}

	// GGMAX 2.36 — drive the SCREEN (HUD) editor from the harness.
	//   HUD_EDIT [title]        enter the screen editor on that storyboard node (default In-Game HUD)
	//   HUD_ADD_IMAGE [path]    add an image widget, centred, pointed at path
	//   HUD_DUMP                report every widget AND whether its image actually loaded
	// The point of the trio is HUD_DUMP's `exist` column: it calls ImageExist() on the very id the
	// editor's draw path tests before it can blit (M-GridEditB_part22.cpp:938), so "is the yellow
	// box empty?" is answered by a number instead of by squinting at a screenshot.
	if (_stricmp(cmd, "HUD_EDIT") == 0)
	{
		extern int GGHudEditScreen(const char* title);
		const char* title = (arg != nullptr && arg[0] != 0) ? arg : "In-Game HUD";
		const int node = GGHudEditScreen(title);
		if (node < 0)
			_snprintf(result, resultSize, "ERROR: no storyboard node titled \"%s\"", title);
		else
			_snprintf(result, resultSize, "OK: HUD_EDIT \"%s\" -> node %d (screen editor open)", title, node);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "HUD_ADD_IMAGE") == 0)
	{
		extern int GGHudAddImage(const char* path, char* result, int resultSize);
		const char* path = (arg != nullptr && arg[0] != 0) ? arg : "imagebank\\hud\\ammo-health-panel.png";
		GGHudAddImage(path, result, resultSize);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "HUD_DUMP") == 0)
	{
		extern void GGHudDumpWidgets(char* result, int resultSize);
		GGHudDumpWidgets(result, resultSize);
		return true;
	}

	// INVALIDATE_LOCALSHADOWS — GGMAX 2.35. Force the cached local shadow atlas to re-render.
	// This is the EXECUTED-CHECK for the 2.35 fix: after removing a caster, if the stale shadow
	// vanishes the instant this is issued, the cached atlas was provably what was holding it —
	// which is the whole claim. Calls the same engine entry the fix calls from the delete and
	// collect paths, so the experiment tests the shipped mechanism and not a lookalike.
	if (_stricmp(cmd, "INVALIDATE_LOCALSHADOWS") == 0)
	{
		extern void GGInvalidateLocalShadows();
		GGInvalidateLocalShadows();
		_snprintf(result, resultSize, "OK: INVALIDATE_LOCALSHADOWS — cached local shadows re-render next frame");
		result[resultSize - 1] = 0;
		return true;
	}

	// MOVE_ENTITY <name-substr> <dx> <dy> <dz> — GGMAX 2.35. Translate matching objects.
	//
	// Exists to REPRODUCE pickup collection without playing the game. `SetEntityCollectedEx`
	// (DarkLUA_part0.cpp:1452) does not hide a collected entity — it TELEPORTS it by -999999 on
	// every axis and repositions the object. So `MOVE_ENTITY <name> -999999 -999999 -999999` is
	// the collect path's exact effect on the scene, drivable from the editor in one command
	// instead of walking to a table in test-game and pressing E.
	// Paired with SET_ENTITY_VIS (which hides instead of moving), this separates the two
	// candidate mechanisms for "the pickup went but its shadow stayed": a MOVED caster (which
	// GGMAX 2.07d's dynamic-caster test is supposed to catch) versus a HIDDEN one (which that
	// test skips outright at its `if (!object.IsRenderable()) continue;` guard).
	if (_stricmp(cmd, "MOVE_ENTITY") == 0)
	{
		char mvName[128] = { 0 }; float mdx = 0, mdy = 0, mdz = 0;
		if (arg == nullptr || sscanf_s(arg, "%127s %f %f %f", mvName, (unsigned)sizeof(mvName), &mdx, &mdy, &mdz) != 4)
		{
			_snprintf(result, resultSize, "ERROR: MOVE_ENTITY needs <name-substr> <dx> <dy> <dz>");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::scene::Scene* mvsc = master.masterrenderer.scene;
		if (mvsc == nullptr)
		{
			_snprintf(result, resultSize, "ERROR: no scene");
			result[resultSize - 1] = 0;
			return true;
		}
		auto mvHit = [](const std::string& hay, const char* needle) -> bool
		{
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		int mvCount = 0; XMFLOAT3 mvLast(0, 0, 0);
		for (size_t mi = 0; mi < mvsc->objects.GetCount(); ++mi)
		{
			const wi::ecs::Entity ment = mvsc->objects.GetEntity(mi);
			const wi::scene::NameComponent* mn = mvsc->names.GetComponent(ment);
			if (mn == nullptr || !mvHit(mn->name, mvName)) continue;
			wi::scene::TransformComponent* mtf = mvsc->transforms.GetComponent(ment);
			if (mtf == nullptr) continue;
			mtf->Translate(XMFLOAT3(mdx, mdy, mdz));
			mtf->UpdateTransform();
			mvLast = mtf->GetPosition();
			mvCount++;
		}
		_snprintf(result, resultSize, "OK: MOVE_ENTITY \"%s\" by (%.0f,%.0f,%.0f) on %d objects; last now at (%.0f,%.0f,%.0f)",
			mvName, mdx, mdy, mdz, mvCount, mvLast.x, mvLast.y, mvLast.z);
		result[resultSize - 1] = 0;
		return true;
	}

	// DUMP_BIGAABB [inflationRatio] — GGMAX 2.34. Census of objects whose world AABB is inflated
	// beyond the box their own mesh actually occupies.
	//
	// WHYNOTDRAWN answers "why is THIS object wrong?"; this answers "how much of the level is wrong?",
	// which is the question that decides whether the engine-side armature fix is worth the risk.
	// The detector is a RATIO, not a coordinate threshold: it recomputes the clean box the engine
	// would produce from the mesh alone (mesh.aabb.transform(world), exactly as RunObjectUpdateSystem
	// does) and compares it to the AABB actually in the scene. That keeps it honest whatever
	// `masterpark` is set to — a coordinate test would silently stop finding anything the moment
	// the park point moved closer, and report "fixed" when nothing had been fixed.
	if (_stricmp(cmd, "DUMP_BIGAABB") == 0)
	{
		wi::scene::Scene* sc = master.masterrenderer.scene;
		if (sc == nullptr)
		{
			_snprintf(result, resultSize, "ERROR: no scene");
			result[resultSize - 1] = 0;
			return true;
		}
		float ratioGate = 2.0f;
		if (arg != nullptr && arg[0] != 0) { const float v = (float)atof(arg); if (v > 1.0f) ratioGate = v; }

		struct BigRec
		{
			std::string name; std::string mesh; float ratio; float radius; float cleanRadius;
			float displace; int bones; int skinned; int dynamic; unsigned long long armature; unsigned filter;
		};
		std::vector<BigRec> bad;
		int total = 0, skinnedTotal = 0, overHalf = 0, displacedOverHalf = 0, transparentInflated = 0, transparentOverHalf = 0;
		float worstRatio = 0.0f, worstRadius = 0.0f;
		std::vector<unsigned long long> distinctMesh, distinctArm;

		for (size_t i = 0; i < sc->objects.GetCount(); ++i)
		{
			const wi::scene::ObjectComponent& ob = sc->objects[i];
			if (ob.meshID == wi::ecs::INVALID_ENTITY) continue;
			total++;
			const wi::ecs::Entity ent = sc->objects.GetEntity(i);
			const wi::scene::MeshComponent* mc = sc->meshes.GetComponent(ob.meshID);
			const wi::scene::TransformComponent* tf = sc->transforms.GetComponent(ent);
			if (mc == nullptr || tf == nullptr) continue;
			if (mc->IsSkinned()) skinnedTotal++;

			// The clean box: what the object bounds would be with no armature merge.
			const XMMATRIX W = XMLoadFloat4x4(&tf->world);
			const wi::primitive::AABB clean = mc->aabb.transform(W);
			const wi::primitive::AABB& actual = sc->aabb_objects[i];
			const float cleanR = clean.getRadius();
			const float actualR = actual.getRadius();
			if (cleanR <= 0.0001f) continue;
			const float ratio = actualR / cleanR;
			if (ratio <= ratioGate) continue;

			const XMFLOAT3 ca = actual.getCenter(), cc = clean.getCenter();
			const float displace = sqrtf((ca.x - cc.x) * (ca.x - cc.x) + (ca.y - cc.y) * (ca.y - cc.y) + (ca.z - cc.z) * (ca.z - cc.z));
			if (actualR > 65504.0f) overHalf++;
			if (displace > 65504.0f) displacedOverHalf++;
			if (ratio > worstRatio) worstRatio = ratio;
			if (actualR > worstRadius) worstRadius = actualR;

			const wi::scene::ArmatureComponent* arm =
				(mc->armatureID != wi::ecs::INVALID_ENTITY) ? sc->armatures.GetComponent(mc->armatureID) : nullptr;
			const wi::scene::NameComponent* nm = sc->names.GetComponent(ent);
			const wi::scene::NameComponent* mn = sc->names.GetComponent(ob.meshID);

			if (std::find(distinctMesh.begin(), distinctMesh.end(), (unsigned long long)ob.meshID) == distinctMesh.end())
				distinctMesh.push_back((unsigned long long)ob.meshID);
			if (arm != nullptr && std::find(distinctArm.begin(), distinctArm.end(), (unsigned long long)mc->armatureID) == distinctArm.end())
				distinctArm.push_back((unsigned long long)mc->armatureID);

			BigRec r;
			r.name = (nm != nullptr) ? nm->name : "(unnamed)";
			r.mesh = (mn != nullptr) ? mn->name : "(unnamed mesh)";
			r.ratio = ratio; r.radius = actualR; r.cleanRadius = cleanR; r.displace = displace;
			r.bones = (arm != nullptr) ? (int)arm->boneCollection.size() : -1;
			r.skinned = mc->IsSkinned() ? 1 : 0; r.dynamic = mc->IsDynamic() ? 1 : 0;
			r.armature = (unsigned long long)mc->armatureID;
			// The filter mask decides the ONE remaining visible-error exposure. An opaque object
			// that sorts wrong costs depth-test efficiency and nothing else; a TRANSPARENT one
			// sorts back-to-front, so a distance pinned at the fp16 ceiling would draw it before
			// everything else and it would appear behind geometry it should be in front of.
			r.filter = (unsigned)ob.GetFilterMask();
			if (r.filter & wi::enums::FILTER_TRANSPARENT)
			{
				transparentInflated++;
				if (actualR > 65504.0f) transparentOverHalf++;
			}
			bad.push_back(r);
		}

		std::sort(bad.begin(), bad.end(), [](const BigRec& a, const BigRec& b) { return a.ratio > b.ratio; });

		FILE* f = GGDiagFopen("dumpbigaabb.txt", "w");
		if (f != nullptr)
		{
			fprintf(f, "DUMP_BIGAABB  inflation gate: actualRadius > %.2f x cleanRadius\n", ratioGate);
			fprintf(f, "objects(with mesh)=%d  skinned=%d  INFLATED=%d (%.1f%% of all, %.1f%% of skinned)\n",
				total, skinnedTotal, (int)bad.size(),
				total ? (100.0 * bad.size() / total) : 0.0,
				skinnedTotal ? (100.0 * bad.size() / skinnedTotal) : 0.0);
			fprintf(f, "distinct meshes affected=%d  distinct armatures blamed=%d\n",
				(int)distinctMesh.size(), (int)distinctArm.size());
			fprintf(f, "radius over fp16 max (65504)=%d   centre displaced over 65504=%d\n", overHalf, displacedOverHalf);
			// The only remaining visible-error exposure. An OPAQUE mis-sort costs depth-test
			// efficiency; a TRANSPARENT one draws in the wrong order and is visible. What decides
			// it is not "is any inflated object transparent" but "is any object whose DISTANCE is
			// pinned at the fp16 ceiling transparent" — a legitimately-merged armature (hair, ~2x)
			// still has a normal distance and sorts correctly.
			fprintf(f, "TRANSPARENT among inflated=%d   of which also over-fp16=%d  <-- only the second number can mis-sort\n",
				transparentInflated, transparentOverHalf);
			fprintf(f, "worst ratio=%.1fx  worst radius=%.1f\n", worstRatio, worstRadius);
			fprintf(f, "scene bounds=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)\n\n",
				sc->bounds._min.x, sc->bounds._min.y, sc->bounds._min.z,
				sc->bounds._max.x, sc->bounds._max.y, sc->bounds._max.z);
			fprintf(f, "%-28s %-24s %9s %11s %11s %11s %6s %4s %4s %9s %s\n",
				"object", "mesh", "ratio", "radius", "cleanR", "displace", "bones", "skin", "dyn", "filter", "flags");
			const int cap = (int)bad.size() < 40 ? (int)bad.size() : 40;
			for (int i = 0; i < cap; ++i)
				fprintf(f, "%-28.28s %-24.24s %8.1fx %11.1f %11.1f %11.1f %6d %4d %4d %09x %s%s\n",
					bad[i].name.c_str(), bad[i].mesh.c_str(), bad[i].ratio, bad[i].radius,
					bad[i].cleanRadius, bad[i].displace, bad[i].bones, bad[i].skinned, bad[i].dynamic,
					bad[i].filter,
					(bad[i].filter & wi::enums::FILTER_TRANSPARENT) ? "TRANSPARENT " : "",
					(bad[i].radius > 65504.0f) ? "OVER-FP16" : "");
			if ((int)bad.size() > cap) fprintf(f, "... %d more\n", (int)bad.size() - cap);
			fclose(f);
		}
		_snprintf(result, resultSize,
			"OK: DUMP_BIGAABB gate=%.1fx objects=%d skinned=%d INFLATED=%d meshes=%d armatures=%d "
			"overFp16=%d TRANSPARENT=%d TRANSPARENT_OVERFP16=%d worstRatio=%.1fx worstRadius=%.0f -> dumpbigaabb.txt",
			ratioGate, total, skinnedTotal, (int)bad.size(), (int)distinctMesh.size(),
			(int)distinctArm.size(), overHalf, transparentInflated, transparentOverHalf, worstRatio, worstRadius);
		result[resultSize - 1] = 0;
		return true;
	}

	// WHYNOTDRAWN <name-substr> — GGMAX 2.30, closes backlog #125 ("why did object X not draw?").
	//
	// Walks the SAME gates wi::renderer::UpdateVisibility applies when it builds the main
	// camera's visible set, in the same order, and prints each verdict for every matching
	// object — then states which gate is the one actually rejecting it.
	// ★ Built for the invisible collectable pistol on spotshadowtest, where the mesh is absent
	// from the camera pass while its shadow renders perfectly. Two whole hypotheses (lazy PSO,
	// apparent-size cull) were eliminated by live A/B before this existed; each cost a rebuild
	// or a relaunch. This answers the same class of question in one command.
	// ⚠ `inVisibleSet` is from the PRIOR frame — the harness runs in GuruLoopLogic, ahead of
	// this frame's UpdateVisibility. Everything else is read live from the scene.
	if (_stricmp(cmd, "WHYNOTDRAWN") == 0)
	{
		if (arg == nullptr || arg[0] == 0)
		{
			_snprintf(result, resultSize, "ERROR: WHYNOTDRAWN needs a name substring");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::scene::Scene* sc = master.masterrenderer.scene;
		if (sc == nullptr)
		{
			_snprintf(result, resultSize, "ERROR: no scene");
			result[resultSize - 1] = 0;
			return true;
		}
		extern float g_apparentCullDirect;
		extern float g_apparentCullK;
		extern float maxApparentSize;
		const wi::renderer::Visibility& vis = master.masterrenderer.visibility_main;

		// The tangent the cull will actually use, mapped exactly as master_part1.cpp does.
		float tangent = 0.0f;
		if (g_apparentCullDirect >= 0.0f) tangent = g_apparentCullDirect;
		else { const float over = maxApparentSize - 0.000008f; tangent = (over > 0.0f) ? over * g_apparentCullK : 0.0f; }
		const float tangentSq = tangent * tangent;

		XMFLOAT3 eye = (vis.camera != nullptr) ? vis.camera->Eye : XMFLOAT3(0, 0, 0);

		// case-insensitive substring, same shape DUMP_INSTANCE uses
		auto nameHit = [](const std::string& hay, const char* needle) -> bool
		{
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};

		FILE* f = GGDiagFopen("whynotdrawn.txt", "w");
		int matches = 0;
		int armedMesh = -1;               // GGMAX 2.31: draw tracer armed on the first match
		const int prevArmed = wi::renderer::gg_dbg_watch_mesh;
		std::string firstVerdict;
		for (size_t i = 0; i < sc->objects.GetCount(); ++i)
		{
			const wi::ecs::Entity ent = sc->objects.GetEntity(i);
			const wi::scene::NameComponent* nm = sc->names.GetComponent(ent);
			if (nm == nullptr || nm->name.empty()) continue;
			if (!nameHit(nm->name, arg)) continue;
			matches++;

			const wi::scene::ObjectComponent& ob = sc->objects[i];
			const wi::primitive::AABB& ab = sc->aabb_objects[i];
			const XMFLOAT3 c = ab.getCenter();
			const float radius = ab.getRadius();
			const float distSq = wi::math::DistanceSquared(eye, c);
			const float dist = sqrtf(distSq);

			const bool layerOk = (ab.layerMask & vis.layerMask) != 0;
			const bool frustumOk = vis.frustum.CheckBoxFast(ab);
			const bool apparentCull = (tangentSq > 0.0f) && (distSq > radius * radius)
				&& (radius * radius < tangentSq * distSq);

			bool occluded = false; uint32_t hist = 0;
			if (i < sc->occlusion_results_objects.size())
			{
				occluded = sc->occlusion_results_objects[i].IsOccluded();
				hist = sc->occlusion_results_objects[i].occlusionHistory;
			}
			const bool occlAllowed = (vis.flags & wi::renderer::Visibility::ALLOW_OCCLUSION_CULLING) != 0
				&& wi::renderer::GetOcclusionCullingEnabled();

			bool inSet = false;
			for (size_t v = 0; v < vis.visibleObjects.size(); ++v)
				if (vis.visibleObjects[v] == (uint32_t)i) { inSet = true; break; }

			// ── DRAW-STAGE gates: being in the visible set is NOT the end of the story. Both of
			// these use the object's CACHED center/radius, so a corrupt AABB reaches them even
			// though it sails through every cull above.
			//   1. DrawScene's queue build   (wiRenderer.cpp:8663) drops the object outright when
			//      distance-to-centre exceeds fadeDistance + radius.
			//   2. RenderMeshes' distance FADE (wiRenderer.cpp:4170-4174) skips the draw when the
			//      dither factor exceeds 0.99. ★ This is the one that is camera-only: the shadow
			//      pass supplies distance 0, so it can never trigger there — which is exactly the
			//      "no mesh, perfect shadow" signature.
			const float objDist = wi::math::Distance(eye, ob.center);
			const bool distReject = (objDist > ob.fadeDistance + ob.radius);
			// ⚠⚠ THE DITHER MUST BE COMPUTED ON THE HALF-ROUND-TRIPPED DISTANCE, not the float.
			// RenderBatch stores distance as a HALF (wiRenderer.cpp:474/483) and RenderMeshes reads
			// it back through GetDistance(). The first version of this instrument used the raw
			// float and therefore printed dither=0.0000 — "DRAWN" — for an object the engine was
			// skipping with dither=INF, because 86,730 overflows a half to infinity. The
			// instrument agreed with my reasoning instead of with the renderer.
			// ★ RULE: an instrument that mirrors engine logic must mirror its PRECISION too.
			// Mirrors RenderBatch::Create exactly, INCLUDING the 2.32 clamp — otherwise the
			// instrument would keep reporting the pre-fix INF on a build where the engine now
			// clamps, i.e. lying in the opposite direction.
			const float batchDist = XMConvertHalfToFloat(XMConvertFloatToHalf(std::min(objDist, 65504.0f)));
			const bool halfWouldOverflow = (objDist > 65504.0f);
			const float ditherFade = (ob.radius > 0.0f)
				? std::max(0.0f, batchDist - ob.fadeDistance) / ob.radius : 0.0f;
			const float dither = std::max(ob.GetTransparency(), ditherFade);
			const bool ditherSkip = (dither > 0.99f);

			// First failing gate, in the engine's own order.
			const char* verdict;
			if (!ob.IsRenderable())                 verdict = "NOT RENDERABLE (SetRenderable false)";
			else if (!layerOk)                      verdict = "LAYER MASK excludes it from this view";
			else if (!frustumOk)                    verdict = "FRUSTUM rejects its AABB";
			else if (apparentCull)                  verdict = "APPARENT-SIZE CULL (delta 1.30) — too small at this distance";
			else if (occlAllowed && occluded)       verdict = "OCCLUSION QUERY says occluded (history all-zero)";
			else if (!inSet)                        verdict = "ABSENT from the prior frame's visible set — look further";
			// ── The three DrawScene queue-build rejects that sit BETWEEN visibleObjects and
			// batch_flush (wiRenderer.cpp:8651-8661). The draw tracer proved the gap is here:
			// the pistol is in the visible set yet its mesh never reaches a batch.
			// ★ IsNotVisibleInMainCamera is the shadow-PROXY flag — its entire purpose is
			// "cast shadows, never draw in the camera", which is the reported symptom verbatim.
			else if (ob.IsNotVisibleInMainCamera())
				verdict = "NOT_VISIBLE_IN_MAIN_CAMERA is SET — the shadow-proxy flag: casts shadows, never drawn by the camera, BY DESIGN";
			else if (ob.IsForeground())             verdict = "IsForeground set — only drawn in the foreground pass";
			else if (ob.GetFilterMask() == 0)       verdict = "FILTER MASK is 0 — matches no pass";
			else if (distReject)                    verdict = "DRAW-DISTANCE REJECT (wiRenderer.cpp:8663) — dist > fadeDistance + radius";
			else if (ditherSkip)                    verdict = "DISTANCE-FADE SKIP (wiRenderer.cpp:4170) — dither > 0.99, camera pass ONLY (the shadow pass passes distance 0)";
			else                                    verdict = "DRAWN (passes every gate; if it is still not on screen the fault is in the pixels, not the plumbing)";
			if (firstVerdict.empty()) firstVerdict = verdict;

			if (f != nullptr)
			{
				fprintf(f, "OBJ idx=%d entity=%llu name=\"%s\"\n", (int)i, (unsigned long long)ent, nm->name.c_str());
				fprintf(f, "  aabb=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) center=(%.0f,%.0f,%.0f) r=%.1f layer=%08x\n",
					ab._min.x, ab._min.y, ab._min.z, ab._max.x, ab._max.y, ab._max.z, c.x, c.y, c.z, radius, ab.layerMask);
				fprintf(f, "  camEye=(%.0f,%.0f,%.0f) distToCenter=%.0f  camInsideAABB=%s\n",
					eye.x, eye.y, eye.z, dist, ab.intersects(eye) ? "YES(forces visible)" : "no");
				fprintf(f, "  renderable=%d layerOk=%d frustum=%s apparentCull=%s(tangent=%.5f) occlusion=%s(hist=%08x,allowed=%d) inVisibleSet=%s\n",
					ob.IsRenderable() ? 1 : 0, layerOk ? 1 : 0,
					frustumOk ? "PASS" : "REJECT",
					apparentCull ? "CULLED" : "pass", tangent,
					occluded ? "OCCLUDED" : "visible", hist, occlAllowed ? 1 : 0,
					inSet ? "yes" : "NO");
				fprintf(f, "  objCenter=(%.0f,%.0f,%.0f) objRadius=%.1f fadeDistance=%.1f distToObjCenter=%.0f\n",
					ob.center.x, ob.center.y, ob.center.z, ob.radius, ob.fadeDistance, objDist);
				fprintf(f, "  drawReject=%s  dither=%.4f (%s, skip if >0.99)  transparency=%.3f\n",
					distReject ? "YES(dist > fade+radius)" : "no",
					dither, ditherSkip ? "SKIPS THE DRAW" : "draws", ob.GetTransparency());
				fprintf(f, "  batchDistance(half round-trip, 2.32-clamped)=%.1f  rawDist=%.0f %s  [half max 65504]\n",
					batchDist, objDist,
					halfWouldOverflow ? "*** would have overflowed the half to INF before the 2.32 clamp ***" : "ok");
				fprintf(f, "  VERDICT: %s\n", verdict);

				// ── DRAW TRACER (GGMAX 2.31). Everything above is CPU-side bookkeeping; this is
				// what the renderer actually did with the mesh, counted inside batch_flush.
				// Arm on first match, then re-run the command to read the frames since.
				// ★ Read MAIN vs SHADOW side by side: draws in SHADOW and none in MAIN is the
				// exact "no mesh, perfect shadow" signature, and nofilter/nopso say which of the
				// two silent skips did it.
				// ★ TWO mesh indices, and they are not the same thing. The renderer keys its
				// batches on the ObjectComponent's CACHED `mesh_index`; `meshes.GetIndex(meshID)`
				// is the truth. `mesh_index` is documented valid for ONE FRAME only
				// (wiScene_Components.h:1229) and is only refreshed inside RunObjectUpdateSystem's
				// `meshes.Contains(meshID) && transforms.Contains(entity)` guard — so a stale one
				// sends the draw to the wrong batch key, or nowhere. Print both; watch on the one
				// the renderer actually uses.
				const size_t meshIdxTrue = sc->meshes.GetIndex(ob.meshID);
				const size_t meshIdx = (size_t)ob.mesh_index;
				// Arm on the first match. ⚠ ZERO THE COUNTERS ONLY WHEN THE WATCHED MESH ACTUALLY
				// CHANGES. The first cut zeroed them on every invocation — `armedMesh` is a local,
				// so the read call reset the counters a microsecond before printing them and every
				// row came back 0. A control object that is plainly on screen printed 0 too, which
				// is the only reason it was caught. Same family as the 2.29 latch bug: a counter
				// that is re-zeroed on every read is indistinguishable from a counter that never
				// fires. ★ Always arm the tracer on a KNOWN-GOOD object first.
				if (armedMesh < 0 && meshIdx != ~0ull)
				{
					armedMesh = (int)meshIdx;
				}
				if (armedMesh >= 0 && armedMesh != prevArmed)
				{
					for (int rp = 0; rp < wi::enums::RENDERPASS_COUNT; ++rp)
					{
						wi::renderer::gg_dbg_watch_batches[rp] = 0;
						wi::renderer::gg_dbg_watch_instances[rp] = 0;
						wi::renderer::gg_dbg_watch_nobuffer[rp] = 0;
						wi::renderer::gg_dbg_watch_subsets[rp] = 0;
						wi::renderer::gg_dbg_watch_nofilter[rp] = 0;
						wi::renderer::gg_dbg_watch_nopso[rp] = 0;
						wi::renderer::gg_dbg_watch_draws[rp] = 0;
						for (int s = 0; s < wi::renderer::GG_Q_COUNT; ++s)
							wi::renderer::gg_dbg_watch_q[rp][s] = 0;
					}
					wi::renderer::gg_dbg_watch_mesh = armedMesh;
				}
				// GGMAX 2.33: the ARMATURE the bounds are merged from. `wiScene.cpp:5231-5241`
				// merges `armature->aabb` (WORLD space) into any object whose MESH IsSkinned() or
				// IsDynamic(), and resolves it from `mesh.armatureID` — a MESH property, so two
				// objects sharing a mesh share the armature and therefore the merge.
				// Printed because the fix for the corrupt pickup bounds depends on which of the
				// two branches actually fires and on where the armature really sits.
				{
					const wi::scene::MeshComponent* mc = sc->meshes.GetComponent(ob.meshID);
					if (mc == nullptr) fprintf(f, "  mesh: <none>\n");
					else
					{
						fprintf(f, "  mesh: skinned=%d(armatureID=%llu) dynamic=%d  meshAABB=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)\n",
							mc->IsSkinned() ? 1 : 0, (unsigned long long)mc->armatureID,
							mc->IsDynamic() ? 1 : 0,
							mc->aabb._min.x, mc->aabb._min.y, mc->aabb._min.z,
							mc->aabb._max.x, mc->aabb._max.y, mc->aabb._max.z);
						const wi::scene::ArmatureComponent* arm =
							(mc->armatureID != wi::ecs::INVALID_ENTITY) ? sc->armatures.GetComponent(mc->armatureID) : nullptr;
						if (arm != nullptr)
							fprintf(f, "  armature: bones=%d worldAABB=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)  <-- merged into the object bounds\n",
								(int)arm->boneCollection.size(),
								arm->aabb._min.x, arm->aabb._min.y, arm->aabb._min.z,
								arm->aabb._max.x, arm->aabb._max.y, arm->aabb._max.z);
						else if (mc->armatureID != wi::ecs::INVALID_ENTITY)
							fprintf(f, "  armature: armatureID set but NO ArmatureComponent found\n");
					}
				}
				fprintf(f, "  notVisibleInMainCamera=%d foreground=%d filterMask=%08x notInReflections=%d\n",
					ob.IsNotVisibleInMainCamera() ? 1 : 0, ob.IsForeground() ? 1 : 0,
					(unsigned)ob.GetFilterMask(), ob.IsNotVisibleInReflections() ? 1 : 0);
				fprintf(f, "  meshID=%llu  mesh_index(cached,used by the renderer)=%d  GetIndex(meshID)(true)=%d  %s\n",
					(unsigned long long)ob.meshID, (int)ob.mesh_index, (int)meshIdxTrue,
					((size_t)ob.mesh_index == meshIdxTrue) ? "AGREE" : "*** MISMATCH — the draw is keyed on a stale index ***");
				fprintf(f, "  --- draw tracer (watching mesh %d, counts since armed; re-run to read frames) ---\n",
					(int)meshIdx);
				static const char* passName[] = { "MAIN", "PREPASS", "PREPASS_DEPTHONLY", "ENVMAP", "SHADOW", "SHADOWCUBE", "VOXELIZE" };
				for (int rp = 0; rp < wi::enums::RENDERPASS_COUNT; ++rp)
				{
					const uint32_t b = wi::renderer::gg_dbg_watch_batches[rp].load(std::memory_order_relaxed);
					const uint32_t d = wi::renderer::gg_dbg_watch_draws[rp].load(std::memory_order_relaxed);
					const uint32_t nf = wi::renderer::gg_dbg_watch_nofilter[rp].load(std::memory_order_relaxed);
					const uint32_t np = wi::renderer::gg_dbg_watch_nopso[rp].load(std::memory_order_relaxed);
					const uint32_t su = wi::renderer::gg_dbg_watch_subsets[rp].load(std::memory_order_relaxed);
					const uint32_t inst = wi::renderer::gg_dbg_watch_instances[rp].load(std::memory_order_relaxed);
					const uint32_t nb = wi::renderer::gg_dbg_watch_nobuffer[rp].load(std::memory_order_relaxed);
					// GGMAX 2.32: the queue-build half — where the object went between
					// visibleObjects and the renderQueue.
					const uint32_t qs = wi::renderer::gg_dbg_watch_q[rp][wi::renderer::GG_Q_SEEN].load(std::memory_order_relaxed);
					if (qs > 0)
					{
						static const char* qn[] = { "seen", "occluded", "notRenderable", "foreground",
							"notMainCam", "noReflect", "filterMask", "fadeDist", "ADDED" };
						char qline[220]; qline[0] = 0;
						for (int s = 0; s < wi::renderer::GG_Q_COUNT; ++s)
						{
							const uint32_t v = wi::renderer::gg_dbg_watch_q[rp][s].load(std::memory_order_relaxed);
							if (v == 0 && s != wi::renderer::GG_Q_SEEN && s != wi::renderer::GG_Q_ADDED) continue;
							char one[48]; _snprintf(one, sizeof(one), "%s=%u ", qn[s], v);
							strncat(qline, one, sizeof(qline) - strlen(qline) - 1);
						}
						fprintf(f, "    %-18s QUEUE  %s\n", (rp < 7) ? passName[rp] : "?", qline);
					}
					if (b == 0 && d == 0 && nf == 0 && np == 0 && qs == 0) continue;   // pass never touched this mesh
					fprintf(f, "    %-18s batches=%u instances=%u subsets=%u draws=%u nofilter=%u nopso=%u nobuffer=%u\n",
						(rp < 7) ? passName[rp] : "?", b, inst, su, d, nf, np, nb);
				}
			}
		}
		if (f != nullptr) fclose(f);

		// The tracer only has data once a frame has rendered WITH it armed, so the first call on a
		// new mesh always reads zeros. Say so rather than letting a zero be read as "never drawn".
		const bool tracerFresh = (armedMesh >= 0 && armedMesh != prevArmed);
		_snprintf(result, resultSize,
			"OK: WHYNOTDRAWN \"%s\" matches=%d visibleSet=%d/%d tangent=%.5f occlusionEnabled=%d -> whynotdrawn.txt\n"
			"FIRST VERDICT: %s\n"
			"DRAW TRACER: %s",
			arg, matches,
			(int)vis.visibleObjects.size(), (int)sc->objects.GetCount(),
			tangent, wi::renderer::GetOcclusionCullingEnabled() ? 1 : 0,
			matches ? firstVerdict.c_str() : "(no object matched that name)",
			tracerFresh
				? "ARMED on this mesh just now — counters are ZERO until a frame renders. RE-RUN the command to read them."
				: (armedMesh >= 0 ? "live (counts are since it was armed)" : "not armed (no mesh)"));
		result[resultSize - 1] = 0;
		return true;
	}

	// ========================================================================================
	// GGMAX 3.25: Terrain Bake / Water Bake / Reduction Scale. FLAT links, same C1061 rule as
	// the block below - never add these to the main else-if ladder.
	// ========================================================================================
	if (_stricmp(cmd, "DUMP_BAKE") == 0)
	{
		const char* GGTerrainBake_Report();
		const char* GGWaterBake_Report();
		_snprintf(result, resultSize, "%s\n%s", GGTerrainBake_Report(), GGWaterBake_Report());
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "TERRAIN_FAR") == 0)
	{
		extern const char* GGTerrainFar_Report();
		_snprintf(result, resultSize, "%s", GGTerrainFar_Report());
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_TERRAINFAR") == 0 || _stricmp(cmd, "SET_TERRAINFAR_TREES") == 0 ||
		_stricmp(cmd, "SET_TERRAINFAR_DEBUG") == 0 || _stricmp(cmd, "SET_TERRAINFAR_SINK") == 0 ||
		_stricmp(cmd, "SET_TERRAINFAR_REFLECT") == 0)
	{
		// GGMAX 3.114 far terrain knobs - all read at draw time, so they apply next frame.
		extern bool gg_terrain_far, gg_terrain_far_trees, gg_terrain_far_reflect;
		extern int gg_terrain_far_debug;
		extern float gg_terrain_far_sink;
		if (arg && arg[0])
		{
			if (_stricmp(cmd, "SET_TERRAINFAR") == 0) gg_terrain_far = atoi(arg) != 0;
			else if (_stricmp(cmd, "SET_TERRAINFAR_TREES") == 0) gg_terrain_far_trees = atoi(arg) != 0;
			else if (_stricmp(cmd, "SET_TERRAINFAR_DEBUG") == 0) gg_terrain_far_debug = atoi(arg);
			else if (_stricmp(cmd, "SET_TERRAINFAR_REFLECT") == 0) gg_terrain_far_reflect = atoi(arg) != 0;
			else gg_terrain_far_sink = (float)atof(arg);
		}
		_snprintf(result, resultSize, "OK: %s - far %d, trees %d, reflect %d, debug %d (1 tint, 2 coverage, 3 levels), sink x%.2f",
			cmd, gg_terrain_far ? 1 : 0, gg_terrain_far_trees ? 1 : 0, gg_terrain_far_reflect ? 1 : 0, gg_terrain_far_debug, gg_terrain_far_sink);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_FOGCURVE") == 0)
	{
		// GGMAX 3.114: 1 = DX11 fog curve (optical depth past the fog start only; default), 0 = the stock
		// engine curve. Frame-wide bit read by the engine fog AND GG's ApplyFogCustom - live next frame.
		if (arg && arg[0]) wi::renderer::gg_dx11_fog_curve = atoi(arg) != 0;
		_snprintf(result, resultSize, "OK: SET_FOGCURVE - now %d (1 = DX11 curve past fog start, 0 = stock engine curve)",
			wi::renderer::gg_dx11_fog_curve ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_MSAOREFFAR") == 0)
	{
		// GGMAX 3.126: MSAO edge tolerances as if zFar were <units> when the camera's far is larger
		// (0 = stock). Live next frame. No argument = report.
		if (arg && arg[0]) wi::renderer::gg_msao_reference_far = (float)atof(arg);
		_snprintf(result, resultSize, "OK: SET_MSAOREFFAR - now %.1f (0 = stock MSAO tolerances; camera far %.1f)",
			wi::renderer::gg_msao_reference_far, wi::scene::GetCamera().zFarP);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_MSAODOWNSAMPLE") == 0)
	{
		// GGMAX 3.126b: MSAO depth-pyramid pick at depth edges - 0 stock, 1 farthest, 2 nearest - and the
		// relative depth range that counts as an edge. Live next frame. No argument = report.
		int dsMode = -1; float dsEdge = -1.0f;
		if (arg && arg[0]) sscanf_s(arg, "%d %f", &dsMode, &dsEdge);
		if (dsMode >= 0 && dsMode <= 2) wi::renderer::gg_msao_downsample_mode = (uint32_t)dsMode;
		if (dsEdge >= 0.0f) wi::renderer::gg_msao_downsample_edge = dsEdge;
		_snprintf(result, resultSize, "OK: SET_MSAODOWNSAMPLE - mode %u (0 stock, 1 farthest, 2 nearest at edges), edge %.3f",
			wi::renderer::gg_msao_downsample_mode, wi::renderer::gg_msao_downsample_edge);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_TERRAINSPLATBLEND") == 0)
	{
		// GGMAX 3.113: 1 = the terrain compositor AVERAGES GG's splat blend weights (default); 0 = the
		// stock opacity stack (pre-3.113 look). Re-renders every live chunk's virtual texture; re-bake after.
		extern void GGTerrainWicked_SetSplatBlend(bool on);
		extern bool gg_terrain_splat_blend;
		if (arg && arg[0]) GGTerrainWicked_SetSplatBlend(atoi(arg) != 0);
		_snprintf(result, resultSize, "OK: SET_TERRAINSPLATBLEND - now %d (1 = splat average, 0 = old opacity stack); live chunks re-rendering",
			gg_terrain_splat_blend ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_BAKESHADING") == 0)
	{
		// GGMAX 3.113: GG_OPT_BAKE_* bits for the Terrain Bake pixel shader - 1 decode normal, 2 SSAO,
		// 4 probe ambient, 8 full sun, 16 live material; 3.115: 32 aniso, 64 near detail colour, 128 near
		// detail normals (kept only if 128 was on at bake time). 255 (0xFF) = all on = the shipped setting.
		// Live, next frame - no re-bake. For A/B'ing the baked ground against the live terrain.
		extern uint32_t gg_terrain_bake_shading;
		if (arg && arg[0]) gg_terrain_bake_shading = (uint32_t)strtoul(arg, nullptr, 0) & 0xFFu;
		_snprintf(result, resultSize, "OK: SET_BAKESHADING - mask now 0x%02X (1 normal, 2 ssao, 4 probe ambient, 8 full sun, 16 live material, 32 aniso, 64 near detail, 128 detail normals)",
			gg_terrain_bake_shading);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_BAKEDETAILFADE") == 0)
	{
		// GGMAX 3.115: the near detail's fade, in the BAKE TEXTURE'S OWN LOD (16x aniso): detail is gone at
		// lod >= <end> and full at lod <= <end> - <width>. Default 0 1: hands over where the bake's mip 0 is
		// exactly material mip log2(dim*32/res). Live, next frame.
		extern float gg_terrain_bake_detail_lod_end, gg_terrain_bake_detail_width;
		float e = gg_terrain_bake_detail_lod_end, w = gg_terrain_bake_detail_width;
		if (arg && arg[0] && sscanf_s(arg, "%f %f", &e, &w) >= 1)
		{
			gg_terrain_bake_detail_lod_end = e;
			gg_terrain_bake_detail_width = (w > 0.01f) ? w : 0.01f;
		}
		_snprintf(result, resultSize, "OK: SET_BAKEDETAILFADE - detail full at bake lod <= %.2f, gone at >= %.2f",
			gg_terrain_bake_detail_lod_end - gg_terrain_bake_detail_width, gg_terrain_bake_detail_lod_end);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_BAKEDETAILDEBUG") == 0)
	{
		// GGMAX 3.115: 0 normal, 1 = detail weight forced to 1 wherever it can run (the sharpness ceiling),
		// 2 = forced to 0 (the bake alone through the same code path).
		extern int gg_terrain_bake_detail_debug;
		if (arg && arg[0]) gg_terrain_bake_detail_debug = atoi(arg);
		_snprintf(result, resultSize, "OK: SET_BAKEDETAILDEBUG %d (0 normal, 1 force full, 2 force off)", gg_terrain_bake_detail_debug);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_BAKERES") == 0)
	{
		// Per-chunk bake resolution. Takes effect on the NEXT bake, so the reply says so rather
		// than letting a caller read the new number and assume the textures changed under it.
		extern int gg_terrain_bake_res;
		const int v = atoi(arg);
		if (v >= 32 && v <= 2048) gg_terrain_bake_res = v;
		_snprintf(result, resultSize,
			"OK: SET_BAKERES - bake resolution now %d per chunk. Applies to the NEXT bake: "
			"untick and re-tick Terrain Bake (or SET_BAKETERRAIN 0 then 1) to rebuild.",
			gg_terrain_bake_res);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WATERBAKEALPHA") == 0)
	{
		extern float gg_water_bake_alpha;
		const int v = atoi(arg);
		// -1 (or any negative) hands control back to the Water Base Color picker's own A.
		if (v < 0) gg_water_bake_alpha = -1.0f;
		else if (v <= 100) gg_water_bake_alpha = (float)v * 0.01f;
		extern int gg_water_bake_depth;
		_snprintf(result, resultSize, "OK: SET_WATERBAKEALPHA - plane alpha %s%s",
			(gg_water_bake_alpha < 0.0f) ? "follows the Water Base Color picker" : "overridden",
			gg_water_bake_depth ? " - in the DX11 water fog (waterbakedepth 1) it is the MINIMUM opacity, 0 = none" : "");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WATERBAKEUNDER") == 0 || _stricmp(cmd, "SET_UWCEILING") == 0)
	{
		// GGMAX 3.120: SET_WATERBAKEUNDER <0|1> [gain x100] - the baked water seen from below (opaque ceiling + the engine
		// underwater postprocess with the ocean off). SET_UWCEILING <0|1> - the engine's exact-ceiling fog term (A/B).
		extern int gg_water_bake_under; extern float gg_water_bake_under_gain; extern float gg_water_bake_under_bed;
		int a = -1, g = -1, bm = -1;
		const int n = (arg && arg[0]) ? sscanf_s(arg, "%d %d %d", &a, &g, &bm) : 0;
		if (_stricmp(cmd, "SET_UWCEILING") == 0)
		{
			if (n >= 1) wi::gg_underwater_ceiling_exact = (a != 0);
			_snprintf(result, resultSize, "OK: SET_UWCEILING %d (%s)", wi::gg_underwater_ceiling_exact ? 1 : 0,
				wi::gg_underwater_ceiling_exact ? "baked ceiling fogs by exactly the distance to the plane" : "stock min(0.1 x distance behind, distance to plane) - shore terrain can ghost");
		}
		else
		{
			if (n >= 1 && (a == 0 || a == 1)) gg_water_bake_under = a;
			if (n >= 2 && g >= 0 && g <= 2000) gg_water_bake_under_gain = (float)g * 0.01f;
			if (n >= 3 && bm >= 0 && bm <= 100) gg_water_bake_under_bed = (float)bm * 0.01f;
			_snprintf(result, resultSize, "OK: SET_WATERBAKEUNDER %d gain %.2f seabed mix %.2f (%s)", gg_water_bake_under, gg_water_bake_under_gain, gg_water_bake_under_bed,
				gg_water_bake_under ? "under the baked water: opaque ceiling + the live ocean's underwater fog" : "3.119 look under water");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WATERBAKEDEPTH") == 0)
	{
		// GGMAX 3.119: SET_WATERBAKEDEPTH <depth 0|1> [lit 0|1] [view 0|3|4] - the Water Bake DX11 water fog.
		// depth 1 = opaque with depth and distance from the scene depth behind the plane (default), 0 = the 3.35f
		// flat plane; lit = sun + ambient on the water colour; view 3 = opacity as grey, 4 = thickness as grey.
		extern int gg_water_bake_depth, gg_water_bake_lit, gg_water_bake_view;
		int d = -1, l = -1, v = -1;
		const int n = (arg && arg[0]) ? sscanf_s(arg, "%d %d %d", &d, &l, &v) : 0;
		if (n >= 1 && (d == 0 || d == 1)) gg_water_bake_depth = d;
		if (n >= 2 && (l == 0 || l == 1)) gg_water_bake_lit = l;
		if (n >= 3 && (v == 0 || v == 3 || v == 4)) gg_water_bake_view = v;
		_snprintf(result, resultSize, "OK: SET_WATERBAKEDEPTH depth %d lit %d view %d (%s)", gg_water_bake_depth, gg_water_bake_lit, gg_water_bake_view,
			!gg_water_bake_depth ? (gg_water_bake_view ? "3.35f flat plane - the debug view needs depth 1" : "3.35f flat plane")
			: gg_water_bake_view == 3 ? "DEBUG: opacity as grey" : gg_water_bake_view == 4 ? "DEBUG: thickness / Water Fog Max as grey"
			: "DX11 water fog: opaque with depth and distance");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WATERBAKEFRESNEL") == 0)
	{
		// SET_WATERBAKEFRESNEL <strength 0..100> [power x10, 10..160]
		//
		// ★ The baked water plane is one flat alpha everywhere, so the lake bed was as visible at
		// the horizon as underfoot. This ramps alpha toward opaque at grazing angles - which on a
		// horizontal plane is the same thing as "with distance", but stays correct when the camera
		// changes height, because it is a real Schlick term on the plane normal rather than a lerp
		// on distance.
		//   strength 0   = off, the plain semi-transparent plane
		//   strength 85  = default
		//   power    30  = default (x10, so 30 means 3.0). Lower closes the water up NEARER.
		extern float gg_water_bake_fresnel;
		extern float gg_water_bake_fresnel_power;
		int s = -1, p = -1;
		const int n = sscanf_s(arg, "%d %d", &s, &p);
		if (n < 1 || s < 0 || s > 100)
		{
			_snprintf(result, resultSize, "ERROR: SET_WATERBAKEFRESNEL needs <strength 0..100> [power 10..160]");
		}
		else
		{
			gg_water_bake_fresnel = (float)s * 0.01f;
			if (n >= 2 && p >= 10 && p <= 160) gg_water_bake_fresnel_power = (float)p * 0.1f;
			extern int gg_water_bake_depth;
			_snprintf(result, resultSize,
				"OK: SET_WATERBAKEFRESNEL strength %.2f power %.1f - %s%s",
				gg_water_bake_fresnel, gg_water_bake_fresnel_power,
				(s == 0) ? "off, plane is uniformly semi-transparent"
				         : "water closes toward opaque as it recedes",
				gg_water_bake_depth ? " - NOT USED while waterbakedepth=1 (the DX11 water fog owns the opacity; SET_WATERBAKEDEPTH 0 to use it)" : "");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WATERBAKETINT") == 0)
	{
		// Percent of the way from the authored absorption tint toward the sky colour.
		extern float gg_water_bake_tint;
		const int v = atoi(arg);
		if (v >= 0 && v <= 100) gg_water_bake_tint = (float)v * 0.01f;
		_snprintf(result, resultSize, "OK: SET_WATERBAKETINT - now %.0f%% toward sky "
			"(0 = the raw authored water colour, which is near-black on most levels)",
			gg_water_bake_tint * 100.0f);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_WATERBAKEDEBUG") == 0)
	{
		// Solid-colour bisect: 1 forces the stand-in plane to OPAQUE MAGENTA. If magenta appears,
		// the draw is reaching the render target and the fault is colour or alpha; if it does not,
		// the draw is being rejected and nothing about the pipeline is cleared.
		extern int gg_water_bake_debug;
		gg_water_bake_debug = atoi(arg);
		if (gg_water_bake_debug < 0 || gg_water_bake_debug > 2) gg_water_bake_debug = 0;
		_snprintf(result, resultSize, "OK: SET_WATERBAKEDEBUG %d - plane is %s",
			gg_water_bake_debug,
			(gg_water_bake_debug == 2) ? "OPAQUE MAGENTA with the depth test DISABLED"
			: (gg_water_bake_debug == 1) ? "OPAQUE MAGENTA"
			: "the authored water colour");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_BAKERESNEAR") == 0)
	{
		// NEAR-tier resolution: the chunks with the most placed objects on them.
		extern int gg_terrain_bake_res_near;
		const int v = atoi(arg);
		if (v >= 32 && v <= 8192) gg_terrain_bake_res_near = v;
		_snprintf(result, resultSize,
			"OK: SET_BAKERESNEAR - near tier now %d per chunk (%d KB each, BC1). Applies to the "
			"NEXT bake: SET_BAKETERRAIN 0 then 1 to rebuild.",
			gg_terrain_bake_res_near, gg_terrain_bake_res_near * gg_terrain_bake_res_near / 2 / 1024);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_BAKENEARBUDGET") == 0)
	{
		// Hard cap on near-tier memory in MB. Chunks are promoted densest-first until it is
		// spent, so this is the one number that bounds the whole feature.
		extern int gg_terrain_bake_near_budget_mb;
		const int v = atoi(arg);
		if (v >= 0 && v <= 4096) gg_terrain_bake_near_budget_mb = v;
		_snprintf(result, resultSize,
			"OK: SET_BAKENEARBUDGET - near tier capped at %d MB. Applies to the NEXT bake.",
			gg_terrain_bake_near_budget_mb);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_ANIMREDUCTION") == 0)
	{
		// What Reduction Scale is actually doing right now. The count of armatures HELD is the
		// hardware-independent measure - it says the mechanism is working even on a machine where
		// skinning costs nothing. Pair it with DUMP_PROFILER's "Skinning and Morph" GPU row for
		// the money figure. The sample table exists so the distance curve can be checked against
		// the specification without arithmetic in your head.
		extern bool bEnable30FpsAnimations;
		wi::scene::Scene& asc = wi::scene::GetScene();
		const uint32_t ascale = wi::scene::gg_anim_reduction_scale.load(std::memory_order_relaxed);
		int w = _snprintf(result, resultSize,
			"ANIM REDUCTION\n"
			"  slider           : %d   (tick box %d - the slider only bites while that is ON)\n"
			"  engine scale     : %u\n"
			"  armatures        : %d in scene, %u HELD this frame\n"
			"  post-load grace  : %.1f s left, re-armed %u times (nothing held while above zero)\n"
			"  decision unit    : ARMATURE (all meshes of one character share it)\n"
			"  frames skipped between updates, by distance:\n",
			t.visuals.iAnimReductionScale, bEnable30FpsAnimations ? 1 : 0,
			ascale, (int)asc.armatures.GetCount(), wi::scene::gg_anim_armatures_skipped,
			wi::scene::gg_anim_reduction_grace, wi::scene::gg_anim_reduction_resets);
		// GGMAX 3.92: the number that proves the streamout ping-pong gate is live. 0 while
		// Reduction Scale is off or the post-load grace is running; several hundred once it
		// engages. If armatures are being HELD but this reads 0, the gate is not running and
		// every held mesh is alternating between two poses a period apart.
		// ⚠ Advance `w`, do NOT append at strlen(result). The distance loop below writes at
		// result + w, so a block that appends without moving the shared cursor is OVERWRITTEN by
		// the very next statement. That is exactly what happened on the first 3.92 build: this row
		// was printed and erased, the dump looked normal, and the verification proved nothing.
		w += _snprintf(result + w, resultSize - w,
			"  meshes held      : %u   (streamout swap suppressed this frame - GGMAX 3.92)\n",
			wi::scene::gg_anim_meshes_held.load(std::memory_order_relaxed));
		// GGMAX 3.93: the premise, as a number. A character's parts are separate armatures that
		// share a world PIVOT, and the phase is keyed on that group so they cannot desync. If
		// `groups` ever climbs toward the armature count, the premise has broken for some content
		// type and the wobbly head is back - visible here before anyone has to photograph it.
		w += _snprintf(result + w, resultSize - w,
			"  pivot groups     : %u of %u armatures, largest %u   (a character is ~6 armatures at one pivot - GGMAX 3.93)\n",
			wi::scene::gg_anim_groups, (unsigned)asc.armatures.GetCount(), wi::scene::gg_anim_group_largest);
		const float samples[6] = { 250.0f, 499.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f };
		for (int i = 0; i < 6 && w < (int)resultSize - 128; i++)
		{
			const uint32_t p = wi::scene::gg_anim_reduction_period(samples[i], ascale);
			w += _snprintf(result + w, resultSize - w, "     %6.0f units : period %u  (%u skipped)\n",
				samples[i], p, p > 0 ? p - 1 : 0);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_ANIMREDUCTION") == 0)
	{
		// Reduction Scale, 1..100. Echoes a LIVE re-read of what the engine now holds, not the
		// value passed in - and reports the tick box too, because the slider is a sub-control
		// and reads as broken if you set it while the box is off.
		int v = atoi(arg);
		if (v < 1) v = 1;
		if (v > 100) v = 100;
		t.gamevisuals.iAnimReductionScale = t.visuals.iAnimReductionScale = v;
		extern bool bEnable30FpsAnimations;
		_snprintf(result, resultSize,
			"OK: SET_ANIMREDUCTION %d - engine scale now %u. Lower Animation tick box is %d; "
			"the slider only bites while that is ON. At scale %d: %d units skips %d frames, "
			"%d units skips %d, under %d never skips.",
			v, wi::scene::gg_anim_reduction_scale.load(std::memory_order_relaxed),
			bEnable30FpsAnimations ? 1 : 0, v,
			// ⚠ DERIVED, not typed. These three distances were hardcoded as 500/1000/500 and
			// went stale the moment the constant moved. Read them off the constant instead.
			(int)wi::scene::GG_ANIM_REDUCTION_NEAR_DIST,
			(int)((v * 0.1f) * 1.0f),
			(int)(wi::scene::GG_ANIM_REDUCTION_NEAR_DIST * 2.0f),
			(int)((v * 0.1f) * 2.0f),
			(int)wi::scene::GG_ANIM_REDUCTION_NEAR_DIST);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_SKINFRESHGUARD") == 0)
	{
		// GGMAX 3.107 (tester report #4): SET_SKINFRESHGUARD [0|1]. No argument = read-only.
		// 1 (default) = a held skinned mesh whose streamout has never been written is skinned
		// anyway; 0 = the pre-3.107 behaviour (it is drawn from the uninitialised buffer - the
		// one-frame splayed character). The report counts every such event either way and lists
		// the last 8, with how many rendered frames ago the ARMATURE got its first pose (0 = by a
		// render-less Scene::Update in the same frame - the mechanism behind report #4). Also the
		// render-side truth counters drawnGarbage / drawnStale, which must stay 0 with the guard on.
		if (arg && arg[0]) wi::scene::gg_skin_fresh_guard = (atoi(arg) != 0);
		wi::scene::gg_skin_fresh_report(result, (int)resultSize);
		return true;
	}
	if (_stricmp(cmd, "SET_SHADOWDISTFADE") == 0)
	{
		// GGMAX 3.108 (tester report #5): SET_SHADOWDISTFADE [0|1]. No argument = read-only.
		// 1 (default) = tree pool meshes dissolve their sun shadow with their distance fade;
		// 0 = the stock hard cut at the end of the fade (the pop). fadingCasters is this frame's count
		// of casters inside their fade band - the executed-check that the dither is being asked for.
		if (arg && arg[0]) wi::renderer::gg_shadow_distance_fade = (atoi(arg) != 0);
		_snprintf(result, resultSize,
			"SHADOW DISTANCE FADE (3.108): %d  fadingCasters=%u  shadowCasters=%u\n"
			"  tree lod_dist=%.0f  meshShadowReach(pool cap)=%.0f  TreeShadowLODDist=%.0f (stored %.0f)",
			wi::renderer::gg_shadow_distance_fade ? 1 : 0,
			wi::renderer::gg_dbg_shadow_fading.load(std::memory_order_relaxed),
			wi::renderer::gg_dbg_shadow_casters.load(std::memory_order_relaxed),
			GGTrees::ggtrees_global_params.lod_dist, GGTrees::GGTrees_GetMeshShadowReach(),
			GGTrees::ggtrees_global_params.lod_dist_shadow, t.visuals.fTreeShadowLODDist);
		result[resultSize - 1] = 0;
		return true;
	}

	// ========================================================================================
	// GGMAX 2.94: brutal off-switches + a keyed GPU-milliseconds readout.
	// FLAT `if (...) return true;` links on purpose - zero nesting depth. Do NOT add these to
	// the main else-if ladder in AutoHarness_CheckForCommand, which sits at the MSVC C1061
	// nesting limit and breaks the build if extended.
	// Replies ECHO A LIVE RE-READ of the flag, never the value passed in - the executed-check
	// convention that caught the SET_TREES pool no-op.
	// ========================================================================================
	{
		extern void GGSetNoTerrainLevel(int);
		extern void GGSetNoTreesLevel(int);
		extern void GGSetNoGrassLevel(int);
		extern void GGSetNoWaterLevel(int);
		// GGMAX 3.08 round-2 switches join the same table.
		extern void GGSetNoPostFXLevel(int);
		extern void GGSetNoAOLevel(int);
		extern void GGSetSimpleSkyLevel(int);
		extern void GGSetNoShadowsLevel(int);
		extern void GGSetNoOcclusionLevel(int);
		extern bool gg_no_postfx, gg_no_ao, gg_simple_sky, gg_no_shadows, gg_no_occlusion;
		// GGMAX 3.25: Terrain Bake / Water Bake replaced Terrain Off / Water Off in the panel.
		// The OLD names stay in the table: they drive the underlying removal and every existing
		// harness script and sweep tool uses them. The new names drive the panel switches, which
		// perform the removal AND put the cheap stand-in in its place.
		extern bool gg_terrain_bake, gg_water_bake;
		struct Sw { const char* name; void (*set)(int); bool* live; };
		static const Sw sws[11] = {
			{ "SET_BAKETERRAIN", GGHarness_SetTerrainBake, &gg_terrain_bake },
			{ "SET_BAKEWATER",   GGHarness_SetWaterBake,   &gg_water_bake   },
			{ "SET_TERRAINOFF", GGSetNoTerrainLevel, &gg_no_terrain },
			{ "SET_TREESOFF",   GGSetNoTreesLevel,   &gg_no_trees   },
			{ "SET_GRASSOFF",   GGSetNoGrassLevel,   &gg_no_grass   },
			{ "SET_WATEROFF",   GGSetNoWaterLevel,   &gg_no_water   },
			{ "SET_POSTFXOFF",  GGSetNoPostFXLevel,  &gg_no_postfx  },
			{ "SET_AOOFF",      GGSetNoAOLevel,      &gg_no_ao      },
			{ "SET_SIMPLESKY",  GGSetSimpleSkyLevel, &gg_simple_sky },
			{ "SET_SHADOWSOFF", GGSetNoShadowsLevel, &gg_no_shadows },
			{ "SET_OCCLUSIONOFF", GGSetNoOcclusionLevel, &gg_no_occlusion },
		};
		for (int i = 0; i < 11; i++)
		{
			if (_stricmp(cmd, sws[i].name) != 0) continue;
			const int on = (atoi(arg) != 0) ? 1 : 0;   // a missing arg reads 0 = ON (subsystem stays)
			sws[i].set(on);
			_snprintf(result, resultSize,
				"OK: %s %d - live flag now %d. Effect lands on the NEXT frame's update; give it "
				"a few frames before reading GET_GPUMS (terrain teardown joins the async VT job).",
				sws[i].name, on, *sws[i].live ? 1 : 0);
			result[resultSize - 1] = 0;
			return true;
		}
	}

	if (_stricmp(cmd, "SET_ENGINEGENGATE") == 0)
	{
		// GGMAX 2.94f: 1 = the terrain idle gate also suppresses the ENGINE's Generation_Update
		// (wiScene.cpp), which was ungated before 2.94f and walked all 625 chunks every frame
		// on a parked, settled scene while the bridge's own call skipped 7 frames in 8.
		// 0 = pre-2.94f behaviour, for the A/B.
		extern bool gg_engine_gen_gate;
		gg_engine_gen_gate = (atoi(arg) != 0);
		_snprintf(result, resultSize,
			"OK: SET_ENGINEGENGATE %d - live gg_engine_gen_gate=%d (%s). The gate only bites once "
			"the terrain has been calm >45 frames, and then 7 frames in 8.",
			atoi(arg), gg_engine_gen_gate ? 1 : 0,
			gg_engine_gen_gate ? "engine caller gated too (2.94f)" : "engine caller ungated (stock)");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_FASTTERRAIN") == 0)
	{
		// GGMAX 3.01 EXPERIMENT: "what would a baked terrain buy?" Combines the two levers that
		// together approximate one - without writing a baker:
		//   gg_near_ring_dist = 0  -> every chunk plain tex.Sample, no residency, all four VT
		//                             GPU passes skipped (2.94e measured this in isolation)
		//   generation         = N -> (2N+1)^2 chunks instead of 625, i.e. far fewer, bigger
		//                             scene entities to push through Scene::Update and culling
		// That is "a dumb floor with one texture" in everything but name. Terrain still provides
		// ground height, collision, and a surface for trees and grass.
		// arg = ring radius (3 => 49 chunks). 0 restores stock (gen 12, nearRing 4).
		wi::scene::Scene& fsc = wi::scene::GetScene();
		const int n = atoi(arg);
		extern void GGSetTerrainGen(int);
		GGSetTerrainGen(n);
		if (fsc.terrains.GetCount() > 0)
			fsc.terrains[0].gg_near_ring_dist = (n > 0) ? 0 : 4;
		_snprintf(result, resultSize,
			"OK: SET_FASTTERRAIN %d - nearRing=%d, ring will be (2n+1)^2 = %d chunks. "
			"NOT LIVE YET: send SET_TERRAINOFF 1, wait >=5s, SET_TERRAINOFF 0, wait ~30s for the "
			"rebuild, then confirm with TERRAIN_RING before trusting any timing.",
			n, (fsc.terrains.GetCount() > 0) ? fsc.terrains[0].gg_near_ring_dist : -1,
			(2 * (n ? n : 12) + 1) * (2 * (n ? n : 12) + 1));
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TREEPOOLCAP") == 0)
	{
		// GGMAX 2.97: radius (world units, 1 = 1 inch) beyond which the real-mesh tree pool stops
		// selecting, so the billboards take over cleanly instead of overlapping.
		//  <0 = derive from lod_dist (lod_dist + 2*500, DX11's arrangement)  [default]
		//   0 = uncapped, pre-2.97 behaviour
		//  >0 = explicit radius
		namespace GGT3 = GGTrees;
		GGT3::gg_tree_pool_max_dist = (float)atof(arg);
		_snprintf(result, resultSize,
			"OK: SET_TREEPOOLCAP %s - live gg_tree_pool_max_dist=%.0f (%s). lod_dist=%.0f, so the "
			"derived cap is %.0f units. Rebuilds on the next camera move or heartbeat; read POLYS "
			"and the pool counters to confirm.",
			arg, GGT3::gg_tree_pool_max_dist,
			GGT3::gg_tree_pool_max_dist < 0.0f ? "derive from lod_dist"
				: (GGT3::gg_tree_pool_max_dist == 0.0f ? "UNCAPPED" : "explicit"),
			GGT3::ggtrees_global_params.lod_dist,
			GGT3::ggtrees_global_params.lod_dist + 1000.0f);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_ELANIMFFCACHE") == 0)
	{
		// GGMAX 3.18: 1 (default) = read footfall step keyframes from a cached contiguous array.
		// 0 = walk the animation-set linked list every frame (pre-3.18). Watch ELANIM: ffRebuild
		// should settle to ~0 and ffHit should equal ffEnt once every object is cached.
		extern int gg_elanim_ff_cache;
		gg_elanim_ff_cache = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_ELANIMFFCACHE %d - %s.",
			gg_elanim_ff_cache,
			gg_elanim_ff_cache ? "cached step array" : "walking the linked list every frame");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_ELANIMFFCHARONLY") == 0)
	{
		// GGMAX 3.17: 1 = only CHARACTERS get the per-frame footfall keyframe scan. 0 = every
		// dynamic entity does (pre-3.17). Watch ELANIM: ffEnt/ffSets and P2-entity_loopanim.
		extern int gg_elanim_ff_charonly;
		gg_elanim_ff_charonly = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_ELANIMFFCHARONLY %d - footfall scan %s.",
			gg_elanim_ff_charonly,
			gg_elanim_ff_charonly ? "CHARACTERS ONLY" : "every dynamic entity (old behaviour)");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_ELANIMSKIPWORK") == 0)
	{
		// GGMAX 3.16 DIAGNOSTIC. 1 = entity_loopanim bails before the dynamic path, so
		// P2-entity_loopanim then measures only "walk every element and decide". CHANGES BEHAVIOUR
		// (animations/decals freeze) - attribution only, never leave it on.
		extern int gg_elanim_skipwork;
		gg_elanim_skipwork = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_ELANIMSKIPWORK %d - %s. Compare P2-entity_loopanim against ELANIM: work=N.",
			gg_elanim_skipwork,
			gg_elanim_skipwork ? "DYNAMIC PATH BYPASSED (diagnostic, anims frozen)" : "normal");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_WAYPOINTFAST") == 0)
	{
		// GGMAX 3.15: 1 (default) = skip the per-node engine intersect when the node cannot
		// possibly be under the cursor. 0 = intersect every waypoint node every frame (pre-3.15).
		extern int gg_waypoint_fastreject;
		gg_waypoint_fastreject = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_WAYPOINTFAST %d - %s. Watch P2M-T2-SelModes+Keys under P2-mainfunc; it was "
			"0.61 of 0.62 ms before this.",
			gg_waypoint_fastreject,
			gg_waypoint_fastreject ? "analytic reject ON" : "intersecting EVERY node (old behaviour)");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "TEST_ELANIMFFCACHE") == 0)
	{
		// GGMAX 3.19: prove the 3.18 footfall keyframe cache reproduces the live animation-set
		// list element by element and in order. 3.18's own evidence was a matching SET COUNT,
		// which a short, stale or mis-ordered array can also produce. Read-only.
		extern void GGFootfall_SelfTest(char*, int);
		GGFootfall_SelfTest(result, resultSize);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "TEST_WAYPOINTFAST") == 0)
	{
		// GGMAX 3.19: prove the 3.15 analytic reject can never skip a node the engine ray test
		// would have hit. The 3.15 argument was geometric (25 > 12.5*sqrt(3) = 21.65) and had
		// never been exercised, because nobody had hovered a waypoint on this build. Editor only,
		// one-shot, and it deliberately hitches - it runs both tests on every node.
		const char* state = AutoHarness_GetAppState();
		if (strcmp(state, "editor") != 0)
		{
			_snprintf(result, resultSize,
				"ERROR: TEST_WAYPOINTFAST needs the level editor (state: %s)", state);
		}
		else
		{
			extern void GGWaypoint_SelfTest(int, char*, int);
			int samples = atoi(arg);
			if (samples <= 0) samples = 27;   // 9 offsets x 3 axes
			GGWaypoint_SelfTest(samples, result, resultSize);
		}
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "TEST_LUAERRSET") == 0)
	{
		// GGMAX 3.24: prove the FunctionsWithErrors index is in sync with the list, and price
		// the lookup the old linear strcmp scan used to do on EVERY lua call in the game.
		extern void gg_TestFunctionErrors(char*, int);
		gg_TestFunctionErrors(result, resultSize);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_LUANAMECACHE") == 0)
	{
		// GGMAX 3.23: 1 (default) = cache the composed "<script>_main" name per entity.
		// 0 = rebuild it every frame with a cstr concat, for A/B. DUMP_LOGICCOST reports the
		// composing cost on its own line either way.
		extern int gg_luanamecache;
		gg_luanamecache = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_LUANAMECACHE %d - %s. Rebuilt whenever the entity's script name differs, so a\nruntime script swap is picked up next frame; a stale name would call the wrong _main and the\nbehaviour would visibly change, which is the loud failure mode a cache wants.",
			gg_luanamecache,
			gg_luanamecache ? "cached per entity" : "rebuilding every frame (pre-3.23)");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "TEST_INERTSKIP") == 0)
	{
		// GGMAX 3.22: prove the invariant the 3.22 skip rests on - an entity that cannot move
		// cannot have a stale position mirror. Read-only, one shot.
		extern void GGInertSkip_SelfTest(char*, int);
		GGInertSkip_SelfTest(result, resultSize);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_LOGICSKIP") == 0)
	{
		// GGMAX 3.22: 1 (default) = skip the per-entity body for INERT entities (no behaviour,
		// static, no waypoint zone, not animating, not a character). 0 = walk every entity as
		// before, for A/B. Read the effect in DUMP_LOGICCOST: "skipped as inert" and the drop
		// in no_behavior_selected.lua total.
		extern int gg_logic_skip_inert;
		gg_logic_skip_inert = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_LOGICSKIP %d - %s. An inert entity cannot move, so the entityelement x/y/z\nmirror the rest of the engine reads cannot go stale while it is skipped; that is why the\npredicate insists on static rather than merely script-less.",
			gg_logic_skip_inert,
			gg_logic_skip_inert ? "skipping inert entities" : "walking every entity (pre-3.22)");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "DUMP_LOGICCOST") == 0)
	{
		// GGMAX 3.21: where the per-frame LUA logic cost actually goes, aggregated BY SCRIPT.
		// The producelogfiles=3 box only ever showed a top-ten per ENTITY, which on a level with
		// hundreds of scripted entities is a rounding error - Lee's Canyon shot listed nine rows
		// totalling 0.147 ms against a 1.17 ms LUA row.
		// Two-step by design: this call ARMS a one-frame aggregation, the NEXT call returns it.
		// Aggregating every frame would mean a strcmp per entity per script per frame.
		// ⚠ Deliberately does NOT raise the MessageBox the producelogfiles path uses - a modal
		// box on a harness run reads as a hang.
		// GGMAX 3.21: IDEMPOTENT ON PURPOSE. The first version consumed the report on read
		// (returned it, then blanked it) and it never once produced data from a script, while
		// the same code lit up Lee's message box perfectly. The harness re-executes a command
		// file that is still sitting there, so between my "arm" call and my "read" call the
		// command ran again by itself, consumed the report, and wrote it to an auto_result.txt
		// nobody was listening to - see feedback_stale_auto_command. A harness command must be
		// safe to run twice. This one now re-arms every call and never clears anything: repeats
		// are free and the caller just gets the most recent completed frame.
		extern volatile int gg_logiccost_arm;
		extern int gg_logiccost_used, gg_logiccost_considered;
		extern char gg_logiccost_report[16384];
		const int wasArmed = gg_logiccost_arm;
		gg_logiccost_arm = 1;                 // always re-arm, so a second call is a fresh frame
		if (gg_logiccost_report[0] == 0)
		{
			_snprintf(result, resultSize,
				"ARMED: no completed frame yet. Send DUMP_LOGICCOST again in a second.\n"
				"  (diag: arm was %d, last pass saw %d entities and %d distinct scripts. If arm was\n"
				"   already 1 and entities is 0, lua_loop_allentities is not running - you are\n"
				"   probably in the editor, where entity logic does not run.)",
				wasArmed, gg_logiccost_considered, gg_logiccost_used);
		}
		else
		{
			_snprintf(result, resultSize, "%s", gg_logiccost_report);
		}
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "DUMP_IDLEPEAKS") == 0)
	{
		// GGMAX 3.20a: what a "hide rows at 0.00" rule can actually reach here. A dump sample
		// sees one frame in a couple of hundred, so a row crossing the display threshold every
		// few seconds reads as permanently 0.00 in the sample and is not - that mis-estimate was
		// 37 rows against a true 11. This reports the PEAK printable time per row over every
		// frame, plus how many rows each candidate threshold would take.
		if (!wi::profiler::IsEnabled())
			_snprintf(result, resultSize, "ERROR: DUMP_IDLEPEAKS needs the profiler - send ENABLE_PROFILER, soak, retry");
		else
			_snprintf(result, resultSize, "%s", wi::profiler::gg_GetIdleRowReport().c_str());
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_HIDEIDLEROWS") == 0)
	{
		// GGMAX 3.20: the Performance panel's "Hide idle rows" tick box, drivable from here so
		// the STABILITY of the shortened list can be measured rather than eyeballed - the whole
		// point of the feature is that the row set holds still, and that is a claim about many
		// consecutive dumps, not about one screenshot.
		// ⚠ The hide latch needs ~600 consecutive frames at 0.00 before it fires, so a dump
		// taken immediately after enabling this will report 0 hidden if the profiler was only
		// just switched on. Leave the level running, then read GET_PERF_DATA.
		// GGMAX 3.20a: optional second arg is the ACTIVITY THRESHOLD in ms. Default 0.005 is the
		// display threshold ("would print 0.00"), which reaches only ~8 rows because most of the
		// 0.00 rows do cross it briefly. Raise it to turn the control into "hide rows that never
		// cost anything worth reading" - measured on Canyon, 0.02 takes ~20 and 0.05 takes ~39.
		int onoff = 0; float thresh = -1.0f;
		if (sscanf_s(arg, "%d %f", &onoff, &thresh) >= 1)
		{
			wi::profiler::gg_hide_idle_rows = (onoff != 0);
			if (thresh >= 0.0f) wi::profiler::gg_idle_row_ms = thresh;
		}
		_snprintf(result, resultSize,
			"OK: SET_HIDEIDLEROWS %d (threshold %.4f ms) - %s. %u rows were suppressed on the last "
			"panel refresh. A row is hidden after ~600 consecutive frames below the threshold; the "
			"moment it crosses it the row comes back and is pinned visible for the session, which is "
			"what stops the list shifting. Use DUMP_IDLEPEAKS to see what any threshold would take.",
			wi::profiler::gg_hide_idle_rows ? 1 : 0,
			wi::profiler::gg_idle_row_ms,
			wi::profiler::gg_hide_idle_rows ? "idle rows hidden" : "every row shown (the pre-3.20c default)",
			wi::profiler::gg_hidden_row_count);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_HIERCACHE") == 0)
	{
		// GGMAX 3.14: 1 (default) = skip the topdown-hierarchy rebuild when the hierarchy has not
		// changed. 0 = rebuild every frame (pre-3.14 behaviour), for A/B.
		wi::scene::gg_hier_cache_snapshot = (atoi(arg) != 0) ? 1 : 0;
		_snprintf(result, resultSize,
			"OK: SET_HIERCACHE %d - %s. Read HIER: rebuilds/skips in GET_PERF_DATA; with it ON and a "
			"static hierarchy, rebuilds should stop climbing while skips climb.",
			wi::scene::gg_hier_cache_snapshot,
			wi::scene::gg_hier_cache_snapshot ? "skipping redundant rebuilds" : "rebuilding EVERY frame");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TEXTUREDIVIDE") == 0)
	{
		// GGMAX 3.12: 1 = full, 2 = half, 4 = quarter.
		// GGMAX 3.19: applies LIVE now - the resident textures are re-created at the new size on
		// the next frame's update, and the terrain's virtual-texture tiles are re-baked with them.
		// gg_texture_divide_rebuilt reports what the last apply actually touched, so read this
		// command's reply, then GET_TEXTUREDIVIDE (or any later reply) to see the count.
		extern void GGSetTextureDivideLive(int);
		extern uint32_t gg_texture_divide_rebuilt;
		extern uint32_t gg_texture_divide_vtrepaints;
		GGSetTextureDivideLive(atoi(arg));
		_snprintf(result, resultSize,
			"OK: SET_TEXTUREDIVIDE %d - live gg_texture_divide=%d. Applies to the CURRENT level on "
			"the NEXT update, so these numbers are the PREVIOUS apply: rebuilt %u textures, "
			"skipped %u, %.1f MB -> %.1f MB of texture resource, %u terrain chunk VTs re-baked. "
			"Send the command a second time to "
			"read the apply you just asked for. Driver VRAM will not track this closely - the "
			"allocator keeps freed heaps, and in the editor streaming may already have walked "
			"these textures below the divided size. GG's tree billboard atlases and grass load "
			"outside the resource manager and are NOT affected.",
			atoi(arg), wi::resourcemanager::gg_texture_divide, gg_texture_divide_rebuilt,
			wi::resourcemanager::gg_texdivide_skipped.load(),
			wi::resourcemanager::gg_texdivide_bytes_before.load() / 1048576.0,
			wi::resourcemanager::gg_texdivide_bytes_after.load() / 1048576.0,
			gg_texture_divide_vtrepaints);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_OBJCULLDIST") == 0)
	{
		// GGMAX 3.11: cap every object's draw distance, in world inches. 0 = off.
		// Objects that already own a finite draw_distance (the 3.03 tree pool) are left alone.
		extern void GGSetObjectCullDistLevel(int);
		extern float gg_object_cull_dist;
		GGSetObjectCullDistLevel(atoi(arg));
		_snprintf(result, resultSize,
			"OK: SET_OBJCULLDIST %d - live gg_object_cull_dist=%.0f units (%.0f m). 0 = off. Objects "
			"dissolve out over their own radius rather than popping. Watch POLYS and DRAW calls.",
			atoi(arg), gg_object_cull_dist, gg_object_cull_dist / 39.37f);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_PARTICLEPCT") == 0)
	{
		// GGMAX 3.09: emit-rate scale for Wicked emitters, 0..100 percent. 100 = untouched.
		// ⚠ Wicked emitters ONLY - the legacy gpup/.arx and WPE systems are not affected.
		extern void GGSetParticlePctLevel(int);
		extern int gg_particle_pct;
		GGSetParticlePctLevel(atoi(arg));
		_snprintf(result, resultSize,
			"OK: SET_PARTICLEPCT %d - live gg_particle_pct=%d%%. Applies to WICKED emitters only "
			"(gpup/.arx and WPE untouched). Existing particles live out their lifetime, so give it "
			"a second before reading; check the EmittedParticles rows.",
			atoi(arg), gg_particle_pct);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TREESHADEWRAP") == 0)
	{
		// GGMAX 3.07: 0..1 wrap strength for the SHADED side of distant-tree billboards.
		// DEFAULT 0.5. 0 = pre-3.07, bit-identical, so it doubles as the A/B control.
		// Lifts the darks without flattening the normal-map shading
		// or dimming the lit side. Live next frame (one CB write, no rebind, no pool churn).
		namespace GGT7 = GGTrees;
		float w = (float)atof(arg);
		if (w < 0.0f) w = 0.0f;
		if (w > 1.0f) w = 1.0f;
		GGT7::gg_tree_shade_wrap = w;
		_snprintf(result, resultSize,
			"OK: SET_TREESHADEWRAP %.2f - %s. Measured on the vine-tree billboard under a high sun: "
			"w=0 leaves the dark tail at 0.14 with 10%% very dark; w=0.30 lifts it to 0.34 with 1.8%%; "
			"w=0.50 to 0.42 with 0%%. Mean lit barely moves (0.982 -> 0.985), so only the SHADE changes.",
			GGT7::gg_tree_shade_wrap,
			GGT7::gg_tree_shade_wrap <= 0.0f ? "OFF, identical to pre-3.07" : "wrap active");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TREEDEBUGSOLID") == 0)
	{
		// GGMAX 3.05 DEBUG. 1 = every distant-tree billboard draws as a flat unlit grey quad: no
		// texture fetch, no alpha cutout, no lighting, no fog. The LOD dither discard is KEPT, so
		// the mesh->billboard handover still happens and can be watched geometrically. Applied
		// identically in the colour PS and the prepass PS so the two coverages cannot diverge.
		// 0 = normal rendering (default). Also setup.ini `treedebugsolid`.
		namespace GGT6 = GGTrees;
		GGT6::gg_tree_debug_solid = atoi(arg);
		_snprintf(result, resultSize,
			"OK: SET_TREEDEBUGSOLID %d - billboards draw as %s. Live next frame (one CB write). "
			"lod_dist=%.0f so quads dissolve IN over %.0f..%.0f; real meshes are UNAFFECTED and still "
			"dissolve out via 3.03. Set 0 before any visual or performance testing.",
			GGT6::gg_tree_debug_solid,
			GGT6::gg_tree_debug_solid == 0 ? "normal textured billboards"
				: (GGT6::gg_tree_debug_solid == 1 ? "FLAT GREY QUADS + dissolve"
				: (GGT6::gg_tree_debug_solid == 2 ? "FLAT GREY QUADS, NO discard in either pass"
				: "GREY + RED where the dissolve would discard")),
			GGT6::ggtrees_global_params.lod_dist,
			GGT6::ggtrees_global_params.lod_dist,
			GGT6::ggtrees_global_params.lod_dist + 500.0f);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TREEPREPASSREACH") == 0)
	{
		// GGMAX 3.04 DIAGNOSTIC. 1 (default, correct) = the billboard PREPASS applies the same
		// tree_terrainReach clip the colour VS does. 0 reproduces the 3.00-3.03 defect: the prepass
		// draws quads the colour pass clips away, so depth is written, the gbuffer at those pixels
		// is never filled, and the tree shades BLACK. Kept as a regression lever because "a
		// visibility test added to one pass and not its partner" has now bitten this codebase twice.
		namespace GGT5 = GGTrees;
		GGT5::gg_tree_prepass_reach = (atoi(arg) != 0);
		_snprintf(result, resultSize,
			"OK: SET_TREEPREPASSREACH %d - prepass %s the terrain-reach clip. %s Live next frame "
			"(one CB write, no rebind, no pool churn needed).",
			GGT5::gg_tree_prepass_reach ? 1 : 0,
			GGT5::gg_tree_prepass_reach ? "APPLIES" : "SKIPS",
			GGT5::gg_tree_prepass_reach ? "Correct." : "DEFECT REPRODUCED: expect black trees past the terrain edge.");
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TREEMESHFADE") == 0)
	{
		// GGMAX 3.03: distance (world units) at which a real-mesh tree STARTS dissolving out, so it
		// cross-fades into its billboard instead of popping off at the pool cap. Wicked fades over
		// [d, d + object.radius] and culls past it (wiRenderer.cpp:4237/8760).
		//  <0 = derive lod_dist + 500 (DX11's arrangement)  [default]
		//   0 = no fade, hard pop at the pool cap (pre-3.03)
		//  >0 = explicit distance
		// ⚠ applied per pool BIND, so an existing binding keeps its old value until the slot rebinds
		// - move the camera far enough to churn the pool before reading the result.
		namespace GGT4 = GGTrees;
		GGT4::gg_tree_mesh_fade_dist = (float)atof(arg);
		const float lodd = GGT4::ggtrees_global_params.lod_dist;
		_snprintf(result, resultSize,
			"OK: SET_TREEMESHFADE %s - live gg_tree_mesh_fade_dist=%.0f (%s). lod_dist=%.0f: billboards "
			"dissolve IN over %.0f..%.0f, mesh %s. Pool cap %.0f. "
			"Applied per pool BIND - churn the pool (fly out past the cap and back) before reading.",
			arg, GGT4::gg_tree_mesh_fade_dist,
			GGT4::gg_tree_mesh_fade_dist < 0.0f ? "derive lod_dist+500"
				: (GGT4::gg_tree_mesh_fade_dist == 0.0f ? "NO FADE (hard pop)" : "explicit"),
			lodd, lodd, lodd + 500.0f,
			GGT4::gg_tree_mesh_fade_dist == 0.0f
				? "does NOT dissolve - hard SetRenderable(false) at the pool cap"
				: "dissolves OUT ending at the pool cap, starting one object radius before it (>= lod_dist)",
			GGT4::gg_tree_pool_max_dist > 0.0f ? GGT4::gg_tree_pool_max_dist : lodd + 1000.0f);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_FARTREES") == 0)
	{
		// GGMAX 2.95: 1 (default) = the merged billboard proxy chunks also draw to the MAIN
		// camera beyond the nearest-N pool radius, restoring DX11's forested distance.
		// 0 = pre-2.95 behaviour (shadow-only proxies, bare distant hillsides).
		// GGMAX 2.96: now drives the DX11-style BILLBOARD PASS (zero ECS, one draw per chunk),
		// not the abandoned merged-proxy experiment from 2.95b.
		namespace GGT2 = GGTrees;
		extern bool gg_trees_far_billboards;
		if (arg && arg[0]) GGT2::gg_far_tree_pass = (atoi(arg) != 0);   // GGMAX 3.102: no argument = READ ONLY (it used to switch the pass off)
		int pc = 0, ps = 0, cand = 0, pb = 0, psz = 0; float cut = -1.0f;
		namespace GGT = GGTrees;
		GGT::GGTrees_GetFarTreeStats(&pc, &ps, &cand, &pb, &psz, &cut);
		extern char gg_ftLoadReason[];
		int vp = 0; float nch = -1.0f, fch = -1.0f;
		extern uint32_t gg_ddsSliceRefused; extern char gg_ddsSliceRefusedWhat[];   // GGMAX 3.102
		char ftbuf[640];
		_snprintf(ftbuf, sizeof(ftbuf),
			"BILLBOARD PASS: entered=%u draws=%u instances=%u atlasOK=%u/typesUsed=%u | chunksTotal=%u withInstances=%u frustumKilled=%u noTerrainKilled=%u | firstFail=%s\nATLAS (3.102): sliceCopiesRefused=%u%s%s rebuildsForNewType=%u typeScans=%u",
			GGT2::g_ftEnterCount, GGT2::g_ftDrawCalls, GGT2::g_ftInstances, GGT2::g_ftAtlasSlices, GGT2::g_ftTypesUsed,
			GGT2::g_ftChunksTotal, GGT2::g_ftChunksWithIn, GGT2::g_ftFrustumKills, GGT2::g_ftTerrainKills,
			gg_ftLoadReason[0] ? gg_ftLoadReason : "(none)",
			gg_ddsSliceRefused, gg_ddsSliceRefused ? " first=" : "", gg_ddsSliceRefused ? gg_ddsSliceRefusedWhat : "",
			GGT2::g_ftAtlasRebuilds, GGT2::g_ftTypeScans);
		ftbuf[sizeof(ftbuf)-1] = 0;
		GGT::GGTrees_GetFarTreeRange(&vp, &nch, &fch);
		_snprintf(result, resultSize,
			"OK: SET_FARTREES %d - live gg_trees_far_billboards=%d (%s)\n"
			"proxyChunks=%d validProxies=%d proxiesShown=%d candidates=%d poolBuilt=%d poolSize=%d\n"
			"cutoffDist=%.0f  validProxyChunkDist: nearest=%.0f farthest=%.0f\n"
			"%s\n"
			"READ THIS: cutoffDist -1 means the gather found FEWER trees than the pool holds, so "
			"every tree is already a real mesh and there is no 'far' for billboards to cover. "
			"proxyChunks 0 means no billboard proxies were built at all.",
			atoi(arg), GGT2::gg_far_tree_pass ? 1 : 0,
			GGT2::gg_far_tree_pass ? "billboard PASS ON" : "billboard pass off",
			pc, vp, ps, cand, pb, psz, (cut >= 0.0f) ? sqrtf(cut) : -1.0f, nch, fch, ftbuf);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TERRAINBAKE") == 0)
	{
		// GGMAX 2.94e: puts every terrain chunk into the exact runtime state Lee's proposed
		// "terrain bake" would produce - WITHOUT writing a baker.
		//
		// gg_near_ring_dist is the radius (in chunks) inside which a chunk gets max_resolution
		// and a RESIDENCY object. Set it to 0 and every chunk takes min_resolution instead,
		// gets no residency, is therefore skipped by all four VT GPU passes (each opens with
		// `if (vt->residency == nullptr) continue;`), and has its material bound with
		// sparse_residencymap_descriptor = -1 (wiTerrain.cpp:2102) so the pixel shader falls
		// through to a plain tex.Sample. That is "dumb mesh + basic texture", exactly, and it
		// costs one integer - no shader permutation, no PSO change, no assets.
		//
		// arg 0 = restore GGMAX default (4), 1 = bake-equivalent (0), or any explicit radius.
		wi::scene::Scene& bsc = wi::scene::GetScene();
		if (bsc.terrains.GetCount() == 0)
		{
			_snprintf(result, resultSize, "ERROR: SET_TERRAINBAKE - no terrain in the scene (is Terrain Off set?)");
			result[resultSize - 1] = 0;
			return true;
		}
		wi::terrain::Terrain& btr = bsc.terrains[0];
		// GGMAX 3.02: second arg = per-chunk texture resolution in bake mode (256 default).
		// WARNING anything above 256 is more than one tile and brings residency - and therefore
		// the VT passes - straight back. Measure it; do not assume.
		int m = 0, bakeRes = 0;
		sscanf_s(arg, "%d %d", &m, &bakeRes);
		wi::terrain::gg_bake_resolution = (bakeRes > 0) ? (uint32_t)bakeRes : 0u;
		btr.gg_near_ring_dist = (m == 1) ? 0 : ((m == 0) ? 4 : m);
		int resVTs = 0, totVTs = 0;
		for (const auto* vt : btr.virtual_textures_in_use) { totVTs++; if (vt->residency != nullptr) resVTs++; }
		_snprintf(result, resultSize,
			"OK: SET_TERRAINBAKE %d (res %u) - live gg_near_ring_dist=%d chunks=%d VTs=%d residencyVTs=%d. "
			"EXECUTED-CHECK: residencyVTs must fall to 0 in bake mode, but only Generation_Update "
			"performs the downgrade - allow ~20s and RE-READ before trusting any timing.",
			m, wi::terrain::gg_bake_resolution ? wi::terrain::gg_bake_resolution : 256u,
			btr.gg_near_ring_dist, (int)btr.chunks.size(), totVTs, resVTs);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_TERRAINGEN") == 0)
	{
		// GGMAX 2.94e: set the chunk-ring radius, then REBUILD the terrain so it takes effect.
		// terrain.generation is only read inside GGTerrainWicked_Init, so a runtime write to
		// g_terrainGenOverride alone is inert - the Terrain Off switch's teardown/re-init is
		// what makes it live. Used as a cheap PROXY for the terrain-bake question: chunk count
		// is (2n+1)^2, so this scales entity count without writing a baker.
		// WARNING lower is NOT free - the ring is what gives terrain its view distance.
		extern void GGSetTerrainGen(int);
		extern bool gg_no_terrain;
		extern void GGSetNoTerrainLevel(int);
		const int n = atoi(arg);
		GGSetTerrainGen(n);
		extern int g_terrainGenOverride;
		const int eff = g_terrainGenOverride ? g_terrainGenOverride : 12;
		// This command ONLY sets the override. It deliberately does NOT cycle the terrain for
		// you: the Terrain Off teardown is EDGE-TRIGGERED inside GGTerrainWicked_Update, so
		// setting the flag on and off inside one harness command means no frame ever observes
		// it set and nothing rebuilds. Measured 2026-08-22 - a whole gen ladder ran and every
		// rung reported chunks=625. The caller must send SET_TERRAINOFF 1, WAIT for frames,
		// then SET_TERRAINOFF 0.
		_snprintf(result, resultSize,
			"OK: SET_TERRAINGEN %d - live g_terrainGenOverride=%d (effective gen %d, ring (2n+1)^2 = %d chunks). "
			"NOT YET LIVE: now send SET_TERRAINOFF 1, wait >=5s, SET_TERRAINOFF 0, wait ~30s for the rebuild. "
			"Verify with TERRAIN_RING in GET_PERF_DATA before trusting any timing.",
			n, g_terrainGenOverride, eff, (2 * eff + 1) * (2 * eff + 1));
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_VTWRITEBACK") == 0)
	{
		// GGMAX 2.94d: cadence of the terrain VT tile-request round trip, in frames.
		// 1 = stock every-frame upstream behaviour, 4 = shipped default. A/B this against 1 to
		// re-measure the saving; the pair (Allocate + Writeback) shares the cadence by design.
		const int n = atoi(arg);
		wi::terrain::gg_vt_writeback_interval = (n < 1) ? 1 : n;
		_snprintf(result, resultSize,
			"OK: SET_VTWRITEBACK %d - live gg_vt_writeback_interval=%d (1 = every frame / stock)",
			n, wi::terrain::gg_vt_writeback_interval);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "DUMP_GPUGAPS") == 0)
	{
		if (!wi::profiler::IsEnabled())
		{
			_snprintf(result, resultSize, "ERROR: DUMP_GPUGAPS needs the profiler - send ENABLE_PROFILER, wait 5s, retry");
			result[resultSize - 1] = 0;
			return true;
		}
		// GGMAX 2.94c: names WHERE the GPU dead time sits. GPU Busy/Idle say how much; this
		// says between which two ranges. Latest frame only, so read it on a parked camera.
		const std::string rep = wi::profiler::GetGPUGapReport();
		_snprintf(result, resultSize, "%s", rep.empty() ? "(no gap data yet - let a frame resolve)" : rep.c_str());
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "GET_GPUMS") == 0)
	{
		if (!wi::profiler::IsEnabled())
		{
			_snprintf(result, resultSize,
				"ERROR: GET_GPUMS needs the profiler - send ENABLE_PROFILER, wait 5s, retry");
			result[resultSize - 1] = 0;
			return true;
		}
		extern uint64_t GGPerf_GetPolyCount();
		const float fps = (ImGui::GetCurrentContext() != nullptr) ? ImGui::GetIO().Framerate : 0.0f;
		// GPU_BUSY is THE metric, not GPU_FRAME. GPU Frame's begin query rides the frame's first
		// command list and its end query the last, so on a paced frame it tends to the frame
		// PERIOD (63.8 fps <-> 15.68 ms is the same number twice). Busy is the union of the
		// child ranges and is the only one of the three that measures actual GPU work.
		_snprintf(result, resultSize,
			"GPU_FRAME_MS: %.3f\n"
			"GPU_BUSY_MS: %.3f\n"
			"GPU_IDLE_MS: %.3f\n"
			"CPU_FRAME_MS: %.3f\n"
			"FPS: %.1f\n"
			"POLYS: %llu\n"
			"OFF: terrain=%d trees=%d grass=%d water=%d\n",
			wi::profiler::GetGPUFrameTime(),
			wi::profiler::gg_gpu_busy_time,
			wi::profiler::gg_gpu_idle_time,
			wi::profiler::GetCPUFrameTime(),
			fps, (unsigned long long)GGPerf_GetPolyCount(),
			gg_no_terrain ? 1 : 0, gg_no_trees ? 1 : 0, gg_no_grass ? 1 : 0, gg_no_water ? 1 : 0);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "SET_CAMERALOG") == 0)
	{
		// SET_CAMERALOG <0|1>: the camera call log (CCameraC_part0.cpp: every camera position and rotation set, with the game loop
		// stage and the caller), written to camera_log.txt beside the exe for the frames round a player hit or a script's
		// CameraLogMark; DUMP_CAMERALOG writes it now (2026-10-21, the vehicle camera's hit-frame jump)
		extern bool g_bCameraLog;
		int on = atoi(arg) != 0 ? 1 : 0;
		g_bCameraLog = on != 0;
		_snprintf(result, resultSize, "OK: SET_CAMERALOG %d", on);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_CAMERALOG") == 0)
	{
		extern void CameraLogDump(const char* pWhy);
		CameraLogDump("on demand");
		_snprintf(result, resultSize, "OK: camera_log.txt written beside the exe");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PROBECAPTURE") == 0)
	{
		// SET_PROBECAPTURE <0|1>: environment probe captures on or held (wi::renderer::gg_probe_capture). With 0 the probes
		// keep their last cube; a lever for the frames drawn from another direction while lights switch (2026-10-21)
		int on = atoi(arg) != 0 ? 1 : 0;
		wi::renderer::gg_probe_capture = on != 0;
		_snprintf(result, resultSize, "OK: SET_PROBECAPTURE %d", on);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_TAA") == 0)
	{
		// SET_TAA <0|1>: the engine's temporal AA, as the game's SetTAA sets it (for the editor, where RUN_LUA has no state)
		extern void LuaRenderSettings_SetTAA(int iOn);
		int on = atoi(arg) != 0 ? 1 : 0;
		LuaRenderSettings_SetTAA(on);
		_snprintf(result, resultSize, "OK: SET_TAA %d", on);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_REFLTRANSPARENT") == 0)
	{
		// SET_REFLTRANSPARENT <0|1>: whether the planar reflection draws the transparent objects (an A/B lever)
		int on = atoi(arg) != 0 ? 1 : 0;
		wi::renderer::gg_reflection_transparent = on;
		_snprintf(result, resultSize, "OK: SET_REFLTRANSPARENT %d", on);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_RAYCOST") == 0)
	{
		// DUMP_RAYCOST: the last 64 ray picks, grouped by the object that took the most triangles of each: rays, triangles
		// tested (skinned), and the object's name
		wi::scene::Scene& rs = wi::scene::GetScene();
		struct RayCostRow { wi::ecs::Entity entity; int rays; unsigned long long tris; unsigned long long skin; unsigned long long dearest; };
		std::vector<RayCostRow> rows;
		unsigned long long totalTris = 0, totalSkin = 0; int totalRays = 0;
		for (uint32_t i = 0; i < wi::scene::Scene::GG_RAYCOST_RING; i++)
		{
			const wi::scene::Scene::GGRayCost& c = wi::scene::Scene::gg_raycost_ring[i];
			if (c.tris == 0 && c.candidates == 0) continue;
			totalRays++; totalTris += c.tris; totalSkin += c.skintris;
			bool bFound = false;
			for (auto& r : rows)
				if (r.entity == c.dearest) { r.rays++; r.tris += c.tris; r.skin += c.skintris; r.dearest += c.dearest_tris; bFound = true; break; }
			if (!bFound) rows.push_back({ c.dearest, 1, c.tris, c.skintris, c.dearest_tris });
		}
		std::sort(rows.begin(), rows.end(), [](const RayCostRow& a, const RayCostRow& b) { return a.dearest > b.dearest; });
		int written = _snprintf(result, resultSize, "OK: DUMP_RAYCOST last %d rays: %llu triangles (%llu skinned)\n", totalRays, totalTris, totalSkin);
		for (size_t i = 0; i < rows.size() && i < 10 && written > 0 && (size_t)written < resultSize - 1; i++)
		{
			const char* pName = "(none)";
			const wi::scene::NameComponent* pNC = rs.names.GetComponent(rows[i].entity);
			if (pNC) pName = pNC->name.c_str();
			// an object with no name of its own: the entity of the GameGuru object that owns it, by its frame
			cstr sOwner = "";
			if (!pNC || pNC->name.empty() || pNC->name == "node_mesh")
			{
				for (int e = 1; e <= g.entityelementlist && sOwner.Len() == 0; e++)
				{
					int iObj = t.entityelement[e].obj;
					if (iObj <= 0 || ObjectExist(iObj) == 0) continue;
					sObject* pObj = GetObjectData(iObj);
					if (!pObj || !pObj->ppFrameList) continue;
					for (int iF = 0; iF < pObj->iFrameCount; iF++)
						if (pObj->ppFrameList[iF] && pObj->ppFrameList[iF]->wickedobjindex == rows[i].entity) { sOwner = cstr("entity ") + Str(e) + " obj " + Str(iObj) + " " + t.entityelement[e].eleprof.name_s; break; }
				}
			}
			// and the mesh behind it: its triangles, whether it has a BVH or bone clusters, whether it is a terrain chunk
			cstr sMesh = "";
			const wi::scene::ObjectComponent* pOC = rs.objects.GetComponent(rows[i].entity);
			if (pOC)
			{
				const wi::scene::MeshComponent* pMC = rs.meshes.GetComponent(pOC->meshID);
				if (pMC)
				{
					bool bChunk = false;
					for (size_t ti = 0; ti < rs.terrains.GetCount() && !bChunk; ++ti)
						for (auto& it : rs.terrains[ti].chunks)
							if (it.second.entity == rows[i].entity) { bChunk = true; break; }
					const wi::scene::NameComponent* pMN = rs.names.GetComponent(pOC->meshID);
					sMesh = cstr(" [mesh ") + (pMN ? pMN->name.c_str() : "") + " " + Str((int)(pMC->indices.size() / 3)) + " tris, lods " + Str((int)pMC->GetLODCount())
						+ (pMC->bvh.IsValid() ? ", bvh" : ", no bvh") + (pMC->IsSkinned() ? ", skinned" : "") + (pMC->gg_bone_clusters_built ? cstr(", clusters ") + Str((int)pMC->gg_bone_clusters.size()) : cstr(""))
						+ (bChunk ? ", terrain chunk" : "") + "]";
				}
			}
			written += _snprintf(result + written, resultSize - written, "  %d rays, %llu tris (%llu skinned), dearest object took %llu: %s %s%s\n",
				rows[i].rays, rows[i].tris, rows[i].skin, rows[i].dearest, pName, sOwner.Get(), sMesh.Get());
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_RAYBLOCK") == 0)
	{
		// DUMP_RAYBLOCK: the object that swallowed the last pick (IntersectAllEx's closest hit outside the entity object
		// range, reported to the caller as a plain miss), with where it is and what it is
		extern DWORD g_ggLastRayBlockedBy;
		extern int g_iCurrentGunObj;
		DWORD iObj = g_ggLastRayBlockedBy;
		int written = _snprintf(result, resultSize, "OK: DUMP_RAYBLOCK blockedby=%u gunobj=%d entityrange=%d..%d\n",
			(unsigned)iObj, g_iCurrentGunObj, g.entityviewstartobj, g.entityviewendobj);
		if (iObj > 0 && ObjectExist((int)iObj) == 1)
		{
			sObject* pObj = GetObjectData((int)iObj);
			const char* pName = "";
			if (pObj && pObj->ppFrameList && pObj->iFrameCount > 0 && pObj->ppFrameList[0])
			{
				const wi::scene::NameComponent* pNC = wi::scene::GetScene().names.GetComponent(pObj->ppFrameList[0]->wickedobjindex);
				if (pNC) pName = pNC->name.c_str();
			}
			int iOwner = 0;
			for (int e = 1; e <= g.entityelementlist; e++) if (t.entityelement[e].obj == (int)iObj) { iOwner = e; break; }
			written += _snprintf(result + written, resultSize - written, "  object %u at %.0f,%.0f,%.0f radius %.0f visible=%d frames=%d name=%s owner entity=%d\n",
				(unsigned)iObj, pObj->position.vecPosition.x, pObj->position.vecPosition.y, pObj->position.vecPosition.z, pObj->collision.fRadius,
				GetVisible((int)iObj), pObj->iFrameCount, pName, iOwner);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_LODSTATS") == 0)
	{
		// DUMP_LODSTATS: the meshes by LOD count, and the objects with LODs by the LOD they draw at this frame
		wi::scene::Scene& ls = wi::scene::GetScene();
		int meshByLod[5] = {};
		int objByLod[5] = {};
		int objLodded = 0;
		for (size_t i = 0; i < ls.meshes.GetCount(); ++i)
		{
			uint32_t n = ls.meshes[i].GetLODCount();
			if (n > 4) n = 4;
			meshByLod[n]++;
		}
		for (size_t i = 0; i < ls.objects.GetCount(); ++i)
		{
			const wi::scene::ObjectComponent& o = ls.objects[i];
			if (o.mesh_index >= ls.meshes.GetCount()) continue;
			if (ls.meshes[o.mesh_index].GetLODCount() <= 1) continue;
			objLodded++;
			uint32_t l = o.lod;
			if (l > 4) l = 4;
			objByLod[l]++;
		}
		_snprintf(result, resultSize, "OK: DUMP_LODSTATS meshes=%d lodcount1=%d lodcount2=%d lodcount3=%d lodcount4=%d  objects_with_lods=%d at_lod0=%d lod1=%d lod2=%d lod3=%d  lod_step=%.0f reflections_lowest_lod=%d  batches main_opaque=%d main_transparent=%d refl_opaque=%d refl_transparent=%d",
			(int)ls.meshes.GetCount(), meshByLod[1], meshByLod[2], meshByLod[3], meshByLod[4],
			objLodded, objByLod[0], objByLod[1], objByLod[2], objByLod[3],
			wi::scene::gg_lod_step, wi::renderer::gg_reflection_lowest_lod,
			wi::renderer::gg_dbg_main_opaque_batches, wi::renderer::gg_dbg_main_transparent_batches,
			wi::renderer::gg_dbg_refl_opaque_batches, wi::renderer::gg_dbg_refl_transparent_batches);
		result[resultSize - 1] = 0;
		return true;
	}

	if (_stricmp(cmd, "DUMP_MESHES") != 0) return false;

	wi::scene::Scene& mc = wi::scene::GetScene();

	// meshIDs referenced by objects, sorted, so per-mesh ref counting is a binary search
	// rather than an O(meshes x objects) scan (2400 x 8900 on a full level).
	std::vector<uint64_t> objMesh;
	objMesh.reserve(mc.objects.GetCount());
	for (size_t i = 0; i < mc.objects.GetCount(); ++i)
		objMesh.push_back((uint64_t)mc.objects[i].meshID);
	std::sort(objMesh.begin(), objMesh.end());

	// meshID -> index of the first object referencing it, so every mesh can be labelled with its
	// OWNER's name. This is what makes the census readable: GG names nearly every loaded mesh
	// "node_mesh" (331 of them on Switch Escape), so the mesh NameComponent alone discriminates
	// nothing — the object name is the real identity.
	std::vector<std::pair<uint64_t, size_t>> meshToObj;
	meshToObj.reserve(mc.objects.GetCount());
	for (size_t i = 0; i < mc.objects.GetCount(); ++i)
		meshToObj.push_back(std::make_pair((uint64_t)mc.objects[i].meshID, i));
	std::sort(meshToObj.begin(), meshToObj.end());
	auto ownerName = [&](wi::ecs::Entity me) -> std::string
	{
		auto it = std::lower_bound(meshToObj.begin(), meshToObj.end(),
			std::make_pair((uint64_t)me, (size_t)0));
		if (it == meshToObj.end() || it->first != (uint64_t)me) return std::string();
		const wi::scene::NameComponent* on = mc.names.GetComponent(mc.objects.GetEntity(it->second));
		return (on != nullptr) ? on->name : std::string();
	};

	struct CBucket { std::string key; int count; size_t verts; size_t tris; int orphans; std::vector<std::string> samples; };
	std::vector<CBucket> sigB, namB;
	auto bump = [](std::vector<CBucket>& v, const std::string& k, size_t vtx, size_t tri, bool orphan, const std::string& sample)
	{
		for (auto& b : v)
			if (b.key == k)
			{
				b.count++; b.verts += vtx; b.tris += tri; b.orphans += orphan ? 1 : 0;
				if (!sample.empty() && b.samples.size() < 4 &&
					std::find(b.samples.begin(), b.samples.end(), sample) == b.samples.end())
					b.samples.push_back(sample);
				return;
			}
		CBucket nb; nb.key = k; nb.count = 1; nb.verts = vtx; nb.tris = tri; nb.orphans = orphan ? 1 : 0;
		if (!sample.empty()) nb.samples.push_back(sample);
		v.push_back(nb);
	};

	int totalOrphans = 0;
	size_t totalVerts = 0, totalTris = 0;
	std::vector<std::string> orphanLines;

	for (size_t i = 0; i < mc.meshes.GetCount(); ++i)
	{
		const wi::ecs::Entity me = mc.meshes.GetEntity(i);
		const wi::scene::MeshComponent& m = mc.meshes[i];
		const size_t vtx = m.vertex_positions.size();
		const size_t tri = m.indices.size() / 3;
		totalVerts += vtx; totalTris += tri;

		const bool orphan = !std::binary_search(objMesh.begin(), objMesh.end(), (uint64_t)me);
		if (orphan) totalOrphans++;

		const std::string owner = ownerName(me);

		char sigKey[128];
		_snprintf(sigKey, sizeof(sigKey), "v=%-6zu tri=%-7zu subsets=%zu", vtx, tri, m.subsets.size());
		sigKey[sizeof(sigKey) - 1] = 0;
		bump(sigB, sigKey, vtx, tri, orphan, owner);

		// OWNER-first, because GG names almost every loaded mesh "node_mesh".
		std::string nameKey = owner;
		const wi::scene::NameComponent* mn = mc.names.GetComponent(me);
		if (nameKey.empty() && mn != nullptr && !mn->name.empty()) nameKey = "[mesh] " + mn->name;
		if (nameKey.empty())
		{
			const wi::scene::HierarchyComponent* h = mc.hierarchy.GetComponent(me);
			if (h != nullptr)
			{
				const wi::scene::NameComponent* pn = mc.names.GetComponent(h->parentID);
				if (pn != nullptr && !pn->name.empty()) nameKey = "[parent] " + pn->name;
			}
		}
		if (nameKey.empty()) nameKey = orphan ? "<unnamed, ORPHAN>" : "<unnamed>";
		bump(namB, nameKey, vtx, tri, orphan, (mn != nullptr) ? mn->name : std::string());

		if (orphan && orphanLines.size() < 40)
		{
			char ol[256];
			_snprintf(ol, sizeof(ol), "  entity=%llu v=%zu tri=%zu subsets=%zu name=\"%s\"",
				(unsigned long long)me, vtx, tri, m.subsets.size(),
				(mn != nullptr) ? mn->name.c_str() : "");
			ol[sizeof(ol) - 1] = 0;
			orphanLines.push_back(ol);
		}
	}

	auto byCount = [](const CBucket& a, const CBucket& b) { return a.count > b.count; };
	std::sort(sigB.begin(), sigB.end(), byCount);
	std::sort(namB.begin(), namB.end(), byCount);

	FILE* f = GGDiagFopen("mesh_census.txt", "w");
	if (f == nullptr)
	{
		_snprintf(result, resultSize, "ERROR: DUMP_MESHES could not open mesh_census.txt");
		result[resultSize - 1] = 0;
		return true;
	}
	fprintf(f, "MESH CENSUS (GGMAX 2.26) — enumerate the floor, do not subtract for it\n");
	fprintf(f, "meshes=%d objects=%d materials=%d transforms=%d hierarchy=%d\n",
		(int)mc.meshes.GetCount(), (int)mc.objects.GetCount(), (int)mc.materials.GetCount(),
		(int)mc.transforms.GetCount(), (int)mc.hierarchy.GetCount());
	fprintf(f, "total verts=%zu  total tris=%zu  ORPHAN meshes (no ObjectComponent refs)=%d\n\n",
		totalVerts, totalTris, totalOrphans);

	auto samplesOf = [](const CBucket& b) -> std::string
	{
		std::string s;
		for (size_t i = 0; i < b.samples.size(); ++i) { if (i) s += ", "; s += b.samples[i]; }
		return s;
	};

	fprintf(f, "== BY GEOMETRY SIGNATURE (generator families cluster here) ==\n");
	fprintf(f, "%6s %8s %-38s %11s  %s\n", "count", "orphans", "signature", "tris_total", "example owners");
	for (const auto& b : sigB)
		fprintf(f, "%6d %8d %-38s %11zu  %s\n", b.count, b.orphans, b.key.c_str(), b.tris, samplesOf(b).c_str());

	fprintf(f, "\n== BY OWNER NAME (referencing object -> mesh -> parent) ==\n");
	fprintf(f, "%6s %8s %-46s %s\n", "count", "orphans", "owner", "mesh names");
	for (const auto& b : namB)
		fprintf(f, "%6d %8d %-46s %s\n", b.count, b.orphans, b.key.c_str(), samplesOf(b).c_str());

	if (!orphanLines.empty())
	{
		fprintf(f, "\n== ORPHAN MESHES (first %d) ==\n", (int)orphanLines.size());
		for (const auto& l : orphanLines) fprintf(f, "%s\n", l.c_str());
	}

	// DUMP_MESHES <filter> — itemise one family. The histograms say WHAT is numerous; this says
	// what those things ARE (who owns them, what they hang off, whether they draw, where they sit).
	// Added to identify the ~110 6-vertex "plane" quads that are the largest non-terrain floor
	// family and sit at 108/113/110 across three unrelated demos.
	if (arg != nullptr && arg[0] != 0)
	{
		auto ciFind = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		fprintf(f, "\n== DETAIL for filter \"%s\" ==\n", arg);
		fprintf(f, "%-10s %-22s %-26s %-10s %-9s %s\n", "entity", "owner", "parent", "renderable", "v/tri", "world pos");
		int shown = 0, matched = 0;
		for (size_t i = 0; i < mc.meshes.GetCount(); ++i)
		{
			const wi::ecs::Entity me = mc.meshes.GetEntity(i);
			const std::string owner = ownerName(me);
			const wi::scene::NameComponent* mn = mc.names.GetComponent(me);
			const bool hit = ciFind(owner, arg) || (mn != nullptr && ciFind(mn->name, arg));
			if (!hit) continue;
			matched++;
			if (shown >= 60) continue;
			shown++;

			// owning object -> renderable + transform
			std::string parent = "-"; std::string rend = "?"; float px = 0, py = 0, pz = 0;
			auto it = std::lower_bound(meshToObj.begin(), meshToObj.end(), std::make_pair((uint64_t)me, (size_t)0));
			if (it != meshToObj.end() && it->first == (uint64_t)me)
			{
				const wi::ecs::Entity oe = mc.objects.GetEntity(it->second);
				rend = mc.objects[it->second].IsRenderable() ? "yes" : "NO";
				const wi::scene::TransformComponent* tr = mc.transforms.GetComponent(oe);
				if (tr != nullptr) { px = tr->world._41; py = tr->world._42; pz = tr->world._43; }
				const wi::scene::HierarchyComponent* h = mc.hierarchy.GetComponent(oe);
				if (h != nullptr)
				{
					const wi::scene::NameComponent* pn = mc.names.GetComponent(h->parentID);
					parent = (pn != nullptr && !pn->name.empty()) ? pn->name : "<unnamed parent>";
				}
			}
			const wi::scene::MeshComponent& m2 = mc.meshes[i];
			fprintf(f, "%-10llu %-22s %-26s %-10s %zu/%-6zu (%.0f, %.0f, %.0f)\n",
				(unsigned long long)me, owner.c_str(), parent.c_str(), rend.c_str(),
				m2.vertex_positions.size(), m2.indices.size() / 3, px, py, pz);
		}
		fprintf(f, "matched=%d shown=%d\n", matched, shown);
	}
	fclose(f);

	_snprintf(result, resultSize,
		"OK: DUMP_MESHES -> mesh_census.txt  meshes=%d orphans=%d sigBuckets=%d nameBuckets=%d",
		(int)mc.meshes.GetCount(), totalOrphans, (int)sigB.size(), (int)namB.size());
	result[resultSize - 1] = 0;
	return true;
}

// GGMAX 3.27: camera spin rate in degrees per second, 0 = off. DIAGNOSTIC ONLY - nothing in
// the product sets it, and it is inert until SPIN_CAMERA is sent. Read by the per-frame
// update in master_part1.cpp, which advances editorfreeflight yaw by rate * wall-clock dt.
float gg_spin_deg_per_sec = 0.0f;
// GGMAX 3.29: translation oscillation for the shadow-cost work. DIAGNOSTIC ONLY - nothing in
// the product sets these. SPIN_CAMERA cannot reproduce the shadow cost because the atlas sizes
// each local shadow from its DISTANCE to the camera, and rotation does not change a distance.
float gg_move_units_per_sec = 0.0f;
float gg_move_range = 600.0f;
float gg_move_anchor_x = 0.0f;
float gg_move_anchor_z = 0.0f;

// GGMAX 3.27: hoisted into its own function rather than added to the dispatch ladder.
// ⚠ Adding one more `else if` to that chain tips MSVC over C1061 ("blocks nested too

// ★★★ GGMAX 3.38: commands to TEST the ported DX11 features without a human at the keyboard.
//
// Why these exist: PRESS_KEY posts WM_KEYDOWN, and GameGuru reads gameplay input through
// DirectInput, so the harness cannot fire a weapon. That left the 3.38 bullet-hole port verifiable
// only by eye. These drive the REAL code paths instead of simulating a keypress.
//
// ⚠ In a helper, not the main ladder - that chain is at the MSVC C1061 nesting limit and the
// error it produces names a line ~2000 away. See the note above AutoHarness_SpinCommands.
static bool AutoHarness_BulletHoleCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	extern int bulletholes_getcount(void);

	if (_stricmp(cmd, "COUNT_BULLETHOLES") == 0)
	{
		_snprintf(result, resultSize, "OK: BULLETHOLES=%d", bulletholes_getcount());
	}
	else if (_stricmp(cmd, "GET_BULLETHOLE_FLAGS") == 0)
	{
		// GET_BULLETHOLE_FLAGS <entityIndex>
		// Reports the three inputs to the shipped decision and evaluates it EXACTLY as
		// G-Entity_part2.cpp does, so an A/B over iAllowBuletHole proves the 3.38 clause works.
		int idx = -1;
		if (sscanf_s(arg, "%d", &idx) < 1 || idx < 0 || idx > g.entityelementlist)
		{
			_snprintf(result, resultSize, "ERROR: GET_BULLETHOLE_FLAGS needs a valid entity index (0..%d)", g.entityelementlist);
		}
		else
		{
			const int sf  = t.entityelement[idx].staticflag;
			const int imm = t.entityelement[idx].eleprof.isimmobile;
			const int abh = t.entityelement[idx].iAllowBuletHole;
			// the shipped predicate, character for character
			const bool allowed = ( idx == 0 || sf == 1 || abh == 1 || ( sf == 0 && imm == 1 ) );
			_snprintf(result, resultSize,
				"OK: idx=%d name='%s' staticflag=%d isimmobile=%d iAllowBuletHole=%d -> BULLETHOLE_%s",
				idx, t.entityelement[idx].eleprof.name_s.Get(), sf, imm, abh, allowed ? "ALLOWED" : "DENIED");
		}
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_BULLETHOLE_FLAG") == 0)
	{
		// SET_BULLETHOLE_FLAG <entityIndex> <0|1>
		// ⚠ IN MEMORY ONLY - deliberately does not mark the project modified and does not save.
		// The point is to A/B the runtime decision, not to edit anyone's level.
		int idx = -1, val = -1;
		if (sscanf_s(arg, "%d %d", &idx, &val) < 2 || idx < 0 || idx > g.entityelementlist || val < 0 || val > 1)
		{
			_snprintf(result, resultSize, "ERROR: SET_BULLETHOLE_FLAG needs <entityIndex> <0|1>");
		}
		else
		{
			t.entityelement[idx].iAllowBuletHole = val;
			_snprintf(result, resultSize, "OK: entity %d iAllowBuletHole=%d (memory only, not saved)", idx, val);
		}
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "GET_ENTITY_SLOTS") == 0)
	{
		// GET_ENTITY_SLOTS <entityIndex>
		// GGMAX 3.41: reads back the two RESERVED SLOTS of the v340 entity record - slot 1 is
		// iAllowBuletHole and slot 2 is iMaterialSoundIndex. Both are DX11 fields, both LONGs.
		// This is what a save/reload round-trip gets checked against.
		int idx = -1;
		if (sscanf_s(arg, "%d", &idx) < 1 || idx < 0 || idx > g.entityelementlist)
		{
			_snprintf(result, resultSize, "ERROR: GET_ENTITY_SLOTS needs a valid entity index (0..%d)", g.entityelementlist);
		}
		else
		{
			_snprintf(result, resultSize,
				"OK: idx=%d name=[%s] slot1_iAllowBuletHole=%d slot2_iMaterialSoundIndex=%d",
				idx, t.entityelement[idx].eleprof.name_s.Get(),
				t.entityelement[idx].iAllowBuletHole,
				t.entityelement[idx].eleprof.iMaterialSoundIndex);
		}
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_ENTITY_SLOTS") == 0)
	{
		// SET_ENTITY_SLOTS <entityIndex> <slot1> <slot2>
		// Writes both reserved slots IN MEMORY. Follow with SAVE_LEVEL to commit - that pairing is
		// the only way to prove the record survives a real File>Save and reload.
		// Slot 2 is the Material Type the editor dropdown sets (0..4), which 3.38 silently dropped.
		int idx = -1, s1 = -1, s2 = -1;
		if (sscanf_s(arg, "%d %d %d", &idx, &s1, &s2) < 3 || idx < 0 || idx > g.entityelementlist
			|| s1 < 0 || s1 > 1 || s2 < 0 || s2 > 4)
		{
			_snprintf(result, resultSize, "ERROR: SET_ENTITY_SLOTS needs <entityIndex> <slot1 0|1> <slot2 0..4>");
		}
		else
		{
			t.entityelement[idx].iAllowBuletHole = s1;
			t.entityelement[idx].eleprof.iMaterialSoundIndex = s2;
			_snprintf(result, resultSize, "OK: idx=%d slot1=%d slot2=%d (memory; use SAVE_LEVEL to commit)", idx, s1, s2);
		}
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "FIRE_RAY_AT") == 0)
	{
		// FIRE_RAY_AT <entityIndex>
		// ★ Drives the REAL path: sets the bullet ray from just outside the entity to its centre and
		// calls entity_hasbulletrayhit(), which raycasts, resolves the material and runs the decal
		// decision. Counting g_bulletholes either side is the end-to-end proof a hole was created.
		// Needs game state and a non-melee weapon - the shipped test is gated on gun settings.
		int idx = -1;
		if (sscanf_s(arg, "%d", &idx) < 1 || idx < 0 || idx > g.entityelementlist)
		{
			_snprintf(result, resultSize, "ERROR: FIRE_RAY_AT needs a valid entity index");
		}
		else
		{
			extern void entity_hasbulletrayhit(void);
			const int before = bulletholes_getcount();
			const float ex = t.entityelement[idx].x;
			const float ey = t.entityelement[idx].y + 30.0f;
			const float ez = t.entityelement[idx].z;
			// muzzle 200 units out on +X, aimed at the entity centre
			t.x1_f = ex + 200.0f; t.y1_f = ey; t.z1_f = ez;
			t.x2_f = ex;          t.y2_f = ey; t.z2_f = ez;
			entity_hasbulletrayhit();
			const int after = bulletholes_getcount();
			_snprintf(result, resultSize,
				"OK: fired at %d  holes %d->%d (delta %d)  rayhite=%d decalimpact=%d gunid=%d ismelee=%d",
				idx, before, after, after - before,
				t.bulletrayhite, t.tttriggerdecalimpact, t.gunid, t.gun[t.gunid].settings.ismelee);
		}
		result[resultSize - 1] = 0;
	}
	else if (_stricmp(cmd, "TRIGGER_LUA_ERROR") == 0)
	{
		// ★ GGMAX 3.38 phase D check: run a Lua chunk that faults, so the runtime error path fires
		// and we can see whether it now names the script and line. Reports what the engine recorded.
		extern lua_State* lua2;
		if (!lua2)
		{
			_snprintf(result, resultSize, "ERROR: no lua state (needs game/test state)");
		}
		else
		{
			const char* chunk = "local t = nil; return t.deliberate_nil_index_from_harness";
			const int rc = luaL_loadbuffer(lua2, chunk, strlen(chunk), "harness_luaerror_test");
			int prc = -1;
			const char* msg = "";
			if (rc == 0)
			{
				prc = lua_pcall(lua2, 0, 0, 0);
				if (prc != 0 && lua_isstring(lua2, -1)) { msg = lua_tostring(lua2, -1); lua_pop(lua2, 1); }
			}
			_snprintf(result, resultSize, "OK: load_rc=%d pcall_rc=%d msg='%s'", rc, prc, msg);
		}
		result[resultSize - 1] = 0;
	}
	else
	{
		return false;
	}
	return true;
}
// deeply") - it is already at the limit, and the error names a line ~2000 further down, so it
// reads as a problem somewhere else entirely. Every new command goes in a helper from here on.
// GGMAX 3.123 (tester report #8, 'TREE TYPES DROPPED' over a read-only demo): GET_TREE_LOADSTATS - the
// tree-type check of the last level load, readable with logs off (testers run producelogfiles=0).
//   dropped        = visible trees the last GGTrees_SetData hid for a type >= 38
//   saved_*        = what a Save would write NOW (GGTrees_GetData): visible trees, records of ANY visibility
//                    with a type >= 38 (must be 0 - pAllTrees never holds one), visible ones among them
//   readonly / fpm = the storyboard's read-only flag, and the level file resolved the way the loader
//                    resolves it (GG_GetRealPath: the writable area FIRST) - a saved copy in
//                    Documents\GameGuruApps\GameGuruMAX\Files\mapbank shadows a read-only demo's level.
static bool AutoHarness_TreeLoadCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "TREE_PAINT_TEST") == 0)
	{
		// TREE_PAINT_TEST <worldX> <worldZ> <frames> [mode 0 spray | 4 spray remove] [inject N] - see
		// AutoHarness_TreePaintTick. Brush = SET_TERRAINBRUSH, types = the palette (SET_PALETTEBITS tree).
		// TREE_PAINT_TEST with no arguments reports the last run's injection count.
		float tx = 0, tz = 0; int tf = 0, tm = GGTREES_PAINT_SPRAY, ti = 0;
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%f %f %d %d %d", &tx, &tz, &tf, &tm, &ti) : 0;
		if (got == 0) _snprintf(result, resultSize, "OK: TREE_PAINT_TEST frames_left=%d injected=%d", s_treeTestFrames, s_treeTestInjected);
		else if (!AutoHarness_TerrainEditAllowed() || !::ggtrees_initialised) _snprintf(result, resultSize, "ERROR: TREE_PAINT_TEST needs the level editor with trees initialised");
		else if (s_treeTestFrames > 0) _snprintf(result, resultSize, "ERROR: a tree test stroke is already running (%d frames left)", s_treeTestFrames);
		else if (s_grassTestFrames > 0 || s_terrainTestFrames > 0) _snprintf(result, resultSize, "ERROR: a terrain/grass test is running");
		else if (g_iCalculatingChangeBounds != 0 || ::g_iActiveTreeEvent != 0) _snprintf(result, resultSize, "ERROR: a terrain/grass/tree stroke is in progress - release the mouse first");
		else if (tm != GGTREES_PAINT_SPRAY && tm != GGTREES_PAINT_SPRAY_REMOVE) _snprintf(result, resultSize, "ERROR: TREE_PAINT_TEST mode must be 0 (spray) or 4 (spray remove)");
		else if (got >= 3 && tf > 0)
		{
			s_treeTestX = tx; s_treeTestZ = tz; s_treeTestMode = tm; s_treeTestInject = (ti > 0) ? ti : 0;
			s_treeTestInjected = (ti > 0) ? 0 : -1;
			s_treeTestFrame = 0; s_treeTestFrames = tf + 1;   // + the release frame
			_snprintf(result, resultSize, "OK: TREE_PAINT_TEST at (%.0f, %.0f) for %d frames, mode %d, inject %d (brushSize=%.0f)",
				tx, tz, tf, tm, s_treeTestInject, GGTerrain::ggterrain_global_render_params2.brushSize);
		}
		else _snprintf(result, resultSize, "ERROR: TREE_PAINT_TEST needs <worldX> <worldZ> <frames> [mode 0|4] [inject N]");
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "GET_TREE_LOADSTATS") != 0) return false;
	const uint32_t nTypes = GGTrees::GGTrees_GetNumTypes();
	const uint32_t nFloats = GGTrees::GGTrees_GetDataSize();
	uint32_t vis = 0, badType = 0, badVisible = 0;
	std::vector<float> buf(nFloats > 0 ? nFloats : 1);
	const int got = (nFloats > 1) ? GGTrees::GGTrees_GetData(buf.data()) : 0;
	if (got)
	{
		const uint32_t* di = (const uint32_t*)buf.data();
		const uint32_t n = (nFloats - 1) / 3;
		for (uint32_t i = 0; i < n; i++)
		{
			const uint32_t d = di[1 + i * 3 + 2];
			const bool bBad = ((d >> 11) & 0x3F) >= nTypes;
			if (d & 1) vis++;
			if (bBad) { badType++; if (d & 1) badVisible++; }
		}
	}
	char fpm[MAX_PATH];
	strncpy(fpm, g.projectfilename_s.Get(), MAX_PATH - 1); fpm[MAX_PATH - 1] = 0;
	if (fpm[0]) GG_GetRealPath(fpm, 0);
	_snprintf(result, resultSize, "OK: dropped=%u saved_visible=%u saved_badtype=%u saved_badvisible=%u getdata=%d types=%u readonly=%d fpm=%s",
		GGTrees::GGTrees_GetLastLoadInvalidTypeTrees(), vis, badType, badVisible, got, nTypes, Storyboard.project_readonly, fpm[0] ? fpm : "(none)");
	result[resultSize - 1] = 0;
	return true;
}
// GGMAX 3.122: the custom arms chain ("legacy arm replacement trick", G-Gun_part3.cpp gun_load), link by
// link - tester report: the interactive swimming arms ignore the arm set chosen on the Player Start marker.
// Here, not in the else-if chain: one more link there is C1061 (blocks nested too deeply).
static bool AutoHarness_ArmsCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	extern cstr g_guns_customArms_s;
	extern std::map<int, std::string> g_guns_loadedArms;
	if (_stricmp(cmd, "SET_PLAYERARMS") == 0)
	{
		// SET_PLAYERARMS <arm set folder | none> - the Player Start marker's arm set (eleprof.texaltd_s, what
		// its hands dropdown writes), set LIVE on every start marker; nothing is saved. Read at the next game
		// start by gun_activategunsfromentities.
		cstr want = (arg && arg[0] && _stricmp(arg, "none") != 0) ? cstr((char*)arg) : cstr("");
		int n = 0; char old[256] = "";
		for (int e = 1; e <= g.entityelementlist; e++)
		{
			const int entid = t.entityelement[e].bankindex;
			if (entid > 0 && t.entityprofile[entid].ismarker == 1)
			{
				if (n == 0) { strncpy(old, t.entityelement[e].eleprof.texaltd_s.Get(), sizeof(old) - 1); old[sizeof(old) - 1] = 0; }
				t.entityelement[e].eleprof.texaltd_s = want;
				n++;
			}
		}
		_snprintf(result, resultSize, "OK: SET_PLAYERARMS '%s' on %d start marker(s) (was '%s'); live-only, read at the next game start", want.Get(), n, old);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "TEST_ARMSLEAK") == 0)
	{
		// TEST_ARMSLEAK <arm set> [append 0|1] [refresh 0|1] - the custom-arms secondary object's life, one step at
		// a time, on a scratch object id: LoadObject(hands\\<set>\\arms.dbo) -> AppendAnimationFromFile
		// (LegacyInteractiveSwimming) -> WickedCall_RefreshObjectAnimations -> DeleteObject, with the Wicked scene
		// counts after each. Names the step that leaves entities behind (3.122 stress: +1 object/mesh/material/
		// armature per arm-set rebuild).
		extern bool AppendAnimationFromFile(sObject* pObject, LPSTR szFilename, int iFrame);
		extern void WickedCall_RefreshObjectAnimations(sObject* pObject, void* pstateptr);
		char set[128] = ""; int doAppend = 1, doRefresh = 1;
		if (arg && arg[0]) { if (sscanf_s(arg, "%127[^|]|%d|%d", set, (unsigned)sizeof(set), &doAppend, &doRefresh) < 1) set[0] = 0; }
		const int iTmp = g.gunbankextraobjoffset + 149;
		wiScene::Scene& sc = wiScene::GetScene();
		auto snap = [&](char* out, size_t n) {
			_snprintf(out, n, "obj=%d mesh=%d mat=%d arm=%d xf=%d hier=%d anim=%d", (int)sc.objects.GetCount(), (int)sc.meshes.GetCount(),
				(int)sc.materials.GetCount(), (int)sc.armatures.GetCount(), (int)sc.transforms.GetCount(), (int)sc.hierarchy.GetCount(), (int)sc.animations.GetCount());
			out[n - 1] = 0; };
		if (!set[0] || ObjectExist(iTmp) == 1)
			_snprintf(result, resultSize, "ERROR: TEST_ARMSLEAK <arm set>|<append 0/1>|<refresh 0/1> (scratch obj %d %s)", iTmp, ObjectExist(iTmp) ? "in use" : "free");
		else
		{
			char s0[160], s1[160], s2[160], s3[160], s4[160];
			snap(s0, sizeof(s0));
			cstr arms = g.fpscrootdir_s + cstr("\\Files\\gamecore\\hands\\") + cstr(set) + cstr("\\arms.dbo");
			LoadObject(arms.Get(), iTmp);
			sc.Update(0); snap(s1, sizeof(s1));
			sObject* pO = GetObjectData(iTmp);
			cstr anim = g.fpscrootdir_s + cstr("\\Files\\gamecore\\hands\\Animations\\LegacyInteractiveSwimming.dbo");
			bool bApp = false;
			if (pO && doAppend) bApp = AppendAnimationFromFile(pO, anim.Get(), 0);
			sc.Update(0); snap(s2, sizeof(s2));
			if (pO && bApp && doRefresh) WickedCall_RefreshObjectAnimations(pO, pO->wickedloaderstateptr);
			sc.Update(0); snap(s3, sizeof(s3));
			if (ObjectExist(iTmp) == 1) DeleteObject(iTmp);
			sc.Update(0); snap(s4, sizeof(s4));
			_snprintf(result, resultSize, "OK: TEST_ARMSLEAK '%s' loaded=%d appended=%d | before %s | load %s | append %s | refresh %s | delete %s",
				set, pO ? 1 : 0, bApp ? 1 : 0, s0, s1, s2, s3, s4);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_CUSTOMARMS_LIVE") == 0)
	{
		// SET_CUSTOMARMS_LIVE <arm set | none> - writes g_guns_customArms_s itself, mid-game: what the NEXT LEVEL
		// of a standalone game does at its start (gun_activategunsfromentities), without a level change. Carried
		// guns then meet a different set on their next selection (gun_selectandorload). Test-only; reverts at the
		// next game start, which re-reads the Player Start.
		cstr old = g_guns_customArms_s;
		g_guns_customArms_s = (arg && arg[0] && _stricmp(arg, "none") != 0) ? cstr((char*)arg) : cstr("");
		_snprintf(result, resultSize, "OK: SET_CUSTOMARMS_LIVE '%s' (was '%s')", g_guns_customArms_s.Get(), old.Get());
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_PLAYERWEAPON") == 0)
	{
		// SET_PLAYERWEAPON <gun path, e.g. enhanced\AK | none> - the Player Start marker's starting weapon
		// (eleprof.hasweapon_s; G-Entity resolves the index at game start), set LIVE; nothing is saved.
		cstr want = (arg && arg[0] && _stricmp(arg, "none") != 0) ? cstr((char*)arg) : cstr("");
		int n = 0; char old[256] = "";
		for (int e = 1; e <= g.entityelementlist; e++)
		{
			const int entid = t.entityelement[e].bankindex;
			if (entid > 0 && t.entityprofile[entid].ismarker == 1)
			{
				if (n == 0) { strncpy(old, t.entityelement[e].eleprof.hasweapon_s.Get(), sizeof(old) - 1); old[sizeof(old) - 1] = 0; }
				t.entityelement[e].eleprof.hasweapon_s = want;
				n++;
			}
		}
		_snprintf(result, resultSize, "OK: SET_PLAYERWEAPON '%s' on %d start marker(s) (was '%s'); live-only, read at the next game start", want.Get(), n, old);
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_ENTITYSCRIPT") == 0)
	{
		// SET_ENTITYSCRIPT <element> <script relative to scriptbank, e.g. global\swimming.lua> - LIVE behaviour
		// swap on one entity (nothing saved), so a behaviour the level does not carry - the swimming arms -
		// can run its real flow at the next game start. No script = report the current one.
		int e = 0; char path[260] = "";
		const int got = (arg && arg[0]) ? sscanf_s(arg, "%d %259[^\n]", &e, path, (unsigned)sizeof(path)) : 0;
		if (got < 1 || e < 1 || e > g.entityelementlist)
			_snprintf(result, resultSize, "ERROR: SET_ENTITYSCRIPT <element 1..%d> [script]", g.entityelementlist);
		else if (got < 2)
			_snprintf(result, resultSize, "OK: e%d script '%s'", e, t.entityelement[e].eleprof.aimain_s.Get());
		else
		{
			cstr old = t.entityelement[e].eleprof.aimain_s;
			t.entityelement[e].eleprof.aimain_s = path;
			const int bi = t.entityelement[e].bankindex;
			const bool bLoads = bi > 0 && t.entityelement[e].staticflag == 0;	// lua_scanandloadactivescripts' own test
			_snprintf(result, resultSize, "OK: e%d script '%s' (was '%s'); live-only, read at the next game start%s", e, path, old.Get(),
				bLoads ? "" : " - WARNING: static or empty element, its script will NOT load");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_GUNARMS") == 0)
	{
		// every loaded gun: its LEGACYARMS limb (hidden when the trick applies) and its secondary arms object
		// (gunbankextraobjoffset + slot: the chosen arm set's arms.dbo) - existence, visibility, Wicked
		// renderable state and the base colour texture the material actually carries.
		wiScene::Scene& sc = wiScene::GetScene();
		auto texOf = [&](sMesh* m) -> const char* {
			if (!m) return "-";
			wiScene::MaterialComponent* mat = sc.materials.GetComponent((wi::ecs::Entity)m->wickedmaterialindex);
			if (!mat) return "(no mat)";
			const std::string& nm = mat->textures[wiScene::MaterialComponent::BASECOLORMAP].name;
			return nm.empty() ? "(none)" : nm.c_str();
		};
		int w = _snprintf(result, resultSize, "OK: customArms='%s' gunid=%d gun='%s' currentgunobj=%d brassbank=%d markers:",
			g_guns_customArms_s.Get(), t.gunid, t.gun_s.Get(), t.currentgunobj, g.brassbankmax);
		for (int e = 1; e <= g.entityelementlist && w > 0 && w < (int)resultSize - 200; e++)
		{
			const int entid = t.entityelement[e].bankindex;
			if (entid > 0 && t.entityprofile[entid].ismarker == 1)
				w += _snprintf(result + w, resultSize - w, " [e%d '%s']", e, t.entityelement[e].eleprof.texaltd_s.Get());
		}
		for (int gid = 1; gid <= g.gunmax && w > 0 && w < (int)resultSize - 600; gid++)
		{
			const int obj = t.gun[gid].obj;
			if (obj <= 0 || ObjectExist(obj) != 1) continue;
			sObject* p = GetObjectData(obj);
			int la = -1, laVis = -1, laRend = -1, laObjRend = -1; const char* laTex = "-";
			for (int f = 0; p && f < p->iFrameCount; f++)
			{
				sFrame* fr = p->ppFrameList[f];
				if (!fr || _stricmp(fr->szName, "LEGACYARMS") != 0) continue;
				la = f;
				if (fr->pMesh)
				{
					laVis = fr->pMesh->bVisible ? 1 : 0;
					wiScene::MeshComponent* m = sc.meshes.GetComponent((wi::ecs::Entity)fr->pMesh->wickedmeshindex);
					laRend = m ? (m->IsRenderable() ? 1 : 0) : -1;
					laTex = texOf(fr->pMesh);
				}
				wiScene::ObjectComponent* oc = sc.objects.GetComponent((wi::ecs::Entity)fr->wickedobjindex);
				laObjRend = oc ? (oc->IsRenderable() ? 1 : 0) : -1;
				break;
			}
			const int sec = g.gunbankextraobjoffset + (obj - g.gunbankoffset);
			int sndSlot = -1, sndId = 0;	// the first sound the gun holds - a rebuild must free the old id
			for (int s = 0; gid < (int)t.gunsound.size() && s <= 15 && sndId == 0; s++)
				if (t.gunsound[gid][s].soundid1 > 0) { sndSlot = s; sndId = t.gunsound[gid][s].soundid1; }
			const int secExist = ObjectExist(sec);
			int secVis = -1, secMeshes = 0, secRend = 0, secFrames = -1; const char* secTex = "-";
			if (secExist == 1)
			{
				sObject* s = GetObjectData(sec);
				secVis = (s && s->bVisible) ? 1 : 0;
				secFrames = s ? s->iFrameCount : -1;
				for (int f = 0; s && f < s->iFrameCount; f++)
				{
					sFrame* fr = s->ppFrameList[f];
					if (!fr || !fr->pMesh) continue;
					secMeshes++;
					wiScene::ObjectComponent* oc = sc.objects.GetComponent((wi::ecs::Entity)fr->wickedobjindex);
					if (oc && oc->IsRenderable()) secRend++;
					if (secTex[0] == '-') secTex = texOf(fr->pMesh);
				}
			}
			std::map<int, std::string>::const_iterator itB = g_guns_loadedArms.find(gid);
			w += _snprintf(result + w, resultSize - w, " || gun%d '%s' built='%s' obj=%d vis=%d LEGACYARMS=%d(mesh vis %d rend %d obj rend %d tex '%s') sec=%d exist=%d vis=%d meshes=%d rend=%d frames=%d tex '%s' snd[%d]=%d brass=%d(exist %d)",
				gid, t.gun[gid].name_s.Get(), (itB != g_guns_loadedArms.end()) ? itB->second.c_str() : "?", obj, (p && p->bVisible) ? 1 : 0, la, laVis, laRend, laObjRend, laTex,
				sec, secExist, secVis, secMeshes, secRend, secFrames, secTex, sndSlot, sndId,
				g.firemodes[gid][0].settings.brassobjmaster, g.firemodes[gid][0].settings.brassobjmaster > 0 ? ObjectExist(g.firemodes[gid][0].settings.brassobjmaster) : 0);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	return false;
}

static bool AutoHarness_SpinCommands(const char* cmd, const char* arg, char* result, size_t resultSize)
{
	if (_stricmp(cmd, "MOVE_CAMERA") == 0)
	{
		// MOVE_CAMERA <units_per_sec> [range]   (0 = stop)
		// ★ SPIN_CAMERA cannot reproduce the shadow cost: the atlas sizes every local shadow from
		// its DISTANCE to the camera, and rotation does not change a distance. This runs back and
		// forth along the facing direction instead.
		extern float gg_move_units_per_sec, gg_move_range, gg_move_anchor_x, gg_move_anchor_z;
		float rate = 0.0f, rng = 600.0f;
		const int got = sscanf_s(arg, "%f %f", &rate, &rng);
		const char* state = AutoHarness_GetAppState();
		if (got < 1)
		{
			_snprintf(result, resultSize, "ERROR: MOVE_CAMERA needs <units_per_sec> [range] (0 stops)");
		}
		else if (strcmp(state, "editor") != 0 && rate != 0.0f)
		{
			_snprintf(result, resultSize,
				"ERROR: MOVE_CAMERA drives the editor free-flight camera (state: %s)", state);
		}
		else
		{
			if (rate != 0.0f)
			{
				// anchor HERE, so the oscillation is around wherever the camera currently is
				gg_move_anchor_x = t.editorfreeflight.c.x_f;
				gg_move_anchor_z = t.editorfreeflight.c.z_f;
			}
			gg_move_range = (rng > 1.0f) ? rng : 600.0f;
			gg_move_units_per_sec = rate;
			_snprintf(result, resultSize,
				"OK: MOVE_CAMERA %.1f units/s over %.0f units (%s), anchored at (%.1f,%.1f)",
				rate, gg_move_range, (rate == 0.0f) ? "stopped" : "moving",
				gg_move_anchor_x, gg_move_anchor_z);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_MATERIALTYPES") == 0)
	{
		// Which shader permutations does this level actually use?
		// ★ Without this, a null result from SET_SUPERQUICK is ambiguous: it could mean the switch
		// does not work, or that the level has nothing expensive to collapse. Those need completely
		// different next steps, and guessing which costs a whole test cycle.
		auto& sc = wi::scene::GetScene();
		int counts[wi::scene::MaterialComponent::SHADERTYPE_COUNT] = {};
		for (size_t i = 0; i < sc.materials.GetCount(); ++i)
		{
			const int st = (int)sc.materials[i].shaderType;
			if (st >= 0 && st < wi::scene::MaterialComponent::SHADERTYPE_COUNT) counts[st]++;
		}
		static const char* ggMatNames[] = { "PBR", "PLANARREFLECTION", "PARALLAXOCCLUSION",
			"ANISOTROPIC", "WATER", "CARTOON", "UNLIT", "CLOTH", "CLEARCOAT",
			"CLOTH_CLEARCOAT", "TERRAINBLENDED", "INTERIORMAPPING" };
		int off = _snprintf(result, resultSize, "OK: %d materials\n", (int)sc.materials.GetCount());
		int collapsible = 0;
		for (int i = 0; i < wi::scene::MaterialComponent::SHADERTYPE_COUNT && off > 0 && off < (int)resultSize - 90; ++i)
		{
			if (counts[i] == 0) continue;
			const bool expensive = (i == wi::scene::MaterialComponent::SHADERTYPE_PBR_PLANARREFLECTION
				|| i == wi::scene::MaterialComponent::SHADERTYPE_PBR_PARALLAXOCCLUSIONMAPPING
				|| i == wi::scene::MaterialComponent::SHADERTYPE_PBR_ANISOTROPIC
				|| i == wi::scene::MaterialComponent::SHADERTYPE_PBR_CLOTH
				|| i == wi::scene::MaterialComponent::SHADERTYPE_PBR_CLEARCOAT
				|| i == wi::scene::MaterialComponent::SHADERTYPE_PBR_CLOTH_CLEARCOAT
				|| i == wi::scene::MaterialComponent::SHADERTYPE_INTERIORMAPPING);
			if (expensive) collapsible += counts[i];
			off += _snprintf(result + off, resultSize - off, "  %-18s %4d%s\n",
				ggMatNames[i], counts[i], expensive ? "   <- Super Quick collapses this" : "");
		}
		_snprintf(result + off, resultSize - off, "  COLLAPSIBLE: %d of %d\n",
			collapsible, (int)sc.materials.GetCount());
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "DUMP_DRAWS") == 0)
	{
		// ★★★ DUMP_DRAWS - draws per FRAME, per render pass.
		//
		// The one number needed to tell a per-DRAW cost from a per-TRIANGLE one, and there was no
		// way to get it. 3.34 measured that Opaque Objects is 99% of Opaque Scene, that a pixel
		// shader fetching nothing still costs 58% of it, and that dropping to 1/6 the pixels barely
		// moves it - so the floor is geometry submission. Whether the fix is "merge the objects" or
		// "cut the triangles" depends entirely on draws/frame, which is this.
		//
		// ★ The counters are free-running totals, so a single reading is meaningless. This latches
		// the previous reading and the previous FRAME NUMBER and reports the rate between them:
		// call it twice, and the SECOND reply is the answer over that interval. Dividing by frames
		// rather than by seconds is deliberate - it stays correct while the frame rate moves.
		extern std::atomic<uint64_t>* GG_GetTotalDrawCounters(void);
		std::atomic<uint64_t>* counters = GG_GetTotalDrawCounters();
		const uint64_t frame = wi::graphics::GetDevice() ? wi::graphics::GetDevice()->GetFrameCount() : 0;

		static uint64_t s_prev[wi::enums::RENDERPASS_COUNT] = {};
		static uint64_t s_prevFrame = 0;
		static bool s_armed = false;

		const uint64_t frames = (s_armed && frame > s_prevFrame) ? (frame - s_prevFrame) : 0;
		static const char* passnames[] = { "MAIN", "PREPASS", "PREPASS_DEPTHONLY", "ENVMAPCAPTURE",
			"SHADOW", "VOXELIZE", "RAINBLOCKER" };
		int n = _snprintf(result, resultSize, "OK: DUMP_DRAWS over %llu frames%s",
			(unsigned long long)frames, frames ? "" : " (first call - arming, call again)");
		for (int rp = 0; rp < (int)wi::enums::RENDERPASS_COUNT && n > 0 && n < (int)resultSize - 64; ++rp)
		{
			const uint64_t now = counters[rp].load(std::memory_order_relaxed);
			if (frames > 0)
			{
				const uint64_t d = (now >= s_prev[rp]) ? (now - s_prev[rp]) : 0;
				if (d > 0)
				{
					n += _snprintf(result + n, resultSize - n, "  %s=%llu/frame",
						(rp < (int)(sizeof(passnames) / sizeof(passnames[0]))) ? passnames[rp] : "?",
						(unsigned long long)(d / frames));
				}
			}
			s_prev[rp] = now;
		}
		s_prevFrame = frame;
		s_armed = true;
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_DELAYEDSHADOWS") == 0)
	{
		// SET_DELAYEDSHADOWS <0|1> - stagger sun cascade refresh across frames.
		//
		// ★ The saving is large and measured: staggering runs cascade 4 every 9th frame, 3 every
		// 4th, 2 every 3rd and 1 every 2nd - an average of 2.19 cascades per frame against 5.0 with it
		// off, i.e. 2.28x the sun cascade work when disabled.
		//
		// ⚠ GGMAX 3.37 CORRECTION: this used to call it "the single largest DX11-vs-DX12 DEFAULT
		// difference", on the grounds that DX11 shipped it ON at both layers (WickedRepo
		// wiRenderer.cpp:47 and GameGuruMAX Types.h:4121) while DX12 ships OFF. Those constants do
		// differ, but they are NOT what a user gets: the quality-preset ladder overrides them and the
		// two ladders are IDENTICAL - HIGHEST forces false, LOW forces true, in BOTH renderers (DX12
		// M-Visuals_part1.cpp:654/678, DX11 M-Visuals.cpp:2451/2480). The default preset is HIGHEST,
		// so a default DX11 install had this OFF too; the constants only reach CUSTOM projects.
		// Verified against the read-only DX11 tree 2026-09-14. See NIGHT_INVESTIGATIONS section 3.37.
		int n = -1;
		if (sscanf_s(arg, "%d", &n) < 1 || n < 0 || n > 1)
		{
			_snprintf(result, resultSize, "ERROR: SET_DELAYEDSHADOWS needs 0 or 1");
		}
		else
		{
			extern bool g_bDelayedShadows;
			g_bDelayedShadows = (n != 0);
			t.gamevisuals.g_bDelayedShadows = t.visuals.g_bDelayedShadows = g_bDelayedShadows;
			wi::renderer::SetDelayedShadowCascadesEnabled(g_bDelayedShadows);
			_snprintf(result, resultSize,
				"OK: SET_DELAYEDSHADOWS %d - sun cascades %s", n,
				n ? "staggered across frames (DX11 behaviour)" : "all refreshed every frame (DX12 stock)");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_SUPERQUICK") == 0)
	{
		// SET_SUPERQUICK <0|1|2|3> - which rung of the cut-down opaque object shader.
		//   0 off   1 Flat (no texture fetch)   2 + albedo   3 + lighting and shadows
		//
		// ★ The gap between two rungs is the measurement, not the rung itself. Flat is the
		// floor: whatever "  Opaque Objects" still costs there is not pixel shading at all.
		// ⚠ Writes the VISUALS fields, because master_part1.cpp pushes them to the engine every
		// frame and would otherwise clobber a direct engine write.
		int n = -1;
		if (sscanf_s(arg, "%d", &n) < 1 || n < 0 || n > 3)
		{
			_snprintf(result, resultSize, "ERROR: SET_SUPERQUICK needs 0..3");
		}
		else
		{
			t.gamevisuals.bSuperQuickObjects = t.visuals.bSuperQuickObjects = (n != 0);
			if (n != 0) t.gamevisuals.iSuperQuickLevel = t.visuals.iSuperQuickLevel = n;
			wi::renderer::gg_super_quick_objects = n;
			static const char* gg_sq_names[4] = { "off (stock shader)", "FLAT - no texture fetch at all",
				"AMBIENT - albedo only", "LIT - albedo + tiled lighting and shadows" };
			_snprintf(result, resultSize, "OK: SET_SUPERQUICK %d - %s", n, gg_sq_names[n]);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_SHADOWPERLIGHT") == 0)
	{
		// SET_SHADOWPERLIGHT <0|1>  - 1 = a moving shadow caster refreshes only the lights it is
		// near; 0 = stock, where any moving caster re-renders EVERY cached local shadow.
		// ★ Not routed through visuals: this is a safety switch, not a per-level setting, so it is
		// deliberately NOT clobbered by the per-frame push the way the tuning sliders are.
		int n = -1;
		if (sscanf_s(arg, "%d", &n) < 1 || n < 0 || n > 1)
		{
			_snprintf(result, resultSize, "ERROR: SET_SHADOWPERLIGHT needs 0 or 1");
		}
		else
		{
			wi::renderer::gg_shadow_perlight_invalidate = n;
			_snprintf(result, resultSize,
				"OK: SET_SHADOWPERLIGHT %d - a moving caster now refreshes %s.", n,
				n ? "only the lights it is actually near" : "EVERY cached local shadow (stock)");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_SHADOWRESSTEPS") == 0)
	{
		// SET_SHADOWRESSTEPS <0..16>  (0 = stock continuous sizing)
		int n = -1;
		if (sscanf_s(arg, "%d", &n) < 1 || n < 0 || n > 16)
		{
			_snprintf(result, resultSize, "ERROR: SET_SHADOWRESSTEPS needs 0..16 (0 = stock)");
		}
		else
		{
			// ⚠ Same reason as SET_OCCLUSIONHISTORY: the per-frame push from t.visuals wins.
			t.gamevisuals.iShadowResSteps = t.visuals.iShadowResSteps = n;
			wi::renderer::gg_shadow_res_steps = n;
			_snprintf(result, resultSize,
				"OK: SET_SHADOWRESSTEPS %d - local shadow resolution snaps to %s. 0 reproduces stock, "
				"where every frame of camera translation re-renders every cached local shadow.",
				n, (n == 0) ? "nothing (continuous)" : "discrete steps");
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SET_OCCLUSIONHISTORY") == 0)
	{
		// SET_OCCLUSIONHISTORY <1..32> - consecutive occluded frames required before a cull.
		// ★ Stock Wicked required all 32, which is why occlusion culling did nothing while the
		// camera turned. Measured on TESTPRO2 at 180 deg/s: 649 draws/frame rotating vs 55
		// parked, and occlusion ON vs OFF made no measurable difference while rotating.
		int n = 0;
		if (sscanf_s(arg, "%d", &n) < 1 || n < 1 || n > 32)
		{
			_snprintf(result, resultSize, "ERROR: SET_OCCLUSIONHISTORY needs 1..32 frames");
		}
		else
		{
			// ⚠ Write the VISUALS field, not the engine global: master_part1.cpp pushes
			// t.visuals.iOcclusionCullDelay into the engine every frame, so setting the global
			// here would be overwritten before the next frame rendered.
			t.gamevisuals.iOcclusionCullDelay = t.visuals.iOcclusionCullDelay = n;
			wi::scene::gg_occlusion_history_mask = (n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u);
			_snprintf(result, resultSize,
				"OK: SET_OCCLUSIONHISTORY %d frames (mask 0x%08X). 32 = stock Wicked, which cannot "
				"engage during camera rotation.", n, wi::scene::gg_occlusion_history_mask);
		}
		result[resultSize - 1] = 0;
		return true;
	}
	if (_stricmp(cmd, "SPIN_CAMERA") != 0) return false;
	{
		// SPIN_CAMERA <degrees_per_second>   (0 = stop)
		//
		// ★ Holds the camera IN MOTION so that ordinary sampling measures the moving cost.
		// Lee's slowdown only exists while the camera turns, and a file-based harness round-trip is
		// about a second - so DUMP_PROFILER/GET_GPUMS/GET_PERF_DATA always used to run against a
		// stationary camera no matter how the sweep was written. With a spin running, the profiler's
		// own 20-frame average becomes the steady-state rotating number.
		//
		// Editor only, because it drives editorfreeflight. 120 deg/s is a brisk look-around; 360 is
		// a full turn a second, which is faster than anyone plays but is the stress case.
		extern float gg_spin_deg_per_sec;
		float rate = 0.0f;
		const int got = sscanf_s(arg, "%f", &rate);
		const char* state = AutoHarness_GetAppState();
		if (got < 1)
		{
			_snprintf(result, resultSize, "ERROR: SPIN_CAMERA needs <degrees_per_second> (0 stops)");
		}
		else if (strcmp(state, "editor") != 0 && rate != 0.0f)
		{
			_snprintf(result, resultSize,
				"ERROR: SPIN_CAMERA drives the editor free-flight camera, so it only works in the "
				"level editor (state: %s). Sending 0 to stop is always allowed.", state);
		}
		else
		{
			gg_spin_deg_per_sec = rate;
			_snprintf(result, resultSize,
				"OK: SPIN_CAMERA %.1f deg/s (%s). Camera now pos=(%.2f,%.2f,%.2f) ang=(%.2f,%.2f). "
				"Sample with DUMP_PROFILER / GET_GPUMS / GET_PERF_DATA while this is running.",
				rate, (rate == 0.0f) ? "stopped" : "spinning",
				t.editorfreeflight.c.x_f, t.editorfreeflight.c.y_f, t.editorfreeflight.c.z_f,
				t.editorfreeflight.c.angx_f, t.editorfreeflight.c.angy_f);
		}
		result[resultSize - 1] = 0;
	}
	return true;
}

void AutoHarness_CheckForCommand(void)
{
	if (g_tvCycleActive)
	{
		static ULONGLONG tvLastSwitch = 0;
		ULONGLONG now = GetTickCount64();
		if (now - tvLastSwitch >= 3000)
		{
			tvLastSwitch = now;
			wi::renderer::gg_debugvis = (wi::renderer::gg_debugvis + 1) % 21;
		}
	}

	Sotan_Tick(); // DUMP_SOTAN two-frame streamout snapshot

	// consecutive-frame capture (see BURST_FRAMES)
	if (g_burstFramesRemaining > 0)
	{
		char burstpath[MAX_PATH];
		_snprintf(burstpath, sizeof(burstpath), "Files/screenshots/frame_%03d.png", g_burstFrameIndex++);
		burstpath[sizeof(burstpath) - 1] = 0;
		wi::helper::screenshot(wi::graphics::GetDevice()->GetBackBuffer(&master.swapChain), burstpath);
		g_burstFramesRemaining--;
	}

	AutoHarness_SkinWatchTick();
	AutoHarness_TerrainEditTick();
	AutoHarness_GrassPaintTick();   // GGMAX 3.118
	AutoHarness_TreePaintTick();    // GGMAX 3.123
	AutoHarness_VerifyTick();
	// Init paths and uptime tracking on first call
	if (!s_initialized)
	{
		AutoHarness_InitPaths();
		s_startTick = GetTickCount();
		s_initialized = true;
	}

	// Process deferred key-up: send WM_KEYUP after key was held down for enough frames
	if (s_pendingKeyUpVK != 0 && g_pGlob && g_pGlob->hWnd)
	{
		s_pendingKeyUpFrames--;
		if (s_pendingKeyUpFrames <= 0)
		{
			UINT scanCode = MapVirtualKey(s_pendingKeyUpVK, MAPVK_VK_TO_VSC);
			LPARAM lParamUp = (1) | (scanCode << 16) | (1 << 30) | (1 << 31);
			PostMessage(g_pGlob->hWnd, WM_KEYUP, (WPARAM)s_pendingKeyUpVK, lParamUp);
			if (s_pendingKeyUpVK < 256 && ImGui::GetCurrentContext() != nullptr) // FIX 2026-07-25: null-context guard
				ImGui::GetIO().KeysDown[s_pendingKeyUpVK] = false;
			s_pendingKeyUpVK = 0;
		}
	}

	// Fast check: does the command file exist?
	DWORD attr = GetFileAttributesA(s_cmdPath);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return; // No file, return immediately (zero overhead path)

	// Read the command file
	char cmdBuf[1024];
	cmdBuf[0] = 0;
	FILE* f = fopen(s_cmdPath, "r");
	if (!f) return;
	if (!fgets(cmdBuf, sizeof(cmdBuf), f))
		cmdBuf[0] = 0;
	fclose(f);

	// Trim trailing newline/carriage return
	int len = (int)strlen(cmdBuf);
	while (len > 0 && (cmdBuf[len - 1] == '\n' || cmdBuf[len - 1] == '\r'))
		cmdBuf[--len] = 0;

	// If file was empty, don't delete it — writer may still be flushing (race condition).
	// We'll pick it up on the next tick.
	if (len == 0) return;

	// Delete the command file now that we have valid content
	DeleteFileA(s_cmdPath);

	// Log the command
	AutoHarness_LogCommand(cmdBuf);

	// Mark harness as active so main loop keeps running even when window loses focus
	g_bAutomationActive = true;

	// Parse command and argument
	char cmd[256];
	char arg[768];
	cmd[0] = 0;
	arg[0] = 0;

	// Find first space
	char* space = strchr(cmdBuf, ' ');
	if (space)
	{
		int cmdLen = (int)(space - cmdBuf);
		if (cmdLen >= (int)sizeof(cmd)) cmdLen = (int)sizeof(cmd) - 1;
		memcpy(cmd, cmdBuf, cmdLen);
		cmd[cmdLen] = 0;

		// Skip spaces
		const char* a = space + 1;
		while (*a == ' ') a++;
		strncpy(arg, a, sizeof(arg) - 1);
		arg[sizeof(arg) - 1] = 0;
	}
	else
	{
		strncpy(cmd, cmdBuf, sizeof(cmd) - 1);
		cmd[sizeof(cmd) - 1] = 0;
	}

	// Dispatch command (32KB buffer for entity/light listing commands)
	char result[32768];
	result[0] = 0;

	if (_stricmp(cmd, "GET_STATE") == 0)
	{
		Cmd_GetState(result, sizeof(result));
	}
	else if (_stricmp(cmd, "GET_SCREEN_TEXT") == 0)
	{
		Cmd_GetScreenText(result, sizeof(result));
	}
	else if (_stricmp(cmd, "NAVIGATE") == 0)
	{
		Cmd_Navigate(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "CLICK") == 0)
	{
		Cmd_Click(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "CLICK_NODE") == 0)
	{
		Cmd_ClickNode(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "WAIT") == 0)
	{
		Cmd_Wait(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "SCREENSHOT") == 0)
	{
		Cmd_Screenshot(result, sizeof(result));
	}
	else if (_stricmp(cmd, "LIST_DEMOS") == 0)
	{
		Cmd_ListDemos(result, sizeof(result));
	}
	else if (_stricmp(cmd, "SELECT_DEMO") == 0)
	{
		Cmd_SelectDemo(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "LIST_PROJECTS") == 0)
	{
		Cmd_ListProjects(result, sizeof(result));
	}
	else if (_stricmp(cmd, "OPEN_PROJECT") == 0)
	{
		Cmd_OpenProject(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "CLICK_ONLY_LEVEL") == 0)
	{
		Cmd_ClickOnlyLevel(result, sizeof(result));
	}
	else if (_stricmp(cmd, "GET_PERF_DATA") == 0)
	{
		Cmd_GetPerfData(result, sizeof(result));
	}
	else if (_stricmp(cmd, "TOGGLE_PROFILER") == 0)
	{
		Cmd_ToggleProfiler(result, sizeof(result));
	}
	else if (_stricmp(cmd, "PRESS_ESCAPE") == 0)
	{
		Cmd_PressEscape(result, sizeof(result));
	}
	else if (_stricmp(cmd, "PRESS_KEY") == 0)
	{
		Cmd_PressKey(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "SET_CAMERA") == 0)
	{
		// SET_CAMERA <x> <y> <z> [angx angy] — relocate the editor free-flight camera (same state
		// the editor's own "travel to" writes). Used to force VT/terrain chunk re-streaming so the
		// shadow-flicker (chunk-recycle) trigger can be reproduced on demand. Editor mode only.
		float cxp = 0, cyp = 0, czp = 0, cax = 0, cay = 0;
		int got = sscanf_s(arg, "%f %f %f %f %f", &cxp, &cyp, &czp, &cax, &cay);
		const char* state = AutoHarness_GetAppState();
		if (strcmp(state, "editor") != 0)
		{
			_snprintf(result, sizeof(result), "ERROR: SET_CAMERA only works in the level editor (state: %s)", state);
		}
		else if (got < 3)
		{
			_snprintf(result, sizeof(result), "ERROR: SET_CAMERA needs <x> <y> <z> [angx angy]");
		}
		else
		{
			t.editorfreeflight.mode = 1; // free-flight so the fields below are applied each frame
			t.editorfreeflight.c.x_f = cxp;
			t.editorfreeflight.c.y_f = cyp;
			t.editorfreeflight.c.z_f = czp;
			if (got >= 5) { t.editorfreeflight.c.angx_f = cax; t.editorfreeflight.c.angy_f = cay; }
			t.cx_f = t.editorfreeflight.c.x_f;
			t.cy_f = t.editorfreeflight.c.z_f;
			_snprintf(result, sizeof(result), "OK: SET_CAMERA pos=(%.1f,%.1f,%.1f) ang=(%.1f,%.1f)",
				cxp, cyp, czp, t.editorfreeflight.c.angx_f, t.editorfreeflight.c.angy_f);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_MATERIALS") == 0)
	{
		// DUMP_MATERIALS — write every scene MaterialComponent (entity id, name, hierarchy
		// parent) to materials_dump.txt next to the exe. Diffing dumps taken across in-place
		// level reloads identifies WHICH materials leak (+3 per reload measured 2026-07-25,
		// the census fingerprint of the reload-corruption teardown gap).
		wi::scene::Scene& dmpScene = wi::scene::GetScene();
		FILE* dmpF = GGDiagFopen("materials_dump.txt", "w");
		if (dmpF != nullptr)
		{
			for (size_t dmi = 0; dmi < dmpScene.materials.GetCount(); ++dmi)
			{
				wi::ecs::Entity dme = dmpScene.materials.GetEntity(dmi);
				const wi::scene::NameComponent* dmn = dmpScene.names.GetComponent(dme);
				const wi::scene::HierarchyComponent* dmh = dmpScene.hierarchy.GetComponent(dme);
				wi::ecs::Entity dmp = (dmh != nullptr) ? dmh->parentID : wi::ecs::INVALID_ENTITY;
				const wi::scene::NameComponent* dmpn = (dmp != wi::ecs::INVALID_ENTITY) ? dmpScene.names.GetComponent(dmp) : nullptr;
				fprintf(dmpF, "%llu\t%s\tparent=%llu(%s)\n",
					(unsigned long long)dme, dmn ? dmn->name.c_str() : "?",
					(unsigned long long)dmp, dmpn ? dmpn->name.c_str() : "?");
			}
			fclose(dmpF);
			_snprintf(result, sizeof(result), "OK: DUMP_MATERIALS wrote %d materials to materials_dump.txt", (int)dmpScene.materials.GetCount());
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_MATERIALS could not open materials_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_ENTITY") == 0)
	{
		// DUMP_ENTITY <name-substr> — for every named object whose name contains the substring
		// (case-insensitive), dump per-subset material texture state: texture name, LIVE
		// descriptor index of the current texture object, and the CACHED composed
		// ShaderMaterial descriptor (gg_shader_cache) + validity. A basecolor slot whose
		// cached descriptor equals the normal map's live descriptor = the "blue tree" mixup.
		wi::scene::Scene& deScene = wi::scene::GetScene();
		wi::graphics::GraphicsDevice* deDev = wi::graphics::GetDevice();
		auto deContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		FILE* deF = GGDiagFopen("entity_dump.txt", "w");
		int deMatches = 0;
		if (deF != nullptr)
		{
			for (size_t dei = 0; dei < deScene.objects.GetCount(); ++dei)
			{
				wi::ecs::Entity dee = deScene.objects.GetEntity(dei);
				const wi::scene::NameComponent* den = deScene.names.GetComponent(dee);
				if (den == nullptr || !deContains(den->name, arg)) continue;
				const wi::scene::ObjectComponent& deo = deScene.objects[dei];
				const wi::scene::TransformComponent* det = deScene.transforms.GetComponent(dee);
				const wi::scene::MeshComponent* dem = deScene.meshes.GetComponent(deo.meshID);
				deMatches++;
				if (deMatches > 40) continue;
				fprintf(deF, "OBJ entity=%llu name=\"%s\" renderable=%d pos=(%.0f,%.0f,%.0f) meshID=%llu subsets=%d\n",
					(unsigned long long)dee, den->name.c_str(), deo.IsRenderable() ? 1 : 0,
					det ? det->world._41 : 0.0f, det ? det->world._42 : 0.0f, det ? det->world._43 : 0.0f,
					(unsigned long long)deo.meshID, dem ? (int)dem->subsets.size() : -1);
				if (dem == nullptr) continue;
				for (const auto& des : dem->subsets)
				{
					const wi::scene::MaterialComponent* dmat = deScene.materials.GetComponent(des.materialID);
					if (dmat == nullptr) { fprintf(deF, "  subset mat=%llu MISSING\n", (unsigned long long)des.materialID); continue; }
					fprintf(deF, "  mat=%llu cacheValid=%d cacheEpoch=%u\n", (unsigned long long)des.materialID,
						dmat->gg_shader_cache_valid ? 1 : 0, dmat->gg_shader_cache_epoch);
					static const int deSlots[2] = { wi::scene::MaterialComponent::BASECOLORMAP, wi::scene::MaterialComponent::NORMALMAP };
					static const char* deSlotNames[2] = { "base", "norm" };
					for (int dq = 0; dq < 2; dq++)
					{
						const auto& dts = dmat->textures[deSlots[dq]];
						int deLive = -1;
						if (dts.resource.IsValid())
							deLive = deDev->GetDescriptorIndex(&dts.resource.GetTexture(), wi::graphics::SubresourceType::SRV);
						fprintf(deF, "    %s name=\"%s\" resValid=%d liveDesc=%d cachedDesc=%d\n",
							deSlotNames[dq], dts.name.c_str(), dts.resource.IsValid() ? 1 : 0, deLive,
							dmat->gg_shader_cache.textures[deSlots[dq]].texture_descriptor);
					}
				}
			}
			fprintf(deF, "MATCHES %d\n", deMatches);
			fclose(deF);
			_snprintf(result, sizeof(result), "OK: DUMP_ENTITY \"%s\" matches=%d -> entity_dump.txt", arg, deMatches);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_ENTITY could not open entity_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_SKINGEO") == 0)
	{
		// DUMP_SKINGEO <name-substr> — skinned-tangent pipeline forensics. For every named object
		// whose name contains the substring, print the CPU-side mesh buffer-view descriptors
		// (bind-pose vb_nor/vb_tan vs skinned streamout so_nor/so_tan) plus vertex array counts
		// and armature binding. Diff against DUMP_GEOMETRY's GPU record: geometry.vb_tan ==
		// so_tan.srv -> pixels lit with SKINNED tangents (correct); == vb_tan.srv -> BIND-POSE
		// tangents on an animated mesh (normal-map swimming); == -1 -> pixel-shader derivative
		// fallback (patchy per-quad flicker scaling with normal strength).
		wi::scene::Scene& sgScene = wi::scene::GetScene();
		auto sgContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		FILE* sgF = GGDiagFopen("skingeo_dump.txt", "w");
		int sgMatches = 0;
		if (sgF != nullptr)
		{
			for (size_t sgi = 0; sgi < sgScene.objects.GetCount(); ++sgi)
			{
				wi::ecs::Entity sge = sgScene.objects.GetEntity(sgi);
				const wi::scene::NameComponent* sgn = sgScene.names.GetComponent(sge);
				if (sgn == nullptr || !sgContains(sgn->name, arg)) continue;
				const wi::scene::ObjectComponent& sgo = sgScene.objects[sgi];
				if (!sgo.IsRenderable()) continue;
				const wi::scene::MeshComponent* sgm = sgScene.meshes.GetComponent(sgo.meshID);
				if (sgm == nullptr) continue;
				sgMatches++;
				if (sgMatches > 60) continue;
				const wi::scene::TransformComponent* sgt = sgScene.transforms.GetComponent(sge);
				fprintf(sgF, "OBJ entity=%llu name=\"%s\" pos=(%.0f,%.0f,%.0f) meshID=%llu armature=%llu\n",
					(unsigned long long)sge, sgn->name.c_str(),
					sgt ? sgt->world._41 : 0.0f, sgt ? sgt->world._42 : 0.0f, sgt ? sgt->world._43 : 0.0f,
					(unsigned long long)sgo.meshID, (unsigned long long)sgm->armatureID);
				fprintf(sgF, "  counts pos=%d nor=%d tan=%d bon=%d  streamoutValid=%d\n",
					(int)sgm->vertex_positions.size(), (int)sgm->vertex_normals.size(),
					(int)sgm->vertex_tangents.size(), (int)sgm->vertex_boneindices.size(),
					sgm->streamoutBuffer.IsValid() ? 1 : 0);
				fprintf(sgF, "  bindpose vb_nor=%d vb_tan=%d | skinned so_pos=%d so_nor=%d so_tan=%d\n",
					sgm->vb_nor.descriptor_srv, sgm->vb_tan.descriptor_srv,
					sgm->so_pos.descriptor_srv, sgm->so_nor.descriptor_srv, sgm->so_tan.descriptor_srv);
				fprintf(sgF, "  UAV so_pos=%d so_nor=%d so_tan=%d | offs %llu/%llu/%llu sizes %llu/%llu/%llu\n",
					sgm->so_pos.descriptor_uav, sgm->so_nor.descriptor_uav, sgm->so_tan.descriptor_uav,
					(unsigned long long)sgm->so_pos.offset, (unsigned long long)sgm->so_nor.offset, (unsigned long long)sgm->so_tan.offset,
					(unsigned long long)sgm->so_pos.size, (unsigned long long)sgm->so_nor.size, (unsigned long long)sgm->so_tan.size);
				const char* sgVerdict = "STATIC (no armature)";
				if (sgm->armatureID != wi::ecs::INVALID_ENTITY)
				{
					if (!sgm->streamoutBuffer.IsValid()) sgVerdict = "BUG: armature but NO streamout buffer";
					else if (sgm->so_tan.descriptor_srv < 0 && sgm->vb_tan.descriptor_srv >= 0) sgVerdict = "BUG: skinned but so_tan MISSING (bind-pose or fallback tangents)";
					else if (sgm->vb_tan.descriptor_srv < 0) sgVerdict = "NO TANGENTS AT ALL (derivative fallback)";
					else sgVerdict = "OK: skinned tangents available";
				}
				fprintf(sgF, "  verdict: %s\n", sgVerdict);
			}
			fprintf(sgF, "MATCHES %d\n", sgMatches);
			fclose(sgF);
			_snprintf(result, sizeof(result), "OK: DUMP_SKINGEO \"%s\" matches=%d -> skingeo_dump.txt", arg, sgMatches);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_SKINGEO could not open skingeo_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_ENTITY_VIS") == 0)
	{
		// SET_ENTITY_VIS <name-substr> <0|1> — SetRenderable on every named object whose name
		// contains the substring. Elimination probe: if the blue palms persist with the
		// verified palm entities hidden, something OUTSIDE the object system draws them.
		char evName[128] = { 0 }; int evVis = 1;
		if (sscanf_s(arg, "%127s %d", evName, (unsigned)sizeof(evName), &evVis) == 2)
		{
			wi::scene::Scene& evScene = wi::scene::GetScene();
			auto evContains = [](const std::string& hay, const char* needle) -> bool {
				std::string h = hay, n = needle;
				for (auto& c : h) c = (char)tolower((unsigned char)c);
				for (auto& c : n) c = (char)tolower((unsigned char)c);
				return h.find(n) != std::string::npos;
			};
			int evCount = 0;
			for (size_t evi = 0; evi < evScene.objects.GetCount(); ++evi)
			{
				const wi::scene::NameComponent* evn = evScene.names.GetComponent(evScene.objects.GetEntity(evi));
				if (evn == nullptr || !evContains(evn->name, evName)) continue;
				evScene.objects[evi].SetRenderable(evVis != 0);
				evCount++;
			}
			_snprintf(result, sizeof(result), "OK: SET_ENTITY_VIS \"%s\" -> %d on %d objects", evName, evVis, evCount);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_ENTITY_VIS needs <name-substr> <0|1>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_STREAMING") == 0)
	{
		// SET_STREAMING <0|1> — pause/resume the engine texture-streaming system (see
		// wi::resourcemanager::gg_streaming_paused). Corruption-hunt A/B probe: 0 pauses
		// all min-lod updates, mip stream in/out and texture replacements.
		int sv = atoi(arg);
		wi::resourcemanager::gg_streaming_paused = (sv == 0);
		_snprintf(result, sizeof(result), "OK: SET_STREAMING %d (paused=%d)", sv, sv == 0 ? 1 : 0);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_STREAM") == 0)
	{
		// DUMP_STREAM — texture-streaming ground truth: every scene material's name, its raw
		// GPU mip-feedback word (textureStreamingFeedbackMapped), and each texture slot's
		// CURRENT resolution/mips + resource name. Written to stream_dump.txt (exe dir).
		// The instrument for "which materials actually produce feedback and did their
		// textures stream up" (2026-08-01 streaming-enable hunt).
		FILE* sdf = GGDiagFopen("stream_dump.txt", "w");
		if (sdf)
		{
			wi::scene::Scene& sdScene = wi::scene::GetScene();
			fprintf(sdf, "materials=%d feedbackMapped=%p\n", (int)sdScene.materials.GetCount(), (const void*)sdScene.textureStreamingFeedbackMapped);
			int sdFb = 0;
			for (size_t sdi = 0; sdi < sdScene.materials.GetCount(); ++sdi)
			{
				const wi::scene::MaterialComponent& sdMat = sdScene.materials[sdi];
				uint32_t sdWord = 0;
				if (sdScene.textureStreamingFeedbackMapped != nullptr)
					sdWord = sdScene.textureStreamingFeedbackMapped[sdi];
				// only dump materials with feedback OR with any valid texture (keeps file readable)
				bool sdAnyTex = false;
				for (auto& sdSlot : sdMat.textures) { if (sdSlot.resource.IsValid() && sdSlot.resource.GetTexture().IsValid()) { sdAnyTex = true; break; } }
				if (!sdAnyTex && sdWord == 0) continue;
				if (sdWord != 0) sdFb++;
				wi::ecs::Entity sdEnt = sdScene.materials.GetEntity(sdi);
				const wi::scene::NameComponent* sdName = sdScene.names.GetComponent(sdEnt);
				fprintf(sdf, "[%4d] fb=0x%08X streamDis=%d \"%s\"\n", (int)sdi, sdWord,
					sdMat.IsTextureStreamingDisabled() ? 1 : 0,
					sdName ? sdName->name.c_str() : "?");
				for (int sdt = 0; sdt < wi::scene::MaterialComponent::TEXTURESLOT_COUNT; ++sdt)
				{
					auto& sdSlot = sdMat.textures[sdt];
					if (!sdSlot.resource.IsValid() || !sdSlot.resource.GetTexture().IsValid()) continue;
					const wi::graphics::TextureDesc& sdDesc = sdSlot.resource.GetTexture().desc;
					fprintf(sdf, "    slot%d %ux%u mips=%u uvset=%u %s\n", sdt, sdDesc.width, sdDesc.height,
						sdDesc.mip_levels, sdSlot.uvset, sdSlot.name.c_str());
				}
			}
			fprintf(sdf, "materialsWithFeedback=%d\n", sdFb);
			fclose(sdf);
			_snprintf(result, sizeof(result), "OK: DUMP_STREAM written to stream_dump.txt");
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_STREAM could not open stream_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_SVTATLAS") == 0)
	{
		// SET_SVTATLAS <height> — terrain SVT physical atlas height (engine 1.71). 16384 =
		// stock (768 MB tile pool), 8192 = half. Measured tile residency is only ~26% of the
		// stock atlas on every demo, so this is the lever for that fixed cost. Applies when
		// the atlas is NEXT created: set it, then load/reload a level.
		int svtH = atoi(arg);
		if (svtH >= 2048 && svtH <= 16384)
		{
			wi::terrain::gg_svt_atlas_height = (uint32_t)svtH;
			_snprintf(result, sizeof(result), "OK: SET_SVTATLAS %d (tile pool ~%d MB; load/reload a level to apply)",
				svtH, (int)((768.0 * svtH) / 16384.0));
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_SVTATLAS needs a height 2048..16384 (stock 16384)");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_VRAM") == 0)
	{
		// DUMP_VRAM [tag] — full per-resource VRAM census (engine 1.70): every live GPU
		// allocation with allocated bytes, dimensions/format/flags and debug name (content
		// textures carry their file path), sorted largest first, plus a header reconciling
		// the census against D3D12MA's own accounting and the driver's reported usage.
		// Written to Files\vram_census[_tag].txt.
		extern void GG_DumpVRAMCensus(const char* path);
		char vcPath[MAX_PATH];
		if (arg && arg[0])
		{
			char vcTag[64]; _snprintf(vcTag, sizeof(vcTag), "%s", arg); vcTag[sizeof(vcTag) - 1] = 0;
			for (char* p = vcTag; *p; ++p) { if (*p == ' ' || *p == '\\' || *p == '/' || *p == ':') *p = '_'; }
			_snprintf(vcPath, sizeof(vcPath), "vram_census_%s.txt", vcTag);
		}
		else
		{
			_snprintf(vcPath, sizeof(vcPath), "vram_census.txt");
		}
		vcPath[sizeof(vcPath) - 1] = 0;
		GG_DumpVRAMCensus(vcPath);
		_snprintf(result, sizeof(result), "OK: DUMP_VRAM written to %s (game CWD = Files dir)", vcPath);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_MAPAD") == 0)
	{
		// DUMP_MAPAD [tag] — attribute the D3D12MA block padding (engine 1.83).
		//
		// The audit's `pad` column is just `d3d12ma_blocks - d3d12ma_allocated`: 363 MB mean,
		// 121-601 range, and never once broken down. Three owners with three different fixes —
		// trailing space in partly-filled 64 MB blocks (fix = smaller blocks), fragmentation
		// between live allocations (fix = defragmentation), or committed-allocation heap rounding
		// (not ours at all). Writes a per-heap-type table (block/alloc bytes, unused-range count
		// and size extremes — a few huge ranges means trailing space, thousands of small ones
		// means fragmentation) followed by D3D12MA's own detailed JSON map, which gives the
		// per-block fill ratio no aggregate can. → Files\ma_padding[_tag].txt.
		extern void GG_DumpMAPadding(const char* path);
		char mpPath[MAX_PATH];
		if (arg && arg[0])
		{
			char mpTag[64]; _snprintf(mpTag, sizeof(mpTag), "%s", arg); mpTag[sizeof(mpTag) - 1] = 0;
			for (char* p = mpTag; *p; ++p) { if (*p == ' ' || *p == '\\' || *p == '/' || *p == ':') *p = '_'; }
			_snprintf(mpPath, sizeof(mpPath), "ma_padding_%s.txt", mpTag);
		}
		else
		{
			_snprintf(mpPath, sizeof(mpPath), "ma_padding.txt");
		}
		mpPath[sizeof(mpPath) - 1] = 0;
		GG_DumpMAPadding(mpPath);
		_snprintf(result, sizeof(result), "OK: DUMP_MAPAD written to %s (game CWD = Files dir)", mpPath);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_LOWVRAM") == 0)
	{
		// SET_LOWVRAM <0|1> [grassdistcap] — the low-VRAM ("fit a 4 GB card") preset, same switch
		// as setup.ini `lowvram`. Currently caps the grass draw distance, which is the one grass
		// lever proven to save real memory (~1.14 GB at 750) without touching placement or
		// density. RELOAD THE LEVEL after flipping: grass entities are built at chunk-spawn time,
		// so an existing level keeps the systems it already created.
		// NOTE the lazy-PSO member of the preset cannot be turned on from here — object pipelines
		// are built once at LoadShaders, long before any harness command lands. Use setup.ini
		// `lowvram=1` for that half. This command drives the grass cap, which IS runtime-settable.
		// SET_LOWVRAM <0|1> [grassdistcap] [grassdensitypercent]
		extern bool gg_lowvram;
		extern float gg_lowvram_grass_dist;
		extern float gg_lowvram_grass_density;
		extern void GGSetLowVRAM(int); // drives the MACHINE half of the OR; the per-level
		                               // checkbox (visuals.bLowVRAM) is the other half
		char lvp[32] = { 0 }; float lvd = 0.0f; float lvden = 0.0f;
		const int got = sscanf_s(arg, "%31s %f %f", lvp, (unsigned)sizeof(lvp), &lvd, &lvden);
		if (got >= 1)
		{
			GGSetLowVRAM(atoi(lvp));
			if (got >= 2 && lvd > 0.0f) gg_lowvram_grass_dist = lvd;
			if (got >= 3 && lvden > 0.0f && lvden <= 100.0f) gg_lowvram_grass_density = lvden * 0.01f;
			extern void GGApplyVisualsNow();
			GGApplyVisualsNow(); // SSR + shadow-cap members apply immediately; grass on reload
		}
		_snprintf(result, sizeof(result), "OK: SET_LOWVRAM %d grassdistcap=%.0f grassdensity=%.0f%% (GGMAX 3.116: grass rebuilds live, nearest first - no reload)",
			gg_lowvram ? 1 : 0, gg_lowvram_grass_dist, gg_lowvram_grass_density * 100.0f);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SHOW_GAMESETTINGS") == 0)
	{
		// SHOW_GAMESETTINGS <0|1> — open/close the Game Settings window (Graphics and
		// Performance etc.) so panel-level UI can be screenshot-verified by the harness.
		Game_Settings_Window = (atoi(arg) != 0);
		_snprintf(result, sizeof(result), "OK: Game Settings window %s", Game_Settings_Window ? "OPEN" : "CLOSED");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SELECT_ENTITY") == 0)
	{
		// SELECT_ENTITY <entity element index | 0 to deselect> - put an entity into the editor's
		// widget selection, exactly as a viewport click does (M-GridEdit_part6.cpp:1973-1981).
		// Needed because CLICK only targets named ImGui widgets, so the "an entity is selected"
		// editor state - which is what costs ~14 ms of common_loop - could not otherwise be
		// reproduced from the harness.
		// widget_updatewidgetobject only CLEARS a stale selection; the function that promotes
		// pickedObject -> activeObject (which is the variable the expensive editor path gates on,
		// M-GridEdit_part6.cpp:956) is widget_check_for_new_object_selection.
		void widget_updatewidgetobject(void);
		void widget_check_for_new_object_selection(void);
		const int idx = atoi(arg);
		if (idx <= 0)
		{
			t.widget.pickedObject = 0;
			t.widget.pickedEntityIndex = 0;
			widget_updatewidgetobject();
			t.widget.activeObject = 0;
			_snprintf(result, sizeof(result), "OK: deselected (activeObject=%d)", t.widget.activeObject);
		}
		else if (idx >= (int)t.entityelement.size() || t.entityelement[idx].obj <= 0)
		{
			_snprintf(result, sizeof(result), "FAIL: entity %d invalid (list=%d)", idx, g.entityelementlist);
		}
		else
		{
			t.widget.pickedEntityIndex = idx;
			t.entityelement[idx].editorlock = 0;
			t.widget.pickedObject = t.entityelement[idx].obj;
			t.widget.activeObject = 0; // force the promote path to run
			widget_check_for_new_object_selection();

			// Optional second arg "oncursor": also put the entity ON THE CURSOR, which is what
			// a real viewport click does when pref.iEnableDragDropEntityMode is on
			// (M-GridEdit_part6.cpp:2322/2338 set t.gridentity + t.gridentityobj). That is a
			// different and more expensive editor state than widget-selection alone -
			// input_calculatelocalcursor() then calls WickedCall_GetPick2 DIRECTLY, bypassing
			// the Perf-P.3 pick cache. Reproducing it is the only way to measure it here.
			// NOTE: this is a diagnostic state; do not save the level after using it.
			const char* pSpace = strchr(arg, ' ');
			if (pSpace && strstr(pSpace, "oncursor"))
			{
				t.gridentity = idx;
				t.gridentityobj = t.entityelement[idx].obj;
			}
			_snprintf(result, sizeof(result), "OK: selected entity %d (obj=%d, activeObject=%d, gridentity=%d gridentityobj=%d)",
				idx, t.entityelement[idx].obj, t.widget.activeObject, t.gridentity, t.gridentityobj);
		}
		result[sizeof(result) - 1] = 0;
	}
	// GGMAX 3.27: OR'd into an existing arm, deliberately. This ladder sits exactly at MSVC's
	// C1061 nesting limit - adding one more `else if` tips it over, and the error it reports
	// points ~2000 lines away, so it reads as an unrelated problem. Extra conditions are free;
	// extra arms are not.
	else if (AutoHarness_SpinCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_StandaloneCommands(cmd, arg, result, sizeof(result)))
	{
		// handled in the helper (see above the dispatch function)
	}
	else if (AutoHarness_OutlineCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_BulletHoleCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_TracerCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_TreeWindCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_MouseCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_PostFXCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_GrassDensityCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_ArmsCommands(cmd, arg, result, sizeof(result))
		|| AutoHarness_TreeLoadCommands(cmd, arg, result, sizeof(result))) // C1061: share the arm, never add one
	{
		// handled in the helper (see above the dispatch function)
	}
	else if (AutoHarness_TransparencyCommands(cmd, arg, result, sizeof(result))
	      || AutoHarness_EnvProbeCommands(cmd, arg, result, sizeof(result))
	      || AutoHarness_ReflectionCommands(cmd, arg, result, sizeof(result))) // C1061: share the block, don't extend the ladder
	{
		// handled in the helper (see above the dispatch function)
	}
	else if (_stricmp(cmd, "DUMP_PROFILER") == 0)
	{
		// DUMP_PROFILER - the cached wi::profiler text (captured in Compose so GPU sub-ranges
		// are present). GET_PROFILER_STATUS only serves this in "game" state; the editor-side
		// costs need it in "editor" state too.
		extern std::string GGPerf_GetCachedProfilerText();
		std::string t2 = GGPerf_GetCachedProfilerText();
		if (t2.empty())
		{
			_snprintf(result, sizeof(result), "EMPTY: profiler not enabled? (wi::profiler::IsEnabled()==%d) - tick 'Enable the 3D Editor Profiler' or use ENABLE_PROFILER",
				wi::profiler::IsEnabled() ? 1 : 0);
		}
		else
		{
			_snprintf(result, sizeof(result), "CPU_FRAME_MS: %.2f\nGPU_FRAME_MS: %.2f\nPROFILER_DATA:\n%s",
				wi::profiler::GetCPUFrameTime(), wi::profiler::GetGPUFrameTime(), t2.c_str());
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_PICKUPDATE") == 0)
	{
		// SET_PICKUPDATE <0|1|2> - A/B the widget-pick scene refresh.
		//   2 = force a full Scene::Update every frame (the pre-fix DX12 behaviour)
		//   1 = gated (the fix: skip when cursor+camera unchanged and no button held)
		//   0 = never (diagnostic ceiling; breaks widget dragging)
		// Mode 2 lets the cost be measured WITHOUT needing an entity selected, because the
		// call site (widget_getplanepos) is otherwise only reached while something is.
		// The mode is deliberately sticky for A/B use, but only within [0,2]; anything else
		// (including "default" or a typo) restores the shipping default of 1 so a stray
		// auto_command.txt cannot leave the editor in a diagnostic mode indefinitely.
		extern int g_iPickSceneUpdateMode, g_iPickSceneUpdateRuns, g_iPickSceneUpdateSkips;
		// strict parse: only the literal arguments "0", "1", "2" select a mode; anything
		// else (garbage, "default", empty) restores the shipping default. atoi() would have
		// mapped garbage to 0 = "never", which silently breaks widget dragging.
		int reqmode = 1;
		bool exact = (strcmp(arg, "0") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "2") == 0);
		if (exact) reqmode = arg[0] - '0';
		g_iPickSceneUpdateMode = reqmode;
		g_iPickSceneUpdateRuns = 0;
		g_iPickSceneUpdateSkips = 0;
		_snprintf(result, sizeof(result), "OK: pick scene-update mode %d (counters reset)%s",
			g_iPickSceneUpdateMode, exact ? "" : " [unrecognized arg -> default 1]");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "FORCE_PICKUPDATE") == 0)
	{
		// FORCE_PICKUPDATE <n> - run WickedCall_UpdateSceneForPick n times right now and
		// report the wall time, so the per-call cost can be measured directly instead of
		// inferred from an FPS delta.
		void WickedCall_UpdateSceneForPick(void);
		extern int g_iPickSceneUpdateMode;
		const int n = std::max(1, std::min(200, atoi(arg)));
		const int saved = g_iPickSceneUpdateMode;
		g_iPickSceneUpdateMode = 2; // force real work
		auto t0 = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < n; i++) WickedCall_UpdateSceneForPick();
		auto t1 = std::chrono::high_resolution_clock::now();
		g_iPickSceneUpdateMode = saved;
		const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
		_snprintf(result, sizeof(result), "OK: %d full Scene::Update(0) calls took %.2f ms total = %.3f ms each",
			n, ms, ms / (double)n);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_EMITTERS") == 0)
	{
		// DUMP_EMITTERS - per-emitter state for WPE particle debugging. Reports the things
		// that decide whether an effect is on screen: the render gates (visible/active), the
		// WORLD position (a wrong hierarchy attach shows up here), and the live GPU alive
		// count, which separates "not simulating" from "simulating but not drawn".
		wiScene::Scene& sc = wiScene::GetScene();
		int written = 0;
		written += _snprintf(result + written, sizeof(result) - written,
			"EMITTERS: %d\n", (int)sc.emitters.GetCount());
		for (int i = 0; i < (int)sc.emitters.GetCount() && written < (int)sizeof(result) - 300; i++)
		{
			wiEmittedParticle& ec = sc.emitters[i];
			wi::ecs::Entity e = sc.emitters.GetEntity(i);
			wiScene::HierarchyComponent* hier = sc.hierarchy.GetComponent(e);
			wiScene::TransformComponent* tr = sc.transforms.GetComponent(e);
			wiScene::NameComponent* nm = sc.names.GetComponent(e);
			wiScene::MaterialComponent* mt = sc.materials.GetComponent(e);
			float wx = 0, wy = 0, wz = 0;
			if (tr) { XMFLOAT3 p = tr->GetPosition(); wx = p.x; wy = p.y; wz = p.z; }
			written += _snprintf(result + written, sizeof(result) - written,
				"[%d] e=%u par=%u '%s' vis=%d act=%d flags=0x%X cnt=%.2f life=%.2f size=%.2f max=%u "
				"world=(%.1f,%.1f,%.1f) alive=%u mat=%d blend=%d basecol.a=%.2f tex=%d fadein=%.3f st=%d\n",
				i, (unsigned)e, (unsigned)(hier ? hier->parentID : 0),
				nm ? nm->name.c_str() : "?",
				ec.IsVisible() ? 1 : 0, ec.IsActive() ? 1 : 0, ec._flags,
				ec.count, ec.life, ec.size, ec.GetMaxParticleCount(),
				wx, wy, wz, ec.statistics.aliveCount,
				mt ? 1 : 0, mt ? (int)mt->userBlendMode : -1,
				mt ? mt->baseColor.w : -1.0f,
				(mt && mt->textures[wiScene::MaterialComponent::BASECOLORMAP].resource.IsValid()) ? 1 : 0,
				ec.fadein_time, (int)ec.shaderType);	// 3.121: st 3 = SOFT_LIGHTING (lit)
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "WPE_PREVIEW") == 0)
	{
		// WPE_PREVIEW <relative .pe path> - replicate EXACTLY what the editor's Preview
		// checkbox does (LoadWPE then emitter actions 1=burst, 4=restart, 5=visible), then
		// park the effect just in front of the camera so it must be on screen if it draws.
		uint32_t WickedCall_LoadWPE(char* filename);
		void WickedCall_PerformEmitterAction(int iAction, uint32_t emitter_root);
		bool WickedCall_ParticleEffectPosition(uint32_t root, float fX, float fY, float fZ);
		char pefile[MAX_PATH];
		strncpy(pefile, arg, sizeof(pefile) - 1); pefile[sizeof(pefile) - 1] = 0;
		uint32_t root = WickedCall_LoadWPE(pefile);
		if (root == 0)
		{
			_snprintf(result, sizeof(result), "FAIL: LoadWPE returned 0 for '%s'", pefile);
		}
		else
		{
			WickedCall_PerformEmitterAction(1, root);
			WickedCall_PerformEmitterAction(4, root);
			WickedCall_PerformEmitterAction(5, root);
			// Hand the effect to the REAL preview path: setting these two globals makes
			// RenderPreviewEmitter() drive its position every frame from the selected entity,
			// exactly as ticking the Preview checkbox does. Without this the harness would be
			// testing a different code path from the one the user reported.
			extern uint32_t PreviewWPERoot;
			extern bool bPreviewWPE;
			PreviewWPERoot = root;
			bPreviewWPE = true;
			// Park it in front of the camera as well. RenderPreviewEmitter only repositions
			// when an editor entity is active, so with nothing selected this leaves the effect
			// where we can actually see it - which is what makes screenshot A/B possible.
			const wi::scene::CameraComponent& cam = wiScene::GetCamera();
			float px = cam.Eye.x + cam.At.x * 250.0f;
			float py = cam.Eye.y + cam.At.y * 250.0f;
			float pz = cam.Eye.z + cam.At.z * 250.0f;
			WickedCall_ParticleEffectPosition(root, px, py, pz);
			_snprintf(result, sizeof(result), "OK: root=%u handed to RenderPreviewEmitter, placed at (%.1f,%.1f,%.1f), emitters now %d",
				root, px, py, pz, (int)wiScene::GetScene().emitters.GetCount());
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "WPE_CLONETEST") == 0)
	{
		// WPE_CLONETEST <relative .pe path> - reproduce, in the editor and without needing a
		// test game, exactly what preload_wicked_particle_effect() (M-Entity_part5.cpp) does
		// when it builds its 5-slot ready_decals[] clone cache: load the effect once, then
		// Scene::Entity_Duplicate() that root for every remaining slot.
		//
		// This is the instrument for "rapid fire only ever shows one particle effect at a
		// time". Entity_Duplicate copies an entity by serializing it, so any emitter field
		// that EmittedParticleSystem::Serialize does not round-trip comes back as its default
		// in the clone. burst_amount defaults to 0 and Burst(0) resolves num = burst_amount,
		// so a clone that lost it emits NOTHING while still occupying a cache slot.
		//
		// Prints the master's values against each clone's and reports PASS/FAIL on an exact
		// match, so the answer does not depend on anyone eyeballing a screenshot.
		uint32_t WickedCall_LoadWPE(char* filename);
		char pefile[MAX_PATH];
		strncpy(pefile, arg, sizeof(pefile) - 1); pefile[sizeof(pefile) - 1] = 0;

		wiScene::Scene& sc = wiScene::GetScene();

		// Same root-from-last-emitter derivation the real preload uses, so this test cannot
		// pass by taking a different path from the code it is meant to be checking.
		auto rootOfNewestEmitter = [&sc]() -> uint32_t {
			if (sc.emitters.GetCount() == 0) return 0;
			wi::ecs::Entity em = sc.emitters.GetEntity(sc.emitters.GetCount() - 1);
			wiScene::HierarchyComponent* h = sc.hierarchy.GetComponent(em);
			return h ? (uint32_t)h->parentID : 0u;
		};
		// First emitter under a given root - the one whose fields we compare.
		auto firstEmitterUnder = [&sc](uint32_t root) -> wiEmittedParticle* {
			for (int i = 0; i < (int)sc.emitters.GetCount(); i++)
			{
				wi::ecs::Entity e = sc.emitters.GetEntity(i);
				wiScene::HierarchyComponent* h = sc.hierarchy.GetComponent(e);
				if (h && (uint32_t)h->parentID == root) return &sc.emitters[i];
			}
			return nullptr;
		};

		const uint32_t masterRoot = WickedCall_LoadWPE(pefile);
		wiEmittedParticle* master = masterRoot ? firstEmitterUnder(masterRoot) : nullptr;
		if (!master)
		{
			_snprintf(result, sizeof(result), "FAIL: could not load '%s' (root=%u)", pefile, masterRoot);
		}
		else
		{
			// Snapshot by value - sc.emitters can reallocate as duplicates are created, which
			// would dangle the pointer.
			const wiEmittedParticle m = *master;
			int written = 0;
			written += _snprintf(result + written, sizeof(result) - written,
				"MASTER root=%u burst_amount=%.3f burst_split=%.3f burst_delay=%.3f spawn_random=%.3f "
				"fadein=%.3f norm_rand=%.3f scal_rand=%.3f endcol=(%.2f,%.2f,%.2f) count=%.2f life=%.2f\n",
				masterRoot, m.burst_amount, m.burst_split, m.burst_delay, m.spawn_random,
				m.fadein_time, m.normal_random, m.scaling_random,
				m.endcolor_red, m.endcolor_green, m.endcolor_blue, m.count, m.life);

			int failures = 0;
			// GGMAX 2.02: track everything this test creates so it can be deleted at the
			// end - the first version leaked the master + 4 clones (plus their materials)
			// into the live scene on every invocation.
			uint32_t createdRoots[5] = { masterRoot, 0, 0, 0, 0 };
			int createdCount = 1;
			for (int c = 1; c <= 4 && written < (int)sizeof(result) - 400; c++)
			{
				sc.Entity_Duplicate(masterRoot);
				const uint32_t cloneRoot = rootOfNewestEmitter();
				if (cloneRoot != 0 && cloneRoot != masterRoot && createdCount < 5)
					createdRoots[createdCount++] = cloneRoot;
				wiEmittedParticle* cl = cloneRoot ? firstEmitterUnder(cloneRoot) : nullptr;
				if (!cl || cloneRoot == masterRoot)
				{
					failures++;
					written += _snprintf(result + written, sizeof(result) - written,
						"CLONE%d root=%u NO EMITTER (duplicate did not produce a usable clone)\n", c, cloneRoot);
					continue;
				}
				const bool ok =
					cl->burst_amount == m.burst_amount && cl->burst_split == m.burst_split &&
					cl->burst_delay == m.burst_delay && cl->spawn_random == m.spawn_random &&
					cl->fadein_time == m.fadein_time && cl->normal_random == m.normal_random &&
					cl->scaling_random == m.scaling_random &&
					cl->endcolor_red == m.endcolor_red && cl->endcolor_green == m.endcolor_green &&
					cl->endcolor_blue == m.endcolor_blue;
				if (!ok) failures++;
				written += _snprintf(result + written, sizeof(result) - written,
					"CLONE%d root=%u burst_amount=%.3f burst_split=%.3f burst_delay=%.3f spawn_random=%.3f "
					"fadein=%.3f norm_rand=%.3f scal_rand=%.3f endcol=(%.2f,%.2f,%.2f) -> %s\n",
					c, cloneRoot, cl->burst_amount, cl->burst_split, cl->burst_delay, cl->spawn_random,
					cl->fadein_time, cl->normal_random, cl->scaling_random,
					cl->endcolor_red, cl->endcolor_green, cl->endcolor_blue,
					ok ? "MATCH" : "MISMATCH");
			}
			written += _snprintf(result + written, sizeof(result) - written,
				"VERDICT: %s (%d of 4 clones differ from master)\n",
				failures == 0 ? "PASS - all cache clones would emit" : "FAIL - silent cache slots", failures);

			// GGMAX 2.02: clean up - the comparison is done, nothing needs to survive.
			{
				void DeleteEmitterEffects(uint32_t root);
				const int before = (int)sc.emitters.GetCount();
				for (int k = 0; k < createdCount; k++)
					if (createdRoots[k] != 0) DeleteEmitterEffects(createdRoots[k]);
				written += _snprintf(result + written, sizeof(result) - written,
					"CLEANUP: deleted %d roots, emitters %d -> %d\n",
					createdCount, before, (int)sc.emitters.GetCount());
			}
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_SCENEUPDATE") == 0)
	{
		// DUMP_SCENEUPDATE — answer "why do the Scene-S* profiler rows say (3x)?" by NAMING the
		// callers (engine 1.81). The engine records each Scene::Update's return address, the
		// Scene instance, the frame and dt; this symbolizes the addresses with dbghelp (already
		// initialised for the crash logger) and groups them.
		//
		// The Scene pointer matters as much as the caller: three updates of ONE scene is
		// duplicated work, three different scenes updated once each is not.
		extern unsigned int GG_GetSceneUpdateCallsBridge(const void**, const void**, unsigned long long*, float*, unsigned int);
		const unsigned int MAXREC = 96;
		const void* rets[MAXREC]; const void* scenes[MAXREC];
		unsigned long long frames[MAXREC]; float dts[MAXREC];
		const unsigned int n = GG_GetSceneUpdateCallsBridge(rets, scenes, frames, dts, MAXREC);

		FILE* f = nullptr;
		f = GGDiagFopen("sceneupdate_dump.txt", "w"); // GGMAX 3.35j
		HANDLE proc = GetCurrentProcess();
		SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
		SymInitialize(proc, NULL, TRUE); // harmless if already initialised

		if (f) fprintf(f, "Scene::Update calls, oldest first (%u records)\n\n", n);
		// Per-frame counts, and per-(caller,scene) totals across the captured window.
		unsigned long long lastFrame = 0; unsigned int perFrame = 0; unsigned int framesSeen = 0;
		unsigned int minPer = 0xFFFFFFFF, maxPer = 0;
		for (unsigned int i = 0; i < n; ++i)
		{
			char symbuf[sizeof(SYMBOL_INFO) + 512] = {};
			SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
			sym->SizeOfStruct = sizeof(SYMBOL_INFO); sym->MaxNameLen = 500;
			DWORD64 disp = 0;
			const bool haveSym = SymFromAddr(proc, (DWORD64)rets[i], &disp, sym) != FALSE;
			IMAGEHLP_LINE64 line = {}; line.SizeOfStruct = sizeof(line); DWORD lineDisp = 0;
			const bool haveLine = SymGetLineFromAddr64(proc, (DWORD64)rets[i], &lineDisp, &line) != FALSE;
			if (f)
			{
				fprintf(f, "frame %llu  scene=%p  dt=%.4f  ret=%p  %s%s",
					frames[i], scenes[i], dts[i], rets[i],
					haveSym ? sym->Name : "<no symbol>", haveSym ? "" : "");
				if (haveLine) fprintf(f, "  (%s:%lu)", line.FileName, line.LineNumber);
				fprintf(f, "\n");
			}
			if (i == 0 || frames[i] != lastFrame)
			{
				if (i != 0) { if (perFrame < minPer) minPer = perFrame; if (perFrame > maxPer) maxPer = perFrame; }
				lastFrame = frames[i]; perFrame = 0; framesSeen++;
			}
			perFrame++;
		}
		if (perFrame) { if (perFrame < minPer) minPer = perFrame; if (perFrame > maxPer) maxPer = perFrame; }
		if (f) { fprintf(f, "\nframes covered=%u  calls/frame min=%u max=%u\n", framesSeen, minPer == 0xFFFFFFFF ? 0 : minPer, maxPer); fclose(f); }
		_snprintf(result, sizeof(result), "OK: DUMP_SCENEUPDATE %u records over %u frames, %u-%u calls/frame -> sceneupdate_dump.txt",
			n, framesSeen, minPer == 0xFFFFFFFF ? 0 : minPer, maxPer);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRASSTYPEFREEZE") == 0)
	{
		// SET_GRASSTYPEFREEZE <0|1> — flicker probe (engine 1.87). Applies INSTANTLY, no reload:
		// it is a constant-buffer flag read by the simulate CS each frame, not a build-time
		// property. That matters — the earlier SET_GRASS attempt at this question changed
		// buffer-STRIDE properties on live systems and corrupted the geometry, which invalidated
		// the whole run.
		//
		// 1 = merged strands keep the entity's own type instead of adopting the paint cell's, so
		// every per-type parameter goes uniform. Density is deliberately WRONG while on; the only
		// valid reading is the CONSECUTIVE-FRAME diff within this config. If the scene-wide churn
		// (merged 12.4 vs per-type 0.4 meanAbsDiff) collapses, the flicker IS type resolution.
		// GGMAX 1.88: 0 = off, 1 = freeze the WHOLE type-dependent path, 2 = `present` only,
		// 3 = textureIndex only, 4 = length only. The selective modes separate the three
		// suspects the whole-path freeze left standing.
		// GGMAX 1.91: BITMASK so probes combine. 1=freeze whole type path, 2=present,
		// 4=textureIndex, 8=length, 16=grasstype visualisation. The control that makes the
		// visualisation interpretable is 17 (= 1|16): type frozen AND visualised, which is the
		// pure animation floor for the flat-colour render. 16 alone cannot be read without it.
		int fz = atoi(arg);
		wi::gg_grass_freeze_type = false;              // superseded by the mask
		wi::gg_grass_freeze_mode = fz;
		char fzdesc[128]; fzdesc[0] = 0;
		if (fz & 1)  strcat(fzdesc, "ALLTYPE ");
		if (fz & 2)  strcat(fzdesc, "present ");
		if (fz & 4)  strcat(fzdesc, "texture ");
		if (fz & 8)  strcat(fzdesc, "length ");
		if (fz & 16) strcat(fzdesc, "TYPEVIS ");
		if (fz & 32) strcat(fzdesc, "STABLETYPE ");
		if (fz & 64) strcat(fzdesc, "ALWAYSWRITE ");
		if (fz & 128) strcat(fzdesc, "UNIFORMCB ");
		// GGMAX 1.95b: bits 8-15 = (forcedType+1) — pin every texture lookup to one type.
		// SET_GRASSTYPEFREEZE (k+1)*256 forces type k.
		if ((fz >> 8) & 0xFF)
		{
			char fbuf[32];
			_snprintf(fbuf, sizeof(fbuf), "FORCE_T%d ", ((fz >> 8) & 0xFF) - 1);
			fbuf[sizeof(fbuf) - 1] = 0;
			strcat(fzdesc, fbuf);
		}
		if (fz == 0) strcpy(fzdesc, "off");
		_snprintf(result, sizeof(result), "OK: SET_GRASSTYPEFREEZE %d [%s] — instant, no reload; density intentionally wrong while non-zero",
			fz, fzdesc);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_GRASSTYPES") == 0)
	{
		// GGMAX 1.95b flicker bisect: the per-type table as CAPTURED into the first merged hair
		// systems at build time (textureIndex is a raw bindless descriptor int) vs the LIVE
		// re-resolve of each built grass material (GGGrass_DumpTypeDescriptors). A captured/live
		// index mismatch = staleness; a weird LIVE identity (dims/mips/name) = wrong resource.
		extern void GGGrass_DumpTypeDescriptors(char* outp, int osize);
		int written = 0;
		auto& sc = wiScene::GetScene();
		int printed = 0;
		for (size_t i = 0; i < sc.hairs.GetCount() && printed < 2; i++)
		{
			auto& h = sc.hairs[i];
			if (h.grass_type != 0xFFFFFFFFu) continue;
			printed++;
			written += _snprintf(result + written, sizeof(result) - written,
				"SYS %d: types=%d strands=%u\n", (int)i, (int)h.grass_types.size(), h.strandCount);
			for (size_t t = 0; t < h.grass_types.size(); t++)
			{
				const auto& gt = h.grass_types[t];
				if (!gt.present && gt.textureIndex == 0) continue;
				written += _snprintf(result + written, sizeof(result) - written,
					"  t%d: texIdx=%u present=%d len=%.1f wid=%.2f bb=%u vd=%.0f\n",
					(int)t, gt.textureIndex, gt.present ? 1 : 0, gt.length, gt.width,
					gt.billboardCount, gt.viewDistance);
				if (written > (int)sizeof(result) - 400) break;
			}
		}
		if (written <= (int)sizeof(result) - 400)
			GGGrass_DumpTypeDescriptors(result + written, (int)sizeof(result) - written);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_HAIRKILL") == 0)
	{
		// DUMP_HAIRKILL — name whatever destroys hair entities (engine 1.85).
		//
		// Merged grass creates 9 hair entities (GRASS_MERGE says created=9, no early exit) and
		// HAIR_SYSTEMS reads 0, with the grass code's own teardown counters at fullResets=0. Two
		// source-read theories about the killer were already wrong, so this records it: every
		// Entity_Remove that takes a live hair component captures _ReturnAddress(), symbolized
		// here with dbghelp (already initialised for the crash logger).
		//
		// reason 0 = removed directly. reason 1 = pulled down as a RECURSIVE CHILD, and `parent`
		// then names the entity that dragged it — which distinguishes "someone removed the grass"
		// from "someone removed the chunk and the grass went with it".
		extern unsigned int GG_GetHairKillsBridge(const void**, unsigned int*, unsigned int*,
			unsigned int*, unsigned int*, unsigned int);
		const void* rets[128] = {};
		unsigned int ents[128] = {}, pars[128] = {}, reas[128] = {}, clearCount = 0;
		const unsigned int n = GG_GetHairKillsBridge(rets, ents, pars, reas, &clearCount, 128);

		HANDLE hProc = GetCurrentProcess();
		static bool symReady = false;
		if (!symReady) { SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME); SymInitialize(hProc, NULL, TRUE); symReady = true; }

		FILE* f = nullptr;
		// ⚠ GGMAX 3.35j: the "Files\\" prefix was the bug - the game CWD is ALREADY
		// Max\Files, so this wrote Max\Files\Files\hairkill_dump.txt. That is exactly where the
		// nested Files/Files tree came from. Beside the exe now, like everything else.
		f = GGDiagFopen("hairkill_dump.txt", "w");
		if (f) fprintf(f, "HAIRKILL: %u removals of live hair entities; Scene::Clear wiped %u hair components\n"
			"reason 0 = direct Entity_Remove, 1 = recursive child of `parent`\n\n", n, clearCount);
		unsigned int direct = 0, recursed = 0;
		for (unsigned int i = 0; i < n; ++i)
		{
			if (reas[i]) recursed++; else direct++;
			char buf[sizeof(SYMBOL_INFO) + 256] = {};
			SYMBOL_INFO* si = (SYMBOL_INFO*)buf;
			si->SizeOfStruct = sizeof(SYMBOL_INFO); si->MaxNameLen = 255;
			DWORD64 disp = 0;
			const char* name = SymFromAddr(hProc, (DWORD64)rets[i], &disp, si) ? si->Name : "<unresolved>";
			IMAGEHLP_LINE64 line = {}; line.SizeOfStruct = sizeof(line); DWORD ld = 0;
			const bool okLine = SymGetLineFromAddr64(hProc, (DWORD64)rets[i], &ld, &line) != FALSE;
			if (f) fprintf(f, "[%3u] entity=%u parent=%u reason=%u  %s+0x%llx  %s:%u\n",
				i, ents[i], pars[i], reas[i], name, (unsigned long long)disp,
				okLine ? line.FileName : "?", okLine ? line.LineNumber : 0);
		}
		if (f) fclose(f);
		_snprintf(result, sizeof(result), "OK: DUMP_HAIRKILL %u removals (%u direct, %u recursive-child), Scene::Clear wiped %u -> hairkill_dump.txt",
			n, direct, recursed, clearCount);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "HITCH_RESET") == 0)
	{
		// HITCH_RESET — start a fresh hitch-measurement window (engine 1.82).
		//
		// Built to measure the cost of lazy object PSOs, whose stalls are 1-30 ms: too small for
		// the 100 ms wall-gap tracer to log, and completely hidden by an FPS average. Send this
		// the moment a level has finished loading, wait, then read the HITCH: line of
		// GET_PERF_DATA — the window then means "since the level came up" rather than "since
		// launch", which is the only framing in which the numbers answer the question.
		wi::profiler::gg_hitch_reset();
		_snprintf(result, sizeof(result), "OK: HITCH_RESET (frame histogram + PSO compile deltas now measured from here)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "VRAM_STAGE") == 0)
	{
		// VRAM_STAGE <label> — record the driver-reported video memory usage at a named point
		// (engine 1.77). The resource census cannot see descriptor heaps, pipeline states or
		// command allocators, and those add up to ~1.4 GB on every level; marking the driver
		// number at hub / level-loaded / after-play turns that lump into an attribution.
		// Marks are printed in the STAGE lines of the next DUMP_VRAM.
		extern void GG_VRAMStage(const char* label);
		const char* label = (arg && arg[0]) ? arg : "mark";
		GG_VRAMStage(label);
		_snprintf(result, sizeof(result), "OK: VRAM_STAGE '%s' recorded (read it in the next DUMP_VRAM)", label);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_STREAM2") == 0)
	{
		// DUMP_STREAM2 — engine-side authoritative enrolled-set dump (wi::resourcemanager
		// resources map: per-resource STREAMING flag, current vs full mip chain, live request).
		wi::resourcemanager::GG_DumpStreamingResources("stream_resources.txt");
		_snprintf(result, sizeof(result), "OK: DUMP_STREAM2 written to stream_resources.txt (game CWD = Files dir)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TEXSTREAM") == 0)
	{
		// SET_TEXSTREAM <0|1> — texture-streaming ENROLLMENT kill-switch (game-side
		// g_bTextureStreamingEnabled, default 1). Unlike SET_STREAMING (pause), this gates
		// whether NEWLY loaded material textures get the STREAMING flag — already-loaded
		// textures keep their state (resource-manager pins flags per name). For a clean A/B:
		// SET_TEXSTREAM 0 then reload/reopen the level.
		int tsv = atoi(arg);
		g_bTextureStreamingEnabled = (tsv != 0);
		_snprintf(result, sizeof(result), "OK: SET_TEXSTREAM %d (affects textures loaded from now on; reload level for full effect)", tsv);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TRANSPARENTSHADOWS") == 0)
	{
		// SET_TRANSPARENTSHADOWS <0|1> — the transparent shadow feature.
		// **DEFAULT IS NOW 0 (engine 1.78, dropped by product decision 2026-08-02.)** It answered
		// its own question: the atlas cost 160 MB on every level and 512 MB on Amazon / Foggy
		// Forest / Disruption / Bounty, and bought nothing visible. The atlas is no longer
		// allocated and the shadow pass is depth-only.
		// Setting 1 at runtime only re-enables the DRAWS and cannot bring the feature back: the
		// atlas is created (if at all) at shadow-packer resize, and the object shadow PSOs latch
		// their render-target count at LoadShaders. A real re-enable needs the engine default
		// flipped and a restart.
		int tsv = atoi(arg);
		wi::renderer::gg_transparent_shadows = (tsv != 0);
		_snprintf(result, sizeof(result), "OK: SET_TRANSPARENTSHADOWS %d (draws only; atlas is NOT allocated in 1.78 - a real re-enable needs the engine default + restart)", tsv);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRASSMERGE") == 0)
	{
		// SET_GRASSMERGE <0|1> — GGMAX 1.74. 1 builds ONE hair system per terrain chunk covering
		// every painted grass type (per-strand type resolved in the simulate CS) instead of one
		// system per (chunk x type). DEFAULT 0 until the TESTPRO1 density gate signs it off.
		// Grass entities are built at chunk-spawn time, so RELOAD THE LEVEL after flipping this.
		extern bool gg_grass_merge;
		int gmv = atoi(arg);
		gg_grass_merge = (gmv != 0);
		_snprintf(result, sizeof(result), "OK: SET_GRASSMERGE %d (reload the level — grass entities are built at chunk spawn)", gmv);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TEXSTREAMTRACE") == 0)
	{
		// SET_TEXSTREAMTRACE <0|1> — GGMAX 1.73 diagnostic. Writes one flushed line per
		// streaming DDS upload to stream_load.txt (name, file dims/mips, reduced upload dims,
		// mip_offset, filesize vs bytes the mip chain needs). Expensive; arm it only to hunt a
		// load-time fault, where the LAST line written names the texture that killed the process.
		int tstv = atoi(arg);
		wi::resourcemanager::gg_stream_load_trace = (tstv != 0);
		gg_upload_trace = (tstv != 0);
		_snprintf(result, sizeof(result), "OK: SET_TEXSTREAMTRACE %d (per-load trace -> stream_load.txt, upload footprints -> last_upload.txt)", tstv);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "REUPLOAD_TEXTURE") == 0)
	{
		// REUPLOAD_TEXTURE <name-substr> — mark every matching object's material textures
		// OUTDATED and re-Load them from disk (resource-manager recreate path). Decisive
		// counterpart of REUPLOAD_ENTITY: if blue entities heal, the TEXTURE CONTENT in GPU
		// memory was corrupted during the reload upload storm (copy-queue race) while every
		// descriptor/name stayed correct.
		wi::scene::Scene& rtScene = wi::scene::GetScene();
		auto rtContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		int rtCount = 0;
		static wi::vector<wi::ecs::Entity> rtDone;
		rtDone.clear();
		for (size_t rti = 0; rti < rtScene.objects.GetCount(); ++rti)
		{
			const wi::scene::NameComponent* rtn = rtScene.names.GetComponent(rtScene.objects.GetEntity(rti));
			if (rtn == nullptr || !rtContains(rtn->name, arg)) continue;
			const wi::scene::MeshComponent* rtm = rtScene.meshes.GetComponent(rtScene.objects[rti].meshID);
			if (rtm == nullptr) continue;
			for (const auto& rts : rtm->subsets)
			{
				wi::scene::MaterialComponent* rmat = rtScene.materials.GetComponent(rts.materialID);
				if (rmat == nullptr) continue;
				bool rtSeen = false;
				for (wi::ecs::Entity d : rtDone) if (d == rts.materialID) { rtSeen = true; break; }
				if (rtSeen) continue;
				rtDone.push_back(rts.materialID);
				for (int rq = 0; rq < wi::scene::MaterialComponent::TEXTURESLOT_COUNT; rq++)
				{
					auto& rtex = rmat->textures[rq];
					if (!rtex.resource.IsValid() || rtex.name.empty()) continue;
					rtex.resource.SetOutdated();
					rtex.resource = wi::resourcemanager::Load(rtex.name);
					rtCount++;
				}
				rmat->SetDirty();
			}
		}
		_snprintf(result, sizeof(result), "OK: REUPLOAD_TEXTURE \"%s\" re-loaded %d texture slots", arg, rtCount);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "REUPLOAD_ENTITY") == 0)
	{
		// REUPLOAD_ENTITY <name-substr> — re-run CreateRenderData() on every matching object's
		// mesh: recreates the GPU buffers and re-uploads vertex/index content from the intact
		// CPU copies. Decisive probe: if blue/corrupt entities snap back to normal, the GPU
		// BUFFER CONTENT was corrupted during the in-place-reload upload storm while all data
		// structures were correct (upload/copy-queue race).
		wi::scene::Scene& ruScene = wi::scene::GetScene();
		auto ruContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		int ruCount = 0;
		static wi::vector<wi::ecs::Entity> ruDone;
		ruDone.clear();
		for (size_t rui = 0; rui < ruScene.objects.GetCount(); ++rui)
		{
			const wi::scene::NameComponent* run = ruScene.names.GetComponent(ruScene.objects.GetEntity(rui));
			if (run == nullptr || !ruContains(run->name, arg)) continue;
			wi::ecs::Entity ruMeshID = ruScene.objects[rui].meshID;
			if (ruMeshID == wi::ecs::INVALID_ENTITY) continue;
			bool ruSeen = false;
			for (wi::ecs::Entity d : ruDone) if (d == ruMeshID) { ruSeen = true; break; }
			if (ruSeen) continue;
			wi::scene::MeshComponent* rum = ruScene.meshes.GetComponent(ruMeshID);
			if (rum == nullptr) continue;
			rum->CreateRenderData();
			ruDone.push_back(ruMeshID);
			ruCount++;
		}
		_snprintf(result, sizeof(result), "OK: REUPLOAD_ENTITY \"%s\" re-uploaded %d meshes", arg, ruCount);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "FIND_OBJECT") == 0)
	{
		// FIND_OBJECT <x> <y> <z> [radius=500] — scan every scene object's world transform
		// for instances near a position (e.g. the editor's Object Tools position readout of a
		// clicked corrupt object) and print entity/name/mesh links. Bridges the GG instance
		// list to the Wicked scene without needing to know internal frame names.
		float fx = 0, fy = 0, fz = 0, fr = 500.0f;
		int nParsed = sscanf_s(arg, "%f %f %f %f", &fx, &fy, &fz, &fr);
		if (nParsed >= 3)
		{
			wi::scene::Scene& foScene = wi::scene::GetScene();
			struct FoHit { float dist; wi::ecs::Entity entity; wi::ecs::Entity meshID; const char* name; };
			std::vector<FoHit> foHits;
			for (size_t foi = 0; foi < foScene.objects.GetCount(); ++foi)
			{
				wi::ecs::Entity foEnt = foScene.objects.GetEntity(foi);
				const wi::scene::TransformComponent* foTr = foScene.transforms.GetComponent(foEnt);
				if (foTr == nullptr) continue;
				float dx = foTr->world._41 - fx, dy = foTr->world._42 - fy, dz = foTr->world._43 - fz;
				float d = sqrtf(dx * dx + dy * dy + dz * dz);
				if (d > fr) continue;
				const wi::scene::NameComponent* foNm = foScene.names.GetComponent(foEnt);
				foHits.push_back({ d, foEnt, foScene.objects[foi].meshID, foNm ? foNm->name.c_str() : "?" });
			}
			std::sort(foHits.begin(), foHits.end(), [](const FoHit& a, const FoHit& b) { return a.dist < b.dist; });
			FILE* foF = GGDiagFopen("find_object.txt", "w");
			int foWritten = 0;
			for (const FoHit& h : foHits)
			{
				if (foF && foWritten < 200)
				{
					const wi::scene::NameComponent* foMn = foScene.names.GetComponent(h.meshID);
					const wi::scene::MeshComponent* foMesh = foScene.meshes.GetComponent(h.meshID);
					fprintf(foF, "dist=%.0f entity=%u name=\"%s\" mesh=%u meshname=\"%s\" bufsize=%llu cpuverts=%llu\n",
						h.dist, (unsigned)h.entity, h.name, (unsigned)h.meshID,
						foMn ? foMn->name.c_str() : "?",
						foMesh ? (unsigned long long)foMesh->generalBuffer.desc.size : 0ull,
						foMesh ? (unsigned long long)foMesh->vertex_positions.size() : 0ull);
					foWritten++;
				}
			}
			if (foF) fclose(foF);
			int rlen = _snprintf(result, sizeof(result), "OK: FIND_OBJECT (%.0f,%.0f,%.0f) r=%.0f hits=%d -> find_object.txt\n",
				fx, fy, fz, fr, (int)foHits.size());
			for (size_t hi = 0; hi < foHits.size() && hi < 8 && rlen > 0 && rlen < (int)sizeof(result) - 200; hi++)
			{
				rlen += _snprintf(result + rlen, sizeof(result) - rlen - 1, "  dist=%.0f entity=%u name=\"%s\" mesh=%u\n",
					foHits[hi].dist, (unsigned)foHits[hi].entity, foHits[hi].name, (unsigned)foHits[hi].meshID);
			}
			result[sizeof(result) - 1] = 0;
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: FIND_OBJECT needs <x> <y> <z> [radius]");
		}
	}
	else if (_stricmp(cmd, "VERIFY_MESH") == 0 || _stricmp(cmd, "VERIFY_WATCH") == 0)
	{
		// See the oracle block above AutoHarness_VerifyTick for the full design.
		// VERIFY_MESH <substr>: snapshot -> re-upload -> snapshot -> compare (detect + heal).
		// VERIFY_WATCH <substr>: snapshot -> wait 60 frames -> snapshot (catch active writer).
		// Empty substring matches EVERY mesh in the scene.
		if (s_verifyPhase != 0)
		{
			_snprintf(result, sizeof(result), "ERROR: a verify is already in progress (phase %d)", s_verifyPhase);
		}
		else
		{
			bool vWatch = (_stricmp(cmd, "VERIFY_WATCH") == 0);
			// Optional trailing "burst" token: rebuild ALL matched meshes in one frame — the
			// deliberate suballocator churn storm that reproduces the overlap/OOB race.
			s_verifyBurst = false;
			{
				size_t alen = strlen(arg);
				if (alen >= 5 && _stricmp(arg + alen - 5, "burst") == 0 && (alen == 5 || arg[alen - 6] == ' '))
				{
					s_verifyBurst = true;
					arg[alen - 5] = 0;
					while (alen > 5 && arg[strlen(arg) - 1] == ' ') arg[strlen(arg) - 1] = 0;
				}
			}
			wi::scene::Scene& vScene = wi::scene::GetScene();
			auto vContains = [](const std::string& hay, const char* needle) -> bool {
				std::string h = hay, n = needle;
				for (auto& c : h) c = (char)tolower((unsigned char)c);
				for (auto& c : n) c = (char)tolower((unsigned char)c);
				return h.find(n) != std::string::npos;
			};
			s_verifyTargets.clear();
			int vNoCpu = 0;
			for (size_t vi = 0; vi < vScene.objects.GetCount(); ++vi)
			{
				wi::ecs::Entity vEnt = vScene.objects.GetEntity(vi);
				const wi::scene::NameComponent* vNm = vScene.names.GetComponent(vEnt);
				if (arg[0] != 0 && (vNm == nullptr || !vContains(vNm->name, arg))) continue;
				wi::ecs::Entity vMeshID = vScene.objects[vi].meshID;
				if (vMeshID == wi::ecs::INVALID_ENTITY) continue;
				bool vSeen = false;
				for (const VerifyTarget& t : s_verifyTargets) { if (t.meshID == vMeshID) { vSeen = true; break; } }
				if (vSeen) continue;
				wi::scene::MeshComponent* vMesh = vScene.meshes.GetComponent(vMeshID);
				if (vMesh == nullptr || !vMesh->generalBuffer.IsValid()) continue;
				VerifyTarget t;
				t.meshID = vMeshID;
				t.label = vNm ? vNm->name : "?";
				const wi::scene::TransformComponent* vTr = vScene.transforms.GetComponent(vEnt);
				if (vTr) t.pos = XMFLOAT3(vTr->world._41, vTr->world._42, vTr->world._43);
				if (!vWatch && (vMesh->vertex_positions.empty() || vMesh->indices.empty()))
				{
					t.skipped = true; t.note = "no CPU arrays (freed after upload) - unverifiable";
					vNoCpu++;
				}
				s_verifyTargets.push_back(std::move(t));
			}
			if (s_verifyTargets.empty())
			{
				_snprintf(result, sizeof(result), "OK: %s \"%s\" matched 0 meshes", cmd, arg);
			}
			else
			{
				s_verifyFile = GGDiagFopen("verify_mesh.txt", "w");
				if (s_verifyFile)
				{
					fprintf(s_verifyFile, "%s \"%s\" targets=%d (corrupt/skip detail only; clean meshes counted in summary)\n",
						cmd, arg, (int)s_verifyTargets.size());
					fflush(s_verifyFile);
				}
				s_verifyMode = vWatch ? 1 : 0;
				s_verifyWaveStart = 0;
				s_verifyWaveEnd = 0;
				s_verifyCorrupt = 0;
				s_verifySkipped = 0;
				s_verifyChecked = 0;
				s_verifyPhase = 1;
				_snprintf(result, sizeof(result),
					"OK: %s started on %d meshes (%d unverifiable, no CPU arrays)%s — final summary rewrites auto_result.txt in a few seconds",
					cmd, (int)s_verifyTargets.size(), vNoCpu,
					vWatch ? " [watch: 60-frame window, no re-upload]" : " [verify: re-uploads (heals) as it checks]");
			}
			result[sizeof(result) - 1] = 0;
		}
	}
	else if (_stricmp(cmd, "DUMP_GEOMETRY") == 0)
	{
		// DUMP_GEOMETRY <name-substr> — read back the GPU-consumed ShaderGeometry record at
		// each matching object's instance geometryOffset and compare: materialIndex vs the
		// EXPECTED index of the subset's material (wrong index = palm renders with another
		// material = blue), vertex/index buffer descriptors, uv ranges. The last GPU data
		// layer not yet verified in the reload-corruption hunt.
		wi::scene::Scene& dgScene = wi::scene::GetScene();
		auto dgContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		FILE* dgF = GGDiagFopen("geometry_dump.txt", "w");
		int dgMatches = 0;
		if (dgF != nullptr)
		{
			if (dgScene.geometryArrayMapped == nullptr || dgScene.instanceArrayMapped == nullptr)
			{
				fprintf(dgF, "mapped arrays NULL (geometry=%p instance=%p)\n",
					(void*)dgScene.geometryArrayMapped, (void*)dgScene.instanceArrayMapped);
			}
			else
			{
				for (size_t dgi = 0; dgi < dgScene.objects.GetCount(); ++dgi)
				{
					wi::ecs::Entity dge = dgScene.objects.GetEntity(dgi);
					const wi::scene::NameComponent* dgn = dgScene.names.GetComponent(dge);
					if (dgn == nullptr || !dgContains(dgn->name, arg)) continue;
					const wi::scene::ObjectComponent& dgo = dgScene.objects[dgi];
					if (!dgo.IsRenderable()) continue;
					const wi::scene::MeshComponent* dgm = dgScene.meshes.GetComponent(dgo.meshID);
					if (dgm == nullptr || dgm->subsets.empty()) continue;
					dgMatches++;
					if (dgMatches > 16) continue;
					const ShaderMeshInstance& dgsi = dgScene.instanceArrayMapped[dgi];
					const ShaderGeometry& dgg = dgScene.geometryArrayMapped[dgsi.geometryOffset];
					const uint32_t dgExpectMat = (uint32_t)dgScene.materials.GetIndex(dgm->subsets[0].materialID);
					fprintf(dgF, "OBJ entity=%llu name=\"%s\" geoOfs=%u\n", (unsigned long long)dge, dgn->name.c_str(), dgsi.geometryOffset);
					fprintf(dgF, "  geo materialIndex=%u EXPECTED=%u %s\n", dgg.materialIndex, dgExpectMat,
						dgg.materialIndex == dgExpectMat ? "MATCH" : "*** MISMATCH ***");
					if (dgg.materialIndex != dgExpectMat && dgg.materialIndex < dgScene.materials.GetCount())
					{
						wi::ecs::Entity dgWrongE = dgScene.materials.GetEntity(dgg.materialIndex);
						const wi::scene::MaterialComponent* dgWrongM = &dgScene.materials[dgg.materialIndex];
						const wi::scene::NameComponent* dgWrongN = dgScene.names.GetComponent(dgWrongE);
						fprintf(dgF, "  ACTUAL material at index: entity=%llu name=\"%s\" basetex=\"%s\"\n",
							(unsigned long long)dgWrongE, dgWrongN ? dgWrongN->name.c_str() : "?",
							dgWrongM->textures[wi::scene::MaterialComponent::BASECOLORMAP].name.c_str());
					}
					fprintf(dgF, "  geo ib=%d vb_pos=%d vb_uvs=%d vb_nor=%d vb_tan=%d vb_pre=%d idxOfs=%u idxCount=%u flags=%08x\n",
						dgg.ib, dgg.vb_pos_wind, dgg.vb_uvs, dgg.vb_nor, dgg.vb_tan, dgg.vb_pre,
						dgg.indexOffset, dgg.indexCount, dgg.flags);
					fprintf(dgF, "  geo aabb=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) uvmin=(%.2f,%.2f) uvmax=(%.2f,%.2f)\n",
						dgg.aabb_min.x, dgg.aabb_min.y, dgg.aabb_min.z, dgg.aabb_max.x, dgg.aabb_max.y, dgg.aabb_max.z,
						dgg.uv_range_min.x, dgg.uv_range_min.y, dgg.uv_range_max.x, dgg.uv_range_max.y);
				}
			}
			fprintf(dgF, "MATCHES %d\n", dgMatches);
			fclose(dgF);
			_snprintf(result, sizeof(result), "OK: DUMP_GEOMETRY \"%s\" renderable-matches=%d -> geometry_dump.txt", arg, dgMatches);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_GEOMETRY could not open geometry_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_INSTANCE") == 0)
	{
		// DUMP_INSTANCE <name-substr> — read back the CPU-written GPU instance record
		// (ShaderMeshInstance in the mapped upload array) for matching objects and compare
		// against the components: per-instance color/emissive (blue-tint suspect),
		// geometryOffset vs the mesh's geometryOffset (wrong-geometry suspect = slabs +
		// wrong textures), transformRaw vs transform.world. The array holds LAST frame's
		// values when this runs pre-Update — exactly what the GPU consumed.
		wi::scene::Scene& diScene = wi::scene::GetScene();
		auto diContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		FILE* diF = GGDiagFopen("instance_dump.txt", "w");
		int diMatches = 0;
		if (diF != nullptr)
		{
			if (diScene.instanceArrayMapped == nullptr)
			{
				fprintf(diF, "instanceArrayMapped is NULL\n");
			}
			else
			{
				for (size_t dii = 0; dii < diScene.objects.GetCount(); ++dii)
				{
					wi::ecs::Entity die = diScene.objects.GetEntity(dii);
					const wi::scene::NameComponent* din = diScene.names.GetComponent(die);
					if (din == nullptr || !diContains(din->name, arg)) continue;
					const wi::scene::ObjectComponent& dio = diScene.objects[dii];
					if (!dio.IsRenderable()) continue;
					diMatches++;
					if (diMatches > 24) continue;
					const wi::scene::TransformComponent* dit = diScene.transforms.GetComponent(die);
					const wi::scene::MeshComponent* dim = diScene.meshes.GetComponent(dio.meshID);
					const ShaderMeshInstance& dsi = diScene.instanceArrayMapped[dii];
					fprintf(diF, "OBJ idx=%zu entity=%llu name=\"%s\" pos=(%.0f,%.0f,%.0f)\n",
						dii, (unsigned long long)die, din->name.c_str(),
						dit ? dit->world._41 : 0.0f, dit ? dit->world._42 : 0.0f, dit ? dit->world._43 : 0.0f);
					fprintf(diF, "  inst uid=%llu layer=%08x flags=%08x color=(%08x,%08x) emissive=(%08x,%08x) rim=(%08x,%08x)\n",
						(unsigned long long)dsi.uid, dsi.layerMask, dsi.flags, dsi.color.x, dsi.color.y,
						dsi.emissive.x, dsi.emissive.y, dsi.rimHighlight.x, dsi.rimHighlight.y);
					fprintf(diF, "  inst geoOfs=%u geoCount=%u baseGeoOfs=%u baseGeoCount=%u  MESH geoOfs=%u subsets=%d lodcount=%u\n",
						dsi.geometryOffset, dsi.geometryCount, dsi.baseGeometryOffset, dsi.baseGeometryCount,
						dim ? dim->geometryOffset : ~0u, dim ? (int)dim->subsets.size() : -1,
						dim ? dim->GetLODCount() : 0);
					fprintf(diF, "  inst center=(%.0f,%.0f,%.0f) r=%.1f  rawT=(%.0f,%.0f,%.0f) compT=(%.0f,%.0f,%.0f)\n",
						dsi.center.x, dsi.center.y, dsi.center.z, dsi.radius,
						dsi.transformRaw.mat0.w, dsi.transformRaw.mat1.w, dsi.transformRaw.mat2.w,
						dit ? dit->world._41 : 0.0f, dit ? dit->world._42 : 0.0f, dit ? dit->world._43 : 0.0f);
				}
			}
			fprintf(diF, "MATCHES %d\n", diMatches);
			fclose(diF);
			_snprintf(result, sizeof(result), "OK: DUMP_INSTANCE \"%s\" renderable-matches=%d -> instance_dump.txt", arg, diMatches);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_INSTANCE could not open instance_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_TREEPOOL") == 0)
	{
		// DUMP_TREEPOOL — tree pool census + orphan detector (renderable unnamed objects the
		// tree system no longer owns). Writes treepool_dump.txt (cwd = Files\ after load).
		GGTrees::GGTrees_DebugDumpPool("treepool_dump.txt");
		_snprintf(result, sizeof(result), "OK: DUMP_TREEPOOL wrote treepool_dump.txt");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_BROKEN") == 0)
	{
		// DUMP_BROKEN — scan every scene ObjectComponent for DANGLING REFERENCES: meshID
		// that no longer resolves, mesh subsets whose materialID no longer resolves, or a
		// mesh whose GPU generalBuffer is invalid. Writes broken_dump.txt (cwd = Files\
		// after level load). Built to identify the orphaned "blue palm" objects in the
		// in-place-reload corruption hunt (2026-07-25).
		wi::scene::Scene& brScene = wi::scene::GetScene();
		FILE* brF = GGDiagFopen("broken_dump.txt", "w");
		if (brF != nullptr)
		{
			int brMesh = 0, brMat = 0, brBuf = 0;
			for (size_t bri = 0; bri < brScene.objects.GetCount(); ++bri)
			{
				wi::ecs::Entity bre = brScene.objects.GetEntity(bri);
				const wi::scene::ObjectComponent& bro = brScene.objects[bri];
				const wi::scene::NameComponent* brn = brScene.names.GetComponent(bre);
				const wi::scene::TransformComponent* brt = brScene.transforms.GetComponent(bre);
				float brx = brt ? brt->world._41 : 0, bry = brt ? brt->world._42 : 0, brz = brt ? brt->world._43 : 0;
				if (bro.meshID == wi::ecs::INVALID_ENTITY) continue; // no mesh bound = legit hidden slot
				const wi::scene::MeshComponent* brm = brScene.meshes.GetComponent(bro.meshID);
				if (brm == nullptr)
				{
					brMesh++;
					fprintf(brF, "BROKEN_MESH entity=%llu name=\"%s\" meshID=%llu renderable=%d pos=(%.0f,%.0f,%.0f)\n",
						(unsigned long long)bre, brn ? brn->name.c_str() : "?", (unsigned long long)bro.meshID,
						bro.IsRenderable() ? 1 : 0, brx, bry, brz);
					continue;
				}
				if (!brm->generalBuffer.IsValid())
				{
					brBuf++;
					fprintf(brF, "BROKEN_BUFFER entity=%llu name=\"%s\" meshID=%llu renderable=%d pos=(%.0f,%.0f,%.0f)\n",
						(unsigned long long)bre, brn ? brn->name.c_str() : "?", (unsigned long long)bro.meshID,
						bro.IsRenderable() ? 1 : 0, brx, bry, brz);
				}
				for (const auto& brs : brm->subsets)
				{
					if (brs.materialID != wi::ecs::INVALID_ENTITY && brScene.materials.GetComponent(brs.materialID) == nullptr)
					{
						brMat++;
						fprintf(brF, "BROKEN_MATERIAL entity=%llu name=\"%s\" meshID=%llu matID=%llu renderable=%d pos=(%.0f,%.0f,%.0f)\n",
							(unsigned long long)bre, brn ? brn->name.c_str() : "?", (unsigned long long)bro.meshID,
							(unsigned long long)brs.materialID, bro.IsRenderable() ? 1 : 0, brx, bry, brz);
						break;
					}
				}
			}
			fprintf(brF, "TOTALS objects=%d broken_mesh=%d broken_material=%d broken_buffer=%d\n",
				(int)brScene.objects.GetCount(), brMesh, brMat, brBuf);
			fclose(brF);
			_snprintf(result, sizeof(result), "OK: DUMP_BROKEN objects=%d broken_mesh=%d broken_material=%d broken_buffer=%d",
				(int)brScene.objects.GetCount(), brMesh, brMat, brBuf);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_BROKEN could not open broken_dump.txt");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "MOVE_CAMERA") == 0)
	{
		// MOVE_CAMERA <dx> <dy> <dz> — offset the editor free-flight camera (force chunk re-stream).
		float dxp = 0, dyp = 0, dzp = 0;
		const char* state = AutoHarness_GetAppState();
		if (strcmp(state, "editor") != 0)
		{
			_snprintf(result, sizeof(result), "ERROR: MOVE_CAMERA only works in the level editor (state: %s)", state);
		}
		else if (sscanf_s(arg, "%f %f %f", &dxp, &dyp, &dzp) != 3)
		{
			_snprintf(result, sizeof(result), "ERROR: MOVE_CAMERA needs <dx> <dy> <dz>");
		}
		else
		{
			t.editorfreeflight.mode = 1;
			t.editorfreeflight.c.x_f += dxp;
			t.editorfreeflight.c.y_f += dyp;
			t.editorfreeflight.c.z_f += dzp;
			t.cx_f = t.editorfreeflight.c.x_f;
			t.cy_f = t.editorfreeflight.c.z_f;
			_snprintf(result, sizeof(result), "OK: MOVE_CAMERA -> (%.1f,%.1f,%.1f)",
				t.editorfreeflight.c.x_f, t.editorfreeflight.c.y_f, t.editorfreeflight.c.z_f);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRASS") == 0)
	{
		char gp[64] = { 0 }; float gv = 0.0f;
		if (sscanf_s(arg, "%63s %f", gp, (unsigned)sizeof(gp), &gv) == 2)
		{
			extern void GGTerrainWicked_SetGrassParam(const char* param, float value);
			GGTerrainWicked_SetGrassParam(gp, gv);
			_snprintf(result, sizeof(result), "OK: SET_GRASS %s = %.3f", gp, gv);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_GRASS needs <param> <value> (length|width|stiffness|drag|blades|maxstrands|segments|billboards|viewdist|lodchunks|tier3|tier2|sss|alpha|tintr|tintg|tintb|sssr|sssg|sssb)");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (AutoHarness_ShadowBudgetCommands(cmd, arg, result, sizeof(result)))
	{
		// SET_SHADOW_MAX / SET_SHADOW_MAX_SPOT / SET_SHADOW_MAX_POINT — handled in the
		// helper above the dispatch function (hoisted to dodge MSVC C1061)
	}
	else if (_stricmp(cmd, "SET_SHADOW_CACHE") == 0)
	{
		// A/B toggle for the Phase 2 static-cache (forces the engine flag directly).
		bool on = (arg[0] != '0');
		wi::renderer::SetLocalShadowCachingEnabled(on);
		_snprintf(result, sizeof(result), "OK: SET_SHADOW_CACHE %s", on ? "ON" : "OFF");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_SHADOW_FARCULL") == 0)
	{
		// A/B toggle for the far-cascade caster cull (DX11 parity). On by default.
		bool on = (arg[0] != '0');
		wi::renderer::SetShadowFarCascadeCull(on);
		_snprintf(result, sizeof(result), "OK: SET_SHADOW_FARCULL %s", on ? "ON" : "OFF");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_REFLECTIONS") == 0)
	{
		// A/B lever for the water reflection stabilisation fix. Toggles planar reflections exactly like the
		// editor "Reflections" checkbox (M-GridEditB_part23). With the engine guard (wiRenderPath3D PreRender),
		// OFF forces texture_reflection_index=-1 so the ocean PS takes its EnvironmentReflection_Global
		// fallback (reflect the sky/global probe) instead of sampling the stale reflection texture = garbage.
		extern MasterRenderer * master_renderer;
		bool on = (arg[0] != '0');
		t.visuals.bReflectionsEnabled = on;
		t.gamevisuals.bReflectionsEnabled = on;
		if (master_renderer) master_renderer->setReflectionsEnabled(on);
		_snprintf(result, sizeof(result), "OK: SET_REFLECTIONS %s", on ? "ON" : "OFF");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_RESSCALE") == 0)
	{
		// GPU lever: render the 3D scene at a fraction of native resolution. RenderPath2D::Update
		// auto-detects the resolutionScale change and ResizeBuffers() next frame (safe). 1.0 = native;
		// lower = less GPU pixel work (Z-prepass/opaque/postproc all scale), softer image (FSR can upscale).
		extern MasterRenderer * master_renderer;
		float s = (float)atof(arg);
		if (s < 0.4f) s = 0.4f;
		if (s > 1.0f) s = 1.0f;
		if (master_renderer) master_renderer->resolutionScale = s;
		_snprintf(result, sizeof(result), "OK: SET_RESSCALE %.3f (auto-resizes next frame)", s);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_FSR") == 0)
	{
		// A/B the FSR1 spatial-upscale modes without the ImGui panel (mirrors the Graphics & Performance
		// combo exactly: resolutionScale -> setFSREnabled -> ResizeBuffers). 0=None 1=UltraQuality 2=Quality
		// 3=Balanced 4=Performance. Reports internal vs physical res == direct proof the 3D renders smaller;
		// pair with ENABLE_PROFILER + GET_PERF_DATA to confirm the 'FSR' GPU range appears (Postprocess_FSR).
		extern MasterRenderer * master_renderer;
		int mode = atoi(arg);
		if (mode < 0) mode = 0;
		if (mode > 4) mode = 4;
		const float scaleTab[5] = { 1.0f, 1.0f / 1.3f, 1.0f / 1.5f, 1.0f / 1.7f, 1.0f / 2.0f };
		if (master_renderer)
		{
			master_renderer->resolutionScale = scaleTab[mode];
			master_renderer->setFSREnabled(mode != 0);
			master_renderer->ResizeBuffers();
			_snprintf(result, sizeof(result),
				"OK: SET_FSR mode=%d scale=%.3f fsrEnabled=%d internal=%dx%d physical=%dx%d",
				mode, scaleTab[mode], master_renderer->getFSREnabled() ? 1 : 0,
				(int)master_renderer->GetInternalResolution().x, (int)master_renderer->GetInternalResolution().y,
				(int)master_renderer->GetPhysicalWidth(), (int)master_renderer->GetPhysicalHeight());
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: no master_renderer");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_FSRSHARP") == 0)
	{
		// Diagnostic for the panel "FSR Sharpness" slider. Value is FFX RCAS *stops*: 0.0 = MAX sharpening
		// (linear exp2(0)=1.0), higher = softer (2.0 -> 1/4). RCAS always runs; this only scales strength,
		// and it's inverted vs intuition. Only visible while an FSR mode is active (rtFSR valid + fsrEnabled).
		extern MasterRenderer * master_renderer;
		float sh = (float)atof(arg);
		if (sh < 0.0f) sh = 0.0f;
		if (master_renderer)
		{
			master_renderer->setFSRSharpness(sh);
			_snprintf(result, sizeof(result), "OK: SET_FSRSHARP %.3f (RCAS stops; 0=sharpest, higher=softer) fsrEnabled=%d",
				sh, master_renderer->getFSREnabled() ? 1 : 0);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: no master_renderer");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_ANIM30FPS") == 0)
	{
		// A/B the "Lower Animation & LUA Speed" 30fps ANIMATION throttle (delta 1.29). The checkbox global
		// is mirrored into the engine each frame by MasterRenderer::Update. Args: <0|1 enable> [farDist].
		// farDist = scene-units distance beyond which on-screen armatures throttle to ~30fps; 0 = throttle
		// ALL eligible ((b)); >0 = keep near-camera armatures full-rate ((c) distance gate). Omit farDist to keep it.
		extern bool bEnable30FpsAnimations;
		extern float g_animThrottleFarDist;
		int en = -1; float fd = -1.0f;
		int n = sscanf(arg, "%d %f", &en, &fd);
		if (n >= 1 && (en == 0 || en == 1)) bEnable30FpsAnimations = (en == 1);
		if (n >= 2 && fd >= 0.0f) g_animThrottleFarDist = fd;
		_snprintf(result, sizeof(result), "OK: SET_ANIM30FPS enabled=%d farDist=%.1f (0=throttle all, >0=only beyond)",
			bEnable30FpsAnimations ? 1 : 0, g_animThrottleFarDist);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_APPARENTSIZE") == 0)
	{
		// A/B the apparent-size object cull (delta 1.30). Objects whose world bounding radius / camera
		// distance falls below the threshold are dropped from the main visible set. Two forms:
		//   SET_APPARENTSIZE <tangent>          DIRECT override of the tangent (radius/dist cutoff); <0 = restore slider.
		//   SET_APPARENTSIZE slider <fASize>    exercise the REAL editor-slider path: sets maxApparentSize = fASize/10000
		//                                       (fASize is the 0.02..2.0 UI value) and clears the direct override, so the
		//                                       game maps it through g_apparentCullK exactly as moving the slider would.
		// NOTE: visible/total counts reflect the PRIOR frame (runs before this frame's UpdateVisibility) — screenshot
		// or re-query after a couple of frames to see the effect.
		extern float g_apparentCullDirect;
		extern float g_apparentCullK;
		extern float maxApparentSize;
		char sub[64] = { 0 };
		float v = 0.0f;
		if (sscanf(arg, "%63s %f", sub, &v) == 2 && _stricmp(sub, "slider") == 0)
		{
			// clamp like the editor slider + M-GridEditB_part3 load guard
			float fA = v; if (fA < 0.02f) fA = 0.02f; if (fA > 2.0f) fA = 2.0f;
			maxApparentSize = fA / 10000.0f;
			g_apparentCullDirect = -1.0f;
			float over = maxApparentSize - 0.000008f;
			float tangent = (over > 0.0f) ? over * g_apparentCullK : 0.0f;
			_snprintf(result, sizeof(result),
				"OK: SET_APPARENTSIZE slider fASize=%.3f -> maxApparentSize=%.7f -> tangent=%.5f (K=%.0f)  visible=%d/%d (prior frame)",
				fA, maxApparentSize, tangent, g_apparentCullK,
				(int)master.masterrenderer.visibility_main.visibleObjects.size(),
				master.masterrenderer.scene ? (int)master.masterrenderer.scene->objects.GetCount() : 0);
		}
		else
		{
			float tg = -1.0f;
			int n = sscanf(arg, "%f", &tg);
			if (n >= 1) g_apparentCullDirect = tg;
			_snprintf(result, sizeof(result),
				"OK: SET_APPARENTSIZE tangent=%.5f (%s)  visible=%d/%d (prior frame)",
				g_apparentCullDirect,
				(g_apparentCullDirect >= 0.0f) ? "forced override" : "slider mapping restored",
				(int)master.masterrenderer.visibility_main.visibleObjects.size(),
				master.masterrenderer.scene ? (int)master.masterrenderer.scene->objects.GetCount() : 0);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SAVE_LEVEL") == 0)
	{
		// Repro lever for the save-crash: runs the same File>Save path (gridedit_save_map).
		// If it crashes, this result is never written and auto_command.txt is never consumed.
		extern void gridedit_save_map(void);
		const char* state = AutoHarness_GetAppState();
		if (strcmp(state, "editor") != 0)
		{
			_snprintf(result, sizeof(result), "ERROR: SAVE_LEVEL only works in the editor (state: %s)", state);
		}
		else
		{
			gridedit_save_map();
			_snprintf(result, sizeof(result), "OK: SAVE_LEVEL completed without crashing");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "LIST_ENTITIES") == 0)
	{
		Cmd_ListEntities(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "GET_ENTITY") == 0)
	{
		Cmd_GetEntity(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "LIST_LIGHTS") == 0)
	{
		Cmd_ListLights(result, sizeof(result));
	}
	else if (_stricmp(cmd, "DUMP_SKIN") == 0)
	{
		Cmd_DumpSkin(arg, result, sizeof(result));
	}
	else if (_stricmp(cmd, "SKIN_WATCH") == 0)
	{
		s_bSkinWatch = (arg[0] != '0');
		_snprintf(result, sizeof(result), "OK: SKIN_WATCH %s (frame=%u seen=%d)",
			s_bSkinWatch ? "ON" : "OFF", s_skinWatchFrame, (int)s_skinWatchSeen.size());
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "UNDO") == 0 || _stricmp(cmd, "REDO") == 0)
	{
		// route through the editor's own Ctrl+Z/Ctrl+Y handlers — they divert to the
		// Easy Building Editor's stack when EBE is open and do the selection cleanup.
		// Editor mode only: an undo fired during test game / storyboard would rewrite
		// editor state under the running game.
		if (bWelcomeScreen_Window || bStoryboardWindow || bImGuiInTestGame)
		{
			_snprintf(result, sizeof(result), "ERROR: %s only works in the level editor", cmd);
		}
		else
		{
			extern void editor_undo(void);
			extern void editor_redo(void);
			if (_stricmp(cmd, "UNDO") == 0) editor_undo(); else editor_redo();
			_snprintf(result, sizeof(result), "OK: %s performed", cmd);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SCULPT_TEST") == 0)
	{
		// SCULPT_TEST <worldX> <worldZ> <frames> [mode] — synthetic RAISE (default) stroke
		// at the given position for N frames; mode = GGTERRAIN_SCULPT_* int to override
		float sx = 0, sz = 0; int sf = 0, sm = GGTERRAIN_SCULPT_RAISE;
		int got = sscanf_s(arg, "%f %f %d %d", &sx, &sz, &sf, &sm);
		if (!AutoHarness_TerrainEditAllowed())
		{
			_snprintf(result, sizeof(result), "ERROR: SCULPT_TEST needs the level editor with terrain initialised");
		}
		else if (s_terrainTestFrames > 0)
		{
			_snprintf(result, sizeof(result), "ERROR: a terrain test is already running (%d frames left)", s_terrainTestFrames);
		}
		else if (s_grassTestFrames > 0)
		{
			_snprintf(result, sizeof(result), "ERROR: a grass test stroke is running (%d frames left)", s_grassTestFrames);   // GGMAX 3.118
		}
		else if (g_iCalculatingChangeBounds != 0 || s_treeTestFrames > 0 || ::g_iActiveTreeEvent != 0)   // 3.123: a tree stroke holds the shared snapshot too
		{
			_snprintf(result, sizeof(result), "ERROR: a real terrain or tree stroke is in progress — release the mouse first");
		}
		else if (got >= 3 && sf > 0)
		{
			s_terrainTestSavedEditMode = GGTerrain::ggterrain_extra_params.edit_mode;
			s_terrainTestSavedSculptMode = GGTerrain::ggterrain_extra_params.sculpt_mode;
			s_terrainTestSavedPaintMat = GGTerrain::ggterrain_extra_params.paint_material;
			s_terrainTestX = sx; s_terrainTestZ = sz;
			s_terrainTestSculptMode = sm;
			s_terrainTestPaint = 0;
			s_terrainTestFrames = sf;
			_snprintf(result, sizeof(result), "OK: SCULPT_TEST at (%.0f, %.0f) mode %d for %d frames (brushSize=%.0f)",
				sx, sz, sm, sf, GGTerrain::ggterrain_global_render_params2.brushSize);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SCULPT_TEST needs <worldX> <worldZ> <frames> [sculptmode]");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "PAINT_TEST") == 0)
	{
		// PAINT_TEST <worldX> <worldZ> <material> <frames> — synthetic texture-paint stroke;
		// material is the GG source-texture index + 1 (0 erases back to auto materials)
		float px = 0, pz = 0; int pm = 0, pf = 0;
		int got = sscanf_s(arg, "%f %f %d %d", &px, &pz, &pm, &pf);
		if (!AutoHarness_TerrainEditAllowed())
		{
			_snprintf(result, sizeof(result), "ERROR: PAINT_TEST needs the level editor with terrain initialised");
		}
		else if (s_terrainTestFrames > 0)
		{
			_snprintf(result, sizeof(result), "ERROR: a terrain test is already running (%d frames left)", s_terrainTestFrames);
		}
		else if (s_grassTestFrames > 0)
		{
			_snprintf(result, sizeof(result), "ERROR: a grass test stroke is running (%d frames left)", s_grassTestFrames);   // GGMAX 3.118
		}
		else if (g_iCalculatingChangeBounds != 0 || s_treeTestFrames > 0 || ::g_iActiveTreeEvent != 0)   // 3.123: a tree stroke holds the shared snapshot too
		{
			_snprintf(result, sizeof(result), "ERROR: a real terrain or tree stroke is in progress — release the mouse first");
		}
		else if (got == 4 && pf > 0)
		{
			s_terrainTestSavedEditMode = GGTerrain::ggterrain_extra_params.edit_mode;
			s_terrainTestSavedSculptMode = GGTerrain::ggterrain_extra_params.sculpt_mode;
			s_terrainTestSavedPaintMat = GGTerrain::ggterrain_extra_params.paint_material;
			s_terrainTestX = px; s_terrainTestZ = pz;
			s_terrainTestMaterial = pm;
			s_terrainTestPaint = 1;
			s_terrainTestFrames = pf;
			_snprintf(result, sizeof(result), "OK: PAINT_TEST at (%.0f, %.0f) material %d for %d frames (brushSize=%.0f)",
				px, pz, pm, pf, GGTerrain::ggterrain_global_render_params2.brushSize);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: PAINT_TEST needs <worldX> <worldZ> <material> <frames>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRASSPARAM") == 0)
	{
		// SET_GRASSPARAM scale|drawdist <value> — write the Grass Scale / Grass Draw Distance
		// slider globals, exactly what the editor UI writes. The per-frame Apply loops in
		// GGTerrainWicked push them to live systems (GGMAX 1.97: including MERGED systems'
		// grass_types[] tables — both sliders were dead on merged grass before that because the
		// typeIdx guard skipped the merged sentinel). READ THE VALUE BACK via HAIR_VIEWDIST /
		// HAIR_LEN in GET_PERF_DATA before believing any A/B — the flicker-hunt lesson.
		char gp[32] = { 0 }; float gv = 0.0f;
		if (sscanf_s(arg, "%31s %f", gp, (unsigned)sizeof(gp), &gv) == 2)
		{
			bool known = true;
			if (_stricmp(gp, "scale") == 0) GGGrass::gggrass_global_params.grass_scale = gv;
			else if (_stricmp(gp, "drawdist") == 0) GGGrass::gggrass_global_params.lod_dist = gv;
			else known = false;
			if (known)
				_snprintf(result, sizeof(result), "OK: SET_GRASSPARAM %s = %.1f (scale=%.1f lod_dist=%.0f)",
					gp, gv, GGGrass::gggrass_global_params.grass_scale, GGGrass::gggrass_global_params.lod_dist);
			else
				_snprintf(result, sizeof(result), "ERROR: SET_GRASSPARAM unknown param '%s' (scale|drawdist)", gp);
		}
		else
			_snprintf(result, sizeof(result), "ERROR: SET_GRASSPARAM needs <scale|drawdist> <value>");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TREES") == 0)
	{
		// live-tune tree shadow params (mirrors the Terrain Tools debug sliders):
		// SET_TREES shadowdist|shadowrange|drawshadows <value>
		char tp[64] = { 0 }; float tv = 0.0f;
		if (sscanf_s(arg, "%63s %f", tp, (unsigned)sizeof(tp), &tv) == 2)
		{
			bool known = true;
			if (_stricmp(tp, "shadowdist") == 0) GGTrees::ggtrees_global_params.lod_dist_shadow = tv;
			else if (_stricmp(tp, "shadowrange") == 0) GGTrees::ggtrees_global_params.tree_shadow_range = (int)tv;
			else if (_stricmp(tp, "drawshadows") == 0) GGTrees::ggtrees_global_params.draw_shadows = (int)tv;
			else if (_stricmp(tp, "stress") == 0) GGTrees::g_treePoolStressFrames = (int)tv;
			else if (_stricmp(tp, "draw") == 0)
			{
				// Elimination probe (corruption hunt): draw 0 = hide every pool tree + gate the
				// legacy draw path; draw 1 = re-enable + force a pool rescan to repopulate.
				GGTrees::ggtrees_draw_enabled = (int)tv;
				if ((int)tv == 0) GGTrees::GGTrees_HideAll();
				else GGTrees::g_treePoolStressFrames = 5;
			}
			else if (_stricmp(tp, "pool") == 0)
			{
				// Perf knob: effective tree pool size (nearest-N trees drawn as real ECS objects).
				// Each pool entity costs per-frame ECS, so this is the dominant editor CPU lever.
				// Applies on the next pool setup (level reload). Lower = fewer trees + big CPU win.
				int n = (int)tv; if (n < 1) n = 1; if (n > 20000) n = 20000;
				GGTrees::g_treePoolSize = (uint32_t)n;
			}
			else known = false;
			if (known)
				_snprintf(result, sizeof(result), "OK: SET_TREES %s = %.1f (shadowdist=%.0f shadowrange=%d drawshadows=%d)",
					tp, tv, GGTrees::ggtrees_global_params.lod_dist_shadow,
					GGTrees::ggtrees_global_params.tree_shadow_range,
					GGTrees::ggtrees_global_params.draw_shadows);
			else
				_snprintf(result, sizeof(result), "ERROR: SET_TREES unknown param '%s' (shadowdist|shadowrange|drawshadows)", tp);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_TREES needs <param> <value> (shadowdist|shadowrange|drawshadows)");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DELAYED_SHADOWS") == 0)
	{
		bool on = (arg[0] != '0');
		wi::renderer::SetDelayedShadowCascadesEnabled(on);
		_snprintf(result, sizeof(result), "OK: DELAYED_SHADOWS %s (staggered cascade refresh %s)",
			on ? "ON" : "OFF", wi::renderer::GetDelayedShadowCascadesEnabled() ? "enabled" : "disabled");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_LAPTOP_SHADOWS") == 0)
	{
		// A/B the "Laptop" aggressive delayed-shadow mode: far directional cascades refresh every 4th frame
		// instead of every 2nd. Forces Delayed Shadows ON and toggles Laptop; MasterRenderer::Update drives
		// the engine interval per-frame from these globals (reported value may lag one frame).
		extern bool g_bDelayedShadows;
		extern bool g_bDelayedShadowsLaptop;
		bool on = (arg[0] != '0');
		g_bDelayedShadows = true;
		g_bDelayedShadowsLaptop = on;
		wi::renderer::SetDelayedShadowCascadesEnabled(true);
		_snprintf(result, sizeof(result), "OK: SET_LAPTOP_SHADOWS %s (delayed far-cascade interval now %d frames)",
			on ? "ON" : "OFF", wi::renderer::GetDelayedShadowCascadeInterval());
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_ANIMVIS") == 0)
	{
		// A/B Wicked delta 1.35: frustum-visibility animation pause.
		//   SET_ANIMVIS <frames> [neardist]   — pause anims for objects unseen for >frames (0=off);
		//                                       neardist = never pause within this camera distance.
		extern int g_animVisPauseFrames;
		extern float g_animVisPauseNearDist;
		int vf = 0; float nd = -1.0f;
		int n = sscanf_s(arg, "%d %f", &vf, &nd);
		if (n >= 1)
		{
			g_animVisPauseFrames = vf;
			if (n >= 2 && nd >= 0.0f) g_animVisPauseNearDist = nd;
			_snprintf(result, sizeof(result), "OK: SET_ANIMVIS frames=%d neardist=%.0f (%s)",
				g_animVisPauseFrames, g_animVisPauseNearDist,
				g_animVisPauseFrames > 0 ? "pause unseen-object anims" : "OFF");
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_ANIMVIS <frames> [neardist]");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_MATCACHE") == 0)
	{
		// A/B Wicked delta 1.41: ShaderMaterial recompose cache (recompose only on dirty /
		// streaming-epoch change / 64-frame heartbeat; memcpy cached otherwise).
		// 1 = cached (default), 0 = stock full recompose every material every frame.
		bool on = (arg[0] != '0');
		wi::scene::gg_material_cache = on;
		_snprintf(result, sizeof(result), "OK: SET_MATCACHE %s", on ? "ON (cached)" : "OFF (stock recompose)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_MERGELISTS") == 0)
	{
		// A/B Wicked delta 1.40: command-list merges (occlusion -> prepass tail;
		// transparents+postFX -> one list). 1 = merged (default), 0 = stock structure.
		bool on = (arg[0] != '0');
		wi::gg_render_merge_lists = on;
		_snprintf(result, sizeof(result), "OK: SET_MERGELISTS %s", on ? "ON (merged)" : "OFF (stock lists)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TERRAINIDLE") == 0)
	{
		// A/B the terrain idle gate: when quiescent (camera parked, no pending chunks/edits)
		// Generation_Update runs every 8th frame instead of every frame (~0.8ms CPU saved).
		// 1 = gated (default), 0 = stock every-frame ring scan.
		bool on = (arg[0] != '0');
		g_terrainIdleGate = on;
		_snprintf(result, sizeof(result), "OK: SET_TERRAINIDLE %s", on ? "ON (gated)" : "OFF (every frame)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRASSLOD") == 0)
	{
		// A/B Wicked delta 1.49: grass strand LOD — beyond step2frac*viewDistance only every
		// 2nd strand draws, beyond step4frac*viewDistance every 4th; survivors widen by boost
		// (per step). GG grass systems only. SET_GRASSLOD <0|1> [step2frac step4frac boost]
		float f2 = 0, f4 = 0, bo = 0;
		int on = 0;
		int n = sscanf_s(arg, "%d %f %f %f", &on, &f2, &f4, &bo);
		if (n >= 1)
		{
			wi::gg_grass_lod = (on != 0);
			if (n >= 2 && f2 > 0) wi::gg_grass_lod_step2_frac = f2;
			if (n >= 3 && f4 > 0) wi::gg_grass_lod_step4_frac = f4;
			if (n >= 4 && bo > 0) wi::gg_grass_lod_width_boost = bo;
			// AUTO grass tiers follow this knob (1.0/1.7 off, 1.5/2.2 on) — nudge the gated
			// grass pass so the tier change applies without waiting for a camera move.
			// GGMAX 3.117: the DX11 far field (default) bypasses the engine strand LOD whenever it is on; it
			// also collapses the AUTO tiers - but only with the DX11 density and no explicit SET_GRASS tier/
			// lodchunks (GGGrass_FarRingCollapsed, the predicate ProcessGrassChunks uses). Say exactly which.
			extern bool g_grassPassNudge; extern int gg_grass_far_mode; extern bool GGGrass_FarRingCollapsed();
			g_grassPassNudge = true;
			const char* farNote = "";
			if (gg_grass_far_mode != 0)
				farNote = GGGrass_FarRingCollapsed()
					? " - INACTIVE: the DX11 far field (SET_GRASSFARMODE != 0) bypasses the strand LOD and has collapsed the AUTO tiers; SET_GRASSFARMODE 0 to A/B it"
					: " - strand LOD INACTIVE (the DX11 far field bypasses it; SET_GRASSFARMODE 0 to A/B it); the AUTO tiers still follow this knob (constant density or an explicit SET_GRASS tier)";
			_snprintf(result, sizeof(result), "OK: SET_GRASSLOD %s (step2=%.2f step4=%.2f boost=%.2f x viewdist)%s",
				wi::gg_grass_lod ? "ON" : "OFF",
				wi::gg_grass_lod_step2_frac, wi::gg_grass_lod_step4_frac, wi::gg_grass_lod_width_boost, farNote);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_GRASSLOD needs <0|1> [step2frac step4frac boost]");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_LEANASYNC") == 0)
	{
		// A/B Wicked delta 1.48c: keep the two big compute lists async but move the four tiny
		// helper lists (VT copy-pages, ocean sim+readback, VT tile-request+writeback) onto the
		// graphics queue — cross-queue fence hops 12 -> ~5, the ~1ms async overlap kept.
		bool on = (arg[0] != '0');
		wi::graphics::gg_lean_async = on;
		_snprintf(result, sizeof(result), "OK: SET_LEANASYNC %s", on ? "ON (helper lists on graphics)" : "OFF (stock async queues)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_SINGLEQUEUE") == 0)
	{
		// A/B Wicked delta 1.48b: route COMPUTE/COPY command lists onto the GRAPHICS
		// queue and drop same-queue fence dependencies. Measures how much of the GPU
		// frame WALL time is cross-queue fence-hop bubbles (submit-tail stall phase).
		// 0 = stock async queues (default), 1 = single queue.
		bool on = (arg[0] != '0');
		wi::graphics::gg_single_queue = on;
		_snprintf(result, sizeof(result), "OK: SET_SINGLEQUEUE %s", on ? "ON (graphics-only)" : "OFF (stock async queues)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TERRAINTILE") == 0)
	{
		// Wicked delta 1.53b: terrain VT tiling repeat CAP. All bake rungs finer than <cap>
		// repeats-per-chunk collapse to the cap scale (pure downsample chain = invisible
		// transitions near/mid); rungs at/below the cap keep stock halving (far anti-tiling).
		// Also sets the terrain texture's world feature size: chunk ~5120in -> cap 8 = ~16m
		// repeat, 16 = ~8m, 32 = ~4m. 0 = stock (cross-fades start at the camera).
		// Repaint of resident chunks is queued automatically — visible within a frame or two.
		// SET_TERRAINTILE <cap> [hold]: cap = world texture scale; hold delays the halving
		// ladder — the first visible scale cross-fade moves ~1.4x further out per +1 hold.
		int k = 0, hold = -1;
		int n = sscanf_s(arg, "%d %d", &k, &hold);
		GGTerrain::GGTerrainWicked_SetTileShare(n >= 1 ? k : 0, hold);
		_snprintf(result, sizeof(result), "OK: SET_TERRAINTILE cap=%d hold=%d (%s; repaint queued)",
			k, hold, k == 0 ? "stock tiling policy" : "cap = feature size, hold = handoff distance");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_GRASSWET") == 0)
	{
		// A/B Wicked delta 1.50: GG grass wetmap. 0 = force-dry (default; culled strands' zero
		// positions read as underwater and ratchet to wet~0.8 = near-black on reveal, drying
		// back over 15-30s). 1 = stock Wicked ocean/rain wetting to reproduce the artifact.
		bool on = (arg[0] != '0');
		wi::renderer::gg_grass_wetmap = on;
		_snprintf(result, sizeof(result), "OK: SET_GRASSWET %s", on ? "ON (stock wetting — dark-on-reveal bug live)" : "OFF (grass force-dried)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_SKYMODE") == 0)
	{
		// Sky investigation: drive the Customize Sky combo programmatically.
		// 0 = Simulated Sky (realistic sky + volumetric clouds), 1 = Sky Box, 2 = None.
		int sm = -1;
		if (sscanf_s(arg, "%d", &sm) >= 1 && sm >= 0 && sm <= 2)
		{
			extern void gridedit_set_sky_type(int iSkyType);
			gridedit_set_sky_type(sm);
			const char* names[] = { "Simulated Sky", "Sky Box", "None" };
			_snprintf(result, sizeof(result), "OK: SET_SKYMODE %d (%s; skyindex=%d disableSkybox=%d)",
				sm, names[sm], t.visuals.skyindex, t.visuals.bDisableSkybox ? 1 : 0);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_SKYMODE <0=simulated|1=skybox|2=none>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "RAY_COST") == 0)
	{
		// GGMAX diag: replicate Scene::Intersects' object loop for one ray and tally which
		// meshes would be triangle-tested (tris, bvh?, skinned?, name). arg = up|down|fwd.
		wi::scene::Scene& scene = wi::scene::GetScene();
		wi::scene::CameraComponent& cam = wi::scene::GetCamera();
		XMFLOAT3 o = cam.Eye;
		XMFLOAT3 d = XMFLOAT3(0, 1, 0);
		float fLen = 3000.0f;
		if (_stricmp(arg, "down") == 0) { d = XMFLOAT3(0, -1, 0); fLen = 300.0f; }
		else if (_stricmp(arg, "fwd") == 0) { d = cam.At; fLen = 120.0f; }
		wi::primitive::Ray ray(XMLoadFloat3(&o), XMVector3Normalize(XMLoadFloat3(&d)), 0, fLen);
		int written2 = _snprintf(result, sizeof(result), "RAY_COST %s from (%.0f,%.0f,%.0f) len %.0f:\n", arg[0] ? arg : "up", o.x, o.y, o.z, fLen);
		unsigned long long totalTris = 0, totalTrisNoBvh = 0; int hitObjects = 0;
		const size_t objectCount = std::min(scene.objects.GetCount(), scene.aabb_objects.size());
		for (size_t i = 0; i < objectCount; ++i)
		{
			const wi::primitive::AABB& aabb = scene.aabb_objects[i];
			if ((GGRENDERLAYERS_NORMAL & aabb.layerMask) == 0) continue;
			if (!ray.intersects(aabb)) continue;
			const wi::scene::ObjectComponent& object = scene.objects[i];
			if (object.meshID == wi::ecs::INVALID_ENTITY) continue;
			const wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(object.meshID);
			if (mesh == nullptr) continue;
			hitObjects++;
			unsigned long long tris = (unsigned long long)(mesh->indices.size() / 3);
			totalTris += tris;
			bool bBvh = mesh->bvh.IsValid();
			bool bSkin = mesh->IsSkinned() || !mesh->vertex_boneindices.empty();
			if (!bBvh) totalTrisNoBvh += tris;
			const wi::scene::NameComponent* nm = scene.names.GetComponent(object.meshID);
			const wi::scene::NameComponent* no = scene.names.GetComponent(scene.objects.GetEntity(i));
			if (written2 < (int)sizeof(result) - 256 && (tris > 200 || !bBvh))
			{
				written2 += _snprintf(result + written2, sizeof(result) - written2,
					"  obj=%s mesh=%s tris=%llu bvh=%d skin=%d renderable=%d\n",
					no ? no->name.c_str() : "?", nm ? nm->name.c_str() : "?",
					tris, bBvh ? 1 : 0, bSkin ? 1 : 0, object.IsRenderable() ? 1 : 0);
			}
		}
		if (written2 < (int)sizeof(result) - 128)
			written2 += _snprintf(result + written2, sizeof(result) - written2,
				"TOTAL: %d objects, %llu tris in path, %llu tris WITHOUT bvh\n", hitObjects, totalTris, totalTrisNoBvh);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "LIST_SKINNED") == 0)
	{
		// GGMAX diag: every object whose mesh is skinned/bone-weighted — with its world
		// AABB — to test the "characters have giant AABBs so every ray pays them" theory.
		wi::scene::Scene& scene = wi::scene::GetScene();
		int written2 = _snprintf(result, sizeof(result), "SKINNED OBJECTS:\n");
		const size_t objectCount = std::min(scene.objects.GetCount(), scene.aabb_objects.size());
		for (size_t i = 0; i < objectCount; ++i)
		{
			const wi::scene::ObjectComponent& object = scene.objects[i];
			if (object.meshID == wi::ecs::INVALID_ENTITY) continue;
			const wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(object.meshID);
			if (mesh == nullptr) continue;
			if (!mesh->IsSkinned() && mesh->vertex_boneindices.empty()) continue;
			const wi::primitive::AABB& aabb = scene.aabb_objects[i];
			XMFLOAT3 mn = aabb.getMin(), mx = aabb.getMax();
			const wi::scene::NameComponent* no = scene.names.GetComponent(scene.objects.GetEntity(i));
			if (written2 < (int)sizeof(result) - 256)
				written2 += _snprintf(result + written2, sizeof(result) - written2,
					"  %s tris=%llu layer=%08x renderable=%d aabb=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f)\n",
					no ? no->name.c_str() : "?", (unsigned long long)(mesh->indices.size() / 3),
					aabb.layerMask, object.IsRenderable() ? 1 : 0,
					mn.x, mn.y, mn.z, mx.x, mx.y, mx.z);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "RUN_LUA") == 0)
	{
		// GGMAX diag: execute a lua chunk in the live game lua state. Runs on the main
		// thread at the harness poll point (top of GuruLoopLogic, before common_loop_logic,
		// so never mid-script). Use 'return <expr>' to read values back.
		extern int RunLuaString(char* pCode, char* pResultBuf, int iResultSize);
		static char luaResult[8192];
		int rc = RunLuaString(arg, luaResult, sizeof(luaResult));
		_snprintf(result, sizeof(result), "%s: %s", (rc == 0) ? "OK" : "FAIL", luaResult[0] ? luaResult : "(no result)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_VOLCLOUDS") == 0)
	{
		// Sky investigation discriminator: toggle ONLY the volumetric cloud pass, leaving
		// realistic sky and fog untouched. If distant terrain reappears with clouds off,
		// the cloud composite is what paints over it.
		int vc = -1;
		if (sscanf_s(arg, "%d", &vc) >= 1 && vc >= 0 && vc <= 1)
		{
			extern wi::ecs::Entity g_weatherEntityID;
			wi::scene::WeatherComponent* weather = wi::scene::GetScene().weathers.GetComponent(g_weatherEntityID);
			if (weather)
			{
				weather->SetVolumetricClouds(vc != 0);
				_snprintf(result, sizeof(result), "OK: SET_VOLCLOUDS %d (live-only; next sky-mode change or visuals update restores)", vc);
			}
			else
			{
				_snprintf(result, sizeof(result), "ERROR: SET_VOLCLOUDS no weather component");
			}
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_VOLCLOUDS <0|1>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_FOGDENS") == 0)
	{
		// Sky investigation discriminator: write weather fogDensity directly (0 = fog fully
		// off). Negative value = restore the level's fog model from t.visuals. Live-only.
		float fd = -999.0f;
		if (sscanf_s(arg, "%f", &fd) >= 1 && fd > -998.0f)
		{
			extern wi::ecs::Entity g_weatherEntityID;
			wi::scene::WeatherComponent* weather = wi::scene::GetScene().weathers.GetComponent(g_weatherEntityID);
			if (weather)
			{
				if (fd < 0.0f)
				{
					extern void Wicked_Update_Fog(void* visual);
					Wicked_Update_Fog((void*)&t.visuals);
					_snprintf(result, sizeof(result), "OK: SET_FOGDENS restored from visuals (start=%.0f density=%g)",
						weather->fogStart, weather->fogDensity);
				}
				else
				{
					weather->fogDensity = fd;
					_snprintf(result, sizeof(result), "OK: SET_FOGDENS %g (start=%.0f; live-only)", fd, weather->fogStart);
				}
			}
			else
			{
				_snprintf(result, sizeof(result), "ERROR: SET_FOGDENS no weather component");
			}
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_FOGDENS <density|-1=restore>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "BURST_FRAMES") == 0)
	{
		// Capture N consecutive rendered frames to Files/screenshots/frame_###.png
		// (temporal-artifact instrument; frame time balloons during capture from the
		// per-frame GPU readback — expected).
		int n = atoi(arg);
		if (n >= 2 && n <= 120)
		{
			g_burstFrameIndex = 0;
			g_burstFramesRemaining = n;
			_snprintf(result, sizeof(result), "OK: BURST_FRAMES capturing %d consecutive frames to Files/screenshots/frame_###.png", n);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: BURST_FRAMES <2-120>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_CHARSHADOW") == 0)
	{
		// Per-character dedicated sun shadow slots (nearest-N to camera, hard cap 3 —
		// 5x2048 sun + 3x2048 dedicated exactly fill the 16384 atlas). 0 = off (stock).
		extern int g_iCharShadowMax;
		int n = atoi(arg);
		if (n >= 0 && n <= 3)
		{
			g_iCharShadowMax = n;
			// NOTE: the slot list is rebuilt in Master::Update AFTER this command runs, so the
			// count below reflects the PREVIOUS max for one frame. A "0" right after switching
			// 0->3 is normal — re-issue the command a frame later for the true count. (This
			// stale read caused the 2026-07-30 "recreated characters lose their dedicated slot"
			// false alarm — measured phantom on 2026-07-31, discovery is stateless per frame.)
			_snprintf(result, sizeof(result), "OK: SET_CHARSHADOW %d (slots last frame: %d — one frame stale after a change, re-query to confirm)",
				n, (int)wiScene::GetScene().character_dedicated_shadows.size());
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_CHARSHADOW <0-3>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_SHADOWBIAS") == 0)
	{
		// A/B Wicked delta 1.57: receiver-side depth bias for the sun's cascade shadows,
		// in D16 shadow-atlas ULPs. Cures animated self-shadow flicker (back/neck of an
		// idling character). 0 = stock hard compare (reproduces the flicker); default 2.
		float ulps = -1.0f;
		int n = sscanf_s(arg, "%f", &ulps);
		if (n >= 1 && ulps >= 0.0f && ulps <= 64.0f)
		{
			wi::renderer::gg_shadow_receiver_bias = ulps / 65536.0f;
			_snprintf(result, sizeof(result), "OK: SET_SHADOWBIAS %.2f ULPs (%.3e NDC)%s",
				ulps, wi::renderer::gg_shadow_receiver_bias, ulps == 0.0f ? " (stock, flicker reproducible)" : "");
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_SHADOWBIAS <0-64 ulps>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_TANGENTVIS") == 0)
	{
		// GGMAX 1.62: tangent-frame visualization for skinned normal-map flicker forensics.
		// Replaces object shading with raw pipeline data (engine objectHF.hlsli early-out):
		// 0=off, 1=world tangent RGB, 2=interpolated vertex normal RGB, 3=final bumped
		// normal RGB, 4=tangent handedness (red=-1 green=+1), 5=strength-scaled normal-map
		// sample rg. Pair with BURST_FRAMES: if mode-1 colors churn on a near-still pose,
		// the skinned tangent DATA is per-frame unstable; if stable, the artifact is
		// downstream (map decode/lighting).
		extern bool g_tvCycleActive;
		int mode = -1;
		if (_strnicmp(arg, "CYCLE", 5) == 0)
		{
			// Auto-cycle all modes, 3s dwell each, 0 (normal shading) marks the loop start.
			g_tvCycleActive = true;
			_snprintf(result, sizeof(result), "OK: SET_TANGENTVIS CYCLE (3s per mode, 0->20, normal shading = loop marker; any SET_TANGENTVIS <n> stops)");
		}
		else if (sscanf_s(arg, "%d", &mode) >= 1 && ((mode >= 0 && mode <= 27) || (mode >= 40 && mode <= 42))) // GGMAX 3.35e: +albedo, +F; 3.113: +25-27 final-colour summands; 3.115: 40-42 Terrain Bake near detail
		{
			g_tvCycleActive = false;
			wi::renderer::gg_debugvis = mode;
			static const char* tvNames[28] = { "off", "world tangent", "vertex normal", "bumped normal", "handedness", "normal-map sample",
				"basecolor UV raw", "basecolor UV x64 grid", "basecolor tex sample", "final albedo input", "ORM sample",
				"roughness", "specular F0", "occlusion", "vertex color", "emissive", "world-pos grid",
				"direct diffuse", "direct specular", "indirect diffuse", "indirect specular",
				"RAW normal-map texels", "RAW normal-map mip0",
				"albedo (post-create)", "F (diffuse scaled by 1-F)",
				"SUN share of final (linear)", "AMBIENT share of final (linear)", "SPECULAR share of final (linear)" };
			static const char* tvBake[3] = { "bake near-detail weight", "bake detail layers sampled (/4)", "bake-only albedo" };   // 3.115, bake PS only
			_snprintf(result, sizeof(result), "OK: SET_TANGENTVIS %d (%s)", mode, (mode >= 40) ? tvBake[mode - 40] : tvNames[mode]);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_TANGENTVIS <0-22|CYCLE>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_WETMAPS") == 0)
	{
		// Engine 1.62e: 0 = skip the whole RefreshWetmaps compute pass (object + hair loops).
		// Streamout-stomp suspect A/B: wetmap junk values match the captured foreign w data
		// (small decaying-toward-zero floats). If the mode-4 coin-flip dies with this OFF on
		// an afflicted load, the wetmap pass is the writer.
		int wm = -1;
		if (sscanf_s(arg, "%d", &wm) >= 1 && wm >= 0 && wm <= 1)
		{
			wi::renderer::gg_wetmap_updates_enabled = (wm != 0);
			_snprintf(result, sizeof(result), "OK: SET_WETMAPS %d (%s)", wm, wm ? "wetmap updates ON" : "wetmap updates SKIPPED");
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_WETMAPS <0|1>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DUMP_SOTAN") == 0)
	{
		// DUMP_SOTAN <name-substr> — byte-level forensics for the tangent-w coin-flip: read back
		// the first matching skinned mesh's whole streamoutBuffer on TWO consecutive frames and
		// diff the so_tan region (R16G16B16A16_FLOAT). Reports w-sign flips (count + contiguous
		// runs = writer fingerprint), xyz churn, and whether w values are legit +-1 halves or
		// garbage (aliased-memory tell). Writes sotan_dump.txt (exe dir).
		wi::scene::Scene& soScene = wi::scene::GetScene();
		auto soContains = [](const std::string& hay, const char* needle) -> bool {
			std::string h = hay, n = needle;
			for (auto& c : h) c = (char)tolower((unsigned char)c);
			for (auto& c : n) c = (char)tolower((unsigned char)c);
			return h.find(n) != std::string::npos;
		};
		g_sotanMesh = wi::ecs::INVALID_ENTITY;
		{
			// nearest-to-camera match: the flickering test characters, not the far crowd
			const XMFLOAT3 soCam = wi::scene::GetCamera().Eye;
			float soBest = 1e30f;
			for (size_t soi = 0; soi < soScene.objects.GetCount(); ++soi)
			{
				wi::ecs::Entity soe = soScene.objects.GetEntity(soi);
				const wi::scene::NameComponent* son = soScene.names.GetComponent(soe);
				if (son == nullptr || !soContains(son->name, arg)) continue;
				const wi::scene::ObjectComponent& soo = soScene.objects[soi];
				if (!soo.IsRenderable()) continue;
				const wi::scene::MeshComponent* som = soScene.meshes.GetComponent(soo.meshID);
				if (som == nullptr || !som->streamoutBuffer.IsValid() || !som->so_tan.IsValid()) continue;
				const wi::scene::TransformComponent* sot = soScene.transforms.GetComponent(soe);
				float dx = sot ? (sot->world._41 - soCam.x) : 1e15f;
				float dy = sot ? (sot->world._42 - soCam.y) : 1e15f;
				float dz = sot ? (sot->world._43 - soCam.z) : 1e15f;
				float d2 = dx * dx + dy * dy + dz * dz;
				if (d2 < soBest)
				{
					soBest = d2;
					g_sotanMesh = soo.meshID;
					_snprintf(g_sotanLabel, sizeof(g_sotanLabel), "%s (entity %llu meshID %llu dist %.0f)", son->name.c_str(), (unsigned long long)soe, (unsigned long long)soo.meshID, sqrtf(d2));
					g_sotanLabel[sizeof(g_sotanLabel) - 1] = 0;
				}
			}
		}
		if (g_sotanMesh != wi::ecs::INVALID_ENTITY)
		{
			g_sotanPhase = 1;
			_snprintf(result, sizeof(result), "OK: DUMP_SOTAN armed for %s — two-frame capture, read sotan_dump.txt in ~2 frames", g_sotanLabel);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: DUMP_SOTAN no renderable skinned mesh with so_tan matches \"%s\"", arg);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_BINDPOSE_TAN") == 0)
	{
		// Engine 1.62c discriminator: 1 = raster tangents come from the BIND-POSE buffer
		// (skip the skinned so_tan patch in RunMeshUpdateSystem). If the per-load tangent-w
		// coin-flip (SET_TANGENTVIS 4) dies with this ON, the poisoned stage is the skinned
		// streamout CONTENT; if it persists, the consumer path. 0 = normal skinned tangents.
		int bp = -1;
		if (sscanf_s(arg, "%d", &bp) >= 1 && bp >= 0 && bp <= 1)
		{
			wi::renderer::gg_force_bindpose_tangents = (bp != 0);
			_snprintf(result, sizeof(result), "OK: SET_BINDPOSE_TAN %d (%s)", bp, bp ? "bind-pose tangents" : "skinned streamout tangents");
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_BINDPOSE_TAN <0|1>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_NORMSTR") == 0)
	{
		// SET_NORMSTR <value> — set normalMapStrength on EVERY scene material that has a
		// normal map bound. Brute-force A/B lever for the flicker-vs-strength law (the saved
		// test level carries strength 4 on some characters; no per-entity setter exists in
		// the harness). Materials are dirtied so the composed cache refreshes.
		float ns = -1.0f;
		if (sscanf_s(arg, "%f", &ns) >= 1 && ns >= 0.0f && ns <= 8.0f)
		{
			wi::scene::Scene& nsScene = wi::scene::GetScene();
			int nsCount = 0;
			for (size_t nsi = 0; nsi < nsScene.materials.GetCount(); ++nsi)
			{
				wi::scene::MaterialComponent& nsMat = nsScene.materials[nsi];
				if (nsMat.textures[wi::scene::MaterialComponent::NORMALMAP].resource.IsValid())
				{
					nsMat.normalMapStrength = ns;
					nsMat.SetDirty();
					nsCount++;
				}
			}
			_snprintf(result, sizeof(result), "OK: SET_NORMSTR %.2f applied to %d materials with normal maps", ns, nsCount);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_NORMSTR <0-8>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_BLENDSCAN") == 0)
	{
		// A/B the post-load dip fix: blend-scan cadence outside initial build.
		//   SET_BLENDSCAN <1-60>   — 1 = stock every-frame scans (reproduces the
		//   post-load FPS dip), 4 = default paced.
		extern int g_blendScanInterval;
		int n = atoi(arg);
		if (n >= 1 && n <= 60)
		{
			g_blendScanInterval = n;
			_snprintf(result, sizeof(result), "OK: SET_BLENDSCAN every %d frame(s)%s", n, n == 1 ? " (stock, dip reproducible)" : "");
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_BLENDSCAN <1-60>");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_LIGHTSHAFTS") == 0)
	{
		// A/B the light shafts fix (engine 1.56 sun-mask + DX11 exposure parity 0.18):
		//   SET_LIGHTSHAFTS <0|1> [strength]   — toggle shafts; optional strength override
		//   (engine "exposure"; DX11 parity = 0.18, upstream Wicked default = 0.5).
		extern MasterRenderer * master_renderer;
		int on = 1; float strength = -1.0f;
		int n = sscanf_s(arg, "%d %f", &on, &strength);
		if (master_renderer && n >= 1)
		{
			master_renderer->setLightShaftsEnabled(on != 0);
			if (n >= 2 && strength >= 0.0f) master_renderer->setLightShaftsStrength(strength);
			_snprintf(result, sizeof(result), "OK: SET_LIGHTSHAFTS %s strength=%.3f",
				on ? "ON" : "OFF", master_renderer->getLightShaftsStrength());
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_LIGHTSHAFTS <0|1> [strength] (master_renderer=%p)", (void*)master_renderer);
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_HAIRSKIP") == 0)
	{
		// A/B Wicked delta 1.37: hair/grass sim reduction at a parked camera.
		//   SET_HAIRSKIP <0|1> [windInterval]
		// 1 = enabled (default): no wind -> skip the simulate dispatch entirely; wind active ->
		// simulate every windInterval-th frame (default 4, slow-motion sway). 0 = stock every-frame.
		int on_i = 0, interval = -1;
		int n = sscanf_s(arg, "%d %d", &on_i, &interval);
		bool on = (n >= 1 && on_i != 0);
		wi::renderer::gg_hair_sim_static_skip = on;
		if (n >= 2 && interval >= 1)
			wi::renderer::gg_hair_sim_wind_interval = (uint32_t)interval;
		_snprintf(result, sizeof(result), "OK: SET_HAIRSKIP %s (wind interval %u)",
			on ? "ON" : "OFF (stock every-frame sim)", wi::renderer::gg_hair_sim_wind_interval);
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_HIERLO") == 0)
	{
		// A/B Wicked delta 1.36: subtree-parallel hierarchy update (one job per root, carry the
		// world down the chain, one multiply per bone) vs stock per-entity ancestor chain walk.
		bool on = (arg[0] != '0');
		wi::scene::gg_hierarchy_levelorder = on;
		_snprintf(result, sizeof(result), "OK: SET_HIERLO %s", on ? "ON (subtree-parallel)" : "OFF (stock chain walk)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_VTINC") == 0)
	{
		// A/B Wicked delta 1.33: incremental terrain-VT bookkeeping (dirty-tracked page-table
		// uploads + lazy free-list rebuild). 1 = incremental (default), 0 = stock every-frame
		// full rewrite (the old ~16ms/frame VT job). Applies instantly.
		bool on = (arg[0] != '0');
		wi::terrain::gg_vt_incremental = on;
		_snprintf(result, sizeof(result), "OK: SET_VTINC %s (VT page-table uploads %s)",
			on ? "ON" : "OFF", on ? "dirty-tracked" : "stock every-frame");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SHADOW_LOD_OVERRIDE") == 0)
	{
		// A/B the "two terrain shapes" shadow flicker: ON = per-cascade shadow LOD (can oscillate vs
		// the visible terrain), OFF = shadows use the stable main-view LOD.
		bool on = (arg[0] != '0');
		wi::renderer::SetShadowLODOverrideEnabled(on);
		_snprintf(result, sizeof(result), "OK: SHADOW_LOD_OVERRIDE %s (%s)",
			on ? "ON" : "OFF", wi::renderer::IsShadowLODOverrideEnabled() ? "per-cascade LOD" : "view LOD");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "SET_OCEAN") == 0)
	{
		// live-tune ocean foam: SET_OCEAN foamscale|foamamount <value>
		// (ocean CB refills from oceanParameters every frame, applies instantly)
		char op[64] = { 0 }; float ov = 0.0f;
		extern float g_fWaterFoamUnitScale;
		extern float g_fWaterFoamAmount;
		extern wi::ecs::Entity g_weatherEntityID;
		if (sscanf_s(arg, "%63s %f", op, (unsigned)sizeof(op), &ov) == 2)
		{
			bool known = true;
			static float s_harnessCausticScale = -1.0f;
				static float s_harnessWaterHeight = -99999.0f;
				static float s_harnessUwDensity = -1.0f;
				static float s_wcR = -1.0f, s_wcG = -1.0f, s_wcB = -1.0f, s_wcA = -1.0f;
				static float s_wcDepth = -1.0f;
			if (_stricmp(op, "foamscale") == 0) g_fWaterFoamUnitScale = ov;
			else if (_stricmp(op, "foamamount") == 0) g_fWaterFoamAmount = ov;
			else if (_stricmp(op, "causticscale") == 0) s_harnessCausticScale = ov;
				else if (_stricmp(op, "waterheight") == 0) s_harnessWaterHeight = ov;
				else if (_stricmp(op, "uwdensity") == 0) s_harnessUwDensity = ov;
				else if (_stricmp(op, "wcr") == 0) s_wcR = ov;
				else if (_stricmp(op, "wcg") == 0) s_wcG = ov;
				else if (_stricmp(op, "wcb") == 0) s_wcB = ov;
				else if (_stricmp(op, "wca") == 0) s_wcA = ov;
				else if (_stricmp(op, "wcdepth") == 0) s_wcDepth = ov;
			else known = false;
			wi::scene::WeatherComponent* weather = wi::scene::GetScene().weathers.GetComponent(g_weatherEntityID);
			if (weather)
			{
				weather->oceanParameters.foam_unit_scale = g_fWaterFoamUnitScale;
				weather->oceanParameters.foam_amount = g_fWaterFoamAmount;
				// Live-tune the seabed caustic scale for quick A/B (see SET_OCEAN causticscale).
				// The Water panel "Caustic Size" slider re-takes control the next time it changes.
				if (s_harnessCausticScale > 0.0f)
					weather->oceanParameters.caustic_scale = s_harnessCausticScale;
					// Test hook: raise/lower the ocean water line (SET_OCEAN waterheight <y>) to
					// submerge the editor camera and verify Wicked's underwaterCS post-process. Reset
					// with the real water line; Wicked_Update_Visuals re-takes control on next change.
					if (s_harnessWaterHeight > -99998.0f)
						weather->oceanParameters.waterHeight = s_harnessWaterHeight;
					// Live-tune the decoupled underwater fog density (SET_OCEAN uwdensity <v>) to find
					// a good default against GG's inch scale, then bake into the visuals default.
					if (s_harnessUwDensity >= 0.0f)
						weather->oceanParameters.underwater_fog_density = s_harnessUwDensity;
					// Live-set the surface water base colour (0..1 each) to reproduce/verify the
					// "Water Base Color doesn't tint from above" issue without the UI picker.
					if (s_wcR >= 0.0f) weather->oceanParameters.waterColor.x = s_wcR;
					if (s_wcG >= 0.0f) weather->oceanParameters.waterColor.y = s_wcG;
					if (s_wcB >= 0.0f) weather->oceanParameters.waterColor.z = s_wcB;
					if (s_wcA >= 0.0f) weather->oceanParameters.waterColor.w = s_wcA;
					if (s_wcDepth >= 0.0f) weather->oceanParameters.water_color_depth = s_wcDepth;
			}
			if (known)
				_snprintf(result, sizeof(result), "OK: SET_OCEAN %s = %.4f (foamscale=%.4f foamamount=%.2f weather=%s)",
					op, ov, g_fWaterFoamUnitScale, g_fWaterFoamAmount, weather ? "live" : "MISSING");
			else
				_snprintf(result, sizeof(result), "ERROR: SET_OCEAN unknown param '%s' (foamscale|foamamount|causticscale|waterheight|uwdensity)", op);
		}
		else
		{
			_snprintf(result, sizeof(result), "ERROR: SET_OCEAN needs <param> <value> (foamscale|foamamount)");
		}
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "ENABLE_PROFILER") == 0)
	{
		// Must set bProfilerEnable too — M-GridEditB_part3.cpp actively disables the
		// profiler every frame in editor mode if bProfilerEnable is false
		extern bool bProfilerEnable;
		bProfilerEnable = true;
		wi::profiler::SetEnabled(true);
		_snprintf(result, sizeof(result), "OK: Profiler enabled (bProfilerEnable=true, SetEnabled=true)");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "DISABLE_PROFILER") == 0)
	{
		extern bool bProfilerEnable;
		bProfilerEnable = false;
		wi::profiler::SetEnabled(false);
		_snprintf(result, sizeof(result), "OK: Profiler disabled");
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "GET_PROFILER_STATUS") == 0)
	{
		_snprintf(result, sizeof(result),
			"OK: REQ=%d EN=%d IsEnabled=%d CPU=%.2f GPU=%.2f",
			wi::profiler::ENABLED_REQUEST?1:0, wi::profiler::ENABLED?1:0,
			wi::profiler::IsEnabled()?1:0,
			wi::profiler::GetCPUFrameTime(),
			wi::profiler::GetGPUFrameTime());
		result[sizeof(result) - 1] = 0;
	}
	else if (_stricmp(cmd, "QUIT") == 0)
	{
		Cmd_Quit(result, sizeof(result));
		return; // Don't write result again, Cmd_Quit already did
	}
	else
	{
		_snprintf(result, sizeof(result), "ERROR: Unknown command '%s'", cmd);
		result[sizeof(result) - 1] = 0;
	}

	AutoHarness_WriteResult(result);
}
