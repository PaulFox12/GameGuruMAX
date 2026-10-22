
#define ONLYLOADWHENUSED

#include "GGGrass.h"

#include <unordered_set>
#include <string>

#include "WickedEngine.h"
#include "wiRenderer.h"
#include "wiProfiler.h"
#include "wiInput.h"

#include "CFileC.h"
//#include "Utility/dds.h" // TODO: DX12 - tinyddsloader removed, rewrite DDS loading

#include "master.h"

// redefines MAX_PATH to 1050
#include "preprocessor-moreflags.h"
#include "gameguru.h"

#include "GGTerrainDataStructures.h"

#include "GGTerrain.h"
#include "M-UndoSys-Terrain.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#ifdef ONLYLOADWHENUSED
bool bGrassTextureUploaded[255];
bool bGrassTextureForceUploaded[255];
//extern std::string FinalGameGuruMaxFolder;
uint64_t last_paint_type;
#endif
using namespace GGTerrain;

using namespace wiGraphics;

extern int iTriggerGrassTreeUpdate;
extern bool bImGuiGotFocus;
extern int g_iCalculatingChangeBounds;
extern uint8_t* g_pTerrainSnapshot;
extern TerrainEditsBB g_EditBounds;
extern UndoRedoMemory g_TerrainUndoMem;
extern UndoRedoMemory g_TerrainRedoMem;
void GGGrass_CreateUndoRedoAction(int type, int eList, bool bUserAction, void* pEventData = nullptr);
void GGGrass_PerformUndoRedoAction(int type, void* pEventData, int eList);

int gggrass_initialised = 0;

namespace GGGrass
{

#define GGGRASS_MAP_SIZE 4096
#define GGGRASS_UNDOREDO

GGGrassParams gggrass_save_params;
GGGrassParams gggrass_global_params;
GGGrassInternalParams gggrass_internal_params;

struct VertexGrass
{
    float x, y;
};

VertexGrass g_VerticesGrass[ 4 ] = 
{
    { -0.5f, 1.0f },
	{ -0.5f, 0.0f },
	{  0.5f, 1.0f },
	{  0.5f, 0.0f }
};

uint16_t g_IndicesGrass[ 6 ] =
{
    0, 1, 2,
    2, 1, 3,
};

float grassRadius = GGGRASS_INITIAL_LOD_DIST + GGGRASS_LOD_TRANSITION;
const uint32_t numTotalGrass = 400000;

const uint32_t grassSplit = 8;
#if (grassSplit % 2) != 0
	#error grassSplit must be even
#endif

const uint32_t numGrassChunks = grassSplit * grassSplit;
const uint32_t numGrassPerChunk = numTotalGrass / numGrassChunks;
float grassAreaPerChunk = grassRadius * 2 / (grassSplit-2);

int gggrass_defer_instance_updates = 0; // see GGGrass_DeferInstanceUpdates
bool gggrass_instance_update_pending = false;

struct InstanceGrass
{
	float x, y, z;
	uint32_t data; // [0-7]=grass type, [8-15]=variation index, [16]=visible

	bool IsVisible() { return (data & 0x10000) != 0; }

	void SetVisible( int visible ) 
	{ 
		if ( visible ) data |= 0x10000;
		else data &= ~0x10000;
	}

	void SetType( uint32_t type )
	{
		if ( type >= GGGRASS_NUM_TYPES ) return;
		data = (data & ~0xFF) | type;
	}

	void SetData( uint32_t id, uint32_t type, int visible )
	{
		data = (type & 0xFF) | ((id & 0xFF) << 8);
		SetVisible( visible );
	}

