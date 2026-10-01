//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "InputGeom.h"
#include "Sample.h"
#include "Sample_TileMesh.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourDebugDraw.h"
#include "NavMeshTesterTool.h"

// so can get BlockerList structure
#include "DetourCommon.h"

#include "GGThread.h"
using namespace GGThread;

#include <float.h>
#include "miniz.h"

void timestampactivity(int i, char* desc_s);

#ifdef OPTICK_ENABLE
#include "optick.h"
#endif

#ifdef WIN32
#	define snprintf _snprintf
#endif

// Globals to help with nav mesh visualisation
bool g_bRefreshNavMeshDebugObjectWhenFocusChanges = false;
int g_iLastFocusNavMeshVisualAtX = 0;
int g_iLastFocusNavMeshVisualAtZ = 0;
int g_iFocusNavMeshVisualAtX = 0;
int g_iFocusNavMeshVisualAtZ = 0;

// logging place holders
void tileLog( int type, const char* format, ... )
{

}

void tileResetLog()
{

}

void tileDumpLog( const char* format, ... )
{

}

inline unsigned int nextPow2(unsigned int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

inline unsigned int ilog2(unsigned int v)
{
	unsigned int r;
	unsigned int shift;
	r = (v > 0xffff) << 4; v >>= r;
	shift = (v > 0xff) << 3; v >>= shift; r |= shift;
	shift = (v > 0xf) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);
	return r;
}

struct TileWork
{
	uint8_t* pOutData = 0;
	int dataSize = 0;
	uint32_t x = 0;
	uint32_t y = 0;
	bool bChanged = false; // GG whole map bake: its inputs changed, so pOutData is its new data (0 for none)
	bool bUnderwater = false;
	bool bTerrainChanged = false;
	uint64_t hash = 0;
	uint64_t terrainHash = 0;
};

// GG: a 64 bit hash for the whole map bake's change checks: FNV-1a over 32 bit words, finished with a mix
static inline uint64_t GGNavHashWords( uint64_t h, const void* pData, size_t words )
{
	const uint32_t* p = (const uint32_t*)pData;
	for ( size_t i = 0; i < words; i++ ) { h ^= p[i]; h *= 0x100000001b3ULL; }
	return h;
}
static inline uint64_t GGNavHashFinish( uint64_t h )
{
	h ^= h >> 33; h *= 0xff51afd7ed558ccdULL; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL; h ^= h >> 33;
	return h ? h : 1;
}
static const uint64_t GGNAV_HASH_SEED = 0xcbf29ce484222325ULL;

class TileMeshThread : public GGThread
{
protected:
	static Sample_TileMesh* pSample_TileMesh;
	static TileWork* pTiles;
	static volatile uint32_t iNextTile;
	static uint32_t iNumTiles;
	static float fTileSize;
	static const float* bmin;
	static const float* bmax;
	static threadLock lock;
	
	static TileMeshThread* pThreads;
	static uint32_t iNumThreads;

	TileMeshData tempData;

public:

	static void SetWork( Sample_TileMesh* tileMesh, float tileSize, TileWork *data, uint32_t numTiles, const float* pMin = 0, const float* pMax = 0 )
	{
		pSample_TileMesh = tileMesh;
		pTiles = data;
		iNextTile = 0;
		fTileSize = tileSize;
		iNumTiles = numTiles;

		bmin = pMin ? pMin : pSample_TileMesh->getInputGeom()->getNavMeshBoundsMin();
		bmax = pMax ? pMax : pSample_TileMesh->getInputGeom()->getNavMeshBoundsMax();
	}

	static bool AnyRunning()
	{
		for( uint32_t i = 0; i < iNumThreads; i++ ) 
		{
			if ( pThreads[i].IsRunning() ) return true;
		}
		return false;
	}

	static void WaitForAll()
	{
		for( uint32_t i = 0; i < iNumThreads; i++ ) pThreads[i].Join();
	}

	static void StartThreads()
	{
		for( uint32_t i = 0; i < iNumThreads; i++ ) pThreads[i].Start();
	}

	static void SetThreads( uint32_t numThreads )
	{
		if ( numThreads == iNumThreads ) return;
		if ( pThreads ) delete [] pThreads;
		
		pThreads = new TileMeshThread[ numThreads ];
		iNumThreads = numThreads;
	}

	static uint32_t GetProgress()
	{
		// no need for lock here since it is an atomic read
		return (iNextTile * 100) / iNumTiles;
	}

	uint32_t Run( ) 
	{
		// profiling
#ifdef OPTICK_ENABLE
		OPTICK_THREAD("TileMeshThread");
#endif

		while( 1 )
		{
			if ( bTerminate ) return 0;
			
			uint32_t localIndex;
			while( !lock.Acquire() );
			localIndex = iNextTile;
			iNextTile++;
			lock.Release();

			if ( localIndex >= iNumTiles ) return 0;

			TileWork* pWork = &pTiles[ localIndex ];
	
			float tileMin[3];
			tileMin[0] = bmin[0] + pWork->x*fTileSize;
			tileMin[1] = bmin[1];
			tileMin[2] = bmin[2] + pWork->y*fTileSize;
	
			float tileMax[3];
			tileMax[0] = bmin[0] + (pWork->x+1)*fTileSize;
			tileMax[1] = bmax[1];
			tileMax[2] = bmin[2] + (pWork->y+1)*fTileSize;
	
			int dataSize = 0;
			if ( pSample_TileMesh->isBaking() ) pSample_TileMesh->bakeTile( &tempData, pWork, tileMin, tileMax );
			else pWork->pOutData = pSample_TileMesh->buildTileMesh(&tempData, pWork->x, pWork->y, tileMin, tileMax, pWork->dataSize);
			tempData.cleanup();
		}

		return 0;
	}

	TileMeshThread() : GGThread() 
	{
		
	}

	~TileMeshThread() 
	{
		if ( pThreads ) delete [] pThreads;
	}
};

Sample_TileMesh* TileMeshThread::pSample_TileMesh = 0;
TileWork* TileMeshThread::pTiles = 0;
volatile uint32_t TileMeshThread::iNextTile = 0;
uint32_t TileMeshThread::iNumTiles = 0;
float TileMeshThread::fTileSize = 0;
const float* TileMeshThread::bmin = 0;
const float* TileMeshThread::bmax = 0;
threadLock TileMeshThread::lock;
TileMeshThread* TileMeshThread::pThreads = 0;
uint32_t TileMeshThread::iNumThreads = 0;

Sample_TileMesh::Sample_TileMesh() :
	m_buildAll(true),
	m_drawMode(DRAWMODE_NAVMESH),
	m_maxTiles(0),
	m_maxPolysPerTile(0),
	m_tileSize(160)
{
	resetCommonSettings();
}

Sample_TileMesh::~Sample_TileMesh()
{
	cleanup();
	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;
}

void Sample_TileMesh::cleanup()
{
	tempData.cleanup();
}

