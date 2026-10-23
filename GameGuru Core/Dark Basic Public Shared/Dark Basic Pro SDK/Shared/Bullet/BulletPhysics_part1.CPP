void CreateCylinder ( int iObjectNumber, int isDynamic, float fXPos, float fYPos, float fZPos, float fXSize, float fYSize, float fZSize, float fXRot, float fYRot, float fZRot, float fWeight, float fFriction, float fRestitution)
{
	// defaults
	if ( fWeight==-1 ) fWeight = 100.0f;
	if ( fFriction==-1 ) fFriction = 90.0f;
	if ( fRestitution==-1 ) fRestitution = 0.0f;

	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;	

	// volume calculation
	float fVolume = 0;
	fVolume = fXSize * fYSize * fZSize;

	// modify with weight modifier
	fVolume *= ( fWeight / 100.0f );

	// create a bullet shape
	btCollisionShape* cylinderShape = new btCylinderShape(btVector3( btScalar( fXSize / 2.0f ), 
		                                                             btScalar( fYSize / 2.0f ), 
		                                                             btScalar( fZSize / 2.0f ) ) );

	// add shape to collision list so can release later, they can be "re-used" amongst bodies!!
	g_collisionShapes.push_back( cylinderShape );

	// mass/volume calculation
	if ( isDynamic==0 ) 
	{
		fVolume = 0;
	}
	else
	{
		// max
		if (fVolume > maxDynamicVolume) fVolume = maxDynamicVolume;

		// also handle min - ensure MASS not SO SMALL THINGS FLY MILLIONS OF MILES!
		if (fVolume < 0.1f) fVolume = 0.1f;
	}
	btScalar mass(fVolume/gMs);
	btVector3 localInertia(0,0,0);
	if ( isDynamic == 1 ) cylinderShape->calculateLocalInertia(mass,localInertia);

	// set position transform for physics object
	btTransform startTransform;
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );

	// motionstate provides interpolation capabilities and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState( startTransform );
	btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, cylinderShape, localInertia );
	rbInfo.m_friction = 1.0f;
	rbInfo.m_restitution = fRestitution / 100.0f;
	btRigidBody* body = new btRigidBody( rbInfo );

	// pass material index as usrptr
	sObject* pRealObject = pObject;
	if ( pRealObject->pInstanceOfObject ) pRealObject = pRealObject->pInstanceOfObject;
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add physics object to system
	short sColliderType = COL_OBJECT;
	if (isDynamic == 1) sColliderType = COL_OBJECT_DYNAMIC;
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	g_dynamicsWorld->addRigidBody ( body, sColliderType, sCollidesWith);

	// boyancy mode
	bool bBouyant = false;
	if ( isDynamic==1 ) 
		bBouyant = true; 
	else
		pObject = NULL;

	// add physics object details to pbject list
	ODEAddObject ( iObjectNumber, pObject, body, isDynamic, myMotionState, false, bBouyant, NULL, NULL, 1, 1, fFriction, mass );
}

void CreateCylinder ( int iObjectNumber, int isDynamic, float fWeight, float fFriction, float fRestitution )
{
	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	GetSizePositionRotation(pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, true);

	CreateCylinder(iObjectNumber, isDynamic, fXPos, fYPos, fZPos, fXSize, fYSize, fZSize, fXRot, fYRot, fZRot, fWeight, fFriction, fRestitution);
}

void SetMeshFilename (LPSTR pFilename)
{
	strcpy (g_ProvidedMeshFilenameForBNVHSaving, pFilename);
}

void SetOBJLoadingFilename (LPSTR pFilename)
{
	strcpy (g_ProvidedMeshFilenameForOBJLoading, pFilename);
}

// while set (a level's physics start), an OBJ collision mesh stays loaded and hidden in the temp object after use, so the
// meshes that follow from the same file use it again instead of loading it each time (a level's 600 streetlights loaded
// one file 600 times), and the file checks are kept for the entity file asked about last
static bool g_bKeepOBJCollisionMesh = false;
static sObject* g_pKeptOBJCollisionObject = NULL;
static char g_pKeptOBJCollisionMesh[MAX_PATH] = "";
static char g_pCheckedOBJCollisionFPE[MAX_PATH] = "";
static char g_pCheckedOBJCollisionMesh[MAX_PATH] = "";

void KeepOBJCollisionMesh (bool bKeep)
{
	if (g_pKeptOBJCollisionObject && ObjectExist(69501) == 1 && GetObjectData(69501) == g_pKeptOBJCollisionObject) DeleteObject(69501);
	g_pKeptOBJCollisionObject = NULL;
	g_pKeptOBJCollisionMesh[0] = 0;
	g_pCheckedOBJCollisionFPE[0] = 0;
	g_pCheckedOBJCollisionMesh[0] = 0;
	g_bKeepOBJCollisionMesh = bKeep;
}

// GG: a hull decomposition's hulls were made from three points for each of their triangles, so each corner was there about
// five times (a .bullet file keeps them so), and every contact test reads all of a hull's points. Only the corners are
// kept: the same shapes. A hull the hull computer can't make again keeps its points
static void PhysicsOptimiseCompoundHulls(btCollisionShape* pShape)
{
	if (!pShape || pShape->getShapeType() != COMPOUND_SHAPE_PROXYTYPE) return;
	btCompoundShape* pCompound = (btCompoundShape*)pShape;
	for (int c = 0; c < pCompound->getNumChildShapes(); c++)
	{
		btCollisionShape* pChild = pCompound->getChildShape(c);
		if (pChild->getShapeType() != CONVEX_HULL_SHAPE_PROXYTYPE) continue;
		btConvexHullShape* pHull = (btConvexHullShape*)pChild;
		const int iPoints = pHull->getNumPoints();
		if (iPoints < 8) continue;
		btAlignedObjectArray<btVector3> original;
		original.resize(iPoints);
		for (int p = 0; p < iPoints; p++) original[p] = pHull->getUnscaledPoints()[p];
		pHull->optimizeConvexHull();
		if (pHull->getNumPoints() < 4)
		{
			for (int p = 0; p < iPoints; p++) pHull->addPoint(original[p], false);
		}
		pHull->recalcLocalAabb();
	}
}

