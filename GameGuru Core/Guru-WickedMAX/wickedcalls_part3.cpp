#ifdef PICKBVHTHREADED
bool WickedCall_GetPick2_OLD(float fMouseX, float fMouseY, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	// Use wicked mouse pointer to determine intersection with solid geometry (for terrain and entity detection)
	// PE: We got hits from hidden objects like "widgets" , now ignore hidden objects in WICKEDREPO.
	bool bHitSuccess = false;

	//PE: Do not do anything if mouse is not over level editor.
	//if (bImGuiGotFocus) return bHitSuccess;

	// do not wipe out hover object detection when widget plane pass happens
	if (iLayerMask & GGRENDERLAYERS_NORMAL)
	{
		g_hovered_pobject = NULL;
		g_hovered_entity = 0;
	}

	//PE: Wicked Mouse is relative to windows pos. ImGui is relative to screen.
	RAY pickRay = wiRenderer::GetPickRay((long)fMouseX, (long)fMouseY, master.masterrenderer);

	// check scene first if flagged, then terrain which is naturally underneath
	if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0 || (iLayerMask & GGRENDERLAYERS_CURSOROBJECT) != 0 || (iLayerMask & GGRENDERLAYERS_WIDGETPLANE) != 0)
	{
		wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
		if (hovered.entity > 0)
		{
			if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0)
			{
				sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hovered.entity);
				if (pHitObject)
				{
					bool ObjectIsEntity(void* pTestObject);

					//PE: Only highlight if this is a gg entity.
					bool bIsEntity = ObjectIsEntity(pHitObject);
					if (bIsEntity)
					{
						if (iLayerMask & GGRENDERLAYERS_NORMAL)
						{
							g_hovered_pobject = pHitObject;
							g_hovered_entity = hovered.entity;
							g_hovered_dot_pobject = NULL;
							g_hovered_dot_entity = 0;
						}
					}
					else
					{
						//#define DOTARCSOBJECTID 70001+40000+21001
						if (pHitObject->dwObjectNumber > 110000 && pHitObject->dwObjectNumber < 131002)
						{
							if (iLayerMask & GGRENDERLAYERS_NORMAL)
							{
								g_hovered_dot_pobject = pHitObject;
								g_hovered_dot_entity = hovered.entity;
								g_bhovered_dot = true;
							}
						}
					}
				}
				else
				{
					if (iLayerMask & GGRENDERLAYERS_NORMAL)
					{
						g_hovered_dot_pobject = NULL;
						g_hovered_dot_entity = 0;
					}
				}
			}

			// return hit position
			*pOutX = hovered.position.x;
			*pOutY = hovered.position.y;
			*pOutZ = hovered.position.z;

			if (iLayerMask & GGRENDERLAYERS_NORMAL)
			{
				fLastHitPosition[0] = hovered.position.x;
				fLastHitPosition[1] = hovered.position.y;
				fLastHitPosition[2] = hovered.position.z;
			}

			// if normals needed
			if (pNormX)
			{
				*pNormX = hovered.normal.x;
				*pNormY = hovered.normal.y;
				*pNormZ = hovered.normal.z;
			}

			// optionally return actual object the cursor hovered over
			if (pHitEntity) *pHitEntity = hovered.entity;

			// report a hit
			bHitSuccess = true;
		}
	}
	if ((iLayerMask & GGRENDERLAYERS_TERRAIN) != 0)
	{
		float fDistToObjectHit = -1;
		if (bHitSuccess == true)
		{
			float fDX = *pOutX - CameraPositionX(0);
			float fDY = *pOutY - CameraPositionY(0);
			float fDZ = *pOutZ - CameraPositionZ(0);
			fDistToObjectHit = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
		}
		float pTerrOutX, pTerrOutY, pTerrOutZ, pTerrNormX, pTerrNormY, pTerrNormZ;
		if (GGTerrain::GGTerrain_RayCast(pickRay, &pTerrOutX, &pTerrOutY, &pTerrOutZ, &pTerrNormX, &pTerrNormY, &pTerrNormZ, 0))
		{
			fLastTerrainHitX = pTerrOutX, fLastTerrainHitY = pTerrOutY, fLastTerrainHitZ = pTerrOutZ;

			float fDX = pTerrOutX - CameraPositionX(0);
			float fDY = pTerrOutY - CameraPositionY(0);
			float fDZ = pTerrOutZ - CameraPositionZ(0);
			float fDist = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
			if (fDist < fDistToObjectHit || fDistToObjectHit == -1)
			{
				// if terrain closer than object hit, we register a terrain detection instead
				if (pHitEntity) *pHitEntity = 0;
				*pOutX = pTerrOutX;
				*pOutY = pTerrOutY;
				*pOutZ = pTerrOutZ;
				if (pNormX)
				{
					*pNormX = pTerrNormX;
					*pNormY = pTerrNormY;
					*pNormZ = pTerrNormZ;
				}
				bHitSuccess = true;
			}
		}
		else
		{
			fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;
		}
	}

	// no entity hovering over, but still want to move the cursor line in visual logic system
	if (g_hovered_entity == 0)
	{
		if (iLayerMask & GGRENDERLAYERS_NORMAL)
		{
			fLastHitPosition[0] = *pOutX;
			fLastHitPosition[1] = *pOutY;
			fLastHitPosition[2] = *pOutZ;
		}
	}

	// return success flag
	return bHitSuccess;
}
#endif

// PERF P.3 diagnostics: how often the per-frame editor pick actually runs the scene raycast vs
// reuses the cached result (see cache below). Read via GET_PERF_DATA -> PICK_REAL_RUNS/PICK_CACHE_HITS.
int g_pickRealRuns = 0;
int g_pickCacheHits = 0;
int g_pickMissMask = 0; // miss because layer/output pattern differed from cached (multi-mask thrash)
int g_pickMissRay  = 0; // miss because the ray inputs (cursor/camera) differed (motion or instability)

// GGMAX 2.64: does a pick-hit wicked entity belong to the given DBO object number?
// The wiScene pick's layer mask cannot EXCLUDE terrain chunks — wicked entities without a
// LayerComponent default to ALL layer bits and pass every mask — so a pick against, e.g.,
// GGRENDERLAYERS_CURSOROBJECT still "hits" anywhere over terrain. Callers that need
// "did I hit THIS object" must test the returned entity (Terrain Generator marker grab).
bool WickedCall_IsEntityOfObject(uint64_t iEntityID, int iObjectNumber)
{
	if (iEntityID == 0) return false;
	sObject* pObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(iEntityID);
	return (pObject != NULL && (int)pObject->dwObjectNumber == iObjectNumber);
}

bool WickedCall_GetPick(float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	XMFLOAT4 currentMouse = wiInput::GetPointer();

	// CORRECTNESS (Stage P.3): NEVER serve a cached pick while the user is actively interacting
	// (any mouse button held). Interactive edits read the pick-derived cursor and gate on it CHANGING
	// each frame: e.g. grass/terrain PAINT (t.tx_f -> t.inputsys.localx_f -> t.terrain.X_f; the paint
	// only applies when t.terrain.X_f differs from last frame - M-Terrain_part1.cpp), terrain SCULPT
	// (which also moves the terrain the ray hits, so a replayed hit is stale by construction), and
	// object dragging. A replayed pick would freeze the cursor and the edit would silently never
	// register. The cache exists only to save cost at a truly IDLE, parked camera - where no mouse
	// button is down - so this bypass costs nothing there. This fixes "cannot paint grass".
	if (wiInput::Down(wiInput::MOUSE_BUTTON_LEFT) || wiInput::Down(wiInput::MOUSE_BUTTON_RIGHT) || wiInput::Down(wiInput::MOUSE_BUTTON_MIDDLE))
	{
		g_pickRealRuns++;
		return WickedCall_GetPick2(currentMouse.x, currentMouse.y, pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ, pHitEntity, iLayerMask);
	}

	// PERF (Stage P.3): the level editor calls this pick EVERY FRAME to find the entity under the
	// cursor (findentitycursorobj -> gridedit_mapediting). The underlying wiScene raycast tests the
	// whole scene (~21K objects on the island) and is the single biggest editor-CPU cost measured
	// (~5ms, "P2-mapediting"). The hovered result can only change when the pick RAY changes, i.e. when
	// the cursor or the camera moves. Memoise on the RAW ray inputs - screen pointer + camera
	// Eye/At/Up + layer mask + which outputs were requested. (Deliberately NOT the derived pick ray:
	// CameraComponent::Projection bakes in the per-frame TAA jitter, so the ray oscillates sub-pixel
	// every frame even when parked and would defeat the cache.) When nothing moved we replay the
	// cached outputs and skip the raycast entirely; the pick's global hover state (g_hovered_*) is
	// produced inside GetPick2 which we skip, so it persists unchanged - exactly correct when nothing
	// moved. Any cursor/camera motion changes the key and re-runs the real pick, so behaviour (and
	// pixels) are identical to before. Sub-pixel-only differences from TAA jitter are intentionally
	// ignored (they never change which object is hovered, and dropping them removes edge hover flicker).
	// Several distinct pick calls happen each frame (different layer masks / requested outputs), so a
	// single-slot cache thrashes (measured: near-100% mask misses). Keep a small per-key slot table so
	// every distinct pick keeps its own cached result and hits frame-to-frame while nothing moves.
	const wiScene::CameraComponent& cam = wiScene::GetCamera();
	const int ptrMask = (pOutX ? 1 : 0) | (pNormX ? 2 : 0) | (pHitEntity ? 4 : 0);
	struct PickCacheSlot
	{
		bool valid;
		int  layer, ptr;
		float mx, my, ex, ey, ez, ax, ay, az, ux, uy, uz;
		bool res;
		float ox, oy, oz, nx, ny, nz;
		uint64_t ent;
	};
	static PickCacheSlot slots[8] = {};
	static int nextSlot = 0;

	// look for a slot that matches this exact pick (mask + requested outputs + ray inputs)
	for (int i = 0; i < 8; i++)
	{
		const PickCacheSlot& s = slots[i];
		if (!s.valid || s.layer != iLayerMask || s.ptr != ptrMask) continue;
		if (s.mx == currentMouse.x && s.my == currentMouse.y &&
			s.ex == cam.Eye.x && s.ey == cam.Eye.y && s.ez == cam.Eye.z &&
			s.ax == cam.At.x  && s.ay == cam.At.y  && s.az == cam.At.z  &&
			s.ux == cam.Up.x  && s.uy == cam.Up.y  && s.uz == cam.Up.z)
		{
			if (pOutX) *pOutX = s.ox; if (pOutY) *pOutY = s.oy; if (pOutZ) *pOutZ = s.oz;
			if (pNormX) *pNormX = s.nx; if (pNormY) *pNormY = s.ny; if (pNormZ) *pNormZ = s.nz;
			if (pHitEntity) *pHitEntity = s.ent;
			g_pickCacheHits++;
			return s.res;
		}
	}
	g_pickMissRay++; // reached only when no slot for this key matched the current ray

	g_pickRealRuns++;
	bool res = WickedCall_GetPick2(currentMouse.x, currentMouse.y, pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ, pHitEntity, iLayerMask);

	// store into the slot already owning this (mask, output-pattern) if present, else the next round-robin
	int useIdx = -1;
	for (int i = 0; i < 8; i++) { if (slots[i].valid && slots[i].layer == iLayerMask && slots[i].ptr == ptrMask) { useIdx = i; break; } }
	if (useIdx < 0) { useIdx = nextSlot; nextSlot = (nextSlot + 1) & 7; }
	PickCacheSlot& s = slots[useIdx];
	s.valid = true; s.layer = iLayerMask; s.ptr = ptrMask; s.res = res;
	s.mx = currentMouse.x; s.my = currentMouse.y;
	s.ex = cam.Eye.x; s.ey = cam.Eye.y; s.ez = cam.Eye.z;
	s.ax = cam.At.x;  s.ay = cam.At.y;  s.az = cam.At.z;
	s.ux = cam.Up.x;  s.uy = cam.Up.y;  s.uz = cam.Up.z;
	s.ox = pOutX ? *pOutX : 0; s.oy = pOutY ? *pOutY : 0; s.oz = pOutZ ? *pOutZ : 0;
	s.nx = pNormX ? *pNormX : 0; s.ny = pNormY ? *pNormY : 0; s.nz = pNormZ ? *pNormZ : 0;
	s.ent = pHitEntity ? *pHitEntity : 0;
	return res;
}

bool WickedCall_SentRay(float originx, float originy, float originz, float directionx, float directiony, float directionz,float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	//PE: Sent ray using wicked.
	XMFLOAT4 currentMouse = wiInput::GetPointer();
	RAY pickRay;
	XMFLOAT3 direction_inverse;
	pickRay.origin.x = originx;
	pickRay.origin.y = originy;
	pickRay.origin.z = originz;
	pickRay.direction.x = directionx;
	pickRay.direction.y = directiony;
	pickRay.direction.z = directionz;
	XMStoreFloat3(&direction_inverse, XMVectorDivide(XMVectorReplicate(1.0f), XMVectorSet(directionx, directiony, directionz,1.0f)));
	pickRay.direction_inverse = direction_inverse;
#ifdef PICKBVHTHREADED
	wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
#else
	wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
#endif
	if (hovered.entity > 0)
	{
		sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hovered.entity);

		if (pHitObject)
		{
			bool ObjectIsEntity(void* pTestObject);

			//PE: Only highlight if this is a gg entity.
			bool bIsEntity = ObjectIsEntity(pHitObject);
			if (bIsEntity) {
				g_ray_pobject = pHitObject;
			}
		}
		else {
			g_ray_pobject = NULL;
		}

		// return hit position
		*pOutX = hovered.position.x;
		*pOutY = hovered.position.y;
		*pOutZ = hovered.position.z;

		fLastHitPosition[0] = hovered.position.x;
		fLastHitPosition[1] = hovered.position.y;
		fLastHitPosition[2] = hovered.position.z;

		// if normals needed
		if (pNormX)
		{
			*pNormX = hovered.normal.x;
			*pNormY = hovered.normal.y;
			*pNormZ = hovered.normal.z;
		}

		// optionally return actual object the cursor hovered over
		if (pHitEntity) *pHitEntity = hovered.entity;

		// report a hit
		return true;
	}
	else
	{
		// report a miss
		g_ray_pobject = NULL;
		return false;
	}
}

bool WickedCall_SentRay2(float originx, float originy, float originz, float directionx, float directiony, float directionz, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, uint64_t* pHitEntity, int iLayerMask)
{
	if (iLayerMask & GGRENDERLAYERS_NORMAL)
	{
		g_hovered_pobject = NULL;
		g_hovered_entity = 0;
	}

	RAY pickRay;
	XMFLOAT3 direction_inverse;
	pickRay.origin.x = originx;
	pickRay.origin.y = originy;
	pickRay.origin.z = originz;
	pickRay.direction.x = directionx;
	pickRay.direction.y = directiony;
	pickRay.direction.z = directionz;
	XMStoreFloat3(&direction_inverse, XMVectorDivide(XMVectorReplicate(1.0f), XMVectorSet(directionx, directiony, directionz, 1.0f)));
	pickRay.direction_inverse = direction_inverse;

	// check scene first if flagged, then terrain which is naturally underneath
	if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0 || (iLayerMask & GGRENDERLAYERS_CURSOROBJECT) != 0 || (iLayerMask & GGRENDERLAYERS_WIDGETPLANE) != 0)
	{
#ifdef PICKBVHTHREADED
		wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
#else
		wiScene::PickResult hovered = wiScene::Pick(pickRay, RENDERTYPE_ALL, iLayerMask);
#endif
		if (hovered.entity > 0)
		{
			if ((iLayerMask & GGRENDERLAYERS_NORMAL) != 0)
			{
				sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hovered.entity);
				if (pHitObject)
				{
					bool ObjectIsEntity(void* pTestObject);

					//PE: Only highlight if this is a gg entity.
					bool bIsEntity = ObjectIsEntity(pHitObject);
					if (bIsEntity)
					{
						if (iLayerMask & GGRENDERLAYERS_NORMAL)
						{
							g_hovered_pobject = pHitObject;
							g_hovered_entity = hovered.entity;
							g_hovered_dot_pobject = NULL;
							g_hovered_dot_entity = 0;
						}
					}
					else
					{
						//#define DOTARCSOBJECTID 70001+40000+21001
						if (pHitObject->dwObjectNumber > 110000 && pHitObject->dwObjectNumber < 131002)
						{
							if (iLayerMask & GGRENDERLAYERS_NORMAL)
							{
								g_hovered_dot_pobject = pHitObject;
								g_hovered_dot_entity = hovered.entity;
							}
						}
					}
				}
				else
				{
					if (iLayerMask & GGRENDERLAYERS_NORMAL)
					{
						g_hovered_dot_pobject = NULL;
						g_hovered_dot_entity = 0;
					}
				}
			}

			// return hit position
			*pOutX = hovered.position.x;
			*pOutY = hovered.position.y;
			*pOutZ = hovered.position.z;

			if (iLayerMask & GGRENDERLAYERS_NORMAL)
			{
				fLastHitPosition[0] = hovered.position.x;
				fLastHitPosition[1] = hovered.position.y;
				fLastHitPosition[2] = hovered.position.z;
			}

			// if normals needed
			if (pNormX)
			{
				*pNormX = hovered.normal.x;
				*pNormY = hovered.normal.y;
				*pNormZ = hovered.normal.z;
			}

			// optionally return actual object the cursor hovered over
			if (pHitEntity) *pHitEntity = hovered.entity;

			// report a hit
			return true;
		}
	}
	if ((iLayerMask & GGRENDERLAYERS_TERRAIN) != 0)
	{
		if (GGTerrain::GGTerrain_RayCast(pickRay, pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ, 0))
		{
			fLastTerrainHitX = *pOutX, fLastTerrainHitY = *pOutY, fLastTerrainHitZ = *pOutZ;
			return true;
		}
		else
		{
			fLastTerrainHitX = 0, fLastTerrainHitY = 0, fLastTerrainHitZ = 0;
		}

	}
	return false;
}

// GGMAX diag: SentRay4 (wiScene::Pick) counters for the FPS-plummet hunt — running
// totals read via harness GET_PERF_DATA "RAYS:" line. Plain adds (worker-thread races
// only cost a tiny undercount; these are diagnostics).
unsigned long long gg_dbg_sentray4_calls = 0;
unsigned long long gg_dbg_sentray4_us = 0;
static unsigned long long gg_dbg_ray_qpc_us(void)
{
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&c);
	return (unsigned long long)((c.QuadPart * 1000000.0) / (double)f.QuadPart);
}

// the limb (frame index) of the object the last WickedCall_SentRay4 hit, -1 if none; IntersectAllEx reports it
int g_iWickedCallRayLimbHit = -1;

// the ground when the scene pick met nothing: the Wicked terrain is chunk meshes made around the camera as it moves, so a
// ray under a fast vehicle or just after a teleport finds no chunk yet, and one exactly down a mesh edge (x or z on the
// vertex spacing) slips between the triangles; both fell to the ray's full range, and a weapon's crosshair ray aimed its
// rockets 2,600 units under the ground. The heightmap answers for the ground then, as the GameGuru terrain's own ray did
static bool WickedCall_SentRay4_Terrain(const RAY& pickRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit)
{
	float fNX, fNY, fNZ;
	if (GGTerrain::GGTerrain_RayCastHeightmap(pickRay, pOutX, pOutY, pOutZ, &fNX, &fNY, &fNZ) != 1) return false;
	if (pNormX)
	{
		*pNormX = fNX;
		*pNormY = fNY;
		*pNormZ = fNZ;
	}
	*pdwObjectNumberHit = 0;
	return true;
}

