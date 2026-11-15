// Includes
#include "GGRecastDetour.h"
#include "CFileC.h"
#include "DetourCommon.h"
#include <chrono>

// Globals
bool g_bNavMeshChanged = false;
std::vector<sBlocker> g_BlockerList;
std::vector<sTokenDrop> g_TokenDropList;
float g_fWaterTableY = 0.0f;

// main functions
GGRecastDetour::GGRecastDetour()
{
	sample = 0;
	geom = 0;
	tool = 0;
}

void GGRecastDetour::freeall (void)
{
	if (tool)
	{
		delete tool;
		tool = 0;
	}
	if (sample)
	{
		delete sample;
		sample = 0;
	}
	if (geom)
	{
		delete geom;
		geom = 0;
	}
}

int GGRecastDetour::buildall (float* pVertices, uint32_t numVertices)
{
	// free any previous resources
	freeall();

	// create simple solo mesh sample
	sample = new Sample_TileMesh();
	if (sample)
	{
		// initialise debug objects in sample class
		sample->getDebugDraw().init();

		// set context for sample
		sample->setContext(&ctx);

		// create geom to hold level data
		geom = new InputGeom;
		sample->handleMeshChanged(geom);

		if ( pVertices )
		{
			if (!geom->loadData(&ctx, pVertices, numVertices))
			{
				void timestampactivity(int i, char* desc_s);
				char pLog[256];
				sprintf_s(pLog, 256, "Navmesh geometry load failed for %u vertices", numVertices);
				timestampactivity(0, pLog);
				// error
				delete geom;
				geom = 0;
				delete sample;
				sample = 0;
				return 0;
			}
		}
		else
		{
			// load data into geom
			char szRealFilename[2048];
			strcpy_s(szRealFilename, 2048, "levelbank\\testmap\\rawlevelgeometry.obj");
			GG_GetRealPath(szRealFilename, 1);
			std::string meshName = szRealFilename;
			if (!geom->load(&ctx, meshName))
			{
				// error
				delete geom;
				geom = 0;
				delete sample;
				sample = 0;
				return 0;
			}
		}

		// if have sample and geom, build nav mesh
		if (geom && sample)
		{
			sample->handleSettings();

			// build navmesh
			ctx.resetLog();
			if (!sample->handleBuild())
			{
				// failed
			}
			ctx.dumpLog("Build log");
		}
	}

	// create navmesh tool and init with build sample
	tool = new NavMeshTesterTool;
	if (tool) tool->init(sample);

	// success
	return 1;
}

