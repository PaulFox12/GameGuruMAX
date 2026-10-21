// GGTrees_part2.cpp — Phase 5 Stage 3: Trunks + alpha-tested leaves on the
// new Wicked Engine terrain. Each per-type MeshComponent now carries TWO
// subsets: subset[0] = trunk (opaque PBR bark texture),
// subset[1] = branches (alpha-tested leaf texture, double-sided). One shared
// ObjectComponent per pool slot draws both.
//
// Grown from `pAllTrees[]` (positions/type/scale set by the DX11-era GGTrees
// data pipeline). Real LOD variants, wind sway, and distance billboards come
// in later stages.
//
// Vertex format (both trunk and branch meshes):
// `VertexTreeHigh` = pos (3xfloat) + packed uint32 normal (R8G8B8A8_UNORM)
// + UV (2xfloat). DX11 input layout at GGTrees_part0.cpp:1231 confirms it.
//
// Included by GGTrees.cpp unity build; lives inside `namespace GGTrees` so
// pAllTrees / numTotalTrees / InstanceTree / g_GGTrees resolve without
// qualification.

// GGMAX 2.94: the "Trees Off" effective flag lives in the GLOBAL namespace
// (GGTerrainWicked.cpp). Declared HERE, outside `namespace GGTrees`, on purpose - a
// block-scope `extern bool gg_no_trees;` written inside the namespace declares
// GGTrees::gg_no_trees instead and fails to link (LNK2001, the 2.53 linkage rule).
extern bool gg_no_trees;

// GGMAX 2.95: draw the merged billboard proxy chunks to the MAIN CAMERA, not just into the far
// shadow cascades, so distant hillsides keep their forest. DX11 renders LOD0 + billboards past
// lod_dist; the DX12 port dropped the impostor path on 2026-07-13 (Wicked's RenderImpostors
// ignores IsNotVisibleInReflections and splattered white quads into the water reflection) and
// far trees have simply not rendered since. Lee's DX11-vs-DX12 comparison on spotshadowtest is
// what surfaced it - the DX11 mountains are forested, the DX12 ones bare.
//
// The geometry for this ALREADY SHIPS: GGTrees_BuildShadowProxyChunk builds, per 16x16 tree
// chunk, one merged mesh of two crossed quads per tree textured with the same billboard DDS
// DX11 uses. It was withheld from the player by a single SetNotVisibleInMainCamera(true).
// Default ON; harness SET_FARTREES 0|1 to A/B.
// DEFAULT OFF: the gate is NOT WORKING YET. With tree shadows on this level reports
// validProxies=244 and chunks out to 125,516 units against a 24,812 cutoff, yet proxiesShown
// stays 0 - the show condition never fires and I have not explained why. Shipping it off so
// the diagnostic is available without changing anyone's picture. Harness SET_FARTREES 1.
bool gg_trees_far_billboards = false;