bool WickedCall_SentRay4(float originx, float originy, float originz, float directionx, float directiony, float directionz, float fDistanceOfRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit, bool bOpaqueOnly)
{
	g_iWickedCallRayLimbHit = -1;
	// ray cast specifically used by game loop to find accurate position of animating objects (performant?)
	// GGMAX 2026-07-31 FPS-plummet fix: the ray previously had no TMin/TMax, so a
	// "120 unit" line-of-sight test actually traversed the ENTIRE island to infinity —
	// Scene::Intersects triangle-tested every hut/tree/character along the line
	// (~26ms per call; three LUA GetPlrLookingAt calls per frame = 13 FPS). Normalize
	// the direction (so TMax and the AABB-prune t-scale agree in world units) and cap
	// the ray at the requested distance.
	RAY pickRay;
	float fDirLen = sqrtf(directionx * directionx + directiony * directiony + directionz * directionz);
	if (fDirLen < 0.000001f) return false;
	float fInvLen = 1.0f / fDirLen;
	pickRay.origin.x = originx;
	pickRay.origin.y = originy;
	pickRay.origin.z = originz;
	pickRay.direction.x = directionx * fInvLen;
	pickRay.direction.y = directiony * fInvLen;
	pickRay.direction.z = directionz * fInvLen;
	XMFLOAT3 direction_inverse;
	XMStoreFloat3(&direction_inverse, XMVectorDivide(XMVectorReplicate(1.0f), XMVectorSet(pickRay.direction.x, pickRay.direction.y, pickRay.direction.z, 1.0f)));
	pickRay.direction_inverse = direction_inverse;
	pickRay.TMin = 0;
	pickRay.TMax = fDistanceOfRay;
	uint32_t checkType = RENDERTYPE_ALL;
	//PE: @Lee we have no checks on transparent objects, we cant shoot glass, no impact effects , no killing pradator ...
	if (bOpaqueOnly == true) checkType = RENDERTYPE_OPAQUE | RENDERTYPE_TRANSPARENT;
	unsigned long long ggT0 = gg_dbg_ray_qpc_us();
#ifdef PICKBVHTHREADED
	wiScene::PickResult hit = wiScene::Pick(pickRay, checkType, GGRENDERLAYERS_NORMAL);
#else
	wiScene::PickResult hit = wiScene::Pick(pickRay, checkType, GGRENDERLAYERS_NORMAL);
#endif
	gg_dbg_sentray4_us += gg_dbg_ray_qpc_us() - ggT0;
	gg_dbg_sentray4_calls++;
	if (hit.entity > 0)
	{
		float fDX = hit.position.x - originx;
		float fDY = hit.position.y - originy;
		float fDZ = hit.position.z - originz;
		float fDistOfHit = sqrt(fabs(fDX*fDX)+fabs(fDY*fDY)+fabs(fDZ*fDZ));
		if (fDistOfHit <= fDistanceOfRay)
		{
			WickedCallProbeScope probeLookup(WICKEDCALL_PROBE_PICK_LOOKUP);
			sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hit.entity);
			if (pHitObject) *pdwObjectNumberHit = pHitObject->dwObjectNumber;
			if (pHitObject && pHitObject->ppFrameList)
			{
				for (int iFrame = 0; iFrame < pHitObject->iFrameCount; iFrame++)
				{
					if (pHitObject->ppFrameList[iFrame] && pHitObject->ppFrameList[iFrame]->wickedobjindex == hit.entity)
					{
						g_iWickedCallRayLimbHit = iFrame;
						break;
					}
				}
			}
			*pOutX = hit.position.x;
			*pOutY = hit.position.y;
			*pOutZ = hit.position.z;
			if (pNormX)
			{
				*pNormX = hit.normal.x;
				*pNormY = hit.normal.y;
				*pNormZ = hit.normal.z;
			}
			return true;
		}
	}
	return WickedCall_SentRay4_Terrain(pickRay, pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ, pdwObjectNumberHit);
}

#ifdef PICKBVHTHREADED
bool WickedCall_SentRay4_ThreadSafe(float originx, float originy, float originz, float directionx, float directiony, float directionz, float fDistanceOfRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit, bool bOpaqueOnly, sObject* pIgnore1, sObject* pIgnore2)
{
	// ray cast specifically used by game loop to find accurate position of animating objects (performant?)
	// GGMAX 2026-07-31: normalized + TMax-capped, same as WickedCall_SentRay4 (see comment there)
	RAY pickRay;
	float fDirLen = sqrtf(directionx * directionx + directiony * directiony + directionz * directionz);
	if (fDirLen < 0.000001f) return false;
	float fInvLen = 1.0f / fDirLen;
	pickRay.origin.x = originx;
	pickRay.origin.y = originy;
	pickRay.origin.z = originz;
	pickRay.direction.x = directionx * fInvLen;
	pickRay.direction.y = directiony * fInvLen;
	pickRay.direction.z = directionz * fInvLen;
	XMFLOAT3 direction_inverse;
	XMStoreFloat3(&direction_inverse, XMVectorDivide(XMVectorReplicate(1.0f), XMVectorSet(pickRay.direction.x, pickRay.direction.y, pickRay.direction.z, 1.0f)));
	pickRay.direction_inverse = direction_inverse;
	pickRay.TMin = 0;
	pickRay.TMax = fDistanceOfRay;
	uint32_t checkType = RENDERTYPE_ALL;
	//PE: @Lee we have no checks on transparent objects, we cant shoot glass, no impact effects , no killing pradator ...
	if (bOpaqueOnly == true) checkType = RENDERTYPE_OPAQUE | RENDERTYPE_TRANSPARENT;
	unsigned long long ggT0 = gg_dbg_ray_qpc_us();

	// the objects to leave out, by their frames' Wicked objects (the main thread's IntersectAllEx moves them to another
	// layer instead, a scene change this thread must not make)
	std::vector<wiECS::Entity> exclude;
	sObject* pIgnore[2] = { pIgnore1, pIgnore2 };
	for (int o = 0; o < 2; o++)
	{
		if (!pIgnore[o]) continue;
		for (int iF = 0; iF < pIgnore[o]->iFrameCount; iF++)
		{
			if (pIgnore[o]->ppFrameList[iF] && pIgnore[o]->ppFrameList[iF]->wickedobjindex > 0) exclude.push_back(pIgnore[o]->ppFrameList[iF]->wickedobjindex);
		}
	}
	wiScene::PickResult hit = wiScene::Pick(pickRay, checkType, GGRENDERLAYERS_NORMAL, wiScene::GetScene(), exclude.data(), (uint32_t)exclude.size());
	gg_dbg_sentray4_us += gg_dbg_ray_qpc_us() - ggT0;
	gg_dbg_sentray4_calls++;
	if (hit.entity > 0)
	{
		float fDX = hit.position.x - originx;
		float fDY = hit.position.y - originy;
		float fDZ = hit.position.z - originz;
		float fDistOfHit = sqrt(fabs(fDX * fDX) + fabs(fDY * fDY) + fabs(fDZ * fDZ));
		if (fDistOfHit <= fDistanceOfRay)
		{
			sObject* pHitObject = m_ObjectManager.FindObjectFromWickedObjectEntityID(hit.entity);
			if (pHitObject) *pdwObjectNumberHit = pHitObject->dwObjectNumber;
			*pOutX = hit.position.x;
			*pOutY = hit.position.y;
			*pOutZ = hit.position.z;
			if (pNormX)
			{
				*pNormX = hit.normal.x;
				*pNormY = hit.normal.y;
				*pNormZ = hit.normal.z;
			}
			return true;
		}
	}
	return WickedCall_SentRay4_Terrain(pickRay, pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ, pdwObjectNumberHit);
}
#endif

bool WickedCall_SentRay3(float originx, float originy, float originz, float directionx, float directiony, float directionz, float fDistanceOfRay, float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ, DWORD* pdwObjectNumberHit)
{
	return WickedCall_SentRay4(originx, originy, originz, directionx, directiony, directionz, fDistanceOfRay, pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ, pdwObjectNumberHit, false);
}

void WickedCall_GetMouseDeltas(float* pfX, float* pfY)
{
	XMFLOAT4 currentMouse = wiInput::GetPointer();
	*pfX = currentMouse.x - g_lastMousePos.x;
	*pfY = currentMouse.y - g_lastMousePos.y;
	g_lastMousePos = currentMouse;
}

uint32_t WickedCall_GetTextureWidth(void* ptex)
{
	Texture* tex = (Texture*)ptex;
	return( tex->GetDesc().width);
}
uint32_t WickedCall_GetTextureHeight(void* ptex)
{
	Texture* tex = (Texture*)ptex;
	return(tex->GetDesc().height);
}

uint64_t WickedCall_GetFirstRootEntityID(sObject* pObject)
{
	//PE: Each frame has its own master object id.
	for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
	{
		if (pObject->ppFrameList && pObject->ppFrameList[iFrameIndex])
		{
			sFrame* pFrame = pObject->ppFrameList[iFrameIndex];
			if (pFrame && pFrame->wickedobjindex > 0)
			{
				ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
				if (object) {
					return(pFrame->wickedobjindex);
				}
			}
		}
	}
	return(0);
}

int WickedCall_GetNumberOfRootEntityIDs(sObject* pObject)
{
	int iRootNumber = 0;
	for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
	{
		sFrame* pFrame = pObject->ppFrameList[iFrameIndex];
		if (pFrame && pFrame->wickedobjindex > 0)
		{
			ObjectComponent* object = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
			if (object) {
				iRootNumber++;
			}
		}
	}
	return(iRootNumber);
}

void WickedCall_SetSelectedObject(sObject* pObject)
{
	g_selected_entity = 0;
	g_selected_pobject = pObject;

	if (!pObject) {
		return;
	}
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	if (rootEntity > 0)
	{
		g_selected_entity = rootEntity;
		g_selected_pobject = pObject;
	}
	return;
}

// per instance multiplier (0 to 1) on the materials' emissive colour and strength (alpha). Instanced entities share
// their mesh and materials, but each has its own object components, so this changes one instance only
void WickedCall_SetObjectEmissiveTint(sObject* pObject, float fRed, float fGreen, float fBlue, float fStrength)
{
	if (!pObject) return;
	XMFLOAT4 tint = XMFLOAT4(max(0.0f, min(1.0f, fRed)), max(0.0f, min(1.0f, fGreen)), max(0.0f, min(1.0f, fBlue)), max(0.0f, min(1.0f, fStrength)));
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	if (rootEntity > 0)
	{
		wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(rootEntity);
		if (pWickedObject) pWickedObject->emissiveColor = tint;
	}
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		if (pObject->ppFrameList && pObject->ppFrameList[iF])
		{
			uint64_t objectEntity = pObject->ppFrameList[iF]->wickedobjindex;
			if (objectEntity > 0 && rootEntity != objectEntity)
			{
				wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(objectEntity);
				if (pWickedObject) pWickedObject->emissiveColor = tint;
			}
		}
	}
}

// per instance colour multiplier (ObjectComponent::color) on every Wicked object of this DBO object; the alpha is kept
void WickedCall_SetObjectColorTint(sObject* pObject, float fRed, float fGreen, float fBlue)
{
	if (!pObject) return;
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	for (int iF = -1; iF < pObject->iFrameCount; iF++)
	{
		uint64_t objectEntity = rootEntity;
		if (iF >= 0)
		{
			if (!pObject->ppFrameList || !pObject->ppFrameList[iF]) continue;
			objectEntity = pObject->ppFrameList[iF]->wickedobjindex;
			if (objectEntity == rootEntity) continue;
		}
		if (objectEntity == 0) continue;
		wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(objectEntity);
		if (pWickedObject)
		{
			pWickedObject->color.x = max(0.0f, min(1.0f, fRed));
			pWickedObject->color.y = max(0.0f, min(1.0f, fGreen));
			pWickedObject->color.z = max(0.0f, min(1.0f, fBlue));
		}
	}
}

bool WickedCall_GetObjectColorTint(sObject* pObject, float* pfRed, float* pfGreen, float* pfBlue)
{
	if (!pObject) return false;
	wiScene::ObjectComponent* pWickedObject = NULL;
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	if (rootEntity > 0) pWickedObject = wiScene::GetScene().objects.GetComponent(rootEntity);
	for (int iF = 0; !pWickedObject && pObject->ppFrameList && iF < pObject->iFrameCount; iF++)
	{
		if (pObject->ppFrameList[iF] && pObject->ppFrameList[iF]->wickedobjindex > 0)
			pWickedObject = wiScene::GetScene().objects.GetComponent(pObject->ppFrameList[iF]->wickedobjindex);
	}
	if (!pWickedObject) return false;
	*pfRed = pWickedObject->color.x;
	*pfGreen = pWickedObject->color.y;
	*pfBlue = pWickedObject->color.z;
	return true;
}

bool WickedCall_GetObjectEmissiveTint(sObject* pObject, float* pfRed, float* pfGreen, float* pfBlue, float* pfStrength)
{
	if (!pObject) return false;
	wiScene::ObjectComponent* pWickedObject = NULL;
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	if (rootEntity > 0) pWickedObject = wiScene::GetScene().objects.GetComponent(rootEntity);
	for (int iF = 0; !pWickedObject && pObject->ppFrameList && iF < pObject->iFrameCount; iF++)
	{
		if (pObject->ppFrameList[iF] && pObject->ppFrameList[iF]->wickedobjindex > 0)
			pWickedObject = wiScene::GetScene().objects.GetComponent(pObject->ppFrameList[iF]->wickedobjindex);
	}
	if (!pWickedObject) return false;
	*pfRed = pWickedObject->emissiveColor.x;
	*pfGreen = pWickedObject->emissiveColor.y;
	*pfBlue = pWickedObject->emissiveColor.z;
	*pfStrength = pWickedObject->emissiveColor.w;
	return true;
}

// a limb's alpha on its own Wicked object (ObjectComponent::color.w). Below 1 the renderer fades the object by dithering it
// in the depth prepass (forceAlphatestForDithering), with no blend mode change; the instance data carries color.w times
// itself, so color.w is the square root of the alpha. Shadows are not dithered
static void WickedCall_SetFrameAlpha(sFrame* pFrame, float fColorW, bool bChildren)
{
	if (pFrame->wickedobjindex > 0)
	{
		wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
		if (pWickedObject) pWickedObject->color.w = fColorW;
	}
	if (bChildren)
	{
		for (sFrame* pChild = pFrame->pChild; pChild; pChild = pChild->pSibling)
			WickedCall_SetFrameAlpha(pChild, fColorW, true);
	}
}

// fAlpha 0 to 1 (1 = as made); bChildren also the limbs under it; iLimb -1 is every Wicked object of this DBO object
void WickedCall_SetLimbAlpha(sObject* pObject, int iLimb, float fAlpha, bool bChildren)
{
	if (!pObject || !pObject->ppFrameList) return;
	float fColorW = sqrt(max(0.0f, min(1.0f, fAlpha)));
	if (iLimb == -1)
	{
		uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
		wiScene::ObjectComponent* pWickedObject = NULL;
		if (rootEntity > 0) pWickedObject = wiScene::GetScene().objects.GetComponent(rootEntity);
		if (pWickedObject) pWickedObject->color.w = fColorW;
		for (int iF = 0; iF < pObject->iFrameCount; iF++)
		{
			if (pObject->ppFrameList[iF]) WickedCall_SetFrameAlpha(pObject->ppFrameList[iF], fColorW, false);
		}
		return;
	}
	if (iLimb < 0 || iLimb >= pObject->iFrameCount || !pObject->ppFrameList[iLimb]) return;
	WickedCall_SetFrameAlpha(pObject->ppFrameList[iLimb], fColorW, bChildren);
}

// the limb's own Wicked object, or the first one under it (a rotor frame without a mesh)
static wiScene::ObjectComponent* WickedCall_FindFrameObject(sFrame* pFrame)
{
	if (pFrame->wickedobjindex > 0)
	{
		wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(pFrame->wickedobjindex);
		if (pWickedObject) return pWickedObject;
	}
	for (sFrame* pChild = pFrame->pChild; pChild; pChild = pChild->pSibling)
	{
		wiScene::ObjectComponent* pWickedObject = WickedCall_FindFrameObject(pChild);
		if (pWickedObject) return pWickedObject;
	}
	return NULL;
}

float WickedCall_GetLimbAlpha(sObject* pObject, int iLimb)
{
	if (!pObject || !pObject->ppFrameList || iLimb < 0 || iLimb >= pObject->iFrameCount || !pObject->ppFrameList[iLimb]) return 1.0f;
	wiScene::ObjectComponent* pWickedObject = WickedCall_FindFrameObject(pObject->ppFrameList[iLimb]);
	if (!pWickedObject) return 1.0f;
	return pWickedObject->color.w * pWickedObject->color.w;
}

void WickedCall_SetObjectHighlightColor(sObject* pObject, bool bHighlight, int highlightColorType)
{
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	if (rootEntity > 0) 
	{
		wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(rootEntity);
		if (bHighlight) 
		{
			if (pWickedObject) pWickedObject->SetUserStencilRef((EDITORSTENCILREF)highlightColorType);
		}
		else 
		{
			if (pWickedObject) pWickedObject->SetUserStencilRef(EDITORSTENCILREF_CLEAR);
		}
	}
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		if (pObject->ppFrameList && pObject->ppFrameList[iF])
		{
			uint64_t objectEntity = pObject->ppFrameList[iF]->wickedobjindex;
			if (objectEntity > 0 && rootEntity != objectEntity)
			{
				wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(objectEntity);
				if (bHighlight) 
				{
					if (pWickedObject) pWickedObject->SetUserStencilRef((EDITORSTENCILREF)highlightColorType);
				}
				else 
				{
					if (pWickedObject) pWickedObject->SetUserStencilRef(EDITORSTENCILREF_CLEAR);
				}
			}
		}
	}
}

void WickedCall_SetObjectHighlightRed(sObject* pObject, bool bHighlight)
{
	return WickedCall_SetObjectHighlightColor (pObject, bHighlight, (int)EDITORSTENCILREF_HIGHLIGHT_OBJECT_RED);
}

void WickedCall_SetObjectHighlightBlue(sObject* pObject, bool bHighlight)
{
	return WickedCall_SetObjectHighlightColor (pObject, bHighlight, (int)EDITORSTENCILREF_HIGHLIGHT_OBJECT_BLUE);
}

void WickedCall_SetObjectHighlight(sObject* pObject, bool bHighlight)
{
	uint64_t rootEntity = WickedCall_GetFirstRootEntityID(pObject);
	if (rootEntity > 0) 
	{
		wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(rootEntity);
		if (bHighlight) 
		{
			if (pWickedObject) pWickedObject->SetUserStencilRef(EDITORSTENCILREF_HIGHLIGHT_OBJECT);
		}
		else 
		{
			if (pWickedObject)
				pWickedObject->SetUserStencilRef(EDITORSTENCILREF_CLEAR);
		}
	}
	
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		if (pObject->ppFrameList && pObject->ppFrameList[iF])
		{
			uint64_t objectEntity = pObject->ppFrameList[iF]->wickedobjindex;
			if (objectEntity > 0 && rootEntity != objectEntity)
			{
				wiScene::ObjectComponent* pWickedObject = wiScene::GetScene().objects.GetComponent(objectEntity);
				if (bHighlight) {
					if (pWickedObject) pWickedObject->SetUserStencilRef(EDITORSTENCILREF_HIGHLIGHT_OBJECT);
				}
				else {
					if (pWickedObject) pWickedObject->SetUserStencilRef(EDITORSTENCILREF_CLEAR);
				}
			}
		}
	}

}


