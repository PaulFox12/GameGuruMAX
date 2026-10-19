#include "GGTrees.h"

#define ONLYLOADWHENUSED

#define GGTREES_CONSTANTS_FULL_DECL
#include "Shaders/GGTreesConstants.hlsli"

#include "WickedEngine.h"
#include "wiRenderer.h"
#include "wiProfiler.h"
#include "wiInput.h"
#include "wiECS.h"
#include "wiScene.h"

#include "CFileC.h"
//#include "Utility/dds.h" // TODO: DX12 - tinyddsloader removed, rewrite DDS loading

#include "master.h"

// redefines MAX_PATH to 1050
#include "preprocessor-moreflags.h"

#include "GGTerrainDataStructures.h"

#include "M-UndoSys-Terrain.h"

#include "GGTerrain.h"
using namespace GGTerrain;

#include "gameguru.h"

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

using namespace wiGraphics;

extern bool bImGuiGotFocus;
extern wiECS::Entity g_entitySunLight;
extern bool g_bAutomationActive;
void timestampactivity(int i, char* desc_s);

extern uint8_t* g_pTerrainSnapshot;
extern UndoRedoMemory g_TerrainUndoMem;
extern UndoRedoMemory g_TerrainRedoMem;
extern void undosys_terrain_settreecount(int treeCount, uint32_t treeSize, int chunkCount, uint32_t chunkSize);

void GGTrees_CreateUndoRedoAction(int type, int eList, bool bUserAction, void* pEventData = nullptr);
void GGTrees_PerformUndoRedoAction(int type, void* pEventData, int eList);

int g_iActiveTreeEvent = 0;
int ggtrees_initialised = 0;

#ifdef ONLYLOADWHENUSED
bool bTreeTextureUploaded[255];
bool bTreeTextureForceUploaded[255];
//extern std::string FinalGameGuruMaxFolder;
uint64_t last_paint_tree_bitfield;
#endif

// GGMAX 2.96: GGTerrain's DDS-into-array-slice loader, exposed at global scope by the shim at
// the foot of GGTerrain_part0.cpp. Declared HERE, outside `namespace GGTrees`, on purpose - a
// declaration written inside the namespace mangles to GGTrees::… and fails to link.
namespace wi { namespace graphics { struct Texture; struct CommandList; } }
bool GG_LoadDDSIntoTextureSlice( const char* filename, wi::graphics::Texture* tex,
                                 uint32_t arraySlice, wi::graphics::CommandList cmd );

// GGMAX 2.98: terrain chunk-ring reach in world units (0 = no terrain / no limit). Declared at
// global scope for the same linkage reason as the shim above.
float GG_GetTerrainViewRadius();

// GGMAX 3.00: the Trees Off effective flag (GGTerrainWicked.cpp). GLOBAL scope on purpose.
extern bool gg_no_trees;

namespace GGTrees
{
#define GGTREES_UNDOREDO

GGTreesParams ggtrees_global_params;
// GGMAX 3.96: snapshot/restore partner for the Test Game round trip - this is what puts lod_dist
// back to 3000 instead of leaving it at the preset's 1000. See M-GridEdit_part2.cpp.
GGTreesParams ggtrees_save_params;
GGTreesInternalParams ggtrees_internal_params;

int ggtrees_draw_enabled = 1;

// billboard vertex
struct VertexTree
{
    float x, y;
};

// billboard vertices
VertexTree g_VerticesTreeBillboard[ 4 ] = 
{
	// x, y
    { -0.5f, 1.0f },
	{ -0.5f, 0.0f },
	{  0.5f, 1.0f },
	{  0.5f, 0.0f },
};

// billboard indices
uint16_t g_IndicesTreeBillboard[ 6 ] =
{
    0, 1, 2,
    2, 1, 3,
};

// high detail tree vertex
struct VertexTreeHigh
{
	float x, y, z;
	uint32_t normal;
	float u, v;
};

// high detail tree trunk or branches
struct TreeMeshHigh
{
	const VertexTreeHigh* const pVertices;
	const uint16_t* const pIndices;
	uint32_t numVertices;
	uint32_t numIndices;
	const char* textureName;
	float height;
};

// high detail tree
struct GGTree
{
	const TreeMeshHigh* const trunk;
	const TreeMeshHigh* const branches;
	float height;
	float billboardScaleX;
	const char* billboardFilename;
	const char* billboardNormalFilename;
};

// all trees, low and high detail
#include "TreeMeshes/TreeMeshesLOD0.h"
#define g_GGTrees g_GGTreesLOD0

float treeMaxHeight = 1; // calculated at run time

// tree instances
// from edge to edge in world units, i.e. 2000 = -1000 to 1000. Covers the editable area of the terrain, never less
// than the original 200000, see GGTrees_GetRequiredArea()
#define GGTREES_MIN_AREA 200000.0f
#define GGTREES_MAX_AREA 400000.0f
float treeArea = GGTREES_MIN_AREA;
// Hired Gun: 700,000 slots (stock 400,000) so a large level can be covered at forest density. The tree file is named
// after its size (8400004.tre); a level with only a 400,000 slot file (4800004.tre) loads into the first slots
const uint32_t numTotalTrees = 700000;
const uint32_t treeSplit = 16;

const uint32_t numTreeChunks = treeSplit * treeSplit;
const uint32_t numTreesPerChunk = numTotalTrees / numTreeChunks;
float treeAreaPerChunk = treeArea / treeSplit;

uint32_t treeHighlighted = 0xFFFFFFFF;

// Dirty stamp for the Wicked tree pool (part2): bumped whenever tree instance
// data changes so the pool re-runs its nearest-N selection next frame. The
// paint/spray/erase/flatten paths mutate pAllTrees flags IN PLACE via the
// setters below (numTotalTrees is fixed — the count never changes), so
// the setters bump this on any REAL state change. Change-detected so redundant
// same-value calls (and hover highlighting, which is excluded entirely) can't
// retrigger the rescan every frame. Also bumped at GGTrees_UpdateInstances
// entry, which writes instance heights directly without going through setters.
static uint32_t g_treeInstanceStamp = 0;

// Far-shadow proxy dirty marking (defined in part2): the merged billboard
// shadow meshes are one per tree chunk; setters mark ONLY the owning chunk so
// a paint/delete stroke rebuilds a handful of chunks instead of all 256 (the
// full-batch rebuild froze the editor ~2s after every tree edit).
void GGTrees_MarkProxyChunkDirtyAt( float x, float z );
void GGTrees_MarkAllProxyChunksDirty();

// each tree type's trunk collider at scale 1, fitted to its LOD0 trunk mesh 0.5 to 1.5 m up: radius, then the trunk's
// centre (x, z) from the tree's origin before the tree is turned
static const float g_TreeTrunkFit[ 38 ][ 3 ] =
{
	{   9.6f,  -1.1f,   1.5f }, // 0 birch
	{  11.4f,   3.7f,  -4.7f }, // 1 cactus_var1
	{  11.1f,  -4.7f,   0.5f }, // 2 cactus_var2
	{  13.7f,   0.1f,   0.0f }, // 3 cactus_var3
	{  20.5f,  -4.9f,   0.2f }, // 4 cactus_var4
	{   5.3f,  -0.6f,   0.4f }, // 5 dead pine tree
	{  16.0f,  -1.0f,  -0.2f }, // 6 drypine
	{   5.0f,  -2.2f,  -1.5f }, // 7 italian pine
	{  58.2f,  -1.6f,   7.6f }, // 8 jungletree1
	{  53.5f,  -2.2f,  -3.7f }, // 9 jungletree2
	{   8.0f,   0.5f,   3.0f }, // 10 jungletree3a
	{   8.3f,   1.0f,   0.1f }, // 11 jungletree3b
	{   8.0f,   0.5f,   3.0f }, // 12 jungletree4a
	{   8.3f,   1.0f,   0.1f }, // 13 jungletree4b
	{  22.4f,  -4.8f, -22.2f }, // 14 jungletree5a
	{   9.5f,  -2.1f,  -6.5f }, // 15 jungletree5b
	{   9.6f,   2.3f,  -0.9f }, // 16 jungletree6a
	{  10.2f,  -1.0f,  -5.7f }, // 17 jungletree6b
	{   4.2f,   3.1f,  -1.4f }, // 18 kentia palm
	{   7.7f,   0.8f,   1.7f }, // 19 palm
	{  11.2f,   1.6f,  -0.5f }, // 20 pine
	{  12.1f,  -0.5f,  -5.5f }, // 21 scotspine1
	{  13.3f,   3.9f,  -3.0f }, // 22 scotspine2
	{  13.3f,   2.6f,  -2.4f }, // 23 scotspine dead
	{  22.5f,   0.0f,   0.0f }, // 24 snow fir2 (low branches in the band, old size)
	{  14.7f, -13.4f,   3.2f }, // 25 snow fir3
	{   6.9f,  -0.1f,   0.5f }, // 26 snow fir1
	{   8.3f,   0.6f,   0.8f }, // 27 snow pine
	{   9.4f,  -0.7f,  -6.2f }, // 28 snow pine tall2
	{   9.3f,   0.4f,   0.6f }, // 29 snow pine tall
	{   7.6f,  -0.3f,   1.1f }, // 30 sparse pine
	{  20.9f,   3.1f,   2.2f }, // 31 vine tree large
	{  10.4f,  -3.8f,  17.0f }, // 32 vine tree small
	{  16.0f,  -1.0f,  -0.2f }, // 33 westernpine
	{   3.6f,   0.7f,  -0.3f }, // 34 white pine
	{   9.6f,  -1.1f,   1.5f }, // 35 birch autumn1
	{   9.6f,  -1.1f,   1.5f }, // 36 birch autumn2
	{   9.6f,  -1.1f,   1.5f }, // 37 birch autumn3
};

struct InstanceTree
{
	float x, y, z;
	uint32_t data; // [0]=visible, [1]=user moved, [2]=highlighted, [3]=flattened, [4]=invalid (slope or underwater), [5-7]=reserved for flags, [8-10]=variation index, [11-16]=tree type, [17-24]=scale
	uint32_t id; // not sent to GPU

	bool IsVisible() { return (data & 0x1) != 0; }
	void SetVisible( int visible )
	{
		uint32_t old = data;
		if ( visible ) data |= 0x1;
		else data &= ~0x1;
		if ( data != old ) { g_treeInstanceStamp++; GGTrees_MarkProxyChunkDirtyAt( x, z ); }
	}

	bool IsUserMoved() { return (data & 0x2) != 0; }
	void SetUserMoved( int moved )
	{
		if ( moved ) data |= 0x2;
		else data &= ~0x2;
	}

	bool IsHighlighted() { return (data & 0x4) != 0; }
	void SetHighlighted( int highlighted )
	{
		if ( highlighted ) data |= 0x4;
		else data &= ~0x4;
	}

	bool IsFlattened() { return (data & 0x8) != 0; }
	void SetFlattened( int flattened )
	{
		uint32_t old = data;
		if ( flattened ) data |= 0x8;
		else data &= ~0x8;
		if ( data != old ) { g_treeInstanceStamp++; GGTrees_MarkProxyChunkDirtyAt( x, z ); }
	}

	// invalid if tree is on a slope or underwater
	bool IsInvalid() { return (data & 0x10) != 0; }
	void SetInvalid( int invalid )
	{
		uint32_t old = data;
		if ( invalid ) data |= 0x10;
		else data &= ~0x10;
		if ( data != old ) { g_treeInstanceStamp++; GGTrees_MarkProxyChunkDirtyAt( x, z ); }
	}

	void SetType( uint32_t type )
	{
		if ( type >= numTreeTypes ) return;
		unsigned int mask = (0x3F << 11);
		uint32_t old = data;
		data = (data & ~mask) | (type << 11);
		if ( data != old ) { g_treeInstanceStamp++; GGTrees_MarkProxyChunkDirtyAt( x, z ); }
	}

	void SetFlags( uint32_t flags )
	{
		uint32_t old = data;
		data = (data & ~0xFF) | (flags & 0xFF);
		if ( data != old ) { g_treeInstanceStamp++; GGTrees_MarkProxyChunkDirtyAt( x, z ); }
	}

	void SetID( uint32_t newID )
	{
		id = newID;
		uint32_t varIndex = id & 0x7;
		data = (data & ~(0x07 << 8)) | (varIndex << 8);
	}

	void SetData( uint32_t treeID, uint32_t type, int visible )
	{
		SetFlags( 0 );
		SetID( treeID );
		type &= 0x3F;
		data = (data & ~(0x3F << 11)) | (type << 11);
		SetVisible( visible );
	}

	void SetScale( unsigned char scale )
	{
		scale >>= 1;
		uint32_t old = data;
		data = (data & 0xFF01FFFF) | (scale << 17);
		if ( data != old ) { g_treeInstanceStamp++; GGTrees_MarkProxyChunkDirtyAt( x, z ); }
	}

	unsigned char GetScale()
	{
		unsigned char scale = (data >> 16) & 0xFE;
		return scale;
	}

	float GetScaleFloat()
	{
		return GetScale() / 170.0f + 0.5f;
	}

	int GetType() { return (data >> 11) & 0x3F; }
	int GetID() { return id; }

	float GetTreeThickness()
	{
		int iTreeType = GetType();
		if ( iTreeType >= 38 ) return 15.0f;
		return g_TreeTrunkFit[ iTreeType ][ 0 ] * 2.0f;
	}

	// the trunk's centre: the tree's origin plus its type's trunk offset, turned and scaled as the tree is drawn
	void GetTrunkCentre( float* pX, float* pZ )
	{
		int iTreeType = GetType();
		float ox = 0, oz = 0;
		if ( iTreeType < 38 ) { ox = g_TreeTrunkFit[ iTreeType ][ 1 ]; oz = g_TreeTrunkFit[ iTreeType ][ 2 ]; }
		float ang = ((data >> 8) & 0x7) * 0.785398f;
		float c = cosf( ang ), s = sinf( ang ), scale = GetScaleFloat();
		*pX = x + (c * ox - s * oz) * scale;
		*pZ = z + (s * ox + c * oz) * scale;
	}
};

struct InstanceTreeGPU
{
	float x, y, z;
	uint32_t data; // same as InstanceTree::data
};

InstanceTreeGPU* treeInstancesHigh[ numTreeTypes ] = { 0 };
uint16_t numTreeInstancesHigh[ numTreeTypes ] = { 0 };
GPUBuffer bufferInstancesHigh[ numTreeTypes ];

InstanceTreeGPU* treeInstancesHighShadow[ numTreeTypes ] = { 0 };
uint16_t numTreeInstancesHighShadow[ numTreeTypes ] = { 0 };
GPUBuffer bufferInstancesHighShadow[ numTreeTypes ];

#define GGTREES_MAX_ENVMAP_TREES 500
InstanceTreeGPU treeInstancesHighEnvProbe[ numTreeTypes ][ GGTREES_MAX_ENVMAP_TREES ] = { 0 };
uint16_t numTreeInstancesHighEnvProbe[ numTreeTypes ] = { 0 };
GPUBuffer bufferInstancesHighEnvProbe[ numTreeTypes ];

InstanceTree pAllTrees[ numTotalTrees ] = { 0 };
UnorderedArray<uint32_t> pInvisibleTrees;

// 1 when the tree's height is known. Trees outside the terrain LOD set get their height from the slower terrain
// function once, and keep it until the terrain under them changes, see GGTrees_InvalidateHeights()
uint8_t pTreeHeightValid[ numTotalTrees ] = { 0 };

// Tree map for reference visualization (same size as grass/material maps)
#define GGTREES_MAP_SIZE 4096
uint8_t pTreeMap[ GGTREES_MAP_SIZE * GGTREES_MAP_SIZE ] = { 0 };
Texture texTreeMap;

void GGTrees_RasterizeTreeMap()
{
	memset( pTreeMap, 0, GGTREES_MAP_SIZE * GGTREES_MAP_SIZE );

	float editSize = ggterrain_global_render_params2.editable_size;
	float invEditSize = 1.0f / editSize;

	for ( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		InstanceTree* pTree = &pAllTrees[ i ];
		if ( !pTree->IsVisible() || pTree->IsInvalid() ) continue;

		float u = pTree->x * invEditSize * 0.5f + 0.5f;
		float v = pTree->z * invEditSize * 0.5f + 0.5f;
		if ( u < 0 || u >= 1 || v < 0 || v >= 1 ) continue;

		int cx = (int)(u * GGTREES_MAP_SIZE);
		int cy = (int)(v * GGTREES_MAP_SIZE);
		uint8_t value = (uint8_t)(pTree->GetType() + 1);

		// Stamp a small circle (radius 3 pixels) so trees are visible
		int radius = 3;
		for ( int dy = -radius; dy <= radius; dy++ )
		{
			for ( int dx = -radius; dx <= radius; dx++ )
			{
				if ( dx*dx + dy*dy > radius*radius ) continue;
				int px = cx + dx;
				int py = cy + dy;
				if ( px < 0 || px >= GGTREES_MAP_SIZE || py < 0 || py >= GGTREES_MAP_SIZE ) continue;
				pTreeMap[ py * GGTREES_MAP_SIZE + px ] = value;
			}
		}
	}
}

void GGTrees_UploadTreeMap()
{
	GraphicsDevice* device = wiGraphics::GetDevice();

	SubresourceData initdata = {};
	initdata.data_ptr = pTreeMap;
	initdata.row_pitch = GGTREES_MAP_SIZE;
	initdata.slice_pitch = 0;

	TextureDesc texDesc = {};
	texDesc.bind_flags = BindFlag::SHADER_RESOURCE;
	texDesc.sample_count = 1;
	texDesc.mip_levels = 1;
	texDesc.array_size = 1;
	texDesc.format = Format::R8_UNORM;
	texDesc.usage = Usage::DEFAULT;
	texDesc.width = GGTREES_MAP_SIZE;
	texDesc.height = GGTREES_MAP_SIZE;

	device->CreateTexture( &texDesc, &initdata, &texTreeMap );
	device->SetName( &texTreeMap, "texTreeMap" );
}

void GGTrees_BindTreeMap( int slot, CommandList cmd )
{
	GraphicsDevice* device = wiGraphics::GetDevice();
	device->BindResource( &texTreeMap, slot, cmd );
}

// a drawn tree's trunk as rays test it (GGTrees_RayCastTrunks): a vertical cylinder from the tree's base to its top, as
// thick as the physics' tree cylinder (the species' thickness times the tree's scale, across)
struct TreeTrunk
{
	float x, z;
	float radius;
	float base, top;
	float crown; // where the crown starts: below it only the bare trunk
	int type;
};

// the height above a type's base where its crown starts, at scale 1: the lowest vertex of its full detail branch and leaf
// mesh (the whole height for a type without one), worked out once
static float GGTrees_CrownBase( int type )
{
	static float crownBase[ numTreeTypes ];
	static bool bDone = false;
	if ( !bDone )
	{
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			float lowest = g_GGTrees[ i ].height;
			const TreeMeshHigh* pBranches = g_GGTrees[ i ].branches;
			if ( pBranches )
			{
				for( uint32_t v = 0; v < pBranches->numVertices; v++ )
				{
					if ( pBranches->pVertices[ v ].y < lowest ) lowest = pBranches->pVertices[ v ].y;
				}
			}
			crownBase[ i ] = lowest > 0 ? lowest : 0;
		}
		bDone = true;
	}
	if ( type < 0 || type >= (int)numTreeTypes ) type = numTreeTypes - 1;
	return crownBase[ type ];
}

struct TreeChunk
{
	UnorderedArray<InstanceTree*> pInstances;
	std::vector<TreeTrunk> trunks; // the drawn trees' trunks, packed for rays, rebuilt with the GPU list in Update()
	float minHeight = 0;
	float maxHeight = 0;
	GPUBuffer bufferInstances;
	uint32_t numValid = 0;
	uint16_t x;
	uint16_t z;

	void GetBounds( AABB* aabb )
	{
		if ( !aabb ) return;

		float offsetX = x;
		offsetX = offsetX - treeSplit/2;
		offsetX = offsetX * treeAreaPerChunk; 
		float offsetZ = z;
		offsetZ = offsetZ - treeSplit/2;
		offsetZ = offsetZ * treeAreaPerChunk;

		float treeWidth = (treeMaxHeight / 1.9f);

		aabb->_min.x = offsetX - treeWidth;
		aabb->_min.y = minHeight;
		aabb->_min.z = offsetZ - treeWidth;

		aabb->_max.x = offsetX + treeAreaPerChunk + treeWidth;
		aabb->_max.y = maxHeight;
		aabb->_max.z = offsetZ + treeAreaPerChunk + treeWidth;
	}

