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
#include <stdlib.h>
#include <string.h>
#include "SDL.h"
#include "SDL_opengl.h"
#ifdef __APPLE__
#	include <OpenGL/glu.h>
#else
#	include <GL/glu.h>
#endif
//#include "imgui.h"
#include "NavMeshTesterTool.h"
#include "Sample.h"
#include "Recast.h"
#include "RecastDebugDraw.h"
#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "DetourDebugDraw.h"
#include "DetourCommon.h"
#include "directx-macros.h"
#include <float.h>

// GG: the blocking boxes, closed doors and parked vehicles (GGRecastDetour.cpp)
extern bool g_bGGNavBoxExits;
extern int g_iGGLastBlockerHit;
bool DoesLineGoThroughBlocker (float fFromX, float fFromY, float fFromZ, float fToX, float fToY, float fToZ);
int GGNavPointInBlocker (float fX, float fY, float fZ);
bool GGNavBlockerFacePoint (int iBox, float fX, float fZ, int iFace, float fMargin, float* pfOutX, float* pfOutZ, float* pfFaceDist);
bool GGNavPastBlockerFace (int iBox, float fX, float fZ, int iFace);
bool GGNavBlockerCorner (int iBox, int iCorner, float fMargin, float* pfOutX, float* pfOutZ);
int GGNavBlockersNear (float fX1, float fZ1, float fX2, float fZ2, float fReach, int* piOut, int iMax);

#ifdef WIN32
#	define snprintf _snprintf
#endif

// Uncomment this to dump all the requests in stdout.
#define DUMP_REQS

// Returns a random number [0..1]
static float frand()
{
//	return ((float)(rand() & 0xffff)/(float)0xffff);
	return (float)rand()/(float)RAND_MAX;
}

inline bool inRange(const float* v1, const float* v2, const float r, const float h)
{
	const float dx = v2[0] - v1[0];
	const float dy = v2[1] - v1[1];
	const float dz = v2[2] - v1[2];
	return (dx*dx + dz*dz) < r*r && fabsf(dy) < h;
}


static int fixupCorridor(dtPolyRef* path, const int npath, const int maxPath,
						 const dtPolyRef* visited, const int nvisited)
{
	int furthestPath = -1;
	int furthestVisited = -1;
	
	// Find furthest common polygon.
	for (int i = npath-1; i >= 0; --i)
	{
		bool found = false;
		for (int j = nvisited-1; j >= 0; --j)
		{
			if (path[i] == visited[j])
			{
				furthestPath = i;
				furthestVisited = j;
				found = true;
			}
		}
		if (found)
			break;
	}

	// If no intersection found just return current path. 
	if (furthestPath == -1 || furthestVisited == -1)
		return npath;
	
	// Concatenate paths.	

	// Adjust beginning of the buffer to include the visited.
	const int req = nvisited - furthestVisited;
	const int orig = rcMin(furthestPath+1, npath);
	int size = rcMax(0, npath-orig);
	if (req+size > maxPath)
		size = maxPath-req;
	if (size)
		memmove(path+req, path+orig, size*sizeof(dtPolyRef));
	
	// Store visited
	for (int i = 0; i < req; ++i)
		path[i] = visited[(nvisited-1)-i];				
	
	return req+size;
}

// This function checks if the path has a small U-turn, that is,
// a polygon further in the path is adjacent to the first polygon
// in the path. If that happens, a shortcut is taken.
// This can happen if the target (T) location is at tile boundary,
// and we're (S) approaching it parallel to the tile edge.
// The choice at the vertex can be arbitrary, 
//  +---+---+
//  |:::|:::|
//  +-S-+-T-+
//  |:::|   | <-- the step can end up in here, resulting U-turn path.
//  +---+---+
static int fixupShortcuts(dtPolyRef* path, int npath, dtNavMeshQuery* navQuery)
{
	if (npath < 3)
		return npath;

	// Get connected polygons
	static const int maxNeis = 16;
	dtPolyRef neis[maxNeis];
	int nneis = 0;

	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	if (dtStatusFailed(navQuery->getAttachedNavMesh()->getTileAndPolyByRef(path[0], &tile, &poly)))
		return npath;
	
	for (unsigned int k = poly->firstLink; k != DT_NULL_LINK; k = tile->links[k].next)
	{
		const dtLink* link = &tile->links[k];
		if (link->ref != 0)
		{
			if (nneis < maxNeis)
				neis[nneis++] = link->ref;
		}
	}

	// If any of the neighbour polygons is within the next few polygons
	// in the path, short cut to that polygon directly.
	static const int maxLookAhead = 6;
	int cut = 0;
	for (int i = dtMin(maxLookAhead, npath) - 1; i > 1 && cut == 0; i--) {
		for (int j = 0; j < nneis; j++)
		{
			if (path[i] == neis[j]) {
				cut = i;
				break;
			}
		}
	}
	if (cut > 1)
	{
		int offset = cut-1;
		npath -= offset;
		for (int i = 1; i < npath; i++)
			path[i] = path[i+offset];
	}

	return npath;
}

