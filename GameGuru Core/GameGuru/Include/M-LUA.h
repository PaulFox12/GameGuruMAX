//----------------------------------------------------
//--- GAMEGURU - M-LUA
//----------------------------------------------------

#include "cstr.h"

void lua_init ( void );
void lua_loadscriptin ( void );
void lua_scanandloadactivescripts ( void );
void lua_free ( void );
void lua_ensureentityglobalarrayisinitialised ( void );
void lua_initscript ( void );
void lua_launchallinitscripts ( void );
void lua_execute_properties_variable(char *string);
void lua_quitting();
void lua_loop_begin ( void );
void lua_loop_finish ( void );
void lua_loop ( void );
void lua_raycastingwork (void);

// GG: what the entities' turns in lua_loop_allentities cost over the last second (GetLuaEntityCosts): each turn's
// row refresh (UpdateEntityRT), main call and USE KEY scan, and the turns in which the Lua heap shrank by 1 MB or more
// (a collection step ran inside them)
#define LUAENTITYCOST_TOP 10
struct sLuaEntityCostTop
{
	int e = 0;
	double dTotal = 0, dRow = 0, dMain = 0, dKey = 0;
	int iGC = 0;
};
struct sLuaEntityCosts
{
	double dWindowMs = 0;
	int iFrames = 0;
	double dTotal = 0, dRow = 0, dMain = 0, dKey = 0;
	int iTurns = 0, iRows = 0, iMains = 0, iKeyScans = 0;
	int iKeyScannersMax = 0;
	int iGC = 0;
	int iTop = 0;
	sLuaEntityCostTop top[LUAENTITYCOST_TOP];
};
bool lua_getentitycosts ( sLuaEntityCosts* pCosts );
void lua_lastframeentitycosts ( char* pText, int iSize );
