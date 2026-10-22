#include "GGTerrain.h"
#include "CFileC.h"
#include "GGTerrainWicked.h"
#include "GGGrass.h"
#include "GGTrees.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiTerrain.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiResourceManager.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiHelper.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiRenderer.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiProfiler.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiRenderPath3D.h"
#include "../../../../WickedEngineDX12/WickedEngine/wiTimer.h"
// GGMAX 1.74: GG_HAIR_GRASS_MERGED / GG_HAIR_MAX_GRASS_TYPES for the merged-grass path
#include "../../../../WickedEngineDX12/WickedEngine/shaders/ShaderInterop_HairParticle.h"
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

// Performance profiling: accessor functions defined in master_part0.cpp / master_part1.cpp
extern wi::RenderPath3D* GGPerf_GetRenderPath();
extern wi::scene::LightComponent* GGPerf_GetSunLight();
extern wi::scene::WeatherComponent& GGPerf_GetWeather();
extern std::string GGPerf_GetCachedProfilerText();

static wi::ecs::Entity wickedTerrainEntity = wi::ecs::INVALID_ENTITY;
static bool wickedTerrainInitialised = false;
static bool wickedTerrainMaterialsSetup = false;
static std::string wickedTerrainExeDir;  // EXE directory for resolving texture paths

// Phase 3: Painted material support
static int materialToSlot[GGTERRAIN_MAX_SOURCE_TEXTURES]; // GG 0-based mat index -> Wicked blendmap layer index
static int maxPaintedSlot = -1;                            // highest blendmap layer used by any painted material

// Path A: DX11-style CPU-computed blend weights (per-vertex) that override Wicked's built-in
// smoothstep-based region weights. Wicked's 4-slot (base/slope/low/high) blend can't reproduce
// GG's per-layer height ramps (see project_terrain_texture_mismatch.md). This system runs after
// Generation_Update and rewrites blendmap_layers[0..maxAutoBlendSlot] to match GG's DX11 shader
// logic exactly, giving mountains their green mid-altitude material instead of grey rock bleed.
static int g_layer1MaterialSlot = -1;                             // Extra slot for layer[1] material (GG mat 0 by default)
static std::unordered_set<uint64_t> dx11BlendProcessedKeys;       // Chunks already given DX11-shape auto weights
static std::unordered_map<uint64_t, wi::ecs::Entity> dx11BlendChunkKeyToEntity;

// GGMAX 3.113: GG's auto-blend (ApplyDX11StyleAutoBlend / FillChunkBlendmapGG) writes DX11 SPLAT
// weights - base starts at 1 and every height/slope ramp cross-fades, so they SUM TO 1 - but Wicked's
// terrain compositor is a front-to-back DECAL stack that wants opacities (stock keeps base = 1). Fed
// splat weights it covered only 1 - prod(1 - w): blended ground was up to 25% too dark everywhere two
// layers mixed, and its normal map (BC5, 0.5 = flat) collapsed toward (-1,-1) - a consistent tilt that
// dimmed the sun by ~40% on DX12TESTLEVEL. Found chasing Lee's "baked ground is a different colour".
// The engine now has a SPLAT mode (wi::terrain::gg_splat_blend: average the weights), which is exact
// between vertices too (splat weights interpolate linearly, opacities do not) - so the weights stay
// as DX11 wrote them. setup.ini terrainsplatblend=0 / harness SET_TERRAINSPLATBLEND 0 = the old look.
bool gg_terrain_splat_blend = true;

// Phase 3: Blendmap state tracking — which chunks have had painted weights applied
static std::unordered_set<uint64_t> processedChunkKeys;
// Track which entity was at each grid key when we last painted it.
// When Wicked removes a distant chunk and regenerates it on camera return,
// the new chunk has a different entity ID — we detect this and repaint.
static std::unordered_map<uint64_t, wi::ecs::Entity> chunkKeyToEntity;

// Grass: a custom per-chunk HairParticleSystem grown from GG's painted grass map (pGrassMap).
// Wicked's built-in terrain grass stays disabled (it places grass from material regions, not our
// grass map). We reuse the HairParticleSystem class but drive placement/lifecycle ourselves,
// mirroring the ProcessPaintedChunkBlendmaps pattern.
//
// Per painted grass type, one cached MaterialComponent (g_grassMaterials[]) and one appearance
// template (g_grassAppearance[]) drive the look. A chunk with N painted grass types spawns N
// HairParticleSystem entities, each masked to just its own vertices via vertex_lengths.
static bool wickedGrassSetup = false;
static bool wickedGrassEnabled = true; // G key toggles grass visibility/creation
static bool wickedTerrainHidden = false; // O key / View Options terrain visibility (file-scope so the UI setter and the O-key toggle share state)
static bool s_ggEmptyModeSwept = false;  // GGMAX 2.68g: the empty-mode poll hid chunks while wickedTerrainHidden was false — must re-show when the mode ends
int g_blendScanInterval = 4; // POST-LOAD DIP FIX 2026-07-29: blend-scan cadence outside initial build (1 = stock every-frame; harness SET_BLENDSCAN)
static std::unordered_map<uint64_t, wi::ecs::Entity> grassChunkKeyToChunkEntity; // chunk entity when grass was built
// Per-chunk per-type hair entity tracking. Each chunk-type slot is INVALID_ENTITY until first paint
// of that type in that chunk; after that the same entity is reused across paint events (Stage 2 —
// vertex_lengths is restamped in place instead of removing + recreating the entity, so existing
// strand positions hold rock-still during paint instead of regenerating their per-frame tail state).
struct ChunkGrassEntities
{
	wi::ecs::Entity perType[GGGRASS_TOTAL_REAL_TYPES];
	// GGMAX 1.74: in merged mode the chunk owns exactly ONE hair entity covering every painted
	// type, and perType[] stays empty. Kept as a separate slot rather than reusing perType[0] so
	// the teardown paths and the GRASS_CHUNKS histogram stay unambiguous about which mode built it.
	wi::ecs::Entity merged = wi::ecs::INVALID_ENTITY;
	uint64_t mergedTypeMask[2] = { 0, 0 };  // which types the merged entity was built for (88 bits)
	// GGMAX 3.116: the strandCount its hair system(s) were created with - a Grass Density change
	// (or a draw-distance change in DX11 density mode) re-queues the chunk when this goes stale.
	uint32_t builtStrands = 0;
	ChunkGrassEntities()
	{
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++) perType[t] = wi::ecs::INVALID_ENTITY;
	}
};
static std::unordered_map<uint64_t, ChunkGrassEntities> grassChunkKeyToGrassEntities;
static std::unordered_map<uint64_t, int> grassChunkKeyToTier;                    // current LOD tier per chunk

// GGMAX 3.112: grass that OUTLIVES A TERRAIN BAKE.
//
// Every grass hair grows from a Wicked terrain chunk's MeshComponent (hair.meshID) and is
// Component_Attach'ed to that chunk. The bake's teardown (GGTerrainWicked_Shutdown) Entity_Remove's
// every chunk, recursively, so ticking Terrain Bake deleted all the grass with them - Lee,
// DX12TESTLEVEL: HAIR_SYSTEMS 22 -> 0 - and GGTerrainWicked_Update then returned before the grass
// manager every frame, so nothing could grow it back.
//
// So before the chunks go, every chunk inside the editable area keeps what grass needs of it:
//  - a chunk with painted grass hands its mesh over to a GRASS EMITTER: a mesh-only entity (a
//    MeshComponent + TransformComponent and NO ObjectComponent, so nothing draws, culls or shadows
//    it - it is only what the hair simulate reads its emitter triangles from). The chunk's live grass
//    is re-parented onto it untouched (no regrow, no pop);
//  - a BARE chunk keeps a CPU-only copy of its shape (positions + normals, ~108 KB; the LOD0
//    topology is identical for every chunk and kept once), holding no video memory at all. The
//    moment grass is painted or Populated onto it, it is PROMOTED to an emitter.
// The grass manager keeps running against the emitters while the bake holds, so Test Game LOD rings
// still follow the player. Grass Off DEMOTES every emitter back to a CPU copy (its video memory is
// for the grass alone); grass back on promotes them again. Released on the way back to a live
// terrain (Init).
// ⚠ The same per-level-value-in-a-process-global shape as every VRAM retention defect: anything
// added here must be released by ReleaseBakeGrassEmitters.
struct BakeGrassBareChunk
{
	XMFLOAT4X4 world;
	wi::vector<XMFLOAT3> positions;   // chunk-LOCAL, exactly the chunk mesh's
	wi::vector<XMFLOAT3> normals;
};
static wi::vector<wi::ecs::Entity> s_bakeGrassEmitters;                    // ProcessGrassChunks' chunk source while baked
static std::unordered_map<uint64_t, wi::ecs::Entity> s_bakeGrassEmitterByKey;
static std::unordered_map<uint64_t, BakeGrassBareChunk> s_bakeGrassBare;   // editable chunks with no emitter
static wi::vector<uint32_t> s_bakeGrassLod0Indices;   // the shared LOD0 triangle list a promoted emitter is built with
static bool s_bakeGrassPromotePending = false;        // promote every bare chunk that now holds grass (budgeted)
static float s_bakeGrassChunkStride = 0.0f;   // the torn-down terrain's chunk stride
static uint32_t s_bakeGrassRehomed = 0;       // DUMP_BAKE: live grass systems moved onto emitters at bake
static uint32_t s_bakeGrassPromoted = 0;      // DUMP_BAKE: bare chunks promoted to emitters since the bake
static float s_bakeGrassAdoptMs = 0.0f;       // DUMP_BAKE: what the adoption cost, once, at bake

// GGMAX: how many distinct grass types are live in each chunk right now. One hair system
// exists per (chunk x painted type), so this histogram IS the payoff available from merging a
// chunk's types into a single system — a chunk with 1 type saves nothing, a chunk with N types
// saves (N-1)/N of its strand buffers and of its wasted degenerate-quad work. Harness
// GET_PERF_DATA prints it as GRASS_CHUNKS. Measure before assuming the merge pays.
void GGGrass_GetChunkTypeHistogram(unsigned int* histOut, unsigned int histLen,
	unsigned int* chunksOut, unsigned int* systemsOut)
{
	unsigned int chunks = 0, systems = 0;
	// A slot holding a non-INVALID entity is NOT proof a hair system exists: teardown paths can
	// remove the entity from the scene while the map record survives, and the map also outlives a
	// level change. Counting slots alone reported a phantom "chunks=9 systems=19" on all six
	// grass-free demos in the 2026-08-02 sweep while HAIR_SYSTEMS read 0. Validate every slot
	// against the live scene so this line can never disagree with HAIR_SYSTEMS again.
	wi::scene::Scene& scene = wi::scene::GetScene();
	auto live = [&scene](wi::ecs::Entity e) {
		return e != wi::ecs::INVALID_ENTITY && scene.hairs.GetComponent(e) != nullptr;
	};
	for (auto& kv : grassChunkKeyToGrassEntities)
	{
		unsigned int n = 0;
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
			if (live(kv.second.perType[t])) n++;
		unsigned int sys = n;
		if (live(kv.second.merged))
		{
			// GGMAX 1.74 merged: ONE system covering however many types the mask records.
			sys += 1;
			for (int w = 0; w < 2; w++)
				for (int b = 0; b < 64; b++)
					if (kv.second.mergedTypeMask[w] & (1ull << b)) n++;
		}
		if (n == 0) continue; // chunk record exists but is bare at this distance
		chunks++;
		systems += sys;
		if (histOut != nullptr && histLen > 0)
			histOut[n < histLen ? n : (histLen - 1)]++;
	}
	if (chunksOut) *chunksOut = chunks;
	if (systemsOut) *systemsOut = systems;
}

// SETTLE GATE (2026-07-25, load-window grass-flicker fix): frame stamp of each chunk key's last
// ENTITY change. During the initial build Wicked regenerates every chunk 2-3 times (progressive
// refinement) — rebuilding the chunk's grass on EVERY regeneration produced ~880 tear-down/regrow
// pops compressed into the first ~8s of a level load (the user-visible flicker wave; it also
// re-fires on sculpt-driven regens). Grass REMOVAL on a recycled chunk stays immediate (the old
// hair entities reference a destroyed mesh) but REGROWTH is deferred until the chunk entity has
// been stable for GG_GRASS_SETTLE_FRAMES — each chunk now grows its grass exactly once, after its
// final regeneration. g_grassSettlePending re-arms the gated ProcessGrassChunks pass (every 10th
// frame) until no deferrals remain, so deferred growth cannot be stranded by the signature gate.
static std::unordered_map<uint64_t, uint32_t> grassChunkKeyToEntityStamp;
static bool g_grassSettlePending = false;
static constexpr uint32_t GG_GRASS_SETTLE_FRAMES = 30;
// Distance-LOD grass density (driven live by the SET_GRASS automation command).
// bladesPerVertex is the NEAR-tier density; mid/far tiers scale it down and grass stops past
// lodChunks chunk-distances from the camera. Only the near chunks are dense, so total strands stay
// bounded — VRAM-safe even during the load burst that previously crashed the driver (full density
// across many streaming chunks). A fully-grassy chunk has ~4489 verts; strands = grassyVerts*blades.
static uint32_t g_grassBladesPerVertex = 120;  // near-tier blades/vertex (LOD keeps total bounded)
static uint32_t g_grassMaxStrands = 350000;    // per-chunk strand cap (split across used types)
// Manual outer-ring override in chunk-distances, used by the SET_GRASS lodchunks automation key
// when non-zero. Otherwise the outer ring is derived from gggrass_global_params.lod_dist
// (the editor's Grass Draw Distance slider). Zero = follow slider.
static float    g_grassLODChunksOverride = 0.0f;
// Tier-upgrade boundaries in chunk-distances (user-reported 2026-07-27: the tier-3 full-density
// rebuild at the old 1.0 chunk ≈ 5000 inches was a visible SQUARE POP right in front of the
// game-mode player — a 5.5x density step applied to a whole chunk in one frame). Pushing the
// boundaries out measured 72 -> 53 FPS with the strand LOD OFF (2.46M strands), so the pushed
// values are COUPLED to the LOD opt-in: 0 = AUTO (LOD off -> stock 1.0/1.7; LOD on -> 1.5/2.2,
// where the strand LOD decimates the added far strands and pays most of the bill). An explicit
// SET_GRASS tier3/tier2 value always overrides AUTO.
// GGMAX 3.117: all of this applies to the pre-3.117 far field only (gg_grass_far_mode 0, setup.ini
// grassfardx11=0) or a constant-density level (grassdensitydx11=0). Under the default DX11 far field the
// rings collapse to one full-density ring at the cull (ProcessGrassChunks) - unless SET_GRASS lodchunks,
// tier3 or tier2 is set explicitly, which restores the ring structure for that A/B.
static float    g_grassTier3Chunks = 0.0f;   // 0 = AUTO; explicit value = full-density ring
static float    g_grassTier2Chunks = 0.0f;   // 0 = AUTO; explicit value = mid-density ring
namespace wi { extern bool gg_grass_lod; }   // engine strand-LOD opt-in (wiHairParticle.cpp)
namespace wi { extern int gg_grass_dx11_falloff; extern float gg_grass_dx11_fade_range; }   // GGMAX 3.117 (wiHairParticle.cpp)
// GGMAX 3.117: the DX11 grass FAR FIELD. 1 (default) = every strand full size out to the Grass Draw
// Distance, then DX11's per-blade dissolve to +2500 (engine HAIR_FLAG_GG_DX11_FALLOFF), full density in
// every chunk that can hold a visible strand, flowers to the same distance as the rest - as DX11 drew
// it. 0 = the 1.49b strand LOD (survivors widened 2-4x, fade over the last 20%, 18%/5% outer rings).
// 2 = mode 1 with the dissolve pushed out to the cull (a measuring baseline). setup.ini grassfardx11,
// harness SET_GRASSFARMODE.
int gg_grass_far_mode = 1;

// GGMAX 1.74 merged grass opt-in. DEFAULT OFF until the TESTPRO1 density gate signs it off —
// grass is the one subsystem in this codebase that has already been broken once by an
// unverified "optimisation", so the shipped path stays the per-type one until measured.
// Harness SET_GRASSMERGE <0|1> then reload the level (entities are built at chunk-spawn time).
bool gg_grass_merge = true;   // GGMAX 1.86: DEFAULT ON (user-approved 2026-08-03). setup.ini grassmerge=0 reverts.

// ============================================================================
// GGMAX low-VRAM preset (2026-08-02) — the "fit inside a 4 GB card" bundle.
// setup.ini `lowvram=1`, or harness SET_LOWVRAM. Off = today's behaviour exactly.
//
// Grass is the whole content spread: 17.3 GB hub-wide, 4297 MB on Z Island alone, and zero on
// six demos. No 4 GB preset works without capping it, so the preset's content lever is the grass
// DRAW DISTANCE, which is the one grass lever already proven to save real memory (~1.14 GB at
// 750) without touching placement or density — the 2026-08-01 coverage-scaling attempt that DID
// touch placement had to be reverted for clumping, so this deliberately stays away from that.
//
// 750 is the editor slider's own minimum, chosen there because the strand fade needs a range to
// work in; going below it would reintroduce whole-chunk pop-in. This is a CAP, not an override:
// a level that already asks for less than the cap keeps its own value.
bool  gg_lowvram = false;

// ============================================================================
// GGMAX 2.94 — BRUTAL OFF-SWITCHES (Graphics and Performance panel, Phase 2 perf)
//
// These are NOT "stop drawing it" flags. Each one is wired so the subsystem's
// elements never enter (or are torn out of) the Wicked scene, so they pay no
// culling, no Scene::Update transform/AABB work, no shadow casting, no virtual
// texture feedback and no VRAM. A renderable=false hide leaves every one of
// those costs in place - that mistake is already documented for terrain in the
// 2.68f comment at GGTerrainWicked.cpp:3021.
//
// Each effective flag is (machine || level): setup.ini key OR the panel tick.
// The OR and the setters live beside GGSetLowVRAM in master_part1.cpp.
//
// gg_no_terrain : the wi::terrain::Terrain component + its 625 chunk entities
// gg_no_trees   : the 20K tree pool + shadow proxies + per-type LOD assets
// gg_no_grass   : every HairParticleSystem the grass manager emits
// gg_no_water   : weather ocean, and with it the PLANAR REFLECTION pass
// ============================================================================
bool  gg_no_terrain = false;
bool  gg_no_trees   = false;
bool  gg_no_grass   = false;
bool  gg_no_water   = false;

// ============================================================================
// GGMAX 3.08: ULTRA-LOW-SPEC OFF-SWITCHES, ROUND 2.
//
// The 2.94 four remove CONTENT. These remove per-frame RENDERING WORK instead, so they help on
// levels with nothing to strip. Every lever here was already implemented and working - it was
// bound to a debug KEYBOARD key (GGTerrainWicked_Update's 1-9 handlers) and never exposed to a
// player. This makes them real switches: panel tick + setup.ini + harness, same shape as 2.94.
//
// ★ RULE followed here: each switch only ever FORCES ITS SUBSYSTEM OFF, and never forces
// anything ON. Re-asserting "on" every frame would CLOBBER whatever the level author chose (the
// dead-UI-knob failure mode: control -> field -> something re-derives the field every frame).
// Restoring is done once, on the ON->OFF edge, by re-running the level's own visuals apply.
//
// ⚠ NOT duplicated here: render resolution. The FSR combo in Graphics & Performance already
// drives resolutionScale down to 1/2 native and is the single biggest low-spec lever there is.
// ============================================================================
bool  gg_no_postfx  = false;   // bloom, DOF, motion blur, shafts, volumetrics, SSR/SSGI, ...
bool  gg_no_ao      = false;   // ambient occlusion pass
bool  gg_simple_sky = false;   // volumetric clouds + realistic sky -> cheap sky
bool  gg_no_shadows = false;   // the entire shadow render
bool  gg_no_occlusion = false; // hardware occlusion QUERIES (they cost to save; on a weak GPU
                               // the query pass can cost more than the draws it removes - A/B it)
int   gg_particle_pct = 100;   // emit-rate scale for Wicked emitters, 0..100
float gg_object_cull_dist = 0.0f; // 0 = off; >0 = cap every object's draw distance (inches)
// GGMAX 3.19: armed by the panel / harness when Texture Detail changes, consumed once on the
// main thread in GGApplyLowSpecSwitches below. Deliberately NOT armed by setup.ini - the ini
// value is already in force by the time anything loads, so a boot-time re-create would be a
// GPU drain for nothing. gg_texture_divide_rebuilt is how many textures the last apply changed.
bool     gg_texture_divide_pending = false;
uint32_t gg_texture_divide_rebuilt = 0;
// GGMAX 3.19: how many resident chunk VTs the last apply asked to re-bake. Reported because
// "the terrain looks the same" has TWO causes and they need telling apart: the re-bake never
// ran (0 here), or it ran and the baked tile resolution simply does not resolve the source
// texture's top mips. Without this counter the two are indistinguishable from a screenshot.
uint32_t gg_texture_divide_vtrepaints = 0;

// GGMAX 3.19: defined further down, inside namespace GGTerrain, where the terrain statics it
// needs are in scope. (GGApplyLowSpecSwitches itself sits above that namespace.)
namespace GGTerrain { static void GGTerrainWicked_RepaintAllResidentVT(); }

void GGApplyLowSpecSwitches()
{
	wi::RenderPath3D* rp = GGPerf_GetRenderPath();
	if ( rp == nullptr ) return;

	// ★ CAPTURE-ON-ENTRY, RESTORE-ON-EXIT, per switch. The first version force-set things OFF
	// every frame and leaned on GGApplyVisualsNow() to put them back on the OFF edge. MEASURED, it
	// only put SOME back: bloom and light shafts returned, shadows and ambient occlusion did not,
	// because the visuals apply never writes those two. Every later row of that ladder was then
	// taken against a scene that still had shadows off - the 2.94 grass latch again, and the
	// reason this pattern is now spelled out rather than assumed. Remembering the caller's own
	// value also means we never invent a default: if a level ships with bloom already off, ticking
	// and unticking the switch leaves it off, which is what the author asked for.
	struct Saved
	{
		bool valid = false;
		bool bloom, shafts, motionblur, dof, lensflare, chroma, sharpen, eye, vollights, ssr, ssgi, sunvol;
		bool shadows;
		wi::RenderPath3D::AO ao;
		bool clouds, realsky;
	};
	static Saved sv;
	static bool s_postfx = false, s_ao = false, s_sky = false, s_shadows = false;

	wi::scene::LightComponent* sun = GGPerf_GetSunLight();
	auto& weather = GGPerf_GetWeather();

	// ---- POST EFFECTS ----------------------------------------------------------------------
	if ( gg_no_postfx && !s_postfx )
	{
		sv.bloom      = rp->getBloomEnabled();       sv.shafts   = rp->getLightShaftsEnabled();
		sv.motionblur = rp->getMotionBlurEnabled();  sv.dof      = rp->getDepthOfFieldEnabled();
		sv.lensflare  = rp->getLensFlareEnabled();   sv.chroma   = rp->getChromaticAberrationEnabled();
		sv.sharpen    = rp->getSharpenFilterEnabled(); sv.eye    = rp->getEyeAdaptionEnabled();
		sv.vollights  = rp->getVolumeLightsEnabled(); sv.ssr     = rp->getSSREnabled();
		sv.ssgi       = rp->getSSGIEnabled();
		sv.sunvol     = sun ? sun->IsVolumetricsEnabled() : false;
		sv.valid = true;
	}
	if ( gg_no_postfx )
	{
		rp->setBloomEnabled(false);      rp->setLightShaftsEnabled(false);
		rp->setMotionBlurEnabled(false); rp->setDepthOfFieldEnabled(false);
		rp->setLensFlareEnabled(false);  rp->setChromaticAberrationEnabled(false);
		rp->setSharpenFilterEnabled(false); rp->setEyeAdaptionEnabled(false);
		rp->setVolumeLightsEnabled(false);  rp->setSSREnabled(false);
		rp->setSSGIEnabled(false);
		// ⚠ deliberately NOT touched: FXAA (the FSR upscaler needs it) and colour grading (a cheap
		// LUT lookup that defines the level's authored look). The editor outline pass stays too.
		if ( sun ) sun->SetVolumetricsEnabled(false);
	}
	else if ( s_postfx && sv.valid )
	{
		rp->setBloomEnabled(sv.bloom);      rp->setLightShaftsEnabled(sv.shafts);
		rp->setMotionBlurEnabled(sv.motionblur); rp->setDepthOfFieldEnabled(sv.dof);
		rp->setLensFlareEnabled(sv.lensflare);   rp->setChromaticAberrationEnabled(sv.chroma);
		rp->setSharpenFilterEnabled(sv.sharpen); rp->setEyeAdaptionEnabled(sv.eye);
		rp->setVolumeLightsEnabled(sv.vollights); rp->setSSREnabled(sv.ssr);
		rp->setSSGIEnabled(sv.ssgi);
		if ( sun ) sun->SetVolumetricsEnabled(sv.sunvol);
	}
	s_postfx = gg_no_postfx;

	// ---- AMBIENT OCCLUSION -----------------------------------------------------------------
	if ( gg_no_ao && !s_ao ) sv.ao = rp->getAO();
	if ( gg_no_ao )          rp->setAO( wi::RenderPath3D::AO_DISABLED );
	else if ( s_ao )         rp->setAO( sv.ao );
	s_ao = gg_no_ao;

	// ---- SHADOWS ---------------------------------------------------------------------------
	if ( gg_no_shadows && !s_shadows ) sv.shadows = rp->getShadowsEnabled();
	if ( gg_no_shadows )               rp->setShadowsEnabled(false);
	else if ( s_shadows )              rp->setShadowsEnabled(sv.shadows);
	s_shadows = gg_no_shadows;

	// ---- SKY -------------------------------------------------------------------------------
	if ( gg_simple_sky && !s_sky )
	{
		sv.clouds  = weather.IsVolumetricClouds();
		sv.realsky = weather.IsRealisticSky();
	}
	if ( gg_simple_sky )
	{
		weather.SetVolumetricClouds(false);
		weather.SetRealisticSky(false);
	}
	else if ( s_sky )
	{
		weather.SetVolumetricClouds(sv.clouds);
		weather.SetRealisticSky(sv.realsky);
	}
	s_sky = gg_simple_sky;

	// ---- OCCLUSION QUERIES -----------------------------------------------------------------
	// Not obviously a win: occlusion culling EXISTS to save time. But the query pass is itself
	// GPU work (on Aztec `Occlusion Culling` + `Occlusion Culling Render` = 0.53 ms) and on a weak
	// card with little overdraw it can cost more than the draws it removes. Shipped as an A/B.
	static bool s_occl = false; static bool sv_occl = true;
	if ( gg_no_occlusion && !s_occl ) sv_occl = rp->getOcclusionCullingEnabled();
	if ( gg_no_occlusion )            rp->setOcclusionCullingEnabled(false);
	else if ( s_occl )                rp->setOcclusionCullingEnabled(sv_occl);
	s_occl = gg_no_occlusion;

	// ---- PARTICLE DENSITY ------------------------------------------------------------------
	// Scales the EMIT RATE of every Wicked emitter. Writing base*k (never count*k) keeps it
	// idempotent - scaling the live value every frame would decay it to zero in a second.
	// ⚠ GG has THREE particle systems and this is only the Wicked one; the legacy gpup/.arx and
	// WPE emitters are untouched, so a level built on those will not respond to this slider.
	{
		static std::vector< std::pair<wi::ecs::Entity, float> > s_origCount;
		static int s_prevPct = 100;
		wi::scene::Scene& scene = wi::scene::GetScene();
		if ( gg_particle_pct != 100 )
		{
			const float k = (float)gg_particle_pct * 0.01f;
			// GGMAX 3.121: the Particle Library's own preview effect is not the level's - Particle Density
			// is a per-level setting, and at 25% the hovered effect previewed a quarter of its particles
			// (at 0% none, and 3.111 thumbnail generation then saw an empty tile). Children of the library's
			// preview root keep their authored count.
			extern uint32_t g_LibraryPreviewWPERoot;
			for ( size_t i = 0; i < scene.emitters.GetCount(); i++ )
			{
				const wi::ecs::Entity e = scene.emitters.GetEntity(i);
				float base = -1.0f;
				for ( size_t j = 0; j < s_origCount.size(); j++ )
					if ( s_origCount[j].first == e ) { base = s_origCount[j].second; break; }
				if ( g_LibraryPreviewWPERoot != 0 )
				{
					const wi::scene::HierarchyComponent* h = scene.hierarchy.GetComponent( e );
					if ( h && h->parentID == g_LibraryPreviewWPERoot )
					{
						if ( base >= 0.0f ) scene.emitters[i].count = base;   // scaled before it became the preview
						continue;
					}
				}
				if ( base < 0.0f ) { base = scene.emitters[i].count; s_origCount.push_back( std::make_pair(e, base) ); }
				scene.emitters[i].count = base * k;
			}
		}
		else if ( s_prevPct != 100 )
		{
			for ( size_t j = 0; j < s_origCount.size(); j++ )
			{
				wi::EmittedParticleSystem* em = scene.emitters.GetComponent( s_origCount[j].first );
				if ( em ) em->count = s_origCount[j].second;   // a stale entity simply no longer resolves
			}
			s_origCount.clear();
		}
		s_prevPct = gg_particle_pct;
	}

	// ---- OBJECT DETAIL DISTANCE ------------------------------------------------------------
	// Caps how far away ANY object is still drawn. Uses ObjectComponent::draw_distance, the same
	// machinery 3.03 used for the tree handover: Wicked dithers the object out over its own
	// radius and culls it outright past draw_distance + radius (wiRenderer.cpp 4237/8760), so
	// things fade rather than pop. Default FLT_MAX means almost every object opts out today.
	//
	// ★ Objects that ALREADY have a finite draw_distance are skipped, never overwritten. That is
	// how the 3.03 tree pool keeps ownership of its own fade - capping it here would fight the
	// billboard handover and reintroduce a pop. It also means restoring is exact: anything we
	// touched went from FLT_MAX, so FLT_MAX is what it goes back to.
	{
		static std::vector<wi::ecs::Entity> s_capped;
		static float s_prevCull = 0.0f;
		const float kInfinite = 1.0e30f;
		wi::scene::Scene& scene = wi::scene::GetScene();
		if ( gg_object_cull_dist > 0.0f )
		{
			// O(n) with an O(1) test - an object already capped fails the >= kInfinite test and is
			// skipped, so this costs one float compare per object per frame on 5000+ objects.
			for ( size_t i = 0; i < scene.objects.GetCount(); i++ )
			{
				wi::scene::ObjectComponent& o = scene.objects[i];
				if ( o.draw_distance < kInfinite ) continue;   // owned by someone else (or already ours)
				o.draw_distance = gg_object_cull_dist;
				s_capped.push_back( scene.objects.GetEntity(i) );
			}
			// ★ GGMAX 3.19: ANY change to the cap has to be written to the ones we already own,
			// not just a lowering. This read `< s_prevCull`, so dragging the slider DOWN worked
			// and dragging it UP did nothing at all: the first loop skips an object that already
			// has a finite draw_distance (that test is what stops us stealing the tree pool's
			// fade), so every object we had already capped stayed at the OLD, smaller distance
			// and the scene never came back. Lee reported it as "only updates when sliding
			// downwards". A refresh either way is the same one write per owned object.
			if ( gg_object_cull_dist != s_prevCull )
				for ( size_t j = 0; j < s_capped.size(); j++ )
				{
					wi::scene::ObjectComponent* o = scene.objects.GetComponent( s_capped[j] );
					if ( o ) o->draw_distance = gg_object_cull_dist;
				}
		}
		else if ( s_prevCull > 0.0f )
		{
			for ( size_t j = 0; j < s_capped.size(); j++ )
			{
				wi::scene::ObjectComponent* o = scene.objects.GetComponent( s_capped[j] );
				if ( o ) o->draw_distance = std::numeric_limits<float>::max();
			}
			s_capped.clear();
		}
		s_prevCull = gg_object_cull_dist;
	}

	// ---- TEXTURE DETAIL ---------------------------------------------------------------------
	// GGMAX 3.19: 3.12 made the divide act inside the DDS loader, which meant the panel control
	// only changed the NEXT level you loaded - looked at from the user's chair, it did nothing.
	// Applied here, on the main thread in the update phase, because the work drains the GPU and
	// re-creates textures: it must not happen inside the ImGui draw that set the flag.
	if ( gg_texture_divide_pending )
	{
		gg_texture_divide_pending = false;
		const uint32_t rebuilt = wi::resourcemanager::gg_ApplyTextureDivideLive();

		// ★ The terrain needs a second nudge - see GGTerrainWicked_RepaintAllResidentVT below,
		// which is where the terrain statics are actually in scope.
		if ( rebuilt > 0 )
			GGTerrain::GGTerrainWicked_RepaintAllResidentVT();

		gg_texture_divide_rebuilt = rebuilt;
	}
}

// GGMAX 2.94f: master switch for extending the terrain idle gate to the ENGINE's own
// Generation_Update caller (wiScene.cpp). Default ON. Harness SET_ENGINEGENGATE 0|1 exists so
// the change can be A/B'd in one binary - 0 restores the pre-2.94f behaviour where the engine
// caller ran every frame regardless of how calm the terrain was.
bool  gg_engine_gen_gate = true;
float gg_lowvram_grass_dist = 750.0f;
// Uniform strand-count multiplier, 0..1 (setup.ini takes it as a percent). Applied in
// GrassTierDensityScale — see the reasoning there for why this axis is safe and the mask axis is
// not. Grass memory is linear in strandCount, so 0.5 here is ~half the grass VRAM.
//
// DEFAULT 0.75, measured against the TESTPRO1 clumpCV gate on 2026-08-02. The meadow is
// over-saturated at full density — strands overlap heavily — so the first quarter is very nearly
// free: coverage 9.450% -> 9.355% (-0.095 pp, inside the shot-to-shot noise band) and clumpCV
// 1.104 -> 1.116, for -541 MB of grass buffers on that scene. Below the knee it stops being free:
// 50% costs -1.47 pp of coverage, and 30% breaks the gate outright (clumpCV +0.25, the same
// magnitude of failure as the reverted 2026-08-01 build, with visibly bare ground).
float gg_lowvram_grass_density = 0.75f;