void Sample_TileMesh::handleSettings()
{
	Sample::handleCommonSettings();
	
	if (m_geom)
	{
		int gw = 0, gh = 0;
		const float* bmin = m_geom->getNavMeshBoundsMin();
		const float* bmax = m_geom->getNavMeshBoundsMax();
		rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
		const int ts = (int)m_tileSize;
		const int tw = (gw + ts-1) / ts;
		const int th = (gh + ts-1) / ts;
		

#ifdef DT_POLYREF64
		// 64-bit poly refs reserve DT_TILE_BITS for the tile index, so room is made for every tile.
		// The 22-bit cap below allows 16384 tiles (about 102400 units square), and buildAllTiles
		// silently dropped every tile past that, leaving holes in the navmesh of larger levels.
		m_maxTiles = rcClamp(tw*th, 1, 1 << DT_TILE_BITS);
		m_maxPolysPerTile = 1 << DT_POLY_BITS;
#else
		// Max tiles and max polys affect how the tile IDs are caculated.
		// There are 22 bits available for identifying a tile and a polygon.
		int tileBits = rcMin((int)ilog2(nextPow2(tw*th)), 14);
		if (tileBits > 14) tileBits = 14;
		int polyBits = 22 - tileBits;
		m_maxTiles = 1 << tileBits;
		m_maxPolysPerTile = 1 << polyBits;
#endif
		char pLog[512];
		sprintf_s(pLog, 512, "Navmesh tiles: geometry %d verts %d tris, bounds x %.0f to %.0f, z %.0f to %.0f, grid %d x %d cells, %d x %d = %d tiles, pool %d",
			m_geom->getMesh() ? m_geom->getMesh()->getVertCount() : 0, m_geom->getMesh() ? m_geom->getMesh()->getTriCount() : 0,
			bmin[0], bmax[0], bmin[2], bmax[2], gw, gh, tw, th, tw*th, m_maxTiles);
		timestampactivity(0, pLog);
	}
	else
	{
		m_maxTiles = 0;
		m_maxPolysPerTile = 0;
	}
}
/*
void Sample_TileMesh::handleTools()
{
	int type = !m_tool ? TOOL_NONE : m_tool->type();

	if (imguiCheck("Test Navmesh", type == TOOL_NAVMESH_TESTER))
	{
		setTool(new NavMeshTesterTool);
	}
	if (imguiCheck("Prune Navmesh", type == TOOL_NAVMESH_PRUNE))
	{
		setTool(new NavMeshPruneTool);
	}
	if (imguiCheck("Create Tiles", type == TOOL_TILE_EDIT))
	{
		setTool(new NavMeshTileTool);
	}
	if (imguiCheck("Create Off-Mesh Links", type == TOOL_OFFMESH_CONNECTION))
	{
		setTool(new OffMeshConnectionTool);
	}
	if (imguiCheck("Create Convex Volumes", type == TOOL_CONVEX_VOLUME))
	{
		setTool(new ConvexVolumeTool);
	}
	if (imguiCheck("Create Crowds", type == TOOL_CROWD))
	{
		setTool(new CrowdTool);
	}
	
	imguiSeparatorLine();

	imguiIndent();

	if (m_tool)
		m_tool->handleMenu();

	imguiUnindent();
}

void Sample_TileMesh::handleDebugMode()
{
	// Check which modes are valid.
	bool valid[MAX_DRAWMODE];
	for (int i = 0; i < MAX_DRAWMODE; ++i)
		valid[i] = false;
	
	if (m_geom)
	{
		valid[DRAWMODE_NAVMESH] = m_navMesh != 0;
		valid[DRAWMODE_NAVMESH_TRANS] = m_navMesh != 0;
		valid[DRAWMODE_NAVMESH_BVTREE] = m_navMesh != 0;
		valid[DRAWMODE_NAVMESH_NODES] = m_navQuery != 0;
		valid[DRAWMODE_NAVMESH_PORTALS] = m_navMesh != 0;
		valid[DRAWMODE_NAVMESH_INVIS] = m_navMesh != 0;
		valid[DRAWMODE_MESH] = true;
		valid[DRAWMODE_VOXELS] = m_solid != 0;
		valid[DRAWMODE_VOXELS_WALKABLE] = m_solid != 0;
		valid[DRAWMODE_COMPACT] = m_chf != 0;
		valid[DRAWMODE_COMPACT_DISTANCE] = m_chf != 0;
		valid[DRAWMODE_COMPACT_REGIONS] = m_chf != 0;
		valid[DRAWMODE_REGION_CONNECTIONS] = m_cset != 0;
		valid[DRAWMODE_RAW_CONTOURS] = m_cset != 0;
		valid[DRAWMODE_BOTH_CONTOURS] = m_cset != 0;
		valid[DRAWMODE_CONTOURS] = m_cset != 0;
		valid[DRAWMODE_POLYMESH] = m_pmesh != 0;
		valid[DRAWMODE_POLYMESH_DETAIL] = m_dmesh != 0;
	}
	
	int unavail = 0;
	for (int i = 0; i < MAX_DRAWMODE; ++i)
		if (!valid[i]) unavail++;
	
	if (unavail == MAX_DRAWMODE)
		return;
	
	imguiLabel("Draw");
	if (imguiCheck("Input Mesh", m_drawMode == DRAWMODE_MESH, valid[DRAWMODE_MESH]))
		m_drawMode = DRAWMODE_MESH;
	if (imguiCheck("Navmesh", m_drawMode == DRAWMODE_NAVMESH, valid[DRAWMODE_NAVMESH]))
		m_drawMode = DRAWMODE_NAVMESH;
	if (imguiCheck("Navmesh Invis", m_drawMode == DRAWMODE_NAVMESH_INVIS, valid[DRAWMODE_NAVMESH_INVIS]))
		m_drawMode = DRAWMODE_NAVMESH_INVIS;
	if (imguiCheck("Navmesh Trans", m_drawMode == DRAWMODE_NAVMESH_TRANS, valid[DRAWMODE_NAVMESH_TRANS]))
		m_drawMode = DRAWMODE_NAVMESH_TRANS;
	if (imguiCheck("Navmesh BVTree", m_drawMode == DRAWMODE_NAVMESH_BVTREE, valid[DRAWMODE_NAVMESH_BVTREE]))
		m_drawMode = DRAWMODE_NAVMESH_BVTREE;
	if (imguiCheck("Navmesh Nodes", m_drawMode == DRAWMODE_NAVMESH_NODES, valid[DRAWMODE_NAVMESH_NODES]))
		m_drawMode = DRAWMODE_NAVMESH_NODES;
	if (imguiCheck("Navmesh Portals", m_drawMode == DRAWMODE_NAVMESH_PORTALS, valid[DRAWMODE_NAVMESH_PORTALS]))
		m_drawMode = DRAWMODE_NAVMESH_PORTALS;
	if (imguiCheck("Voxels", m_drawMode == DRAWMODE_VOXELS, valid[DRAWMODE_VOXELS]))
		m_drawMode = DRAWMODE_VOXELS;
	if (imguiCheck("Walkable Voxels", m_drawMode == DRAWMODE_VOXELS_WALKABLE, valid[DRAWMODE_VOXELS_WALKABLE]))
		m_drawMode = DRAWMODE_VOXELS_WALKABLE;
	if (imguiCheck("Compact", m_drawMode == DRAWMODE_COMPACT, valid[DRAWMODE_COMPACT]))
		m_drawMode = DRAWMODE_COMPACT;
	if (imguiCheck("Compact Distance", m_drawMode == DRAWMODE_COMPACT_DISTANCE, valid[DRAWMODE_COMPACT_DISTANCE]))
		m_drawMode = DRAWMODE_COMPACT_DISTANCE;
	if (imguiCheck("Compact Regions", m_drawMode == DRAWMODE_COMPACT_REGIONS, valid[DRAWMODE_COMPACT_REGIONS]))
		m_drawMode = DRAWMODE_COMPACT_REGIONS;
	if (imguiCheck("Region Connections", m_drawMode == DRAWMODE_REGION_CONNECTIONS, valid[DRAWMODE_REGION_CONNECTIONS]))
		m_drawMode = DRAWMODE_REGION_CONNECTIONS;
	if (imguiCheck("Raw Contours", m_drawMode == DRAWMODE_RAW_CONTOURS, valid[DRAWMODE_RAW_CONTOURS]))
		m_drawMode = DRAWMODE_RAW_CONTOURS;
	if (imguiCheck("Both Contours", m_drawMode == DRAWMODE_BOTH_CONTOURS, valid[DRAWMODE_BOTH_CONTOURS]))
		m_drawMode = DRAWMODE_BOTH_CONTOURS;
	if (imguiCheck("Contours", m_drawMode == DRAWMODE_CONTOURS, valid[DRAWMODE_CONTOURS]))
		m_drawMode = DRAWMODE_CONTOURS;
	if (imguiCheck("Poly Mesh", m_drawMode == DRAWMODE_POLYMESH, valid[DRAWMODE_POLYMESH]))
		m_drawMode = DRAWMODE_POLYMESH;
	if (imguiCheck("Poly Mesh Detail", m_drawMode == DRAWMODE_POLYMESH_DETAIL, valid[DRAWMODE_POLYMESH_DETAIL]))
		m_drawMode = DRAWMODE_POLYMESH_DETAIL;
		
	if (unavail)
	{
		imguiValue("Tick 'Keep Itermediate Results'");
		imguiValue("rebuild some tiles to see");
		imguiValue("more debug mode options.");
	}
}
*/

