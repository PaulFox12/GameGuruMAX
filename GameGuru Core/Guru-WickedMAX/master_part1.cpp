//
// MasterRenderer Functions
//
#include "GGAnimBridge.h"
#include "wiGraphicsDevice_DX12.h" // Phase 5: For DX12 ImGui rendering in Compose
#include "GGTerrain/GGTerrainWicked.h"
#include "GGTerrain/GGTerrainBake.h" // GGMAX 3.25: Terrain Bake
#include "GGTerrain/GGTerrainFar.h"  // GGMAX 3.114: far terrain (the DX11 horizon)
#include "GGTerrain/GGWaterBake.h"   // GGMAX 3.25: Water Bake
#include "wiProfiler.h"
#include "wiTerrain.h" // GGMAX 1.71: gg_svt_atlas_height (setup.ini svtatlasheight)
#include "wiRenderer.h" // GGMAX 2.89: SetProbeView (setup.ini probeview / probeviewmip)
#include "wiHelper.h"   // GGMAX 3.35i: GetDiagnosticPath, for GGDiagFopen below
#include <atomic>
#include <chrono>

// GGMAX delta 1.29: engine-side 30fps animation throttle flags (defined in WickedEngine/wiScene.cpp).
// MasterRenderer::Update drives these per-frame from the editor "Lower Animation & LUA Speed" checkbox,
// before __super::Update runs the animation jobs. Replaces the removed GGAnimBridge pre-hook.
namespace wi { namespace scene {
	extern std::atomic<uint32_t> gg_anim30fps_enabled;
	extern std::atomic<uint32_t> gg_anim30fps_frame;
	extern std::atomic<uint32_t> gg_anim30fps_dist2;
	// GGMAX delta 1.35: frustum-visibility animation pause (defined in WickedEngine/wiScene.cpp)
	extern std::atomic<uint32_t> gg_anim_vis_pause_frames;
	extern std::atomic<uint32_t> gg_anim_vis_pause_neardist2;
	// GGMAX 3.25: Reduction Scale (defined in WickedEngine/wiScene.cpp)
	extern std::atomic<uint32_t> gg_anim_reduction_scale;
	extern float gg_anim_reduction_grace;
	extern float gg_anim_reduction_grace_seconds;
	extern uint32_t gg_anim_reduction_resets;
	void gg_ResetAnimReduction();
} }
// GGMAX delta 1.30: engine-side apparent-size object cull threshold (defined in WickedEngine/wiRenderer.cpp).
// MasterRenderer::Update drives it per-frame from the editor "Apparent Size" slider (see below).
namespace wi { namespace renderer {
	extern std::atomic<uint32_t> gg_apparent_cull_bits;
} }

// Cached profiler text — updated in Compose() when GPU ranges are active.
// GetTextData() called during Update misses GPU sub-ranges because BeginFrame()
// clears in_use before Render creates GPU ranges. Capturing in Compose() gets all ranges.
static std::string g_cachedProfilerText;
std::string GGPerf_GetCachedProfilerText() { return g_cachedProfilerText; }

// GGMAX 3.15: analytic pre-reject in waypoint_mousemanage. 1 = on (default), 0 = the old
// intersect-every-node behaviour, for A/B. See M-Waypoint.cpp for why the bound is safe.
int gg_waypoint_fastreject = 1;

// GGMAX 3.16: entity_loopanim shape counters (see G-Entity_part1.cpp).
uint32_t gg_elanim_total = 0, gg_elanim_skip_noent = 0, gg_elanim_skip_static = 0, gg_elanim_work = 0;
uint32_t gg_elanim_ff_entities = 0, gg_elanim_ff_sets = 0;   // GGMAX 3.16 footfall scan shape
int gg_elanim_ff_charonly = 0;   // GGMAX 3.17: 1 = footfall scan for characters only
int gg_elanim_ff_cache = 1;      // GGMAX 3.18: 1 = cached step triples, 0 = walk the list
int gg_elanim_skipwork = 0;   // GGMAX 3.16 diagnostic, see G-Entity_part1.cpp

// GGMAX 1.67: main-camera poly count for the Performance HUD (latched per frame in the engine)
uint64_t GGPerf_GetPolyCount() { return wi::renderer::GG_GetMainCameraPolyCount(); }

// GGMAX 1.71: setup.ini `svtatlasheight` bridge. The terrain virtual-texture physical atlas is a
// fixed cost (16384 tall = a 768 MB tile pool) regardless of how much a level uses, and measured
// residency is only ~26% of it. Must be set before the atlas is created (first terrain update),
// which is why it comes from setup.ini rather than a runtime harness command. See VRAM_CENSUS.md.
void GGSetSVTAtlasHeight(int height)
{
	if (height >= 2048 && height <= 16384)
	{
		wi::terrain::gg_svt_atlas_height = (uint32_t)height;
	}
}

// GGMAX 1.98 (A4): setup.ini `svtemissive` bridge — 1 restores the stock 4th SVT map.
void GGSetSVTKeepEmissive(int keep)
{
	wi::terrain::gg_svt_keep_emissive = (keep != 0);
}

// GGMAX low-VRAM preset bridge (setup.ini `lowvram`, harness SET_LOWVRAM). One entry point so the
// preset's members stay in one place — the old-namespace setup.ini reader cannot declare engine
// namespace externs at block scope, and spraying them through it would rot.
//
// Members today:
//   * grass draw distance cap (gg_lowvram, GGTerrainWicked.cpp)
//   * grass density scale     (gg_lowvram_grass_density, same file)
//
// GGMAX 1.82: lazy object PSOs USED to be a member of this preset and no longer are. They became
// the default for everyone, so the preset does not need to switch them on, and — more importantly
// — leaving the line here would make the outcome depend on the order the two setup.ini keys
// happen to appear in. `lazypso` is now the single owner of that flag. See GGSetLazyPSO below.
// GGMAX: the preset has TWO independent switches that OR together into the one effective flag
// every consumer reads (`gg_lowvram`): the MACHINE switch (setup.ini `lowvram`, for owners of
// 4 GB cards — applies to every level) and the LEVEL switch (the "Low VRAM Mode" checkbox in
// Graphics and Performance, saved per-level in the FPM). Either one on = preset on.
static bool gg_lowvram_machine = false;
static bool gg_lowvram_level = false;
static void GGRecomputeLowVRAM()
{
	extern bool gg_lowvram;                 // GGTerrainWicked.cpp — the effective flag
	gg_lowvram = gg_lowvram_machine || gg_lowvram_level;
}
void GGSetLowVRAM(int on)
{
	gg_lowvram_machine = (on != 0);
	GGRecomputeLowVRAM();
}
void GGSetLowVRAMLevel(int on)
{
	gg_lowvram_level = (on != 0);
	GGRecomputeLowVRAM();
}

// GGMAX 2.94: the four brutal off-switches, same (machine || level) shape as lowvram above.
// MACHINE = setup.ini (disableterrainall / disabletreesall / disablegrassall / disablewaterall),
// LEVEL   = the tick boxes in Graphics and Performance. Either one on = the subsystem does not
// exist. Effective flags are defined in GGTerrain/GGTerrainWicked.cpp next to gg_lowvram.
//
// INT PASSTHROUGH: the setters take int, never bool - the 2.89 lesson (an INI bool-ization was
// the reason a knob looked dead when the pipeline was fine). The (on != 0) inside the setter is
// the same shape GGSetLowVRAM uses and keeps mode-widening open.
extern bool gg_no_terrain;   // GGTerrainWicked.cpp - the effective flags
extern bool gg_no_trees;
extern bool gg_no_grass;
extern bool gg_no_water;
static bool gg_no_terrain_machine = false, gg_no_terrain_level = false;
static bool gg_no_trees_machine   = false, gg_no_trees_level   = false;
static bool gg_no_grass_machine   = false, gg_no_grass_level   = false;
static bool gg_no_water_machine   = false, gg_no_water_level   = false;
extern bool gg_no_postfx, gg_no_ao, gg_simple_sky, gg_no_shadows;   // GGMAX 3.08
extern bool gg_no_occlusion; extern int gg_particle_pct;             // GGMAX 3.09
extern float gg_object_cull_dist;                                    // GGMAX 3.11
void GGSetNoTerrain     (int on) { gg_no_terrain_machine = (on != 0); gg_no_terrain = gg_no_terrain_machine || gg_no_terrain_level; }
void GGSetNoTerrainLevel(int on) { gg_no_terrain_level   = (on != 0); gg_no_terrain = gg_no_terrain_machine || gg_no_terrain_level; }
void GGSetNoTrees       (int on) { gg_no_trees_machine   = (on != 0); gg_no_trees   = gg_no_trees_machine   || gg_no_trees_level;   }
void GGSetNoTreesLevel  (int on) { gg_no_trees_level     = (on != 0); gg_no_trees   = gg_no_trees_machine   || gg_no_trees_level;   }
void GGSetNoGrass       (int on) { gg_no_grass_machine   = (on != 0); gg_no_grass   = gg_no_grass_machine   || gg_no_grass_level;   }
void GGSetNoGrassLevel  (int on) { gg_no_grass_level     = (on != 0); gg_no_grass   = gg_no_grass_machine   || gg_no_grass_level;   }
void GGSetNoWater       (int on) { gg_no_water_machine   = (on != 0); gg_no_water   = gg_no_water_machine   || gg_no_water_level;   }
void GGSetNoWaterLevel  (int on) { gg_no_water_level     = (on != 0); gg_no_water   = gg_no_water_machine   || gg_no_water_level;   }

// GGMAX 3.08: round-2 low-spec switches. Same machine||level shape as the four above.
static bool gg_no_postfx_machine  = false, gg_no_postfx_level  = false;
static bool gg_no_ao_machine      = false, gg_no_ao_level      = false;
static bool gg_simple_sky_machine = false, gg_simple_sky_level = false;
static bool gg_no_shadows_machine = false, gg_no_shadows_level = false;
void GGSetNoPostFX      (int on) { gg_no_postfx_machine  = (on != 0); gg_no_postfx  = gg_no_postfx_machine  || gg_no_postfx_level;  }
void GGSetNoPostFXLevel (int on) { gg_no_postfx_level    = (on != 0); gg_no_postfx  = gg_no_postfx_machine  || gg_no_postfx_level;  }
void GGSetNoAO          (int on) { gg_no_ao_machine      = (on != 0); gg_no_ao      = gg_no_ao_machine      || gg_no_ao_level;      }
void GGSetNoAOLevel     (int on) { gg_no_ao_level        = (on != 0); gg_no_ao      = gg_no_ao_machine      || gg_no_ao_level;      }
void GGSetSimpleSky     (int on) { gg_simple_sky_machine = (on != 0); gg_simple_sky = gg_simple_sky_machine || gg_simple_sky_level; }
void GGSetSimpleSkyLevel(int on) { gg_simple_sky_level   = (on != 0); gg_simple_sky = gg_simple_sky_machine || gg_simple_sky_level; }
void GGSetNoShadows     (int on) { gg_no_shadows_machine = (on != 0); gg_no_shadows = gg_no_shadows_machine || gg_no_shadows_level; }
void GGSetNoShadowsLevel(int on) { gg_no_shadows_level   = (on != 0); gg_no_shadows = gg_no_shadows_machine || gg_no_shadows_level; }
static bool gg_no_occlusion_machine = false, gg_no_occlusion_level = false;
void GGSetNoOcclusion     (int on) { gg_no_occlusion_machine = (on != 0); gg_no_occlusion = gg_no_occlusion_machine || gg_no_occlusion_level; }
void GGSetNoOcclusionLevel(int on) { gg_no_occlusion_level   = (on != 0); gg_no_occlusion = gg_no_occlusion_machine || gg_no_occlusion_level; }
// GGMAX 3.09: percent, INT passthrough. Machine and level take the LOWER (more brutal) of the two.
static int gg_particle_pct_machine = 100, gg_particle_pct_level = 100;
static void GGRecalcParticlePct() { gg_particle_pct = (gg_particle_pct_machine < gg_particle_pct_level) ? gg_particle_pct_machine : gg_particle_pct_level; }
void GGSetParticlePct     (int pct) { if (pct < 0) pct = 0; if (pct > 100) pct = 100; gg_particle_pct_machine = pct; GGRecalcParticlePct(); }
void GGSetParticlePctLevel(int pct) { if (pct < 0) pct = 0; if (pct > 100) pct = 100; gg_particle_pct_level   = pct; GGRecalcParticlePct(); }
// GGMAX 3.116: Grass Density, percent of DX11's density (GGTerrainWicked.cpp). Machine (setup.ini
// grassdensity) and level (visuals.GrassDensity) MULTIPLY - unlike particle percent this one may go
// above 100, so taking the lower of the two would block a level that asks for more.
extern int gg_grass_density_pct; extern bool gg_grass_density_dx11;
static int gg_grass_density_machine = 100, gg_grass_density_level = 100;
static void GGRecalcGrassDensity() { int v = gg_grass_density_machine * gg_grass_density_level / 100; if (v < 5) v = 5; if (v > 400) v = 400; gg_grass_density_pct = v; }
static int  GGClampGrassPct(int pct) { if (pct < 10) pct = 10; if (pct > 200) pct = 200; return pct; }
void GGSetGrassDensity     (int pct) { gg_grass_density_machine = GGClampGrassPct(pct); GGRecalcGrassDensity(); }
void GGSetGrassDensityLevel(int pct) { gg_grass_density_level   = GGClampGrassPct(pct); GGRecalcGrassDensity(); }
int  GGGetGrassDensityLevel()   { return gg_grass_density_level; }
int  GGGetGrassDensityMachine() { return gg_grass_density_machine; }
void GGSetGrassDensityDX11(int on) { gg_grass_density_dx11 = (on != 0); }
// GGMAX 3.11: object detail distance in world inches. 0 = off. Machine and level take the
// LOWER NON-ZERO of the two (0 means "no opinion", not "cap at zero").
static float gg_objcull_machine = 0.0f, gg_objcull_level = 0.0f;
static void GGRecalcObjCull()
{
	if (gg_objcull_machine <= 0.0f)      gg_object_cull_dist = gg_objcull_level;
	else if (gg_objcull_level <= 0.0f)   gg_object_cull_dist = gg_objcull_machine;
	else gg_object_cull_dist = (gg_objcull_machine < gg_objcull_level) ? gg_objcull_machine : gg_objcull_level;
}
void GGSetObjectCullDist     (int units) { gg_objcull_machine = (units < 0) ? 0.0f : (float)units; GGRecalcObjCull(); }
void GGSetObjectCullDistLevel(int units) { gg_objcull_level   = (units < 0) ? 0.0f : (float)units; GGRecalcObjCull(); }