// ★★ GGMAX 3.116: GRASS DENSITY - DX11 parity plus a whole-level control.
//
// DX11 (GGGrass.cpp, the instanced path) drew a FIXED budget of 400,000 single-quad instances
// over an 8x8 camera-centred grid whose cell side is (lod_dist + 2500)/3, so its density near the
// camera was 56250 / (lod_dist + 2500)^2 per square inch - the Grass Draw Distance WAS the density
// control: longer reach, thinner grass (1500 -> 5.45 per m2, the biomes' 4208-7000 -> 1.94-0.97).
// DX12 laid a constant 100,000 strands per 5280-unit chunk (5.56 per m2) whatever the distance, so
// biome-made levels drew 2.9-5.8x DX11's grass near the camera and 750-unit levels 0.67x.
// gg_grass_density_dx11 (default ON) lays the DX11 density for the level's draw distance; the
// percentage (setup.ini grassdensity x the level's Grass Density, master_part1.cpp) scales it.
bool gg_grass_density_dx11 = true;
int  gg_grass_density_pct = 100;            // effective = machine x level / 100
static const uint32_t GG_GRASS_MAX_STRANDS = 150000;   // per system: DX11 at the 750 slider minimum
// LATCHED values the grass systems are built from. Moved only after the inputs have held still for
// GG_GRASS_DENSITY_SETTLE frames (a draw-distance drag must not rebuild grass every frame).
static float g_grassNearStrands = 100000.0f;   // strands per chunk at full (tier 3) density
static float g_grassLowVramMul = 1.0f;
static bool  g_grassDensityLatched = false;
// GGMAX 3.117 (follow-up review): the density MODE rides the latch too - the far-field ring collapse reads
// this, not the live flag, so rings and strand counts flip in the SAME pass (reading the live flag let a
// harness SET_GRASSDENSITYMODE collapse the ring ~15 frames before the counts followed: outer chunks built
// at the old constant 100k, then rebuilt again - every chunk twice, ~+550 MB transient at 6000).
static bool  g_grassDensityDX11Latched = true;
// GGMAX 3.117: ONE predicate for "the DX11 far field has collapsed the tier rings to one full-density ring
// at the cull" - ProcessGrassChunks and the harness (SET_GRASSLOD reply) must agree. Only with the DX11
// density (a constant 100k per chunk would grow with R^2) and no explicit SET_GRASS lodchunks/tier3/tier2.
static bool GrassFarRingCollapsed()
{
	return gg_grass_far_mode != 0 && g_grassDensityDX11Latched && g_grassLODChunksOverride <= 0.0f
		&& g_grassTier3Chunks <= 0.0f && g_grassTier2Chunks <= 0.0f;
}
bool GGGrass_FarRingCollapsed() { return GrassFarRingCollapsed(); }
static const int GG_GRASS_DENSITY_SETTLE = 15;
unsigned int g_dbgGrassDensityStale = 0;      // chunks queued for a density rebuild by the last pass
unsigned int g_dbgGrassDensityLatches = 0;    // how many times the latch moved

// What the live inputs ask for right now (unlatched). nearStrands = strands per chunk of stride
// `chunkStride` at full density, BEFORE the low-VRAM multiplier.
static void GrassDensityTarget( float chunkStride, float& nearStrands, float& lowVramMul )
{
	float base = 100000.0f;   // the pre-3.116 DX12 constant
	if ( gg_grass_density_dx11 && chunkStride > 0.0f )
	{
		// the RAW slider, not GGGrass_LodDistEffective(): the low-VRAM distance cap must never RAISE density
		const float R = (float)GGGrass::gggrass_global_params.lod_dist + 2500.0f;
		base = 56250.0f * chunkStride * chunkStride / ( R * R );
	}
	nearStrands = base * (float)std::max( 1, gg_grass_density_pct ) / 100.0f;
	// ★ AFTER the percentage (review 3.116): a 4 GB card never gets more grass than before 3.116, even on
	// a level its author saved above 100% - the low-VRAM preset already overrides the author's draw
	// distance, and this is the same promise for density. (x0.75 below still applies on top.)
	if ( gg_lowvram ) nearStrands = std::min( nearStrands, 100000.0f );
	lowVramMul = ( gg_lowvram && gg_lowvram_grass_density > 0.0f && gg_lowvram_grass_density < 1.0f ) ? gg_lowvram_grass_density : 1.0f;
}

// For the panel / harness: DX11's near density at the level's draw distance, per square metre.
float GGGrass_DX11DensityPerM2()
{
	const float R = (float)GGGrass::gggrass_global_params.lod_dist + 2500.0f;
	return 56250.0f / ( R * R ) * 1550.0031f;
}
// Harness GET_GRASSDENSITY: the latched near strands per chunk, the low-VRAM multiplier, the strands
// per system for each tier, and the bookkeeping counters.
void GGGrass_GetDensityState( float* nearStrands, float* lowMul, unsigned int* t3, unsigned int* t2, unsigned int* t1,
	unsigned int* stale, unsigned int* latches );
// ...and what this build lays near the camera now (latched), per square metre of painted ground.
float GGGrass_NearDensityPerM2()
{
	const float stride = ( wi::terrain::chunk_width - 1 ) * 80.0f;   // GG terrain: chunk_scale 80
	const float n = std::min( (float)GG_GRASS_MAX_STRANDS, std::max( 1024.0f, g_grassNearStrands * g_grassLowVramMul ) );
	return n / ( stride * stride ) * 1550.0031f;
}

// The effective grass draw distance in inches. Every consumer of lod_dist must go through this,
// otherwise the per-strand cull and the chunk-creation ring disagree and grass pops in as whole
// chunks (the exact failure the decoupling comment further down exists to prevent).
float GGGrass_LodDistEffective()
{
	const float slider = GGGrass::gggrass_global_params.lod_dist;
	if (gg_lowvram && gg_lowvram_grass_dist > 0.0f && slider > gg_lowvram_grass_dist)
		return gg_lowvram_grass_dist;
	return slider;
}
namespace wi::terrain { extern uint32_t gg_terrain_tile_share_mips, gg_terrain_tile_hold_mips; } // engine 1.53b/c VT tiling cap + hold (wiTerrain.cpp)
// Set by SET_GRASSLOD / tier-knob changes so the gated grass pass re-evaluates tiers without
// waiting for a camera move or chunk churn (consumed by the maintenance block each frame).
bool g_grassPassNudge = false;
// While a tier-boundary SHRINK sweep is in flight (LOD toggled off -> AUTO boundaries pull in),
// the downgrade hysteresis must yield — otherwise its 0.5-chunk anti-wobble margin holds the
// fat tiers FOREVER at a parked camera (measured: stuck at 2.46M strands / 48 FPS after an
// OFF toggle). Set alongside the nudge; cleared when a pass completes with nothing demoted.
static bool g_grassTierShrinkPending = false;
static bool g_grassRebuildRequested = false;

// Brush cursor: a single DecalComponent entity that projects Files/editors/gfx/brush_ring.png down
// onto whatever's below it (terrain). Mirrors the legacy GG terrain-shader procedural circle.
// The entity is created lazily on first SetBrushCursor call (needs wickedTerrainExeDir + device).
// Hidden by zeroing the material baseColor alpha; the entity stays in the scene for reuse.
static wi::ecs::Entity g_brushCursorEntity = wi::ecs::INVALID_ENTITY;
static bool            g_brushCursorSetup = false;

// Census of chunks awaiting in-place regeneration (invalidated flag up). Written by the
// chunkSig loop each frame and primed directly by GGTerrainWicked_InvalidateRegion so a
// mass invalidation gets the turbo generation budget on its very first frame.
static size_t s_pendingRegenCount = 0;

// Blendmap VT refresh mode for the current blend-pass batch. Set by each pass from its
// pending count: small batches (interactive brush strokes, <= a max-size brush's chunk
// footprint) use the resident-tile repaint fast path (next-frame visible); big batches
// (level-load / mass regen streams of 64) use the classic invalidate() so hundreds of
// chunks don't re-render their entire resident tile sets every frame.
static bool g_blendRepaintFastPath = false;

// Live diagnostics for GET_PERF_DATA (TERRAIN_DEBUG line) — non-static so the
// automation harness can extern them. Cheap counters, always on.
uint64_t g_dbgBridgeCalls = 0;        // GGTerrainWicked_InvalidateRegion invocations
uint64_t g_dbgBridgeChunksMarked = 0; // chunks marked invalidated by the bridge
uint64_t g_dbgBridgeKeysErased = 0;   // processed-keys erased by the bridge
uint64_t g_dbgAutoBlendChunks = 0;    // chunks processed by ApplyDX11StyleAutoBlend
uint64_t g_dbgPaintBlendChunks = 0;   // chunks processed by ProcessPaintedChunkBlendmaps
size_t   g_dbgInvalidatedCensus = 0;  // chunks with invalidated flag up (last sig loop)
size_t   g_dbgMergePendingCensus = 0; // chunks with merge_pending up (last sig loop)
uint64_t g_dbgAutoSkipNoChunk = 0;    // auto pass: entity not found in terrain->chunks
uint64_t g_dbgAutoSkipNoLayers = 0;   // auto pass: blendmap_layers empty
uint64_t g_dbgAutoSkipInvalid = 0;    // auto pass: invalidated flag up
uint64_t g_dbgAutoSkipMergePend = 0;  // auto pass: merge_pending flag up
uint64_t g_dbgAutoPassRuns = 0;       // auto pass invocations (gate fires)
size_t   g_dbgAutoLastPending = 0;    // auto pass: pending size on last run

// GGMAX 2.25: `setup.ini terraingen=<N>` overrides wi::terrain's chunk ring radius, whose
// shipped value is set at GGTerrainWicked_Init (see the terrain.generation assignment). Every
// ring costs (2N+1)^2 chunk entities, each one a mesh + material + transform + hierarchy node
// walked by Scene::Update EVERY frame — so this is the single biggest lever on the DX12 entity
// floor (SWITCHESCAPE_PERF.md §16). Needs the early setup.ini pass: the terrain is created once
// by GGTerrainWicked_Init, called from GameGuruMain.cpp's init sequence, and nothing re-reads
// the value afterwards. 0 = leave the built-in default alone.
// ⚠ Lower is NOT free: the ring radius is what covers the map. See GGTerrainRingCoverage().
int g_terrainGenOverride = 0;
void GGSetTerrainGen(int n)
{
	// Clamp to the range the engine's chunk LOD/removal maths stays sane over. 2 is below any
	// usable map; 24 is 49x49 = 2401 chunks, already far past the point of diminishing returns.
	if (n < 0) n = 0;
	if (n > 24) n = 24;
	if (n != 0 && n < 2) n = 2;
	g_terrainGenOverride = n;
}

// GGMAX terrain idle gate (perf): when the terrain is fully quiescent (camera parked,
// no pending/invalidated/merge-pending chunks, chunk set stable, no edits) the engine
// Generation_Update ring scan (~0.9ms/frame) runs only every 8th frame. ANY activity
// signal restores full rate the same frame. Harness: SET_TERRAINIDLE 0|1.
bool     g_terrainIdleGate = true;
uint64_t g_dbgIdleGateSkips = 0;      // Generation_Update calls skipped by the idle gate
uint32_t g_dbgIdleCalmFrames = 0;     // consecutive quiescent frames (0 = active)
static bool s_terrainActivityPing = false; // set by edit/paint entry points, consumed in Update

// Grass-hair lifecycle diagnostics for the shadow-flicker investigation (GET_PERF_DATA).
// Trigger-1 confirmation: when a terrain chunk's scene-object entity is recycled/removed by
// Wicked streaming, its grass HairParticleSystem's meshID points at a vanished MeshComponent
// until ProcessGrassChunks re-detects the change and rebuilds. The hair simulate pass silently
// skips a hair whose meshID mesh is missing (wiRenderer.cpp GetComponent==null), so that chunk's
// grass — and its shadow (shadow pass reuses the camera-culled hair set) — vanish together for
// the gap. A slow, state-dependent flicker == chunks cycling through that gap.
uint64_t g_dbgGrassRecycles    = 0;   // ProcessGrassChunks: chunk-object entity changed (recycle) events
uint64_t g_dbgGrassFullResets  = 0;   // fullReset removals (old hair torn down before recreate)
uint64_t g_dbgGrassRecreates   = 0;   // grass hair entities (re)created
size_t   g_dbgGrassDeadMeshNow = 0;   // live GG-grass hairs whose meshID mesh is GONE this frame (>0 == flicker)
uint64_t g_dbgGrassExternalKills = 0; // record held only DEAD entities (in-place chunk regen killed them) - repaired + re-queued

static uint64_t MakeChunkKey(int32_t cx, int32_t cz)
{
	return ((uint64_t)(uint32_t)cx << 32) | (uint64_t)(uint32_t)cz;
}

// Helper to get the terrain component from the scene (registered, not static)
static wi::terrain::Terrain* GetWickedTerrain()
{
	if (wickedTerrainEntity == wi::ecs::INVALID_ENTITY) return nullptr;
	return wi::scene::GetScene().terrains.GetComponent(wickedTerrainEntity);
}

// Height modifier that feeds GameGuru's terrain height data into Wicked Engine chunks
struct GGHeightModifier : public wi::terrain::Modifier
{
	float bottomLevel = -20000.0f;
	float topLevel = 20000.0f;

	GGHeightModifier()
	{
		type = Type::Heightmap;
		blend = BlendMode::Normal;
		weight = 1.0f;
	}

	void Apply(const XMFLOAT2& world_pos, float& height) override
	{
		// world_pos.x = world X, world_pos.y = world Z
		float worldY = GGTerrain::GGTerrain_CalculateHeight(world_pos.x, world_pos.y);
		// Convert world height to 0-1 range for Wicked's lerp(bottomLevel, topLevel, height)
		float range = topLevel - bottomLevel;
		if (range > 0.0f)
			height = (worldY - bottomLevel) / range;
		else
			height = 0.5f;
	}
};

static std::shared_ptr<GGHeightModifier> heightModifier;

// Helper: set up a terrain material entity following the Wicked Editor pattern.
// Sets texture names, PBR properties, and calls CreateRenderData() for synchronous loading.
// Textures must be valid before Generation_Restart() because Generation_Update() checks
// resource.IsValid() on the same frame to decide whether to run the VT pipeline.
static void SetupTerrainMaterial(wi::scene::Scene& scene, wi::ecs::Entity entity, int ggMatIndex)
{
	using namespace wi::scene;

	// GG material index is 0-based, folder names are 1-based (mat1..mat32)
	int folderNum = (ggMatIndex & 0xFF) + 1;

	// Build paths from EXE directory (CWD-independent)
	char colorPath[512], normalPath[512], surfacePath[512];
	// the level's own texture for the slot first (a palette folder chosen in the editor, visuals.TerrainTextures): the
	// GameGuru terrain always loaded those per slot, so a level painted with its palette's asphalt showed the matN
	// folder's snow here
	extern const char* GGGame_TerrainSlotTexture(int iSlot);
	const char* pOwn = GGGame_TerrainSlotTexture(ggMatIndex & 0xFF);
	if (pOwn && pOwn[0])
	{
		strcpy_s(colorPath, pOwn);
		// the folder's Normal.dds and Surface.dds beside its Color.dds, whatever the colour file is called
		char folder[512];
		strcpy_s(folder, pOwn);
		char* pSlash = strrchr(folder, '\\');
		if (!pSlash) pSlash = strrchr(folder, '/');
		if (pSlash) pSlash[1] = 0; else folder[0] = 0;
		sprintf_s(normalPath, "%sNormal.dds", folder);
		sprintf_s(surfacePath, "%sSurface.dds", folder);
	}
	else
	{
		sprintf_s(colorPath, "%s/Files/terraintextures/mat%d/Color.dds", wickedTerrainExeDir.c_str(), folderNum);
		sprintf_s(normalPath, "%s/Files/terraintextures/mat%d/Normal.dds", wickedTerrainExeDir.c_str(), folderNum);
		sprintf_s(surfacePath, "%s/Files/terraintextures/mat%d/Surface.dds", wickedTerrainExeDir.c_str(), folderNum);
	}
	GGTerrain::GGTerrain_RecordSlotTextureSource(ggMatIndex & 0xFF, 0, colorPath);
	GGTerrain::GGTerrain_RecordSlotTextureSource(ggMatIndex & 0xFF, 1, normalPath);
	GGTerrain::GGTerrain_RecordSlotTextureSource(ggMatIndex & 0xFF, 2, surfacePath);
	// the writable folder's copy ahead of the install's, as the GameGuru terrain, grass and trees load them (GG_GetRealPath)
	GG_GetRealPath(colorPath, 0);
	GG_GetRealPath(normalPath, 0);
	GG_GetRealPath(surfacePath, 0);

	MaterialComponent* mat = scene.materials.GetComponent(entity);
	if (!mat)
	{
		mat = &scene.materials.Create(entity);
	}

	mat->textures[MaterialComponent::BASECOLORMAP].name = colorPath;
	mat->textures[MaterialComponent::NORMALMAP].name = normalPath;
	// GG Surface.dds uses R=AO, G=Roughness, B=Metalness, A=255 — matches Wicked's SURFACEMAP
	// convention (R=occlusion, G=roughness, B=metalness, A=reflectance). A=255 means reflectance
	// multiplier is 1.0, so the base reflectance value set below is used directly.
	mat->textures[MaterialComponent::SURFACEMAP].name = surfacePath;

	// PBR defaults matching Wicked Editor terrain presets
	mat->SetRoughness(1.0f);
	mat->SetMetalness(0.0f);
	mat->SetReflectance(0.005f);

	mat->SetTextureStreamingDisabled(true);
	mat->CreateRenderData();
}