void Sample_TileMesh::handleRender()
{
	if (!m_geom || !m_geom->getMesh())
		return;
	
	const float texScale = 1.0f / (m_cellSize * 10.0f);
	
	// Draw mesh
	if (m_drawMode != DRAWMODE_NAVMESH_TRANS)
	{
		// Draw mesh
		bool bOverlayReducedGeometryOverRealLevel = false;
		if (bOverlayReducedGeometryOverRealLevel == true )
		{
			m_dd.setDebugObjectSlot(0);
			duDebugDrawTriMeshSlope(&m_dd, m_geom->getMesh()->getVerts(), m_geom->getMesh()->getVertCount(),
									m_geom->getMesh()->getTris(), m_geom->getMesh()->getNormals(), m_geom->getMesh()->getTriCount(),
									m_agentMaxSlope, texScale);
			m_geom->drawOffMeshConnections(&m_dd);
		}
	}
		
	//glDepthMask(GL_FALSE);
	
	// Draw bounds
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	//duDebugDrawBoxWire(&m_dd, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duRGBA(255,255,255,128), 1.0f);
	
	/*
	// Tiling grid.
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
	const int tw = (gw + (int)m_tileSize-1) / (int)m_tileSize;
	const int th = (gh + (int)m_tileSize-1) / (int)m_tileSize;
	const float s = m_tileSize*m_cellSize;
	duDebugDrawGridXZ(&m_dd, bmin[0],bmin[1],bmin[2], tw,th, s, duRGBA(0,0,0,64), 1.0f);
	*/

	// Draw active tile
	//duDebugDrawBoxWire(&m_dd, m_lastBuiltTileBmin[0],m_lastBuiltTileBmin[1],m_lastBuiltTileBmin[2],
	//				   m_lastBuiltTileBmax[0],m_lastBuiltTileBmax[1],m_lastBuiltTileBmax[2], m_tileCol, 1.0f);
		
	if (m_navMesh && m_navQuery &&
		(m_drawMode == DRAWMODE_NAVMESH ||
		 m_drawMode == DRAWMODE_NAVMESH_TRANS ||
		 m_drawMode == DRAWMODE_NAVMESH_BVTREE ||
		 m_drawMode == DRAWMODE_NAVMESH_NODES ||
		 m_drawMode == DRAWMODE_NAVMESH_PORTALS ||
		 m_drawMode == DRAWMODE_NAVMESH_INVIS))
	{
		if (m_drawMode != DRAWMODE_NAVMESH_INVIS)
		{
			// can visualise entire nav mesh by only drawing whereever the camera is focused
			extern float CameraPositionX(int);
			extern float CameraPositionZ(int);
			g_iFocusNavMeshVisualAtX = CameraPositionX(0);
			g_iFocusNavMeshVisualAtZ = CameraPositionZ(0);
			if (abs(g_iFocusNavMeshVisualAtX - g_iLastFocusNavMeshVisualAtX) > 500 || abs(g_iFocusNavMeshVisualAtZ - g_iLastFocusNavMeshVisualAtZ) > 500)
			{
				g_bRefreshNavMeshDebugObjectWhenFocusChanges = true;
				g_iLastFocusNavMeshVisualAtX = g_iFocusNavMeshVisualAtX;
				g_iLastFocusNavMeshVisualAtZ = g_iFocusNavMeshVisualAtZ;
			}
			m_dd.setDebugObjectSlot(1, g_bRefreshNavMeshDebugObjectWhenFocusChanges);
			// GG: drawn only when its debug object is to be made again (when it is current, end() drops the drawing), not
			// every frame
			if (m_dd.isDebugObjectSlotRefreshed(1) == false)
				duDebugDrawNavMeshWithClosedList(&m_dd, *m_navMesh, *m_navQuery, m_navMeshDrawFlags);
			g_bRefreshNavMeshDebugObjectWhenFocusChanges = false;
		}
		if (m_drawMode == DRAWMODE_NAVMESH_BVTREE)
		{
			m_dd.setDebugObjectSlot(2);
			duDebugDrawNavMeshBVTree(&m_dd, *m_navMesh);
		}
		//if (m_drawMode == DRAWMODE_NAVMESH_PORTALS)
		//{
		//	duDebugDrawNavMeshPortals(&m_dd, *m_navMesh);
		//}
		if (m_drawMode == DRAWMODE_NAVMESH_NODES)
		{
			m_dd.setDebugObjectSlot(3);
			duDebugDrawNavMeshNodes(&m_dd, *m_navQuery);
		}
		//Hmm, does not draw correct polygons!!
		//m_dd.setDebugObjectSlot(4);
		//duDebugDrawNavMeshPolysWithFlags(&m_dd, *m_navMesh, SAMPLE_POLYFLAGS_DISABLED, duRGBA(0,0,255,225));
	}

	// draw all blocklist entries for better level crafting (and debugging)
	const float off = 10.5f;
	extern std::vector<sBlocker> g_BlockerList;
	if (g_BlockerList.size() > 0)
	{
		m_dd.setDebugObjectSlot(4);
		m_dd.begin(DU_DRAW_TRIS, 5.0f);
		int iDoorCount = g_BlockerList.size();
		for (int iDoorIndex = 0; iDoorIndex < iDoorCount; iDoorIndex++)
		{
			// simple rectangle when blocking
			if (g_BlockerList[iDoorIndex].bBlocking == true)
			{
				unsigned int blockerColor = duRGBA(255, 255, 255, 128);
				float fMinX = g_BlockerList[iDoorIndex].minX;
				float fMinY = g_BlockerList[iDoorIndex].minY + off;
				float fMinZ = g_BlockerList[iDoorIndex].minZ;
				float fMaxX = g_BlockerList[iDoorIndex].maxX;
				float fMaxY = g_BlockerList[iDoorIndex].maxY + off;
				float fMaxZ = g_BlockerList[iDoorIndex].maxZ;
				float fAngle = g_BlockerList[iDoorIndex].fAngle;
				float fSizeX = fMaxX - fMinX;
				float fSizeZ = fMaxZ - fMinZ;
				float fCenterX = fMinX + (fSizeX / 2.0f);
				float fCenterZ = fMinZ + (fSizeZ / 2.0f);
				float tofromradian = 0.0174533f;
				float fDX = fMinX - fCenterX;
				float fDZ = fMinZ - fCenterZ;
				float fDD = sqrtf(fabs(fDX * fDX) + fabs(fDZ * fDZ));
				float fTLA = atan2(fMinX - fCenterX, fCenterZ - fMinZ) / tofromradian;
				float fTRA = atan2(fMaxX - fCenterX, fCenterZ - fMinZ) / tofromradian;
				float fBLA = atan2(fMinX - fCenterX, fCenterZ - fMaxZ) / tofromradian;
				float fBRA = atan2(fMaxX - fCenterX, fCenterZ - fMaxZ) / tofromradian;
				fTLA = (fTLA + fAngle) * tofromradian;
				fTRA = (fTRA + fAngle) * tofromradian;
				fBLA = (fBLA + fAngle) * tofromradian;
				fBRA = (fBRA + fAngle) * tofromradian;
				float fX1 = fCenterX + (sinf(fTLA) * fDD);
				float fZ1 = fCenterZ + (cosf(fTLA) * fDD);
				float fX2 = fCenterX + (sinf(fTRA) * fDD);
				float fZ2 = fCenterZ + (cosf(fTRA) * fDD);
				float fX3 = fCenterX + (sinf(fBRA) * fDD);
				float fZ3 = fCenterZ + (cosf(fBRA) * fDD);
				float fX4 = fCenterX + (sinf(fBLA) * fDD);
				float fZ4 = fCenterZ + (cosf(fBLA) * fDD);
				m_dd.vertex(fX1, fMinY, fZ1, blockerColor);
				m_dd.vertex(fX2, fMinY, fZ2, blockerColor);
				m_dd.vertex(fX3, fMinY, fZ3, blockerColor);
				m_dd.vertex(fX1, fMinY, fZ1, blockerColor);
				m_dd.vertex(fX3, fMinY, fZ3, blockerColor);
				m_dd.vertex(fX4, fMinY, fZ4, blockerColor);
			}
		}
		m_dd.end();
	}

	// draw all token drops (show in debugging)
	m_dd.setDebugObjectSlot(5);
	m_dd.begin(DU_DRAW_TRIS, 5.0f);
	extern std::vector<sTokenDrop> g_TokenDropList;
	if (g_TokenDropList.size() > 0)
	{
		int iTokenCount = g_TokenDropList.size();
		for (int iTokenIndex = 0; iTokenIndex < iTokenCount; iTokenIndex++)
		{
			// simple rectangle when blocking
			if (g_TokenDropList[iTokenIndex].fTimeLeft > 0.0f)
			{
				int iFadeValue = 128;
				unsigned int tokenColor = duRGBA(0, 255, 128, 128);
				float fCenterX = g_TokenDropList[iTokenIndex].X;
				float fCenterY = g_TokenDropList[iTokenIndex].Y + 0.1f;
				float fCenterZ = g_TokenDropList[iTokenIndex].Z;
				float fDD = 1.5f + (g_TokenDropList[iTokenIndex].fTimeLeft / 1000.0f);
				if (fDD > 10.0f) fDD = 10.0f;
				float fX1 = fCenterX - fDD;
				float fZ1 = fCenterZ - fDD;
				float fX2 = fCenterX + fDD;
				float fZ2 = fCenterZ - fDD;
				float fX3 = fCenterX + fDD;
				float fZ3 = fCenterZ + fDD;
				float fX4 = fCenterX - fDD;
				float fZ4 = fCenterZ + fDD;
				m_dd.vertex(fX1, fCenterY, fZ1, tokenColor);
				m_dd.vertex(fX2, fCenterY, fZ2, tokenColor);
				m_dd.vertex(fX3, fCenterY, fZ3, tokenColor);
				m_dd.vertex(fX1, fCenterY, fZ1, tokenColor);
				m_dd.vertex(fX3, fCenterY, fZ3, tokenColor);
				m_dd.vertex(fX4, fCenterY, fZ4, tokenColor);
			}
		}
	}
	m_dd.end();

	/*
	glDepthMask(GL_TRUE);
	
	if (m_chf && m_drawMode == DRAWMODE_COMPACT)
		duDebugDrawCompactHeightfieldSolid(&m_dd, *m_chf);
	
	if (m_chf && m_drawMode == DRAWMODE_COMPACT_DISTANCE)
		duDebugDrawCompactHeightfieldDistance(&m_dd, *m_chf);
	if (m_chf && m_drawMode == DRAWMODE_COMPACT_REGIONS)
		duDebugDrawCompactHeightfieldRegions(&m_dd, *m_chf);
	if (m_solid && m_drawMode == DRAWMODE_VOXELS)
	{
		glEnable(GL_FOG);
		duDebugDrawHeightfieldSolid(&m_dd, *m_solid);
		glDisable(GL_FOG);
	}
	if (m_solid && m_drawMode == DRAWMODE_VOXELS_WALKABLE)
	{
		glEnable(GL_FOG);
		duDebugDrawHeightfieldWalkable(&m_dd, *m_solid);
		glDisable(GL_FOG);
	}
	
	if (m_cset && m_drawMode == DRAWMODE_RAW_CONTOURS)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawRawContours(&m_dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	
	if (m_cset && m_drawMode == DRAWMODE_BOTH_CONTOURS)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawRawContours(&m_dd, *m_cset, 0.5f);
		duDebugDrawContours(&m_dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_cset && m_drawMode == DRAWMODE_CONTOURS)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawContours(&m_dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_chf && m_cset && m_drawMode == DRAWMODE_REGION_CONNECTIONS)
	{
		duDebugDrawCompactHeightfieldRegions(&m_dd, *m_chf);
		
		glDepthMask(GL_FALSE);
		duDebugDrawRegionConnections(&m_dd, *m_cset);
		glDepthMask(GL_TRUE);
	}
	if (m_pmesh && m_drawMode == DRAWMODE_POLYMESH)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawPolyMesh(&m_dd, *m_pmesh);
		glDepthMask(GL_TRUE);
	}
	if (m_dmesh && m_drawMode == DRAWMODE_POLYMESH_DETAIL)
	{
		glDepthMask(GL_FALSE);
		duDebugDrawPolyMeshDetail(&m_dd, *m_dmesh);
		glDepthMask(GL_TRUE);
	}
		
	m_geom->drawConvexVolumes(&m_dd);
	*/
	
	//if (m_tool)
	//	m_tool->handleRender();
	//renderToolStates();

	//glDepthMask(GL_TRUE);

	m_dd.setDebugObjectSlot(99, true);
}

/*
void Sample_TileMesh::handleRenderOverlay(double* proj, double* model, int* view)
{
	GLdouble x, y, z;
	
	// Draw start and end point labels
	if (m_tileBuildTime > 0.0f && gluProject((GLdouble)(m_lastBuiltTileBmin[0]+m_lastBuiltTileBmax[0])/2, (GLdouble)(m_lastBuiltTileBmin[1]+m_lastBuiltTileBmax[1])/2, (GLdouble)(m_lastBuiltTileBmin[2]+m_lastBuiltTileBmax[2])/2,
											 model, proj, view, &x, &y, &z))
	{
		char text[32];
		snprintf(text,32,"%.3fms / %dTris / %.1fkB", m_tileBuildTime, m_tileTriCount, m_tileMemUsage);
		imguiDrawText((int)x, (int)y-25, IMGUI_ALIGN_CENTER, text, imguiRGBA(0,0,0,220));
	}
	
	if (m_tool)
		m_tool->handleRenderOverlay(proj, model, view);
	renderOverlayToolStates(proj, model, view);
}
*/

void Sample_TileMesh::handleMeshChanged(InputGeom* geom)
{
	Sample::handleMeshChanged(geom);

	//const BuildSettings* buildSettings = geom->getBuildSettings();
	//if (buildSettings && buildSettings->tileSize > 0)
	//	m_tileSize = buildSettings->tileSize;

	cleanup();

	dtFreeNavMesh(m_navMesh);
	m_navMesh = 0;
}

// helper for internal prompts
void printscreenprompt(char*);

bool Sample_TileMesh::handleBuild()
{
	if (!m_geom || !m_geom->getMesh())
	{
		tileLog(RC_LOG_ERROR, "buildTiledNavigation: No vertices and triangles.");
		return false;
	}

	int get_gameisexe(void);
	int isExe = get_gameisexe();
	
	dtFreeNavMesh(m_navMesh);
	
	m_navMesh = dtAllocNavMesh();
	if (!m_navMesh)
	{
		tileLog(RC_LOG_ERROR, "buildTiledNavigation: Could not allocate navmesh.");
		return false;
	}

	dtNavMeshParams params;
	rcVcopy(params.orig, m_geom->getNavMeshBoundsMin());
	params.tileWidth = m_tileSize*m_cellSize;
	params.tileHeight = m_tileSize*m_cellSize;
	params.maxTiles = m_maxTiles;
	params.maxPolys = m_maxPolysPerTile;
	
	dtStatus status;
	
	status = m_navMesh->init(&params);
	if (dtStatusFailed(status))
	{
		tileLog(RC_LOG_ERROR, "buildTiledNavigation: Could not init navmesh.");
		char pLog[256];
		sprintf_s(pLog, 256, "Navmesh init failed (status 0x%x) for a pool of %d tiles", status, m_maxTiles);
		timestampactivity(0, pLog);
		return false;
	}
	
	// search nodes per path query (was 2048). A route that cannot be reached searches until they run out, so this bounds
	// the cost of such a query: on the Hired Gun level 16384 nodes took up to 15 ms. Long routes come back partial and
	// are walked in legs (the game's AI steers 1,500 unit legs), so 4096 is enough
	status = m_navQuery->init(m_navMesh, 4096);
	if (dtStatusFailed(status))
	{
		tileLog(RC_LOG_ERROR, "buildTiledNavigation: Could not init Detour navmesh query");
		return false;
	}
	
	if (m_buildAll)
	{
		buildAllTiles();
	}

	return true;
}

void Sample_TileMesh::collectSettings(BuildSettings& settings)
{
	//Sample::collectSettings(settings);

	settings.tileSize = m_tileSize;
}

void Sample_TileMesh::buildTile(const float* pos)
{
	if (!m_geom) return;
	if (!m_navMesh) return;
		
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	
	const float ts = m_tileSize*m_cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);
	
	float tileMin[3];
	tileMin[0] = bmin[0] + tx*ts;
	tileMin[1] = bmin[1];
	tileMin[2] = bmin[2] + ty*ts;
	
	float tileMax[3];
	tileMax[0] = bmin[0] + (tx+1)*ts;
	tileMax[1] = bmax[1];
	tileMax[2] = bmin[2] + (ty+1)*ts;
	
	tileResetLog();
	
	int dataSize = 0;
	unsigned char* data = buildTileMesh(&tempData, tx, ty, tileMin, tileMax, dataSize);

	// Remove any previous data (navmesh owns and deletes the data).
	m_navMesh->removeTile(m_navMesh->getTileRefAt(tx,ty,0),0,0);

	// Add tile, or leave the location empty.
	if (data)
	{
		// Let the navmesh own the data.
		dtStatus status = m_navMesh->addTile(data,dataSize,DT_TILE_FREE_DATA,0,0);
		if (dtStatusFailed(status))
			dtFree(data);
	}
	
	tileDumpLog("Build Tile (%d,%d):", tx,ty);
}