// GGMAX 3.12: global texture-detail divide. 1 = full, 2 = half, 4 = quarter. Lives in the ENGINE
// (wi::resourcemanager) because it acts inside the DDS loader.
//
// GGMAX 3.19: TWO entry points on purpose.
//   GGSetTextureDivide     - value only. This is the setup.ini path, and it runs before anything
//                            has loaded, so a live re-create would drain the GPU for nothing.
//   GGSetTextureDivideLive - value AND a request to re-create what is already resident. This is
//                            the panel and harness path, where the user is looking at a level and
//                            expects it to change. The request is only ARMED here; it is consumed
//                            on the main thread in GGApplyLowSpecSwitches, because it must not run
//                            inside the ImGui draw call that set it.
static int GGClampTextureDivide(int d)
{
	if (d < 1) d = 1;
	if (d > 4) d = 4;
	if (d == 3) d = 2;   // only 1/2/4 are meaningful (whole mip steps)
	return d;
}
void GGSetTextureDivide(int d)
{
	wi::resourcemanager::gg_texture_divide = GGClampTextureDivide(d);
}
void GGSetTextureDivideLive(int d)
{
	extern bool gg_texture_divide_pending;
	d = GGClampTextureDivide(d);
	if (wi::resourcemanager::gg_texture_divide == d) return;   // nothing to do, and no GPU drain
	wi::resourcemanager::gg_texture_divide = d;
	gg_texture_divide_pending = true;
}

// ★★★ GGMAX 3.35i: open a diagnostic/debug file BESIDE THE EXE, never in the current directory.
//
// Every DUMP_*/trace writer used a bare relative fopen, so the file landed wherever the process
// CWD happened to point. GameGuru sets the CWD to Files at startup, but editor paths and Windows
// FILE DIALOGS both move it - which is how a nested Max/Files/Files/ tree accumulated, and why
// screenshots and log.txt turned up in two different folders in the same session.
//
// Lee's call (2026-08-29): put them all in the root, so every AI-generated temp, script and debug
// file sits in one obvious set that can be deleted before shipping.
//
// ⚠ Deliberately NOT applied to setup.ini (Common_part3.cpp) - that is a PRODUCT file being
// read, not a diagnostic being written. The rule here is "debug output goes beside the exe", not
// "every relative path is a bug".
//
// Lives in this file rather than the harness because the harness is not in every build
// configuration and four of the callers are core game code.
FILE* GGDiagFopen(const char* name, const char* mode)
{
	return fopen(wi::helper::GetDiagnosticPath(name).c_str(), mode);
}

// GGMAX 3.05 DEBUG (not an off-switch): flat unlit grey billboard quads, see GGTreesConstants.hlsli.
void GGSetTreeDebugSolid(int on) { GGTrees::gg_tree_debug_solid = on; }   // INT passthrough: 0/1/2/3
// GGMAX 3.07: ini gives PERCENT (int passthrough), shader wants 0..1.
void GGSetTreeShadeWrap(int pct) { GGTrees::gg_tree_shade_wrap = pct * 0.01f; }

// GGMAX 2.71: the producelogfiles setup.ini key now also gates the ROUTINE diagnostic
// FILE writers (standalone exports write producelogfiles=0, the editor ships =1), so
// players' game folders stay clean. Detection/healing and the crash handlers
// (Guru-Crash.log, crashdump.dmp, dred_report.txt) stay live everywhere — only the
// trace files are gated. Called from GetSetupIniEarly (before engine init — the same
// ordering trap as lazypso) and again from FPSC_LoadSETUPINI's normal parse.
// File-scope namespace-qualified externs per the 2.53 linkage rule.
namespace wi::allocator { extern bool gg_alloc_tripwire_file; }   // engine wiAllocator.h (inline)
namespace wi::profiler { void gg_trace_file_enable(bool enable); } // engine wiProfiler.cpp
extern bool gg_anim_garbage_file;           // engine wiScene.cpp
extern bool gg_applytransform_garbage_file; // engine wiScene_Components.cpp
extern bool gg_videotrace_enabled;          // game CAnimation_part0.cpp
extern bool gg_reload_quiesce_file;         // game wickedcalls_part2.cpp
namespace GPUParticles { extern bool gg_gpup_trace_file; } // game GPUParticles_part0.cpp (whole file sits in this namespace)
void GGSetDiagTraceFiles(int on)
{
	const bool enable = (on != 0);
	wi::allocator::gg_alloc_tripwire_file = enable;
	wi::profiler::gg_trace_file_enable(enable);
	gg_anim_garbage_file = enable;
	gg_applytransform_garbage_file = enable;
	gg_videotrace_enabled = enable;
	gg_reload_quiesce_file = enable;
	GPUParticles::gg_gpup_trace_file = enable;
}

// GGMAX 2.89 (#157): setup.ini bridge for PROBE INSPECTION MODE (see wiRenderer.h SetProbeView
// and the SET_PROBEVIEW harness command). Both keys are INT PASSTHROUGH - a mode value must
// never be bool-ized (that class of bug cost a whole night on 08-17), and the two keys are held
// separately here so they can appear in the ini in either order without clobbering each other.
static int s_ggProbeViewMode = 0;
static float s_ggProbeViewMip = 0.0f;
void GGSetProbeViewIni(int v)
{
	s_ggProbeViewMode = v;
	wiRenderer::SetProbeView(s_ggProbeViewMode, s_ggProbeViewMip);
}
void GGSetProbeViewMipIni(int v)
{
	s_ggProbeViewMip = (float)v;
	wiRenderer::SetProbeView(s_ggProbeViewMode, s_ggProbeViewMip);
}
// GGMAX 2.89 (#157): setup.ini bridge for the env-probe parallax precision mode. INT
// PASSTHROUGH — 0/1/2 are three distinct modes, not a flag (see lightingHF.hlsli).
namespace wi::scene { extern int gg_probeparallax; } // engine wiScene.cpp:47
void GGSetProbeParallaxIni(int v)
{
	wi::scene::gg_probeparallax = v;
}

// GGMAX 1.82: lazy object PSOs — DEFAULT ON, this is the revert switch (setup.ini `lazypso=0`).
//
// TIMING, measured not assumed: the flag must be set before wi::renderer::LoadShaders builds the
// object pipelines, and setup.ini's main parse (FPSC_LoadSETUPINI) runs LATER than that — the
// 1.79 attempt wired it only there and it did nothing at all (pso_creates stayed at 7496,
// identical to the control). It is therefore read in GetSetupIniEarly(), which main() calls
// before the engine starts. Same reason there is no harness command for it: nothing a harness
// can send lands early enough. This ordering trap has now bitten three separate features.
void GGSetLazyPSO(int on)
{
	wi::renderer::gg_pso_lazy_object = (on != 0);
}

// GGMAX 2.08: hair/leaf DX11 parity — DEFAULT ON, this is the revert switch
// (setup.ini `hairnodepthwrite=0`). Unlike GGSetLazyPSO above there is NO ordering trap here:
// both pipeline permutations are built by LoadShaders either way and the flag is read at
// draw-call selection time, so it can also be flipped live (harness `SET_HAIRDEPTH 0|1`).
void GGSetHairNoDepthWrite(int on)
{
	wi::renderer::gg_transparent_doublesided_nodepthwrite = (on != 0);
}

// GGMAX 2.09: first-person weapon depth carve — DEFAULT ON.
//
// `setup.ini weaponforcedepth=0` is a TRUE revert: the flag is read both here (renderer selection)
// and in WickedCall_SetMeshDisableDepth (material setup), so the objects go back to plain opaque
// single-sided exactly as before 2.09. The harness `SET_WEAPONDEPTH 0|1` flips only the renderer
// half — gun materials are set up once at load — so it is an A/B lever for the carve itself, NOT a
// revert to the pre-2.09 baseline. Do not report a SET_WEAPONDEPTH 0 arm as "the old behaviour".
void GGSetWeaponForceDepth(int on)
{
	wi::renderer::gg_weapon_forcedepth = (on != 0);
}

// GGMAX 2.10: DX11 light power parity — DEFAULT ON, this is the revert switch
// (setup.ini `lightfalloff=0`). Fully live like SET_HAIRDEPTH: the shader branches on a
// per-frame FrameCB bit and lighting_loop re-pushes every light's intensity through
// WickedCall_UpdateLight each frame, so both halves follow the bool within a frame.
void GGSetDX11LightFalloff(int on)
{
	wi::renderer::gg_dx11_light_falloff = (on != 0);
}

// GGMAX 2.14: first-person weapon shadow-position pull — DEFAULT ON (DX11 SHADERTYPE_WEAPON
// parity, the second half of the 2.09 weapon work).
//
// FULLY LIVE, unlike SET_WEAPONDEPTH: the material bit that marks the weapon set is written from
// the existing GG_FORCEDEPTH flag when the gun loads, and this bool only drives a per-frame
// FrameCB bit the shader tests alongside it. So both `setup.ini weaponshadow=0` and the harness
// `SET_WEAPONSHADOW 0|1` land within a frame, and 0 IS the true pre-2.14 behaviour.
void GGSetWeaponShadow(int on)
{
	wi::renderer::gg_weapon_shadow = (on != 0);
}

// GGMAX 2.16: `setup.ini singlequeue=<0|1>`. Routes COMPUTE/COPY command lists onto the
// GRAPHICS queue and drops the same-queue fences that then become redundant, removing the
// cross-queue dependency bubble the editor frame stalls on (SWITCHESCAPE_PERF.md §8).
// Session-scoped equivalent: harness SET_SINGLEQUEUE. This exists so a project can hold the
// setting across launches. Read in main()'s early setup.ini pass because command lists start
// being begun on the first frame.
namespace wi::graphics { extern bool gg_single_queue; }
void GGSetSingleQueue(int on)
{
	wi::graphics::gg_single_queue = (on != 0);
}