void WickedCall_DrawObjctBox_CHECK_IF_WE_HAVE_A_GG_COLLISION_PROBLEM(sObject* pObject, XMFLOAT4 color)
{
	if (!pObject) return;

	GGMATRIX matARotation;
	GGVECTOR3 box1;
	GGMATRIX matRotateX, matRotateY, matRotateZ;
	if (pObject->position.bFreeFlightRotation)
	{
		matARotation = pObject->position.matFreeFlightRotate;
	}
	else
	{
		GGMatrixRotationX(&matRotateX, GGToRadian(pObject->position.vecRotate.x));	// x rotation
		GGMatrixRotationY(&matRotateY, GGToRadian(pObject->position.vecRotate.y));	// y rotation
		GGMatrixRotationZ(&matRotateZ, GGToRadian(pObject->position.vecRotate.z));	// z rotation
		matARotation = matRotateX * matRotateY * matRotateZ;
	}
	if (pObject->position.bApplyPivot)
	{
		matARotation *= pObject->position.matPivot;
	}

	AABB aabb;
	aabb._min.x = pObject->collision.vecMin.x;
	aabb._min.y = pObject->collision.vecMin.y;
	aabb._min.z = pObject->collision.vecMin.z;
	aabb._max.x = pObject->collision.vecMax.x;
	aabb._max.y = pObject->collision.vecMax.y;
	aabb._max.z = pObject->collision.vecMax.z;

	box1.x = aabb._min.x; box1.y = aabb._min.y; box1.z = aabb._min.z;
	GGVec3TransformCoord(&box1, &box1, &matARotation);
	aabb._min.x = box1.x; aabb._min.y = box1.y; aabb._min.z = box1.z;
	box1.x = aabb._max.x; box1.y = aabb._max.y; box1.z = aabb._max.z;
	GGVec3TransformCoord(&box1, &box1, &matARotation);
	aabb._max.x = box1.x; aabb._max.y = box1.y; aabb._max.z = box1.z;
	
	aabb._min.x = (aabb._min.x * pObject->position.vecScale.x) + pObject->position.vecPosition.x;
	aabb._min.y = (aabb._min.y * pObject->position.vecScale.y) + pObject->position.vecPosition.y;
	aabb._min.z = (aabb._min.z * pObject->position.vecScale.z) + pObject->position.vecPosition.z;
	aabb._max.x = (aabb._max.x * pObject->position.vecScale.x) + pObject->position.vecPosition.x;
	aabb._max.y = (aabb._max.y * pObject->position.vecScale.y) + pObject->position.vecPosition.y;
	aabb._max.z = (aabb._max.z * pObject->position.vecScale.z) + pObject->position.vecPosition.z;

	XMFLOAT4X4 hoverBox;
	XMStoreFloat4x4(&hoverBox, aabb.getAsBoxMatrix());
	wiRenderer::DrawBox(hoverBox, color);
}


void WickedCall_DrawObjctBox(sObject* pObject, XMFLOAT4 color, bool bThickLine, bool ForceBox)
{
	if (!pObject) return;

	if(!bUseEditorOutlineSelection())
		ForceBox = true;
	if (!ForceBox || bImGuiInTestGame)
	{
		if (ObjectExist(pObject->dwObjectNumber))
		{
			g_ObjectHighlightList.push_back(pObject->dwObjectNumber);
			if(color.x==1.0 && color.y==0.0 && color.z==0.0)
				WickedCall_SetObjectHighlightRed(pObject, true);
			else
				WickedCall_SetObjectHighlight(pObject, true);
		}
		return;
	}
	
	if (color.w == 0.0) return; //PE: Disable box if no color.

	AABB aabb;
	aabb._min.x = pObject->collision.vecMin.x;
	aabb._min.y = pObject->collision.vecMin.y;
	aabb._min.z = pObject->collision.vecMin.z;
	aabb._max.x = pObject->collision.vecMax.x;
	aabb._max.y = pObject->collision.vecMax.y;
	aabb._max.z = pObject->collision.vecMax.z;

	XMFLOAT4X4 hoverBox;

	XMMATRIX rot;
#ifndef MATCHCLASSICROTATION
	//PE: For some reaon we need ZXY rotation ?
	rot = XMMatrixRotationZ(GGToRadian(pObject->position.vecRotate.z));
	rot = rot * XMMatrixRotationX(GGToRadian(pObject->position.vecRotate.x));
	rot = rot * XMMatrixRotationY(GGToRadian(pObject->position.vecRotate.y));
#else
	rot = XMMatrixRotationX(GGToRadian(pObject->position.vecRotate.x));
	rot = rot * XMMatrixRotationY(GGToRadian(pObject->position.vecRotate.y));
	rot = rot * XMMatrixRotationZ(GGToRadian(pObject->position.vecRotate.z));
#endif

	GGVECTOR3 vObjectCenter = pObject->collision.vecCentre;

	XMFLOAT3 ext = aabb.getHalfWidth();
	XMMATRIX sca = XMMatrixScaling(ext.x*pObject->position.vecScale.x, ext.y*pObject->position.vecScale.y, ext.z*pObject->position.vecScale.z);
	XMMATRIX tra = XMMatrixTranslation(pObject->collision.vecCentre.x*pObject->position.vecScale.x, pObject->collision.vecCentre.y*pObject->position.vecScale.y, pObject->collision.vecCentre.z*pObject->position.vecScale.z) * rot;

	XMStoreFloat4x4(&hoverBox, sca * tra);

	hoverBox._41 += pObject->position.vecPosition.x;
	hoverBox._42 += pObject->position.vecPosition.y;
	hoverBox._43 += pObject->position.vecPosition.z;

	wiRenderer::DrawBox(hoverBox, color);

	if (bThickLine)
	{
		float seperate_line = 0.05; //0.1;
		hoverBox._41 += seperate_line;
		hoverBox._42 += seperate_line;
		hoverBox._43 += seperate_line;
		wiRenderer::DrawBox(hoverBox, color);
		hoverBox._41 -= seperate_line * 2.0;
		hoverBox._42 -= seperate_line * 2.0;
		hoverBox._43 -= seperate_line * 2.0;
		wiRenderer::DrawBox(hoverBox, color);
		hoverBox._42 += seperate_line;
		wiRenderer::DrawBox(hoverBox, color);
		hoverBox._41 += seperate_line * 2.0;
		hoverBox._43 += seperate_line * 2.0;
		wiRenderer::DrawBox(hoverBox, color);
	}

}


void WickedCall_DrawPoint(float fx, float fy, float fz, float size, XMFLOAT4 color, bool bThickLine)
{

	AABB aabb;
	aabb._min.x = fx- size;
	aabb._min.y = fy- size;
	aabb._min.z = fz- size;
	aabb._max.x = fx+ size;
	aabb._max.y = fy+ size;
	aabb._max.z = fz+ size;

	XMFLOAT4X4 hoverBox;

	XMFLOAT3 ext = aabb.getHalfWidth();
	XMMATRIX sca = XMMatrixScaling(ext.x*1.0, ext.y*1.0, ext.z*1.0);
	XMMATRIX tra = XMMatrixTranslation(0.0, 0.0, 0.0);

	XMStoreFloat4x4(&hoverBox, sca * tra);

	hoverBox._41 += fx;
	hoverBox._42 += fy;
	hoverBox._43 += fz;

	wiRenderer::DrawBox(hoverBox, color);

	if (bThickLine)
	{
		float seperate_line = 0.1;
		hoverBox._41 += seperate_line;
		hoverBox._42 += seperate_line;
		hoverBox._43 += seperate_line;
		wiRenderer::DrawBox(hoverBox, color);
		hoverBox._41 -= seperate_line * 2.0;
		hoverBox._42 -= seperate_line * 2.0;
		hoverBox._43 -= seperate_line * 2.0;
		wiRenderer::DrawBox(hoverBox, color);
		hoverBox._42 += seperate_line;
		wiRenderer::DrawBox(hoverBox, color);
		hoverBox._41 += seperate_line * 2.0;
		hoverBox._43 += seperate_line * 2.0;
		wiRenderer::DrawBox(hoverBox, color);
	}

}

void WickedCall_DrawObjctBox_Color(sObject* pObject, float r, float g, float b, float a)
{
	XMFLOAT4 color = { r,g,b,a };
	WickedCall_DrawObjctBox(pObject, color);
}

float WickedCall_GetVRAMUsageMB( void )
{
	// real usage of THIS process on the device Wicked actually created (D3D12MA
	// budget) — the legacy DXGI EnumAdapters(0) query can watch the wrong GPU
	wiGraphics::GraphicsDevice* device = wiGraphics::GetDevice();
	if ( device == nullptr ) return 0.0f;
	wiGraphics::GraphicsDevice::MemoryUsage mem = device->GetMemoryUsage();
	return (float)( mem.usage / (1024.0 * 1024.0) );
}

void WickedCall_RenderEditorFunctions( void )
{
	// if shooter genre mode active, show all logic objects
	extern bool Shooter_Tools_Window;
	if (Shooter_Tools_Window == true)
	{
		// only logic highlights
		Wicked_Highlight_ClearAllObjects();
		Wicked_Highlight_AllLogicObjects();
		return;
	}

	// Remove all highlights
	if (g_ObjectHighlightList.size() > 0)
	{
		for (int i = 0; i < (int)g_ObjectHighlightList.size(); i++)
		{
			int obj = g_ObjectHighlightList[i];
			if (obj > 0)
			{
				if (ObjectExist(obj))
				{
					sObject* pObject = (sObject*)GetObjectsInternalData(obj);
					if (pObject)
						WickedCall_SetObjectHighlight(pObject, false);
				}
			}
		}
		g_ObjectHighlightList.clear();
	}

	// exit early if not in gridedit selection mode, in procedural level creation mode or in test level mode
	if (iGetgrideditselect() != 5)
		return;
	if (bProceduralLevel)
		return;
	if (bImGuiInTestGame)
		return;

	// highlight selected object too
	if (g_selected_pobject) 
	{
		WickedCall_DrawObjctBox(g_selected_pobject, XMFLOAT4(0.25f, 1.0f, 0.25f, 0.5f));
	}

	// highlight editor selection too
	if (g_selected_editor_object) 
	{
		try
		{
			if (ObjectExist(g_selected_editor_objectID))
			{
				int iObjNo = g_selected_editor_object->dwObjectNumber;
				if (iObjNo > 0 && iObjNo <= 300000)
				{
					WickedCall_DrawObjctBox(g_selected_editor_object, g_selected_editor_color);
					WickedCall_DrawObjctBox(g_selected_editor_object, g_selected_editor_color, true, true);
				}
			}
		}
		catch(...)
		{
			// error trapping in case object is deleted elsewhere!
			g_selected_editor_object = NULL;
			g_selected_editor_objectID = 0;
		}
	}

	// highlight highlighted object too
	if (g_highlight_pobject) 
	{
		#ifdef ONLY_USE_OUTLINE_HIGHLIGHT
		WickedCall_DrawObjctBox(g_highlight_pobject, XMFLOAT4(1.0f, 0.75f, 0.0f, 0.85f), true, false);
		#else
		WickedCall_DrawObjctBox(g_highlight_pobject, XMFLOAT4(1.0f, 0.75f, 0.0f, 0.85f), true, true);
		#endif
	}

	// process highlights for rubber banded objects, selections and locked objects
	Wicked_Highlight_Rubberband();
	Wicked_Highlight_Selection();
	Wicked_Highlight_LockedList();
}

void Wicked_Update_Shadows(void *voidvisual);
uint64_t WickedCall_AddLight(int iLightType)
{
	Entity light = wiScene::GetScene ( ).Entity_CreateLight ( "light", XMFLOAT3 ( 0, 0, 0 ), XMFLOAT3 ( 1, 1, 1 ), 1, 500 );
	LightComponent* lightComponent = wiScene::GetScene ( ).lights.GetComponent ( light );
	lightComponent->_flags = 0;
	lightComponent->SetType ( (wiScene::LightComponent::LightType)iLightType );
	// GGMAX 2.10: under the DX11 falloff parity curve, intensity is in DX11 energy units and
	// the DX11 product created every map light at energy 30 (its wickedcalls.cpp:6655).
	// UpdateLight re-asserts this every push; set it here too so a light is never bright for
	// the frames before its first lighting_loop update.
	if (wi::renderer::gg_dx11_light_falloff)
		lightComponent->intensity = 30.0f;
	else
		lightComponent->BackCompatSetEnergy(60);
	Wicked_Update_Shadows(NULL);
	return light;
}

void WickedCall_DeleteLight(uint64_t wickedlightindex)
{
	wiScene::GetScene().Entity_Remove(wickedlightindex);
	//PE: When creating light also update probes.
	WickedCall_UpdateProbes();

	
}

//PE: Calculate needed light textures.
int WickedCall_Get2DShadowLights(void)
{
	wiScene::Scene* pScene = &wiScene::GetScene();
	int lights = pScene->lights.GetCount();
	int shadows = 0;
	for (int i = 0; i < lights; i++)
	{
		if (pScene->lights[i].GetType() == ENTITY_TYPE_SPOTLIGHT)
		{
			if (pScene->lights[i].IsCastingShadow()) shadows++;
		}
	}
	return(shadows);
}

int WickedCall_GetCubeShadowLights(bool bDebug)
{
	wiScene::Scene* pScene = &wiScene::GetScene();
	int lights = pScene->lights.GetCount();
	int shadows = 0;
	for (int i = 0; i < lights; i++)
	{
		if (pScene->lights[i].GetType() == ENTITY_TYPE_POINTLIGHT)
		{
			if (pScene->lights[i].IsCastingShadow())
			{
				shadows++;
				if (bDebug)
				{
					XMFLOAT4X4 hoverBox;
					AABB aabb = pScene->aabb_lights[i];
					XMStoreFloat4x4(&hoverBox, aabb.getAsBoxMatrix());
					XMFLOAT4 color = { 1,0,0,1 };
					wiRenderer::DrawBox(hoverBox, color);
				}
			}
		}
	}
	return(shadows);
}

int WickedCall_GetSpotShadowLights(bool bDebug)
{
	wiScene::Scene* pScene = &wiScene::GetScene();
	int lights = pScene->lights.GetCount();
	int shadows = 0;
	for (int i = 0; i < lights; i++)
	{
		if (pScene->lights[i].GetType() == ENTITY_TYPE_SPOTLIGHT)
		{
			if (pScene->lights[i].IsCastingShadow())
			{
				shadows++;
				if (bDebug)
				{
					XMFLOAT4X4 hoverBox;
					AABB aabb = pScene->aabb_lights[i];
					XMStoreFloat4x4(&hoverBox, aabb.getAsBoxMatrix());
					XMFLOAT4 color = { 1,0,0,1 };
					wiRenderer::DrawBox(hoverBox, color);
				}
			}
		}
	}
	return(shadows);
}


// Wicked fills visibility_main.visibleObjects during UpdateVisibility() with the scene-object
// indices that survived main-camera frustum culling. Total objects minus this == frustum-culled.
// Exposes the real count for the editor "Frustum/Apparent Culled" readout, which the DX12 port
// had left hardcoded to 0 (GameGuru's legacy CPU frustum cull is gone; Wicked culls internally).
int WickedCall_GetFrustumVisibleObjects(void)
{
	return (int)master.masterrenderer.visibility_main.visibleObjects.size();
}

void WickedCall_UpdateLight(uint64_t wickedlightindex, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, float fRange, float fSpotRadius, int iColR, int iColG, int iColB, bool bCastShadow)
{
	bool bLightScapeHasChangedEnoughForEnvProbeUpdate = false;

	LightComponent* lightComponent = wiScene::GetScene ( ).lights.GetComponent ( wickedlightindex );
	lightComponent->SetCastShadow ( bCastShadow );
	// 2026-08-05 SPOT CONE FIX: GG's spot "cone angle" (light.offsetup, FULL angle in
	// degrees) went into the OLD Wicked's LightComponent::fov, whose semantic WAS the full
	// angle (old engine: cone cos = cos(fov*0.5), projection fov = fov). New Wicked
	// replaced fov with outerConeAngle = the HALF angle (cone cos = cos(outer), projection
	// fov = outer*2); the port renamed WITHOUT halving, so every spot ran at DOUBLE its
	// authored cone, and any cone authored >=90 made GetConeAngleCos() zero/negative -
	// which breaks the Forward+ tile-culling cone sphere (r = range*0.5/cos^2: infinite or
	// mirrored) and the shadow projection (fov = outer*2 >= 180 = degenerate matrix). Seen
	// as the Snowy Mountain Stroll start-room "shadow flicker on mouselook": the spot's
	// lighting/shadow dropped per screen tile depending on camera pose. Half it, and clamp
	// to a valid projection range.
	{
		float fHalfConeDeg = fSpotRadius * 0.5f;
		if (fHalfConeDeg < 1.0f) fHalfConeDeg = 1.0f;
		if (fHalfConeDeg > 85.0f) fHalfConeDeg = 85.0f;
		lightComponent->outerConeAngle = GGToRadian(fHalfConeDeg);
	}
	lightComponent->color = XMFLOAT3((float)iColR / 255.0f, (float)iColG / 255.0f, (float)iColB / 255.0f);

	// GGMAX 2.10 LIGHT POWER PARITY: the DX11 product ran EVERY point/spot light at a constant
	// energy of 30 (set once in WickedCall_AddLight via Entity_CreateLight, DX11
	// wickedcalls.cpp:6655 — UpdateLight never touched it) and shaded it as
	// energy*(1-d²/r²)² with NO inverse-square term. Range alone shaped the curve. The engine
	// now carries that exact curve behind OPTION_BIT_GG_DX11_LIGHT_FALLOFF
	// (wi::renderer::gg_dx11_light_falloff, lightingHF.hlsli), in which intensity is in DX11
	// energy units 1:1 — so the DX11 constant passes straight through. The old range²×π/4
	// heuristic below could never reproduce the SHAPE of the DX11 falloff on upstream's
	// windowed 1/d² (mid-range flood ~half brightness collapsing into a hot pool at the
	// source — user-reported vs the DX11 spotshadowtest baseline 2026-08-07), and its premise
	// was wrong besides: DX11's BRDF_GetDiffuse DID divide by PI (WickedRepo brdf.hlsli:449).
	// It is kept only as the shader-side revert pair (setup.ini lightfalloff=0).
	if (wi::renderer::gg_dx11_light_falloff)
	{
		lightComponent->intensity = 30.0f; // DX11 energy units under the parity curve
	}
	else if (fRange > 0.1f)
	{
		float fIntensity = fRange * fRange * 0.785f; // PI/4 ≈ 0.785
		if (fIntensity < 600.0f) fIntensity = 600.0f;
		if (fIntensity > 60000.0f) fIntensity = 60000.0f;
		if (lightComponent->GetType() == LightComponent::SPOT)
		{
			fIntensity = fRange * fRange * 7.85f; // spots need ~10x more (cone focus)
			if (fIntensity < 6000.0f) fIntensity = 6000.0f;
			if (fIntensity > 60000.0f) fIntensity = 60000.0f;
		}
		lightComponent->intensity = fIntensity;
	}

	// for now, only change env probe if light on/off
	TransformComponent* transformLight = wiScene::GetScene ().transforms.GetComponent (wickedlightindex);
	bool bDetectOnOffNotRangeChange = false;
	if (lightComponent->range != fRange)
	{
		if (lightComponent->range > 1 && fRange <= 1) bDetectOnOffNotRangeChange = true;
		if (lightComponent->range <= 1 && fRange > 1) bDetectOnOffNotRangeChange = true;
		lightComponent->range = fRange;
	}
	float fCurrentX = transformLight->GetPosition().x;
	if (fCurrentX <= -999999.0 && fX > -999999.0) bDetectOnOffNotRangeChange = true;
	if (fCurrentX > -999999.0 && fX <= -999999.0) bDetectOnOffNotRangeChange = true;
	if (bDetectOnOffNotRangeChange == true)
	{
		bLightScapeHasChangedEnoughForEnvProbeUpdate = true;
	}

	transformLight->ClearTransform();
	transformLight->Translate(XMFLOAT3(fX, fY, fZ));

	if (lightComponent->GetType() == LightComponent::SPOT)
	{
		// fAX, fAY, fAZ is a directional normal pointing in direction of spot light
		XMMATRIX rot;
		rot = XMMatrixRotationX(GGToRadian(fAX-90.0)); //Match the spot light object.
		rot = rot * XMMatrixRotationY(GGToRadian(fAY));
		rot = rot * XMMatrixRotationZ(GGToRadian(fAZ));
		XMVECTOR S;
		XMVECTOR R;
		XMVECTOR T;
		XMMatrixDecompose(&S, &R, &T, rot);
		transformLight->Rotate(R);
	}
	transformLight->SetDirty();

	// when light-scape changes, refresh env probes (performance hit here)
	if (bLightScapeHasChangedEnoughForEnvProbeUpdate == true)
	{
		extern bool g_bLightProbeInstantChange;
		g_bLightProbeInstantChange = true;
	}
}

