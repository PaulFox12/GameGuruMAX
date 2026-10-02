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
	uint64_t terrainFingerprint = 0; // the terrain's height inputs over the whole area: while they and the objects match, nothing is checked
	uint64_t terrainGlobal = 0; // of those, the ones that apply everywhere (the settings, the imported heightmap)
	uint64_t (*pfnTerrainInputs)( float minX, float minZ, float maxX, float maxZ ) = 0; // the rest on a rect (sculpting, flat areas), called from the build threads
	float (*pfnWaterLevel)( float x, float z ) = 0; // water above the sea (rivers): its height at a point, -1e30 for none
	uint64_t (*pfnWaterInputs)( float minX, float minZ, float maxX, float maxZ ) = 0; // that water over a rect, hashed (0 for none)
};

// GG: the whole map bake's settings (setup.ini navmesh*); any change builds every tile
struct GGNavMeshSettings
{
	int vertsPerPoly = 6;
	float openCellSize = 10.0f; // the cell size of a tile with no static objects on it, coarser for a smaller navmesh; the cell size (5) for none
	int bvTreeMinPolys = 128; // a tile with more polygons gets a bounding volume tree; a smaller one is searched polygon by polygon, with the same results
	float edgeMaxError = 1.3f; // how far a polygon edge may stray from the walkable area's outline, in cells
	float detailSampleDist = 6.0f; // the height detail's sample spacing, in cells of the cell size
	float detailSampleMaxError = 1.0f; // how far the height detail may stray from the surface, in cell heights
};

// GG: what a whole map bake did
struct GGNavMeshBakeStats
{
	int tilesX = 0, tilesZ = 0;
	int rebuilt = 0; // tiles built again, their inputs changed or new
	int rebuiltTerrain = 0; // of those, the ones whose terrain changed (the rest changed only in their objects)
	int terrainExamples = 0, objectsExamples = 0; // the first few of each, their centres
	float terrainExampleX[3] = {}, terrainExampleZ[3] = {}, objectsExampleX[3] = {}, objectsExampleZ[3] = {};
	int underwater = 0; // of those, wholly under water with nothing on them, so left empty
	int failed = 0; // tiles Detour refused
	int withData = 0; // tiles in the navmesh after the bake
	uint64_t bytes = 0; // their data
	uint32_t polys = 0, verts = 0, detailVerts = 0, detailTris = 0;
	uint64_t linkBytes = 0, bvBytes = 0; // of bytes, the polygon links (rebuilt when a tile loads, so not saved) and the bounding volume trees
	int openTiles = 0; // tiles at the open cell size
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
	float m_cellSize = 0; // the cell size this tile is built at, 0 for the sample's
	std::vector<float> m_bakeTris;
	std::vector<unsigned char> m_bakeAreas;
	std::vector<float> m_treeTops; // the tops of the tile's tree trunk boxes, in the order its trees are listed

	void cleanup()
	{
		m_hasSamples = false;
		m_treeTops.clear();
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
	float m_openCellSize = 0; // GGNavMeshSettings
	int m_bvTreeMinPolys = 0;
	std::vector<uint32_t> m_treeTileStart; // the trees overlapping each tile and its border, as indices into the bake's
	std::vector<uint32_t> m_treeTileIndex;

	void sampleTerrain( TileMeshData* tempData, const float* bmin, const float* bmax );
	uint64_t hashTileObjects( int index, const float* bmin, const float* bmax, float* pMinY, float* pMaxY, int* pStatics, int* pTrees );
	float bakeBorder() const; // the widest tile border either cell size needs
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
	uint64_t getWholeMapTerrainFingerprint() const { return m_tilesTerrainFingerprint; }
	uint64_t getWholeMapObjectsHash() const { return m_tilesObjectsHash; }
	uint64_t saveWholeMap( const char* pPath ); // the file's size, 0 if it could not be written
	int loadWholeMap( const char* pPath, uint64_t buildKey ); // tiles loaded, -1 for no file or one made with other settings or another area
	bool isBaking() const { return m_pBake != 0; }
	void bakeTile( TileMeshData* tempData, struct TileWork* pWork, const float* tileMin, const float* tileMax );
	void setBakeGeom( class InputGeom* geom ) { m_geom = geom; } // the static objects, keeping the navmesh (handleMeshChanged drops it)
	void setBakeSettings( const GGNavMeshSettings& settings );

private:
	// Explicitly disabled copy constructor and copy assignment operator.
	Sample_TileMesh(const Sample_TileMesh&);
	//Sample_TileMesh& operator=(const Sample_TileMesh&);
};


#endif // RECASTSAMPLETILEMESH_H