void CreateMesh ( int iObjectNumber, int isDynamic, int iLimbNumber, int iTerrainMesh, int iCollisionScaling, int iHullReduction, float fWeight, float fFriction, float fRestitution )
{
	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// real object for mesh data
	sObject* pRealObject = pObject;
	if ( pObject->pInstanceOfObject ) pRealObject = pObject->pInstanceOfObject;                 

	// scaling modifier
	float fScalingFactor = (float)iCollisionScaling / 100.0f;

	// obtain sizes, position and rotation
	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	GetSizePositionRotation( pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, false, fScalingFactor );

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// LB: Account for any offset from the model importer
	GGMATRIX matModelOffset;
	GGMatrixIdentity(&matModelOffset);
	GGMATRIX matIdentity;
	GGMatrixIdentity(&matIdentity);
	if (pObject->pFrame)
	{
		// work out the inverse of the model frame zero transform - we need to remove the offset from the raw mesh data further down
		float fDet = 0.0f;
		matModelOffset = pObject->pFrame->matCombined;
		GGMatrixInverse(&matModelOffset, &fDet, &matModelOffset);
	}

	// volume calculation
	float fVolume = 0;
	fVolume = fXSize * fYSize * fZSize;

	// create aabb boundboxes
	btVector3 aabbMin(pObject->collision.vecMin.x / gSc, pObject->collision.vecMin.y / gSc, pObject->collision.vecMin.z / gSc),
			  aabbMax(pObject->collision.vecMax.x / gSc, pObject->collision.vecMax.y / gSc,	pObject->collision.vecMax.z / gSc);

	// creation part
	bool useQuantizedAabbCompression = true;
	btIndexedMesh btMesh;
	btTriangleIndexVertexArray* m_indexVertexArrays;
	btAlignedObjectArray<btVector3> ch_vertexBuffer;
	bool bDWORDSizeIndices = false;
	float* gVertices = NULL;
	LPVOID gIndices = NULL;

	// defaults to passed in object for collision mesh data
	sObject* pObjectUsedToExtractPhysicsMesh = pRealObject;

	// determine if OBJ file carries collision mesh
	bool bObjectUsesOBJCollisionMesh = false;
	int iObjectUsedToExtractPhysicsMesh = 0;
	if (strlen(g_ProvidedMeshFilenameForOBJLoading) > 4)
	{
		char pNoFPE[MAX_PATH];
		strcpy(pNoFPE, g_ProvidedMeshFilenameForOBJLoading);
		pNoFPE[strlen(pNoFPE) - 4] = 0;
		char pOBJCollisionMesh[MAX_PATH];
		if (g_bKeepOBJCollisionMesh && stricmp(pNoFPE, g_pCheckedOBJCollisionFPE) == 0)
		{
			strcpy(pOBJCollisionMesh, g_pCheckedOBJCollisionMesh);
		}
		else
		{
			sprintf(pOBJCollisionMesh, "%s.obj", pNoFPE);
			GG_GetRealPath(pOBJCollisionMesh, 0);
			if (FileExist(pOBJCollisionMesh) == 0)
			{
				sprintf(pOBJCollisionMesh, "%s_COL.obj", pNoFPE);
				GG_GetRealPath(pOBJCollisionMesh, 0);
				if (FileExist(pOBJCollisionMesh) == 0) pOBJCollisionMesh[0] = 0;
			}
			if (g_bKeepOBJCollisionMesh)
			{
				strcpy(g_pCheckedOBJCollisionFPE, pNoFPE);
				strcpy(g_pCheckedOBJCollisionMesh, pOBJCollisionMesh);
			}
		}
		if (pOBJCollisionMesh[0])
		{
			iObjectUsedToExtractPhysicsMesh = 69501;// g.temp2objectoffset;
			bool bKept = g_pKeptOBJCollisionObject && stricmp(pOBJCollisionMesh, g_pKeptOBJCollisionMesh) == 0
				&& ObjectExist(iObjectUsedToExtractPhysicsMesh) == 1 && GetObjectData(iObjectUsedToExtractPhysicsMesh) == g_pKeptOBJCollisionObject;
			if (!bKept)
			{
				if (ObjectExist(iObjectUsedToExtractPhysicsMesh) == 1) DeleteObject(iObjectUsedToExtractPhysicsMesh);
				LoadObject (pOBJCollisionMesh, iObjectUsedToExtractPhysicsMesh);
				g_pKeptOBJCollisionObject = NULL;
				if (g_bKeepOBJCollisionMesh && ObjectExist(iObjectUsedToExtractPhysicsMesh) == 1)
				{
					HideObject(iObjectUsedToExtractPhysicsMesh);
					g_pKeptOBJCollisionObject = GetObjectData(iObjectUsedToExtractPhysicsMesh);
					strcpy(g_pKeptOBJCollisionMesh, pOBJCollisionMesh);
				}
			}
			RotateObject(iObjectUsedToExtractPhysicsMesh, ObjectAngleX(iObjectNumber), ObjectAngleY(iObjectNumber), ObjectAngleZ(iObjectNumber));
			ScaleObject(iObjectUsedToExtractPhysicsMesh, ObjectScaleX(iObjectNumber), ObjectScaleY(iObjectNumber), ObjectScaleZ(iObjectNumber));
			pObjectUsedToExtractPhysicsMesh = GetObjectData(iObjectUsedToExtractPhysicsMesh);
			bObjectUsesOBJCollisionMesh = true;
			if(pObjectUsedToExtractPhysicsMesh)
			{
				//PE: New loaded .obj collision mesh dont match original frame zero transform, so set to MatrixIdentity
				GGMatrixIdentity(&matModelOffset);
			}
		}
	}

	// determine if using LOD, if so, choose lowest
	extern bool bPhysicsLowestLOD;
	bool bGotLowestLOD = false;

	if (bPhysicsLowestLOD && bObjectUsesOBJCollisionMesh == false)
	{
		for (int iFrame = 0; iFrame < pRealObject->iFrameCount; iFrame++)
		{
			sFrame* pFrame = pRealObject->ppFrameList[iFrame];
			if (pFrame)
			{
				sMesh* pMesh = pFrame->pMesh;
				if (pMesh)
				{
					if (pMesh->dwIndexCountLOD3 > 0 && pMesh->pIndicesLOD3)
					{
						bGotLowestLOD = true;
						break;
					}
					else if (pMesh->dwIndexCountLOD2 > 0 && pMesh->pIndicesLOD2)
					{
						bGotLowestLOD = true;
						break;
					}
					else if (pMesh->dwIndexCountLOD1 > 0 && pMesh->pIndicesLOD1)
					{
						bGotLowestLOD = true;
						break;
					}
				}
			}
		}
	}

	int iChooseLowLOD = -1;
	if (bObjectUsesOBJCollisionMesh == false && !bGotLowestLOD)
	{
		// if no OBJ, rely on internal mesh stored in object (prefer frame named 'collision_mesh')
		int iBestLowLOD = 0;
		for (int iFrame = 0; iFrame < pRealObject->iFrameCount; iFrame++)
		{
			sFrame* pFrame = pRealObject->ppFrameList[iFrame];
			if (pFrame)
			{
				if (pFrame->pMesh)
				{
					if (stricmp(pFrame->szName, "lod_0") == NULL && iBestLowLOD < 1) { iChooseLowLOD = iFrame; iBestLowLOD = 1; }
					if (stricmp(pFrame->szName, "lod_1") == NULL && iBestLowLOD < 2) { iChooseLowLOD = iFrame; iBestLowLOD = 2; }
					if (stricmp(pFrame->szName, "lod_2") == NULL && iBestLowLOD < 3) { iChooseLowLOD = iFrame; iBestLowLOD = 3; }
					if (stricmp(pFrame->szName, "collision_mesh") == NULL && iBestLowLOD < 4) { iChooseLowLOD = iFrame; iBestLowLOD = 4; }
				}
			}
		}
	}

	// do I need to create new data, or can I use mesh data directly in mesh builder? (save memory)
	int totalVerts = 0, totalIndices = 0;
	for (int iFrame = 0; iFrame < pObjectUsedToExtractPhysicsMesh->iFrameCount; iFrame++)
	{
		if (iChooseLowLOD == -1 || iChooseLowLOD == iFrame)
		{
			sFrame* pFrame = pObjectUsedToExtractPhysicsMesh->ppFrameList[iFrame];
			if (pFrame)
			{
				sMesh* pMesh = pFrame->pMesh;
				if (pMesh)
				{
					if (pMesh->Collision.dwArbitaryValue != 99999) // mesh excluded for collision purposes
					{
						if (!bObjectUsesOBJCollisionMesh && bPhysicsLowestLOD)
						{
							if (pMesh->dwIndexCountLOD3 > 0 && pMesh->pIndicesLOD3)
							{
								totalVerts = totalVerts + pMesh->dwVertexCount;
								totalIndices = totalIndices + pMesh->dwIndexCountLOD3;
							}
							else if (pMesh->dwIndexCountLOD2 > 0 && pMesh->pIndicesLOD2)
							{
								totalVerts = totalVerts + pMesh->dwVertexCount;
								totalIndices = totalIndices + pMesh->dwIndexCountLOD2;
							}
							else if (pMesh->dwIndexCountLOD1 > 0 && pMesh->pIndicesLOD1)
							{
								totalVerts = totalVerts + pMesh->dwVertexCount;
								totalIndices = totalIndices + pMesh->dwIndexCountLOD1;
							}
							else
							{
								totalVerts = totalVerts + pMesh->dwVertexCount;
								totalIndices = totalIndices + pMesh->dwIndexCount;
							}
						}
						else
						{
							totalVerts = totalVerts + pMesh->dwVertexCount;
							totalIndices = totalIndices + pMesh->dwIndexCount;
						}
					}
				}
			}
		}
	}

	// if more vertices than a WORD can index, use 32bit indices
	if (totalVerts > 0xFFFF)
	{
		bDWORDSizeIndices = true;
	}
	gVertices = new float[totalVerts * 4];
	memset(gVertices, 0, sizeof(float) * totalVerts * 4);

	//PE: We need to convert some triangle lists to indices.
	std::vector<DWORD> dwIndices;

	//LB: get root node rotation to ensure mesh is right way up
	GGMATRIX matRootRotOnly;
	matRootRotOnly = pObjectUsedToExtractPhysicsMesh->ppFrameList[0]->matCombined;
	matRootRotOnly._41 = 0;
	matRootRotOnly._42 = 0;
	matRootRotOnly._43 = 0;

	//PE: This system do not work with Mesh->dwIndexCount == 0 triangle lists they are ignored.
	//PE: use dwIndices and convert triangle lists to indices based.
	
	// for convex hull, collect for vert only initially
	int vcount = 0, icount = 0;
	for (int iFrame = 0; iFrame < pObjectUsedToExtractPhysicsMesh->iFrameCount; iFrame++)
	{
		if (iChooseLowLOD == -1 || iChooseLowLOD == iFrame)
		{
			sFrame* pFrame = pObjectUsedToExtractPhysicsMesh->ppFrameList[iFrame];
			if (pFrame)
			{
				sMesh* pMesh = pFrame->pMesh;
				if (pMesh)
				{
					if (pMesh->Collision.dwArbitaryValue != 99999) // mesh excluded for collision purposes
					{
						// transform matrices for frame/mesh
						GGMATRIX matFinal;
						matFinal = pFrame->matCombined;

						// LB: reverse ANY effect of the frame zero transform, then restore the rotation required
						// this removes the offset portion from the mesh data, and will be returned with moving the physics object directly
						//PE: Using matModelOffset * matFinal on frame 0 invert changes and return a Identity matix, so they even themself out.
						//PE: Other frames with Identity matix do not need this.
						if (matFinal != matIdentity)
						{
							GGMatrixMultiply(&matFinal, &matFinal, &matModelOffset);
						}
						GGMatrixMultiply (&matFinal, &matFinal, &matRootRotOnly);

						// Do not scale raw geometry, we use setlocalscaling on shape instead (so can save and load raw convex hulls,etc)
						GGMATRIX matScale;
						GGMatrixScaling (&matScale, (1.0f * fScalingFactor) / gSc, (1.0f * fScalingFactor) / gSc, (1.0f * fScalingFactor) / gSc);
						GGMatrixMultiply (&matFinal, &matFinal, &matScale);

						// copy vertex to physics mesh buffer
						float* pVertData = (float*)pMesh->pVertexData;
						DWORD dwSkip = pMesh->dwFVFSize / sizeof(float);
						for (int v = 0; v < (int)pMesh->dwVertexCount; v++)
						{
							GGVECTOR3 vec = GGVECTOR3(*(pVertData + 0), *(pVertData + 1), *(pVertData + 2));
							GGVec3TransformCoord (&vec, &vec, &matFinal);
							*(gVertices + ((vcount + v) * 4) + 0) = vec.x;
							*(gVertices + ((vcount + v) * 4) + 1) = vec.y;
							*(gVertices + ((vcount + v) * 4) + 2) = vec.z;
							*(gVertices + ((vcount + v) * 4) + 3) = 1.0f;
							ch_vertexBuffer.push_back(btVector3(vec.x, vec.y, vec.z));
							pVertData += dwSkip;
						}

						WORD* pIndexData = (WORD*)pMesh->pIndices;
						int iIndexCount = pMesh->dwIndexCount;
						if (!bObjectUsesOBJCollisionMesh && bPhysicsLowestLOD)
						{
							if (pMesh->dwIndexCountLOD3 > 0 && pMesh->pIndicesLOD3)
							{
								pIndexData = (WORD*)pMesh->pIndicesLOD3;
								iIndexCount = pMesh->dwIndexCountLOD3;
							}
							else if (pMesh->dwIndexCountLOD2 > 0 && pMesh->pIndicesLOD2)
							{
								pIndexData = (WORD*)pMesh->pIndicesLOD2;
								iIndexCount = pMesh->dwIndexCountLOD2;
							}
							else if (pMesh->dwIndexCountLOD1 > 0 && pMesh->pIndicesLOD1)
							{
								pIndexData = (WORD*)pMesh->pIndicesLOD1;
								iIndexCount = pMesh->dwIndexCountLOD1;
							}
						}
						if (iIndexCount == 0)
						{
							//PE: Convert triangle lists.
							for (int v = 0; v < (int)pMesh->dwVertexCount; v++)
							{
								dwIndices.push_back(vcount + v);
								iIndexCount++;
							}
						}
						else
						{
							for (int i = 0; i < (int)iIndexCount; i++)
							{
								dwIndices.push_back(vcount + (*pIndexData));
								//if (bDWORDSizeIndices == true)
								//	((DWORD*)gIndices)[icount + i] = vcount + (*pIndexData);
								//else
								//	((WORD*)gIndices)[icount + i] = vcount + (*pIndexData);
								pIndexData++;
							}
						}
						vcount += pMesh->dwVertexCount;
						icount += iIndexCount;
					}
				}
			}
		}
	}
	
	totalIndices = dwIndices.size();

	if (bDWORDSizeIndices == true)
	{
		gIndices = (LPVOID)new DWORD[totalIndices];
		memset(gIndices, 0, sizeof(DWORD) * totalIndices);
	}
	else
	{
		gIndices = (LPVOID)new WORD[totalIndices];
		memset(gIndices, 0, sizeof(WORD) * totalIndices);
	}

	//PE: Copy from dwIndices to gIndices.
	for( int i = 0; i < dwIndices.size(); i++)
	{
		if (bDWORDSizeIndices == true)
			((DWORD*)gIndices)[i] = dwIndices[i];
		else
			((WORD*)gIndices)[i] = (WORD) dwIndices[i];
	}
	dwIndices.clear();

	// if turns out this is index buffer based, ch_vertexBuffer data not enough
	if (totalIndices > 0)
	{
		ch_vertexBuffer.clear();
		for (int i = 0; i < totalIndices; i += 3)
		{
			int iFace0, iFace1, iFace2;
			if (bDWORDSizeIndices == true)
			{
				iFace0 = ((DWORD*)gIndices)[i + 0];
				iFace1 = ((DWORD*)gIndices)[i + 1];
				iFace2 = ((DWORD*)gIndices)[i + 2];
			}
			else
			{
				iFace0 = ((WORD*)gIndices)[i + 0];
				iFace1 = ((WORD*)gIndices)[i + 1];
				iFace2 = ((WORD*)gIndices)[i + 2];
			}
			GGVECTOR3 vec;
			vec = GGVECTOR3(*(gVertices + ((iFace0) * 4) + 0), *(gVertices + ((iFace0) * 4) + 1), *(gVertices + ((iFace0) * 4) + 2));
			ch_vertexBuffer.push_back(btVector3(vec.x, vec.y, vec.z));
			vec = GGVECTOR3(*(gVertices + ((iFace1) * 4) + 0), *(gVertices + ((iFace1) * 4) + 1), *(gVertices + ((iFace1) * 4) + 2));
			ch_vertexBuffer.push_back(btVector3(vec.x, vec.y, vec.z));
			vec = GGVECTOR3(*(gVertices + ((iFace2) * 4) + 0), *(gVertices + ((iFace2) * 4) + 1), *(gVertices + ((iFace2) * 4) + 2));
			ch_vertexBuffer.push_back(btVector3(vec.x, vec.y, vec.z));
		}
	}

	// index based mesh or verts only
	if (totalIndices == 0)
	{
		// verts only, so CREATE index list
		totalIndices = totalVerts;
		gIndices = (LPVOID)new DWORD[totalIndices];
		memset (gIndices, 0, sizeof(DWORD)*totalIndices);
		for (int n = 0; n < totalIndices; n++) ((DWORD*)gIndices)[n] = n;
		bDWORDSizeIndices = true;
	}

	// index based mesh
	const int totalTriangles = totalIndices / 3;
	int vertStride = sizeof(float) * 4;
	btMesh.m_numVertices = totalVerts;
	btMesh.m_vertexBase = (const unsigned char*)gVertices;
	btMesh.m_vertexStride = vertStride;
	btMesh.m_numTriangles = totalTriangles;
	btMesh.m_triangleIndexBase = (const unsigned char*)gIndices;

	// free any temp object (used to load OBJ collision data in), unless kept for the next mesh
	if (bObjectUsesOBJCollisionMesh==true)
	{
		if (g_pKeptOBJCollisionObject == NULL && ObjectExist(iObjectUsedToExtractPhysicsMesh) == 1) DeleteObject(iObjectUsedToExtractPhysicsMesh);
		bObjectUsesOBJCollisionMesh = false;
		iObjectUsedToExtractPhysicsMesh = 0;
	}

	// possible shapes
	int iShapeUsed = 0;
	btBvhTriangleMeshShape* trimeshShape;
	btConvexHullShape* simplifiedConvexShape;
	btCompoundShape* compoundShape;

	// convex hull optimization if flagged
	// PE: Convex hull - We keep gVertices,gIndices as mem1/2 , gVertices is used for height data directly on terrain.
	// PE: so we later can test terrain and convex hull. remove (iTerrainMesh != 1)
	int ch_size = ch_vertexBuffer.size();
	if( iHullReduction > 0 && iTerrainMesh != 1 && ch_size > 6 )
	{
		bool bLoadedAndCreatedOkay = false;
		if (strlen(g_ProvidedMeshFilenameForBNVHSaving) > 0)
		{
			// try to load in convex hull/hull decomp shapes
			btBulletWorldImporter* importer = new btBulletWorldImporter();
			char pFindWhereBULLETFIleIsLocated[MAX_PATH];
			strcpy(pFindWhereBULLETFIleIsLocated, g_ProvidedMeshFilenameForBNVHSaving);
			GG_GetRealPath(pFindWhereBULLETFIleIsLocated, 0);
			if (importer->loadFile(pFindWhereBULLETFIleIsLocated))
			{
				int numShapes = importer->getNumCollisionShapes();
				if (numShapes)
				{
					compoundShape = (btCompoundShape*)importer->getCollisionShapeByIndex(0);
					PhysicsOptimiseCompoundHulls(compoundShape);
					bLoadedAndCreatedOkay = true;
					iShapeUsed = 2;
				}
				g_importers.push_back(importer); //PE: Free mem when ending ODE.
			}
		}
		if (bLoadedAndCreatedOkay == false)
		{
			// iHullReduction : 1-single hull, 2-hull decomposition (multiple hulls)
			if (iHullReduction == 2)
			{
				// fill in temp points(verts) and triangles(indices)
				std::vector<float> points;
				std::vector<int> triangles;
				points.clear();
				triangles.clear();
				for (int v = 0; v < ch_vertexBuffer.size(); v += 3)
				{
					points.push_back(ch_vertexBuffer[v + 0].getX());
					points.push_back(ch_vertexBuffer[v + 0].getY());
					points.push_back(ch_vertexBuffer[v + 0].getZ());
					points.push_back(ch_vertexBuffer[v + 1].getX());
					points.push_back(ch_vertexBuffer[v + 1].getY());
					points.push_back(ch_vertexBuffer[v + 1].getZ());
					points.push_back(ch_vertexBuffer[v + 2].getX());
					points.push_back(ch_vertexBuffer[v + 2].getY());
					points.push_back(ch_vertexBuffer[v + 2].getZ());
					triangles.push_back(v + 0);
					triangles.push_back(v + 1);
					triangles.push_back(v + 2);
				}

				// process 'ch_vertexBuffer' and create multiple convex hulls
				VHACD::IVHACD::Parameters paramsVHACD;
				paramsVHACD.Init();
				paramsVHACD.m_maxNumVerticesPerCH = 1024;
				VHACD::IVHACD* interfaceVHACD = VHACD::CreateVHACD();
				bool res = interfaceVHACD->Compute(&points[0], 3, (unsigned int)points.size() / 3, &triangles[0], 3, (unsigned int)triangles.size() / 3, paramsVHACD);
				if (res)
				{
					unsigned int nConvexHulls = interfaceVHACD->GetNConvexHulls();
					VHACD::IVHACD::ConvexHull ch;
					compoundShape = new btCompoundShape();
					for (unsigned int p = 0; p < nConvexHulls; ++p)
					{
						btVector3 vert;
						interfaceVHACD->GetConvexHull(p, ch);
						ch_vertexBuffer.clear();
						for (int tri = 0; tri < (int)ch.m_nTriangles * 3; tri += 3)
						{
							int face0 = ch.m_triangles[tri + 0];
							int face1 = ch.m_triangles[tri + 1];
							int face2 = ch.m_triangles[tri + 2];
							vert = btVector3(btScalar(ch.m_points[(face0 * 3) + 0]), btScalar(ch.m_points[(face0 * 3) + 1]), btScalar(ch.m_points[(face0 * 3) + 2]));
							ch_vertexBuffer.push_back(vert);
							vert = btVector3(btScalar(ch.m_points[(face1 * 3) + 0]), btScalar(ch.m_points[(face1 * 3) + 1]), btScalar(ch.m_points[(face1 * 3) + 2]));
							ch_vertexBuffer.push_back(vert);
							vert = btVector3(btScalar(ch.m_points[(face2 * 3) + 0]), btScalar(ch.m_points[(face2 * 3) + 1]), btScalar(ch.m_points[(face2 * 3) + 2]));
							ch_vertexBuffer.push_back(vert);
						}

						simplifiedConvexShape = new btConvexHullShape(&(ch_vertexBuffer[0].getX()), ch_vertexBuffer.size());
						btTransform localTransform;
						fillTransform(localTransform, 0, 0, 0, 0, 0, 0);
						compoundShape->addChildShape(localTransform, simplifiedConvexShape);
						g_collisionShapes.push_back(simplifiedConvexShape);
					}
					PhysicsOptimiseCompoundHulls(compoundShape);
					g_collisionShapes.push_back(compoundShape);
					iShapeUsed = 2;

					// save out all shapes of hull decomp so can load in quickly in future
					if (strlen(g_ProvidedMeshFilenameForBNVHSaving) > 0)
					{
						char pShapeName[256];
						sprintf(pShapeName, "objectshape");
						btVector3 aabbMin(-1000, -1000, -1000), aabbMax(1000, 1000, 1000);
						int maxSerializeBufferSize = 1024 * 1024 * 5;
						btDefaultSerializer* serializer = new btDefaultSerializer(maxSerializeBufferSize);
						serializer->startSerialization();
						serializer->registerNameForPointer(compoundShape, pShapeName);
						compoundShape->serializeSingleShape(serializer);
						serializer->finishSerialization();
						char pWriteOutBULLETFile[MAX_PATH];
						strcpy(pWriteOutBULLETFile, g_ProvidedMeshFilenameForBNVHSaving);
						GG_GetRealPath(pWriteOutBULLETFile, 1);
						FILE* f2 = fopen(pWriteOutBULLETFile, "wb");
						fwrite(serializer->getBufferPointer(), serializer->getCurrentBufferSize(), 1, f2);
						fclose(f2);

						//PE: We need to free the mem used here.
						serializer->~btDefaultSerializer();
					}
				}

				// free resources
				interfaceVHACD->Clean();
				interfaceVHACD->Release();
				points.clear();
				triangles.clear();
			}
			if (iHullReduction == 1)
			{
				//PE: Add support for convex hull (collisionmode = 9) for faster physics.
				btConvexHullShape* ch_collShape = new btConvexHullShape(&(ch_vertexBuffer[0].getX()), ch_vertexBuffer.size());
				ch_collShape->setMargin(0.00);
				btShapeHull* hull = new btShapeHull(ch_collShape);
				btScalar margin = ch_collShape->getMargin();
				hull->buildHull(margin);
				simplifiedConvexShape = new btConvexHullShape(&(hull->getVertexPointer()[0].getX()), hull->numVertices());
				SAFE_DELETE(ch_collShape);

				// only use CH if large enough
				int hull_size = hull->numVertices();
				if (hull_size < 6)
				{
					m_indexVertexArrays = new btTriangleIndexVertexArray();
					if (bDWORDSizeIndices == true)
					{
						btMesh.m_indexType = PHY_INTEGER;
						btMesh.m_triangleIndexStride = 3 * sizeof(DWORD);
						m_indexVertexArrays->addIndexedMesh(btMesh, PHY_INTEGER);
					}
					else
					{
						btMesh.m_indexType = PHY_SHORT;
						btMesh.m_triangleIndexStride = 3 * sizeof(WORD);
						m_indexVertexArrays->addIndexedMesh(btMesh, PHY_SHORT);
					}
					g_indexvertexarrays.push_back(m_indexVertexArrays);

					trimeshShape = new btBvhTriangleMeshShape(m_indexVertexArrays, useQuantizedAabbCompression, aabbMin, aabbMax, false);
					trimeshShape->buildOptimizedBvh();
					g_collisionShapes.push_back(trimeshShape);
				}
				else
				{
					// we will use convex hull instead below
					g_collisionShapes.push_back(simplifiedConvexShape);
					iShapeUsed = 1;
				}
			}
		}
	}
	else 
	{
		// not HullReduction
		m_indexVertexArrays = new btTriangleIndexVertexArray();
		if (bDWORDSizeIndices == true)
		{
			btMesh.m_indexType = PHY_INTEGER;
			btMesh.m_triangleIndexStride = 3 * sizeof(DWORD);
			m_indexVertexArrays->addIndexedMesh(btMesh, PHY_INTEGER);
		}
		else
		{
			btMesh.m_indexType = PHY_SHORT;
			btMesh.m_triangleIndexStride = 3 * sizeof(WORD);
			m_indexVertexArrays->addIndexedMesh(btMesh, PHY_SHORT);
		}
		g_indexvertexarrays.push_back(m_indexVertexArrays);

		if (strlen(g_ProvidedMeshFilenameForBNVHSaving) == 0)
		{
			// generate regular trimeshshape chunk as normal
			trimeshShape = new btBvhTriangleMeshShape(m_indexVertexArrays, useQuantizedAabbCompression, aabbMin, aabbMax, false);
			trimeshShape->buildOptimizedBvh();
		}
		else
		{
			// see if terrain obj file exists for this chunk?
			//PE: Found it , we was leaking around 400-600mb on each test game.
			//PE: Problem was all the btBulletWorldImporter was never free'ed :)
			//PE: Now keep a list and free everything when physics end's.
			btBulletWorldImporter* importer = new btBulletWorldImporter();
			char pBULLETFileForReading[MAX_PATH];
			strcpy(pBULLETFileForReading, g_ProvidedMeshFilenameForBNVHSaving);
			GG_GetRealPath(pBULLETFileForReading, 0);
			if (importer->loadFile(pBULLETFileForReading))
			{
				// this just saves and loads BVH, half a second quicker than above!
				int numBvh = importer->getNumBvhs();
				if (numBvh)
				{
					btOptimizedBvh* bvh = importer->getBvhByIndex(0);
					btVector3 aabbMin(-1000, -1000, -1000), aabbMax(1000, 1000, 1000);
					trimeshShape = new btBvhTriangleMeshShape(m_indexVertexArrays, useQuantizedAabbCompression, aabbMin, aabbMax, false);
					trimeshShape->setOptimizedBvh(bvh);
				}
				g_importers.push_back(importer); //PE: Free mem when ending ODE.
			}
			else
			{
				// generate trimeshshape for BVH chunk
				trimeshShape = new btBvhTriangleMeshShape(m_indexVertexArrays, useQuantizedAabbCompression, aabbMin, aabbMax, false);
				trimeshShape->buildOptimizedBvh();

				// save out BVH shape so can load in quickly for same terrain physics
				char pTerrainBVHChunkName[256];
				sprintf(pTerrainBVHChunkName, "terrainobj%d", pObject->dwObjectNumber);
				btVector3 aabbMin(-1000, -1000, -1000), aabbMax(1000, 1000, 1000);
				int maxSerializeBufferSize = 1024 * 1024 * 5;
				btDefaultSerializer* serializer = new btDefaultSerializer(maxSerializeBufferSize);
				serializer->startSerialization();
				serializer->registerNameForPointer(trimeshShape, pTerrainBVHChunkName);
				// 3.5 seconds for terrain
				trimeshShape->serializeSingleBvh(serializer);
				serializer->finishSerialization();
				char pWriteOutBULLETFile[MAX_PATH];
				strcpy(pWriteOutBULLETFile, g_ProvidedMeshFilenameForBNVHSaving);
				GG_GetRealPath(pWriteOutBULLETFile, 1);
				FILE* f2 = fopen(pWriteOutBULLETFile, "wb");
				fwrite(serializer->getBufferPointer(), serializer->getCurrentBufferSize(), 1, f2);
				fclose(f2);
				//PE: We need to free the mem used here.
				serializer->~btDefaultSerializer();	
			}
		}
		g_collisionShapes.push_back(trimeshShape);
	}

	// clear usage now hull constructed (if any)
	ch_vertexBuffer.clear();

	// mass/volume calculation
	if ( isDynamic==0 ) fVolume = 0;
	btScalar mass(fVolume/gMs);
	btVector3 localInertia( 0, 0, 0 );
	if (isDynamic == 1)
	{
		switch (iShapeUsed)
		{
			case 0: trimeshShape->calculateLocalInertia(mass, localInertia); break;
			case 1: simplifiedConvexShape->calculateLocalInertia(mass, localInertia); break;
			case 2: compoundShape->calculateLocalInertia(mass, localInertia); break;
		}
	}

	// physics object pos and rotation
	btTransform startTransform;
	fillTransform(startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot);

	// get real scale of object
	btVector3 vLocalShapeScale = btVector3(pObject->position.vecScale.x, pObject->position.vecScale.y, pObject->position.vecScale.z);

	// create rigid body
	btRigidBody* body = NULL;
	btDefaultMotionState* myMotionState = new btDefaultMotionState( startTransform );

	switch (iShapeUsed)
	{
		case 0: 
		{
			trimeshShape->setLocalScaling(vLocalShapeScale);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, trimeshShape, localInertia);
			body = new btRigidBody(rbInfo);
			break;
		}
		case 1:
		{
			simplifiedConvexShape->setLocalScaling(vLocalShapeScale);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, simplifiedConvexShape, localInertia);
			body = new btRigidBody(rbInfo);
			break;
		}
		case 2:
		{
			compoundShape->setLocalScaling(vLocalShapeScale);
			btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, compoundShape, localInertia);
			body = new btRigidBody(rbInfo);
			break;
		}	
	}

	// pass material index as usrptr
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add physics object to system
	short sColliderType = COL_OBJECT;
	if (isDynamic == 1) sColliderType = COL_OBJECT_DYNAMIC;
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	if ( iTerrainMesh == 1 )
		g_dynamicsWorld->addRigidBody ( body, COL_TERRAIN, sCollidesWith);
	else
		g_dynamicsWorld->addRigidBody ( body, sColliderType, sCollidesWith);

	// boyancy mode
	bool bBouyant = false;
	if ( isDynamic==1 ) 
		bBouyant = true; 
	else
		pObject = NULL;

	// add physics object details to pbject list
	ODEAddObject ( iObjectNumber, pObject, body, isDynamic, myMotionState, false, bBouyant, gVertices, gIndices, 1, 1, fFriction, mass, false ); // placed at the object's origin, not centred
}

