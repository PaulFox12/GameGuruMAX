DARKSDK_DLL void MakeObjectFromLimbEx ( int iNewID, int iSrcID, int iLimbID, int iCopyAllFromLimb )
{
	// check the mesh exists
	if ( !ConfirmObjectAndLimbInstance ( iSrcID, iLimbID ) )
		return;

	// attempt to create a new object
	if ( !CreateNewObject ( iNewID, "limbmesh" ) )
		return;

	// make object from mesh contained within object
	sObject* pNewObject = g_ObjectList [ iNewID ];
	sObject* pSrcObject = g_ObjectList [ iSrcID ];
	sObject* pActualSrcObject = pSrcObject;
	if ( pSrcObject->pInstanceOfObject ) pActualSrcObject = pSrcObject->pInstanceOfObject;
	sFrame* pSrcFrame = pActualSrcObject->ppFrameList [ iLimbID ];

	// leefix - 181105 - reset original before copy
	ResetVertexDataInMesh ( pSrcObject );

	// leefix - 181105 - can copy limbs even if no mesh there (copy hierarchy and
	// lee - 030406 - u6rc5 - added new command to make one-limb vs all-limb-hierarchy legacy U59 compat.
	if ( iCopyAllFromLimb==1 )
	{
		// Delete frame and mesh from new-object (start from scratch)
		SAFE_DELETE ( pNewObject->pFrame->pMesh );
		SAFE_DELETE ( pNewObject->pFrame );

		// trace through pFrame (limb) selected and copy all hierarchy from it
		sFrame* pDstFrameRoot = NULL;
		pDstFrameRoot = MakeObjectFromLimbRec ( pSrcFrame, NULL );

		// copy frame reference to new object
		pNewObject->pFrame = pDstFrameRoot;

		// force obj to make framelist
		CreateFrameAndMeshList ( pNewObject );

		// copy meshes of src object to dst object
		DWORD dwMeshCount = pActualSrcObject->iMeshCount;
		pNewObject->iMeshCount = dwMeshCount;
		if ( dwMeshCount>0 )
		{
			pNewObject->ppMeshList = new sMesh* [ dwMeshCount ];
			for ( int m=0; m<(int)dwMeshCount; m++ )
			{
				sMesh* pSrcMesh = pActualSrcObject->ppMeshList [ m ];
				sMesh* pDestMesh = new sMesh;

				// root matrix
				GGMATRIX matWorld;
				GGMatrixIdentity ( &matWorld );

				// mesh copy
				MakeMeshFromOtherMesh ( true, pDestMesh, pSrcMesh, &matWorld );
				pNewObject->ppMeshList [ m ] = pDestMesh;

				// bone data copy
				DWORD dwBoneCount = pSrcMesh->dwBoneCount;
				if ( dwBoneCount>0 )
				{
					DWORD dwNewBone = 0;
					pDestMesh->pBones = new sBone [ dwBoneCount ];
					for ( DWORD b=0; b<dwBoneCount; b++ )
					{
						bool bNeedThisBone = false;
						for ( DWORD l=0; l<(DWORD)pNewObject->iFrameCount; l++ )
							if ( strcmp ( pSrcMesh->pBones [ b ].szName, pNewObject->ppFrameList [ l ]->szName )==NULL )
								{ bNeedThisBone = true; break; }

						if ( bNeedThisBone==true )
						{
							memcpy ( &pDestMesh->pBones [ dwNewBone ], &pSrcMesh->pBones [ b ], sizeof ( sBone ) );
							DWORD dwNumInfluences = pDestMesh->pBones [ dwNewBone ].dwNumInfluences;
							pDestMesh->pBones [ dwNewBone ].pVertices = new DWORD [ dwNumInfluences ];
							pDestMesh->pBones [ dwNewBone ].pWeights = new float [ dwNumInfluences ];
							memcpy ( pDestMesh->pBones [ dwNewBone ].pVertices, pSrcMesh->pBones [ b ].pVertices, sizeof(DWORD)*dwNumInfluences );
							memcpy ( pDestMesh->pBones [ dwNewBone ].pWeights, pSrcMesh->pBones [ b ].pWeights, sizeof(float)*dwNumInfluences );
							dwNewBone=dwNewBone+1;
						}
					}
					pDestMesh->dwBoneCount = dwNewBone;
				}
			}
		}

		// only one main mesh exists for bone animation, so we assign it here
		if ( pNewObject->pFrame && pNewObject->ppMeshList )
		{
			// lee - 270306 - u6b5 - first look for mesh from original frame, if exist
			int iMeshBest = -1;
			sMesh* pMeshUsedByOldFrame = pSrcFrame->pMesh;
			for ( int iM=0; iM<(int)pSrcObject->iMeshCount; iM++ )
				if ( pSrcObject->ppMeshList [ iM ]==pMeshUsedByOldFrame )
					iMeshBest = iM;

			// lee - 270306 - u6b5 - pre-U6b5 code..
			if ( iMeshBest==-1 )
			{
				// make SURE the main bone mesh stolen is the BIGGEST
				DWORD dwVertCountLargest = 0;
				for ( int iM=0; iM<(int)dwMeshCount; iM++ )
				{
					if ( pNewObject->ppMeshList [iM ]->dwVertexCount > dwVertCountLargest )
					{
						dwVertCountLargest = pNewObject->ppMeshList [iM ]->dwVertexCount;
						iMeshBest = iM;
					}
				}
			}

			// assign chosen mesh to frame now
			pNewObject->pFrame->pMesh = pNewObject->ppMeshList [ iMeshBest ];
		}

		// copy all animation keyframe data for frames we have taken (if any)
		sAnimationSet* pOrigAnimSet = pSrcObject->pAnimationSet;
		if ( pOrigAnimSet )
		{
			sAnimation* pOrigAnim = pOrigAnimSet->pAnimation;
			if ( pOrigAnim )
			{
				pNewObject->pAnimationSet = new sAnimationSet;
				memcpy ( pNewObject->pAnimationSet, pOrigAnimSet, sizeof ( sAnimationSet ) );
				pNewObject->pAnimationSet->pAnimation = NULL;
				pNewObject->pAnimationSet->pvecBoundCenter = NULL;
				pNewObject->pAnimationSet->pfBoundRadius = NULL;
				pNewObject->pAnimationSet->pvecBoundMax = NULL;
				pNewObject->pAnimationSet->pvecBoundMin = NULL;
				sAnimation* pWorkAnim = NULL;
				while ( pOrigAnim != NULL )
				{
					// if animation name same as any frame in new object, copy anim from src obj to new obj
					bool bNeedThisAnim = false;
					for ( DWORD l=0; l<(DWORD)pNewObject->iFrameCount; l++ )
						if ( strcmp ( pOrigAnim->szName, pNewObject->ppFrameList [ l ]->szName )==NULL )
							{ bNeedThisAnim = true; break; }

					// add this one?
					if ( bNeedThisAnim==true )
					{
						// new animation 
						sAnimation* pCurrentAnim = new sAnimation;		

						// link new anim with previous item
						if ( pWorkAnim ) pWorkAnim->pNext = pCurrentAnim;

						// assign first animation 
						if ( pNewObject->pAnimationSet->pAnimation==NULL )
							pNewObject->pAnimationSet->pAnimation = pCurrentAnim;

						// work animation
						pWorkAnim = pCurrentAnim;

						// fill with name
						memcpy ( pWorkAnim->szName, pOrigAnim->szName, sizeof(pWorkAnim->szName) );

						// create animation and copy orig data
						pWorkAnim->dwNumMatrixKeys = pOrigAnim->dwNumMatrixKeys;
						pWorkAnim->pMatrixKeys = new sMatrixKey [ pWorkAnim->dwNumMatrixKeys ];
						memcpy ( pWorkAnim->pMatrixKeys, pOrigAnim->pMatrixKeys, sizeof ( sMatrixKey )*pWorkAnim->dwNumMatrixKeys );
					}

					// move to the next sequence
					pOrigAnim = pOrigAnim->pNext;
				}
			}
		}

		// setup new object and introduce to buffers
		pNewObject->iMeshCount *= -1; // negated within function CreateFrameAndMeshList
		pNewObject->iFrameCount *= -1; // negated within function CreateFrameAndMeshList
		SetNewObjectFinalProperties ( iNewID, -1.0f );

		// give the object a default texture
		TextureObject ( iNewID, 0 );

		// leeadd - 181105 - many meshes are possible
		for ( int m=0; m<(int)dwMeshCount; m++ )
			CloneInternalTextures ( pNewObject->ppMeshList [ m ], pActualSrcObject->ppMeshList [ m ] );
	}
	else
	{
		// src and destination
		sMesh* pSrcMesh = pActualSrcObject->ppMeshList [ 0 ];
		if ( pSrcFrame ) if ( pSrcFrame->pMesh ) pSrcMesh = pSrcFrame->pMesh;
		sFrame* pDestFrame = g_ObjectList [ iNewID ]->pFrame;
		sMesh* pDestMesh = pDestFrame->pMesh;

		// just copy one limb mesh from object
		if ( pSrcMesh==NULL )
		{
			// failed
			RunTimeError ( RUNTIMEERROR_LIMBNOTEXIST );
			return;
		}

		// work out the world transform to apply to the captured mesh
		CalculateObjectWorld ( pSrcObject, NULL );
		GGMATRIX matWorld = pSrcFrame->matCombined * pSrcObject->position.matObjectNoTran;

		// create new mesh from existing mesh
		MakeMeshFromOtherMesh ( true, pDestMesh, pSrcMesh, &matWorld );

		// setup new object and introduce to buffers
		SetNewObjectFinalProperties ( iNewID, -1.0f );

		// give the object a default texture
		TextureObject ( iNewID, 0 );

		// leeadd - 240604 - u54 - copy texture from original limb to new object
		if ( pDestMesh && pSrcMesh )
			CloneInternalTextures ( pDestMesh, pSrcMesh );
	}
}

