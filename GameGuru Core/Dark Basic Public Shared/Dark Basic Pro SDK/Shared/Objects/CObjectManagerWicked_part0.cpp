//
// CObjectManager Wicked Functions Implementation
//
//#include "..\..\GameGuru\Include\preprocessor-flags.h"

// Includes
#include "CObjectsC.h"

// conflicts with old graphics engine defines of certain datatypes!
// instead call functions from Wicked-MAX to do all Wicked Calls!
//#include "WickedEngine.h"
#include "..\..\..\..\Guru-WickedMAX\wickedcalls.h"
#include <unordered_map>

// Globals from OLD OBJECT MANAGER!
std::vector< sMesh* > g_vRefreshMeshList;
bool g_ForceTextureListUpdate = false;
float g_fShrinkObjectsTo = 0.0f;
int	g_iSortedObjectCount;
GGHANDLE g_pMainCameraDepthHandle = NULL;
LPGGEFFECT g_pMainCameraDepthEffect = NULL;
bool g_bSkipAnyDedicatedDepthRendering = false;
sObject** g_ppSortedObjectList = NULL;

// Class functions
bool CObjectManager::Setup ( void )
{
	/*
	// Render State Global Defaults
    memset( &m_RenderStates, 0, sizeof( m_RenderStates ) );
	m_RenderStates.dwGlobalCullDirection = GGCULL_CCW;

	// set all pointers to null
	m_ppCurrentVBRef	= NULL;
	m_ppLastVBRef		= NULL;
	m_ppCurrentIBRef	= NULL;
	m_ppLastIBRef		= NULL;

	m_dwCurrentShader = 0;
	m_dwCurrentFVF = 0;
	m_dwLastShader = 0;
	m_dwLastFVF = 0;

	m_iCurrentTexture = 0;
	m_iLastTexture = 0;
	m_dwLastTextureCount = 0;
	m_bUpdateTextureList = false;
	m_bUpdateVertexDecs = 0;
	m_bUpdateStreams = 0;
	g_iSortedObjectCount = 0;
	m_iLastCount = 0;
	m_iListCount = 0;
	m_pbMarkedList = 0;
	g_ppSortedObjectList = 0;
	m_ppSortedObjectVisibleList = 0;
	m_pVertexDataList = 0;
	m_pIndexDataList = 0;

	// Reset member vars
	m_bGlobalShadows				= false;
    m_pCamera						= 0;
	*/

	//m_iVisibleObjectCount = 0;
    g_bObjectReplacedUpdateBuffers	= false;

	// all okay
	return true;
}

bool CObjectManager::Free ( void )
{
	/*
	// ensure this is deleted before leave
	SAFE_RELEASE ( g_pMainCameraDepthStencilTextureView );
	SAFE_RELEASE ( g_pMainCameraDepthStencilTexture );
	SAFE_RELEASE ( g_pMainCameraDepthTextureSurfaceRef );
	SAFE_RELEASE ( g_pMainCameraDepthTexture );

	// safely delete any arrays and objects
	SAFE_DELETE ( m_pVertexDataList );
	SAFE_DELETE ( m_pIndexDataList );
	SAFE_DELETE_ARRAY ( m_pbMarkedList );
	SAFE_DELETE_ARRAY ( g_ppSortedObjectList );
	SAFE_DELETE_ARRAY ( m_ppSortedObjectVisibleList );
	*/

	// all okay
	return true;
}

void CObjectManager::UpdateInitOnce(void)
{
	// now done at end of object creation, need wicked object soon as!
}

bool CObjectManager::Update(void)
{
	return true;
}

bool CObjectManager::UpdateLayer( int iLayer )
{
	return true;
}

bool CObjectManager::AddObjectToBuffers(sObject* pObject)
{
	WickedCall_AddObject ( pObject );
	return true;
}

bool CObjectManager::RemoveObjectFromBuffers(sObject* pObject)
{
	return true;
}

bool CObjectManager::AddObjectMeshToBuffers(sMesh* pMesh, bool bUsesItsOwnBuffers)
{
	return true;
}

bool CObjectManager::RemoveBuffersUsedByObjectMesh(sMesh* pMesh, bool bRecurse)
{
	return true;
}

bool CObjectManager::RemoveBuffersUsedByObjectMeshDirectly(sMesh* pMesh)
{
	return true;
}

bool CObjectManager::RemoveBuffersUsedByObject		( sObject* pObject )
{
	// remove wicked resources associated with this object
	//WickedCall_RemoveObject ( pObject ); // done later and at right time with DeleteMesh
	return true;
}

bool CObjectManager::RefreshObjectInBuffer			( sObject* pObject )
{
	return true;
}

bool CObjectManager::UpdateObjectListSize			( int iSize )
{
	return true;
}

bool CObjectManager::UpdateTextures					( void )
{
	return true;
}

bool CObjectManager::UpdateAnimationCycle			( void )
{
	return true;
}

bool CObjectManager::SortTextureList				( void )
{
	return true;
}

int	CObjectManager::SwitchRenderTargetToDepth		( int iFlag )
{
	return 0;
}

bool CObjectManager::AddFlaggedObjectsBackToBuffers	( void )
{
	return true;
}

bool CObjectManager::RenewReplacedMeshes			( sObject* pObject )
{
	return true;
}

void CObjectManager::RemoveTextureRefFromAllObjects(LPGGTEXTURE pTextureRef)
{
}

void CObjectManager::UpdateAnimationCyclePerObject	( sObject* pObject )
{
}

void CObjectManager::SetGlobalShadowsOn				( void )
{
}

void CObjectManager::SetGlobalShadowsOff			( void )
{
}

int CObjectManager::GetVisibleObjectCount			( void )
{
	return 0;
}

sObject** CObjectManager::GetSortedObjectVisibleList( void )
{
	return NULL;
}

void CObjectManager::ResetIBRef						( void )
{
}

bool CObjectManager::SortVisibilityList				( void )
{
	return true;
}

bool CObjectManager::ReplaceAllFlaggedObjectsInBuffers(void)
{
	// only if global flag switched
	if ( g_bObjectReplacedUpdateBuffers )
	{
		/*
		// delete all buffers that these object resided in
		for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
		{
			// get index from shortlist
			int iObjectID = g_ObjectListRef [ iShortList ];
			sObject* pRemovedObject = g_ObjectList [ iObjectID ];
			if ( pRemovedObject )
			{
				// 210214 - completely ignore excluded objects
				if ( pRemovedObject->bExcluded ) continue;

				if ( pRemovedObject->bReplaceObjectFromBuffers )
					RemoveBuffersUsedByObject ( pRemovedObject );
			}
		}

		// upon buffer removal, some object where flagged for re-creation
		AddFlaggedObjectsBackToBuffers ();
		*/

		// when all buffers clear of removed objects, can add new instances of them back in..
		for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
		{
			// get index from shortlist
			int iObjectID = g_ObjectListRef [ iShortList ];
			sObject* pObject = g_ObjectList [ iObjectID ];
			if ( pObject )
			{
				if ( pObject->bExcluded ) continue;
				if ( pObject->bReplaceObjectFromBuffers )
				{
					// add object back in
					AddObjectToBuffers ( pObject );

					// and clear flag 
					pObject->bReplaceObjectFromBuffers = false;		
				}
			}
		}

		// update texture list when introduce new object(s)
		//UpdateTextures();

		// reset global flag
		g_bObjectReplacedUpdateBuffers = false;
	}

	// everything went okay
	return true;
}

std::unordered_map<int, sFrame*> lastHitFrame;

sObject* CObjectManager::FindObjectFromWickedObjectEntityID ( uint64_t iWickedEntityID )
{
	// go through ALL objects
	for (int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++)
	{
		int iObjectID = g_ObjectListRef[iShortList];
		sObject* pObject = g_ObjectList[iObjectID];
		if (pObject)
		{
			// for each object, look for wicked root entity reference in meshes
			uint64_t wickedRootEntity = pObject->wickedrootentityindex;
			for (int iFrameIndex = 0; iFrameIndex < pObject->iFrameCount; iFrameIndex++)
			{
				sFrame* pFrame = pObject->ppFrameList[iFrameIndex];
				if (pFrame && pFrame->wickedobjindex>0)
				{
					if (pFrame->wickedobjindex == iWickedEntityID)
					{
						// lastHitFrame is written by WickedCall_SentRay4 on the main thread only: this also runs on pick threads
						return pObject;
					}
				}
			}
			/*
			for (int iMeshIndex = 0; iMeshIndex < pObject->iMeshCount; iMeshIndex++)
			{
				sMesh* pMesh = pObject->ppMeshList[iMeshIndex];
				if (pMesh && pMesh->wickedmeshindex>0)
				{
					if (pMesh->wickedmeshindex == iWickedEntityID)
					{
						return pObject;
					}
				}
			}
			*/
		}
	}
	return NULL;
}

