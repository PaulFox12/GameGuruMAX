#include "GGTrees.h"

#define ONLYLOADWHENUSED

#define GGTREES_CONSTANTS_FULL_DECL
#include "Shaders/GGTreesConstants.hlsli"

#include "wiRenderer.h"
#include "wiProfiler.h"
#include "wiInput.h"
#include "wiECS.h"
#include "wiScene.h"

#include "CFileC.h"
#include "Utility/tinyddsloader.h"

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

namespace GGTrees
{
#define GGTREES_UNDOREDO

GGTreesParams ggtrees_global_params;
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

// full detail trees per type per frame (draw and shadow lists). A tree inside lod_dist has no billboard, so any tree
// past this limit is not drawn at all; 1000 was reached easily in dense forest
#define GGTREES_MAX_HIGH_DETAIL_PER_TYPE 8192

const uint32_t numTreeChunks = treeSplit * treeSplit;
const uint32_t numTreesPerChunk = numTotalTrees / numTreeChunks;
float treeAreaPerChunk = treeArea / treeSplit;

uint32_t treeHighlighted = 0xFFFFFFFF;

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
	uint32_t data; // [0]=visible, [1]=user moved, [2]=highlighted, [3]=flattened, [4]=invalid (slope or underwater), [5]=killed (removed by a blast in a game, never saved), [6-7]=reserved for flags, [8-10]=variation index, [11-16]=tree type, [17-24]=scale
	uint32_t id; // not sent to GPU

	bool IsVisible() { return (data & 0x1) != 0; }
	void SetVisible( int visible ) 
	{ 
		if ( visible ) data |= 0x1;
		else data &= ~0x1;
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
		if ( flattened ) data |= 0x8;
		else data &= ~0x8;
	}

	// removed by a blast for the rest of the level (GGTrees_KillBox, GGTrees_KillCircle)
	bool IsKilled() { return (data & 0x20) != 0; }
	void SetKilled( int killed )
	{
		if ( killed ) data |= 0x20;
		else data &= ~0x20;
	}

	// invalid if tree is on a slope or underwater
	bool IsInvalid() { return (data & 0x10) != 0; }
	void SetInvalid( int invalid ) 
	{ 
		if ( invalid ) data |= 0x10;
		else data &= ~0x10;
	}

	void SetType( uint32_t type )
	{
		if ( type >= numTreeTypes ) return;
		unsigned int mask = (0x3F << 11);
		data = (data & ~mask) | (type << 11);
	}

	void SetFlags( uint32_t flags )
	{
		data = (data & ~0xFF) | (flags & 0xFF);
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
		data = (data & 0xFF01FFFF) | (scale << 17);
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

// GG: each chunk's run of trees in a type's shadow list, so a cascade draws only the chunks it can see
struct ShadowChunkRun
{
	uint32_t chunk;
	uint32_t start;
	uint32_t count;
};
std::vector<ShadowChunkRun> shadowChunkRuns[ numTreeTypes ];
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

// a drawn tree's trunk as rays test it (GGTrees_RayCastTrunks): a vertical cylinder from the tree's base to its top, as
// thick as the physics' tree cylinder (the species' thickness times the tree's scale, across)
struct TreeTrunk
{
	float x, z;
	float radius;
	float base, top;
	int type;
};

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
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsKilled() || pInstance->IsInvalid() ) continue;

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
		numValid = 0;
		for( uint32_t j = 0; j < pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pInstances[ j ];
			int type = pInstance->GetType();
			float treeHeight = g_GGTrees[ type ].height;

			if ( pInstance->y < minHeight ) minHeight = pInstance->y;
			if ( pInstance->y + treeHeight > maxHeight ) maxHeight = pInstance->y + treeHeight;

			if ( pInstance->IsVisible() && !pInstance->IsFlattened() && !pInstance->IsKilled() && !pInstance->IsInvalid() ) 
			{
				pData[ numValid ].x = pInstance->x;
				pData[ numValid ].y = pInstance->y;
				pData[ numValid ].z = pInstance->z;
				pData[ numValid ].data = pInstance->data;
				numValid++;

				TreeTrunk trunk;
				pInstance->GetTrunkCentre( &trunk.x, &trunk.z );
				trunk.radius = pInstance->GetTreeThickness() * pInstance->GetScaleFloat() * 0.5f;
				trunk.base = pInstance->y;
				trunk.top = pInstance->y + treeHeight * pInstance->GetScaleFloat();
				trunk.type = pInstance->GetType();
				trunks.push_back( trunk );
			}
		}

		if (numValid == 0)
		{
			//PE: Leak - if numValid == 0 memory was not freed.
			delete[] pData;
			return;
		}

		GPUBufferDesc bufferDesc = {};
		SubresourceData data = {};
		data.pSysMem = pData;
		bufferDesc.ByteWidth = sizeof(InstanceTreeGPU) * numValid;
		bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferInstances );
		
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
GPUBuffer treeConstantBufferNoShadowCut; // GG: for cascades without full detail tree shadows

// low detail
GPUBuffer bufferTreeVertices;
GPUBuffer bufferTreeIndices;

Shader shaderTreesVS;
Shader shaderTreesPS;
PipelineState psoTrees;

Shader shaderTreesPrepassVS;
Shader shaderTreesPrepassPS;
PipelineState psoTreesPrepass;

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

void GGTrees_LoadTextureDDS( const char* filename, Texture* tex ) 
{ 
	if (!ggtrees_initialised) return;
	GraphicsDevice* device = wiRenderer::GetDevice();
	
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
	desc.Width = dds.GetWidth();
	desc.Height = dds.GetHeight();
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

void GGTrees_CreateEmptyTexture( int width, int height, int mipLevels, int levels, FORMAT format, Texture* tex ) 
{
	if (!ggtrees_initialised) return;
	GraphicsDevice* device = wiRenderer::GetDevice();
	
	TextureDesc texDesc = {};
	texDesc.BindFlags = BIND_SHADER_RESOURCE;
	texDesc.SampleCount = 1;
	texDesc.MipLevels = mipLevels;
	texDesc.ArraySize = levels;
	texDesc.Format = format;
	texDesc.Usage = USAGE_DEFAULT;
	texDesc.Width = width;
	texDesc.Height = height;

	device->CreateTexture( &texDesc, nullptr, tex );
	device->SetName( tex, "tex" );
}

void GGTrees_LoadTextureDDSIntoSlice( const char* filename, Texture* tex, uint32_t arraySlice ) 
{ 
	if (!ggtrees_initialised) return;
	GraphicsDevice* device = wiRenderer::GetDevice();
	
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

		// SetData clears every flag; keep a tree under a flatten pad flattened, as GGTrees_RepopulateInstances does
		bool flattened = pInstance->IsFlattened();
		pInstance->SetData(j, chosenType, 1);
		pInstance->SetScale(Random(ggtrees_global_params.paint_scale_random_low, ggtrees_global_params.paint_scale_random_high));
		if (flattened) pInstance->SetFlattened(1);
		int test = j % 100;
		// a slot outside the tree area is in no chunk, so it stays hidden and free for the Add tool
		if (test >= ggtrees_global_params.paint_density || !GGTrees_GetChunk(pInstance->x, pInstance->z))
		{
			pInstance->SetVisible(0);
		}
	}

	// rebuild the list of hidden slots. It was updated per tree, but SetData above shows every tree first, so trees shown
	// here stayed in the list and trees hidden again were added twice; the Add tool could then move a visible tree
	pInvisibleTrees.Clear();
	for (uint32_t j = 0; j < numTotalTrees; j++)
	{
		if (!pAllTrees[j].IsVisible()) pInvisibleTrees.AddItem(j);
	}
}

uint32_t GGTrees_GetNumHighDetail()
{
	uint32_t total = 0;
	for( uint32_t i = 0; i < numTreeTypes; i++ ) total += numTreeInstancesHigh[ i ];
	return total;
}

uint32_t GGTrees_GetNumHighDetailShadow()
{
	uint32_t total = 0;
	for( uint32_t i = 0; i < numTreeTypes; i++ ) total += numTreeInstancesHighShadow[ i ];
	return total;
}

