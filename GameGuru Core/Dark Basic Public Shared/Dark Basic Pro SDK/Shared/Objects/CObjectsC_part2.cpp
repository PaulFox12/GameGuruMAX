DARKSDK_DLL void UnGlueObject ( int iSource )
{
	// check the object exists
	if ( !ConfirmObject ( iSource ) )
		return;

	// get object pointer
	sObject* pSourceObject = g_ObjectList [ iSource ];

	// get target object pointer
	int iTarget = pSourceObject->position.iGluedToObj;
	int iLimbID = abs ( pSourceObject->position.iGluedToMesh );
	if ( iTarget > 0 )
	{
		sObject* pTargetObject = g_ObjectList [ iTarget ];
		if ( pTargetObject )
		{
			// set new position of source to target
			if ( iLimbID < pTargetObject->iFrameCount )
			{
				pSourceObject->position.vecPosition.x	= pTargetObject->ppFrameList [ iLimbID ]->matAbsoluteWorld._41;
				pSourceObject->position.vecPosition.y	= pTargetObject->ppFrameList [ iLimbID ]->matAbsoluteWorld._42;
				pSourceObject->position.vecPosition.z	= pTargetObject->ppFrameList [ iLimbID ]->matAbsoluteWorld._43;
			}
			
			// wipe out glue assignment
			sObject* pTargetObject = g_ObjectList[pSourceObject->position.iGluedToObj];
			if (pTargetObject) pTargetObject->position.iBeenGluedToBy = 0;
			pSourceObject->position.bGlued			= false;
			pSourceObject->position.iGluedToObj		= 0;
			pSourceObject->position.iGluedToMesh	= 0;
		}
	}

	#ifdef WICKEDENGINE
	// wicked has its own way to glue objects
	if (pSourceObject->position.bGlued == false)
	{
		WickedCall_UnGlueObjectToObject(pSourceObject);
	}
	#endif
}

DARKSDK_DLL void UnGlueAllObjects ( void )
{
	// check the object exists
	for ( int iSource = 0; iSource < g_iObjectListCount; iSource++ )
	{
		if ( !ConfirmObject ( iSource ) )
			return;

		// get object pointer
		sObject* pSourceObject = g_ObjectList [ iSource ];

		// get target object pointer
		int iTarget = pSourceObject->position.iGluedToObj;
		if ( iTarget > 0 )
		{
			sObject* pTargetObject = g_ObjectList [ iTarget ];
			if ( pTargetObject )
			{
				// set new position of source to target
				int iFrameIndex = abs ( pSourceObject->position.iGluedToMesh );
				
				CalcObjectWorld ( pSourceObject );
				CalcObjectWorld ( pTargetObject );

				DWORD x = LimbDirectionX ( iSource, 0 );
				DWORD y = LimbDirectionY ( iSource, 0 );
				DWORD z = LimbDirectionZ ( iSource, 0 );

				float fX = *( float* ) &x;
				float fY = *( float* ) &y;
				float fZ = *( float* ) &z;

				RotateObject ( iSource, fX, fY, fZ );

				pSourceObject->position.vecPosition.x = pSourceObject->position.matWorld._41;
				pSourceObject->position.vecPosition.y = pSourceObject->position.matWorld._42;
				pSourceObject->position.vecPosition.z = pSourceObject->position.matWorld._43;

				// wipe out glue assignment
				sObject* pTargetObject = g_ObjectList[pSourceObject->position.iGluedToObj];
				if (pTargetObject) pTargetObject->position.iBeenGluedToBy = 0;
				pSourceObject->position.bGlued			= false;
				pSourceObject->position.iGluedToObj		= 0;
				pSourceObject->position.iGluedToMesh	= 0;
			}
		}
	}
}

DARKSDK_DLL void LockObjectOn ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// promote to overlay layer
	pObject->bLockedObject = true;
	UpdateOverlayFlag ( pObject );
}

DARKSDK_DLL void LockObjectOff ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// promote to overlay layer
	pObject->bLockedObject = false;
	UpdateOverlayFlag ( pObject );
}

DARKSDK_DLL void DisableObjectZDepthEx	( int iID, int iKeepUpdateStage )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// promote to overlay layer
	if ( iKeepUpdateStage == 1 )
	{
		// only want to ensure object in natural update stage does not write to depth
		sObject* pActualObject = pObject; if ( pActualObject->pInstanceOfObject ) pActualObject = pActualObject->pInstanceOfObject;
		for ( int iMeshIndex = 0; iMeshIndex < pActualObject->iMeshCount; iMeshIndex++ )
		{
			sMesh* pMesh = pActualObject->ppMeshList[iMeshIndex];
			if ( pMesh ) pMesh->bZWrite = false;
		}
	}
	else
	{
		// default which pushes render to new update stage
		pObject->bNewZLayerObject = true;
		UpdateOverlayFlag ( pObject );
	}
}

DARKSDK_DLL void DisableObjectZDepth ( int iID )
{
	DisableObjectZDepthEx ( iID, 0 );
}

DARKSDK_DLL void EnableObjectZDepth ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// promote to overlay layer
	pObject->bNewZLayerObject = false;
	UpdateOverlayFlag ( pObject );
}

DARKSDK_DLL void DisableObjectZRead ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetZRead ( pObject->ppMeshList [ iMesh ], false );
}

DARKSDK_DLL void EnableObjectZRead ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetZRead ( pObject->ppMeshList [ iMesh ], true );
}

DARKSDK_DLL void DisableObjectZWriteEx ( int iID, bool bProtectState )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
	{
		SetZWrite(pObject->ppMeshList[iMesh], false);
		pObject->ppMeshList[iMesh]->bProtectZWriteState = bProtectState;
	}
}

DARKSDK_DLL void DisableObjectZWrite(int iID)
{
	DisableObjectZWriteEx(iID,false);
}

DARKSDK_DLL void DisableLimbZWrite ( int iID, int iLimbIndex )
{
	// check the object exists
	if ( !ConfirmObjectAndLimb ( iID, iLimbIndex ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	sFrame* pFrame = pObject->ppFrameList[iLimbIndex];
	if ( pFrame )
		if ( pFrame->pMesh ) 
			SetZWrite ( pFrame->pMesh, false );
}

DARKSDK_DLL void EnableObjectZWrite ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for (int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++)
		SetZWrite(pObject->ppMeshList[iMesh], true);
}

DARKSDK_DLL void DisableObjectZBias ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetZBias ( pObject->ppMeshList [ iMesh ], false, 0.0f, 0.0f );
}

DARKSDK_DLL void EnableObjectZBias ( int iID, float fSlopeScale, float fDepth )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetZBias ( pObject->ppMeshList [ iMesh ], true, fSlopeScale, fDepth );
}

DARKSDK_DLL void ReverseObjectFrames ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	if ( pObject ) 
	{
		int iFrameCount = pObject->iFrameCount;
		sFrame** pTempList = new sFrame*[iFrameCount];
		if ( pTempList )
		{
			for ( int iFrame=0; iFrame<iFrameCount; iFrame++ )
			{
				pTempList[iFrame] = pObject->ppFrameList[iFrame];
			}
			for ( int iFrame=0; iFrame<iFrameCount; iFrame++ )
			{
				pObject->ppFrameList[iFrameCount-1-iFrame] = pTempList[iFrame];
			}
			delete pTempList;
		}
	}
}

DARKSDK_DLL void SetObjectFOV ( int iID, float fFOV )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to object
	sObject* pObject = g_ObjectList [ iID ];
	pObject->fFOV = fFOV;
}

DARKSDK_DLL void FixObjectPivot ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// update to latest rotation
	UpdateObjectRotation ( pObject );

	// rotation to apply
	GGMATRIX matRotation;
	if ( pObject->position.bFreeFlightRotation )
		matRotation = pObject->position.matFreeFlightRotate;
	else
		matRotation = pObject->position.matRotation;

	// copy rotation to pivot and activate
	if ( pObject->position.bApplyPivot==false )
	{
		// first pivot capture
		pObject->position.bApplyPivot = true;
		pObject->position.matPivot = matRotation;
	}
	else
	{
		// compounded pivot capture
		pObject->position.matPivot = matRotation * pObject->position.matPivot;
	}

	// reset rotation
	pObject->position.bFreeFlightRotation = false;
	pObject->position.vecRotate = GGVECTOR3 ( 0.0f, 0.0f, 0.0f );
	GGMatrixIdentity ( &pObject->position.matRotation );

	// regenerate look vectors
	RegenerateLookVectors( pObject );

	#ifdef WICKEDENGINE
	WickedCall_UpdateObject(pObject);
	#endif
}