DARKSDK_DLL void MakeObjectFromLimb ( int iNewID, int iSrcID, int iLimbID )
{
	// see above
	MakeObjectFromLimbEx ( iNewID, iSrcID, iLimbID, 0 );
}

void SetObjectLOD ( int iCurrentID, int iLODLevel, float fDistanceOfLOD )
{
	// if object exists
	if ( !ConfirmObjectInstance ( iCurrentID ) )
		return;

	// identify real object (if instance)
	sObject* pObject = g_ObjectList [ iCurrentID ];
	sObject* pRealObject = pObject;
	if ( pObject->pInstanceOfObject ) pRealObject = pObject->pInstanceOfObject;

	// initially scan object limb names for LOD_0 LOD_1 LOD_2
	int iObjLOD0LimbIndex = -1;
	int iObjLOD1LimbIndex = -1;
	int iObjLOD2LimbIndex = -1;
	PerformCheckListForLimbs(pRealObject->dwObjectNumber);
	for(int c=0; c<g_pGlob->checklistqty; c++)
	{
		// standard LOD markers
		if ( strcmp ( g_pGlob->checklist[c].string, "LOD_0" )==NULL ) iObjLOD0LimbIndex = c;
		if ( strcmp ( g_pGlob->checklist[c].string, "LOD_1" )==NULL ) iObjLOD1LimbIndex = c;
		if ( strcmp ( g_pGlob->checklist[c].string, "LOD_2" )==NULL ) iObjLOD2LimbIndex = c;

		// LOD markers produced by X->ASSIMP->OBJ->SIMPLYGON->FBX->BLENDER->X
		if ( strlen(g_pGlob->checklist[c].string) > 5 )
		{
			LPSTR pLODTextPart = g_pGlob->checklist[c].string + strlen(g_pGlob->checklist[c].string) - 5;
			if ( strcmp ( pLODTextPart, "_LOD1" )==NULL ) iObjLOD1LimbIndex = c;
			if ( strcmp ( pLODTextPart, "_LOD2" )==NULL ) iObjLOD2LimbIndex = c;
		}
	}

	// ensue we fill in missing LOD indices based on availability
	if ( iObjLOD1LimbIndex==-1 )
	{
		iObjLOD1LimbIndex = iObjLOD2LimbIndex;
		if ( iObjLOD1LimbIndex==-1 ) iObjLOD1LimbIndex = iObjLOD0LimbIndex;
	}
	if ( iObjLOD2LimbIndex==-1 )
	{
		iObjLOD2LimbIndex = iObjLOD1LimbIndex;
		if ( iObjLOD2LimbIndex==-1 ) iObjLOD2LimbIndex = iObjLOD0LimbIndex;
	}

	// use this command to assign per-object LOD distances
	if ( iLODLevel >= 1 && iLODLevel <= 2 )
		pObject->fLODDistance [ iLODLevel-1 ] = fDistanceOfLOD;

	// find first mesh (as fallback of LOD_X missing)
	int iFirstFrameWithMesh = 0;
	for (; iFirstFrameWithMesh<pRealObject->iFrameCount; iFirstFrameWithMesh++ )
		if ( pRealObject->ppFrameList[iFirstFrameWithMesh]->pMesh )
			break;

	// extra check to ensure mesh is not already covered by LOD1 or LOD2 (sometimes primary mesh has no LOD0 marker)
	for ( int iUniqueLOD0=0; iUniqueLOD0<pRealObject->iFrameCount; iUniqueLOD0++ )
	{
		if ( pRealObject->ppFrameList[iUniqueLOD0]->pMesh )
		{
			if ( iUniqueLOD0 != iObjLOD1LimbIndex && iUniqueLOD0 != iObjLOD2LimbIndex )
			{
				iFirstFrameWithMesh = iUniqueLOD0;
			}
		}
	}

	// special mode to disregard 'AddLODToObject' style mesh LOD, and use limb visibility style
	pObject->iUsingWhichLOD = -1000;
	pObject->bHadLODNeedCamDistance = true;
	pObject->iLOD0LimbIndex = iObjLOD0LimbIndex;
	pObject->iLOD1LimbIndex = iObjLOD1LimbIndex;
	pObject->iLOD2LimbIndex = iObjLOD2LimbIndex;
	if ( pObject->iLOD0LimbIndex==-1 ) pObject->iLOD0LimbIndex = iFirstFrameWithMesh;
	if ( pObject->iLOD1LimbIndex==-1 ) pObject->iLOD1LimbIndex = iFirstFrameWithMesh;
	if ( pObject->iLOD2LimbIndex==-1 ) pObject->iLOD2LimbIndex = iFirstFrameWithMesh;
}

void AddLODToObject ( int iCurrentID, int iLODModelID, int iLODLevel, float fDistanceOfLOD )
{
	// takes all meshes of lodmodel and adds them to a special lod alternative meshes of specified object
	if ( !ConfirmObject ( iCurrentID ) || !ConfirmObject ( iLODModelID ) )
		return;

	// make object from mesh contained within object
	sObject* pObject = g_ObjectList [ iCurrentID ];
	sObject* pLODObject = g_ObjectList [ iLODModelID ];

	// leeadd - 061208 - U71 - limit lod levels (allow 0,1 and since U71 use 2 for last QUAD/DECAL level)
	if ( iLODLevel<0 || iLODLevel>2 )
		return;

	// leefix - 250106 - so important when copying meshes!
	ResetVertexDataInMesh ( pLODObject );

	// flag as a LOD object
	pObject->iUsingWhichLOD = 0;
	pObject->bHadLODNeedCamDistance = true;
	if ( iLODLevel < 2 )
		pObject->fLODDistance [ iLODLevel ] = fDistanceOfLOD;
	else
		pObject->fLODDistanceQUAD = fDistanceOfLOD;

	// leeadd - 061208 - new alpha fade feature of LOD system
	pObject->iUsingOldLOD = -1;
	pObject->fLODTransition = 0.0f;

	// U74 - 120409 - if quad level used, default for object is at QUAD (furthest first)
	if ( iLODLevel==2 )
	{
		// start off as QUAD level, and adjust as required (copied by instance command too)
		pObject->iUsingOldLOD = 3;
		pObject->iUsingWhichLOD = 3;
	}

	// copy meshes of src object to dst object
	DWORD dwLODMeshCount = pLODObject->iMeshCount;
	if ( dwLODMeshCount>0 )
	{
		// go through all meshes in LOD model
		for ( int iLODFrame = 0; iLODFrame < pLODObject->iFrameCount; iLODFrame++ )
		{
			sFrame* pLODFrame = pLODObject->ppFrameList [ iLODFrame ];
			if ( pLODFrame )
			{
				if ( pLODFrame->pMesh )
				{
					// root frame used to scan obj heirarchy to make bone matrix assignments
					sFrame* pRootFrame = pObject->ppFrameList [ 0 ];

					// find this mesh in main object
					for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
					{
						// get frame ptr
						sFrame* pFrame = pObject->ppFrameList [ iFrame ];
						if ( pFrame )
						{
							// leeadd - U71 - where the mesh is going to go
							sMesh* pDestinationLODMesh = NULL;
							if ( iLODLevel < 2 )
								pDestinationLODMesh = pFrame->pLOD [ iLODLevel ];
							else
								pDestinationLODMesh = pFrame->pLODForQUAD;

							if ( pFrame->pMesh && pDestinationLODMesh==NULL )
							{
								// when found match (or proving a final QUAD model)
								if ( _stricmp ( pLODFrame->szName, pFrame->szName )==NULL || iLODLevel==2 )
								{
									// create mesh within main object, copying lod data across
									sMesh* pSrcMesh = pLODFrame->pMesh;
									sMesh* pDestMesh = new sMesh;

									// root matrix
									GGMATRIX matWorld;
									GGMatrixIdentity ( &matWorld );

									// mesh copy
									MakeMeshFromOtherMesh ( true, pDestMesh, pSrcMesh, &matWorld );
									if ( iLODLevel < 2 )
										pFrame->pLOD [ iLODLevel ] = pDestMesh;
									else
										pFrame->pLODForQUAD = pDestMesh;

									// ensure work from original mesh base
									CollectOriginalVertexData ( pDestMesh );

									// copy over settings from main mesh (transparency, etc) - cannot change after LOD in place!
									CopyMeshSettings ( pDestMesh, pSrcMesh );

									// bone data copy
									DWORD dwBoneCount = pSrcMesh->dwBoneCount;
									if ( dwBoneCount>0 )
									{
										pDestMesh->pBones = new sBone [ dwBoneCount ];
										for ( DWORD b=0; b<dwBoneCount; b++ )
										{
											DWORD dwNewBone = b;
											memcpy ( &pDestMesh->pBones [ dwNewBone ], &pSrcMesh->pBones [ b ], sizeof ( sBone ) );
											DWORD dwNumInfluences = pDestMesh->pBones [ dwNewBone ].dwNumInfluences;
											pDestMesh->pBones [ dwNewBone ].pVertices = new DWORD [ dwNumInfluences ];
											pDestMesh->pBones [ dwNewBone ].pWeights = new float [ dwNumInfluences ];
											memcpy ( pDestMesh->pBones [ dwNewBone ].pVertices, pSrcMesh->pBones [ b ].pVertices, sizeof(DWORD)*dwNumInfluences );
											memcpy ( pDestMesh->pBones [ dwNewBone ].pWeights, pSrcMesh->pBones [ b ].pWeights, sizeof(float)*dwNumInfluences );
										}
										pDestMesh->dwBoneCount = dwBoneCount;
									}

									// now map frames to bones
									InitOneMeshFramesToBones ( pDestMesh );
									MapOneMeshFramesToBones ( pDestMesh, pRootFrame );

									// share textures and shaders
									if ( iLODLevel<2 )
									{
										// copy texture from pFrame->pMesh (not srcmesh) as we want a LOD mesh only for geometry
										pDestMesh->dwTextureCount = pFrame->pMesh->dwTextureCount; 
										pDestMesh->pTextures = new sTexture [ pDestMesh->dwTextureCount ]; 
										CloneInternalTextures ( pDestMesh, pFrame->pMesh );

										// leefix - 041208 - U71 - copy over shader influence too
										CloneShaderEffects ( pDestMesh, pFrame->pMesh );

										// add mesh to buffers
										m_ObjectManager.AddObjectMeshToBuffers ( pFrame->pLOD [ iLODLevel ], false );
									}
									else
									{
										// copy over texture from pSrcMesh
										pDestMesh->dwTextureCount = pSrcMesh->dwTextureCount; 
										pDestMesh->pTextures = new sTexture [ pDestMesh->dwTextureCount ]; 
										CloneInternalTextures ( pDestMesh, pSrcMesh );
										CloneShaderEffects ( pDestMesh, pSrcMesh );
										m_ObjectManager.AddObjectMeshToBuffers ( pFrame->pLODForQUAD, false );
									}

									// 220513 - finished adding this LOD to parent object
									iFrame = pObject->iFrameCount;
								}
							}
						}
					}
				}
			}
		}
	}
}

