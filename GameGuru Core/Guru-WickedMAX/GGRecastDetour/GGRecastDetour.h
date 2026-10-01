#ifndef GGRECASTDETOUR_H
#define GGRECASTDETOUR_H

#include "InputGeom.h"
#include "Sample.h"
#include "Sample_SoloMesh.h"
#include "Sample_TileMesh.h"
#include "NavMeshTesterTool.h"
#include <string>

class GGRecastDetour
{
public:
	GGRecastDetour();
	void freeall (void);
	int buildall (float* pVertices, uint32_t numVertices);

	// GG: the whole map navmesh (Sample_TileMesh::bakeWholeMap). prepareWholeMap keeps the navmesh held if it is this
	// level's (pLevelId), else loads one from pLoadFile (else pLoadFile2), and returns true if it was built from these
	// inputs (pBake, and objectsHash, a hash of the static objects and trees), so nothing need be built; it sets
	// pBake->buildKey. bakeWholeMap then builds the tiles whose inputs changed, from pStaticVerts (a triangle soup of the
	// static objects) and pBake, and saves the navmesh to pSaveFile. Each gives a line for the log; bakeWholeMap 0 if it failed
	bool prepareWholeMap( GGNavMeshBake* pBake, uint64_t objectsHash, int vertsPerPoly, const char* pLevelId, const char* pLoadFile, const char* pLoadFile2,
		char* pReport, int reportSize );
	int bakeWholeMap( GGNavMeshBake* pBake, float* pStaticVerts, uint32_t numStaticVerts, uint64_t objectsHash, const char* pSaveFile, char* pReport, int reportSize );
	uint64_t saveWholeMap( const char* pSaveFile ); // the file's size, 0 if not written
	int findPath (float fStart[3], float fEnd[3]);
	int getPath (int* piPointCount, float** ppfPointData);
	bool isWithinNavMeshEx(float fX, float fY, float fZ, float* pvecNearestPt, bool bMustBeOverPoly);
	bool isWithinNavMesh (float fX, float fY, float fZ);
	float getYFromPos (float fX, float fY, float fZ);
	void forceDebugUpdate (void);
	void handleDebugRender (void);
	void cleanupDebugRender(void);

	void TogglePolys( float x, float y, float z, float radius, bool enable );
	void ResetBlockerSystem( void );
	void ToggleBlocker(float x, float y, float z, float radius, bool enable, float fRadius2, float fAngle, float fAdjMinY, float fAdjMaxY);

	void ResetTokenDropSystem(void);
	void DoTokenDrop(float x, float y, float z, int iType, float fDuration);
	void ManageTokenDropSystem(float fTimeDelta);
	int GetTokenDropCount();
	float GetTokenDropX(int iIndex);
	float GetTokenDropY(int iIndex);
	float GetTokenDropZ(int iIndex);
	int GetTokenDropType(int iIndex);
	float GetTokenDropTimeLeft(int iIndex);

	void SetWaterTableY(float y);

private:
	// core classes
	BuildContext ctx;
	Sample* sample;
	InputGeom* geom;
	NavMeshTesterTool* tool;
	std::string m_sWholeMapLevel; // GG: the level the whole map navmesh held belongs to
};

#endif // GGRECASTDETOUR_H