void ResetObjectPivot ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// reset pivot and anim update flags
	pObject->position.bApplyPivot = false;
	pObject->bAnimUpdateOnce = true;

	// also reset user matrix
	for ( int iLimbID=0; iLimbID<pObject->iFrameCount; iLimbID++ )
	{
		GGMatrixIdentity ( &pObject->ppFrameList[iLimbID]->matUserMatrix );
	}

	WickedCall_UpdateObject(pObject);
}

void SetToObjectOrientationEx ( int iID, int iWhichID, int iLimbID, int iMode )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// and possible limb
	if ( iLimbID!=-1 )
		if ( !ConfirmObjectAndLimb ( iID, iLimbID ) )
			return;

	// check the object exists
	if ( !ConfirmObject ( iWhichID ) )
		return;

	// get object pointers
	sObject* pObject = g_ObjectList [ iID ];
	sObject* pWhichObject = g_ObjectList [ iWhichID ];

	// copy data from 'which-object'
	if ( iLimbID!=-1 )
	{
		// FROM LIMB
		sFrame* pWhichFrame = pWhichObject->ppFrameList [ iLimbID ];
		if ( pWhichFrame )
		{
			pObject->position.bFreeFlightRotation	= true;
			pObject->position.matFreeFlightRotate	= pWhichFrame->matCombined;
		}
	}
	else
	{
		// FROM OBJECT
		pObject->position.bFreeFlightRotation	= pWhichObject->position.bFreeFlightRotation;
		pObject->position.matFreeFlightRotate	= pWhichObject->position.matFreeFlightRotate;
		pObject->position.dwRotationOrder		= pWhichObject->position.dwRotationOrder;
		pObject->position.vecRotate				= pWhichObject->position.vecRotate;
		pObject->position.matRotation			= pWhichObject->position.matRotation;
		pObject->position.matRotateX			= pWhichObject->position.matRotateX;
		pObject->position.matRotateY			= pWhichObject->position.matRotateY;
		pObject->position.matRotateZ			= pWhichObject->position.matRotateZ;
		
		// mike - 011005 - use or leave pivot
		if ( iMode == 1 )
		{
			pObject->position.bApplyPivot		= pWhichObject->position.bApplyPivot;
			pObject->position.matPivot			= pWhichObject->position.matPivot;
		}

		pObject->position.vecLook				= pWhichObject->position.vecLook;
		pObject->position.vecUp					= pWhichObject->position.vecUp;
		pObject->position.vecRight				= pWhichObject->position.vecRight;
	}

	#ifdef WICKEDENGINE
	WickedCall_UpdateObject(pObject);
	#endif
}

DARKSDK_DLL void SetObjectToObjectOrientation ( int iID, int iWhichID )
{
	SetToObjectOrientationEx ( iID, iWhichID, -1, 0 );
}

DARKSDK_DLL void SetObjectToObjectOrientation ( int iID, int iWhichID, int iMode )
{
	SetToObjectOrientationEx ( iID, iWhichID, -1, iMode );
}

DARKSDK_DLL void SetObjectToCameraOrientation ( int iID )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];

	// get camera pointer
	tagCameraData* m_Camera_Ptr = (tagCameraData*)GetCameraInternalData ( g_pGlob->dwCurrentSetCameraID );

	// copy data from camera zero
	pObject->position.vecLook				= m_Camera_Ptr->vecLook;
	pObject->position.vecUp					= m_Camera_Ptr->vecUp;
	pObject->position.vecRight				= m_Camera_Ptr->vecRight;
	pObject->position.bFreeFlightRotation	= m_Camera_Ptr->bUseFreeFlightRotation;

    // camera and object free flights are inverse of each other
    // pObject->position.matFreeFlightRotate = m_Camera_Ptr->matFreeFlightRotate;
	FLOAT fDeterminant;
	GGMatrixInverse ( &pObject->position.matFreeFlightRotate, &fDeterminant, &m_Camera_Ptr->matFreeFlightRotate );

	pObject->position.vecRotate.x			= m_Camera_Ptr->fXRotate;
	pObject->position.vecRotate.y			= m_Camera_Ptr->fYRotate;
	pObject->position.vecRotate.z			= m_Camera_Ptr->fZRotate;
	if ( m_Camera_Ptr->bRotate )
		pObject->position.dwRotationOrder		= ROTORDER_XYZ;
	else
		pObject->position.dwRotationOrder		= ROTORDER_ZYX;

	// update object with new rotation
}

// Texture commands

DARKSDK_DLL void TextureObjectRef ( int iID, LPGGSHADERRESOURCEVIEW pTextureRef, float fClipU, float fClipV )
{
	// check the object exists
	g_pGlob->dwInternalFunctionCode=12001;
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
	{
		sMesh* pMesh = pObject->ppMeshList [ iMesh ];
		SetBaseTextureStageRef ( pMesh, 0, pTextureRef );

		// get the offset map for the FVF
		sOffsetMap offsetMap;
		GetFVFOffsetMap ( pMesh, &offsetMap );

		// change the UV offsets
		*( ( float* ) pMesh->pVertexData + offsetMap.dwTU[0] + ( offsetMap.dwSize * 0 ) ) = fClipU;
		*( ( float* ) pMesh->pVertexData + offsetMap.dwTU[0] + ( offsetMap.dwSize * 2 ) ) = fClipU;
		*( ( float* ) pMesh->pVertexData + offsetMap.dwTV[0] + ( offsetMap.dwSize * 2 ) ) = fClipV;
		*( ( float* ) pMesh->pVertexData + offsetMap.dwTV[0] + ( offsetMap.dwSize * 4 ) ) = fClipV;
		*( ( float* ) pMesh->pVertexData + offsetMap.dwTU[0] + ( offsetMap.dwSize * 5 ) ) = fClipU;
		*( ( float* ) pMesh->pVertexData + offsetMap.dwTV[0] + ( offsetMap.dwSize * 5 ) ) = fClipV;

		// flag mesh for a VB update
		pMesh->bVBRefreshRequired=true;
#ifndef WICKEDENGINE
		g_vRefreshMeshList.push_back ( pMesh );
#endif
	}
}

DARKSDK_DLL void TextureObject ( int iID, int iImage )
{
	// check the object exists
	g_pGlob->dwInternalFunctionCode=12001;
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetBaseTexture ( pObject->ppMeshList [ iMesh ], -1, iImage );
	
	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
	g_pGlob->dwInternalFunctionCode=12002;

	#ifdef WICKEDENGINE
	// added this for objects that update their texture in-game (may have duplication calls now when texturing objects in setup!)
	WickedCall_TextureObject(pObject, NULL);
	#endif
}

DARKSDK_DLL void TextureObject ( int iID, int iStage, int iImage )
{
	SetObjectTextureStageEx ( iID, iStage, iImage, 0 );
}

DARKSDK_DLL void SetObjectTextureStageEx ( int iID, int iStage, int iImage, int iDoNotSortTextures )
{
	// check the object exists
	g_pGlob->dwInternalFunctionCode=11001;
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	g_pGlob->dwInternalFunctionCode=11011;
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetBaseTextureStage ( pObject->ppMeshList [ iMesh ], iStage, iImage );
	
	// trigger a ew-new
	g_pGlob->dwInternalFunctionCode=11022;
	m_ObjectManager.RenewReplacedMeshes ( pObject );

	// res-sort textures only if flagged
	if ( iDoNotSortTextures==0 ) m_ObjectManager.UpdateTextures ( );
	g_pGlob->dwInternalFunctionCode=11023;

	#ifdef WICKEDENGINE
	// added this for objects that update their texture in-game (may have duplication calls now when texturing objects in setup!)
	WickedCall_TextureObject(pObject, NULL);
	#endif
}

DARKSDK_DLL void ScrollObjectTexture ( int iID, int iStage, float fU, float fV )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		ScrollTexture ( pObject->ppMeshList [ iMesh ], iStage, fU, fV );

	// lee - 130206 - update 'original' data to reflect this UV change
	UpdateVertexDataInMesh ( pObject );
}

DARKSDK_DLL void ScrollObjectTexture ( int iID, float fU, float fV )
{
	// refers to core function above
	ScrollObjectTexture ( iID, 0, fU, fV );
}