	int RayCastBox( RAY ray, float maxDist, float* outDist )
	{
		float offsetX = x;
		offsetX = offsetX - treeSplit/2;
		offsetX = offsetX * treeAreaPerChunk; 
		float offsetZ = z;
		offsetZ = offsetZ - treeSplit/2;
		offsetZ = offsetZ * treeAreaPerChunk;

		float treeWidth = (treeMaxHeight / 1.9f);

		float minx = offsetX - treeWidth;
		float miny = minHeight;
		float minz = offsetZ - treeWidth;

		float maxx = offsetX + treeAreaPerChunk + treeWidth;
		float maxy = maxHeight;
		float maxz = offsetZ + treeAreaPerChunk + treeWidth;

		float tmin1, tmax1, tmin2, tmax2;

		if ( ray.direction.x == 0 )
		{
			tmin1 = (minx - ray.origin.x) * 1e20f;
			tmax1 = (maxx - ray.origin.x) * 1e20f;
		}
		else if ( ray.direction.x > 0 )
		{
			tmin1 = (minx - ray.origin.x) * ray.direction_inverse.x;
			tmax1 = (maxx - ray.origin.x) * ray.direction_inverse.x;
		}
		else 
		{
			tmax1 = (minx - ray.origin.x) * ray.direction_inverse.x;
			tmin1 = (maxx - ray.origin.x) * ray.direction_inverse.x;
		}
    
		if ( ray.direction.y == 0 ) 
		{
			tmin2 = (miny - ray.origin.y) * 1e20f;
			tmax2 = (maxy - ray.origin.y) * 1e20f;
		}
		else if ( ray.direction.y > 0 ) 
		{
			tmin2 = (miny - ray.origin.y) * ray.direction_inverse.y;
			tmax2 = (maxy - ray.origin.y) * ray.direction_inverse.y;
		}
		else 
		{
			tmax2 = (miny - ray.origin.y) * ray.direction_inverse.y;
			tmin2 = (maxy - ray.origin.y) * ray.direction_inverse.y;
		}
               
		if ( (tmin1 > tmax2) || (tmin2 > tmax1) ) return 0;
    
		if ( tmin2 > tmin1 ) tmin1 = tmin2;
		if ( tmax2 < tmax1 ) tmax1 = tmax2;
    
		if ( ray.direction.z == 0 )
		{
			tmin2 = (minz - ray.origin.z) * 1e20f;
			tmax2 = (maxz - ray.origin.z) * 1e20f;
		}
		else if ( ray.direction.z > 0 ) 
		{
			tmin2 = (minz - ray.origin.z) * ray.direction_inverse.z;
			tmax2 = (maxz - ray.origin.z) * ray.direction_inverse.z;
		}
		else 
		{
			tmax2 = (minz - ray.origin.z) * ray.direction_inverse.z;
			tmin2 = (maxz - ray.origin.z) * ray.direction_inverse.z;
		}
    
		if ( (tmin1 > tmax2) || (tmin2 > tmax1) ) return 0;
                                      
		if ( tmin2 > tmin1 ) tmin1 = tmin2;
		if ( tmax2 < tmax1 ) tmax1 = tmax2;
    
		if ( tmax1 < 0.0 ) return 0;
		if ( maxDist > 0 && tmin1 > maxDist ) return 0;
    
		if ( outDist ) *outDist = tmin1;

		return 1;
	}

	int RayCast( RAY ray, float maxDist, float* outDist, InstanceTree** pTree )
	{
		if (!ggtrees_initialised) return 0;
		if ( !RayCastBox( ray, maxDist, 0 ) ) return 0;

		int hit = 0;
		
		for( uint32_t j = 0; j < pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pInstances[ j ];
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsInvalid() ) continue;

			int type = pInstance->GetType();
			float height = g_GGTrees[ type ].height;
			float radius = (height * pInstance->GetScaleFloat()) / 2.0f;

			float diffX = pInstance->x - ray.origin.x;
			float diffY = pInstance->y + radius - ray.origin.y;
			float diffZ = pInstance->z - ray.origin.z; 

			float dotp = (diffX * ray.direction.x) + (diffY * ray.direction.y) + (diffZ * ray.direction.z);
			if ( dotp < 0 ) continue;
			if ( maxDist > 0 && dotp > maxDist ) continue;

			float v3x = ray.direction.x * dotp;
			float v3y = ray.direction.y * dotp;
			float v3z = ray.direction.z * dotp;

			v3x = v3x - diffX;
			v3y = v3y - diffY;
			v3z = v3z - diffZ; 

			v3x *= 2.0f;
			v3z *= 2.0f;

			float sqrDist = v3x*v3x + v3y*v3y + v3z*v3z;
			if ( sqrDist > radius*radius ) continue;
			
			if ( outDist ) 
			{
				float newDist = dotp + sqrDist;
				if ( newDist < *outDist )
				{
					*outDist = newDist;
					if ( pTree ) *pTree = pInstance;
				}
			}

			/*
			float diffX = ray.origin.x - pInstance->x;
			float diffY = ray.origin.y - (pInstance->y + (radius / 2.0f));
			float diffZ = ray.origin.z - pInstance->z; 
			
			float dotp = (diffX*ray.direction.x) + (diffY*ray.direction.y) + (diffZ*ray.direction.z);
			if ( dotp > 0 ) continue; // ray is pointing away from the sphere

			float sqrDist = diffX*diffX + diffY*diffY + diffZ*diffZ;
			sqrDist -= radius*radius;
			sqrDist = dotp*dotp - sqrDist;

			if ( sqrDist < 0 ) continue;

			float dist = -dotp - sqrt(sqrDist);
			if ( dist < 0 ) continue;

			if ( outDist ) 
			{
				if ( dist < *outDist )
				{
					*outDist = dist;
					if ( pTree ) *pTree = pInstance;
				}
			}
			*/

			hit = 1;
		}
		
		return hit;
	}

	void Update()
	{
		trunks.clear();
		if (!ggtrees_initialised) return;
		if ( pInstances.NumItems() == 0 ) return;

		InstanceTreeGPU* pData = new InstanceTreeGPU[ pInstances.NumItems() ];

		minHeight = 1e9f;
		maxHeight = -1e9f;
		// GGMAX 3.53: numValid is read by the BILLBOARD DRAW, which runs while a level is
		// still loading (gridedit_load_map pumps frames mid-load - see the note in
		// WickedCall_ReloadQuiesceGPU). It used to be incremented in this loop, so between the
		// first ++ and CreateBuffer below, the draw could issue N instances against the PREVIOUS
		// level's smaller buffer and the GPU would read off the end of the allocation.
		// Publish it ONLY once the buffer that backs it exists; 0 here means "skip this chunk".
		numValid = 0;
		uint32_t count = 0;
		for( uint32_t j = 0; j < pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pInstances[ j ];
			int type = pInstance->GetType();
			float treeHeight = g_GGTrees[ type ].height;

			if ( pInstance->y < minHeight ) minHeight = pInstance->y;
			if ( pInstance->y + treeHeight > maxHeight ) maxHeight = pInstance->y + treeHeight;


				TreeTrunk trunk;
				pInstance->GetTrunkCentre( &trunk.x, &trunk.z );
				trunk.radius = pInstance->GetTreeThickness() * pInstance->GetScaleFloat() * 0.5f;
				trunk.base = pInstance->y;
				trunk.top = pInstance->y + treeHeight * pInstance->GetScaleFloat();
				trunk.crown = pInstance->y + GGTrees_CrownBase( type ) * pInstance->GetScaleFloat();
				trunk.type = pInstance->GetType();
				trunks.push_back( trunk );
			if ( pInstance->IsVisible() && !pInstance->IsFlattened() && !pInstance->IsInvalid() ) 
			{
				pData[ count ].x = pInstance->x;
				pData[ count ].y = pInstance->y;
				pData[ count ].z = pInstance->z;
				pData[ count ].data = pInstance->data;
				count++;
			}
		}

		if (count == 0)
		{
			//PE: Leak - if numValid == 0 memory was not freed.
			delete[] pData;
			return;
		}

		GPUBufferDesc bufferDesc = {};
		SubresourceData data = {};
		data.data_ptr = pData;
		bufferDesc.size = sizeof(InstanceTreeGPU) * count;
		bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
		//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
		bufferDesc.misc_flags = ResourceMiscFlag::NONE;
		wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferInstances );
		numValid = count;   // GGMAX 3.53: publish LAST - the buffer now exists and matches
		
		delete [] pData;
	}

	void RemoveTree( InstanceTree* pTree )
	{
		if (!ggtrees_initialised) return;
		for( uint32_t i = 0; i < pInstances.NumItems(); i++ )
		{
			if ( pInstances[ i ] == pTree )
			{
				pInstances.RemoveIndex( i );
				return;
			}
		}
	}
};

TreeChunk pTreeChunks[ numTreeChunks ];

TreeCB treeConstantData = {};
GPUBuffer treeConstantBuffer;

// low detail
GPUBuffer bufferTreeVertices;
GPUBuffer bufferTreeIndices;

Shader shaderTreesVS;
Shader shaderTreesPS;
// ---------------------------------------------------------------------------------------------
// GGMAX 2.96: PER-PSO PERSISTENT STATE SNAPSHOTS.
//
// PipelineStateDesc stores POINTERS to the rasterizer/depth/blend/input-layout objects, and
// Wicked's DX12 backend does not compile the pipeline at CreatePipelineState time - it defers
// to pso_validate() at BIND time (wiGraphicsDevice_DX12.cpp:2578). GGTrees_Init built all 11
// tree PSOs from state objects declared as STACK LOCALS, so by the time anything bound one the
// desc pointed at destroyed stack memory. That is the "PSO compilation failure in DX12" the
// disabled GGTrees_Draw blames, and it is why the distant forest has not rendered since the
// port. The engine even has a logger for this failure class (gg_pso_fail.txt, added for the
// gpup particle restore).
//
// There is a SECOND bug in the same code: the locals are MUTATED between the 11 create calls
// (cull_mode, depth_write_mask, alpha_to_coverage flip back and forth). Because every desc
// holds a pointer to the same object, fixing only the lifetime would give all 11 PSOs whatever
// the LAST write happened to be. So each PSO needs its own copy, taken at create time.
// ---------------------------------------------------------------------------------------------
static const uint32_t GGTREE_PSO_MAX = 16;
static RasterizerState   g_ggtreePsoRS [ GGTREE_PSO_MAX ];
static DepthStencilState g_ggtreePsoDSS[ GGTREE_PSO_MAX ];
static BlendState        g_ggtreePsoBS [ GGTREE_PSO_MAX ];
static InputLayout       g_ggtreePsoIL [ GGTREE_PSO_MAX ];
static uint32_t          g_ggtreePsoSlot = 0;

// GGMAX 2.96: master switch for the DX11-style distant-tree billboard pass. Default OFF while
// it is being brought up. Harness SET_FARTREES 0|1.
bool gg_far_tree_pass = true;
// GGMAX 3.04: see GGTreesConstants.hlsli. 1 = prepass honours the terrain-reach clip (correct).
// 0 reproduces the black-tree defect on demand. Harness SET_TREEPREPASSREACH.
bool gg_tree_prepass_reach = true;
// GGMAX 3.05: DEBUG - draw every billboard as a flat unlit grey quad (no texture, no alpha
// cutout, no lighting, no fog) so the raw quad geometry is visible through the mesh->billboard
// handover. The LOD dither discard is kept, so the transition still happens. Lee asked for this
// to eyeball the remaining flicker. setup.ini `treedebugsolid`, harness SET_TREEDEBUGSOLID.
// GGMAX 3.07: wrap strength for the billboard's shaded side, 0..1. 0 = pre-3.07 behaviour
// (bit-identical). Lifts the darks without flattening the normal-map shading or dimming the lit
// side. setup.ini `treeshadewrap` (PERCENT, int), harness SET_TREESHADEWRAP (0..1 float).
// DEFAULT 0.5, chosen by Lee at the machine on 2026-08-24 after A/Bing 0 / 0.35 / 0.5 live.
// ★ 0.6+ starts eating the shading PATTERN rather than lifting it, flattening the tree toward a
// card - which is the thing the normal map exists to prevent. Treat 0.5 as the top of the range.
// ⚠ Tuned under a near-overhead morning sun (87/75). Wrap does proportionally more work as the
// sun drops, so a dusk or overcast level may want less; if that bites, this wants to move to the
// per-level visuals rather than a machine-wide ini key.
float gg_tree_shade_wrap = 0.5f;
int  gg_tree_debug_solid = 0;   // 0 off, 1 grey+dissolve, 2 grey no-discard, 3 red dissolve zone

// GGMAX 2.96: why-did-nothing-draw counters for the billboard pass, read by SET_FARTREES.
uint32_t g_ftDrawCalls    = 0;   // chunks that actually issued a draw
uint32_t g_ftInstances    = 0;   // total billboard instances submitted
uint32_t g_ftChunksTotal  = 0;   // numTreeChunks
uint32_t g_ftChunksWithIn = 0;   // chunks holding at least one instance
uint32_t g_ftFrustumKills = 0;   // chunks rejected by the frustum test
uint32_t g_ftEnterCount   = 0;   // times the pass body was entered
uint32_t g_ftTerrainKills = 0;   // chunks dropped for having no terrain under them
uint32_t g_ftTypesUsed    = 0;   // GGMAX 2.99: tree types this level actually places
static float gg_ftTerrainReach2 = 0.0f;

PipelineState psoTrees;

Shader shaderTreesPrepassVS;
Shader shaderTreesPrepassPS;
PipelineState psoTreesPrepass;
PipelineState psoTreesPrepassNoDepth;   // GGMAX 3.06: debug mode 4 - identical, depth_write ZERO
PipelineState psoTreesDepthAlways;      // GGMAX 3.06: debug mode 5 - colour pass, depth_func ALWAYS

Shader shaderTreesShadowVS;
Shader shaderTreesShadowPS;
PipelineState psoTreesShadow;

Shader shaderTreesHighEnvProbeVS;
Shader shaderTreesHighEnvProbePS;
PipelineState psoTreesHighEnvProbe;

Shader shaderBranchesHighEnvProbeVS;
Shader shaderBranchesHighEnvProbePS;
PipelineState psoBranchesHighEnvProbe;

// high detail
GPUBuffer bufferTreeHighVertices[ numTreeTypes ];
GPUBuffer bufferTreeHighIndices[ numTreeTypes ];

GPUBuffer bufferBranchesHighVertices[ numTreeTypes ];
GPUBuffer bufferBranchesHighIndices[ numTreeTypes ];

Shader shaderTreesHighVS;
Shader shaderTreesHighPS;
PipelineState psoTreesHigh;

Shader shaderBranchesHighVS;
Shader shaderBranchesHighPS;
PipelineState psoBranchesHigh;

Shader shaderTreesHighPrepassVS;
Shader shaderTreesHighPrepassPS;
PipelineState psoTreesHighPrepass;

Shader shaderBranchesHighPrepassVS;
Shader shaderBranchesHighPrepassPS;
PipelineState psoBranchesHighPrepass;

Shader shaderTreesHighShadowVS;
Shader shaderTreesHighShadowPS;
PipelineState psoTreesHighShadow;

Shader shaderBranchesHighShadowVS;
Shader shaderBranchesHighShadowPS;
PipelineState psoBranchesHighShadow;

Texture texTree;
Texture texTreeNormal;
Texture texTreeHigh;
Texture texBranchesHigh;
Texture texNoise;
Sampler samplerTrilinearClamp;
Sampler samplerTrilinearWrap;
Sampler samplerPointWrap;
Sampler samplerBilinearWrap;

// TODO: DX12 - tinyddsloader removed, rewrite DDS loading
#if 0
wiGraphics::FORMAT ConvertDDSFormat( tinyddsloader::DDSFile::DXGIFormat format )
{
	// ... tinyddsloader format conversion table removed ...
	return FORMAT_UNKNOWN;
}

void GGTrees_LoadTextureDDS( const char* filename, Texture* tex )
{
	// ... tinyddsloader DDS loading removed ...
}
#endif

// TODO: DX12 - tinyddsloader removed, stub functions until DDS loading is rewritten
// GGMAX 2.96c: was a stub since tinyddsloader was dropped. The billboard pass needs texNoise
// (the dither source both the billboard PS and the mesh PS sample) - binding an INVALID texture
// makes Wicked's DescriptorBinder::flush dereference null and take the app down with an AV at
// 0x0, which is exactly how this surfaced. Loaded through the resource manager, which handles
// DDS everywhere else in the build. The Resource must be kept alive or the texture is freed.
static std::vector<wi::Resource> g_ftKeepAlive;
void GGTrees_LoadTextureDDS( const char* filename, Texture* tex )
{
	if ( tex == nullptr ) return;
	char filePath[ MAX_PATH ];
	strcpy_s( filePath, MAX_PATH, filename );
	GG_GetRealPath( filePath, 0 );
	wi::Resource res = wi::resourcemanager::Load( filePath );
	if ( !res.IsValid() ) return;
	g_ftKeepAlive.push_back( res );
	*tex = res.GetTexture();
}

// GGMAX Tier A (2026-08-02): the four legacy tree atlases (texTree 52.2 MB, texBranchesHigh
// 52.2, texTreeNormal 26.1, texTreeHigh 26.1 = 156.6 MB) are allocated on demand rather than at
// init, for exactly the reasons set out over GGGrass_EnsureLegacyTexArray():
//   * created EMPTY, and their only writer GGTrees_LoadTextureDDSIntoSlice is a disabled stub,
//   * their only readers are GGTrees' own draw functions, and every customDraw_* callback in
//     master_part1.cpp early-returns while ggterrain_use_wicked_terrain is set,
//   * shipping trees render through the Wicked object path with per-tree materials.
// Hooked to the legacy path's activation point so the atlases exist before anything binds them.
void GGTrees_CreateEmptyTexture( int width, int height, int mipLevels, int levels, Format format, Texture* tex );
static bool g_treesLegacyTexArraysCreated = false;
void GGTrees_EnsureLegacyTexArrays()
{
	if (g_treesLegacyTexArraysCreated) return;
	if (!ggtrees_initialised) return;
	g_treesLegacyTexArraysCreated = true;
	// billboard textures
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, Format::BC3_UNORM_SRGB, &texTree );
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, Format::BC1_UNORM, &texTreeNormal );
	// high detail tree textures
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, Format::BC1_UNORM_SRGB, &texTreeHigh );
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, Format::BC3_UNORM_SRGB, &texBranchesHigh );
}