// Phase 2: Set up 4-layer terrain materials from GG render params
// Called lazily on first update (after level load, so params are correct)
static void SetupWickedTerrainMaterials()
{
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (!terrain) return;

	// 2026-08-05 DEVICE_HUNG guard (Aztec Teaser PLAY GAME, dred 20:13:19): this re-setup
	// drops the previous terrain material textures and ends in Generation_Restart (frees
	// every chunk VT). The SVT "Render Tile Regions" compute pass samples those material
	// textures bindlessly and its request queue drains over several frames — the DRED dump
	// caught it hung mid-dispatch on a page fault whose freed-VA matches were
	// terraintextures/matNN/Normal.dds. Drain the GPU before swapping so nothing in flight
	// can still sample the outgoing textures. This path only runs on texture-set changes
	// (level load) and new painted-slot registration, so the stall is invisible.
	if (wi::graphics::GetDevice() != nullptr)
	{
		wi::graphics::GetDevice()->WaitForGPU();
	}

	auto& scene = wi::scene::GetScene();

	// 2026-08-05 DEVICE_HUNG FIX — faulted resource NAMED by keep-alive bisection (see
	// PLAYGAME_CRASH_2026-08-05.md): the SVT tile-render CS can still sample the OUTGOING
	// set's DDS textures after this swap through a stale GPU-side ShaderMaterial descriptor
	// (persists across even a full WaitForGPU — it is data, not in-flight work; pinning
	// ONLY terraintextures/* resources took the soak from ~85% hang-per-cycle to 0/6).
	// Retain the outgoing set's Resources until the NEXT set swap so they can never die
	// mid-play. Cost: one extra material set (~a few MB), released on the next level load.
	{
		static wi::vector<wi::Resource> gg_prevMaterialSetRetention;
		wi::vector<wi::Resource> outgoing;
		for (size_t i = 0; i < terrain->materialEntities.size(); ++i)
		{
			wi::scene::MaterialComponent* m = scene.materials.GetComponent(terrain->materialEntities[i]);
			if (m == nullptr) continue;
			for (int t = 0; t < wi::scene::MaterialComponent::TEXTURESLOT_COUNT; ++t)
			{
				if (m->textures[t].resource.IsValid()) outgoing.push_back(m->textures[t].resource);
			}
		}
		gg_prevMaterialSetRetention = std::move(outgoing); // the set from TWO swaps ago releases here
	}

	// Read material indices from GG render params
	int baseMat = GGTerrain::ggterrain_global_render_params.baseLayerMaterial & 0xFF;
	int slopeMat = GGTerrain::ggterrain_global_render_params.slopeMatIndex[0] & 0xFF;
	int lowMat = GGTerrain::ggterrain_global_render_params.layerMatIndex[0] & 0xFF;
	int highMat = GGTerrain::ggterrain_global_render_params.layerMatIndex[2] & 0xFF;

	// Create material entities and attach to terrain (following Wicked Editor pattern).
	// GGMAX 2.63c: FRESH entities on every re-setup — REUSING the old entity kept its slot
	// in the GPU-side ShaderMaterial array, and the SVT tile-render CS reads that data on
	// its own refresh cadence, so tiles baked in the first seconds after a biome swap
	// sampled the OUTGOING material set through the stale entries (the 2026-08-05
	// DEVICE_HUNG post-mortem's exact mechanism, benign-rendering edition: snow swap with
	// the centre cone baked in rainforest's mat20 olive — blendmaps were PROVEN correct,
	// L3=100% onto s3=mat13 snow, yet the tile rendered the slot's PREVIOUS material).
	// A fresh entity gets a fresh array slot resolved at bake time — no staleness window.
	// The outgoing entities are removed (same idiom as the stale-tail truncation below);
	// their textures stay alive in gg_prevMaterialSetRetention so in-flight GPU work and
	// stale descriptors can never fault (the 08-05 crash rule).
	for (int i = 0; i < wi::terrain::MATERIAL_COUNT; i++)
	{
		wi::ecs::Entity old = terrain->materialEntities[i];
		terrain->materialEntities[i] = wi::ecs::CreateEntity();
		scene.Component_Attach(terrain->materialEntities[i], wickedTerrainEntity);
		if (old != wi::ecs::INVALID_ENTITY)
		{
			scene.Entity_Remove(old);
		}
	}

	// Configure each material with texture names and properties
	SetupTerrainMaterial(scene, terrain->materialEntities[wi::terrain::MATERIAL_BASE], baseMat);
	SetupTerrainMaterial(scene, terrain->materialEntities[wi::terrain::MATERIAL_SLOPE], slopeMat);
	SetupTerrainMaterial(scene, terrain->materialEntities[wi::terrain::MATERIAL_LOW_ALTITUDE], lowMat);
	SetupTerrainMaterial(scene, terrain->materialEntities[wi::terrain::MATERIAL_HIGH_ALTITUDE], highMat);

	// Region parameters control height/slope-based material blending.
	// Wicked uses smoothstep(0, regionN, value) where:
	//   slope_amount = 1 - normal.y (0=flat, 1=vertical)
	//   low_alt = InverseLerp(0, bottomLevel, height) — increases as height goes below 0
	//   high_alt = InverseLerp(0, topLevel, height) — increases as height goes above 0
	// GG uses independent start/end thresholds per layer which don't map exactly, so these
	// are approximate mappings that give visually similar results.
	// Wicked's built-in slope/altitude weights are overwritten every frame by
	// ApplyDX11StyleAutoBlend below; these region values are left at safe defaults so any
	// vertex the DX11 override misses (should be none) still gets a plausible base blend.
	terrain->region1 = GGTerrain::ggterrain_global_render_params.slopeEnd[0];
	terrain->region2 = 2.0f;
	terrain->region3 = 1.0f;

	// Phase 3: Initialize materialToSlot lookup — maps GG 0-based material index to Wicked blendmap layer
	for (int i = 0; i < GGTERRAIN_MAX_SOURCE_TEXTURES; i++)
		materialToSlot[i] = -1;

	// Map the 4 auto materials to their fixed slots (0-3)
	materialToSlot[baseMat] = wi::terrain::MATERIAL_BASE;
	materialToSlot[slopeMat] = wi::terrain::MATERIAL_SLOPE;
	materialToSlot[lowMat] = wi::terrain::MATERIAL_LOW_ALTITUDE;
	materialToSlot[highMat] = wi::terrain::MATERIAL_HIGH_ALTITUDE;
	maxPaintedSlot = wi::terrain::MATERIAL_HIGH_ALTITUDE;  // 3

	// Path A: register GG layer[1]'s material as slot 4 so the CPU-computed DX11 blend can
	// place its weight there. This is the "mid altitude" band material (heights 180-360 in
	// TESTPRO1 island) that DX11 fully REPLACES base with above height 360 — Wicked's 4-slot
	// auto blend has no equivalent. Skip if the layer[1] material already occupies one of
	// the 4 auto slots (nothing to add).
	int layer1Mat = GGTerrain::ggterrain_global_render_params.layerMatIndex[1] & 0xFF;
	g_layer1MaterialSlot = -1;
	int numExtraMaterials = 0;
	if (materialToSlot[layer1Mat] < 0)
	{
		int newSlot = wi::terrain::MATERIAL_COUNT;  // 4
		materialToSlot[layer1Mat] = newSlot;
		while ((int)terrain->materialEntities.size() <= newSlot)
			terrain->materialEntities.push_back(wi::ecs::INVALID_ENTITY);
		// GGMAX 3.65 VRAM LEAK: this slot may already hold an entity from the PREVIOUS level.
		// Overwriting the vector entry orphans it - the entity stays in the scene, its
		// MaterialComponent stays alive, and its three terraintextures/matNN DDS resources stay
		// resident for the rest of the process. Measured: Operation Amazon alone loads 11 terrain
		// materials; after one visit to RPG Template it holds 20, including mat14-mat28 which it
		// does not use (+142 MB). The 08-05 tail truncation below removes slots beyond the new
		// SIZE but never the ones overwritten in place. Remove after creating the replacement,
		// same idiom as the four auto slots above; the outgoing textures are already pinned in
		// gg_prevMaterialSetRetention for one more swap, so nothing in flight can fault.
		wi::ecs::Entity oldLayer1 = terrain->materialEntities[newSlot];
		terrain->materialEntities[newSlot] = wi::ecs::CreateEntity();
		if (oldLayer1 != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(oldLayer1);
		scene.Component_Attach(terrain->materialEntities[newSlot], wickedTerrainEntity);
		SetupTerrainMaterial(scene, terrain->materialEntities[newSlot], layer1Mat);
		maxPaintedSlot = newSlot;
		g_layer1MaterialSlot = newSlot;
		numExtraMaterials = 1;
	}
	else
	{
		g_layer1MaterialSlot = materialToSlot[layer1Mat];
	}

	// Scan material map for unique painted materials not already in auto slots
	const uint8_t* matMap = GGTerrain::GGTerrain_GetMaterialMapPtr();
	int mapRes = GGTerrain::GGTerrain_GetMaterialMapResolution();

	if (matMap && mapRes > 0)
	{
		bool usedMaterials[GGTERRAIN_MAX_SOURCE_TEXTURES] = {};
		for (int i = 0; i < mapRes * mapRes; i++)
		{
			uint8_t val = matMap[i];
			if (val > 0 && val <= GGTERRAIN_MAX_SOURCE_TEXTURES)
				usedMaterials[val - 1] = true;
		}

		// Create material entities for painted materials beyond the 4 auto slots
		for (int i = 0; i < GGTERRAIN_MAX_SOURCE_TEXTURES; i++)
		{
			if (usedMaterials[i] && materialToSlot[i] < 0)
			{
				int newSlot = wi::terrain::MATERIAL_COUNT + numExtraMaterials;
				materialToSlot[i] = newSlot;

				// Extend materialEntities vector if needed
				while ((int)terrain->materialEntities.size() <= newSlot)
					terrain->materialEntities.push_back(wi::ecs::INVALID_ENTITY);

				// GGMAX 3.65 VRAM LEAK: see the note at the layer1 slot above - the painted slots
				// leak the same way, and there are many more of them.
				wi::ecs::Entity oldPainted = terrain->materialEntities[newSlot];
				terrain->materialEntities[newSlot] = wi::ecs::CreateEntity();
				if (oldPainted != wi::ecs::INVALID_ENTITY) scene.Entity_Remove(oldPainted);
				scene.Component_Attach(terrain->materialEntities[newSlot], wickedTerrainEntity);
				SetupTerrainMaterial(scene, terrain->materialEntities[newSlot], i);

				if (newSlot > maxPaintedSlot)
					maxPaintedSlot = newSlot;
				numExtraMaterials++;
			}
		}
	}

	// 2026-08-05 DEVICE_HUNG structural fix: a previous material set with MORE painted
	// materials leaves stale tail slots in materialEntities. Their components/textures die
	// with the old level, but chunk blendmap arrays still span those layers, so the SVT
	// tile-render CS resolved GetIndex(dead entity) = SIZE_MAX -> out-of-bounds
	// ShaderMaterial -> garbage texture descriptor -> GPU page fault (mat18 in every dump).
	// Truncate to the new set and remove the stale entities (GPU already drained above).
	{
		int newSize = wi::terrain::MATERIAL_COUNT + numExtraMaterials;
		while ((int)terrain->materialEntities.size() > newSize)
		{
			wi::ecs::Entity stale = terrain->materialEntities.back();
			terrain->materialEntities.pop_back();
			if (stale != wi::ecs::INVALID_ENTITY)
			{
				scene.Entity_Remove(stale);
			}
		}
	}

	// Reset blendmap tracking so all chunks get reprocessed with new materials
	processedChunkKeys.clear();
	chunkKeyToEntity.clear();
	dx11BlendProcessedKeys.clear();
	dx11BlendChunkKeyToEntity.clear();

	// Restart generation to pick up all materials (auto + painted)
	// Generation_Restart() deep-copies the materials internally
	terrain->Generation_Restart();
	wickedTerrainMaterialsSetup = true;

	wi::backlog::post(std::string("GGTerrainWicked: materials setup complete (" +
		std::to_string(numExtraMaterials) + " extra painted materials, maxSlot=" +
		std::to_string(maxPaintedSlot) + ")").c_str());
}

// Register a blendmap slot for a material the user just started painting with,
// WITHOUT the full re-setup + Generation_Restart (that path tears down and rebuilds
// the entire island — the visible blur/glitch on the first stroke with each new
// texture). Safe because the VT tile renderer resolves layer materials LIVE from
// the scene (wiTerrain UpdateVirtualTexturesGPU reads
// scene->materials.GetIndex(materialEntities[i]) per render) and per-chunk blendmap
// layers grow on demand when the painted pass writes them. The generator's internal
// material snapshot only feeds newly-generated chunk DEFAULTS, which our passes
// overwrite anyway; the next Generation_Restart re-snapshots everything.
static void RegisterPaintedMaterialSlot(int matIndex)
{
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (terrain == nullptr || !wickedTerrainMaterialsSetup)
	{
		// no live material set to extend — fall back to the full setup path
		wickedTerrainMaterialsSetup = false;
		return;
	}

	auto& scene = wi::scene::GetScene();
	int newSlot = (int)terrain->materialEntities.size();

	terrain->materialEntities.push_back(wi::ecs::CreateEntity());
	scene.Component_Attach(terrain->materialEntities[newSlot], wickedTerrainEntity);
	SetupTerrainMaterial(scene, terrain->materialEntities[newSlot], matIndex);

	materialToSlot[matIndex] = newSlot;
	if (newSlot > maxPaintedSlot)
		maxPaintedSlot = newSlot;

	wi::backlog::post(std::string("GGTerrainWicked: registered painted material " +
		std::to_string(matIndex) + " -> slot " + std::to_string(newSlot) +
		" (incremental, no restart)").c_str());
}

// Path A: recompute per-vertex blendmap weights using DX11's terrain shader formula,
// replacing Wicked's built-in smoothstep(0, regionN, x) blend that we can't reproduce with a
// single scalar threshold. See project_terrain_texture_mismatch.md for the analysis.
//
// DX11 formula (from GGTerrainPageGenPS.hlsl:216-246):
//   finalSurface = base
//   for i in 0..5:
//     t = clamp((height - layer[i].start) / (layer[i].end - layer[i].start), 0, 1)
//     if t >= 1: finalSurface = REPLACE with layer[i]
//     elif t > 0: finalSurface = LERP(finalSurface, layer[i], t)
//   normaly = 1 - abs(normal.y)
//   for i in 0..2:
//     t = clamp((normaly - slope[i].start) / (slope[i].end - slope[i].start), 0, 1)
//     if t >= 1: finalSurface = REPLACE with slope[i]
//     elif t > 0: finalSurface = LERP(finalSurface, slope[i], t)
//
// Translated to Wicked's weight system: track (w_base, w_slope, w_low, w_high, w_layer1)
// and apply each layer/slope as a proportional replace: all_weights *= (1-t), target += t.
// Runs BEFORE ProcessPaintedChunkBlendmaps so painted materials still override correctly.
// Returns true when every discovered chunk was processed; false when the batch
// was capped (call again to drain the backlog).
static bool FillChunkBlendmapGG(wi::terrain::ChunkData& cd, const wi::scene::MeshComponent& mesh); // GGMAX 3.113: below

static bool ApplyDX11StyleAutoBlend(wi::terrain::Terrain* terrain)
{
	if (!terrain) return true;
	auto& scene = wi::scene::GetScene();
	const auto& rp = GGTerrain::ggterrain_global_render_params;
	float chunkStride = (wi::terrain::chunk_width - 1) * terrain->chunk_scale;

	// Pre-compute reciprocal widths of layer/slope ramps.
	float layerRcpWidth[5];
	float layerStart[5];
	for (int i = 0; i < 5; i++)
	{
		layerStart[i] = rp.layerStartHeight[i];
		float w = rp.layerEndHeight[i] - rp.layerStartHeight[i];
		layerRcpWidth[i] = (w > 0.001f) ? (1.0f / w) : 0.0f;
	}
	float slopeRcpWidth[2];
	float slopeStart[2];
	for (int i = 0; i < 2; i++)
	{
		slopeStart[i] = rp.slopeStart[i];
		float w = rp.slopeEnd[i] - rp.slopeStart[i];
		slopeRcpWidth[i] = (w > 0.001f) ? (1.0f / w) : 0.0f;
	}

	// Which Wicked slots each GG material family lands in.
	int baseSlot  = wi::terrain::MATERIAL_BASE;           // baseLayerMaterial (typically mat 17 grass)
	int slopeSlot = wi::terrain::MATERIAL_SLOPE;          // slopeMatIndex[0]   (typically mat 4 rock)
	int lowSlot   = wi::terrain::MATERIAL_LOW_ALTITUDE;   // layerMatIndex[0]   (mat 2 sand on TESTPRO1)
	int highSlot  = wi::terrain::MATERIAL_HIGH_ALTITUDE;  // layerMatIndex[2]   (mat 20 rock on TESTPRO1)
	int layer1Slot = g_layer1MaterialSlot;                 // layerMatIndex[1]   (mat 0 mid-material) or -1

	// Phase 1: Scan scene.objects (main-thread safe) to collect chunks that haven't had DX11 blend applied
	struct PendingChunk {
		wi::ecs::Entity entity;
		int32_t cx, cz;
		wi::scene::MeshComponent* mesh;
		const wi::scene::TransformComponent* transform;
	};
	wi::vector<PendingChunk> pending;

	for (size_t oi = 0; oi < scene.objects.GetCount(); oi++)
	{
		wi::scene::ObjectComponent& obj = scene.objects[oi];
		wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(obj.meshID);
		if (!mesh || mesh->vertex_positions.size() != wi::terrain::vertexCount) continue;
		if (mesh->vertex_normals.size() != wi::terrain::vertexCount) continue;

		wi::ecs::Entity entity = scene.objects.GetEntity(oi);
		const wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
		if (!transform) continue;

		int32_t cx = (int32_t)std::round(transform->world._41 / chunkStride);
		int32_t cz = (int32_t)std::round(transform->world._43 / chunkStride);
		uint64_t key = MakeChunkKey(cx, cz);

		if (dx11BlendProcessedKeys.count(key))
		{
			// Same regen-detection pattern as ProcessPaintedChunkBlendmaps
			auto it = dx11BlendChunkKeyToEntity.find(key);
			if (it != dx11BlendChunkKeyToEntity.end() && it->second != entity)
			{
				dx11BlendProcessedKeys.erase(key);
				dx11BlendChunkKeyToEntity.erase(it);
			}
			else continue;
		}

		pending.push_back({ entity, cx, cz, mesh, transform });
	}

	if (pending.empty()) return true;

	// Cap the batch so one pass can't stall the frame for hundreds of ms when
	// the initial build delivers chunks faster than we process them. The gate
	// keeps calling until we report caught-up.
	bool caughtUp = true;
	if (pending.size() > 64)
	{
		pending.resize(64);
		caughtUp = false;
	}
	// interactive-scale batch? (a max-size brush spans at most a 4x4 chunk footprint)
	g_blendRepaintFastPath = caughtUp && pending.size() <= 16;
	g_dbgAutoPassRuns++;
	g_dbgAutoLastPending = pending.size();

	// Phase 2: Cancel generation for safe chunk data access, then rewrite blendmaps
	terrain->Generation_Cancel();

	int chunksModified = 0;
	int slotsNeeded = std::max(4, (layer1Slot >= 0 ? layer1Slot + 1 : 4));

	// POST-LOAD DIP FIX 2026-07-29: entity -> chunk-data map built once per pass.
	// The old per-pending linear walk of terrain->chunks was O(pending x chunks)
	// (64 pending x ~700 chunks every frame at the load-tail peak).
	std::unordered_map<wi::ecs::Entity, wi::terrain::ChunkData*> entityToChunkData;
	entityToChunkData.reserve(terrain->chunks.size());
	for (auto& [chunk, cd] : terrain->chunks) entityToChunkData[cd.entity] = &cd;

	for (auto& pc : pending)
	{
		uint64_t key = MakeChunkKey(pc.cx, pc.cz);
		wi::terrain::ChunkData* chunk_data = nullptr;
		auto itCD = entityToChunkData.find(pc.entity);
		if (itCD != entityToChunkData.end()) chunk_data = itCD->second;
		if (!chunk_data) { g_dbgAutoSkipNoChunk++; continue; }
		if (chunk_data->blendmap_layers.empty()) { g_dbgAutoSkipNoLayers++; continue; }  // generation not finished yet
		if (chunk_data->invalidated) { g_dbgAutoSkipInvalid++; continue; }  // pending regen would discard this work — retry after
		if (chunk_data->merge_pending) { g_dbgAutoSkipMergePend++; continue; }  // regenerated but main-scene mesh still stale — retry after merge

		if (chunk_data->gg_blendmap_generated)
		{
			// Born-correct chunk (delta 1.17): the generator-thread callback already wrote
			// these exact weights and the GPU texture was built from them — just latch the
			// key. The flag stays up; only the edit bridge clears it (real edits reprocess).
			dx11BlendProcessedKeys.insert(key);
			dx11BlendChunkKeyToEntity[key] = pc.entity;
			continue;
		}

		dx11BlendProcessedKeys.insert(key);
		dx11BlendChunkKeyToEntity[key] = pc.entity;

		// Ensure enough blendmap layers exist for all slots we might write to
		for (int i = 0; i < slotsNeeded; i++)
			chunk_data->enable_blendmap_layer(i);

		// GGMAX 3.113: write the COMPLETE, paint-aware weight set - the same writer the generator thread
		// uses for born-correct chunks (FillChunkBlendmapGG: every layer zeroed, a painted vertex wins
		// outright, else the DX11 auto weights). The loop below wrote only the five auto slots and left
		// a painted slot as it was. The stock decal stack hid that (the top slot won), but under the
		// splat compositor a stale 255 left by Restore, a paint undo, or a re-blend wider than the
		// painted pass's 64-chunk window averaged to a 50/50 mix of paint and auto ground (review
		// 3.113). One writer = the pass order no longer matters. The old loop stays as the fallback
		// for a chunk FillChunkBlendmapGG refuses (materials not set up yet, odd mesh).
		if (!FillChunkBlendmapGG(*chunk_data, *pc.mesh))
		{
			XMMATRIX worldMatrix = XMLoadFloat4x4(&pc.transform->world);

			for (size_t vi = 0; vi < wi::terrain::vertexCount; vi++)
			{
				XMVECTOR wp = XMVector3Transform(XMLoadFloat3(&pc.mesh->vertex_positions[vi]), worldMatrix);
				XMFLOAT3 worldPos;
				XMStoreFloat3(&worldPos, wp);
				float height = worldPos.y;

				const XMFLOAT3& normal = pc.mesh->vertex_normals[vi];
				float normaly = 1.0f - fabsf(normal.y);

				// Per-slot weight accumulator. We only care about the 5 auto slots; anything else
				// stays zero and will be overwritten by ProcessPaintedChunkBlendmaps if painted.
				float w[8] = { 0 };
				w[baseSlot] = 1.0f;

				// Layer 0: replace/blend at height ramp
				// (targetSlot bounds guard: the auto slots are 0-4 today, but slot mappings
				// have grown dynamic — an out-of-range slot must never scribble the stack)
				auto applyLayer = [&](int layerIdx, int targetSlot)
				{
					if (targetSlot < 0 || targetSlot >= 8) return;
					if (layerRcpWidth[layerIdx] == 0.0f) return; // start == end -> unused
					float t = (height - layerStart[layerIdx]) * layerRcpWidth[layerIdx];
					if (t <= 0.0f) return;
					if (t > 1.0f) t = 1.0f;
					for (int s = 0; s < 8; s++) w[s] *= (1.0f - t);
					w[targetSlot] += t;
				};
				auto applySlope = [&](int slopeIdx, int targetSlot)
				{
					if (targetSlot < 0 || targetSlot >= 8) return;
					if (slopeRcpWidth[slopeIdx] == 0.0f) return;
					float t = (normaly - slopeStart[slopeIdx]) * slopeRcpWidth[slopeIdx];
					if (t <= 0.0f) return;
					if (t > 1.0f) t = 1.0f;
					for (int s = 0; s < 8; s++) w[s] *= (1.0f - t);
					w[targetSlot] += t;
				};

				// DX11 iteration order: all layers, then all slopes.
				applyLayer(0, lowSlot);
				if (layer1Slot >= 0) applyLayer(1, layer1Slot);
				applyLayer(2, highSlot);
				// layers 3, 4 in TESTPRO1 have start==end (59055 marker) -> no-op via layerRcpWidth==0
				applyLayer(3, highSlot);
				applyLayer(4, highSlot);
				applySlope(0, slopeSlot);
				applySlope(1, slopeSlot);

				chunk_data->blendmap_layers[baseSlot ].pixels[vi] = (uint8_t)(w[baseSlot ] * 255.0f);
				chunk_data->blendmap_layers[slopeSlot].pixels[vi] = (uint8_t)(w[slopeSlot] * 255.0f);
				chunk_data->blendmap_layers[lowSlot  ].pixels[vi] = (uint8_t)(w[lowSlot  ] * 255.0f);
				chunk_data->blendmap_layers[highSlot ].pixels[vi] = (uint8_t)(w[highSlot ] * 255.0f);
				if (layer1Slot >= 0 && layer1Slot < (int)chunk_data->blendmap_layers.size())
					chunk_data->blendmap_layers[layer1Slot].pixels[vi] = (uint8_t)(w[layer1Slot] * 255.0f);
			}
		}

		// Rebuild the GPU blendmap texture and refresh the VT. For INTERACTIVE edits,
		// residency-backed (near) chunks take the repaint fast path — resident tiles
		// re-render next frame with the new blendmap; a full invalidate() would
		// re-stream the chunk through several seconds of GPU-feedback round-trips
		// (the visible paint lag). During the LOAD/mass bulk passes the fast path is
		// poison: hundreds of chunks per batch, each re-rendering its whole resident
		// tile set per frame = GPU storm (Island Showdown loaded at 6.7 FPS with the
		// entire terrain stuck on the default grey blend) — those keep invalidate().
		chunk_data->blendmap = {};
		terrain->CreateChunkRegionTexture(*chunk_data);
		if (chunk_data->vt)
		{
			if (g_blendRepaintFastPath && chunk_data->vt->residency != nullptr && chunk_data->vt->resolution != 0)
				chunk_data->vt->pending_repaint_blendmap = true;
			else
				chunk_data->vt->invalidate();
		}

		chunksModified++;
	}

	g_dbgAutoBlendChunks += chunksModified;
	if (chunksModified > 0)
	{
		wi::backlog::post(std::string("GGTerrainWicked: DX11-style auto blend on " +
			std::to_string(chunksModified) + " chunks (layer1Slot=" +
			std::to_string(layer1Slot) + ")").c_str());
	}
	return caughtUp;
}

// Phase 3: Process terrain chunks for painted material blendmaps.
// Iterates scene.objects (main-thread safe) to find terrain chunks, then
// cancels generation once for safe chunk data access and processes the batch.
// Returns true when every discovered chunk was processed (see AutoBlend).
static bool ProcessPaintedChunkBlendmaps(wi::terrain::Terrain* terrain)
{
	const uint8_t* matMap = GGTerrain::GGTerrain_GetMaterialMapPtr();
	if (!matMap) return true;

	int mapRes = GGTerrain::GGTerrain_GetMaterialMapResolution();
	float editableSize = GGTerrain::GGTerrain_GetEditableSize();
	float editableSizeRcp = (editableSize > 0.0f) ? (1.0f / editableSize) : 0.0f;
	// editableSize is already the half-size (area goes from -editableSize to +editableSize)
	float chunkStride = (wi::terrain::chunk_width - 1) * terrain->chunk_scale;
	auto& scene = wi::scene::GetScene();

	// Phase 1: Scan scene.objects (main-thread safe) to collect unprocessed terrain chunks
	struct PendingChunk {
		wi::ecs::Entity entity;
		int32_t cx, cz;
		wi::scene::MeshComponent* mesh;
		const wi::scene::TransformComponent* transform;
	};
	wi::vector<PendingChunk> pending;

	for (size_t oi = 0; oi < scene.objects.GetCount(); oi++)
	{
		wi::scene::ObjectComponent& obj = scene.objects[oi];
		wi::scene::MeshComponent* mesh = scene.meshes.GetComponent(obj.meshID);
		if (!mesh || mesh->vertex_positions.size() != wi::terrain::vertexCount) continue;

		wi::ecs::Entity entity = scene.objects.GetEntity(oi);
		const wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
		if (!transform) continue;

		int32_t cx = (int32_t)std::round(transform->world._41 / chunkStride);
		int32_t cz = (int32_t)std::round(transform->world._43 / chunkStride);
		uint64_t key = MakeChunkKey(cx, cz);

		// Check if chunk center is within editable area
		float chunkCenterX = cx * chunkStride;
		float chunkCenterZ = cz * chunkStride;
		bool inEditable = (chunkCenterX >= -editableSize && chunkCenterX <= editableSize &&
			chunkCenterZ >= -editableSize && chunkCenterZ <= editableSize);

		if (processedChunkKeys.count(key))
		{
			// Check if the chunk was removed and recreated (new entity at same position).
			// Wicked Engine removes distant chunks and regenerates them on camera return.
			auto it = chunkKeyToEntity.find(key);
			if (it != chunkKeyToEntity.end() && it->second != entity)
			{
				// Entity changed — chunk was recreated, needs repainting
				processedChunkKeys.erase(key);
				chunkKeyToEntity.erase(it);
			}
			else
			{
				continue;
			}
		}

		// Skip chunks outside editable area (no paint data there)
		if (!inEditable)
		{
			processedChunkKeys.insert(key);
			continue;
		}

		pending.push_back({ entity, cx, cz, mesh, transform });
	}

	if (pending.empty()) return true;

	// Same batch cap as ApplyDX11StyleAutoBlend — the gate re-calls until done.
	bool caughtUp = true;
	if (pending.size() > 64)
	{
		pending.resize(64);
		caughtUp = false;
	}
	// interactive-scale batch? (a max-size brush spans at most a 4x4 chunk footprint)
	g_blendRepaintFastPath = caughtUp && pending.size() <= 16;

	// Phase 2: Cancel generation for safe chunk data access, then process the batch
	terrain->Generation_Cancel();

	int chunksModified = 0;

	// POST-LOAD DIP FIX 2026-07-29: same entity -> chunk-data map as the auto pass
	// (was an O(pending x chunks) linear walk per pending chunk).
	std::unordered_map<wi::ecs::Entity, wi::terrain::ChunkData*> entityToChunkData;
	entityToChunkData.reserve(terrain->chunks.size());
	for (auto& [chunk, cd] : terrain->chunks) entityToChunkData[cd.entity] = &cd;

	for (auto& pc : pending)
	{
		uint64_t key = MakeChunkKey(pc.cx, pc.cz);

		// Find chunk data via the pass-local map (safe after Generation_Cancel)
		wi::terrain::ChunkData* chunk_data = nullptr;
		auto itCD = entityToChunkData.find(pc.entity);
		if (itCD != entityToChunkData.end()) chunk_data = itCD->second;
		if (!chunk_data) continue;  // Don't mark as processed — retry next frame

		// Skip chunks whose blendmap hasn't been generated yet by the pipeline.
		// Painting before generation completes would be overwritten by the default
		// height/slope blending stage. Retry next frame when generation is done.
		if (chunk_data->blendmap_layers.empty()) continue;
		if (chunk_data->invalidated) continue;  // pending regen would discard this work — retry after
		if (chunk_data->merge_pending) continue;  // regenerated but main-scene mesh still stale — retry after merge

		// Mark as processed only after confirming chunk_data exists
		processedChunkKeys.insert(key);
		chunkKeyToEntity[key] = pc.entity;

		if (chunk_data->gg_blendmap_generated)
			continue; // born-correct chunk (delta 1.17): painted weights already included at generation

		XMMATRIX worldMatrix = XMLoadFloat4x4(&pc.transform->world);

		// First pass: check if any vertex is painted
		bool hasPainted = false;
		for (size_t vi = 0; vi < wi::terrain::vertexCount && !hasPainted; vi++)
		{
			XMVECTOR wp = XMVector3Transform(XMLoadFloat3(&pc.mesh->vertex_positions[vi]), worldMatrix);
			XMFLOAT3 worldPos;
			XMStoreFloat3(&worldPos, wp);

			float mapU = worldPos.x * editableSizeRcp * 0.5f + 0.5f;
			float mapV = worldPos.z * editableSizeRcp * 0.5f + 0.5f;
			int mapX = (int)(mapU * mapRes);
			int mapZ = (int)(mapV * mapRes);

			if (mapX >= 0 && mapX < mapRes && mapZ >= 0 && mapZ < mapRes)
			{
				uint8_t matVal = matMap[mapZ * mapRes + mapX];
				if (matVal > 0 && matVal <= GGTERRAIN_MAX_SOURCE_TEXTURES && materialToSlot[matVal - 1] >= 0)
					hasPainted = true;
			}
		}

		if (!hasPainted) continue;

		// Ensure all blendmap layers exist up to maxPaintedSlot
		for (int i = 0; i <= maxPaintedSlot; i++)
			chunk_data->enable_blendmap_layer(i);

		// Second pass: write painted weights into blendmap layers
		for (size_t vi = 0; vi < wi::terrain::vertexCount; vi++)
		{
			XMVECTOR wp = XMVector3Transform(XMLoadFloat3(&pc.mesh->vertex_positions[vi]), worldMatrix);
			XMFLOAT3 worldPos;
			XMStoreFloat3(&worldPos, wp);

			float mapU = worldPos.x * editableSizeRcp * 0.5f + 0.5f;
			float mapV = worldPos.z * editableSizeRcp * 0.5f + 0.5f;
			int mapX = (int)(mapU * mapRes);
			int mapZ = (int)(mapV * mapRes);

			if (mapX < 0 || mapX >= mapRes || mapZ < 0 || mapZ >= mapRes)
				continue;

			uint8_t matVal = matMap[mapZ * mapRes + mapX];
			if (matVal == 0 || matVal > GGTERRAIN_MAX_SOURCE_TEXTURES)
				continue;

			int slot = materialToSlot[matVal - 1];
			if (slot < 0 || slot >= (int)chunk_data->blendmap_layers.size())
				continue;

			// Zero all layers at this vertex, then set painted layer to full weight
			for (size_t li = 0; li < chunk_data->blendmap_layers.size(); li++)
				chunk_data->blendmap_layers[li].pixels[vi] = 0;
			chunk_data->blendmap_layers[slot].pixels[vi] = 255;
		}

		// Rebuild the GPU blendmap texture and refresh the VT. For INTERACTIVE edits,
		// residency-backed (near) chunks take the repaint fast path — resident tiles
		// re-render next frame with the new blendmap; a full invalidate() would
		// re-stream the chunk through several seconds of GPU-feedback round-trips
		// (the visible paint lag). During the LOAD/mass bulk passes the fast path is
		// poison: hundreds of chunks per batch, each re-rendering its whole resident
		// tile set per frame = GPU storm (Island Showdown loaded at 6.7 FPS with the
		// entire terrain stuck on the default grey blend) — those keep invalidate().
		chunk_data->blendmap = {};
		terrain->CreateChunkRegionTexture(*chunk_data);
		if (chunk_data->vt)
		{
			if (g_blendRepaintFastPath && chunk_data->vt->residency != nullptr && chunk_data->vt->resolution != 0)
				chunk_data->vt->pending_repaint_blendmap = true;
			else
				chunk_data->vt->invalidate();
		}

		chunksModified++;
	}

	g_dbgPaintBlendChunks += chunksModified;
	if (chunksModified > 0)
	{
		wi::backlog::post(std::string("GGTerrainWicked: painted blendmaps on " +
			std::to_string(chunksModified) + " chunks").c_str());
	}
	return caughtUp;
}


// Born-correct blendmaps (Wicked delta 1.17): the terrain generator calls this on ITS
// thread for every freshly generated chunk, right after the vertex data is complete and
// before the chunk's region texture is built. It fills blendmap_layers with the same
// DX11-style auto weights + painted overrides the two main-thread passes would compute,
// so a streamed-in chunk never renders the engine-default region weights (the green
// default-blend squares that flickered during fast camera zooms while the passes caught
// up). PURE data path: reads only GG globals, the paint byte map and the chunk's own
// vertex arrays. Races with a concurrent editor stroke are benign — the stroke's
// invalidation bridge erases the chunk's keys and clears gg_blendmap_generated, so the
// passes reprocess it immediately after.
static bool FillChunkBlendmapGG(wi::terrain::ChunkData& cd, const wi::scene::MeshComponent& mesh)
{
	if (!wickedTerrainMaterialsSetup) return false; // level load: bulk passes handle it
	if (mesh.vertex_positions.size() != wi::terrain::vertexCount) return false;
	if (mesh.vertex_normals.size() != wi::terrain::vertexCount) return false;

	const auto& rp = GGTerrain::ggterrain_global_render_params;

	// Same ramp precompute as ApplyDX11StyleAutoBlend
	float layerRcpWidth[5];
	float layerStart[5];
	for (int i = 0; i < 5; i++)
	{
		layerStart[i] = rp.layerStartHeight[i];
		float w = rp.layerEndHeight[i] - rp.layerStartHeight[i];
		layerRcpWidth[i] = (w > 0.001f) ? (1.0f / w) : 0.0f;
	}
	float slopeRcpWidth[2];
	float slopeStart[2];
	for (int i = 0; i < 2; i++)
	{
		slopeStart[i] = rp.slopeStart[i];
		float w = rp.slopeEnd[i] - rp.slopeStart[i];
		slopeRcpWidth[i] = (w > 0.001f) ? (1.0f / w) : 0.0f;
	}

	const int baseSlot   = wi::terrain::MATERIAL_BASE;
	const int slopeSlot  = wi::terrain::MATERIAL_SLOPE;
	const int lowSlot    = wi::terrain::MATERIAL_LOW_ALTITUDE;
	const int highSlot   = wi::terrain::MATERIAL_HIGH_ALTITUDE;
	const int layer1Slot = g_layer1MaterialSlot;
	const int slotsNeeded = std::max(4, (layer1Slot >= 0 ? layer1Slot + 1 : 4));

	// Painted-material lookup (may be absent — auto weights only then)
	const uint8_t* matMap = GGTerrain::GGTerrain_GetMaterialMapPtr();
	const int mapRes = matMap ? GGTerrain::GGTerrain_GetMaterialMapResolution() : 0;
	float editableSize = GGTerrain::GGTerrain_GetEditableSize();
	float editableSizeRcp = (editableSize > 0.0f) ? (1.0f / editableSize) : 0.0f;
	const int paintTop = maxPaintedSlot;

	const int layersNeeded = std::max(slotsNeeded, paintTop + 1);
	for (int i = 0; i < layersNeeded; i++)
		cd.enable_blendmap_layer(i);

	const size_t layerCount = cd.blendmap_layers.size();

	for (size_t vi = 0; vi < wi::terrain::vertexCount; vi++)
	{
		const XMFLOAT3& lp = mesh.vertex_positions[vi];
		const float worldX = cd.position.x + lp.x;
		const float worldZ = cd.position.z + lp.z;
		const float height = cd.position.y + lp.y;

		// Painted vertex wins outright (same rule as ProcessPaintedChunkBlendmaps)
		if (matMap && mapRes > 0)
		{
			float mapU = worldX * editableSizeRcp * 0.5f + 0.5f;
			float mapV = worldZ * editableSizeRcp * 0.5f + 0.5f;
			int mapX = (int)(mapU * mapRes);
			int mapZ = (int)(mapV * mapRes);
			if (mapX >= 0 && mapX < mapRes && mapZ >= 0 && mapZ < mapRes)
			{
				uint8_t matVal = matMap[mapZ * mapRes + mapX];
				if (matVal > 0 && matVal <= GGTERRAIN_MAX_SOURCE_TEXTURES)
				{
					int slot = materialToSlot[matVal - 1];
					if (slot >= 0 && slot < (int)layerCount)
					{
						for (size_t li = 0; li < layerCount; li++)
							cd.blendmap_layers[li].pixels[vi] = 0;
						cd.blendmap_layers[slot].pixels[vi] = 255;
						continue;
					}
				}
			}
		}

		// DX11-style auto weights (identical math to ApplyDX11StyleAutoBlend)
		const float normaly = 1.0f - fabsf(mesh.vertex_normals[vi].y);
		float w[8] = { 0 };
		w[baseSlot] = 1.0f;
		auto applyLayer = [&](int layerIdx, int targetSlot)
		{
			if (targetSlot < 0 || targetSlot >= 8) return;
			if (layerRcpWidth[layerIdx] == 0.0f) return;
			float t = (height - layerStart[layerIdx]) * layerRcpWidth[layerIdx];
			if (t <= 0.0f) return;
			if (t > 1.0f) t = 1.0f;
			for (int s = 0; s < 8; s++) w[s] *= (1.0f - t);
			w[targetSlot] += t;
		};
		auto applySlope = [&](int slopeIdx, int targetSlot)
		{
			if (targetSlot < 0 || targetSlot >= 8) return;
			if (slopeRcpWidth[slopeIdx] == 0.0f) return;
			float t = (normaly - slopeStart[slopeIdx]) * slopeRcpWidth[slopeIdx];
			if (t <= 0.0f) return;
			if (t > 1.0f) t = 1.0f;
			for (int s = 0; s < 8; s++) w[s] *= (1.0f - t);
			w[targetSlot] += t;
		};
		applyLayer(0, lowSlot);
		if (layer1Slot >= 0) applyLayer(1, layer1Slot);
		applyLayer(2, highSlot);
		applyLayer(3, highSlot);
		applyLayer(4, highSlot);
		applySlope(0, slopeSlot);
		applySlope(1, slopeSlot);

		// Zero any painted layers left over from the engine-default fill, then store
		for (size_t li = 0; li < layerCount; li++)
			cd.blendmap_layers[li].pixels[vi] = 0;
		cd.blendmap_layers[baseSlot ].pixels[vi] = (uint8_t)(w[baseSlot ] * 255.0f);
		cd.blendmap_layers[slopeSlot].pixels[vi] = (uint8_t)(w[slopeSlot] * 255.0f);
		cd.blendmap_layers[lowSlot  ].pixels[vi] = (uint8_t)(w[lowSlot  ] * 255.0f);
		cd.blendmap_layers[highSlot ].pixels[vi] = (uint8_t)(w[highSlot ] * 255.0f);
		if (layer1Slot >= 0 && layer1Slot < (int)layerCount)
			cd.blendmap_layers[layer1Slot].pixels[vi] = (uint8_t)(w[layer1Slot] * 255.0f);
	}

	return true;
}

// Per-grass-type material cache. One MaterialComponent per entry in GGGrass::grassFiles[] (the 46
// DDS files in Files/grassbank/). Each material is built lazily the first time we see its type id
// in ProcessGrassChunks, so a level that uses 3 grass types pays for 3 DDS loads, not 46.
static wi::scene::MaterialComponent g_grassMaterials[GGGRASS_TOTAL_REAL_TYPES];
static bool                         g_grassMaterialReady[GGGRASS_TOTAL_REAL_TYPES] = {};
unsigned int gg_dbg_grass_materials_released = 0; // 3.68 diag: cumulative cache entries dropped

// GGMAX 3.68: drop every cached grass material. The cache above is lazy and was NEVER reset, so
// it is a process-lifetime union of every grass type any level in the session painted - 4 resident
// DDS at boot became 40 by the twentieth level load (~47 MB, strictly monotone, nothing evicted).
// Same defect shape as 3.65's terrain paint map and 3.68's rect packer: a PER-LEVEL value living
// in a process-global that the next level only overwrites if it happens to supply one.
//
// Safe to drop wholesale because nothing holds a pointer into g_grassMaterials[]: every consumer
// either reads it immediately or COPIES it (`scene.materials.Create(grassEntity) = *mat;` in the
// chunk CREATE branch), so a live hair entity keeps its own texture reference and is unaffected.
// The cache simply refills lazily - a level that paints three grass types pays three DDS loads,
// which is exactly what it paid the first time.
namespace GGTerrain {   // the cache statics above sit at global scope in this TU; the public
                        // entry point must be namespace-qualified to match the header.
void GGTerrainWicked_ReleaseGrassMaterials()
{
	int released = 0;
	for (uint32_t t = 0; t < (uint32_t)GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		if (!g_grassMaterialReady[t]) continue;
		g_grassMaterials[t] = wi::scene::MaterialComponent();
		g_grassMaterialReady[t] = false;
		released++;
	}
	gg_dbg_grass_materials_released += released;
}
} // namespace GGTerrain

// Build one cached grass material from Files/grassbank/<filename>. Returns nullptr on bad index.
// Kelp/seaweed sprites have authored _normal.dds siblings; we wire those in when present.
static wi::scene::MaterialComponent* BuildGrassMaterial(uint32_t typeIdx)
{
	if (typeIdx >= GGGRASS_TOTAL_REAL_TYPES) return nullptr;
	if (g_grassMaterialReady[typeIdx]) return &g_grassMaterials[typeIdx];

	// Stage B.9: dispatch between stock (typeIdx < GGGRASS_CUSTOM_REAL_TYPE_BASE) and custom
	// (typeIdx >= base). Stock reads the DDS path from the built-in grassFiles[] table via
	// GGGrass_GetTypeInfo; custom reads the user-registered filename via GGGrass_GetCustomSlotFilename
	// (slot = typeIdx - 24, matching the paint-side encoding).
	const char* dds_relpath = nullptr;
	bool isCustom = (typeIdx >= (uint32_t)GGGRASS_CUSTOM_REAL_TYPE_BASE);
	if (isCustom)
	{
		int slot = (int)typeIdx - 24;
		dds_relpath = GGGrass::GGGrass_GetCustomSlotFilename(slot);
		if (!dds_relpath || !dds_relpath[0]) return nullptr;
	}
	else
	{
		const GGGrass::GrassTypeInfo* info = GGGrass::GGGrass_GetTypeInfo(typeIdx);
		if (!info || !info->filename) return nullptr;
		dds_relpath = info->filename;
	}

	// Stock paths are just the DDS filename (relative to Files/grassbank/); custom paths from
	// sGrassTextures[] are relative to Files/ (e.g. "grassbank/foo.dds" or "user/mygrass.dds").
	// Both variants live under Files/ so we build the absolute path off the EXE-dir + Files/.
	char colorPath[512];
	if (isCustom)
		sprintf_s(colorPath, "%s/Files/%s", wickedTerrainExeDir.c_str(), dds_relpath);
	else
		sprintf_s(colorPath, "%s/Files/grassbank/%s", wickedTerrainExeDir.c_str(), dds_relpath);

	// info is only used by stock for the normal-map sibling detection below.
	const GGGrass::GrassTypeInfo* info = isCustom ? nullptr : GGGrass::GGGrass_GetTypeInfo(typeIdx);
	const char* nrmSrcName = isCustom ? dds_relpath : (info ? info->filename : nullptr);

	// Build a normal-map path if the sprite name ends in "_color.dds" (kelp/seaweed convention).
	// Otherwise leave normals unset — the alpha-cutout blade silhouette doesn't need them.
	char normalPath[512] = {0};
	bool haveNormal = false;
	const char* colorSuffix = nrmSrcName ? strstr(nrmSrcName, "_color.dds") : nullptr;
	if (colorSuffix)
	{
		size_t prefixLen = (size_t)(colorSuffix - nrmSrcName);
		char normalName[256];
		if (prefixLen < sizeof(normalName) - 16)
		{
			memcpy(normalName, nrmSrcName, prefixLen);
			memcpy(normalName + prefixLen, "_normal.dds", 12); // includes NUL
			if (isCustom)
				sprintf_s(normalPath, "%s/Files/%s", wickedTerrainExeDir.c_str(), normalName);
			else
				sprintf_s(normalPath, "%s/Files/grassbank/%s", wickedTerrainExeDir.c_str(), normalName);
			haveNormal = true;
		}
	}
	GGGrass::GGGrass_RecordTextureSource(typeIdx, colorPath);
	// the writable folder's copy ahead of the install's, as the GameGuru terrain, grass and trees load them (GG_GetRealPath)
	GG_GetRealPath(colorPath, 0);
	if (haveNormal) GG_GetRealPath(normalPath, 0);

	wi::scene::MaterialComponent& mat = g_grassMaterials[typeIdx];
	mat = wi::scene::MaterialComponent();
	mat.textures[wi::scene::MaterialComponent::BASECOLORMAP].name = colorPath;
	if (haveNormal)
		mat.textures[wi::scene::MaterialComponent::NORMALMAP].name = normalPath;
	mat.SetAlphaRef(0.5f);          // alpha cutout for the blade silhouette
	mat.SetDoubleSided(true);
	mat.SetRoughness(1.0f);
	mat.SetMetalness(0.0f);
	mat.SetReflectance(0.02f);
	// Green subsurface keeps back-lit blades softly translucent instead of going dark.
	mat.SetSubsurfaceScatteringColor(XMFLOAT3(0.35f, 0.6f, 0.2f));
	mat.SetSubsurfaceScatteringAmount(1.0f);
	mat.SetCastShadow(false);
	mat.SetTextureStreamingDisabled(true);
	mat.CreateRenderData();

	g_grassMaterialReady[typeIdx] = true;
	return &mat;
}

// GGMAX 1.95b flicker bisect (harness DUMP_GRASSTYPES): for every grass material already built
// this session, print the LIVE re-resolved bindless descriptor index + the texture's identity
// (dims / mips / srgb subresource / source path). Cross-checked against the textureIndex values
// captured into hair.grass_types at merge-build time: a mismatch = the captured index went stale;
// a non-blade identity = the capture pointed at the wrong resource from the start. Touches only
// g_grassMaterialReady slots, so it never loads anything — pure read.
void GGGrass_DumpTypeDescriptors(char* out, int size)
{
	int written = 0;
	auto* device = wi::graphics::GetDevice();
	for (uint32_t t = 0; t < (uint32_t)GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		if (!g_grassMaterialReady[t]) continue;
		if (written > size - 220) break;
		const wi::scene::MaterialComponent::TextureMap& tex =
			g_grassMaterials[t].textures[wi::scene::MaterialComponent::BASECOLORMAP];
		const wi::graphics::GPUResource* res = tex.GetGPUResource();
		int live = -1;
		uint32_t w = 0, h = 0, mips = 0;
		if (res != nullptr)
		{
			live = device->GetDescriptorIndex(res, wi::graphics::SubresourceType::SRV, tex.resource.GetTextureSRGBSubresource());
			if (res->IsTexture())
			{
				const wi::graphics::TextureDesc& d = ((const wi::graphics::Texture*)res)->GetDesc();
				w = d.width; h = d.height; mips = d.mip_levels;
			}
		}
		int n = snprintf(out + written, (size_t)(size - written),
			"LIVE t%u: idx=%d %ux%u mips=%u srgbsub=%d name=%s\n",
			t, live, w, h, mips,
			res != nullptr ? tex.resource.GetTextureSRGBSubresource() : -1,
			tex.name.c_str());
		if (n < 0) break;
		written += n;
	}
	if (written >= 0 && written < size) out[written] = 0;
}

// Per-grass-type appearance templates (length / width / billboards / stiffness etc.).
// Copied onto each spawned HairParticleSystem so the strand placement / mesh / strand-count are
// per-chunk but the look-and-feel comes from the type table.
static wi::HairParticleSystem g_grassAppearance[GGGRASS_TOTAL_REAL_TYPES];
static bool                   g_grassAppearanceReady = false;

// 8 categories spanning the 46 entries in GGGrass::grassFiles[].
// Boundaries kept in lockstep with that table — change one, change both.
enum GrassCategory { GCAT_COURSE, GCAT_SHORT, GCAT_TALL, GCAT_WILD, GCAT_WEED, GCAT_FLOWER, GCAT_KELP, GCAT_SEAWEED };

static GrassCategory CategoryFor(uint32_t typeIdx)
{
	if (typeIdx <= 6)  return GCAT_COURSE;   // 0..6   course grass mat1..mat30
	if (typeIdx <= 13) return GCAT_SHORT;    // 7..13  short  grass mat1..mat30
	if (typeIdx <= 20) return GCAT_TALL;     // 14..20 tall   grass mat1..mat30
	if (typeIdx <= 27) return GCAT_WILD;     // 21..27 wild   grass mat1..mat30
	if (typeIdx <= 36) return GCAT_WEED;     // 28..36 weeds 1..9
	if (typeIdx <= 39) return GCAT_FLOWER;   // 37..39 red / white / yellow flowers
	if (typeIdx <= 42) return GCAT_KELP;     // 40..42 kelp 1..3
	return GCAT_SEAWEED;                     // 43..45 seaweed 1..3
}

// Fill g_grassAppearance from the per-type GGGrass metadata + category defaults.
// scaleFactor (from the _SF_x.xx filename suffix) scales blade length so e.g. "short grass" really
// renders shorter than "tall grass". Width / stiffness / billboard count come from the category.
// Per-type baseline blade length (units). Populated at the end of BuildGrassAppearance from
// whatever category-specific value each type ended up with; ApplyGrassScale reads this and
// writes `a.length = baseline * (slider / GGGRASS_SCALE)` so the Grass Scale slider linearly
// scales every blade uniformly, matching DX11's `IN.position * grass_scale` in GGGrassVS.hlsl.
static float g_grassBaseLength[GGGRASS_TOTAL_REAL_TYPES] = {};

static void BuildGrassAppearance()
{
	if (g_grassAppearanceReady) return;
	uint32_t numTypes = GGGrass::GGGrass_GetNumTypes();
	if (numTypes > GGGRASS_TOTAL_REAL_TYPES) numTypes = GGGRASS_TOTAL_REAL_TYPES;

	for (uint32_t t = 0; t < numTypes; t++)
	{
		const GGGrass::GrassTypeInfo* info = GGGrass::GGGrass_GetTypeInfo(t);
		float sf = info ? info->scaleFactor : 1.0f;
		if (sf <= 0.01f) sf = 1.0f;

		wi::HairParticleSystem& a = g_grassAppearance[t];
		a = wi::HairParticleSystem();
		a.segmentCount = 1;        // single segment keeps GPU memory bounded across chunks
		a.randomness = 0.35f;
		a.stiffness = 9.0f;        // upright enough to dodge the wind-driven twist/flip (confirmed safe)
		a.drag = 0.5f;
		a.atlas_rects.clear();     // each material is a single-sprite DDS — no atlas
		a.uniformity = 1.0f;
		// Sane default; ApplyGrassDrawDistance() (called below + on every slider change) overwrites
		// from gggrass_global_params.lod_dist so this only matters for the first-frame create window.
		a.viewDistance = 5000.0f;

		// Stage B.5: match DX11's posOrig formula — posOrig.x *= scaleFactor (legacy GGGrassVS.hlsl
		// line 45) — so SF drives WIDTH per type, not length. Length is anchored to DX11's uniform
		// grass_scale = 40 with a category factor for short/tall variation. Within-category visual
		// differences (e.g. course mat1 vs course mat3) are then purely texture-content driven,
		// which is what DX11 also shows.
		//
		// Effective rendered blade in Wicked:
		//   quad width  = hair.width * (texW / texH) * hair.length
		//   quad height = hair.length
		// With width = sf, length = ~40, and roughly-square DDS textures, that's ≈ 40*sf wide by
		// 40 tall — same numeric shape as DX11's `IN.position * grass_scale * SF`.
		switch (CategoryFor(t))
		{
		case GCAT_COURSE:
			a.length = 40.0f;       // 1.0 × DX11 grass_scale
			a.width = sf;           // SF=1.15
			a.billboardCount = 2;
			break;
		case GCAT_SHORT:
			a.length = 30.0f;       // 0.75 ×
			a.width = sf;           // SF=1.4 (wider stubby blade)
			a.billboardCount = 2;
			break;
		case GCAT_TALL:
			a.length = 56.0f;       // 1.4 ×
			a.width = sf;           // SF=0.87 (narrower tall blade)
			a.billboardCount = 2;
			break;
		case GCAT_WILD:
			a.length = 35.0f;       // 0.875 ×
			a.width = sf;           // SF=1.17
			a.billboardCount = 2;
			break;
		case GCAT_WEED:
			// DX11 parity: length = grass_scale = 40, width = sf. Prior sf*2 was overreach — the
			// DDS texture already conveys the stem shape at the SF value the artist encoded.
			a.length = 40.0f;
			a.width = sf;
			a.billboardCount = 1;
			a.stiffness = 12.0f;
			break;
		case GCAT_FLOWER:
			// DX11 parity: length = grass_scale = 40, width = sf. Same rationale as Weed —
			// texture-content variance replaces the prior width-multiplier hack.
			a.length = 40.0f;
			a.width = sf;
			a.billboardCount = 2;
			// viewDistance: ApplyGrassDrawDistance() halves the slider value for FLOWER (tiny features
			// benefit from earlier cull) - in far mode 0 only (GGMAX 3.117: under the DX11 far field flowers
			// reach the full Grass Draw Distance, as in DX11). No per-case assignment needed.
			break;
		case GCAT_KELP:
			// DX11 parity: same length as Course Grass (grass_scale = 40) with SF applied to width
			// only (matches legacy GGGrassVS.hlsl line 45: posOrig.x *= scaleFactor). Previous
			// length=75 + width=sf*6 was pure overreach — the kelp DDS itself already conveys the
			// broad-blade silhouette at length=40, and multiplying width by 6 gave ~10-ft towering
			// plants that tanked FPS via overdraw.
			a.length = 40.0f;
			a.width = sf;           // SF=0.47 -> thin narrow kelp blade
			a.billboardCount = 1;
			a.stiffness = 4.0f;
			a.drag = 0.8f;
			break;
		case GCAT_SEAWEED:
			// DX11 parity: same length as Course Grass, SF -> width only. Prior length=110 +
			// width=sf*4 was double overreach.
			a.length = 40.0f;
			a.width = sf;           // SF=0.7..0.9
			a.billboardCount = 1;
			a.stiffness = 3.0f;
			a.drag = 0.9f;
			break;
		}
	}

	// Stage B.9: custom palette slots (22..GGGRASS_MAX_PALETTE_SLOTS-1) map to real_types
	// (GGGRASS_CUSTOM_REAL_TYPE_BASE..). Each active slot gets an appearance built with a
	// default category (GCAT_WILD — medium blade, generic look) so it renders as reasonable
	// grass without requiring the user to specify per-slot tuning yet. SF is parsed from the
	// filename's "_SF_x.xx" suffix if present (matching the stock file convention); else 1.0.
	for (int slot = GGGRASS_CUSTOM_SLOT_BASE; slot < GGGRASS_MAX_PALETTE_SLOTS; slot++)
	{
		const char* fn = GGGrass::GGGrass_GetCustomSlotFilename(slot);
		if (!fn || !fn[0]) continue;
		uint32_t realType = (uint32_t)slot + 24;
		if (realType >= GGGRASS_TOTAL_REAL_TYPES) continue; // defensive

		// Parse SF from filename ("_SF_x.xx" suffix). Fall back to 1.0 if absent.
		float sf = 1.0f;
		{
			const char* p = strstr(fn, "_SF_");
			if (p)
			{
				float parsed = (float)atof(p + 4);
				if (parsed > 0.01f) sf = parsed;
			}
		}

		wi::HairParticleSystem& a = g_grassAppearance[realType];
		a = wi::HairParticleSystem();
		a.segmentCount = 1;
		a.randomness = 0.35f;
		a.stiffness = 9.0f;
		a.drag = 0.5f;
		a.atlas_rects.clear();
		a.uniformity = 1.0f;
		a.viewDistance = 5000.0f;   // overwritten by ApplyGrassDrawDistance() below + on slider drag
		a.length = 40.0f;           // DX11 grass_scale baseline (Course-Grass-equivalent height)
		a.width = sf;               // matches DX11 posOrig.x *= scaleFactor mapping
		a.billboardCount = 2;
	}

	// Snapshot the per-type baseline length AFTER all category tweaks have landed. ApplyGrassScale
	// reads from this array and writes `a.length = baseline * (slider / 40)`; every subsequent
	// BuildGrassAppearance call refreshes the snapshot so custom-slot rebuilds pick up their own
	// baseline before scaling is applied. Since quad width in the hair simulate CS is proportional
	// to length (`quad_width = hair.width * xHairAspect * hair.length`), scaling length alone gives
	// uniform blade scaling — matches DX11's `IN.position * grass_scale` in GGGrassVS.
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		g_grassBaseLength[t] = g_grassAppearance[t].length;
	}

	g_grassAppearanceReady = true;
}