static bool getSteerTarget(dtNavMeshQuery* navQuery, const float* startPos, const float* endPos,
						   const float minTargetDist,
						   const dtPolyRef* path, const int pathSize,
						   float* steerPos, unsigned char& steerPosFlag, dtPolyRef& steerPosRef,
						   float* outPoints = 0, int* outPointCount = 0)							 
{
	// Find steer target.
	static const int MAX_STEER_POINTS = 3;
	float steerPath[MAX_STEER_POINTS*3];
	unsigned char steerPathFlags[MAX_STEER_POINTS];
	dtPolyRef steerPathPolys[MAX_STEER_POINTS];
	int nsteerPath = 0;
	navQuery->findStraightPath(startPos, endPos, path, pathSize,
							   steerPath, steerPathFlags, steerPathPolys, &nsteerPath, MAX_STEER_POINTS);
	if (!nsteerPath)
		return false;
		
	if (outPoints && outPointCount)
	{
		*outPointCount = nsteerPath;
		for (int i = 0; i < nsteerPath; ++i)
			dtVcopy(&outPoints[i*3], &steerPath[i*3]);
	}

	
	// Find vertex far enough to steer to.
	int ns = 0;
	while (ns < nsteerPath)
	{
		// Stop at Off-Mesh link or when point is further than slop away.
		if ((steerPathFlags[ns] & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ||
			!inRange(&steerPath[ns*3], startPos, minTargetDist, 1000.0f))
			break;
		ns++;
	}
	// Failed to find good point to steer to.
	if (ns >= nsteerPath)
		return false;
	
	dtVcopy(steerPos, &steerPath[ns*3]);
	steerPos[1] = startPos[1];
	steerPosFlag = steerPathFlags[ns];
	steerPosRef = steerPathPolys[ns];
	
	return true;
}


NavMeshTesterTool::NavMeshTesterTool() :
	m_sample(0),
	m_navMesh(0),
	m_navQuery(0),
	m_pathFindStatus(DT_FAILURE),
	m_toolMode(TOOLMODE_PATHFIND_STRAIGHT),//TOOLMODE_PATHFIND_FOLLOW),
	m_straightPathOptions(0),
	m_startRef(0),
	m_endRef(0),
	m_npolys(0),
	m_nstraightPath(0),
	m_nsmoothPath(0),
	m_nrandPoints(0),
	m_randPointsInCircle(false),
	m_hitResult(false),
	m_distanceToWall(0),
	m_sposSet(false),
	m_eposSet(false),
	m_pathIterNum(0),
	m_pathIterPolyCount(0),
	m_steerPointCount(0)
{
	m_filter.setIncludeFlags(SAMPLE_POLYFLAGS_ALL ^ SAMPLE_POLYFLAGS_DISABLED);
	m_filter.setExcludeFlags(SAMPLE_POLYFLAGS_DISABLED);

	m_polyPickExt[0] = 80;// 2;
	m_polyPickExt[1] = 160;// 4;
	m_polyPickExt[2] = 80;// 2;
	
	m_neighbourhoodRadius = 2.5f;
	m_randomRadius = 5.0f;
}

void NavMeshTesterTool::init(Sample* sample)
{
	m_sample = sample;
	m_navMesh = sample->getNavMesh();
	m_navQuery = sample->getNavMeshQuery();
	recalc();

	if (m_navQuery)
	{
		// Change costs.
		m_filter.setAreaCost(SAMPLE_POLYAREA_GROUND, 1.0f);
		m_filter.setAreaCost(SAMPLE_POLYAREA_WATER, 10.0f);
		m_filter.setAreaCost(SAMPLE_POLYAREA_ROAD, 1.0f);
		m_filter.setAreaCost(SAMPLE_POLYAREA_DOOR, 1.0f);
		m_filter.setAreaCost(SAMPLE_POLYAREA_GRASS, 2.0f);
		m_filter.setAreaCost(SAMPLE_POLYAREA_JUMP, 1.5f);
	}
	
	m_neighbourhoodRadius = sample->getAgentRadius() * 20.0f;
	m_randomRadius = sample->getAgentRadius() * 30.0f;
}

/*
void NavMeshTesterTool::handleMenu()
{
	if (imguiCheck("Pathfind Follow", m_toolMode == TOOLMODE_PATHFIND_FOLLOW))
	{
		m_toolMode = TOOLMODE_PATHFIND_FOLLOW;
		recalc();
	}
	if (imguiCheck("Pathfind Straight", m_toolMode == TOOLMODE_PATHFIND_STRAIGHT))
	{
		m_toolMode = TOOLMODE_PATHFIND_STRAIGHT;
		recalc();
	}
	if (m_toolMode == TOOLMODE_PATHFIND_STRAIGHT)
	{
		imguiIndent();
		imguiLabel("Vertices at crossings");
		if (imguiCheck("None", m_straightPathOptions == 0))
		{
			m_straightPathOptions = 0;
			recalc();
		}
		if (imguiCheck("Area", m_straightPathOptions == DT_STRAIGHTPATH_AREA_CROSSINGS))
		{
			m_straightPathOptions = DT_STRAIGHTPATH_AREA_CROSSINGS;
			recalc();
		}
		if (imguiCheck("All", m_straightPathOptions == DT_STRAIGHTPATH_ALL_CROSSINGS))
		{
			m_straightPathOptions = DT_STRAIGHTPATH_ALL_CROSSINGS;
			recalc();
		}

		imguiUnindent();
	}
	if (imguiCheck("Pathfind Sliced", m_toolMode == TOOLMODE_PATHFIND_SLICED))
	{
		m_toolMode = TOOLMODE_PATHFIND_SLICED;
		recalc();
	}

	imguiSeparator();

	if (imguiCheck("Distance to Wall", m_toolMode == TOOLMODE_DISTANCE_TO_WALL))
	{
		m_toolMode = TOOLMODE_DISTANCE_TO_WALL;
		recalc();
	}

	imguiSeparator();

	if (imguiCheck("Raycast", m_toolMode == TOOLMODE_RAYCAST))
	{
		m_toolMode = TOOLMODE_RAYCAST;
		recalc();
	}

	imguiSeparator();

	if (imguiCheck("Find Polys in Circle", m_toolMode == TOOLMODE_FIND_POLYS_IN_CIRCLE))
	{
		m_toolMode = TOOLMODE_FIND_POLYS_IN_CIRCLE;
		recalc();
	}
	if (imguiCheck("Find Polys in Shape", m_toolMode == TOOLMODE_FIND_POLYS_IN_SHAPE))
	{
		m_toolMode = TOOLMODE_FIND_POLYS_IN_SHAPE;
		recalc();
	}

	imguiSeparator();

	if (imguiCheck("Find Local Neighbourhood", m_toolMode == TOOLMODE_FIND_LOCAL_NEIGHBOURHOOD))
	{
		m_toolMode = TOOLMODE_FIND_LOCAL_NEIGHBOURHOOD;
		recalc();
	}

	imguiSeparator();
	
	if (imguiButton("Set Random Start"))
	{
		dtStatus status = m_navQuery->findRandomPoint(&m_filter, frand, &m_startRef, m_spos);
		if (dtStatusSucceed(status))
		{
			m_sposSet = true;
			recalc();
		}
	}
	if (imguiButton("Set Random End", m_sposSet))
	{
		if (m_sposSet)
		{
			dtStatus status = m_navQuery->findRandomPointAroundCircle(m_startRef, m_spos, m_randomRadius, &m_filter, frand, &m_endRef, m_epos);
			if (dtStatusSucceed(status))
			{
				m_eposSet = true;
				recalc();
			}
		}
	}

	imguiSeparator();

	if (imguiButton("Make Random Points"))
	{
		m_randPointsInCircle = false;
		m_nrandPoints = 0;
		for (int i = 0; i < MAX_RAND_POINTS; i++)
		{
			float pt[3];
			dtPolyRef ref;
			dtStatus status = m_navQuery->findRandomPoint(&m_filter, frand, &ref, pt);
			if (dtStatusSucceed(status))
			{
				dtVcopy(&m_randPoints[m_nrandPoints*3], pt);
				m_nrandPoints++;
			}
		}
	}
	if (imguiButton("Make Random Points Around", m_sposSet))
	{
		if (m_sposSet)
		{
			m_nrandPoints = 0;
			m_randPointsInCircle = true;
			for (int i = 0; i < MAX_RAND_POINTS; i++)
			{
				float pt[3];
				dtPolyRef ref;
				dtStatus status = m_navQuery->findRandomPointAroundCircle(m_startRef, m_spos, m_randomRadius, &m_filter, frand, &ref, pt);
				if (dtStatusSucceed(status))
				{
					dtVcopy(&m_randPoints[m_nrandPoints*3], pt);
					m_nrandPoints++;
				}
			}
		}
	}

	
	imguiSeparator();

	imguiLabel("Include Flags");

	imguiIndent();
	if (imguiCheck("Walk", (m_filter.getIncludeFlags() & SAMPLE_POLYFLAGS_WALK) != 0))
	{
		m_filter.setIncludeFlags(m_filter.getIncludeFlags() ^ SAMPLE_POLYFLAGS_WALK);
		recalc();
	}
	if (imguiCheck("Swim", (m_filter.getIncludeFlags() & SAMPLE_POLYFLAGS_SWIM) != 0))
	{
		m_filter.setIncludeFlags(m_filter.getIncludeFlags() ^ SAMPLE_POLYFLAGS_SWIM);
		recalc();
	}
	if (imguiCheck("Door", (m_filter.getIncludeFlags() & SAMPLE_POLYFLAGS_DOOR) != 0))
	{
		m_filter.setIncludeFlags(m_filter.getIncludeFlags() ^ SAMPLE_POLYFLAGS_DOOR);
		recalc();
	}
	if (imguiCheck("Jump", (m_filter.getIncludeFlags() & SAMPLE_POLYFLAGS_JUMP) != 0))
	{
		m_filter.setIncludeFlags(m_filter.getIncludeFlags() ^ SAMPLE_POLYFLAGS_JUMP);
		recalc();
	}
	imguiUnindent();

	imguiSeparator();
	imguiLabel("Exclude Flags");
	
	imguiIndent();
	if (imguiCheck("Walk", (m_filter.getExcludeFlags() & SAMPLE_POLYFLAGS_WALK) != 0))
	{
		m_filter.setExcludeFlags(m_filter.getExcludeFlags() ^ SAMPLE_POLYFLAGS_WALK);
		recalc();
	}
	if (imguiCheck("Swim", (m_filter.getExcludeFlags() & SAMPLE_POLYFLAGS_SWIM) != 0))
	{
		m_filter.setExcludeFlags(m_filter.getExcludeFlags() ^ SAMPLE_POLYFLAGS_SWIM);
		recalc();
	}
	if (imguiCheck("Door", (m_filter.getExcludeFlags() & SAMPLE_POLYFLAGS_DOOR) != 0))
	{
		m_filter.setExcludeFlags(m_filter.getExcludeFlags() ^ SAMPLE_POLYFLAGS_DOOR);
		recalc();
	}
	if (imguiCheck("Jump", (m_filter.getExcludeFlags() & SAMPLE_POLYFLAGS_JUMP) != 0))
	{
		m_filter.setExcludeFlags(m_filter.getExcludeFlags() ^ SAMPLE_POLYFLAGS_JUMP);
		recalc();
	}
	imguiUnindent();

	imguiSeparator();	
}
*/

void NavMeshTesterTool::findAPath(float* pStart, float* pEnd)
{
	// the terminals of this path (can change (below))
	bool bUseOriginalStartPos = false;
	bool bUseOriginalFinishPos = false;
	GGVECTOR3 vecVeryStart = GGVECTOR3(pStart[0], pStart[1], pStart[2]);
	GGVECTOR3 vecVeryFinish = GGVECTOR3(pEnd[0], pEnd[1], pEnd[2]);

	// optional transition points
	bool bOptionalTransitionPoint = false;
	GGVECTOR3 vecOptionalTransitionPoint;

	/* nav mesh now covers whole level, no need for this (and caused AI to move through walls as they created artificial paths at edges of navmesh!!)
	// new nav free zone system, can navigate within nav mesh cluster, or negotiate transition to free zone, or entirely navigate the free zone (straight line)
	bool bUseNavMeshPath = false;
	bool bStartInside = isWithinNavMesh (pStart[0], pStart[1], pStart[2]);
	bool bFinishInside = isWithinNavMesh (pEnd[0], pEnd[1], pEnd[2]);
	if (bStartInside == true && bFinishInside == true)
	{
		// traditional path finding within nav cluster
		bUseNavMeshPath = true;
	}
	else
	{
		// start or finish outside of nav cluster
		GGVECTOR3 vecRayPoint = vecVeryStart;
		GGVECTOR3 vecTrajectory = vecVeryFinish - vecRayPoint;
		float fSlices = GGVec3Length(&vecTrajectory) / 10.0f;
		GGVECTOR3 vecTrajectoryInc = vecTrajectory / fSlices;
		if (bStartInside == true)
		{
			// quick check to see if we are on the edge
			int iCount = 0;
			while (iCount < 10)
			{
				vecRayPoint += vecTrajectoryInc;
				bool bIsThisInNavCluster = isWithinNavMesh (vecRayPoint.x, vecRayPoint.y, vecRayPoint.z);
				if (bIsThisInNavCluster == false)
				{
					// one step toward finish leaves nav, so count as outside now!
					bStartInside = false;
				}
				iCount++;
			}
		}
		if (bStartInside == false)
		{
			// add transition point at border of nav mesh to get in
			int iCount = 0;
			while (iCount < (int)fSlices)
			{
				vecRayPoint += vecTrajectoryInc;
				bool bIsThisInNavCluster = isWithinNavMesh (vecRayPoint.x, vecRayPoint.y, vecRayPoint.z);
				if (bIsThisInNavCluster == true )
				{
					// replace start to one within nav cluster
					pStart[0] = vecRayPoint.x;
					pStart[1] = vecRayPoint.y;
					pStart[2] = vecRayPoint.z;
					bUseNavMeshPath = true;
					// and provide the original start pos as an extra
					bUseOriginalStartPos = true;
					break;
				}
				iCount++;
			}
		}
		if (bFinishInside == false && (bStartInside == true || (bStartInside == false && bUseNavMeshPath == true)))
		{
			// add transition point at border of nav mesh to get in
			vecRayPoint = GGVECTOR3(pStart[0], pStart[1], pStart[2]);
			vecTrajectory = vecVeryFinish - vecRayPoint;
			fSlices = GGVec3Length(&vecTrajectory) / 10.0f;
			vecTrajectoryInc = vecTrajectory / fSlices;
			int iCount = 0;
			while (iCount < (int)fSlices)
			{
				vecRayPoint += vecTrajectoryInc;
				bool bIsThisInNavCluster = isWithinNavMesh (vecRayPoint.x, vecRayPoint.y, vecRayPoint.z);
				if (bIsThisInNavCluster == false)
				{
					// when leave nav cluster, mark transition point
					vecRayPoint -= vecTrajectoryInc; // step back so back inside nav
					pEnd[0] = vecRayPoint.x;
					pEnd[1] = vecRayPoint.y;
					pEnd[2] = vecRayPoint.z;
					bUseNavMeshPath = true;
					// and provide the original start pos as an extra
					bUseOriginalFinishPos = true;
					break;
				}
				iCount++;
			}
		}
	}
	if (bUseNavMeshPath == true)
	{
	*/
	// path within nav cluster
	m_sposSet = true;
	dtVcopy(m_spos, pStart);
	m_eposSet = true;
	dtVcopy(m_epos, pEnd);
	recalc();
	/*
	}
	else
	{
		// entirely outside nav cluster and not crossing cluster - nav free zone straight line :)
		m_nstraightPath = 2;
		m_straightPath[0] = pStart[0];
		m_straightPath[1] = pStart[1];
		m_straightPath[2] = pStart[2];
		m_straightPath[3] = pEnd[0];
		m_straightPath[4] = pEnd[1];
		m_straightPath[5] = pEnd[2];
	}
	*/

	//
	// can add extra stuff to detect tree installations and plot a route around the little buggers - post EA!
	//

	// any path from above nav mesh/etc
	int iPointCount = m_nstraightPath;
	float* pfPointData = (float*)&m_straightPath[0];
	if (iPointCount <= 1)
		return;

	// create smoothed and augmented path array
	int iSlicesCount = 5;
	int iSmoothedPathCount = iPointCount * (iSlicesCount - 1);

	// add additional points to the path
	if (bUseOriginalStartPos == true) iSmoothedPathCount++;
	if (bUseOriginalFinishPos == true) iSmoothedPathCount+=2;

	// GG: a path whose rounded corners would not fit in m_straightPath (over a quarter of MAX_POLYS points) keeps its sharp
	// corners; they were written past the array's end
	if (iSmoothedPathCount > MAX_POLYS)
		return;

	// reformat path for smooth curves
	int iSmoothedPathIndex = 0;
	float* pSmoothedPath = new float[iSmoothedPathCount * 3];
	memset(pSmoothedPath, 0, sizeof(float)*iSmoothedPathCount * 3);

	// start with inserted start position if come from outside
	if (bUseOriginalStartPos == true)
	{
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 0) = vecVeryStart.x;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 1) = vecVeryStart.y;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 2) = vecVeryStart.z;
		iSmoothedPathIndex++;
	}

	// the first point (original nav mesh path)
	*(pSmoothedPath + (iSmoothedPathIndex * 3) + 0) = *(pfPointData + (0 * 3) + 0);
	*(pSmoothedPath + (iSmoothedPathIndex * 3) + 1) = *(pfPointData + (0 * 3) + 1);
	*(pSmoothedPath + (iSmoothedPathIndex * 3) + 2) = *(pfPointData + (0 * 3) + 2);
	iSmoothedPathIndex++;

	// create smooth corners
	for (int p = 0; p < iPointCount - 2; p++)
	{
		// original points A and B and C
		GGVECTOR3 vA = GGVECTOR3 (*(pfPointData + ((p + 0) * 3) + 0), *(pfPointData + ((p + 0) * 3) + 1), *(pfPointData + ((p + 0) * 3) + 2));
		GGVECTOR3 vB = GGVECTOR3 (*(pfPointData + ((p + 1) * 3) + 0), *(pfPointData + ((p + 1) * 3) + 1), *(pfPointData + ((p + 1) * 3) + 2));
		GGVECTOR3 vC = GGVECTOR3 (*(pfPointData + ((p + 2) * 3) + 0), *(pfPointData + ((p + 2) * 3) + 1), *(pfPointData + ((p + 2) * 3) + 2));

		// before and after dist
		GGVECTOR3 vBeforeDiff = vA - vB;
		float fBeforeDist = GGVec3Length(&vBeforeDiff) * 0.25f;
		if (fBeforeDist > 15.0f) fBeforeDist = 15.0f;
		GGVECTOR3 vAfterDiff = vC - vB;
		float fAfterDist = GGVec3Length(&vAfterDiff) * 0.25f;
		if (fAfterDist > 15.0f) fAfterDist = 15.0f;

		// find local pos around B
		GGVECTOR3 vAB = vA - vB; GGVec3Normalize(&vAB, &vAB); vAB *= fBeforeDist;
		GGVECTOR3 vCB = vC - vB; GGVec3Normalize(&vCB, &vCB); vCB *= fAfterDist;
		GGVECTOR3 vNewA = vB + vAB;
		GGVECTOR3 vNewC = vB + vCB;

		// define an arc using vector change from AB to BC
		GGVECTOR3 vStart = vB - vNewA;
		GGVECTOR3 vFinish = vNewC - vB;
		GGVECTOR3 vDirectionInc = vFinish - vStart;

		// calculate steps for vector direction and plot points
		float fArcDist = GGVec3Length(&vStart) + GGVec3Length(&vFinish);
		float fDistanceStep = fArcDist / (iSlicesCount - 2);
		GGVec3Normalize(&vDirectionInc, &vDirectionInc);
		vDirectionInc /= (iSlicesCount-1);

		// first point at B
		GGVECTOR3 vAt = vNewA;
		GGVECTOR3 vArc[8];
		int iArcCount = 0;
		vArc[iArcCount++] = vAt;

		// work out curve through 3D space
		GGVECTOR3 vDirection;
		GGVec3Normalize(&vDirection, &vStart);
		for (int slice = 1; slice < iSlicesCount-1; slice++)
		{
			vDirection += vDirectionInc;
			GGVec3Normalize(&vDirection, &vDirection);
			vAt += vDirection * fDistanceStep;
			vArc[iArcCount++] = vAt;
		}

		// GG: a curve that cuts into a blocking box (a corner the path takes round a closed door or a parked vehicle, or the step
		// out of the box the start stands in) is left as the sharp corner
		bool bArcBlocked = false;
		if (g_bGGNavBoxExits == true)
		{
			for (int i = 0; i < iArcCount && bArcBlocked == false; i++)
			{
				GGVECTOR3 vTo = (i + 1 < iArcCount) ? vArc[i + 1] : vNewC;
				if (DoesLineGoThroughBlocker(vArc[i].x, vArc[i].y, vArc[i].z, vTo.x, vTo.y, vTo.z) == true)
					bArcBlocked = true;
			}
		}
		if (bArcBlocked == true)
		{
			m_ggArcsKept++;
			vArc[0] = vB;
			iArcCount = 1;
		}
		for (int i = 0; i < iArcCount; i++)
		{
			*(pSmoothedPath + (iSmoothedPathIndex * 3) + 0) = vArc[i].x;
			*(pSmoothedPath + (iSmoothedPathIndex * 3) + 1) = vArc[i].y;
			*(pSmoothedPath + (iSmoothedPathIndex * 3) + 2) = vArc[i].z;
			iSmoothedPathIndex++;
		}
	}

	// and the final point
	int lastp = iPointCount - 1;
	*(pSmoothedPath + (iSmoothedPathIndex * 3) + 0) = *(pfPointData + (lastp * 3) + 0);
	*(pSmoothedPath + (iSmoothedPathIndex * 3) + 1) = *(pfPointData + (lastp * 3) + 1);
	*(pSmoothedPath + (iSmoothedPathIndex * 3) + 2) = *(pfPointData + (lastp * 3) + 2);
	iSmoothedPathIndex++;

	// finish with inserted end position if going to outside
	if (bUseOriginalFinishPos == true)
	{
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 0) = vecVeryFinish.x;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 1) = vecVeryFinish.y;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 2) = vecVeryFinish.z;
		iSmoothedPathIndex++;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 0) = vecVeryFinish.x;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 1) = vecVeryFinish.y;
		*(pSmoothedPath + (iSmoothedPathIndex * 3) + 2) = vecVeryFinish.z;
		iSmoothedPathIndex++;
	}

	// GG: the rounded path checked against the blocking boxes as a whole (a curve's last point joins the next corner's first off
	// the line checked before): one that touches a box is dropped and the path keeps its sharp corners
	if (g_bGGNavBoxExits == true)
	{
		for (int i = (m_ggStartBox >= 0) ? 1 : 0; i + 1 < iSmoothedPathIndex; i++)
		{
			const float* p = pSmoothedPath + (i * 3);
			const float* q = pSmoothedPath + ((i + 1) * 3);
			if (DoesLineGoThroughBlocker(p[0], p[1], p[2], q[0], q[1], q[2]) == true)
			{
				m_ggArcsKept = -1;
				delete[] pSmoothedPath;
				return;
			}
		}
	}

	// replace old one
	m_nstraightPath = iSmoothedPathIndex;
	memset(m_straightPath, 0, sizeof(m_straightPath));
	for (int i = 0; i < m_nstraightPath * 3; i++)
		m_straightPath[i] = pSmoothedPath[i];

	// GG: the rounded path was never freed, a leak on every path search
	delete[] pSmoothedPath;
}

