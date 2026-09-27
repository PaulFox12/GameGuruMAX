//----------------------------------------------------
//--- GAMEGURU - M-Bulletholes
//----------------------------------------------------

#include "cstr.h"

void bulletholes_init               ( void );
void bulletholes_clearall			( void );
void bulletholes_add				( int iMaterialIndex, float fX, float fY, float fZ, float fNX, float fNY, float fNZ, int iOwnerEntity = 0 );
void bulletholes_update             ( void );
void bulletholes_free               ( void );