// Populate the per-type appearance templates (one per entry in GGGrass::grassFiles[]). Materials
// are built lazily on first sighting (see BuildGrassMaterial), so no DDS is loaded until a chunk
// actually paints that type.
//
// Runtime assets: Files/grassbank/*.dds — already shipped as part of the GameGuru asset pack.
// Nothing else to deploy.
// Tracks last-applied Grass Draw Distance slider value so we can detect changes per frame and push
// the new viewDistance to live hair entities. -1 forces a sync on first call after setup.
static float g_grassPrevSliderInches = -1.0f;

// Sync the editor's Grass Draw Distance slider into per-entity viewDistance. Called after
// BuildGrassAppearance and from GGTerrainWicked_Update whenever the slider changes — so dragging
// the slider in the editor pulls every existing grass entity's cull radius along with it instead
// of waiting for chunks to be recreated.
static void ApplyGrassDrawDistance()
{
	// Per-strand visibility cull. Matches the DX11 grassRadius semantic (lod_dist + 2500), so the
	// slider directly represents "visible grass radius in inches above 2500". outerC in
	// ProcessGrassChunks sits 1 chunk past this in far mode 0 so chunks are created with their near edges
	// outside the cull — strands fade in gradually instead of whole chunks popping. (GGMAX 3.117: under
	// the DX11 far field + DX11 density the ring is the cull + the half-diagonal - GrassFarRingCollapsed.)
	const float viewDistInches = GGGrass_LodDistEffective() + 2500.0f;
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		// Flowers cull at half the radius — tiny features that don't read past mid-distance.
		// GGMAX 3.117: not under the DX11 far field - DX11 drew every grass type to the same distance.
		float vd = (CategoryFor(t) == GCAT_FLOWER && gg_grass_far_mode == 0) ? viewDistInches * 0.5f : viewDistInches;
		g_grassAppearance[t].viewDistance = vd;
	}
	// Push to live entities (those already created on prior frames). New CREATEs pick up the
	// updated template directly.
	auto& scene = wi::scene::GetScene();
	for (size_t hi = 0; hi < scene.hairs.GetCount(); hi++)
	{
		wi::HairParticleSystem& h = scene.hairs[hi];
		if (h.grass_type == 0) continue;                  // not a GG grass entity (upstream hair)
		// GGMAX 3.117: the cull radius and the far-field mode are both read by the SIMULATE CS. WHEN the
		// 1.37 static-skip is on (SET_HAIRSKIP 1 - it is OFF by default since 2026-07-25, so this is a
		// guard, not a fix) it never re-simulates a parked camera without wind, and a slider drag or a
		// far-mode switch while parked would change nothing on screen. Unsettle the system: the next
		// frames run full-rate (4 sims fill both ping-pong buffers), then the skip re-engages.
		h.gg_sim_runs = 0;
		// GGMAX 1.97: merged systems carry per-type values in grass_types[] — the typeIdx guard
		// below silently skipped them, which is why the Grass Draw Distance slider went dead on
		// merged grass (and why three SET_GRASS probe rows in the flicker hunt were void).
		// Refresh the table from the updated templates (keeps the per-category rules, e.g. the
		// flower half-radius) and restore the merge-build invariant: system viewDistance = max
		// across PRESENT types (it is the LOD rescale reference and the VS fade radius).
		if (h.grass_type == GG_HAIR_GRASS_MERGED)
		{
			float maxVd = 0.0f;
			const size_t n = std::min(h.grass_types.size(), (size_t)GGGRASS_TOTAL_REAL_TYPES);
			for (size_t t = 0; t < n; t++)
			{
				h.grass_types[t].viewDistance = g_grassAppearance[t].viewDistance;
				if (h.grass_types[t].present)
					maxVd = std::max(maxVd, h.grass_types[t].viewDistance);
			}
			if (maxVd > 0.0f) h.viewDistance = maxVd;
			continue;
		}
		uint32_t typeIdx = h.grass_type - 1;
		if (typeIdx >= GGGRASS_TOTAL_REAL_TYPES) continue;
		h.viewDistance = g_grassAppearance[typeIdx].viewDistance;
	}
	g_grassPrevSliderInches = GGGrass_LodDistEffective();
}

// Sync the editor's Grass Start/End Altitude sliders (and their underwater siblings + the water
// height) into per-entity CB values that the hair simulate CS reads for the altitude filter added
// alongside the slope filter (WICKED_ENGINE_CHANGES.md entry 1.5). Called after
// BuildGrassAppearance and from GGTerrainWicked_Update every frame — dragging any of the four
// sliders (or moving the water plane) pulls every existing grass entity's altitude band along
// live, without waiting for chunks to be recreated.
static void ApplyGrassAltitude()
{
	const float minH   = GGGrass::gggrass_global_params.min_height;
	const float maxH   = GGGrass::gggrass_global_params.max_height;
	const float minHU  = GGGrass::gggrass_global_params.min_height_underwater;
	const float maxHU  = GGGrass::gggrass_global_params.max_height_underwater;
	const float waterH = GGGrass::GGGrass_GetDefaultWaterHeight();

	// Update templates so newly created entities pick up current values on CREATE.
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		g_grassAppearance[t].grass_water_height           = waterH;
		g_grassAppearance[t].grass_min_height             = minH;
		g_grassAppearance[t].grass_max_height             = maxH;
		g_grassAppearance[t].grass_min_height_underwater  = minHU;
		g_grassAppearance[t].grass_max_height_underwater  = maxHU;
	}
	// Push to live entities so slider drags apply immediately.
	auto& scene = wi::scene::GetScene();
	for (size_t hi = 0; hi < scene.hairs.GetCount(); hi++)
	{
		wi::HairParticleSystem& h = scene.hairs[hi];
		if (h.grass_type == 0) continue;               // upstream Wicked hair — leave alone
		h.grass_water_height          = waterH;
		h.grass_min_height            = minH;
		h.grass_max_height            = maxH;
		h.grass_min_height_underwater = minHU;
		h.grass_max_height_underwater = maxHU;
	}
}

// Sync the editor's Grass Scale slider into per-entity `length`. Slider value 40 maps 1:1 to the
// per-category baseline set in BuildGrassAppearance (Course=40, Short=30, Tall=56, Wild=35,
// weed/flower/kelp/seaweed = 40 each). Slider 80 = 2× baseline, slider 20 = 0.5× baseline. Called
// after BuildGrassAppearance (so the baselines are captured) and every frame from
// GGTerrainWicked_Update so slider drags apply live without waiting for chunks to be rebuilt.
static void ApplyGrassScale()
{
	const float sliderScale = GGGrass::gggrass_global_params.grass_scale;
	constexpr float baselineSlider = 40.0f; // GGGRASS_SCALE from GGGrassConstants.hlsli
	const float mult = sliderScale / baselineSlider;
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		g_grassAppearance[t].length = g_grassBaseLength[t] * mult;
	}
	auto& scene = wi::scene::GetScene();
	for (size_t hi = 0; hi < scene.hairs.GetCount(); hi++)
	{
		wi::HairParticleSystem& h = scene.hairs[hi];
		if (h.grass_type == 0) continue;               // upstream Wicked hair — leave alone
		// GGMAX 1.97: same merged-system routing as ApplyGrassDrawDistance — the typeIdx guard
		// skipped merged systems, so the Grass Scale slider was dead on merged grass. Per-type
		// lengths refresh from the templates (baseline × slider mult, per category); system
		// length keeps the merge-build max-across-present invariant (CS cull radius source).
		if (h.grass_type == GG_HAIR_GRASS_MERGED)
		{
			float maxLen = 0.0f;
			const size_t n = std::min(h.grass_types.size(), (size_t)GGGRASS_TOTAL_REAL_TYPES);
			for (size_t t = 0; t < n; t++)
			{
				h.grass_types[t].length = g_grassAppearance[t].length;
				if (h.grass_types[t].present)
					maxLen = std::max(maxLen, h.grass_types[t].length);
			}
			if (maxLen > 0.0f) h.length = maxLen;
			continue;
		}
		uint32_t typeIdx = h.grass_type - 1;
		if (typeIdx >= GGGRASS_TOTAL_REAL_TYPES) continue;
		h.length = g_grassAppearance[typeIdx].length;
	}
}

static void SetupWickedGrass()
{
	BuildGrassAppearance();
	ApplyGrassDrawDistance();
	ApplyGrassAltitude();
	ApplyGrassScale();
	wickedGrassSetup = true;
}

// Distance-LOD grass: dense fine grass on the chunks nearest the camera, thinning with distance and
// stopping past the outer ring. Concentrating strands near the camera (and dropping them far) keeps
// the total bounded — the trick native grass uses — so VRAM stays safe even during the load burst.
// Placement still follows GG's painted grass map; chunks crossing a tier boundary (or regenerated by
// the terrain) are rebuilt at the new density. All scene mutation is deferred to after Generation_Cancel.
//
// The outer-ring radius `outerC` is driven at runtime from the editor's Grass Draw Distance slider
// (gggrass_global_params.lod_dist, in inches) converted to chunk-distances. Near/mid boundaries
// scale proportionally so cutting the slider tight pulls all tiers in together, and extending it
// pushes them all out.
static int GrassTierForRingDist(float ringDist, float nearC, float midC, float outerC)
{
	if (ringDist <= nearC) return 3;             // near: full density
	if (ringDist <= midC)  return 2;             // mid
	if (ringDist <= outerC) return 1;            // far: sparse
	return 0;                                     // beyond: no grass
}
// GGMAX 3.116: superseded by GrassStrandsForTier (the low-VRAM multiplier now rides the density latch);
// kept for the reasoning in its comments and for any older caller.
[[maybe_unused]] static float GrassTierDensityScale(int tier)
{
	float s;
	switch (tier) { case 3: s = 1.00f; break; case 2: s = 0.18f; break; case 1: s = 0.05f; break; default: return 0.0f; }

	// GGMAX low-VRAM preset: uniform strand THINNING. Grass memory is linear in strandCount
	// (~497 B/strand), so this is the one grass lever with real leverage — the draw-distance cap
	// only reaches 11% on Z Island because the chunk ring is dominated by its +1-chunk term.
	//
	// Why this is safe where the 2026-08-01 attempt was not. That one narrowed the emitter MASK
	// (vertex_lengths, built from per-vertex point samples of the paint map at ~2 m spacing) and
	// scaled strandCount by the resulting fraction. The mask under-reported interleaved paint, so
	// strands bunched into whatever it happened to catch: coverage fell 30% AND clumpCV rose 31%.
	// This touches strandCount ONLY. The emitter mesh, vertex_lengths and randomSeed are all
	// untouched, so the same painted region is still sampled uniformly — just with fewer strands.
	//
	// Coverage is EXPECTED to fall here; that is precisely the trade a 4 GB preset is buying, and
	// it is why the usual coverage-parity threshold does not apply. The gate that DOES apply is
	// clumpCV (tools/grassdensity.ps1, GRASS_BENCHMARK.md): thinning must stay even, never clump.
	if (gg_lowvram && gg_lowvram_grass_density > 0.0f && gg_lowvram_grass_density < 1.0f)
	{
		s *= gg_lowvram_grass_density;
	}
	return s;
}

// GGMAX 3.116: strands for one grass system at a tier, from the LATCHED density. Tier weights are
// relative to the near tier (1.00 / 0.18 / 0.05); the low-VRAM multiplier is applied here too.
static float GrassTierRel( int tier )
{
	switch ( tier ) { case 3: return 1.00f; case 2: return 0.18f; case 1: return 0.05f; default: return 0.0f; }
}
static uint32_t GrassStrandsForTier( int tier )
{
	const float rel = GrassTierRel( tier );
	if ( rel <= 0.0f ) return 0;
	uint32_t n = (uint32_t)( g_grassNearStrands * rel * g_grassLowVramMul + 0.5f );
	if ( n < 1024 ) n = 1024;                         // the floor every system always had
	if ( n > GG_GRASS_MAX_STRANDS ) n = GG_GRASS_MAX_STRANDS;
	return n;
}

void GGGrass_GetDensityState( float* nearStrands, float* lowMul, unsigned int* t3, unsigned int* t2, unsigned int* t1,
	unsigned int* stale, unsigned int* latches )
{
	if ( nearStrands ) *nearStrands = g_grassNearStrands;
	if ( lowMul ) *lowMul = g_grassLowVramMul;
	if ( t3 ) *t3 = GrassStrandsForTier( 3 );
	if ( t2 ) *t2 = GrassStrandsForTier( 2 );
	if ( t1 ) *t1 = GrassStrandsForTier( 1 );
	if ( stale )
	{
		// LIVE: chunks whose systems were built at a count the latch no longer asks for (same 2% / 256
		// dead band as the pass) - 0 means every chunk carries the current density
		unsigned int n = 0;
		for ( const auto& kv : grassChunkKeyToGrassEntities )
		{
			if ( kv.second.builtStrands == 0 ) continue;
			auto tr = grassChunkKeyToTier.find( kv.first );
			if ( tr == grassChunkKeyToTier.end() || tr->second <= 0 ) continue;
			const uint32_t want = GrassStrandsForTier( tr->second ), have = kv.second.builtStrands;
			const uint32_t diff = ( want > have ) ? want - have : have - want;
			if ( diff >= 256 && diff * 50u > have ) n++;
		}
		*stale = n;
	}
	if ( latches ) *latches = g_dbgGrassDensityLatches;
}

// GGMAX 1.74: build (or keep) the single merged hair entity for one chunk. Everything that
// decides WHERE a strand goes — emitter mesh, vertex_lengths, strandCount, randomSeed — is
// identical to the per-type path, which is what makes placement bit-identical rather than
// merely similar. What changes is that the simulate CS is told to own every type
// (GG_HAIR_GRASS_MERGED) and handed a table of per-type length/width/stiffness/drag/viewDistance
// and blade texture, so a strand adopts its cell's type instead of being discarded.
// GGMAX 1.84: merged-grass forensics. `grassmerge=1` produced ZERO hair systems on the resaved
// TESTPRO1 (HAIR_SYSTEMS 0, 0 MB of grass buffers, coverage 0.02%) instead of the documented
// 5-systems-too-dense result, and reading the function did not explain it. These counters name
// which exit it takes. Reported on the GRASS_CHUNKS line together with the LIVE flag value —
// the mablockmb lesson: an instrument must echo the setting back, not just the effect, or an
// inert knob reads as a working one.
unsigned int gg_dbg_merge_calls = 0;      // ProcessGrassChunkMerged entered
unsigned int gg_dbg_merge_notypes = 0;    // ...and bailed with typeCount == 0
unsigned int gg_dbg_merge_reused = 0;     // ...existing entity still valid, nothing to do
unsigned int gg_dbg_merge_nomat = 0;      // ...BuildGrassMaterial returned null
unsigned int gg_dbg_merge_created = 0;    // ...created a merged hair entity

template <typename PendingGrassT>
static void ProcessGrassChunkMerged(
	wi::scene::Scene& scene,
	const PendingGrassT& pc,
	ChunkGrassEntities& existingEntities,
	const bool (&typesSeen)[GGGRASS_TOTAL_REAL_TYPES],
	float editableSize)
{
	gg_dbg_merge_calls++;
	// Which types does this chunk need? Also the mask we compare against to decide whether an
	// existing merged entity is still valid (a newly painted type must appear in the table).
	uint64_t wantMask[2] = { 0, 0 };
	uint32_t typeCount = 0;
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		if (!typesSeen[t]) continue;
		wantMask[t >> 6] |= (1ull << (t & 63));
		typeCount++;
	}

	if (typeCount == 0)
	{
		gg_dbg_merge_notypes++;
		// Chunk has no painted grass left — drop the merged entity if one exists.
		if (existingEntities.merged != wi::ecs::INVALID_ENTITY)
		{
			if (scene.hairs.GetComponent(existingEntities.merged))
				scene.Entity_Remove(existingEntities.merged);
			existingEntities.merged = wi::ecs::INVALID_ENTITY;
			existingEntities.mergedTypeMask[0] = existingEntities.mergedTypeMask[1] = 0;
		}
		return;
	}

	// Existing entity still covers exactly the painted set: nothing to do. Paint that only moves
	// cells between types already present needs no rebuild at all, because type resolution is
	// per-strand in the shader and reads the live paint texture.
	if (existingEntities.merged != wi::ecs::INVALID_ENTITY
		&& existingEntities.mergedTypeMask[0] == wantMask[0]
		&& existingEntities.mergedTypeMask[1] == wantMask[1]
		&& scene.hairs.GetComponent(existingEntities.merged) != nullptr)
	{
		gg_dbg_merge_reused++;
		return;
	}

	// Rebuild: the painted type set changed (or this is the first build for the chunk).
	if (existingEntities.merged != wi::ecs::INVALID_ENTITY)
	{
		if (scene.hairs.GetComponent(existingEntities.merged))
			scene.Entity_Remove(existingEntities.merged);
		existingEntities.merged = wi::ecs::INVALID_ENTITY;
	}

	// The merged entity's material is only the fallback / shared surface settings; the per-strand
	// blade texture comes from the table below. Use the lowest painted type's material so alpha
	// ref, double-sidedness and subsurface match what the chunk used before.
	uint32_t firstType = 0;
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++) { if (typesSeen[t]) { firstType = t; break; } }
	wi::scene::MaterialComponent* mat = BuildGrassMaterial(firstType);
	if (!mat) { gg_dbg_merge_nomat++; return; }

	wi::vector<float> vertex_lengths;
	vertex_lengths.resize(wi::terrain::vertexCount, 1.0f);

	gg_dbg_merge_created++;
	wi::ecs::Entity grassEntity = wi::ecs::CreateEntity();
	wi::HairParticleSystem& hair = scene.hairs.Create(grassEntity);
	hair = g_grassAppearance[firstType];
	hair.meshID = pc.entity;
	hair.vertex_lengths = std::move(vertex_lengths);

	hair.grass_type = GG_HAIR_GRASS_MERGED;
	hair.grass_map_inv_world_size = 0.5f / editableSize;
	hair.grass_map_origin_x = 0.0f;
	hair.grass_map_origin_z = 0.0f;
	hair.grass_visibility_texture = GGGrass::GGGrass_GetMapTexture();

	// Per-type table. billboardCount and viewDistance also drive buffer sizing / fade on the
	// SYSTEM, so those take the max across the chunk's types: the stride must be big enough for
	// the greediest type, and a strand of a smaller type collapses its surplus billboard to zero
	// area in the CS rather than shortening the stride (which would corrupt later strands).
	hair.grass_types.resize(GGGRASS_TOTAL_REAL_TYPES);
	uint32_t maxBillboards = 1;
	float maxViewDistance = 0.0f;
	float maxLength = 0.0f;
	wi::graphics::GraphicsDevice* device = wi::graphics::GetDevice();
	for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
	{
		if (!typesSeen[t]) continue;
		const wi::HairParticleSystem& src = g_grassAppearance[t];
		wi::HairParticleSystem::GrassTypeParams& dst = hair.grass_types[t];
		dst.length = src.length;
		// The CS multiplies frame.x by (atlas_rect.aspect * xHairAspect) in the per-type path.
		// GG grass clears atlas_rects, so that product is just the system's aspect — fold the
		// per-type width in here so the merged shader can use one number.
		dst.width = src.width;
		dst.stiffness = src.stiffness;
		dst.drag = src.drag;
		dst.viewDistance = src.viewDistance;
		dst.billboardCount = src.billboardCount;
		dst.textureIndex = 0;
		dst.present = true;   // only scanned types get an entry, matching the per-type build

		// Bindless descriptor for this type's blade DDS, resolved the same way the material
		// system resolves BASECOLORMAP (SRGB subresource).
		wi::scene::MaterialComponent* typeMat = BuildGrassMaterial(t);
		if (typeMat != nullptr)
		{
			const wi::scene::MaterialComponent::TextureMap& tex =
				typeMat->textures[wi::scene::MaterialComponent::BASECOLORMAP];
			const wi::graphics::GPUResource* res = tex.GetGPUResource();
			if (res != nullptr)
			{
				const int descriptor = device->GetDescriptorIndex(
					res, wi::graphics::SubresourceType::SRV, tex.resource.GetTextureSRGBSubresource());
				if (descriptor >= 0) dst.textureIndex = (uint32_t)descriptor;
			}
		}

		maxBillboards = std::max(maxBillboards, src.billboardCount);
		maxViewDistance = std::max(maxViewDistance, src.viewDistance);
		maxLength = std::max(maxLength, src.length);
	}
	hair.billboardCount = maxBillboards;
	hair.viewDistance = maxViewDistance;
	hair.length = maxLength;

	// Same strand count as ONE per-type system had. That is the entire saving: the chunk used to
	// pay this per painted type. GGMAX 3.116: from the latched Grass Density (DX11 parity).
	const uint32_t strands = GrassStrandsForTier(pc.tier);
	hair.strandCount = strands;
	existingEntities.builtStrands = strands;

	hair.CreateFromMesh(*pc.mesh);
	hair.position_format = wi::graphics::Format::R32G32B32A32_FLOAT;
	hair.CreateRenderData();

	scene.materials.Create(grassEntity) = *mat;
	scene.transforms.Create(grassEntity);
	scene.Component_Attach(grassEntity, pc.entity, true);

	existingEntities.merged = grassEntity;
	existingEntities.mergedTypeMask[0] = wantMask[0];
	existingEntities.mergedTypeMask[1] = wantMask[1];
	g_dbgGrassRecreates++;
}