// only call this when tree heights need to be updated, e.g. when the terrain has changed
int GGTrees_UpdateInstances( int accurate )
{
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
void GGTrees_LoadTextures(bool bInit = false)
{
	char path[1024];
	for (int i = 0; i < numTreeTypes; i++)
	{
		if (!bTreeTextureUploaded[i])
		{
			if (g_GGTrees[i].height > treeMaxHeight) treeMaxHeight = g_GGTrees[i].height;
			if (ggtrees_global_params.paint_tree_bitfield & (1ULL << i) || bTreeTextureForceUploaded[i])
			{
				bTreeTextureUploaded[i] = true;

				char szDstRoot[MAX_PATH];
				//sprintf_s(szDstRoot, MAX_PATH, "%streebank/", FinalGameGuruMaxFolder.c_str());
				if(bInit)
					strcpy_s(szDstRoot, "files/treebank/");
				else
					strcpy_s(szDstRoot, "treebank/");

				strcpy_s(path, szDstRoot);
				strcat_s(path, "billboards/");
				strcat_s(path, g_GGTrees[i].billboardFilename);
				GGTrees_LoadTextureDDSIntoSlice(path, &texTree, i);

				strcpy_s(path, szDstRoot);
				strcat_s(path, "billboards/");
				strcat_s(path, g_GGTrees[i].billboardNormalFilename);
				GGTrees_LoadTextureDDSIntoSlice(path, &texTreeNormal, i);

				strcpy_s(path, szDstRoot);
				strcat_s(path, "textures/");
				strcat_s(path, g_GGTrees[i].trunk->textureName);
				GGTrees_LoadTextureDDSIntoSlice(path, &texTreeHigh, i);

				if (g_GGTrees[i].branches)
				{
					strcpy_s(path, szDstRoot);
					strcat_s(path, "textures/");
					strcat_s(path, g_GGTrees[i].branches->textureName);
					GGTrees_LoadTextureDDSIntoSlice(path, &texBranchesHigh, i);
				}
			}
		}
	}

}
#endif

void GGTrees_Init()
{
	ggtrees_initialised = 1;
	GraphicsDevice* device = wiRenderer::GetDevice();

	wiRenderer::LoadShader( VS, shaderTreesVS, "GGTreesVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesPS, "GGTreesPS.cso" );

	wiRenderer::LoadShader( VS, shaderTreesPrepassVS, "GGTreesPrepassVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesPrepassPS, "GGTreesPrepassPS.cso" );

	wiRenderer::LoadShader( VS, shaderTreesShadowVS, "GGTreesShadowMapVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesShadowPS, "GGTreesShadowMapPS.cso" );

	wiRenderer::LoadShader( VS, shaderTreesHighVS, "GGTreesHighVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesHighPS, "GGTreesHighPS.cso" );

	wiRenderer::LoadShader( VS, shaderTreesHighPrepassVS, "GGTreesHighPrepassVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesHighPrepassPS, "GGTreesHighPrepassPS.cso" );

	wiRenderer::LoadShader( VS, shaderTreesHighShadowVS, "GGTreesHighShadowMapVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesHighShadowPS, "GGTreesHighShadowMapPS.cso" );

	wiRenderer::LoadShader( VS, shaderBranchesHighVS, "GGTreeBranchesHighVS.cso" );
	wiRenderer::LoadShader( PS, shaderBranchesHighPS, "GGTreeBranchesHighPS.cso" );
									  
	wiRenderer::LoadShader( VS, shaderBranchesHighPrepassVS, "GGTreeBranchesHighPrepassVS.cso" );
	wiRenderer::LoadShader( PS, shaderBranchesHighPrepassPS, "GGTreeBranchesHighPrepassPS.cso" );
									  
	wiRenderer::LoadShader( VS, shaderBranchesHighShadowVS, "GGTreeBranchesHighShadowMapVS.cso" );
	wiRenderer::LoadShader( PS, shaderBranchesHighShadowPS, "GGTreeBranchesHighShadowMapPS.cso" );

	wiRenderer::LoadShader( VS, shaderTreesHighEnvProbeVS, "GGTreesHighEnvProbeVS.cso" );
	wiRenderer::LoadShader( PS, shaderTreesHighEnvProbePS, "GGTreesHighEnvProbePS.cso" );

	wiRenderer::LoadShader( VS, shaderBranchesHighEnvProbeVS, "GGTreeBranchesHighEnvProbeVS.cso" );
	wiRenderer::LoadShader( PS, shaderBranchesHighEnvProbePS, "GGTreeBranchesHighEnvProbePS.cso" );

	GGTrees_LoadTextureDDS( "Files/treebank/noise.dds", &texNoise );

	// billboard textures
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, FORMAT_BC3_UNORM_SRGB, &texTree );
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, FORMAT_BC1_UNORM, &texTreeNormal );

	// high detail tree textures
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, FORMAT_BC1_UNORM_SRGB, &texTreeHigh );
	GGTrees_CreateEmptyTexture( 1024, 1024, 9, numTreeTypes, FORMAT_BC3_UNORM_SRGB, &texBranchesHigh );

#ifdef ONLYLOADWHENUSED
	for (int i = 0; i < numTreeTypes; i++)
	{
		bTreeTextureUploaded[i] = false;
		bTreeTextureForceUploaded[i] = false;
		treeInstancesHigh[i] = new InstanceTreeGPU[GGTREES_MAX_HIGH_DETAIL_PER_TYPE];
		treeInstancesHighShadow[i] = new InstanceTreeGPU[GGTREES_MAX_HIGH_DETAIL_PER_TYPE];
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

		treeInstancesHigh[ i ] = new InstanceTreeGPU[ GGTREES_MAX_HIGH_DETAIL_PER_TYPE ];
		treeInstancesHighShadow[ i ] = new InstanceTreeGPU[ GGTREES_MAX_HIGH_DETAIL_PER_TYPE ];
	}
#endif

	//GGTerrain_DecompressImage( "E:/Programs/GameGuruMAX/Max/Files/pinetree_BC7.dds", "E:/Programs/GameGuruMAX/Max/Files/pinetree_RGB.dds" );

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		pTreeChunks[ i ].x = i % treeSplit;
		pTreeChunks[ i ].z = i / treeSplit;
	}

	GGTrees_RepopulateInstances();

	// raster state
	RasterizerState rastState = {};
	rastState.FillMode = FILL_SOLID;
	rastState.CullMode = CULL_NONE;
	rastState.FrontCounterClockwise = true;
	rastState.DepthBias = 0;
	rastState.DepthBiasClamp = 0;
	rastState.SlopeScaledDepthBias = 0;
	rastState.DepthClipEnable = true;
	rastState.MultisampleEnable = false;
	rastState.AntialiasedLineEnable = false;
	
	// depth stencil state
	DepthStencilState depthStateOpaque = {};
	depthStateOpaque.DepthEnable = true;
	depthStateOpaque.DepthFunc = COMPARISON_GREATER_EQUAL;
	depthStateOpaque.StencilEnable = false;
	depthStateOpaque.DepthWriteMask = DEPTH_WRITE_MASK_ALL;
	
	// blend state
	BlendState blendStateOpaque = {};
	blendStateOpaque.RenderTarget[0].BlendEnable = false;
	blendStateOpaque.RenderTarget[0].SrcBlend = BLEND_ONE;
	blendStateOpaque.RenderTarget[0].DestBlend = BLEND_ZERO;
	blendStateOpaque.RenderTarget[0].BlendOp = BLEND_OP_ADD;
	blendStateOpaque.RenderTarget[0].SrcBlendAlpha = BLEND_ONE;
	blendStateOpaque.RenderTarget[0].DestBlendAlpha = BLEND_ZERO;
	blendStateOpaque.RenderTarget[0].BlendOpAlpha = BLEND_OP_ADD;
	blendStateOpaque.RenderTarget[0].RenderTargetWriteMask = COLOR_WRITE_ENABLE_ALL;
	blendStateOpaque.IndependentBlendEnable = false;
	blendStateOpaque.AlphaToCoverageEnable = true;
	
	// input layout
	InputLayout inputLayout;
	inputLayout.elements = {
		{ "POSITION", 0, wiGraphics::FORMAT_R32G32_FLOAT,    0, 0,  INPUT_PER_VERTEX_DATA },
		{ "OFFSET",   0, wiGraphics::FORMAT_R32G32B32_FLOAT, 1, 0,  INPUT_PER_INSTANCE_DATA },
		{ "DATA",     0, wiGraphics::FORMAT_R32_UINT,        1, 12, INPUT_PER_INSTANCE_DATA },
	};

	InputLayout inputLayoutHigh;
	inputLayoutHigh.elements = {
		{ "POSITION", 0, wiGraphics::FORMAT_R32G32B32_FLOAT, 0, 0,  INPUT_PER_VERTEX_DATA },
		{ "INORMAL",  0, wiGraphics::FORMAT_R8G8B8A8_UNORM,  0, 12, INPUT_PER_VERTEX_DATA },
		{ "UV",       0, wiGraphics::FORMAT_R32G32_FLOAT,    0, 16, INPUT_PER_VERTEX_DATA },
		{ "OFFSET",   0, wiGraphics::FORMAT_R32G32B32_FLOAT, 1, 0,  INPUT_PER_INSTANCE_DATA },
		{ "DATA",     0, wiGraphics::FORMAT_R32_UINT,        1, 12, INPUT_PER_INSTANCE_DATA },
	};

	// pipeline state object
	PipelineStateDesc desc = {};
	desc.vs = &shaderTreesVS;
	desc.ps = &shaderTreesPS;

	desc.il = &inputLayout;
	desc.pt = TRIANGLELIST;
	desc.rs = &rastState;
	desc.dss = &depthStateOpaque;
	desc.bs = &blendStateOpaque;
	depthStateOpaque.DepthWriteMask = DEPTH_WRITE_MASK_ZERO;
	device->CreatePipelineState( &desc, &psoTrees );

	rastState.CullMode = CULL_BACK;
	desc.vs = &shaderTreesHighVS;
	desc.ps = &shaderTreesHighPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.AlphaToCoverageEnable = false;
	device->CreatePipelineState( &desc, &psoTreesHigh );

	rastState.CullMode = CULL_NONE;
	desc.vs = &shaderBranchesHighVS;
	desc.ps = &shaderBranchesHighPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.AlphaToCoverageEnable = true;
	device->CreatePipelineState( &desc, &psoBranchesHigh );

	rastState.CullMode = CULL_BACK;
	depthStateOpaque.DepthWriteMask = DEPTH_WRITE_MASK_ALL;
	desc.vs = &shaderTreesHighEnvProbeVS;
	desc.ps = &shaderTreesHighEnvProbePS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.AlphaToCoverageEnable = false;
	device->CreatePipelineState( &desc, &psoTreesHighEnvProbe );

	rastState.CullMode = CULL_NONE;
	desc.vs = &shaderBranchesHighEnvProbeVS;
	desc.ps = &shaderBranchesHighEnvProbePS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.AlphaToCoverageEnable = true;
	device->CreatePipelineState( &desc, &psoBranchesHighEnvProbe );

	// prepass pipeline state
	desc.vs = &shaderTreesPrepassVS;
	desc.ps = &shaderTreesPrepassPS;
	desc.il = &inputLayout;
	depthStateOpaque.DepthWriteMask = DEPTH_WRITE_MASK_ALL;
	device->CreatePipelineState( &desc, &psoTreesPrepass );

	rastState.CullMode = CULL_BACK;
	desc.vs = &shaderTreesHighPrepassVS;
	desc.ps = &shaderTreesHighPrepassPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.AlphaToCoverageEnable = false;
	device->CreatePipelineState( &desc, &psoTreesHighPrepass );

	rastState.CullMode = CULL_NONE;
	desc.vs = &shaderBranchesHighPrepassVS;
	desc.ps = &shaderBranchesHighPrepassPS;
	desc.il = &inputLayoutHigh;
	blendStateOpaque.AlphaToCoverageEnable = true;
	device->CreatePipelineState( &desc, &psoBranchesHighPrepass );

	// shadow pipeline state
	rastState.DepthBias = -1;
	rastState.SlopeScaledDepthBias = -4.0f;
	rastState.MultisampleEnable = false;
	desc.vs = &shaderTreesShadowVS;
	desc.ps = &shaderTreesShadowPS;
	desc.il = &inputLayout;
	rastState.DepthClipEnable = false;
	blendStateOpaque.AlphaToCoverageEnable = false;
	device->CreatePipelineState( &desc, &psoTreesShadow );
	desc.vs = &shaderTreesHighShadowVS;
	desc.ps = &shaderTreesHighShadowPS;
	desc.il = &inputLayoutHigh;
	rastState.CullMode = CULL_BACK;
	device->CreatePipelineState( &desc, &psoTreesHighShadow );
	desc.vs = &shaderBranchesHighShadowVS;
	desc.ps = &shaderBranchesHighShadowPS;
	rastState.CullMode = CULL_NONE;
	device->CreatePipelineState( &desc, &psoBranchesHighShadow );
	rastState.DepthBias = 0;
	rastState.DepthClipEnable = true;
	rastState.SlopeScaledDepthBias = 0;

	// samplers
	SamplerDesc samplerDesc;
	samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
	samplerDesc.Filter = FILTER_MIN_MAG_MIP_LINEAR;
	device->CreateSampler( &samplerDesc, &samplerTrilinearClamp );

	samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
	samplerDesc.Filter = FILTER_MIN_MAG_MIP_LINEAR;
	device->CreateSampler( &samplerDesc, &samplerTrilinearWrap );

	samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
	samplerDesc.Filter = FILTER_MIN_MAG_LINEAR_MIP_POINT;
	device->CreateSampler( &samplerDesc, &samplerBilinearWrap );

	// constant buffer
	GPUBufferDesc bd = {};
	bd.Usage = USAGE_DEFAULT;
	bd.ByteWidth = sizeof(TreeCB);
	bd.BindFlags = BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	wiRenderer::GetDevice()->CreateBuffer( &bd, nullptr, &treeConstantBuffer );
	wiRenderer::GetDevice()->CreateBuffer( &bd, nullptr, &treeConstantBufferNoShadowCut );

	// vertex buffer
	GPUBufferDesc bufferDesc = {};
	SubresourceData data = {};
	data.pSysMem = g_VerticesTreeBillboard;
	bufferDesc.ByteWidth = sizeof(g_VerticesTreeBillboard);
	bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferTreeVertices );

	// index buffer
	data.pSysMem = g_IndicesTreeBillboard;
	bufferDesc.ByteWidth = sizeof(g_IndicesTreeBillboard);
	bufferDesc.BindFlags = BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferTreeIndices );

	// vertex buffer high detail
	bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		data.pSysMem = g_GGTrees[ i ].trunk->pVertices;
		bufferDesc.ByteWidth = g_GGTrees[ i ].trunk->numVertices * sizeof(VertexTreeHigh);
		wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferTreeHighVertices[i] );
	}

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( !g_GGTrees[ i ].branches ) continue; // some don't have branches
		data.pSysMem = g_GGTrees[ i ].branches->pVertices;
		bufferDesc.ByteWidth = g_GGTrees[ i ].branches->numVertices * sizeof(VertexTreeHigh);
		wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferBranchesHighVertices[i] );
	}

	// index buffer high detail
	bufferDesc.BindFlags = BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		data.pSysMem = g_GGTrees[ i ].trunk->pIndices;
		bufferDesc.ByteWidth = g_GGTrees[ i ].trunk->numIndices * sizeof(uint16_t);
		wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferTreeHighIndices[i] );
	}

	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( !g_GGTrees[ i ].branches ) continue; // some don't have branches
		data.pSysMem = g_GGTrees[ i ].branches->pIndices;
		bufferDesc.ByteWidth = g_GGTrees[ i ].branches->numIndices * sizeof(uint16_t);
		wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferBranchesHighIndices[i] );
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
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsKilled() || pInstance->IsInvalid() ) continue;

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
int GGTrees_SweepBoxTrunks( const float* pCentre, const float* pHalf, float yawDegrees, const float* pMotion, float* pOut, int* pType )
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
			if ( boxY - hy > trunk.top || boxY + hy < trunk.base ) continue;
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
	if ( hitY < pBest->base ) hitY = pBest->base;
	if ( hitY > pBest->top ) hitY = pBest->top;
	pOut[ 0 ] = t;
	pOut[ 1 ] = pBest->x + nx * pBest->radius;
	pOut[ 2 ] = hitY;
	pOut[ 3 ] = pBest->z + nz * pBest->radius;
	pOut[ 4 ] = nx; pOut[ 5 ] = 0; pOut[ 6 ] = nz;
	if ( pType ) *pType = pBest->type;
	return 1;
}