void Sample_TileMesh::getTilePos(const float* pos, int& tx, int& ty)
{
	if (!m_geom) return;
	
	const float* bmin = m_geom->getNavMeshBoundsMin();
	
	const float ts = m_tileSize*m_cellSize;
	tx = (int)((pos[0] - bmin[0]) / ts);
	ty = (int)((pos[2] - bmin[2]) / ts);
}

void Sample_TileMesh::removeTile(const float* pos)
{
	if (!m_geom) return;
	if (!m_navMesh) return;
	
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();

	const float ts = m_tileSize*m_cellSize;
	const int tx = (int)((pos[0] - bmin[0]) / ts);
	const int ty = (int)((pos[2] - bmin[2]) / ts);
	
	m_navMesh->removeTile(m_navMesh->getTileRefAt(tx,ty,0),0,0);
}

void Sample_TileMesh::buildAllTiles()
{
	if (!m_geom) return;
	if (!m_navMesh) return;
	
	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
	const int ts = (int)m_tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;
	const float tcs = m_tileSize*m_cellSize;

	//Report progress vars / func
	int get_gameisexe(void);
	int isExe = get_gameisexe();
	extern int g_iLastProgressPercentage;
	char pProgressStr[256];
	
	const bool bMultithread = true;
	if ( bMultithread )
	{
		TileWork* pWork = new TileWork[ th*tw ]; 
		for (int y = 0; y < th; ++y)
		{
			for (int x = 0; x < tw; ++x)
			{
				uint32_t index = y * tw + x;
				pWork[ index ].x = x;
				pWork[ index ].y = y;
			}
		}

		SYSTEM_INFO sysinfo;
		GetSystemInfo( &sysinfo );
		uint32_t numThreads = sysinfo.dwNumberOfProcessors;
		if ( numThreads > 3 ) numThreads--;
		TileMeshThread::SetThreads( numThreads );
		TileMeshThread::SetWork( this, tcs, pWork, th*tw );
		TileMeshThread::StartThreads();
		while( TileMeshThread::AnyRunning() )
		{
			int iProgressPercentage = 20 + ((TileMeshThread::GetProgress() * 80) / 100);
			if (isExe == 0 && g_iLastProgressPercentage != iProgressPercentage)
			{
				g_iLastProgressPercentage = iProgressPercentage;
				sprintf_s(pProgressStr, 256, "RASTERIZING NAVIGATION MESH - %d\\100 Complete", iProgressPercentage);
				void printscreenprompt(char*);
				printscreenprompt(pProgressStr);
			}

			Sleep( 10 );
		}
		//TileMeshThread::WaitForAll();

		int iTilesAdded = 0, iTilesFailed = 0;
		dtStatus firstFailStatus = 0;
		for (int i = 0; i < th*tw; i++)
		{
			uint8_t* data = pWork[ i ].pOutData;
			int dataSize = pWork[ i ].dataSize;
			uint32_t x = pWork[ i ].x;
			uint32_t y = pWork[ i ].y;

			if ( data )
			{
				// Remove any previous data (navmesh owns and deletes the data).
				m_navMesh->removeTile( m_navMesh->getTileRefAt(x,y,0), 0, 0 );
				// Let the navmesh own the data.
				dtStatus status = m_navMesh->addTile( data, dataSize, DT_TILE_FREE_DATA, 0, 0 );
				if ( dtStatusFailed(status) )
				{
					dtFree( data );
					if ( iTilesFailed == 0 ) firstFailStatus = status;
					iTilesFailed++;
				}
				else
				{
					iTilesAdded++;
				}
			}
		}
		char pLog[256];
		sprintf_s(pLog, 256, "Navmesh tiles built: %d added, %d failed to add (first status 0x%x), %d had no walkable data", iTilesAdded, iTilesFailed, firstFailStatus, th*tw - iTilesAdded - iTilesFailed);
		timestampactivity(0, pLog);

		delete [] pWork;
	}
	else
	{
		for (int y = 0; y < th; ++y)
		{
			for (int x = 0; x < tw; ++x)
			{
				//report progress
				int index = y * tw + x;
				int iProgressPercentage = 20 + ((index * 80) / (th * tw));
				if (isExe == 0 && g_iLastProgressPercentage != iProgressPercentage)
				{
					g_iLastProgressPercentage = iProgressPercentage;
					sprintf_s(pProgressStr, 256, "RASTERIZING NAVIGATION MESH - %d\\100 Complete", iProgressPercentage);
					void printscreenprompt(char*);
					printscreenprompt(pProgressStr);
				}

				float tileMin[ 3 ];
				tileMin[0] = bmin[0] + x*tcs;
				tileMin[1] = bmin[1];
				tileMin[2] = bmin[2] + y*tcs;
			
				float tileMax[ 3 ];
				tileMax[0] = bmin[0] + (x+1)*tcs;
				tileMax[1] = bmax[1];
				tileMax[2] = bmin[2] + (y+1)*tcs;

				int dataSize = 0;
				unsigned char* data = buildTileMesh(&tempData, x, y, tileMin, tileMax, dataSize);
				if (data)
				{
					// Remove any previous data (navmesh owns and deletes the data).
					m_navMesh->removeTile(m_navMesh->getTileRefAt(x,y,0),0,0);
					// Let the navmesh own the data.
					dtStatus status = m_navMesh->addTile(data,dataSize,DT_TILE_FREE_DATA,0,0);
					if (dtStatusFailed(status))
						dtFree(data);
				}
			}
		}
	}
}

// GG: the whole map bake ------------------------------------------------------------------------------------------------

void Sample_TileMesh::setBakeSettings( const GGNavMeshSettings& settings )
{
	m_vertsPerPoly = (float)rcClamp( settings.vertsPerPoly, 3, 6 );
	m_edgeMaxError = settings.edgeMaxError;
	m_detailSampleDist = settings.detailSampleDist;
	m_detailSampleMaxError = settings.detailSampleMaxError;
	m_bvTreeMinPolys = settings.bvTreeMinPolys;
	// a coarser open cell size only if it tiles the same width
	const float tileWidth = m_tileSize * m_cellSize;
	const float cells = tileWidth / rcMax( settings.openCellSize, 0.001f );
	m_openCellSize = (settings.openCellSize > m_cellSize && fabsf( cells - floorf( cells + 0.5f ) ) < 0.001f) ? settings.openCellSize : m_cellSize;
}

float Sample_TileMesh::bakeBorder() const
{
	const float fine = (ceilf( m_agentRadius / m_cellSize ) + 3) * m_cellSize;
	const float open = (ceilf( m_agentRadius / m_openCellSize ) + 3) * m_openCellSize;
	return rcMax( fine, open );
}

// the settings and the area the tiles depend on: any change and no tile can be kept
uint64_t Sample_TileMesh::wholeMapKey( const GGNavMeshBake* pBake )
{
	const float values[] = { 3.0f /* format: 3, the terrain hashed by its inputs */, m_cellSize, m_cellHeight, m_agentHeight, m_agentRadius, m_agentMaxClimb, m_agentMaxSlope,
		m_openCellSize, (float)m_bvTreeMinPolys,
		m_regionMinSize, m_regionMergeSize, m_edgeMaxLen, m_edgeMaxError, m_vertsPerPoly, m_detailSampleDist, m_detailSampleMaxError,
		(float)m_partitionType, m_tileSize, pBake->sampleSpacing, pBake->bmin[0], pBake->bmin[2], pBake->bmax[0], pBake->bmax[2],
		pBake->waterY, m_filterLowHangingObstacles ? 1.0f : 0.0f, m_filterLedgeSpans ? 1.0f : 0.0f, m_filterWalkableLowHeightSpans ? 1.0f : 0.0f };
	return GGNavHashFinish( GGNavHashWords( GGNAV_HASH_SEED, values, sizeof(values) / 4 ) );
}

// the terrain's height on the grid over the rect, which neighbouring tiles share (the grid is fixed to the world)
void Sample_TileMesh::sampleTerrain( TileMeshData* tempData, const float* bmin, const float* bmax )
{
	const float s = m_pBake->sampleSpacing;
	const int x0 = (int)floorf( bmin[0] / s ), x1 = (int)ceilf( bmax[0] / s );
	const int z0 = (int)floorf( bmin[2] / s ), z1 = (int)ceilf( bmax[2] / s );
	tempData->m_samplesX = x1 - x0 + 1;
	tempData->m_samplesZ = z1 - z0 + 1;
	tempData->m_samplesMinX = x0 * s;
	tempData->m_samplesMinZ = z0 * s;
	tempData->m_heights.resize( tempData->m_samplesX * tempData->m_samplesZ );
	tempData->m_terrainMinY = FLT_MAX;
	tempData->m_terrainMaxY = -FLT_MAX;
	float* pHeight = tempData->m_heights.data();
	for ( int z = z0; z <= z1; z++ )
	{
		for ( int x = x0; x <= x1; x++ )
		{
			float y = 0;
			if ( !m_pBake->pfnHeight || !m_pBake->pfnHeight( x * s, z * s, &y ) || y != y ) y = -FLT_MAX; // no terrain here
			else
			{
				if ( y < tempData->m_terrainMinY ) tempData->m_terrainMinY = y;
				if ( y > tempData->m_terrainMaxY ) tempData->m_terrainMaxY = y;
			}
			*pHeight++ = y;
		}
	}
	tempData->m_hasSamples = true;
}