static void ProcessGrassChunks(wi::terrain::Terrain* terrain, const XMFLOAT3& cameraPos)
{
	auto& scene = wi::scene::GetScene();

	// Shadow-flicker census (runs every frame, before any early-out): count live GG-grass hair
	// systems whose emitter chunk mesh is GONE this frame. Such a hair is skipped by the simulate
	// pass (its grass + shadow disappear) until we rebuild it below. >0 == the flicker is happening.
	{
		size_t deadNow = 0;
		for (size_t hi = 0; hi < scene.hairs.GetCount(); hi++)
		{
			const wi::HairParticleSystem& h = scene.hairs[hi];
			if (h.grass_type == 0) continue; // not a GG grass hair (upstream Wicked hair)
			if (h.meshID == wi::ecs::INVALID_ENTITY || scene.meshes.GetComponent(h.meshID) == nullptr)
				deadNow++;
		}
		g_dbgGrassDeadMeshNow = deadNow;
	}

	float editableSize = GGTerrain::GGTerrain_GetEditableSize();
	// GGMAX 3.112: terrain is NULL while a Terrain Bake holds - the chunks are the bake's grass
	// emitters then, and the stride is the one the torn-down terrain had.
	float chunkStride = terrain ? (wi::terrain::chunk_width - 1) * terrain->chunk_scale : s_bakeGrassChunkStride;
	if (chunkStride <= 0.0f) return;

	// LOD ring vs per-strand cull — these MUST be decoupled, or chunk pop-in happens.
	// viewDistInches is the per-strand binary visibility cull (matches DX11 grassRadius =
	// lod_dist + GGGRASS_LOD_TRANSITION). outerC is the chunk-entity-creation ring, which is
	// always 1 chunk PAST viewDistInches — so when a chunk's center first crosses outerC, its
	// near edge is still chunkStride/2 past viewDistInches, meaning none of its strands are
	// eligible to render yet. As the camera approaches, strands fade in one-by-one as each
	// crosses the per-strand cull threshold. No more whole-chunk pop-in.
	//
	// Tier 3 (full density) and tier 2 (mid) keep their canonical 1.0 / 1.7 chunk boundaries
	// clamped DOWN to outerC. SET_GRASS "lodchunks" hard-overrides via g_grassLODChunksOverride.
	// GGMAX 3.117: the "1 chunk PAST" lead above is far mode 0 (and the override paths). Under the DX11
	// far field with the DX11 density (GrassFarRingCollapsed) the ring below is R/stride + 0.7072: a new chunk's near edge is 0.207 chunk outside the
	// cull along an axis and exactly AT it on the diagonal (the cull is 3D, the ring horizontal, so never
	// inside), and the dissolve over [R-2500, R] hides a chunk that arrives late.
	const float viewDistInches = GGGrass_LodDistEffective() + 2500.0f;
	float outerC = (g_grassLODChunksOverride > 0.0f)
		? g_grassLODChunksOverride
		: std::max(0.5f, viewDistInches / chunkStride + 1.0f);
	// AUTO tier boundaries follow the strand-LOD opt-in (see g_grassTier3Chunks comment)
	const bool lodOn = wi::gg_grass_lod;
	const float tier3C = (g_grassTier3Chunks > 0.0f) ? g_grassTier3Chunks : (lodOn ? 1.5f : 1.0f);
	const float tier2C = (g_grassTier2Chunks > 0.0f) ? g_grassTier2Chunks : (lodOn ? 2.2f : 1.7f);
	float midC   = std::min(tier2C, outerC);
	float nearC  = std::min(tier3C, outerC);
	// GGMAX 3.117: under the DX11 far field EVERY chunk that can hold a visible strand gets the full count
	// (DX11 was one uniform density out to the cull, then the dissolve): the ring reaches the cull radius
	// plus the half-diagonal (a chunk whose centre is inside it has ground within the cull). Nothing past
	// it is built - its strands could never be seen, and they arrive dissolved to nothing anyway, so a
	// chunk created as its corner crosses the cull cannot pop.
	// ★ Only with the DX11 DENSITY (review 3.117): DX11 density falls with R^2, so the collapsed ring stays
	// a bounded budget (~56250*pi*(1 + 0.71*stride/R)^2 strands); a constant 100k per chunk
	// (grassdensitydx11=0) would grow with R^2 - 1.7M+ strands at 6000 - so that mode keeps its rings and
	// still gets the engine dissolve. An explicit SET_GRASS tier2 (or tier3/lodchunks) restores the rings.
	if (GrassFarRingCollapsed())
	{
		const float fullC = std::min(outerC, viewDistInches / chunkStride + 0.7072f);
		nearC = midC = outerC = fullC;
	}

	struct PendingGrass {
		wi::ecs::Entity entity;
		uint64_t key;
		wi::scene::MeshComponent* mesh;
		XMFLOAT4X4 world;
		int tier;
		bool fullReset; // true when Wicked recycled the chunk; existing hair entities reference a
		                // gone mesh and must be removed before recreate. False = paint update path
		                // where we reuse existing entities (Stage 2).
		bool deadMesh;  // fullReset CAUSE split: true = chunk entity recycled (old hair points at a
		                // vanished mesh — teardown may NOT be deferred); false = live tier change
		                // (old grass keeps rendering, so deferring the rebuild is invisible).
		float ringDist; // chunk-center distance from camera in chunk units (creation priority)
	};
	wi::vector<PendingGrass> pending;

	// Phase 1 (read-only): pick each chunk's target LOD tier from camera distance; queue chunks whose
	// tier (or chunk entity) changed. No scene/map mutation here — the generator thread may be running.
	const uint32_t nowFrame = (uint32_t)wi::graphics::GetDevice()->GetFrameCount(); // settle gate
	g_grassSettlePending = false; // re-set below while any chunk is still deferring
	g_dbgGrassDensityStale = 0;
	// Creation budget: when a whole streaming wave settles at once (~300+ chunks at level load),
	// growing everything in one pass would hang a frame for hundreds of ms. Cap grass GROWTH to a
	// few chunks per pass; the settle-retry keeps the pass running until the queue drains
	// (~140 chunks/s => the whole island greens up smoothly over ~2-3s). Removals are uncapped.
	int creationBudget = 6;
	// GGMAX 3.112: the chunk source. Live terrain = the chunk objects in the scene; baked = the
	// mesh-only grass emitters the bake's teardown left behind (no ObjectComponent, so the objects
	// loop could never see them).
	const size_t sourceCount = terrain ? scene.objects.GetCount() : s_bakeGrassEmitters.size();
	for (size_t oi = 0; oi < sourceCount; oi++)
	{
		wi::ecs::Entity entity;
		wi::scene::MeshComponent* mesh;
		if (terrain)
		{
			wi::scene::ObjectComponent& obj = scene.objects[oi];
			mesh = scene.meshes.GetComponent(obj.meshID);
			entity = scene.objects.GetEntity(oi);
		}
		else
		{
			entity = s_bakeGrassEmitters[oi];
			mesh = scene.meshes.GetComponent(entity);
		}
		if (!mesh || mesh->vertex_positions.size() != wi::terrain::vertexCount) continue;

		const wi::scene::TransformComponent* transform = scene.transforms.GetComponent(entity);
		if (!transform) continue;

		int32_t cx = (int32_t)std::round(transform->world._41 / chunkStride);
		int32_t cz = (int32_t)std::round(transform->world._43 / chunkStride);
		uint64_t key = MakeChunkKey(cx, cz);

		float chunkCenterX = cx * chunkStride;
		float chunkCenterZ = cz * chunkStride;
		bool inEditable = (chunkCenterX >= -editableSize && chunkCenterX <= editableSize &&
			chunkCenterZ >= -editableSize && chunkCenterZ <= editableSize);

		float dx = chunkCenterX - cameraPos.x;
		float dz = chunkCenterZ - cameraPos.z;
		float ringDist = sqrtf(dx * dx + dz * dz) / chunkStride;
		int targetTier_raw = inEditable ? GrassTierForRingDist(ringDist, nearC, midC, outerC) : 0;

		auto entIt = grassChunkKeyToChunkEntity.find(key);
		const bool firstSeen = (entIt == grassChunkKeyToChunkEntity.end());
		bool entityChanged = (!firstSeen && entIt->second != entity);
		if (entityChanged) g_dbgGrassRecycles++; // diag: chunk object entity recycled (flicker trigger)
		if (entityChanged || firstSeen)
		{
			grassChunkKeyToEntityStamp[key] = nowFrame; // settle gate: chunk entity is churning
		}
		auto tierIt = grassChunkKeyToTier.find(key);
		int currentTier = (tierIt != grassChunkKeyToTier.end()) ? tierIt->second : -1;

		// GGMAX 2026-08-05 THE SCULPT-GRASS FIX: validate that the recorded grass entities
		// are still ALIVE. Sculpting invalidates chunks and Generation_Update regenerates
		// them IN PLACE - Scene::Entity_Remove recursively kills the attached grass child
		// (hair-kill tracer: reason=1, wiScene.cpp:1362) but the chunk comes back under the
		// SAME entity id, so the 1.85 entityChanged detector never fires. The record then
		// holds a dead merged entity forever, currentTier == targetTier, and the "already
		// correct" early-out below skips the chunk for the rest of the session: grass gone
		// until the level is reloaded (user repro, TESTPRO1 hill: 6 systems -> 5, no regrow
		// in 45 s, all teardown counters frozen). Same lesson the type histogram already
		// carries: a map record is NOT proof the hair system exists.
		// Repair: drop the dead bookkeeping, mark the chunk bare, and STAMP it - continued
		// sculpt strokes keep re-killing regrown grass, so deferring regrowth to the settle
		// gate makes grass return exactly once, after the stroke ends.
		{
			auto gitCheck = grassChunkKeyToGrassEntities.find(key);
			if (gitCheck != grassChunkKeyToGrassEntities.end())
			{
				bool anyRecorded = false, anyLive = false;
				for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
				{
					wi::ecs::Entity e = gitCheck->second.perType[t];
					if (e != wi::ecs::INVALID_ENTITY)
					{
						anyRecorded = true;
						if (scene.hairs.GetComponent(e) != nullptr) { anyLive = true; break; }
					}
				}
				if (!anyLive && gitCheck->second.merged != wi::ecs::INVALID_ENTITY)
				{
					anyRecorded = true;
					if (scene.hairs.GetComponent(gitCheck->second.merged) != nullptr) anyLive = true;
				}
				if (anyRecorded && !anyLive)
				{
					for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
						gitCheck->second.perType[t] = wi::ecs::INVALID_ENTITY;
					gitCheck->second.merged = wi::ecs::INVALID_ENTITY;
					gitCheck->second.mergedTypeMask[0] = gitCheck->second.mergedTypeMask[1] = 0;
					grassChunkKeyToTier[key] = 0;                 // bare: the grow path re-queues
					currentTier = 0;
					grassChunkKeyToEntityStamp[key] = nowFrame;   // defer regrow until churn settles
					g_grassSettlePending = true;
					g_dbgGrassExternalKills++;                    // GET_PERF_DATA: GRASS_EXTKILLS
				}
			}
		}

		// Hysteresis: chunks that already have grass at a higher tier keep it until the camera moves
		// substantially past the LOD boundary. Without this, small camera nudges (the user wobbling
		// the view, or stepping across a chunk edge) drop grass in/out as ringDist crosses tier
		// boundaries. 0.5 chunk-distance buffer = ~2560 inches of camera slop before tier downgrade
		// — invisible to the user but eliminates pop-out. Only applies on DOWNGRADES; upgrades
		// always take effect immediately so painted grass shows up the moment the camera nears it.
		int targetTier = targetTier_raw;
		if (!g_grassTierShrinkPending && currentTier > targetTier_raw && currentTier > 0)
		{
			constexpr float HYS_MARGIN = 0.5f;
			bool keep = false;
			if (currentTier == 3 && ringDist <= nearC + HYS_MARGIN) keep = true;
			else if (currentTier == 2 && ringDist <= midC + HYS_MARGIN) keep = true;
			else if (currentTier == 1 && ringDist <= outerC + HYS_MARGIN) keep = true;
			if (keep) targetTier = currentTier;
		}

		// GGMAX 3.116: the right tier but built at a different density (Grass Density moved, or the draw
		// distance in DX11 mode) - rebuild through the same path a tier change takes: nearest first, six a
		// pass, and the old grass keeps rendering until its replacement exists. A 2% / 256-strand dead
		// band keeps rounding from ever re-queueing a chunk.
		// Checked whenever the chunk HAS grass, not only at an unchanged tier (review 3.116): a paint stroke
		// erases the tier record, and in per-type mode the paint path would then add the new type at the
		// new count beside old types left at the old one - with builtStrands overwritten, for good.
		bool densityStale = false;
		if (!entityChanged && targetTier > 0)
		{
			auto gs = grassChunkKeyToGrassEntities.find(key);
			if (gs != grassChunkKeyToGrassEntities.end() && gs->second.builtStrands > 0)
			{
				const uint32_t want = GrassStrandsForTier(targetTier);
				const uint32_t have = gs->second.builtStrands;
				const uint32_t diff = (want > have) ? want - have : have - want;
				if (diff >= 256 && diff * 50u > have) { densityStale = true; g_dbgGrassDensityStale++; }
			}
		}
		if (!entityChanged && currentTier == targetTier && !densityStale) continue; // already correct for this chunk

		auto git = grassChunkKeyToGrassEntities.find(key);
		bool hasExistingEntities = false;
		if (git != grassChunkKeyToGrassEntities.end())
		{
			for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
			{
				if (git->second.perType[t] != wi::ecs::INVALID_ENTITY) { hasExistingEntities = true; break; }
			}
			// GGMAX 1.85 — THE MERGED-GRASS BUG. In merged mode perType[] is ALWAYS empty (the
			// chunk's one entity lives in the `merged` slot), so this predicate read false forever
			// and the chunk never got the repair pass that regrows grass after Wicked recycles it.
			//
			// Measured, not reasoned: the hair-kill tracer recorded 9 removals of live hair
			// entities, ALL of them reason=1 (pulled down as a recursive child of the chunk they
			// are Component_Attach'ed to), Scene::Clear wiped 0, and the grass code's own teardown
			// counters read fullResets=0. So the kill is normal chunk recycling — which per-type
			// grass also suffers 574 times and survives, because THIS line sees its entities and
			// re-queues the chunk. Merged grass was created 9 times, killed 9 times, and never
			// rebuilt.
			//
			// 1.74 added the `merged` slot and correctly updated every TEARDOWN path (see the
			// comment at the fullReset branch below) but missed this one QUEUEING predicate.
			if (git->second.merged != wi::ecs::INVALID_ENTITY) hasExistingEntities = true;
		}

		// SETTLE GATE: while the chunk's entity is still churning (progressive regeneration during
		// the initial build, sculpt-driven regens), DEFER growing grass — but still tear down grass
		// orphaned by a recycle immediately (its mesh entity is gone). Each chunk then grows grass
		// exactly once, after its final regeneration, instead of popping on every intermediate one.
		if (targetTier > 0)
		{
			auto stampIt = grassChunkKeyToEntityStamp.find(key);
			const bool settled = (stampIt == grassChunkKeyToEntityStamp.end()) ||
				((nowFrame - stampIt->second) >= GG_GRASS_SETTLE_FRAMES);
			if (!settled)
			{
				if (entityChanged && hasExistingEntities)
				{
					// remove-only pass: tier 0 + fullReset drops the dead-mesh hair entities and
					// records the new chunk entity; regrowth happens once the stamp ages out
					// (or the next budgeted pass picks it up).
					pending.push_back({ entity, key, mesh, transform->world, 0, true, true, ringDist });
				}
				else
				{
					// nothing to remove — just track the (possibly new) entity, keep/record bare tier
					grassChunkKeyToChunkEntity[key] = entity;
					if (currentTier < 0) grassChunkKeyToTier[key] = 0;
				}
				g_grassSettlePending = true;
				continue;
			}
			// NOTE: the creation budget is applied AFTER the scan now (nearest chunks first) —
			// see the priority pass below. Settled grow candidates all enter `pending` here.
		}

		// Chunks that need NO scene mutation (no old grass to remove AND new tier wants no grass)
		// are recorded inline without going through Phase 2. This avoids triggering
		// Generation_Cancel for the common case of Wicked discovering a new far chunk during its
		// background streaming — each Cancel interrupts Wicked's own chunk-removal pass, leaving
		// stale chunks in scene.objects, and during paint the chunk count was growing at ~65/sec.
		if (!hasExistingEntities && targetTier == 0)
		{
			grassChunkKeyToChunkEntity[key] = entity;
			grassChunkKeyToTier[key] = targetTier;
			continue;
		}

		// Force a fullReset when the tier itself is changing on a chunk that still has hair entities.
		// Without this, an existing chunk that upgrades from tier 1 (5000 strands) to tier 3 (100000
		// strands) keeps its old low-density entities — the existing-entity branch in Phase 2 only
		// updates vertex_lengths, not strandCount, so the visible density stays stuck at the lower
		// tier even after the chunk re-enters the dense ring.
		bool tierChanged = (currentTier > 0 && currentTier != targetTier && hasExistingEntities) || densityStale;

		// Capture the world matrix by value (a transform pointer would be invalidated when we
		// create transform components for grass entities in Phase 2).
		pending.push_back({ entity, key, mesh, transform->world, targetTier, entityChanged || tierChanged, entityChanged, ringDist });
	}

	if (pending.empty())
	{
		g_grassTierShrinkPending = false; // nothing left to re-tier — shrink sweep complete
		return;
	}

	// Priority pass (2026-07-27, user-reported travel flicker): the creation budget used to be
	// consumed in scene-iteration order, so during travel churn the visible gaps RIGHT IN FRONT
	// of the player could wait many retry passes while off-screen far chunks regrew first.
	// Now: sort by camera distance, grow the nearest `creationBudget` chunks this pass, and
	// demote the rest — dead-mesh chunks still get their (uncapped) teardown immediately,
	// live-grass tier changes simply keep their old grass until a later pass reaches them.
	{
		std::sort(pending.begin(), pending.end(),
			[](const PendingGrass& a, const PendingGrass& b) { return a.ringDist < b.ringDist; });
		size_t writeIdx = 0;
		bool anyDemoted = false;
		for (size_t i = 0; i < pending.size(); i++)
		{
			PendingGrass pc = pending[i];
			if (pc.tier > 0)
			{
				if (creationBudget > 0)
				{
					creationBudget--;
				}
				else if (pc.deadMesh)
				{
					// over budget but the old hair references a vanished mesh — teardown now,
					// record bare tier so the settle-retry regrows it when its turn comes
					pc.tier = 0;
					pc.fullReset = true;
					anyDemoted = true;
				}
				else
				{
					// over budget, old grass still valid — skip entirely (keeps rendering at the
					// old tier; the unchanged tier record refires this chunk on a later pass)
					anyDemoted = true;
					continue;
				}
			}
			pending[writeIdx++] = pc;
		}
		pending.resize(writeIdx);
		if (anyDemoted) g_grassSettlePending = true; // re-arms the every-3rd-frame retry pass
		else g_grassTierShrinkPending = false;       // sweep fit in budget — hysteresis resumes
	}

	if (pending.empty()) return;

	// Generation cancelled = safe window to mutate the scene (no generator-thread race).
	// (Baked: there is no generator - GGMAX 3.112.)
	if (terrain) terrain->Generation_Cancel();

	// Phase 2: apply removals + (re)creations and update tracking.
	for (auto& pc : pending)
	{
		ChunkGrassEntities& existingEntities = grassChunkKeyToGrassEntities[pc.key]; // default-construct if first time

		// Full reset paths: Wicked recycled the chunk (old hair entities reference a vanished
		// mesh) OR the chunk's new tier is 0 (bare — no grass should render here at all). Both
		// drop every per-type entity for this chunk so we start clean.
		if (pc.fullReset || pc.tier == 0)
		{
			for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
			{
				wi::ecs::Entity e = existingEntities.perType[t];
				if (e != wi::ecs::INVALID_ENTITY && scene.hairs.GetComponent(e))
				{
					scene.Entity_Remove(e);
					g_dbgGrassFullResets++; // diag: old hair torn down (recycle/bare gap)
				}
				existingEntities.perType[t] = wi::ecs::INVALID_ENTITY;
			}
			// GGMAX 1.74: the merged entity lives in its own slot and must be torn down here too,
			// otherwise a recycled chunk keeps a hair system pointing at a vanished mesh.
			if (existingEntities.merged != wi::ecs::INVALID_ENTITY)
			{
				if (scene.hairs.GetComponent(existingEntities.merged))
				{
					scene.Entity_Remove(existingEntities.merged);
					g_dbgGrassFullResets++;
				}
				existingEntities.merged = wi::ecs::INVALID_ENTITY;
				existingEntities.mergedTypeMask[0] = existingEntities.mergedTypeMask[1] = 0;
			}
			existingEntities.builtStrands = 0;
		}

		grassChunkKeyToChunkEntity[pc.key] = pc.entity;
		grassChunkKeyToTier[pc.key] = pc.tier;
		if (pc.tier == 0) continue; // bare at this distance / outside editable area

		// Stage B.4: per-cell visibility is the simulate CS's job (Option B sample at the strand's
		// world XZ). C++ only needs to know which per-(chunk, type) hair entities should exist.
		// A single scan of the chunk's world-AABB grass-map cells gives us that — no per-vertex
		// multi-sample, no coverage scaling, no vertex_lengths restamp on paint.
		bool typesSeen[GGGRASS_TOTAL_REAL_TYPES] = {};
		const float halfChunkWorld = chunkStride * 0.5f;
		GGGrass::GGGrass_ScanRegion(
			pc.world._41 - halfChunkWorld, pc.world._43 - halfChunkWorld,
			pc.world._41 + halfChunkWorld, pc.world._43 + halfChunkWorld,
			typesSeen );

		// GGMAX 1.74 MERGED GRASS: one hair system for the whole chunk instead of one per painted
		// type. Measured on the benchmark scene, chunks carry 7-8 types each, so the per-type split
		// allocated ~10x the strand buffers it drew and simulated ~10x the strands — every strand
		// on another type's cell was still placed, still run through physics, and still emitted a
		// degenerate zero-area quad. The merged system keeps the SAME strandCount, emitter mesh,
		// index list and randomSeed, so strand i lands on the identical triangle with identical
		// barycentrics; a paint cell holds exactly one type, so the union of what the per-type
		// systems drew is exactly what this one draws. Placement is bit-identical by construction.
		if (gg_grass_merge)
		{
			ProcessGrassChunkMerged(scene, pc, existingEntities, typesSeen, editableSize);
			continue;
		}

		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
		{
			wi::ecs::Entity existing = existingEntities.perType[t];

			if (existing != wi::ecs::INVALID_ENTITY)
			{
				if (typesSeen[t]) continue; // entity already here, type still painted — done
				// Type erased from this chunk — drop its entity so the simulate CS isn't dispatching
				// strands that will all be culled to zero length by the shader visibility check.
				if (scene.hairs.GetComponent(existing))
					scene.Entity_Remove(existing);
				existingEntities.perType[t] = wi::ecs::INVALID_ENTITY;
				continue;
			}

			if (!typesSeen[t]) continue;

			// CREATE: first paint of this type in this chunk (or full reset path).
			wi::scene::MaterialComponent* mat = BuildGrassMaterial(t);
			if (!mat) continue;

			// vertex_lengths = all 1.0 forever. CreateFromMesh keeps every chunk triangle in the
			// index buffer (triangleCount = constant); the per-strand shader sample (Stage B.3) is
			// the sole visibility authority. Paint events do not touch this entity again.
			wi::vector<float> vertex_lengths;
			vertex_lengths.resize(wi::terrain::vertexCount, 1.0f);

			wi::ecs::Entity grassEntity = wi::ecs::CreateEntity();
			wi::HairParticleSystem& hair = scene.hairs.Create(grassEntity);
			hair = g_grassAppearance[t];   // per-type look (length/width/billboards/stiffness)
			hair.meshID = pc.entity;
			hair.vertex_lengths = std::move(vertex_lengths);

			// Stage 3 Option B: tell the simulate CS this entity represents grass type `t` and
			// hand it the GG paint mask. xHairGrassType is 1-based so 0 means "feature disabled"
			// for non-GG hair systems. invWorldSize matches GGGrass_GetGrassMap's CPU formula:
			// uv = worldXZ * (0.5 / editableSize) + 0.5  (editableSize is HALF the world extent).
			hair.grass_type = (uint32_t)t + 1;
			hair.grass_map_inv_world_size = 0.5f / editableSize;
			hair.grass_map_origin_x = 0.0f;
			hair.grass_map_origin_z = 0.0f;
			hair.grass_visibility_texture = GGGrass::GGGrass_GetMapTexture();

			// Fixed strand count per tier — independent of paint state, so painting a new type
			// into the chunk never perturbs an existing entity's strandCount.
			const uint32_t strands = GrassStrandsForTier(pc.tier);   // GGMAX 3.116: latched Grass Density
			hair.strandCount = strands;
			existingEntities.builtStrands = strands;

			hair.CreateFromMesh(*pc.mesh);
			// Force FP32 position buffer (CreateFromMesh defaults to R16G16B16A16_UNORM for static
			// base meshes; the resulting ~0.08-inch quantization step over a 5280-inch chunk AABB
			// shows up as visibly choppy micro-sway on slow-moving tips).
			hair.position_format = wi::graphics::Format::R32G32B32A32_FLOAT;
			hair.CreateRenderData();

			scene.materials.Create(grassEntity) = *mat;
			scene.transforms.Create(grassEntity);
			scene.Component_Attach(grassEntity, pc.entity, true); // inherit chunk transform

			existingEntities.perType[t] = grassEntity;
			g_dbgGrassRecreates++; // diag: grass hair (re)created
		}
	}

	// Step 1 auto-resolve (see SCRATCHPAD "Advanced Grass Settings — DX11 Baseline Port Plan"):
	// GGGrass_ScanRegion resolves encoded==1 cells from terrain height/material (a real_type, or 0 on
	// ground with no matching grass - 3.118; a cell whose lookups fail stays 1). If any cell was
	// rewritten, the R8_UNORM texGrassMap on the GPU still holds
	// the pre-resolve byte, so the hair simulate CS (Option B mask check) will keep matching only
	// Course Grass entities until we re-upload. Fold that into a single 16 MB upload per frame
	// regardless of how many chunks resolved — the paint stroke path already uses this cadence
	// (~10 Hz) without measurable overhead.
	if ( GGGrass::GGGrass_TakePendingMapUpload() )
	{
		GGGrass::GGGrass_UploadGrassMap();
	}
}


// Remove all current grass entities and clear tracking so ProcessGrassChunks rebuilds them next
// frame with the updated template/material (used after a live param change via SET_GRASS).
static void ForceGrassRebuild()
{
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (terrain) terrain->Generation_Cancel();   // make scene mutation safe (no generator-thread race)
	auto& scene = wi::scene::GetScene();
	for (auto& kv : grassChunkKeyToGrassEntities)
	{
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
		{
			wi::ecs::Entity e = kv.second.perType[t];
			if (e != wi::ecs::INVALID_ENTITY && scene.hairs.GetComponent(e))
				scene.Entity_Remove(e);
		}
		// GGMAX 1.74: merged entity too
		if (kv.second.merged != wi::ecs::INVALID_ENTITY && scene.hairs.GetComponent(kv.second.merged))
			scene.Entity_Remove(kv.second.merged);
	}
	grassChunkKeyToGrassEntities.clear();
	grassChunkKeyToTier.clear();
	grassChunkKeyToChunkEntity.clear();
}

// Runtime grass tuning, driven by the SET_GRASS automation command. Now that each grass type has
// its own appearance template and material, the tuning knobs apply uniformly across all types
// (multiplicative for sizes, replacement for material props). A deferred rebuild applies them.
// GGMAX 3.112: drop the bake's grass emitters (and everything that grew on them) and the bare-chunk
// copies. Called on the way back to a live terrain (GGTerrainWicked_Init), where the real chunks
// regrow the grass. Entity ids are never reused, so removing an id a level load already swept away
// is a harmless no-op. The grass maps are cleared only if emitters existed - Adopt calls this too,
// at a moment when the maps still hold the LIVE terrain's records it is about to re-home.
static void ReleaseBakeGrassEmitters()
{
	auto& scene = wi::scene::GetScene();
	const bool hadEmitters = !s_bakeGrassEmitters.empty();
	for (wi::ecs::Entity e : s_bakeGrassEmitters)
		scene.Entity_Remove(e);   // recursive: the grass re-parented onto it goes with it
	s_bakeGrassEmitters.clear();
	s_bakeGrassEmitterByKey.clear();
	s_bakeGrassBare.clear();
	wi::vector<uint32_t>().swap(s_bakeGrassLod0Indices);
	s_bakeGrassPromotePending = false;
	s_bakeGrassRehomed = 0;
	s_bakeGrassPromoted = 0;
	if (hadEmitters)
	{
		grassChunkKeyToGrassEntities.clear();
		grassChunkKeyToTier.clear();
		grassChunkKeyToChunkEntity.clear();
	}
}

// GGMAX 3.112 (review 2): the bake's own Clear releases the grass too. Init is not enough on its
// own: a level saved as Completely Empty never runs GGTerrainWicked_Update (master_part1 gates the
// whole terrain block), so a baked outdoor level followed by an indoor one kept every emitter, hair
// and CPU copy alive for as long as the player stayed indoors. Clear is where the bake ends on every
// path - untick, level load, Reset Visuals, the near-detail slider - and Init's call is then a no-op.
void GGTerrainWicked_ReleaseBakeGrass()
{
	ReleaseBakeGrassEmitters();
}

// A new emitter entity at a chunk's world transform, registered as a grass chunk source. The caller
// gives it its MeshComponent.
static wi::ecs::Entity NewBakeGrassEmitter(uint64_t key, const XMFLOAT4X4& world)
{
	auto& scene = wi::scene::GetScene();
	const wi::ecs::Entity e = wi::ecs::CreateEntity();
	scene.names.Create(e) = "GGGrassEmitter";   // ⚠ WickedCall_BuildStaticMeshBVHs skips by this name
	wi::scene::TransformComponent& t = scene.transforms.Create(e);
	t.MatrixTransform(world);
	t.UpdateTransform();
	s_bakeGrassEmitters.push_back(e);
	s_bakeGrassEmitterByKey[key] = e;
	return e;
}

// Bare chunk -> emitter, if grass has landed on it. A lean mesh: the chunk's own positions and normals
// and the shared LOD0 triangles - everything a hair reads (GPU: vb_pos_wind, vb_nor, aabb; CPU on a
// rebuild: positions.size() and the LOD0 indices). About 1 ms (CreateRenderData). Returns true if
// promoted.
static bool PromoteBareGrassChunk(uint64_t key)
{
	auto it = s_bakeGrassBare.find(key);
	if (it == s_bakeGrassBare.end() || s_bakeGrassLod0Indices.empty()) return false;
	const XMFLOAT4X4 world = it->second.world;
	const float half = s_bakeGrassChunkStride * 0.5f;
	if (!GGGrass::GGGrass_RegionHasGrass(world._41 - half, world._43 - half, world._41 + half, world._43 + half))
		return false;
	auto& scene = wi::scene::GetScene();
	const wi::ecs::Entity e = NewBakeGrassEmitter(key, world);
	wi::scene::MeshComponent& m = scene.meshes.Create(e);
	m.SetQuantizedPositionsDisabled(true);   // as wiTerrain builds every chunk mesh
	m.vertex_positions = std::move(it->second.positions);
	m.vertex_normals = std::move(it->second.normals);
	m.indices = s_bakeGrassLod0Indices;
	wi::scene::MeshComponent::MeshSubset& ss = m.subsets.emplace_back();
	ss.indexOffset = 0;
	ss.indexCount = (uint32_t)m.indices.size();   // materialID stays INVALID: nothing draws this mesh
	m.CreateRenderData();
	s_bakeGrassBare.erase(it);
	// Record it as this key's chunk entity NOW. Seen for the first time by ProcessGrassChunks, it
	// would wait out the 30-frame settle gate that exists for chunks Wicked is still regenerating.
	grassChunkKeyToChunkEntity[key] = e;
	s_bakeGrassPromoted++;
	return true;
}

// Every bare chunk that now holds grass, nearest the camera first, at most `budget` a call (each is a
// CreateRenderData). Leaves s_bakeGrassPromotePending set until a call finishes the list.
static void PromoteBareGrassChunks(const XMFLOAT3& eye, uint32_t budget)
{
	wi::vector<std::pair<float, uint64_t>> order;
	order.reserve(s_bakeGrassBare.size());
	for (auto& kv : s_bakeGrassBare)
	{
		const float dx = kv.second.world._41 - eye.x, dz = kv.second.world._43 - eye.z;
		order.push_back({ dx * dx + dz * dz, kv.first });
	}
	std::sort(order.begin(), order.end());
	uint32_t done = 0;
	for (auto& o : order)
	{
		if (done >= budget) return;   // more next frame
		if (PromoteBareGrassChunk(o.second)) done++;
	}
	s_bakeGrassPromotePending = false;
}

// Grass Off while baked: an emitter's video memory is for the grass alone, so every emitter goes back
// to a CPU-only copy (GPU buffers freed with the entity). ForceGrassRebuild has already removed the
// hairs and cleared the grass maps.
static void DemoteBakeGrassEmitters()
{
	auto& scene = wi::scene::GetScene();
	for (auto& kv : s_bakeGrassEmitterByKey)
	{
		wi::scene::MeshComponent* m = scene.meshes.GetComponent(kv.second);
		const wi::scene::TransformComponent* t = scene.transforms.GetComponent(kv.second);
		if (m != nullptr && t != nullptr && m->vertex_positions.size() == wi::terrain::vertexCount)
		{
			BakeGrassBareChunk& b = s_bakeGrassBare[kv.first];
			b.world = t->world;
			b.positions = std::move(m->vertex_positions);
			b.normals = std::move(m->vertex_normals);
		}
		scene.Entity_Remove(kv.second);
	}
	s_bakeGrassEmitters.clear();
	s_bakeGrassEmitterByKey.clear();
}

