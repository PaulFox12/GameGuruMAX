#ifndef _H_GGTERRAIN
#define _H_GGTERRAIN

#include "wiGraphicsDevice.h"
#include "wiScene.h"

#include "Shaders/GGTerrainConstants.hlsli"

namespace KMaths
{
	class Vector3;
}

#define GGTERRAIN_MAX_SOURCE_TEXTURES  32

#define GGTERRAIN_INVALIDATE_CHUNKS    0x01
#define GGTERRAIN_INVALIDATE_TEXTURES  0x02
#define GGTERRAIN_INVALIDATE_ALL       (GGTERRAIN_INVALIDATE_CHUNKS | GGTERRAIN_INVALIDATE_TEXTURES)  

struct sUndoSysEventTerrainSculpt;
struct sUndoSysEventTerrainPaint;

namespace GGTerrain
{

struct envProbeItem
{
	XMFLOAT3 position;
	float distance;
	float range;
	XMFLOAT4 rotation;
	XMFLOAT3 size;
	int used;
	float brightness;
};

void RandomSeed( uint32_t seed );
uint32_t Random();
uint32_t Random( uint32_t from, uint32_t to );
void Random2Seed( uint32_t seed );
uint32_t Random2();
uint32_t Random2( uint32_t from, uint32_t to );
float RandomFloat();

float GGTerrain_UnitsToMeters( float units );
float GGTerrain_MetersToUnits( float meters );
float GGTerrain_OffsetToMeters( float units );
float GGTerrain_MetersToOffset( float meters );

#define GGTERRAIN_SCULPT_NONE    0
#define GGTERRAIN_SCULPT_RAISE   1
#define GGTERRAIN_SCULPT_LOWER   2
#define GGTERRAIN_SCULPT_LEVEL   3
#define GGTERRAIN_SCULPT_BLEND   4
#define GGTERRAIN_SCULPT_RAMP    5
#define GGTERRAIN_SCULPT_PICK    6
#define GGTERRAIN_SCULPT_WRITE   7
#define GGTERRAIN_SCULPT_RANDOM  8
#define GGTERRAIN_SCULPT_RESTORE 9

#define GGTERRAIN_EDIT_NONE    0
#define GGTERRAIN_EDIT_SCULPT  1
#define GGTERRAIN_EDIT_PAINT   2
#define GGTERRAIN_EDIT_TREES   3
#define GGTERRAIN_EDIT_GRASS   4

// parameters used to communicate with other code modules
struct GGTerrainExtraParams 
{
	int iProceduralTerrainType = -1;
	bool bTerrainChanged = true;
	int iUpdateTrees = 0;
	int iUpdateGrass = 0;
	bool bUpdateProbes = false;

	int edit_mode = GGTERRAIN_EDIT_NONE;
	int edit_pick_mode = 0; // 0=pick terrain, 1=pick plane
	
	int sculpt_mode = GGTERRAIN_SCULPT_RAISE;//GGTERRAIN_SCULPT_NONE;
	float sculpt_speed = 25.0f;
	float sculpt_chosen_height = 0.0f; // set by GGTERRAIN_SCULPT_PICK, used by GGTERRAIN_SCULPT_WRITE
	float sculpt_randomness_frequency = 30.0f;

	float flat_area_angle = 0;