// the static object triangles and the trees on the rect, combined in any order (an object added elsewhere changes no
// other tile's hash), with their height range
uint64_t Sample_TileMesh::hashTileObjects( int index, const float* bmin, const float* bmax, float* pMinY, float* pMaxY, int* pStatics, int* pTrees )
{
	uint64_t sum = 0;
	int count = 0;
	int trees = 0;
	if ( m_geom && m_geom->getMesh() && m_geom->getChunkyMesh() )
	{
		const float* verts = m_geom->getMesh()->getVerts();
		const rcChunkyTriMesh* chunkyMesh = m_geom->getChunkyMesh();
		float tbmin[2] = { bmin[0], bmin[2] };
		float tbmax[2] = { bmax[0], bmax[2] };
		int* cid = 0;
		const int ncid = rcGetChunksOverlappingRect( chunkyMesh, tbmin, tbmax, &cid );
		for ( int i = 0; i < ncid; i++ )
		{
			const rcChunkyTriMeshNode& node = chunkyMesh->nodes[ cid[i] ];
			const int* ctris = &chunkyMesh->tris[ node.i*3 ];
			for ( int t = 0; t < node.n; t++ )
			{
				const float* v0 = &verts[ ctris[t*3+0]*3 ];
				const float* v1 = &verts[ ctris[t*3+1]*3 ];
				const float* v2 = &verts[ ctris[t*3+2]*3 ];
				if ( rcMax( v0[0], rcMax( v1[0], v2[0] ) ) < bmin[0] || rcMin( v0[0], rcMin( v1[0], v2[0] ) ) > bmax[0] ) continue;
				if ( rcMax( v0[2], rcMax( v1[2], v2[2] ) ) < bmin[2] || rcMin( v0[2], rcMin( v1[2], v2[2] ) ) > bmax[2] ) continue;
				uint64_t h = GGNavHashWords( GGNAV_HASH_SEED, v0, 3 );
				h = GGNavHashWords( h, v1, 3 );
				h = GGNavHashWords( h, v2, 3 );
				sum += GGNavHashFinish( h );
				*pMinY = rcMin( *pMinY, rcMin( v0[1], rcMin( v1[1], v2[1] ) ) );
				*pMaxY = rcMax( *pMaxY, rcMax( v0[1], rcMax( v1[1], v2[1] ) ) );
				count++;
			}
		}
		if ( cid ) delete [] cid;
	}
	if ( index >= 0 && index + 1 < (int)m_treeTileStart.size() )
	{
		for ( uint32_t k = m_treeTileStart[ index ]; k < m_treeTileStart[ index + 1 ]; k++ )
		{
			const float* pTree = &m_pBake->pTrees[ m_treeTileIndex[ k ] * 4 ];
			// not its height, which follows the terrain (hashed apart) and moves with the terrain mesh's detail
			const float treeKey[3] = { pTree[0], pTree[2], pTree[3] };
			sum += GGNavHashFinish( GGNavHashWords( GGNAV_HASH_SEED ^ 0x9e3779b97f4a7c15ULL, treeKey, 3 ) );
			*pMinY = rcMin( *pMinY, pTree[1] - 100.0f );
			*pMaxY = rcMax( *pMaxY, pTree[1] + 100.0f );
			trees++;
		}
	}
	*pStatics = count;
	*pTrees = trees;
	return sum;
}

// the terrain as triangles on its sample grid (the diagonal GGTerrain's own triangle list uses), the tree trunks as boxes
// that nothing stands on, and every surface below the water dropped
void Sample_TileMesh::rasteriseBakeInputs( TileMeshData* tempData, int index, const rcConfig& cfg )
{
	std::vector<float>& tris = tempData->m_bakeTris;
	std::vector<unsigned char>& areas = tempData->m_bakeAreas;
	tris.clear();
	areas.clear();
	const float walkableThr = cosf( cfg.walkableSlopeAngle / 180.0f * RC_PI );

	if ( tempData->m_hasSamples )
	{
		const float s = m_pBake->sampleSpacing;
		const int sx = tempData->m_samplesX;
		const float* H = tempData->m_heights.data();
		for ( int z = 0; z < tempData->m_samplesZ - 1; z++ )
		{
			for ( int x = 0; x < sx - 1; x++ )
			{
				const float h00 = H[ z*sx + x ], h10 = H[ z*sx + x + 1 ];
				const float h01 = H[ (z+1)*sx + x ], h11 = H[ (z+1)*sx + x + 1 ];
				if ( h00 == -FLT_MAX || h10 == -FLT_MAX || h01 == -FLT_MAX || h11 == -FLT_MAX ) continue;
				const float fx = tempData->m_samplesMinX + x * s;
				const float fz = tempData->m_samplesMinZ + z * s;
				const float quad[6][3] = { { fx, h00, fz }, { fx, h01, fz + s }, { fx + s, h10, fz },
										   { fx + s, h10, fz }, { fx, h01, fz + s }, { fx + s, h11, fz + s } };
				for ( int t = 0; t < 2; t++ )
				{
					const float* v0 = quad[t*3+0];
					const float* v1 = quad[t*3+1];
					const float* v2 = quad[t*3+2];
					float e0[3], e1[3], n[3];
					rcVsub( e0, v1, v0 );
					rcVsub( e1, v2, v0 );
					rcVcross( n, e0, e1 );
					const float len = sqrtf( rcVdot( n, n ) );
					areas.push_back( (len > 0 && n[1] / len > walkableThr) ? RC_WALKABLE_AREA : RC_NULL_AREA );
					for ( int v = 0; v < 3; v++ ) { tris.push_back( quad[t*3+v][0] ); tris.push_back( quad[t*3+v][1] ); tris.push_back( quad[t*3+v][2] ); }
				}
			}
		}
	}

	// tree trunks: solid boxes from 100 below the base to 100 above it, as the old navmesh's tree obstacle, or to 100 above
	// the ground at the trunk where the tree is set deeper (bakeTile)
	if ( index >= 0 && index + 1 < (int)m_treeTileStart.size() )
	{
		static const int boxTris[12][3] = { {0,1,2},{0,2,3},{4,6,5},{4,7,6},{0,4,5},{0,5,1},{1,5,6},{1,6,2},{2,6,7},{2,7,3},{3,7,4},{3,4,0} };
		const uint32_t first = m_treeTileStart[ index ];
		for ( uint32_t k = first; k < m_treeTileStart[ index + 1 ]; k++ )
		{
			const float* pTree = &m_pBake->pTrees[ m_treeTileIndex[ k ] * 4 ];
			const float half = rcMax( pTree[3] * 0.5f, 5.0f );
			const float top = (k - first < tempData->m_treeTops.size()) ? tempData->m_treeTops[ k - first ] : pTree[1] + 100.0f;
			const float corners[8][3] = {
				{ pTree[0]-half, pTree[1]-100.0f, pTree[2]-half }, { pTree[0]+half, pTree[1]-100.0f, pTree[2]-half },
				{ pTree[0]+half, pTree[1]-100.0f, pTree[2]+half }, { pTree[0]-half, pTree[1]-100.0f, pTree[2]+half },
				{ pTree[0]-half, top, pTree[2]-half }, { pTree[0]+half, top, pTree[2]-half },
				{ pTree[0]+half, top, pTree[2]+half }, { pTree[0]-half, top, pTree[2]+half } };
			for ( int t = 0; t < 12; t++ )
			{
				areas.push_back( RC_NULL_AREA );
				for ( int v = 0; v < 3; v++ ) { tris.push_back( corners[ boxTris[t][v] ][0] ); tris.push_back( corners[ boxTris[t][v] ][1] ); tris.push_back( corners[ boxTris[t][v] ][2] ); }
			}
		}
	}

	if ( !areas.empty() ) rcRasterizeTriangles( 0, tris.data(), areas.data(), (int)areas.size(), *tempData->m_solid, cfg.walkableClimb );

	// below the water nothing is walkable; a bridge or pier above it keeps its surface
	if ( m_pBake->waterY > -1e29f )
	{
		rcHeightfield& hf = *tempData->m_solid;
		for ( int i = 0; i < hf.width * hf.height; i++ )
		{
			for ( rcSpan* pSpan = hf.spans[ i ]; pSpan; pSpan = pSpan->next )
			{
				if ( hf.bmin[1] + pSpan->smax * hf.ch < m_pBake->waterY ) pSpan->area = RC_NULL_AREA;
			}
		}
	}
}

// one tile: its inputs hashed, and built again only if they changed (called from the build threads)
void Sample_TileMesh::bakeTile( TileMeshData* tempData, TileWork* pWork, const float* tileMin, const float* tileMax )
{
	const int index = pWork->y * m_tilesX + pWork->x;
	const float border = bakeBorder(); // buildTileMesh's cfg.borderSize at either cell size
	const float rmin[3] = { tileMin[0] - border, 0, tileMin[2] - border };
	const float rmax[3] = { tileMax[0] + border, 0, tileMax[2] + border };

	float minY = FLT_MAX, maxY = -FLT_MAX;
	int statics = 0, trees = 0;
	const uint64_t objectsHash = hashTileObjects( index, rmin, rmax, &minY, &maxY, &statics, &trees );
	const int objects = statics + trees;

	// the terrain's hash from its height inputs on the sample grid (those that apply everywhere, and the sculpting and flat
	// areas there), so the terrain is sampled only for a tile to be built; sampling every tile to check it took some 24 s
	// on an 8 km level. Without the inputs, from the samples
	const bool bKnown = m_tilesBuildKey == m_pBake->buildKey && index < (int)m_tileHash.size();
	uint64_t terrainHash;
	if ( m_pBake->pfnTerrainInputs )
	{
		const float s = m_pBake->sampleSpacing;
		const uint64_t terrainParts[2] = { m_pBake->terrainGlobal,
			m_pBake->pfnTerrainInputs( floorf( rmin[0] / s ) * s, floorf( rmin[2] / s ) * s, ceilf( rmax[0] / s ) * s, ceilf( rmax[2] / s ) * s ) };
		terrainHash = GGNavHashFinish( GGNavHashWords( GGNAV_HASH_SEED, terrainParts, 4 ) );
	}
	else
	{
		sampleTerrain( tempData, rmin, rmax );
		const float grid[4] = { tempData->m_samplesMinX, tempData->m_samplesMinZ, (float)tempData->m_samplesX, (float)tempData->m_samplesZ };
		terrainHash = GGNavHashFinish( GGNavHashWords( GGNavHashWords( GGNAV_HASH_SEED, grid, 4 ), tempData->m_heights.data(), tempData->m_heights.size() ) );
	}
	const uint64_t parts[3] = { terrainHash, objectsHash, m_pBake->buildKey };
	const uint64_t hash = GGNavHashFinish( GGNavHashWords( GGNAV_HASH_SEED, parts, 6 ) );
	pWork->hash = hash;
	pWork->terrainHash = terrainHash;
	if ( bKnown && m_tileHash[ index ] == hash ) return; // unchanged: the navmesh keeps the tile it has

	pWork->bChanged = true;
	pWork->bTerrainChanged = !bKnown || m_tileTerrainHash[ index ] != terrainHash;
	if ( !tempData->m_hasSamples ) sampleTerrain( tempData, rmin, rmax );
	if ( objects == 0 && tempData->m_terrainMaxY < m_pBake->waterY )
	{
		pWork->bUnderwater = true;
		return;
	}
	// a tree's trunk box reaches 100 above the ground at its centre, however deep a tree on a slope is set (GGTrees_SlopeSink)
	if ( index >= 0 && index + 1 < (int)m_treeTileStart.size() )
	{
		for ( uint32_t k = m_treeTileStart[ index ]; k < m_treeTileStart[ index + 1 ]; k++ )
		{
			const float* pTree = &m_pBake->pTrees[ m_treeTileIndex[ k ] * 4 ];
			float top = pTree[1] + 100.0f;
			float ground = 0;
			if ( m_pBake->pfnHeight && m_pBake->pfnHeight( pTree[0], pTree[2], &ground ) && ground == ground ) top = rcMax( top, ground + 100.0f );
			tempData->m_treeTops.push_back( top );
			maxY = rcMax( maxY, top );
		}
	}
	minY = rcMin( minY, tempData->m_terrainMinY );
	maxY = rcMax( maxY, tempData->m_terrainMaxY );
	if ( minY > maxY ) return; // nothing at all here
	const float tmin[3] = { tileMin[0], minY, tileMin[2] };
	const float tmax[3] = { tileMax[0], maxY, tileMax[2] };
	tempData->m_cellSize = (statics == 0) ? m_openCellSize : m_cellSize; // the fine cells only where static objects (doorways) need them
	pWork->pOutData = buildTileMesh( tempData, pWork->x, pWork->y, tmin, tmax, pWork->dataSize );
	tempData->m_cellSize = 0;
}