// GGMAX 3.112: called by GGTerrainWicked_Shutdown for a TERRAIN BAKE only - after the generator and
// the async VT job are joined (nothing else is touching the chunk meshes) and BEFORE the chunk
// entities are removed. See the note at s_bakeGrassEmitters.
static void AdoptGrassEmittersForBake(wi::terrain::Terrain* terrain)
{
	ReleaseBakeGrassEmitters();   // belt and braces - a bake always starts from a live terrain
	wi::Timer adoptTimer;
	auto& scene = wi::scene::GetScene();
	const float chunkStride = (wi::terrain::chunk_width - 1) * terrain->chunk_scale;
	const float editableSize = GGTerrain::GGTerrain_GetEditableSize();
	s_bakeGrassChunkStride = chunkStride;
	if (chunkStride <= 0.0f) return;

	std::unordered_set<uint64_t> adopted;   // keys whose grass records survive (they got an emitter)
	for (auto& [chunk, chunk_data] : terrain->chunks)
	{
		const wi::ecs::Entity chunkEntity = chunk_data.entity;
		if (chunkEntity == wi::ecs::INVALID_ENTITY) continue;
		const wi::scene::ObjectComponent* obj = scene.objects.GetComponent(chunkEntity);
		const wi::scene::TransformComponent* tr = scene.transforms.GetComponent(chunkEntity);
		if (obj == nullptr || tr == nullptr) continue;
		const wi::ecs::Entity meshEntity = obj->meshID;
		const wi::scene::MeshComponent* srcMesh = scene.meshes.GetComponent(meshEntity);
		if (srcMesh == nullptr || srcMesh->vertex_positions.size() != wi::terrain::vertexCount ||
			srcMesh->vertex_normals.size() != wi::terrain::vertexCount) continue;
		// BY VALUE: the Create calls below can grow the component arrays and move what tr points at.
		const XMFLOAT4X4 world = tr->world;

		// The same key and editable-area test ProcessGrassChunks uses - grass never grows outside it.
		const int32_t cx = (int32_t)std::round(world._41 / chunkStride);
		const int32_t cz = (int32_t)std::round(world._43 / chunkStride);
		const uint64_t key = MakeChunkKey(cx, cz);
		const float ccx = cx * chunkStride, ccz = cz * chunkStride;
		if (ccx < -editableSize || ccx > editableSize || ccz < -editableSize || ccz > editableSize) continue;

		// The LOD0 triangle list, once - every chunk shares wiTerrain's one ChunkIndices topology.
		if (s_bakeGrassLod0Indices.empty())
		{
			uint32_t firstSubset = 0, lastSubset = 0;
			srcMesh->GetLODSubsetRange(0, firstSubset, lastSubset);
			for (uint32_t si = firstSubset; si < lastSubset; si++)
			{
				const wi::scene::MeshComponent::MeshSubset& ss = srcMesh->subsets[si];
				s_bakeGrassLod0Indices.insert(s_bakeGrassLod0Indices.end(),
					srcMesh->indices.begin() + ss.indexOffset, srcMesh->indices.begin() + ss.indexOffset + ss.indexCount);
			}
		}

		// Painted grass (and grass not switched off) = an emitter now; otherwise a CPU copy that can
		// become one later. ⚠ NOT GGGrass_ScanRegion for the test: its Match Terrain Color auto-resolve
		// costs a height + material lookup per unresolved cell, and asked of every chunk at once it
		// froze the first bake for 4.4 s. A raw yes/no is all this needs; the resolve still happens,
		// lazily, when ProcessGrassChunks actually grows the chunk.
		const float half = chunkStride * 0.5f;
		const bool grassy = !gg_no_grass &&
			GGGrass::GGGrass_RegionHasGrass(world._41 - half, world._43 - half, world._41 + half, world._43 + half);
		if (!grassy)
		{
			// MOVE the arrays out - the chunk mesh is removed a few lines from now.
			wi::scene::MeshComponent* src = scene.meshes.GetComponent(meshEntity);
			BakeGrassBareChunk& b = s_bakeGrassBare[key];
			b.world = world;
			b.positions = std::move(src->vertex_positions);
			b.normals = std::move(src->vertex_normals);
			continue;
		}

		const wi::ecs::Entity emitter = NewBakeGrassEmitter(key, world);
		// MOVE the chunk's mesh, do not copy or rebuild it. The chunk is removed a few lines from now,
		// and a move hands the CPU arrays and the GPU buffers (with their descriptors) over as they
		// are: nothing is allocated, nothing uploaded, and a strand still lands on exactly the
		// triangle it was on. (Rebuilding a lean mesh for all of them - 67 MB instead of 107 MB on
		// DX12TESTLEVEL - was tried: 361 CreateRenderData calls are a ~300 ms hitch on the frame the
		// bake lands.) Re-fetch the source AFTER Create - Create can reallocate the meshes array.
		wi::scene::MeshComponent& em = scene.meshes.Create(emitter);
		wi::scene::MeshComponent* src = scene.meshes.GetComponent(meshEntity);
		em = std::move(*src);
		// Drop what only a DRAWN, PICKED or RE-CREATED mesh needs - a hair reads positions.size(),
		// the LOD0 indices and the GPU positions/normals. The CPU triangle BVH that wiTerrain gives
		// every chunk is ~1.15 MB alone (Scene::Intersects only reaches meshes through objects, and an
		// emitter has none); UVs + tangents are ~108 KB. Kept, they cost ~415 MB of system memory on
		// DX12TESTLEVEL (review 3.112). SetBVHEnabled(false) only clear()s the leaf AABBs - swap.
		em.SetBVHEnabled(false);
		wi::vector<wi::primitive::AABB>().swap(em.bvh_leaf_aabbs);
		wi::vector<XMFLOAT2>().swap(em.vertex_uvset_0);
		wi::vector<XMFLOAT4>().swap(em.vertex_tangents);

		// Re-parent the chunk's live grass onto the emitter - exactly the state ProcessGrassChunks
		// creates it in (identity local transform, attached in local space, meshID = its parent).
		auto git = grassChunkKeyToGrassEntities.find(key);
		if (git != grassChunkKeyToGrassEntities.end())
		{
			auto rehome = [&](wi::ecs::Entity h)
			{
				if (h == wi::ecs::INVALID_ENTITY) return;
				wi::HairParticleSystem* hair = scene.hairs.GetComponent(h);
				if (hair == nullptr) return;
				hair->meshID = emitter;
				scene.Component_Detach(h);
				wi::scene::TransformComponent* ht = scene.transforms.GetComponent(h);
				if (ht != nullptr) ht->ClearTransform();
				scene.Component_Attach(h, emitter, true);
				s_bakeGrassRehomed++;
			};
			for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++) rehome(git->second.perType[t]);
			rehome(git->second.merged);
		}
		grassChunkKeyToChunkEntity[key] = emitter;   // so the pass does not read it as a recycle
		adopted.insert(key);
	}
	// Grass Off at bake time: everything is a CPU copy; grass coming back promotes them (see the
	// Grass Off edges in UpdateWickedGrassManager).
	if (gg_no_grass) s_bakeGrassPromotePending = true;

	// A record for a chunk that got no emitter names entities that die with that chunk below. Drop
	// it now, so no map is left holding an id that is about to be removed.
	for (auto it = grassChunkKeyToGrassEntities.begin(); it != grassChunkKeyToGrassEntities.end(); )
		it = adopted.count(it->first) ? std::next(it) : grassChunkKeyToGrassEntities.erase(it);
	for (auto it = grassChunkKeyToTier.begin(); it != grassChunkKeyToTier.end(); )
		it = adopted.count(it->first) ? std::next(it) : grassChunkKeyToTier.erase(it);
	for (auto it = grassChunkKeyToChunkEntity.begin(); it != grassChunkKeyToChunkEntity.end(); )
		it = adopted.count(it->first) ? std::next(it) : grassChunkKeyToChunkEntity.erase(it);
	s_bakeGrassAdoptMs = (float)adoptTimer.elapsed_milliseconds();
}

// GGMAX 3.112: DUMP_BAKE reporting.
void GGTerrainWicked_GetBakeGrassStats(int* pEmitters, int* pBare, int* pRehomed, int* pPromoted,
	int* pLiveSystems, unsigned long long* pMeshBytes, float* pAdoptMs)
{
	auto& scene = wi::scene::GetScene();
	int live = 0;
	unsigned long long bytes = 0;
	for (wi::ecs::Entity e : s_bakeGrassEmitters)
	{
		const wi::scene::MeshComponent* m = scene.meshes.GetComponent(e);
		if (m != nullptr && m->generalBuffer.IsValid()) bytes += m->generalBuffer.desc.size;
	}
	for (auto& kv : grassChunkKeyToGrassEntities)
	{
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES + 1; t++)
		{
			wi::ecs::Entity h = (t < GGGRASS_TOTAL_REAL_TYPES) ? kv.second.perType[t] : kv.second.merged;
			if (h != wi::ecs::INVALID_ENTITY && scene.hairs.GetComponent(h) != nullptr) live++;
		}
	}
	*pEmitters = (int)s_bakeGrassEmitters.size();
	*pBare = (int)s_bakeGrassBare.size();
	*pRehomed = (int)s_bakeGrassRehomed;
	*pPromoted = (int)s_bakeGrassPromoted;
	*pLiveSystems = live;
	*pMeshBytes = bytes;
	*pAdoptMs = s_bakeGrassAdoptMs;
}

void GGTerrainWicked_SetGrassParam(const char* param, float value)
{
	std::string p = param ? param : "";
	auto forAllAppearance = [&](auto fn) {
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++) fn(g_grassAppearance[t]);
	};
	auto forAllMaterials = [&](auto fn) {
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
			if (g_grassMaterialReady[t]) fn(g_grassMaterials[t]);
	};
	if      (p == "length")     forAllAppearance([&](wi::HairParticleSystem& h){ h.length = value; });
	else if (p == "width")      forAllAppearance([&](wi::HairParticleSystem& h){ h.width = value; });
	else if (p == "stiffness")  forAllAppearance([&](wi::HairParticleSystem& h){ h.stiffness = value; });
	else if (p == "drag")       forAllAppearance([&](wi::HairParticleSystem& h){ h.drag = value; });
	else if (p == "viewdist")   forAllAppearance([&](wi::HairParticleSystem& h){ h.viewDistance = value; });
	else if (p == "segments")   forAllAppearance([&](wi::HairParticleSystem& h){ h.segmentCount = (uint32_t)(value < 1.0f ? 1.0f : value); });
	else if (p == "billboards") forAllAppearance([&](wi::HairParticleSystem& h){ h.billboardCount = (uint32_t)(value < 1.0f ? 1.0f : value); });
	else if (p == "blades")     g_grassBladesPerVertex = (uint32_t)(value < 1.0f ? 1.0f : value);
	else if (p == "maxstrands") g_grassMaxStrands = (uint32_t)(value < 1.0f ? 1.0f : value);
	else if (p == "lodchunks")  g_grassLODChunksOverride = value;     // hard override outer ring (0 = follow slider)
	else if (p == "tier3")      g_grassTier3Chunks = std::max(0.5f, value); // full-density ring in chunk-distances
	else if (p == "tier2")      g_grassTier2Chunks = std::max(0.5f, value); // mid-density ring in chunk-distances
	else if (p == "sss")        forAllMaterials([&](wi::scene::MaterialComponent& m){ m.SetSubsurfaceScatteringAmount(value); });
	else if (p == "alpha")      forAllMaterials([&](wi::scene::MaterialComponent& m){ m.SetAlphaRef(value); });
	else if (p == "tintr")      forAllMaterials([&](wi::scene::MaterialComponent& m){ m.baseColor.x = value; m.SetDirty(); });
	else if (p == "tintg")      forAllMaterials([&](wi::scene::MaterialComponent& m){ m.baseColor.y = value; m.SetDirty(); });
	else if (p == "tintb")      forAllMaterials([&](wi::scene::MaterialComponent& m){ m.baseColor.z = value; m.SetDirty(); });
	else if (p == "sssr")       forAllMaterials([&](wi::scene::MaterialComponent& m){ m.SetSubsurfaceScatteringColor(XMFLOAT3(value, m.subsurfaceScattering.y, m.subsurfaceScattering.z)); });
	else if (p == "sssg")       forAllMaterials([&](wi::scene::MaterialComponent& m){ m.SetSubsurfaceScatteringColor(XMFLOAT3(m.subsurfaceScattering.x, value, m.subsurfaceScattering.z)); });
	else if (p == "sssb")       forAllMaterials([&](wi::scene::MaterialComponent& m){ m.SetSubsurfaceScatteringColor(XMFLOAT3(m.subsurfaceScattering.x, m.subsurfaceScattering.y, value)); });
	g_grassRebuildRequested = true;
}


// Lazily build the brush-cursor decal entity (material + texture + decal + transform). One-time;
// reused every frame thereafter. Texture is the procedurally-generated 1024-px brush_ring.png.
static void SetupBrushCursor()
{
	using namespace wi::scene;
	auto& scene = wi::scene::GetScene();

	// Resilience: scene wipes on level load destroy our entity even though our static flag stays
	// true. Verify all three components still exist; if not, treat this as a re-init and rebuild.
	if (g_brushCursorSetup)
	{
		if (scene.materials.GetComponent(g_brushCursorEntity) &&
			scene.transforms.GetComponent(g_brushCursorEntity) &&
			scene.decals.GetComponent(g_brushCursorEntity))
		{
			return;
		}
		g_brushCursorSetup = false;
	}
	if (wickedTerrainExeDir.empty()) return; // wait for Init() to capture the EXE dir

	g_brushCursorEntity = wi::ecs::CreateEntity();

	char texPath[512];
	sprintf_s(texPath, "%s/Files/editors/gfx/brush_ring.png", wickedTerrainExeDir.c_str());

	MaterialComponent& mat = scene.materials.Create(g_brushCursorEntity);
	mat.textures[MaterialComponent::BASECOLORMAP].name = texPath;
	mat.baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f); // start hidden via alpha multiplier
	mat.SetCastShadow(false);
	mat.SetTextureStreamingDisabled(true);
	mat.CreateRenderData();

	TransformComponent& tx = scene.transforms.Create(g_brushCursorEntity);
	// Wicked decals project along local -Z. Rotate the entity -90 deg around X so its local -Z axis
	// points to world -Y (straight down into the terrain).
	XMVECTOR rot = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), -XM_PIDIV2);
	XMStoreFloat4(&tx.rotation_local, rot);
	tx.SetDirty();

	DecalComponent& decal = scene.decals.Create(g_brushCursorEntity);
	decal.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // material baseColor multiplies texture; this is multiplied in
	decal.SetBaseColorOnlyAlpha(false);             // RGBA from texture (we want the green to come through)

	g_brushCursorSetup = true;
}


// GG-side globals — declared at GLOBAL scope (block-scope externs inside
// namespace GGTerrain mangle namespace-qualified and fail to link). The message
// pump keeps the window responding while the pregenerate loop blocks at the
// end of a level load.
extern bool bKeepWindowsResponding;
void EmptyMessages(void);
void timestampactivity(int i, char* desc_s);

// GGMAX 2.53: game-callable setter for the generation-center override (global scope for the
// same linkage reason as above). The Terrain Generator calls this every frame it is open,
// pinning the chunk ring to the editable-area marker; the per-frame auto-clear below reverts
// to camera-centred generation the moment the generator is no longer the active mode.
void GGTerrainWicked_SetGenCenterOverride(float fWorldX, float fWorldZ, bool bEnable)
{
	wi::terrain::gg_generation_center_override_x = fWorldX;
	wi::terrain::gg_generation_center_override_z = fWorldZ;
	wi::terrain::gg_generation_center_override_enabled = bEnable;
}
extern bool bProceduralLevel; // GGMAX 2.53: the Terrain Generator mode flag (game global)
extern bool g_ggTerrainGenEntryPending; // GGMAX 2.59: entry recipes set it BEFORE the flat-level load
extern bool GGGame_IsEmptyLevelMode(void); // GGMAX 2.68g: t.visuals.bEnableEmptyLevelMode (M-TerrainNew_part4)
extern bool gg_terrain_bake;  // GGMAX 3.73: Terrain Bake switch (GGTerrainBake.cpp, global scope)