void DeleteallImporters()
{
	for (int i = 0; i < g_importers.size(); i++)
	{
		g_importers[i]->deleteAllData();
	}
	g_importers.clear();
}

void CreateTerrain ( int iObjectNumber, int isDynamic, int iWidth, int iLength, DWORD dwMemBlockPtr )
{
	// problem is that Blitzwerks uses alternating quad edge order which means polygon slope cannot be
	// predicted by Bullet height map shape (pity as it would have been low memory fast performance

	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	GetSizePositionRotation(pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, false );

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// create a bullet shape
	int s_gridSize = iWidth; // 1024x1024
	float m_minHeight = 0.0f;
	float m_maxHeight = 50000.0f/gSc;
	int m_upAxis = 1;
	bool flipQuadEdges = true;
	float* m_rawHeightfieldData = new float[s_gridSize*s_gridSize];
	memset ( m_rawHeightfieldData, 0, sizeof(float)*s_gridSize*s_gridSize );
	float* pMemBlockPtr = (float*)dwMemBlockPtr;
	for ( int t=0; t<s_gridSize*s_gridSize; t++ )
	{
		m_rawHeightfieldData[t] = (*pMemBlockPtr) / gSc;
		pMemBlockPtr++;
	}
	btHeightfieldTerrainShape* terrainShape = new btHeightfieldTerrainShape(	s_gridSize, s_gridSize,
																					m_rawHeightfieldData,
																					0.0f,
																					m_minHeight, m_maxHeight,
																					m_upAxis, PHY_FLOAT, flipQuadEdges);
	btVector3 localScaling = btVector3(50.0f/gSc,1.0f,50.0f/gSc);
	terrainShape->setLocalScaling(localScaling);

	// add shape to collision list so can release later, they can be "re-used" amongst bodies!!
	g_collisionShapes.push_back(terrainShape);

	// fully static is our ground terrain!!
	btScalar mass(0/gMs);
	btVector3 localInertia(0,0,0);
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin( btVector3( fXPos, fYPos, fZPos ) );

	// motionstate provides interpolation capabilities and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,terrainShape,localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);

	// pass material index as usrptr
	sObject* pRealObject = pObject;
	if ( pRealObject->pInstanceOfObject ) pRealObject = pRealObject->pInstanceOfObject;
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add physics object to system
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	g_dynamicsWorld->addRigidBody ( body, COL_TERRAIN, sCollidesWith);

	// boyancy mode
	bool bBouyant = false;
	if ( isDynamic==1 ) 
		bBouyant = true; 
	else
		pObject = NULL;

	// add physics object details to pbject list
	ODEAddObject ( iObjectNumber, pObject, body, isDynamic, myMotionState, false, bBouyant, (LPVOID)m_rawHeightfieldData, NULL, 1, 1, 100.0f, mass );
}

