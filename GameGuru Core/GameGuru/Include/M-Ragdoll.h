//----------------------------------------------------
//--- GAMEGURU - M-Ragdoll
//----------------------------------------------------

#include "cstr.h"

void ragdoll_init ( void );
void ragdoll_free ( void );
void ragdoll_setcollisionmask ( int iMode );
void ragdoll_create ( void );
void ragdoll_destroy ( void );
int newvector3 ( int x_f, int y_f, int z_f );
int newvector2 ( int x_f, int y_f );
int vector3exist ( int vec );
int deletevector3 ( int vec );
int getlimbbyname ( int objid, char* limbname_s );

// GG: a living character made a ragdoll and woken again, holding a ragdoll body, and where a ragdoll lies
int ragdoll_knockout ( int e, float fPushX, float fPushY, float fPushZ, float fPushForce );
int ragdoll_wake ( int e, bool bAngleGiven, float fAngle );
bool ragdoll_isknockedout ( int e );
int ragdoll_hold ( int e, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, const char* pLimbName );
void ragdoll_release ( int e );
int ragdoll_setholdreference ( int e, const char* pClip, float fFraction );
bool ragdoll_getpose ( int e, float* pX, float* pY, float* pZ, float* pHeadAngle, int* pFaceUp, int* pResting, int* pHeld );
void ragdoll_knockoutloop ( void );
void ragdoll_knockoutclear ( void );