// the spawn probe (G-Entity.cpp): named slices of a probed spawn's time, and calls counted in it
void SpawnProbe_Sub ( const char* pName );
long long SpawnProbe_CallStart ( void );
void SpawnProbe_CallEnd ( const char* pName, long long llStart );

DARKSDK_DLL void CloneObject ( int iDestinationID, int iSourceID, int iCloneSharedData )
{
	// iCloneSharedData modes:
	// 101 = use existing object in destination and JUST CLONE TEXTURES OVER

	// check if dest object not exists
	if ( iCloneSharedData!=101 )
	{
		// except for when dest already exists via mode 101
		if ( !ConfirmNewObject ( iDestinationID ) ) return;
		ID_ALLOCATION ( iDestinationID );
	}

	// check if src object exists
	if ( !ConfirmObjectInstance ( iSourceID ) )
		return;

	// first restore original vertex data in object 
	sObject* pObject = g_ObjectList [ iSourceID ];

	// mike - 011005 - see if the original source object is an instance
	if ( pObject->pInstanceOfObject )
		pObject = pObject->pInstanceOfObject;

	// clone the object
	if ( iCloneSharedData!=101 )
	{
		// create new object in destination
		ResetVertexDataInMesh ( pObject );

		if ( !CloneDBO ( &g_ObjectList [ iDestinationID ], pObject ) )
			return;
		SpawnProbe_Sub("clone: copy");

		// Special clone parameter shares an original object not deleted (good for lots of bone anim objects)
		sObject* pNewObject = g_ObjectList [ iDestinationID ];
		if ( iCloneSharedData!=0 )
		{
			// work on cloned object (cut out potential huge anim data)
			// Update dependency details
			// Do this whether or not there is animation data, to ensure that is predictable
			pObject->dwDependencyCount++;
			pNewObject->pObjectDependency = pObject;

			if ( pNewObject->pAnimationSet )
			{
				sAnimation* pOrigAnim = pObject->pAnimationSet->pAnimation;
				sAnimation* pAnim = pNewObject->pAnimationSet->pAnimation;
				if ( pAnim && pOrigAnim )
				{
					while ( pAnim != NULL )
					{
						// Erase Animation Data (matrix data usually huge)
						SAFE_DELETE(pAnim->pMatrixKeys);
						pAnim->dwNumMatrixKeys=0;

						// substitute with original objects anim data
						pAnim->pSharedReadAnim = pOrigAnim;

						// move to the next sequence
						pOrigAnim = pOrigAnim->pNext;
						pAnim = pAnim->pNext;
					}
				}
			}
		}

		// and put back the latest frame of the source object
		pObject->fAnimLastFrame=-1.0f;
		UpdateObjectAnimation ( pObject );
		SpawnProbe_Sub("clone: source animation");
		// setup new object and introduce to buffers
		SetNewObjectFinalProperties ( iDestinationID, -1.0f );

		// lee - 180406 - u6rc10 - clone all collision data
		pNewObject->collision = g_ObjectList [ iSourceID ]->collision;
	}

	// transfer references within meshes that cannot be DBO cloned (post-setup)
	sObject* pNewObject = g_ObjectList [ iDestinationID ];
	DWORD dwMeshCount = pObject->iMeshCount;
	if ( pNewObject->iMeshCount<(int)dwMeshCount ) dwMeshCount = pNewObject->iMeshCount;
	for ( int iMesh = 0; iMesh < (int)dwMeshCount; iMesh++ )
	{
		sMesh* pMesh = NULL; if ( iMesh<pObject->iMeshCount ) pMesh = pObject->ppMeshList [ iMesh ];
		sMesh* pNewMesh = NULL; if ( iMesh<pNewObject->iMeshCount ) pNewMesh = pNewObject->ppMeshList [ iMesh ];
		if ( pMesh && pNewMesh )
		{
			CloneShaderEffects ( pNewMesh, pMesh );
			#ifdef WICKEDENGINE
			//PE: Copy material
			pNewMesh->mMaterial.Diffuse.r = pMesh->mMaterial.Diffuse.r;
			pNewMesh->mMaterial.Diffuse.g = pMesh->mMaterial.Diffuse.g;
			pNewMesh->mMaterial.Diffuse.b = pMesh->mMaterial.Diffuse.b;
			pNewMesh->mMaterial.Diffuse.a = pMesh->mMaterial.Diffuse.a;
			#endif
		}
	}

	// 010917 - also clone frame exclusion flag
	DWORD dwFrameCount = pNewObject->iFrameCount;
	for ( int iFrame = 0; iFrame < (int)dwFrameCount; iFrame++ )
	{
		sFrame* pFrame = NULL; if ( iFrame<pObject->iFrameCount ) pFrame = pObject->ppFrameList [ iFrame ];
		sFrame* pNewFrame = NULL; if ( iFrame<pNewObject->iFrameCount ) pNewFrame = pNewObject->ppFrameList [ iFrame ];
		pNewFrame->bExcluded = pFrame->bExcluded;
	}

	// 131115 - transfer flags required by clones
	pNewObject->bIgnoreDefAnim = g_ObjectList [ iSourceID ]->bIgnoreDefAnim;

	// 090217 - clone special spine center system
	pNewObject->bUseSpineCenterSystem = g_ObjectList [ iSourceID ]->bUseSpineCenterSystem;
	pNewObject->dwSpineCenterLimbIndex = g_ObjectList [ iSourceID ]->dwSpineCenterLimbIndex;
	pNewObject->fSpineCenterTravelDeltaX = g_ObjectList [ iSourceID ]->fSpineCenterTravelDeltaX;
	pNewObject->fSpineCenterTravelDeltaZ = g_ObjectList [ iSourceID ]->fSpineCenterTravelDeltaZ;

	SpawnProbe_Sub("clone: effects and frames");

	// handle clone ref
	if ( iCloneSharedData!=101 )
	{
		// some shader settings take over vertex control (so reset to original data)
		ResetVertexDataInMesh ( pNewObject );

		// add object id to shortlist
		AddObjectToObjectListRef ( iDestinationID );
	}
	SpawnProbe_Sub("clone: vertex reset");

	// clone textures for all meshes (clone references)
	#ifdef WICKEDENGINE
	//PE: Until we get LOD support , hide lowest LOD frames.
	Wicked_Hide_Lower_Lod_Meshes(iDestinationID);
	//PE: Particle scale bug fix.
	pNewObject->bUseFixedSize = false;
	if (pObject->bUseFixedSize)
	{
		pNewObject->bUseFixedSize = pObject->bUseFixedSize;
		pNewObject->vecFixedSize = pObject->vecFixedSize;
	}

	#else
	if ( g_ObjectList [ iDestinationID ]->ppMeshList )
	{
		for ( int iMesh = 0; iMesh < g_ObjectList [ iDestinationID ]->iMeshCount; iMesh++ )
		{
			// get mesh ptr
			sMesh* pMesh = NULL; if ( iMesh<g_ObjectList [ iDestinationID ]->iMeshCount ) pMesh = g_ObjectList [ iDestinationID ]->ppMeshList [ iMesh ];
			sMesh* pOrigMesh = NULL; if ( iMesh<pObject->iMeshCount ) pOrigMesh = pObject->ppMeshList [ iMesh ];
			if ( pMesh && pOrigMesh )
			{
				// lee - 230206 - handle if multimaterial or regular mesh
				//pMesh->bUseMultiMaterial = pOrigMesh->bUseMultiMaterial;
				DWORD dwMultiMatCount = pOrigMesh->dwMultiMaterialCount;
				if ( pMesh->bUseMultiMaterial==true && pOrigMesh->bUseMultiMaterial==true )
				{
					// Currrent texture if any used
					sTexture* pTexture = NULL;

					// if multimaterial not exist (clone texture only)
					if ( pMesh->pMultiMaterial==NULL )
					{
						// create it now
						pMesh->pMultiMaterial = new sMultiMaterial [ dwMultiMatCount ];
						memset ( pMesh->pMultiMaterial, 0, sizeof(sMultiMaterial) * dwMultiMatCount );
					}

					// Define textures for multi material array
					pMesh->dwMultiMaterialCount = dwMultiMatCount;
					for ( DWORD m=0; m<dwMultiMatCount; m++ )
					{
						// get multimat at index
						sMultiMaterial* pMultiMat = &(pMesh->pMultiMaterial [ m ]);
						sMultiMaterial* pOrigMultiMat = &(pOrigMesh->pMultiMaterial [ m ]);

						// copy references over (clone)
						strcpy ( pMultiMat->pName, pOrigMultiMat->pName );
						pMultiMat->dwIndexCount		= pOrigMultiMat->dwIndexCount;
						pMultiMat->dwIndexStart		= pOrigMultiMat->dwIndexStart;
						pMultiMat->mMaterial		= pOrigMultiMat->mMaterial;
						pMultiMat->pTexturesRef		= pOrigMultiMat->pTexturesRef;
						pMultiMat->pTexturesRefN	= pOrigMultiMat->pTexturesRefN;
						pMultiMat->pTexturesRefS	= pOrigMultiMat->pTexturesRefS;
						pMultiMat->pTexturesRefG	= pOrigMultiMat->pTexturesRefG;
						pMultiMat->pTexturesRefM	= pOrigMultiMat->pTexturesRefM;
					}

					// multimaterial still uses texture)
					CloneInternalTextures ( pMesh, pOrigMesh );
				}
				else
				{
					// if original multimesh but dest regular, copy multi to reg (first material wins)
					if ( pMesh->bUseMultiMaterial==false && pOrigMesh->bUseMultiMaterial==true )
					{
						if ( dwMultiMatCount>=1 )
						{
							//SAFE_DELETE(pMesh->pTextures);//crashes, mem leak, fuind out whY!!
							if ( pMesh->dwTextureCount!=2 )
							{
								pMesh->dwTextureCount = 2;
								pMesh->pTextures = new sTexture [ pMesh->dwTextureCount ];
								memset ( pMesh->pTextures, 0, sizeof(sTexture)*pMesh->dwTextureCount );
							}
							pMesh->pTextures [ 0 ].iImageID  = pOrigMesh->pTextures [ 0 ].iImageID;;
#ifdef WICKEDENGINE
							pMesh->pTextures [ 0 ].pTexturesRefView = pOrigMesh->pMultiMaterial [ 0 ].pTexturesRef;
#endif
							strcpy ( pMesh->pTextures [ 0 ].pName, pOrigMesh->pMultiMaterial [ 0 ].pName );
							pMesh->pTextures [ 0 ].dwBlendMode = GGTOP_MODULATE;
							pMesh->pTextures [ 0 ].dwBlendArg1 = GGTA_TEXTURE;
							pMesh->pTextures [ 0 ].dwBlendArg2 = GGTA_DIFFUSE;
							pMesh->bUseMultiMaterial = false;
							pMesh->fSpecularOverride = 1.0f;
							pMesh->bUsesMaterial = false;

							// also wipe out shader as it will screw up lightmapper
							SetSpecialEffect ( pMesh, NULL );
						}
					}
					else
					{
						// regular mesh clone texture
						CloneInternalTextures ( pMesh, pOrigMesh );
					}
				}
			}
		}
	}
	#endif
	SpawnProbe_Sub("clone: lod and textures");

	if ( iCloneSharedData!=101 )
	{
		// position data of cloned object must match source
		memcpy ( &g_ObjectList [ iDestinationID ]->position, &pObject->position, sizeof ( sPositionData ) );
	}

	// 110416 - copy over matrix mode state (some models use special FBX rendering matrix styles)
	g_ObjectList [ iDestinationID ]->dwApplyOriginalScaling = g_ObjectList [ iSourceID ]->dwApplyOriginalScaling;
}