// GGMAX 2.96: create + FILL just the two BILLBOARD atlases, for the distant-tree pass.
//
// GGTrees_EnsureLegacyTexArrays above is reached only from GGTerrain_Update's legacy tail, which
// early-returns on the shipping Wicked-terrain path - so on a normal level none of the four
// atlases exist at all. This is the billboard pass's own lazy init.
//
// Deliberately only two of the four: texTree (BC3, 52.2 MB) + texTreeNormal (BC1, 26.1 MB) =
// 78.3 MB, Lee-approved. The HIGH pair (texTreeHigh + texBranchesHigh, another 78.3 MB) stays
// absent - near trees render through the Wicked object path with their own per-tree materials
// and never sample them.
void GGTrees_LoadTextureDDSIntoSlice( const char* filename, Texture* tex, uint32_t arraySlice ); // fwd
bool GGTrees_LoadTextureDDSIntoSliceCmd( const char* filename, Texture* tex, uint32_t arraySlice, CommandList cmd ); // fwd
void GGTrees_LoadTextureDDS( const char* filename, Texture* tex ); // fwd
bool GGTrees_BillboardAtlasesReady(); // fwd
void GGTrees_EnsureBillboardAtlases(); // fwd
void GGTrees_ReleaseLevelAssets();     // fwd - GGMAX 3.54, defined in GGTrees_part2.cpp
static bool g_ftAtlasesReady = false;
char g_ftAtlasFailName[ MAX_PATH ] = { 0 };   // first path that failed to load
uint32_t g_ftAtlasSlices = 0;   // diagnostics: slices actually uploaded
// GGMAX 3.102: the type -> slice map the atlas was built with (-1 = type not in it), and the
// g_treeInstanceStamp it was last checked against - see GGTrees_RefreshBillboardAtlasTypes.
static int g_ftTypeSlice[ numTreeTypes ];
static uint32_t g_ftAtlasStamp = 0;
uint32_t g_ftAtlasRebuilds = 0;   // diagnostics: rebuilds because a placed type had no slice
uint32_t g_ftTypeScans = 0;       // diagnostics: placed-type scans run
void GGTrees_EnsureBillboardAtlases()
{
	if ( g_ftAtlasesReady ) return;
	if ( !ggtrees_initialised ) return;

	// GGMAX 2.99: PER-LEVEL SLICES. Previously this allocated 38 array slices in each atlas and
	// uploaded all 38, regardless of how many tree types the level actually places. Levels
	// typically use a handful. Now: scan the placed trees, allocate exactly that many slices,
	// and record each type's slice in the constant buffer so the shader can look it up.
	// Zero quality cost - it is the same texels, just none of the unused ones.
	int typeSlice[ numTreeTypes ];
	for ( uint32_t i = 0; i < numTreeTypes; i++ ) typeSlice[ i ] = -1;

	uint32_t sliceCount = 0;
	for ( uint32_t c = 0; c < numTreeChunks; c++ )
	{
		TreeChunk* pc = &pTreeChunks[ c ];
		for ( uint32_t j = 0; j < pc->pInstances.NumItems(); j++ )
		{
			const uint32_t t = pc->pInstances[ j ]->GetType();
			if ( t >= numTreeTypes ) continue;
			if ( typeSlice[ t ] < 0 ) typeSlice[ t ] = (int)sliceCount++;
		}
	}
	g_ftTypesUsed = sliceCount;
	for ( uint32_t i = 0; i < numTreeTypes; i++ ) g_ftTypeSlice[ i ] = typeSlice[ i ];   // GGMAX 3.102
	g_ftAtlasStamp = g_treeInstanceStamp;
	if ( sliceCount == 0 ) { g_ftAtlasesReady = true; return; }   // treeless level: no atlas at all

	// publish the mapping for the shader (spare CB float; -1 for absent types)
	for ( uint32_t i = 0; i < numTreeTypes; i++ )
		treeConstantData.tree_type[ i ].slice = (float)( typeSlice[ i ] < 0 ? 0 : typeSlice[ i ] );

	if ( !texTree.IsValid() )
		GGTrees_CreateEmptyTexture( 1024, 1024, 9, sliceCount, Format::BC3_UNORM_SRGB, &texTree );
	if ( !texTreeNormal.IsValid() )
		GGTrees_CreateEmptyTexture( 1024, 1024, 9, sliceCount, Format::BC1_UNORM, &texTreeNormal );

	// The dither/noise source. Both tree pixel shaders sample it; an invalid bind is fatal.
	if ( !texNoise.IsValid() )
		GGTrees_LoadTextureDDS( "treebank/noise.dds", &texNoise );   // GG_GetRealPath prepends Files/

	// Flag is set at the END, so a draw can never see half-built atlases.
	// ONE command list for all slice uploads.
	CommandList upcmd = wiGraphics::GetDevice()->BeginCommandList();
	char path[ MAX_PATH ];
	for ( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( typeSlice[ i ] < 0 ) continue;             // type not placed in this level
		if ( !g_GGTrees[ i ].trunk ) continue;
		const uint32_t sl = (uint32_t)typeSlice[ i ];
		if ( g_GGTrees[ i ].billboardFilename && g_GGTrees[ i ].billboardFilename[ 0 ] )
		{
			strcpy_s( path, "treebank/billboards/" );   // GG_GetRealPath already prepends Files/
			strcat_s( path, g_GGTrees[ i ].billboardFilename );
			if ( GGTrees_LoadTextureDDSIntoSliceCmd( path, &texTree, sl, upcmd ) ) g_ftAtlasSlices++;
			else if ( g_ftAtlasFailName[0] == 0 ) strcpy_s( g_ftAtlasFailName, path );
		}
		if ( g_GGTrees[ i ].billboardNormalFilename && g_GGTrees[ i ].billboardNormalFilename[ 0 ] )
		{
			strcpy_s( path, "treebank/billboards/" );
			strcat_s( path, g_GGTrees[ i ].billboardNormalFilename );
			GGTrees_LoadTextureDDSIntoSliceCmd( path, &texTreeNormal, sl, upcmd );
		}
	}
	g_ftAtlasesReady = true;   // GGMAX 2.97: only now is it safe for a draw to bind these

}

bool GGTrees_BillboardAtlasesReady() { return g_ftAtlasesReady && texTree.IsValid(); }

// GGMAX 3.53: the 2.99 per-level atlas was built ONCE PER PROCESS, never once per level.
// g_ftAtlasesReady was set true and never cleared by anything - not a level load, not a
// project change, not GGTrees_WickedShutdown. Two consequences, and the second is the bad one:
//
//   1. Every level after the first kept the FIRST level's type->slice mapping and its
//      uploaded images, so trees drew as the wrong species - or all as the same one.
//      (Lee, 2026-09-18: "they all use the same tree image".)
//   2. Worse: the readiness flag is also the GATE that keeps the billboard pass from drawing
//      DURING a level load. On the first level it is false until the atlas is built, so the
//      pass stays silent while the chunks are rebuilt. On every later level it was already
//      true, so the pass drew straight through the load -> DXGI_ERROR_DEVICE_HUNG, 5/5.
//
// Called wherever the level's tree instances are replaced. Releasing the textures is safe:
// Wicked defers the actual destroy by framecount (Texture_DX12::~Texture_DX12).
void GGTrees_InvalidateBillboardAtlases()
{
	g_ftAtlasesReady = false;
	g_ftAtlasSlices  = 0;
	g_ftTypesUsed    = 0;
	g_ftAtlasFailName[ 0 ] = 0;
	// The atlas is sized to the OLD level's slice count, so it cannot be reused - drop it and
	// let GGTrees_EnsureBillboardAtlases create one sized for the new level.
	texTree       = Texture();
	texTreeNormal = Texture();
	for ( uint32_t i = 0; i < numTreeTypes; i++ ) treeConstantData.tree_type[ i ].slice = 0.0f;
}

// GGMAX 3.102: the 2.99 atlas holds only the types placed when it was BUILT, and nothing rebuilt
// it when a type arrived later - painting a newly selected species, GGTrees_ChangeDensity (every
// biome pick re-types all the trees from the palette), an undo. Those trees drew slice 0, another
// species' billboard, in the distance. Every one of those paths bumps g_treeInstanceStamp, so on a
// stamp change collect the placed types and rebuild only when one has no slice. A paint stroke
// bumps the stamp every frame, hence at most one scan per 30 frames.
static void GGTrees_RefreshBillboardAtlasTypes()
{
	static uint32_t s_calls = 0, s_lastScan = 0;
	s_calls++;
	if ( !g_ftAtlasesReady ) return;                    // not built yet - the build scans anyway
	if ( g_ftAtlasStamp == g_treeInstanceStamp ) return;
	if ( s_calls - s_lastScan < 30 ) return;
	s_lastScan = s_calls;
	g_ftAtlasStamp = g_treeInstanceStamp;
	g_ftTypeScans++;
	uint64_t placed = 0;
	for ( uint32_t c = 0; c < numTreeChunks; c++ )
	{
		TreeChunk* pc = &pTreeChunks[ c ];
		for ( uint32_t j = 0; j < pc->pInstances.NumItems(); j++ )
		{
			const uint32_t t = pc->pInstances[ j ]->GetType();
			if ( t < numTreeTypes ) placed |= 1ULL << t;
		}
	}
	for ( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( ( placed & ( 1ULL << i ) ) && g_ftTypeSlice[ i ] < 0 )
		{
			g_ftAtlasRebuilds++;
			GGTrees_InvalidateBillboardAtlases();   // GGTrees_EnsureBillboardAtlases rebuilds it next
			return;
		}
	}
}

void GGTrees_CreateEmptyTexture( int width, int height, int mipLevels, int levels, Format format, Texture* tex )
{
	if (!ggtrees_initialised) return;
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

// TODO: DX12 - tinyddsloader removed, stub function until DDS loading is rewritten
// GGMAX 2.96: was "stub - DDS loading disabled" since tinyddsloader was dropped in the DX12
// port. With the billboard pass alive the two BILLBOARD atlases have to be filled again, so
// delegate to GGTerrain's DDS loader, which is a working DX12 implementation of exactly this
// (parses the header itself, builds a staging texture, CopyTexture's each mip into the array
// slice). No new loader, no third-party header.
//
// Only texTree and texTreeNormal are filled - see GGTrees_FillBillboardAtlases. The HIGH-detail
// pair (texTreeHigh, texBranchesHigh, 78.3 MB) stays empty on purpose: near trees render through
// the Wicked object path with their own per-tree materials and never sample these.
// GGMAX 2.96c: takes the command list from the caller. The first version opened a FRESH
// command list per slice - 76 of them - from inside a draw callback. The copies never landed,
// so the atlas stayed blank and every billboard pixel discarded on alpha. One list for the
// whole upload, opened once by GGTrees_EnsureBillboardAtlases.
bool GGTrees_LoadTextureDDSIntoSliceCmd( const char* filename, Texture* tex, uint32_t arraySlice, CommandList cmd )
{
	if ( !ggtrees_initialised ) return false;
	if ( tex == nullptr || !tex->IsValid() ) return false;
	return ::GG_LoadDDSIntoTextureSlice( filename, tex, arraySlice, cmd );
}
void GGTrees_LoadTextureDDSIntoSlice( const char* filename, Texture* tex, uint32_t arraySlice )
{
	if ( !ggtrees_initialised ) return;
	if ( tex == nullptr || !tex->IsValid() ) return;
	CommandList cmd = wiGraphics::GetDevice()->BeginCommandList();
	::GG_LoadDDSIntoTextureSlice( filename, tex, arraySlice, cmd );
}

TreeChunk* GGTrees_GetChunk( float x, float z )
{
	float fX = (x / treeArea) + 0.5f;
	fX *= treeSplit;
	float fZ = (z / treeArea) + 0.5f;
	fZ *= treeSplit;

	int iX = (int) fX;
	int iZ = (int) fZ;

	if ( iX < 0 || iZ < 0 || iX >= (int)treeSplit || iZ >= (int)treeSplit ) return 0;

	uint32_t index = iZ * treeSplit + iX;
	return &pTreeChunks[ index ];
}

// the tree area covers the editable area of the terrain, so levels up to +-100000 keep the original area and layout
float GGTrees_GetRequiredArea()
{
	float area = ggterrain_global_render_params2.editable_size * 2;
	if ( area < GGTREES_MIN_AREA ) area = GGTREES_MIN_AREA;
	if ( area > GGTREES_MAX_AREA ) area = GGTREES_MAX_AREA;
	return area;
}

void GGTrees_SetArea( float area )
{
	treeArea = area;
	treeAreaPerChunk = treeArea / treeSplit;
}

// puts every tree in the chunk at its position, trees outside the tree area are hidden
void GGTrees_SortIntoChunks()
{
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		pTreeChunks[ i ].pInstances.Clear();
	}
	pInvisibleTrees.Clear();

	for( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		InstanceTree* pInstance = &pAllTrees[ i ];
		TreeChunk* pChunk = GGTrees_GetChunk( pInstance->x, pInstance->z );
		if ( pChunk ) pChunk->pInstances.AddItem( pInstance );
		else pInstance->SetVisible( 0 );

		if ( !pInstance->IsVisible() ) pInvisibleTrees.AddItem( i );
	}
}

// height and normal Y from the terrain function, for trees outside the terrain LOD set. Much slower than the LOD lookup
int GGTrees_GetTerrainHeight( float x, float z, float* height, float* ny )
{
	const float step = 25.0f;
	float h, hx0, hx1, hz0, hz1;
	if ( !GGTerrain_GetHeight( x, z, &h, 1 ) ) return 0;
	GGTerrain_GetHeight( x - step, z, &hx0, 1 );
	GGTerrain_GetHeight( x + step, z, &hx1, 1 );
	GGTerrain_GetHeight( x, z - step, &hz0, 1 );
	GGTerrain_GetHeight( x, z + step, &hz1, 1 );

	float dx = hx1 - hx0;
	float dz = hz1 - hz0;
	*height = h;
	*ny = (step * 2) / sqrt( dx*dx + dz*dz + step*step*4 );
	return 1;
}

// the terrain in this area has changed, trees outside the terrain LOD set must find their height again
void GGTrees_InvalidateHeights( float minX, float minZ, float maxX, float maxZ )
{
	if (!ggtrees_initialised) return;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( minX > aabb._max.x || minZ > aabb._max.z || maxX < aabb._min.x || maxZ < aabb._min.z ) continue;

		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			if ( pInstance->x < minX || pInstance->x > maxX || pInstance->z < minZ || pInstance->z > maxZ ) continue;
			pTreeHeightValid[ pInstance - pAllTrees ] = 0;
		}
	}
}

void GGTrees_RepopulateInstances()
{
	GGTrees_InvalidateBillboardAtlases();   // GGMAX 3.53: new instance set = new atlas
	GGTrees_SetArea( GGTrees_GetRequiredArea() );
	memset( pTreeHeightValid, 0, sizeof(pTreeHeightValid) );

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		pTreeChunks[ i ].pInstances.Clear();
	}
	pInvisibleTrees.Clear();

	uint32_t numTypes = 0;
	uint64_t values = ggtrees_global_params.paint_tree_bitfield;
	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( values & 1 ) numTypes++;
		values >>= 1;
	}
	if ( numTypes == 0 ) numTypes = 1;

	// place trees in a regular grid then slightly offset them from their grid position in a random direction

	// create tree grid
	float gridX = sqrt( (float)numTotalTrees );
	uint32_t iGridX = (int) gridX;
	uint32_t iGridZ = numTotalTrees / iGridX;
	float gridSizeX = treeArea / (float)iGridX;
	float gridSizeZ = treeArea / (float)iGridZ;
	float halfArea = treeArea * 0.5f;

	// start with sorted array
	std::vector<uint32_t> treeLocation( numTotalTrees );
	for( uint32_t z = 0; z < iGridZ; z++ )
	{
		for( uint32_t x = 0; x < iGridX; x++ )
		{
			uint32_t index = z * iGridX + x;
			treeLocation[ index ] = (z << 16) | x;
		}
	}

	// randomise it
	for( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		uint32_t index = Random() << 16;
		index |= Random();
		index %= numTotalTrees;

		uint32_t temp = treeLocation[ i ];
		treeLocation[ i ] = treeLocation[ index ];
		treeLocation[ index ] = temp;
	}

	const float gridSpacing = 0.18f;
	const float gridScale = (1.0f - gridSpacing*2.0f) / 65535.0f;

	for( uint32_t i = 0; i < iGridX*iGridZ; i++ )
	{
		InstanceTree* pInstance = &pAllTrees[ i ];
		
		uint32_t type = i;
		type = 214313*type + 2539011;
		type = 214313*type + 2539011;
		type = 214313*type + 2539011;
		type ^= (type >> 16);
		type ^= (type >> 8);
		type %= numTypes;

		uint32_t chosenType = 0;
		uint32_t count = 0;
		uint64_t values = ggtrees_global_params.paint_tree_bitfield;
		for( uint32_t k = 0; k < numTreeTypes; k++ )
		{
			if ( values & 1 ) 
			{
				if ( count == type )
				{
					chosenType = k;
					break;
				}
				count++;
			}
			values >>= 1;
		}

		// get tree grid location
		uint32_t location = treeLocation[ i ];
		uint32_t iX = location & 0xFFFF;
		uint32_t iZ = location >> 16;

		bool flattened = pInstance->IsFlattened();
		// offset the tree slightly from its grid position
		pInstance->x = gridSizeX * ((float)iX + gridSpacing + (Random() * gridScale)) - halfArea;
		pInstance->y = 0; // to be determined later
		pInstance->z = gridSizeZ * ((float)iZ + gridSpacing + (Random() * gridScale)) - halfArea;
		pInstance->SetData( i, chosenType, 1 );
		pInstance->SetScale( Random(ggtrees_global_params.paint_scale_random_low, ggtrees_global_params.paint_scale_random_high) );
		if ( flattened ) pInstance->SetFlattened( 1 );

		TreeChunk* pChunk = GGTrees_GetChunk( pInstance->x, pInstance->z );
		if ( pChunk ) 
		{
			pChunk->pInstances.AddItem( pInstance );

			int test = i % 100;
			if ( test >= ggtrees_global_params.paint_density )
			{
				pInstance->SetVisible( 0 );
				pInvisibleTrees.AddItem( i );
			}
		}
		else
		{
			pInstance->SetVisible( 0 );
			pInvisibleTrees.AddItem( i );
		}
	}

	// set remaining trees as undefined location
	for( uint32_t i = iGridX*iGridZ; i < numTotalTrees; i++ )
	{
		InstanceTree* pInstance = &pAllTrees[ i ];
		pInstance->x = treeArea*2;
		pInstance->z = treeArea*2;
		pInstance->SetVisible( 0 );
		pInvisibleTrees.AddItem( i );
	}

	GGTrees_UpdateInstances( 0 );
}

void GGTrees_ChangeDensity( int density)
{
	ggtrees_global_params.paint_density = density;

	uint32_t numTypes = 0;
	uint64_t values = ggtrees_global_params.paint_tree_bitfield;
	for (uint32_t i = 0; i < numTreeTypes; i++)
	{
		if (values & 1) numTypes++;
		values >>= 1;
	}
	if (numTypes == 0) numTypes = 1;

	for (uint32_t j = 0; j < numTotalTrees; j++)
	{
		InstanceTree* pInstance = &pAllTrees[j];
		uint32_t type = j;
		type = 214313 * type + 2539011;
		type = 214313 * type + 2539011;
		type = 214313 * type + 2539011;
		type ^= (type >> 16);
		type ^= (type >> 8);
		type %= numTypes;

		uint32_t chosenType = 0;
		uint32_t count = 0;
		uint64_t values = ggtrees_global_params.paint_tree_bitfield;
		for (uint32_t k = 0; k < numTreeTypes; k++)
		{
			if (values & 1)
			{
				if (count == type)
				{
					chosenType = k;
					break;
				}
				count++;
			}
			values >>= 1;
		}

		pInstance->SetData(j, chosenType, 1);
		pInstance->SetScale(Random(ggtrees_global_params.paint_scale_random_low, ggtrees_global_params.paint_scale_random_high));
		int test = j % 100;
		if (test >= ggtrees_global_params.paint_density)
		{
			pInstance->SetVisible(0);
			pInvisibleTrees.AddItem(j);
		}
		else
		{
			if (!pInstance->IsVisible()) pInvisibleTrees.RemoveItem(pInstance->GetID());
			pInstance->SetVisible(1);
		}
	}
}

uint32_t GGTrees_GetNumHighDetail()
{
	uint32_t total = 0;
	for( uint32_t i = 0; i < numTreeTypes; i++ ) total += numTreeInstancesHigh[ i ];
	return total;
}

// only call this when tree heights need to be updated, e.g. when the terrain has changed
int GGTrees_UpdateInstances( int accurate )
{
	// heights are written directly (pInstance->y), bypassing the stamped setters
	g_treeInstanceStamp++;
	GGTrees_MarkAllProxyChunksDirty();   // heights written directly below (bypass setters)
	if (!ggtrees_initialised) return 0;
	if ( !GGTerrain_IsReady() ) return 0;

	// the editable area has grown past the tree area, e.g. the level size was changed in the editor
	float area = GGTrees_GetRequiredArea();
	if ( area > treeArea )
	{
		GGTrees_SetArea( area );
		GGTrees_SortIntoChunks();
	}

	for( uint32_t j = 0; j < numTotalTrees; j++ )
	{
		InstanceTree* pInstance = &pAllTrees[ j ];

		float height, nx, ny, nz;
		if ( !GGTerrain_GetNormal( pInstance->x, pInstance->z, &nx, &ny, &nz )
		  || !GGTerrain_GetHeight( pInstance->x, pInstance->z, &height, accurate ) )
		{
			// outside the terrain LOD set, use the terrain function once for trees on the level
			if ( pTreeHeightValid[ j ] ) continue;
			if ( !GGTrees_GetChunk( pInstance->x, pInstance->z ) ) continue;
			if ( !GGTrees_GetTerrainHeight( pInstance->x, pInstance->z, &height, &ny ) ) continue;
		}
		pTreeHeightValid[ j ] = 1;

		float adjustment = (1 - ny) * 75 * pInstance->GetScaleFloat();
		pInstance->y = height - 10 - adjustment;
		
		if ( (height > g.gdefaultwaterheight + ggtrees_global_params.water_dist && ny > 0.7) || pInstance->IsUserMoved() )
		{
			pInstance->SetInvalid( 0 );
		}
		else 
		{
			pInstance->SetInvalid( 1 );
		}
	}

	ggtrees_internal_params.minTotalHeight = 1e20f;
	ggtrees_internal_params.maxTotalHeight = -1e20f;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		pChunk->Update();
		if ( pChunk->minHeight < ggtrees_internal_params.minTotalHeight ) ggtrees_internal_params.minTotalHeight = pChunk->minHeight;
		if ( pChunk->maxHeight > ggtrees_internal_params.maxTotalHeight ) ggtrees_internal_params.maxTotalHeight = pChunk->maxHeight;
	}

	// Trees are hidden, show them again if they are enabled.
	if (ggtrees_global_params.hide_until_update)
	{
		ggtrees_global_params.hide_until_update = 0;
		if(t.visuals.bEndableTreeDrawing)
			ggtrees_global_params.draw_enabled = 1;
	}
	
	return 1;
}

