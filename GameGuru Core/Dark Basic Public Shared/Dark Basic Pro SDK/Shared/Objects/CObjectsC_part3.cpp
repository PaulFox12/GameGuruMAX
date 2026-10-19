DARKSDK_DLL void SetEffectTranspose ( int iEffectID, int iTransposeFlag )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// Get effect ptr
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;

	// apply flag
	if ( iTransposeFlag==1 )
		pEffectObject->m_bTranposeToggle = true;
	else
		pEffectObject->m_bTranposeToggle = false;
}

DARKSDK_DLL void ResetEffect ( int iEffectID )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// called when switching shader and need new param values from new shader
	//SAFE_DELETE ( g_CascadedShadow.m_pEffectParam[iEffectID] );
}

DARKSDK_DLL void EraseEffectParameterIndex ( int iEffectID, LPSTR pConstantName )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// prevent system from overriding manual change by removing hook
	#ifdef DX11
	#else
	if ( pEffectObject && pEffectPtr )
	{
		GGHANDLE hConstantParamHandle = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );
		if ( !g_EffectParamHandleList.empty() )
		{
			for ( DWORD iIndex = 0; iIndex < g_EffectParamHandleList.size(); ++iIndex )
			{
				GGHANDLE hThisHandle = g_EffectParamHandleList [ iIndex ];
				if ( hThisHandle==hConstantParamHandle )
				{
					g_EffectParamHandleList [ iIndex ] = NULL;
				}
			}
		}
	}
	#endif
}

DARKSDK_DLL DWORD GetEffectParameterIndex ( int iEffectID, LPSTR pConstantName )
{
	#ifdef WICKEDENGINE
	// Wicked has own shaders
	return 0;
	#else
	// return unique index
	DWORD dwParameterIndex = 0;
#ifdef WICKEDENGINE
	//PE: WICKED CHANGE
	return dwParameterIndex;
#endif
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return 0;

	// get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// prevent system from overriding manual change by removing hook
	if ( pEffectObject && pEffectPtr )
	{
		#ifdef DX11
		GGHANDLE hConstantParamHandle = pEffectPtr->GetVariableByName ( (char*)pConstantName );
		#else
		GGHANDLE hConstantParamHandle = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );
		#endif
		if ( !g_EffectParamHandleList.empty() )
		{
			for ( DWORD iIndex = 0; iIndex < g_EffectParamHandleList.size(); ++iIndex )
			{
				GGHANDLE hThisHandle = g_EffectParamHandleList [ iIndex ];
				if ( hThisHandle==hConstantParamHandle )
				{
					// found param already in list, return unique index
					#ifdef DX11
					dwParameterIndex = (DWORD)hThisHandle;
					#else
					dwParameterIndex = iIndex;
					#endif
					hConstantParamHandle=NULL;
					break;
				}
			}
		}
		if ( hConstantParamHandle!=NULL )
		{
			pEffectObject->AssignValueHookCore ( NULL, hConstantParamHandle, 0, true ); //remove handle
			g_EffectParamHandleList.push_back ( hConstantParamHandle );
			#ifdef DX11
			dwParameterIndex = (DWORD)hConstantParamHandle;
			#else
			dwParameterIndex = g_EffectParamHandleList.size() - 1;
			#endif
		}
	}

	// return unique index to parameter
	return dwParameterIndex;
	#endif
}

DARKSDK_DLL LPGGEFFECT SetEffectConstantCore ( int iEffectID, LPSTR pConstantName, int iOptionalParamIndex )
{
	#ifdef WICKEDENGINE
	// Wicked has own shaders
	return NULL;
	#else

	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return NULL;

	// Get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// special hook constant names for internal shader texture hooks
	if ( pConstantName )
	{
		if ( strnicmp ( (char*)pConstantName, "[hook-depth-data]", strlen((char*)pConstantName) )==NULL )
		{
			// find shader handle, assign to texture in effect
			g_pMainCameraDepthEffect = pEffectPtr;
			if ( g_pMainCameraDepthEffect!=NULL )
			{
				#ifdef DX11
				g_pMainCameraDepthHandle = g_pMainCameraDepthEffect->GetVariableByName( "DepthTex" );
				#else
				g_pMainCameraDepthHandle = g_pMainCameraDepthEffect->GetParameterByName( NULL, "DepthTex" );
				#endif
			}
		}
	}

	// prevent system from overriding manual change by removing hook
	if ( pEffectObject && pEffectPtr )
	{
		if ( pConstantName==NULL )
		{
			// already removed hook for paramindex based constant setting
		}
		else
		{
			#ifdef DX11
			GGHANDLE hParam = pEffectPtr->GetVariableByName ( (char*)pConstantName );
			if ( hParam->IsValid() )
			{
				pEffectObject->AssignValueHookCore ( NULL, hParam, 0, true ); //remove handle
			}
			#else
			GGHANDLE hParam = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );
			pEffectObject->AssignValueHookCore ( NULL, hParam, 0, true ); //remove handle
			#endif
		}
	}

	// return effect ptr (if any)
	return pEffectPtr;
	#endif
}

DARKSDK_DLL LPGGEFFECT SetEffectConstantCore ( int iEffectID, LPSTR pConstantName )
{
	return SetEffectConstantCore ( iEffectID, pConstantName, -1 );
}

DARKSDK_DLL void SetEffectConstantB ( int iEffectID, LPSTR pConstantName, DWORD dwOptionalParamIndex, int iValue )
{
	// get constant ptr
	LPGGEFFECT pEffectPtr = SetEffectConstantCore ( iEffectID, pConstantName );
	#ifdef DX11
	if ( pEffectPtr )
	{
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = (GGHANDLE)dwOptionalParamIndex;
		else
			hParam = pEffectPtr->GetVariableByName ( (char*)pConstantName );

		if ( hParam->IsValid() )
			hParam->AsScalar()->SetBool ( iValue );
	}
	#else
	if ( pEffectPtr )
	{
		// apply value to constant
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = g_EffectParamHandleList [ iOptionalParamIndex ];
		else
			hParam = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );

		pEffectPtr->SetBool ( hParam, iValue );
	}
	#endif
}

DARKSDK_DLL void SetEffectConstantB ( int iEffectID, LPSTR pConstantName, int iValue )
{
	SetEffectConstantB ( iEffectID, pConstantName, -1, iValue );
}

DARKSDK_DLL void SetEffectConstantBEx ( int iEffectID, DWORD dwParamIndex, int iValue )
{
	SetEffectConstantB ( iEffectID, NULL, dwParamIndex, iValue );
}

DARKSDK_DLL void SetEffectConstantI ( int iEffectID, LPSTR pConstantName, DWORD dwOptionalParamIndex, int iValue )
{
	// get constant ptr
	LPGGEFFECT pEffectPtr = SetEffectConstantCore ( iEffectID, pConstantName );
	#ifdef DX11
	if ( pEffectPtr )
	{
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = (GGHANDLE)dwOptionalParamIndex;
		else
			hParam = pEffectPtr->GetVariableByName ( (char*)pConstantName );

		if ( hParam->IsValid() )
			hParam->AsScalar()->SetInt ( iValue );
	}
	#else
	if ( pEffectPtr )
	{
		// apply value to constant
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = g_EffectParamHandleList [ iOptionalParamIndex ];
		else
			hParam = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );

		pEffectPtr->SetInt ( hParam, iValue );
	}
	#endif
}

DARKSDK_DLL void SetEffectConstantI ( int iEffectID, LPSTR pConstantName, int iValue )
{
	SetEffectConstantI ( iEffectID, pConstantName, -1, iValue );
}

DARKSDK_DLL void SetEffectConstantIEx ( int iEffectID, DWORD dwParamIndex, int iValue )
{
	SetEffectConstantI ( iEffectID, NULL, dwParamIndex, iValue );
}

DARKSDK_DLL void SetEffectConstantF ( int iEffectID, LPSTR pConstantName, DWORD dwOptionalParamIndex, float fValue )
{
	// get constant ptr
	LPGGEFFECT pEffectPtr = SetEffectConstantCore ( iEffectID, pConstantName );
	#ifdef DX11
	if ( pEffectPtr )
	{
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = (GGHANDLE)dwOptionalParamIndex;
		else
			hParam = pEffectPtr->GetVariableByName ( (char*)pConstantName );

		if ( hParam->IsValid() )
			hParam->AsScalar()->SetFloat ( fValue );
	}
	#else
	if ( pEffectPtr )
	{
		// apply value to constant
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = g_EffectParamHandleList [ iOptionalParamIndex ];
		else
			hParam = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );

		pEffectPtr->SetFloat ( hParam, fValue );
	}
	#endif
}

DARKSDK_DLL void SetEffectConstantF ( int iEffectID, LPSTR pConstantName, float fValue )
{
	SetEffectConstantF ( iEffectID, pConstantName, -1, fValue );
}

DARKSDK_DLL void SetEffectConstantFEx ( int iEffectID, DWORD dwParamIndex, float fValue )
{
	SetEffectConstantF ( iEffectID, NULL, dwParamIndex, fValue );
}