// GGMAX 2.18: `setup.ini treepool=<N>` — tree-pool slot count (clamped to [1, GG_TREE_POOL_MAX]
// at build time). This MUST be an early-pass key, and that is the whole point of it existing:
// the pool is built exactly once per process inside GGTrees_WickedSetup, latched by
// g_wickedTreesSetup, and NOTHING clears that latch after startup — GGTrees_WickedInit runs
// only via GGTerrainWicked_Init (GameGuruMain.cpp init-sequence case 2) and
// GGTrees_WickedShutdown's only caller GGTerrainWicked_Shutdown has zero callers of its own.
// So the runtime harness `SET_TREES pool N` cannot shrink an already-built pool: it writes the
// same variable but arrives after the one read of it. That is why the 2026-08-09 "the tree pool
// costs nothing" A/B measured nothing at all — both arms ran the identical 6000 slots, and the
// unchanged SCENE_OBJECTS was the proof, not the refutation. See SWITCHESCAPE_PERF.md §2/§10.
// On Switch Escape 6000 of the 7322 objects and 6000 of the 8437 transforms ARE these slots.
namespace GGTrees { extern uint32_t g_treePoolSize; }
// GGMAX 3.73: driven from the terrain bridge below - the pool outlives the Wicked terrain.
namespace GGTrees { void GGTrees_WickedUpdate(); }
void GGSetTreePool(int n)
{
	if (n > 0) GGTrees::g_treePoolSize = (uint32_t)n;
}

// GGMAX 1.83: D3D12MA PreferredBlockSize override (setup.ini `mablockmb`, 0 = library default
// 64 MB). Same early-parse constraint as the two above, and a harder one — the allocator is
// created with the device, so nothing later than main()'s early pass can influence it.
void GGSetMABlockMB(int mb)
{
	extern int gg_ma_block_mb;              // wiGraphicsDevice_DX12.cpp, global namespace
	if (mb >= 0 && mb <= 1024) gg_ma_block_mb = mb;
}

// GGMAX: merged grass (setup.ini `grassmerge`, harness SET_GRASSMERGE). One hair system per
// terrain CHUNK instead of one per (chunk x painted type). Still DEFAULT OFF — it fails the
// TESTPRO1 density gate at coverage 10.96 vs 9.40, which is UNIFORM OVER-density (clumpCV is
// clean), not the clumping failure of the reverted 2026-08-01 attempt.
void GGSetGrassMerge(int on)
{
	extern bool gg_grass_merge;             // GGTerrainWicked.cpp
	gg_grass_merge = (on != 0);
}
void GGSetLowVRAMGrassDist(float inches)
{
	extern float gg_lowvram_grass_dist;     // GGTerrainWicked.cpp
	if (inches > 0.0f) gg_lowvram_grass_dist = inches;
}
void GGSetLowVRAMGrassDensity(float scale01)
{
	extern float gg_lowvram_grass_density;  // GGTerrainWicked.cpp
	if (scale01 > 0.0f && scale01 <= 1.0f) gg_lowvram_grass_density = scale01;
}

// GGMAX 1.85: bridge for the hair-entity destruction tracer (same namespace reason as below).
unsigned int GG_GetHairKillsBridge(const void** ret, unsigned int* entity, unsigned int* parent,
	unsigned int* reason, unsigned int* clearcount, unsigned int max_out)
{
	return wi::scene::GG_GetHairKills(ret, entity, parent, reason, clearcount, max_out);
}

// GGMAX 1.81: bridge for the Scene::Update caller tracer. Lives here because the harness is
// compiled outside the wi::scene namespace and a block-scope extern would mangle wrongly.
unsigned int GG_GetSceneUpdateCallsBridge(const void** ret, const void** scene,
	unsigned long long* frame, float* dt, unsigned int max_out)
{
	return wi::scene::GG_GetSceneUpdateCalls(ret, scene, frame, dt, max_out);
}

// GGMAX wall-gap tracer bridges for old-namespace files (main.cpp pump timing / master preamble)
unsigned long long GGPerf_TraceNowUs(void) { return wi::profiler::gg_trace_now_us(); }
void GGPerf_TraceMarkId(const char* prefix, unsigned int id) { wi::profiler::gg_trace_mark_id(prefix, id); }
void GGPerf_TraceMark(const char* name) { wi::profiler::gg_trace_mark(name); }
extern std::atomic<unsigned long long> gg_dbg_pump_dispatches, gg_dbg_pump_us; // wiProfiler.cpp, global namespace
void GGPerf_TracePumpAccum(unsigned long long us)
{
	gg_dbg_pump_dispatches.fetch_add(1, std::memory_order_relaxed);
	gg_dbg_pump_us.fetch_add(us, std::memory_order_relaxed);
}

namespace wi::scene { extern float gg_lod_step; } // the LOD distance step (wiScene.cpp), from the level's LOD Multiplier