DARKSDK_DLL void CloneObject ( int iDestinationID, int iSourceID )
{
	CloneObject ( iDestinationID, iSourceID, 0 );
}

DARKSDK_DLL void InstanceObject ( int iDestinationID, int iSourceID )
{
	#ifdef WICKEDENGINE
	// for now, handle instanced objects as cloned objects until
	// we understand how wicked handles its instances
	//PE: This one should be second, the one below (2-) should be more easy to add:
	//PE: InstanceObject - We need to use:
	//PE: 1
	//Entity entity;
	//entity = localScene.Entity_Duplicate(pFrame->wickedobjindex);
	//PE: was thinking if t.entityelement[t.e].eleprof.WEMaterial == t.entityprofile[t.entid].WEMaterial we should be fine.
	//PE: We would have to do this per mesh, setting pFrame->wickedobjindex = entity;
	//PE: and pFrame->pMesh->wickedmeshindex = meshEntity; (need to be retrived from entity). entity should include everything including transforms animation ...

	//PE: 2 - another way perhaps more easy.
	//PE: https://github.com/turanszkij/WickedEngine/blob/master/Content/Documentation/WickedEngine-Documentation.md#instancing
	//PE: When adding the object. (WickedCall_LoadNode)
	//PE: When we set up the "master" objects 50001+ name the mesh proper.
	//PE: So meshEntity = scene.Entity_CreateMesh("node_mesh"); , would be "objnr_meshnr" like "50001_1","50001_2"
	//PE: Then when creating real objects 70001+ , use Findmesh of master object and always use this meshEntity.
	//PE: So: wiScene::MeshComponent& mesh = *scene.meshes.GetComponent(foundMeshEntity);
	//PE: Resuing the mesh is what trigger wicked to use instancing.
	//PE: As i can read, it dont matter what other different transformations, colors, or dithering parameters are used, so should work out of the box.
	//PE:
	//PE: In: SetNewObjectFinalProperties() if t.entobj >= 50001 && t.entobj < 55000 set a flag to produce real mesh names. (Entity_CreateMesh)
	//PE: else if t.obj >= 70000 && t.obj < 90000. set flag to reused mesh from master. (master: pFrame->pMesh->wickedmeshindex )
	//PE: else if t.entobj >= 70000 cursor obj. set flag to reused mesh from master.


	CloneObject(iDestinationID, iSourceID);
	#else
    int iActualSourceID = iSourceID;

	// check if dest object not exists
	if ( !ConfirmNewObject ( iDestinationID ) )
		return;

    // u74b7 - Allow instancing of an instance
    //         Actually instance the original instead.

	// check if src object exists, either as an original or an instance
	if ( !ConfirmObjectInstance ( iSourceID ) )
		return;

    // If src is an instance, use that instances original for mesh data
    // but copy the other details from the instance
    if (g_ObjectList [ iSourceID ]->pInstanceOfObject)
    {
        iActualSourceID = g_ObjectList [ iSourceID ]->pInstanceOfObject->dwObjectNumber;
    }

	// check memory allocation
	ID_ALLOCATION ( iDestinationID );

	// create pure instance of source object
	g_ObjectList [ iDestinationID ] = new sObject;
	g_ObjectList [ iDestinationID ]->pInstanceOfObject = g_ObjectList [ iActualSourceID ];
    g_ObjectList [ iDestinationID ]->pObjectDependency = g_ObjectList [ iActualSourceID ];
    g_ObjectList [ iActualSourceID ]->dwDependencyCount++;

	// U72 - 100109 - flag parent so it knows to animate even if not visible (irreversable)
	g_ObjectList [ iSourceID ]->position.bParentOfInstance = true;

	// lee - 250307 - store object number for reference
	g_ObjectList [ iDestinationID ]->dwObjectNumber = iDestinationID;

	// copy over basic info such as collision (so instances can have instant collision)
	g_ObjectList [ iDestinationID ]->collision = g_ObjectList [ iSourceID ]->collision;

	// 250217 - messed up collision center values, so recalc here for destination
	g_ObjectList [ iDestinationID ]->collision.vecCentre = g_ObjectList [ iDestinationID ]->collision.vecMin + ((g_ObjectList [ iDestinationID ]->collision.vecMax - g_ObjectList [ iDestinationID ]->collision.vecMin)/2.0f); 
	g_ObjectList [ iDestinationID ]->collision.bColCenterUpdated = false;

	// lee - 310306 - u6rc5 - must carry LOD flag info across
	g_ObjectList [ iDestinationID ]->fLODDistance[0] = g_ObjectList [ iSourceID ]->fLODDistance[0];
	g_ObjectList [ iDestinationID ]->fLODDistance[1] = g_ObjectList [ iSourceID ]->fLODDistance[1];
	g_ObjectList [ iDestinationID ]->fLODDistanceQUAD = g_ObjectList [ iSourceID ]->fLODDistanceQUAD;
	g_ObjectList [ iDestinationID ]->bHadLODNeedCamDistance = g_ObjectList [ iSourceID ]->bHadLODNeedCamDistance;

	// U74 - 120409 - copy current state of parent LOD as well (so instancing when as a QUAD, recreate QUAD)
	g_ObjectList [ iDestinationID ]->iUsingOldLOD = g_ObjectList [ iSourceID ]->iUsingOldLOD;
	g_ObjectList [ iDestinationID ]->iUsingWhichLOD = g_ObjectList [ iSourceID ]->iUsingWhichLOD;
	g_ObjectList [ iDestinationID ]->fLODTransition = g_ObjectList [ iSourceID ]->fLODTransition;

	// create limb visibility array
	g_ObjectList [ iDestinationID ]->pInstanceMeshVisible = new bool [ g_ObjectList[iSourceID]->iFrameCount ];
	memset ( g_ObjectList [ iDestinationID ]->pInstanceMeshVisible, 255, g_ObjectList[iSourceID]->iFrameCount * sizeof(bool) );

	// mike - 021005 - retain pivot from source
	g_ObjectList [ iDestinationID ]->position.bApplyPivot = g_ObjectList [ iSourceID ]->position.bApplyPivot;
	g_ObjectList [ iDestinationID ]->position.matPivot = g_ObjectList [ iSourceID ]->position.matPivot;

	// add object id to shortlist
	AddObjectToObjectListRef ( iDestinationID );
	#endif
}