#ifdef ONLYLOADWHENUSED
// GGMAX 3.102: computes treeMaxHeight and NOTHING ELSE. It used to upload each newly selected
// palette type's images into slice = TYPE index of texTree / texTreeNormal / texTreeHigh /
// texBranchesHigh, run from GGTrees_Update whenever paint_tree_bitfield changed. That matched
// DX11, where all four are full 38-slice arrays. In DX12 the HIGH pair is never created, and
// texTree / texTreeNormal became the 2.99 per-level billboard atlases - one slice per type the
// level PLACES, in placement order, tree_type[t].slice mapping type -> slice. So every palette
// click wrote one species over another's slice, or, for a type index past the slice count, past
// the end of the texture (Autumn Birch 3 = slice 37 of a 12-slice atlas): GPU memory corruption,
// the terrain torn into white sheets. GGTrees_EnsureBillboardAtlases alone fills the atlas, and
// GGTrees_RefreshBillboardAtlasTypes rebuilds it when a placed type has no slice.
void GGTrees_LoadTextures(bool bInit = false)
{
	for (int i = 0; i < numTreeTypes; i++)
		if (g_GGTrees[i].height > treeMaxHeight) treeMaxHeight = g_GGTrees[i].height;
}
#endif

void GGTrees_Init()
{
	ggtrees_initialised = 1;
	GraphicsDevice* device = wiGraphics::GetDevice();

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesVS, "GGTreesVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesPS, "GGTreesPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesPrepassVS, "GGTreesPrepassVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesPrepassPS, "GGTreesPrepassPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesShadowVS, "GGTreesShadowMapVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesShadowPS, "GGTreesShadowMapPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesHighVS, "GGTreesHighVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesHighPS, "GGTreesHighPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesHighPrepassVS, "GGTreesHighPrepassVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesHighPrepassPS, "GGTreesHighPrepassPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesHighShadowVS, "GGTreesHighShadowMapVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesHighShadowPS, "GGTreesHighShadowMapPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderBranchesHighVS, "GGTreeBranchesHighVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderBranchesHighPS, "GGTreeBranchesHighPS.cso" );
									  
	wiRenderer::LoadShader( ShaderStage::VS, shaderBranchesHighPrepassVS, "GGTreeBranchesHighPrepassVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderBranchesHighPrepassPS, "GGTreeBranchesHighPrepassPS.cso" );
									  
	wiRenderer::LoadShader( ShaderStage::VS, shaderBranchesHighShadowVS, "GGTreeBranchesHighShadowMapVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderBranchesHighShadowPS, "GGTreeBranchesHighShadowMapPS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderTreesHighEnvProbeVS, "GGTreesHighEnvProbeVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderTreesHighEnvProbePS, "GGTreesHighEnvProbePS.cso" );

	wiRenderer::LoadShader( ShaderStage::VS, shaderBranchesHighEnvProbeVS, "GGTreeBranchesHighEnvProbeVS.cso" );
	wiRenderer::LoadShader( ShaderStage::PS, shaderBranchesHighEnvProbePS, "GGTreeBranchesHighEnvProbePS.cso" );

	GGTrees_LoadTextureDDS( "Files/treebank/noise.dds", &texNoise );

	// GGMAX Tier A (2026-08-02): the four 1024x1024 x numTreeTypes atlases (156.6 MB together)
	// are NOT created here any more — see GGTrees_EnsureLegacyTexArrays() for the reasoning and
	// the on-demand creation.

#ifdef ONLYLOADWHENUSED
	for (int i = 0; i < numTreeTypes; i++)
	{
		bTreeTextureUploaded[i] = false;
		bTreeTextureForceUploaded[i] = false;
		treeInstancesHigh[i] = new InstanceTreeGPU[1000];
		treeInstancesHighShadow[i] = new InstanceTreeGPU[1000];
	}
	last_paint_tree_bitfield = ggtrees_global_params.paint_tree_bitfield;
	GGTrees_LoadTextures(true);
#else
	char path[ 1024 ];
	for( int i = 0; i < numTreeTypes; i++ )
	{
		if ( g_GGTrees[ i ].height > treeMaxHeight ) treeMaxHeight = g_GGTrees[ i ].height;

		strcpy_s( path, "Files/treebank/billboards/" );
		strcat_s( path, g_GGTrees[ i ].billboardFilename );
		GGTrees_LoadTextureDDSIntoSlice( path, &texTree, i );

		strcpy_s( path, "Files/treebank/billboards/" );
		strcat_s( path, g_GGTrees[ i ].billboardNormalFilename );
		GGTrees_LoadTextureDDSIntoSlice( path, &texTreeNormal, i );

		strcpy_s( path, "Files/treebank/textures/" );
		strcat_s( path, g_GGTrees[ i ].trunk->textureName );
		GGTrees_LoadTextureDDSIntoSlice( path, &texTreeHigh, i );

		if ( g_GGTrees[ i ].branches )
		{
			strcpy_s( path, "Files/treebank/textures/" );
			strcat_s( path, g_GGTrees[ i ].branches->textureName );
			GGTrees_LoadTextureDDSIntoSlice( path, &texBranchesHigh, i );
		}

		treeInstancesHigh[ i ] = new InstanceTreeGPU[ 1000 ];
		treeInstancesHighShadow[ i ] = new InstanceTreeGPU[ 1000 ];
	}
#endif

	//GGTerrain_DecompressImage( "E:/Programs/GameGuruMAX/Max/Files/pinetree_BC7.dds", "E:/Programs/GameGuruMAX/Max/Files/pinetree_RGB.dds" );

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		pTreeChunks[ i ].x = i % treeSplit;
		pTreeChunks[ i ].z = i / treeSplit;
	}

	GGTrees_RepopulateInstances();
	GGTrees_RasterizeTreeMap();
	GGTrees_UploadTreeMap();

	// raster state
	RasterizerState rastState = {};
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
	DepthStencilState depthStateOpaque = {};
	depthStateOpaque.depth_enable = true;
	depthStateOpaque.depth_func = ComparisonFunc::GREATER_EQUAL;
	depthStateOpaque.stencil_enable = false;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ALL;

	// blend state
	BlendState blendStateOpaque = {};
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
	InputLayout inputLayout;
	inputLayout.elements = {
		{ "POSITION", 0, wiGraphics::Format::R32G32_FLOAT,    0, 0,  InputClassification::PER_VERTEX_DATA },
		{ "OFFSET",   0, wiGraphics::Format::R32G32B32_FLOAT, 1, 0,  InputClassification::PER_INSTANCE_DATA },
		{ "DATA",     0, wiGraphics::Format::R32_UINT,        1, 12, InputClassification::PER_INSTANCE_DATA },
	};

	InputLayout inputLayoutHigh;
	inputLayoutHigh.elements = {
		{ "POSITION", 0, wiGraphics::Format::R32G32B32_FLOAT, 0, 0,  InputClassification::PER_VERTEX_DATA },
		{ "INORMAL",  0, wiGraphics::Format::R8G8B8A8_UNORM,  0, 12, InputClassification::PER_VERTEX_DATA },
		{ "UV",       0, wiGraphics::Format::R32G32_FLOAT,    0, 16, InputClassification::PER_VERTEX_DATA },
		{ "OFFSET",   0, wiGraphics::Format::R32G32B32_FLOAT, 1, 0,  InputClassification::PER_INSTANCE_DATA },
		{ "DATA",     0, wiGraphics::Format::R32_UINT,        1, 12, InputClassification::PER_INSTANCE_DATA },
	};

	// pipeline state object
	PipelineStateDesc desc = {};

	// GGMAX 2.96: snapshot the CURRENT state into persistent storage and repoint desc at it,
	// so the desc survives this function AND each PSO keeps the state it was created with.
	auto GGTreeCreatePSO = [&]( PipelineState* out )
	{
		const uint32_t slot = g_ggtreePsoSlot++;
		if ( slot >= GGTREE_PSO_MAX ) { return; }   // never expected; fail closed, not corrupt
		g_ggtreePsoRS [ slot ] = rastState;
		g_ggtreePsoDSS[ slot ] = depthStateOpaque;
		g_ggtreePsoBS [ slot ] = blendStateOpaque;
		if ( desc.il ) g_ggtreePsoIL[ slot ] = *desc.il;
		desc.rs  = &g_ggtreePsoRS [ slot ];
		desc.dss = &g_ggtreePsoDSS[ slot ];
		desc.bs  = &g_ggtreePsoBS [ slot ];
		desc.il  = &g_ggtreePsoIL [ slot ];
		device->CreatePipelineState( &desc, out );
	};
	desc.vs = &shaderTreesVS;
	desc.ps = &shaderTreesPS;

	desc.il = &inputLayout;
	desc.pt = PrimitiveTopology::TRIANGLELIST;
	desc.rs = &rastState;
	desc.dss = &depthStateOpaque;
	desc.bs = &blendStateOpaque;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ZERO;
	GGTreeCreatePSO( &psoTrees );

	// GGMAX 3.06: same colour PSO with the depth TEST disabled, for debug mode 5. If the black
	// is fragments being depth-REJECTED (colour VS depth landing behind the separately-compiled
	// prepass VS depth under GREATER_EQUAL), this clears it while depth writes stay on.
	depthStateOpaque.depth_func = ComparisonFunc::ALWAYS;
	GGTreeCreatePSO( &psoTreesDepthAlways );
	depthStateOpaque.depth_func = ComparisonFunc::GREATER_EQUAL;

	rastState.cull_mode = CullMode::BACK;
	desc.vs = &shaderTreesHighVS;
	desc.ps = &shaderTreesHighPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.alpha_to_coverage_enable = false;
	GGTreeCreatePSO( &psoTreesHigh );

	rastState.cull_mode = CullMode::NONE;
	desc.vs = &shaderBranchesHighVS;
	desc.ps = &shaderBranchesHighPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.alpha_to_coverage_enable = true;
	GGTreeCreatePSO( &psoBranchesHigh );

	rastState.cull_mode = CullMode::BACK;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ALL;
	desc.vs = &shaderTreesHighEnvProbeVS;
	desc.ps = &shaderTreesHighEnvProbePS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.alpha_to_coverage_enable = false;
	GGTreeCreatePSO( &psoTreesHighEnvProbe );

	rastState.cull_mode = CullMode::NONE;
	desc.vs = &shaderBranchesHighEnvProbeVS;
	desc.ps = &shaderBranchesHighEnvProbePS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.alpha_to_coverage_enable = true;
	GGTreeCreatePSO( &psoBranchesHighEnvProbe );

	// prepass pipeline state
	// GGMAX 3.06: THE PREPASS USES THE COLOUR PASS'S VERTEX SHADER ON PURPOSE.
	// psoTrees has depth_write_mask ZERO and depth_func GREATER_EQUAL, so a billboard's colour
	// fragments only survive if their depth matches the depth THIS prepass wrote. That made the
	// two passes' vertex maths a correctness contract, and two separate compilations of the same
	// source do NOT honour it: the quad's camera-facing rotation runs through rsqrt (an
	// approximate instruction - the source comment even says "approximation"), so a different
	// instruction schedule moves the quad in WORLD space, not merely one ULP of depth. Fragments
	// landing behind the prepass depth were rejected, no gbuffer texel was written, and the quad
	// showed BLACK - view-dependent and flickering, which is exactly how Lee described it.
	// Sharing the compiled VS makes the depths bit-identical by construction. Legal because the
	// prepass PS input signature (position/worldPos/clip/uv/data) is an exact SUBSET of this VS's
	// output - it simply ignores the extra TEXCOORD4 'dir' - and both PSOs use the same
	// inputLayout. ★ It also permanently retires the 3.04 bug class between these two passes:
	// a clip or transform added to the VS can no longer reach one pass and not the other.
	// GGTreesPrepassVS.hlsl is now unused by this PSO; kept only for the shadow/probe variants.
	desc.vs = &shaderTreesVS;
	desc.ps = &shaderTreesPrepassPS;
	desc.il = &inputLayout;
	depthStateOpaque.depth_write_mask = DepthWriteMask::ALL;
	// ★★★ GGMAX 3.75: NO COLOUR WRITES FROM THE PREPASS.
	//
	// In DX11 the prepass's render target 0 was VELOCITY, and GGTreesPrepassPS still declares
	// that layout (float4 velocity : SV_TARGET0) and writes float4(0,0,0,alpha) into it.
	// In DX12 that same target is the VISIBILITY BUFFER: wiRenderer.h format_idbuffer =
	// R32_UINT, holding a packed PrimitiveID, and visibility_resolveCS.hlsl RECONSTRUCTS the
	// depth that every texture_depth consumer reads from it - where a zero ID means SKY:
	//     if (any(primitiveID)) { ...depth = saturate(tmp.z); } else { /* sky: */ depth = 0; }
	// texture_depth is depthBuffer_Copy (wiRenderPath3D.cpp:800), and NOTHING copies the real
	// depth buffer into it - DX11 did, DX12 does not. So the billboards were not merely absent
	// from that depth: with ColorWrite::ENABLE_ALL and depth_func GREATER_EQUAL this prepass
	// OVERWROTE the hill's valid ID with zero at every billboard pixel, and the volumetric
	// cloud behind the hill was then composited over the tree (Lee, Aztec Game Kit Teaser).
	//
	// Disabling colour writes costs nothing and loses nothing: the silhouette comes from the
	// shader's own `if ( alpha < 0.3 ) discard;`, not from the colour output, and no GG code
	// reads the ID buffer (customDraw_AfterPrepass is commented out in master_part1.cpp).
	// alpha_to_coverage is deliberately LEFT ON - the write mask does not gate it, and it is
	// what feathers the cutout into the depth buffer.
	//
	// ⚠ This restores the hill BEHIND a billboard, it does not make the billboard itself an
	// occluder - its pixels against open SKY still read as sky, because nothing writes an ID
	// there. Giving GG draws a real PrimitiveID is the full fix and is NOT cheap: unpack()
	// calls load_meshlet BEFORE validating, so a synthetic ID reads out of bounds.
	// ⚠ 3.77: independent blend, or RT1 inherits RT0's mask. In D3D12 a BlendState with
	// independent_blend_enable false applies render_target[0] to EVERY bound target, so
	// masking RT0 off would silently mask off the depth we now write to RT1.
	blendStateOpaque.independent_blend_enable = true;
	blendStateOpaque.render_target[0].render_target_write_mask = ColorWrite::DISABLE;
	blendStateOpaque.render_target[1] = blendStateOpaque.render_target[0];
	blendStateOpaque.render_target[1].render_target_write_mask = ColorWrite::ENABLE_ALL;
	GGTreeCreatePSO( &psoTreesPrepass );

	// GGMAX 3.06: same PSO with depth writes OFF, for debug mode 4. Lee's test: draw every quad
	// pixel, discard nothing, and write no depth. The billboard COLOUR pso already has
	// depth_write_mask ZERO, so this prepass is the ONLY thing billboards put in the depth
	// buffer - swapping this in makes depth the single changing variable.
	depthStateOpaque.depth_write_mask = DepthWriteMask::ZERO;
	GGTreeCreatePSO( &psoTreesPrepassNoDepth );
	depthStateOpaque.depth_write_mask = DepthWriteMask::ALL;
	// 3.75/3.77: hand the shared blend state back before the HIGH prepass PSOs are built.
	blendStateOpaque.independent_blend_enable = false;
	blendStateOpaque.render_target[0].render_target_write_mask = ColorWrite::ENABLE_ALL;

	rastState.cull_mode = CullMode::BACK;
	desc.vs = &shaderTreesHighPrepassVS;
	desc.ps = &shaderTreesHighPrepassPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.alpha_to_coverage_enable = false;
	GGTreeCreatePSO( &psoTreesHighPrepass );

	rastState.cull_mode = CullMode::NONE;
	desc.vs = &shaderBranchesHighPrepassVS;
	desc.ps = &shaderBranchesHighPrepassPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.alpha_to_coverage_enable = true;
	GGTreeCreatePSO( &psoBranchesHighPrepass );

	// shadow pipeline state
	rastState.depth_bias = -1;
	rastState.slope_scaled_depth_bias = -4.0f;
	rastState.multisample_enable = false;
	desc.vs = &shaderTreesShadowVS;
	desc.ps = &shaderTreesShadowPS;
	desc.il = &inputLayout;
	rastState.depth_clip_enable = false;
	blendStateOpaque.alpha_to_coverage_enable = false;
	GGTreeCreatePSO( &psoTreesShadow );
	desc.vs = &shaderTreesHighShadowVS;
	desc.ps = &shaderTreesHighShadowPS;
	desc.il = &inputLayoutHigh;
	rastState.cull_mode = CullMode::BACK;
	GGTreeCreatePSO( &psoTreesHighShadow );
	desc.vs = &shaderBranchesHighShadowVS;
	desc.ps = &shaderBranchesHighShadowPS;
	rastState.cull_mode = CullMode::NONE;
	GGTreeCreatePSO( &psoBranchesHighShadow );
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
	samplerDesc.filter = Filter::MIN_MAG_LINEAR_MIP_POINT;
	device->CreateSampler( &samplerDesc, &samplerBilinearWrap );

	// constant buffer
	GPUBufferDesc bd = {};
	bd.usage = Usage::DEFAULT;
	bd.size = sizeof(TreeCB);
	bd.bind_flags = BindFlag::CONSTANT_BUFFER;
	//bd.CPUAccessFlags = 0; // removed in DX12 API
	bd.misc_flags = ResourceMiscFlag::NONE;
	wiGraphics::GetDevice()->CreateBuffer( &bd, nullptr, &treeConstantBuffer );

	// vertex buffer
	GPUBufferDesc bufferDesc = {};
	SubresourceData data = {};
	data.data_ptr = g_VerticesTreeBillboard;
	bufferDesc.size = sizeof(g_VerticesTreeBillboard);
	bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
	//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
	bufferDesc.misc_flags = ResourceMiscFlag::NONE;
	wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferTreeVertices );

	// index buffer
	data.data_ptr = g_IndicesTreeBillboard;
	bufferDesc.size = sizeof(g_IndicesTreeBillboard);
	bufferDesc.bind_flags = BindFlag::INDEX_BUFFER;
	//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
	bufferDesc.misc_flags = ResourceMiscFlag::NONE;
	wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferTreeIndices );

	// vertex buffer high detail
	bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
	//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
	bufferDesc.misc_flags = ResourceMiscFlag::NONE;

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		data.data_ptr = g_GGTrees[ i ].trunk->pVertices;
		bufferDesc.size = g_GGTrees[ i ].trunk->numVertices * sizeof(VertexTreeHigh);
		wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferTreeHighVertices[i] );
	}

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( !g_GGTrees[ i ].branches ) continue; // some don't have branches
		data.data_ptr = g_GGTrees[ i ].branches->pVertices;
		bufferDesc.size = g_GGTrees[ i ].branches->numVertices * sizeof(VertexTreeHigh);
		wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferBranchesHighVertices[i] );
	}

	// index buffer high detail
	bufferDesc.bind_flags = BindFlag::INDEX_BUFFER;
	//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
	bufferDesc.misc_flags = ResourceMiscFlag::NONE;

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		data.data_ptr = g_GGTrees[ i ].trunk->pIndices;
		bufferDesc.size = g_GGTrees[ i ].trunk->numIndices * sizeof(uint16_t);
		wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferTreeHighIndices[i] );
	}

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( !g_GGTrees[ i ].branches ) continue; // some don't have branches
		data.data_ptr = g_GGTrees[ i ].branches->pIndices;
		bufferDesc.size = g_GGTrees[ i ].branches->numIndices * sizeof(uint16_t);
		wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferBranchesHighIndices[i] );
	}

	undosys_terrain_settreecount(numTotalTrees, sizeof(InstanceTree), numTreeChunks, sizeof(TreeChunk));
}