DARKSDK_DLL void ScaleObjectTexture ( int iID, int iStage, float fU, float fV )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// if object has effect applied, skip UV vertex write and pass into vars for later use when setting effect constant
	sObject* pObject = g_ObjectList [ iID ];
	if ( pObject==NULL ) return;

	cSpecialEffect* pEff = NULL;
	if ( pObject->ppMeshList[0] ) pEff = pObject->ppMeshList[0]->pVertexShaderEffect;
	if ( pEff!=NULL )
	{
		// apply to all meshes
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];
			if ( pMesh )
			{
				pMesh->fUVScalingU = fU;
				pMesh->fUVScalingV = fV;
			}
		}
	}
	else
	{
		// apply to all meshes
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
			ScaleTexture ( pObject->ppMeshList [ iMesh ], iStage, fU, fV );

		// lee - 130206 - update 'original' data to reflect this UV change
		UpdateVertexDataInMesh ( pObject );
	}
}

DARKSDK_DLL void ScaleObjectTexture ( int iID, float fU, float fV )
{
	// refers to core function above
	ScaleObjectTexture ( iID, 0, fU, fV );
}

DARKSDK_DLL void SetObjectUVManually ( int iObjID, int iFrameIndex, float fWidth, float fHeight )
{
	// set UV data manually for now, but see about replacing with Wicked GPU Particles for speed and efficiency!
	float U_f = 0;
	float V_f = 0;
	float USize_f = 1.0f / (float) fWidth;
	float VSize_f = 1.0f / (float) fHeight;

	if ( iFrameIndex != 0 )
	{
		//PE: U_f dont work here if we have large animations like 20x20
		//int across = int(iFrameIndex/fWidth);
		//V_f = VSize_f* across;
		//U_f = iFrameIndex * USize_f;

		//PE: Calculate the row and column of the sprite in the atlas
		int row = iFrameIndex / fWidth;
		int col = iFrameIndex % (int)fWidth;
		//PE: Calculate the UV of the topleft corner
		U_f = col * USize_f;
		V_f = row * VSize_f;
	}

	LockVertexDataForLimb ( iObjID, 0 );
	SetVertexDataUV(0, U_f, V_f);
	SetVertexDataUV(1, U_f + USize_f, V_f);
	SetVertexDataUV(2, U_f + USize_f, V_f + VSize_f);
	SetVertexDataUV(3, U_f + USize_f, V_f + VSize_f);
	SetVertexDataUV(4, U_f, V_f + VSize_f);
	SetVertexDataUV(5, U_f, V_f);
	UnlockVertexData();

	#ifdef WICKEDENGINE
	sObject* pObject = GetObjectData(iObjID);
	if (pObject->ppMeshList)
	{
		sMesh* pMesh = pObject->ppMeshList[0];
		if (pMesh) WickedCall_UpdateMeshVertexData(pMesh);
	}
	#endif
}

DARKSDK_DLL void SetObjectSmoothing ( int iID, float fPercentage )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// limit percentage range
	if ( fPercentage<0.0f ) fPercentage=0.0f;
	if ( fPercentage>100.0f ) fPercentage=100.0f;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SmoothNormals ( pObject->ppMeshList [ iMesh ], fPercentage/100.0f );
}

DARKSDK_DLL void SetObjectNormalsEx ( int iID, int iMode )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		GenerateNormals ( pObject->ppMeshList [ iMesh ], iMode );

	// lee - 130206 - update 'original' data to reflect this UV change
	UpdateVertexDataInMesh ( pObject );
}

DARKSDK_DLL void SetObjectNormals ( int iID )
{
	SetObjectNormalsEx ( iID, 0 );
}

DARKSDK_DLL void SetObjectTextureModeStage ( int iID, int iStage, int iMode, int iMipGeneration )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply setting to all meshes (for one stage only)
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetTextureMode ( pObject->ppMeshList [ iMesh ], iStage, iMode, iMipGeneration );
}

DARKSDK_DLL void SetObjectTextureMode ( int iID, int iMode, int iMipGeneration )
{
	// iMode (AddressU and AddressV)
	// D3DTADDRESS_WRAP = 1
    // D3DTADDRESS_MIRROR = 2
    // GGTADDRESS_CLAMP = 3
    // D3DTADDRESS_BORDER = 4
    // D3DTADDRESS_MIRRORONCE = 5
	// iMipGeneration (MipStage)
	// GGTEXF_NONE = 0
	// D3DTEXF_POINT = 1
	// GGTEXF_LINEAR = 2
	// D3DTEXF_ANISOTROPIC = 3
	// D3DTEXF_PYRAMIDALQUAD = 6
	// D3DTEXF_GAUSSIANQUAD = 7
	SetObjectTextureModeStage ( iID, 0, iMode, iMipGeneration );
}

DARKSDK_DLL void SetObjectLightMap ( int iID, int iImage, int iAddDIffuseToStageZero )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object ptr
	sObject* pObject = g_ObjectList [ iID ];

	// added for the benefit of FPSXC104RC7 (used for darklight replacement to built-in lightmapper)
	if ( iAddDIffuseToStageZero==0 )
	{
		// regular light mapping blend
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
			SetMultiTexture ( pObject->ppMeshList [ iMesh ], 1, GGTOP_MODULATE, 3, iImage );
	}
	else
	{
		// the idea is we want DIFFUSE+lightmap in stage zero, and texture on stage two
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];
			if ( pMesh->dwTextureCount>=2 )
			{
				// switch them 
				pMesh->pTextures [ 0 ].dwBlendMode		= GGTOP_ADD;
				pMesh->pTextures [ 0 ].dwBlendArg1		= GGTA_TEXTURE;
				pMesh->pTextures [ 0 ].dwBlendArg2		= GGTA_DIFFUSE;
				pMesh->pTextures [ 1 ].dwBlendMode		= GGTOP_MODULATE;
				pMesh->pTextures [ 1 ].dwBlendArg1		= GGTA_TEXTURE;
				pMesh->pTextures [ 1 ].dwBlendArg2		= GGTA_CURRENT;
			}
		}
	}

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}

DARKSDK_DLL void SetObjectLightMap ( int iID, int iImage )
{
	SetObjectLightMap ( iID, iImage, 0 );
}

DARKSDK_DLL void SetObjectSphereMap ( int iID, int iSphereImage )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetMultiTexture ( pObject->ppMeshList [ iMesh ], 1, GGTOP_MODULATE, 1, iSphereImage );

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}

DARKSDK_DLL void SetObjectCubeMapStage ( int iID, int iStage, int i1, int i2, int i3, int i4, int i5, int i6 )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// generate cube map 
	LPGGCUBETEXTURE pCubeTexture = NULL;
	pCubeTexture = CreateNewImageCubeMap ( i1, i2, i3, i4, i5, i6 );

	// These six images are sources for a cubemap, so if the images are used as
	// camera render targets, we can use a cube face instead

	SetCubeFace ( i1, pCubeTexture, 0 );
	SetCubeFace ( i2, pCubeTexture, 1 );
	SetCubeFace ( i3, pCubeTexture, 2 );
	SetCubeFace ( i4, pCubeTexture, 3 );
	SetCubeFace ( i5, pCubeTexture, 4 );
	SetCubeFace ( i6, pCubeTexture, 5 );

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
	{
		// mesh ptr
		sMesh* pMesh = pObject->ppMeshList [ iMesh ];

		// apply cube map reference to mesh
		SetCubeTexture ( pMesh, iStage, pCubeTexture );

		// U75 - 120809 - also, if currently using shader, ensure this dynamic cube map is allowed in manager
		if ( pMesh->pVertexShaderEffect )
		{
			DWORD dwCorrectBitForThisStage = 1 << iStage;
			pMesh->pVertexShaderEffect->m_dwUseDynamicTextureMask = pMesh->pVertexShaderEffect->m_dwUseDynamicTextureMask | dwCorrectBitForThisStage;
		}
	}

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}

DARKSDK_DLL void SetObjectCubeMap ( int iID, int i1, int i2, int i3, int i4, int i5, int i6 )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// Cube is on stage one
	int iStage = 1;

	// generate cube map 
	LPGGCUBETEXTURE pCubeTexture = NULL;
	pCubeTexture = CreateNewImageCubeMap ( i1, i2, i3, i4, i5, i6 );

	// These six images are sources for a cubemap, so if the images are used as
	// camera render targets, we can use a cube face instead

	SetCubeFace ( i1, pCubeTexture, 0 );
	SetCubeFace ( i2, pCubeTexture, 1 );
	SetCubeFace ( i3, pCubeTexture, 2 );
	SetCubeFace ( i4, pCubeTexture, 3 );
	SetCubeFace ( i5, pCubeTexture, 4 );
	SetCubeFace ( i6, pCubeTexture, 5 );

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
	{
		// mesh ptr
		sMesh* pMesh = pObject->ppMeshList [ iMesh ];

		// apply cube map reference to mesh
		SetCubeTexture ( pMesh, iStage, pCubeTexture );

		// U75 - 120809 - also, if currently using shader, ensure this dynamic cube map is allowed in manager
		if ( pMesh->pVertexShaderEffect )
		{
			DWORD dwCorrectBitForThisStage = 1 << iStage;
			pMesh->pVertexShaderEffect->m_dwUseDynamicTextureMask = pMesh->pVertexShaderEffect->m_dwUseDynamicTextureMask | dwCorrectBitForThisStage;
		}
	}

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}