void NavMeshTesterTool::getPathResult(int* piPointCount, float** ppfPointData)
{
	if (m_toolMode == TOOLMODE_PATHFIND_FOLLOW)
	{
		*piPointCount = m_nsmoothPath;
		*ppfPointData = (float*)&m_smoothPath[0];
	}
	if (m_toolMode == TOOLMODE_PATHFIND_STRAIGHT)
	{
		*piPointCount = m_nstraightPath;
		*ppfPointData = (float*)&m_straightPath[0];
	}
}

bool NavMeshTesterTool::isWithinNavMeshEx(float fX, float fY, float fZ, float* pvecNearestPt, bool* isOverPoly)
{
	float fPoint[3] = { fX, fY, fZ };
	dtPolyRef ref = NULL;
	m_navQuery->findNearestPoly(fPoint, m_polyPickExt, &m_filter, &ref, pvecNearestPt, isOverPoly);
	if (ref)
		return true;
	else
		return false;
}

bool NavMeshTesterTool::isWithinNavMesh(float fX, float fY, float fZ)
{
	return isWithinNavMeshEx(fX, fY, fZ, 0, 0);
}

float NavMeshTesterTool::getYFromPos(float fX, float fY, float fZ)
{
	float fPoint[3] = { fX, fY, fZ };
	float fNearestPoint[3] = { fX, fY, fZ };
	dtPolyRef ref = NULL;
	m_navQuery->findNearestPoly(fPoint, m_polyPickExt, &m_filter, &ref, fNearestPoint);
	if (ref)
		return fNearestPoint[1];
	else
		return fY;
}