// Phase 3: Forward declarations for GG custom draw functions (terrain/trees/grass)
extern "C" void GGTerrain_Draw_Prepass(const wi::primitive::Frustum*, wi::graphics::CommandList);
extern "C" void GGTerrain_Draw_Prepass_Reflections(const wi::primitive::Frustum*, wi::graphics::CommandList);
extern "C" void GGTerrain_Draw(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGTrees_Draw(const wi::primitive::Frustum*, int, wi::graphics::CommandList); // GGMAX 2.96 far-tree billboards
extern "C" void GGTrees_Draw_Prepass(const wi::primitive::Frustum*, int, wi::graphics::CommandList); // GGMAX 2.96
extern "C" void GGTerrain_Draw_Transparent(const wi::primitive::Frustum*, wi::graphics::CommandList);
extern "C" void GGTerrain_Draw_ShadowMap(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGTerrain_Draw_EnvProbe(const wi::primitive::Sphere*, const wi::primitive::Frustum*, uint32_t, wi::graphics::CommandList);
extern "C" void GGTrees_Draw_Prepass(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGTrees_Draw(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGTrees_Draw_ShadowMap(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGTrees_Draw_EnvProbe(const wi::primitive::Sphere*, const wi::primitive::Frustum*, uint32_t, wi::graphics::CommandList);
extern "C" void GGGrass_Draw_Prepass(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGGrass_Draw(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGGrass_Draw_ShadowMap(const wi::primitive::Frustum*, int, wi::graphics::CommandList);
extern "C" void GGTerrain_Draw_Debug(wi::graphics::CommandList);
extern "C" void GGTerrain_Draw_Overlay(wi::graphics::CommandList);
extern "C" void GGTerrain_VirtualTexReadBack(const wi::graphics::Texture&, uint32_t, wi::graphics::CommandList);

void MasterRenderer::Load()
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	__super::Load();

	// switch OFF JOLT (for now)
	wi::physics::SetEnabled(false);

	// remove VSYNC cap
	wiEvent::SetVSync( false );

	// clear image management system early
	char pCurrentDir[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, pCurrentDir);
	WickedCall_InitImageManagement(pCurrentDir);

	// default wicked settings
	setSSREnabled ( false ); //PE: Not on by default.
	setReflectionsEnabled ( true );
	setFXAAEnabled ( false ); //PE: We already have MSAA 8 so.
	#ifdef POSTPROCESSRAIN
	setRainEnabled(false); //PE: test post process shader.
	setRainTextures("Files\\effectbank\\common\\rain_color.png", "Files\\effectbank\\common\\rain_normal.png");
	setRainOpacity(0.0);
	setRainScaleX(1.0);
	setRainScaleY(1.0);
	setRainRefreactionScale(0.01);
	#endif
	#ifdef POSTPROCESSSNOW
	setSnowLayers(15.0);
	setSnowDepth(0.5);
	setSnowWindiness(0.5);
	setSnowSpeed(0.15);
	setSnowOpacity(1.0);
	setSnowOffset(0.0);
	#endif
	setBloomEnabled ( true ); 
	setShadowsEnabled ( true );
	wiRenderer::SetTessellationEnabled(false); //PE: Tessellation dont work like this it has to be set per mesh, so have never worked.
	setLightShaftsEnabled ( true );
	setLightShaftsStrength ( 0.18f ); // PORT TRANSLATE 2026-07-29: DX11 exposure parity (see Wicked_Update_Visuals)
	setBloomThreshold ( 2.0f );
	//setBloomStrength( 1.0f );

	// only activated when in TEST LEVEL
	setEyeAdaptionEnabled( false );

	// disable wicked backlog, can draw behind imgui , can be seen sometimes.
	if (wiBackLog::isActive()) 
	{
		wiBackLog::Toggle();
	}

	// for best terrain rendering
	wiGraphics::SamplerDesc desc = wiRenderer::GetSampler ( SAMPLER_OBJECTSHADER )->GetDesc ( );
	desc.filter = wiGraphics::Filter::ANISOTROPIC;

	// create cloudy sky by default
	Scene weatherscene;
	g_weatherEntityID = CreateEntity();
	auto& weather = weatherscene.weathers.Create ( g_weatherEntityID );
	weather.ambient = XMFLOAT3(0.5f, 0.5f, 0.5f);
	weather.horizon = XMFLOAT3 ( 0.38f, 0.38f, 0.38f );
	weather.zenith = XMFLOAT3 ( 0.42f, 0.42f, 0.42f );
	
	//PE: We dont want any lightshaft from the sun. before we activate it.
	weather.volumetricCloudParameters.layerFirst.coverageAmount = 0.0f;
	weather.volumetricCloudParameters.layerFirst.windSpeed = 0.0f;
	weather.fogStart = 0;
	weather.fogDensity = 0;
	weather.SetRealisticSky( true );

	weather.SetVolumetricClouds( true );

	wiScene::GetScene ( ).Merge ( weatherscene );

	// create directional light from sun
	Entity entitySunLight = wiScene::GetScene ( ).Entity_CreateLight ( "sunLight", XMFLOAT3 ( 0, 3, 0 ), XMFLOAT3 ( 1, 1, 1 ), 4, 600 );
	g_entitySunLight = entitySunLight;
	LightComponent* lightSun = wiScene::GetScene ( ).lights.GetComponent ( entitySunLight );
	lightSun->SetCastShadow ( true );
	lightSun->SetVisualizerEnabled ( false );
	lightSun->direction = XMFLOAT3 ( 0.25, -0.5, 0.25 );
	lightSun->color = XMFLOAT3 ( 1.0, 1.0, 1.0 );
	lightSun->SetType ( wiScene::LightComponent::DIRECTIONAL );
	// Sun volumetrics OFF (2026-07-18): the volumetric raymarch scales with fog
	// density, which was 0 until the fog wiring landed — so this has never been
	// visible. With fog active it renders god-rays tinted by TRANSPARENT
	// shadow casters, and the editor's translucent red/green zone markers
	// painted giant coloured beams across the sky. DX11 shows no sun shafts.
	// Key 8 (perf toggles) re-enables for experiments; if god-rays are wanted
	// later, first stop editor markers casting transparent shadows.
	lightSun->SetVolumetricsEnabled( false );
	// Production DX11 cascade splits in world inches (old engine GGREDUCED block);
	// stock Wicked default {8,80,800} is ~20m here. Kept in sync by
	// WickedCall_SetShadowRange whenever the level's visuals apply.
	lightSun->cascade_distances = { 380.0f, 950.0f, 7500.0f, 30000.0f, 500000.0f };

	// Production DX11 parity: staggered cascade refresh (c0 every frame, then
	// /2 /3 /4 /9) — skipped cascades keep their atlas contents + frozen
	// matrices; a >64" camera move or any atlas/rect change forces a refresh.
	// Wicked delta 1.11; the DELAYED_SHADOWS harness command A/Bs it live.
	// Respect the project's Delayed Shadows setting (Graphics & Performance panel): at HIGHEST
	// quality g_bDelayedShadows=false -> every-frame cascades -> stable shadows (no cliff-shadow
	// flicker under camera movement). M-Visuals apply + the checkbox keep this in sync live.
	extern bool g_bDelayedShadows;
	wi::renderer::SetDelayedShadowCascadesEnabled( g_bDelayedShadows );
	// Shadow LOD override renders each object into the shadow at a per-cascade projected-size LOD
	// (Wicked default ON) — for the terrain this picks a DIFFERENT, threshold-oscillating LOD mesh
	// than the visible chunk, so the cast shadow flips between two terrain shapes as the cascade
	// shifts (the author's own warning: "can result in shadow mismatch"). Tie it to the Delayed
	// Shadows setting: OFF (highest quality) = shadows use the stable main-view LOD, no flicker.
	wi::renderer::SetShadowLODOverrideEnabled( g_bDelayedShadows );

	// LB: sun needs lens flare texture
	int iFlareCount = 3;
	lightSun->lensFlareRimTextures.resize(iFlareCount);
	lightSun->lensFlareNames.resize(iFlareCount);
	for (int iFlareChain = 0; iFlareChain < iFlareCount; iFlareChain++)
	{
		std::string fileName;
		if (iFlareChain == 0) fileName = "Files\\lensflares\\flare1.jpg";
		if (iFlareChain == 1) fileName = "Files\\lensflares\\flare2.jpg";
		if (iFlareChain == 2) fileName = "Files\\lensflares\\flare3.jpg";
		lightSun->lensFlareRimTextures[iFlareChain] = wiResourceManager::Load(fileName);
		lightSun->lensFlareNames[iFlareChain] = fileName;
	}

	// force window to maximised view
	HWND hWnd = GetActiveWindow();
	ShowWindow(hWnd, SW_MAXIMIZE);

	// Phase 3: Set up custom scene draw callbacks
	// Terrain only (trees/grass disabled — see GRASSISSUE.md for details)
	// When ggterrain_use_wicked_terrain is active, all callbacks return early
	customDraw_Prepass = [](const Frustum* frustum, CommandList cmd) {
		// GGMAX 3.25: Terrain Bake depth prepass. ⚠ The compute dispatches that FILL the baked
		// textures are deliberately NOT here. This callback fires inside the prepass's
		// BeginRenderPass/EndRenderPass and DX12 forbids a Dispatch inside a render pass - doing
		// it here removed the device with DXGI_ERROR_INVALID_CALL on the first tick of the
		// switch. The bake takes its own command list on the main thread instead.
		GGTerrainBake_DrawPrepass(frustum, cmd);
		// GGMAX 3.114: far terrain prepass - its colour pass writes no depth either.
		GGTerrainFar_DrawPrepass(frustum, cmd, false);
		// GGMAX 2.96: the far-tree billboard PREPASS. Required, not optional - the main pass
		// runs depth_write_mask = ZERO and relies on this to lay the depth down.
		GGTrees_Draw_Prepass(frustum, 0, cmd);
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw_Prepass(frustum, cmd);
	};
	customDraw_Prepass_Reflections = [](const Frustum* frustum, CommandList cmd) {
		// GGMAX 3.114: the far terrain in the planar reflection (its mode-1 colour draw needs this depth)
		GGTerrainFar_DrawPrepass(frustum, cmd, true);
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw_Prepass_Reflections(frustum, cmd);
	};
	customDraw_Opaque = [](const Frustum* frustum, int mode, CommandList cmd) {
		// GGMAX 2.96: the distant-tree BILLBOARD pass runs on the shipping Wicked-terrain path,
		// so it must NOT sit behind ggterrain_use_wicked_terrain like the dead GGTerrain draw
		// below. Zero scene entities, one DrawIndexedInstanced per visible tree chunk, static
		// instance buffers the build already maintains. Self-gated on gg_far_tree_pass.
		// GGMAX 3.114: in the PLANAR REFLECTION the far terrain goes FIRST. The billboards and the
		// bake have no reflection prepass and their colour PSOs write no depth, so drawn after it the
		// far field (whose reflection prepass did lay depth) painted straight over them (review 3.114).
		// In the main view each has its own prepass, so the order there does not matter.
		if (mode != 0) GGTerrainFar_Draw(frustum, mode, cmd);
		GGTrees_Draw(frustum, mode, cmd);
		// GGMAX 3.25: the baked terrain, on the shipping Wicked-terrain path, so it must sit
		// ABOVE the early-out for the same reason the billboards do.
		GGTerrainBake_Draw(frustum, mode, cmd);   // GGMAX 3.115: mode, so the reflection skips the near detail
		// GGMAX 3.114: far terrain - the DX11 horizon beyond the live (or baked) ring.
		if (mode == 0) GGTerrainFar_Draw(frustum, mode, cmd);
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw(frustum, mode, cmd);
	};
	// GGMAX 3.120: the baked water seen from BELOW - drawn in the ocean's slot, before the transparent scene (engine
	// hook customDraw_WaterSurface, called only while the camera is under the water). Reads the engine camera CB like
	// GGWaterBake_Draw; the engine re-binds the camera CB and common resources right after it.
	customDraw_WaterSurface = [](const Frustum* frustum, CommandList cmd) {
		GGWaterBake_DrawUnder(frustum, cmd);
	};
	customDraw_Transparent = [](const Frustum* frustum, CommandList cmd) {
		// GGMAX 2026-08-07: legacy gpup/.arx particle draw (task #118 — steam columns etc.).
		// DX11's engine fork drove this from inside RenderPath3D::RenderTransparents
		// (WickedRepo RenderPath3D.cpp:2010/2027 + the per-object interleave in its
		// wiRenderer.cpp:3546); none of those hooks exist in the DX12 clone. This callback
		// fires at the equivalent point — inside the transparent render pass, after the
		// transparent scene draw, before DrawSoftParticles (wiRenderPath3D.cpp:2435) — so
		// gpup renders back-to-front among its own effects OVER the scene's transparents
		// (no per-object interleave; deliberate, see gpup_draw_bydistance's comment).
		// MUST run before the wicked-terrain early-out, same as the selection outline.
		// ★★★ GGMAX 3.25f: WATER BAKE MUST DRAW *BEFORE* gpup_draw. Two reasons, and the first
		// one is not a preference:
		//
		// (1) gpup_draw REBINDS b0 (FrameCB) and b1 (CameraCB) with its own per-emitter constants
		//     (GPUParticles_part0.cpp BindDynamicConstantBuffer slots 0 and 1) and never restores
		//     them. GGWaterBakeVS reads g_xCamera_VP / g_xCamera_CamPos, which ARE b1 - so run
		//     after gpup and the plane is transformed by particle constants and vanishes. It cost
		//     a long hunt because it is LEVEL-DEPENDENT: gpup only clobbers when an emitter is
		//     loaded and visible, so the plane rendered perfectly on a demo with no legacy
		//     emitters and produced nothing on a level that had them. Every counter said "drawn".
		//     The engine repairs the camera CB after the whole hook returns
		//     (wiRenderPath3D.cpp, BindCameraCB + BindCommonResources) - but nothing repairs it
		//     BETWEEN two draws inside the hook, which is where this sat.
		// (2) Ordering it this way is also just correct: particles (spray, steam, splashes) belong
		//     OVER the water surface, not under it.
		//
		// ⚠ Anything added here that consumes the engine camera CB must go ABOVE gpup_draw too, or
		// restore b0/b1 itself. GGTerrain_Draw_Transparent below has the same latent exposure; it
		// is editor-only debug geometry, which is the only reason it has never shown.
		GGWaterBake_Draw(frustum, cmd);
		// GGMAX 3.55: bullet tracers (Tracers::tracer_draw). DX11 called this from inside the
		// engine at WickedRepo RenderPath3D.cpp:2025 - after the DRAWSCENE_TRANSPARENT scene draw,
		// before gpup_draw_bydistance. The DX12 clone has no GGREDUCED hook layer at all, and the
		// port's work inventory (DX11_to_DX12_Shader_Porting_Plan.md 13.3) was built by grepping
		// GGTerrain/GGTrees/GGGrass, so every other hook in those same #ifdef blocks fell out of
		// scope silently - tracers sat fully implemented and orphaned for seven months.
		// Position: it computes its own WVP on the CPU and binds its own CB at b2, so unlike
		// GGWaterBake it does NOT read the engine camera CB and is immune to gpup's b0/b1 clobber.
		// It still must stay ABOVE the wicked-terrain early-out below or it never runs.
		Tracers::tracer_draw(wiScene::GetCamera(), cmd);
		GPUParticles::gpup_draw(wiScene::GetCamera(), cmd);
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw_Transparent(frustum, cmd);
	};
	wi::renderer::customDraw_ShadowMap = [](const Frustum* frustum, int cascade, CommandList cmd) {
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw_ShadowMap(frustum, cascade, cmd);
	};
	wi::renderer::customDraw_EnvProbe = [](const wi::primitive::Sphere* culler, const Frustum* frusta, uint32_t frustum_count, CommandList cmd) {
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw_EnvProbe(culler, frusta, frustum_count, cmd);
	};
	customDraw_Compose = [](CommandList cmd) {
		// GGMAX 2026-08-05: selection-outline composite. DX11 called this from
		// RenderPath2D::Compose just before the GUI (fork RenderPath2D.cpp:337); the
		// clone's customDraw_Compose fires at the same point in the frame
		// (wiRenderPath3D.cpp Compose, inside the compose render pass). Must run BEFORE
		// the wicked-terrain early-out - the outline is unrelated to which terrain
		// path is live.
		void Wicked_Render_Opaque_Scene(CommandList cmd);
		Wicked_Render_Opaque_Scene(cmd);
		if (ggterrain_use_wicked_terrain) return;
		GGTerrain_Draw_Debug(cmd);
		GGTerrain_Draw_Overlay(cmd);
	};
	// TODO: Enable when the prepass has a second RT for terrain page readback (SV_TARGET1).
	// Currently the prepass only has rtPrimitiveID_render at SV_TARGET0, so terrain page IDs
	// written to SV_TARGET1 are discarded. Reading rtPrimitiveID_render would produce garbage.
	// The CPU-based progressive page seeding (in GGTerrain_Update) provides the fallback.
	//customDraw_AfterPrepass = [](const Texture& texReadBack, uint32_t sampleCount, CommandList cmd) {
	//	GGTerrain_VirtualTexReadBack(texReadBack, sampleCount, cmd);
	//};

}

// GGMAX 3.25t: global-scope shim so M-Visuals can clear the animation-reduction
// posed-once set on level load without pulling wiScene's namespace into that file.
void gg_ResetAnimReductionBridge()
{
	wiScene::gg_ResetAnimReduction();
}

void MasterRenderer::PreUpdate()
{
	__super::PreUpdate();
}

void MasterRenderer::Update(float dt)
{
	// otherwise continue
	if (m_bRenderingVR == false)
	{
#ifdef OPTICK_ENABLE
		OPTICK_EVENT("GuruLoopLogic");
#endif
		// GGMAX 2026-08-07 (task #121): the 1/30s dt cap MOVED UP from below ("P6") so
		// gpup_update sees it too. DX11 capped deltaTime at 1/30 INSIDE the engine loop
		// (fork MainComponent.cpp:138, GGREDUCED) before ANY consumer; the DX12 engine
		// clamps at 0.5s (wiApplication.cpp) and the old cap position was AFTER the
		// gpup_update call — so a single hitch frame (lazy-PSO compile, SVT upload,
		// alt-tab) fed the legacy particle sim up to 0.5s = a 33-quantum warp in ONE
		// tick: particles leapt, mass-aged (bigger via size-over-life) and wrapped
		// lifetimes = the user-reported "particles reset a little every so often" and
		// the balloooning steam column. Nothing between here and the old position
		// consumes dt except gpup_update (terrain/tree/grass updates take camera+cmd).
		if (dt > (1.0f / 30.0f)) dt = 1.0f / 30.0f;

		// regular update mode
		auto range = wiProfiler::BeginRangeCPU("Update - Logic (Total)");
		bool bFullyInitialised = GuruLoopLogic();
		wiProfiler::EndRange(range);
		GGPerf_TraceMark("logic"); // GGMAX 2.61 gap decomposition

		// no further than logic while in splash show mode
		// DX12: Must NOT return early — __super::Update(dt) must always run so
		// RenderPath3D prepares render targets before Render()/Compose() are called.
		if (g_iShowSplashForFirstFewCycles <= 0 && bFullyInitialised == true )
		{
			// normal update
			wiScene::CameraComponent &camera = wiScene::GetCamera();

			// GGMAX 3.08: re-assert the low-spec off-switches every frame. Deliberately here and not
			// in GGTerrainWicked_Update - that one is gated on both bEnableEmptyLevelMode and
			// ggterrain_use_wicked_terrain, so an indoor or legacy-terrain level would never see it.
			extern void GGApplyLowSpecSwitches();
			GGApplyLowSpecSwitches();
			// GGMAX 3.25: Terrain Bake state machine. MAIN THREAD only - it creates GPU
			// resources, which must never happen inside a render callback.
			GGTerrainBake_Update();
			GGWaterBake_Update();

			// must be outside a render pass and only called once, even if VR renders twice
			CommandList cmd = wiGraphics::GetDevice()->BeginCommandList();
			auto range = wiProfiler::BeginRangeCPU("Update - Particles");
			gpup_update(dt, cmd);
			wiProfiler::EndRange(range);
			GGPerf_TraceMark("gpup"); // GGMAX 2.61

			// terrain processing (if used)
			// GGMAX 2.68i (Lee's ssss10 repro, the REAL mechanism at last): empty mode skips
			// the whole terrain block below — which is also where the wicked branch clears
			// ggterrain_draw_enabled EVERY frame. Skipped, the LEGACY terrain draw callbacks
			// run with a stale flag (default 1 on a fresh launch) and render the old-path
			// terrain — the "grid is back" was never wicked chunks (terrain-mask pick misses;
			// hidden=1 all along), it is the legacy render showing through, flat at the empty
			// biome's height in legacy textures. And with GGTerrainWicked_Update skipped, the
			// 2.68f/g newborn-chunk sweep never ran here either (engine-side Generation_Update
			// still births renderable chunks from Scene::Update). Keep BOTH paths dead:
			if (t.visuals.bEnableEmptyLevelMode == true)
			{
				ggterrain_draw_enabled = 0;
				GGTerrainWicked_EnforceHidden();
			}
			// GGMAX 3.47 (DX11 2224088a "No terrain update in standalone saver"): skip all
			// terrain/tree/grass activity while the standalone saver window is running.
			extern bool bExport_Standalone_Window;
			if (bExport_Standalone_Window == false)
			{
				if (t.visuals.bEnableEmptyLevelMode == false)
				{
					extern int g_iDisableTerrainSystem;
					auto range3 = wiProfiler::BeginRangeCPU("Update - Terrain");
					extern bool bImGuiRenderTargetFocus;
					auto rangeT1 = wiProfiler::BeginRangeCPU("Terrain - GG Core");
					GGTerrain_Update(camera.Eye.x, camera.Eye.y, camera.Eye.z, cmd, bImGuiRenderTargetFocus);
					wiProfiler::EndRange(rangeT1);
					GGPerf_TraceMark("ggcore"); // GGMAX 2.61
					if (ggterrain_use_wicked_terrain)
					{
						ggterrain_draw_enabled = 0;  // suppress all old draw callbacks
						auto rangeT2 = wiProfiler::BeginRangeCPU("Terrain - Wicked Bridge");
						GGTerrainWicked_Update(camera);
						// ★ GGMAX 3.73: the tree pool is NOT part of the terrain, and now it is not nested
						// inside the terrain's update either. It lived at the bottom of
						// GGTerrainWicked_Update, which returns early whenever gg_no_terrain is set - so
						// ticking Terrain Bake both tore the pool down AND removed the one thing that could
						// have rebuilt it. Near 3D trees vanished; only the far billboards were left.
						// Kept inside rangeT2 and in the same frame position so the profiler tree and the
						// update order are byte-for-byte what they were.
						{
							auto rangeTP = wiProfiler::BeginRangeCPU("TerrainW - Tree Pool");
							GGTrees::GGTrees_WickedUpdate();
							wiProfiler::EndRange(rangeTP);
						}
						// GGMAX 3.114: far terrain - only FLAGGED here (this block ran: not an empty level,
						// not the standalone saver). Beside the tree pool and NOT inside GGTerrainWicked_Update,
						// which returns early forever under a Terrain Bake (the 3.73 lesson). The mask itself
						// is built after __super::Update below, from the chunk set Scene::Update leaves.
						GGTerrainFar_RequestUpdate();
						wiProfiler::EndRange(rangeT2);
						GGPerf_TraceMark("ggbridge"); // GGMAX 2.61
					}
					else
					{
						ggterrain_draw_enabled = 1;
					}
					if (g_iDisableTerrainSystem == 0)
					{
						GGTrees_Update(camera.Eye.x, camera.Eye.y, camera.Eye.z, cmd, bImGuiRenderTargetFocus);
						auto rangeT3 = wiProfiler::BeginRangeCPU("Trees - FrustumCull");
						GGTrees_UpdateFrustumCulling(&camera);
						wiProfiler::EndRange(rangeT3);
						auto rangeT4 = wiProfiler::BeginRangeCPU("Grass - GG Update");
						GGGrass_Update(&camera, cmd, bImGuiRenderTargetFocus);
						wiProfiler::EndRange(rangeT4);
					}
					wiProfiler::EndRange(range3);
					GGPerf_TraceMark("trees-grass"); // GGMAX 2.61
				}
				else
				{
					// still need for terrain globals to update local params (for editable_size reading)
					GGTerrain_Update_EmptyLevel(camera.Eye.x, camera.Eye.y, camera.Eye.z, cmd);
				}
			}
			
#ifdef WICKEDPARTICLESYSTEM
			auto range4 = wiProfiler::BeginRangeCPU("Update - Emitters");
			WickedCall_UpdateEmitters();
			wiProfiler::EndRange(range4);
#endif

      // now just prepared IMGUI, but actual render called from Wicked hook
			auto range2 = wiProfiler::BeginRangeCPU("Update - Render");
			GuruLoopRender();
			wiProfiler::EndRange(range2);
			GGPerf_TraceMark("loop-render"); // GGMAX 2.61
		}
	}

	//Disable wicked backlog, can draw behind imgui , can be seen sometimes. Make sure it is never activated.
	if (wiBackLog::isActive()) wiBackLog::Toggle();

	// P6: cap delta time to 1/30s to prevent animation jumps after alt-tab or stalls
	// GGMAX 2026-08-07: cap MOVED to the top of this function (see task #121 comment) so the
	// legacy gpup particle sim is covered as well; kept here as a no-op re-assert for safety.
	if (dt > (1.0f / 30.0f)) dt = 1.0f / 30.0f;

	// animation bridge pre-hook (before scene->Update runs animations)
	////GGAnimBridge_PreUpdate(&wiScene::GetScene(), dt);   // removed 89873913 (too slow); throttle now lives in the engine
	// GGMAX delta 1.29: drive the engine 30fps animation throttle from the editor "Lower Animation & LUA
	// Speed" checkbox. Set BEFORE __super::Update (which runs the animation jobs). Parity counter flips each
	// frame for a deterministic every-other-frame skip. g_animThrottleFarDist = distance (scene units) beyond
	// which to throttle; 0 = throttle all eligible armatures ((b) behaviour), >0 = only distant ((c) gate).
	{
		extern bool bEnable30FpsAnimations;
		extern float g_animThrottleFarDist;
		// ★★★ GGMAX 3.94: THE OBJECT LIBRARY LIVE PREVIEW IS EXEMPT FROM EVERY ANIMATION
		// THROTTLE WHILE IT IS ON SCREEN.
		//
		// The preview PARKS its object ~39,000 units above the editor camera (master_part2.cpp:285)
		// and every throttle here measures distance from the EDITOR camera. Reduction Scale's
		// period is 1 + (scale*0.1)*(distance/1000), so at the shipped default scale 25 the preview
		// character updated once every 98 frames - 1.6 s per pose - and at scale 100 it hit the
		// 240-frame clamp, four seconds. Lee: hovering a character 'no longer produces animation'.
		//
		// ★ NOT caused by 3.93, EXPOSED by it. Before 3.93 the character's parts sat on different
		// phases, so something moved most frames and it LOOKED animated while being internally
		// incoherent. 3.93 gave them one shared phase, so they freeze together and it is visibly
		// stopped. The throttle was always wrong here; full rate is what the preview always needed.
		//
		// ★★ FIFTH INSTANCE of the rule this feature keeps teaching: A PREVIEW IS ITS OWN SCENE,
		// AND EVERY PARAMETER IT INHERITS FROM THE LEVEL CAN BE WRONG FOR IT. Previously exposure,
		// near/far planes, scissor and blend mode (3.83) and field of view (3.84). The distance that
		// matters for the preview is to the PREVIEW camera (~300 units), not the editor camera.
		//
		// Gated HERE and not with a per-armature exemption list, deliberately: this block is the one
		// place both knobs are published, it runs inside MasterRenderer::Update BEFORE
		// __super::Update(dt) and therefore before the animation jobs, and it stores NOTHING per
		// object - so the exemption CANNOT LEAK. There is no path where a preview ends and a
		// character is left permanently un-throttled, which a per-entity list would have risked.
		//
		// ⚠ LOAD-BEARING: redScale must be zeroed too, not just the 30fps flag. The CPU animation
		// skip is nested under the 30fps flag, but the GPU skinning dispatch skip and the 3.92
		// streamout ping-pong gate both read gg_anim_reduction_scale directly - clearing only the
		// 30fps flag would leave the preview posed on the CPU and skinned from a stale buffer, which
		// looks WORSE, not better.
		extern bool GGObjectPreview_IsActive(void);
		const bool bGGPreviewLive = GGObjectPreview_IsActive();
		wiScene::gg_anim30fps_enabled.store((bEnable30FpsAnimations && !bGGPreviewLive) ? 1u : 0u, std::memory_order_relaxed);
		static uint32_t s_ggAnimFrame = 0;
		wiScene::gg_anim30fps_frame.store(++s_ggAnimFrame, std::memory_order_relaxed);
		float fd = g_animThrottleFarDist;
		if (fd < 0.0f) fd = 0.0f;
		if (fd > 60000.0f) fd = 60000.0f;   // guard: dist^2 must fit uint32 (60000^2 = 3.6e9 < 4.29e9)
		wiScene::gg_anim30fps_dist2.store((uint32_t)(fd * fd), std::memory_order_relaxed);

		// GGMAX 3.25: Reduction Scale. A SUB-CONTROL of the tick box above, so it only bites
		// while that is ticked - which is also what makes it safe to leave a level saved with a
		// scale set: untick the box and everything returns to full rate without touching the
		// slider. Drives BOTH the CPU animation skip and the skinning dispatch skip; see the
		// note by gg_anim_reduction_scale in wiScene.cpp.
		uint32_t redScale = 0;
		if (bEnable30FpsAnimations && !bGGPreviewLive)   // GGMAX 3.94, see the note above
		{
			int rs = t.visuals.iAnimReductionScale;
			if (rs < 1) rs = 1;
			if (rs > 100) rs = 100;
			redScale = (uint32_t)rs;
		}
		wiScene::gg_anim_reduction_scale.store(redScale, std::memory_order_relaxed);

		// GGMAX 3.28: push the Occlusion Cull Delay to the engine as a bitmask. Per-level like the
		// other performance settings, so a level author can dial it for their own content.
		{
			extern uint32_t gg_occlusion_history_mask;
			int occd = t.visuals.iOcclusionCullDelay;
			if (occd < 1 || occd > 32) occd = 8;   // a level saved before 3.28 parses as 0
			wiScene::gg_occlusion_history_mask = (occd >= 32) ? 0xFFFFFFFFu : ((1u << occd) - 1u);
		}

		// GGMAX 3.29: Shadow Detail Steps -> the shadow-atlas resolution quantiser. Per-level like
		// the rest of the performance settings.
		{
			extern int gg_shadow_res_steps;
			int shs = t.visuals.iShadowResSteps;
			if (shs < 0 || shs > 16) shs = 4;   // a level saved before 3.29 parses as 0
			wi::renderer::gg_shadow_res_steps = shs;
		}

		// ★★ GGMAX 3.89: Water Reflection Size -> the planar reflection target's width.
		//
		// THE SINGLE APPLY POINT, deliberately. The panel, the harness verb, Reset Visuals and
		// the visuals.ini parser all write only the struct field; this block is the one place
		// that reaches the engine. That is what makes Test Game, Play Game and the editorvisuals
		// restore work for free - they are WHOLE-STRUCT assignments (M-GridEdit_part2.cpp:1041,
		// :1733, :1738) that no per-field copy list knows about.
		//
		// NOT in ResizeBuffers: that only runs on a window or resolution change, so the slider
		// would appear to do nothing - the exact complaint Texture Detail drew in 3.12/3.19.
		{
			int rw = t.visuals.iReflectionWidth;
			if (rw != 0 && (rw < 128 || rw > 2048)) rw = 0;   // pre-3.89 levels parse as 0 = Auto
			if (wi::renderer::gg_reflection_width != rw)
			{
				wi::renderer::gg_reflection_width = rw;
				if (getReflectionsEnabled())
				{
					// ⚠ These two are built LAZILY in RenderPath3D::PreRender and ONLY WHEN INVALID,
					// so they do not follow a re-create on their own - they would keep running at the
					// old size against a differently sized reflection buffer. setReflectionsEnabled's
					// off-branch clears only rtReflection, depthBuffer_Reflection and the tiled light
					// resources, so an off/on toggle does not do it either. Null them here.
					// (A window resize is already safe: ResizeBuffers calls DeleteGPUResources, which
					// does clear both.)
					aerialperspectiveResources_reflection = {};
					volumetriccloudResources_reflection = {};
					setReflectionsEnabled(true);
				}
				// Reflections off: nothing to re-create. The global is set, so the engine builds at
				// the new size whenever the checkbox turns them back on.
			}
		}

		// GGMAX 3.90: Water Reflection Blur. A BARE assignment on purpose - unlike 3.89 there is
		// nothing to re-create here, because the blur temp is created in setReflectionsEnabled
		// beside the target it tracks, so it already follows the size.
		{
			int rb = t.visuals.iReflectionBlur;
			if (rb < 0 || rb > 3) rb = 0;   // pre-3.90 levels parse as 0 = off
			wi::renderer::gg_reflection_blur = rb;
		}

		// LOD Multiplier -> the engine's LOD distance step (the DX11 rule, 200 units a step), and Reflections Use
		// Fastest LOD -> the planar reflection passes at the last LOD. Both globals are already kept in step with
		// the visuals (M-GridEditB_part3.cpp) and the Lua render settings.
		{
			extern float fLODMultiplier;
			extern bool bReflectionsLowestLOD;
			wi::scene::gg_lod_step = 200.0f * fLODMultiplier;
			wi::renderer::gg_reflection_lowest_lod = bReflectionsLowestLOD ? 1 : 0;
		}

		// ★★★ GGMAX 3.34: Super Quick Objects -> which rung of the cut-down opaque shader.
		//
		// 3.31 pushed a bare 0/1 and all it ever did was collapse exotic material permutations
		// onto base PBR - a no-op on a scene whose materials are already PBR, which is why Lee
		// measured no change at all on the AMD card. The value now carries the RUNG:
		//   1 Flat     no texture fetch of any kind
		//   2 Ambient  + albedo
		//   3 Lit      + tiled lighting and shadows
		// ⚠ A level saved before 3.34 has no SuperQuickLevel field and parses as 0, which would
		// read as OFF and quietly disable a box the user had ticked. Clamp it to the default, which
		// 3.35b moved from 1 (Flat) to 3 (Lit) - an untextured world is not what a user expects to
		// get for ticking a performance box.
		{
			int sql = t.visuals.iSuperQuickLevel;
			if (sql < 1 || sql > 3) sql = 3;   // GGMAX 3.35b: default is now Lit, not Flat
			wi::renderer::gg_super_quick_objects = t.visuals.bSuperQuickObjects ? sql : 0;
		}

		// ★★★ GGMAX 3.27: continuous camera spin, for measuring rotation-dependent cost.
		//
		// Diagnostic only - inert unless SPIN_CAMERA has been sent, which nothing in the product
		// does. It exists because the cost Lee is chasing only appears WHILE the camera is moving,
		// and a file-based harness cannot hold a camera in motion across a sample.
		{
			extern float gg_spin_deg_per_sec;
			// ⚠ Sample the clock EVERY frame, not only while spinning. If it were only read while
			// active, the first frame after starting a spin would see the whole idle period as its
			// delta and jump the camera. Same shape of bug as the 3.25w grace countdown.
			static std::chrono::steady_clock::time_point s_ggSpinLast = std::chrono::steady_clock::now();
			const std::chrono::steady_clock::time_point ggSpinNow = std::chrono::steady_clock::now();
			float ggSpinDt = std::chrono::duration<float>(ggSpinNow - s_ggSpinLast).count();
			s_ggSpinLast = ggSpinNow;
			if (ggSpinDt < 0.0f)  ggSpinDt = 0.0f;
			if (ggSpinDt > 0.25f) ggSpinDt = 0.25f;
			// GGMAX 3.29: translation oscillation, for the shadow work. SPIN only changes the camera's
			// ORIENTATION, and the shadow atlas sizes off DISTANCE - so a spin cannot reproduce this
			// one at all. Runs back and forth along the facing direction like Lee running corner to
			// corner in the starting room.
			extern float gg_move_units_per_sec;
			extern float gg_move_range;
			extern float gg_move_anchor_x, gg_move_anchor_z;
			static float s_ggMoveDist = 0.0f;
			if (gg_move_units_per_sec != 0.0f)
			{
				s_ggMoveDist += gg_move_units_per_sec * ggSpinDt;
				const float r = (gg_move_range > 1.0f) ? gg_move_range : 1.0f;
				float p = fmodf(s_ggMoveDist, 2.0f * r);
				if (p < 0.0f) p += 2.0f * r;
				const float off = (p < r) ? p : (2.0f * r - p);   // triangle wave: 0 -> r -> 0
				const float yaw = t.editorfreeflight.c.angy_f * 0.01745329252f;
				t.editorfreeflight.mode = 1;
				t.editorfreeflight.c.x_f = gg_move_anchor_x + sinf(yaw) * off;
				t.editorfreeflight.c.z_f = gg_move_anchor_z + cosf(yaw) * off;
			}
			else
			{
				s_ggMoveDist = 0.0f;
			}

			if (gg_spin_deg_per_sec != 0.0f)
			{
				t.editorfreeflight.mode = 1;   // free-flight applies these fields every frame
				float ggYaw = t.editorfreeflight.c.angy_f + gg_spin_deg_per_sec * ggSpinDt;
				while (ggYaw >= 360.0f) ggYaw -= 360.0f;
				while (ggYaw <    0.0f) ggYaw += 360.0f;
				t.editorfreeflight.c.angy_f = ggYaw;
			}
		}

		// GGMAX 3.25v: count down the post-load grace, and re-arm it whenever we cross into or
		// out of the test game. Level loads are covered by the visuals reset; entering test game
		// is NOT a level load, and it spawns characters and starts their scripts - exactly the
		// window Lee saw flicker in. Both edges, because leaving the game rebuilds the editor
		// view from the same content.
		// ★ A DEADLINE, NOT AN ACCUMULATED DELTA.
		//
		// Two wrong versions preceded this one and both under-counted, which is the dangerous
		// direction - the grace silently lasted longer than advertised and kept the feature
		// switched off. First I subtracted the Update() dt, which is not wall-clock on this path
		// (measured 10.0 s decaying to 3.1 s over 16 real seconds). Then I subtracted a real
		// steady_clock delta but clamped each step to 0.25 s so a load hitch could not burn the
		// grace - and the clamp swallowed every gap longer than that, giving 9.7 s -> 1.8 s over
		// 16 seconds. Accumulating deltas is simply the wrong shape: it depends on how often this
		// code runs and on every clamp in the chain.
		//
		// A deadline depends on neither. Re-arm sets it once; every frame just reports the
		// remainder. "Ten seconds after the level loads" is a statement about the clock on the
		// wall, so read the clock on the wall.
		{
			static uint32_t s_ggSeenResets = 0xFFFFFFFFu;
			static std::chrono::steady_clock::time_point s_ggGraceDeadline;
			if (s_ggSeenResets != wiScene::gg_anim_reduction_resets)
			{
				s_ggSeenResets = wiScene::gg_anim_reduction_resets;
				s_ggGraceDeadline = std::chrono::steady_clock::now()
					+ std::chrono::milliseconds((int)(wiScene::gg_anim_reduction_grace_seconds * 1000.0f));
			}
			const float ggLeft = std::chrono::duration<float>(
				s_ggGraceDeadline - std::chrono::steady_clock::now()).count();
			wiScene::gg_anim_reduction_grace = (ggLeft > 0.0f) ? ggLeft : 0.0f;
		}
		{
			extern bool bImGuiInTestGame;
			static bool s_ggPrevInTestGame = false;
			if (bImGuiInTestGame != s_ggPrevInTestGame)
			{
				s_ggPrevInTestGame = bImGuiInTestGame;
				wiScene::gg_ResetAnimReduction();
			}
		}
	}

	// GGMAX delta 1.35: frustum-visibility animation pause — drive the engine knobs per frame.
	// Objects not passing the main-view cull for N frames stop animation EVALUATION (timers keep
	// advancing). Near guard protects just-off-frame characters (visible shadows).
	{
		extern int g_animVisPauseFrames;
		extern float g_animVisPauseNearDist;
		// ★ GGMAX 3.94b: the live preview is exempt from this one too.
		//
		// It does NOT bite today - but only by ACCIDENT. Its 2000-unit near guard cannot protect an
		// object parked 39,000 units away; what saves it is that RenderCameraComponents runs a full
		// UpdateVisibility for the PREVIEW camera each frame, which re-stamps gg_last_visible_frame
		// (wiRenderer.cpp:4777). So the preview is immune as a SIDE EFFECT of an unrelated pass, and
		// the first frames of a hover - before that pass has run once - are unverified.
		//
		// ★★ An immunity you get by accident is not a property you have. Lee's instruction was
		// categorical - 'any delayed or lowered animation stuff should be switched off' - and this
		// costs one condition, so cover it rather than depend on a coincidence holding.
		extern bool GGObjectPreview_IsActive(void);
		uint32_t vp = (g_animVisPauseFrames < 0 || GGObjectPreview_IsActive()) ? 0u : (uint32_t)g_animVisPauseFrames;
		wiScene::gg_anim_vis_pause_frames.store(vp, std::memory_order_relaxed);
		float nd = g_animVisPauseNearDist;
		if (nd < 0.0f) nd = 0.0f;
		if (nd > 60000.0f) nd = 60000.0f;   // same uint32 dist^2 guard as above
		wiScene::gg_anim_vis_pause_neardist2.store((uint32_t)(nd * nd), std::memory_order_relaxed);
	}

	// GGMAX delta 1.30: drive the engine apparent-size object cull from the editor "Apparent Size" slider.
	// maxApparentSize is the stored slider value (0.000002..0.0002, default 0.000008). Map the amount ABOVE
	// the default to a radius/distance tangent cutoff (at/below default => 0 = draw everything; slide right
	// to cull distant specks). g_apparentCullDirect >= 0 overrides for harness tuning. Stored as fixed-point
	// micro-units (tangent * 1e6) in the uint32 atomic the engine reads.
	{
		extern float maxApparentSize;
		extern float g_apparentCullK;
		extern float g_apparentCullDirect;
		float tangent;
		if (g_apparentCullDirect >= 0.0f)
		{
			tangent = g_apparentCullDirect;
		}
		else
		{
			const float over = maxApparentSize - 0.000008f;   // amount past the default
			tangent = (over > 0.0f) ? over * g_apparentCullK : 0.0f;
		}
		if (tangent < 0.0f) tangent = 0.0f;
		if (tangent > 0.5f) tangent = 0.5f;   // clamp: never cull objects filling more than ~half the view
		wiRenderer::gg_apparent_cull_bits.store((uint32_t)(tangent * 1000000.0f + 0.5f), std::memory_order_relaxed);
	}

	// GGMAX: "Laptop" delayed-shadow mode = twice as aggressive. When Delayed Shadows + Laptop are both
	// ticked, hold the far directional cascades for 4 frames instead of 2 (another ~halving of the
	// staggered-cascade shadow cost). Driven per-frame so panel toggles / level loads all stay in sync;
	// interval is only consumed when the cascade staggering is actually enabled.
	{
		extern bool g_bDelayedShadows;
		extern bool g_bDelayedShadowsLaptop;
		wi::renderer::SetDelayedShadowCascadeInterval((g_bDelayedShadows && g_bDelayedShadowsLaptop) ? 4 : 2);
	}

	// super update
	auto range2 = wiProfiler::BeginRangeCPU("Update - Wicked (Total)");
	GGPerf_TraceMark("pre-wicked"); // GGMAX 2.61
	__super::Update(dt);
	wiProfiler::EndRange(range2);
	GGPerf_TraceMark("wicked-upd"); // GGMAX 2.61

	// GGMAX 3.114: far terrain coverage + constants + chunk list, AFTER Scene::Update: its own
	// Generation_Update merges and removes terrain chunks, and the mask must describe the chunks that
	// are actually rendered this frame. Inactive unless the terrain block flagged it above.
	{
		auto rangeFar = wiProfiler::BeginRangeCPU("Terrain - Far Field");
		GGTerrainFar_Update();
		wiProfiler::EndRange(rangeFar);
	}
}

void MasterRenderer::PostUpdate()
{
	////GGAnimBridge_PostUpdate(&wiScene::GetScene());
	__super::PostUpdate();
}

void MasterRenderer::ResizeBuffers(void)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	if ( GetInternalResolution().x == 0 || GetInternalResolution().y == 0 ) return;


	//PE: Must be the same, currently its out of synch with internal resolution.
	//PE: Cant change if in VR that have another resolution.
	if (!m_bUsingVR)
	{
		// TODO: Set3DResolution removed, use resolutionScale instead
		//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false); //GGREDUCED
	}

	//PE: Resizebuffers change FOV.

	__super::ResizeBuffers();

	GraphicsDevice* device = wiGraphics::GetDevice();
	HRESULT hr;

	if (GetDepthStencil() != nullptr)
	{
		TextureDesc desc;
		// GGREDUCED DX12/FSR: these outline RTs are bound in renderpass_Outline together with the
		// internal-res main depth (*GetDepthStencil()). When FSR lowers resolutionScale<1.0 the depth is
		// smaller than physical, so sizing the RTs at physical res mismatches the pass dimensions (D3D12
		// DEVICE_DRAW_VIEW_DIMENSION_MISMATCH). Size to internal res instead; Postprocess_Outline later
		// samples + composites them full-screen, so a smaller source simply upscales.
		desc.width = GetInternalResolution().x;
		desc.height = GetInternalResolution().y;
		desc.sample_count = 1;
		desc.format = Format::R8_UNORM;
		desc.bind_flags = BindFlag::RENDER_TARGET | BindFlag::SHADER_RESOURCE;

		//PE: Get the below error when using dx11 debug layer, so switched to resolving MSAA instead.
		//D3D11 ERROR: ID3D11DeviceContext::Draw: The Shader Resource View dimension declared in the shader code (TEXTURE2D)
		//             does not match the view type bound to slot 23 of the Pixel Shader unit (TEXTURE2DMS).
		//             This mismatch is invalid if the shader actually uses the view (e.g. it is not skipped due to shader code branching).
		//             [ EXECUTION ERROR #354: DEVICE_DRAW_VIEW_DIMENSION_MISMATCH]

		hr = device->CreateTexture(&desc, nullptr, &rt_Outline);
		hr = device->CreateTexture(&desc, nullptr, &rt_Outline_Red);
		hr = device->CreateTexture(&desc, nullptr, &rt_Outline_Blue);
		if (getMSAASampleCount() > 1)
		{
			desc.sample_count = getMSAASampleCount();
			hr = device->CreateTexture(&desc, nullptr, &rt_MSAAOutline);
			hr = device->CreateTexture(&desc, nullptr, &rt_MSAAOutline_Red);
			hr = device->CreateTexture(&desc, nullptr, &rt_MSAAOutline_Blue);
		}
		else
		{
			// GGMAX 3.68: these three only exist while MSAA is on, and nothing released them when
			// it went off - ResizeBuffers simply skipped the branch and left ~12 MB of multisampled
			// R8 targets resident for the rest of the process. Safe to drop here because the three
			// outline RenderPasses are rebuilt unconditionally a few lines below and, with MSAA off,
			// reference only the single-sample rt_Outline* - RenderPassAttachment holds its Texture
			// BY VALUE, so a pass still holding one of these would otherwise keep it alive (the trap
			// that made the 3.65 gpup fix only half a fix).
			rt_MSAAOutline = wiGraphics::Texture();
			rt_MSAAOutline_Red = wiGraphics::Texture();
			rt_MSAAOutline_Blue = wiGraphics::Texture();
		}
		assert(SUCCEEDED(hr));
	}

	{
		RenderPassDesc desc;

		//PE: New wickedrepo.
		desc.attachments.push_back(RenderPassAttachment::RenderTarget(rt_Outline, RenderPassAttachment::LoadOp::CLEAR));

		//PE: We now use the MSAA desc.sample_count so dont need to resolve it.
		if (getMSAASampleCount() > 1)
		{
			desc.attachments[0].texture = rt_MSAAOutline;
			desc.attachments.push_back(RenderPassAttachment::Resolve(rt_Outline));
		}

		//wiTextureHelper::getBlack(),

		desc.attachments.push_back(
			RenderPassAttachment::DepthStencil(
				*GetDepthStencil(),
				RenderPassAttachment::LoadOp::LOAD,
				RenderPassAttachment::StoreOp::STORE,
				ResourceState::DEPTHSTENCIL_READONLY,
				ResourceState::DEPTHSTENCIL_READONLY,
				ResourceState::DEPTHSTENCIL_READONLY
			)
		);

		hr = device->CreateRenderPass(&desc, &renderpass_Outline);
		assert(hr);

		desc.attachments[0].texture = rt_Outline_Red;
		if (getMSAASampleCount() > 1)
		{
			desc.attachments[0].texture = rt_MSAAOutline_Red;
			desc.attachments[1].texture = rt_Outline_Red;
		}
		hr = device->CreateRenderPass(&desc, &renderpass_Outline_Red);
		assert(hr);

		desc.attachments[0].texture = rt_Outline_Blue;
		if (getMSAASampleCount() > 1)
		{
			desc.attachments[0].texture = rt_MSAAOutline_Blue;
			desc.attachments[1].texture = rt_Outline_Blue;
	}
		hr = device->CreateRenderPass(&desc, &renderpass_Outline_Blue);
		assert(hr);

	}

	GGTerrain_WindowResized();
	extern bool bTriggerFovUpdate;
	bTriggerFovUpdate = true; //PE: restore FOV.
}


float fGetHighlightThickness(void);

// GGMAX 2026-08-05: outline-pipeline stage counters for DUMP_OUTLINE. The pipeline has
// four stages that can each silently no-op (feed -> stencil -> mask -> composite);
// these name which stage ran this session so a dead outline is diagnosable in one dump.
uint64_t g_dbgOutlineCompositeRuns = 0; // Wicked_Render_Opaque_Scene editor branch executed
uint64_t g_dbgOutlineMaskRuns = 0;      // RenderOutlineHighlighers mask passes executed
uint64_t g_dbgOutlineSkippedFrames = 0; // frames the idle gate below skipped (mask + composite)
int g_iOutlineMaskTest = 0;             // harness OUTLINE_MASKTEST: 1 = stencil ALWAYS (draw
                                        // unconditionally) to split draw-lands vs compare-fails
int g_iOutlineIdleGate = 1;             // harness OUTLINE_GATE: 0 = always run (pre-2026-08-06
                                        // behaviour, kept for A/B measurement)

// GGMAX 2026-08-06: idle gate for the selection-outline pipeline.
//
// Restoring the outline (2026-08-05) put SIX full-screen passes back into every editor
// frame - three stencil-compare mask draws on their OWN command list, then three
// Postprocess_Outline composites - and they ran whether or not anything was selected.
// The 08-06 hub sweep measured the bill: every demo gained exactly one command list
// (14->15, 16->17, 18->19) and ~1.0-1.7 ms of submit stall, which is noise on a 10 ms
// level but 12-17% on the light ones (Trapped 154->128, Switch Escape 150->128 FPS).
// Upstream Wicked gated both sites on the selection being non-empty; the port left that
// condition commented out at both call sites (the "//&& !translator.selected.empty()"
// remnants). This is that gate, keyed on GGMAX's own highlight registry.
//
// WickedCall_RenderEditorFunctions runs immediately before the mask pass and rebuilds
// g_ObjectHighlightList from scratch every frame: it clears the previous frame's
// highlights, then every path that sets a user stencil ref (WickedCall_DrawObjctBox,
// Wicked_Highlight_AllLogicObjects) pushes its object onto the list as it does so. So
// once that call returns, an empty list means no object carries a stencil ref, the mask
// target would be cleared-and-empty, and the composite would sample nothing.
//
// Both sites must share one verdict: the mask pass is what CLEARS rt_Outline*, so a
// frame that skips the mask must skip the composite too or it would composite last
// frame's stale silhouette.
static bool GGMax_OutlineWorkPending()
{
	if (g_iOutlineIdleGate == 0 || g_iOutlineMaskTest != 0) return true; // forced on for A/B + diagnostics

	extern std::vector<int> g_ObjectHighlightList;
	if (!g_ObjectHighlightList.empty()) return true; // O(1), and true whenever anything is selected/hovered

	// Belt and braces: a stencil ref set outside that registry would never be cleared by
	// RenderEditorFunctions either, so it would sit highlighted forever - and the O(1)
	// check above cannot see it. Sweep the object array occasionally (~once a second) so
	// such a leak still gets drawn; amortised this is far below the 1 ms the gate saves.
	static uint32_t sweepTick = 0;
	static bool bLeakedStencilRef = false;
	if ((sweepTick++ % 60) == 0)
	{
		bLeakedStencilRef = false;
		auto& scene = wiScene::GetScene();
		for (size_t i = 0; i < scene.objects.GetCount(); i++)
		{
			if (scene.objects[i].userStencilRef != 0) { bLeakedStencilRef = true; break; }
		}
	}
	return bLeakedStencilRef;
}

void Wicked_Render_Opaque_Scene(CommandList cmd)
{
	extern bool g_bNo2DRender;
	extern bool BackBufferSnapShotMode;
	if (bImGuiInTestGame)
	{
		if (bActivateStandaloneOutline && master_renderer->GetDepthStencil() != nullptr) //&& !translator.selected.empty())
		{
			GraphicsDevice* device = wiGraphics::GetDevice();

			XMFLOAT4 area;
			bool ImGuiHook_GetScissorArea(float* pX1, float* pY1, float* pX2, float* pY2);
			if (ImGuiHook_GetScissorArea(&area.x, &area.y, &area.z, &area.w) == true)
				//device->SetScissorArea(cmd, area);

			wiRenderer::BindCommonResources(cmd);
			XMFLOAT4 col = XMFLOAT4(0.8f, 0.8f, 0.8f, 0.8f);;
			// GGMAX 2026-08-05: the fork's Postprocess_Outline left an event open for the
			// caller to close; the clone's is internally balanced, so the EventEnd calls
			// that used to follow each Postprocess_Outline are removed (they underflowed
			// the event stack once this path went live).
			wiRenderer::Postprocess_Outline(rt_Outline, cmd, 0.1f, 0.8f, col);
			area = { 0, 0, (float)master.masterrenderer.GetPhysicalWidth(), (float)master.masterrenderer.GetPhysicalHeight() };
			//device->SetScissorArea(cmd, area);
		}
		return;
	}

	if (!bImGuiInTestGame && (!g_bNo2DRender || BackBufferSnapShotMode) )
	{
		// GGMAX 2026-08-06: nothing highlighted this frame = the mask stage skipped, so
		// rt_Outline* hold no silhouette to composite (see GGMax_OutlineWorkPending).
		if (!GGMax_OutlineWorkPending()) return;

		if (master_renderer->GetDepthStencil() != nullptr) //&& !translator.selected.empty())
		{
			GraphicsDevice* device = wiGraphics::GetDevice();

			XMFLOAT4 area;
			float thickness = fGetHighlightThickness();
			bool ImGuiHook_GetScissorArea(float* pX1, float* pY1, float* pX2, float* pY2);
			if (ImGuiHook_GetScissorArea(&area.x, &area.y, &area.z, &area.w) == true)
				//device->SetScissorArea(cmd, area);

			wiRenderer::BindCommonResources(cmd);
			g_dbgOutlineCompositeRuns++;
			XMFLOAT4 col = selectionColor;
			col.w *= 0.65; //opacity;
			// (EventEnd calls removed - see the note in the test-game branch above)
			wiRenderer::Postprocess_Outline(rt_Outline, cmd, 0.1f, thickness, col);

			col = selectionColorRed;
			col.w *= 0.65; //opacity;
			wiRenderer::Postprocess_Outline(rt_Outline_Red, cmd, 0.1f, thickness, col);

			col = selectionColorBlue;
			col.w *= 0.65; //opacity;
			wiRenderer::Postprocess_Outline(rt_Outline_Blue, cmd, 0.1f, thickness, col);

			area = { 0, 0, (float)master.masterrenderer.GetPhysicalWidth(), (float)master.masterrenderer.GetPhysicalHeight() };
			//device->SetScissorArea(cmd, area);

		}
	}
}

void MasterRenderer::Compose(CommandList cmd) const
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	// Always suppress wiProfiler::DrawData() — it calls AllocateGPU() during Compose
	// which can return an invalid allocation, causing an access violation and GPU TDR.
	// Profiler timing data is still collected normally; use GetTextData() to read it safely.
	wi::profiler::DisableDrawForThisFrame();

	__super::Compose(cmd);

	// GGMAX 2.91: snapshot the profiler text AFTER Compose, not before.
	// The intent below was always "read it when all GPU ranges are active" — but sitting one
	// line EARLIER defeated exactly that. GetTextData() skips any range whose in_use is false
	// (wiProfiler.cpp), and BeginFrame clears in_use every frame, so a range OPENED DURING
	// Compose was never in_use at read time and could never appear in the panel — permanently
	// hiding "Outline" (customDraw_Compose -> Wicked_Render_Opaque_Scene), "Terrain - Debug"
	// and "Terrain - Overlay", all three correctly instrumented the whole time.
	// ⚠ Anything opened AFTER this point (e.g. the ImGui bridge below) still cannot show a
	// named row — the text has to exist before the UI that displays it is built. Those costs
	// are not lost though: the Busy/Idle union in wiProfiler::BeginFrame reads resolved query
	// results, not the snapshot, so they are still counted in "GPU Busy".
	if (wi::profiler::IsEnabled())
		g_cachedProfilerText = wi::profiler::GetTextData();

	// DX12: Render splash screen AFTER normal compose (drawn on top with opaque blend).
	// Moved from Master::Update where it created a separate render pass that was
	// immediately cleared by Application::Run's own render pass.
	extern int g_iShowSplashForFirstFewCycles;
	extern bool bAreWeAEditor;
	if (g_iShowSplashForFirstFewCycles > 0)
	{
		if (master.g_pSplashTexture.IsValid())
		{
			wi::image::Params fx;
			fx.enableFullScreen();
			fx.blendFlag = wi::enums::BLENDMODE_OPAQUE;
			wi::image::Draw(&master.g_pSplashTexture, fx, cmd);
		}
		if (bAreWeAEditor)
		{
			wi::Resource tex = wi::resourcemanager::Load("Files\\editors\\uiv3\\MAX-Logo-Square.png");
			if (tex.IsValid())
			{
				wi::graphics::Texture logoTex = tex.GetTexture();
				if (logoTex.IsValid())
				{
					wi::image::Params logoFx;
					logoFx.disableFullScreen();
					logoFx.pos.x = master.canvas.GetLogicalWidth() * 0.5f;
					logoFx.pos.y = master.canvas.GetLogicalHeight() * 0.5f;
					logoFx.pivot = XMFLOAT2(0.5f, 0.5f);
					logoFx.siz.x = (float)logoTex.desc.width;
					logoFx.siz.y = (float)logoTex.desc.height;
					logoFx.blendFlag = wi::enums::BLENDMODE_ALPHA;
					wi::image::Draw(&logoTex, logoFx, cmd);
				}
			}
		}
		return; // skip ImGui during splash
	}
	// Phase 5: Render ImGui draw data using DX12 backend
	extern bool bImGuiInitDone;
	if (bImGuiInitDone)
	{
		extern bool ImGui_DX12_IsInitialized();
		if (ImGui_DX12_IsInitialized())
		{
			// static_cast: WickedEngine compiled with RTTI disabled (/GR-)
			auto* dx12Device = static_cast<wi::graphics::GraphicsDevice_DX12*>(wi::graphics::GetDevice());
			if (dx12Device)
			{
				ID3D12GraphicsCommandList* nativeCmdList = dx12Device->GetDX12GraphicsCommandList(cmd);
				if (nativeCmdList)
				{
					// GGMAX 2.91: the whole editor UI draws here and had NO profiler range and
					// not even a PIX marker — in a docked-panel editor session that is real,
					// invisible GPU cost. wi::gui's own "GUI Render" range never fires because
					// GG does not use wi::gui (zero call sites), so nothing covered this.
					// ⚠ This range CANNOT show a named row: the panel text is snapshotted just
					// after Compose (above) and must exist before the UI that displays it is
					// built, so anything opened here is always past the snapshot. It is still
					// counted in "GPU Busy" — that union is computed in wiProfiler::BeginFrame
					// from resolved query results, which do not care about snapshot order.
					auto range_imgui = wi::profiler::BeginRangeGPU("Editor UI (ImGui)", cmd);
					extern void ImGui_DX12_RenderBridge(ID3D12GraphicsCommandList* cmdList);
					ImGui_DX12_RenderBridge(nativeCmdList);
					wi::profiler::EndRange(range_imgui);
				}
			}
		}
	}
}

void MasterRenderer::Render() const
{
	__super::Render();

	// GGMAX 2026-08-05: selection outline RESTORED. In DX11 the engine fork called this
	// virtual from inside RenderPath3D::Render (fork RenderPath3D.cpp:1630, GGREDUCED
	// block after RenderPostprocessChain); the DX12 clone never gained that hook, so the
	// entire outline pipeline - stencil setters, the rt_Outline* mask passes below, the
	// Postprocess_Outline composite in Wicked_Render_Opaque_Scene, and the settings
	// checkbox + thickness slider - was fully ported but dead (2026-08-05 audit finding).
	// No engine hook is needed: the mask function ignores its cmd argument and begins its
	// OWN command list, so calling it here keeps the ordering guarantee - depth lists
	// (begun earlier in __super::Render) -> mask list -> compose list (begun later).
	RenderOutlineHighlighers(CommandList());

	// GGMAX 3.83: object-library live preview. __super::Render() has just recorded the
	// preview camera's own command list (RenderPath3D::RenderCameraComponents), so the HDR
	// result is complete by the time the tonemap list below runs - command lists execute in
	// the order they were BEGUN, the same ordering guarantee the outline pass relies on.
	// Costs nothing at all unless the pointer is resting on a library thumbnail.
	extern void GGObjectPreview_Render(float fExposure);
	GGObjectPreview_Render(getExposure());
}

// moved into function so we can call it at the right time from within renderpath3D, just before 2D is rendered
std::vector<int> g_StandaloneObjectHighlightList;
void MasterRenderer::RenderOutlineHighlighers(CommandList cmd) const
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif
	if (m_bRenderingVR == false)
	{
		// regular update
		// PE: Make sure highlight and other editor functions is not redered in test game.
		void WickedCall_RenderEditorFunctions(void);
		//PE: Moved to function so we can delete any already setup highlights.
		//if (!bImGuiInTestGame) WickedCall_RenderEditorFunctions();
		extern bool bImGuiRenderTargetFocus;
		WickedCall_SetRenderTargetMouseFocus(bImGuiRenderTargetFocus);
		WickedCall_RenderEditorFunctions();

		if (!bImGuiInTestGame)
		{
			// Selection outline:
			// GGMAX 2026-08-06: WickedCall_RenderEditorFunctions above has just rebuilt the
			// highlight registry for this frame, so the gate reads the current selection.
			if (!GGMax_OutlineWorkPending())
			{
				extern uint64_t g_dbgOutlineSkippedFrames;
				g_dbgOutlineSkippedFrames++;
				return;
			}
			if (GetDepthStencil() != nullptr)
			{
				extern uint64_t g_dbgOutlineMaskRuns;
				g_dbgOutlineMaskRuns++;
				GraphicsDevice* device = wiGraphics::GetDevice();
				CommandList cmd = device->BeginCommandList();

				device->EventBegin("GGMax - Selection Outline Mask", cmd);

				Viewport vp;
				vp.width = (float)rt_Outline.GetDesc().width;
				vp.height = (float)rt_Outline.GetDesc().height;
				device->BindViewports(1, &vp, cmd);

				wiImageParams fx;
				fx.enableFullScreen();
				fx.stencilComp = STENCILMODE::STENCILMODE_EQUAL;
				fx.stencilRefMode = wi::image::STENCILREFMODE_USER;
				// diagnostic splits (harness OUTLINE_MASKTEST):
				//  1 = unconditional draw (proves pass/PSO/RT plumbing, no stencil compare)
				//  2 = ENGINE-nibble compare vs STENCILREF_DEFAULT (proves whether ANY
				//      stencil writes land - every drawn object writes engine ref 1)
				extern int g_iOutlineMaskTest;
				if (g_iOutlineMaskTest == 1) fx.stencilComp = STENCILMODE::STENCILMODE_DISABLED;
				if (g_iOutlineMaskTest == 2) fx.stencilRefMode = wi::image::STENCILREFMODE_ENGINE;
				// 3 = FULL-BYTE compare vs 0x11 (engine DEFAULT | user 1<<4): silhouette
				// appearing = the buffer DOES hold the user nibble and the USER-mode image
				// compare is at fault; black = the write side drops the high nibble.
				if (g_iOutlineMaskTest == 3) fx.stencilRefMode = wi::image::STENCILREFMODE_ALL;

				// Objects outline:
				{
					//device->UnbindResources(TEXSLOT_ONDEMAND0, 1, cmd);
					device->RenderPassBegin(&renderpass_Outline, cmd);
					// Draw solid blocks of selected objects
					fx.stencilRef = EDITORSTENCILREF_HIGHLIGHT_OBJECT;
					if (g_iOutlineMaskTest == 3)
						fx.stencilRef = wi::renderer::CombineStencilrefs(wi::enums::STENCILREF_DEFAULT, (uint8_t)EDITORSTENCILREF_HIGHLIGHT_OBJECT_BLUE); // full-byte engine|user compare sample
					wiImage::Draw(wiTextureHelper::getWhite(), fx, cmd);
					device->RenderPassEnd(cmd);
				}

				// Objects outline Red:
				{
					//device->UnbindResources(TEXSLOT_ONDEMAND0, 1, cmd);
					device->RenderPassBegin(&renderpass_Outline_Red, cmd);
					// Draw solid blocks of selected objects
					fx.stencilRef = EDITORSTENCILREF_HIGHLIGHT_OBJECT_RED;
					wiImage::Draw(wiTextureHelper::getWhite(), fx, cmd);
					device->RenderPassEnd(cmd);
				}

				// Objects outline Blue:
				{
					//device->UnbindResources(TEXSLOT_ONDEMAND0, 1, cmd);
					device->RenderPassBegin(&renderpass_Outline_Blue, cmd);
					// Draw solid blocks of selected objects
					fx.stencilRef = EDITORSTENCILREF_HIGHLIGHT_OBJECT_BLUE;
					wiImage::Draw(wiTextureHelper::getWhite(), fx, cmd);
					device->RenderPassEnd(cmd);
				}
				device->EventEnd(cmd);
			}
		}
		else
		{
			if (!bActivateStandaloneOutline) return;

			//PE: Add objects here for standalone highlights/Outline.
			if (g_StandaloneObjectHighlightList.size() > 0)
			{
				for (int i = 0; i < (int)g_StandaloneObjectHighlightList.size(); i++)
				{
					int obj = g_StandaloneObjectHighlightList[i];
					if (obj > 0)
					{
						if (ObjectExist(obj))
						{
							void* GetObjectsInternalData(int iID);
							sObject* pObject = (sObject*)GetObjectsInternalData(obj);
							if (pObject)
							{
								//WickedCall_SetObjectHighlight(pObject, false);
								void WickedCall_DrawObjctBox(sObject * pObject, XMFLOAT4 color, bool bThickLine, bool ForceBox);
								WickedCall_DrawObjctBox(pObject, XMFLOAT4(0.8f, 0.8f, 0.8f, 0.8f), false, false);
							}
						}
					}
				}
			}


			if (GetDepthStencil() != nullptr)
			{
				GraphicsDevice* device = wiGraphics::GetDevice();
				CommandList cmd = device->BeginCommandList();

				device->EventBegin("GGMax - Selection Outline Mask", cmd);

				Viewport vp;
				vp.width = (float)rt_Outline.GetDesc().width;
				vp.height = (float)rt_Outline.GetDesc().height;
				device->BindViewports(1, &vp, cmd);

				wiImageParams fx;
				fx.enableFullScreen();
				fx.stencilComp = STENCILMODE::STENCILMODE_EQUAL;
				fx.stencilRefMode = wi::image::STENCILREFMODE_USER;

				// Objects outline:
				{
					//device->UnbindResources(TEXSLOT_ONDEMAND0, 1, cmd);
					device->RenderPassBegin(&renderpass_Outline, cmd);
					// Draw solid blocks of selected objects
					fx.stencilRef = EDITORSTENCILREF_HIGHLIGHT_OBJECT;
					wiImage::Draw(wiTextureHelper::getWhite(), fx, cmd);
					device->RenderPassEnd(cmd);
				}
				device->EventEnd(cmd);
			}
		}
	}
}