DARKSDK_DLL void SetObjectDetailMap ( int iID, int iImage )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetMultiTexture ( pObject->ppMeshList [ iMesh ], 1, GGTOP_ADD, 3, iImage );

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}

DARKSDK_DLL void SetObjectBlendMap ( int iID, int iLimbID, int iStage, int iImage, int iTexCoordMode, int iMode, int iA, int iB, int iC, int iR )
{
	// BLEND MODE PARAMS
	// iStage 0-7
	// iImage Index
	// iTexCoordMode
	// 0 - Regular UV Stage Match
	// 1 - Sphere Mapping UV Data
	// 2 - Cube Mapping UV Data
	// 3 - Steal UV Data From Stage Zero
	// 10-17 - Take UV Data From Stage.. (10=0,11-1,etc)
	// iBlendMode
	// GGTOP_DISABLE = 1,
	// GGTOP_SELECTARG1 = 2,
	// D3DTOP_SELECTARG2 = 3,
	// GGTOP_MODULATE = 4,
	// GGTOP_MODULATE2X = 5,
	// GGTOP_MODULATE4X = 6,
	// GGTOP_ADD = 7,
	// GGTOP_ADDSIGNED = 8,
	// GGTOP_ADDSIGNED2X = 9,
	// D3DTOP_SUBTRACT = 10,
	// GGTOP_ADDSMOOTH = 11,
	// D3DTOP_BLENDDIFFUSEALPHA = 12,
	// D3DTOP_BLENDTEXTUREALPHA = 13,
	// D3DTOP_BLENDFACTORALPHA = 14,
	// D3DTOP_BLENDTEXTUREALPHAPM = 15,
	// D3DTOP_BLENDCURRENTALPHA = 16,
	// D3DTOP_PREMODULATE = 17,
	// GGTOP_MODULATEALPHA_ADDCOLOR = 18,
	// GGTOP_MODULATECOLOR_ADDALPHA = 19,
	// GGTOP_MODULATEINVALPHA_ADDCOLOR = 20,
	// GGTOP_MODULATEINVCOLOR_ADDALPHA = 21,
	// D3DTOP_BUMPENVMAP = 22,
	// D3DTOP_BUMPENVMAPLUMINANCE = 23,
	// D3DTOP_DOTPRODUCT3 = 24,
	// D3DTOP_MULTIPLYADD = 25,
	// D3DTOP_LERP = 26,
	// iA, iB, iC, iR: D3DTA's : default is GGTA_TEXTURE,GGTA_CURRENT,-1,GGTA_CURRENT
	// iForceArgX
	// [forcably change the COLORARG values]
	// GGTA_DIFFUSE = 0          0x00000000  // select diffuse color (read only)
	// GGTA_CURRENT = 1          0x00000001  // select stage destination register (read/write)
	// GGTA_TEXTURE = 2          0x00000002  // select texture color (read only)
	// D3DTA_TFACTOR = 3          0x00000003  // select D3DRS_TEXTUREFACTOR (read only)
	// D3DTA_SPECULAR = 4         0x00000004  // select specular color (read only)
	// D3DTA_TEMP = 5             0x00000005  // select temporary register color (read/write)
	// D3DTA_CONSTANT = 6         0x00000006  // select texture stage constant
	// D3DTA_COMPLEMENT = 16      0x00000010  // take 1.0 - x (read modifier)
	// D3DTA_ALPHAREPLICATE = 32  0x00000020  // replicate alpha to color components (read modifier)

	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// U74 - 080509 - is limb number ok
	if ( iLimbID!=-1 )
		if ( !ConfirmObjectAndLimb ( iID, iLimbID ) )
			return;

	// apply to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	if ( iLimbID==-1 )
	{
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
			SetMultiTexture ( pObject->ppMeshList [ iMesh ], iStage, (DWORD)iMode, iTexCoordMode, iImage );

		// U73 - 210309 - apply D3DTA values to all meshes (if applicable)
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			if ( iA!=-1 ) pObject->ppMeshList [ iMesh ]->pTextures [ iStage ].dwBlendArg1 = iA;
			if ( iB!=-1 ) pObject->ppMeshList [ iMesh ]->pTextures [ iStage ].dwBlendArg2 = iB;
			if ( iC!=-1 ) pObject->ppMeshList [ iMesh ]->pTextures [ iStage ].dwBlendArg0 = iC;
			if ( iR!=-1 ) pObject->ppMeshList [ iMesh ]->pTextures [ iStage ].dwBlendArgR = iR;
		}
	}
	else
	{
		// U74 - 080509 - single limb
		sFrame* pFrame = pObject->ppFrameList[iLimbID];
		if ( pFrame )
		{
			if ( pFrame->pMesh )
			{
				SetMultiTexture ( pFrame->pMesh, iStage, (DWORD)iMode, iTexCoordMode, iImage );
				if ( iA!=-1 ) pFrame->pMesh->pTextures [ iStage ].dwBlendArg1 = iA;
				if ( iB!=-1 ) pFrame->pMesh->pTextures [ iStage ].dwBlendArg2 = iB;
				if ( iC!=-1 ) pFrame->pMesh->pTextures [ iStage ].dwBlendArg0 = iC;
				if ( iR!=-1 ) pFrame->pMesh->pTextures [ iStage ].dwBlendArgR = iR;
			}
		}
	}

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}

DARKSDK_DLL void SetObjectBlendMap ( int iID, int iStage, int iImage, int iTexCoordMode, int iMode, int iA, int iB, int iC, int iR )
{
	// U75 - 080509 - default all meshes, specify -1 for mesh param
	SetObjectBlendMap ( iID, -1, iStage, iImage, iTexCoordMode, iMode, iA, iB, iC, iR );
}

DARKSDK_DLL void SetObjectBlendMap ( int iID, int iStage, int iImage, int iTexCoordMode, int iMode )
{
	// default blend mapping command up to U73 - 210309 - replaced with larger blending command for lerping
	SetObjectBlendMap ( iID, iStage, iImage, iTexCoordMode, iMode, -1, -1, -1, -1 );
}

DARKSDK_DLL void SetBlendMap ( int iID, int iImage, int iMode )
{
	SetObjectBlendMap ( iID, 1, iImage, 3, iMode );
}

DARKSDK_DLL void SetTextureMD3 ( int iID, int iH0, int iH1, int iL0, int iL1, int iL2, int iU0 )
{
	// MIKEMIKE : Fits in with MD3 format of what DBO will make of it..[MD3]
}

DARKSDK_DLL int SwitchRenderTargetToDepthTexture ( int iFlag )
{
	return m_ObjectManager.SwitchRenderTargetToDepth(iFlag);
}


DARKSDK_DLL void SetObjectDebugInfo(int iID, DWORD value)
{
	// check the object exists
	if (!ConfirmObjectInstance(iID))
		return;

	// get object ptr
	sObject* pActualObject = g_ObjectList[iID];
	pActualObject->dwReservedR4 = value;
}

// New Texture Functions

