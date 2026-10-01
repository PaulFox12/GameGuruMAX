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

#ifndef RECASTSAMPLETILEMESH_H
#define RECASTSAMPLETILEMESH_H

#include "Sample.h"
#include "DetourNavMesh.h"
#include "Recast.h"
#include "ChunkyTriMesh.h"
#include <vector>
#include <stdint.h>

// GG: the whole map navmesh's inputs beside the static objects in the InputGeom (Sample_TileMesh::bakeWholeMap)
struct GGNavMeshBake
{
	float bmin[3] = { 0, 0, 0 }; // the area; only x and z matter, as each tile finds its own height range
	float bmax[3] = { 0, 0, 0 };
	float waterY = -1e30f; // a walkable surface below it is dropped (bridges and piers above it stay)
	float sampleSpacing = 32.0f; // the terrain is sampled on a grid this far apart
	int (*pfnHeight)( float x, float z, float* pY ) = 0; // the terrain's height there, called from the build threads
	const float* pTrees = 0; // tree trunks: x, y, z and the trunk's diameter each
	uint32_t numTrees = 0;
	uint64_t buildKey = 0; // the settings and the area (Sample_TileMesh::wholeMapKey): a change builds every tile
	uint64_t terrainFingerprint = 0; // the terrain's height inputs: while they match the tiles', their terrain is not sampled to check them
};

// GG: what a whole map bake did
struct GGNavMeshBakeStats
{
	int tilesX = 0, tilesZ = 0;
	int rebuilt = 0; // tiles built again, their inputs changed or new
	int underwater = 0; // of those, wholly under water with nothing on them, so left empty
	int failed = 0; // tiles Detour refused
	int withData = 0; // tiles in the navmesh after the bake
	uint64_t bytes = 0; // their data
	uint32_t polys = 0, verts = 0, detailVerts = 0, detailTris = 0;
	bool fresh = false; // no navmesh for these settings to start from, so every tile was built
	double milliseconds = 0;
};

struct TileMeshData
{
	unsigned char* m_triareas = 0; // delete
	rcHeightfield* m_solid = 0; // delete
	rcCompactHeightfield* m_chf = 0; // delete
	rcContourSet* m_cset = 0; // delete
	rcPolyMesh* m_pmesh = 0;
	rcPolyMeshDetail* m_dmesh = 0;
	rcConfig m_cfg;

	// GG: the whole map bake's terrain samples over the tile and its border, the tile's height range, and the triangles
	// rasterised beside the static objects (the terrain, the tree trunks)
	std::vector<float> m_heights;
	int m_samplesX = 0, m_samplesZ = 0;
	float m_samplesMinX = 0, m_samplesMinZ = 0;
	float m_terrainMinY = 0, m_terrainMaxY = 0;
	bool m_hasSamples = false;
	std::vector<float> m_bakeTris;
	std::vector<unsigned char> m_bakeAreas;

	void cleanup()
	{
		m_hasSamples = false;
		if ( m_triareas ) delete [] m_triareas;
		m_triareas = 0;
		rcFreeHeightField(m_solid);
		m_solid = 0;
		rcFreeCompactHeightfield(m_chf);
		m_chf = 0;
		rcFreeContourSet(m_cset);
		m_cset = 0;
		rcFreePolyMesh(m_pmesh);
		m_pmesh = 0;
		rcFreePolyMeshDetail(m_dmesh);
		m_dmesh = 0;
	}
};

class Sample_TileMesh : public Sample
{
protected:
	bool m_buildAll;
	
	/*
	unsigned char* m_triareas;
	rcHeightfield* m_solid;
	rcCompactHeightfield* m_chf;
	rcContourSet* m_cset;
	rcPolyMesh* m_pmesh;
	rcPolyMeshDetail* m_dmesh;
	rcConfig m_cfg;	
	*/

	TileMeshData tempData;
	
	enum DrawMode
	{
		DRAWMODE_NAVMESH,
		DRAWMODE_NAVMESH_TRANS,
		DRAWMODE_NAVMESH_BVTREE,
		DRAWMODE_NAVMESH_NODES,
		DRAWMODE_NAVMESH_PORTALS,
		DRAWMODE_NAVMESH_INVIS,
		DRAWMODE_MESH,
		DRAWMODE_VOXELS,
		DRAWMODE_VOXELS_WALKABLE,
		DRAWMODE_COMPACT,
		DRAWMODE_COMPACT_DISTANCE,
		DRAWMODE_COMPACT_REGIONS,
		DRAWMODE_REGION_CONNECTIONS,
		DRAWMODE_RAW_CONTOURS,
		DRAWMODE_BOTH_CONTOURS,
		DRAWMODE_CONTOURS,
		DRAWMODE_POLYMESH,
		DRAWMODE_POLYMESH_DETAIL,		
		MAX_DRAWMODE
	};
		