namespace GGTrees
{

// LOD1/LOD2 mesh tables (g_GGTreesLOD1/g_GGTreesLOD2, 38 entries each).
// Included inside namespace GGTrees to match the LOD0 include in part0.
#include "TreeMeshes/TreeMeshesLOD1.h"
#include "TreeMeshes/TreeMeshesLOD2.h"

// Pool caps visible-tree ObjectComponents in the scene. Wicked's per-entity
// ECS overhead is O(N) per frame regardless of what each object rasterizes
// as, so we can't just crank this to numTotalTrees (400K) without tanking
// FPS (400K measured 2.6 FPS; 50K workable; 10K comfortable on the DX11
// A/B baseline — settled 2026-07-13).
//
// To ensure the pool is actually spent on the trees the player can see, the
// update loop does a nearest-N-to-camera pick every frame instead of the
// first-N-in-array-order (which used to leave the visible area sparse while
// filling the pool with distant trees). See GGTrees_WickedUpdate.
// Tree pool: renders the nearest-N placed tree instances as real ECS ObjectComponents. EACH pool
// entity costs per-frame ECS (object/transform update) whether drawn or parked, so the pool size is
// the dominant editor CPU lever (TESTPRO1 default cam: 20000->2000 pool = 54->76 FPS). GG_TREE_POOL_MAX
// is the fixed array capacity; g_treePoolSize is the runtime EFFECTIVE count read at pool setup — lower
// draws fewer/nearer trees but frees a lot of CPU. Change via harness SET_TREES pool <N> (applies on the
// next level load / pool rebuild). This is a work-reducing quality knob (fewer simultaneous trees).
static constexpr uint32_t GG_TREE_POOL_MAX = 20000;
uint32_t g_treePoolSize = 6000; // effective count = the CEILING; slots are created on demand

// GGMAX 2.19 (2026-08-10): LAZY POOL GROWTH.
// g_treePoolSize is now only the ceiling. g_treePoolBuilt is how many slots actually exist as
// ECS entities; it starts at 0 and grows on demand, capped per frame, and never shrinks while
// the pool lives.
//
// WHY. Every pool slot is an ObjectComponent + TransformComponent that Scene::Update walks
// EVERY FRAME whether or not it is bound to a visible tree. Setup used to create all 6000 up
// front, at app startup, before any level was known — so a level with no terrain and no trees
// still paid for 6000 objects and 6000 transforms. Measured on Switch Escape: 6000 of its 7322
// objects and 6000 of its 8437 transforms were parked pool slots, and forcing the pool to 1
// (setup.ini treepool=1) took a further ~0.13 ms off Scene::Update with SU-Mesh 0.41->0.17,
// SU-Object 0.57->0.38 and SU-Hierarchy 0.65->0.47. DX11 pays none of this: it creates ZERO ECS
// entities for trees and draws them with DrawIndexedInstanced from its own instance buffers.
// See SWITCHESCAPE_PERF.md sections 10.3(1) and 11.3.
//
// Growth is driven by poolFill — the nearest-N count the selection pass actually wants this
// frame — so a treeless level stays at 0 slots forever and a tree level converges to the same
// pool it always had. The per-frame cap keeps the one-off creation of thousands of entities
// from landing as a single hitch; it spreads over a few frames during level load instead.
static uint32_t g_treePoolBuilt = 0;
static constexpr uint32_t GG_TREE_POOL_GROW_PER_FRAME = 2048;
// GGMAX 2.23: consecutive parked frames before the pool is released (see the park branch in
// GGTrees_WickedUpdate). Must comfortably exceed a terrain-regen park, which is brief, while
// still catching a level change, where the park is permanent. Frame-based rather than
// time-based deliberately — it is the number of Scene::Update passes we are avoiding, and at
// any frame rate 600 of them is far longer than a regen and far shorter than a session.
static constexpr uint32_t GG_TREE_POOL_PARK_RELEASE_FRAMES = 600;
// Hardcoded here to keep this file free of the HLSL-flavoured numTreeTypes
// constant from GGTreesConstants.hlsli. Matches the g_GGTrees[38] array length.
static constexpr uint32_t GG_TREE_TYPES      = 38;

// Mesh LOD distance bands (world inches, 1" = 1 unit). Beyond LOD1_DIST a tree
// uses the LOD1 mesh, beyond LOD2_DIST the LOD2 mesh (e.g. birch 5156 -> 3448
// -> 2061 tris). The LOD1/LOD2 tables have existed as assets since the DX11
// era but were never referenced by ANY engine path (DX11 renders LOD0 +
// billboards past lod_dist) — this is their first use, so watch the A/B for
// authoring defects. Compared as squared distances against the nearest-N dist2.
static constexpr float GG_TREE_LOD1_DIST = 2500.0f;  // ~63 m
static constexpr float GG_TREE_LOD2_DIST = 7000.0f;  // ~178 m

// GGMAX 2.97: POOL RADIUS CAP. The real-mesh pool must stop where the billboards take over,
// or the two overlap. DX11's arrangement, which we now match:
//   billboard PS discards inside            tree_lodDist                     (GGTreesPS.hlsl:81)
//   mesh dissolves out over  lodDist+500 .. lodDist+1000                     (DX11 GGTreesHighPS)
// so the mesh is entirely gone by lod_dist + 2*GGTREES_LOD_TRANSITION.
//
// Before this cap the pool reached a measured 24,812 units (~630 m) while billboards began at
// lod_dist = 3000 (~76 m) - a 550 m band with BOTH drawn. Capping also shrinks the pool
// dramatically: at DX11 distances it holds a few hundred trees rather than 6000, which is a
// large CPU saving in Scene::Update on top of the visual fix.
//
// 0 disables the cap (pre-2.97 behaviour). Harness SET_TREEPOOLCAP <units>.
static constexpr float GG_TREE_LOD_TRANSITION = 500.0f;   // mirrors GGTREES_LOD_TRANSITION
float gg_tree_pool_max_dist = -1.0f;   // <0 = derive from lod_dist; 0 = uncapped; >0 = explicit
static float gg_ftCapDist2 = 0.0f;   // squared cap, refreshed each gather

// GGMAX 3.03: MESH FADE-OUT ACROSS THE HANDOVER BAND.
// The cap above stops the mesh pool, but it stops it with a BINARY SetRenderable(false) - the
// tree is 100% there one frame and gone the next. DX11 never pops: its mesh pixel shader carries
//     limit = noise*500 + 500 + lod_dist;  if ( sqrDist > limit*limit ) discard;
// (DX11 GGTreesHighPS.hlsl:215) so the mesh DISSOLVES out over lod_dist+500 .. lod_dist+1000,
// which is exactly the band the billboard has already dissolved IN over lod_dist .. lod_dist+500.
// One band of honest double-draw, then a cross-dissolve. We had the first half and not the second.
//
// We cannot port that discard - our near trees are stock Wicked ObjectComponents on the stock
// object shader, and a custom PSO for them is the large precedent-free job DESIGN_FAR_TREES.md
// ruled out. We do not need to: Wicked already has this exact feature per-object.
// ObjectComponent::draw_distance -> fadeDistance (wiScene.cpp:5281) drives
//     dither = max(0, batch.GetDistance() - fadeDistance) / radius            (wiRenderer.cpp:4237)
// with a real dithered alpha-test, and culls outright past fadeDistance + radius
// (wiRenderer.cpp:8760). Default is FLT_MAX, i.e. every pool tree today opts OUT of it.
//
// So the fade band is [fade, fade + object.radius] - width is the tree's own bounding radius
// rather than DX11's fixed 500, which if anything is nicer (big trees fade over a longer run).
// ⚠ radius is the AABB HALF-DIAGONAL, so the band is wider than the tree is tall; that is fine
// here because the billboard is already fully opaque for the whole of it.
// ⚠ fp16: the engine-side fade math saturates past ~1.66 km. 3500 units = 89 m. Nowhere near.
//
// 0 = no fade (pre-3.03 hard pop). <0 = derive lod_dist + GGTREES_LOD_TRANSITION. >0 = explicit.
// Harness SET_TREEMESHFADE <units>.
float gg_tree_mesh_fade_dist = -1.0f;

// `radius` is the object's world-space AABB radius - the SAME quantity Wicked divides the dither
// by (wiScene.cpp:5349), so the fade ends at exactly fadeDistance + radius. Pass 0 when unknown.
static float TreeMeshFadeDistance( float radius )
{
	if ( gg_tree_mesh_fade_dist == 0.0f ) return 1.0e30f;   // effectively FLT_MAX, no overflow on +radius
	if ( gg_tree_mesh_fade_dist >  0.0f ) return gg_tree_mesh_fade_dist;

	const float lodd     = ggtrees_global_params.lod_dist;
	const float dx11Start = lodd + GG_TREE_LOD_TRANSITION;   // 3500: DX11's own fade start

	// Resolve the pool cap the same way the gather does, then work BACKWARDS from it. The fade
	// band is the tree's own radius wide and can be ~900 units on a big trunk, so a fixed 3500
	// start would still be half-dissolved when the pool hard-drops the slot at 4000 - a smaller
	// pop, but the same bug. Starting at cap-radius makes the dissolve land exactly on the cap.
	float cap = gg_tree_pool_max_dist;
	if ( cap < 0.0f ) cap = lodd + 2.0f * GG_TREE_LOD_TRANSITION;
	if ( cap <= 0.0f ) return dx11Start;   // pool uncapped: nothing to land on, use DX11's number

	float d = cap - radius * 1.1f;   // 10% margin: our radius estimate ignores transform rotation
	if ( d > dx11Start ) d = dx11Start;   // small tree - no reason to start later than DX11 does
	if ( d < lodd ) d = lodd;   // never fade the mesh before the billboard has begun dissolving in
	return d;
}

// GGMAX 3.108: how far tree MESH shadows can reach - the pool cap, resolved exactly as the gather
// resolves it. Every pool tree's mesh (and so its mesh shadow, which since 3.108 dissolves with it)
// has faded out by here, so a Tree Shadow LOD Distance beyond this value changes nothing. The cap
// follows the quality preset (lod_dist 1000/2000/3000/4000 -> 2000/3000/4000/5000), which is why the
// Visuals > Shadows slider asks for it rather than offering a fixed 7000 that no preset reaches.
// Returns a very large value when the pool is uncapped.
float GGTrees_GetMeshShadowReach()
{
	float cap = gg_tree_pool_max_dist;
	if ( cap < 0.0f ) cap = ggtrees_global_params.lod_dist + 2.0f * GG_TREE_LOD_TRANSITION;
	if ( cap <= 0.0f ) return 1.0e30f;
	return cap;
}

static bool             g_wickedTreesSetup   = false;
static std::string      g_treesExeDir;

// Per-tree-type shared assets. Trunk always present (typewise), branches
// nullable — a few tree types (dead pine tree in LOD0) have branches == null;
// those meshes get only subset[0].
static wi::ecs::Entity  g_treeMeshEntity            [ GG_TREE_TYPES ] = { wi::ecs::INVALID_ENTITY };
// LOD1/LOD2 mesh variants, sharing the type's materials. Where a variant is
// missing in the tables, the slot ALIASES the LOD0 entity (never INVALID for a
// valid type) so the pick loop needs no per-frame guards. Shutdown must skip
// aliased entries to avoid double Entity_Remove.
static wi::ecs::Entity  g_treeMeshEntityLOD1        [ GG_TREE_TYPES ] = { wi::ecs::INVALID_ENTITY };
static wi::ecs::Entity  g_treeMeshEntityLOD2        [ GG_TREE_TYPES ] = { wi::ecs::INVALID_ENTITY };
static wi::ecs::Entity  g_treeTrunkMaterialEntity   [ GG_TREE_TYPES ] = { wi::ecs::INVALID_ENTITY };
static wi::ecs::Entity  g_treeBranchesMaterialEntity[ GG_TREE_TYPES ] = { wi::ecs::INVALID_ENTITY };

// Fixed pool of ObjectComponents. meshID is (re)assigned every frame based on
// which tree type the slot represents.
static wi::ecs::Entity  g_treePoolEntities[ GG_TREE_POOL_MAX ] = { wi::ecs::INVALID_ENTITY };

// Append a TreeMeshHigh's verts to the given mesh, unpacking the packed
// R8G8B8A8_UNORM normal to XMFLOAT3 in [-1,1]. Returns the vertex-index offset
// to add to this part's indices when they get merged.
// GGMAX 3.57: tree-sway height mask, in LOCAL model units, matching DX11.
// PIN_HEIGHT is DX11's "posy - 120" pedestal; REF_HEIGHT is the rise over which the weight
// reaches full, chosen so a 600-unit tree tops out at its canopy (600 - 120 = 480).
#define GG_TREE_WIND_PIN_HEIGHT 120.0f
#define GG_TREE_WIND_REF_HEIGHT 480.0f

static uint32_t AppendTreeVerts( wi::scene::MeshComponent& mesh, const TreeMeshHigh* tm )
{
	uint32_t baseVertex = (uint32_t)mesh.vertex_positions.size();

	mesh.vertex_positions  .reserve( baseVertex + tm->numVertices );
	mesh.vertex_normals    .reserve( baseVertex + tm->numVertices );
	mesh.vertex_uvset_0    .reserve( baseVertex + tm->numVertices );
	mesh.vertex_windweights.reserve( baseVertex + tm->numVertices );

	for ( uint32_t v = 0; v < tm->numVertices; v++ )
	{
		const VertexTreeHigh& src = tm->pVertices[ v ];
		mesh.vertex_positions.push_back( XMFLOAT3( src.x, src.y, src.z ) );

		// Little-endian byte order matches DX11 input layout at
		// GGTrees_part0.cpp:1231 (R8G8B8A8_UNORM).
		uint32_t n = src.normal;
		float nx = ( (float)( ( n >>  0 ) & 0xFF ) ) / 127.5f - 1.0f;
		float ny = ( (float)( ( n >>  8 ) & 0xFF ) ) / 127.5f - 1.0f;
		float nz = ( (float)( ( n >> 16 ) & 0xFF ) ) / 127.5f - 1.0f;
		mesh.vertex_normals.push_back( XMFLOAT3( nx, ny, nz ) );

		mesh.vertex_uvset_0.push_back( XMFLOAT2( src.u, src.v ) );

		// GGMAX 3.57: per-vertex WIND WEIGHT - what turns the engine's wind from a rigid
		// translation into a bend. Reproduces DX11's height mask from GGTreesConstants.hlsli,
		// clamp((posy - 120) * 0.35, 0, posy): nothing below 120 local units moves, so the trunk
		// base stays planted, and displacement grows linearly with height above it.
		// DX11's ramp is unbounded; the engine weight is a uint8 0..1, so it is normalized
		// against a FIXED reference height rather than each tree's own. That preserves DX11's
		// property that a taller tree travels further in absolute units (up to the clamp) instead
		// of every tree saturating at its own tip, and it reproduces DX11's numbers: a 600-unit
		// tree reaches weight 1.0 exactly at the canopy, and a 137.5-unit cactus reaches 0.036,
		// about 2.5 units at full wind - which is what DX11 gives it.
		// WARNING: an EMPTY vertex_windweights does NOT mean "no wind". The engine substitutes
		// 0xFF (wiScene_Components.cpp:870), i.e. weight 1.0 on every vertex, which would
		// translate the whole tree rigidly, base included. Filling this is what makes
		// SetUseWind safe to enable below.
		const float ggWindH = ( src.y - GG_TREE_WIND_PIN_HEIGHT ) / GG_TREE_WIND_REF_HEIGHT;
		const float ggWindW = ggWindH < 0.0f ? 0.0f : ( ggWindH > 1.0f ? 1.0f : ggWindH );
		mesh.vertex_windweights.push_back( (uint8_t)( ggWindW * 255.0f + 0.5f ) );
	}

	return baseVertex;
}

// Append a TreeMeshHigh's indices to the given mesh, shifting each by
// `vertexBaseOffset` so branch indices point at the branch vert range that was
// appended after the trunk verts. Returns the number of indices appended.
static uint32_t AppendTreeIndices( wi::scene::MeshComponent& mesh,
	const TreeMeshHigh* tm, uint32_t vertexBaseOffset )
{
	uint32_t startIndex = (uint32_t)mesh.indices.size();
	mesh.indices.reserve( startIndex + tm->numIndices );
	for ( uint32_t i = 0; i < tm->numIndices; i++ )
		mesh.indices.push_back( vertexBaseOffset + (uint32_t)tm->pIndices[ i ] );
	return tm->numIndices;
}

// Build the merged trunk+branches MeshComponent for one tree type. Trunk goes
// in as subset[0] with the trunk (opaque bark) material. Branches, if non-null,
// go in as subset[1] with the leaf (alpha-tested, double-sided) material —
// their indices are re-offset to point at their own vert range.
static wi::ecs::Entity BuildTreeMesh(
	const TreeMeshHigh* trunk, wi::ecs::Entity trunkMaterial,
	const TreeMeshHigh* branches, wi::ecs::Entity branchesMaterial )
{
	auto& scene = wi::scene::GetScene();

	wi::ecs::Entity meshEntity = wi::ecs::CreateEntity();
	wi::scene::MeshComponent& mesh = scene.meshes.Create( meshEntity );

	// Trunk verts first (base offset 0), then trunk indices, then subset[0].
	AppendTreeVerts( mesh, trunk );
	uint32_t trunkIndexCount = AppendTreeIndices( mesh, trunk, 0 );

	wi::scene::MeshComponent::MeshSubset trunkSubset;
	trunkSubset.materialID  = trunkMaterial;
	trunkSubset.indexOffset = 0;
	trunkSubset.indexCount  = trunkIndexCount;
	mesh.subsets.push_back( trunkSubset );

	// Branches (optional) — verts appended after trunk's vert range so we can
	// reuse the same vertex/index buffers. Branch indices get shifted by
	// trunkNumVerts on the append. Subset[1] slices the tail of `mesh.indices`.
	if ( branches != nullptr )
	{
		uint32_t branchBaseVertex   = (uint32_t)mesh.vertex_positions.size();
		AppendTreeVerts  ( mesh, branches );
		uint32_t branchIndexOffset  = (uint32_t)mesh.indices.size();
		uint32_t branchIndexCount   = AppendTreeIndices( mesh, branches, branchBaseVertex );

		wi::scene::MeshComponent::MeshSubset branchSubset;
		branchSubset.materialID  = branchesMaterial;
		branchSubset.indexOffset = branchIndexOffset;
		branchSubset.indexCount  = branchIndexCount;
		mesh.subsets.push_back( branchSubset );
	}

	mesh.CreateRenderData();
	return meshEntity;
}

// Build a PBR MaterialComponent that samples one tree DDS as basecolor.
// `isBranches` = true switches to alpha-tested + double-sided leaves. Trunks
// stay opaque + single-sided (BACK-face culled).
static wi::ecs::Entity BuildTreeMaterial( const char* textureName, bool isBranches, int recordType = -1 )
{
	auto& scene = wi::scene::GetScene();

	wi::ecs::Entity matEntity = wi::ecs::CreateEntity();
	wi::scene::MaterialComponent& mat = scene.materials.Create( matEntity );

	// Branches and trunks both use an untinted white baseColor so the leaf DDS
	// colours pass through exactly as authored — matching DX11, which samples
	// the same textures with no material tint. (A dark-green branch tint lived
	// here until 2026-07-17; it was a leftover from the retired impostor-capture
	// path and turned red autumn leaf textures muddy brown: red x green = dark
	// brown. Removed after the A/B exposed it — do not reintroduce.)
	mat.SetBaseColor( XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f ) );
	mat.SetRoughness ( 1.0f );
	mat.SetMetalness ( 0.0f );
	mat.SetReflectance( 0.02f );

	char colorPath[ 512 ];
	sprintf_s( colorPath, "%s/Files/treebank/textures/%s", g_treesExeDir.c_str(), textureName );
	mat.textures[ wi::scene::MaterialComponent::BASECOLORMAP ].name = colorPath;
	if ( recordType >= 0 ) GGTrees_RecordTextureSource( recordType, isBranches ? 3 : 2, colorPath, false );

	if ( isBranches )
	{
		// Alpha-tested leaves — cut out background between the leaf silhouettes
		// via the basecolor DDS's alpha channel. 0.5 is the standard foliage
		// cutoff (see Wicked's alphaRef semantics — anything < 1.0-1/256 counts
		// as alpha-test enabled). Double-sided so leaves show from both sides.
		// Wicked semantics: shader clips at (1 - alphaRef), so HIGHER alphaRef
		// keeps MORE texels. 0.85 = clip below alpha 0.15 — full canopies that
		// survive mip-averaged alpha at distance, background still cut.
		mat.SetAlphaRef   ( 0.85f );
		mat.SetDoubleSided( true );
	}

	// GGMAX 3.57: opt this material into the engine's per-vertex wind. Applied to BOTH the
	// trunk and the branches, deliberately: DX11 waved trunk and canopy with the same function,
	// and the height mask above is what keeps the base planted. Enabling it on leaves only
	// would slide the canopy off a rigid trunk. The displacement happens inside the engine's
	// shared VertexSurface::create, so prepass, colour, shadow and env-probe move identically
	// by construction - the one property this codebase has twice paid for losing.
	mat.SetUseWind( true );

	mat.SetTextureStreamingDisabled( true );
	mat.CreateRenderData();

	return matEntity;
}