void WickedCall_UpdateProbes(void)
{
	GGTerrain::ggterrain_extra_params.bUpdateProbes = true;
}

void WickedCall_EnableCameraLight(bool On)
{
	if (On)
	{
		if (!g_entityCameraLight)
		{
			g_entityCameraLight = WickedCall_AddLight(2);
			g_entityCameraLight = wiScene::GetScene().Entity_CreateLight("cameraLight", XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1), 8, 2900);
			LightComponent* lightCamera = wiScene::GetScene().lights.GetComponent(g_entityCameraLight);
			lightCamera->SetCastShadow(false); //PE: No need for shadows its a editor light.
			lightCamera->color = XMFLOAT3(0.65, 0.65, 0.65); //PE: Dont make it to bright.
		}
	}
	else {
		if (g_entityCameraLight)
		{
			WickedCall_DeleteLight(g_entityCameraLight);
			g_entityCameraLight = NULL;
		}
	}
}

void WickedCall_EnableThumbLight(bool On)
{
	if (On)
	{
		if (!g_entityThumbLight)
		{
			//g_entityThumbLight = WickedCall_AddLight(2);
			g_entityThumbLight = wiScene::GetScene().Entity_CreateLight("thumbLight", XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1), 8, 2900);
			LightComponent* lightCamera = wiScene::GetScene().lights.GetComponent(g_entityThumbLight);
			lightCamera->SetCastShadow(false); //PE: No need for shadows its a editor light.
			lightCamera->color = XMFLOAT3(0.65, 0.65, 0.65); //PE: Dont make it to bright.
			g_bLightShaftState = GetLightShaftState();
			g_bLensFlareState = GetLensFlareState();
		}
		if (!g_entityThumbLight2)
		{
			g_entityThumbLight2 = wiScene::GetScene().Entity_CreateLight("thumbLight2", XMFLOAT3(0, 0, 0), XMFLOAT3(1, 1, 1), 8, 2900);
			LightComponent* lightCamera = wiScene::GetScene().lights.GetComponent(g_entityThumbLight2);
			lightCamera->SetCastShadow(false); //PE: No need for shadows its a editor light.
			lightCamera->color = XMFLOAT3(0.65, 0.65, 0.65); //PE: Dont make it to bright.
			g_bLightShaftState = GetLightShaftState();
			g_bLensFlareState = GetLensFlareState();
		}
		SetLightShaftState(false);
		SetLensFlareState(false);
	}
	else 
	{
		if (g_entityThumbLight)
		{
			WickedCall_DeleteLight(g_entityThumbLight);
			g_entityThumbLight = NULL;
		}
		if (g_entityThumbLight2)
		{
			WickedCall_DeleteLight(g_entityThumbLight2);
			g_entityThumbLight2 = NULL;
		}
		SetLightShaftState(g_bLightShaftState);
		SetLensFlareState(g_bLensFlareState);
	}
}

void WickedCall_SetEditorCameraLight(bool bSwitchOn)
{
	LightComponent* lightCamera = wiScene::GetScene ( ).lights.GetComponent ( g_entityCameraLight );
	if (lightCamera)
	{
		if (bSwitchOn == true)
			lightCamera->color = XMFLOAT3(1, 1, 1);
		else
			lightCamera->color = XMFLOAT3(0, 0, 0);
	}
}

void WickedCall_SetSpriteBoundBox(bool bShow,float fX1, float fY1,float fX2, float fY2)
{
	//PE: Wicked sprite do not support DPI and placement is wrong. like if you set windows DPI to 150%.
	//PE: Use imgui to draw it.
	void DrawRubberBand(float fx, float fy, float fx2, float fy2);
	if (bShow == true)
	{
		float fMouseCenterOffset = 6.0f;
		fX1 -= fMouseCenterOffset;
		fY1 -= fMouseCenterOffset;
		fX2 -= fMouseCenterOffset;
		fY2 -= fMouseCenterOffset;
		DrawRubberBand(fX1, fY1, fX2, fY2);
	}
}

void WickedCall_SetSunDirection(float fAx, float fAy, float fAz)
{
	TransformComponent* transformSunLight = wiScene::GetScene().transforms.GetComponent(g_entitySunLight);
	transformSunLight->ClearTransform();
	XMFLOAT3 rotationinrads;
	rotationinrads.x = fAx * (3.141592654f / 180.0f);
	rotationinrads.y = fAy * (3.141592654f / 180.0f);
	rotationinrads.z = fAz * (3.141592654f / 180.0f);
	transformSunLight->RotateRollPitchYaw(rotationinrads);

	// a moved sun makes frozen staggered-cascade contents stale; change-latched
	// because this is called on every visuals apply
	static float s_lastSunAx = -99999.0f, s_lastSunAy = -99999.0f, s_lastSunAz = -99999.0f;
	if (fAx != s_lastSunAx || fAy != s_lastSunAy || fAz != s_lastSunAz)
	{
		s_lastSunAx = fAx; s_lastSunAy = fAy; s_lastSunAz = fAz;
		wi::renderer::InvalidateDelayedShadowCascades();
	}
}

void WickedCall_SetSunColors(float fRed, float fGreen, float fBlue,float fEnergy,float fFov, float fShadowBias)
{
	LightComponent* lightSun = wiScene::GetScene().lights.GetComponent(g_entitySunLight);
	lightSun->color.x = fRed;
	lightSun->color.y = fGreen;
	lightSun->color.z = fBlue;
	lightSun->intensity = fEnergy;
	lightSun->outerConeAngle = fFov;
}
#include "master.h"

void WickedCall_SetVisualizerEnabled(bool bVisualizer)
{
	LightComponent* lightSun = wiScene::GetScene().lights.GetComponent(g_entitySunLight);
	lightSun->SetVisualizerEnabled(bVisualizer);

}

void WickedCall_SunSetVolumetricsEnabled(bool bVolumetrics)
{
	LightComponent* lightSun = wiScene::GetScene().lights.GetComponent(g_entitySunLight);
	lightSun->SetVolumetricsEnabled(bVolumetrics);
}

void WickedCall_SunSetSetStatic(bool bSetStatic)
{
	LightComponent* lightSun = wiScene::GetScene().lights.GetComponent(g_entitySunLight);
	lightSun->SetStatic(bSetStatic);
}
void WickedCall_SunSetRange(float fRange)
{
	LightComponent* lightSun = wiScene::GetScene().lights.GetComponent(g_entitySunLight);
	lightSun->range = fRange;
}

void WickedCall_SetTextureName(int obj, char *texturename)
{
	sObject* GetObjectData(int iID);
	sObject* pObject = GetObjectData(obj);
	if (pObject)
	{
		for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
		{
			sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
			if (pMesh && pMesh->pTextures)
			{
				strcpy(pMesh->pTextures[0].pName, texturename);
			}
		}
	}
}


void WickedCall_DrawImguiNow(void)
{
	// Phase 5: ImGui rendering is now done in MasterRenderer::Compose() via the DX12 backend.
	// This function is kept for compatibility but is a no-op — the actual rendering happens
	// through the WickedEngine render pipeline using ImGui_DX12_RenderBridge().

	// TODO: removed DX11 ImGui path
	// void ImGuiHook_RenderCall_Direct(void* ctxptr, void* d3dptr);
	// extern LPGGDEVICE            m_pD3D;
	// extern LPGGIMMEDIATECONTEXT  m_pImmediateContext;
	// ImGuiHook_RenderCall_Direct((void*)m_pImmediateContext, (void*)m_pD3D);
}

//PE: Changing the far plane in indoor levels can really boost the FPS.
void WickedCall_SetCameraFarPlanes(float farplane)
{
	float fNear = wiScene::GetCamera().zNearP;
	float fFar = farplane;
	float fCameraFov = wiScene::GetCamera().fov;
	wiScene::GetCamera().CreatePerspective((float)master.masterrenderer.GetLogicalWidth(), (float)master.masterrenderer.GetLogicalHeight(), fNear, fFar, fCameraFov);
}

void WickedCall_SetCameraFOV ( float fFOV )
{
	// from wicked camera
	float fNear = wiScene::GetCamera().zNearP;
	float fFar = wiScene::GetCamera().zFarP;

	// directly set camera FOV (ingame weapon FOV changes)
	float fCameraFov;
	extern bool bImGuiInTestGame;
	float fUsedFOV = fFOV;
	if (bImGuiInTestGame == false) fUsedFOV = 45;
	if (bImGuiInTestGame == true)
	{
		// when in game, weapon FOV correction
		fCameraFov = GGToRadian(fUsedFOV); // Oops - backwards logic, lower FOV needs lower angle passed in
	}
	else
	{
		// keep consistency with editor, even though reversed
		fCameraFov = XM_PI / (fUsedFOV / 15.0f); //Fit GG settings.
	}
	wiScene::GetCamera().CreatePerspective((float)master.masterrenderer.GetLogicalWidth(), (float)master.masterrenderer.GetLogicalHeight(), fNear, fFar, fCameraFov);
}

int WickedCall_GetSkinable(void)
{
	int iSkinable = 0;
	wiScene::Scene* pScene = &wiScene::GetScene();
	for (size_t i = 0; i < pScene->meshes.GetCount(); ++i)
	{
		Entity entity = pScene->meshes.GetEntity(i);
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(entity);
		//MeshComponent * mesh = pScene->meshes[i];
		if (mesh)
		{
			if (mesh->IsSkinned() && pScene->armatures.Contains(mesh->armatureID))
			{
				iSkinable++;
			}
		}
	}
	return(iSkinable);
}

int WickedCall_GetSkinableVisible(void)
{
	int iSkinable = 0;
	wiScene::Scene* pScene = &wiScene::GetScene();
	for (size_t i = 0; i < pScene->meshes.GetCount(); ++i)
	{
		Entity entity = pScene->meshes.GetEntity(i);
		wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(entity);
		//MeshComponent * mesh = pScene->meshes[i];
		if (mesh)
		{
			if (mesh->IsRenderable() && mesh->IsSkinned() && pScene->armatures.Contains(mesh->armatureID))
			{
				iSkinable++;
			}
		}
	}
	return(iSkinable);
}

void WickedCall_SetShadowRange(float ShadowFar)
{
	fWickedCallShadowFarPlane = ShadowFar;

	// DX11 parity (2026-07-18): the old engine consumed fWickedCallShadowFarPlane
	// inside CreateDirLightShadowCams with hand-tuned GGREDUCED cascade splits
	// (WickedRepo wiRenderer.cpp: 0/380/950/7500/30000/farPlane, 5 cascades).
	// The new engine ignores that global entirely — cascades come from each
	// light's cascade_distances, whose stock default {8,80,800} is ~20 metres
	// in GG's inch-scaled world. That left DX12 shadows stopping just past the
	// nearest trees. Recreate the production splits on the sun light here —
	// this is the same choke point production used (visuals apply/level load).
	wiScene::LightComponent* lightSun = wiScene::GetScene().lights.GetComponent(g_entitySunLight);
	if (lightSun)
	{
		float farCascade = ShadowFar;
		if (farCascade < 30001.0f) farCascade = 30001.0f;
		lightSun->cascade_distances = { 380.0f, 950.0f, 7500.0f, 30000.0f, farCascade };

		// changed splits make frozen staggered-cascade contents stale;
		// change-latched because this is called on every visuals apply
		static float s_lastFarCascade = -1.0f;
		if (farCascade != s_lastFarCascade)
		{
			s_lastFarCascade = farCascade;
			wi::renderer::InvalidateDelayedShadowCascades();
		}
	}
}


bool bCubesVisible = false; //PE: Default off.
void WickedCall_DisplayCubes(bool Visible)
{
	if (Visible) {
		bCubesVisible = Visible;
		return;
	}
	if (bCubesVisible && !Visible)
	{
		bCubesVisible = Visible;
		void set_terrain_sculpt_mode(int mode);
		void set_terrain_edit_mode(int mode);
		void clear_highlighted_tree(void);
		int get_terrain_sculpt_mode(void);
		extern int iLastTerrainSculptMode;
		if(iLastTerrainSculptMode == -1)
			iLastTerrainSculptMode = get_terrain_sculpt_mode();
		set_terrain_sculpt_mode(0); // GGTERRAIN_SCULPT_NONE; Disable terrain sculpt circle.
		set_terrain_edit_mode(0); // GGTERRAIN_EDIT_NONE; Disable terrain paint circle.
		clear_highlighted_tree();

		//PE: Not sure if we are going to use the below for grass ? so keep it here.

		Scene& scene = wiScene::GetScene();
		for (unsigned int i = 0; i < 32; i++)
		{
			Entity entity;
			TransformComponent* transform = NULL;

			entity = scene.Entity_FindByName("CubeDisplay_" + std::to_string(i));
			if (entity)
			{
				transform = scene.transforms.GetComponent(entity);
				if (transform)
				{
					transform->ClearTransform();
					transform->Translate(XMFLOAT3(-100000, -100000, -100000));
				}
			}
		}
	}
}

void WickedCall_CreateReflectionProbe(float x, float y, float z,char *name,float size)
{
	// no code was here, can we delete all calls to this function ?
}

void WickedCall_MoveReflectionProbe(float x, float y, float z, char *name, float size)
{
	// no code was here, can we delete all calls to this function ?
}

void WickedCall_DeleteReflectionProbe(char *name)
{
	// no code was here, can we delete all calls to this function ?
}

//PE: Capsule need more work. just some test code.
void WickedCall_DrawObjctCapsule(sObject* pObject, XMFLOAT4 color)
{
	if (!pObject) return;

	AABB aabb;
	aabb._min.x = pObject->collision.vecMin.x;
	aabb._min.y = pObject->collision.vecMin.y;
	aabb._min.z = pObject->collision.vecMin.z;
	aabb._max.x = pObject->collision.vecMax.x;
	aabb._max.y = pObject->collision.vecMax.y;
	aabb._max.z = pObject->collision.vecMax.z;

	XMFLOAT4X4 hoverBox;

	XMMATRIX rot;
#ifndef MATCHCLASSICROTATION
	//PE: For some reaon we need ZXY rotation ?
	rot = XMMatrixRotationZ(GGToRadian(pObject->position.vecRotate.z));
	rot = rot * XMMatrixRotationX(GGToRadian(pObject->position.vecRotate.x));
	rot = rot * XMMatrixRotationY(GGToRadian(pObject->position.vecRotate.y));
#else
	rot = XMMatrixRotationX(GGToRadian(pObject->position.vecRotate.x));
	rot = rot * XMMatrixRotationY(GGToRadian(pObject->position.vecRotate.y));
	rot = rot * XMMatrixRotationZ(GGToRadian(pObject->position.vecRotate.z));
#endif
	GGVECTOR3 vObjectCenter = pObject->collision.vecCentre;

	XMFLOAT3 ext = aabb.getHalfWidth();
	XMMATRIX sca = XMMatrixScaling(ext.x*pObject->position.vecScale.x, ext.y*pObject->position.vecScale.y, ext.z*pObject->position.vecScale.z);
	XMMATRIX tra = XMMatrixTranslation(pObject->collision.vecCentre.x*pObject->position.vecScale.x, pObject->collision.vecCentre.y*pObject->position.vecScale.y, pObject->collision.vecCentre.z*pObject->position.vecScale.z) * rot;

	XMStoreFloat4x4(&hoverBox, sca * tra);
	XMFLOAT3 pos = XMFLOAT3(pObject->position.vecPosition.x, pObject->position.vecPosition.y, pObject->position.vecPosition.z);
	XMFLOAT3 tip = pos;
	//tip.x += 10.0f;
	tip.y += 1.0f;
	//tip.z += 10.0f;

	CAPSULE capsule = CAPSULE(pos, tip, 40.0f);
	wiRenderer::DrawCapsule(capsule, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) );
}
void CapsuleTest(void)
{
	if (g_selected_editor_object)
	{
		if (ObjectExist(g_selected_editor_objectID) == 1)
		{
			WickedCall_DrawObjctCapsule(g_selected_editor_object, g_selected_editor_color);
		}
	}
}

void WickedCall_SetLDSSkinningEnabled(bool enabled)
{
	//wiRenderer::SetLDSSkinningEnabled(enabled); // removed from wi::renderer
}

void WickedCall_SetLightShaftParameters(float density, float weight, float decay, float exposure)
{
	//LB: master.masterrenderer.setLightShaftValues(density, weight, decay, exposure);
}

void WickedCall_GetLightShaftParameters(float& density, float& weight, float& decay, float& exposure)
{
	//LB: density = master.masterrenderer.GetLightShaftDensity();
	//LB: weight = master.masterrenderer.GetLightShaftWeight();
	//LB: decay = master.masterrenderer.GetLightShaftDecay();
	//LB: exposure = master.masterrenderer.GetLightShaftExposure();
}