bool Sample_TileMesh::bakeWholeMap( const GGNavMeshBake* pBake, uint64_t objectsHash, GGNavMeshBakeStats* pStats )
{
	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency( &freq );
	QueryPerformanceCounter( &start );

	int gw = 0, gh = 0;
	rcCalcGridSize( pBake->bmin, pBake->bmax, m_cellSize, &gw, &gh );
	const int ts = (int)m_tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;
	const float tcs = m_tileSize * m_cellSize;
	pStats->tilesX = tw;
	pStats->tilesZ = th;

	// the navmesh held (loaded or kept from the last bake) if it was built with these settings over this area, else a new one
	pStats->fresh = !m_navMesh || m_tilesBuildKey != pBake->buildKey || m_tilesX != tw || m_tilesZ != th || (int)m_tileHash.size() != tw*th;
	if ( pStats->fresh )
	{
		dtFreeNavMesh( m_navMesh );
		m_navMesh = dtAllocNavMesh();
		if ( !m_navMesh ) return false;
		dtNavMeshParams params;
		memset( &params, 0, sizeof(params) );
		rcVcopy( params.orig, pBake->bmin );
		params.tileWidth = tcs;
		params.tileHeight = tcs;
		params.maxTiles = tw * th;
		params.maxPolys = 1 << DT_POLY_BITS;
		dtStatus status = m_navMesh->init( &params );
		if ( dtStatusFailed( status ) )
		{
			char pLog[256];
			sprintf_s( pLog, 256, "Navmesh (whole map): init failed (status 0x%x) for %d x %d tiles", status, tw, th );
			timestampactivity( 0, pLog );
			return false;
		}
		m_tileHash.assign( tw * th, 0 );
		m_tileTerrainHash.assign( tw * th, 0 );
		m_tilesBuildKey = 0;
		m_tilesTerrainFingerprint = 0;
		m_tilesX = tw;
		m_tilesZ = th;
	}
	// search nodes per path query, as handleBuild
	if ( dtStatusFailed( m_navQuery->init( m_navMesh, 4096 ) ) ) return false;

	// the trees bucketed by the tiles (with their borders) they overlap
	const float border = bakeBorder();
	m_treeTileStart.assign( tw * th + 1, 0 );
	m_treeTileIndex.clear();
	for ( int pass = 0; pass < 2; pass++ )
	{
		std::vector<uint32_t> fill;
		if ( pass == 1 )
		{
			for ( int i = 0; i < tw * th; i++ ) m_treeTileStart[ i + 1 ] += m_treeTileStart[ i ];
			m_treeTileIndex.resize( m_treeTileStart[ tw * th ] );
			fill.assign( m_treeTileStart.begin(), m_treeTileStart.end() - 1 );
		}
		for ( uint32_t n = 0; n < pBake->numTrees; n++ )
		{
			const float* pTree = &pBake->pTrees[ n * 4 ];
			const float reach = rcMax( pTree[3] * 0.5f, 5.0f ) + border;
			const int x0 = rcMax( 0, (int)floorf( (pTree[0] - reach - pBake->bmin[0]) / tcs ) );
			const int x1 = rcMin( tw - 1, (int)floorf( (pTree[0] + reach - pBake->bmin[0]) / tcs ) );
			const int z0 = rcMax( 0, (int)floorf( (pTree[2] - reach - pBake->bmin[2]) / tcs ) );
			const int z1 = rcMin( th - 1, (int)floorf( (pTree[2] + reach - pBake->bmin[2]) / tcs ) );
			for ( int z = z0; z <= z1; z++ )
			{
				for ( int x = x0; x <= x1; x++ )
				{
					if ( pass == 0 ) m_treeTileStart[ z * tw + x + 1 ]++;
					else m_treeTileIndex[ fill[ z * tw + x ]++ ] = n;
				}
			}
		}
	}

	// every tile, on the build threads
	m_pBake = pBake;
	TileWork* pWork = new TileWork[ tw * th ];
	for ( int z = 0; z < th; z++ )
	{
		for ( int x = 0; x < tw; x++ )
		{
			pWork[ z * tw + x ].x = x;
			pWork[ z * tw + x ].y = z;
		}
	}
	int get_gameisexe(void);
	int isExe = get_gameisexe();
	extern int g_iLastProgressPercentage;
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	uint32_t numThreads = sysinfo.dwNumberOfProcessors;
	if ( numThreads > 3 ) numThreads--;
	TileMeshThread::SetThreads( numThreads );
	TileMeshThread::SetWork( this, tcs, pWork, tw * th, pBake->bmin, pBake->bmax );
	TileMeshThread::StartThreads();
	while ( TileMeshThread::AnyRunning() )
	{
		int iProgressPercentage = 20 + ((TileMeshThread::GetProgress() * 80) / 100);
		if ( isExe == 0 && g_iLastProgressPercentage != iProgressPercentage )
		{
			g_iLastProgressPercentage = iProgressPercentage;
			char pProgressStr[256];
			sprintf_s( pProgressStr, 256, "BUILDING NAVIGATION MESH - %d\\100 Complete", iProgressPercentage );
			printscreenprompt( pProgressStr );
		}
		Sleep( 10 );
	}
	m_pBake = 0;

	// the changed tiles replace the old (Detour links them to their neighbours)
	for ( int i = 0; i < tw * th; i++ )
	{
		TileWork& work = pWork[ i ];
		m_tileTerrainHash[ i ] = work.terrainHash;
		if ( !work.bChanged ) continue;
		pStats->rebuilt++;
		if ( work.bUnderwater ) pStats->underwater++;
		const float centreX = pBake->bmin[0] + (work.x + 0.5f) * tcs;
		const float centreZ = pBake->bmin[2] + (work.y + 0.5f) * tcs;
		if ( work.bTerrainChanged )
		{
			pStats->rebuiltTerrain++;
			if ( pStats->terrainExamples < 3 )
			{
				pStats->terrainExampleX[ pStats->terrainExamples ] = centreX;
				pStats->terrainExampleZ[ pStats->terrainExamples++ ] = centreZ;
			}
		}
		else if ( pStats->objectsExamples < 3 )
		{
			pStats->objectsExampleX[ pStats->objectsExamples ] = centreX;
			pStats->objectsExampleZ[ pStats->objectsExamples++ ] = centreZ;
		}
		dtTileRef ref = m_navMesh->getTileRefAt( work.x, work.y, 0 );
		if ( ref ) m_navMesh->removeTile( ref, 0, 0 );
		if ( work.pOutData )
		{
			if ( dtStatusFailed( m_navMesh->addTile( work.pOutData, work.dataSize, DT_TILE_FREE_DATA, 0, 0 ) ) )
			{
				dtFree( work.pOutData );
				pStats->failed++;
			}
		}
		m_tileHash[ i ] = work.hash;
	}
	delete [] pWork;
	m_treeTileStart.clear();
	m_treeTileIndex.clear();
	m_tilesBuildKey = pBake->buildKey;
	m_tilesTerrainFingerprint = pBake->terrainFingerprint;
	m_tilesObjectsHash = objectsHash;

	// what the navmesh holds now
	const dtNavMesh* pNav = m_navMesh;
	for ( int i = 0; i < pNav->getMaxTiles(); i++ )
	{
		const dtMeshTile* pTile = pNav->getTile( i );
		if ( !pTile || !pTile->header || !pTile->dataSize ) continue;
		pStats->withData++;
		pStats->bytes += pTile->dataSize;
		pStats->polys += pTile->header->polyCount;
		pStats->verts += pTile->header->vertCount;
		pStats->detailVerts += pTile->header->detailVertCount;
		pStats->detailTris += pTile->header->detailTriCount;
		pStats->linkBytes += (uint64_t)pTile->header->maxLinkCount * sizeof(dtLink);
		pStats->bvBytes += (uint64_t)pTile->header->bvNodeCount * sizeof(dtBVNode);
		if ( pTile->header->bvQuantFactor < 0.999f / m_cellSize ) pStats->openTiles++;
	}
	QueryPerformanceCounter( &end );
	pStats->milliseconds = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
	return true;
}

// the saved navmesh: a header, then one miniz stream of every tile's input and terrain hashes and each tile with data
struct GGNavMeshFileHeader
{
	uint32_t magic; // 'GGNV'
	uint32_t version;
	uint64_t buildKey;
	uint64_t terrainFingerprint;
	uint64_t objectsHash;
	dtNavMeshParams params;
	int32_t tilesX, tilesZ;
	uint32_t tileCount;
	uint32_t reserved;
	uint64_t rawSize;
	uint64_t compressedSize;
};
static const uint32_t GGNAV_FILE_MAGIC = 'G' | ('G' << 8) | ('N' << 16) | ('V' << 24);
static const uint32_t GGNAV_FILE_VERSION = 1;

