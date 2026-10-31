#ifndef _CSOUNDMANAGER_H_
#define _CSOUNDMANAGER_H_

#include ".\..\data\cdatac.h"
#include "CSoundDataC.h"

//#define DARKSDK __declspec ( dllexport )

// sound ids below SOUNDINDEX_MAX are also found through a direct index beside the list, a page of entries made when an id
// in it is first added; every sound command looks its sound up, and walking the list for each was the most of a 25 ms
// pause or resume (each walks every sound slot) with a game's sounds loaded
#define SOUNDINDEX_PAGEBITS		10
#define SOUNDINDEX_PAGESIZE		( 1 << SOUNDINDEX_PAGEBITS )
#define SOUNDINDEX_MAX			( 1 << 20 )

class CSDKSoundManager : private CData
{
	private:
		CData	m_List;	
		sSoundData** m_pIndexPages [ SOUNDINDEX_MAX >> SOUNDINDEX_PAGEBITS ];

		void SetIndex ( int iID, sSoundData* pData );

	public:
		CSDKSoundManager  ( );			// constructor
		~CSDKSoundManager ( );			// destructor

		bool Add    ( sSoundData* pData, int iID );
		bool Delete ( int iID );

		sSoundData* GetData ( int iID );

		void Update ( void );

		void ShutDown ( void );

		link* GetList() { return m_List.m_start; }
};

#endif _CSOUNDMANAGER_H_