// GGMAX 2.24 (2026-08-11): build ONE tree type's assets, on demand. Returns false if the type
// has no trunk in the static tables (nothing can ever be built for it).
//
// WHY THIS IS NOW LAZY. Setup used to build ALL 38 types x 3 LOD meshes + their materials up
// front, at app startup, before any level was known — 114 MeshComponents + 70 MaterialComponents
// that a level with no trees paid for in full, and that a level using 17 types still paid double
// for. The empty-level census measured exactly that (SWITCHESCAPE_PERF.md §16). DX11 keeps its
// tree geometry in plain GPU buffers and creates no ECS entities at all.
//
// The driver is the spatial-grid rebuild's `passes` filter, which already walks every tree
// instance whenever the level's tree data changes. Asking it to ensure the type means each level
// builds exactly the types it actually places, once, at load — not at app startup, and not the
// 21 types it never uses. A late first sighting (camera reaches a new area with a new type) costs
// one type's build mid-frame; that is 3 small meshes, far cheaper than the 114 it replaces.
static bool EnsureTreeType( uint32_t t )
{
	if ( t >= GG_TREE_TYPES ) return false;
	if ( g_treeMeshEntity[ t ] != wi::ecs::INVALID_ENTITY ) return true;   // already built

	const GGTree& tree = g_GGTrees[ t ];
	if ( !tree.trunk ) return false;                                        // nothing to build, ever

	g_treeTrunkMaterialEntity[ t ] = BuildTreeMaterial( tree.trunk->textureName, false, (int)t );

	wi::ecs::Entity branchesMat = wi::ecs::INVALID_ENTITY;
	if ( tree.branches )
	{
		branchesMat = BuildTreeMaterial( tree.branches->textureName, true, (int)t );
		g_treeBranchesMaterialEntity[ t ] = branchesMat;
	}

	g_treeMeshEntity[ t ] = BuildTreeMesh(
		tree.trunk,    g_treeTrunkMaterialEntity[ t ],
		tree.branches, branchesMat );

	// LOD1/LOD2 variants share the type's materials (same texture set, just
	// decimated geometry). A missing variant aliases the previous LOD so the
	// per-frame pick never needs a validity check. If a lower LOD has
	// branches where LOD0 had none, build the branch material on demand.
	auto buildLodVariant = [&]( const GGTree& lodTree, wi::ecs::Entity fallback ) -> wi::ecs::Entity
	{
		if ( !lodTree.trunk ) return fallback;
		wi::ecs::Entity lodBranchesMat = branchesMat;
		if ( lodTree.branches && lodBranchesMat == wi::ecs::INVALID_ENTITY )
		{
			lodBranchesMat = BuildTreeMaterial( lodTree.branches->textureName, true, (int)t );
			g_treeBranchesMaterialEntity[ t ] = lodBranchesMat;
		}
		return BuildTreeMesh( lodTree.trunk, g_treeTrunkMaterialEntity[ t ],
			lodTree.branches, lodBranchesMat );
	};
	g_treeMeshEntityLOD1[ t ] = buildLodVariant( g_GGTreesLOD1[ t ], g_treeMeshEntity[ t ] );
	g_treeMeshEntityLOD2[ t ] = buildLodVariant( g_GGTreesLOD2[ t ], g_treeMeshEntityLOD1[ t ] );
	return true;
}