DARKSDK_DLL void MakeObjectSphere ( int iID, float fRadius, int iRings, int iSegments )
{
	// attempt to create a new object
	if ( !CreateNewObject ( iID, "sphere" ) )
		return;

	// DBV1 size=diameter
	fRadius/=2;

	// setup general object data
	sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;
	if ( MakeMeshSphere ( true, pMesh, GGVECTOR3(0,0,0), fRadius, iRings, iSegments, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, GGCOLOR_ARGB(255,255,255,255) )==false )
		return;

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, fRadius );

	// give the object a default texture
	TextureObject ( iID, 0 );
}

DARKSDK_DLL void MakeObjectSphere ( int iID, float fRadius )
{
	MakeObjectSphere ( iID, fRadius, 12, 12 );
}

DARKSDK_DLL void MakeObjectCube ( int iID, float fSize )
{
	MakeObjectBox ( iID, fSize, fSize, fSize );
}

DARKSDK_DLL GlobStruct* GetGlobalStructure ( void )
{
	return g_pGlob;
}

DARKSDK_DLL void GetPositionData ( int iID, sPositionData** ppPosition )
{
	if ( g_ObjectList [ iID ] )
	{
		CalculateObjectWorld ( g_ObjectList [ iID ], NULL );
		*ppPosition = &g_ObjectList [ iID ]->position;
	}
}

//Dave Performance - enables objects to be marked as static (they are set to false by default)
DARKSDK void SetObjectStatic ( int iID , bool isStatic = true )
{
	if ( g_ObjectList [ iID ] )
	{
		g_ObjectList [ iID ]->bIsStatic = isStatic;

		// Update collision centre, because it won't get updated again
		if ( isStatic ) 
			UpdateColCenter ( g_ObjectList [ iID ] );
	}
}

DARKSDK void SetObjectAsCharacter ( int iID, bool mode )
{
	if ( g_ObjectList [ iID ] )
	{
		g_ObjectList [ iID ]->bIsCharacter = mode;
	}
}

std::vector <int> g_vIgnoredObjectList;
extern bool g_ForceTextureListUpdate;

//Dave Performance - enables objects to be marked as ignored and not be in the sorting list
void SetIgnoreObject ( int iID , bool mode )
{
	if ( g_ObjectList [ iID ] )
	{
		g_ObjectList [ iID ]->bIgnored = mode;
		if ( mode ) g_vIgnoredObjectList.push_back ( iID );
	}
}

//Dave Performance - sets no objects to be ignored and resets the list
void ClearIgnoredObjects ( void )
{
	for ( int c = 0 ; c < (int)g_vIgnoredObjectList.size() ; c++ )
	{
		if ( g_ObjectList [ g_vIgnoredObjectList[c] ] )
		{
			g_ObjectList [ g_vIgnoredObjectList[c] ]->bIgnored = false;
		}
	}

	g_vIgnoredObjectList.clear();
}

void ShowIgnoredObjects ( void )
{
	for ( int c = 0 ; c < (int)g_vIgnoredObjectList.size() ; c++ )
	{
		if ( g_ObjectList [ g_vIgnoredObjectList[c] ] )
		{
			g_ObjectList [ g_vIgnoredObjectList[c] ]->bIgnored = false;
			g_ObjectList [ g_vIgnoredObjectList[c] ]->bVisible = true;
		}
	}

	DoTextureListSort();
}

void HideIgnoredObjects ( void )
{
	for ( int c = 0 ; c < (int)g_vIgnoredObjectList.size() ; c++ )
	{
		if ( g_ObjectList [ g_vIgnoredObjectList[c] ] )
		{
			g_ObjectList [ g_vIgnoredObjectList[c] ]->bIgnored = true;
			g_ObjectList [ g_vIgnoredObjectList[c] ]->bVisible = false;
		}
	}

	DoTextureListSort();
}

//Dave Performance - force the texture list sorting to happen, ignoring any objects set to be ignored
void DoTextureListSort ( void )
{
	g_ForceTextureListUpdate = true;
	m_ObjectManager.SortTextureList();
	g_ForceTextureListUpdate = false;
}


DARKSDK_DLL void GetEmitterData ( int iID, BYTE** ppVertices, DWORD* pdwVertexCount, int** ppiDrawCount )
{
	if ( g_ObjectList [ iID ] )
	{
		if ( g_ObjectList [ iID ]->pFrame->pMesh )
		{
			sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;

			*ppVertices     = pMesh->pVertexData;
			*pdwVertexCount = pMesh->dwVertexCount;
			*ppiDrawCount   = &pMesh->iDrawPrimitives;
		}
	}
}

DARKSDK_DLL void UpdateEmitter ( int iID )
{
	if ( g_ObjectList [ iID ] )
	{
		if ( g_ObjectList [ iID ]->pFrame->pMesh )
		{
			g_ObjectList [ iID ]->pFrame->pMesh->bVBRefreshRequired = true;
#ifndef WICKEDENGINE
			g_vRefreshMeshList.push_back ( g_ObjectList [ iID ]->pFrame->pMesh );
#endif
		}
	}
}

DARKSDK_DLL void MakeEmitter ( int iID, int iSize )
{
	MakeObjectPlane ( iID, iSize, iSize );
}

// New construction commands for multiplayer nameplates
DARKSDK int MakeNewObjectPanel	( int iID , int iNumberOfCharacters )
{
	// attempt to create a new object
	if ( !CreateNewObject ( iID, "nameplate" ) )
		return 0;

	sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;

	// create memory
	DWORD dwVertexCount = 6 * iNumberOfCharacters; // store number of vertices
	DWORD dwIndexCount  = 0; // store number of indices
	if ( !SetupMeshFVFData ( pMesh, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_DIFFUSE | GGFVF_TEX1, dwVertexCount, dwIndexCount, false ) )
	{
		RunTimeError ( RUNTIMEERROR_B3DMESHLOADFAILED );
		return 0;
	}

	// setup mesh drawing properties
	pMesh->iPrimitiveType   = GGPT_TRIANGLELIST;
	pMesh->iDrawVertexCount = pMesh->dwVertexCount;
	pMesh->iDrawPrimitives  = iNumberOfCharacters * 2;

	return 1;
}

DARKSDK void SetObjectPanelQuad	( int iID, int index, float fX, float fY, float fWidth, float fHeight, float fU1, float fV1, float fU2, float fV2, int r , int g , int b )
{
	index *= 6;

	// DB compatability
	fWidth  /= 2.0f;
	fHeight /= 2.0f;
	fX /= 2.0f;
	fY /= 2.0f;

	float pos_pos_x = fX + fWidth;
	float neg_pos_x = fX;
	float pos_pos_y = fY + fHeight;
	float neg_pos_y = fY;

	sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;

	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  index + 0,  neg_pos_x,  pos_pos_y, 0.0f,  0.0f,  0.0f,  -1.0f, GGCOLOR_ARGB ( 255, r, g, b ), fU1, fV1 );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  index + 1,   pos_pos_x,  pos_pos_y, 0.0f,  0.0f,  0.0f,  -1.0f, GGCOLOR_ARGB ( 255, r, g, b ), fU2, fV1 );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  index + 2,  pos_pos_x, neg_pos_y, 0.0f,  0.0f,  0.0f,  -1.0f, GGCOLOR_ARGB ( 255, r, g, b ), fU2, fV2 );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  index + 3,   pos_pos_x,  neg_pos_y, 0.0f,  0.0f,  0.0f, -1.0f, GGCOLOR_ARGB ( 255, r, g, b ), fU2, fV2 );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  index + 4,   neg_pos_x, neg_pos_y, 0.0f,  0.0f,  0.0f,  -1.0f, GGCOLOR_ARGB ( 255, r, g, b ), fU1, fV2 );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  index + 5,   neg_pos_x, pos_pos_y, 0.0f,  0.0f,  0.0f,  -1.0f, GGCOLOR_ARGB ( 255, r, g, b ), fU1, fV1 );

}

DARKSDK void FinishObjectPanel	( int iID, float fWidth, float fHeight )
{
	// DB compatability
	fWidth  /= 2.0f;
	fHeight /= 2.0f;

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, (fWidth+fHeight)/2 );

	// give the object a default texture
	TextureObject ( iID, 0 );
}

DARKSDK_DLL void MakeObjectPlane ( int iID, float fWidth, float fHeight, int iFlag , bool legacymode)
{
	#ifdef WICKEDENGINE
	// wicked engine requires an index buffer at all times!
	int iRotateFlag = iFlag; //PE: We should only y rotate if needed. (particles ... )
	iFlag = 2;
	#endif

	// attempt to create a new object
	if ( !CreateNewObject ( iID, "plane" ) )
		return;

	// DB compatability
	fWidth  /= 2.0f;
	fHeight /= 2.0f;

	// create box mesh for object
	sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;

#ifdef PRODUCTCLASSIC
	MakeMeshPlain(true, pMesh, fWidth, fHeight, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, GGCOLOR_ARGB(255, 255, 255, 255));
#else
	if(legacymode)
		MakeMeshPlain(true, pMesh, fWidth, fHeight, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, GGCOLOR_ARGB(255, 255, 255, 255));
	else
		MakeMeshPlainEx ( true, pMesh, fWidth, fHeight, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, GGCOLOR_ARGB (255,255,255,255) );
#endif

	// if flag is 2, create index buffer for this plane
	if (iFlag == 2)
	{
		//// make temp terrain nicer to look at
		//ScaleTexture(pMesh, 200, 200);

		// for when planes require an index buffer
		if (pMesh->dwIndexCount == 0)
		{
			// create some indices right now, wicked uses them!
			pMesh->dwIndexCount = pMesh->dwVertexCount;
			pMesh->pIndices = new WORD[pMesh->dwIndexCount];
			for (int i = 0; i < pMesh->dwIndexCount; i++)
				pMesh->pIndices[i] = i;
		}
	}

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, (fWidth+fHeight)/2 );
	
	// this is not called in regular MakeObjectPlane(id,w,h), i.e. when flag==0