void UpdateTerrain ( int iObjectNumber, int isDynamic, int iWidth, int iLength, LPSTR dwMemBlockPtr, int iX1, int iZ1, int iX2, int iZ2 )
{
	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return;

	// NOTE: Terrain collision replaced with sector meshes (which will need to be updated instead!!)
	float* m_rawHeightfieldData = (float*)pPhyObject->pMem1;
	if ( m_rawHeightfieldData==NULL ) return;

	// create a bullet shape
	int s_gridSize = iWidth; // 1024x1024
	float m_minHeight = 0.0f;
	float m_maxHeight = 50000.0f/gSc;
	int m_upAxis = 1;
	bool flipQuadEdges = true;
	float* pMemBlockPtr = (float*)dwMemBlockPtr;
	for ( int y=iZ1; y<=iZ2; y++ )
	{
		for ( int x=iX1; x<=iX2; x++ )
		{
			int t = (y*s_gridSize)+x;
			m_rawHeightfieldData[t] = (*pMemBlockPtr) / gSc;
			pMemBlockPtr++;
		}
	}

	// remove body from simulation
	g_dynamicsWorld->removeRigidBody(pPhyObject->body);

	// remove collision shape
	btHeightfieldTerrainShape* terrainOLDShape = (btHeightfieldTerrainShape*)pPhyObject->body->getCollisionShape();
	delete terrainOLDShape;

	// create new shape
	btHeightfieldTerrainShape* terrainNEWShape = new btHeightfieldTerrainShape(	s_gridSize, s_gridSize,
																					m_rawHeightfieldData,
																					0.0f,
																					m_minHeight, m_maxHeight,
																					m_upAxis, PHY_FLOAT, flipQuadEdges);

	// prepare new shape intertia
	btVector3 localScaling = btVector3(50.0f/gSc,1.0f,50.0f/gSc);
	terrainNEWShape->setLocalScaling(localScaling);

	// assign new shape
	pPhyObject->body->setCollisionShape(terrainNEWShape);

	// pass material index as usrptr
	sObject* pRealObject = pObject;
	if ( pRealObject->pInstanceOfObject ) pRealObject = pRealObject->pInstanceOfObject;
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	pPhyObject->body->setUserPointer((void*)iArbValue);
	pPhyObject->body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add new body back to simulation
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	g_dynamicsWorld->addRigidBody ( pPhyObject->body, COL_TERRAIN, sCollidesWith);

	// replace shape in collision shapes list
	for (int j=0;j<g_collisionShapes.size();j++)
	{
		if ( g_collisionShapes[j]==terrainOLDShape )
		{
			g_collisionShapes[j] = terrainNEWShape;
			break;
		}
	}
}

btCollisionShape* g_pTerrainShape = 0;

void ODECreateGGTerrain()
{
	g_pTerrainShape = new GGTerrainPhysicsShape( gSc );

	g_collisionShapes.push_back( g_pTerrainShape );

	btScalar mass( 0 );
	btVector3 localInertia( 0, 0, 0 );
	btTransform startTransform;
	startTransform.setIdentity();
	startTransform.setOrigin( btVector3(0, 0, 0) );

	btDefaultMotionState* myMotionState = new btDefaultMotionState( startTransform );
	btRigidBody::btRigidBodyConstructionInfo rbInfo( mass, myMotionState, g_pTerrainShape, localInertia );
	btRigidBody* body = new btRigidBody( rbInfo );

	body->setUserIndex(0); // the ground (GGPhysicsQuery)
	body->setUserPointer((void*)12345);// indicate this is terrain and uses material look up for footfall sounds - old was 0 );

	// add physics object to system
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR;
	g_dynamicsWorld->addRigidBody ( body, COL_TERRAIN, sCollidesWith);

	//PE: Add fake collision callback for new ggterrain system.
	ODEAddObject(7000, nullptr, body, false, myMotionState, false, false, NULL, NULL, 1, 1, 100.0f, mass);

}