uint64_t Sample_TileMesh::saveWholeMap( const char* pPath )
{
	if ( !m_navMesh || !m_tilesBuildKey ) return 0;
	const dtNavMesh* pNav = m_navMesh;
	const int numTiles = m_tilesX * m_tilesZ;
	uint64_t rawSize = (uint64_t)numTiles * 16;
	uint32_t tileCount = 0;
	for ( int i = 0; i < pNav->getMaxTiles(); i++ )
	{
		const dtMeshTile* pTile = pNav->getTile( i );
		if ( !pTile || !pTile->header || !pTile->dataSize ) continue;
		rawSize += 12 + pTile->dataSize;
		tileCount++;
	}
	std::vector<unsigned char> raw( (size_t)rawSize );
	unsigned char* p = raw.data();
	memcpy( p, m_tileHash.data(), numTiles * 8 ); p += numTiles * 8;
	memcpy( p, m_tileTerrainHash.data(), numTiles * 8 ); p += numTiles * 8;
	for ( int i = 0; i < pNav->getMaxTiles(); i++ )
	{
		const dtMeshTile* pTile = pNav->getTile( i );
		if ( !pTile || !pTile->header || !pTile->dataSize ) continue;
		const int32_t rec[3] = { pTile->header->x, pTile->header->y, pTile->dataSize };
		memcpy( p, rec, 12 ); p += 12;
		memcpy( p, pTile->data, pTile->dataSize );
		// the polygon links are rebuilt when the tile is added (dtNavMesh::addTile), so they go as zeros, which compress away
		const int linksOffset = dtAlign4( sizeof(dtMeshHeader) ) + dtAlign4( sizeof(float) * 3 * pTile->header->vertCount ) + dtAlign4( sizeof(dtPoly) * pTile->header->polyCount );
		const int linksSize = dtAlign4( sizeof(dtLink) * pTile->header->maxLinkCount );
		if ( linksOffset + linksSize <= pTile->dataSize ) memset( p + linksOffset, 0, linksSize );
		p += pTile->dataSize;
	}

	mz_ulong compressedSize = mz_compressBound( (mz_ulong)rawSize );
	std::vector<unsigned char> compressed( compressedSize );
	if ( mz_compress2( compressed.data(), &compressedSize, raw.data(), (mz_ulong)rawSize, MZ_BEST_SPEED ) != MZ_OK ) return 0;

	GGNavMeshFileHeader header;
	memset( &header, 0, sizeof(header) );
	header.magic = GGNAV_FILE_MAGIC;
	header.version = GGNAV_FILE_VERSION;
	header.buildKey = m_tilesBuildKey;
	header.terrainFingerprint = m_tilesTerrainFingerprint;
	header.objectsHash = m_tilesObjectsHash;
	header.params = *pNav->getParams();
	header.tilesX = m_tilesX;
	header.tilesZ = m_tilesZ;
	header.tileCount = tileCount;
	header.rawSize = rawSize;
	header.compressedSize = compressedSize;

	// written beside and then moved over the old file, so a failed save leaves the old one
	char pTemp[MAX_PATH];
	sprintf_s( pTemp, MAX_PATH, "%s.tmp", pPath );
	FILE* fp = 0;
	if ( fopen_s( &fp, pTemp, "wb" ) != 0 || !fp ) return 0;
	bool bOk = fwrite( &header, sizeof(header), 1, fp ) == 1 && fwrite( compressed.data(), 1, compressedSize, fp ) == compressedSize;
	fclose( fp );
	if ( !bOk || !MoveFileExA( pTemp, pPath, MOVEFILE_REPLACE_EXISTING ) )
	{
		DeleteFileA( pTemp );
		return 0;
	}
	return sizeof(header) + compressedSize;
}

int Sample_TileMesh::loadWholeMap( const char* pPath, uint64_t buildKey )
{
	FILE* fp = 0;
	if ( fopen_s( &fp, pPath, "rb" ) != 0 || !fp ) return -1;
	GGNavMeshFileHeader header;
	bool bOk = fread( &header, sizeof(header), 1, fp ) == 1 && header.magic == GGNAV_FILE_MAGIC && header.version == GGNAV_FILE_VERSION && header.buildKey == buildKey;
	std::vector<unsigned char> compressed;
	if ( bOk )
	{
		compressed.resize( (size_t)header.compressedSize );
		bOk = fread( compressed.data(), 1, compressed.size(), fp ) == compressed.size();
	}
	fclose( fp );
	if ( !bOk ) return -1;

	const int numTiles = header.tilesX * header.tilesZ;
	std::vector<unsigned char> raw( (size_t)header.rawSize );
	mz_ulong rawSize = (mz_ulong)header.rawSize;
	if ( header.rawSize < (uint64_t)numTiles * 16 || mz_uncompress( raw.data(), &rawSize, compressed.data(), (mz_ulong)compressed.size() ) != MZ_OK || rawSize != header.rawSize ) return -1;
	compressed.clear();
	compressed.shrink_to_fit();

	m_tilesBuildKey = 0; // nothing held counts as built until the load has finished
	dtFreeNavMesh( m_navMesh );
	m_navMesh = dtAllocNavMesh();
	if ( !m_navMesh || dtStatusFailed( m_navMesh->init( &header.params ) ) ) return -1;
	const unsigned char* p = raw.data();
	m_tileHash.assign( (const uint64_t*)p, (const uint64_t*)p + numTiles ); p += numTiles * 8;
	m_tileTerrainHash.assign( (const uint64_t*)p, (const uint64_t*)p + numTiles ); p += numTiles * 8;
	const unsigned char* pEnd = raw.data() + raw.size();
	int loaded = 0;
	for ( uint32_t t = 0; t < header.tileCount && p + 12 <= pEnd; t++ )
	{
		int32_t rec[3];
		memcpy( rec, p, 12 ); p += 12;
		if ( rec[2] <= 0 || p + rec[2] > pEnd ) break;
		unsigned char* pData = (unsigned char*)dtAlloc( rec[2], DT_ALLOC_PERM );
		memcpy( pData, p, rec[2] ); p += rec[2];
		if ( dtStatusFailed( m_navMesh->addTile( pData, rec[2], DT_TILE_FREE_DATA, 0, 0 ) ) ) dtFree( pData );
		else loaded++;
	}
	m_tilesBuildKey = header.buildKey;
	m_tilesTerrainFingerprint = header.terrainFingerprint;
	m_tilesObjectsHash = header.objectsHash;
	m_tilesX = header.tilesX;
	m_tilesZ = header.tilesZ;
	if ( dtStatusFailed( m_navQuery->init( m_navMesh, 4096 ) ) ) return -1;
	return loaded;
}

void Sample_TileMesh::removeAllTiles()
{
	if (!m_geom || !m_navMesh)
		return;

	const float* bmin = m_geom->getNavMeshBoundsMin();
	const float* bmax = m_geom->getNavMeshBoundsMax();
	int gw = 0, gh = 0;
	rcCalcGridSize(bmin, bmax, m_cellSize, &gw, &gh);
	const int ts = (int)m_tileSize;
	const int tw = (gw + ts-1) / ts;
	const int th = (gh + ts-1) / ts;
	
	for (int y = 0; y < th; ++y)
		for (int x = 0; x < tw; ++x)
			m_navMesh->removeTile(m_navMesh->getTileRefAt(x,y,0),0,0);
}