#ifdef WICKEDENGINE
	if (iRotateFlag > 0)
	{
		// set object Y=180 for compatibility with correct plain object
		YRotateObject(iID, 180);
	}
#else

#ifdef PRODUCTCLASSIC
	//PE: Strange Classic always rotated this.
	YRotateObject(iID, 180);
#else
	if (iFlag > 0)
	{
		// set object Y=180 for compatibility with correct plain object
		YRotateObject(iID, 180);
	}
#endif
#endif

	// box collision for box shapes
	SetColToBoxes ( g_ObjectList [ iID ] );

	// give the object a default texture
	TextureObject ( iID, 0 );

	// special settings
	SetObjectCull ( iID, false );
}

DARKSDK_DLL void MakeObjectPlane ( int iID, float fWidth, float fHeight )
{
	MakeObjectPlane(iID, fWidth, fHeight, 0 , false);
}

DARKSDK_DLL void MakeObjectTriangle ( int iID, float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3 )
{
	// attempt to create a new object
	if ( !CreateNewObject ( iID, "triangle" ) )
		return;

	// setup general object data
	sMesh* pMesh         = g_ObjectList [ iID ]->pFrame->pMesh;	// get a pointer to the mesh ( easier to access now )

	// create vertex memory
	DWORD dwVertexCount = 3;									// store number of vertices
	DWORD dwIndexCount  = 0;									// store number of indices
	if ( !SetupMeshFVFData ( pMesh, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, dwVertexCount, dwIndexCount, false ) )
	{
		RunTimeError ( RUNTIMEERROR_B3DMESHLOADFAILED );
		return;
	}
	
	// create vertices for plane
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  0,  x1, y1, z1,  0.0f,  0.0f, -1.0f, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 0.0f, 0.0f );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  1,  x2, y2, z2,  0.0f,  0.0f, -1.0f, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 1.0f, 0.0f );
	SetupStandardVertex ( pMesh->dwFVF, pMesh->pVertexData,  2,  x3, y3, z3,  0.0f,  0.0f, -1.0f, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 1.0f, 1.0f );

	// setup mesh drawing properties
	pMesh->iPrimitiveType   = GGPT_TRIANGLELIST;
	pMesh->iDrawVertexCount = pMesh->dwVertexCount;
	pMesh->iDrawPrimitives  = 1;

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, x2-x1 );

	// box collision for box shapes
	SetColToBoxes ( g_ObjectList [ iID ] );

	// give the object a default texture
	TextureObject ( iID, 0 );

	// special settings
	SetObjectCull ( iID, false );
}

DARKSDK_DLL void MakeObjectBox ( int iID, float fWidth, float fHeight, float fDepth )
{
	// attempt to create a new object
	if ( !CreateNewObject ( iID, "box" ) )
		return;

	// first off divide the size by 2 to keep compatibility with DB
	fWidth  /= 2.0f;
	fHeight /= 2.0f;
	fDepth  /= 2.0f;

	// create box mesh for object
	sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;
	MakeMeshBox ( true, pMesh, -fWidth, -fHeight, -fDepth, fWidth, fHeight, fDepth, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, GGCOLOR_ARGB (255,255,255,255) );

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, (fWidth+fHeight)/2 );

	// box collision for box shapes
	SetColToBoxes ( g_ObjectList [ iID ] );

	// give the object a default texture
	TextureObject ( iID, 0 );
}

DARKSDK_DLL void MakeObjectPyramid ( int iID, float fSize )
{
	// attempt to create a new object
	if ( !CreateNewObject ( iID, "pyramid" ) )
		return;

	// first off divide the size by 2
	fSize  /= 2.0f;

	// create mesh for object
	sMesh* pMesh = g_ObjectList [ iID ]->pFrame->pMesh;
// lee - 150306 - u60b3 - added DIFFUSE so that latest DX can handle the undeclared format of this model (aniso)
//	MakeMeshPyramid ( true, pMesh, fSize, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, GGCOLOR_ARGB (255,255,255,255) );
	MakeMeshPyramid ( true, pMesh, fSize, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_DIFFUSE | GGFVF_TEX1, GGCOLOR_ARGB (255,255,255,255) );

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, fSize );

	// box collision for box shapes
	SetColToBoxes ( g_ObjectList [ iID ] );

	// give the object a default texture
	TextureObject ( iID, 0 );
}

DARKSDK_DLL void MakeObjectCylinder ( int iID, float fSize )
{
	// attempt to create a new object
	if ( !CreateNewObject ( iID, "cylinder" ) )
		return;

	float fHeight   = fSize;
	float fRadius   = fSize / 2;
	int   iSegments = 30;

	// setup general object data
	sMesh* pMesh         = g_ObjectList [ iID ]->pFrame->pMesh;		// get a pointer to the mesh ( easier to access now )

	// create vrtex memory
	DWORD dwVertexCount = ( iSegments + 1 ) * 2;					// store number of vertices
	DWORD dwIndexCount  = 0;										// store number of indices
	if ( !SetupMeshFVFData ( pMesh, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, dwVertexCount, dwIndexCount, false ) )
	{
		RunTimeError ( RUNTIMEERROR_B3DMESHLOADFAILED );
		return;
	}

	float fDeltaSegAngle = ( 2.0f * GG_PI / iSegments );
	float fSegmentLength = 1.0f / ( float ) iSegments;
	int	  iVertex        = 0;

	// create the sides triangle strip
	for ( int iCurrentSegment = 0; iCurrentSegment <= iSegments; iCurrentSegment++ )
	{
		float x0 = fRadius * sinf ( iCurrentSegment * fDeltaSegAngle );
		float z0 = fRadius * cosf ( iCurrentSegment * fDeltaSegAngle );

		// Calculate normal
		GGVECTOR3 Normal = GGVECTOR3 ( x0, 0.0f, z0 );
		GGVec3Normalize ( &Normal, &Normal ); 

		// set vertex A
		SetupStandardVertex ( pMesh->dwFVF,	pMesh->pVertexData,  iVertex, x0, 0.0f+(fHeight/2.0f), z0, Normal.x, Normal.y, Normal.z, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 1.0f - ( fSegmentLength * ( float ) iCurrentSegment ), 0.0f );

		// increment vertex index
		iVertex++;

		// set vertex B
		SetupStandardVertex ( pMesh->dwFVF,	pMesh->pVertexData,  iVertex, x0, 0.0f-(fHeight/2.0f), z0, Normal.x, Normal.y, Normal.z, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 1.0f - ( fSegmentLength * ( float ) iCurrentSegment ), 1.0f );

		// increment vertex index
		iVertex++;
	}

	// setup mesh drawing properties
	pMesh->iPrimitiveType   = GGPT_TRIANGLESTRIP;
	pMesh->iDrawVertexCount = pMesh->dwVertexCount;
	pMesh->iDrawPrimitives  = pMesh->dwVertexCount - 2;

	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, fSize );

	// box collision for box shapes
	SetColToBoxes ( g_ObjectList [ iID ] );

	// give the object a default texture
	TextureObject ( iID, 0 );
}

DARKSDK_DLL void MakeObjectCone ( int iID, float fSize )
{
	// make a cone

	// attempt to create a new object
	if ( !CreateNewObject ( iID, "cone" ) )
		return;

	float fHeight   = fSize;
	int   iSegments = 11;

	// correct cone size
	fSize/=2.0f;
	
	// setup general object data
	sMesh* pMesh         = g_ObjectList [ iID ]->pFrame->pMesh;		// get a pointer to the mesh ( easier to access now )

	// create vrtex memory
	DWORD dwVertexCount = (iSegments * 2) + 1;						// store number of vertices
	DWORD dwIndexCount  = iSegments * 3;							// store number of indices
	if ( !SetupMeshFVFData ( pMesh, GGFVF_XYZ | GGFVF_NORMAL | GGFVF_TEX1, dwVertexCount, dwIndexCount, false ) )
	{
		RunTimeError ( RUNTIMEERROR_B3DMESHLOADFAILED );
		return;
	}

	float fDeltaSegAngle = ( (2.0f * GG_PI) / iSegments );
	float fSegmentLength = 1.0f / ( float ) iSegments;
	float fy0            = ( 90.0f - ( float ) GGToDegree ( atan ( fHeight / fSize ) ) ) / 90.0f;
	int	  iVertex        = 0;
	int	  iIndex         = 0;
	WORD  wVertexIndex   = 0;

	// for each segment, add a triangle to the sides triangle list
	for ( int iCurrentSegment = 0; iCurrentSegment <= iSegments; iCurrentSegment++ )
	{
		float x0 = fSize * sinf ( iCurrentSegment * fDeltaSegAngle );
		float z0 = fSize * cosf ( iCurrentSegment * fDeltaSegAngle );

		// Calculate normal
		GGVECTOR3 Normal = GGVECTOR3 ( x0, fy0, z0 );
		GGVec3Normalize ( &Normal, &Normal ); 

		// not the last segment though
		if ( iCurrentSegment < iSegments )
		{
			// set vertex A
			SetupStandardVertex ( pMesh->dwFVF,	pMesh->pVertexData,  iVertex, 0.0f, 0.0f+(fHeight/2.0f), 0.0f, Normal.x, Normal.y, Normal.z, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 1.0f - ( fSegmentLength * ( float ) iCurrentSegment ), 0.0f );

			// increment vertex index
			iVertex++;

			// set vertex B
			SetupStandardVertex ( pMesh->dwFVF,	pMesh->pVertexData,  iVertex, x0, 0.0f-(fHeight/2.0f), z0, Normal.x, Normal.y, Normal.z, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 1.0f - ( fSegmentLength * ( float ) iCurrentSegment ), 1.0f );

			// increment vertex index
			iVertex++;
		}
		else
		{
			// set last vertex
			SetupStandardVertex ( pMesh->dwFVF,	pMesh->pVertexData,  iVertex, x0, 0.0f - ( fHeight / 2.0f ), z0, Normal.x, Normal.y, Normal.z, GGCOLOR_ARGB ( 255, 255, 255, 255 ), 0.0f, 1.0f );

			// increment vertex index
			iVertex++;
		}

		// not the last segment though
		if ( iCurrentSegment < iSegments )
		{
			// set three indices per segment
			pMesh->pIndices [ iIndex ] = wVertexIndex;
			iIndex++;
			wVertexIndex++;
			
			pMesh->pIndices [ iIndex ] = wVertexIndex;
			iIndex++;

			if ( iCurrentSegment == iSegments-1 )
				wVertexIndex += 1;
			else
				wVertexIndex += 2;
			
			pMesh->pIndices [ iIndex ] = wVertexIndex;
			iIndex++;
			wVertexIndex--;	
		}
	}

	// setup mesh drawing properties
	pMesh->iPrimitiveType   = GGPT_TRIANGLELIST;
	pMesh->iDrawVertexCount = ( iSegments * 2 ) + 1;
	pMesh->iDrawPrimitives  = iSegments;
	
	// setup new object and introduce to buffers
	SetNewObjectFinalProperties ( iID, fSize );

	// give the object a default texture
	TextureObject ( iID, 0 );
}