void CreateCapsule ( int iObjectNumber, int isDynamic, float fScaleModifier, float fRaised, float fWeight, float fFriction, float fCollisionScaling, float fCollisionScalingXZ)
{
	// defaults
	if ( fWeight==-1 ) fWeight = 100.0f;
	if ( fFriction==-1 ) fFriction = 90.0f;
	if (fCollisionScaling == 0) fCollisionScaling = 1.0f;
	if (fCollisionScalingXZ == 0) fCollisionScalingXZ = 1.0f;
	float fRestitution = 0.0f;

	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	GetSizePositionRotation(pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, true);

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// volume calculation
	float fVolume = 0;
	fVolume = fXSize * fYSize * fZSize;

	// modify with weight modifier
	fVolume *= (fWeight/100.0f);

	// instead of fixed capsule height, calc proportion so a character 73 units high gives capsule of 80(half=40)
	float fProportionalCapsuleHeight = (fYSize*gSc)*0.55f;

	// additional ability to scale the capsule height
	fProportionalCapsuleHeight *= fCollisionScaling;

	// apply modifier
	if ( fScaleModifier==0.0f ) fScaleModifier = 1.0f;
	float fCapsuleHeight = fProportionalCapsuleHeight/gSc;
	fCapsuleHeight = fCapsuleHeight * fScaleModifier;

	// somenow affects final height of capsule in simulation, find out metrics!!
	// also use proportional radius for capsule (fat animals; cows)
	#ifdef WICKEDENGINE
	// LB: nav mesh allows paths through thinner gaps, still so reduce a little more
	float fProportionalCapsuleRadius = (((fXSize + fZSize) / 2.0f / 2.0f)*gSc)*0.5f;
	#else
	float fProportionalCapsuleRadius = (((fXSize+fZSize)/2.0f/2.0f)*gSc)*0.8f; // so zombie can get through narrower door!
	#endif

	// will offset real object by HALF of total height of capsule (sphere halves+cylinder)
	fRaised = fRaised + ((fProportionalCapsuleRadius*2)+fProportionalCapsuleHeight * fScaleModifier)/2.0f;

	// create a bullet shape (fScaleModifier applied to support on-standard characters; creatures)
	float fCapsuleWidth = (fProportionalCapsuleRadius/gSc) * fScaleModifier * fCollisionScalingXZ;
	btCollisionShape* capsuleShape = new btCapsuleShape(fCapsuleWidth, fCapsuleHeight);

	// add shape to collision list so can release later, they can be "re-used" amongst bodies!!
	g_collisionShapes.push_back(capsuleShape);

	// mass/volume calculation
	if ( isDynamic==0 )
	{
		fVolume = 0;
	}
	else
	{
		// max
		if (fVolume > maxDynamicVolume) fVolume = maxDynamicVolume;

		// also handle min - ensure MASS not SO SMALL THINGS FLY MILLIONS OF MILES!
		if (fVolume < 0.1f) fVolume = 0.1f;
	}
	btScalar mass(fVolume/gMs);
	btVector3 localInertia(0,0,0);
	if ( isDynamic==1 ) capsuleShape->calculateLocalInertia(mass,localInertia);

	// set position transform for physics object
	btTransform startTransform;
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );

	// motionstate provides interpolation capabilities and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,capsuleShape,localInertia);
	rbInfo.m_friction = 1.0f;
	rbInfo.m_restitution = fRestitution/100.0f;
	btRigidBody* body = new btRigidBody(rbInfo);

	// ensure capsule stands upright
	body->setAngularFactor(0.0f);

	// pass material index as usrptr
	sObject* pRealObject = pObject;
	if ( pRealObject->pInstanceOfObject ) pRealObject = pRealObject->pInstanceOfObject;
	int iArbValue = 0;
	if ( pRealObject->ppMeshList!=NULL ) iArbValue = pRealObject->ppMeshList[0]->Collision.dwArbitaryValue;
	body->setUserPointer((void*)iArbValue);
	body->setUserIndex(iObjectNumber); // the object it stands for (GGPhysicsQuery)

	// add physics object to system
	short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_TERRAIN;
	g_dynamicsWorld->addRigidBody ( body, COL_CAPSULECHAR, sCollidesWith);

	// add physics object details to pbject list
	bool bBouyant = false;
	ODEAddObject ( iObjectNumber, pObject, body, isDynamic, myMotionState, true, bBouyant, NULL, NULL, fScaleModifier, fRaised, fFriction, mass );
}

void ODECreateDynamicCharacterController ( int iObjectNumber, float fGravity, float fFallSpeed, float fMaxSlope )
{
	// set new global defaults for character controller
	fCharacterGravity = fGravity;
	fCharacterFallSpeed = fFallSpeed;
	fCharacterMaxSlope = fMaxSlope;

	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// obtain sizes, position and rotation
	float fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot;
	// 280618 - this threw initial player position up (incorrectly correcting for object center!)
	bool bMakeCollisionCenterAdjustment = false;
	GetSizePositionRotation(pObject, fXSize, fYSize, fZSize, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot, bMakeCollisionCenterAdjustment); 

	// just before shape creation, reduce scale of size and position
	fXSize /= gSc;
	fYSize /= gSc;
	fZSize /= gSc;
	fXPos /= gSc;
	fYPos /= gSc;
	fZPos /= gSc;

	// set position transform for physics object
	btTransform startTransform;
	fillTransform( startTransform, fXPos, fYPos, fZPos, fXRot, fYRot, fZRot );

	// create a character controller shape (if not already created)
	if ( m_ghostObject==NULL )
	{
		// create a new
		m_ghostObject = new btPairCachingGhostObject();
		m_ghostObject->setWorldTransform(startTransform);
		sweepBP->getOverlappingPairCache()->setInternalGhostPairCallback(new btGhostPairCallback());
		btScalar characterWidth  = 15.0f/gSc;
		btScalar characterHeight = 40.0f/gSc;

		btConvexShape* capsule = new btCapsuleShape(characterWidth,characterHeight);
		m_ghostObject->setCollisionShape (capsule);
		m_ghostObject->setCollisionFlags (btCollisionObject::CF_CHARACTER_OBJECT);
		btScalar stepHeight = btScalar(10.35 / gSc);
		m_character = new btKinematicCharacterController (m_ghostObject,capsule,stepHeight);

		// only collide with static for now (no interaction with dynamic objects)
		#ifdef WICKEDENGINE
		g_dynamicsWorld->addCollisionObject (m_ghostObject, COL_CAPSULECHAR, COL_OBJECT | COL_OBJECT_DYNAMIC | COL_TERRAIN);
		#else
		g_dynamicsWorld->addCollisionObject (m_ghostObject, COL_OBJECT, COL_OBJECT | COL_OBJECT_DYNAMIC | COL_TERRAIN);
		#endif
		g_dynamicsWorld->addAction(m_character);
	}
	else
	{
		// simply reposition existing
		m_ghostObject->setWorldTransform(startTransform);
	}

	// required for character
	g_dynamicsWorld->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(m_ghostObject->getBroadphaseHandle(),g_dynamicsWorld->getDispatcher());

	// setup character settings
	m_character->reset ();
	m_character->resetFallStuff();
	m_character->setGravity(fCharacterGravity/gSc); 
	m_character->setFallSpeed(fCharacterFallSpeed/gSc);
	m_character->setMaxSlope(GGToRadian(fCharacterMaxSlope));
	m_character->warp (btVector3(fXPos,fYPos,fZPos));
	m_character->m_bCurrentlyDucked = false;
	m_character->m_bDidJump = false;

	// and pass water level to character
	m_character->setWaterLineY (g_fWaterLineY);

	// add physics object details to pbject list
	bool bBouyant = false;
	ODEAddObject ( iObjectNumber, pObject, NULL, 1, NULL, false, bBouyant, NULL, NULL, NULL, NULL, 100.0f, -1.0f );
}

void ODESetDynamicCharacterController ( int iObjectNumber, float fWaterLineY, float fRes1, float fRes2, float fRes3, float fRes4, float fRes5, float fRes6 )
{
	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;

	// set properties for the character on the fly
	g_fWaterLineY = fWaterLineY;
	if ( m_character ) m_character->setWaterLineY ( fWaterLineY );
}

void ODESetWaterLine ( float fWaterLineY )
{
	// set properties for the character on the fly
	g_fWaterLineY = fWaterLineY;
	if ( m_character ) m_character->setWaterLineY ( fWaterLineY );
}

// Manually set character walk direction. axisX,Y,Z are local to the player. Should be called after ODEControlDynamicCharacterController.
void ODESetCharacterDirectionOverride(float angX, float angY, float angZ, float axisX, float axisY, float axisZ, float mag) 
{
	if (m_character)
	{
		GGMATRIX matRot;
		GGMatrixIdentity(&matRot);
		GGMATRIX matX, matY, matZ;

		// Create a rotation matrix using the provided angles.
		GGMatrixRotationX(&matX, GGToRadian(angX));
		GGMatrixRotationY(&matY, GGToRadian(angY));
		GGMatrixRotationZ(&matZ, GGToRadian(angZ));
		GGMatrixMultiply(&matRot, &matRot, &matX);
		GGMatrixMultiply(&matRot, &matRot, &matY);
		GGMatrixMultiply(&matRot, &matRot, &matZ);
	
		// Transform default forward vector by the rotation matrix to get the new direction vector.
		GGVECTOR3 look(axisX, axisY, axisZ);
		GGVec3TransformCoord(&look, &look, &matRot);
		GGVec3Normalize(&look, &look);

		#ifdef PRODUCTCONVERTER
		m_character->setWalkDirection((btVector3(look.x, look.y, look.z)*mag) / gSc);
		#else
		if (m_character->GetSwimmingLimitVertical())
			m_character->setWalkDirection((btVector3(look.x, 0, look.z)*mag) / gSc);
		else
			m_character->setWalkDirection((btVector3(look.x, look.y, look.z)*mag) / gSc);
		#endif
	}
}

void ODESwimmingLimitVertical(int limit)
{
#ifdef WICKEDENGINE
	m_character->SetSwimmingLimitVertical(limit);
#endif
}

void ODEGetCharacterWalkDirection(GGVECTOR3* look)
{
#ifdef WICKEDENGINE
	btVector3 direction = m_character->GetWalkDirection();
	GGVECTOR3 dir = GGVECTOR3(direction.getX(), direction.getY(), direction.getZ());
	GGVec3Normalize(look, &dir);
#endif
}

bool g_bGlobalCurrentlyDucked = false;