constexpr DXGI_FORMAT _ConvertFormat(wiGraphics::Format value)
{
	switch (value)
	{
	case wiGraphics::Format::UNKNOWN:
		return DXGI_FORMAT_UNKNOWN;
		break;
	case wiGraphics::Format::R32G32B32A32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case wiGraphics::Format::R32G32B32A32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
		break;
	case wiGraphics::Format::R32G32B32A32_SINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;
		break;
	case wiGraphics::Format::R32G32B32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
		break;
	case wiGraphics::Format::R32G32B32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
		break;
	case wiGraphics::Format::R32G32B32_SINT:
		return DXGI_FORMAT_R32G32B32_SINT;
		break;
	case wiGraphics::Format::R16G16B16A16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case wiGraphics::Format::R16G16B16A16_UNORM:
		return DXGI_FORMAT_R16G16B16A16_UNORM;
		break;
	case wiGraphics::Format::R16G16B16A16_UINT:
		return DXGI_FORMAT_R16G16B16A16_UINT;
		break;
	case wiGraphics::Format::R16G16B16A16_SNORM:
		return DXGI_FORMAT_R16G16B16A16_SNORM;
		break;
	case wiGraphics::Format::R16G16B16A16_SINT:
		return DXGI_FORMAT_R16G16B16A16_SINT;
		break;
	case wiGraphics::Format::R32G32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
		break;
	case wiGraphics::Format::R32G32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
		break;
	case wiGraphics::Format::R32G32_SINT:
		return DXGI_FORMAT_R32G32_SINT;
		break;
	//case wiGraphics::Format::R32G8X24_TYPELESS: // not in WickedEngine Format enum
	//	return DXGI_FORMAT_R32G8X24_TYPELESS;
	//	break;
	case wiGraphics::Format::D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		break;
	case wiGraphics::Format::R10G10B10A2_UNORM:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
		break;
	case wiGraphics::Format::R10G10B10A2_UINT:
		return DXGI_FORMAT_R10G10B10A2_UINT;
		break;
	case wiGraphics::Format::R11G11B10_FLOAT:
		return DXGI_FORMAT_R11G11B10_FLOAT;
		break;
	case wiGraphics::Format::R8G8B8A8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case wiGraphics::Format::R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		break;
	case wiGraphics::Format::R8G8B8A8_UINT:
		return DXGI_FORMAT_R8G8B8A8_UINT;
		break;
	case wiGraphics::Format::R8G8B8A8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
		break;
	case wiGraphics::Format::R8G8B8A8_SINT:
		return DXGI_FORMAT_R8G8B8A8_SINT;
		break;
	case wiGraphics::Format::R16G16_FLOAT:
		return DXGI_FORMAT_R16G16_FLOAT;
		break;
	case wiGraphics::Format::R16G16_UNORM:
		return DXGI_FORMAT_R16G16_UNORM;
		break;
	case wiGraphics::Format::R16G16_UINT:
		return DXGI_FORMAT_R16G16_UINT;
		break;
	case wiGraphics::Format::R16G16_SNORM:
		return DXGI_FORMAT_R16G16_SNORM;
		break;
	case wiGraphics::Format::R16G16_SINT:
		return DXGI_FORMAT_R16G16_SINT;
		break;
	//case wiGraphics::Format::R32_TYPELESS: // not in WickedEngine Format enum
	//	return DXGI_FORMAT_R32_TYPELESS;
	//	break;
	case wiGraphics::Format::D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
		break;
	case wiGraphics::Format::R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
		break;
	case wiGraphics::Format::R32_UINT:
		return DXGI_FORMAT_R32_UINT;
		break;
	case wiGraphics::Format::R32_SINT:
		return DXGI_FORMAT_R32_SINT;
		break;
	//case wiGraphics::Format::R24G8_TYPELESS: // not in WickedEngine Format enum
	//	return DXGI_FORMAT_R24G8_TYPELESS;
	//	break;
	case wiGraphics::Format::D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
		break;
	case wiGraphics::Format::R8G8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
		break;
	case wiGraphics::Format::R8G8_UINT:
		return DXGI_FORMAT_R8G8_UINT;
		break;
	case wiGraphics::Format::R8G8_SNORM:
		return DXGI_FORMAT_R8G8_SNORM;
		break;
	case wiGraphics::Format::R8G8_SINT:
		return DXGI_FORMAT_R8G8_SINT;
		break;
	//case wiGraphics::Format::R16_TYPELESS: // not in WickedEngine Format enum
	//	return DXGI_FORMAT_R16_TYPELESS;
	//	break;
	case wiGraphics::Format::R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
		break;
	case wiGraphics::Format::D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
		break;
	case wiGraphics::Format::R16_UNORM:
		return DXGI_FORMAT_R16_UNORM;
		break;
	case wiGraphics::Format::R16_UINT:
		return DXGI_FORMAT_R16_UINT;
		break;
	case wiGraphics::Format::R16_SNORM:
		return DXGI_FORMAT_R16_SNORM;
		break;
	case wiGraphics::Format::R16_SINT:
		return DXGI_FORMAT_R16_SINT;
		break;
	case wiGraphics::Format::R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
		break;
	case wiGraphics::Format::R8_UINT:
		return DXGI_FORMAT_R8_UINT;
		break;
	case wiGraphics::Format::R8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
		break;
	case wiGraphics::Format::R8_SINT:
		return DXGI_FORMAT_R8_SINT;
		break;
	case wiGraphics::Format::BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM;
		break;
	case wiGraphics::Format::BC1_UNORM_SRGB:
		return DXGI_FORMAT_BC1_UNORM_SRGB;
		break;
	case wiGraphics::Format::BC2_UNORM:
		return DXGI_FORMAT_BC2_UNORM;
		break;
	case wiGraphics::Format::BC2_UNORM_SRGB:
		return DXGI_FORMAT_BC2_UNORM_SRGB;
		break;
	case wiGraphics::Format::BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM;
		break;
	case wiGraphics::Format::BC3_UNORM_SRGB:
		return DXGI_FORMAT_BC3_UNORM_SRGB;
		break;
	case wiGraphics::Format::BC4_UNORM:
		return DXGI_FORMAT_BC4_UNORM;
		break;
	case wiGraphics::Format::BC4_SNORM:
		return DXGI_FORMAT_BC4_SNORM;
		break;
	case wiGraphics::Format::BC5_UNORM:
		return DXGI_FORMAT_BC5_UNORM;
		break;
	case wiGraphics::Format::BC5_SNORM:
		return DXGI_FORMAT_BC5_SNORM;
		break;
	case wiGraphics::Format::B8G8R8A8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case wiGraphics::Format::B8G8R8A8_UNORM_SRGB:
		return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		break;
	case wiGraphics::Format::BC6H_UF16:
		return DXGI_FORMAT_BC6H_UF16;
		break;
	case wiGraphics::Format::BC6H_SF16:
		return DXGI_FORMAT_BC6H_SF16;
		break;
	case wiGraphics::Format::BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM;
		break;
	case wiGraphics::Format::BC7_UNORM_SRGB:
		return DXGI_FORMAT_BC7_UNORM_SRGB;
		break;
	}
	return DXGI_FORMAT_UNKNOWN;
}

void Wicked_Memory_Use_Textures(void)
{
	void timestampactivity(int i, char* desc_s);
	char timestampMsg[1024];
	sprintf(timestampMsg, "WICKED TEXTURES:");
	timestampactivity(0, timestampMsg);

	//PE: Displaying everything directly is way better.
	wiScene::Scene* pScene = &wiScene::GetScene();
	long usedsize = 0;
	long usedFilesize = 0;
	auto size = pScene->materials.GetCount();
	std::vector<void*> already_registred; //PE: We reuse textures, so ignore when reused.
	for (int i = 0; i < size; i++)
	{
		for (int a = 0; a < 13; a++) //TEXTURESLOT_COUNT = 13
		{
			if (!pScene->materials[i].textures[a].name.empty())
			{
				wiResource image = pScene->materials[i].textures[a].resource;
				if (image.IsValid())
				{
					auto filedata = image.GetFileData().data();
					auto filesize = image.GetFileData().size();

					void *pmat = (void *)pScene->materials[i].textures[a].GetGPUResource();
						// TODO: MaterialGetSRV removed from GraphicsDevice
					ID3D11ShaderResourceView* lpTexture = (ID3D11ShaderResourceView*)nullptr; //wiGraphics::GetDevice()->MaterialGetSRV((void*)pmat);
					bool bAlreadyDisplayed = false;
					for(int b=0;b< already_registred.size();b++)
						if (already_registred[b] == (void*)lpTexture) { bAlreadyDisplayed = true; break; }
					if (!bAlreadyDisplayed)
					{
						already_registred.push_back((void*)lpTexture);

						wiGraphics::Texture texture = image.GetTexture();
						auto Height = texture.desc.height;
						auto Width = texture.desc.width;
						auto Format = texture.desc.format;
						TextureDesc imgdesc = texture.GetDesc();
							// _ConvertFormat was DX11-specific, cast format directly (values match DXGI_FORMAT)
						DXGI_FORMAT DxgiFormat = static_cast<DXGI_FORMAT>(Format);

						int getBitsPerPixel(int fmt);
						float bperpixel = 32;
						if (DxgiFormat != DXGI_FORMAT_UNKNOWN)
						{
							bperpixel = (float)getBitsPerPixel(DxgiFormat) / 8;
							if (bperpixel == 0.5) {
								bperpixel -= 0.125; // remove alpha count from BC1
							}
						}
						int addmipmapssize;

						if (imgdesc.mip_levels > 1) {
							addmipmapssize = (int)((float)(Width*Height) * bperpixel) / 1024 * imgdesc.array_size - 1; // Full mipmaps always give size -1.
							if (addmipmapssize <= 0) addmipmapssize = 0;
						}
						else {
							addmipmapssize = 0;
						}

						usedFilesize += filesize;
						usedsize += ((int)(((float)(Width*Height) * bperpixel) / 1024) * imgdesc.array_size) + addmipmapssize;

						std::string getImageformat(int fmt);
						sprintf(timestampMsg, "WList%d: (%ld,%ld) (%ld kb.+ mipm %ld kb.) filesize %ldkb mipmaps %d array %d format %s \"%s\" (*%ld)", i, Width, Height, (int)((float)(Width*Height) * bperpixel) / 1024 * imgdesc.array_size, addmipmapssize, (long)filesize / 1024.0, imgdesc.mip_levels, imgdesc.array_size, getImageformat(DxgiFormat).c_str(), pScene->materials[i].textures[a].name.c_str(), lpTexture);
						timestampactivity(0, timestampMsg);
					}
				}
			}
		}
	}
	sprintf(timestampMsg, "Total WICKED texture mem used: %ld (%.2fmb) (%.2fgb)", usedsize, (float)usedsize / 1024.0, (float)usedsize / 1024.0 / 1024.0);
	timestampactivity(0, timestampMsg);
	sprintf(timestampMsg, "Total WICKED filedata allocated used: %ld (%.2fmb) (%.2fgb)", usedFilesize, (float)usedFilesize / 1024.0, (float)usedFilesize / 1024.0 / 1024.0);
	timestampactivity(0, timestampMsg);
	sprintf(timestampMsg, "Total: %ld (%.2fmb) (%.2fgb)", usedsize+usedFilesize, (float)(usedsize+usedFilesize) / 1024.0, (float)(usedsize+usedFilesize) / 1024.0 / 1024.0);
	timestampactivity(0, timestampMsg);


}

void WickedCall_SetRenderTargetMouseFocus(bool focus)
{
	bRenderTargetHasFocus = focus;
}



void WickedCall_UpdateWaterColor(float red, float green, float blue)
{
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		XMFLOAT4 oldColor = weather->oceanParameters.waterColor;
		XMFLOAT4 waterColor = XMFLOAT4(red / 255.0f, green / 255.0f, blue / 255.0f, oldColor.w);
		weather->oceanParameters.waterColor = waterColor;
	}
}


// GGMAX 3.57: TREE SWAY. The one place visuals.tree_wind (0..1) becomes engine wind.
//
// DX11 pushed this into WeatherComponent::tree_wind and its tree shaders read it back as
// g_xFrame_TreeWind. That field does not exist in this engine, AND the shaders that consumed
// it - GGTrees' high-detail instanced pass - are not on the DX12 near-tree path at all: near
// trees are engine ObjectComponents from the tree pool now, drawn by the stock object shader.
// So the sway is driven through the ENGINE's own per-vertex wind instead, which reaches
// ObjectComponents and gives prepass/colour/shadow/env-probe agreement for free.
//
// Two scalars, both object-path only (ShaderWind::gg_object_*, read only by
// sample_wind_object). They exist because stock Wicked wind is authored for a 1-unit-per-
// metre world and GameGuru is 39.37 units per metre: raw, the field gives ~2.5 cm of travel
// with a ~5 cm spatial period, which reads as per-vertex shimmer rather than sway.
//
// WARNING: do NOT get amplitude by inflating windDirection. That vector is shared with grass,
// rain and spring bones; scaling it ~40x to move trees would flatten the grass.
// sample_wind_object normalizes the direction and takes its magnitude from the amplitude
// scalar precisely so the two are independent.
//
// ★★★ SPACE (GGMAX 3.61, Lee): this is about TEXEL SIZE, not wavelength, and the first value
// was 40x too small. The wind volume is 32x32x32 addressed over texcoord 0..1, so one texel
// spans SPACE/32 world units. At the old SPACE=1000 that is 31 world units per texel - and a
// tree canopy is ~600 units across, so the canopy sampled NINETEEN independent noise texels.
//
// That matters because of what the noise actually is. windCS.hlsl builds the volume from the
// VOXEL INDEX, not world space:  waveoffset = 6 octaves of noise(voxel/2^k) * randomness,
// and the result is sin(time*speed + waveoffset). So `randomness` is a PER-VOXEL PHASE offset:
// at 0 every voxel shares one phase and the whole volume pulses together (Lee: "sways slowly
// back and forth in a predictable loop"), and at 2 adjacent voxels are at very different
// phases. Sampling nineteen of those across one canopy is not a gust - it is per-branch
// shimmer, which is exactly what Lee reported: "wibble and wobble as though going through a
// dense refraction field".
//
// At SPACE=40000 a texel is 1250 world units (~32 m): a 600-unit canopy spans under HALF a
// texel, so the whole crown shares one gust phase and bends as one, while trees a few thousand
// units apart still sit in different cells and catch the gust at different times. The BEND
// comes from the per-vertex height ramp, not from spatial variation in the field, so making
// the field coarse costs nothing in shape - DX11 had no spatial field at all, just one phase
// per tree, and this is closer to that.
//
// ⚠ Tunable live with the SET_TREESWAYSPACE harness verb - no rebuild needed to A/B it.
// AMPLITUDE: calibrated against DX11, which displaces a 600-unit tree's canopy by 68 world
// units at tree_wind 1.0. Weight is 1.0 at that canopy, so amplitude == the peak travel.
float g_ggTreeSwaySpace = 40000.0f;   // GGMAX 3.61: was 1000 - see the note above
#define GG_TREE_SWAY_AMPLITUDE    70.0f
// GGMAX 3.62 (Lee): make the "Wind Gust Size" slider mean what it says.
//
// The engine value is windWaveSize, and windCS.hlsl does `position *= wavesize` BEFORE the
// noise - so a larger wavesize makes the field vary faster, i.e. SMALLER gusts. The slider was
// therefore backwards relative to its own name (inherited from the legacy "Wind Wave Size",
// renamed in 3.58). Size is the reciprocal of frequency, so invert rather than subtract.
//
// ⚠ windWaveSize is shared with grass and rain, so this flips all three together. That is
// intended: the label promises gust size for the whole weather system, not just trees.
//
// GGMAX 3.63 (Lee): +0.5 FLOOR ON THE GUST SIZE. With a bare reciprocal, slider 0 meant
// wavesize 10 - the noise varying ten times faster than the engine default - and Lee reported
// it as "very jerky". The bottom of the slider was effectively a strobe rather than a wind.
// Adding 0.5 to the size before inverting keeps the full 0..5 range and its direction, but
// bounds the worst case at wavesize 2.0 instead of 10: "still a little violent but back to a
// normal natural looking effect".
//   slider 0 -> 2.00   (was 10.00)
//   slider 1 -> 0.67   (was  1.00)
//   slider 5 -> 0.18   (was  0.20)
// ⚠ This gives up the property that 1.0 was a no-op. Deliberate - Lee is tuning by eye and
// the bottom of the range being unusable mattered more than bit-identity at the default.
#define GG_WIND_GUST_FLOOR 0.5f
float GGWind_GustSizeToWaveSize(float gustSize)
{
	if (gustSize < 0.0f) gustSize = 0.0f;
	return 1.0f / (gustSize + GG_WIND_GUST_FLOOR);
}

void GGTrees_SetSwayFromVisuals( float treeWind, wi::scene::WeatherComponent* weather )
{
	if ( weather == nullptr ) return;
	if ( treeWind < 0.0f ) treeWind = 0.0f;
	if ( treeWind > 100.0f ) treeWind = 100.0f;   // >1 is diagnostic only; the UI slider caps at 1
	weather->gg_objectWindSpaceRcp  = 1.0f / g_ggTreeSwaySpace;
	weather->gg_objectWindAmplitude = treeWind * GG_TREE_SWAY_AMPLITUDE;
}

static const char* g_pWickedCallProbeNames[WICKEDCALL_PROBE_COUNT] = { "pick", "pick layers", "pick wicked", "pick lookup", "decal create", "decal fade" };
static double g_dWickedCallProbeFrame[WICKEDCALL_PROBE_COUNT] = {};
static int g_iWickedCallProbeFrameCalls[WICKEDCALL_PROBE_COUNT] = {};
static float g_fWickedCallProbeMilliseconds[WICKEDCALL_PROBE_COUNT][20] = {};
static float g_fWickedCallProbeCalls[WICKEDCALL_PROBE_COUNT][20] = {};
static int g_iWickedCallProbeSlot = 0;

// milliseconds on the performance counter
double WickedCall_ProbeNow(void)
{
	static LARGE_INTEGER freq = {};
	if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)now.QuadPart * 1000.0 / (double)freq.QuadPart;
}

void WickedCall_ProbeAdd(int iProbe, double dMilliseconds)
{
	if (iProbe < 0 || iProbe >= WICKEDCALL_PROBE_COUNT) return;
	g_dWickedCallProbeFrame[iProbe] += dMilliseconds;
	g_iWickedCallProbeFrameCalls[iProbe]++;
}

// once a frame: the frame's totals join the last 20 frames'
void WickedCall_ProbeFrame(void)
{
	for (int i = 0; i < WICKEDCALL_PROBE_COUNT; i++)
	{
		g_fWickedCallProbeMilliseconds[i][g_iWickedCallProbeSlot] = (float)g_dWickedCallProbeFrame[i];
		g_fWickedCallProbeCalls[i][g_iWickedCallProbeSlot] = (float)g_iWickedCallProbeFrameCalls[i];
		g_dWickedCallProbeFrame[i] = 0;
		g_iWickedCallProbeFrameCalls[i] = 0;
	}
	g_iWickedCallProbeSlot = (g_iWickedCallProbeSlot + 1) % 20;
}

// a probe's milliseconds and calls a frame, averaged over the last 20 frames; false for an unknown name
bool WickedCall_ProbeGet(const char* pName, float* pMilliseconds, float* pCalls)
{
	for (int i = 0; i < WICKEDCALL_PROBE_COUNT; i++)
	{
		if (_stricmp(pName, g_pWickedCallProbeNames[i]) != 0) continue;
		float fMilliseconds = 0, fCalls = 0;
		for (int s = 0; s < 20; s++)
		{
			fMilliseconds += g_fWickedCallProbeMilliseconds[i][s];
			fCalls += g_fWickedCallProbeCalls[i][s];
		}
		*pMilliseconds = fMilliseconds / 20.0f;
		*pCalls = fCalls / 20.0f;
		return true;
	}
	return false;
}