	int paint_material = 0; // 0 is special ID to use default height materials, add 1 to select a material index to paint with
};

// parameters that don't affect terrain vertices or pages and only used internally
// these values must not be modified outside the terrain module
struct GGTerrainInternalParams 
{
	int prevMouseLeft = 0;
	int mouseLeftPressed = 0;
	int mouseLeftState = 0;
	int mouseLeftReleased = 0;
	float sculpt_pick_y = 0;
	float sculpt_prev_ramp_x = 0;
	float sculpt_prev_ramp_z = 0;
	int sculpt_prev_ramp_valid = 0;
	int update_flat_areas = 0;
	float update_flat_areas_minX = 1e20f;
	float update_flat_areas_minZ = 1e20f;
	float update_flat_areas_maxX = -1e20f;
	float update_flat_areas_maxZ = -1e20f;
};

#define GGTERRAIN_FRACTAL_VALLEYS0    0x00000001
#define GGTERRAIN_FRACTAL_VALLEYS1    0x00000002
#define GGTERRAIN_FRACTAL_VALLEYS2    0x00000004
#define GGTERRAIN_FRACTAL_VALLEYS3    0x00000008
#define GGTERRAIN_FRACTAL_RIDGES0     0x00000010
#define GGTERRAIN_FRACTAL_RIDGES1     0x00000020
#define GGTERRAIN_FRACTAL_RIDGES2     0x00000040
#define GGTERRAIN_FRACTAL_RIDGES3     0x00000080
#define GGTERRAIN_FRACTAL_OUTSIDE_HEIGHTMAP  0x00000100

// parameters that affect the terrain vertices and result in chunk updates
struct GGTerrainParams
{
	// quality settings
	uint32_t lod_levels = 9; // min = 1, max = 16
	uint32_t segments_per_chunk = 64; // must be a power of 2, max = 128
	float segment_size = 8.0f; // distance between vertices at the smallest detail level, for now don't change this

	// generation settings
	uint32_t fractal_levels = 10; // number of levels of noise to combine to generate the fractal noise
	float fractal_initial_freq = 0.3f; // scale of the initial noise frequency
	float fractal_initial_amplitude = 1.0f; // amplitude of first fractal level, default = 1.0f
	float fractal_freq_increase = 2.4f; // how the noise frequency changes with increasing detail level
	float fractal_freq_weight = 0.4f; // how much the noise weight changes with increasing detail level
	uint32_t fractal_flags = GGTERRAIN_FRACTAL_VALLEYS0 | GGTERRAIN_FRACTAL_RIDGES2;
	float noise_power = 1.2f; // how the noise value is modified after being generated
	float noise_fallof_power = 0.37f; // how the noise value is modified by the height of the previous detail levels
	float height = 20000.0f; // maximum height of the terrain
	float minHeight = 20000.0f; // minimum height of the terrain underwater
	float offset_x = GGTerrain_MetersToOffset(-500.0f);
	float offset_y = 0.0f;
	float offset_z = 0.0f;
	float heightmap_scale = 0.2f; // pixels per meter, 1/3 arc-seconds = 0.1 (or 10 meters per pixel)
	uint32_t heightmap_width = 0;
	uint32_t heightmap_height = 0;
	uint16_t* pHeightmapMain = 0;
	uint32_t iHeightMapEditMode = 0;
	float heightmap_roughness = 1.0f;
	float height_outside_heightmap = -2000;
	float fade_outside_heightmap = 100;

	uint32_t seed = 0x31947521;
	
	bool bForceUpdate = false;

	bool IsEqual( GGTerrainParams *other )
	{
		return (memcmp( this, other, sizeof(GGTerrainParams) ) == 0);
	}

	void Copy( GGTerrainParams* other )
	{
		memcpy( this, other, sizeof(GGTerrainParams) );
	}
};

// parameters that affect terrain materials and result in page updates being needed
struct GGTerrainRenderParams
{
	int baseLayerMaterial = 0x100 | 17;
	float layerStartHeight[ 5 ] = { 0, 300, 9560, 100000, 100000 };
	float layerEndHeight[ 5 ] = { 100, 600, 13500,100000, 100000 };
	int layerMatIndex[ 5 ] = { 0x100 | 2, 0x100 | 0, 0x100 | 20, 0x100 | 0, 0x100 | 0 };

	float slopeStart[ 2 ] = { 0.2f, 1.0f };
	float slopeEnd[ 2 ] = { 0.4f, 1.0f };
	int slopeMatIndex[ 2 ] = { 0x100 | 4, 0x100 | 4 };

	float bumpiness = 1.0f;

	uint32_t flags = 0;
	float maskScale = 0.3f;
	float tilingPower = 0.56f;
		
	bool IsEqual( GGTerrainRenderParams *other )
	{
		return (memcmp( this, other, sizeof(GGTerrainRenderParams) ) == 0);
	}