DARKSDK_DLL void SetAlphaMappingOn ( int iID, float fPercentage, bool bForceUnTransparency )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object ptr
	sObject* pActualObject = g_ObjectList [ iID ];

	// 060217 - need to remember transparency state when not 100%
	if ( fPercentage != 100.0f )
	{
		if ( pActualObject->dwRememberTransparencyState == 0 )
		{
			if ( pActualObject->bTransparentObject == false ) 
				pActualObject->dwRememberTransparencyState  = 1;
			else
				pActualObject->dwRememberTransparencyState  = 2;
		}
	}

	// new mode to apply color for highlighting 
	if ( fPercentage > 100.0f )
	{
		GGCOLOR dwColorValueOnly = GGCOLOR_ARGB ( 0, 0, 0, 0 );
		if ( fPercentage < 110.0f )
		{
			if ( fPercentage <= 102.0f )
			{
				if ( fPercentage==101.0f )
				{
					dwColorValueOnly = GGCOLOR_ARGB ( 255, 128, 0, 0 );
				}
				else
				{
					dwColorValueOnly = GGCOLOR_ARGB ( 255, 128, 0, 64 );
				}
			}
			else
			{
				if ( fPercentage==103.0f )
				{
					dwColorValueOnly = GGCOLOR_ARGB ( 255, 0, 128, 0 );
				}
				else
				{
					dwColorValueOnly = GGCOLOR_ARGB ( 255, 0, 128, 64 );
				}
			}
			pActualObject->dwInstanceAlphaOverride = dwColorValueOnly;
			pActualObject->bInstanceAlphaOverride = true;
			pActualObject->bTransparentObject = true;
		}
		if ( fPercentage == 110.0f ) fPercentage = 52.0f;
		pActualObject->fArtificialDistanceOffset = 50000.0f;
	}

	// or set true alpha transparency
	if ( fPercentage <= 100.0f )
	{
		// reset extra color feature if back to 100
		if ( fPercentage == 100.0f )
		{
			pActualObject->dwInstanceAlphaOverride = 0;
			pActualObject->bInstanceAlphaOverride = false;

			// 261115 - cannot set to nontransparent as this may have been an explosion/fading decal and
			// must remain transparent for semi-transparent render ordering!
			if ( bForceUnTransparency == true )
			{
				// flagged when dehighlighting in editor (so non transparents can be rendered BEHIND transparent ones, EBE)
				pActualObject->bTransparentObject = false;
			}
			else
			{
				if ( pActualObject->dwRememberTransparencyState == 1 ) pActualObject->bTransparentObject = false;
				if ( pActualObject->dwRememberTransparencyState == 2 ) pActualObject->bTransparentObject = true;
			}
			pActualObject->dwRememberTransparencyState = 0;

			// 060217 - ensure we restore artificialdistanceoffset
			pActualObject->fArtificialDistanceOffset = 0;
		}

		// apply alpha factor
		if ( pActualObject->pInstanceOfObject )
		{
			// direct alpha factor effect on instance
			if ( fPercentage!=100.0f )
			{
				// some level of alpha, make transparent and set alpha value
				fPercentage/=100.0f;
				DWORD dwAlpha = (DWORD)(fPercentage*255);
				GGCOLOR dwAlphaValueOnly = GGCOLOR_ARGB ( dwAlpha, 0, 0, 0 );
				pActualObject->dwInstanceAlphaOverride = dwAlphaValueOnly;
				pActualObject->bInstanceAlphaOverride = true;
				pActualObject->bTransparentObject = true;
			}
		}
		else
		{
			// apply to all meshes
			for ( int iMesh = 0; iMesh < pActualObject->iMeshCount; iMesh++ )
			{
				WickedSetMeshNumber(iMesh);
				SetAlphaOverride ( pActualObject->ppMeshList [ iMesh ], fPercentage );
				SetTransparency ( pActualObject->ppMeshList [ iMesh ], true );
			}
		}

		#ifdef WICKEDENGINE
		WickedCall_SetObjectAlpha(pActualObject, fPercentage);
		#endif
	}
}

DARKSDK_DLL void SetAlphaMappingOn ( int iID, float fPercentage )
{
	SetAlphaMappingOn ( iID, fPercentage, false );
}

/*
DARKSDK_DLL void SetObjectEffectOn ( int iID, SDK_LPSTR pFilename, int iUseDefaultTextures )
{
	// check the object exists or not
	bool bUseDefaultModel = false;
	if ( !CheckObjectExist ( iID ) )
	{
		// create blank object to hold 'default model'
		MakeObjectPyramid ( iID, 1.0f );
		bUseDefaultModel = true;
	}

	// determine if we should use default textures
	bool bUseDefaultTextures = false;
	if ( iUseDefaultTextures==1 )
		bUseDefaultTextures = true;

	// Create external effect obj
	cSpecialEffect* pEffectObj = new cExternalEffect;
	pEffectObj->Load ( (char*)pFilename, bUseDefaultModel, bUseDefaultTextures );

	// reset vertex data before apply special effect
	sObject* pObject = g_ObjectList [ iID ];
	ResetVertexDataInMesh ( pObject );

	// apply to all meshes
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
	{
		// get mesh ptr
		sMesh* pMesh = pObject->ppMeshList [ iMesh ];

		// apply unique effect
		if ( !SetSpecialEffect ( pMesh, pEffectObj ) )
		{
			// failed to setup effect
			if ( bUseDefaultModel ) DeleteObjectSpecial ( iID );
			RunTimeError ( RUNTIMEERROR_B3DVSHADERCANNOTCREATE );
			return;
		}

		// first mesh is original, rest are references only
		if ( iMesh==0 )
			pMesh->bVertexShaderEffectRefOnly = false;
		else
			pMesh->bVertexShaderEffectRefOnly = true;
	}

	// full or quick update of object
	if ( bUseDefaultModel )
	{
		// setup new object and introduce to buffers
		SetNewObjectFinalProperties ( iID, -1.0f );
	}
	else
	{
		// as shader recreates mesh format, must regenerate buffer instance
		m_ObjectManager.RemoveObjectFromBuffers ( pObject );
		m_ObjectManager.AddObjectToBuffers ( pObject );
	}
}
*/

// FX effects commands

DARKSDK_DLL void LoadEffectEx ( LPSTR pFilename, int iEffectID, int iUseDefaultTextures, int iDoNotGenerateExtraData )
{
	#ifdef WICKEDENGINE
	// WickedEngine has its own shaders - dont need any FX5 legacy ones
	#else
	// check the effect exists
	if ( !ConfirmNewEffect ( iEffectID ) )
		return;

	// determine if file even exists
	if ( !DoesFileExist ( (LPSTR)pFilename ) )
	{
		RunTimeError ( RUNTIMEERROR_FILENOTEXIST );
		return;
	}

	// determine if we should use default textures
	bool bUseDefaultTextures = false;
	if ( iUseDefaultTextures==1 )
		bUseDefaultTextures = true;

	// load the effect into array
	m_EffectList [ iEffectID ] = new sEffectItem;
	if ( m_EffectList [ iEffectID ]->pEffectObj )
	{
		// assign generate extra data flag then load the effect
		m_EffectList [ iEffectID ]->pEffectObj->m_dwEffectIndex = iEffectID;
		m_EffectList [ iEffectID ]->pEffectObj->m_bDoNotGenerateExtraData = (DWORD)iDoNotGenerateExtraData;
		m_EffectList [ iEffectID ]->pEffectObj->m_bNeed8BonesPerVertex = false;
		if ( !m_EffectList [ iEffectID ]->pEffectObj->Load ( iEffectID, (char*)pFilename, false, bUseDefaultTextures ) )
		{
			// leefix - 200906 - u63 - if effect failed, still keep it in mem for delete effect clearup (can still checklist for errors on it)
			//SAFE_DELETE ( m_EffectList [ iEffectID ] );
		}
	}
	#endif
}

DARKSDK_DLL void LoadEffect ( LPSTR pFilename, int iEffectID, int iUseDefaultTextures )
{
	// default call generates extra data such as normals, tangents/binormals, etc
	#ifdef WICKEDENGINE
	// WickedEngine has its own shaders
	#else
	LoadEffectEx ( pFilename, iEffectID, iUseDefaultTextures, 0 );
	#endif
}