	DrawMode m_drawMode;
	
	int m_maxTiles;
	int m_maxPolysPerTile;
	float m_tileSize;
	
	void cleanup();
	
	void saveAll(const char* path, const dtNavMesh* mesh);
	dtNavMesh* loadAll(const char* path);

	// GG: the whole map bake (bakeWholeMap): its inputs while it runs, and each tile's input and terrain hashes, so only
	// the tiles whose inputs change are built again
	const GGNavMeshBake* m_pBake = 0;
	std::vector<uint64_t> m_tileHash;
	std::vector<uint64_t> m_tileTerrainHash;
	uint64_t m_tilesBuildKey = 0;
	uint64_t m_tilesTerrainFingerprint = 0;
	uint64_t m_tilesObjectsHash = 0;
	int m_tilesX = 0, m_tilesZ = 0;
	std::vector<uint32_t> m_treeTileStart; // the trees overlapping each tile and its border, as indices into the bake's
	std::vector<uint32_t> m_treeTileIndex;

	void sampleTerrain( TileMeshData* tempData, const float* bmin, const float* bmax );
	uint64_t hashTileObjects( int index, const float* bmin, const float* bmax, float* pMinY, float* pMaxY, int* pCount );
	void rasteriseBakeInputs( TileMeshData* tempData, int index, const rcConfig& cfg );
		
public:
	Sample_TileMesh();
	virtual ~Sample_TileMesh();
	
	virtual void handleSettings();
	//virtual void handleTools();
	//virtual void handleDebugMode();
	virtual void handleRender();
	//virtual void handleRenderOverlay(double* proj, double* model, int* view);
	virtual void handleMeshChanged(class InputGeom* geom);
	virtual bool handleBuild();
	virtual void collectSettings(struct BuildSettings& settings);
	
	void getTilePos(const float* pos, int& tx, int& ty);
	float getTileSize() { return m_tileSize; }
	float getCellSize() { return m_cellSize; }
	
	unsigned char* buildTileMesh(TileMeshData* tempData, const int tx, const int ty, const float* bmin, const float* bmax, int& dataSize);
	void buildTile(const float* pos);
	void removeTile(const float* pos);
	void buildAllTiles();
	void removeAllTiles();

	// GG: the whole map navmesh. bakeWholeMap builds the tiles over the bake's area whose inputs (the terrain under them,
	// the static objects in the InputGeom, the trees) changed since the navmesh held was built, or every tile when it holds
	// none for these settings; saveWholeMap and loadWholeMap keep it with the level (miniz-compressed tiles and hashes)
	uint64_t wholeMapKey( const GGNavMeshBake* pBake );
	bool bakeWholeMap( const GGNavMeshBake* pBake, uint64_t objectsHash, GGNavMeshBakeStats* pStats );
	bool hasWholeMap( uint64_t buildKey ) const { return m_navMesh && m_tilesBuildKey == buildKey && m_tilesBuildKey != 0; }
	bool isWholeMapCurrent( uint64_t buildKey, uint64_t terrainFingerprint, uint64_t objectsHash ) const { return hasWholeMap( buildKey ) && m_tilesTerrainFingerprint == terrainFingerprint && m_tilesObjectsHash == objectsHash; }
	uint64_t saveWholeMap( const char* pPath ); // the file's size, 0 if it could not be written
	int loadWholeMap( const char* pPath, uint64_t buildKey ); // tiles loaded, -1 for no file or one made with other settings or another area
	bool isBaking() const { return m_pBake != 0; }
	void bakeTile( TileMeshData* tempData, struct TileWork* pWork, const float* tileMin, const float* tileMax );
	void setBakeGeom( class InputGeom* geom ) { m_geom = geom; } // the static objects, keeping the navmesh (handleMeshChanged drops it)
	void setVertsPerPoly( int n ) { m_vertsPerPoly = (float)n; }

private:
	// Explicitly disabled copy constructor and copy assignment operator.
	Sample_TileMesh(const Sample_TileMesh&);
	//Sample_TileMesh& operator=(const Sample_TileMesh&);
};


#endif // RECASTSAMPLETILEMESH_H