// how many tree trunks a box (as GGTrees_SweepBoxTrunks's) touches now
int GGTrees_OverlapBoxTrunks( const float* pCentre, const float* pHalf, float yawDegrees )
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
			if ( pCentre[1] - hy > trunk.top || pCentre[1] + hy < trunk.base ) continue;
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
		dataInt[ i * 3 + 2 ] = pAllTrees[ i ].data & ~0x20; // a tree a blast removed is saved as it was
	}

	return 1;
}

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

	// the terrain settings are loaded first, so the tree area matches this level
	GGTrees_SetArea( GGTrees_GetRequiredArea() );
	memset( pTreeHeightValid, 0, sizeof(pTreeHeightValid) );

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		pTreeChunks[ i ].pInstances.Clear();
	}
	pInvisibleTrees.Clear();

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
	GGTrees_UpdateInstances( 1 );
	return 1;
}

struct GGTreesLuaOverrides
{
	float lod_dist = 0;
	float lod_dist_shadow = 0;
	float lod_transition = 0;
	float lod_transition_shadow = 0;
	int tree_shadow_range = -1;
	int tree_shadow_range_high = -1;
};
GGTreesLuaOverrides ggtrees_lua_overrides;

void GGTrees_ApplyLuaOverrides()
{
	if ( ggtrees_lua_overrides.lod_dist > 0 ) ggtrees_global_params.lod_dist = ggtrees_lua_overrides.lod_dist;
	if ( ggtrees_lua_overrides.lod_dist_shadow > 0 ) ggtrees_global_params.lod_dist_shadow = ggtrees_lua_overrides.lod_dist_shadow;
	if ( ggtrees_lua_overrides.lod_transition > 0 ) ggtrees_global_params.lod_transition = ggtrees_lua_overrides.lod_transition;
	if ( ggtrees_lua_overrides.lod_transition_shadow > 0 ) ggtrees_global_params.lod_transition_shadow = ggtrees_lua_overrides.lod_transition_shadow;
	if ( ggtrees_lua_overrides.tree_shadow_range >= 0 ) ggtrees_global_params.tree_shadow_range = ggtrees_lua_overrides.tree_shadow_range;
	if ( ggtrees_lua_overrides.tree_shadow_range_high >= 0 ) ggtrees_global_params.tree_shadow_range_high = ggtrees_lua_overrides.tree_shadow_range_high;
}