// Animation Commands

DARKSDK_DLL void AppendObject ( LPSTR pString, int iID, int iFrame )
{
	//PE: Support Decrypt/Encrypt when adding animations. (standalone).
	char VirtualFilename[_MAX_PATH];
	strcpy(VirtualFilename, pString);

	// store current folder (typically mode dir)
	char pStoreCurrentDir[_MAX_PATH];
	GetCurrentDirectory(_MAX_PATH, pStoreCurrentDir);

	// determine if loading an encrypted model file
	bool bTempFolderChangeForEncrypt = CheckForWorkshopFile(VirtualFilename);

	// get path of original model file passed in
	char pPathToOriginalFile[_MAX_PATH];
	strcpy(pPathToOriginalFile, "");
	if (strlen(VirtualFilename) > 0)
	{
		// get relative path from current
		strcpy(pPathToOriginalFile, VirtualFilename);
		for (DWORD n = strlen(pPathToOriginalFile) - 1; n > 0; n--)
		{
			if (pPathToOriginalFile[n] == '\\' || pPathToOriginalFile[n] == '/' || (unsigned char)(pPathToOriginalFile[n]) < 32)
			{
				pPathToOriginalFile[n] = 0;
				break;
			}
		}
	}

	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object ptr
	sObject* pObject = g_ObjectList [ iID ];
	if ( pObject->pAnimationSet==NULL )
	{
		//RunTimeError(RUNTIMEERROR_B3DKEYFRAMENOTEXIST); soft fail!
		return;
	}
	else
	{
		// Append animation from file to model
		if (g_pGlob->Decrypt) g_pGlob->Decrypt(VirtualFilename);
		if (!AppendAnimationFromFile (pObject, (LPSTR)VirtualFilename, iFrame))
		{
			if (g_pGlob->Encrypt) g_pGlob->Encrypt(VirtualFilename);
			RunTimeError(RUNTIMEERROR_B3DOBJECTAPPENDFAILED);
			return;
		}
		if (g_pGlob->Encrypt) g_pGlob->Encrypt(VirtualFilename);
	}
}

DARKSDK_DLL void PlayObject ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];

	pObject->bAnimLooping = false;
	#ifdef WICKEDENGINE
	WickedCall_PlayObject(pObject,0.0f,-1,false);
	pObject->fAnimLastFrame=-1.0f;
	#else
	pObject->bAnimPlaying = true;
	pObject->fAnimFrame = 0.0f;
	#endif
}

DARKSDK_DLL void PlayObject ( int iID, int iStart )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];

	pObject->bAnimLooping = false;
	#ifdef WICKEDENGINE
	WickedCall_PlayObject(pObject,(float)iStart,-1,false);
	pObject->fAnimLastFrame=-1.0f;
	#else
	pObject->bAnimPlaying = true;
	pObject->fAnimFrame = (float)iStart;
	if ( pObject->fAnimFrame > pObject->fAnimFrameEnd )
		pObject->fAnimFrameEnd=pObject->fAnimTotalFrames;
	#endif
}

DARKSDK_DLL void PlayObject ( int iID, int iStart, int iEnd )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];

	pObject->bAnimLooping = false;
	#ifdef WICKEDENGINE
	WickedCall_PlayObject(pObject,(float)iStart,(float)iEnd,false);
	pObject->fAnimLastFrame=-1.0f;
	pObject->fAnimFrameEnd = (float)iEnd;
	#else
	pObject->bAnimPlaying = true;
	pObject->fAnimFrame = (float)iStart;
	pObject->fAnimFrameEnd = (float)iEnd;
	#endif
}

DARKSDK_DLL void LoopObject ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];

	#ifdef WICKEDENGINE
	WickedCall_PlayObject(pObject, 0.0f, -1, true);
	pObject->fAnimLastFrame=-1.0f;
	#else
	pObject->bAnimPlaying = true;
	pObject->bAnimLooping = true;
	pObject->fAnimFrame = 0.0f;
	#endif
}

DARKSDK_DLL void LoopObject ( int iID, int iStart )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];
	pObject->bAnimLooping = true;
	#ifdef WICKEDENGINE
	WickedCall_PlayObject(pObject, (float)iStart, -1, true);
	pObject->fAnimLastFrame=-1.0f;
	pObject->fAnimLoopStart = (float)iStart;
	#else
	pObject->fAnimLastFrame = -1.0f;
	pObject->fAnimLoopStart = (float)iStart;
	pObject->bAnimPlaying = true;
	pObject->fAnimLoopStart = (float)iStart;
	if ( pObject->fAnimFrame < pObject->fAnimLoopStart )
		pObject->fAnimFrame = pObject->fAnimLoopStart;
	#endif
}

DARKSDK_DLL void LoopObject ( int iID, int iStart, int iEnd )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];

	pObject->bAnimLooping = true;
	#ifdef WICKEDENGINE
	WickedCall_PlayObject(pObject, (float)iStart, (float)iEnd, true);
	pObject->fAnimLastFrame=-1.0f;
	pObject->fAnimLoopStart = (float)iStart;
	pObject->fAnimFrameEnd = (float)iEnd;
	#else
	pObject->fAnimLastFrame = -1.0f;
	pObject->fAnimLoopStart = (float)iStart;
	pObject->fAnimFrameEnd = (float)iEnd;
	pObject->bAnimPlaying = true;
	if ( pObject->fAnimFrame < pObject->fAnimLoopStart )
		pObject->fAnimFrame = pObject->fAnimLoopStart;
	if ( pObject->fAnimFrame > pObject->fAnimFrameEnd )
		pObject->fAnimFrame = pObject->fAnimLoopStart;
	#endif
}

DARKSDK_DLL void StopObject(int iID)
{
	// check the object exists
	if (!ConfirmObject(iID))
		return;

	// control animation
	sObject* pObject = g_ObjectList[iID];

	pObject->bAnimLooping = false;
	#ifdef WICKEDENGINE
	WickedCall_StopObject(pObject);
	pObject->fAnimLastFrame = -1;
	#else
	pObject->bAnimPlaying = false;
	#endif
}

DARKSDK_DLL void SetObjectSpeed ( int iID, int iSpeed )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];

	#ifdef WICKEDENGINE
	float fSpeed = (float)iSpeed / 100.0f;
	fSpeed *= 0.5f; // also removed timeelapsed from engine code (relying on Wicked anim system time delta only), but need to compensate
	WickedCall_SetObjectSpeed(pObject, fSpeed);
	#endif
	pObject->fAnimSpeed = (float)iSpeed / 100.0f;
}

DARKSDK_DLL void SetObjectDefAnim ( int iID, int iSkipDefFrameInIntersectAll )
{
	if ( !ConfirmObject ( iID ) ) return;
	sObject* pObject = g_ObjectList [ iID ];
	if ( iSkipDefFrameInIntersectAll==1 )
		pObject->bIgnoreDefAnim = true;
	else
		pObject->bIgnoreDefAnim = false;
}