DARKSDK_DLL void DeleteEffectCore ( int iEffectID, bool bAlsoEraseObjReferences )
{
	#ifdef WICKEDENGINE
	// WickedEngine has its own shaders
	#else
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// decouple depth render system if used
	sEffectItem* pEffectItem = m_EffectList [ iEffectID ];
	if ( pEffectItem )
	{
		if ( pEffectItem->pEffectObj )
		{
			if ( g_pMainCameraDepthEffect==pEffectItem->pEffectObj->m_pEffect )
			{
				g_pMainCameraDepthEffect = NULL;
				g_pMainCameraDepthHandle = NULL;
			}
		}
	}

	// check all other objects that reference this effect, if they reference - it then remove the link so it wont cause a crash later on
	if ( bAlsoEraseObjReferences == true )
	{
		for ( DWORD dwObject = 0; dwObject < (DWORD)g_iObjectListCount; dwObject++ )
		{
			sObject* pObject = g_ObjectList [ dwObject ];
			if ( pObject )
			{
				for ( DWORD dwMesh = 0; dwMesh < (DWORD)pObject->iMeshCount; dwMesh++ )
				{
					if ( pObject->ppMeshList [ dwMesh ]->pVertexShaderEffect == m_EffectList [ iEffectID ]->pEffectObj )
					{
						strcpy ( pObject->ppMeshList [ dwMesh ]->pEffectName, "" );
						pObject->ppMeshList [ dwMesh ]->pVertexShaderEffect = NULL;
						pObject->ppMeshList[dwMesh]->dl_lights = NULL;
						pObject->ppMeshList[dwMesh]->dl_lightsVS = NULL;
						pObject->ppMeshList[dwMesh]->dl_pos[0] = NULL;
						pObject->ppMeshList[dwMesh]->dl_diffuse[0] = NULL;
						pObject->ppMeshList[dwMesh]->dl_angle[0] = NULL;
					}
				}
			}
		}
	}

	// also remove from SETUP-shortlist
	if ( SETUPFreeShader ( iEffectID ) == true )
	{
		// above deleted effect, so wipe effect ptr here
		m_EffectList [ iEffectID ] = NULL;
	}
	else
	{
		// delete effect from array
		SAFE_DELETE ( m_EffectList [ iEffectID ] );
	}

	// 210917 - also remove from effectparam list
	if ( iEffectID < EFFECT_INDEX_SIZE )
		SAFE_DELETE ( g_CascadedShadow.m_pEffectParam [ iEffectID ] );
	#endif
}

DARKSDK_DLL void DeleteEffect ( int iEffectID )
{
	#ifdef WICKEDENGINE
	// WickedEngine has its own shaders
	#else
	DeleteEffectCore ( iEffectID, true );
	#endif
}

// globals for now
//#include "ShadowMapping\cShadowMaps.h" not for DX12
int							g_PrimaryShadowEffect = 0;
int							g_iDebugObjStart = 0;
int							g_iDebugEffectIndex = 0;
int							g_HideDistantShadows = 1;
int							g_TerrainShadows = 0;
int							g_RealShadowResolution = 1024;
//CascadedShadowsManager      g_CascadedShadow;
//CascadeConfig               g_CascadeConfig;
bool                        g_bMoveLightTexelSize = TRUE;
//CFirstPersonCamera          g_ViewerCamera;          
//CFirstPersonCamera          g_LightCamera;         
//CFirstPersonCamera*         g_pActiveCamera = &g_ViewerCamera;

DARKSDK_DLL void SetEffectToShadowMappingEx ( int iEffectID, int iDebugObjStart, int iDebugEffectIndex, int iHideDistantShadows, int iTerrainShadows, int iRealShadowResolution, int iRealShadowCascadeCount, int iC0, int iC1, int iC2, int iC3, int iC4, int iC5, int iC6, int iC7 )
{
	// setup effect to support shadow mapping
	g_PrimaryShadowEffect = iEffectID;
	if ( iDebugObjStart > 0 )
	{
		g_iDebugObjStart = iDebugObjStart;
		g_iDebugEffectIndex = iDebugEffectIndex;
	}
	g_HideDistantShadows = iHideDistantShadows;
	g_TerrainShadows = iTerrainShadows;
	g_RealShadowResolution = iRealShadowResolution;
    //g_CascadeConfig.m_iBufferSize = g_RealShadowResolution;
    //g_CascadeConfig.m_nCascadeLevels = iRealShadowCascadeCount;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[0] = iC0;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[1] = iC1;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[2] = iC2;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[3] = iC3;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[4] = iC4;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[5] = iC5;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[6] = iC6;
    //g_CascadedShadow.m_iCascadePartitionsZeroToOne[7] = iC7;
    //g_CascadedShadow.m_iCascadePartitionsMax = 100;
    //SHADOW_TEXTURE_FORMAT sbt = (SHADOW_TEXTURE_FORMAT)0;
    //g_CascadeConfig.m_ShadowBufferFormat = sbt;
    //g_CascadedShadow.m_bMoveLightTexelSize = g_bMoveLightTexelSize;
    //g_CascadedShadow.m_eSelectedCascadesFit = FIT_TO_SCENE; 
    //g_CascadedShadow.m_eSelectedNearFarFit = FIT_NEARFAR_SCENE_AABB;
	//g_CascadedShadow.m_fBlurBetweenCascadesAmount = 0.25f;

	// create resources for shadow mapper
   // g_CascadedShadow.Init(	&g_ViewerCamera, &g_LightCamera, &g_CascadeConfig );

	// cascade render mask (upto eight cascades)
	//g_CascadedShadow.m_dwMask = 0xF;

	// complete
	return;
}

DARKSDK_DLL void ChangeShadowMappingPrimary ( int iEffectID )
{
	// when switch from two terrain shaders (PBR and NONPBR), ensure shadow creaiton tied to active one
	g_PrimaryShadowEffect = iEffectID;
}

DARKSDK_DLL void SetEffectToShadowMapping ( int iEffectID )
{
	SetEffectToShadowMappingEx ( iEffectID, 0, 0, 1, 0, 1024, 4, 2, 8, 16, 75, 100, 100, 100, 100 );
}

DARKSDK_DLL void SetEffectShadowMappingMode ( int iMode )
{
	// Can set the mask for which cascades get rendered
	//g_CascadedShadow.m_dwMask = iMode;
}

DARKSDK_DLL void SetShadowTexelSize(int isize)
{
	// Can set the size of the cascade textures use, to calculate the texel size.
	//g_CascadeConfig.m_iBufferSize = isize;
}

DARKSDK_DLL void RenderEffectShadowMapping ( int iEffectID )
{
	// renders shadow maps for effect

	// check the effect exists
	if ( iEffectID!=0 )
		if ( !ConfirmEffect ( iEffectID ) )
			return;

	// effect pointer
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// primary shadow map must produce the shadow
	if ( iEffectID==g_PrimaryShadowEffect )
	{
		// assign this effect as primary
		pEffectObject->m_bPrimaryEffectForCascadeShadowMapping = true;

		// set a clear color
		FLOAT ClearColor[4] = { 0.0f, 0.25f, 0.25f, 0.55f };

		// process shadow mapping frame
		//g_CascadedShadow.InitFrame( pEffectPtr );

		// set technique for depth rendering
		#ifdef DX11
		GGTECHNIQUEHANDLE hOldTechnique = pEffectObject->m_hCurrentTechnique;
		if ( pEffectPtr )
		{
			pEffectObject->m_hCurrentTechnique = pEffectPtr->GetTechniqueByName ( "DepthMap" );
		}
		#else
		GGHANDLE hOldTechnique = pEffectPtr->GetCurrentTechnique();
		if ( pEffectPtr )
		{
			GGHANDLE hTechnique = pEffectPtr->GetTechniqueByName ( "DepthMap" );
			if ( hTechnique )
				pEffectPtr->SetTechnique(hTechnique);
		}
		#endif

		// render all shadows in cascades (multiple shadow maps based on frustrum slices)
		//g_CascadedShadow.RenderShadowsForAllCascades(pEffectPtr);

		// Restore technique after depth renders
		#ifdef DX11
		if ( pEffectPtr && hOldTechnique )
		{
			pEffectObject->m_hCurrentTechnique = hOldTechnique;
		}
		#else
		if ( pEffectPtr && hOldTechnique )
			pEffectPtr->SetTechnique(hOldTechnique);
		#endif
	}
	// set shaodw mapping settings for final render (for all effects that call this command inc. primary)
    //g_CascadedShadow.RenderScene( iEffectID, pEffectPtr, NULL, NULL, NULL, false );

	// create debug objects to view shadow maps
	if ( g_iDebugObjStart > 0 )
	{
		if ( ObjectExist ( g_iDebugObjStart+0 ) == 0 )
		{
			int iShadowDebugObj = g_iDebugObjStart+0;
			MakeObjectBox ( iShadowDebugObj, 1024, 1024, 0.1f );
			SetObjectLight ( iShadowDebugObj, 1 );
			LockObjectOn ( iShadowDebugObj );
			SetObjectEffect(iShadowDebugObj, g_iDebugEffectIndex);
			PositionObject ( iShadowDebugObj, 0, 0, 1155 );
			DisableObjectZRead ( iShadowDebugObj );
			HideObject ( iShadowDebugObj );
		}
	}

	// complete
	return;
}

DARKSDK_DLL void SetDefaultCPUAnimState ( int iCPUAnimMode )
{
	g_iDefaultCPUAnimState = iCPUAnimMode;
}