	void Copy( GGTerrainRenderParams* other )
	{
		memcpy( this, other, sizeof(GGTerrainRenderParams) );
	}
};

// parameters that affect rendering but don't need any vertex or page updates
struct GGTerrainRenderParams2
{
	float reflectance = 0.02f;
	float textureGamma = 2.2f;
	float detailScale = 1.0f;
	uint32_t detailLimit = 0;
	float brushSize = 500.0f;//way too big for up default start position on terrain 3000.0f;
	uint32_t readBackTextureReduction = 4; // don't set less than 4, lower gives better quality when a page covers a low number of pixels, higher gives better performance

	float editable_size = 50000.0f;
	uint32_t flags2 = GGTERRAIN_SHADER_FLAG2_USE_FOG | GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE | GGTERRAIN_SHADER_FLAG2_SHOW_BRUSH_SIZE;

	bool IsEqual( GGTerrainRenderParams2 *other )
	{
		return (memcmp( this, other, sizeof(GGTerrainRenderParams2) ) == 0);
	}

	void Copy( GGTerrainRenderParams2* other )
	{
		memcpy( this, other, sizeof(GGTerrainRenderParams2) );
	}
};

// more parameters that affect rendering, kept apart from the two above so the settings string (GGTerrain_GetSettings),
// which checks their sizes, stays the same; saved by name with the level (GGTerrain_SaveSettings)
struct GGTerrainRenderParams3
{
	float edgeBreakup = 0; // painted textures meet along an uneven edge, not a straight blend over the material map's cell: 0 (none) to 1
	float edgeSmoothing = 0; // painted textures meet along a smooth line fitted through the cells (a diagonal stroke runs straight): 0 (none) to 1
	float edgeSoftness = 0.5f; // with smoothing or breakup, how wide the blend is: 0 a crisp line, 1 a blend over a few metres
	float rockStrength = 0; // steep slopes take a texture laid on from the sides (GGTerrainRockHF.hlsli), 0 (none) to 1
	float rockStart = 0.3f; // the slope (1 - normal Y) where it starts to show
	float rockEnd = 0.5f; // and where it is full
	int rockMaterial = -1; // the texture slot, -1 the main slope's (slopeMatIndex[0])
	float rockTileSize = 1181.1f; // the texture repeats this often (units, 30 m: a cliff is mostly seen from afar, where the pages' own tiling is 10-60 m)
	float rockEdgeBreakup = 0.5f; // the rock's edge moved by a noise in world space, so it runs unevenly into the ground, and blended wider: 0 (a slope line) to 1
	float rockVariation = 0.6f; // the rock texture at two sizes in patches, so its repeats don't line up across a big face: 0 (one size) to 1

	bool IsEqual( GGTerrainRenderParams3 *other )
	{
		return (memcmp( this, other, sizeof(GGTerrainRenderParams3) ) == 0);
	}

	void Copy( GGTerrainRenderParams3* other )
	{
		memcpy( this, other, sizeof(GGTerrainRenderParams3) );
	}
};

#define GGTERRAIN_FLAT_AREA_TYPE_RECT    0
#define GGTERRAIN_FLAT_AREA_TYPE_CIRCLE  1

struct GGTerrainFlatArea
{
	uint32_t data = 0; // bits: [0]=valid, [1-2]=type (0=rect, 1=circle, 2=reserved, 3=reserved), [3]=vegetation only (clears trees and grass, leaves the terrain height)
	float x;
	float y;
	float z;
	float sizeX; // diameter for circles
	float sizeZ;
	float angle;

	bool IsValid() { return (data & 0x01) != 0; }
	void SetValid( uint32_t valid ) 
	{
		if ( valid ) data |= 0x01;
		else data &= ~0x01;
	}

	void SetTypeCircle() { data = (data & ~0x06) | 0x02; }
	void SetTypeRect() { data = (data & ~0x06); }
	int GetType() { return (data >> 1) & 0x03; }