DARKSDK_DLL void SetEffectConstantV ( int iEffectID, LPSTR pConstantName, DWORD dwOptionalParamIndex, int iVector )
{
	// early out if no valid  param index
	if ( pConstantName==NULL && dwOptionalParamIndex==-1 )
		return;

	// get constant ptr
	LPGGEFFECT pEffectPtr = SetEffectConstantCore ( iEffectID, pConstantName );
	#ifdef DX11
	if ( pEffectPtr )
	{
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = (GGHANDLE)dwOptionalParamIndex;
		else
			hParam = pEffectPtr->GetVariableByName ( (char*)pConstantName );

		if ( hParam->IsValid() )
		{
			GGVECTOR4 vecData = GetVector4 ( iVector );
			hParam->AsVector()->SetFloatVector ( (float*)&vecData );
		}
	}
	#else
	if ( pEffectPtr )
	{
		// apply value to constant
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = g_EffectParamHandleList [ iOptionalParamIndex ];
		else
			hParam = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );

		GGVECTOR4 vecData = GetVector4 ( iVector );
		pEffectPtr->SetVector ( hParam, &vecData );
	}
	#endif
}

DARKSDK_DLL void SetEffectConstantV ( int iEffectID, LPSTR pConstantName, int iVector )
{
	SetEffectConstantV ( iEffectID, pConstantName, -1, iVector );
}

DARKSDK_DLL void SetEffectConstantVEx ( int iEffectID, DWORD dwParamIndex, int iValue )
{
	SetEffectConstantV ( iEffectID, NULL, dwParamIndex, iValue );
}

DARKSDK_DLL void SetEffectConstantM ( int iEffectID, LPSTR pConstantName, DWORD dwOptionalParamIndex, int iMatrix )
{
	// early out if no valid  param index
	if ( pConstantName==NULL && dwOptionalParamIndex==-1 )
		return;

	// get constant ptr
	LPGGEFFECT pEffectPtr = SetEffectConstantCore ( iEffectID, pConstantName );
	#ifdef DX11
	if ( pEffectPtr )
	{
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = (GGHANDLE)dwOptionalParamIndex;
		else
			hParam = pEffectPtr->GetVariableByName ( (char*)pConstantName );

		if ( hParam->IsValid() )
		{
			GGMATRIX matData = GetMatrix ( iMatrix );
			hParam->AsMatrix()->SetMatrix ( (float*)&matData );
		}
	}
	#else
	if ( pEffectPtr )
	{
		// apply value to constant
		GGHANDLE hParam;
		if ( pConstantName==NULL )
			hParam = g_EffectParamHandleList [ iOptionalParamIndex ];
		else
			hParam = pEffectPtr->GetParameterByName ( NULL, (char*)pConstantName );

		GGMATRIX matData = GetMatrix ( iMatrix );
		pEffectPtr->SetMatrix ( hParam, &matData );
	}
	#endif
}

DARKSDK_DLL void SetEffectConstantM ( int iEffectID, LPSTR pConstantName, int iMatrix )
{
	SetEffectConstantM ( iEffectID, pConstantName, -1, iMatrix );
}

DARKSDK_DLL void SetEffectConstantMEx ( int iEffectID, DWORD dwParamIndex, int iValue )
{
	SetEffectConstantM ( iEffectID, NULL, dwParamIndex, iValue );
}

DARKSDK_DLL void SetEffectTechnique	( int iEffectID, LPSTR pTechniqueName )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// Get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// Choose technique based on name
	#ifdef DX11
	if ( pEffectPtr )
	{
		if ( pTechniqueName == NULL )
		{
			pEffectObject->m_hCurrentTechnique = pEffectPtr->GetTechniqueByIndex(0);
		}
		else
		{
			ID3DX11EffectTechnique* hTechnique = pEffectPtr->GetTechniqueByName ( (LPSTR)pTechniqueName );
			if ( hTechnique ) 
			{
				// assign the technique for this effect
				if ( hTechnique->IsValid() )
				{
					pEffectObject->m_hCurrentTechnique = hTechnique;
				}

				// find a pass named 'RenderDepthPixelsPass' and flag if found as we can skip this pass if engine does not use depth related stuff like DOF and MOTION BLUR (performance)
				//pEffectObject->m_DepthRenderPassHandle = pEffectPtr->GetPassByName ( hTechnique, "RenderDepthPixelsPass");
			}
		}
	}
	#else
	if ( pEffectPtr )
	{
		GGHANDLE hTechnique = pEffectPtr->GetTechniqueByName ( (LPSTR)pTechniqueName );
		if ( hTechnique ) 
		{
			// assign the technique for this effect
			pEffectPtr->SetTechnique(hTechnique);

			// 091115 - find a pass named 'RenderDepthPixelsPass' and flag if found as we can skip this pass if engine does not use depth related stuff like DOF and MOTION BLUR (performance)
			pEffectObject->m_DepthRenderPassHandle = pEffectPtr->GetPassByName ( hTechnique, "RenderDepthPixelsPass");
		}
	}
	#endif
	return;
}

DARKSDK_DLL void SetEffectTechniqueEx ( int iEffectID, DWORD dwPtr )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// Get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// Choose technique based on name
	#ifdef DX11
	if ( pEffectPtr )
	{
		ID3DX11EffectTechnique* hTechnique = (ID3DX11EffectTechnique*)dwPtr;
		if ( hTechnique ) 
		{
			// assign the technique for this effect
			if ( hTechnique->IsValid() )
			{
				pEffectObject->m_hCurrentTechnique = hTechnique;
			}
		}
	}
	#else
	#endif
	return;
}

DARKSDK_DLL DWORD GetEffectTechniqueEx ( int iEffectID )
{
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return NULL;

	// Get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// Choose technique based on name
	#ifdef DX11
	if ( pEffectPtr )
	{
		return (DWORD)pEffectObject->m_hCurrentTechnique;
	}
	#else
	#endif
	return NULL;
}

DARKSDK_DLL void SetEffectLODTechnique	( int iEffectID, LPSTR pTechniqueName )
{
	/* experimental idea
	// check the effect exists
	if ( !ConfirmEffect ( iEffectID ) )
		return;

	// Get effect ptr
	LPGGEFFECT pEffectPtr = NULL;
	cSpecialEffect* pEffectObject = m_EffectList [ iEffectID ]->pEffectObj;
	if ( pEffectObject )
		if ( pEffectObject->m_pEffect )
			pEffectPtr = pEffectObject->m_pEffect;

	// Choose technique based on name
	if ( pEffectPtr && pTechniqueName )
	{
		if ( strcmp ( (LPSTR)pTechniqueName, "" )!=NULL )
		{
			// use this when object at LOD distance
			GGHANDLE hTechnique = pEffectPtr->GetTechniqueByName ( (LPSTR)pTechniqueName );
			pEffectObject->m_hLODTechnique = hTechnique;
		}
		else
		{
			// do not override effect based on object distance
			pEffectObject->m_hLODTechnique = NULL;
		}
	}
	else
		return;
	*/
}

DARKSDK_DLL void SetGlobalDepthSkipSystem ( bool bSkipDepthRenderings )
{
	g_bSkipAnyDedicatedDepthRendering = bSkipDepthRenderings;
}

DARKSDK_DLL int GetEffectExist ( int iEffectID )
{
	#ifdef WICKEDENGINE
	// WickedEngine has its own shaders
	return 1;
	#else
	// check the effect exists
	if ( iEffectID < 1 || iEffectID > MAX_EFFECTS )
	{ 
		RunTimeError ( RUNTIMEERROR_B3DEFFECTNUMBERILLEGAL );
		return 0;
	}
	if ( m_EffectList [ iEffectID ] )
		return 1;
	else
		return 0;
	#endif
}

DARKSDK int GetObjectPolygonCount ( int iObjectNumber )
{
	// total count
	int iPolygonTotal = 0;

	// check the object exists
	if ( !ConfirmObject ( iObjectNumber ) )
		return 0;

	// return object information
	sObject* pObject = g_ObjectList [ iObjectNumber ];
	if ( pObject )
	{
		if ( pObject->iMeshCount>0 )
		{
			for ( int iM=0; iM<pObject->iMeshCount; iM++ )
			{
				sMesh* pMesh = pObject->ppMeshList[iM];
				if ( pMesh )
				{
					iPolygonTotal = iPolygonTotal + pMesh->iDrawPrimitives;
				}
			}
		}
	}

	// return total
	return iPolygonTotal;
}

int GetObjectVertexCount ( int iObjectNumber )
{
	// total count
	int iVertexCountTotal = 0;

	// check the object exists
	if ( !ConfirmObject ( iObjectNumber ) )
		return 0;

	// return object information
	sObject* pObject = g_ObjectList [ iObjectNumber ];
	if ( pObject )
	{
		if ( pObject->iMeshCount>0 )
		{
			for ( int iM=0; iM<pObject->iMeshCount; iM++ )
			{
				sMesh* pMesh = pObject->ppMeshList[iM];
				if ( pMesh )
				{
					if ( (int)pMesh->dwVertexCount>iVertexCountTotal )
					{
						iVertexCountTotal = pMesh->dwVertexCount;
					}
				}
			}
		}
	}

	// return total
	return iVertexCountTotal;
}