bool GGTrees_GetDefaultDataV2(char *filename)
{
	if (!filename) return(false);
	bool bRet = false;
	uint32_t dataSize = GGTrees_GetDataSize() * 4;
	char *data = new char[dataSize];
	if (data)
	{
		GGTrees_GetEmptyData((float*)data);
		if (FileExist(filename) == 1)
		{
			// Open file to be read
			HANDLE hreadfile = GG_CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hreadfile != INVALID_HANDLE_VALUE)
			{
				// Read file into memory
				DWORD bytesread;
				ReadFile(hreadfile, data, dataSize, &bytesread, NULL);
				CloseHandle(hreadfile);
				GGTrees::GGTrees_SetData((float*)data);
				bRet = true;
			}
		}
		delete(data);
	}
	return bRet;
}

int GGTrees_GetClosest( float x, float z, float radius, GGTreePoint** pOutPoints )
{
	if (!ggtrees_initialised) return 0;
	UnorderedArray<GGTreePoint> points;
	uint32_t numPoints = 0;

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		float sqrDist = 0;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( x > aabb._max.x ) sqrDist = (x - aabb._max.x) * (x - aabb._max.x);
		else if ( x < aabb._min.x ) sqrDist = (x - aabb._min.x) * (x - aabb._min.x);

		if ( z > aabb._max.z ) sqrDist += (z - aabb._max.z) * (z - aabb._max.z);
		else if ( z < aabb._min.z ) sqrDist += (z - aabb._min.z) * (z - aabb._min.z);

		float sqrRadius = radius * radius;
		if ( sqrDist > sqrRadius ) continue;

		// check individual trees
		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			// a tree under a flatten pad or a Clear Grass and Trees area is not drawn, so it gets no collision cylinder
			// and carves no hole in the navmesh either
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsInvalid() ) continue;

			sqrDist = 0;
			sqrDist += (pInstance->x - x) * (pInstance->x - x);
			sqrDist += (pInstance->z - z) * (pInstance->z - z);

			if ( sqrDist > sqrRadius ) continue;

			if ( pOutPoints ) 
			{
				GGTreePoint tree;
				pInstance->GetTrunkCentre( &tree.x, &tree.z );
				tree.y = pInstance->y;
				tree.scale = pInstance->GetTreeThickness() * pInstance->GetScaleFloat();
				tree.type = pInstance->GetType();
				tree.instanceScale = pInstance->GetScaleFloat();
				int heightType = tree.type < (int)numTreeTypes ? tree.type : numTreeTypes - 1;
				tree.top = pInstance->y + g_GGTrees[ heightType ].height * tree.instanceScale;
				tree.crown = pInstance->y + GGTrees_CrownBase( tree.type ) * tree.instanceScale;
				points.AddItem( tree );
			}

			numPoints++;
		}
	}

	if ( numPoints == 0 ) return 0;

	if ( pOutPoints )
	{
		*pOutPoints = new GGTreePoint[ numPoints ];
		for( uint32_t i = 0; i < numPoints; i++ )
		{
			(*pOutPoints)[ i ] = points[ i ];
		}
	}

	return numPoints;
}

// returns 1 if hit, 0 if not. If hit then treeID will be populated
int GGTrees_RayCast( RAY ray, float maxDist, float* outDist, uint32_t* treeID )
{
	if (!ggtrees_initialised) return 0;
	if ( isnan(ray.direction.x) || isnan(ray.direction.y) || isnan(ray.direction.z) ) return 0;

	float currDist = 1e20f;
	int hit = 0;
	InstanceTree* pTreeHit = 0;

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->RayCast( ray, maxDist, &currDist, &pTreeHit ) ) hit = 1;
	}

	if ( hit && pTreeHit )
	{
		if ( treeID )
		{
			uint64_t index = (uint64_t) (pTreeHit - pAllTrees);
			*treeID = (uint32_t) index;
		}

		if ( outDist ) *outDist = currDist;
	}

	return hit;
}

// narrows t0..t1 to where p + d * t lies between lo and hi; false if it never does
static bool GGTrees_ClipSegment( float p, float d, float lo, float hi, float* t0, float* t1 )
{
	if ( fabsf( d ) < 1e-9f ) return p >= lo && p <= hi;
	float ta = (lo - p) / d;
	float tb = (hi - p) / d;
	if ( ta > tb ) { float tmp = ta; ta = tb; tb = tmp; }
	if ( ta > *t0 ) *t0 = ta;
	if ( tb < *t1 ) *t1 = tb;
	return *t0 <= *t1;
}

// where the segment from x1, y1, z1 to x2, y2, z2 first meets a tree trunk, anywhere on the map. A trunk is a vertical
// cylinder as thick as the physics' tree cylinder (the species' thickness times the tree's scale, across) from the tree's
// base to its top; canopies don't count, nor trees that are not drawn (hidden, flattened, killed or invalid). It reads the
// packed trunk list of each chunk the segment crosses (TreeChunk::trunks), nearest chunk first, stopping once a hit lies
// before the next one. pOut gets the point and the trunk's normal there (x, y, z, nx, ny, nz); returns 1 on a hit
int GGTrees_RayCastTrunks( float x1, float y1, float z1, float x2, float y2, float z2, float* pOut )
{
	if ( !ggtrees_initialised || !ggtrees_global_params.draw_enabled ) return 0;
	float dx = x2 - x1;
	float dy = y2 - y1;
	float dz = z2 - z1;
	float minX = x1 < x2 ? x1 : x2;
	float maxX = x1 < x2 ? x2 : x1;
	float minZ = z1 < z2 ? z1 : z2;
	float maxZ = z1 < z2 ? z2 : z1;

	// the chunks the segment crosses on x and z, in the order it enters them
	float chunkEntry[ numTreeChunks ];
	uint32_t chunkIndex[ numTreeChunks ];
	uint32_t numChunks = 0;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->trunks.empty() ) continue;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		float t0 = 0, t1 = 1;
		if ( !GGTrees_ClipSegment( x1, dx, aabb._min.x, aabb._max.x, &t0, &t1 ) ) continue;
		if ( !GGTrees_ClipSegment( z1, dz, aabb._min.z, aabb._max.z, &t0, &t1 ) ) continue;
		uint32_t k = numChunks++;
		while ( k > 0 && chunkEntry[ k - 1 ] > t0 )
		{
			chunkEntry[ k ] = chunkEntry[ k - 1 ];
			chunkIndex[ k ] = chunkIndex[ k - 1 ];
			k--;
		}
		chunkEntry[ k ] = t0;
		chunkIndex[ k ] = i;
	}

	float a = dx*dx + dz*dz;
	float bestT = 2.0f;
	int bestEntry = 0; // how the segment enters the nearest trunk: 0 it starts inside, 1 through the top or base, 2 the side
	const TreeTrunk* pBest = 0;
	for( uint32_t c = 0; c < numChunks; c++ )
	{
		if ( chunkEntry[ c ] > bestT ) break;
		const std::vector<TreeTrunk>& trunks = pTreeChunks[ chunkIndex[ c ] ].trunks;
		for( size_t j = 0; j < trunks.size(); j++ )
		{
			const TreeTrunk& trunk = trunks[ j ];

			// past at once a trunk outside the segment's bounds
			if ( trunk.x + trunk.radius < minX || trunk.x - trunk.radius > maxX ) continue;
			if ( trunk.z + trunk.radius < minZ || trunk.z - trunk.radius > maxZ ) continue;

			// the part of the segment within the trunk's radius of its axis
			float ox = x1 - trunk.x;
			float oz = z1 - trunk.z;
			float b = 2 * (ox*dx + oz*dz);
			float cc = ox*ox + oz*oz - trunk.radius*trunk.radius;
			float t0 = 0, t1 = 1;
			int entry = 0;
			if ( a < 1e-9f )
			{
				if ( cc > 0 ) continue;
			}
			else
			{
				float disc = b*b - 4*a*cc;
				if ( disc < 0 ) continue;
				float root = sqrtf( disc );
				float ta = (-b - root) / (2*a);
				float tb = (-b + root) / (2*a);
				if ( ta > t0 ) { t0 = ta; entry = 2; }
				if ( tb < t1 ) t1 = tb;
				if ( t0 > t1 || t0 >= bestT ) continue;
			}

			// and between its base and its top
			if ( fabsf( dy ) < 1e-9f )
			{
				if ( y1 < trunk.base || y1 > trunk.top ) continue;
			}
			else
			{
				float ta = (trunk.base - y1) / dy;
				float tb = (trunk.top - y1) / dy;
				if ( ta > tb ) { float tmp = ta; ta = tb; tb = tmp; }
				if ( ta > t0 ) { t0 = ta; entry = 1; }
				if ( tb < t1 ) t1 = tb;
				if ( t0 > t1 ) continue;
			}

			if ( t0 < bestT )
			{
				bestT = t0;
				bestEntry = entry;
				pBest = &trunk;
			}
		}
	}
	if ( !pBest ) return 0;

	float hitX = x1 + dx * bestT;
	float hitY = y1 + dy * bestT;
	float hitZ = z1 + dz * bestT;
	float nx = 0, ny = 0, nz = 0;
	if ( bestEntry == 2 )
	{
		nx = hitX - pBest->x;
		nz = hitZ - pBest->z;
	}
	else if ( bestEntry == 1 )
	{
		ny = dy > 0 ? -1.0f : 1.0f;
	}
	else
	{
		nx = -dx; ny = -dy; nz = -dz;
	}
	float length = sqrtf( nx*nx + ny*ny + nz*nz );
	if ( length > 0.000001f ) { nx /= length; ny /= length; nz /= length; }
	pOut[ 0 ] = hitX; pOut[ 1 ] = hitY; pOut[ 2 ] = hitZ;
	pOut[ 3 ] = nx; pOut[ 4 ] = ny; pOut[ 5 ] = nz;
	return 1;
}

// a point moving from p by v (t 0 to 1) against a rectangle of half sizes hx, hz grown by r with rounded corners, which is
// where a circle of radius r meets the rectangle: the first t it is inside, 0 if it starts inside
static bool GGTrees_PointMeetsRoundedRect( float px, float pz, float vx, float vz, float hx, float hz, float r, float* pT )
{
	float qx = fabsf( px ), qz = fabsf( pz );
	float cx = qx - hx, cz = qz - hz;
	if ( (qx <= hx + r && qz <= hz) || (qx <= hx && qz <= hz + r) || (cx > 0 && cz > 0 && cx*cx + cz*cz <= r*r) )
	{
		*pT = 0;
		return true;
	}
	float best = 2.0f;
	float t0 = 0, t1 = 1;
	if ( GGTrees_ClipSegment( px, vx, -(hx + r), hx + r, &t0, &t1 ) && GGTrees_ClipSegment( pz, vz, -hz, hz, &t0, &t1 ) && t0 < best ) best = t0;
	t0 = 0; t1 = 1;
	if ( GGTrees_ClipSegment( px, vx, -hx, hx, &t0, &t1 ) && GGTrees_ClipSegment( pz, vz, -(hz + r), hz + r, &t0, &t1 ) && t0 < best ) best = t0;
	float a = vx*vx + vz*vz;
	if ( a > 1e-9f )
	{
		for( int corner = 0; corner < 4; corner++ )
		{
			float ox = px - ((corner & 1) ? hx : -hx);
			float oz = pz - ((corner & 2) ? hz : -hz);
			float b = 2 * (ox*vx + oz*vz);
			float c = ox*ox + oz*oz - r*r;
			float disc = b*b - 4*a*c;
			if ( disc < 0 ) continue;
			float t = (-b - sqrtf( disc )) / (2*a);
			if ( t >= 0 && t <= 1 && t < best ) best = t;
		}
	}
	if ( best > 1.0f ) return false;
	*pT = best;
	return true;
}

// where a box (centre, half sizes, turned by yaw degrees about Y as an object is) moving by pMotion first meets a tree trunk,
// anywhere on the map: the same trunks as GGTrees_RayCastTrunks, each a cylinder from base to top. pOut gets the fraction
// of the motion, the point and the trunk's normal there (fraction, x, y, z, nx, ny, nz); returns 1 on a hit, with fraction 0
// for a box already touching a trunk
int GGTrees_SweepBoxTrunks( const float* pCentre, const float* pHalf, float yawDegrees, const float* pMotion, float* pOut, int* pType, bool bBareTrunk )
{
	if ( !ggtrees_initialised || !ggtrees_global_params.draw_enabled ) return 0;
	float hx = fabsf( pHalf[0] ), hy = fabsf( pHalf[1] ), hz = fabsf( pHalf[2] );
	float yaw = yawDegrees * 3.14159265f / 180.0f;
	float c = cosf( yaw ), s = sinf( yaw );

	// the ground the box covers on its way
	float reachX = fabsf( c ) * hx + fabsf( s ) * hz;
	float reachZ = fabsf( s ) * hx + fabsf( c ) * hz;
	float endX = pCentre[0] + pMotion[0], endZ = pCentre[2] + pMotion[2];
	float minX = (pCentre[0] < endX ? pCentre[0] : endX) - reachX, maxX = (pCentre[0] < endX ? endX : pCentre[0]) + reachX;
	float minZ = (pCentre[2] < endZ ? pCentre[2] : endZ) - reachZ, maxZ = (pCentre[2] < endZ ? endZ : pCentre[2]) + reachZ;

	// in the box's own frame a trunk moves against the box's motion
	float vx = -(pMotion[0] * c - pMotion[2] * s);
	float vz = -(pMotion[0] * s + pMotion[2] * c);
	float bestT = 2.0f;
	const TreeTrunk* pBest = 0;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->trunks.empty() ) continue;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( minX > aabb._max.x || maxX < aabb._min.x || minZ > aabb._max.z || maxZ < aabb._min.z ) continue;
		for( size_t j = 0; j < pChunk->trunks.size(); j++ )
		{
			const TreeTrunk& trunk = pChunk->trunks[ j ];
			if ( trunk.x + trunk.radius < minX || trunk.x - trunk.radius > maxX ) continue;
			if ( trunk.z + trunk.radius < minZ || trunk.z - trunk.radius > maxZ ) continue;
			float dx = trunk.x - pCentre[0];
			float dz = trunk.z - pCentre[2];
			float t;
			if ( !GGTrees_PointMeetsRoundedRect( dx * c - dz * s, dx * s + dz * c, vx, vz, hx, hz, trunk.radius, &t ) || t >= bestT ) continue;

			// and the box's height then reaches the trunk
			float boxY = pCentre[1] + pMotion[1] * t;
			if ( boxY - hy > (bBareTrunk ? trunk.crown : trunk.top) || boxY + hy < trunk.base ) continue;
			bestT = t;
			pBest = &trunk;
		}
	}
	if ( !pBest ) return 0;

	// the contact: on the trunk's surface, facing the nearest point of the box's footprint (against the motion when the
	// trunk's axis is inside it)
	float t = bestT;
	float dx = pBest->x - (pCentre[0] + pMotion[0] * t);
	float dz = pBest->z - (pCentre[2] + pMotion[2] * t);
	float px = dx * c - dz * s;
	float pz = dx * s + dz * c;
	float qx = px < -hx ? -hx : (px > hx ? hx : px);
	float qz = pz < -hz ? -hz : (pz > hz ? hz : pz);
	float nlx = qx - px, nlz = qz - pz;
	float length = sqrtf( nlx*nlx + nlz*nlz );
	if ( length < 0.0001f ) { nlx = vx; nlz = vz; length = sqrtf( nlx*nlx + nlz*nlz ); }
	if ( length < 0.0001f ) { nlx = 1; nlz = 0; length = 1; }
	nlx /= length; nlz /= length;
	float nx = nlx * c + nlz * s;
	float nz = -nlx * s + nlz * c;
	float hitY = pCentre[1] + pMotion[1] * t;
	float hitTop = bBareTrunk ? pBest->crown : pBest->top;
	if ( hitY < pBest->base ) hitY = pBest->base;
	if ( hitY > hitTop ) hitY = hitTop;
	pOut[ 0 ] = t;
	pOut[ 1 ] = pBest->x + nx * pBest->radius;
	pOut[ 2 ] = hitY;
	pOut[ 3 ] = pBest->z + nz * pBest->radius;
	pOut[ 4 ] = nx; pOut[ 5 ] = 0; pOut[ 6 ] = nz;
	if ( pType ) *pType = pBest->type;
	return 1;
}

// how many tree trunks a box (as GGTrees_SweepBoxTrunks's) touches now
int GGTrees_OverlapBoxTrunks( const float* pCentre, const float* pHalf, float yawDegrees, bool bBareTrunk )
{
	if ( !ggtrees_initialised || !ggtrees_global_params.draw_enabled ) return 0;
	float hx = fabsf( pHalf[0] ), hy = fabsf( pHalf[1] ), hz = fabsf( pHalf[2] );
	float yaw = yawDegrees * 3.14159265f / 180.0f;
	float c = cosf( yaw ), s = sinf( yaw );
	float reachX = fabsf( c ) * hx + fabsf( s ) * hz;
	float reachZ = fabsf( s ) * hx + fabsf( c ) * hz;
	int count = 0;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->trunks.empty() ) continue;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( pCentre[0] - reachX > aabb._max.x || pCentre[0] + reachX < aabb._min.x ) continue;
		if ( pCentre[2] - reachZ > aabb._max.z || pCentre[2] + reachZ < aabb._min.z ) continue;
		for( size_t j = 0; j < pChunk->trunks.size(); j++ )
		{
			const TreeTrunk& trunk = pChunk->trunks[ j ];
			if ( pCentre[1] - hy > (bBareTrunk ? trunk.crown : trunk.top) || pCentre[1] + hy < trunk.base ) continue;
			float dx = trunk.x - pCentre[0];
			float dz = trunk.z - pCentre[2];
			float t;
			if ( GGTrees_PointMeetsRoundedRect( dx * c - dz * s, dx * s + dz * c, 0, 0, hx, hz, trunk.radius, &t ) ) count++;
		}
	}
	return count;
}

uint32_t GGTrees_GetDataSize()
{
	return numTotalTrees * 3 + 1;
}

#define GGTREES_CURRENT_VERSION 3

// fills data with every tree hidden and outside any tree area, so a smaller tree file read over it leaves the
// remaining slots unused
void GGTrees_GetEmptyData( float* data )
{
	uint32_t* dataInt = (uint32_t*) data;
	dataInt[ 0 ] = GGTREES_CURRENT_VERSION;

	data++;
	dataInt++;

	for( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		data[ i * 3 + 0 ] = GGTREES_MAX_AREA;
		data[ i * 3 + 1 ] = GGTREES_MAX_AREA;
		dataInt[ i * 3 + 2 ] = 0;
	}
}

int GGTrees_GetData( float* data )
{
	if (!ggtrees_initialised) return 0;
	uint32_t* dataInt = (uint32_t*) data;
	dataInt[ 0 ] = GGTREES_CURRENT_VERSION;
	
	data++;
	dataInt++;

	for( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		data[ i * 3 + 0 ] = pAllTrees[ i ].x;
		data[ i * 3 + 1 ] = pAllTrees[ i ].z;
		uint32_t d = pAllTrees[ i ].data;
		// GGMAX 3.123: never SAVE a visible tree with an invalid type (>= numTreeTypes) - the file
		// tester report #8 loaded held 15015. Nothing in memory should carry one (SetData and the
		// tree undo strip them), so this only guarantees no DX12 save can ever write that file.
		if ( ((d >> 11) & 0x3F) >= numTreeTypes ) d &= ~0x1u;
		dataInt[ i * 3 + 2 ] = d;
	}

	return 1;
}