	bool IsVegetationOnly() { return (data & 0x08) != 0; }
	void SetVegetationOnly( uint32_t vegetationOnly )
	{
		if ( vegetationOnly ) data |= 0x08;
		else data &= ~0x08;
	}
};

// Public functions

// safe to modify these params anywhere
// other instances of GGTerrainParams may be in use by threads
extern GGTerrainExtraParams ggterrain_extra_params; 
extern GGTerrainParams ggterrain_global_params; 
extern GGTerrainRenderParams ggterrain_global_render_params;
extern GGTerrainRenderParams2 ggterrain_global_render_params2;
extern GGTerrainRenderParams3 ggterrain_global_render_params3;
extern int ggterrain_draw_enabled;

int GGTerrain_Init( wiGraphics::CommandList cmd);
void GGTerrain_WindowResized();

void GGTerrain_ClearEnvProbeList(void);
void GGTerrain_InstantEnvProbeRefresh(int iCoolDownIndex);
void GGTerrain_AddEnvProbeList(float x, float y, float z, float range, float quatx, float quaty, float quatz, float quatw, float sx, float sy, float sz, float brightness = 1.0f);

void GGTerrain_Update( float playerX, float playerY, float playerZ, wiGraphics::CommandList cmd, bool bRenderTargetFocus ); // updates chunks and virtual texture, must not be called from a render pass
void GGTerrain_Update_EmptyLevel(float playerX, float playerY, float playerZ, wiGraphics::CommandList cmd);

int GGTerrain_IsReady();

int GGTerrain_RayCast( RAY pickRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint32_t* lodLevel, int includeFlatAreas=1 );
int GGTerrain_GetHeight( float x, float z, float* outHeight, int accurateButSlow=0, int includeFlatAreas=1 );
// GG: hashes of everything the accurate height reads, for the whole map navmesh's change checks: the settings that apply
// everywhere (noise, heightmap and the imported heightmap), the edit maps on a rect (sculpting and flat areas, each flat
// area taken as its height), and both over the whole editable area
uint64_t GGTerrain_GetHeightFingerprintGlobal();
uint64_t GGTerrain_GetHeightFingerprintRect( float minX, float minZ, float maxX, float maxZ );
uint64_t GGTerrain_GetHeightFingerprint();
// GG: the edit maps for a tool that writes them directly (the spline roads). Sculpt heights (normalized, see
// GGTerrain_HeightToEdit) and their types, rows flipped as the height function reads them (index (4095 - z) * 4096 + x);
// the material map (0 automatic, n texture slot n-1), not flipped (index z * 4096 + x); each over the editable area.
// After writing: GGTerrain_InvalidateRegion for heights, GGTerrain_MaterialMapChanged for materials
float* GGTerrain_GetHeightEditMap();
uint8_t* GGTerrain_GetHeightEditTypeMap();
uint8_t* GGTerrain_GetMaterialMap();
float GGTerrain_HeightToEdit( float y );
void GGTerrain_MaterialMapChanged( float minX, float minZ, float maxX, float maxZ );
int GGTerrain_GetNormal( float x, float z, float* outNx, float* outNy, float* outNz );

void GGTerrain_CancelRamp();
void GGTerrain_ResetSculpting();

bool GGTerrain_GetTerrainChanged ();

float GGTerrain_GetEditableSize();
int GGTerrain_GetHeightMapEx(uint32_t sizeX, uint32_t sizeZ, float* pHeightMap, bool bUseOriginalLOD);
int GGTerrain_GetHeightMap(uint32_t sizeX, uint32_t sizeZ, float* pHeightMap);

uint32_t GGTerrain_GetSculptDataSize();
int GGTerrain_GetSculptData( uint8_t* data ); // data must be allocated with a size of GGTerrain_GetSculptDataSize(), returns 1 on success
int GGTerrain_SetSculptData( uint32_t size, uint8_t* data, sUndoSysEventTerrainSculpt* = nullptr ); // size must be equal to GGTerrain_GetSculptDataSize(), returns 1 on success

void GGTerrain_InvalidateRegion(float minX, float minZ, float maxX, float maxZ, uint32_t flags);
void GGTerrain_InvalidateEverything( uint32_t flags );

// a piece of painted road line (the spline roads' markings): straight from a to b, solid or dashed, painted into the pages
// over whatever ground is there; the page shader's TerrainMarking has the same layout
struct GGTerrainMarking
{
	float ax, az, bx, bz; // world units
	float halfWidth;
	float dash, gap; // a dash and the gap after it along the line, gap 0 a solid line
	float phase; // how far along its line a is, so a dash pattern runs on from piece to piece
	float r, g, b; // the paint's colour, linear
	float wear; // 0 fresh paint, 1 worn away in patches
};
// replaces the markings painted into the pages; the caller invalidates the pages under what changed
// (GGTerrain_InvalidateRegion with GGTERRAIN_INVALIDATE_TEXTURES)
void GGTerrain_SetMarkings( const GGTerrainMarking* pMarkings, uint32_t count );
// GG: the material map texels the paint brush leaves alone (the spline roads and rivers), one bit each (index z * 4096 + x,
// bit index & 31 of word index >> 5), from the given function, asked on each frame the brush paints; null paints everywhere
void GGTerrain_SetPaintKeep( const uint32_t* (*pfnKeep)( void ) );
uint32_t GGTerrain_GetPaintDataSize();
int GGTerrain_GetPaintData( uint8_t* data ); // data must be allocated with a size of GGTerrain_GetPaintDataSize(), returns 1 on success
int GGTerrain_SetPaintData( uint32_t size, uint8_t* data, sUndoSysEventTerrainPaint* = nullptr); // size must be equal to GGTerrain_GetPaintDataSize(), returns 1 on success
void GGTerrain_ResetPaintData();

int GGTerrain_GetMaterialIndex( float x, float z ); // returns an index between 0 and 31 inclusive for the material closest to the given point

int GGTerrain_GetPagesNeeded();
int GGTerrain_GetPagesActive();
void GGTerrain_GetPageCounts( uint32_t* pReadBacks, uint32_t* pPagesMade );
int GGTerrain_GetDebugValue();
int GGTerrain_GetPagesRefreshNeeded();

int GGTerrain_GetEditSizeVisible();
void GGTerrain_SetEditSizeVisible( int visible );
int GGTerrain_GetEditSizeVisible3D();
void GGTerrain_SetEditSizeVisible3D( int visible );
int GGTerrain_GetMiniMapVisible();
void GGTerrain_SetMiniMapVisible( int visible );
int GGTerrain_GetGenerateTerrainOutsideHeightMap();
void GGTerrain_SetGenerateTerrainOutsideHeightMap( int generate );

char* GGTerrain_GetSettings();
int GGTerrain_SetSettings( const char* settingsString ); // returns 0 if successful, -1 if not

char* GGTerrain_SaveSettings( int water_height =  0); // returns a JSON string
int GGTerrain_LoadSettings(const char* settingsJSON, bool bRestoreWater = false ); // returns 1 if successful, 0 if not

int GGTerrain_LoadHeightMap( const char* szFilename, uint32_t width, uint32_t height , bool bBigEndian = true);
int GGTerrain_SetHeightMap(uint16_t* srcdata, uint32_t width, uint32_t height, bool bBigEndian = true);
void GGTerrain_RemoveHeightMap();
uint32_t GGTerrain_GetHeightmapDataSize(uint32_t &width, uint32_t &height);
int GGTerrain_GetHeightmapData(uint16_t* data);
int GGTerrain_SetHeightmapData(uint16_t* data, uint32_t width, uint32_t height);

// flat areas
uint32_t GGTerrain_AddFlatRect( float posX, float posZ, float sizeX, float sizeZ, float angle, float height = NAN ); // returns 0 if too many flat areas have been added
uint32_t GGTerrain_AddFlatCircle( float posX, float posZ, float diameter, float height = NAN ); // returns 0 if too many flat areas have been added
void GGTerrain_UpdateFlatArea( uint32_t id, float posX, float posZ, float angle, float sizeX, float sizeZ, float height = NAN ); // angle is ignored for circle areas
void GGTerrain_RemoveFlatArea( uint32_t id );
uint32_t GGTerrain_AddVegetationArea( float posX, float posZ, float sizeX, float sizeZ, float angle ); // clears trees and grass in a rect without flattening, returns 0 if too many flat areas have been added, remove with GGTerrain_RemoveFlatArea
void GGTerrain_UpdateVegetationArea( uint32_t id, float posX, float posZ, float sizeX, float sizeZ, float angle ); // does nothing if the area is unchanged
int GGTerrain_IsVegetationArea( uint32_t id );
struct GGTerrainFlatAreaShape { int type; float x, z, sizeX, sizeZ, angle; }; // type GGTERRAIN_FLAT_AREA_TYPE_*, sizeX the diameter for a circle
int GGTerrain_GetFlatAreasInRect( float minX, float minZ, float maxX, float maxZ, GGTerrainFlatAreaShape* pShapes, int maxShapes ); // flat areas of either kind overlapping the rect; -1 if more than maxShapes do
void GGTerrain_RemoveAllFlatAreas();

void GGTerrain_SetGameMode( int mode ); // 0 = editor, 1 = test game and standalone

#define GGTERRAIN_PERFORMANCE_LOW    0
#define GGTERRAIN_PERFORMANCE_MED    1
#define GGTERRAIN_PERFORMANCE_HIGH   2
#define GGTERRAIN_PERFORMANCE_ULTRA  3
void GGTerrain_SetPerformanceMode( uint32_t mode );

void GGTerrain_SetBrush(int i);
void GGTerrain_SetPaintHardness(unsigned int iHardness);
void GGTerrain_SetScaleSource(float fScale);

//void GGTerrain_CreateUndoRedoAction(int type, int eList);
//void GGTerrain_CreateRedoAction(int type, void* pEventData);

int GGTerrain_Physics_GetBounds( float* minX, float* minY, float* minZ, float* maxX, float* maxY, float* maxZ );
void GGTerrain_Physics_ProcessVertices( void* callback, float worldToPhysScale, float minX, float minY, float minZ, float maxX, float maxY, float maxZ );
void GGTerrain_Physics_RayCast( void* callback, float worldToPhysScale, float srcX, float srcY, float srcZ, float dstX, float dstY, float dstZ );

int GGTerrain_GetTriangleList( KMaths::Vector3** vertices, float minX, float minZ, float maxX, float maxZ, int firstLOD=2 ); 
int GGTerrain_GetTriangleListHighQuality(KMaths::Vector3** vertices, float minX, float minZ, float maxX, float maxZ, int firstLOD = 2);

void GGTerrain_ReloadTextures(wiGraphics::CommandList cmd = 0, std::vector<std::string>* files = nullptr, std::vector<int>* failures = nullptr, char* rootDir = nullptr);
void GGTerrain_LoadDefaultTextureIntoSlot(int i, char* rootDir, wiGraphics::CommandList cmd = 0);

void GGTerrain_DebugOutputFlattenedAreas(void);
void GGTerrain_CheckMaterialUsed(wiGraphics::CommandList cmd);

// where a texture array slice was last uploaded from (GetTextureSource): the file asked for, made absolute, the file
// GG_GetRealPath resolved it to (a Documents copy ahead of the install's) and that file's write time then
struct GGTextureSource
{
	std::string requested;
	std::string loaded;
	uint64_t writeTime = 0;
};
void GGTerrain_RecordTextureSource( GGTextureSource* pSource, const char* requested, const char* loaded );
// 0 the same file would load now, unchanged; 1 it was written since; 2 another file would load now (a Documents copy
// added or removed); 3 it is gone
int GGTerrain_CheckTextureSource( const GGTextureSource* pSource );
// the file a terrain slot (0 to 31) was last uploaded from, kind 0 color, 1 normal, 2 surface: "" none, 0 a bad slot or
// kind; *pChanged as GGTerrain_CheckTextureSource
const char* GGTerrain_GetTextureSource( int slot, int kind, int* pChanged );
// GG: measurements (0 none, 1 the prepass depth only, 2 both terrain passes into a 1 x 1 viewport), the prepass's chunks
// nearest first (on at first), and the last prepass's chunks drawn, those flat, and its triangles
void GGTerrain_SetMeasure( int mode );
void GGTerrain_SetPrepassNearFirst( int on );
void GGTerrain_GetDrawStats( int* pChunks, int* pFlat, int* pTriangles );
} // namespace GGTerrain

#endif // _H_GGTERRAIN