void GGTrees_SetLuaDistances( float lodDist, float lodDistShadow )
{
	if ( lodDist > 0 ) ggtrees_lua_overrides.lod_dist = lodDist;
	if ( lodDistShadow > 0 ) ggtrees_lua_overrides.lod_dist_shadow = lodDistShadow;
	GGTrees_ApplyLuaOverrides();
}

void GGTrees_SetLuaTransitions( float lodTransition, float lodTransitionShadow )
{
	if ( lodTransition > 0 ) ggtrees_lua_overrides.lod_transition = lodTransition;
	if ( lodTransitionShadow > 0 ) ggtrees_lua_overrides.lod_transition_shadow = lodTransitionShadow;
	GGTrees_ApplyLuaOverrides();
}

void GGTrees_SetLuaShadowCascades( int billboardCascades, int fullDetailCascades )
{
	if ( billboardCascades >= 0 ) ggtrees_lua_overrides.tree_shadow_range = billboardCascades;
	if ( fullDetailCascades >= 0 ) ggtrees_lua_overrides.tree_shadow_range_high = fullDetailCascades;
	GGTrees_ApplyLuaOverrides();
}

void GGTrees_ClearLuaOverrides()
{
	ggtrees_lua_overrides = GGTreesLuaOverrides();
}

void GGTrees_ResetDistances()
{
	GGTreesParams defaults;
	ggtrees_global_params.lod_dist = defaults.lod_dist;
	ggtrees_global_params.lod_dist_shadow = defaults.lod_dist_shadow;
	ggtrees_global_params.lod_transition = defaults.lod_transition;
	ggtrees_global_params.lod_transition_shadow = defaults.lod_transition_shadow;
	ggtrees_global_params.tree_shadow_range = defaults.tree_shadow_range;
	ggtrees_global_params.tree_shadow_range_high = defaults.tree_shadow_range_high;
}

// GG: the tree distances the level was made with (set at level load and when a test game starts); the qualities scale them
GGTreesParams ggtrees_level_lod_params;

void GGTrees_KeepLevelDistances()
{
	ggtrees_level_lod_params = ggtrees_global_params;
}

void GGTrees_SetPerformanceMode( uint32_t mode )
{
	// GG: High is the level's own tree distances and the other qualities scale them, as the grass distance does; they used
	// to replace them with fixed values (Low 1000, Medium 2000, High 3000, Ultra 4000) and left the transitions alone
	float scale = 1.0f;
	switch( mode )
	{
		case GGTERRAIN_PERFORMANCE_LOW: scale = 0.4f; break;
		case GGTERRAIN_PERFORMANCE_MED: scale = 0.7f; break;
		case GGTERRAIN_PERFORMANCE_HIGH: scale = 1.0f; break;
		case GGTERRAIN_PERFORMANCE_ULTRA: scale = 1.3f; break;
	}
	const GGTreesParams& level = ggtrees_level_lod_params;
	ggtrees_global_params.lod_dist = level.lod_dist * scale;
	ggtrees_global_params.lod_dist_shadow = level.lod_dist_shadow * scale;

	// a narrow crossfade close to the camera shows the swap, so keep at least 300 units (or the level's own if less)
	float minTransition = level.lod_transition < 300.0f ? level.lod_transition : 300.0f;
	float minTransitionShadow = level.lod_transition_shadow < 300.0f ? level.lod_transition_shadow : 300.0f;
	float transition = level.lod_transition * scale;
	float transitionShadow = level.lod_transition_shadow * scale;
	if ( transition < minTransition ) transition = minTransition;
	if ( transitionShadow < minTransitionShadow ) transitionShadow = minTransitionShadow;
	if ( transition > 4000.0f ) transition = 4000.0f;
	if ( transitionShadow > 4000.0f ) transitionShadow = 4000.0f;
	ggtrees_global_params.lod_transition = transition;
	ggtrees_global_params.lod_transition_shadow = transitionShadow;

	// trees cast into cascades 0-2 at most; Low keeps them to the nearest two
	ggtrees_global_params.tree_shadow_range = level.tree_shadow_range;
	if ( mode == GGTERRAIN_PERFORMANCE_LOW && ggtrees_global_params.tree_shadow_range > 2 ) ggtrees_global_params.tree_shadow_range = 2;

	// a game that set its own tree distances keeps them through a quality change
	GGTrees_ApplyLuaOverrides();
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

	pInstance->x = x;
	pInstance->z = z;
	pTreeHeightValid[ treeID ] = 0;

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

		float distLOD = ggtrees_global_params.lod_dist + ggtrees_global_params.lod_transition*2 + treeMaxHeight;
		float sqrDistLOD = distLOD * distLOD;

		if ( sqrDist > sqrDistLOD ) continue;

		// check individual trees
		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsKilled() || pInstance->IsInvalid() ) continue;

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
					if ( index < GGTREES_MAX_HIGH_DETAIL_PER_TYPE )
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
			data.pSysMem = treeInstancesHigh[ i ];
			bufferDesc.ByteWidth = sizeof(InstanceTreeGPU) * numTreeInstancesHigh[ i ];
			bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferInstancesHigh[i] );
		}
	}
}