namespace GGTerrain
{

// ---------------------------------------------------------------------------
// Level reveal hold (2026-07-18). The safe replacement for the reverted
// synchronous pregeneration: the editor covers the 3D view with a loading
// overlay while REAL frames run underneath — the legacy terrain finishes its
// height readback (GGTerrain_IsReady), the generator builds the camera-facing
// cone with CORRECT heights (deltas 7+8 + boosted budget), and the cover
// drops the moment the visible chunk set exists. Deadline-capped so a stall
// can never black-screen the editor.
// ---------------------------------------------------------------------------
static int g_revealHoldFrames = 0;

void GGTerrainWicked_BeginRevealHold()
{
	if (!wickedTerrainInitialised) return;
	// GGMAX 2.59: a load heading into the Terrain Generator suppresses chunk generation
	// (the ring would be wiped on arrival), so the cover's early-drop condition can never
	// trigger - don't arm it, the "Preparing the Terrain Generator" message covers UX.
	if (g_ggTerrainGenEntryPending) return;
	g_revealHoldFrames = 300;  // ~5s deadline backstop at 60fps
}

// Queried once per editor frame by the loading-cover draw (M-GridEdit). The
// deadline ticks HERE (not in GGTerrainWicked_Update) so the cover can never
// stick if terrain updates stop for any reason.
bool GGTerrainWicked_IsRevealHeld()
{
	if (g_revealHoldFrames <= 0) return false;
	g_revealHoldFrames--;
	return g_revealHoldFrames > 0;
}

// Synchronously pre-build the chunks the camera can see, bounded by
// maxMilliseconds. Runs at the END of a level load with the loading screen
// still up: kicks the generator (turbo budget + high priority + the view-cone
// order from Wicked delta #7) and waits for the camera-facing chunk set, then
// runs the blendmap passes so the chunks are correctly coloured on the very
// first visible frame.
void GGTerrainWicked_Pregenerate(float camX, float camY, float camZ,
	float dirX, float dirY, float dirZ, int maxMilliseconds)
{
	if (!wickedTerrainInitialised) return;
	// GGMAX 2.58: pointless when entering the Terrain Generator — the 2.54 entry wipe
	// discards every pregenerated chunk on the first bridge frame anyway, and this pump
	// runs BEFORE the per-frame skip-bvh mirror so its ~600 cone chunks each paid the
	// 8.2 ms BVH build for nothing (measured as the bvh=2.95 leak in TERRAIN_GENPROF).
	// (bProceduralLevel resolves to the GLOBAL-scope extern above the namespace — a
	// block-scope extern here would mangle as GGTerrain:: and fail to link.)
	if (bProceduralLevel || g_ggTerrainGenEntryPending) return; // GGMAX 2.59: pending window too
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (!terrain) return;

	// Materials must exist before generation produces correctly-textured chunks
	// (normally set up lazily on the first GGTerrainWicked_Update after load).
	if (!wickedTerrainMaterialsSetup)
	{
		SetupWickedTerrainMaterials();
	}

	// The real wi camera isn't synced from GG until the first editor frame, so
	// build a local one from the restored level camera.
	wi::scene::CameraComponent cam = wi::scene::GetCamera();
	cam.Eye = XMFLOAT3(camX, camY, camZ);
	cam.At  = XMFLOAT3(dirX, dirY, dirZ);

	const int genSpan = 2 * terrain->generation + 1;
	const int expectedChunks = genSpan * genSpan;
	const int coneTarget = (expectedChunks * 2) / 5;  // matches the in-frame high-priority window

	terrain->generation_time_budget_milliseconds = 150.0f;
	terrain->generation_high_priority = true;

	wi::Timer timer;
	int pumpCounter = 0;
	while (timer.elapsed_milliseconds() < (double)maxMilliseconds &&
	       (int)terrain->chunks.size() < coneTarget)
	{
		terrain->Generation_Update(cam);
		wi::helper::Sleep(5);
		if ((++pumpCounter & 15) == 0 && ::bKeepWindowsResponding)
		{
			::EmptyMessages();
		}
	}

	// Colour everything we just generated before the user sees it. The passes
	// are batch-capped at 64 chunks — drain until caught up (bounded).
	if (wickedTerrainMaterialsSetup)
	{
		int guard = 32;
		while (!ApplyDX11StyleAutoBlend(terrain) && guard-- > 0) {}
		if (maxPaintedSlot >= 0)
		{
			guard = 32;
			while (!ProcessPaintedChunkBlendmaps(terrain) && guard-- > 0) {}
		}
	}

	char pregenLog[128];
	sprintf_s(pregenLog, "GGTerrainWicked_Pregenerate: %d chunks in %d ms",
		(int)terrain->chunks.size(), (int)timer.elapsed_milliseconds());
	wi::backlog::post(pregenLog);
	::timestampactivity(0, pregenLog);
}

void GGTerrainWicked_SetBrushCursor(bool visible, float x, float y, float z, float size)
{
	SetupBrushCursor();
	if (!g_brushCursorSetup) return;

	auto& scene = wi::scene::GetScene();
	auto* tx = scene.transforms.GetComponent(g_brushCursorEntity);
	auto* mat = scene.materials.GetComponent(g_brushCursorEntity);
	if (!tx || !mat)
	{
		// Component lookup failed — entity got wiped between SetupBrushCursor and here. Force the
		// next frame's SetupBrushCursor to rebuild from scratch instead of silently failing forever.
		g_brushCursorSetup = false;
		return;
	}

	if (visible && size > 0.0f)
	{
		tx->translation_local = XMFLOAT3(x, y, z);
		tx->scale_local = XMFLOAT3(size, size, size);
		tx->SetDirty();
		mat->baseColor.w = 1.0f;
	}
	else
	{
		mat->baseColor.w = 0.0f;
	}
	mat->SetDirty();
}

// GGMAX 3.73: did the last terrain teardown take the tree pool with it? Terrain Bake's
// teardown does not (the baked ground is still there, so the trees must be too), and Init
// must not then reset tree state that still describes a live pool. True initially so the
// process's first Init resets the state exactly as it always did.
static bool s_teardownTookTrees = true;

void GGTerrainWicked_Init()
{
	// Capture EXE directory for resolving texture paths (CWD-independent)
	wickedTerrainExeDir = wi::helper::GetDirectoryFromPath(wi::helper::GetExecutablePath());
	// Remove trailing slash if present
	if (!wickedTerrainExeDir.empty() && (wickedTerrainExeDir.back() == '/' || wickedTerrainExeDir.back() == '\\'))
		wickedTerrainExeDir.pop_back();

	auto& scene = wi::scene::GetScene();

	// GGMAX 3.113: the compositor's blend rule (see gg_terrain_splat_blend) - before any chunk renders.
	wi::terrain::gg_splat_blend = gg_terrain_splat_blend;

	// GGMAX 3.112: leaving a Terrain Bake - the grass emitters it left go, with their grass; the
	// real chunks this Init brings back regrow it (no-op on the startup Init and after Terrain Off).
	ReleaseBakeGrassEmitters();

	// Create entity and register terrain in scene (so renderer calls UpdateVirtualTexturesGPU)
	wickedTerrainEntity = wi::ecs::CreateEntity();
	wi::terrain::Terrain& terrain = scene.terrains.Create(wickedTerrainEntity);
	terrain.scene = &scene;
	terrain.terrainEntity = wickedTerrainEntity;

	terrain.SetCenterToCamEnabled(true);
	terrain.SetRemovalEnabled(true);
	terrain.SetGrassEnabled(false);       // Phase 0: no grass yet
	// GGMAX 2.22 (2026-08-11): kill the engine's per-chunk PROP scaffolding.
	//
	// wi::terrain creates one empty "props" child entity per chunk within prop_generation
	// (default 10 -> (2*10+1)^2 = 441 of them) purely to parent scattered props under
	// (wiTerrain.cpp:1420-1425: CreateEntity + transforms.Create + names.Create +
	// Component_Attach). GameGuru NEVER POPULATES terrain.props — grep for `.props` / Prop /
	// props.push_back across Guru-WickedMAX returns nothing — so all 441 are childless nodes
	// that draw no pixels and exist only to be walked.
	//
	// Cost they were imposing: 441 TransformComponents + 441 HierarchyComponents, revisited by
	// RunTransformUpdateSystem and RunHierarchyUpdateSystem EVERY FRAME. On Switch Escape that
	// is 441 of the 1995 hierarchy nodes (22%), inside the 1283-node chunk subtree that
	// SU-Hierarchy spends ~0.48 ms walking (SWITCHESCAPE_PERF.md §16).
	//
	// prop_density = 0 both blocks creation (the `prop_density > 0` gate at wiTerrain.cpp:1420)
	// and retro-deletes any already made (the prop_density_current mismatch branch at :936-939),
	// so it is correct whatever order init happens in. ZERO rendering change by construction:
	// no props exist to lose. ⚠ If GG ever starts using engine terrain props, remove this line.
	terrain.prop_density = 0.0f;
	terrain.SetPhysicsEnabled(false);      // keep Bullet physics from old terrain
	terrain.chunk_scale = 80.0f;          // one chunk spans (chunk_width-1)*scale = 66*80 = 5280 units (134m)
	// Chunk ring radius. (2N+1)^2 chunk entities, each a mesh+material+transform+hierarchy node
	// that Scene::Update walks every frame — 14 => 29x29 = 841, 12 => 25x25 = 625.
	//
	// ⚠ What decides how low this can go is NOT the frame cost — it is TERRAIN VIEW DISTANCE.
	// Radius = N * 5280 units AROUND THE CAMERA: SetCenterToCamEnabled(true) above means the
	// engine recentres the ring on camera.Eye every frame (wiTerrain.cpp:776-780), so the ring
	// travels with the player. N=14 reaches 73920u (1878m), N=12 reaches 63360u (1609m).
	// ⚠ Because the ring is camera-centred, the editable map size is IRRELEVANT here — a 5 km map
	// is not "too big" for the ring and nothing gets cropped. (Checked the hard way: an earlier
	// pass computed needGen from editable_size, called the two 5 km demos cropped, and was WRONG.
	// Flying to x=90000 on A Grand Canyon Adventure shows solid ground.) The ONLY criterion is
	// whether the shorter horizon is visible — settle that with the same-setting-control pixel
	// method in SWITCHESCAPE_PERF.md §22.2, never by arithmetic. Override: setup.ini terraingen=<N>.
	//
	// GGMAX 2.25 (2026-08-11): 14 -> 12. 841 -> 625 chunks = -216 of EVERY per-frame ECS counter,
	// measured Scene::Update 1.515 -> 1.128 ms (-25.5%) on Switch Escape, both arms one binary via
	// the terraingen key (SWITCHESCAPE_PERF.md §22).
	//
	// ★ 12 is the LAST SAFE STEP, not a round number. Verified against a same-setting control —
	// two independent gen-14 launches, because Island Showdown's palms/water/grass make any two
	// cold launches differ on their own (the first pixel-diff read 13.3% and meant nothing).
	// Against that floor gen 12 lands BELOW it on both test views (it differs from a gen-14 run by
	// LESS than a second gen-14 run does), while gen 10 is 1.4-2.0x the floor and gen 8 is 3.6x.
	// ⚠ So do NOT "round down a bit more" — 10 measurably shortens the visible horizon.
	//
	terrain.generation = (g_terrainGenOverride > 0) ? g_terrainGenOverride : 12;
	terrain.generation_view_cone_priority = true; // Wicked delta #7: build the chunks the camera faces first (see WICKED_ENGINE_CHANGES.md 1.7)
	// Wicked delta 1.14: stock Wicked tears down and rebuilds ALL chunks whenever a
	// terrain material is dirty (editor convenience). GG registers painted-material
	// slots at runtime — the freshly-created material is dirty for a frame, and
	// depending on frame order the restart fired = the 4-5s full-terrain flicker on
	// the first stroke with each new texture. GG owns the blendmaps; never restart.
	terrain.generation_restart_on_dirty_materials = false;
	// Wicked delta 1.15: in-place chunk regen (sculpt drag) keeps GG's blendmap layers,
	// the GPU blendmap texture AND the virtual-texture residency. Stock behavior rebuilt
	// engine-default region weights + reset the VT every regen = chunk-shaped blur /
	// wrong-texture flash for the whole duration of a sculpt drag. GG's blend passes
	// rewrite the weights right after regen anyway (bridge erases the processed keys).
	terrain.gg_preserve_blendmap_on_regen = true;
	// Wicked delta 1.17: chunks are born with GG-correct blendmaps — the generator thread
	// fills auto+painted weights before the region texture is built, so streamed-in chunks
	// (fast camera zooms re-create removed chunks) never flash the engine-default green
	// region blend while the main-thread passes catch up. The passes just latch the keys.
	terrain.gg_generate_blendmap = [](wi::terrain::ChunkData& cd, const wi::scene::MeshComponent& mesh)
	{
		return FillChunkBlendmapGG(cd, mesh);
	};
	// Wicked delta 1.18: fast camera zooms sweep the dist<2 high-res ring across the
	// island and every crossing chunk reset its VT residency mid-motion (square tiles of
	// mixed sharpness flickering until the camera stopped). With hysteresis, chunks keep
	// their correct low-res tile while the camera crosses boundaries; residency upgrades
	// run a few per frame once it holds still.
	terrain.gg_vt_upgrade_hysteresis = true;
	// Wicked delta 1.21: expand the VT working set so the island lives entirely inside
	// the full-resolution zone (stock ring was +/-2 chunks — crossed in milliseconds by
	// a fast camera, forcing cache re-inits; the residual violent-zoom squares). With
	// +/-6 the camera never crosses a resolution boundary over the island, and the
	// removal margin keeps chunks alive across zoom travel (no destroy/recreate churn).
	// (ring 6 = 169 residency chunks cost ~17 FPS on TESTPRO1; 4 = 49 chunks keeps 50+)
	terrain.gg_near_ring_dist = 4;
	terrain.gg_removal_margin = 12;
	terrain.lod_bias = 0.0f;              // hold higher mesh LOD one step further out (inch-scale world)
	terrain.bottomLevel = -20000.0f;       // match GG height range
	terrain.topLevel = 20000.0f;

	// Phase 1: Height modifier feeds GG's heightmap+fractal+sculpt+flat area data
	heightModifier = std::make_shared<GGHeightModifier>();
	heightModifier->bottomLevel = terrain.bottomLevel;
	heightModifier->topLevel = terrain.topLevel;
	terrain.modifiers.push_back(heightModifier);

	terrain.Generation_Restart();
	wickedTerrainInitialised = true;
	wickedTerrainMaterialsSetup = false;

	// Phase 5: Reset the cylinder-tree pool state. Actual setup is lazy on the
	// first WickedUpdate call so pAllTrees[] has been populated by the level load.
	// GGMAX 3.73: only if the matching teardown actually took the pool - see the note in
	// GGTerrainWicked_Shutdown. This is a state RESET, not a removal, so calling it over a
	// pool the bake left alive would orphan 400+ live tree objects. Starts true so the
	// one-time startup init (GameGuruMain.cpp) behaves exactly as before.
	if (s_teardownTookTrees)
	{
		GGTrees::GGTrees_WickedInit();
		s_teardownTookTrees = false;
	}
}

// GGMAX 2.54/2.55: full chunk wipe — Generation_Restart (frees chunk VTs, clears chunks,
// removes the chunk group entity, joins the async VT job per the 1.45 guard) PLUS the
// blendmap-tracking clears, without which recoloring skips "already processed" chunk keys.
// Used on Terrain Generator entry (fresh session at the wide ring) and exit (reclaim the
// wide ring — removal alone can never shrink a ring below generation+2+removal_margin).
static void GGResetTerrainChunks(wi::terrain::Terrain* terrain)
{
	terrain->Generation_Restart();
	processedChunkKeys.clear();
	chunkKeyToEntity.clear();
	dx11BlendProcessedKeys.clear();
	dx11BlendChunkKeyToEntity.clear();
}

// GGMAX 2.62: biome/params reaction. GGTerrain's CheckParams detects any
// ggterrain_global_params change (biome buttons reseed + reload the noise recipe) and resets
// the LEGACY chunk system + reshuffles the noise — but that system is DEAD CODE under wicked
// terrain, so the wicked ring kept its old geometry and the biome buttons looked dead (Lee's
// Desert repro: sel/seed/slopemat all changed, ring never regenerated). CheckParams now calls
// NotifyParamsChanged; the reaction below is GENERATOR-ONLY (editor/test game/level load keep
// their existing flows untouched), debounced so slider drags coalesce into one wipe, and
// swallowed right after an entry/exit wipe (the entry auto-rainforest click lands frames after
// the 2.54 wipe — its params are what that fill already reads; reacting again would double-fill).
static int      s_ggParamsNotifyCountdown = -1;      // <0 idle; else bridge frames until the ring wipe
static uint32_t s_ggFramesSinceRingWipe = 1000000;   // reset by entry/exit/params wipes
static bool     s_ggMaterialsNotifyDirty = false;    // GGMAX 2.63: render_params (material indices) changed too

// GGMAX 2.62 diagnostics (see header): chain counters read by harness TERRAINGEN_BIOME
uint32_t gg_dbg_checkparams_runs = 0;
uint32_t gg_dbg_checkparams_resets = 0;
uint32_t gg_dbg_params_notifies = 0;
uint32_t gg_dbg_params_wipes = 0;
uint32_t gg_dbg_material_notifies = 0; // GGMAX 2.63

void GGTerrainWicked_NotifyParamsChanged()
{
	// Re-armed on EVERY change: the wipe fires 20 quiet frames after the LAST one.
	s_ggParamsNotifyCountdown = 20;
	gg_dbg_params_notifies++;
}

void GGTerrainWicked_NotifyMaterialsChanged()
{
	// GGMAX 2.63: ggterrain_global_render_params changed — that struct carries the biome's
	// MATERIAL SET (baseLayerMaterial, layerMatIndex[0..3], slopeMatIndex into the shared
	// terraintextures/matN catalogue). Wicked materials resolve those indices ONCE in
	// SetupTerrainMaterial, so a biome click regenerated correct HEIGHTS (2.62) but kept the
	// old biome's textures. Shares the 2.62 debounce; on fire the consumption drops
	// wickedTerrainMaterialsSetup instead of a plain wipe — that path re-reads the indices,
	// reloads the DDS set, clears both blend-pass key sets and ends in Generation_Restart.
	s_ggParamsNotifyCountdown = 20;
	s_ggMaterialsNotifyDirty = true;
	gg_dbg_material_notifies++;
}

// GGMAX 3.112: the grass manager, lifted VERBATIM out of GGTerrainWicked_Update (the only edits:
// terrain may be NULL and the chunk stride falls back to the bake's). It used to sit below that
// function's gg_no_terrain early return, so a Terrain Bake stopped it dead - the 3.73 tree pool's
// exact shape. Live terrain calls it from the same place in the frame as before; a bake calls it
// from the gg_no_terrain branch, against the grass emitters (s_bakeGrassEmitters).
static void UpdateWickedGrassManager(wi::terrain::Terrain* terrain, const wi::scene::CameraComponent& camera, uint64_t chunkSig)
{
	// Grass: set up the material/template once, then grow grass on chunks from the painted grass
	// map. Runs after blendmap processing (generation already cancelled = safe to add entities).
	// GGMAX 2.94: "Grass Off" brutal off-switch. Edge-triggered teardown then a hard skip of
	// the whole grass manager. ForceGrassRebuild is the right primitive - it Entity_Remove's
	// every per-type and merged hair and clears the three tracking maps, which returns the
	// HairParticleSystem GPU buffers as well as stopping the per-strand simulate dispatch.
	// NOT GGGrass_RemoveAll(): that zeroes all 16 MB of pGrassMap in place, so the level's
	// PAINTED grass would be destroyed and the next save would write an empty .gra.
	// Skipping SetupWickedGrass as well means the grass DDS blades are never even loaded.
	{
		// Edge-TRIGGERED both ways. The latch must be declared outside the `if` and cleared on
		// the OFF->ON edge, or the switch is one-way: measured 2026-08-22, grass stayed gone
		// after SET_GRASSOFF 0 and every later reading in that ladder was silently taken on a
		// grass-free scene.
		static bool s_ggNoGrassApplied = false;
		if (gg_no_grass && !s_ggNoGrassApplied)
		{
			ForceGrassRebuild();          // remove what exists; never rebuilt while the flag holds
			// GGMAX 3.112: a bake's grass emitters exist only for the grass - back to CPU copies
			// (their video memory freed) until grass comes back (see s_bakeGrassEmitters).
			if (terrain == nullptr && !s_bakeGrassEmitters.empty())
			{
				DemoteBakeGrassEmitters();
				s_bakeGrassPromotePending = true;
			}
			s_ggNoGrassApplied = true;
		}
		else if (!gg_no_grass && s_ggNoGrassApplied)
		{
			// Coming back on: clear the tier records so ProcessGrassChunks re-visits every chunk
			// and regrows. ForceGrassRebuild already cleared the maps on the way out, but a chunk
			// created WHILE grass was off has a stale tier entry and would be skipped.
			grassChunkKeyToTier.clear();
			// ...and NUDGE the pass. ProcessGrassChunks is gated on (grassDirty || chunkSig
			// changed || settle retry || nudge || camera moved > 8 units). With a parked camera
			// on a settled level NONE of those fire, so without this the switch is one-way:
			// measured twice on 2026-08-22 before the nudge was added - grass never came back
			// and every later reading in the ladder was silently taken on a grass-free scene.
			g_grassPassNudge = true;
			s_ggNoGrassApplied = false;
		}
	}
	// GGMAX 3.112: while baked, bare chunks that now hold grass become emitters - after Grass Off
	// (every emitter was demoted), after a bake that landed under Grass Off, after Populate. STATE-
	// driven (pending flag + grass on), not on the edge, so no frame ordering can strand it. 24 a
	// frame, nearest first, so a whole grassed map comes back in a dozen frames, not one long one.
	if (terrain == nullptr && s_bakeGrassPromotePending && !gg_no_grass)
	{
		PromoteBareGrassChunks(camera.Eye, 24);
	}
	if (!wickedGrassSetup && !gg_no_grass)
	{
		SetupWickedGrass();
	}
	// Stage B.9: custom palette slot registered/cleared by the editor UI ("Add New Grass" /
	// "Delete Grass"). Rebuild the appearance templates + clear cached materials for custom
	// real_types so BuildGrassMaterial re-loads DDS on next lookup.
	//
	// Also blast grassChunkKeyToTier so ProcessGrassChunks re-visits every chunk on the next pass.
	// This fixes two related bugs:
	//   1. Delete + re-add with a DIFFERENT DDS on the same slot — existing hair entities cache a
	//      snapshot of the OLD MaterialComponent, which persists even after we clear the material-
	//      ready flag. Forcing a chunk-tier re-eval triggers the tier-change branch (fullReset =
	//      true) so entities get destroyed and recreated with the fresh material.
	//   2. Level reload where ProcessGrassChunks runs on frame N BEFORE the palette-sync (which is
	//      in the ImGui render at end-of-frame). Frame N tries to build the custom material with a
	//      null filename (sync not yet run), fails silently, but still records the chunk's tier —
	//      frame N+1's dirty poll rebuilds appearances but the chunks stay skipped because their
	//      tier hasn't changed. Clearing tier tracking forces the re-visit.
	bool grassDirty = false;
	if (GGGrass::GGGrass_TakeCustomSlotsDirty())
	{
		grassDirty = true;
		g_grassAppearanceReady = false;
		for (uint32_t t = GGGRASS_CUSTOM_REAL_TYPE_BASE; t < GGGRASS_TOTAL_REAL_TYPES; t++)
			g_grassMaterialReady[t] = false;
		BuildGrassAppearance();
		ApplyGrassDrawDistance(); // sync viewDistance onto the newly-built custom appearances

		// Invalidate all chunk tier tracking + delete any existing hair entities for CUSTOM
		// real_types across every tracked chunk. Two-step because Phase 2's existing-entity branch
		// short-circuits on "type still painted, entity exists" — so if we only cleared the tier
		// map, existing entities would survive and keep their OLD material snapshot even after the
		// slot's DDS changed. Removing the custom entities here forces Phase 2 to hit the CREATE
		// branch, which calls BuildGrassMaterial with the freshly-registered filename.
		//
		// Stock entities (real_type 0..45) are left alone — their DDS never changed.
		grassChunkKeyToTier.clear();
		{
			auto& scene = wi::scene::GetScene();
			for (auto& kv : grassChunkKeyToGrassEntities)
			{
				for (uint32_t t = GGGRASS_CUSTOM_REAL_TYPE_BASE; t < GGGRASS_TOTAL_REAL_TYPES; t++)
				{
					wi::ecs::Entity e = kv.second.perType[t];
					if (e != wi::ecs::INVALID_ENTITY)
					{
						if (scene.hairs.GetComponent(e))
							scene.Entity_Remove(e);
						kv.second.perType[t] = wi::ecs::INVALID_ENTITY;
					}
				}
			}
		}
	}

	// Grass Draw Distance slider → per-entity viewDistance. Detect slider movement and sync every
	// live grass entity in one pass so dragging the slider in the editor pulls every cull radius
	// along with it (rather than waiting for chunks to be rebuilt on camera move).
	// Compare the EFFECTIVE distance, not the raw slider: under the low-VRAM cap the slider can
	// move without the effective value changing, and re-applying then would be pure churn.
	// GGMAX 3.117: the far-field mode rides to the engine every frame; a change re-applies the per-type
	// view distances (flowers) and re-tiers every chunk (the full-density ring moves).
	{
		static int s_prevFarMode = -1;
		wi::gg_grass_dx11_falloff = gg_grass_far_mode;
		wi::gg_grass_dx11_fade_range = 2500.0f;   // DX11 GGGRASS_LOD_TRANSITION
		if (gg_grass_far_mode != s_prevFarMode)
		{
			if (s_prevFarMode >= 0) { g_grassPassNudge = true; g_grassTierShrinkPending = true; }
			s_prevFarMode = gg_grass_far_mode;
			ApplyGrassDrawDistance();
		}
	}
	if (GGGrass_LodDistEffective() != g_grassPrevSliderInches)
	{
		ApplyGrassDrawDistance();
	}

	auto rangeGM = wi::profiler::BeginRangeCPU("TerrainW - Grass Maint");
	// Grass Start/End Altitude sliders (+ underwater pair + water plane) → per-entity CB values
	// consumed by the hair simulate CS altitude filter. Unconditional every frame — 4 float
	// compares per entity is well under the noise floor next to the CB upload the shader path
	// already does, and skipping the sync when values look unchanged would miss water-plane
	// drift and edge cases where the entities were re-created since the last sync.
	ApplyGrassAltitude();

	// Grass Scale slider → per-entity `length`. Same rationale as the altitude sync: cheaper to
	// always push than to track a "did the slider move?" flag and get it wrong on re-creation.
	ApplyGrassScale();
	// GGMAX 2.94: && !gg_no_grass - ProcessGrassChunks is the ONLY caller of scene.hairs.Create
	// for grass, so this one condition is the difference between "grass exists" and "it never
	// did". ApplyGrassAltitude/ApplyGrassScale above are left unguarded on purpose: they walk
	// scene.hairs, which is EMPTY once the teardown above has run, so they cost nothing.
	if (wickedGrassEnabled && !gg_no_grass)
	{
		// Editor paint mutates pGrassMap via GGGrass_Update_Painting. The Wicked grass renderer
		// caches per-chunk hair entities and only rebuilds them on tier/entity changes — so it
		// can't notice a paint stroke on its own. GGGrass tracks the *set* of chunk keys the
		// brush footprint touched (not the bounding box of the whole stroke), so we only erase
		// tier records for chunks the user actually painted into. Chunks the cursor merely passed
		// over without writing keep their grass intact and don't reshuffle.
		float gridChunkStride = terrain ? (wi::terrain::chunk_width - 1) * terrain->chunk_scale : s_bakeGrassChunkStride;
		GGGrass::GGGrass_SetChunkStride(gridChunkStride);

		// GGMAX 3.116: the Grass Density LATCH. Inputs = density mode, percentage, raw draw distance and
		// the low-VRAM preset. Latched at once while no grass exists (level load: grow at the right
		// density first time); otherwise only after they hold still for GG_GRASS_DENSITY_SETTLE frames,
		// then the pass is nudged and the stale-density test above re-queues chunks.
		{
			float tNear = 0.0f, tMul = 1.0f;
			GrassDensityTarget(gridChunkStride, tNear, tMul);
			static float s_pendNear = -1.0f, s_pendMul = -1.0f;
			static int s_pendMode = -1;
			static int s_stable = 0;
			const int tMode = gg_grass_density_dx11 ? 1 : 0;
			if (tNear != g_grassNearStrands || tMul != g_grassLowVramMul || !g_grassDensityLatched
				|| tMode != (g_grassDensityDX11Latched ? 1 : 0))
			{
				bool anyGrass = false;
				for (const auto& kv : grassChunkKeyToGrassEntities) { if (kv.second.builtStrands > 0) { anyGrass = true; break; } }
				if (tNear == s_pendNear && tMul == s_pendMul && tMode == s_pendMode) s_stable++;
				else { s_pendNear = tNear; s_pendMul = tMul; s_pendMode = tMode; s_stable = 0; }
				if (!anyGrass || !g_grassDensityLatched || s_stable >= GG_GRASS_DENSITY_SETTLE)
				{
					// a density-MODE change moves the far-field ring too (GrassFarRingCollapsed): re-tier
					// without the downgrade hysteresis holding the old rings at a parked camera
					if (anyGrass && tMode != (g_grassDensityDX11Latched ? 1 : 0)) g_grassTierShrinkPending = true;
					g_grassNearStrands = tNear;
					g_grassLowVramMul = tMul;
					g_grassDensityDX11Latched = (tMode != 0);
					g_grassDensityLatched = true;
					g_dbgGrassDensityLatches++;
					s_stable = 0;
					if (anyGrass) g_grassPassNudge = true;
				}
			}
			else s_stable = 0;
		}
		static wi::vector<uint64_t> dirtyChunks; // reused across frames; cleared per drain
		dirtyChunks.clear();
		if (GGGrass::GGGrass_TakeDirtyChunks(dirtyChunks))
		{
			grassDirty = true;
			for (uint64_t key : dirtyChunks)
			{
				// Erase the tier record so ProcessGrassChunks sees a tier change and rebuilds it.
				grassChunkKeyToTier.erase(key);
				// GGMAX 3.112: baked, and painted onto a chunk that had no grass at bake time -
				// give it an emitter now (one CreateRenderData) so the stroke shows straight away.
				if (terrain == nullptr) PromoteBareGrassChunk(key);
			}
		}
		// SET_GRASS knob changes still flow through the full-rebuild path. Populate/Clear
		// Vegetation buttons (GGGrass_AddAll / GGGrass_RemoveAll) also raise this signal —
		// they rewrite pGrassMap in bulk, which the per-chunk paint-invalidation pipeline
		// never sees. Note TakeFullRebuildPending is take-and-clear, so we consume it every
		// frame regardless of g_grassRebuildRequested's state.
		bool bulkMapRewrite = GGGrass::GGGrass_TakeFullRebuildPending();
		// GGMAX 3.112: Populate Vegetation while baked can put grass on any bare chunk.
		if (bulkMapRewrite && terrain == nullptr) s_bakeGrassPromotePending = true;
		if (g_grassRebuildRequested || bulkMapRewrite)
		{
			ForceGrassRebuild();
			g_grassRebuildRequested = false;
			grassDirty = true;
		}
		// Gate the chunk pass: tiers are a pure function of camera distance + chunk set +
		// paint state. Skip unless one of those moved. Cache updates only when the pass
		// runs, so chunk churn or camera drift during a grass-disabled stretch still
		// triggers a pass on re-enable. SETTLE-GATE re-arm: while any chunk deferred its
		// grass regrowth (entity still churning), retry every 10th frame so deferred growth
		// can never be stranded by an unchanged signature.
		static uint64_t s_grassSig = ~0ull;
		static float s_grassCamX = 1e30f, s_grassCamZ = 1e30f;
		static uint32_t s_grassSettleTick = 0;
		const bool settleRetry = g_grassSettlePending && ((++s_grassSettleTick % 3) == 0);
		const float gdx = camera.Eye.x - s_grassCamX;
		const float gdz = camera.Eye.z - s_grassCamZ;
		if (grassDirty || s_grassSig != chunkSig || settleRetry || g_grassPassNudge || (gdx * gdx + gdz * gdz) > (8.0f * 8.0f))
		{
			if (g_grassPassNudge)
			{
				g_grassPassNudge = false; // consumed (SET_GRASSLOD toggles re-evaluate AUTO tiers now)
				g_grassTierShrinkPending = true; // boundaries may have pulled in — let downgrades through
			}
			ProcessGrassChunks(terrain, camera.Eye);
			s_grassSig = chunkSig;
			s_grassCamX = camera.Eye.x;
			s_grassCamZ = camera.Eye.z;
		}
	}
	wi::profiler::EndRange(rangeGM);
}

void GGTerrainWicked_Update(const wi::scene::CameraComponent& camera)
{
	// GGMAX 2.94: "Terrain Off" brutal off-switch, edge-triggered, placed before every other
	// early-out so it cannot be shadowed by one.
	//
	// This calls the REAL teardown, not GGTerrainWicked_SetTerrainVisible(false). That hide
	// only clears SetRenderable on the chunk ObjectComponents: the 625 chunks still exist, still
	// pay transform/hierarchy/AABB/matrix work in Scene::Update, still hold their meshes and
	// virtual-texture tiles and the whole ~576 MB SVT atlas, and - the killer - Generation_Update
	// KEEPS MAKING NEW ONES (the 2.68f note at the head of this file). Removing the Terrain
	// component instead takes terrains.GetCount() to 0, which additionally makes
	// wiRenderPath3D.cpp skip THREE whole per-frame command lists (CopyVirtualTexturePageStatus,
	// AllocateVirtualTextureTileRequests, WritebackTileRequests) and drops two cross-queue waits.
	//
	// Note the CPU heightmap (GGTerrain_Init) is deliberately left alive - ~91 BT_GetGroundHeight
	// call sites and the Bullet terrain shape read it. This switch removes the RENDERED terrain.
	{
		// GGMAX 2.94f fail-safe: every early return below happens BEFORE the idle gate is
		// recomputed, so clear the engine-side skip first. Leaving it set from a previous
		// frame would gate the engine's Generation_Update permanently and terrain would never
		// generate again.
		wi::terrain::gg_skip_generation_update = false;
		static bool s_noTerrainApplied = false;
		if (gg_no_terrain && !s_noTerrainApplied)
		{
			GGTerrainWicked_Shutdown();       // clears wickedTerrainInitialised
			s_noTerrainApplied = true;
			return;
		}
		if (!gg_no_terrain && s_noTerrainApplied)
		{
			// Toggling back ON: re-init from scratch. The tree pool comes back with it
			// (GGTerrainWicked_Init calls GGTrees_WickedInit), but painted grass and blendmaps
			// re-derive over the next few generation passes, so expect a visible rebuild.
			s_noTerrainApplied = false;
			GGTerrainWicked_Init();
		}
		if (gg_no_terrain)
		{
			// GGMAX 3.112: a Terrain Bake left grass emitters standing in for the chunks - keep the
			// grass manager running on them (LOD rings follow the camera, Grass Off, paint, sliders).
			// Plain Terrain Off (no bake) does not call it, as before.
			// (Called with no emitters too: bare chunks are promoted from inside it when grass is
			// painted, Populated or switched back on.)
			if (::gg_terrain_bake)
				UpdateWickedGrassManager(nullptr, camera, (uint64_t)s_bakeGrassEmitters.size());
			return;
		}
	}
	if (!wickedTerrainInitialised) return;
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (!terrain) return;

	// U key: toggle wireframe overlay for terrain chunk visualization
	extern bool GGTerrain_GetKeyPressed(uint8_t key);
	static int wickedWireframeMode = 0;
	if (GGTerrain_GetKeyPressed(0x55)) // GGKEY_U
	{
		wickedWireframeMode = 1 - wickedWireframeMode;
		if (wickedWireframeMode)
			wi::renderer::SetWireframeMode(wi::renderer::WIREFRAME_OVERLAY);
		else
			wi::renderer::SetWireframeMode(wi::renderer::WIREFRAME_DISABLED);
	}

	// I key: toggle normal visualization
	static int wickedNormalVisMode = 0;
	if (GGTerrain_GetKeyPressed(0x49)) // GGKEY_I
	{
		wickedNormalVisMode = 1 - wickedNormalVisMode;
		wi::renderer::SetDebugNormalVis(wickedNormalVisMode != 0); // VS could not find this!!
	}

	// O key: toggle terrain rendering on/off (flag is file-scope, shared with the View Options setter)
	if (GGTerrain_GetKeyPressed(0x4F)) // GGKEY_O
	{
		wickedTerrainHidden = !wickedTerrainHidden;
		if (wickedTerrainHidden)
		{
			// Hide all existing chunks and stop VT GPU work
			for (auto& [chunk, chunk_data] : terrain->chunks)
			{
				if (chunk_data.entity != wi::ecs::INVALID_ENTITY)
				{
					wi::scene::ObjectComponent* obj = terrain->scene->objects.GetComponent(chunk_data.entity);
					if (obj) obj->SetRenderable(false);
				}
			}
			terrain->virtual_textures_in_use.clear();
		}
		else
		{
			// Re-show all chunks — Generation_Update will resume next frame
			for (auto& [chunk, chunk_data] : terrain->chunks)
			{
				if (chunk_data.entity != wi::ecs::INVALID_ENTITY)
				{
					wi::scene::ObjectComponent* obj = terrain->scene->objects.GetComponent(chunk_data.entity);
					if (obj) obj->SetRenderable(true);
				}
			}
		}
	}

	// P key: toggle profiler and dump to file when disabling
	static bool perfProfilerEnabled = false;
	if (GGTerrain_GetKeyPressed(0x50)) // VK_P
	{
		perfProfilerEnabled = !perfProfilerEnabled;
		wi::profiler::SetEnabled(perfProfilerEnabled);
		if (perfProfilerEnabled)
		{
			wi::backlog::post("[Perf] Profiler ENABLED — press P again to dump and disable");
		}
		else
		{
			std::string profText = GGPerf_GetCachedProfilerText();
			if (!profText.empty())
			{
				std::string dumpPath = wickedTerrainExeDir + "/terrain_perf.log";
				FILE* f = nullptr;
				fopen_s(&f, dumpPath.c_str(), "w");
				if (f) { fputs(profText.c_str(), f); fclose(f); }
				wi::backlog::post("[Perf] Profiler DISABLED — dumped to terrain_perf.log");
			}
			else
			{
				wi::backlog::post("[Perf] Profiler DISABLED — no data to dump");
			}
		}
	}

	// 1 key: toggle shadows
	static int perfShadowsOff = 0;
	if (GGTerrain_GetKeyPressed(0x31)) // VK_1
	{
		perfShadowsOff = 1 - perfShadowsOff;
		wi::RenderPath3D* rp = GGPerf_GetRenderPath();
		if (rp) rp->setShadowsEnabled(!perfShadowsOff);
		wi::backlog::post(perfShadowsOff ? "[Perf] Shadows OFF" : "[Perf] Shadows ON");
	}

	// 2 key: toggle AO
	static int perfAOOff = 0;
	if (GGTerrain_GetKeyPressed(0x32)) // VK_2
	{
		perfAOOff = 1 - perfAOOff;
		wi::RenderPath3D* rp = GGPerf_GetRenderPath();
		if (rp) rp->setAO(perfAOOff ? wi::RenderPath3D::AO_DISABLED : wi::RenderPath3D::AO_MSAO);
		wi::backlog::post(perfAOOff ? "[Perf] AO OFF" : "[Perf] AO ON (MSAO)");
	}

	// 3 key: toggle bloom
	static int perfBloomOff = 0;
	if (GGTerrain_GetKeyPressed(0x33)) // VK_3
	{
		perfBloomOff = 1 - perfBloomOff;
		wi::RenderPath3D* rp = GGPerf_GetRenderPath();
		if (rp) rp->setBloomEnabled(!perfBloomOff);
		wi::backlog::post(perfBloomOff ? "[Perf] Bloom OFF" : "[Perf] Bloom ON");
	}

	// 4 key: toggle volumetric clouds
	static int perfCloudsOff = 0;
	if (GGTerrain_GetKeyPressed(0x34)) // VK_4
	{
		perfCloudsOff = 1 - perfCloudsOff;
		auto& weather = GGPerf_GetWeather();
		weather.SetVolumetricClouds(!perfCloudsOff);
		wi::backlog::post(perfCloudsOff ? "[Perf] Volumetric Clouds OFF" : "[Perf] Volumetric Clouds ON");
	}

	// 5 key: toggle realistic sky
	static int perfSkyOff = 0;
	if (GGTerrain_GetKeyPressed(0x35)) // VK_5
	{
		perfSkyOff = 1 - perfSkyOff;
		auto& weather = GGPerf_GetWeather();
		weather.SetRealisticSky(!perfSkyOff);
		wi::backlog::post(perfSkyOff ? "[Perf] Realistic Sky OFF" : "[Perf] Realistic Sky ON");
	}

	// 6 key: toggle light shafts
	static int perfLightShaftsOff = 0;
	if (GGTerrain_GetKeyPressed(0x36)) // VK_6
	{
		perfLightShaftsOff = 1 - perfLightShaftsOff;
		wi::RenderPath3D* rp = GGPerf_GetRenderPath();
		if (rp) rp->setLightShaftsEnabled(!perfLightShaftsOff);
		wi::backlog::post(perfLightShaftsOff ? "[Perf] Light Shafts OFF" : "[Perf] Light Shafts ON");
	}

	// 7 key: toggle reflections
	static int perfReflectionsOff = 0;
	if (GGTerrain_GetKeyPressed(0x37)) // VK_7
	{
		perfReflectionsOff = 1 - perfReflectionsOff;
		wi::RenderPath3D* rp = GGPerf_GetRenderPath();
		if (rp) rp->setReflectionsEnabled(!perfReflectionsOff);
		wi::backlog::post(perfReflectionsOff ? "[Perf] Reflections OFF" : "[Perf] Reflections ON");
	}

	// 8 key: toggle volumetric lights on sun
	static int perfVolLightsOff = 0;
	if (GGTerrain_GetKeyPressed(0x38)) // VK_8
	{
		perfVolLightsOff = 1 - perfVolLightsOff;
		wi::scene::LightComponent* lightSun = GGPerf_GetSunLight();
		if (lightSun) lightSun->SetVolumetricsEnabled(!perfVolLightsOff);
		wi::backlog::post(perfVolLightsOff ? "[Perf] Sun Volumetrics OFF" : "[Perf] Sun Volumetrics ON");
	}

	// 9 key: toggle global occlusion culling (A/B experiment; tree pool objects are
	// individually exempt from queries regardless — Wicked delta #6)
	static int perfOcclusionOff = 0;
	if (GGTerrain_GetKeyPressed(0x39)) // VK_9
	{
		perfOcclusionOff = 1 - perfOcclusionOff;
		wi::renderer::SetOcclusionCullingEnabled(!perfOcclusionOff);
		wi::backlog::post(perfOcclusionOff ? "[Perf] Occlusion Culling OFF" : "[Perf] Occlusion Culling ON");
	}

	// G key: toggle grass on/off (A/B + perf). Hides existing grass via layerMask and pauses new
	// grass creation; re-enabling shows it again and resumes creation for any new chunks.
	if (GGTerrain_GetKeyPressed(0x47)) // GGKEY_G
	{
		wickedGrassEnabled = !wickedGrassEnabled;
		auto& gscene = wi::scene::GetScene();
		for (auto& kv : grassChunkKeyToGrassEntities)
		{
			for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
			{
				wi::ecs::Entity e = kv.second.perType[t];
				if (e == wi::ecs::INVALID_ENTITY) continue;
				wi::HairParticleSystem* hair = gscene.hairs.GetComponent(e);
				if (hair) hair->layerMask = wickedGrassEnabled ? ~0u : 0u;
			}
			// GGMAX 1.74: merged entity honours the same toggle
			if (kv.second.merged != wi::ecs::INVALID_ENTITY)
			{
				wi::HairParticleSystem* hair = gscene.hairs.GetComponent(kv.second.merged);
				if (hair) hair->layerMask = wickedGrassEnabled ? ~0u : 0u;
			}
		}
		wi::backlog::post(wickedGrassEnabled ? "[Grass] ON" : "[Grass] OFF");
	}

	// UI AUDIT 2026-07-28 (v2): map boundary via engine debug lines (same visual family as
	// the object-bounds boxes). The View Options boundary checkboxes flip the legacy
	// SHOW_MAP_SIZE flags that only the DEAD custom terrain shaders consumed — draw them
	// for real here. Editable area spans -half..+half around world origin (see the
	// GGTerrain_GetEditableSize grid math in GGTerrain_part0).
	{
		const uint32_t bflags = ggterrain_global_render_params2.flags2;
		const bool bShow3D = (bflags & GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE_3D) != 0;
		const bool bShow2D = (bflags & GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE) != 0;
		if (bShow3D || bShow2D)
		{
			const float half = GGTerrain_GetEditableSize(); // returns HALF-size
			// GGMAX 2.65: in the Terrain Generator the editable area follows the MARKER
			// (releasing a drag no longer recentres the world), and the marker's position
			// is exactly the generation-center override the generator feeds every frame —
			// draw the boundary there. Everywhere else the override is auto-cleared and
			// the box stays on world origin (the editor's real map bounds).
			float fBoundsCX = 0.0f, fBoundsCZ = 0.0f;
			if (wi::terrain::gg_generation_center_override_enabled)
			{
				fBoundsCX = wi::terrain::gg_generation_center_override_x;
				fBoundsCZ = wi::terrain::gg_generation_center_override_z;
			}
			const XMFLOAT4 boundaryColor = XMFLOAT4(1.0f, 0.75f, 0.1f, 1.0f);
			if (bShow3D)
			{
				// tall wire box marking the editable volume (height is visual-only)
				wi::primitive::AABB box(XMFLOAT3(fBoundsCX - half, 0.0f, fBoundsCZ - half), XMFLOAT3(fBoundsCX + half, 25000.0f, fBoundsCZ + half));
				wi::renderer::DrawBox(box, boundaryColor, true);
			}
			if (bShow2D)
			{
				// ground-hugging boundary ring: sample terrain height along each edge
				const int segs = 64;
				const float step = (half * 2.0f) / (float)segs;
				const float lift = 20.0f; // keep the line just above the surface
				const float xW = fBoundsCX - half, xE = fBoundsCX + half;
				const float zN = fBoundsCZ - half, zS = fBoundsCZ + half;
				auto ground = [&](float x, float z) -> float
				{
					float h = 0.0f;
					GGTerrain_GetHeight(x, z, &h);
					return h + lift;
				};
				for (int i = 0; i < segs; i++)
				{
					const float ax = xW + step * i, bx = ax + step;
					const float az = zN + step * i, bz = az + step;
					wi::renderer::RenderableLine line;
					line.color_start = line.color_end = boundaryColor;
					// north + south edges
					line.start = XMFLOAT3(ax, ground(ax, zN), zN);
					line.end = XMFLOAT3(bx, ground(bx, zN), zN);
					wi::renderer::DrawLine(line, true);
					line.start = XMFLOAT3(ax, ground(ax, zS), zS);
					line.end = XMFLOAT3(bx, ground(bx, zS), zS);
					wi::renderer::DrawLine(line, true);
					// west + east edges
					line.start = XMFLOAT3(xW, ground(xW, az), az);
					line.end = XMFLOAT3(xW, ground(xW, bz), bz);
					wi::renderer::DrawLine(line, true);
					line.start = XMFLOAT3(xE, ground(xE, az), az);
					line.end = XMFLOAT3(xE, ground(xE, bz), bz);
					wi::renderer::DrawLine(line, true);
				}
			}
		}
	}

	// Skip all terrain work when hidden (Generation_Update, VT CPU/GPU, blendmap painting)
	// GGMAX 2.68g: Completely Empty Level mode is polled DIRECTLY here — the old wiring
	// relied on Wicked_Update_Visuals running after the level's visuals were parsed, which
	// the fpm load path does not guarantee (Lee's ssss9: flag loaded true, collision gone,
	// grid still rendered because wickedTerrainHidden was never set on that path).
	const bool bEmptyModeHide = GGGame_IsEmptyLevelMode();
	if (wickedTerrainHidden || bEmptyModeHide)
	{
		// GGMAX 2.68f (Lee's repro: a freshly LOADED Completely Empty level showed the
		// grid again): the hide sweep in SetTerrainVisible only reaches chunks that exist
		// at that moment — a level load rebuilds the ring AFTERWARDS (Scene::Update calls
		// the engine's Generation_Update directly, independent of this early-out), and
		// newborn chunks arrive renderable. Re-assert the hide on anything that slipped
		// in; ~1500 flag checks per frame, only while terrain is hidden.
		if (bEmptyModeHide && !wickedTerrainHidden) s_ggEmptyModeSwept = true;
		::wi::terrain::Terrain* terrainHidden = GetWickedTerrain();
		if (terrainHidden != nullptr && terrainHidden->scene != nullptr)
		{
			for (auto& [chunkH, cdH] : terrainHidden->chunks)
			{
				if (cdH.entity == wi::ecs::INVALID_ENTITY) continue;
				wi::scene::ObjectComponent* objH = terrainHidden->scene->objects.GetComponent(cdH.entity);
				if (objH != nullptr && objH->IsRenderable()) objH->SetRenderable(false);
			}
		}
		return;
	}
	// GGMAX 2.68g: empty mode just ended while the UI hide is off — SetTerrainVisible(true)
	// early-outs in that state (wickedTerrainHidden never flipped), so re-show what the
	// empty-mode sweep hid. One-shot.
	if (s_ggEmptyModeSwept)
	{
		s_ggEmptyModeSwept = false;
		::wi::terrain::Terrain* terrainShow = GetWickedTerrain();
		if (terrainShow != nullptr && terrainShow->scene != nullptr)
		{
			for (auto& [chunkS, cdS] : terrainShow->chunks)
			{
				if (cdS.entity == wi::ecs::INVALID_ENTITY) continue;
				wi::scene::ObjectComponent* objS = terrainShow->scene->objects.GetComponent(cdS.entity);
				if (objS != nullptr && !objS->IsRenderable()) objS->SetRenderable(true);
			}
		}
	}

	// Phase 2: Lazy material setup on first update (after level load has set render params)
	if (!wickedTerrainMaterialsSetup)
	{
		SetupWickedTerrainMaterials();
	}

	// Initial-build turbo (2026-07-18): the stock generator thread exits after
	// generation_time_budget_milliseconds = 8ms per launch, and it checks the
	// budget after EVERY chunk — with our chunk cost that's ~1 chunk per frame,
	// so a full build (generation=14 -> 841 chunks) took ~30+ seconds of
	// visible radial pop-in after level load. While the chunk set is far from
	// complete (level load / Generation_Restart), give the generator a fat
	// per-frame budget; normal editing churn (camera movement removes/regens
	// at most a ring or two, ~100 chunks with removal enabled) stays on the
	// stock budget so flying has no generation hitches.
	// NOTE: steady-state chunk count is governed by removal_threshold, NOT the
	// generation ring total — on TESTPRO1 it settles ~650-700 of the 841 ring
	// chunks. 60% of the ring total (~504) is comfortably below steady state
	// (flying churn never sheds that many) and comfortably above "just
	// restarted", so the turbo runs exactly during the initial build.
	// ⚠ MEASURED 2026-08-11 (GGMAX 2.25, TERRAIN_RING in GET_PERF_DATA): with the camera
	// PARKED after a level load, Island Showdown and Switch Escape both settle at the FULL
	// 841 = ringMax, not 650-700. The under-count above only happens once the camera has
	// travelled (removal lags creation by removal_threshold = generation+2+12 rings, so a
	// moved camera can also read ABOVE ringMax — 898 measured after one SET_CAMERA jump).
	// Both directions leave the 60% initial-build test correct; do not retune it off the
	// TESTPRO1 figure alone.
	const int genSpan = 2 * terrain->generation + 1;
	const int expectedChunks = genSpan * genSpan;
	const int coneTarget = (expectedChunks * 2) / 5;  // cone + near rings ≈ 40% of ring total
	const bool initialBuild = (int)terrain->chunks.size() < (expectedChunks * 6) / 10;
	const bool heightsReady = GGTerrain_IsReady() != 0;

	// Level reveal hold: drop the loading cover the moment the legacy heights
	// are ready AND the camera-facing chunk set exists. (The deadline backstop
	// ticks in GGTerrainWicked_IsRevealHeld.)
	if (g_revealHoldFrames > 0 && heightsReady && (int)terrain->chunks.size() >= coneTarget)
	{
		g_revealHoldFrames = 0;
	}
	const bool revealHeld = g_revealHoldFrames > 0;

	// Mass in-place regen (terrain parameter change, sculpt reset, full undo restore):
	// chunks.size() never drops so the initial-build throttles don't engage — detect it
	// from last frame's pending-regen census (counted in the chunkSig loop below) and
	// apply the same medicine: fatter generation budget + high-priority pool + blend
	// passes only every 30th frame (their Generation_Cancel chops the generator).
	// Small brush edits (a few chunks) stay on the responsive 8ms path.
	const bool massRegen = s_pendingRegenCount > 32;

	// While the cover is up nobody sees the frame rate — let the generator eat
	// most of the frame. Otherwise: turbo during the visible initial build,
	// stock 8ms in normal editing.
	// GGMAX 2.57: the Terrain Generator (bProceduralLevel) trades FPS for fill speed by
	// the user's explicit request — 20 ms budget takes the 70-80 FPS preview to ~30 while
	// chunks are outstanding and costs NOTHING once the ring is full (the budget is only
	// consumed when generation work exists, so FPS snaps back on completion). massRegen's
	// 50 ms still wins when a box-drop mass-invalidates inside the generator.
	terrain->generation_time_budget_milliseconds = revealHeld ? 300.0f : (initialBuild ? 150.0f : (massRegen ? 50.0f : (bProceduralLevel ? 20.0f : 8.0f)));
	// Wicked delta #8: while the CAMERA-FACING cone is still building (~40% of
	// the ring total covers the cone + near rings), run generation on the HIGH
	// job pool — the Low pool is THREAD_PRIORITY_LOWEST and gets starved by the
	// busy CPU during level load, which was most of the visible build time.
	// Once the cone is done, drop back to polite Low so the off-camera fill
	// doesn't steal frame time from the editor (measured 40-55 -> 19-43 FPS
	// during the tail when left on High).
	// GGMAX 2.57: high-priority for the WHOLE generator fill too — the polite-Low tail
	// rationale protects the editor's frame rate, which the generator has agreed to spend.
	terrain->generation_high_priority = revealHeld || massRegen || bProceduralLevel || (int)terrain->chunks.size() < coneTarget;
	static uint32_t s_terrainFrame = 0;
	s_terrainFrame++;
	if (s_ggFramesSinceRingWipe < 0xFFFFFFFEu) s_ggFramesSinceRingWipe++; // GGMAX 2.62
	// While the turbo build runs, only interrupt the generator with blendmap
	// processing every 30th frame — each ApplyDX11StyleAutoBlend/painted pass
	// calls Generation_Cancel, and doing that per-frame while chunks stream in
	// was chopping the generator off after 1-2 chunks per launch.
	// POST-LOAD DIP FIX 2026-07-29: the same problem recurred at full rate once
	// initialBuild ended (60% of ring) — chunkSig churns every frame until the
	// ring completes, so both scans ran EVERY frame (profiled 4.7ms + 1.0ms on
	// Canyon Adventure = the "FPS halves for ~10s after load" dip), and each
	// pass's Generation_Cancel kept chopping the generator, prolonging the very
	// churn that re-fired the scans. Pace them to every g_blendScanInterval-th
	// frame (default 4): worst case +3 frames of paint-stroke latency, and the
	// build tail drains FASTER because the generator is cancelled 4x less.
	const int blendEvery = (initialBuild || massRegen) ? 30 : (g_blendScanInterval > 1 ? g_blendScanInterval : 1);
	const bool blendTickAllowed = (s_terrainFrame % blendEvery) == 0;

	// GGMAX terrain idle gate: quiescence detection. Calm = camera parked AND no build /
	// reveal / regen / merge activity AND no edit ping AND the chunk-set signature has
	// been stable (checked in the census loop below, which still runs every frame).
	// Conservative by construction: any signal resets calm to 0 the same frame, and the
	// every-8th-frame heartbeat bounds a missed signal's delay to ~110ms of ring scan.
	// GGMAX 2.53: the generation-center override may only live while the Terrain Generator
	// owns the screen — any other mode reverts to camera-centred generation here, so no
	// exit path from the generator can leak a pinned ring into the editor or test game.
	// A state flip jumps the ring centre, so it also counts as activity for the idle gate.
	{
		static bool s_lastGenOverride = false;
		if (!bProceduralLevel && wi::terrain::gg_generation_center_override_enabled)
			wi::terrain::gg_generation_center_override_enabled = false;
		// GGMAX 2.58: the generator skips the 8.2 ms/chunk pick-BVH build (its drag rays use
		// the brute-force fallback); every other mode builds BVHs as always. Mirrored per
		// frame, and the 2.55 exit wipe regenerates everything WITH BVHs for the editor.
		wi::terrain::gg_generation_skip_bvh = bProceduralLevel;
		// GGMAX 2.94: || gg_no_grass - kills the per-vertex perlin sample (4489/chunk) and the
		// per-chunk CreateFromMesh on the generator thread, which SetGrassEnabled(false) does NOT
		// reach. Without this the generator still pays for grass the scene will never show.
		wi::terrain::gg_generation_skip_grass = bProceduralLevel || gg_no_grass; // GGMAX 2.60: no grass in the generator (invisible at those distances + known bug there)
		if (wi::terrain::gg_generation_center_override_enabled != s_lastGenOverride)
		{
			s_lastGenOverride = wi::terrain::gg_generation_center_override_enabled;
			s_terrainActivityPing = true;
		}

		// GGMAX 2.54: entering the Terrain Generator = a NEW session — wipe every chunk.
		// Leftovers were structural, not transient: a previous session's box-drags change
		// the noise offset, but height-change invalidation marks only reach chunks under
		// the GG editable area — ring chunks OUTSIDE it keep the old-offset geometry
		// forever (the torn rim the user screenshotted). Generation_Restart nukes all
		// chunks + their VTs + the chunk group entity unconditionally (and already joins
		// the async VT job — the 1.45 use-after-free guard), then the blendmap tracking
		// must restart too or recoloring skips "already processed" chunk keys. Watching
		// the bProceduralLevel transition HERE catches every entry route (storyboard
		// empty node, File > New Level) with one site.
		//
		// GGMAX 2.55: the generator also gets a WIDER chunk ring — gen 19 (39x39 = 1521
		// chunks, reach 2548 m) covers the full 5 km editable-area view; the shipping
		// gen 12 (625) only reaches 1609 m and left the 5 km box half empty. Generator
		// ONLY: measured cost is +258 MB driver VRAM / ~-12% preview FPS, fine for the
		// generator's empty scene but not wanted in the editor or test game. The exit
		// restore needs its own Restart because a ring can never SHRINK on its own —
		// removal only reclaims past generation+2+removal_margin(12), so 1521 stale
		// chunks would ride into the editor otherwise. Both exit routes are safe: back
		// arrow lands in the storyboard (no 3D view, ring rebuilds unseen), Generate
		// lands in a real level load whose reveal hold covers the rebuild.
		// GGMAX 2.59: end of the entry-pending window = the generator has arrived. Also an
		// auto-heal: if ANY missed route leaves the flag up, force-clear after ~30s so the
		// editor can never sit generation-suppressed (that failure would be silent + severe).
		static uint32_t s_pendingFrames = 0;
		if (g_ggTerrainGenEntryPending)
		{
			if (++s_pendingFrames > 1800)
			{
				g_ggTerrainGenEntryPending = false;
				wi::backlog::post("GGTerrainWicked: gen-entry pending flag auto-healed after 1800 frames (a clear route was missed)");
			}
		}
		else s_pendingFrames = 0;

		static bool s_lastProceduralLevel = false;
		static int s_savedGeneration = 0;
		if (bProceduralLevel && !s_lastProceduralLevel)
		{
			g_ggTerrainGenEntryPending = false; // GGMAX 2.59: handover complete
			s_savedGeneration = terrain->generation;
			terrain->generation = 19; // 2.55: set BEFORE the restart so the rebuild targets 1521 immediately
			GGResetTerrainChunks(terrain);
			s_ggFramesSinceRingWipe = 0; // GGMAX 2.62: swallow the entry auto-click's params notify
			s_terrainActivityPing = true;
		}
		if (!bProceduralLevel && s_lastProceduralLevel)
		{
			if (s_savedGeneration > 0) terrain->generation = s_savedGeneration; // 2.55: back to the shipping ring
			GGResetTerrainChunks(terrain);
			s_ggFramesSinceRingWipe = 0; // GGMAX 2.62
			s_terrainActivityPing = true;
		}
		s_lastProceduralLevel = bProceduralLevel;

		// GGMAX 2.62: consume a params-change notify (see NotifyParamsChanged above the
		// function). Generator only; anywhere else the notify is discarded — editor, test
		// game and level load already own their terrain-rebuild flows and must not gain a
		// second wipe path. Within 60 frames of an entry/exit wipe the notify is the entry
		// auto-click's own params landing — that fill already reads them, so discard too.
		if (s_ggParamsNotifyCountdown >= 0)
		{
			if (!bProceduralLevel || s_ggFramesSinceRingWipe < 60)
			{
				s_ggParamsNotifyCountdown = -1;
				s_ggMaterialsNotifyDirty = false;
			}
			else if (--s_ggParamsNotifyCountdown < 0)
			{
				if (s_ggMaterialsNotifyDirty)
				{
					// GGMAX 2.63: the biome's MATERIAL SET changed too — full material
					// re-setup instead of a plain wipe. That path re-resolves the matN
					// indices, reloads the DDS set, clears both blend-pass key sets and
					// ends in its own Generation_Restart (a separate reset here would
					// double-fill the ring).
					// GGMAX 2.63b: call it SYNCHRONOUSLY — merely dropping the flag left
					// a one-frame hole: the setup check runs EARLIER in this function
					// than this consumption, so this frame's generation kick (below)
					// still ran on the OLD material snapshot and the first cone chunks
					// baked their one-shot VT tiles from stale material data (Lee's
					// snow-with-sand-centre screenshot — same population as the level
					// load "burst through" cone, which never hits this because setup
					// always precedes the first kick there).
					s_ggMaterialsNotifyDirty = false;
					wickedTerrainMaterialsSetup = false;
					SetupWickedTerrainMaterials();
				}
				else
				{
					GGResetTerrainChunks(terrain);
				}
				s_ggFramesSinceRingWipe = 0;
				s_terrainActivityPing = true;
				gg_dbg_params_wipes++;
				wi::backlog::post("GGTerrainWicked: generator params changed (biome/slider) - terrain regeneration triggered");
			}
		}
	}
	{
		static XMFLOAT3 s_idleLastEye = {}, s_idleLastAt = {};
		const bool cameraMoved =
			fabsf(camera.Eye.x - s_idleLastEye.x) > 0.25f ||
			fabsf(camera.Eye.y - s_idleLastEye.y) > 0.25f ||
			fabsf(camera.Eye.z - s_idleLastEye.z) > 0.25f ||
			fabsf(camera.At.x - s_idleLastAt.x) > 0.001f ||
			fabsf(camera.At.y - s_idleLastAt.y) > 0.001f ||
			fabsf(camera.At.z - s_idleLastAt.z) > 0.001f;
		s_idleLastEye = camera.Eye;
		s_idleLastAt = camera.At;
		const bool active = cameraMoved || s_terrainActivityPing || initialBuild || revealHeld
			|| massRegen || !heightsReady || !wickedTerrainMaterialsSetup
			|| s_pendingRegenCount > 0 || g_dbgMergePendingCensus > 0;
		s_terrainActivityPing = false;
		if (active) g_dbgIdleCalmFrames = 0;
		else if (g_dbgIdleCalmFrames < 0xFFFFFFFEu) g_dbgIdleCalmFrames++;
	}
	// GGMAX 2.59: while a Terrain Generator entry is pending, generate NOTHING - the whole
	// ring would be wiped by the 2.54 entry restart moments later (measured: ~875 throwaway
	// chunks per entry, each paying the full bake including the 8.2ms BVH).
	const bool idleSkipGen = g_ggTerrainGenEntryPending || (g_terrainIdleGate && g_dbgIdleCalmFrames > 45 && (s_terrainFrame & 7) != 0);
	// GGMAX 2.94f: publish the SAME predicate to the engine, which calls Generation_Update a
	// second time on this frame from Scene::Update. This bridge runs BEFORE __super::Update
	// (master_part1.cpp), so the value is fresh for that call. Until 2.94f the engine caller
	// was ungated and walked all 625 chunks every frame on a parked, settled scene while this
	// one skipped 7 frames in 8 - i.e. the gate was only ever half-applied.
	wi::terrain::gg_skip_generation_update = idleSkipGen && gg_engine_gen_gate;

	// Let the VT system run — generates chunks, creates atlas, blends materials.
	// CORRECTNESS GATE (2026-07-18): during the initial build, do NOT generate
	// until the legacy GG terrain reports its heights ready — chunks generated
	// from not-yet-read-back heights bake permanently wrong geometry (the
	// reverted load-time pregeneration bug). Normal editing (initialBuild
	// false) is unaffected.
	if (!initialBuild || heightsReady)
	{
		if (!idleSkipGen)
		{
			auto rangeGen = wi::profiler::BeginRangeCPU("TerrainW - Generation_Update");
			terrain->Generation_Update(camera);
			wi::profiler::EndRange(rangeGen);
		}
		else
		{
			g_dbgIdleGateSkips++;
		}
	}

	// Cheap change signature over the live chunk set (a few hundred entries, vs the 21K-object
	// scene scans it gates below). Captures chunk create/remove/regen (entity value changes)
	// AND blendmap arrival on freshly-generated chunks (layers.size() 0 -> N), which is what
	// lets a gated scan retry chunks it had to skip mid-generation. External invalidations
	// (paint brush, level load, material setup) clear the processed-key maps instead — each
	// gate below also compares its map size against the size it recorded after its last run.
	// NOTE: each gate keeps its OWN sig cache and only updates it when its work actually runs,
	// so a gate that was disabled (e.g. materials not set up yet) still fires once re-enabled.
	uint64_t chunkSig = (uint64_t)terrain->chunks.size();
	size_t pendingRegenCensus = 0;
	size_t mergePendingCensus = 0;
	for (const auto& [sigChunk, sigCd] : terrain->chunks)
	{
		if (sigCd.invalidated) pendingRegenCensus++;
		if (sigCd.merge_pending) mergePendingCensus++;
		chunkSig = chunkSig * 1099511628211ull
			+ (uint64_t)sigCd.entity * 31ull
			+ (uint64_t)sigCd.blendmap_layers.size()
			// sculpt invalidation flips invalidated true, regen swaps it for
			// merge_pending, the merge clears that — every transition must refire the
			// blend gates (the passes skip chunks while either flag is up, so the
			// final clear is what lets them finish the job on the MERGED fresh mesh)
			+ (sigCd.invalidated ? 0x9E3779B9ull : 0ull)
			+ (sigCd.merge_pending ? 0x85EBCA6Bull : 0ull);
	}
	s_pendingRegenCount = pendingRegenCensus; // consumed by next frame's massRegen throttle
	g_dbgInvalidatedCensus = pendingRegenCensus;
	g_dbgMergePendingCensus = mergePendingCensus;

	// GGMAX idle gate: chunk-set churn (create/remove/regen/blendmap arrival) = activity.
	{
		static uint64_t s_idleLastChunkSig = ~0ull;
		if (chunkSig != s_idleLastChunkSig)
		{
			g_dbgIdleCalmFrames = 0;
			s_idleLastChunkSig = chunkSig;
		}
	}

	// Path A: rewrite the auto material weights (slots 0-4) with DX11-shape blend, then let
	// the painted-material pass overlay its own weights on painted vertices. Must run AFTER
	// Generation_Update so blendmap layers exist, and BEFORE ProcessPaintedChunkBlendmaps so
	// painting still wins on painted cells.
	if (wickedTerrainMaterialsSetup)
	{
		static uint64_t s_autoSig = ~0ull;
		static size_t   s_autoCount = (size_t)-1;
		if (blendTickAllowed && (s_autoSig != chunkSig || s_autoCount != dx11BlendProcessedKeys.size()))
		{
			auto rangeAB = wi::profiler::BeginRangeCPU("TerrainW - AutoBlend Scan");
			bool caughtUp = ApplyDX11StyleAutoBlend(terrain);
			wi::profiler::EndRange(rangeAB);
			if (caughtUp)
			{
				// Only latch when fully processed — a capped (sliced) pass leaves
				// the cache stale so the gate refires until the backlog drains.
				s_autoSig = chunkSig;
				s_autoCount = dx11BlendProcessedKeys.size();
			}
		}
	}

	// Phase 3: Process terrain chunks for painted material blendmaps AFTER Generation_Update.
	// Must run after so the generation pipeline has finished creating default blendmaps for
	// new/regenerated chunks. Painting before generation completes gets overwritten by the
	// height/slope blending stage. Our VT invalidation is picked up on the next frame's
	// Generation_Update (1-frame delay, but avoids permanent corruption from race condition).
	if (wickedTerrainMaterialsSetup && maxPaintedSlot >= 0)
	{
		static uint64_t s_paintSig = ~0ull;
		static size_t   s_paintCount = (size_t)-1;
		if (blendTickAllowed && (s_paintSig != chunkSig || s_paintCount != processedChunkKeys.size()))
		{
			auto rangePB = wi::profiler::BeginRangeCPU("TerrainW - PaintedBlend Scan");
			bool caughtUp = ProcessPaintedChunkBlendmaps(terrain);
			wi::profiler::EndRange(rangePB);
			if (caughtUp)
			{
				s_paintSig = chunkSig;
				s_paintCount = processedChunkKeys.size();
			}
		}
	}

	// GGMAX 3.112: the grass manager is its own function now, so a Terrain Bake can keep calling it
	// after the terrain is gone (see the gg_no_terrain branch at the top of this function).
	UpdateWickedGrassManager(terrain, camera, chunkSig);

	// GGMAX 3.73: the tree pool update MOVED OUT to the caller (master_part1.cpp, same
	// position in the frame and same profiler range). It was nested in here behind a
	// comment claiming it was "independent of terrain chunk lifecycle" - and it was
	// independent, except that this function returns early the moment gg_no_terrain is
	// set, so Terrain Bake stopped calling it and the near trees could never regrow.
}

void GGTerrainWicked_Shutdown()
{
	if (!wickedTerrainInitialised) return;
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (terrain)
	{
		terrain->Generation_Cancel();
		// GGMAX 2.94: Generation_Cancel only waits on generator->workload. The ASYNC VT JOB is a
		// separate context holding raw VirtualTexture pointers - freeing VTs underneath it is the
		// use-after-free documented in the GGMAX 1.45 comment in Terrain::Generation_Restart
		// (travel-churn / level-switch corruption, AMD DEVICE_HUNG). Generation_Restart joins it;
		// this teardown never did - until 2.94 it had ZERO callers and had never been executed.
		wi::terrain::gg_WaitVirtualTextureJob();
		// ★ GGMAX 3.112: a Terrain Bake hands the grassy chunks' meshes to grass emitters and moves
		// the grass onto them FIRST - the removal below is recursive and used to take every grass
		// hair with it. Plain Terrain Off (no baked ground to stand on) still drops the grass.
		if (::gg_terrain_bake) AdoptGrassEmittersForBake(terrain);
		// Remove all chunk entities from the scene
		for (auto& [chunk, chunk_data] : terrain->chunks)
		{
			if (chunk_data.entity != wi::ecs::INVALID_ENTITY)
			{
				terrain->scene->Entity_Remove(chunk_data.entity);
			}
		}
		terrain->chunks.clear();
		terrain->modifiers.clear();
	}
	heightModifier.reset();
	if (wickedTerrainEntity != wi::ecs::INVALID_ENTITY)
	{
		wi::scene::GetScene().Entity_Remove(wickedTerrainEntity);
		wickedTerrainEntity = wi::ecs::INVALID_ENTITY;
	}
	wickedTerrainInitialised = false;
	wickedTerrainMaterialsSetup = false;
	maxPaintedSlot = -1;
	processedChunkKeys.clear();
	chunkKeyToEntity.clear();
	// GGMAX 2.94: the GRASS tracking maps point at hair entities Component_Attach'ed to the chunks
	// just removed. Entity_Remove is recursive so the hairs are already gone, but the maps would
	// still hand dangling entity ids to ProcessGrassChunks on a later re-init.
	// GGMAX 3.112: ...unless a bake just moved the grass onto its emitters - then the maps point at
	// LIVE hairs on live emitters and are exactly what the grass manager needs to carry on.
	if (s_bakeGrassEmitters.empty())
	{
		grassChunkKeyToGrassEntities.clear();
		grassChunkKeyToTier.clear();
		grassChunkKeyToChunkEntity.clear();
	}

	// ★ GGMAX 3.73: ONLY when the world really is going away.
	//
	// Terrain Bake reuses this teardown to drop the Wicked terrain, but it leaves a baked
	// copy of the ground standing in its place - the world is still there to walk on and
	// look at. Taking the tree pool down with it deleted every near 3D tree in the level
	// (Lee, testpro2level: SCENE_OBJECTS 1410 -> 104, POOL built 443 -> 0) while the far
	// billboards carried on drawing, because those run off the CPU instance arrays the
	// teardown deliberately keeps alive.
	//
	// ⚠ Init and Shutdown MUST agree about this. GGTrees_WickedInit only FORGETS the pool
	// entities (it clears g_treePoolEntities without removing them), so an unpaired re-init
	// over a live pool orphans every one of them. s_teardownTookTrees is that pairing.
	s_teardownTookTrees = !::gg_terrain_bake;
	if (s_teardownTookTrees) GGTrees::GGTrees_WickedShutdown();
}

void GGTerrainWicked_InvalidateRegion(float minX, float minZ, float maxX, float maxZ, uint32_t flags)
{
	// Phase 6 bridge: called from GGTerrain_InvalidateRegion when sculpt/paint/undo
	// modifies the CPU height-edit or material maps. Marks the overlapping chunks for
	// in-place mesh regeneration (heights) and/or blendmap re-processing (textures).
	if (!wickedTerrainInitialised) return;
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (terrain == nullptr) return;
	// nothing built yet (e.g. the load-time InvalidateEverything calls fire before
	// generation starts) — skip so we don't Generation_Cancel the initial build
	if (terrain->chunks.empty()) return;

	// interactive painting can introduce a material that has no blendmap slot yet.
	// Gated on PAINT mode: sculpt/undo also carry the TEXTURES flag, and with a
	// slot-less palette selection merely SELECTED (never painted into the map) a
	// full re-setup could never assign it a slot — ungated this became a
	// Generation_Restart livelock on every sculpt stroke frame. The slot itself is
	// registered INCREMENTALLY below (after the cancel) — the full re-setup +
	// Generation_Restart used previously tore down and rebuilt the whole island on
	// the first stroke with each new texture (the user-visible blur/glitch).
	int newPaintSlotMat = -1;
	if ((flags & GGTERRAIN_INVALIDATE_TEXTURES) && ggterrain_extra_params.edit_mode == GGTERRAIN_EDIT_PAINT)
	{
		int paintMat = ggterrain_extra_params.paint_material & 0xff;
		if (paintMat > 0 && paintMat <= GGTERRAIN_MAX_SOURCE_TEXTURES && materialToSlot[paintMat - 1] < 0)
			newPaintSlotMat = paintMat - 1;
	}

	// the generator thread mutates the chunks map — stop it before iterating
	terrain->Generation_Cancel();
	g_dbgBridgeCalls++;
	s_terrainActivityPing = true; // GGMAX idle gate: edits restore full-rate Generation_Update

	if (newPaintSlotMat >= 0)
		RegisterPaintedMaterialSlot(newPaintSlotMat);

	const bool heightsChanged = (flags & GGTERRAIN_INVALIDATE_CHUNKS) != 0;
	// tight chunk-AABB overlap, padded by one heightmap cell so a brush touching a
	// seam also regenerates the neighbour that shares those border vertices. (The
	// chunk bounding SPHERE is the wrong test here — its half-diagonal radius turns
	// a 100-unit brush dab into a 3x3 chunk regen, 9x the churn and heal time.)
	const float chunkStrideW = (float)(wi::terrain::chunk_width - 1) * terrain->chunk_scale;
	const float chunkHalf = chunkStrideW * 0.5f;
	const float seamPad = terrain->chunk_scale;
	size_t marked = 0;
	for (auto& [chunk, cd] : terrain->chunks)
	{
		if (cd.entity == wi::ecs::INVALID_ENTITY) continue;
		const float chunkCX = (float)chunk.x * chunkStrideW;
		const float chunkCZ = (float)chunk.z * chunkStrideW;
		if (chunkCX + chunkHalf + seamPad < minX || chunkCX - chunkHalf - seamPad > maxX) continue;
		if (chunkCZ + chunkHalf + seamPad < minZ || chunkCZ - chunkHalf - seamPad > maxZ) continue;

		// Generation_Update regenerates invalidated chunks in place (entity reused)
		if (heightsChanged) { cd.invalidated = true; marked++; }

		// forget the blend work on this chunk so both blendmap passes re-run once the
		// chunk settles; the passes skip chunks still flagged invalidated, and the
		// invalidated bit is part of chunkSig, so the gates refire when regen completes
		uint64_t key = MakeChunkKey(chunk.x, chunk.z);
		g_dbgBridgeChunksMarked += heightsChanged ? 1 : 0;
		g_dbgBridgeKeysErased += processedChunkKeys.erase(key);
		chunkKeyToEntity.erase(key);
		g_dbgBridgeKeysErased += dx11BlendProcessedKeys.erase(key);
		dx11BlendChunkKeyToEntity.erase(key);
		cd.gg_blendmap_generated = false; // real edit: the passes must reprocess (delta 1.17)
	}
	// prime the mass-regen throttle so the first frame after a big invalidation
	// already runs the generator on the turbo budget
	if (marked > s_pendingRegenCount) s_pendingRegenCount = marked;
}

void GGTerrainWicked_OnTextureSetChanged()
{
	// Change Texture Folder / ReloadTextures: every material's DDS content changed on
	// disk — the incremental path can't help here, do the full re-setup + restart so
	// SetupTerrainMaterial re-loads everything from the new set.
	if (!wickedTerrainInitialised) return;
	wickedTerrainMaterialsSetup = false;
	s_terrainActivityPing = true; // GGMAX idle gate
}

// UI AUDIT 2026-07-28: real visibility levers for the View Options checkboxes. The legacy
// gggrass draw_enabled / ggterrain_draw_enabled flags only gate the DEAD custom draw path —
// shipping grass is Wicked hair entities and shipping terrain is Wicked chunk objects — so
// these sweep the live entities (identical mechanisms to the G-key / O-key debug toggles).
void GGTerrainWicked_SetGrassVisible(bool visible)
{
	if (wickedGrassEnabled == visible) return;
	wickedGrassEnabled = visible;
	auto& gscene = wi::scene::GetScene();
	for (auto& kv : grassChunkKeyToGrassEntities)
	{
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES; t++)
		{
			wi::ecs::Entity e = kv.second.perType[t];
			if (e == wi::ecs::INVALID_ENTITY) continue;
			wi::HairParticleSystem* hair = gscene.hairs.GetComponent(e);
			if (hair) hair->layerMask = visible ? ~0u : 0u;
		}
		// GGMAX 1.74: merged entity honours the same toggle
		if (kv.second.merged != wi::ecs::INVALID_ENTITY)
		{
			wi::HairParticleSystem* hair = gscene.hairs.GetComponent(kv.second.merged);
			if (hair) hair->layerMask = visible ? ~0u : 0u;
		}
	}
}