// GGMAX 2.24: free every built tree type. Called from ReleaseTreePool, so the type assets go the
// same way the pool and the shadow proxies do — a treeless level should carry none of the
// previous level's tree data. LOD1/LOD2 may ALIAS a higher LOD (missing variant), so only
// distinct entities are removed, exactly as GGTrees_WickedShutdown does.
static void ReleaseTreeTypes( wi::scene::Scene& scene )
{
	for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
	{
		if ( g_treeMeshEntityLOD2[ t ] != wi::ecs::INVALID_ENTITY && g_treeMeshEntityLOD2[ t ] != g_treeMeshEntityLOD1[ t ] && g_treeMeshEntityLOD2[ t ] != g_treeMeshEntity[ t ] )
			scene.Entity_Remove( g_treeMeshEntityLOD2[ t ] );
		if ( g_treeMeshEntityLOD1[ t ] != wi::ecs::INVALID_ENTITY && g_treeMeshEntityLOD1[ t ] != g_treeMeshEntity[ t ] )
			scene.Entity_Remove( g_treeMeshEntityLOD1[ t ] );
		if ( g_treeMeshEntity            [ t ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_treeMeshEntity            [ t ] );
		if ( g_treeTrunkMaterialEntity   [ t ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_treeTrunkMaterialEntity   [ t ] );
		if ( g_treeBranchesMaterialEntity[ t ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_treeBranchesMaterialEntity[ t ] );
		g_treeMeshEntity            [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeMeshEntityLOD1        [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeMeshEntityLOD2        [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeTrunkMaterialEntity   [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeBranchesMaterialEntity[ t ] = wi::ecs::INVALID_ENTITY;
	}
}

static void GGTrees_WickedSetup()
{
	// Capture EXE directory once — DDS lookups need CWD-independent paths.
	g_treesExeDir = wi::helper::GetDirectoryFromPath( wi::helper::GetExecutablePath() );
	if ( !g_treesExeDir.empty() && ( g_treesExeDir.back() == '/' || g_treesExeDir.back() == '\\' ) )
		g_treesExeDir.pop_back();


	// GGMAX 2.24: NO type assets are built here any more — see EnsureTreeType. Setup only
	// validates that the compiled-in tables actually contain tree data. `typesBuilt` therefore
	// counts types that COULD be built, which is what the latch below really wanted to know.
	uint32_t typesBuilt = 0, branchesBuilt = 0;
	for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
	{
		const GGTree& tree = g_GGTrees[ t ];
		if ( !tree.trunk ) continue;
		if ( tree.branches ) branchesBuilt++;

		// Stage 4 (deprecated 2026-07-13 evening): no ImpostorComponent.
		// Wicked's impostor render path (wiRenderer.cpp:7298 RenderImpostors)
		// draws via a separate DrawIndexedInstancedIndirect that does NOT
		// respect ObjectComponent::IsNotVisibleInReflections. Result: water
		// reflection pass drew impostor atlas quads as bright white splats
		// (edge-alpha pixels sampled from the un-tinted capture). Since our
		// nearest-N-to-camera pick fills the 10K pool with just the near
		// trees anyway — the trees past swapInDistance would have been
		// pixelated dot-scale billboards contributing minimal visual value
		// for the ECS overhead — we drop the impostor path entirely. Far
		// trees now just don't render; the pool covers what's visible.
		typesBuilt++;
	}

	// Entity pool. Each slot has an ObjectComponent (meshID assigned per frame)
	// and a TransformComponent (position/scale per frame). Starts hidden until
	// the update loop finds it a tree to represent.
	//
	// SetNotVisibleInReflections(true): trees don't render into planar/water
	// reflection passes. Prevents Stage 4 impostor billboards from showing up
	// as bright white squares in the water (Wicked would otherwise render the
	// impostor atlas quad in the reflection pass same as the main pass).
	// One-shot flag, persists across every SetRenderable toggle from the
	// update loop.
	// Clamp the runtime pool size to the fixed array capacity (SET_TREES pool <N> can set it).
	// GGMAX 2.19: NO slots are created here any more — see GrowTreePool. Setup only decides the
	// ceiling. A level with no trees never calls GrowTreePool and so costs zero ECS entities.
	if ( g_treePoolSize < 1u ) g_treePoolSize = 1u;
	if ( g_treePoolSize > GG_TREE_POOL_MAX ) g_treePoolSize = GG_TREE_POOL_MAX;
	g_treePoolBuilt = 0;

	if ( typesBuilt == 0 )
	{
		// Should be impossible (g_GGTrees is compiled-in static data), but never
		// latch setup "done" with zero meshes — let the next frame retry rather
		// than rendering nothing forever. (Nothing to tear down: the pool is empty
		// until GrowTreePool runs.)
		return;
	}

	g_wickedTreesSetup = true;
	wi::backlog::post( ( "GGTrees: real tree meshes ready ("
		+ std::to_string( typesBuilt )    + " trunks, "
		+ std::to_string( branchesBuilt ) + " with branches, pool ceiling "
		+ std::to_string( g_treePoolSize ) + " slots, grown on demand)" ).c_str() );
}

// Debug/benchmark hook: while > 0, the nearest-N selection re-runs every frame
// (decremented per frame). Set via the SET_TREES harness command to measure
// the rescan cost with the profiler — a moving camera does the same thing.
int g_treePoolStressFrames = 0;

// Spatial grid state (declared here so init/shutdown can invalidate it; the
// grid itself — rebuild + ring gather — lives with the selection code below).
// Entries carry their OWN x/z so the ring gather streams the CSR arrays
// linearly — an index-only payload made the gather deref pAllTrees per tree
// (random access into an 8MB array = a cache miss each; measured 3.9ms of a
// 5.2ms rescan). Self-contained entries cut that to a linear ~1MB stream.
struct TreeGridEntry { float x, z; uint32_t idx; };
static constexpr uint32_t GG_TREE_GRID_DIM = 128;                        // 128x128 cells over treeArea
static std::vector<uint32_t>      g_gridCellStart;                       // CSR: cell -> first entry, size DIM*DIM+1
static std::vector<TreeGridEntry> g_gridEntries;                         // CSR: filtered trees, cell-grouped
static uint32_t g_gridStamp = ~0u;
static bool     g_gridValid = false;

// Per-chunk far-shadow proxy state (declared here so init/shutdown can reset
// it; the builders live with the proxy code below). Rebuilt INCREMENTALLY:
// tree setters mark only the owning chunk dirty (GGTrees_MarkProxyChunkDirtyAt,
// forward-declared in part0) and the update loop rebuilds dirty chunks within
// a small per-frame time budget. The previous all-256-chunks-in-one-frame
// batch rebuilt every merged mesh on the island after any tree edit — a ~2s
// editor freeze.
static std::vector<wi::ecs::Entity> g_chunkProxyMesh;    // size numTreeChunks, INVALID = chunk has no proxy
static std::vector<wi::ecs::Entity> g_chunkProxyObject;
static std::vector<uint8_t>         g_chunkProxyDirty;   // pending rebuild per chunk

void GGTrees_WickedInit()
{
	// Lazy setup on first update — same phase ordering as SetupWickedGrass /
	// SetupWickedTerrainMaterials. State reset here only.
	g_wickedTreesSetup = false;
	g_treePoolBuilt = 0;   // GGMAX 2.19: no slots exist until GrowTreePool is asked for some
	for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
	{
		g_treeMeshEntity            [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeTrunkMaterialEntity   [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeBranchesMaterialEntity[ t ] = wi::ecs::INVALID_ENTITY;
	}
	for ( uint32_t i = 0; i < GG_TREE_POOL_MAX; i++ )
		g_treePoolEntities[ i ] = wi::ecs::INVALID_ENTITY;
	g_gridValid = false;   // spatial grid must not survive an engine restart
	// proxy entities belong to the previous scene — forget them, rebuild all
	g_chunkProxyMesh.clear();
	g_chunkProxyObject.clear();
	g_chunkProxyDirty.clear();
}

// ---------------------------------------------------------------------------
// Stable slot binding (perf fix 2026-07-18).
//
// The pool used to rebind meshID + rebuild the transform of EVERY slot EVERY
// frame in nth_element order: 20K UpdateTransform calls = ~20ms CPU (measured
// "TerrainW - Tree Pool" 19.7ms, 48% of the whole frame at the A/B camera).
// nth_element order is also arbitrary frame to frame, so a pool entity's
// previous-frame matrix belonged to a DIFFERENT tree — wrong motion vectors,
// TAA/motion-blur ghosting on every tree.
//
// Now a slot stays bound to the same tree until the nearest-N set evicts it.
// The 400K candidate scan + nth_element still run each frame (cheap), but the
// bind loop only touches slots whose tree binding, LOD band, or source data
// actually changed — near-zero component writes with a still camera, and each
// pool entity keeps a stable identity for motion vectors.
// ---------------------------------------------------------------------------
static std::vector<uint32_t> g_slotToTree;       // pool slot -> pAllTrees index, UINT32_MAX = free
static std::vector<int32_t>  g_treeToSlot;       // pAllTrees index -> pool slot, -1 = unbound
static std::vector<uint32_t> g_treeDesiredEpoch; // per tree: frame epoch when it last made the nearest-N cut
static std::vector<uint8_t>  g_treeBand;         // per tree: LOD band, valid when epoch matches current
static std::vector<uint8_t>  g_treeShadow;       // per tree: 1 = within lod_dist_shadow (mesh casts shadow), valid when epoch matches
struct TreeSlotCache { float x, y, z, scale; uint8_t type, band, shadow; };
static std::vector<TreeSlotCache> g_slotCache;   // per slot: source-data snapshot at bind time
static uint32_t g_treeEpoch = 0;

// GGMAX 2.19: grow the pool towards `want` slots, capped per frame.
// Creates exactly what the old eager setup loop created, so a grown slot is indistinguishable
// from a pre-created one; only the moment of creation changed. Returns nothing — callers read
// g_treePoolBuilt. Safe to call every frame; it is a no-op once built >= want.
//
// The per-slot flags below are the originals and their reasoning is unchanged:
//  - SetNotVisibleInReflections: trees must not draw into planar/water reflection passes.
//  - SetOcclusionQueryDisabled (Wicked delta #6): 20K query slots + proxy draws cost ~2.5 ms CPU
//    + ~1.3 ms GPU per frame on TESTPRO1 and foliage occludes almost nothing.
//  - cascadeMask = 2: real tree meshes shadow only cascades 0-2 (out to 7500 units). Casting
//    them into cascade 3 cost ~11 ms CPU + 7 ms GPU; the merged billboard shadow proxies cover
//    the far cascades instead (DX11's own mesh-near/billboard-far recipe).
static void GrowTreePool( wi::scene::Scene& scene, uint32_t want )
{
	if ( want > g_treePoolSize ) want = g_treePoolSize;
	if ( want <= g_treePoolBuilt ) return;

	uint32_t target = want;
	if ( target - g_treePoolBuilt > GG_TREE_POOL_GROW_PER_FRAME )
		target = g_treePoolBuilt + GG_TREE_POOL_GROW_PER_FRAME;

	for ( uint32_t i = g_treePoolBuilt; i < target; i++ )
	{
		wi::ecs::Entity e = wi::ecs::CreateEntity();
		wi::scene::ObjectComponent& obj = scene.objects.Create( e );
		obj.SetRenderable( false );
		obj.SetNotVisibleInReflections( true );
		obj.SetOcclusionQueryDisabled( true );
		// GGMAX 3.108 (tester report #5): the mesh's SUN SHADOW dissolves over the same band as the
		// mesh (3.03 fade) instead of staying solid until the engine culls the caster at the end of
		// that band and dropping out in one frame - which took the tree's whole self-shadow with it.
		obj.SetShadowDistanceFade( true );
		obj.gg_shadow_fade_start = ggtrees_global_params.lod_dist_shadow;   // GGMAX 3.108b: slider distance fades too
		obj.draw_distance = TreeMeshFadeDistance( 0.0f );   // GGMAX 3.03: placeholder, BindTreeSlot refines it
		obj.cascadeMask = 2;
		scene.transforms.Create( e );
		g_treePoolEntities[ i ] = e;
	}

	// Keep the per-slot side arrays exactly in step with the entity count. They are compared
	// against g_treePoolBuilt every frame to detect a stale pool, so letting them drift would
	// force a full rebind on every single frame.
	g_slotToTree.resize( target, UINT32_MAX );
	g_slotCache .resize( target );

	g_treePoolBuilt = target;
}

// DX11 tree_shadow_range semantics: number of shadow cascades that receive tree
// shadows (5 = all, 0 = none). Wicked cascadeMask counts skipped-FARTHEST
// cascades, so proxy mask = 5 - range. Real tree meshes additionally never go
// beyond cascade 2 (their detail is wasted there and 20K far casters cost
// ~11ms CPU + 7ms GPU) -> mesh mask = max(2, 5 - range).
static uint32_t TreeShadowRangeClamped()
{
	int r = ggtrees_global_params.tree_shadow_range;
	return (uint32_t)( r < 0 ? 0 : r > 5 ? 5 : r );
}
static uint32_t TreeProxyCascadeMask() { return 5u - TreeShadowRangeClamped(); }
static uint32_t TreeMeshCascadeMask()
{
	uint32_t skip = 5u - TreeShadowRangeClamped();
	return skip < 2u ? 2u : skip;
}
// range==0 = DX11 "no tree shadows at all" (mesh AND proxy)
static bool TreeShadowsEnabled()
{
	return ggtrees_global_params.draw_shadows != 0 && TreeShadowRangeClamped() > 0u;
}

static wi::ecs::Entity TreeMeshForBand( uint32_t type, uint8_t band )
{
	return ( band == 0 ) ? g_treeMeshEntity    [ type ] :
	       ( band == 1 ) ? g_treeMeshEntityLOD1[ type ] :
	                       g_treeMeshEntityLOD2[ type ];
}

// ---------------------------------------------------------------------------
// Spatial grid over pAllTrees (camera-move rescan fix, 2026-07-18).
//
// The nearest-N selection used to stream all 400K instances (~8 MB) through a
// branchy filter on EVERY camera move >8", then nth_element the ~10^5
// survivors: ~8-10ms on movement frames (editor flythrough dipped 60 -> 45-55
// FPS). The grid bins the FILTERED trees once per data change (stamp), and a
// camera move only ring-gathers cells around the camera until the pool is
// provably covered — candidates drop from ~10^5 to a few 10^4.
//
// CSR layout, rebuilt whole on any g_treeInstanceStamp change (the mutation
// surface is too wide for incremental updates — one path is a raw memcpy).
// The heartbeat also invalidates it, so any mutation that fails to announce
// itself is repaired within 256 frames, same insurance as before. The grid is
// only a pruning structure: final selection always uses exact per-tree
// distances, so binning can never misplace a chosen tree.
// ---------------------------------------------------------------------------
static inline int TreeGridCoord( float v )
{
	int c = (int)( ( v / treeArea + 0.5f ) * (float)GG_TREE_GRID_DIM );
	return c < 0 ? 0 : c >= (int)GG_TREE_GRID_DIM ? (int)GG_TREE_GRID_DIM - 1 : c;
}

static void RebuildTreeGrid()
{
	const uint32_t numCells = GG_TREE_GRID_DIM * GG_TREE_GRID_DIM;
	static std::vector<uint32_t> cellCount;
	cellCount.assign( numCells, 0 );
	g_gridCellStart.assign( numCells + 1, 0 );

	// The filter matches the candidate loop exactly: every flag it reads is
	// mutated through a stamped setter, and mesh availability is fixed after
	// GGTrees_WickedSetup (which has always run by the time we're called).
	// (InstanceTree accessors are not const-qualified, hence non-const refs.)
	auto passes = []( InstanceTree& t ) -> bool
	{
		if ( !t.IsVisible() || t.IsInvalid() || t.IsFlattened() ) return false;
		uint32_t type = (uint32_t)t.GetType();
		if ( type >= GG_TREE_TYPES ) return false;
		// GGMAX 2.24: build this type's assets on first sight instead of testing whether setup
		// already built all 38. This filter walks every tree instance whenever the level's tree
		// data changes, so it is the natural place to realise exactly the types the level uses.
		if ( !EnsureTreeType( type ) ) return false;
		return true;
	};

	for ( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		InstanceTree& t = pAllTrees[ i ];
		if ( !passes( t ) ) continue;
		cellCount[ TreeGridCoord( t.z ) * GG_TREE_GRID_DIM + TreeGridCoord( t.x ) ]++;
	}

	uint32_t total = 0;
	for ( uint32_t c = 0; c < numCells; c++ )
	{
		g_gridCellStart[ c ] = total;
		total += cellCount[ c ];
	}
	g_gridCellStart[ numCells ] = total;

	g_gridEntries.resize( total );
	static std::vector<uint32_t> cellCursor;
	cellCursor.assign( g_gridCellStart.begin(), g_gridCellStart.end() - 1 );
	for ( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		InstanceTree& t = pAllTrees[ i ];
		if ( !passes( t ) ) continue;
		const uint32_t c = TreeGridCoord( t.z ) * GG_TREE_GRID_DIM + TreeGridCoord( t.x );
		g_gridEntries[ cellCursor[ c ]++ ] = { t.x, t.z, i };
	}
}

// Bind pool slot -> tree: swap in the merged trunk+branches mesh for the LOD
// band and rebuild the slot transform. Wicked's per-object AABB derives from
// mesh AABB * transform, so culling picks the change up on the next scene
// update. Returns false (mappings untouched) if the tree can't be represented.
static bool BindTreeSlot( wi::scene::Scene& scene, uint32_t slot, uint32_t treeIdx, uint8_t band )
{
	InstanceTree* pTree = &pAllTrees[ treeIdx ];
	uint32_t type = (uint32_t)pTree->GetType();
	if ( type >= GG_TREE_TYPES ) return false;
	if ( !EnsureTreeType( type ) ) return false;   // GGMAX 2.24: safety net; grid build normally did it
	wi::ecs::Entity treeMesh = TreeMeshForBand( type, band );
	if ( treeMesh == wi::ecs::INVALID_ENTITY ) return false;

	wi::ecs::Entity e = g_treePoolEntities[ slot ];
	wi::scene::ObjectComponent*    obj   = scene.objects   .GetComponent( e );
	wi::scene::TransformComponent* xform = scene.transforms.GetComponent( e );
	if ( !obj || !xform ) return false;

	obj->meshID = treeMesh;
	obj->SetRenderable( true );
	// Exact DX11 recipe: detailed MESH shadows only within lod_dist_shadow
	// (slider-driven, default 2500); every tree beyond that gets its shadow
	// from the merged billboard proxies instead. Keeps the near-shadow
	// crispness and collapses the far-cascade caster count.
	const uint8_t castShadow = g_treeShadow[ treeIdx ];
	obj->SetCastShadow( TreeShadowsEnabled() && castShadow != 0 );
	obj->gg_shadow_fade_start = ggtrees_global_params.lod_dist_shadow;   // GGMAX 3.108b

	float scale = pTree->GetScaleFloat();  // 0.5 .. ~1.99 (byte/170 + 0.5; the default paint range tops out at ~1.94)

	// GGMAX 3.03: dissolve the mesh out where the billboard has finished dissolving in, instead of
	// letting the pool cap hard-drop it. Scaled mesh radius approximates the world AABB radius that
	// Scene::Update will compute next frame; being one frame stale is harmless (the value only
	// moves when this slot rebinds to a different-sized tree, and the band is ~500 units wide).
	{
		const wi::scene::MeshComponent* mc = scene.meshes.GetComponent( treeMesh );
		const float meshRadius = mc ? mc->aabb.getRadius() * scale : 0.0f;
		obj->draw_distance = TreeMeshFadeDistance( meshRadius );
	}

	xform->ClearTransform();
	xform->Scale    ( XMFLOAT3( scale, scale, scale ) );
	xform->Translate( XMFLOAT3( pTree->x, pTree->y, pTree->z ) );
	xform->UpdateTransform();

	g_slotToTree[ slot ]    = treeIdx;
	g_treeToSlot[ treeIdx ] = (int32_t)slot;
	g_slotCache [ slot ]    = { pTree->x, pTree->y, pTree->z, scale, (uint8_t)type, band, castShadow };
	return true;
}

// ---------------------------------------------------------------------------
// Far tree shadow proxies (DX11 shadow-distance parity, 2026-07-18).
//
// Production DX11 gets island-wide tree shadows cheaply: real tree meshes
// shadow only within lod_dist_shadow, and every tree beyond that renders a
// BILLBOARD QUAD into the far shadow cascades. Rendering our 20K real meshes
// into the far cascades instead cost ~11ms CPU + 7ms GPU (cascade 3 alone).
//
// So: per tree chunk (16x16 grid), ONE merged static mesh holding two crossed
// vertical quads per valid tree, textured with the type's authored billboard
// silhouette DDS (Files/treebank/billboards/*_BB_SF_*_color.dds — the exact
// same asset DX11 uses), alpha-tested. The objects are shadow-only: invisible
// to the main camera and reflections, no occlusion queries. Per frame they
// cost one object each (max 256); the real tree meshes keep cascadeMask=2 so
// they only shadow the near cascades where their detail shows.
//
// Rebuilt as one deferred batch ~0.5s after the last tree edit (same
// g_treeInstanceStamp signal the pool uses).
// ---------------------------------------------------------------------------
static wi::ecs::Entity g_shadowProxyMaterial[ GG_TREE_TYPES ] = { wi::ecs::INVALID_ENTITY };

// GGMAX 2.95: squared XZ radius the nearest-N pool actually reaches this frame, or -1 when the
// pool covers every tree. Set in the SelectMark pass, consumed by the far-billboard gate.
static float g_treePoolCutoffDist2 = -1.0f;
static uint32_t g_ftProxiesShown = 0;   // GGMAX 2.95 diagnostics for SET_FARTREES
static uint32_t g_ftProxyCount   = 0;
static uint32_t g_ftCandidates   = 0;
static uint32_t g_ftProxyValid   = 0;
static float    g_ftNearest      = -1.0f;
static float    g_ftFarthest     = -1.0f;

void GGTrees_MarkProxyChunkDirtyAt( float x, float z )
{
	if ( g_chunkProxyDirty.size() != (size_t)numTreeChunks ) g_chunkProxyDirty.assign( numTreeChunks, 1 );
	// same cell mapping as GGTrees_GetChunk (out of range = no chunk, no mark)
	const int iX = (int)( ( ( x / treeArea ) + 0.5f ) * treeSplit );
	const int iZ = (int)( ( ( z / treeArea ) + 0.5f ) * treeSplit );
	if ( iX < 0 || iZ < 0 || iX >= (int)treeSplit || iZ >= (int)treeSplit ) return;
	g_chunkProxyDirty[ iZ * treeSplit + iX ] = 1;
}

void GGTrees_MarkAllProxyChunksDirty()
{
	g_chunkProxyDirty.assign( numTreeChunks, 1 );
}

static wi::ecs::Entity BuildBillboardShadowMaterial( uint32_t type )
{
	auto& scene = wi::scene::GetScene();

	wi::ecs::Entity matEntity = wi::ecs::CreateEntity();
	wi::scene::MaterialComponent& mat = scene.materials.Create( matEntity );

	mat.SetBaseColor( XMFLOAT4( 1.0f, 1.0f, 1.0f, 1.0f ) );
	mat.SetRoughness ( 1.0f );
	mat.SetMetalness ( 0.0f );
	mat.SetReflectance( 0.02f );

	char colorPath[ MAX_PATH ];
	sprintf_s( colorPath, "%s/Files/treebank/billboards/%s",
		g_treesExeDir.c_str(), g_GGTrees[ type ].billboardFilename );
	mat.textures[ wi::scene::MaterialComponent::BASECOLORMAP ].name = colorPath;

	// Wicked semantics: shader clips at (1 - alphaRef) — 0.85 keeps texels with
	// alpha >= 0.15, same cutoff as the leaf materials.
	mat.SetAlphaRef   ( 0.85f );
	mat.SetDoubleSided( true );
	mat.SetTextureStreamingDisabled( true );
	mat.CreateRenderData();
	return matEntity;
}

static void GGTrees_SetShadowProxiesVisible( wi::scene::Scene& scene, bool visible )
{
	for ( wi::ecs::Entity e : g_chunkProxyObject )
	{
		wi::scene::ObjectComponent* obj = scene.objects.GetComponent( e );
		if ( obj ) obj->SetRenderable( visible );
	}
}

static void GGTrees_RemoveShadowProxyChunk( wi::scene::Scene& scene, uint32_t c )
{
	if ( g_chunkProxyObject[ c ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_chunkProxyObject[ c ] );
	if ( g_chunkProxyMesh  [ c ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_chunkProxyMesh  [ c ] );
	g_chunkProxyObject[ c ] = wi::ecs::INVALID_ENTITY;
	g_chunkProxyMesh  [ c ] = wi::ecs::INVALID_ENTITY;
}

// GGMAX 2.23: destroy every built pool slot and drop all bindings. The inverse of GrowTreePool.
//
// Called only from the sustained-park branch of GGTrees_WickedUpdate (see the long comment
// there) — i.e. the trees are gone for good, which in practice means a level change. It is
// deliberately NOT called on ordinary camera movement: one-way growth within a level is correct
// and avoids create/destroy thrash.
//
// Scope note: this releases the POOL only. The per-type tree meshes/materials (38 types x 3 LODs
// = 114 meshes + 70 materials, built once by GGTrees_WickedSetup) are intentionally KEPT, so a
// later tree level does not pay to rebuild them or re-upload their GPU data. Those are a fixed
// ~184 entities; the pool is up to 6000 objects PLUS 6000 transforms, which is the part that
// costs per-frame Scene::Update work.
//
// Bindings must die with the slots: g_slotToTree/g_slotCache are indexed BY SLOT and would
// otherwise describe entities that no longer exist, and g_treeToSlot points back at them. The
// next GGTrees_WickedUpdate re-derives all of it (the size mismatch it leaves behind is exactly
// what the forceRebind check keys on).
static void ReleaseTreePool( wi::scene::Scene& scene )
{
	for ( uint32_t i = 0; i < g_treePoolBuilt; i++ )
	{
		if ( g_treePoolEntities[ i ] != wi::ecs::INVALID_ENTITY )
		{
			scene.Entity_Remove( g_treePoolEntities[ i ] );
			g_treePoolEntities[ i ] = wi::ecs::INVALID_ENTITY;
		}
	}
	g_treePoolBuilt = 0;
	g_slotToTree.clear();
	g_slotCache .clear();
	g_treeToSlot.clear();
	g_treeDesiredEpoch.clear();
	g_treeBand  .clear();
	g_treeShadow.clear();

	// GGMAX 2.23b: the FAR-SHADOW PROXIES go too, and they were the residual leak.
	//
	// After 2.23 fixed the 6000-slot pool retention, a treeless level loaded after a tree level
	// still carried +79 objects, +79 meshes and +17 materials. Named by diffing DUMP_MATERIALS
	// and DUMP_TREEPOOL across the two load orders: `proxyChunks=0` when loaded FRESH versus
	// `proxyChunks=256` when loaded AFTER — the merged billboard shadow chunks built for the
	// PREVIOUS level's trees. GGTrees_SetShadowProxiesVisible only calls SetRenderable(false)
	// (see just above), so a park hides them and nothing ever removes them; the only teardown
	// lives in GGTrees_WickedShutdown, whose caller has no callers.
	//
	// The arithmetic matched exactly: 79 chunks that actually held trees -> 79 proxy objects +
	// 79 proxy meshes, and 17 distinct tree types present -> 17 billboard shadow materials.
	//
	// They must die with the pool: a proxy describes where the PREVIOUS level's trees cast far
	// shadows, so keeping it is not just wasted per-frame ECS work, it is stale data. Unlike the
	// per-type tree meshes/materials (kept deliberately — they are level-independent assets), the
	// proxies are rebuilt per level from tree data anyway, by the dirty-chunk path.
	for ( uint32_t c = 0; c < g_chunkProxyObject.size(); c++ )
	{
		GGTrees_RemoveShadowProxyChunk( scene, c );
	}
	g_chunkProxyObject.clear();
	g_chunkProxyMesh  .clear();
	g_chunkProxyDirty .clear();
	for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
	{
		if ( g_shadowProxyMaterial[ t ] != wi::ecs::INVALID_ENTITY )
		{
			scene.Entity_Remove( g_shadowProxyMaterial[ t ] );
			g_shadowProxyMaterial[ t ] = wi::ecs::INVALID_ENTITY;
		}
	}

	// GGMAX 2.24: the per-type assets go too. Before 2.24 they were built once at startup for all
	// 38 types and deliberately kept; now they are built per level for only the types that level
	// uses, so KEEPING them across a level change would be the same retention bug just fixed for
	// the pool and the proxies. The next tree level rebuilds exactly what it needs at grid-build.
	ReleaseTreeTypes( scene );

	wi::backlog::post( "GGTrees: tree pool + shadow proxies + type assets released (no trees)" );
}

// GGMAX 3.54: RELEASE THE LEVEL'S TREE ASSETS WHEN THE LEVEL CHANGES.
//
// EnsureTreeType builds, per tree type the level places, a trunk material, an optional branches
// material and up to three meshes (base + LOD1 + LOD2) - all via wi::ecs::CreateEntity() with NO
// NameComponent. The only thing that frees them is ReleaseTreeTypes, whose ONLY caller is
// ReleaseTreePool, which ran only after the pool had been parked for 600 consecutive frames or
// from GGTrees_WickedShutdown - and that shutdown's caller chain (GGTerrainWicked_Shutdown) has
// ZERO callers. A level change reaches neither, so the outgoing level's type assets stayed
// resident while the incoming level built its own.
//
// Measured River Raiders -> hub -> Island Showdown vs Island Showdown first: +15 meshes,
// +17 materials, +1 object, +1 transform. The leak-namer reported the survivors as UNNAMED and
// 14 of the meshes as ORPHAN, which is exactly this signature - GG names nearly every loaded
// mesh, and these are created nameless.
//
// 2.23 already intended this ("release the tree pool on a sustained park - fixes 6000 entities
// retained across level change") but implemented it as a park HEURISTIC; a level change does not
// guarantee 600 idle frames, so the intent never fired. This is the explicit hook.
//
// Routed through ReleaseTreePool deliberately: the pool's ObjectComponents reference the type
// MESHES, so the pool and the shadow proxies must go first. ReleaseTreePool already does that in
// the right order. The pool regrows lazily on demand (2.19), which a level change does anyway.
void GGTrees_ReleaseLevelAssets()
{
	if ( !ggtrees_initialised ) return;
	if ( !g_wickedTreesSetup ) return;   // nothing built yet; nothing to free
	ReleaseTreePool( wi::scene::GetScene() );
}

// Rebuild ONE chunk's merged billboard shadow mesh (materials kept and reused).
static void GGTrees_BuildShadowProxyChunk( uint32_t c )
{
	auto& scene = wi::scene::GetScene();

	GGTrees_RemoveShadowProxyChunk( scene, c );

	if ( !TreeShadowsEnabled() ) return;

	// Per-type instance buckets, reused across calls to keep allocations warm.
	static std::vector<InstanceTree*> byType[ GG_TREE_TYPES ];

	{
		TreeChunk* pChunk = &pTreeChunks[ c ];
		const uint32_t numInst = pChunk->pInstances.NumItems();
		if ( numInst == 0 ) return;

		for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ ) byType[ t ].clear();
		uint32_t validCount = 0;
		for ( uint32_t j = 0; j < numInst; j++ )
		{
			InstanceTree* p = pChunk->pInstances[ j ];
			if ( !p->IsVisible() || p->IsInvalid() || p->IsFlattened() ) continue;
			uint32_t type = (uint32_t)p->GetType();
			if ( type >= GG_TREE_TYPES ) continue;
			if ( !EnsureTreeType( type ) ) continue;   // GGMAX 2.24: proxies need the type's material
			byType[ type ].push_back( p );
			validCount++;
		}
		if ( validCount == 0 ) return;

		wi::ecs::Entity meshEntity = wi::ecs::CreateEntity();
		wi::scene::MeshComponent& mesh = scene.meshes.Create( meshEntity );
		mesh.vertex_positions.reserve( validCount * 8 );
		mesh.vertex_normals  .reserve( validCount * 8 );
		mesh.vertex_uvset_0  .reserve( validCount * 8 );
		mesh.indices         .reserve( validCount * 12 );

		// One quad = 4 verts / 6 indices; uv v=0 at the top of the billboard.
		auto emitQuad = [&]( const XMFLOAT3& bl, const XMFLOAT3& br,
			const XMFLOAT3& tr, const XMFLOAT3& tl, const XMFLOAT3& n )
		{
			uint32_t base = (uint32_t)mesh.vertex_positions.size();
			mesh.vertex_positions.push_back( bl ); mesh.vertex_uvset_0.push_back( XMFLOAT2( 0, 1 ) );
			mesh.vertex_positions.push_back( br ); mesh.vertex_uvset_0.push_back( XMFLOAT2( 1, 1 ) );
			mesh.vertex_positions.push_back( tr ); mesh.vertex_uvset_0.push_back( XMFLOAT2( 1, 0 ) );
			mesh.vertex_positions.push_back( tl ); mesh.vertex_uvset_0.push_back( XMFLOAT2( 0, 0 ) );
			for ( int v = 0; v < 4; v++ ) mesh.vertex_normals.push_back( n );
			mesh.indices.push_back( base + 0 ); mesh.indices.push_back( base + 1 ); mesh.indices.push_back( base + 2 );
			mesh.indices.push_back( base + 0 ); mesh.indices.push_back( base + 2 ); mesh.indices.push_back( base + 3 );
		};

		for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
		{
			if ( byType[ t ].empty() ) continue;
			if ( g_shadowProxyMaterial[ t ] == wi::ecs::INVALID_ENTITY )
				g_shadowProxyMaterial[ t ] = BuildBillboardShadowMaterial( t );

			uint32_t indexOffset = (uint32_t)mesh.indices.size();
			for ( InstanceTree* p : byType[ t ] )
			{
				const float h = g_GGTrees[ t ].height * p->GetScaleFloat();
				const float halfW = 0.5f * g_GGTrees[ t ].billboardScaleX * h;
				const float x = p->x, y = p->y, z = p->z;
				emitQuad( XMFLOAT3( x - halfW, y, z ), XMFLOAT3( x + halfW, y, z ),
				          XMFLOAT3( x + halfW, y + h, z ), XMFLOAT3( x - halfW, y + h, z ),
				          XMFLOAT3( 0, 0, 1 ) );
				emitQuad( XMFLOAT3( x, y, z - halfW ), XMFLOAT3( x, y, z + halfW ),
				          XMFLOAT3( x, y + h, z + halfW ), XMFLOAT3( x, y + h, z - halfW ),
				          XMFLOAT3( 1, 0, 0 ) );
			}

			wi::scene::MeshComponent::MeshSubset subset;
			subset.materialID  = g_shadowProxyMaterial[ t ];
			subset.indexOffset = indexOffset;
			subset.indexCount  = (uint32_t)mesh.indices.size() - indexOffset;
			mesh.subsets.push_back( subset );
		}

		mesh.CreateRenderData();

		wi::ecs::Entity objEntity = wi::ecs::CreateEntity();
		wi::scene::ObjectComponent& obj = scene.objects.Create( objEntity );
		obj.meshID = meshEntity;
		obj.SetRenderable( true );
		obj.SetCastShadow( true );
		obj.SetNotVisibleInMainCamera( true );
		obj.SetNotVisibleInReflections( true );
		obj.SetOcclusionQueryDisabled( true );
		obj.cascadeMask = TreeProxyCascadeMask();  // tree_shadow_range slider: how many cascades get tree shadows
		scene.transforms.Create( objEntity );  // identity — verts are world-space

		g_chunkProxyMesh  [ c ] = meshEntity;
		g_chunkProxyObject[ c ] = objEntity;
	}
}

void GGTrees_WickedUpdate()
{
	// GGMAX 2.94: "Trees Off" brutal off-switch. Placed at the very top so it is BOTH arms of
	// the switch in one guard:
	//   - set before the first update after a load, GrowTreePool / RebuildTreeGrid /
	//     EnsureTreeType / BuildShadowProxyChunk are never reached, so not one tree entity,
	//     mesh, material or GPU buffer is ever created;
	//   - set on an already-loaded level, ReleaseTreePool tears the whole lot out now instead
	//     of after the 600-frame park delay.
	// ReleaseTreePool is the right primitive and is already proven: it removes every pool
	// entity, every shadow proxy + billboard material, and all per-type LOD assets.
	// NOT GGTrees_HideAll() - that walks all 400,000 instances writing bit 0 of the instance
	// data word, which is LEVEL DATA, and a later save would persist the hide.
	{
		if ( ::gg_no_trees )
		{
			auto& scene = wi::scene::GetScene();
			// widened past the stock `g_treePoolBuilt > 0` guard: proxies can outlive the pool
			if ( g_treePoolBuilt > 0 || !g_chunkProxyObject.empty() )
				ReleaseTreePool( scene );
			return;
		}
	}

	if ( !g_wickedTreesSetup )
	{
		GGTrees_WickedSetup();
		if ( !g_wickedTreesSetup ) return;
	}

	auto& scene = wi::scene::GetScene();

	// Respect the GG-side visibility flags (parity fix: GGTrees_Update hides
	// trees during terrain regen via hide_until_update/draw_enabled, and the
	// pool used to ignore that and keep stale trees on screen). While hidden,
	// park the pool; on re-show force a full rebind because the instance data
	// (esp. Y heights) was regenerated while we were parked.
	static bool s_poolParked = false;
	static uint32_t s_parkedFrames = 0;
	bool forceRebind = false;
	if ( !ggtrees_global_params.draw_enabled )
	{
		if ( !s_poolParked )
		{
			for ( uint32_t i = 0; i < g_treePoolBuilt; i++ )
			{
				wi::scene::ObjectComponent* obj = scene.objects.GetComponent( g_treePoolEntities[ i ] );
				if ( obj ) obj->SetRenderable( false );
			}
			GGTrees_SetShadowProxiesVisible( scene, false );
			s_poolParked = true;
			s_parkedFrames = 0;
		}
		// GGMAX 2.23 (2026-08-11): RELEASE THE POOL AFTER A SUSTAINED PARK.
		//
		// THE BUG THIS FIXES, measured: load Island Showdown (tree level), return to the hub,
		// load Switch Escape (no trees) and the treeless level carried +6079 objects and +6079
		// transforms versus loading it first in a clean session. DUMP_TREEPOOL named it:
		// "POOL built=6000 bound=6000 renderable=0". Invisible (POLYS identical) but walked by
		// Scene::Update every frame. `bound=6000` is the giveaway that we got here: Pass A
		// evicts stale bindings, so if bindings survived, Pass A never ran — i.e. the update
		// took THIS early return and the pool was hidden but never released.
		//
		// It is a consequence of 2.19's deliberately one-way GrowTreePool. One-way is right for
		// camera movement (no create/destroy thrash as you walk away from trees) and wrong for
		// level changes, where the pool should go back to nothing. NOT a regression — before
		// 2.19 the pool was 6000 on every level unconditionally — but without this the lazy-pool
		// saving only survives until the session's first tree level.
		//
		// WHY A FRAME THRESHOLD RATHER THAN A LEVEL-CHANGE HOOK: draw_enabled is also cleared
		// transiently while GGTrees_Update regenerates terrain, and there is no single reliable
		// "level changed" signal here. A sustained park means the trees are gone for good; a
		// regen park is short. The threshold only has to separate those two, and releasing a
		// few seconds late on a treeless level costs nothing — the slots are already invisible.
		// Regrowth is already handled: unpark below sets forceRebind, and GrowTreePool rebuilds
		// on demand under its 2048/frame cap.
		if ( g_treePoolBuilt > 0 && ++s_parkedFrames >= GG_TREE_POOL_PARK_RELEASE_FRAMES )
		{
			ReleaseTreePool( scene );
			s_parkedFrames = 0;
		}
		return;
	}
	if ( s_poolParked )
	{
		s_poolParked = false;
		s_parkedFrames = 0;
		forceRebind = true;
		GGTrees_SetShadowProxiesVisible( scene, true );
	}

	// Far-shadow proxy upkeep: chunk rebuilds are deferred ~0.5s after the last
	// tree-data change so a paint stroke batches, then consumed incrementally
	// under a small per-frame time budget (only DIRTY chunks rebuild — a brush
	// swath touches a handful; terrain regen / level load dirties all 256).
	bool forceRescan = false;
	{
		static uint32_t s_proxyStamp = ~0u;
		static int      s_proxyCountdown = -1;
		static int      s_lastDrawShadows = -1;
		if ( s_lastDrawShadows != ggtrees_global_params.draw_shadows )
		{
			s_lastDrawShadows = ggtrees_global_params.draw_shadows;
			forceRebind = true;   // refresh per-slot SetCastShadow via rebind
			GGTrees_MarkAllProxyChunksDirty();
			s_proxyCountdown = 1;
		}
		// ★ GGMAX 3.73: the dirty-array check is OUTSIDE the stamp branch.
		// It was inside it, and s_proxyStamp is a function static that survives
		// GGTrees_WickedShutdown - so after a shutdown/re-setup inside one session the stamp
		// still matched, the branch never ran, and the proxy array was never re-sized. Measured
		// on a Terrain Bake on/off cycle: proxyChunks 256 -> 0 -> 0, SCENE_OBJECTS 1410 -> 1172,
		// i.e. the far-tree shadow proxies never came back until the level was reloaded.
		// Cheap to hoist: assign() only runs when the size genuinely disagrees.
		if ( g_chunkProxyDirty.size() != (size_t)numTreeChunks )
		{
			GGTrees_MarkAllProxyChunksDirty();
			if ( s_proxyCountdown < 0 ) s_proxyCountdown = 30;
		}
		if ( s_proxyStamp != g_treeInstanceStamp )
		{
			s_proxyStamp = g_treeInstanceStamp;
			s_proxyCountdown = 30;
		}

		// "Tree Shadow LOD Distance" slider (Terrain Tools debug panel): a
		// changed radius re-evaluates each tree's mesh-shadow flag on the next
		// rescan — no full rebind needed, Pass A applies flag deltas in place.
		static float s_lastLodDistShadow = -1.0f;
		if ( s_lastLodDistShadow != ggtrees_global_params.lod_dist_shadow )
		{
			s_lastLodDistShadow = ggtrees_global_params.lod_dist_shadow;
			forceRescan = true;
			// GGMAX 3.108b: every pool mesh carries the slider value as its shadow fade start (the
			// engine dissolves the shadow from there). Cheap component writes, like the range below.
			for ( uint32_t i = 0; i < g_treePoolBuilt; i++ )
			{
				wi::scene::ObjectComponent* obj = scene.objects.GetComponent( g_treePoolEntities[ i ] );
				if ( obj ) obj->gg_shadow_fade_start = ggtrees_global_params.lod_dist_shadow;
			}
		}
		// GGMAX 3.108b: the per-tree cast threshold depends on the engine's shadow fade lever and the
		// mesh fade (see the selection pass), so a change to either must re-threshold on the same
		// frame - otherwise a parked camera keeps the old cast set for up to the 256-frame heartbeat.
		static int s_lastShadowFadeBand = -1;
		const int shadowFadeBandNow = ( wi::renderer::gg_shadow_distance_fade && gg_tree_mesh_fade_dist != 0.0f ) ? 1 : 0;
		if ( s_lastShadowFadeBand != shadowFadeBandNow )
		{
			s_lastShadowFadeBand = shadowFadeBandNow;
			forceRescan = true;
		}

		// "Tree Shadow Range" slider: cascade reach. Existing proxies + pool
		// meshes get their cascadeMask updated live (cheap component writes);
		// range 0 <-> nonzero also needs proxies torn down/rebuilt and the
		// per-slot cast flags refreshed (DX11 semantics: range 0 = no tree
		// shadows at all).
		static int s_lastShadowRange = -999;
		if ( s_lastShadowRange != ggtrees_global_params.tree_shadow_range )
		{
			s_lastShadowRange = ggtrees_global_params.tree_shadow_range;
			const uint32_t proxyMask = TreeProxyCascadeMask();
			bool anyProxy = false;
			for ( wi::ecs::Entity e : g_chunkProxyObject )
			{
				wi::scene::ObjectComponent* obj = scene.objects.GetComponent( e );
				if ( obj ) { obj->cascadeMask = proxyMask; anyProxy = true; }
			}
			const uint32_t meshMask = TreeMeshCascadeMask();
			for ( uint32_t i = 0; i < g_treePoolBuilt; i++ )
			{
				wi::scene::ObjectComponent* obj = scene.objects.GetComponent( g_treePoolEntities[ i ] );
				if ( obj ) obj->cascadeMask = meshMask;
			}
			if ( !TreeShadowsEnabled() || !anyProxy )
			{
				// tear down (range 0) or build from nothing (came from range 0)
				GGTrees_MarkAllProxyChunksDirty();
				s_proxyCountdown = 1;
			}
			forceRescan = true;       // re-apply cast flags (range 0 <-> nonzero)
		}

		// Consume dirty chunks once the batching countdown has expired.
		if ( s_proxyCountdown >= 0 ) --s_proxyCountdown;
		if ( s_proxyCountdown < 0 && g_chunkProxyDirty.size() == (size_t)numTreeChunks )
		{
			if ( g_chunkProxyMesh.size() != (size_t)numTreeChunks )
			{
				g_chunkProxyMesh  .assign( numTreeChunks, wi::ecs::INVALID_ENTITY );
				g_chunkProxyObject.assign( numTreeChunks, wi::ecs::INVALID_ENTITY );
			}
			wi::Timer budget;
			for ( uint32_t c = 0; c < numTreeChunks; c++ )
			{
				if ( !g_chunkProxyDirty[ c ] ) continue;
				g_chunkProxyDirty[ c ] = 0;
				GGTrees_BuildShadowProxyChunk( c );
				if ( budget.elapsed_milliseconds() > 3.0 ) break;   // resume next frame
			}
		}
	}

	// (Re)size per-tree state to the live instance array. A count change means
	// a level load / bulk tree edit — all bets on existing bindings are off.
	// Slot-vector size mismatch = first run or post-shutdown re-setup (fresh
	// pool entities, stale bindings).
	if ( g_treeToSlot.size() != (size_t)numTotalTrees ) forceRebind = true;
	if ( g_slotToTree.size() != (size_t)g_treePoolBuilt ) forceRebind = true;
	if ( forceRebind )
	{
		g_treeToSlot      .assign( numTotalTrees, -1 );
		g_treeDesiredEpoch.assign( numTotalTrees, 0 );
		g_treeBand        .assign( numTotalTrees, 0 );
		g_treeShadow      .assign( numTotalTrees, 0 );
		g_slotToTree      .assign( g_treePoolBuilt, UINT32_MAX );
		g_slotCache       .resize( g_treePoolBuilt );
		for ( uint32_t i = 0; i < g_treePoolBuilt; i++ )
		{
			wi::scene::ObjectComponent* obj = scene.objects.GetComponent( g_treePoolEntities[ i ] );
			if ( obj ) obj->SetRenderable( false );
		}
	}

	// Camera position — used to pick the nearest N trees to spend the pool on.
	// XZ-only distance is sufficient: trees are all ~ground-level, and vertical
	// distance would distort priority when the camera flies above the terrain.
	const wi::scene::CameraComponent& camera = wi::scene::GetCamera();
	const float camX = camera.Eye.x;
	const float camZ = camera.Eye.z;

	// GGMAX 2.95: FAR-TREE BILLBOARD GATE. Runs EVERY frame - it sits above the selection
	// throttle below, which early-returns on a parked camera, and visibility has to track the
	// camera even when the nearest-N set does not need recomputing.
	//
	// Show a chunk's merged billboards to the main camera only when the ENTIRE chunk lies
	// beyond the radius the real tree meshes reach. Conservative on purpose: the proxy holds a
	// billboard for EVERY tree in its chunk, including ones the pool is already drawing as real
	// meshes, so any chunk that straddles the cutoff would double-draw them. Chunks are
	// treeArea/treeSplit = 12500 units (~317 m) so the straddling ring is one chunk wide.
	{
		auto& sceneFT = wi::scene::GetScene();
		const float chunkSpan = treeArea / (float)treeSplit;
		uint32_t shownFT = 0, validFT = 0;
		float nearestFT = 1e30f, farthestFT = 0.0f;
		for ( uint32_t c = 0; c < (uint32_t)g_chunkProxyObject.size(); c++ )
		{
			wi::scene::ObjectComponent* obj = sceneFT.objects.GetComponent( g_chunkProxyObject[ c ] );
			if ( obj == nullptr ) continue;
			bool show = false;
			if ( gg_trees_far_billboards && g_treePoolCutoffDist2 >= 0.0f )
			{
				const uint32_t iX = c % treeSplit, iZ = c / treeSplit;
				const float minX = ( (float)iX / (float)treeSplit - 0.5f ) * treeArea;
				const float minZ = ( (float)iZ / (float)treeSplit - 0.5f ) * treeArea;
				// nearest point of the chunk box to the camera, XZ only (same basis as the pool)
				const float dx = std::max( 0.0f, std::max( minX - camX, camX - ( minX + chunkSpan ) ) );
				const float dz = std::max( 0.0f, std::max( minZ - camZ, camZ - ( minZ + chunkSpan ) ) );
				show = ( dx * dx + dz * dz ) >= g_treePoolCutoffDist2;
			}
			obj->SetNotVisibleInMainCamera( !show );
			if ( show ) shownFT++;
			validFT++;
			{
				const uint32_t iXd = c % treeSplit, iZd = c / treeSplit;
				const float mnX = ( (float)iXd / (float)treeSplit - 0.5f ) * treeArea;
				const float mnZ = ( (float)iZd / (float)treeSplit - 0.5f ) * treeArea;
				const float ddx = std::max( 0.0f, std::max( mnX - camX, camX - ( mnX + chunkSpan ) ) );
				const float ddz = std::max( 0.0f, std::max( mnZ - camZ, camZ - ( mnZ + chunkSpan ) ) );
				const float nd = sqrtf( ddx * ddx + ddz * ddz );
				if ( nd < nearestFT ) nearestFT = nd;
				if ( nd > farthestFT ) farthestFT = nd;
			}
		}
		g_ftProxiesShown = shownFT;
		g_ftProxyCount   = (uint32_t)g_chunkProxyObject.size();
		g_ftProxyValid   = validFT;
		g_ftNearest      = ( validFT > 0 ) ? nearestFT : -1.0f;
		g_ftFarthest     = ( validFT > 0 ) ? farthestFT : -1.0f;
	}

	// Selection throttle: the 400K candidate scan + nth_element + bind passes
	// only need to re-run when the answer can change — camera moved (>8 inches,
	// accumulated against the last SCANNED position so slow drift still lands),
	// tree data rebuilt (g_treeInstanceStamp bumps in GGTrees_UpdateInstances),
	// pool state reset, or a slow safety heartbeat for any mutation path that
	// doesn't announce itself. With a still camera this makes the whole pool
	// update near-free.
	// benchmark hook: emulates a camera-move rescan (no slot-data verify)
	bool stressRescan = false;
	if ( g_treePoolStressFrames > 0 ) { g_treePoolStressFrames--; stressRescan = true; }

	static float    s_lastCamX = 1e30f, s_lastCamZ = 1e30f;  // far sentinel -> first frame always scans
	static uint32_t s_lastInstanceStamp = ~0u;
	static uint32_t s_heartbeat = 0;
	// Pure camera-move rescans can trust the slot caches: every data mutation
	// announces itself via the stamp (or is swept up by the heartbeat), so the
	// expensive per-slot source-data verify in Pass A only runs on those
	// triggers — a moving camera pays for band/eviction bookkeeping only.
	bool verifySlotData = false;
	{
		const float mdx = camX - s_lastCamX;
		const float mdz = camZ - s_lastCamZ;
		const bool heartbeat = ++s_heartbeat >= 256;
		const bool stampChanged = s_lastInstanceStamp != g_treeInstanceStamp;
		const bool rescan = forceRebind || forceRescan || stampChanged || stressRescan
			|| ( mdx * mdx + mdz * mdz ) > ( 8.0f * 8.0f )
			|| heartbeat;
		if ( !rescan ) return;
		verifySlotData = forceRebind || forceRescan || stampChanged || heartbeat;
		s_heartbeat = 0;
		s_lastCamX = camX;
		s_lastCamZ = camZ;
		s_lastInstanceStamp = g_treeInstanceStamp;
		// Heartbeat = "a mutation may have happened without announcing itself"
		// (direct writes that bypass the stamped setters). The grid must not be
		// trusted either, or a silent position change would poison it forever.
		if ( heartbeat ) g_gridValid = false;
	}

	// Keep the spatial grid in step with the instance data.
	if ( !g_gridValid || g_gridStamp != g_treeInstanceStamp )
	{
		auto rangeGrid = wi::profiler::BeginRangeCPU( "TreePool - GridRebuild" );
		RebuildTreeGrid();
		g_gridStamp = g_treeInstanceStamp;
		g_gridValid = true;
		wi::profiler::EndRange( rangeGrid );
	}
	auto rangeGather = wi::profiler::BeginRangeCPU( "TreePool - Gather" );

	// Candidate list — index into pAllTrees + squared XZ distance to camera.
	// Static + clear() keeps the allocated capacity across frames.
	struct TreeCandidate { uint32_t idx; float dist2; };
	static std::vector<TreeCandidate> candidates;
	candidates.clear();

	// Ring-gather cells around the camera (Chebyshev rings). The pool fills at
	// ring r0; one padding ring beyond that and we stop. This is approximate at
	// the far rim (an exact nearest-N superset needs sqrt2*(r0+1) rings — twice
	// the gather area): a corner-cell tree can lose its pool place to a
	// slightly-farther gathered one. Those rim trees are LOD2 at ~6000+ inches
	// — subpixel — and the error is bounded by one cell (1562"). Measured win:
	// candidates (and the nth_element input) drop by ~2x vs the exact rule.
	{
		const int dim = (int)GG_TREE_GRID_DIM;
		const int ccx = TreeGridCoord( camX );
		const int ccz = TreeGridCoord( camZ );
		const uint32_t totalInGrid = g_gridCellStart[ (size_t)dim * dim ];

		auto gatherCell = [&]( int cx, int cz )
		{
			if ( cx < 0 || cx >= dim || cz < 0 || cz >= dim ) return;
			const uint32_t c = (uint32_t)cz * GG_TREE_GRID_DIM + (uint32_t)cx;
			const uint32_t end = g_gridCellStart[ c + 1 ];
			for ( uint32_t k = g_gridCellStart[ c ]; k < end; k++ )
			{
				const TreeGridEntry& e = g_gridEntries[ k ];
				const float dx = e.x - camX;
				const float dz = e.z - camZ;
				const float d2 = dx * dx + dz * dz;
				if ( gg_ftCapDist2 > 0.0f && d2 > gg_ftCapDist2 ) continue;   // GGMAX 2.97
				candidates.push_back( { e.idx, d2 } );
			}
		};

		// GGMAX 2.97: distance cap + a ring bound derived from it, so a sparse level cannot make
		// the gather walk the whole 128x128 grid looking for trees it will never accept.
		float capDist = gg_tree_pool_max_dist;
		if ( capDist < 0.0f ) capDist = ggtrees_global_params.lod_dist + 2.0f * GG_TREE_LOD_TRANSITION;
		const bool  capOn   = ( capDist > 0.0f );
		const float capDist2 = capDist * capDist;
		gg_ftCapDist2 = capOn ? capDist2 : 0.0f;
		const float cellSize = treeArea / (float)GG_TREE_GRID_DIM;
		const int   maxRing  = capOn ? ( (int)( capDist / cellSize ) + 2 ) : ( dim * 2 );

		int coveredRing = -1;   // first ring where the pool count was reached
		for ( int r = 0; r < dim * 2; r++ )
		{
			if ( r > maxRing ) break;   // GGMAX 2.97: nothing beyond here can be inside the cap
			if ( r == 0 )
			{
				gatherCell( ccx, ccz );
			}
			else
			{
				for ( int x = ccx - r; x <= ccx + r; x++ )
				{
					gatherCell( x, ccz - r );
					gatherCell( x, ccz + r );
				}
				for ( int z = ccz - r + 1; z <= ccz + r - 1; z++ )
				{
					gatherCell( ccx - r, z );
					gatherCell( ccx + r, z );
				}
			}
			if ( candidates.size() >= (size_t)totalInGrid ) break;   // grid exhausted
			if ( coveredRing < 0 )
			{
				if ( candidates.size() >= (size_t)g_treePoolSize )
					coveredRing = r;
			}
			else if ( r > coveredRing )
			{
				break;   // one padding ring gathered beyond the fill ring
			}
		}
	}

	wi::profiler::EndRange( rangeGather );
	g_ftCandidates = (uint32_t)candidates.size();   // GGMAX 2.95 diagnostics
	auto rangeSelect = wi::profiler::BeginRangeCPU( "TreePool - SelectMark" );

	// Partial-sort: put the N closest at the front, don't care about the rest.
	// nth_element is O(N) on average — much cheaper than a full sort.
	size_t poolFill = candidates.size();
	if ( poolFill > g_treePoolSize )
	{
		std::nth_element(
			candidates.begin(),
			candidates.begin() + g_treePoolSize,
			candidates.end(),
			[]( const TreeCandidate& a, const TreeCandidate& b ) { return a.dist2 < b.dist2; } );
		poolFill = g_treePoolSize;
		// GGMAX 2.95: after nth_element the element AT g_treePoolSize is in its sorted place and
		// everything before it is <=, so this IS the exact radius the real tree meshes reach.
		// Beyond it the pool draws nothing, which is precisely where the billboards must take over.
		g_treePoolCutoffDist2 = candidates[ g_treePoolSize ].dist2;
	}
	else
	{
		// Pool is not full: every tree in range is a real mesh, so there is nothing for the
		// billboards to cover. -1 means "no cutoff" and keeps them hidden.
		g_treePoolCutoffDist2 = -1.0f;
	}

	// GGMAX 2.19: poolFill is how many slots this frame actually WANTS, so it is the demand
	// signal for lazy growth. A treeless level never gets here with poolFill > 0 and so never
	// creates a single pool entity; a tree level converges to the same pool it always had,
	// spread over a few frames by the per-frame cap. Growth is one-way for the pool's lifetime —
	// slots are only released by GGTrees_WickedShutdown — so a camera that walks away from the
	// trees does not thrash entity create/destroy.
	GrowTreePool( scene, (uint32_t)poolFill );

	// Mark this frame's desired set + LOD band + mesh-shadow flag per tree.
	constexpr float lod1Dist2 = GG_TREE_LOD1_DIST * GG_TREE_LOD1_DIST;
	constexpr float lod2Dist2 = GG_TREE_LOD2_DIST * GG_TREE_LOD2_DIST;
	// GGMAX 3.108b: with the shadow distance fade on, the engine dissolves each mesh shadow over
	// [slider, slider + tree radius] (gg_shadow_fade_start), so the per-tree cast flag must not cut it
	// at the slider any more - it moves out past the end of that band, where the shadow has already
	// gone (the engine also stops submitting a fully dissolved caster, so the extra trees cost only a
	// cull test). The band must cover the BIGGEST tree: western pine / drypine reach an AABB radius of
	// ~1134 at the maximum scale 1.994 (review measured the TreeMeshes data), so 1300 = 1.15x that.
	// Fade off, or the harness mesh fade off (SET_TREEMESHFADE 0 - the engine cannot shift off a
	// 1e30 fade distance): the old hard switch at the slider.
	constexpr float GG_TREE_SHADOW_FADE_BAND = 1300.0f;
	const bool  shadowFadeBand = wi::renderer::gg_shadow_distance_fade && gg_tree_mesh_fade_dist != 0.0f;
	const float shadowDist  = ggtrees_global_params.lod_dist_shadow
		+ ( shadowFadeBand ? GG_TREE_SHADOW_FADE_BAND : 0.0f );   // "Tree Shadow LOD Distance" slider
	const float shadowDist2 = shadowDist * shadowDist;
	g_treeEpoch++;
	for ( size_t k = 0; k < poolFill; k++ )
	{
		const uint32_t idx = candidates[ k ].idx;
		const float d2 = candidates[ k ].dist2;
		g_treeDesiredEpoch[ idx ] = g_treeEpoch;
		g_treeBand[ idx ] = ( d2 < lod1Dist2 ) ? 0 : ( d2 < lod2Dist2 ) ? 1 : 2;
		g_treeShadow[ idx ] = ( d2 < shadowDist2 ) ? 1 : 0;
	}

	wi::profiler::EndRange( rangeSelect );
	auto rangePass = wi::profiler::BeginRangeCPU( "TreePool - BindPasses" );

	// Pass A over slots: evict bindings that fell out of the nearest-N set,
	// keep (and verify) the rest. Kept slots cost a few array reads + float
	// compares — no component writes unless the tree's data or band changed.
	static std::vector<uint32_t> freeSlots;
	if ( freeSlots.capacity() < g_treePoolBuilt ) freeSlots.reserve( g_treePoolBuilt );
	freeSlots.clear();

	for ( uint32_t s = 0; s < g_treePoolBuilt; s++ )
	{
		const uint32_t t = g_slotToTree[ s ];
		if ( t == UINT32_MAX ) { freeSlots.push_back( s ); continue; }

		bool evict = ( t >= numTotalTrees || g_treeDesiredEpoch[ t ] != g_treeEpoch );
		if ( !evict )
		{
			TreeSlotCache& c = g_slotCache[ s ];
			const uint8_t band = g_treeBand[ t ];
			bool dataChanged = false;
			if ( verifySlotData )
			{
				// Verify the source data hasn't changed under us (editor tree
				// move/retype/rescale mutates pAllTrees in place). Skipped on
				// pure camera-move rescans — this deref is a cache miss into
				// the 8MB instance array per kept slot.
				InstanceTree* pTree = &pAllTrees[ t ];
				const float scale = pTree->GetScaleFloat();
				dataChanged = ( c.x != pTree->x || c.y != pTree->y || c.z != pTree->z ||
				                c.scale != scale || c.type != (uint8_t)pTree->GetType() );
			}
			if ( dataChanged )
			{
				evict = !BindTreeSlot( scene, s, t, band );  // full rebind in place
			}
			else if ( c.band != band || c.shadow != g_treeShadow[ t ] )
			{
				// LOD boundary or mesh-shadow boundary crossed — mesh/flag swap
				// only, transform unchanged.
				wi::scene::ObjectComponent* obj = scene.objects.GetComponent( g_treePoolEntities[ s ] );
				if ( obj )
				{
					obj->meshID = TreeMeshForBand( c.type, band );
					obj->SetCastShadow( TreeShadowsEnabled() && g_treeShadow[ t ] != 0 );
					c.band = band;
					c.shadow = g_treeShadow[ t ];
				}
			}
		}
		if ( evict )
		{
			wi::scene::ObjectComponent* obj = scene.objects.GetComponent( g_treePoolEntities[ s ] );
			if ( obj ) obj->SetRenderable( false );
			if ( t < numTotalTrees && g_treeToSlot[ t ] == (int32_t)s ) g_treeToSlot[ t ] = -1;
			g_slotToTree[ s ] = UINT32_MAX;
			freeSlots.push_back( s );
		}
	}

	// Pass B over the desired set: bind whatever isn't already in a slot.
	for ( size_t k = 0; k < poolFill && !freeSlots.empty(); k++ )
	{
		const uint32_t idx = candidates[ k ].idx;
		if ( g_treeToSlot[ idx ] >= 0 ) continue;  // kept from a previous frame
		const uint32_t s = freeSlots.back();
		if ( BindTreeSlot( scene, s, idx, g_treeBand[ idx ] ) )
			freeSlots.pop_back();
	}
	wi::profiler::EndRange( rangePass );
}

// DIAG (2026-07-25 reload-corruption hunt): dump the pool's view of the world + an orphan
// detector. An "orphan" = a renderable, UNNAMED scene object that is NOT one of the pool
// entities, NOT a shadow-proxy object, and NOT a grass/chunk child — after an in-place level
// reload these are the leftover tree objects nobody manages anymore (the "blue palms").
void GGTrees_DebugDumpPool( const char* path )
{
	auto& scene = wi::scene::GetScene();
	FILE* f = fopen( path, "w" );
	if ( !f ) return;

	uint32_t slotBound = 0, slotRenderable = 0, slotMissingObj = 0;
	for ( uint32_t s = 0; s < g_treePoolBuilt; s++ )
	{
		wi::ecs::Entity e = g_treePoolEntities[ s ];
		if ( e == wi::ecs::INVALID_ENTITY ) continue;
		wi::scene::ObjectComponent* obj = scene.objects.GetComponent( e );
		if ( !obj ) { slotMissingObj++; continue; }
		if ( s < g_slotToTree.size() && g_slotToTree[ s ] != UINT32_MAX ) slotBound++;
		if ( obj->IsRenderable() ) slotRenderable++;
	}
	// GGMAX 2.19: report built AND ceiling. "size" alone used to be the whole story; with lazy
	// growth `built` is the number that costs per-frame ECS, and built=0 on a treeless level is
	// the expected, correct reading — not a broken pool.
	fprintf( f, "POOL built=%u ceiling=%u bound=%u renderable=%u missingObj=%u numTotalTrees=%u types=%u proxyChunks=%zu\n",
		g_treePoolBuilt, g_treePoolSize, slotBound, slotRenderable, slotMissingObj, (uint32_t)numTotalTrees,
		(uint32_t)GG_TREE_TYPES, g_chunkProxyObject.size() );

	// Build a fast lookup of every entity the tree system currently OWNS.
	static std::vector<wi::ecs::Entity> owned;
	owned.clear();
	for ( uint32_t s = 0; s < g_treePoolBuilt; s++ )
		if ( g_treePoolEntities[ s ] != wi::ecs::INVALID_ENTITY ) owned.push_back( g_treePoolEntities[ s ] );
	for ( wi::ecs::Entity e : g_chunkProxyObject ) if ( e != wi::ecs::INVALID_ENTITY ) owned.push_back( e );
	std::sort( owned.begin(), owned.end() );

	// Orphan scan: renderable, mesh-bound, UNNAMED, parent-less objects not owned by the
	// tree system. (Level entities, chunks, grass and props all carry names or hierarchy
	// parents; pool trees are unnamed + parent-less by construction.)
	int orphans = 0;
	for ( size_t i = 0; i < scene.objects.GetCount(); ++i )
	{
		wi::ecs::Entity e = scene.objects.GetEntity( i );
		const wi::scene::ObjectComponent& obj = scene.objects[ i ];
		if ( obj.meshID == wi::ecs::INVALID_ENTITY || !obj.IsRenderable() ) continue;
		if ( std::binary_search( owned.begin(), owned.end(), e ) ) continue;
		if ( scene.names.GetComponent( e ) != nullptr ) continue;
		if ( scene.hierarchy.GetComponent( e ) != nullptr ) continue;
		const wi::scene::NameComponent* mn = scene.names.GetComponent( obj.meshID );
		const wi::scene::TransformComponent* tr = scene.transforms.GetComponent( e );
		orphans++;
		if ( orphans <= 200 )
			fprintf( f, "ORPHAN entity=%llu meshID=%llu meshname=\"%s\" pos=(%.0f,%.0f,%.0f)\n",
				(unsigned long long)e, (unsigned long long)obj.meshID, mn ? mn->name.c_str() : "?",
				tr ? tr->world._41 : 0.0f, tr ? tr->world._42 : 0.0f, tr ? tr->world._43 : 0.0f );
	}
	fprintf( f, "ORPHAN_TOTAL %d\n", orphans );
	fclose( f );
}

void GGTrees_WickedShutdown()
{
	if ( !g_wickedTreesSetup ) return;
	auto& scene = wi::scene::GetScene();

	for ( uint32_t i = 0; i < g_treePoolBuilt; i++ )
	{
		wi::ecs::Entity e = g_treePoolEntities[ i ];
		if ( e != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( e );
		g_treePoolEntities[ i ] = wi::ecs::INVALID_ENTITY;
	}
	// GGMAX 2.19: the pool is gone, so the built count must go with it or the next setup would
	// iterate slots whose entities were just removed.
	g_treePoolBuilt = 0;
	// Bindings refer to pool entities that no longer exist — drop them so the
	// next setup's update pass starts with a force-rebind.
	g_slotToTree.clear();
	g_treeToSlot.clear();
	g_treeDesiredEpoch.clear();
	g_treeBand.clear();
	g_treeShadow.clear();
	g_gridValid = false;   // instance data will be rebuilt for the next level

	// Shadow proxies + their materials go down with the pool.
	for ( wi::ecs::Entity e : g_chunkProxyObject ) if ( e != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( e );
	for ( wi::ecs::Entity e : g_chunkProxyMesh   ) if ( e != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( e );
	g_chunkProxyObject.clear();
	g_chunkProxyMesh.clear();
	g_chunkProxyDirty.clear();
	for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
	{
		if ( g_shadowProxyMaterial[ t ] != wi::ecs::INVALID_ENTITY )
		{
			scene.Entity_Remove( g_shadowProxyMaterial[ t ] );
			g_shadowProxyMaterial[ t ] = wi::ecs::INVALID_ENTITY;
		}
	}
	for ( uint32_t t = 0; t < GG_TREE_TYPES; t++ )
	{
		// LOD1/LOD2 slots may ALIAS a higher LOD's entity (missing variant) —
		// remove only distinct entities to avoid double Entity_Remove.
		if ( g_treeMeshEntityLOD2[ t ] != wi::ecs::INVALID_ENTITY && g_treeMeshEntityLOD2[ t ] != g_treeMeshEntityLOD1[ t ] && g_treeMeshEntityLOD2[ t ] != g_treeMeshEntity[ t ] )
			scene.Entity_Remove( g_treeMeshEntityLOD2[ t ] );
		if ( g_treeMeshEntityLOD1[ t ] != wi::ecs::INVALID_ENTITY && g_treeMeshEntityLOD1[ t ] != g_treeMeshEntity[ t ] )
			scene.Entity_Remove( g_treeMeshEntityLOD1[ t ] );
		if ( g_treeMeshEntity            [ t ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_treeMeshEntity            [ t ] );
		if ( g_treeTrunkMaterialEntity   [ t ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_treeTrunkMaterialEntity   [ t ] );
		if ( g_treeBranchesMaterialEntity[ t ] != wi::ecs::INVALID_ENTITY ) scene.Entity_Remove( g_treeBranchesMaterialEntity[ t ] );
		g_treeMeshEntity            [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeMeshEntityLOD1        [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeMeshEntityLOD2        [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeTrunkMaterialEntity   [ t ] = wi::ecs::INVALID_ENTITY;
		g_treeBranchesMaterialEntity[ t ] = wi::ecs::INVALID_ENTITY;
	}
	g_wickedTreesSetup = false;
}


// GGMAX 2.95: diagnostics for the far-tree billboard gate. Answers the only questions that
// matter when the gate does not fire: do the proxy chunks even exist, did the candidate gather
// find more trees than the pool can hold (if not there is no "far" to cover), and how many
// chunks are currently showing.
void GGTrees_GetFarTreeStats( int* proxyCount, int* proxiesShown, int* candidates,
                              int* poolBuilt, int* poolSize, float* cutoffDist2 )
{
	if ( proxyCount )   *proxyCount   = (int)g_ftProxyCount;
	if ( proxiesShown ) *proxiesShown = (int)g_ftProxiesShown;
	if ( candidates )   *candidates   = (int)g_ftCandidates;
	if ( poolBuilt )    *poolBuilt    = (int)g_treePoolBuilt;
	if ( poolSize )     *poolSize     = (int)g_treePoolSize;
	if ( cutoffDist2 )  *cutoffDist2  = g_treePoolCutoffDist2;
}

void GGTrees_GetFarTreeRange( int* validProxies, float* nearestChunk, float* farthestChunk )
{
	if ( validProxies )  *validProxies  = (int)g_ftProxyValid;
	if ( nearestChunk )  *nearestChunk  = g_ftNearest;
	if ( farthestChunk ) *farthestChunk = g_ftFarthest;
}

} // namespace GGTrees