int GetObjectTotalVertexCount(int iObjectNumber)
{
	// total count
	int iVertexCountTotal = 0;

	// check the object exists
	if (!ConfirmObject(iObjectNumber))
		return 0;

	// return object information
	sObject* pObject = g_ObjectList[iObjectNumber];
	if (pObject)
	{
		if (pObject->iMeshCount > 0)
		{
			for (int iM = 0; iM < pObject->iMeshCount; iM++)
			{
				sMesh* pMesh = pObject->ppMeshList[iM];
				if (pMesh)
				{
					iVertexCountTotal += pMesh->dwVertexCount;
				}
			}
		}
	}

	// return total
	return iVertexCountTotal;
}

// Custom vertex shaders

DARKSDK_DLL void SetVertexShaderOn ( int iID, int iShader )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// validate shader
	if ( iShader < 0 || iShader > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}
	if ( m_VertexShaders [ iShader ].pVertexDec==NULL )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERINVALID);
		return;
	}

	// shader to set
	LPGGVERTEXSHADER pVertexShader = m_VertexShaders [ iShader ].pVertexShader;
	LPGGVERTEXLAYOUT pVertexDec = m_VertexShaders [ iShader ].pVertexDec;

	// apply shader to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetCustomShader ( pObject->ppMeshList [ iMesh ], pVertexShader, pVertexDec, 1 );
}

DARKSDK_DLL void SetVertexShaderOff ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply shader off to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetNoShader ( pObject->ppMeshList [ iMesh ] );
}

DARKSDK_DLL void CloneMeshToNewFormat ( int iID, DWORD dwFVF, DWORD dwEraseBones )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// object ref
	sObject* pObject = g_ObjectList [ iID ];
	if ( pObject==NULL )
		return;

	// create new mesh list to store ALL new meshes (erase bones means lightmapper process)
	if ( dwEraseBones==1 )
	{
		DWORD dwTotalMaterialCount = 0;
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];
			if ( pMesh )
				if ( pMesh->bUseMultiMaterial==true )
					dwTotalMaterialCount+=pMesh->dwMultiMaterialCount;
		}

		// convert object if found to use multimaterial heracy
		if ( dwTotalMaterialCount>0 )
		{
			// new mesh list and ref to connect old frame ptrs to new meshes
			sMesh** pTotalMeshListRef = new sMesh*[dwTotalMaterialCount];
			sMesh** pTotalMeshList = new sMesh*[dwTotalMaterialCount];

			// start off new frame list contents
			int iNewFrameCount = pObject->iFrameCount+dwTotalMaterialCount;
			sFrame** pTotalFrameList = new sFrame*[iNewFrameCount];
			memset ( pTotalFrameList, 0, sizeof(sFrame*)*iNewFrameCount );
			for ( int iFrameIndex=0; iFrameIndex<pObject->iFrameCount; iFrameIndex++ )
				pTotalFrameList [ iFrameIndex ] = pObject->ppFrameList [ iFrameIndex ];
			int iFrameCurrentIndex = pObject->iFrameCount;

			DWORD dwMaterialMeshIndex = 0;
			for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
			{
				sMesh* pMesh = pObject->ppMeshList [ iMesh ];
				if ( pMesh )
				{
					if ( pMesh->bUseMultiMaterial==true )
					{
						// for each material, create a new mesh
						DWORD dwMaterialCount = pMesh->dwMultiMaterialCount;
						sMultiMaterial* pMultiMaterial = pMesh->pMultiMaterial;
						for ( DWORD dwMaterialIndex=0; dwMaterialIndex<dwMaterialCount; dwMaterialIndex++ )
						{
							sMesh* pNewMesh = new sMesh();
							if ( pNewMesh )
							{
								// duplicate mesh
								GGMATRIX matWorld;
								GGMatrixIdentity ( &matWorld );
								MakeMeshFromOtherMesh ( true, pNewMesh, pMesh, &matWorld );

								// copy in correct texture
								if ( pNewMesh->pTextures==NULL )
								{
									pNewMesh->dwTextureCount = 2;
									pNewMesh->pTextures = new sTexture [ pNewMesh->dwTextureCount ];
									memset ( pNewMesh->pTextures, 0, sizeof(sTexture)*pNewMesh->dwTextureCount );
								}
								pNewMesh->pTextures [ 0 ].iImageID  = pMesh->pTextures [ 0 ].iImageID;
								pNewMesh->pTextures [ 0 ].pTexturesRefView = pMultiMaterial [ dwMaterialIndex ].pTexturesRef;
								strcpy ( pNewMesh->pTextures [ 0 ].pName, pMultiMaterial [ dwMaterialIndex ].pName );
								pNewMesh->pTextures [ 0 ].dwBlendMode = GGTOP_MODULATE;
								pNewMesh->pTextures [ 0 ].dwBlendArg1 = GGTA_TEXTURE;
								pNewMesh->pTextures [ 0 ].dwBlendArg2 = GGTA_DIFFUSE;
								pNewMesh->bUseMultiMaterial = false;
								pNewMesh->fSpecularOverride = 1.0f;
								pNewMesh->bUsesMaterial = false;

								// modify index data so mesh only points to revelant polygons
								DWORD dwPolyCount = pMultiMaterial [ dwMaterialIndex ].dwPolyCount;
								if ( pNewMesh->pIndices != NULL )
								{
									// straight copy of relevant indices for this material 
									pNewMesh->dwIndexCount = dwPolyCount*3;
									pNewMesh->iDrawVertexCount = pMesh->iDrawVertexCount;
									pNewMesh->iDrawPrimitives  = dwPolyCount;
									memcpy ( pNewMesh->pIndices, pMesh->pIndices + pMultiMaterial [ dwMaterialIndex ].dwIndexStart, dwPolyCount*3*sizeof(WORD) );
								}
								else
								{
									// mesh exceeded 16bit index buffer, so need to manually copy the relevant verts for vert only mesh
									pNewMesh->dwIndexCount = 0;
									pNewMesh->iDrawVertexCount = dwPolyCount*3;
									pNewMesh->iDrawPrimitives  = dwPolyCount;
									for ( int i = 0; i < dwPolyCount*3; i+=3 )
									{
										int iV0 = pMultiMaterial [ dwMaterialIndex ].dwIndexStart + i + 0;
										int iV1 = pMultiMaterial [ dwMaterialIndex ].dwIndexStart + i + 1;
										int iV2 = pMultiMaterial [ dwMaterialIndex ].dwIndexStart + i + 2;
										*((GGVECTOR3*)pNewMesh->pVertexData+i+0) = *(GGVECTOR3*)pMesh->pVertexData+iV0;
										*((GGVECTOR3*)pNewMesh->pVertexData+i+1) = *(GGVECTOR3*)pMesh->pVertexData+iV1;
										*((GGVECTOR3*)pNewMesh->pVertexData+i+2) = *(GGVECTOR3*)pMesh->pVertexData+iV2;
									}
								}

								// add to new mesh list
								pTotalMeshList [ dwMaterialMeshIndex ] = pNewMesh;
								pTotalMeshListRef [ dwMaterialMeshIndex ] = pMesh;
								dwMaterialMeshIndex++;
							}
						}
					}
				}
			}

			// and replace old mesh list and references with new
			for ( int iMeshIndex=0; iMeshIndex<pObject->iMeshCount; iMeshIndex++ )
			{
				SAFE_DELETE(pObject->ppMeshList[iMeshIndex]);
			}
			SAFE_DELETE(pObject->ppMeshList);
			pObject->iMeshCount = dwTotalMaterialCount;
			pObject->ppMeshList = pTotalMeshList;

			// and update frame references
			for ( int iFrameIndex=0; iFrameIndex<pObject->iFrameCount; iFrameIndex++ )
			{
				sFrame* pFrame = pObject->ppFrameList[iFrameIndex];
				if ( pFrame )
				{
					sMesh* pMeshToReplace = pFrame->pMesh;
					sMesh* pMeshToReplaceWith = NULL;
					if ( pMeshToReplace )
					{
						for ( int iScanMatIndex=0; iScanMatIndex<(int)dwTotalMaterialCount; iScanMatIndex++ )
						{
							if ( pMeshToReplace==pTotalMeshListRef[iScanMatIndex] )
							{
								// found reference to OLD mesh
								pMeshToReplaceWith = pTotalMeshList[iScanMatIndex];
								pTotalMeshListRef[iScanMatIndex] = NULL;
								break;
							}
						}
					}
					pFrame->pMesh = pMeshToReplaceWith;
					if ( pMeshToReplaceWith )
					{
						// also tag 'additional' meshes onto the end as sybling frames
						sFrame* pThisFrame = pFrame;
						for ( int iScanMatIndex=0; iScanMatIndex<(int)dwTotalMaterialCount; iScanMatIndex++ )
						{
							if ( pTotalMeshListRef[iScanMatIndex]==pMeshToReplace )
							{
								while ( pThisFrame->pSibling ) pThisFrame = pThisFrame->pSibling;
								pThisFrame->pSibling = new sFrame();
								pThisFrame->pSibling->matOriginal = pFrame->matOriginal;
								pThisFrame->pSibling->matTransformed = pFrame->matTransformed;
								pThisFrame->pSibling->matCombined = pFrame->matCombined;
								strcpy ( pThisFrame->pSibling->szName, pFrame->szName );
								pThisFrame->pSibling->pMesh = pTotalMeshList[iScanMatIndex];
								// NOTE: Ensure this does not wipe critical REF needed if more meshes are looking for this hierarchy slot
								pTotalMeshListRef[iScanMatIndex] = NULL;
								pTotalFrameList [ iFrameCurrentIndex ] = pThisFrame->pSibling;
								iFrameCurrentIndex++;
								//break;
							}
						}
					}
				}
			}
			SAFE_DELETE ( pObject->ppFrameList );
			pObject->ppFrameList = pTotalFrameList;
			pObject->iFrameCount = iFrameCurrentIndex;

			// free usages
			SAFE_DELETE(pTotalMeshListRef);
		}
	}

	/*
	// Flexible vertex format bits
	#define D3DFVF_RESERVED0        0x001
	#define D3DFVF_POSITION_MASK    0x00E
	#define GGFVF_XYZ              0x002
	#define GGFVF_XYZRHW           0x004
	#define GGFVF_XYZB1            0x006
	#define GGFVF_XYZB2            0x008
	#define GGFVF_XYZB3            0x00a
	#define GGFVF_XYZB4            0x00c
	#define GGFVF_XYZB5            0x00e

	#define GGFVF_NORMAL           0x010
	#define GGFVF_PSIZE            0x020
	#define GGFVF_DIFFUSE          0x040
	#define D3DFVF_SPECULAR         0x080

	#define GGFVF_TEXCOUNT_MASK    0xf00
	#define GGFVF_TEXCOUNT_SHIFT   8
	#define D3DFVF_TEX0             0x000
	#define GGFVF_TEX1             0x100
	#define GGFVF_TEX2             0x200
	#define GGFVF_TEX3             0x300
	#define D3DFVF_TEX4             0x400
	#define D3DFVF_TEX5             0x500
	#define D3DFVF_TEX6             0x600
	#define D3DFVF_TEX7             0x700
	#define D3DFVF_TEX8             0x800
	#define D3DFVF_LASTBETA_UBYTE4  0x1000

    // Typical
    model         = GGFVF_XYZ + GGFVF_NORMAL + GGFVF_TEX1
					0x002 + 0x010 + 0x100 = 0x152 (274)
    model+diffuse = GGFVF_XYZ + GGFVF_NORMAL + GGFVF_TEX1 + GGFVF_DIFFUSE
					0x002 + 0x010 + 0x100 + 0x040 = 0x152 (338)
    model-normal  = GGFVF_XYZ + GGFVF_DIFFUSE + GGFVF_TEX1
					0x002 + 0x040 + 0x100 = (332)
	*/

	// clone mesh to the specific format
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		ConvertToFVF ( pObject->ppMeshList [ iMesh ], dwFVF );

	// lee - 050914 - also remove any bone animation data as converted object cannot animate without correct FVF skinning
	if ( dwEraseBones==1 )
	{
		for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		{
			sMesh* pMesh = pObject->ppMeshList [ iMesh ];
			if ( pMesh->dwBoneCount > 0 )
			{
				// erase the bone data
				SAFE_DELETE_ARRAY ( pMesh->pBones );
				pMesh->dwBoneCount = 0;
			}
		}
	}

	// regenerate buffer instance
	m_ObjectManager.RemoveObjectFromBuffers ( pObject );
	m_ObjectManager.AddObjectToBuffers ( pObject );
}