// GGMAX 3.123: how many trees the LAST GGTrees_SetData hid for an invalid type (harness
// GET_TREE_LOADSTATS). A per-level value: cleared by GGTrees_HideAll (every level load calls it, with or
// without a .tre) and on SetData entry (every return path).
static uint32_t g_iLastLoadInvalidTypeTrees = 0;
uint32_t GGTrees_GetLastLoadInvalidTypeTrees() { return g_iLastLoadInvalidTypeTrees; }

// Using this instead of GGTrees_GetData to allow reuse of g_pTerrainSnapshot.
int GGTrees_GetSnapshot(uint8_t* data)
{
	if (!ggtrees_initialised) return 0;
	uint32_t size1 = sizeof(InstanceTree) * numTotalTrees;
	memcpy(data, &pAllTrees, size1);

	//uint32_t size2 = sizeof(TreeChunk) * numTreeChunks;
	//memcpy(data + size1, pTreeChunks, size2);

	return 1;
}

int GGTrees_SetData( float* data )
{
	if (!ggtrees_initialised) return 0;
	g_iLastLoadInvalidTypeTrees = 0;
	uint32_t* dataInt = (uint32_t*) data;
	uint32_t version = dataInt[ 0 ];
	if ( version < 2 ) // can't import from before version 2
	{
		MessageBoxA( NULL, "Tree data format has changed since this level was last saved, trees will be laid out in the default pattern", "Warning", 0 );
		GGTrees_RepopulateInstances();
		return 0; // version check
	}

	if ( version > GGTREES_CURRENT_VERSION ) // can't import future formats
	{
		MessageBoxA( NULL, "Tree data format is newer than expected, you may be missing a software update. Trees will be laid out in the default pattern instead", "Warning", 0 );
		GGTrees_RepopulateInstances();
		return 0; // version check
	}

	data++;
	dataInt++;

	GGTrees_InvalidateBillboardAtlases();   // GGMAX 3.53: THE level-load hook - see the note above
	// GGMAX 3.54: and free the OUTGOING level's tree-type meshes/materials + pool + shadow proxies.
	// Defined in GGTrees_part2.cpp, which is #included after this file into GGTrees.cpp, so it needs
	// the forward declaration above. Without this the previous level's type assets stayed resident.
	GGTrees_ReleaseLevelAssets();

	// the terrain settings are loaded first, so the tree area matches this level
	GGTrees_SetArea( GGTrees_GetRequiredArea() );
	memset( pTreeHeightValid, 0, sizeof(pTreeHeightValid) );

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		pTreeChunks[ i ].pInstances.Clear();
	}
	pInvisibleTrees.Clear();
	
	uint32_t iInvalidTypeTrees = 0;
	for( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		InstanceTree* pInstance = &pAllTrees[ i ];
		pInstance->x = data[ i * 3 + 0 ];
		pInstance->y = 0;
		pInstance->z = data[ i * 3 + 1 ];

		if ( version == GGTREES_CURRENT_VERSION )
		{
			uint32_t data = dataInt[ i * 3 + 2 ];
			uint32_t flags = (data & 0xFF);
			uint32_t type = (data >> 11) & 0x3F;
			uint32_t scale = (data >> 16) & 0xFE;

			pInstance->SetFlags( flags );
			pInstance->SetScale( scale );

			if ( type >= numTreeTypes )
			{
				// GGMAX 3.123: an invalid type - only types 0-37 exist (DX11 too, there are no
				// custom tree slots). Hidden exactly as DX11 does; the file's type bits are NOT
				// copied in, so pAllTrees never holds a type >= numTreeTypes.
				if ( pInstance->IsVisible() ) iInvalidTypeTrees++;
				pInstance->SetVisible( 0 );
			}
			else
			{
#ifdef ONLYLOADWHENUSED
				bTreeTextureForceUploaded[type] = true;
#endif
				pInstance->SetType( type );
			}
		}
		else if ( version == 2 )
		{
			uint32_t oldData = dataInt[ i * 3 + 2 ];
			uint32_t oldFlags = (oldData & 0xFF);
			uint32_t oldType = (oldData >> 11) & 0x3F;
			uint32_t oldScale = (oldData >> 16) & 0xFE;

			uint32_t newType = 20; // pine as default

			switch( oldType )
			{
				case 0: newType = 20; break; // pine
				case 1: newType =  0; break; // birch
				case 2: newType =  1; break; // cactus var 1
				case 3: newType =  7; break; // italian pine
				case 4: newType = 18; break; // kentia palm
				case 5: newType = 19; break; // palm
				case 6: newType = 31; break; // vine tree large
				case 7: newType = 32; break; // vine tree small
				case 8: newType = 34; break; // white pine
			}

			pInstance->SetFlags( oldFlags );
			pInstance->SetScale( oldScale );
			pInstance->SetType( newType );
#ifdef ONLYLOADWHENUSED
			bTreeTextureForceUploaded[newType] = true;
#endif
		}
		else
		{
			// this shouldn't happen
			MessageBoxA( NULL, "Unrecognised tree version, trees will be laid out in the default pattern", "Warning", 0 );
			GGTrees_RepopulateInstances();
#ifdef ONLYLOADWHENUSED
			last_paint_tree_bitfield = -1;
#endif
			return 0; // version check
		}

		pInstance->SetID( i );

		TreeChunk* pChunk = GGTrees_GetChunk( pInstance->x, pInstance->z );
		if ( !pChunk ) 
		{
			pInstance->SetVisible( 0 );
			pInvisibleTrees.AddItem( i );
		}
		else
		{
			pChunk->pInstances.AddItem( pInstance );

			if ( !pInstance->IsVisible() )
			{
				pInvisibleTrees.AddItem( i );
			}
		}
	}
#ifdef ONLYLOADWHENUSED
	last_paint_tree_bitfield = -1;
#endif
	// GGMAX 3.123 (tester report #8): log only - NEVER a modal. This was a MessageBox ("TREE TYPES
	// DROPPED ... custom tree slots not ported to DX12") on a false premise: DX11 has the same 38
	// compiled-in types and hides these trees SILENTLY (GGTrees.cpp:1664). It fired on every load of
	// a level whose file held such data - e.g. a read-only demo shadowed by a saved copy in the user's
	// writable mapbank (one Save of that level would have written those trees hidden and ended it, but
	// nothing told the user so, and the text blamed a feature that does not exist) - and in Test Game,
	// Play Game and standalone games too, where a modal behind the game window reads as a hang.
	g_iLastLoadInvalidTypeTrees = iInvalidTypeTrees;
	if ( iInvalidTypeTrees > 0 )
	{
		char pInvalidLog[256];
		sprintf_s( pInvalidLog, "TREE DATA: %u visible tree instances have an invalid type (>= %u) and were hidden, as DX11 does",
			iInvalidTypeTrees, (uint32_t)numTreeTypes );
		::timestampactivity( 0, pInvalidLog );
	}
	if ( !GGTrees_UpdateInstances( 1 ) )
	{
		// Terrain heights may not be ready this early in a level load — arm the
		// same 5-frame retry backoff GGTrees_Update uses on failure, instead of
		// leaving instances at Y=0 until terrain LOD regen happens to set
		// iUpdateTrees for us.
		ggterrain_extra_params.iUpdateTrees = 5;
	}
	GGTrees_RasterizeTreeMap();
	GGTrees_UploadTreeMap();
	return 1;
}

void GGTrees_SetPerformanceMode( uint32_t mode )
{
	// NOTE: lod_dist_shadow / tree_shadow_range are no longer preset-dialed here — they
	// are level-authored (t.visuals, saved with the FPM, defaults 4000/5) and re-asserted
	// by Wicked_Update_Shadows right after every SetGlobalGraphicsSettings call, so any
	// values stamped here would be dead. Only the visual LOD distance scales with preset.
	switch( mode )
	{
		case GGTERRAIN_PERFORMANCE_LOW:
		{
			ggtrees_global_params.lod_dist = 1000.0f;
		} break;

		case GGTERRAIN_PERFORMANCE_MED:
		{
			ggtrees_global_params.lod_dist = 2000.0f;
		} break;

		case GGTERRAIN_PERFORMANCE_HIGH:
		{
			ggtrees_global_params.lod_dist = 3000.0f;
		} break;

		case GGTERRAIN_PERFORMANCE_ULTRA:
		{
			ggtrees_global_params.lod_dist = 4000.0f;
		} break;
	}
}

void GGTrees_SetTreePosition( uint32_t treeID, float x, float z ) 
{
	if (!ggtrees_initialised) return;
	if ( treeID >= numTotalTrees ) return;

	InstanceTree* pInstance = &pAllTrees[ treeID ];

	TreeChunk* pOldChunk = GGTrees_GetChunk( pInstance->x, pInstance->z );
	TreeChunk* pNewChunk = GGTrees_GetChunk( x, z );
	if ( pOldChunk != pNewChunk && pOldChunk )
	{
		pOldChunk->RemoveTree( pInstance );
	}

	// positions bypass the flag setters — announce the change so the Wicked
	// pool (and its spatial grid) rescan instead of waiting for the heartbeat;
	// both the old and new chunk's shadow proxy meshes are now stale
	GGTrees_MarkProxyChunkDirtyAt( pInstance->x, pInstance->z );
	pInstance->x = x;
	pInstance->z = z;
	pTreeHeightValid[ treeID ] = 0;
	g_treeInstanceStamp++;
	GGTrees_MarkProxyChunkDirtyAt( x, z );

	float height = 0;
	float ny = 0;
	GGTerrain_GetHeight( pInstance->x, pInstance->z, &height, 1 );
	GGTerrain_GetNormal( pInstance->x, pInstance->z, 0, &ny, 0 );
	if ( (height > g.gdefaultwaterheight + ggtrees_global_params.water_dist && ny > 0.7) || pInstance->IsUserMoved() )
	{
		float adjustment = (1 - ny) * 75 * pInstance->GetScaleFloat();
		pInstance->y = height - 10 - adjustment;
		if ( pOldChunk != pNewChunk && pNewChunk ) 
		{
			pNewChunk->pInstances.AddItem( pInstance );
		}
	}

	if ( pOldChunk ) pOldChunk->Update();
	if ( pOldChunk != pNewChunk && pNewChunk ) pNewChunk->Update();
}

int GGTrees_UsingBrush()
{
	if( ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY || ggtrees_global_params.paint_mode == GGTREES_PAINT_ADD || ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY_REMOVE ) return true;
	else return false;
}

uint32_t GGTrees_GetNumTypes()
{
	return numTreeTypes;
}

void GGTrees_Update_Painting( RAY ray )
{
	if (!ggtrees_initialised) return;
	float pickX = 0, pickY = 0, pickZ = 0;
	int pickHit = GGTerrain_RayCast( ray, &pickX, &pickY, &pickZ, 0, 0, 0, 0 );

	float maxDist = 0;
	if ( pickHit )
	{
		float diffX = pickX - ray.origin.x;
		float diffY = pickY - ray.origin.y;
		float diffZ = pickZ - ray.origin.z;
		maxDist = diffX*diffX + diffY*diffY + diffZ*diffZ;
		maxDist = sqrt( maxDist );
	}

	float treeHitDist = 0;
	uint32_t treeHit = 0xFFFFFFFF;
	if ( ggtrees_global_params.paint_mode != GGTREES_PAINT_SPRAY 
	  && ggtrees_global_params.paint_mode != GGTREES_PAINT_SPRAY_REMOVE
	  && ggtrees_global_params.paint_mode != GGTREES_PAINT_ADD
	  && ggtrees_internal_params.tree_selected == 0xFFFFFFFF )
	{
		GGTrees_RayCast( ray, maxDist, &treeHitDist, &treeHit );
	}

	if ( ggtrees_internal_params.mouseLeftPressed )
	{
		switch( ggtrees_global_params.paint_mode )
		{
			case GGTREES_PAINT_REMOVE: 
			{
				if ( treeHit != 0xFFFFFFFF ) 
				{
					if ( pAllTrees[ treeHit ].IsVisible() )
					{
						// Create an undo action before removing this tree.
						TreeRemoveData removeData;
						removeData.treeIndex = treeHit;
						removeData.scale = pAllTrees[treeHit].GetScale();
						removeData.x = pAllTrees[treeHit].x;
						removeData.z = pAllTrees[treeHit].z;
						removeData.userMoved = pAllTrees[treeHit].IsUserMoved();
						removeData.type = pAllTrees[treeHit].GetType();
						GGTrees_CreateUndoRedoAction(eUndoSys_Terrain_RemoveTree, eUndoSys_UndoList, true, &removeData);

						pAllTrees[ treeHit ].SetVisible( 0 );
						pAllTrees[ treeHit ].SetUserMoved( 0 );
						pInvisibleTrees.AddItem( treeHit );
						TreeChunk* pChunk = GGTrees_GetChunk( pAllTrees[ treeHit ].x, pAllTrees[ treeHit ].z );
						if ( pChunk ) pChunk->Update();
					}
				}
			} break;

			case GGTREES_PAINT_MOVE: 
			{
				ggtrees_internal_params.tree_selected = treeHit;
			} break;

			case GGTREES_PAINT_SCALE: 
			{
				ggtrees_internal_params.tree_selected = treeHit;
				if ( treeHit != 0xFFFFFFFF ) 
				{
					ggtrees_internal_params.scaleMouseYStart = ggtrees_internal_params.mouseY;
					ggtrees_internal_params.scaleStart = pAllTrees[ ggtrees_internal_params.tree_selected ].GetScale() * 2;
				}
			} break;

			case GGTREES_PAINT_ADD:
			{
				if ( pickHit )
				{
					uint32_t index = 0;
					if ( pInvisibleTrees.NumItems() > 0 ) index = pInvisibleTrees.PopItem();
					else 
					{
						// steal a tree from the edge of the map
						for( int i = 0; i < treeSplit; i++ )
						{
							TreeChunk* pChunk = &pTreeChunks[ i ];
							if ( pChunk->pInstances.NumItems() > 0 )
							{
								index = pChunk->pInstances[ 0 ]->GetID();
								break;
							}
						}
					}

					uint32_t numTypes = 0;
					uint64_t values = ggtrees_global_params.paint_tree_bitfield;
					for( uint32_t i = 0; i < numTreeTypes; i++ )
					{
						if ( values & 1 ) numTypes++;
						values >>= 1;
					}
					if ( numTypes == 0 ) numTypes = 1; 
					
					pAllTrees[ index ].SetVisible( 1 );
					pAllTrees[ index ].SetInvalid( 0 );
					pAllTrees[ index ].SetUserMoved( 1 );

					uint32_t type = pAllTrees[ index ].GetID();
					type = 214313*type + 2539011;
					type = 214313*type + 2539011;
					type = 214313*type + 2539011;
					type ^= (type >> 16);
					type ^= (type >> 8);
					type %= numTypes;

					uint32_t count = 0;
					values = ggtrees_global_params.paint_tree_bitfield;
					for( uint32_t k = 0; k < numTreeTypes; k++ )
					{
						if ( values & 1 ) 
						{
							if ( count == type )
							{
								pAllTrees[ index ].SetType( k );
								RandomSeed(pAllTrees[index].GetID()*113);
								pAllTrees[index].SetScale(Random(ggtrees_global_params.paint_scale_random_low, ggtrees_global_params.paint_scale_random_high));
								break;
							}
							count++;
						}
						values >>= 1;
					}

					GGTrees_SetTreePosition( index, pickX, pickZ );

					// Create an undo action for this add.
					TreeAddData undoData;
					undoData.treeIndex = index;
					undoData.scale = pAllTrees[index].GetScale();
					undoData.x = pAllTrees[index].x;
					undoData.z = pAllTrees[index].z;
					undoData.userMoved = true;
					undoData.type = pAllTrees[index].GetType();
					GGTrees_CreateUndoRedoAction(eUndoSys_Terrain_AddTree, eUndoSys_UndoList, true, &undoData);
				}
			} break;
		}
	}

	if ( ggtrees_internal_params.mouseLeftReleased )
	{
		ggtrees_internal_params.tree_selected = 0xFFFFFFFF;
		g_iActiveTreeEvent = 0;
		if (ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY || ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY_REMOVE)
		{
			// Create an undo event for the trees that were added/removed.
			GGTrees_CreateUndoRedoAction(eUndoSys_Terrain_PaintTree, eUndoSys_UndoList, true);
		}
	}

	if ( ggtrees_internal_params.mouseLeftState )
	{		
		if ( (ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY || ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY_REMOVE) && pickHit )
		{
			#ifdef GGTREES_UNDOREDO
			if (g_iActiveTreeEvent == 0)
			{
				// Take a snapshot of how the grass is before any edits take place.
				GGTrees_GetSnapshot(g_pTerrainSnapshot);
				g_iActiveTreeEvent = 1;
			}
			#endif
			float radius = ggterrain_global_render_params2.brushSize;
		
			float minX = pickX - radius;
			float minZ = pickZ - radius;
			float maxX = pickX + radius;
			float maxZ = pickZ + radius;

			uint32_t numTypes = 0;
			uint64_t values = ggtrees_global_params.paint_tree_bitfield;
			for( uint32_t i = 0; i < numTreeTypes; i++ )
			{
				if ( values & 1 ) numTypes++;
				values >>= 1;
			}
			if ( numTypes == 0 ) numTypes = 1;

			for( uint32_t i = 0; i < numTreeChunks; i++ )
			{
				TreeChunk* pChunk = &pTreeChunks[ i ];
				if ( pChunk->pInstances.NumItems() == 0 ) continue;

				AABB aabb;
				pChunk->GetBounds( &aabb );
				if ( minX > aabb._max.x || minZ > aabb._max.z || maxX < aabb._min.x || maxZ < aabb._min.z ) continue;

				for( int j = 0; j < (int)pChunk->pInstances.NumItems(); j++ )
				{
					InstanceTree* pTree = pChunk->pInstances[ j ];
					float diffX = pickX - pTree->x;
					float diffZ = pickZ - pTree->z;
					float sqrDist = diffX*diffX + diffZ*diffZ;
					if ( sqrDist > radius*radius ) continue;

					int test = j % 100;
					if ( test < ggtrees_global_params.paint_density && ggtrees_global_params.paint_mode == GGTREES_PAINT_SPRAY ) 
					{
						if ( !pTree->IsVisible() ) pInvisibleTrees.RemoveItem( pTree->GetID() );
						pTree->SetVisible( 1 );
						pTree->SetUserMoved( 0 );

						uint32_t type = pTree->GetID();
						type = 214313*type + 2539011;
						type = 214313*type + 2539011;
						type = 214313*type + 2539011;
						type ^= (type >> 16);
						type ^= (type >> 8);
						type %= numTypes;

						uint32_t count = 0;
						uint64_t values = ggtrees_global_params.paint_tree_bitfield;
						for( uint32_t k = 0; k < numTreeTypes; k++ )
						{
							if ( values & 1 ) 
							{
								if ( count == type )
								{
									pTree->SetType( k );
									RandomSeed(pTree->GetID()*113); //PE: After loading a map, the ID's are sequential and dont really generate random values.
									pTree->SetScale(Random(ggtrees_global_params.paint_scale_random_low, ggtrees_global_params.paint_scale_random_high));
									break;
								}
								count++;
							}
							values >>= 1;
						}
					}
					else 
					{
						if ( pTree->IsVisible() ) pInvisibleTrees.AddItem( pTree->GetID() );
						pTree->SetVisible( 0 );
						pTree->SetUserMoved( 0 );
					}
				}

				pChunk->Update();
			}
		}

		if ( ggtrees_global_params.paint_mode == GGTREES_PAINT_MOVE && ggtrees_internal_params.tree_selected != 0xFFFFFFFF )
		{
			InstanceTree* pInstance = &pAllTrees[ ggtrees_internal_params.tree_selected ];

			// Create the undo event before we change anything, so we have the state of the tree to restore to.
			if (g_iActiveTreeEvent == 0)
			{
				g_iActiveTreeEvent = 1;
				TreeMoveData moveData;
				moveData.x = pInstance->x;
				moveData.z = pInstance->z;
				moveData.treeIndex = ggtrees_internal_params.tree_selected;
				moveData.userMoved = pInstance->IsUserMoved();
				GGTrees_CreateUndoRedoAction(eUndoSys_Terrain_MoveTree, eUndoSys_UndoList, true, &moveData);
			}

			pInstance->SetUserMoved( 1 );
			GGTrees_SetTreePosition( ggtrees_internal_params.tree_selected, pickX, pickZ );
		}

		if ( ggtrees_global_params.paint_mode == GGTREES_PAINT_SCALE && ggtrees_internal_params.tree_selected != 0xFFFFFFFF )
		{
			InstanceTree* pInstance = &pAllTrees[ ggtrees_internal_params.tree_selected ];

			// Create the undo event before we change anything, so we have the state of the tree to restore to.
			if (g_iActiveTreeEvent == 0)
			{
				g_iActiveTreeEvent = 1;
				TreeScaleData scaleData;
				scaleData.scale = pInstance->GetScale();
				scaleData.treeIndex = ggtrees_internal_params.tree_selected;
				scaleData.userMoved = pInstance->IsUserMoved();
				GGTrees_CreateUndoRedoAction(eUndoSys_Terrain_ScaleTree, eUndoSys_UndoList, true, &scaleData);
			}


			pInstance->SetUserMoved( 1 );
			int mouseDiff = ggtrees_internal_params.scaleMouseYStart - ggtrees_internal_params.mouseY;
			int newScale = (ggtrees_internal_params.scaleStart + mouseDiff) / 2;
			if ( newScale < 1 ) newScale = 1;
			if ( newScale > 255 ) newScale = 255;
			pInstance->SetScale( newScale );
		}
	}

	if ( treeHighlighted < numTotalTrees ) pAllTrees[ treeHighlighted ].SetHighlighted( 0 );

	if ( ggtrees_internal_params.tree_selected == 0xFFFFFFFF ) treeHighlighted = treeHit;
	else treeHighlighted = ggtrees_internal_params.tree_selected;

	if ( treeHighlighted < numTotalTrees ) pAllTrees[ treeHighlighted ].SetHighlighted( 1 );

	ggtrees_internal_params.minTotalHeight = 1e20f;
	ggtrees_internal_params.maxTotalHeight = -1e20f;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->minHeight < ggtrees_internal_params.minTotalHeight ) ggtrees_internal_params.minTotalHeight = pChunk->minHeight;
		if ( pChunk->maxHeight > ggtrees_internal_params.maxTotalHeight ) ggtrees_internal_params.maxTotalHeight = pChunk->maxHeight;
	}
}