// GGMAX 2.67 diagnostic: the wicked grass state in numbers, for the harness. Splits
// "creation never ran" (hairs=0) from "hidden" (vis<hairs) from "invisible for another
// reason" (vis=hairs but nothing on screen: altitude filter, scale, draw distance).
void GGTerrainWicked_GetGrassDebug(int* pWickedEnabled, int* pHairCount, int* pVisibleCount, unsigned long long* pStrandSum)
{
	*pWickedEnabled = wickedGrassEnabled ? 1 : 0;
	*pHairCount = 0; *pVisibleCount = 0; *pStrandSum = 0;
	auto& gscene = wi::scene::GetScene();
	for (auto& kv : grassChunkKeyToGrassEntities)
	{
		for (uint32_t t = 0; t < GGGRASS_TOTAL_REAL_TYPES + 1; t++)
		{
			wi::ecs::Entity e = (t < GGGRASS_TOTAL_REAL_TYPES) ? kv.second.perType[t] : kv.second.merged;
			if (e == wi::ecs::INVALID_ENTITY) continue;
			wi::HairParticleSystem* hair = gscene.hairs.GetComponent(e);
			if (!hair) continue;
			(*pHairCount)++;
			if (hair->layerMask != 0) (*pVisibleCount)++;
			*pStrandSum += hair->strandCount;
		}
	}
}

// GGMAX 2.68i: the sweep as a callable — the main loop SKIPS GGTerrainWicked_Update
// entirely while bEnableEmptyLevelMode is true (master_part1 gates the whole terrain
// block), so the in-update sweep never ran in exactly the mode that needs it. The
// engine-side Generation_Update (Scene::Update) still births renderable chunks there.
// Sets the swept flag so the re-show path fires when empty mode ends.
void GGTerrainWicked_EnforceHidden()
{
	s_ggEmptyModeSwept = true;
	::wi::terrain::Terrain* tr = GetWickedTerrain();
	if (tr == nullptr || tr->scene == nullptr) return;
	for (auto& [chunkE, cdE] : tr->chunks)
	{
		if (cdE.entity == wi::ecs::INVALID_ENTITY) continue;
		wi::scene::ObjectComponent* objE = tr->scene->objects.GetComponent(cdE.entity);
		if (objE != nullptr && objE->IsRenderable()) objE->SetRenderable(false);
	}
}

// GGMAX 2.68h accessors: the ssss10 storyboard-switch hunt needs the hidden state visible
// to the harness, and the generator panel gates the Completely Empty tick on ring
// completion (ticking mid-fill leaves a half-and-half state, Lee's report).
bool GGTerrainWicked_IsTerrainHidden()
{
	return wickedTerrainHidden;
}
bool GGTerrainWicked_IsRingComplete()
{
	::wi::terrain::Terrain* tr = GetWickedTerrain();
	if (tr == nullptr) return false;
	const int ringMax = (2 * tr->generation + 1) * (2 * tr->generation + 1);
	return (int)tr->chunks.size() >= ringMax;
}

void GGTerrainWicked_SetTerrainVisible(bool visible)
{
	if (wickedTerrainHidden == !visible) return;
	wickedTerrainHidden = !visible;
	::wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (terrain == nullptr || terrain->scene == nullptr) return;
	for (auto& [chunk, chunk_data] : terrain->chunks)
	{
		if (chunk_data.entity != wi::ecs::INVALID_ENTITY)
		{
			wi::scene::ObjectComponent* obj = terrain->scene->objects.GetComponent(chunk_data.entity);
			if (obj) obj->SetRenderable(visible);
		}
	}
	if (!visible) terrain->virtual_textures_in_use.clear();
}

// GGMAX 1.53: live re-tune of the terrain VT tiling share-mips (see wiTerrain.cpp knob).
// Sets the engine knob, then queues the proven-safe fast repaint (1.13 latch) on every
// resident chunk VT so the change is visible within a frame or two at the camera;
// non-resident chunks pick the new policy up as their tiles stream in naturally.
void GGTerrainWicked_SetTileShare(int k, int hold)
{
	::wi::terrain::gg_terrain_tile_share_mips = (uint32_t)std::max(0, k);
	if (hold >= 0) ::wi::terrain::gg_terrain_tile_hold_mips = (uint32_t)hold; // -1 = keep current
	s_terrainActivityPing = true; // GGMAX idle gate — repaint work incoming
	::wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (!terrain) return;
	for (auto& [chunk, cd] : terrain->chunks)
	{
		if (cd.vt && cd.vt->residency != nullptr && cd.vt->resolution != 0)
			cd.vt->pending_repaint_blendmap = true;
	}
}

// GGMAX 3.19: re-bake every RESIDENT virtual-texture tile from the material textures as they
// stand right now. Used when Texture Detail re-creates the terrain DDS files underneath a
// terrain that has already baked them into tiles - without this the ground is the only surface
// in the level that ignores the setting. Same 1.13 fast-repaint latch SET_TILESHARE uses, so it
// keeps residency and costs a frame or two instead of a full Generation_Restart.
static void GGTerrainWicked_RepaintAllResidentVT()
{
	::gg_texture_divide_vtrepaints = 0;
	if (!wickedTerrainInitialised) return;
	s_terrainActivityPing = true; // idle gate - repaint work incoming
	::wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (!terrain) return;
	for (auto& [chunk, cd] : terrain->chunks)
	{
		if (cd.vt && cd.vt->residency != nullptr && cd.vt->resolution != 0)
		{
			cd.vt->pending_repaint_blendmap = true;
			::gg_texture_divide_vtrepaints++;
		}
	}
}

void GGTerrainWicked_OnPaintDataChanged()
{
	// Called when pMaterialMap is updated (level load or paint brush).
	if (!wickedTerrainInitialised) return;
	s_terrainActivityPing = true; // GGMAX idle gate

	// Always clear so chunks get repainted with fresh pixel data
	processedChunkKeys.clear();
	chunkKeyToEntity.clear();

	// If materials aren't set up yet, nothing more to do (pending first setup)
	if (!wickedTerrainMaterialsSetup) return;

	// Check if paint data contains materials not yet in our slot mapping
	const uint8_t* matMap = GGTerrain::GGTerrain_GetMaterialMapPtr();
	int mapRes = GGTerrain::GGTerrain_GetMaterialMapResolution();
	if (!matMap || mapRes <= 0) return;

	bool needsNewMaterials = false;
	for (int i = 0; i < mapRes * mapRes && !needsNewMaterials; i++)
	{
		uint8_t val = matMap[i];
		if (val > 0 && val <= GGTERRAIN_MAX_SOURCE_TEXTURES)
			if (materialToSlot[val - 1] < 0)
				needsNewMaterials = true;
	}

	if (needsNewMaterials)
		wickedTerrainMaterialsSetup = false; // Will trigger full re-setup + restart
}

} // namespace GGTerrain

// GGMAX 3.113: the setup.ini key. FPSC_LoadSETUPINI runs AFTER the startup GGTerrainWicked_Init has
// copied the flag into the engine (review 3.113), so the key sets both; the first level load's
// material setup ends in Generation_Restart, so every chunk renders with it.
void GGTerrainWicked_SetSplatBlendFlag(bool on)
{
	gg_terrain_splat_blend = on;
	wi::terrain::gg_splat_blend = on;
}

// GGMAX 3.113: flip the compositor's blend rule live (harness A/B). The blendmaps do not change, only
// how they are composited - but the chunks must re-render, and that goes through the full chunk wipe
// the Terrain Generator uses (GGResetTerrainChunks: Generation_Restart joins the async VT job, then the
// ring regenerates born-correct). ⚠ Invalidating every chunk's virtual texture in one go instead
// crashed the engine's tile writeback (WritebackTileRequestsGPU -> CopyResource on a freed resource).
// A bake holding the terrain down keeps its old textures until it is re-baked.
void GGTerrainWicked_SetSplatBlend(bool on)
{
	gg_terrain_splat_blend = on;
	wi::terrain::gg_splat_blend = on;
	wi::terrain::Terrain* terrain = GetWickedTerrain();
	if (terrain == nullptr) return;
	GGTerrain::GGResetTerrainChunks(terrain);
}