DARKSDK void CloneMeshToNewFormat ( int iID, DWORD dwFVF )
{
	CloneMeshToNewFormat ( iID, dwFVF, 0 );
}

DARKSDK_DLL void SetVertexShaderStreamCount ( int iID, int iCount )
{
	// set the size of the stream buffer

	// check the id is valid
	if ( iID < 0 || iID > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	// make sure the count is ok
	if ( iCount < 1 )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERCOUNTILLEGAL);
		return;
	}

	// store the count
	m_VertexShaders [ iID ].dwDecArrayCount = iCount;
	
	// allocate memory for the begining and end addons
	DWORD dwSize = iCount + 1;
	if ( ! ( m_VertexShaders [ iID ].pDecArray = new GGVERTEXELEMENT [ dwSize ] ) )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERCANNOTCREATE);
		return;
	}

	// clear declaration data
	memset ( m_VertexShaders [ iID ].pDecArray, 0, sizeof(GGVERTEXELEMENT)*dwSize );

	// add the start and end values
	#ifdef DX11
	#else
	m_VertexShaders [ iID ].pDecArray [ iCount ].Stream = 0xFF;
	m_VertexShaders [ iID ].pDecArray [ iCount ].Offset  = 0;
	m_VertexShaders [ iID ].pDecArray [ iCount ].Type  = D3DDECLTYPE_UNUSED;
	m_VertexShaders [ iID ].pDecArray [ iCount ].Method  = 0;
	m_VertexShaders [ iID ].pDecArray [ iCount ].Usage = 0;
	m_VertexShaders [ iID ].pDecArray [ iCount ].UsageIndex = 0;
	#endif
}

DARKSDK_DLL void SetVertexShaderStream ( int iID, int iPos, int iDataUsage, int iDataType )
{
	// check the id is valid
	if ( iID < 0 || iID > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	// make sure the dec pos is ok
	if ( iPos < 0 || iPos > (int)m_VertexShaders [ iID ].dwDecArrayCount )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERSTREAMPOSINVALID);
		return;
	}

	// check data usage 0-13
	if ( iDataUsage < 0 || iDataUsage > 13 )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERDATAINVALID);
		return;
	}

	#ifdef DX11
	#else
	DWORD dwOffset=0;
	int iIndex=0;
	while ( iIndex<(iPos-1) )
	{
		DWORD dwSize=0;
		switch ( m_VertexShaders [ iID ].pDecArray [ iIndex ].Type )
		{
			case D3DDECLTYPE_FLOAT1 :
			case D3DDECLTYPE_D3DCOLOR :
				dwSize=1;
				break;

			case GGDECLTYPE_FLOAT2 :
			case D3DDECLTYPE_SHORT2 :
				dwSize=2;
				break;

			case GGDECLTYPE_FLOAT3 :
				dwSize=3;
				break;

			case D3DDECLTYPE_FLOAT4 :
			case D3DDECLTYPE_UBYTE4 :
			case D3DDECLTYPE_SHORT4 :
				dwSize=4;
				break;
		}
		dwOffset+=dwSize*4;
		iIndex++;
	}

	// add the dec
	m_VertexShaders [ iID ].pDecArray [ iPos-1 ].Stream = 0;
	m_VertexShaders [ iID ].pDecArray [ iPos-1 ].Offset = dwOffset;
	m_VertexShaders [ iID ].pDecArray [ iPos-1 ].Type = iDataType;
	m_VertexShaders [ iID ].pDecArray [ iPos-1 ].Method = GGDECLMETHOD_DEFAULT;
	m_VertexShaders [ iID ].pDecArray [ iPos-1 ].Usage = iDataUsage;
	m_VertexShaders [ iID ].pDecArray [ iPos-1 ].UsageIndex = 0;
	#endif
}

DARKSDK_DLL void CreateVertexShaderFromFile ( int iID, SDK_LPSTR szFile )
{
	// check the id is valid
	if ( iID < 0 || iID > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	// free any previous shaders
	#ifdef DX11
	#else
	SAFE_RELEASE ( m_VertexShaders [ iID ].pVertexShader );
	SAFE_RELEASE ( m_VertexShaders [ iID ].pVertexDec );

	// compile and create shader
	LPD3DXBUFFER pCode;
	LPD3DXBUFFER pErrorMsg;
	if ( FAILED ( D3DXAssembleShaderFromFile ( (LPSTR)szFile, NULL, NULL, 0, &pCode, &pErrorMsg ) ) )
	{
		//LPSTR pSee = (LPSTR)pErrorMsg->GetBufferPointer();
		RunTimeError(RUNTIMEERROR_B3DVSHADERCANNOTASSEMBLE);
		return;
	}
	if ( FAILED ( m_pD3D->CreateVertexShader ( (DWORD*)pCode->GetBufferPointer ( ), &m_VertexShaders [ iID ].pVertexShader ) ) )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERCANNOTCREATE);
		return;
	}

	// free shader buffer
	pCode->Release ( );

	// create vertex declaration object
	if ( FAILED ( m_pD3D->CreateVertexDeclaration ( m_VertexShaders [ iID ].pDecArray, &m_VertexShaders [ iID ].pVertexDec ) ) )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERCANNOTCREATE);
		return;
	}
	#endif
}