void GGTrees_Delete_Trees(float pickX,float pickZ, float radius)
{
	if (!ggtrees_initialised) return;
	float minX = pickX - radius;
	float minZ = pickZ - radius;
	float maxX = pickX + radius;
	float maxZ = pickZ + radius;

	for (uint32_t i = 0; i < numTreeChunks; i++)
	{
		TreeChunk* pChunk = &pTreeChunks[i];
		if (pChunk->pInstances.NumItems() == 0) continue;

		AABB aabb;
		pChunk->GetBounds(&aabb);
		if (minX > aabb._max.x || minZ > aabb._max.z || maxX < aabb._min.x || maxZ < aabb._min.z) continue;

		for (int j = 0; j < (int)pChunk->pInstances.NumItems(); j++)
		{
			InstanceTree* pTree = pChunk->pInstances[j];
			float diffX = pickX - pTree->x;
			float diffZ = pickZ - pTree->z;
			float sqrDist = diffX * diffX + diffZ * diffZ;
			if (sqrDist > radius*radius) continue;

			if (pTree->IsVisible()) pInvisibleTrees.AddItem(pTree->GetID());
			pTree->SetVisible(0);
			pTree->SetUserMoved(0);
		}

		pChunk->Update();
	}

}

void GGTrees_UpdateFrustumCulling( wiScene::CameraComponent* camera )
{
	if (!ggtrees_initialised) return;
	if (!ggtrees_global_params.draw_enabled) return; //OPT3
	// Wicked path: culling is done by Wicked on the pool objects; the instance
	// lists built here feed only the old DX11 draw path (suppressed). The sole
	// other consumer (GGTrees_GetNumHighDetail) has no callers.
	if ( ggterrain_use_wicked_terrain ) return;
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	float cameraX = camera->Eye.x;
	float cameraY = camera->Eye.y;
	float cameraZ = camera->Eye.z;

	// find close trees 
	for( uint32_t i = 0; i < numTreeTypes; i++ ) 
	{
		numTreeInstancesHigh[ i ] = 0;
	}

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		float sqrDist = 0;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( cameraX > aabb._max.x ) sqrDist = (cameraX - aabb._max.x) * (cameraX - aabb._max.x);
		else if ( cameraX < aabb._min.x ) sqrDist = (cameraX - aabb._min.x) * (cameraX - aabb._min.x);

		if ( cameraY > aabb._max.y ) sqrDist += (cameraY - aabb._max.y) * (cameraY - aabb._max.y);
		else if ( cameraY < aabb._min.y ) sqrDist += (cameraY - aabb._min.y) * (cameraY - aabb._min.y);

		if ( cameraZ > aabb._max.z ) sqrDist += (cameraZ - aabb._max.z) * (cameraZ - aabb._max.z);
		else if ( cameraZ < aabb._min.z ) sqrDist += (cameraZ - aabb._min.z) * (cameraZ - aabb._min.z);

		float distLOD = ggtrees_global_params.lod_dist + (float)GGTREES_LOD_TRANSITION*2 + treeMaxHeight;
		float sqrDistLOD = distLOD * distLOD;

		if ( sqrDist > sqrDistLOD ) continue;

		// check individual trees
		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsInvalid() ) continue;

			sqrDist = 0;
			sqrDist += (pInstance->x - cameraX) * (pInstance->x - cameraX);
			sqrDist += (pInstance->y - cameraY) * (pInstance->y - cameraY);
			sqrDist += (pInstance->z - cameraZ) * (pInstance->z - cameraZ);

			uint32_t treeType = pInstance->GetType();

			// error trap if data held exceeds latest tree choices
			if (treeType >= numTreeTypes)
			{
				treeType = numTreeTypes - 1;
			}

			float scale = pInstance->GetScaleFloat();
			float height = g_GGTrees[ treeType ].height * scale;
			float halfHeight = height / 2.0f;

			XMFLOAT3 pos;
			pos.x = pInstance->x;
			pos.y = pInstance->y + halfHeight;
			pos.z = pInstance->z;

			if ( sqrDist <= sqrDistLOD )
			{
				if ( camera->frustum.CheckSphere( pos, halfHeight ) ) 
				{
					// add tree to draw list
					uint32_t index = numTreeInstancesHigh[ treeType ];
					if ( index < 999 )
					{
						// only if fit within dynamic array
						treeInstancesHigh[treeType][index].x = pInstance->x;
						treeInstancesHigh[treeType][index].y = pInstance->y;
						treeInstancesHigh[treeType][index].z = pInstance->z;
						treeInstancesHigh[treeType][index].data = pInstance->data;
						numTreeInstancesHigh[treeType]++;
					}
				}
			}
		}
	}

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( numTreeInstancesHigh[ i ] > 0 )
		{
			GPUBufferDesc bufferDesc = {};
			SubresourceData data = {};
			data.data_ptr = treeInstancesHigh[ i ];
			bufferDesc.size = sizeof(InstanceTreeGPU) * numTreeInstancesHigh[ i ];
			bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
			//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
			bufferDesc.misc_flags = ResourceMiscFlag::NONE;
			wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferInstancesHigh[i] );
		}
	}
}

static void GGTrees_UpdateBillboardCB( float camX, float camY, float camZ, CommandList cmd ); // GGMAX 2.96

void GGTrees_Update( float camX, float camY, float camZ, CommandList cmd, bool bRenderTargetFocus )
{
	if (!ggtrees_initialised) return;
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	if ( !ggtrees_global_params.draw_enabled && ggtrees_global_params.hide_until_update == 0 ) return;

	// GGMAX 3.102: the palette-change texture upload that stood here wrote past the end of the
	// billboard atlas - see GGTrees_LoadTextures. A palette click no longer touches any texture.

	if (ggtrees_global_params.hide_until_update)
	{
		// Trees are not drawn when choosing a new biome, so need check if we can draw them again.
		if (ggterrain_extra_params.iUpdateTrees > 0)
		{
			ggterrain_extra_params.iUpdateTrees--;
			if (ggterrain_extra_params.iUpdateTrees == 0)
			{
				if (!GGTrees_UpdateInstances(0)) ggterrain_extra_params.iUpdateTrees = 5;
			}
		}
		return;
	}

	auto range = wiProfiler::BeginRangeCPU( "Max - Tree Update" );

	if ( ggterrain_extra_params.edit_mode == GGTERRAIN_EDIT_TREES && !bImGuiGotFocus )
	{
		wiInput::MouseState mouseState = wiInput::GetMouseState();
		ggtrees_internal_params.mouseLeftState = mouseState.left_button_press;
		ggtrees_internal_params.mouseLeftPressed = (mouseState.left_button_press && !ggtrees_internal_params.prevMouseLeft) ? 1 : 0;
		ggtrees_internal_params.mouseLeftReleased = (!mouseState.left_button_press && ggtrees_internal_params.prevMouseLeft) ? 1 : 0;
		ggtrees_internal_params.prevMouseLeft = mouseState.left_button_press ? 1 : 0;
		ggtrees_internal_params.mouseY = (int) mouseState.position.y;

		if (bRenderTargetFocus)
		{
			RAY pickRay = wiRenderer::GetPickRay((long)mouseState.position.x, (long)mouseState.position.y, master.masterrenderer);
			GGTrees_Update_Painting(pickRay);

			// need to do this for the editor to update highlighting, can be skipped in standalone game
			// stagger it to improve performance
			uint32_t start = ggtrees_internal_params.treeChunkUpdate;
			uint32_t end = start + 16;
			if (end >= numTreeChunks) end = numTreeChunks - 1;
			for (uint32_t i = start; i < end; i++)
			{
				TreeChunk* pChunk = &pTreeChunks[i];
				pChunk->Update();
			}

			ggtrees_internal_params.treeChunkUpdate = end;
			if (end == numTreeChunks - 1) ggtrees_internal_params.treeChunkUpdate = 0;
		}
	}

	if ( ggterrain_extra_params.iUpdateTrees > 0 )
	{
		ggterrain_extra_params.iUpdateTrees--;
		if ( ggterrain_extra_params.iUpdateTrees == 0 )
		{
			// something changed so adjust tree heights
			if ( !GGTrees_UpdateInstances( 0 ) ) ggterrain_extra_params.iUpdateTrees = 5;
		}
	}

	// Wicked path: trees render via the ObjectComponent pool (GGTrees_WickedUpdate).
	// Everything below builds shadow instance lists + per-type GPU buffers + tree
	// constant buffers for the OLD DX11 draw path, which is suppressed when the
	// wicked terrain is active — skip the dead work (~1ms + per-frame CreateBuffer churn).
	if ( ggterrain_use_wicked_terrain )
	{
		// GGMAX 2.96: the CONSTANT BUFFER is not dead work any more - the distant-tree billboard
		// pass reads it. Without this the per-type scaleX/scaleY stay zero, so every billboard
		// quad has zero area: measured 98,410 instances submitted across 87 draws for exactly
		// zero pixels and zero measurable cost. Fill and upload just the CB, then skip the rest
		// (the per-type shadow instance buffers and their CreateBuffer churn ARE still dead).
		if ( gg_far_tree_pass )
		{
			GGTrees_UpdateBillboardCB( camX, camY, camZ, cmd );
		}
		wiProfiler::EndRange( range );
		return;
	}

	// find close trees for shadows only, do normal trees in GGTrees_UpdateFrustumCulling()
	for( uint32_t i = 0; i < numTreeTypes; i++ ) 
	{
		numTreeInstancesHighShadow[ i ] = 0;
	}

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		float sqrDist = 0;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( camX > aabb._max.x ) sqrDist = (camX - aabb._max.x) * (camX - aabb._max.x);
		else if ( camX < aabb._min.x ) sqrDist = (camX - aabb._min.x) * (camX - aabb._min.x);

		if ( camY > aabb._max.y ) sqrDist += (camY - aabb._max.y) * (camY - aabb._max.y);
		else if ( camY < aabb._min.y ) sqrDist += (camY - aabb._min.y) * (camY - aabb._min.y);

		if ( camZ > aabb._max.z ) sqrDist += (camZ - aabb._max.z) * (camZ - aabb._max.z);
		else if ( camZ < aabb._min.z ) sqrDist += (camZ - aabb._min.z) * (camZ - aabb._min.z);

		float distLODShadow = ggtrees_global_params.lod_dist_shadow + (float)GGTREES_LOD_SHADOW_TRANSITION*2 + treeMaxHeight;
		float sqrDistLODShadow = distLODShadow * distLODShadow;

		if ( sqrDist > sqrDistLODShadow ) continue;

		// check individual trees
		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsInvalid() ) continue;

			sqrDist = 0;
			sqrDist += (pInstance->x - camX) * (pInstance->x - camX);
			sqrDist += (pInstance->y - camY) * (pInstance->y - camY);
			sqrDist += (pInstance->z - camZ) * (pInstance->z - camZ);

			uint32_t treeType = pInstance->GetType();

			// error trap if data held exceeds latest tree choices
			if (treeType >= numTreeTypes)
			{
				treeType = numTreeTypes - 1;
			}

			float scale = pInstance->GetScaleFloat();
			float height = g_GGTrees[ treeType ].height * scale;
			float halfHeight = height / 2.0f;

			XMFLOAT3 pos;
			pos.x = pInstance->x;
			pos.y = pInstance->y + halfHeight;
			pos.z = pInstance->z;

			if ( sqrDist <= sqrDistLODShadow ) 
			{
				// add tree to shadow draw list
				uint32_t index = numTreeInstancesHighShadow[ treeType ];
				if (index < 999)
				{
					// only if fit within dynamic array
					treeInstancesHighShadow[treeType][index].x = pInstance->x;
					treeInstancesHighShadow[treeType][index].y = pInstance->y;
					treeInstancesHighShadow[treeType][index].z = pInstance->z;
					treeInstancesHighShadow[treeType][index].data = pInstance->data;
					numTreeInstancesHighShadow[treeType]++;
				}
			}
		}
	}

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( numTreeInstancesHighShadow[ i ] > 0 )
		{
			GPUBufferDesc bufferDesc = {};
			SubresourceData data = {};
			data.data_ptr = treeInstancesHighShadow[ i ];
			bufferDesc.size = sizeof(InstanceTreeGPU) * numTreeInstancesHighShadow[ i ];
			bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
			//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
			bufferDesc.misc_flags = ResourceMiscFlag::NONE;
			wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferInstancesHighShadow[i] );
		}
	}

	// update shader constants buffer (shared with the 2.96 billboard-only path above)
	GGTrees_UpdateBillboardCB( camX, camY, camZ, cmd );

	wiProfiler::EndRange( range );
}

// GGMAX 2.96: fills + uploads treeConstantBuffer. Factored out of GGTrees_Update so the
// billboard pass can get it on the Wicked-terrain path, where the rest of GGTrees_Update is
// genuinely dead. Everything here is cheap: 8 sin/cos, one light lookup, numTreeTypes floats.
static void GGTrees_UpdateBillboardCB( float camX, float camY, float camZ, CommandList cmd )
{
	GGTrees_RefreshBillboardAtlasTypes();   // GGMAX 3.102: a newly placed type rebuilds the atlas
	GGTrees_EnsureBillboardAtlases();   // GGMAX 2.97: MAIN-THREAD one-shot, before any draw binds these
	float ang = 0.785398f; // (2.0 * pi) / 8.0
	for( int i = 0; i < 8; i++ )
	{
		float c = cos( ang * i );
		float s = sin( ang * i );
		treeConstantData.tree_rotMat[ i ].x = c;
		treeConstantData.tree_rotMat[ i ].y = -s;
		treeConstantData.tree_rotMat[ i ].z = s;
		treeConstantData.tree_rotMat[ i ].w = c;
	}

	wiScene::LightComponent* lightSun = wiScene::GetScene ( ).lights.GetComponent ( g_entitySunLight );
	float dirX = lightSun->direction.x;
	float dirZ = lightSun->direction.z;
	float invV = 1 / sqrt( dirX*dirX + dirZ*dirZ );

	treeConstantData.tree_rotMatShadow.x = dirZ * invV;
	treeConstantData.tree_rotMatShadow.y = dirX * invV;
	treeConstantData.tree_rotMatShadow.z = -dirX * invV;
	treeConstantData.tree_rotMatShadow.w = dirZ * invV;

	for( uint32_t i = 0; i < numTreeTypes; i++ ) 
	{
		float height = g_GGTrees[ i ].height;
		treeConstantData.tree_type[ i ].scaleX = g_GGTrees[ i ].billboardScaleX * height;
		treeConstantData.tree_type[ i ].scaleY = g_GGTrees[ i ].height;
	}

	treeConstantData.tree_playerPos.x = camX;
	treeConstantData.tree_playerPos.y = camY;
	treeConstantData.tree_playerPos.z = camZ;

	treeConstantData.tree_lodDist = ggtrees_global_params.lod_dist;
	treeConstantData.tree_lodDistShadow = ggtrees_global_params.lod_dist_shadow;
	{
		// GGMAX 3.00: 0 means "no terrain / no limit" and the VS then never clips.
		const float reach = ::GG_GetTerrainViewRadius();
		treeConstantData.tree_terrainReach = reach;
	}
	treeConstantData.tree_prepassReach = gg_tree_prepass_reach ? 1.0f : 0.0f;   // GGMAX 3.04 diagnostic
	treeConstantData.tree_debugSolid   = (uint32_t)gg_tree_debug_solid;        // GGMAX 3.05 debug mode
	treeConstantData.tree_shadeWrap    = gg_tree_shade_wrap;                  // GGMAX 3.07

	wiGraphics::GetDevice()->UpdateBuffer( &treeConstantBuffer, &treeConstantData, cmd, sizeof(TreeCB) );
	// GGMAX 3.52: UpdateBuffer is a GPU COPY and inserts NO barrier - "appropriate synchronization is expected".
	// wiRenderer barriers every one of its own; the GG path was ported with none, so a buffer could still be COPY_DEST when the same list bound it (D3D12 id 538).
	{
		GPUBarrier gg_bar = GPUBarrier::Buffer( &treeConstantBuffer, ResourceState::COPY_DST, ResourceState::CONSTANT_BUFFER );
		wiGraphics::GetDevice()->Barrier( &gg_bar, 1, cmd );
	}
}

void GGTrees_HideAll()
{
	if (!ggtrees_initialised) return;
	g_iLastLoadInvalidTypeTrees = 0;   // GGMAX 3.123: the loader calls this on every load, .tre or not
	pInvisibleTrees.Clear();

	for (uint32_t i = 0; i < numTotalTrees; i++)
	{
		InstanceTree* pTree = &pAllTrees[i];
		pInvisibleTrees.AddItem( pTree->GetID() );
		pTree->SetVisible( 0 );
	}

	for (uint32_t i = 0; i < numTreeChunks; i++)
	{
		TreeChunk* pChunk = &pTreeChunks[i];
		pChunk->Update();
	}
}
void GGTrees_DeselectHighlightedTree(void)
{
	if (!ggtrees_initialised) return;
	if ( treeHighlighted < numTotalTrees )
	{
		pAllTrees[ treeHighlighted ].SetHighlighted( 0 );
	}

	treeHighlighted = 0xFFFFFFFF;
}

void GGTrees_LockVisibility()
{
	if (!ggtrees_initialised) return;
	// Convert any trees on slopes from invalid to invisible so they are always hidden, 
	// and set everything as user moved so visible trees won't be hidden by slopes
	// Warning: This can only be undone by GGTrees_RepopulateInstances
	for (uint32_t i = 0; i < numTotalTrees; i++)
	{
		InstanceTree* pTree = &pAllTrees[i];
		if ( pTree->IsInvalid() )
		{
			if ( pTree->IsVisible() )
			{
				pInvisibleTrees.AddItem( pTree->GetID() );
				pTree->SetVisible( 0 );
			}
			pTree->SetInvalid( 0 );
		}
		pTree->SetUserMoved( 1 );
	}
}

const char* GGTrees_GetTextureName( uint32_t index )
{
	if ( index >= numTreeTypes ) return "";

	return g_GGTrees[ index ].billboardFilename;
}

