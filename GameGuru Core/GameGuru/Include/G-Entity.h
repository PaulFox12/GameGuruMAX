//----------------------------------------------------
//--- GAMEGURU - G-Entity
//----------------------------------------------------

#include "cstr.h"

void entity_init_overwritefireratesettings ( void );
void entity_init ( void );
void entity_init_nowcreateattachments (void);
void entity_bringnewentitiestolife ( bool bAllNewOnes );
void entity_initafterphysics ( void );
void entity_configueelementforuse ( void );
void entity_freeragdoll ( void );
void entity_free ( void );
void entity_delete ( void );
void entity_pauseanimations ( void );
void entity_resumeanimations ( void );
void entity_loop ( void );
void entity_loopanim ( void );
void entity_controlrecalcdist ( void );
void entity_getmaxfreezedistance ( void );
void entity_updatepos ( void );
void entity_determinedamagemultiplier ( void );
void entity_determinegunforce ( void );
void entity_find_charanimindex_fromttte ( void );
void entity_adddestroyevent(int e);
void entity_applydamage ( void );
void entity_applydecalfordamage (int ee, float fX, float fY, float fZ);
void entity_gettruecamera ( void );
void entity_gettrueplayerpos(void);
bool entity_allowsbulletholes ( int e );
extern int g_iDefaultBulletHoles;
extern int g_iDefaultClearVegetation;
struct sPlayerHit
{
	int iSeq;
	int iHit;
	int e;
	float fX, fY, fZ;
	float fNX, fNY, fNZ;
	int iMaterial;
	int iHole;
	int iLimb;
	int iDamage;
	int iHealthBefore;
	int iHealthAfter;
	int iKilled;
	int iKind;
	int iShot;
	int iGunID;
	float fOX, fOY, fOZ;
	int iDamageE;
};
void playerhit_clear ( void );
void playerhit_openray ( int iKind, int iShot, int iGunID, float fOX, float fOY, float fOZ );
void playerhit_openblast ( int iShot, int iGunID, float fX, float fY, float fZ );
void playerhit_close ( void );
void playerhit_adddamage ( int e, int iDamage, int iHealthBefore, int iHealthAfter );
int playerhit_getseq ( void );
sPlayerHit* playerhit_get ( int iSeq );
struct sScriptBulletRayHit
{
	int e;
	float fX, fY, fZ;
	float fNX, fNY, fNZ;
	int iMaterial;
	bool bHole;
};
int entity_scriptbulletray ( float fX1, float fY1, float fZ1, float fX2, float fY2, float fZ2, int iIgnoreObj, bool bLeaveHole, int iTerrainMaterial, sScriptBulletRayHit* pHit );
void entity_hasbulletrayhit ( void );
void entity_hitentity ( int e, int obj );
void entity_triggerdecalatimpact ( float fX, float fY, float fZ );
void entity_createattachment ( void );
void entity_freeattachment ( void );
void entity_monitorattachments ( void );
void entity_monitorloot(void);
void entity_converttoclone ( void );
void entity_converttoclonetransparent ( void );
void entity_converttoinstance ( void );
void entity_createobj ( void );
void entity_updatelightobjtype (int obj, int spotlight);
void entity_updatelightobj (int e, int obj);
void entity_preparedepth(int entid, int obj);
void entity_prepareobj ( void );
void entity_applymaterial ( int tentid, int tte, int tobj );
void entity_resetmaterial ( int e );
void entity_calculateentityLODdistances ( int tentid, int tobj, int iModifier );
void entity_setupcharobjsettings ( void );
void entity_resettodefaultanimation ( void );
void entity_positionandscale ( void );
void entity_updateentityobj ( void );
void entity_cleargrideleprofrelationshipdata (void);

// the time of each step of a clone made from Lua (SpawnNewEntity), logged with it (a diagnostic): each mark adds the time
// since the last one to its step
enum { SPAWNSTEP_PROFILE, SPAWNSTEP_ADDCORE, SPAWNSTEP_BEFORECLONE, SPAWNSTEP_CLONE, SPAWNSTEP_ANIMATIONS, SPAWNSTEP_PREPARE,
	SPAWNSTEP_DEPTHLOD, SPAWNSTEP_POSITION, SPAWNSTEP_LIGHTS, SPAWNSTEP_EMITTER, SPAWNSTEP_FLATTEN, SPAWNSTEP_ADDREST,
	SPAWNSTEP_PHYSICS, SPAWNSTEP_COUNT };
void SpawnProbe_Start ( void );
void SpawnProbe_Mark ( int iStep );
void SpawnProbe_Stop ( char* pText, int iSize );
// finer timing in a probed spawn: a named slice (the time since the last mark or slice, shown in the step whose mark
// comes next; a name repeated adds up, with a count) and named calls counted and timed wherever they happen (file
// checks, image loads); all do nothing outside a probed spawn or off the spawning thread
void SpawnProbe_Sub ( const char* pName );
long long SpawnProbe_CallStart ( void );
void SpawnProbe_CallEnd ( const char* pName, long long llStart );