DARKSDK_DLL void SetVertexShaderVector ( int iID, DWORD dwRegister, int iVector, DWORD dwConstantCount )
{
	// vertify shader valid
	if ( iID < 0 || iID > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}
	
	// set constant
	#ifdef DX11
	#else
	GGVECTOR4 vecData = GetVector4 ( iVector );
	m_pD3D->SetVertexShaderConstantF ( dwRegister, (float*)&vecData, 1 );
	#endif
}

DARKSDK_DLL void SetVertexShaderMatrix ( int iID, DWORD dwRegister, int iMatrix, DWORD dwConstantCount )
{
	// vertify shader valid
	if ( iID < 0 || iID > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	// set constant
	#ifdef DX11
	#else
	GGMATRIX matData = GetMatrix ( iMatrix );
	m_pD3D->SetVertexShaderConstantF ( dwRegister, (float*)&matData, 4 );
	#endif
}

DARKSDK_DLL void DeleteVertexShader ( int iShader )
{
	// vertify shader valid
	if ( iShader < 0 || iShader > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	#ifdef DX11
	#else
	// free vertex dec array
	SAFE_DELETE_ARRAY ( m_VertexShaders [ iShader ].pDecArray );

	// free vertex shader objects
	if ( m_VertexShaders [ iShader ].pVertexShader )
	{
		m_VertexShaders [ iShader ].pVertexShader->Release();
		m_VertexShaders [ iShader ].pVertexShader=NULL;
	}
	if ( m_VertexShaders [ iShader ].pVertexDec )
	{
		m_VertexShaders [ iShader ].pVertexDec->Release();
		m_VertexShaders [ iShader ].pVertexDec=NULL;
	}
	#endif
}

DARKSDK_DLL void SetPixelShaderOn ( int iID, int iShader )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// verify shader
	if ( iShader < 0 || iShader > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	// apply shader to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetCustomPixelShader ( pObject->ppMeshList [ iMesh ], m_PixelShaders [ iShader ].pPixelShader );
}

DARKSDK_DLL void SetPixelShaderOff ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// apply shader off to all meshes
	sObject* pObject = g_ObjectList [ iID ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetNoPixelShader ( pObject->ppMeshList [ iMesh ] );
}

DARKSDK_DLL void SetPixelShaderTexture ( int iShaderObject, int iSlot, int iTexture )
{
	// check the object exists
	if ( !ConfirmObject ( iShaderObject ) )
		return;

	// apply shader to all meshes
	sObject* pObject = g_ObjectList [ iShaderObject ];
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
		SetMultiTexture ( pObject->ppMeshList [ iMesh ], iSlot, GGTOP_MODULATE, 0, iTexture );

	// trigger a ew-new and re-sort
	m_ObjectManager.RenewReplacedMeshes ( pObject );
	m_ObjectManager.UpdateTextures ( );
}

DARKSDK_DLL void CreatePixelShaderFromFile ( int iID, SDK_LPSTR szFile )
{
	// check the id is valid
	if ( iID < 0 || iID > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	#ifdef DX11
	#else
	// free any previous shaders
	SAFE_RELEASE ( m_PixelShaders [ iID ].pPixelShader );

	// compile and create shader
	LPD3DXBUFFER pCode;
	if ( FAILED ( D3DXAssembleShaderFromFile ( (LPSTR)szFile, 0, NULL, 0, &pCode, NULL ) ) )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERCANNOTASSEMBLE);
		return;
	}
	if ( FAILED ( m_pD3D->CreatePixelShader ( (DWORD*)pCode->GetBufferPointer(), &m_PixelShaders [ iID ].pPixelShader ) ) )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERCANNOTCREATE);
		return;
	}

	// free buffer
	pCode->Release ( );
	#endif
}

DARKSDK_DLL void DeletePixelShader ( int iShader )
{
	// vertify shader valid
	if ( iShader < 0 || iShader > MAX_VERTEX_SHADERS )
	{
		RunTimeError(RUNTIMEERROR_B3DVSHADERNUMBERILLEGAL);
		return;
	}

	// delete pixel shader
	#ifdef DX11
	#else
	if ( m_PixelShaders [ iShader ].pPixelShader )
	{
		m_PixelShaders [ iShader ].pPixelShader->Release();
		m_PixelShaders [ iShader ].pPixelShader=NULL;
	}
	#endif
}

// Collision Commands

DARKSDK_DLL void SetObjectCollisionOn ( int iID )
{
}

DARKSDK_DLL void SetObjectCollisionOff ( int iID )
{
}

DARKSDK_DLL void MakeCollisionBox ( int iID, float iX1, float iY1, float iZ1, float iX2, float iY2, float iZ2, int iRotatedBoxFlag )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	SetColBox( pObject, iX1, iY1, iZ1, iX2, iY2, iZ2, iRotatedBoxFlag );
}

DARKSDK_DLL void DeleteCollisionBox ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	FreeColBox ( pObject );
}

DARKSDK_DLL void SetCollisionToSpheres ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	SetColToSpheres ( pObject );
}

DARKSDK_DLL void SetCollisionToBoxes ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	SetColToBoxes ( pObject );
}

DARKSDK_DLL void SetCollisionToPolygons ( int iID )
{
	// check the object exists
	if ( !ConfirmObject ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	SetColToPolygons ( pObject );
}

DARKSDK_DLL void SetSphereRadius ( int iID, float fRadius )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iID ];
	SetSphereRadius ( pObject, fRadius );
}

DARKSDK_DLL void SetGlobalCollisionOn ( void )
{
	GlobalColOn();
}

DARKSDK_DLL void SetGlobalCollisionOff ( void )
{
	GlobalColOff();
}

DARKSDK_DLL float IntersectObjectCore ( sObject* pObject, float fX, float fY, float fZ, float fNewX, float fNewY, float fNewZ, int iIgnoreAllButLastFrame )
{
	// object must have its world data calculated
	CalcObjectWorld ( pObject );

	// do intersect check
	return CheckIntersectObject ( pObject, fX, fY, fZ, fNewX, fNewY, fNewZ, iIgnoreAllButLastFrame );
}

DARKSDK_DLL float IntersectObject ( int iObjectID, float fX, float fY, float fZ, float fNewX, float fNewY, float fNewZ )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iObjectID ) )
		return 0;

	// get object pointer
	sObject* pObject = g_ObjectList [ iObjectID ];

	// do intersect check
	float fDistance=IntersectObjectCore ( pObject, fX, fY, fZ, fNewX, fNewY, fNewZ, 0 );

	// and must be within distance of specified vectors
	float fDistanceBetweenPoints;
	GGVECTOR3 vec3value = GGVECTOR3(fX,fY,fZ) - GGVECTOR3(fNewX,fNewY,fNewZ);
	fDistanceBetweenPoints = GGVec3Length(&vec3value);
	if ( fDistance > fDistanceBetweenPoints ) fDistance=0.0f;
	return fDistance;
}

bool intersectRayAABox2(const IntersectRay &ray, const IntersectBox &box, int& tnear, int& tfar)
{
    GGVECTOR3 T_1, T_2; // vectors to hold the T-values for every direction
    double t_near = -DBL_MAX; // maximums defined in float.h
    double t_far = DBL_MAX;

    for (int i = 0; i < 3; i++)
	{ 
		//we test slabs in every direction
        if (ray.direction[i] == 0)
		{ 
			// ray parallel to planes in this direction
            if ((ray.origin[i] < box.min[i]) || (ray.origin[i] > box.max[i])) 
			{
                return false; // parallel AND outside box : no intersection possible
            }
        } 
		else 
		{ 
			// ray not parallel to planes in this direction
            T_1[i] = (box.min[i] - ray.origin[i]) / ray.direction[i];
            T_2[i] = (box.max[i] - ray.origin[i]) / ray.direction[i];

            if(T_1[i] > T_2[i])
			{ 
				// we want T_1 to hold values for intersection with near plane
                std::swap(T_1,T_2);
            }
            if (T_1[i] > t_near)
			{
                t_near = T_1[i];
            }
            if (T_2[i] < t_far)
			{
                t_far = T_2[i];
            }
            if( (t_near > t_far) || (t_far < 0) )
			{
                return false;
            }
        }
    }
    tnear = t_near; tfar = t_far; // put return values in place
    return true; // if we made it here, there was an intersection - YAY
}