void ODEControlDynamicCharacterController ( int iObjectNumber, float fAngleY, float fAngleX, float fSpeed, float fJump, float fDucking, float fPushAngle, float fPushForce, float fThrustUpwards )
{
	// get reference object
	sObject* pObject = GetObjectData ( iObjectNumber );
	if ( pObject==NULL ) return;
	if ( m_character==NULL ) return;

	// if physics world present
	if ( g_dynamicsWorld!=NULL )
	{
		GGMATRIX matRotX;
		GGMatrixRotationX ( &matRotX, GGToRadian(fAngleX) );
		GGMATRIX matRotY;
		GGMatrixRotationY ( &matRotY, GGToRadian(fAngleY) );
		GGMATRIX matRot;
		GGMatrixMultiply ( &matRot, &matRotX, &matRotY );
		GGVECTOR3 vecForward = GGVECTOR3(0,0,1);
		GGVec3TransformCoord ( &vecForward, &vecForward, &matRot );
		if ( fAngleX<0 ) vecForward.y *= -1;
		btVector3 walkDirection = btVector3(vecForward.x,vecForward.y,vecForward.z);
		btScalar walkVelocity = fSpeed;
		btScalar walkSpeed = walkVelocity;

		// and pass water level to character
		m_character->setWaterLineY (g_fWaterLineY);

		// control character controller jumping
		m_character->m_bDidJump = false;
		if ( m_character->canJump() )
		{
			if ( fJump > 0.0f )
			{
				m_character->setJumpSpeed(fJump/gSc);
				m_character->jump();
			}
		}

		// new walk/push method work together
		// push character if forced in a specific direction
		// control character controller for walk direction
		btVector3 bvWalkDir = walkDirection * walkSpeed;
		btVector3 bvCombinedDir = bvWalkDir;
		if (fPushForce > 0.0f)
		{
			float fPushAngleRad = GGToRadian(fPushAngle);
			btVector3 vecPush = btVector3(sin(fPushAngleRad), 0.0, cos(fPushAngleRad));
			bvCombinedDir += (vecPush * fPushForce);
		}
		if (fabs(bvCombinedDir.getX())>0 || fabs(bvCombinedDir.getY())>0 || fabs(bvCombinedDir.getZ())>0)
		{
			bPlayerShouldbeMoving = true;
			m_character->setWalkDirection(bvCombinedDir / gSc);
		}
		else 
		{
			bPlayerShouldbeMoving = false;
			m_character->setWalkDirection(btVector3(0.0f, 0.0f, 0.0f));
		}
		/* old method can only walk OR push, not both
		// push character if forced in a specific direction
		if ( fPushForce>0.0f )
		{
			// cannot walk when being pushed
			float fPushAngleRad = GGToRadian(fPushAngle);
			btVector3 vecPush = btVector3(sin(fPushAngleRad),0.0,cos(fPushAngleRad));
			m_character->setWalkDirection((vecPush*fPushForce)/gSc);
		}
		else
		{
			// control character controller for walk direction
			btVector3 bvWalkDir = walkDirection * walkSpeed;
			if (walkSpeed > 0) {
				bPlayerShouldbeMoving = true;
				m_character->setWalkDirection(bvWalkDir / gSc);
			}
			else {
				bPlayerShouldbeMoving = false;
				m_character->setWalkDirection(btVector3(0.0f,0.0f,0.0f));
			}
		}
		*/

		// Apply thrust value to system
		m_character->m_fThrustUpwards = fThrustUpwards;

		// Ducking control
		float fXPos = pObject->position.vecPosition.x;
		float fYPos = pObject->position.vecPosition.y+1;
		float fZPos = pObject->position.vecPosition.z;
		if ( fDucking==1.0f )
		{
			if (g_bGlobalCurrentlyDucked == false )// m_character->m_bCurrentlyDucked==false )
			{
				// change to small capsule
				btCollisionShape* oldrigidbodyshape = m_ghostObject->getCollisionShape();
				btScalar characterWidth  = 15.0f/gSc;
				btScalar characterHeight = 22.0f/gSc;
				btConvexShape* capsule = new btCapsuleShape(characterWidth,characterHeight);
				m_ghostObject->setCollisionShape ( capsule );
				m_character->SetConvexShape ( capsule );
				delete oldrigidbodyshape;

				//m_character->m_bCurrentlyDucked = true;
				g_bGlobalCurrentlyDucked = true;

				// reposition character for ducking (otherwise it jitters)
				m_character->warp (btVector3(fXPos/gSc,fYPos/gSc,fZPos/gSc));
			}
		}
		else
		{
			if (g_bGlobalCurrentlyDucked==true )//m_character->m_bCurrentlyDucked==true )
			{
				// do raycast to see if we can unduck 
				bool bHitSomething = false;
				for ( int allcorners=0; allcorners<5; allcorners++ )
				{
					btVector3 Start, End, Normal;
					if ( allcorners==0 ) { Start = btVector3(fXPos,fYPos,fZPos); End = btVector3(fXPos,fYPos+40,fZPos); }
					if ( allcorners==1 ) { Start = btVector3(fXPos-15,fYPos,fZPos-15); End = btVector3(fXPos-15,fYPos+40,fZPos-15); }
					if ( allcorners==2 ) { Start = btVector3(fXPos+15,fYPos,fZPos-15); End = btVector3(fXPos+15,fYPos+40,fZPos-15); }
					if ( allcorners==3 ) { Start = btVector3(fXPos-15,fYPos,fZPos+15); End = btVector3(fXPos-15,fYPos+40,fZPos+15); }
					if ( allcorners==4 ) { Start = btVector3(fXPos+15,fYPos,fZPos+15); End = btVector3(fXPos+15,fYPos+40,fZPos+15); }
					Start.setX(Start.getX() / gSc);
					Start.setY(Start.getY() / gSc);
					Start.setZ(Start.getZ() / gSc);
					End.setX(End.getX() / gSc);
					End.setY(End.getY() / gSc);
					End.setZ(End.getZ() / gSc);
					btCollisionWorld::ClosestRayResultCallback RayCallback(Start, End);
					g_dynamicsWorld->rayTest(Start, End, RayCallback);
					if(RayCallback.hasHit())
					{
						//End = RayCallback.m_hitPointWorld;
						//Normal = RayCallback.m_hitNormalWorld;
						bHitSomething = true;
					}
				}

				// if hit, don't get up yet!
				if ( bHitSomething==false )
				{
					// change back to regular capsule
					btCollisionShape* oldrigidbodyshape = m_ghostObject->getCollisionShape();
					btScalar characterWidth  = 15.0f/gSc;
					btScalar characterHeight = 40.0f/gSc;
					btConvexShape* capsule = new btCapsuleShape(characterWidth,characterHeight);
					m_ghostObject->setCollisionShape ( capsule );
					m_character->SetConvexShape ( capsule );
					delete oldrigidbodyshape;

					//m_character->m_bCurrentlyDucked = false;
					g_bGlobalCurrentlyDucked = false;
				}
			}
		}

		// ensure object associated with controller is updated in ODEUPDATE
		g_CharacterControlObject = iObjectNumber;
	}
}