DARKSDK_DLL void SetObjectEffectCore ( int iID, int iEffectID, int iEffectNoBoneID, int iForceCPUAnimationMode )
{
	#ifdef WICKEDENGINE
	// Wicked has its own shaders
	#else
	// iForceCPUAnimationMode:
	// 0-USE DEFAULT SETTING (0=GPU/CPU or 3=strictly GPU only)
	// 1-Force CPU Animation
	// 2-Anim + Hide all meshes with no bone data
	// 3-Force GPU Animation Only (prevents ALL CPU animations for performance)
	if ( iForceCPUAnimationMode == 0 ) iForceCPUAnimationMode = g_iDefaultCPUAnimState;

	// check the object exists
	g_pGlob->dwInternalFunctionCode=10001;
	if ( !ConfirmObject ( iID ) )
		return;

	// check the effect exists
	if ( iEffectID!=0 )
		if ( !ConfirmEffect ( iEffectID ) )
			return;

	// check the effectnobone exists
	if ( iEffectNoBoneID!=0 )
		if ( !ConfirmEffect ( iEffectNoBoneID ) )
			return;

	// get object ptr
	g_pGlob->dwInternalFunctionCode=10002;
	sObject* pObject = g_ObjectList [ iID ];

	// leefix - 040805 - if object ALREADY has effect, must remove it first
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
	{
		sMesh* pMesh = pObject->ppMeshList [ iMesh ];
		SetSpecialEffect ( pMesh, NULL );
	}

	// reset vertex data before apply special effect
	ResetVertexDataInMesh ( pObject );

	// remove effect if zero
	if ( iEffectID>0 )
	{
		// apply to specific mesh
		g_pGlob->dwInternalFunctionCode=11000;
		sEffectItem* pEffectItem = m_EffectList [ iEffectID ];

		sEffectItem* pEffectNoBoneItem = NULL;
		if ( iEffectNoBoneID>0 ) pEffectNoBoneItem = m_EffectList [ iEffectNoBoneID ];

		// 131018 - check if any of the meshes require 8 bones per vertex
		// if they do, the whole model needs to use the 8 bone system
		bool bUses8BonePerVertexSystem = false;
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];
			if ( bUses8BonePerVertexSystem == false )
				if ( CheckIfNeedExtraBonesPerVertices ( pMesh ) == true )
					bUses8BonePerVertexSystem = true;
		}
		pEffectItem->pEffectObj->m_bNeed8BonesPerVertex = bUses8BonePerVertexSystem;
		if ( pEffectNoBoneItem != NULL ) pEffectNoBoneItem->pEffectObj->m_bNeed8BonesPerVertex = bUses8BonePerVertexSystem;

		// apply setting to all meshes
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			// for each mesh
			g_pGlob->dwInternalFunctionCode=11001+iMesh;
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];

			// if nobone effect index submitted, means we want to assign two effect types based on whether mesh has bones
			bool bShaderApplySuccess = false;
			if ( iEffectNoBoneID>0 && pEffectNoBoneItem!=NULL && pMesh->dwBoneCount==0 )
			{
				// apply specific NON-BONE effect passed in as optional parameter
				bShaderApplySuccess = SetSpecialEffect ( pMesh, pEffectNoBoneItem->pEffectObj );
			}
			else
			{
				// apply REGULAR (BONE) shared effect
				bShaderApplySuccess = SetSpecialEffect ( pMesh, pEffectItem->pEffectObj );
			}
			if ( bShaderApplySuccess==true )
			{
				pMesh->bVertexShaderEffectRefOnly = true;
				pMesh->dwForceCPUAnimationMode = (DWORD)iForceCPUAnimationMode;
				if ( pMesh->dwForceCPUAnimationMode == 2 ) pMesh->dwForceCPUAnimationMode = 1;
				if ( iForceCPUAnimationMode==2 && pMesh->dwBoneCount==0 )
				{
					pMesh->bVisible = false;
				}
			}
			else
			{
				// lee - 300914 - maybe replace this with substitute technique?
				pMesh->bVisible = false;
			}
		}
	}
	else
	{
		// reset setting to all meshes
		g_pGlob->dwInternalFunctionCode=12001;
		sObject* pObject = g_ObjectList [ iID ];
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			g_pGlob->dwInternalFunctionCode=12001+iMesh;
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];
			SetSpecialEffect ( pMesh, NULL );
			pMesh->dwForceCPUAnimationMode = 0;
		}
	}

	// as recreates mesh format, must regenerate buffer instance
	g_pGlob->dwInternalFunctionCode=13001;
	m_ObjectManager.RemoveObjectFromBuffers ( pObject );
	m_ObjectManager.AddObjectToBuffers ( pObject );
	g_pGlob->dwInternalFunctionCode=13002;
	#endif
}

DARKSDK_DLL void SetObjectEffectCore ( int iID, int iEffectID, int iForceCPUAnimationMode )
{
	SetObjectEffectCore ( iID, iEffectID, 0, iForceCPUAnimationMode );
}

DARKSDK_DLL void SetOcclusionMode ( int iOcclusionMode )
{
	// 0 - none
	// 1 - HOQ - Hardware Occlusion Queries (determine if visible pixels rendered)
	g_Occlusion.SetOcclusionMode ( iOcclusionMode );
}

DARKSDK_DLL void SetObjectOcclusion ( int iID, int iOcclusionShape, int iMeshOrLimbID, int iA, int iIsOccluder, int iDeleteFromOccluder )
{
	// iOcclusionShape & iMeshOrLimbID ignored for now

	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// object ptr
	sObject* pObject = g_ObjectList [ iID ];

	// HOQ Method
	if ( g_Occlusion.GetOcclusionMode()==1 )
	{
		// only if not already an occludabler
		if ( g_Occlusion.d3dQuery[pObject->dwObjectNumber] )
			return;

		// create query object
		#ifdef DX11
		#else
		m_pD3D->CreateQuery( D3DQUERYTYPE_OCCLUSION, &g_Occlusion.d3dQuery[pObject->dwObjectNumber] );
		g_Occlusion.d3dQuery[pObject->dwObjectNumber]->Issue( D3DISSUE_BEGIN );
		g_Occlusion.d3dQuery[pObject->dwObjectNumber]->Issue( D3DISSUE_END );
		#endif
	}

	// complete
	return;
}

DARKSDK_DLL int GetObjectOcclusionValue ( int iID )
{
	// return var
	int iReturnValue = 0;

	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return 0;

	if ( g_Occlusion.GetOcclusionMode()==1 )
	{
		// can get how many of its pixels got rendered
		sObject* pObject = g_ObjectList [ iID ];
		iReturnValue = g_Occlusion.dwQueryValue[pObject->dwObjectNumber];

		// and trigger the next query (does not happen each cycle as expensive for some GPUs)
		if ( g_Occlusion.iQueryBusyStage[pObject->dwObjectNumber]==0 )
			g_Occlusion.iQueryBusyStage[pObject->dwObjectNumber] = 99;
	}
	if ( g_Occlusion.GetOcclusionMode()==2 )
	{
		// 0-shown or 1-occluded
		iReturnValue = 0;
	}

	// return value
	return iReturnValue;
}

DARKSDK_DLL void SetObjectEffect ( int iID, int iEffectID )
{
	// call master core function for this
	SetObjectEffectCore ( iID, iEffectID, 0 );
}

DARKSDK_DLL void SetLimbEffect ( int iID, int iLimbID, int iEffectID )
{
	#ifdef WICKEDENGINE
	// Wicked has its own shaders
	#else
	// check the object exists
	if ( !ConfirmObjectAndLimb ( iID, iLimbID ) )
		return;

	// check the effect exists
	if ( iEffectID!=0 )
		if ( !ConfirmEffect ( iEffectID ) )
			return;

	// ensure limb has mesh
	sObject* pObject = g_ObjectList [ iID ];
	sMesh* pMesh = pObject->ppFrameList [ iLimbID ]->pMesh;
	if ( !pMesh )
		return;

	// ensure mesh is reset
	ResetVertexDataInMeshPerMesh ( pMesh );

	// apply to specific mesh
	if ( iEffectID>0 )
	{
		sEffectItem* pEffectItem = m_EffectList [ iEffectID ];
		SetSpecialEffect ( pMesh, pEffectItem->pEffectObj );
		pMesh->bVertexShaderEffectRefOnly = true;
	}
	else
		SetSpecialEffect ( pMesh, NULL );

	// as recreates mesh format, must regenerate buffer instance
	m_ObjectManager.RemoveObjectFromBuffers ( pObject );
	m_ObjectManager.AddObjectToBuffers ( pObject );
	#endif
}