static double GGRecastDetour_Milliseconds( const LARGE_INTEGER& start )
{
	LARGE_INTEGER freq, now;
	QueryPerformanceFrequency( &freq );
	QueryPerformanceCounter( &now );
	return (double)(now.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
}

bool GGRecastDetour::prepareWholeMap( GGNavMeshBake* pBake, uint64_t objectsHash, const GGNavMeshSettings& settings, const char* pLevelId, const char* pLoadFile, const char* pLoadFile2,
	char* pReport, int reportSize )
{
	// another level's saved navmesh replaces the one held; without one the held navmesh is brought up to date, which
	// builds nothing for the same level under another name
	if ( !pLevelId ) pLevelId = "";
	const bool bLevelChanged = m_sWholeMapLevel != pLevelId;
	m_sWholeMapLevel = pLevelId;
	if ( !sample )
	{
		sample = new Sample_TileMesh();
		sample->getDebugDraw().init();
		sample->setContext( &ctx );
	}
	Sample_TileMesh* pTiles = (Sample_TileMesh*)sample;
	pTiles->setBakeSettings( settings );
	pBake->buildKey = pTiles->wholeMapKey( pBake );

	// the saved navmesh, while none is held for these settings
	char pLoad[640] = "";
	LARGE_INTEGER start;
	if ( !pTiles->hasWholeMap( pBake->buildKey ) || bLevelChanged )
	{
		const char* pFiles[2] = { pLoadFile, pLoadFile2 };
		for ( int f = 0; f < 2 && pLoad[0] == 0; f++ )
		{
			if ( !pFiles[f] || !pFiles[f][0] ) continue;
			QueryPerformanceCounter( &start );
			int tiles = pTiles->loadWholeMap( pFiles[f], pBake->buildKey );
			if ( tiles >= 0 ) sprintf_s( pLoad, 640, "loaded %d tiles from %s in %.0f ms; ", tiles, pFiles[f], GGRecastDetour_Milliseconds( start ) );
		}
		if ( pLoad[0] == 0 ) sprintf_s( pLoad, 640, "no saved navmesh for these settings and this area%s; ", pTiles->hasWholeMap( pBake->buildKey ) ? " (the one held is updated)" : "" );
	}
	strcpy_s( pReport, reportSize, pLoad );

	// nothing changed since it was built
	if ( pTiles->isWholeMapCurrent( pBake->buildKey, pBake->terrainFingerprint, objectsHash ) )
	{
		if ( !tool ) tool = new NavMeshTesterTool;
		tool->init( sample );
		return true;
	}
	if ( pTiles->hasWholeMap( pBake->buildKey ) )
	{
		const bool bTerrain = pTiles->getWholeMapTerrainFingerprint() != pBake->terrainFingerprint;
		const bool bObjects = pTiles->getWholeMapObjectsHash() != objectsHash;
		strcat_s( pReport, reportSize, bTerrain ? (bObjects ? "terrain and objects differ; " : "terrain differs; ") : "objects differ; " );
	}
	return false;
}

int GGRecastDetour::bakeWholeMap( GGNavMeshBake* pBake, float* pStaticVerts, uint32_t numStaticVerts, uint64_t objectsHash, const char* pSaveFile, char* pReport, int reportSize )
{
	if ( !sample ) return 0;
	Sample_TileMesh* pTiles = (Sample_TileMesh*)sample;
	LARGE_INTEGER start;

	// the static objects
	if ( geom ) delete geom;
	geom = new InputGeom;
	if ( numStaticVerts > 0 && !geom->loadData( &ctx, pStaticVerts, numStaticVerts ) )
	{
		delete geom;
		geom = 0;
	}
	pTiles->setBakeGeom( geom );

	GGNavMeshBakeStats stats;
	if ( !pTiles->bakeWholeMap( pBake, objectsHash, &stats ) )
	{
		// the tool may point at a navmesh the failed bake let go, so path queries answer nothing until one is built
		if ( tool )
		{
			delete tool;
			tool = 0;
		}
		sprintf_s( pReport, reportSize, "Navmesh (whole map): the bake failed" );
		return 0;
	}
	if ( !tool ) tool = new NavMeshTesterTool;
	tool->init( sample );

	char pSave[640] = "";
	if ( pSaveFile && pSaveFile[0] )
	{
		QueryPerformanceCounter( &start );
		uint64_t bytes = pTiles->saveWholeMap( pSaveFile );
		if ( bytes ) sprintf_s( pSave, 640, "; saved %s, %llu bytes, in %.0f ms", pSaveFile, (unsigned long long)bytes, GGRecastDetour_Milliseconds( start ) );
		else sprintf_s( pSave, 640, "; could not save %s", pSaveFile );
	}
	// where the tiles built again changed, the first few of each kind
	char pWhere[256] = "";
	if ( !stats.fresh && stats.rebuilt > 0 )
	{
		strcpy_s( pWhere, 256, "; terrain changed at" );
		for ( int i = 0; i < stats.terrainExamples; i++ ) sprintf_s( pWhere + strlen( pWhere ), 256 - strlen( pWhere ), " (%.0f,%.0f)", stats.terrainExampleX[i], stats.terrainExampleZ[i] );
		strcat_s( pWhere, 256, ", objects only at" );
		for ( int i = 0; i < stats.objectsExamples; i++ ) sprintf_s( pWhere + strlen( pWhere ), 256 - strlen( pWhere ), " (%.0f,%.0f)", stats.objectsExampleX[i], stats.objectsExampleZ[i] );
	}
	sprintf_s( pReport, reportSize, "Navmesh (whole map): %d x %d tiles, %d built%s (%d for terrain, %d under water, %d refused) in %.0f ms; now %d tiles (%d at the open cell size), %llu bytes (%llu links, %llu BV trees): %u polys, %u verts, %u detail verts, %u detail tris%s%s",
		stats.tilesX, stats.tilesZ, stats.rebuilt, stats.fresh ? " (all)" : "", stats.rebuiltTerrain, stats.underwater, stats.failed, stats.milliseconds,
		stats.withData, stats.openTiles, (unsigned long long)stats.bytes, (unsigned long long)stats.linkBytes, (unsigned long long)stats.bvBytes,
		stats.polys, stats.verts, stats.detailVerts, stats.detailTris, pWhere, pSave );
	return 1;
}

uint64_t GGRecastDetour::saveWholeMap( const char* pSaveFile )
{
	if ( !sample || !pSaveFile || !pSaveFile[0] ) return 0;
	return ((Sample_TileMesh*)sample)->saveWholeMap( pSaveFile );
}

int GGRecastDetour::findPath (float fStart[3], float fEnd[3])
{
	if (tool)
	{
		// find path through nav mesh
		tool->findAPath (fStart, fEnd);

		// success
		return 1;
	}

	// failure
	return 0;
}

void GGRecastDetour::getBoxCheck (char* pOut, int size)
{
	// GG: the last path as Lua's RDGetPathPoint* calls give it, each of its lines checked against every blocking box (the first
	// line excepted when it steps out of the box the start stands in): how many cross one, and the first that does
	if (!tool) { sprintf_s(pOut, size, "no navmesh"); return; }
	int iPoints = 0;
	float* pPoints = NULL;
	tool->getPathResult(&iPoints, &pPoints);
	if (pPoints == NULL) iPoints = 0;
	extern int g_iGGLastBlockerHit;
	extern bool g_bGGNavBoxExits;
	extern bool DoesLineGoThroughBlocker (float fFromX, float fFromY, float fFromZ, float fToX, float fToY, float fToZ);
	const int iLastHit = g_iGGLastBlockerHit;
	int iCrossing = 0, iFirst = -1, iFirstBox = -1;
	for (int i = 0; i + 1 < iPoints; i++)
	{
		if (i == 0 && tool->m_ggStartBox >= 0) continue;
		const float* p = pPoints + (i * 3);
		const float* q = pPoints + ((i + 1) * 3);
		if (DoesLineGoThroughBlocker(p[0], p[1], p[2], q[0], q[1], q[2]))
		{
			iCrossing++;
			if (iFirst < 0) { iFirst = i; iFirstBox = g_iGGLastBlockerHit; }
		}
	}
	g_iGGLastBlockerHit = iLastHit;
	const float* e = iPoints > 0 ? pPoints + ((iPoints - 1) * 3) : NULL;
	sprintf_s(pOut, size, "points=%d crossing=%d first=%d box=%d start_box=%d goal_box=%d rerouted=%d reroute_failed=%d arcs_kept=%d end=%.0f,%.0f,%.0f fix=%d",
		iPoints, iCrossing, iFirst, iFirstBox, tool->m_ggStartBox, tool->m_ggGoalBox, tool->m_ggRerouted, tool->m_ggRerouteFailed,
		tool->m_ggArcsKept, e ? e[0] : 0.0f, e ? e[1] : 0.0f, e ? e[2] : 0.0f, g_bGGNavBoxExits ? 1 : 0);
}
int GGRecastDetour::runPathBatch (const char* pIn, const char* pOut, int iReps)
{
	// GG: path searches read from a file, one a line (start x y z, goal x y z), each run iReps times and timed, then a line each
	// written: its index, ms a search, the box check (getBoxCheck) and its points as Lua's RDGetPathPoint* calls give them. The
	// searches done, or -1 if a file could not be opened (harness NAV_PATHBATCH: a test's hundreds of searches in one call)
	if (!tool) return -1;
	FILE* fIn = NULL;
	FILE* fOut = NULL;
	if (fopen_s(&fIn, pIn, "r") != 0 || fIn == NULL) return -1;
	if (fopen_s(&fOut, pOut, "w") != 0 || fOut == NULL) { fclose(fIn); return -1; }
	if (iReps < 1) iReps = 1;
	char line[512];
	int iDone = 0;
	while (fgets(line, sizeof(line), fIn))
	{
		float s[3], e[3];
		if (sscanf_s(line, "%f %f %f %f %f %f", &s[0], &s[1], &s[2], &e[0], &e[1], &e[2]) != 6) continue;
		auto t0 = std::chrono::high_resolution_clock::now();
		for (int r = 0; r < iReps; r++)
		{
			float fStart[3] = { s[0], s[1], s[2] };
			float fEnd[3] = { e[0], e[1], e[2] };
			findPath(fStart, fEnd);
		}
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count() / iReps;
		char check[400];
		getBoxCheck(check, sizeof(check));
		int iPoints = 0;
		float* pPoints = NULL;
		getPath(&iPoints, &pPoints);
		if (pPoints == NULL) iPoints = 0;
		fprintf(fOut, "%d ms=%.4f %s pts=", iDone, ms, check);
		for (int i = 0; i < iPoints; i++)
			fprintf(fOut, "%s%.1f,%.1f,%.1f", i ? " " : "", pPoints[i * 3], pPoints[i * 3 + 1], pPoints[i * 3 + 2]);
		fprintf(fOut, "\n");
		iDone++;
	}
	fclose(fIn);
	fclose(fOut);
	return iDone;
}
void GGRecastDetour::getBoxes (char* pOut, int size)
{
	// GG: every blocker box: its index, closed (1) or open, centre, half sizes in its own frame, turn and height span
	int len = sprintf_s(pOut, size, "%d", (int)g_BlockerList.size());
	for (int i = 0; i < (int)g_BlockerList.size() && len > 0 && len < size - 128; i++)
	{
		const sBlocker& b = g_BlockerList[i];
		int n = sprintf_s(pOut + len, size - len, ";%d %d %.2f %.2f %.2f %.2f %.3f %.2f %.2f", i, b.bBlocking ? 1 : 0,
			(b.minX + b.maxX) / 2, (b.minZ + b.maxZ) / 2, (b.maxX - b.minX) / 2, (b.maxZ - b.minZ) / 2, b.fAngle, b.minY, b.maxY);
		if (n <= 0) break;
		len += n;
	}
}
int GGRecastDetour::getPath (int* piPointCount, float** ppfPointData)
{
	if (tool)
	{
		// resulting path
		tool->getPathResult(piPointCount, ppfPointData);
		if (*ppfPointData != NULL)
		{
			// success
			return 1;
		}
	}

	// failure
	return 0;
}

bool GGRecastDetour::isWithinNavMeshEx (float fX, float fY, float fZ, float* pvecNearestPt, bool bMustBeOverPoly)
{
	if (tool)
	{ 
		bool pIsOverPoly = false;
		if (tool->isWithinNavMeshEx(fX, fY, fZ, pvecNearestPt, &pIsOverPoly) == true)
		{
			if (bMustBeOverPoly == true)
			{
				if (pIsOverPoly == true)
				{
					return true;
				}
			}
			else
			{
				return true;
			}
		}
		return false;
	}
	else
		return false;
}

bool GGRecastDetour::isWithinNavMesh (float fX, float fY, float fZ)
{
	return isWithinNavMeshEx(fX, fY, fZ, 0, 0);
}

float GGRecastDetour::getYFromPos (float fX, float fY, float fZ)
{
	if (tool)
		return tool->getYFromPos(fX, fY, fZ);
	else
		return fY;
}

void GGRecastDetour::forceDebugUpdate(void)
{
	g_bNavMeshChanged = true;
}

void GGRecastDetour::handleDebugRender(void)
{
	if (sample)
	{
		if (g_bNavMeshChanged == true)
		{
			sample->GetDD()->hideDebugObjects();
			g_bNavMeshChanged = false;
		}
		sample->handleRender();
	}
	if (tool)
	{
		tool->handleRender();
	}
}

void GGRecastDetour::cleanupDebugRender(void)
{
	if (sample)
	{
		sample->GetDD()->hideDebugObjects();
	}
}

// radius of 15 seems about right
void GGRecastDetour::TogglePolys( float x, float y, float z, float radius, bool enable )
{
	float p[ 3 ] = { x, y, z };
	
	if ( !sample ) return;
	dtNavMesh* nav = sample->getNavMesh();
	dtNavMeshQuery* navquery = sample->getNavMeshQuery();
	if (nav && navquery)
	{
		dtQueryFilter filter;
		const float halfExtents[ 3 ] = { radius, radius, radius };
			
		int numFound = 0;
		const int findMax = 100;
		dtPolyRef ref[ findMax ];
			
		navquery->queryPolygons(p, halfExtents, &filter, ref, &numFound, findMax);
		for( int i = 0; i < numFound; i++ )
		{
			const dtPoly* pPoly = 0;
			const dtMeshTile* pTile = 0;
			nav->getTileAndPolyByRefUnsafe( ref[i], &pTile, &pPoly );

			float closest[3];
			unsigned short vert1 = pPoly->verts[0];
			unsigned short vert2 = pPoly->verts[1];
			unsigned short vert3 = pPoly->verts[2];
			float* a = &pTile->verts[ vert1*3 ];
			float* b = &pTile->verts[ vert2*3 ];
			float* c = &pTile->verts[ vert3*3 ];
			dtClosestPtPointTriangle(closest, p, a, b, c );

			float dist = dtVdistSqr( closest, p );
			if ( dist > radius*radius ) continue;

			unsigned short flags = 0;
			if (dtStatusSucceed(nav->getPolyFlags(ref[i], &flags)))
			{
				if ( enable ) flags |= SAMPLE_POLYFLAGS_DISABLED;
				else flags &= ~SAMPLE_POLYFLAGS_DISABLED;
				nav->setPolyFlags(ref[i], flags);
			}
		}
	}

	// when we toggle polys, force a debug update
	forceDebugUpdate();
}

void GGRecastDetour::ResetTokenDropSystem(void)
{
	g_TokenDropList.clear();
}
void GGRecastDetour::DoTokenDrop(float x, float y, float z, int iType, float fDuration)
{
	bool bAnyNavMeshTokenDropStateChanged = false;
	if(iType>0)
	{
		sTokenDrop item;
		item.iType = iType;
		item.X = x;
		item.Y = y;
		item.Z = z;
		item.fTimeLeft = fDuration;
		g_TokenDropList.push_back(item);
		bAnyNavMeshTokenDropStateChanged = true;
	}
	if (bAnyNavMeshTokenDropStateChanged == true)
	{
		// GG: only the token drops' debug object is made again, at the next debug render (making every debug object again
		// here, the navmesh's too, took 20-30 ms a call on a whole map navmesh)
		if (sample) sample->GetDD()->setDebugObjectSlot(5, true);
	}
}
void GGRecastDetour::ManageTokenDropSystem(float fTimeDelta)
{
	// fTimeDelta comes in at 0.75 for 60fps, so for MS we *20
	fTimeDelta *= 20.0f; // so now we get a nice 16.6ms chomp from the duration
	int iTokenDropCount = g_TokenDropList.size();
	if (iTokenDropCount > 0)
	{
		for (int iTokenDropIndex = 0; iTokenDropIndex < iTokenDropCount; iTokenDropIndex++)
		{
			g_TokenDropList[iTokenDropIndex].fTimeLeft -= fTimeDelta;
			if (g_TokenDropList[iTokenDropIndex].fTimeLeft <= 0.0f)
			{
				g_TokenDropList.erase(g_TokenDropList.begin() + iTokenDropIndex);
				iTokenDropCount--;
				iTokenDropIndex--;
			}
		}
	}
}
int GGRecastDetour::GetTokenDropCount() { return g_TokenDropList.size(); }
float GGRecastDetour::GetTokenDropX(int iIndex) { return g_TokenDropList[iIndex].X; }
float GGRecastDetour::GetTokenDropY(int iIndex) { return g_TokenDropList[iIndex].Y; }
float GGRecastDetour::GetTokenDropZ(int iIndex) { return g_TokenDropList[iIndex].Z; }
int GGRecastDetour::GetTokenDropType(int iIndex) { return g_TokenDropList[iIndex].iType; }
float GGRecastDetour::GetTokenDropTimeLeft(int iIndex) { return g_TokenDropList[iIndex].fTimeLeft; }

bool g_bGGNavBoxExits = true; // GG: the blocking boxes' new rule (see GGNavPointInBlocker); harness SET_NAVBOXFIX 0 gives the old
int g_iGGLastBlockerHit = -1; // GG: the blocker the last blocked line went through
bool DoesLineGoThroughBlocker (float fFromOrigX, float fFromOrigY, float fFromOrigZ, float fToOrigX, float fToOrigY, float fToOrigZ)
{
	// LB: add an extra cost if the path runs through an area marked as a door, it will
	// force the system to find another path that does NOT go through dor areas (effectively blocking them as path ways when active)
	bool bBlocked = false;
	extern std::vector<sBlocker> g_BlockerList;
	if (g_BlockerList.size() > 0)
	{
		int iDoorCount = g_BlockerList.size();
		for (int iDoorIndex = 0; iDoorIndex < iDoorCount; iDoorIndex++)
		{
			if (bBlocked==false)
			{
				if (g_BlockerList[iDoorIndex].bBlocking == true)
				{
					float fDoorMinX = g_BlockerList[iDoorIndex].minX;
					float fDoorMaxX = g_BlockerList[iDoorIndex].maxX;
					float fDoorMinY = g_BlockerList[iDoorIndex].minY;
					float fDoorMaxY = g_BlockerList[iDoorIndex].maxY;
					float fDoorMinZ = g_BlockerList[iDoorIndex].minZ;
					float fDoorMaxZ = g_BlockerList[iDoorIndex].maxZ;
					float fCenterX = fDoorMinX + ((fDoorMaxX - fDoorMinX) / 2);
					float fCenterZ = fDoorMinZ + ((fDoorMaxZ - fDoorMinZ) / 2);

					// quick out, same result: the test below turns the segment about the box centre, which keeps its
					// distance from the centre, and samples points along it. A segment wholly above or below the box, or
					// further from the centre (in XZ) than the box corners, cannot pass through it. The path search
					// calls this for every step it considers, so skipping the turn and the sampling matters.
					if ((fFromOrigY < fDoorMinY && fToOrigY < fDoorMinY) || (fFromOrigY > fDoorMaxY && fToOrigY > fDoorMaxY))
						continue;
					float fHalfX = (fDoorMaxX - fDoorMinX) / 2;
					float fHalfZ = (fDoorMaxZ - fDoorMinZ) / 2;
					float fReach = sqrtf((fHalfX * fHalfX) + (fHalfZ * fHalfZ)) + 1.0f;
					float fSegX = fToOrigX - fFromOrigX;
					float fSegZ = fToOrigZ - fFromOrigZ;
					float fSegLenSqr = (fSegX * fSegX) + (fSegZ * fSegZ);
					float fT = 0.0f;
					if (fSegLenSqr > 0.0f) fT = (((fCenterX - fFromOrigX) * fSegX) + ((fCenterZ - fFromOrigZ) * fSegZ)) / fSegLenSqr;
					if (fT < 0.0f) fT = 0.0f;
					if (fT > 1.0f) fT = 1.0f;
					float fNearX = fFromOrigX + (fSegX * fT) - fCenterX;
					float fNearZ = fFromOrigZ + (fSegZ * fT) - fCenterZ;
					if ((fNearX * fNearX) + (fNearZ * fNearZ) > fReach * fReach)
						continue;

					float fFromX = fFromOrigX;
					float fFromY = fFromOrigY;
					float fFromZ = fFromOrigZ;
					float fToX = fToOrigX;
					float fToY = fToOrigY;
					float fToZ = fToOrigZ;
					// the blocker is turned by fAngle as an entity is (and as the navmesh debug view draws it). These angles are taken
					// with z flipped (fCenterZ - z), so adding fAngle turns the segment back by fAngle into the box's own space;
					// subtracting it blocked the box's mirror image at any angle but 0, 90, 180 and 270
					float tofromradian = 0.0174533f;
					float fDX = fFromX - fCenterX;
					float fDZ = fCenterZ - fFromZ;
					float fDD = sqrtf(fabs(fDX * fDX) + fabs(fDZ * fDZ));
					float fFromA = atan2(fDX, fDZ) / tofromradian;
					fFromA = (fFromA + g_BlockerList[iDoorIndex].fAngle) * tofromradian;
					fFromX = fCenterX + (sinf(fFromA) * fDD);
					fFromZ = fCenterZ - (cosf(fFromA) * fDD);
					fDX = fToX - fCenterX;
					fDZ = fCenterZ - fToZ;
					fDD = sqrtf(fabs(fDX * fDX) + fabs(fDZ * fDZ));
					float fToA = atan2(fDX, fDZ) / tofromradian;
					fToA = (fToA + g_BlockerList[iDoorIndex].fAngle) * tofromradian;
					fToX = fCenterX + (sinf(fToA) * fDD);
					fToZ = fCenterZ - (cosf(fToA) * fDD);

					// GG: the turned line against the box exactly (slabs), not by samples 2 units apart: one test where a long line
					// near a box took hundreds, and no corner clipped between two samples
					if (g_bGGNavBoxExits == true)
					{
						const float fF[3] = { fFromX, fFromY, fFromZ };
						const float fD[3] = { fToX - fFromX, fToY - fFromY, fToZ - fFromZ };
						const float fMin[3] = { fDoorMinX, fDoorMinY, fDoorMinZ };
						const float fMax[3] = { fDoorMaxX, fDoorMaxY, fDoorMaxZ };
						float fTMin = 0.0f, fTMax = 1.0f;
						bool bHit = true;
						for (int a = 0; a < 3 && bHit; a++)
						{
							if (fabs(fD[a]) < 0.000001f)
							{
								if (fF[a] < fMin[a] || fF[a] > fMax[a]) bHit = false;
							}
							else
							{
								float fT1 = (fMin[a] - fF[a]) / fD[a];
								float fT2 = (fMax[a] - fF[a]) / fD[a];
								if (fT1 > fT2) { float fT = fT1; fT1 = fT2; fT2 = fT; }
								if (fT1 > fTMin) fTMin = fT1;
								if (fT2 < fTMax) fTMax = fT2;
								if (fTMin > fTMax) bHit = false;
							}
						}
						if (bHit)
						{
							bBlocked = true;
							g_iGGLastBlockerHit = iDoorIndex;
						}
						continue;
					}

					float fX = fFromX;
					float fY = fFromY;
					float fZ = fFromZ;
					float fIX = fToX - fFromX;
					float fIY = fToY - fFromY;
					float fIZ = fToZ - fFromZ;
					int iStepCount = sqrt(fabs(fIX * fIX) + fabs(fIY * fIY) + fabs(fIZ * fIZ));
					fIX /= iStepCount;
					fIY /= iStepCount;
					fIZ /= iStepCount;
					for (int iStep = 0; iStep < iStepCount; iStep += 2)
					{
						if (fX >= fDoorMinX && fX <= fDoorMaxX)
						{
							if (fY >= fDoorMinY && fY <= fDoorMaxY)
							{
								if (fZ >= fDoorMinZ && fZ <= fDoorMaxZ)
								{
									// the path goes through a door
									bBlocked = true;
									g_iGGLastBlockerHit = iDoorIndex;
									break;
								}
							}
						}
						fX += (fIX * 2);
						fY += (fIY * 2);
						fZ += (fIZ * 2);
					}
				}
			}
			else
			{
				// the path goes through a door
				break;
			}
		}
	}
	return bBlocked;
}

// GG: points against the blocking boxes (closed doors, parked vehicles). DoesLineGoThroughBlocker refuses any line with a sample
// inside a box, so no path could leave a box it starts in or reach a goal inside one: the search runs from (to) the nearest
// point just outside the box instead, and a straight path line that cuts across a box goes round it (NavMeshTesterTool::recalc)
static void GGNavTurnAboutBox (const sBlocker& b, float fX, float fZ, float fTurn, float* pfOutX, float* pfOutZ)
{
	// as DoesLineGoThroughBlocker turns a point into the box's own frame (fTurn the box's angle); minus the angle turns it back
	float tofromradian = 0.0174533f;
	float fCenterX = b.minX + ((b.maxX - b.minX) / 2);
	float fCenterZ = b.minZ + ((b.maxZ - b.minZ) / 2);
	float fDX = fX - fCenterX;
	float fDZ = fCenterZ - fZ;
	float fDD = sqrtf((fDX * fDX) + (fDZ * fDZ));
	float fA = ((atan2(fDX, fDZ) / tofromradian) + fTurn) * tofromradian;
	*pfOutX = fCenterX + (sinf(fA) * fDD);
	*pfOutZ = fCenterZ - (cosf(fA) * fDD);
}
int GGNavPointInBlocker (float fX, float fY, float fZ)
{
	// the first blocking box holding the point, tested as DoesLineGoThroughBlocker tests a line's samples, or -1
	for (int i = 0; i < (int)g_BlockerList.size(); i++)
	{
		const sBlocker& b = g_BlockerList[i];
		if (b.bBlocking == false) continue;
		if (fY < b.minY || fY > b.maxY) continue;
		float fBX, fBZ;
		GGNavTurnAboutBox(b, fX, fZ, b.fAngle, &fBX, &fBZ);
		if (fBX >= b.minX && fBX <= b.maxX && fBZ >= b.minZ && fBZ <= b.maxZ) return i;
	}
	return -1;
}
bool GGNavBlockerFacePoint (int iBox, float fX, float fZ, int iFace, float fMargin, float* pfOutX, float* pfOutZ, float* pfFaceDist)
{
	// the point fMargin past face iFace of box iBox (0 its -x side, 1 +x, 2 -z, 3 +z, in the box's own frame) straight out from
	// the point given, in the world, and how far the point given is from that face
	if (iBox < 0 || iBox >= (int)g_BlockerList.size()) return false;
	const sBlocker& b = g_BlockerList[iBox];
	float fBX, fBZ, fDist;
	GGNavTurnAboutBox(b, fX, fZ, b.fAngle, &fBX, &fBZ);
	switch (iFace)
	{
	case 0: fDist = fBX - b.minX; fBX = b.minX - fMargin; break;
	case 1: fDist = b.maxX - fBX; fBX = b.maxX + fMargin; break;
	case 2: fDist = fBZ - b.minZ; fBZ = b.minZ - fMargin; break;
	default: fDist = b.maxZ - fBZ; fBZ = b.maxZ + fMargin; break;
	}
	GGNavTurnAboutBox(b, fBX, fBZ, -b.fAngle, pfOutX, pfOutZ);
	if (pfFaceDist) *pfFaceDist = fDist;
	return true;
}
bool GGNavPastBlockerFace (int iBox, float fX, float fZ, int iFace)
{
	// the point lies straight out from face iFace of box iBox: past that face, within the box's span along it
	if (iBox < 0 || iBox >= (int)g_BlockerList.size()) return false;
	const sBlocker& b = g_BlockerList[iBox];
	float fBX, fBZ;
	GGNavTurnAboutBox(b, fX, fZ, b.fAngle, &fBX, &fBZ);
	switch (iFace)
	{
	case 0: return fBX < b.minX && fBZ >= b.minZ && fBZ <= b.maxZ;
	case 1: return fBX > b.maxX && fBZ >= b.minZ && fBZ <= b.maxZ;
	case 2: return fBZ < b.minZ && fBX >= b.minX && fBX <= b.maxX;
	default: return fBZ > b.maxZ && fBX >= b.minX && fBX <= b.maxX;
	}
}
int GGNavBlockersNear (float fX1, float fZ1, float fX2, float fZ2, float fReach, int* piOut, int iMax)
{
	// the closed boxes that come within fReach of the line from 1 to 2 (each taken as a circle round its corners), nearest first,
	// up to iMax; how many
	float fDist[64];
	int iIndex[64];
	int n = 0;
	const float fSegX = fX2 - fX1, fSegZ = fZ2 - fZ1;
	const float fSegLenSqr = (fSegX * fSegX) + (fSegZ * fSegZ);
	for (int i = 0; i < (int)g_BlockerList.size() && n < 64; i++)
	{
		const sBlocker& b = g_BlockerList[i];
		if (b.bBlocking == false) continue;
		const float fCenterX = (b.minX + b.maxX) / 2, fCenterZ = (b.minZ + b.maxZ) / 2;
		const float fHalfX = (b.maxX - b.minX) / 2, fHalfZ = (b.maxZ - b.minZ) / 2;
		float fT = 0.0f;
		if (fSegLenSqr > 0.0f) fT = (((fCenterX - fX1) * fSegX) + ((fCenterZ - fZ1) * fSegZ)) / fSegLenSqr;
		if (fT < 0.0f) fT = 0.0f;
		if (fT > 1.0f) fT = 1.0f;
		const float fNearX = fX1 + (fSegX * fT) - fCenterX, fNearZ = fZ1 + (fSegZ * fT) - fCenterZ;
		const float fD = sqrtf((fNearX * fNearX) + (fNearZ * fNearZ)) - sqrtf((fHalfX * fHalfX) + (fHalfZ * fHalfZ));
		if (fD > fReach) continue;
		int k = n++;
		while (k > 0 && fDist[k - 1] > fD) { fDist[k] = fDist[k - 1]; iIndex[k] = iIndex[k - 1]; k--; }
		fDist[k] = fD;
		iIndex[k] = i;
	}
	if (n > iMax) n = iMax;
	for (int i = 0; i < n; i++) piOut[i] = iIndex[i];
	return n;
}
bool GGNavBlockerCorner (int iBox, int iCorner, float fMargin, float* pfOutX, float* pfOutZ)
{
	// corner iCorner of box iBox pushed fMargin out along both sides, in the world; corners 0 to 3 go round the box in turn
	if (iBox < 0 || iBox >= (int)g_BlockerList.size()) return false;
	const sBlocker& b = g_BlockerList[iBox];
	float fBX = (iCorner == 1 || iCorner == 2) ? b.maxX + fMargin : b.minX - fMargin;
	float fBZ = (iCorner >= 2) ? b.maxZ + fMargin : b.minZ - fMargin;
	GGNavTurnAboutBox(b, fBX, fBZ, -b.fAngle, pfOutX, pfOutZ);
	return true;
}

void GGRecastDetour::ResetBlockerSystem(void)
{
	g_BlockerList.clear();
}

void GGRecastDetour::ToggleBlocker(float x, float y, float z, float radius, bool enable, float fRadius2, float fAngle, float fAdjMinY, float fAdjMaxY)
{
	bool bAnyNavMeshBlockerStateChanged = false;
	float minX, maxX, minY, maxY, minZ, maxZ;
	bool bFoundBlocker = false;
	if (radius != fRadius2)
	{
		// new fully cunstomizable and accurate
		if (radius < 5) radius = 5;
		if (fRadius2 < 5) fRadius2 = 5;
		radius /= 2.0f;
		fRadius2 /= 2.0f;
		radius += 5.0f;
		fRadius2 += 5.0f;
		minX = x - radius;
		maxX = x + radius;
		minY = y + fAdjMinY;
		maxY = y + fAdjMaxY;
		minZ = z - fRadius2;
		maxZ = z + fRadius2;
	}
	else
	{
		// old default
		if (radius < 5) radius = 5;
		radius /= 2.0f;
		radius += 5.0f;
		minX = x - radius;
		maxX = x + radius;
		minY = y - 5;
		maxY = y + 95;
		minZ = z - radius;
		maxZ = z + radius;
	}
	for (int b = 0; b < g_BlockerList.size(); b++)
	{
		sBlocker* blocker = &g_BlockerList[b];
		if (blocker->minX == minX && blocker->maxX == maxX)
		{
			if (blocker->minY == minY && blocker->maxY == maxY)
			{
				if (blocker->minZ == minZ && blocker->maxZ == maxZ)
				{
					bFoundBlocker = true;
					if (blocker->bBlocking != enable)
					{
						bAnyNavMeshBlockerStateChanged = true;
					}
					blocker->bBlocking = enable;
					// GG: one laid again where it was takes its new angle (a vehicle that turned in place), which matching by
					// its unturned bounds kept as it was first laid
					if (enable && blocker->fAngle != fAngle)
					{
						blocker->fAngle = fAngle;
						bAnyNavMeshBlockerStateChanged = true;
					}
					break;
				}
			}
		}
	}
	if (bFoundBlocker == false)
	{
		sBlocker item;
		item.minX = minX;
		item.maxX = maxX;
		item.minY = minY;
		item.maxY = maxY;
		item.minZ = minZ;
		item.maxZ = maxZ;
		item.bBlocking = enable;
		item.fAngle = fAngle;
		g_BlockerList.push_back(item);
		bAnyNavMeshBlockerStateChanged = true;
	}
	if (bAnyNavMeshBlockerStateChanged == true)
	{
		// GG: only the blockers' debug object is made again, at the next debug render (making every debug object again
		// here, the navmesh's too, took 20-30 ms a call on a whole map navmesh)
		if (sample) sample->GetDD()->setDebugObjectSlot(4, true);
	}
}


void GGRecastDetour::SetWaterTableY(float y)
{
	g_fWaterTableY = y;
}