/*
void NavMeshTesterTool::handleClick(const float* /*s*[leeremovedslash], const float* p, bool shift)
{
	if (shift)
	{
		m_sposSet = true;
		dtVcopy(m_spos, p);
	}
	else
	{
		m_eposSet = true;
		dtVcopy(m_epos, p);
	}
	recalc();
}

void NavMeshTesterTool::handleStep()
{
}

void NavMeshTesterTool::handleToggle()
{
	// TODO: merge separate to a path iterator. Use same code in recalc() too.
	if (m_toolMode != TOOLMODE_PATHFIND_FOLLOW)
		return;
		
	if (!m_sposSet || !m_eposSet || !m_startRef || !m_endRef)
		return;
		
	static const float STEP_SIZE = 0.5f;
	static const float SLOP = 0.01f;

	if (m_pathIterNum == 0)
	{
		m_navQuery->findPath(m_startRef, m_endRef, m_spos, m_epos, &m_filter, m_polys, &m_npolys, MAX_POLYS);
		m_nsmoothPath = 0;

		m_pathIterPolyCount = m_npolys;
		if (m_pathIterPolyCount)
			memcpy(m_pathIterPolys, m_polys, sizeof(dtPolyRef)*m_pathIterPolyCount); 
		
		if (m_pathIterPolyCount)
		{
			// Iterate over the path to find smooth path on the detail mesh surface.
			m_navQuery->closestPointOnPoly(m_startRef, m_spos, m_iterPos, 0);
			m_navQuery->closestPointOnPoly(m_pathIterPolys[m_pathIterPolyCount-1], m_epos, m_targetPos, 0);
			
			m_nsmoothPath = 0;
			
			dtVcopy(&m_smoothPath[m_nsmoothPath*3], m_iterPos);
			m_nsmoothPath++;
		}
	}
	
	dtVcopy(m_prevIterPos, m_iterPos);

	m_pathIterNum++;

	if (!m_pathIterPolyCount)
		return;

	if (m_nsmoothPath >= MAX_SMOOTH)
		return;

	// Move towards target a small advancement at a time until target reached or
	// when ran out of memory to store the path.

	// Find location to steer towards.
	float steerPos[3];
	unsigned char steerPosFlag;
	dtPolyRef steerPosRef;
		
	if (!getSteerTarget(m_navQuery, m_iterPos, m_targetPos, SLOP,
						m_pathIterPolys, m_pathIterPolyCount, steerPos, steerPosFlag, steerPosRef,
						m_steerPoints, &m_steerPointCount))
		return;
		
	dtVcopy(m_steerPos, steerPos);
	
	bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END) ? true : false;
	bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ? true : false;
		
	// Find movement delta.
	float delta[3], len;
	dtVsub(delta, steerPos, m_iterPos);
	len = sqrtf(dtVdot(delta,delta));
	// If the steer target is end of path or off-mesh link, do not move past the location.
	if ((endOfPath || offMeshConnection) && len < STEP_SIZE)
		len = 1;
	else
		len = STEP_SIZE / len;
	float moveTgt[3];
	dtVmad(moveTgt, m_iterPos, delta, len);
		
	// Move
	float result[3];
	dtPolyRef visited[16];
	int nvisited = 0;
	m_navQuery->moveAlongSurface(m_pathIterPolys[0], m_iterPos, moveTgt, &m_filter,
								 result, visited, &nvisited, 16);
	m_pathIterPolyCount = fixupCorridor(m_pathIterPolys, m_pathIterPolyCount, MAX_POLYS, visited, nvisited);
	m_pathIterPolyCount = fixupShortcuts(m_pathIterPolys, m_pathIterPolyCount, m_navQuery);

	float h = 0;
	m_navQuery->getPolyHeight(m_pathIterPolys[0], result, &h);
	result[1] = h;
	dtVcopy(m_iterPos, result);
	
	// Handle end of path and off-mesh links when close enough.
	if (endOfPath && inRange(m_iterPos, steerPos, SLOP, 1.0f))
	{
		// Reached end of path.
		dtVcopy(m_iterPos, m_targetPos);
		if (m_nsmoothPath < MAX_SMOOTH)
		{
			dtVcopy(&m_smoothPath[m_nsmoothPath*3], m_iterPos);
			m_nsmoothPath++;
		}
		return;
	}
	else if (offMeshConnection && inRange(m_iterPos, steerPos, SLOP, 1.0f))
	{
		// Reached off-mesh connection.
		float startPos[3], endPos[3];
		
		// Advance the path up to and over the off-mesh connection.
		dtPolyRef prevRef = 0, polyRef = m_pathIterPolys[0];
		int npos = 0;
		while (npos < m_pathIterPolyCount && polyRef != steerPosRef)
		{
			prevRef = polyRef;
			polyRef = m_pathIterPolys[npos];
			npos++;
		}
		for (int i = npos; i < m_pathIterPolyCount; ++i)
			m_pathIterPolys[i-npos] = m_pathIterPolys[i];
		m_pathIterPolyCount -= npos;
				
		// Handle the connection.
		dtStatus status = m_navMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, startPos, endPos);
		if (dtStatusSucceed(status))
		{
			if (m_nsmoothPath < MAX_SMOOTH)
			{
				dtVcopy(&m_smoothPath[m_nsmoothPath*3], startPos);
				m_nsmoothPath++;
				// Hack to make the dotted path not visible during off-mesh connection.
				if (m_nsmoothPath & 1)
				{
					dtVcopy(&m_smoothPath[m_nsmoothPath*3], startPos);
					m_nsmoothPath++;
				}
			}
			// Move position at the other side of the off-mesh link.
			dtVcopy(m_iterPos, endPos);
			float eh = 0.0f;
			m_navQuery->getPolyHeight(m_pathIterPolys[0], m_iterPos, &eh);
			m_iterPos[1] = eh;
		}
	}
	
	// Store results.
	if (m_nsmoothPath < MAX_SMOOTH)
	{
		dtVcopy(&m_smoothPath[m_nsmoothPath*3], m_iterPos);
		m_nsmoothPath++;
	}

}

void NavMeshTesterTool::handleUpdate(const float /*dt*[leeremovedslash])
{
	if (m_toolMode == TOOLMODE_PATHFIND_SLICED)
	{
		if (dtStatusInProgress(m_pathFindStatus))
		{
			m_pathFindStatus = m_navQuery->updateSlicedFindPath(1,0);
		}
		if (dtStatusSucceed(m_pathFindStatus))
		{
			m_navQuery->finalizeSlicedFindPath(m_polys, &m_npolys, MAX_POLYS);
			m_nstraightPath = 0;
			if (m_npolys)
			{
				// In case of partial path, make sure the end point is clamped to the last polygon.
				float epos[3];
				dtVcopy(epos, m_epos);
				if (m_polys[m_npolys-1] != m_endRef)
				m_navQuery->closestPointOnPoly(m_polys[m_npolys-1], m_epos, epos, 0);

				m_navQuery->findStraightPath(m_spos, epos, m_polys, m_npolys,
											 m_straightPath, m_straightPathFlags,
											 m_straightPathPolys, &m_nstraightPath, MAX_POLYS, DT_STRAIGHTPATH_ALL_CROSSINGS);
			}
			 
			m_pathFindStatus = DT_FAILURE;
		}
	}
}
*/

void NavMeshTesterTool::reset()
{
	m_startRef = 0;
	m_endRef = 0;
	m_npolys = 0;
	m_nstraightPath = 0;
	m_nsmoothPath = 0;
	memset(m_hitPos, 0, sizeof(m_hitPos));
	memset(m_hitNormal, 0, sizeof(m_hitNormal));
	m_distanceToWall = 0;
}

bool NavMeshTesterTool::ggBoxExit(int iBox, const float* pPos, dtPolyRef posRef, const float* pOther, bool bGoal, float* pOut, dtPolyRef* pOutRef)
{
	// GG: the nearest point on the navmesh just outside box iBox from pPos, straight out from one of its faces, in no box and
	// walkable from pPos. A start leaves by its nearest face (a closed door's own side of it); a goal by the face nearest it
	// counting half the way from there to pOther, the start (a goal deep in a vehicle is met on the start's side, one near its far
	// side on that side)
	const float fMargin = 20.0f;
	float face[4][3], score[4];
	int order[4] = { 0, 1, 2, 3 };
	for (int f = 0; f < 4; f++)
	{
		float fDist = 0;
		if (GGNavBlockerFacePoint(iBox, pPos[0], pPos[2], f, fMargin, &face[f][0], &face[f][2], &fDist) == false) return false;
		face[f][1] = pPos[1];
		score[f] = fDist;
		if (bGoal == true) score[f] += 0.5f * dtVdist2D(face[f], pOther);
	}
	for (int i = 1; i < 4; i++)
		for (int j = i; j > 0 && score[order[j]] < score[order[j - 1]]; j--)
		{
			int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
		}
	float from[3];
	if (dtStatusFailed(m_navQuery->closestPointOnPoly(posRef, pPos, from, 0))) return false;
	for (int i = 0; i < 4; i++)
	{
		const int f = order[i];
		dtPolyRef ref = 0;
		float nearest[3];
		m_navQuery->findNearestPoly(face[f], m_polyPickExt, &m_filter, &ref, nearest);
		if (ref == 0) continue;
		if (dtVdist2D(nearest, face[f]) > fMargin * 3) continue;
		if (GGNavPastBlockerFace(iBox, nearest[0], nearest[2], f) == false) continue;
		if (GGNavPointInBlocker(nearest[0], nearest[1], nearest[2]) >= 0) continue;
		if (ggWalkable(posRef, from, nearest) == false) continue;
		dtVcopy(pOut, nearest);
		*pOutRef = ref;
		return true;
	}
	return false;
}

int NavMeshTesterTool::ggCorridorIndex(int iPoint)
{
	// GG: the corridor polygon a straight path point leads into (where its line onward starts): its own polygon ref names it; the
	// first point's is the first polygon, the end point's (ref 0) past the last
	if (iPoint <= 0) return 0;
	const dtPolyRef ref = m_straightPathPolys[iPoint];
	if (ref == 0) return m_npolys;
	for (int j = 0; j < m_npolys; j++)
		if (m_polys[j] == ref) return j;
	return -1;
}

bool NavMeshTesterTool::ggWalkable(dtPolyRef fromRef, const float* pFrom, const float* pTo)
{
	// GG: a straight line on the navmesh from pFrom (in polygon fromRef) reaches pTo without meeting its edge
	if (fromRef == 0) return false;
	dtRaycastHit hit;
	memset(&hit, 0, sizeof(hit));
	if (dtStatusFailed(m_navQuery->raycast(fromRef, pFrom, pTo, &m_filter, 0, &hit))) return false;
	return hit.t == FLT_MAX;
}

int NavMeshTesterTool::ggInsertPoints(int iAfter, const float* pPoints, const dtPolyRef* pRefs, int iCount)
{
	// GG: points put into the straight path after point iAfter (-1 the front) with their polygons; the number added, or -1 if
	// the path has no room for them
	if (iCount <= 0) return 0;
	if (m_nstraightPath + iCount > MAX_POLYS) return -1;
	const int iAt = iAfter + 1;
	const int iMove = m_nstraightPath - iAt;
	if (iMove > 0)
	{
		memmove(&m_straightPath[(iAt + iCount) * 3], &m_straightPath[iAt * 3], sizeof(float) * 3 * iMove);
		memmove(&m_straightPathFlags[iAt + iCount], &m_straightPathFlags[iAt], sizeof(unsigned char) * iMove);
		memmove(&m_straightPathPolys[iAt + iCount], &m_straightPathPolys[iAt], sizeof(dtPolyRef) * iMove);
	}
	for (int i = 0; i < iCount; i++)
	{
		dtVcopy(&m_straightPath[(iAt + i) * 3], &pPoints[i * 3]);
		m_straightPathFlags[iAt + i] = 0;
		m_straightPathPolys[iAt + i] = pRefs[i];
	}
	m_nstraightPath += iCount;
	return iCount;
}

void NavMeshTesterTool::ggRemovePoints(int iFrom, int iCount)
{
	// GG: straight path points iFrom on, iCount of them, taken out with their flags and polygons
	if (iFrom < 0 || iCount <= 0 || iFrom + iCount > m_nstraightPath) return;
	const int iMove = m_nstraightPath - (iFrom + iCount);
	if (iMove > 0)
	{
		memmove(&m_straightPath[iFrom * 3], &m_straightPath[(iFrom + iCount) * 3], sizeof(float) * 3 * iMove);
		memmove(&m_straightPathFlags[iFrom], &m_straightPathFlags[iFrom + iCount], sizeof(unsigned char) * iMove);
		memmove(&m_straightPathPolys[iFrom], &m_straightPathPolys[iFrom + iCount], sizeof(dtPolyRef) * iMove);
	}
	m_nstraightPath -= iCount;
}