void GGTrees_Update(float camX, float camY, float camZ, CommandList cmd, bool bRenderTargetFocus)
{
	if (!ggtrees_initialised) return;
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	if (!ggtrees_global_params.draw_enabled && ggtrees_global_params.hide_until_update == 0) return;

#ifdef ONLYLOADWHENUSED
	if (last_paint_tree_bitfield != ggtrees_global_params.paint_tree_bitfield)
	{
		last_paint_tree_bitfield = ggtrees_global_params.paint_tree_bitfield;
		GGTrees_LoadTextures(false);
	}
#endif

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

	auto range = wiProfiler::BeginRangeCPU("Max - Tree Update");

	// GUARD: Validate numTreeChunks before processing
	const uint32_t MAX_SAFE_TREE_CHUNKS = 100000;
	if (numTreeChunks > MAX_SAFE_TREE_CHUNKS)
	{
		char buf[256];
		sprintf(buf, "GGTrees_Update GUARD: numTreeChunks=%u exceeds safe limit %u, skipping update\n", numTreeChunks, MAX_SAFE_TREE_CHUNKS);
		OutputDebugStringA(buf);
		wiProfiler::EndRange(range);
		return;
	}

	// GUARD: Validate numTreeTypes before processing
	const uint32_t MAX_SAFE_TREE_TYPES = 256;
	if (numTreeTypes > MAX_SAFE_TREE_TYPES || numTreeTypes == 0)
	{
		char buf[256];
		sprintf(buf, "GGTrees_Update GUARD: numTreeTypes=%u is invalid (max=%u), skipping update\n", numTreeTypes, MAX_SAFE_TREE_TYPES);
		OutputDebugStringA(buf);
		wiProfiler::EndRange(range);
		return;
	}

	if (ggterrain_extra_params.edit_mode == GGTERRAIN_EDIT_TREES && !bImGuiGotFocus)
	{
		wiInput::MouseState mouseState = wiInput::GetMouseState();
		ggtrees_internal_params.mouseLeftState = mouseState.left_button_press;
		ggtrees_internal_params.mouseLeftPressed = (mouseState.left_button_press && !ggtrees_internal_params.prevMouseLeft) ? 1 : 0;
		ggtrees_internal_params.mouseLeftReleased = (!mouseState.left_button_press && ggtrees_internal_params.prevMouseLeft) ? 1 : 0;
		ggtrees_internal_params.prevMouseLeft = mouseState.left_button_press ? 1 : 0;
		ggtrees_internal_params.mouseY = (int)mouseState.position.y;

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

	if (ggterrain_extra_params.iUpdateTrees > 0)
	{
		ggterrain_extra_params.iUpdateTrees--;
		if (ggterrain_extra_params.iUpdateTrees == 0)
		{
			// something changed so adjust tree heights
			if (!GGTrees_UpdateInstances(0)) ggterrain_extra_params.iUpdateTrees = 5;
		}
	}

	// find close trees for shadows only, do normal trees in GGTrees_UpdateFrustumCulling()
	for (uint32_t i = 0; i < numTreeTypes; i++)
	{
		numTreeInstancesHighShadow[i] = 0;
		shadowChunkRuns[i].clear();
	}

	// GUARD: Track total instances processed to detect runaway loops
	uint32_t totalInstancesProcessed = 0;
	const uint32_t MAX_TOTAL_INSTANCES = 500000;
	bool bLoopGuardTriggered = false;

	for (uint32_t i = 0; i < numTreeChunks; i++)
	{
		TreeChunk* pChunk = &pTreeChunks[i];

		// GUARD: Validate chunk pointer
		if (pChunk == nullptr)
		{
			char buf[256];
			sprintf(buf, "GGTrees_Update GUARD: pTreeChunks[%u] is NULL, skipping chunk\n", i);
			OutputDebugStringA(buf);
			continue;
		}

		uint32_t numInstances = pChunk->pInstances.NumItems();

		// GUARD: Validate instance count for this chunk
		const uint32_t MAX_INSTANCES_PER_CHUNK = 50000;
		if (numInstances > MAX_INSTANCES_PER_CHUNK)
		{
			char buf[256];
			sprintf(buf, "GGTrees_Update GUARD: Chunk %u has %u instances (max=%u), skipping chunk\n", i, numInstances, MAX_INSTANCES_PER_CHUNK);
			OutputDebugStringA(buf);
			continue;
		}

		if (numInstances == 0) continue;

		float sqrDist = 0;
		AABB aabb;
		pChunk->GetBounds(&aabb);
		if (camX > aabb._max.x) sqrDist = (camX - aabb._max.x) * (camX - aabb._max.x);
		else if (camX < aabb._min.x) sqrDist = (camX - aabb._min.x) * (camX - aabb._min.x);

		if (camY > aabb._max.y) sqrDist += (camY - aabb._max.y) * (camY - aabb._max.y);
		else if (camY < aabb._min.y) sqrDist += (camY - aabb._min.y) * (camY - aabb._min.y);

		if (camZ > aabb._max.z) sqrDist += (camZ - aabb._max.z) * (camZ - aabb._max.z);
		else if (camZ < aabb._min.z) sqrDist += (camZ - aabb._min.z) * (camZ - aabb._min.z);

		float distLODShadow = ggtrees_global_params.lod_dist_shadow + ggtrees_global_params.lod_transition_shadow * 2 + treeMaxHeight;
		float sqrDistLODShadow = distLODShadow * distLODShadow;

		if (sqrDist > sqrDistLODShadow) continue;

		// check individual trees
		for (uint32_t j = 0; j < numInstances; j++)
		{
			// GUARD: Check total instances processed
			totalInstancesProcessed++;
			if (totalInstancesProcessed > MAX_TOTAL_INSTANCES)
			{
				if (!bLoopGuardTriggered)
				{
					char buf[256];
					sprintf(buf, "GGTrees_Update GUARD: Processed %u instances, exceeds limit %u, aborting loops\n", totalInstancesProcessed, MAX_TOTAL_INSTANCES);
					OutputDebugStringA(buf);
					bLoopGuardTriggered = true;
				}
				break;
			}

			InstanceTree* pInstance = pChunk->pInstances[j];

			// GUARD: Validate instance pointer
			if (pInstance == nullptr)
			{
				continue;
			}

			if (!pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsKilled() || pInstance->IsInvalid()) continue;

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
			float height = g_GGTrees[treeType].height * scale;
			float halfHeight = height / 2.0f;

			XMFLOAT3 pos;
			pos.x = pInstance->x;
			pos.y = pInstance->y + halfHeight;
			pos.z = pInstance->z;

			if (sqrDist <= sqrDistLODShadow)
			{
				// add tree to shadow draw list
				uint32_t index = numTreeInstancesHighShadow[treeType];
				if (index < GGTREES_MAX_HIGH_DETAIL_PER_TYPE)
				{
					// only if fit within dynamic array
					treeInstancesHighShadow[treeType][index].x = pInstance->x;
					treeInstancesHighShadow[treeType][index].y = pInstance->y;
					treeInstancesHighShadow[treeType][index].z = pInstance->z;
					treeInstancesHighShadow[treeType][index].data = pInstance->data;
					numTreeInstancesHighShadow[treeType]++;

					std::vector<ShadowChunkRun>& runs = shadowChunkRuns[treeType];
					if (runs.empty() || runs.back().chunk != i) runs.push_back({ i, index, 1 });
					else runs.back().count++;
				}
			}
		}

		// GUARD: Break outer loop too if guard was triggered
		if (bLoopGuardTriggered) break;
	}

	// DIAGNOSTIC: Log shadow tree counts periodically
	static int frameCounter = 0;
	frameCounter++;
	if (frameCounter % 500 == 0 || bLoopGuardTriggered)
	{
		uint32_t totalShadowTrees = 0;
		for (uint32_t i = 0; i < numTreeTypes; i++)
		{
			totalShadowTrees += numTreeInstancesHighShadow[i];
		}
		char buf[512];
		sprintf(buf, "GGTrees_Update DIAG: frame=%d, numTreeTypes=%u, numTreeChunks=%u, totalInstancesProcessed=%u, totalShadowTrees=%u, lodDistShadow=%.1f\n",
			frameCounter, numTreeTypes, numTreeChunks, totalInstancesProcessed, totalShadowTrees, ggtrees_global_params.lod_dist_shadow);
		OutputDebugStringA(buf);
	}

	for (uint32_t i = 0; i < numTreeTypes; i++)
	{
		if (numTreeInstancesHighShadow[i] > 0)
		{
			GPUBufferDesc bufferDesc = {};
			SubresourceData data = {};
			data.pSysMem = treeInstancesHighShadow[i];
			bufferDesc.ByteWidth = sizeof(InstanceTreeGPU) * numTreeInstancesHighShadow[i];
			bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			wiRenderer::GetDevice()->CreateBuffer(&bufferDesc, &data, &bufferInstancesHighShadow[i]);
		}
	}

	// update shader constants buffer
	float ang = 0.785398f; // (2.0 * pi) / 8.0
	for (int i = 0; i < 8; i++)
	{
		float c = cos(ang * i);
		float s = sin(ang * i);
		treeConstantData.tree_rotMat[i].x = c;
		treeConstantData.tree_rotMat[i].y = -s;
		treeConstantData.tree_rotMat[i].z = s;
		treeConstantData.tree_rotMat[i].w = c;
	}

	wiScene::LightComponent* lightSun = wiScene::GetScene ().lights.GetComponent (g_entitySunLight);

	// GUARD: Validate sun light component
	if (lightSun == nullptr)
	{
		OutputDebugStringA("GGTrees_Update GUARD: lightSun is NULL, skipping shadow rotation calc\n");
		wiProfiler::EndRange(range);
		return;
	}

	float dirX = lightSun->direction.x;
	float dirZ = lightSun->direction.z;

	// GUARD: Prevent division by zero
	float dirMagSq = dirX * dirX + dirZ * dirZ;
	if (dirMagSq < 0.0001f)
	{
		OutputDebugStringA("GGTrees_Update GUARD: Sun direction XZ magnitude near zero, using default\n");
		dirMagSq = 1.0f;
	}
	float invV = 1.0f / sqrt(dirMagSq);

	treeConstantData.tree_rotMatShadow.x = dirZ * invV;
	treeConstantData.tree_rotMatShadow.y = dirX * invV;
	treeConstantData.tree_rotMatShadow.z = -dirX * invV;
	treeConstantData.tree_rotMatShadow.w = dirZ * invV;

	for (uint32_t i = 0; i < numTreeTypes; i++)
	{
		float height = g_GGTrees[i].height;
		treeConstantData.tree_type[i].scaleX = g_GGTrees[i].billboardScaleX * height;
		treeConstantData.tree_type[i].scaleY = g_GGTrees[i].height;
	}

	treeConstantData.tree_playerPos.x = camX;
	treeConstantData.tree_playerPos.y = camY;
	treeConstantData.tree_playerPos.z = camZ;

	treeConstantData.tree_lodDist = ggtrees_global_params.lod_dist;
	treeConstantData.tree_lodDistShadow = ggtrees_global_params.lod_dist_shadow;
	treeConstantData.tree_lodTransition = ggtrees_global_params.lod_transition;
	treeConstantData.tree_lodTransitionShadow = ggtrees_global_params.lod_transition_shadow;

	wiRenderer::GetDevice()->UpdateBuffer(&treeConstantBuffer, &treeConstantData, cmd, sizeof(TreeCB));

	// GG: with no full detail shadows in a cascade, its billboard shadows start at the tree, not at the shadow distance
	TreeCB treeConstantDataNoShadowCut = treeConstantData;
	treeConstantDataNoShadowCut.tree_lodDistShadow = 0;
	treeConstantDataNoShadowCut.tree_lodTransitionShadow = 0;
	wiRenderer::GetDevice()->UpdateBuffer(&treeConstantBufferNoShadowCut, &treeConstantDataNoShadowCut, cmd, sizeof(TreeCB));

	wiProfiler::EndRange(range);
}

/* old version with possible root cause of driver hang due to too many demands on the GPU
void GGTrees_Update( float camX, float camY, float camZ, CommandList cmd, bool bRenderTargetFocus )
{
	if (!ggtrees_initialised) return;
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	if ( !ggtrees_global_params.draw_enabled && ggtrees_global_params.hide_until_update == 0 ) return;

#ifdef ONLYLOADWHENUSED
	if (last_paint_tree_bitfield != ggtrees_global_params.paint_tree_bitfield)
	{
		last_paint_tree_bitfield = ggtrees_global_params.paint_tree_bitfield;
		GGTrees_LoadTextures(false);
	}
#endif

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

		float distLODShadow = ggtrees_global_params.lod_dist_shadow + ggtrees_global_params.lod_transition_shadow*2 + treeMaxHeight;
		float sqrDistLODShadow = distLODShadow * distLODShadow;

		if ( sqrDist > sqrDistLODShadow ) continue;

		// check individual trees
		for( uint32_t j = 0; j < pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pInstance = pChunk->pInstances[ j ];
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsKilled() || pInstance->IsInvalid() ) continue;

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
				if (index < GGTREES_MAX_HIGH_DETAIL_PER_TYPE)
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
			data.pSysMem = treeInstancesHighShadow[ i ];
			bufferDesc.ByteWidth = sizeof(InstanceTreeGPU) * numTreeInstancesHighShadow[ i ];
			bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferInstancesHighShadow[i] );
		}
	}

	// update shader constants buffer
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
	treeConstantData.tree_lodTransition = ggtrees_global_params.lod_transition;
	treeConstantData.tree_lodTransitionShadow = ggtrees_global_params.lod_transition_shadow;

	wiRenderer::GetDevice()->UpdateBuffer( &treeConstantBuffer, &treeConstantData, cmd, sizeof(TreeCB) );

	wiProfiler::EndRange( range );
}
*/

void GGTrees_HideAll()
{
	if (!ggtrees_initialised) return;
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
		realMinX = posX - (sx / 2.0f);
		realMinZ = posZ - (sx / 2.0f);
		realMaxX = posX + (sx / 2.0f);
		realMaxZ = posZ + (sx / 2.0f);
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

bool ggtrees_any_killed = false;

// removes, for the rest of the level, every tree whose trunk (from its base to its top) reaches into the area; across,
// only its base counts, tested as the grass tests a blade's root (GrassInKillBox). type 0 is a box turned by its yaw
// (cos c, sin s) about Y, type 1 a circle of radius halfX reaching halfY above and below. Only the chunks that change
// are rebuilt
void GGTrees_KillArea( int type, float x, float y, float z, float halfX, float halfY, float halfZ, float c, float s )
{
	if (!ggtrees_initialised) return;

	// the area's reach on x and z
	float reachX = halfX;
	float reachZ = halfX;
	if ( type == 0 )
	{
		reachX = fabsf( c ) * halfX + fabsf( s ) * halfZ;
		reachZ = fabsf( s ) * halfX + fabsf( c ) * halfZ;
	}

	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		if ( pChunk->pInstances.NumItems() == 0 ) continue;

		AABB aabb;
		pChunk->GetBounds( &aabb );
		if ( x - reachX > aabb._max.x || z - reachZ > aabb._max.z || x + reachX < aabb._min.x || z + reachZ < aabb._min.z ) continue;

		bool changed = false;
		for( int j = 0; j < (int)pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pTree = pChunk->pInstances[ j ];
			if ( pTree->IsKilled() ) continue;

			float dx = pTree->x - x;
			float dz = pTree->z - z;
			if ( type == 1 )
			{
				if ( dx*dx + dz*dz > halfX*halfX ) continue;
			}
			else
			{
				if ( fabsf( dx * c - dz * s ) > halfX ) continue;
				if ( fabsf( dx * s + dz * c ) > halfZ ) continue;
			}

			// the base sits a little under the ground, more on a slope, so the whole trunk is tested
			float top = pTree->y + g_GGTrees[ pTree->GetType() ].height * pTree->GetScaleFloat();
			if ( pTree->y > y + halfY || top < y - halfY ) continue;

			pTree->SetKilled( 1 );
			changed = true;
		}
		if ( changed )
		{
			pChunk->Update();
			ggtrees_any_killed = true;
		}
	}
}

void GGTrees_KillBox( float x, float y, float z, float halfX, float halfY, float halfZ, float yawDegrees )
{
	float yaw = yawDegrees * 3.14159265f / 180.0f;
	GGTrees_KillArea( 0, x, y, z, fabsf( halfX ), fabsf( halfY ), fabsf( halfZ ), cosf( yaw ), sinf( yaw ) );
}

void GGTrees_KillCircle( float x, float y, float z, float radius )
{
	radius = fabsf( radius );
	GGTrees_KillArea( 1, x, y, z, radius, radius, radius, 1, 0 );
}

void GGTrees_RestoreKilled()
{
	if ( !ggtrees_initialised || !ggtrees_any_killed ) return;
	for( uint32_t i = 0; i < numTreeChunks; i++ )
	{
		TreeChunk* pChunk = &pTreeChunks[ i ];
		bool changed = false;
		for( int j = 0; j < (int)pChunk->pInstances.NumItems(); j++ )
		{
			InstanceTree* pTree = pChunk->pInstances[ j ];
			if ( !pTree->IsKilled() ) continue;
			pTree->SetKilled( 0 );
			changed = true;
		}
		if ( changed ) pChunk->Update();
	}
	ggtrees_any_killed = false;
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::Render()
extern "C" void GGTrees_Draw_Prepass( const Frustum* frustum, int mode, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;

	GraphicsDevice* device = wiRenderer::GetDevice();
	device->EventBegin("GGTrees Prepass Draw", cmd);
	wiProfiler::range_id range;
	if ( mode == 0 ) range = wiProfiler::BeginRangeGPU("Z-Prepass - Trees Low", cmd);
	else range = wiProfiler::BeginRangeGPU("Planar Reflections Z-Prepass - Trees", cmd);
		
	device->BindPipelineState( &psoTreesPrepass, cmd );

	uint32_t bindSlot = 2;
	device->BindConstantBuffer( VS, &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( PS, &treeConstantBuffer, bindSlot, cmd );

	device->BindResource( PS, &texTree, 50, cmd );
	device->BindResource( PS, &texNoise, 51, cmd );
	device->BindResource( PS, &texTreeNormal, 53, cmd );
	device->BindSampler( PS, &samplerBilinearWrap, 0, cmd );
	device->BindSampler( PS, &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( PS, &samplerTrilinearWrap, 2, cmd );

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
		device->BindIndexBuffer( &bufferTreeIndices, INDEXFORMAT_16BIT, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}
	
	wiProfiler::EndRange( range );

	// only draw high detail in non-reflection passes
	if ( mode == 0 )
	{
		// high detail
		range = wiProfiler::BeginRangeGPU("Z-Prepass - Trees High", cmd);

		device->BindPipelineState( &psoTreesHighPrepass, cmd );
		device->BindResource( PS, &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHigh[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferTreeHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHighPrepass, cmd );
		device->BindResource( PS, &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHigh[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferBranchesHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}
			
		wiProfiler::EndRange( range );
	}

	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine wiRenderer::DrawShadowmaps()
// GG: draws a type's full detail shadow trees in the chunks the cascade can see, joining neighbouring runs into one draw
static void GGTrees_DrawShadowRuns( uint32_t type, uint32_t numIndices, const Frustum* frustum, CommandList cmd )
{
	GraphicsDevice* device = wiRenderer::GetDevice();
	uint32_t drawStart = 0, drawCount = 0;
	for( const ShadowChunkRun& run : shadowChunkRuns[ type ] )
	{
		AABB aabb;
		pTreeChunks[ run.chunk ].GetBounds( &aabb );
		if ( !frustum->CheckBoxFast( aabb ) ) continue;
		if ( drawCount > 0 && drawStart + drawCount == run.start )
		{
			drawCount += run.count;
			continue;
		}
		if ( drawCount > 0 ) device->DrawIndexedInstanced( numIndices, drawCount, 0, 0, drawStart, cmd );
		drawStart = run.start;
		drawCount = run.count;
	}
	if ( drawCount > 0 ) device->DrawIndexedInstanced( numIndices, drawCount, 0, 0, drawStart, cmd );
}

extern "C" void GGTrees_Draw_ShadowMap( const Frustum* frustum, int cascade, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;
	if ( !ggtrees_global_params.draw_shadows ) return;

	if ( cascade >= ggtrees_global_params.tree_shadow_range ) return;

	GraphicsDevice* device = wiRenderer::GetDevice();
	device->EventBegin("GGTrees Shadow Draw", cmd);

	device->BindPipelineState( &psoTreesShadow, cmd );

	// GG: without full detail shadows here, nearer trees need their billboard shadows
	const GPUBuffer* pTreeCB = &treeConstantBuffer;
	if ( cascade >= ggtrees_global_params.tree_shadow_range_high ) pTreeCB = &treeConstantBufferNoShadowCut;
	uint32_t bindSlot = 2;
	device->BindConstantBuffer( VS, pTreeCB, bindSlot, cmd );
	device->BindConstantBuffer( PS, pTreeCB, bindSlot, cmd );
	
	device->BindResource( PS, &texTree, 50, cmd );
	device->BindResource( PS, &texNoise, 51, cmd );
	device->BindResource( PS, &texTreeNormal, 53, cmd );
	device->BindSampler( PS, &samplerBilinearWrap, 0, cmd );
	device->BindSampler( PS, &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( PS, &samplerTrilinearWrap, 2, cmd );

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
		device->BindIndexBuffer( &bufferTreeIndices, INDEXFORMAT_16BIT, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}
	
	// high detail
	if ( cascade < ggtrees_global_params.tree_shadow_range_high )
	{
		device->BindPipelineState( &psoTreesHighShadow, cmd );
		device->BindResource( PS, &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHighShadow[ i ] > 0 )
			{
				const GPUBuffer* vbs2[] = { &bufferTreeHighVertices[i], &bufferInstancesHighShadow[i] };
				const uint32_t strides2[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs2, 0, 2, strides2, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				GGTrees_DrawShadowRuns( i, g_GGTrees[ i ].trunk->numIndices, frustum, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHighShadow, cmd );
		device->BindResource( PS, &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHighShadow[ i ] > 0 )
			{
				const GPUBuffer* vbs2[] = { &bufferBranchesHighVertices[i], &bufferInstancesHighShadow[i] };
				const uint32_t strides2[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs2, 0, 2, strides2, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				GGTrees_DrawShadowRuns( i, g_GGTrees[ i ].branches->numIndices, frustum, cmd );
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

	GraphicsDevice* device = wiRenderer::GetDevice();
	device->EventBegin("GGTerrain Draw Env Probe", cmd);
	
	// environment probe doesn't render every frame, but can be expensive when it does
	auto range = wiProfiler::BeginRangeGPU( "Environment Probe - Trees", cmd );

	int bindSlot = 2;
	device->BindConstantBuffer( VS, &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( PS, &treeConstantBuffer, bindSlot, cmd );

	// bind texture and sampler
	device->BindSampler( PS, &samplerBilinearWrap, 0, cmd );
	device->BindSampler( PS, &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( PS, &samplerTrilinearWrap, 2, cmd );

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
			if ( !pInstance->IsVisible() || pInstance->IsFlattened() || pInstance->IsKilled() || pInstance->IsInvalid() ) continue;

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
	device->BindConstantBuffer(PS, &wiRenderer::constantBuffers[CBTYPE_FORWARDENTITYMASK], CB_GETBINDSLOT(ForwardEntityMaskCB), cmd);

	// draw trees
	for( uint32_t i = 0; i < numTreeTypes; i++ )
	{
		if ( numTreeInstancesHighEnvProbe[ i ] == 0 ) continue;
		
		GPUBufferDesc bufferDesc = {};
		SubresourceData data = {};
		data.pSysMem = treeInstancesHighEnvProbe[ i ];
		bufferDesc.ByteWidth = sizeof(InstanceTreeGPU) * numTreeInstancesHighEnvProbe[ i ];
		bufferDesc.BindFlags = BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;
		wiRenderer::GetDevice()->CreateBuffer( &bufferDesc, &data, &bufferInstancesHighEnvProbe[i] );
		
		device->BindPipelineState( &psoTreesHighEnvProbe, cmd );
		device->BindResource( PS, &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHighEnvProbe[i] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferTreeHighVertices[i], &bufferInstancesHighEnvProbe[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHighEnvProbe[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHighEnvProbe, cmd );
		device->BindResource( PS, &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHighEnvProbe[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferBranchesHighVertices[i], &bufferInstancesHighEnvProbe[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHighEnvProbe[i], 0, 0, 0, cmd );
			}
		}
	}

	wiProfiler::EndRange( range );
	
	device->EventEnd(cmd);
}

// must be extern "C" to allow /alternatename linker flag to be set correctly
// called from WickedEngine RenderPath3D::Render()
extern "C" void GGTrees_Draw( const Frustum* frustum, int mode, CommandList cmd )
{
	if (!ggtrees_initialised) return;
	if ( !ggtrees_global_params.draw_enabled ) return;

	GraphicsDevice* device = wiRenderer::GetDevice();
	device->EventBegin("GGTrees Draw", cmd);
	wiProfiler::range_id range;
	if ( mode == 0 ) range = wiProfiler::BeginRangeGPU("Opaque - Trees Low", cmd);
	else range = wiProfiler::BeginRangeGPU("Planar Reflections - Trees", cmd);
		
	device->BindPipelineState( &psoTrees, cmd );

	uint32_t bindSlot = 2;
	device->BindConstantBuffer( VS, &treeConstantBuffer, bindSlot, cmd );
	device->BindConstantBuffer( PS, &treeConstantBuffer, bindSlot, cmd );

	// bind texture and sampler
	device->BindResource( PS, &texTree, 50, cmd ); 
	device->BindResource( PS, &texNoise, 51, cmd );
	device->BindResource( PS, &texTreeNormal, 53, cmd );
	device->BindSampler( PS, &samplerBilinearWrap, 0, cmd );
	device->BindSampler( PS, &samplerTrilinearClamp, 1, cmd );
	device->BindSampler( PS, &samplerTrilinearWrap, 2, cmd );

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
		device->BindIndexBuffer( &bufferTreeIndices, INDEXFORMAT_16BIT, 0, cmd );
		device->DrawIndexedInstanced( 6, pChunk->numValid, 0, 0, 0, cmd );
	}
	
	wiProfiler::EndRange( range );

	// only draw high detail in non-reflection passes
	if ( mode == 0 )
	{
		// high detail
		range = wiProfiler::BeginRangeGPU("Opaque - Trees High", cmd);

		device->BindPipelineState( &psoTreesHigh, cmd );
		device->BindResource( PS, &texTreeHigh, 52, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( numTreeInstancesHigh[i] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferTreeHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferTreeHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].trunk->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}

		device->BindPipelineState( &psoBranchesHigh, cmd );
		device->BindResource( PS, &texBranchesHigh, 54, cmd );
		
		for( uint32_t i = 0; i < numTreeTypes; i++ )
		{
			if ( !g_GGTrees[ i ].branches ) continue; // some trees don't have branches
			if ( numTreeInstancesHigh[ i ] > 0 )
			{
				const GPUBuffer* vbs[] = { &bufferBranchesHighVertices[i], &bufferInstancesHigh[i] };
				const uint32_t strides[] = { sizeof(VertexTreeHigh), sizeof(InstanceTreeGPU) };
				device->BindVertexBuffers( vbs, 0, 2, strides, 0, cmd );
				device->BindIndexBuffer( &bufferBranchesHighIndices[i], INDEXFORMAT_16BIT, 0, cmd );
				device->DrawIndexedInstanced( g_GGTrees[ i ].branches->numIndices, numTreeInstancesHigh[i], 0, 0, 0, cmd );
			}
		}

		wiProfiler::EndRange( range );
	}

	device->EventEnd(cmd);
}

} // namespace GGTrees

#ifdef GGTREES_UNDOREDO
void GGTrees_CreateUndoRedoAction(int type, int eList, bool bUserAction, void* pEventData)
{
	if (!ggtrees_initialised) return;
	// User performed this undo action, so clear the redo stack since it now contains outdated events.
	if (bUserAction == true)
	{
		undosys_clearredostack();
		undosys_terrain_cleardata(g_TerrainRedoMem);
	}

	if (type == eUndoSys_Terrain_MoveTree)
	{
		// Event data needed.
		TreeMoveData moveData;

		if (bUserAction)
		{
			TreeMoveData* pTreeMoveData = (TreeMoveData*)pEventData;
			moveData.x = pTreeMoveData->x;
			moveData.z = pTreeMoveData->z;
			moveData.treeIndex = pTreeMoveData->treeIndex;
			moveData.userMoved = pTreeMoveData->userMoved;
		}
		else
		{
			// Undoing a redo or vice versa - get up-to-date data needed for a TreeMove event.
			sUndoSysEventTreeSingle* pImpendingUndoRedoEvent = (sUndoSysEventTreeSingle*)pEventData;
			moveData.treeIndex = undosys_terrain_treemove_gettreeindex(pImpendingUndoRedoEvent);

			// Use the tree index to get the current state of the tree to create the undo or redo action.
			GGTrees::InstanceTree* pInstance = &GGTrees::pAllTrees[moveData.treeIndex];
			moveData.x = pInstance->x;
			moveData.z = pInstance->z;
			moveData.userMoved = pInstance->IsUserMoved();
		}

		undosys_terrain_treemove(moveData, eList);
	}
	else if (type == eUndoSys_Terrain_ScaleTree)
	{
		TreeScaleData scaleData;
	
		if (bUserAction)
		{
			TreeScaleData* pTreeScaleData = (TreeScaleData*)pEventData;
			scaleData.treeIndex = pTreeScaleData->treeIndex;
			scaleData.userMoved = pTreeScaleData->userMoved;
			scaleData.scale = pTreeScaleData->scale;
		}
		else
		{
			// Undoing a redo or vice versa - get up-to-date data needed for a TreeMove event.
			sUndoSysEventTreeSingle* pImpendingUndoRedoEvent = (sUndoSysEventTreeSingle*)pEventData;
			scaleData.treeIndex = undosys_terrain_treemove_gettreeindex(pImpendingUndoRedoEvent);
	
			// Use the tree index to get the current state of the tree to create the undo or redo action.
			GGTrees::InstanceTree* pInstance = &GGTrees::pAllTrees[scaleData.treeIndex];
			scaleData.userMoved = pInstance->IsUserMoved();
			scaleData.scale = pInstance->GetScale();
		}
	
		undosys_terrain_treescale(scaleData, eList);
	}
	else if (type == eUndoSys_Terrain_AddTree)
	{
		// To undo an AddTree event, we must make a RemoveTree event.
		TreeRemoveData removeData;

		if (bUserAction)
		{
			TreeAddData* pTreeAddData = (TreeAddData*)pEventData;
			removeData.treeIndex = pTreeAddData->treeIndex;
			removeData.x = pTreeAddData->x;
			removeData.z = pTreeAddData->z;
			removeData.scale = pTreeAddData->scale;
			removeData.userMoved = pTreeAddData->userMoved;
		}
		else
		{
			// Undoing a redo or vice versa - get up-to-date data needed for a TreeMove event.
			sUndoSysEventTreeSingle* pImpendingUndoRedoEvent = (sUndoSysEventTreeSingle*)pEventData;
			removeData.treeIndex = undosys_terrain_treemove_gettreeindex(pImpendingUndoRedoEvent);

			// Use the tree index to get the current state of the tree to create the undo or redo action.
			GGTrees::InstanceTree* pInstance = &GGTrees::pAllTrees[removeData.treeIndex];
			removeData.scale = pInstance->GetScale();
			removeData.userMoved = pInstance->IsUserMoved();
			removeData.x = pInstance->x;
			removeData.z = pInstance->z;
		}

		undosys_terrain_treeremove(removeData, eList);
	}
	else if (type == eUndoSys_Terrain_RemoveTree)
	{
		// To undo a RemoveTree event, we must make an AddTree event.
		TreeAddData addData;

		if (bUserAction)
		{
			TreeRemoveData* pTreeRemoveData = (TreeRemoveData*)pEventData;
			addData.treeIndex = pTreeRemoveData->treeIndex;
			addData.scale = pTreeRemoveData->scale;
			addData.x = pTreeRemoveData->x;
			addData.z = pTreeRemoveData->z;
			addData.userMoved = pTreeRemoveData->userMoved;
			addData.type = pTreeRemoveData->type;
		}
		else
		{
			// Undoing a redo or vice versa - get up-to-date data needed for a TreeMove event.
			sUndoSysEventTreeSingle* pImpendingUndoRedoEvent = (sUndoSysEventTreeSingle*)pEventData;
			addData.treeIndex = undosys_terrain_treemove_gettreeindex(pImpendingUndoRedoEvent);
			
			// Use the tree index to get the current state of the tree to create the undo or redo action.
			GGTrees::InstanceTree* pInstance = &GGTrees::pAllTrees[addData.treeIndex];
			addData.x = pInstance->x;
			addData.z = pInstance->z;
			addData.userMoved = pInstance->IsUserMoved();
			addData.scale = pInstance->GetScale();
			addData.type = pInstance->GetType();
		}

		//undosys_terrain_treeremove(removeData, eList);
		undosys_terrain_treeadd(addData, eList);
	}
	else if (type == eUndoSys_Terrain_PaintTree)
	{
		// If we are undoing a redo or vice versa then the terrain snapshot will be out of date.
		if (!bUserAction)
		{
			// If we are undoing a redo or vice versa then the snapshot will be out of date.
			GGTrees::GGTrees_GetSnapshot(g_pTerrainSnapshot);
		}

		undosys_terrain_trees(g_pTerrainSnapshot, eList);
	}
}

void GGTrees_PerformUndoRedoAction(int type, void* pEventData, int eList)
{
	if (!ggtrees_initialised) return;
	if (!pEventData)
		return;

	uint8_t* pAddressOfEventData = nullptr;

	switch (type)
	{
	case eUndoSys_Terrain_MoveTree:
	{
		sUndoSysEventTreeSingle* pEvent = (sUndoSysEventTreeSingle*)pEventData;

		TreeMoveData moveData;
		memcpy(&moveData, pEvent->data, sizeof(TreeMoveData));
		GGTrees::InstanceTree* pInstance = &GGTrees::pAllTrees[moveData.treeIndex];
		GGTrees::GGTrees_SetTreePosition(moveData.treeIndex, moveData.x, moveData.z);
		pInstance->SetUserMoved(moveData.userMoved);

		pAddressOfEventData = pEvent->data;

		delete pEvent;
		pEvent = 0;
		
		break;
	}
	case eUndoSys_Terrain_ScaleTree:
	{
		sUndoSysEventTreeSingle* pEvent = (sUndoSysEventTreeSingle*)pEventData;

		TreeScaleData scaleData;
		memcpy(&scaleData, pEvent->data, sizeof(TreeScaleData));
		GGTrees::InstanceTree* pInstance = &GGTrees::pAllTrees[scaleData.treeIndex];
		pInstance->SetScale(scaleData.scale);
		pInstance->SetUserMoved(scaleData.userMoved);

		pAddressOfEventData = pEvent->data;

		delete pEvent;
		pEvent = 0;

		break;
	}
	case eUndoSys_Terrain_AddTree:
	{
		sUndoSysEventTreeSingle* pEvent = (sUndoSysEventTreeSingle*)pEventData;

		TreeAddData addData;
		memcpy(&addData, pEvent->data, sizeof(TreeAddData));

		if (GGTrees::pInvisibleTrees.NumItems() > 0)
		{
			uint32_t index = GGTrees::pInvisibleTrees.PopItem();

			GGTrees::pAllTrees[index].SetVisible(1);
			GGTrees::pAllTrees[index].SetUserMoved(1);
			GGTrees::pAllTrees[index].SetInvalid(0);
			GGTrees::pAllTrees[index].SetType(addData.type);
			GGTrees::pAllTrees[index].SetScale(addData.scale);
			GGTrees::GGTrees_SetTreePosition(index, addData.x, addData.z);
		}

		pAddressOfEventData = pEvent->data;

		delete pEvent;
		pEvent = 0;

		break;
	}
	case eUndoSys_Terrain_RemoveTree:
	{
		sUndoSysEventTreeSingle* pEvent = (sUndoSysEventTreeSingle*)pEventData; 
		TreeRemoveData removeData;
		memcpy(&removeData, pEvent->data, sizeof(TreeRemoveData));

		if ( GGTrees::pAllTrees[removeData.treeIndex].IsVisible() ) GGTrees::pInvisibleTrees.AddItem(removeData.treeIndex);
		GGTrees::pAllTrees[removeData.treeIndex].SetVisible(0);
		GGTrees::pAllTrees[removeData.treeIndex].SetUserMoved(0);
		GGTrees::TreeChunk* pChunk = GGTrees::GGTrees_GetChunk(GGTrees::pAllTrees[removeData.treeIndex].x, GGTrees::pAllTrees[removeData.treeIndex].z);
		if (pChunk) pChunk->Update();

		pAddressOfEventData = pEvent->data;

		delete pEvent;
		pEvent = 0;

		break;
	}
	case eUndoSys_Terrain_PaintTree:
	{
		sUndoSysEventTrees* pEvent = (sUndoSysEventTrees*)pEventData;

		memcpy(&GGTrees::pAllTrees, pEvent->treeData, sizeof(GGTrees::InstanceTree) * GGTrees::numTotalTrees);
		//memcpy(&GGTrees::pTreeChunks, pEvent->chunkData, sizeof(GGTrees::TreeChunk) * GGTrees::numTreeChunks);
		
		GGTrees::pInvisibleTrees.Clear();
		for (int i = 0; i < GGTrees::numTotalTrees; i++)
		{
			if ( !GGTrees::pAllTrees[i].IsVisible() )
				GGTrees::pInvisibleTrees.AddItem(i);
		}

		pAddressOfEventData = pEvent->treeData;

		delete pEvent;
		pEvent = 0;
	}
	}

	//Mark the memory for this event as unused (0 = undo, 1 = redo)
	if (eList == eUndoSys_UndoList)
		undosys_terrain_setmemoryunusedfrom(pAddressOfEventData, g_TerrainUndoMem);
	else
		undosys_terrain_setmemoryunusedfrom(pAddressOfEventData, g_TerrainRedoMem);
}
#endif