DWORD ODEGetDynamicCharacterControllerDidJump ( void )
{
	if ( m_character )
	{
		if ( m_character->m_bDidJump==true )
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

DWORD ODEGetCharacterControllerDucking ( int iObjectNumber )
{
	if ( m_character )
	{
		if (g_bGlobalCurrentlyDucked==true) //m_character->m_bCurrentlyDucked==true )
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

// handle occasional skipping over materials (occasionally zero and then touch surface again
int iTouchFloorMaterialCount = 0;
int iTouchFloorMaterialID = 0;

int ODEGetCharacterHitFloor ( void )
{
	// returns value passed in for this collider (material type)
	if ( m_character )
	{
		if (m_character->iHitFloor > 0)
		{
			iTouchFloorMaterialCount = 50;
			iTouchFloorMaterialID = m_character->iHitFloor;
			m_character->iHitFloor = 0;
		}
		else
		{
			if (iTouchFloorMaterialCount > 0)
			{
				iTouchFloorMaterialCount--;
				if (iTouchFloorMaterialCount <= 0)
				{
					iTouchFloorMaterialCount = 0;
					iTouchFloorMaterialID = 0;
				}
			}
		}
		return iTouchFloorMaterialID;
	}
	else
		return 0;
}
float ODEGetCharacterFallDistance ( void )
{
	if ( m_character )
	{
		float fReturnValue = m_character->fFallDistance * gSc;
		fReturnValue = fReturnValue / 2.0f; //seems too sensitive, check with debug later!
		m_character->fFallDistance = 0.0f;
		return fReturnValue;
	}
	else
		return 0;
}

void ODECreateStaticSphere	( int iObjectNumber )
{
	CreateSphere ( iObjectNumber, 0, -1, -1, -1 );
}
void ODECreateStaticBox ( int iObjectNumber, int iLimbNumber )
{
	CreateBox ( iObjectNumber, 0, iLimbNumber, 0, -1, -1, -1, false, 0,0,0,0,0,0,0,0,0 );
}
void ODECreateStaticBox ( int iObjectNumber )
{
	CreateBox ( iObjectNumber, 0, -1, 0, -1, -1, -1, false, 0,0,0,0,0,0,0,0,0);	            
}
void ODECreateStaticCylinder ( int iObjectNumber )
{
	CreateCylinder ( iObjectNumber, 0, -1, -1, -1 );
}
void ODECreateStaticCylinder ( int iObjectNumber, float fXPos, float fYPos, float fZPos, float fXSize, float fYSize, float fZSize, float fXRot, float fYRot, float fZRot )
{
	CreateCylinder ( iObjectNumber, 0, fXPos, fYPos, fZPos, fXSize, fYSize, fZSize, fXRot, fYRot, fZRot, -1, -1, -1 );
}
void ODESetMeshFilename (LPSTR pName)
{
	SetMeshFilename (pName);
}
void ODESetOBJLoadingFilename (LPSTR pName)
{
	SetOBJLoadingFilename (pName);
}
void ODEKeepOBJCollisionMesh ( int iKeep )
{
	KeepOBJCollisionMesh ( iKeep != 0 );
}
void ODECreateStaticTerrainMesh ( int iObjectNumber )
{
	CreateMesh ( iObjectNumber, 0, -1, 1, 100, 0, -1, -1, -1 );
}
void ODECreateStaticTriangleMesh ( int iObjectNumber )
{
	CreateMesh ( iObjectNumber, 0, -1, 0, 100, 0, -1, -1, -1 );
}
void ODECreateStaticTriangleMesh ( int iObjectNumber, int iLimbNumber )
{
	CreateMesh ( iObjectNumber, 0, iLimbNumber, 0, 100, 0, -1, -1, -1 );
}
void ODECreateStaticTriangleMesh ( int iObjectNumber, int iLimbNumber, int iCollisionScaling )
{
	CreateMesh ( iObjectNumber, 0, iLimbNumber, 0, iCollisionScaling, 0, -1, -1, -1 );
}
void ODECreateStaticTriangleMesh ( int iObjectNumber, int iLimbNumber, int iCollisionScaling, int iHullReductionMode )
{
	CreateMesh ( iObjectNumber, 0, iLimbNumber, 0, iCollisionScaling, iHullReductionMode, -1, -1, -1 );
}
void ODECreateStaticTerrain ( int iObjectNumber, int iWidth, int iLength, DWORD dwMemBlockPtr )
{
	CreateTerrain ( iObjectNumber, 0, iWidth, iLength, dwMemBlockPtr );
}
void ODEUpdateStaticTerrain ( int iObjectNumber, int iWidth, int iLength, LPSTR dwMemBlockPtr, int iX1, int iZ1, int iX2, int iZ2 )
{
	UpdateTerrain ( iObjectNumber, 0, iWidth, iLength, dwMemBlockPtr, iX1, iZ1, iX2, iZ2 );
}

// GG: static bodies made from triangles of the world's own (the spline tool's pavements), not an object's: in the ground's
// group with no object, so the player, rays and contacts take them as ground (object 0), their material for footsteps and
// impacts; each kept by a handle until deleted, or until the world ends (ODEEnd deletes what is left first)
struct sStaticTriangleBody
{
	btTriangleMesh* pMesh = NULL;
	btBvhTriangleMeshShape* pShape = NULL;
	btDefaultMotionState* pMotion = NULL;
	btRigidBody* pBody = NULL;
};
static std::unordered_map<int, sStaticTriangleBody> g_StaticTriangleBodies;
static int g_iNextStaticTriangleBody = 1;

static void DeleteStaticTriangleBody ( sStaticTriangleBody& b )
{
	if ( g_dynamicsWorld && b.pBody ) g_dynamicsWorld->removeRigidBody ( b.pBody );
	SAFE_DELETE ( b.pBody );
	SAFE_DELETE ( b.pMotion );
	SAFE_DELETE ( b.pShape );
	SAFE_DELETE ( b.pMesh );
}

// the triangles' corners, x y z each in world units, three a triangle; 0 when there is no world or nothing to make
int ODECreateStaticTriangleMeshData ( const float* pPositions, int iTriangleCount, int iMaterial )
{
	if ( !g_dynamicsWorld || !pPositions || iTriangleCount <= 0 ) return 0;
	std::lock_guard<std::recursive_mutex> lock ( physicslock );
	sStaticTriangleBody b;
	b.pMesh = new btTriangleMesh ( true, false );
	for ( int t = 0; t < iTriangleCount; t++ )
	{
		const float* p = pPositions + t * 9;
		b.pMesh->addTriangle ( btVector3 ( p[0] / gSc, p[1] / gSc, p[2] / gSc ), btVector3 ( p[3] / gSc, p[4] / gSc, p[5] / gSc ), btVector3 ( p[6] / gSc, p[7] / gSc, p[8] / gSc ), false );
	}
	b.pShape = new btBvhTriangleMeshShape ( b.pMesh, true );
	btTransform transform;
	transform.setIdentity();
	b.pMotion = new btDefaultMotionState ( transform );
	btRigidBody::btRigidBodyConstructionInfo rbInfo ( 0, b.pMotion, b.pShape, btVector3 ( 0, 0, 0 ) );
	b.pBody = new btRigidBody ( rbInfo );
	b.pBody->setUserPointer ( (void*)(intptr_t)iMaterial );
	b.pBody->setUserIndex ( -1 );
	b.pBody->setUserIndex2 ( 1 ); // a surface built from a spline (PHYSICS_SURFACE_SPLINE, PhysicsRay's surface)
	const short sCollidesWith = COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | COL_TERRAIN;
	g_dynamicsWorld->addRigidBody ( b.pBody, COL_TERRAIN, sCollidesWith );
	const int iHandle = g_iNextStaticTriangleBody++;
	g_StaticTriangleBodies[ iHandle ] = b;
	return iHandle;
}

void ODEDeleteAllStaticTriangleMeshData ( void )
{
	std::lock_guard<std::recursive_mutex> lock ( physicslock );
	for ( auto& entry : g_StaticTriangleBodies ) DeleteStaticTriangleBody ( entry.second );
	g_StaticTriangleBodies.clear();
}

void ODEDeleteStaticTriangleMeshData ( int iHandle )
{
	auto it = g_StaticTriangleBodies.find ( iHandle );
	if ( it == g_StaticTriangleBodies.end() ) return;
	std::lock_guard<std::recursive_mutex> lock ( physicslock );
	DeleteStaticTriangleBody ( it->second );
	g_StaticTriangleBodies.erase ( it );
}
void ODECreateStaticCapsule ( int iObjectNumber, float fCollisionScaling, float fCollisionScalingXZ)
{
	CreateCapsule ( iObjectNumber, 0, 1.0f, 0.0f, -1, -1, fCollisionScaling, fCollisionScalingXZ);
}
void ODECreateDynamicSphere ( int iObjectNumber, float fWeight, float fFriction, float fRestitution )
{
	CreateSphere ( iObjectNumber, 1, fWeight, fFriction, fRestitution );
}
void ODECreateDynamicBox ( int iObjectNumber )
{
	CreateBox ( iObjectNumber, 1, -1, 0, -1, -1, -1, false,0,0,0,0,0,0,0,0,0 );
}
void ODECreateDynamicBox ( int iObjectNumber, int iLimbNumber, int iTipOverMode )
{
	CreateBox ( iObjectNumber, 1, iLimbNumber, iTipOverMode, -1, -1, -1 , false, 0,0,0,0,0,0,0,0,0);
}
void ODECreateDynamicBox ( int iObjectNumber, int iLimbNumber, int iTipOverMode, float fWeight, float fFriction, float fRestitution )
{
	CreateBox ( iObjectNumber, 1, iLimbNumber, iTipOverMode, fWeight, fFriction, fRestitution, false, 0,0,0,0,0,0,0,0,0 );
}
void ODECreateDynamicBoxManual( int iObjectNumber, float fX, float fY, float fZ, float fXSize, float fYSize, float fZSize, float fXAng, float fYAng, float fZAng)
{
	CreateBox(iObjectNumber, 1, -1, 0, -1, -1, -1, true, fX,fY,fZ,fXSize,fYSize,fZSize,fXAng,fYAng,fZAng);
}
void ODECreateDynamicCylinder ( int iObjectNumber, float fWeight, float fFriction, float fRestitution )
{
	CreateCylinder ( iObjectNumber, 1, fWeight, fFriction, fRestitution );
}

void ODECreateDynamicTriangleMesh(int iObjectNumber, int iLimbNumber, int iCollisionScaling, int iHullReductionMode)
{
	CreateMesh(iObjectNumber, 1, iLimbNumber, 0, iCollisionScaling, iHullReductionMode, -1, -1, -1);
}

void ODECreateDynamicTriangleMesh ( int iObjectNumber, float fWeight, float fFriction, float fRestitution, int iHullReductionMode )
{
	CreateMesh ( iObjectNumber, 1, -1, 0, 100, iHullReductionMode, fWeight, fFriction, fRestitution );
}
void ODECreateDynamicCapsule ( int iObjectNumber, float fScaleModifier, float fRaised, float fWeight, float fFriction, float fCollisionScaling, float fCollisionScalingXZ )
{
	CreateCapsule ( iObjectNumber, 1, fScaleModifier, fRaised, fWeight, fFriction, fCollisionScaling, fCollisionScalingXZ);
}
void ODEStartStaticObject ( int iObjectNumber )
{
	StartStaticObject ( iObjectNumber );
}
void ODEEndStaticObject ( int iObjectNumber, int iTerrainCollider  )
{
	EndStaticObject ( iObjectNumber, iTerrainCollider );
}
void ODEAddStaticObjectBox ( int iObjectNumber , int iStaticBoxObject, int iMaterialRefIndex )
{
	AddStaticObjectBox ( iObjectNumber , iStaticBoxObject, iMaterialRefIndex );
}
int ODECreateHingeSingle( int iObjectNumber, float fX, float fY, float fZ, int iType, float fAngMin, float fAngMax )
{
	return CreateHingeSingle( iObjectNumber, fX, fY, fZ, iType, fAngMin, fAngMax );
}
void ODESetHingeLimits( int iConstraint, float fAngMin, float fAngMax, float fSoftness, float fBias, float fRelaxation )
{
	SetHingeLimits(iConstraint, fAngMin, fAngMax, fSoftness, fBias, fRelaxation );
}
void ODESetSliderLimits( int iConstraint, float fLowerLin, float fUpperLin, float fLowerAng, float fUpperAng )
{
	SetSliderLimits( iConstraint, fLowerLin, fUpperLin, fLowerAng, fUpperAng );
}
void ODESetSliderMotor( int iConstraint, bool OnOff, float fForce, float fVelocity )
{
	SetSliderMotor( iConstraint, OnOff, fForce, fVelocity );
}
void ODESetHingeMotor( int iConstraint, float fSpeed, float fAngle, float fForce )
{
	SetHingeMotor( iConstraint, fSpeed, fAngle, fForce );
}
float ODEGetHingeAngle( int iConstraint )
{
	return GetHingeAngle( iConstraint );
}
float ODEGetSliderPosition( int iConstraint )
{
	return GetSliderPosition( iConstraint );
}
int ODECreateHingeDouble( int iObjectNumA, int iObjectNumB, float fXa, float fYa, float fZa, 
	                                                        float fXb, float fYb, float fZb, 
	                                                        int iTypeA, int iTypeB, int noCollision )
{
    return CreateHingeDouble( iObjectNumA, iObjectNumB, fXa, fYa, fZa, fXb, fYb, fZb, iTypeA, iTypeB, noCollision );
}
int ODECreateJointSingle( int iObjectNumber, float fX, float fY, float fZ )
{
	return CreateJointSingle( iObjectNumber, fX, fY, fZ );
}
int ODECreateJointDouble( int iObjectNumA, int iObjectNumB, float fXa, float fYa, float fZa, float fXb, float fYb, float fZb, int noCollision)
{
	return CreateJointDouble( iObjectNumA, iObjectNumB, fXa, fYa, fZa, fXb, fYb, fZb, noCollision );
}
int ODECreateSliderDouble( int iObjectNumA, int iObjectNumB,
	                       float qXa, float qYa, float qZa, float qWa,
	                       float fXa, float fYa, float fZa, 
	                       float qXb, float qYb, float qZb, float qWb,
	                       float fXb, float fYb, float fZb, 
	                       bool useLinearReferenceFrameA )
{
	return CreateSliderDouble( iObjectNumA, iObjectNumB, qXa, qYa, qZa, qWa, fXa, fYa, fZa, qXb, qYb, qZb, qWb, fXb, fYb, fZb, useLinearReferenceFrameA );
}


void ODEDestroyObject ( int iObjectNumber )
{
	//PE: dont touch while simulation is processed.
	physicslock.lock();
	// get reference object
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if (pPhyObject == NULL)
	{
		physicslock.unlock();
		return;
	}
	DestroyThisObject ( pPhyObject );
	ODERemoveObject ( iObjectNumber );
	for ( int i = (int)g_ProjectileContacts.size() - 1; i >= 0; i-- )
	{
		if ( g_ProjectileContacts[i].iObject == iObjectNumber ) g_ProjectileContacts.erase ( g_ProjectileContacts.begin() + i );
	}
	physicslock.unlock();
}

// Does not work at the moment
// The idea is to have a dynamic entity that does not response to gravity or hitting the terrain
// The no gravity works, but without setting collision flags to no response it bounces off terrain
// Not always consistant. No contact response does seem to work, but then nothing collides with the object either 
// (so it gives no response and has no respone either)
void ODESetNoGravity( int iObjectNumber, int iGravity )
{
	#ifdef WICKEDENGINE
	// all objects start with gravity, force off if user wishes
	sObjectList* pPhyObject = ODEFindID (iObjectNumber);
	if (pPhyObject)
	{
		if (iGravity == 0)
		{
			// zero gravity
			btVector3 vecGravReset = btVector3(0, 0, 0);
			pPhyObject->body->setGravity (vecGravReset);
			pPhyObject->body->setFlags (pPhyObject->body->getFlags() & ~BT_DISABLE_WORLD_GRAVITY);
		}
		else
		{
			// default gravity
			pPhyObject->body->setGravity (btVector3(0, -10.0f, 0));
			pPhyObject->body->setFlags (pPhyObject->body->getFlags() | BT_DISABLE_WORLD_GRAVITY);
		}
	}
	#else
	/*sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return;
	if ( iGravity )
	{
		btVector3 acceleration(0,0,0);
		pPhyObject->body->setGravity( acceleration);
		//pPhyObject->body->setLinearVelocity(acceleration);
		//pPhyObject->body->setAngularVelocity(acceleration);
		//pPhyObject->body->setMassProps(0,acceleration);
		pPhyObject->body->setFlags ( BT_DISABLE_WORLD_GRAVITY );
		//COL_OBJECT | COL_OBJECT_DYNAMIC | COL_CAPSULECHAR | 
		pPhyObject->body->setCollisionFlags( btCollisionObject::CF_NO_CONTACT_RESPONSE) ;
		pPhyObject->body->forceActivationState(1);
	}
	else
	{
		pPhyObject->body->setFlags ( !BT_DISABLE_WORLD_GRAVITY );
	}
	*/
	#endif
}

// Ragdoll commands

void DBProReportError ( LPSTR pErr1, LPSTR pErr2 )
{
	MessageBox ( NULL, pErr1, pErr2, MB_OK );
}

void BPhys_RagDollBegin(int objectID, float objTotalWeight)
{
#ifdef WICKEDENGINE
	// Ragdoll rigid bodies are not added to g_PhyObjectList..
	// ...so store their object IDs for the debug drawer to find them later.
	g_debugDrawer.iRagdollID.push_back(objectID);
#endif

	currentDBProRagDoll = new DBProRagDoll(objectID);
	currentDBProRagDoll->SetTotalWeight(objTotalWeight);	
}

int BPhys_RagDollGetIDFromBoneObject(int objectID)
{
	return ragdollManager->GetIDFromBoneObject(objectID);
}

int BPhys_RagDollAddBone(int startLimbID, int endLimbID, float diameter, int collisionGroup, int collisionMask)
{
	if(currentDBProRagDoll != NULL)
	{
		if ( startLimbID != -1 && endLimbID != -1 )
		{
			return currentDBProRagDoll->AddBone(currentDBProRagDoll->GetID(), startLimbID, endLimbID, diameter, collisionGroup, collisionMask);
		}
	}
	else
	{
		DBProReportError("You Must Call BPhys_RagDollBegin() before BPhys_RagDollAddBone","Bullet Physics Wrapper");
	}
	return -1;
} 

int BPhys_RagDollAddBone2(int startLimbID, int endLimbID, float diameter, float lengthmod, int collisionGroup, int collisionMask)
{
	if (startLimbID == -1 || endLimbID == -1) return -1;
	if(currentDBProRagDoll != NULL)
	{
		return currentDBProRagDoll->AddBone(currentDBProRagDoll->GetID(), startLimbID, endLimbID, diameter, lengthmod, collisionGroup, collisionMask);
	}
	else
	{
		DBProReportError("You Must Call BPhys_RagDollBegin() before BPhys_RagDollAddBone","Bullet Physics Wrapper");
	}
	return -1;
} 

void BPhys_RagDollBoneAddLimbID(int boneID, int limbID) 
{
	if (limbID == -1) return;
	if(currentDBProRagDoll != NULL)
	{
		currentDBProRagDoll->AssignLimbIDToBone( boneID, limbID);
	}
	else
	{
		DBProReportError("You Must Call BPhys_RagDollBegin() before BPhys_RagDollAddBone","Bullet Physics Wrapper");
	}
}

void BPhys_RagDollAddHingeJoint(int boneAID, int boneBID, int limbID, int jointRotationVec3, int limitsVec2)
{
	if (boneAID == -1 || boneBID == -1 || limbID == -1) return;
	if(currentDBProRagDoll!=NULL)
	{
		DBProToBullet::AssertValidVector(jointRotationVec3, "Parameter Passed To BPhys_RagDollAddHingeJoint Invalid");
		DBProToBullet::AssertValidVector(limitsVec2, "Parameter Passed To BPhys_RagDollAddHingeJoint Invalid");
		currentDBProRagDoll->AddHingeJoint(boneAID, boneBID, limbID, DBProToBullet::GetVector3(jointRotationVec3), 
																		 btScalar(GetXVector2(limitsVec2)), 
																			btScalar(GetYVector2(limitsVec2)));
	}
	else
	{
		DBProReportError("Can not call BPhys_RagDollAddHingeJoint before BPhys_RagDollBegin", "Bullet Physics Wrapper");
	}
}

void BPhys_RagDollAddTwistJoint(int boneAID, int boneBID, int limbID, int jointRotationVec3, int limitsVec3)
{
	if (limbID == -1 || boneBID == -1 || boneAID == -1) return;
	if(currentDBProRagDoll!=NULL)
	{
		DBProToBullet::AssertValidVector(jointRotationVec3, "Parameter Passed To BPhys_RagDollAddTwistJoint Invalid");
		DBProToBullet::AssertValidVector(limitsVec3, "Parameter Passed To BPhys_RagDollAddTwistJoint Invalid");
		currentDBProRagDoll->AddTwistConstraint(boneAID, boneBID, limbID, DBProToBullet::GetVector3(jointRotationVec3), DBProToBullet::GetVector3(limitsVec3));
	}
	else
	{
		DBProReportError("Can not call BPhys_RagDollAddTwistJoint before BPhys_RagDollBegin", "Bullet Physics Wrapper");
	}
}

int BPhys_RagdollGetBoneObjID(int ragdollID, int boneIndex)
{
	DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
	return ragdoll ? ragdoll->GetBoneObjID(boneIndex) : -1;
}

void BPhys_RagDollEnd()
{
	ragdollManager->AddRagdoll(currentDBProRagDoll);
	currentDBProRagDoll->Finalize();
	currentDBProRagDoll->Activate();
	currentDBProRagDoll = NULL;
}

bool BPhys_RagDollApplyForce(int ragdollID,int iLimbNumber,float fFX,float fFY,float fFZ,float fTX,float fTY,float fTZ,float fForce)
{
	if ( ragdollManager )
	{
		DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
		if ( ragdoll )
		{
			#ifdef WICKEDENGINE
			// halve the force for MAX - more subtle
			fForce /= 2;
			#endif
			ragdoll->Activate();
			ragdoll->ApplyForce(iLimbNumber,btVector3(fFX,fFY,fFZ),btVector3(fTX,fTY,fTZ),fForce);
			return true;
		}
	}
	return false;
}

int BPhys_RagdollExist(int ragdollID)
{
	if ( ragdollManager )
	{
		return ragdollManager->GetRagdoll(ragdollID) != NULL;
	}
	else
		return 0;
}

int BPhys_DeleteRagdoll(int ragdollID)
{
	if ( ragdollManager )
	{
		DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
		if(ragdoll)
		{
			ragdollManager->DeleteRagdoll(ragdollID);
		}
		else
		{
			return 0;
		}
	}
	return 1;
}

// GG: whether a ragdoll is registered for an object (GGPhysicsQuery's awake list)
int BPhys_RagdollExists(int ragdollID)
{
	return ( ragdollManager && ragdollManager->GetRagdoll(ragdollID) ) ? 1 : 0;
}

int BPhys_RagdollIsStatic(int ragdollID)
{
	if ( ragdollManager )
	{
		DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
		if(ragdoll)
		{
			return ragdoll->IsStatic();
		}
	}
	return 0;
}

int BPhys_RagdollSetStatic(int ragdollID, int isStatic)
{
	if ( ragdollManager )
	{
		DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
		if(ragdoll)
		{
			if ( isStatic==0 )
				ragdoll->SetStatic(false);
			else
				ragdoll->SetStatic(true);
		}
		else
		{
			return 0;
		}
	}
	return 1;
}

int BPhys_RagDollHideBones(int ragdollID)
{
	if ( ragdollManager )
	{
		DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
		if(ragdoll)
		{
			ragdoll->HideBones();
		}
		else
		{
			return 0;
		}
	}
	return 1;
}

int BPhys_RagDollShowBones(int ragdollID)
{
	if ( ragdollManager )
	{
		DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(ragdollID);
		if(ragdoll)
		{
			ragdoll->ShowBones();
		}
		else
		{
			return 0;
		}
	}
	return 1;
}

//FHB:Can only call these function before Ragdoll END.
void BPhys_RagDollSetDamping(float linear, float angular)
{
	if(currentDBProRagDoll)
	{
		currentDBProRagDoll->SetDamping(btScalar(linear), btScalar(angular));
	}
}

void BPhys_RagDollSetSleepingThresholds(float linear, float angular)
{
	if(currentDBProRagDoll)
	{
		currentDBProRagDoll->SetSleepingThresholds(btScalar(linear), btScalar(angular));
	}
}

void BPhys_RagDollSetDeactivationTime(float time)
{
	if(currentDBProRagDoll)
	{
		currentDBProRagDoll->SetDeactivationTime(btScalar(time));
	}
}

// GG: the ragdoll body whose capsule moves a limb, -1 when none does
int BPhys_RagDollFindBone(int ragdollID, int limbID)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	return ragdoll ? ragdoll->FindBone(limbID) : -1;
}

// GG: a ragdoll body's centre in world units
bool BPhys_RagDollGetBonePosition(int ragdollID, int boneIndex, float* pX, float* pY, float* pZ)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( !ragdoll || boneIndex < 0 ) return false;
	btVector3 position = ragdoll->GetBonePosition(boneIndex);
	*pX = position.getX();
	*pY = position.getY();
	*pZ = position.getZ();
	return true;
}

// GG: taken as a ragdoll is made, with the angle the character faces, so a hold can turn a body as the character
void BPhys_RagDollSetRestYaw(int ragdollID, float fYaw)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( ragdoll ) ragdoll->SetRestYaw(btScalar(fYaw));
}

// GG: the object's position follows a ragdoll body (-1 stops it)
void BPhys_RagDollSetFollowBone(int ragdollID, int boneIndex)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( ragdoll ) ragdoll->SetFollowBone(boneIndex);
}