int NavMeshTesterTool::ggRerouteBySearch(int iPoint)
{
	// GG: straight path line iPoint to iPoint+1 crosses a blocking box: the line goes the search's own way instead. Through each
	// corridor polygon it passes, the point the search entered it by (each step between those the search took clear of the
	// boxes) and the portal it leaves by, in turn, so each point and the next lie in one polygon; then from each point the
	// furthest of the next ones (up to 64) reached clear of the boxes and walkable on the navmesh. The points added, or -1 if
	// some point reaches none
	const int a = ggCorridorIndex(iPoint);
	const int b = ggCorridorIndex(iPoint + 1);
	static const int MAX_WAY = 512;
	if (a < 0 || b < 0 || b <= a || m_ggNodeCount != m_npolys || (b - a) * 2 + 2 > MAX_WAY) return -1;
	float way[MAX_WAY * 3];
	dtPolyRef wayRef[MAX_WAY];
	int n = 0;
	dtVcopy(&way[0], &m_straightPath[iPoint * 3]);
	wayRef[n++] = m_polys[a];
	for (int j = a; j < b; j++)
	{
		// the search's point in polygon j (the start's own for the first polygon, already the line's first point)
		if (j > 0)
		{
			dtVcopy(&way[n * 3], &m_ggNodePos[j * 3]);
			wayRef[n++] = m_polys[j];
		}
		// the portal out of it, but for the last polygon, which the line's end point leaves by
		if (j + 1 < b)
		{
			if (dtStatusFailed(m_navQuery->ggPortalMidPoint(m_polys[j], m_polys[j + 1], &way[n * 3]))) return -1;
			wayRef[n++] = m_polys[j + 1];
		}
	}
	dtVcopy(&way[n * 3], &m_straightPath[(iPoint + 1) * 3]);
	wayRef[n++] = m_straightPathPolys[iPoint + 1];
	float keep[MAX_WAY * 3];
	dtPolyRef keepRef[MAX_WAY];
	int nKeep = 0;
	int cur = 0;
	while (cur < n - 1)
	{
		// a point and the next lie in one convex polygon, so only a longer hop needs its walk checked
		int next = -1;
		const int iFurthest = (cur + 64 < n - 1) ? cur + 64 : n - 1;
		for (int m = iFurthest; m > cur && next < 0; m--)
		{
			const float* p = &way[cur * 3];
			const float* q = &way[m * 3];
			if (DoesLineGoThroughBlocker(p[0], p[1], p[2], q[0], q[1], q[2]) == true) continue;
			if (m > cur + 1 && ggWalkable(wayRef[cur], p, q) == false) continue;
			next = m;
		}
		if (next < 0) return -1;
		if (next < n - 1)
		{
			dtVcopy(&keep[nKeep * 3], &way[next * 3]);
			keepRef[nKeep++] = wayRef[next];
		}
		cur = next;
	}
	return ggInsertPoints(iPoint, keep, keepRef, nKeep);
}

int NavMeshTesterTool::ggRerouteRoundBoxes(int iPoint)
{
	// GG: straight path line iPoint to iPoint+1 taken round the blocking boxes near it instead: the shortest way from its start
	// to its end through their corners (each pushed out 20 units along both sides, on the navmesh and in no box), every hop
	// clear of the boxes and walkable on the navmesh. For a line within one polygon, or one the search's own way can't replace
	// (vehicles parked close together). The points added, or -1 if there is no such way
	const float fMargin = 20.0f;
	const float* p = &m_straightPath[iPoint * 3];
	const float* q = &m_straightPath[(iPoint + 1) * 3];
	static const int MAX_BOX = 8;
	int boxes[MAX_BOX];
	const int nBox = GGNavBlockersNear(p[0], p[2], q[0], q[2], 400.0f, boxes, MAX_BOX);
	if (nBox <= 0) return -1;
	static const int MAX_V = 2 + MAX_BOX * 4;
	float v[MAX_V][3];
	dtPolyRef vRef[MAX_V];
	int nv = 0;
	dtVcopy(v[nv], p);
	vRef[nv++] = m_straightPathPolys[iPoint];
	dtVcopy(v[nv], q);
	vRef[nv++] = 0;
	for (int i = 0; i < nBox; i++)
	{
		for (int c = 0; c < 4; c++)
		{
			float pt[3];
			if (GGNavBlockerCorner(boxes[i], c, fMargin, &pt[0], &pt[2]) == false) continue;
			pt[1] = (p[1] + q[1]) * 0.5f;
			float nearest[3];
			dtPolyRef ref = 0;
			m_navQuery->findNearestPoly(pt, m_polyPickExt, &m_filter, &ref, nearest);
			if (ref == 0 || dtVdist2D(nearest, pt) > fMargin) continue;
			if (GGNavPointInBlocker(nearest[0], nearest[1], nearest[2]) >= 0) continue;
			dtVcopy(v[nv], nearest);
			vRef[nv++] = ref;
		}
	}
	// A* over the corners, each hop tested as it is first wanted
	float g[MAX_V];
	int parent[MAX_V];
	bool bClosed[MAX_V];
	for (int i = 0; i < nv; i++) { g[i] = FLT_MAX; parent[i] = -1; bClosed[i] = false; }
	g[0] = 0;
	for (;;)
	{
		int cur = -1;
		float fBest = FLT_MAX;
		for (int i = 0; i < nv; i++)
		{
			if (bClosed[i] || g[i] == FLT_MAX) continue;
			const float f = g[i] + dtVdist(v[i], v[1]);
			if (f < fBest) { fBest = f; cur = i; }
		}
		if (cur < 0)
		{
			static int iLogCount = 0;
			if (iLogCount < 40)
			{
				iLogCount++;
				int iReached = 0;
				for (int i = 0; i < nv; i++) if (bClosed[i]) iReached++;
				extern void timestampactivity(int i, char* desc_s);
				char pLog[512];
				sprintf_s(pLog, sizeof(pLog), "Navmesh path thrown out: line %d of %d, %.0f,%.0f,%.0f to %.0f,%.0f,%.0f, crosses a closed door or parked vehicle and no way round was found (%d boxes near, %d of %d corners usable, %d reached)",
					iPoint, m_nstraightPath, p[0], p[1], p[2], q[0], q[1], q[2], nBox, nv - 2, nBox * 4, iReached);
				timestampactivity(0, pLog);
			}
			return -1;
		}
		if (cur == 1) break;
		bClosed[cur] = true;
		for (int u = 1; u < nv; u++)
		{
			if (u == cur || bClosed[u]) continue;
			if (cur == 0 && u == 1) continue; // the line that crossed a box
			const float fG = g[cur] + dtVdist(v[cur], v[u]);
			if (fG >= g[u]) continue;
			if (DoesLineGoThroughBlocker(v[cur][0], v[cur][1], v[cur][2], v[u][0], v[u][1], v[u][2]) == true) continue;
			if (ggWalkable(vRef[cur], v[cur], v[u]) == false) continue;
			g[u] = fG;
			parent[u] = cur;
		}
	}
	float pts[MAX_V * 3];
	dtPolyRef refs[MAX_V];
	int iCount = 0;
	for (int i = parent[1]; i > 0; i = parent[i]) iCount++;
	int k = iCount;
	for (int i = parent[1]; i > 0; i = parent[i])
	{
		k--;
		dtVcopy(&pts[k * 3], v[i]);
		refs[k] = vRef[i];
	}
	return ggInsertPoints(iPoint, pts, refs, iCount);
}