DARKSDK_DLL void SetObjectFrameEx ( int iID, float fFrame, int iRecalculateBounds, int iChangeNoStop )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object ptr
	sObject* pObject = g_ObjectList [ iID ];

	#ifdef WICKEDENGINE
	// can improve this to slerp using the animation lerp inside wicked at some point!
	if(iChangeNoStop==1)
		WickedCall_SetObjectFrameEx(pObject, fFrame);
	else
		WickedCall_SetObjectFrame(pObject, fFrame);
	#else
	// control animation
	if ( pObject->fAnimInterp==1.0f || pObject->bAnimPlaying )
	{
		// direct frame set if animating or not manually slerping
		pObject->fAnimFrame = fFrame;
	}
	else
	{
		// use manual slerp for frame interpolation
		pObject->bAnimManualSlerp = true;
		pObject->fAnimSlerpStartFrame = pObject->fAnimFrame;
		pObject->fAnimSlerpEndFrame = fFrame;
		pObject->fAnimSlerpTime = 0.0f;
	}

	// ensure object is updated
	pObject->bAnimUpdateOnce = true;

	// leeadd - 211008 - u71 - whether frame zero shifts overall object bounds
	DWORD dwDoesNotShiftFrameZero = 0;

	// leeadd - 070305 - if recalc, perform update and recalc bounds
	if ( iRecalculateBounds==1 )
	{
		// triggers bounds to be recalculated
		pObject->bUpdateOnlyCurrentFrameBounds = true;
		pObject->bUpdateOverallBounds = true;
	}

	// leefix - 260806 - u63 - replaced else which forced a bounds recalc from the SET FRAME obj,frm command..
	if ( iRecalculateBounds==2 )
	{
		// can reverse the setting
		if ( pObject->bUpdateOnlyCurrentFrameBounds )
		{
			pObject->bUpdateOnlyCurrentFrameBounds = false;
			pObject->bUpdateOverallBounds = true;
		}
	}

	// leeadd - 211008 - u71 - recalc, but flag frame zero so it does not shift overall object bounds
	if ( iRecalculateBounds==3 )
	{
		// triggers bounds to be recalculated
		pObject->bUpdateOnlyCurrentFrameBounds = false;
		pObject->bUpdateOverallBounds = true;
		dwDoesNotShiftFrameZero = 1;
	}

	// flag frame to NOT shift object bounds
	if ( pObject->ppFrameList )
	{
		if ( pObject->ppFrameList [ 0 ] )
		{
			if ( dwDoesNotShiftFrameZero==1 )
				pObject->ppFrameList [ 0 ]->dwStatusBits |= 1; // bit 1
			else
				pObject->ppFrameList [ 0 ]->dwStatusBits &= 1; // bit 1
		}
	}
	#endif
}

DARKSDK_DLL void ChangeObjectFrame (int iID, float fFrame)
{
	// by default it does not recalculate bounds as it is slow
	int iChangeNoStop = 1;
	SetObjectFrameEx (iID, fFrame, 0, iChangeNoStop);
}

DARKSDK_DLL void SetObjectFrameEx ( int iID, float fFrame )
{
	// by default it does not recalculate bounds as it is slow
	SetObjectFrameEx ( iID, fFrame, 0, 0 );
}

DARKSDK_DLL void SetObjectFrame ( int iID, int iFrame )
{
	SetObjectFrameEx ( iID, (float)iFrame );
}

DARKSDK_DLL void SetObjectFrameEx ( int iID, int iLimbID, float fFrame, int iEnableOverride )
{
	// check the object exists (and limb)
	if ( !ConfirmObjectAndLimb ( iID, iLimbID ) )
		return;

	// get object ptr
	sObject* pObject = g_ObjectList [ iID ];

	// can set an animation frame override per limb
	if ( pObject->pfAnimLimbFrame==NULL )
	{
		// must allocate it if not currently created
		pObject->pfAnimLimbFrame = new float [ pObject->iFrameCount ];
		for ( int iN=0; iN<pObject->iFrameCount; iN++ )
			pObject->pfAnimLimbFrame [ iN ] = -1.0f;
	}

	// set the per limb animation frame
	switch ( iEnableOverride )
	{
		case 0 : pObject->pfAnimLimbFrame [ iLimbID ] = -1.0f;	break; // normal
		case 1 : pObject->pfAnimLimbFrame [ iLimbID ] = fFrame;	break; // override with new frame
		case 2 : pObject->pfAnimLimbFrame [ iLimbID ] = -2.0f;	break; // disable all animation (manual angles only)
	}

	// ensure object is updated
	pObject->bAnimUpdateOnce = true;
}

DARKSDK_DLL void SetObjectInterpolation ( int iID, float fInterp )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// control animation
	sObject* pObject = g_ObjectList [ iID ];
	pObject->fAnimInterp = fInterp / 100.0f;

	#ifdef WICKEDENGINE
	// transitions handled differently with MAX, we control a lerp factor that handles transitions
	// nicely within the Wicked animation system
	WickedCall_SetAnimationLerpFactor(pObject);
	#endif

	// leefix - 210303 - to switch off interpolation
	if ( (int)fInterp == 100 ) pObject->bAnimManualSlerp=false;
}

DARKSDK_DLL void SaveObjectAnimation ( int iID, LPSTR pFilename )
{
	// Not Implemented in DBPRO V1 RELEASE
	RunTimeError(RUNTIMEERROR_COMMANDNOWOBSOLETE);
}

DARKSDK_DLL void AppendObjectAnimation ( int iID, LPSTR pFilename )
{
	// Not Implemented in DBPRO V1 RELEASE
	RunTimeError(RUNTIMEERROR_COMMANDNOWOBSOLETE);
}

DARKSDK_DLL void SetObjectAnimationMode ( int iID, int iMode )
{
	// Not Implemented in DBPRO V1 RELEASE
	RunTimeError(RUNTIMEERROR_COMMANDNOWOBSOLETE);
}

// Visual Commands

DARKSDK_DLL void AddVisibilityListMask ( int iID )
{
	#ifdef WICKEDENGINE
	#else
	m_ObjectManager.m_dwSkipVisibilityListMask |= (1<<iID);
	#endif
}

DARKSDK_DLL void HideObject ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// set object to visibility
	sObject* pObject = g_ObjectList [ iID ];
	pObject->bVisible = false;

	#ifdef WICKEDENGINE
	WickedCall_SetObjectVisible(pObject,false);
	#endif
}

DARKSDK_DLL void ShowObject ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// set object to visibility
	sObject* pObject = g_ObjectList [ iID ];
	pObject->bVisible = true;

	#ifdef WICKEDENGINE
	WickedCall_SetObjectVisible(pObject,true);
	#endif
}

DARKSDK_DLL void SetObjectRotationXYZ ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// object rotation setting
	sObject* pObject = g_ObjectList [ iID ];
	pObject->position.dwRotationOrder = ROTORDER_XYZ;
}

DARKSDK_DLL void SetObjectRotationZYX ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// object rotation setting
	sObject* pObject = g_ObjectList [ iID ];
	pObject->position.dwRotationOrder = ROTORDER_ZYX;
}

DARKSDK_DLL void SetObjectRotationZXY ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// object rotation setting
	sObject* pObject = g_ObjectList [ iID ];
	pObject->position.dwRotationOrder = ROTORDER_ZXY;
}

/*
DARKSDK_DLL void GhostObjectOn ( int iID, int iFlag )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	if ( pObject->pInstanceOfObject==NULL )
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
			SetGhost ( pObject->ppMeshList [ iMesh ], true, iFlag );

	// promote to overlay layer
	pObject->bGhostedObject = true;

	UpdateOverlayFlag ( pObject );
}

DARKSDK_DLL void GhostObjectOn ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// use higher function
	GhostObjectOn ( iID, -1 );
}

DARKSDK_DLL void GhostObjectOff ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetGhost ( pObject->ppMeshList [ iMesh ], false, -1 );

	// promote to overlay layer
	pObject->bGhostedObject = false;
	UpdateOverlayFlag ( pObject );
}
*/

DARKSDK_DLL void FadeObject ( int iID, float fPercentage )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// knock down perc
	fPercentage/=100.0f;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetDiffuse ( pObject->ppMeshList [ iMesh ], fPercentage );
}

void GlueObjectToLimbEx ( int iSource, int iTarget, int iLimbID, int iMode )
{
	// iMode greater than 1000 are all MODE 3 (specifying actual object to sync anim to)
	
	// check the object exists
	if ( !ConfirmObject ( iSource ) )
		return;

	// leefix - 100304 - check the object exists (and limb)
	if ( !ConfirmObjectAndLimb ( iTarget, iLimbID ) )
		return;

	// get object pointers
	sObject* pSourceObject = g_ObjectList [ iSource ];

	#ifdef WICKEDENGINE
	if (pSourceObject->position.bGlued == true)
	{
		UnGlueObject(iSource);
	}
	#endif

	// assign target limb to override source position data
	pSourceObject->position.bGlued			= true;
	pSourceObject->position.iGluedToObj		= iTarget;
	sObject* pTargetObject = g_ObjectList[iTarget];
	if (pTargetObject) pTargetObject->position.iBeenGluedToBy = iSource;

	// glue mode
	if ( iMode==1 )
	{
		// mode 1 - wipe out frame orient, leaving position only (avoid hierarchy frame problems)
		pSourceObject->position.iGluedToMesh	= iLimbID * -1;
	}
	else
	{
		if ( iMode==2 )
		{
			// mode 2 - wipe out child object position for accurate limb location placement
			pSourceObject->position.iGluedToMesh	= iLimbID;

			// leeadd - 150306 - u60b3 - reset position
			pSourceObject->position.vecPosition = GGVECTOR3 ( 0, 0, 0 );
		}
		else
		{
			// mode 0 or mode 4 and other modes - regular glue object to a limb (default behaviour)
			pSourceObject->position.iGluedToMesh	= iLimbID;
		}
	}

	// wicked has its own way to glue objects
	if (pSourceObject->position.bGlued == true)
	{
		sObject* pParentObject = GetObjectData(iTarget);
		if (iMode > 1000)
		{
			// mode 3 - is mode 0 plus ability to sync with object animation glued to
			int iModeAsObjID = iMode;
			WickedCall_GlueObjectToObject(pSourceObject, pParentObject, iLimbID, iModeAsObjID, 0);
		}
		else
		{
			int iWorldToLocal = 0;
			if (iMode == 4) iWorldToLocal = 1;
			WickedCall_GlueObjectToObject(pSourceObject, pParentObject, iLimbID, -1, iWorldToLocal);
		}
	}
}

DARKSDK_DLL void GlueObjectToLimb ( int iSource, int iTarget, int iLimbID )
{
	GlueObjectToLimbEx ( iSource, iTarget, iLimbID, 0 );
}