void WickedCall_UpdateTreeWind(float wind)
{
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		// GGMAX 3.57: re-homed. This is the sink for Lua SetTreeWind(), which the shipped
		// scriptbank/effects/weather_event.lua drives every frame during a wind event - with the
		// body commented out, every one of those scripts was a silent no-op.
		GGTrees_SetSwayFromVisuals( wind, weather );
	}
}

void WickedCall_UpdateWaterHeight(float height)
{
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		weather->oceanParameters.waterHeight = height;
	}
}

void WickedCall_RemoveObjectTextures(sObject* pObject)
{
	for (int i = 0; i < pObject->iMeshCount; i++)
	{
		sMesh* pMesh = pObject->ppMeshList[i];
		if (pMesh)
		{
			// Remove mesh texures
			for (int slot = 0; slot < pMesh->dwTextureCount; slot++)
			{
				pMesh->pTextures[slot].pName[0] = 0;
			}

			// Remove Wicked material textures
			wiScene::MeshComponent* mesh = wiScene::GetScene().meshes.GetComponent(pMesh->wickedmeshindex);
			if (mesh)
			{
				// get material from mesh
				uint64_t materialEntity = mesh->subsets[0].materialID;
				wiScene::MaterialComponent* pObjectMaterial = wiScene::GetScene().materials.GetComponent(materialEntity);
				if (pObjectMaterial)
				{
					for (int slot = 0; slot < wiScene::MaterialComponent::TEXTURESLOT_COUNT; slot++)
					{
						pObjectMaterial->textures[slot].name = "";
						pObjectMaterial->textures[slot].resource = {};
						pObjectMaterial->SetDirty();
					}
				}
			}
		}
	}
}

void WickedCall_SetExposure(float exposure)
{
	master.masterrenderer.setExposure(exposure);
}

#ifdef WICKEDPARTICLESYSTEM
uint32_t WickedCall_LoadLegacyWPE(const char* filename);   // GGMAX 2.00, defined below
uint32_t WickedCall_LoadWiSceneDirect(Scene& scene2,char* filename, bool attached, char* changename, char* changenameto)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	Entity root = 0;

	XMMATRIX& transformMatrix = XMMatrixIdentity();

	// Validate file is a genuine Wicked Engine archive before opening
	// (old .pe particle files from the DX11 engine have incompatible binary format)
	{
		FILE* fcheck = fopen(filename, "rb");
		if (fcheck)
		{
			uint64_t fileVersion = 0;
			size_t bytesRead = fread(&fileVersion, 1, sizeof(fileVersion), fcheck);
			fclose(fcheck);
			if (bytesRead < sizeof(fileVersion) || fileVersion < 22 || fileVersion > 200)
			{
				char msg[1024];
				sprintf(msg, "Skipped non-Wicked archive: %s (version=%llu, expected 22-93)", filename, (unsigned long long)fileVersion);
				void timestampactivity(int i, char* desc_s);
				timestampactivity(0, msg);
				return 0;
			}
		}
	}
	wiArchive archive(filename, true);
	if (archive.IsOpen())
	{
		if (archive.IsReadMode())
		{
			uint32_t reserved;
			archive >> reserved;
		}
		else
		{
			uint32_t reserved = 0;
			archive << reserved;
		}

		//PE: Keeping this alive to keep serialized resources alive until entity serialization ends:
		//wiResourceManager::Serialize removed from API
		//wiResourceManager::ResourceSerializer resource_seri;
		//if (archive.GetVersion() >= 63)
		//{
		//	wiResourceManager::Serialize(archive, resource_seri);
		//}

		EntitySerializer seri;

		scene2.names.Serialize(archive, seri);
		scene2.layers.Serialize(archive, seri);
		scene2.transforms.Serialize(archive, seri);
		//scene2.prev_transforms.Serialize(archive, seri); // removed, replaced by matrix_objects_prev
		scene2.hierarchy.Serialize(archive, seri);
		scene2.materials.Serialize(archive, seri);
		scene2.meshes.Serialize(archive, seri);
		scene2.impostors.Serialize(archive, seri);
		scene2.objects.Serialize(archive, seri);
		//scene2.aabb_objects.Serialize(archive, seri); // aabb vectors no longer have Serialize
		scene2.rigidbodies.Serialize(archive, seri);
		scene2.softbodies.Serialize(archive, seri);
		scene2.armatures.Serialize(archive, seri);
		scene2.lights.Serialize(archive, seri);
		//scene2.aabb_lights.Serialize(archive, seri); // aabb vectors no longer have Serialize
		scene2.cameras.Serialize(archive, seri);
		scene2.probes.Serialize(archive, seri);
		//scene2.aabb_probes.Serialize(archive, seri); // aabb vectors no longer have Serialize
		scene2.forces.Serialize(archive, seri);
		scene2.decals.Serialize(archive, seri);
		//scene2.aabb_decals.Serialize(archive, seri); // aabb vectors no longer have Serialize
		scene2.animations.Serialize(archive, seri);
		scene2.emitters.Serialize(archive, seri);
		scene2.hairs.Serialize(archive, seri);
		scene2.weathers.Serialize(archive, seri);
		if (archive.GetVersion() >= 30)
		{
			scene2.sounds.Serialize(archive, seri);
		}
		if (archive.GetVersion() >= 37)
		{
			scene2.inverse_kinematics.Serialize(archive, seri);
		}
		if (archive.GetVersion() >= 38)
		{
			scene2.springs.Serialize(archive, seri);
		}
		if (archive.GetVersion() >= 46)
		{
			scene2.animation_datas.Serialize(archive, seri);
		}

		//PE: create new root:
		root = CreateEntity();
		scene2.transforms.Create(root);
		scene2.layers.Create(root).layerMask = ~0;

		//PE: Parent all unparented transforms to new root entity
		for (size_t i = 0; i < scene2.transforms.GetCount() - 1; ++i) // GetCount() - 1 because the last added was the "root"
		{
			Entity entity = scene2.transforms.GetEntity(i);
			if (!scene2.hierarchy.Contains(entity))
			{
				scene2.Component_Attach(entity, root);
			}
		}
		//PE: The root component is transformed, scene is updated:
		scene2.transforms.GetComponent(root)->MatrixTransform(transformMatrix);
		scene2.Update(0);

		if (!attached)
		{
			//PE: In this case, we don't care about the root anymore, so delete it. This will simplify overall hierarchy
			scene2.Component_DetachChildren(root);
			scene2.Entity_Remove(root);
			root = INVALID_ENTITY;
		}

		//PE: Support _e_ here for all materials.
		for (int i = 0; i < scene2.materials.GetCount(); i++)
		{
			for (int a = 0; a < MaterialComponent::EMISSIVEMAP; a++)
			{
				if (scene2.materials[i].textures[a].name.size() > 0)
				{
					if (!scene2.materials[i].textures[a].resource.IsValid())
					{
						scene2.materials[i].textures[a].resource = WickedCall_LoadImage(scene2.materials[i].textures[a].name);
					}
				}
			}
		}
	}

	//PE: Fix for GG moved enums
	if (pestrcasestr(filename, ".wiscene"))
	{
		for (int i = 0; i < scene2.materials.GetCount(); i++)
		{
			if (scene2.materials[i].userBlendMode == BLENDMODE_PREMULTIPLIED)
				scene2.materials[i].userBlendMode = BLENDMODE_MULTIPLY;
			if (scene2.materials[i].userBlendMode == BLENDMODE_ALPHA)
				scene2.materials[i].userBlendMode = BLENDMODE_ADDITIVE;
			if (scene2.materials[i].userBlendMode == BLENDMODE_OPAQUE) // was BLENDMODE_FORCEDEPTH, removed
				scene2.materials[i].userBlendMode = BLENDMODE_PREMULTIPLIED;
			if (scene2.materials[i].userBlendMode == BLENDMODE_ALPHA) // was BLENDMODE_ALPHANOZ, removed
				scene2.materials[i].userBlendMode = BLENDMODE_ALPHA;
			scene2.materials[i].SetDirty();
		}
	}

	if (changename && changenameto)
	{
		for (int i = 0; i < scene2.names.GetCount(); i++)
		{
			if (stricmp(scene2.names[i].name.c_str(), changename) == 0)
			{
				scene2.names[i].name = changenameto;
			}
		}
	}
	
	return root;
}
uint32_t WickedCall_LoadWiScene(char* filename, bool attached, char* changename, char* changenameto)
{
#ifdef OPTICK_ENABLE
	OPTICK_EVENT();
#endif

	// GGMAX 2.00: legacy .PE hook lives HERE because this is the real choke point.
	// WickedCall_LoadWPE has only three callers (editor preview, weapon trail); the two
	// paths that actually matter - preload_wicked_particle_effect for decals
	// (M-Entity_part5.cpp) and WParticleEffectLoad for LUA (DarkLUA_part5.cpp) - both call
	// WickedCall_LoadWiScene directly. Hooking it here covers all of them, and because the
	// legacy reader adds its emitters to the live scene, the callers' existing
	// count_before/count_after root detection keeps working unchanged.
	{
		uint32_t legacyRoot = WickedCall_LoadLegacyWPE(filename);
		if (legacyRoot != 0)
		{
			return legacyRoot;
		}
	}

	Scene scene2;
	Entity root = WickedCall_LoadWiSceneDirect(scene2, filename, attached, changename, changenameto);
	GetScene().Merge(scene2);
	return root;
}

// ============================================================================
// GGMAX 2.00: legacy .PE (WPE) reader.
//
// A .PE file is a Wicked scene archive written by the DX11 GameGuru fork, whose
// __archiveVersion is 5077. Every shipped .PE declares 5076 or 5077. The modern
// engine's archive ceiling is 93 and it hard-rejects anything higher, so these
// files could not be opened at all - the whole WPE particle system had been dead
// since the DX12 port. Rather than move the engine's shared archive version
// ceiling (which would silently change the meaning of every
// archive.GetVersion() >= N test across ~40 component serializers), this reads
// the frozen legacy layout directly and builds the scene through the normal API.
//
// The format is documented in GameGuru Core/PARTICLE_SYSTEM_PLAN.md section 4 and
// cross-checked against tools/particle_forensics/pe_decode.py, which parses all
// 27 shipped files. Traps worth remembering:
//   - uint32_t/int are promoted to 8 bytes on disk; bool is 4; strings carry a
//     length that INCLUDES the null terminator.
//   - the BLENDMODE enum was renumbered (the fork had ALPHANOZ and FORCEDEPTH at
//     1 and 2), so blend modes must be mapped, not copied.
//   - the fork used flag bit 7 for EMIT_PAUSE; upstream now uses bit 7 for
//     COLLIDERS_DISABLED, so flags must be remapped too.
// A .PE only ever contains names/layers/transforms/prev_transforms/hierarchy/
// materials/emitters - the other 16 component managers are always count 0.
// ============================================================================
namespace ggwpe
{
	struct Reader
	{
		const uint8_t* d = nullptr;
		size_t n = 0;
		size_t p = 0;
		bool ok = true;

		bool need(size_t bytes) { if (!ok || p + bytes > n) { ok = false; return false; } return true; }
		uint64_t u64() { if (!need(8)) return 0; uint64_t v = 0; memcpy(&v, d + p, 8); p += 8; return v; }
		uint8_t u8() { if (!need(1)) return 0; return d[p++]; }
		float f32() { if (!need(4)) return 0.0f; float v = 0; memcpy(&v, d + p, 4); p += 4; return v; }
		bool b32() { if (!need(4)) return false; uint32_t v = 0; memcpy(&v, d + p, 4); p += 4; return v != 0; }
		XMFLOAT2 f2() { XMFLOAT2 v; v.x = f32(); v.y = f32(); return v; }
		XMFLOAT3 f3() { XMFLOAT3 v; v.x = f32(); v.y = f32(); v.z = f32(); return v; }
		XMFLOAT4 f4() { XMFLOAT4 v; v.x = f32(); v.y = f32(); v.z = f32(); v.w = f32(); return v; }
		std::string str()
		{
			uint64_t len = u64();
			if (len > n || !need((size_t)len)) return std::string();
			const char* s = (const char*)(d + p);
			size_t realLen = 0;
			while (realLen < (size_t)len && s[realLen] != 0) realLen++;
			std::string out(s, realLen);
			p += (size_t)len;
			return out;
		}
	};

	struct MatInfo
	{
		XMFLOAT4 baseColor = XMFLOAT4(1, 1, 1, 1);
		XMFLOAT4 emissiveColor = XMFLOAT4(1, 1, 1, 0);
		uint8_t blendMode = 0;
		std::string basecolormap;
		std::string normalmap;
		std::string emissivemap;
	};

	struct EmitInfo
	{
		uint64_t flags = 0;
		uint64_t shaderType = 0;
		uint64_t maxParticles = 1000;
		float fixedTimestep = -1, size = 1, random_factor = 1, normal_factor = 1;
		float count = 0, life = 1, random_life = 1, scaleX = 1, scaleY = 1;
		float rotation = 0, motionBlur = 0, mass = 1;
		float sph_h = 1, sph_k = 250, sph_p0 = 1, sph_e = 0.018f;
		uint64_t framesX = 1, framesY = 1, frameCount = 1, frameStart = 0;
		float frameRate = 0;
		XMFLOAT3 velocity = {}, gravity = {};
		float drag = 1, random_color = 0;
		float restitution = 0.70f, fadein_time = 0.1f, burst_amount = 0, burst_delay = 0;
		float nfx = 0, nfy = 0, nfz = 0;
		float normal_random = 1, rotation_random = 0, size_random = 0, spawn_random = 0;
		float scaling_random = 1, spawn_pause = 0, spawn_pause_random = 0;
		uint64_t endR = 255, endG = 255, endB = 255;
		float burst_split = 0, bfx = 0, bfy = 0, bfz = 0;
		XMFLOAT3 startpos = {};
		bool findFloor = false;
		float burst_factor_speed = 1, start_rotation = 0;
		bool followCamera = false;
		float random_position = 0, random_position_scale = 1;
		float distance_sort_bias = 0;
		float wpe_filler_1 = 0, wpe_filler_2 = 0, wpe_filler_3 = 0; // the layer's fade-out start, mirroring and option bits (the DX11 fork's names)
	};

	// The fork's BLENDMODE had ALPHANOZ=1 and FORCEDEPTH=2 inserted after OPAQUE.
	static wi::enums::BLENDMODE MapBlendMode(uint8_t legacy)
	{
		switch (legacy)
		{
		case 0: return wi::enums::BLENDMODE_OPAQUE;      // OPAQUE
		case 1: return wi::enums::BLENDMODE_ALPHA;       // ALPHANOZ (fork disabled blending; ALPHA is the closest live mode)
		case 2: return wi::enums::BLENDMODE_OPAQUE;      // FORCEDEPTH
		case 3: return wi::enums::BLENDMODE_ALPHA;       // ALPHA
		case 4: return wi::enums::BLENDMODE_PREMULTIPLIED;
		case 5: return wi::enums::BLENDMODE_ADDITIVE;
		case 6: return wi::enums::BLENDMODE_MULTIPLY;
		default: return wi::enums::BLENDMODE_ALPHA;
		}
	}

	// GG: a particle sheet: the .dds beside the .png (or .jpg) the effect names, when there is one, for its smaller footprint
	// (the game's DDS copies)
	static wi::Resource LoadParticleSheet(std::string& name)
	{
		const size_t dot = name.find_last_of('.');
		if (dot != std::string::npos)
		{
			std::string ext = name.substr(dot);
			for (char& c : ext) c = (char)tolower((unsigned char)c);
			if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
			{
				const std::string dds = name.substr(0, dot) + ".dds";
				if (GetFileAttributesA(dds.c_str()) != INVALID_FILE_ATTRIBUTES) name = dds;
			}
		}
		wi::Resource res = wi::resourcemanager::Load(name);
		return res;
	}

	static void ReadMaterial(Reader& r, uint64_t ver, MatInfo& m)
	{
		r.u64();               // _flags
		r.u8();                // engineStencilRef
		r.u8();                // userStencilRef
		m.blendMode = r.u8();  // userBlendMode
		m.baseColor = r.f4();
		if (ver >= 25) m.emissiveColor = r.f4();
		r.f4();                // texMulAdd
		r.f32(); r.f32(); r.f32();      // roughness, reflectance, metalness
		r.f32();                        // refraction
		r.f32(); r.f32(); r.f32();      // normalMapStrength, parallaxOcclusionMapping, alphaRef
		r.f2();                         // texAnimDirection
		r.f32(); r.f32();               // texAnimFrameRate, texAnimElapsedTime

		m.basecolormap = r.str();       // BASECOLORMAP
		r.str();                        // SURFACEMAP
		m.normalmap = r.str();          // NORMALMAP
		r.str();                        // DISPLACEMENTMAP
		if (ver >= 24) m.emissivemap = r.str(); // EMISSIVEMAP (GG: loaded, the sprite's glow)
		if (ver >= 28)
		{
			r.str();                    // OCCLUSIONMAP
			for (int i = 0; i < 6; i++) r.u64();  // uvsets
			r.f32();                    // displacementMapping
		}
		if (ver >= 48) r.u8();          // shadingRate
		if (ver >= 50) { r.u64(); r.u64(); }   // shaderType, customShaderID
		if (ver >= 52 && ver < 54) r.u64();    // subsurfaceProfile
		if (ver >= 54) r.f4();          // subsurfaceScattering
		if (ver >= 56) r.f4();          // specularColor
		if (ver >= 59) { r.f32(); r.str(); r.u64(); }   // transmission + map + uvset
		if (ver >= 61)
		{
			r.f4(); r.f32();            // sheenColor, sheenRoughness
			r.str(); r.str();           // sheen maps
			r.u64(); r.u64();           // uvsets
			r.f32(); r.f32();           // clearcoat, clearcoatRoughness
			r.str(); r.str(); r.str();  // clearcoat maps
			r.u64(); r.u64(); r.u64();  // uvsets
		}
		if (ver >= 68) { r.str(); r.u64(); }   // SPECULARMAP + uvset
	}

	static void ReadEmitter(Reader& r, uint64_t ver, EmitInfo& e)
	{
		e.flags = r.u64();
		e.shaderType = r.u64();
		r.u64();                 // meshID (remapped; shipped .PE never carry meshes)
		e.maxParticles = r.u64();
		e.fixedTimestep = r.f32(); e.size = r.f32(); e.random_factor = r.f32();
		e.normal_factor = r.f32(); e.count = r.f32(); e.life = r.f32(); e.random_life = r.f32();
		e.scaleX = r.f32(); e.scaleY = r.f32(); e.rotation = r.f32();
		e.motionBlur = r.f32(); e.mass = r.f32();
		e.sph_h = r.f32(); e.sph_k = r.f32(); e.sph_p0 = r.f32(); e.sph_e = r.f32();
		if (ver >= 45)
		{
			e.framesX = r.u64(); e.framesY = r.u64(); e.frameCount = r.u64();
			e.frameStart = r.u64(); e.frameRate = r.f32();
		}
		if (ver == 48) r.u8();
		if (ver >= 64)
		{
			e.velocity = r.f3(); e.gravity = r.f3(); e.drag = r.f32(); e.random_color = r.f32();
		}
		if (ver >= 5072)
		{
			e.restitution = r.f32(); e.fadein_time = r.f32();
			e.burst_amount = r.f32(); e.burst_delay = r.f32();
		}
		if (ver >= 5073) { e.nfx = r.f32(); e.nfy = r.f32(); e.nfz = r.f32(); }
		if (ver >= 5074)
		{
			e.normal_random = r.f32(); e.rotation_random = r.f32(); e.size_random = r.f32();
			e.spawn_random = r.f32(); e.scaling_random = r.f32();
			e.spawn_pause = r.f32(); e.spawn_pause_random = r.f32();
			e.endR = r.u64(); e.endG = r.u64(); e.endB = r.u64();
			e.burst_split = r.f32();
			e.bfx = r.f32(); e.bfy = r.f32(); e.bfz = r.f32();
		}
		if (ver >= 5075)
		{
			e.startpos = r.f3(); e.findFloor = r.b32();
			e.burst_factor_speed = r.f32(); e.start_rotation = r.f32();
			e.followCamera = r.b32();
		}
		if (ver >= 5076) { e.random_position = r.f32(); e.random_position_scale = r.f32(); }
		if (ver >= 5077) { e.distance_sort_bias = r.f32(); e.wpe_filler_1 = r.f32(); e.wpe_filler_2 = r.f32(); e.wpe_filler_3 = r.f32(); }
	}
}