void NavMeshTesterTool::recalc()
{
	if (!m_navMesh)
		return;
	
	if (m_sposSet)
		m_navQuery->findNearestPoly(m_spos, m_polyPickExt, &m_filter, &m_startRef, 0);
	else
		m_startRef = 0;
	
	if (m_eposSet)
		m_navQuery->findNearestPoly(m_epos, m_polyPickExt, &m_filter, &m_endRef, 0);
	else
		m_endRef = 0;
	
	m_pathFindStatus = DT_FAILURE;
	
	if (m_toolMode == TOOLMODE_PATHFIND_FOLLOW)
	{
		m_pathIterNum = 0;
		if (m_sposSet && m_eposSet && m_startRef && m_endRef)
		{
#ifdef DUMP_REQS
			printf("pi  %f %f %f  %f %f %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], m_epos[0],m_epos[1],m_epos[2],
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags()); 
#endif

			m_navQuery->findPath(m_startRef, m_endRef, m_spos, m_epos, &m_filter, m_polys, &m_npolys, MAX_POLYS);

			m_nsmoothPath = 0;

			if (m_npolys)
			{
				// Iterate over the path to find smooth path on the detail mesh surface.
				dtPolyRef polys[MAX_POLYS];
				memcpy(polys, m_polys, sizeof(dtPolyRef)*m_npolys); 
				int npolys = m_npolys;
				
				float iterPos[3], targetPos[3];
				m_navQuery->closestPointOnPoly(m_startRef, m_spos, iterPos, 0);
				m_navQuery->closestPointOnPoly(polys[npolys-1], m_epos, targetPos, 0);
				
				static const float STEP_SIZE = 0.5f;
				static const float SLOP = 0.01f;
				
				m_nsmoothPath = 0;
				
				dtVcopy(&m_smoothPath[m_nsmoothPath*3], iterPos);
				m_nsmoothPath++;
				
				// Move towards target a small advancement at a time until target reached or
				// when ran out of memory to store the path.
				while (npolys && m_nsmoothPath < MAX_SMOOTH)
				{
					// Find location to steer towards.
					float steerPos[3];
					unsigned char steerPosFlag;
					dtPolyRef steerPosRef;
					
					if (!getSteerTarget(m_navQuery, iterPos, targetPos, SLOP,
										polys, npolys, steerPos, steerPosFlag, steerPosRef))
						break;
					
					bool endOfPath = (steerPosFlag & DT_STRAIGHTPATH_END) ? true : false;
					bool offMeshConnection = (steerPosFlag & DT_STRAIGHTPATH_OFFMESH_CONNECTION) ? true : false;
					
					// Find movement delta.
					float delta[3], len;
					dtVsub(delta, steerPos, iterPos);
					len = dtMathSqrtf(dtVdot(delta, delta));
					// If the steer target is end of path or off-mesh link, do not move past the location.
					if ((endOfPath || offMeshConnection) && len < STEP_SIZE)
						len = 1;
					else
						len = STEP_SIZE / len;
					float moveTgt[3];
					dtVmad(moveTgt, iterPos, delta, len);
					
					// Move
					float result[3];
					dtPolyRef visited[16];
					int nvisited = 0;
					m_navQuery->moveAlongSurface(polys[0], iterPos, moveTgt, &m_filter,
												 result, visited, &nvisited, 16);

					npolys = fixupCorridor(polys, npolys, MAX_POLYS, visited, nvisited);
					npolys = fixupShortcuts(polys, npolys, m_navQuery);

					float h = 0;
					m_navQuery->getPolyHeight(polys[0], result, &h);
					result[1] = h;
					dtVcopy(iterPos, result);

					// Handle end of path and off-mesh links when close enough.
					if (endOfPath && inRange(iterPos, steerPos, SLOP, 1.0f))
					{
						// Reached end of path.
						dtVcopy(iterPos, targetPos);
						if (m_nsmoothPath < MAX_SMOOTH)
						{
							dtVcopy(&m_smoothPath[m_nsmoothPath*3], iterPos);
							m_nsmoothPath++;
						}
						break;
					}
					else if (offMeshConnection && inRange(iterPos, steerPos, SLOP, 1.0f))
					{
						// Reached off-mesh connection.
						float startPos[3], endPos[3];
						
						// Advance the path up to and over the off-mesh connection.
						dtPolyRef prevRef = 0, polyRef = polys[0];
						int npos = 0;
						while (npos < npolys && polyRef != steerPosRef)
						{
							prevRef = polyRef;
							polyRef = polys[npos];
							npos++;
						}
						for (int i = npos; i < npolys; ++i)
							polys[i-npos] = polys[i];
						npolys -= npos;
						
						// Handle the connection.
						dtStatus status = m_navMesh->getOffMeshConnectionPolyEndPoints(prevRef, polyRef, startPos, endPos);
						if (dtStatusSucceed(status))
						{
							if (m_nsmoothPath < MAX_SMOOTH)
							{
								dtVcopy(&m_smoothPath[m_nsmoothPath*3], startPos);
								m_nsmoothPath++;
								// Hack to make the dotted path not visible during off-mesh connection.
								if (m_nsmoothPath & 1)
								{
									dtVcopy(&m_smoothPath[m_nsmoothPath*3], startPos);
									m_nsmoothPath++;
								}
							}
							// Move position at the other side of the off-mesh link.
							dtVcopy(iterPos, endPos);
							float eh = 0.0f;
							m_navQuery->getPolyHeight(polys[0], iterPos, &eh);
							iterPos[1] = eh;
						}
					}
					
					// Store results.
					if (m_nsmoothPath < MAX_SMOOTH)
					{
						dtVcopy(&m_smoothPath[m_nsmoothPath*3], iterPos);
						m_nsmoothPath++;
					}
				}
			}

		}
		else
		{
			m_npolys = 0;
			m_nsmoothPath = 0;
		}
	}
	else if (m_toolMode == TOOLMODE_PATHFIND_STRAIGHT)
	{
		// GG: a start or goal inside a blocking box (a closed door or a parked vehicle): no step may leave or enter a box
		// (DoesLineGoThroughBlocker), so the search runs from (to) the nearest point on the navmesh just outside it; the start
		// itself goes back at the front of the path after the door check, its first line stepping out of the box
		float ggSpos[3], ggEpos[3];
		dtVcopy(ggSpos, m_spos);
		dtVcopy(ggEpos, m_epos);
		dtPolyRef ggStartRef = m_startRef;
		dtPolyRef ggEndRef = m_endRef;
		m_ggStartBox = m_ggGoalBox = -1;
		m_ggRerouted = m_ggRerouteFailed = m_ggArcsKept = 0;
		if (g_bGGNavBoxExits == true && m_sposSet && m_eposSet && m_startRef && m_endRef)
		{
			int iBox = GGNavPointInBlocker(m_spos[0], m_spos[1], m_spos[2]);
			if (iBox >= 0 && ggBoxExit(iBox, m_spos, m_startRef, m_epos, false, ggSpos, &ggStartRef) == true)
				m_ggStartBox = iBox;
			iBox = GGNavPointInBlocker(m_epos[0], m_epos[1], m_epos[2]);
			if (iBox >= 0 && ggBoxExit(iBox, m_epos, m_endRef, ggSpos, true, ggEpos, &ggEndRef) == true)
				m_ggGoalBox = iBox;
		}
		if (m_sposSet && m_eposSet && ggStartRef && ggEndRef)
		{
#ifdef DUMP_REQS
			printf("ps  %f %f %f  %f %f %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], m_epos[0],m_epos[1],m_epos[2],
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags()); 
#endif
			dtStatus pathStatus = m_navQuery->findPath(ggStartRef, ggEndRef, ggSpos, ggEpos, &m_filter, m_polys, &m_npolys, MAX_POLYS);
			// GG: the points the search stepped between, for a line the string pull cuts across a box (ggRerouteBySearch)
			m_ggNodeCount = m_navQuery->ggGetPathNodePositions(m_ggNodePos, MAX_POLYS);

			// log a path cut short by the search or path size limits (not by a blocked or missing route), a limited number of times
			if (dtStatusDetail(pathStatus, DT_OUT_OF_NODES) || dtStatusDetail(pathStatus, DT_BUFFER_TOO_SMALL))
			{
				static int iPathLimitLogCount = 0;
				if (iPathLimitLogCount < 50)
				{
					iPathLimitLogCount++;
					extern void timestampactivity(int i, char* desc_s);
					char pLogLine[512];
					sprintf_s(pLogLine, 512, "Navmesh path cut short:%s%s %d polygons, from %.0f,%.0f to %.0f,%.0f",
						dtStatusDetail(pathStatus, DT_OUT_OF_NODES) ? " ran out of search nodes," : "",
						dtStatusDetail(pathStatus, DT_BUFFER_TOO_SMALL) ? " longer than MAX_POLYS," : "",
						m_npolys, m_spos[0], m_spos[2], m_epos[0], m_epos[2]);
					timestampactivity(0, pLogLine);
				}
			}

			m_nstraightPath = 0;
			if (m_npolys)
			{
				// In case of partial path, make sure the end point is clamped to the last polygon.
				float epos[3];
				dtVcopy(epos, ggEpos);
				if (m_polys[m_npolys-1] != ggEndRef)
					m_navQuery->closestPointOnPoly(m_polys[m_npolys-1], ggEpos, epos, 0);
				
				m_navQuery->findStraightPath(ggSpos, epos, m_polys, m_npolys,
											 m_straightPath, m_straightPathFlags,
											 m_straightPathPolys, &m_nstraightPath, MAX_POLYS, m_straightPathOptions);

				// after 'findStraightPath' string pull, check blockerlist again and prevent path if crosses!
				if (m_nstraightPath > 0)
				{
					bool bBlockedByDoor = false;
					for (int pointindex = 0; pointindex < m_nstraightPath-1; pointindex++)
					{
						float fFromX = *(m_straightPath + (pointindex * 3) + 0);
						float fFromY = *(m_straightPath + (pointindex * 3) + 1);
						float fFromZ = *(m_straightPath + (pointindex * 3) + 2);
						float fToX = *(m_straightPath + ((pointindex + 1) * 3) + 0);
						float fToY = *(m_straightPath + ((pointindex + 1) * 3) + 1);
						float fToZ = *(m_straightPath + ((pointindex + 1) * 3) + 2);
						extern bool DoesLineGoThroughBlocker (float fFromX, float fFromY, float fFromZ, float fToX, float fToY, float fToZ);
						if ( DoesLineGoThroughBlocker (fFromX, fFromY, fFromZ, fToX, fToY, fToZ) == true )
						{
							// GG: the string pull cut across a box the search went round: the line goes the corridor's own way
							// (its portals' mid points, which the search stepped by) or round the box's corners instead, and
							// the path is thrown out only when neither is clear
							if (g_bGGNavBoxExits == true)
							{
								const int iBox = g_iGGLastBlockerHit;
								// a point the string pull bent at inside a box (a polygon's corner under a parked vehicle) can't
								// be reached clear of it: the line goes on to the next point outside every box instead
								int iTo = pointindex + 1;
								while (iTo < m_nstraightPath - 1 && GGNavPointInBlocker(m_straightPath[iTo * 3], m_straightPath[iTo * 3 + 1], m_straightPath[iTo * 3 + 2]) >= 0)
									iTo++;
								if (iTo > pointindex + 1)
									ggRemovePoints(pointindex + 1, iTo - pointindex - 1);
								int iAdded = ggRerouteBySearch(pointindex);
								if (iAdded < 0) iAdded = ggRerouteRoundBoxes(pointindex);
								if (iAdded >= 0)
								{
									m_ggRerouted++;
									pointindex += iAdded;
									continue;
								}
								m_ggRerouteFailed++;
								g_iGGLastBlockerHit = iBox;
							}
							bBlockedByDoor = true;
							break;
						}
					}
					if (bBlockedByDoor == true)
					{
						m_npolys = 0;
						m_nstraightPath = 0;
					}
				}

				// GG: the start, in the box it stands in, back at the front: the path's first line steps out of the box
				if (m_ggStartBox >= 0 && m_nstraightPath > 0)
				{
					if (ggInsertPoints(-1, m_spos, &m_startRef, 1) < 0)
						m_ggStartBox = -1;
				}
			}
		}
		else
		{
			m_npolys = 0;
			m_nstraightPath = 0;
		}
	}
	else if (m_toolMode == TOOLMODE_PATHFIND_SLICED)
	{
		if (m_sposSet && m_eposSet && m_startRef && m_endRef)
		{
#ifdef DUMP_REQS
			printf("ps  %f %f %f  %f %f %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], m_epos[0],m_epos[1],m_epos[2],
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags()); 
#endif
			m_npolys = 0;
			m_nstraightPath = 0;
			
			m_pathFindStatus = m_navQuery->initSlicedFindPath(m_startRef, m_endRef, m_spos, m_epos, &m_filter, DT_FINDPATH_ANY_ANGLE);
		}
		else
		{
			m_npolys = 0;
			m_nstraightPath = 0;
		}
	}
	else if (m_toolMode == TOOLMODE_RAYCAST)
	{
		m_nstraightPath = 0;
		if (m_sposSet && m_eposSet && m_startRef)
		{
#ifdef DUMP_REQS
			printf("rc  %f %f %f  %f %f %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], m_epos[0],m_epos[1],m_epos[2],
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags()); 
#endif
			float t = 0;
			m_npolys = 0;
			m_nstraightPath = 2;
			m_straightPath[0] = m_spos[0];
			m_straightPath[1] = m_spos[1];
			m_straightPath[2] = m_spos[2];
			m_navQuery->raycast(m_startRef, m_spos, m_epos, &m_filter, &t, m_hitNormal, m_polys, &m_npolys, MAX_POLYS);
			if (t > 1)
			{
				// No hit
				dtVcopy(m_hitPos, m_epos);
				m_hitResult = false;
			}
			else
			{
				// Hit
				dtVlerp(m_hitPos, m_spos, m_epos, t);
				m_hitResult = true;
			}
			// Adjust height.
			if (m_npolys > 0)
			{
				float h = 0;
				m_navQuery->getPolyHeight(m_polys[m_npolys-1], m_hitPos, &h);
				m_hitPos[1] = h;
			}
			dtVcopy(&m_straightPath[3], m_hitPos);
		}
	}
	else if (m_toolMode == TOOLMODE_DISTANCE_TO_WALL)
	{
		m_distanceToWall = 0;
		if (m_sposSet && m_startRef)
		{
#ifdef DUMP_REQS
			printf("dw  %f %f %f  %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], 100.0f,
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags()); 
#endif
			m_distanceToWall = 0.0f;
			m_navQuery->findDistanceToWall(m_startRef, m_spos, 100.0f, &m_filter, &m_distanceToWall, m_hitPos, m_hitNormal);
		}
	}
	else if (m_toolMode == TOOLMODE_FIND_POLYS_IN_CIRCLE)
	{
		if (m_sposSet && m_startRef && m_eposSet)
		{
			const float dx = m_epos[0] - m_spos[0];
			const float dz = m_epos[2] - m_spos[2];
			float dist = sqrtf(dx*dx + dz*dz);
#ifdef DUMP_REQS
			printf("fpc  %f %f %f  %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], dist,
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags());
#endif
			m_navQuery->findPolysAroundCircle(m_startRef, m_spos, dist, &m_filter,
											  m_polys, m_parent, 0, &m_npolys, MAX_POLYS);

		}
	}
	else if (m_toolMode == TOOLMODE_FIND_POLYS_IN_SHAPE)
	{
		if (m_sposSet && m_startRef && m_eposSet)
		{
			const float nx = (m_epos[2] - m_spos[2])*0.25f;
			const float nz = -(m_epos[0] - m_spos[0])*0.25f;
			const float agentHeight = m_sample ? m_sample->getAgentHeight() : 0;

			m_queryPoly[0] = m_spos[0] + nx*1.2f;
			m_queryPoly[1] = m_spos[1] + agentHeight/2;
			m_queryPoly[2] = m_spos[2] + nz*1.2f;

			m_queryPoly[3] = m_spos[0] - nx*1.3f;
			m_queryPoly[4] = m_spos[1] + agentHeight/2;
			m_queryPoly[5] = m_spos[2] - nz*1.3f;

			m_queryPoly[6] = m_epos[0] - nx*0.8f;
			m_queryPoly[7] = m_epos[1] + agentHeight/2;
			m_queryPoly[8] = m_epos[2] - nz*0.8f;

			m_queryPoly[9] = m_epos[0] + nx;
			m_queryPoly[10] = m_epos[1] + agentHeight/2;
			m_queryPoly[11] = m_epos[2] + nz;
			
#ifdef DUMP_REQS
			printf("fpp  %f %f %f  %f %f %f  %f %f %f  %f %f %f  0x%x 0x%x\n",
				   m_queryPoly[0],m_queryPoly[1],m_queryPoly[2],
				   m_queryPoly[3],m_queryPoly[4],m_queryPoly[5],
				   m_queryPoly[6],m_queryPoly[7],m_queryPoly[8],
				   m_queryPoly[9],m_queryPoly[10],m_queryPoly[11],
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags());
#endif
			m_navQuery->findPolysAroundShape(m_startRef, m_queryPoly, 4, &m_filter,
											 m_polys, m_parent, 0, &m_npolys, MAX_POLYS);
		}
	}
	else if (m_toolMode == TOOLMODE_FIND_LOCAL_NEIGHBOURHOOD)
	{
		if (m_sposSet && m_startRef)
		{
#ifdef DUMP_REQS
			printf("fln  %f %f %f  %f  0x%x 0x%x\n",
				   m_spos[0],m_spos[1],m_spos[2], m_neighbourhoodRadius,
				   m_filter.getIncludeFlags(), m_filter.getExcludeFlags());
#endif
			m_navQuery->findLocalNeighbourhood(m_startRef, m_spos, m_neighbourhoodRadius, &m_filter,
											   m_polys, m_parent, &m_npolys, MAX_POLYS);
		}
	}
}