//#include "CommonC.h"
//#include "SoftwareCulling.h"
//#include "ShadowMapping\cShadowMaps.h"
//#include "Occlusion\cOcclusion.h"
//#include "CGfxC.h"
//#include <algorithm>

/*
#define SupportTechniqueOutLine (1 << 31)
bool update_mesh_light(sMesh* pMesh, sObject* pObject, sFrame* pFrame);
void start_mesh_light(void);
void end_mesh_light(void);
void setlayer_mesh_light(int layer);

GGFORMAT GetValidStencilBufferFormat ( GGFORMAT Format );
extern UINT	g_StereoEyeToggle;
extern DWORD g_dwSyncMaskOverride;

// weapon shader effect indexes
extern int g_weaponbasicshadereffectindex;
extern int g_weaponboneshadereffectindex;
extern int g_jetpackboneshadereffectindex;

// shadow mapping
extern CascadedShadowsManager g_CascadedShadow;
float g_fShrinkObjectsTo = 0.0f;

// Globals for new main camea depth texture
bool g_bMainCameraDepthCaptureActive = true;
bool g_bFirstRenderClearsRenderTarget = false;
LPGGTEXTURE g_pMainCameraDepthTexture = NULL;
LPGGRENDERTARGETVIEW g_pMainCameraDepthTextureSurfaceRef = NULL;
LPGGSURFACE g_pMainCameraDepthStencilTexture = NULL;
LPGGDEPTHSTENCILVIEW g_pMainCameraDepthStencilTextureView = NULL;

// Globals for DBO/Manager relationship
std::vector< sMesh* >						g_vRefreshMeshList;
std::vector< sObject* >						g_vAnimatableObjectList;
int											g_iSortedObjectCount;

// can prepare scene in two places now, so need flag to keeps tabs on it
bool										g_bScenePrepared				= false;
bool										g_bRenderVeryEarlyObjects		= false;

// during LOD QUAD transition, use ZBIAS to move quad out of way when 3D fades in/out by THIS amount in total
float										g_fZBiasEpsilon					= 0.0005f;

// detect AnisotropyLevel
int											g_iAnisotropyLevel				= -1;

// Retain the last objects distance and use to decide if shader should toggle to LOD variant
float										g_fObjectCamDistance			= 0.0f;

#ifdef DX11
struct CBPerMesh
{
	KMaths::Matrix mWorld;
	KMaths::Matrix mView;
	KMaths::Matrix mProjection;
};				
struct CBPerMeshPS
{
	GGCOLOR vMaterialEmissive;
	float fAlphaOverride;
	float fRes1;
	float fRes2;
	float fRes3;
	KMaths::Matrix mViewInv;
	KMaths::Matrix mViewProj;
	KMaths::Matrix mPrevViewProj;
};				
ID3D11Buffer* g_pCBPerMesh		= NULL;
ID3D11Buffer* g_pCBPerMeshPS	= NULL;
#endif

// externals
extern GGMATRIX 		g_matThisViewProj;
extern GGMATRIX 		g_matThisCameraView;
extern GGMATRIX 		g_matPreviousViewProj;
extern LPGG			m_pDX;

// Occlusion object external
extern COcclusion			g_Occlusion;

namespace
{
    // Structures for sorting using the STL sort
    // Used for sorting the m_ppSortedObjectList, m_pDepthSortedList
    struct OrderByReverseCameraDistance
    {
        bool operator()(sObject* pObjectA, sObject* pObjectB)
        {
            if (pObjectA->position.fCamDistance > pObjectB->position.fCamDistance)
                return true;
            if (pObjectA->position.fCamDistance == pObjectB->position.fCamDistance)
                return (pObjectA->dwObjectNumber < pObjectB->dwObjectNumber);
            return false;
        }
    };
    struct OrderByTexture
    {
        bool operator()(sObject* pObjectA, sObject* pObjectB)
        {
            int iImageA = 0;
            int iImageB = 0;

            // Calculate order by object number now,
            // just in case it's needed and to avoid getting incorrect results
            // if instances are involved.
            bool bObjectOrder = pObjectA->dwObjectNumber < pObjectB->dwObjectNumber;

            // Get the image id for object a
		    if ( pObjectA->pInstanceOfObject )
			    pObjectA = pObjectA->pInstanceOfObject;
		    if ( pObjectA->ppMeshList && pObjectA->ppMeshList [ 0 ]->pTextures)
                iImageA = pObjectA->ppMeshList [ 0 ]->pTextures [ 0 ].iImageID;

            // Get the image id for object b
		    if ( pObjectB->pInstanceOfObject )
			    pObjectB = pObjectB->pInstanceOfObject;
		    if ( pObjectB->ppMeshList && pObjectB->ppMeshList [ 0 ]->pTextures)
                iImageB = pObjectB->ppMeshList [ 0 ]->pTextures [ 0 ].iImageID;

            if (iImageA < iImageB)
                return true;
            if (iImageA > iImageB)
                return false;

            // Same images, so order by object id
            return bObjectOrder;
        }
    };
    struct OrderByObject
    {
        bool operator()(sObject* pObjectA, sObject* pObjectB)
        {
            return (pObjectA->dwObjectNumber < pObjectB->dwObjectNumber);
        }
    };
}

// move these into cpp file for debugging
CObjectManager::sVertexData::sVertexData ( )
{
	memset ( this, 0, sizeof(sVertexData) );
}

CObjectManager::sVertexData::~sVertexData ( )
{
	SAFE_RELEASE ( pVB );
	sVertexData* pThis = pNext;
	while ( pThis )
	{
		sVertexData* pNextOne = pThis->pNext;
		pThis->pNext = NULL;
		SAFE_RELEASE ( pThis->pVB );
		delete pThis;
		pThis = pNextOne;
	}
	pNext = NULL;
}

CObjectManager::sIndexData::sIndexData ( )
{
	memset ( this, 0, sizeof(sIndexData) );
}

CObjectManager::sIndexData::~sIndexData ( )
{
	SAFE_RELEASE ( pIB );
	sIndexData* pThis = pNext;
	while ( pThis )
	{
		sIndexData* pNextOne = pThis->pNext;
		pThis->pNext = NULL;
		SAFE_RELEASE ( pThis->pIB );
		delete pThis;
		pThis = pNextOne;
	}
	pNext = NULL;
}

bool CObjectManager::UpdateObjectListSize ( int iSize )
{
	// if list count is larger than size passed in, we can ignore a resize
	if ( iSize < m_iListCount )
		return true;

	// allocate memory
	sObject**	ppSortedObjectVisibleList = new sObject* [ iSize ];
	sObject**	ppSortedObjectList        = new sObject* [ iSize ];
	bool*		pbMarkedList              = new bool     [ iSize ];

	// safety checks on new memory
	SAFE_MEMORY ( ppSortedObjectVisibleList );
	SAFE_MEMORY ( ppSortedObjectList );
	SAFE_MEMORY ( pbMarkedList );

	// set all pointers to null
	for ( int iArrayIndex = 0; iArrayIndex < iSize; iArrayIndex++ )
	{
		ppSortedObjectVisibleList [ iArrayIndex ] = NULL;
		ppSortedObjectList        [ iArrayIndex ] = NULL;
		pbMarkedList              [ iArrayIndex ] = false;
	}

	// copy old data to new arrays
	int iSizeToCopyNow = m_iListCount;
	if ( iSizeToCopyNow > 0 )
	{
		if ( m_ppSortedObjectVisibleList ) memcpy ( ppSortedObjectVisibleList, m_ppSortedObjectVisibleList, sizeof(sObject*) * iSizeToCopyNow );
		if ( g_ppSortedObjectList ) memcpy ( ppSortedObjectList, g_ppSortedObjectList, sizeof(sObject*) * iSizeToCopyNow );
		if ( m_pbMarkedList ) memcpy ( pbMarkedList, m_pbMarkedList, sizeof(bool) * iSizeToCopyNow );
	}

	// safely delete any of the arrays
	SAFE_DELETE_ARRAY ( m_ppSortedObjectVisibleList );
	SAFE_DELETE_ARRAY ( g_ppSortedObjectList );
	SAFE_DELETE_ARRAY ( m_pbMarkedList );

	// allocate memory
	m_ppSortedObjectVisibleList = ppSortedObjectVisibleList;
	g_ppSortedObjectList        = ppSortedObjectList;
	m_pbMarkedList              = pbMarkedList;

	// store the size of the list
	m_iListCount = iSize;

	// return back
	return true;
}

void CObjectManager::ResetIBRef(void)
{
	m_ppLastIBRef = NULL;
	m_bUpdateStreams = true;
}

//
// VERTEX AND INDEX BUFFERS
//

CObjectManager::sIndexData* CObjectManager::FindIndexBuffer ( DWORD dwIndexCount, bool bUsesItsOwnBuffers )
{
	// find an index buffer which we wan use

	// check D3D device is valid
	if ( !m_pD3D )
		return NULL;

	// make sure the parameter is valid
	if ( dwIndexCount < 1 )
		return NULL;

	// local variables
	bool		 bMatch = false;
	sIndexData*  pIndexData  = m_pIndexDataList;

	// create a decent start size of the IB
	DWORD dwIndexBufferSize = 0;
	#ifdef DX11
	dwIndexBufferSize = 65534;
	#else
	GGCAPS caps;
	m_pD3D->GetDeviceCaps ( &caps );
	dwIndexBufferSize = caps.MaxVertexIndex;
	#endif

	// no search if need to use its own buffer
	if ( bUsesItsOwnBuffers )
		pIndexData=NULL;

	// run through all nodes in list
	while ( pIndexData )
	{
		// see if we can fit the data into the buffer
		if ( pIndexData->dwCurrentIndexCount + dwIndexCount < pIndexData->dwMaxIndexCount )
		{
			bMatch = true;
			break;
		}

		// move to next node
		pIndexData = pIndexData->pNext;
	}

	// if we don't have a match then create a new item
	if ( !bMatch )
	{
		if ( !m_pIndexDataList )
		{
			// create new list
			m_pIndexDataList	= new sIndexData;
			pIndexData			= m_pIndexDataList;

			// ensure creation okay
			SAFE_MEMORY ( m_pIndexDataList );
		}
		else
		{
			// find end of list
			pIndexData = m_pIndexDataList;
			while ( pIndexData )
			{
				if ( pIndexData->pNext )
				{
					pIndexData = pIndexData->pNext;
					continue;
				}
				else
					break;
			}

			// add new item to list
			pIndexData->pNext = new sIndexData ( );
			pIndexData        = pIndexData->pNext;
		}

		// own buffer needs only to be the size of the data
		if ( bUsesItsOwnBuffers )
			dwIndexBufferSize = dwIndexCount;

		// loop until succeed in creating a IB
		bool bCreate = true;
		while ( bCreate )
		{
			// attempt to create a IB
			#ifdef DX11

			D3D11_BUFFER_DESC bufferDesc;
			bufferDesc.Usage           = D3D11_USAGE_DEFAULT;
			bufferDesc.ByteWidth       = sizeof ( WORD ) * dwIndexBufferSize;
			bufferDesc.BindFlags       = D3D11_BIND_INDEX_BUFFER;
			bufferDesc.CPUAccessFlags  = 0;
			bufferDesc.MiscFlags       = 0;

			if ( FAILED ( m_pD3D->CreateBuffer( &bufferDesc, NULL, &pIndexData->pIB ) ) )
			{
				// failed, try half the size
				dwIndexBufferSize /= 2;
			}
			else
			{
				// only if IB can hold required vertex data
				if ( dwIndexBufferSize >= dwIndexCount )
				{
					// success, we can use this size
					bCreate = false;
				}
				else
				{
					// IB created, but just too small!
					SAFE_RELEASE(pIndexData->pIB);
					return NULL;
				}
			}
			#else
			if ( FAILED ( m_pD3D->CreateIndexBuffer ( 
														sizeof ( WORD ) * dwIndexBufferSize,
														D3DUSAGE_WRITEONLY,
														GGFMT_INDEX16,
														D3DPOOL_DEFAULT,
														&pIndexData->pIB,
														NULL
													 ) ) )
			{
				// failed, try half the size
				dwIndexBufferSize /= 2;
			}
			else
			{
				// only if IB can hold required vertex data
				if ( dwIndexBufferSize >= dwIndexCount )
				{
					// success, we can use this size
					bCreate = false;
				}
				else
				{
					// IB created, but just too small!
					SAFE_RELEASE(pIndexData->pIB);
					return NULL;
				}
			}
			#endif

			// if we continue until a ridiculously low value, we must fail
			if ( dwIndexBufferSize <= 0 )
				return NULL;
		}

		// ensure can fit inside the max size of a buffer
		if ( dwIndexCount > dwIndexBufferSize )
		{
			// if not, index data cannot fit inside single IB!
			SAFE_RELEASE ( pIndexData->pIB );
			return NULL;
		}

		// save the format of the buffer and the number allowed
		pIndexData->dwMaxIndexCount = dwIndexBufferSize;
		pIndexData->dwCurrentIndexCount = 0;
	}

	// return the final buffer
	return pIndexData;
}

CObjectManager::sVertexData* CObjectManager::FindVertexBuffer ( DWORD dwFVF, LPGGVERTEXLAYOUT pVertexDec, DWORD dwSize, DWORD dwVertexCount, DWORD dwIndexCount, bool bUsesItsOwnBuffers, int iType )
{
	// we need to find a buffer which the objects data can be added into
	// this function will go through the list of all buffers and find a
	// match for the FVF

	// check D3D device is valid
	if ( !m_pD3D ) return NULL;

	// make sure we have a FVF mode (FVF zero means we have converted it to declaration, fine for DX11)
	#ifndef DX11
	if ( dwFVF==0 && pVertexDec==NULL ) return NULL;
	#endif

	// make sure the parameters are valid
	if ( dwSize < 1 || dwVertexCount < 1 ) return NULL;

	// local variables
	bool			bMatch			= false;
	sVertexData*	pVertexData		= m_pVertexDataList;

	// get device capabilities
	DWORD dwPrimCountMax = 0;
	DWORD dwVBSize = 0;
	#ifdef DX11
	dwPrimCountMax = 65534;
	dwVBSize = 65534;
	#else
	GGCAPS caps;
	m_pD3D->GetDeviceCaps ( &caps );
	dwPrimCountMax = caps.MaxPrimitiveCount;
	dwVBSize = caps.MaxVertexIndex;
	#endif

	// make sure primitive count can be achieved
	if ( dwIndexCount>0 )
	{
		if ( dwIndexCount/3 > dwPrimCountMax )
			return NULL;
	}
	else
	{
		if ( dwVertexCount/3 > dwPrimCountMax )
			return NULL;
	}

	// no search if need to use its own buffer
	if ( bUsesItsOwnBuffers )
		pVertexData=NULL;

	// run through all nodes in list
	while ( pVertexData )
	{
		// check if vertex declarations match
		bool bVertDecMatch = false;
		if ( pVertexData->dwFormat==0 )
		{
			UINT numElementsThis;
			GGVERTEXELEMENT VertexDecFromThisBuffer[256];
			if ( pVertexData->pVertexDec )
			{
				#ifdef DX11
				#else
				HRESULT hr = pVertexData->pVertexDec->GetDeclaration( VertexDecFromThisBuffer, &numElementsThis);
				UINT numElementsRequest;
				GGVERTEXELEMENT VertexDecFromRequest[256];
				if ( pVertexDec )
				{
					hr = pVertexDec->GetDeclaration( VertexDecFromRequest, &numElementsRequest);
					DWORD dwCMPSize = sizeof(GGVERTEXELEMENT)*numElementsRequest;
					if ( numElementsRequest == numElementsThis )
						if ( memcmp ( VertexDecFromRequest, VertexDecFromThisBuffer, dwCMPSize )==0 )
							bVertDecMatch = true;
				}
				#endif
			}
		}
		else
			bVertDecMatch = true;

		// see if we find a match to the FVF
		if ( pVertexData->dwFormat == dwFVF && bVertDecMatch==true )
		{
			// see if we can fit the data into the buffer
			if ( pVertexData->dwCurrentVertexCount + dwVertexCount < pVertexData->dwMaxVertexCount )
			{
				bMatch = true;
				break;
			}
		}

		// move to next node
		pVertexData = pVertexData->pNext;
	}

	// if we don't have a match then create a new VB
	if ( !bMatch )
	{
		if ( !m_pVertexDataList )
		{
			// create new list
			m_pVertexDataList		= new sVertexData;
			pVertexData				= m_pVertexDataList;
			SAFE_MEMORY ( m_pVertexDataList );
		}
		else
		{
			// find end of list
			pVertexData = m_pVertexDataList;
			while ( pVertexData )
			{
				if ( pVertexData->pNext )
				{
					pVertexData = pVertexData->pNext;
					continue;
				}
				else
					break;
			}

			// add new item to list
			pVertexData->pNext = new sVertexData ( );
			pVertexData        = pVertexData->pNext;
		}

		// create a decent start size of the VB
		bool bCreate = true;

		// if size exceeds 16bit, make max size 16bit (32bit index supported maybe in future though it shows no speed increase!!)
		if ( dwIndexCount > 0 )
		{
			// mesh uses index buffer so can only have a 16bit vertex buffer
			if ( dwVBSize > 0x0000FFFF ) dwVBSize = 0x0000FFFF;
		}

		// own buffer needs only to be the size of the data
		if ( bUsesItsOwnBuffers )
			dwVBSize = dwVertexCount;

		// loop until succeed in creating a VB
		#ifdef DX11
		while ( bCreate )
		{
			// attempt to create a VB
			D3D11_BUFFER_DESC bufferDesc;
			bufferDesc.Usage           = D3D11_USAGE_DEFAULT;
			bufferDesc.ByteWidth       = dwSize * dwVBSize;
			bufferDesc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags  = 0;
			bufferDesc.MiscFlags       = 0;

			if ( FAILED ( m_pD3D->CreateBuffer( &bufferDesc, NULL, &pVertexData->pVB ) ) )
			{
				// failed, try half the size
				dwVBSize /= 2;
			}
			else
			{
				// only if VB can hold required vertex data
				if ( dwVBSize >= dwVertexCount )
				{
					// success, we can use this size
					bCreate = false;
				}
				else
				{
					// VB created, but just too small!
					SAFE_RELEASE(pVertexData->pVB);
					return NULL;
				}
			}

			// if we continue until a ridiculously low value, we must fail
			if ( dwVBSize <= 0 )
				return NULL;
		}
		#else
		while ( bCreate )
		{
			DWORD dwUsage = D3DUSAGE_WRITEONLY;

			if ( iType == D3DPT_POINTLIST )
				dwUsage = D3DUSAGE_WRITEONLY | D3DUSAGE_POINTS;

			// attempt to create a VB
			if ( FAILED ( m_pD3D->CreateVertexBuffer ( 
														dwSize * dwVBSize,
														dwUsage,
														dwFVF,
														D3DPOOL_DEFAULT,
														&pVertexData->pVB,
														NULL
												     ) ) )
			{
				// failed, try half the size
				dwVBSize /= 2;
			}
			else
			{
				// only if VB can hold required vertex data
				if ( dwVBSize >= dwVertexCount )
				{
					// success, we can use this size
					bCreate = false;
				}
				else
				{
					// VB created, but just too small!
					SAFE_RELEASE(pVertexData->pVB);
					return NULL;
				}
			}

			// if we continue until a ridiculously low value, we must fail
			if ( dwVBSize <= 0 )
				return NULL;
		}
		#endif

		// save the format of the VB and the number of vertices allowed
		pVertexData->dwFormat					= dwFVF;
		pVertexData->pVertexDec					= pVertexDec;
		pVertexData->dwMaxVertexCount			= dwVBSize;
		pVertexData->dwCurrentVertexCount		= 0;
	}

	// return the final buffer
	return pVertexData;
}

bool CObjectManager::AddObjectMeshToBuffers ( sMesh* pMesh, bool bUsesItsOwnBuffers )
{
	// vertex and index buffer set up
	WORD*		 pIndices    = NULL;
	sVertexData* pVertexData = NULL;
	sIndexData*	 pIndexData  = NULL;

	// find a vertex buffer we can use which matches the FVF component
	pVertexData = this->FindVertexBuffer (	pMesh->dwFVF,
											pMesh->pVertexDec,
											pMesh->dwFVFSize,
											pMesh->dwVertexCount,						
											pMesh->dwIndexCount,
											bUsesItsOwnBuffers,
											pMesh->iPrimitiveType
										); // if no indexbuffer, can make larger vertex buffer (16bit index only)

	// check the vertex buffer is valid
	if ( pVertexData==NULL )
	{
		pMesh->pDrawBuffer = NULL;
		return false;
	}

	// find an index buffer (if one is required)
	if ( pMesh->dwIndexCount> 0 )
	{
		// find and check the index buffer is valid
		pIndexData = this->FindIndexBuffer ( pMesh->dwIndexCount, bUsesItsOwnBuffers );
		SAFE_MEMORY ( pIndexData );
	}

	// create a new vertex buffer reference array
	SAFE_DELETE ( pMesh->pDrawBuffer );
	pMesh->pDrawBuffer = new sDrawBuffer;

	// check the reference array is okay
	SAFE_MEMORY ( pMesh->pDrawBuffer );

	// draw primitive type
	pMesh->pDrawBuffer->dwPrimType			= ( GGPRIMITIVETYPE ) pMesh->iPrimitiveType;

	// store a reference to the vertex buffer
	pMesh->pDrawBuffer->pVertexBufferRef	= pVertexData->pVB;
	pMesh->pDrawBuffer->dwVertexStart		= ( pVertexData->dwPosition * sizeof(float) ) / pMesh->dwFVFSize;
	pMesh->pDrawBuffer->dwVertexCount		= pMesh->iDrawVertexCount;

	// primitive count for drawing
	pMesh->pDrawBuffer->dwPrimitiveCount	= pMesh->iDrawPrimitives;

	// store a reference to the indice buffer
	if( pIndexData )
	{
		pMesh->pDrawBuffer->pIndexBufferRef		= pIndexData->pIB;
		pMesh->pDrawBuffer->dwIndexStart		= pIndexData->dwCurrentIndexCount;
	}
	else
	{
		pMesh->pDrawBuffer->pIndexBufferRef		= NULL;
		pMesh->pDrawBuffer->dwIndexStart		= 0;
	}

	// store a reference to the FVF size
	pMesh->pDrawBuffer->dwFVFSize			= pMesh->dwFVFSize;

	// store references to the VB and IB list items
	pMesh->pDrawBuffer->pVBListEntryRef		= (LPVOID)pVertexData;
	pMesh->pDrawBuffer->pIBListEntryRef		= (LPVOID)pIndexData;

	// copy the indices (if applicable)
	if ( pIndexData )
	{
		// update base vertex index for indices (and adjust vertex start accordingly)
		pMesh->pDrawBuffer->dwBaseVertexIndex	 = ( WORD ) pVertexData->dwCurrentVertexCount;
		pMesh->pDrawBuffer->dwVertexStart		-= ( WORD ) pMesh->pDrawBuffer->dwBaseVertexIndex;

		// copy index data to index buffer (WORD based)
		CopyMeshDataToIndexBuffer ( pMesh, pIndexData->pIB, pIndexData->dwCurrentIndexCount );

		// increment the index count
		pIndexData->dwCurrentIndexCount += pMesh->dwIndexCount;
	}

	// copy vertex data to vertex buffer
	CopyMeshDataToVertexBufferSameFVF ( pMesh, pVertexData->pVB, pVertexData->dwPosition );

	// save the current position
	pVertexData->dwPosition += pMesh->dwVertexCount * ( pMesh->dwFVFSize/sizeof(float) );

	// increment the vertex count in the buffer
	pVertexData->dwCurrentVertexCount += pMesh->dwVertexCount;

	// everything went okay
	return true;
}

bool CObjectManager::AddObjectToBuffers ( sObject* pObject )
{
	// vertex and index buffer set up
	bool			bAllOkay		= true;
	WORD*			pIndices		= NULL;
	sVertexData*	pVertexData		= NULL;
	sIndexData*		pIndexData		= NULL;

	// run through each frame within an object
	for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
	{
		// get frame ptr
		sFrame* pFrame = pObject->ppFrameList [ iFrame ];

		// add each mesh to the buffers
		if ( pFrame->pMesh )		if ( !AddObjectMeshToBuffers ( pFrame->pMesh, pObject->bUsesItsOwnBuffers ) )				bAllOkay=false;
		if ( pFrame->pLOD[0] )		if ( !AddObjectMeshToBuffers ( pFrame->pLOD[0], pObject->bUsesItsOwnBuffers ) )				bAllOkay=false;
		if ( pFrame->pLOD[1] )		if ( !AddObjectMeshToBuffers ( pFrame->pLOD[1], pObject->bUsesItsOwnBuffers ) )				bAllOkay=false;
		if ( pFrame->pLODForQUAD )	if ( !AddObjectMeshToBuffers ( pFrame->pLODForQUAD, pObject->bUsesItsOwnBuffers ) )				bAllOkay=false;
	}

	// update texture list when introduce new object
	m_bUpdateTextureList=true;

	// everything went..
	return bAllOkay;
}


bool CObjectManager::FlagAllObjectMeshesUsingBuffer ( sVertexData* pVertexData, sIndexData* pIndexData )
{
	// flag any object mesh that uses either of these buffers
	for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
	{
		// get index from shortlist
		int iObjectID = g_ObjectListRef [ iShortList ];

		// see if we have a valid list
		sObject* pObject = g_ObjectList [ iObjectID ];
		if ( !pObject )
			continue;

		// run through each frame within an object
		for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
		{
			// get frame ptr
			sFrame* pFrame = pObject->ppFrameList [ iFrame ];

			// add this object:mesh back in
			if ( pFrame->pMesh )
			{
				if ( pFrame->pMesh->pDrawBuffer )
				{
					if(pFrame->pMesh->pDrawBuffer->pVBListEntryRef==(LPVOID)pVertexData
					|| pFrame->pMesh->pDrawBuffer->pIBListEntryRef==(LPVOID)pIndexData )
					{
						// leefix - 070403 - ensure associated meshes are also removed from buffers to prevent duplication!
						if ( pFrame->pMesh->bAddObjectToBuffers==false )
						{
							pFrame->pMesh->bAddObjectToBuffers=true;
							// mike - 301106 - add flag to stop recursion
							RemoveBuffersUsedByObjectMesh ( pFrame->pMesh, false );
						}
					}
				}
			}
			for ( int l=0; l<3; l++ )
			{
				sMesh* pLODMesh = NULL;
				if ( l<2 )
					pLODMesh = pFrame->pLOD[l];
				else
					pLODMesh = pFrame->pLODForQUAD;

				if ( pLODMesh )
				{
					if ( pLODMesh->pDrawBuffer )
					{
						if(pLODMesh->pDrawBuffer->pVBListEntryRef==(LPVOID)pVertexData
						|| pLODMesh->pDrawBuffer->pIBListEntryRef==(LPVOID)pIndexData )
						{
							if ( pLODMesh->bAddObjectToBuffers==false )
							{
								pLODMesh->bAddObjectToBuffers=true;
								RemoveBuffersUsedByObjectMesh ( pLODMesh, false );
							}
						}
					}
				}	
			}
		}
	}

	// everything went okay
	return true;
}

// mike - 301106 - add flag to stop recursion
bool CObjectManager::RemoveBuffersUsedByObjectMesh ( sMesh* pMesh, bool bRecurse )
{
	// get reference to drawbuffer
	sDrawBuffer* pDrawBuffer = pMesh->pDrawBuffer;
	if(pDrawBuffer)
	{
		DWORD* pdwAdd = (DWORD*)&pMesh->pDrawBuffer;

		// get reference to VB and IB ptrs
		sVertexData* pVertexData = (sVertexData*)pDrawBuffer->pVBListEntryRef;
		sIndexData*	 pIndexData  = (sIndexData* )pDrawBuffer->pIBListEntryRef;

		// scan for and delete vertex item
		sVertexData* pLastVertexData = NULL;
		sVertexData* pFindVertexData = m_pVertexDataList;
		while ( pFindVertexData )
		{
			// check this item
			sVertexData* pNextVertexData = pFindVertexData->pNext;
			if ( pFindVertexData==pVertexData )
			{
				// sever and delete vertex item
				pFindVertexData->pNext=NULL;

				// free VB
				SAFE_RELEASE( pFindVertexData->pVB );

				// delete vertexdata
				SAFE_DELETE( pFindVertexData );

				// adjust next value to leap deleted item
				if ( pLastVertexData ) pLastVertexData->pNext = pNextVertexData;

				// new start item to replace deleted one
				if ( m_pVertexDataList==pVertexData )
					m_pVertexDataList = pNextVertexData;

				// done here
				break;
			}

			// next item
			pLastVertexData = pFindVertexData;
			pFindVertexData = pNextVertexData;
		}

		// scan for and delete index item
		sIndexData* pLastIndexData = NULL;
		sIndexData* pFindIndexData = m_pIndexDataList;
		while ( pFindIndexData )
		{
			// check this item
			sIndexData* pNextIndexData = pFindIndexData->pNext;
			if ( pFindIndexData==pIndexData )
			{
				// sever and delete Index item
				pFindIndexData->pNext=NULL;

				// release IB
				SAFE_RELEASE ( pFindIndexData->pIB );

				// delete indexdata
				SAFE_DELETE( pFindIndexData );

				// adjust next value to leap deleted item
				if ( pLastIndexData ) pLastIndexData->pNext = pNextIndexData;

				// new start item to replace deleted one
				if ( m_pIndexDataList==pIndexData )
					m_pIndexDataList = pNextIndexData;

				// done here
				break;
			}

			// next item
			pLastIndexData = pFindIndexData;
			pFindIndexData = pNextIndexData;
		}

		// flag any objects that used either of these buffers
		if ( bRecurse == true )
		{
			// useful when we KNOW that the VB IB buffers are not shared
			FlagAllObjectMeshesUsingBuffer ( pVertexData, pIndexData );
		}
	}

	// everything went okay
	return true;
}

// lee - 140307 - added to delete buffers quickly if we know VB/IBs are not shared
bool CObjectManager::RemoveBuffersUsedByObjectMeshDirectly ( sMesh* pMesh )
{
	// delete the buffers from the lit, and release the ptrs, but do NOT assume buffers are shared!
	return RemoveBuffersUsedByObjectMesh ( pMesh, false );
}

bool CObjectManager::RemoveBuffersUsedByObject ( sObject* pObject )
{
	// run through each frame within an object
	for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
	{
		// get frame ptr
		sFrame* pFrame = pObject->ppFrameList [ iFrame ];

		// lee - 140307 - if object 'uses its own buffers' there is no need to use
		// a recursive check for whether the VB/IB buffers are shared, as they cannot be
		// so we can simply delete the buffers with out recursive (buffer remove/flag back in)
		if ( pObject->bUsesItsOwnBuffers==true )
		{
			// delete all vertex/index entries 'directly' used by this mesh (no recursive)
			if(pFrame->pMesh) RemoveBuffersUsedByObjectMeshDirectly ( pFrame->pMesh );
			if(pFrame->pLOD[0]) RemoveBuffersUsedByObjectMeshDirectly ( pFrame->pLOD[0] );
			if(pFrame->pLOD[1]) RemoveBuffersUsedByObjectMeshDirectly ( pFrame->pLOD[1] );
			if(pFrame->pLODForQUAD) RemoveBuffersUsedByObjectMeshDirectly ( pFrame->pLODForQUAD );
		}
		else
		{
			// delete all vertex/index entries used by this mesh
			if(pFrame->pMesh) RemoveBuffersUsedByObjectMesh ( pFrame->pMesh );
			if(pFrame->pLOD[0]) RemoveBuffersUsedByObjectMesh ( pFrame->pLOD[0] );
			if(pFrame->pLOD[1]) RemoveBuffersUsedByObjectMesh ( pFrame->pLOD[1] );
			if(pFrame->pLODForQUAD) RemoveBuffersUsedByObjectMesh ( pFrame->pLODForQUAD );
		}
	}

	// everything went okay
	return true;
}

bool CObjectManager::AddFlaggedObjectsBackToBuffers ( void )
{
	// upon buffer removal, some object where flagged for re-creation
	for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
	{
		// get index from shortlist
		int iObjectID = g_ObjectListRef [ iShortList ];

		// see if we have a valid list
		sObject* pObject = g_ObjectList [ iObjectID ];
		if ( !pObject )
			continue;

		// 210214 - completely ignore excluded objects
		if ( pObject->bExcluded ) continue;

		// also ensure we skip the removed object (not to be re-added)
		if ( pObject->bReplaceObjectFromBuffers==true )
			continue;

		// run through each frame within an object
		for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
		{
			// refresh the VB data for this mesh (it will auto-lock the VB)
			sFrame* pFrame = pObject->ppFrameList [ iFrame ];

			// add this object:mesh back in
			if ( pFrame->pMesh )
			{
				if ( pFrame->pMesh->bAddObjectToBuffers==true )
				{
					AddObjectMeshToBuffers ( pFrame->pMesh, pObject->bUsesItsOwnBuffers );
					pFrame->pMesh->bAddObjectToBuffers=false;
				}
			}
			// add this object:mesh back in
			for ( int l=0; l<3; l++ )
			{
				sMesh* pLODMesh = NULL;
				if ( l<2 )
					pLODMesh = pFrame->pLOD[l];
				else
					pLODMesh = pFrame->pLODForQUAD;

				if ( pLODMesh )
				{
					if ( pLODMesh->bAddObjectToBuffers==true )
					{
						AddObjectMeshToBuffers ( pLODMesh, pObject->bUsesItsOwnBuffers );
						pLODMesh->bAddObjectToBuffers=false;
					}
				}
			}
		}
	}

	// everything went okay
	return true;
}

bool CObjectManager::RemoveObjectFromBuffers ( sObject* pRemovedObject )
{
	// delete all buffers that this object resided in
	RemoveBuffersUsedByObject ( pRemovedObject );

	// upon buffer removal, some object where flagged for re-creation
	AddFlaggedObjectsBackToBuffers ();

	// update texture list when introduce new object(s)
	UpdateTextures();

	// everything went okay
	return true;
}

bool CObjectManager::ReplaceAllFlaggedObjectsInBuffers ( void )
{
	// only if global flag switched
	if ( g_bObjectReplacedUpdateBuffers )
	{
		// delete all buffers that these object resided in
		for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
		{
			// get index from shortlist
			int iObjectID = g_ObjectListRef [ iShortList ];
			sObject* pRemovedObject = g_ObjectList [ iObjectID ];
			if ( pRemovedObject )
			{
				// 210214 - completely ignore excluded objects
				if ( pRemovedObject->bExcluded ) continue;

				if ( pRemovedObject->bReplaceObjectFromBuffers )
					RemoveBuffersUsedByObject ( pRemovedObject );
			}
		}

		// upon buffer removal, some object where flagged for re-creation
		AddFlaggedObjectsBackToBuffers ();

		// when all buffers clear of removed objects, can add new instances of them back in..
		for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
		{
			// get index from shortlist
			int iObjectID = g_ObjectListRef [ iShortList ];
			sObject* pObject = g_ObjectList [ iObjectID ];
			if ( pObject )
			{
				// 210214 - completely ignore excluded objects
				if ( pObject->bExcluded ) continue;

				if ( pObject->bReplaceObjectFromBuffers )
				{
					// add object back in
					AddObjectToBuffers ( pObject );

					// and clear flag 
					pObject->bReplaceObjectFromBuffers = false;		
				}
			}
		}

		// update texture list when introduce new object(s)
		UpdateTextures();

		// reset global flag
		g_bObjectReplacedUpdateBuffers = false;
	}

	// everything went okay
	return true;
}

bool CObjectManager::UpdateObjectMeshInBuffer ( sMesh* pMesh )
{
	// only if have a drawbuffer
	if ( !pMesh->pDrawBuffer )
		return false;

	// if drawbuffer is insufficient, i.e. not big enough
	if ( pMesh->pDrawBuffer->dwVertexCount < pMesh->dwVertexCount 
	||	 pMesh->pDrawBuffer->dwFVFSize != pMesh->dwFVFSize )	
	{
		// recreate drawbuffer
		RemoveBuffersUsedByObjectMesh ( pMesh, true );
		AddObjectMeshToBuffers ( pMesh, true );
		return true;
	}

	// get the offset map
	sOffsetMap	offsetMap;
	GetFVFOffsetMap ( pMesh, &offsetMap );

	// get vertex list item pointer
	sVertexData* pVertexData = (sVertexData*)pMesh->pDrawBuffer->pVBListEntryRef;
	IGGVertexBuffer* pVertexBuffer = pVertexData->pVB;

	// lock the vertex buffer (if not already locked)
	#ifdef DX11
	#else
	if ( pVertexData->bBufferLocked==false )
	{
		pVertexData->pfLockedData = NULL;
		if ( FAILED ( pVertexBuffer->Lock ( 0, 0, ( VOID** ) &pVertexData->pfLockedData, 0 ) ) )
		{
			return false;
		}

		// set the VB flag when locked
		pVertexData->bBufferLocked = true;
	}
	#endif

	// copy vertex-data-block from object to VB
	DWORD dwPosWithinVB = (pMesh->pDrawBuffer->dwBaseVertexIndex + pMesh->pDrawBuffer->dwVertexStart) * pMesh->dwFVFSize;
	LPVOID pSourceData = pMesh->pVertexData;
	DWORD dwSizeToCopy = pMesh->dwVertexCount * pMesh->dwFVFSize;
	#ifdef DX11
	D3D11_BOX box;
	box.left = dwPosWithinVB;
	box.right = dwPosWithinVB + dwSizeToCopy;
	box.top = 0;
	box.bottom = 1;
	box.front = 0;
	box.back = 1;
	m_pImmediateContext->UpdateSubresource ( pVertexBuffer, 0, &box, pMesh->pVertexData, 0, 0 );
	#else
	LPVOID pDestPtr = pVertexData->pfLockedData + dwPosWithinVB;
	if ( pDestPtr ) memcpy ( pDestPtr, pSourceData, dwSizeToCopy );
	#endif

	// draw quantity can change without having to recreate (like for shadows)
	if ( pMesh->pDrawBuffer )
	{
		pMesh->pDrawBuffer->dwVertexCount		= pMesh->iDrawVertexCount;
		pMesh->pDrawBuffer->dwPrimitiveCount	= pMesh->iDrawPrimitives;
	}

	// leeadd - 230304 - physics changes INDEX DATA TOO,
	// which a VB update above does not do..so add this
	if ( pMesh->pDrawBuffer )
	{
		// only if index buffer exists
		if ( pMesh->pDrawBuffer->pIndexBufferRef )
			CopyMeshDataToIndexBuffer ( pMesh, pMesh->pDrawBuffer->pIndexBufferRef, pMesh->pDrawBuffer->dwIndexStart );
	}

	// everything went okay
	return true;
}

bool CObjectManager::UpdateAllObjectsInBuffers ( void )
{
	// objects that have changed are flagged, and passed to VB updater..

	// lee - 300914 - now uses a short list added to when meshes require refreshing (much faster)
	if ( !g_vRefreshMeshList.empty() )
    {
        for ( DWORD iIndex = 0; iIndex < g_vRefreshMeshList.size(); ++iIndex )
        {
			// get mesh to refresh
            sMesh* pMesh = g_vRefreshMeshList [ iIndex ];
			if ( !pMesh ) continue;

			// only refresh if not already done so (can have multiple entries in this list)
			if ( pMesh->bVBRefreshRequired==true )
			{
				UpdateObjectMeshInBuffer ( pMesh );
				pMesh->bVBRefreshRequired=false;
			}
		}

		// go through all vertex buffer items (unlock any that have been locked)
		CompleteUpdateInBuffers();

		// clear refresh list for next cycle (must quicker than going through ALL objects each cycle)
		g_vRefreshMeshList.clear();
	}

	// okay
	return true;
}

bool CObjectManager::CompleteUpdateInBuffers ( void )
{
	// go through all vertex buffer items (unlock any that have been locked)
	sVertexData* pVertexData = m_pVertexDataList;
	while ( pVertexData )
	{
		// if buffer has been locked
		#ifdef DX11
		#else
		if ( pVertexData->bBufferLocked==true )
		{
			// unlock and restore flag
			pVertexData->pVB->Unlock ( );
			pVertexData->bBufferLocked=false;
		}
		#endif

		// move to next node
		pVertexData = pVertexData->pNext;
	}

	// okay
	return true;
}

bool CObjectManager::QuicklyUpdateObjectMeshInBuffer ( sMesh* pMesh, DWORD dwVertexFrom, DWORD dwVertexTo )
{
	// only if have a drawbuffer
	if ( !pMesh->pDrawBuffer )
		return false;

	// get the offset map
	sOffsetMap	offsetMap;
	GetFVFOffsetMap ( pMesh, &offsetMap );

	// get vertex list item pointer
	sVertexData* pVertexData = (sVertexData*)pMesh->pDrawBuffer->pVBListEntryRef;
	IGGVertexBuffer* pVertexBuffer = pVertexData->pVB;

	// lock the vertex buffer (if not already locked)
	#ifdef DX11
	#else
	if ( pVertexData->bBufferLocked==false )
	{
		pVertexData->pfLockedData = NULL;
		if ( FAILED ( pVertexBuffer->Lock ( 0, 0, ( VOID** ) &pVertexData->pfLockedData, 0 ) ) )
			return false;

		// set the VB flag when locked
		pVertexData->bBufferLocked = true;
	}
	#endif

	// copy only vertex data changed as described in params passed in
	DWORD dwPosWithinVB = (pMesh->pDrawBuffer->dwBaseVertexIndex + pMesh->pDrawBuffer->dwVertexStart + dwVertexFrom) * pMesh->dwFVFSize;
	LPVOID pDestPtr = pVertexData->pfLockedData + dwPosWithinVB;
	DWORD dwPosWithinMesh = dwVertexFrom * pMesh->dwFVFSize;
	LPVOID pSourceData = pMesh->pVertexData + dwPosWithinMesh;
	DWORD dwSizeToCopy = (dwVertexTo-dwVertexFrom) * pMesh->dwFVFSize;
	memcpy ( pDestPtr, pSourceData, dwSizeToCopy );

	// draw quantity can change without having to recreate (like for shadows)
	pMesh->pDrawBuffer->dwVertexCount		= pMesh->iDrawVertexCount;
	pMesh->pDrawBuffer->dwPrimitiveCount	= pMesh->iDrawPrimitives;

	// everything went okay
	return true;
}

bool CObjectManager::RenewReplacedMeshes ( sObject* pObject )
{
	// run through each mesh within an object
	bool bReplaceObjectOwningMesh=false;
	for ( int iMesh = 0; iMesh < pObject->iMeshCount; iMesh++ )
	{
		// replace any meshes in VB/IB that have been changed
		sMesh* pMesh = pObject->ppMeshList [ iMesh ];
		if ( pMesh->bMeshHasBeenReplaced==true )
		{
			pMesh->bMeshHasBeenReplaced=false;
			bReplaceObjectOwningMesh=true;
			break;
		}
	}

	// replace object
	if ( bReplaceObjectOwningMesh )
	{
		pObject->bReplaceObjectFromBuffers = true;
		g_bObjectReplacedUpdateBuffers = true;
		bReplaceObjectOwningMesh = false;
	}

	// okay
	return true;
}

bool CObjectManager::RefreshObjectInBuffer ( sObject* pObject )
{
	pObject->bReplaceObjectFromBuffers = true;
	g_bObjectReplacedUpdateBuffers = true;
	return true;
}

void CObjectManager::RemoveTextureRefFromAllObjects ( LPGGTEXTURE pTextureRef )
{
	// run through all objects
	for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
	{
		// get index from shortlist
		int iObjectID = g_ObjectListRef [ iShortList ];

		// see if we have a valid list
		sObject* pObject = g_ObjectList [ iObjectID ];
		if ( !pObject )
			continue;

		// run through each frame within an object
		for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
		{
			// get frame
			sFrame* pFrame = pObject->ppFrameList [ iFrame ];
			sMesh* pMesh = pFrame->pMesh;
			if ( pMesh )
			{
				// go through all textures in mesh
				RemoveTextureRefFromMesh ( pMesh, pTextureRef );
			}
		}
	}
}

bool CObjectManager::SortTextureList ( void )
{
    // If the object list size hasn't changed and no textures have changed, then nothing to do here.
	//Dave Performance, can force an update with g_ForceTextureListUpdate flag set to true, to take into account ignored objects
	if ( !g_ForceTextureListUpdate )
	{
		if ( m_iLastCount == g_iObjectListRefCount && m_bUpdateTextureList==false )
			return true;
	}

    // Reset ready for next time
    m_iLastCount         = g_iObjectListRefCount;
    m_bUpdateTextureList = false;

    // make sure the lists we're using are valid
    SAFE_MEMORY ( g_ppSortedObjectList );
    SAFE_MEMORY ( m_ppSortedObjectVisibleList );

    // reset all data so we can build the list from scratch
    g_iSortedObjectCount = 0;
    g_bRenderVeryEarlyObjects = false;

	// at same time, collect object lists that DO NOT change from cycle to cycle
	g_vAnimatableObjectList.clear();

    // run through all known items and put them into render list ready for sorting
    for ( int iShortList = 0; iShortList < g_iObjectListRefCount; iShortList++ )
    {
	    // Get an object id from shortlist
	    int iObjectID = g_ObjectListRef [ iShortList ];

	    // Actual object or instance of object
	    sObject* pOriginalObject = g_ObjectList [ iObjectID ];
        if ( ! pOriginalObject )
            continue;

		// quick reject objects which have a sync mask of ZERO
		if ( pOriginalObject->dwCameraMaskBits==0 )
			continue;

		//Dave Performance - do not add ignored objects into the list
		if ( pOriginalObject->bIgnored )
			continue;

		// 210214 - quick reject objects which are excluded
		if ( pOriginalObject->bExcluded )
			continue;

		// lee - 300914 - if this object holds some animation data, add to animatable list (done once per texture sort for speed!)
		if ( pOriginalObject->pAnimationSet ) 
		{
			g_vAnimatableObjectList.push_back ( pOriginalObject );
		}

		// get the object we would render
        sObject* pRenderObject = pOriginalObject;
        if ( pRenderObject->pInstanceOfObject )
		    pRenderObject = pRenderObject->pInstanceOfObject;

	    // See if we have enough information to render this object
        // A (possibly instanced) object with a mesh list and with mesh 0 having a texture.
        if ( pRenderObject && pRenderObject->ppMeshList && pRenderObject->ppMeshList[0]->pTextures)
	    {
            // Add the original object into the render list
            g_ppSortedObjectList [ g_iSortedObjectCount++ ] = pOriginalObject;

            // If we are sorting by distance, calculate the distance ready for sorting
            if ( g_eGlobalSortOrder == E_SORT_BY_DEPTH )
		    {
			    if ( pOriginalObject->bVeryEarlyObject == true )
			    {
                    // very early objects are placed at extreme distance
				    pOriginalObject->position.fCamDistance = 9999999.9f;
			    }
			    else
			    {
				    pOriginalObject->position.fCamDistance = CalculateObjectDistanceFromCamera ( pOriginalObject );
			    }
            }
            
            // Check to see if there is an early draw object
            if ( pOriginalObject->bVeryEarlyObject == true )
		    {
			    // If this object is an early draw, set global flag to show we have one in the scene
                g_bRenderVeryEarlyObjects = true;
            }
	    }
    }

    // Now sort the list appropriately
    switch(g_eGlobalSortOrder)
    {
		case E_SORT_BY_TEXTURE:
			std::sort(g_ppSortedObjectList, g_ppSortedObjectList + g_iSortedObjectCount, OrderByTexture() );
			break;
		case E_SORT_BY_OBJECT:
			std::sort(g_ppSortedObjectList, g_ppSortedObjectList + g_iSortedObjectCount, OrderByObject() );
			break;
		case E_SORT_BY_DEPTH:
			// Delay sort until the visibility list is generated
			// This increases the accuracy of the sorting for depth.
			break;
		case E_SORT_BY_NONE:
			// No sort needed
			break;
		default:
			break;
	}

	// return back to caller
	return true;
}

//Dave Performance - used for shadows to ignore objects that are far away (relative to size) to stop them being considered for shadows
extern bool g_bIgnoreFarObjects;
extern int g_HideDistantShadows;
bool waited = false;

bool CObjectManager::SortVisibilityList ( void )
{
	// run through the sorted texture list and find out which objects
	// are visible, then create a new list which stores the visible objects

	// can quickly skip this step if the camera does not need to use latest visibilities (cheap shadow quad camera 5)
	if ( ((1<<g_pGlob->dwRenderCameraID) & m_dwSkipVisibilityListMask)!=0 )
		return true;

	// make sure we have a valid object
	if ( g_ppSortedObjectList==NULL )
		return true;

	// reset the number of visible objects to 0
	m_iVisibleObjectCount = 0;
    m_vVisibleObjectList.clear();
    m_vVisibleObjectEarly.clear();
    m_vVisibleObjectTransparent.clear();
    m_vVisibleObjectNoZDepth.clear();
    m_vVisibleObjectStandard.clear();

    // when the textures have been sorted we have a counter which stores
	// the number of sorted objects, this is m_iSortedObjectCount, we now
	// run through the sorted texture list and find which objects are visible
	for ( int iSort = 0; iSort < g_iSortedObjectCount; iSort++ )
	{
		// get a pointer to the object from the sorted draw list
		sObject* pObject = g_ppSortedObjectList [ iSort ];
		if ( !pObject ) continue;

		// if the object is not visible, and not a parent that is animating, continue
		if ( ( !pObject->bVisible || (!pObject->bUniverseVisible && g_bIgnoreFarObjects == false) ) )
		   continue;

		// Dave Performance - this is used when sorting the list for shadow maps
		// If the objects are a certain size and certain distance away we skip their shadow
		// Smaller objects will drop their shadow earlier than larger ones
		if ( g_bIgnoreFarObjects )
		{
			// ensure have latest object center
			if ( !pObject->bIsStatic )
			{
				if ( pObject->bUniverseVisible )
					UpdateColCenter ( pObject );
				else
					continue;
			}

			// get center of the object
			GGVECTOR3 vecRealCenter = pObject->position.vecPosition + pObject->collision.vecColCenter;

			// work out distance and recommended disappearance distance
			float dx = CameraPositionX( g_pGlob->dwCurrentSetCameraID ) - vecRealCenter.x;
			float dy = CameraPositionY( g_pGlob->dwCurrentSetCameraID ) - vecRealCenter.y;
			float dz = CameraPositionZ( g_pGlob->dwCurrentSetCameraID ) - vecRealCenter.z;
			float dist = sqrtf ( dx*dx + dy*dy + dz*dz );
			float dmax = 1000;
			if ( g_HideDistantShadows == 1 )
			{
				// default from V1.14 small and large entity shadows culled from vis list
				if ( pObject->collision.fScaledLargestRadius > 100 ) dmax = 1500;
				if ( pObject->collision.fScaledLargestRadius > 300 ) dmax = 2500; // 070216 - was 150
				if ( pObject->collision.fScaledLargestRadius > 500 ) dmax = 4000;
			}
			if ( g_HideDistantShadows == 2 )
			{
				// protect larger structures from being removed from vis list
				if ( pObject->collision.fScaledLargestRadius > 200 ) dmax = 8000;
			}
			if ( dist > dmax ) continue;
		}

		// VISIBILITY CULLING CHECK PROCESS
		bool bIsVisible=false;

		// 20120307 IRM
		// If the object is a parent to an instance and is animating, then always
		// count it as visible to ensure that any instance animations continue.
		// This is true even if the object is off-screen, hidden or even excluded.
		if (pObject->position.bParentOfInstance && pObject->bAnimPlaying)
		{
			bIsVisible = true;
		}
		else
		{
			// actual object or instance of object
			sObject* pActualObject = pObject;
			if ( pActualObject->pInstanceOfObject )
				pActualObject=pActualObject->pInstanceOfObject;

			// locked objects are always visible
			// glued objects are always visible (deferred to parent visibility)
			int iGluedToObj = pObject->position.iGluedToObj;
			if ( pObject->bLockedObject || iGluedToObj!=0 )
			{
				// leefix -040803- maintenance check, if glued to object that has been deleted, deal with it
				if ( iGluedToObj!=0 )
				{
					sObject* pParentObject = g_ObjectList [ iGluedToObj ];
					if ( pParentObject==NULL )
					{
						// wipe out glue assignment
						pObject->position.bGlued		= false;
						pObject->position.iGluedToObj	= 0;
						pObject->position.iGluedToMesh	= 0;
					}
				}

				// locked objects and glued are visible
				bIsVisible=true;
			}
			else
			{
				// send the position of the object and it's radius to the "CheckSphere" function, if this returns true the object will be visible
				float fScaledRadius = pObject->collision.fScaledLargestRadius;
				if ( fScaledRadius<=0.0f )
				{
					// objects with no mesh scope are visible
					bIsVisible=true;
				}
				else
				{
					// ensure have latest object center
					// only do this is the object is not static
					if ( !pObject->bIsStatic )
						UpdateColCenter ( pObject );

					// get center of the object
					GGVECTOR3 vecRealCenter = pObject->position.vecPosition + pObject->collision.vecColCenter;

					// leeadd - 100805 - add in offset from first frame (limb zero), as this moves whole object render)
					if ( pActualObject->ppFrameList )
					{
						sFrame* pRootFrame = pActualObject->ppFrameList [ 0 ];
						if ( pRootFrame )
						{
							// leeadd - 211008 - u71 - added flag to NOT shift object bounds by frame zero matrix
							if ( (pRootFrame->dwStatusBits && 1)==0 ) 
							{
								// offset center to account for movement of the object by limb zero (root frame)
								vecRealCenter.x += pRootFrame->matUserMatrix._41;
								vecRealCenter.y += pRootFrame->matUserMatrix._42;
								vecRealCenter.z += pRootFrame->matUserMatrix._43;
							}
						}
					}

					// to avoid ugly clipping issues, double radius for objects that are anim-shifted
					float fFinalRadiusForVisCull = fScaledRadius * 2.0f;

					// objects within frustrum are visible
					if ( CheckSphere ( vecRealCenter.x, vecRealCenter.y, vecRealCenter.z, fFinalRadiusForVisCull ) )
						bIsVisible=true;
				}
			}
		}

		// determine visiblity

		// MIKE - 021203 - added in second part of if statement for external objects, physics DLL
		if ( bIsVisible || pObject->bDisableTransform == true )
		{
			// save a pointer to the object and place it in the new drawlist
			m_ppSortedObjectVisibleList [ m_iVisibleObjectCount++ ] = pObject;

            // Build individual draw lists for each layer
            if (pObject->bVeryEarlyObject == true)
            {
                m_vVisibleObjectEarly.push_back( pObject );
            }
            else if ( pObject->bNewZLayerObject || pObject->bLockedObject )
            {
				m_vVisibleObjectNoZDepth.push_back( pObject );
            }
            else if ( pObject->bGhostedObject || pObject->bTransparentObject )
            {
                m_vVisibleObjectTransparent.push_back( pObject );
            }
            else
            {
                m_vVisibleObjectStandard.push_back( pObject );
            }

            // u74b8 - If sort order is by distance, update the object distance
            if (g_eGlobalSortOrder == E_SORT_BY_DEPTH)
            {
			    if ( pObject->bVeryEarlyObject == true )
			    {
                    // very early objects are placed at extreme distance
				    pObject->position.fCamDistance = 9999999.9f;
			    }
			    else
			    {
				    pObject->position.fCamDistance = CalculateObjectDistanceFromCamera ( pObject );
			    }
            }
		}
	}

    // u74b8 - If sort order is by distance, sort the list into the correct order
    //         as it varies by camera.
    if (g_eGlobalSortOrder == E_SORT_BY_DEPTH)
    {
        // No ghost/transparent sort just yet - still need to take into account water -
        //but do need to sort everything else.
        std::sort( m_vVisibleObjectEarly.begin(), m_vVisibleObjectEarly.end(), OrderByReverseCameraDistance() );
        std::sort( m_vVisibleObjectNoZDepth.begin(), m_vVisibleObjectNoZDepth.end(), OrderByReverseCameraDistance() );
        std::sort( m_vVisibleObjectStandard.begin(), m_vVisibleObjectStandard.end(), OrderByReverseCameraDistance() );
    }

	// all went okay
	return true;
}
*/