unsigned char* Sample_TileMesh::buildTileMesh(TileMeshData* tempData, const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize)
{
	// GG: the whole map bake builds a tile from the terrain and trees too, so it needs no static objects
	const bool bHasStatics = m_geom && m_geom->getMesh() && m_geom->getChunkyMesh();
	if (!bHasStatics && !m_pBake)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Input mesh is not specified.");
		return 0;
	}
	
	cleanup();

	const float* verts = bHasStatics ? m_geom->getMesh()->getVerts() : 0;
	const int nverts = bHasStatics ? m_geom->getMesh()->getVertCount() : 0;
	const int ntris = bHasStatics ? m_geom->getMesh()->getTriCount() : 0;
	const rcChunkyTriMesh* chunkyMesh = bHasStatics ? m_geom->getChunkyMesh() : 0;

	rcConfig& cfg = tempData->m_cfg;

	// GG: a whole map tile with no static objects can be built at a coarser cell size (m_openCellSize); the tile keeps its
	// size in the world, and the region areas and height detail keep theirs
	const float cs = (tempData->m_cellSize > 0) ? tempData->m_cellSize : m_cellSize;
	const float scale = m_cellSize / cs;

	// Init build configuration from GUI
	memset(&cfg, 0, sizeof(cfg));
	cfg.cs = cs;
	cfg.ch = m_cellHeight;
	cfg.walkableSlopeAngle = m_agentMaxSlope;
	cfg.walkableHeight = (int)ceilf(m_agentHeight / cfg.ch);
	cfg.walkableClimb = (int)floorf(m_agentMaxClimb / cfg.ch);
	cfg.walkableRadius = (int)ceilf(m_agentRadius / cfg.cs);
	cfg.maxEdgeLen = (int)(m_edgeMaxLen / cs);
	cfg.maxSimplificationError = m_edgeMaxError;
	cfg.minRegionArea = (int)rcSqr(m_regionMinSize * scale);		// Note: area = size*size
	cfg.mergeRegionArea = (int)rcSqr(m_regionMergeSize * scale);	// Note: area = size*size
	cfg.maxVertsPerPoly = (int)m_vertsPerPoly;
	cfg.tileSize = (int)(m_tileSize * scale + 0.5f);
	cfg.borderSize = cfg.walkableRadius + 3; // Reserve enough padding.
	cfg.width = cfg.tileSize + cfg.borderSize*2;
	cfg.height = cfg.tileSize + cfg.borderSize*2;
	cfg.detailSampleDist = m_detailSampleDist < 0.9f ? 0 : m_cellSize * m_detailSampleDist; // GG: in world units, whatever the tile's cell size
	cfg.detailSampleMaxError = m_cellHeight * m_detailSampleMaxError;
	
	// Expand the heighfield bounding box by border size to find the extents of geometry we need to build this tile.
	//
	// This is done in order to make sure that the navmesh tiles connect correctly at the borders,
	// and the obstacles close to the border work correctly with the dilation process.
	// No polygons (or contours) will be created on the border area.
	//
	// IMPORTANT!
	//
	//   :''''''''':
	//   : +-----+ :
	//   : |     | :
	//   : |     |<--- tile to build
	//   : |     | :  
	//   : +-----+ :<-- geometry needed
	//   :.........:
	//
	// You should use this bounding box to query your input geometry.
	//
	// For example if you build a navmesh for terrain, and want the navmesh tiles to match the terrain tile size
	// you will need to pass in data from neighbour terrain tiles too! In a simple case, just pass in all the 8 neighbours,
	// or use the bounding box below to only pass in a sliver of each of the 8 neighbours.
	rcVcopy(cfg.bmin, bmin);
	rcVcopy(cfg.bmax, bmax);
	cfg.bmin[0] -= cfg.borderSize*cfg.cs;
	cfg.bmin[2] -= cfg.borderSize*cfg.cs;
	cfg.bmax[0] += cfg.borderSize*cfg.cs;
	cfg.bmax[2] += cfg.borderSize*cfg.cs;
	if (m_pBake)
	{
		// GG: the tile's own height range (bmin and bmax carry it), with room for an agent above the highest surface
		cfg.bmin[1] -= cfg.ch * 2;
		cfg.bmax[1] += m_agentHeight + cfg.ch * 2;
	}
	
	tileLog(RC_LOG_PROGRESS, "Building navigation:");
	tileLog(RC_LOG_PROGRESS, " - %d x %d cells", cfg.width, cfg.height);
	tileLog(RC_LOG_PROGRESS, " - %.1fK verts, %.1fK tris", nverts/1000.0f, ntris/1000.0f);
	
	// Allocate voxel heightfield where we rasterize our input data to.
	tempData->m_solid = rcAllocHeightfield();
	if (!tempData->m_solid)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Out of memory 'solid'.");
		return 0;
	}
	if (!rcCreateHeightfield(0, *tempData->m_solid, cfg.width, cfg.height, cfg.bmin, cfg.bmax, cfg.cs, cfg.ch))
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
		return 0;
	}
	
	if (bHasStatics)
	{
	// Allocate array that can hold triangle flags.
	// If you have multiple meshes you need to process, allocate
	// and array which can hold the max number of triangles you need to process.
	tempData->m_triareas = new unsigned char[chunkyMesh->maxTrisPerChunk];
	if (!tempData->m_triareas)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Out of memory 'm_triareas' (%d).", chunkyMesh->maxTrisPerChunk);
		return 0;
	}
	
	float tbmin[2], tbmax[2];
	tbmin[0] = cfg.bmin[0];
	tbmin[1] = cfg.bmin[2];
	tbmax[0] = cfg.bmax[0];
	tbmax[1] = cfg.bmax[2];
	int* cid = 0;
	const int ncid = rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, &cid);
	if ( !cid ) return 0;
	else if ( !ncid && !m_pBake )
	{
		delete [] cid;
		return 0;
	}

	for (int i = 0; i < ncid; ++i)
	{
		const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
		const int* ctris = &chunkyMesh->tris[node.i*3];
		const int nctris = node.n;
		
		memset(tempData->m_triareas, 0, nctris*sizeof(unsigned char));
		rcMarkWalkableTriangles(0, cfg.walkableSlopeAngle,
								verts, nverts, ctris, nctris, tempData->m_triareas);
		
		if (!rcRasterizeTriangles(0, verts, nverts, ctris, tempData->m_triareas, nctris, *tempData->m_solid, cfg.walkableClimb))
		{
			delete [] cid;
			return 0;
		}
	}

	delete [] cid;
	
	delete [] tempData->m_triareas;
	tempData->m_triareas = 0;
	}

	// GG: the whole map bake's terrain and trees, and its water
	if (m_pBake) rasteriseBakeInputs(tempData, ty * m_tilesX + tx, cfg);
		
	// Once all geometry is rasterized, we do initial pass of filtering to
	// remove unwanted overhangs caused by the conservative rasterization
	// as well as filter spans where the character cannot possibly stand.
	if (m_filterLowHangingObstacles)
		rcFilterLowHangingWalkableObstacles(0, cfg.walkableClimb, *tempData->m_solid);
	if (m_filterLedgeSpans)
		rcFilterLedgeSpans(0, cfg.walkableHeight, cfg.walkableClimb, *tempData->m_solid);
	if (m_filterWalkableLowHeightSpans)
		rcFilterWalkableLowHeightSpans(0, cfg.walkableHeight, *tempData->m_solid);
	
	// Compact the heightfield so that it is faster to handle from now on.
	// This will result more cache coherent data as well as the neighbours
	// between walkable cells will be calculated.
	tempData->m_chf = rcAllocCompactHeightfield();
	if (!tempData->m_chf)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Out of memory 'chf'.");
		return 0;
	}
	if (!rcBuildCompactHeightfield(0, cfg.walkableHeight, cfg.walkableClimb, *tempData->m_solid, *tempData->m_chf))
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
		return 0;
	}
	
	rcFreeHeightField(tempData->m_solid);
	tempData->m_solid = 0;
	
	// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(0, cfg.walkableRadius, *tempData->m_chf))
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Could not erode.");
		return 0;
	}

	// (Optional) Mark areas.
	const ConvexVolume* vols = m_geom->getConvexVolumes();
	for (int i  = 0; i < m_geom->getConvexVolumeCount(); ++i)
		rcMarkConvexPolyArea(0, vols[i].verts, vols[i].nverts, vols[i].hmin, vols[i].hmax, (unsigned char)vols[i].area, *tempData->m_chf);
	
	
	// Partition the heightfield so that we can use simple algorithm later to triangulate the walkable areas.
	// There are 3 martitioning methods, each with some pros and cons:
	// 1) Watershed partitioning
	//   - the classic Recast partitioning
	//   - creates the nicest tessellation
	//   - usually slowest
	//   - partitions the heightfield into nice regions without holes or overlaps
	//   - the are some corner cases where this method creates produces holes and overlaps
	//      - holes may appear when a small obstacles is close to large open area (triangulation can handle this)
	//      - overlaps may occur if you have narrow spiral corridors (i.e stairs), this make triangulation to fail
	//   * generally the best choice if you precompute the nacmesh, use this if you have large open areas
	// 2) Monotone partioning
	//   - fastest
	//   - partitions the heightfield into regions without holes and overlaps (guaranteed)
	//   - creates long thin polygons, which sometimes causes paths with detours
	//   * use this if you want fast navmesh generation
	// 3) Layer partitoining
	//   - quite fast
	//   - partitions the heighfield into non-overlapping regions
	//   - relies on the triangulation code to cope with holes (thus slower than monotone partitioning)
	//   - produces better triangles than monotone partitioning
	//   - does not have the corner cases of watershed partitioning
	//   - can be slow and create a bit ugly tessellation (still better than monotone)
	//     if you have large open areas with small obstacles (not a problem if you use tiles)
	//   * good choice to use for tiled navmesh with medium and small sized tiles
	
	if (m_partitionType == SAMPLE_PARTITION_WATERSHED)
	{
		// Prepare for region partitioning, by calculating distance field along the walkable surface.
		if (!rcBuildDistanceField(0, *tempData->m_chf))
		{
			tileLog(RC_LOG_ERROR, "buildNavigation: Could not build distance field.");
			return 0;
		}
		
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildRegions(0, *tempData->m_chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			tileLog(RC_LOG_ERROR, "buildNavigation: Could not build watershed regions.");
			return 0;
		}
	}
	else if (m_partitionType == SAMPLE_PARTITION_MONOTONE)
	{
		// Partition the walkable surface into simple regions without holes.
		// Monotone partitioning does not need distancefield.
		if (!rcBuildRegionsMonotone(0, *tempData->m_chf, cfg.borderSize, cfg.minRegionArea, cfg.mergeRegionArea))
		{
			tileLog(RC_LOG_ERROR, "buildNavigation: Could not build monotone regions.");
			return 0;
		}
	}
	else // SAMPLE_PARTITION_LAYERS
	{
		// Partition the walkable surface into simple regions without holes.
		if (!rcBuildLayerRegions(0, *tempData->m_chf, cfg.borderSize, cfg.minRegionArea))
		{
			tileLog(RC_LOG_ERROR, "buildNavigation: Could not build layer regions.");
			return 0;
		}
	}
	 	
	// Create contours.
	tempData->m_cset = rcAllocContourSet();
	if (!tempData->m_cset)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Out of memory 'cset'.");
		return 0;
	}
	if (!rcBuildContours(0, *tempData->m_chf, cfg.maxSimplificationError, cfg.maxEdgeLen, *tempData->m_cset))
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Could not create contours.");
		return 0;
	}
	
	if (tempData->m_cset->nconts == 0)
	{
		return 0;
	}
	
	// Build polygon navmesh from the contours.
	tempData->m_pmesh = rcAllocPolyMesh();
	rcPolyMesh* pmesh = tempData->m_pmesh;
	if (!pmesh)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Out of memory 'pmesh'.");
		return 0;
	}
	if (!rcBuildPolyMesh(0, *tempData->m_cset, cfg.maxVertsPerPoly, *pmesh))
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Could not triangulate contours.");
		return 0;
	}
	
	// Build detail mesh.
	tempData->m_dmesh = rcAllocPolyMeshDetail();
	if (!tempData->m_dmesh)
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Out of memory 'dmesh'.");
		return 0;
	}
	
	if (!rcBuildPolyMeshDetail(0, *pmesh, *tempData->m_chf,
							   cfg.detailSampleDist, cfg.detailSampleMaxError,
							   *tempData->m_dmesh))
	{
		tileLog(RC_LOG_ERROR, "buildNavigation: Could build polymesh detail.");
		return 0;
	}
	
	rcFreeCompactHeightfield( tempData->m_chf );
	tempData->m_chf = 0;
	rcFreeContourSet( tempData->m_cset );
	tempData->m_cset = 0;
	
	unsigned char* navData = 0;
	int navDataSize = 0;
	if (cfg.maxVertsPerPoly <= DT_VERTS_PER_POLYGON)
	{
		if (pmesh->nverts >= 0xffff)
		{
			// The vertex indices are ushorts, and cannot point to more than 0xffff vertices.
			tileLog(RC_LOG_ERROR, "Too many vertices per tile %d (max: %d).", pmesh->nverts, 0xffff);
			return 0;
		}
		
		// Update poly flags from areas.
		for (int i = 0; i < pmesh->npolys; ++i)
		{
			if (pmesh->areas[i] == RC_WALKABLE_AREA)
				pmesh->areas[i] = SAMPLE_POLYAREA_GROUND;
			
			if (pmesh->areas[i] == SAMPLE_POLYAREA_GROUND ||
				pmesh->areas[i] == SAMPLE_POLYAREA_GRASS ||
				pmesh->areas[i] == SAMPLE_POLYAREA_ROAD)
			{
				pmesh->flags[i] = SAMPLE_POLYFLAGS_WALK;
			}
			else if (pmesh->areas[i] == SAMPLE_POLYAREA_WATER)
			{
				pmesh->flags[i] = SAMPLE_POLYFLAGS_SWIM;
			}
			else if (pmesh->areas[i] == SAMPLE_POLYAREA_DOOR)
			{
				pmesh->flags[i] = SAMPLE_POLYFLAGS_WALK | SAMPLE_POLYFLAGS_DOOR;
			}
		}
		
		dtNavMeshCreateParams params;
		memset(&params, 0, sizeof(params));
		params.verts = pmesh->verts;
		params.vertCount = pmesh->nverts;
		params.polys = pmesh->polys;
		params.polyAreas = pmesh->areas;
		params.polyFlags = pmesh->flags;
		params.polyCount = pmesh->npolys;
		params.nvp = pmesh->nvp;
		params.detailMeshes = tempData->m_dmesh->meshes;
		params.detailVerts = tempData->m_dmesh->verts;
		params.detailVertsCount = tempData->m_dmesh->nverts;
		params.detailTris = tempData->m_dmesh->tris;
		params.detailTriCount = tempData->m_dmesh->ntris;
		params.offMeshConVerts = m_geom->getOffMeshConnectionVerts();
		params.offMeshConRad = m_geom->getOffMeshConnectionRads();
		params.offMeshConDir = m_geom->getOffMeshConnectionDirs();
		params.offMeshConAreas = m_geom->getOffMeshConnectionAreas();
		params.offMeshConFlags = m_geom->getOffMeshConnectionFlags();
		params.offMeshConUserID = m_geom->getOffMeshConnectionId();
		params.offMeshConCount = m_geom->getOffMeshConnectionCount();
		params.walkableHeight = m_agentHeight;
		params.walkableRadius = m_agentRadius;
		params.walkableClimb = m_agentMaxClimb;
		params.tileX = tx;
		params.tileY = ty;
		params.tileLayer = 0;
		rcVcopy(params.bmin, pmesh->bmin);
		rcVcopy(params.bmax, pmesh->bmax);
		params.cs = cfg.cs;
		params.ch = cfg.ch;
		// GG: a whole map tile with few polygons is searched polygon by polygon instead (the same results), as its tree would
		// be a fifth of the navmesh
		params.buildBvTree = !m_pBake || pmesh->npolys > m_bvTreeMinPolys;
		
		if (!dtCreateNavMeshData(&params, &navData, &navDataSize))
		{
			tileLog(RC_LOG_ERROR, "Could not build Detour navmesh.");
			return 0;
		}		
	}
	
	// Show performance stats.
	tileLog(RC_LOG_PROGRESS, ">> Polymesh: %d vertices  %d polygons", pmesh->nverts, pmesh->npolys);

	dataSize = navDataSize;
	return navData;
}