static void getPolyCenter(dtNavMesh* navMesh, dtPolyRef ref, float* center)
{
	center[0] = 0;
	center[1] = 0;
	center[2] = 0;
	
	const dtMeshTile* tile = 0;
	const dtPoly* poly = 0;
	dtStatus status = navMesh->getTileAndPolyByRef(ref, &tile, &poly);
	if (dtStatusFailed(status))
		return;
		
	for (int i = 0; i < (int)poly->vertCount; ++i)
	{
		const float* v = &tile->verts[poly->verts[i]*3];
		center[0] += v[0];
		center[1] += v[1];
		center[2] += v[2];
	}
	const float s = 1.0f / poly->vertCount;
	center[0] *= s;
	center[1] *= s;
	center[2] *= s;
}

void NavMeshTesterTool::handleRender()
{
	duDebugDraw& dd = m_sample->getDebugDraw();
	
	/*
	static const unsigned int startCol = duRGBA(128,25,0,192);
	static const unsigned int endCol = duRGBA(51,102,0,129);
	static const unsigned int pathCol = duRGBA(0,0,0,64);
	
	const float agentRadius = m_sample->getAgentRadius();
	const float agentHeight = m_sample->getAgentHeight();
	const float agentClimb = m_sample->getAgentClimb();
	
	dd.depthMask(false);
	if (m_sposSet)
		drawAgent(m_spos, agentRadius, agentHeight, agentClimb, startCol);
	if (m_eposSet)
		drawAgent(m_epos, agentRadius, agentHeight, agentClimb, endCol);
	dd.depthMask(true);
	
	if (!m_navMesh)
	{
		return;
	}
	*/

	if (m_toolMode == TOOLMODE_PATHFIND_FOLLOW)
	{
		/*
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_endRef, endCol);
		
		if (m_npolys)
		{
			for (int i = 0; i < m_npolys; ++i)
			{
				if (m_polys[i] == m_startRef || m_polys[i] == m_endRef)
					continue;
				duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			}
		}
		*/	

		if (m_nsmoothPath)
		{
			dd.depthMask(false);
			const unsigned int spathCol = duRGBA(0,0,255,220);
			dd.begin(DU_DRAW_LINES, 3.0f);
			for (int i = 0; i < m_nsmoothPath; ++i)
				dd.vertex(m_smoothPath[i*3], m_smoothPath[i*3+1]+5.0f, m_smoothPath[i*3+2], spathCol);
			dd.end();
			dd.depthMask(true);
		}
		
		/*
		if (m_pathIterNum)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_pathIterPolys[0], duRGBA(255,255,255,128));

			dd.depthMask(false);
			dd.begin(DU_DRAW_LINES, 1.0f);
			
			const unsigned int prevCol = duRGBA(255,192,0,220);
			const unsigned int curCol = duRGBA(255,255,255,220);
			const unsigned int steerCol = duRGBA(0,192,255,220);

			dd.vertex(m_prevIterPos[0],m_prevIterPos[1]-0.3f,m_prevIterPos[2], prevCol);
			dd.vertex(m_prevIterPos[0],m_prevIterPos[1]+0.3f,m_prevIterPos[2], prevCol);

			dd.vertex(m_iterPos[0],m_iterPos[1]-0.3f,m_iterPos[2], curCol);
			dd.vertex(m_iterPos[0],m_iterPos[1]+0.3f,m_iterPos[2], curCol);

			dd.vertex(m_prevIterPos[0],m_prevIterPos[1]+0.3f,m_prevIterPos[2], prevCol);
			dd.vertex(m_iterPos[0],m_iterPos[1]+0.3f,m_iterPos[2], prevCol);

			dd.vertex(m_prevIterPos[0],m_prevIterPos[1]+0.3f,m_prevIterPos[2], steerCol);
			dd.vertex(m_steerPos[0],m_steerPos[1]+0.3f,m_steerPos[2], steerCol);
			
			for (int i = 0; i < m_steerPointCount-1; ++i)
			{
				dd.vertex(m_steerPoints[i*3+0],m_steerPoints[i*3+1]+0.2f,m_steerPoints[i*3+2], duDarkenCol(steerCol));
				dd.vertex(m_steerPoints[(i+1)*3+0],m_steerPoints[(i+1)*3+1]+0.2f,m_steerPoints[(i+1)*3+2], duDarkenCol(steerCol));
			}
			
			dd.end();
			dd.depthMask(true);
		}
		*/
	}
	else if (m_toolMode == TOOLMODE_PATHFIND_STRAIGHT ||
			 m_toolMode == TOOLMODE_PATHFIND_SLICED)
	{
		/*
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_endRef, endCol);
		
		if (m_npolys)
		{
			for (int i = 0; i < m_npolys; ++i)
			{
				if (m_polys[i] == m_startRef || m_polys[i] == m_endRef)
					continue;
				duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			}
		}
		*/

		if (m_nstraightPath)
		{
			dd.depthMask(false);
			const unsigned int spathCol = duRGBA(0, 0, 255, 220);
			dd.begin(DU_DRAW_LINES, 3.0f);
			float fX = m_straightPath[0];
			float fY = m_straightPath[1];
			float fZ = m_straightPath[2];
			for (int i = 0; i < m_nstraightPath-1; ++i)
			{
				dd.vertex(fX,fY + 5.0f,fZ, spathCol);
				float fNextX = m_straightPath[((i + 1) * 3) + 0];
				float fNextY = m_straightPath[((i + 1) * 3) + 1];
				float fNextZ = m_straightPath[((i + 1) * 3) + 2];
				float fDiffX = fNextX - fX;
				float fDiffY = fNextY - fY;
				float fDiffZ = fNextZ - fZ;
				float fDD = sqrt((fDiffX*fDiffX) + (fDiffY*fDiffY) + (fDiffZ*fDiffZ));
				float fIX = fDiffX / fDD;
				float fIY = fDiffY / fDD;
				float fIZ = fDiffZ / fDD;
				for (int iSteps = 0; iSteps < (int)fDD; iSteps++)
				{
					fX += fIX;
					fY += fIY;
					fZ += fIZ;
					dd.vertex(fX, fY + 5.0f, fZ, spathCol);
				}
			}
			dd.end();
			dd.depthMask(true);
		}

		/*
		if (m_nstraightPath)
		{
			dd.depthMask(false);
			const unsigned int spathCol = duRGBA(64,16,0,220);
			const unsigned int offMeshCol = duRGBA(128,96,0,220);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int i = 0; i < m_nstraightPath-1; ++i)
			{
				unsigned int col;
				if (m_straightPathFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
					col = offMeshCol;
				else
					col = spathCol;
				
				dd.vertex(m_straightPath[i*3], m_straightPath[i*3+1]+0.4f, m_straightPath[i*3+2], col);
				dd.vertex(m_straightPath[(i+1)*3], m_straightPath[(i+1)*3+1]+0.4f, m_straightPath[(i+1)*3+2], col);
			}
			dd.end();
			dd.begin(DU_DRAW_POINTS, 6.0f);
			for (int i = 0; i < m_nstraightPath; ++i)
			{
				unsigned int col;
				if (m_straightPathFlags[i] & DT_STRAIGHTPATH_START)
					col = startCol;
				else if (m_straightPathFlags[i] & DT_STRAIGHTPATH_END)
					col = endCol;
				else if (m_straightPathFlags[i] & DT_STRAIGHTPATH_OFFMESH_CONNECTION)
					col = offMeshCol;
				else
					col = spathCol;
				dd.vertex(m_straightPath[i*3], m_straightPath[i*3+1]+0.4f, m_straightPath[i*3+2], col);
			}
			dd.end();
			dd.depthMask(true);
		}
		*/
	}
	/*
	else if (m_toolMode == TOOLMODE_RAYCAST)
	{
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		
		if (m_nstraightPath)
		{
			for (int i = 1; i < m_npolys; ++i)
				duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			
			dd.depthMask(false);
			const unsigned int spathCol = m_hitResult ? duRGBA(64,16,0,220) : duRGBA(240,240,240,220);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int i = 0; i < m_nstraightPath-1; ++i)
			{
				dd.vertex(m_straightPath[i*3], m_straightPath[i*3+1]+0.4f, m_straightPath[i*3+2], spathCol);
				dd.vertex(m_straightPath[(i+1)*3], m_straightPath[(i+1)*3+1]+0.4f, m_straightPath[(i+1)*3+2], spathCol);
			}
			dd.end();
			dd.begin(DU_DRAW_POINTS, 4.0f);
			for (int i = 0; i < m_nstraightPath; ++i)
				dd.vertex(m_straightPath[i*3], m_straightPath[i*3+1]+0.4f, m_straightPath[i*3+2], spathCol);
			dd.end();

			if (m_hitResult)
			{
				const unsigned int hitCol = duRGBA(0,0,0,128);
				dd.begin(DU_DRAW_LINES, 2.0f);
				dd.vertex(m_hitPos[0], m_hitPos[1] + 0.4f, m_hitPos[2], hitCol);
				dd.vertex(m_hitPos[0] + m_hitNormal[0]*agentRadius,
						  m_hitPos[1] + 0.4f + m_hitNormal[1]*agentRadius,
						  m_hitPos[2] + m_hitNormal[2]*agentRadius, hitCol);
				dd.end();
			}
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == TOOLMODE_DISTANCE_TO_WALL)
	{
		duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_startRef, startCol);
		dd.depthMask(false);
		duDebugDrawCircle(&dd, m_spos[0], m_spos[1]+agentHeight/2, m_spos[2], m_distanceToWall, duRGBA(64,16,0,220), 2.0f);
		dd.begin(DU_DRAW_LINES, 3.0f);
		dd.vertex(m_hitPos[0], m_hitPos[1] + 0.02f, m_hitPos[2], duRGBA(0,0,0,192));
		dd.vertex(m_hitPos[0], m_hitPos[1] + agentHeight, m_hitPos[2], duRGBA(0,0,0,192));
		dd.end();
		dd.depthMask(true);
	}
	else if (m_toolMode == TOOLMODE_FIND_POLYS_IN_CIRCLE)
	{
		for (int i = 0; i < m_npolys; ++i)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			dd.depthMask(false);
			if (m_parent[i])
			{
				float p0[3], p1[3];
				dd.depthMask(false);
				getPolyCenter(m_navMesh, m_parent[i], p0);
				getPolyCenter(m_navMesh, m_polys[i], p1);
				duDebugDrawArc(&dd, p0[0],p0[1],p0[2], p1[0],p1[1],p1[2], 0.25f, 0.0f, 0.4f, duRGBA(0,0,0,128), 2.0f);
				dd.depthMask(true);
			}
			dd.depthMask(true);
		}
		
		if (m_sposSet && m_eposSet)
		{
			dd.depthMask(false);
			const float dx = m_epos[0] - m_spos[0];
			const float dz = m_epos[2] - m_spos[2];
			const float dist = sqrtf(dx*dx + dz*dz);
			duDebugDrawCircle(&dd, m_spos[0], m_spos[1]+agentHeight/2, m_spos[2], dist, duRGBA(64,16,0,220), 2.0f);
			dd.depthMask(true);
		}
	}	
	else if (m_toolMode == TOOLMODE_FIND_POLYS_IN_SHAPE)
	{
		for (int i = 0; i < m_npolys; ++i)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			dd.depthMask(false);
			if (m_parent[i])
			{
				float p0[3], p1[3];
				dd.depthMask(false);
				getPolyCenter(m_navMesh, m_parent[i], p0);
				getPolyCenter(m_navMesh, m_polys[i], p1);
				duDebugDrawArc(&dd, p0[0],p0[1],p0[2], p1[0],p1[1],p1[2], 0.25f, 0.0f, 0.4f, duRGBA(0,0,0,128), 2.0f);
				dd.depthMask(true);
			}
			dd.depthMask(true);
		}
		
		if (m_sposSet && m_eposSet)
		{
			dd.depthMask(false);
			const unsigned int col = duRGBA(64,16,0,220);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int i = 0, j = 3; i < 4; j=i++)
			{
				const float* p0 = &m_queryPoly[j*3];
				const float* p1 = &m_queryPoly[i*3];
				dd.vertex(p0, col);
				dd.vertex(p1, col);
			}
			dd.end();
			dd.depthMask(true);
		}
	}
	else if (m_toolMode == TOOLMODE_FIND_LOCAL_NEIGHBOURHOOD)
	{
		for (int i = 0; i < m_npolys; ++i)
		{
			duDebugDrawNavMeshPoly(&dd, *m_navMesh, m_polys[i], pathCol);
			dd.depthMask(false);
			if (m_parent[i])
			{
				float p0[3], p1[3];
				dd.depthMask(false);
				getPolyCenter(m_navMesh, m_parent[i], p0);
				getPolyCenter(m_navMesh, m_polys[i], p1);
				duDebugDrawArc(&dd, p0[0],p0[1],p0[2], p1[0],p1[1],p1[2], 0.25f, 0.0f, 0.4f, duRGBA(0,0,0,128), 2.0f);
				dd.depthMask(true);
			}

			static const int MAX_SEGS = DT_VERTS_PER_POLYGON*4;
			float segs[MAX_SEGS*6];
			dtPolyRef refs[MAX_SEGS];
			memset(refs, 0, sizeof(dtPolyRef)*MAX_SEGS); 
			int nsegs = 0;
			m_navQuery->getPolyWallSegments(m_polys[i], &m_filter, segs, refs, &nsegs, MAX_SEGS);
			dd.begin(DU_DRAW_LINES, 2.0f);
			for (int j = 0; j < nsegs; ++j)
			{
				const float* s = &segs[j*6];
				
				// Skip too distant segments.
				float tseg;
				float distSqr = dtDistancePtSegSqr2D(m_spos, s, s+3, tseg);
				if (distSqr > dtSqr(m_neighbourhoodRadius))
					continue;
				
				float delta[3], norm[3], p0[3], p1[3];
				dtVsub(delta, s+3,s);
				dtVmad(p0, s, delta, 0.5f);
				norm[0] = delta[2];
				norm[1] = 0;
				norm[2] = -delta[0];
				dtVnormalize(norm);
				dtVmad(p1, p0, norm, agentRadius*0.5f);

				// Skip backfacing segments.
				if (refs[j])
				{
					unsigned int col = duRGBA(255,255,255,32);
					dd.vertex(s[0],s[1]+agentClimb,s[2],col);
					dd.vertex(s[3],s[4]+agentClimb,s[5],col);
				}
				else
				{
					unsigned int col = duRGBA(192,32,16,192);
					if (dtTriArea2D(m_spos, s, s+3) < 0.0f)
						col = duRGBA(96,32,16,192);
					
					dd.vertex(p0[0],p0[1]+agentClimb,p0[2],col);
					dd.vertex(p1[0],p1[1]+agentClimb,p1[2],col);

					dd.vertex(s[0],s[1]+agentClimb,s[2],col);
					dd.vertex(s[3],s[4]+agentClimb,s[5],col);
				}
			}
			dd.end();
			
			dd.depthMask(true);
		}
		
		if (m_sposSet)
		{
			dd.depthMask(false);
			duDebugDrawCircle(&dd, m_spos[0], m_spos[1]+agentHeight/2, m_spos[2], m_neighbourhoodRadius, duRGBA(64,16,0,220), 2.0f);
			dd.depthMask(true);
		}
	}
	
	if (m_nrandPoints > 0)
	{
		dd.begin(DU_DRAW_POINTS, 6.0f);
		for (int i = 0; i < m_nrandPoints; i++)
		{
			const float* p = &m_randPoints[i*3];
			dd.vertex(p[0],p[1]+0.1f,p[2], duRGBA(220,32,16,192));
		} 
		dd.end();
		
		if (m_randPointsInCircle && m_sposSet)
		{
			duDebugDrawCircle(&dd, m_spos[0], m_spos[1]+agentHeight/2, m_spos[2], m_randomRadius, duRGBA(64,16,0,220), 2.0f);
		}
	}
	*/
}