void GGTrees_GetTypeName( uint32_t index, char* pOut, int size )
{
	if ( size <= 0 ) return;
	pOut[ 0 ] = 0;
	if ( index >= numTreeTypes ) return;

	// "jungletree3a_BB_SF_0.4_color.dds" -> "jungletree3a"
	const char* pName = g_GGTrees[ index ].billboardFilename;
	const char* pEnd = strstr( pName, "_BB_" );
	int length = pEnd ? (int)(pEnd - pName) : (int)strlen( pName );
	if ( length > size - 1 ) length = size - 1;
	memcpy( pOut, pName, length );
	pOut[ length ] = 0;
}

float GGTrees_GetImageScale( uint32_t index )
{
	if ( index >= numTreeTypes ) return 1;

	return g_GGTrees[ index ].billboardScaleX;
}

void GGTrees_UpdateFlatArea( int mode, int type, float posX, float posZ, float sx, float sz, float angle )
{
	if (!ggtrees_initialised) return;
	float realMinX = 1e20f;
	float realMinZ = 1e20f;
	float realMaxX = -1e20f;
	float realMaxZ = -1e20f;

	if ( type == 0 )
	{
		float ca = (float) cos( angle * PI / 180.0f );
		float sa = (float) sin( angle * PI / 180.0f );

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

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( realMinX > aabb._max.x || realMinZ > aabb._max.z || realMaxX < aabb._min.x || realMaxZ < aabb._min.z ) continue;

		for( int j = 0; j < (int)pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pTree = pChunk->pInstances[ j ];
			if ( type == 0 )
			{
				float ca = (float) cos( angle * PI / 180.0f );
				float sa = (float) sin( angle * PI / 180.0f );

				float fpX = pTree->x - posX;
				float fpZ = pTree->z - posZ;

				float faX = fpX * ca - fpZ * sa;
				float faZ = fpX * sa + fpZ * ca;

				float halfX = sx / 2.0f;
				float halfZ = sz / 2.0f;
				if ( faX < -halfX ) continue;
				if ( faX >  halfX ) continue;
				if ( faZ < -halfZ ) continue;
				if ( faZ >  halfZ ) continue;
			}
			else
			{
				float radius = sx / 2.0f;
				float diffX = posX - pTree->x;
				float diffZ = posZ - pTree->z;
				float sqrDist = diffX*diffX + diffZ*diffZ;
				if ( sqrDist > radius*radius ) continue;
			}

			if ( mode == 0 ) pTree->SetFlattened( 1 ); // remove trees
			else pTree->SetFlattened( 0 ); // restore trees
		}

		pChunk->Update();
	}
}

void GGTrees_RestoreAllFlattened()
{
	if (!ggtrees_initialised) return;
	for( uint32_t i = 0; i < numTotalTrees; i++ )
	{
		pAllTrees[ i ].SetFlattened( 0 );
	}
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::Render()
extern "C" void GGTrees_Draw_Prepass( const Frustum* frustum, int mode, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;

	{
		const float reach = ::GG_GetTerrainViewRadius();   // GGMAX 2.98: prepass runs first
		gg_ftTerrainReach2 = ( reach > 0.0f ) ? ( reach * reach ) : 0.0f;
	}
	// GGMAX 2.97: atlas readiness is a GATE here, never an init. Creating textures inside a draw
	// callback (job thread) and binding them in the same command list AV'd in
	// DescriptorBinder::flush - twice. The init now runs on the MAIN thread from GGTrees_Update.
	if ( !GGTrees_BillboardAtlasesReady() ) return;

	// GGMAX 2.96: same PSO lifetime bug as GGTrees_Draw - fixed by GGTreeCreatePSO.
	// The prepass is STRUCTURALLY REQUIRED, not optional: the main billboard pass runs with
	// depth_write_mask = ZERO precisely because DX11 has the prepass lay the depth down first.
	if ( !gg_far_tree_pass ) return;
	// GGMAX 3.00: honour "Trees Off" HERE, directly. gg_no_trees only reaches
	// ggtrees_global_params.draw_enabled when Wicked_Update_Visuals re-runs, and the Trees Off
	// tick box does not trigger that - which is why ticking WATER Off (which does call
	// GGApplyVisualsNow) was what made the billboards finally vanish. Lee spotted exactly that.
	if ( ::gg_no_trees ) return;

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGTrees Prepass Draw", cmd);
	// wiProfiler calls removed — they acquire a mutex that deadlocks with job system threads
	wiProfiler::range_id range = {};

	device->BindPipelineState( ( gg_tree_debug_solid == 4 ) ? &psoTreesPrepassNoDepth : &psoTreesPrepass, cmd );   // GGMAX 3.06

	uint32_t bindSlot = 3;
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	device->BindResource( &texTree, 50, cmd );
	device->BindResource( &texNoise, 51, cmd );
	device->BindResource( &texTreeNormal, 53, cmd );
	device->BindSampler( &samplerBilinearWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferTreeVertices };
	const uint32_t strides[] = { sizeof( VertexTree ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( !frustum->CheckBoxFast( aabb ) ) continue;
		// GGMAX 2.98: must match the colour pass exactly, or the prepass lays depth for trees
		// the colour pass then skips.
		if ( gg_ftTerrainReach2 > 0.0f )
		{
			const float bdx = std::max( 0.0f, std::max( aabb.getMin().x - treeConstantData.tree_playerPos.x,
			                                            treeConstantData.tree_playerPos.x - aabb.getMax().x ) );
			const float bdz = std::max( 0.0f, std::max( aabb.getMin().z - treeConstantData.tree_playerPos.z,
			                                            treeConstantData.tree_playerPos.z - aabb.getMax().z ) );
			if ( bdx * bdx + bdz * bdz > gg_ftTerrainReach2 ) continue;
		}

		const GPUBuffer* vbs[] = { &pChunk->bufferInstances };
		const uint32_t strides[] = { sizeof(InstanceTreeGPU) };
		device->BindVertexBuffers( vbs, 1, 1, strides, 0, cmd );
		device->BindIndexBuffer( &bufferTreeIndices, IndexBufferFormat::UINT16, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}

	// GGMAX 2.96: billboards only - the Wicked tree pool prepasses its own near meshes.
	device->EventEnd( cmd );
	return;

	// only draw high detail in non-reflection passes
	if ( mode == 0 )
	{
		// high detail

		device->BindPipelineState( &psoTreesHighPrepass, cmd );
		device->BindResource( &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHigh[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferTreeHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHighPrepass, cmd );
		device->BindResource( &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHigh[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferBranchesHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}
			
	}

	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine wiRenderer::DrawShadowmaps()
extern "C" void GGTrees_Draw_ShadowMap( const Frustum* frustum, int cascade, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;
	if ( !ggtrees_global_params.draw_shadows ) return;
	// DISABLED: tree Draw_ShadowMap causes GPU hang due to PSO compilation failure in DX12
	return;

	if ( cascade >= ggtrees_global_params.tree_shadow_range ) return;

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGTrees Shadow Draw", cmd);

	device->BindPipelineState( &psoTreesShadow, cmd );

	uint32_t bindSlot = 3;
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	device->BindResource( &texTree, 50, cmd );
	device->BindResource( &texNoise, 51, cmd );
	device->BindResource( &texTreeNormal, 53, cmd );
	device->BindSampler( &samplerBilinearWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferTreeVertices };
	const uint32_t strides[] = { sizeof( VertexTree ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );
	
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( !frustum->CheckBoxFast( aabb ) ) continue;

		const GPUBuffer* vbs[] = { &pChunk->bufferInstances };
		const uint32_t strides[] = { sizeof(InstanceTreeGPU) };
		device->BindVertexBuffers( vbs, 1, 1, strides, 0, cmd );
		device->BindIndexBuffer( &bufferTreeIndices, IndexBufferFormat::UINT16, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}
	
	// high detail
	if ( cascade < 3 )
	{
		device->BindPipelineState( &psoTreesHighShadow, cmd );
		device->BindResource( &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHighShadow[ i ] > 0 )
			{
				const GPUBuffer* vbs2[] = { &bufferTreeHighVertices[i], &bufferInstancesHighShadow[i] };
				const uint32_t strides2[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs2, 0, 2, strides2, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHighShadow[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHighShadow, cmd );
		device->BindResource( &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHighShadow[ i ] > 0 )
			{
				const GPUBuffer* vbs2[] = { &bufferBranchesHighVertices[i], &bufferInstancesHighShadow[i] };
				const uint32_t strides2[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs2, 0, 2, strides2, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHighShadow[i], 0, 0, 0, cmd );
			}
		}
	}
	
	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine wiRenderer::RefreshEnvProbes()
extern "C" void GGTrees_Draw_EnvProbe( const SPHERE* culler, const Frustum* frusta, uint32_t frustum_count, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;
	if ( frustum_count > 255 ) frustum_count = 255; // limited to 8 bits in instance data
	if (!GGTrees::ggtrees_draw_enabled) return;
	// DISABLED: tree Draw_EnvProbe causes GPU hang due to PSO compilation failure in DX12
	return;

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGTerrain Draw Env Probe", cmd);
	
	// environment probe doesn't render every frame, but can be expensive when it does

	int bindSlot = 3;
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	// bind texture and sampler
	device->BindSampler( &samplerBilinearWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferTreeVertices };
	const uint32_t strides[] = { sizeof( VertexTree ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );

	float cameraX = culler->center.x;
	float cameraY = culler->center.y;
	float cameraZ = culler->center.z;

	//float maxDist = ggtrees_global_params.lod_dist + GGTREES_LOD_TRANSITION*2;
	float maxDist = GGTerrain_MetersToUnits( 50 );
	float maxDistSqr = maxDist * maxDist;

	SPHERE closeSphere = *culler;
	closeSphere.radius = maxDist;

	bool anyTrees = false;

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		numTreeInstancesHighEnvProbe[ i ] = 0;
	}

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		float sqrDist = 0;
		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( cameraX > aabb._max.x ) sqrDist = (cameraX - aabb._max.x) * (cameraX - aabb._max.x);
		else if ( cameraX < aabb._min.x ) sqrDist = (cameraX - aabb._min.x) * (cameraX - aabb._min.x);

		if ( cameraY > aabb._max.y ) sqrDist += (cameraY - aabb._max.y) * (cameraY - aabb._max.y);
		else if ( cameraY < aabb._min.y ) sqrDist += (cameraY - aabb._min.y) * (cameraY - aabb._min.y);

		if ( cameraZ > aabb._max.z ) sqrDist += (cameraZ - aabb._max.z) * (cameraZ - aabb._max.z);
		else if ( cameraZ < aabb._min.z ) sqrDist += (cameraZ - aabb._min.z) * (cameraZ - aabb._min.z);
		
		if ( sqrDist > maxDistSqr ) continue;

		// check individual trees
		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsInvalid() ) continue;

			sqrDist = 0;
			sqrDist += (pInstance->x - cameraX) * (pInstance->x - cameraX);
			sqrDist += (pInstance->y - cameraY) * (pInstance->y - cameraY);
			sqrDist += (pInstance->z - cameraZ) * (pInstance->z - cameraZ);

			uint32_t treeType = pInstance->GetType();
			float scale = pInstance->GetScaleFloat();
			float height = g_GGTrees[ treeType ].height * scale;
			float halfHeight = height / 2.0f;

			XMFLOAT3 pos;
			pos.x = pInstance->x;
			pos.y = pInstance->y + halfHeight;
			pos.z = pInstance->z;

			if ( sqrDist <= maxDistSqr )
			{
				uint32_t facesMask = 0;
				for( uint32_t face = 0; face < frustum_count; face++ )
				{
					if ( frusta[ face ].CheckSphere( pos, halfHeight ) ) 
					{
						// add tree to draw list
						uint32_t index = numTreeInstancesHighEnvProbe[ treeType ];
						if ( index >= GGTREES_MAX_ENVMAP_TREES ) continue;
						treeInstancesHighEnvProbe[ treeType ][ index ].x = pInstance->x;
						treeInstancesHighEnvProbe[ treeType ][ index ].y = pInstance->y;
						treeInstancesHighEnvProbe[ treeType ][ index ].z = pInstance->z;
						treeInstancesHighEnvProbe[ treeType ][ index ].data = pInstance->data & 0xFFFFFF00; // clear out flags and use them for face mask instead
						treeInstancesHighEnvProbe[ treeType ][ index ].data |= face;
						numTreeInstancesHighEnvProbe[ treeType ]++;
						anyTrees = true;
					}
				}
			}
		}
	}

	if ( !anyTrees ) 
	{
		device->EventEnd( cmd );
		return;
	}

	// PE: Commented out - ForwardEntityMaskCB, constantBuffers, and CBTYPE_FORWARDENTITYMASK
	// were removed from WickedEngine. visibleLights is now vector<uint32_t> (no .index member).
	/*
	// find nearby lights for forward renderer
	ForwardEntityMaskCB cb;
	cb.xForwardLightMask.x = 0;
	cb.xForwardLightMask.y = 0;
	cb.xForwardDecalMask = 0;
	cb.xForwardEnvProbeMask = 0;

	uint32_t buckets[2] = { 0,0 };
	wiRenderer::Visibility *vis = &master.masterrenderer.visibility_main;
	uint32_t size = (uint32_t) vis->visibleLights.size();
	if ( size > 64 ) size = 64; // only support indexing 64 lights at max for now
	for (size_t i = 0; i < size; ++i)
	{
		const uint16_t lightIndex = vis->visibleLights[i].index;
		const AABB& light_aabb = vis->scene->aabb_lights[lightIndex];
		if ( light_aabb.intersects(closeSphere) )
		{
			const uint8_t bucket_index = uint8_t(i / 32);
			const uint8_t bucket_place = uint8_t(i % 32);
			buckets[bucket_index] |= 1 << bucket_place;
		}
	}
	cb.xForwardLightMask.x = buckets[0];
	cb.xForwardLightMask.y = buckets[1];

	// update light buffer
	device->UpdateBuffer(&wiRenderer::constantBuffers[CBTYPE_FORWARDENTITYMASK], &cb, cmd);
	// GGMAX 3.52: pair the copy with its barrier - see the note above.
	{
		GPUBarrier gg_bar = GPUBarrier::Buffer( &wiRenderer::constantBuffers[CBTYPE_FORWARDENTITYMASK], ResourceState::COPY_DST, ResourceState::CONSTANT_BUFFER );
		wiGraphics::GetDevice()->Barrier( &gg_bar, 1, cmd );
	}
	device->BindConstantBuffer(&wiRenderer::constantBuffers[CBTYPE_FORWARDENTITYMASK], CB_GETBINDSLOT(ForwardEntityMaskCB), cmd);
	*/

	// draw trees
	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( numTreeInstancesHighEnvProbe[ i ] == 0 ) continue;
		
		GPUBufferDesc bufferDesc = {};
		SubresourceData data = {};
		data.data_ptr = treeInstancesHighEnvProbe[ i ];
		bufferDesc.size = sizeof(InstanceTreeGPU) * numTreeInstancesHighEnvProbe[ i ];
		bufferDesc.bind_flags = BindFlag::VERTEX_BUFFER;
		//bufferDesc.CPUAccessFlags = 0; // removed in DX12 API
		bufferDesc.misc_flags = ResourceMiscFlag::NONE;
		wiGraphics::GetDevice()->CreateBuffer( &bufferDesc, data.data_ptr, &bufferInstancesHighEnvProbe[i] );
		
		device->BindPipelineState( &psoTreesHighEnvProbe, cmd );
		device->BindResource( &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHighEnvProbe[i] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferTreeHighVertices[i], &bufferInstancesHighEnvProbe[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHighEnvProbe[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHighEnvProbe, cmd );
		device->BindResource( &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHighEnvProbe[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferBranchesHighVertices[i], &bufferInstancesHighEnvProbe[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHighEnvProbe[i], 0, 0, 0, cmd );
			}
		}
	}

	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::Render()
extern "C" void GGTrees_Draw( const Frustum* frustum, int mode, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;
	// GGMAX 2.96: was hard-disabled with "tree Draw causes GPU hang due to PSO compilation
	// failure in DX12". The PSO failure was a C++ lifetime bug, not a graphics one - every tree
	// PSO held pointers to stack locals that died when GGTrees_Init returned, and Wicked's DX12
	// backend only compiles the pipeline at BIND time. Fixed by GGTreeCreatePSO above.
	//
	// STEP 1/2 SCOPE: the BILLBOARD pass only. The high-detail mesh section below stays off
	// because the Wicked nearest-N tree pool already draws those as ObjectComponents - running
	// both would double-draw every near tree.
	if ( !gg_far_tree_pass ) return;
	// GGMAX 3.00: honour "Trees Off" HERE, directly. gg_no_trees only reaches
	// ggtrees_global_params.draw_enabled when Wicked_Update_Visuals re-runs, and the Trees Off
	// tick box does not trigger that - which is why ticking WATER Off (which does call
	// GGApplyVisualsNow) was what made the billboards finally vanish. Lee spotted exactly that.
	if ( ::gg_no_trees ) return;
	if ( !GGTrees_BillboardAtlasesReady() ) return;   // GGMAX 2.97: gate, not init - see the prepass
	g_ftEnterCount++;
	g_ftDrawCalls = 0; g_ftInstances = 0; g_ftChunksWithIn = 0; g_ftFrustumKills = 0; g_ftTerrainKills = 0;
	g_ftChunksTotal = numTreeChunks;
	{
		const float reach = ::GG_GetTerrainViewRadius();
		gg_ftTerrainReach2 = ( reach > 0.0f ) ? ( reach * reach ) : 0.0f;
	}

	GraphicsDevice* device = wiGraphics::GetDevice();
	device->EventBegin("GGTrees Draw", cmd);
	// wiProfiler calls removed — they acquire a mutex that deadlocks with job system threads
	wiProfiler::range_id range = {};
		
	device->BindPipelineState( ( gg_tree_debug_solid == 5 ) ? &psoTreesDepthAlways : &psoTrees, cmd );   // GGMAX 3.06

	uint32_t bindSlot = 3;
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( &treeConstantBuffer, bindSlot, cmd );
	GGCustomFrame_Bind(cmd);

	// bind texture and sampler
	device->BindResource( &texTree, 50, cmd );
	device->BindResource( &texNoise, 51, cmd );
	device->BindResource( &texTreeNormal, 53, cmd );
	device->BindSampler( &samplerBilinearWrap, 0, cmd );
	device->BindSampler( &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( &samplerTrilinearWrap, 2, cmd );

	const GPUBuffer* vbs[] = { &bufferTreeVertices };
	const uint32_t strides[] = { sizeof( VertexTree ) };
	device->BindVertexBuffers( vbs, 0, 1, strides, 0, cmd );
	
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;
		g_ftChunksWithIn++;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( !frustum->CheckBoxFast( aabb ) ) { g_ftFrustumKills++; continue; }
		// GGMAX 2.98: never draw billboards past the terrain. The tree grid spans the whole
		// 200,000-unit area but terrain chunks only exist within generation*chunkU of the
		// camera, so beyond that the trees stand on nothing.
		if ( gg_ftTerrainReach2 > 0.0f )
		{
			const float bdx = std::max( 0.0f, std::max( aabb.getMin().x - treeConstantData.tree_playerPos.x,
			                                            treeConstantData.tree_playerPos.x - aabb.getMax().x ) );
			const float bdz = std::max( 0.0f, std::max( aabb.getMin().z - treeConstantData.tree_playerPos.z,
			                                            treeConstantData.tree_playerPos.z - aabb.getMax().z ) );
			if ( bdx * bdx + bdz * bdz > gg_ftTerrainReach2 ) { g_ftTerrainKills++; continue; }
		}

		const GPUBuffer* vbs[] = { &pChunk->bufferInstances };
		const uint32_t strides[] = { sizeof(InstanceTreeGPU) };
		device->BindVertexBuffers( vbs, 1, 1, strides, 0, cmd );
		device->BindIndexBuffer( &bufferTreeIndices, IndexBufferFormat::UINT16, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
		g_ftDrawCalls++; g_ftInstances += pChunk->numValid;
	}
	
	// GGMAX 2.96: high-detail meshes are the Wicked tree pool's job now - never draw them here.
	device->EventEnd( cmd );
	return;

	// only draw high detail in non-reflection passes
	if ( mode == 0 )
	{
		// high detail
		device->BindPipelineState( &psoTreesHigh, cmd );
		device->BindResource( &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHigh[i] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferTreeHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHigh, cmd );
		device->BindResource( &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHigh[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferBranchesHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], IndexBufferFormat::UINT16, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}

	}

	device->EventEnd(cmd);
}

} // namespace GGTrees