// Returns the root entity of the loaded effect, or 0 on failure.
// GGMAX 3.60: FORWARD COMPATIBILITY for .PE archive versions.
//
// The format is strictly APPEND-ONLY and every GameGuru-era bump has landed in exactly one
// place. Checked across all six: 5072, 5073, 5074, 5075, 5076 and 5077 each only appended
// fields to the END of the emitter record (ReadEmitter). The material reader stops evolving at
// v68 and the resource reader at v63 - nothing in the 507x era touched them. The emitter record
// is fixed-size for a given version (no strings, no variable-length members), the emitter block
// is followed only by emCount entity ids, and that array is the LAST thing in the file.
//
// So the stride of one emitter record is derivable from the file itself:
//     stride = (bytes between the emitter array and the trailing id array) / emCount
// Parse each record with the newest rules we know, then seek to the next stride boundary. Any
// fields a future version appended are skipped instead of desynchronising the read, and known
// versions are bit-identical because stride == what we consumed.
//
// What this does NOT cover, honestly: a future version that changes a section OTHER than the
// emitter tail. That desynchronises earlier and the 16-zero-manager check below rejects the
// file - it fails CLOSED (no effect) rather than loading garbage, and gg_wpe_lastError says so.
#define GG_WPE_VER_NEWEST_KNOWN  5077
#define GG_WPE_VER_SANITY        6000

uint64_t    gg_wpe_lastVer    = 0;
uint64_t    gg_wpe_lastStride = 0;
uint64_t    gg_wpe_lastKnown  = 0;   // bytes ReadEmitter consumed with the rules we have
uint64_t    gg_wpe_emStart = 0, gg_wpe_afterRecs = 0, gg_wpe_afterIds = 0, gg_wpe_fileSize = 0;
const char* gg_wpe_lastError  = "";

uint32_t WickedCall_LoadLegacyWPE(const char* filename)
{
	using namespace ggwpe;

	FILE* f = fopen(filename, "rb");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize < 32) { fclose(f); return 0; }
	std::vector<uint8_t> data((size_t)fsize);
	size_t got = fread(data.data(), 1, (size_t)fsize, f);
	fclose(f);
	if (got != (size_t)fsize) return 0;

	Reader r; r.d = data.data(); r.n = data.size(); r.p = 0;

	const uint64_t ver = r.u64();
	// GGMAX 3.60: the upper bound used to be 5077 - the newest version that existed - so the day
	// the particle editor bumped its save version, every .PE saved by it stopped loading and rain
	// and snow silently vanished. It is now a SANITY bound rather than a format ceiling; unknown
	// newer versions are read with the newest rules we know plus the tail-skip below.
	if (ver < 5000 || ver > GG_WPE_VER_SANITY) { gg_wpe_lastError = "version out of range"; return 0; }
	gg_wpe_lastVer = ver;
	r.u64();                                  // reserved

	// Source directory, used to key embedded resources exactly as the DX11 loader did.
	std::string dir = filename;
	{
		size_t slash = dir.find_last_of("\\/");
		dir = (slash == std::string::npos) ? std::string() : dir.substr(0, slash + 1);
	}

	// Embedded resources (textures baked into the .PE).
	if (ver >= 63)
	{
		uint64_t rescount = r.u64();
		if (rescount > 4096) return 0;
		for (uint64_t i = 0; i < rescount && r.ok; i++)
		{
			std::string name = r.str();
			r.u64();                        // flags
			uint64_t len = r.u64();
			if (!r.need((size_t)len)) return 0;
			const uint8_t* blob = r.d + r.p;
			r.p += (size_t)len;
			wi::resourcemanager::Load(dir + name, wi::resourcemanager::Flags::NONE, blob, (size_t)len);
		}
	}
	if (!r.ok) return 0;

	// --- component managers, in the DX11 scene order ---
	struct Ent { uint64_t id; };

	uint64_t cnt = r.u64();                                  // names
	std::vector<std::string> names(cnt <= 4096 ? (size_t)cnt : 0);
	if (cnt > 4096) return 0;
	for (uint64_t i = 0; i < cnt; i++) names[(size_t)i] = r.str();
	std::vector<uint64_t> nameEnt((size_t)cnt);
	for (uint64_t i = 0; i < cnt; i++) nameEnt[(size_t)i] = r.u64();

	uint64_t layerCount = r.u64();                           // layers
	if (layerCount > 4096) return 0;
	std::vector<uint64_t> layerMask((size_t)layerCount);
	for (uint64_t i = 0; i < layerCount; i++) layerMask[(size_t)i] = r.u64();
	std::vector<uint64_t> layerEnt((size_t)layerCount);
	for (uint64_t i = 0; i < layerCount; i++) layerEnt[(size_t)i] = r.u64();

	uint64_t trCount = r.u64();                              // transforms
	if (trCount > 4096) return 0;
	struct TrInfo { XMFLOAT3 scale; XMFLOAT4 rot; XMFLOAT3 tra; };
	std::vector<TrInfo> trs((size_t)trCount);
	for (uint64_t i = 0; i < trCount; i++)
	{
		r.u64();                                             // _flags
		trs[(size_t)i].scale = r.f3();
		trs[(size_t)i].rot = r.f4();
		trs[(size_t)i].tra = r.f3();
	}
	std::vector<uint64_t> trEnt((size_t)trCount);
	for (uint64_t i = 0; i < trCount; i++) trEnt[(size_t)i] = r.u64();

	uint64_t prevCount = r.u64();                            // prev_transforms (no payload)
	if (prevCount > 4096) return 0;
	for (uint64_t i = 0; i < prevCount; i++) r.u64();

	uint64_t hiCount = r.u64();                              // hierarchy
	if (hiCount > 4096) return 0;
	std::vector<uint64_t> hiParent((size_t)hiCount);
	for (uint64_t i = 0; i < hiCount; i++) { hiParent[(size_t)i] = r.u64(); r.u64(); }
	std::vector<uint64_t> hiEnt((size_t)hiCount);
	for (uint64_t i = 0; i < hiCount; i++) hiEnt[(size_t)i] = r.u64();

	uint64_t matCount = r.u64();                             // materials
	if (matCount > 4096) return 0;
	std::vector<MatInfo> mats((size_t)matCount);
	for (uint64_t i = 0; i < matCount && r.ok; i++) ReadMaterial(r, ver, mats[(size_t)i]);
	std::vector<uint64_t> matEnt((size_t)matCount);
	for (uint64_t i = 0; i < matCount; i++) matEnt[(size_t)i] = r.u64();
	if (!r.ok) return 0;

	// 16 managers that are always empty in a .PE. If any is non-zero the file is
	// something we do not understand - bail rather than desynchronise.
	for (int i = 0; i < 16; i++)
	{
		// GGMAX 3.60: this is the tripwire for a format change OUTSIDE the emitter tail. If a
		// future version alters an earlier section, the read desynchronises and these sixteen
		// always-empty manager counts stop being zero - so we refuse the file rather than build
		// a scene out of misaligned bytes.
		if (r.u64() != 0) { gg_wpe_lastError = "desync before emitters (format changed outside the emitter record)"; return 0; }
	}
	if (!r.ok) return 0;

	uint64_t emCount = r.u64();                              // emitters
	if (emCount == 0 || emCount > 64) { gg_wpe_lastError = "bad emitter count"; return 0; }
	std::vector<EmitInfo> ems((size_t)emCount);

	// GGMAX 3.60: work out the on-disk size of ONE emitter record, so a newer archive version
	// that appended fields still reads correctly instead of desynchronising.
	//
	// First measure what OUR rules consume, by parsing one record and rewinding. Then, only if
	// the file is newer than anything we know, solve for the real size.
	const size_t emStart = r.p;
	size_t emRecKnown = 0;
	{
		EmitInfo probe;
		ReadEmitter(r, ver, probe);
		if (!r.ok) { gg_wpe_lastError = "truncated emitter record"; return 0; }
		emRecKnown = r.p - emStart;
		r.p = emStart;
	}
	size_t emRecSize = emRecKnown;
	gg_wpe_lastKnown = (uint64_t)emRecKnown;

	if (ver > GG_WPE_VER_NEWEST_KNOWN)
	{
		// SOLVE for the record size rather than assume one. The trailing entity-id array is NOT
		// at EOF - measurement on the shipped v5077 files showed a 48-byte trailer after it - so
		// the size cannot be derived from the file length. What CAN be relied on is that every
		// emitter carries a TransformComponent, so each id in that array must be an entity we
		// have already seen in trEnt. Step the candidate record size up in 4-byte units (every
		// field in the record is 4 or 8 bytes) and take the first size that makes all the ids
		// resolve. A wrong size produces garbage ids and is rejected.
		bool found = false;
		for (size_t R = emRecKnown; R <= emRecKnown + 512 && !found; R += 4)
		{
			const size_t idPos = emStart + (size_t)emCount * R;
			if (idPos + (size_t)emCount * 8 > r.n) break;
			bool allSeen = true;
			for (uint64_t k = 0; k < emCount && allSeen; k++)
			{
				uint64_t id = 0;
				memcpy(&id, r.d + idPos + (size_t)k * 8, 8);
				bool seen = false;
				for (size_t t2 = 0; t2 < trEnt.size() && !seen; t2++) if (trEnt[t2] == id) seen = true;
				if (!seen) allSeen = false;
			}
			if (allSeen) { emRecSize = R; found = true; }
		}
		if (!found)
		{
			gg_wpe_lastError = "newer archive version and the emitter record size could not be located";
			return 0;
		}
	}
	gg_wpe_lastStride = (uint64_t)emRecSize;

	for (uint64_t i = 0; i < emCount && r.ok; i++)
	{
		ReadEmitter(r, ver, ems[(size_t)i]);
		// Skip any tail a newer version appended. For a KNOWN version emRecSize == what we just
		// consumed, so this is exactly a no-op and the path is bit-identical to before.
		const size_t next = emStart + (size_t)(i + 1) * emRecSize;
		if (next >= r.p && next <= r.n) r.p = next;
	}
	gg_wpe_emStart = (uint64_t)emStart;
	gg_wpe_afterRecs = (uint64_t)r.p;
	std::vector<uint64_t> emEnt((size_t)emCount);
	for (uint64_t i = 0; i < emCount; i++) emEnt[(size_t)i] = r.u64();
	gg_wpe_afterIds = (uint64_t)r.p;
	gg_wpe_fileSize = (uint64_t)r.n;
	if (!r.ok) return 0;

	// --- build the scene ---
	Scene& scene = wiScene::GetScene();
	// Small linear map (a .PE has a handful of entities); avoids pulling <map> in here.
	std::vector<std::pair<uint64_t, Entity>> remap;
	auto Resolve = [&](uint64_t oldId) -> Entity
	{
		if (oldId == 0) return wiECS::INVALID_ENTITY;
		for (size_t k = 0; k < remap.size(); k++)
		{
			if (remap[k].first == oldId) return remap[k].second;
		}
		Entity e = CreateEntity();
		remap.push_back(std::make_pair(oldId, e));
		return e;
	};

	for (size_t i = 0; i < (size_t)cnt; i++)          scene.names.Create(Resolve(nameEnt[i])) = names[i];
	for (size_t i = 0; i < (size_t)layerCount; i++)   scene.layers.Create(Resolve(layerEnt[i])).layerMask = (uint32_t)layerMask[i];
	for (size_t i = 0; i < (size_t)trCount; i++)
	{
		TransformComponent& t = scene.transforms.Create(Resolve(trEnt[i]));
		t.scale_local = trs[i].scale;
		t.rotation_local = trs[i].rot;
		t.translation_local = trs[i].tra;
		t.SetDirty();
		t.UpdateTransform();
	}
	for (size_t i = 0; i < (size_t)matCount; i++)
	{
		MaterialComponent& m = scene.materials.Create(Resolve(matEnt[i]));
		m.baseColor = mats[i].baseColor;
		m.emissiveColor = mats[i].emissiveColor;
		m.userBlendMode = MapBlendMode(mats[i].blendMode);
		if (!mats[i].basecolormap.empty())
		{
			m.textures[MaterialComponent::BASECOLORMAP].name = dir + mats[i].basecolormap;
			m.textures[MaterialComponent::BASECOLORMAP].resource = LoadParticleSheet(m.textures[MaterialComponent::BASECOLORMAP].name);
		}
		if (!mats[i].normalmap.empty())
		{
			m.textures[MaterialComponent::NORMALMAP].name = dir + mats[i].normalmap;
			m.textures[MaterialComponent::NORMALMAP].resource =
				wi::resourcemanager::Load(m.textures[MaterialComponent::NORMALMAP].name);
		}
		if (!mats[i].emissivemap.empty())
		{
			m.textures[MaterialComponent::EMISSIVEMAP].name = dir + mats[i].emissivemap;
			m.textures[MaterialComponent::EMISSIVEMAP].resource = LoadParticleSheet(m.textures[MaterialComponent::EMISSIVEMAP].name);
		}
		m.SetDirty();
		m.CreateRenderData();
	}
	for (size_t i = 0; i < (size_t)emCount; i++)
	{
		const EmitInfo& s = ems[i];
		wiEmittedParticle& ec = scene.emitters.Create(Resolve(emEnt[i]));

		// Flags: strip the fork's bit 7 (EMIT_PAUSE) so it is not misread as
		// COLLIDERS_DISABLED, and re-apply it on the modern bit.
		const bool legacyEmitPause = (s.flags & (1u << 7)) != 0;
		uint32_t f = (uint32_t)s.flags & ~(1u << 7);
		if (legacyEmitPause) f |= wiEmittedParticle::FLAG_EMIT_PAUSE;
		ec._flags = f;

		ec.shaderType = (wiEmittedParticle::PARTICLESHADERTYPE)(s.shaderType > 3 ? 3 : s.shaderType);

		// GGMAX 2.44: restore the DX11 distortion contract.
		//
		// The fork's distortion pixel shader sampled ONE texture slot, TEXSLOT_ONDEMAND0, and the
		// fork's renderer bound the material's BASECOLORMAP there (WickedRepo
		// wiEmittedParticle.cpp:702) - so a distortion emitter distorted through its own animated
		// colour map. New Wicked moved that slot to NORMALMAP:
		//     emittedparticlePS_soft.hlsl:9   static const uint SLOT = NORMALMAP;
		// and every shipped GameGuru .PE carries a colour map only (verified: all six explosion
		// decals plus splash_large have a SOFT_DISTORTION emitter with normalmap == "").
		//
		// With NORMALMAP invalid the shader never samples, so `color` keeps its initialiser of 1.
		// opacity becomes 1 across the WHOLE quad (no texture mask -> a hard-edged rectangle) and
		// the distortion vector becomes the constant (1 - 0.5), so every pixel in that rectangle
		// refracts the scene at one fixed screen-space offset. That is the blocky duplicated-scene
		// artifact, not a soft textured haze.
		//
		// Pointing NORMALMAP at the colour map the author actually supplied reproduces the fork
		// exactly. It is safe to write NORMALMAP here: a SOFT_DISTORTION emitter only ever draws in
		// the distortion pass, where the shader's normal-map lighting logic is #ifndef'd out, so
		// this slot has no other consumer.
		//
		// Upstream ships this same migration for its own archives - wiScene_Serializers.cpp:2767,
		// "Fixup old emittedparticle distortion basecolor slot -> normalmap slot", gated on archive
		// version < 89. A legacy .PE never reaches it: the reader above parses raw bytes because
		// version 5076/5077 is past the wiArchive ceiling, so this content is the one path that
		// migration cannot see. Two traps for whoever next diffs this against upstream:
		//
		//   1. COPY, do NOT std::move as upstream does. BASECOLORMAP is still read for this emitter
		//      in the ray-tracing / BVH hit path (surfaceHF.hlsli:588 samples BASECOLORMAP with
		//      is_emittedparticle already accounted for), so moving it would blank that.
		//   2. Do NOT "correct" this to CreateRenderData(true). The forced variant calls
		//      SetOutdated(), which misses the resource cache and re-imports the PNG with
		//      IMPORT_NORMALMAP - BC5, two channels, alpha pinned to 1. opacity is
		//      color.a * inputColor.a (emittedparticlePS_soft.hlsl:54), so alpha 1 puts the
		//      unmasked full-quad rectangle straight back, and corrupts the shared colour texture
		//      for the SOFT emitter sampling the same file.
		if (ec.shaderType == wiEmittedParticle::SOFT_DISTORTION)
		{
			MaterialComponent* dm = scene.materials.GetComponent(Resolve(emEnt[i]));
			if (dm != nullptr &&
				dm->textures[MaterialComponent::NORMALMAP].name.empty() &&
				!dm->textures[MaterialComponent::BASECOLORMAP].name.empty())
			{
				dm->textures[MaterialComponent::NORMALMAP].name =
					dm->textures[MaterialComponent::BASECOLORMAP].name;
				dm->textures[MaterialComponent::NORMALMAP].resource =
					dm->textures[MaterialComponent::BASECOLORMAP].resource;
				dm->SetDirty();
				dm->CreateRenderData();
			}
		}

		// GG: the sheets' colours were made against their stored values, which the DX11 build sampled as they are; a sheet
		// stored plain that this engine reads through an sRGB view is put back to them in the particle shader
		if (const MaterialComponent* sm = scene.materials.GetComponent(Resolve(emEnt[i])))
		{
			const wi::Resource& sheet = sm->textures[MaterialComponent::BASECOLORMAP].resource;
			const wi::Resource& glow = sm->textures[MaterialComponent::EMISSIVEMAP].resource;
			ec.gg_reencode_sheet = sheet.IsValid() && sheet.GetTextureSRGBSubresource() >= 0;
			ec.gg_reencode_emissive = glow.IsValid() && glow.GetTextureSRGBSubresource() >= 0;
		}

		// GGMAX 2.02: clamp maxParticles - it is a raw uint64 off disk and sizes eleven GPU
		// buffers plus a synchronous fence-and-recreate. The largest shipped effect requests
		// 25,000 (downpour/heavy-rain3); 262,144 is ~10x headroom. A corrupt/hostile value
		// (e.g. 0xFFFF... from a truncated write) otherwise turns into a multi-GB allocation.
		{
			uint64_t mp = s.maxParticles;
			if (mp < 1) mp = 1;
			if (mp > 262144)
			{
				void timestampactivity(int i, char* desc_s);
				char dbg[MAX_PATH];
				sprintf(dbg, "WPE: clamped absurd maxParticles %llu -> 262144", (unsigned long long)mp);
				timestampactivity(0, dbg);
				mp = 262144;
			}
			ec.SetMaxParticleCount((uint32_t)mp);
		}
		ec.FIXED_TIMESTEP = s.fixedTimestep;
		ec.size = s.size;
		ec.random_factor = 0.0f;   // the fork ignored this; its independent randomisers below replace it
		ec.normal_factor = s.normal_factor;
		ec.count = s.count;
		ec.life = s.life;
		ec.random_life = s.random_life;
		ec.scaleX = s.scaleX;
		ec.scaleY = s.scaleY;

		// GGMAX 2.45: rotation UNITS. The fork and the new engine disagree about what the .PE's
		// two rotation fields mean, so shipped effects spin wrong - in both directions.
		//
		//   fork  wiEmittedParticle.cpp:343   cb.xParticleRotation = rotation * XM_PI * 60
		//         emittedparticleVS.hlsl:27   rotation = lifeLerp * particle.rotationalVelocity
		//         lifeLerp runs 0..1 across the particle's life, so TOTAL spin over a particle's
		//         whole life is rotation * PI * 60 radians - independent of how long it lives.
		//
		//   now   wiEmittedParticle.cpp:574        cb.xParticleRotation = rotation * XM_PI
		//         emittedparticle_simulateCS:267   rotation += rotationVelocity * dt
		//         integrated in radians per SECOND, so total spin is rotation * PI * life.
		//
		// The ratio new/fork is life/60. The 6.88 s smoke plume spins 8.7x too slow, the 0.87 s
		// fireball layer 69x too slow; two thirds of shipped emitters set this. Scaling by
		// 60/life at load restores the fork's total.
		//
		// rotation_random diverges the OTHER way, because the fork never applied its * 60 to it:
		// emittedparticle_emitCS.hlsl:118 adds the raw value into rotationalVelocity, which the VS
		// then multiplies by lifeLerp alone. Fork total = the raw value; new total = value * life,
		// i.e. 1.5-6.9x too FAST. Divide by life.
		//
		// ⚠ Exact only at the nominal life. The fork's lifeLerp normalised by each particle's OWN
		// maxLife, which made its totals life-independent; dt integration is not, so an emitter
		// with random_life > 0 keeps a spin spread the fork did not have. A CPU-side compensation
		// cannot remove that - it would take a shader change. Correct on average.
		const float rotLife = (s.life > 0.01f) ? s.life : 0.01f;
		ec.rotation = s.rotation * (60.0f / rotLife);

		ec.motionBlurAmount = s.motionBlur;
		ec.mass = s.mass;
		ec.SPH_h = s.sph_h; ec.SPH_K = s.sph_k; ec.SPH_p0 = s.sph_p0; ec.SPH_e = s.sph_e;
		ec.framesX = (uint32_t)(s.framesX < 1 ? 1 : s.framesX);
		ec.framesY = (uint32_t)(s.framesY < 1 ? 1 : s.framesY);
		ec.frameCount = (uint32_t)(s.frameCount < 1 ? 1 : s.frameCount);
		ec.frameStart = (uint32_t)s.frameStart;
		ec.frameRate = s.frameRate;
		ec.velocity = s.velocity;
		ec.gravity = s.gravity;
		ec.drag = s.drag;
		ec.random_color = s.random_color;
		ec.restitution = s.restitution;

		ec.fadein_time = s.fadein_time;
		ec.wpe_filler_1 = s.wpe_filler_1; ec.wpe_filler_2 = s.wpe_filler_2; ec.wpe_filler_3 = s.wpe_filler_3;
		ec.endcolor_red = (float)s.endR / 255.0f;
		ec.endcolor_green = (float)s.endG / 255.0f;
		ec.endcolor_blue = (float)s.endB / 255.0f;
		ec.normal_factor_x = s.nfx; ec.normal_factor_y = s.nfy; ec.normal_factor_z = s.nfz;
		ec.burst_factor_x = s.bfx; ec.burst_factor_y = s.bfy; ec.burst_factor_z = s.bfz;
		ec.burst_factor_speed = s.burst_factor_speed;
		ec.normal_random = s.normal_random;
		ec.rotation_random = s.rotation_random / rotLife;   // GGMAX 2.45 - see the rotation note above
		ec.size_random = s.size_random;
		ec.scaling_random = s.scaling_random;
		ec.start_rotation = s.start_rotation;
		ec.random_position = s.random_position;
		ec.random_position_scale = s.random_position_scale;
		ec.startpos = s.startpos;
		ec.burst_amount = s.burst_amount;
		ec.burst_split = s.burst_split;
		ec.burst_delay = s.burst_delay;
		ec.spawn_random = s.spawn_random;
		ec.distance_sort_bias = s.distance_sort_bias;
		ec.bFindFloor = s.findFloor;
		ec.bFollowCamera = s.followCamera;
	}

	// Hierarchy, then find the root (an entity that is a parent but never a child).
	uint32_t root = 0;
	for (size_t i = 0; i < (size_t)hiCount; i++)
	{
		Entity child = Resolve(hiEnt[i]);
		Entity parent = Resolve(hiParent[i]);
		if (parent != wiECS::INVALID_ENTITY && child != wiECS::INVALID_ENTITY)
		{
			// GGMAX 2.02: reject malformed hierarchy rows. A row with child == parent, or
			// two rows forming a mutual cycle, would create a HierarchyComponent whose
			// parentID chain never terminates. Component_Attach's only guard is a debug
			// assert (compiled out of Release), and the engine's stock ancestor walk in
			// RunHierarchyUpdateSystem has no cycle cap - the next frame's Scene::Update
			// would spin forever and hang the render thread. Rejecting the row degrades a
			// corrupt/hand-edited .PE to a mis-parented effect instead of a hard hang.
			if (child == parent)
			{
				void timestampactivity(int i, char* desc_s);
				char dbg[128]; strcpy(dbg, "WPE: rejected self-parenting hierarchy row in .PE");
				timestampactivity(0, dbg);
				continue;
			}
			bool closesCycle = false;
			{
				Entity walk = parent;
				for (size_t guard = 0; guard <= (size_t)hiCount; guard++)
				{
					HierarchyComponent* h = scene.hierarchy.GetComponent(walk);
					if (!h || h->parentID == wiECS::INVALID_ENTITY) break;
					if (h->parentID == child) { closesCycle = true; break; }
					walk = h->parentID;
				}
			}
			if (closesCycle)
			{
				void timestampactivity(int i, char* desc_s);
				char dbg[128]; strcpy(dbg, "WPE: rejected cyclic hierarchy row in .PE");
				timestampactivity(0, dbg);
				continue;
			}
			if (!scene.transforms.Contains(parent)) scene.transforms.Create(parent);
			if (!scene.layers.Contains(parent)) scene.layers.Create(parent).layerMask = ~0u;
			// child_already_in_local_space = TRUE: the transforms we just created came
			// straight out of the archive and are ALREADY parent-local. The default (false)
			// would multiply them by inverse(parent.world) a second time.
			scene.Component_Attach(child, parent, true);
			bool parentIsChild = false;
			for (size_t j = 0; j < (size_t)hiCount; j++)
			{
				if (hiEnt[j] == hiParent[i]) { parentIsChild = true; break; }
			}
			if (!parentIsChild) root = (uint32_t)parent;
		}
	}
	if (root == 0 && emCount > 0)
	{
		// No hierarchy in the file: synthesise a root so the game gets one handle.
		Entity newRoot = CreateEntity();
		scene.transforms.Create(newRoot);
		scene.layers.Create(newRoot).layerMask = ~0u;
		for (size_t i = 0; i < (size_t)emCount; i++) scene.Component_Attach(Resolve(emEnt[i]), newRoot);
		root = (uint32_t)newRoot;
	}
	return root;
}

