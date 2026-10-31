// include definition
#include "csoundmanagerc.h"

#define SAFE_RELEASE( p )      { if ( p ) { ( p )->Release ( ); ( p ) = NULL; } }
#define SAFE_DELETE( p )       { if ( p ) { delete ( p );       ( p ) = NULL; } }

CSDKSoundManager::CSDKSoundManager ( )
{
	memset ( m_pIndexPages, 0, sizeof ( m_pIndexPages ) );
}

CSDKSoundManager::~CSDKSoundManager ( )
{
	for ( int i = 0; i < SOUNDINDEX_MAX >> SOUNDINDEX_PAGEBITS; i++ )
	{
		delete [ ] m_pIndexPages [ i ];
		m_pIndexPages [ i ] = NULL;
	}
}

void CSDKSoundManager::SetIndex ( int iID, sSoundData* pData )
{
	if ( iID <= 0 || iID >= SOUNDINDEX_MAX )
		return;

	sSoundData**& pPage = m_pIndexPages [ iID >> SOUNDINDEX_PAGEBITS ];
	if ( !pPage )
	{
		if ( !pData )
			return;

		pPage = new sSoundData* [ SOUNDINDEX_PAGESIZE ];
		memset ( pPage, 0, sizeof ( sSoundData* ) * SOUNDINDEX_PAGESIZE );
	}
	pPage [ iID & ( SOUNDINDEX_PAGESIZE - 1 ) ] = pData;
}

bool CSDKSoundManager::Add ( sSoundData* pData, int iID )
{
	// check if an object already exists //
	// with the same id, if it does then //
	// delete it                         //
	///////////////////////////////////////
	sSoundData* ptr = NULL;
	ptr = this->GetData ( iID );
			
	if ( ptr != NULL )
		m_List.Delete ( iID );
	///////////////////////////////////////

	///////////////////////////////////////
	// create a new object and insert in //
	// the list                          //
	///////////////////////////////////////
	sSoundData* test;
	test = new sSoundData;

	memset ( test,     0, sizeof ( test       ) );
	memcpy ( test, pData, sizeof ( sSoundData ) );

	m_List.Add ( iID, ( VOID* ) test, 0, 1 );
	SetIndex ( iID, test );
	///////////////////////////////////////


	return true;
}

bool CSDKSoundManager::Delete ( int iID )
{
	sSoundData* ptr = NULL;
	ptr = this->GetData ( iID );

	if(ptr)
	{
		SAFE_DELETE ( ptr->pSound );
		SAFE_RELEASE ( ptr->pDSBuffer3D );
		delete ptr;
	}

	m_List.Delete ( iID );
	SetIndex ( iID, NULL );

	return true;
}

sSoundData* CSDKSoundManager::GetData ( int iID )
{		
	if ( iID > 0 && iID < SOUNDINDEX_MAX )
	{
		sSoundData** pPage = m_pIndexPages [ iID >> SOUNDINDEX_PAGEBITS ];
		return pPage ? pPage [ iID & ( SOUNDINDEX_PAGESIZE - 1 ) ] : NULL;
	}
	return ( sSoundData* ) m_List.Get ( iID );
}

void CSDKSoundManager::ShutDown ( void )
{
	link* check = m_List.m_start;

	for ( int temp = 0; temp < m_List.m_count; temp++ )
	{
		sSoundData* ptr = NULL;
		ptr = this->GetData ( check->id );
		if ( ptr == NULL ) continue;

		SAFE_DELETE ( ptr->pSound );
		SAFE_RELEASE ( ptr->pDSBuffer3D );

		delete ptr;

		check = check->next;
	}

	// the index pointed at the data just deleted
	for ( int i = 0; i < SOUNDINDEX_MAX >> SOUNDINDEX_PAGEBITS; i++ )
	{
		if ( m_pIndexPages [ i ] )
			memset ( m_pIndexPages [ i ], 0, sizeof ( sSoundData* ) * SOUNDINDEX_PAGESIZE );
	}
}

void CSDKSoundManager::Update ( void )
{
	link* check = m_List.m_start;

	for ( int temp = 0; temp < m_List.m_count; temp++ )
	{
		sSoundData* ptr = NULL;
		ptr = this->GetData ( check->id );
		if ( ptr == NULL ) continue;
		
		check = check->next;
	}
}