/*
void NavMeshTesterTool::handleRenderOverlay(double* proj, double* model, int* view)
{
	GLdouble x, y, z;
	
	// Draw start and end point labels
	if (m_sposSet && gluProject((GLdouble)m_spos[0], (GLdouble)m_spos[1], (GLdouble)m_spos[2],
								model, proj, view, &x, &y, &z))
	{
		imguiDrawText((int)x, (int)(y-25), IMGUI_ALIGN_CENTER, "Start", imguiRGBA(0,0,0,220));
	}
	if (m_eposSet && gluProject((GLdouble)m_epos[0], (GLdouble)m_epos[1], (GLdouble)m_epos[2],
								model, proj, view, &x, &y, &z))
	{
		imguiDrawText((int)x, (int)(y-25), IMGUI_ALIGN_CENTER, "End", imguiRGBA(0,0,0,220));
	}
	
	// Tool help
	const int h = view[3];
	imguiDrawText(280, h-40, IMGUI_ALIGN_LEFT, "LMB+SHIFT: Set start location  LMB: Set end location", imguiRGBA(255,255,255,192));	
}

void NavMeshTesterTool::drawAgent(const float* pos, float r, float h, float c, const unsigned int col)
{
	duDebugDraw& dd = m_sample->getDebugDraw();
	
	dd.depthMask(false);
	
	// Agent dimensions.	
	duDebugDrawCylinderWire(&dd, pos[0]-r, pos[1]+0.02f, pos[2]-r, pos[0]+r, pos[1]+h, pos[2]+r, col, 2.0f);

	duDebugDrawCircle(&dd, pos[0],pos[1]+c,pos[2],r,duRGBA(0,0,0,64),1.0f);

	unsigned int colb = duRGBA(0,0,0,196);
	dd.begin(DU_DRAW_LINES);
	dd.vertex(pos[0], pos[1]-c, pos[2], colb);
	dd.vertex(pos[0], pos[1]+c, pos[2], colb);
	dd.vertex(pos[0]-r/2, pos[1]+0.02f, pos[2], colb);
	dd.vertex(pos[0]+r/2, pos[1]+0.02f, pos[2], colb);
	dd.vertex(pos[0], pos[1]+0.02f, pos[2]-r/2, colb);
	dd.vertex(pos[0], pos[1]+0.02f, pos[2]+r/2, colb);
	dd.end();
	
	dd.depthMask(true);
}
*/