DARKSDK_DLL int IntersectAll_OLD ( int iPrimaryStart, int iPrimaryEnd, float fX, float fY, float fZ, float fNewX, float fNewY, float fNewZ, int iIgnoreObjNo )
{
	// special iIgnoreObjNo mode
	if ( iIgnoreObjNo==-123 || iIgnoreObjNo==-124 )
	{
		if ( iIgnoreObjNo==-123 )
		{
			// obtain second range of objects to check
			g_iIntersectAllSecondStart = iPrimaryStart;
			g_iIntersectAllSecondEnd = iPrimaryEnd;
			#ifdef SKIPGRIDUSED
			g_fIntersectAllSkipGridX = fX;
			g_fIntersectAllSkipGridZ = fZ;
			#endif
			return 0;
		}
		else
		{
			// obtain third range of objects to check
			g_iIntersectAllThirdStart = iPrimaryStart;
			g_iIntersectAllThirdEnd = iPrimaryEnd;
			#ifdef SKIPGRIDUSED
			g_fIntersectAllSkipGridX = fX;
			g_fIntersectAllSkipGridZ = fZ;
			#endif
			return 0;
		}
	}
	else
	{
		// detect if the ray is recorded in skipgrid as blocked
		#ifdef SKIPGRIDUSED
		if ( g_fIntersectAllSkipGridX>0 )
		{
			int iSkipGridRefFromX = fX/50.0f;
			int iSkipGridRefFromZ = fZ/50.0f;
			if ( iSkipGridRefFromX < 0 ) iSkipGridRefFromX=0;
			if ( iSkipGridRefFromX > 1023 ) iSkipGridRefFromX=1023;
			if ( iSkipGridRefFromZ < 0 ) iSkipGridRefFromZ=0;
			if ( iSkipGridRefFromZ > 1023 ) iSkipGridRefFromZ=1023;
			WORD wTargetX = fNewX/10.0f;
			WORD wTargetZ = fNewZ/10.0f;
			DWORD dwSkipGridValue = g_dwSkipGrid[iSkipGridRefFromX][iSkipGridRefFromZ];
			if ( dwSkipGridValue > 0 )
			{
				WORD wRefX = (dwSkipGridValue & 0xFFFF0000) >> 16;
				WORD wRefZ = (dwSkipGridValue & 0x0000FFFF);
				if ( wRefX==wTargetX && wRefZ==wTargetZ )
				{
					// this target was found to be blocked from this coordinate
					return g_iSkipGridResult[iSkipGridRefFromX][iSkipGridRefFromZ];
				}
			}
		}
		#endif
	}

	// return value (0=no hit, >0=object number, -1=other geometry)
	int iHitValue = 0;

	// work out length of ray
	GGVECTOR3 vec3value = GGVECTOR3(fX,fY,fZ) - GGVECTOR3(fNewX,fNewY,fNewZ);
	float fDistanceBetweenPoints = GGVec3Length(&vec3value);

	// Dave - Added these declarations here, since the above routine isn't used anymore
	float fBestDistance = 999999.9f;
	GlobChecklistStruct pBestHit[10];
	
	// DAVE 9122014 This block was commented out, but put back in so it checks every object in the list
	// go through all objects presently in scene
	for ( int iPass=0; iPass<3; iPass++ )
	{
		int iStart, iEnd;
		if ( iPass==0 ) { iStart=iPrimaryStart; iEnd=iPrimaryEnd; }
		if ( iPass==1 ) { iStart=g_iIntersectAllSecondStart; iEnd=g_iIntersectAllSecondEnd; if ( iStart == 0 ) continue; }
		if ( iPass==2 ) { iStart=g_iIntersectAllThirdStart; iEnd=g_iIntersectAllThirdEnd; if ( iStart == 0 ) break; }
		for ( int iObjectID = iStart; iObjectID <= iEnd; iObjectID++ )
		{
			// make sure we have a valid object
			sObject* pObject = g_ObjectList [ iObjectID ];
			if ( pObject )
			{
				// check if object is excluded
				// check if object in dead state (non collisin detectable)
				//Dave added a skip for hidden objects
				if ( pObject->dwObjectNumber==iIgnoreObjNo || pObject->collision.dwCollisionPropertyValue==1 || !pObject->bVisible )
				{
					// ignore this object (usually the caster)
				}
				else
				{
					// check if object in same 'region' as ray
					float fDX=0, fDY=0, fDZ=0;
					if ( pObject->position.iGluedToObj>0 )
					{
						// use parent object instead
						sObject* pParentObject = g_ObjectList [ pObject->position.iGluedToObj ];
						if ( pParentObject )
						{
							fDX = pParentObject->position.vecPosition.x - fX;
							fDY = pParentObject->position.vecPosition.y - fY;
							fDZ = pParentObject->position.vecPosition.z - fZ;
						}
					}
					else
					{
						fDX = pObject->position.vecPosition.x - fX;
						fDY = pObject->position.vecPosition.y - fY;
						fDZ = pObject->position.vecPosition.z - fZ;
					}
					float fDist = sqrt(fabs(fDX*fDX)+fabs(fDY*fDY)+fabs(fDZ*fDZ));
					if ( fDist <= ((pObject->collision.fLargestRadius*3)+fDistanceBetweenPoints) )
					{
						// check if ray intersects object bound box (ray vs box)
						IntersectBox box;
						box.min[0] = pObject->position.vecPosition.x + pObject->collision.vecMin.x;
						box.min[1] = pObject->position.vecPosition.y + pObject->collision.vecMin.y;
						box.min[2] = pObject->position.vecPosition.z + pObject->collision.vecMin.z;
						box.max[0] = pObject->position.vecPosition.x + pObject->collision.vecMax.x;
						box.max[1] = pObject->position.vecPosition.y + pObject->collision.vecMax.y;
						box.max[2] = pObject->position.vecPosition.z + pObject->collision.vecMax.z;
						if ( true )//intersectRayAABox2(ray, box, tnear, tfar)==true )
						{
							// do intersect check
							float fDistance = IntersectObjectCore ( pObject, fX, fY, fZ, fNewX, fNewY, fNewZ, 0 );
							if ( fDistance > 0 && fDistance < fBestDistance )
							{
								// and must be within distance of specified vectors
								if ( fDistance <= fDistanceBetweenPoints )
								{
									// only if object in detection range
									int iObjectHit = pObject->dwObjectNumber;
									if ( iObjectHit >= iStart && iObjectHit <= iEnd )
									{
										// object was intersected, return obj number
										iHitValue = iObjectHit;

										// populate checklist with extra hit info
										// 0 - frame indexes (A = mesh number, B = related frame number (if A is bone mesh))
										// 1 - vertex indexes
										// 2 - object-space coordinate of V0
										// 3 - object-space coordinate of V1
										// 4 - object-space coordinate of V2
										// 5 - world space coordinate where the collision struck!
										// 6 - normal of polygon struck (from vertA)
										// 7 - reflection normal based on angle of impact
										// 8 - normal vector?
										for ( int iI=0; iI<9; iI++ ) pBestHit [ iI ] = g_pGlob->checklist [ iI ];

										// find closest distance
										fBestDistance = fDistance;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	

	// if a hit was detected
	if ( iHitValue!=0 )
	{
		// copy best hit info back to checklist
		for ( int iI=0; iI<9; iI++ )
		{
			LPSTR pSaveStr = g_pGlob->checklist [ iI ].string;
			g_pGlob->checklist [ iI ] = pBestHit [ iI ];
			g_pGlob->checklist [ iI ].string = pSaveStr;
			if ( pSaveStr ) strcpy ( pSaveStr, "" );
		}
	}

	#ifdef SKIPGRIDUSED
	// also record in skipgrid (for optimized future collisions)
	int iSkipGridRefFromX = fX/50.0f;
	int iSkipGridRefFromZ = fZ/50.0f;
	if ( iSkipGridRefFromX < 0 ) iSkipGridRefFromX=0;
	if ( iSkipGridRefFromX > 1023 ) iSkipGridRefFromX=1023;
	if ( iSkipGridRefFromZ < 0 ) iSkipGridRefFromZ=0;
	if ( iSkipGridRefFromZ > 1023 ) iSkipGridRefFromZ=1023;
	WORD wTargetX = fNewX/10.0f;
	WORD wTargetZ = fNewZ/10.0f;
	DWORD dwSkipGridValue = (wTargetX<<16) + (wTargetZ);
	g_dwSkipGrid[iSkipGridRefFromX][iSkipGridRefFromZ] = dwSkipGridValue;
	g_iSkipGridResult[iSkipGridRefFromX][iSkipGridRefFromZ] = iHitValue;
	#endif

	// incase we dont want the third list next time
	g_iIntersectAllThirdStart = 0;

	// return hit value depending on what was hit
	return iHitValue;
}

// performance work
DWORD g_dwIntersectDatabaseSize = 0;
DWORD* g_pIntersectDatabase = NULL;
int* g_pIntersectDatabaseLastResult = NULL;
bool g_bForceActualCheckNextTime = false;

//This version combines the orignal method with the shortlist of boxes checked to provide the best of both versions
// objects IntersectAllEx also leaves out of its full-accuracy pick (main thread only), besides iIgnoreObjNo; the Lua
// intersect commands fill it for one call from an ignore table and empty it afterwards
std::vector<int> g_IntersectExtraIgnoreObjects;

DARKSDK_DLL int IntersectAllEx ( int iPrimaryStart, int iPrimaryEnd, float fX, float fY, float fZ, float fNewX, float fNewY, float fNewZ, int iIgnoreObjNo, int iStaticOnly, int iIndexInIntersectDatabase, int iLifeInMilliseconds, int iIgnorePlayerCapsule, bool bFullWickedAccuracy, bool bThreadSafe)
{
	// shiny new system for intersect tests
	// iStaticOnly : 0-dynamic, 1-static, 2-line of sight only performant from LUA scripts

	// return value (0=no hit, >0=object number, -1=other geometry)
	int iHitValue = 0;

	// wicked based intersect test with speed-up database
	if (g_pIntersectDatabase == NULL )
	{
		// create if not exist
		if (g_pIntersectDatabase) delete g_pIntersectDatabase;
		if (g_pIntersectDatabaseLastResult) delete g_pIntersectDatabaseLastResult;
		g_dwIntersectDatabaseSize = 50001;// g.entityelementmax; generous database
		g_pIntersectDatabase = new DWORD[g_dwIntersectDatabaseSize];
		g_pIntersectDatabaseLastResult = new int[g_dwIntersectDatabaseSize];
		memset(g_pIntersectDatabase, 0, sizeof(DWORD)*g_dwIntersectDatabaseSize);
		memset(g_pIntersectDatabaseLastResult, 0, sizeof(int)*g_dwIntersectDatabaseSize);

		// block everything initially, prevents characters being able to see through everything at start of level
		for (int i = 0; i < g_dwIntersectDatabaseSize; i++)
		{
			g_pIntersectDatabaseLastResult[i] = -1;
		}
	}
	if (iIndexInIntersectDatabase > 50000) iIndexInIntersectDatabase = 50000;
	bool bUseHitResultFromIntersectDatabase = false;
	if (iIndexInIntersectDatabase > 0 && g_bForceActualCheckNextTime == false)
	{
		// special code to wipe out database record for when switch targets
		if (iLifeInMilliseconds == -1)
		{
			// skip database early exit so can generate new result right after target switch
			g_pIntersectDatabase[iIndexInIntersectDatabase] = timeGetTime() + 30.0f;// -1.0; need to ensure when reset then immediately call rayscan, we calculate the ray live!!
			g_bForceActualCheckNextTime = true;
			return g_pIntersectDatabaseLastResult[iIndexInIntersectDatabase];
		}
		else
		{
			// update intersect database entry using extra thread
			if (timeGetTime() > g_pIntersectDatabase[iIndexInIntersectDatabase])
			{
				g_pIntersectDatabase[iIndexInIntersectDatabase] = timeGetTime() + iLifeInMilliseconds;
				sIntersectDatabaseExtraThreadItem item;
				item.iPrimaryStart = iPrimaryStart;
				item.iPrimaryEnd = iPrimaryEnd;
				item.fX = fX;
				item.fY = fY;
				item.fZ = fZ;
				item.fNewX = fNewX;
				item.fNewY = fNewY;
				item.fNewZ = fNewZ;
				item.iIgnoreObjNo = iIgnoreObjNo;
				item.iStaticOnly = iStaticOnly;
				item.iIndexInIntersectDatabase = iIndexInIntersectDatabase;
				item.iLifeInMilliseconds = iLifeInMilliseconds;
				item.iIgnorePlayerCapsule = iIgnorePlayerCapsule;
				item.bFullWickedAccuracy = bFullWickedAccuracy;
				while(!g_IntersectDatabaseExtraThreadItemListLock.Acquire()) {}
				g_IntersectDatabaseExtraThreadItemList.push_back(item);
				g_IntersectDatabaseExtraThreadItemListLock.Release();
			}

			// we can use last result
			bUseHitResultFromIntersectDatabase = true;
			iHitValue = g_pIntersectDatabaseLastResult[iIndexInIntersectDatabase];
			return iHitValue;
		}
	}

	// static mode 2 can use regular static test, just as fast!
	if (iStaticOnly == 2) iStaticOnly = 1;

	// visible geometry ray detection
	g_bForceActualCheckNextTime = false;
	if (bFullWickedAccuracy==false)
	{
		GGVECTOR3 vecFrom = GGVECTOR3(fX, fY, fZ);
		GGVECTOR3 vecTo = GGVECTOR3(fNewX, fNewY, fNewZ);
		GGVECTOR3 vecDir = vecTo - vecFrom;
		float pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ;
		float fDistanceOfRay = GGVec3Length(&vecDir);
		DWORD dwObjectNumberHit = 0;
		extern int ODERayEx (float fX, float fY, float fZ, float fToX, float fToY, float fToZ, int iCollisionType, int iGObj1, int iIgObj2);
		int iCollisionMode = 1 | (1 << (1)) | (1 << (2)) | (1 << (3)); // COL_TERRAIN | COL_OBJECT | COL_CAPSULECHAR | COL_OBJECT_DYNAMIC;
		if (iStaticOnly == 1 ) iCollisionMode = 1 | (1 << (1)); // COL_TERRAIN | COL_OBJECT
		if (ODERayEx (vecFrom.x, vecFrom.y, vecFrom.z, fNewX, fNewY, fNewZ, iCollisionMode, iIgnoreObjNo, g_iCurrentGunObj) == 1)
		{
			extern int g_hitObjectNumber;
			dwObjectNumberHit = g_hitObjectNumber;
			extern void ODEGetPosAndNorm(float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ);
			ODEGetPosAndNorm(&pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ);
			// only objects within given range (to exclude weapon HUDs)
			if (dwObjectNumberHit >= iPrimaryStart && dwObjectNumberHit <= iPrimaryEnd)
			{
				// ray hit object
				iHitValue = dwObjectNumberHit; 
			}
			else
			{
				if (dwObjectNumberHit == 0)
				{
					// ray hit terrain
					iHitValue = -1; 
				}
				else
				{
					// hit some other non-entity object (weapon, etc)
				}
			}
			if (iHitValue != 0 && !bThreadSafe) //PE: Not needed in thread, just ruin the checklist for mainthread.
			{
				// 0 - limb struck, not known to the physics ray
				g_pGlob->checklist[0].valueb = -1;

				// 5 - world space coordinate where the collision struck!
				g_pGlob->checklist[5].fvaluea = pOutX;
				g_pGlob->checklist[5].fvalueb = pOutY;
				g_pGlob->checklist[5].fvaluec = pOutZ;

				// 6 - normal of polygon struck (from vertA)
				g_pGlob->checklist[6].fvaluea = pNormX;
				g_pGlob->checklist[6].fvalueb = pNormY;
				g_pGlob->checklist[6].fvaluec = pNormZ;
			}
		}
	}
	else
	{
		// Wicked Raycast TOO expensive, try physics again for performance!
		if (iStaticOnly != 2)
		{
			// timed on the main thread (GetEngineProbe "pick", and "pick layers" for the layer changes)
			WickedCallProbeScope probePick(bThreadSafe ? -1 : WICKEDCALL_PROBE_PICK);
			double dLayersStart = WickedCall_ProbeNow();
			// wicked uses own ray cast which handles objects AND terrain ( a little on the slow side, see above for less overkill )
			GGVECTOR3 vecFrom = GGVECTOR3(fX, fY, fZ);
			GGVECTOR3 vecTo = GGVECTOR3(fNewX, fNewY, fNewZ);
			GGVECTOR3 vecDir = vecTo - vecFrom;
			float pOutX, pOutY, pOutZ, pNormX, pNormY, pNormZ;
			sObject* pIgnoreObject = NULL;
			if (iIgnoreObjNo > 0) pIgnoreObject = GetObjectData(iIgnoreObjNo);
			if (pIgnoreObject) WickedCall_SetObjectRenderLayer(pIgnoreObject, GGRENDERLAYERS_CURSOROBJECT);
			sObject* pGunObject = NULL;
			if (g_iCurrentGunObj > 0 && ObjectExist(g_iCurrentGunObj) == 1)
			{
				pGunObject = GetObjectData(g_iCurrentGunObj);
				WickedCall_SetObjectRenderLayer(pGunObject, GGRENDERLAYERS_CURSOROBJECT);
			}
			double dLayers = WickedCall_ProbeNow() - dLayersStart;
			float fDistanceOfRay = GGVec3Length(&vecDir);
			DWORD dwObjectNumberHit = 0;
			bool bRes = false;
			// GGMAX 2.48 diagnostic: when the pick's closest hit is DISCARDED for being outside
			// the entity range (the else-else below), the caller sees a total miss and cannot
			// tell it from empty air. Record who swallowed the ray so DUMP_SHOT can name it.
			extern DWORD g_ggLastRayBlockedBy;
			g_ggLastRayBlockedBy = 0;
			#ifdef PICKBVHTHREADED
			if (bThreadSafe)
				bRes = WickedCall_SentRay4_ThreadSafe(vecFrom.x, vecFrom.y, vecFrom.z, vecDir.x, vecDir.y, vecDir.z, fDistanceOfRay, &pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ, &dwObjectNumberHit, true);
			else
				bRes = WickedCall_SentRay4(vecFrom.x, vecFrom.y, vecFrom.z, vecDir.x, vecDir.y, vecDir.z, fDistanceOfRay, &pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ, &dwObjectNumberHit, true);
			#else
			bRes = WickedCall_SentRay4(vecFrom.x, vecFrom.y, vecFrom.z, vecDir.x, vecDir.y, vecDir.z, fDistanceOfRay, &pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ, &dwObjectNumberHit, true);
			#endif
			if(bRes == true)
			{
				// only objects within given range (to exclude weapon HUDs)
				if (dwObjectNumberHit >= iPrimaryStart && dwObjectNumberHit <= iPrimaryEnd)
				{
					// if hit the caster (ignored obj), move start along ray to avoid this accidental collision
					if (dwObjectNumberHit == iIgnoreObjNo)
					{
						// try five times (50 units outward)
						GGVECTOR3 vecDiff = vecDir;
						vecDiff /= fDistanceOfRay;
						int iTries = 5;
						while (iTries > 0)
						{
							vecFrom += vecDiff * 10.0f;
							vecDir -= vecDiff * 10.0f;
							dwObjectNumberHit = 0;
							bool bResesult = false;
							#ifdef PICKBVHTHREADED
							if (bThreadSafe)
								bResesult = WickedCall_SentRay4_ThreadSafe(vecFrom.x, vecFrom.y, vecFrom.z, vecDir.x, vecDir.y, vecDir.z, fDistanceOfRay, &pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ, &dwObjectNumberHit, true, pIgnoreObject, pGunObject);
							else
								bResesult = WickedCall_SentRay4(vecFrom.x, vecFrom.y, vecFrom.z, vecDir.x, vecDir.y, vecDir.z, fDistanceOfRay, &pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ, &dwObjectNumberHit, true);
							#else
								bResesult = WickedCall_SentRay4(vecFrom.x, vecFrom.y, vecFrom.z, vecDir.x, vecDir.y, vecDir.z, fDistanceOfRay, &pOutX, &pOutY, &pOutZ, &pNormX, &pNormY, &pNormZ, &dwObjectNumberHit, true);
							#endif
							if (bResesult == true)
							{
								// did we hit an object
								if (dwObjectNumberHit >= iPrimaryStart && dwObjectNumberHit <= iPrimaryEnd)
								{
									if (dwObjectNumberHit != iIgnoreObjNo)
									{
										// found a non-ignored hit, we can leave!
										break;
									}
								}
							}
							iTries--;
						}
					}
					iHitValue = dwObjectNumberHit; // ray hit object!
				}
				else
				{
					if (dwObjectNumberHit == 0)
					{
						iHitValue = -1; // ray hit terrain!
					}
					else
					{
						// hit some other non-entity object (weapon, etc)
						// GGMAX 2.48: this discard is the ray being SWALLOWED - record the culprit
						g_ggLastRayBlockedBy = dwObjectNumberHit;
					}
				}
				if (iHitValue != 0 && !bThreadSafe)
				{
					// 0 - limb struck (read by entity_hasbulletrayhit as the limb hit)
					extern int g_iWickedCallRayLimbHit;
					g_pGlob->checklist[0].valueb = (iHitValue > 0) ? g_iWickedCallRayLimbHit : -1;

					// 5 - world space coordinate where the collision struck!
					g_pGlob->checklist[5].fvaluea = pOutX;
					g_pGlob->checklist[5].fvalueb = pOutY;
					g_pGlob->checklist[5].fvaluec = pOutZ;

					// 6 - normal of polygon struck (from vertA)
					g_pGlob->checklist[6].fvaluea = pNormX;
					g_pGlob->checklist[6].fvalueb = pNormY;
					g_pGlob->checklist[6].fvaluec = pNormZ;
				}
			}

			// we can extend this to exclude MANY objects - and have an 'exclude from ray' flag instead of using GGRENDERLAYERS (optimization op)
			dLayersStart = WickedCall_ProbeNow();
			if (pIgnoreObject && !bThreadSafe) WickedCall_SetObjectRenderLayer(pIgnoreObject, GGRENDERLAYERS_NORMAL);
			if (!bThreadSafe)
			{
				for (int i = 0; i < (int)g_IntersectExtraIgnoreObjects.size(); i++)
				{
					int iObj = g_IntersectExtraIgnoreObjects[i];
					if (iObj > 0 && iObj != iIgnoreObjNo && ObjectExist(iObj) == 1) WickedCall_SetObjectRenderLayer(GetObjectData(iObj), GGRENDERLAYERS_NORMAL);
				}
			}
			if (pGunObject && !bThreadSafe) WickedCall_SetObjectRenderLayer(pGunObject, GGRENDERLAYERS_NORMAL);
			if (!bThreadSafe) WickedCall_ProbeAdd(WICKEDCALL_PROBE_PICK_LAYERS, dLayers + WickedCall_ProbeNow() - dLayersStart);
		}
	}

	// store in database if using this mode
	if (iIndexInIntersectDatabase > 0)
	{
		if (iLifeInMilliseconds < 0) iLifeInMilliseconds = 0;
		if (timeGetTime() > g_pIntersectDatabase[iIndexInIntersectDatabase])
		{
			// but ONLY when the previous time has expired (so the reset can affect ALL ray tests such as when scanning for new enemy)
			g_pIntersectDatabase[iIndexInIntersectDatabase] = timeGetTime() + iLifeInMilliseconds;
			g_pIntersectDatabaseLastResult[iIndexInIntersectDatabase] = iHitValue;
		}
	}

	// ensure the checklist hack to carry results back ensure the checklist qty does not force a return of zero (nasty bug this one)!
	if(!bThreadSafe)
		g_pGlob->checklistqty = 7;

	// return hit value
	return iHitValue;
}

DARKSDK_DLL void ProcessIntersectDatabaseExtraThreadItemList ( void )
{
	// go through all extra thread items and work out intersections for perforant intersect all command
	if (g_pIntersectDatabase)
	{
		while (!g_IntersectDatabaseExtraThreadItemListLock.Acquire()) {}
		if (g_IntersectDatabaseExtraThreadItemList.size() > 0)
		{
			for (int i = 0; i < (int)g_IntersectDatabaseExtraThreadItemList.size(); i++)
			{
				sIntersectDatabaseExtraThreadItem* pItem = &g_IntersectDatabaseExtraThreadItemList[i];
				int iHitValue = IntersectAllEx(pItem->iPrimaryStart, pItem->iPrimaryEnd, pItem->fX, pItem->fY, pItem->fZ, pItem->fNewX, pItem->fNewY, pItem->fNewZ, pItem->iIgnoreObjNo, 0, 0, 0, pItem->iIgnorePlayerCapsule, pItem->bFullWickedAccuracy, true);
				g_pIntersectDatabaseLastResult[pItem->iIndexInIntersectDatabase] = iHitValue;
			}
			g_IntersectDatabaseExtraThreadItemList.clear();
		}
		g_IntersectDatabaseExtraThreadItemListLock.Release();
	}
}

DARKSDK_DLL void ResetIntersectDatabaseExtraThreadItemList (void)
{
	if (g_pIntersectDatabase)
	{
		while (!g_IntersectDatabaseExtraThreadItemListLock.Acquire()) {}
		g_IntersectDatabaseExtraThreadItemList.clear();
		for (int i = 0; i < g_dwIntersectDatabaseSize; i++)
		{
			g_pIntersectDatabaseLastResult[i] = -1;
		}
		g_IntersectDatabaseExtraThreadItemListLock.Release();
	}
}

DARKSDK_DLL int IntersectAll(int iPrimaryStart, int iPrimaryEnd, float fX, float fY, float fZ, float fNewX, float fNewY, float fNewZ, int iIgnoreObjNo)
{
	return IntersectAllEx(iPrimaryStart, iPrimaryEnd, fX, fY, fZ, fNewX, fNewY, fNewZ, iIgnoreObjNo, 0, 0, 0, 1, false);
}

DARKSDK void SetObjectCollisionProperty ( int iObjectID, int iPropertyValue )
{
	// check the object exists
	if ( !ConfirmObjectInstance ( iObjectID ) )
		return;

	// assign property value
	sObject* pObject = g_ObjectList [ iObjectID ];
	pObject->collision.dwCollisionPropertyValue = iPropertyValue;
}

// Cool Auto-Collision Commands

DARKSDK_DLL void AutomaticObjectCollision ( int iObjectID, float fRadius, int iResponse )
{
	// check the object exists
	if ( !ConfirmObject ( iObjectID ) )
		return;

	// get object pointer
	sObject* pObject = g_ObjectList [ iObjectID ];
	AutoObjectCol ( pObject, fRadius, iResponse );
}

DARKSDK_DLL void AutomaticCameraCollision ( int iCameraID, float fRadius, int iResponse, int iStandGroundMode )
{
	AutoCameraCol ( iCameraID, fRadius, iResponse, iStandGroundMode );
}

DARKSDK_DLL void AutomaticCameraCollision ( int iCameraID, float fRadius, int iResponse )
{
	AutoCameraCol ( iCameraID, fRadius, iResponse );
}

DARKSDK_DLL void ForceAutomaticEnd ( void )
{
	// leeadd - 080604 - required to find new camera/obj position before sync!
	AutomaticEnd ();
}

DARKSDK_DLL void HideBounds ( int iID )
{
}

DARKSDK_DLL void ShowBoundsEx ( int iID, int iBoxOnly )
{
}

DARKSDK_DLL void ShowBounds ( int iID )
{
}

DARKSDK_DLL void ShowBounds ( int iID, int iLimb )
{
}