// GG: holds a ragdoll body at a point (world units), turned as the character would be by the angles (X, then Y, then Z,
// as for an object: 0, a, 0 is that body as when standing facing a); called each frame while held
bool BPhys_RagDollHold(int ragdollID, int boneIndex, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, unsigned int uNowMs)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( !ragdoll || !g_dynamicsWorld ) return false;
	btMatrix3x3 rotation;
	rotation.setEulerYPR(btRadians(fAZ), btRadians(fAY), btRadians(fAX));
	ragdoll->Hold(boneIndex, btVector3(fX, fY, fZ), rotation, btScalar(fAY), uNowMs);
	return ragdoll->IsHeld();
}

void BPhys_RagDollRelease(int ragdollID)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( ragdoll ) ragdoll->Release();
}

int BPhys_RagDollIsHeld(int ragdollID)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	return ( ragdoll && ragdoll->IsHeld() ) ? 1 : 0;
}

int BPhys_RagDollIsResting(int ragdollID)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	return ( ragdoll && ragdoll->IsResting() ) ? 1 : 0;
}

// GG: how hard a held ragdoll is pulled to its drape (0 to 1), its swing's damping (0 to 1), the folds in degrees and
// the knees' bend (negative: as made)
bool BPhys_RagDollSetHoldStiffness(int ragdollID, float fStiffness, float fDamping, float fWaistFold, float fHipFold, float fKneeFold)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( !ragdoll ) return false;
	ragdoll->SetHoldStiffness(fStiffness, fDamping, fWaistFold, fHipFold, fKneeFold);
	return true;
}

bool BPhys_RagDollGetHoldStiffness(int ragdollID, float* pStiffness, float* pDamping, float* pWaistFold, float* pHipFold, float* pKneeFold)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( !ragdoll ) return false;
	btScalar fStiffness, fDamping, fWaistFold, fHipFold, fKneeFold;
	ragdoll->GetHoldStiffness(&fStiffness, &fDamping, &fWaistFold, &fHipFold, &fKneeFold);
	*pStiffness = fStiffness;
	*pDamping = fDamping;
	*pWaistFold = fWaistFold;
	*pHipFold = fHipFold;
	*pKneeFold = fKneeFold;
	return true;
}

// GG: a held ragdoll's knees, each one's bend in degrees as made and in its drape; how many
int BPhys_RagDollGetHoldKnees(int ragdollID, float* pMade, float* pHeld, int iMax)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( !ragdoll || iMax <= 0 ) return 0;
	btScalar fMade[8], fHeld[8];
	int iCount = ragdoll->GetHoldKnees(fMade, fHeld, btMin(iMax, 8));
	for ( int k = 0; k < iCount; k++ )
	{
		pMade[k] = fMade[k];
		pHeld[k] = fHeld[k];
	}
	return iCount;
}

// GG: the pose a held ragdoll's drape starts from, from each limb's world matrix (16 floats, row by row, world units)
// as the ragdoll was made and in a reference pose; with none, the pose it was made in
bool BPhys_RagDollSetHoldReference(int ragdollID, const float* pMadeWorlds, const float* pReferenceWorlds, int iLimbs)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	if ( !ragdoll ) return false;
	return ragdoll->SetHoldReference(pMadeWorlds, pReferenceWorlds, iLimbs);
}

int BPhys_RagDollHasHoldReference(int ragdollID)
{
	DBProRagDoll* ragdoll = ragdollManager ? ragdollManager->GetRagdoll(ragdollID) : NULL;
	return ( ragdoll && ragdoll->HasHoldReference() ) ? 1 : 0;
}