uint32_t WickedCall_LoadWPE(char* filename)
{
	Scene& scene = wiScene::GetScene();
	uint32_t root = 0;
	uint32_t count_before = scene.emitters.GetCount();

	char path[MAX_PATH];
	strcpy(path, filename);
	GG_GetRealPath(path, 0);

	// GGMAX 2.00: legacy GameGuru .PE files (archive version 5072-5077) cannot be
	// opened by the modern wiArchive, so read them with the dedicated reader first.
	{
		uint32_t legacyRoot = WickedCall_LoadLegacyWPE(path);
		if (legacyRoot != 0)
		{
			// Match the DX11 loader: Restart() + hide the LAST emitter of the effect.
			// Callers switch it on with emitter action 5 when they want it shown.
			if (scene.emitters.GetCount() > 0)
			{
				Entity em = scene.emitters.GetEntity(scene.emitters.GetCount() - 1);
				wiEmittedParticle* ec = scene.emitters.GetComponent(em);
				if (ec)
				{
					ec->Restart();
					ec->SetVisible(false);
				}
			}
			return legacyRoot;
		}
	}

	WickedCall_LoadWiScene(path, false, NULL, NULL);
	uint32_t count_after = scene.emitters.GetCount();
	if (count_before != count_after)
	{
		Entity emitter = scene.emitters.GetEntity(scene.emitters.GetCount() - 1);
		if (scene.emitters.GetCount() > 0)
		{
			HierarchyComponent* hier = scene.hierarchy.GetComponent(emitter);
			if (hier)
			{
				root = hier->parentID;
			}
		}
		wiEmittedParticle* ec = scene.emitters.GetComponent(emitter);
		if (ec)
		{
			ec->Restart();
			//ec->SetVisible(false); // SetVisible removed from EmittedParticleSystem
		}
	}
	return root;
}
#endif // WICKEDPARTICLESYSTEM (continued in wickedcalls_part4.cpp)

#include <algorithm>
#include <vector>

// GGMAX 2026-07-30: per-character DEDICATED sun shadows.
// The new Wicked renderer already supports high-res per-character shadow slots: every sphere
// pushed into scene.character_dedicated_shadows becomes one extra FULL-RES cascade slice
// fitted tightly around that character (prepended before the 5 sun cascades; sampling and the
// 1.58 feathered PCF cover it automatically). Upstream drives the list from CharacterComponents,
// which GG does not use (their controller would fight our entity system), so we fill the vector
// directly from live GG character elements each frame.
// MUST run AFTER Scene::Update (which clears the vector) and BEFORE UpdateVisibility — the
// call site is Master::Update right after __super::Update(dt).
// CAP: 5x2048 sun cascades + 3x2048 dedicated slots exactly fill the 16384 atlas cap (1.58b);
// a 4th slot would make the packer silently shrink ALL slices. Nearest-to-camera wins.
// NOTE (engine interaction): while ANY slot is live the engine disables the delayed-cascade
// stagger and far-cascade cull for that frame (dedicated slots shift cascade indices).
int g_iCharShadowMax = 3; // 0 = off (stock); harness SET_CHARSHADOW <0-3>
void WickedCall_UpdateCharacterShadows(void)
{
	wiScene::Scene& scene = wiScene::GetScene();
	scene.character_dedicated_shadows.clear();
	if (g_iCharShadowMax <= 0) return;
	if (g.entityelementlist <= 0) return;
	if (t.entityelement.size() <= (size_t)g.entityelementlist) return;

	const wi::scene::CameraComponent& cam = wiScene::GetCamera();
	const bool bInGame = (t.game.set.ismapeditormode == 0);

	struct GGCharShadowCand { float dist2; XMFLOAT3 center; float radius; };
	static std::vector<GGCharShadowCand> s_cands;
	s_cands.clear();

	for (int e = 1; e <= g.entityelementlist; e++)
	{
		const auto& ee = t.entityelement[e];
		if (ee.bankindex <= 0 || ee.bankindex >= (int)t.entityprofile.size()) continue;
		if (t.entityprofile[ee.bankindex].ischaracter != 1) continue;
		if (ee.eleprof.disableascharacter != 0) continue;
		if (ee.obj <= 0 || ObjectExist(ee.obj) == 0) continue;
		if (ee.ishidden != 0) continue;
		if (ee.ragdollified != 0) continue;
		if (bInGame)
		{
			// editor-placed characters are not "active" until the game runs — only
			// apply liveness filters in test game / standalone
			if (ee.active == 0) continue;
			if (ee.health <= 0) continue;
		}

		// capsule approximation: human-scale height x profile scale (percent)
		float fScale = t.entityprofile[ee.bankindex].scale > 0 ? (float)t.entityprofile[ee.bankindex].scale / 100.0f : 1.0f;
		float fHeight = 72.0f * fScale;
		if (fHeight < 30.0f) fHeight = 30.0f;
		if (fHeight > 200.0f) fHeight = 200.0f;
		const float fRadius = fHeight * 0.5f;
		const XMFLOAT3 vCenter = XMFLOAT3(ee.x, ee.y + fRadius, ee.z);

		// engine consumes the list as-is, so cull to the live camera here (upstream does the same)
		if (!cam.frustum.CheckSphere(vCenter, fRadius)) continue;

		const float dx = vCenter.x - cam.Eye.x, dy = vCenter.y - cam.Eye.y, dz = vCenter.z - cam.Eye.z;
		s_cands.push_back({ dx * dx + dy * dy + dz * dz, vCenter, fRadius });
	}
	if (s_cands.empty()) return;

	const int iMax = g_iCharShadowMax > 3 ? 3 : g_iCharShadowMax;
	std::sort(s_cands.begin(), s_cands.end(), [](const GGCharShadowCand& a, const GGCharShadowCand& b) { return a.dist2 < b.dist2; });
	for (int i = 0; i < (int)s_cands.size() && i < iMax; i++)
	{
		scene.character_dedicated_shadows.push_back(wi::primitive::Sphere(s_cands[i].center, s_cands[i].radius));
	}
}

// GGMAX 2.28: setter for the directional shadow-CASTER culling extrusion, so setup.ini
// `shadowextrude=<units>` can reach the engine global from Common_part3.cpp, which does not
// include wiRenderer.h. Live equivalent: harness SET_SHADOWEXTRUDE.
// 0 restores stock (an effective 1000 units = 25 m — a metres-scale constant in an inch-scale
// world, which is why shadows popped as their casters left the camera frustum).
void GGSetShadowExtrude(int iUnits)
{
	wi::renderer::gg_shadow_caster_extrude = (iUnits < 0) ? 0.0f : (float)iUnits;
}

// GGMAX 3.126: setup.ini `msaoreffar=<units>` -> the engine's MSAO tolerance reference far plane
// (default 10000 = 254 m; 0 = stock). Live equivalent: harness SET_MSAOREFFAR.
void GGSetMSAOReferenceFar(int iUnits)
{
	wi::renderer::gg_msao_reference_far = (iUnits < 0) ? 0.0f : (float)iUnits;
}

// GGMAX 3.126b: setup.ini `msaodownsample=<0|1|2>` -> MSAO depth-pyramid pick at depth edges
// (0 stock, 1 farthest = default, 2 nearest). Live equivalent: harness SET_MSAODOWNSAMPLE.
void GGSetMSAODownsample(int iMode)
{
	if (iMode >= 0 && iMode <= 2) wi::renderer::gg_msao_downsample_mode = (uint32_t)iMode;
}

// GGMAX 2.52: parallel-for shim for the ImGui DX12 bridge's video YUY2->RGBA convert.
// Lives here because the bridge cannot include wicked headers without risking include-order
// conflicts with ImGui/d3d12; wickedcalls already compiles against the full engine.
// Called from the main thread between frames (ConstantNonDisplay) - Dispatch+Wait is the
// jobsystem's normal foreground use.
extern "C" void GGVideo_ParallelFor(int jobCount, void(*fn)(int, void*), void* ctx)
{
	if (jobCount <= 1) { if (jobCount == 1) fn(0, ctx); return; }
	wi::jobsystem::context jc;
	wi::jobsystem::Dispatch(jc, (uint32_t)jobCount, 1, [fn, ctx](wi::jobsystem::JobArgs args) { fn((int)args.jobIndex, ctx); });
	wi::jobsystem::Wait(jc);
}

// GGMAX 2.75b (#155 round 3 — THE REAL ROOT CAUSE): the %probe marker ball was being
// RENDERED INTO ITS OWN PROBE'S CAPTURE. The pool probe captures from the marker's
// centre, i.e. from INSIDE the (double-sided, circular-featured) probe.dbo ball — the
// "circle image on each cube side" Lee kept seeing was the ball's own openings acting as
// portholes to the world, with its interior as the pale wash. DX11 excluded the marker
// via probe userdata (see the commented `probe->userdata` fossils in GGTerrain_part0);
// the DX12 port dropped userdata and with it the exclusion. Wicked's env capture culling
// honours ObjectComponent::NOT_VISIBLE_IN_REFLECTIONS (wiRenderer RefreshEnvProbes), so
// mark every frame-object of the marker. The 2.75 material-matte treatment is REVERTED —
// with self-capture gone the ball's natural glossy look reflects a CLEAN capture again.
// Idempotent; called from the lighting_loop probe-list rebuild (level load + placement).
void WickedCall_MakeObjectEnvMatte(int iObj)
{
	if (iObj <= 0 || !ObjectExist(iObj)) return;
	sObject* pObj = GetObjectData(iObj);
	if (pObj == NULL) return;
	auto& scene = wi::scene::GetScene();
	for (int f = 0; f < pObj->iFrameCount; f++)
	{
		if (pObj->ppFrameList[f] == NULL) continue;
		wi::ecs::Entity objent = (wi::ecs::Entity)pObj->ppFrameList[f]->wickedobjindex;
		wi::scene::ObjectComponent* oc = scene.objects.GetComponent(objent);
		if (oc == nullptr) continue;
		oc->SetNotVisibleInReflections(true);
	}
}

// GGMAX 2.76 (#158): live world-space HALF-EXTENT of an object's largest frame AABB — i.e.
// the visible radius of a ball-shaped mesh, which is what the debug probe mirror sphere must
// match so that picking a probe does not change the marker's apparent size.
// ⚠ This replaces 2.75's WickedCall_GetObjectWorldRadius: AABB::getRadius() returns the
// half-DIAGONAL (sqrt(3) x the half-extent for a sphere's tight box), so sizing the preview
// by it — and then scaling a further 1.15 — drew the sphere at ~2x the marker ball. Any
// "make X the size of Y" job wants the half-extent, never the bounding-sphere radius.
float WickedCall_GetObjectWorldExtent(int iObj)
{
	if (iObj <= 0 || !ObjectExist(iObj)) return 0.0f;
	sObject* pObj = GetObjectData(iObj);
	if (pObj == NULL) return 0.0f;
	auto& scene = wi::scene::GetScene();
	float fBest = 0.0f;
	for (int f = 0; f < pObj->iFrameCount; f++)
	{
		if (pObj->ppFrameList[f] == NULL) continue;
		wi::ecs::Entity objent = (wi::ecs::Entity)pObj->ppFrameList[f]->wickedobjindex;
		size_t idx = scene.objects.GetIndex(objent);
		if (idx >= scene.objects.GetCount() || idx >= scene.aabb_objects.size()) continue;
		const wi::primitive::AABB& ab = scene.aabb_objects[idx];
		XMFLOAT3 hw = ab.getHalfWidth();
		float r = hw.x;
		if (hw.y > r) r = hw.y;
		if (hw.z > r) r = hw.z;
		if (r > fBest) fBest = r;
	}
	return fBest;
}