DARKSDK_DLL void PerformChecklistForEffectValues ( int iEffectID )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	#ifdef DX11
	#else
	// Get effect ptr and desc
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObj = NULL;
	if ( m_EffectList [ iEffectID ]->pEffectObj )
	{
		pEffectObj = m_EffectList [ iEffectID ]->pEffectObj;
		if ( pEffectObj->m_pEffect ) pEffectPtr = pEffectObj->m_pEffect;
	}

	// if effect tr valid
	if ( !pEffectPtr )
		return;

	// build checklist from all top-level params
	GGEFFECT_DESC	EffectDesc;
	pEffectPtr->GetDesc( &EffectDesc );

	// Generate Checklist
	DWORD dwMaxStringSizeInEnum=0;
	bool bCreateChecklistNow=false;
	g_pGlob->checklisthasvalues=true;
	g_pGlob->checklisthasstrings=true;
	for(int pass=0; pass<2; pass++)
	{
		if(pass==1)
		{
			// Ensure checklist is large enough
			bCreateChecklistNow=true;
			for(int c=0; c<g_pGlob->checklistqty; c++)
				GlobExpandChecklist(c, dwMaxStringSizeInEnum);
		}

		// Look at parameters
		g_pGlob->checklistqty=0;
		for( UINT iParam = 0; iParam < EffectDesc.Parameters; iParam++ )
		{
			// get this parameter handle and description
			D3DXPARAMETER_DESC ParamDesc;
			GGHANDLE hParam = pEffectPtr->GetParameter ( NULL, iParam );
			pEffectPtr->GetParameterDesc( hParam, &ParamDesc );

			// Add to checklist
			DWORD dwSize=0;
			if(ParamDesc.Name) dwSize=strlen(ParamDesc.Name);
			if(dwSize>dwMaxStringSizeInEnum) dwMaxStringSizeInEnum=dwSize;
			if(bCreateChecklistNow)
			{
				// New checklist item
				if(ParamDesc.Name==NULL)
					strcpy(g_pGlob->checklist[g_pGlob->checklistqty].string, "<noname>");
				else
					strcpy(g_pGlob->checklist[g_pGlob->checklistqty].string, ParamDesc.Name);

				// class and type form var type id (dbpro usage)
				int iVarTypeIdentity = 0;
				if ( ParamDesc.Class==D3DXPC_SCALAR )
				{
					if ( ParamDesc.Type==D3DXPT_BOOL ) iVarTypeIdentity = 1;
					if ( ParamDesc.Type==D3DXPT_INT ) iVarTypeIdentity = 2;
					if ( ParamDesc.Type==D3DXPT_FLOAT ) iVarTypeIdentity = 3;
				}
				if ( ParamDesc.Class==D3DXPC_VECTOR )
				{
					iVarTypeIdentity = 4;
				}
				if ( ParamDesc.Class==D3DXPC_MATRIX_ROWS
				||   ParamDesc.Class==D3DXPC_MATRIX_COLUMNS )
				{
					iVarTypeIdentity = 5;
				}
				g_pGlob->checklist[g_pGlob->checklistqty].valuea = iVarTypeIdentity;

				// whether app-hooked (dbpro providing the value)
				if ( pEffectObj->AssignValueHook ( (char*)ParamDesc.Semantic, NULL)==true )
					g_pGlob->checklist[g_pGlob->checklistqty].valueb = 1;
				else
				{
					// non semantic hook identities
					bool bHookIsValid = false;

					// by annotation
					GGHANDLE hAnnot = pEffectPtr->GetAnnotationByName( hParam, "UIDirectional" );
					if( hAnnot != NULL && pEffectObj->m_LightDirHandle ) bHookIsValid=true;
					hAnnot = pEffectPtr->GetAnnotationByName( hParam, "UIDirectionalInv" );
					if( hAnnot != NULL && pEffectObj->m_LightDirInvHandle ) bHookIsValid=true;
					hAnnot = pEffectPtr->GetAnnotationByName( hParam, "UIPosition" );
					if( hAnnot != NULL && pEffectObj->m_LightPosHandle ) bHookIsValid=true;
					hAnnot = pEffectPtr->GetAnnotationByName( hParam, "UIObject" );
					if( hAnnot != NULL )
					{
						// light type
						LPCSTR pstrLightType = NULL;
						if ( hAnnot != NULL ) pEffectPtr->GetString( hAnnot, &pstrLightType );
						if ( pstrLightType )
						{
							if ( _stricmp((char*)pstrLightType,"directionalight")==NULL && pEffectObj->m_LightDirHandle ) bHookIsValid=true;
							if ( _stricmp((char*)pstrLightType,"pointlight")==NULL && pEffectObj->m_LightPosHandle ) bHookIsValid=true;
							if ( _stricmp((char*)pstrLightType,"spotlight")==NULL && pEffectObj->m_LightPosHandle ) bHookIsValid=true;
						}
					}

					// special cases
					if( _stricmp ( ParamDesc.Name, "XFile" )==NULL ) bHookIsValid=true;
					if ( ParamDesc.Type>=D3DXPT_TEXTURE ) bHookIsValid=true;

					// assign result
					if ( bHookIsValid==true )
						g_pGlob->checklist[g_pGlob->checklistqty].valueb = 1;
					else
						g_pGlob->checklist[g_pGlob->checklistqty].valueb = 0;
				}

				// class
				g_pGlob->checklist[g_pGlob->checklistqty].valuec = ParamDesc.Class;

				// type
				g_pGlob->checklist[g_pGlob->checklistqty].valued = ParamDesc.Type;
			}
			g_pGlob->checklistqty++;
		}
	}

	// Determine if checklist has any contents
	if(g_pGlob->checklistqty>0)
		g_pGlob->checklistexists=true;
	else
		g_pGlob->checklistexists=false;
	#endif
}

DARKSDK_DLL void PerformChecklistForEffectErrors ( void )
{
	// Generate Checklist
	DWORD dwMaxStringSizeInEnum=0;
	bool bCreateChecklistNow=false;
	g_pGlob->checklisthasvalues=false;
	g_pGlob->checklisthasstrings=true;
	for(int pass=0; pass<2; pass++)
	{
		if(pass==1)
		{
			// Ensure checklist is large enough
			bCreateChecklistNow=true;
			for(int c=0; c<g_pGlob->checklistqty; c++)
				GlobExpandChecklist(c, dwMaxStringSizeInEnum);
		}

		// Look at error buffer (if any)
		if ( g_pEffectErrorMsg ) 
		{
			LPSTR pPtr = g_pEffectErrorMsg;
			LPSTR pPtrEnd = g_pEffectErrorMsg + g_dwEffectErrorMsgSize;
			LPSTR pLastByte = pPtr;
			g_pGlob->checklistqty=0;
			while ( 1 )
			{
				if ( pPtr>=pPtrEnd || ( *(unsigned char*)pPtr==13 || *(unsigned char*)(pPtr+1)==10 ) )
				{
					// determine error line
					DWORD dwSize = (pPtr-pLastByte)+1;
					LPSTR pErrorLine = new char[dwSize];
					memcpy ( pErrorLine, pLastByte, dwSize );
					pErrorLine[dwSize]=0;

					// skip the colons
					int nn=-1; int iCount=2;
					for ( int n=0; n<(int)dwSize; n++)
					{
						if ( pErrorLine[n]==':' )
						{
							if ( iCount>0 )
							{
								nn=n+2;
								iCount--;
							}
							else
								break;
						}
					}
					if ( nn!=-1 )
					{
						_strrev ( pErrorLine );
						pErrorLine[dwSize-nn]=0;
						_strrev ( pErrorLine );
						dwSize = strlen(pErrorLine)+1;
					}

					// Add to checklist at end of line or buffer
					if(dwSize>dwMaxStringSizeInEnum) dwMaxStringSizeInEnum=dwSize;
					if(bCreateChecklistNow)
					{
						// New checklist item
						strcpy(g_pGlob->checklist[g_pGlob->checklistqty].string, pErrorLine);
					}
					g_pGlob->checklistqty++;

					// go for next line
					if ( pPtr>=pPtrEnd )
					{
						// exit buffer scan
						break;
					}
					else
					{
						// next line
						pPtr+=2; pLastByte=pPtr;
					}
				}
				else
				{
					// next byte
					pPtr++;
				}
			}
		}
	}
 
	// Determine if checklist has any contents
	if(g_pGlob->checklistqty>0)
		g_pGlob->checklistexists=true;
	else
		g_pGlob->checklistexists=false;
}

DARKSDK_DLL void PerformChecklistForEffectErrors ( int iEffectID )
{
	PerformChecklistForEffectErrors();
}