	int GetType() { return data & 0xFF; }
};

struct GrassFile
{
	const char* filename;
	const char* shortname;
	float scaleFactor;
	uint32_t material; // 0 = any
};

#if GGGRASS_NUM_TYPES != 46
	#error Update the grassFiles array for new GGGRASS_NUM_TYPES value
#endif

GrassFile grassFiles[ GGGRASS_NUM_TYPES ] = 
{
	{ "course grass_mat1_SF_1.15.dds",  "Course Grass", 1.15f, 1 },   // 0
	{ "course grass_mat3_SF_1.15.dds",  "Course Grass", 1.15f, 3 },   // 1
	{ "course grass_mat9_SF_1.15.dds",  "Course Grass", 1.15f, 9 },   // 2
	{ "course grass_mat20_SF_1.15.dds", "Course Grass", 1.15f, 20 },  // 3
	{ "course grass_mat25_SF_1.15.dds", "Course Grass", 1.15f, 25 },  // 4
	{ "course grass_mat29_SF_1.15.dds", "Course Grass", 1.15f, 29 },  // 5
	{ "course grass_mat30_SF_1.15.dds", "Course Grass", 1.15f, 30 },  // 6
	{ "short grass_mat1_SF_1.4.dds",  "Short Grass", 1.4f, 1 },	      // 7
	{ "short grass_mat3_SF_1.4.dds",  "Short Grass", 1.4f, 3 },	      // 8
	{ "short grass_mat9_SF_1.4.dds",  "Short Grass", 1.4f, 9 },	      // 9
	{ "short grass_mat20_SF_1.4.dds", "Short Grass", 1.4f, 20 },	  // 10
	{ "short grass_mat25_SF_1.4.dds", "Short Grass", 1.4f, 25 },	  // 11
	{ "short grass_mat29_SF_1.4.dds", "Short Grass", 1.4f, 29 },	  // 12
	{ "short grass_mat30_SF_1.4.dds", "Short Grass", 1.4f, 30 },	  // 13
	{ "tall grass_mat1_SF_0.87.dds",  "Tall Grass", 0.87f, 1 },	      // 14
	{ "tall grass_mat3_SF_0.87.dds",  "Tall Grass", 0.87f, 3 },	      // 15
	{ "tall grass_mat9_SF_0.87.dds",  "Tall Grass", 0.87f, 9 },	      // 16
	{ "tall grass_mat20_SF_0.87.dds", "Tall Grass", 0.87f, 20 },	  // 17
	{ "tall grass_mat25_SF_0.87.dds", "Tall Grass", 0.87f, 25 },	  // 18
	{ "tall grass_mat29_SF_0.87.dds", "Tall Grass", 0.87f, 29 },	  // 19
	{ "tall grass_mat30_SF_0.87.dds", "Tall Grass", 0.87f, 30 },	  // 20
	{ "wild grass_mat1_SF_1.17.dds",  "Wild Grass", 1.17f, 1 },	      // 21
	{ "wild grass_mat3_SF_1.17.dds",  "Wild Grass", 1.17f, 3 },	      // 22
	{ "wild grass_mat9_SF_1.17.dds",  "Wild Grass", 1.17f, 9 },	      // 23
	{ "wild grass_mat20_SF_1.17.dds", "Wild Grass", 1.17f, 20 },      // 24
	{ "wild grass_mat25_SF_1.17.dds", "Wild Grass", 1.17f, 25 },      // 25
	{ "wild grass_mat29_SF_1.17.dds", "Wild Grass", 1.17f, 29 },      // 26
	{ "wild grass_mat30_SF_1.17.dds", "Wild Grass", 1.17f, 30 },      // 27
	{ "weed1_SF_0.6.dds",  "Wheat", 0.6f,  0 },                       // 28
	{ "weed2_SF_0.5.dds",  "Weed 1", 0.5f,  0 },                      // 29
	{ "weed3_SF_1.0.dds",  "Weed 2", 1.0f,  0 },                      // 30
	{ "weed4_SF_1.0.dds",  "Daisies", 1.0f,  0 },                     // 31
	{ "weed5_SF_0.43.dds", "Reed", 0.43f, 0 },                        // 32
	{ "weed6_SF_0.35.dds", "Weed 3", 0.35f, 0 },                      // 33
	{ "weed7_SF_1.0.dds",  "Weed 4", 1.0f,  0 },                      // 34
	{ "weed8_SF_1.0.dds",  "Weed 5", 1.0f,  0 },                      // 35
	{ "weed9_SF_1.0.dds",  "Weed 6", 1.0f,  0 },                      // 36
	{ "flower red_color_SF_0.5.dds",    "Red Flower", 0.5f, 0 },      // 37
	{ "flower white_color_SF_0.5.dds",  "White Flower", 0.5f, 0 },    // 38
	{ "flower yellow_color_SF_0.5.dds", "Yellow Flower", 0.5f, 0 },   // 39
	{ "kelp_BB_SF_0.47_color.dds", "Kelp 1", 0.47f, 0 },              // 40
	{ "kelp2_BB_SF_0.47_color.dds", "Kelp 2", 0.47f, 0 },             // 41
	{ "kelp3_BB_SF_0.47_color.dds", "Kelp 3", 0.47f, 0 },             // 42
	{ "seaweed_BB_SF_0.77_color.dds", "Seaweed 1", 0.77f, 0 },        // 43
	{ "seaweed2_BB_SF_0.9_color.dds", "Seaweed 2", 0.9f, 0 },         // 44
	{ "seaweed3_BB_SF_0.7_color.dds", "Seaweed 3", 0.7f, 0 },         // 45
};

#if (GGTERRAIN_MAX_SOURCE_TEXTURES != 32) || (GGGRASS_NUM_SELECTABLE_TYPES != 22)
	#error Update the grassMaterialTypes array for new GGTERRAIN_MAX_SOURCE_TEXTURES and GGGRASS_NUM_SELECTABLE_TYPES values
#endif

uint8_t grassMaterialTypes[ GGTERRAIN_MAX_SOURCE_TEXTURES ][ GGGRASS_NUM_SELECTABLE_TYPES ] = 
{
	{   0,   7,  14,  21, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 0 - rainforest, specific grass
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 1 - wet sand (no good match)
	{   1,   8,  15,  22, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 2 - rainforest grass, specific grass
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 3 - dry sand (no good match)
	{   6,  13,  20,  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 4 - rainforest rock (grass 29)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 5 - desert gravel (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 6 - desert rock (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 7 - desert sand (no good match)
	{   2,   9,  16,  23, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 8 - arctic snow and grass, specific grass
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 9 - mud (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 10 - snowy rock (no good match)
	{   2,   9,  16,  23, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 11 - trampled snow (grass 8)
	{   2,   9,  16,  23, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 12 - fresh snow (grass 8)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 13 - cracked mud (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 14 - canyon gravel (no good match)
	{   6,  13,  20,  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 15 - canyon gravel and grass (grass 29)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 16 - canyon rock (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 17 - loose rock (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 18 - mountain rock (no good match)
	{   3,  10,  17,  24, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 19 - moss, specific grass
	{   4,  11,  18,  25, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 20 - loose rock and grass (grass 24)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 21 - wet pebbles (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 22 - dry pebbles (no good match)
	{   3,  10,  17,  24, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 23 - ferns (grass 19)
	{   4,  11,  18,  25, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 24 - damp grass, specific grass
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 25 - rocky sand (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 26 - rock (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 27 - dry rough ground (no good match)
	{  5,   12,  19,  26, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 28 - forest grass, specific grass
	{  6,   13,  20,  27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 29 - firest twigs, specific grass
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 30 - wet rocks (no good match)
	{ 255, 255, 255, 255, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45 },  // 31 - checkerboard (no grass)
};

#if (GGGRASS_NUM_TYPES != 46) || (GGGRASS_NUM_SELECTABLE_TYPES != 22)
	#error Update the grassSourceTypes array for new GGGRASS_NUM_TYPES and GGGRASS_NUM_SELECTABLE_TYPES values
#endif

uint8_t grassSourceTypes[ GGGRASS_NUM_TYPES ] = 
{
	0, 0, 0, 0, 0, 0, 0, // course grass
	1, 1, 1, 1, 1, 1, 1, // short grass
	2, 2, 2, 2, 2, 2, 2, // tall grass
	3, 3, 3, 3, 3, 3, 3, // wild grass
	4, 5, 6, 7, 8, 9, 10, 11, 12, // weeds
	13, 14, 15, // flowers
	16, 17, 18, // kelp
	19, 20, 21, // seaweed
};

uint32_t GGGrass_GetRealIndex( uint32_t matIndex, uint32_t grassTypeIndex )
{
	if ( matIndex >= GGTERRAIN_MAX_SOURCE_TEXTURES ) matIndex = 0;
	if ( grassTypeIndex >= GGGRASS_NUM_SELECTABLE_TYPES ) grassTypeIndex = 0;

	return grassMaterialTypes[ matIndex ][ grassTypeIndex ];
}

const char* GGGrass_GetTextureFilename( uint32_t matIndex, uint32_t grassIndex )
{
	uint32_t index = GGGrass_GetRealIndex( matIndex, grassIndex );
	if ( index > GGGRASS_NUM_TYPES ) return "None";
	return grassFiles[ index ].filename;
}

const char* GGGrass_GetTextureShortName( uint32_t matIndex, uint32_t grassIndex )
{
	uint32_t index = GGGrass_GetRealIndex( matIndex, grassIndex );
	if ( index > GGGRASS_NUM_TYPES ) return "None";
	return grassFiles[ index ].shortname;
}

uint32_t GGGrass_GetNumTypes()
{
	return GGGRASS_NUM_TYPES;
}

// where each grass type's Wicked material loaded its texture from (GetTextureSource); made when a type is first drawn
GGTerrain::GGTextureSource gggrass_textureSource[ GGGRASS_TOTAL_REAL_TYPES ];

void GGGrass_RecordTextureSource( uint32_t typeIdx, const char* path )
{
	if ( typeIdx >= GGGRASS_TOTAL_REAL_TYPES || !path ) return;
	GGTerrain::GGTerrain_RecordTextureSource( &gggrass_textureSource[ typeIdx ], path, true );
}

const char* GGGrass_GetTextureSource( int slice, int* pChanged )
{
	*pChanged = 0;
	if ( slice < 0 || slice >= GGGRASS_TOTAL_REAL_TYPES ) return 0;
	*pChanged = GGTerrain::GGTerrain_CheckTextureSource( &gggrass_textureSource[ slice ] );
	return gggrass_textureSource[ slice ].loaded.c_str();
}

const GrassTypeInfo* GGGrass_GetTypeInfo( uint32_t typeIdx )
{
	if ( typeIdx >= GGGRASS_NUM_TYPES ) return nullptr;
	// GrassFile and GrassTypeInfo are layout-compatible (same field order/types);
	// reinterpret keeps the call zero-copy and avoids dragging GrassFile into the header.
	static_assert( sizeof(GrassFile) == sizeof(GrassTypeInfo), "GrassFile and GrassTypeInfo must match" );
	return reinterpret_cast<const GrassTypeInfo*>( &grassFiles[ typeIdx ] );
}

// Set by GGGrass_Update_Painting whenever pGrassMap cells inside a brush footprint may have been
// written. Tracks the set of chunk keys (cx<<32|cz) the painted area covers. The Wicked grass
// renderer drains this every frame and only rebuilds the listed chunks — previously we kept one
// bounding rect for the whole stroke, which invalidated every chunk the cursor swept over (so
// distant patches reseeded even though their grass map cells were unchanged).
static float                       gggrass_chunk_stride = 0.0f;
static std::unordered_set<uint64_t> gggrass_dirty_chunks;

void GGGrass_SetChunkStride( float strideInUnits )
{
	gggrass_chunk_stride = strideInUnits;
}

bool GGGrass_TakeDirtyChunks( wi::vector<uint64_t>& out )
{
	if ( gggrass_dirty_chunks.empty() ) return false;
	out.reserve( out.size() + gggrass_dirty_chunks.size() );
	for ( uint64_t k : gggrass_dirty_chunks ) out.push_back( k );
	gggrass_dirty_chunks.clear();
	return true;
}

// Mid-stroke throttle: every paint frame the brush footprint adds chunk keys to the *pending*
// set. The pending set is moved into the public *dirty* set every 6 frames (and on stroke
// release), which gives ~10 Hz visible paint feedback at 60 fps without thrashing the Wicked
// grass rebuilder every frame.
static bool                         gggrass_stroke_in_progress = false;
static int                          gggrass_stroke_frames_since_drain = 0;
static std::unordered_set<uint64_t> gggrass_pending_chunks;

// Helper: insert every chunk-key that intersects the supplied world-XZ AABB into the pending set.
// Chunk centers sit on the integer grid `cx*stride`, so a chunk overlaps a rect when
// `cx*stride ± stride/2` straddles the rect bounds. We sweep with a 1-cell margin as a generous
// safety bound — a typical 34-inch brush at a chunk boundary touches at most 4 chunks, far
// cheaper than the old stroke-bbox sweep that touched every chunk the cursor passed over.
static void GGGrass_MarkPaintedRect( float minX, float minZ, float maxX, float maxZ )
{
	if ( gggrass_chunk_stride <= 0.0f ) return; // GGTerrainWicked hasn't told us the stride yet
	float inv = 1.0f / gggrass_chunk_stride;
	int32_t cxMin = (int32_t)floorf( minX * inv ) - 1;
	int32_t cxMax = (int32_t)floorf( maxX * inv ) + 1;
	int32_t czMin = (int32_t)floorf( minZ * inv ) - 1;
	int32_t czMax = (int32_t)floorf( maxZ * inv ) + 1;
	float half = gggrass_chunk_stride * 0.5f;
	for ( int32_t cx = cxMin; cx <= cxMax; cx++ )
	{
		float cxCenter = cx * gggrass_chunk_stride;
		if ( cxCenter + half < minX || cxCenter - half > maxX ) continue;
		for ( int32_t cz = czMin; cz <= czMax; cz++ )
		{
			float czCenter = cz * gggrass_chunk_stride;
			if ( czCenter + half < minZ || czCenter - half > maxZ ) continue;
			uint64_t key = ( (uint64_t)(uint32_t)cx << 32 ) | (uint64_t)(uint32_t)cz;
			gggrass_pending_chunks.insert( key );
		}
	}
}

static void GGGrass_AccumStrokeRect( float minX, float minZ, float maxX, float maxZ )
{
	gggrass_stroke_in_progress = true;
	GGGrass_MarkPaintedRect( minX, minZ, maxX, maxZ );
}

static void GGGrass_PublishStrokeRect()
{
	if ( !gggrass_pending_chunks.empty() )
	{
		for ( uint64_t k : gggrass_pending_chunks ) gggrass_dirty_chunks.insert( k );
		gggrass_pending_chunks.clear();
	}
	gggrass_stroke_in_progress = false;
	gggrass_stroke_frames_since_drain = 0;
}

bool GGGrass_IsPaintStrokeActive()
{
	return gggrass_internal_params.mouseLeftState != 0;
}

int grassGridOffsetX = 0;
int grassGridOffsetZ = 0;

float grassCameraX = 0;
float grassCameraY = 0;
float grassCameraZ = 0;

uint32_t GGGrass_GetGrassMap( float x, float z );
void GGGrass_SetGrassMap( float x, float z, uint8_t grassID );

struct GrassChunk
{
	InstanceGrass pInstances[ numGrassPerChunk ];
	float minHeight = 0;
	float maxHeight = 0;
	GPUBuffer bufferInstances;
	uint32_t numValid = 0;
	float centerX;
	float centerZ;
	int x;
	int z;
	uint32_t randSeedOrig = 0;
	uint32_t randSeed = 0;

	float LocalRandomFloat() 
	{ 
		// Uses integer overflow to generate pseudo random numbers
		randSeed = (214013*randSeed + 2531011);
		// only use the top 16 bits as the lower 16 bits produce very short repeat cycles
		return ((randSeed >> 16) & 0xffff) / (float) 0xffff;
	}

	uint32_t LocalRandom() 
	{ 
		// Uses integer overflow to generate pseudo random numbers
		randSeed = (214013*randSeed + 2531011);
		// only use the top 16 bits as the lower 16 bits produce very short repeat cycles
		return (randSeed >> 16) & 0xffff;
	}

	void GetBounds( AABB* aabb )
	{
		if ( !aabb ) return;

		float halfWidth = grassAreaPerChunk / 2;
		float grassWidth = 50.0f * GGGRASS_SCALE;

		aabb->_min.x = centerX - halfWidth - grassWidth;
		aabb->_min.y = minHeight;
		aabb->_min.z = centerZ - halfWidth - grassWidth;

		aabb->_max.x = centerX + halfWidth + grassWidth;
		aabb->_max.y = maxHeight;
		aabb->_max.z = centerZ + halfWidth + grassWidth;
	}

	int Update()
	{
		if (!gggrass_initialised) return 0;
		if ( !GGTerrain_IsReady() ) return 0;

		minHeight = 1e9f;
		maxHeight = -1e9f;
		numValid = 0;
		randSeed = randSeedOrig;
		for( int i = 0; i < numGrassPerChunk; i++ )
		{
			InstanceGrass* pInstance = &pInstances[ i ];
			
			pInstance->x = centerX + ((LocalRandomFloat() - 0.5f) * grassAreaPerChunk);
			pInstance->y = 0; // to be determined later
			pInstance->z = centerZ + ((LocalRandomFloat() - 0.5f) * grassAreaPerChunk);
			//pInstance->SetData( i, LocalRandom() % 16, 1 );
			pInstance->SetData( i, 0, 0 );

			uint32_t grassType = GGGrass_GetGrassMap( pInstance->x, pInstance->z );
			if ( grassType == 0 ) continue;

			float ny;
			if ( !GGTerrain_GetNormal( pInstance->x, pInstance->z, 0, &ny, 0 ) )
				continue;
			if ( ny < 0.7 ) continue;

			float height;
			if ( !GGTerrain_GetHeight( pInstance->x, pInstance->z, &height ) )
				continue;

			if ( height > g.gdefaultwaterheight )
			{
				// above water
				if ( height > gggrass_global_params.max_height || height < gggrass_global_params.min_height ) continue;
			}
			else
			{
				// underwater
				if ( height > gggrass_global_params.max_height_underwater || height < gggrass_global_params.min_height_underwater ) continue;
			}

			if ( grassType == 1 )
			{
				if ( g.gdefaultwaterheight > height )
				{
					// under water
					grassType = 43; // seaweed 1
				}
				else
				{
					// check for grass materials
					int material = GGTerrain_GetMaterialIndex( pInstance->x, pInstance->z );
					grassType = GGGrass_GetRealIndex(material, 0);
				}

				if ( grassType >= GGGRASS_NUM_TYPES ) grassType = 0;
				else grassType += 2;
				
				GGGrass_SetGrassMap( pInstance->x, pInstance->z, grassType );
			}
			
			if ( grassType > 1 ) 
			{
				grassType -= 2;
				
				if ( grassType >= GGGRASS_NUM_TYPES )
				{
					pInstance->SetVisible( 0 ); 
				}
				else
				{
					pInstance->SetType( grassType );
					pInstance->SetVisible( 1 );
				}
			}
			
			pInstance->y = height;
							
			if ( pInstance->IsVisible() ) 
			{
				if ( pInstance->y < minHeight ) minHeight = pInstance->y;
				float grassHeight = 100 * GGGRASS_SCALE;
				if ( pInstance->y + grassHeight > maxHeight ) maxHeight = pInstance->y + grassHeight;

				if ( numValid != i )
				{
					// swap elements to keep all visible ones near the beginning
					InstanceGrass temp = pInstances[ i ];
					pInstances[ i ] = pInstances[ numValid ];
					pInstances[ numValid ] = temp;
				}
				numValid++;
			}
		}

		if ( numValid > 0 )
		{
			GPUBufferDesc bufferDesc = {};
			SubresourceData data = {};
			data.data_ptr = pInstances;
			bufferDesc.size = sizeof(InstanceGrass) * numValid;
			bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
			//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
			bufferDesc.misc_flags = ResourceMiscFlag::NONE;
			wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferInstances );
		}

		return 1;
	}

	int UpdatePosition( int x, int z )
	{
		centerX = (float) x;
		centerX += 0.5f;
		centerX *= grassAreaPerChunk;

		centerZ = (float) z;
		centerZ += 0.5f;
		centerZ *= grassAreaPerChunk;

		randSeedOrig = ((x & 0xFFFF) << 16) | (z & 0xFFFF);

		return Update();
	}
};

GrassChunk pGrassChunks[ numGrassChunks ];
uint8_t pGrassGrid[ numGrassChunks ];
uint8_t* pGrassMap = 0;
Texture texGrassMap;

GrassCB grassConstantData = {};
GPUBuffer grassConstantBuffer;

GPUBuffer bufferGrassVertices;
GPUBuffer bufferGrassIndices;

Shader shaderGrassVS;
Shader shaderGrassPS;
PipelineState psoGrass;

Shader shaderGrassPrepassVS;
Shader shaderGrassPrepassPS;
PipelineState psoGrassPrepass;

Shader shaderGrassShadowVS;
Shader shaderGrassShadowPS;
PipelineState psoGrassShadow;

Texture texGrass;
Texture texGrassNormal;
Texture texNoise;
Sampler samplerTrilinearClamp;
Sampler samplerTrilinearWrap;
Sampler samplerPointWrap;

void GGGrass_UploadGrassMap()
{
	if ( !pGrassMap ) return;
	GraphicsDevice* device = wiGraphics::GetDevice();

	SubresourceData initdata = {};
	initdata.data_ptr = pGrassMap;
	initdata.row_pitch = GGGRASS_MAP_SIZE;
	initdata.slice_pitch = 0;

	TextureDesc texDesc = {};
	texDesc.bind_flags = BindFlag::SHADER_RESOURCE;
	texDesc.sample_count = 1;
	texDesc.mip_levels = 1;
	texDesc.array_size = 1;
	texDesc.format = Format::R8_UNORM;
	texDesc.usage = Usage::DEFAULT;
	texDesc.width = GGGRASS_MAP_SIZE;
	texDesc.height = GGGRASS_MAP_SIZE;

	device->CreateTexture( &texDesc, &initdata, &texGrassMap );
	device->SetName( &texGrassMap, "texGrassMap" );
}

void GGGrass_BindGrassMap( int slot, CommandList cmd )
{
	GraphicsDevice* device = wiGraphics::GetDevice();
	device->BindResource( &texGrassMap, slot, cmd );
}

const wi::graphics::Texture* GGGrass_GetMapTexture()
{
	return &texGrassMap;
}

// Stage B.9: custom-slot registry. Palette slots 22..(GGGRASS_MAX_PALETTE_SLOTS-1) hold user-added
// DDS filenames (relative to Files/, matches t.visuals.sGrassTextures[] storage). The Wicked
// appearance builder reads back via GGGrass_GetCustomSlotFilename() when it constructs
// g_grassAppearance[real_type] for real_type >= GGGRASS_CUSTOM_REAL_TYPE_BASE.
static std::string g_customSlotFilenames[GGGRASS_MAX_PALETTE_SLOTS];
static bool        g_customSlotsDirty = false;

void GGGrass_SetCustomSlotFilename( int slot, const char* filename )
{
	if ( slot < GGGRASS_CUSTOM_SLOT_BASE || slot >= GGGRASS_MAX_PALETTE_SLOTS ) return;
	std::string nv = ( filename && filename[0] ) ? filename : "";
	if ( g_customSlotFilenames[slot] != nv )
	{
		g_customSlotFilenames[slot] = nv;
		g_customSlotsDirty = true;
	}
}

const char* GGGrass_GetCustomSlotFilename( int slot )
{
	if ( slot < GGGRASS_CUSTOM_SLOT_BASE || slot >= GGGRASS_MAX_PALETTE_SLOTS ) return nullptr;
	if ( g_customSlotFilenames[slot].empty() ) return nullptr;
	return g_customSlotFilenames[slot].c_str();
}

bool GGGrass_TakeCustomSlotsDirty()
{
	if ( !g_customSlotsDirty ) return false;
	g_customSlotsDirty = false;
	return true;
}

// Set by GGGrass_ScanRegion when it rewrites at least one pGrassMap cell during auto-resolve.
// GGGrass_TakePendingMapUpload() returns and clears this. The Wicked-side ProcessGrassChunks
// polls once per frame after its per-chunk scan pass and fires a single GGGrass_UploadGrassMap()
// if set — one 16 MB upload amortized across every chunk resolved that frame.
static bool s_gggrass_map_upload_pending = false;

void GGGrass_ScanRegion( float minX, float minZ, float maxX, float maxZ, bool* typesSeen )
{
	if ( !gggrass_initialised || !pGrassMap || !typesSeen ) return;

	const float editableSize = ggterrain_global_render_params2.editable_size;
	const float invWorld = 0.5f / editableSize;
	auto worldToCell = [&]( float w ) -> int {
		float f = ( w * invWorld + 0.5f ) * GGGRASS_MAP_SIZE;
		return (int)floorf( f );
	};
	// Inverse of worldToCell for the cell CENTER (SetGrassMap truncates worldToCell so
	// cell c covers [worldMin, worldMax); the center is c + 0.5 in cell space).
	const float cellToWorldScale = 2.0f * editableSize / (float)GGGRASS_MAP_SIZE;
	auto cellCenterToWorld = [&]( int c ) -> float {
		return ( (float)c + 0.5f ) * cellToWorldScale - editableSize;
	};

	int x0 = worldToCell( minX );
	int x1 = worldToCell( maxX );
	int z0 = worldToCell( minZ );
	int z1 = worldToCell( maxZ );

	if ( x0 < 0 ) x0 = 0;
	if ( z0 < 0 ) z0 = 0;
	if ( x1 >= GGGRASS_MAP_SIZE ) x1 = GGGRASS_MAP_SIZE - 1;
	if ( z1 >= GGGRASS_MAP_SIZE ) z1 = GGGRASS_MAP_SIZE - 1;
	if ( x1 < x0 || z1 < z0 ) return;

	for ( int z = z0; z <= z1; z++ )
	{
		uint8_t* row = &pGrassMap[ z * GGGRASS_MAP_SIZE ];
		for ( int x = x0; x <= x1; x++ )
		{
			uint8_t v = row[ x ];
			if ( v == 0 || ( v & 0x80 ) ) continue; // empty or flattened
			uint8_t encoded = v & 0x7F;

			// DX11-parity auto-resolve of value 1 ("auto": GGGrass_Init fills the map with it, and
			// Populate / every new-level preset writes it via GGGrass_AddAll with Match Terrain
			// Color - the brush never writes 1). DX11 resolved it in GrassChunk::Update; in Wicked
			// mode only the per-stroke call to that is gated off - its countdown/flatten/undo calls
			// still run, sparsely and without a texGrassMap upload, and resolve bare ground to 0 the
			// same as here - so this is the resolve that matters. Cheap: only encoded == 1 cells,
			// and the in-place rewrite means later scans and the .fpm save see the resolved byte.
			// Encoding: 0 = no grass, real_type + 2 = that grass.
			//
			// Slope filtering (grass on cliffs) is handled per-strand in the Wicked hair
			// simulate CS, not here — see WICKED_ENGINE_CHANGES.md entry 1.5. Filtering at
			// grass-map cell resolution against the coarse DX11 normal map produced ragged
			// cliff edges (~4.8 unit cell vs. hundreds-of-units per normal-map texel), and
			// filtering at cell resolution against locally-computed heights still left flat
			// cells adjacent to cliffs contributing strands that landed on the cliff triangle.
			// Per-strand shader filtering picks up the exact face normal of the triangle each
			// strand sits on, so a strand on a cliff-face triangle vanishes regardless of
			// which paint cell it belongs to.
			if ( encoded == 1 )
			{
				const float worldX = cellCenterToWorld( x );
				const float worldZ = cellCenterToWorld( z );
				float height;
				if ( !GGTerrain_GetHeight( worldX, worldZ, &height ) ) continue;

				uint32_t realType;
				if ( g.gdefaultwaterheight > height )
				{
					realType = 43;   // seaweed 1
				}
				else
				{
					// GGMAX 3.118: GGTerrain_GetMaterialIndex falls back to material 31 (a bare row) when its
					// own normal lookup fails - and bare ground now resolves to EMPTY, so a transient failure
					// would DELETE the grass. Leave the cell auto (1) and resolve it on a later scan (DX11
					// GrassChunk::Update also required GGTerrain_GetNormal to succeed before resolving).
					float ny;
					if ( !GGTerrain_GetNormal( worldX, worldZ, 0, &ny, 0 ) ) continue;
					int material = GGTerrain_GetMaterialIndex( worldX, worldZ );
					realType = GGGrass_GetRealIndex( (uint32_t)material, 0u );
				}
				// GGMAX 3.118: DX11 parity (DX11 GGGrass.cpp:371-374) - ground with no matching grass (255 in
				// grassMaterialTypes: sand, rock, mud, gravel, pebbles...) resolves to EMPTY. Stage B.10 wrote
				// real type 0 + 2 = Course Grass there, and saving the level kept it. Never leave it at 1: the
				// hair simulate CS draws an unresolved 1 as Course Grass as well. Never (uint8_t)(255 + 2):
				// that wraps to 1 - re-resolved every scan, a 16 MB map upload every frame.
				row[ x ] = ( realType >= GGGRASS_NUM_TYPES ) ? (uint8_t)0 : (uint8_t)( realType + 2 );
				s_gggrass_map_upload_pending = true;
				// Deliberate divergence (review 3.118): DX11 left STEEP (ny < 0.7) or out-of-height-band cells at 1
				// and resolved them later, so a cliff sculpted flat regrew grass. Leaving them at 1 here would draw
				// them as Course Grass on the gentler triangles of rocky cliffs (the CS reads 1 as type 0) - the
				// very thing this fixes - so bare ground resolves to empty at any slope; repaint or Populate after
				// flattening a cliff.
				if ( row[ x ] == 0 ) continue;   // nothing grows here - must not mark typesSeen[0]
				encoded = row[ x ];
			}

			uint32_t typeIdx = ( encoded >= 2 ) ? (uint32_t)( encoded - 2 ) : 0u; // mirror C++ side
			// Stage B.9: typesSeen array is sized to GGGRASS_TOTAL_REAL_TYPES on the Wicked side to
			// hold stock (0..45) + custom (46..GGGRASS_TOTAL_REAL_TYPES-1) real_type indices.
			if ( typeIdx < GGGRASS_TOTAL_REAL_TYPES ) typesSeen[ typeIdx ] = true;
		}
	}
}

bool GGGrass_RegionHasGrass( float minX, float minZ, float maxX, float maxZ )
{
	if ( !gggrass_initialised || !pGrassMap ) return false;
	const float editableSize = ggterrain_global_render_params2.editable_size;
	if ( editableSize <= 0.0f ) return false;
	// the same cell mapping and clamping as GGGrass_ScanRegion, so the two can never disagree
	const float invWorld = 0.5f / editableSize;
	auto worldToCell = [&]( float w ) -> int { return (int)floorf( ( w * invWorld + 0.5f ) * GGGRASS_MAP_SIZE ); };
	int x0 = worldToCell( minX ), x1 = worldToCell( maxX );
	int z0 = worldToCell( minZ ), z1 = worldToCell( maxZ );
	if ( x0 < 0 ) x0 = 0;
	if ( z0 < 0 ) z0 = 0;
	if ( x1 >= GGGRASS_MAP_SIZE ) x1 = GGGRASS_MAP_SIZE - 1;
	if ( z1 >= GGGRASS_MAP_SIZE ) z1 = GGGRASS_MAP_SIZE - 1;
	for ( int z = z0; z <= z1; z++ )
	{
		const uint8_t* row = &pGrassMap[ z * GGGRASS_MAP_SIZE ];
		for ( int x = x0; x <= x1; x++ )
		{
			const uint8_t v = row[ x ];
			if ( v != 0 && !( v & 0x80 ) ) return true;   // ScanRegion's own "empty or flattened" skip
		}
	}
	return false;
}

// GGMAX 3.118: see GGGrass.h. Cell centres and the height/material lookups match GGGrass_ScanRegion, so
// the audit sees each cell the way the resolve does.
void GGGrass_AuditMap( int step, char* out, size_t outSize )
{
	if ( !out || outSize == 0 ) return;
	out[ 0 ] = 0;
	if ( !gggrass_initialised || !pGrassMap ) { _snprintf( out, outSize, "ERROR: grass map not initialised" ); out[ outSize - 1 ] = 0; return; }
	if ( step < 1 ) step = 1;
	const float editableSize = ggterrain_global_render_params2.editable_size;
	const float cellToWorldScale = 2.0f * editableSize / (float)GGGRASS_MAP_SIZE;
	const float waterH = g.gdefaultwaterheight;
	// rows: 0 bare ground above water, 1 grassy material above water, 2 bare underwater, 3 grassy underwater
	// cols: 0 empty, 1 auto (unresolved), 2 Course Grass (real type 0), 3 another real type, 4 flattened
	uint32_t c[ 4 ][ 5 ] = {};
	uint32_t bareCourse[ GGTERRAIN_MAX_SOURCE_TEXTURES ] = {}, bareAuto[ GGTERRAIN_MAX_SOURCE_TEXTURES ] = {};
	uint32_t sampled = 0, noHeight = 0;
	for ( int z = 0; z < GGGRASS_MAP_SIZE; z += step )
	{
		for ( int x = 0; x < GGGRASS_MAP_SIZE; x += step )
		{
			const uint8_t v = pGrassMap[ z * GGGRASS_MAP_SIZE + x ];
			const float wx = ( (float)x + 0.5f ) * cellToWorldScale - editableSize;
			const float wz = ( (float)z + 0.5f ) * cellToWorldScale - editableSize;
			float h;
			if ( !GGTerrain_GetHeight( wx, wz, &h ) ) { noHeight++; continue; }
			const int mat = GGTerrain_GetMaterialIndex( wx, wz );
			const bool bareGround = ( GGGrass_GetRealIndex( (uint32_t)mat, 0u ) >= GGGRASS_NUM_TYPES );
			const int row = ( h >= waterH ) ? ( bareGround ? 0 : 1 ) : ( bareGround ? 2 : 3 );
			const int col = ( v & 0x80 ) ? 4 : ( v == 0 ? 0 : ( v == 1 ? 1 : ( v == 2 ? 2 : 3 ) ) );
			c[ row ][ col ]++;
			sampled++;
			if ( row == 0 && mat >= 0 && mat < GGTERRAIN_MAX_SOURCE_TEXTURES )
			{
				if ( col == 2 ) bareCourse[ mat ]++;
				else if ( col == 1 ) bareAuto[ mat ]++;
			}
		}
	}
	static const char* rowName[ 4 ] = { "bare ground (no matching grass)", "grassy material", "underwater, bare ground", "underwater, grassy material" };
	int len = _snprintf( out, outSize, "GRASSMAP AUDIT (every %d cell(s), %u sampled, %u off the terrain)\n"
		"  %-32s %9s %9s %9s %9s %9s %9s\n", step, sampled, noHeight, "ground under the cell", "total", "empty", "auto(1)", "course(2)", "other", "flat" );
	for ( int r = 0; r < 4 && len > 0 && len < (int)outSize - 160; r++ )
	{
		const uint32_t t = c[ r ][ 0 ] + c[ r ][ 1 ] + c[ r ][ 2 ] + c[ r ][ 3 ] + c[ r ][ 4 ];
		len += _snprintf( out + len, outSize - len, "  %-32s %9u %9u %9u %9u %9u %9u\n", rowName[ r ], t, c[ r ][ 0 ], c[ r ][ 1 ], c[ r ][ 2 ], c[ r ][ 3 ], c[ r ][ 4 ] );
	}
	if ( len > 0 && len < (int)outSize - 64 ) len += _snprintf( out + len, outSize - len, "  bare materials (index: course(2) / auto(1)):" );
	for ( int m = 0; m < GGTERRAIN_MAX_SOURCE_TEXTURES && len > 0 && len < (int)outSize - 32; m++ )
		if ( bareCourse[ m ] || bareAuto[ m ] ) len += _snprintf( out + len, outSize - len, " %d: %u / %u;", m, bareCourse[ m ], bareAuto[ m ] );
	if ( len > 0 && len < (int)outSize - 2 ) len += _snprintf( out + len, outSize - len, "\n" );
	out[ outSize - 1 ] = 0;
}

bool GGGrass_TakePendingMapUpload()
{
	if ( !s_gggrass_map_upload_pending ) return false;
	s_gggrass_map_upload_pending = false;
	return true;
}

// Set by bulk pGrassMap writers (GGGrass_AddAll, GGGrass_RemoveAll) so the Wicked hair entity
// cache — which is per-chunk and blind to non-paint writes — can be flushed and rebuilt from
// the new map contents.
static bool s_gggrass_full_rebuild_pending = false;

bool GGGrass_TakeFullRebuildPending()
{
	if ( !s_gggrass_full_rebuild_pending ) return false;
	s_gggrass_full_rebuild_pending = false;
	return true;
}

// GGMAX 3.118: see GGGrass.h. Same cell centres and lookups as GGGrass_ScanRegion.
int GGGrass_RemoveCourseGrassFromBareGround( bool dryRun )
{
	if ( !gggrass_initialised || !pGrassMap ) return -1;
	if ( !GGTerrain_IsReady() ) return -1;
	const float editableSize = ggterrain_global_render_params2.editable_size;
	const float cellToWorldScale = 2.0f * editableSize / (float)GGGRASS_MAP_SIZE;
	int cleared = 0;
	for ( int z = 0; z < GGGRASS_MAP_SIZE; z++ )
	{
		uint8_t* row = &pGrassMap[ z * GGGRASS_MAP_SIZE ];
		for ( int x = 0; x < GGGRASS_MAP_SIZE; x++ )
		{
			const uint8_t v = row[ x ];
			if ( ( v & 0x7F ) != 2 ) continue;   // Course Grass, flattened or not
			const float wx = ( (float)x + 0.5f ) * cellToWorldScale - editableSize;
			const float wz = ( (float)z + 0.5f ) * cellToWorldScale - editableSize;
			float h, ny;
			// above OR below the water (review 3.118): the pre-3.118 Match-on brush wrote byte 2 on underwater
			// sand too, while the resolve only ever wrote seaweed there - so byte 2 underwater is the same case
			if ( !GGTerrain_GetHeight( wx, wz, &h ) ) continue;
			if ( !GGTerrain_GetNormal( wx, wz, 0, &ny, 0 ) ) continue;
			const int mat = GGTerrain_GetMaterialIndex( wx, wz );
			if ( GGGrass_GetRealIndex( (uint32_t)mat, 0u ) < GGGRASS_NUM_TYPES ) continue;   // ground that grows grass
			// a flattened cell goes back to AUTO + flattened: when the flat area is removed the resolve judges the
			// real ground, instead of the Course Grass reappearing (review 3.118)
			if ( !dryRun ) row[ x ] = ( v & 0x80 ) ? (uint8_t)0x81 : (uint8_t)0;
			cleared++;
		}
	}
	if ( !dryRun && cleared > 0 )
	{
		GGGrass_UploadGrassMap();
		s_gggrass_full_rebuild_pending = true;   // as GGGrass_AddAll: the Wicked hair cache is per chunk
	}
	return cleared;
}

float GGGrass_GetDefaultWaterHeight()
{
	return g.gdefaultwaterheight;
}

// TODO: DX12 - tinyddsloader removed, rewrite DDS loading
#if 0
wiGraphics::FORMAT ConvertDDSFormat( tinyddsloader::DDSFile::DXGIFormat format )
{
	switch (format)
	{
		case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_Float: return FORMAT_R32G32B32A32_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_UInt: return FORMAT_R32G32B32A32_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_SInt: return FORMAT_R32G32B32A32_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_Float: return FORMAT_R32G32B32_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_UInt: return FORMAT_R32G32B32_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_SInt: return FORMAT_R32G32B32_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Float: return FORMAT_R16G16B16A16_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_UNorm: return FORMAT_R16G16B16A16_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_UInt: return FORMAT_R16G16B16A16_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_SNorm: return FORMAT_R16G16B16A16_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_SInt: return FORMAT_R16G16B16A16_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32_Float: return FORMAT_R32G32_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32_UInt: return FORMAT_R32G32_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R32G32_SInt: return FORMAT_R32G32_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_UNorm: return FORMAT_R10G10B10A2_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_UInt: return FORMAT_R10G10B10A2_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R11G11B10_Float: return FORMAT_R11G11B10_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_UNorm: return FORMAT_B8G8R8A8_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm: return FORMAT_B8G8R8A8_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm_SRGB: return FORMAT_B8G8R8A8_UNORM_SRGB;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm: return FORMAT_R8G8B8A8_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm_SRGB: return FORMAT_R8G8B8A8_UNORM_SRGB;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UInt: return FORMAT_R8G8B8A8_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SNorm: return FORMAT_R8G8B8A8_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SInt: return FORMAT_R8G8B8A8_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16_Float: return FORMAT_R16G16_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16_UNorm: return FORMAT_R16G16_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16_UInt: return FORMAT_R16G16_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16_SNorm: return FORMAT_R16G16_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16G16_SInt: return FORMAT_R16G16_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::D32_Float: return FORMAT_D32_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R32_Float: return FORMAT_R32_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::R32_UInt: return FORMAT_R32_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R32_SInt: return FORMAT_R32_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8_UNorm: return FORMAT_R8G8_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8_UInt: return FORMAT_R8G8_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8_SNorm: return FORMAT_R8G8_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R8G8_SInt: return FORMAT_R8G8_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R16_Float: return FORMAT_R16_FLOAT;
		case tinyddsloader::DDSFile::DXGIFormat::D16_UNorm: return FORMAT_D16_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16_UNorm: return FORMAT_R16_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16_UInt: return FORMAT_R16_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R16_SNorm: return FORMAT_R16_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R16_SInt: return FORMAT_R16_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::R8_UNorm: return FORMAT_R8_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R8_UInt: return FORMAT_R8_UINT;
		case tinyddsloader::DDSFile::DXGIFormat::R8_SNorm: return FORMAT_R8_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::R8_SInt: return FORMAT_R8_SINT;
		case tinyddsloader::DDSFile::DXGIFormat::A8_UNorm: return FORMAT_R8_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm: return FORMAT_BC1_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB: return FORMAT_BC1_UNORM_SRGB;
		case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm: return FORMAT_BC2_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB: return FORMAT_BC2_UNORM_SRGB;
		case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm: return FORMAT_BC3_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB: return FORMAT_BC3_UNORM_SRGB;
		case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm: return FORMAT_BC4_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm: return FORMAT_BC4_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm: return FORMAT_BC5_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm: return FORMAT_BC5_SNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm: return FORMAT_BC7_UNORM;
		case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB: return FORMAT_BC7_UNORM_SRGB;
		default:
			assert(0); // incoming format is not supported
			return FORMAT_UNKNOWN;
	}
}

void GGGrass_LoadTextureDDS( const char* filename, Texture* tex )
{
	if (!gggrass_initialised) return;

	GraphicsDevice* device = wiGraphics::GetDevice();

	char filePath[ MAX_PATH ];
	strcpy_s( filePath, MAX_PATH, filename );
	GG_GetRealPath( filePath, 0 );

	tinyddsloader::DDSFile dds;
	auto result = dds.Load( filePath );

	if (result != tinyddsloader::Result::Success) return;

	TextureDesc desc;
	desc.ArraySize = 1;
	desc.BindFlags = BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.width = dds.GetWidth();
	desc.height = dds.GetHeight();
	desc.Depth = dds.GetDepth();
	desc.MipLevels = dds.GetMipCount();
	desc.ArraySize = dds.GetArraySize();
	desc.MiscFlags = 0;
	desc.Usage = USAGE_IMMUTABLE;
	desc.layout = IMAGE_LAYOUT_SHADER_RESOURCE;
	desc.Format = ConvertDDSFormat( dds.GetFormat() );

	std::vector<SubresourceData> InitData;
	for (uint32_t arrayIndex = 0; arrayIndex < desc.ArraySize; ++arrayIndex)
	{
		for (uint32_t mip = 0; mip < desc.MipLevels; ++mip)
		{
			auto imageData = dds.GetImageData(mip, arrayIndex);
			SubresourceData subresourceData;
			subresourceData.pSysMem = imageData->m_mem;
			subresourceData.SysMemPitch = imageData->m_memPitch;
			subresourceData.SysMemSlicePitch = imageData->m_memSlicePitch;
			InitData.push_back(subresourceData);
		}
	}

	auto dim = dds.GetTextureDimension();
	switch (dim)
	{
		case tinyddsloader::DDSFile::TextureDimension::Texture1D: desc.type = TextureDesc::TEXTURE_1D; break;
		case tinyddsloader::DDSFile::TextureDimension::Texture2D: desc.type = TextureDesc::TEXTURE_2D; break;
		case tinyddsloader::DDSFile::TextureDimension::Texture3D: desc.type = TextureDesc::TEXTURE_3D;break;
		default: assert(0); break;
	}

	device->CreateTexture( &desc, InitData.data(), tex );
}
#endif

void GGGrass_CreateEmptyTexture( int width, int height, int mipLevels, int levels, Format format, Texture* tex )
{
	if (!gggrass_initialised) return;

	GraphicsDevice* device = wiGraphics::GetDevice();

	TextureDesc texDesc = {};
	texDesc.bind_flags = BindFlag::SHADER_RESOURCE;
	texDesc.sample_count = 1;
	texDesc.mip_levels = mipLevels;
	texDesc.array_size = levels;
	texDesc.format = format;
	texDesc.usage = Usage::DEFAULT;
	texDesc.width = width;
	texDesc.height = height;

	device->CreateTexture( &texDesc, nullptr, tex );
	device->SetName( tex, "tex" );
}

// TODO: DX12 - tinyddsloader removed, rewrite DDS loading
#if 0
void GGGrass_LoadTextureDDSIntoSlice( const char* filename, Texture* tex, uint32_t arraySlice )
{
	if (!gggrass_initialised) return;

	GraphicsDevice* device = wiGraphics::GetDevice();

	char filePath[ MAX_PATH ];
	strcpy_s( filePath, MAX_PATH, filename );
	GG_GetRealPath( filePath, 0 );

	tinyddsloader::DDSFile dds;
	auto result = dds.Load( filePath );

	if (result != tinyddsloader::Result::Success) return;

	uint32_t maxMip = dds.GetMipCount();
	if ( maxMip > tex->desc.MipLevels ) maxMip = tex->desc.MipLevels;

	std::vector<SubresourceData> InitData;
	for( uint32_t mip = 0; mip < maxMip; ++mip )
	{
		auto imageData = dds.GetImageData(mip, 0);
		device->UpdateTexture( tex, mip, arraySlice, 0, imageData->m_mem, imageData->m_memPitch, -1 );
	}
}
#endif

// TODO: DX12 - tinyddsloader removed, stub functions until DDS loading is rewritten
void GGGrass_LoadTextureDDS( const char* filename, Texture* tex )
{
	// stub - DDS loading disabled
}

void GGGrass_LoadTextureDDSIntoSlice( const char* filename, Texture* tex, uint32_t arraySlice )
{
	// stub - DDS loading disabled
}

// only call this when grass heights need to be updated, e.g. when the terrain has changed
int GGGrass_UpdateInstances()
{
	if (!gggrass_initialised) return 0;

	for( uint32_t i = 0; i < numGrassChunks; i++ )
	{
		if ( !pGrassChunks[ i ].Update() ) return 0;
	}
	
	return 1;
}

int64_t i64StartTime = 0;
int64_t i64TimeFreq = 1;
void GrassTimerInit()
{
	QueryPerformanceCounter ( (LARGE_INTEGER*) &i64StartTime );
	QueryPerformanceFrequency ( (LARGE_INTEGER*) &i64TimeFreq );
	if ( i64TimeFreq == 0 ) i64TimeFreq = 1;
}

// Timer in seconds
float GrassTimer()
{
	int64_t i64CurrentTime;
	QueryPerformanceCounter ( (LARGE_INTEGER*) &i64CurrentTime );
	i64CurrentTime -= i64StartTime;
	return (float) (i64CurrentTime / (double) i64TimeFreq);
}

#ifdef ONLYLOADWHENUSED
//#pragma optimize("", off)

void GGGrass_LoadTextures(bool bAll = false,bool bInit = false)
{
	char grassFilename[256];
	bool bChanged = false;
	uint64_t values = gggrass_global_params.paint_type;
	uint32_t currMat = gggrass_global_params.paint_material;

	if (bInit)
	{
		//PE: Defaults 0,7
		bGrassTextureUploaded[0] = true;
		char szDstRoot[MAX_PATH];
		strcpy_s(szDstRoot, "files/grassbank/");
		strcpy_s(grassFilename, szDstRoot);
		strcat_s(grassFilename, grassFiles[0].filename);
		GGGrass_LoadTextureDDSIntoSlice(grassFilename, &texGrass, 0);

		bGrassTextureUploaded[7] = true;
		strcpy_s(grassFilename, szDstRoot);
		strcat_s(grassFilename, grassFiles[7].filename);
		GGGrass_LoadTextureDDSIntoSlice(grassFilename, &texGrass, 7);
		bChanged = true;

	}
	else
	{
		for (uint32_t i = 0; i < GGGRASS_NUM_SELECTABLE_TYPES; i++)
		{
			if (values & 1)
			{
				uint32_t realIndex = 0;

				realIndex = GGGrass_GetRealIndex(0, i); //PE: Manual paint version
				if (realIndex >= GGGRASS_NUM_TYPES) realIndex = 0;

				if (!bGrassTextureUploaded[realIndex])
				{
					bGrassTextureUploaded[realIndex] = true;
					char szDstRoot[MAX_PATH];
					strcpy_s(szDstRoot, "grassbank/");
					strcpy_s(grassFilename, szDstRoot);
					strcat_s(grassFilename, grassFiles[realIndex].filename);
					GGGrass_LoadTextureDDSIntoSlice(grassFilename, &texGrass, realIndex);
					bChanged = true;
				}

				realIndex = GGGrass_GetRealIndex(1, i);  //PE: Auto paint version
				if (realIndex >= GGGRASS_NUM_TYPES) realIndex = 0;

				if (!bGrassTextureUploaded[realIndex])
				{
					bGrassTextureUploaded[realIndex] = true;
					char szDstRoot[MAX_PATH];
					//sprintf_s(szDstRoot, MAX_PATH, "%sgrassbank/", FinalGameGuruMaxFolder.c_str());
					if (bInit)
						strcpy_s(szDstRoot, "files/grassbank/");
					else
						strcpy_s(szDstRoot, "grassbank/");

					strcpy_s(grassFilename, szDstRoot);
					strcat_s(grassFilename, grassFiles[realIndex].filename);
					GGGrass_LoadTextureDDSIntoSlice(grassFilename, &texGrass, realIndex);
					bChanged = true;
				}

				//pGrassMap[index] &= 0x80; // keep flattened state
				//pGrassMap[index] |= realIndex + 2;
			}
			values >>= 1;
		}
	}
	if (bChanged)
	{
		ggterrain_extra_params.iUpdateGrass = 5;
	}
}
#endif
void GGGrass_Init_Textures ( LPSTR pRemoteGrassPath )
{
	char grassFilename[256];
	for (uint32_t i = 0; i < GGGRASS_NUM_TYPES; i++)
	{
		if (strlen(pRemoteGrassPath) > 0)
		{
			strcpy_s(grassFilename, pRemoteGrassPath);
		}
		else
		{
			strcpy_s(grassFilename, "");
		}
		strcat_s(grassFilename, "grassbank\\");
		strcat_s(grassFilename, grassFiles[i].filename);
		GGGrass_LoadTextureDDSIntoSlice(grassFilename, &texGrass, i);
	}
}

void GGGrass_Init()
{
	gggrass_initialised = 1;
	GrassTimerInit();

	GraphicsDevice* device = wiGraphics::GetDevice();

	wiRenderer::LoadShader( ShaderStage::VS, shaderGrassVS, "GGGrassVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderGrassPS, "GGGrassPS.cso" );
									  
	wiRenderer::LoadShader( ShaderStage::VS, shaderGrassPrepassVS, "GGGrassPrepassVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderGrassPrepassPS, "GGGrassPrepassPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderGrassShadowVS, "GGGrassShadowMapVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderGrassShadowPS, "GGGrassShadowMapPS.cso" );

	GGGrass_LoadTextureDDS("Files/treebank/noise.dds", &texNoise);
	// GGMAX Tier A (2026-08-02): texGrass is NOT created here any more — see
	// GGGrass_EnsureLegacyTexArray() below for why, and for the on-demand creation.

#ifdef ONLYLOADWHENUSED
	for (uint32_t i = 0; i < GGGRASS_NUM_TYPES; i++)
	{
		bGrassTextureUploaded[i] = false;
		bGrassTextureForceUploaded[i] = false;
	}
	last_paint_type = gggrass_global_params.paint_type;
	GGGrass_LoadTextures(false,true);
#else

	GGGrass_Init_Textures("Files\\");
#endif

	pGrassMap = new uint8_t[ GGGRASS_MAP_SIZE * GGGRASS_MAP_SIZE ];
	memset( pGrassMap, 1, GGGRASS_MAP_SIZE * GGGRASS_MAP_SIZE );

	GGGrass_UploadGrassMap();
	
	for( uint32_t i = 0; i < numGrassChunks; i++ )
	{
		int x = i % grassSplit;
		int z = i / grassSplit;

		x -= grassSplit / 2;
		z -= grassSplit / 2;

		pGrassChunks[ i ].UpdatePosition( x, z );
		pGrassGrid[ i ] = i;
	}

	// raster state
	// GGMAX 2026-08-07: static — PipelineStateDesc stores POINTERS and the driver PSO
	// compiles lazily at first draw (same dangling-desc class as the gpup crash; this
	// path is currently dormant but the landmine is real). See GPUParticles_part0.cpp.
	static RasterizerState rastState = {};
	rastState.fill_mode = FillMode::SOLID;
	rastState.cull_mode = CullMode::NONE;
	rastState.front_counter_clockwise = true;
	rastState.depth_bias = 0;
	rastState.depth_bias_clamp = 0;
	rastState.slope_scaled_depth_bias = 0;
	rastState.depth_clip_enable = true;
	rastState.multisample_enable = false;
	rastState.antialiased_line_enable = false;
	
	// depth stencil state
	static DepthStencilState depthStateOpaque = {};
	depthStateOpaque.depth_enable = true;
	depthStateOpaque.depth_func = ComparisonFunc::GREATER_EQUAL;
	depthStateOpaque.stencil_enable = false;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ALL;
	
	// blend state
	static BlendState blendStateOpaque = {};
	blendStateOpaque.render_target[0].blend_enable = false;
	blendStateOpaque.render_target[0].src_blend = Blend::ONE;
	blendStateOpaque.render_target[0].dest_blend = Blend::ZERO;
	blendStateOpaque.render_target[0].blend_op = BlendOp::ADD;
	blendStateOpaque.render_target[0].src_blend_alpha = Blend::ONE;
	blendStateOpaque.render_target[0].dest_blend_alpha = Blend::ZERO;
	blendStateOpaque.render_target[0].blend_op_alpha = BlendOp::ADD;
	blendStateOpaque.render_target[0].render_target_write_mask = ColorWrite::ENABLE_ALL;
	blendStateOpaque.independent_blend_enable = false;
	blendStateOpaque.alpha_to_coverage_enable = true;
	
	// input layout
	static InputLayout inputLayout;
	inputLayout.elements = {
		{ "POSITION", 0, wiGraphics::Format::R32G32_FLOAT,    0, 0,  InputClassification::PER_VERTEX_DATA },
		{ "OFFSET",   0, wiGraphics::Format::R32G32B32_FLOAT, 1, 0,  InputClassification::PER_INSTANCE_DATA },
		{ "DATA",     0, wiGraphics::Format::R32_UINT,        1, 12, InputClassification::PER_INSTANCE_DATA },
	};

	// pipeline state object
	PipelineStateDesc desc = {};
	desc.vs = &shaderGrassVS;
	desc.ps = &shaderGrassPS;

	desc.il = &inputLayout;
	desc.pt = PrimitiveTopology::TRIANGLELIST;
	desc.rs = &rastState;
	desc.dss = &depthStateOpaque;
	desc.bs = &blendStateOpaque;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ZERO;
	device->CreatePipelineState( &desc, &psoGrass );

	// prepass pipeline state
	desc.vs = &shaderGrassPrepassVS;
	desc.ps = &shaderGrassPrepassPS;
	desc.il = &inputLayout;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ALL;
	device->CreatePipelineState( &desc, &psoGrassPrepass );

	// shadow pipeline state
	rastState.depth_bias = -1;
	rastState.slope_scaled_depth_bias = -4.0f;
	rastState.multisample_enable = false;
	desc.vs = &shaderGrassShadowVS;
	desc.ps = &shaderGrassShadowPS;
	desc.il = &inputLayout;
	rastState.depth_clip_enable = false;
	blendStateOpaque.alpha_to_coverage_enable = false;
	device->CreatePipelineState( &desc, &psoGrassShadow );
	rastState.depth_bias = 0;
	rastState.depth_clip_enable = true;
	rastState.slope_scaled_depth_bias = 0;

	// samplers
	SamplerDesc samplerDesc;
	samplerDesc.address_u = TextureAddressMode::CLAMP;
	samplerDesc.address_v = TextureAddressMode::CLAMP;
	samplerDesc.filter = Filter::MIN_MAG_MIP_LINEAR;
	device->CreateSampler( &samplerDesc, &samplerTrilinearClamp );

	samplerDesc.address_u = TextureAddressMode::WRAP;
	samplerDesc.address_v = TextureAddressMode::WRAP;
	samplerDesc.filter = Filter::MIN_MAG_MIP_LINEAR;
	device->CreateSampler( &samplerDesc, &samplerTrilinearWrap );

	samplerDesc.address_u = TextureAddressMode::WRAP;
	samplerDesc.address_v = TextureAddressMode::WRAP;
	samplerDesc.filter = Filter::MIN_MAG_MIP_POINT;
	device->CreateSampler( &samplerDesc, &samplerPointWrap );

	// constant buffer
	GPUBufferDesc bd = {};
	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(GrassCB);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed in DX12 API
	bd.misc_flags = ResourceMiscFlag::NONE;
	wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &grassConstantBuffer );

	// vertex buffer
	GPUBufferDesc bufferDesc = {};
	SubresourceData data = {};
	data.data_ptr = g_VerticesGrass;
	bufferDesc.size = sizeof(g_VerticesGrass);
	bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
	//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
	bufferDesc.misc_flags = ResourceMiscFlag::NONE;
	wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferGrassVertices );

	// index buffer
	data.data_ptr = g_IndicesGrass;
	bufferDesc.size = sizeof(g_IndicesGrass);
	bufferDesc.bind_flags = BindFlag::INDEX_BUFFER;
	//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
	bufferDesc.misc_flags = ResourceMiscFlag::NONE;
	wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferGrassIndices );
}

int GGGrass_UsingBrush()
{
	return true;
}

// GGMAX Tier A (2026-08-02): the 1024x1024 x GGGRASS_NUM_TYPES blade atlas (63.2 MB) is dead
// weight on the shipping path, so it is allocated on demand instead of at init.
//
// The proof it is dead is three-fold and static:
//   * it is created EMPTY, and its only writer — GGGrass_LoadTextureDDSIntoSlice — is a no-op
//     stub ("stub - DDS loading disabled"); the real implementation is inside an `#if 0`,
//   * its only readers are GGGrass_BindGrassArray (called solely from GGTerrain's own draw) and
//     GGGrass_Draw_Prepass, and every customDraw_* callback in master_part1.cpp early-returns
//     while ggterrain_use_wicked_terrain is set — which it is on the shipping path,
//   * shipping grass renders through Wicked's hair particle system, which samples the per-type
//     material textures directly, not this atlas.
//
// Same treatment and same reasoning as the 1.70 GGTerrain page-atlas fix. Creation is hooked to
// the legacy path's activation point (the Y-key toggle tail of GGTerrain_Update) so that if the
// old path is ever switched on at runtime the atlas exists before anything binds it.
static bool g_grassLegacyTexArrayCreated = false;
void GGGrass_EnsureLegacyTexArray()
{
	if (g_grassLegacyTexArrayCreated) return;
	if (!gggrass_initialised) return;
	g_grassLegacyTexArrayCreated = true;
	GGGrass_CreateEmptyTexture(1024, 1024, 9, GGGRASS_NUM_TYPES, Format::BC3_UNORM_SRGB, &texGrass);
}

void GGGrass_BindGrassArray( uint32_t slot, wiGraphics::CommandList cmd )
{
	if (!gggrass_initialised) return;
	GGGrass_EnsureLegacyTexArray();
	wiGraphics::GetDevice()->BindResource( &texGrass, slot, cmd );
}

uint32_t GGGrass_GetGrassMap( float x, float z )
{
	if (!gggrass_initialised) return 0;
	float fX = x / ggterrain_global_render_params2.editable_size;
	fX = fX * 0.5f + 0.5f;
	fX *= GGGRASS_MAP_SIZE;

	float fZ = z / ggterrain_global_render_params2.editable_size;
	fZ = fZ * 0.5f + 0.5f;
	fZ *= GGGRASS_MAP_SIZE;

	int iX = (int) fX;
	int iZ = (int) fZ;

	if ( iX >= 0 && iZ >= 0 && iX < GGGRASS_MAP_SIZE && iZ < GGGRASS_MAP_SIZE )
	{
		uint32_t index = iZ * GGGRASS_MAP_SIZE + iX;
		uint32_t value = pGrassMap[ index ];
		if ( value & 0x80 ) return 0; // flattened
		value &= 0x7F;
		if ( value > 0 ) return value;
	}

	return 0;
}

// not for external use
void GGGrass_SetGrassMap( float x, float z, uint8_t grassID )
{
	if (!gggrass_initialised) return;
	float fX = x / ggterrain_global_render_params2.editable_size;
	fX = fX * 0.5f + 0.5f;
	fX *= GGGRASS_MAP_SIZE;

	float fZ = z / ggterrain_global_render_params2.editable_size;
	fZ = fZ * 0.5f + 0.5f;
	fZ *= GGGRASS_MAP_SIZE;

	int iX = (int) fX;
	int iZ = (int) fZ;

	if ( iX >= 0 && iZ >= 0 && iX < GGGRASS_MAP_SIZE && iZ < GGGRASS_MAP_SIZE )
	{
		uint32_t index = iZ * GGGRASS_MAP_SIZE + iX;
		pGrassMap[ index ] &= 0x80;
		pGrassMap[ index ] |= grassID;
	}
}

void GGGrass_SetPerformanceMode( uint32_t mode )
{
	switch( mode )
	{
		case GGTERRAIN_PERFORMANCE_LOW:
		{
			gggrass_global_params.simplePBR = 1;
		} break;

		case GGTERRAIN_PERFORMANCE_MED:
		{
			gggrass_global_params.simplePBR = 1;
		} break;

		case GGTERRAIN_PERFORMANCE_HIGH:
		{
			gggrass_global_params.simplePBR = 0;
		} break;

		case GGTERRAIN_PERFORMANCE_ULTRA:
		{
			gggrass_global_params.simplePBR = 0;
		} break;
	}
}

void GGGrass_AddAll()
{
	if (!gggrass_initialised) return;
	uint32_t numTypes = 0;
	uint64_t values = gggrass_global_params.paint_type;
	uint32_t currMat = gggrass_global_params.paint_material;

	for( uint32_t i = 0; i < GGGRASS_NUM_SELECTABLE_TYPES; i++ )
	{
		if ( values & 1 ) numTypes++;
		values >>= 1;
	}

	// ZJ: Prevent crash when numTypes is 0
	if (numTypes == 0)
		return;

	for( uint32_t z = 0; z < GGGRASS_MAP_SIZE; z++ )
	{
		for( uint32_t x = 0; x < GGGRASS_MAP_SIZE; x++ )
		{
			uint32_t index = z * GGGRASS_MAP_SIZE + x;

			uint32_t density = index;
			density = 214313*density + 2539011;
			density = 214313*density + 2539011;
			density = 214313*density + 2539011;
			density ^= (density >> 16);
			density ^= (density >> 8);
			density %= 100;
			
			if ( density >= (uint32_t) gggrass_global_params.paint_density )
			{
				pGrassMap[index] &= 0x80; // clear but keep flatten state
			}
			else
			{
				if ( currMat == 0 )
				{
					pGrassMap[index] &= 0x80;
					pGrassMap[index] |= 1;
				}
				else
				{
					uint32_t type = index;
					type = 214013*type + 2531011;
					type = 214013*type + 2531011;
					type = 214013*type + 2531011;
					type ^= (type >> 16);
					type ^= (type >> 8);
					type %= numTypes;
					uint32_t count = 0;
					uint64_t values = gggrass_global_params.paint_type;
					for( uint32_t i = 0; i < GGGRASS_NUM_SELECTABLE_TYPES; i++ )
					{
						if ( values & 1 ) 
						{
							if ( count == type )
							{
								uint32_t realIndex = 0;
								realIndex = GGGrass_GetRealIndex( currMat-1, i );
							
								pGrassMap[ index ] &= 0x80; // keep flattened state
								pGrassMap[ index ] |= realIndex + 2;
								break;
							}
							count++;
						}
						values >>= 1;
					}
				}
			}
		}
	}

	ggterrain_extra_params.iUpdateGrass = 5;

	// Wicked mode: pGrassMap has been rewritten wholesale, but the hair entity cache is per-chunk
	// and won't notice on its own — and the R8_UNORM texGrassMap on the GPU still holds the pre-fill
	// bytes. Push both signals so the Wicked side re-uploads the texture (Option B CS visibility
	// check) and drops every existing entity so ProcessGrassChunks recreates them from the new map.
	GGGrass_UploadGrassMap();
	s_gggrass_full_rebuild_pending = true;
}

void GGGrass_RemoveAll()
{
	if (!gggrass_initialised) return;
	for( uint32_t z = 0; z < GGGRASS_MAP_SIZE; z++ )
	{
		for( uint32_t x = 0; x < GGGRASS_MAP_SIZE; x++ )
		{
			uint32_t index = z * GGGRASS_MAP_SIZE + x;
			pGrassMap[ index ] &= 0x80; // keep flattened state
		}
	}

	GGGrass_UpdateInstances();

	// Wicked mode: same story as GGGrass_AddAll — bulk pGrassMap write that the paint-stroke
	// dirty-chunk pipeline never sees. Upload the cleared bytes so the CS visibility check
	// hides every strand, and drop every hair entity so the scene is genuinely empty (not just
	// visually empty with dormant strand geometry still consuming VRAM).
	GGGrass_UploadGrassMap();
	s_gggrass_full_rebuild_pending = true;
}

void GGGrass_RestoreAll()
{
	// no longer does anything
	/*
	for( uint32_t z = 0; z < GGGRASS_MAP_SIZE; z++ )
	{
		for( uint32_t x = 0; x < GGGRASS_MAP_SIZE; x++ )
		{
			uint32_t index = z * GGGRASS_MAP_SIZE + x;
			pGrassMap[ index ] &= 0x80; // keep flattened state
			pGrassMap[ index ] |= 0x01;
		}
	}

	GGGrass_UpdateInstances();
	*/
}

uint32_t GGGrass_GetDataSize()
{
	return GGGRASS_MAP_SIZE * GGGRASS_MAP_SIZE * sizeof(uint8_t);
}

int GGGrass_GetData( uint8_t* data )
{
	if (!gggrass_initialised) return 0;
	uint32_t size = GGGRASS_MAP_SIZE * GGGRASS_MAP_SIZE * sizeof(uint8_t);
	memcpy( data, pGrassMap, size );

	return 1;
}

int GGGrass_SetData( uint32_t size, uint8_t* data, sUndoSysEventGrass* pEvent)
{
	if (!gggrass_initialised) return 0;
	if(!pEvent)
	{ 
		uint32_t size1 = GGGRASS_MAP_SIZE * GGGRASS_MAP_SIZE * sizeof(uint8_t);

		if (size1 != size) return 0;
		memcpy(pGrassMap, data, size1);

		GGGrass_UploadGrassMap();
#ifdef ONLYLOADWHENUSED
		for (uint32_t y = 0; y < GGGRASS_MAP_SIZE; y++)
		{
			for (uint32_t x = 0; x < GGGRASS_MAP_SIZE; x++)
			{
				uint32_t index = y * GGGRASS_MAP_SIZE + x;
				uint8_t grassID = pGrassMap[index] & 0x7F;
				if (grassID > 2)
				{
					bGrassTextureForceUploaded[grassID-2] = true;
				}
			}
		}
		last_paint_type = -1;
#endif

		return 1;
	}
	
#ifdef GGGRASS_UNDOREDO
	for (int y = pEvent->minY; y < pEvent->maxY; y++)
	{
		for (int x = pEvent->minX; x < pEvent->maxX; x++)
		{
			int indexGrassMap = y * GGGRASS_MAP_SIZE + x;
			int indexSavedData = (pEvent->maxX - pEvent->minX) * (y - pEvent->minY) + (x - pEvent->minX);
			pGrassMap[indexGrassMap] = pEvent->grassData[indexSavedData];
		}
	}

	GGGrass_UpdateInstances();
#endif
	return 1;
}

void GGGrass_UpdateFlatArea( int mode, int type, float posX, float posZ, float sx, float sz, float angle )
{
	if (!gggrass_initialised) return;
	if ( !pGrassMap ) return;

	float realMinX = 1e20f;
	float realMinZ = 1e20f;
	float realMaxX = -1e20f;
	float realMaxZ = -1e20f;

	if ( type == 0 )
	{
		float ca = cos( angle * 3.14156265358979f / 180.0f );
		float sa = sin( angle * 3.14156265358979f / 180.0f );

		for( int x = -1; x < 2; x += 2 )
		{
			for( int z = -1; z < 2; z += 2 )
			{
				float x1 = sx*x/2.0f;
				float z1 = sz*z/2.0f;

				// clockwise rotation
				float xr = x1 * ca + z1 * sa;
				float zr = z1 * ca - x1 * sa;

				xr += posX;
				zr += posZ;

				if ( xr < realMinX ) realMinX = xr;
				if ( xr > realMaxX ) realMaxX = xr;
				if ( zr < realMinZ ) realMinZ = zr;
				if ( zr > realMaxZ ) realMaxZ = zr;
			}
		}
	}
	else if ( type == 1 )
	{
		float realMinX = posX - (sx / 2.0f);
		float realMinZ = posZ - (sx / 2.0f);
		float realMaxX = posX + (sx / 2.0f);
		float realMaxZ = posZ + (sx / 2.0f);
	}

	float fMinX = realMinX / ggterrain_global_render_params2.editable_size;
	fMinX = fMinX * 0.5f + 0.5f;
	fMinX *= GGGRASS_MAP_SIZE;
	float fMinZ = realMinZ / ggterrain_global_render_params2.editable_size;
	fMinZ = fMinZ * 0.5f + 0.5f;
	fMinZ *= GGGRASS_MAP_SIZE;
	float fMaxX = realMaxX / ggterrain_global_render_params2.editable_size;
	fMaxX = fMaxX * 0.5f + 0.5f;
	fMaxX *= GGGRASS_MAP_SIZE;
	float fMaxZ = realMaxZ / ggterrain_global_render_params2.editable_size;
	fMaxZ = fMaxZ * 0.5f + 0.5f;
	fMaxZ *= GGGRASS_MAP_SIZE;

	int minX = 0;
	int minY = 0;
	int maxX = GGGRASS_MAP_SIZE;
	int maxY = GGGRASS_MAP_SIZE;

	if ( fMinX > 0 ) minX = (int) fMinX;
	if ( fMinZ > 0 ) minY = (int) fMinZ;
	if ( fMaxX < (float) GGGRASS_MAP_SIZE ) maxX = (int) fMaxX;
	if ( fMaxZ < (float) GGGRASS_MAP_SIZE ) maxY = (int) fMaxZ;

	// don't allow drawing of the border pixels so clamp mode will always read 0
	if ( maxX >= 1 && maxY >= 1 && minX < GGGRASS_MAP_SIZE-2 && minY < GGGRASS_MAP_SIZE-2 )
	{
		if ( minX < 1 ) minX = 1;
		if ( minY < 1 ) minY = 1;
		if ( maxX > GGGRASS_MAP_SIZE-1 ) maxX = GGGRASS_MAP_SIZE-1;
		if ( maxY > GGGRASS_MAP_SIZE-1 ) maxY = GGGRASS_MAP_SIZE-1;

		for( int y = minY; y < maxY; y++ )
		{
			for( int x = minX; x < maxX; x++ )
			{
				float fX = (float) x;
				fX = fX / GGGRASS_MAP_SIZE;
				fX = fX * 2 - 1;
				fX = fX * ggterrain_global_render_params2.editable_size;

				float fZ = (float) y;
				fZ = fZ / GGGRASS_MAP_SIZE;
				fZ = fZ * 2 - 1;
				fZ = fZ * ggterrain_global_render_params2.editable_size;

				if ( type == 0 )
				{
					float ca = cos( angle * 3.14156265358979f / 180.0f );
					float sa = sin( angle * 3.14156265358979f / 180.0f );

					float fpX = fX - posX;
					float fpZ = fZ - posZ;

					float faX = fpX * ca - fpZ * sa;
					float faZ = fpX * sa + fpZ * ca;

					float halfX = sx / 2.0f;
					float halfZ = sz / 2.0f;
					if ( faX < -halfX ) continue;
					if ( faX >  halfX ) continue;
					if ( faZ < -halfZ ) continue;
					if ( faZ >  halfZ ) continue;
				}
				else if ( type == 1 )
				{
					float radius = sz / 2.0f;
					float diffX = posX - fX;
					float diffY = posZ - fZ;
					float dist = diffX*diffX + diffY*diffY;
					if ( dist > radius*radius ) continue;
				}

				uint32_t index = y * GGGRASS_MAP_SIZE + x;

				if ( mode == 0 ) pGrassMap[ index ] |= 0x80; // remove grass
				else pGrassMap[ index ] &= 0x7F; // restore grass
			}
		}

		GGGrass_UploadGrassMap();
	}

	if ( gggrass_defer_instance_updates > 0 ) gggrass_instance_update_pending = true;
	else GGGrass_UpdateInstances();
}

// rebuilding the grass instances places every one again (numTotalGrass), so a run of flat area changes, such as a
// level load or a flat area that is moved and then cleared again by the areas overlapping it, rebuilds them once
void GGGrass_DeferInstanceUpdates( int defer )
{
	if ( defer )
	{
		gggrass_defer_instance_updates++;
		return;
	}
	if ( gggrass_defer_instance_updates > 0 ) gggrass_defer_instance_updates--;
	if ( gggrass_defer_instance_updates == 0 && gggrass_instance_update_pending )
	{
		gggrass_instance_update_pending = false;
		GGGrass_UpdateInstances();
	}
}

void GGGrass_RestoreAllFlattened()
{
	if (!gggrass_initialised) return;
	if ( !pGrassMap ) return;

	for( uint32_t y = 0; y < GGGRASS_MAP_SIZE; y++ )
	{
		for( uint32_t x = 0; x < GGGRASS_MAP_SIZE; x++ )
		{
			uint32_t index = y * GGGRASS_MAP_SIZE + x;
			pGrassMap[ index ] &= 0x7F;
		}
	}

	GGGrass_UpdateInstances();
}

void GGGrass_Update_Painting( RAY ray )
{
	if (!gggrass_initialised) return;
	float pickX = 0, pickY = 0, pickZ = 0;
	int pickHit = GGTerrain_RayCast( ray, &pickX, &pickY, &pickZ, 0, 0, 0, 0 );

	float fX = pickX / ggterrain_global_render_params2.editable_size;
	fX = fX * 0.5f + 0.5f;
	fX *= GGGRASS_MAP_SIZE;

	float fZ = pickZ / ggterrain_global_render_params2.editable_size;
	fZ = fZ * 0.5f + 0.5f;
	fZ *= GGGRASS_MAP_SIZE;

	if ( gggrass_internal_params.mouseLeftPressed )
	{
		
	}

	if ( gggrass_internal_params.mouseLeftState )
	{
		#ifdef GGGRASS_UNDOREDO
		if (g_iCalculatingChangeBounds == 0)
		{
			// Reset the calculated bounding box, ready for the next undo action.
			// minX and minY set higher than they can be, so the < comparison is always correct.
			g_EditBounds.minX = 50000;
			g_EditBounds.minY = 50000;
			g_EditBounds.maxX = 0;
			g_EditBounds.maxY = 0;
			// Take a snapshot of how the grass is before any edits take place.
			GGGrass_GetData(g_pTerrainSnapshot);
			g_iCalculatingChangeBounds = 1;
		}
		#endif
		float radius = ggterrain_global_render_params2.brushSize / ggterrain_global_render_params2.editable_size;
		radius *= GGGRASS_MAP_SIZE * 0.5f;

		int startX = (int) (fX - radius);
		int startY = (int) (fZ - radius);
		int endX = (int) (fX + radius);
		int endY = (int) (fZ + radius);

		uint32_t numTypes = 0;
		uint64_t values = gggrass_global_params.paint_type;
		for( uint32_t i = 0; i < GGGRASS_MAX_PALETTE_SLOTS; i++ )
		{
			if ( values & 1 ) numTypes++;
			values >>= 1;
		}

		// don't allow drawing of the border pixels so clamp mode will always read 0
		if ( endX >= 1 && endY >= 1 && startX < GGGRASS_MAP_SIZE-2 && startY < GGGRASS_MAP_SIZE-2 )
		{
			if ( startX < 1 ) startX = 1;
			if ( startY < 1 ) startY = 1;
			if ( endX > GGGRASS_MAP_SIZE-1 ) endX = GGGRASS_MAP_SIZE-1;
			if ( endY > GGGRASS_MAP_SIZE-1 ) endY = GGGRASS_MAP_SIZE-1;

			#ifdef GGGRASS_UNDOREDO
			if (g_iCalculatingChangeBounds == 1)
			{
				undosys_terrain_editsize(startX, endX, startY, endY);
			}
			#endif

			for( int y = startY; y < endY; y++ )
			{
				for( int x = startX; x < endX; x++ )
				{
					float diffX = x - fX;
					float diffY = y - fZ;
					float dist = diffX*diffX + diffY*diffY;
					if ( dist > radius*radius ) continue;

					uint32_t index = y * GGGRASS_MAP_SIZE + x;

					if ( ggterrain_extra_params.edit_mode != GGTERRAIN_EDIT_GRASS )
					{
						uint8_t grassID = pGrassMap[ index ] & 0x7F;
						uint8_t flattened = pGrassMap[ index ] & 0x80;

						// terrain painting may update grass
						if ( gggrass_global_params.paint_material == 0 && grassID > 0 )
						{
							float fX = (float) x;
							fX = fX / GGGRASS_MAP_SIZE;
							fX = fX * 2 - 1;
							fX = fX * ggterrain_global_render_params2.editable_size;

							float fZ = (float) y;
							fZ = fZ / GGGRASS_MAP_SIZE;
							fZ = fZ * 2 - 1;
							fZ = fZ * ggterrain_global_render_params2.editable_size;

							int material = GGTerrain_GetMaterialIndex( fX, fZ );

							// get current grass ID and replace it 
							if ( grassID > 1 )
							{
								grassID -= 2;
								// GGMAX 3.106: a CUSTOM palette grass (byte = slot + 26, real type >= GGGRASS_NUM_TYPES)
								// has no per-material variants - leave it as painted. Indexing grassSourceTypes[46]
								// with it read past the array and swapped the custom grass for a stock one.
								if ( grassID >= GGGRASS_NUM_TYPES ) continue;
								int grassSourceType = grassSourceTypes[ grassID ];
								grassID = GGGrass_GetRealIndex( material, grassSourceType );
							}
							else // equals 1
							{
								grassID = GGGrass_GetRealIndex( material, 0 );
							}

							if ( grassID >= GGGRASS_NUM_TYPES ) grassID = 0;
							else grassID += 2;

							pGrassMap[ index ] = flattened | grassID;
						}
					}
					else
					{
						switch( gggrass_global_params.paint_mode )
						{
							case GGGRASS_PAINT_SPRAY:
							{
								if ( numTypes == 0 ) pGrassMap[ index ] &= 0x80; // keep flattened state
								else
								{
									uint32_t density = index;
									density = 214313*density + 2539011;
									density = 214313*density + 2539011;
									density = 214313*density + 2539011;
									density ^= (density >> 16);
									density ^= (density >> 8);
									density %= 100;
			
									if ( density >= (uint32_t) gggrass_global_params.paint_density )
									{
										pGrassMap[ index ] &= 0x80; // keep flattened state
									}
									else
									{
										uint32_t type = index;
										type = 214013*type + 2531011;
										type = 214013*type + 2531011;
										type = 214013*type + 2531011;
										type ^= (type >> 16);
										type ^= (type >> 8);
										type %= numTypes;
										uint32_t count = 0;
										uint64_t values = gggrass_global_params.paint_type;
										// Stage B.9: iterate the full palette (stock 0..21 + custom
										// 22..GGGRASS_MAX_PALETTE_SLOTS-1). Custom slots skip the
										// grassMaterialTypes lookup and encode directly: real_type =
										// slot + 24 so byte = slot + 26 stays disjoint from stock
										// bytes (0..47) and below the 0x80 flatten bit.
										for( uint32_t i = 0; i < GGGRASS_MAX_PALETTE_SLOTS; i++ )
										{
											if ( values & 1 )
											{
												if ( count == type )
												{
													uint32_t realIndex = 0;
													if ( i >= GGGRASS_CUSTOM_SLOT_BASE )
													{
														// Custom slot: direct real_type encoding - byte = slot + 26 (at most 89,
														// below the 0x80 flatten bit).
														realIndex = (uint32_t)i + 24 + 2;
													}
													else
													{
														// Stock slot: existing terrain-material variant lookup.
														uint32_t currMat = gggrass_global_params.paint_material;
														if ( currMat > 0 )
														{
															// don't match, use material 0
															realIndex = GGGrass_GetRealIndex( 0, i );
														}
														else
														{
															// match terrain material
															float fX = (float) x;
															fX = fX / GGGRASS_MAP_SIZE;
															fX = fX * 2 - 1;
															fX = fX * ggterrain_global_render_params2.editable_size;

															float fZ = (float) y;
															fZ = fZ / GGGRASS_MAP_SIZE;
															fZ = fZ * 2 - 1;
															fZ = fZ * ggterrain_global_render_params2.editable_size;

															int material = GGTerrain_GetMaterialIndex( fX, fZ );
															realIndex = GGGrass_GetRealIndex( material, i );
														}
														// GGMAX 3.118: DX11 parity (DX11 GGGrass.cpp:1523-1524) - a stock grass painted with
														// Match Terrain Color on ground with no matching variant (255: sand, rock...) writes
														// EMPTY. Stage B.9 capped the 255 to real type 0 and wrote Course Grass there.
														// (Match off looks up material 0, which always has a variant.)
														if ( realIndex >= GGGRASS_NUM_TYPES ) realIndex = 0;
														else realIndex += 2;
													}

													// Byte encoding: 0 = empty, 1 = auto, 2+ = real_type + 2, bit 7 = flattened.

													//pGrassMap[ index ] &= 0x80; // keep flattened state
													pGrassMap[ index ] = 0; // allow painting in flattened areas
													pGrassMap[ index ] |= realIndex;
													break;
												}
												count++;
											}
											values >>= 1;
										}
									}
								}							
							} break;

							case GGGRASS_PAINT_SPRAY_REMOVE:
							{
								pGrassMap[ index ] &= 0x80; // keep flattened state
							} break;
						}
					}
				}
			}

			// Accumulate the brush AABB into the in-progress stroke. Publish to the Wicked-side
			// dirty flag every 6 paint frames (~10x/sec at 60 fps) so the user sees responsive
			// mid-stroke updates, and again at stroke end. Stage 3 Option B: Wicked-mode grass
			// now reads the GPU texGrassMap per-strand in the hair simulate CS, so we must
			// refresh it mid-stroke in BOTH modes. 16 MB upload @ 10 Hz = ~160 MB/s — negligible.
			float bs = ggterrain_global_render_params2.brushSize;
			GGGrass_AccumStrokeRect( pickX - bs, pickZ - bs, pickX + bs, pickZ + bs );
			gggrass_stroke_frames_since_drain++;
			if ( gggrass_stroke_frames_since_drain >= 6 )
			{
				GGGrass_UploadGrassMap();
				GGGrass_PublishStrokeRect();
			}
		}

		// GGGrass_UpdateInstances regenerates 64 * 6250 = 400000 random instances each call, every
		// one of them acquiring terrainlock twice (GetNormal + GetHeight). It feeds the LEGACY GG
		// grass shader. In Wicked mode that shader renders nothing, so this is pure overhead — and
		// the more painted cells exist the more full-pipeline iterations run, which compounded into
		// a 70->12 FPS drop during a 20 sec paint hold. Skip when Wicked is active.
		if (!ggterrain_use_wicked_terrain)
		{
			GGGrass_UpdateInstances();
		}
	}

#ifdef GGGRASS_UNDOREDO
	if (gggrass_internal_params.mouseLeftReleased && g_iCalculatingChangeBounds)
	{
		// Create an undo item here.
		g_iCalculatingChangeBounds = 0;

		GGGrass_CreateUndoRedoAction(eUndoSys_Terrain_Grass, eUndoSys_UndoList, true);
	}
#endif

	// Stroke ended — upload the final grass map to the legacy GPU texture and flush the accumulated
	// paint rect so the user sees the final result.
	if ( gggrass_internal_params.mouseLeftReleased )
	{
		if ( gggrass_stroke_in_progress )
		{
			GGGrass_UploadGrassMap();
		}
		GGGrass_PublishStrokeRect();
	}
}


void GGGrass_Update( wiScene::CameraComponent* camera, CommandList cmd, bool bRenderTargetFocus)
{
	if (!gggrass_initialised) return;
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	if ( !gggrass_global_params.draw_enabled ) return;

#ifdef ONLYLOADWHENUSED
	if (last_paint_type != gggrass_global_params.paint_type)
	{
		GGGrass_LoadTextures(false,false);
		last_paint_type = gggrass_global_params.paint_type;
	}
#endif

	//auto range = wiProfiler::BeginRangeCPU( "Max - Grass Update" );

	if ( gggrass_global_params.max_height < gggrass_global_params.min_height ) gggrass_global_params.max_height = gggrass_global_params.min_height;
	if ( gggrass_global_params.max_height_underwater < gggrass_global_params.min_height_underwater ) gggrass_global_params.max_height_underwater = gggrass_global_params.min_height_underwater;

	if ( (ggterrain_extra_params.edit_mode == GGTERRAIN_EDIT_GRASS 
		 || (ggterrain_extra_params.edit_mode == GGTERRAIN_EDIT_PAINT && gggrass_global_params.paint_material == 0))
		 && !bImGuiGotFocus )
	{
		wiInput::MouseState mouseState = wiInput::GetMouseState();
		gggrass_internal_params.mouseLeftState = mouseState.left_button_press;
		gggrass_internal_params.mouseLeftPressed = (mouseState.left_button_press && !gggrass_internal_params.prevMouseLeft) ? 1 : 0;
		gggrass_internal_params.mouseLeftReleased = (!mouseState.left_button_press && gggrass_internal_params.prevMouseLeft) ? 1 : 0;
		gggrass_internal_params.prevMouseLeft = mouseState.left_button_press ? 1 : 0;

		RAY pickRay = wiRenderer::GetPickRay( (long)mouseState.position.x, (long)mouseState.position.y, master.masterrenderer );

		if(bRenderTargetFocus)
			GGGrass_Update_Painting( pickRay );
	}

	grassCameraX = camera->Eye.x;
	grassCameraY = camera->Eye.y;
	grassCameraZ = camera->Eye.z;

	if ( gggrass_global_params.lod_dist != gggrass_internal_params.prevLodDist )
	{
		gggrass_internal_params.prevLodDist = gggrass_global_params.lod_dist;

		grassRadius = gggrass_global_params.lod_dist + (float) GGGRASS_LOD_TRANSITION;
		grassAreaPerChunk = grassRadius * 2 / (grassSplit-2);

		for( uint32_t i = 0; i < numGrassChunks; i++ )
		{
			int x = i % grassSplit;
			int z = i / grassSplit;

			x -= grassSplit / 2;
			z -= grassSplit / 2;

			pGrassChunks[ i ].UpdatePosition( grassGridOffsetX + x, grassGridOffsetZ + z );
			pGrassGrid[ i ] = i;
		}
	}

	float gridX = grassGridOffsetX * grassAreaPerChunk;
	float gridZ = grassGridOffsetZ * grassAreaPerChunk;

	float diffX = grassCameraX - gridX;
	float diffZ = grassCameraZ - gridZ;

	float distCheck = grassAreaPerChunk * 0.75f;

	int iShiftedX = 0;
	int iShiftedZ = 0;

	if ( abs(diffX) > distCheck ) 
	{
		// move grass grid in X direction
		iShiftedX = (int) round( diffX / grassAreaPerChunk );
	}

	if ( abs(diffZ) > distCheck )
	{
		// move grass grid in Z direction
		iShiftedZ = (int) round( diffZ / grassAreaPerChunk );
	}

	if (abs(iShiftedX) > 10 || abs(iShiftedZ) > 10)
	{
		//PE: When moving a long way it dont update everything (grass floating), reset.
		//PE: Unfortunally terrain is generating at this point, so need to wait a few frames.
		iTriggerGrassTreeUpdate = 40; //PE: Make sure trees and grass height is set after terrain has finish.
	}

	if ( iShiftedX != 0 || iShiftedZ != 0 )
	{
		grassGridOffsetX += iShiftedX;
		grassGridOffsetZ += iShiftedZ;

		StackArray<uint8_t> unusedChunks;
		// collect old chunks
		for( int z = 0; z < grassSplit; z++ )
		{
			int newZ = z - iShiftedZ;
			for( int x = 0; x < grassSplit; x++ )
			{
				int index = z * grassSplit + x;
				int newX = x - iShiftedX;
				if ( newZ < 0 || newZ >= grassSplit || newX < 0 || newX >= grassSplit ) 
				{
					unusedChunks.PushItem( pGrassGrid[index] );
				}
			}
		}

		uint8_t newGrid[ numGrassChunks ];

		// assign new chunks
		int halfGrid = grassSplit / 2;
		for( int z = 0; z < grassSplit; z++ )
		{
			int oldZ = z + iShiftedZ;
			for( int x = 0; x < grassSplit; x++ )
			{
				int index = z * grassSplit + x;
				int oldX = x + iShiftedX;
				if ( oldZ < 0 || oldZ >= grassSplit || oldX < 0 || oldX >= grassSplit ) 
				{
					assert( unusedChunks.NumItems() > 0 );
					uint8_t grassIndex = unusedChunks.PopItem();
					newGrid[ index ] = grassIndex;
					pGrassChunks[ grassIndex ].UpdatePosition( grassGridOffsetX + x - halfGrid, grassGridOffsetZ + z - halfGrid );
				}
				else
				{
					int oldIndex = oldZ * grassSplit + oldX;
					uint8_t grassIndex = pGrassGrid[ oldIndex ];
					newGrid[ index ] = grassIndex;
					//pGrassChunks[ grassIndex ].UpdatePosition( grassGridOffsetX + x - halfGrid, grassGridOffsetZ + z - halfGrid );
				}
			}
		}

		memcpy( pGrassGrid, newGrid, sizeof(pGrassGrid) );
	}
	
	if ( ggterrain_extra_params.iUpdateGrass > 0 )
	{
		ggterrain_extra_params.iUpdateGrass--;
		if ( ggterrain_extra_params.iUpdateGrass == 0 )
		{
			// something changed so adjust grass heights
			if ( !GGGrass_UpdateInstances() ) ggterrain_extra_params.iUpdateGrass = 5;
		}
	}

	// update shader constants buffer
	float ang = 0.19634954f; // (2.0 * pi) / 32.0
	for( int i = 0; i < 32; i++ )
	{
		float c = cos( ang * i );
		float s = sin( ang * i );
		grassConstantData.grass_rotMat[ i ].x = c;
		grassConstantData.grass_rotMat[ i ].y = -s;
		grassConstantData.grass_rotMat[ i ].z = s;
		grassConstantData.grass_rotMat[ i ].w = c;
	}

	float time = GrassTimer();
	for( uint32_t i = 0; i < GGGRASS_NUM_TYPES; i++ ) 
	{
		grassConstantData.grass_type[ i ].scaleFactor = grassFiles[ i ].scaleFactor;
		grassConstantData.grass_type[ i ].cosTime = cos( time + (i * 0.1f) );
	}

	grassConstantData.grass_lodDist = gggrass_global_params.lod_dist;
	grassConstantData.grass_scale = gggrass_global_params.grass_scale;
	grassConstantData.grass_flags = 0;
	if ( gggrass_global_params.simplePBR ) grassConstantData.grass_flags |= GGGRASS_FLAGS_SIMPLE_PBR;
	
	wiGraphics::GetDevice()->UpdateBuffer( &grassConstantBuffer, &grassConstantData, cmd, sizeof(GrassCB) );
	// GGMAX 3.52: UpdateBuffer is a GPU COPY and inserts NO barrier - "appropriate synchronization is expected".
	// wiRenderer barriers every one of its own; the GG path was ported with none, so a buffer could still be COPY_DEST when the same list bound it (D3D12 id 538).
	{
		GPUBarrier gg_bar = GPUBarrier::Buffer( &grassConstantBuffer, ResourceState::COPY_DST, ResourceState::CONSTANT_BUFFER );
		wiGraphics::GetDevice()->Barrier( &gg_bar, 1, cmd );
	}

	//wiProfiler::EndRange( range );
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::Render()
extern "C" void GGGrass_Draw_Prepass( const Frustum* frustum, int mode, CommandList cmd )
{
	if (!gggrass_initialised) return;
	if ( !gggrass_global_params.draw_enabled ) return;

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGGrass Prepass Draw", cmd);
	// wiProfiler calls removed — they acquire a mutex that deadlocks with job system threads
	if ( mode != 0 ) return;  // no grass in reflection passes
		
	device->BindPipelineState( &psoGrassPrepass, cmd );

	uint32_t bindSlot = 3;
	device->BindConstantBuffer( &grassConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &grassConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	device->BindResource( &texGrass, 50, cmd );
	device->BindResource( &texNoise, 51, cmd );
	device->BindResource( &texGrassNormal, 53, cmd );
	device->BindSampler( &samplerPointWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferGrassVertices };
	const uint32_t strides[] = { sizeof( VertexGrass ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );

	float chunkRadius = sqrt( grassAreaPerChunk*grassAreaPerChunk*3 );
	float checkRadius = grassRadius + chunkRadius;
	
	for( uint32_t i = 0; i < numGrassChunks; i++ )
	{
		GrassChunk* pChunk = &pGrassChunks[ i ];
		if ( pChunk->numValid == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );

		float diffX = pChunk->centerX - grassCameraX;
		float diffY = (aabb._min.y + aabb._max.y) / 2.0f - grassCameraY;
		float diffZ = pChunk->centerZ - grassCameraZ;
		float dist = diffX*diffX + diffY*diffY + diffZ*diffZ;

		if ( dist > checkRadius*checkRadius ) continue;

		if ( !frustum->CheckBoxFast( aabb ) ) continue;

		const GPUBuffer* vbs[] = { &pChunk->bufferInstances };
		const uint32_t strides[] = { sizeof(InstanceGrass) };
		device->BindVertexBuffers( vbs, 1, 1, strides, 0, cmd );
		device->BindIndexBuffer( &bufferGrassIndices, IndexBufferFormat::UINT16, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}
	
	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine wiRenderer::DrawShadowmaps()
extern "C" void GGGrass_Draw_ShadowMap( const Frustum* frustum, int cascade, CommandList cmd )
{
	if (!gggrass_initialised) return;
	if ( !gggrass_global_params.draw_enabled ) return;
	if ( cascade >= gggrass_global_params.shadow_range ) return;

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGGrass Shadow Draw", cmd);

	device->BindPipelineState( &psoGrassShadow, cmd );

	uint32_t bindSlot = 3;
	device->BindConstantBuffer( &grassConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &grassConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	device->BindResource( &texGrass, 50, cmd );
	device->BindResource( &texNoise, 51, cmd );
	device->BindResource( &texGrassNormal, 53, cmd );
	device->BindSampler( &samplerPointWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferGrassVertices };
	const uint32_t strides[] = { sizeof( VertexGrass ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );

	for( uint32_t i = 0; i < numGrassChunks; i++ )
	{
		GrassChunk* pChunk = &pGrassChunks[ i ];
		if ( pChunk->numValid == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( !frustum->CheckBoxFast( aabb ) ) continue;

		const GPUBuffer* vbs[] = { &pChunk->bufferInstances };
		const uint32_t strides[] = { sizeof(InstanceGrass) };
		device->BindVertexBuffers( vbs, 1, 1, strides, 0, cmd );
		device->BindIndexBuffer( &bufferGrassIndices, IndexBufferFormat::UINT16, 0, cmd );
		device->DrawIndexedInstanced( 12, pChunk->numValid, 0, 0, 0, cmd );
	}

	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::Render()
extern "C" void GGGrass_Draw( const Frustum* frustum, int mode, CommandList cmd )
{
	if (!gggrass_initialised) return;
	if ( !gggrass_global_params.draw_enabled ) return;

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGGrass Draw", cmd);
	// wiProfiler calls removed — they acquire a mutex that deadlocks with job system threads
	if ( mode != 0 ) return;  // no grass in reflection passes
		
	device->BindPipelineState( &psoGrass, cmd );

	uint32_t bindSlot = 3;
	device->BindConstantBuffer( &grassConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &grassConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	// bind texture and sampler
	device->BindResource( &texGrass, 50, cmd );
	device->BindResource( &texNoise, 51, cmd );
	device->BindResource( &texGrassNormal, 53, cmd );
	device->BindSampler( &samplerPointWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferGrassVertices };
	const uint32_t strides[] = { sizeof( VertexGrass ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );

	float chunkRadius = sqrt( grassAreaPerChunk*grassAreaPerChunk*3 );
	float checkRadius = grassRadius + chunkRadius;
	
	for( uint32_t i = 0; i < numGrassChunks; i++ )
	{
		GrassChunk* pChunk = &pGrassChunks[ i ];
		if ( pChunk->numValid == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );

		float diffX = pChunk->centerX - grassCameraX;
		float diffY = (aabb._min.y + aabb._max.y) / 2.0f - grassCameraY;
		float diffZ = pChunk->centerZ - grassCameraZ;
		float dist = diffX*diffX + diffY*diffY + diffZ*diffZ;

		if ( dist > checkRadius*checkRadius ) continue;

		if ( !frustum->CheckBoxFast( aabb ) ) continue;

		const GPUBuffer* vbs[] = { &pChunk->bufferInstances };
		const uint32_t strides[] = { sizeof(InstanceGrass) };
		device->BindVertexBuffers( vbs, 1, 1, strides, 0, cmd );
		device->BindIndexBuffer( &bufferGrassIndices, IndexBufferFormat::UINT16, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}
	
	device->EventEnd(cmd);
}

} // namespace GGGrass

#ifdef GGGRASS_UNDOREDO
void GGGrass_CreateUndoRedoAction(int type, int eList, bool bUserAction, void* pEventData)
{
	if (!gggrass_initialised) return;
	// User performed this undo action, so clear the redo stack since it now contains outdated events.
	if (bUserAction == true)
	{
		undosys_clearredostack();
		undosys_terrain_cleardata(g_TerrainRedoMem);
	}

	if (type == eUndoSys_Terrain_Grass)
	{
		// If we are undoing a redo or vice versa then the terrain snapshot will be out of date.
		if (!bUserAction)
		{
			// If we are undoing a redo or vice versa then the terrain snapshot will be out of date.
			GGGrass::GGGrass_GetData(g_pTerrainSnapshot);
		}

		TerrainEditsBB bb;
		if (bUserAction)
		{	// Use the bounding box from the sculpt/paint that the user performed.
			bb = g_EditBounds;
		}
		else
		{
			// Use the bounding box from the event that we are storing the undo/redo data for.
			sUndoSysEventGrass* pImpendingUndoRedoEvent = (sUndoSysEventGrass*)pEventData;
			bb.maxX = pImpendingUndoRedoEvent->maxX;
			bb.maxY = pImpendingUndoRedoEvent->maxY;
			bb.minX = pImpendingUndoRedoEvent->minX;
			bb.minY = pImpendingUndoRedoEvent->minY;
		}

		undosys_terrain_grass(bb, g_pTerrainSnapshot, eList);
	}
}

void GGGrass_PerformUndoRedoAction(int type, void* pEventData, int eList)
{
	if (!gggrass_initialised) return;
	if (!pEventData)
		return;

	uint8_t* pAddressOfEventData = nullptr;

	switch (type)
	{
	case eUndoSys_Terrain_Grass:
	{
		sUndoSysEventGrass* pEvent = (sUndoSysEventGrass*)pEventData;

		GGGrass::GGGrass_SetData(0, 0, pEvent);

		pAddressOfEventData = pEvent->grassData;

		delete pEvent;
		pEvent = 0;

		break;
	}
	}

	//Mark the sculpt data memory for this event as unused (0 = undo, 1 = redo)
	if (eList == eUndoSys_UndoList)
		undosys_terrain_setmemoryunusedfrom(pAddressOfEventData, g_TerrainUndoMem);
	else
		undosys_terrain_setmemoryunusedfrom(pAddressOfEventData, g_TerrainRedoMem);
}
#endif
